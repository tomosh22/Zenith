#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIText.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPHUDController_Behaviour.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include <cmath>
#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P4Playthrough_LossByApprehend (MVP-4.2 -- Apprehend playthrough)
//
// Test_P1Apprehend_PriestCatchesPlayer already pins the priest's BT
// Apprehend branch + DP_OnRunLost{Apprehended} dispatch. This test
// pins the FULL playthrough chain: the dispatch reaches the HUD
// subscriber and the HUD writes the correct run-lost banner copy.
//
// Procedure:
//   1. Load GameLevel + subscribe to DP_OnRunLost.
//   2. Pick the priest + the closest villager (matches the Phase 1
//      apprehend test's selection so the scene state is identical).
//   3. Possess the villager via DP_Player::SetPossessedVillager.
//   4. Teleport the priest to ~1.5 m from the villager -- inside the
//      2 m apprehend range default.
//   5. Run frames until DP_OnRunLost fires (or 360-frame timeout).
//   6. Verify:
//        DP_OnRunLost fired with cause = Apprehended
//        HUD's DidRunLostHandlerFireForTest() returns true
//        HUD's LastRunLostCauseForTest() returns Apprehended
//        HUD "Status" UI element text == "CAUGHT BY AELFRIC"
//
// What this catches (vs the Phase 1 unit test):
//   * DPHUDController didn't subscribe to DP_OnRunLost.
//   * HUD subscriber subscribed but didn't write the banner text.
//   * BuildRunLostText returned the wrong string for Apprehended cause.
//   * Status element wasn't authored on the HUD entity (the SetStatusText
//     code path silently no-ops if the element is missing).
// ============================================================================

namespace
{
	enum Phase : int { kLA_Start, kLA_WaitScene, kLA_Possess, kLA_TeleportPriest,
	                   kLA_RunChannel, kLA_Snapshot, kLA_Verify, kLA_Done };

	int                     g_iPhase = kLA_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xVillager;
	Zenith_EventHandle      g_xRunLostHandle = INVALID_EVENT_HANDLE;
	bool                    g_bRunLostFired = false;
	DP_RunLostCause         g_eRunLostCause = DP_RunLostCause::Apprehended;
	int                     g_iRunFrames = 0;
	bool                    g_bHudFired = false;
	DP_RunLostCause         g_eHudCause = DP_RunLostCause::Apprehended;
	char                    g_szHudStatusText[128] = "<unset>";
	bool                    g_bHudStatusVisible = false;

	void OnRunLost(const DP_OnRunLost& xEvt)
	{
		g_bRunLostFired = true;
		g_eRunLostCause = xEvt.m_eCause;
	}

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	bool TrySetEntityPos(Zenith_EntityID xId, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
		return true;
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
		Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xActive);
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

static void Setup_P4LossApprehend()
{
	g_iPhase = kLA_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xVillager = INVALID_ENTITY_ID;
	g_bRunLostFired = false;
	g_eRunLostCause = DP_RunLostCause::Apprehended;
	g_iRunFrames = 0;
	g_bHudFired = false;
	g_eHudCause = DP_RunLostCause::Apprehended;
	std::snprintf(g_szHudStatusText, sizeof(g_szHudStatusText), "<unset>");
	g_bHudStatusVisible = false;
	g_xRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(&OnRunLost);
}

static bool Step_P4LossApprehend(int iFrame)
{
	switch (g_iPhase)
	{
	case kLA_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kLA_WaitScene;
		return true;

	case kLA_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		Zenith_EntityID xFoundVillager;
		float fClosestDist = 1e30f;

		Zenith_Maths::Vector3 xPriestPos(0.0f);
		bool bGotPriestPos = false;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundPriest, &xPriestPos, &bGotPriestPos]
			(Zenith_EntityID xId, Priest_Behaviour&)
			{
				xFoundPriest = xId;
				bGotPriestPos = TryGetEntityPos(xId, xPriestPos);
			});

		if (xFoundPriest.IsValid() && bGotPriestPos)
		{
			DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
				[&xFoundVillager, &fClosestDist, &xPriestPos]
				(Zenith_EntityID xId, DPVillager_Behaviour&)
				{
					Zenith_Maths::Vector3 xVPos;
					if (!TryGetEntityPos(xId, xVPos)) return;
					const float fD = HorizontalDistance(xPriestPos, xVPos);
					if (fD < fClosestDist)
					{
						fClosestDist = fD;
						xFoundVillager = xId;
					}
				});
		}

		// Wait for HUD to exist too -- it's authored alongside the GameManager.
		DPHUDController_Behaviour* pxHud = FindHud();
		if (xFoundPriest.IsValid() && xFoundVillager.IsValid() && pxHud != nullptr)
		{
			// Reset HUD run-lost flag so we observe this run's dispatch, not a
			// stale one from a previous batched test that may have leaked.
			pxHud->ResetRunLostForTest();
			g_xPriest   = xFoundPriest;
			g_xVillager = xFoundVillager;
			g_iPhase    = kLA_Possess;
		}
		else if (iFrame > 90)
		{
			g_iPhase = kLA_Done;
		}
		return true;
	}

	case kLA_Possess:
		// MVP-1.9 cleanup: register the villager so the priest's
		// sight pass considers it once teleported into range.
		Zenith_PerceptionSystem::RegisterTarget(g_xVillager, /*hostile=*/true);
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kLA_TeleportPriest;
		return true;

	case kLA_TeleportPriest:
	{
		Zenith_Maths::Vector3 xVillagerPos;
		if (!TryGetEntityPos(g_xVillager, xVillagerPos)) { g_iPhase = kLA_Done; return false; }
		const Zenith_Maths::Vector3 xPriestPos(
			xVillagerPos.x + 1.5f, xVillagerPos.y, xVillagerPos.z);
		TrySetEntityPos(g_xPriest, xPriestPos);
		g_iRunFrames = 0;
		g_iPhase = kLA_RunChannel;
		return true;
	}

	case kLA_RunChannel:
		++g_iRunFrames;
		// Channel default 3 s; allow 360 frames (~6 s) for BT reset latency.
		// After dispatch, wait an additional 3 frames for the HUD's
		// SetStatusText to make it through the dispatcher invocation.
		if (g_bRunLostFired && g_iRunFrames >= 5)
		{
			// Allow at least 5 frames AND dispatch fired -- the dispatcher
			// is synchronous so by next frame the HUD subscriber will have
			// written the text.
			g_iPhase = kLA_Snapshot;
		}
		else if (g_iRunFrames >= 360)
		{
			g_iPhase = kLA_Snapshot;
		}
		return true;

	case kLA_Snapshot:
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
		g_iPhase = kLA_Verify;
		return true;
	}

	case kLA_Verify:
		std::printf("[P4LossApprehend] dispatched=%d cause=%d hudFired=%d hudCause=%d "
			"hudStatusText=%s hudStatusVisible=%d frames=%d\n",
			(int)g_bRunLostFired, (int)g_eRunLostCause,
			(int)g_bHudFired, (int)g_eHudCause,
			g_szHudStatusText, (int)g_bHudStatusVisible, g_iRunFrames);
		std::fflush(stdout);
		Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
		g_xRunLostHandle = INVALID_EVENT_HANDLE;
		g_iPhase = kLA_Done;
		return false;

	case kLA_Done:
	default:
		if (g_xRunLostHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
			g_xRunLostHandle = INVALID_EVENT_HANDLE;
		}
		return false;
	}
}

static bool Verify_P4LossApprehend()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P4LossApprehend: priest entity not found");
		return false;
	}
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P4LossApprehend: villager entity not found");
		return false;
	}
	if (!g_bRunLostFired)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossApprehend: DP_OnRunLost never fired (frames=%d) -- the priest's apprehend channel didn't complete, OR the apprehend action regressed and stopped dispatching DP_OnRunLost",
			g_iRunFrames);
		return false;
	}
	if (g_eRunLostCause != DP_RunLostCause::Apprehended)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossApprehend: DP_OnRunLost fired with cause=%d (expected %d=Apprehended)",
			(int)g_eRunLostCause, (int)DP_RunLostCause::Apprehended);
		return false;
	}
	if (!g_bHudFired)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossApprehend: HUD's DidRunLostHandlerFireForTest() returned false -- DPHUDController didn't subscribe to DP_OnRunLost, OR the subscription handle was unsubscribed prematurely (check TearDown)");
		return false;
	}
	if (g_eHudCause != DP_RunLostCause::Apprehended)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossApprehend: HUD recorded cause=%d (expected %d=Apprehended) -- subscriber lambda forgot to cache xEvt.m_eCause",
			(int)g_eHudCause, (int)DP_RunLostCause::Apprehended);
		return false;
	}
	if (std::strcmp(g_szHudStatusText, "CAUGHT BY AELFRIC") != 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossApprehend: HUD Status text is '%s', expected 'CAUGHT BY AELFRIC' -- BuildRunLostText returned wrong string for Apprehended cause, OR the SetStatusText call path failed",
			g_szHudStatusText);
		return false;
	}
	if (!g_bHudStatusVisible)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P4LossApprehend: HUD Status text not visible -- SetStatusText's SetVisible(true) didn't take");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP4LossApprehendTest = {
	"Test_P4Playthrough_LossByApprehend",
	&Setup_P4LossApprehend,
	&Step_P4LossApprehend,
	&Verify_P4LossApprehend,
	500
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP4LossApprehendTest);

#endif // ZENITH_INPUT_SIMULATOR
