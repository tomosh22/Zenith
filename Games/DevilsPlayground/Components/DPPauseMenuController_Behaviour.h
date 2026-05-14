#pragma once
/**
 * DPPauseMenuController_Behaviour - Esc-toggleable pause overlay + real pause.
 *
 * MVP-1.1 (post round-1):
 *   On Esc the controller toggles a UI text element named "PauseOverlay"
 *   AND calls `Zenith_SceneManager::SetScenePaused` on the gameplay scene
 *   so every entity OnUpdate (DPVillager life timer, Priest BT, ...) halts.
 *
 *   To keep this very controller running while the gameplay scene is paused
 *   the entity is moved to the persistent scene during OnStart. The UI
 *   component travels with it so the overlay still renders.
 *
 *   We capture the original (gameplay) scene handle just before marking
 *   ourselves persistent so we can pause/unpause exactly that scene -- not
 *   the persistent scene we now live in.
 *
 * Singleton pattern:
 *   Subsequent gameplay scenes (e.g. quitting back to FrontEnd then starting
 *   a new run) re-author a PauseManager entity. Without a guard we'd end up
 *   with stale persistent instances accumulating across scene loads. The
 *   static side-table `s_pxPersistentInstance` tracks the live persistent
 *   controller. New PauseManager entities OnStart detect an existing
 *   singleton, hand it the current scene handle, and let themselves be
 *   destroyed with the scene rather than migrating.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Scene.h"
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

	void OnStart() ZENITH_FINAL override
	{
		// If a persistent singleton already exists (subsequent scene load),
		// just hand it the new gameplay scene handle + force-unpause it
		// (the new scene starts unpaused) and let THIS instance live in the
		// gameplay scene -- it'll be destroyed when the scene unloads.
		if (s_pxPersistentInstance != nullptr && s_pxPersistentInstance != this)
		{
			s_pxPersistentInstance->m_xGameplayScene = m_xParentEntity.GetScene();
			s_pxPersistentInstance->ResetVisibleAndUnpause();
			return;
		}

		// First-time wiring: capture the gameplay scene then migrate to the
		// persistent scene so we keep ticking while the gameplay scene is paused.
		m_xGameplayScene = m_xParentEntity.GetScene();
		Zenith_SceneManager::MarkEntityPersistent(m_xParentEntity);
		s_pxPersistentInstance = this;
	}

	void OnDestroy()
	{
		if (s_pxPersistentInstance == this)
		{
			s_pxPersistentInstance = nullptr;
		}
	}

	void OnUpdate(const float /*fDt*/) ZENITH_FINAL override
	{
		const bool bEsc = Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE);

		// MVP-2.5.5: while paused, R restarts the run (reload
		// gameplay scene) and Q quits to the main menu. Both are
		// only honoured while shown=true so they don't fire during
		// normal play.
		if (m_bShown)
		{
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R))
			{
				HandleRestart();
				return;
			}
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_Q))
			{
				HandleQuit();
				return;
			}
		}

		if (!bEsc) return;
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>()) return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		auto* pxOverlay = xUI.FindElement<Zenith_UI::Zenith_UIText>("PauseOverlay");
		if (pxOverlay == nullptr) return;

		m_bShown = !m_bShown;
		pxOverlay->SetVisible(m_bShown);

		// MVP-1.1.2: drive the real pause. If the gameplay scene was unloaded
		// for some reason (handle no longer valid), we still toggle the
		// overlay but skip the pause call -- nothing to pause.
		if (m_xGameplayScene.IsValid())
		{
			Zenith_SceneManager::SetScenePaused(m_xGameplayScene, m_bShown);
		}
	}

#ifdef ZENITH_INPUT_SIMULATOR
	// Test-only accessors. Not part of the shipping API.
	bool IsPausedForTest() const { return m_bShown; }
	Zenith_Scene GetGameplaySceneForTest() const { return m_xGameplayScene; }

	// MVP-2.5.5 test accessors. Both are flags flipped by
	// HandleRestart / HandleQuit so tests can verify the
	// shortcut wiring without actually reloading scenes or
	// quitting the process.
	static bool WasRestartRequestedForTest() { return s_bRestartRequestedForTest; }
	static bool WasQuitToMenuRequestedForTest() { return s_bQuitToMenuRequestedForTest; }
	static void ResetPauseShortcutsForTest()
	{
		s_bRestartRequestedForTest = false;
		s_bQuitToMenuRequestedForTest = false;
	}
	static void FireRestartShortcutForTest()
	{
		if (s_pxPersistentInstance != nullptr) s_pxPersistentInstance->HandleRestart();
	}
	static void FireQuitToMenuShortcutForTest()
	{
		if (s_pxPersistentInstance != nullptr) s_pxPersistentInstance->HandleQuit();
	}

	// Allow the test harness to reset the persistent singleton between
	// batched tests so we don't carry pause state across them. The
	// matching call lives in DevilsPlayground.cpp's between-tests hook.
	static void ResetForTest()
	{
		if (s_pxPersistentInstance != nullptr)
		{
			s_pxPersistentInstance->ResetVisibleAndUnpause();
			s_pxPersistentInstance->m_xGameplayScene = Zenith_Scene();
		}
		s_bRestartRequestedForTest = false;
		s_bQuitToMenuRequestedForTest = false;
	}
	static DPPauseMenuController_Behaviour* GetPersistentInstanceForTest()
	{
		return s_pxPersistentInstance;
	}
#endif

private:
	void HandleRestart()
	{
#ifdef ZENITH_INPUT_SIMULATOR
		s_bRestartRequestedForTest = true;
#endif
		// Unpause + hide overlay before reloading so the new scene
		// boots with a clean pause state.
		ResetVisibleAndUnpause();
		// Reload gameplay scene (build index 1 = GameLevel).
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	void HandleQuit()
	{
#ifdef ZENITH_INPUT_SIMULATOR
		s_bQuitToMenuRequestedForTest = true;
#endif
		ResetVisibleAndUnpause();
		// Load front-end (build index 0).
		Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	void ResetVisibleAndUnpause()
	{
		// Force overlay off + unpause the (possibly stale) captured scene.
		// Used both when a new gameplay scene takes over and from the test
		// harness's between-tests hook.
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
			auto* pxOverlay = xUI.FindElement<Zenith_UI::Zenith_UIText>("PauseOverlay");
			if (pxOverlay != nullptr) pxOverlay->SetVisible(false);
		}
		if (m_xGameplayScene.IsValid())
		{
			Zenith_SceneManager::SetScenePaused(m_xGameplayScene, false);
		}
		m_bShown = false;
	}

	bool         m_bShown = false;
	Zenith_Scene m_xGameplayScene;

	static inline DPPauseMenuController_Behaviour* s_pxPersistentInstance = nullptr;
#ifdef ZENITH_INPUT_SIMULATOR
	static inline bool s_bRestartRequestedForTest = false;
	static inline bool s_bQuitToMenuRequestedForTest = false;
#endif
};
