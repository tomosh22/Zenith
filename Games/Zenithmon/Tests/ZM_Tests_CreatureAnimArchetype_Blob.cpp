#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimArchetype_Blob -- S4 SC4 structural lock on the BLOB anim
// builder (suite ZM_Gen).
//
// Where the GENERIC ChannelsMatchSkeleton harness proves a clip binds to SOME real
// skeleton bone, THIS test pins the archetype SIGNATURE: the EXACT blob bone NAMES
// (extracted verbatim from ZM_CreatureAnimArchetype_Blob.cpp -- just Spine00,
// Spine01, Spine02 and the crown Nub) that each clip actually keys. With only four
// bones and no limbs, the clips are distinguished by WHICH nodes they drive, so
// this test locks that structure: a builder edit that stops driving the base on the
// bounce, or lets Attack coil the base like Special, is caught as a real regression.
//
// The assertions are tied to what ZM_BlobBuild* ACTUALLY keys:
//   IDLE   -> the mid + crown spine wobble (Spine01/Spine02) + the Nub; the planted
//             base (Spine00) is left alone.
//   WALK   -> the bouncing pulse drives the WHOLE body INCLUDING the base
//             (Spine00/Spine01/Spine02) + the Nub -- unlike Idle, which leaves the
//             base planted.
//   ATTACK -> the mid + crown lunge (Spine01/Spine02) + the Nub jab; it does NOT
//             engage the base (Spine00 -- that whole-body coil is the Special).
//   SPECIAL-> the whole-body coil rears the base too (Spine00) + the Nub -- the
//             wind-up Attack never performs.
//
// The Spine00 (base) contrast is a TRUE negative lock: Idle/Attack leave it planted
// while Walk/Special drive it, so neither pair can silently collapse into the other.
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
	// True when the species' body plan has a wired ANIM builder (SC4: + BLOB).
	bool HasAnimBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeAnimBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}
}

// ############################################################################
// The BLOB archetype signature -- each clip keys the body nodes its motion requires.
// RUBBLET (walking cairn, stage 1) is the fixed BLOB reference species.
// ############################################################################

ZENITH_TEST(ZM_Gen, BlobAnim_ExpectedChannels)
{
	const ZM_SPECIES_ID eId = ZM_SPECIES_RUBBLET;

	// Non-vacuity: the reference species must actually be a wired BLOB, so the
	// assertions below exercise real builder output.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(eId).m_eArchetype, (u_int)ZM_ARCHETYPE_BLOB,
		"RUBBLET must be BLOB (the SC4 reference species)");
	ZENITH_ASSERT_TRUE(HasAnimBuilder(eId), "the BLOB anim builder must be wired (SC4)");

	const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

	// ---- IDLE: mid + crown wobble + Nub; the planted base stays put. ----
	{
		Flux_AnimationClip xIdle;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_IDLE, xIdle);

		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Spine01"), "IDLE must wobble the mid body (Spine01)");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Spine02"), "IDLE must wobble the crown (Spine02)");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Nub"),     "IDLE must sway the Nub");

		// Structural lock (negative): the planted base holds at idle.
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("Spine00"), "IDLE must NOT drive the planted base (Spine00)");
	}

	// ---- WALK: the bounce pulse drives the WHOLE body (base too) + Nub. ----
	{
		Flux_AnimationClip xWalk;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_WALK, xWalk);

		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine00"), "WALK must pulse the base (Spine00) -- unlike Idle");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine01"), "WALK must pulse the mid body (Spine01)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine02"), "WALK must pulse the crown (Spine02)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Nub"),     "WALK must bob the Nub");
	}

	// ---- ATTACK: mid + crown lunge + Nub jab; the base is NOT engaged. ----
	{
		Flux_AnimationClip xAttack;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_ATTACK, xAttack);

		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine02"), "ATTACK must lunge the crown (Spine02)");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Nub"),     "ATTACK must jab the Nub forward");

		// Structural lock (negative): the lunge never coils the base (that is Special).
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("Spine00"), "ATTACK must NOT coil the base (that is Special)");
	}

	// ---- SPECIAL: the whole-body coil rears the base too + the Nub. ----
	{
		Flux_AnimationClip xSpecial;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_SPECIAL, xSpecial);

		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine00"), "SPECIAL must coil the base (Spine00) -- unlike Attack");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine02"), "SPECIAL must coil the crown (Spine02)");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Nub"),     "SPECIAL must whip the Nub");
	}
}
