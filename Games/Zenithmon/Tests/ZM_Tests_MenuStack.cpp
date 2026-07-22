#include "Zenith.h"

// ============================================================================
// ZM_Tests_MenuStack -- S6 item 2 (SC1) unit tests for ZM_UI_MenuStack, the
// overworld pause-menu machine. Everything here is PURE: no ECS, no scene, no
// graphics, no baked assets -- just the value-typed screen stack and the static
// decision surface (the gating predicate, the scene-kind test, the focused-name
// -> action resolver, the root-item name round trip, the action -> screen map).
// Every fixture is deterministic and hermetic, so no RequestSkip is needed.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"   // ZM_SCENE_KIND (IsOverworldSceneKind fixtures)

// ---- ZM_MenuScreenStack: push / pop / top / depth / clear -------------------

ZENITH_TEST(ZM_MenuStack, Stack_StartsEmpty)
{
	ZM_MenuScreenStack xStack;
	ZENITH_ASSERT_TRUE(xStack.IsEmpty(), "a fresh stack must be empty");
	ZENITH_ASSERT_EQ(xStack.GetDepth(), 0u, "a fresh stack has depth 0");
	ZENITH_ASSERT_EQ((u_int)xStack.Top(), (u_int)ZM_MENU_SCREEN_NONE,
		"an empty stack's Top must be the NONE sentinel");
}

ZENITH_TEST(ZM_MenuStack, Stack_PushSetsTopAndDepth)
{
	ZM_MenuScreenStack xStack;
	ZENITH_ASSERT_TRUE(xStack.Push(ZM_MENU_SCREEN_ROOT), "pushing a real screen succeeds");
	ZENITH_ASSERT_FALSE(xStack.IsEmpty(), "after a push the stack is non-empty");
	ZENITH_ASSERT_EQ(xStack.GetDepth(), 1u, "one push -> depth 1");
	ZENITH_ASSERT_EQ((u_int)xStack.Top(), (u_int)ZM_MENU_SCREEN_ROOT, "Top is the pushed screen");

	ZENITH_ASSERT_TRUE(xStack.Push(ZM_MENU_SCREEN_PARTY), "second push succeeds");
	ZENITH_ASSERT_EQ(xStack.GetDepth(), 2u, "two pushes -> depth 2");
	ZENITH_ASSERT_EQ((u_int)xStack.Top(), (u_int)ZM_MENU_SCREEN_PARTY,
		"Top is the most-recently pushed screen (LIFO)");
}

ZENITH_TEST(ZM_MenuStack, Stack_PopRestoresLowerScreen)
{
	ZM_MenuScreenStack xStack;
	xStack.Push(ZM_MENU_SCREEN_ROOT);
	xStack.Push(ZM_MENU_SCREEN_DEX);
	ZENITH_ASSERT_TRUE(xStack.Pop(), "popping a non-empty stack succeeds");
	ZENITH_ASSERT_EQ(xStack.GetDepth(), 1u, "pop drops depth by one");
	ZENITH_ASSERT_EQ((u_int)xStack.Top(), (u_int)ZM_MENU_SCREEN_ROOT,
		"pop exposes the lower screen (ROOT)");
	ZENITH_ASSERT_TRUE(xStack.Pop(), "second pop succeeds");
	ZENITH_ASSERT_TRUE(xStack.IsEmpty(), "popping the last screen empties the stack");
}

ZENITH_TEST(ZM_MenuStack, Stack_PopEmptyFails)
{
	ZM_MenuScreenStack xStack;
	ZENITH_ASSERT_FALSE(xStack.Pop(), "popping an empty stack must fail");
	ZENITH_ASSERT_EQ(xStack.GetDepth(), 0u, "a failed pop leaves depth 0");
}

ZENITH_TEST(ZM_MenuStack, Stack_ClearEmpties)
{
	ZM_MenuScreenStack xStack;
	xStack.Push(ZM_MENU_SCREEN_ROOT);
	xStack.Push(ZM_MENU_SCREEN_BAG);
	xStack.Clear();
	ZENITH_ASSERT_TRUE(xStack.IsEmpty(), "Clear empties the stack");
	ZENITH_ASSERT_EQ((u_int)xStack.Top(), (u_int)ZM_MENU_SCREEN_NONE, "cleared Top is NONE");
}

ZENITH_TEST(ZM_MenuStack, Stack_RejectsNoneScreen)
{
	ZM_MenuScreenStack xStack;
	ZENITH_ASSERT_FALSE(xStack.Push(ZM_MENU_SCREEN_NONE),
		"the NONE sentinel is never a valid stored screen");
	ZENITH_ASSERT_TRUE(xStack.IsEmpty(), "a rejected push leaves the stack empty");
}

ZENITH_TEST(ZM_MenuStack, Stack_RespectsCapacity)
{
	ZM_MenuScreenStack xStack;
	for (u_int i = 0u; i < ZM_MenuScreenStack::uMAX_DEPTH; ++i)
	{
		ZENITH_ASSERT_TRUE(xStack.Push(ZM_MENU_SCREEN_ROOT),
			"pushes up to capacity all succeed");
	}
	ZENITH_ASSERT_EQ(xStack.GetDepth(), ZM_MenuScreenStack::uMAX_DEPTH,
		"the stack fills exactly to capacity");
	ZENITH_ASSERT_FALSE(xStack.Push(ZM_MENU_SCREEN_ROOT),
		"a push past capacity fails");
	ZENITH_ASSERT_EQ(xStack.GetDepth(), ZM_MenuScreenStack::uMAX_DEPTH,
		"a full-stack push does not grow the stack");
}

// ---- ShouldOpenMenu: the overworld pause-open gate --------------------------

ZENITH_TEST(ZM_MenuStack, ShouldOpen_AllConditionsMet)
{
	ZENITH_ASSERT_TRUE(ZM_UI_MenuStack::ShouldOpenMenu(
		/* menuKey */ true, /* alreadyOpen */ false, /* overworld */ true,
		/* warp */ false, /* battle */ false),
		"key pressed + closed + overworld + no warp + no battle -> open");
}

ZENITH_TEST(ZM_MenuStack, ShouldOpen_RequiresKeyPress)
{
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::ShouldOpenMenu(
		false, false, true, false, false),
		"no menu key -> never opens");
}

ZENITH_TEST(ZM_MenuStack, ShouldOpen_NotWhenAlreadyOpen)
{
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::ShouldOpenMenu(
		true, /* alreadyOpen */ true, true, false, false),
		"already-open never re-opens");
}

ZENITH_TEST(ZM_MenuStack, ShouldOpen_RequiresOverworld)
{
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::ShouldOpenMenu(
		true, false, /* overworld */ false, false, false),
		"non-overworld (title / battle scene) never opens the pause menu");
}

ZENITH_TEST(ZM_MenuStack, ShouldOpen_BlockedByWarp)
{
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::ShouldOpenMenu(
		true, false, true, /* warp */ true, false),
		"a warp in progress owns the screen -> no menu");
}

ZENITH_TEST(ZM_MenuStack, ShouldOpen_BlockedByBattleTransition)
{
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::ShouldOpenMenu(
		true, false, true, false, /* battle */ true),
		"a battle transition owns the screen -> no menu");
}

// ---- IsOverworldSceneKind ---------------------------------------------------

ZENITH_TEST(ZM_MenuStack, SceneKind_FrontEndAndBattleAreNotOverworld)
{
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::IsOverworldSceneKind(ZM_SCENE_KIND_FRONTEND),
		"the title screen is not an overworld");
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::IsOverworldSceneKind(ZM_SCENE_KIND_BATTLE),
		"the additive battle scene is not an overworld");
}

ZENITH_TEST(ZM_MenuStack, SceneKind_TownRouteInteriorGymAreOverworld)
{
	ZENITH_ASSERT_TRUE(ZM_UI_MenuStack::IsOverworldSceneKind(ZM_SCENE_KIND_TOWN),
		"a town is an overworld the menu opens over");
	ZENITH_ASSERT_TRUE(ZM_UI_MenuStack::IsOverworldSceneKind(ZM_SCENE_KIND_ROUTE),
		"a route is an overworld");
	ZENITH_ASSERT_TRUE(ZM_UI_MenuStack::IsOverworldSceneKind(ZM_SCENE_KIND_INTERIOR),
		"a building interior is an overworld");
	ZENITH_ASSERT_TRUE(ZM_UI_MenuStack::IsOverworldSceneKind(ZM_SCENE_KIND_GYM),
		"a gym interior is an overworld");
}

// ---- ResolveRootAction: focused-element-NAME -> action ----------------------

ZENITH_TEST(ZM_MenuStack, ResolveAction_EachRootEntry)
{
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::ResolveRootAction(ZM_UI_MenuStack::szROOT_PARTY_NAME),
		(u_int)ZM_MENU_ACTION_OPEN_PARTY, "the Party entry resolves to OPEN_PARTY");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::ResolveRootAction(ZM_UI_MenuStack::szROOT_BAG_NAME),
		(u_int)ZM_MENU_ACTION_OPEN_BAG, "the Bag entry resolves to OPEN_BAG");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::ResolveRootAction(ZM_UI_MenuStack::szROOT_DEX_NAME),
		(u_int)ZM_MENU_ACTION_OPEN_DEX, "the Dex entry resolves to OPEN_DEX");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::ResolveRootAction(ZM_UI_MenuStack::szROOT_EXIT_NAME),
		(u_int)ZM_MENU_ACTION_CLOSE, "the Exit entry resolves to CLOSE");
}

ZENITH_TEST(ZM_MenuStack, ResolveAction_NullAndUnknownAreNone)
{
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::ResolveRootAction(nullptr),
		(u_int)ZM_MENU_ACTION_NONE, "a null focused name resolves to NONE (never dispatched)");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::ResolveRootAction("Menu_NotAnEntry"),
		(u_int)ZM_MENU_ACTION_NONE, "an unknown element name resolves to NONE");
}

// ---- RootItemElementName <-> RootItemIndexFromElementName round trip --------

ZENITH_TEST(ZM_MenuStack, RootItemNames_RoundTrip)
{
	for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
	{
		const char* szName = ZM_UI_MenuStack::RootItemElementName(static_cast<ZM_MENU_ROOT_ITEM>(i));
		ZENITH_ASSERT_EQ(ZM_UI_MenuStack::RootItemIndexFromElementName(szName), (int)i,
			"a root item's element name maps back to its own index");
	}
}

ZENITH_TEST(ZM_MenuStack, RootItemIndex_NullAndUnknownAreNegative)
{
	ZENITH_ASSERT_EQ(ZM_UI_MenuStack::RootItemIndexFromElementName(nullptr), -1,
		"a null name is not a root item");
	ZENITH_ASSERT_EQ(ZM_UI_MenuStack::RootItemIndexFromElementName(ZM_UI_MenuStack::szROOT_PANEL_NAME), -1,
		"the backing panel is not a selectable root item");
}

// ---- RootActionToScreen -----------------------------------------------------

ZENITH_TEST(ZM_MenuStack, ActionToScreen_OpensPushPlaceholders)
{
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::RootActionToScreen(ZM_MENU_ACTION_OPEN_PARTY),
		(u_int)ZM_MENU_SCREEN_PARTY, "OPEN_PARTY pushes the PARTY screen");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::RootActionToScreen(ZM_MENU_ACTION_OPEN_BAG),
		(u_int)ZM_MENU_SCREEN_BAG, "OPEN_BAG pushes the BAG screen");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::RootActionToScreen(ZM_MENU_ACTION_OPEN_DEX),
		(u_int)ZM_MENU_SCREEN_DEX, "OPEN_DEX pushes the DEX screen");
}

ZENITH_TEST(ZM_MenuStack, ActionToScreen_CloseAndNonePushNothing)
{
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::RootActionToScreen(ZM_MENU_ACTION_CLOSE),
		(u_int)ZM_MENU_SCREEN_NONE, "CLOSE pushes nothing");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::RootActionToScreen(ZM_MENU_ACTION_NONE),
		(u_int)ZM_MENU_SCREEN_NONE, "NONE pushes nothing");
}

// ---- Screen enum sanity -----------------------------------------------------

ZENITH_TEST(ZM_MenuStack, ScreenEnum_NoneIsZeroAndRootDistinct)
{
	ZENITH_ASSERT_EQ((u_int)ZM_MENU_SCREEN_NONE, 0u, "the NONE sentinel is 0");
	ZENITH_ASSERT_NE((u_int)ZM_MENU_SCREEN_ROOT, (u_int)ZM_MENU_SCREEN_NONE,
		"ROOT is a distinct, real screen");
	ZENITH_ASSERT_TRUE((u_int)ZM_MENU_SCREEN_ROOT < (u_int)ZM_MENU_SCREEN_COUNT,
		"ROOT is within the enumerated range");
}

// ---- IsMenuOpen without a singleton (S6 item 3 SC1) -------------------------

ZENITH_TEST(ZM_MenuStack, MenuStack_IsMenuOpenIsFalseWithoutSingleton)
{
	// Units run at boot, BEFORE any scene loads, so no ZM_UI_MenuStack singleton
	// exists here. The static must answer a plain "no" rather than assert or
	// dereference an unresolved singleton -- the interaction gate (SC2+) calls it
	// from a free context that has no way to know whether a menu entity exists.
	//
	// Pin the PRECONDITION first: IsMenuOpen returns false on BOTH the unresolved
	// branch and the resolved-but-closed branch, so without this the unit would
	// still pass while exercising a completely different code path from the one it
	// is named for -- and it would stop being able to fail for its stated reason.
	Zenith_EntityID xUnresolved = INVALID_ENTITY_ID;
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xUnresolved),
		"precondition: boot units run before any scene, so no ZM_MenuRoot may resolve");

	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::IsMenuOpen(),
		"IsMenuOpen must be a skip-safe false when no menu singleton resolves");
}

// ---- TryOpenSaveScreen without a singleton (S7 item 2 SC4 forward seam) ------

ZENITH_TEST(ZM_MenuStack, MenuStack_TryOpenSaveScreenRefusesWithoutSingleton)
{
	// TryOpenSaveScreen is the static seam SC5's title-screen "Continue"/LOAD path will
	// call; it has NO caller yet, so this pins its refusal contract now -- the consistent
	// parallel of the sibling TryPushDialogue / TryOpenShop without-singleton units. Boot
	// units run BEFORE any scene loads, so no ZM_UI_MenuStack singleton has claimed itself:
	// the static must resolve nothing and return a clean false (its body bails at the
	// TryGetUniqueSingletonEntityID guard, never dereferencing a stale id or OpenSaveScreen).
	//
	// Pin the PRECONDITION first, exactly as MenuStack_IsMenuOpenIsFalseWithoutSingleton
	// does: without it the unit would still pass even if TryOpenSaveScreen returned false
	// for some UNRELATED reason, and it would stop being able to fail for its stated one --
	// the day a live singleton makes TryGetUniqueSingletonEntityID succeed here, THIS assert
	// is what flags that the test is no longer exercising the refusal branch.
	Zenith_EntityID xUnresolved = INVALID_ENTITY_ID;
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xUnresolved),
		"precondition: boot units run before any scene, so no ZM_MenuRoot may resolve");

	// A VALID mode (SAVE) so the false is proven to come from the MISSING SINGLETON, not
	// from an out-of-range mode folding away. Reds if TryOpenSaveScreen ever reports success
	// (or drives a menu) with no live singleton -- e.g. if its TryGetUniqueSingletonEntityID
	// guard is dropped and it resolves an INVALID_ENTITY_ID entity.
	ZENITH_ASSERT_FALSE(ZM_UI_MenuStack::TryOpenSaveScreen(ZM_SAVE_SCREEN_MODE_SAVE),
		"TryOpenSaveScreen reports failure when there is no live ZM_UI_MenuStack singleton");
}

// ============================================================================
// S7 item 2 SC4 -- the ROOT Save / Quit entries and the visual==enum order rule.
//
// SC4 inserts SAVE (3) and QUIT (4) into ZM_MENU_ROOT_ITEM *before* EXIT (which
// moves 3 -> 5), because the nav chain is built in axEntries (visual) order while
// RootItemIndexFromElementName returns the enum ordinal, and ZM_AutoTests_UI.cpp's
// focus walk presses UP/DOWN by comparing those two. Enum order MUST equal the
// authored visual order or that walk oscillates until its deadline. These four
// units pin the new resolver arms and that ordering invariant.
// ============================================================================

ZENITH_TEST(ZM_MenuStack, MenuStack_ResolveRootActionMapsSaveAndQuit)
{
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::ResolveRootAction(ZM_UI_MenuStack::szROOT_SAVE_NAME),
		(u_int)ZM_MENU_ACTION_OPEN_SAVE, "the Save entry resolves to OPEN_SAVE");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::ResolveRootAction(ZM_UI_MenuStack::szROOT_QUIT_NAME),
		(u_int)ZM_MENU_ACTION_QUIT_TO_TITLE, "the Quit entry resolves to QUIT_TO_TITLE");
	// The two must not collide with the pre-existing entries, or an old entry becomes
	// inert while the new one shadows it.
	ZENITH_ASSERT_NE((u_int)ZM_MENU_ACTION_OPEN_SAVE, (u_int)ZM_MENU_ACTION_QUIT_TO_TITLE,
		"OPEN_SAVE and QUIT_TO_TITLE are distinct actions");
	ZENITH_ASSERT_NE((u_int)ZM_UI_MenuStack::ResolveRootAction(ZM_UI_MenuStack::szROOT_SAVE_NAME),
		(u_int)ZM_UI_MenuStack::ResolveRootAction(ZM_UI_MenuStack::szROOT_EXIT_NAME),
		"Save does not resolve to the same action as Exit");
}

ZENITH_TEST(ZM_MenuStack, MenuStack_RootActionToScreenMapsOpenSaveAndNotQuit)
{
	// OPEN_SAVE pushes the SAVE screen; QUIT_TO_TITLE is an ACTION like CLOSE and
	// pushes NOTHING (it runs quit-to-title, it does not open a screen). Reds if quit
	// is made to push a screen.
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::RootActionToScreen(ZM_MENU_ACTION_OPEN_SAVE),
		(u_int)ZM_MENU_SCREEN_SAVE, "OPEN_SAVE pushes the SAVE screen");
	ZENITH_ASSERT_EQ((u_int)ZM_UI_MenuStack::RootActionToScreen(ZM_MENU_ACTION_QUIT_TO_TITLE),
		(u_int)ZM_MENU_SCREEN_NONE, "QUIT_TO_TITLE pushes nothing -- it is an action, not a screen");
	// The SAVE screen must be a real, distinct screen.
	ZENITH_ASSERT_NE((u_int)ZM_MENU_SCREEN_SAVE, (u_int)ZM_MENU_SCREEN_NONE,
		"SAVE is a distinct, real screen");
	ZENITH_ASSERT_TRUE((u_int)ZM_MENU_SCREEN_SAVE < (u_int)ZM_MENU_SCREEN_COUNT,
		"SAVE is within the enumerated screen range");
}

ZENITH_TEST(ZM_MenuStack, MenuStack_RootItemElementNameIsTotalOverSixItems)
{
	// The ROOT now has SIX items. Every one must have a non-empty element name that
	// round-trips -- an entry whose name is "" is invisible and unfocusable. Reds if
	// ZM_MENU_ROOT_ITEM_COUNT was raised without adding the Save / Quit arms.
	ZENITH_ASSERT_EQ((u_int)ZM_MENU_ROOT_ITEM_COUNT, 6u,
		"the ROOT holds exactly six items after SC4 (Party/Bag/Dex/Save/Quit/Exit)");
	for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
	{
		const char* szName = ZM_UI_MenuStack::RootItemElementName(static_cast<ZM_MENU_ROOT_ITEM>(i));
		ZENITH_ASSERT_NOT_NULL(szName, "root item %u has a name", i);
		if (szName == nullptr) { continue; }
		ZENITH_ASSERT_TRUE(szName[0] != '\0',
			"root item %u has a non-empty element name (an empty name is unfocusable)", i);
		ZENITH_ASSERT_EQ(ZM_UI_MenuStack::RootItemIndexFromElementName(szName), (int)i,
			"root item %u round-trips through its element name", i);
	}
}

ZENITH_TEST(ZM_MenuStack, MenuStack_RootEnumOrderMatchesTheAuthoredVisualOrder)
{
	// The load-bearing invariant. StepGateRootFocusWalk (ZM_AutoTests_UI.cpp) presses
	// DOWN whenever the focused ordinal is BELOW the target ordinal, so a visually
	// LOWER entry MUST carry a HIGHER enum ordinal, or the walk oscillates to its
	// deadline. This pins that the enum ordinals are strictly increasing top-to-bottom
	// AND that the authored Y layout is strictly increasing in that same order.
	ZENITH_ASSERT_TRUE((u_int)ZM_MENU_ROOT_PARTY < (u_int)ZM_MENU_ROOT_BAG
		&& (u_int)ZM_MENU_ROOT_BAG < (u_int)ZM_MENU_ROOT_DEX
		&& (u_int)ZM_MENU_ROOT_DEX < (u_int)ZM_MENU_ROOT_SAVE
		&& (u_int)ZM_MENU_ROOT_SAVE < (u_int)ZM_MENU_ROOT_QUIT
		&& (u_int)ZM_MENU_ROOT_QUIT < (u_int)ZM_MENU_ROOT_EXIT,
		"the ROOT enum ordinals must be strictly increasing top-to-bottom "
		"(Party<Bag<Dex<Save<Quit<Exit) -- appending Save/Quit AFTER Exit while placing "
		"them visually above it is exactly the divergence that oscillates the focus walk");
	ZENITH_ASSERT_EQ((u_int)ZM_MENU_ROOT_EXIT, 5u,
		"Exit is the last of the six ROOT items after SC4 moved it 3 -> 5");

	// The OTHER half of the invariant -- that a visually LOWER ROOT entry really does carry
	// a HIGHER enum ordinal -- is NOT re-checkable in this pure boot unit. Proving it would
	// mean reading the real axEntries authored in Zenithmon.cpp's ZM_ConfigureMenuRoot,
	// which this unit (no scene, no ECS, no graphics) has no access to. A hand-copied local
	// table of authored Y values would only prove ITSELF internally consistent: swapping the
	// authored Y of ZM_MENU_ROOT_SAVE and ZM_MENU_ROOT_QUIT in ZM_ConfigureMenuRoot -- so
	// Quit renders ABOVE Save while the enum ordinals here stay put -- leaves such a table
	// untouched and GREEN, yet oscillates the runtime focus walk. That local-table check was
	// therefore a tautology and has been removed.
	//
	// The enum->authored-Y binding is instead enforced at RUNTIME by the windowed
	// focus-walk gates. StepGateRootFocusWalk (ZM_S6UIGate_Test, ZM_AutoTests_UI.cpp)
	// presses UP when the focused ENUM ordinal is above the target's and DOWN when it is
	// below, and ZM_SaveMenuFlow_Test walks DOWN onto the Save entry the same way -- so an
	// authored visual order that disagrees with the enum order makes the walk step the wrong
	// way, oscillate, and hit its deadline. The windowed gate thus REDS on exactly the
	// Save/Quit Y swap this pure unit cannot see; the strictly-increasing enum assertion
	// above is the pure half of the same invariant.
}
