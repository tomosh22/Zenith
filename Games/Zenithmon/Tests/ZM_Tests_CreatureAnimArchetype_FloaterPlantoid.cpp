#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimArchetype_FloaterPlantoid -- S4 SC5 structural lock on the
// FLOATER-PLANTOID anim builder (suite ZM_Gen).
//
// Where the GENERIC ChannelsMatchSkeleton harness proves a clip binds to SOME real
// skeleton bone, THIS test pins the archetype SIGNATURE: the EXACT floater-plantoid
// bone NAMES (extracted verbatim from ZM_CreatureAnimArchetype_FloaterPlantoid.cpp)
// that each clip actually keys. So a builder edit that stops pulsing the tendril
// skirt on the drift, stops flaring the tendrils on Special, or forgets to wilt them
// on faint is caught here as a true structural regression -- not a tautology.
//
// The assertions are tied to what ZM_FloatBuild* ACTUALLY keys:
//   IDLE   -> bulb bob (Spine01 + Spine02) + crown (Head) bob + a per-tendril radial
//             pulse across the WHOLE skirt (Tendril0..5); the bulb base (Spine00)
//             holds steady.
//   WALK   -> the drift drives the base (Spine00) too + Spine02 + Head + a stronger
//             skirt pulse (Tendril0..5).
//   ATTACK -> a forward lash: crown (Head) + bulb top (Spine02) + tendrils whip
//             forward; it does NOT key Spine01 (that is the Special bloom) -- the lock
//             separating Attack from Special.
//   SPECIAL-> a bloom: tendrils FLARE (Tendril0..5) + crown (Head) raise + bulb
//             expand (Spine01 + Spine02); Spine01 is keyed here but NOT by Attack.
//   HIT    -> recoil (Spine02 + Head) + tendrils flick inward (Tendril0..5).
//   FAINT  -> collapse: the whole bulb sags (Spine00 + Spine02) + crown (Head) droops
//             + EVERY tendril wilts slack (Tendril0..5).
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
	// True when the species' body plan has a wired ANIM builder (SC5: + FLOATER_PLANTOID
	// -- the dispatch is now total).
	bool HasAnimBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeAnimBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}
}

// ############################################################################
// The FLOATER-PLANTOID archetype signature -- each clip keys the bones its motion
// requires. PUFFSEED (F08, stage 1, the dandelion drifter) is the fixed
// FLOATER_PLANTOID reference species.
// ############################################################################

ZENITH_TEST(ZM_Gen, FloaterPlantoidAnim_ExpectedChannels)
{
	const ZM_SPECIES_ID eId = ZM_SPECIES_PUFFSEED;

	// Non-vacuity: the reference species must actually be a wired FLOATER_PLANTOID, so
	// the assertions below exercise real builder output.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(eId).m_eArchetype, (u_int)ZM_ARCHETYPE_FLOATER_PLANTOID,
		"PUFFSEED must be FLOATER_PLANTOID (the SC5 reference species)");
	ZENITH_ASSERT_TRUE(HasAnimBuilder(eId), "the FLOATER_PLANTOID anim builder must be wired (SC5)");

	const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

	// ---- IDLE: bulb bob + crown bob + a soft radial skirt pulse across the tendrils. ----
	{
		Flux_AnimationClip xIdle;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_IDLE, xIdle);

		// MULTIPLE tendrils pulse (the jellyfish-bell wave).
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Tendril0"), "IDLE must pulse tendril 0");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Tendril2"), "IDLE must pulse tendril 2");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Tendril5"), "IDLE must pulse tendril 5");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Head"),     "IDLE must bob the crown (Head)");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Spine02"),  "IDLE must bob the bulb (Spine02)");

		// Structural lock (negative): the bulb base holds steady at idle (driven only on
		// the stronger drift), so Spine00 is NOT keyed.
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("Spine00"), "IDLE must NOT drive the bulb base (Spine00)");
	}

	// ---- WALK (drift): base-driven bob + a STRONGER skirt paddle. ----
	{
		Flux_AnimationClip xWalk;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_WALK, xWalk);

		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Tendril0"), "WALK must paddle tendril 0");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Tendril3"), "WALK must paddle tendril 3");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Head"),     "WALK must bob the crown (Head)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine02"),  "WALK must bob the bulb (Spine02)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine00"),  "WALK must drive the bulb base (Spine00) -- unlike Idle");
	}

	// ---- ATTACK: forward lash -> crown thrust + bulb-top lean + tendrils whip forward. ----
	{
		Flux_AnimationClip xAttack;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_ATTACK, xAttack);

		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Head"),     "ATTACK must thrust the crown (Head)");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine02"),  "ATTACK must lean the bulb top (Spine02)");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Tendril0"), "ATTACK must lash tendril 0 forward");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Tendril4"), "ATTACK must lash tendril 4 forward");

		// Structural lock (negative): the lash never expands the mid-bulb (Spine01) -- that
		// is the Special bloom. This is the lock separating Attack from Special.
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("Spine01"), "ATTACK must NOT expand the mid-bulb (Spine01) -- that is Special");
	}

	// ---- SPECIAL: bloom -> tendrils FLARE + crown raise + bulb (Spine01) expand. ----
	{
		Flux_AnimationClip xSpecial;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_SPECIAL, xSpecial);

		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Tendril0"), "SPECIAL must flare tendril 0");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Tendril3"), "SPECIAL must flare tendril 3");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Head"),     "SPECIAL must raise the crown (Head)");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine02"),  "SPECIAL must expand the bulb top (Spine02)");

		// The POSITIVE half of the Attack/Special lock: Special keys Spine01, which Attack
		// (asserted above) does not -- so the two clips key genuinely different bone sets.
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine01"),  "SPECIAL must expand the mid-bulb (Spine01) -- the lock vs Attack");
	}

	// ---- HIT: recoil + tendrils flick inward. ----
	{
		Flux_AnimationClip xHit;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_HIT, xHit);

		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Spine02"),  "HIT must recoil the bulb top (Spine02)");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Head"),     "HIT must snap the crown (Head)");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Tendril0"), "HIT must flick tendril 0 inward");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Tendril5"), "HIT must flick tendril 5 inward");
	}

	// ---- FAINT: collapse -> bulb sag + crown droop + EVERY tendril wilts slack. ----
	{
		Flux_AnimationClip xFaint;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT, xFaint);

		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine00"),  "FAINT must sag the bulb base (Spine00)");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine02"),  "FAINT must sag the bulb top (Spine02)");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Head"),     "FAINT must droop the crown (Head)");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Tendril0"), "FAINT must wilt tendril 0");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Tendril3"), "FAINT must wilt tendril 3");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Tendril5"), "FAINT must wilt tendril 5");
	}
}
