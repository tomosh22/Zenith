#pragma once
/**
 * DPPauseMenuController_Behaviour - Esc-toggleable pause overlay.
 *
 * Skeleton-grade: toggles a UI text element named "PauseOverlay" between
 * visible/hidden. Wave-4 polish wires SetScenePaused for true pause.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_KeyCodes.h"
#include "UI/Zenith_UIText.h"

class DPPauseMenuController_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPPauseMenuController_Behaviour)

	DPPauseMenuController_Behaviour() = delete;
	DPPauseMenuController_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnUpdate(const float /*fDt*/) ZENITH_FINAL override
	{
		if (!Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE)) return;
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>()) return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		auto* pxOverlay = xUI.FindElement<Zenith_UI::Zenith_UIText>("PauseOverlay");
		if (pxOverlay == nullptr) return;
		m_bShown = !m_bShown;
		pxOverlay->SetVisible(m_bShown);
	}

private:
	bool m_bShown = false;
};
