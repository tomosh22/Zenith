#pragma once
/**
 * DPMainMenuController_Behaviour - L_FrontEnd controller. Wires the "Play"
 * button to LoadSceneByIndex(1, SCENE_LOAD_SINGLE).
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "UI/Zenith_UIButton.h"

class DPMainMenuController_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPMainMenuController_Behaviour)

	DPMainMenuController_Behaviour() = delete;
	DPMainMenuController_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnAwake() ZENITH_FINAL override
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>()) return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		if (auto* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay"))
		{
			pxPlay->SetOnClick(&OnPlayClicked, this);
		}
	}

private:
	static void OnPlayClicked(void* /*pUserData*/)
	{
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}
};
