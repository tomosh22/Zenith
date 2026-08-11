#include "Zenith.h"

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "Zenithmon/Components/ZM_TouchLayoutController.h"
#include "Zenithmon/Tests/ZM_BindingsTestRig.h"

#include <cstring>

// =============================================================================
// Zenithmon input program WP3b -- the C2 action table and the B11 HUD contexts.
//
// Everything here drives a LOCAL Zenith_InputActions (see ZM_BindingsTestRig.h
// for why): the engine's own instance is opened and closed by the main loop, and
// a boot unit runs before the first frame exists.
//
// The parity units are deliberately written against the game's FROZEN legacy
// movement semantics -- +y forward, opposite keys cancel, diagonals
// UNNORMALISED, one edge per press -- because a migration that changed the
// feel of walking would otherwise pass every structural assertion.
// =============================================================================

namespace
{
	constexpr float fTEST_EPSILON = 0.001f;

	bool ContainsKey(const int32_t* piKeys, u_int uCount, int32_t iKey)
	{
		for (u_int u = 0u; u < uCount; ++u)
		{
			if (piKeys[u] == iKey)
			{
				return true;
			}
		}
		return false;
	}

	// The one binding row of a given type on an action, or nullptr.
	const Zenith_InputBinding* FindBinding(const Zenith_InputActions& xActions,
		Zenith_InputActionID uAction, Zenith_EInputBindingType eType)
	{
		const u_int32 uCount = xActions.GetBindingCount(uAction);
		for (u_int32 u = 0; u < uCount; u++)
		{
			const Zenith_InputBinding& xBinding = xActions.GetBinding(uAction, u);
			if (xBinding.m_eType == eType)
			{
				return &xBinding;
			}
		}
		return nullptr;
	}

	u_int CountBindingsOfType(const Zenith_InputActions& xActions,
		Zenith_InputActionID uAction, Zenith_EInputBindingType eType)
	{
		u_int uFound = 0u;
		const u_int32 uCount = xActions.GetBindingCount(uAction);
		for (u_int32 u = 0; u < uCount; u++)
		{
			if (xActions.GetBinding(uAction, u).m_eType == eType)
			{
				++uFound;
			}
		}
		return uFound;
	}

	// Every action this game registers, in id order.
	constexpr Zenith_InputActionID auALL_ACTIONS[] =
	{
		ZM_Bindings::ZM_ACTION_MOVE,
		ZM_Bindings::ZM_ACTION_RUN,
		ZM_Bindings::ZM_ACTION_INTERACT,
		ZM_Bindings::ZM_ACTION_CONFIRM,
		ZM_Bindings::ZM_ACTION_CANCEL,
		ZM_Bindings::ZM_ACTION_MENU,
		ZM_Bindings::ZM_ACTION_MENU_UP,
		ZM_Bindings::ZM_ACTION_MENU_DOWN,
	};
	constexpr u_int uALL_ACTION_COUNT =
		(u_int)(sizeof(auALL_ACTIONS) / sizeof(auALL_ACTIONS[0]));

	static_assert(uALL_ACTION_COUNT == (u_int)ZM_Bindings::ZM_ACTION_COUNT,
		"the walk above must enumerate EVERY registered action, or the units below "
		"silently stop covering the one that was added");
}

// -----------------------------------------------------------------------------
// Registration integrity
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_Bindings, ProfilesReplaceTheEngineDefaultsWithOneSchemeEach)
{
	ZM_BindingsTest::Rig xRig;

	// The FIRST game RegisterProfile call clears the engine's platform default
	// wholesale. If that ever stopped happening, this game would be competing
	// with EngineDesktop/EngineTouch for a scheme and the auto-switch would have
	// two answers -- which the engine asserts on rather than resolves.
	ZENITH_ASSERT_FALSE(xRig.m_xActions.AreEngineDefaultProfilesActive(),
		"the game's profiles must have replaced the engine defaults");

	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsProfileRegistered(ZM_Bindings::uPROFILE_KEYBOARD));
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsProfileRegistered(ZM_Bindings::uPROFILE_TOUCH));
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsProfileRegistered(ZM_Bindings::uPROFILE_GAMEPAD));

	ZENITH_ASSERT_EQ((u_int)xRig.m_xActions.GetProfileSchemeMask(ZM_Bindings::uPROFILE_KEYBOARD),
		(u_int)uINPUT_SCHEME_MASK_KEYBOARD, "P_KEYBOARD owns exactly the keyboard");
	ZENITH_ASSERT_EQ((u_int)xRig.m_xActions.GetProfileSchemeMask(ZM_Bindings::uPROFILE_TOUCH),
		(u_int)uINPUT_SCHEME_MASK_TOUCH, "P_TOUCH owns exactly touch");
	ZENITH_ASSERT_EQ((u_int)xRig.m_xActions.GetProfileSchemeMask(ZM_Bindings::uPROFILE_GAMEPAD),
		(u_int)uINPUT_SCHEME_MASK_GAMEPAD, "P_GAMEPAD owns exactly the pad");

	// A scheme may live in at most ONE profile, so the three masks must be
	// pairwise disjoint. MOUSE deliberately lives in none of them.
	const u_int8 uUnion = (u_int8)(uINPUT_SCHEME_MASK_KEYBOARD | uINPUT_SCHEME_MASK_TOUCH
		| uINPUT_SCHEME_MASK_GAMEPAD);
	ZENITH_ASSERT_EQ((u_int)(uUnion & uINPUT_SCHEME_MASK_MOUSE), 0u,
		"this game registers no mouse-sourced action, so MOUSE owns no profile");
}

ZENITH_TEST(ZM_Bindings, EveryActionIsRegisteredWithItsContractIdNameAndKind)
{
	ZM_BindingsTest::Rig xRig;

	struct Expected
	{
		Zenith_InputActionID m_uId;
		const char* m_szName;
		Zenith_EInputActionKind m_eKind;
	};
	const Expected axExpected[] =
	{
		{ ZM_Bindings::ZM_ACTION_MOVE,      ZM_Bindings::szACTION_MOVE,      INPUT_ACTION_AXIS2D },
		{ ZM_Bindings::ZM_ACTION_RUN,       ZM_Bindings::szACTION_RUN,       INPUT_ACTION_BUTTON },
		{ ZM_Bindings::ZM_ACTION_INTERACT,  ZM_Bindings::szACTION_INTERACT,  INPUT_ACTION_BUTTON },
		{ ZM_Bindings::ZM_ACTION_CONFIRM,   ZM_Bindings::szACTION_CONFIRM,   INPUT_ACTION_BUTTON },
		{ ZM_Bindings::ZM_ACTION_CANCEL,    ZM_Bindings::szACTION_CANCEL,    INPUT_ACTION_BUTTON },
		{ ZM_Bindings::ZM_ACTION_MENU,      ZM_Bindings::szACTION_MENU,      INPUT_ACTION_BUTTON },
		{ ZM_Bindings::ZM_ACTION_MENU_UP,   ZM_Bindings::szACTION_MENU_UP,   INPUT_ACTION_BUTTON },
		{ ZM_Bindings::ZM_ACTION_MENU_DOWN, ZM_Bindings::szACTION_MENU_DOWN, INPUT_ACTION_BUTTON },
	};
	const u_int uExpectedCount = (u_int)(sizeof(axExpected) / sizeof(axExpected[0]));
	ZENITH_ASSERT_EQ(uExpectedCount, uALL_ACTION_COUNT,
		"the expectation table must cover every registered action");

	for (u_int u = 0u; u < uExpectedCount; ++u)
	{
		const Expected& xRow = axExpected[u];
		ZENITH_ASSERT_TRUE(xRig.m_xActions.IsActionRegistered(xRow.m_uId),
			"action '%s' (id %u) is not registered", xRow.m_szName, (u_int)xRow.m_uId);
		ZENITH_ASSERT_EQ((u_int)xRig.m_xActions.GetActionKind(xRow.m_uId), (u_int)xRow.m_eKind,
			"action '%s' has the wrong value kind", xRow.m_szName);
		// The NAME is the contract an on-screen control's SetAction and a graph
		// node both use, so a rename that missed one site fails here.
		ZENITH_ASSERT_EQ((u_int)xRig.m_xActions.FindActionByName(xRow.m_szName),
			(u_int)xRow.m_uId, "action name '%s' does not resolve to its id", xRow.m_szName);
	}

	// Game ids start at 16; 0-15 are the engine's UI nav set and are registered by
	// the engine alone. A game id that strayed below the line would either assert
	// at registration or silently overwrite UI navigation.
	for (u_int u = 0u; u < uALL_ACTION_COUNT; ++u)
	{
		ZENITH_ASSERT_GE((u_int)auALL_ACTIONS[u], (u_int)uINPUT_ACTION_FIRST_GAME_ID,
			"game action %u is inside the engine-reserved range", u);
	}
}

ZENITH_TEST(ZM_Bindings, BindingTableMatchesTheC2ContractIncludingThePadColumn)
{
	ZM_BindingsTest::Rig xRig;
	const Zenith_InputActions& xActions = xRig.m_xActions;

	// --- MOVE: keyboard composite + virtual stick + pad stick + pad d-pad ---
	ZENITH_ASSERT_EQ((u_int)xActions.GetBindingCount(ZM_Bindings::ZM_ACTION_MOVE), 4u,
		"MOVE carries the keyboard, touch and BOTH pad rows");
	const Zenith_InputBinding* pxMoveKeys =
		FindBinding(xActions, ZM_Bindings::ZM_ACTION_MOVE, INPUT_BINDING_KEY_AXIS2D);
	ZENITH_ASSERT_NOT_NULL(pxMoveKeys, "MOVE has no keyboard row");
	if (pxMoveKeys != nullptr)
	{
		// WASD *and* the arrows, two keys per direction.
		for (u_int32 uSet = 0; uSet < 4u; uSet++)
		{
			ZENITH_ASSERT_EQ((u_int)pxMoveKeys->m_auKeyCount[uSet], 2u,
				"MOVE direction set %u must carry both its WASD and arrow key", uSet);
		}
		ZENITH_ASSERT_EQ(pxMoveKeys->m_aaiKeys[Zenith_InputBinding::uSET_FORWARD][0], (int32_t)ZENITH_KEY_W);
		ZENITH_ASSERT_EQ(pxMoveKeys->m_aaiKeys[Zenith_InputBinding::uSET_FORWARD][1], (int32_t)ZENITH_KEY_UP);
		ZENITH_ASSERT_EQ(pxMoveKeys->m_aaiKeys[Zenith_InputBinding::uSET_BACK][0], (int32_t)ZENITH_KEY_S);
		ZENITH_ASSERT_EQ(pxMoveKeys->m_aaiKeys[Zenith_InputBinding::uSET_BACK][1], (int32_t)ZENITH_KEY_DOWN);
		ZENITH_ASSERT_EQ(pxMoveKeys->m_aaiKeys[Zenith_InputBinding::uSET_LEFT][0], (int32_t)ZENITH_KEY_A);
		ZENITH_ASSERT_EQ(pxMoveKeys->m_aaiKeys[Zenith_InputBinding::uSET_LEFT][1], (int32_t)ZENITH_KEY_LEFT);
		ZENITH_ASSERT_EQ(pxMoveKeys->m_aaiKeys[Zenith_InputBinding::uSET_RIGHT][0], (int32_t)ZENITH_KEY_D);
		ZENITH_ASSERT_EQ(pxMoveKeys->m_aaiKeys[Zenith_InputBinding::uSET_RIGHT][1], (int32_t)ZENITH_KEY_RIGHT);
	}
	const Zenith_InputBinding* pxMoveStick =
		FindBinding(xActions, ZM_Bindings::ZM_ACTION_MOVE, INPUT_BINDING_GAMEPAD_STICK);
	ZENITH_ASSERT_NOT_NULL(pxMoveStick, "MOVE has no pad stick row");
	if (pxMoveStick != nullptr)
	{
		ZENITH_ASSERT_EQ(pxMoveStick->m_iCode, (int32_t)ZENITH_GAMEPAD_AXIS_LEFT_X,
			"MOVE reads the LEFT stick");
		// ★ THE Y INVERSION IS THE CONTRACT, NOT A PREFERENCE. GLFW reports a
		// forward push as -1 and the engine's axis2D convention is +y FORWARD, so
		// a scale of +1 here would drive the player backwards on a pad and nowhere
		// else -- exactly the defect nobody notices without a pad on the desk.
		ZENITH_ASSERT_EQ_FLOAT(pxMoveStick->m_fScaleX,  1.0f, fTEST_EPSILON);
		ZENITH_ASSERT_EQ_FLOAT(pxMoveStick->m_fScaleY, -1.0f, fTEST_EPSILON);
	}
	ZENITH_ASSERT_NOT_NULL(
		FindBinding(xActions, ZM_Bindings::ZM_ACTION_MOVE, INPUT_BINDING_GAMEPAD_DPAD_AXIS2D),
		"MOVE has no pad d-pad composite row");

	// --- The button rows, per C2 (keyboard set / virtual / pad button) ---
	struct ButtonRow
	{
		Zenith_InputActionID m_uAction;
		const char* m_szName;
		int32_t m_aiKeys[2];
		u_int m_uKeyCount;
		int32_t m_iPadButton;
		bool m_bHasVirtual;
	};
	const ButtonRow axRows[] =
	{
		{ ZM_Bindings::ZM_ACTION_RUN, "RUN",
			{ ZENITH_KEY_LEFT_SHIFT, ZENITH_KEY_RIGHT_SHIFT }, 2u, ZENITH_GAMEPAD_BUTTON_B, true },
		{ ZM_Bindings::ZM_ACTION_INTERACT, "INTERACT",
			{ ZENITH_KEY_E, -1 }, 1u, ZENITH_GAMEPAD_BUTTON_X, true },
		{ ZM_Bindings::ZM_ACTION_CONFIRM, "CONFIRM",
			{ ZENITH_KEY_ENTER, ZENITH_KEY_SPACE }, 2u, ZENITH_GAMEPAD_BUTTON_A, true },
		{ ZM_Bindings::ZM_ACTION_CANCEL, "CANCEL",
			{ ZENITH_KEY_ESCAPE, ZENITH_KEY_BACKSPACE }, 2u, ZENITH_GAMEPAD_BUTTON_B, true },
		{ ZM_Bindings::ZM_ACTION_MENU, "MENU",
			{ ZENITH_KEY_M, ZENITH_KEY_TAB }, 2u, ZENITH_GAMEPAD_BUTTON_START, true },
		// MENU_UP / MENU_DOWN have NO touch row: a touch player taps the entry.
		{ ZM_Bindings::ZM_ACTION_MENU_UP, "MENU_UP",
			{ ZENITH_KEY_W, ZENITH_KEY_UP }, 2u, ZENITH_GAMEPAD_BUTTON_DPAD_UP, false },
		{ ZM_Bindings::ZM_ACTION_MENU_DOWN, "MENU_DOWN",
			{ ZENITH_KEY_S, ZENITH_KEY_DOWN }, 2u, ZENITH_GAMEPAD_BUTTON_DPAD_DOWN, false },
	};

	for (const ButtonRow& xRow : axRows)
	{
		const Zenith_InputBinding* pxKeys =
			FindBinding(xActions, xRow.m_uAction, INPUT_BINDING_KEY_SET);
		ZENITH_ASSERT_NOT_NULL(pxKeys, "%s has no keyboard row", xRow.m_szName);
		if (pxKeys != nullptr)
		{
			ZENITH_ASSERT_EQ((u_int)pxKeys->m_auKeyCount[Zenith_InputBinding::uSET_NEGATIVE],
				xRow.m_uKeyCount, "%s keyboard row has the wrong key count", xRow.m_szName);
			for (u_int u = 0u; u < xRow.m_uKeyCount; ++u)
			{
				ZENITH_ASSERT_EQ(pxKeys->m_aaiKeys[Zenith_InputBinding::uSET_NEGATIVE][u],
					xRow.m_aiKeys[u], "%s keyboard key %u", xRow.m_szName, u);
			}
		}

		const Zenith_InputBinding* pxPad =
			FindBinding(xActions, xRow.m_uAction, INPUT_BINDING_GAMEPAD_BUTTON);
		ZENITH_ASSERT_NOT_NULL(pxPad, "%s has no PAD row -- the C2 pad column is mandatory",
			xRow.m_szName);
		if (pxPad != nullptr)
		{
			ZENITH_ASSERT_EQ(pxPad->m_iCode, xRow.m_iPadButton, "%s pad button", xRow.m_szName);
		}

		ZENITH_ASSERT_EQ(CountBindingsOfType(xActions, xRow.m_uAction, INPUT_BINDING_VIRTUAL),
			xRow.m_bHasVirtual ? 1u : 0u, "%s virtual row count", xRow.m_szName);
	}

	// INTERACT and CONFIRM are live at the SAME time on a pad (unlike on touch,
	// where the same button retargets), so aliasing their pad face buttons would
	// open a menu every time the player talked to somebody.
	const Zenith_InputBinding* pxInteractPad =
		FindBinding(xActions, ZM_Bindings::ZM_ACTION_INTERACT, INPUT_BINDING_GAMEPAD_BUTTON);
	const Zenith_InputBinding* pxConfirmPad =
		FindBinding(xActions, ZM_Bindings::ZM_ACTION_CONFIRM, INPUT_BINDING_GAMEPAD_BUTTON);
	if (pxInteractPad != nullptr && pxConfirmPad != nullptr)
	{
		ZENITH_ASSERT_NE(pxInteractPad->m_iCode, pxConfirmPad->m_iCode,
			"INTERACT and CONFIRM must not share a pad face button");
	}

	// CANCEL alone carries the platform BACK gesture, and that row is mask-exempt.
	const Zenith_InputBinding* pxBack =
		FindBinding(xActions, ZM_Bindings::ZM_ACTION_CANCEL, INPUT_BINDING_SYSTEM_BACK);
	ZENITH_ASSERT_NOT_NULL(pxBack, "CANCEL must carry the SYSTEM_BACK row");
	if (pxBack != nullptr)
	{
		ZENITH_ASSERT_TRUE(pxBack->IsMaskExempt(),
			"the SYSTEM_BACK row must answer whatever profile is active");
	}
	for (u_int u = 0u; u < uALL_ACTION_COUNT; ++u)
	{
		if (auALL_ACTIONS[u] == ZM_Bindings::ZM_ACTION_CANCEL)
		{
			continue;
		}
		ZENITH_ASSERT_EQ(
			CountBindingsOfType(xActions, auALL_ACTIONS[u], INPUT_BINDING_SYSTEM_BACK), 0u,
			"only CANCEL may claim the system Back gesture");
	}
}

ZENITH_TEST(ZM_Bindings, VirtualSourceIdsArePairwiseDistinct)
{
	ZM_BindingsTest::Rig xRig;

	// ★ THE LOAD-BEARING PROPERTY OF THE WHOLE TOUCH LAYOUT. A widget publishes to
	// the SOURCE its target action names, so INTERACT and CONFIRM sharing an id
	// would make the overworld A button hold BOTH actions at once -- and the
	// context switch, which exists precisely to keep them apart, would do nothing.
	int32_t aiSources[uALL_ACTION_COUNT];
	u_int uSourceCount = 0u;
	for (u_int u = 0u; u < uALL_ACTION_COUNT; ++u)
	{
		const Zenith_InputBinding* pxVirtual =
			FindBinding(xRig.m_xActions, auALL_ACTIONS[u], INPUT_BINDING_VIRTUAL);
		if (pxVirtual == nullptr)
		{
			continue;
		}
		ZENITH_ASSERT_FALSE(ContainsKey(aiSources, uSourceCount, pxVirtual->m_iCode),
			"virtual source %d is claimed by two actions", pxVirtual->m_iCode);
		ZENITH_ASSERT_GE(pxVirtual->m_iCode, 0);
		ZENITH_ASSERT_LT(pxVirtual->m_iCode,
			(int32_t)Zenith_InputActions::uMAX_VIRTUAL_SOURCES);
		aiSources[uSourceCount++] = pxVirtual->m_iCode;
	}
	// Stick + A/INTERACT + B/RUN + A/CONFIRM + B/CANCEL + MENU.
	ZENITH_ASSERT_EQ(uSourceCount, 6u, "six actions are reachable from an on-screen control");

	// A VIRTUAL row belongs to the TOUCH column, so it is dead in the keyboard and
	// pad profiles by construction rather than by anybody remembering to check.
	const Zenith_InputBinding* pxMoveVirtual =
		FindBinding(xRig.m_xActions, ZM_Bindings::ZM_ACTION_MOVE, INPUT_BINDING_VIRTUAL);
	ZENITH_ASSERT_NOT_NULL(pxMoveVirtual);
	if (pxMoveVirtual != nullptr)
	{
		ZENITH_ASSERT_EQ((u_int)pxMoveVirtual->GetScheme(), (u_int)INPUT_SCHEME_TOUCH);
	}
}

// -----------------------------------------------------------------------------
// Behaviour parity with the frozen legacy movement semantics
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_Bindings, MoveCompositeKeepsTheLegacyForwardCancelAndDiagonalRules)
{
	ZM_BindingsTest::Rig xRig;
	xRig.m_xActions.SetProfileOverride(ZM_Bindings::uPROFILE_KEYBOARD);

	// W + Right -> the UNNORMALISED diagonal (1, 1). The old ResolveMove added
	// each axis independently and never normalised; ZM_PlayerController normalises
	// downstream, so changing it here would silently change walking speed.
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_W);
	xRig.KeyDown(ZENITH_KEY_RIGHT);
	xRig.CloseFrame();
	Zenith_Maths::Vector2 xMove = xRig.Move();
	ZENITH_ASSERT_EQ_FLOAT(xMove.x, 1.0f, fTEST_EPSILON, "D/Right is +x");
	ZENITH_ASSERT_EQ_FLOAT(xMove.y, 1.0f, fTEST_EPSILON, "W/Up is +y FORWARD");

	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_W);
	xRig.KeyUp(ZENITH_KEY_RIGHT);
	xRig.KeyDown(ZENITH_KEY_S);
	xRig.KeyDown(ZENITH_KEY_LEFT);
	xRig.CloseFrame();
	xMove = xRig.Move();
	ZENITH_ASSERT_EQ_FLOAT(xMove.x, -1.0f, fTEST_EPSILON, "A/Left is -x");
	ZENITH_ASSERT_EQ_FLOAT(xMove.y, -1.0f, fTEST_EPSILON, "S/Down is -y");

	// Opposite keys CANCEL rather than latching one of them.
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_UP);
	xRig.KeyDown(ZENITH_KEY_A);
	xRig.KeyDown(ZENITH_KEY_D);
	xRig.CloseFrame();
	xMove = xRig.Move();
	ZENITH_ASSERT_EQ_FLOAT(xMove.x, 0.0f, fTEST_EPSILON, "A and D cancel");
	ZENITH_ASSERT_EQ_FLOAT(xMove.y, 0.0f, fTEST_EPSILON, "S/Down and Up cancel");

	// The pure rule the engine exposes IS the rule this game's readers get.
	const Zenith_Maths::Vector2 xResolved =
		Zenith_InputActions::ResolveMoveComposite(false, true, true, false);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.x, -1.0f, fTEST_EPSILON);
	ZENITH_ASSERT_EQ_FLOAT(xResolved.y, -1.0f, fTEST_EPSILON);
}

ZENITH_TEST(ZM_Bindings, ConfirmCancelAndMenuFireExactlyOneEdgePerPress)
{
	ZM_BindingsTest::Rig xRig;
	xRig.m_xActions.SetProfileOverride(ZM_Bindings::uPROFILE_KEYBOARD);

	struct EdgeCase
	{
		Zenith_InputActionID m_uAction;
		int32_t m_iFirstKey;
		int32_t m_iSecondKey;
		const char* m_szName;
	};
	const EdgeCase axCases[] =
	{
		{ ZM_Bindings::ZM_ACTION_CONFIRM, ZENITH_KEY_ENTER,  ZENITH_KEY_SPACE,     "CONFIRM" },
		{ ZM_Bindings::ZM_ACTION_CANCEL,  ZENITH_KEY_ESCAPE, ZENITH_KEY_BACKSPACE, "CANCEL"  },
		{ ZM_Bindings::ZM_ACTION_MENU,    ZENITH_KEY_M,      ZENITH_KEY_TAB,       "MENU"    },
	};

	for (const EdgeCase& xCase : axCases)
	{
		xRig.BeginFrame();
		xRig.KeyDown(xCase.m_iFirstKey);
		xRig.CloseFrame();
		ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(xCase.m_uAction),
			"%s did not fire on its first key", xCase.m_szName);

		// Held, not re-pressed: one press is one menu step.
		xRig.EmptyFrame();
		ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(xCase.m_uAction),
			"%s repeated its edge while the key stayed held", xCase.m_szName);

		// The ALTERNATE key rising while the first is still held must fire
		// NOTHING: the aggregate never fell, so there is no rise to report.
		xRig.BeginFrame();
		xRig.KeyDown(xCase.m_iSecondKey);
		xRig.CloseFrame();
		ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(xCase.m_uAction),
			"%s double-fired when its second bound key joined the first", xCase.m_szName);

		xRig.BeginFrame();
		xRig.KeyUp(xCase.m_iFirstKey);
		xRig.KeyUp(xCase.m_iSecondKey);
		xRig.CloseFrame();
		ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(xCase.m_uAction),
			"%s did not release when both keys let go", xCase.m_szName);

		// ...and the alternate key alone fires it again from rest.
		xRig.BeginFrame();
		xRig.KeyDown(xCase.m_iSecondKey);
		xRig.CloseFrame();
		ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(xCase.m_uAction),
			"%s did not fire on its alternate key", xCase.m_szName);
		xRig.BeginFrame();
		xRig.KeyUp(xCase.m_iSecondKey);
		xRig.CloseFrame();
	}
}

ZENITH_TEST(ZM_Bindings, RunIsAHeldReadAndMenuVerticalIsAPerPressStep)
{
	ZM_BindingsTest::Rig xRig;
	xRig.m_xActions.SetProfileOverride(ZM_Bindings::uPROFILE_KEYBOARD);

	// RUN is a LEVEL: the speed selector samples it every frame, so it must stay
	// true for as long as either shift is down and go false the frame it is not.
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_LEFT_SHIFT);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(ZM_Bindings::ZM_ACTION_RUN));
	xRig.EmptyFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(ZM_Bindings::ZM_ACTION_RUN),
		"RUN must stay held across frames -- it is not an edge");
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_RIGHT_SHIFT);
	xRig.KeyUp(ZENITH_KEY_LEFT_SHIFT);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(ZM_Bindings::ZM_ACTION_RUN),
		"releasing one shift while the other is down must not stop the run");
	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_RIGHT_SHIFT);
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(ZM_Bindings::ZM_ACTION_RUN));

	// The battle cursor: -1 up, +1 down, and BOTH in one frame cancels, exactly as
	// the deleted ReadMenuVertical did.
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_UP);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_MENU_UP));
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_MENU_DOWN));

	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_UP);
	xRig.CloseFrame();

	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_UP);
	xRig.KeyDown(ZENITH_KEY_DOWN);
	xRig.CloseFrame();
	const bool bUp   = xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_MENU_UP);
	const bool bDown = xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_MENU_DOWN);
	ZENITH_ASSERT_EQ((bDown ? 1 : 0) - (bUp ? 1 : 0), 0,
		"an up and a down in the same frame must cancel to a zero step");
}

// -----------------------------------------------------------------------------
// Mask / profile invariants
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_Bindings, TouchProfileMasksOutKeyboardRowsAndEnablesVirtualOnes)
{
	ZM_BindingsTest::Rig xRig;
	xRig.m_xActions.SetProfileOverride(ZM_Bindings::uPROFILE_TOUCH);
	ZENITH_ASSERT_EQ((u_int)xRig.m_xActions.GetActiveSchemeMask(), (u_int)uINPUT_SCHEME_MASK_TOUCH);

	// A key press under P_TOUCH resolves nothing: the row's column is not in the
	// mask. (The engine ignores the key for ACTIONS only -- the device layer still
	// sees it, which is what keeps an editor shortcut working.)
	xRig.BeginFrame();
	xRig.KeyDown(ZENITH_KEY_ESCAPE);
	xRig.CloseFrame();
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_CANCEL),
		"a keyboard row must be dead while the touch profile is active");
	xRig.BeginFrame();
	xRig.KeyUp(ZENITH_KEY_ESCAPE);
	xRig.CloseFrame();

	// ...while the on-screen stick's VIRTUAL source drives MOVE.
	//
	// ★ THE PUBLISH GOES BETWEEN OpenActionFrame AND CloseStages, because a real
	// widget publishes at 10d -- AFTER step 8 -- and step 8 clears the virtual
	// transition queue. See ZM_BindingsTestRig.h.
	xRig.BeginFrame();
	xRig.OpenActionFrame();
	xRig.m_xActions.PublishVirtualButton(ZM_Bindings::uVIRTUAL_MOVE, true);
	xRig.m_xActions.PublishVirtualAxis(ZM_Bindings::uVIRTUAL_MOVE, 0.5f, 0.75f);
	xRig.CloseStages();
	Zenith_Maths::Vector2 xMove = xRig.Move();
	ZENITH_ASSERT_EQ_FLOAT(xMove.x, 0.5f,  fTEST_EPSILON, "the stick's x reaches MOVE");
	ZENITH_ASSERT_EQ_FLOAT(xMove.y, 0.75f, fTEST_EPSILON, "the stick's y reaches MOVE");

	// A button source publishes an ordered transition, so a tap fires both edges.
	xRig.BeginFrame();
	xRig.OpenActionFrame();
	xRig.m_xActions.PublishVirtualButton(ZM_Bindings::uVIRTUAL_INTERACT, true);
	xRig.m_xActions.PublishVirtualButton(ZM_Bindings::uVIRTUAL_INTERACT, false);
	xRig.CloseStages();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_INTERACT),
		"a same-frame virtual tap presses");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(ZM_Bindings::ZM_ACTION_INTERACT),
		"...and releases");
	// The action that SHARES the physical button in another context must stay
	// silent: the two carry different virtual sources by construction.
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_CONFIRM),
		"the A button's other context must not fire alongside INTERACT");
}

ZENITH_TEST(ZM_Bindings, SystemBackFiresCancelUnderEveryProfile)
{
	const u_int8 auProfiles[] =
	{
		ZM_Bindings::uPROFILE_KEYBOARD,
		ZM_Bindings::uPROFILE_TOUCH,
		ZM_Bindings::uPROFILE_GAMEPAD,
	};

	for (u_int u = 0u; u < 3u; ++u)
	{
		ZM_BindingsTest::Rig xRig;
		xRig.m_xActions.SetProfileOverride(auProfiles[u]);

		xRig.BeginFrame();
		xRig.SystemBack();
		xRig.CloseFrame();

		// The gesture has no held phase at all: the replay PULSES it, so the frame
		// carries both edges and nothing is left held afterwards.
		ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_CANCEL),
			"system Back must cancel under profile %u", (u_int)auProfiles[u]);
		ZENITH_ASSERT_FALSE(xRig.m_xActions.IsHeld(ZM_Bindings::ZM_ACTION_CANCEL),
			"the Back gesture has no held phase");

		// ...and it is SYSTEM navigation, so it must not be mistaken for the player
		// reaching for a device: the profile is unchanged.
		ZENITH_ASSERT_EQ((u_int)xRig.m_xActions.GetActiveProfile(), (u_int)auProfiles[u],
			"a system gesture must not move the active profile");
	}
}

ZENITH_TEST(ZM_Bindings, SimulatedGamepadDrivesMoveAndConfirmEndToEnd)
{
#ifdef ZENITH_INPUT_SIMULATOR
	ZM_BindingsTest::SimScope xSimScope;
	ZM_BindingsTest::Rig xRig;

	// No override: the pad has to WIN the auto-switch on its own activity, which
	// is the half of the pad story a forced profile would hide.
	xRig.BeginFrame();
	Zenith_InputSimulator::SimulateGamepadConnected(true, 0);
	Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 0.5f, -1.0f, 0);
	Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_A, 0);
	ZM_BindingsTest::DrainSimulatorInjections(xRig);
	xRig.CloseFrame();

	ZENITH_ASSERT_EQ((u_int)xRig.m_xActions.GetActiveProfile(), (u_int)ZM_Bindings::uPROFILE_GAMEPAD,
		"pad activity must auto-switch this game into P_GAMEPAD");

	const Zenith_Maths::Vector2 xMove = xRig.Move();
	ZENITH_ASSERT_EQ_FLOAT(xMove.x, 0.5f, fTEST_EPSILON, "the pad stick's x reaches MOVE");
	// -1 on the device axis is a FORWARD push, and the row's y inversion turns it
	// into the engine's +y forward.
	ZENITH_ASSERT_EQ_FLOAT(xMove.y, 1.0f, fTEST_EPSILON,
		"a forward stick push must read +y after the row's inversion");

	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_CONFIRM),
		"pad A must confirm");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.IsHeld(ZM_Bindings::ZM_ACTION_CONFIRM));
	// Pad A is CONFIRM, pad X is INTERACT: pressing one must not fire the other.
	ZENITH_ASSERT_FALSE(xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_INTERACT));

	xRig.BeginFrame();
	Zenith_InputSimulator::SimulateGamepadButtonUp(ZENITH_GAMEPAD_BUTTON_A, 0);
	ZM_BindingsTest::DrainSimulatorInjections(xRig);
	xRig.CloseFrame();
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasReleasedThisFrame(ZM_Bindings::ZM_ACTION_CONFIRM));

	// The d-pad composite is the pad's OTHER move row, and it must agree with the
	// keyboard composite's convention rather than invent its own.
	xRig.BeginFrame();
	Zenith_InputSimulator::SimulateGamepadStick(ZENITH_GAMEPAD_AXIS_LEFT_X, 0.0f, 0.0f, 0);
	Zenith_InputSimulator::SimulateGamepadButtonDown(ZENITH_GAMEPAD_BUTTON_DPAD_UP, 0);
	ZM_BindingsTest::DrainSimulatorInjections(xRig);
	xRig.CloseFrame();
	const Zenith_Maths::Vector2 xDpad = xRig.Move();
	ZENITH_ASSERT_EQ_FLOAT(xDpad.y, 1.0f, fTEST_EPSILON, "d-pad up is +y FORWARD");
	ZENITH_ASSERT_TRUE(xRig.m_xActions.WasPressedThisFrame(ZM_Bindings::ZM_ACTION_MENU_UP),
		"...and the same press steps the battle cursor up");
#else
	ZENITH_SKIP("input simulator is unavailable in this configuration");
#endif
}

// -----------------------------------------------------------------------------
// B11 HUD contexts (the pure half of ZM_TouchLayoutController)
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_TouchLayout, ContextResolutionRanksBattleThenDialogueThenTitle)
{
	// Nothing open -> the world.
	ZENITH_ASSERT_EQ(
		(u_int)ZM_TouchLayoutController::ResolveContext(false, false, ZM_MENU_SCREEN_NONE),
		(u_int)ZM_TOUCH_CONTEXT_OVERWORLD);

	// A battle round trip owns the screen from enter to exit, INCLUDING both
	// fades and any menu that was open when it started -- otherwise the overworld
	// stick would still be live over an arena that is fading in.
	ZENITH_ASSERT_EQ(
		(u_int)ZM_TouchLayoutController::ResolveContext(true, false, ZM_MENU_SCREEN_NONE),
		(u_int)ZM_TOUCH_CONTEXT_BATTLE);
	ZENITH_ASSERT_EQ(
		(u_int)ZM_TouchLayoutController::ResolveContext(true, true, ZM_MENU_SCREEN_DIALOGUE),
		(u_int)ZM_TOUCH_CONTEXT_BATTLE, "a battle outranks a raised dialogue");

	// A dialogue stacks ON TOP of whatever menu screen raised it, so the TOP
	// screen is what decides -- not the depth.
	ZENITH_ASSERT_EQ(
		(u_int)ZM_TouchLayoutController::ResolveContext(false, true, ZM_MENU_SCREEN_DIALOGUE),
		(u_int)ZM_TOUCH_CONTEXT_DIALOGUE);
	ZENITH_ASSERT_EQ(
		(u_int)ZM_TouchLayoutController::ResolveContext(false, true, ZM_MENU_SCREEN_TITLE),
		(u_int)ZM_TOUCH_CONTEXT_TITLE);

	// Every other open screen is an ordinary menu.
	const ZM_MENU_SCREEN aeMenuScreens[] =
	{
		ZM_MENU_SCREEN_ROOT, ZM_MENU_SCREEN_PARTY, ZM_MENU_SCREEN_BAG,
		ZM_MENU_SCREEN_DEX, ZM_MENU_SCREEN_SHOP, ZM_MENU_SCREEN_SAVE,
	};
	for (const ZM_MENU_SCREEN eScreen : aeMenuScreens)
	{
		ZENITH_ASSERT_EQ(
			(u_int)ZM_TouchLayoutController::ResolveContext(false, true, eScreen),
			(u_int)ZM_TOUCH_CONTEXT_MENU, "screen %u is an ordinary menu", (u_int)eScreen);
	}

	// A closed stack is the overworld whatever stale screen id came with it: the
	// "is it open" bool leads, because that is the one the menu machine maintains.
	ZENITH_ASSERT_EQ(
		(u_int)ZM_TouchLayoutController::ResolveContext(false, false, ZM_MENU_SCREEN_DIALOGUE),
		(u_int)ZM_TOUCH_CONTEXT_OVERWORLD);
}

ZENITH_TEST(ZM_TouchLayout, EachContextGivesOneSemanticsPerButton)
{
	// OVERWORLD is the only context with a stick or a menu button, and the only
	// one where A talks and B runs (B11).
	const ZM_TouchLayout xOverworld =
		ZM_TouchLayoutController::LayoutForContext(ZM_TOUCH_CONTEXT_OVERWORLD);
	ZENITH_ASSERT_STREQ(xOverworld.m_szStickAction,      ZM_Bindings::szACTION_MOVE);
	ZENITH_ASSERT_STREQ(xOverworld.m_szButtonAAction,    ZM_Bindings::szACTION_INTERACT);
	ZENITH_ASSERT_STREQ(xOverworld.m_szButtonBAction,    ZM_Bindings::szACTION_RUN);
	ZENITH_ASSERT_STREQ(xOverworld.m_szButtonMenuAction, ZM_Bindings::szACTION_MENU);

	// DIALOGUE / MENU / BATTLE share one semantics: A confirms, B cancels, no
	// stick (there is nowhere to walk) and no menu button (you are in it).
	const ZM_TOUCH_CONTEXT aeConfirmCancel[] =
	{
		ZM_TOUCH_CONTEXT_DIALOGUE, ZM_TOUCH_CONTEXT_MENU, ZM_TOUCH_CONTEXT_BATTLE,
	};
	for (const ZM_TOUCH_CONTEXT eContext : aeConfirmCancel)
	{
		const ZM_TouchLayout xLayout = ZM_TouchLayoutController::LayoutForContext(eContext);
		ZENITH_ASSERT_NULL(xLayout.m_szStickAction,
			"%s must hide the stick", ZM_TouchLayoutController::ContextName(eContext));
		ZENITH_ASSERT_STREQ(xLayout.m_szButtonAAction, ZM_Bindings::szACTION_CONFIRM);
		ZENITH_ASSERT_STREQ(xLayout.m_szButtonBAction, ZM_Bindings::szACTION_CANCEL);
		ZENITH_ASSERT_NULL(xLayout.m_szButtonMenuAction,
			"%s must hide the menu button", ZM_TouchLayoutController::ContextName(eContext));
	}

	// TITLE is the base screen and cannot pop to nothing, so it deliberately
	// offers CONFIRM alone -- matching ZM_UI_MenuStack, which ignores a cancel
	// there. A B button that did nothing would be worse than no B button.
	const ZM_TouchLayout xTitle =
		ZM_TouchLayoutController::LayoutForContext(ZM_TOUCH_CONTEXT_TITLE);
	ZENITH_ASSERT_NULL(xTitle.m_szStickAction);
	ZENITH_ASSERT_STREQ(xTitle.m_szButtonAAction, ZM_Bindings::szACTION_CONFIRM);
	ZENITH_ASSERT_NULL(xTitle.m_szButtonBAction, "the title screen has nothing to cancel to");
	ZENITH_ASSERT_NULL(xTitle.m_szButtonMenuAction);

	// An out-of-range context folds to OVERWORLD rather than leaving the player
	// with a HUD of nothing.
	const ZM_TouchLayout xFallback =
		ZM_TouchLayoutController::LayoutForContext((ZM_TOUCH_CONTEXT)ZM_TOUCH_CONTEXT_COUNT);
	ZENITH_ASSERT_STREQ(xFallback.m_szStickAction, ZM_Bindings::szACTION_MOVE);

	// Every context has a name of its own, so a failure message can localise.
	for (u_int u = 0u; u < (u_int)ZM_TOUCH_CONTEXT_COUNT; ++u)
	{
		const char* szName = ZM_TouchLayoutController::ContextName((ZM_TOUCH_CONTEXT)u);
		ZENITH_ASSERT_NOT_NULL(szName);
		for (u_int v = u + 1u; v < (u_int)ZM_TOUCH_CONTEXT_COUNT; ++v)
		{
			ZENITH_ASSERT_TRUE(
				std::strcmp(szName, ZM_TouchLayoutController::ContextName((ZM_TOUCH_CONTEXT)v)) != 0,
				"contexts %u and %u share a name", u, v);
		}
	}
}

ZENITH_TEST(ZM_TouchLayout, EveryLayoutTargetIsARegisteredActionWithAVirtualRow)
{
	ZM_BindingsTest::Rig xRig;

	// ★ THE SEAM THAT CANNOT BE PROVED BY EITHER SIDE ALONE. A widget resolves its
	// publish source by looking up its action NAME and walking that action's
	// VIRTUAL row. A layout naming an action that has no virtual row therefore
	// produces a control that claims the finger and publishes NOTHING -- silently,
	// with no assert and no log. This walks every context's four targets through
	// exactly the lookup Zenith_UIVirtualStick/Button perform.
	for (u_int uContext = 0u; uContext < (u_int)ZM_TOUCH_CONTEXT_COUNT; ++uContext)
	{
		const ZM_TouchLayout xLayout =
			ZM_TouchLayoutController::LayoutForContext((ZM_TOUCH_CONTEXT)uContext);
		const char* aszTargets[4] =
		{
			xLayout.m_szStickAction, xLayout.m_szButtonAAction,
			xLayout.m_szButtonBAction, xLayout.m_szButtonMenuAction
		};
		for (u_int uTarget = 0u; uTarget < 4u; ++uTarget)
		{
			if (aszTargets[uTarget] == nullptr)
			{
				continue;
			}
			const Zenith_InputActionID uAction =
				xRig.m_xActions.FindActionByName(aszTargets[uTarget]);
			ZENITH_ASSERT_NE((u_int)uAction, (u_int)uINPUT_ACTION_INVALID,
				"%s target '%s' names no registered action",
				ZM_TouchLayoutController::ContextName((ZM_TOUCH_CONTEXT)uContext),
				aszTargets[uTarget]);
			if (uAction == uINPUT_ACTION_INVALID)
			{
				continue;
			}
			ZENITH_ASSERT_EQ(CountBindingsOfType(xRig.m_xActions, uAction, INPUT_BINDING_VIRTUAL), 1u,
				"'%s' needs exactly one VIRTUAL row for an on-screen control to publish into",
				aszTargets[uTarget]);
			// The stick slot needs an AXIS2D and the button slots need BUTTONs.
			const Zenith_EInputActionKind eKind = xRig.m_xActions.GetActionKind(uAction);
			ZENITH_ASSERT_EQ((u_int)eKind,
				(u_int)(uTarget == 0u ? INPUT_ACTION_AXIS2D : INPUT_ACTION_BUTTON),
				"'%s' is the wrong value kind for the control that targets it",
				aszTargets[uTarget]);
		}
	}
}
