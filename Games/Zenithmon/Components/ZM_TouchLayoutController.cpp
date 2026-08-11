#include "Zenith.h"

#include "Zenithmon/Components/ZM_TouchLayoutController.h"

#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIVirtualButton.h"
#include "UI/Zenith_UIVirtualStick.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Source/ZM_Bindings.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace
{
	// The persistent singleton, exactly as ZM_UI_MenuStack / ZM_BattleTransition
	// track theirs: FrontEnd re-authors a ZM_TouchRoot on every scene-0 (re)load
	// and only the FIRST survives.
	Zenith_EntityID g_xTouchLayoutSingletonEntityID = INVALID_ENTITY_ID;

	// Push one control's target + visibility. A null action name means "not in
	// this context": the widget is hidden AND cleared, so nothing behind it can
	// fire. SetAction("") mid-gesture releases the old action and disarms, which
	// is the same B9 path a real retarget takes.
	template <typename WidgetType>
	void ApplyControl(Zenith_UIComponent& xUI, const char* szElementName, const char* szActionName)
	{
		WidgetType* pxWidget = xUI.FindElement<WidgetType>(szElementName);
		if (pxWidget == nullptr)
		{
			return;
		}
		pxWidget->SetAction(szActionName != nullptr ? szActionName : "");
		pxWidget->SetVisible(szActionName != nullptr);
	}
}

ZM_TouchLayoutController::ZM_TouchLayoutController(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

// ---- Pure decision surface --------------------------------------------------

ZM_TOUCH_CONTEXT ZM_TouchLayoutController::ResolveContext(bool bBattleActive, bool bMenuOpen,
	ZM_MENU_SCREEN eTopScreen)
{
	// A battle round trip owns the screen from the moment it is entered until the
	// overworld is resumed, INCLUDING the fades, so it is tested first: the
	// overworld stick must not be live over a battle that is fading in.
	if (bBattleActive)
	{
		return ZM_TOUCH_CONTEXT_BATTLE;
	}
	if (!bMenuOpen)
	{
		return ZM_TOUCH_CONTEXT_OVERWORLD;
	}
	if (eTopScreen == ZM_MENU_SCREEN_DIALOGUE)
	{
		return ZM_TOUCH_CONTEXT_DIALOGUE;
	}
	if (eTopScreen == ZM_MENU_SCREEN_TITLE)
	{
		return ZM_TOUCH_CONTEXT_TITLE;
	}
	return ZM_TOUCH_CONTEXT_MENU;
}

ZM_TouchLayout ZM_TouchLayoutController::LayoutForContext(ZM_TOUCH_CONTEXT eContext)
{
	ZM_TouchLayout xLayout;
	switch (eContext)
	{
	case ZM_TOUCH_CONTEXT_DIALOGUE:
	case ZM_TOUCH_CONTEXT_MENU:
	case ZM_TOUCH_CONTEXT_BATTLE:
		// One semantics per button per context: A confirms, B cancels, and the
		// stick is gone because there is nothing to walk towards.
		xLayout.m_szButtonAAction = ZM_Bindings::szACTION_CONFIRM;
		xLayout.m_szButtonBAction = ZM_Bindings::szACTION_CANCEL;
		return xLayout;

	case ZM_TOUCH_CONTEXT_TITLE:
		// The title screen is the base screen and cannot pop to nothing, so it
		// deliberately offers no cancel (ZM_UI_MenuStack ignores one there).
		xLayout.m_szButtonAAction = ZM_Bindings::szACTION_CONFIRM;
		return xLayout;

	case ZM_TOUCH_CONTEXT_OVERWORLD:
	default:
		xLayout.m_szStickAction     = ZM_Bindings::szACTION_MOVE;
		xLayout.m_szButtonAAction   = ZM_Bindings::szACTION_INTERACT;
		xLayout.m_szButtonBAction   = ZM_Bindings::szACTION_RUN;
		xLayout.m_szButtonMenuAction = ZM_Bindings::szACTION_MENU;
		return xLayout;
	}
}

const char* ZM_TouchLayoutController::ContextName(ZM_TOUCH_CONTEXT eContext)
{
	switch (eContext)
	{
	case ZM_TOUCH_CONTEXT_OVERWORLD: return "OVERWORLD";
	case ZM_TOUCH_CONTEXT_DIALOGUE:  return "DIALOGUE";
	case ZM_TOUCH_CONTEXT_MENU:      return "MENU";
	case ZM_TOUCH_CONTEXT_BATTLE:    return "BATTLE";
	case ZM_TOUCH_CONTEXT_TITLE:     return "TITLE";
	default:                         return "<unknown>";
	}
}

// ---- Lifecycle ---------------------------------------------------------------

void ZM_TouchLayoutController::OnStart()
{
	const Zenith_EntityID xOwnEntityID = m_xParentEntity.GetEntityID();
	if (g_xTouchLayoutSingletonEntityID == xOwnEntityID)
	{
		return;
	}

	Zenith_Entity xExisting = g_xEngine.Scenes().ResolveEntity(g_xTouchLayoutSingletonEntityID);
	if (xExisting.IsValid()
		&& xExisting.TryGetComponent<ZM_TouchLayoutController>() != nullptr)
	{
		m_xParentEntity.Destroy();
		return;
	}

	g_xTouchLayoutSingletonEntityID = xOwnEntityID;

	// ★ NO ApplyLayout HERE. OnStart also runs on the tools AUTHORING boot, and a
	// widget property written before AddStep_SaveScene serializes it is a
	// committed-scene churn defect waiting to happen. The first OnUpdate applies.
	m_eContext     = ZM_TOUCH_CONTEXT_OVERWORLD;
	m_bAppliedOnce = false;

	// The HUD outlives every scene load, exactly like the menu and fade roots --
	// which is also what makes the additive battle load a RETARGET rather than a
	// widget swap.
	m_xParentEntity.DontDestroyOnLoad();
}

void ZM_TouchLayoutController::OnDestroy()
{
	if (g_xTouchLayoutSingletonEntityID == m_xParentEntity.GetEntityID())
	{
		g_xTouchLayoutSingletonEntityID = INVALID_ENTITY_ID;
	}
}

void ZM_TouchLayoutController::OnUpdate(float fDeltaSeconds)
{
	(void)fDeltaSeconds;   // a context is state-driven; nothing here needs a rate

	// ONE resolve of the menu singleton for BOTH facts. Asking
	// ZM_UI_MenuStack::IsMenuOpen() and then re-resolving for the top screen
	// would read the two out of two separate lookups, and a menu that closed
	// between them would answer "open, on nothing".
	bool bMenuOpen = false;
	ZM_MENU_SCREEN eTopScreen = ZM_MENU_SCREEN_NONE;
	Zenith_EntityID xMenuEntityID = INVALID_ENTITY_ID;
	if (ZM_UI_MenuStack::TryGetUniqueSingletonEntityID(xMenuEntityID))
	{
		Zenith_Entity xMenuEntity = g_xEngine.Scenes().ResolveEntity(xMenuEntityID);
		const ZM_UI_MenuStack* pxMenu = xMenuEntity.IsValid()
			? xMenuEntity.TryGetComponent<ZM_UI_MenuStack>()
			: nullptr;
		if (pxMenu != nullptr)
		{
			bMenuOpen  = pxMenu->IsOpen();
			eTopScreen = pxMenu->GetTopScreen();
		}
	}

	const ZM_TOUCH_CONTEXT eContext = ResolveContext(
		ZM_BattleTransition::IsTransitionActive(), bMenuOpen, eTopScreen);

	// Only on a MOVE. Re-pushing the same action name every frame would be a
	// no-op inside SetAction (it early-returns on an unchanged name), but
	// re-pushing SetVisible every frame would fight anything else that legitimately
	// hides the HUD, and an unconditional write is how a controller quietly becomes
	// the owner of a property it does not own.
	if (m_bAppliedOnce && eContext == m_eContext)
	{
		return;
	}

	m_eContext = eContext;
	m_bAppliedOnce = true;
	ApplyLayout(eContext);
}

void ZM_TouchLayoutController::ApplyLayout(ZM_TOUCH_CONTEXT eContext)
{
	Zenith_UIComponent* pxUI = ResolveUI();
	if (pxUI == nullptr)
	{
		// No canvas yet (a headless boot that authored nothing, or a teardown
		// frame). Do not latch: re-resolve on the next context change.
		m_bAppliedOnce = false;
		return;
	}

	const ZM_TouchLayout xLayout = LayoutForContext(eContext);
	ApplyControl<Zenith_UI::Zenith_UIVirtualStick>(*pxUI, szSTICK_NAME, xLayout.m_szStickAction);
	ApplyControl<Zenith_UI::Zenith_UIVirtualButton>(*pxUI, szBUTTON_A_NAME, xLayout.m_szButtonAAction);
	ApplyControl<Zenith_UI::Zenith_UIVirtualButton>(*pxUI, szBUTTON_B_NAME, xLayout.m_szButtonBAction);
	ApplyControl<Zenith_UI::Zenith_UIVirtualButton>(*pxUI, szBUTTON_MENU_NAME, xLayout.m_szButtonMenuAction);
}

Zenith_UIComponent* ZM_TouchLayoutController::ResolveUI() const
{
	return m_xParentEntity.IsValid()
		? m_xParentEntity.TryGetComponent<Zenith_UIComponent>()
		: nullptr;
}

// ---- Serialization -----------------------------------------------------------
//
// Authoring data only: the live context is re-derived from the world on the very
// next OnUpdate, so storing it would only create a way for a stale one to load.

void ZM_TouchLayoutController::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
}

void ZM_TouchLayoutController::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;
	Zenith_Assert(uVersion <= uSERIALIZATION_VERSION,
		"ZM_TouchLayoutController stream version %u is newer than %u", uVersion, uSERIALIZATION_VERSION);
	m_eContext     = ZM_TOUCH_CONTEXT_OVERWORLD;
	m_bAppliedOnce = false;
}

#ifdef ZENITH_TOOLS
void ZM_TouchLayoutController::RenderPropertiesPanel()
{
	ImGui::Text("Context: %s", ContextName(m_eContext));
	ImGui::Text("Applied: %s", m_bAppliedOnce ? "yes" : "not yet");
	ImGui::TextWrapped("Retargets the four on-screen controls (B9) to the actions the "
		"current UI state needs. Widgets are authored in FrontEnd and never "
		"re-created; a context change is SetAction + SetVisible.");
}
#endif

// ---- Singleton observation ---------------------------------------------------

bool ZM_TouchLayoutController::TryGetUniqueSingletonEntityID(Zenith_EntityID& xEntityIDOut)
{
	xEntityIDOut = INVALID_ENTITY_ID;
	if (g_xTouchLayoutSingletonEntityID == INVALID_ENTITY_ID)
	{
		return false;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(g_xTouchLayoutSingletonEntityID);
	if (!xEntity.IsValid() || xEntity.TryGetComponent<ZM_TouchLayoutController>() == nullptr)
	{
		return false;
	}
	xEntityIDOut = g_xTouchLayoutSingletonEntityID;
	return true;
}

ZM_TOUCH_CONTEXT ZM_TouchLayoutController::GetLiveContext()
{
	Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xEntityID))
	{
		return ZM_TOUCH_CONTEXT_OVERWORLD;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	ZM_TouchLayoutController* pxController = xEntity.IsValid()
		? xEntity.TryGetComponent<ZM_TouchLayoutController>()
		: nullptr;
	return pxController != nullptr ? pxController->GetContext() : ZM_TOUCH_CONTEXT_OVERWORLD;
}

void ZM_TouchLayoutController::ResetRuntimeStateForTests()
{
	Zenith_EntityID xEntityID = INVALID_ENTITY_ID;
	if (!TryGetUniqueSingletonEntityID(xEntityID))
	{
		return;
	}
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	ZM_TouchLayoutController* pxController = xEntity.IsValid()
		? xEntity.TryGetComponent<ZM_TouchLayoutController>()
		: nullptr;
	if (pxController == nullptr)
	{
		return;
	}
	// Drop the latch AND push the resting layout back, so a test that ended in
	// DIALOGUE cannot hand the next one an A button still pointing at Confirm
	// (the widgets are DontDestroyOnLoad and outlive every test boundary).
	pxController->m_eContext     = ZM_TOUCH_CONTEXT_OVERWORLD;
	pxController->m_bAppliedOnce = false;
	pxController->ApplyLayout(ZM_TOUCH_CONTEXT_OVERWORLD);
}
