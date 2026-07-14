#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimArchetype_Aquatic -- S4 SC3 structural lock on the AQUATIC
// anim builder (suite ZM_Gen).
//
// Where the GENERIC ChannelsMatchSkeleton harness proves a clip binds to SOME
// real skeleton bone, THIS test pins the archetype SIGNATURE: the EXACT aquatic
// bone NAMES (extracted verbatim from ZM_CreatureAnimArchetype_Aquatic.cpp) that
// each clip actually keys. So a builder edit that stops sweeping the caudal fin on
// the swim, stops flaring the pectorals on Special, or forgets to slacken the fins
// on faint is caught here as a true structural regression -- not a tautology.
//
// The assertions are tied to what ZM_AquaBuild* ACTUALLY keys:
//   IDLE   -> body-yaw (Spine02 + Head) + FinCaudal trailing sway + both pectorals
//             (FinPecL/R) slow paddle; the pelvis root (Spine00) holds and the
//             dorsal sail (FinDorsal) stays still.
//   WALK   -> the swim drives the pelvis (Spine00) + Spine02 + Head + a strong
//             FinCaudal sweep + pectoral paddle; no dorsal.
//   ATTACK -> a body ram: Head + Spine00..Spine02 + FinCaudal thrust; no pectorals,
//             no dorsal (those are the Special display).
//   SPECIAL-> pectorals FLARE (FinPecL/R) + FinDorsal erects + body arches
//             (Spine02 + Head); it NEVER keys FinCaudal (the caudal thrust is the
//             Attack ram), which is the lock separating Special from Attack.
//   HIT    -> body recoil (Spine02 + Head) + FinCaudal + pectoral flick; no dorsal.
//   FAINT  -> body roll (Spine00 + Spine02 + Head) + ALL fins slack (FinDorsal,
//             FinPecL/R, FinCaudal).
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
	// True when the species' body plan has a wired ANIM builder (SC3: + AQUATIC).
	bool HasAnimBuilder(ZM_SPECIES_ID eId)
	{
		return ZM_GetArchetypeAnimBuilder(ZM_GetSpeciesData(eId).m_eArchetype) != nullptr;
	}
}

// ############################################################################
// The AQUATIC archetype signature -- each clip keys the bones its motion requires.
// FINLET (Water, stage 1) is the fixed AQUATIC reference species.
// ############################################################################

ZENITH_TEST(ZM_Gen, AquaticAnim_ExpectedChannels)
{
	const ZM_SPECIES_ID eId = ZM_SPECIES_FINLET;

	// Non-vacuity: the reference species must actually be a wired AQUATIC, so the
	// assertions below exercise real builder output.
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(eId).m_eArchetype, (u_int)ZM_ARCHETYPE_AQUATIC,
		"FINLET must be AQUATIC (the SC3 reference species)");
	ZENITH_ASSERT_TRUE(HasAnimBuilder(eId), "the AQUATIC anim builder must be wired (SC3)");

	const ZM_ARCHETYPE eArch = ZM_GetSpeciesData(eId).m_eArchetype;

	// ---- IDLE: body-yaw + caudal trailing sway + pectoral slow paddle. ----
	{
		Flux_AnimationClip xIdle;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_IDLE, xIdle);

		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("FinCaudal"), "IDLE must trail-sway the caudal fin");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("FinPecL"),   "IDLE must paddle the left pectoral");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("FinPecR"),   "IDLE must paddle the right pectoral");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Spine02"),   "IDLE must yaw the body (Spine02)");
		ZENITH_ASSERT_TRUE(xIdle.HasBoneChannel("Head"),      "IDLE must counter-yaw the Head");

		// Structural lock (negative): the pelvis holds at idle, and the dorsal sail is
		// the Special display -- never idle.
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("Spine00"),   "IDLE must NOT drive the pelvis (Spine00)");
		ZENITH_ASSERT_FALSE(xIdle.HasBoneChannel("FinDorsal"), "IDLE must NOT raise the dorsal sail");
	}

	// ---- WALK (swim): pelvis-driven body-yaw + a STRONG caudal sweep + paddle. ----
	{
		Flux_AnimationClip xWalk;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_WALK, xWalk);

		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("FinCaudal"), "WALK must sweep the caudal fin (propulsion)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine00"),   "WALK must drive the pelvis (Spine00) -- unlike Idle");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Spine02"),   "WALK must yaw the body (Spine02)");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("Head"),      "WALK must counter-yaw the Head");
		ZENITH_ASSERT_TRUE(xWalk.HasBoneChannel("FinPecL"),   "WALK must paddle the left pectoral");

		// Structural lock (negative): swimming never raises the dorsal display sail.
		ZENITH_ASSERT_FALSE(xWalk.HasBoneChannel("FinDorsal"), "WALK must NOT raise the dorsal sail");
	}

	// ---- ATTACK: body ram -> Head + spine thrust + caudal power stroke. ----
	{
		Flux_AnimationClip xAttack;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_ATTACK, xAttack);

		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Head"),      "ATTACK must thrust the Head");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine00"),   "ATTACK must ram from the pelvis (Spine00)");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("Spine02"),   "ATTACK must ram the body (Spine02)");
		ZENITH_ASSERT_TRUE(xAttack.HasBoneChannel("FinCaudal"), "ATTACK must drive with the caudal fin");

		// Structural lock (negative): the ram never flares the display fins -- that is
		// the Special.
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("FinPecL"),   "ATTACK must NOT flare the pectorals (that is Special)");
		ZENITH_ASSERT_FALSE(xAttack.HasBoneChannel("FinDorsal"), "ATTACK must NOT raise the dorsal sail");
	}

	// ---- SPECIAL: display -> pectoral FLARE + dorsal erect + body arch. ----
	{
		Flux_AnimationClip xSpecial;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_SPECIAL, xSpecial);

		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("FinPecL"),   "SPECIAL must flare the left pectoral");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("FinPecR"),   "SPECIAL must flare the right pectoral");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("FinDorsal"), "SPECIAL must erect the dorsal sail");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Spine02"),   "SPECIAL must arch the body (Spine02)");
		ZENITH_ASSERT_TRUE(xSpecial.HasBoneChannel("Head"),      "SPECIAL must raise the Head");

		// Structural lock (negative): the display never drives the caudal thrust (that
		// is the Attack ram) -- the lock that separates Special from Attack.
		ZENITH_ASSERT_FALSE(xSpecial.HasBoneChannel("FinCaudal"), "SPECIAL must NOT drive the caudal fin (that is Attack)");
	}

	// ---- HIT: body recoil + caudal + pectoral fin flick. ----
	{
		Flux_AnimationClip xHit;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_HIT, xHit);

		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Spine02"),   "HIT must recoil the body (Spine02)");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("Head"),      "HIT must snap the Head");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("FinCaudal"), "HIT must flick the caudal fin");
		ZENITH_ASSERT_TRUE(xHit.HasBoneChannel("FinPecL"),   "HIT must flick the left pectoral");

		// Structural lock (negative): the flinch never raises the dorsal display sail.
		ZENITH_ASSERT_FALSE(xHit.HasBoneChannel("FinDorsal"), "HIT must NOT raise the dorsal sail");
	}

	// ---- FAINT: collapse -> body roll + EVERY fin goes slack. ----
	{
		Flux_AnimationClip xFaint;
		ZM_BuildCreatureClip(eArch, ZM_ANIM_CLIP_FAINT, xFaint);

		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine00"),   "FAINT must roll the pelvis (Spine00)");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Spine02"),   "FAINT must roll the body (Spine02)");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("Head"),      "FAINT must droop the Head");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("FinDorsal"), "FAINT must slacken the dorsal sail");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("FinPecL"),   "FAINT must slacken the left pectoral");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("FinPecR"),   "FAINT must slacken the right pectoral");
		ZENITH_ASSERT_TRUE(xFaint.HasBoneChannel("FinCaudal"), "FAINT must slacken the caudal fin");
	}
}
