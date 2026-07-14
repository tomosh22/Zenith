#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimArchetype_Serpent -- S4 SC3 structural lock on the SERPENT
// anim builder (suite ZM_Gen).
//
// Where the GENERIC ChannelsMatchSkeleton harness proves a clip binds to SOME
// real skeleton bone, THIS test pins the archetype SIGNATURE: the EXACT serpent
// bone NAMES (extracted verbatim from ZM_CreatureAnimArchetype_Serpent.cpp) that
// each clip actually keys. So a builder edit that drops vertebrae from the
// undulation, stops flaring the brow horns on Special, or forgets to collapse the
// spine chain on faint is caught here as a true structural regression -- not a
// tautology.
//
// The assertions are tied to what ZM_SerpBuild* ACTUALLY keys:
//   IDLE   -> Spine01..Spine05 (lateral travelling-wave undulation) + Head + Tail;
//             the planted coil-base root Spine00 is NOT keyed, and the brow horns
//             stay still.
//   WALK   -> Spine00..Spine05 (the crawl drives the coil-base too) + Head + Tail;
//             the horns do NOT move.
//   ATTACK -> Head + the FRONT spine Spine03..Spine05 (the strike); no lower spine
//             (Spine02), no horns, no tail.
//   SPECIAL-> the brow horns FLARE (HornL/HornR -- the Special signature) + the
//             whole front rears (Spine02..Spine05 + Head); no tail.
//   HIT    -> Spine02..Spine05 + Head (a lateral recoil); no horns, no tail, no
//             coil-base.
//   FAINT  -> the entire spine chain Spine00..Spine05 + Head + Tail (the collapse);
//             the horns are left out.
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
	// True when the species' body plan has a wired ANIM builder (SC3: + SERPENT).
	bool HasAnimBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeAnimBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}
}

// ############################################################################
// The SERPENT archetype signature -- each clip keys the bones its motion requires.
// WYRMLING (Drake, stage 1) is the fixed SERPENT reference species.
// ############################################################################

ZENITH_TEST(ZM_Gen, SerpentAnim_ExpectedChannels)
{
	const ZM_SPECIES_ID eId = ZM_SPECIES_WYRMLING;

	// Non-vacuity: the reference species must actually be a wired SERPENT, so the
	// assertions below exercise real builder output.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(eId).m_eArchetype, (u_int)ZM_ARCHETYPE_SERPENT,
		"WYRMLING must be SERPENT (the SC3 reference species)");
	ZENITH_ASSERT_TRUE(HasAnimBuilder(eId), "the SERPENT anim builder must be wired (SC3)");

	const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

	// ---- IDLE: lateral travelling undulation across MULTIPLE spine bones + tail. ----
	{
		Flux_AnimationClip xIdle;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_IDLE, xIdle);

		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Spine01"), "IDLE must undulate Spine01");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Spine03"), "IDLE must undulate Spine03");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Spine05"), "IDLE must undulate the neck (Spine05)");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Head"),    "IDLE must sway the Head");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Tail00"),  "IDLE must sway the tail (Tail00)");

		// Structural lock (negative): the planted coil-base holds at idle, and the
		// brow horns are the Special threat display -- never idle.
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("Spine00"), "IDLE must NOT key the planted coil-base (Spine00)");
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("HornL"),   "IDLE must NOT flare the brow horns");
	}

	// ---- WALK: the crawl drives the coil-base too, across the whole spine + tail. ----
	{
		Flux_AnimationClip xWalk;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_WALK, xWalk);

		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine00"), "WALK must drive the coil-base (Spine00) -- unlike Idle");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine02"), "WALK must undulate Spine02");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine05"), "WALK must undulate the neck (Spine05)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Tail00"),  "WALK must undulate the tail (Tail00)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Head"),    "WALK must sway the Head");

		// Structural lock (negative): the crawl never flares the horns.
		ZENITH_ASSERT_FALSE(xWalk.HasBoneChannel("HornL"), "WALK must NOT flare the brow horns");
	}

	// ---- ATTACK: head/neck strike -> Head + front spine (Spine03..Spine05). ----
	{
		Flux_AnimationClip xAttack;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_ATTACK, xAttack);

		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Head"),    "ATTACK must whip the Head");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine05"), "ATTACK must strike from the neck (Spine05)");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine04"), "ATTACK must strike from the front spine (Spine04)");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine03"), "ATTACK must strike from the front spine (Spine03)");

		// Structural lock (negative): the strike is front-only, and never flares the
		// horns (that is Special) nor lashes the tail.
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("Spine02"), "ATTACK must NOT engage the lower spine (Spine02)");
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("HornL"),   "ATTACK must NOT flare the horns (that is Special)");
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("Tail00"),  "ATTACK must NOT lash the tail");
	}

	// ---- SPECIAL: threat display -> horn FLARE + whole front rears. ----
	{
		Flux_AnimationClip xSpecial;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_SPECIAL, xSpecial);

		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("HornL"),   "SPECIAL must flare the left brow horn");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("HornR"),   "SPECIAL must flare the right brow horn");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Head"),    "SPECIAL must rear the Head");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine05"), "SPECIAL must rear the neck (Spine05)");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine02"), "SPECIAL must rear the whole front (Spine02)");

		// Structural lock (negative): the display never lashes the tail.
		ZENITH_ASSERT_FALSE(xSpecial.HasBoneChannel("Tail00"), "SPECIAL must NOT lash the tail");
	}

	// ---- HIT: lateral recoil of the front spine + head snap. ----
	{
		Flux_AnimationClip xHit;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_HIT, xHit);

		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Spine05"), "HIT must recoil the neck (Spine05)");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Spine02"), "HIT must recoil the front spine (Spine02)");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Head"),    "HIT must snap the Head");

		// Structural lock (negative): the flinch never flares the horns, lashes the
		// tail, or drives the coil-base.
		ZENITH_ASSERT_FALSE(xHit.HasBoneChannel("HornL"),   "HIT must NOT flare the horns");
		ZENITH_ASSERT_FALSE(xHit.HasBoneChannel("Tail00"),  "HIT must NOT lash the tail");
		ZENITH_ASSERT_FALSE(xHit.HasBoneChannel("Spine00"), "HIT must NOT drive the coil-base (Spine00)");
	}

	// ---- FAINT: collapse -> the ENTIRE spine chain + Head + Tail go limp. ----
	{
		Flux_AnimationClip xFaint;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT, xFaint);

		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Head"),   "FAINT must drop the Head");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Tail00"), "FAINT must sag the tail (Tail00)");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Tail02"), "FAINT must sag the tail tip (Tail02)");

		// The whole 6-vertebra spine chain must fold (Spine00..Spine05).
		const char* const aszSpine[6] = { "Spine00", "Spine01", "Spine02", "Spine03", "Spine04", "Spine05" };
		u_int uSpine = 0u;
		for (u_int s = 0; s < 6u; ++s)
		{
			if (xFaint.HasBoneChannel(aszSpine[s])) { ++uSpine; }
		}
		ZENITH_ASSERT_GE(uSpine, 6u, "FAINT must fold the entire spine chain (all six vertebrae)");

		// Structural lock (negative): the brow horns are left out of the collapse.
		ZENITH_ASSERT_FALSE(xFaint.HasBoneChannel("HornL"), "FAINT must NOT key the brow horns");
	}
}
