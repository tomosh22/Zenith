#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimArchetype_Biped -- S4 SC2 structural lock on the BIPED
// anim builder (suite ZM_Gen).
//
// Where the GENERIC ChannelsMatchSkeleton harness proves a clip binds to SOME
// real skeleton bone, THIS test pins the archetype SIGNATURE: the EXACT biped
// bone NAMES (extracted verbatim from ZM_CreatureAnimArchetype_Biped.cpp) that
// each clip actually keys. So a builder edit that drops a leg from the walk cycle,
// stops raising both arms on Special, or forgets to buckle the legs on faint is
// caught here as a true structural regression -- not a tautology.
//
// The assertions are tied to what ZM_BipedBuild* ACTUALLY keys:
//   WALK   -> both leg-'Up' bones (opposed gait) + both arm-'Up' bones (counter-
//             swing) + spine (Spine00 pelvis + Spine03 shoulders) + Head bob;
//             Crest does NOT move.
//   IDLE   -> Head drift + the spine chain (Spine01..03) + arm micro-sway; legs
//             stay planted and the Crest stays still.
//   ATTACK -> the RIGHT (striking) arm (ArmRUp/Lo) + Head + Spine01/03; the LEFT
//             arm is NOT keyed (that is the Special two-arm tell) and the Crest is
//             NOT keyed.
//   SPECIAL-> BOTH arms (ArmLUp + ArmRUp) + the spine chain + Head + the Crest
//             (the ONLY clip that flares the Crest); no legs.
//   HIT    -> Spine01/03 + Head + a both-arm flail (ArmLUp/ArmRUp); no Crest.
//   FAINT  -> the spine chain (Spine01..03) + all four leg bones (Up fold + Lo
//             knee) + the arms buckling.
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
	// True when the species' body plan has a wired ANIM builder (SC2: + BIPED).
	bool HasAnimBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeAnimBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}
}

// ############################################################################
// The BIPED archetype signature -- each clip keys the bones its motion requires.
// KINDLET (Fire, stage 1) is the fixed BIPED reference species.
// ############################################################################

ZENITH_TEST(ZM_Gen, BipedAnim_ExpectedChannels)
{
	const ZM_SPECIES_ID eId = ZM_SPECIES_KINDLET;

	// Non-vacuity: the reference species must actually be a wired BIPED, so the
	// assertions below exercise real builder output.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(eId).m_eArchetype, (u_int)ZM_ARCHETYPE_BIPED,
		"KINDLET must be BIPED (the SC2 reference species)");
	ZENITH_ASSERT_TRUE(HasAnimBuilder(eId), "the BIPED anim builder must be wired (SC2)");

	const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

	// ---- WALK: opposed gait -> both leg-'Up' + both arm-'Up' + spine + head. ----
	{
		Flux_AnimationClip xWalk;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_WALK, xWalk);

		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegLUp"), "WALK must swing the left leg (LegLUp)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegRUp"), "WALK must swing the right leg (LegRUp)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("ArmLUp"), "WALK must counter-swing the left arm (ArmLUp)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("ArmRUp"), "WALK must counter-swing the right arm (ArmRUp)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine00"), "WALK must counter-yaw the pelvis (Spine00)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine03"), "WALK must counter-yaw the shoulders (Spine03)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Head"),    "WALK must bob the Head");

		// Structural lock (negative): the Crest is the SPECIAL tell, never the walk.
		ZENITH_ASSERT_FALSE(xWalk.HasBoneChannel("Crest"), "WALK must NOT flare the Crest");
	}

	// ---- IDLE: attentive Head drift + spine sway + arm micro-sway; legs planted. ----
	{
		Flux_AnimationClip xIdle;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_IDLE, xIdle);

		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Head"),    "IDLE must drift the Head");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Spine01"), "IDLE must sway Spine01");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Spine03"), "IDLE must sway the shoulders (Spine03)");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("ArmLUp"),  "IDLE must micro-sway the left arm");

		// Structural lock (negative): the biped stands planted at idle (no legs) and
		// the Crest does not move.
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("LegLUp"), "IDLE must NOT step (no leg motion)");
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("Crest"),  "IDLE must NOT flare the Crest");
	}

	// ---- ATTACK: right-arm melee swing + head/spine thrust; LEFT arm stays out. ----
	{
		Flux_AnimationClip xAttack;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_ATTACK, xAttack);

		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("ArmRUp"),  "ATTACK must swing the striking arm (ArmRUp)");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("ArmRLo"),  "ATTACK must extend the striking forearm (ArmRLo)");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Head"),    "ATTACK must thrust the Head");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine01"), "ATTACK must thrust Spine01");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine03"), "ATTACK must thrust Spine03");

		// Structural lock (negative): Attack is ONE-armed (the left arm is untouched)
		// and never flares the Crest -- distinguishing it from the two-arm Special and
		// the both-arm Hit flail.
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("ArmLUp"), "ATTACK must NOT key the off arm (that is the Special tell)");
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("Crest"),  "ATTACK must NOT flare the Crest");
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("LegLUp"), "ATTACK must NOT step");
	}

	// ---- SPECIAL: both arms raise + spine lean back + Crest flare (the tell). ----
	{
		Flux_AnimationClip xSpecial;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_SPECIAL, xSpecial);

		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("ArmLUp"),  "SPECIAL must raise the left arm");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("ArmRUp"),  "SPECIAL must raise the right arm");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine01"), "SPECIAL must lean Spine01 back");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine03"), "SPECIAL must lean the shoulders back (Spine03)");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Head"),    "SPECIAL must raise the Head");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Crest"),   "SPECIAL must flare the Crest (its unique tell)");

		// Structural lock (negative): the Special charge drives BOTH arms, never the
		// legs.
		ZENITH_ASSERT_FALSE(xSpecial.HasBoneChannel("LegLUp"), "SPECIAL must NOT step");
	}

	// ---- HIT: spine/head recoil + a both-arm flail; no Crest. ----
	{
		Flux_AnimationClip xHit;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_HIT, xHit);

		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Spine01"), "HIT must recoil Spine01");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Spine03"), "HIT must recoil Spine03");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Head"),    "HIT must snap the Head");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("ArmLUp"),  "HIT must flail the left arm");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("ArmRUp"),  "HIT must flail the right arm");

		// Structural lock (negative): the hit reaction never flares the Crest and
		// never steps.
		ZENITH_ASSERT_FALSE(xHit.HasBoneChannel("Crest"),  "HIT must NOT flare the Crest");
		ZENITH_ASSERT_FALSE(xHit.HasBoneChannel("LegLUp"), "HIT must NOT step");
	}

	// ---- FAINT: monotonic collapse -> spine chain (Spine01..03) + all four leg
	//      bones ('Up' fold + 'Lo' knee) + the arms buckling. ----
	{
		Flux_AnimationClip xFaint;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT, xFaint);

		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine01"), "FAINT must fold Spine01");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine02"), "FAINT must fold Spine02");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine03"), "FAINT must fold Spine03");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Head"),    "FAINT must drop the Head");

		const char* const aszLegUp[2] = { "LegLUp", "LegRUp" };
		const char* const aszLegLo[2] = { "LegLLo", "LegRLo" };
		u_int uLeg = 0u;
		for (u_int l = 0; l < 2u; ++l)
		{
			if (xFaint.HasBoneChannel(aszLegUp[l])) { ++uLeg; }
			if (xFaint.HasBoneChannel(aszLegLo[l])) { ++uLeg; }
		}
		ZENITH_ASSERT_GE(uLeg, 4u, "FAINT must buckle all four leg bones (Up fold + Lo knee)");

		// The arms buckle under too.
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("ArmLUp"), "FAINT must flop the left arm forward");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("ArmRUp"), "FAINT must flop the right arm forward");
	}
}
