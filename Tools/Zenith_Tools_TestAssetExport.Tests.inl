#include "UnitTests/Zenith_UnitTests.h"
#include "Core/Zenith_TestFramework.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"

#include <cmath>

// ============================================================================
// StickFigure asset tests.
//
// ★★ THE SEVENTEEN CLIPS ARE READ FROM DISK, NOT BUILT (WU-9.1 stage 2).
// There are no Create*Animation factories any more: the clips are AUTHORED data
// under `engine:Authored/Meshes/StickFigure/`, committed, hand-edited in the
// Animation Editor and written by no generator. So every clip assertion below
// loads the FILE through the asset registry — which is both the only thing left
// to check and a strictly better thing to check, because the file is what the
// three games actually bind.
//
// ★ THESE UNITS ARE NOW THE HOME OF THE BAKE-TIME GATES. GenerateStickFigureAssets'
// export loop used to assert, on every clip it wrote, that it drove both
// UpperArms and named the shared rig. That loop is gone with the generators. The
// same two properties are asserted here over the tracked files, plus the ones the
// loop never checked: that each file PARSES at the current schema, that its keys
// fit its stated duration, and that the set's names, durations and loop flags are
// the ones the Combat hit windows and the tennis testbed were built around.
//
// ★ A FAILURE HERE MEANS A FILE IS WRONG, AND THE FIX IS AN EDIT OR A REVERT,
// never a re-bake: nothing regenerates these. `zagent`-style advice does not
// apply — `del` plus a boot DESTROYS an authored clip (D21).
//
// Headless: a clip is pure CPU data and the registry resolves an engine: path
// without a device. None of these is requiresGraphics.
//
// Note: Flux_BoneChannel::SampleRotation takes time in SECONDS (D3) — the same
// clock as Flux_AnimationClip::GetDuration.
// ============================================================================

namespace
{
	bool StickFigureQuatEquals(const Zenith_Maths::Quat& a, const Zenith_Maths::Quat& b, float fTol = 1e-4f)
	{
		// Quaternion comparison must allow for double-cover (q and -q are the same rotation).
		float fDot = std::abs(a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z);
		return std::abs(fDot - 1.0f) < fTol;
	}

	// The 24 fps grid every one of these clips was authored on, and still declares
	// in its own m_uAuthoredFrameRate (D6, asserted below). A test that wants "the
	// pose at frame N" says StickFigureFrameSeconds(N) — a channel stores SECONDS.
	constexpr float fSTICKFIGURE_AUTHORED_FPS = 24.0f;

	constexpr float StickFigureFrameSeconds(float fFrame) { return fFrame / fSTICKFIGURE_AUTHORED_FPS; }

	//--------------------------------------------------------------------------
	// Load one authored clip THROUGH THE ASSET REGISTRY, by the same asset path a
	// game binds — `engine:Authored/Meshes/StickFigure/StickFigure_<Name>.zanim`,
	// built by the shipped helpers rather than spelled out, so a test cannot pass
	// against a path no consumer uses.
	//
	// nullptr when the file is missing or refused. Zenith_AnimationAsset::
	// LoadFromFile returns the parse status and the registry DELETES the asset on
	// failure rather than caching an empty one, so a non-null return here really
	// does mean "parsed at the current schema".
	//--------------------------------------------------------------------------
	const Flux_AnimationClip* StickFigureAuthoredClip(const char* szClipName)
	{
		const std::string strAssetPath =
			Zenith_Tools_StickFigureAuthoredPath(Zenith_Tools_StickFigureClipFileName(szClipName).c_str());
		const Zenith_AnimationAsset* pxAsset =
			Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>(strAssetPath);
		return (pxAsset != nullptr) ? pxAsset->GetClip() : nullptr;
	}

	//--------------------------------------------------------------------------
	// ★ THE TIMINGS ARE A CONTRACT WITH THE GAMES, so they are pinned by name.
	//
	// A duration here is not decoration: Combat's hit windows are a fraction of
	// the attack clip's length (30-70% normalized), RenderTest's shooter blends
	// Aim/Fire/Reload against each other, and the tennis state machine's swing
	// timing is the Serve/Forehand/Backhand lengths. Retiming a clip in the editor
	// is allowed — this table then moves WITH it, deliberately, in the same commit,
	// so "the animation got longer" can never be an invisible gameplay change.
	//
	// The numbers are the ones the seventeen files were authored with (the WU-9.1
	// stage-1 seeding, commit 2d772cd5); the NAME comes from
	// azZENITH_STICKFIGURE_CLIP_NAMES rather than being retyped, and the two are
	// asserted to agree so this table cannot silently address a different clip.
	//--------------------------------------------------------------------------
	struct StickFigureClipTiming
	{
		const char* m_szName;
		float       m_fDurationSeconds;
		bool        m_bLooping;
	};

	const StickFigureClipTiming axSTICKFIGURE_CLIP_TIMINGS[uZENITH_STICKFIGURE_CLIP_COUNT] =
	{
		{ "Idle",        2.00f, true  },
		{ "Walk",        1.00f, true  },
		{ "Run",         0.50f, true  },
		{ "Attack1",     0.40f, false },
		{ "Attack2",     0.40f, false },
		{ "Attack3",     0.50f, false },
		{ "Dodge",       0.50f, false },
		{ "Hit",         0.30f, false },
		{ "Death",       1.00f, false },
		{ "Aim",         0.50f, true  },
		{ "Fire",        0.20f, false },
		{ "Reload",      1.50f, false },
		{ "Jump",        0.80f, false },
		{ "Serve",       1.25f, false },
		{ "Forehand",    0.75f, false },
		{ "Backhand",    0.75f, false },
		{ "ReadyStance", 1.50f, true  },
	};
}

// ----- The whole set, off disk ------------------------------------------------

ZENITH_TEST(StickFigureAuthored, EveryAuthoredClipLoadsAndCarriesTheRigContract)
{
	// ★ THE REPLACEMENT FOR THE EXPORTER'S GATE. Everything
	// GenerateStickFigureAssets used to assert as it wrote a clip, asserted here
	// against the file instead — plus the two things it could not check, because
	// it held the clip in memory and never read it back: that the bytes on disk
	// PARSE at the current schema, and that the flag says AUTHORED.
	//
	// ★ AND THE PREFIX IS PART OF THE ASSERTION, not decoration.
	// Zenith_AssetRegistry::NormalizeAssetPath leaves a bare RELATIVE path exactly
	// as it found it, so "Meshes/StickFigure/StickFigure.zskel" would satisfy a
	// "non-empty" check, round-trip through the stream unchanged, and resolve to
	// nothing (Docs/HumanoidImport.md invariant 6).
	ZENITH_ASSERT_EQ(uZENITH_STICKFIGURE_CLIP_COUNT, 17u,
		"the StickFigure clip set is seventeen files");

	for (u_int u = 0; u < uZENITH_STICKFIGURE_CLIP_COUNT; u++)
	{
		const char* szName = azZENITH_STICKFIGURE_CLIP_NAMES[u];
		const Flux_AnimationClip* pxClip = StickFigureAuthoredClip(szName);

		// A null here is a MISSING OR CORRUPT COMMITTED FILE. It is not fixable by
		// a re-bake and the message says so, because the reflex is wrong.
		ZENITH_ASSERT_NOT_NULL(pxClip,
			"authored clip '%s' did not load from %s -- these files are COMMITTED and no bake writes them; "
			"restore it from git rather than re-baking",
			szName, Zenith_Tools_StickFigureAuthoredPath(
				Zenith_Tools_StickFigureClipFileName(szName).c_str()).c_str());
		if (pxClip == nullptr)
		{
			continue;
		}

		const Flux_AnimationClipMetadata& xMeta = pxClip->GetMetadata();

		ZENITH_ASSERT_STREQ(pxClip->GetName().c_str(), szName,
			"the clip in file %u is not the one its name says it is", u);
		ZENITH_ASSERT_FALSE(xMeta.m_bGenerated,
			"clip '%s' is AUTHORED and must not claim to be regenerated on every boot (D8) -- "
			"a consumer reads that flag as 'editing this file is pointless'", szName);
		ZENITH_ASSERT_STREQ(xMeta.m_strSkeletonPath.c_str(),
			"engine:Meshes/StickFigure/StickFigure.zskel",
			"clip '%s' does not name the ONE shared humanoid rig, engine:-prefixed", szName);
		ZENITH_ASSERT_TRUE(!xMeta.m_strPreviewModelPath.empty(),
			"clip '%s' names no model to preview it on (D7)", szName);
		ZENITH_ASSERT_EQ(xMeta.m_uAuthoredFrameRate, 24u,
			"clip '%s' authored frame rate must be the 24 fps grid it was keyed on (D6)", szName);

		// ★★ EVERY CLIP DRIVES BOTH UPPER ARMS. Also asserted on its own below;
		// it is here as well because this test is the exporter gate's replacement
		// and that gate is exactly what it was.
		ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("LeftUpperArm") && pxClip->HasBoneChannel("RightUpperArm"),
			"clip '%s' does not animate both UpperArms -- a T-posed human would hold that arm out", szName);

		// ★ FIRE IS EXEMPT, DELIBERATELY, AND THE NUMBERS ARE HERE SO THE EXEMPTION
		// CAN BE FALSIFIED. Fire's recoil channels carry a settle key at authored
		// frame 5 = 5/24 = 0.208333 s against a stated duration of 0.20 s — 8.3 ms
		// past the end. Decision D13 permits a key past the duration (the mutators
		// do not veto one; a panel warns), and the duration itself is pinned at
		// 0.20 s by the timing table, so moving the duration to close the gap would
		// red that instead. The exemption is the smaller lie of the two, and it is
		// one clip.
		if (std::string(szName) == "Fire")
		{
			ZENITH_ASSERT_EQ_FLOAT(Flux_ClipLastKeyTimeSeconds(*pxClip), 5.0f / 24.0f, 1e-4f,
				"Fire's last key is no longer authored frame 5 -- the D13 exemption may be unnecessary now");
		}
		else
		{
			ZENITH_ASSERT_TRUE(Flux_ClipKeyTimesFitDuration(*pxClip),
				"clip '%s' carries a key past its %.4f s duration (last key at %.4f s)",
				szName, pxClip->GetDuration(), Flux_ClipLastKeyTimeSeconds(*pxClip));
		}
	}
}

ZENITH_TEST(StickFigureAuthored, EveryClipDrivesBothUpperArms)
{
	// ★★ THE ONE RIG DEPENDENCY, AND ITS OWN TEST SO A FAILURE NAMES IT.
	// A bone a clip omits keeps its BIND local transform, and the two UpperArms
	// are the only bones whose T-pose bind rotation is not identity
	// (Zenith_HumanArmBindRotation) — so a clip that omits one leaves that arm
	// sticking straight out sideways for its whole duration, on StickFigure, on
	// Zenithmon's humans and on every imported artist humanoid alike.
	//
	// This used to be a Zenith_Assert inside GenerateStickFigureAssets' export
	// loop, i.e. a check on data that had just been built in memory. It is a check
	// on the FILE now, which is the only form that can catch the way these clips
	// actually change: somebody editing one in the Animation Editor and deleting a
	// channel.
	for (u_int u = 0; u < uZENITH_STICKFIGURE_CLIP_COUNT; u++)
	{
		const char* szName = azZENITH_STICKFIGURE_CLIP_NAMES[u];
		const Flux_AnimationClip* pxClip = StickFigureAuthoredClip(szName);
		ZENITH_ASSERT_NOT_NULL(pxClip, "authored clip '%s' must load", szName);
		if (pxClip == nullptr)
		{
			continue;
		}

		ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("LeftUpperArm"),
			"clip '%s' does not animate LeftUpperArm -- a T-posed human would hold that arm out", szName);
		ZENITH_ASSERT_TRUE(pxClip->HasBoneChannel("RightUpperArm"),
			"clip '%s' does not animate RightUpperArm -- a T-posed human would hold that arm out", szName);
	}
}

ZENITH_TEST(StickFigureAuthored, TheSetsNamesDurationsAndLoopFlagsAreUnchanged)
{
	// ★ A RETIME IS A GAMEPLAY CHANGE, and this is what makes it visible. Combat's
	// hit windows are a fraction of an attack clip's length, the shooter blends
	// Aim/Fire/Reload against one another, and the tennis swing timing IS the
	// Serve/Forehand/Backhand durations. Nothing else in the tree would notice a
	// clip getting 30% longer in an editor session.
	for (u_int u = 0; u < uZENITH_STICKFIGURE_CLIP_COUNT; u++)
	{
		const StickFigureClipTiming& xTiming = axSTICKFIGURE_CLIP_TIMINGS[u];

		// One list of names, not two: this table only adds the timing columns.
		ZENITH_ASSERT_STREQ(xTiming.m_szName, azZENITH_STICKFIGURE_CLIP_NAMES[u],
			"the timing table and the clip-name table disagree at index %u", u);

		const Flux_AnimationClip* pxClip = StickFigureAuthoredClip(xTiming.m_szName);
		ZENITH_ASSERT_NOT_NULL(pxClip, "authored clip '%s' must load", xTiming.m_szName);
		if (pxClip == nullptr)
		{
			continue;
		}

		ZENITH_ASSERT_EQ_FLOAT(pxClip->GetDuration(), xTiming.m_fDurationSeconds, 1e-4f,
			"clip '%s' has been retimed -- update this table in the same commit if that was intended",
			xTiming.m_szName);
		ZENITH_ASSERT_TRUE(pxClip->IsLooping() == xTiming.m_bLooping,
			"clip '%s' loop flag changed -- a looping action clip (or a one-shot idle) is a state-machine bug",
			xTiming.m_szName);
		ZENITH_ASSERT_EQ(pxClip->GetTicksPerSecond(), 24u,
			"clip '%s' import provenance moved off the 24 fps grid", xTiming.m_szName);
	}
}

ZENITH_TEST(StickFigureAuthored, TheAuthoredPathKeepsTheRootPrefixAndTheSubdirectory)
{
	// ★ THE PREFIX AND THE SUBDIRECTORY ARE BOTH PART OF THE ASSERTION.
	// NormalizeAssetPath leaves a bare RELATIVE path exactly as it found it, so a
	// ref without "engine:" would serialize cleanly, load cleanly and resolve to
	// nothing; and flattening "Meshes/StickFigure/" away would put every set's
	// "Walk" on one path, so the next promoted clip would silently overwrite this
	// one. Same rule as Zenith_AnimationDocument::BuildAuthoredAssetPath, matched
	// here rather than called — Tools may not include Editor.

	// One row spelled out in full, with nothing constructed, so at least one
	// expectation cannot drift with the helper it is checking.
	ZENITH_ASSERT_STREQ(Zenith_Tools_StickFigureClipFileName("Idle").c_str(),
		"StickFigure_Idle.zanim", "the clip file naming moved");
	ZENITH_ASSERT_STREQ(Zenith_Tools_StickFigureAuthoredPath("StickFigure_Idle.zanim").c_str(),
		"engine:Authored/Meshes/StickFigure/StickFigure_Idle.zanim",
		"the authored path for the Idle clip moved");

	for (u_int u = 0; u < uZENITH_STICKFIGURE_CLIP_COUNT; u++)
	{
		const char* szName = azZENITH_STICKFIGURE_CLIP_NAMES[u];

		const std::string strFileName = Zenith_Tools_StickFigureClipFileName(szName);
		const std::string strExpectedFileName = std::string("StickFigure_") + szName + ".zanim";
		ZENITH_ASSERT_STREQ(strFileName.c_str(), strExpectedFileName.c_str(),
			"clip '%s' does not use the set's own file naming", szName);

		const std::string strExpectedPath = "engine:Authored/Meshes/StickFigure/" + strFileName;
		ZENITH_ASSERT_STREQ(Zenith_Tools_StickFigureAuthoredPath(strFileName.c_str()).c_str(),
			strExpectedPath.c_str(),
			"clip '%s' does not map into engine:Authored/Meshes/StickFigure/", szName);
	}

	// And the DIRECTORY the exporter reads back from is the same location the
	// asset path describes — an asset path nothing reads would resolve to a file
	// nobody notices is missing.
	const std::string strDir = Zenith_Tools_StickFigureAuthoredDir();
	ZENITH_ASSERT_TRUE(strDir.ends_with("Authored/Meshes/StickFigure/"),
		"the authored directory '%s' is not the location engine:Authored/Meshes/StickFigure/ resolves to",
		strDir.c_str());
}

// ----- Aim / Fire / Reload: the shooter set's continuity -----------------------
//
// ★ THESE THREE MUST AGREE WITH EACH OTHER, and that agreement is now checked
// BETWEEN THE FILES rather than against a constant. The clips used to share a
// `StickFigureAimHoldPose` helper in the generator, and the tests compared each
// clip's end keys against it; with the generator deleted, re-typing those six
// quaternions into this file would invent a second authority for a pose that now
// exists only in the .zanim files. So Aim's own pose is the reference, read off
// the file, and Fire and Reload are asserted to return to it. A pose edit that
// moves all three together is legal and stays green — which is correct, because
// what matters is that the transitions do not snap.

ZENITH_TEST(StickFigureAuthored, AimHoldsASteadyUpperBodyPose)
{
	const Flux_AnimationClip* pxAim = StickFigureAuthoredClip("Aim");
	ZENITH_ASSERT_NOT_NULL(pxAim, "the Aim clip must load");
	if (pxAim == nullptr)
	{
		return;
	}

	// The shooter's upper body is what this clip is for.
	ZENITH_ASSERT_TRUE(pxAim->HasBoneChannel("RightUpperArm"), "Aim missing RightUpperArm channel");
	ZENITH_ASSERT_TRUE(pxAim->HasBoneChannel("RightLowerArm"), "Aim missing RightLowerArm channel");
	ZENITH_ASSERT_TRUE(pxAim->HasBoneChannel("LeftUpperArm"),  "Aim missing LeftUpperArm channel");
	ZENITH_ASSERT_TRUE(pxAim->HasBoneChannel("LeftLowerArm"),  "Aim missing LeftLowerArm channel");
	ZENITH_ASSERT_TRUE(pxAim->HasBoneChannel("Spine"),         "Aim missing Spine channel");
	ZENITH_ASSERT_TRUE(pxAim->HasBoneChannel("Head"),          "Aim missing Head channel");

	// It LOOPS, and the loop must not pop: the pose at the start and at the
	// authored end (frame 12 = 0.5 s) are the same. A breathing waver lives on the
	// mid keys and is deliberately not pinned.
	const Flux_BoneChannel* pxCh = pxAim->GetBoneChannel("RightUpperArm");
	ZENITH_ASSERT_NOT_NULL(pxCh, "Aim should have a RightUpperArm channel");
	if (pxCh == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(pxCh->SampleRotation(0.0f),
		pxCh->SampleRotation(StickFigureFrameSeconds(12.0f))),
		"Aim's RightUpperArm does not return to its start pose at the end of the loop");
}

ZENITH_TEST(StickFigureAuthored, FireAndReloadReturnToAimsHoldPose)
{
	// ★ A SNAP IS THE FAILURE THIS CATCHES, and no screenshot pass would: the
	// arms simply jump to a different pose for one blend when the state machine
	// goes Fire -> Aim or Reload -> Aim. Read the last authored key directly —
	// SampleRotation's end-of-clip behaviour is a clamp, so a boundary sample is
	// the wrong instrument for "what does this clip END on".
	const Flux_AnimationClip* pxAim = StickFigureAuthoredClip("Aim");
	const Flux_AnimationClip* pxFire = StickFigureAuthoredClip("Fire");
	const Flux_AnimationClip* pxReload = StickFigureAuthoredClip("Reload");
	ZENITH_ASSERT_NOT_NULL(pxAim, "the Aim clip must load");
	ZENITH_ASSERT_NOT_NULL(pxFire, "the Fire clip must load");
	ZENITH_ASSERT_NOT_NULL(pxReload, "the Reload clip must load");
	if (pxAim == nullptr || pxFire == nullptr || pxReload == nullptr)
	{
		return;
	}

	struct ContinuityCheck { const Flux_AnimationClip* m_pxClip; const char* m_szClip; const char* m_szBone; };
	const ContinuityCheck axChecks[] = {
		{ pxFire,   "Fire",   "RightUpperArm" },
		{ pxFire,   "Fire",   "LeftUpperArm"  },
		{ pxReload, "Reload", "LeftUpperArm"  },
		{ pxReload, "Reload", "RightUpperArm" },
	};

	for (const ContinuityCheck& xCheck : axChecks)
	{
		const Flux_BoneChannel* pxAimCh = pxAim->GetBoneChannel(xCheck.m_szBone);
		const Flux_BoneChannel* pxCh = xCheck.m_pxClip->GetBoneChannel(xCheck.m_szBone);
		ZENITH_ASSERT_NOT_NULL(pxAimCh, "Aim should have a %s channel", xCheck.m_szBone);
		ZENITH_ASSERT_NOT_NULL(pxCh, "%s should have a %s channel", xCheck.m_szClip, xCheck.m_szBone);
		if (pxAimCh == nullptr || pxCh == nullptr)
		{
			continue;
		}

		const auto& axRotations = pxCh->GetRotationKeyframes();
		ZENITH_ASSERT_TRUE(axRotations.GetSize() != 0,
			"%s %s should have rotation keyframes", xCheck.m_szClip, xCheck.m_szBone);
		if (axRotations.GetSize() == 0)
		{
			continue;
		}

		// Aim's hold pose, taken from Aim itself at t = 0.
		const Zenith_Maths::Quat xHold = pxAimCh->SampleRotation(0.0f);
		ZENITH_ASSERT_TRUE(StickFigureQuatEquals(axRotations.GetBack().first, xHold),
			"%s's last %s key is not Aim's hold pose -- the transition back to Aim will snap",
			xCheck.m_szClip, xCheck.m_szBone);

		// ...and it must START there too, or the transition INTO it snaps instead.
		ZENITH_ASSERT_TRUE(StickFigureQuatEquals(axRotations.Get(0).first, xHold),
			"%s's first %s key is not Aim's hold pose -- the transition into it will snap",
			xCheck.m_szClip, xCheck.m_szBone);
	}
}

ZENITH_TEST(StickFigureAuthored, FiresRecoilPeaksAt15DegreesAboveTheAimPose)
{
	// The recoil is authored as a delta ON the aim hold pose: at frame 2 the right
	// upper arm is the hold pose plus 15 degrees about X. Expressed as the RELATIVE
	// rotation between the two clips, so it needs no copy of either pose.
	const Flux_AnimationClip* pxAim = StickFigureAuthoredClip("Aim");
	const Flux_AnimationClip* pxFire = StickFigureAuthoredClip("Fire");
	ZENITH_ASSERT_NOT_NULL(pxAim, "the Aim clip must load");
	ZENITH_ASSERT_NOT_NULL(pxFire, "the Fire clip must load");
	if (pxAim == nullptr || pxFire == nullptr)
	{
		return;
	}

	const Flux_BoneChannel* pxAimCh = pxAim->GetBoneChannel("RightUpperArm");
	const Flux_BoneChannel* pxFireCh = pxFire->GetBoneChannel("RightUpperArm");
	ZENITH_ASSERT_NOT_NULL(pxAimCh, "Aim should have a RightUpperArm channel");
	ZENITH_ASSERT_NOT_NULL(pxFireCh, "Fire should have a RightUpperArm channel");
	if (pxAimCh == nullptr || pxFireCh == nullptr)
	{
		return;
	}

	const Zenith_Maths::Quat xHold = pxAimCh->SampleRotation(0.0f);
	const Zenith_Maths::Quat xPeak = pxFireCh->SampleRotation(StickFigureFrameSeconds(2.0f));

	// xPeak = kick * xHold, so kick = xPeak * inverse(xHold).
	Zenith_Maths::Quat xKick = glm::normalize(xPeak * glm::inverse(xHold));
	if (xKick.w < 0.0f)
	{
		// Shortest arc: q and -q are the same rotation, but glm::angle would report
		// the long way round (> 180 degrees) for the negated one.
		xKick = Zenith_Maths::Quat(-xKick.w, -xKick.x, -xKick.y, -xKick.z);
	}

	const float fAngleDeg = glm::degrees(glm::angle(xKick));
	ZENITH_ASSERT_EQ_FLOAT(fAngleDeg, 15.0f, 0.5f,
		"Fire's recoil peak is %.2f degrees off the aim pose, not 15", fAngleDeg);

	// The axis carries the SIGN: +X is the muzzle rising. A -X kick of the same
	// size would pass an absolute-value check and point the weapon at the floor.
	const Zenith_Maths::Vector3 xAxis = glm::axis(xKick);
	ZENITH_ASSERT_EQ_FLOAT(xAxis.x, 1.0f, 0.02f,
		"Fire's recoil should kick about +X (the muzzle rising), not (%.2f, %.2f, %.2f)",
		xAxis.x, xAxis.y, xAxis.z);
}

ZENITH_TEST(StickFigureAuthored, ReloadKeepsItsEightKeyMagazineCycle)
{
	// Eight keys: rest, drop off the grip, reach to the belt, mag in hand, up to
	// the mag-well, seat it, slap the release, back on the grip. The COUNT is the
	// cheapest description of "the reload still reads as a reload"; dropping one
	// of the middle keys is what turns it into a vague wave.
	const Flux_AnimationClip* pxReload = StickFigureAuthoredClip("Reload");
	ZENITH_ASSERT_NOT_NULL(pxReload, "the Reload clip must load");
	if (pxReload == nullptr)
	{
		return;
	}

	const Flux_BoneChannel* pxCh = pxReload->GetBoneChannel("LeftUpperArm");
	ZENITH_ASSERT_NOT_NULL(pxCh, "Reload should have a LeftUpperArm channel");
	if (pxCh == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_EQ(pxCh->GetRotationKeyframes().GetSize(), 8u,
		"Reload's LeftUpperArm should keep its 8 rotation keys "
		"(rest, drop, reach, grab, lift, seat, slap, rest)");
}

// ----- Jump -------------------------------------------------------------------

ZENITH_TEST(StickFigureAuthored, JumpDrivesBothLegsAndRecoversToIdentity)
{
	const Flux_AnimationClip* pxJump = StickFigureAuthoredClip("Jump");
	ZENITH_ASSERT_NOT_NULL(pxJump, "the Jump clip must load");
	if (pxJump == nullptr)
	{
		return;
	}

	ZENITH_ASSERT_TRUE(pxJump->HasBoneChannel("LeftUpperLeg"),  "Jump missing LeftUpperLeg channel");
	ZENITH_ASSERT_TRUE(pxJump->HasBoneChannel("RightUpperLeg"), "Jump missing RightUpperLeg channel");
	ZENITH_ASSERT_TRUE(pxJump->HasBoneChannel("LeftLowerLeg"),  "Jump missing LeftLowerLeg channel");
	ZENITH_ASSERT_TRUE(pxJump->HasBoneChannel("RightLowerLeg"), "Jump missing RightLowerLeg channel");

	// The spine's LAST key is identity: the jump ends standing, so the blend back
	// to Idle/Walk has nothing to undo. Read the authored key rather than sampling
	// at the boundary (see FireAndReloadReturnToAimsHoldPose).
	const Flux_BoneChannel* pxCh = pxJump->GetBoneChannel("Spine");
	ZENITH_ASSERT_NOT_NULL(pxCh, "Jump should have a Spine channel");
	if (pxCh == nullptr)
	{
		return;
	}
	const auto& axRotations = pxCh->GetRotationKeyframes();
	ZENITH_ASSERT_TRUE(axRotations.GetSize() != 0, "Jump's Spine should have rotation keyframes");
	if (axRotations.GetSize() == 0)
	{
		return;
	}
	ZENITH_ASSERT_TRUE(StickFigureQuatEquals(axRotations.GetBack().first, glm::identity<Zenith_Maths::Quat>()),
		"Jump's last Spine key should be identity -- the landing recovers to standing");
}

// ----- Human body mesh -------------------------------------------------------

ZENITH_TEST(StickFigureBody, BodyMeshInvariants) { Zenith_UnitTests::TestStickFigureBodyMeshInvariants(); }
void Zenith_UnitTests::TestStickFigureBodyMeshInvariants()
{
	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	Zenith_HumanWarp xWarp;
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(xP, xWarp);
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton(xP, xWarp);

	// A lofted human body, not the old 128-vert cube figure.
	ZENITH_ASSERT_TRUE(pxMesh->GetNumVerts() >= 1200, "Body mesh should have at least 1200 verts");
	ZENITH_ASSERT_TRUE(pxMesh->GetNumIndices() >= 6000, "Body mesh should have at least 6000 indices");
	ZENITH_ASSERT_TRUE(pxMesh->m_xBitangents.GetSize() == pxMesh->GetNumVerts(),
		"Body mesh must author bitangents (normal mapping TBN)");
	ZENITH_ASSERT_TRUE(pxMesh->m_xColors.GetSize() == pxMesh->GetNumVerts(),
		"Body mesh must author vertex colors (baked AO)");

	// Bounds: soles below the -1.0 foot bind (inside the 1.05 capsule), crown
	// above the 1.4 head bind.
	ZENITH_ASSERT_TRUE(pxMesh->GetBoundsMin().y < -1.0f && pxMesh->GetBoundsMin().y > -1.06f,
		"Soles should sit just below the foot bind at -1.0");
	ZENITH_ASSERT_TRUE(pxMesh->GetBoundsMax().y > 1.55f && pxMesh->GetBoundsMax().y < 1.65f,
		"Crown should top out just above 1.55");

	for (uint32_t v = 0; v < pxMesh->GetNumVerts(); v++)
	{
		// Weights normalized, bone indices valid.
		const glm::vec4& xW = pxMesh->m_xBoneWeights.Get(v);
		const float fSum = xW.x + xW.y + xW.z + xW.w;
		ZENITH_ASSERT_TRUE(std::abs(fSum - 1.0f) < 0.001f, "Vertex weights must sum to 1");
		const glm::uvec4& xI = pxMesh->m_xBoneIndices.Get(v);
		ZENITH_ASSERT_TRUE(xI.x < STICK_BONE_COUNT && xI.y < STICK_BONE_COUNT,
			"Bone indices must reference the 16-bone rig");

		// UVs inside the atlas, tangent frame finite and unit-ish.
		const Zenith_Maths::Vector2& xUV = pxMesh->m_xUVs.Get(v);
		ZENITH_ASSERT_TRUE(xUV.x >= -0.001f && xUV.x <= 1.001f && xUV.y >= -0.001f && xUV.y <= 1.001f,
			"UVs must stay inside the atlas");
		const Zenith_Maths::Vector3& xT = pxMesh->m_xTangents.Get(v);
		ZENITH_ASSERT_TRUE(std::isfinite(xT.x) && std::isfinite(xT.y) && std::isfinite(xT.z)
			&& std::abs(glm::length(xT) - 1.0f) < 0.01f, "Tangents must be finite unit vectors");
		const Zenith_Maths::Vector3& xN = pxMesh->m_xNormals.Get(v);
		ZENITH_ASSERT_TRUE(std::abs(glm::length(xN) - 1.0f) < 0.01f, "Normals must be unit length");
	}

	delete pxMesh;
	delete pxSkel;
}

ZENITH_TEST(StickFigureBody, BodySmoothSkinning) { Zenith_UnitTests::TestStickFigureBodySmoothSkinning(); }
void Zenith_UnitTests::TestStickFigureBodySmoothSkinning()
{
	// The point of the body overhaul: joints carry BLENDED weights between the
	// adjacent bones so elbows/knees bend smoothly instead of tearing. Verify a
	// genuinely blended vertex exists at each major joint.
	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	Zenith_HumanWarp xWarp;
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(xP, xWarp);
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton(xP, xWarp);

	// ★ RE-DERIVED FROM THE TABLE, never re-typed. These used to be the literals
	// 0.715 and -0.480, which were where the joints happened to be. The whole
	// point of the warp is that a blended vertex now lands on the RIG's joint
	// plane, so asking the table is both the correct check and one that survives
	// the next proportion edit.
	//
	// ★★ THE JOINT IS FOUND BY ITS BONE'S POSITION, NOT BY A HEIGHT. This used to
	// select candidate vertices with |y - ElbowY()| < 0.09, which only ever worked
	// because the arm hung straight down: the rig is T-POSED now (see
	// Zenith_HumanArmBindRotation) and an elbow is a distance OUT along X, at
	// shoulder height, so a height band finds the ribcage instead. Reading the
	// bone's own model-space position covers both poses and cannot go stale the
	// next time one changes -- the bone IS where the joint is, by definition.
	struct JointCheck { uint32_t uBoneA; uint32_t uBoneB; const char* szName; };
	const JointCheck axJoints[] = {
		{ 4 /*LUA*/, 5 /*LLA*/,  "left elbow"  },
		{ 7 /*RUA*/, 8 /*RLA*/,  "right elbow" },
		{ 10 /*LUL*/, 11 /*LLL*/, "left knee"  },
		{ 13 /*RUL*/, 14 /*RLL*/, "right knee" },
	};

	for (const JointCheck& xJoint : axJoints)
	{
		// The child bone's own origin is the joint it rotates about.
		const Zenith_Maths::Vector3 xJointPos(
			pxSkel->GetBone(xJoint.uBoneB).m_xBindPoseModel[3]);
		bool bFoundBlend = false;
		for (uint32_t v = 0; v < pxMesh->GetNumVerts() && !bFoundBlend; v++)
		{
			const Zenith_Maths::Vector3& xPos = pxMesh->m_xPositions.Get(v);
			if (glm::length(xPos - xJointPos) > 0.14f)
			{
				continue;
			}
			const glm::uvec4& xI = pxMesh->m_xBoneIndices.Get(v);
			const glm::vec4& xW = pxMesh->m_xBoneWeights.Get(v);
			const bool bPair = (xI.x == xJoint.uBoneA && xI.y == xJoint.uBoneB)
			                || (xI.x == xJoint.uBoneB && xI.y == xJoint.uBoneA);
			if (bPair && xW.x > 0.25f && xW.x < 0.75f && xW.y > 0.25f && xW.y < 0.75f)
			{
				bFoundBlend = true;
			}
		}
		ZENITH_ASSERT_TRUE(bFoundBlend, "Expected blended skin weights at the %s", xJoint.szName);
	}

	delete pxMesh;
	delete pxSkel;
}

// ----- ProceduralTree leaf material regression -------------------------------
// The leaf albedo's alpha channel is a real leaf-shape mask, so the GENERATED leaf
// material MUST be MASKED (GenerateTreeMaterials). A regression to OPAQUE makes the
// leaves render as opaque quads (leaf texture on a black square) — BuildMaterialDraw-
// Constants only feeds a non-zero cutoff to the shader's discard for MASKED materials.
// Loads the committed/generated .zmtrl and guards against the SetBlendMode omission.
ZENITH_TEST(ProceduralTree, LeafMaterialIsAlphaMasked)
{
	// Load via the asset registry (the public path; LoadFromFile is private). The
	// registry resolves the engine: prefix and caches.
	Zenith_MaterialAsset* pxLeaves = Zenith_AssetRegistry::GetView<Zenith_MaterialAsset>(
		"engine:Meshes/ProceduralTree/Tree_Leaves.zmtrl");
	ZENITH_ASSERT_NOT_NULL(pxLeaves, "Tree_Leaves.zmtrl must load (run a tools boot to (re)generate it)");
	ZENITH_ASSERT_EQ(pxLeaves->GetBlendMode(), MATERIAL_BLEND_MASKED,
		"Leaf material must be MASKED so the alpha mask cuts the leaves out");
	ZENITH_ASSERT_EQ_FLOAT(pxLeaves->GetAlphaCutoff(), 0.45f, 0.0001f,
		"Leaf alpha cutoff must stay 0.45");
}

// ----- The proportion warp actually reaching the mesh -------------------------

ZENITH_TEST(StickFigureBody, WarpedLoftLandmarksLandOnTheRigsJointPlanes)
{
	// ★ THE TEST FOR "DID THE TABLE EDIT REACH THE GEOMETRY". Every other check in
	// this file would pass if the warp were skipped entirely -- the mesh would
	// still be a valid, well-weighted, correctly-bounded human, just one whose
	// knee is 8 cm from the knee bone it bends around. Re-measuring the FINISHED
	// mesh and comparing against the rig's own planes is the only thing that
	// notices, and it is the whole point of the change.
	const Zenith_HumanProportions& xP = Zenith_HumanProportionsRealistic();
	Zenith_HumanWarp xWarp;
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(xP, xWarp);
	ZENITH_ASSERT_TRUE(xWarp.IsValid(), "the loft must have produced a usable warp");

	// ★ MEASURED AS A T-POSE, because that is what it now is. The loft is authored
	// arms-down and warped arms-down -- every pass above the rotation seam still
	// works in that space -- but what SHIPS has its arms out, so re-measuring the
	// finished mesh has to ask the right question. In T_POSE the arm chain is
	// reported as distances OUT along the lateral axis rather than as heights.
	Zenith_SkinDeformView xView = Zenith_MakeSkinDeformView(*pxMesh);
	Zenith_HumanLandmarks xAfter;
	ZENITH_ASSERT_TRUE(Zenith_MeasureHumanLandmarks(xView, ZENITH_HUMAN_POSE_T_POSE, xAfter),
		"the warped loft must still measure");
	Zenith_LogHumanLandmarks("StickFigure loft (POST-warp)", xAfter);

	// One scan bin is height/128 = 0.020, so 2.5 bins is the resolution floor.
	const float fTol = 0.05f;
	ZENITH_ASSERT_EQ_FLOAT(xAfter.SoleY(), fZENITH_HUMAN_RIG_SOLE_Y, 1.0e-3f,
		"the sole is PINNED: colliders and spawn lifts are tuned against it");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.Height(), fZENITH_HUMAN_RIG_HEIGHT, 1.0e-3f,
		"and so is total height");

	ZENITH_ASSERT_TRUE(xAfter.m_abBodyFound[ZENITH_HUMAN_BODY_ANKLE], "the warped loft still has an ankle seam");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_afBodyY[ZENITH_HUMAN_BODY_ANKLE], xP.AnkleY(), fTol,
		"the loft's ankle seam now sits on the RIG's ankle plane");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_afBodyY[ZENITH_HUMAN_BODY_SHOULDER], xP.ShoulderY(), fTol,
		"...and its shoulder on the rig's shoulder plane");
	// The arm chain, as lateral reach: the shoulder's own half-width plus the
	// segment length that used to be measured as a drop in Y. Same table, same
	// lengths -- the rotation moved the arm without resizing it, and this is the
	// assertion that says so about the shipped geometry rather than about the rig.
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_afArmChain[ZENITH_HUMAN_ARM_ELBOW],
		xP.ShoulderHalfX() + (xP.ShoulderY() - xP.ElbowY()), fTol,
		"...its elbow at the rig's elbow reach");
	ZENITH_ASSERT_EQ_FLOAT(xAfter.m_afArmChain[ZENITH_HUMAN_ARM_WRIST],
		xP.ShoulderHalfX() + (xP.ShoulderY() - xP.WristY()), fTol,
		"...and its wrist at the rig's wrist reach");

	delete pxMesh;
}
