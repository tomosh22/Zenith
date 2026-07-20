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
// ============================================================================

#include <cstring>   // strcmp (reject-name distinctness)

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"
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
