#include "Zenith.h"

#include "Zenith_Tools_AnimMigrate.h"

#ifndef ZENITH_TOOLS

// Understanding an older schema is a TOOLS capability by ruling (D22/D23): a
// runtime build has no business reading a non-current .zanim, and
// Flux_AnimationClip::ParsePayload will not let it. Non-tools builds get no-ops
// so the boot phase still links.
bool Zenith_Tools_MigrateAuthoredClips(const std::filesystem::path&, Zenith_Tools_AnimMigrateReport&)
{
	return true;
}

void Zenith_Tools_MigrateAuthoredClipsAtBoot()
{
}

#else

#include "AssetHandling/Zenith_AssetRegistry.h"   // ResolvePath -- the game assets root at RUNTIME
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_DataStream.h"
#include "DataStream/Zenith_StreamEnvelope.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

#include <string>
#include <system_error>
#include <utility>

namespace
{
	// The suffix the staged write lands on before it is renamed over the
	// original. It deliberately keeps the whole original name in front of it, so
	// a leftover from a killed process is obviously paired with its source, and
	// it does NOT end in .zanim, so a re-run cannot pick a temp file up as input.
	const char* szTEMP_SUFFIX = ".migrate.tmp";

	// A Windows source tree is not case-stable, so the walk matches the extension
	// case-insensitively -- the same reasoning TextureUsage.ztexdecl's path
	// matching uses. A `.ZANIM` the walk skipped would be an authored clip left
	// silently on an old schema, which is the one outcome this phase exists to
	// prevent.
	bool AnimMigrateIsZanimExtension(const std::filesystem::path& xPath)
	{
		std::string strExtension = xPath.extension().string();
		for (char& cChar : strExtension)
		{
			if (cChar >= 'A' && cChar <= 'Z') { cChar = static_cast<char>(cChar - 'A' + 'a'); }
		}
		return strExtension == ZENITH_ANIMATION_EXT;
	}

	//--------------------------------------------------------------------------
	// One channel, retimed.
	//
	// ★ THE TIMES ARE DIVIDED, NOT MULTIPLIED BY A RECIPROCAL. `t * (1.0f/24.0f)`
	// and `t / 24.0f` are different floats: 1/24 is not representable, so the
	// reciprocal form rounds twice and lands up to an ulp away from the value an
	// author would have typed. Dividing means a clip whose tick times were
	// produced as `seconds * ticksPerSecond` comes back BIT-IDENTICAL to that
	// seconds value whenever the multiply was exact -- which is what lets the
	// corpus test compare migrated bytes against a hand-authored twin instead of
	// against a tolerance.
	//
	// ★ IT REBUILDS THE CHANNEL RATHER THAN RETIMING IT IN PLACE. The public
	// mutators (SetKeyframeTime) implement the D11 no-merge policy: a retime onto
	// a time within fANIM_TIME_EPSILON of another key is REFUSED. That is exactly
	// right for a pointer drag and exactly wrong for a bulk unit conversion,
	// which would then fail silently on a dense track. Appending in the SOURCE
	// index order also preserves the array order exactly, so no re-sort can
	// perturb the bytes of two keys sharing a time (D5).
	//--------------------------------------------------------------------------
	// Both conversion functions run under /fp:precise (Zenith.h: ZENITH_AUTHORING_
	// DETERMINISM). Every project compiles /fp:fast, and an optimised Release
	// build turned `x / fTicksPerSecond` into a reciprocal multiply - a last-bit
	// drift that broke D5's byte identity against the seconds-authored twin
	// (Schema1CorpusMigratesAndMatchesItsCurrentSchemaTwin, Release only).
	ZENITH_AUTHORING_DETERMINISM_BEGIN
	Flux_BoneChannel AnimMigrateDivideChannelTimes(const Flux_BoneChannel& xSource, float fDivisor)
	{
		Flux_BoneChannel xOut;
		xOut.SetBoneName(xSource.GetBoneName());

		const Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& xPositions = xSource.GetPositionKeyframes();
		for (u_int u = 0; u < xPositions.GetSize(); ++u)
		{
			xOut.AddPositionKeyframe(xPositions.Get(u).second / fDivisor, xPositions.Get(u).first);
		}
		const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xRotations = xSource.GetRotationKeyframes();
		for (u_int u = 0; u < xRotations.GetSize(); ++u)
		{
			// AddRotationKeyframe is the raw append path and does NOT normalize, so
			// a quaternion round-trips bit-for-bit through the migration.
			xOut.AddRotationKeyframe(xRotations.Get(u).second / fDivisor, xRotations.Get(u).first);
		}
		const Zenith_Vector<std::pair<Zenith_Maths::Vector3, float>>& xScales = xSource.GetScaleKeyframes();
		for (u_int u = 0; u < xScales.GetSize(); ++u)
		{
			xOut.AddScaleKeyframe(xScales.Get(u).second / fDivisor, xScales.Get(u).first);
		}

		// The tangent block (D17) is carried across VERBATIM -- both vectors and both
		// Flux_TangentModes, which Flux_ReadKeyTangents already DERIVED on the way in
		// for a schema-1/2 record (exactly zero is LINEAR, anything else CUSTOM).
		//
		// A tangent is a rate -- units (or radians) per second -- so a strict reading
		// says it should be scaled by the same factor as the clock. It is NOT,
		// deliberately: no clip that was ever written at schema 1 carries a non-zero
		// tangent (the block was reserved and unsampled for that whole era, and the
		// zero default makes scaling a no-op), and silently rescaling authored numbers
		// would be an invented migration nobody could check against anything.
		for (u_int u = 0; u < xSource.GetPositionTangents().GetSize(); ++u)
		{
			xOut.SetPositionTangent(u, xSource.GetPositionTangents().Get(u));
		}
		for (u_int u = 0; u < xSource.GetRotationTangents().GetSize(); ++u)
		{
			xOut.SetRotationTangent(u, xSource.GetRotationTangents().Get(u));
		}
		for (u_int u = 0; u < xSource.GetScaleTangents().GetSize(); ++u)
		{
			xOut.SetScaleTangent(u, xSource.GetScaleTangents().Get(u));
		}

		return xOut;
	}

	//--------------------------------------------------------------------------
	// STEP 1 -> 2: key times were TICKS, they are SECONDS now (D3).
	//
	// Nothing else in the payload moves. The DURATION was already seconds under
	// schema 1 -- that is precisely the two-clocks shape D3 removed -- so it is
	// left alone, and so is Flux_AnimationEvent::m_fNormalizedTime, which is a
	// [0,1] fraction of the clip and never was a time (D4). m_uTicksPerSecond
	// itself survives as import provenance and is NOT cleared: it is what a
	// re-export to a tick-based format still needs.
	//--------------------------------------------------------------------------
	bool AnimMigrateStep_TicksToSeconds(Flux_AnimationClip& xClip, const std::string& strPath, std::string& strOutReason)
	{
		const uint32_t uTicksPerSecond = xClip.GetTicksPerSecond();
		if (uTicksPerSecond == 0u)
		{
			// LOUD, and the file is left alone. A tick rate of zero makes the
			// conversion undefined, and "guess 24" would silently place every key in
			// the clip somewhere it was never authored -- an error a person can see
			// only by watching the animation, months later.
			Zenith_Assert(false,
				"Zenith_Tools_AnimMigrate: '%s' is schema 1 (key times in TICKS) but declares 0 ticks per second, "
				"so there is no conversion to seconds. The file is left untouched.", strPath.c_str());
			strOutReason = "schema 1 clip declares 0 ticks per second";
			return false;
		}

		const float fTicksPerSecond = static_cast<float>(uTicksPerSecond);

		// The channel map is rebuilt from a snapshot: AddBoneChannel Emplaces, and
		// removing first is what keeps that unambiguous for a key that already
		// exists. Two passes, because the map may not be mutated mid-iteration.
		const Flux_AnimationClip xSource = xClip;
		for (Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xSource.GetBoneChannels()); !xIt.Done(); xIt.Next())
		{
			xClip.RemoveBoneChannel(xIt.GetKey());
		}
		for (Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xSource.GetBoneChannels()); !xIt.Done(); xIt.Next())
		{
			xClip.AddBoneChannel(xIt.GetKey(), AnimMigrateDivideChannelTimes(xIt.GetValue(), fTicksPerSecond));
		}

		// Root motion rides the SAME clock as the bone channels -- it is sampled at
		// the clip's playback time -- so its delta key times convert too. It carries
		// no tangents and no name map, so it is scaled in place.
		Flux_RootMotion& xRootMotion = xClip.GetRootMotion();
		for (u_int u = 0; u < xRootMotion.m_xPositionDeltas.GetSize(); ++u)
		{
			xRootMotion.m_xPositionDeltas.Get(u).second /= fTicksPerSecond;
		}
		for (u_int u = 0; u < xRootMotion.m_xRotationDeltas.GetSize(); ++u)
		{
			xRootMotion.m_xRotationDeltas.Get(u).second /= fTicksPerSecond;
		}

		return true;
	}
	ZENITH_AUTHORING_DETERMINISM_END

	//--------------------------------------------------------------------------
	// STEP 2 -> 3: the per-key tangent MODES reached the wire (B2).
	//
	// ★ AN EXPLICIT NO-OP, AND IT IS REQUIRED RATHER THAN OPTIONAL. Nothing in
	// MEMORY has to move -- Flux_ReadKeyTangents already derived a mode per end
	// while reading the 24-byte schema-1/2 record, so the clip handed to this
	// function is already in the shape schema 3 describes, and the change of layout
	// happens in AnimMigratePublish's re-serialize through the CURRENT writer.
	//
	// The case still has to EXIST because the loop below is `uSchema < CURRENT` and
	// walks one version at a time: without it every schema-1 file would take the
	// 1->2 step and then fall onto the "no migration step implemented" assert at
	// 2->3, which is a refusal, not a migration. That is a real failure mode, not a
	// hypothetical -- it is exactly what the assert is for.
	//--------------------------------------------------------------------------
	bool AnimMigrateStep_TangentModesToWire(Flux_AnimationClip&, const std::string&, std::string&)
	{
		return true;
	}

	//--------------------------------------------------------------------------
	// The step CHAIN. A file at schema N is walked forward one step at a time to
	// uZENITH_ANIMATION_SCHEMA_CURRENT, so adding a schema means adding one case
	// here and nothing else -- a clip two versions behind is carried by 1->2 then
	// 2->3 rather than by a bespoke 1->3 path that only the newest bump exercises.
	//--------------------------------------------------------------------------
	bool AnimMigrateApplyStepChain(Flux_AnimationClip& xClip, u_int uFromSchema,
		const std::string& strPath, std::string& strOutReason)
	{
		for (u_int uSchema = uFromSchema; uSchema < uZENITH_ANIMATION_SCHEMA_CURRENT; ++uSchema)
		{
			if (uSchema == 1u)
			{
				if (!AnimMigrateStep_TicksToSeconds(xClip, strPath, strOutReason))
				{
					return false;
				}
				continue;
			}

			if (uSchema == 2u)
			{
				if (!AnimMigrateStep_TangentModesToWire(xClip, strPath, strOutReason))
				{
					return false;
				}
				continue;
			}

			Zenith_Assert(false,
				"Zenith_Tools_AnimMigrate: no migration step is implemented from .zanim schema %u to %u ('%s'). "
				"Bumping uZENITH_ANIMATION_SCHEMA_CURRENT means adding the step here in the same change.",
				uSchema, uSchema + 1u, strPath.c_str());
			strOutReason = "no migration step implemented for this schema";
			return false;
		}
		return true;
	}

	//--------------------------------------------------------------------------
	// Publication: temp file, re-parse with the RUNTIME reader, then rename.
	//
	// ★ THE VERIFY IS THE POINT, NOT THE TEMP FILE. A staged rename alone only
	// guarantees the replacement is whole; re-parsing the bytes that were actually
	// written -- through Flux_AnimationClip::ParseStream, i.e. the exact code the
	// game will use -- is what guarantees they are a .zanim at the CURRENT schema.
	// A migration that produced something the runtime refuses would otherwise be
	// discovered by a player, not by the boot that caused it.
	//--------------------------------------------------------------------------
	bool AnimMigratePublish(const Flux_AnimationClip& xClip, const std::filesystem::path& xFinalPath,
		std::string& strOutReason)
	{
		std::filesystem::path xTempPath = xFinalPath;
		xTempPath += szTEMP_SUFFIX;
		const std::string strTempPath = xTempPath.string();

		{
			Zenith_DataStream xOut;
			xClip.WriteToDataStream(xOut);
			xOut.WriteToFile(strTempPath.c_str());
		}

		std::error_code xError;
		if (!std::filesystem::exists(xTempPath, xError))
		{
			strOutReason = "the staged write produced no file";
			return false;
		}

		{
			Zenith_DataStream xVerify;
			xVerify.ReadFromFile(strTempPath.c_str());
			Flux_AnimationClip xRoundTrip;
			const bool bParsed = xVerify.IsValid() && xRoundTrip.ParseStream(xVerify).IsOk();
			if (!bParsed)
			{
				std::filesystem::remove(xTempPath, xError);
				strOutReason = "the migrated bytes do not re-parse at the current schema";
				return false;
			}
		}

		// std::filesystem::rename REPLACES an existing file on Windows (MoveFileEx
		// with MOVEFILE_REPLACE_EXISTING underneath), which is not what POSIX rename
		// semantics would lead you to expect for a name that already exists.
		std::filesystem::rename(xTempPath, xFinalPath, xError);
		if (xError)
		{
			std::error_code xIgnored;
			std::filesystem::remove(xTempPath, xIgnored);
			strOutReason = std::string("could not publish the migrated file (") + xError.message() + ")";
			return false;
		}
		return true;
	}

	enum AnimMigrateFileOutcome
	{
		ANIM_MIGRATE_FILE_MIGRATED,
		ANIM_MIGRATE_FILE_ALREADY_CURRENT,
		ANIM_MIGRATE_FILE_FAILED,
	};

	AnimMigrateFileOutcome AnimMigrateOneFile(const std::filesystem::path& xPath, std::string& strOutReason)
	{
		const std::string strPath = xPath.string();

		Zenith_DataStream xStream;
		xStream.ReadFromFile(strPath.c_str());
		if (!xStream.IsValid())
		{
			Zenith_Assert(false, "Zenith_Tools_AnimMigrate: '%s' could not be read (empty or unreadable)", strPath.c_str());
			strOutReason = "the file could not be read";
			return ANIM_MIGRATE_FILE_FAILED;
		}

		// The envelope answers three of the four questions before a byte of payload
		// is touched, and Zenith_ReadStreamHeader restores the cursor on every
		// failure path, so a refused file is handed back exactly as it arrived.
		Zenith_Result<Zenith_StreamHeader> xHeader = Zenith_ReadStreamHeader(xStream, uZENITH_ANIMATION_ASSET_TYPE_ID);
		if (!xHeader.IsOk())
		{
			Zenith_Assert(false,
				"Zenith_Tools_AnimMigrate: '%s' is under Assets/Authored but is not a .zanim -- no envelope, or another "
				"asset's type id. The file is left untouched.", strPath.c_str());
			strOutReason = "not an animation-asset envelope";
			return ANIM_MIGRATE_FILE_FAILED;
		}

		const u_int uSchema = xHeader.Value().m_uSchemaVersion;

		if (uSchema == uZENITH_ANIMATION_SCHEMA_CURRENT)
		{
			// ★ A NO-OP, NOT AN ERROR, AND NOT A REWRITE EITHER. The file is not
			// opened for writing, so its bytes cannot move -- which matters because
			// these files are COMMITTED, and a boot that re-serialized every authored
			// clip would put the whole tree in `git status` on every run.
			//
			// It is also not re-validated. Whether a current-schema file is WELL
			// FORMED is the runtime loader's question (ParseStream answers it, loudly,
			// at load); the migrator's only question is whether the schema needs
			// moving, and parsing a file it has no intention of writing would just be
			// a second opinion nothing acts on.
			return ANIM_MIGRATE_FILE_ALREADY_CURRENT;
		}

		if (uSchema > uZENITH_ANIMATION_SCHEMA_CURRENT)
		{
			// A file from a NEWER build. There is no backwards step and there never
			// will be -- the newer layout is not described anywhere in this build --
			// so this is a refusal, not a downgrade.
			Zenith_Assert(false,
				"Zenith_Tools_AnimMigrate: '%s' is .zanim schema %u, NEWER than this build's current %u. "
				"It was written by a later tool; update this build rather than the file. Left untouched.",
				strPath.c_str(), uSchema, uZENITH_ANIMATION_SCHEMA_CURRENT);
			strOutReason = "schema is newer than this build";
			return ANIM_MIGRATE_FILE_FAILED;
		}

		// Behind. Read the body AT ITS OWN SCHEMA -- the one call in the engine
		// allowed to do that, and only because this TU is ZENITH_TOOLS.
		Flux_AnimationClip xClip;
		const Zenith_Status xParsed = xClip.ParsePayload(xStream, uSchema);
		if (!xParsed.IsOk())
		{
			strOutReason = "the payload does not parse at its own schema";
			return ANIM_MIGRATE_FILE_FAILED;
		}

		// ★ A .zanim PAYLOAD ENDS EXACTLY AT END OF FILE, and that is the truncation
		// detector. Zenith_DataStream refuses an over-long read rather than crashing
		// -- it asserts, logs, and leaves the cursor where it was -- so a truncated
		// payload parses into a plausible-looking clip with zero-filled tails and no
		// return value to say so. Comparing the consumed length against the file
		// length is what turns that into a refusal. (It catches trailing garbage
		// too, which is the same class of "these are not the bytes we wrote".)
		if (xStream.GetCursor() != xStream.GetCapacity())
		{
			Zenith_Assert(false,
				"Zenith_Tools_AnimMigrate: '%s' payload consumed %llu of %llu bytes -- the file is truncated or has "
				"trailing bytes. Left untouched.",
				strPath.c_str(),
				static_cast<unsigned long long>(xStream.GetCursor()),
				static_cast<unsigned long long>(xStream.GetCapacity()));
			strOutReason = "payload length does not match the file length";
			return ANIM_MIGRATE_FILE_FAILED;
		}

		if (!AnimMigrateApplyStepChain(xClip, uSchema, strPath, strOutReason))
		{
			return ANIM_MIGRATE_FILE_FAILED;
		}

		if (!AnimMigratePublish(xClip, xPath, strOutReason))
		{
			Zenith_Assert(false, "Zenith_Tools_AnimMigrate: '%s' -- %s", strPath.c_str(), strOutReason.c_str());
			return ANIM_MIGRATE_FILE_FAILED;
		}

		Zenith_Log(LOG_CATEGORY_TOOLS, "[AnimMigrate] '%s': schema %u -> %u", strPath.c_str(),
			uSchema, uZENITH_ANIMATION_SCHEMA_CURRENT);
		return ANIM_MIGRATE_FILE_MIGRATED;
	}

	void AnimMigrateRecordFailure(Zenith_Tools_AnimMigrateReport& xReport,
		const std::string& strPath, const std::string& strReason)
	{
		xReport.m_uFailed++;
		if (xReport.m_strFirstFailurePath.empty())
		{
			xReport.m_strFirstFailurePath = strPath;
			xReport.m_strFirstFailureReason = strReason;
		}
		Zenith_Error(LOG_CATEGORY_TOOLS, "[AnimMigrate] REFUSED '%s': %s", strPath.c_str(), strReason.c_str());
	}
}

bool Zenith_Tools_MigrateAuthoredClips(const std::filesystem::path& xAuthoredRoot,
	Zenith_Tools_AnimMigrateReport& xOutReport)
{
	// ★ CHEAP AND SUCCESSFUL WHEN THE ROOT IS NOT THERE. `Assets/Authored/` does
	// not exist in most trees, and a missing directory makes
	// recursive_directory_iterator THROW -- an unhandled throw in a boot phase
	// kills the tools boot before any export runs (Tools/CLAUDE.md). Every
	// filesystem call below takes an error_code for the same reason.
	std::error_code xError;
	if (!std::filesystem::is_directory(xAuthoredRoot, xError))
	{
		return true;
	}

	Zenith_Tools_AnimMigrateReport xLocal;

	std::filesystem::recursive_directory_iterator xIt(xAuthoredRoot,
		std::filesystem::directory_options::skip_permission_denied, xError);
	if (xError)
	{
		Zenith_Error(LOG_CATEGORY_TOOLS, "[AnimMigrate] could not walk '%s': %s",
			xAuthoredRoot.string().c_str(), xError.message().c_str());
		return true;
	}

	const std::filesystem::recursive_directory_iterator xEnd;
	for (; xIt != xEnd; xIt.increment(xError))
	{
		if (xError)
		{
			Zenith_Error(LOG_CATEGORY_TOOLS, "[AnimMigrate] walk of '%s' stopped: %s",
				xAuthoredRoot.string().c_str(), xError.message().c_str());
			break;
		}

		const std::filesystem::path& xPath = xIt->path();
		if (!xIt->is_regular_file(xError) || xError)
		{
			xError.clear();
			continue;
		}
		if (!AnimMigrateIsZanimExtension(xPath))
		{
			continue;
		}

		xLocal.m_uScanned++;
		std::string strReason;
		switch (AnimMigrateOneFile(xPath, strReason))
		{
		case ANIM_MIGRATE_FILE_MIGRATED:
			xLocal.m_uMigrated++;
			break;
		case ANIM_MIGRATE_FILE_ALREADY_CURRENT:
			xLocal.m_uSkippedCurrent++;
			break;
		case ANIM_MIGRATE_FILE_FAILED:
			AnimMigrateRecordFailure(xLocal, xPath.string(), strReason);
			break;
		}
	}

	Zenith_Assert(xLocal.CountsAddUp(),
		"Zenith_Tools_AnimMigrate: '%s' scanned %u files but accounted for %u -- a file was silently dropped",
		xAuthoredRoot.string().c_str(), xLocal.m_uScanned,
		xLocal.m_uMigrated + xLocal.m_uSkippedCurrent + xLocal.m_uFailed);

	xOutReport.Accumulate(xLocal);
	return xLocal.m_uFailed == 0u;
}

void Zenith_Tools_MigrateAuthoredClipsAtBoot()
{
	// Both roots, because Zenith_AnimationDocument::BuildAuthoredAssetPath keeps
	// the SOURCE's root prefix and inserts "Authored/" directly under it -- so an
	// engine clip promotes into Zenith/Assets/Authored/ and a game clip into
	// Games/<Game>/Assets/Authored/, and scanning one would leave the other's
	// authored overrides behind on an old schema with nothing to say so.
	//
	// ★ THE TWO ROOTS ARE REACHED DIFFERENTLY, AND THAT IS NOT AN INCONSISTENCY.
	// ENGINE_ASSETS_DIR is a define on the ENGINE library, which this file is
	// compiled into. GAME_ASSETS_DIR is a per-GAME project define and DOES NOT
	// EXIST here -- every sibling exporter that appears to use it says so in
	// PROSE, in a comment, and none of them names it in code. The game root is
	// therefore resolved at RUNTIME through the registry, which is handed the
	// game's assets directory before Zenith_AssetRegistry::Initialize() and so is
	// already populated by the time this boot phase runs.
	const std::filesystem::path xEngineRoot = std::filesystem::path(ENGINE_ASSETS_DIR) / "Authored";

	Zenith_Tools_AnimMigrateReport xReport;
	Zenith_Tools_MigrateAuthoredClips(xEngineRoot, xReport);

	// ★ AN UNSET GAME DIRECTORY DOES NOT RESOLVE TO AN EMPTY STRING -- IT RESOLVES
	// TO THE BARE RELATIVE PATH. Zenith_AssetRegistry::ResolvePath returns the
	// suffix unchanged when s_strGameAssetsDir is empty, so a naive
	// `if (!strResolved.empty())` would hand this walk the literal "Authored",
	// which the OS then interprets against the process CWD -- next to the exe,
	// where a stray directory of that name would be silently migrated. So the
	// un-set case is detected by the resolution having done nothing at all.
	const char* szGAME_AUTHORED_REF = "game:Authored";
	const char* szAUTHORED_RELATIVE = "Authored";
	const std::string strGameAuthored = Zenith_AssetRegistry::ResolvePath(szGAME_AUTHORED_REF);
	if (strGameAuthored.empty() || strGameAuthored == szAUTHORED_RELATIVE)
	{
		// Not fatal, and not an assert: this phase's whole contract is that an
		// absent authored tree is normal. But an unresolvable GAME root is
		// different from an absent one -- it means authored overrides for this game
		// were not even LOOKED at -- so it gets its own line rather than being
		// folded into the "nothing found" report below.
		Zenith_Log(LOG_CATEGORY_TOOLS,
			"[AnimMigrate] the game assets directory is not set on the asset registry -- "
			"skipping the game's Assets/Authored root (the engine root was still walked)");
	}
	else
	{
		const std::filesystem::path xGameRoot(strGameAuthored);
		// A game whose assets dir IS the engine's would otherwise be walked twice
		// and double-count every file in the report.
		std::error_code xError;
		if (!std::filesystem::equivalent(xEngineRoot, xGameRoot, xError) || xError)
		{
			Zenith_Tools_MigrateAuthoredClips(xGameRoot, xReport);
		}
	}

	if (xReport.m_uScanned == 0u)
	{
		// The normal case today: no authored overrides anywhere. Say so once, at
		// info level, so the phase is visible in a boot log without being noise.
		Zenith_Log(LOG_CATEGORY_TOOLS, "[AnimMigrate] no authored .zanim found under either Assets/Authored root");
		return;
	}

	Zenith_Log(LOG_CATEGORY_TOOLS,
		"[AnimMigrate] %u authored clip(s): %u migrated, %u already current, %u refused",
		xReport.m_uScanned, xReport.m_uMigrated, xReport.m_uSkippedCurrent, xReport.m_uFailed);

	if (xReport.m_uFailed != 0u)
	{
		Zenith_Error(LOG_CATEGORY_TOOLS,
			"[AnimMigrate] %u authored clip(s) were REFUSED and left untouched -- first: '%s' (%s)",
			xReport.m_uFailed, xReport.m_strFirstFailurePath.c_str(), xReport.m_strFirstFailureReason.c_str());
	}
}

#include "Zenith_Tools_AnimMigrate.Tests.inl"

#endif // ZENITH_TOOLS
