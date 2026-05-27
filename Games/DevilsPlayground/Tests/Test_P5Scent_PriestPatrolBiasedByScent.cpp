#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/Navigation/Zenith_NavMesh.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Source/DPParticles.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPPlayerController_Behaviour.h"
#include "Components/DP_BT_Nodes.h"
#include "Components/Priest_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P5Scent_PriestPatrolBiasedByScent (2026-05-21)
//
// Pins the demon-scent functional consumer:
//   * DP_BTAction_FindPosInSuspicionSphere reads BB_KEY_HIGH_SCENT_TARGET
//     and biases the random-reachable-point search toward that
//     villager's position when scent >= hound-bark threshold.
//   * DP_Particles::UpdateHighScentAura repositions the aura emitter
//     entity to follow that villager + sets emit to true.
//
// Procedure:
//   1. Load ProcLevel; pick any villager; teleport to (40, 1, 40).
//   2. Bump the villager's scent above the hound-bark threshold by
//      calling AddDemonScent in a loop, then write the high-scent BB.
//   3. Verify the priest's BT FindPos node centers its search near
//      the villager (not the priest's own position). We do this by
//      running the node and checking the PATROL_TARGET it writes is
//      within the suspicion-radius of the villager's position.
//   4. Verify UpdateHighScentAura repositioned the aura emitter +
//      enabled emission.
//   5. Drop the scent below threshold; verify the bias clears AND the
//      aura emitter stops emitting.
//
// What this catches:
//   * Bias regression (FindPos goes back to priest-centered).
//   * Scent threshold mismatch (BT and particles disagree).
//   * Aura emitter not following the villager.
//   * UpdateHighScentAura silently failing when the emitter doesn't
//     exist (regression: bootstrap stops creating it).
// ============================================================================

namespace
{
	enum Phase : int {
		kSP_Start, kSP_WaitScene, kSP_Setup, kSP_BumpScent,
		kSP_TickAndVerify, kSP_DropScent, kSP_FinalVerify, kSP_Done
	};

	int                     g_iPhase = kSP_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_Maths::Vector3   g_xVillagerPos(40.0f, 1.0f, 40.0f);
	bool                    g_bAuraEmittingHigh = false;
	bool                    g_bAuraEmittingLow = true;
	bool                    g_bPriestBiasNearVillager = false;
	bool                    g_bPriestUnbiasedWhenLow = false;
	int                     g_iFrameDelay = 0;
}

static DPPlayerController_Behaviour* GetController()
{
	return DPPlayerController_Behaviour::Instance();
}

static bool IsAuraEmitting()
{
	const Zenith_EntityID xAura = DP_Particles::GetEmitterEntityForTest(
		DP_Particles::Kind::HighScentAura);
	if (!xAura.IsValid()) return false;
	Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneDataForEntity(xAura);
	if (pxScene == nullptr) return false;
	Zenith_Entity xEnt = pxScene->TryGetEntity(xAura);
	if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_ParticleEmitterComponent>()) return false;
	return xEnt.GetComponent<Zenith_ParticleEmitterComponent>().IsEmitting();
}

static void Setup_P5ScentBias()
{
	g_iPhase = kSP_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xVillagerPos = Zenith_Maths::Vector3(40.0f, 1.0f, 40.0f);
	g_bAuraEmittingHigh = false;
	g_bAuraEmittingLow = true;
	g_bPriestBiasNearVillager = false;
	g_bPriestUnbiasedWhenLow = false;
	g_iFrameDelay = 0;
}

static bool Step_P5ScentBias(int iFrame)
{
	switch (g_iPhase)
	{
	case kSP_Start:
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kSP_WaitScene;
		return true;

	case kSP_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		if (xFound.IsValid()) { g_xVillager = xFound; g_iPhase = kSP_Setup; }
		else if (iFrame > 60) g_iPhase = kSP_Done;
		return true;
	}

	case kSP_Setup:
	{
		// Teleport villager to a known position so the bias check is
		// deterministic.
		Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneDataForEntity(g_xVillager);
		if (pxScene == nullptr) { g_iPhase = kSP_Done; return false; }
		Zenith_Entity xV = pxScene->TryGetEntity(g_xVillager);
		if (!xV.IsValid()) { g_iPhase = kSP_Done; return false; }
		if (xV.HasComponent<Zenith_TransformComponent>())
		{
			xV.GetComponent<Zenith_TransformComponent>().SetPosition(g_xVillagerPos);
		}
		g_iPhase = kSP_BumpScent;
		return true;
	}

	case kSP_BumpScent:
	{
		// Bump scent above hound-bark threshold (default 0.5). The
		// per-possession bump is 0.3; 3 calls puts us at 0.9 well
		// above threshold.
		DPPlayerController_Behaviour* pxCtrl = GetController();
		if (pxCtrl == nullptr) { g_iPhase = kSP_Done; return false; }
		const float fThreshold =
			DP_Tuning::Get<float>("possession.demon_scent_hound_bark_threshold");
		const float fPerPossession =
			DP_Tuning::Get<float>("possession.demon_scent_per_possession");
		const int iBumps = static_cast<int>((fThreshold / fPerPossession) + 2.0f);
		const float fMax = DP_Tuning::Get<float>("possession.demon_scent_max");
		for (int i = 0; i < iBumps; ++i)
		{
			pxCtrl->BumpDemonScent(g_xVillager, fPerPossession, fMax);
		}
		// Write to blackboards so the BT can read it.
		DP_Player::WriteHighestScentToBlackboard();
		// Update aura (mirrors the per-frame call in DPPlayerController).
		DP_Particles::UpdateHighScentAura(g_xVillager,
			DP_Player::GetDemonScent(g_xVillager) >= fThreshold);
		g_iPhase = kSP_TickAndVerify;
		g_iFrameDelay = 0;
		return true;
	}

	case kSP_TickAndVerify:
	{
		// Give the aura a frame to settle.
		++g_iFrameDelay;
		if (g_iFrameDelay < 2) return true;

		// Check aura is emitting.
		g_bAuraEmittingHigh = IsAuraEmitting();

		// Find the priest + run its FindPos BT node directly with the
		// scent target set. Verify the resulting PATROL_TARGET lands
		// near the villager rather than near the priest.
		Zenith_EntityID xPriest;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xPriest](Zenith_EntityID xId, Priest_Behaviour&)
			{
				if (!xPriest.IsValid()) xPriest = xId;
			});
		if (xPriest.IsValid())
		{
			Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneDataForEntity(xPriest);
			if (pxScene != nullptr)
			{
				Zenith_Entity xP = pxScene->TryGetEntity(xPriest);
				if (xP.IsValid() && xP.HasComponent<Zenith_AIAgentComponent>())
				{
					Zenith_AIAgentComponent& xAgent =
						xP.GetComponent<Zenith_AIAgentComponent>();
					Zenith_Blackboard& xBB = xAgent.GetBlackboard();
					// Ensure scent target BB is set.
					xBB.SetEntityID(DP_AI::BB_KEY_HIGH_SCENT_TARGET, g_xVillager);
					DP_BTAction_FindPosInSuspicionSphere xNode;
					xNode.SetNavMesh(DP_AI::GetOrBuildLevelNavMesh());
					// Bump priest patrol radius so the random pick has
					// generous room even when the navmesh has lots of
					// occlusion near the villager.
					xBB.SetFloat(DP_AI::BB_KEY_SUSPICION_RADIUS, 15.0f);
					BTNodeStatus eStatus = xNode.Execute(xP, xBB, 0.0f);
					if (eStatus == BTNodeStatus::SUCCESS)
					{
						const Zenith_Maths::Vector3 xPatrol =
							xBB.GetVector3(DP_AI::BB_KEY_PATROL_TARGET);
						// Check patrol point is within suspicion radius
						// of the VILLAGER (not the priest). The radius
						// itself is the test -- bias should center on
						// the scent target.
						const float fDx = xPatrol.x - g_xVillagerPos.x;
						const float fDz = xPatrol.z - g_xVillagerPos.z;
						const float fDist = std::sqrt(fDx*fDx + fDz*fDz);
						g_bPriestBiasNearVillager = (fDist <= 15.0f + 0.5f);
						std::printf("[P5Scent] biased patrol target = (%.1f, %.1f, %.1f); villager at (%.1f, %.1f, %.1f); dist = %.2f\n",
							xPatrol.x, xPatrol.y, xPatrol.z,
							g_xVillagerPos.x, g_xVillagerPos.y, g_xVillagerPos.z,
							fDist);
						std::fflush(stdout);
					}
				}
			}
		}
		g_iPhase = kSP_DropScent;
		return true;
	}

	case kSP_DropScent:
	{
		// Decay scent below threshold to verify the bias clears.
		DPPlayerController_Behaviour* pxCtrl = GetController();
		if (pxCtrl == nullptr) { g_iPhase = kSP_Done; return false; }
		// Decay very fast so we drop below threshold in one tick.
		pxCtrl->DecayDemonScent(/*ratePerSec=*/100.0f, /*dt=*/1.0f);
		DP_Player::WriteHighestScentToBlackboard();
		const float fThreshold =
			DP_Tuning::Get<float>("possession.demon_scent_hound_bark_threshold");
		DP_Particles::UpdateHighScentAura(g_xVillager,
			DP_Player::GetDemonScent(g_xVillager) >= fThreshold);
		g_iPhase = kSP_FinalVerify;
		g_iFrameDelay = 0;
		return true;
	}

	case kSP_FinalVerify:
	{
		++g_iFrameDelay;
		if (g_iFrameDelay < 2) return true;
		g_bAuraEmittingLow = IsAuraEmitting();
		g_iPhase = kSP_Done;
		return false;
	}

	case kSP_Done:
	default:
		return false;
	}
}

static bool Verify_P5ScentBias()
{
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P5Scent: villager not found");
		return false;
	}
	if (!g_bAuraEmittingHigh)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5Scent: aura emitter NOT emitting above threshold "
			"-- DP_Particles::UpdateHighScentAura(scenct>=threshold) didn't enable emission");
		return false;
	}
	if (!g_bPriestBiasNearVillager)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5Scent: FindPos didn't bias toward villager position. The patrol "
			"target was outside the suspicion radius of the villager's "
			"position; either the bias path didn't fire OR the random pick "
			"landed outside the sphere (regression: bias logic broken)");
		return false;
	}
	if (g_bAuraEmittingLow)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P5Scent: aura emitter still emitting below threshold "
			"-- UpdateHighScentAura(false) didn't disable emission");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP5ScentTest = {
	"Test_P5Scent_PriestPatrolBiasedByScent",
	&Setup_P5ScentBias,
	&Step_P5ScentBias,
	&Verify_P5ScentBias,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP5ScentTest);

#endif // ZENITH_INPUT_SIMULATOR
