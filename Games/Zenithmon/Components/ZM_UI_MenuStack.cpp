#include "Zenith.h"

#include "Zenithmon/Components/ZM_UI_MenuStack.h"

#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"   // ZM_BattleTransition::IsTransitionActive (gating)
#include "Zenithmon/Components/ZM_GameStateManager.h"   // IsWarpInProgress + TryGetUniqueActiveScenePlayerEntityID + TryGetGameState (+ ZM_GameState)
#include "Zenithmon/Components/ZM_PlayerController.h"    // SetMovementEnabled (freeze seam)
#include "Zenithmon/Source/CareCenter/ZM_CareCenter.h"  // the SC8 prompt lines + the heal
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"       // ZM_SPECIES_COUNT (the dex size the DEX screen pages over)
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"         // ZM_FindSceneByBuildIndex / ZM_GetWorldSpec / ZM_SCENE_KIND
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"         // WriteState / ProbeSlot / ResolveLiveSaveBlocker (S7 SC4)
#include "Zenithmon/Source/ZM_InputActions.h"           // ReadMenuPressed / ReadConfirmPressed / ReadCancelPressed

#include <cstdio>
#include <cstring>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// ZM_UI_MenuStack (S6 item 2 SC1 + SC2 + SC4 + SC5 + SC6 + SC7 + SC8). The overworld pause-menu machine on
// the persistent ZM_MenuRoot entity. Opens a focus-navigable ROOT menu, freezes the
// player, drives traversal via the engine focus-nav API, dispatches confirm by
// the focused element's NAME, pops on cancel/Escape. Mirrors ZM_BattleTransition
// (persistent singleton) + ZM_UI_BattleHUD (re-resolve elements by name, never
// cache; dispatch by name, never SetOnClick(this)).
//
// Screen dispatch lives in exactly TWO per-screen switches -- the input routing in
// OnUpdate and the show/hide + focus policy in PresentTopScreen. A new screen adds
// an arm to each and a by-value presenter member; nothing else moves. SC7 (Shop) is
// the fourth screen added that way, and it reshaped neither site. SC8 adds NO screen
// at all -- the Care Center prompt is the DIALOGUE screen with a yes/no choice armed
// on the box, so it only widened the two DIALOGUE arms.
// ============================================================================

Zenith_EntityID ZM_UI_MenuStack::s_xSingletonEntityID = INVALID_ENTITY_ID;

// The scratch buffer for a save prompt / result line. Every string built into it is a
// short literal plus a slot display name ("Slot 1".."Slot 3" / "Auto"), so this is
// comfortably larger than any line and snprintf truncates safely regardless.
static constexpr u_int uSAVE_LINE_CAPACITY = 96u;

// ---- ZM_MenuScreenStack (pure) ---------------------------------------------

bool ZM_MenuScreenStack::Push(ZM_MENU_SCREEN eScreen)
{
	if (eScreen == ZM_MENU_SCREEN_NONE || m_uDepth >= uMAX_DEPTH)
	{
		return false;
	}
	m_aeScreens[m_uDepth] = eScreen;
	++m_uDepth;
	return true;
}

bool ZM_MenuScreenStack::Pop()
{
	if (m_uDepth == 0u)
	{
		return false;
	}
	--m_uDepth;
	return true;
}

bool ZM_MenuScreenStack::Contains(ZM_MENU_SCREEN eScreen) const
{
	if (eScreen == ZM_MENU_SCREEN_NONE)
	{
		return false;
	}
	for (u_int u = 0u; u < m_uDepth; ++u)
	{
		if (m_aeScreens[u] == eScreen)
		{
			return true;
		}
	}
	return false;
}

ZM_MENU_SCREEN ZM_MenuScreenStack::Top() const
{
	return (m_uDepth == 0u) ? ZM_MENU_SCREEN_NONE : m_aeScreens[m_uDepth - 1u];
}

// ---- ZM_LoadConfirmState (pure) --------------------------------------------

bool ZM_LoadConfirmState::Arm(ZM_SAVE_SLOT eSlot)
{
	if (IsArmed()
		|| static_cast<u_int>(eSlot) >= static_cast<u_int>(ZM_SAVE_SLOT_COUNT))
	{
		return false;
	}
	m_ePendingSlot = eSlot;
	return true;
}

ZM_SAVE_SLOT ZM_LoadConfirmState::Resolve(ZM_DIALOGUE_CHOICE eAnswer)
{
	if (!IsArmed()
		|| (eAnswer != ZM_DIALOGUE_CHOICE_YES && eAnswer != ZM_DIALOGUE_CHOICE_NO))
	{
		return ZM_SAVE_SLOT_NONE;   // NONE/invalid is not an answer and preserves the arm
	}
	const ZM_SAVE_SLOT eSlot = m_ePendingSlot;
	Reset();
	return (eAnswer == ZM_DIALOGUE_CHOICE_YES) ? eSlot : ZM_SAVE_SLOT_NONE;
}

// ---- Construction + lifecycle ----------------------------------------------

ZM_UI_MenuStack::ZM_UI_MenuStack(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_UI_MenuStack::OnStart()
{
	// Persistent-singleton claim-or-destroy (mirrors ZM_BattleTransition::OnStart):
	// FrontEnd re-authors a ZM_MenuRoot on every scene-0 (re)load, but only the FIRST
	// survives -- a duplicate detects the live singleton and destroys itself.
	const Zenith_EntityID xOwnEntityID = m_xParentEntity.GetEntityID();
	if (s_xSingletonEntityID == xOwnEntityID)
	{
		return;
	}

	Zenith_Entity xExisting = g_xEngine.Scenes().ResolveEntity(s_xSingletonEntityID);
	if (xExisting.IsValid()
		&& xExisting.TryGetComponent<ZM_UI_MenuStack>() != nullptr)
	{
		m_xParentEntity.Destroy();
		return;
	}

	s_xSingletonEntityID = xOwnEntityID;

	// Start closed: menu is a session-only overlay, nothing persists across saves.
	m_xStack.Clear();
	m_xDialogue.Reset();
	m_xParty.Reset();
	m_xDex.Reset();
	m_xBagScreen.Reset();
	m_xShop.Reset();
	m_xSaveScreen.Reset();
	m_xTitle.Open(nullptr, 0u);
	m_ePendingSaveSlot = ZM_SAVE_SLOT_NONE;
	m_xLoadConfirm.Reset();
	m_eDialogueAction = ZM_DIALOGUE_ACTION_NONE;
	// The latched answer is deliberately NOT cleared by CloseMenu (it must survive the
	// close that resolving a prompt triggers), so boot and deserialize are the only
	// places a stale one can be dropped. The save latch (status + write count) shares
	// that lifetime for the same reason.
	m_eLastDialogueAnswer = ZM_DIALOGUE_CHOICE_NONE;
	m_xLastSaveStatus = Zenith_ErrorCode::INVALID_ARGUMENT;
	m_uSaveWriteCount = 0u;
	m_eLastLoadSlot = ZM_SAVE_SLOT_NONE;
	m_xLastLoadStatus = Zenith_ErrorCode::INVALID_ARGUMENT;
	m_uLoadReadCount = 0u;
	m_iCursor = -1;
	m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;

	// Moving to the persistent scene relocates this component's pool entry. Nothing
	// may access `this` after the call -- keep it LAST.
	m_xParentEntity.DontDestroyOnLoad();
}

void ZM_UI_MenuStack::OnDestroy()
{
	if (s_xSingletonEntityID == m_xParentEntity.GetEntityID())
	{
		s_xSingletonEntityID = INVALID_ENTITY_ID;
	}
}

void ZM_UI_MenuStack::OnUpdate(float fDeltaSeconds)
{
	const bool bWarpInProgress = ZM_GameStateManager::IsWarpInProgress();
	const bool bBattleTransitionActive = ZM_BattleTransition::IsTransitionActive();
	const bool bFrontEndActive = IsActiveSceneFrontEnd();

	// TITLE is a persistent-root session, not scene state. OnStart does not rerun when the
	// surviving MenuRoot returns to FrontEnd, so raise it from settled active-scene state;
	// conversely, tear the whole TITLE -> LOAD -> DIALOGUE stack down the instant another
	// transition or scene owns the screen so no title control can leak into Dawnmere.
	if (m_xStack.Contains(ZM_MENU_SCREEN_TITLE)
		&& (!bFrontEndActive || bWarpInProgress || bBattleTransitionActive))
	{
		CloseMenu();
		return;
	}

	if (!IsOpen())
	{
		if (bFrontEndActive && !bWarpInProgress && !bBattleTransitionActive)
		{
			OpenTitleMenu();
			return;
		}

		// Overworld pause-open gate: menu key, in an overworld scene, no warp / battle.
		if (ShouldOpenMenu(
			ZM_InputActions::ReadMenuPressed(),
			/* bAlreadyOpen */ false,
			IsActiveSceneOverworld(),
			bWarpInProgress,
			bBattleTransitionActive))
		{
			OpenRootMenu();
		}
		return;
	}

	// -- Menu open. Arrow-key traversal is driven automatically by the engine's
	//    per-canvas UpdateFocusNavigation (keyboard confirm / cancel are NOT wired
	//    engine-side, so they are game-supplied here). Both readers are pure edge
	//    queries with no consume side effect, so reading them up front is free. --
	const bool bConfirm = ZM_InputActions::ReadConfirmPressed();
	const bool bCancel  = ZM_InputActions::ReadCancelPressed();

	// ONE per-screen input-routing switch: every screen owns its own confirm / cancel
	// semantics here, so adding SC7 (Shop) is a new arm rather than a reshape of the
	// routing.
	switch (m_xStack.Top())
	{
	case ZM_MENU_SCREEN_TITLE:
		// FrontEnd dispatch is by the focused element NAME, exactly like ROOT. Cancel is
		// deliberately ignored: the title is the base screen and cannot pop to nothing.
		if (bConfirm)
		{
			// Re-probe at the input boundary so a slot removed while the title was open cannot
			// leave a stale Continue name live. ResolveConfirm applies the refreshed visibility.
			RefreshTitleMenu();
			const ZM_TITLE_ACTION eAction = m_xTitle.ResolveConfirm(ResolveFocusedElementName());
			if (TitleActionToScreen(eAction) == ZM_MENU_SCREEN_SAVE
				&& TitleActionToSaveMode(eAction) == ZM_SAVE_SCREEN_MODE_LOAD)
			{
				OpenSaveScreen(ZM_SAVE_SCREEN_MODE_LOAD);   // pushes above TITLE; Back reveals it
			}
			else if (eAction == ZM_TITLE_ACTION_NEW_GAME
				&& ZM_GameStateManager::RequestNewGame())
			{
				// Queue first, close second: a refused transition leaves the title fully live.
				CloseMenu();
			}
		}
		break;

	case ZM_MENU_SCREEN_DIALOGUE:
		// The dialogue owns input while it is the top screen: confirm advances the
		// typewriter / the line, and CANCEL is deliberately IGNORED -- a dialogue is
		// modal and closes only by being read to the end, so its LINES can never be
		// escaped past.
		m_xDialogue.Tick(fDeltaSeconds);
		if (m_xDialogue.IsAwaitingChoice())
		{
			// SC8: the lines are read and a yes/no prompt is up. Confirm answers BY THE
			// FOCUSED ELEMENT'S NAME (the box owns the two buttons), and cancel resolves NO --
			// the lines stay modal, but a question the player could neither answer nor escape
			// would be a dead end.
			ZM_DIALOGUE_CHOICE eAnswer = ZM_DIALOGUE_CHOICE_NONE;
			if (bConfirm)
			{
				eAnswer = m_xDialogue.ResolveChoice(ResolveFocusedElementName());
			}
			else if (bCancel)
			{
				eAnswer = m_xDialogue.CancelChoice();
			}
			if (eAnswer != ZM_DIALOGUE_CHOICE_NONE)
			{
				ApplyDialogueChoice(eAnswer);
			}
		}
		else if (bConfirm)
		{
			if (m_xDialogue.Confirm() == ZM_DIALOGUE_ADVANCE_CLOSED)
			{
				PopTopScreen();
			}
		}
		break;

	case ZM_MENU_SCREEN_PARTY:
		// Confirm toggles the focused member's summary; cancel is offered to the screen
		// FIRST, so an open summary swallows the Escape and only the next one pops back
		// to ROOT. Traversal stays the engine focus-nav (no hand-rolled cursor).
		if (bConfirm)
		{
			m_xParty.Confirm();
		}
		else if (bCancel && !m_xParty.Cancel())
		{
			PopTopScreen();
		}
		break;

	case ZM_MENU_SCREEN_DEX:
		// The dex dispatches BY THE FOCUSED ELEMENT'S NAME too (never SetOnClick(this)):
		// the two page buttons page, and a cell is inert until a per-species detail panel
		// exists. Cancel pops straight back to ROOT -- the screen has no sub-state to
		// swallow it with. Grid traversal is the engine SPATIAL focus-nav.
		if (bConfirm)
		{
			m_xDex.Confirm(ResolveFocusedElementName(), static_cast<u_int>(ZM_SPECIES_COUNT));
		}
		else if (bCancel)
		{
			PopTopScreen();
		}
		break;

	case ZM_MENU_SCREEN_BAG:
		// The bag dispatches BY THE FOCUSED ELEMENT'S NAME too (never SetOnClick(this)):
		// the four nav buttons page / change pocket, and a row is inert until an item
		// action menu exists (SC7+). Cancel pops straight back to ROOT -- the screen has
		// no sub-state to swallow it with. The live bag is resolved per press rather than
		// cached: the same TryGetGameState the presenter uses, and a missing state simply
		// eats the confirm instead of crashing.
		if (bConfirm)
		{
			ZM_GameState* pxState = nullptr;
			if (ZM_GameStateManager::TryGetGameState(pxState) && pxState != nullptr)
			{
				m_xBagScreen.Confirm(ResolveFocusedElementName(), pxState->m_xBag);
			}
		}
		else if (bCancel)
		{
			PopTopScreen();
		}
		break;

	case ZM_MENU_SCREEN_SHOP:
		// The shop dispatches BY THE FOCUSED ELEMENT'S NAME too. Its EXIT control is
		// resolved HERE rather than inside the presenter, because leaving is the STACK's
		// business -- a by-value screen cannot pop itself. Everything else (the mode tabs,
		// the paging, the quantity stepper, and the Confirm that actually moves money) goes
		// to the screen, with the LIVE game state's bag and money passed BY REFERENCE so a
		// purchase persists. A missing state simply eats the confirm instead of crashing.
		// Cancel pops straight back to whatever raised the shop.
		if (bConfirm)
		{
			const char* szFocusedName = ResolveFocusedElementName();
			if (ZM_UI_Shop::IsExitElementName(szFocusedName))
			{
				PopTopScreen();
			}
			else
			{
				ZM_GameState* pxState = nullptr;
				if (ZM_GameStateManager::TryGetGameState(pxState) && pxState != nullptr)
				{
					m_xShop.Confirm(szFocusedName, pxState->m_xBag, pxState->m_uMoney);
				}
			}
		}
		else if (bCancel)
		{
			PopTopScreen();
		}
		break;

	case ZM_MENU_SCREEN_SAVE:
		// The save screen dispatches confirm BY THE FOCUSED ELEMENT'S NAME too (never
		// SetOnClick(this)). Back (or Cancel) pops back to whatever raised it; a row resolves
		// to an action the pure model owns, and only the STACK-level consequences (a confirm
		// prompt, a refusal line) are decided here.
		if (bConfirm)
		{
			const char* szFocusedName = ResolveFocusedElementName();
			if (ZM_UI_SaveSlots::IsCancelElementName(szFocusedName))
			{
				PopTopScreen();
			}
			else
			{
				ZM_SAVE_SLOT eSlot = ZM_SAVE_SLOT_NONE;
				switch (m_xSaveScreen.ResolveConfirm(szFocusedName, eSlot))
				{
				case ZM_SAVE_ROW_ACTION_WRITE:
					// SAVE + EMPTY manual slot: write straight away (no prompt).
					PerformSaveToSlot(eSlot);
					break;
				case ZM_SAVE_ROW_ACTION_CONFIRM_WRITE:
					// SAVE + READY/DAMAGED: yes/no first. The single-tenant box may refuse; that
					// simply eats the press rather than half-arming.
					OpenSaveConfirmPrompt(eSlot);
					break;
				case ZM_SAVE_ROW_ACTION_CONFIRM_LOAD:
					// LOAD + READY only (the pure resolver excludes EMPTY/DAMAGED). Arm one
					// input-driven Yes/No transaction; Auto is an ordinary load target here.
					OpenLoadConfirmPrompt(eSlot);
					break;
				case ZM_SAVE_ROW_ACTION_NONE:
				default:
				{
					// A non-confirmable row (Auto in SAVE, empty/damaged in LOAD): one refusal line
					// onto the empty box. Best-effort -- a busy box just eats it.
					const char* aszRefusal[1] = { "That slot can't be used." };
					PushDialogueLines(aszRefusal, 1u);
					break;
				}
				}
			}
		}
		else if (bCancel)
		{
			PopTopScreen();
		}
		break;

	// ROOT dispatches its focused entry by NAME.
	case ZM_MENU_SCREEN_ROOT:
	default:
		if (bConfirm)
		{
			HandleConfirm();
		}
		else if (bCancel)
		{
			PopTopScreen();
		}
		break;
	}

	// Refresh visibility / focus + mirror the cursor every frame the menu stays open
	// (CloseMenu leaves IsOpen()==false, so a close skips this).
	if (IsOpen())
	{
		PresentTopScreen();
	}
}

// ---- Session helpers -------------------------------------------------------

void ZM_UI_MenuStack::RefreshTitleMenu()
{
	ZM_SAVE_SLOT_STATUS aeStatuses[static_cast<u_int>(ZM_SAVE_SLOT_COUNT)] = {};
	for (u_int u = 0u; u < static_cast<u_int>(ZM_SAVE_SLOT_COUNT); ++u)
	{
		aeStatuses[u] = ZM_SaveSlots::ProbeSlot(static_cast<ZM_SAVE_SLOT>(u));
	}
	m_xTitle.Open(aeStatuses, static_cast<u_int>(ZM_SAVE_SLOT_COUNT));
}

void ZM_UI_MenuStack::OpenTitleMenu()
{
	RefreshTitleMenu();
	m_xStack.Clear();
	m_xStack.Push(ZM_MENU_SCREEN_TITLE);
	m_iCursor = static_cast<int>(m_xTitle.GetDefaultFocusItem());
	// FrontEnd is playerless: unlike ROOT, TITLE never freezes a controller.
	PresentTopScreen();
}

void ZM_UI_MenuStack::OpenRootMenu()
{
	m_xStack.Clear();
	m_xStack.Push(ZM_MENU_SCREEN_ROOT);
	m_iCursor = 0;
	FreezePlayer();
	PresentTopScreen();   // show root + focus the first entry
}

void ZM_UI_MenuStack::CloseMenu()
{
	m_xStack.Clear();
	m_iCursor = -1;
	// Drop EVERY screen's session state too: a force-close (ResetRuntimeStateForTests)
	// must not bleed a half-read conversation or an open party summary into the next
	// batched test / play session.
	m_xDialogue.Reset();
	m_xDialogue.Hide(m_xParentEntity);
	m_xParty.Reset();
	m_xParty.Hide(m_xParentEntity);
	m_xDex.Reset();
	m_xDex.Hide(m_xParentEntity);
	m_xBagScreen.Reset();
	m_xBagScreen.Hide(m_xParentEntity);
	m_xShop.Reset();
	m_xShop.Hide(m_xParentEntity);
	m_xSaveScreen.Reset();
	m_xSaveScreen.Hide(m_xParentEntity);
	m_xTitle.Hide(m_xParentEntity);
	m_xTitle.Open(nullptr, 0u);
	// ...including any pending prompt action and armed overwrite slot: a force-close must
	// never leave a HEAL_PARTY or a WRITE_SAVE_SLOT armed for whatever conversation comes
	// next. The last-save latch (status + count) deliberately survives, like
	// m_eLastDialogueAnswer -- CloseMenu runs on every ordinary close, not just teardown.
	m_ePendingSaveSlot = ZM_SAVE_SLOT_NONE;
	m_xLoadConfirm.Reset();
	m_eDialogueAction = ZM_DIALOGUE_ACTION_NONE;

	// Hide every ROOT element + clear the canvas focus so arrow keys never drive an
	// invisible menu on the shared persistent canvas (watch-out 2).
	if (Zenith_UIComponent* pxUI = ResolveUI())
	{
		SetRootElementsShown(*pxUI, false);
		pxUI->GetCanvas().SetFocusedElement(nullptr);
	}

	UnfreezePlayer();
}

void ZM_UI_MenuStack::HandleConfirm()
{
	// Only the ROOT screen dispatches by ROOT entry name; every other screen owns its
	// own confirm arm in OnUpdate.
	if (m_xStack.Top() != ZM_MENU_SCREEN_ROOT)
	{
		return;
	}

	// DISPATCH BY THE FOCUSED ELEMENT'S NAME -- never SetOnClick(this): a `this`
	// userdata dangles when the ECS pool relocates this component.
	const ZM_MENU_ACTION eAction = ResolveRootAction(ResolveFocusedElementName());
	if (eAction == ZM_MENU_ACTION_CLOSE)
	{
		CloseMenu();
		return;
	}
	if (eAction == ZM_MENU_ACTION_OPEN_SAVE)
	{
		// Not the generic push: the save screen must be OPENED (its Open() re-probes the
		// four slots and sets SAVE mode) before it is shown, which the bare stack push does
		// not do. RootActionToScreen(OPEN_SAVE) still names SCREEN_SAVE for the unit tests.
		OpenSaveScreen(ZM_SAVE_SCREEN_MODE_SAVE);
		return;
	}
	if (eAction == ZM_MENU_ACTION_QUIT_TO_TITLE)
	{
		// Quit is an ACTION, not a screen push: it raises the yes/no confirm over ROOT.
		OpenQuitConfirmPrompt();
		return;
	}
	const ZM_MENU_SCREEN eScreen = RootActionToScreen(eAction);
	if (eScreen != ZM_MENU_SCREEN_NONE)
	{
		m_xStack.Push(eScreen);   // PresentTopScreen raises it and hides ROOT this frame
	}
}

void ZM_UI_MenuStack::PopTopScreen()
{
	m_xStack.Pop();
	if (m_xStack.IsEmpty())
	{
		CloseMenu();
	}
	else if (m_xStack.Top() == ZM_MENU_SCREEN_TITLE)
	{
		// Returning from LOAD is a real reopen: disk may have changed while it was up.
		RefreshTitleMenu();
	}
	// else: back to a lower screen (ROOT) -- PresentTopScreen re-shows it this frame.
}

void ZM_UI_MenuStack::ApplyDialogueChoice(ZM_DIALOGUE_CHOICE eAnswer)
{
	// The action is consumed WHATEVER the answer was: one prompt, one action, so a NO (or
	// a cancel) can never leave a HEAL_PARTY armed for the next unrelated conversation. The
	// pending save slot rides the SAME consumption -- a NO on an overwrite prompt must not
	// leave a target armed for the next confirm.
	const ZM_DIALOGUE_ACTION eAction = m_eDialogueAction;
	m_eDialogueAction = ZM_DIALOGUE_ACTION_NONE;
	const ZM_SAVE_SLOT ePendingSlot = m_ePendingSaveSlot;
	m_ePendingSaveSlot = ZM_SAVE_SLOT_NONE;
	const ZM_SAVE_SLOT ePendingLoadSlot = m_xLoadConfirm.Resolve(eAnswer);

	// LATCH the answer on the HOST, FIRST and on EVERY path. The box's own GetChoice()
	// survives the resolve, but NOT the close: a prompt raised over an empty stack pops to
	// empty, which calls CloseMenu(), which Reset()s the box and clears the stored answer --
	// all synchronously inside this one OnUpdate, so no caller ever gets a frame in which it
	// could read it. The latch outlives that and is what consumers (and the windowed tests)
	// actually read. CloseMenu deliberately does NOT touch it (it is the LAST answer, not
	// live session state), so writing it here rather than after the pop is equivalent -- and
	// it keeps the healed-line early return below from having to repeat the assignment.
	m_eLastDialogueAnswer = eAnswer;

	if (eAnswer == ZM_DIALOGUE_CHOICE_YES
		&& eAction == ZM_DIALOGUE_ACTION_LOAD_SAVE_SLOT
		&& ePendingLoadSlot != ZM_SAVE_SLOT_NONE)
	{
		// ONE definitive transaction. RequestContinue owns the transactional disk read,
		// validated resume queue and publish; the host only records exactly one attempt.
		m_eLastLoadSlot = ePendingLoadSlot;
		++m_uLoadReadCount;
		m_xLastLoadStatus = ZM_GameStateManager::RequestContinue(ePendingLoadSlot);
		if (m_xLastLoadStatus.IsOk())
		{
			// Queue is already accepted and the candidate already published. Only now may
			// the title/load UI close; the latches above deliberately survive CloseMenu.
			CloseMenu();
			return;
		}

		// Failure leaves live state/transition untouched in the manager. Re-probe LOAD
		// because the definitive read may have discovered a newly damaged/missing file,
		// then keep this already-top DIALOGUE for one generic result line. Reading that
		// line to the end pops back to LOAD, never to TITLE.
		m_xSaveScreen.Open(ZM_SAVE_SCREEN_MODE_LOAD);
		if (m_xDialogue.QueueLine("Could not load that save."))
		{
			return;
		}
		PopTopScreen();
		return;
	}

	if (eAnswer == ZM_DIALOGUE_CHOICE_YES && eAction == ZM_DIALOGUE_ACTION_HEAL_PARTY)
	{
		// The LIVE state, resolved per answer rather than cached (the same seam the bag and
		// shop arms use); a missing state simply eats the heal instead of crashing.
		ZM_GameState* pxState = nullptr;
		if (ZM_GameStateManager::TryGetGameState(pxState) && pxState != nullptr
			&& ZM_ApplyCareCenterHeal(*pxState))
		{
			// The heal ACTUALLY changed something, so SAY SO instead of closing on a silent
			// button (an S8 manual-playthrough risk: a YES that heals and vanishes reads as a
			// dead control). ResolveChoice has already fully Reset() the box -- queue empty,
			// choice UNARMED -- so queueing one line leaves it active with nothing armed, and
			// the ORDINARY read-to-the-end CLOSED path pops it on the next confirm. No pop
			// here; OnUpdate's trailing PresentTopScreen raises the line this same frame.
			if (m_xDialogue.QueueLine(ZM_CareCenterHealedLine()))
			{
				return;
			}
			// The queue refused the line (it cannot, from an empty queue with a literal) --
			// fall through and close exactly as every other path does rather than strand the
			// player on a box with nothing in it.
		}
	}

	if (eAnswer == ZM_DIALOGUE_CHOICE_YES && eAction == ZM_DIALOGUE_ACTION_WRITE_SAVE_SLOT)
	{
		// The overwrite confirm resolved YES over a READY/DAMAGED manual slot. PerformSaveToSlot
		// queues its OWN result line onto the (already-reset, now unarmed) box -- ResolveChoice
		// reset it as part of resolving -- so the ordinary read-to-the-end path pops it and NO
		// pop happens here (the heal path's shape).
		PerformSaveToSlot(ePendingSlot);
		return;
	}
	if (eAnswer == ZM_DIALOGUE_CHOICE_YES && eAction == ZM_DIALOGUE_ACTION_QUIT_TO_TITLE)
	{
		// Quit confirmed: tear the whole menu down (unfreeze + clear focus) and hand off to the
		// SC3 playerless quit-to-title transition. CloseMenu empties the stack, so there is
		// nothing left for a trailing pop to do.
		CloseMenu();
		ZM_GameStateManager::RequestQuitToFrontEnd();
		return;
	}

	// The box already reset itself as part of resolving, so leaving is the stack's half of
	// the same close path a read-to-the-end dialogue takes.
	PopTopScreen();
}

// ---- Dialogue (SC2) --------------------------------------------------------

bool ZM_UI_MenuStack::PushDialogueLines(const char* const* paszLines, u_int uCount)
{
	// An UNANSWERED question owns the box (the mirror of OpenCareCenterPrompt's guard).
	// Without this, ordinary NPC lines queued while a prompt awaits its answer would push
	// the read cursor back below the line count, drop the box out of IsAwaitingChoice()
	// and strand the armed choice with its buttons hidden -- an unanswerable prompt.
	if (m_xDialogue.IsChoiceArmed())
	{
		return false;
	}
	if (!m_xDialogue.QueueLines(paszLines, uCount))
	{
		return false;
	}
	// An already-showing dialogue just gains lines: keep the in-flight reveal (the
	// stack already has DIALOGUE on top, and re-pushing would double the pop).
	if (m_xStack.Top() != ZM_MENU_SCREEN_DIALOGUE)
	{
		const bool bWasClosed = m_xStack.IsEmpty();
		if (!m_xStack.Push(ZM_MENU_SCREEN_DIALOGUE))
		{
			m_xDialogue.Reset();   // depth-limit: never leave a queue with no screen to show it
			return false;
		}
		if (bWasClosed)
		{
			FreezePlayer();   // an NPC can talk without the pause menu ever being opened
		}
	}
	PresentTopScreen();
	return true;
}

bool ZM_UI_MenuStack::TryPushDialogue(const char* const* paszLines, u_int uCount)
{
	Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xEntityID))
	{
		return false;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	ZM_UI_MenuStack* pxMenu = xEntity.IsValid()
		? xEntity.TryGetComponent<ZM_UI_MenuStack>()
		: nullptr;
	return pxMenu != nullptr && pxMenu->PushDialogueLines(paszLines, uCount);
}

// ---- Care Center prompt (SC8) ----------------------------------------------

bool ZM_UI_MenuStack::OpenCareCenterPrompt()
{
	// A prompt OWNS the box: refused outright while a conversation is still being read or
	// another choice is already armed. Refusing here (rather than stacking lines onto
	// whatever is showing) is what lets the two mutations below be all-or-nothing -- the
	// box is known empty, so a Reset on the rejection path can never destroy someone
	// else's half-read conversation.
	if (m_xDialogue.IsActive() || m_xDialogue.IsChoiceArmed())
	{
		return false;
	}

	const char* aszLines[1] = { ZM_CareCenterPromptLine() };
	if (!m_xDialogue.QueueLines(aszLines, 1u))
	{
		return false;
	}
	if (!m_xDialogue.ArmChoice(ZM_CareCenterYesLabel(), ZM_CareCenterNoLabel()))
	{
		m_xDialogue.Reset();   // never leave the question queued with no way to answer it
		return false;
	}
	m_eDialogueAction = ZM_DIALOGUE_ACTION_HEAL_PARTY;

	// From here it raises the screen exactly as PushDialogueLines does.
	if (m_xStack.Top() != ZM_MENU_SCREEN_DIALOGUE)
	{
		const bool bWasClosed = m_xStack.IsEmpty();
		if (!m_xStack.Push(ZM_MENU_SCREEN_DIALOGUE))
		{
			m_xDialogue.Reset();   // depth-limit: never leave a prompt with no screen to show it
			m_eDialogueAction = ZM_DIALOGUE_ACTION_NONE;
			return false;
		}
		if (bWasClosed)
		{
			FreezePlayer();   // the Care Center attendant can talk without the pause menu
		}
	}
	PresentTopScreen();
	return true;
}

bool ZM_UI_MenuStack::TryOpenCareCenterPrompt()
{
	Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xEntityID))
	{
		return false;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	ZM_UI_MenuStack* pxMenu = xEntity.IsValid()
		? xEntity.TryGetComponent<ZM_UI_MenuStack>()
		: nullptr;
	return pxMenu != nullptr && pxMenu->OpenCareCenterPrompt();
}

// ---- Shop (SC7) ------------------------------------------------------------

bool ZM_UI_MenuStack::OpenShop(const ZM_ITEM_ID* paeInventory, u_int uCount)
{
	// The stock is validated FIRST and all-or-nothing (SetInventory rejects a null list,
	// an empty / over-capacity one, and any out-of-range id), so a mis-authored mart
	// never raises a screen with nothing to sell -- and never wipes the stock of the
	// shop that is already open.
	if (!m_xShop.SetInventory(paeInventory, uCount))
	{
		return false;
	}
	// An already-showing shop just gains the new stock: re-pushing would double the pop.
	if (m_xStack.Top() != ZM_MENU_SCREEN_SHOP)
	{
		const bool bWasClosed = m_xStack.IsEmpty();
		if (!m_xStack.Push(ZM_MENU_SCREEN_SHOP))
		{
			m_xShop.Reset();   // depth-limit: never leave stock with no screen to show it
			return false;
		}
		if (bWasClosed)
		{
			FreezePlayer();   // a mart clerk can trade without the pause menu ever being opened
		}
	}
	PresentTopScreen();
	return true;
}

bool ZM_UI_MenuStack::TryOpenShop(const ZM_ITEM_ID* paeInventory, u_int uCount)
{
	Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xEntityID))
	{
		return false;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	ZM_UI_MenuStack* pxMenu = xEntity.IsValid()
		? xEntity.TryGetComponent<ZM_UI_MenuStack>()
		: nullptr;
	return pxMenu != nullptr && pxMenu->OpenShop(paeInventory, uCount);
}

// ---- Save / load slot screen (S7 item 2 SC4) -------------------------------

bool ZM_UI_MenuStack::OpenSaveScreen(ZM_SAVE_SCREEN_MODE eMode)
{
	// LOAD must remain available on FrontEnd, where the live policy necessarily reports
	// NOT_OVERWORLD. Every other value is SAVE (including Open()'s documented invalid-mode
	// fallback), and must be refused before probing slots or mutating the stack.
	const bool bSaveMode = eMode != ZM_SAVE_SCREEN_MODE_LOAD;
	if (bSaveMode
		&& ZM_SaveSlots::ResolveLiveSaveBlocker() != ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE)
	{
		return false;
	}

	// Open() re-probes all four slots (uncached) and sets the mode BEFORE the screen is
	// shown, so the very first Present renders the true disk state.
	m_xSaveScreen.Open(eMode);
	if (m_xStack.Top() != ZM_MENU_SCREEN_SAVE)
	{
		const bool bWasClosed = m_xStack.IsEmpty();
		if (!m_xStack.Push(ZM_MENU_SCREEN_SAVE))
		{
			return false;   // depth-limit: leave nothing raised
		}
		if (bWasClosed)
		{
			FreezePlayer();   // the SC5 title Continue path opens the screen with no menu underneath
		}
	}
	PresentTopScreen();
	return true;
}

bool ZM_UI_MenuStack::TryOpenSaveScreen(ZM_SAVE_SCREEN_MODE eMode)
{
	Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xEntityID))
	{
		return false;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	ZM_UI_MenuStack* pxMenu = xEntity.IsValid()
		? xEntity.TryGetComponent<ZM_UI_MenuStack>()
		: nullptr;
	return pxMenu != nullptr && pxMenu->OpenSaveScreen(eMode);
}

bool ZM_UI_MenuStack::OpenSaveConfirmPrompt(ZM_SAVE_SLOT eSlot)
{
	// This public seam is a manual-overwrite boundary in its own right: only Save0-2 may
	// ever arm it. Reject Auto, NONE and every out-of-range value before touching the
	// single-tenant dialogue box, stack, pending action or target.
	if (!ZM_SaveSlots::IsManualSlot(eSlot))
	{
		return false;
	}

	// A prompt OWNS the single-tenant box: refused outright while a conversation is being
	// read or a choice is already armed (Shortfalls.md:72). Refusing here (rather than
	// stacking lines onto whatever is showing) is what lets the two mutations below be
	// all-or-nothing.
	if (m_xDialogue.IsActive() || m_xDialogue.IsChoiceArmed())
	{
		return false;
	}
	char acPrompt[uSAVE_LINE_CAPACITY];
	std::snprintf(acPrompt, sizeof(acPrompt), "Overwrite %s?", ZM_SaveSlots::SlotDisplayName(eSlot));
	const char* aszLines[1] = { acPrompt };
	if (!m_xDialogue.QueueLines(aszLines, 1u))
	{
		return false;
	}
	if (!m_xDialogue.ArmChoice("Yes", "No"))
	{
		m_xDialogue.Reset();   // never leave the question queued with no way to answer it
		return false;
	}
	m_eDialogueAction = ZM_DIALOGUE_ACTION_WRITE_SAVE_SLOT;
	m_ePendingSaveSlot = eSlot;

	// From here it raises the DIALOGUE screen exactly as OpenCareCenterPrompt does.
	if (m_xStack.Top() != ZM_MENU_SCREEN_DIALOGUE)
	{
		const bool bWasClosed = m_xStack.IsEmpty();
		if (!m_xStack.Push(ZM_MENU_SCREEN_DIALOGUE))
		{
			m_xDialogue.Reset();
			m_eDialogueAction = ZM_DIALOGUE_ACTION_NONE;
			m_ePendingSaveSlot = ZM_SAVE_SLOT_NONE;
			return false;
		}
		if (bWasClosed)
		{
			FreezePlayer();
		}
	}
	PresentTopScreen();
	return true;
}

bool ZM_UI_MenuStack::OpenLoadConfirmPrompt(ZM_SAVE_SLOT eSlot)
{
	// This boundary is stricter than "valid slot": it may only be reached from the LOAD
	// presenter while that exact row is still READY. Auto is intentionally accepted.
	if (static_cast<u_int>(eSlot) >= static_cast<u_int>(ZM_SAVE_SLOT_COUNT)
		|| m_xSaveScreen.GetMode() != ZM_SAVE_SCREEN_MODE_LOAD
		|| m_xSaveScreen.GetRowStatus(static_cast<u_int>(eSlot)) != ZM_SAVE_SLOT_READY
		|| m_xLoadConfirm.IsArmed()
		|| m_xDialogue.IsActive()
		|| m_xDialogue.IsChoiceArmed())
	{
		return false;
	}

	char acPrompt[uSAVE_LINE_CAPACITY];
	std::snprintf(acPrompt, sizeof(acPrompt), "Load %s? Current progress will be lost.",
		ZM_SaveSlots::SlotDisplayName(eSlot));
	const char* aszLines[1] = { acPrompt };
	if (!m_xDialogue.QueueLines(aszLines, 1u))
	{
		return false;
	}
	if (!m_xDialogue.ArmChoice("Yes", "No"))
	{
		m_xDialogue.Reset();
		return false;
	}
	if (!m_xLoadConfirm.Arm(eSlot))
	{
		// All-or-nothing even if the pure one-shot model refuses unexpectedly.
		m_xDialogue.Reset();
		return false;
	}
	m_eDialogueAction = ZM_DIALOGUE_ACTION_LOAD_SAVE_SLOT;

	if (m_xStack.Top() != ZM_MENU_SCREEN_DIALOGUE)
	{
		const bool bWasClosed = m_xStack.IsEmpty();
		if (!m_xStack.Push(ZM_MENU_SCREEN_DIALOGUE))
		{
			m_xDialogue.Reset();
			m_xLoadConfirm.Reset();
			m_eDialogueAction = ZM_DIALOGUE_ACTION_NONE;
			return false;
		}
		if (bWasClosed)
		{
			FreezePlayer();
		}
	}
	PresentTopScreen();
	return true;
}

bool ZM_UI_MenuStack::OpenQuitConfirmPrompt()
{
	if (m_xDialogue.IsActive() || m_xDialogue.IsChoiceArmed())
	{
		return false;
	}
	const char* aszLines[1] = { "Quit to title? Unsaved progress will be lost." };
	if (!m_xDialogue.QueueLines(aszLines, 1u))
	{
		return false;
	}
	if (!m_xDialogue.ArmChoice("Yes", "No"))
	{
		m_xDialogue.Reset();
		return false;
	}
	m_eDialogueAction = ZM_DIALOGUE_ACTION_QUIT_TO_TITLE;

	if (m_xStack.Top() != ZM_MENU_SCREEN_DIALOGUE)
	{
		const bool bWasClosed = m_xStack.IsEmpty();
		if (!m_xStack.Push(ZM_MENU_SCREEN_DIALOGUE))
		{
			m_xDialogue.Reset();
			m_eDialogueAction = ZM_DIALOGUE_ACTION_NONE;
			return false;
		}
		if (bWasClosed)
		{
			FreezePlayer();
		}
	}
	PresentTopScreen();
	return true;
}

void ZM_UI_MenuStack::PerformSaveToSlot(ZM_SAVE_SLOT eSlot)
{
	// Permission is rechecked at the irreversible boundary: a blocker can arise after the
	// SAVE screen opened or while an overwrite prompt was awaiting its answer. Fail closed
	// before resolving/capturing state, writing, updating latches, probing, or result UI.
	if (ZM_SaveSlots::ResolveLiveSaveBlocker() != ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE)
	{
		return;
	}

	// Resolve the LIVE game state (the same seam the bag / shop / heal arms use); without
	// one there is nothing to save, so latch a failed status rather than crash.
	ZM_GameState* pxState = nullptr;
	if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
	{
		m_xLastSaveStatus = Zenith_ErrorCode::INVALID_ARGUMENT;
	}
	else
	{
		// Capture the live world position (scene + body CENTRE + yaw + arrived tag) into the
		// state FIRST, so the slot records where the player is standing. Best-effort: a context
		// with no unique bodied player leaves the last-known position in place, and WriteState
		// still persists the rest of the state. WriteState never writes Auto here -- the pure
		// resolver already excluded it -- and re-probes internally to prove the file landed.
		ZM_GameStateManager::CaptureWorldPosition(*pxState);
		m_xLastSaveStatus = ZM_SaveSlots::WriteState(*pxState, eSlot);
	}
	++m_uSaveWriteCount;

	// Reflect the new status: re-probe all four slots (uncached), keeping the current mode.
	m_xSaveScreen.Open(m_xSaveScreen.GetMode());

	// Queue a result line onto the box. It is EMPTY here (either the SAVE screen is on top,
	// or the confirm prompt already Reset the box when it resolved), so this never appends
	// to a live conversation -- the 8-line cap is cumulative per conversation
	// (Shortfalls.md:72). PushDialogueLines stacks DIALOGUE on top of SAVE (or keeps the
	// prompt's DIALOGUE screen) and the ordinary read-to-the-end path pops it.
	char acLine[uSAVE_LINE_CAPACITY];
	if (m_xLastSaveStatus.IsOk())
	{
		std::snprintf(acLine, sizeof(acLine), "Saved to %s.", ZM_SaveSlots::SlotDisplayName(eSlot));
	}
	else
	{
		std::snprintf(acLine, sizeof(acLine), "Could not save.");
	}
	const char* aszLines[1] = { acLine };
	PushDialogueLines(aszLines, 1u);
}

void ZM_UI_MenuStack::PresentTopScreen()
{
	Zenith_UIComponent* pxUI = ResolveUI();
	if (pxUI == nullptr)
	{
		return;   // best-effort: a missing UI component never crashes the menu
	}
	Zenith_UI::Zenith_UICanvas& xCanvas = pxUI->GetCanvas();
	const ZM_MENU_SCREEN eTop = m_xStack.Top();

	// ---- Show/hide, one step per screen. Each step owns exactly ITS OWN elements and
	//      hides them for every other top screen, so adding a screen is one more step
	//      plus one more focus arm below -- never a reshape of this function. ----
	SetRootElementsShown(*pxUI, eTop == ZM_MENU_SCREEN_ROOT);
	if (eTop == ZM_MENU_SCREEN_TITLE)
	{
		m_xTitle.Present(m_xParentEntity);
	}
	else
	{
		m_xTitle.Hide(m_xParentEntity);
	}
	if (eTop == ZM_MENU_SCREEN_DIALOGUE)
	{
		m_xDialogue.Present(m_xParentEntity);
	}
	else
	{
		m_xDialogue.Hide(m_xParentEntity);
	}
	const bool bPartyPresented = PresentPartyScreen(eTop == ZM_MENU_SCREEN_PARTY);
	const bool bDexPresented = PresentDexScreen(eTop == ZM_MENU_SCREEN_DEX);
	const bool bBagPresented = PresentBagScreen(eTop == ZM_MENU_SCREEN_BAG);
	const bool bShopPresented = PresentShopScreen(eTop == ZM_MENU_SCREEN_SHOP);
	const bool bSavePresented = PresentSaveScreen(eTop == ZM_MENU_SCREEN_SAVE);

	// ---- Focus policy. A FOCUS-NAVIGABLE screen owns the canvas focus and mirrors it
	//      into m_iCursor; every other screen clears both, so arrows can never drive a
	//      hidden screen's entries (watch-out 2). ----
	switch (eTop)
	{
	case ZM_MENU_SCREEN_TITLE:
		// ZM_UI_TitleMenu::Present repairs stale/hidden focus and owns its dynamic nav.
		// Mirror the actual focused title item; -1 exposes a missing authored control.
		m_iCursor = ZM_UI_TitleMenu::ItemIndexFromElementName(ResolveFocusedElementName());
		break;

	case ZM_MENU_SCREEN_ROOT:
	{
		// Ensure a focused ROOT entry (freshly opened, or returned from a sub-screen
		// where focus was cleared / left on that screen). Otherwise mirror the
		// engine-navigated focus.
		Zenith_UI::Zenith_UIElement* pxFocused = xCanvas.GetFocusedElement();
		const int iFocusedIdx = (pxFocused != nullptr)
			? RootItemIndexFromElementName(pxFocused->GetName().c_str())
			: -1;
		if (iFocusedIdx < 0)
		{
			xCanvas.SetFocusedElement(pxUI->FindElement(RootItemElementName(ZM_MENU_ROOT_PARTY)));
			m_iCursor = 0;
		}
		else
		{
			m_iCursor = iFocusedIdx;
		}
		break;
	}

	case ZM_MENU_SCREEN_PARTY:
		if (bPartyPresented)
		{
			// ZM_UI_Party::Present already ensured a focused VISIBLE slot and mirrored the
			// engine-navigated focus; just carry its cursor up.
			m_iCursor = m_xParty.GetCursor();
		}
		else
		{
			// The screen could not be presented (no live game state) and its widgets were
			// just hidden, so it degrades to the non-navigable policy -- otherwise the
			// canvas focus would stay parked on the ROOT entry hidden two lines above.
			xCanvas.SetFocusedElement(nullptr);
			m_iCursor = -1;
		}
		break;

	case ZM_MENU_SCREEN_DEX:
		if (bDexPresented)
		{
			// ZM_UI_Dex::Present already ensured a focused LIVE cell and mirrored the
			// engine-navigated focus; just carry its cursor up (-1 on a page button).
			m_iCursor = m_xDex.GetCursor();
		}
		else
		{
			// Same degradation as PARTY: the widgets were just hidden, so leaving the focus
			// parked on the ROOT entry hidden a few lines above would let the arrows drive
			// an invisible menu.
			xCanvas.SetFocusedElement(nullptr);
			m_iCursor = -1;
		}
		break;

	case ZM_MENU_SCREEN_BAG:
		if (bBagPresented)
		{
			// ZM_UI_Bag::Present already ensured a focused LIVE row (or a nav button over an
			// empty pocket) and mirrored the engine-navigated focus; carry its cursor up.
			m_iCursor = m_xBagScreen.GetCursor();
		}
		else
		{
			// Same degradation as PARTY / DEX: the widgets were just hidden, so leaving the
			// focus parked on the ROOT entry hidden a few lines above would let the arrows
			// drive an invisible menu.
			xCanvas.SetFocusedElement(nullptr);
			m_iCursor = -1;
		}
		break;

	case ZM_MENU_SCREEN_SHOP:
		if (bShopPresented)
		{
			// ZM_UI_Shop::Present already ensured a focused LIVE row (or, over an empty list,
			// a control) and settled the selection; carry its cursor up. Unlike the bag's, this
			// cursor stays put while the focus walks onto a control -- that is the selection the
			// Confirm button transacts on.
			m_iCursor = m_xShop.GetCursor();
		}
		else
		{
			// Same degradation as PARTY / DEX / BAG: the widgets were just hidden, so leaving
			// the focus parked on the ROOT entry hidden a few lines above would let the arrows
			// drive an invisible menu.
			xCanvas.SetFocusedElement(nullptr);
			m_iCursor = -1;
		}
		break;

	case ZM_MENU_SCREEN_SAVE:
		if (bSavePresented)
		{
			// ZM_UI_SaveSlots::Present already ensured a focused row (rows are always shown, so
			// there is always one to focus) and mirrored the engine-navigated focus; carry its
			// selection up (-1 while the Back button holds the focus).
			m_iCursor = m_xSaveScreen.GetSelectedRow();
		}
		else
		{
			// Only reachable if the UI component vanished mid-frame: degrade like the others so
			// the arrows can never drive a hidden screen.
			xCanvas.SetFocusedElement(nullptr);
			m_iCursor = -1;
		}
		break;

	case ZM_MENU_SCREEN_DIALOGUE:
		// A dialogue advances on a confirm press, NOT focus-nav, so it clears the focus --
		// EXCEPT while its SC8 yes/no prompt is awaiting an answer, where the two choice
		// buttons ARE the navigable surface and ZM_UI_DialogueBox::Present has just parked
		// the focus on one of them (clearing it here would kill the arrow keys and leave the
		// question unanswerable). The cursor mirror stays -1 either way: the box has no list.
		if (!m_xDialogue.IsAwaitingChoice())
		{
			xCanvas.SetFocusedElement(nullptr);
		}
		m_iCursor = -1;
		break;

	default:
		xCanvas.SetFocusedElement(nullptr);
		m_iCursor = -1;
		break;
	}
}

void ZM_UI_MenuStack::SetRootElementsShown(Zenith_UIComponent& xUI, bool bShown)
{
	if (Zenith_UI::Zenith_UIElement* pxPanel = xUI.FindElement(szROOT_PANEL_NAME))
	{
		if (pxPanel->IsVisible() != bShown)
		{
			pxPanel->SetVisible(bShown);
		}
	}

	// The SAVE entry is SUPPRESSED whenever the live context forbids saving (FrontEnd, a
	// battle / warp transition, a pending whiteout): it is hidden, unfocusable, AND removed
	// from the nav chain. ZM-D-119: the engine follows an explicit nav link FIRST and drops
	// it with NO spatial fallback when its target is hidden, so a bake-time DEX->SAVE link
	// would swallow the press while SAVE is gone. DEX must therefore point straight at QUIT.
	// Re-evaluated every frame ROOT is shown, so SAVE reappears the instant saving is legal
	// again. Consulted only while showing -- a hide never asks the live singletons anything.
	const bool bSaveBlocked = bShown
		&& (ZM_SaveSlots::ResolveLiveSaveBlocker() != ZM_SaveSlots::ZM_SAVE_BLOCKER_NONE);

	Zenith_UI::Zenith_UIElement* apxItems[ZM_MENU_ROOT_ITEM_COUNT] = {};
	bool abInChain[ZM_MENU_ROOT_ITEM_COUNT] = {};
	bool bRehomeSuppressedSaveFocus = false;
	for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
	{
		const bool bSuppressed = (i == static_cast<u_int>(ZM_MENU_ROOT_SAVE)) && bSaveBlocked;
		abInChain[i] = bShown && !bSuppressed;
		Zenith_UI::Zenith_UIElement* pxItem =
			xUI.FindElement(RootItemElementName(static_cast<ZM_MENU_ROOT_ITEM>(i)));
		apxItems[i] = pxItem;
		if (pxItem == nullptr)
		{
			continue;
		}
		if (pxItem->IsVisible() != abInChain[i])
		{
			// Re-home only on the live visible -> suppressed transition, and only when Save
			// itself owns focus. A repeated hidden frame or another focused ROOT entry must
			// leave the canvas focus untouched.
			if (bSuppressed && xUI.GetCanvas().GetFocusedElement() == pxItem)
			{
				bRehomeSuppressedSaveFocus = true;
			}
			pxItem->SetVisible(abInChain[i]);
		}
		pxItem->SetFocusable(abInChain[i]);   // nav collects only visible + focusable elements
	}

	if (!bShown)
	{
		return;   // nothing shown, nothing to wire
	}
	if (bRehomeSuppressedSaveFocus)
	{
		// Quit is the deterministic nearest live entry below Save in the authored ROOT order.
		// It has already been made visible + focusable above, so the canvas never retains a
		// pointer to the newly hidden Save entry and focused-name dispatch remains valid.
		Zenith_UI::Zenith_UIElement* pxQuit = apxItems[static_cast<u_int>(ZM_MENU_ROOT_QUIT)];
		if (pxQuit != nullptr && pxQuit->IsVisible() && pxQuit->IsFocusable())
		{
			xUI.GetCanvas().SetFocusedElement(pxQuit);
		}
	}

	// Re-wire up/down across the IN-CHAIN items only, skipping a suppressed SAVE, so no live
	// link ever points at a hidden entry. When SAVE is present this restores the authored
	// six-item chain; when it is blocked DEX links to QUIT and QUIT back to DEX.
	for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
	{
		if (apxItems[i] == nullptr || !abInChain[i])
		{
			continue;
		}
		Zenith_UI::Zenith_UIElement* pxUp = nullptr;
		for (int j = static_cast<int>(i) - 1; j >= 0; --j)
		{
			if (abInChain[j] && apxItems[j] != nullptr) { pxUp = apxItems[j]; break; }
		}
		Zenith_UI::Zenith_UIElement* pxDown = nullptr;
		for (u_int j = i + 1u; j < ZM_MENU_ROOT_ITEM_COUNT; ++j)
		{
			if (abInChain[j] && apxItems[j] != nullptr) { pxDown = apxItems[j]; break; }
		}
		apxItems[i]->SetNavigation(pxUp, pxDown, nullptr, nullptr);
	}
}

bool ZM_UI_MenuStack::PresentPartyScreen(bool bShown)
{
	ZM_GameState* pxState = nullptr;
	if (!bShown
		|| !ZM_GameStateManager::TryGetGameState(pxState)
		|| pxState == nullptr)
	{
		// Not the top screen, or no live game state (headless / between scenes): hide
		// the screen rather than crash on a party that cannot be resolved.
		m_xParty.Hide(m_xParentEntity);
		return false;
	}
	m_xParty.Present(m_xParentEntity, pxState->m_xParty);
	return true;
}

bool ZM_UI_MenuStack::PresentDexScreen(bool bShown)
{
	ZM_GameState* pxState = nullptr;
	if (!bShown
		|| !ZM_GameStateManager::TryGetGameState(pxState)
		|| pxState == nullptr)
	{
		// Not the top screen, or no live game state (headless / between scenes): hide the
		// screen rather than page over a caught set that cannot be resolved.
		m_xDex.Hide(m_xParentEntity);
		return false;
	}
	m_xDex.Present(m_xParentEntity, pxState->m_xCaught);
	return true;
}

bool ZM_UI_MenuStack::PresentBagScreen(bool bShown)
{
	ZM_GameState* pxState = nullptr;
	if (!bShown
		|| !ZM_GameStateManager::TryGetGameState(pxState)
		|| pxState == nullptr)
	{
		// Not the top screen, or no live game state (headless / between scenes): hide the
		// screen rather than list a bag that cannot be resolved.
		m_xBagScreen.Hide(m_xParentEntity);
		return false;
	}
	m_xBagScreen.Present(m_xParentEntity, pxState->m_xBag, pxState->m_uMoney);
	return true;
}

bool ZM_UI_MenuStack::PresentShopScreen(bool bShown)
{
	ZM_GameState* pxState = nullptr;
	if (!bShown
		|| !ZM_GameStateManager::TryGetGameState(pxState)
		|| pxState == nullptr)
	{
		// Not the top screen, or no live game state (headless / between scenes): hide the
		// screen rather than trade against a bag and a purse that cannot be resolved.
		m_xShop.Hide(m_xParentEntity);
		return false;
	}
	// PRESENTATION IS READ-ONLY over the model -- the money is passed by value here on
	// purpose. The only write path is the Confirm arm in OnUpdate.
	m_xShop.Present(m_xParentEntity, pxState->m_xBag, pxState->m_uMoney);
	return true;
}

bool ZM_UI_MenuStack::PresentSaveScreen(bool bShown)
{
	if (!bShown)
	{
		m_xSaveScreen.Hide(m_xParentEntity);
		return false;
	}
	// Deliberately NO live-game-state gate (unlike party / dex / bag / shop): the slot list
	// is read from DISK, not the game state, and the LOAD half runs on the title screen
	// where no game state exists yet. The screen owns its own probing (Open), so Present is
	// a pure show / label / focus pass.
	m_xSaveScreen.Present(m_xParentEntity);
	return true;
}

void ZM_UI_MenuStack::FreezePlayer()
{
	m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;

	Zenith_EntityID xPlayerID = INVALID_ENTITY_ID;
	if (!ZM_GameStateManager::TryGetUniqueActiveScenePlayerEntityID(xPlayerID))
	{
		return;   // no unique active-scene player (headless / between scenes) -- nothing to freeze
	}
	Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(xPlayerID);
	ZM_PlayerController* pxController = xPlayer.IsValid()
		? xPlayer.TryGetComponent<ZM_PlayerController>()
		: nullptr;
	if (pxController == nullptr)
	{
		return;
	}
	// The same movement-enable seam the warp / battle machines use (not teleportation;
	// no position is written). The overworld scene is NOT paused -- the pause menu is a
	// lightweight overlay, so only the controller's input-driven motion is disabled.
	pxController->SetMovementEnabled(false);
	m_xFrozenPlayerEntityID = xPlayerID;
}

void ZM_UI_MenuStack::UnfreezePlayer()
{
	if (m_xFrozenPlayerEntityID == INVALID_ENTITY_ID)
	{
		return;
	}
	const Zenith_EntityID xPlayerID = m_xFrozenPlayerEntityID;
	m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;

	// Coordinate with the warp / battle owners: if one of them started while the menu
	// was open it now owns the player's movement-enable, so leave it frozen for them
	// (watch-out 4). In the normal case (no transition) re-enable movement.
	if (ZM_GameStateManager::IsWarpInProgress()
		|| ZM_BattleTransition::IsTransitionActive())
	{
		return;
	}
	Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(xPlayerID);
	if (ZM_PlayerController* pxController = xPlayer.IsValid()
		? xPlayer.TryGetComponent<ZM_PlayerController>()
		: nullptr)
	{
		pxController->SetMovementEnabled(true);
	}
}

Zenith_UIComponent* ZM_UI_MenuStack::ResolveUI() const
{
	return m_xParentEntity.IsValid()
		? m_xParentEntity.TryGetComponent<Zenith_UIComponent>()
		: nullptr;
}

const char* ZM_UI_MenuStack::ResolveFocusedElementName() const
{
	Zenith_UIComponent* pxUI = ResolveUI();
	if (pxUI == nullptr)
	{
		return nullptr;
	}
	Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
	return (pxFocused != nullptr) ? pxFocused->GetName().c_str() : nullptr;
}

// ---- Persistent-singleton observation --------------------------------------

bool ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(Zenith_EntityID& xEntityIDOut)
{
	xEntityIDOut = INVALID_ENTITY_ID;
	if (s_xSingletonEntityID == INVALID_ENTITY_ID)
	{
		return false;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(s_xSingletonEntityID);
	if (!xEntity.IsValid() || xEntity.TryGetComponent<ZM_UI_MenuStack>() == nullptr)
	{
		return false;
	}
	xEntityIDOut = s_xSingletonEntityID;
	return true;
}

void ZM_UI_MenuStack::ResetRuntimeStateForTests()
{
	Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xEntityID))
	{
		return;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	if (ZM_UI_MenuStack* pxMenu = xEntity.IsValid()
		? xEntity.TryGetComponent<ZM_UI_MenuStack>()
		: nullptr)
	{
		if (pxMenu->IsOpen())
		{
			pxMenu->CloseMenu();   // unfreeze + clear focus + hide the root
		}
		// The latched answer deliberately survives CloseMenu (a resolved prompt closes the
		// menu and the answer must outlive that pop), and it is otherwise cleared only in
		// OnStart / ReadFromDataStream. A batched test process never re-boots and ZM_MenuRoot
		// is DontDestroyOnLoad, so without this the NEXT test inherits this one's answer and
		// any "the prompt answered YES" assertion passes without a press. The between-tests
		// reset is exactly the right place to drop it.
		pxMenu->m_eLastDialogueAnswer = ZM_DIALOGUE_CHOICE_NONE;
		// UNCONDITIONAL (outside the IsOpen guard above): ResetRuntimeStateForTests only
		// CloseMenu()s when the menu is OPEN, but the save screen's cached slot statuses, the
		// pending overwrite slot, the write count and the last-save latch are by-value
		// presenter / latch state that a test ending CLOSED would otherwise leak into the next
		// batched test -- the "new by-value presenter" trap.
		pxMenu->m_xSaveScreen.Reset();
		pxMenu->m_ePendingSaveSlot = ZM_SAVE_SLOT_NONE;
		pxMenu->m_xLastSaveStatus = Zenith_ErrorCode::INVALID_ARGUMENT;
		pxMenu->m_uSaveWriteCount = 0u;
		pxMenu->m_xTitle.Hide(pxMenu->m_xParentEntity);
		pxMenu->m_xTitle.Open(nullptr, 0u);
		pxMenu->m_xLoadConfirm.Reset();
		pxMenu->m_eLastLoadSlot = ZM_SAVE_SLOT_NONE;
		pxMenu->m_xLastLoadStatus = Zenith_ErrorCode::INVALID_ARGUMENT;
		pxMenu->m_uLoadReadCount = 0u;
		pxMenu->m_eDialogueAction = ZM_DIALOGUE_ACTION_NONE;
	}
}

bool ZM_UI_MenuStack::IsMenuOpen()
{
	// Same resolve-then-ask shape as the Try* seams. An unresolved singleton is a
	// plain FALSE, never an assert: headless unit boots and the FrontEnd have no
	// menu entity at all, and "there is no menu, so no menu is open" is the honest
	// answer for the interaction gate to act on.
	Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xEntityID))
	{
		return false;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	const ZM_UI_MenuStack* pxMenu = xEntity.IsValid()
		? xEntity.TryGetComponent<ZM_UI_MenuStack>()
		: nullptr;
	return pxMenu != nullptr && pxMenu->IsOpen();
}

// ---- PURE decision surface (unit-tested) -----------------------------------

bool ZM_UI_MenuStack::ShouldOpenMenu(bool bMenuKeyPressed, bool bAlreadyOpen,
	bool bOverworld, bool bWarpInProgress, bool bBattleTransitionActive)
{
	return bMenuKeyPressed
		&& !bAlreadyOpen
		&& bOverworld
		&& !bWarpInProgress
		&& !bBattleTransitionActive;
}

bool ZM_UI_MenuStack::IsOverworldSceneKind(ZM_SCENE_KIND eKind)
{
	// Everything except the title screen and the additive battle scene.
	return eKind != ZM_SCENE_KIND_FRONTEND && eKind != ZM_SCENE_KIND_BATTLE;
}

ZM_MENU_ACTION ZM_UI_MenuStack::ResolveRootAction(const char* szFocusedElementName)
{
	if (szFocusedElementName == nullptr)
	{
		return ZM_MENU_ACTION_NONE;
	}
	if (std::strcmp(szFocusedElementName, szROOT_PARTY_NAME) == 0) { return ZM_MENU_ACTION_OPEN_PARTY;    }
	if (std::strcmp(szFocusedElementName, szROOT_BAG_NAME)   == 0) { return ZM_MENU_ACTION_OPEN_BAG;      }
	if (std::strcmp(szFocusedElementName, szROOT_DEX_NAME)   == 0) { return ZM_MENU_ACTION_OPEN_DEX;      }
	if (std::strcmp(szFocusedElementName, szROOT_SAVE_NAME)  == 0) { return ZM_MENU_ACTION_OPEN_SAVE;     }
	if (std::strcmp(szFocusedElementName, szROOT_QUIT_NAME)  == 0) { return ZM_MENU_ACTION_QUIT_TO_TITLE; }
	if (std::strcmp(szFocusedElementName, szROOT_EXIT_NAME)  == 0) { return ZM_MENU_ACTION_CLOSE;         }
	return ZM_MENU_ACTION_NONE;
}

const char* ZM_UI_MenuStack::RootItemElementName(ZM_MENU_ROOT_ITEM eItem)
{
	switch (eItem)
	{
	case ZM_MENU_ROOT_PARTY: return szROOT_PARTY_NAME;
	case ZM_MENU_ROOT_BAG:   return szROOT_BAG_NAME;
	case ZM_MENU_ROOT_DEX:   return szROOT_DEX_NAME;
	case ZM_MENU_ROOT_SAVE:  return szROOT_SAVE_NAME;
	case ZM_MENU_ROOT_QUIT:  return szROOT_QUIT_NAME;
	case ZM_MENU_ROOT_EXIT:  return szROOT_EXIT_NAME;
	default:                 return "";
	}
}

int ZM_UI_MenuStack::RootItemIndexFromElementName(const char* szElementName)
{
	if (szElementName == nullptr)
	{
		return -1;
	}
	for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
	{
		if (std::strcmp(szElementName, RootItemElementName(static_cast<ZM_MENU_ROOT_ITEM>(i))) == 0)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

ZM_MENU_SCREEN ZM_UI_MenuStack::RootActionToScreen(ZM_MENU_ACTION eAction)
{
	switch (eAction)
	{
	case ZM_MENU_ACTION_OPEN_PARTY: return ZM_MENU_SCREEN_PARTY;
	case ZM_MENU_ACTION_OPEN_BAG:   return ZM_MENU_SCREEN_BAG;
	case ZM_MENU_ACTION_OPEN_DEX:   return ZM_MENU_SCREEN_DEX;
	case ZM_MENU_ACTION_OPEN_SAVE:  return ZM_MENU_SCREEN_SAVE;
	// CLOSE / QUIT_TO_TITLE / NONE push no screen: they run as ACTIONS (HandleConfirm owns
	// OPEN_SAVE's probing raise and QUIT_TO_TITLE's confirm) rather than a bare stack push.
	default:                        return ZM_MENU_SCREEN_NONE;
	}
}

ZM_MENU_SCREEN ZM_UI_MenuStack::TitleActionToScreen(ZM_TITLE_ACTION eAction)
{
	return (eAction == ZM_TITLE_ACTION_OPEN_LOAD)
		? ZM_MENU_SCREEN_SAVE
		: ZM_MENU_SCREEN_NONE;
}

ZM_SAVE_SCREEN_MODE ZM_UI_MenuStack::TitleActionToSaveMode(ZM_TITLE_ACTION eAction)
{
	return (eAction == ZM_TITLE_ACTION_OPEN_LOAD)
		? ZM_SAVE_SCREEN_MODE_LOAD
		: ZM_SAVE_SCREEN_MODE_COUNT;
}

bool ZM_UI_MenuStack::IsActiveSceneFrontEnd()
{
	const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
	const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(xActive);
	return xInfo.m_bLoaded
		&& xInfo.m_iBuildIndex == static_cast<int>(ZM_GameStateManager::uFRONTEND_BUILD_INDEX);
}

bool ZM_UI_MenuStack::IsActiveSceneOverworld()
{
	const Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
	const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(xActive);
	if (!xInfo.m_bLoaded || xInfo.m_iBuildIndex < 0)
	{
		return false;
	}
	const ZM_SCENE_ID eScene = ZM_FindSceneByBuildIndex(static_cast<u_int>(xInfo.m_iBuildIndex));
	if (eScene == ZM_SCENE_NONE)
	{
		return false;
	}
	return IsOverworldSceneKind(ZM_GetWorldSpec(eScene).m_eKind);
}

// ---- Serialization ---------------------------------------------------------

void ZM_UI_MenuStack::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// No live session state persists -- a version stamp is enough; the menu always
	// deserializes CLOSED (mirrors ZM_BattleDirector).
	xStream << uSERIALIZATION_VERSION;
}

void ZM_UI_MenuStack::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;

	// Reset-first: never retain a stale open menu / queued dialogue / open party summary
	// / dex page / bag pocket / shop stock from a reused instance.
	m_xStack.Clear();
	m_xDialogue.Reset();
	m_xParty.Reset();
	m_xDex.Reset();
	m_xBagScreen.Reset();
	m_xShop.Reset();
	m_xSaveScreen.Reset();
	m_xTitle.Open(nullptr, 0u);
	m_ePendingSaveSlot = ZM_SAVE_SLOT_NONE;
	m_xLoadConfirm.Reset();
	m_eDialogueAction = ZM_DIALOGUE_ACTION_NONE;
	// The latched answer is deliberately NOT cleared by CloseMenu (it must survive the
	// close that resolving a prompt triggers), so boot and deserialize are the only
	// places a stale one can be dropped. The save latch shares that lifetime.
	m_eLastDialogueAnswer = ZM_DIALOGUE_CHOICE_NONE;
	m_xLastSaveStatus = Zenith_ErrorCode::INVALID_ARGUMENT;
	m_uSaveWriteCount = 0u;
	m_eLastLoadSlot = ZM_SAVE_SLOT_NONE;
	m_xLastLoadStatus = Zenith_ErrorCode::INVALID_ARGUMENT;
	m_uLoadReadCount = 0u;
	m_iCursor = -1;
	m_xFrozenPlayerEntityID = INVALID_ENTITY_ID;
	(void)uVersion;
}

#ifdef ZENITH_TOOLS
void ZM_UI_MenuStack::RenderPropertiesPanel()
{
	ImGui::Text("Menu stack - open=%s depth=%u top=%u cursor=%d",
		IsOpen() ? "true" : "false",
		m_xStack.GetDepth(),
		static_cast<u_int>(m_xStack.Top()),
		m_iCursor);
	ImGui::Text("Dialogue - active=%s line=%u/%u revealed=%s",
		m_xDialogue.IsActive() ? "true" : "false",
		m_xDialogue.GetCurrentLineIndex(),
		m_xDialogue.GetQueuedLineCount(),
		m_xDialogue.IsRevealComplete() ? "true" : "false");
	ImGui::Text("Prompt - armed=%s awaiting=%s answer=%u action=%u ('%s' / '%s')",
		m_xDialogue.IsChoiceArmed() ? "true" : "false",
		m_xDialogue.IsAwaitingChoice() ? "true" : "false",
		static_cast<u_int>(m_xDialogue.GetChoice()),
		static_cast<u_int>(m_eDialogueAction),
		m_xDialogue.GetYesLabel().c_str(),
		m_xDialogue.GetNoLabel().c_str());
	ImGui::Text("Party - slot=%d summary=%s",
		m_xParty.GetCursor(),
		m_xParty.IsSummaryOpen() ? "open" : "closed");
	ImGui::Text("Dex - page=%d/%u cell=%d",
		m_xDex.GetPage(),
		ZM_UI_Dex::PageCount(static_cast<u_int>(ZM_SPECIES_COUNT)),
		m_xDex.GetCursor());

	// The bag's page COUNT depends on the live pocket, so it is read from the game
	// state when one resolves (the panel must stay a read-only observer).
	ZM_GameState* pxState = nullptr;
	const u_int uBagStacks = (ZM_GameStateManager::TryGetGameState(pxState) && pxState != nullptr)
		? pxState->m_xBag.PocketStackCount(m_xBagScreen.GetPocket())
		: 0u;
	ImGui::Text("Bag - pocket=%s page=%d/%u row=%d stacks=%u",
		ZM_ItemCategoryToString(m_xBagScreen.GetPocket()),
		m_xBagScreen.GetPage(),
		ZM_UI_Bag::PageCount(uBagStacks),
		m_xBagScreen.GetCursor(),
		uBagStacks);

	// The shop's page COUNT depends on the live list (its own stock in BUY, the player's
	// stacks in SELL), so it is read from the same state when one resolves.
	const u_int uShopEntries = (pxState != nullptr)
		? m_xShop.EntryCount(pxState->m_xBag)
		: m_xShop.GetInventoryCount();
	// row=%d, matching the bag's field: the shop's cursor is a PAGE-RELATIVE row, not a
	// flat list index (labelling it "entry" would misread page 1 row 0 as entry 0).
	ImGui::Text("Shop - mode=%s page=%d/%u qty=%u row=%d stock=%u entries=%u result=%s",
		ZM_UI_Shop::ModeToString(m_xShop.GetMode()),
		m_xShop.GetPage(),
		ZM_UI_Shop::PageCount(uShopEntries),
		m_xShop.GetQuantity(),
		m_xShop.GetCursor(),
		m_xShop.GetInventoryCount(),
		uShopEntries,
		m_xShop.HasResult() ? ZM_UI_Shop::FormatResult(m_xShop.GetLastResult()) : "(none yet)");

	ImGui::Text("Save - mode=%u row=%d pending=%u writes=%u lastOk=%s blocker=%s",
		static_cast<u_int>(m_xSaveScreen.GetMode()),
		m_xSaveScreen.GetSelectedRow(),
		static_cast<u_int>(m_ePendingSaveSlot),
		m_uSaveWriteCount,
		m_xLastSaveStatus.IsOk() ? "true" : "false",
		ZM_SaveSlots::SaveBlockerName(ZM_SaveSlots::ResolveLiveSaveBlocker()));
}
#endif
