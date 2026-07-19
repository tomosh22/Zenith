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
#include "Zenithmon/Components/ZM_GameStateManager.h"   // IsWarpInProgress + TryGetUniqueActiveScenePlayerEntityID
#include "Zenithmon/Components/ZM_PlayerController.h"    // SetMovementEnabled (freeze seam)
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"         // ZM_FindSceneByBuildIndex / ZM_GetWorldSpec / ZM_SCENE_KIND
#include "Zenithmon/Source/ZM_InputActions.h"           // ReadMenuPressed / ReadConfirmPressed / ReadCancelPressed

#include <cstring>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// ZM_UI_MenuStack (S6 item 2 SC1). The overworld pause-menu machine on the
// persistent ZM_MenuRoot entity. Opens a focus-navigable ROOT menu, freezes the
// player, drives traversal via the engine focus-nav API, dispatches confirm by
// the focused element's NAME, pops on cancel/Escape. Mirrors ZM_BattleTransition
// (persistent singleton) + ZM_UI_BattleHUD (re-resolve elements by name, never
// cache; dispatch by name, never SetOnClick(this)).
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

	if (m_xStack.Top() == ZM_MENU_SCREEN_DIALOGUE)
	{
		// The dialogue owns input while it is the top screen: confirm advances the
		// typewriter / the line, and CANCEL is deliberately IGNORED -- a dialogue is
		// modal and closes only by being read to the end, so a prompt can never be
		// escaped past (S6 item 3 hangs yes/no prompts off exactly this).
		m_xDialogue.Tick(fDeltaSeconds);
		if (ZM_InputActions::ReadConfirmPressed())
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
	}
	else
	{
		// -- Menu open. Arrow-key traversal is driven automatically by the engine's
		//    per-canvas UpdateFocusNavigation (keyboard confirm / cancel are NOT wired
		//    engine-side, so they are game-supplied here). --
		const bool bConfirm = ZM_InputActions::ReadConfirmPressed();
		const bool bCancel  = ZM_InputActions::ReadCancelPressed();
		if (bConfirm)
		{
			HandleConfirm();
		}
		else if (bCancel)
		{
			HandleCancel();
		}
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
	// Drop any queued dialogue too: a force-close (ResetRuntimeStateForTests) must not
	// bleed a half-read conversation into the next batched test / play session.
	m_xDialogue.Reset();
	m_xDialogue.Hide(m_xParentEntity);

	// Hide every ROOT element + clear the canvas focus so arrow keys never drive an
	// invisible menu on the shared persistent canvas (watch-out 2).
	if (Zenith_UIComponent* pxUI = ResolveUI())
	{
		if (Zenith_UI::Zenith_UIElement* pxPanel = pxUI->FindElement(szROOT_PANEL_NAME))
		{
			pxPanel->SetVisible(false);
		}
		for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
		{
			if (Zenith_UI::Zenith_UIElement* pxItem =
				pxUI->FindElement(RootItemElementName(static_cast<ZM_MENU_ROOT_ITEM>(i))))
			{
				pxItem->SetVisible(false);
			}
		}
		pxUI->GetCanvas().SetFocusedElement(nullptr);
	}

	UnfreezePlayer();
}

void ZM_UI_MenuStack::HandleConfirm()
{
	// Only the ROOT screen is interactive in SC1 (the placeholder sub-screens have no
	// focusable elements, so their focused name is null -> NONE anyway).
	if (m_xStack.Top() != ZM_MENU_SCREEN_ROOT)
	{
		return;
	}

	// DISPATCH BY THE FOCUSED ELEMENT'S NAME -- never SetOnClick(this): a `this`
	// userdata dangles when the ECS pool relocates this component.
	const char* szFocusedName = nullptr;
	if (Zenith_UIComponent* pxUI = ResolveUI())
	{
		Zenith_UI::Zenith_UIElement* pxFocused = pxUI->GetCanvas().GetFocusedElement();
		if (pxFocused != nullptr)
		{
			szFocusedName = pxFocused->GetName().c_str();
		}
	}

	const ZM_MENU_ACTION eAction = ResolveRootAction(szFocusedName);
	if (eAction == ZM_MENU_ACTION_CLOSE)
	{
		CloseMenu();
		return;
	}
	const ZM_MENU_SCREEN eScreen = RootActionToScreen(eAction);
	if (eScreen != ZM_MENU_SCREEN_NONE)
	{
		m_xStack.Push(eScreen);   // placeholder screen (SC2+ present it); PresentTopScreen hides root
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

void ZM_UI_MenuStack::PresentTopScreen()
{
	Zenith_UIComponent* pxUI = ResolveUI();
	if (pxUI == nullptr)
	{
		return;   // best-effort: a missing UI component never crashes the menu
	}
	Zenith_UI::Zenith_UICanvas& xCanvas = pxUI->GetCanvas();
	const bool bRoot     = (m_xStack.Top() == ZM_MENU_SCREEN_ROOT);
	const bool bDialogue = (m_xStack.Top() == ZM_MENU_SCREEN_DIALOGUE);

	if (Zenith_UI::Zenith_UIElement* pxPanel = pxUI->FindElement(szROOT_PANEL_NAME))
	{
		pxPanel->SetVisible(bRoot);
	}
	for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
	{
		if (Zenith_UI::Zenith_UIElement* pxItem =
			pxUI->FindElement(RootItemElementName(static_cast<ZM_MENU_ROOT_ITEM>(i))))
		{
			pxItem->SetVisible(bRoot);
			pxItem->SetFocusable(bRoot);   // nav collects only visible + focusable elements
		}
	}

	if (bRoot)
	{
		// Ensure a focused ROOT entry (freshly opened, or returned from a sub-screen
		// where focus was cleared). Otherwise mirror the engine-navigated focus.
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
	}
	else
	{
		// Any non-ROOT screen (a placeholder, or DIALOGUE -- whose advance is a confirm
		// press, NOT focus-nav): clear focus so arrows never drive the hidden root
		// entries.
		xCanvas.SetFocusedElement(nullptr);
		m_iCursor = -1;
	}

	if (bDialogue)
	{
		m_xDialogue.Present(m_xParentEntity);
	}
	else
	{
		m_xDialogue.Hide(m_xParentEntity);
	}
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

	// Reset-first: never retain a stale open menu / queued dialogue from a reused instance.
	m_xStack.Clear();
	m_xDialogue.Reset();
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
}
#endif
