#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"

// ============================================================================
// PriestPursuit_Test (NavMesh + BT + perception bridge end-to-end)
//
// Loads GameLevel, possesses an authored villager via DP_Player, runs the
// game-loop for ~120 frames at fixed-dt, and asserts that the priest closed
// the horizontal distance to the possessed villager.
//
// This proves three things at once:
//   1. The perception bridge writes BB.TargetWithDevil for the possessed
//      villager (priest.OnUpdate's BridgePerceptionToBlackboard).
//   2. The BT's pursue branch fires (Selector → Sequence → MoveToEntity).
//   3. The NavMeshAgent + AIAgentComponent::OnUpdate chain actually moves
//      the priest's transform towards the target.
//
// Tolerance is generous — 0.5m closer is enough to call it a pass (the
// priest moves ≈ 5 m/s × 2s ≈ 10m unobstructed, but the synthetic flat
// navmesh + path-smoothing rounding can shave that down).
// ============================================================================

namespace
{
	enum Phase : int { kPP_Start, kPP_WaitScene, kPP_Possess, kPP_RecordInitial,
	                   kPP_RunFrames, kPP_RecordFinal, kPP_Done };

	int             g_iPPPhase     = kPP_Start;
	Zenith_EntityID g_xPriest;
	Zenith_EntityID g_xVillager;
	float           g_fInitialDist = 0.0f;
	float           g_fFinalDist   = 0.0f;
	int             g_iRunFrames   = 0;

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
}

static void Setup_PriestPursuit()
{
	g_iPPPhase     = kPP_Start;
	g_xPriest      = INVALID_ENTITY_ID;
	g_xVillager    = INVALID_ENTITY_ID;
	g_fInitialDist = 0.0f;
	g_fFinalDist   = 0.0f;
	g_iRunFrames   = 0;
}

static bool Step_PriestPursuit(int iFrame)
{
	switch (g_iPPPhase)
	{
	case kPP_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPPPhase = kPP_WaitScene;
		return true;

	case kPP_WaitScene:
	{
		// Wait until the priest and at least one villager have been spawned
		// and run their OnAwake/OnStart.
		Zenith_EntityID xFoundPriest;
		Zenith_EntityID xFoundVillager;
		float fClosestDist = 1e30f;

		// Pick the villager closest to the priest so the pursue branch has a
		// chance to actually close the gap within 120 frames at 5 m/s.
		Zenith_Maths::Vector3 xPriestPos(0.0f);
		bool bGotPriestPos = false;

		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundPriest, &xPriestPos, &bGotPriestPos](Zenith_EntityID xId, Priest_Behaviour&)
			{
				xFoundPriest = xId;
				bGotPriestPos = TryGetEntityPos(xId, xPriestPos);
			});

		if (xFoundPriest.IsValid() && bGotPriestPos)
		{
			DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
				[&xFoundVillager, &fClosestDist, &xPriestPos](Zenith_EntityID xId, DPVillager_Behaviour&)
				{
					Zenith_Maths::Vector3 xVPos;
					if (!TryGetEntityPos(xId, xVPos)) return;
					float fDist = HorizontalDistance(xPriestPos, xVPos);
					if (fDist < fClosestDist)
					{
						fClosestDist = fDist;
						xFoundVillager = xId;
					}
				});
		}

		if (xFoundPriest.IsValid() && xFoundVillager.IsValid())
		{
			g_xPriest   = xFoundPriest;
			g_xVillager = xFoundVillager;
			g_iPPPhase  = kPP_Possess;
		}
		else if (iFrame > 60)
		{
			g_iPPPhase = kPP_Done;
		}
		return true;
	}

	case kPP_Possess:
		// Possess the chosen villager. The villager will then satisfy
		// Priest_Behaviour::IsPossessedVillager(), and the priest's BB-bridge
		// will write its EntityID into BB.TargetWithDevil — driving the
		// pursue branch of the BT.
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPPPhase = kPP_RecordInitial;
		return true;

	case kPP_RecordInitial:
	{
		Zenith_Maths::Vector3 xPP, xVP;
		if (!TryGetEntityPos(g_xPriest, xPP)) { g_iPPPhase = kPP_Done; return false; }
		if (!TryGetEntityPos(g_xVillager, xVP)) { g_iPPPhase = kPP_Done; return false; }
		g_fInitialDist = HorizontalDistance(xPP, xVP);
		g_iRunFrames   = 0;
		g_iPPPhase     = kPP_RunFrames;
		return true;
	}

	case kPP_RunFrames:
		++g_iRunFrames;
		if (g_iRunFrames >= 120)
		{
			g_iPPPhase = kPP_RecordFinal;
		}
		return true;

	case kPP_RecordFinal:
	{
		Zenith_Maths::Vector3 xPP, xVP;
		if (!TryGetEntityPos(g_xPriest, xPP)) { g_iPPPhase = kPP_Done; return false; }
		if (!TryGetEntityPos(g_xVillager, xVP)) { g_iPPPhase = kPP_Done; return false; }
		g_fFinalDist = HorizontalDistance(xPP, xVP);

		// Diagnostic dump — find the priest's blackboard and report what got wired.
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(g_xPriest);
		if (pxScene != nullptr)
		{
			Zenith_Entity xEnt = pxScene->TryGetEntity(g_xPriest);
			if (xEnt.IsValid() && xEnt.HasComponent<Zenith_AIAgentComponent>())
			{
				Zenith_AIAgentComponent& xAg = xEnt.GetComponent<Zenith_AIAgentComponent>();
				const Zenith_Blackboard& xBB = xAg.GetBlackboard();
				Zenith_NavMeshAgent* pxNav = xAg.GetNavMeshAgent();
				const Zenith_EntityID xTgt = xBB.GetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
				const Zenith_Maths::Vector3 xPatrol = xBB.GetVector3(DP_AI::BB_KEY_PATROL_TARGET);
				const Zenith_Maths::Vector3 xVel = pxNav ? pxNav->GetVelocity() : Zenith_Maths::Vector3(0.0f);
				const bool bHasPath = pxNav ? pxNav->HasPath() : false;
				Zenith_Log(LOG_CATEGORY_AI,
					"PriestPursuit: navAgent=%p target=(%u/%u) priestPos=(%.1f,%.1f,%.1f) villagerPos=(%.1f,%.1f,%.1f) hasPath=%d vel=(%.2f,%.2f,%.2f) patrol=(%.1f,%.1f,%.1f)",
					(void*)pxNav,
					xTgt.m_uIndex, xTgt.m_uGeneration,
					xPP.x, xPP.y, xPP.z,
					xVP.x, xVP.y, xVP.z,
					bHasPath ? 1 : 0,
					xVel.x, xVel.y, xVel.z,
					xPatrol.x, xPatrol.y, xPatrol.z);
			}
			else
			{
				Zenith_Log(LOG_CATEGORY_AI, "PriestPursuit: priest has no AIAgent component");
			}
		}

		g_iPPPhase   = kPP_Done;
		return false;
	}

	case kPP_Done:
	default:
		return false;
	}
}

static bool Verify_PriestPursuit()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "PriestPursuit_Test: priest entity not found");
		return false;
	}
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "PriestPursuit_Test: villager entity not found");
		return false;
	}

	// Initial distance must be positive — sanity check that we found two
	// distinct entities with valid transforms.
	if (g_fInitialDist <= 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI, "PriestPursuit_Test: initial distance was zero/negative");
		return false;
	}

	const float fProgress = g_fInitialDist - g_fFinalDist;
	Zenith_Log(LOG_CATEGORY_AI, "PriestPursuit_Test: initial=%.2f final=%.2f progress=%.2f",
		g_fInitialDist, g_fFinalDist, fProgress);

	// The priest must have meaningfully closed the gap. We require at least
	// 0.5m of progress; a healthy run sees ~5+ metres at 5m/s nav speed.
	const float fMinProgressMetres = 0.5f;
	return fProgress >= fMinProgressMetres;
}

static const Zenith_AutomatedTest g_xPriestPursuitTest = {
	"PriestPursuit_Test",
	&Setup_PriestPursuit,
	&Step_PriestPursuit,
	&Verify_PriestPursuit,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPriestPursuitTest);

#endif // ZENITH_INPUT_SIMULATOR
