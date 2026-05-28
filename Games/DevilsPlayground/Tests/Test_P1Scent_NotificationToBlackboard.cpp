#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>

// ============================================================================
// Test_P1Scent_NotificationToBlackboard (MVP-1.6.3)
//
// Verifies the data-path from DP_Player's scent table to the priest's
// blackboard: after a successful possession, `WriteHighestScentToBlackboard`
// (driven from DPPlayerController_Behaviour::OnUpdate) must push the
// highest-scent villager's EntityID to BB_KEY_HIGH_SCENT_TARGET.
//
// In MVP no production behaviour CONSUMES this key (hounds are post-MVP);
// the test is the only reader. This locks the data path so the future
// hound BT can plug in without a separate plumbing PR.
//
// Procedure:
//   1. Load GameLevel; find the priest + a switchable villager pair.
//   2. SetPossessedVillager(A) -- system path, no scent.
//   3. TryVoluntaryPossessSwitch(B) -- voluntary, scent[B] = 0.3.
//   4. Run one frame so DPPlayerController::OnUpdate fires
//      TickDemonScent + WriteHighestScentToBlackboard.
//   5. Read the priest's BB_KEY_HIGH_SCENT_TARGET.
//   6. Assert it equals B's EntityID.
//
// The "one frame" wait is important: TryVoluntaryPossessSwitch
// accumulates scent immediately, but the BB write happens in the next
// OnUpdate pass. A test that reads BB inside the same Step that
// switched would always read INVALID (or stale data from the prior
// frame).
// ============================================================================

namespace
{
	enum Phase : int { kSN_Start, kSN_WaitScene, kSN_PossessA, kSN_SwitchToB,
	                   kSN_WaitForBlackboardWrite, kSN_ReadBlackboard,
	                   kSN_Done };

	int                     g_iPhase = kSN_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	Zenith_EntityID         g_xBBValue;
	bool                    g_bSwitchOk = false;
	int                     g_iWaitFrames = 0;

	// One frame's plenty for the controller to fire WriteHighestScent
	// after the switch. We wait 2 just to be safe against tier order.
	constexpr int kFRAMES_FOR_BB_WRITE = 2;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	void PickClosestPair(Zenith_EntityID& xA, Zenith_EntityID& xB)
	{
		struct Cand { Zenith_EntityID xId; Zenith_Maths::Vector3 xPos; };
		Zenith_Vector<Cand> axCands;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axCands](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				Cand xV; xV.xId = xId;
				if (TryGetEntityPos(xId, xV.xPos)) axCands.PushBack(xV);
			});
		if (axCands.GetSize() < 2) return;
		float fMin = 1e30f;
		for (uint32_t i = 0; i < axCands.GetSize(); ++i)
		{
			for (uint32_t j = i + 1; j < axCands.GetSize(); ++j)
			{
				const float fD = HorizontalDistance(
					axCands.Get(i).xPos, axCands.Get(j).xPos);
				if (fD < fMin)
				{
					fMin = fD;
					xA = axCands.Get(i).xId;
					xB = axCands.Get(j).xId;
				}
			}
		}
	}

	Zenith_EntityID ReadPriestBBHighScent(Zenith_EntityID xPriestId)
	{
		Zenith_SceneData* pxScene =
			g_xEngine.Scenes().GetSceneDataForEntity(xPriestId);
		if (pxScene == nullptr) return INVALID_ENTITY_ID;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xPriestId);
		if (!xEnt.IsValid()) return INVALID_ENTITY_ID;
		if (!xEnt.HasComponent<Zenith_AIAgentComponent>()) return INVALID_ENTITY_ID;
		Zenith_AIAgentComponent& xAg = xEnt.GetComponent<Zenith_AIAgentComponent>();
		return xAg.GetBlackboard().GetEntityID(DP_AI::BB_KEY_HIGH_SCENT_TARGET);
	}
}

static void Setup_P1ScentBB()
{
	g_iPhase = kSN_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_xBBValue = INVALID_ENTITY_ID;
	g_bSwitchOk = false;
	g_iWaitFrames = 0;
}

static bool Step_P1ScentBB(int iFrame)
{
	switch (g_iPhase)
	{
	case kSN_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kSN_WaitScene;
		return true;

	case kSN_WaitScene:
	{
		Zenith_EntityID xFoundPriest;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundPriest]
			(Zenith_EntityID xId, Priest_Behaviour&) { xFoundPriest = xId; });
		PickClosestPair(g_xA, g_xB);
		if (xFoundPriest.IsValid() && g_xA.IsValid() && g_xB.IsValid())
		{
			g_xPriest = xFoundPriest;
			g_iPhase = kSN_PossessA;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kSN_Done;
		}
		return true;
	}

	case kSN_PossessA:
		// Direct write so no scent accumulates on A. The first scent
		// bump in this test happens in kSN_SwitchToB.
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kSN_SwitchToB;
		return true;

	case kSN_SwitchToB:
		g_bSwitchOk = DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iWaitFrames = 0;
		g_iPhase = kSN_WaitForBlackboardWrite;
		return true;

	case kSN_WaitForBlackboardWrite:
		++g_iWaitFrames;
		if (g_iWaitFrames >= kFRAMES_FOR_BB_WRITE)
		{
			g_iPhase = kSN_ReadBlackboard;
		}
		return true;

	case kSN_ReadBlackboard:
		g_xBBValue = ReadPriestBBHighScent(g_xPriest);
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentBB: switchOk=%d bbValue=(%u/%u) expected=(%u/%u)",
			(int)g_bSwitchOk,
			g_xBBValue.m_uIndex, g_xBBValue.m_uGeneration,
			g_xB.m_uIndex, g_xB.m_uGeneration);
		g_iPhase = kSN_Done;
		return false;

	case kSN_Done:
	default:
		return false;
	}
}

static bool Verify_P1ScentBB()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ScentBB: priest not found");
		return false;
	}
	if (!g_xA.IsValid() || !g_xB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ScentBB: villager pair not found");
		return false;
	}
	if (!g_bSwitchOk)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ScentBB: voluntary switch to B refused -- range or cooldown gate misfired");
		return false;
	}
	if (g_xBBValue.m_uIndex != g_xB.m_uIndex
		|| g_xBBValue.m_uGeneration != g_xB.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentBB: priest BB_KEY_HIGH_SCENT_TARGET = (%u/%u), expected (%u/%u) (highest-scent villager B)",
			g_xBBValue.m_uIndex, g_xBBValue.m_uGeneration,
			g_xB.m_uIndex, g_xB.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1ScentBBTest = {
	"Test_P1Scent_NotificationToBlackboard",
	&Setup_P1ScentBB,
	&Step_P1ScentBB,
	&Verify_P1ScentBB,
	120
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1ScentBBTest);

#endif // ZENITH_INPUT_SIMULATOR
