#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimGen -- S4 SC1 unit gate for ZM_CreatureAnimGen (suite
// ZM_Gen).
//
// This is the GENERIC, contract-driven creature-ANIMATION harness: it authors
// against the frozen seam (Games/Zenithmon/Source/Gen/ZM_CreatureAnimGen.h),
// NEVER against a specific archetype builder .cpp. It loops every ZM_SPECIES_ID
// whose ZM_GetArchetypeAnimBuilder is non-null (SC1: QUADRUPED only; the wired
// archetype set GROWS per SC, so un-wired archetypes are SKIPPED and coverage
// auto-grows as later SCs land their anim builders), and proves the load-bearing
// S4 clip invariants:
//   (1) every channel binds to a real skeleton bone (no dead channels) + every
//       authored quat is finite and ~unit-length      -- ChannelsMatchSkeleton
//   (2) the whole ZM_ValidateCreatureClip contract holds -- ValidationPasses
//   (3) golden clip metadata (names / durations / looping / ticks-per-second)
//   (4) clips are PURE f(archetype, clip): byte-identical across two species of
//       one archetype                                  -- SameArchetypeByteIdentical
//   (5) same-inputs determinism (repeat build byte-identical)
//   (6) meaningfully-different clips are actually distinct (no motion collision)
//   (7) looping clips (Idle / Walk) wrap cleanly; one-shots do not loop
//   (8) Faint clamps past the end (KO pose holds, no extrapolation)
//
// PURE / HEADLESS: no disk, no GPU, no ZENITH_TOOLS reach (the .zanim bake is
// compiled out). Clips draw from NO RNG; a clip is a closed-form function of
// (archetype, clip-id) only. These run at boot before the scene loads.
//
// The seam already provides the byte-equality / content-hash / validation
// helpers -- this harness USES them; it never re-derives curve maths.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"     // ZM_BuildCreatureMesh, ZM_ResolveCreatureRecipe, ZM_GetArchetypeBuilder
#include "Zenithmon/Source/Gen/ZM_GenCommon.h"        // ZM_GenMesh, ZM_GenMeshFindBone
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_HashMap.h"
#include "Collections/Zenith_Vector.h"

#include <cmath>     // std::isfinite, fabsf
#include <string>
#include <utility>   // std::pair

namespace
{
	// Quaternion closeness epsilons.
	constexpr float fUNIT_TOL   = 1.0e-3f;   // |len^2 - 1| tolerance for "unit-length"
	constexpr float fDOT_CLOSE  = 0.999f;    // |dot| >= this => same orientation (double-cover aware)
	constexpr float fDOT_DIFFER = 0.99f;     // |dot| <= this => clearly different orientation
	constexpr float fTICK_TOL   = 0.05f;     // keyframe-tick tolerance (ticks are ~integers)

	// Golden per-clip metadata (the version-bump contract; mirrors the header's
	// LOCKED table). Any change here is a deliberate .zanim re-bake.
	struct ZM_ClipGolden
	{
		ZM_ANIM_CLIP m_eClip;
		const char*  m_szName;
		float        m_fDurationSeconds;
		bool         m_bLooping;
	};
	const ZM_ClipGolden g_axGolden[ZM_ANIM_CLIP_COUNT] =
	{
		{ ZM_ANIM_CLIP_IDLE,    "Idle",    2.0f, true  },
		{ ZM_ANIM_CLIP_WALK,    "Walk",    1.0f, true  },
		{ ZM_ANIM_CLIP_ATTACK,  "Attack",  0.7f, false },
		{ ZM_ANIM_CLIP_SPECIAL, "Special", 0.9f, false },
		{ ZM_ANIM_CLIP_HIT,     "Hit",     0.4f, false },
		{ ZM_ANIM_CLIP_FAINT,   "Faint",   1.2f, false },
	};

	// True when the species' body plan has a wired ANIM builder (SC1: QUADRUPED
	// only). The generic harness SKIPS every species whose anim builder is nullptr,
	// so coverage GROWS automatically as later archetype anim builders land.
	bool HasAnimBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeAnimBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}

	// True when the species' body plan has a wired MESH builder (needed to build
	// the per-species skeleton the clip channels bind against).
	bool HasMeshBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}

	bool QuatFinite(const Zenith_Maths::Quat& xQ)
	{
		return std::isfinite(xQ.w) && std::isfinite(xQ.x) && std::isfinite(xQ.y) && std::isfinite(xQ.z);
	}

	bool QuatUnit(const Zenith_Maths::Quat& xQ)
	{
		const float fLen2 = xQ.w * xQ.w + xQ.x * xQ.x + xQ.y * xQ.y + xQ.z * xQ.z;
		return fabsf(fLen2 - 1.0f) <= fUNIT_TOL;
	}

	// Double-cover-aware orientation closeness: |dot| in [0,1], 1 == identical.
	float QuatAbsDot(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB)
	{
		return fabsf(xA.w * xB.w + xA.x * xB.x + xA.y * xB.y + xA.z * xB.z);
	}

	// Build the SC1 reference (QUADRUPED) clip for a clip-id into a fresh clip.
	void BuildQuadClip(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
	{
		ZM_BuildCreatureClip(ZM_ARCHETYPE_QUADRUPED, eClip, xOut);
	}
}

// ############################################################################
// (1) The core playability proof: every channel binds to the skeleton, every
//     authored quat is finite + ~unit-length, no dead channels / skeleton drift.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_ChannelsMatchSkeleton)
{
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!HasAnimBuilder(eId) || !HasMeshBuilder(eId)) { continue; }   // skip un-authored archetypes
		++uTested;

		// Build the species' skeleton mesh ONCE, reuse across all 6 clips.
		ZM_GenMesh xMesh;
		ZM_BuildCreatureMesh(ZM_ResolveCreatureRecipe(eId), xMesh);
		const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

		for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
		{
			const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClip(eArch, eClip, xClip);

			const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
			ZENITH_ASSERT_GT(xChannels.GetSize(), 0u,
				"species %u clip %u has no bone channels", id, c);

			Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
			for (; !xIt.Done(); xIt.Next())
			{
				const Flux_BoneChannel& xChannel = xIt.GetValue();
				const std::string& strBone = xChannel.GetBoneName();

				// Rotation-only clips: every channel must carry rotation keyframes.
				ZENITH_ASSERT_TRUE(xChannel.HasRotationKeyframes(),
					"species %u clip %u channel '%s' has no rotation keyframes",
					id, c, strBone.c_str());

				// Every authored quat finite AND ~unit-length.
				const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys =
					xChannel.GetRotationKeyframes();
				for (u_int k = 0; k < xKeys.GetSize(); ++k)
				{
					const Zenith_Maths::Quat& xRot = xKeys.Get(k).first;
					ZENITH_ASSERT_TRUE(QuatFinite(xRot),
						"species %u clip %u channel '%s' key %u rotation not finite",
						id, c, strBone.c_str(), k);
					ZENITH_ASSERT_TRUE(QuatUnit(xRot),
						"species %u clip %u channel '%s' key %u rotation not unit-length",
						id, c, strBone.c_str(), k);
				}

				// No dead channels: the channel's bone name must resolve in the skeleton.
				ZENITH_ASSERT_GE(ZM_GenMeshFindBone(xMesh, strBone.c_str()), 0,
					"species %u clip %u channel '%s' binds to no skeleton bone (drift)",
					id, c, strBone.c_str());
			}
		}
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no anim-buildable species exercised the channel/skeleton gate");
}

// ############################################################################
// (2) The whole ZM_ValidateCreateClip contract holds against a real skeleton.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_ValidationPasses)
{
	u_int uTested = 0u;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (!HasAnimBuilder(eId) || !HasMeshBuilder(eId)) { continue; }
		++uTested;

		ZM_GenMesh xMesh;
		ZM_BuildCreatureMesh(ZM_ResolveCreatureRecipe(eId), xMesh);
		const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

		for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
		{
			const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClip(eArch, eClip, xClip);

			const ZM_CreatureClipValidation xVal =
				ZM_ValidateCreatureClip(xClip, xMesh, ZM_CreatureClipLooping(eClip));
			ZENITH_ASSERT_TRUE(xVal.m_bAllValid,
				"species %u clip %u failed ZM_ValidateCreatureClip (first bad bone '%s')",
				id, c, xVal.m_szFirstBadBone);
		}
	}
	ZENITH_ASSERT_GT(uTested, 0u, "no anim-buildable species exercised the clip-validation gate");
}

// ############################################################################
// (3) Golden clip metadata -- literal pins (any change is a version-bump).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_ClipMetadataGolden)
{
	// The ticks-per-second constant is a golden literal.
	ZENITH_ASSERT_EQ(uZM_CREATURE_ANIM_TICKS_PER_SECOND, 24u,
		"creature-anim ticks-per-second golden-pinned to 24");

	for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
	{
		const ZM_ClipGolden& xG = g_axGolden[c];
		const ZM_ANIM_CLIP eClip = xG.m_eClip;

		// Golden accessors (literal-pinned; exact float compare -- these are exact literals).
		ZENITH_ASSERT_EQ(ZM_CreatureClipDurationSeconds(eClip), xG.m_fDurationSeconds,
			"clip %u duration drifted from its golden literal", c);
		ZENITH_ASSERT_EQ(ZM_CreatureClipLooping(eClip), xG.m_bLooping,
			"clip %u looping flag drifted from its golden literal", c);
		ZENITH_ASSERT_STREQ(ZM_CreatureClipName(eClip), xG.m_szName,
			"clip %u name drifted from its golden literal", c);

		// Build one clip per id and confirm the metadata is stamped onto the Flux clip.
		Flux_AnimationClip xClip;
		BuildQuadClip(eClip, xClip);
		ZENITH_ASSERT_EQ(xClip.GetTicksPerSecond(), 24u,
			"clip %u built-clip ticks-per-second != 24", c);
		ZENITH_ASSERT_STREQ(xClip.GetName().c_str(), xG.m_szName,
			"clip %u built-clip name != golden", c);
		ZENITH_ASSERT_EQ(xClip.GetDuration(), xG.m_fDurationSeconds,
			"clip %u built-clip duration != golden", c);
		ZENITH_ASSERT_EQ(xClip.IsLooping(), xG.m_bLooping,
			"clip %u built-clip looping != golden", c);
	}
}

// ############################################################################
// (4) THE leverage proof: a clip is pure f(archetype, clip) -- byte-identical
//     across two DISTINCT species of the same archetype (author once, transfer
//     to every species of the body plan).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_SameArchetypeByteIdentical)
{
	// Find the first TWO distinct QUADRUPED species that are anim-buildable.
	ZM_SPECIES_ID eA = ZM_SPECIES_FERNFAWN;   // placeholder until found
	ZM_SPECIES_ID eB = ZM_SPECIES_FERNFAWN;
	bool bFoundA = false;
	bool bFoundB = false;
	for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
	{
		const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
		if (ZM_GetSpeciesData(eId).m_eArchetype != ZM_ARCHETYPE_QUADRUPED) { continue; }
		if (!HasAnimBuilder(eId)) { continue; }
		if (!bFoundA) { eA = eId; bFoundA = true; }
		else          { eB = eId; bFoundB = true; break; }
	}
	ZENITH_ASSERT_TRUE(bFoundA, "no quadruped anim-buildable species found (harness would be vacuous)");
	ZENITH_ASSERT_TRUE(bFoundB, "no SECOND distinct quadruped anim-buildable species found");

	const ZM_ARCHETYPE eArchA = ZM_GetSpeciesData(eA).m_eArchetype;
	const ZM_ARCHETYPE eArchB = ZM_GetSpeciesData(eB).m_eArchetype;

	for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
	{
		const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
		Flux_AnimationClip xClipA;
		Flux_AnimationClip xClipB;
		ZM_BuildCreatureClip(eArchA, eClip, xClipA);
		ZM_BuildCreatureClip(eArchB, eClip, xClipB);

		ZENITH_ASSERT_TRUE(ZM_CreatureClipBytesEqual(xClipA, xClipB),
			"clip %u differs byte-wise across two quadruped species (clip is not pure f(archetype,clip))", c);
		ZENITH_ASSERT_EQ(ZM_CreatureClipContentHash(xClipA), ZM_CreatureClipContentHash(xClipB),
			"clip %u content hash differs across two quadruped species", c);
	}
}

// ############################################################################
// (5) Same-inputs determinism: the SAME (archetype, clip) built twice is
//     byte-identical (no RNG, no clock, no address-dependent data).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_SameInputsDeterminism)
{
	for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
	{
		const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
		Flux_AnimationClip xClip1;
		Flux_AnimationClip xClip2;
		BuildQuadClip(eClip, xClip1);
		BuildQuadClip(eClip, xClip2);

		ZENITH_ASSERT_TRUE(ZM_CreatureClipBytesEqual(xClip1, xClip2),
			"quadruped clip %u not byte-identical on a repeat build (non-determinism)", c);
		ZENITH_ASSERT_EQ(ZM_CreatureClipContentHash(xClip1), ZM_CreatureClipContentHash(xClip2),
			"quadruped clip %u content hash diverged on a repeat build", c);
	}
}

// ############################################################################
// (6) Meaningfully-different clips are actually DISTINCT (guards two clips from
//     accidentally sharing the same motion).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_ClipsDistinct)
{
	Flux_AnimationClip xIdle;
	Flux_AnimationClip xWalk;
	Flux_AnimationClip xAttack;
	Flux_AnimationClip xSpecial;
	Flux_AnimationClip xHit;
	Flux_AnimationClip xFaint;
	BuildQuadClip(ZM_ANIM_CLIP_IDLE,    xIdle);
	BuildQuadClip(ZM_ANIM_CLIP_WALK,    xWalk);
	BuildQuadClip(ZM_ANIM_CLIP_ATTACK,  xAttack);
	BuildQuadClip(ZM_ANIM_CLIP_SPECIAL, xSpecial);
	BuildQuadClip(ZM_ANIM_CLIP_HIT,     xHit);
	BuildQuadClip(ZM_ANIM_CLIP_FAINT,   xFaint);

	const u_int uIdle    = ZM_CreatureClipContentHash(xIdle);
	const u_int uWalk    = ZM_CreatureClipContentHash(xWalk);
	const u_int uAttack  = ZM_CreatureClipContentHash(xAttack);
	const u_int uSpecial = ZM_CreatureClipContentHash(xSpecial);
	const u_int uHit     = ZM_CreatureClipContentHash(xHit);
	const u_int uFaint   = ZM_CreatureClipContentHash(xFaint);

	ZENITH_ASSERT_NE(uIdle,   uWalk,    "Idle and Walk clips share a content hash (motion collision)");
	ZENITH_ASSERT_NE(uAttack, uSpecial, "Attack and Special clips share a content hash");
	ZENITH_ASSERT_NE(uAttack, uHit,     "Attack and Hit clips share a content hash");
	ZENITH_ASSERT_NE(uHit,    uFaint,   "Hit and Faint clips share a content hash");
}

// ############################################################################
// (7) Looping clips (Idle / Walk) wrap cleanly -- a key at t=0 and at
//     t=durationTicks per channel with matching orientation (no loop pop). The
//     one-shot action clips do NOT loop.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_LoopingClipsWrapCleanly)
{
	const ZM_ANIM_CLIP aeLooping[2] = { ZM_ANIM_CLIP_IDLE, ZM_ANIM_CLIP_WALK };
	for (u_int i = 0; i < 2u; ++i)
	{
		const ZM_ANIM_CLIP eClip = aeLooping[i];
		Flux_AnimationClip xClip;
		BuildQuadClip(eClip, xClip);

		ZENITH_ASSERT_TRUE(xClip.IsLooping(), "clip %u must be looping", (u_int)eClip);
		const float fDurTicks = xClip.GetDurationInTicks();

		const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
		ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "looping clip %u has no channels", (u_int)eClip);

		Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
		for (; !xIt.Done(); xIt.Next())
		{
			const Flux_BoneChannel& xChannel = xIt.GetValue();
			const char* szBone = xChannel.GetBoneName().c_str();
			const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys = xChannel.GetRotationKeyframes();
			ZENITH_ASSERT_GE(xKeys.GetSize(), 2u,
				"looping clip %u channel '%s' needs >= 2 keys to close", (u_int)eClip, szBone);

			// SortKeyframes puts the earliest tick first and the latest last.
			const float fFirstTick = xKeys.GetFront().second;
			const float fLastTick  = xKeys.GetBack().second;
			ZENITH_ASSERT_LE(fabsf(fFirstTick - 0.0f), fTICK_TOL,
				"looping clip %u channel '%s' has no key at t=0", (u_int)eClip, szBone);
			ZENITH_ASSERT_LE(fabsf(fLastTick - fDurTicks), fTICK_TOL,
				"looping clip %u channel '%s' has no key at t=durationTicks", (u_int)eClip, szBone);

			// Loop closes: rot(t=0) ~= rot(t=durationTicks) per channel (|dot| ~ 1).
			const Zenith_Maths::Quat& xR0 = xKeys.GetFront().first;
			const Zenith_Maths::Quat& xRN = xKeys.GetBack().first;
			ZENITH_ASSERT_GE(QuatAbsDot(xR0, xRN), fDOT_CLOSE,
				"looping clip %u channel '%s' does not close (t=0 rot != t=end rot)", (u_int)eClip, szBone);
		}
	}

	// The one-shot action clips must NOT loop (their clip-end clamp holds neutral/KO).
	const ZM_ANIM_CLIP aeOneShot[4] =
		{ ZM_ANIM_CLIP_ATTACK, ZM_ANIM_CLIP_SPECIAL, ZM_ANIM_CLIP_HIT, ZM_ANIM_CLIP_FAINT };
	for (u_int i = 0; i < 4u; ++i)
	{
		Flux_AnimationClip xClip;
		BuildQuadClip(aeOneShot[i], xClip);
		ZENITH_ASSERT_FALSE(xClip.IsLooping(), "one-shot clip %u must not loop", (u_int)aeOneShot[i]);
	}
}

// ############################################################################
// (8) Faint settles and CLAMPS: the KO pose holds past the end (clamp, not
//     extrapolate) AND the final pose genuinely differs from the t=0 pose.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_FaintSettlesAndClamps)
{
	Flux_AnimationClip xClip;
	BuildQuadClip(ZM_ANIM_CLIP_FAINT, xClip);
	ZENITH_ASSERT_FALSE(xClip.IsLooping(), "Faint must be a one-shot (non-looping)");

	// Spine01 is a confirmed FAINT-animated bone (the spine folds forward-down).
	ZENITH_ASSERT_TRUE(xClip.HasBoneChannel("Spine01"), "Faint must animate Spine01");
	const Flux_BoneChannel* pxChannel = xClip.GetBoneChannel("Spine01");
	ZENITH_ASSERT_TRUE(pxChannel != nullptr, "Faint Spine01 channel must resolve");
	if (pxChannel == nullptr) { return; }

	const float fDurTicks = xClip.GetDurationInTicks();
	const Zenith_Maths::Quat xAt0   = pxChannel->SampleRotation(0.0f);
	const Zenith_Maths::Quat xAtMid = pxChannel->SampleRotation(fDurTicks * 0.5f);
	const Zenith_Maths::Quat xAtEnd = pxChannel->SampleRotation(fDurTicks);
	const Zenith_Maths::Quat xPast  = pxChannel->SampleRotation(fDurTicks * 2.0f);

	ZENITH_ASSERT_TRUE(QuatFinite(xAt0) && QuatFinite(xAtMid) && QuatFinite(xAtEnd) && QuatFinite(xPast),
		"Faint Spine01 samples must all be finite");

	// Clamp-not-extrapolate: sampling past the end holds the settled KO pose.
	ZENITH_ASSERT_GE(QuatAbsDot(xPast, xAtEnd), fDOT_CLOSE,
		"Faint does not clamp past the end (KO pose should hold, not extrapolate)");

	// The KO pose is a genuine collapse: the final pose differs from bind (t=0).
	ZENITH_ASSERT_LE(QuatAbsDot(xAtEnd, xAt0), fDOT_DIFFER,
		"Faint final pose ~= t=0 pose (the creature did not visibly collapse)");
}

// ############################################################################
// (9) One-shot ACTION clips (Attack / Special / Hit) resolve to ~neutral at their
//     END so the clip-end clamp holds a clean bind pose. Faint deliberately does
//     NOT (it holds a collapsed KO pose), which the CONTRAST below proves -- so the
//     end-neutral property is specific to the action clips, not trivially true of
//     every one-shot.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_OneShotClipsEndNeutral)
{
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();

	// Attack / Special / Hit: EVERY channel returns to ~identity at the clip end.
	const ZM_ANIM_CLIP aeAction[3] =
		{ ZM_ANIM_CLIP_ATTACK, ZM_ANIM_CLIP_SPECIAL, ZM_ANIM_CLIP_HIT };
	for (u_int i = 0; i < 3u; ++i)
	{
		const ZM_ANIM_CLIP eClip = aeAction[i];
		Flux_AnimationClip xClip;
		BuildQuadClip(eClip, xClip);

		const float fDurTicks = xClip.GetDurationInTicks();
		const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
		ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "action clip %u has no channels", (u_int)eClip);

		u_int uExamined = 0u;
		Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
		for (; !xIt.Done(); xIt.Next())
		{
			const Flux_BoneChannel& xChannel = xIt.GetValue();
			const char* szBone = xChannel.GetBoneName().c_str();
			const Zenith_Maths::Quat xEnd = xChannel.SampleRotation(fDurTicks);
			ZENITH_ASSERT_TRUE(QuatFinite(xEnd),
				"action clip %u channel '%s' end sample not finite", (u_int)eClip, szBone);
			ZENITH_ASSERT_GE(QuatAbsDot(xEnd, xIdentity), fDOT_CLOSE,
				"action clip %u channel '%s' does NOT end at ~identity (clip-end clamp would not hold neutral)",
				(u_int)eClip, szBone);
			++uExamined;
		}
		ZENITH_ASSERT_GT(uExamined, 0u,
			"action clip %u examined no channels (end-neutral gate would be vacuous)", (u_int)eClip);
	}

	// CONTRAST: Faint holds a collapsed pose -- at least one channel is NOT ~identity
	// at the end. This proves the end-neutral property above is a real property of the
	// action clips, not a trivial truth of every one-shot clip.
	{
		Flux_AnimationClip xFaint;
		BuildQuadClip(ZM_ANIM_CLIP_FAINT, xFaint);

		const float fDurTicks = xFaint.GetDurationInTicks();
		const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xFaint.GetBoneChannels();
		ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "Faint has no channels");

		u_int uExamined  = 0u;
		u_int uCollapsed = 0u;
		Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
		for (; !xIt.Done(); xIt.Next())
		{
			const Flux_BoneChannel& xChannel = xIt.GetValue();
			const Zenith_Maths::Quat xEnd = xChannel.SampleRotation(fDurTicks);
			if (QuatAbsDot(xEnd, xIdentity) <= fDOT_DIFFER) { ++uCollapsed; }
			++uExamined;
		}
		ZENITH_ASSERT_GT(uExamined, 0u, "Faint examined no channels (contrast would be vacuous)");
		ZENITH_ASSERT_GT(uCollapsed, 0u,
			"Faint ends at ~identity on every channel (end-neutral would be trivially true, not specific to action clips)");
	}
}
