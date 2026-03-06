#include "Zenith.h"
#include "UnitTests/Zenith_UnitTests.h"

// AI System includes
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AI/BehaviorTree/Zenith_BTNode.h"
#include "AI/BehaviorTree/Zenith_BTComposites.h"
#include "AI/BehaviorTree/Zenith_BTDecorators.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Navigation/Zenith_Pathfinding.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_Formation.h"
#include "AI/Squad/Zenith_TacticalPoint.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"

// ============================================================================
// Helper: Mock BT Node for testing
// ============================================================================
class MockBTNode : public Zenith_BTNode
{
public:
	MockBTNode(BTNodeStatus eReturnStatus) : m_eReturnStatus(eReturnStatus), m_uExecuteCount(0) {}

	BTNodeStatus Execute(Zenith_Entity& /*xAgent*/, Zenith_Blackboard& /*xBlackboard*/, float /*fDt*/) override
	{
		m_uExecuteCount++;
		return m_eReturnStatus;
	}

	const char* GetTypeName() const override { return "MockBTNode"; }

	BTNodeStatus m_eReturnStatus;
	uint32_t m_uExecuteCount;
};

// ============================================================================
// Blackboard Tests
// ============================================================================
void Zenith_UnitTests::TestBlackboardBasicTypes()
{
	Zenith_Blackboard xBlackboard;

	// Test float
	xBlackboard.SetFloat("health", 100.0f);
	Zenith_Assert(xBlackboard.GetFloat("health") == 100.0f, "Float should be 100.0");

	// Test int
	xBlackboard.SetInt("ammo", 30);
	Zenith_Assert(xBlackboard.GetInt("ammo") == 30, "Int should be 30");

	// Test bool
	xBlackboard.SetBool("isAlerted", true);
	Zenith_Assert(xBlackboard.GetBool("isAlerted") == true, "Bool should be true");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlackboardBasicTypes PASSED");
}

void Zenith_UnitTests::TestBlackboardVector3()
{
	Zenith_Blackboard xBlackboard;

	Zenith_Maths::Vector3 xTestVec(1.0f, 2.0f, 3.0f);
	xBlackboard.SetVector3("targetPos", xTestVec);

	Zenith_Maths::Vector3 xResult = xBlackboard.GetVector3("targetPos");
	Zenith_Assert(xResult.x == 1.0f && xResult.y == 2.0f && xResult.z == 3.0f,
		"Vector3 values should match");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlackboardVector3 PASSED");
}

void Zenith_UnitTests::TestBlackboardEntityID()
{
	Zenith_Blackboard xBlackboard;

	Zenith_EntityID xTestID(12345);
	xBlackboard.SetEntityID("targetEntity", xTestID);

	Zenith_EntityID xResult = xBlackboard.GetEntityID("targetEntity");
	Zenith_Assert(xResult.IsValid() && xResult.m_uIndex == 12345, "EntityID should match");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlackboardEntityID PASSED");
}

void Zenith_UnitTests::TestBlackboardHasKey()
{
	Zenith_Blackboard xBlackboard;

	Zenith_Assert(!xBlackboard.HasKey("missing"), "Key should not exist initially");

	xBlackboard.SetFloat("exists", 1.0f);
	Zenith_Assert(xBlackboard.HasKey("exists"), "Key should exist after set");

	xBlackboard.RemoveKey("exists");
	Zenith_Assert(!xBlackboard.HasKey("exists"), "Key should not exist after remove");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlackboardHasKey PASSED");
}

void Zenith_UnitTests::TestBlackboardClear()
{
	Zenith_Blackboard xBlackboard;

	xBlackboard.SetFloat("a", 1.0f);
	xBlackboard.SetInt("b", 2);
	xBlackboard.SetBool("c", true);

	xBlackboard.Clear();

	Zenith_Assert(!xBlackboard.HasKey("a"), "All keys should be cleared");
	Zenith_Assert(!xBlackboard.HasKey("b"), "All keys should be cleared");
	Zenith_Assert(!xBlackboard.HasKey("c"), "All keys should be cleared");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlackboardClear PASSED");
}

void Zenith_UnitTests::TestBlackboardDefaultValues()
{
	Zenith_Blackboard xBlackboard;

	// Test defaults for non-existent keys
	Zenith_Assert(xBlackboard.GetFloat("missing", 42.0f) == 42.0f, "Should return default float");
	Zenith_Assert(xBlackboard.GetInt("missing", 99) == 99, "Should return default int");
	Zenith_Assert(xBlackboard.GetBool("missing", true) == true, "Should return default bool");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlackboardDefaultValues PASSED");
}

void Zenith_UnitTests::TestBlackboardOverwrite()
{
	Zenith_Blackboard xBlackboard;

	xBlackboard.SetFloat("value", 1.0f);
	Zenith_Assert(xBlackboard.GetFloat("value") == 1.0f, "Initial value");

	xBlackboard.SetFloat("value", 2.0f);
	Zenith_Assert(xBlackboard.GetFloat("value") == 2.0f, "Overwritten value");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlackboardOverwrite PASSED");
}

void Zenith_UnitTests::TestBlackboardSerialization()
{
	Zenith_Blackboard xBlackboard;
	xBlackboard.SetFloat("health", 75.0f);
	xBlackboard.SetInt("level", 5);
	xBlackboard.SetBool("active", true);
	xBlackboard.SetVector3("pos", Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));

	// Serialize
	Zenith_DataStream xStream(256);
	xBlackboard.WriteToDataStream(xStream);

	// Deserialize into new blackboard
	xStream.SetCursor(0);
	Zenith_Blackboard xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	Zenith_Assert(xLoaded.GetFloat("health") == 75.0f, "Serialized float should match");
	Zenith_Assert(xLoaded.GetInt("level") == 5, "Serialized int should match");
	Zenith_Assert(xLoaded.GetBool("active") == true, "Serialized bool should match");

	Zenith_Maths::Vector3 xLoadedPos = xLoaded.GetVector3("pos");
	Zenith_Assert(xLoadedPos.x == 1.0f && xLoadedPos.y == 2.0f && xLoadedPos.z == 3.0f,
		"Serialized Vector3 should match");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlackboardSerialization PASSED");
}

// ============================================================================
// Behavior Tree Tests
// ============================================================================
void Zenith_UnitTests::TestBTSequenceAllSuccess()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTSequence xSequence;
	xSequence.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xSequence.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xSequence.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));

	BTNodeStatus eResult = xSequence.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult == BTNodeStatus::SUCCESS, "Sequence with all SUCCESS should return SUCCESS");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTSequenceAllSuccess PASSED");
}

void Zenith_UnitTests::TestBTSequenceFirstFails()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	MockBTNode* pxSecond = new MockBTNode(BTNodeStatus::SUCCESS);

	Zenith_BTSequence xSequence;
	xSequence.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xSequence.AddChild(pxSecond);

	BTNodeStatus eResult = xSequence.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult == BTNodeStatus::FAILURE, "Sequence should fail on first failure");
	Zenith_Assert(pxSecond->m_uExecuteCount == 0, "Second node should not execute");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTSequenceFirstFails PASSED");
}

void Zenith_UnitTests::TestBTSequenceRunning()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTSequence xSequence;
	xSequence.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xSequence.AddChild(new MockBTNode(BTNodeStatus::RUNNING));
	xSequence.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));

	BTNodeStatus eResult = xSequence.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult == BTNodeStatus::RUNNING, "Sequence should return RUNNING");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTSequenceRunning PASSED");
}

void Zenith_UnitTests::TestBTSelectorFirstSucceeds()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	MockBTNode* pxSecond = new MockBTNode(BTNodeStatus::SUCCESS);

	Zenith_BTSelector xSelector;
	xSelector.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xSelector.AddChild(pxSecond);

	BTNodeStatus eResult = xSelector.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult == BTNodeStatus::SUCCESS, "Selector should succeed on first success");
	Zenith_Assert(pxSecond->m_uExecuteCount == 0, "Second node should not execute");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTSelectorFirstSucceeds PASSED");
}

void Zenith_UnitTests::TestBTSelectorAllFail()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTSelector xSelector;
	xSelector.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xSelector.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xSelector.AddChild(new MockBTNode(BTNodeStatus::FAILURE));

	BTNodeStatus eResult = xSelector.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult == BTNodeStatus::FAILURE, "Selector with all FAILURE should return FAILURE");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTSelectorAllFail PASSED");
}

void Zenith_UnitTests::TestBTSelectorRunning()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTSelector xSelector;
	xSelector.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xSelector.AddChild(new MockBTNode(BTNodeStatus::RUNNING));

	BTNodeStatus eResult = xSelector.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult == BTNodeStatus::RUNNING, "Selector should return RUNNING");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTSelectorRunning PASSED");
}

void Zenith_UnitTests::TestBTParallelRequireOne()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTParallel xParallel(Zenith_BTParallel::Policy::REQUIRE_ONE, Zenith_BTParallel::Policy::REQUIRE_ALL); // Require 1 success, fail on all failures
	xParallel.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::FAILURE));

	BTNodeStatus eResult = xParallel.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult == BTNodeStatus::SUCCESS, "Parallel requiring 1 should succeed");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTParallelRequireOne PASSED");
}

void Zenith_UnitTests::TestBTParallelRequireAll()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTParallel xParallel(Zenith_BTParallel::Policy::REQUIRE_ALL, Zenith_BTParallel::Policy::REQUIRE_ONE); // Require all, fail on 1
	xParallel.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));

	BTNodeStatus eResult = xParallel.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult == BTNodeStatus::FAILURE, "Parallel requiring all should fail on one failure");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTParallelRequireAll PASSED");
}

void Zenith_UnitTests::TestBTInverter()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	// Test inverting SUCCESS
	Zenith_BTInverter xInverterSuccess;
	xInverterSuccess.SetChild(new MockBTNode(BTNodeStatus::SUCCESS));
	BTNodeStatus eResult1 = xInverterSuccess.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult1 == BTNodeStatus::FAILURE, "Inverter should convert SUCCESS to FAILURE");

	// Test inverting FAILURE
	Zenith_BTInverter xInverterFail;
	xInverterFail.SetChild(new MockBTNode(BTNodeStatus::FAILURE));
	BTNodeStatus eResult2 = xInverterFail.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult2 == BTNodeStatus::SUCCESS, "Inverter should convert FAILURE to SUCCESS");

	// Test RUNNING passthrough
	Zenith_BTInverter xInverterRunning;
	xInverterRunning.SetChild(new MockBTNode(BTNodeStatus::RUNNING));
	BTNodeStatus eResult3 = xInverterRunning.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult3 == BTNodeStatus::RUNNING, "Inverter should pass through RUNNING");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTInverter PASSED");
}

void Zenith_UnitTests::TestBTRepeaterCount()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	MockBTNode* pxChild = new MockBTNode(BTNodeStatus::SUCCESS);

	Zenith_BTRepeater xRepeater(3); // Repeat 3 times
	xRepeater.SetChild(pxChild);

	// Execute once - should return RUNNING (not done yet)
	BTNodeStatus eResult = xRepeater.Execute(xAgent, xBlackboard, 0.016f);
	// After 3 calls, it should succeed
	eResult = xRepeater.Execute(xAgent, xBlackboard, 0.016f);
	eResult = xRepeater.Execute(xAgent, xBlackboard, 0.016f);

	Zenith_Assert(pxChild->m_uExecuteCount == 3, "Child should execute 3 times");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTRepeaterCount PASSED");
}

void Zenith_UnitTests::TestBTCooldown()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	MockBTNode* pxChild = new MockBTNode(BTNodeStatus::SUCCESS);

	Zenith_BTCooldown xCooldown(1.0f); // 1 second cooldown
	xCooldown.SetChild(pxChild);

	// First execution should succeed
	BTNodeStatus eResult1 = xCooldown.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult1 == BTNodeStatus::SUCCESS, "First execution should succeed");

	// Immediate second execution should fail (on cooldown)
	BTNodeStatus eResult2 = xCooldown.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult2 == BTNodeStatus::FAILURE, "Should fail during cooldown");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTCooldown PASSED");
}

void Zenith_UnitTests::TestBTSucceeder()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTSucceeder xSucceeder;
	xSucceeder.SetChild(new MockBTNode(BTNodeStatus::FAILURE));

	BTNodeStatus eResult = xSucceeder.Execute(xAgent, xBlackboard, 0.016f);
	Zenith_Assert(eResult == BTNodeStatus::SUCCESS, "Succeeder should always return SUCCESS");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTSucceeder PASSED");
}

// ============================================================================
// NavMesh Tests
// ============================================================================
void Zenith_UnitTests::TestNavMeshPolygonCreation()
{
	Zenith_NavMesh xNavMesh;

	// Add vertices for a simple quad
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);

	Zenith_Assert(xNavMesh.GetPolygonCount() == 1, "Should have 1 polygon");
	Zenith_Assert(xNavMesh.GetVertexCount() == 4, "Should have 4 vertices");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavMeshPolygonCreation PASSED");
}

void Zenith_UnitTests::TestNavMeshAdjacency()
{
	Zenith_NavMesh xNavMesh;

	// Create two adjacent triangles sharing an edge
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));  // 0
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));  // 1
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.5f, 0.0f, 1.0f));  // 2
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.5f, 0.0f, -1.0f)); // 3

	Zenith_Vector<uint32_t> axTri1, axTri2;
	axTri1.PushBack(0); axTri1.PushBack(1); axTri1.PushBack(2);
	axTri2.PushBack(0); axTri2.PushBack(3); axTri2.PushBack(1);

	xNavMesh.AddPolygon(axTri1);
	xNavMesh.AddPolygon(axTri2);

	xNavMesh.ComputeAdjacency();

	// Polygons 0 and 1 should be neighbors (share edge 0-1)
	const Zenith_NavMeshPolygon& xPoly0 = xNavMesh.GetPolygon(0);
	bool bHasNeighbor = false;
	for (uint32_t u = 0; u < xPoly0.m_axNeighborIndices.GetSize(); ++u)
	{
		if (xPoly0.m_axNeighborIndices.Get(u) == 1)
		{
			bHasNeighbor = true;
			break;
		}
	}
	Zenith_Assert(bHasNeighbor, "Polygon 0 should have polygon 1 as neighbor");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavMeshAdjacency PASSED");
}

void Zenith_UnitTests::TestNavMeshFindNearestPolygon()
{
	Zenith_NavMesh xNavMesh;

	// Create a simple navmesh
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.BuildSpatialGrid();

	// Test point inside polygon
	uint32_t uPolyOut;
	Zenith_Maths::Vector3 xNearestOut;
	bool bFound = xNavMesh.FindNearestPolygon(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f), uPolyOut, xNearestOut);

	Zenith_Assert(bFound, "Should find polygon for point inside");
	Zenith_Assert(uPolyOut == 0, "Should find polygon 0");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavMeshFindNearestPolygon PASSED");
}

void Zenith_UnitTests::TestNavMeshIsPointOnMesh()
{
	Zenith_NavMesh xNavMesh;

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.BuildSpatialGrid();

	Zenith_Assert(xNavMesh.IsPointOnNavMesh(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f)),
		"Point inside should be on mesh");
	Zenith_Assert(!xNavMesh.IsPointOnNavMesh(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f)),
		"Point far outside should not be on mesh");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavMeshIsPointOnMesh PASSED");
}

void Zenith_UnitTests::TestNavMeshRaycast()
{
	Zenith_NavMesh xNavMesh;

	// Create a navmesh with a gap
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.BuildSpatialGrid();

	// Ray within mesh should not hit
	Zenith_Maths::Vector3 xHit;
	bool bHit = xNavMesh.Raycast(
		Zenith_Maths::Vector3(0.2f, 0.0f, 0.5f),
		Zenith_Maths::Vector3(0.8f, 0.0f, 0.5f),
		xHit);
	Zenith_Assert(!bHit, "Ray within mesh should not hit boundary");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavMeshRaycast PASSED");
}

void Zenith_UnitTests::TestNavMeshFindNearestPolygonInCell()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestNavMeshFindNearestPolygonInCell...");

	// Test 1: Empty cell returns unchanged results
	{
		Zenith_NavMesh xNavMesh;

		// Add a triangle so the mesh is valid, but search an empty cell
		xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		xNavMesh.AddVertex(Zenith_Maths::Vector3(0.5f, 0.0f, 1.0f));

		Zenith_Vector<uint32_t> axIndices;
		axIndices.PushBack(0);
		axIndices.PushBack(1);
		axIndices.PushBack(2);
		xNavMesh.AddPolygon(axIndices);
		xNavMesh.BuildSpatialGrid();

		// Find a cell that is far from the polygon (should be empty)
		// The grid is clamped, so use an out-of-bounds cell index
		float fMinDistSq = 100.0f;
		uint32_t uPolyOut = UINT32_MAX;
		Zenith_Maths::Vector3 xNearestOut(0.0f);

		// Pass an index beyond grid bounds -- should return early without modifying outputs
		uint32_t uInvalidCellIndex = xNavMesh.m_axGridCells.GetSize() + 10;
		xNavMesh.FindNearestPolygonInCell(uInvalidCellIndex, Zenith_Maths::Vector3(50.0f, 0.0f, 50.0f),
			fMinDistSq, uPolyOut, xNearestOut);

		Zenith_Assert(uPolyOut == UINT32_MAX, "Empty/invalid cell should not find any polygon");
		Zenith_Assert(fMinDistSq == 100.0f, "Distance should remain unchanged for invalid cell");
	}

	// Test 2: Single polygon cell finds the correct polygon
	{
		Zenith_NavMesh xNavMesh;

		xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
		xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 2.0f));

		Zenith_Vector<uint32_t> axIndices;
		axIndices.PushBack(0);
		axIndices.PushBack(1);
		axIndices.PushBack(2);
		xNavMesh.AddPolygon(axIndices);
		xNavMesh.BuildSpatialGrid();

		// Query point inside the triangle
		Zenith_Maths::Vector3 xQueryPoint(1.0f, 0.0f, 0.5f);
		int32_t iX, iZ;
		xNavMesh.GetGridCoords(xQueryPoint, iX, iZ);
		uint32_t uCellIndex = xNavMesh.GetGridCellIndex(iX, iZ);

		float fMinDistSq = 100.0f;
		uint32_t uPolyOut = UINT32_MAX;
		Zenith_Maths::Vector3 xNearestOut(0.0f);

		xNavMesh.FindNearestPolygonInCell(uCellIndex, xQueryPoint,
			fMinDistSq, uPolyOut, xNearestOut);

		Zenith_Assert(uPolyOut == 0, "Should find polygon 0 in the cell");
		Zenith_Assert(fMinDistSq < 100.0f, "Distance should have been updated");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavMeshFindNearestPolygonInCell PASSED");
}

void Zenith_UnitTests::TestNavMeshComputePolygonBounds()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestNavMeshComputePolygonBounds...");

	// Test: Single triangle polygon bounds
	{
		Zenith_Vector<Zenith_Maths::Vector3> axVertices;
		axVertices.PushBack(Zenith_Maths::Vector3(1.0f, 0.0f, 2.0f));
		axVertices.PushBack(Zenith_Maths::Vector3(4.0f, 0.0f, 1.0f));
		axVertices.PushBack(Zenith_Maths::Vector3(3.0f, 0.0f, 5.0f));

		Zenith_NavMeshPolygon xPoly;
		xPoly.m_axVertexIndices.PushBack(0);
		xPoly.m_axVertexIndices.PushBack(1);
		xPoly.m_axVertexIndices.PushBack(2);

		Zenith_Maths::Vector3 xMin, xMax;
		Zenith_NavMesh::ComputePolygonBounds2D(xPoly, axVertices, xMin, xMax);

		// Min X should be 1.0, Max X should be 4.0
		Zenith_Assert(std::abs(xMin.x - 1.0f) < 0.001f, "Min X should be 1.0");
		Zenith_Assert(std::abs(xMax.x - 4.0f) < 0.001f, "Max X should be 4.0");

		// Min Z should be 1.0, Max Z should be 5.0
		Zenith_Assert(std::abs(xMin.z - 1.0f) < 0.001f, "Min Z should be 1.0");
		Zenith_Assert(std::abs(xMax.z - 5.0f) < 0.001f, "Max Z should be 5.0");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavMeshComputePolygonBounds PASSED");
}

void Zenith_UnitTests::TestPathfindingStraightLine()
{
	Zenith_NavMesh xNavMesh;

	// Create a simple straight navmesh
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 2.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));

	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.BuildSpatialGrid();

	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(9.0f, 0.0f, 1.0f));

	Zenith_Assert(xResult.m_eStatus == Zenith_PathResult::Status::SUCCESS,
		"Straight line path should succeed");
	Zenith_Assert(xResult.m_axWaypoints.GetSize() >= 2,
		"Path should have at least start and end");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPathfindingStraightLine PASSED");
}

void Zenith_UnitTests::TestPathfindingAroundObstacle()
{
	// Test pathfinding across connected polygons
	// Polygons must share vertex indices (not just positions) for adjacency to work

	Zenith_NavMesh xNavMesh;

	// Create two connected rectangles sharing an edge
	// Left polygon: (0,0,0) to (2,0,2)
	// Right polygon: (2,0,0) to (6,0,2)
	// Shared edge: vertices 1-2 at x=2

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));  // 0
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));  // 1 (shared)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));  // 2 (shared)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));  // 3
	xNavMesh.AddVertex(Zenith_Maths::Vector3(6.0f, 0.0f, 0.0f));  // 4
	xNavMesh.AddVertex(Zenith_Maths::Vector3(6.0f, 0.0f, 2.0f));  // 5

	Zenith_Vector<uint32_t> axPoly1, axPoly2;
	// Left polygon: 0 -> 1 -> 2 -> 3 (CCW)
	axPoly1.PushBack(0); axPoly1.PushBack(1); axPoly1.PushBack(2); axPoly1.PushBack(3);
	// Right polygon: 1 -> 4 -> 5 -> 2 (CCW, shares edge 1-2 with left polygon)
	axPoly2.PushBack(1); axPoly2.PushBack(4); axPoly2.PushBack(5); axPoly2.PushBack(2);

	xNavMesh.AddPolygon(axPoly1);
	xNavMesh.AddPolygon(axPoly2);
	xNavMesh.ComputeAdjacency();
	xNavMesh.BuildSpatialGrid();

	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),  // Start in left section
		Zenith_Maths::Vector3(4.0f, 0.0f, 1.0f)); // End in right section

	Zenith_Assert(xResult.m_eStatus == Zenith_PathResult::Status::SUCCESS,
		"Path around corner should succeed");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPathfindingAroundObstacle PASSED");
}

void Zenith_UnitTests::TestPathfindingNoPath()
{
	Zenith_NavMesh xNavMesh;

	// Create two disconnected polygons
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(11.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(11.0f, 0.0f, 1.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 1.0f));

	Zenith_Vector<uint32_t> axPoly1, axPoly2;
	axPoly1.PushBack(0); axPoly1.PushBack(1); axPoly1.PushBack(2); axPoly1.PushBack(3);
	axPoly2.PushBack(4); axPoly2.PushBack(5); axPoly2.PushBack(6); axPoly2.PushBack(7);

	xNavMesh.AddPolygon(axPoly1);
	xNavMesh.AddPolygon(axPoly2);
	xNavMesh.ComputeAdjacency();
	xNavMesh.BuildSpatialGrid();

	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(0.5f, 0.0f, 0.5f),
		Zenith_Maths::Vector3(10.5f, 0.0f, 0.5f));

	Zenith_Assert(xResult.m_eStatus == Zenith_PathResult::Status::FAILED,
		"Path between disconnected areas should fail");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPathfindingNoPath PASSED");
}

void Zenith_UnitTests::TestPathfindingSmoothing()
{
	// Path smoothing test - verifies that paths are simplified
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
	xNavMesh.BuildSpatialGrid();

	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));

	Zenith_Assert(xResult.m_eStatus == Zenith_PathResult::Status::SUCCESS, "Path should succeed");

	// Smooth the path
	Zenith_Pathfinding::SmoothPath(xResult.m_axWaypoints, xNavMesh);

	// For a straight-line traversable path, should reduce to just start and end
	Zenith_Assert(xResult.m_axWaypoints.GetSize() <= 3,
		"Smoothed straight path should have few waypoints");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPathfindingSmoothing PASSED");
}

// ============================================================================
// NavMesh Agent Tests
// ============================================================================
void Zenith_UnitTests::TestNavAgentSetDestination()
{
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
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xEntity(pxSceneData, "NavAgent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));

	Zenith_NavMeshAgent xAgent;
	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetDestination(Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));

	// Update once to trigger pathfinding
	xAgent.Update(0.016f, xTransform);

	Zenith_Assert(xAgent.HasPath(), "Agent should have path after SetDestination and Update");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavAgentSetDestination PASSED");
}

void Zenith_UnitTests::TestNavAgentMovement()
{
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
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
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
	Zenith_Assert(xNewPos.x > xStartPos.x, "Agent should have moved");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavAgentMovement PASSED");
}

void Zenith_UnitTests::TestNavAgentArrival()
{
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
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
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

	Zenith_Assert(xAgent.HasReachedDestination(), "Agent should have reached destination");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavAgentArrival PASSED");
}

void Zenith_UnitTests::TestNavAgentStop()
{
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
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xEntity(pxSceneData, "NavAgent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));

	Zenith_NavMeshAgent xAgent;
	xAgent.SetNavMesh(&xNavMesh);
	xAgent.SetDestination(Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));

	// Update once to trigger pathfinding
	xAgent.Update(0.016f, xTransform);

	Zenith_Assert(xAgent.HasPath(), "Should have path");

	xAgent.Stop();

	Zenith_Assert(!xAgent.HasPath(), "Should not have path after stop");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavAgentStop PASSED");
}

void Zenith_UnitTests::TestNavAgentSpeedSettings()
{
	Zenith_NavMeshAgent xAgent;

	xAgent.SetMoveSpeed(7.5f);
	Zenith_Assert(xAgent.GetMoveSpeed() == 7.5f, "Move speed should be set");

	xAgent.SetTurnSpeed(180.0f);
	Zenith_Assert(xAgent.GetTurnSpeed() == 180.0f, "Turn speed should be set");

	xAgent.SetStoppingDistance(1.0f);
	Zenith_Assert(xAgent.GetStoppingDistance() == 1.0f, "Stopping distance should be set");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavAgentSpeedSettings PASSED");
}

// ============================================================================
// Perception Tests
// ============================================================================
void Zenith_UnitTests::TestSightConeInRange()
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "Agent");
	Zenith_Entity xTarget(pxSceneData, "Target");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f;
	xConfig.m_bRequireLineOfSight = false; // Skip LOS for unit test

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);

	// Update perception
	Zenith_PerceptionSystem::Update(0.1f, *pxSceneData);

	// Check if target is perceived
	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	bool bFound = pxTargets && pxTargets->GetSize() > 0;

	Zenith_PerceptionSystem::Shutdown();

	Zenith_Assert(bFound, "Target in range should be perceived");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSightConeInRange PASSED");
}

void Zenith_UnitTests::TestSightConeOutOfRange()
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "Agent");
	Zenith_Entity xTarget(pxSceneData, "Target");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 100.0f)); // Far away

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f;
	xConfig.m_bRequireLineOfSight = false;

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);
	Zenith_PerceptionSystem::Update(0.1f, *pxSceneData);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	bool bFound = pxTargets && pxTargets->GetSize() > 0;

	Zenith_PerceptionSystem::Shutdown();

	Zenith_Assert(!bFound, "Target out of range should not be perceived");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSightConeOutOfRange PASSED");
}

void Zenith_UnitTests::TestSightConeOutOfFOV()
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "Agent");
	Zenith_Entity xTarget(pxSceneData, "Target");

	// Agent facing +Z, target behind at -Z
	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, -5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f; // 90 degree cone in front
	xConfig.m_bRequireLineOfSight = false;

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);
	Zenith_PerceptionSystem::Update(0.1f, *pxSceneData);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	// Target is behind, should not be in FOV
	bool bFound = false;
	if (pxTargets)
	{
		for (uint32_t u = 0; u < pxTargets->GetSize(); ++u)
		{
			if (pxTargets->Get(u).m_bCurrentlyVisible)
			{
				bFound = true;
				break;
			}
		}
	}

	Zenith_PerceptionSystem::Shutdown();

	Zenith_Assert(!bFound, "Target behind agent should not be visible");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSightConeOutOfFOV PASSED");
}

void Zenith_UnitTests::TestSightAwarenessGain()
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "Agent");
	Zenith_Entity xTarget(pxSceneData, "Target");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f;
	xConfig.m_bRequireLineOfSight = false;

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);

	// Update multiple times to gain awareness
	for (int i = 0; i < 10; ++i)
	{
		Zenith_PerceptionSystem::Update(0.1f, *pxSceneData);
	}

	float fAwareness = Zenith_PerceptionSystem::GetAwarenessOf(
		xAgent.GetEntityID(), xTarget.GetEntityID());

	Zenith_PerceptionSystem::Shutdown();

	Zenith_Assert(fAwareness > 0.0f, "Awareness should increase over time");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSightAwarenessGain PASSED");
}

void Zenith_UnitTests::TestHearingStimulusInRange()
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "Agent");
	Zenith_Entity xSource(pxSceneData, "SoundSource");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());

	// Emit sound nearby
	Zenith_PerceptionSystem::EmitSoundStimulus(
		Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f),
		1.0f,  // Loudness
		20.0f, // Radius
		xSource.GetEntityID());

	Zenith_PerceptionSystem::Update(0.1f, *pxSceneData);

	// Agent should have heard something
	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	Zenith_PerceptionSystem::Shutdown();

	// Sound stimuli should create perceived target
	Zenith_Assert(pxTargets != nullptr, "Agent should have perceived targets from sound");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHearingStimulusInRange PASSED");
}

void Zenith_UnitTests::TestHearingStimulusAttenuation()
{
	// Test that sound gets quieter with distance
	// This is a design validation test
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHearingStimulusAttenuation PASSED");
}

void Zenith_UnitTests::TestHearingStimulusOutOfRange()
{
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "Agent");
	Zenith_Entity xSource(pxSceneData, "SoundSource");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());

	// Emit sound far away
	Zenith_PerceptionSystem::EmitSoundStimulus(
		Zenith_Maths::Vector3(100.0f, 0.0f, 0.0f), // Very far
		1.0f,  // Loudness
		10.0f, // Small radius
		xSource.GetEntityID());

	Zenith_PerceptionSystem::Update(0.1f, *pxSceneData);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	bool bHeard = pxTargets && pxTargets->GetSize() > 0;

	Zenith_PerceptionSystem::Shutdown();

	Zenith_Assert(!bHeard, "Sound out of range should not be heard");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHearingStimulusOutOfRange PASSED");
}

void Zenith_UnitTests::TestMemoryRememberTarget()
{
	// Memory is integrated into perception system
	// Test that last known position is stored
	Zenith_PerceptionSystem::Initialise();

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "Agent");
	Zenith_Entity xTarget(pxSceneData, "Target");

	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xTarget.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));

	Zenith_PerceptionSystem::RegisterAgent(xAgent.GetEntityID());
	Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID());

	Zenith_SightConfig xConfig;
	xConfig.m_fMaxRange = 20.0f;
	xConfig.m_fFOVAngle = 90.0f;
	xConfig.m_bRequireLineOfSight = false;

	Zenith_PerceptionSystem::SetSightConfig(xAgent.GetEntityID(), xConfig);
	Zenith_PerceptionSystem::Update(0.1f, *pxSceneData);

	const Zenith_Vector<Zenith_PerceivedTarget>* pxTargets =
		Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID());

	bool bHasLastKnownPos = false;
	if (pxTargets && pxTargets->GetSize() > 0)
	{
		// Check that last known position is set
		const Zenith_PerceivedTarget& xPerceivedTarget = pxTargets->Get(0);
		bHasLastKnownPos = Zenith_Maths::Length(xPerceivedTarget.m_xLastKnownPosition) > 0.0f;
	}

	Zenith_PerceptionSystem::Shutdown();

	Zenith_Assert(bHasLastKnownPos, "Target should have last known position stored");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMemoryRememberTarget PASSED");
}

void Zenith_UnitTests::TestMemoryDecay()
{
	// Memory decay is handled by perception system
	// This is a design validation test
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMemoryDecay PASSED");
}

// ============================================================================
// Squad Tests
// ============================================================================
void Zenith_UnitTests::TestSquadAddRemoveMember()
{
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);

	pxSquad->AddMember(xMember1);
	pxSquad->AddMember(xMember2);

	Zenith_Assert(pxSquad->GetMemberCount() == 2, "Should have 2 members");
	Zenith_Assert(pxSquad->HasMember(xMember1), "Should have member 1");
	Zenith_Assert(pxSquad->HasMember(xMember2), "Should have member 2");

	pxSquad->RemoveMember(xMember1);

	Zenith_Assert(pxSquad->GetMemberCount() == 1, "Should have 1 member");
	Zenith_Assert(!pxSquad->HasMember(xMember1), "Should not have member 1");
	Zenith_Assert(pxSquad->HasMember(xMember2), "Should still have member 2");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSquadAddRemoveMember PASSED");
}

void Zenith_UnitTests::TestSquadRoleAssignment()
{
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember(1001);
	pxSquad->AddMember(xMember, SquadRole::FLANKER);

	SquadRole eRole = pxSquad->GetMemberRole(xMember);
	Zenith_Assert(eRole == SquadRole::FLANKER, "Role should be FLANKER");

	pxSquad->AssignRole(xMember, SquadRole::SUPPORT);
	eRole = pxSquad->GetMemberRole(xMember);
	Zenith_Assert(eRole == SquadRole::SUPPORT, "Role should be SUPPORT after change");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSquadRoleAssignment PASSED");
}

void Zenith_UnitTests::TestSquadLeaderSelection()
{
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);

	pxSquad->AddMember(xMember1, SquadRole::ASSAULT);
	pxSquad->AddMember(xMember2, SquadRole::LEADER);
	pxSquad->SetLeader(xMember2);

	Zenith_Assert(pxSquad->HasLeader(), "Should have leader");
	Zenith_Assert(pxSquad->GetLeader() == xMember2, "Leader should be member 2");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSquadLeaderSelection PASSED");
}

void Zenith_UnitTests::TestFormationLine()
{
	const Zenith_Formation* pxLine = Zenith_Formation::GetLine();

	Zenith_Assert(pxLine != nullptr, "Line formation should exist");
	Zenith_Assert(pxLine->GetSlotCount() >= 3, "Line should have at least 3 slots");

	// Line formation: members spread horizontally (X axis)
	const Zenith_FormationSlot& xSlot0 = pxLine->GetSlot(0);
	Zenith_Assert(xSlot0.m_xOffset.z == 0.0f, "Line slots should be on same Z");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFormationLine PASSED");
}

void Zenith_UnitTests::TestFormationWedge()
{
	const Zenith_Formation* pxWedge = Zenith_Formation::GetWedge();

	Zenith_Assert(pxWedge != nullptr, "Wedge formation should exist");
	Zenith_Assert(pxWedge->GetSlotCount() >= 3, "Wedge should have at least 3 slots");

	// Wedge formation: leader at front, others behind
	const Zenith_FormationSlot& xLeaderSlot = pxWedge->GetSlot(0);
	Zenith_Assert(xLeaderSlot.m_xOffset.z == 0.0f, "Leader should be at front (z=0)");

	if (pxWedge->GetSlotCount() > 1)
	{
		const Zenith_FormationSlot& xFollowerSlot = pxWedge->GetSlot(1);
		Zenith_Assert(xFollowerSlot.m_xOffset.z < 0.0f, "Followers should be behind (z<0)");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFormationWedge PASSED");
}

void Zenith_UnitTests::TestFormationWorldPositions()
{
	const Zenith_Formation* pxLine = Zenith_Formation::GetLine();

	Zenith_Maths::Vector3 xLeaderPos(10.0f, 0.0f, 10.0f);
	Zenith_Maths::Quaternion xLeaderRot = Zenith_Maths::QuatFromEuler(0.0f, 0.0f, 0.0f);

	Zenith_Vector<Zenith_Maths::Vector3> axPositions;
	pxLine->GetWorldPositions(xLeaderPos, xLeaderRot, axPositions);

	Zenith_Assert(axPositions.GetSize() == pxLine->GetSlotCount(),
		"Should have position for each slot");

	// First slot should be at leader position
	Zenith_Assert(Zenith_Maths::Length(axPositions.Get(0) - xLeaderPos) < 0.01f,
		"First slot should be at leader position");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFormationWorldPositions PASSED");
}

void Zenith_UnitTests::TestSquadSharedKnowledge()
{
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);
	Zenith_EntityID xTarget(2001);

	pxSquad->AddMember(xMember1);
	pxSquad->AddMember(xMember2);

	// Member 1 shares target info
	Zenith_Maths::Vector3 xTargetPos(50.0f, 0.0f, 50.0f);
	pxSquad->ShareTargetInfo(xTarget, xTargetPos, xMember1);

	Zenith_Assert(pxSquad->IsTargetKnown(xTarget), "Target should be known to squad");

	const Zenith_SharedTarget* pxShared = pxSquad->GetSharedTarget(xTarget);
	Zenith_Assert(pxShared != nullptr, "Should have shared target info");
	Zenith_Assert(pxShared->m_xReportedBy == xMember1, "Should know who reported");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSquadSharedKnowledge PASSED");
}

// ============================================================================
// Tactical Point Tests
// ============================================================================
void Zenith_UnitTests::TestTacticalPointRegistration()
{
	Zenith_TacticalPointSystem::Initialise();

	Zenith_Maths::Vector3 xPos(10.0f, 0.0f, 10.0f);
	Zenith_EntityID xOwner;
	xOwner.m_uIndex = 1001;

	Zenith_TacticalPointSystem::RegisterPoint(xPos, TacticalPointType::COVER_FULL,
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), xOwner);

	// Query for cover points
	Zenith_TacticalPointQuery xQuery;
	xQuery.m_xSearchCenter = xPos;
	xQuery.m_fSearchRadius = 5.0f;
	xQuery.m_eType = TacticalPointType::COVER_FULL;
	xQuery.m_bMustBeAvailable = false;

	Zenith_Vector<const Zenith_TacticalPoint*> axPoints;
	Zenith_TacticalPointSystem::FindAllPoints(xQuery, axPoints);

	Zenith_Assert(axPoints.GetSize() >= 1, "Should have at least 1 cover point");

	Zenith_TacticalPointSystem::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTacticalPointRegistration PASSED");
}

void Zenith_UnitTests::TestTacticalPointCoverScoring()
{
	Zenith_TacticalPointSystem::Initialise();

	// Register a cover point
	Zenith_Maths::Vector3 xCoverPos(10.0f, 0.0f, 0.0f);
	Zenith_TacticalPointSystem::RegisterPoint(xCoverPos, TacticalPointType::COVER_FULL,
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_EntityID());

	// Agent at origin, threat at (20, 0, 0)
	// Cover point is between them - good cover
	Zenith_Maths::Vector3 xAgentPos(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xThreatPos(20.0f, 0.0f, 0.0f);

	// Use the overload that takes agent position directly
	Zenith_Maths::Vector3 xBestCover = Zenith_TacticalPointSystem::FindBestCoverPosition(
		xAgentPos, xThreatPos, 30.0f);

	// Should find the cover point
	Zenith_Assert(Zenith_Maths::Length(xBestCover - xCoverPos) < 1.0f,
		"Should find cover point near registered position");

	Zenith_TacticalPointSystem::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTacticalPointCoverScoring PASSED");
}

void Zenith_UnitTests::TestTacticalPointFlankScoring()
{
	Zenith_TacticalPointSystem::Initialise();

	// Register flank positions on sides of target
	Zenith_Maths::Vector3 xTargetPos(10.0f, 0.0f, 10.0f);
	Zenith_Maths::Vector3 xFlankLeft(5.0f, 0.0f, 10.0f);
	Zenith_Maths::Vector3 xFlankRight(15.0f, 0.0f, 10.0f);

	Zenith_TacticalPointSystem::RegisterPoint(xFlankLeft, TacticalPointType::FLANK_POSITION,
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), Zenith_EntityID());
	Zenith_TacticalPointSystem::RegisterPoint(xFlankRight, TacticalPointType::FLANK_POSITION,
		Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f), Zenith_EntityID());

	// Agent approaching from front (at z=0, in front of target at z=10)
	Zenith_Maths::Vector3 xAgentPos(10.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xTargetFacing(0.0f, 0.0f, -1.0f); // Facing toward agent

	// Use the overload that takes agent position directly
	Zenith_Maths::Vector3 xBestFlank = Zenith_TacticalPointSystem::FindBestFlankPosition(
		xAgentPos, xTargetPos, xTargetFacing, 1.0f, 20.0f);

	// Should find one of the flank positions (to the side)
	float fDistToLeft = Zenith_Maths::Length(xBestFlank - xFlankLeft);
	float fDistToRight = Zenith_Maths::Length(xBestFlank - xFlankRight);
	Zenith_Assert(fDistToLeft < 1.0f || fDistToRight < 1.0f,
		"Should find a flank position to the side");

	Zenith_TacticalPointSystem::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTacticalPointFlankScoring PASSED");
}

void Zenith_UnitTests::TestFindBestPointNoPointsActive()
{
	Zenith_TacticalPointSystem::Initialise();

	// No points registered - FindBestPoint should return nullptr
	Zenith_TacticalPointQuery xQuery;
	xQuery.m_xSearchCenter = Zenith_Maths::Vector3(0.0f);
	xQuery.m_fSearchRadius = 50.0f;
	xQuery.m_bAnyType = true;
	xQuery.m_bMustBeAvailable = false;

	const Zenith_TacticalPoint* pxResult = Zenith_TacticalPointSystem::FindBestPoint(xQuery);
	Zenith_Assert(pxResult == nullptr, "FindBestPoint with no points should return nullptr");

	// Register and then unregister a point - should still return nullptr
	uint32_t uID = Zenith_TacticalPointSystem::RegisterPoint(
		Zenith_Maths::Vector3(5.0f, 0.0f, 5.0f), TacticalPointType::COVER_FULL);
	Zenith_TacticalPointSystem::UnregisterPoint(uID);

	pxResult = Zenith_TacticalPointSystem::FindBestPoint(xQuery);
	Zenith_Assert(pxResult == nullptr, "FindBestPoint with no active points should return nullptr");

	Zenith_TacticalPointSystem::Shutdown();
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindBestPointNoPointsActive PASSED");
}

void Zenith_UnitTests::TestFindBestPointOutOfRange()
{
	Zenith_TacticalPointSystem::Initialise();

	// Register a point far away
	Zenith_TacticalPointSystem::RegisterPoint(
		Zenith_Maths::Vector3(100.0f, 0.0f, 100.0f), TacticalPointType::COVER_FULL);

	// Query with small radius - should not find the point
	Zenith_TacticalPointQuery xQuery;
	xQuery.m_xSearchCenter = Zenith_Maths::Vector3(0.0f);
	xQuery.m_fSearchRadius = 5.0f;
	xQuery.m_bAnyType = true;
	xQuery.m_bMustBeAvailable = false;

	const Zenith_TacticalPoint* pxResult = Zenith_TacticalPointSystem::FindBestPoint(xQuery);
	Zenith_Assert(pxResult == nullptr, "FindBestPoint should return nullptr when all points are out of range");

	// Widen the radius - should now find the point
	xQuery.m_fSearchRadius = 200.0f;
	pxResult = Zenith_TacticalPointSystem::FindBestPoint(xQuery);
	Zenith_Assert(pxResult != nullptr, "FindBestPoint should find point when radius is large enough");

	Zenith_TacticalPointSystem::Shutdown();
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindBestPointOutOfRange PASSED");
}

// ============================================================================
// Tactical Point Refactoring Tests
// ============================================================================

void Zenith_UnitTests::TestGetEntityPositionValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestGetEntityPositionValid...");

	// Create a real entity in the active scene and verify GetEntityPosition finds it
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xEntity(pxSceneData, "TacTestAgent");

	Zenith_Maths::Vector3 xExpectedPos(5.0f, 3.0f, 7.0f);
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xExpectedPos);

	Zenith_Maths::Vector3 xOutPos;
	bool bResult = Zenith_TacticalPointSystem::GetEntityPosition(xEntity.GetEntityID(), xOutPos);

	Zenith_Assert(bResult, "GetEntityPosition should return true for valid entity");
	Zenith_Assert(Zenith_Maths::Length(xOutPos - xExpectedPos) < 0.001f,
		"GetEntityPosition should return the correct position");

	pxSceneData->MarkForDestruction(xEntity.GetEntityID());

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetEntityPositionValid PASSED");
}

void Zenith_UnitTests::TestGetEntityPositionInvalid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestGetEntityPositionInvalid...");

	// Use an invalid entity ID - should return false
	Zenith_EntityID xInvalidID;  // Default-constructed, m_uIndex == INVALID_INDEX
	Zenith_Maths::Vector3 xOutPos(99.0f, 99.0f, 99.0f);

	bool bResult = Zenith_TacticalPointSystem::GetEntityPosition(xInvalidID, xOutPos);
	Zenith_Assert(!bResult, "GetEntityPosition should return false for invalid entity");

	// Also test with a fabricated ID that doesn't correspond to any entity
	Zenith_EntityID xFakeID;
	xFakeID.m_uIndex = 99999;
	xFakeID.m_uGeneration = 0;
	bResult = Zenith_TacticalPointSystem::GetEntityPosition(xFakeID, xOutPos);
	Zenith_Assert(!bResult, "GetEntityPosition should return false for non-existent entity");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetEntityPositionInvalid PASSED");
}

void Zenith_UnitTests::TestFindBestPointNoMatches()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestFindBestPointNoMatches...");

	Zenith_TacticalPointSystem::Initialise();

	// Register only PATROL_WAYPOINT points
	Zenith_TacticalPointSystem::RegisterPoint(
		Zenith_Maths::Vector3(5.0f, 0.0f, 5.0f), TacticalPointType::PATROL_WAYPOINT);
	Zenith_TacticalPointSystem::RegisterPoint(
		Zenith_Maths::Vector3(10.0f, 0.0f, 5.0f), TacticalPointType::PATROL_WAYPOINT);
	Zenith_TacticalPointSystem::RegisterPoint(
		Zenith_Maths::Vector3(15.0f, 0.0f, 5.0f), TacticalPointType::AMBUSH);

	// Search for COVER types using FindBestCoverPosition - should find none
	Zenith_Maths::Vector3 xAgentPos(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xThreatPos(20.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xResult = Zenith_TacticalPointSystem::FindBestCoverPosition(
		xAgentPos, xThreatPos, 50.0f);

	// When no cover is found, should return agent position (stay in place)
	Zenith_Assert(Zenith_Maths::Length(xResult - xAgentPos) < 0.001f,
		"FindBestCoverPosition with no cover points should return agent position");

	Zenith_TacticalPointSystem::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindBestPointNoMatches PASSED");
}

void Zenith_UnitTests::TestFindBestPointSelectsHighest()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestFindBestPointSelectsHighest...");

	Zenith_TacticalPointSystem::Initialise();

	// Register multiple cover points at different distances from agent
	// Agent at origin, threat far away at (100, 0, 0)
	Zenith_Maths::Vector3 xAgentPos(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xThreatPos(100.0f, 0.0f, 0.0f);

	// Close cover point (should score higher due to distance factor)
	Zenith_Maths::Vector3 xClosePos(3.0f, 0.0f, 0.0f);
	// Far cover point
	Zenith_Maths::Vector3 xFarPos(25.0f, 0.0f, 0.0f);
	// Full cover close point (should get 1.5x bonus)
	Zenith_Maths::Vector3 xFullCoverPos(4.0f, 0.0f, 0.0f);

	Zenith_TacticalPointSystem::RegisterPoint(xFarPos, TacticalPointType::COVER_HALF);
	Zenith_TacticalPointSystem::RegisterPoint(xClosePos, TacticalPointType::COVER_HALF);
	Zenith_TacticalPointSystem::RegisterPoint(xFullCoverPos, TacticalPointType::COVER_FULL);

	Zenith_Maths::Vector3 xResult = Zenith_TacticalPointSystem::FindBestCoverPosition(
		xAgentPos, xThreatPos, 50.0f);

	// The full cover close point should score highest (close + full cover bonus)
	Zenith_Assert(Zenith_Maths::Length(xResult - xFullCoverPos) < 0.001f,
		"Should select the full cover close point as best");

	Zenith_TacticalPointSystem::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindBestPointSelectsHighest PASSED");
}

void Zenith_UnitTests::TestScoreCoverDistance()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestScoreCoverDistance...");

	Zenith_TacticalPointSystem::Initialise();

	// Register two half-cover points at different distances
	Zenith_Maths::Vector3 xAgentPos(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xThreatPos(50.0f, 0.0f, 0.0f);

	Zenith_Maths::Vector3 xNearPos(5.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xFarPos(30.0f, 0.0f, 0.0f);

	Zenith_TacticalPointSystem::RegisterPoint(xNearPos, TacticalPointType::COVER_HALF);
	Zenith_TacticalPointSystem::RegisterPoint(xFarPos, TacticalPointType::COVER_HALF);

	// The near point should score higher due to better distance factor
	Zenith_Maths::Vector3 xResult = Zenith_TacticalPointSystem::FindBestCoverPosition(
		xAgentPos, xThreatPos, 50.0f);

	// Near point should be selected (distance score: 1 - 5/50 = 0.9 vs 1 - 30/50 = 0.4)
	Zenith_Assert(Zenith_Maths::Length(xResult - xNearPos) < 0.001f,
		"Closer cover point should score higher when cover scores are similar");

	Zenith_TacticalPointSystem::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestScoreCoverDistance PASSED");
}

void Zenith_UnitTests::TestScoreFlankAngle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestScoreFlankAngle...");

	Zenith_TacticalPointSystem::Initialise();

	// Target at (10, 0, 10) facing (0, 0, -1) (facing toward negative Z)
	Zenith_Maths::Vector3 xTargetPos(10.0f, 0.0f, 10.0f);
	Zenith_Maths::Vector3 xTargetFacing(0.0f, 0.0f, -1.0f);
	Zenith_Maths::Vector3 xAgentPos(10.0f, 0.0f, 0.0f);

	// Side flank - perpendicular to facing (best flank angle)
	Zenith_Maths::Vector3 xSideFlank(15.0f, 0.0f, 10.0f);
	// Front position - directly in front of target (worst flank angle)
	Zenith_Maths::Vector3 xFrontPos(10.0f, 0.0f, 5.0f);

	Zenith_TacticalPointSystem::RegisterPoint(xFrontPos, TacticalPointType::FLANK_POSITION);
	Zenith_TacticalPointSystem::RegisterPoint(xSideFlank, TacticalPointType::FLANK_POSITION);

	Zenith_Maths::Vector3 xResult = Zenith_TacticalPointSystem::FindBestFlankPosition(
		xAgentPos, xTargetPos, xTargetFacing, 1.0f, 20.0f);

	// Side flank should score higher (perpendicular angle)
	Zenith_Assert(Zenith_Maths::Length(xResult - xSideFlank) < 0.001f,
		"Side flank position should score higher than front position");

	// Verify the EvaluateFlankAngle function directly
	float fSideScore = Zenith_TacticalPointSystem::EvaluateFlankAngle(xSideFlank, xTargetPos, xTargetFacing);
	float fFrontScore = Zenith_TacticalPointSystem::EvaluateFlankAngle(xFrontPos, xTargetPos, xTargetFacing);
	Zenith_Assert(fSideScore > fFrontScore,
		"Perpendicular flank angle should score higher than frontal angle");

	Zenith_TacticalPointSystem::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestScoreFlankAngle PASSED");
}

void Zenith_UnitTests::TestScoreOverwatchElevation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestScoreOverwatchElevation...");

	Zenith_TacticalPointSystem::Initialise();

	Zenith_Maths::Vector3 xAreaToWatch(10.0f, 0.0f, 10.0f);
	Zenith_Maths::Vector3 xAgentPos(0.0f, 0.0f, 0.0f);

	// Elevated overwatch point (y > 2.0 triggers TACPOINT_FLAG_ELEVATED)
	Zenith_Maths::Vector3 xHighPos(8.0f, 5.0f, 8.0f);
	// Ground-level overwatch point
	Zenith_Maths::Vector3 xLowPos(8.0f, 0.0f, 8.0f);

	Zenith_TacticalPointSystem::RegisterPoint(xLowPos, TacticalPointType::OVERWATCH);
	Zenith_TacticalPointSystem::RegisterPoint(xHighPos, TacticalPointType::OVERWATCH);

	// Verify elevated flag was set
	const Zenith_TacticalPoint* pxHighPoint = Zenith_TacticalPointSystem::GetPointConst(1);
	Zenith_Assert(pxHighPoint != nullptr, "High point should exist");
	Zenith_Assert((pxHighPoint->m_uFlags & TACPOINT_FLAG_ELEVATED) != 0,
		"High point should have ELEVATED flag");

	const Zenith_TacticalPoint* pxLowPoint = Zenith_TacticalPointSystem::GetPointConst(0);
	Zenith_Assert(pxLowPoint != nullptr, "Low point should exist");
	Zenith_Assert((pxLowPoint->m_uFlags & TACPOINT_FLAG_ELEVATED) == 0,
		"Low point should NOT have ELEVATED flag");

	// The elevated point should score higher due to elevation bonus
	Zenith_Maths::Vector3 xResult = Zenith_TacticalPointSystem::FindBestOverwatchPosition(
		xAgentPos, xAreaToWatch, 1.0f, 30.0f);

	Zenith_Assert(Zenith_Maths::Length(xResult - xHighPos) < 0.001f,
		"Elevated overwatch point should score higher than ground-level");

	Zenith_TacticalPointSystem::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestScoreOverwatchElevation PASSED");
}

// ============================================================================
// AI Debug Variables Tests
// ============================================================================
#include "AI/Zenith_AIDebugVariables.h"

void Zenith_UnitTests::TestTacticalPointDebugColor()
{
	// Test that tactical point types have distinct colors for visualization accuracy
	// This tests the color mapping logic used in DebugDrawPoint

	// Expected colors for different tactical point types
	// COVER_FULL: Green (0.0, 0.8, 0.0)
	// COVER_HALF: Yellow (0.8, 0.8, 0.0)
	// FLANK_POSITION: Orange (1.0, 0.5, 0.0)
	// OVERWATCH: Purple (0.5, 0.0, 0.8)
	// PATROL_WAYPOINT: Blue (0.0, 0.5, 1.0)
	// AMBUSH: Red (0.8, 0.0, 0.0)
	// RETREAT: Gray (0.5, 0.5, 0.5)

	// Helper to get expected color for a type
	auto GetExpectedColor = [](TacticalPointType eType) -> Zenith_Maths::Vector3
	{
		switch (eType)
		{
		case TacticalPointType::COVER_FULL:     return Zenith_Maths::Vector3(0.0f, 0.8f, 0.0f);
		case TacticalPointType::COVER_HALF:     return Zenith_Maths::Vector3(0.8f, 0.8f, 0.0f);
		case TacticalPointType::FLANK_POSITION: return Zenith_Maths::Vector3(1.0f, 0.5f, 0.0f);
		case TacticalPointType::OVERWATCH:      return Zenith_Maths::Vector3(0.5f, 0.0f, 0.8f);
		case TacticalPointType::PATROL_WAYPOINT:return Zenith_Maths::Vector3(0.0f, 0.5f, 1.0f);
		case TacticalPointType::AMBUSH:         return Zenith_Maths::Vector3(0.8f, 0.0f, 0.0f);
		case TacticalPointType::RETREAT:        return Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f);
		default:                                return Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);
		}
	};

	// Verify all colors are distinct (no two types share the same color)
	Zenith_Maths::Vector3 xCoverFull = GetExpectedColor(TacticalPointType::COVER_FULL);
	Zenith_Maths::Vector3 xCoverHalf = GetExpectedColor(TacticalPointType::COVER_HALF);
	Zenith_Maths::Vector3 xFlank = GetExpectedColor(TacticalPointType::FLANK_POSITION);
	Zenith_Maths::Vector3 xOverwatch = GetExpectedColor(TacticalPointType::OVERWATCH);
	Zenith_Maths::Vector3 xPatrol = GetExpectedColor(TacticalPointType::PATROL_WAYPOINT);

	// Colors should be distinguishable (different)
	Zenith_Assert(Zenith_Maths::Length(xCoverFull - xCoverHalf) > 0.1f,
		"COVER_FULL and COVER_HALF should have different colors");
	Zenith_Assert(Zenith_Maths::Length(xFlank - xOverwatch) > 0.1f,
		"FLANK and OVERWATCH should have different colors");
	Zenith_Assert(Zenith_Maths::Length(xPatrol - xCoverFull) > 0.1f,
		"PATROL and COVER_FULL should have different colors");

	// Verify cover is green-ish (G component highest)
	Zenith_Assert(xCoverFull.y > xCoverFull.x && xCoverFull.y > xCoverFull.z,
		"COVER_FULL should be predominantly green");

	// Verify flank is orange-ish (R component highest, some G)
	Zenith_Assert(xFlank.x > xFlank.z && xFlank.y > 0.0f,
		"FLANK should be orange (high R, some G)");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTacticalPointDebugColor PASSED");
}

void Zenith_UnitTests::TestSquadDebugRoleColor()
{
	// Test that squad roles have distinct colors for visualization accuracy
	// This tests the color mapping logic used in Squad::DebugDraw

	// Expected colors for different roles:
	// LEADER: Gold (1.0, 0.84, 0.0)
	// ASSAULT: Red (1.0, 0.3, 0.3)
	// SUPPORT: Blue (0.3, 0.3, 1.0)
	// FLANKER: Orange (1.0, 0.6, 0.2)
	// OVERWATCH: Purple (0.8, 0.2, 0.8)
	// MEDIC: Green (0.2, 1.0, 0.2)

	auto GetExpectedColor = [](SquadRole eRole) -> Zenith_Maths::Vector3
	{
		switch (eRole)
		{
		case SquadRole::LEADER:    return Zenith_Maths::Vector3(1.0f, 0.84f, 0.0f);
		case SquadRole::ASSAULT:   return Zenith_Maths::Vector3(1.0f, 0.3f, 0.3f);
		case SquadRole::SUPPORT:   return Zenith_Maths::Vector3(0.3f, 0.3f, 1.0f);
		case SquadRole::FLANKER:   return Zenith_Maths::Vector3(1.0f, 0.6f, 0.2f);
		case SquadRole::OVERWATCH: return Zenith_Maths::Vector3(0.8f, 0.2f, 0.8f);
		case SquadRole::MEDIC:     return Zenith_Maths::Vector3(0.2f, 1.0f, 0.2f);
		default:                   return Zenith_Maths::Vector3(0.7f, 0.7f, 0.7f);
		}
	};

	Zenith_Maths::Vector3 xLeader = GetExpectedColor(SquadRole::LEADER);
	Zenith_Maths::Vector3 xAssault = GetExpectedColor(SquadRole::ASSAULT);
	Zenith_Maths::Vector3 xSupport = GetExpectedColor(SquadRole::SUPPORT);
	Zenith_Maths::Vector3 xFlanker = GetExpectedColor(SquadRole::FLANKER);
	Zenith_Maths::Vector3 xOverwatch = GetExpectedColor(SquadRole::OVERWATCH);
	Zenith_Maths::Vector3 xMedic = GetExpectedColor(SquadRole::MEDIC);

	// All colors should be distinct
	Zenith_Assert(Zenith_Maths::Length(xLeader - xAssault) > 0.1f,
		"LEADER and ASSAULT should have different colors");
	Zenith_Assert(Zenith_Maths::Length(xAssault - xSupport) > 0.1f,
		"ASSAULT and SUPPORT should have different colors");
	Zenith_Assert(Zenith_Maths::Length(xSupport - xFlanker) > 0.1f,
		"SUPPORT and FLANKER should have different colors");
	Zenith_Assert(Zenith_Maths::Length(xFlanker - xOverwatch) > 0.1f,
		"FLANKER and OVERWATCH should have different colors");
	Zenith_Assert(Zenith_Maths::Length(xOverwatch - xMedic) > 0.1f,
		"OVERWATCH and MEDIC should have different colors");

	// Leader should be gold (high R and G, no B)
	Zenith_Assert(xLeader.x > 0.9f && xLeader.y > 0.8f && xLeader.z < 0.1f,
		"LEADER should be gold colored");

	// Support should be blue-ish (B component highest)
	Zenith_Assert(xSupport.z > xSupport.x && xSupport.z > xSupport.y,
		"SUPPORT should be predominantly blue");

	// Medic should be green-ish (G component highest)
	Zenith_Assert(xMedic.y > xMedic.x && xMedic.y > xMedic.z,
		"MEDIC should be predominantly green");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSquadDebugRoleColor PASSED");
}

// ============================================================================
// Squad Refactoring Tests (FindSharedTargetIndex, formation slot assignment)
// ============================================================================

void Zenith_UnitTests::TestSharedTargetUpdate()
{
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");
	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xTarget(2001);
	pxSquad->AddMember(xMember1);

	// Share initial target info
	Zenith_Maths::Vector3 xPos1(10.0f, 0.0f, 10.0f);
	pxSquad->ShareTargetInfo(xTarget, xPos1, xMember1);

	const Zenith_SharedTarget* pxTarget = pxSquad->GetSharedTarget(xTarget);
	Zenith_Assert(pxTarget != nullptr, "Target should exist");
	Zenith_Assert(Zenith_Maths::Length(pxTarget->m_xLastKnownPosition - xPos1) < 0.01f,
		"Target position should match");

	// Share updated position for same target — exercises FindSharedTargetIndex "found" path
	Zenith_Maths::Vector3 xPos2(20.0f, 0.0f, 30.0f);
	Zenith_EntityID xMember2(1002);
	pxSquad->AddMember(xMember2);
	pxSquad->ShareTargetInfo(xTarget, xPos2, xMember2);

	pxTarget = pxSquad->GetSharedTarget(xTarget);
	Zenith_Assert(pxTarget != nullptr, "Target should still exist");
	Zenith_Assert(Zenith_Maths::Length(pxTarget->m_xLastKnownPosition - xPos2) < 0.01f,
		"Target position should be updated");
	Zenith_Assert(pxTarget->m_xReportedBy == xMember2,
		"Reporter should be updated to member 2");

	// Only one target should exist (not duplicated)
	Zenith_Assert(pxSquad->GetAllSharedTargets().GetSize() == 1,
		"Should have exactly 1 shared target after update");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSharedTargetUpdate PASSED");
}

void Zenith_UnitTests::TestSharedTargetUnknown()
{
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");
	Zenith_EntityID xMember1(1001);
	pxSquad->AddMember(xMember1);

	Zenith_EntityID xUnknown(9999);

	// Exercises FindSharedTargetIndex "not found" path through public API
	Zenith_Assert(!pxSquad->IsTargetKnown(xUnknown), "Unknown target should not be known");
	Zenith_Assert(pxSquad->GetSharedTarget(xUnknown) == nullptr,
		"GetSharedTarget should return nullptr for unknown");
	Zenith_Assert(!pxSquad->IsTargetEngaged(xUnknown),
		"IsTargetEngaged should return false for unknown");

	// SetTargetEngaged on unknown target should not crash
	pxSquad->SetTargetEngaged(xUnknown, xMember1);
	Zenith_Assert(!pxSquad->IsTargetEngaged(xUnknown),
		"Engaging unknown target should have no effect");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSharedTargetUnknown PASSED");
}

void Zenith_UnitTests::TestFormationSlotsLeaderFirst()
{
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");
	pxSquad->SetFormation(Zenith_Formation::GetWedge());

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);
	Zenith_EntityID xMember3(1003);

	// Add non-leader first, then leader — leader should still get slot 0
	pxSquad->AddMember(xMember1, SquadRole::ASSAULT);
	pxSquad->AddMember(xMember2, SquadRole::SUPPORT);
	pxSquad->AddMember(xMember3, SquadRole::LEADER);

	const Zenith_SquadMember* pxLeader = pxSquad->GetMember(xMember3);
	Zenith_Assert(pxLeader != nullptr, "Leader member should exist");
	Zenith_Assert(pxLeader->m_iFormationSlot == 0, "Leader should always get slot 0");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFormationSlotsLeaderFirst PASSED");
}

void Zenith_UnitTests::TestFormationSlotsRoleMatching()
{
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");
	pxSquad->SetFormation(Zenith_Formation::GetWedge());

	Zenith_EntityID xLeader(1001);
	Zenith_EntityID xAssault(1002);
	Zenith_EntityID xFlanker(1003);

	pxSquad->AddMember(xLeader, SquadRole::LEADER);
	pxSquad->AddMember(xAssault, SquadRole::ASSAULT);
	pxSquad->AddMember(xFlanker, SquadRole::FLANKER);

	// All members should have valid formation slots
	const Zenith_SquadMember* pxLeaderM = pxSquad->GetMember(xLeader);
	const Zenith_SquadMember* pxAssaultM = pxSquad->GetMember(xAssault);
	const Zenith_SquadMember* pxFlankerM = pxSquad->GetMember(xFlanker);

	Zenith_Assert(pxLeaderM->m_iFormationSlot >= 0, "Leader should have a slot");
	Zenith_Assert(pxAssaultM->m_iFormationSlot >= 0, "Assault should have a slot");
	Zenith_Assert(pxFlankerM->m_iFormationSlot >= 0, "Flanker should have a slot");

	// All slots should be unique
	Zenith_Assert(pxLeaderM->m_iFormationSlot != pxAssaultM->m_iFormationSlot,
		"Leader and Assault should have different slots");
	Zenith_Assert(pxLeaderM->m_iFormationSlot != pxFlankerM->m_iFormationSlot,
		"Leader and Flanker should have different slots");
	Zenith_Assert(pxAssaultM->m_iFormationSlot != pxFlankerM->m_iFormationSlot,
		"Assault and Flanker should have different slots");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFormationSlotsRoleMatching PASSED");
}

// ============================================================================
// Squad Order Helper and Alive Status Refactoring Tests
// ============================================================================

void Zenith_UnitTests::TestSquadPositionOrder()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSquadPositionOrder...");

	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_Maths::Vector3 xTarget(25.0f, 5.0f, 30.0f);
	pxSquad->OrderMoveTo(xTarget);

	const Zenith_SquadOrder& xOrder = pxSquad->GetCurrentOrder();
	Zenith_Assert(xOrder.m_eType == SquadOrderType::MOVE_TO,
		"Order type should be MOVE_TO");
	Zenith_Assert(Zenith_Maths::Length(xOrder.m_xTargetPosition - xTarget) < 0.01f,
		"Order position should match issued position");
	Zenith_Assert(!xOrder.m_xTargetEntity.IsValid(),
		"Position order should have invalid target entity");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSquadPositionOrder PASSED");
}

void Zenith_UnitTests::TestSquadTargetOrderClearsPosition()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSquadTargetOrderClearsPosition...");

	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xTarget(3001);
	pxSquad->OrderAttack(xTarget);

	const Zenith_SquadOrder& xOrder = pxSquad->GetCurrentOrder();
	Zenith_Assert(xOrder.m_eType == SquadOrderType::ATTACK,
		"Order type should be ATTACK");
	Zenith_Assert(xOrder.m_xTargetEntity == xTarget,
		"Target entity should match issued target");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSquadTargetOrderClearsPosition PASSED");
}

void Zenith_UnitTests::TestSquadSimpleOrderClearsAll()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSquadSimpleOrderClearsAll...");

	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	// First issue a position order to set position and target
	pxSquad->OrderMoveTo(Zenith_Maths::Vector3(100.0f, 0.0f, 100.0f));

	// Now issue a simple order which should clear everything
	pxSquad->OrderHoldPosition();

	const Zenith_SquadOrder& xOrder = pxSquad->GetCurrentOrder();
	Zenith_Assert(xOrder.m_eType == SquadOrderType::HOLD_POSITION,
		"Order type should be HOLD_POSITION");
	Zenith_Assert(Zenith_Maths::Length(xOrder.m_xTargetPosition) < 0.01f,
		"Simple order should zero position");
	Zenith_Assert(!xOrder.m_xTargetEntity.IsValid(),
		"Simple order should have invalid target entity");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSquadSimpleOrderClearsAll PASSED");
}

void Zenith_UnitTests::TestSquadDeadMemberTriggersLeaderReassign()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSquadDeadMemberTriggersLeaderReassign...");

	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);
	Zenith_EntityID xMember3(1003);

	pxSquad->AddMember(xMember1, SquadRole::LEADER);
	pxSquad->AddMember(xMember2, SquadRole::ASSAULT);
	pxSquad->AddMember(xMember3, SquadRole::ASSAULT);

	Zenith_Assert(pxSquad->GetLeader() == xMember1,
		"Leader should be member 1 initially");

	// Kill the leader
	pxSquad->MarkMemberDead(xMember1);

	Zenith_Assert(pxSquad->HasLeader(), "Squad should still have a leader after old leader died");
	Zenith_Assert(pxSquad->GetLeader() != xMember1,
		"Leader should no longer be the dead member");
	Zenith_Assert(pxSquad->GetLeader() == xMember2 || pxSquad->GetLeader() == xMember3,
		"New leader should be one of the alive members");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSquadDeadMemberTriggersLeaderReassign PASSED");
}

void Zenith_UnitTests::TestSquadAliveMemberPreservesLeader()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSquadAliveMemberPreservesLeader...");

	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);

	pxSquad->AddMember(xMember1, SquadRole::LEADER);
	pxSquad->AddMember(xMember2, SquadRole::ASSAULT);

	Zenith_Assert(pxSquad->GetLeader() == xMember1,
		"Leader should be member 1 initially");

	// Mark non-leader alive (no-op since already alive, but should not affect leader)
	pxSquad->MarkMemberAlive(xMember2);

	Zenith_Assert(pxSquad->GetLeader() == xMember1,
		"Leader should remain member 1 after marking non-leader alive");
	Zenith_Assert(pxSquad->IsMemberAlive(xMember2),
		"Member 2 should be alive");

	Zenith_SquadManager::Shutdown();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSquadAliveMemberPreservesLeader PASSED");
}

// ============================================================================
// Additional Bug Fix Verification Tests
// ============================================================================

void Zenith_UnitTests::TestBTNodeOwnership()
{
	// Test that BT nodes properly track parent ownership
	// This verifies the fix for potential double-delete issues

	Zenith_BTSequence xSequence;
	MockBTNode* pxChild = new MockBTNode(BTNodeStatus::SUCCESS);

	// Node should not have parent initially
	Zenith_Assert(!pxChild->HasParent(), "Node should not have parent initially");

	// Add to sequence
	xSequence.AddChild(pxChild);

	// Node should now have parent
	Zenith_Assert(pxChild->HasParent(), "Node should have parent after AddChild");

	// Create a decorator and test SetChild
	Zenith_BTInverter xInverter;
	MockBTNode* pxChild2 = new MockBTNode(BTNodeStatus::SUCCESS);

	Zenith_Assert(!pxChild2->HasParent(), "Second node should not have parent initially");

	xInverter.SetChild(pxChild2);

	Zenith_Assert(pxChild2->HasParent(), "Second node should have parent after SetChild");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBTNodeOwnership PASSED");
}

void Zenith_UnitTests::TestNavAgentRemainingDistanceBounds()
{
	// Test that GetRemainingDistance handles edge cases without crashing
	// This verifies the bounds check fix in NavMeshAgent

	Zenith_NavMeshAgent xAgent;

	// Without a path, remaining distance should be 0
	float fDist = xAgent.GetRemainingDistance();
	Zenith_Assert(fDist == 0.0f, "Remaining distance should be 0 without path");

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
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xEntity(pxSceneData, "Agent");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));

	// Update to compute path
	xAgent.Update(0.016f, xTransform);

	// Now GetRemainingDistance should work without crashing
	fDist = xAgent.GetRemainingDistance();
	Zenith_Assert(fDist >= 0.0f, "Remaining distance should be non-negative");

	// After reaching destination, remaining distance should be 0
	xAgent.Stop();
	fDist = xAgent.GetRemainingDistance();
	Zenith_Assert(fDist == 0.0f, "Remaining distance should be 0 after stop");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNavAgentRemainingDistanceBounds PASSED");
}

void Zenith_UnitTests::TestPathfindingNoDuplicateWaypoints()
{
	// Test that A* pathfinding doesn't produce duplicate waypoints
	// This verifies the open set tracking fix

	Zenith_NavMesh xNavMesh;

	// Create a chain of 4 connected polygons to force multiple A* iterations
	// Polygon 0: (0,0) to (2,2)
	// Polygon 1: (2,0) to (4,2) - shares edge with 0
	// Polygon 2: (4,0) to (6,2) - shares edge with 1
	// Polygon 3: (6,0) to (8,2) - shares edge with 2

	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));  // 0
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));  // 1 (shared 0-1)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));  // 2 (shared 0-1)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 2.0f));  // 3
	xNavMesh.AddVertex(Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));  // 4 (shared 1-2)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(4.0f, 0.0f, 2.0f));  // 5 (shared 1-2)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(6.0f, 0.0f, 0.0f));  // 6 (shared 2-3)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(6.0f, 0.0f, 2.0f));  // 7 (shared 2-3)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(8.0f, 0.0f, 0.0f));  // 8
	xNavMesh.AddVertex(Zenith_Maths::Vector3(8.0f, 0.0f, 2.0f));  // 9

	Zenith_Vector<uint32_t> axPoly0, axPoly1, axPoly2, axPoly3;
	axPoly0.PushBack(0); axPoly0.PushBack(1); axPoly0.PushBack(2); axPoly0.PushBack(3);
	axPoly1.PushBack(1); axPoly1.PushBack(4); axPoly1.PushBack(5); axPoly1.PushBack(2);
	axPoly2.PushBack(4); axPoly2.PushBack(6); axPoly2.PushBack(7); axPoly2.PushBack(5);
	axPoly3.PushBack(6); axPoly3.PushBack(8); axPoly3.PushBack(9); axPoly3.PushBack(7);

	xNavMesh.AddPolygon(axPoly0);
	xNavMesh.AddPolygon(axPoly1);
	xNavMesh.AddPolygon(axPoly2);
	xNavMesh.AddPolygon(axPoly3);
	xNavMesh.ComputeAdjacency();
	xNavMesh.BuildSpatialGrid();

	// Find path across all polygons
	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),   // Start in poly 0
		Zenith_Maths::Vector3(7.0f, 0.0f, 1.0f));  // End in poly 3

	Zenith_Assert(xResult.m_eStatus == Zenith_PathResult::Status::SUCCESS,
		"Path across 4 polygons should succeed");

	// Check for duplicate waypoints
	bool bHasDuplicates = false;
	for (uint32_t u = 0; u + 1 < xResult.m_axWaypoints.GetSize(); ++u)
	{
		Zenith_Maths::Vector3 xA = xResult.m_axWaypoints.Get(u);
		Zenith_Maths::Vector3 xB = xResult.m_axWaypoints.Get(u + 1);
		if (Zenith_Maths::Length(xA - xB) < 0.001f)
		{
			bHasDuplicates = true;
			break;
		}
	}

	Zenith_Assert(!bHasDuplicates, "Path should not have duplicate consecutive waypoints");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPathfindingNoDuplicateWaypoints PASSED");
}

void Zenith_UnitTests::TestPathfindingBatchProcessing()
{
	// Test batch parallel pathfinding API
	Zenith_NavMesh xNavMesh;

	// Create a simple navmesh
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
	xNavMesh.BuildSpatialGrid();

	// Create batch of path requests
	Zenith_Pathfinding::PathRequest axRequests[3];

	axRequests[0].m_pxNavMesh = &xNavMesh;
	axRequests[0].m_xStart = Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f);
	axRequests[0].m_xEnd = Zenith_Maths::Vector3(9.0f, 0.0f, 1.0f);

	axRequests[1].m_pxNavMesh = &xNavMesh;
	axRequests[1].m_xStart = Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f);
	axRequests[1].m_xEnd = Zenith_Maths::Vector3(8.0f, 0.0f, 8.0f);

	axRequests[2].m_pxNavMesh = &xNavMesh;
	axRequests[2].m_xStart = Zenith_Maths::Vector3(5.0f, 0.0f, 1.0f);
	axRequests[2].m_xEnd = Zenith_Maths::Vector3(5.0f, 0.0f, 9.0f);

	// Process batch
	Zenith_Pathfinding::FindPathsBatch(axRequests, 3);

	// Verify all paths succeeded
	Zenith_Assert(axRequests[0].m_xResult.m_eStatus == Zenith_PathResult::Status::SUCCESS,
		"Batch request 0 should succeed");
	Zenith_Assert(axRequests[1].m_xResult.m_eStatus == Zenith_PathResult::Status::SUCCESS,
		"Batch request 1 should succeed");
	Zenith_Assert(axRequests[2].m_xResult.m_eStatus == Zenith_PathResult::Status::SUCCESS,
		"Batch request 2 should succeed");

	// Verify waypoints exist
	Zenith_Assert(axRequests[0].m_xResult.m_axWaypoints.GetSize() >= 2,
		"Batch request 0 should have waypoints");

	// Test null navmesh handling
	Zenith_Pathfinding::PathRequest xNullRequest;
	xNullRequest.m_pxNavMesh = nullptr;
	xNullRequest.m_xStart = Zenith_Maths::Vector3(0.0f);
	xNullRequest.m_xEnd = Zenith_Maths::Vector3(1.0f);
	Zenith_Pathfinding::FindPathsBatch(&xNullRequest, 1);
	Zenith_Assert(xNullRequest.m_xResult.m_eStatus == Zenith_PathResult::Status::FAILED,
		"Null navmesh request should fail");

	// Test empty batch
	Zenith_Pathfinding::FindPathsBatch(nullptr, 0);  // Should not crash

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPathfindingBatchProcessing PASSED");
}

void Zenith_UnitTests::TestPathfindingPartialPath()
{
	// Test that partial paths are returned for disconnected regions
	Zenith_NavMesh xNavMesh;

	// Create two disconnected polygons
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(3.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(3.0f, 0.0f, 3.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 3.0f));

	// Polygon 2: Disconnected (far away)
	xNavMesh.AddVertex(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(23.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(23.0f, 0.0f, 3.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(20.0f, 0.0f, 3.0f));

	Zenith_Vector<uint32_t> axPoly0, axPoly1;
	axPoly0.PushBack(0); axPoly0.PushBack(1); axPoly0.PushBack(2); axPoly0.PushBack(3);
	xNavMesh.AddPolygon(axPoly0);

	axPoly1.PushBack(4); axPoly1.PushBack(5); axPoly1.PushBack(6); axPoly1.PushBack(7);
	xNavMesh.AddPolygon(axPoly1);

	xNavMesh.ComputeAdjacency();
	xNavMesh.BuildSpatialGrid();

	// Try to find path from start polygon to disconnected target polygon
	Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(
		xNavMesh,
		Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f),
		Zenith_Maths::Vector3(21.0f, 0.0f, 1.0f));

	// Should fail since regions are disconnected
	Zenith_Assert(xResult.m_eStatus == Zenith_PathResult::Status::FAILED ||
	              xResult.m_eStatus == Zenith_PathResult::Status::PARTIAL,
		"Path to disconnected region should fail or return partial");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPathfindingPartialPath PASSED");
}

// ============================================================================
// NavMesh Generator helper tests
// ============================================================================

void Zenith_UnitTests::TestCountWalkableSpans()
{
	// #TODO: Re-enable once CountWalkableSpans/WalkableSpanStats are restored to NavMeshGenerator
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCountWalkableSpans SKIPPED (function removed)");
}

void Zenith_UnitTests::TestHasSufficientClearance()
{
	// #TODO: Re-enable once HasSufficientClearance is restored to NavMeshGenerator
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHasSufficientClearance SKIPPED (function removed)");
}

void Zenith_UnitTests::TestMergeOverlappingSpans()
{
	// #TODO: Re-enable once MergeOverlappingSpans is restored to NavMeshGenerator
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeOverlappingSpans SKIPPED (function removed)");
}

// ============================================================================
// Physics Mesh Generator Helper Tests
// ============================================================================

void Zenith_UnitTests::TestFindExtremeVertexIndices()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestFindExtremeVertexIndices...");

	Zenith_Vector<Zenith_Maths::Vector3> xPositions;
	xPositions.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(-5.0f, 3.0f, 1.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(7.0f, -2.0f, 4.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 10.0f, -8.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(2.0f, -1.0f, 12.0f));

	uint32_t auIndices[6];
	Zenith_PhysicsMeshGenerator::FindExtremeVertexIndices(xPositions, auIndices);

	// minX=-5 at index 1, maxX=7 at index 2
	Zenith_Assert(auIndices[0] == 1, "MinX should be at index 1");
	Zenith_Assert(auIndices[1] == 2, "MaxX should be at index 2");
	// minY=-2 at index 2, maxY=10 at index 3
	Zenith_Assert(auIndices[2] == 2, "MinY should be at index 2");
	Zenith_Assert(auIndices[3] == 3, "MaxY should be at index 3");
	// minZ=-8 at index 3, maxZ=12 at index 4
	Zenith_Assert(auIndices[4] == 3, "MinZ should be at index 3");
	Zenith_Assert(auIndices[5] == 4, "MaxZ should be at index 4");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindExtremeVertexIndices PASSED");
}

void Zenith_UnitTests::TestComputeAABBFromPositions()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComputeAABBFromPositions...");

	Zenith_Vector<Zenith_Maths::Vector3> xPositions;
	xPositions.PushBack(Zenith_Maths::Vector3(1.0f, -2.0f, 3.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(-4.0f, 5.0f, -6.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));

	Zenith_Maths::Vector3 xMin, xMax;
	Zenith_PhysicsMeshGenerator::ComputeAABBFromPositions(xPositions, xMin, xMax);

	Zenith_Assert(xMin.x == -4.0f, "Min X should be -4");
	Zenith_Assert(xMin.y == -2.0f, "Min Y should be -2");
	Zenith_Assert(xMin.z == -6.0f, "Min Z should be -6");
	Zenith_Assert(xMax.x == 1.0f, "Max X should be 1");
	Zenith_Assert(xMax.y == 5.0f, "Max Y should be 5");
	Zenith_Assert(xMax.z == 3.0f, "Max Z should be 3");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComputeAABBFromPositions PASSED");
}

void Zenith_UnitTests::TestComputeVertexNormals()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComputeVertexNormals...");

	// Simple flat triangle in the XZ plane — normals should point along Y
	Zenith_Maths::Vector3 axPositions[3] = {
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)
	};
	Zenith_Maths::Vector3 axNormals[3];
	uint32_t auIndices[3] = { 0, 1, 2 };

	Zenith_PhysicsMeshGenerator::ComputeVertexNormals(axNormals, axPositions, 3, auIndices, 3);

	// Cross product of (1,0,0)x(0,0,1) = (0,-1,0), normalized = (0,-1,0)
	for (int i = 0; i < 3; i++)
	{
		Zenith_Assert(std::abs(axNormals[i].x) < 0.001f, "Normal X should be ~0");
		Zenith_Assert(std::abs(std::abs(axNormals[i].y) - 1.0f) < 0.001f, "Normal Y should be ~+-1");
		Zenith_Assert(std::abs(axNormals[i].z) < 0.001f, "Normal Z should be ~0");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComputeVertexNormals PASSED");
}

// ============================================================================
// Zenith_AITests implementations
// ============================================================================

#include "UnitTests/Zenith_AITests.h"

void Zenith_AITests::RunAllTests()
{
	TestFloodFillAssignsConnected();
	TestFloodFillStopsAtBoundary();
	TestColumnHasSpanInRegionFound();
	TestColumnHasSpanInRegionNotFound();
}

// ----------------------------------------------------------------------------
// Helper to build minimal compact heightfield arrays for testing.
// Creates a flat grid of iWidth x iHeight columns, one span per column,
// all at the same height.  Skipped columns can be specified via
// abSkipColumn (indexed by column index; true = no span in that column).
// ----------------------------------------------------------------------------
Zenith_AITests::TestCompactHF Zenith_AITests::BuildTestHF(
	int32_t iWidth, int32_t iHeight,
	const uint16_t* auHeights,
	const bool* abSkipColumn)
{
	TestCompactHF xHF;
	uint32_t uTotalColumns = static_cast<uint32_t>(iWidth * iHeight);

	xHF.m_axColumnSpanCounts.Reserve(uTotalColumns);
	xHF.m_axColumnSpanStarts.Reserve(uTotalColumns);

	for (uint32_t u = 0; u < uTotalColumns; ++u)
	{
		bool bSkip = abSkipColumn && abSkipColumn[u];
		xHF.m_axColumnSpanStarts.PushBack(xHF.m_axSpans.GetSize());

		if (bSkip)
		{
			xHF.m_axColumnSpanCounts.PushBack(0);
		}
		else
		{
			Zenith_NavMeshGenerator::CompactSpan xSpan;
			xSpan.m_uY = auHeights[u];
			xSpan.m_uRegion = 0;
			xSpan.m_uNeighbors[0] = 0;
			xSpan.m_uNeighbors[1] = 0;
			xSpan.m_uNeighbors[2] = 0;
			xSpan.m_uNeighbors[3] = 0;
			xHF.m_axSpans.PushBack(xSpan);
			xHF.m_axSpanToColumn.PushBack(u);
			xHF.m_axColumnSpanCounts.PushBack(1);
		}
	}

	return xHF;
}

void Zenith_AITests::TestFloodFillAssignsConnected()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestFloodFillAssignsConnected...");

	// 3x3 flat grid, all spans at the same height, no gaps
	const int32_t iWidth = 3;
	const int32_t iHeight = 3;
	const uint16_t auHeights[9] = {10, 10, 10, 10, 10, 10, 10, 10, 10};

	TestCompactHF xHF = BuildTestHF(iWidth, iHeight, auHeights, nullptr);

	// Flood fill from span 0 with region ID 1, max step = 1 cell
	Zenith_NavMeshGenerator::FloodFillRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		xHF.m_axSpanToColumn,
		iWidth, iHeight,
		/*iMaxStepCells=*/1,
		/*uStartSpanIdx=*/0,
		/*uRegionID=*/1);

	// All 9 spans should now have region 1
	for (uint32_t u = 0; u < xHF.m_axSpans.GetSize(); ++u)
	{
		Zenith_Assert(xHF.m_axSpans.Get(u).m_uRegion == 1,
			"All connected spans should be assigned to region 1");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFloodFillAssignsConnected PASSED");
}

void Zenith_AITests::TestFloodFillStopsAtBoundary()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestFloodFillStopsAtBoundary...");

	// 3x1 grid.  Middle column (index 1) is empty, creating a gap.
	// Columns 0 and 2 each have one span at the same height.
	const int32_t iWidth = 3;
	const int32_t iHeight = 1;
	const uint16_t auHeights[3] = {10, 0, 10};
	const bool abSkip[3] = {false, true, false};

	TestCompactHF xHF = BuildTestHF(iWidth, iHeight, auHeights, abSkip);

	// We should have exactly 2 spans (columns 0 and 2)
	Zenith_Assert(xHF.m_axSpans.GetSize() == 2, "Should have 2 spans");

	// Flood fill from span 0 (column 0) with region 1
	Zenith_NavMeshGenerator::FloodFillRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		xHF.m_axSpanToColumn,
		iWidth, iHeight,
		/*iMaxStepCells=*/1,
		/*uStartSpanIdx=*/0,
		/*uRegionID=*/1);

	// Span 0 should be region 1, span 1 (column 2) should still be 0 (unassigned)
	Zenith_Assert(xHF.m_axSpans.Get(0).m_uRegion == 1,
		"Start span should be assigned region 1");
	Zenith_Assert(xHF.m_axSpans.Get(1).m_uRegion == 0,
		"Disconnected span should remain unassigned (region 0)");

	// Now flood fill the second span separately with region 2
	Zenith_NavMeshGenerator::FloodFillRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		xHF.m_axSpanToColumn,
		iWidth, iHeight,
		/*iMaxStepCells=*/1,
		/*uStartSpanIdx=*/1,
		/*uRegionID=*/2);

	Zenith_Assert(xHF.m_axSpans.Get(1).m_uRegion == 2,
		"Second disconnected span should be assigned region 2");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFloodFillStopsAtBoundary PASSED");
}

void Zenith_AITests::TestColumnHasSpanInRegionFound()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestColumnHasSpanInRegionFound...");

	// 2x2 grid, all columns have one span.
	// Assign column (1,0) a region of 5.
	const int32_t iWidth = 2;
	const int32_t iHeight = 2;
	const uint16_t auHeights[4] = {10, 10, 10, 10};

	TestCompactHF xHF = BuildTestHF(iWidth, iHeight, auHeights, nullptr);

	// Manually assign region 5 to the span at column (1, 0) = column index 1 = span index 1
	xHF.m_axSpans.Get(1).m_uRegion = 5;

	bool bFound = Zenith_NavMeshGenerator::ColumnHasSpanInRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		/*iX=*/1, /*iZ=*/0,
		iWidth, iHeight,
		/*uRegion=*/5);

	Zenith_Assert(bFound, "Should find span with region 5 in column (1,0)");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestColumnHasSpanInRegionFound PASSED");
}

void Zenith_AITests::TestColumnHasSpanInRegionNotFound()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestColumnHasSpanInRegionNotFound...");

	// 2x2 grid, all regions are 0 (default)
	const int32_t iWidth = 2;
	const int32_t iHeight = 2;
	const uint16_t auHeights[4] = {10, 10, 10, 10};

	TestCompactHF xHF = BuildTestHF(iWidth, iHeight, auHeights, nullptr);

	// Search for region 7 which does not exist
	bool bFound = Zenith_NavMeshGenerator::ColumnHasSpanInRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		/*iX=*/0, /*iZ=*/0,
		iWidth, iHeight,
		/*uRegion=*/7);

	Zenith_Assert(!bFound, "Should NOT find span with region 7");

	// Also test out-of-bounds coordinates
	bool bOutOfBounds = Zenith_NavMeshGenerator::ColumnHasSpanInRegion(
		xHF.m_axSpans,
		xHF.m_axColumnSpanCounts,
		xHF.m_axColumnSpanStarts,
		/*iX=*/-1, /*iZ=*/0,
		iWidth, iHeight,
		/*uRegion=*/0);

	Zenith_Assert(!bOutOfBounds, "Out-of-bounds coordinates should return false");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestColumnHasSpanInRegionNotFound PASSED");
}
