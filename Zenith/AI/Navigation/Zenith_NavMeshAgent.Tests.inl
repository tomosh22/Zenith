#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
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
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xEntity(pxSceneData, "NavAgent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));

	Zenith_NavMeshAgent xAgent;
	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetDestination(Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));

	// Update once to trigger pathfinding
	xAgent.Update(0.016f, xTransform);

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
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xEntity(pxSceneData, "NavAgent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

	Zenith_Maths::Vector3 xStartPos(1.0f, 0.0f, 1.0f);
	xTransform.SetPosition(xStartPos);

	Zenith_NavMeshAgent xAgent;
	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetMoveSpeed(5.0f);
	xAgent.SetDestination(Zenith_Maths::Vector3(5.0f, 0.0f, 1.0f));

	// Update for 0.5 seconds
	xAgent.Update(0.5f, xTransform);

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
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xEntity(pxSceneData, "NavAgent");
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
		xAgent.Update(0.1f, xTransform);
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
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xEntity(pxSceneData, "NavAgent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));

	Zenith_NavMeshAgent xAgent;
	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetDestination(Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));

	// Update once to trigger pathfinding
	xAgent.Update(0.016f, xTransform);

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
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xEntity(pxSceneData, "Agent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));

	// Update to compute path
	xAgent.Update(0.016f, xTransform);

	// Now GetRemainingDistance should work without crashing
	fDist = xAgent.GetRemainingDistance();
	ZENITH_ASSERT_GE(fDist, 0.0f, "Remaining distance should be non-negative");

	// After reaching destination, remaining distance should be 0
	xAgent.Stop();
	fDist = xAgent.GetRemainingDistance();
	ZENITH_ASSERT_EQ(fDist, 0.0f, "Remaining distance should be 0 after stop");

}

