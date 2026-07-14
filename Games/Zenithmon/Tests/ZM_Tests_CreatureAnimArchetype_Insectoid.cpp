#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimArchetype_Insectoid -- S4 SC4 structural lock on the
// INSECTOID anim builder (suite ZM_Gen).
//
// Where the GENERIC ChannelsMatchSkeleton harness proves a clip binds to SOME real
// skeleton bone, THIS test pins the archetype SIGNATURE: the EXACT insectoid bone
// NAMES (extracted verbatim from ZM_CreatureAnimArchetype_Insectoid.cpp) that each
// clip actually keys. So a builder edit that drops the tripod legs from Walk, stops
// flaring the antennae on Special, or lets Attack lunge with the wrong limbs is
// caught here as a true structural regression -- not a tautology.
//
// The assertions are tied to what ZM_InsBuild* ACTUALLY keys:
//   WALK   -> the metachronal TRIPOD: BOTH tripods' hips animate (tripod A =
//             LegL0/LegL2/LegR1, tripod B = LegL1/LegR0/LegR2), the knees follow,
//             and the body sways (Spine01).
//   IDLE   -> the antennae sway (AntennaL/AntennaR -- the idle signature) + Head.
//   ATTACK -> Head + FRONT legs (LegL0Up/LegR0Up) lunge forward; it does NOT key the
//             antennae (that is Special) nor the REAR legs (LegL2Up -- that is the
//             Special rear-up).
//   SPECIAL-> the antennae FLARE (AntennaL/AntennaR) + the HIND legs rear up
//             (LegL2Up/LegR2Up) + Head -- the threat display Attack never performs.
//
// The Attack/Special antennae + front-vs-rear-leg contrast is a TRUE negative lock:
// the two clips key disjoint signature bones, so neither can silently degrade into
// the other.
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
	// True when the species' body plan has a wired ANIM builder (SC4: + INSECTOID).
	bool HasAnimBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeAnimBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}
}

// ############################################################################
// The INSECTOID archetype signature -- each clip keys the bones its motion requires.
// WRIGGLET (early butterfly, stage 1) is the fixed INSECTOID reference species.
// ############################################################################

ZENITH_TEST(ZM_Gen, InsectoidAnim_ExpectedChannels)
{
	const ZM_SPECIES_ID eId = ZM_SPECIES_WRIGGLET;

	// Non-vacuity: the reference species must actually be a wired INSECTOID, so the
	// assertions below exercise real builder output.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(eId).m_eArchetype, (u_int)ZM_ARCHETYPE_INSECTOID,
		"WRIGGLET must be INSECTOID (the SC4 reference species)");
	ZENITH_ASSERT_TRUE(HasAnimBuilder(eId), "the INSECTOID anim builder must be wired (SC4)");

	const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

	// ---- WALK: the metachronal tripod -- BOTH tripods' hips + knees + body sway. ----
	{
		Flux_AnimationClip xWalk;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_WALK, xWalk);

		// Tripod A hips (front-left / rear-left / mid-right).
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegL0Up"), "WALK must swing the front-left hip (tripod A)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegL2Up"), "WALK must swing the rear-left hip (tripod A)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegR1Up"), "WALK must swing the mid-right hip (tripod A)");
		// Tripod B hips (mid-left / front-right / rear-right).
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegL1Up"), "WALK must swing the mid-left hip (tripod B)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegR0Up"), "WALK must swing the front-right hip (tripod B)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegR2Up"), "WALK must swing the rear-right hip (tripod B)");
		// The knees follow the hips.
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegL0Lo"), "WALK must flex the front-left knee");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("LegR2Lo"), "WALK must flex the rear-right knee");
		// The body sways with the gait.
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine01"), "WALK must sway the body (Spine01)");
	}

	// ---- IDLE: the antennae sway (the idle signature) + head drift. ----
	{
		Flux_AnimationClip xIdle;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_IDLE, xIdle);

		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("AntennaL"), "IDLE must sway the left antenna");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("AntennaR"), "IDLE must sway the right antenna");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Head"),     "IDLE must drift the Head");
	}

	// ---- ATTACK: front legs + head lunge; NOT the antennae, NOT the rear legs. ----
	{
		Flux_AnimationClip xAttack;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_ATTACK, xAttack);

		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Head"),    "ATTACK must lunge the Head");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("LegL0Up"), "ATTACK must reach with the front-left leg");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("LegR0Up"), "ATTACK must reach with the front-right leg");

		// Structural lock (negative): the lunge never flares the antennae (that is
		// Special) nor rears the hind legs (that is Special's rear-up).
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("AntennaL"), "ATTACK must NOT flare the antennae (that is Special)");
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("LegL2Up"),  "ATTACK must NOT rear the hind legs (that is Special)");
	}

	// ---- SPECIAL: threat display -> antennae flare + hind-leg rear-up + head. ----
	{
		Flux_AnimationClip xSpecial;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_SPECIAL, xSpecial);

		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("AntennaL"), "SPECIAL must flare the left antenna");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("AntennaR"), "SPECIAL must flare the right antenna");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("LegL2Up"),  "SPECIAL must rear up on the left hind leg");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("LegR2Up"),  "SPECIAL must rear up on the right hind leg");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Head"),     "SPECIAL must raise the Head");
	}
}
