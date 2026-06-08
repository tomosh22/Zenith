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
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPHUDController_Behaviour.h"
#include "Components/DPPauseMenuController_Behaviour.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P4UI_RestartPromptAfterRunOver (MVP-4.3.2)
//
// Pins the "press any key to restart" overlay contract. When the run
// ends (either DP_OnVictory or DP_OnRunLost), the player should see
// a "Press R to restart, Q to quit" hint AND those keys should work
// without the player first having to open the pause menu.
//
// Verifies the full chain:
//   1. DP_OnVictory dispatches.
//   2. DPHUDController_Behaviour's subscriber sets m_bRunOver = true.
//   3. DPHUDController_Behaviour::OnUpdate makes the RestartPrompt UI
//      element visible with the canonical hint text.
//   4. DPPauseMenuController_Behaviour's subscriber sets its OWN
//      m_bRunOver = true (the two controllers track independently
//      so neither needs the other's interface).
//   5. PauseMenu::IsRunOverForTest() returns true -- so any R-key
//      press from the input handler will fire HandleRestart even
//      though m_bShown is false.
//   6. Repeat with DP_OnRunLost{Dawn} as the trigger.
//
// Procedure (single test exercises both code paths because the
// production wiring is identical):
//   PHASE A: Victory path
//     a. Load GameLevel; find HUD + PauseManager.
//     b. Reset HUD's and PauseMenu's run-over flags.
//     c. Verify RestartPrompt is NOT visible pre-event.
//     d. Dispatch DP_OnVictory.
//     e. Tick one frame so OnUpdate observes the flag flip.
//     f. Snapshot: HUD::IsRunOverForTest, PauseMenu::IsRunOverForTest,
//        RestartPrompt visibility, RestartPrompt text.
//   PHASE B: Dawn (loss) path
//     a. Reset both flags via the test accessors.
//     b. Dispatch DP_OnRunLost{Dawn}.
//     c. Tick one frame.
//     d. Snapshot again.
//
// What this catches:
//   * The HUD's OnUpdate forgets to flip RestartPrompt visible
//     when m_bRunOver is true.
//   * The DPHUDController's DP_OnVictory subscriber forgets to
//     also set m_bRunOver (it always set m_bRunLostReceived before
//     MVP-4.3.2; the new flag is the addition).
//   * DPPauseMenuController forgets to subscribe to DP_OnVictory
//     (or DP_OnRunLost) and the R/Q shortcuts only work after the
//     player manually opens the pause menu with Esc.
//   * The RestartPrompt UI element was never authored in the
//     scene (a future refactor of DevilsPlayground.cpp's HUD
//     authoring sequence could drop the line by accident).
// ============================================================================

namespace
{
	enum Phase : int {
		kRP_Start, kRP_WaitScene, kRP_ResetA, kRP_SnapshotPre_A,
		kRP_DispatchVictory, kRP_TickA, kRP_SnapshotPost_A,
		kRP_ResetB, kRP_DispatchRunLost, kRP_TickB,
		kRP_SnapshotPost_B, kRP_Verify, kRP_Done
	};

	int g_iPhase = kRP_Start;
	Zenith_EntityID g_xVillager;
	DPHUDController_Behaviour* g_pxHud = nullptr;

	// Pre-event snapshot (sanity: prompt is hidden before run ends).
	bool g_bPromptVisiblePreA = false;
	bool g_bHudRunOverPreA = false;
	bool g_bPauseRunOverPreA = false;

	// Phase A (Victory) snapshot.
	bool g_bPromptVisiblePostA = false;
	bool g_bHudRunOverPostA = false;
	bool g_bPauseRunOverPostA = false;
	char g_szPromptTextPostA[128] = "<unset>";

	// Phase B (RunLost Dawn) snapshot.
	bool g_bPromptVisiblePostB = false;
	bool g_bHudRunOverPostB = false;
	bool g_bPauseRunOverPostB = false;
	char g_szPromptTextPostB[128] = "<unset>";

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

	// Walk all loaded scenes for RestartPrompt. The element lives on the
	// GameManager's UICanvas in the active scene today, but use the
	// PR #81 robust pattern in case future work migrates it.
	Zenith_UI::Zenith_UIText* FindRestartPromptText()
	{
		Zenith_UI::Zenith_UIText* pxResult = nullptr;
		const uint32_t uSlotCount = g_xEngine.Scenes().GetSceneSlotCount();
		for (uint32_t uSlot = 0; uSlot < uSlotCount; ++uSlot)
		{
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetLoadedSceneDataAtSlot(uSlot);
			if (pxScene == nullptr) continue;
			pxScene->Query<Zenith_UIComponent>().ForEach(
				[&pxResult](Zenith_EntityID, Zenith_UIComponent& xUI)
				{
					if (pxResult != nullptr) return;
					pxResult = xUI.FindElement<Zenith_UI::Zenith_UIText>("RestartPrompt");
				});
			if (pxResult) break;
		}
		return pxResult;
	}

	void SnapshotPromptText(char* szOut, size_t uOutSize, bool& bOutVisible)
	{
		Zenith_UI::Zenith_UIText* pxPrompt = FindRestartPromptText();
		if (pxPrompt == nullptr)
		{
			std::snprintf(szOut, uOutSize, "<missing>");
			bOutVisible = false;
			return;
		}
		std::snprintf(szOut, uOutSize, "%s", pxPrompt->GetText().c_str());
		bOutVisible = pxPrompt->IsVisible();
	}
}

static void Setup_P4RestartPrompt()
{
	g_iPhase = kRP_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_pxHud = nullptr;
	g_bPromptVisiblePreA = false;
	g_bHudRunOverPreA = false;
	g_bPauseRunOverPreA = false;
	g_bPromptVisiblePostA = false;
	g_bHudRunOverPostA = false;
	g_bPauseRunOverPostA = false;
	std::snprintf(g_szPromptTextPostA, sizeof(g_szPromptTextPostA), "<unset>");
	g_bPromptVisiblePostB = false;
	g_bHudRunOverPostB = false;
	g_bPauseRunOverPostB = false;
	std::snprintf(g_szPromptTextPostB, sizeof(g_szPromptTextPostB), "<unset>");
}

static bool Step_P4RestartPrompt(int iFrame)
{
	switch (g_iPhase)
	{
	case kRP_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kRP_WaitScene;
		return true;

	case kRP_WaitScene:
	{
		Zenith_EntityID xFoundV;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFoundV](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFoundV.IsValid()) xFoundV = xId;
			});
		DPHUDController_Behaviour* pxHud = FindHud();
		const bool bHasPause =
			DPPauseMenuController_Behaviour::GetPersistentInstanceForTest() != nullptr;
		if (xFoundV.IsValid() && pxHud != nullptr && bHasPause)
		{
			g_xVillager = xFoundV;
			g_pxHud = pxHud;
			g_iPhase = kRP_ResetA;
		}
		else if (iFrame > 120)
		{
			g_iPhase = kRP_Done;
		}
		return true;
	}

	case kRP_ResetA:
		// Reset both controllers' run-over flags so we observe a clean
		// rising edge on dispatch.
		g_pxHud->ResetRunLostForTest();
		DPPauseMenuController_Behaviour::ResetForTest();
		// Re-pull the persistent instance after ResetForTest (it doesn't
		// drop the singleton, just clears state).
		g_iPhase = kRP_SnapshotPre_A;
		return true;

	case kRP_SnapshotPre_A:
	{
		g_bHudRunOverPreA = g_pxHud->IsRunOverForTest();
		g_bPauseRunOverPreA = DPPauseMenuController_Behaviour::IsRunOverForTest();
		Zenith_UI::Zenith_UIText* pxPrompt = FindRestartPromptText();
		g_bPromptVisiblePreA = pxPrompt != nullptr ? pxPrompt->IsVisible() : false;
		g_iPhase = kRP_DispatchVictory;
		return true;
	}

	case kRP_DispatchVictory:
		Zenith_EventDispatcher::Get().Dispatch(DP_OnVictory{});
		g_iPhase = kRP_TickA;
		return true;

	case kRP_TickA:
		// One frame for the HUD's OnUpdate to see m_bRunOver=true and
		// flip the RestartPrompt's visibility.
		g_iPhase = kRP_SnapshotPost_A;
		return true;

	case kRP_SnapshotPost_A:
		g_bHudRunOverPostA = g_pxHud->IsRunOverForTest();
		g_bPauseRunOverPostA = DPPauseMenuController_Behaviour::IsRunOverForTest();
		SnapshotPromptText(g_szPromptTextPostA, sizeof(g_szPromptTextPostA), g_bPromptVisiblePostA);
		g_iPhase = kRP_ResetB;
		return true;

	case kRP_ResetB:
		// Reset for the RunLost path.
		g_pxHud->ResetRunLostForTest();
		DPPauseMenuController_Behaviour::ResetForTest();
		g_iPhase = kRP_DispatchRunLost;
		return true;

	case kRP_DispatchRunLost:
		Zenith_EventDispatcher::Get().Dispatch(DP_OnRunLost{ DP_RunLostCause::Dawn });
		g_iPhase = kRP_TickB;
		return true;

	case kRP_TickB:
		g_iPhase = kRP_SnapshotPost_B;
		return true;

	case kRP_SnapshotPost_B:
		g_bHudRunOverPostB = g_pxHud->IsRunOverForTest();
		g_bPauseRunOverPostB = DPPauseMenuController_Behaviour::IsRunOverForTest();
		SnapshotPromptText(g_szPromptTextPostB, sizeof(g_szPromptTextPostB), g_bPromptVisiblePostB);
		g_iPhase = kRP_Verify;
		return true;

	case kRP_Verify:
		std::printf("[P4RestartPrompt] Pre-A: hudRO=%d pauseRO=%d promptVis=%d\n",
			(int)g_bHudRunOverPreA, (int)g_bPauseRunOverPreA, (int)g_bPromptVisiblePreA);
		std::printf("[P4RestartPrompt] Victory: hudRO=%d pauseRO=%d promptVis=%d promptText='%s'\n",
			(int)g_bHudRunOverPostA, (int)g_bPauseRunOverPostA,
			(int)g_bPromptVisiblePostA, g_szPromptTextPostA);
		std::printf("[P4RestartPrompt] RunLost(Dawn): hudRO=%d pauseRO=%d promptVis=%d promptText='%s'\n",
			(int)g_bHudRunOverPostB, (int)g_bPauseRunOverPostB,
			(int)g_bPromptVisiblePostB, g_szPromptTextPostB);
		std::fflush(stdout);
		// Cleanup: leave both controllers reset for the next batched test.
		g_pxHud->ResetRunLostForTest();
		DPPauseMenuController_Behaviour::ResetForTest();
		g_iPhase = kRP_Done;
		return false;

	case kRP_Done:
	default:
		return false;
	}
}

static bool Verify_P4RestartPrompt()
{
	if (g_pxHud == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4RestartPrompt: HUD controller not found in GameLevel after 120 frames -- scene authoring may have dropped the AttachScript step");
		return false;
	}
	// Pre-event sanity: flags clear, prompt hidden. Establishes that the
	// resets and authoring defaults are sane.
	if (g_bHudRunOverPreA || g_bPauseRunOverPreA)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4RestartPrompt: HUD or PauseMenu reported run-over BEFORE any event dispatched. ResetForTest didn't clear m_bRunOver");
		return false;
	}
	if (g_bPromptVisiblePreA)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4RestartPrompt: RestartPrompt was visible BEFORE any event dispatched. Element should be authored hidden by default");
		return false;
	}
	// Phase A: Victory triggers the prompt + both controllers' flags.
	if (!g_bHudRunOverPostA)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4RestartPrompt: after DP_OnVictory, HUD m_bRunOver is false -- DPHUDController's victory subscriber regressed (was only setting status text; should also set m_bRunOver)");
		return false;
	}
	if (!g_bPauseRunOverPostA)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4RestartPrompt: after DP_OnVictory, PauseMenu m_bRunOver is false -- DPPauseMenuController forgot to subscribe to DP_OnVictory in OnStart. R/Q shortcuts won't work after victory without first opening the pause menu");
		return false;
	}
	if (!g_bPromptVisiblePostA)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4RestartPrompt: after DP_OnVictory, RestartPrompt is not visible. HUD's OnUpdate didn't flip the element when m_bRunOver=true");
		return false;
	}
	if (std::strcmp(g_szPromptTextPostA, "Press R to restart, Q to quit") != 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4RestartPrompt: after Victory, RestartPrompt text is '%s', expected 'Press R to restart, Q to quit'. HUD's prompt-text writer regressed",
			g_szPromptTextPostA);
		return false;
	}
	// Phase B: RunLost triggers the same flags + prompt visibility.
	if (!g_bHudRunOverPostB || !g_bPauseRunOverPostB)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4RestartPrompt: after DP_OnRunLost{Dawn}, hudRunOver=%d pauseRunOver=%d. One or both subscribers regressed",
			(int)g_bHudRunOverPostB, (int)g_bPauseRunOverPostB);
		return false;
	}
	if (!g_bPromptVisiblePostB)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4RestartPrompt: after DP_OnRunLost, RestartPrompt is not visible. Either the HUD's OnUpdate condition was specific to Victory, OR the RunLost reset between phases stuck m_bRunOver to false");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP4RestartPromptTest = {
	"Test_P4UI_RestartPromptAfterRunOver",
	&Setup_P4RestartPrompt,
	&Step_P4RestartPrompt,
	&Verify_P4RestartPrompt,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP4RestartPromptTest);

#endif // ZENITH_INPUT_SIMULATOR
