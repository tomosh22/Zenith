#include "Zenith.h"

// ============================================================================
// ZM_Tests_Interaction -- S6 item 3 (SC1 + SC2) unit tests for the NPC-interaction
// foundation: the pure ZM_ShouldInteract gate, its reject-reason name formatter,
// the key-binding collision units that keep the interact key from aliasing
// anything Zenithmon already claims, and (SC2) the pure candidate picker plus its
// facing-vector helper.
//
// Everything here is PURE: no ECS, no scene, no graphics, no baked assets, no
// engine instance -- the key-set constants are constexpr and the gate is all
// bools in / one enum out. Every fixture is deterministic and hermetic, so no
// RequestSkip is needed.
//
// ★ S7 item 1 SC3 APPENDED THE WALK-UP UNITS AT THE BOTTOM, AND THEY CREATE NO
// ENTITY. That is a hard constraint rather than a style note: scene authoring bakes
// in the entity indices assigned during the boot it runs in, and the boot-time unit
// suite allocates entities FIRST -- so one entity-creating boot unit re-authors
// different Dawnmere.zscen bytes and invalidates the two-boot hash proof SC3 owes.
// The units below call only PURE STATICS of ZM_Interactable (nothing is
// constructed), the pure ZM_TrainerSightFsm (a plain value type), and the pure
// walker/approach maths.
// ============================================================================

#include <cmath>     // sqrt / isfinite (the S7 item 1 SC3 walk-up simulation)
#include <cstring>   // strcmp (reject-name distinctness)
#include <limits>    // quiet_NaN (the SC3 primitive's documented non-finite arm)

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Components/ZM_Interactable.h"        // the S7 item 1 SC3 PURE statics (nothing is constructed)
#include "Zenithmon/Components/ZM_PlayerController.h"    // fWALK_SPEED -- the walk-up's speed, by name not by literal
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"
#include "Zenithmon/Source/Interaction/ZM_NpcWalkerLogic.h"     // ZM_BuildPatrolVelocity -- the shared velocity idiom
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"    // ZM_StepTrainerApproach + the machine
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"  // fZM_SIGHT_MAX_DISTANCE -- the cone the walk must cross
#include "Zenithmon/Source/ZM_InputActions.h"   // the key-set constants the collision units walk

// ---- ZM_ShouldInteract: one unit per blocker --------------------------------

ZENITH_TEST(ZM_Interaction, Gate_AllConditionsMetReturnsOK)
{
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(
		/* pressed */ true, /* menuOpen */ false, /* overworld */ true,
		/* warp */ false, /* battleTransition */ false, /* movementEnabled */ true),
		(u_int)ZM_INTERACT_OK,
		"edge + no menu + overworld + no warp + no battle + free player -> interact");
}

ZENITH_TEST(ZM_Interaction, Gate_NoEdgeReturnsNoInputEdge)
{
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(false, false, true, false, false, true),
		(u_int)ZM_INTERACT_REJECT_NO_INPUT_EDGE,
		"no interact edge this frame -> never interacts, and says so");
}

ZENITH_TEST(ZM_Interaction, Gate_MenuOpenReturnsMenuOpen)
{
	// The menu lock is what stops the SAME non-consuming edge from re-raising the
	// conversation that is already on screen (a re-raise loop).
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(true, true, true, false, false, true),
		(u_int)ZM_INTERACT_REJECT_MENU_OPEN,
		"a menu / dialogue already owns the screen -> refuse with MENU_OPEN");
}

ZENITH_TEST(ZM_Interaction, Gate_NotOverworldReturnsNotOverworld)
{
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(true, false, false, false, false, true),
		(u_int)ZM_INTERACT_REJECT_NOT_OVERWORLD,
		"title screen / battle scene -> nothing is talkable");
}

ZENITH_TEST(ZM_Interaction, Gate_WarpReturnsWarpInProgress)
{
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(true, false, true, true, false, true),
		(u_int)ZM_INTERACT_REJECT_WARP_IN_PROGRESS,
		"a warp owns the screen -> no conversation may land mid-warp");
}

ZENITH_TEST(ZM_Interaction, Gate_BattleTransitionReturnsBattleTransition)
{
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(true, false, true, false, true, true),
		(u_int)ZM_INTERACT_REJECT_BATTLE_TRANSITION,
		"a battle fade owns the screen -> no conversation may land mid-fade");
}

ZENITH_TEST(ZM_Interaction, Gate_PlayerFrozenReturnsPlayerFrozen)
{
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(true, false, true, false, false, false),
		(u_int)ZM_INTERACT_REJECT_PLAYER_FROZEN,
		"a frozen (scripted) player cannot start a new interaction");
}

// ---- Blocker precedence -----------------------------------------------------

ZENITH_TEST(ZM_Interaction, Gate_BlockerPrecedenceIsStable)
{
	// One case per ADJACENT pair in the precedence chain: both blockers hold, and
	// the EARLIER one must be the reported reason. Later windowed tests assert on
	// specific reject values, so a reorder here would silently change what they mean.

	// 1 NO_INPUT_EDGE beats 2 MENU_OPEN.
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(false, true, true, false, false, true),
		(u_int)ZM_INTERACT_REJECT_NO_INPUT_EDGE,
		"no edge outranks an open menu");

	// 2 MENU_OPEN beats 3 NOT_OVERWORLD.
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(true, true, false, false, false, true),
		(u_int)ZM_INTERACT_REJECT_MENU_OPEN,
		"an open menu outranks a non-overworld scene");

	// 3 NOT_OVERWORLD beats 4 WARP_IN_PROGRESS.
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(true, false, false, true, false, true),
		(u_int)ZM_INTERACT_REJECT_NOT_OVERWORLD,
		"a non-overworld scene outranks a warp");

	// 4 WARP_IN_PROGRESS beats 5 BATTLE_TRANSITION.
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(true, false, true, true, true, true),
		(u_int)ZM_INTERACT_REJECT_WARP_IN_PROGRESS,
		"a warp outranks a battle transition");

	// 5 BATTLE_TRANSITION beats 6 PLAYER_FROZEN.
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(true, false, true, false, true, false),
		(u_int)ZM_INTERACT_REJECT_BATTLE_TRANSITION,
		"a battle transition outranks a frozen player");

	// 6 PLAYER_FROZEN beats the OK fallthrough.
	ZENITH_ASSERT_EQ((u_int)ZM_ShouldInteract(true, false, true, false, false, false),
		(u_int)ZM_INTERACT_REJECT_PLAYER_FROZEN,
		"a frozen player outranks the OK fallthrough");
}

// ---- Totality over the whole input space ------------------------------------

ZENITH_TEST(ZM_Interaction, Gate_IsTotalOverAllSixtyFourInputCombinations)
{
	u_int uOkCount = 0u;
	for (u_int uBits = 0u; uBits < 64u; ++uBits)
	{
		const bool bPressed         = (uBits & 1u)  != 0u;
		const bool bMenuOpen        = (uBits & 2u)  != 0u;
		const bool bOverworld       = (uBits & 4u)  != 0u;
		const bool bWarp            = (uBits & 8u)  != 0u;
		const bool bBattle          = (uBits & 16u) != 0u;
		const bool bMovementEnabled = (uBits & 32u) != 0u;

		// The precedence rule spelled out LONGHAND and independently of the function
		// under test -- deriving the expectation by calling ZM_ShouldInteract would
		// make this unit vacuous.
		ZM_INTERACT_REJECT eExpected = ZM_INTERACT_OK;
		if (!bPressed)
		{
			eExpected = ZM_INTERACT_REJECT_NO_INPUT_EDGE;
		}
		else if (bMenuOpen)
		{
			eExpected = ZM_INTERACT_REJECT_MENU_OPEN;
		}
		else if (!bOverworld)
		{
			eExpected = ZM_INTERACT_REJECT_NOT_OVERWORLD;
		}
		else if (bWarp)
		{
			eExpected = ZM_INTERACT_REJECT_WARP_IN_PROGRESS;
		}
		else if (bBattle)
		{
			eExpected = ZM_INTERACT_REJECT_BATTLE_TRANSITION;
		}
		else if (!bMovementEnabled)
		{
			eExpected = ZM_INTERACT_REJECT_PLAYER_FROZEN;
		}

		const ZM_INTERACT_REJECT eActual = ZM_ShouldInteract(
			bPressed, bMenuOpen, bOverworld, bWarp, bBattle, bMovementEnabled);
		ZENITH_ASSERT_EQ((u_int)eActual, (u_int)eExpected,
			"input combination %u must resolve to the precedence-rule reason", uBits);

		if (eActual == ZM_INTERACT_OK)
		{
			++uOkCount;
		}
	}

	ZENITH_ASSERT_EQ(uOkCount, 1u,
		"exactly ONE of the 64 combinations may permit an interaction");
}

// ---- ZM_InteractRejectName totality -----------------------------------------

ZENITH_TEST(ZM_Interaction, RejectName_IsTotalAndDistinct)
{
	ZENITH_ASSERT_GT((u_int)ZM_INTERACT_REJECT_COUNT, 0u,
		"the reject enum must have at least one enumerator to walk");

	// Hoisted so every IN-RANGE enumerator can be checked against it below. Without
	// that check this unit could not catch its own stated regression: appending an
	// enumerator and forgetting its switch arm makes ZM_InteractRejectName fall
	// through to "UNKNOWN", which is non-null, non-empty and distinct from all the
	// real names -- so every other assertion here would still pass while the later
	// windowed tests reported UNKNOWN and lied about why they failed.
	const char* szUnknown = ZM_InteractRejectName((ZM_INTERACT_REJECT)((u_int)ZM_INTERACT_REJECT_COUNT + 7u));
	ZENITH_ASSERT_NOT_NULL(szUnknown, "an out-of-range reject value must still name something");
	if (szUnknown != nullptr)
	{
		ZENITH_ASSERT_TRUE(szUnknown[0] != '\0', "the out-of-range name must be non-empty");
	}

	for (u_int u = 0u; u < (u_int)ZM_INTERACT_REJECT_COUNT; ++u)
	{
		const char* szName = ZM_InteractRejectName((ZM_INTERACT_REJECT)u);
		ZENITH_ASSERT_NOT_NULL(szName, "reject reason %u must have a name", u);
		if (szName == nullptr)
		{
			continue;
		}
		ZENITH_ASSERT_TRUE(szName[0] != '\0', "reject reason %u must have a NON-EMPTY name", u);

		if (szUnknown != nullptr)
		{
			ZENITH_ASSERT_TRUE(std::strcmp(szName, szUnknown) != 0,
				"reject reason %u fell through to the default arm -- it has no name of its own", u);
		}

		// Pairwise distinct: a duplicated name would make a later windowed failure
		// message name the wrong reason.
		for (u_int v = u + 1u; v < (u_int)ZM_INTERACT_REJECT_COUNT; ++v)
		{
			const char* szOther = ZM_InteractRejectName((ZM_INTERACT_REJECT)v);
			ZENITH_ASSERT_NOT_NULL(szOther, "reject reason %u must have a name", v);
			if (szOther == nullptr)
			{
				continue;
			}
			ZENITH_ASSERT_TRUE(std::strcmp(szName, szOther) != 0,
				"reject reasons %u and %u must not share a name", u, v);
		}
	}
}

// ---- Key-binding collision units --------------------------------------------
//
// Each walks the SAME named key-set constant the live reader walks, so rebinding a
// key onto the interact key fails here instead of double-firing at runtime. The
// non-empty assertion in front of every loop is deliberate: a loop bounded by a
// count that could be zero would pass vacuously.

ZENITH_TEST(ZM_Interaction, Keys_InteractDiffersFromConfirmKeys)
{
	ZENITH_ASSERT_GT(ZM_InputActions::uZM_CONFIRM_KEY_COUNT, 0u,
		"the confirm key set must be non-empty or the walk below is vacuous");
	for (u_int u = 0u; u < ZM_InputActions::uZM_CONFIRM_KEY_COUNT; ++u)
	{
		ZENITH_ASSERT_NE(ZM_InputActions::ZM_KEY_INTERACT, ZM_InputActions::ZM_CONFIRM_KEYS[u],
			"the interact key must not alias confirm key %u", u);
	}
}

ZENITH_TEST(ZM_Interaction, Keys_InteractDiffersFromCancelKeys)
{
	ZENITH_ASSERT_GT(ZM_InputActions::uZM_CANCEL_KEY_COUNT, 0u,
		"the cancel key set must be non-empty or the walk below is vacuous");
	for (u_int u = 0u; u < ZM_InputActions::uZM_CANCEL_KEY_COUNT; ++u)
	{
		ZENITH_ASSERT_NE(ZM_InputActions::ZM_KEY_INTERACT, ZM_InputActions::ZM_CANCEL_KEYS[u],
			"the interact key must not alias cancel key %u", u);
	}
}

ZENITH_TEST(ZM_Interaction, Keys_InteractDiffersFromMenuKeys)
{
	ZENITH_ASSERT_GT(ZM_InputActions::uZM_MENU_KEY_COUNT, 0u,
		"the menu key set must be non-empty or the walk below is vacuous");
	for (u_int u = 0u; u < ZM_InputActions::uZM_MENU_KEY_COUNT; ++u)
	{
		ZENITH_ASSERT_NE(ZM_InputActions::ZM_KEY_INTERACT, ZM_InputActions::ZM_MENU_KEYS[u],
			"the interact key must not alias menu key %u", u);
	}
}

ZENITH_TEST(ZM_Interaction, Keys_InteractDiffersFromRunKeys)
{
	// ReadRunHeld walks this set every overworld frame, so it is as live a binding
	// as confirm/cancel/menu even though it is a modifier.
	ZENITH_ASSERT_GT(ZM_InputActions::uZM_RUN_KEY_COUNT, 0u,
		"the run key set must be non-empty or the walk below is vacuous");
	for (u_int u = 0u; u < ZM_InputActions::uZM_RUN_KEY_COUNT; ++u)
	{
		ZENITH_ASSERT_NE(ZM_InputActions::ZM_KEY_INTERACT, ZM_InputActions::ZM_RUN_KEYS[u],
			"the interact key must not alias run key %u", u);
	}
}

ZENITH_TEST(ZM_Interaction, Keys_InteractDiffersFromEveryMovementKey)
{
	// Eight bindings (WASD + the four arrows): interacting must never also step.
	ZENITH_ASSERT_GT(ZM_InputActions::uZM_MOVE_KEY_COUNT, 0u,
		"the movement key set must be non-empty or the walk below is vacuous");
	for (u_int u = 0u; u < ZM_InputActions::uZM_MOVE_KEY_COUNT; ++u)
	{
		ZENITH_ASSERT_NE(ZM_InputActions::ZM_KEY_INTERACT, ZM_InputActions::ZM_MOVE_KEYS[u],
			"the interact key must not alias movement key %u", u);
	}
}

// =============================================================================
// SC2 -- ZM_PickInteractTarget / ZM_ForwardFromRotation
//
// Every fixture below puts the player at the WORLD ORIGIN looking down +Z, so a
// probe's raw position IS its offset from the player and each unit reads as a
// picture. All of it is pure arithmetic: no scene, no entity, no simulator.
// =============================================================================

namespace
{
	constexpr float fTEST_EPSILON = 0.001f;

	// A distinctive seed for uBestIndexOut, so "the picker never wrote it" is
	// distinguishable from "the picker wrote the sentinel".
	constexpr u_int uTEST_INDEX_POISON = 4242u;

	// The SHIPPED tuning. Boundary units derive their probe positions FROM this, so
	// they move with the constants instead of hard-coding 2.5 / 0.35 / 2.0.
	ZM_InteractTuning MakeLiveTuning()
	{
		ZM_InteractTuning xTuning;
		xTuning.m_fMaxDistance  = fZM_INTERACT_MAX_DISTANCE;
		xTuning.m_fMinFacingDot = fZM_INTERACT_MIN_FACING_DOT;
		xTuning.m_fMaxVertical  = fZM_INTERACT_MAX_VERTICAL;
		return xTuning;
	}

	ZM_InteractProbe MakeProbe(float fX, float fY, float fZ,
		float fRadius = 0.0f, bool bEnabled = true)
	{
		ZM_InteractProbe xProbe;
		xProbe.m_xPosition = Zenith_Maths::Vector3(fX, fY, fZ);
		xProbe.m_fRadius = fRadius;
		xProbe.m_bEnabled = bEnabled;
		return xProbe;
	}

	ZM_InteractOrigin MakeOriginLookingAlongPlusZ()
	{
		ZM_InteractOrigin xOrigin;
		xOrigin.m_xPosition = Zenith_Maths::Vector3(0.0f);
		xOrigin.m_xForward = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		return xOrigin;
	}

	// A 3-4-5 triangle scaled by 1/4: offset (0.75, 0, 1.0) has XZ length EXACTLY
	// 1.25 and therefore a facing dot of exactly 1.0 / 1.25 == 0.8f against +Z.
	// Every term is a dyadic rational, so the picker's dot and this constant are
	// bit-identical and the inclusive-boundary units cannot flake.
	constexpr float fBOUNDARY_PROBE_X = 0.75f;
	constexpr float fBOUNDARY_PROBE_Z = 1.0f;
	constexpr float fBOUNDARY_FACING_DOT = 0.8f;
}

// ---- Empty / disabled sets ---------------------------------------------------

ZENITH_TEST(ZM_Interaction, Pick_EmptySetReturnsNoCandidate)
{
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(nullptr, 0u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_NO_CANDIDATE,
		"a null / empty probe set reports NO_CANDIDATE instead of reading past the array");
	ZENITH_ASSERT_EQ(uBest, 0u, "with zero probes the sentinel index is zero (== uCount)");

	// The same answer with a real, non-null array of length zero.
	const ZM_InteractProbe axProbes[1] = { MakeProbe(0.0f, 0.0f, 1.0f) };
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 0u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_NO_CANDIDATE,
		"uCount 0 must be honoured even when the array behind it holds a perfect probe");
	ZENITH_ASSERT_EQ(uBest, 0u, "uCount 0 -> sentinel index 0");
}

ZENITH_TEST(ZM_Interaction, Pick_AllDisabledReturnsNoCandidate)
{
	// Both are perfectly placed: only m_bEnabled keeps them out.
	const ZM_InteractProbe axProbes[2] =
	{
		MakeProbe(0.0f, 0.0f, 1.0f, 0.0f, /* enabled */ false),
		MakeProbe(0.0f, 0.0f, 1.5f, 0.0f, /* enabled */ false),
	};
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_NO_CANDIDATE,
		"a scene of parked NPCs is NO_CANDIDATE -- disabled probes are not candidates");
	ZENITH_ASSERT_EQ(uBest, 2u, "no winner -> the unreachable index uCount");
}

// ---- The happy path and the two world gates ---------------------------------

ZENITH_TEST(ZM_Interaction, Pick_SingleInRangeFacedTargetWins)
{
	const ZM_InteractProbe axProbes[1] = { MakeProbe(0.0f, 0.0f, 1.5f) };
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_OK,
		"an enabled probe 1.5m straight ahead is interactable");
	ZENITH_ASSERT_EQ(uBest, 0u, "the only candidate is the winner");
}

ZENITH_TEST(ZM_Interaction, Pick_OutOfRangeReturnsOutOfRange)
{
	const ZM_InteractProbe axProbes[1] = { MakeProbe(0.0f, 0.0f, 10.0f) };
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_OUT_OF_RANGE,
		"a probe 10m away is NOT talkable -- the range check is what stops "
		"interacting from across the map");
	ZENITH_ASSERT_EQ(uBest, 1u, "no winner -> the unreachable index uCount");
}

ZENITH_TEST(ZM_Interaction, Pick_BehindPlayerReturnsNotFacing)
{
	// Directly BEHIND: in range, in band, dot == -1.
	const ZM_InteractProbe axProbes[1] = { MakeProbe(0.0f, 0.0f, -1.5f) };
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_NOT_FACING,
		"a probe behind the player fails the cone -- an inverted cone would accept it");
	ZENITH_ASSERT_EQ(uBest, 1u, "no winner -> the unreachable index uCount");
}

// ---- Inclusive boundaries ----------------------------------------------------

ZENITH_TEST(ZM_Interaction, Pick_ExactlyAtMaxDistanceAccepted)
{
	const ZM_InteractTuning xTuning = MakeLiveTuning();

	// Built FROM the tuning, so raising fZM_INTERACT_MAX_DISTANCE moves the fixture.
	const ZM_InteractProbe axProbes[1] = { MakeProbe(0.0f, 0.0f, xTuning.m_fMaxDistance) };
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_OK,
		"the distance test is INCLUSIVE: a probe at exactly the max distance is reachable");
	ZENITH_ASSERT_EQ(uBest, 0u, "the boundary probe is the winner");

	// And with a reach bonus, exactly at max + radius.
	const float fRADIUS = 1.0f;
	const ZM_InteractProbe axRadiusProbes[1] =
	{
		MakeProbe(0.0f, 0.0f, xTuning.m_fMaxDistance + fRADIUS, fRADIUS)
	};
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axRadiusProbes, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_OK,
		"inclusive at exactly max distance PLUS the probe's own radius");
	ZENITH_ASSERT_EQ(uBest, 0u, "the boundary-plus-radius probe is the winner");
}

ZENITH_TEST(ZM_Interaction, Pick_JustBeyondMaxDistanceRejected)
{
	const ZM_InteractTuning xTuning = MakeLiveTuning();
	const ZM_InteractProbe axProbes[1] =
	{
		MakeProbe(0.0f, 0.0f, xTuning.m_fMaxDistance + 0.01f)
	};
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_REJECT_OUT_OF_RANGE,
		"1cm past the max distance is out of range -- the boundary must be a real edge");
	ZENITH_ASSERT_EQ(uBest, 1u, "no winner -> the unreachable index uCount");
}

ZENITH_TEST(ZM_Interaction, Pick_ExactlyAtMinFacingDotAccepted)
{
	// The shipped cone must stay WIDER than this fixture's boundary, otherwise the
	// probe below would be rejected on the shipped tuning and this unit would be
	// testing something other than the boundary it claims to test.
	ZENITH_ASSERT_LT(fZM_INTERACT_MIN_FACING_DOT, fBOUNDARY_FACING_DOT,
		"the shipped facing cone must be wider than the boundary fixture's cone");

	ZM_InteractTuning xTuning = MakeLiveTuning();
	xTuning.m_fMinFacingDot = fBOUNDARY_FACING_DOT;

	const ZM_InteractProbe axProbes[1] =
	{
		MakeProbe(fBOUNDARY_PROBE_X, 0.0f, fBOUNDARY_PROBE_Z)
	};
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_OK,
		"the cone test is INCLUSIVE: a probe exactly on the cone edge is faced");
	ZENITH_ASSERT_EQ(uBest, 0u, "the cone-edge probe is the winner");
}

ZENITH_TEST(ZM_Interaction, Pick_JustBelowMinFacingDotRejected)
{
	// Same probe, cone tightened a hair past its dot.
	ZM_InteractTuning xTuning = MakeLiveTuning();
	xTuning.m_fMinFacingDot = fBOUNDARY_FACING_DOT + 0.001f;

	const ZM_InteractProbe axProbes[1] =
	{
		MakeProbe(fBOUNDARY_PROBE_X, 0.0f, fBOUNDARY_PROBE_Z)
	};
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_REJECT_NOT_FACING,
		"a probe a hair outside the cone is NOT faced -- the boundary must be a real edge");
	ZENITH_ASSERT_EQ(uBest, 1u, "no winner -> the unreachable index uCount");
}

// ---- Nearest-wins and tie-breaking ------------------------------------------

ZENITH_TEST(ZM_Interaction, Pick_NearestOfTwoWins)
{
	const ZM_InteractProbe axProbes[2] =
	{
		MakeProbe(0.0f, 0.0f, 2.0f),
		MakeProbe(0.0f, 0.0f, 1.0f),
	};
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_OK, "both probes are valid candidates");
	ZENITH_ASSERT_EQ(uBest, 1u,
		"the NEARER probe wins even though it is later in the array");
}

ZENITH_TEST(ZM_Interaction, Pick_NearestWinsRegardlessOfArrayOrder)
{
	// The index-bias test: the SAME two probes in both orders must select the same
	// probe BY IDENTITY (its index of course changes with the ordering).
	const ZM_InteractProbe xNear = MakeProbe(0.0f, 0.0f, 1.0f);
	const ZM_InteractProbe xFar  = MakeProbe(0.5f, 0.0f, 2.0f);

	const ZM_InteractProbe axForward[2] = { xNear, xFar };
	u_int uForwardBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axForward, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uForwardBest),
		(u_int)ZM_INTERACT_OK, "near-then-far ordering yields a winner");
	ZENITH_ASSERT_EQ(uForwardBest, 0u, "near-then-far: the nearer probe is at index 0");

	const ZM_InteractProbe axReversed[2] = { xFar, xNear };
	u_int uReversedBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axReversed, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uReversedBest),
		(u_int)ZM_INTERACT_OK, "far-then-near ordering yields a winner");
	ZENITH_ASSERT_EQ(uReversedBest, 1u, "far-then-near: the nearer probe is at index 1");

	// Stated as identity, which is the property that actually matters.
	if ((uForwardBest < 2u) && (uReversedBest < 2u))
	{
		ZENITH_ASSERT_NEAR_VEC3(axForward[uForwardBest].m_xPosition,
			axReversed[uReversedBest].m_xPosition, fTEST_EPSILON,
			"reversing the array must select the SAME probe");
		ZENITH_ASSERT_NEAR_VEC3(axForward[uForwardBest].m_xPosition,
			xNear.m_xPosition, fTEST_EPSILON,
			"and that probe must be the nearer one");
	}
}

ZENITH_TEST(ZM_Interaction, Pick_TiedDistanceBreaksToLowestIndex)
{
	// Mirror-image probes: identical XZ distance (1.25) and identical facing dot.
	const ZM_InteractProbe xRight = MakeProbe(fBOUNDARY_PROBE_X, 0.0f, fBOUNDARY_PROBE_Z);
	const ZM_InteractProbe xLeft  = MakeProbe(-fBOUNDARY_PROBE_X, 0.0f, fBOUNDARY_PROBE_Z);

	const ZM_InteractProbe axRightFirst[2] = { xRight, xLeft };
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axRightFirst, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_OK, "two equidistant faced probes yield a winner");
	ZENITH_ASSERT_EQ(uBest, 0u, "a tie breaks to the LOWEST index, not the last seen");

	// Swapped: still index 0, which is now the OTHER probe -- i.e. the tie-break is
	// positional and deterministic, not a property of one particular probe.
	const ZM_InteractProbe axLeftFirst[2] = { xLeft, xRight };
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axLeftFirst, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_OK, "the swapped tie also yields a winner");
	ZENITH_ASSERT_EQ(uBest, 0u, "swapping the tied pair still selects index 0");
}

// ---- Disabled probes ---------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Pick_DisabledProbeIsSkipped)
{
	// The disabled probe is the PERFECT candidate and the enabled one is hopeless:
	// if disabled probes were counted this would return OK, and if they merely
	// raised the near-miss stage the reason would be wrong too.
	const ZM_InteractProbe axProbes[2] =
	{
		MakeProbe(0.0f, 0.0f, 1.0f, 0.0f, /* enabled */ false),
		MakeProbe(0.0f, 0.0f, 20.0f),
	};
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_OUT_OF_RANGE,
		"a parked NPC must not absorb the interaction -- only the far enabled probe counts");
	ZENITH_ASSERT_EQ(uBest, 2u, "no winner -> the unreachable index uCount");
}

ZENITH_TEST(ZM_Interaction, Pick_DisabledNearProbeDoesNotMaskFarEnabledOne)
{
	// A disabled probe FIRST in the array must not short-circuit the scan.
	const ZM_InteractProbe axProbes[2] =
	{
		MakeProbe(0.0f, 0.0f, 0.5f, 0.0f, /* enabled */ false),
		MakeProbe(0.0f, 0.0f, 2.0f),
	};
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_OK,
		"the scan continues past a disabled probe");
	ZENITH_ASSERT_EQ(uBest, 1u, "the enabled probe behind it is the winner");
}

ZENITH_TEST(ZM_Interaction, Pick_ProbeRadiusExtendsReach)
{
	const ZM_InteractTuning xTuning = MakeLiveTuning();
	const float fBEYOND = xTuning.m_fMaxDistance + 1.0f;

	const ZM_InteractProbe axNoRadius[1] = { MakeProbe(0.0f, 0.0f, fBEYOND) };
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axNoRadius, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_REJECT_OUT_OF_RANGE,
		"1m past the max distance with NO reach bonus is out of range");
	ZENITH_ASSERT_EQ(uBest, 1u, "no winner -> the unreachable index uCount");

	// The identical position, now with a reach bonus that covers the gap.
	const ZM_InteractProbe axWithRadius[1] = { MakeProbe(0.0f, 0.0f, fBEYOND, 1.5f) };
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axWithRadius, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_OK,
		"a per-NPC radius extends reach -- ignoring m_fRadius makes big interactables unusable");
	ZENITH_ASSERT_EQ(uBest, 0u, "the radius-extended probe is the winner");
}

// ---- The vertical band -------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Pick_VerticalBandRejectsRoofTarget)
{
	const ZM_InteractTuning xTuning = MakeLiveTuning();
	const ZM_InteractProbe axProbes[1] =
	{
		MakeProbe(0.0f, xTuning.m_fMaxVertical + 1.0f, 1.0f)
	};
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_REJECT_OUT_OF_VERTICAL_BAND,
		"an NPC on the roof directly above is NOT talkable -- ignoring Y talks through floors");
	ZENITH_ASSERT_EQ(uBest, 1u, "no winner -> the unreachable index uCount");
}

ZENITH_TEST(ZM_Interaction, Pick_VerticalBandAcceptsSmallStep)
{
	const ZM_InteractTuning xTuning = MakeLiveTuning();

	// Terrain float / a kerb: well inside the band, and it must stay talkable.
	const ZM_InteractProbe axStep[1] = { MakeProbe(0.0f, 0.5f, 1.0f) };
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axStep, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_OK,
		"half a metre of height difference must not break interaction");
	ZENITH_ASSERT_EQ(uBest, 0u, "the stepped-up probe is the winner");

	// Inclusive at exactly the band edge, and symmetric below the player too.
	const ZM_InteractProbe axEdge[1] = { MakeProbe(0.0f, -xTuning.m_fMaxVertical, 1.0f) };
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axEdge, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_OK,
		"the band is INCLUSIVE and absolute: exactly maxVertical BELOW still counts");
	ZENITH_ASSERT_EQ(uBest, 0u, "the band-edge probe is the winner");
}

ZENITH_TEST(ZM_Interaction, Pick_DistanceIgnoresYComponent)
{
	const ZM_InteractTuning xTuning = MakeLiveTuning();

	// XZ distance 2.0 (inside 2.5) but a 1.9m drop, so the 3D distance is ~2.76 --
	// a distance test that kept Y would reject this sunk NPC.
	const ZM_InteractProbe axProbes[1] = { MakeProbe(0.0f, -1.9f, 2.0f) };
	ZENITH_ASSERT_GT(glm::length(axProbes[0].m_xPosition), xTuning.m_fMaxDistance,
		"the fixture is only adversarial if its 3D distance really does exceed the max");

	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u,
		MakeOriginLookingAlongPlusZ(), xTuning, uBest),
		(u_int)ZM_INTERACT_OK,
		"range is measured in XZ ONLY -- a sunk NPC inside the band stays reachable");
	ZENITH_ASSERT_EQ(uBest, 0u, "the sunk probe is the winner");
}

// ---- Degenerate origin -------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Pick_ZeroForwardReturnsDegenerateOrigin)
{
	// A flawless candidate, so only the origin can be the reason for the reject.
	const ZM_InteractProbe axProbes[1] = { MakeProbe(0.0f, 0.0f, 1.0f) };

	ZM_InteractOrigin xOrigin = MakeOriginLookingAlongPlusZ();
	xOrigin.m_xForward = Zenith_Maths::Vector3(0.0f);
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u, xOrigin, MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_DEGENERATE_ORIGIN,
		"an exactly-zero forward is degenerate -- normalising it would NaN the dot product");
	ZENITH_ASSERT_EQ(uBest, 1u, "no winner -> the unreachable index uCount");

	// Straight up and straight down both flatten to nothing in XZ.
	xOrigin.m_xForward = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u, xOrigin, MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_DEGENERATE_ORIGIN,
		"a straight-up facing has no XZ direction to test the cone against");

	xOrigin.m_xForward = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u, xOrigin, MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_DEGENERATE_ORIGIN,
		"a straight-down facing is degenerate for the same reason");
}

ZENITH_TEST(ZM_Interaction, Pick_CoincidentProbeIsInRangeAndFacedAndWins)
{
	// Standing ON an interactable: there is no direction to the candidate, so the
	// documented rule is IN RANGE + FACED, and at distance zero it outranks the
	// perfectly good probe ahead of the player.
	const ZM_InteractProbe axProbes[2] =
	{
		MakeProbe(0.0f, 0.0f, 1.0f),
		MakeProbe(0.0f, 0.0f, 0.0f),
	};
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_OK,
		"a probe coincident with the player is interactable, not a division by zero");
	ZENITH_ASSERT_EQ(uBest, 1u, "distance zero is the smallest distance, so it wins");
}

// ---- Most-specific-last reject precedence -----------------------------------

ZENITH_TEST(ZM_Interaction, Pick_InRangeButWrongYReportsBandNotOutOfRange)
{
	// One probe in XZ range but three floors up, one probe simply far away. The
	// BAND reason is the more specific near-miss and must win the report.
	const ZM_InteractProbe axProbes[2] =
	{
		MakeProbe(0.0f, 6.0f, 1.0f),
		MakeProbe(0.0f, 0.0f, 30.0f),
	};
	u_int uBest = uTEST_INDEX_POISON;
	const ZM_INTERACT_REJECT eReject = ZM_PickInteractTarget(axProbes, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest);
	ZENITH_ASSERT_EQ((u_int)eReject, (u_int)ZM_INTERACT_REJECT_OUT_OF_VERTICAL_BAND,
		"something passed the distance test, so the reason is the BAND, not the range");
	ZENITH_ASSERT_NE((u_int)eReject, (u_int)ZM_INTERACT_REJECT_OUT_OF_RANGE,
		"reporting OUT_OF_RANGE here would tell a walk-up test the player is far away "
		"when they are actually standing right underneath");
	ZENITH_ASSERT_EQ(uBest, 2u, "no winner -> the unreachable index uCount");
}

ZENITH_TEST(ZM_Interaction, Pick_BandMissBetweenTwoRangeMissesReportsBand)
{
	// The BEST near-miss must be reported, not the LAST one walked: the band miss
	// sits in the middle, with out-of-range probes on either side of it.
	const ZM_InteractProbe axProbes[3] =
	{
		MakeProbe(0.0f, 0.0f, 20.0f),
		MakeProbe(0.0f, 5.0f, 1.0f),
		MakeProbe(0.0f, 0.0f, 40.0f),
	};
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 3u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_OUT_OF_VERTICAL_BAND,
		"the furthest stage reached wins the report even when a weaker miss is walked last");
	ZENITH_ASSERT_EQ(uBest, 3u, "no winner -> the unreachable index uCount");

	// And the NOT_FACING rung, one step more specific again: this probe clears both
	// the distance test and the band, so only the cone can reject it.
	const ZM_InteractProbe axFacingProbes[2] =
	{
		MakeProbe(0.0f, 0.0f, -1.0f),
		MakeProbe(0.0f, 0.0f, 40.0f),
	};
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axFacingProbes, 2u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_NOT_FACING,
		"distance + band passed, cone failed -> NOT_FACING outranks the far probe's OUT_OF_RANGE");
	ZENITH_ASSERT_EQ(uBest, 2u, "no winner -> the unreachable index uCount");
}

ZENITH_TEST(ZM_Interaction, Pick_BestIndexOutIsUnreachableOnReject)
{
	// One case per reject flavour. A caller that ignores the return value must never
	// be handed index 0, so uBestIndexOut is uCount on every single one of them.
	const ZM_InteractOrigin xOrigin = MakeOriginLookingAlongPlusZ();
	const ZM_InteractTuning xTuning = MakeLiveTuning();

	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(nullptr, 0u, xOrigin, xTuning, uBest),
		(u_int)ZM_INTERACT_REJECT_NO_CANDIDATE, "flavour 1: the empty set");
	ZENITH_ASSERT_EQ(uBest, 0u, "NO_CANDIDATE (empty) -> uBestIndexOut == uCount");

	const ZM_InteractProbe axDisabled[2] =
	{
		MakeProbe(0.0f, 0.0f, 1.0f, 0.0f, false),
		MakeProbe(0.0f, 0.0f, 1.5f, 0.0f, false),
	};
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axDisabled, 2u, xOrigin, xTuning, uBest),
		(u_int)ZM_INTERACT_REJECT_NO_CANDIDATE, "flavour 2: every probe disabled");
	ZENITH_ASSERT_EQ(uBest, 2u, "NO_CANDIDATE (all disabled) -> uBestIndexOut == uCount");

	const ZM_InteractProbe axFar[1] = { MakeProbe(0.0f, 0.0f, 50.0f) };
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axFar, 1u, xOrigin, xTuning, uBest),
		(u_int)ZM_INTERACT_REJECT_OUT_OF_RANGE, "flavour 3: out of range");
	ZENITH_ASSERT_EQ(uBest, 1u, "OUT_OF_RANGE -> uBestIndexOut == uCount");

	const ZM_InteractProbe axHigh[1] = { MakeProbe(0.0f, 9.0f, 1.0f) };
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axHigh, 1u, xOrigin, xTuning, uBest),
		(u_int)ZM_INTERACT_REJECT_OUT_OF_VERTICAL_BAND, "flavour 4: out of the vertical band");
	ZENITH_ASSERT_EQ(uBest, 1u, "OUT_OF_VERTICAL_BAND -> uBestIndexOut == uCount");

	const ZM_InteractProbe axBehind[1] = { MakeProbe(0.0f, 0.0f, -1.0f) };
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axBehind, 1u, xOrigin, xTuning, uBest),
		(u_int)ZM_INTERACT_REJECT_NOT_FACING, "flavour 5: not faced");
	ZENITH_ASSERT_EQ(uBest, 1u, "NOT_FACING -> uBestIndexOut == uCount");

	ZM_InteractOrigin xDegenerateOrigin = MakeOriginLookingAlongPlusZ();
	xDegenerateOrigin.m_xForward = Zenith_Maths::Vector3(0.0f);
	const ZM_InteractProbe axPerfect[1] = { MakeProbe(0.0f, 0.0f, 1.0f) };
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axPerfect, 1u, xDegenerateOrigin, xTuning, uBest),
		(u_int)ZM_INTERACT_REJECT_DEGENERATE_ORIGIN, "flavour 6: degenerate origin");
	ZENITH_ASSERT_EQ(uBest, 1u, "DEGENERATE_ORIGIN -> uBestIndexOut == uCount");
}

// ---- ZM_ForwardFromRotation --------------------------------------------------

ZENITH_TEST(ZM_Interaction, Forward_IdentityRotationIsPlusZ)
{
	// glm::quat's scalar-first constructor: (w, x, y, z).
	const Zenith_Maths::Quat xIdentity(1.0f, 0.0f, 0.0f, 0.0f);
	ZENITH_ASSERT_NEAR_VEC3(ZM_ForwardFromRotation(xIdentity),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), fTEST_EPSILON,
		"the unrotated facing is +Z -- the whole picker's cone is measured from this convention");
}

ZENITH_TEST(ZM_Interaction, Forward_NinetyDegreeYawIsPlusX)
{
	const Zenith_Maths::Quat xYaw90 =
		Zenith_Maths::AngleAxis(glm::radians(90.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	ZENITH_ASSERT_NEAR_VEC3(ZM_ForwardFromRotation(xYaw90),
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), fTEST_EPSILON,
		"a +90 degree yaw faces +X -- a flipped handedness would give -X");
}

ZENITH_TEST(ZM_Interaction, Forward_OneEightyYawIsMinusZ)
{
	// THE regression unit. Rewriting ZM_ForwardFromRotation in terms of
	// glm::eulerAngles(quat).y collapses once the rotation is more than 90 degrees
	// off +Z (it cost RenderTest's tennis AI a full debugging cycle), and a
	// half-turn is exactly that case. The FULL vector is asserted, not just a sign,
	// so a decomposition that yields +Z, +X or -X all fail here.
	const Zenith_Maths::Quat xYaw180 =
		Zenith_Maths::AngleAxis(glm::radians(180.0f), Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	const Zenith_Maths::Vector3 xForward = ZM_ForwardFromRotation(xYaw180);
	ZENITH_ASSERT_NEAR_VEC3(xForward, Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f), fTEST_EPSILON,
		"a half-turn faces exactly -Z");
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xForward), 1.0f, fTEST_EPSILON,
		"and it is still a unit vector, so the cone dot stays in [-1, 1]");
}

// ---- The two contracts the picker DOCUMENTS but nothing else exercises -------

ZENITH_TEST(ZM_Interaction, Pick_UnnormalisedPitchedForwardStillConesCorrectly)
{
	// Every other Pick_* fixture looks along an already-flat, already-unit +Z, so
	// the picker's flatten/normalise is only ever exercised as the IDENTITY: delete
	// it and use m_xForward raw, and they all still pass. In the live game the
	// forward comes off a transform or camera, so it carries pitch and is not unit
	// -- with (0, 8, 4) a raw dot is ~4x too large and the cone is effectively OFF,
	// making an NPC 80 degrees off to the side talkable. This pins the header's
	// "need NOT be normalised and need NOT be XZ-flat" contract.
	ZM_InteractOrigin xOrigin = MakeOriginLookingAlongPlusZ();
	xOrigin.m_xForward = Zenith_Maths::Vector3(0.0f, 8.0f, 4.0f);   // flattens to +Z, 4x over-long

	const ZM_InteractProbe axProbes[] =
	{
		MakeProbe(fBOUNDARY_PROBE_X, 0.0f, fBOUNDARY_PROBE_Z)
	};

	// Just INSIDE the cone: the pitched, over-long forward must still resolve to the
	// same unit +Z every other unit uses, so this is accepted.
	ZM_InteractTuning xInside = MakeLiveTuning();
	xInside.m_fMinFacingDot = fBOUNDARY_FACING_DOT;
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u, xOrigin, xInside, uBest),
		(u_int)ZM_INTERACT_OK,
		"a pitched, un-normalised forward must flatten+normalise to the same facing");
	ZENITH_ASSERT_EQ(uBest, 0u, "and pick the only probe");

	// Just OUTSIDE the cone. A picker that skipped the normalise would compute a dot
	// of roughly 3.2 here, sail past the threshold, and wrongly return OK.
	ZM_InteractTuning xOutside = MakeLiveTuning();
	xOutside.m_fMinFacingDot = fBOUNDARY_FACING_DOT + 0.001f;
	uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u, xOrigin, xOutside, uBest),
		(u_int)ZM_INTERACT_REJECT_NOT_FACING,
		"an un-normalised forward must NOT inflate the dot past the cone threshold");
	ZENITH_ASSERT_EQ(uBest, 1u, "and the index stays the unreachable sentinel");
}

ZENITH_TEST(ZM_Interaction, Pick_NegativeRadiusIsNeverInRange)
{
	// The header promises a negative radius never shrinks reach below zero. The
	// guard enforcing it is five lines with no other coverage -- and deleting them
	// INVERTS the behaviour rather than merely losing it: reach becomes -7.5, and
	// the squared comparison against 56.25 puts this probe in range out to 7.5 m.
	const ZM_InteractProbe axProbes[] =
	{
		MakeProbe(0.0f, 0.0f, 5.0f, -10.0f)
	};

	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_EQ((u_int)ZM_PickInteractTarget(axProbes, 1u,
		MakeOriginLookingAlongPlusZ(), MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_REJECT_OUT_OF_RANGE,
		"a negative radius must never square back into a LARGER reach");
	ZENITH_ASSERT_EQ(uBest, 1u, "and nothing is picked");
}

ZENITH_TEST(ZM_Interaction, Forward_StraightUpFlattensToZero)
{
	// A -90 degree pitch about +X maps +Z onto +Y: nothing survives the flatten, so
	// the answer must degrade to a clean zero rather than NaN its way into the picker.
	const Zenith_Maths::Quat xPitchUp =
		Zenith_Maths::AngleAxis(glm::radians(-90.0f), Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	const Zenith_Maths::Vector3 xForward = ZM_ForwardFromRotation(xPitchUp);
	ZENITH_ASSERT_NEAR_VEC3(xForward, Zenith_Maths::Vector3(0.0f), fTEST_EPSILON,
		"a straight-up facing flattens to the zero vector");

	// The two assertions above are NaN-PERMISSIVE and cannot carry this unit on
	// their own: both AssertNearVec3 and AssertEqFloat fail only when a difference
	// EXCEEDS the epsilon, and every comparison against NaN is false. Drop the
	// degenerate early-out from ZM_FlattenXZ and this becomes (NaN, 0, NaN) with a
	// NaN length -- which would sail through them. These two DO fail on NaN:
	// `NaN < eps` is false, and `NaN == 0.0f` is false.
	ZENITH_ASSERT_LT(glm::length(xForward), fTEST_EPSILON,
		"length below epsilon -- and NOT NaN, which fails this strict comparison");
	ZENITH_ASSERT_TRUE((xForward.x == 0.0f) && (xForward.y == 0.0f) && (xForward.z == 0.0f),
		"EXACTLY zero, not NaN -- a NaN would compare false against every threshold "
		"and silently accept whatever probe came first");
}

// ---- The EXTRACTED angular primitive, pinned directly (S7 item 3 SC3) --------
//
// ZM_IsFacingXZ was the picker's PRIVATE inner block until SC3 lifted it out so
// the trainer sight predicate could call the SAME cone. The 44 units above are
// the behaviour-preservation net for that extraction -- they exercise it through
// ZM_PickInteractTarget and are deliberately UNEDITED. The six units below pin
// the primitive at its OWN surface, including the two guards the picker can
// never reach and the one arm that is a preserved quirk rather than an ideal.

ZENITH_TEST(ZM_Interaction, Facing_ExactlyAtMinDotIsFaced)
{
	// The same dyadic 3-4-5 fixture Pick_ExactlyAtMinFacingDotAccepted uses, called
	// DIRECTLY: the primitive's edge must be inclusive exactly where the picker's is,
	// which is the whole point of there now being one cone instead of two.
	ZENITH_ASSERT_TRUE(ZM_IsFacingXZ(Zenith_Maths::Vector3(0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(fBOUNDARY_PROBE_X, 0.0f, fBOUNDARY_PROBE_Z),
		fBOUNDARY_FACING_DOT),
		"the cone edge is INCLUSIVE: a target exactly at the threshold dot is faced");
}

ZENITH_TEST(ZM_Interaction, Facing_JustBelowMinDotIsNotFaced)
{
	// The reject arm of the pair -- without it the accept arm would pass on a
	// primitive that returned true unconditionally.
	ZENITH_ASSERT_FALSE(ZM_IsFacingXZ(Zenith_Maths::Vector3(0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(fBOUNDARY_PROBE_X, 0.0f, fBOUNDARY_PROBE_Z),
		fBOUNDARY_FACING_DOT + 0.001f),
		"one thousandth outside the cone is OUTSIDE the cone");
}

ZENITH_TEST(ZM_Interaction, Facing_CoincidentTargetIsFaced)
{
	// The carve-out the picker's header documents -- "you are standing on it" --
	// now lives in the primitive, so it is pinned HERE directly rather than only
	// through Pick_CoincidentProbeIsInRangeAndFacedAndWins.
	ZENITH_ASSERT_TRUE(ZM_IsFacingXZ(Zenith_Maths::Vector3(3.0f, -2.0f, 7.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(3.0f, -2.0f, 7.0f),
		fZM_INTERACT_MIN_FACING_DOT),
		"a coincident target has no direction to test, so it is faced rather than "
		"divided by zero into a NaN");
}

ZENITH_TEST(ZM_Interaction, Facing_DegenerateForwardIsNotFaced)
{
	// The all-directions threshold is load-bearing: with it the cone comparison
	// cannot be what rejects, so the fail-closed forward guard is the ONLY thing
	// under test. That guard is UNREACHABLE from ZM_PickInteractTarget, which
	// returns DEGENERATE_ORIGIN before it walks a single probe -- so adding it
	// cannot have changed one picker answer, and this unit is its only coverage.
	ZENITH_ASSERT_FALSE(ZM_IsFacingXZ(Zenith_Maths::Vector3(0.0f),
		Zenith_Maths::Vector3(0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), -1.5f),
		"no facing means nothing is faced: a zero forward FAILS CLOSED");

	// CONTROL: the same all-directions threshold with a real forward IS faced.
	ZENITH_ASSERT_TRUE(ZM_IsFacingXZ(Zenith_Maths::Vector3(0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f), -1.5f),
		"...and the same threshold with a real forward faces everything, so this "
		"unit is not passing merely because everything is rejected");
}

ZENITH_TEST(ZM_Interaction, Facing_IgnoresYSeparation)
{
	// The primitive is PLANAR. The vertical band is the picker's separate job, so
	// folding Y into the dot here would silently change what that band means.
	const Zenith_Maths::Vector3 xForward(0.0f, 0.0f, 1.0f);
	const bool bLow  = ZM_IsFacingXZ(Zenith_Maths::Vector3(0.0f), xForward,
		Zenith_Maths::Vector3(0.0f, -25.0f, 2.0f), fZM_INTERACT_MIN_FACING_DOT);
	const bool bHigh = ZM_IsFacingXZ(Zenith_Maths::Vector3(0.0f), xForward,
		Zenith_Maths::Vector3(0.0f,  25.0f, 2.0f), fZM_INTERACT_MIN_FACING_DOT);

	ZENITH_ASSERT_EQ(bLow, bHigh,
		"two targets identical in XZ but fifty units apart in Y must be faced identically");
	ZENITH_ASSERT_TRUE(bLow,
		"and both ARE faced -- a unit where both answers are false would agree vacuously");
}

ZENITH_TEST(ZM_Interaction, Facing_NonFiniteSeparationIsTreatedAsCoincident)
{
	// A DOCUMENTED QUIRK, pinned because it is today's behaviour, NOT because it is
	// the ideal. A NaN separation makes the squared distance NaN, `NaN > epsilon` is
	// false, and the primitive takes the coincident arm and answers TRUE. That is
	// verbatim what the fused block inside ZM_PickInteractTarget did before SC3
	// lifted it out, and preserving it bit-for-bit is why the branch polarity is
	// `if (distSq > eps) { ...dot... } return true;` rather than the tidier inverse.
	//
	// The fail-CLOSED layer for non-finite input lives ONE LEVEL UP, in
	// ZM_IsTargetInTrainerSight's finite guard, pinned by Sight_NonFiniteInputFailsClosed.
	// Whoever changes this branch changes ZM_PickInteractTarget's answer on NaN
	// input, so they have to edit this unit and read this note first.
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	ZENITH_ASSERT_TRUE(ZM_IsFacingXZ(Zenith_Maths::Vector3(0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(fNaN, 0.0f, 4.0f), fZM_INTERACT_MIN_FACING_DOT),
		"a non-finite separation lands in the coincident arm and answers TRUE -- the "
		"preserved behaviour of the picker's original fused cone block");
}

ZENITH_TEST(ZM_Interaction, Facing_NonFiniteOperandFailsClosed)
{
	// A CONTRACT pin: however the cone comparison is spelled, a non-finite operand
	// must FAIL CLOSED -- face nothing at the primitive's own surface, and hand back
	// no winner through the shipped ZM_PickInteractTarget caller. Both operands that
	// can go bad are covered below (the forward, and the threshold), plus the same
	// input driven through the picker.
	//
	// It is NOT a mutation pin for the `>=` vs `!(<)` spelling, and must not be
	// described as one. The SC3 extraction respells the fused block's
	// `if (fFacingDot < fMinFacingDot) { continue; }` as
	// `return fFacingDot >= fMinFacingDot`, and the two were MEASURED: mutating the
	// shipped line back to `!(fFacingDot < fMinFacingDot)`, rebuilding the Null
	// config from scratch and running the whole boot unit gate left every unit green
	// -- identical counts, nothing redded -- on a battery whose sibling mutations
	// elsewhere DID red. So this unit does NOT red on that respelling, and the
	// extraction is not known to differ from the pre-SC3 fused block on any input.
	// Why the two spellings agree was not measured; do not guess at a mechanism here.
	//
	// The unit still earns its place. Fail-closed on non-finite input is a real
	// contract for a predicate that SC6 will feed live physics positions, and this
	// is its only coverage at this surface.
	// Facing_NonFiniteSeparationIsTreatedAsCoincident does NOT cover it -- a NaN
	// SEPARATION is a different arm, taken before the dot is ever computed.
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const Zenith_Maths::Vector3 xOriginPos(0.0f);
	const Zenith_Maths::Vector3 xTarget(0.0f, 0.0f, 4.0f);

	// CONTROL: the identical fixture with both operands finite IS faced, so neither
	// assertion below can pass merely because everything is rejected.
	ZENITH_ASSERT_TRUE(ZM_IsFacingXZ(xOriginPos, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		xTarget, fZM_INTERACT_MIN_FACING_DOT),
		"fixture precondition: the un-poisoned dead-centre case must be FACED");

	// Operand 1: a non-finite FORWARD. ZM_FlattenXZ answers (NaN, 0, NaN) for
	// (NaN, 0, 1) rather than the zero vector, so what reaches ZM_IsFacingXZ is
	// itself non-finite. The assertion pins only the OBSERVABLE answer -- nothing is
	// faced -- and deliberately does not claim WHICH guard inside the primitive
	// produced it.
	ZENITH_ASSERT_FALSE(ZM_IsFacingXZ(xOriginPos, ZM_FlattenXZ(Zenith_Maths::Vector3(fNaN, 0.0f, 1.0f)),
		xTarget, fZM_INTERACT_MIN_FACING_DOT),
		"a non-finite FORWARD makes the dot non-finite, and a non-finite dot faces "
		"NOTHING -- fail closed");

	// Operand 2: a non-finite THRESHOLD, with a perfectly finite forward. The
	// second, independent path to the same fail-closed answer.
	ZENITH_ASSERT_FALSE(ZM_IsFacingXZ(xOriginPos, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		xTarget, fNaN),
		"a non-finite THRESHOLD faces nothing either -- the fail-closed answer does "
		"not depend on WHICH operand went bad");

	// ...and the same fail-closed answer observed through the SHIPPED caller, which
	// is the level that actually matters to the game: a non-finite origin forward
	// yields a reject (NOT_FACING today) and no winner index. Asserted as "not OK"
	// rather than as one specific reason, so the near-miss stage bookkeeping stays
	// free to change.
	const ZM_InteractProbe axProbes[1] = { MakeProbe(0.0f, 0.0f, 1.0f) };
	ZM_InteractOrigin xOrigin = MakeOriginLookingAlongPlusZ();
	xOrigin.m_xForward = Zenith_Maths::Vector3(fNaN, 0.0f, 1.0f);
	u_int uBest = uTEST_INDEX_POISON;
	ZENITH_ASSERT_NE((u_int)ZM_PickInteractTarget(axProbes, 1u, xOrigin, MakeLiveTuning(), uBest),
		(u_int)ZM_INTERACT_OK,
		"a non-finite player facing must never hand back a winner -- one body that "
		"goes non-finite must not make the picker accept whatever probe came first");
	ZENITH_ASSERT_EQ(uBest, 1u, "no winner -> the unreachable index uCount");
}

// ============================================================================
// S7 item 1 SC3 -- THE WALK-UP. Five contracts, all PURE, all entity-free:
//   * the DYNAMIC-CAPSULE body contract that decides whether the walk may happen
//     at all (and, by its default answer, that nothing changes when it may not);
//   * the SPOTTED exit with no body, which must remain the shipped beat;
//   * the velocity idiom, which must leave the vertical component alone;
//   * the facing, which must survive the quadrants glm::eulerAngles collapses;
//   * the speed / timeout / sight-range coupling, integrated rather than argued.
// ============================================================================

// The body contract, walked as a TRUTH TABLE rather than sampled. It is the ONE
// gate between "a trainer who can walk" and "a trainer who behaves exactly as he
// did before this sub-commit", so every way of failing it is worth a line.
ZENITH_TEST(ZM_Interaction, Approach_PossibleRequiresDynamicCapsuleAndActiveSim)
{
	// THE CONTROL. Every falsification below is measured against this, so none of
	// them can pass merely because the predicate rejects everything.
	ZENITH_ASSERT_TRUE(ZM_Interactable::IsDrivableBodyContractMet(
		true, true, true,
		COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC, true),
		"a valid DYNAMIC CAPSULE on an active simulation IS drivable -- with this "
		"clause failing, every negative below is vacuous");

	// The four observations, falsified ONE AT A TIME, so a predicate that had
	// collapsed into a single term could not pass.
	ZENITH_ASSERT_FALSE(ZM_Interactable::IsDrivableBodyContractMet(
		false, true, true, COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC, true),
		"a component whose parent entity handle is dead owns no body");
	ZENITH_ASSERT_FALSE(ZM_Interactable::IsDrivableBodyContractMet(
		true, false, true, COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC, true),
		"no collider component -> no body to drive");
	ZENITH_ASSERT_FALSE(ZM_Interactable::IsDrivableBodyContractMet(
		true, true, false, COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC, true),
		"a collider whose body was never created (or was torn down) is not drivable");
	ZENITH_ASSERT_FALSE(ZM_Interactable::IsDrivableBodyContractMet(
		true, true, true, COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC, false),
		"no active simulation -> SetLinearVelocity has nowhere to land");

	// EVERY other volume type, walked rather than sampled: the shape is half the
	// contract, and a future enum insertion must not quietly become drivable.
	const CollisionVolumeType aeRejectedVolumes[] = {
		COLLISION_VOLUME_TYPE_AABB,
		COLLISION_VOLUME_TYPE_OBB,
		COLLISION_VOLUME_TYPE_SPHERE,
		COLLISION_VOLUME_TYPE_TERRAIN,
		COLLISION_VOLUME_TYPE_MODEL_MESH,
	};
	for (u_int u = 0u; u < 5u; ++u)
	{
		ZENITH_ASSERT_FALSE(ZM_Interactable::IsDrivableBodyContractMet(
			true, true, true, aeRejectedVolumes[u], RIGIDBODY_TYPE_DYNAMIC, true),
			"volume type %u is not a CAPSULE and must not be drivable",
			(u_int)aeRejectedVolumes[u]);
	}

	// A STATIC body cannot be given a velocity at all, so it is not drivable
	// whatever its shape.
	ZENITH_ASSERT_FALSE(ZM_Interactable::IsDrivableBodyContractMet(
		true, true, true, COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_STATIC, true),
		"a STATIC capsule cannot be driven -- RIGIDBODY_TYPE has no KINEMATIC, so "
		"DYNAMIC is the only body class this component can move");

	// THE EXACT PAIR THE COMMITTED SCENE CARRIED BEFORE THIS SUB-COMMIT. Revert
	// ZM_QueueDawnmereTrainerNpc to OBB/STATIC and re-author, and the authored rival
	// answers FALSE here -- i.e. the FSM skips APPROACHING and he never walks. That
	// is the property the scene change is load-bearing FOR, stated where a headless
	// unit can hold it.
	ZENITH_ASSERT_FALSE(ZM_Interactable::IsDrivableBodyContractMet(
		true, true, true, COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC, true),
		"OBB + STATIC -- the pre-SC3 authored trainer body -- must NOT be drivable");
}

// THE NO-BEHAVIOUR-CHANGE CLAIM, held as a unit. With no body the machine must take
// the SHIPPED SPOTTED exit: no APPROACHING, no approach booked, no approach clock
// started, and the same action on the same tick it always returned.
ZENITH_TEST(ZM_Interaction, Approach_NoBodyLeavesTheShippedBeatByteForByte)
{
	const ZM_TrainerSightFsmTuning xTuning;   // default-constructed IS the shipped tuning

	// ---- (a) a trainer WITH challenge lines: SPOTTED -> CHALLENGING ----------
	ZM_TrainerSightFsm xTalker;
	ZM_TrainerSightInputs xInputs;
	xInputs.m_bMayEngage = true;
	xInputs.m_bTargetInSight = true;
	xInputs.m_bSightLineClear = true;
	xInputs.m_bChallengeAvailable = true;
	xInputs.m_fDeltaSeconds = 0.1f;
	// m_bApproachPossible is DELIBERATELY LEFT AT ITS DEFAULT. Spelling it false
	// would test a value this unit set; leaving it is what pins the DEFAULT, which is
	// the entire compatibility story for every caller SC1 did not teach.
	ZENITH_ASSERT_FALSE(xInputs.m_bApproachPossible,
		"ZM_TrainerSightInputs::m_bApproachPossible must DEFAULT to false -- every "
		"untaught caller's behaviour rides on that default");

	ZENITH_ASSERT_EQ((u_int)xTalker.Step(xInputs, xTuning),
		(u_int)ZM_TRAINER_SIGHT_ACTION_NONE, "first sighting opens the visual beat");
	ZENITH_ASSERT_EQ((u_int)xTalker.GetState(), (u_int)ZM_TRAINER_SIGHT_SPOTTED);

	ZM_TRAINER_SIGHT_ACTION eAction = ZM_TRAINER_SIGHT_ACTION_NONE;
	u_int uGuard = 0u;
	while (xTalker.GetState() == ZM_TRAINER_SIGHT_SPOTTED && uGuard < 64u)
	{
		eAction = xTalker.Step(xInputs, xTuning);
		++uGuard;
	}
	ZENITH_ASSERT_LT(uGuard, 64u, "the SPOTTED beat never completed");
	ZENITH_ASSERT_EQ((u_int)eAction, (u_int)ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE,
		"with no body the completed beat hands STRAIGHT to the bark, exactly as it "
		"did before the walk-up existed");
	ZENITH_ASSERT_EQ((u_int)xTalker.GetState(), (u_int)ZM_TRAINER_SIGHT_CHALLENGING);
	ZENITH_ASSERT_EQ(xTalker.GetApproachCount(), 0u,
		"a bodyless trainer BOOKED a walk -- either the fail-open moved below the "
		"state entry, or m_bApproachPossible is being ignored");
	ZENITH_ASSERT_EQ_FLOAT(xTalker.GetApproachElapsedSeconds(), 0.0f, 0.0f,
		"a bodyless trainer started the approach clock");

	// ---- (b) a SILENT row: SPOTTED -> the encounter, still with no walk -------
	ZM_TrainerSightFsm xSilent;
	xInputs.m_bChallengeAvailable = false;
	ZENITH_ASSERT_EQ((u_int)xSilent.Step(xInputs, xTuning),
		(u_int)ZM_TRAINER_SIGHT_ACTION_NONE);
	eAction = ZM_TRAINER_SIGHT_ACTION_NONE;
	uGuard = 0u;
	while (xSilent.GetState() == ZM_TRAINER_SIGHT_SPOTTED && uGuard < 64u)
	{
		eAction = xSilent.Step(xInputs, xTuning);
		++uGuard;
	}
	ZENITH_ASSERT_LT(uGuard, 64u, "the silent SPOTTED beat never completed");
	ZENITH_ASSERT_EQ((u_int)eAction, (u_int)ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"a silent bodyless trainer still raises straight out of the visual beat");
	ZENITH_ASSERT_EQ(xSilent.GetApproachCount(), 0u);
	ZENITH_ASSERT_EQ(xSilent.GetRaiseCount(), 1u,
		"exactly one raise per spotting, unchanged");
}

// The velocity idiom, driven DIRECTLY. The walk and the patrol go through the same
// helper for one reason: the vertical component belongs to gravity and the terrain,
// and a walk that wrote a full 3D velocity would launch or bury the trainer while
// the XZ maths still looked perfect.
ZENITH_TEST(ZM_Interaction, Approach_VelocityPreservesVerticalComponent)
{
	// A FALLING body: the vertical component is real, non-zero, and NOT ours.
	const Zenith_Maths::Vector3 xFalling(0.0f, -7.25f, 0.0f);
	const float fSpeed = ZM_PlayerController::fWALK_SPEED;

	const ZM_TrainerApproachStep xStep = ZM_StepTrainerApproach(
		Zenith_Maths::Vector3(10.0f, 3.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 99.0f, 0.0f),   // a wildly different Y: XZ ONLY
		2.0f, fSpeed);
	ZENITH_ASSERT_FALSE(xStep.m_bArrived, "10 m of XZ gap is not an arrival");
	ZENITH_ASSERT_EQ_FLOAT(xStep.m_xDirXZ.y, 0.0f, 0.0f,
		"the approach direction must be flat -- Y belongs to the body");

	const Zenith_Maths::Vector3 xVelocity =
		ZM_BuildPatrolVelocity(xStep.m_xDirXZ, xStep.m_fSpeed, xFalling);
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.y, xFalling.y, 0.0f,
		"the walk overwrote the body's vertical velocity -- gravity and terrain "
		"response are no longer in sole ownership of it");
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.x, -fSpeed, 0.0005f,
		"the trainer must travel toward the target along -X at exactly the walk speed");
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.z, 0.0f, 0.0005f);

	// THE STATION HOLD uses the same helper with a zero request, so the same
	// guarantee has to hold there: XZ is clamped, Y is untouched.
	const Zenith_Maths::Vector3 xHeld = ZM_BuildPatrolVelocity(
		Zenith_Maths::Vector3(0.0f), 0.0f, Zenith_Maths::Vector3(3.0f, -7.25f, -4.0f));
	ZENITH_ASSERT_EQ_FLOAT(xHeld.x, 0.0f, 0.0f,
		"the station hold must clamp a shove out of XZ");
	ZENITH_ASSERT_EQ_FLOAT(xHeld.z, 0.0f, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xHeld.y, -7.25f, 0.0f,
		"the station hold must NOT clamp the fall -- a held trainer still has to rest "
		"on the ground");
}

// THE PAST-90-DEGREES QUADRANTS ARE THE WHOLE POINT. Building the yaw with
// glm::eulerAngles(quat).y collapses once the direction is more than 90 degrees off
// +Z, which is why this walks all four quadrants and asserts on the ROTATED FORWARD
// rather than on quaternion components -- a decomposition bug is invisible in the
// raw floats and obvious in where the trainer ends up looking.
ZENITH_TEST(ZM_Interaction, Approach_YawFacesTravelDirectionAcrossAllFourQuadrants)
{
	// Deliberately NOT axis-aligned and NOT unit length: the builder normalises, and
	// off-axis directions are what actually distinguish the two spellings.
	const Zenith_Maths::Vector3 axDirections[] = {
		Zenith_Maths::Vector3( 3.0f, 0.0f,  4.0f),   // +X +Z  (inside 90 deg of +Z)
		Zenith_Maths::Vector3( 4.0f, 0.0f, -3.0f),   // +X -Z  PAST 90 DEGREES
		Zenith_Maths::Vector3(-3.0f, 0.0f, -4.0f),   // -X -Z  PAST 90 DEGREES
		Zenith_Maths::Vector3(-4.0f, 0.0f,  3.0f),   // -X +Z
	};

	for (u_int u = 0u; u < 4u; ++u)
	{
		const Zenith_Maths::Vector3& xDirection = axDirections[u];
		Zenith_Maths::Quat xFacing(1.0f, 0.0f, 0.0f, 0.0f);
		ZENITH_ASSERT_TRUE(
			ZM_Interactable::TryBuildApproachFacing(xDirection, xFacing),
			"quadrant %u is a perfectly good travel direction and must be accepted", u);

		const Zenith_Maths::Vector3 xExpected = ZM_FlattenXZ(xDirection);
		const float fExpectedLength =
			std::sqrt(xExpected.x * xExpected.x + xExpected.z * xExpected.z);
		ZENITH_ASSERT_GT(fExpectedLength, 0.0f, "fixture precondition (quadrant %u)", u);

		// ZM_ForwardFromRotation is the SAME function the sight cone uses to decide
		// where a trainer is looking, so this asserts the facing in the only terms the
		// game ever reads it in.
		const Zenith_Maths::Vector3 xForward = ZM_ForwardFromRotation(xFacing);
		ZENITH_ASSERT_EQ_FLOAT(xForward.x, xExpected.x / fExpectedLength, 0.0005f,
			"quadrant %u: the authored facing does not point along the travel "
			"direction. A yaw rebuilt through glm::eulerAngles(quat).y collapses on "
			"exactly the quadrants more than 90 degrees off +Z", u);
		ZENITH_ASSERT_EQ_FLOAT(xForward.z, xExpected.z / fExpectedLength, 0.0005f,
			"quadrant %u: the authored facing does not point along the travel "
			"direction", u);
		ZENITH_ASSERT_EQ_FLOAT(xForward.y, 0.0f, 0.0005f,
			"quadrant %u: the walk-up facing must be a pure yaw about +Y", u);
	}
}

// TOTALITY, and a refusal that MATTERS: atan2(0, 0) is a finite zero, so a builder
// that answered instead of refusing would pivot an arriving trainer to face +Z on
// the frame he stopped -- with no NaN, no assert, and nothing to grep for.
ZENITH_TEST(ZM_Interaction, Approach_FacingIsRefusedForEveryDegenerateDirection)
{
	const float fNaNDirection = std::numeric_limits<float>::quiet_NaN();
	const float fInfDirection = std::numeric_limits<float>::infinity();

	// The sentinel is a RECOGNISABLE non-identity value, so "left untouched" is an
	// observation rather than a coincidence with the identity a builder might write.
	const Zenith_Maths::Quat xSentinel(0.5f, 0.5f, 0.5f, 0.5f);
	const Zenith_Maths::Vector3 axRefused[] = {
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),    // the ARRIVAL tick's direction
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),    // vertical only: no XZ heading
		Zenith_Maths::Vector3(fNaNDirection, 0.0f, 1.0f),
		Zenith_Maths::Vector3(1.0f, 0.0f, fNaNDirection),
		Zenith_Maths::Vector3(fInfDirection, 0.0f, -fInfDirection),
	};

	for (u_int u = 0u; u < 5u; ++u)
	{
		Zenith_Maths::Quat xFacing = xSentinel;
		ZENITH_ASSERT_FALSE(
			ZM_Interactable::TryBuildApproachFacing(axRefused[u], xFacing),
			"degenerate direction %u must be REFUSED, never answered with a yaw", u);
		ZENITH_ASSERT_EQ_FLOAT(xFacing.w, xSentinel.w, 0.0f,
			"a refused direction must leave the caller's facing untouched (%u)", u);
		ZENITH_ASSERT_EQ_FLOAT(xFacing.x, xSentinel.x, 0.0f);
		ZENITH_ASSERT_EQ_FLOAT(xFacing.y, xSentinel.y, 0.0f);
		ZENITH_ASSERT_EQ_FLOAT(xFacing.z, xSentinel.z, 0.0f);
	}
}

// THE SPEED / TIMEOUT / SIGHT-RANGE COUPLING, PROVEN BY SIMULATION RATHER THAN BY
// ARITHMETIC. The approach timeout FAILS OPEN: when it expires the machine hands off
// wherever the trainer happens to be standing. So a walk that is merely too slow
// produces no failure anywhere -- just a rival who stops short and a beat that reads
// as a stutter. This integrates the real step from the worst case the cone admits and
// requires arrival inside the real timeout.
//
// ★ IT SIMULATES THE **DERATED** SPEED, AND THE FIRST REVISION'S FAILURE TO DO SO IS
// WHY THIS COMMENT EXISTS. That revision integrated a frictionless point mass at the
// full commanded fWALK_SPEED and measured arrival with the same positions it
// integrated -- no friction, no contact response, no start-up latency, no observation
// skew. It therefore certified a timing margin the live walk does not have, and it
// PASSED on the run where the live walk was in question. A unit that models something
// the shipped path is not cannot be evidence about the shipped path.
//
// ZM_Interactable::fAPPROACH_SPEED_EFFICIENCY carries the measured floor and its
// derivation; ZM_RivalVesperAuthored_Test logs the achieved speed live on every run.
// This unit is the CHEAP half of that pair: it runs headless in CI where the windowed
// walk can only RequestSkip.
ZENITH_TEST(ZM_Interaction, Approach_WalkConvergesIntoTheStandoffRingBeforeTheTimeout)
{
	const ZM_TrainerSightFsmTuning xTuning;
	// The speed the body ACHIEVES, not the one it is handed. Arrival is governed by
	// the former, and the two are not the same number.
	const float fSpeed =
		ZM_PlayerController::fWALK_SPEED * ZM_Interactable::fAPPROACH_SPEED_EFFICIENCY;
	const float fDt = 1.0f / 60.0f;
	const float fStepLength = fSpeed * fDt;

	// The efficiency floor is a DERATING, so anything at or above 1 would mean the
	// body is assumed to travel at least as fast as commanded -- which is the exact
	// assumption that made the first revision of this unit vacuous.
	ZENITH_ASSERT_GT(ZM_Interactable::fAPPROACH_SPEED_EFFICIENCY, 0.0f,
		"the achieved-speed floor must be a positive fraction");
	ZENITH_ASSERT_LT(ZM_Interactable::fAPPROACH_SPEED_EFFICIENCY, 1.0f,
		"the achieved-speed floor must be a DERATING of the commanded speed -- at or "
		"above 1.0 this unit is back to simulating a frictionless point mass");

	// The WORST case: the target sits at the very edge of the sight range, off-axis
	// and at a different height, so nothing about this fixture is axis-aligned.
	const Zenith_Maths::Vector3 xTarget(0.0f, 26.0f, 0.0f);
	Zenith_Maths::Vector3 xTrainer(
		fZM_SIGHT_MAX_DISTANCE * 0.6f, 24.5f, fZM_SIGHT_MAX_DISTANCE * 0.8f);

	float fElapsed = 0.0f;
	float fDistance = 1.0e9f;
	float fPrevDistance = 1.0e9f;
	bool bArrived = false;
	bool bMonotonic = true;
	u_int uSteps = 0u;
	while (uSteps < 4096u)
	{
		const ZM_TrainerApproachStep xStep = ZM_StepTrainerApproach(
			xTrainer, xTarget, xTuning.m_fApproachStandoffMetres, fSpeed);
		fDistance = std::sqrt(
			(xTarget.x - xTrainer.x) * (xTarget.x - xTrainer.x)
			+ (xTarget.z - xTrainer.z) * (xTarget.z - xTrainer.z));
		if (fDistance > fPrevDistance)
		{
			bMonotonic = false;
		}
		fPrevDistance = fDistance;
		if (xStep.m_bArrived)
		{
			bArrived = true;
			break;
		}
		// The SAME two-call idiom the runtime uses: direction + speed through
		// ZM_BuildPatrolVelocity, integrated over one fixed frame. The trainer stays
		// 1.5 m below the target for the whole walk and it changes nothing, which is
		// the XZ-only claim restated as motion.
		const Zenith_Maths::Vector3 xVelocity = ZM_BuildPatrolVelocity(
			xStep.m_xDirXZ, xStep.m_fSpeed, Zenith_Maths::Vector3(0.0f));
		xTrainer.x += xVelocity.x * fDt;
		xTrainer.z += xVelocity.z * fDt;
		fElapsed += fDt;
		++uSteps;
	}

	ZENITH_ASSERT_TRUE(bArrived,
		"the walk never reached the standoff ring at all (%u steps)", uSteps);
	ZENITH_ASSERT_TRUE(bMonotonic,
		"the walk did not close monotonically -- it oscillated or overshot");
	ZENITH_ASSERT_LE(fElapsed, xTuning.m_fApproachTimeoutSeconds,
		"the walk took %.3f s to cross the sight range at the DERATED speed but the "
		"FSM gives it %.3f s and then FAILS OPEN -- the rival would visibly stop "
		"short with every test still green. Raise the speed, shorten the cone, "
		"shrink the standoff, or lengthen the timeout",
		(double)fElapsed, (double)xTuning.m_fApproachTimeoutSeconds);
	// ...and the same claim the static_assert in ZM_Interactable.cpp makes, restated
	// as a MEASURED margin rather than a compile-time inequality, so the log carries
	// the number a future tuning change has to keep positive.
	ZENITH_ASSERT_GT(xTuning.m_fApproachTimeoutSeconds - fElapsed, 0.10f,
		"the derated walk finishes with only %.3f s of the %.3f s timeout to spare -- "
		"too thin to absorb a slope or a contact, and the timeout fails OPEN so the "
		"symptom would be a rival who stops short, never a red test",
		(double)(xTuning.m_fApproachTimeoutSeconds - fElapsed),
		(double)xTuning.m_fApproachTimeoutSeconds);

	// Arrival is INCLUSIVE, and the step can never carry the trainer PAST the ring by
	// more than the frame he arrived on -- i.e. he stops ON the standoff, never on
	// top of the player.
	ZENITH_ASSERT_LE(fDistance, xTuning.m_fApproachStandoffMetres + 0.0005f,
		"the walk stopped outside the standoff ring");
	ZENITH_ASSERT_GE(fDistance,
		xTuning.m_fApproachStandoffMetres - fStepLength - 0.0005f,
		"the walk carried the trainer well past the ring it was asked to stop on");
}
