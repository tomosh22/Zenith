#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIText.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPHUDController_Component.h"
#include "Components/DPVillager_Component.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P4Playthrough_LossByDawn (MVP-4.2 -- Dawn playthrough)
//
// Test_P1Dawn_DispatchesRunLost pins the night timer + dispatch
// mechanism. This test pins the FULL playthrough: the dispatch
// reaches DPHUDController and the HUD writes "DAWN BREAKS".
//
// Procedure:
//   1. Load GameLevel + subscribe to DP_OnRunLost.
//   2. Wait for HUD controller to be authored.
//   3. Reset HUD run-lost flag (clear any leak from previous batched test).
//   4. Call DP_Night::StartNight(0.5) -- short night that expires in ~30
//      frames at 60 Hz. DPPlayerController::OnUpdate calls TickNight each
//      frame; the cross-zero detection fires DP_OnRunLost{Dawn} once.
//   5. Tick ~90 frames so the dispatch has time to land AND the HUD's
//      subscriber lambda runs.
//   6. Verify:
//        DP_OnRunLost fired with cause = Dawn
//        HUD's DidRunLostHandlerFireForTest() returns true
//        HUD's LastRunLostCauseForTest() returns Dawn
//        HUD "Status" UI element text == "DAWN BREAKS"
//
// What this catches (vs Phase 1 unit test):
//   * HUD's BuildRunLostText returned wrong string for Dawn cause.
//   * HUD's subscriber switch-case missed the Dawn variant.
//   * Status element was hidden by another writer between the dispatch
//     and our snapshot (e.g., death timer fired during the test wait).
// ============================================================================

namespace
{
	enum Phase : int { kLD_Start, kLD_WaitScene, kLD_ArmTimer,
	                   kLD_TickWindow, kLD_Snapshot, kLD_Verify, kLD_Done };

	int                     g_iPhase = kLD_Start;
	int                     g_iTickFrames = 0;
	Zenith_EventHandle      g_xRunLostHandle = INVALID_EVENT_HANDLE;
	int                     g_iRunLostDawnCount = 0;
	bool                    g_bHudFired = false;
	DP_RunLostCause         g_eHudCause = DP_RunLostCause::Apprehended;
	char                    g_szHudStatusText[128] = "<unset>";
	bool                    g_bHudStatusVisible = false;

	constexpr int kTICK_WINDOW_FRAMES = 90;
	constexpr float kNIGHT_DURATION_S = 0.5f;

	void OnRunLost(const DP_OnRunLost& xEvt)
	{
		if (xEvt.m_eCause == DP_RunLostCause::Dawn)
		{
			++g_iRunLostDawnCount;
		}
	}

	DPHUDController_Component* FindHud()
	{
		DPHUDController_Component* pxHud = nullptr;
		DP_Query::ForEachComponentInActiveScene<DPHUDController_Component>(
			[&pxHud](Zenith_EntityID, DPHUDController_Component& xH)
			{
				if (pxHud == nullptr) pxHud = &xH;
			});
		return pxHud;
	}

	Zenith_UI::Zenith_UIText* FindHudStatus()
	{
		Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xActive);
		if (pxScene == nullptr) return nullptr;
		Zenith_UI::Zenith_UIText* pxResult = nullptr;
		pxScene->Query<Zenith_UIComponent>().ForEach(
			[&pxResult](Zenith_EntityID, Zenith_UIComponent& xUI)
			{
				if (pxResult != nullptr) return;
				pxResult = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status");
			});
		return pxResult;
	}
}

static void Setup_P4LossDawn()
{
	g_iPhase = kLD_Start;
	g_iTickFrames = 0;
	g_iRunLostDawnCount = 0;
	g_bHudFired = false;
	g_eHudCause = DP_RunLostCause::Apprehended;
	std::snprintf(g_szHudStatusText, sizeof(g_szHudStatusText), "<unset>");
	g_bHudStatusVisible = false;
	g_xRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(&OnRunLost);
}

static bool Step_P4LossDawn(int iFrame)
{
	switch (g_iPhase)
	{
	case kLD_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kLD_WaitScene;
		return true;

	case kLD_WaitScene:
	{
		// Wait for HUD + at least one villager (so DPPlayerController is
		// guaranteed to be ticking TickNight from OnUpdate -- it only fires
		// when the controller exists, which happens after scene authoring).
		DPHUDController_Component* pxHud = FindHud();
		int iVillagerCount = 0;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&iVillagerCount](Zenith_EntityID, DPVillager_Component&)
			{
				++iVillagerCount;
			});
		if (pxHud != nullptr && iVillagerCount > 0)
		{
			pxHud->ResetRunLostForTest();
			g_iPhase = kLD_ArmTimer;
		}
		else if (iFrame > 90)
		{
			g_iPhase = kLD_Done;
		}
		return true;
	}

	case kLD_ArmTimer:
		DP_Night::StartNight(kNIGHT_DURATION_S);
		g_iTickFrames = 0;
		g_iPhase = kLD_TickWindow;
		return true;

	case kLD_TickWindow:
		++g_iTickFrames;
		if (g_iTickFrames >= kTICK_WINDOW_FRAMES) g_iPhase = kLD_Snapshot;
		return true;

	case kLD_Snapshot:
	{
		DPHUDController_Component* pxHud = FindHud();
		if (pxHud != nullptr)
		{
			g_bHudFired = pxHud->DidRunLostHandlerFireForTest();
			g_eHudCause = pxHud->LastRunLostCauseForTest();
		}
		Zenith_UI::Zenith_UIText* pxStatus = FindHudStatus();
		if (pxStatus != nullptr)
		{
			const std::string& strText = pxStatus->GetText();
			std::snprintf(g_szHudStatusText, sizeof(g_szHudStatusText),
				"%s", strText.c_str());
			g_bHudStatusVisible = pxStatus->IsVisible();
		}
		g_iPhase = kLD_Verify;
		return true;
	}

	case kLD_Verify:
		std::printf("[P4LossDawn] dispatched=%d hudFired=%d hudCause=%d "
			"hudStatusText=%s hudStatusVisible=%d ticks=%d\n",
			g_iRunLostDawnCount,
			(int)g_bHudFired, (int)g_eHudCause,
			g_szHudStatusText, (int)g_bHudStatusVisible, g_iTickFrames);
		std::fflush(stdout);
		Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
		g_xRunLostHandle = INVALID_EVENT_HANDLE;
		// Reset DP_Night so the next batched test starts clean.
		DP_Night::Reset();
		g_iPhase = kLD_Done;
		return false;

	case kLD_Done:
	default:
		if (g_xRunLostHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
			g_xRunLostHandle = INVALID_EVENT_HANDLE;
		}
		return false;
	}
}

static bool Verify_P4LossDawn()
{
	if (g_iRunLostDawnCount != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossDawn: DP_OnRunLost{Dawn} fired %d times, expected exactly 1 -- the cross-zero detection failed, OR DP_Night::Reset() between tests didn't clear m_bDawnDispatched so a duplicate fired",
			g_iRunLostDawnCount);
		return false;
	}
	if (!g_bHudFired)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossDawn: HUD's DidRunLostHandlerFireForTest() returned false -- DPHUDController didn't subscribe to DP_OnRunLost, OR ResetRunLostForTest() ran AFTER the dispatch (timing bug)");
		return false;
	}
	if (g_eHudCause != DP_RunLostCause::Dawn)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossDawn: HUD recorded cause=%d (expected %d=Dawn) -- subscriber lambda forgot to cache xEvt.m_eCause, OR a stale Apprehended dispatch from an earlier test polluted the state",
			(int)g_eHudCause, (int)DP_RunLostCause::Dawn);
		return false;
	}
	if (std::strcmp(g_szHudStatusText, "DAWN BREAKS") != 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossDawn: HUD Status text is '%s', expected 'DAWN BREAKS' -- BuildRunLostText returned wrong string for Dawn cause",
			g_szHudStatusText);
		return false;
	}
	if (!g_bHudStatusVisible)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossDawn: HUD Status text not visible -- SetStatusText's SetVisible(true) didn't take");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP4LossDawnTest = {
	"Test_P4Playthrough_LossByDawn",
	&Setup_P4LossDawn,
	&Step_P4LossDawn,
	&Verify_P4LossDawn,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP4LossDawnTest);

#endif // ZENITH_INPUT_SIMULATOR
