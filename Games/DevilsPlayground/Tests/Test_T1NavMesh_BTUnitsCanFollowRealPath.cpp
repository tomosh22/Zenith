#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Navigation/Zenith_Pathfinding.h"

#include <cstdio>
#include <cmath>

// ============================================================================
// Test_T1NavMesh_BTUnitsCanFollowRealPath (engine integration smoke)
//
// Proves the contract that the MVP-1.2.2 wiring depends on: given a fresh
// scene with a simple floor collider, the navmesh generator emits a
// connected polygon graph, `FindPath` returns SUCCESS for two close points
// on the floor, AND a `Zenith_NavMeshAgent` configured with that navmesh
// actually MOVES its registered transform toward the destination over time.
//
// This is the integration test PriestPursuit_Test ought to be once
// GameLevel's collider authoring stabilises. By using a fresh scratch
// scene we sidestep GameLevel's complex floor system + isolate the
// engine surface from level-data noise.
//
// Pass criteria:
//   * GenerateFromScene returns non-null navmesh with > 100 polygons.
//   * FindPath(start, end) returns SUCCESS with path length > 0.
//   * NavMeshAgent::Update advances the agent's transform toward the
//     destination by at least 1m over 60 frames at fixed dt (5 m/s).
// ============================================================================

namespace
{
	enum Phase : int { kBT_Start, kBT_BuildScene, kBT_Generate, kBT_Path,
	                   kBT_RecordInitial, kBT_RunFrames, kBT_RecordFinal,
	                   kBT_Verify, kBT_Done };

	int                   g_iPhase = kBT_Start;
	Zenith_Scene          g_xScene;
	Zenith_NavMesh*       g_pxNavMesh = nullptr;
	Zenith_NavMeshAgent   g_xAgent;
	Zenith_Entity         g_xAgentEntity;
	Zenith_Maths::Vector3 g_xStart;
	Zenith_Maths::Vector3 g_xDestination;
	float                 g_fInitialDist = 0.0f;
	float                 g_fFinalDist = 0.0f;
	int                   g_iRunFrames = 0;
	bool                  g_bGenerateOK = false;
	bool                  g_bPathOK = false;
	bool                  g_bMovedTowardTarget = false;
	bool                  g_bPassed = false;
}

static void Setup_T1BTUnitsCanFollowRealPath()
{
	g_iPhase = kBT_Start;
	g_xScene = Zenith_Scene();
	delete g_pxNavMesh;
	g_pxNavMesh = nullptr;
	g_xAgent = Zenith_NavMeshAgent();
	g_xAgentEntity = Zenith_Entity();
	g_xStart = Zenith_Maths::Vector3(0.0f);
	g_xDestination = Zenith_Maths::Vector3(0.0f);
	g_fInitialDist = 0.0f;
	g_fFinalDist = 0.0f;
	g_iRunFrames = 0;
	g_bGenerateOK = false;
	g_bPathOK = false;
	g_bMovedTowardTarget = false;
	g_bPassed = false;
}

static bool Step_T1BTUnitsCanFollowRealPath(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	case kBT_Start:
		g_xScene = g_xEngine.Scenes().LoadScene("BTUnitsCanFollowRealPath", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(g_xScene);
		g_iPhase = kBT_BuildScene;
		return true;

	case kBT_BuildScene:
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(g_xScene);
		if (pxScene == nullptr) { g_iPhase = kBT_Done; return false; }

		// Floor: 20m x 0.2m x 20m centred at origin. Scale = full box size.
		Zenith_Entity xFloor = g_xEngine.Scenes().CreateEntity(pxScene, "Floor");
		Zenith_TransformComponent& xFloorT =
			xFloor.GetComponent<Zenith_TransformComponent>();
		xFloorT.SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xFloorT.SetScale(Zenith_Maths::Vector3(20.0f, 0.2f, 20.0f));
		Zenith_ColliderComponent& xFloorCol =
			xFloor.AddComponent<Zenith_ColliderComponent>();
		xFloorCol.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);

		// Agent: a barebones entity with a TransformComponent we'll move.
		g_xAgentEntity = g_xEngine.Scenes().CreateEntity(pxScene, "TestAgent");
		Zenith_TransformComponent& xAgentT =
			g_xAgentEntity.GetComponent<Zenith_TransformComponent>();
		g_xStart       = Zenith_Maths::Vector3(-5.0f, 0.1f, 0.0f);
		g_xDestination = Zenith_Maths::Vector3( 5.0f, 0.1f, 0.0f);
		xAgentT.SetPosition(g_xStart);
		g_iPhase = kBT_Generate;
		return true;
	}

	case kBT_Generate:
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(g_xScene);
		if (pxScene == nullptr) { g_iPhase = kBT_Done; return false; }
		NavMeshGenerationConfig xCfg{};
		xCfg.m_fAgentRadius = 0.2f;
		g_pxNavMesh = Zenith_NavMeshGenerator::GenerateFromScene(*pxScene, xCfg);
		g_bGenerateOK = (g_pxNavMesh != nullptr) && (g_pxNavMesh->GetPolygonCount() > 100);
		std::printf("[T1BTUnits] GenerateFromScene: %s (polys=%u)\n",
			g_bGenerateOK ? "OK" : "FAIL",
			g_pxNavMesh ? g_pxNavMesh->GetPolygonCount() : 0u);
		std::fflush(stdout);
		g_iPhase = g_bGenerateOK ? kBT_Path : kBT_Verify;
		return true;
	}

	case kBT_Path:
	{
		// Verify FindPath works between the two endpoints.
		const Zenith_PathResult xResult =
			Zenith_Pathfinding::FindPath(*g_pxNavMesh, g_xStart, g_xDestination);
		g_bPathOK = (xResult.m_eStatus == Zenith_PathResult::Status::SUCCESS)
		         && (xResult.m_axWaypoints.GetSize() >= 2);
		std::printf("[T1BTUnits] FindPath: status=%d waypoints=%u length=%.3f\n",
			(int)xResult.m_eStatus, xResult.m_axWaypoints.GetSize(),
			xResult.m_fTotalDistance);
		std::fflush(stdout);

		// Configure the agent with the navmesh + destination.
		g_xAgent.SetNavMesh(g_pxNavMesh);
		g_xAgent.SetMoveSpeed(5.0f);
		g_xAgent.SetDestination(g_xDestination);
		g_iPhase = kBT_RecordInitial;
		return true;
	}

	case kBT_RecordInitial:
	{
		Zenith_Maths::Vector3 xPos;
		g_xAgentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		const float fDx = xPos.x - g_xDestination.x;
		const float fDz = xPos.z - g_xDestination.z;
		g_fInitialDist = std::sqrt(fDx * fDx + fDz * fDz);
		g_iRunFrames = 0;
		g_iPhase = kBT_RunFrames;
		return true;
	}

	case kBT_RunFrames:
	{
		// Tick the NavMeshAgent every frame -- this is what
		// AIAgentComponent::OnUpdate does in production.
		Zenith_TransformComponent& xT =
			g_xAgentEntity.GetComponent<Zenith_TransformComponent>();
		g_xAgent.Update(0.01666f, xT);
		++g_iRunFrames;
		if (g_iRunFrames >= 60)
		{
			g_iPhase = kBT_RecordFinal;
		}
		return true;
	}

	case kBT_RecordFinal:
	{
		Zenith_Maths::Vector3 xPos;
		g_xAgentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		const float fDx = xPos.x - g_xDestination.x;
		const float fDz = xPos.z - g_xDestination.z;
		g_fFinalDist = std::sqrt(fDx * fDx + fDz * fDz);
		std::printf("[T1BTUnits] dist initial=%.3f final=%.3f delta=%.3f\n",
			g_fInitialDist, g_fFinalDist, g_fInitialDist - g_fFinalDist);
		std::fflush(stdout);
		// Required forward progress: 1m in 60 frames at 5 m/s gives ~5m of
		// theoretical travel. 1m is a conservative pass threshold that
		// absorbs path smoothing + acceleration ramp-up.
		g_bMovedTowardTarget = (g_fInitialDist - g_fFinalDist) >= 1.0f;
		g_iPhase = kBT_Verify;
		return true;
	}

	case kBT_Verify:
		g_bPassed = g_bGenerateOK && g_bPathOK && g_bMovedTowardTarget;
		std::printf("[T1BTUnits] verify: gen=%d path=%d moved=%d passed=%d\n",
			(int)g_bGenerateOK, (int)g_bPathOK, (int)g_bMovedTowardTarget, (int)g_bPassed);
		std::fflush(stdout);
		delete g_pxNavMesh;
		g_pxNavMesh = nullptr;
		g_iPhase = kBT_Done;
		return false;

	case kBT_Done:
	default:
		return false;
	}
}

static bool Verify_T1BTUnitsCanFollowRealPath()
{
	return g_bPassed;
}

static const Zenith_AutomatedTest g_xT1BTUnitsTest = {
	"Test_T1NavMesh_BTUnitsCanFollowRealPath",
	&Setup_T1BTUnitsCanFollowRealPath,
	&Step_T1BTUnitsCanFollowRealPath,
	&Verify_T1BTUnitsCanFollowRealPath,
	120,
	false // m_bRequiresGraphics: pure compute (collider + navmesh + agent)
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xT1BTUnitsTest);

#endif // ZENITH_INPUT_SIMULATOR
