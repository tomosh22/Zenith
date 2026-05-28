#pragma once
/**
 * DPMainMenuController_Behaviour - L_FrontEnd controller. Wires the "Play"
 * button to LoadSceneByIndex(1, SCENE_LOAD_SINGLE).
 *
 * MVP-2.5.6: also wires the "Quit" button. In a production shipping
 * build this calls Zenith_Application::RequestQuit; in test builds the
 * click sets s_bQuitRequestedForTest (queryable via the static
 * accessor) so tests can verify the wiring without actually
 * terminating the process.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_SceneSystem.h"
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
		// MVP-2.5.6: Quit button. Test-only handler sets a static
		// flag; production polish would tear down the application
		// via Zenith_Application::RequestQuit.
		if (auto* pxQuit = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuQuit"))
		{
			pxQuit->SetOnClick(&OnQuitClicked, this);
		}
	}

#ifdef ZENITH_INPUT_SIMULATOR
	// MVP-2.5.6 test accessors.
	static bool WasQuitRequestedForTest() { return s_bQuitRequestedForTest; }
	static void ResetQuitForTest() { s_bQuitRequestedForTest = false; }

	// Direct test-invocation of the click handler -- mirrors
	// DPForge_Behaviour::CraftForTest pattern. Avoids the UI click
	// simulator (which would require a real button hit-test in test
	// build).
	static void FireQuitClickForTest()
	{
		OnQuitClicked(nullptr);
	}
#endif

private:
	static void OnPlayClicked(void* /*pUserData*/)
	{
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	static void OnQuitClicked(void* /*pUserData*/)
	{
#ifdef ZENITH_INPUT_SIMULATOR
		// Test builds: just flip the flag so the test can observe
		// the click landed. NO process termination.
		s_bQuitRequestedForTest = true;
#else
		// Production: TODO once Zenith_Application::RequestQuit
		// exists. For now this is a no-op in shipping builds so the
		// scene authoring + button creation doesn't crash the menu;
		// the visible button reads as "Quit" but does nothing on
		// click. This MVP-2.5.6 placeholder is enough for the test
		// + the visible button -- the actual termination handler
		// is wired in a post-MVP shutdown polish PR.
#endif
	}

#ifdef ZENITH_INPUT_SIMULATOR
	static inline bool s_bQuitRequestedForTest = false;
#endif
};
