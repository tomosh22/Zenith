#include "Zenith.h"

// =============================================================================
// SentinelAI — leaf-proof executable for the ZenithAI extraction.
//
// Links ONLY zenithai.lib + zenithphysics.lib + zenithecs.lib + zenithbase.lib
// (see Build/Sharpmake_SentinelAI.cs) — no aggregate engine lib, no Flux, no
// concrete component. The AI core reaches the engine only through the
// Zenith_AIWorldHooks function-pointer seam, which is null here (safe no-ops), so
// a green link + run proves ZenithAI is a self-contained leaf over
// ZenithBase + ZenithECS + ZenithPhysics (+ the L0 platform/infra floor shimmed in
// sentinel_platform.cpp). A new engine-coupling leak (g_xEngine / Flux / a concrete
// component) would surface here as an unresolved external at link time, or in the
// dumpbin UNDEF scan that backs this proof.
// =============================================================================

#include "ZenithECS/Zenith_SceneSystem.h"
#include "Physics/Zenith_Physics.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Navigation/Zenith_Pathfinding.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_TacticalPoint.h"
#include "AI/Zenith_AI.h"
#include "Collections/Zenith_Vector.h"

static int s_iFailures = 0;

static void Expect(bool bCond, const char* szWhat)
{
	if (!bCond)
	{
		++s_iFailures;
		Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelAI FAIL: %s", szWhat);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelAI ok:   %s", szWhat);
	}
}

int main()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"SentinelAI: AI leaf-proof starting (zenithai + zenithphysics + zenithecs + zenithbase only)");

	// Some AI paths resolve the scene system via Zenith_SceneSystem::Get(); construct it.
	Zenith_SceneSystem* pxScenes = new Zenith_SceneSystem();
	Expect(pxScenes != nullptr, "Zenith_SceneSystem constructed");

	// AI raycasts reach the sibling Physics leaf via Zenith_Physics::Get(); bring it
	// up so that edge is live + linkable.
	Zenith_Physics xPhysics;
	xPhysics.Initialise();
	Expect(xPhysics.HasActiveSimulation(), "Physics initialised (AI->Physics sibling-leaf edge)");

	// Build a navmesh by hand (deterministic) and pathfind on it — links NavMesh +
	// Pathfinding without needing the engine.
	Zenith_NavMesh xNavMesh;
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));
	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0); axIndices.PushBack(1); axIndices.PushBack(2); axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.ComputeSpatialData();
	xNavMesh.BuildSpatialGrid();
	Expect(xNavMesh.GetPolygonCount() == 1, "NavMesh built one polygon");

	Zenith_PathResult xPath = Zenith_Pathfinding::FindPath(xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f), Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));
	Expect(xPath.m_eStatus == Zenith_PathResult::Status::SUCCESS, "FindPath succeeded on the single-polygon navmesh");

	// NavMeshGenerator (pure leaf) from raw geometry — exercise its link surface.
	Zenith_Vector<Zenith_Maths::Vector3> axVerts;
	axVerts.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	axVerts.PushBack(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	axVerts.PushBack(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
	axVerts.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));
	Zenith_Vector<uint32_t> axGenIdx;
	axGenIdx.PushBack(0); axGenIdx.PushBack(1); axGenIdx.PushBack(2);
	axGenIdx.PushBack(0); axGenIdx.PushBack(2); axGenIdx.PushBack(3);
	NavMeshGenerationConfig xCfg;
	Zenith_NavMesh* pxGenerated = Zenith_NavMeshGenerator::GenerateFromGeometry(axVerts, axGenIdx, xCfg);
	Expect(true, "GenerateFromGeometry ran (pure leaf generation path)");
	delete pxGenerated;  // null-safe (generation may legitimately produce none for tiny input)

	// Blackboard round-trip.
	Zenith_Blackboard xBB;
	xBB.SetFloat("k", 1.5f);
	Expect(xBB.GetFloat("k", 0.0f) == 1.5f, "Blackboard round-trips a float");

	// NavMeshAgent config (its Update uses the null world hooks -> no-op).
	Zenith_NavMeshAgent xAgent;
	xAgent.SetMoveSpeed(4.0f);
	Expect(xAgent.GetMoveSpeed() == 4.0f, "NavMeshAgent config round-trips");

	// AI managers tick with no registered agents — links Perception / Squad /
	// TacticalPoint. With null hooks the per-agent loops are empty, so nothing
	// reaches the engine. All three must be initialised; Zenith_AI::Update ticks
	// perception -> squad -> tactical-point.
	Zenith_PerceptionSystem::Initialise();
	Zenith_SquadManager::Initialise();
	Zenith_TacticalPointSystem::Initialise();
	Zenith_AI::Update(1.0f / 60.0f);
	Expect(true, "Zenith_AI::Update ran with no registered agents");

	Zenith_TacticalPointSystem::Shutdown();
	Zenith_SquadManager::Shutdown();
	xPhysics.Shutdown();
	delete pxScenes;
	pxScenes = nullptr;

	if (s_iFailures == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelAI: PASS (AI leaf links + runs with no engine externals)");
		return 0;
	}

	Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelAI: FAIL (%d expectation(s) failed)", s_iFailures);
	return 1;
}
