#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Collections/Zenith_Vector.h"
#include "UI/Zenith_UIText.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPHUDController_Behaviour.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P4Playthrough_LossByNoVessels (MVP-4.2 -- NoVessels playthrough)
//
// Test_P1NoVessels_DispatchesRunLost pins the Kill()-driven NoVessels
// dispatch. This test pins the FULL playthrough: the dispatch reaches
// DPHUDController and the HUD writes "NO VESSELS REMAIN".
//
// Procedure:
//   1. Load GameLevel + subscribe to DP_OnRunLost.
//   2. Wait for HUD + at least 2 villagers.
//   3. Reset HUD run-lost flag (clear any leak from previous batched test).
//   4. Collect every villager EntityID, then call Kill() on each.
//      Kill()'s alive-count scan dispatches DP_OnRunLost{NoVessels} on
//      the last villager.
//   5. Wait one tick for the dispatch to land at the HUD subscriber.
//   6. Verify:
//        DP_OnRunLost fired with cause = NoVessels
//        HUD's DidRunLostHandlerFireForTest() returns true
//        HUD's LastRunLostCauseForTest() returns NoVessels
//        HUD "Status" UI element text == "NO VESSELS REMAIN"
//
// What this catches (vs Phase 1 unit test):
//   * HUD's BuildRunLostText returned wrong string for NoVessels cause.
//   * HUD's subscriber switch-case missed the NoVessels variant.
//   * Per-villager DP_OnVillagerDied "Possess another villager" status
//     update fired AFTER the run-lost status, overwriting it (banner
//     should be permanent when the run is over -- the most-recent dead
//     villager status would still show otherwise).
// ============================================================================

namespace
{
	enum Phase : int { kLNV_Start, kLNV_WaitScene, kLNV_KillAll,
	                   kLNV_WaitDispatch, kLNV_Snapshot, kLNV_Verify, kLNV_Done };

	int                     g_iPhase = kLNV_Start;
	int                     g_iVillagerCountAtStart = 0;
	int                     g_iWait = 0;
	Zenith_EventHandle      g_xRunLostHandle = INVALID_EVENT_HANDLE;
	int                     g_iRunLostNoVesselsCount = 0;
	bool                    g_bHudFired = false;
	DP_RunLostCause         g_eHudCause = DP_RunLostCause::Apprehended;
	char                    g_szHudStatusText[128] = "<unset>";
	bool                    g_bHudStatusVisible = false;

	void OnRunLost(const DP_OnRunLost& xEvt)
	{
		if (xEvt.m_eCause == DP_RunLostCause::NoVessels)
		{
			++g_iRunLostNoVesselsCount;
		}
	}

	DPHUDController_Behaviour* FindHud()
	{
		DPHUDController_Behaviour* pxHud = nullptr;
		DP_Query::ForEachScriptInActiveScene<DPHUDController_Behaviour>(
			[&pxHud](Zenith_EntityID, DPHUDController_Behaviour& xH)
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

static void Setup_P4LossNoVessels()
{
	g_iPhase = kLNV_Start;
	g_iVillagerCountAtStart = 0;
	g_iWait = 0;
	g_iRunLostNoVesselsCount = 0;
	g_bHudFired = false;
	g_eHudCause = DP_RunLostCause::Apprehended;
	std::snprintf(g_szHudStatusText, sizeof(g_szHudStatusText), "<unset>");
	g_bHudStatusVisible = false;
	g_xRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(&OnRunLost);
}

static bool Step_P4LossNoVessels(int iFrame)
{
	switch (g_iPhase)
	{
	case kLNV_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kLNV_WaitScene;
		return true;

	case kLNV_WaitScene:
	{
		int iCount = 0;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&iCount](Zenith_EntityID, DPVillager_Behaviour&) { ++iCount; });
		DPHUDController_Behaviour* pxHud = FindHud();
		if (iCount >= 2 && pxHud != nullptr)
		{
			pxHud->ResetRunLostForTest();
			g_iVillagerCountAtStart = iCount;
			g_iPhase = kLNV_KillAll;
		}
		else if (iFrame > 90)
		{
			g_iPhase = kLNV_Done;
		}
		return true;
	}

	case kLNV_KillAll:
	{
		Zenith_Vector<Zenith_EntityID> axVillagers;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axVillagers](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				axVillagers.PushBack(xId);
			});
		for (uint32_t u = 0; u < axVillagers.GetSize(); ++u)
		{
			Zenith_EntityID xId = axVillagers.Get(u);
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
			if (pxScene == nullptr) continue;
			Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
			if (!xEnt.IsValid()) continue;
			if (!xEnt.HasComponent<Zenith_ScriptComponent>()) continue;
			DPVillager_Behaviour* pxV =
				xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
			if (pxV != nullptr) pxV->Kill();
		}
		g_iWait = 0;
		g_iPhase = kLNV_WaitDispatch;
		return true;
	}

	case kLNV_WaitDispatch:
		++g_iWait;
		// One frame for the dispatch chain (Kill() is synchronous so the
		// HUD has already received the event, but the test status text
		// readback wants a clean OnUpdate cycle to be safe).
		if (g_iWait >= 3) g_iPhase = kLNV_Snapshot;
		return true;

	case kLNV_Snapshot:
	{
		DPHUDController_Behaviour* pxHud = FindHud();
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
		g_iPhase = kLNV_Verify;
		return true;
	}

	case kLNV_Verify:
		std::printf("[P4LossNoVessels] villagers=%d noVesselsCount=%d hudFired=%d hudCause=%d "
			"hudStatusText=%s hudStatusVisible=%d\n",
			g_iVillagerCountAtStart, g_iRunLostNoVesselsCount,
			(int)g_bHudFired, (int)g_eHudCause,
			g_szHudStatusText, (int)g_bHudStatusVisible);
		std::fflush(stdout);
		Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
		g_xRunLostHandle = INVALID_EVENT_HANDLE;
		g_iPhase = kLNV_Done;
		return false;

	case kLNV_Done:
	default:
		if (g_xRunLostHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
			g_xRunLostHandle = INVALID_EVENT_HANDLE;
		}
		return false;
	}
}

static bool Verify_P4LossNoVessels()
{
	if (g_iVillagerCountAtStart < 2)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossNoVessels: only %d villagers found in GameLevel (need >= 2 for the test to be meaningful)",
			g_iVillagerCountAtStart);
		return false;
	}
	if (g_iRunLostNoVesselsCount != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossNoVessels: DP_OnRunLost{NoVessels} fired %d times, expected exactly 1",
			g_iRunLostNoVesselsCount);
		return false;
	}
	if (!g_bHudFired)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossNoVessels: HUD's DidRunLostHandlerFireForTest() returned false -- DPHUDController didn't subscribe to DP_OnRunLost");
		return false;
	}
	if (g_eHudCause != DP_RunLostCause::NoVessels)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossNoVessels: HUD recorded cause=%d (expected %d=NoVessels) -- subscriber lambda forgot to cache xEvt.m_eCause",
			(int)g_eHudCause, (int)DP_RunLostCause::NoVessels);
		return false;
	}
	if (std::strcmp(g_szHudStatusText, "NO VESSELS REMAIN") != 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossNoVessels: HUD Status text is '%s', expected 'NO VESSELS REMAIN' -- BuildRunLostText returned wrong string for NoVessels cause, OR a per-villager death-status update fired AFTER and overwrote the permanent run-lost banner",
			g_szHudStatusText);
		return false;
	}
	if (!g_bHudStatusVisible)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossNoVessels: HUD Status text not visible -- SetStatusText's SetVisible(true) didn't take");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP4LossNoVesselsTest = {
	"Test_P4Playthrough_LossByNoVessels",
	&Setup_P4LossNoVessels,
	&Step_P4LossNoVessels,
	&Verify_P4LossNoVessels,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP4LossNoVesselsTest);

#endif // ZENITH_INPUT_SIMULATOR
