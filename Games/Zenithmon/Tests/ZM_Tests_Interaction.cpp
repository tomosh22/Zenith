#include "Zenith.h"

// ============================================================================
// ZM_Tests_Interaction -- S6 item 3 (SC1) unit tests for the NPC-interaction
// foundation: the pure ZM_ShouldInteract gate, its reject-reason name formatter,
// and the key-binding collision units that keep the interact key from aliasing
// anything Zenithmon already claims.
//
// Everything here is PURE: no ECS, no scene, no graphics, no baked assets, no
// engine instance -- the key-set constants are constexpr and the gate is all
// bools in / one enum out. Every fixture is deterministic and hermetic, so no
// RequestSkip is needed.
// ============================================================================

#include <cstring>   // strcmp (reject-name distinctness)

#include "Core/Zenith_TestFramework.h"
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
