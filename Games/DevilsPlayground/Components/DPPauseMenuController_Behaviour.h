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
#include "EntityComponent/Zenith_EventSystem.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_KeyCodes.h"
#include "UI/Zenith_UIText.h"

#include "Source/PublicInterfaces.h"

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
			s_pxPersistentInstance->m_bRunOver = false;
			return;
		}

		// First-time wiring: capture the gameplay scene then migrate to the
		// persistent scene so we keep ticking while the gameplay scene is paused.
		m_xGameplayScene = m_xParentEntity.GetScene();
		Zenith_SceneManager::MarkEntityPersistent(m_xParentEntity);
		s_pxPersistentInstance = this;

		// MVP-4.3.2: mirror DPHUDController's run-over flag so the R/Q
		// shortcuts work even when the player hasn't opened the pause
		// menu first. The HUD owns the user-facing prompt ("Press R to
		// restart"); this controller owns the input handler. Both
		// subscribe to the same two events.
		m_xVictoryHandle = Zenith_EventDispatcher::Get().SubscribeLambda<DP_OnVictory>(
			[this](const DP_OnVictory&)
			{
				m_bRunOver = true;
			});
		m_xRunLostHandle = Zenith_EventDispatcher::Get().SubscribeLambda<DP_OnRunLost>(
			[this](const DP_OnRunLost&)
			{
				m_bRunOver = true;
			});
	}

	void OnDestroy()
	{
		if (m_xVictoryHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(m_xVictoryHandle);
			m_xVictoryHandle = INVALID_EVENT_HANDLE;
		}
		if (m_xRunLostHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(m_xRunLostHandle);
			m_xRunLostHandle = INVALID_EVENT_HANDLE;
		}
		if (s_pxPersistentInstance == this)
		{
			s_pxPersistentInstance = nullptr;
		}
	}

	void OnUpdate(const float /*fDt*/) ZENITH_FINAL override
	{
		const bool bEsc = Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE);

		// MVP-2.5.5: while paused, R restarts the run (reload
		// gameplay scene) and Q quits to the main menu.
		// MVP-4.3.2: same shortcuts also honoured when the run is
		// over (m_bRunOver set by Victory / RunLost subscribers).
		// The player sees the permanent banner + "Press R to restart"
		// HUD prompt and can press the key without first opening the
		// pause menu.
		if (m_bShown || m_bRunOver)
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
			s_pxPersistentInstance->m_bRunOver = false;
		}
		s_bRestartRequestedForTest = false;
		s_bQuitToMenuRequestedForTest = false;
	}
	// MVP-4.3.2 test accessor: did either Victory or RunLost handler
	// set the run-over flag in the pause controller? Used by tests
	// that verify R/Q work without first opening the pause menu.
	static bool IsRunOverForTest()
	{
		return s_pxPersistentInstance != nullptr && s_pxPersistentInstance->m_bRunOver;
	}
	static DPPauseMenuController_Behaviour* GetPersistentInstanceForTest()
	{
		return s_pxPersistentInstance;
	}
#endif

private:
	// Shared post-pause-menu reset. Clears every per-run DP state
	// owner so the next scene-load starts from a fresh demon run.
	// Called by both HandleRestart (re-load GameLevel) and HandleQuit
	// (load FrontEnd) -- both intents leave no state-leak across the
	// transition.
	void ResetAllRunStateBeforeReload()
	{
		ResetVisibleAndUnpause();
		DP_Player::ResetForNewRun();    // Clears possessed handle,
		                                // held items, cooldown, scent,
		                                // anchor, channel.
		DP_Win::Reset();
		DP_Fog::ClearAllFogHoles();
		DP_Fog::ClearAllMemoryReveals();
		DP_Night::Reset();
	}

	void HandleRestart()
	{
#ifdef ZENITH_INPUT_SIMULATOR
		s_bRestartRequestedForTest = true;
#endif
		ResetAllRunStateBeforeReload();
		// Reload gameplay scene (build index 1 = GameLevel).
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	void HandleQuit()
	{
#ifdef ZENITH_INPUT_SIMULATOR
		s_bQuitToMenuRequestedForTest = true;
#endif
		ResetAllRunStateBeforeReload();
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

	bool               m_bShown = false;
	// MVP-4.3.2: set by Victory / RunLost subscribers in OnStart. Lets the
	// R/Q shortcuts fire when the run is over without first opening the
	// pause menu (the HUD's RestartPrompt element prompts the player).
	bool               m_bRunOver = false;
	Zenith_Scene       m_xGameplayScene;
	Zenith_EventHandle m_xVictoryHandle = INVALID_EVENT_HANDLE;
	Zenith_EventHandle m_xRunLostHandle = INVALID_EVENT_HANDLE;

	static inline DPPauseMenuController_Behaviour* s_pxPersistentInstance = nullptr;
#ifdef ZENITH_INPUT_SIMULATOR
	static inline bool s_bRestartRequestedForTest = false;
	static inline bool s_bQuitToMenuRequestedForTest = false;
#endif
};
