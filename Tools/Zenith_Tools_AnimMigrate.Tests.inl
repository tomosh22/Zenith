//------------------------------------------------------------------------------
// Zenith_Tools_AnimMigrate unit tests (WU-2.5).
// Included at the bottom of Zenith_Tools_AnimMigrate.cpp, inside ZENITH_TOOLS.
//
// ★ THE HEADLINE PROPERTY: a schema-1 authored clip, carried forward by the
// migrator, is BYTE-IDENTICAL to the same clip authored directly in seconds at
// the current schema — and samples identically to it at every matched time.
// Everything else here pins the edges of that: an already-current file is a
// no-op rather than a rewrite, a corrupt file is refused and left exactly as it
// was, a zero tick rate is a loud refusal rather than an infinity, and an
// absent authored root is success rather than a throw.
//
// All of it runs headless under the Null backend: a clip is pure CPU data, the
// corpus is written into a private directory under the OS temp dir and removed
// again, and nothing touches a device or the asset registry. None of these is
// requiresGraphics.
//------------------------------------------------------------------------------

#include "UnitTests/Zenith_UnitTests.h"
#include "UnitTests/Zenith_AssertCapture.h"   // the refusal paths assert on purpose

#include <cmath>        // std::abs — the sampled-pose comparisons
#include <cstring>      // std::memcpy — poking the envelope's words
#include <fstream>
#include <iterator>
#include <string>

namespace
{
	//--------------------------------------------------------------------------
	// Fixture — a private directory under the OS temp dir, removed on the way out.
	//--------------------------------------------------------------------------
	struct AnimMigrateTempTree
	{
		std::filesystem::path m_xRoot;

		explicit AnimMigrateTempTree(const char* szLeafDirectory)
		{
			std::error_code xError;
			std::filesystem::path xBase = std::filesystem::temp_directory_path(xError);
			if (xError)
			{
				xBase = ".";
			}
			m_xRoot = xBase / szLeafDirectory;
			std::filesystem::remove_all(m_xRoot, xError);
			std::filesystem::create_directories(m_xRoot, xError);
		}

		~AnimMigrateTempTree()
		{
			std::error_code xError;
			std::filesystem::remove_all(m_xRoot, xError);
		}

		AnimMigrateTempTree(const AnimMigrateTempTree&) = delete;
		AnimMigrateTempTree& operator=(const AnimMigrateTempTree&) = delete;

		// A subdirectory of the tree, created on demand.
		std::filesystem::path Dir(const char* szName) const
		{
			std::error_code xError;
			const std::filesystem::path xPath = m_xRoot / szName;
			std::filesystem::create_directories(xPath, xError);
			return xPath;
		}
	};

	//--------------------------------------------------------------------------
	// Raw bytes in and out. The byte-identity assertions are the determinism pin
	// (D5), so they compare FILES, not parsed clips.
	//--------------------------------------------------------------------------
	std::string AnimMigrateReadBytes(const std::filesystem::path& xPath)
	{
		std::ifstream xFile(xPath, std::ios::binary);
		if (!xFile)
		{
			return std::string();
		}
		return std::string(std::istreambuf_iterator<char>(xFile), std::istreambuf_iterator<char>());
	}

	void AnimMigrateWriteBytes(const std::filesystem::path& xPath, const std::string& strBytes)
	{
		std::ofstream xFile(xPath, std::ios::binary | std::ios::trunc);
		xFile.write(strBytes.data(), static_cast<std::streamsize>(strBytes.size()));
	}

	// Zenith_StreamHeader is four u_ints written in declaration order by
	// Zenith_WriteStreamHeader: magic, envelope version, asset type id, schema.
	constexpr u_int uANIM_MIGRATE_WORD_TYPE_ID = 2u;
	constexpr u_int uANIM_MIGRATE_WORD_SCHEMA  = 3u;

	void AnimMigratePokeHeaderWord(std::string& strBytes, u_int uWordIndex, u_int uValue)
	{
		std::memcpy(&strBytes[static_cast<size_t>(uWordIndex) * sizeof(u_int)], &uValue, sizeof(u_int));
	}

	//--------------------------------------------------------------------------
	// The corpus.
	//
	// ★★ EVERY KEY TIME IS A MULTIPLE OF 0.25 s AND EVERY TICK RATE IS SMALL, AND
	// THAT IS LOAD-BEARING FOR THE BYTE-IDENTITY ASSERTION. The schema-1 twin of a
	// clip is built by multiplying its seconds times by the tick rate, and the
	// migrator recovers them by DIVIDING. `(x * n) / n == x` exactly only when
	// `x * n` was itself exact — no rounding to undo — which holds here because
	// every product below (1.0*24, 2.0*24, 0.75*30, 1.5*30, 2.0*32, 4.0*32) has a
	// mantissa that fits a float with room to spare. Pick a time of 0.1 s or a
	// tick rate of 1001 and the round trip lands an ulp out, the bytes differ, and
	// the failure looks like a migrator bug rather than a test-data bug.
	//
	// The DURATION is deliberately NOT scaled: it was seconds under schema 1 too.
	// That is exactly the two-clocks shape D3 removed, and it is why
	// Flux_ClipKeyTimesFitDuration is FALSE for a schema-1 clip and TRUE after the
	// migration — asserted below, so a migrator that did nothing at all could not
	// pass this file.
	//--------------------------------------------------------------------------
	struct AnimMigrateCorpusSpec
	{
		const char* m_szClipName;
		uint32_t    m_uTicksPerSecond;
		float       m_fDurationSeconds;
	};

	constexpr u_int uANIM_MIGRATE_CORPUS_COUNT = 3u;

	const AnimMigrateCorpusSpec axANIM_MIGRATE_CORPUS[uANIM_MIGRATE_CORPUS_COUNT] =
	{
		{ "AuthoredWalk", 24u, 2.0f },
		{ "AuthoredIdle", 30u, 1.5f },
		{ "AuthoredWave", 32u, 4.0f },
	};

	// fTimeScale is 1.0 for the seconds-authored twin and the clip's own tick rate
	// for its schema-1 original. NOTHING ELSE differs between the two.
	void AnimMigrateBuildCorpusClip(Flux_AnimationClip& xClip, u_int uIndex, float fTimeScale)
	{
		const AnimMigrateCorpusSpec& xSpec = axANIM_MIGRATE_CORPUS[uIndex];
		const float fDuration = xSpec.m_fDurationSeconds;
		const float fHalf = fDuration * 0.5f;

		xClip.SetName(xSpec.m_szClipName);
		xClip.SetDuration(fDuration);
		xClip.SetTicksPerSecond(xSpec.m_uTicksPerSecond);
		xClip.GetMetadata().m_uAuthoredFrameRate = 30u + uIndex;
		xClip.GetMetadata().m_bLooping = (uIndex % 2u) == 0u;
		xClip.GetMetadata().m_fBlendInTime = 0.25f;
		xClip.GetMetadata().m_strSkeletonPath = "engine:Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT;
		xClip.GetMetadata().m_bGenerated = false;   // D8: authored, which is why it is committed at all

		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f,                 Zenith_Maths::Vector3(0.0f,  1.0f,   0.0f));
		xHip.AddPositionKeyframe(fHalf * fTimeScale,   Zenith_Maths::Vector3(0.25f, 1.125f, 0.0f));
		xHip.AddPositionKeyframe(fDuration* fTimeScale,Zenith_Maths::Vector3(0.5f,  1.0f,   0.0f));
		xHip.AddRotationKeyframe(0.0f,                 Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));
		xHip.AddRotationKeyframe(fDuration* fTimeScale,glm::angleAxis(glm::radians(30.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
		xHip.AddScaleKeyframe   (0.0f,                 Zenith_Maths::Vector3(1.0f));
		xHip.AddScaleKeyframe   (fHalf * fTimeScale,   Zenith_Maths::Vector3(1.0f, 1.25f, 1.0f));

		// One non-zero reserved tangent (D17), so the block is proved to survive the
		// migration rather than merely being written as zeros at both ends.
		Flux_KeyTangents xTangent;
		xTangent.m_xInTangent  = Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f);
		xTangent.m_xOutTangent = Zenith_Maths::Vector3(0.0f, 0.25f, 0.0f);
		xHip.SetPositionTangent(1u, xTangent);
		xClip.AddBoneChannel("Hip", std::move(xHip));

		Flux_BoneChannel xKnee;
		xKnee.AddRotationKeyframe(0.0f,                  glm::angleAxis(glm::radians(5.0f),  Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)));
		xKnee.AddRotationKeyframe(fHalf * fTimeScale,    glm::angleAxis(glm::radians(35.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)));
		xKnee.AddRotationKeyframe(fDuration * fTimeScale,glm::angleAxis(glm::radians(10.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)));
		xClip.AddBoneChannel("Knee", std::move(xKnee));

		// D4: an event time is a [0,1] FRACTION of the clip, not a time on the key
		// clock — so it is identical in both twins and must not be converted.
		Flux_AnimationEvent xFootstep;
		xFootstep.m_fNormalizedTime = 0.25f;
		xFootstep.m_strEventName = "FootstepLeft";
		xFootstep.m_xData = Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.0f);
		xClip.AddEvent(xFootstep);

		Flux_AnimationEvent xFootstepRight;
		xFootstepRight.m_fNormalizedTime = 0.75f;
		xFootstepRight.m_strEventName = "FootstepRight";
		xFootstepRight.m_xData = Zenith_Maths::Vector4(0.0f, 1.0f, 0.0f, 0.0f);
		xClip.AddEvent(xFootstepRight);

		// Root motion rides the SAME clock as the bone channels, so its delta times
		// scale with them.
		Flux_RootMotion& xRootMotion = xClip.GetRootMotion();
		xRootMotion.m_bEnabled = true;
		xRootMotion.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f),             0.0f);
		xRootMotion.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.5f), fHalf * fTimeScale);
		xRootMotion.m_xPositionDeltas.EmplaceBack(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), fDuration * fTimeScale);
		xRootMotion.m_xRotationDeltas.EmplaceBack(Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), 0.0f);
		xRootMotion.m_xRotationDeltas.EmplaceBack(glm::angleAxis(glm::radians(15.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)),
			fDuration * fTimeScale);
	}

	// Write a clip's bytes with the envelope's schema word forced to uSchema. The
	// payload layout of schemas 1 and 2 is identical, so this produces a genuine
	// schema-1 file rather than a doctored one.
	void AnimMigrateWriteClipAtSchema(const Flux_AnimationClip& xClip, const std::filesystem::path& xPath, u_int uSchema)
	{
		Zenith_DataStream xStream;
		xClip.WriteToDataStream(xStream);
		if (uSchema != uZENITH_ANIMATION_SCHEMA_CURRENT)
		{
			std::memcpy(static_cast<uint8_t*>(xStream.GetData()) + uANIM_MIGRATE_WORD_SCHEMA * sizeof(u_int),
				&uSchema, sizeof(u_int));
		}
		xStream.WriteToFile(xPath.string().c_str());
	}

	// Loads through the RUNTIME reader, which refuses anything that is not the
	// current schema — so "this file loads" is exactly the acceptance question.
	bool AnimMigrateLoadClip(const std::filesystem::path& xPath, Flux_AnimationClip& xOutClip)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(xPath.string().c_str());
		if (!xStream.IsValid())
		{
			return false;
		}
		return xOutClip.ParseStream(xStream).IsOk();
	}

	bool AnimMigrateFileSchema(const std::filesystem::path& xPath, u_int& uOutSchema)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(xPath.string().c_str());
		if (!xStream.IsValid())
		{
			return false;
		}
		Zenith_Result<Zenith_StreamHeader> xHeader = Zenith_ReadStreamHeader(xStream, uZENITH_ANIMATION_ASSET_TYPE_ID);
		if (!xHeader.IsOk())
		{
			return false;
		}
		uOutSchema = xHeader.Value().m_uSchemaVersion;
		return true;
	}

	// 24 matched times spanning [0, duration], every channel, all three tracks,
	// plus the root-motion deltas. This is the property; the byte comparison
	// beside it is the DETERMINISM pin. Keeping both is deliberate — bytes alone
	// would still pass if the writer and the migrator drifted together, and
	// samples alone would not notice a re-ordering that changes the file on every
	// re-bake.
	constexpr u_int uANIM_MIGRATE_SAMPLE_COUNT = 24u;

	bool AnimMigrateSamplesMatch(const Flux_AnimationClip& xA, const Flux_AnimationClip& xB, float fDuration)
	{
		if (xA.GetBoneChannels().GetSize() != xB.GetBoneChannels().GetSize())
		{
			return false;
		}

		for (Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xA.GetBoneChannels()); !xIt.Done(); xIt.Next())
		{
			const Flux_BoneChannel* pxOther = xB.GetBoneChannel(xIt.GetKey());
			if (pxOther == nullptr)
			{
				return false;
			}
			const Flux_BoneChannel& xChannel = xIt.GetValue();
			for (u_int u = 0; u < uANIM_MIGRATE_SAMPLE_COUNT; ++u)
			{
				const float fTime = fDuration * (static_cast<float>(u) / static_cast<float>(uANIM_MIGRATE_SAMPLE_COUNT - 1u));
				if (glm::length(xChannel.SamplePosition(fTime) - pxOther->SamplePosition(fTime)) > 1e-5f) { return false; }
				if (glm::length(xChannel.SampleScale(fTime)    - pxOther->SampleScale(fTime))    > 1e-5f) { return false; }
				// A quaternion and its negation are the same rotation, so compare |dot|.
				const float fDot = glm::dot(xChannel.SampleRotation(fTime), pxOther->SampleRotation(fTime));
				if (std::abs(std::abs(fDot) - 1.0f) > 1e-5f) { return false; }
			}
		}

		for (u_int u = 0; u < uANIM_MIGRATE_SAMPLE_COUNT; ++u)
		{
			const float fTime = fDuration * (static_cast<float>(u) / static_cast<float>(uANIM_MIGRATE_SAMPLE_COUNT - 1u));
			if (glm::length(xA.GetRootMotion().SamplePositionDelta(fTime) - xB.GetRootMotion().SamplePositionDelta(fTime)) > 1e-5f)
			{
				return false;
			}
			const float fDot = glm::dot(xA.GetRootMotion().SampleRotationDelta(fTime), xB.GetRootMotion().SampleRotationDelta(fTime));
			if (std::abs(std::abs(fDot) - 1.0f) > 1e-5f) { return false; }
		}

		return true;
	}

	// No `.migrate.tmp` may survive a walk, whatever happened during it.
	u_int AnimMigrateCountTempFiles(const std::filesystem::path& xRoot)
	{
		u_int uCount = 0u;
		std::error_code xError;
		for (std::filesystem::recursive_directory_iterator xIt(xRoot, xError), xEnd;
			!xError && xIt != xEnd; xIt.increment(xError))
		{
			if (xIt->path().extension() == ".tmp")
			{
				uCount++;
			}
		}
		return uCount;
	}
}

//==============================================================================
// (1) THE HEADLINE. A schema-1 corpus is carried to the current schema, and each
//     migrated file is byte-identical to — and samples identically to — the same
//     clip authored directly in seconds.
//==============================================================================
ZENITH_TEST(AnimMigrate, Schema1CorpusMigratesAndMatchesItsCurrentSchemaTwin)
{
	AnimMigrateTempTree xTree("zenith_animmigrate_corpus");
	const std::filesystem::path xAuthored = xTree.Dir("Authored");
	// One clip lives a directory down, so the walk is proved to be RECURSIVE — an
	// authored tree mirrors its source's subdirectories (BuildAuthoredAssetPath
	// preserves them precisely so two "Walk" clips cannot collide).
	const std::filesystem::path xNested = xTree.Dir("Authored/Meshes/StickFigure");
	// The twins live OUTSIDE the migrated root so they are not scanned themselves.
	const std::filesystem::path xTwins = xTree.Dir("Twins");

	for (u_int u = 0; u < uANIM_MIGRATE_CORPUS_COUNT; ++u)
	{
		const std::string strLeaf = std::string(axANIM_MIGRATE_CORPUS[u].m_szClipName) + ZENITH_ANIMATION_EXT;

		Flux_AnimationClip xTickClip;
		AnimMigrateBuildCorpusClip(xTickClip, u, static_cast<float>(axANIM_MIGRATE_CORPUS[u].m_uTicksPerSecond));
		ZENITH_ASSERT_FALSE(Flux_ClipKeyTimesFitDuration(xTickClip),
			"a schema-1 clip carries key times on a TICK grid beside a duration in seconds — if this fitted, "
			"the corpus is not actually exercising the conversion");
		AnimMigrateWriteClipAtSchema(xTickClip, (u == 2u ? xNested : xAuthored) / strLeaf, 1u);

		Flux_AnimationClip xTwinClip;
		AnimMigrateBuildCorpusClip(xTwinClip, u, 1.0f);
		AnimMigrateWriteClipAtSchema(xTwinClip, xTwins / strLeaf, uZENITH_ANIMATION_SCHEMA_CURRENT);
	}

	Zenith_Tools_AnimMigrateReport xReport;
	ZENITH_ASSERT_TRUE(Zenith_Tools_MigrateAuthoredClips(xAuthored, xReport), "a clean schema-1 corpus migrates");
	ZENITH_ASSERT_EQ(xReport.m_uScanned, uANIM_MIGRATE_CORPUS_COUNT, "every .zanim under the root is scanned, subdirectories included");
	ZENITH_ASSERT_EQ(xReport.m_uMigrated, uANIM_MIGRATE_CORPUS_COUNT, "and every one of them is carried forward");
	ZENITH_ASSERT_EQ(xReport.m_uSkippedCurrent, 0u, "none of them was already current");
	ZENITH_ASSERT_EQ(xReport.m_uFailed, 0u, "and none was refused");
	ZENITH_ASSERT_TRUE(xReport.CountsAddUp(), "the counts account for every scanned file");
	ZENITH_ASSERT_EQ(AnimMigrateCountTempFiles(xAuthored), 0u, "no staged .migrate.tmp survives the walk");

	for (u_int u = 0; u < uANIM_MIGRATE_CORPUS_COUNT; ++u)
	{
		const std::string strLeaf = std::string(axANIM_MIGRATE_CORPUS[u].m_szClipName) + ZENITH_ANIMATION_EXT;
		const std::filesystem::path xMigratedPath = (u == 2u ? xNested : xAuthored) / strLeaf;
		const std::filesystem::path xTwinPath = xTwins / strLeaf;

		u_int uSchema = 0u;
		ZENITH_ASSERT_TRUE(AnimMigrateFileSchema(xMigratedPath, uSchema), "the migrated file still carries an animation envelope");
		ZENITH_ASSERT_EQ(uSchema, uZENITH_ANIMATION_SCHEMA_CURRENT, "and it now declares the current schema");

		Flux_AnimationClip xMigrated;
		ZENITH_ASSERT_TRUE(AnimMigrateLoadClip(xMigratedPath, xMigrated),
			"the migrated file loads through the RUNTIME reader, which refuses any non-current schema");

		Flux_AnimationClip xTwin;
		ZENITH_ASSERT_TRUE(AnimMigrateLoadClip(xTwinPath, xTwin), "the seconds-authored twin loads");

		ZENITH_ASSERT_TRUE(Flux_ClipKeyTimesFitDuration(xMigrated),
			"after the migration the key times and the duration are on ONE clock");
		ZENITH_ASSERT_TRUE(AnimMigrateSamplesMatch(xMigrated, xTwin, axANIM_MIGRATE_CORPUS[u].m_fDurationSeconds),
			"the migrated clip samples identically to its seconds-authored twin at every matched time");

		// D5: the same animation data must serialize to the same bytes, whatever
		// route it took to get there. This is what makes an authored clip's
		// migration a ONE-TIME diff in git rather than churn on every boot.
		ZENITH_ASSERT_TRUE(AnimMigrateReadBytes(xMigratedPath) == AnimMigrateReadBytes(xTwinPath),
			"the migrated bytes are IDENTICAL to the seconds-authored twin's bytes");
	}
}

//==============================================================================
// (2) An already-current file is a NO-OP — not an error, and not a rewrite.
//     Re-running the migrator over a tree it has already migrated changes nothing.
//==============================================================================
ZENITH_TEST(AnimMigrate, AnAlreadyCurrentFileIsSkippedAndTheWalkIsIdempotent)
{
	AnimMigrateTempTree xTree("zenith_animmigrate_noop");
	const std::filesystem::path xAuthored = xTree.Dir("Authored");
	const std::filesystem::path xOldPath = xAuthored / ("Old" ZENITH_ANIMATION_EXT);
	const std::filesystem::path xCurrentPath = xAuthored / ("Current" ZENITH_ANIMATION_EXT);

	Flux_AnimationClip xOldClip;
	AnimMigrateBuildCorpusClip(xOldClip, 0u, static_cast<float>(axANIM_MIGRATE_CORPUS[0].m_uTicksPerSecond));
	AnimMigrateWriteClipAtSchema(xOldClip, xOldPath, 1u);

	Flux_AnimationClip xCurrentClip;
	AnimMigrateBuildCorpusClip(xCurrentClip, 1u, 1.0f);
	AnimMigrateWriteClipAtSchema(xCurrentClip, xCurrentPath, uZENITH_ANIMATION_SCHEMA_CURRENT);
	const std::string strCurrentBytesBefore = AnimMigrateReadBytes(xCurrentPath);

	Zenith_Tools_AnimMigrateReport xFirstRun;
	ZENITH_ASSERT_TRUE(Zenith_Tools_MigrateAuthoredClips(xAuthored, xFirstRun), "the first walk succeeds");
	ZENITH_ASSERT_EQ(xFirstRun.m_uScanned, 2u, "both files are scanned");
	ZENITH_ASSERT_EQ(xFirstRun.m_uMigrated, 1u, "only the stale one is rewritten");
	ZENITH_ASSERT_EQ(xFirstRun.m_uSkippedCurrent, 1u, "and the current one is SKIPPED, not re-serialized");
	ZENITH_ASSERT_EQ(xFirstRun.m_uFailed, 0u, "a no-op is not a failure");
	ZENITH_ASSERT_TRUE(xFirstRun.CountsAddUp(), "the counts account for every scanned file");

	// ★ THE POINT OF THE SKIP. These files are COMMITTED, so a boot that
	// re-serialized every authored clip — even to identical content — would still
	// have to touch and rewrite each one. Its bytes are not merely equal; the
	// migrator never opened it for writing.
	ZENITH_ASSERT_TRUE(AnimMigrateReadBytes(xCurrentPath) == strCurrentBytesBefore,
		"an already-current file's bytes are untouched");

	const std::string strMigratedBytes = AnimMigrateReadBytes(xOldPath);

	Zenith_Tools_AnimMigrateReport xSecondRun;
	ZENITH_ASSERT_TRUE(Zenith_Tools_MigrateAuthoredClips(xAuthored, xSecondRun), "the second walk succeeds");
	ZENITH_ASSERT_EQ(xSecondRun.m_uScanned, 2u, "both files are scanned again");
	ZENITH_ASSERT_EQ(xSecondRun.m_uMigrated, 0u, "nothing is left to migrate");
	ZENITH_ASSERT_EQ(xSecondRun.m_uSkippedCurrent, 2u, "both are now current");
	ZENITH_ASSERT_TRUE(AnimMigrateReadBytes(xOldPath) == strMigratedBytes,
		"and the file migrated by the first walk is byte-identical after the second");
	ZENITH_ASSERT_TRUE(AnimMigrateReadBytes(xCurrentPath) == strCurrentBytesBefore,
		"as is the one that was already current");

	// A shared report accumulates across walks rather than being reset by them.
	Zenith_Tools_AnimMigrateReport xShared;
	Zenith_Tools_MigrateAuthoredClips(xAuthored, xShared);
	Zenith_Tools_MigrateAuthoredClips(xAuthored, xShared);
	ZENITH_ASSERT_EQ(xShared.m_uScanned, 4u, "one report accumulates across roots/walks");
	ZENITH_ASSERT_TRUE(xShared.CountsAddUp(), "and still adds up");
}

//==============================================================================
// (3) Every corrupt shape is REFUSED and left byte-identical — and a good file in
//     the same walk is still migrated.
//==============================================================================
ZENITH_TEST(AnimMigrate, ACorruptFileIsReportedFailedAndLeftByteIdentical)
{
	AnimMigrateTempTree xTree("zenith_animmigrate_corrupt");
	const std::filesystem::path xAuthored = xTree.Dir("Authored");

	const std::filesystem::path xGoodPath      = xAuthored / ("Good"      ZENITH_ANIMATION_EXT);
	const std::filesystem::path xTruncatedPath = xAuthored / ("Truncated" ZENITH_ANIMATION_EXT);
	const std::filesystem::path xWrongTypePath = xAuthored / ("WrongType" ZENITH_ANIMATION_EXT);
	const std::filesystem::path xFuturePath    = xAuthored / ("Future"    ZENITH_ANIMATION_EXT);

	Flux_AnimationClip xClip;
	AnimMigrateBuildCorpusClip(xClip, 0u, static_cast<float>(axANIM_MIGRATE_CORPUS[0].m_uTicksPerSecond));
	AnimMigrateWriteClipAtSchema(xClip, xGoodPath, 1u);

	// (a) A TRUNCATED PAYLOAD. Three bytes off the end, so the final key-time read
	// crosses end-of-file — Zenith_DataStream refuses an over-long read and leaves
	// the cursor where it was, which is exactly why "the payload ends at EOF" is
	// the detector rather than a return value from the parse.
	{
		std::string strBytes = AnimMigrateReadBytes(xGoodPath);
		ZENITH_ASSERT_TRUE(strBytes.size() > 3u, "the corpus clip has a payload to truncate");
		AnimMigrateWriteBytes(xTruncatedPath, strBytes.substr(0, strBytes.size() - 3u));
	}

	// (b) ANOTHER ASSET'S TYPE ID. A .zskel that somebody renamed to .zanim is a
	// wrong-type file, not an old one.
	{
		std::string strBytes = AnimMigrateReadBytes(xGoodPath);
		AnimMigratePokeHeaderWord(strBytes, uANIM_MIGRATE_WORD_TYPE_ID, uZENITH_SKELETON_ASSET_TYPE_ID);
		AnimMigrateWriteBytes(xWrongTypePath, strBytes);
	}

	// (c) A SCHEMA FROM THE FUTURE. There is no backwards step and there never
	// will be: the newer layout is not described anywhere in this build.
	{
		std::string strBytes = AnimMigrateReadBytes(xGoodPath);
		AnimMigratePokeHeaderWord(strBytes, uANIM_MIGRATE_WORD_SCHEMA, 99u);
		AnimMigrateWriteBytes(xFuturePath, strBytes);
	}

	const std::string strTruncatedBefore = AnimMigrateReadBytes(xTruncatedPath);
	const std::string strWrongTypeBefore = AnimMigrateReadBytes(xWrongTypePath);
	const std::string strFutureBefore    = AnimMigrateReadBytes(xFuturePath);

	Zenith_Tools_AnimMigrateReport xReport;
	bool bResult = true;
	{
		// Every refusal asserts on purpose, and the truncated file also trips
		// Zenith_DataStream's own read-past-end assert. The COUNT is deliberately not
		// pinned — it is a property of how far the truncation happens to get, which
		// is not something this test should freeze.
		Zenith_AssertCaptureScope xCapture;
		bResult = Zenith_Tools_MigrateAuthoredClips(xAuthored, xReport);
		ZENITH_ASSERT_TRUE(xCapture.DidAssertFire(), "a refusal is LOUD, not just counted");
	}

	ZENITH_ASSERT_FALSE(bResult, "a walk with refusals does not report success");
	ZENITH_ASSERT_EQ(xReport.m_uScanned, 4u, "all four files are scanned");
	ZENITH_ASSERT_EQ(xReport.m_uMigrated, 1u, "the good file is still migrated");
	ZENITH_ASSERT_EQ(xReport.m_uSkippedCurrent, 0u, "none of them was already current");
	ZENITH_ASSERT_EQ(xReport.m_uFailed, 3u, "and all three corrupt shapes are refused");
	ZENITH_ASSERT_TRUE(xReport.CountsAddUp(), "the counts account for every scanned file");
	ZENITH_ASSERT_FALSE(xReport.m_strFirstFailurePath.empty(), "the report names the first refusal");
	ZENITH_ASSERT_FALSE(xReport.m_strFirstFailureReason.empty(), "and says why");

	// ★ THE ACCEPTANCE PROPERTY: a corrupt input never yields a truncated output.
	// Each of these is byte-for-byte what it was, and no staged temp file is left
	// behind for a later walk to trip over.
	ZENITH_ASSERT_TRUE(AnimMigrateReadBytes(xTruncatedPath) == strTruncatedBefore, "the truncated file is untouched");
	ZENITH_ASSERT_TRUE(AnimMigrateReadBytes(xWrongTypePath) == strWrongTypeBefore, "the wrong-type file is untouched");
	ZENITH_ASSERT_TRUE(AnimMigrateReadBytes(xFuturePath)    == strFutureBefore,    "the future-schema file is untouched");
	ZENITH_ASSERT_EQ(AnimMigrateCountTempFiles(xAuthored), 0u, "no staged .migrate.tmp survives a failed file");

	u_int uGoodSchema = 0u;
	ZENITH_ASSERT_TRUE(AnimMigrateFileSchema(xGoodPath, uGoodSchema), "the good file still has its envelope");
	ZENITH_ASSERT_EQ(uGoodSchema, uZENITH_ANIMATION_SCHEMA_CURRENT, "and reached the current schema despite its neighbours");
}

//==============================================================================
// (4) A schema-1 clip declaring ZERO ticks per second is a loud refusal, not a
//     division by zero and not a guessed 24.
//==============================================================================
ZENITH_TEST(AnimMigrate, AZeroTicksPerSecondIsALoudRefusalNotAGuess)
{
	AnimMigrateTempTree xTree("zenith_animmigrate_zerotps");
	const std::filesystem::path xAuthored = xTree.Dir("Authored");
	const std::filesystem::path xPath = xAuthored / ("ZeroRate" ZENITH_ANIMATION_EXT);

	Flux_AnimationClip xClip;
	AnimMigrateBuildCorpusClip(xClip, 0u, 24.0f);
	// The metadata says the key times are ticks on a grid of NO ticks per second.
	// There is no conversion; guessing one would place every key in the clip
	// somewhere it was never authored, which only a person watching the animation
	// months later would ever notice.
	xClip.SetTicksPerSecond(0u);
	AnimMigrateWriteClipAtSchema(xClip, xPath, 1u);
	const std::string strBytesBefore = AnimMigrateReadBytes(xPath);

	Zenith_Tools_AnimMigrateReport xReport;
	bool bResult = true;
	{
		Zenith_AssertCaptureScope xCapture;
		bResult = Zenith_Tools_MigrateAuthoredClips(xAuthored, xReport);
		ZENITH_ASSERT_TRUE(xCapture.DidAssertFire(), "the zero tick rate is reported LOUDLY");
	}

	ZENITH_ASSERT_FALSE(bResult, "the walk does not report success");
	ZENITH_ASSERT_EQ(xReport.m_uScanned, 1u, "the file is scanned");
	ZENITH_ASSERT_EQ(xReport.m_uMigrated, 0u, "and is NOT rewritten");
	ZENITH_ASSERT_EQ(xReport.m_uFailed, 1u, "it is counted as a refusal");
	ZENITH_ASSERT_TRUE(xReport.CountsAddUp(), "the counts account for every scanned file");
	ZENITH_ASSERT_TRUE(AnimMigrateReadBytes(xPath) == strBytesBefore, "and the file is left byte-identical");
}

//==============================================================================
// (5) The normal case today: no authored tree at all. It must be cheap, silent
//     and SUCCESSFUL — recursive_directory_iterator THROWS on a missing path, and
//     an unhandled throw in a boot phase kills the tools boot before any export
//     runs.
//==============================================================================
ZENITH_TEST(AnimMigrate, AnAbsentOrEmptyAuthoredRootIsSuccessNotAThrow)
{
	AnimMigrateTempTree xTree("zenith_animmigrate_absent");

	Zenith_Tools_AnimMigrateReport xReport;

	ZENITH_ASSERT_TRUE(Zenith_Tools_MigrateAuthoredClips(xTree.m_xRoot / "NoSuchDirectory", xReport),
		"a missing authored root is SUCCESS — a tree with no authored overrides is the normal state");
	ZENITH_ASSERT_EQ(xReport.m_uScanned, 0u, "and nothing is scanned");

	ZENITH_ASSERT_TRUE(Zenith_Tools_MigrateAuthoredClips(xTree.Dir("Empty"), xReport), "an empty authored root is success");
	ZENITH_ASSERT_EQ(xReport.m_uScanned, 0u, "and still nothing is scanned");

	// A file that is not a .zanim is not this phase's business.
	const std::filesystem::path xMixed = xTree.Dir("Mixed");
	AnimMigrateWriteBytes(xMixed / "notes.txt", std::string("this is not a clip"));
	AnimMigrateWriteBytes(xMixed / ("Leftover" ZENITH_ANIMATION_EXT ".migrate.tmp"), std::string("nor is this"));
	ZENITH_ASSERT_TRUE(Zenith_Tools_MigrateAuthoredClips(xMixed, xReport), "a root with no .zanim is success");
	ZENITH_ASSERT_EQ(xReport.m_uScanned, 0u,
		"neither a foreign file nor a leftover .migrate.tmp is picked up as input");
	ZENITH_ASSERT_TRUE(xReport.CountsAddUp(), "the counts account for every scanned file");
}
