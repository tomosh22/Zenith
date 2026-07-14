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

	// Build the QUADRUPED reference clip for a clip-id into a fresh clip (used by
	// the metadata-golden test, whose stamp check is archetype-independent).
	void BuildQuadClip(ZM_ANIM_CLIP eClip, Flux_AnimationClip& xOut)
	{
		ZM_BuildCreatureClip(ZM_ARCHETYPE_QUADRUPED, eClip, xOut);
	}

	// Enumerate the archetypes that have a wired anim builder into aeOut (capacity
	// uCap), returning the count. Iterates [0, ZM_ARCHETYPE_COUNT) so coverage
	// AUTO-GROWS as later SCs wire more archetype builders (SC1 Quadruped; SC2
	// + Biped + Avian; SC3-SC5 the rest). The generic byte-identity / determinism /
	// distinctness / loop-wrap / faint-clamp / end-neutral gates below all iterate
	// this set, so every wired archetype is exercised without touching this harness.
	u_int WiredAnimArchetypes(ZM_ARCHETYPE* aeOut, u_int uCap)
	{
		u_int uCount = 0u;
		for (u_int a = 0; a < (u_int)ZM_ARCHETYPE_COUNT; ++a)
		{
			const ZM_ARCHETYPE eArch = (ZM_ARCHETYPE)a;
			if (ZM_GetArchetypeAnimBuilder(eArch) != nullptr)
			{
				if (uCount < uCap) { aeOut[uCount] = eArch; }
				++uCount;
			}
		}
		return uCount;
	}

	// Find up to two DISTINCT species whose body plan is eArch (both are anim-
	// buildable, since eArch is a wired archetype). Writes them into eOutA / eOutB
	// and returns how many were found (0, 1, or 2). Species order follows the
	// ZM_SPECIES_ID enum, so the pick is deterministic.
	u_int FindTwoSpeciesOfArchetype(ZM_ARCHETYPE eArch, ZM_SPECIES_ID& eOutA, ZM_SPECIES_ID& eOutB)
	{
		u_int uFound = 0u;
		for (u_int id = 0; id < (u_int)ZM_SPECIES_COUNT; ++id)
		{
			const ZM_SPECIES_ID eId = (ZM_SPECIES_ID)id;
			if (ZM_GetSpeciesData(eId).m_eArchetype != eArch) { continue; }
			if (!HasAnimBuilder(eId)) { continue; }   // defensive; always true for a wired archetype
			if (uFound == 0u) { eOutA = eId; uFound = 1u; }
			else              { eOutB = eId; uFound = 2u; break; }
		}
		return uFound;
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
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	// For EVERY wired archetype: two DISTINCT species must produce byte-identical
	// clips (a clip is pure f(archetype, clip)). An archetype with only a single
	// species is skipped, but at least one archetype must have been compared.
	u_int uTestedArch = 0u;
	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];

		ZM_SPECIES_ID eA = (ZM_SPECIES_ID)0;   // placeholders; only read when uFound == 2
		ZM_SPECIES_ID eB = (ZM_SPECIES_ID)0;
		if (FindTwoSpeciesOfArchetype(eArch, eA, eB) < 2u) { continue; }
		++uTestedArch;

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
				"clip %u differs byte-wise across two species of archetype %u (clip is not pure f(archetype,clip))", c, (u_int)eArch);
			ZENITH_ASSERT_EQ(ZM_CreatureClipContentHash(xClipA), ZM_CreatureClipContentHash(xClipB),
				"clip %u content hash differs across two species of archetype %u", c, (u_int)eArch);
		}
	}
	ZENITH_ASSERT_GT(uTestedArch, 0u,
		"no wired archetype had two distinct species to compare (harness would be vacuous)");
}

// ############################################################################
// (5) Same-inputs determinism: the SAME (archetype, clip) built twice is
//     byte-identical (no RNG, no clock, no address-dependent data).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_SameInputsDeterminism)
{
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];
		for (u_int c = 0; c < (u_int)ZM_ANIM_CLIP_COUNT; ++c)
		{
			const ZM_ANIM_CLIP eClip = (ZM_ANIM_CLIP)c;
			Flux_AnimationClip xClip1;
			Flux_AnimationClip xClip2;
			ZM_BuildCreatureClip(eArch, eClip, xClip1);
			ZM_BuildCreatureClip(eArch, eClip, xClip2);

			ZENITH_ASSERT_TRUE(ZM_CreatureClipBytesEqual(xClip1, xClip2),
				"archetype %u clip %u not byte-identical on a repeat build (non-determinism)", (u_int)eArch, c);
			ZENITH_ASSERT_EQ(ZM_CreatureClipContentHash(xClip1), ZM_CreatureClipContentHash(xClip2),
				"archetype %u clip %u content hash diverged on a repeat build", (u_int)eArch, c);
		}
	}
}

// ############################################################################
// (6) Meaningfully-different clips are actually DISTINCT (guards two clips from
//     accidentally sharing the same motion).
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_ClipsDistinct)
{
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];

		Flux_AnimationClip xIdle;
		Flux_AnimationClip xWalk;
		Flux_AnimationClip xAttack;
		Flux_AnimationClip xSpecial;
		Flux_AnimationClip xHit;
		Flux_AnimationClip xFaint;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_IDLE,    xIdle);
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_WALK,    xWalk);
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_ATTACK,  xAttack);
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_SPECIAL, xSpecial);
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_HIT,     xHit);
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT,   xFaint);

		const u_int uIdle    = ZM_CreatureClipContentHash(xIdle);
		const u_int uWalk    = ZM_CreatureClipContentHash(xWalk);
		const u_int uAttack  = ZM_CreatureClipContentHash(xAttack);
		const u_int uSpecial = ZM_CreatureClipContentHash(xSpecial);
		const u_int uHit     = ZM_CreatureClipContentHash(xHit);
		const u_int uFaint   = ZM_CreatureClipContentHash(xFaint);

		ZENITH_ASSERT_NE(uIdle,   uWalk,    "archetype %u: Idle and Walk clips share a content hash (motion collision)", (u_int)eArch);
		ZENITH_ASSERT_NE(uAttack, uSpecial, "archetype %u: Attack and Special clips share a content hash", (u_int)eArch);
		ZENITH_ASSERT_NE(uAttack, uHit,     "archetype %u: Attack and Hit clips share a content hash", (u_int)eArch);
		ZENITH_ASSERT_NE(uHit,    uFaint,   "archetype %u: Hit and Faint clips share a content hash", (u_int)eArch);
	}
}

// ############################################################################
// (7) Looping clips (Idle / Walk) wrap cleanly -- a key at t=0 and at
//     t=durationTicks per channel with matching orientation (no loop pop). The
//     one-shot action clips do NOT loop.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_LoopingClipsWrapCleanly)
{
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];

		const ZM_ANIM_CLIP aeLooping[2] = { ZM_ANIM_CLIP_IDLE, ZM_ANIM_CLIP_WALK };
		for (u_int i = 0; i < 2u; ++i)
		{
			const ZM_ANIM_CLIP eClip = aeLooping[i];
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClip(eArch, eClip, xClip);

			ZENITH_ASSERT_TRUE(xClip.IsLooping(), "archetype %u clip %u must be looping", (u_int)eArch, (u_int)eClip);
			const float fDurTicks = xClip.GetDurationInTicks();

			const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
			ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "archetype %u looping clip %u has no channels", (u_int)eArch, (u_int)eClip);

			Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
			for (; !xIt.Done(); xIt.Next())
			{
				const Flux_BoneChannel& xChannel = xIt.GetValue();
				const char* szBone = xChannel.GetBoneName().c_str();
				const Zenith_Vector<std::pair<Zenith_Maths::Quat, float>>& xKeys = xChannel.GetRotationKeyframes();
				ZENITH_ASSERT_GE(xKeys.GetSize(), 2u,
					"archetype %u looping clip %u channel '%s' needs >= 2 keys to close", (u_int)eArch, (u_int)eClip, szBone);

				// SortKeyframes puts the earliest tick first and the latest last.
				const float fFirstTick = xKeys.GetFront().second;
				const float fLastTick  = xKeys.GetBack().second;
				ZENITH_ASSERT_LE(fabsf(fFirstTick - 0.0f), fTICK_TOL,
					"archetype %u looping clip %u channel '%s' has no key at t=0", (u_int)eArch, (u_int)eClip, szBone);
				ZENITH_ASSERT_LE(fabsf(fLastTick - fDurTicks), fTICK_TOL,
					"archetype %u looping clip %u channel '%s' has no key at t=durationTicks", (u_int)eArch, (u_int)eClip, szBone);

				// Loop closes: rot(t=0) ~= rot(t=durationTicks) per channel (|dot| ~ 1).
				const Zenith_Maths::Quat& xR0 = xKeys.GetFront().first;
				const Zenith_Maths::Quat& xRN = xKeys.GetBack().first;
				ZENITH_ASSERT_GE(QuatAbsDot(xR0, xRN), fDOT_CLOSE,
					"archetype %u looping clip %u channel '%s' does not close (t=0 rot != t=end rot)", (u_int)eArch, (u_int)eClip, szBone);
			}
		}

		// The one-shot action clips must NOT loop (their clip-end clamp holds neutral/KO).
		const ZM_ANIM_CLIP aeOneShot[4] =
			{ ZM_ANIM_CLIP_ATTACK, ZM_ANIM_CLIP_SPECIAL, ZM_ANIM_CLIP_HIT, ZM_ANIM_CLIP_FAINT };
		for (u_int i = 0; i < 4u; ++i)
		{
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClip(eArch, aeOneShot[i], xClip);
			ZENITH_ASSERT_FALSE(xClip.IsLooping(), "archetype %u one-shot clip %u must not loop", (u_int)eArch, (u_int)aeOneShot[i]);
		}
	}
}

// ############################################################################
// (8) Faint settles and CLAMPS: the KO pose holds past the end (clamp, not
//     extrapolate) AND the final pose genuinely differs from the t=0 pose.
// ############################################################################

ZENITH_TEST(ZM_Gen, CreatureAnimGen_FaintSettlesAndClamps)
{
	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];

		Flux_AnimationClip xClip;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT, xClip);
		ZENITH_ASSERT_FALSE(xClip.IsLooping(), "archetype %u Faint must be a one-shot (non-looping)", (u_int)eArch);

		const float fDurTicks = xClip.GetDurationInTicks();
		const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
		ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "archetype %u Faint has no channels", (u_int)eArch);

		// Archetype-agnostic (no hardcoded bone name): EVERY channel must clamp past
		// the end (KO pose holds, not extrapolate), and AT LEAST ONE channel's final
		// pose must genuinely differ from t=0 (a real collapse, not a no-op).
		u_int uExamined  = 0u;
		u_int uCollapsed = 0u;
		Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
		for (; !xIt.Done(); xIt.Next())
		{
			const Flux_BoneChannel& xChannel = xIt.GetValue();
			const char* szBone = xChannel.GetBoneName().c_str();

			const Zenith_Maths::Quat xAt0   = xChannel.SampleRotation(0.0f);
			const Zenith_Maths::Quat xAtMid = xChannel.SampleRotation(fDurTicks * 0.5f);
			const Zenith_Maths::Quat xAtEnd = xChannel.SampleRotation(fDurTicks);
			const Zenith_Maths::Quat xPast  = xChannel.SampleRotation(fDurTicks * 2.0f);

			ZENITH_ASSERT_TRUE(QuatFinite(xAt0) && QuatFinite(xAtMid) && QuatFinite(xAtEnd) && QuatFinite(xPast),
				"archetype %u Faint channel '%s' samples must all be finite", (u_int)eArch, szBone);

			// Clamp-not-extrapolate: sampling past the end holds the settled KO pose.
			ZENITH_ASSERT_GE(QuatAbsDot(xPast, xAtEnd), fDOT_CLOSE,
				"archetype %u Faint channel '%s' does not clamp past the end (KO pose should hold, not extrapolate)", (u_int)eArch, szBone);

			// Count channels whose final pose genuinely differs from bind (t=0).
			if (QuatAbsDot(xAtEnd, xAt0) <= fDOT_DIFFER) { ++uCollapsed; }
			++uExamined;
		}
		ZENITH_ASSERT_GT(uExamined, 0u, "archetype %u Faint examined no channels", (u_int)eArch);
		ZENITH_ASSERT_GT(uCollapsed, 0u,
			"archetype %u Faint final pose ~= t=0 pose on every channel (the creature did not visibly collapse)", (u_int)eArch);
	}
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

	ZM_ARCHETYPE aeArch[ZM_ARCHETYPE_COUNT];
	const u_int uArch = WiredAnimArchetypes(aeArch, (u_int)ZM_ARCHETYPE_COUNT);
	ZENITH_ASSERT_GT(uArch, 0u, "no wired anim archetypes (harness would be vacuous)");

	for (u_int ia = 0; ia < uArch; ++ia)
	{
		const ZM_ARCHETYPE eArch = aeArch[ia];

		// Attack / Special / Hit: EVERY channel returns to ~identity at the clip end.
		const ZM_ANIM_CLIP aeAction[3] =
			{ ZM_ANIM_CLIP_ATTACK, ZM_ANIM_CLIP_SPECIAL, ZM_ANIM_CLIP_HIT };
		for (u_int i = 0; i < 3u; ++i)
		{
			const ZM_ANIM_CLIP eClip = aeAction[i];
			Flux_AnimationClip xClip;
			ZM_BuildCreatureClip(eArch, eClip, xClip);

			const float fDurTicks = xClip.GetDurationInTicks();
			const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xClip.GetBoneChannels();
			ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "archetype %u action clip %u has no channels", (u_int)eArch, (u_int)eClip);

			u_int uExamined = 0u;
			Zenith_HashMap<std::string, Flux_BoneChannel>::Iterator xIt(xChannels);
			for (; !xIt.Done(); xIt.Next())
			{
				const Flux_BoneChannel& xChannel = xIt.GetValue();
				const char* szBone = xChannel.GetBoneName().c_str();
				const Zenith_Maths::Quat xEnd = xChannel.SampleRotation(fDurTicks);
				ZENITH_ASSERT_TRUE(QuatFinite(xEnd),
					"archetype %u action clip %u channel '%s' end sample not finite", (u_int)eArch, (u_int)eClip, szBone);
				ZENITH_ASSERT_GE(QuatAbsDot(xEnd, xIdentity), fDOT_CLOSE,
					"archetype %u action clip %u channel '%s' does NOT end at ~identity (clip-end clamp would not hold neutral)",
					(u_int)eArch, (u_int)eClip, szBone);
				++uExamined;
			}
			ZENITH_ASSERT_GT(uExamined, 0u,
				"archetype %u action clip %u examined no channels (end-neutral gate would be vacuous)", (u_int)eArch, (u_int)eClip);
		}

		// CONTRAST: Faint holds a collapsed pose -- at least one channel is NOT
		// ~identity at the end. This proves the end-neutral property above is a real
		// property of the action clips, not a trivial truth of every one-shot clip.
		{
			Flux_AnimationClip xFaint;
			ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT, xFaint);

			const float fDurTicks = xFaint.GetDurationInTicks();
			const Zenith_HashMap<std::string, Flux_BoneChannel>& xChannels = xFaint.GetBoneChannels();
			ZENITH_ASSERT_GT(xChannels.GetSize(), 0u, "archetype %u Faint has no channels", (u_int)eArch);

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
			ZENITH_ASSERT_GT(uExamined, 0u, "archetype %u Faint examined no channels (contrast would be vacuous)", (u_int)eArch);
			ZENITH_ASSERT_GT(uCollapsed, 0u,
				"archetype %u Faint ends at ~identity on every channel (end-neutral would be trivially true, not specific to action clips)", (u_int)eArch);
		}
	}
}
