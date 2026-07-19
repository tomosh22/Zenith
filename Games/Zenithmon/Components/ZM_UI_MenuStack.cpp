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
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"       // ZM_SPECIES_COUNT (the dex size the DEX screen pages over)
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"         // ZM_FindSceneByBuildIndex / ZM_GetWorldSpec / ZM_SCENE_KIND
#include "Zenithmon/Source/ZM_InputActions.h"           // ReadMenuPressed / ReadConfirmPressed / ReadCancelPressed

#include <cstring>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// ZM_UI_MenuStack (S6 item 2 SC1 + SC2 + SC4 + SC5 + SC6 + SC7). The overworld pause-menu machine on
// the persistent ZM_MenuRoot entity. Opens a focus-navigable ROOT menu, freezes the
// player, drives traversal via the engine focus-nav API, dispatches confirm by
// the focused element's NAME, pops on cancel/Escape. Mirrors ZM_BattleTransition
// (persistent singleton) + ZM_UI_BattleHUD (re-resolve elements by name, never
// cache; dispatch by name, never SetOnClick(this)).
//
// Screen dispatch lives in exactly TWO per-screen switches -- the input routing in
// OnUpdate and the show/hide + focus policy in PresentTopScreen. A new screen adds
// an arm to each and a by-value presenter member; nothing else moves. SC7 (Shop) is
// the fourth screen added that way, and it reshaped neither site.
// ============================================================================

Zenith_EntityID ZM_UI_MenuStack::s_xSingletonEntityID = INVALID_ENTITY_ID;

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

ZM_MENU_SCREEN ZM_MenuScreenStack::Top() const
{
	return (m_uDepth == 0u) ? ZM_MENU_SCREEN_NONE : m_aeScreens[m_uDepth - 1u];
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
	if (!IsOpen())
	{
		// Overworld pause-open gate: menu key, in an overworld scene, no warp / battle.
		if (ShouldOpenMenu(
			ZM_InputActions::ReadMenuPressed(),
			/* bAlreadyOpen */ false,
			IsActiveSceneOverworld(),
			ZM_GameStateManager::IsWarpInProgress(),
			ZM_BattleTransition::IsTransitionActive()))
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
	case ZM_MENU_SCREEN_DIALOGUE:
		// The dialogue owns input while it is the top screen: confirm advances the
		// typewriter / the line, and CANCEL is deliberately IGNORED -- a dialogue is
		// modal and closes only by being read to the end, so a prompt can never be
		// escaped past (S6 item 3 hangs yes/no prompts off exactly this).
		m_xDialogue.Tick(fDeltaSeconds);
		if (bConfirm)
		{
			if (m_xDialogue.Confirm() == ZM_DIALOGUE_ADVANCE_CLOSED)
			{
				m_xStack.Pop();
				if (m_xStack.IsEmpty())
				{
					CloseMenu();
				}
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
			HandleCancel();
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
			HandleCancel();
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
			HandleCancel();
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
				HandleCancel();
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
			HandleCancel();
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
			HandleCancel();
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
	const ZM_MENU_SCREEN eScreen = RootActionToScreen(eAction);
	if (eScreen != ZM_MENU_SCREEN_NONE)
	{
		m_xStack.Push(eScreen);   // PresentTopScreen raises it and hides ROOT this frame
	}
}

void ZM_UI_MenuStack::HandleCancel()
{
	m_xStack.Pop();
	if (m_xStack.IsEmpty())
	{
		CloseMenu();
	}
	// else: back to a lower screen (ROOT) -- PresentTopScreen re-shows it this frame.
}

// ---- Dialogue (SC2) --------------------------------------------------------

bool ZM_UI_MenuStack::PushDialogueLines(const char* const* paszLines, u_int uCount)
{
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

	// ---- Focus policy. A FOCUS-NAVIGABLE screen owns the canvas focus and mirrors it
	//      into m_iCursor; every other screen clears both, so arrows can never drive a
	//      hidden screen's entries (watch-out 2). ----
	switch (eTop)
	{
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

	// DIALOGUE advances on a confirm press, NOT focus-nav, so it clears the focus.
	case ZM_MENU_SCREEN_DIALOGUE:
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
		pxPanel->SetVisible(bShown);
	}
	for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
	{
		if (Zenith_UI::Zenith_UIElement* pxItem =
			xUI.FindElement(RootItemElementName(static_cast<ZM_MENU_ROOT_ITEM>(i))))
		{
			pxItem->SetVisible(bShown);
			pxItem->SetFocusable(bShown);   // nav collects only visible + focusable elements
		}
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
	}
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
	if (std::strcmp(szFocusedElementName, szROOT_PARTY_NAME) == 0) { return ZM_MENU_ACTION_OPEN_PARTY; }
	if (std::strcmp(szFocusedElementName, szROOT_BAG_NAME)   == 0) { return ZM_MENU_ACTION_OPEN_BAG;   }
	if (std::strcmp(szFocusedElementName, szROOT_DEX_NAME)   == 0) { return ZM_MENU_ACTION_OPEN_DEX;   }
	if (std::strcmp(szFocusedElementName, szROOT_EXIT_NAME)  == 0) { return ZM_MENU_ACTION_CLOSE;      }
	return ZM_MENU_ACTION_NONE;
}

const char* ZM_UI_MenuStack::RootItemElementName(ZM_MENU_ROOT_ITEM eItem)
{
	switch (eItem)
	{
	case ZM_MENU_ROOT_PARTY: return szROOT_PARTY_NAME;
	case ZM_MENU_ROOT_BAG:   return szROOT_BAG_NAME;
	case ZM_MENU_ROOT_DEX:   return szROOT_DEX_NAME;
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
	default:                        return ZM_MENU_SCREEN_NONE;   // CLOSE / NONE push nothing
	}
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
}
#endif
