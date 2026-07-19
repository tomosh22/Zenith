#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"   // ZM_SCENE_KIND (by value in the pure gating statics)

class Zenith_DataStream;
class Zenith_UIComponent;

// ============================================================================
// Zenithmon S6 item 2 SC1 -- ZM_UI_MenuStack: the overworld pause-menu machine.
//
// A new ECS component (order 112) living on the persistent ZM_MenuRoot entity
// (authored in FrontEnd, DontDestroyOnLoad, with its own Zenith_UIComponent),
// exactly mirroring ZM_BattleTransitionRoot + ZM_BattleTransition. It opens a
// focus-navigable ROOT pause menu over the overworld when the player presses the
// menu key (M / Tab), freezes the player, drives traversal via the ENGINE focus-
// navigation API (arrow keys, automatic once an element is focused), dispatches
// confirm BY THE FOCUSED ELEMENT'S NAME (never a `this` userdata -- a component
// pointer dangles when the ECS pool relocates), and pops on cancel/Escape,
// unfreezing the player + clearing focus when the stack empties.
//
// SC1 ships the ROOT screen (Party / Bag / Dex / Exit entries) plus the screen-
// stack machinery; SC2+ fill in the real Party/Bag/Dex/Dialogue/Shop screens
// (the extra ZM_MENU_SCREEN_* enumerators here are forward placeholders). The
// pure decision surface (the screen stack, the gating predicate, the focused-
// name -> action resolver, the scene-kind test) is static / value-typed and
// unit-tested with NO scene / graphics.
// ============================================================================

// The menu screen ids pushed onto the stack. ROOT is the pause root; the rest
// are forward placeholders wired to real presenters in later SCs (pushing one in
// SC1 simply hides the root panel until the player pops back). Save-stable:
// append before ZM_MENU_SCREEN_COUNT, never reorder.
enum ZM_MENU_SCREEN : u_int
{
	ZM_MENU_SCREEN_NONE = 0u,   // "empty stack" sentinel (never stored)
	ZM_MENU_SCREEN_ROOT,        // the pause root (Party / Bag / Dex / Exit)
	ZM_MENU_SCREEN_PARTY,       // placeholder (SC4)
	ZM_MENU_SCREEN_BAG,         // placeholder (SC6)
	ZM_MENU_SCREEN_DEX,         // placeholder (SC5)

	ZM_MENU_SCREEN_COUNT
};

// The ROOT screen's selectable entries, top to bottom (== the focus order).
enum ZM_MENU_ROOT_ITEM : u_int
{
	ZM_MENU_ROOT_PARTY = 0u,
	ZM_MENU_ROOT_BAG   = 1u,
	ZM_MENU_ROOT_DEX   = 2u,
	ZM_MENU_ROOT_EXIT  = 3u,

	ZM_MENU_ROOT_ITEM_COUNT = 4u
};

// The action a confirmed ROOT entry resolves to (dispatch is BY FOCUSED-ELEMENT
// NAME, mapped through ResolveRootAction below).
enum ZM_MENU_ACTION : u_int
{
	ZM_MENU_ACTION_NONE = 0u,   // unknown / no focused item -> ignore
	ZM_MENU_ACTION_OPEN_PARTY,
	ZM_MENU_ACTION_OPEN_BAG,
	ZM_MENU_ACTION_OPEN_DEX,
	ZM_MENU_ACTION_CLOSE,       // Exit -> pop the whole menu
};

// ----------------------------------------------------------------------------
// ZM_MenuScreenStack -- a pure fixed-capacity LIFO of screen ids. NO ECS / no
// graphics; owned BY VALUE by the component and unit-tested directly (the
// "pure model before its presenter" doctrine). Trivially movable (POD), so the
// owning component's ECS pool move-construct is preserved.
// ----------------------------------------------------------------------------
class ZM_MenuScreenStack
{
public:
	static constexpr u_int uMAX_DEPTH = 8u;

	void  Clear() { m_uDepth = 0u; }
	// Push a screen; rejects ZM_MENU_SCREEN_NONE and a full stack. Returns success.
	bool  Push(ZM_MENU_SCREEN eScreen);
	// Pop the top screen; false when already empty.
	bool  Pop();
	u_int GetDepth() const { return m_uDepth; }
	bool  IsEmpty() const { return m_uDepth == 0u; }
	// The current top, or ZM_MENU_SCREEN_NONE when empty.
	ZM_MENU_SCREEN Top() const;

private:
	ZM_MENU_SCREEN m_aeScreens[uMAX_DEPTH] = {};
	u_int          m_uDepth = 0u;
};

// ----------------------------------------------------------------------------
// ZM_UI_MenuStack -- the ECS component (order 112). Persistent-singleton, exactly
// like ZM_BattleTransition (claim-or-destroy in OnStart, DontDestroyOnLoad last).
// ----------------------------------------------------------------------------
class ZM_UI_MenuStack
{
public:
	static constexpr u_int uSERIALIZATION_VERSION = 1u;

	// Authored element names -- the SINGLE SOURCE of the ZM_ConfigureMenuRoot
	// contract in Zenithmon.cpp (create/place these) and this component's runtime
	// re-resolution (never cache pointers across frames -- the pool relocates).
	static constexpr const char* szROOT_PANEL_NAME = "Menu_RootPanel";
	static constexpr const char* szROOT_PARTY_NAME = "Menu_RootParty";
	static constexpr const char* szROOT_BAG_NAME   = "Menu_RootBag";
	static constexpr const char* szROOT_DEX_NAME   = "Menu_RootDex";
	static constexpr const char* szROOT_EXIT_NAME  = "Menu_RootExit";

	// Sort band: BELOW the fade overlays (WarpFade 10000 / BattleFade 10001) so a
	// warp / battle fade always covers the menu (watch-out 3 / ZM-D-097 band).
	static constexpr int iMENU_PANEL_SORT_ORDER  = 9000;
	static constexpr int iMENU_BUTTON_SORT_ORDER = 9001;

	ZM_UI_MenuStack() = delete;
	explicit ZM_UI_MenuStack(Zenith_Entity& xParentEntity);

	// Move-CONSTRUCTIBLE only (the ECS pool move-constructs on Grow / swap-and-pop /
	// DontDestroyOnLoad). All members are trivially movable, so noexcept-default is
	// well-formed (mirrors ZM_BattleTransition / ZM_GameStateManager). Copy deleted.
	ZM_UI_MenuStack(const ZM_UI_MenuStack&) = delete;
	ZM_UI_MenuStack& operator=(const ZM_UI_MenuStack&) = delete;
	ZM_UI_MenuStack(ZM_UI_MenuStack&&) noexcept = default;
	ZM_UI_MenuStack& operator=(ZM_UI_MenuStack&&) noexcept = default;

	void OnStart();
	void OnUpdate(float fDeltaSeconds);
	void OnDestroy();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

	// ---- Read accessors for the windowed test ----
	bool           IsOpen()       const { return !m_xStack.IsEmpty(); }
	ZM_MENU_SCREEN GetTopScreen() const { return m_xStack.Top(); }
	u_int          GetDepth()     const { return m_xStack.GetDepth(); }
	// The focused ROOT-item index mirror (0..ZM_MENU_ROOT_ITEM_COUNT-1); refreshed
	// from the canvas focus each frame the ROOT is shown. -1 when not on ROOT.
	int            GetCursor()    const { return m_iCursor; }

	// ---- Persistent-singleton observation (mirrors ZM_BattleTransition) ----
	static bool TryGetUniqueSingletonEntityID(Zenith_EntityID& xEntityIDOut);
	// Force-close the menu on the live singleton (unfreeze + clear focus). Skip-safe
	// no-op when no singleton exists. Called from windowed-test teardown so a batched
	// test cannot inherit an open menu from a previous (possibly failed) test.
	static void ResetRuntimeStateForTests();

	// ---- PURE decision surface (no scene / graphics -- unit-tested verbatim) ----

	// The overworld pause-open predicate: open iff the menu key was pressed, the
	// menu is not already open, the active scene is an overworld, and neither a warp
	// nor a battle transition owns the screen.
	static bool ShouldOpenMenu(bool bMenuKeyPressed, bool bAlreadyOpen,
		bool bOverworld, bool bWarpInProgress, bool bBattleTransitionActive);
	// A scene kind the pause menu may open over: everything except the title screen
	// (FRONTEND) and the additive battle scene (BATTLE).
	static bool IsOverworldSceneKind(ZM_SCENE_KIND eKind);
	// Map a confirmed ROOT element name to its action (nullptr / unknown -> NONE).
	static ZM_MENU_ACTION ResolveRootAction(const char* szFocusedElementName);
	// The authored element name for a ROOT item (the ZM_ConfigureMenuRoot contract).
	static const char* RootItemElementName(ZM_MENU_ROOT_ITEM eItem);
	// The ROOT item index for an element name, or -1 when it is not a ROOT entry.
	static int RootItemIndexFromElementName(const char* szElementName);
	// The screen an OPEN_* action pushes; ZM_MENU_SCREEN_NONE for CLOSE / NONE.
	static ZM_MENU_SCREEN RootActionToScreen(ZM_MENU_ACTION eAction);

private:
	// Session lifecycle helpers.
	void OpenRootMenu();
	void CloseMenu();
	void HandleConfirm();
	void HandleCancel();
	// Re-resolve the authored elements by name each frame (never cache) and show /
	// hide + focus them for the current top screen. Also mirrors m_iCursor.
	void PresentTopScreen();
	void FreezePlayer();
	void UnfreezePlayer();

	// Best-effort resolvers (the persistent UI component / the focused element name).
	Zenith_UIComponent* ResolveUI() const;
	// True iff the active scene is an overworld the menu may open over.
	static bool IsActiveSceneOverworld();

	static Zenith_EntityID s_xSingletonEntityID;

	// Stored BY VALUE (never a reference): a reference member would dangle on the
	// temporary ctor handle and break the pool's move-construct. Zenith_Entity has no
	// mutable state -- it re-resolves its own slot every call.
	Zenith_Entity      m_xParentEntity;
	ZM_MenuScreenStack m_xStack;                                  // empty == closed
	int                m_iCursor = -1;                            // focused ROOT-item mirror
	Zenith_EntityID    m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;
};
