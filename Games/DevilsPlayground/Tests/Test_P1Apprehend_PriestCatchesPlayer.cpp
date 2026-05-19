#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "Source/DP_Tuning.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include <cmath>

// ============================================================================
// Test_P1Apprehend_PriestCatchesPlayer (MVP-1.3.1 + 1.3.2)
//
// End-to-end apprehend flow on the real GameLevel scene:
//   1. Load GameLevel (scene 1) -- spawns priest + 17 villagers.
//   2. Subscribe to DP_OnRunLost so we can detect dispatch.
//   3. Pick the priest + the closest villager (same logic as
//      Test_PriestPursuit so the apprehend can fire from a known
//      starting separation).
//   4. Possess the chosen villager via DP_Player::SetPossessedVillager;
//      Priest_Behaviour::BridgePerceptionToBlackboard observes the
//      possession on the next frame and writes BB_KEY_TARGET_WITH_DEVIL.
//   5. Teleport the priest to ~1.5 m horizontal from the villager --
//      well inside priest.apprehend_range_m (2.0 m default) so the
//      Apprehend BT branch is the highest-priority Selector child that
//      can run.
//   6. Run frames for priest.apprehend_channel_s + slack (~4 s at
//      60 Hz = 240 frames; we use 360 for safety).
//   7. Assert:
//      a) DP_OnRunLost fired with cause = Apprehended.
//      b) DP_Player::GetPossessedVillager() returns INVALID (the
//         Apprehend action clears possession on channel complete).
// ============================================================================

namespace
{
	enum Phase : int { kAP_Start, kAP_WaitScene, kAP_Possess, kAP_TeleportPriest,
	                   kAP_RunChannel, kAP_Verify, kAP_Done };

	int                     g_iPhase = kAP_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xVillager;
	Zenith_EventHandle      g_xRunLostHandle = INVALID_EVENT_HANDLE;
	bool                    g_bRunLostFired = false;
	DP_RunLostCause         g_eRunLostCause = DP_RunLostCause::Apprehended;
	int                     g_iRunFrames = 0;
	bool                    g_bPossessionCleared = false;

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
}

static void Setup_P1Apprehend()
{
	g_iPhase = kAP_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xVillager = INVALID_ENTITY_ID;
	g_bRunLostFired = false;
	g_eRunLostCause = DP_RunLostCause::Apprehended;
	g_iRunFrames = 0;
	g_bPossessionCleared = false;
	// Subscribe in setup so we don't miss any early dispatch from the
	// priest's first OnUpdate after possession (defensive; the channel
	// time + slack means a single-frame race is unlikely, but it costs
	// nothing to subscribe early).
	g_xRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(&OnRunLost);
}

static bool Step_P1Apprehend(int iFrame)
{
	switch (g_iPhase)
	{
	case kAP_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kAP_WaitScene;
		return true;

	case kAP_WaitScene:
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

		if (xFoundPriest.IsValid() && xFoundVillager.IsValid())
		{
			g_xPriest   = xFoundPriest;
			g_xVillager = xFoundVillager;
			g_iPhase    = kAP_Possess;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kAP_Done;
		}
		return true;
	}

	case kAP_Possess:
		// MVP-1.9 cleanup: register the villager so the priest's sight
		// pass considers it. (Production DPVillager_Behaviour doesn't
		// self-register today; the omniscient fallback obviated that.)
		// Real perception now drives BB_KEY_TARGET_WITH_DEVIL once the
		// priest teleports within sight range of the villager in the
		// next phase.
		Zenith_PerceptionSystem::RegisterTarget(g_xVillager, /*hostile=*/true);
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kAP_TeleportPriest;
		return true;

	case kAP_TeleportPriest:
	{
		// Wait one frame after Possess so DPVillager_Behaviour::OnUpdate
		// has a chance to observe DP_Player's possession side-table and
		// flip its IsPossessed() flag (the priest's bridge needs that).
		// Then teleport the priest to ~1.5 m east of the villager --
		// well inside the 2.0 m apprehend_range default.
		Zenith_Maths::Vector3 xVillagerPos;
		if (!TryGetEntityPos(g_xVillager, xVillagerPos)) { g_iPhase = kAP_Done; return false; }
		const Zenith_Maths::Vector3 xPriestPos(
			xVillagerPos.x + 1.5f, xVillagerPos.y, xVillagerPos.z);
		TrySetEntityPos(g_xPriest, xPriestPos);
		g_iRunFrames = 0;
		g_iPhase = kAP_RunChannel;
		return true;
	}

	case kAP_RunChannel:
		++g_iRunFrames;
		// 4 s at 60 Hz = ~240 frames. Channel default is 3 s. We allow
		// 360 frames (~6 s) so the test still passes if the priest's
		// BT reset latency or one-frame possession-propagation delay
		// extends the elapsed window.
		if (g_bRunLostFired || g_iRunFrames >= 360)
		{
			g_iPhase = kAP_Verify;
		}
		return true;

	case kAP_Verify:
	{
		g_bPossessionCleared =
			!DP_Player::GetPossessedVillager().IsValid();
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Apprehend: runLost=%d cause=%d possessionCleared=%d frames=%d",
			(int)g_bRunLostFired, (int)g_eRunLostCause,
			(int)g_bPossessionCleared, g_iRunFrames);
		// Unsubscribe before kAP_Done so a follow-on batched test that
		// dispatches DP_OnRunLost (none exist today, but future
		// MVP-1.3.5 / 4.2.x tests will) doesn't accidentally re-fire
		// our captured flag.
		Zenith_EventDispatcher::Get().Unsubscribe(g_xRunLostHandle);
		g_iPhase = kAP_Done;
		return false;
	}

	case kAP_Done:
	default:
		return false;
	}
}

static bool Verify_P1Apprehend()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Apprehend: priest entity not found");
		return false;
	}
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Apprehend: villager entity not found");
		return false;
	}
	if (!g_bRunLostFired)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Apprehend: DP_OnRunLost never fired (frames=%d)", g_iRunFrames);
		return false;
	}
	if (g_eRunLostCause != DP_RunLostCause::Apprehended)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Apprehend: DP_OnRunLost fired with cause=%d (expected %d=Apprehended)",
			(int)g_eRunLostCause, (int)DP_RunLostCause::Apprehended);
		return false;
	}
	if (!g_bPossessionCleared)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Apprehend: possession not cleared after apprehend");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1ApprehendTest = {
	"Test_P1Apprehend_PriestCatchesPlayer",
	&Setup_P1Apprehend,
	&Step_P1Apprehend,
	&Verify_P1Apprehend,
	500
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1ApprehendTest);

#endif // ZENITH_INPUT_SIMULATOR
