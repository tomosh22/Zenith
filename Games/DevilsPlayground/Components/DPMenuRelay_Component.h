#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPMenuRelay_Component - the graph-era front-end menu shim. Owns ONLY the
 * UI-button wiring (the same OnAwake wiring DPMainMenuController_Component
 * had, with the same heap-stability rules: static handlers, nullptr
 * user-data); what the buttons DO lives in the entity's Behaviour Graph,
 * reached via "MenuPlay" / "MenuQuit" custom events.
 */

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "UI/Zenith_UIButton.h"
#include "DataStream/Zenith_DataStream.h"

class DPMenuRelay_Component ZENITH_FINAL
{
public:
	DPMenuRelay_Component() = delete;
	DPMenuRelay_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	void OnAwake()
	{
		// Per-scene singleton entity id: the static button handlers resolve the
		// live entity through it (components relocate; EntityIDs don't).
		s_xMenuEntityID = m_xParentEntity.GetEntityID();
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>()) return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		if (auto* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay"))
		{
			pxPlay->SetOnClick(&OnPlayClicked, nullptr);
		}
		if (auto* pxQuit = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuQuit"))
		{
			pxQuit->SetOnClick(&OnQuitClicked, nullptr);
		}
	}

	void OnDestroy()
	{
		if (s_xMenuEntityID == m_xParentEntity.GetEntityID())
		{
			s_xMenuEntityID = Zenith_EntityID();
		}
	}

	// Component contract: version-only payload (button wiring is rebuilt every
	// OnAwake), matching the retired DPMainMenuController.
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
	// Test parity with the retired DPMainMenuController: drive the quit click
	// through the same handler the UI button invokes.
	static void FireQuitClickForTest()
	{
		OnQuitClicked(nullptr);
	}
#endif

private:
	static void OnPlayClicked(void* /*pUserData*/) { FireMenuEvent("MenuPlay"); }
	static void OnQuitClicked(void* /*pUserData*/) { FireMenuEvent("MenuQuit"); }

	static void FireMenuEvent(const char* szName)
	{
		if (!s_xMenuEntityID.IsValid()) return;
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(s_xMenuEntityID);
		if (pxScene == nullptr) return;
		Zenith_Entity xEntity = pxScene->TryGetEntity(s_xMenuEntityID);
		if (!xEntity.IsValid() || !xEntity.HasComponent<Zenith_GraphComponent>()) return;
		xEntity.GetComponent<Zenith_GraphComponent>().FireCustomEvent(szName);
	}

	Zenith_Entity m_xParentEntity;

	static inline Zenith_EntityID s_xMenuEntityID;
};
