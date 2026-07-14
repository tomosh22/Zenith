#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimArchetype_Avian -- S4 SC2 structural lock on the AVIAN
// anim builder (suite ZM_Gen).
//
// Where the GENERIC ChannelsMatchSkeleton harness proves a clip binds to SOME
// real skeleton bone, THIS test pins the archetype SIGNATURE: the EXACT avian
// bone NAMES (extracted verbatim from ZM_CreatureAnimArchetype_Avian.cpp) that
// each clip actually keys. So a builder edit that drops a leg from the walk cycle,
// stops flaring the wings on Special, or forgets to droop the wings on faint is
// caught here as a true structural regression -- not a tautology.
//
// The assertions are tied to what ZM_AvianBuild* ACTUALLY keys:
//   IDLE   -> both Wings (settle micro-flap) + Head bob + both Tail bones; legs
//             stay planted and the Beak stays still.
//   WALK   -> both leg-'Up' bones (opposed gait) + both Tail bones (counterbalance)
//             + the wings settling + Head; the Beak does NOT move.
//   ATTACK -> Head + Beak (the peck lead -- the ONLY clip that keys the Beak) +
//             the wings tucking; no legs, no Tail.
//   SPECIAL-> both Wings (the wide FLARE) + Head raise + Tail00 fan; the Beak is
//             NOT keyed (that is the peck signature).
//   HIT    -> Spine01/02 + Head + a both-wing flinch; no Tail, no Beak.
//   FAINT  -> the spine + both Wings (drooping slack) + all four leg bones + both
//             Tail bones.
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
	// True when the species' body plan has a wired ANIM builder (SC2: + AVIAN).
	bool HasAnimBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeAnimBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}
}

// ############################################################################
// The AVIAN archetype signature -- each clip keys the bones its motion requires.
// PIPWIT (Normal/Sky, stage 1) is the fixed AVIAN reference species.
// ############################################################################

ZENITH_TEST(ZM_Gen, AvianAnim_ExpectedChannels)
{
	const ZM_SPECIES_ID eId = ZM_SPECIES_PIPWIT;

	// Non-vacuity: the reference species must actually be a wired AVIAN, so the
	// assertions below exercise real builder output.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(eId).m_eArchetype, (u_int)ZM_ARCHETYPE_AVIAN,
		"PIPWIT must be AVIAN (the SC2 reference species)");
	ZENITH_ASSERT_TRUE(HasAnimBuilder(eId), "the AVIAN anim builder must be wired (SC2)");

	const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

	// ---- IDLE: wing settle-flap + Head bob + tail balance; legs planted. ----
	{
		Flux_AnimationClip xIdle;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_IDLE, xIdle);

		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("WingL"),  "IDLE must settle-flap the left wing");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("WingR"),  "IDLE must settle-flap the right wing");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Head"),   "IDLE must bob the Head");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Tail00"), "IDLE must balance-sway the tail (Tail00)");

		// Structural lock (negative): the bird stands planted at idle (no legs) and
		// the Beak is the ATTACK peck lead, never idle.
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("LegLUp"), "IDLE must NOT step (no leg motion)");
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("Beak"),   "IDLE must NOT key the Beak");
	}

	// ---- WALK: opposed leg gait + tail counterbalance + wing settle + Head. ----
	{
		Flux_AnimationClip xWalk;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_WALK, xWalk);

		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegLUp"), "WALK must step the left leg (LegLUp)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegRUp"), "WALK must step the right leg (LegRUp)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Tail00"), "WALK must counterbalance with the tail (Tail00)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("WingL"),  "WALK must settle the left wing");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Head"),   "WALK must bob the Head");

		// Structural lock (negative): the walk never pecks (no Beak).
		ZENITH_ASSERT_FALSE(xWalk.HasBoneChannel("Beak"), "WALK must NOT key the Beak");
	}

	// ---- ATTACK: peck -> Head + Beak lead + wings tuck. ----
	{
		Flux_AnimationClip xAttack;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_ATTACK, xAttack);

		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Beak"),    "ATTACK must lead the peck with the Beak");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Head"),    "ATTACK must thrust the Head");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine02"), "ATTACK must thrust the neck (Spine02)");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("WingL"),   "ATTACK must tuck the left wing");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("WingR"),   "ATTACK must tuck the right wing");

		// Structural lock (negative): the peck neither steps nor fans the tail --
		// distinguishing it from Walk and from the tail-fanning Special.
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("LegLUp"), "ATTACK must NOT step");
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("Tail00"), "ATTACK must NOT fan the tail");
	}

	// ---- SPECIAL: screech tell -> wings FLARE wide + Head raise + Tail00 fan. ----
	{
		Flux_AnimationClip xSpecial;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_SPECIAL, xSpecial);

		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("WingL"),   "SPECIAL must flare the left wing");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("WingR"),   "SPECIAL must flare the right wing");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Head"),    "SPECIAL must raise the Head");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Tail00"),  "SPECIAL must fan the tail (Tail00)");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine02"), "SPECIAL must puff the neck back (Spine02)");

		// Structural lock (negative): the screech display never pecks with the Beak
		// (the Beak is the Attack signature) and never steps.
		ZENITH_ASSERT_FALSE(xSpecial.HasBoneChannel("Beak"),   "SPECIAL must NOT key the Beak (that is Attack)");
		ZENITH_ASSERT_FALSE(xSpecial.HasBoneChannel("LegLUp"), "SPECIAL must NOT step");
	}

	// ---- HIT: spine/head recoil + a both-wing flinch; no tail, no beak. ----
	{
		Flux_AnimationClip xHit;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_HIT, xHit);

		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Spine01"), "HIT must recoil Spine01");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Spine02"), "HIT must recoil the neck (Spine02)");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Head"),    "HIT must snap the Head");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("WingL"),   "HIT must flinch the left wing");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("WingR"),   "HIT must flinch the right wing");

		// Structural lock (negative): the flinch does not fan the tail (that is the
		// Special) and does not peck.
		ZENITH_ASSERT_FALSE(xHit.HasBoneChannel("Tail00"), "HIT must NOT fan the tail");
		ZENITH_ASSERT_FALSE(xHit.HasBoneChannel("Beak"),   "HIT must NOT key the Beak");
	}

	// ---- FAINT: collapse -> spine + wings droop slack + all four leg bones + tail. ----
	{
		Flux_AnimationClip xFaint;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT, xFaint);

		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine01"), "FAINT must fold Spine01");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine02"), "FAINT must fold Spine02");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Head"),    "FAINT must drop the Head");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("WingL"),   "FAINT must droop the left wing slack");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("WingR"),   "FAINT must droop the right wing slack");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Tail00"),  "FAINT must sag the tail (Tail00)");

		const char* const aszLegUp[2] = { "LegLUp", "LegRUp" };
		const char* const aszLegLo[2] = { "LegLLo", "LegRLo" };
		u_int uLeg = 0u;
		for (u_int l = 0; l < 2u; ++l)
		{
			if (xFaint.HasBoneChannel(aszLegUp[l])) { ++uLeg; }
			if (xFaint.HasBoneChannel(aszLegLo[l])) { ++uLeg; }
		}
		ZENITH_ASSERT_GE(uLeg, 4u, "FAINT must buckle all four leg bones (Up fold + Lo knee)");
	}
}
