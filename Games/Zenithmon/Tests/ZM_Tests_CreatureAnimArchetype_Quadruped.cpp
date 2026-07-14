#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimArchetype_Quadruped -- S4 SC1 structural lock on the
// QUADRUPED anim builder (suite ZM_Gen).
//
// Where the GENERIC ChannelsMatchSkeleton harness proves a clip binds to SOME
// real skeleton bone, THIS test pins the archetype SIGNATURE: the EXACT quadruped
// bone NAMES (extracted verbatim from ZM_CreatureAnimArchetype_Quadruped.cpp)
// that each clip actually keys. So a builder edit that drops a leg from the walk
// cycle, stops swaying the spine, or forgets to buckle the legs on faint is
// caught here as a true structural regression -- not a tautology.
//
// The assertions are tied to what ZM_QuadBuild* ACTUALLY keys:
//   WALK   -> all four leg-'Up' bones (diagonal gait) + spine (Spine00 root +
//             Spine03 chest counter-yaw) + Head bob; horns do NOT wag.
//   IDLE   -> Head drift + at least one Tail bone; horns do NOT wag.
//   ATTACK -> HornL/HornR (the ONLY clip that keys horns) + Head + Spine01/03 +
//             the FORElegs (LegFLUp/LegFRUp); no hind legs, no tail.
//   SPECIAL-> the spine chain Spine01..03 + Head + the HIND legs (LegHLUp/LegHRUp);
//             no horns, no forelegs.
//   HIT    -> Spine01/03 + Head + Tail00 + a hind-leg twitch (LegHLUp); no horns.
//   FAINT  -> the spine chain Spine01..03 + all eight leg bones (Up fold + Lo knee).
//
// PURE / HEADLESS: no disk, no GPU. A clip is a closed-form function of
// (archetype, clip-id) only. Runs at boot before the scene loads.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"

namespace
{
	// True when the species' body plan has a wired ANIM builder (SC1: QUADRUPED).
	bool HasAnimBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeAnimBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}
}

// ############################################################################
// (9) The QUADRUPED archetype signature -- each clip keys the bones its motion
//     requires. FERNFAWN (F01 stage 1) is the fixed QUADRUPED reference species.
// ############################################################################

ZENITH_TEST(ZM_Gen, QuadrupedAnim_ExpectedChannels)
{
	const ZM_SPECIES_ID eId = ZM_SPECIES_FERNFAWN;

	// Non-vacuity: the reference species must actually be a wired QUADRUPED, so
	// the assertions below exercise real builder output.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(eId).m_eArchetype, (u_int)ZM_ARCHETYPE_QUADRUPED,
		"FERNFAWN must be QUADRUPED (the SC1 reference species)");
	ZENITH_ASSERT_TRUE(HasAnimBuilder(eId), "the QUADRUPED anim builder must be wired (SC1)");

	const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

	// The EXACT frozen quadruped bone-name strings (verbatim from the builder).
	const char* const aszLegUp[4] = { "LegFLUp", "LegFRUp", "LegHLUp", "LegHRUp" };
	const char* const aszLegLo[4] = { "LegFLLo", "LegFRLo", "LegHLLo", "LegHRLo" };
	const char* const aszTail[3]  = { "Tail00", "Tail01", "Tail02" };

	// ---- WALK: diagonal gait -> all four leg-'Up' bones + spine + head bob. ----
	{
		Flux_AnimationClip xWalk;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_WALK, xWalk);

		u_int uLegUp = 0u;
		for (u_int l = 0; l < 4u; ++l)
		{
			if (xWalk.HasBoneChannel(aszLegUp[l])) { ++uLegUp; }
		}
		ZENITH_ASSERT_GE(uLegUp, 4u, "WALK must animate all four leg-'Up' bones (diagonal gait)");

		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine00"), "WALK must counter-yaw the spine root (Spine00)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine03"), "WALK must counter-yaw the chest (Spine03)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Head"),    "WALK must bob the Head");

		// Structural lock (negative): horns only lead the ATTACK gore, never the walk.
		ZENITH_ASSERT_FALSE(xWalk.HasBoneChannel("HornL"), "WALK must NOT animate horns");
	}

	// ---- IDLE: attentive Head drift + tail micro-sway (>= 1 tail bone). ----
	{
		Flux_AnimationClip xIdle;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_IDLE, xIdle);

		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Head"), "IDLE must drift the Head");

		u_int uTail = 0u;
		for (u_int t = 0; t < 3u; ++t)
		{
			if (xIdle.HasBoneChannel(aszTail[t])) { ++uTail; }
		}
		ZENITH_ASSERT_GE(uTail, 1u, "IDLE must sway at least one Tail bone");

		// Structural lock (negative): horns do not move at idle.
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("HornL"), "IDLE must NOT animate horns");
	}

	// ---- ATTACK: gore lunge -> horns LEAD + head + spine thrust + the FORElegs. ----
	{
		Flux_AnimationClip xAttack;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_ATTACK, xAttack);

		// Horns lead the gore -- ATTACK is the ONLY clip that keys them.
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("HornL"), "ATTACK must lead the gore with HornL");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("HornR"), "ATTACK must lead the gore with HornR");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Head"),    "ATTACK must thrust the Head");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine01"), "ATTACK must thrust Spine01");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine03"), "ATTACK must thrust Spine03");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("LegFLUp"), "ATTACK must reach with the left foreleg");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("LegFRUp"), "ATTACK must reach with the right foreleg");

		// Structural lock (negative): Attack drives the FORElegs, not the hind legs,
		// and does not flick the tail -- distinguishing it from Special / Hit.
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("LegHLUp"), "ATTACK must NOT key the hind legs");
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("Tail00"),  "ATTACK must NOT flick the tail");
	}

	// ---- SPECIAL: rear-back tell -> spine chain (Spine01..03) + head + HIND legs. ----
	{
		Flux_AnimationClip xSpecial;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_SPECIAL, xSpecial);

		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine01"), "SPECIAL must rear Spine01");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine02"), "SPECIAL must rear Spine02");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine03"), "SPECIAL must rear Spine03");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Head"),    "SPECIAL must raise the Head");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("LegHLUp"), "SPECIAL must crouch the left hind leg");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("LegHRUp"), "SPECIAL must crouch the right hind leg");

		// Structural lock (negative): the Special tell drives the HIND legs, never the
		// forelegs, and never leads with horns -- distinguishing it from Attack.
		ZENITH_ASSERT_FALSE(xSpecial.HasBoneChannel("LegFLUp"), "SPECIAL must NOT key the forelegs");
		ZENITH_ASSERT_FALSE(xSpecial.HasBoneChannel("HornL"),   "SPECIAL must NOT lead with horns");
	}

	// ---- HIT: recoil -> Spine01/03 + head snap + base-tail flick + a hind-leg twitch. ----
	{
		Flux_AnimationClip xHit;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_HIT, xHit);

		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Spine01"), "HIT must recoil Spine01");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Spine03"), "HIT must recoil Spine03");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Head"),    "HIT must snap the Head");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Tail00"),  "HIT must flick the base tail (Tail00)");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("LegHLUp"), "HIT must twitch the left hind leg");

		// Structural lock (negative): the hit reaction never leads with horns.
		ZENITH_ASSERT_FALSE(xHit.HasBoneChannel("HornL"), "HIT must NOT key horns");
	}

	// ---- FAINT: monotonic collapse -> the spine chain (Spine01..03) + all legs
	//      ('Up' fold + 'Lo' knee). ----
	{
		Flux_AnimationClip xFaint;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT, xFaint);

		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine01"), "FAINT must fold Spine01");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine02"), "FAINT must fold Spine02");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine03"), "FAINT must fold Spine03");

		u_int uLeg = 0u;
		for (u_int l = 0; l < 4u; ++l)
		{
			if (xFaint.HasBoneChannel(aszLegUp[l])) { ++uLeg; }
			if (xFaint.HasBoneChannel(aszLegLo[l])) { ++uLeg; }
		}
		ZENITH_ASSERT_GE(uLeg, 8u, "FAINT must buckle all eight leg bones (Up fold + Lo knee)");
	}
}
