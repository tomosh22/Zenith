#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPPauseMenuController_Component - Esc-toggleable pause overlay + real pause.
 *
 * MVP-1.1 (post round-1):
 *   On Esc the controller toggles a UI text element named "PauseOverlay"
 *   AND calls `g_xEngine.Scenes().SetScenePaused` on the gameplay scene
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
 *
 * Heap-stability (CRITICAL): DontDestroyOnLoad triggers a cross-scene
 * component transfer that MOVE-CONSTRUCTS this component into the persistent
 * scene's pool (MoveEmplaceBack) — `this` relocates mid-OnStart. Therefore:
 *   (a) DontDestroyOnLoad MUST be the final statement of OnStart (nothing
 *       may touch members afterwards), and
 *   (b) the hand-written move operations re-point s_pxPersistentInstance and
 *       re-subscribe the this-capturing Victory/RunLost handlers at the new
 *       address.
 */

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "DataStream/Zenith_DataStream.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_KeyCodes.h"
#include "UI/Zenith_UIText.h"

#include "Source/PublicInterfaces.h"

class DPPauseMenuController_Component ZENITH_FINAL
{
public:
	DPPauseMenuController_Component() = delete;
	DPPauseMenuController_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	// Heap-stability: hand-written moves (see header comment); copies deleted.
	DPPauseMenuController_Component(const DPPauseMenuController_Component&) = delete;
	DPPauseMenuController_Component& operator=(const DPPauseMenuController_Component&) = delete;

	DPPauseMenuController_Component(DPPauseMenuController_Component&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_bShown(xOther.m_bShown)
		, m_bRunOver(xOther.m_bRunOver)
		, m_xGameplayScene(xOther.m_xGameplayScene)
	{
		if (s_pxPersistentInstance == &xOther) s_pxPersistentInstance = this;
		TakeOverSubscriptionsFrom(xOther);
	}

	DPPauseMenuController_Component& operator=(DPPauseMenuController_Component&& xOther) noexcept
	{
		if (this != &xOther)
		{
			UnsubscribeRunOverEvents();
			m_xParentEntity  = xOther.m_xParentEntity;
			m_bShown         = xOther.m_bShown;
			m_bRunOver       = xOther.m_bRunOver;
			m_xGameplayScene = xOther.m_xGameplayScene;
			if (s_pxPersistentInstance == &xOther) s_pxPersistentInstance = this;
			TakeOverSubscriptionsFrom(xOther);
		}
		return *this;
	}

	~DPPauseMenuController_Component()
	{
		// Belt-and-braces: OnDisable/OnDestroy already unsubscribe + clear
		// the singleton; this covers teardown paths that skip the hooks.
		// Moved-from sources hold INVALID handles and a repointed singleton,
		// so this is a no-op for them.
		UnsubscribeRunOverEvents();
		if (s_pxPersistentInstance == this)
		{
			s_pxPersistentInstance = nullptr;
		}
	}

	void OnStart()
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

		// First-time wiring: capture the gameplay scene, subscribe, claim the
		// singleton, THEN migrate to the persistent scene so we keep ticking
		// while the gameplay scene is paused.
		m_xGameplayScene = m_xParentEntity.GetScene();

		// MVP-4.3.2: mirror DPHUDController's run-over flag so the R/Q
		// shortcuts work even when the player hasn't opened the pause
		// menu first. The HUD owns the user-facing prompt ("Press R to
		// restart"); this controller owns the input handler. Both
		// subscribe to the same two events.
		SubscribeRunOverEvents();

		s_pxPersistentInstance = this;

		// DontDestroyOnLoad MUST come last: the cross-scene move it triggers
		// move-constructs this component into the persistent scene's pool,
		// relocating `this`. The move constructor re-points
		// s_pxPersistentInstance and re-subscribes the handlers at the new
		// address; nothing here may touch members after this call.
		m_xParentEntity.DontDestroyOnLoad();
	}

	void OnDisable()
	{
		// Unsubscribe before the destroy wave reaches us. The lambda captures
		// `this`; leaving the subscription live across the disable->destroy
		// window risks the dispatcher invoking it after destruction. OnDestroy
		// repeats the unsubscribe as a safety net (the handle zeroing makes
		// the second call a no-op).
		UnsubscribeRunOverEvents();
	}

	void OnDestroy()
	{
		UnsubscribeRunOverEvents();
		if (s_pxPersistentInstance == this)
		{
			s_pxPersistentInstance = nullptr;
		}
	}

	// Component contract: version-only payload (pause state is per-session).
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

	void OnUpdate(const float /*fDt*/)
	{
		const bool bEsc = g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE);

		// MVP-2.5.5: while paused, R restarts the run (reload
		// gameplay scene) and Q quits to the main menu.
		// MVP-4.3.2: same shortcuts also honoured when the run is
		// over (m_bRunOver set by Victory / RunLost subscribers).
		// The player sees the permanent banner + "Press R to restart"
		// HUD prompt and can press the key without first opening the
		// pause menu.
		if (m_bShown || m_bRunOver)
		{
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_R))
			{
				HandleRestart();
				return;
			}
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_Q))
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
			g_xEngine.Scenes().SetScenePaused(m_xGameplayScene, m_bShown);
		}

		// Telemetry-v3: surface the toggle transition so the visualiser
		// can render a "paused frames" band on the timeline. Fires on
		// every Esc-press (both enter-pause and leave-pause).
		Zenith_EventDispatcher::Get().Dispatch(DP_OnPauseToggle{ m_bShown });
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
	static DPPauseMenuController_Component* GetPersistentInstanceForTest()
	{
		return s_pxPersistentInstance;
	}
#endif

private:
	// Shared post-pause-menu reset. Clears every per-run DP state
	// owner so the next scene-load starts from a fresh demon run.
	// Called by both HandleRestart (re-load gameplay scene) and HandleQuit
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
		// Reload gameplay scene (build index 1 = ProcLevel).
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	void HandleQuit()
	{
#ifdef ZENITH_INPUT_SIMULATOR
		s_bQuitToMenuRequestedForTest = true;
#endif
		ResetAllRunStateBeforeReload();
		// Load front-end (build index 0).
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	// The single factored subscription site. Called from OnStart (first
	// instance only) and from the move operations so the relocated instance
	// re-captures the new `this`.
	void SubscribeRunOverEvents()
	{
		m_xVictoryHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVictory>(
			[this](const DP_OnVictory&)
			{
				m_bRunOver = true;
			});
		m_xRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(
			[this](const DP_OnRunLost&)
			{
				m_bRunOver = true;
			});
	}

	void UnsubscribeRunOverEvents()
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
	}

	// Move-helper: drop the source's this-capturing subscriptions and stand
	// up fresh ones against the new address iff the source was subscribed.
	void TakeOverSubscriptionsFrom(DPPauseMenuController_Component& xOther)
	{
		const bool bWasSubscribed =
			xOther.m_xVictoryHandle != INVALID_EVENT_HANDLE ||
			xOther.m_xRunLostHandle != INVALID_EVENT_HANDLE;
		xOther.UnsubscribeRunOverEvents();
		if (bWasSubscribed)
		{
			SubscribeRunOverEvents();
		}
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
			g_xEngine.Scenes().SetScenePaused(m_xGameplayScene, false);
		}
		m_bShown = false;
	}

	Zenith_Entity      m_xParentEntity;

	bool               m_bShown = false;
	// MVP-4.3.2: set by Victory / RunLost subscribers in OnStart. Lets the
	// R/Q shortcuts fire when the run is over without first opening the
	// pause menu (the HUD's RestartPrompt element prompts the player).
	bool               m_bRunOver = false;
	Zenith_Scene       m_xGameplayScene;
	Zenith_EventHandle m_xVictoryHandle = INVALID_EVENT_HANDLE;
	Zenith_EventHandle m_xRunLostHandle = INVALID_EVENT_HANDLE;

	static inline DPPauseMenuController_Component* s_pxPersistentInstance = nullptr;
#ifdef ZENITH_INPUT_SIMULATOR
	static inline bool s_bRestartRequestedForTest = false;
	static inline bool s_bQuitToMenuRequestedForTest = false;
#endif
};
