#pragma once

#include "Core/Zenith_Result.h"                         // Zenith_Status (the last-save latch)
#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"        // ZM_SCENE_KIND (by value in the pure gating statics)
#include "Zenithmon/Source/UI/ZM_UI_Bag.h"             // owned BY VALUE (the SC6 bag screen)
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"     // owned BY VALUE (the SC2 dialogue screen)
#include "Zenithmon/Source/UI/ZM_UI_Dex.h"             // owned BY VALUE (the SC5 dex screen)
#include "Zenithmon/Source/UI/ZM_UI_Party.h"           // owned BY VALUE (the SC4 party screen)
#include "Zenithmon/Source/UI/ZM_UI_SaveSlots.h"       // owned BY VALUE (the S7 SC4 save/load screen)
#include "Zenithmon/Source/UI/ZM_UI_Shop.h"            // owned BY VALUE (the SC7 shop screen)

class Zenith_DataStream;
class Zenith_UIComponent;

// ============================================================================
// Zenithmon S6 item 2 (SC1 + SC2) -- ZM_UI_MenuStack: the overworld pause-menu
// machine, and host of the dialogue box.
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
// SC1 shipped the ROOT screen (Party / Bag / Dex / Exit entries) plus the screen-
// stack machinery; SC2 adds the DIALOGUE screen -- a by-value ZM_UI_DialogueBox
// raised by PushDialogueLines / TryPushDialogue (the seam NPCs and prompts talk
// through), modal (Escape never dismisses it) and advanced only by confirm; SC4
// adds the PARTY screen -- a by-value ZM_UI_Party list + summary; SC5 adds the DEX
// screen -- a by-value ZM_UI_Dex paged grid; SC6 adds the BAG screen -- a by-value
// ZM_UI_Bag pocket-tabbed, paged item list; SC7 adds the SHOP screen -- a by-value
// ZM_UI_Shop buy/sell list raised by OpenShop / the static TryOpenShop (the seam the
// mart NPC talks through), and the first screen that WRITES the live game state; SC8
// adds the yes/no PROMPT -- the same DIALOGUE screen with a choice armed on the box,
// raised by OpenCareCenterPrompt / the static TryOpenCareCenterPrompt, whose answer
// runs the pending ZM_DIALOGUE_ACTION (the Care Center heal) against that state.
//
// Screen dispatch is GENERALIZED: OnUpdate routes input through ONE per-screen
// switch and PresentTopScreen shows/hides through ONE per-screen block, so adding
// a screen is a new arm in each -- never a reshape of either site. The pure
// decision surface (the screen stack, the gating predicate, the focused-name ->
// action resolver, the scene-kind test, the dialogue + party models) is static /
// value-typed and unit-tested with NO scene / graphics.
// ============================================================================

// The menu screen ids pushed onto the stack. ROOT is the pause root and DIALOGUE
// is the SC2 dialogue box. Save-stable: append before ZM_MENU_SCREEN_COUNT, never
// reorder.
enum ZM_MENU_SCREEN : u_int
{
	ZM_MENU_SCREEN_NONE = 0u,   // "empty stack" sentinel (never stored)
	ZM_MENU_SCREEN_ROOT,        // the pause root (Party / Bag / Dex / Exit)
	ZM_MENU_SCREEN_PARTY,       // SC4: the party list + per-member summary
	ZM_MENU_SCREEN_BAG,         // SC6: the pocket-tabbed, paged item list + money
	ZM_MENU_SCREEN_DEX,         // SC5: the paged species grid
	ZM_MENU_SCREEN_DIALOGUE,    // SC2: the NPC / prompt dialogue box (modal, not a ROOT entry)
	ZM_MENU_SCREEN_SHOP,        // SC7: the mart buy/sell screen (raised by TryOpenShop, not a ROOT entry)
	ZM_MENU_SCREEN_SAVE,        // S7 SC4: the save/load slot screen (serves BOTH modes)

	ZM_MENU_SCREEN_COUNT
};

// The ROOT screen's selectable entries, top to bottom (== the focus order). ENUM ORDER
// MUST EQUAL THE AUTHORED VISUAL ORDER (Zenithmon.cpp ZM_ConfigureMenuRoot axEntries):
// ZM_AutoTests_UI's focus walk presses DOWN when the focused ENUM ordinal is below the
// target's and UP otherwise, so an entry placed visually above EXIT but numbered after
// it would make the walk oscillate to its deadline. SAVE and QUIT are therefore INSERTED
// before EXIT (which moves 3 -> 5), never appended after it. All consumers are symbolic
// and COUNT-driven.
enum ZM_MENU_ROOT_ITEM : u_int
{
	ZM_MENU_ROOT_PARTY = 0u,
	ZM_MENU_ROOT_BAG   = 1u,
	ZM_MENU_ROOT_DEX   = 2u,
	ZM_MENU_ROOT_SAVE  = 3u,   // S7 SC4: opens the save-slot screen (SAVE mode)
	ZM_MENU_ROOT_QUIT  = 4u,   // S7 SC4: confirm, then quit to the title screen
	ZM_MENU_ROOT_EXIT  = 5u,

	ZM_MENU_ROOT_ITEM_COUNT = 6u
};

// What a resolved YES on the dialogue's yes/no prompt actually DOES (S6 item 2
// SC8). It lives here rather than on the box because a by-value screen cannot call
// back into its host, and a function pointer would be the wrong shape for a single
// consumer. Session state only -- never serialized, and cleared on every close.
enum ZM_DIALOGUE_ACTION : u_int
{
	ZM_DIALOGUE_ACTION_NONE = 0u,   // an ordinary conversation: YES/NO do nothing extra
	ZM_DIALOGUE_ACTION_HEAL_PARTY,  // the Care Center heal (ZM_ApplyCareCenterHeal)
	ZM_DIALOGUE_ACTION_WRITE_SAVE_SLOT,   // S7 SC4: YES writes m_ePendingSaveSlot (overwrite confirm)
	ZM_DIALOGUE_ACTION_QUIT_TO_TITLE,     // S7 SC4: YES quits to the title screen
};

// The action a confirmed ROOT entry resolves to (dispatch is BY FOCUSED-ELEMENT
// NAME, mapped through ResolveRootAction below).
enum ZM_MENU_ACTION : u_int
{
	ZM_MENU_ACTION_NONE = 0u,   // unknown / no focused item -> ignore
	ZM_MENU_ACTION_OPEN_PARTY,
	ZM_MENU_ACTION_OPEN_BAG,
	ZM_MENU_ACTION_OPEN_DEX,
	ZM_MENU_ACTION_CLOSE,           // Exit -> pop the whole menu
	ZM_MENU_ACTION_OPEN_SAVE,       // S7 SC4: Save -> push the save-slot screen (SAVE mode)
	ZM_MENU_ACTION_QUIT_TO_TITLE,   // S7 SC4: Quit -> confirm, then quit to the title screen
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
	static constexpr const char* szROOT_SAVE_NAME  = "Menu_RootSave";
	static constexpr const char* szROOT_QUIT_NAME  = "Menu_RootQuit";
	static constexpr const char* szROOT_EXIT_NAME  = "Menu_RootExit";

	// Sort band: BELOW the fade overlays (WarpFade 10000 / BattleFade 10001) so a
	// warp / battle fade always covers the menu (watch-out 3 / ZM-D-097 band).
	static constexpr int iMENU_PANEL_SORT_ORDER  = 9000;
	static constexpr int iMENU_BUTTON_SORT_ORDER = 9001;

	ZM_UI_MenuStack() = delete;
	explicit ZM_UI_MenuStack(Zenith_Entity& xParentEntity);

	// Move-CONSTRUCTIBLE only (the ECS pool move-constructs on Grow / swap-and-pop /
	// DontDestroyOnLoad). Every member is NOEXCEPT-movable -- the PODs trivially, and
	// m_xDialogue's std::string queue via std::string's noexcept move (the same reason
	// ZM_BattleDirector may hold ZM_UI_BattleHUD by value) -- so noexcept-default is
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
	// The focused-item mirror of the top FOCUS-NAVIGABLE screen, refreshed from the
	// canvas focus each frame it is shown: the ROOT entry index on ROOT, the party
	// slot on PARTY, the dex CELL on DEX (-1 there while a page button holds the
	// focus), the bag ROW on BAG (-1 while a nav button holds it), and the selected ROW
	// on the shop's current PAGE on SHOP -- a page-relative row, NOT a flat list index
	// (ZM_UI_Shop::GetSelectedEntryIndex resolves that), which deliberately SURVIVES the
	// focus moving onto a control so the walk to Confirm cannot forget what was picked.
	// ALWAYS -1 on DIALOGUE: it has no list to mirror, even while its SC8 yes/no prompt
	// owns the canvas focus (which of the two buttons is focused is read off the canvas).
	int            GetCursor()    const { return m_iCursor; }

	// ---- Dialogue (SC2) ----

	// Queue NPC / prompt lines and raise the DIALOGUE screen. All-or-nothing (the
	// queue is only mutated when every line is accepted). Freezes the player and
	// opens the menu when the stack was empty, so an NPC can talk without the pause
	// menu; when a menu screen is already open the dialogue stacks ON TOP of it and
	// popping returns to it. Returns false when the lines are rejected.
	bool PushDialogueLines(const char* const* paszLines, u_int uCount);
	// Singleton-resolving convenience (the seam S6 item 3 ZM_Interactable + the
	// windowed test use). False when no live ZM_MenuRoot singleton exists.
	static bool TryPushDialogue(const char* const* paszLines, u_int uCount);
	const ZM_UI_DialogueBox& GetDialogue() const { return m_xDialogue; }

	// ---- Care Center prompt (SC8) ----

	// Raise the Care Center's yes/no prompt: queue ZM_CareCenterPromptLine, arm the
	// choice with the Yes/No labels, set the pending action to HEAL_PARTY, and push /
	// raise the DIALOGUE screen exactly as PushDialogueLines does (freezing the player
	// when the stack was empty). Refused when the box is already busy -- a prompt owns
	// the box outright, it never interleaves itself into someone else's conversation.
	bool OpenCareCenterPrompt();
	// Singleton-resolving convenience -- the SAME seam shape as TryPushDialogue /
	// TryOpenShop (what S6 item 3's Care Center NPC and the windowed test call). False
	// when no live ZM_MenuRoot singleton exists, or when the prompt is refused.
	static bool TryOpenCareCenterPrompt();
	// What a YES on the prompt currently in the box would do (NONE when nothing is armed).
	ZM_DIALOGUE_ACTION GetPendingDialogueAction() const { return m_eDialogueAction; }
	// The LAST answer a prompt resolved to, latched on the HOST. Read this rather than
	// GetDialogue().GetChoice() after the fact: resolving a prompt that was raised over an
	// empty stack pops to empty, which CloseMenu()s, which Reset()s the box and clears its
	// stored answer -- all in the same OnUpdate, so the box-level answer is unobservable
	// from outside. Survives the close; cleared only on boot / deserialize.
	ZM_DIALOGUE_CHOICE GetLastDialogueAnswer() const { return m_eLastDialogueAnswer; }
	// The dialogue has read its lines and is waiting on the yes/no answer.
	bool IsDialogueAwaitingChoice() const { return m_xDialogue.IsAwaitingChoice(); }

	// ---- Party (SC4) ----
	const ZM_UI_Party& GetPartyScreen() const { return m_xParty; }

	// ---- Dex (SC5) ----
	const ZM_UI_Dex& GetDexScreen() const { return m_xDex; }

	// ---- Bag (SC6) ----
	const ZM_UI_Bag& GetBagScreen() const { return m_xBagScreen; }

	// ---- Shop (SC7) ----

	// Configure the mart's stock and raise the SHOP screen. All-or-nothing (the stock
	// is only taken when SetInventory accepts the whole list). Freezes the player and
	// opens the menu when the stack was empty, so a mart clerk can trade without the
	// pause menu; when a menu screen is already open the shop stacks ON TOP of it and
	// popping returns to it. The shop is NOT a ROOT entry -- a mart is entered by
	// talking to its clerk, never from the pause menu.
	bool OpenShop(const ZM_ITEM_ID* paeInventory, u_int uCount);
	// Singleton-resolving convenience -- the SAME seam shape as TryPushDialogue (what
	// S6 item 3's mart NPC and the windowed test call). False when no live ZM_MenuRoot
	// singleton exists, or when the inventory is rejected.
	static bool TryOpenShop(const ZM_ITEM_ID* paeInventory, u_int uCount);
	const ZM_UI_Shop& GetShopScreen() const { return m_xShop; }

	// ---- Save / load slot screen (S7 item 2 SC4) ----

	// Re-probe all four slots, set the mode and raise the SAVE screen. Opens the menu +
	// freezes the player when the stack was empty (the SC5 title Continue path), or stacks
	// on top of an already-open menu (the pause-menu Save entry). SAVE (including an invalid
	// mode, which folds to SAVE) is refused while the live save policy reports a blocker;
	// LOAD remains available on FrontEnd. SC5 owns LOAD's eventual Continue trigger.
	bool OpenSaveScreen(ZM_SAVE_SCREEN_MODE eMode);
	// Singleton-resolving convenience -- the SAME seam shape as TryOpenShop. False when no
	// live ZM_MenuRoot singleton exists, or when the screen refuses to raise.
	static bool TryOpenSaveScreen(ZM_SAVE_SCREEN_MODE eMode);
	// Arm the EXISTING yes/no dialogue over a READY/DAMAGED MANUAL slot (ZM-D-121: the choice
	// mode IS the yes/no primitive -- no new screen, no new ECS order). Auto, NONE and every
	// out-of-range slot are rejected before mutation. The box is SINGLE-TENANT
	// (Shortfalls.md:72), so this returns FALSE while it is busy and the caller MUST handle
	// the refusal rather than half-arm.
	bool OpenSaveConfirmPrompt(ZM_SAVE_SLOT eSlot);
	// The quit-to-title yes/no confirm, armed the same way; also FALSE when the box is busy.
	bool OpenQuitConfirmPrompt();
	const ZM_UI_SaveSlots& GetSaveScreen() const { return m_xSaveScreen; }
	// The slot an armed WRITE confirm targets (ZM_SAVE_SLOT_NONE when none is armed).
	ZM_SAVE_SLOT  GetPendingSaveSlot() const { return m_ePendingSaveSlot; }
	// The status of the LAST manual save (non-Ok until one runs). Survives a menu close,
	// like m_eLastDialogueAnswer -- it is a latch, not live session state.
	Zenith_Status GetLastSaveStatus()  const { return m_xLastSaveStatus; }
	// Manual writes performed this session (test / observation latch; survives close).
	u_int         GetSaveWriteCount()  const { return m_uSaveWriteCount; }

	// ---- Persistent-singleton observation (mirrors ZM_BattleTransition) ----
	static bool TryGetUniqueSingletonEntityID(Zenith_EntityID& xEntityIDOut);
	// Force-close the menu on the live singleton (unfreeze + clear focus). Skip-safe
	// no-op when no singleton exists. Called from windowed-test teardown so a batched
	// test cannot inherit an open menu from a previous (possibly failed) test.
	static void ResetRuntimeStateForTests();
	// Is the menu (any screen, including DIALOGUE) up on the live singleton? FALSE
	// when no singleton resolves -- headless units and FrontEnd-less contexts must
	// get a plain "no", never an assert. The static counterpart of the instance
	// IsOpen() above, added by S6 item 3 SC1 so the interaction gate can ask the
	// question from a free context without hand-rolling the resolve.
	static bool IsMenuOpen();

	// ---- PURE decision surface (no scene / graphics -- unit-tested verbatim) ----

	// The overworld pause-open predicate: open iff the menu key was pressed, the
	// menu is not already open, the active scene is an overworld, and neither a warp
	// nor a battle transition owns the screen.
	static bool ShouldOpenMenu(bool bMenuKeyPressed, bool bAlreadyOpen,
		bool bOverworld, bool bWarpInProgress, bool bBattleTransitionActive);
	// A scene kind the pause menu may open over: everything except the title screen
	// (FRONTEND) and the additive battle scene (BATTLE).
	static bool IsOverworldSceneKind(ZM_SCENE_KIND eKind);
	// True iff the active scene is an overworld the menu may open over. NOT pure --
	// it reads the live scene table -- but it touches no instance state, so it is a
	// plain static. PUBLIC since S6 item 3 SC1: the interaction gate asks the SAME
	// question, and duplicating the build-index -> scene -> kind resolve in
	// interaction code is exactly how the two would drift apart.
	static bool IsActiveSceneOverworld();
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
	// Leave the top screen: pop it, and close the whole menu once the stack empties.
	// The ONE close path -- cancel, a dialogue read to the end, and an answered prompt
	// all leave through it.
	void PopTopScreen();
	// Run the pending action for an answered prompt (YES + HEAL_PARTY heals the LIVE
	// game state), clear it, latch the answer, and leave the screen through PopTopScreen.
	// ONE exception (SC9): a YES whose heal actually CHANGED something queues
	// ZM_CareCenterHealedLine() onto the (already reset, now unarmed) box and does NOT pop
	// -- the ordinary read-to-the-end CLOSED path pops it on the next confirm, so the heal
	// is never a silent button. Every other path pops immediately.
	void ApplyDialogueChoice(ZM_DIALOGUE_CHOICE eAnswer);
	// Re-resolve the authored elements by name each frame (never cache) and show /
	// hide + focus them for the current top screen. Also mirrors m_iCursor.
	void PresentTopScreen();
	// Per-screen show/hide steps, called by PresentTopScreen (and by CloseMenu for the
	// hide side). Each owns exactly ONE screen's authored elements.
	static void SetRootElementsShown(Zenith_UIComponent& xUI, bool bShown);
	// True only when the party screen was actually PRESENTED (it is the top screen AND
	// a live game state resolved); false means it was hidden, so the caller must fall
	// back to the non-navigable focus policy.
	bool PresentPartyScreen(bool bShown);
	// The same contract for the SC5 dex screen (top screen AND a live game state).
	bool PresentDexScreen(bool bShown);
	// ...and for the SC6 bag screen (top screen AND a live game state).
	bool PresentBagScreen(bool bShown);
	// ...and for the SC7 shop screen (top screen AND a live game state).
	bool PresentShopScreen(bool bShown);
	// ...and for the S7 SC4 save screen. Unlike the four above it does NOT gate on a live
	// game state -- the LOAD half runs on the title screen where none exists yet -- so it is
	// always presented when it is the top screen. Returns bShown.
	bool PresentSaveScreen(bool bShown);
	// Perform a manual save into eSlot: recheck the canonical live blocker, then capture the
	// live world position into the game state, WriteState through the SC2 slot layer, latch
	// the status + bump the write count, RE-PROBE the screen so the row reflects the new
	// status, and queue a result line onto the (empty) box. The single write path shared by
	// the immediate-WRITE arm and the WRITE_SAVE_SLOT confirm.
	void PerformSaveToSlot(ZM_SAVE_SLOT eSlot);
	void FreezePlayer();
	void UnfreezePlayer();

	// Best-effort resolvers (the persistent UI component / the focused element name).
	Zenith_UIComponent* ResolveUI() const;
	// The canvas's currently-focused element NAME, or null. Points into that element's
	// own string, so it is only valid for the duration of the dispatching call -- which
	// is exactly how every by-name dispatch site uses it.
	const char* ResolveFocusedElementName() const;

	static Zenith_EntityID s_xSingletonEntityID;

	// Stored BY VALUE (never a reference): a reference member would dangle on the
	// temporary ctor handle and break the pool's move-construct. Zenith_Entity has no
	// mutable state -- it re-resolves its own slot every call.
	Zenith_Entity      m_xParentEntity;
	ZM_MenuScreenStack m_xStack;                                  // empty == closed
	ZM_UI_DialogueBox  m_xDialogue;                               // the DIALOGUE screen's model (SC2)
	ZM_UI_Party        m_xParty;                                  // the PARTY screen's model (SC4; PODs only)
	ZM_UI_Dex          m_xDex;                                    // the DEX screen's model (SC5; PODs only)
	// NOT m_xBag -- that name belongs to ZM_GameState's MODEL, which this presenter
	// renders; keeping them distinct keeps the two straight at every call site.
	ZM_UI_Bag          m_xBagScreen;                              // the BAG screen's model (SC6; PODs only)
	ZM_UI_Shop         m_xShop;                                   // the SHOP screen's model (SC7; PODs only)
	ZM_UI_SaveSlots    m_xSaveScreen;                            // the SAVE/LOAD screen's model (S7 SC4; PODs only)
	// The slot an armed WRITE confirm targets. Session state; consumed with m_eDialogueAction.
	ZM_SAVE_SLOT       m_ePendingSaveSlot = ZM_SAVE_SLOT_NONE;
	// The LAST manual save's status. A deliberately non-Ok default (no save has run yet);
	// a latch that survives a menu close, like m_eLastDialogueAnswer.
	Zenith_Status      m_xLastSaveStatus = Zenith_ErrorCode::INVALID_ARGUMENT;
	u_int              m_uSaveWriteCount = 0u;                   // manual writes performed (test / observation latch)
	ZM_DIALOGUE_ACTION m_eDialogueAction = ZM_DIALOGUE_ACTION_NONE;   // what a YES does (SC8)
	ZM_DIALOGUE_CHOICE m_eLastDialogueAnswer = ZM_DIALOGUE_CHOICE_NONE;   // the latched answer (SC8)
	int                m_iCursor = -1;                            // focused-item mirror (see GetCursor)
	Zenith_EntityID    m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;
};
