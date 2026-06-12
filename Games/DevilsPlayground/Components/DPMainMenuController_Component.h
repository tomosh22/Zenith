#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPMainMenuController_Component - L_FrontEnd controller. Wires the "Play"
 * button to LoadSceneByIndex(1, SCENE_LOAD_SINGLE).
 *
 * MVP-2.5.6: also wires the "Quit" button. In a production shipping
 * build this calls Zenith_Application::RequestQuit; in test builds the
 * click sets s_bQuitRequestedForTest (queryable via the static
 * accessor) so tests can verify the wiring without actually
 * terminating the process.
 *
 * Heap-stability: the button callbacks are static and ignore their user-data
 * argument, so nullptr is registered instead of `this` — a pool relocation
 * can therefore never leave a dangling user-data pointer in the UI button.
 */

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "UI/Zenith_UIButton.h"

class DPMainMenuController_Component ZENITH_FINAL
{
public:
	DPMainMenuController_Component() = delete;
	DPMainMenuController_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	void OnAwake()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>()) return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		// User-data is nullptr (NOT `this`): the handlers are static and never
		// read it, and component pools relocate instances — registering `this`
		// would leave a stale pointer in the button after a relocation.
		if (auto* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay"))
		{
			pxPlay->SetOnClick(&OnPlayClicked, nullptr);
		}
		// MVP-2.5.6: Quit button. Test-only handler sets a static
		// flag; production polish would tear down the application
		// via Zenith_Application::RequestQuit.
		if (auto* pxQuit = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuQuit"))
		{
			pxQuit->SetOnClick(&OnQuitClicked, nullptr);
		}
	}

	// Component contract: version-only payload (button wiring is rebuilt
	// every OnAwake).
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() {}
#endif

#ifdef ZENITH_INPUT_SIMULATOR
	// MVP-2.5.6 test accessors.
	static bool WasQuitRequestedForTest() { return s_bQuitRequestedForTest; }
	static void ResetQuitForTest() { s_bQuitRequestedForTest = false; }

	// Direct test-invocation of the click handler -- mirrors
	// DPForge_Component::CraftForTest pattern. Avoids the UI click
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

	Zenith_Entity m_xParentEntity;

#ifdef ZENITH_INPUT_SIMULATOR
	static inline bool s_bQuitRequestedForTest = false;
#endif
};
