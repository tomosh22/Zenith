#include "Core/Zenith_Engine.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

// ============================================================================
// NavMesh Agent Tests
// ============================================================================
ZENITH_TEST(AI, NavAgentSetDestination) { Zenith_UnitTests::TestNavAgentSetDestination(); }
void Zenith_UnitTests::TestNavAgentSetDestination(){
	Zenith_NavMesh xNavMesh;

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.ComputeSpatialData();
	xNavMesh.BuildSpatialGrid();

	// Create entity with transform for position
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "NavAgent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));

	Zenith_NavMeshAgent xAgent;
	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetDestination(Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));

	// Update once to trigger pathfinding
	xAgent.Update(0.016f, xEntity.GetEntityID());

	ZENITH_ASSERT_TRUE(xAgent.HasPath(), "Agent should have path after SetDestination and Update");

}

ZENITH_TEST(AI, NavAgentMovement) { Zenith_UnitTests::TestNavAgentMovement(); }

void Zenith_UnitTests::TestNavAgentMovement(){
	Zenith_NavMesh xNavMesh;

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.ComputeSpatialData();
	xNavMesh.BuildSpatialGrid();

	// Create entity with transform for position
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "NavAgent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

	Zenith_Maths::Vector3 xStartPos(1.0f, 0.0f, 1.0f);
	xTransform.SetPosition(xStartPos);

	Zenith_NavMeshAgent xAgent;
	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetMoveSpeed(5.0f);
	xAgent.SetDestination(Zenith_Maths::Vector3(5.0f, 0.0f, 1.0f));

	// Update for 0.5 seconds
	xAgent.Update(0.5f, xEntity.GetEntityID());

	Zenith_Maths::Vector3 xNewPos;
	xTransform.GetPosition(xNewPos);
	ZENITH_ASSERT_GT(xNewPos.x, xStartPos.x, "Agent should have moved");

}

ZENITH_TEST(AI, NavAgentArrival) { Zenith_UnitTests::TestNavAgentArrival(); }

void Zenith_UnitTests::TestNavAgentArrival(){
	Zenith_NavMesh xNavMesh;

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.ComputeSpatialData();
	xNavMesh.BuildSpatialGrid();

	// Create entity with transform for position
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "NavAgent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));

	Zenith_NavMeshAgent xAgent;
	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetMoveSpeed(10.0f);
	xAgent.SetStoppingDistance(0.5f);
	xAgent.SetDestination(Zenith_Maths::Vector3(2.0f, 0.0f, 1.0f));

	// Update for enough time to reach destination
	for (int i = 0; i < 10; ++i)
	{
		xAgent.Update(0.1f, xEntity.GetEntityID());
	}

	ZENITH_ASSERT_TRUE(xAgent.HasReachedDestination(), "Agent should have reached destination");

}

ZENITH_TEST(AI, NavAgentStop) { Zenith_UnitTests::TestNavAgentStop(); }

void Zenith_UnitTests::TestNavAgentStop(){
	Zenith_NavMesh xNavMesh;

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.ComputeSpatialData();
	xNavMesh.BuildSpatialGrid();

	// Create entity with transform for position
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "NavAgent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));

	Zenith_NavMeshAgent xAgent;
	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetDestination(Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));

	// Update once to trigger pathfinding
	xAgent.Update(0.016f, xEntity.GetEntityID());

	ZENITH_ASSERT_TRUE(xAgent.HasPath(), "Should have path");

	xAgent.Stop();

	ZENITH_ASSERT_FALSE(xAgent.HasPath(), "Should not have path after stop");

}

ZENITH_TEST(AI, NavAgentSpeedSettings) { Zenith_UnitTests::TestNavAgentSpeedSettings(); }

void Zenith_UnitTests::TestNavAgentSpeedSettings(){
	Zenith_NavMeshAgent xAgent;

	xAgent.SetMoveSpeed(7.5f);
	ZENITH_ASSERT_EQ(xAgent.GetMoveSpeed(), 7.5f, "Move speed should be set");

	xAgent.SetTurnSpeed(180.0f);
	ZENITH_ASSERT_EQ(xAgent.GetTurnSpeed(), 180.0f, "Turn speed should be set");

	xAgent.SetStoppingDistance(1.0f);
	ZENITH_ASSERT_EQ(xAgent.GetStoppingDistance(), 1.0f, "Stopping distance should be set");

}

ZENITH_TEST(AI, NavAgentRemainingDistanceBounds) { Zenith_UnitTests::TestNavAgentRemainingDistanceBounds(); }

void Zenith_UnitTests::TestNavAgentRemainingDistanceBounds(){
	// Test that GetRemainingDistance handles edge cases without crashing
	// This verifies the bounds check fix in NavMeshAgent

	Zenith_NavMeshAgent xAgent;

	// Without a path, remaining distance should be 0
	float fDist = xAgent.GetRemainingDistance();
	ZENITH_ASSERT_EQ(fDist, 0.0f, "Remaining distance should be 0 without path");

	// Create a simple navmesh and set destination
	Zenith_NavMesh xNavMesh;
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.ComputeSpatialData();
	xNavMesh.BuildSpatialGrid();

	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetDestination(Zenith_Maths::Vector3(5.0f, 0.0f, 5.0f));

	// Create entity for transform
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "Agent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));

	// Update to compute path
	xAgent.Update(0.016f, xEntity.GetEntityID());

	// Now GetRemainingDistance should work without crashing
	fDist = xAgent.GetRemainingDistance();
	ZENITH_ASSERT_GE(fDist, 0.0f, "Remaining distance should be non-negative");

	// After reaching destination, remaining distance should be 0
	xAgent.Stop();
	fDist = xAgent.GetRemainingDistance();
	ZENITH_ASSERT_EQ(fDist, 0.0f, "Remaining distance should be 0 after stop");

}

// Regression: an agent FACING the -Z hemisphere turns toward its travel direction.
// The current heading used to be read via glm::eulerAngles(quat).y, which collapses for
// a 180-deg facing (decoding to yaw 0) and re-encodes a corrupted pitch=pi/roll=pi
// quaternion; the fix reads the heading from quat*+Z and writes a pure-yaw quat.
// The destination is deliberately OFF the -X axis (it has a -Z component): on the -X
// symmetry axis the old + new code converge to the SAME forward, so the test must steer
// asymmetrically to discriminate. The old decode under-turns in Z (forward.z ~ -0.06)
// while the fix tracks the travel direction (forward.z ~ -0.3); we assert forward.z well
// below the old value so this FAILS on the pre-fix code.
ZENITH_TEST(AI, NavAgentFacingNegativeZTurnsTowardTravel)
{
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

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "NavAgentNegZ");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(8.0f, 0.0f, 8.0f));
	// Face -Z (180 deg about Y) — the hemisphere the old eulerAngles().y decode mangled.
	xTransform.SetRotation(glm::angleAxis(3.14159265f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));

	Zenith_NavMeshAgent xAgent;
	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetMoveSpeed(3.0f);
	xAgent.SetTurnSpeed(360.0f);
	// Travel toward (-X, -Z): an ASYMMETRIC heading (move dir ~(-0.95, 0, -0.32)) so the
	// fixed code (tracking travel) and the old code (under-turning in Z) diverge.
	xAgent.SetDestination(Zenith_Maths::Vector3(2.0f, 0.0f, 6.0f));

	for (int i = 0; i < 60; ++i)
		xAgent.Update(0.02f, xEntity.GetEntityID());

	Zenith_Maths::Quat xRot;
	xTransform.GetRotation(xRot);
	const Zenith_Maths::Vector3 xForward = xRot * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	// Turned toward the travel direction in BOTH axes (the -Z component is the
	// discriminator: the old decode leaves forward.z ~ -0.06, the fix reaches ~ -0.32),
	// and stayed upright (pure-yaw writeback: no spurious pitch/roll off the XZ plane).
	ZENITH_ASSERT_LT(xForward.x, -0.7f,  "Agent should turn toward its -X travel component");
	ZENITH_ASSERT_LT(xForward.z, -0.15f, "Agent must track the -Z travel component (old eulerAngles decode fails this)");
	ZENITH_ASSERT_LT(xForward.y,  0.1f,  "Nav rotation must stay pure-yaw (upright)");
	ZENITH_ASSERT_GT(xForward.y, -0.1f,  "Nav rotation must stay pure-yaw (upright)");
}

