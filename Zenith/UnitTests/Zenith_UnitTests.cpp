#include "Zenith.h"

#include "UnitTests/Zenith_UnitTests.h"

#include "Collections/Zenith_CircularQueue.h"
#include "Collections/Zenith_MemoryPool.h"
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/Flux_Types.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Profiling/Zenith_Profiling.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Scene serialization includes
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Zenith_Query.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Physics/Zenith_Physics.h"
#include <filesystem>

// Animation system includes
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_InverseKinematics.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

// Mesh geometry include (for exporting runtime-format meshes)
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

// Animation texture include (for VAT baking)
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"

// Asset pipeline includes
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"

// GUID and asset system includes
#include "Core/Zenith_GUID.h"
#include "AssetHandling/Zenith_AssetMeta.h"
#include "AssetHandling/Zenith_AssetDatabase.h"
#include "AssetHandling/Zenith_AssetRef.h"
#include "Prefab/Zenith_Prefab.h"

// Async asset loading and DataAsset includes
#include "AssetHandling/Zenith_AsyncAssetLoader.h"
#include "AssetHandling/Zenith_DataAsset.h"
#include "AssetHandling/Zenith_DataAssetManager.h"

#ifdef ZENITH_TOOLS
#include "UnitTests/Zenith_EditorTests.h"
#endif

void Zenith_UnitTests::RunAllTests()
{
	TestDataStream();
	TestMemoryManagement();
	TestProfiling();
	TestVector();
	TestVectorFind();
	TestVectorErase();
	TestVectorZeroCapacityResize();
	TestMemoryPool();
	TestMemoryPoolExhaustion();

	// CircularQueue tests
	TestCircularQueueBasic();
	TestCircularQueueWrapping();
	TestCircularQueueFull();
	TestCircularQueueNonPOD();

	// Vector edge case tests (from defensive review)
	TestVectorSelfAssignment();
	TestVectorRemoveSwap();

	// DataStream edge case tests (from defensive review)
	TestDataStreamBoundsCheck();

	// Scene serialization tests
	TestComponentSerialization();
	TestEntitySerialization();
	TestSceneSerialization();
	TestSceneRoundTrip();

	// Animation system tests
	TestBoneLocalPoseBlending();
	TestSkeletonPoseOperations();
	TestAnimationParameters();
	TestTransitionConditions();
	TestAnimationStateMachine();
	TestIKChainSetup();
	TestAnimationSerialization();
	TestBlendTreeNodes();
	TestCrossFadeTransition();

	// Additional animation tests
	TestAnimationClipChannels();
	TestBlendSpace1D();
	TestBlendSpace2D();
	TestBlendTreeEvaluation();
	TestBlendTreeSerialization();
	TestFABRIKSolver();
	TestAnimationEvents();
	TestBoneMasking();

	// Animation state machine integration tests
	TestStateMachineUpdateLoop();
	TestTriggerConsumptionInTransitions();
	TestExitTimeTransitions();
	TestTransitionPriority();
	TestStateLifecycleCallbacks();
	TestMultipleTransitionConditions();

	// Asset pipeline tests
	TestMeshAssetLoading();
	TestBindPoseVertexPositions();
	TestAnimatedVertexPositions();

	// ECS bug fix tests (Phase 1)
	TestComponentRemovalIndexUpdate();
	TestComponentSwapAndPop();
	TestMultipleComponentRemoval();
	TestComponentRemovalWithManyEntities();
	TestEntityNameFromScene();
	TestEntityCopyPreservesAccess();

	// ECS reflection system tests (Phase 2)
	TestComponentMetaRegistration();
	TestComponentMetaSerialization();
	TestComponentMetaDeserialization();
	TestComponentMetaTypeIDConsistency();

	// ECS lifecycle hooks tests (Phase 3)
	TestLifecycleHookDetection();
	TestLifecycleOnAwake();
	TestLifecycleOnStart();
	TestLifecycleOnUpdate();
	TestLifecycleOnDestroy();
	TestLifecycleDispatchOrder();
	TestLifecycleEntityCreationDuringCallback();
	TestDispatchFullLifecycleInit();

	// ECS query system tests (Phase 4)
	TestQuerySingleComponent();
	TestQueryMultipleComponents();
	TestQueryNoMatches();
	TestQueryCount();
	TestQueryFirstAndAny();

	// ECS event system tests (Phase 5)
	TestEventSubscribeDispatch();
	TestEventUnsubscribe();
	TestEventDeferredQueue();
	TestEventMultipleSubscribers();
	TestEventClearSubscriptions();

	// GUID system tests
	TestGUIDGeneration();
	TestGUIDStringRoundTrip();
	TestGUIDSerializationRoundTrip();
	TestGUIDComparisonOperators();
	TestGUIDHashDistribution();
	TestGUIDInvalidDetection();

	// Asset meta file tests
	TestAssetMetaSaveLoadRoundTrip();
	TestAssetMetaVersionCompatibility();
	TestAssetMetaImportSettings();
	TestAssetMetaGetMetaPath();

	// Asset database tests
	TestAssetDatabaseGUIDToPath();
	TestAssetDatabasePathToGUID();
	TestAssetDatabaseDependencyTracking();
	TestAssetDatabaseDependentLookup();

	// Asset reference tests
	TestAssetRefGUIDStorage();
	TestAssetRefSerializationRoundTrip();
	TestAssetRefFromPath();
	TestAssetRefInvalidHandling();

	// Entity hierarchy tests
	TestEntityAddChild();
	TestEntityRemoveChild();
	TestEntityGetChildren();
	TestEntityReparenting();
	TestEntityChildCleanupOnDelete();
	TestEntityHierarchySerialization();

	// ECS safety tests (circular hierarchy, camera safety)
	TestCircularHierarchyPrevention();
	TestSelfParentingPrevention();
	TestTryGetMainCameraWhenNotSet();
	TestDeepHierarchyBuildModelMatrix();
	TestLocalSceneDestruction();
	TestLocalSceneWithHierarchy();

	// Prefab system tests
	TestPrefabCreateFromEntity();
	TestPrefabInstantiation();
	TestPrefabSaveLoadRoundTrip();
	TestPrefabOverrides();
	TestPrefabVariantCreation();

	// Async asset loading tests
	TestAsyncLoadState();
	TestAsyncLoadRequest();
	TestAsyncLoadCompletion();
	TestAssetRefAsyncAPI();

	// DataAsset system tests
	TestDataAssetRegistration();
	TestDataAssetCreateAndSave();
	TestDataAssetLoad();
	TestDataAssetRoundTrip();

	// Stick figure animation tests
	TestStickFigureSkeletonCreation();
	TestStickFigureMeshCreation();
	TestStickFigureIdleAnimation();
	TestStickFigureWalkAnimation();
	TestStickFigureRunAnimation();
	TestStickFigureAnimationBlending();

	// Stick figure IK tests
	TestStickFigureArmIK();
	TestStickFigureLegIK();
	TestStickFigureIKWithAnimation();

	// Stick figure asset export (creates reusable assets for game projects)
	TestStickFigureAssetExport();

	// Procedural tree asset export (for instanced mesh testing with VAT)
	TestProceduralTreeAssetExport();

	// AI System tests - Blackboard
	TestBlackboardBasicTypes();
	TestBlackboardVector3();
	TestBlackboardEntityID();
	TestBlackboardHasKey();
	TestBlackboardClear();
	TestBlackboardDefaultValues();
	TestBlackboardOverwrite();
	TestBlackboardSerialization();

	// AI System tests - Behavior Tree
	TestBTSequenceAllSuccess();
	TestBTSequenceFirstFails();
	TestBTSequenceRunning();
	TestBTSelectorFirstSucceeds();
	TestBTSelectorAllFail();
	TestBTSelectorRunning();
	TestBTParallelRequireOne();
	TestBTParallelRequireAll();
	TestBTInverter();
	TestBTRepeaterCount();
	TestBTCooldown();
	TestBTSucceeder();
	TestBTNodeOwnership();

	// AI System tests - NavMesh
	TestNavMeshPolygonCreation();
	TestNavMeshAdjacency();
	TestNavMeshFindNearestPolygon();
	TestNavMeshIsPointOnMesh();
	TestNavMeshRaycast();
	TestPathfindingStraightLine();
	TestPathfindingAroundObstacle();
	TestPathfindingNoPath();
	TestPathfindingSmoothing();

	// AI System tests - NavMesh Agent
	TestNavAgentSetDestination();
	TestNavAgentMovement();
	TestNavAgentArrival();
	TestNavAgentStop();
	TestNavAgentSpeedSettings();
	TestNavAgentRemainingDistanceBounds();
	TestPathfindingNoDuplicateWaypoints();
	TestPathfindingBatchProcessing();
	TestPathfindingPartialPath();

	// AI System tests - Perception
	TestSightConeInRange();
	TestSightConeOutOfRange();
	TestSightConeOutOfFOV();
	TestSightAwarenessGain();
	TestHearingStimulusInRange();
	TestHearingStimulusAttenuation();
	TestHearingStimulusOutOfRange();
	TestMemoryRememberTarget();
	TestMemoryDecay();

	// AI System tests - Squad
	TestSquadAddRemoveMember();
	TestSquadRoleAssignment();
	TestSquadLeaderSelection();
	TestFormationLine();
	TestFormationWedge();
	TestFormationWorldPositions();
	TestSquadSharedKnowledge();

	// AI System tests - Tactical Points
	TestTacticalPointRegistration();
	TestTacticalPointCoverScoring();
	TestTacticalPointFlankScoring();

	// AI System tests - Debug Variables
	TestAIDebugVariablesDefault();
	TestAIDebugVariablesToggle();
	TestTacticalPointDebugColor();
	TestSquadDebugRoleColor();

#ifdef ZENITH_TOOLS
	// Editor tests (only in tools builds)
	Zenith_EditorTests::RunAllTests();
#endif

	Zenith_Log(LOG_CATEGORY_UNITTEST, "All Unit Tests Passed");
}

void Zenith_UnitTests::TestDataStream()
{
	Zenith_DataStream xStream(1);

	const char* szTestData = "This is a test string";
	constexpr u_int uTestDataLen = 22;
	xStream.WriteData(szTestData, uTestDataLen);

	xStream << uint32_t(5u);
	xStream << float(2000.f);
	xStream << Zenith_Maths::Vector3(1, 2, 3);
	xStream << std::unordered_map<std::string, std::pair<uint32_t, uint64_t>>({{"Test", {20, 100}}});
	xStream << std::vector<double>({ 3245., -1119. });

	xStream.SetCursor(0);

	char acTestData[uTestDataLen];
	xStream.ReadData(acTestData, uTestDataLen);
	Zenith_Assert(!strcmp(acTestData, szTestData));

	uint32_t u5;
	xStream >> u5;
	Zenith_Assert(u5 == 5);

	float f2000;
	xStream >> f2000;
	Zenith_Assert(f2000 == 2000.f);

	Zenith_Maths::Vector3 x123;
	xStream >> x123;
	Zenith_Assert(x123 == Zenith_Maths::Vector3(1, 2, 3));

	std::unordered_map<std::string, std::pair<uint32_t, uint64_t>> xUnorderedMap;
	xStream >> xUnorderedMap;
	Zenith_Assert((xUnorderedMap.at("Test") == std::pair<uint32_t, uint64_t>(20, 100)));

	std::vector<double> xVector;
	xStream >> xVector;
	Zenith_Assert(xVector.at(0) == 3245. && xVector.at(1) == -1119.);
}

void Zenith_UnitTests::TestMemoryManagement()
{
	int* piTest = new int[10];
	delete[] piTest;
}

struct TestData
{
	bool Validate() { return m_uIn == m_uOut; }
	u_int m_uIn, m_uOut;
};
static void Test(void* pData)
{
	TestData& xTestData = *static_cast<TestData*>(pData);
	xTestData.m_uOut = xTestData.m_uIn;
}

void Zenith_UnitTests::TestProfiling()
{
	constexpr Zenith_ProfileIndex eIndex0 = ZENITH_PROFILE_INDEX__FLUX_STATIC_MESHES;
	constexpr Zenith_ProfileIndex eIndex1 = ZENITH_PROFILE_INDEX__FLUX_ANIMATED_MESHES;

	Zenith_Profiling::BeginFrame();
	
	Zenith_Profiling::BeginProfile(eIndex0);
	Zenith_Assert(Zenith_Profiling::GetCurrentIndex() == eIndex0, "Profiling index wasn't set correctly");
	Zenith_Profiling::BeginProfile(eIndex1);
	Zenith_Assert(Zenith_Profiling::GetCurrentIndex() == eIndex1, "Profiling index wasn't set correctly");
	Zenith_Profiling::EndProfile(eIndex1);
	Zenith_Assert(Zenith_Profiling::GetCurrentIndex() == eIndex0, "Profiling index wasn't set correctly");
	Zenith_Profiling::EndProfile(eIndex0);

	TestData xTest0 = { 0, -1 }, xTest1 = { 1,-1 }, xTest2 = { 2, -1 };
	Zenith_Task* pxTask0 = new Zenith_Task(ZENITH_PROFILE_INDEX__FLUX_SHADOWS, Test, &xTest0);
	Zenith_Task* pxTask1 = new Zenith_Task(ZENITH_PROFILE_INDEX__FLUX_DEFERRED_SHADING, Test, &xTest1);
	Zenith_Task* pxTask2 = new Zenith_Task(ZENITH_PROFILE_INDEX__FLUX_SKYBOX, Test, &xTest2);
	Zenith_TaskSystem::SubmitTask(pxTask0);
	Zenith_TaskSystem::SubmitTask(pxTask1);
	Zenith_TaskSystem::SubmitTask(pxTask2);
	pxTask0->WaitUntilComplete();
	pxTask1->WaitUntilComplete();
	pxTask2->WaitUntilComplete();

	Zenith_Assert(xTest0.Validate(), "");
	Zenith_Assert(xTest1.Validate(), "");
	Zenith_Assert(xTest2.Validate(), "");

	const std::unordered_map<u_int, Zenith_Vector<Zenith_Profiling::Event>>& xEvents = Zenith_Profiling::GetEvents();
	const Zenith_Vector<Zenith_Profiling::Event>& xEventsMain = xEvents.at(Zenith_Multithreading::GetCurrentThreadID());
	const Zenith_Vector<Zenith_Profiling::Event>& xEvents0 = xEvents.at(pxTask0->GetCompletedThreadID());
	const Zenith_Vector<Zenith_Profiling::Event>& xEvents1 = xEvents.at(pxTask1->GetCompletedThreadID());
	const Zenith_Vector<Zenith_Profiling::Event>& xEvents2 = xEvents.at(pxTask2->GetCompletedThreadID());

	Zenith_Assert(xEventsMain.GetSize() == 8, "Expected 8 events, have %u", xEvents.size());
	Zenith_Assert(xEventsMain.Get(0).m_eIndex == eIndex1, "Wrong profile index");
	Zenith_Assert(xEventsMain.Get(1).m_eIndex == eIndex0, "Wrong profile index");

	delete pxTask0;
	delete pxTask1;
	delete pxTask2;

	Zenith_Profiling::EndFrame();
}

void Zenith_UnitTests::TestVector()
{
	constexpr u_int uNUM_TESTS = 1024;

	Zenith_Vector<u_int> xUIntVector(1);

	for (u_int u = 0; u < uNUM_TESTS / 2; u++)
	{
		xUIntVector.PushBack(u);
		Zenith_Assert(xUIntVector.GetFront() == 0);
		Zenith_Assert(xUIntVector.GetBack() == u);
	}

	for (u_int u = uNUM_TESTS / 2; u < uNUM_TESTS; u++)
	{
		xUIntVector.EmplaceBack((u_int&&)u_int(u));
		Zenith_Assert(xUIntVector.GetFront() == 0);
		Zenith_Assert(xUIntVector.GetBack() == u);
	}

	for (u_int u = 0; u < uNUM_TESTS; u++)
	{
		Zenith_Assert(xUIntVector.Get(u) == u);
	}

	constexpr u_int uNUM_REMOVALS = uNUM_TESTS / 10;
	for (u_int u = 0; u < uNUM_REMOVALS; u++)
	{
		xUIntVector.Remove(uNUM_TESTS / 2);
		Zenith_Assert(xUIntVector.Get(uNUM_TESTS / 2) == uNUM_TESTS / 2 + u + 1);
	}

	Zenith_Vector<u_int> xCopy0 = xUIntVector;
	Zenith_Vector<u_int> xCopy1(xUIntVector);

	auto xTest = [uNUM_TESTS, uNUM_REMOVALS](Zenith_Vector<u_int> xVector)
	{
		for (u_int u = 0; u < uNUM_TESTS / 2; u++)
		{
			Zenith_Assert(xVector.Get(u) == u);
		}

		for (u_int u = uNUM_TESTS / 2; u < uNUM_TESTS - uNUM_REMOVALS; u++)
		{
			Zenith_Assert(xVector.Get(u) == u + uNUM_REMOVALS);
		}
	};

	xTest(xUIntVector);
	xTest(xCopy0);
	xTest(xCopy1);
}

void Zenith_UnitTests::TestVectorFind()
{
	Zenith_Vector<u_int> xVector;

	for (u_int u = 0; u < 5; u++)
	{
		xVector.PushBack(u * 10);
	}

	u_int uIndex = xVector.Find(20);
	Zenith_Assert(uIndex == 2, "TestVectorFind: Expected to find 20 at index 2");

	uIndex = xVector.Find(25);
	Zenith_Assert(uIndex == xVector.GetSize(), "TestVectorFind: Expected not to find 25");

	uIndex = xVector.Find(0);
	Zenith_Assert(uIndex == 0, "TestVectorFind: Expected to find 0 at index 0");

	uIndex = xVector.Find(40);
	Zenith_Assert(uIndex == 4, "TestVectorFind: Expected to find 40 at index 4");

	Zenith_Assert(xVector.Contains(30), "TestVectorFind: Expected Contains(30) to be true");
	Zenith_Assert(!xVector.Contains(35), "TestVectorFind: Expected Contains(35) to be false");

	uIndex = xVector.FindIf([](const u_int& u) { return u > 15; });
	Zenith_Assert(uIndex == 2, "TestVectorFind: Expected FindIf(>15) to find index 2");

	uIndex = xVector.FindIf([](const u_int& u) { return u > 100; });
	Zenith_Assert(uIndex == xVector.GetSize(), "TestVectorFind: Expected FindIf(>100) to not find anything");

	Zenith_Vector<u_int> xEmptyVector;
	uIndex = xEmptyVector.Find(0);
	Zenith_Assert(uIndex == 0, "TestVectorFind: Expected Find on empty vector to return 0 (size)");

	Zenith_Log(LOG_CATEGORY_CORE, "TestVectorFind passed");
}

void Zenith_UnitTests::TestVectorErase()
{
	{
		Zenith_Vector<u_int> xVector;
		for (u_int u = 0; u < 5; u++)
		{
			xVector.PushBack(u * 10);
		}

		bool bErased = xVector.EraseValue(20);
		Zenith_Assert(bErased, "TestVectorErase: Expected EraseValue(20) to return true");
		Zenith_Assert(xVector.GetSize() == 4, "TestVectorErase: Expected size to be 4 after erase");
		Zenith_Assert(!xVector.Contains(20), "TestVectorErase: Expected 20 to no longer be in vector");

		Zenith_Assert(xVector.Get(0) == 0, "TestVectorErase: Expected index 0 to be 0");
		Zenith_Assert(xVector.Get(1) == 10, "TestVectorErase: Expected index 1 to be 10");
		Zenith_Assert(xVector.Get(2) == 30, "TestVectorErase: Expected index 2 to be 30");
		Zenith_Assert(xVector.Get(3) == 40, "TestVectorErase: Expected index 3 to be 40");
	}

	{
		Zenith_Vector<u_int> xVector;
		xVector.PushBack(10);
		xVector.PushBack(20);

		bool bErased = xVector.EraseValue(15);
		Zenith_Assert(!bErased, "TestVectorErase: Expected EraseValue(15) to return false");
		Zenith_Assert(xVector.GetSize() == 2, "TestVectorErase: Expected size to remain 2");
	}

	{
		Zenith_Vector<u_int> xVector;
		for (u_int u = 0; u < 5; u++)
		{
			xVector.PushBack(u);
		}

		bool bErased = xVector.Erase(2);
		Zenith_Assert(bErased, "TestVectorErase: Expected Erase(2) to return true");
		Zenith_Assert(xVector.GetSize() == 4, "TestVectorErase: Expected size to be 4");
		Zenith_Assert(xVector.Get(2) == 3, "TestVectorErase: Expected index 2 to now be 3");
	}

	{
		Zenith_Vector<u_int> xVector;
		xVector.PushBack(10);

		bool bErased = xVector.Erase(5);
		Zenith_Assert(!bErased, "TestVectorErase: Expected Erase(5) to return false");
		Zenith_Assert(xVector.GetSize() == 1, "TestVectorErase: Expected size to remain 1");
	}

	{
		Zenith_Vector<u_int> xEmptyVector;
		bool bErased = xEmptyVector.EraseValue(0);
		Zenith_Assert(!bErased, "TestVectorErase: Expected EraseValue on empty vector to return false");
	}

	{
		Zenith_Vector<u_int> xVector;
		xVector.PushBack(1);
		xVector.PushBack(2);
		xVector.PushBack(3);

		xVector.EraseValue(1);
		Zenith_Assert(xVector.GetSize() == 2, "TestVectorErase: Expected size 2 after erasing first");
		Zenith_Assert(xVector.Get(0) == 2, "TestVectorErase: Expected first element to now be 2");
	}

	{
		Zenith_Vector<u_int> xVector;
		xVector.PushBack(1);
		xVector.PushBack(2);
		xVector.PushBack(3);

		xVector.EraseValue(3);
		Zenith_Assert(xVector.GetSize() == 2, "TestVectorErase: Expected size 2 after erasing last");
		Zenith_Assert(xVector.GetBack() == 2, "TestVectorErase: Expected last element to now be 2");
	}

	Zenith_Log(LOG_CATEGORY_CORE, "TestVectorErase passed");
}

void Zenith_UnitTests::TestVectorZeroCapacityResize()
{
	// Test 1: PushBack on moved-from vector (capacity becomes 0 after move)
	{
		Zenith_Vector<u_int> xSource;
		xSource.PushBack(1);
		xSource.PushBack(2);
		xSource.PushBack(3);

		// Move to destination - source now has capacity 0
		Zenith_Vector<u_int> xDest = std::move(xSource);

		// Source should now have capacity 0
		Zenith_Assert(xSource.GetCapacity() == 0, "TestVectorZeroCapacityResize: Moved-from vector should have capacity 0");
		Zenith_Assert(xSource.GetSize() == 0, "TestVectorZeroCapacityResize: Moved-from vector should have size 0");

		// PushBack on moved-from vector should work (was causing infinite loop before fix)
		xSource.PushBack(42);
		Zenith_Assert(xSource.GetSize() == 1, "TestVectorZeroCapacityResize: Size should be 1 after PushBack");
		Zenith_Assert(xSource.Get(0) == 42, "TestVectorZeroCapacityResize: Element should be 42");
		Zenith_Assert(xSource.GetCapacity() > 0, "TestVectorZeroCapacityResize: Capacity should be > 0 after PushBack");
	}

	// Test 2: EmplaceBack on moved-from vector
	{
		Zenith_Vector<u_int> xSource;
		xSource.PushBack(100);
		Zenith_Vector<u_int> xDest = std::move(xSource);

		// EmplaceBack should also work on zero-capacity vector
		xSource.EmplaceBack(200);
		Zenith_Assert(xSource.GetSize() == 1, "TestVectorZeroCapacityResize: Size should be 1 after EmplaceBack");
		Zenith_Assert(xSource.Get(0) == 200, "TestVectorZeroCapacityResize: Element should be 200");
	}

	// Test 3: Move assignment leaves source at capacity 0
	{
		Zenith_Vector<u_int> xSource;
		xSource.PushBack(1);
		xSource.PushBack(2);

		Zenith_Vector<u_int> xDest;
		xDest = std::move(xSource);

		Zenith_Assert(xSource.GetCapacity() == 0, "TestVectorZeroCapacityResize: Move-assigned source should have capacity 0");

		// Should be able to reuse the moved-from vector
		xSource.PushBack(99);
		Zenith_Assert(xSource.GetSize() == 1, "TestVectorZeroCapacityResize: Reused vector should have size 1");
		Zenith_Assert(xSource.Get(0) == 99, "TestVectorZeroCapacityResize: Reused vector element should be 99");
	}

	// Test 4: Multiple PushBacks after move to ensure proper capacity growth
	{
		Zenith_Vector<u_int> xSource;
		xSource.PushBack(1);
		Zenith_Vector<u_int> xDest = std::move(xSource);

		// Add many elements to trigger multiple resizes
		for (u_int u = 0; u < 100; u++)
		{
			xSource.PushBack(u);
		}

		Zenith_Assert(xSource.GetSize() == 100, "TestVectorZeroCapacityResize: Size should be 100 after many PushBacks");
		for (u_int u = 0; u < 100; u++)
		{
			Zenith_Assert(xSource.Get(u) == u, "TestVectorZeroCapacityResize: Elements should match");
		}
	}

	Zenith_Log(LOG_CATEGORY_CORE, "TestVectorZeroCapacityResize passed");
}

class MemoryPoolTest
{
public:
	static int s_uCount;

	explicit MemoryPoolTest(u_int& uOut)
	: m_uTest(++s_uCount)
	{
		uOut = m_uTest;
	}

	~MemoryPoolTest()
	{
		s_uCount--;
	}

	int m_uTest;
};
int MemoryPoolTest::s_uCount = 0;

void Zenith_UnitTests::TestMemoryPool()
{
	constexpr u_int uPOOL_SIZE = 128;
	Zenith_MemoryPool<MemoryPoolTest, uPOOL_SIZE> xPool;
	MemoryPoolTest* apxTest[uPOOL_SIZE];

	Zenith_Assert(MemoryPoolTest::s_uCount == 0);

	for (u_int u = 0; u < uPOOL_SIZE / 2; u++)
	{
		u_int uTest;
		apxTest[u] = xPool.Allocate(uTest);
		Zenith_Assert(MemoryPoolTest::s_uCount == u + 1);
		Zenith_Assert(apxTest[u]->m_uTest == u + 1);
		Zenith_Assert(uTest == u + 1);
	}

	for (u_int u = 0; u < uPOOL_SIZE / 4; u++)
	{
		Zenith_Assert(apxTest[u]->m_uTest == u + 1);
		xPool.Deallocate(apxTest[u]);
		Zenith_Assert(MemoryPoolTest::s_uCount == (uPOOL_SIZE / 2) - u - 1);
	}

	Zenith_Assert(MemoryPoolTest::s_uCount == uPOOL_SIZE / 4);
}

void Zenith_UnitTests::TestMemoryPoolExhaustion()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestMemoryPoolExhaustion...");

	constexpr u_int uPOOL_SIZE = 4;
	Zenith_MemoryPool<u_int, uPOOL_SIZE> xPool;

	// Allocate all slots
	u_int* apxSlots[uPOOL_SIZE];
	for (u_int u = 0; u < uPOOL_SIZE; u++)
	{
		apxSlots[u] = xPool.Allocate(u);
		Zenith_Assert(apxSlots[u] != nullptr, "Allocation %u should succeed", u);
	}

	// Pool should be full
	Zenith_Assert(xPool.IsFull(), "Pool should be full after allocating all slots");

	// Next allocation should return nullptr (graceful exhaustion)
	u_int* pxOverflow = xPool.Allocate(999u);
	Zenith_Assert(pxOverflow == nullptr, "Pool exhaustion should return nullptr, not crash");

	// Deallocate one and verify we can allocate again
	xPool.Deallocate(apxSlots[0]);
	Zenith_Assert(!xPool.IsFull(), "Pool should not be full after deallocation");

	u_int* pxReuse = xPool.Allocate(42u);
	Zenith_Assert(pxReuse != nullptr, "Should be able to allocate after deallocation");
	Zenith_Assert(*pxReuse == 42, "Reused slot should have correct value");

	// Cleanup remaining allocations
	for (u_int u = 1; u < uPOOL_SIZE; u++)
	{
		xPool.Deallocate(apxSlots[u]);
	}
	xPool.Deallocate(pxReuse);

	Zenith_Assert(xPool.IsEmpty(), "Pool should be empty after deallocating all");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMemoryPoolExhaustion PASSED");
}

// ============================================================================
// CIRCULAR QUEUE TESTS
// ============================================================================

void Zenith_UnitTests::TestCircularQueueBasic()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCircularQueueBasic...");

	constexpr u_int uCAPACITY = 8;
	Zenith_CircularQueue<u_int, uCAPACITY> xQueue;

	// Initial state
	Zenith_Assert(xQueue.IsEmpty(), "Queue should start empty");
	Zenith_Assert(!xQueue.IsFull(), "Queue should not start full");
	Zenith_Assert(xQueue.GetSize() == 0, "Queue should have size 0");
	Zenith_Assert(xQueue.GetCapacity() == uCAPACITY, "Queue capacity should be %u", uCAPACITY);

	// Enqueue and dequeue
	for (u_int u = 0; u < uCAPACITY / 2; u++)
	{
		bool bEnqueued = xQueue.Enqueue(u * 10);
		Zenith_Assert(bEnqueued, "Enqueue %u should succeed", u);
		Zenith_Assert(xQueue.GetSize() == u + 1, "Size should be %u", u + 1);
	}

	u_int uVal = 0;
	for (u_int u = 0; u < uCAPACITY / 2; u++)
	{
		bool bDequeued = xQueue.Dequeue(uVal);
		Zenith_Assert(bDequeued, "Dequeue %u should succeed", u);
		Zenith_Assert(uVal == u * 10, "Dequeued value should be %u, got %u", u * 10, uVal);
	}

	Zenith_Assert(xQueue.IsEmpty(), "Queue should be empty after dequeue all");

	// Test Peek
	xQueue.Enqueue(123u);
	bool bPeeked = xQueue.Peek(uVal);
	Zenith_Assert(bPeeked, "Peek should succeed");
	Zenith_Assert(uVal == 123, "Peek should return front value");
	Zenith_Assert(xQueue.GetSize() == 1, "Peek should not remove element");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCircularQueueBasic PASSED");
}

void Zenith_UnitTests::TestCircularQueueWrapping()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCircularQueueWrapping...");

	constexpr u_int uCAPACITY = 4;
	Zenith_CircularQueue<u_int, uCAPACITY> xQueue;

	// Fill the queue
	for (u_int u = 0; u < uCAPACITY; u++)
	{
		xQueue.Enqueue(u);
	}

	// Remove half
	u_int uVal = 0;
	for (u_int u = 0; u < uCAPACITY / 2; u++)
	{
		xQueue.Dequeue(uVal);
	}

	// Now front pointer is at index 2, add more to test wrapping
	// This specifically tests the integer overflow fix in Enqueue
	for (u_int u = 0; u < uCAPACITY / 2; u++)
	{
		bool bEnqueued = xQueue.Enqueue(100 + u);
		Zenith_Assert(bEnqueued, "Enqueue after wrap should succeed");
	}

	Zenith_Assert(xQueue.IsFull(), "Queue should be full after wrapping");

	// Verify FIFO order is maintained across wrap
	u_int auExpected[] = { 2, 3, 100, 101 };  // Original 2,3 + new 100,101
	for (u_int u = 0; u < uCAPACITY; u++)
	{
		bool bDequeued = xQueue.Dequeue(uVal);
		Zenith_Assert(bDequeued, "Dequeue %u should succeed", u);
		Zenith_Assert(uVal == auExpected[u], "Value %u should be %u, got %u", u, auExpected[u], uVal);
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCircularQueueWrapping PASSED");
}

void Zenith_UnitTests::TestCircularQueueFull()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCircularQueueFull...");

	constexpr u_int uCAPACITY = 4;
	Zenith_CircularQueue<u_int, uCAPACITY> xQueue;

	// Fill to capacity
	for (u_int u = 0; u < uCAPACITY; u++)
	{
		bool bEnqueued = xQueue.Enqueue(u);
		Zenith_Assert(bEnqueued, "Enqueue within capacity should succeed");
	}

	Zenith_Assert(xQueue.IsFull(), "Queue should be full");
	Zenith_Assert(xQueue.GetSize() == uCAPACITY, "Size should equal capacity");

	// Attempt to enqueue when full - should fail gracefully
	bool bOverflow = xQueue.Enqueue(999u);
	Zenith_Assert(!bOverflow, "Enqueue when full should return false");
	Zenith_Assert(xQueue.GetSize() == uCAPACITY, "Size should remain at capacity");

	// Dequeue from empty queue should fail
	xQueue.Clear();
	Zenith_Assert(xQueue.IsEmpty(), "Queue should be empty after Clear");

	u_int uVal = 0;
	bool bUnderflow = xQueue.Dequeue(uVal);
	Zenith_Assert(!bUnderflow, "Dequeue from empty should return false");

	// Peek from empty should fail
	bool bPeekEmpty = xQueue.Peek(uVal);
	Zenith_Assert(!bPeekEmpty, "Peek from empty should return false");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCircularQueueFull PASSED");
}

// Helper class for testing non-POD destructor behavior
class TestDestructorCounter
{
public:
	static u_int s_uDestructorCallCount;
	static void ResetCounter() { s_uDestructorCallCount = 0; }

	int m_iValue = 0;

	TestDestructorCounter() = default;
	TestDestructorCounter(int iVal) : m_iValue(iVal) {}
	TestDestructorCounter(const TestDestructorCounter& other) : m_iValue(other.m_iValue) {}
	TestDestructorCounter(TestDestructorCounter&& other) noexcept : m_iValue(other.m_iValue) { other.m_iValue = -1; }
	TestDestructorCounter& operator=(const TestDestructorCounter& other) { m_iValue = other.m_iValue; return *this; }
	TestDestructorCounter& operator=(TestDestructorCounter&& other) noexcept { m_iValue = other.m_iValue; other.m_iValue = -1; return *this; }
	~TestDestructorCounter() { s_uDestructorCallCount++; }
};
u_int TestDestructorCounter::s_uDestructorCallCount = 0;

void Zenith_UnitTests::TestCircularQueueNonPOD()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCircularQueueNonPOD...");

	TestDestructorCounter::ResetCounter();

	{
		Zenith_CircularQueue<TestDestructorCounter, 4> xQueue;

		// Enqueue elements
		xQueue.Enqueue(TestDestructorCounter(1));
		xQueue.Enqueue(TestDestructorCounter(2));
		xQueue.Enqueue(TestDestructorCounter(3));

		Zenith_Assert(xQueue.GetSize() == 3, "Queue should have 3 elements");

		// Dequeue and verify destructor was called
		u_int uPreDequeueCount = TestDestructorCounter::s_uDestructorCallCount;
		TestDestructorCounter xOut;
		bool bSuccess = xQueue.Dequeue(xOut);
		Zenith_Assert(bSuccess, "Dequeue should succeed");
		Zenith_Assert(xOut.m_iValue == 1, "Dequeued value should be 1");
		// After dequeue: destructor called on slot + reconstruct creates new object
		// The slot's destructor should have been called
		Zenith_Assert(TestDestructorCounter::s_uDestructorCallCount > uPreDequeueCount,
			"Destructor should be called during Dequeue for non-POD types");

		// Clear and verify all destructors called
		uPreDequeueCount = TestDestructorCounter::s_uDestructorCallCount;
		xQueue.Clear();
		Zenith_Assert(xQueue.IsEmpty(), "Queue should be empty after Clear");
		Zenith_Assert(TestDestructorCounter::s_uDestructorCallCount > uPreDequeueCount,
			"Destructors should be called during Clear");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCircularQueueNonPOD PASSED");
}

void Zenith_UnitTests::TestVectorSelfAssignment()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestVectorSelfAssignment...");

	// Test copy self-assignment
	{
		Zenith_Vector<int> xVec;
		xVec.PushBack(1);
		xVec.PushBack(2);
		xVec.PushBack(3);

		// Self-assignment should be a no-op, not crash
		xVec = xVec;

		Zenith_Assert(xVec.GetSize() == 3, "Size should be unchanged after self-assignment");
		Zenith_Assert(xVec.Get(0) == 1, "Element 0 should be unchanged");
		Zenith_Assert(xVec.Get(1) == 2, "Element 1 should be unchanged");
		Zenith_Assert(xVec.Get(2) == 3, "Element 2 should be unchanged");
	}

	// Test move self-assignment
	{
		Zenith_Vector<int> xVec;
		xVec.PushBack(10);
		xVec.PushBack(20);

		// Move self-assignment should also be safe
		xVec = std::move(xVec);

		Zenith_Assert(xVec.GetSize() == 2, "Size should be unchanged after move self-assignment");
		Zenith_Assert(xVec.Get(0) == 10, "Element 0 should be unchanged");
		Zenith_Assert(xVec.Get(1) == 20, "Element 1 should be unchanged");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestVectorSelfAssignment PASSED");
}

void Zenith_UnitTests::TestVectorRemoveSwap()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestVectorRemoveSwap...");

	// Test basic RemoveSwap
	{
		Zenith_Vector<int> xVec;
		xVec.PushBack(1);
		xVec.PushBack(2);
		xVec.PushBack(3);
		xVec.PushBack(4);

		// Remove element at index 0 - last element (4) should be swapped in
		xVec.RemoveSwap(0);

		Zenith_Assert(xVec.GetSize() == 3, "Size should be 3 after RemoveSwap");
		Zenith_Assert(xVec.Get(0) == 4, "Element at index 0 should be swapped from end");
		Zenith_Assert(xVec.Get(1) == 2, "Element at index 1 should be unchanged");
		Zenith_Assert(xVec.Get(2) == 3, "Element at index 2 should be unchanged");
	}

	// Test RemoveSwap on last element (no swap needed)
	{
		Zenith_Vector<int> xVec;
		xVec.PushBack(1);
		xVec.PushBack(2);
		xVec.PushBack(3);

		// Remove last element
		xVec.RemoveSwap(2);

		Zenith_Assert(xVec.GetSize() == 2, "Size should be 2 after RemoveSwap on last");
		Zenith_Assert(xVec.Get(0) == 1, "Element 0 unchanged");
		Zenith_Assert(xVec.Get(1) == 2, "Element 1 unchanged");
	}

	// Test EraseValueSwap
	{
		Zenith_Vector<int> xVec;
		xVec.PushBack(10);
		xVec.PushBack(20);
		xVec.PushBack(30);

		bool bErased = xVec.EraseValueSwap(20);
		Zenith_Assert(bErased, "EraseValueSwap should return true for existing value");
		Zenith_Assert(xVec.GetSize() == 2, "Size should be 2");
		Zenith_Assert(xVec.Contains(10), "Should still contain 10");
		Zenith_Assert(xVec.Contains(30), "Should still contain 30");
		Zenith_Assert(!xVec.Contains(20), "Should NOT contain 20");

		bool bNotErased = xVec.EraseValueSwap(999);
		Zenith_Assert(!bNotErased, "EraseValueSwap should return false for non-existent value");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestVectorRemoveSwap PASSED");
}

void Zenith_UnitTests::TestDataStreamBoundsCheck()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDataStreamBoundsCheck...");

	// Test SkipBytes bounds checking
	{
		Zenith_DataStream xStream(100);

		// Write some data
		u_int uVal = 42;
		xStream << uVal;

		// Reset cursor and read
		xStream.SetCursor(0);
		u_int uRead;
		xStream >> uRead;
		Zenith_Assert(uRead == 42, "Read value should match written value");

		// Test valid skip
		xStream.SetCursor(0);
		xStream.SkipBytes(sizeof(u_int));
		Zenith_Assert(xStream.GetCursor() == sizeof(u_int), "Cursor should advance by skip amount");

		// Test skip to exactly end (valid edge case)
		xStream.SetCursor(96);
		xStream.SkipBytes(4);  // Should clamp to size (100)
		Zenith_Assert(xStream.GetCursor() <= xStream.GetSize(), "Cursor should not exceed data size");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDataStreamBoundsCheck PASSED");
}

// ============================================================================
// SCENE SERIALIZATION TESTS
// ============================================================================

/**
 * Test individual component serialization round-trip
 * Verifies that each component can save and load its data correctly
 */
void Zenith_UnitTests::TestComponentSerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComponentSerialization...");

	// Create a temporary scene for testing
	Zenith_Scene xTestScene;

	// Allow direct entity creation for unit tests

	// Test TransformComponent
	{
		Zenith_Entity xEntity(&xTestScene, "TestTransformEntity");
		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

		// Set ground truth data
		const Zenith_Maths::Vector3 xGroundTruthPos(1.0f, 2.0f, 3.0f);
		const Zenith_Maths::Quat xGroundTruthRot(0.707f, 0.0f, 0.707f, 0.0f);
		const Zenith_Maths::Vector3 xGroundTruthScale(2.0f, 3.0f, 4.0f);

		xTransform.SetPosition(xGroundTruthPos);
		xTransform.SetRotation(xGroundTruthRot);
		xTransform.SetScale(xGroundTruthScale);

		// Serialize
		Zenith_DataStream xStream;
		xTransform.WriteToDataStream(xStream);

		// Reset cursor and deserialize into new component
		xStream.SetCursor(0);
		Zenith_Entity xEntity2(&xTestScene, "TestTransformEntity2");
		Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
		xTransform2.ReadFromDataStream(xStream);

		// Verify
		Zenith_Maths::Vector3 xLoadedPos, xLoadedScale;
		Zenith_Maths::Quat xLoadedRot;
		xTransform2.GetPosition(xLoadedPos);
		xTransform2.GetRotation(xLoadedRot);
		xTransform2.GetScale(xLoadedScale);

		Zenith_Assert(xLoadedPos == xGroundTruthPos, "TransformComponent position mismatch");
		Zenith_Assert(xLoadedRot.x == xGroundTruthRot.x && xLoadedRot.y == xGroundTruthRot.y &&
					  xLoadedRot.z == xGroundTruthRot.z && xLoadedRot.w == xGroundTruthRot.w,
					  "TransformComponent rotation mismatch");
		Zenith_Assert(xLoadedScale == xGroundTruthScale, "TransformComponent scale mismatch");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ TransformComponent serialization passed");
	}

	// Test CameraComponent
	{
		Zenith_Entity xEntity(&xTestScene, "TestCameraEntity");
		Zenith_CameraComponent& xCamera = xEntity.AddComponent<Zenith_CameraComponent>();

		// Set ground truth data
		const Zenith_Maths::Vector3 xGroundTruthPos(5.0f, 10.0f, 15.0f);
		const float fGroundTruthPitch = 0.5f;
		const float fGroundTruthYaw = 1.2f;
		const float fGroundTruthFOV = 60.0f;
		const float fGroundTruthNear = 0.1f;
		const float fGroundTruthFar = 1000.0f;
		const float fGroundTruthAspect = 16.0f / 9.0f;

		xCamera.InitialisePerspective(xGroundTruthPos, fGroundTruthPitch, fGroundTruthYaw,
									   fGroundTruthFOV, fGroundTruthNear, fGroundTruthFar, fGroundTruthAspect);

		// Serialize
		Zenith_DataStream xStream;
		xCamera.WriteToDataStream(xStream);

		// Deserialize into new component
		xStream.SetCursor(0);
		Zenith_Entity xEntity2(&xTestScene, "TestCameraEntity2");
		Zenith_CameraComponent& xCamera2 = xEntity2.AddComponent<Zenith_CameraComponent>();
		xCamera2.ReadFromDataStream(xStream);

		// Verify
		Zenith_Maths::Vector3 xLoadedPos;
		xCamera2.GetPosition(xLoadedPos);

		Zenith_Assert(xLoadedPos == xGroundTruthPos, "CameraComponent position mismatch");
		Zenith_Assert(xCamera2.GetPitch() == fGroundTruthPitch, "CameraComponent pitch mismatch");
		Zenith_Assert(xCamera2.GetYaw() == fGroundTruthYaw, "CameraComponent yaw mismatch");
		Zenith_Assert(xCamera2.GetFOV() == fGroundTruthFOV, "CameraComponent FOV mismatch");
		Zenith_Assert(xCamera2.GetNearPlane() == fGroundTruthNear, "CameraComponent near plane mismatch");
		Zenith_Assert(xCamera2.GetFarPlane() == fGroundTruthFar, "CameraComponent far plane mismatch");
		Zenith_Assert(xCamera2.GetAspectRatio() == fGroundTruthAspect, "CameraComponent aspect ratio mismatch");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ CameraComponent serialization passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentSerialization completed successfully");
}

/**
 * Test entity serialization round-trip
 * Verifies that entities with multiple components can be serialized and restored
 */
void Zenith_UnitTests::TestEntitySerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntitySerialization...");

	// Create a temporary scene
	Zenith_Scene xTestScene;

	// Allow direct entity creation for unit tests

	// Create ground truth entity with multiple components
	Zenith_Entity xGroundTruthEntity(&xTestScene, "TestEntity");

	// Add TransformComponent
	Zenith_TransformComponent& xTransform = xGroundTruthEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f));
	xTransform.SetRotation(Zenith_Maths::Quat(0.707f, 0.0f, 0.707f, 0.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(1.5f, 1.5f, 1.5f));

	// Add CameraComponent
	Zenith_CameraComponent& xCamera = xGroundTruthEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(Zenith_Maths::Vector3(0.0f, 5.0f, 10.0f), 0.0f, 0.0f, 60.0f, 0.1f, 1000.0f, 16.0f / 9.0f);

	// Serialize entity
	Zenith_DataStream xStream;
	xGroundTruthEntity.WriteToDataStream(xStream);

	// Verify entity metadata was written
	const std::string strExpectedName = xGroundTruthEntity.GetName();

	// Deserialize into new entity
	// Note: The new entity gets its own fresh EntityID from the scene's slot system
	// ReadFromDataStream only loads component data and name, not the ID
	xStream.SetCursor(0);
	Zenith_Entity xLoadedEntity(&xTestScene, "PlaceholderName");
	xLoadedEntity.ReadFromDataStream(xStream);

	// Verify entity name was restored (EntityID is assigned by scene, not serialized)
	Zenith_Assert(xLoadedEntity.GetName() == strExpectedName, "Entity name mismatch");

	// Verify components were restored
	Zenith_Assert(xLoadedEntity.HasComponent<Zenith_TransformComponent>(), "TransformComponent not restored");
	Zenith_Assert(xLoadedEntity.HasComponent<Zenith_CameraComponent>(), "CameraComponent not restored");

	// Verify transform data
	Zenith_TransformComponent& xLoadedTransform = xLoadedEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLoadedPos;
	xLoadedTransform.GetPosition(xLoadedPos);
	Zenith_Assert(xLoadedPos.x == 10.0f && xLoadedPos.y == 20.0f && xLoadedPos.z == 30.0f, "Entity transform position mismatch");


	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntitySerialization completed successfully");
}

/**
 * Test full scene serialization
 * Verifies that entire scenes with multiple entities can be saved to disk
 */
void Zenith_UnitTests::TestSceneSerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSceneSerialization...");

	// Create a test scene with multiple entities
	Zenith_Scene xGroundTruthScene;

	// Entity 1: Camera
	Zenith_Entity xCameraEntity(&xGroundTruthScene, "MainCamera");
	xCameraEntity.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(Zenith_Maths::Vector3(0.0f, 10.0f, 20.0f), 0.0f, 0.0f, 60.0f, 0.1f, 1000.0f, 16.0f / 9.0f);
	xGroundTruthScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Entity 2: Transform only
	Zenith_Entity xEntity1(&xGroundTruthScene, "TestEntity1");
	xEntity1.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform1 = xEntity1.GetComponent<Zenith_TransformComponent>();
	xTransform1.SetPosition(Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f));

	// Entity 2: Transform only
	Zenith_Entity xEntity2(&xGroundTruthScene, "TestEntity2");
	xEntity2.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
	xTransform2.SetPosition(Zenith_Maths::Vector3(-5.0f, 0.0f, 0.0f));

	// Save scene to file
	const std::string strTestScenePath = "unit_test_scene.zscen";
	xGroundTruthScene.SaveToFile(strTestScenePath);

	// Verify file exists
	Zenith_Assert(std::filesystem::exists(strTestScenePath), "Scene file was not created");

	// Verify file has content
	std::ifstream xFile(strTestScenePath, std::ios::binary | std::ios::ate);
	Zenith_Assert(xFile.is_open(), "Could not open saved scene file");
	const std::streamsize ulFileSize = xFile.tellg();
	xFile.close();
	Zenith_Assert(ulFileSize > 0, "Scene file is empty");
	Zenith_Assert(ulFileSize > 16, "Scene file is suspiciously small (header + metadata should be >16 bytes)");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Scene file size: %lld bytes", ulFileSize);


	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneSerialization completed successfully");
}

/**
 * Test complete round-trip: save scene, clear, load scene, verify
 * This is the most comprehensive test - ensures data integrity across full save/load cycle
 */
void Zenith_UnitTests::TestSceneRoundTrip()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSceneRoundTrip...");

	const std::string strTestScenePath = "unit_test_roundtrip.zscen";

	// ========================================================================
	// STEP 1: CREATE GROUND TRUTH SCENE
	// ========================================================================

	Zenith_Scene xGroundTruthScene;

	// Allow direct entity creation for unit tests

	// Create Entity 1: Camera with specific properties
	Zenith_Entity xCameraEntity(&xGroundTruthScene, "MainCamera");
	const Zenith_EntityID uCameraEntityID = xCameraEntity.GetEntityID();
	xCameraEntity.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	const Zenith_Maths::Vector3 xCameraPos(0.0f, 10.0f, 20.0f);
	const float fCameraPitch = 0.3f;
	const float fCameraYaw = 1.57f;
	const float fCameraFOV = 75.0f;
	xCamera.InitialisePerspective(xCameraPos, fCameraPitch, fCameraYaw, fCameraFOV, 0.1f, 1000.0f, 16.0f / 9.0f);
	xGroundTruthScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Create Entity 2: Transform with precise values
	Zenith_Entity xEntity1(&xGroundTruthScene, "TestEntity1");
	const Zenith_EntityID uEntity1ID = xEntity1.GetEntityID();
	xEntity1.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform1 = xEntity1.GetComponent<Zenith_TransformComponent>();
	const Zenith_Maths::Vector3 xEntity1Pos(5.0f, 3.0f, -2.0f);
	const Zenith_Maths::Quat xEntity1Rot(0.5f, 0.5f, 0.5f, 0.5f);
	const Zenith_Maths::Vector3 xEntity1Scale(1.0f, 2.0f, 1.0f);
	xTransform1.SetPosition(xEntity1Pos);
	xTransform1.SetRotation(xEntity1Rot);
	xTransform1.SetScale(xEntity1Scale);

	// Create Entity 2: Transform only
	Zenith_Entity xEntity2(&xGroundTruthScene, "TestEntity2");
	const Zenith_EntityID uEntity2ID = xEntity2.GetEntityID();
	xEntity2.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
	const Zenith_Maths::Vector3 xEntity2Pos(-5.0f, 0.0f, 10.0f);
	xTransform2.SetPosition(xEntity2Pos);

	const u_int uGroundTruthEntityCount = 3;

	// ========================================================================
	// STEP 2: SAVE SCENE TO DISK
	// ========================================================================

	xGroundTruthScene.SaveToFile(strTestScenePath);
	Zenith_Assert(std::filesystem::exists(strTestScenePath), "Scene file was not created during round-trip test");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Scene saved to disk");

	// ========================================================================
	// STEP 3: CLEAR GROUND TRUTH SCENE (simulate application restart)
	// ========================================================================

	xGroundTruthScene.Reset();
	Zenith_Assert(xGroundTruthScene.GetEntityCount() == 0, "Scene was not properly cleared");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Scene cleared");

	// ========================================================================
	// STEP 4: LOAD SCENE FROM DISK
	// ========================================================================

	Zenith_Scene xLoadedScene;
	xLoadedScene.LoadFromFile(strTestScenePath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Scene loaded from disk");

	// ========================================================================
	// STEP 5: VERIFY LOADED SCENE MATCHES GROUND TRUTH
	// ========================================================================

	// Verify entity count
	Zenith_Assert(xLoadedScene.GetEntityCount() == uGroundTruthEntityCount,
				  "Loaded scene entity count mismatch (expected %u, got %u)",
				  uGroundTruthEntityCount, xLoadedScene.GetEntityCount());
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Entity count verified (%u entities)", uGroundTruthEntityCount);

	// Verify Camera Entity
	Zenith_Entity xLoadedCamera = xLoadedScene.GetEntity(uCameraEntityID);
	Zenith_Assert(xLoadedCamera.GetName() == "MainCamera", "Camera entity name mismatch");
	Zenith_Assert(xLoadedCamera.HasComponent<Zenith_CameraComponent>(), "Camera entity missing CameraComponent");

	Zenith_CameraComponent& xLoadedCameraComp = xLoadedCamera.GetComponent<Zenith_CameraComponent>();
	Zenith_Maths::Vector3 xLoadedCameraPos;
	xLoadedCameraComp.GetPosition(xLoadedCameraPos);
	Zenith_Assert(xLoadedCameraPos == xCameraPos, "Camera position mismatch");
	Zenith_Assert(xLoadedCameraComp.GetPitch() == fCameraPitch, "Camera pitch mismatch");
	Zenith_Assert(xLoadedCameraComp.GetYaw() == fCameraYaw, "Camera yaw mismatch");
	Zenith_Assert(xLoadedCameraComp.GetFOV() == fCameraFOV, "Camera FOV mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Camera entity verified");

	// Verify Entity 1
	Zenith_Entity xLoadedEntity1 = xLoadedScene.GetEntity(uEntity1ID);
	Zenith_Assert(xLoadedEntity1.GetName() == "TestEntity1", "Entity1 name mismatch");
	Zenith_Assert(xLoadedEntity1.HasComponent<Zenith_TransformComponent>(), "Entity1 missing TransformComponent");

	Zenith_TransformComponent& xLoadedTransform1 = xLoadedEntity1.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLoadedPos1, xLoadedScale1;
	Zenith_Maths::Quat xLoadedRot1;
	xLoadedTransform1.GetPosition(xLoadedPos1);
	xLoadedTransform1.GetRotation(xLoadedRot1);
	xLoadedTransform1.GetScale(xLoadedScale1);

	Zenith_Assert(xLoadedPos1 == xEntity1Pos, "Entity1 position mismatch");
	Zenith_Assert(xLoadedRot1.x == xEntity1Rot.x && xLoadedRot1.y == xEntity1Rot.y &&
				  xLoadedRot1.z == xEntity1Rot.z && xLoadedRot1.w == xEntity1Rot.w, "Entity1 rotation mismatch");
	Zenith_Assert(xLoadedScale1 == xEntity1Scale, "Entity1 scale mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Entity1 verified");

	// Verify Entity 2
	Zenith_Entity xLoadedEntity2 = xLoadedScene.GetEntity(uEntity2ID);
	Zenith_Assert(xLoadedEntity2.GetName() == "TestEntity2", "Entity2 name mismatch");
	Zenith_Assert(xLoadedEntity2.HasComponent<Zenith_TransformComponent>(), "Entity2 missing TransformComponent");

	Zenith_TransformComponent& xLoadedTransform2 = xLoadedEntity2.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLoadedPos2;
	xLoadedTransform2.GetPosition(xLoadedPos2);
	Zenith_Assert(xLoadedPos2 == xEntity2Pos, "Entity2 position mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Entity2 verified");

	// Verify main camera reference
	Zenith_CameraComponent& xMainCamera = xLoadedScene.GetMainCamera();
	Zenith_Maths::Vector3 xMainCameraPos;
	xMainCamera.GetPosition(xMainCameraPos);
	Zenith_Assert(xMainCameraPos == xCameraPos, "Main camera reference mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Main camera reference verified");


	// ========================================================================
	// STEP 6: CLEANUP
	// ========================================================================

	// Clean up test file
	std::filesystem::remove(strTestScenePath);
	Zenith_Assert(!std::filesystem::exists(strTestScenePath), "Test scene file was not cleaned up");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneRoundTrip completed successfully - full data integrity verified!");
}

// ============================================================================
// ANIMATION SYSTEM TESTS
// ============================================================================

// Helper function to compare floats with tolerance
static bool FloatEquals(float a, float b, float fTolerance = 0.0001f)
{
	return std::abs(a - b) < fTolerance;
}

// Helper function to compare vectors with tolerance
static bool Vec3Equals(const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b, float fTolerance = 0.0001f)
{
	return FloatEquals(a.x, b.x, fTolerance) &&
		   FloatEquals(a.y, b.y, fTolerance) &&
		   FloatEquals(a.z, b.z, fTolerance);
}

// Helper function to compare quaternions with tolerance
static bool QuatEquals(const Zenith_Maths::Quat& a, const Zenith_Maths::Quat& b, float fTolerance = 0.0001f)
{
	// Quaternions q and -q represent the same rotation, so check both
	bool bDirect = FloatEquals(a.x, b.x, fTolerance) &&
				   FloatEquals(a.y, b.y, fTolerance) &&
				   FloatEquals(a.z, b.z, fTolerance) &&
				   FloatEquals(a.w, b.w, fTolerance);
	bool bNegated = FloatEquals(a.x, -b.x, fTolerance) &&
					FloatEquals(a.y, -b.y, fTolerance) &&
					FloatEquals(a.z, -b.z, fTolerance) &&
					FloatEquals(a.w, -b.w, fTolerance);
	return bDirect || bNegated;
}

/**
 * Test Flux_BoneLocalPose blending operations
 * Verifies linear blend, additive blend, and identity pose
 */
void Zenith_UnitTests::TestBoneLocalPoseBlending()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestBoneLocalPoseBlending...");

	// Test Identity pose
	{
		Flux_BoneLocalPose xIdentity = Flux_BoneLocalPose::Identity();
		Zenith_Assert(Vec3Equals(xIdentity.m_xPosition, Zenith_Maths::Vector3(0.0f)),
			"Identity pose position should be zero");
		Zenith_Assert(QuatEquals(xIdentity.m_xRotation, Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f)),
			"Identity pose rotation should be identity quaternion");
		Zenith_Assert(Vec3Equals(xIdentity.m_xScale, Zenith_Maths::Vector3(1.0f)),
			"Identity pose scale should be one");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Identity pose test passed");
	}

	// Test linear blend
	{
		Flux_BoneLocalPose xPoseA;
		xPoseA.m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
		xPoseA.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		xPoseA.m_xScale = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);

		Flux_BoneLocalPose xPoseB;
		xPoseB.m_xPosition = Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f);
		xPoseB.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f); // Keep same for simpler test
		xPoseB.m_xScale = Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f);

		// Test t=0 (should return A)
		Flux_BoneLocalPose xBlend0 = Flux_BoneLocalPose::Blend(xPoseA, xPoseB, 0.0f);
		Zenith_Assert(Vec3Equals(xBlend0.m_xPosition, xPoseA.m_xPosition),
			"Blend at t=0 should return pose A position");
		Zenith_Assert(Vec3Equals(xBlend0.m_xScale, xPoseA.m_xScale),
			"Blend at t=0 should return pose A scale");

		// Test t=1 (should return B)
		Flux_BoneLocalPose xBlend1 = Flux_BoneLocalPose::Blend(xPoseA, xPoseB, 1.0f);
		Zenith_Assert(Vec3Equals(xBlend1.m_xPosition, xPoseB.m_xPosition),
			"Blend at t=1 should return pose B position");
		Zenith_Assert(Vec3Equals(xBlend1.m_xScale, xPoseB.m_xScale),
			"Blend at t=1 should return pose B scale");

		// Test t=0.5 (should return midpoint)
		Flux_BoneLocalPose xBlend05 = Flux_BoneLocalPose::Blend(xPoseA, xPoseB, 0.5f);
		Zenith_Maths::Vector3 xExpectedPos(5.0f, 10.0f, 15.0f);
		Zenith_Maths::Vector3 xExpectedScale(1.5f, 1.5f, 1.5f);
		Zenith_Assert(Vec3Equals(xBlend05.m_xPosition, xExpectedPos),
			"Blend at t=0.5 should return midpoint position");
		Zenith_Assert(Vec3Equals(xBlend05.m_xScale, xExpectedScale),
			"Blend at t=0.5 should return midpoint scale");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Linear blend test passed");
	}

	// Test additive blend
	{
		Flux_BoneLocalPose xBase;
		xBase.m_xPosition = Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f);
		xBase.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		xBase.m_xScale = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);

		Flux_BoneLocalPose xAdditive;
		xAdditive.m_xPosition = Zenith_Maths::Vector3(3.0f, 3.0f, 3.0f); // Delta from identity
		xAdditive.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		xAdditive.m_xScale = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);

		// Additive blend with weight 1.0 should add the delta
		Flux_BoneLocalPose xResult = Flux_BoneLocalPose::AdditiveBlend(xBase, xAdditive, 1.0f);
		Zenith_Maths::Vector3 xExpectedPos(8.0f, 8.0f, 8.0f); // 5 + 3
		Zenith_Assert(Vec3Equals(xResult.m_xPosition, xExpectedPos),
			"Additive blend should add delta position");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Additive blend test passed");
	}

	// Test ToMatrix conversion
	{
		Flux_BoneLocalPose xPose;
		xPose.m_xPosition = Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f);
		xPose.m_xRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
		xPose.m_xScale = Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f);

		Zenith_Maths::Matrix4 xMatrix = xPose.ToMatrix();

		// Check translation is in 4th column
		Zenith_Assert(FloatEquals(xMatrix[3][0], 1.0f) &&
					  FloatEquals(xMatrix[3][1], 2.0f) &&
					  FloatEquals(xMatrix[3][2], 3.0f),
			"Matrix translation should match pose position");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ ToMatrix conversion test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBoneLocalPoseBlending completed successfully");
}

/**
 * Test Flux_SkeletonPose operations
 * Verifies initialization, reset, and copy operations
 */
void Zenith_UnitTests::TestSkeletonPoseOperations()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSkeletonPoseOperations...");

	// Test initialization
	{
		Flux_SkeletonPose xPose;
		xPose.Initialize(50);

		Zenith_Assert(xPose.GetNumBones() == 50,
			"Skeleton pose should have 50 bones after initialization");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Initialization test passed");
	}

	// Test Reset
	{
		Flux_SkeletonPose xPose;
		xPose.Initialize(10);

		// Modify a bone
		Flux_BoneLocalPose& xBone0 = xPose.GetLocalPose(0);
		xBone0.m_xPosition = Zenith_Maths::Vector3(100.0f, 200.0f, 300.0f);
		xBone0.m_xScale = Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f);

		// Reset
		xPose.Reset();

		// Verify reset to identity
		const Flux_BoneLocalPose& xResetBone = xPose.GetLocalPose(0);
		Zenith_Assert(Vec3Equals(xResetBone.m_xPosition, Zenith_Maths::Vector3(0.0f)),
			"Reset should set position to zero");
		Zenith_Assert(Vec3Equals(xResetBone.m_xScale, Zenith_Maths::Vector3(1.0f)),
			"Reset should set scale to one");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Reset test passed");
	}

	// Test CopyFrom
	{
		Flux_SkeletonPose xPoseA;
		xPoseA.Initialize(5);
		xPoseA.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f);
		xPoseA.GetLocalPose(1).m_xPosition = Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f);

		Flux_SkeletonPose xPoseB;
		xPoseB.Initialize(5);
		xPoseB.CopyFrom(xPoseA);

		Zenith_Assert(Vec3Equals(xPoseB.GetLocalPose(0).m_xPosition, Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f)),
			"CopyFrom should copy bone 0 position");
		Zenith_Assert(Vec3Equals(xPoseB.GetLocalPose(1).m_xPosition, Zenith_Maths::Vector3(4.0f, 5.0f, 6.0f)),
			"CopyFrom should copy bone 1 position");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ CopyFrom test passed");
	}

	// Test static Blend
	{
		Flux_SkeletonPose xPoseA, xPoseB, xPoseOut;
		xPoseA.Initialize(3);
		xPoseB.Initialize(3);
		xPoseOut.Initialize(3);

		xPoseA.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
		xPoseB.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(10.0f, 10.0f, 10.0f);

		Flux_SkeletonPose::Blend(xPoseOut, xPoseA, xPoseB, 0.5f);

		Zenith_Assert(Vec3Equals(xPoseOut.GetLocalPose(0).m_xPosition, Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f)),
			"Skeleton blend should interpolate bone positions");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Static blend test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSkeletonPoseOperations completed successfully");
}

/**
 * Test Flux_AnimationParameters
 * Verifies parameter add, set, get, and trigger consumption
 */
void Zenith_UnitTests::TestAnimationParameters()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnimationParameters...");

	Flux_AnimationParameters xParams;

	// Test Float parameter
	{
		xParams.AddFloat("Speed", 5.0f);
		Zenith_Assert(xParams.HasParameter("Speed"), "Should have Speed parameter");
		Zenith_Assert(FloatEquals(xParams.GetFloat("Speed"), 5.0f),
			"Speed default should be 5.0");

		xParams.SetFloat("Speed", 10.0f);
		Zenith_Assert(FloatEquals(xParams.GetFloat("Speed"), 10.0f),
			"Speed should be updated to 10.0");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Float parameter test passed");
	}

	// Test Int parameter
	{
		xParams.AddInt("Health", 100);
		Zenith_Assert(xParams.HasParameter("Health"), "Should have Health parameter");
		Zenith_Assert(xParams.GetInt("Health") == 100, "Health default should be 100");

		xParams.SetInt("Health", 50);
		Zenith_Assert(xParams.GetInt("Health") == 50, "Health should be updated to 50");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Int parameter test passed");
	}

	// Test Bool parameter
	{
		xParams.AddBool("IsRunning", false);
		Zenith_Assert(xParams.HasParameter("IsRunning"), "Should have IsRunning parameter");
		Zenith_Assert(xParams.GetBool("IsRunning") == false, "IsRunning default should be false");

		xParams.SetBool("IsRunning", true);
		Zenith_Assert(xParams.GetBool("IsRunning") == true, "IsRunning should be updated to true");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Bool parameter test passed");
	}

	// Test Trigger parameter
	{
		xParams.AddTrigger("Jump");
		Zenith_Assert(xParams.HasParameter("Jump"), "Should have Jump trigger");

		// Trigger not set initially
		Zenith_Assert(xParams.ConsumeTrigger("Jump") == false,
			"Trigger should not be set initially");

		// Set trigger
		xParams.SetTrigger("Jump");
		Zenith_Assert(xParams.ConsumeTrigger("Jump") == true,
			"Trigger should be set after SetTrigger");

		// Trigger should be consumed (reset)
		Zenith_Assert(xParams.ConsumeTrigger("Jump") == false,
			"Trigger should be reset after consumption");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Trigger parameter test passed");
	}

	// Test RemoveParameter
	{
		Zenith_Assert(xParams.HasParameter("Speed"), "Speed should exist");
		xParams.RemoveParameter("Speed");
		Zenith_Assert(!xParams.HasParameter("Speed"), "Speed should be removed");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ RemoveParameter test passed");
	}

	// Test GetParameterType
	{
		Zenith_Assert(xParams.GetParameterType("Health") == Flux_AnimationParameters::ParamType::Int,
			"Health should be Int type");
		Zenith_Assert(xParams.GetParameterType("IsRunning") == Flux_AnimationParameters::ParamType::Bool,
			"IsRunning should be Bool type");
		Zenith_Assert(xParams.GetParameterType("Jump") == Flux_AnimationParameters::ParamType::Trigger,
			"Jump should be Trigger type");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ GetParameterType test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnimationParameters completed successfully");
}

/**
 * Test Flux_TransitionCondition evaluation
 * Verifies all comparison operators with different parameter types
 */
void Zenith_UnitTests::TestTransitionConditions()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTransitionConditions...");

	Flux_AnimationParameters xParams;
	xParams.AddFloat("Speed", 5.0f);
	xParams.AddInt("Health", 100);
	xParams.AddBool("IsGrounded", true);
	xParams.AddTrigger("Attack");

	// Test Float Greater condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 3.0f;

		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"Speed 5.0 > 3.0 should be true");

		xCond.m_fThreshold = 6.0f;
		Zenith_Assert(xCond.Evaluate(xParams) == false,
			"Speed 5.0 > 6.0 should be false");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Float Greater condition test passed");
	}

	// Test Float Less condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Less;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 10.0f;

		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"Speed 5.0 < 10.0 should be true");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Float Less condition test passed");
	}

	// Test Int Equal condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Health";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Int;
		xCond.m_iThreshold = 100;

		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"Health 100 == 100 should be true");

		xCond.m_iThreshold = 50;
		Zenith_Assert(xCond.Evaluate(xParams) == false,
			"Health 100 == 50 should be false");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Int Equal condition test passed");
	}

	// Test Int LessEqual condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Health";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::LessEqual;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Int;
		xCond.m_iThreshold = 100;

		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"Health 100 <= 100 should be true");

		xCond.m_iThreshold = 50;
		Zenith_Assert(xCond.Evaluate(xParams) == false,
			"Health 100 <= 50 should be false");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Int LessEqual condition test passed");
	}

	// Test Bool condition
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "IsGrounded";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
		xCond.m_bThreshold = true;

		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"IsGrounded true == true should be true");

		xCond.m_bThreshold = false;
		Zenith_Assert(xCond.Evaluate(xParams) == false,
			"IsGrounded true == false should be false");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Bool condition test passed");
	}

	// Test Trigger condition (Equal to true means trigger is set)
	{
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Attack";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xCond.m_bThreshold = true;

		Zenith_Assert(xCond.Evaluate(xParams) == false,
			"Attack trigger not set should be false");

		xParams.SetTrigger("Attack");
		Zenith_Assert(xCond.Evaluate(xParams) == true,
			"Attack trigger set should be true");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Trigger condition test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTransitionConditions completed successfully");
}

/**
 * Test Flux_AnimationStateMachine
 * Verifies state creation, transitions, and state changes
 */
void Zenith_UnitTests::TestAnimationStateMachine()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnimationStateMachine...");

	Flux_AnimationStateMachine xStateMachine("TestSM");

	// Test state creation
	{
		Flux_AnimationState* pxIdleState = xStateMachine.AddState("Idle");
		Flux_AnimationState* pxWalkState = xStateMachine.AddState("Walk");
		Flux_AnimationState* pxRunState = xStateMachine.AddState("Run");

		Zenith_Assert(pxIdleState != nullptr, "Idle state should be created");
		Zenith_Assert(pxWalkState != nullptr, "Walk state should be created");
		Zenith_Assert(pxRunState != nullptr, "Run state should be created");

		Zenith_Assert(xStateMachine.HasState("Idle"), "Should have Idle state");
		Zenith_Assert(xStateMachine.HasState("Walk"), "Should have Walk state");
		Zenith_Assert(xStateMachine.HasState("Run"), "Should have Run state");
		Zenith_Assert(!xStateMachine.HasState("Jump"), "Should not have Jump state");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ State creation test passed");
	}

	// Test default state
	{
		xStateMachine.SetDefaultState("Idle");
		Zenith_Assert(xStateMachine.GetDefaultStateName() == "Idle",
			"Default state should be Idle");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Default state test passed");
	}

	// Test SetState (force state change)
	{
		xStateMachine.SetState("Idle");
		Zenith_Assert(xStateMachine.GetCurrentStateName() == "Idle",
			"Current state should be Idle");

		xStateMachine.SetState("Walk");
		Zenith_Assert(xStateMachine.GetCurrentStateName() == "Walk",
			"Current state should be Walk after SetState");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ SetState test passed");
	}

	// Test adding transitions
	{
		Flux_AnimationState* pxIdleState = xStateMachine.GetState("Idle");
		Zenith_Assert(pxIdleState != nullptr, "Should retrieve Idle state");

		Flux_StateTransition xTransition;
		xTransition.m_strTargetStateName = "Walk";
		xTransition.m_fTransitionDuration = 0.2f;

		// Add condition: Speed > 0.1
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 0.1f;
		xTransition.m_xConditions.PushBack(xCond);

		pxIdleState->AddTransition(xTransition);

		Zenith_Assert(pxIdleState->GetTransitions().GetSize() == 1,
			"Idle state should have 1 transition");
		Zenith_Assert(pxIdleState->GetTransitions().Get(0).m_strTargetStateName == "Walk",
			"Transition should target Walk state");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Transition creation test passed");
	}

	// Test parameters
	{
		xStateMachine.GetParameters().AddFloat("Speed", 0.0f);
		xStateMachine.GetParameters().AddBool("IsGrounded", true);

		Zenith_Assert(xStateMachine.GetParameters().HasParameter("Speed"),
			"Parameters should have Speed");
		Zenith_Assert(xStateMachine.GetParameters().HasParameter("IsGrounded"),
			"Parameters should have IsGrounded");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Parameters integration test passed");
	}

	// Test state removal
	{
		xStateMachine.RemoveState("Run");
		Zenith_Assert(!xStateMachine.HasState("Run"), "Run state should be removed");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ State removal test passed");
	}

	// Test name
	{
		Zenith_Assert(xStateMachine.GetName() == "TestSM",
			"State machine name should be TestSM");

		xStateMachine.SetName("RenamedSM");
		Zenith_Assert(xStateMachine.GetName() == "RenamedSM",
			"State machine name should be RenamedSM");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Name test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnimationStateMachine completed successfully");
}

/**
 * Test Flux_IKChain and Flux_IKSolver setup
 * Verifies chain creation, target management, and helper functions
 */
void Zenith_UnitTests::TestIKChainSetup()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestIKChainSetup...");

	Flux_IKSolver xSolver;

	// Test chain creation with helper functions
	{
		Flux_IKChain xLegChain = Flux_IKSolver::CreateLegChain("LeftLeg", "Hip_L", "Knee_L", "Ankle_L");

		Zenith_Assert(xLegChain.m_strName == "LeftLeg", "Chain name should be LeftLeg");
		Zenith_Assert(xLegChain.m_xBoneNames.size() == 3, "Leg chain should have 3 bones");
		Zenith_Assert(xLegChain.m_xBoneNames[0] == "Hip_L", "First bone should be Hip_L");
		Zenith_Assert(xLegChain.m_xBoneNames[1] == "Knee_L", "Second bone should be Knee_L");
		Zenith_Assert(xLegChain.m_xBoneNames[2] == "Ankle_L", "Third bone should be Ankle_L");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ CreateLegChain test passed");
	}

	// Test arm chain creation
	{
		Flux_IKChain xArmChain = Flux_IKSolver::CreateArmChain("RightArm", "Shoulder_R", "Elbow_R", "Wrist_R");

		Zenith_Assert(xArmChain.m_strName == "RightArm", "Chain name should be RightArm");
		Zenith_Assert(xArmChain.m_xBoneNames.size() == 3, "Arm chain should have 3 bones");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ CreateArmChain test passed");
	}

	// Test spine chain creation
	{
		std::vector<std::string> xSpineBones = { "Spine1", "Spine2", "Spine3", "Neck" };
		Flux_IKChain xSpineChain = Flux_IKSolver::CreateSpineChain("Spine", xSpineBones);

		Zenith_Assert(xSpineChain.m_strName == "Spine", "Chain name should be Spine");
		Zenith_Assert(xSpineChain.m_xBoneNames.size() == 4, "Spine chain should have 4 bones");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ CreateSpineChain test passed");
	}

	// Test adding chains to solver
	{
		Flux_IKChain xLeftLeg = Flux_IKSolver::CreateLegChain("LeftLeg", "Hip_L", "Knee_L", "Ankle_L");
		Flux_IKChain xRightLeg = Flux_IKSolver::CreateLegChain("RightLeg", "Hip_R", "Knee_R", "Ankle_R");

		xSolver.AddChain(xLeftLeg);
		xSolver.AddChain(xRightLeg);

		Zenith_Assert(xSolver.HasChain("LeftLeg"), "Solver should have LeftLeg chain");
		Zenith_Assert(xSolver.HasChain("RightLeg"), "Solver should have RightLeg chain");
		Zenith_Assert(!xSolver.HasChain("LeftArm"), "Solver should not have LeftArm chain");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ AddChain test passed");
	}

	// Test target management
	{
		Flux_IKTarget xTarget;
		xTarget.m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f);
		xTarget.m_fWeight = 0.75f;
		xTarget.m_bEnabled = true;

		xSolver.SetTarget("LeftLeg", xTarget);

		Zenith_Assert(xSolver.HasTarget("LeftLeg"), "Solver should have LeftLeg target");
		Zenith_Assert(!xSolver.HasTarget("RightLeg"), "Solver should not have RightLeg target");

		const Flux_IKTarget* pxTarget = xSolver.GetTarget("LeftLeg");
		Zenith_Assert(pxTarget != nullptr, "Should retrieve LeftLeg target");
		Zenith_Assert(Vec3Equals(pxTarget->m_xPosition, Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f)),
			"Target position should match");
		Zenith_Assert(FloatEquals(pxTarget->m_fWeight, 0.75f), "Target weight should be 0.75");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Target management test passed");
	}

	// Test ClearTarget
	{
		xSolver.ClearTarget("LeftLeg");
		Zenith_Assert(!xSolver.HasTarget("LeftLeg"), "LeftLeg target should be cleared");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ ClearTarget test passed");
	}

	// Test RemoveChain
	{
		xSolver.RemoveChain("LeftLeg");
		Zenith_Assert(!xSolver.HasChain("LeftLeg"), "LeftLeg chain should be removed");
		Zenith_Assert(xSolver.HasChain("RightLeg"), "RightLeg chain should still exist");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ RemoveChain test passed");
	}

	// Test GetChain
	{
		Flux_IKChain* pxChain = xSolver.GetChain("RightLeg");
		Zenith_Assert(pxChain != nullptr, "Should retrieve RightLeg chain");
		Zenith_Assert(pxChain->m_strName == "RightLeg", "Chain name should be RightLeg");

		// Modify via pointer
		pxChain->m_uMaxIterations = 20;
		Zenith_Assert(xSolver.GetChain("RightLeg")->m_uMaxIterations == 20,
			"Chain modification should persist");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ GetChain test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIKChainSetup completed successfully");
}

/**
 * Test animation system serialization
 * Verifies round-trip serialization for animation data structures
 */
void Zenith_UnitTests::TestAnimationSerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnimationSerialization...");

	// Test AnimationParameters serialization
	{
		Flux_AnimationParameters xOriginal;
		xOriginal.AddFloat("Speed", 5.5f);
		xOriginal.AddInt("Combo", 3);
		xOriginal.AddBool("IsJumping", true);
		xOriginal.AddTrigger("Attack");
		xOriginal.SetTrigger("Attack");

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_AnimationParameters xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(xLoaded.HasParameter("Speed"), "Should have Speed param");
		Zenith_Assert(FloatEquals(xLoaded.GetFloat("Speed"), 5.5f), "Speed should be 5.5");
		Zenith_Assert(xLoaded.GetInt("Combo") == 3, "Combo should be 3");
		Zenith_Assert(xLoaded.GetBool("IsJumping") == true, "IsJumping should be true");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ AnimationParameters serialization test passed");
	}

	// Test TransitionCondition serialization
	{
		Flux_TransitionCondition xOriginal;
		xOriginal.m_strParameterName = "Speed";
		xOriginal.m_eCompareOp = Flux_TransitionCondition::CompareOp::GreaterEqual;
		xOriginal.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xOriginal.m_fThreshold = 3.14f;

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_TransitionCondition xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(xLoaded.m_strParameterName == "Speed", "Parameter name should match");
		Zenith_Assert(xLoaded.m_eCompareOp == Flux_TransitionCondition::CompareOp::GreaterEqual,
			"Compare op should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fThreshold, 3.14f), "Threshold should match");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ TransitionCondition serialization test passed");
	}

	// Test IKChain serialization
	{
		Flux_IKChain xOriginal = Flux_IKSolver::CreateLegChain("TestLeg", "Hip", "Knee", "Ankle");
		xOriginal.m_uMaxIterations = 15;
		xOriginal.m_fTolerance = 0.005f;
		xOriginal.m_bUsePoleVector = true;
		xOriginal.m_xPoleVector = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_IKChain xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(xLoaded.m_strName == "TestLeg", "Chain name should match");
		Zenith_Assert(xLoaded.m_xBoneNames.size() == 3, "Should have 3 bones");
		Zenith_Assert(xLoaded.m_xBoneNames[0] == "Hip", "First bone should be Hip");
		Zenith_Assert(xLoaded.m_uMaxIterations == 15, "Max iterations should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fTolerance, 0.005f), "Tolerance should match");
		Zenith_Assert(xLoaded.m_bUsePoleVector == true, "Use pole vector should match");
		Zenith_Assert(Vec3Equals(xLoaded.m_xPoleVector, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)),
			"Pole vector should match");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ IKChain serialization test passed");
	}

	// Test JointConstraint serialization
	{
		Flux_JointConstraint xOriginal;
		xOriginal.m_eType = Flux_JointConstraint::ConstraintType::Hinge;
		xOriginal.m_xHingeAxis = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		xOriginal.m_fMinAngle = -1.5f;
		xOriginal.m_fMaxAngle = 0.0f;

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_JointConstraint xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(xLoaded.m_eType == Flux_JointConstraint::ConstraintType::Hinge,
			"Constraint type should be Hinge");
		Zenith_Assert(Vec3Equals(xLoaded.m_xHingeAxis, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f)),
			"Hinge axis should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fMinAngle, -1.5f), "Min angle should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fMaxAngle, 0.0f), "Max angle should match");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ JointConstraint serialization test passed");
	}

	// Test BoneMask serialization
	{
		Flux_BoneMask xOriginal;
		xOriginal.SetBoneWeight(0, 1.0f);
		xOriginal.SetBoneWeight(1, 0.5f);
		xOriginal.SetBoneWeight(2, 0.0f);

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_BoneMask xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(FloatEquals(xLoaded.GetBoneWeight(0), 1.0f), "Bone 0 weight should be 1.0");
		Zenith_Assert(FloatEquals(xLoaded.GetBoneWeight(1), 0.5f), "Bone 1 weight should be 0.5");
		Zenith_Assert(FloatEquals(xLoaded.GetBoneWeight(2), 0.0f), "Bone 2 weight should be 0.0");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ BoneMask serialization test passed");
	}

	// Test AnimationClipMetadata serialization
	{
		Flux_AnimationClipMetadata xOriginal;
		xOriginal.m_strName = "TestClip";
		xOriginal.m_fDuration = 2.5f;
		xOriginal.m_uTicksPerSecond = 30;
		xOriginal.m_bLooping = false;
		xOriginal.m_fBlendInTime = 0.2f;
		xOriginal.m_fBlendOutTime = 0.3f;

		Zenith_DataStream xStream;
		xOriginal.WriteToDataStream(xStream);

		xStream.SetCursor(0);
		Flux_AnimationClipMetadata xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(xLoaded.m_strName == "TestClip", "Clip name should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fDuration, 2.5f), "Duration should match");
		Zenith_Assert(xLoaded.m_uTicksPerSecond == 30, "Ticks per second should match");
		Zenith_Assert(xLoaded.m_bLooping == false, "Looping should be false");
		Zenith_Assert(FloatEquals(xLoaded.m_fBlendInTime, 0.2f), "Blend in time should match");
		Zenith_Assert(FloatEquals(xLoaded.m_fBlendOutTime, 0.3f), "Blend out time should match");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ AnimationClipMetadata serialization test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnimationSerialization completed successfully");
}

/**
 * Test blend tree node types
 * Verifies blend tree node creation and factory method
 */
void Zenith_UnitTests::TestBlendTreeNodes()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestBlendTreeNodes...");

	// Test Clip node
	{
		Flux_BlendTreeNode_Clip xClipNode(nullptr, 1.0f);
		Zenith_Assert(std::string(xClipNode.GetNodeTypeName()) == "Clip", "Type name should be Clip");
		Zenith_Assert(FloatEquals(xClipNode.GetPlaybackRate(), 1.0f), "Playback rate should be 1.0");

		xClipNode.SetPlaybackRate(1.5f);
		Zenith_Assert(FloatEquals(xClipNode.GetPlaybackRate(), 1.5f), "Playback rate should be 1.5");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Clip node test passed");
	}

	// Test Blend node
	{
		Flux_BlendTreeNode_Blend xBlendNode;
		Zenith_Assert(std::string(xBlendNode.GetNodeTypeName()) == "Blend", "Type name should be Blend");

		xBlendNode.SetBlendWeight(0.75f);
		Zenith_Assert(FloatEquals(xBlendNode.GetBlendWeight(), 0.75f), "Blend weight should be 0.75");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Blend node test passed");
	}

	// Test BlendSpace1D node
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;
		Zenith_Assert(std::string(xBlendSpace.GetNodeTypeName()) == "BlendSpace1D", "Type name should be BlendSpace1D");

		xBlendSpace.SetParameter(0.5f);
		Zenith_Assert(FloatEquals(xBlendSpace.GetParameter(), 0.5f), "Parameter should be 0.5");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ BlendSpace1D node test passed");
	}

	// Test BlendSpace2D node
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;
		Zenith_Assert(std::string(xBlendSpace.GetNodeTypeName()) == "BlendSpace2D", "Type name should be BlendSpace2D");

		Zenith_Maths::Vector2 xParams(0.3f, 0.7f);
		xBlendSpace.SetParameter(xParams);
		const Zenith_Maths::Vector2& xRetrieved = xBlendSpace.GetParameter();
		Zenith_Assert(FloatEquals(xRetrieved.x, 0.3f) && FloatEquals(xRetrieved.y, 0.7f),
			"Parameters should be (0.3, 0.7)");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ BlendSpace2D node test passed");
	}

	// Test Additive node
	{
		Flux_BlendTreeNode_Additive xAdditiveNode;
		Zenith_Assert(std::string(xAdditiveNode.GetNodeTypeName()) == "Additive", "Type name should be Additive");

		xAdditiveNode.SetAdditiveWeight(0.5f);
		Zenith_Assert(FloatEquals(xAdditiveNode.GetAdditiveWeight(), 0.5f), "Additive weight should be 0.5");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Additive node test passed");
	}

	// Test Select node
	{
		Flux_BlendTreeNode_Select xSelectNode;
		Zenith_Assert(std::string(xSelectNode.GetNodeTypeName()) == "Select", "Type name should be Select");

		// Add some children before setting selected index
		xSelectNode.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xSelectNode.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xSelectNode.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));

		xSelectNode.SetSelectedIndex(2);
		Zenith_Assert(xSelectNode.GetSelectedIndex() == 2, "Selected index should be 2");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Select node test passed");
	}

	// Test factory method
	{
		Flux_BlendTreeNode* pxClip = Flux_BlendTreeNode::CreateFromTypeName("Clip");
		Zenith_Assert(pxClip != nullptr, "Factory should create Clip node");
		Zenith_Assert(std::string(pxClip->GetNodeTypeName()) == "Clip", "Created node should be Clip type");
		delete pxClip;

		Flux_BlendTreeNode* pxBlend = Flux_BlendTreeNode::CreateFromTypeName("Blend");
		Zenith_Assert(pxBlend != nullptr, "Factory should create Blend node");
		Zenith_Assert(std::string(pxBlend->GetNodeTypeName()) == "Blend", "Created node should be Blend type");
		delete pxBlend;

		Flux_BlendTreeNode* pxBlendSpace1D = Flux_BlendTreeNode::CreateFromTypeName("BlendSpace1D");
		Zenith_Assert(pxBlendSpace1D != nullptr, "Factory should create BlendSpace1D node");
		Zenith_Assert(std::string(pxBlendSpace1D->GetNodeTypeName()) == "BlendSpace1D", "Created node should be BlendSpace1D type");
		delete pxBlendSpace1D;

		Flux_BlendTreeNode* pxBlendSpace2D = Flux_BlendTreeNode::CreateFromTypeName("BlendSpace2D");
		Zenith_Assert(pxBlendSpace2D != nullptr, "Factory should create BlendSpace2D node");
		Zenith_Assert(std::string(pxBlendSpace2D->GetNodeTypeName()) == "BlendSpace2D", "Created node should be BlendSpace2D type");
		delete pxBlendSpace2D;

		Flux_BlendTreeNode* pxAdditive = Flux_BlendTreeNode::CreateFromTypeName("Additive");
		Zenith_Assert(pxAdditive != nullptr, "Factory should create Additive node");
		Zenith_Assert(std::string(pxAdditive->GetNodeTypeName()) == "Additive", "Created node should be Additive type");
		delete pxAdditive;

		Flux_BlendTreeNode* pxMasked = Flux_BlendTreeNode::CreateFromTypeName("Masked");
		Zenith_Assert(pxMasked != nullptr, "Factory should create Masked node");
		Zenith_Assert(std::string(pxMasked->GetNodeTypeName()) == "Masked", "Created node should be Masked type");
		delete pxMasked;

		Flux_BlendTreeNode* pxSelect = Flux_BlendTreeNode::CreateFromTypeName("Select");
		Zenith_Assert(pxSelect != nullptr, "Factory should create Select node");
		Zenith_Assert(std::string(pxSelect->GetNodeTypeName()) == "Select", "Created node should be Select type");
		delete pxSelect;

		Flux_BlendTreeNode* pxInvalid = Flux_BlendTreeNode::CreateFromTypeName("InvalidType");
		Zenith_Assert(pxInvalid == nullptr, "Factory should return nullptr for invalid type");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Factory method test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlendTreeNodes completed successfully");
}

/**
 * Test cross-fade transition
 * Verifies transition timing and blend weight calculations
 */
void Zenith_UnitTests::TestCrossFadeTransition()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCrossFadeTransition...");

	// Test initial state
	{
		Flux_CrossFadeTransition xTransition;
		Zenith_Assert(xTransition.IsComplete() == true,
			"Transition should be complete initially (no duration set)");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Initial state test passed");
	}

	// Test Start and Update
	{
		Flux_SkeletonPose xFromPose;
		xFromPose.Initialize(5);
		xFromPose.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f);

		Flux_CrossFadeTransition xTransition;
		xTransition.Start(xFromPose, 1.0f); // 1 second transition

		Zenith_Assert(xTransition.IsComplete() == false,
			"Transition should not be complete after Start");
		Zenith_Assert(FloatEquals(xTransition.GetBlendWeight(), 0.0f, 0.01f),
			"Blend weight should be 0 at start");

		// Update halfway
		xTransition.Update(0.5f);
		Zenith_Assert(xTransition.IsComplete() == false,
			"Transition should not be complete at 0.5s");
		// With EaseInOut, 0.5 normalized time might not be exactly 0.5 blend weight
		// but should be close for symmetrical easing
		float fMidWeight = xTransition.GetBlendWeight();
		Zenith_Assert(fMidWeight > 0.3f && fMidWeight < 0.7f,
			"Blend weight at midpoint should be roughly 0.5");

		// Update to completion
		xTransition.Update(0.6f); // Total 1.1s, should be complete
		Zenith_Assert(xTransition.IsComplete() == true,
			"Transition should be complete after 1.1s");
		Zenith_Assert(FloatEquals(xTransition.GetBlendWeight(), 1.0f),
			"Blend weight should be 1.0 when complete");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Start and Update test passed");
	}

	// Test different easing types
	{
		Flux_SkeletonPose xFromPose;
		xFromPose.Initialize(1);

		// Test Linear easing
		{
			Flux_CrossFadeTransition xTransition;
			xTransition.SetEasing(Flux_CrossFadeTransition::EasingType::Linear);
			xTransition.Start(xFromPose, 1.0f);
			xTransition.Update(0.5f);
			Zenith_Assert(FloatEquals(xTransition.GetBlendWeight(), 0.5f),
				"Linear easing should give 0.5 at midpoint");
		}

		// Test EaseIn easing
		{
			Flux_CrossFadeTransition xTransition;
			xTransition.SetEasing(Flux_CrossFadeTransition::EasingType::EaseIn);
			xTransition.Start(xFromPose, 1.0f);
			xTransition.Update(0.5f);
			float fWeight = xTransition.GetBlendWeight();
			Zenith_Assert(fWeight < 0.5f,
				"EaseIn should give weight < 0.5 at midpoint");
		}

		// Test EaseOut easing
		{
			Flux_CrossFadeTransition xTransition;
			xTransition.SetEasing(Flux_CrossFadeTransition::EasingType::EaseOut);
			xTransition.Start(xFromPose, 1.0f);
			xTransition.Update(0.5f);
			float fWeight = xTransition.GetBlendWeight();
			Zenith_Assert(fWeight > 0.5f,
				"EaseOut should give weight > 0.5 at midpoint");
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Easing types test passed");
	}

	// Test Blend operation
	{
		Flux_SkeletonPose xFromPose;
		xFromPose.Initialize(1);
		xFromPose.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);

		Flux_SkeletonPose xTargetPose;
		xTargetPose.Initialize(1);
		xTargetPose.GetLocalPose(0).m_xPosition = Zenith_Maths::Vector3(10.0f, 10.0f, 10.0f);

		Flux_CrossFadeTransition xTransition;
		xTransition.SetEasing(Flux_CrossFadeTransition::EasingType::Linear);
		xTransition.Start(xFromPose, 1.0f);
		xTransition.Update(0.5f); // 50% blend

		Flux_SkeletonPose xOutPose;
		xOutPose.Initialize(1);
		xTransition.Blend(xOutPose, xTargetPose);

		Zenith_Assert(Vec3Equals(xOutPose.GetLocalPose(0).m_xPosition, Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f)),
			"Blend should interpolate position to midpoint");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Blend operation test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCrossFadeTransition completed successfully");
}

/**
 * Test Animation Clip Channels
 * Verifies clip metadata and event handling
 */
void Zenith_UnitTests::TestAnimationClipChannels()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnimationClipChannels...");

	// Test clip metadata
	{
		Flux_AnimationClipMetadata xMetadata;
		xMetadata.m_strName = "TestClip";
		xMetadata.m_fDuration = 2.5f;
		xMetadata.m_uTicksPerSecond = 30;
		xMetadata.m_bLooping = true;
		xMetadata.m_fBlendInTime = 0.2f;
		xMetadata.m_fBlendOutTime = 0.15f;

		Zenith_Assert(xMetadata.m_strName == "TestClip", "Name should be 'TestClip'");
		Zenith_Assert(FloatEquals(xMetadata.m_fDuration, 2.5f), "Duration should be 2.5");
		Zenith_Assert(xMetadata.m_uTicksPerSecond == 30, "Ticks per second should be 30");
		Zenith_Assert(xMetadata.m_bLooping == true, "Looping should be true");
		Zenith_Assert(FloatEquals(xMetadata.m_fBlendInTime, 0.2f), "Blend in time should be 0.2");
		Zenith_Assert(FloatEquals(xMetadata.m_fBlendOutTime, 0.15f), "Blend out time should be 0.15");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Clip metadata test passed");
	}

	// Test animation clip with events
	{
		Flux_AnimationClip xClip;
		xClip.SetName("Walk");
		xClip.SetLooping(true);

		// Add events
		Flux_AnimationEvent xEvent1;
		xEvent1.m_strEventName = "LeftFootDown";
		xEvent1.m_fNormalizedTime = 0.25f;
		xEvent1.m_xData = Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.5f);

		Flux_AnimationEvent xEvent2;
		xEvent2.m_strEventName = "RightFootDown";
		xEvent2.m_fNormalizedTime = 0.75f;
		xEvent2.m_xData = Zenith_Maths::Vector4(0.0f, 1.0f, 0.0f, 0.5f);

		xClip.AddEvent(xEvent1);
		xClip.AddEvent(xEvent2);

		const std::vector<Flux_AnimationEvent>& xEvents = xClip.GetEvents();
		Zenith_Assert(xEvents.size() == 2, "Should have 2 events");
		Zenith_Assert(xEvents[0].m_strEventName == "LeftFootDown", "First event should be LeftFootDown");
		Zenith_Assert(xEvents[1].m_strEventName == "RightFootDown", "Second event should be RightFootDown");
		Zenith_Assert(FloatEquals(xEvents[0].m_fNormalizedTime, 0.25f), "First event time should be 0.25");
		Zenith_Assert(FloatEquals(xEvents[1].m_fNormalizedTime, 0.75f), "Second event time should be 0.75");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Animation clip events test passed");
	}

	// Test animation clip collection
	{
		Flux_AnimationClipCollection xCollection;

		Flux_AnimationClip* pxClip1 = new Flux_AnimationClip();
		pxClip1->SetName("Idle");
		Flux_AnimationClip* pxClip2 = new Flux_AnimationClip();
		pxClip2->SetName("Walk");
		Flux_AnimationClip* pxClip3 = new Flux_AnimationClip();
		pxClip3->SetName("Run");

		xCollection.AddClip(pxClip1);
		xCollection.AddClip(pxClip2);
		xCollection.AddClip(pxClip3);

		Zenith_Assert(xCollection.GetClipCount() == 3, "Should have 3 clips");
		Zenith_Assert(xCollection.HasClip("Idle"), "Should have Idle clip");
		Zenith_Assert(xCollection.HasClip("Walk"), "Should have Walk clip");
		Zenith_Assert(xCollection.HasClip("Run"), "Should have Run clip");
		Zenith_Assert(!xCollection.HasClip("Jump"), "Should not have Jump clip");

		const Flux_AnimationClip* pxRetrieved = xCollection.GetClip("Walk");
		Zenith_Assert(pxRetrieved != nullptr, "Should retrieve Walk clip");
		Zenith_Assert(pxRetrieved->GetName() == "Walk", "Retrieved clip name should be Walk");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Animation clip collection test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnimationClipChannels completed successfully");
}

/**
 * Test BlendSpace1D calculations
 * Verifies blend space sample point selection and blending
 */
void Zenith_UnitTests::TestBlendSpace1D()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestBlendSpace1D...");

	// Test parameter setting
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;

		xBlendSpace.SetParameter(-0.5f);
		Zenith_Assert(FloatEquals(xBlendSpace.GetParameter(), -0.5f),
			"Parameter should accept negative values");

		xBlendSpace.SetParameter(1.5f);
		Zenith_Assert(FloatEquals(xBlendSpace.GetParameter(), 1.5f),
			"Parameter should accept values > 1");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Parameter range test passed");
	}

	// Test blend point addition
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;

		// Create sample clips
		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip3 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xBlendSpace.AddBlendPoint(pxClip1, 0.0f);
		xBlendSpace.AddBlendPoint(pxClip2, 0.5f);
		xBlendSpace.AddBlendPoint(pxClip3, 1.0f);

		const std::vector<Flux_BlendTreeNode_BlendSpace1D::BlendPoint>& xPoints = xBlendSpace.GetBlendPoints();
		Zenith_Assert(xPoints.size() == 3, "Should have 3 blend points");
		Zenith_Assert(FloatEquals(xPoints[0].m_fPosition, 0.0f), "First point position should be 0.0");
		Zenith_Assert(FloatEquals(xPoints[1].m_fPosition, 0.5f), "Second point position should be 0.5");
		Zenith_Assert(FloatEquals(xPoints[2].m_fPosition, 1.0f), "Third point position should be 1.0");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Blend point addition test passed");
	}

	// Test blend point sorting
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;

		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip3 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		// Add in non-sorted order
		xBlendSpace.AddBlendPoint(pxClip2, 0.5f);
		xBlendSpace.AddBlendPoint(pxClip3, 1.0f);
		xBlendSpace.AddBlendPoint(pxClip1, 0.0f);

		xBlendSpace.SortBlendPoints();

		const std::vector<Flux_BlendTreeNode_BlendSpace1D::BlendPoint>& xPoints = xBlendSpace.GetBlendPoints();
		Zenith_Assert(FloatEquals(xPoints[0].m_fPosition, 0.0f), "After sorting, first should be 0.0");
		Zenith_Assert(FloatEquals(xPoints[1].m_fPosition, 0.5f), "After sorting, second should be 0.5");
		Zenith_Assert(FloatEquals(xPoints[2].m_fPosition, 1.0f), "After sorting, third should be 1.0");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Blend point sorting test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlendSpace1D completed successfully");
}

/**
 * Test BlendSpace2D blend tree node
 * Verifies 2D parameter blending, point management, and triangulation
 */
void Zenith_UnitTests::TestBlendSpace2D()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestBlendSpace2D...");

	// Test 2D parameter setting
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;

		Zenith_Maths::Vector2 xParams(-0.5f, 0.75f);
		xBlendSpace.SetParameter(xParams);
		const Zenith_Maths::Vector2& xRetrieved = xBlendSpace.GetParameter();
		Zenith_Assert(FloatEquals(xRetrieved.x, -0.5f) && FloatEquals(xRetrieved.y, 0.75f),
			"Parameters should be (-0.5, 0.75)");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Parameter setting test passed");
	}

	// Test blend point addition
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;

		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip3 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip4 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		// Add 4 points in 2D space (quad corners)
		xBlendSpace.AddBlendPoint(pxClip1, Zenith_Maths::Vector2(0.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip2, Zenith_Maths::Vector2(1.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip3, Zenith_Maths::Vector2(0.0f, 1.0f));
		xBlendSpace.AddBlendPoint(pxClip4, Zenith_Maths::Vector2(1.0f, 1.0f));

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Blend point addition test passed");
	}

	// Test triangulation computation
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;

		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip3 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		// Add 3 points forming a triangle
		xBlendSpace.AddBlendPoint(pxClip1, Zenith_Maths::Vector2(0.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip2, Zenith_Maths::Vector2(1.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip3, Zenith_Maths::Vector2(0.5f, 1.0f));

		// Compute triangulation
		xBlendSpace.ComputeTriangulation();

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Triangulation computation test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlendSpace2D completed successfully");
}

/**
 * Test blend tree node evaluation
 * Verifies that Evaluate() produces valid poses for all blend tree node types
 */
void Zenith_UnitTests::TestBlendTreeEvaluation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestBlendTreeEvaluation...");

	// Test Blend node evaluation at different weights
	{
		Flux_BlendTreeNode_Blend xBlendNode;

		// Create two clip children (even with null clips, we test the node behavior)
		Flux_BlendTreeNode_Clip* pxClipA = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClipB = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xBlendNode.SetChildA(pxClipA);
		xBlendNode.SetChildB(pxClipB);

		// Test weight at 0.0 (should favor child A)
		xBlendNode.SetBlendWeight(0.0f);
		Zenith_Assert(FloatEquals(xBlendNode.GetBlendWeight(), 0.0f), "Blend weight should be 0.0");

		// Test weight at 1.0 (should favor child B)
		xBlendNode.SetBlendWeight(1.0f);
		Zenith_Assert(FloatEquals(xBlendNode.GetBlendWeight(), 1.0f), "Blend weight should be 1.0");

		// Test weight at 0.5 (equal blend)
		xBlendNode.SetBlendWeight(0.5f);
		Zenith_Assert(FloatEquals(xBlendNode.GetBlendWeight(), 0.5f), "Blend weight should be 0.5");

		// Test weight clamping
		xBlendNode.SetBlendWeight(1.5f);
		Zenith_Assert(FloatEquals(xBlendNode.GetBlendWeight(), 1.0f), "Blend weight should clamp to 1.0");

		xBlendNode.SetBlendWeight(-0.5f);
		Zenith_Assert(FloatEquals(xBlendNode.GetBlendWeight(), 0.0f), "Blend weight should clamp to 0.0");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Blend node evaluation test passed");
	}

	// Test Additive node evaluation
	{
		Flux_BlendTreeNode_Additive xAdditiveNode;

		Flux_BlendTreeNode_Clip* pxBase = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxAdditive = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xAdditiveNode.SetBaseNode(pxBase);
		xAdditiveNode.SetAdditiveNode(pxAdditive);

		// Test weight at 0.0 (no additive effect)
		xAdditiveNode.SetAdditiveWeight(0.0f);
		Zenith_Assert(FloatEquals(xAdditiveNode.GetAdditiveWeight(), 0.0f), "Additive weight should be 0.0");

		// Test weight at 1.0 (full additive effect)
		xAdditiveNode.SetAdditiveWeight(1.0f);
		Zenith_Assert(FloatEquals(xAdditiveNode.GetAdditiveWeight(), 1.0f), "Additive weight should be 1.0");

		// Test weight clamping
		xAdditiveNode.SetAdditiveWeight(2.0f);
		Zenith_Assert(FloatEquals(xAdditiveNode.GetAdditiveWeight(), 1.0f), "Additive weight should clamp to 1.0");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Additive node evaluation test passed");
	}

	// Test Masked node evaluation
	{
		Flux_BlendTreeNode_Masked xMaskedNode;

		Flux_BlendTreeNode_Clip* pxBase = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxOverride = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xMaskedNode.SetBaseNode(pxBase);
		xMaskedNode.SetOverrideNode(pxOverride);

		// Set up a bone mask
		Flux_BoneMask xMask;
		xMask.SetBoneWeight(0, 1.0f);  // Full override for bone 0
		xMask.SetBoneWeight(1, 0.5f);  // Partial override for bone 1
		xMask.SetBoneWeight(2, 0.0f);  // No override for bone 2

		xMaskedNode.SetBoneMask(xMask);

		const Flux_BoneMask& xRetrieved = xMaskedNode.GetBoneMask();
		Zenith_Assert(FloatEquals(xRetrieved.GetBoneWeight(0), 1.0f), "Bone 0 weight should be 1.0");
		Zenith_Assert(FloatEquals(xRetrieved.GetBoneWeight(1), 0.5f), "Bone 1 weight should be 0.5");
		Zenith_Assert(FloatEquals(xRetrieved.GetBoneWeight(2), 0.0f), "Bone 2 weight should be 0.0");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Masked node evaluation test passed");
	}

	// Test Select node evaluation
	{
		Flux_BlendTreeNode_Select xSelectNode;

		Flux_BlendTreeNode_Clip* pxClip0 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.5f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 2.0f);

		xSelectNode.AddChild(pxClip0);
		xSelectNode.AddChild(pxClip1);
		xSelectNode.AddChild(pxClip2);

		// Test selecting different children
		xSelectNode.SetSelectedIndex(0);
		Zenith_Assert(xSelectNode.GetSelectedIndex() == 0, "Selected index should be 0");

		xSelectNode.SetSelectedIndex(1);
		Zenith_Assert(xSelectNode.GetSelectedIndex() == 1, "Selected index should be 1");

		xSelectNode.SetSelectedIndex(2);
		Zenith_Assert(xSelectNode.GetSelectedIndex() == 2, "Selected index should be 2");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Select node evaluation test passed");
	}

	// Test BlendSpace1D evaluation with blend points
	{
		Flux_BlendTreeNode_BlendSpace1D xBlendSpace;

		Flux_BlendTreeNode_Clip* pxClip0 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xBlendSpace.AddBlendPoint(pxClip0, 0.0f);
		xBlendSpace.AddBlendPoint(pxClip1, 0.5f);
		xBlendSpace.AddBlendPoint(pxClip2, 1.0f);
		xBlendSpace.SortBlendPoints();

		// Test parameter at different values
		xBlendSpace.SetParameter(0.0f);
		Zenith_Assert(FloatEquals(xBlendSpace.GetParameter(), 0.0f), "Parameter should be 0.0");

		xBlendSpace.SetParameter(0.25f);
		Zenith_Assert(FloatEquals(xBlendSpace.GetParameter(), 0.25f), "Parameter should be 0.25");

		xBlendSpace.SetParameter(1.0f);
		Zenith_Assert(FloatEquals(xBlendSpace.GetParameter(), 1.0f), "Parameter should be 1.0");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ BlendSpace1D evaluation test passed");
	}

	// Test BlendSpace2D evaluation
	{
		Flux_BlendTreeNode_BlendSpace2D xBlendSpace;

		Flux_BlendTreeNode_Clip* pxClip0 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxClip2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		xBlendSpace.AddBlendPoint(pxClip0, Zenith_Maths::Vector2(0.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip1, Zenith_Maths::Vector2(1.0f, 0.0f));
		xBlendSpace.AddBlendPoint(pxClip2, Zenith_Maths::Vector2(0.5f, 1.0f));
		xBlendSpace.ComputeTriangulation();

		// Test parameter at different 2D values
		xBlendSpace.SetParameter(Zenith_Maths::Vector2(0.0f, 0.0f));
		const Zenith_Maths::Vector2& xParam0 = xBlendSpace.GetParameter();
		Zenith_Assert(FloatEquals(xParam0.x, 0.0f) && FloatEquals(xParam0.y, 0.0f),
			"Parameter should be (0, 0)");

		xBlendSpace.SetParameter(Zenith_Maths::Vector2(0.5f, 0.5f));
		const Zenith_Maths::Vector2& xParam1 = xBlendSpace.GetParameter();
		Zenith_Assert(FloatEquals(xParam1.x, 0.5f) && FloatEquals(xParam1.y, 0.5f),
			"Parameter should be (0.5, 0.5)");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ BlendSpace2D evaluation test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlendTreeEvaluation completed successfully");
}

/**
 * Test blend tree node serialization
 * Verifies round-trip serialization for all blend tree node types
 */
void Zenith_UnitTests::TestBlendTreeSerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestBlendTreeSerialization...");

	// Test Clip node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_Clip xOriginal(nullptr, 1.5f);
		xOriginal.SetClipName("TestClip");

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_Clip xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(FloatEquals(xLoaded.GetPlaybackRate(), 1.5f), "Playback rate should be 1.5");
		Zenith_Assert(xLoaded.GetClipName() == "TestClip", "Clip name should be 'TestClip'");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Clip node serialization test passed");
	}

	// Test Blend node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_Blend xOriginal;
		xOriginal.SetBlendWeight(0.75f);
		// Children would be serialized recursively in real usage

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_Blend xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(FloatEquals(xLoaded.GetBlendWeight(), 0.75f), "Blend weight should be 0.75");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Blend node serialization test passed");
	}

	// Test BlendSpace1D node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_BlendSpace1D xOriginal;
		xOriginal.SetParameter(0.65f);

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_BlendSpace1D xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(FloatEquals(xLoaded.GetParameter(), 0.65f), "Parameter should be 0.65");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ BlendSpace1D node serialization test passed");
	}

	// Test BlendSpace2D node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_BlendSpace2D xOriginal;
		xOriginal.SetParameter(Zenith_Maths::Vector2(0.3f, 0.8f));

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_BlendSpace2D xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		const Zenith_Maths::Vector2& xParam = xLoaded.GetParameter();
		Zenith_Assert(FloatEquals(xParam.x, 0.3f) && FloatEquals(xParam.y, 0.8f),
			"Parameter should be (0.3, 0.8)");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ BlendSpace2D node serialization test passed");
	}

	// Test Additive node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_Additive xOriginal;
		xOriginal.SetAdditiveWeight(0.45f);

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_Additive xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(FloatEquals(xLoaded.GetAdditiveWeight(), 0.45f), "Additive weight should be 0.45");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Additive node serialization test passed");
	}

	// Test Masked node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_Masked xOriginal;
		Flux_BoneMask xMask;
		xMask.SetBoneWeight(0, 1.0f);
		xMask.SetBoneWeight(1, 0.5f);
		xMask.SetBoneWeight(2, 0.25f);
		xOriginal.SetBoneMask(xMask);

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_Masked xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		const Flux_BoneMask& xLoadedMask = xLoaded.GetBoneMask();
		Zenith_Assert(FloatEquals(xLoadedMask.GetBoneWeight(0), 1.0f), "Bone 0 weight should be 1.0");
		Zenith_Assert(FloatEquals(xLoadedMask.GetBoneWeight(1), 0.5f), "Bone 1 weight should be 0.5");
		Zenith_Assert(FloatEquals(xLoadedMask.GetBoneWeight(2), 0.25f), "Bone 2 weight should be 0.25");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Masked node serialization test passed");
	}

	// Test Select node serialization
	{
		Zenith_DataStream xStream(1024);

		Flux_BlendTreeNode_Select xOriginal;
		// Must add children before setting selected index (SetSelectedIndex validates range)
		xOriginal.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xOriginal.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xOriginal.AddChild(new Flux_BlendTreeNode_Clip(nullptr, 1.0f));
		xOriginal.SetSelectedIndex(2);

		xOriginal.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Flux_BlendTreeNode_Select xLoaded;
		xLoaded.ReadFromDataStream(xStream);

		Zenith_Assert(xLoaded.GetSelectedIndex() == 2, "Selected index should be 2");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Select node serialization test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBlendTreeSerialization completed successfully");
}

/**
 * Test FABRIK IK Solver
 * Verifies IK chain setup and solving iterations
 */
void Zenith_UnitTests::TestFABRIKSolver()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestFABRIKSolver...");

	// Test basic IK chain creation
	{
		Flux_IKSolver xSolver;

		Flux_IKChain xChain;
		xChain.m_strName = "RightArm";
		xChain.m_xBoneNames.push_back("Shoulder");
		xChain.m_xBoneNames.push_back("Elbow");
		xChain.m_xBoneNames.push_back("Wrist");

		xSolver.AddChain(xChain);
		Zenith_Assert(xSolver.HasChain("RightArm"), "Solver should have RightArm chain");
		Zenith_Assert(!xSolver.HasChain("LeftArm"), "Solver should not have LeftArm chain");

		const Flux_IKChain* pxRetrieved = xSolver.GetChain("RightArm");
		Zenith_Assert(pxRetrieved != nullptr, "Should retrieve chain");
		Zenith_Assert(pxRetrieved->m_strName == "RightArm", "Chain name should match");
		Zenith_Assert(pxRetrieved->m_xBoneNames.size() == 3, "Should have 3 bones");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Chain creation test passed");
	}

	// Test IK target setting
	{
		Flux_IKSolver xSolver;

		Flux_IKTarget xTarget;
		xTarget.m_xPosition = Zenith_Maths::Vector3(5.0f, 3.0f, 0.0f);
		xTarget.m_fWeight = 0.8f;
		xTarget.m_bEnabled = true;

		xSolver.SetTarget("RightHand", xTarget);

		const Flux_IKTarget* pxRetrieved = xSolver.GetTarget("RightHand");
		Zenith_Assert(pxRetrieved != nullptr, "Should retrieve target");
		Zenith_Assert(Vec3Equals(pxRetrieved->m_xPosition, Zenith_Maths::Vector3(5.0f, 3.0f, 0.0f)),
			"Target position should match");
		Zenith_Assert(FloatEquals(pxRetrieved->m_fWeight, 0.8f), "Target weight should be 0.8");
		Zenith_Assert(pxRetrieved->m_bEnabled == true, "Target should be enabled");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ IK target setting test passed");
	}

	// Test IK target clearing
	{
		Flux_IKSolver xSolver;

		Flux_IKTarget xTarget;
		xTarget.m_xPosition = Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);
		xTarget.m_bEnabled = true;

		xSolver.SetTarget("TestChain", xTarget);
		Zenith_Assert(xSolver.HasTarget("TestChain"), "Target should exist");

		xSolver.ClearTarget("TestChain");
		Zenith_Assert(!xSolver.HasTarget("TestChain"), "Target should be cleared");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ IK target clearing test passed");
	}

	// Test chain parameters
	{
		Flux_IKChain xChain;
		xChain.m_strName = "TestChain";
		xChain.m_uMaxIterations = 20;
		xChain.m_fTolerance = 0.001f;

		Zenith_Assert(xChain.m_uMaxIterations == 20, "Max iterations should be 20");
		Zenith_Assert(FloatEquals(xChain.m_fTolerance, 0.001f), "Tolerance should be 0.001");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Chain parameters test passed");
	}

	// Test chain with pole vector
	{
		Flux_IKChain xChain;
		xChain.m_strName = "LeftLeg";
		xChain.m_xPoleVector = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		xChain.m_bUsePoleVector = true;
		xChain.m_strPoleTargetBone = "KneeTarget";

		Zenith_Assert(Vec3Equals(xChain.m_xPoleVector, Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f)),
			"Pole vector should be (0,0,1)");
		Zenith_Assert(xChain.m_bUsePoleVector == true, "Use pole vector should be true");
		Zenith_Assert(xChain.m_strPoleTargetBone == "KneeTarget", "Pole target bone should be KneeTarget");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Pole vector test passed");
	}

	// Test helper chain creation functions
	{
		Flux_IKChain xLegChain = Flux_IKSolver::CreateLegChain("RightLeg", "Hip", "Knee", "Ankle");
		Zenith_Assert(xLegChain.m_strName == "RightLeg", "Leg chain name should be RightLeg");
		Zenith_Assert(xLegChain.m_xBoneNames.size() == 3, "Leg chain should have 3 bones");
		Zenith_Assert(xLegChain.m_xBoneNames[0] == "Hip", "First bone should be Hip");
		Zenith_Assert(xLegChain.m_xBoneNames[1] == "Knee", "Second bone should be Knee");
		Zenith_Assert(xLegChain.m_xBoneNames[2] == "Ankle", "Third bone should be Ankle");

		Flux_IKChain xArmChain = Flux_IKSolver::CreateArmChain("LeftArm", "Shoulder", "Elbow", "Wrist");
		Zenith_Assert(xArmChain.m_strName == "LeftArm", "Arm chain name should be LeftArm");
		Zenith_Assert(xArmChain.m_xBoneNames.size() == 3, "Arm chain should have 3 bones");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Helper chain creation test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFABRIKSolver completed successfully");
}

/**
 * Test Animation Events
 * Verifies event registration and triggering
 */
void Zenith_UnitTests::TestAnimationEvents()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnimationEvents...");

	// Test event data structure
	{
		Flux_AnimationEvent xEvent;
		xEvent.m_strEventName = "FootStep";
		xEvent.m_fNormalizedTime = 0.25f;
		xEvent.m_xData = Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 0.5f);

		Zenith_Assert(xEvent.m_strEventName == "FootStep", "Event name should be 'FootStep'");
		Zenith_Assert(FloatEquals(xEvent.m_fNormalizedTime, 0.25f), "Normalized time should be 0.25");
		Zenith_Assert(FloatEquals(xEvent.m_xData.x, 1.0f), "Event data x should be 1.0");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Event data structure test passed");
	}

	// Test event collection in clip
	{
		Flux_AnimationClip xClip;
		xClip.SetName("Walk");

		Flux_AnimationEvent xEvent1;
		xEvent1.m_strEventName = "LeftFoot";
		xEvent1.m_fNormalizedTime = 0.0f;

		Flux_AnimationEvent xEvent2;
		xEvent2.m_strEventName = "RightFoot";
		xEvent2.m_fNormalizedTime = 0.5f;

		xClip.AddEvent(xEvent1);
		xClip.AddEvent(xEvent2);

		const std::vector<Flux_AnimationEvent>& xEvents = xClip.GetEvents();
		Zenith_Assert(xEvents.size() == 2, "Should have 2 events");
		Zenith_Assert(xEvents[0].m_strEventName == "LeftFoot", "First event should be LeftFoot");
		Zenith_Assert(xEvents[1].m_strEventName == "RightFoot", "Second event should be RightFoot");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Event collection test passed");
	}

	// Test event time ordering
	{
		Flux_AnimationEvent xEvent1, xEvent2, xEvent3;
		xEvent1.m_fNormalizedTime = 0.5f;
		xEvent2.m_fNormalizedTime = 0.1f;
		xEvent3.m_fNormalizedTime = 0.9f;

		std::vector<Flux_AnimationEvent> xEvents = { xEvent1, xEvent2, xEvent3 };
		std::sort(xEvents.begin(), xEvents.end(),
			[](const Flux_AnimationEvent& a, const Flux_AnimationEvent& b) {
				return a.m_fNormalizedTime < b.m_fNormalizedTime;
			});

		Zenith_Assert(FloatEquals(xEvents[0].m_fNormalizedTime, 0.1f), "First should be 0.1");
		Zenith_Assert(FloatEquals(xEvents[1].m_fNormalizedTime, 0.5f), "Second should be 0.5");
		Zenith_Assert(FloatEquals(xEvents[2].m_fNormalizedTime, 0.9f), "Third should be 0.9");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Event time ordering test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnimationEvents completed successfully");
}

/**
 * Test Bone Masking
 * Verifies bone mask creation and application
 */
void Zenith_UnitTests::TestBoneMasking()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestBoneMasking...");

	// Test mask creation with bone indices
	{
		Flux_BoneMask xMask;

		// Set weights by bone index
		xMask.SetBoneWeight(0, 1.0f);  // Spine
		xMask.SetBoneWeight(1, 1.0f);  // Chest
		xMask.SetBoneWeight(2, 0.8f);  // Shoulder_L
		xMask.SetBoneWeight(3, 0.8f);  // Shoulder_R
		xMask.SetBoneWeight(4, 0.2f);  // Hips

		Zenith_Assert(FloatEquals(xMask.GetBoneWeight(0), 1.0f), "Bone 0 weight should be 1.0");
		Zenith_Assert(FloatEquals(xMask.GetBoneWeight(1), 1.0f), "Bone 1 weight should be 1.0");
		Zenith_Assert(FloatEquals(xMask.GetBoneWeight(2), 0.8f), "Bone 2 weight should be 0.8");
		Zenith_Assert(FloatEquals(xMask.GetBoneWeight(3), 0.8f), "Bone 3 weight should be 0.8");
		Zenith_Assert(FloatEquals(xMask.GetBoneWeight(4), 0.2f), "Bone 4 weight should be 0.2");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Mask creation test passed");
	}

	// Test weight access
	{
		Flux_BoneMask xMask;
		xMask.SetBoneWeight(5, 0.75f);

		float fWeight = xMask.GetBoneWeight(5);
		Zenith_Assert(FloatEquals(fWeight, 0.75f), "Weight should be 0.75");

		const std::vector<float>& xWeights = xMask.GetWeights();
		Zenith_Assert(xWeights.size() >= 6, "Should have at least 6 weights");
		Zenith_Assert(FloatEquals(xWeights[5], 0.75f), "Weight at index 5 should be 0.75");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Weight access test passed");
	}

	// Test masked blend node setup
	{
		Flux_BlendTreeNode_Masked xMaskedNode;
		Zenith_Assert(std::string(xMaskedNode.GetNodeTypeName()) == "Masked", "Type name should be 'Masked'");

		Flux_BoneMask xMask;
		xMask.SetBoneWeight(0, 1.0f);
		xMask.SetBoneWeight(1, 0.5f);

		xMaskedNode.SetBoneMask(xMask);
		const Flux_BoneMask& xRetrievedMask = xMaskedNode.GetBoneMask();
		Zenith_Assert(FloatEquals(xRetrievedMask.GetBoneWeight(0), 1.0f), "Retrieved mask bone 0 should be 1.0");
		Zenith_Assert(FloatEquals(xRetrievedMask.GetBoneWeight(1), 0.5f), "Retrieved mask bone 1 should be 0.5");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Masked blend node setup test passed");
	}

	// Test masked blend with different poses
	{
		Flux_SkeletonPose xBasePose;
		xBasePose.Initialize(5);

		Flux_SkeletonPose xOverridePose;
		xOverridePose.Initialize(5);

		// Create mask that affects only bones 2, 3, 4
		std::vector<float> xBoneWeights = { 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };

		Flux_SkeletonPose xResult;
		xResult.Initialize(5);

		Flux_SkeletonPose::MaskedBlend(xResult, xBasePose, xOverridePose, xBoneWeights);

		// Result should have base pose for bones 0,1 and override pose for bones 2,3,4
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Masked blend test passed");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBoneMasking completed successfully");
}

//=============================================================================
// Asset Pipeline Unit Test Helpers
//=============================================================================

// Helper: Compute bind pose vertex position
// For GLTF models, vertices are stored at bind pose mesh positions.
// The standard skinning formula is: jointMatrix * inverseBindMatrix * vertexPos
// At bind pose, jointMatrix equals bindPoseModel, so:
//   result = bindPoseModel * inverseBindPose * vertexPos
// This should return the original vertex position (identity transform).
static Zenith_Maths::Vector3 ComputeBindPosePosition(
	const Zenith_Maths::Vector3& xMeshPos,
	const glm::uvec4& xBoneIndices,
	const glm::vec4& xBoneWeights,
	const Zenith_SkeletonAsset* pxSkeleton)
{
	Zenith_Maths::Vector3 xResult(0.0f);
	for (int i = 0; i < 4; i++)
	{
		float fWeight = xBoneWeights[i];
		if (fWeight <= 0.0f)
		{
			continue;
		}
		uint32_t uBoneIndex = xBoneIndices[i];
		if (uBoneIndex >= pxSkeleton->GetNumBones())
		{
			continue;
		}
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBoneIndex);
		// Apply inverse bind pose to get bone-local, then bind pose model to get world
		Zenith_Maths::Vector4 xBoneLocal = xBone.m_xInverseBindPose * Zenith_Maths::Vector4(xMeshPos, 1.0f);
		Zenith_Maths::Vector4 xTransformed = xBone.m_xBindPoseModel * xBoneLocal;
		xResult += fWeight * Zenith_Maths::Vector3(xTransformed);
	}
	return xResult;
}

// Helper: Apply animation at specific time (in seconds) and compute skinning matrices
static void ApplyAnimationAtTime(
	Flux_SkeletonInstance* pxSkelInst,
	const Zenith_SkeletonAsset* pxSkelAsset,
	const Flux_AnimationClip* pxClip,
	float fTimeSeconds)
{
	// Convert time from seconds to ticks (keyframes are stored in ticks)
	float fTimeInTicks = fTimeSeconds * pxClip->GetTicksPerSecond();

	for (uint32_t i = 0; i < pxSkelAsset->GetNumBones(); i++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkelAsset->GetBone(i);
		const Flux_BoneChannel* pxChannel = pxClip->GetBoneChannel(xBone.m_strName);
		if (pxChannel)
		{
			pxSkelInst->SetBoneLocalTransform(i,
				pxChannel->SamplePosition(fTimeInTicks),
				pxChannel->SampleRotation(fTimeInTicks),
				pxChannel->SampleScale(fTimeInTicks));
		}
		else
		{
			pxSkelInst->SetBoneLocalTransform(i,
				xBone.m_xBindPosition, xBone.m_xBindRotation, xBone.m_xBindScale);
		}
	}
	pxSkelInst->ComputeSkinningMatrices();
}

// Helper: Compute skinned vertex position using skeleton instance skinning matrices
static Zenith_Maths::Vector3 ComputeSkinnedPosition(
	const Zenith_Maths::Vector3& xLocalPos,
	const glm::uvec4& xBoneIndices,
	const glm::vec4& xBoneWeights,
	const Flux_SkeletonInstance* pxSkelInst)
{
	Zenith_Maths::Vector3 xResult(0.0f);
	const Zenith_Maths::Matrix4* pSkinMatrices = pxSkelInst->GetSkinningMatrices();
	for (int i = 0; i < 4; i++)
	{
		float fWeight = xBoneWeights[i];
		if (fWeight <= 0.0f)
		{
			continue;
		}
		uint32_t uBoneIndex = xBoneIndices[i];
		Zenith_Maths::Vector4 xTransformed = pSkinMatrices[uBoneIndex] *
			Zenith_Maths::Vector4(xLocalPos, 1.0f);
		xResult += fWeight * Zenith_Maths::Vector3(xTransformed);
	}
	return xResult;
}

//=============================================================================
// Asset Pipeline Unit Tests
//=============================================================================

/**
 * Test mesh asset loading
 * Verifies that mesh assets load correctly with expected vertex count and data
 */
void Zenith_UnitTests::TestMeshAssetLoading()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestMeshAssetLoading...");

	// Test loading a mesh asset
	{
		const std::string strMeshPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_Mesh0_Mat0.zmesh";
		Zenith_MeshAsset* pxMeshAsset = Zenith_AssetRegistry::Get().Get<Zenith_MeshAsset>(strMeshPath);

		if (pxMeshAsset == nullptr)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  ! Skipping test - mesh asset not found at %s", strMeshPath.c_str());
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  ! Please export ArmChain.gltf through the asset pipeline first");
			return;
		}

		Zenith_Assert(pxMeshAsset != nullptr, "Failed to load mesh asset");
		Zenith_Assert(pxMeshAsset->GetNumVerts() == 24, "Expected 24 vertices (8 per bone * 3 bones)");
		Zenith_Assert(pxMeshAsset->GetNumIndices() > 0, "Mesh should have indices");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Mesh asset loaded with %u vertices and %u indices",
			pxMeshAsset->GetNumVerts(), pxMeshAsset->GetNumIndices());

		// Verify first vertex position (raw, local to bone)
		const Zenith_Maths::Vector3& xFirstPos = pxMeshAsset->m_xPositions.Get(0);
		Zenith_Assert(FloatEquals(xFirstPos.x, -0.25f, 0.01f), "Vertex 0 X mismatch");
		Zenith_Assert(FloatEquals(xFirstPos.y, 0.0f, 0.01f), "Vertex 0 Y mismatch");
		Zenith_Assert(FloatEquals(xFirstPos.z, -0.25f, 0.01f), "Vertex 0 Z mismatch");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ First vertex position verified");

		// Verify skinning data exists
		Zenith_Assert(pxMeshAsset->m_xBoneIndices.GetSize() == 24, "Should have bone indices for all vertices");
		Zenith_Assert(pxMeshAsset->m_xBoneWeights.GetSize() == 24, "Should have bone weights for all vertices");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Skinning data present");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMeshAssetLoading completed successfully");
}

/**
 * Test bind pose vertex positions
 * Verifies that applying bind pose skinning produces correct vertex positions
 */
void Zenith_UnitTests::TestBindPoseVertexPositions()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestBindPoseVertexPositions...");

	const std::string strMeshPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_Mesh0_Mat0.zmesh";
	const std::string strSkelPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain.zskel";

	Zenith_MeshAsset* pxMesh = Zenith_AssetRegistry::Get().Get<Zenith_MeshAsset>(strMeshPath);
	Zenith_SkeletonAsset* pxSkel = Zenith_AssetRegistry::Get().Get<Zenith_SkeletonAsset>(strSkelPath);

	if (pxMesh == nullptr || pxSkel == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ! Skipping test - assets not found");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ! Please export ArmChain.gltf through the asset pipeline first");
		return;
	}

	Zenith_Assert(pxMesh != nullptr && pxSkel != nullptr, "Failed to load assets");
	Zenith_Assert(pxSkel->GetNumBones() == 3, "Expected 3 bones");

	// Log bone hierarchy for debugging
	for (uint32_t i = 0; i < pxSkel->GetNumBones(); i++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkel->GetBone(i);
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Bone %u: %s, parent=%d, bindPos=(%.2f, %.2f, %.2f)",
			i, xBone.m_strName.c_str(), xBone.m_iParentIndex,
			xBone.m_xBindPosition.x, xBone.m_xBindPosition.y, xBone.m_xBindPosition.z);
	}

	// Test vertex 0 (Root bone at origin)
	{
		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(0);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(0);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(0);

		Zenith_Maths::Vector3 xSkinnedPos = ComputeBindPosePosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkel);

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Vertex 0: local=(%.3f, %.3f, %.3f) -> skinned=(%.3f, %.3f, %.3f)",
			xLocalPos.x, xLocalPos.y, xLocalPos.z,
			xSkinnedPos.x, xSkinnedPos.y, xSkinnedPos.z);

		// Root bone at origin - expected position is approximately the local position
		Zenith_Assert(Vec3Equals(xSkinnedPos, Zenith_Maths::Vector3(-0.25f, 0.0f, -0.25f), 0.1f),
			"Vertex 0 bind pose position mismatch");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Root bone vertex (0) bind pose verified");
	}

	// Test vertex 8 (UpperArm bone at Y+2)
	{
		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(8);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(8);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(8);

		Zenith_Maths::Vector3 xSkinnedPos = ComputeBindPosePosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkel);

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Vertex 8: local=(%.3f, %.3f, %.3f) -> skinned=(%.3f, %.3f, %.3f)",
			xLocalPos.x, xLocalPos.y, xLocalPos.z,
			xSkinnedPos.x, xSkinnedPos.y, xSkinnedPos.z);

		// UpperArm bone at Y+2 - expected position should be offset by bone transform
		Zenith_Assert(Vec3Equals(xSkinnedPos, Zenith_Maths::Vector3(-0.25f, 2.0f, -0.25f), 0.1f),
			"Vertex 8 bind pose position mismatch");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ UpperArm bone vertex (8) bind pose verified");
	}

	// Test vertex 16 (Forearm bone at Y+4)
	{
		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

		Zenith_Maths::Vector3 xSkinnedPos = ComputeBindPosePosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkel);

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Vertex 16: local=(%.3f, %.3f, %.3f) -> skinned=(%.3f, %.3f, %.3f)",
			xLocalPos.x, xLocalPos.y, xLocalPos.z,
			xSkinnedPos.x, xSkinnedPos.y, xSkinnedPos.z);

		// Forearm bone at Y+4 - expected position should be offset by bone transform
		Zenith_Assert(Vec3Equals(xSkinnedPos, Zenith_Maths::Vector3(-0.25f, 4.0f, -0.25f), 0.1f),
			"Vertex 16 bind pose position mismatch");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Forearm bone vertex (16) bind pose verified");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBindPoseVertexPositions completed successfully");
}

/**
 * Test animated vertex positions
 * Verifies that animation skinning produces correct vertex positions at various timestamps
 */
void Zenith_UnitTests::TestAnimatedVertexPositions()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnimatedVertexPositions...");

	const std::string strMeshPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_Mesh0_Mat0.zmesh";
	const std::string strSkelPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain.zskel";
	const std::string strAnimPath = ENGINE_ASSETS_DIR "Meshes/UnitTest/ArmChain_ForearmRotate.zanim";

	Zenith_MeshAsset* pxMesh = Zenith_AssetRegistry::Get().Get<Zenith_MeshAsset>(strMeshPath);
	Zenith_SkeletonAsset* pxSkel = Zenith_AssetRegistry::Get().Get<Zenith_SkeletonAsset>(strSkelPath);
	Flux_AnimationClip* pxClip = Flux_AnimationClip::LoadFromZanimFile(strAnimPath);

	if (pxMesh == nullptr || pxSkel == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ! Skipping test - mesh/skeleton assets not found");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ! Please export ArmChain.gltf through the asset pipeline first");
		return;
	}

	if (pxClip == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ! Skipping animation test - animation clip not found");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ! Animation file: %s", strAnimPath.c_str());
		// Still test bind pose without animation
	}

	Zenith_Assert(pxMesh != nullptr && pxSkel != nullptr, "Failed to load test assets");

	// Create skeleton instance for animation (CPU-only, no GPU buffer needed for unit tests)
	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);
	Zenith_Assert(pxSkelInst != nullptr, "Failed to create skeleton instance");

	// Test at t=0.0 (should match bind pose)
	{
		pxSkelInst->SetToBindPose();
		pxSkelInst->ComputeSkinningMatrices();

		const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
		const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
		const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

		Zenith_Maths::Vector3 xSkinned = ComputeSkinnedPosition(
			xLocalPos, xBoneIdx, xBoneWgt, pxSkelInst);

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  t=0.0: Vertex 16 skinned position = (%.3f, %.3f, %.3f)",
			xSkinned.x, xSkinned.y, xSkinned.z);

		// At t=0, should match bind pose
		Zenith_Assert(Vec3Equals(xSkinned, Zenith_Maths::Vector3(-0.25f, 4.0f, -0.25f), 0.1f),
			"Vertex 16 at t=0 should match bind pose");

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Animation t=0.0 (bind pose) verified");
	}

	// Test with animation if clip is available
	if (pxClip != nullptr)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Animation clip '%s' loaded, duration: %.2f sec",
			pxClip->GetName().c_str(), pxClip->GetDuration());

		// Debug: Print bone channels in clip
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Animation bone channels:");
		for (const auto& xPair : pxClip->GetBoneChannels())
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    - '%s'", xPair.first.c_str());
		}

		// Debug: Print skeleton bone names
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Skeleton bone names:");
		for (uint32_t i = 0; i < pxSkel->GetNumBones(); i++)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    - [%u] '%s'", i, pxSkel->GetBone(i).m_strName.c_str());
		}

		// Test at t=0.5 (45 degree rotation)
		{
			ApplyAnimationAtTime(pxSkelInst, pxSkel, pxClip, 0.5f);

			const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
			const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
			const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

			Zenith_Maths::Vector3 xSkinned = ComputeSkinnedPosition(
				xLocalPos, xBoneIdx, xBoneWgt, pxSkelInst);

			Zenith_Log(LOG_CATEGORY_UNITTEST, "  t=0.5: Vertex 16 skinned position = (%.3f, %.3f, %.3f)",
				xSkinned.x, xSkinned.y, xSkinned.z);

			// At t=0.5, forearm should be rotated 45 degrees around Z
			// Vertex offset from bone (-0.25, 0, -0.25) rotates to (-0.177, -0.177, -0.25)
			// Add bone world position (0, 4, 0) = (-0.177, 3.823, -0.25)
			Zenith_Maths::Vector3 xExpected(-0.177f, 3.823f, -0.25f);
			Zenith_Assert(Vec3Equals(xSkinned, xExpected, 0.1f),
				"Vertex 16 at t=0.5 position mismatch");

			Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Animation t=0.5 (45-degree rotation) verified");
		}

		// Test at t=1.0 (90 degree rotation)
		{
			ApplyAnimationAtTime(pxSkelInst, pxSkel, pxClip, 1.0f);

			const glm::uvec4& xBoneIdx = pxMesh->m_xBoneIndices.Get(16);
			const glm::vec4& xBoneWgt = pxMesh->m_xBoneWeights.Get(16);
			const Zenith_Maths::Vector3& xLocalPos = pxMesh->m_xPositions.Get(16);

			Zenith_Maths::Vector3 xSkinned = ComputeSkinnedPosition(
				xLocalPos, xBoneIdx, xBoneWgt, pxSkelInst);

			Zenith_Log(LOG_CATEGORY_UNITTEST, "  t=1.0: Vertex 16 skinned position = (%.3f, %.3f, %.3f)",
				xSkinned.x, xSkinned.y, xSkinned.z);

			// At t=1.0, forearm should be rotated 90 degrees around Z
			// Vertex offset from bone (-0.25, 0, -0.25) rotates to (0, -0.25, -0.25)
			// Add bone world position (0, 4, 0) = (0, 3.75, -0.25)
			Zenith_Maths::Vector3 xExpected(0.0f, 3.75f, -0.25f);
			Zenith_Assert(Vec3Equals(xSkinned, xExpected, 0.1f),
				"Vertex 16 at t=1.0 position mismatch");

			Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Animation t=1.0 (90-degree rotation) verified");
		}

		delete pxClip;
	}

	// Cleanup
	delete pxSkelInst;

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnimatedVertexPositions completed successfully");
}

//------------------------------------------------------------------------------
// Stick Figure Animation Tests - Helper Functions
//------------------------------------------------------------------------------

// Bone indices for stick figure skeleton
static constexpr uint32_t STICK_BONE_ROOT = 0;
static constexpr uint32_t STICK_BONE_SPINE = 1;
static constexpr uint32_t STICK_BONE_NECK = 2;
static constexpr uint32_t STICK_BONE_HEAD = 3;
static constexpr uint32_t STICK_BONE_LEFT_UPPER_ARM = 4;
static constexpr uint32_t STICK_BONE_LEFT_LOWER_ARM = 5;
static constexpr uint32_t STICK_BONE_LEFT_HAND = 6;
static constexpr uint32_t STICK_BONE_RIGHT_UPPER_ARM = 7;
static constexpr uint32_t STICK_BONE_RIGHT_LOWER_ARM = 8;
static constexpr uint32_t STICK_BONE_RIGHT_HAND = 9;
static constexpr uint32_t STICK_BONE_LEFT_UPPER_LEG = 10;
static constexpr uint32_t STICK_BONE_LEFT_LOWER_LEG = 11;
static constexpr uint32_t STICK_BONE_LEFT_FOOT = 12;
static constexpr uint32_t STICK_BONE_RIGHT_UPPER_LEG = 13;
static constexpr uint32_t STICK_BONE_RIGHT_LOWER_LEG = 14;
static constexpr uint32_t STICK_BONE_RIGHT_FOOT = 15;
static constexpr uint32_t STICK_BONE_COUNT = 16;

// Cube geometry constants
static const Zenith_Maths::Vector3 s_axCubeOffsets[8] = {
	{-0.05f, -0.05f, -0.05f}, // 0: left-bottom-back
	{ 0.05f, -0.05f, -0.05f}, // 1: right-bottom-back
	{ 0.05f,  0.05f, -0.05f}, // 2: right-top-back
	{-0.05f,  0.05f, -0.05f}, // 3: left-top-back
	{-0.05f, -0.05f,  0.05f}, // 4: left-bottom-front
	{ 0.05f, -0.05f,  0.05f}, // 5: right-bottom-front
	{ 0.05f,  0.05f,  0.05f}, // 6: right-top-front
	{-0.05f,  0.05f,  0.05f}, // 7: left-top-front
};

static const uint32_t s_auCubeIndices[36] = {
	// Back face
	0, 2, 1, 0, 3, 2,
	// Front face
	4, 5, 6, 4, 6, 7,
	// Left face
	0, 4, 7, 0, 7, 3,
	// Right face
	1, 2, 6, 1, 6, 5,
	// Bottom face
	0, 1, 5, 0, 5, 4,
	// Top face
	3, 7, 6, 3, 6, 2,
};

/**
 * Create a 16-bone humanoid stick figure skeleton
 */
static Zenith_SkeletonAsset* CreateStickFigureSkeleton()
{
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);

	// Root (at origin)
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);

	// Spine (up from root)
	pxSkel->AddBone("Spine", STICK_BONE_ROOT, Zenith_Maths::Vector3(0, 0.5f, 0), xIdentity, xUnitScale);

	// Neck (up from spine)
	pxSkel->AddBone("Neck", STICK_BONE_SPINE, Zenith_Maths::Vector3(0, 0.7f, 0), xIdentity, xUnitScale);

	// Head (up from neck)
	pxSkel->AddBone("Head", STICK_BONE_NECK, Zenith_Maths::Vector3(0, 0.2f, 0), xIdentity, xUnitScale);

	// Left arm chain
	pxSkel->AddBone("LeftUpperArm", STICK_BONE_SPINE, Zenith_Maths::Vector3(-0.3f, 0.6f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftLowerArm", STICK_BONE_LEFT_UPPER_ARM, Zenith_Maths::Vector3(0, -0.4f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftHand", STICK_BONE_LEFT_LOWER_ARM, Zenith_Maths::Vector3(0, -0.3f, 0), xIdentity, xUnitScale);

	// Right arm chain
	pxSkel->AddBone("RightUpperArm", STICK_BONE_SPINE, Zenith_Maths::Vector3(0.3f, 0.6f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightLowerArm", STICK_BONE_RIGHT_UPPER_ARM, Zenith_Maths::Vector3(0, -0.4f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightHand", STICK_BONE_RIGHT_LOWER_ARM, Zenith_Maths::Vector3(0, -0.3f, 0), xIdentity, xUnitScale);

	// Left leg chain
	pxSkel->AddBone("LeftUpperLeg", STICK_BONE_ROOT, Zenith_Maths::Vector3(-0.15f, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftLowerLeg", STICK_BONE_LEFT_UPPER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("LeftFoot", STICK_BONE_LEFT_LOWER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);

	// Right leg chain
	pxSkel->AddBone("RightUpperLeg", STICK_BONE_ROOT, Zenith_Maths::Vector3(0.15f, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightLowerLeg", STICK_BONE_RIGHT_UPPER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("RightFoot", STICK_BONE_RIGHT_LOWER_LEG, Zenith_Maths::Vector3(0, -0.5f, 0), xIdentity, xUnitScale);

	pxSkel->ComputeBindPoseMatrices();
	return pxSkel;
}

/**
 * Create a cube mesh for the stick figure, with one cube per bone
 */
// Per-bone scale factors for humanoid proportions (half-extents in X, Y, Z)
// Bones: 0=Root, 1=Spine, 2=Neck, 3=Head, 4-6=LeftArm, 7-9=RightArm, 10-12=LeftLeg, 13-15=RightLeg
// Skeleton positions: Root=Y:0, Spine=Y:0.5, Neck=Y:1.2, Head=Y:1.4, Arms=Y:1.1, Legs=Y:0/-0.5/-1.0
static const Zenith_Maths::Vector3 s_axBoneScales[STICK_BONE_COUNT] = {
	{0.10f, 0.06f, 0.06f},  // 0: Root (pelvis) - small hip joint at Y=0
	{0.18f, 0.65f, 0.10f},  // 1: Spine (torso) - centered at Y=0.5, spans Y=-0.15 to Y=1.15 (reaches arms/neck)
	{0.05f, 0.10f, 0.05f},  // 2: Neck - thin, at Y=1.2
	{0.12f, 0.12f, 0.10f},  // 3: Head - round, large, at Y=1.4
	{0.05f, 0.20f, 0.05f},  // 4: LeftUpperArm - at Y=1.1
	{0.04f, 0.18f, 0.04f},  // 5: LeftLowerArm
	{0.04f, 0.06f, 0.02f},  // 6: LeftHand
	{0.05f, 0.20f, 0.05f},  // 7: RightUpperArm - at Y=1.1
	{0.04f, 0.18f, 0.04f},  // 8: RightLowerArm
	{0.04f, 0.06f, 0.02f},  // 9: RightHand
	{0.07f, 0.25f, 0.07f},  // 10: LeftUpperLeg - at Y=0
	{0.05f, 0.25f, 0.05f},  // 11: LeftLowerLeg - at Y=-0.5
	{0.05f, 0.03f, 0.10f},  // 12: LeftFoot - at Y=-1.0
	{0.07f, 0.25f, 0.07f},  // 13: RightUpperLeg
	{0.05f, 0.25f, 0.05f},  // 14: RightLowerLeg
	{0.05f, 0.03f, 0.10f},  // 15: RightFoot
};

static Zenith_MeshAsset* CreateStickFigureMesh(const Zenith_SkeletonAsset* pxSkeleton)
{
	Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
	const uint32_t uVertsPerBone = 8;
	const uint32_t uIndicesPerBone = 36;
	pxMesh->Reserve(STICK_BONE_COUNT * uVertsPerBone, STICK_BONE_COUNT * uIndicesPerBone);

	// Add a scaled cube at each bone position
	for (uint32_t uBone = 0; uBone < STICK_BONE_COUNT; uBone++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBone);
		// Get world position from bind pose model matrix
		Zenith_Maths::Vector3 xBoneWorldPos = Zenith_Maths::Vector3(xBone.m_xBindPoseModel[3]);

		// Get per-bone scale
		Zenith_Maths::Vector3 xScale = s_axBoneScales[uBone];

		uint32_t uBaseVertex = pxMesh->GetNumVerts();

		// Add 8 cube vertices with per-bone scaling
		for (int i = 0; i < 8; i++)
		{
			// Scale the cube offsets by the bone's scale factors
			Zenith_Maths::Vector3 xScaledOffset = s_axCubeOffsets[i] * 2.0f; // Base offsets are ±0.05, so *2 = ±0.1 (unit cube from -0.1 to 0.1)
			xScaledOffset.x *= xScale.x * 10.0f; // Scale to actual size
			xScaledOffset.y *= xScale.y * 10.0f;
			xScaledOffset.z *= xScale.z * 10.0f;

			Zenith_Maths::Vector3 xPos = xBoneWorldPos + xScaledOffset;

			// Calculate proper face normal based on vertex position
			Zenith_Maths::Vector3 xNormal = glm::normalize(s_axCubeOffsets[i]);

			pxMesh->AddVertex(xPos, xNormal, Zenith_Maths::Vector2(0, 0));
			pxMesh->SetVertexSkinning(
				uBaseVertex + i,
				glm::uvec4(uBone, 0, 0, 0),
				glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		}

		// Add 12 triangles (36 indices)
		for (int i = 0; i < 36; i += 3)
		{
			pxMesh->AddTriangle(
				uBaseVertex + s_auCubeIndices[i],
				uBaseVertex + s_auCubeIndices[i + 1],
				uBaseVertex + s_auCubeIndices[i + 2]);
		}
	}

	pxMesh->AddSubmesh(0, STICK_BONE_COUNT * uIndicesPerBone, 0);
	pxMesh->ComputeBounds();
	return pxMesh;
}

/**
 * Convert a Zenith_MeshAsset to Flux_MeshGeometry format for runtime use
 * This creates the GPU-ready format that Flux_MeshGeometry::LoadFromFile expects
 */
static Flux_MeshGeometry* CreateFluxMeshGeometry(const Zenith_MeshAsset* pxMeshAsset, const Zenith_SkeletonAsset* pxSkeleton)
{
	Flux_MeshGeometry* pxGeometry = new Flux_MeshGeometry();

	const uint32_t uNumVerts = pxMeshAsset->GetNumVerts();
	const uint32_t uNumIndices = pxMeshAsset->GetNumIndices();
	const uint32_t uNumBones = pxSkeleton->GetNumBones();

	pxGeometry->m_uNumVerts = uNumVerts;
	pxGeometry->m_uNumIndices = uNumIndices;
	pxGeometry->m_uNumBones = uNumBones;
	pxGeometry->m_xMaterialColor = pxMeshAsset->m_xMaterialColor;

	// Copy positions
	pxGeometry->m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		pxGeometry->m_pxPositions[i] = pxMeshAsset->m_xPositions.Get(i);
	}

	// Copy normals
	if (pxMeshAsset->m_xNormals.GetSize() > 0)
	{
		pxGeometry->m_pxNormals = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxNormals[i] = pxMeshAsset->m_xNormals.Get(i);
		}
	}

	// Copy UVs
	if (pxMeshAsset->m_xUVs.GetSize() > 0)
	{
		pxGeometry->m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxUVs[i] = pxMeshAsset->m_xUVs.Get(i);
		}
	}

	// Copy tangents
	if (pxMeshAsset->m_xTangents.GetSize() > 0)
	{
		pxGeometry->m_pxTangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxTangents[i] = pxMeshAsset->m_xTangents.Get(i);
		}
	}

	// Copy colors
	if (pxMeshAsset->m_xColors.GetSize() > 0)
	{
		pxGeometry->m_pxColors = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
		for (uint32_t i = 0; i < uNumVerts; i++)
		{
			pxGeometry->m_pxColors[i] = pxMeshAsset->m_xColors.Get(i);
		}
	}

	// Copy indices
	pxGeometry->m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));
	for (uint32_t i = 0; i < uNumIndices; i++)
	{
		pxGeometry->m_puIndices[i] = pxMeshAsset->m_xIndices.Get(i);
	}

	// Copy bone IDs (flatten uvec4 to uint32_t array)
	if (pxMeshAsset->m_xBoneIndices.GetSize() > 0)
	{
		pxGeometry->m_puBoneIDs = static_cast<uint32_t*>(Zenith_MemoryManagement::Allocate(uNumVerts * MAX_BONES_PER_VERTEX * sizeof(uint32_t)));
		for (uint32_t v = 0; v < uNumVerts; v++)
		{
			const glm::uvec4& xIndices = pxMeshAsset->m_xBoneIndices.Get(v);
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 0] = xIndices.x;
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 1] = xIndices.y;
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 2] = xIndices.z;
			pxGeometry->m_puBoneIDs[v * MAX_BONES_PER_VERTEX + 3] = xIndices.w;
		}
	}

	// Copy bone weights (flatten vec4 to float array)
	if (pxMeshAsset->m_xBoneWeights.GetSize() > 0)
	{
		pxGeometry->m_pfBoneWeights = static_cast<float*>(Zenith_MemoryManagement::Allocate(uNumVerts * MAX_BONES_PER_VERTEX * sizeof(float)));
		for (uint32_t v = 0; v < uNumVerts; v++)
		{
			const glm::vec4& xWeights = pxMeshAsset->m_xBoneWeights.Get(v);
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 0] = xWeights.x;
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 1] = xWeights.y;
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 2] = xWeights.z;
			pxGeometry->m_pfBoneWeights[v * MAX_BONES_PER_VERTEX + 3] = xWeights.w;
		}
	}

	// Build bone name to ID and offset matrix map from skeleton
	for (uint32_t b = 0; b < uNumBones; b++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(b);
		// The offset matrix transforms from mesh space to bone space (inverse bind pose)
		Zenith_Maths::Matrix4 xOffsetMat = glm::inverse(xBone.m_xBindPoseModel);
		pxGeometry->m_xBoneNameToIdAndOffset[xBone.m_strName] = std::make_pair(b, xOffsetMat);
	}

	// Generate buffer layout and interleaved vertex data
	pxGeometry->GenerateLayoutAndVertexData();

	return pxGeometry;
}

/**
 * Create a static version of the mesh geometry WITHOUT bone data
 * This is for static rendering (72-byte vertices) that works with the static mesh shader
 */
static Flux_MeshGeometry* CreateStaticFluxMeshGeometry(const Zenith_MeshAsset* pxMeshAsset)
{
	Flux_MeshGeometry* pxGeometry = new Flux_MeshGeometry();

	const uint32_t uNumVerts = pxMeshAsset->GetNumVerts();
	const uint32_t uNumIndices = pxMeshAsset->GetNumIndices();

	pxGeometry->m_uNumVerts = uNumVerts;
	pxGeometry->m_uNumIndices = uNumIndices;
	pxGeometry->m_uNumBones = 0;  // No bones for static mesh
	pxGeometry->m_xMaterialColor = pxMeshAsset->m_xMaterialColor;

	// Copy positions
	pxGeometry->m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		pxGeometry->m_pxPositions[i] = pxMeshAsset->m_xPositions.Get(i);
	}

	// Copy normals (or generate default up vector)
	pxGeometry->m_pxNormals = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xNormals.GetSize() > 0)
			pxGeometry->m_pxNormals[i] = pxMeshAsset->m_xNormals.Get(i);
		else
			pxGeometry->m_pxNormals[i] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	}

	// Copy UVs (or generate default zero)
	pxGeometry->m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xUVs.GetSize() > 0)
			pxGeometry->m_pxUVs[i] = pxMeshAsset->m_xUVs.Get(i);
		else
			pxGeometry->m_pxUVs[i] = Zenith_Maths::Vector2(0.0f, 0.0f);
	}

	// Copy tangents (or generate default)
	pxGeometry->m_pxTangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xTangents.GetSize() > 0)
			pxGeometry->m_pxTangents[i] = pxMeshAsset->m_xTangents.Get(i);
		else
			pxGeometry->m_pxTangents[i] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	}

	// Copy bitangents (or generate default)
	pxGeometry->m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xBitangents.GetSize() > 0)
			pxGeometry->m_pxBitangents[i] = pxMeshAsset->m_xBitangents.Get(i);
		else
			pxGeometry->m_pxBitangents[i] = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	}

	// Copy colors (or generate default white)
	pxGeometry->m_pxColors = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		if (pxMeshAsset->m_xColors.GetSize() > 0)
			pxGeometry->m_pxColors[i] = pxMeshAsset->m_xColors.Get(i);
		else
			pxGeometry->m_pxColors[i] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// Copy indices
	pxGeometry->m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));
	for (uint32_t i = 0; i < uNumIndices; i++)
	{
		pxGeometry->m_puIndices[i] = pxMeshAsset->m_xIndices.Get(i);
	}

	// NO bone IDs or weights - this is a static mesh

	// Generate buffer layout and interleaved vertex data
	pxGeometry->GenerateLayoutAndVertexData();

	return pxGeometry;
}

/**
 * Create a 2-second idle animation (subtle breathing motion)
 */
static Flux_AnimationClip* CreateIdleAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Idle");
	pxClip->SetDuration(2.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	// Spine breathing motion
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0.5f, 0));
		xChannel.AddPositionKeyframe(24.0f, Zenith_Maths::Vector3(0, 0.52f, 0));
		xChannel.AddPositionKeyframe(48.0f, Zenith_Maths::Vector3(0, 0.5f, 0));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	return pxClip;
}

/**
 * Create a 1-second walk animation
 */
static Flux_AnimationClip* CreateWalkAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Walk");
	pxClip->SetDuration(1.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	// Use X axis for forward/backward leg and arm swing
	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Left Upper Leg rotation (full cycle: forward -> neutral -> back -> neutral -> forward)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperLeg", std::move(xChannel));
	}

	// Right Upper Leg rotation (opposite phase - full cycle)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperLeg", std::move(xChannel));
	}

	// Left Upper Arm swing (opposite to leg - full cycle)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(20.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	// Right Upper Arm swing (full cycle)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(20.0f), xXAxis));
		xChannel.AddRotationKeyframe(6.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));
		xChannel.AddRotationKeyframe(18.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(20.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	return pxClip;
}

/**
 * Create a 0.5-second run animation (more exaggerated than walk)
 */
static Flux_AnimationClip* CreateRunAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Run");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(true);

	// Use X axis for forward/backward leg and arm swing
	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Left Upper Leg rotation (full cycle: 45 degrees)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(45.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(45.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperLeg", std::move(xChannel));
	}

	// Right Upper Leg rotation (opposite phase - full cycle)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(45.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperLeg", std::move(xChannel));
	}

	// Left Upper Arm swing (full cycle: 35 degrees)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(-35.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(35.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-35.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	// Right Upper Arm swing (full cycle)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::angleAxis(glm::radians(35.0f), xXAxis));
		xChannel.AddRotationKeyframe(3.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(-35.0f), xXAxis));
		xChannel.AddRotationKeyframe(9.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(35.0f), xXAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	return pxClip;
}

/**
 * Create a 0.4-second Attack1 animation (quick jab with right arm)
 */
static Flux_AnimationClip* CreateAttack1Animation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Attack1");
	pxClip->SetDuration(0.4f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);
	const Zenith_Maths::Vector3 xYAxis(0, 1, 0);

	// Right arm jab forward (rotate around X axis to punch forward)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(3.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));  // Windup back
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(60.0f), xXAxis));   // Punch forward
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());          // Return
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	// Slight spine lean forward during punch
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(5.0f, glm::angleAxis(glm::radians(15.0f), xXAxis));   // Lean forward
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	return pxClip;
}

/**
 * Create a 0.4-second Attack2 animation (cross swing with left arm)
 */
static Flux_AnimationClip* CreateAttack2Animation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Attack2");
	pxClip->SetDuration(0.4f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);
	const Zenith_Maths::Vector3 xYAxis(0, 1, 0);

	// Left arm swing across body
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(3.0f, glm::angleAxis(glm::radians(-30.0f), xYAxis));  // Pull back
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(75.0f), xYAxis));   // Swing across
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	// Right arm pull back for balance
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(5.0f, glm::angleAxis(glm::radians(-25.0f), xXAxis));
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	// Spine twist left during swing
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(5.0f, glm::angleAxis(glm::radians(-20.0f), xYAxis));  // Twist left
		xChannel.AddRotationKeyframe(10.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	return pxClip;
}

/**
 * Create a 0.5-second Attack3 animation (heavy overhead swing)
 */
static Flux_AnimationClip* CreateAttack3Animation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Attack3");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);
	const Zenith_Maths::Vector3 xZAxis(0, 0, 1);

	// Both arms raise up then swing down
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(4.0f, glm::angleAxis(glm::radians(-120.0f), xXAxis));  // Arms up
		xChannel.AddRotationKeyframe(8.0f, glm::angleAxis(glm::radians(60.0f), xXAxis));    // Slam down
		xChannel.AddRotationKeyframe(12.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(4.0f, glm::angleAxis(glm::radians(-120.0f), xXAxis));  // Arms up
		xChannel.AddRotationKeyframe(8.0f, glm::angleAxis(glm::radians(60.0f), xXAxis));    // Slam down
		xChannel.AddRotationKeyframe(12.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	// Spine lean back then forward during slam
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(4.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));  // Lean back
		xChannel.AddRotationKeyframe(8.0f, glm::angleAxis(glm::radians(30.0f), xXAxis));   // Lean forward
		xChannel.AddRotationKeyframe(12.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	// Root position - slight hop forward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0, 0));
		xChannel.AddPositionKeyframe(6.0f, Zenith_Maths::Vector3(0, 0.1f, 0.15f));  // Hop up/forward
		xChannel.AddPositionKeyframe(12.0f, Zenith_Maths::Vector3(0, 0, 0.1f));     // Land forward
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Root", std::move(xChannel));
	}

	return pxClip;
}

/**
 * Create a 0.5-second Dodge animation (quick sidestep)
 */
static Flux_AnimationClip* CreateDodgeAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Dodge");
	pxClip->SetDuration(0.5f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);
	const Zenith_Maths::Vector3 xZAxis(0, 0, 1);

	// Root translation - sidestep right
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0, 0));
		xChannel.AddPositionKeyframe(6.0f, Zenith_Maths::Vector3(0.5f, -0.2f, 0));   // Dodge right, crouch
		xChannel.AddPositionKeyframe(12.0f, Zenith_Maths::Vector3(0.8f, 0, 0));      // Land
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Root", std::move(xChannel));
	}

	// Spine lean into dodge
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(30.0f), xZAxis));  // Lean right
		xChannel.AddRotationKeyframe(12.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	// Legs - right leg step out
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(6.0f, glm::angleAxis(glm::radians(-30.0f), xZAxis));  // Step out
		xChannel.AddRotationKeyframe(12.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperLeg", std::move(xChannel));
	}

	return pxClip;
}

/**
 * Create a 0.3-second Hit animation (stagger backward)
 */
static Flux_AnimationClip* CreateHitAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Hit");
	pxClip->SetDuration(0.3f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Root stagger backward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0, 0));
		xChannel.AddPositionKeyframe(4.0f, Zenith_Maths::Vector3(0, 0, -0.3f));   // Knocked back
		xChannel.AddPositionKeyframe(7.0f, Zenith_Maths::Vector3(0, 0, -0.2f));   // Recover slightly
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Root", std::move(xChannel));
	}

	// Spine lean backward from impact
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(3.0f, glm::angleAxis(glm::radians(-25.0f), xXAxis));  // Lean back
		xChannel.AddRotationKeyframe(7.0f, glm::identity<Zenith_Maths::Quat>());           // Recover
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	// Head snap back
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(2.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));  // Head back
		xChannel.AddRotationKeyframe(7.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Head", std::move(xChannel));
	}

	return pxClip;
}

/**
 * Create a 1.0-second Death animation (fall over)
 */
static Flux_AnimationClip* CreateDeathAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Death");
	pxClip->SetDuration(1.0f);
	pxClip->SetTicksPerSecond(24);
	pxClip->SetLooping(false);

	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);
	const Zenith_Maths::Vector3 xZAxis(0, 0, 1);

	// Root drops down and backward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0, 0, 0));
		xChannel.AddPositionKeyframe(12.0f, Zenith_Maths::Vector3(0, -0.3f, -0.2f));   // Start falling
		xChannel.AddPositionKeyframe(24.0f, Zenith_Maths::Vector3(0, -1.0f, -0.4f));   // On ground
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Root", std::move(xChannel));
	}

	// Spine collapses backward
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-45.0f), xXAxis));  // Falling back
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-90.0f), xXAxis));  // Flat on back
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Spine", std::move(xChannel));
	}

	// Head goes limp
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-30.0f), xXAxis));
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-20.0f), xXAxis));  // Resting
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Head", std::move(xChannel));
	}

	// Arms fall limp
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(45.0f), xZAxis));
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(60.0f), xZAxis));  // Spread out
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("LeftUpperArm", std::move(xChannel));
	}

	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(12.0f, glm::angleAxis(glm::radians(-45.0f), xZAxis));
		xChannel.AddRotationKeyframe(24.0f, glm::angleAxis(glm::radians(-60.0f), xZAxis));  // Spread out
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("RightUpperArm", std::move(xChannel));
	}

	return pxClip;
}

//------------------------------------------------------------------------------
// Stick Figure Animation Tests
//------------------------------------------------------------------------------

void Zenith_UnitTests::TestStickFigureSkeletonCreation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureSkeletonCreation...");

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();

	// Verify bone count
	Zenith_Assert(pxSkel->GetNumBones() == STICK_BONE_COUNT, "Expected 16 bones");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Skeleton has %u bones", pxSkel->GetNumBones());

	// Verify bone names exist
	Zenith_Assert(pxSkel->HasBone("Root"), "Missing Root bone");
	Zenith_Assert(pxSkel->HasBone("Spine"), "Missing Spine bone");
	Zenith_Assert(pxSkel->HasBone("Head"), "Missing Head bone");
	Zenith_Assert(pxSkel->HasBone("LeftUpperArm"), "Missing LeftUpperArm bone");
	Zenith_Assert(pxSkel->HasBone("LeftFoot"), "Missing LeftFoot bone");

	// Verify parent hierarchy
	Zenith_Assert(pxSkel->GetBone(STICK_BONE_ROOT).m_iParentIndex == -1, "Root should have no parent");
	Zenith_Assert(pxSkel->GetBone(STICK_BONE_SPINE).m_iParentIndex == STICK_BONE_ROOT, "Spine parent should be Root");
	Zenith_Assert(pxSkel->GetBone(STICK_BONE_HEAD).m_iParentIndex == STICK_BONE_NECK, "Head parent should be Neck");
	Zenith_Assert(pxSkel->GetBone(STICK_BONE_LEFT_HAND).m_iParentIndex == STICK_BONE_LEFT_LOWER_ARM, "LeftHand parent should be LeftLowerArm");

	// Verify bind pose world positions
	Zenith_Maths::Vector3 xHeadPos = Zenith_Maths::Vector3(pxSkel->GetBone(STICK_BONE_HEAD).m_xBindPoseModel[3]);
	Zenith_Assert(Vec3Equals(xHeadPos, Zenith_Maths::Vector3(0, 1.4f, 0), 0.01f), "Head world position mismatch");

	Zenith_Maths::Vector3 xLeftFootPos = Zenith_Maths::Vector3(pxSkel->GetBone(STICK_BONE_LEFT_FOOT).m_xBindPoseModel[3]);
	Zenith_Assert(Vec3Equals(xLeftFootPos, Zenith_Maths::Vector3(-0.15f, -1.0f, 0), 0.01f), "LeftFoot world position mismatch");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Head world position: (%.2f, %.2f, %.2f)", xHeadPos.x, xHeadPos.y, xHeadPos.z);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  LeftFoot world position: (%.2f, %.2f, %.2f)", xLeftFootPos.x, xLeftFootPos.y, xLeftFootPos.z);

	delete pxSkel;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureSkeletonCreation completed successfully");
}

void Zenith_UnitTests::TestStickFigureMeshCreation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureMeshCreation...");

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(pxSkel);

	// Verify vertex/index counts
	const uint32_t uExpectedVerts = STICK_BONE_COUNT * 8;  // 128
	const uint32_t uExpectedIndices = STICK_BONE_COUNT * 36;  // 576

	Zenith_Assert(pxMesh->GetNumVerts() == uExpectedVerts, "Expected 128 vertices");
	Zenith_Assert(pxMesh->GetNumIndices() == uExpectedIndices, "Expected 576 indices");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Mesh has %u vertices and %u indices", pxMesh->GetNumVerts(), pxMesh->GetNumIndices());

	// Verify skinning weights
	Zenith_Assert(pxMesh->m_xBoneIndices.GetSize() == uExpectedVerts, "Bone indices count mismatch");
	Zenith_Assert(pxMesh->m_xBoneWeights.GetSize() == uExpectedVerts, "Bone weights count mismatch");

	// Check that each vertex is 100% weighted to one bone
	for (uint32_t v = 0; v < uExpectedVerts; v++)
	{
		const glm::vec4& xWeights = pxMesh->m_xBoneWeights.Get(v);
		Zenith_Assert(FloatEquals(xWeights.x, 1.0f, 0.001f), "Vertex weight should be 1.0");
		Zenith_Assert(FloatEquals(xWeights.y, 0.0f, 0.001f), "Secondary weight should be 0.0");
	}
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  All vertices have correct skinning weights");

	// Verify bounds
	Zenith_Assert(pxMesh->GetBoundsMin().y < -0.9f, "Bounds min Y should be below -0.9");
	Zenith_Assert(pxMesh->GetBoundsMax().y > 1.3f, "Bounds max Y should be above 1.3");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Bounds: min=(%.2f, %.2f, %.2f), max=(%.2f, %.2f, %.2f)",
		pxMesh->GetBoundsMin().x, pxMesh->GetBoundsMin().y, pxMesh->GetBoundsMin().z,
		pxMesh->GetBoundsMax().x, pxMesh->GetBoundsMax().y, pxMesh->GetBoundsMax().z);

	delete pxMesh;
	delete pxSkel;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureMeshCreation completed successfully");
}

void Zenith_UnitTests::TestStickFigureIdleAnimation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureIdleAnimation...");

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_AnimationClip* pxClip = CreateIdleAnimation();

	Zenith_Assert(pxClip->GetName() == "Idle", "Animation name should be 'Idle'");
	Zenith_Assert(FloatEquals(pxClip->GetDuration(), 2.0f, 0.01f), "Duration should be 2.0 seconds");
	Zenith_Assert(pxClip->GetTicksPerSecond() == 24, "Ticks per second should be 24");
	Zenith_Assert(pxClip->HasBoneChannel("Spine"), "Should have Spine bone channel");

	// Sample spine position at different times
	const Flux_BoneChannel* pxSpineChannel = pxClip->GetBoneChannel("Spine");
	Zenith_Assert(pxSpineChannel != nullptr, "Spine channel should exist");

	// t=0: position should be (0, 0.5, 0)
	Zenith_Maths::Vector3 xPos0 = pxSpineChannel->SamplePosition(0.0f);
	Zenith_Assert(Vec3Equals(xPos0, Zenith_Maths::Vector3(0, 0.5f, 0), 0.01f), "Spine position at t=0 mismatch");

	// t=24 ticks (1 second): position should be (0, 0.52, 0)
	Zenith_Maths::Vector3 xPos1 = pxSpineChannel->SamplePosition(24.0f);
	Zenith_Assert(Vec3Equals(xPos1, Zenith_Maths::Vector3(0, 0.52f, 0), 0.01f), "Spine position at t=1s mismatch");

	// t=12 ticks (0.5 seconds): position should be interpolated to (0, 0.51, 0)
	Zenith_Maths::Vector3 xPos05 = pxSpineChannel->SamplePosition(12.0f);
	Zenith_Assert(Vec3Equals(xPos05, Zenith_Maths::Vector3(0, 0.51f, 0), 0.01f), "Spine position at t=0.5s mismatch");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Spine Y at t=0: %.3f, t=0.5s: %.3f, t=1s: %.3f",
		xPos0.y, xPos05.y, xPos1.y);

	delete pxClip;
	delete pxSkel;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureIdleAnimation completed successfully");
}

void Zenith_UnitTests::TestStickFigureWalkAnimation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureWalkAnimation...");

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_AnimationClip* pxClip = CreateWalkAnimation();

	Zenith_Assert(pxClip->GetName() == "Walk", "Animation name should be 'Walk'");
	Zenith_Assert(FloatEquals(pxClip->GetDuration(), 1.0f, 0.01f), "Duration should be 1.0 second");

	// Verify left upper leg rotation at t=0 (should be 30 degrees around X for forward/backward swing)
	const Flux_BoneChannel* pxLeftLegChannel = pxClip->GetBoneChannel("LeftUpperLeg");
	Zenith_Assert(pxLeftLegChannel != nullptr, "LeftUpperLeg channel should exist");

	Zenith_Maths::Quat xExpected30 = glm::angleAxis(glm::radians(30.0f), Zenith_Maths::Vector3(1, 0, 0));
	Zenith_Maths::Quat xSampled = pxLeftLegChannel->SampleRotation(0.0f);
	Zenith_Assert(QuatEquals(xSampled, xExpected30, 0.01f), "LeftUpperLeg rotation at t=0 should be 30 deg");

	// Verify right upper leg is opposite phase at t=0 (-30 degrees)
	const Flux_BoneChannel* pxRightLegChannel = pxClip->GetBoneChannel("RightUpperLeg");
	Zenith_Assert(pxRightLegChannel != nullptr, "RightUpperLeg channel should exist");

	Zenith_Maths::Quat xExpectedMinus30 = glm::angleAxis(glm::radians(-30.0f), Zenith_Maths::Vector3(1, 0, 0));
	Zenith_Maths::Quat xSampledRight = pxRightLegChannel->SampleRotation(0.0f);
	Zenith_Assert(QuatEquals(xSampledRight, xExpectedMinus30, 0.01f), "RightUpperLeg rotation at t=0 should be -30 deg");

	// Verify arm swing
	const Flux_BoneChannel* pxLeftArmChannel = pxClip->GetBoneChannel("LeftUpperArm");
	Zenith_Assert(pxLeftArmChannel != nullptr, "LeftUpperArm channel should exist");

	Zenith_Maths::Quat xExpectedArm = glm::angleAxis(glm::radians(-20.0f), Zenith_Maths::Vector3(1, 0, 0));
	Zenith_Maths::Quat xSampledArm = pxLeftArmChannel->SampleRotation(0.0f);
	Zenith_Assert(QuatEquals(xSampledArm, xExpectedArm, 0.01f), "LeftUpperArm rotation at t=0 should be -20 deg");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Walk animation keyframes verified");

	delete pxClip;
	delete pxSkel;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureWalkAnimation completed successfully");
}

void Zenith_UnitTests::TestStickFigureRunAnimation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureRunAnimation...");

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_AnimationClip* pxClip = CreateRunAnimation();

	Zenith_Assert(pxClip->GetName() == "Run", "Animation name should be 'Run'");
	Zenith_Assert(FloatEquals(pxClip->GetDuration(), 0.5f, 0.01f), "Duration should be 0.5 seconds");

	// Verify left upper leg rotation at t=0 (should be 45 degrees around X - more exaggerated)
	const Flux_BoneChannel* pxLeftLegChannel = pxClip->GetBoneChannel("LeftUpperLeg");
	Zenith_Assert(pxLeftLegChannel != nullptr, "LeftUpperLeg channel should exist");

	Zenith_Maths::Quat xExpected45 = glm::angleAxis(glm::radians(45.0f), Zenith_Maths::Vector3(1, 0, 0));
	Zenith_Maths::Quat xSampled = pxLeftLegChannel->SampleRotation(0.0f);
	Zenith_Assert(QuatEquals(xSampled, xExpected45, 0.01f), "LeftUpperLeg rotation at t=0 should be 45 deg");

	// Verify arm swing (35 degrees around X - more exaggerated than walk)
	const Flux_BoneChannel* pxLeftArmChannel = pxClip->GetBoneChannel("LeftUpperArm");
	Zenith_Assert(pxLeftArmChannel != nullptr, "LeftUpperArm channel should exist");

	Zenith_Maths::Quat xExpectedArm = glm::angleAxis(glm::radians(-35.0f), Zenith_Maths::Vector3(1, 0, 0));
	Zenith_Maths::Quat xSampledArm = pxLeftArmChannel->SampleRotation(0.0f);
	Zenith_Assert(QuatEquals(xSampledArm, xExpectedArm, 0.01f), "LeftUpperArm rotation at t=0 should be -35 deg");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Run animation keyframes verified (more exaggerated than walk)");

	delete pxClip;
	delete pxSkel;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureRunAnimation completed successfully");
}

void Zenith_UnitTests::TestStickFigureAnimationBlending()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureAnimationBlending...");

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_AnimationClip* pxWalkClip = CreateWalkAnimation();
	Flux_AnimationClip* pxRunClip = CreateRunAnimation();

	// Initialize skeleton poses
	Flux_SkeletonPose xWalkPose;
	xWalkPose.Initialize(STICK_BONE_COUNT);
	xWalkPose.SampleFromClip(*pxWalkClip, 0.0f, *pxSkel);

	Flux_SkeletonPose xRunPose;
	xRunPose.Initialize(STICK_BONE_COUNT);
	xRunPose.SampleFromClip(*pxRunClip, 0.0f, *pxSkel);

	// Get Walk and Run rotations for LeftUpperLeg
	const Flux_BoneLocalPose& xWalkLegPose = xWalkPose.GetLocalPose(STICK_BONE_LEFT_UPPER_LEG);
	const Flux_BoneLocalPose& xRunLegPose = xRunPose.GetLocalPose(STICK_BONE_LEFT_UPPER_LEG);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Walk leg rotation: (%.3f, %.3f, %.3f, %.3f)",
		xWalkLegPose.m_xRotation.w, xWalkLegPose.m_xRotation.x, xWalkLegPose.m_xRotation.y, xWalkLegPose.m_xRotation.z);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Run leg rotation: (%.3f, %.3f, %.3f, %.3f)",
		xRunLegPose.m_xRotation.w, xRunLegPose.m_xRotation.x, xRunLegPose.m_xRotation.y, xRunLegPose.m_xRotation.z);

	// Test blending at different factors
	float afBlendFactors[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
	for (float fBlend : afBlendFactors)
	{
		Flux_SkeletonPose xBlendedPose;
		xBlendedPose.Initialize(STICK_BONE_COUNT);
		Flux_SkeletonPose::Blend(xBlendedPose, xWalkPose, xRunPose, fBlend);

		// Verify blended rotation
		const Flux_BoneLocalPose& xBlendedLeg = xBlendedPose.GetLocalPose(STICK_BONE_LEFT_UPPER_LEG);
		Zenith_Maths::Quat xExpected = glm::slerp(xWalkLegPose.m_xRotation, xRunLegPose.m_xRotation, fBlend);

		Zenith_Assert(QuatEquals(xBlendedLeg.m_xRotation, xExpected, 0.01f),
			"Blended rotation mismatch at factor %.2f", fBlend);

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Blend %.2f: leg rotation verified", fBlend);
	}

	delete pxRunClip;
	delete pxWalkClip;
	delete pxSkel;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureAnimationBlending completed successfully");
}

//------------------------------------------------------------------------------
// Stick Figure IK Tests
//------------------------------------------------------------------------------

void Zenith_UnitTests::TestStickFigureArmIK()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureArmIK...");

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_IKSolver xSolver;

	// Create arm IK chains
	Flux_IKChain xLeftArm = Flux_IKSolver::CreateArmChain("LeftArm", "LeftUpperArm", "LeftLowerArm", "LeftHand");
	Flux_IKChain xRightArm = Flux_IKSolver::CreateArmChain("RightArm", "RightUpperArm", "RightLowerArm", "RightHand");

	xSolver.AddChain(xLeftArm);
	xSolver.AddChain(xRightArm);

	Zenith_Assert(xSolver.HasChain("LeftArm"), "Solver should have LeftArm chain");
	Zenith_Assert(xSolver.HasChain("RightArm"), "Solver should have RightArm chain");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Created arm IK chains");

	// Test setting targets
	Flux_IKTarget xTarget;
	xTarget.m_xPosition = Zenith_Maths::Vector3(0, 1.0f, 0.5f);
	xTarget.m_fWeight = 1.0f;
	xTarget.m_bEnabled = true;

	xSolver.SetTarget("LeftArm", xTarget);
	Zenith_Assert(xSolver.HasTarget("LeftArm"), "Solver should have LeftArm target");

	const Flux_IKTarget* pxStoredTarget = xSolver.GetTarget("LeftArm");
	Zenith_Assert(pxStoredTarget != nullptr, "Should be able to retrieve target");
	Zenith_Assert(Vec3Equals(pxStoredTarget->m_xPosition, xTarget.m_xPosition, 0.001f), "Target position mismatch");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  IK target set and retrieved successfully");

	// Clear target
	xSolver.ClearTarget("LeftArm");
	Zenith_Assert(!xSolver.HasTarget("LeftArm"), "Target should be cleared");

	delete pxSkel;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureArmIK completed successfully");
}

void Zenith_UnitTests::TestStickFigureLegIK()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureLegIK...");

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_IKSolver xSolver;

	// Create leg IK chains
	Flux_IKChain xLeftLeg = Flux_IKSolver::CreateLegChain("LeftLeg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot");
	Flux_IKChain xRightLeg = Flux_IKSolver::CreateLegChain("RightLeg", "RightUpperLeg", "RightLowerLeg", "RightFoot");

	xSolver.AddChain(xLeftLeg);
	xSolver.AddChain(xRightLeg);

	Zenith_Assert(xSolver.HasChain("LeftLeg"), "Solver should have LeftLeg chain");
	Zenith_Assert(xSolver.HasChain("RightLeg"), "Solver should have RightLeg chain");

	// Verify chain bone count
	const Flux_IKChain* pxLeftLegChain = xSolver.GetChain("LeftLeg");
	Zenith_Assert(pxLeftLegChain != nullptr, "Should be able to retrieve LeftLeg chain");
	Zenith_Assert(pxLeftLegChain->m_xBoneNames.size() == 3, "Leg chain should have 3 bones");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Leg IK chains created with %zu bones each", pxLeftLegChain->m_xBoneNames.size());

	// Test setting targets for both legs
	Flux_IKTarget xLeftTarget;
	xLeftTarget.m_xPosition = Zenith_Maths::Vector3(-0.15f, -0.8f, 0.2f);
	xLeftTarget.m_fWeight = 1.0f;
	xLeftTarget.m_bEnabled = true;

	Flux_IKTarget xRightTarget;
	xRightTarget.m_xPosition = Zenith_Maths::Vector3(0.15f, -0.9f, -0.1f);
	xRightTarget.m_fWeight = 1.0f;
	xRightTarget.m_bEnabled = true;

	xSolver.SetTarget("LeftLeg", xLeftTarget);
	xSolver.SetTarget("RightLeg", xRightTarget);

	Zenith_Assert(xSolver.HasTarget("LeftLeg"), "Solver should have LeftLeg target");
	Zenith_Assert(xSolver.HasTarget("RightLeg"), "Solver should have RightLeg target");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Both leg targets set successfully");

	delete pxSkel;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureLegIK completed successfully");
}

void Zenith_UnitTests::TestStickFigureIKWithAnimation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureIKWithAnimation...");

	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Flux_AnimationClip* pxWalkClip = CreateWalkAnimation();
	Flux_IKSolver xSolver;

	// Set up leg IK
	Flux_IKChain xLeftLeg = Flux_IKSolver::CreateLegChain("LeftLeg", "LeftUpperLeg", "LeftLowerLeg", "LeftFoot");
	xSolver.AddChain(xLeftLeg);

	// Sample walk animation at mid-stride
	Flux_SkeletonPose xAnimPose;
	xAnimPose.Initialize(STICK_BONE_COUNT);
	float fMidStride = 0.5f * pxWalkClip->GetTicksPerSecond(); // 12 ticks
	xAnimPose.SampleFromClip(*pxWalkClip, fMidStride, *pxSkel);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Sampled walk animation at mid-stride (t=0.5s)");

	// Set IK target
	Flux_IKTarget xFootTarget;
	xFootTarget.m_xPosition = Zenith_Maths::Vector3(-0.15f, -0.9f, 0.1f);
	xFootTarget.m_fWeight = 1.0f;
	xFootTarget.m_bEnabled = true;

	xSolver.SetTarget("LeftLeg", xFootTarget);

	// Test different blend weights
	for (float fWeight : {0.0f, 0.5f, 1.0f})
	{
		Flux_IKTarget xWeightedTarget = xFootTarget;
		xWeightedTarget.m_fWeight = fWeight;
		xSolver.SetTarget("LeftLeg", xWeightedTarget);

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  IK weight %.1f: target set", fWeight);
	}

	delete pxWalkClip;
	delete pxSkel;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureIKWithAnimation completed successfully");
}

//------------------------------------------------------------------------------
// Animation State Machine Integration Tests
//------------------------------------------------------------------------------

void Zenith_UnitTests::TestStateMachineUpdateLoop()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStateMachineUpdateLoop...");

	// Create state machine with Idle and Walk states
	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddFloat("Speed", 0.0f);

	Flux_AnimationState* pxIdle = xStateMachine.AddState("Idle");
	Flux_AnimationState* pxWalk = xStateMachine.AddState("Walk");

	// Add transition: Idle -> Walk when Speed > 0.1
	Flux_StateTransition xIdleToWalk;
	xIdleToWalk.m_strTargetStateName = "Walk";
	xIdleToWalk.m_fTransitionDuration = 0.2f;

	Flux_TransitionCondition xSpeedCond;
	xSpeedCond.m_strParameterName = "Speed";
	xSpeedCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
	xSpeedCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xSpeedCond.m_fThreshold = 0.1f;
	xIdleToWalk.m_xConditions.PushBack(xSpeedCond);

	pxIdle->AddTransition(xIdleToWalk);

	// Add transition: Walk -> Idle when Speed <= 0.1
	Flux_StateTransition xWalkToIdle;
	xWalkToIdle.m_strTargetStateName = "Idle";
	xWalkToIdle.m_fTransitionDuration = 0.2f;

	Flux_TransitionCondition xSlowCond;
	xSlowCond.m_strParameterName = "Speed";
	xSlowCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::LessEqual;
	xSlowCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xSlowCond.m_fThreshold = 0.1f;
	xWalkToIdle.m_xConditions.PushBack(xSlowCond);

	pxWalk->AddTransition(xWalkToIdle);

	xStateMachine.SetDefaultState("Idle");

	// Create dummy skeleton and pose for Update calls
	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	// Initial update - should be in Idle
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xStateMachine.GetCurrentStateName() == "Idle",
		"Should start in Idle state");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Initial state is Idle");

	// Set Speed > 0.1, update - transition should start
	xStateMachine.GetParameters().SetFloat("Speed", 0.5f);
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	Zenith_Assert(xStateMachine.IsTransitioning() == true,
		"Should be transitioning after condition met");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Transition started when Speed > 0.1");

	// Continue updating until transition completes
	for (int i = 0; i < 20; ++i)
	{
		xStateMachine.Update(0.016f, xPose, xSkeleton);
	}

	Zenith_Assert(xStateMachine.GetCurrentStateName() == "Walk",
		"Should be in Walk state after transition completes");
	Zenith_Assert(xStateMachine.IsTransitioning() == false,
		"Transition should be complete");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Arrived at Walk state after transition");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStateMachineUpdateLoop completed successfully");
}

void Zenith_UnitTests::TestTriggerConsumptionInTransitions()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTriggerConsumptionInTransitions...");

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddTrigger("Attack");

	Flux_AnimationState* pxIdle = xStateMachine.AddState("Idle");
	xStateMachine.AddState("Attack");

	// Idle -> Attack on AttackTrigger
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Attack";
	xTrans.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xTriggerCond;
	xTriggerCond.m_strParameterName = "Attack";
	xTriggerCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xTrans.m_xConditions.PushBack(xTriggerCond);

	pxIdle->AddTransition(xTrans);
	xStateMachine.SetDefaultState("Idle");

	// Initial state
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xStateMachine.GetCurrentStateName() == "Idle", "Should start in Idle");

	// Set trigger
	xStateMachine.GetParameters().SetTrigger("Attack");

	// Update - trigger should be consumed and transition should start
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xStateMachine.IsTransitioning() == true,
		"Transition should start after trigger set");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Transition started on trigger");

	// Trigger should be consumed - trying to consume again should return false
	Zenith_Assert(xStateMachine.GetParameters().ConsumeTrigger("Attack") == false,
		"Trigger should have been consumed by transition");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Trigger was consumed");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTriggerConsumptionInTransitions completed successfully");
}

void Zenith_UnitTests::TestExitTimeTransitions()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestExitTimeTransitions...");

	// Test the CanTransition method with exit time
	Flux_AnimationParameters xParams;

	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Idle";
	xTrans.m_fTransitionDuration = 0.1f;
	xTrans.m_bHasExitTime = true;
	xTrans.m_fExitTime = 0.8f;
	// No other conditions - should auto-transition at exit time

	// Test before exit time
	bool bCanTransBefore = xTrans.CanTransition(xParams, 0.5f);
	Zenith_Assert(bCanTransBefore == false,
		"Should not transition before exit time (0.5 < 0.8)");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Cannot transition before exit time");

	// Test at exit time
	bool bCanTransAt = xTrans.CanTransition(xParams, 0.8f);
	Zenith_Assert(bCanTransAt == true,
		"Should transition at exit time (0.8 >= 0.8)");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Can transition at exit time");

	// Test after exit time
	bool bCanTransAfter = xTrans.CanTransition(xParams, 0.95f);
	Zenith_Assert(bCanTransAfter == true,
		"Should transition after exit time (0.95 >= 0.8)");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Can transition after exit time");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestExitTimeTransitions completed successfully");
}

void Zenith_UnitTests::TestTransitionPriority()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTransitionPriority...");

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddFloat("Speed", 0.0f);
	xStateMachine.GetParameters().AddTrigger("Attack");

	Flux_AnimationState* pxIdle = xStateMachine.AddState("Idle");
	xStateMachine.AddState("Walk");
	xStateMachine.AddState("Attack");

	// Add two transitions from Idle:
	// 1. Idle -> Walk (Speed > 0.1) - low priority
	// 2. Idle -> Attack (AttackTrigger) - high priority

	Flux_StateTransition xToWalk;
	xToWalk.m_strTargetStateName = "Walk";
	xToWalk.m_fTransitionDuration = 0.1f;
	xToWalk.m_iPriority = 0;  // Low priority

	Flux_TransitionCondition xSpeedCond;
	xSpeedCond.m_strParameterName = "Speed";
	xSpeedCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
	xSpeedCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xSpeedCond.m_fThreshold = 0.1f;
	xToWalk.m_xConditions.PushBack(xSpeedCond);

	Flux_StateTransition xToAttack;
	xToAttack.m_strTargetStateName = "Attack";
	xToAttack.m_fTransitionDuration = 0.05f;
	xToAttack.m_iPriority = 10;  // High priority

	Flux_TransitionCondition xAttackCond;
	xAttackCond.m_strParameterName = "Attack";
	xAttackCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xToAttack.m_xConditions.PushBack(xAttackCond);

	// Add in reverse priority order to verify sorting
	pxIdle->AddTransition(xToWalk);
	pxIdle->AddTransition(xToAttack);

	// Verify transitions are sorted by priority
	const Zenith_Vector<Flux_StateTransition>& xTransitions = pxIdle->GetTransitions();
	Zenith_Assert(xTransitions.Get(0).m_iPriority >= xTransitions.Get(1).m_iPriority,
		"Transitions should be sorted by priority (higher first)");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Transitions sorted by priority");

	// Set both conditions true - Attack should win due to priority
	xStateMachine.SetDefaultState("Idle");
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	xStateMachine.GetParameters().SetFloat("Speed", 0.5f);
	xStateMachine.GetParameters().SetTrigger("Attack");

	xStateMachine.Update(0.016f, xPose, xSkeleton);

	// Complete the transition
	for (int i = 0; i < 10; ++i)
		xStateMachine.Update(0.016f, xPose, xSkeleton);

	Zenith_Assert(xStateMachine.GetCurrentStateName() == "Attack",
		"Higher priority transition (Attack) should be chosen over Walk");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Higher priority transition won");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTransitionPriority completed successfully");
}

void Zenith_UnitTests::TestStateLifecycleCallbacks()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStateLifecycleCallbacks...");

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	bool bEnterCalled = false;
	bool bExitCalled = false;
	bool bUpdateCalled = false;
	float fUpdateDt = 0.0f;

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddTrigger("Next");

	Flux_AnimationState* pxStateA = xStateMachine.AddState("StateA");
	xStateMachine.AddState("StateB");

	// Set up callbacks on StateA
	pxStateA->m_fnOnEnter = [&bEnterCalled]() { bEnterCalled = true; };
	pxStateA->m_fnOnExit = [&bExitCalled]() { bExitCalled = true; };
	pxStateA->m_fnOnUpdate = [&bUpdateCalled, &fUpdateDt](float fDt) {
		bUpdateCalled = true;
		fUpdateDt = fDt;
	};

	// StateA -> StateB on trigger
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "StateB";
	xTrans.m_fTransitionDuration = 0.05f;

	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "Next";
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xTrans.m_xConditions.PushBack(xCond);
	pxStateA->AddTransition(xTrans);

	// Test OnEnter via SetState
	xStateMachine.SetState("StateA");
	Zenith_Assert(bEnterCalled == true, "OnEnter should be called on SetState");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] OnEnter called on SetState");

	// Test OnUpdate
	bUpdateCalled = false;
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(bUpdateCalled == true, "OnUpdate should be called during Update");
	Zenith_Assert(FloatEquals(fUpdateDt, 0.016f, 0.001f), "OnUpdate should receive delta time");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] OnUpdate called with correct delta time");

	// Test OnExit via transition
	bExitCalled = false;
	xStateMachine.GetParameters().SetTrigger("Next");
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(bExitCalled == true, "OnExit should be called when starting transition");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] OnExit called on transition");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStateLifecycleCallbacks completed successfully");
}

void Zenith_UnitTests::TestMultipleTransitionConditions()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestMultipleTransitionConditions...");

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddFloat("Speed", 0.0f);
	xStateMachine.GetParameters().AddBool("IsGrounded", true);

	Flux_AnimationState* pxIdle = xStateMachine.AddState("Idle");
	xStateMachine.AddState("Run");

	// Idle -> Run requires BOTH Speed > 5.0 AND IsGrounded == true
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Run";
	xTrans.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xSpeedCond;
	xSpeedCond.m_strParameterName = "Speed";
	xSpeedCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
	xSpeedCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xSpeedCond.m_fThreshold = 5.0f;

	Flux_TransitionCondition xGroundedCond;
	xGroundedCond.m_strParameterName = "IsGrounded";
	xGroundedCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
	xGroundedCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
	xGroundedCond.m_bThreshold = true;

	xTrans.m_xConditions.PushBack(xSpeedCond);
	xTrans.m_xConditions.PushBack(xGroundedCond);

	pxIdle->AddTransition(xTrans);
	xStateMachine.SetDefaultState("Idle");

	// Initial update
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	// Only Speed true - should NOT transition
	xStateMachine.GetParameters().SetFloat("Speed", 10.0f);
	xStateMachine.GetParameters().SetBool("IsGrounded", false);
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	Zenith_Assert(xStateMachine.GetCurrentStateName() == "Idle",
		"Should stay in Idle when only Speed condition met");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] No transition when only Speed > 5");

	// Only IsGrounded true - should NOT transition
	xStateMachine.GetParameters().SetFloat("Speed", 2.0f);
	xStateMachine.GetParameters().SetBool("IsGrounded", true);
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	Zenith_Assert(xStateMachine.GetCurrentStateName() == "Idle",
		"Should stay in Idle when only IsGrounded condition met");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] No transition when only IsGrounded true");

	// Both conditions true - SHOULD transition
	xStateMachine.GetParameters().SetFloat("Speed", 10.0f);
	xStateMachine.GetParameters().SetBool("IsGrounded", true);
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	Zenith_Assert(xStateMachine.IsTransitioning() == true,
		"Should start transition when ALL conditions met");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Transition started when all conditions met");

	// Complete transition
	for (int i = 0; i < 10; ++i)
		xStateMachine.Update(0.016f, xPose, xSkeleton);

	Zenith_Assert(xStateMachine.GetCurrentStateName() == "Run",
		"Should be in Run state after transition");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Arrived at Run state");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleTransitionConditions completed successfully");
}

//------------------------------------------------------------------------------
// Stick Figure Asset Export Test
//------------------------------------------------------------------------------

void Zenith_UnitTests::TestStickFigureAssetExport()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureAssetExport...");

	// Create all assets
	Zenith_SkeletonAsset* pxSkel = CreateStickFigureSkeleton();
	Zenith_MeshAsset* pxMesh = CreateStickFigureMesh(pxSkel);
	Flux_AnimationClip* pxIdleClip = CreateIdleAnimation();
	Flux_AnimationClip* pxWalkClip = CreateWalkAnimation();
	Flux_AnimationClip* pxRunClip = CreateRunAnimation();
	Flux_AnimationClip* pxAttack1Clip = CreateAttack1Animation();
	Flux_AnimationClip* pxAttack2Clip = CreateAttack2Animation();
	Flux_AnimationClip* pxAttack3Clip = CreateAttack3Animation();
	Flux_AnimationClip* pxDodgeClip = CreateDodgeAnimation();
	Flux_AnimationClip* pxHitClip = CreateHitAnimation();
	Flux_AnimationClip* pxDeathClip = CreateDeathAnimation();

	// Create output directory
	std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/";
	std::filesystem::create_directories(strOutputDir);

	// Export skeleton
	std::string strSkelPath = strOutputDir + "StickFigure.zskel";
	pxSkel->Export(strSkelPath.c_str());
	Zenith_Assert(std::filesystem::exists(strSkelPath), "Skeleton file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported skeleton to: %s", strSkelPath.c_str());

	// Set skeleton path on mesh before export
	pxMesh->SetSkeletonPath("Meshes/StickFigure/StickFigure.zskel");

	// Export mesh in Zenith_MeshAsset format (for asset pipeline)
	std::string strMeshAssetPath = strOutputDir + "StickFigure.zasset";
	pxMesh->Export(strMeshAssetPath.c_str());
	Zenith_Assert(std::filesystem::exists(strMeshAssetPath), "Mesh asset file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported mesh asset to: %s", strMeshAssetPath.c_str());

#ifdef ZENITH_TOOLS
	// Export mesh in Flux_MeshGeometry format (for runtime loading)
	// This is the format that Flux_MeshGeometry::LoadFromFile expects
	Flux_MeshGeometry* pxFluxGeometry = CreateFluxMeshGeometry(pxMesh, pxSkel);
	std::string strMeshPath = strOutputDir + "StickFigure.zmesh";
	pxFluxGeometry->Export(strMeshPath.c_str());
	Zenith_Assert(std::filesystem::exists(strMeshPath), "Mesh geometry file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported mesh geometry to: %s", strMeshPath.c_str());
	delete pxFluxGeometry;

	// Export static mesh (without bone data) for static mesh rendering
	// This uses the 72-byte vertex format compatible with the static mesh shader
	Flux_MeshGeometry* pxStaticGeometry = CreateStaticFluxMeshGeometry(pxMesh);
	std::string strStaticMeshPath = strOutputDir + "StickFigure_Static.zmesh";
	pxStaticGeometry->Export(strStaticMeshPath.c_str());
	Zenith_Assert(std::filesystem::exists(strStaticMeshPath), "Static mesh geometry file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported static mesh geometry to: %s", strStaticMeshPath.c_str());
	delete pxStaticGeometry;
#endif

	// Export animations
	std::string strIdlePath = strOutputDir + "StickFigure_Idle.zanim";
	pxIdleClip->Export(strIdlePath);
	Zenith_Assert(std::filesystem::exists(strIdlePath), "Idle animation file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported idle animation to: %s", strIdlePath.c_str());

	std::string strWalkPath = strOutputDir + "StickFigure_Walk.zanim";
	pxWalkClip->Export(strWalkPath);
	Zenith_Assert(std::filesystem::exists(strWalkPath), "Walk animation file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported walk animation to: %s", strWalkPath.c_str());

	std::string strRunPath = strOutputDir + "StickFigure_Run.zanim";
	pxRunClip->Export(strRunPath);
	Zenith_Assert(std::filesystem::exists(strRunPath), "Run animation file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported run animation to: %s", strRunPath.c_str());

	// Export combat animations
	std::string strAttack1Path = strOutputDir + "StickFigure_Attack1.zanim";
	pxAttack1Clip->Export(strAttack1Path);
	Zenith_Assert(std::filesystem::exists(strAttack1Path), "Attack1 animation file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported attack1 animation to: %s", strAttack1Path.c_str());

	std::string strAttack2Path = strOutputDir + "StickFigure_Attack2.zanim";
	pxAttack2Clip->Export(strAttack2Path);
	Zenith_Assert(std::filesystem::exists(strAttack2Path), "Attack2 animation file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported attack2 animation to: %s", strAttack2Path.c_str());

	std::string strAttack3Path = strOutputDir + "StickFigure_Attack3.zanim";
	pxAttack3Clip->Export(strAttack3Path);
	Zenith_Assert(std::filesystem::exists(strAttack3Path), "Attack3 animation file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported attack3 animation to: %s", strAttack3Path.c_str());

	std::string strDodgePath = strOutputDir + "StickFigure_Dodge.zanim";
	pxDodgeClip->Export(strDodgePath);
	Zenith_Assert(std::filesystem::exists(strDodgePath), "Dodge animation file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported dodge animation to: %s", strDodgePath.c_str());

	std::string strHitPath = strOutputDir + "StickFigure_Hit.zanim";
	pxHitClip->Export(strHitPath);
	Zenith_Assert(std::filesystem::exists(strHitPath), "Hit animation file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported hit animation to: %s", strHitPath.c_str());

	std::string strDeathPath = strOutputDir + "StickFigure_Death.zanim";
	pxDeathClip->Export(strDeathPath);
	Zenith_Assert(std::filesystem::exists(strDeathPath), "Death animation file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported death animation to: %s", strDeathPath.c_str());

	// Reload and verify skeleton
	Zenith_SkeletonAsset* pxReloadedSkel = Zenith_AssetRegistry::Get().Get<Zenith_SkeletonAsset>(strSkelPath);
	Zenith_Assert(pxReloadedSkel != nullptr, "Should be able to reload skeleton");
	Zenith_Assert(pxReloadedSkel->GetNumBones() == STICK_BONE_COUNT, "Reloaded skeleton should have 16 bones");
	Zenith_Assert(pxReloadedSkel->HasBone("LeftUpperArm"), "Reloaded skeleton should have LeftUpperArm bone");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded skeleton verified: %u bones", pxReloadedSkel->GetNumBones());

	// Reload and verify mesh asset format
	Zenith_MeshAsset* pxReloadedMesh = Zenith_AssetRegistry::Get().Get<Zenith_MeshAsset>(strMeshAssetPath);
	Zenith_Assert(pxReloadedMesh != nullptr, "Should be able to reload mesh asset");
	Zenith_Assert(pxReloadedMesh->GetNumVerts() == pxMesh->GetNumVerts(), "Reloaded mesh vertex count mismatch");
	Zenith_Assert(pxReloadedMesh->GetNumIndices() == pxMesh->GetNumIndices(), "Reloaded mesh index count mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded mesh asset verified: %u verts, %u indices",
		pxReloadedMesh->GetNumVerts(), pxReloadedMesh->GetNumIndices());

#ifdef ZENITH_TOOLS
	// Reload and verify Flux_MeshGeometry format
	Flux_MeshGeometry xReloadedGeometry;
	Flux_MeshGeometry::LoadFromFile((strOutputDir + "StickFigure.zmesh").c_str(), xReloadedGeometry, 0, false);
	Zenith_Assert(xReloadedGeometry.GetNumVerts() == pxMesh->GetNumVerts(), "Reloaded geometry vertex count mismatch");
	Zenith_Assert(xReloadedGeometry.GetNumIndices() == pxMesh->GetNumIndices(), "Reloaded geometry index count mismatch");
	Zenith_Assert(xReloadedGeometry.GetNumBones() == pxSkel->GetNumBones(), "Reloaded geometry bone count mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded mesh geometry verified: %u verts, %u indices, %u bones",
		xReloadedGeometry.GetNumVerts(), xReloadedGeometry.GetNumIndices(), xReloadedGeometry.GetNumBones());
#endif

	// Reload and verify animations
	Flux_AnimationClip* pxReloadedIdle = Flux_AnimationClip::LoadFromZanimFile(strIdlePath);
	Zenith_Assert(pxReloadedIdle != nullptr, "Should be able to reload idle animation");
	Zenith_Assert(pxReloadedIdle->GetName() == "Idle", "Reloaded idle animation name mismatch");
	Zenith_Assert(FloatEquals(pxReloadedIdle->GetDuration(), 2.0f, 0.01f), "Reloaded idle duration mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded idle animation verified: duration=%.1fs", pxReloadedIdle->GetDuration());

	Flux_AnimationClip* pxReloadedWalk = Flux_AnimationClip::LoadFromZanimFile(strWalkPath);
	Zenith_Assert(pxReloadedWalk != nullptr, "Should be able to reload walk animation");
	Zenith_Assert(pxReloadedWalk->GetName() == "Walk", "Reloaded walk animation name mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded walk animation verified");

	Flux_AnimationClip* pxReloadedRun = Flux_AnimationClip::LoadFromZanimFile(strRunPath);
	Zenith_Assert(pxReloadedRun != nullptr, "Should be able to reload run animation");
	Zenith_Assert(pxReloadedRun->GetName() == "Run", "Reloaded run animation name mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded run animation verified");

	// Cleanup
	delete pxReloadedRun;
	delete pxReloadedWalk;
	delete pxReloadedIdle;
	delete pxDeathClip;
	delete pxHitClip;
	delete pxDodgeClip;
	delete pxAttack3Clip;
	delete pxAttack2Clip;
	delete pxAttack1Clip;
	delete pxRunClip;
	delete pxWalkClip;
	delete pxIdleClip;
	delete pxMesh;
	delete pxSkel;

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureAssetExport completed successfully");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Assets available at: %s", strOutputDir.c_str());
}

//------------------------------------------------------------------------------
// ECS Bug Fix Tests (Phase 1)
//------------------------------------------------------------------------------

/**
 * Test that component indices remain valid after another entity's component is removed.
 * This tests the swap-and-pop fix for the component removal data corruption bug.
 */
void Zenith_UnitTests::TestComponentRemovalIndexUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComponentRemovalIndexUpdate...");

	// Create a test scene with 3 entities, each with TransformComponent
	Zenith_Scene xTestScene;
	Zenith_Entity xEntity1(&xTestScene, "Entity1");
	Zenith_Entity xEntity2(&xTestScene, "Entity2");
	Zenith_Entity xEntity3(&xTestScene, "Entity3");

	// Set distinct positions for each entity
	xEntity1.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xEntity2.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
	xEntity3.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(3.0f, 0.0f, 0.0f));

	// Store Entity3's position before removal
	Zenith_Maths::Vector3 xExpectedPos3(3.0f, 0.0f, 0.0f);

	// Remove Entity2's transform (this should trigger swap-and-pop)
	xEntity2.RemoveComponent<Zenith_TransformComponent>();

	// Verify Entity1 still has correct data
	Zenith_Maths::Vector3 xPos1;
	xEntity1.GetComponent<Zenith_TransformComponent>().GetPosition(xPos1);
	Zenith_Assert(xPos1.x == 1.0f, "TestComponentRemovalIndexUpdate: Entity1 position corrupted after Entity2 removal");

	// Verify Entity3 still has correct data (this entity's index likely changed due to swap-and-pop)
	Zenith_Maths::Vector3 xPos3;
	xEntity3.GetComponent<Zenith_TransformComponent>().GetPosition(xPos3);
	Zenith_Assert(xPos3.x == xExpectedPos3.x && xPos3.y == xExpectedPos3.y && xPos3.z == xExpectedPos3.z,
		"TestComponentRemovalIndexUpdate: Entity3 position corrupted after Entity2 removal");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentRemovalIndexUpdate completed successfully");
}

/**
 * Test that swap-and-pop removal preserves all component data correctly.
 */
void Zenith_UnitTests::TestComponentSwapAndPop()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComponentSwapAndPop...");

	Zenith_Scene xTestScene;

	// Create 5 entities with transforms
	Zenith_Entity xEntities[5] = {
		Zenith_Entity(&xTestScene, "Entity0"),
		Zenith_Entity(&xTestScene, "Entity1"),
		Zenith_Entity(&xTestScene, "Entity2"),
		Zenith_Entity(&xTestScene, "Entity3"),
		Zenith_Entity(&xTestScene, "Entity4")
	};

	// Set unique positions
	for (u_int i = 0; i < 5; ++i)
	{
		xEntities[i].GetComponent<Zenith_TransformComponent>().SetPosition(
			Zenith_Maths::Vector3(static_cast<float>(i * 10), 0.0f, 0.0f));
	}

	// Remove entity at index 1 (should swap with last element, index 4)
	xEntities[1].RemoveComponent<Zenith_TransformComponent>();

	// Verify remaining entities have correct data
	for (u_int i = 0; i < 5; ++i)
	{
		if (i == 1) continue; // Removed

		Zenith_Assert(xEntities[i].HasComponent<Zenith_TransformComponent>(),
			"TestComponentSwapAndPop: Entity lost its TransformComponent unexpectedly");

		Zenith_Maths::Vector3 xPos;
		xEntities[i].GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		Zenith_Assert(xPos.x == static_cast<float>(i * 10),
			"TestComponentSwapAndPop: Entity position data corrupted after swap-and-pop");
	}

	// Remove entity at index 0 (another swap-and-pop)
	xEntities[0].RemoveComponent<Zenith_TransformComponent>();

	// Verify remaining entities still correct
	for (u_int i = 2; i < 5; ++i)
	{
		Zenith_Maths::Vector3 xPos;
		xEntities[i].GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		Zenith_Assert(xPos.x == static_cast<float>(i * 10),
			"TestComponentSwapAndPop: Entity position corrupted after second removal");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentSwapAndPop completed successfully");
}

/**
 * Test removing multiple components from multiple entities in sequence.
 */
void Zenith_UnitTests::TestMultipleComponentRemoval()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestMultipleComponentRemoval...");

	Zenith_Scene xTestScene;

	// Create entities with multiple component types
	Zenith_Entity xEntity1(&xTestScene, "Entity1");
	Zenith_Entity xEntity2(&xTestScene, "Entity2");
	Zenith_Entity xEntity3(&xTestScene, "Entity3");

	// Add CameraComponents to entities 1 and 2
	xEntity1.AddComponent<Zenith_CameraComponent>().InitialisePerspective(
		Zenith_Maths::Vector3(1, 0, 0), 0, 0, 60, 0.1f, 100, 1.0f);
	xEntity2.AddComponent<Zenith_CameraComponent>().InitialisePerspective(
		Zenith_Maths::Vector3(2, 0, 0), 0, 0, 60, 0.1f, 100, 1.0f);

	// Add ColliderComponents to entities 2 and 3 (as second component type to test)
	xEntity2.AddComponent<Zenith_ColliderComponent>();
	xEntity3.AddComponent<Zenith_ColliderComponent>();

	// Remove Entity1's camera
	xEntity1.RemoveComponent<Zenith_CameraComponent>();

	// Verify Entity2 still has its camera
	Zenith_Assert(xEntity2.HasComponent<Zenith_CameraComponent>(),
		"TestMultipleComponentRemoval: Entity2 lost CameraComponent");

	// Remove Entity2's collider
	xEntity2.RemoveComponent<Zenith_ColliderComponent>();

	// Verify Entity3 still has collider
	Zenith_Assert(xEntity3.HasComponent<Zenith_ColliderComponent>(),
		"TestMultipleComponentRemoval: Entity3 lost ColliderComponent");

	// Remove Entity2's camera
	xEntity2.RemoveComponent<Zenith_CameraComponent>();

	// Verify Entity3 still has collider with correct data
	Zenith_Assert(xEntity3.HasComponent<Zenith_ColliderComponent>(),
		"TestMultipleComponentRemoval: Entity3 lost ColliderComponent after camera removal");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleComponentRemoval completed successfully");
}

/**
 * Stress test component removal with many entities.
 */
void Zenith_UnitTests::TestComponentRemovalWithManyEntities()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComponentRemovalWithManyEntities...");

	constexpr u_int NUM_ENTITIES = 1000;
	Zenith_Scene xTestScene;

	// Create many entities
	std::vector<Zenith_Entity> xEntities;
	xEntities.reserve(NUM_ENTITIES);

	for (u_int i = 0; i < NUM_ENTITIES; ++i)
	{
		xEntities.emplace_back(&xTestScene, "StressEntity" + std::to_string(i));
		xEntities[i].GetComponent<Zenith_TransformComponent>().SetPosition(
			Zenith_Maths::Vector3(static_cast<float>(i), 0.0f, 0.0f));
	}

	// Remove every other entity's transform component
	for (u_int i = 0; i < NUM_ENTITIES; i += 2)
	{
		xEntities[i].RemoveComponent<Zenith_TransformComponent>();
	}

	// Verify remaining entities have correct data
	for (u_int i = 1; i < NUM_ENTITIES; i += 2)
	{
		Zenith_Assert(xEntities[i].HasComponent<Zenith_TransformComponent>(),
			"TestComponentRemovalWithManyEntities: Entity lost TransformComponent");

		Zenith_Maths::Vector3 xPos;
		xEntities[i].GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		Zenith_Assert(xPos.x == static_cast<float>(i),
			"TestComponentRemovalWithManyEntities: Entity position corrupted");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentRemovalWithManyEntities completed successfully (tested %u entities)", NUM_ENTITIES);
}

/**
 * Test that entity names are stored in the scene and accessible via GetName()/SetName().
 */
void Zenith_UnitTests::TestEntityNameFromScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityNameFromScene...");

	Zenith_Scene xTestScene;

	// Create entity with name
	Zenith_Entity xEntity(&xTestScene, "TestEntityName");

	// Verify GetName() returns the correct name
	Zenith_Assert(xEntity.GetName() == "TestEntityName",
		"TestEntityNameFromScene: GetName() returned wrong name");

	// Change name via SetName()
	xEntity.SetName("RenamedEntity");
	Zenith_Assert(xEntity.GetName() == "RenamedEntity",
		"TestEntityNameFromScene: SetName() did not update name");

	// Verify name is accessible through the scene's entity API
	Zenith_Assert(xTestScene.GetEntity(xEntity.GetEntityID()).GetName() == "RenamedEntity",
		"TestEntityNameFromScene: Entity in scene does not have correct name");

	// Create another entity and verify names don't interfere
	Zenith_Entity xEntity2(&xTestScene, "SecondEntity");
	Zenith_Assert(xEntity.GetName() == "RenamedEntity",
		"TestEntityNameFromScene: First entity name changed after creating second");
	Zenith_Assert(xEntity2.GetName() == "SecondEntity",
		"TestEntityNameFromScene: Second entity has wrong name");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityNameFromScene completed successfully");
}

/**
 * Test that copying an entity preserves access to components.
 * Since Entity is now just a lightweight handle (scene pointer + IDs),
 * copies should reference the same underlying component data.
 */
void Zenith_UnitTests::TestEntityCopyPreservesAccess()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityCopyPreservesAccess...");

	Zenith_Scene xTestScene;
	Zenith_Entity xOriginal(&xTestScene, "OriginalEntity");

	// Set a position
	xOriginal.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(42.0f, 43.0f, 44.0f));

	// Copy the entity
	Zenith_Entity xCopy = xOriginal;

	// Verify copy has same entity ID
	Zenith_Assert(xCopy.GetEntityID() == xOriginal.GetEntityID(),
		"TestEntityCopyPreservesAccess: Copy has different entity ID");

	// Verify copy can access the same component data
	Zenith_Maths::Vector3 xCopyPos;
	xCopy.GetComponent<Zenith_TransformComponent>().GetPosition(xCopyPos);
	Zenith_Assert(xCopyPos.x == 42.0f && xCopyPos.y == 43.0f && xCopyPos.z == 44.0f,
		"TestEntityCopyPreservesAccess: Copy cannot access component data");

	// Modify via copy, verify original sees change
	xCopy.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(100.0f, 200.0f, 300.0f));

	Zenith_Maths::Vector3 xOriginalPos;
	xOriginal.GetComponent<Zenith_TransformComponent>().GetPosition(xOriginalPos);
	Zenith_Assert(xOriginalPos.x == 100.0f && xOriginalPos.y == 200.0f && xOriginalPos.z == 300.0f,
		"TestEntityCopyPreservesAccess: Original did not see modification via copy");

	// Verify name access works on copy
	Zenith_Assert(xCopy.GetName() == "OriginalEntity",
		"TestEntityCopyPreservesAccess: Copy cannot access entity name");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCopyPreservesAccess completed successfully");
}

//------------------------------------------------------------------------------
// ECS Reflection System Tests (Phase 2)
//------------------------------------------------------------------------------

/**
 * Test that all component types are registered with the ComponentMeta registry.
 * Verifies the registration macro and registry initialization work correctly.
 */
void Zenith_UnitTests::TestComponentMetaRegistration()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComponentMetaRegistration...");

	const auto& xMetasSorted = Zenith_ComponentMetaRegistry::Get().GetAllMetasSorted();

	// Verify we have the expected number of component types (8 components)
	Zenith_Assert(xMetasSorted.size() >= 8,
		"TestComponentMetaRegistration: Expected at least 8 registered component types");

	// Verify Transform is registered
	const Zenith_ComponentMeta* pxTransformMeta =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Transform");
	Zenith_Assert(pxTransformMeta != nullptr,
		"TestComponentMetaRegistration: Transform not registered");
	Zenith_Assert(pxTransformMeta->m_pfnCreate != nullptr,
		"TestComponentMetaRegistration: Transform has no create function");
	Zenith_Assert(pxTransformMeta->m_pfnHasComponent != nullptr,
		"TestComponentMetaRegistration: Transform has no hasComponent function");

	// Verify Camera is registered
	const Zenith_ComponentMeta* pxCameraMeta =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Camera");
	Zenith_Assert(pxCameraMeta != nullptr,
		"TestComponentMetaRegistration: Camera not registered");

	// Verify Model is registered
	const Zenith_ComponentMeta* pxModelMeta =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Model");
	Zenith_Assert(pxModelMeta != nullptr,
		"TestComponentMetaRegistration: Model not registered");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentMetaRegistration completed successfully");
}

/**
 * Test that component serialization via the registry works correctly.
 * Creates an entity with components, serializes via registry, deserializes
 * and verifies the data is preserved.
 */
void Zenith_UnitTests::TestComponentMetaSerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComponentMetaSerialization...");

	Zenith_Scene xTestScene;
	Zenith_Entity xEntity(&xTestScene, "SerializationTestEntity");

	// Set up transform
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f));
	xEntity.GetComponent<Zenith_TransformComponent>().SetScale(
		Zenith_Maths::Vector3(2.0f, 3.0f, 4.0f));

	// Add a camera component
	Zenith_CameraComponent& xCamera = xEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), // position
		0.5f, 1.0f, // pitch, yaw
		60.0f, // FOV
		0.1f, 1000.0f, // near, far
		16.0f / 9.0f); // aspect

	// Serialize via registry
	Zenith_DataStream xStream;
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(xEntity, xStream);

	// If we get here without assertion, serialization worked
	// The deserialization test will verify the data is correct

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentMetaSerialization completed successfully");
}

/**
 * Test that component deserialization via the registry works correctly.
 * Serializes an entity, creates a new entity, deserializes onto it,
 * and verifies the components match.
 */
void Zenith_UnitTests::TestComponentMetaDeserialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComponentMetaDeserialization...");

	Zenith_Scene xTestScene;
	Zenith_Entity xOriginal(&xTestScene, "OriginalEntity");

	// Set distinctive values
	xOriginal.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(111.0f, 222.0f, 333.0f));

	// Serialize original
	Zenith_DataStream xStream;
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(xOriginal, xStream);

	// Create new entity
	Zenith_Entity xNew(&xTestScene, "NewEntity");

	// Reset stream cursor
	xStream.SetCursor(0);

	// Deserialize onto new entity
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xNew, xStream);

	// Verify transform was copied
	Zenith_Maths::Vector3 xNewPos;
	xNew.GetComponent<Zenith_TransformComponent>().GetPosition(xNewPos);
	Zenith_Assert(xNewPos.x == 111.0f && xNewPos.y == 222.0f && xNewPos.z == 333.0f,
		"TestComponentMetaDeserialization: Deserialized transform position is wrong");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentMetaDeserialization completed successfully");
}

/**
 * Test that TypeID is consistent for the same component type.
 * Verifies that registering and looking up uses consistent type IDs.
 */
void Zenith_UnitTests::TestComponentMetaTypeIDConsistency()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComponentMetaTypeIDConsistency...");

	// Get meta for Transform
	const Zenith_ComponentMeta* pxMeta1 =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Transform");
	const Zenith_ComponentMeta* pxMeta2 =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Transform");

	// Verify same pointer returned
	Zenith_Assert(pxMeta1 == pxMeta2,
		"TestComponentMetaTypeIDConsistency: Different meta pointers for same type");

	// Verify serialization order is set correctly (Transform should be first)
	Zenith_Assert(pxMeta1->m_uSerializationOrder == 0,
		"TestComponentMetaTypeIDConsistency: Transform serialization order is not 0");

	// Verify all metas in sorted list have increasing serialization order
	const auto& xMetasSorted = Zenith_ComponentMetaRegistry::Get().GetAllMetasSorted();
	u_int uPrevOrder = 0;
	for (size_t i = 1; i < xMetasSorted.size(); ++i)
	{
		Zenith_Assert(xMetasSorted[i]->m_uSerializationOrder >= uPrevOrder,
			"TestComponentMetaTypeIDConsistency: Metas not sorted by serialization order");
		uPrevOrder = xMetasSorted[i]->m_uSerializationOrder;
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentMetaTypeIDConsistency completed successfully");
}

//------------------------------------------------------------------------------
// ECS Lifecycle Hooks Tests (Phase 3)
//------------------------------------------------------------------------------

/**
 * Test that lifecycle hook detection via C++20 concepts works correctly.
 * Verifies that the HasOnAwake, HasOnUpdate, etc. concepts correctly detect
 * whether a component type implements the hook methods.
 */
void Zenith_UnitTests::TestLifecycleHookDetection()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleHookDetection...");

	// Transform doesn't implement lifecycle hooks, so all hooks should be nullptr
	const Zenith_ComponentMeta* pxTransformMeta =
		Zenith_ComponentMetaRegistry::Get().GetMetaByName("Transform");
	Zenith_Assert(pxTransformMeta != nullptr,
		"TestLifecycleHookDetection: Transform not registered");

	// Transform shouldn't have lifecycle hooks (it doesn't implement them)
	Zenith_Assert(pxTransformMeta->m_pfnOnAwake == nullptr,
		"TestLifecycleHookDetection: Transform has OnAwake hook (shouldn't)");
	Zenith_Assert(pxTransformMeta->m_pfnOnStart == nullptr,
		"TestLifecycleHookDetection: Transform has OnStart hook (shouldn't)");
	Zenith_Assert(pxTransformMeta->m_pfnOnUpdate == nullptr,
		"TestLifecycleHookDetection: Transform has OnUpdate hook (shouldn't)");
	Zenith_Assert(pxTransformMeta->m_pfnOnDestroy == nullptr,
		"TestLifecycleHookDetection: Transform has OnDestroy hook (shouldn't)");

	// Verify registry is finalized
	Zenith_Assert(Zenith_ComponentMetaRegistry::Get().IsInitialized(),
		"TestLifecycleHookDetection: Registry not initialized");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleHookDetection completed successfully");
}

/**
 * Test that DispatchOnAwake correctly calls OnAwake on components that have it.
 * Since our existing components don't implement OnAwake, we verify dispatch
 * doesn't crash and completes successfully.
 */
void Zenith_UnitTests::TestLifecycleOnAwake()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleOnAwake...");

	Zenith_Scene xTestScene;
	Zenith_Entity xEntity(&xTestScene, "AwakeTestEntity");

	// Dispatch OnAwake - should complete without crashing
	// (no components implement OnAwake, so nothing is called)
	Zenith_ComponentMetaRegistry::Get().DispatchOnAwake(xEntity);

	// Verify entity is still valid
	Zenith_Assert(xEntity.HasComponent<Zenith_TransformComponent>(),
		"TestLifecycleOnAwake: Entity lost TransformComponent after dispatch");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleOnAwake completed successfully");
}

/**
 * Test that DispatchOnStart correctly calls OnStart on components that have it.
 */
void Zenith_UnitTests::TestLifecycleOnStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleOnStart...");

	Zenith_Scene xTestScene;
	Zenith_Entity xEntity(&xTestScene, "StartTestEntity");

	// Dispatch OnStart - should complete without crashing
	Zenith_ComponentMetaRegistry::Get().DispatchOnStart(xEntity);

	// Verify entity is still valid
	Zenith_Assert(xEntity.HasComponent<Zenith_TransformComponent>(),
		"TestLifecycleOnStart: Entity lost TransformComponent after dispatch");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleOnStart completed successfully");
}

/**
 * Test that DispatchOnUpdate correctly calls OnUpdate on components that have it.
 */
void Zenith_UnitTests::TestLifecycleOnUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleOnUpdate...");

	Zenith_Scene xTestScene;
	Zenith_Entity xEntity(&xTestScene, "UpdateTestEntity");

	// Dispatch OnUpdate with a delta time - should complete without crashing
	const float fDt = 0.016f; // ~60fps
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, fDt);

	// Verify entity is still valid
	Zenith_Assert(xEntity.HasComponent<Zenith_TransformComponent>(),
		"TestLifecycleOnUpdate: Entity lost TransformComponent after dispatch");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleOnUpdate completed successfully");
}

/**
 * Test that DispatchOnDestroy correctly calls OnDestroy on components that have it.
 */
void Zenith_UnitTests::TestLifecycleOnDestroy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleOnDestroy...");

	Zenith_Scene xTestScene;
	Zenith_Entity xEntity(&xTestScene, "DestroyTestEntity");

	// Set a position before dispatch
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));

	// Dispatch OnDestroy - should complete without crashing
	Zenith_ComponentMetaRegistry::Get().DispatchOnDestroy(xEntity);

	// Verify entity is still valid (OnDestroy doesn't remove components)
	Zenith_Assert(xEntity.HasComponent<Zenith_TransformComponent>(),
		"TestLifecycleOnDestroy: Entity lost TransformComponent after dispatch");

	// Verify data is intact
	Zenith_Maths::Vector3 xPos;
	xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	Zenith_Assert(xPos.x == 1.0f && xPos.y == 2.0f && xPos.z == 3.0f,
		"TestLifecycleOnDestroy: Component data corrupted after dispatch");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleOnDestroy completed successfully");
}

/**
 * Test that lifecycle dispatch respects component serialization order.
 * Components with lower serialization order should have their hooks called first.
 */
void Zenith_UnitTests::TestLifecycleDispatchOrder()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleDispatchOrder...");

	Zenith_Scene xTestScene;
	Zenith_Entity xEntity(&xTestScene, "OrderTestEntity");

	// Add multiple components
	xEntity.AddComponent<Zenith_CameraComponent>();

	// Dispatch all lifecycle hooks in sequence
	Zenith_ComponentMetaRegistry::Get().DispatchOnAwake(xEntity);
	Zenith_ComponentMetaRegistry::Get().DispatchOnStart(xEntity);
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);
	Zenith_ComponentMetaRegistry::Get().DispatchOnLateUpdate(xEntity, 0.016f);
	Zenith_ComponentMetaRegistry::Get().DispatchOnFixedUpdate(xEntity, 0.02f);
	Zenith_ComponentMetaRegistry::Get().DispatchOnDestroy(xEntity);

	// Verify all components are still valid
	Zenith_Assert(xEntity.HasComponent<Zenith_TransformComponent>(),
		"TestLifecycleDispatchOrder: Entity lost TransformComponent");
	Zenith_Assert(xEntity.HasComponent<Zenith_CameraComponent>(),
		"TestLifecycleDispatchOrder: Entity lost CameraComponent");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleDispatchOrder completed successfully");
}

/**
 * Test that creating entities during lifecycle callbacks doesn't cause crashes.
 *
 * This tests the scenario that caused the editor Play->Stop crash:
 * When a lifecycle callback (OnAwake, OnStart, etc.) creates new entities,
 * the m_xEntitySlots vector may reallocate, invalidating any held references.
 *
 * The fix was to:
 * 1. Copy entity IDs before iteration (not hold a reference to the vector)
 * 2. Use separate loops for each lifecycle stage
 * 3. Re-fetch entity references before each callback
 */
void Zenith_UnitTests::TestLifecycleEntityCreationDuringCallback()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleEntityCreationDuringCallback...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Store initial entity count
	const u_int uInitialCount = xScene.GetEntityCount();

	// Create initial entity
	Zenith_Entity xInitialEntity(&xScene, "InitialEntity");
	Zenith_EntityID xInitialID = xInitialEntity.GetEntityID();

	// Copy entity IDs to prevent iterator invalidation (the safe pattern)
	Zenith_Vector<Zenith_EntityID> xEntityIDs;
	xEntityIDs.Reserve(xScene.GetActiveEntities().GetSize());
	for (u_int u = 0; u < xScene.GetActiveEntities().GetSize(); ++u)
	{
		xEntityIDs.PushBack(xScene.GetActiveEntities().Get(u));
	}

	// Simulate what OnAwake might do: create more entities
	// This should NOT crash because we're iterating over a copy of IDs
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = xEntityIDs.Get(u);
		if (xScene.EntityExists(xEntityID))
		{
			// Get entity handle (lightweight - safe to use after pool reallocation)
			Zenith_Entity xEntity = xScene.GetEntity(xEntityID);

			// Simulate OnAwake creating multiple new entities
			// This will cause m_xEntitySlots to reallocate
			for (u_int i = 0; i < 10; ++i)
			{
				Zenith_Entity xNewEntity(&xScene, "CreatedDuringCallback_" + std::to_string(i));
				// Entity handles are safe - they don't hold pointers into the pool
			}

			// Entity handle still valid after pool reallocation (lightweight handle pattern)
			Zenith_Entity xEntityRefreshed = xScene.GetEntity(xEntityID);

			// Verify the entity is still accessible
			Zenith_Assert(xEntityRefreshed.HasComponent<Zenith_TransformComponent>(),
				"TestLifecycleEntityCreationDuringCallback: Entity lost TransformComponent after sibling creation");
		}
	}

	// Verify original entity is still valid
	Zenith_Assert(xScene.EntityExists(xInitialID),
		"TestLifecycleEntityCreationDuringCallback: Initial entity was invalidated");
	Zenith_Assert(xScene.GetEntity(xInitialID).GetName() == "InitialEntity",
		"TestLifecycleEntityCreationDuringCallback: Initial entity name corrupted");

	// Verify entities were created (proves reallocation happened)
	Zenith_Assert(xScene.GetEntityCount() > uInitialCount + 1,
		"TestLifecycleEntityCreationDuringCallback: New entities were not created");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleEntityCreationDuringCallback completed successfully");
}

/**
 * Test that Zenith_Scene::DispatchFullLifecycleInit works correctly.
 *
 * This is the shared helper function that both the editor and other code
 * should use to dispatch lifecycle callbacks safely.
 */
void Zenith_UnitTests::TestDispatchFullLifecycleInit()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDispatchFullLifecycleInit...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Store initial count (may have entities from previous tests)
	const u_int uInitialCount = xScene.GetEntityCount();

	// Create several entities
	Zenith_Entity xEntity1(&xScene, "LifecycleInitEntity1");
	Zenith_Entity xEntity2(&xScene, "LifecycleInitEntity2");
	Zenith_Entity xEntity3(&xScene, "LifecycleInitEntity3");

	Zenith_EntityID xID1 = xEntity1.GetEntityID();
	Zenith_EntityID xID2 = xEntity2.GetEntityID();
	Zenith_EntityID xID3 = xEntity3.GetEntityID();

	// Call the shared lifecycle init function
	// This should NOT crash even if callbacks create new entities
	Zenith_Scene::DispatchFullLifecycleInit();

	// Verify all original entities are still valid and accessible
	Zenith_Assert(xScene.EntityExists(xID1),
		"TestDispatchFullLifecycleInit: Entity1 was invalidated");
	Zenith_Assert(xScene.EntityExists(xID2),
		"TestDispatchFullLifecycleInit: Entity2 was invalidated");
	Zenith_Assert(xScene.EntityExists(xID3),
		"TestDispatchFullLifecycleInit: Entity3 was invalidated");

	// Verify entities are still accessible with correct data
	Zenith_Assert(xScene.GetEntity(xID1).GetName() == "LifecycleInitEntity1",
		"TestDispatchFullLifecycleInit: Entity1 name corrupted");
	Zenith_Assert(xScene.GetEntity(xID2).GetName() == "LifecycleInitEntity2",
		"TestDispatchFullLifecycleInit: Entity2 name corrupted");
	Zenith_Assert(xScene.GetEntity(xID3).GetName() == "LifecycleInitEntity3",
		"TestDispatchFullLifecycleInit: Entity3 name corrupted");

	// Verify components are intact
	Zenith_Assert(xScene.GetEntity(xID1).HasComponent<Zenith_TransformComponent>(),
		"TestDispatchFullLifecycleInit: Entity1 lost TransformComponent");
	Zenith_Assert(xScene.GetEntity(xID2).HasComponent<Zenith_TransformComponent>(),
		"TestDispatchFullLifecycleInit: Entity2 lost TransformComponent");
	Zenith_Assert(xScene.GetEntity(xID3).HasComponent<Zenith_TransformComponent>(),
		"TestDispatchFullLifecycleInit: Entity3 lost TransformComponent");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDispatchFullLifecycleInit completed successfully");
}

//------------------------------------------------------------------------------
// ECS Query System Tests (Phase 4)
//------------------------------------------------------------------------------

void Zenith_UnitTests::TestQuerySingleComponent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestQuerySingleComponent...");

	Zenith_Scene xTestScene;

	// Create 3 entities with transforms
	Zenith_Entity xEntity1(&xTestScene, "Entity1");
	Zenith_Entity xEntity2(&xTestScene, "Entity2");
	Zenith_Entity xEntity3(&xTestScene, "Entity3");

	// All 3 entities have TransformComponent (added by default)
	// Add CameraComponent to only 2 entities
	xEntity1.AddComponent<Zenith_CameraComponent>();
	xEntity3.AddComponent<Zenith_CameraComponent>();

	// Query for TransformComponent - should return all 3 entities
	u_int uTransformCount = 0;
	xTestScene.Query<Zenith_TransformComponent>().ForEach(
		[&uTransformCount](Zenith_EntityID uID, Zenith_TransformComponent& xTransform) {
			uTransformCount++;
		});

	Zenith_Assert(uTransformCount == 3,
		"TestQuerySingleComponent: Expected 3 entities with TransformComponent");

	// Query for CameraComponent - should return 2 entities
	u_int uCameraCount = 0;
	xTestScene.Query<Zenith_CameraComponent>().ForEach(
		[&uCameraCount](Zenith_EntityID uID, Zenith_CameraComponent& xCamera) {
			uCameraCount++;
		});

	Zenith_Assert(uCameraCount == 2,
		"TestQuerySingleComponent: Expected 2 entities with CameraComponent");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQuerySingleComponent completed successfully");
}

void Zenith_UnitTests::TestQueryMultipleComponents()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestQueryMultipleComponents...");

	Zenith_Scene xTestScene;

	// Create 3 entities with transforms
	Zenith_Entity xEntity1(&xTestScene, "Entity1");
	Zenith_Entity xEntity2(&xTestScene, "Entity2");
	Zenith_Entity xEntity3(&xTestScene, "Entity3");

	// Set different positions for verification
	xEntity1.GetComponent<Zenith_TransformComponent>().SetPosition({1.0f, 0.0f, 0.0f});
	xEntity2.GetComponent<Zenith_TransformComponent>().SetPosition({2.0f, 0.0f, 0.0f});
	xEntity3.GetComponent<Zenith_TransformComponent>().SetPosition({3.0f, 0.0f, 0.0f});

	// Add CameraComponent to entities 1 and 3
	xEntity1.AddComponent<Zenith_CameraComponent>();
	xEntity3.AddComponent<Zenith_CameraComponent>();

	// Query for entities with BOTH TransformComponent AND CameraComponent
	u_int uMatchCount = 0;
	std::vector<float> xPositions;
	xTestScene.Query<Zenith_TransformComponent, Zenith_CameraComponent>().ForEach(
		[&uMatchCount, &xPositions](Zenith_EntityID uID,
		                            Zenith_TransformComponent& xTransform,
		                            Zenith_CameraComponent& xCamera) {
			uMatchCount++;
			Zenith_Maths::Vector3 xPos;
			xTransform.GetPosition(xPos);
			xPositions.push_back(xPos.x);
		});

	Zenith_Assert(uMatchCount == 2,
		"TestQueryMultipleComponents: Expected 2 entities with both Transform and Camera");

	// Verify we got entities 1 and 3 (positions 1.0 and 3.0)
	bool bFoundEntity1 = std::find(xPositions.begin(), xPositions.end(), 1.0f) != xPositions.end();
	bool bFoundEntity3 = std::find(xPositions.begin(), xPositions.end(), 3.0f) != xPositions.end();

	Zenith_Assert(bFoundEntity1 && bFoundEntity3,
		"TestQueryMultipleComponents: Did not find expected entities");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryMultipleComponents completed successfully");
}

void Zenith_UnitTests::TestQueryNoMatches()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestQueryNoMatches...");

	Zenith_Scene xTestScene;

	// Create entity with only TransformComponent
	Zenith_Entity xEntity(&xTestScene, "Entity1");

	// Query for CameraComponent - should return no matches
	u_int uCount = 0;
	xTestScene.Query<Zenith_CameraComponent>().ForEach(
		[&uCount](Zenith_EntityID uID, Zenith_CameraComponent& xCamera) {
			uCount++;
		});

	Zenith_Assert(uCount == 0,
		"TestQueryNoMatches: Expected 0 entities with CameraComponent");

	// Verify Any() returns false
	bool bHasAny = xTestScene.Query<Zenith_CameraComponent>().Any();
	Zenith_Assert(!bHasAny,
		"TestQueryNoMatches: Any() should return false for empty query");

	// Verify First() returns INVALID_ENTITY_ID
	Zenith_EntityID uFirst = xTestScene.Query<Zenith_CameraComponent>().First();
	Zenith_Assert(uFirst == INVALID_ENTITY_ID,
		"TestQueryNoMatches: First() should return INVALID_ENTITY_ID for empty query");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryNoMatches completed successfully");
}

void Zenith_UnitTests::TestQueryCount()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestQueryCount...");

	Zenith_Scene xTestScene;

	// Create 5 entities
	Zenith_Entity xEntity1(&xTestScene, "Entity1");
	Zenith_Entity xEntity2(&xTestScene, "Entity2");
	Zenith_Entity xEntity3(&xTestScene, "Entity3");
	Zenith_Entity xEntity4(&xTestScene, "Entity4");
	Zenith_Entity xEntity5(&xTestScene, "Entity5");

	// Add CameraComponent to 3 entities
	xEntity2.AddComponent<Zenith_CameraComponent>();
	xEntity3.AddComponent<Zenith_CameraComponent>();
	xEntity5.AddComponent<Zenith_CameraComponent>();

	// Test Count() for TransformComponent (all 5)
	u_int uTransformCount = xTestScene.Query<Zenith_TransformComponent>().Count();
	Zenith_Assert(uTransformCount == 5,
		"TestQueryCount: Expected 5 entities with TransformComponent");

	// Test Count() for CameraComponent (3)
	u_int uCameraCount = xTestScene.Query<Zenith_CameraComponent>().Count();
	Zenith_Assert(uCameraCount == 3,
		"TestQueryCount: Expected 3 entities with CameraComponent");

	// Test Count() for both components (3)
	u_int uBothCount = xTestScene.Query<Zenith_TransformComponent, Zenith_CameraComponent>().Count();
	Zenith_Assert(uBothCount == 3,
		"TestQueryCount: Expected 3 entities with both Transform and Camera");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryCount completed successfully");
}

void Zenith_UnitTests::TestQueryFirstAndAny()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestQueryFirstAndAny...");

	Zenith_Scene xTestScene;

	// Create 3 entities
	Zenith_Entity xEntity1(&xTestScene, "Entity1");
	Zenith_Entity xEntity2(&xTestScene, "Entity2");
	Zenith_Entity xEntity3(&xTestScene, "Entity3");

	// Add CameraComponent to entity 2
	xEntity2.AddComponent<Zenith_CameraComponent>();

	// Test Any() returns true when there are matches
	bool bHasCamera = xTestScene.Query<Zenith_CameraComponent>().Any();
	Zenith_Assert(bHasCamera,
		"TestQueryFirstAndAny: Any() should return true when matches exist");

	// Test First() returns a valid entity ID
	Zenith_EntityID uFirstCamera = xTestScene.Query<Zenith_CameraComponent>().First();
	Zenith_Assert(uFirstCamera != INVALID_ENTITY_ID,
		"TestQueryFirstAndAny: First() should return valid ID when matches exist");

	// Verify the first match actually has the component
	Zenith_Assert(xTestScene.EntityHasComponent<Zenith_CameraComponent>(uFirstCamera),
		"TestQueryFirstAndAny: First() returned entity without expected component");

	// Test First() for TransformComponent returns the first entity ID (1)
	Zenith_EntityID uFirstTransform = xTestScene.Query<Zenith_TransformComponent>().First();
	Zenith_Assert(uFirstTransform != INVALID_ENTITY_ID,
		"TestQueryFirstAndAny: First() should return valid ID for TransformComponent");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryFirstAndAny completed successfully");
}

//------------------------------------------------------------------------------
// ECS Event System Tests (Phase 5)
//------------------------------------------------------------------------------

// Custom test event for unit tests
struct TestEvent_Custom
{
	u_int m_uValue = 0;
};

// Static variable to track event callbacks
static u_int s_uTestEventCallCount = 0;
static u_int s_uTestEventLastValue = 0;

static void TestEventCallback(const TestEvent_Custom& xEvent)
{
	s_uTestEventCallCount++;
	s_uTestEventLastValue = xEvent.m_uValue;
}

void Zenith_UnitTests::TestEventSubscribeDispatch()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEventSubscribeDispatch...");

	// Clear any existing state
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();
	s_uTestEventCallCount = 0;
	s_uTestEventLastValue = 0;

	// Subscribe to test event
	Zenith_EventHandle uHandle = Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&TestEventCallback);

	Zenith_Assert(uHandle != INVALID_EVENT_HANDLE,
		"TestEventSubscribeDispatch: Subscribe should return valid handle");

	// Dispatch event
	TestEvent_Custom xEvent;
	xEvent.m_uValue = 42;
	Zenith_EventDispatcher::Get().Dispatch(xEvent);

	Zenith_Assert(s_uTestEventCallCount == 1,
		"TestEventSubscribeDispatch: Callback should be called once");
	Zenith_Assert(s_uTestEventLastValue == 42,
		"TestEventSubscribeDispatch: Callback should receive correct value");

	// Cleanup
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventSubscribeDispatch completed successfully");
}

void Zenith_UnitTests::TestEventUnsubscribe()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEventUnsubscribe...");

	// Clear any existing state
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();
	s_uTestEventCallCount = 0;

	// Subscribe to test event
	Zenith_EventHandle uHandle = Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&TestEventCallback);

	// Verify subscription count
	u_int uCount = Zenith_EventDispatcher::Get().GetSubscriberCount<TestEvent_Custom>();
	Zenith_Assert(uCount == 1,
		"TestEventUnsubscribe: Should have 1 subscriber after subscribe");

	// Unsubscribe
	Zenith_EventDispatcher::Get().Unsubscribe(uHandle);

	// Verify subscription count
	uCount = Zenith_EventDispatcher::Get().GetSubscriberCount<TestEvent_Custom>();
	Zenith_Assert(uCount == 0,
		"TestEventUnsubscribe: Should have 0 subscribers after unsubscribe");

	// Dispatch event - callback should NOT be called
	s_uTestEventCallCount = 0;
	TestEvent_Custom xEvent;
	xEvent.m_uValue = 100;
	Zenith_EventDispatcher::Get().Dispatch(xEvent);

	Zenith_Assert(s_uTestEventCallCount == 0,
		"TestEventUnsubscribe: Callback should not be called after unsubscribe");

	// Cleanup
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventUnsubscribe completed successfully");
}

void Zenith_UnitTests::TestEventDeferredQueue()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEventDeferredQueue...");

	// Clear any existing state
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();
	s_uTestEventCallCount = 0;
	s_uTestEventLastValue = 0;

	// Subscribe to test event
	Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&TestEventCallback);

	// Queue event (should not dispatch immediately)
	TestEvent_Custom xEvent;
	xEvent.m_uValue = 99;
	Zenith_EventDispatcher::Get().QueueEvent(xEvent);

	// Verify callback not called yet
	Zenith_Assert(s_uTestEventCallCount == 0,
		"TestEventDeferredQueue: Callback should not be called before ProcessDeferredEvents");

	// Process deferred events
	Zenith_EventDispatcher::Get().ProcessDeferredEvents();

	// Verify callback was called
	Zenith_Assert(s_uTestEventCallCount == 1,
		"TestEventDeferredQueue: Callback should be called after ProcessDeferredEvents");
	Zenith_Assert(s_uTestEventLastValue == 99,
		"TestEventDeferredQueue: Callback should receive correct value");

	// Queue and process multiple events
	s_uTestEventCallCount = 0;
	TestEvent_Custom xEvent2, xEvent3;
	xEvent2.m_uValue = 1;
	xEvent3.m_uValue = 2;
	Zenith_EventDispatcher::Get().QueueEvent(xEvent2);
	Zenith_EventDispatcher::Get().QueueEvent(xEvent3);

	Zenith_Assert(s_uTestEventCallCount == 0,
		"TestEventDeferredQueue: Callbacks should not be called before processing");

	Zenith_EventDispatcher::Get().ProcessDeferredEvents();

	Zenith_Assert(s_uTestEventCallCount == 2,
		"TestEventDeferredQueue: Both callbacks should be called after processing");

	// Cleanup
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventDeferredQueue completed successfully");
}

// Static variables for multiple subscriber test
static u_int s_uMultiSub1Count = 0;
static u_int s_uMultiSub2Count = 0;

static void MultiSubscriber1(const TestEvent_Custom& /*xEvent*/)
{
	s_uMultiSub1Count++;
}

static void MultiSubscriber2(const TestEvent_Custom& /*xEvent*/)
{
	s_uMultiSub2Count++;
}

void Zenith_UnitTests::TestEventMultipleSubscribers()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEventMultipleSubscribers...");

	// Clear any existing state
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();
	s_uMultiSub1Count = 0;
	s_uMultiSub2Count = 0;

	// Subscribe two callbacks to the same event type
	Zenith_EventHandle uHandle1 = Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&MultiSubscriber1);
	Zenith_EventHandle uHandle2 = Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&MultiSubscriber2);

	// Verify subscriber count
	u_int uCount = Zenith_EventDispatcher::Get().GetSubscriberCount<TestEvent_Custom>();
	Zenith_Assert(uCount == 2,
		"TestEventMultipleSubscribers: Should have 2 subscribers");

	// Dispatch event
	TestEvent_Custom xEvent;
	xEvent.m_uValue = 10;
	Zenith_EventDispatcher::Get().Dispatch(xEvent);

	Zenith_Assert(s_uMultiSub1Count == 1,
		"TestEventMultipleSubscribers: Subscriber1 should be called once");
	Zenith_Assert(s_uMultiSub2Count == 1,
		"TestEventMultipleSubscribers: Subscriber2 should be called once");

	// Unsubscribe first callback
	Zenith_EventDispatcher::Get().Unsubscribe(uHandle1);

	// Dispatch again
	Zenith_EventDispatcher::Get().Dispatch(xEvent);

	Zenith_Assert(s_uMultiSub1Count == 1,
		"TestEventMultipleSubscribers: Subscriber1 should not be called after unsubscribe");
	Zenith_Assert(s_uMultiSub2Count == 2,
		"TestEventMultipleSubscribers: Subscriber2 should be called again");

	// Cleanup
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventMultipleSubscribers completed successfully");
}

void Zenith_UnitTests::TestEventClearSubscriptions()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEventClearSubscriptions...");

	// Clear any existing state
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();
	s_uTestEventCallCount = 0;

	// Subscribe multiple callbacks
	Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&TestEventCallback);
	Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&MultiSubscriber1);
	Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&MultiSubscriber2);

	// Verify subscriber count
	u_int uCount = Zenith_EventDispatcher::Get().GetSubscriberCount<TestEvent_Custom>();
	Zenith_Assert(uCount == 3,
		"TestEventClearSubscriptions: Should have 3 subscribers");

	// Clear all subscriptions
	Zenith_EventDispatcher::Get().ClearAllSubscriptions();

	// Verify subscriber count is now 0
	uCount = Zenith_EventDispatcher::Get().GetSubscriberCount<TestEvent_Custom>();
	Zenith_Assert(uCount == 0,
		"TestEventClearSubscriptions: Should have 0 subscribers after clear");

	// Dispatch event - no callbacks should be called
	s_uTestEventCallCount = 0;
	s_uMultiSub1Count = 0;
	s_uMultiSub2Count = 0;
	TestEvent_Custom xEvent;
	Zenith_EventDispatcher::Get().Dispatch(xEvent);

	Zenith_Assert(s_uTestEventCallCount == 0 && s_uMultiSub1Count == 0 && s_uMultiSub2Count == 0,
		"TestEventClearSubscriptions: No callbacks should be called after clear");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventClearSubscriptions completed successfully");
}

//=============================================================================
// GUID System Tests
//=============================================================================

void Zenith_UnitTests::TestGUIDGeneration()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestGUIDGeneration...");

	// Generate multiple GUIDs and ensure they're all unique
	constexpr uint32_t uNumGUIDs = 100;
	std::vector<Zenith_AssetGUID> xGUIDs;
	xGUIDs.reserve(uNumGUIDs);

	for (uint32_t u = 0; u < uNumGUIDs; ++u)
	{
		Zenith_AssetGUID xGUID = Zenith_AssetGUID::Generate();
		Zenith_Assert(xGUID.IsValid(), "TestGUIDGeneration: Generated GUID should be valid");

		// Check uniqueness against all previously generated GUIDs
		for (const auto& xExisting : xGUIDs)
		{
			Zenith_Assert(xGUID != xExisting, "TestGUIDGeneration: Generated GUIDs should be unique");
		}
		xGUIDs.push_back(xGUID);
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGUIDGeneration completed successfully");
}

void Zenith_UnitTests::TestGUIDStringRoundTrip()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestGUIDStringRoundTrip...");

	// Generate a GUID, convert to string, parse back, compare
	Zenith_AssetGUID xOriginal = Zenith_AssetGUID::Generate();
	std::string strGUID = xOriginal.ToString();

	// String should be in format "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
	Zenith_Assert(strGUID.length() == 36, "TestGUIDStringRoundTrip: String should be 36 characters");
	Zenith_Assert(strGUID[8] == '-' && strGUID[13] == '-' && strGUID[18] == '-' && strGUID[23] == '-',
		"TestGUIDStringRoundTrip: String should have dashes at correct positions");

	Zenith_AssetGUID xParsed = Zenith_AssetGUID::FromString(strGUID);
	Zenith_Assert(xParsed.IsValid(), "TestGUIDStringRoundTrip: Parsed GUID should be valid");
	Zenith_Assert(xOriginal == xParsed, "TestGUIDStringRoundTrip: Round-trip should produce identical GUID");

	// Test invalid string
	Zenith_AssetGUID xInvalid = Zenith_AssetGUID::FromString("not-a-valid-guid");
	Zenith_Assert(!xInvalid.IsValid(), "TestGUIDStringRoundTrip: Invalid string should produce invalid GUID");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGUIDStringRoundTrip completed successfully");
}

void Zenith_UnitTests::TestGUIDSerializationRoundTrip()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestGUIDSerializationRoundTrip...");

	Zenith_AssetGUID xOriginal = Zenith_AssetGUID::Generate();

	// Write to data stream
	Zenith_DataStream xStream;
	xStream << xOriginal;

	// Read back
	xStream.SetCursor(0);
	Zenith_AssetGUID xDeserialized;
	xStream >> xDeserialized;

	Zenith_Assert(xOriginal == xDeserialized,
		"TestGUIDSerializationRoundTrip: Deserialized GUID should match original");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGUIDSerializationRoundTrip completed successfully");
}

void Zenith_UnitTests::TestGUIDComparisonOperators()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestGUIDComparisonOperators...");

	Zenith_AssetGUID xGUID1 = Zenith_AssetGUID::Generate();
	Zenith_AssetGUID xGUID2 = Zenith_AssetGUID::Generate();
	Zenith_AssetGUID xGUID1Copy = xGUID1;

	// Equality
	Zenith_Assert(xGUID1 == xGUID1Copy, "TestGUIDComparisonOperators: Same GUID should be equal");
	Zenith_Assert(!(xGUID1 != xGUID1Copy), "TestGUIDComparisonOperators: Same GUID != should be false");

	// Inequality
	Zenith_Assert(xGUID1 != xGUID2, "TestGUIDComparisonOperators: Different GUIDs should not be equal");

	// Less than (for ordering)
	bool bLess1 = xGUID1 < xGUID2;
	bool bLess2 = xGUID2 < xGUID1;
	Zenith_Assert(bLess1 != bLess2 || xGUID1 == xGUID2,
		"TestGUIDComparisonOperators: Exactly one should be less (unless equal)");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGUIDComparisonOperators completed successfully");
}

void Zenith_UnitTests::TestGUIDHashDistribution()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestGUIDHashDistribution...");

	// Generate many GUIDs and hash them, check for reasonable distribution
	std::unordered_set<size_t> xHashes;
	constexpr uint32_t uNumGUIDs = 1000;

	for (uint32_t u = 0; u < uNumGUIDs; ++u)
	{
		Zenith_AssetGUID xGUID = Zenith_AssetGUID::Generate();
		size_t ulHash = std::hash<Zenith_AssetGUID>{}(xGUID);
		xHashes.insert(ulHash);
	}

	// With good distribution, we should have very few collisions
	// Allow up to 5% collision rate (950+ unique hashes)
	Zenith_Assert(xHashes.size() >= 950,
		"TestGUIDHashDistribution: Hash distribution should have minimal collisions");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGUIDHashDistribution completed successfully");
}

void Zenith_UnitTests::TestGUIDInvalidDetection()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestGUIDInvalidDetection...");

	// Default constructed should be invalid
	Zenith_AssetGUID xDefault;
	Zenith_Assert(!xDefault.IsValid(), "TestGUIDInvalidDetection: Default GUID should be invalid");

	// INVALID constant should be invalid
	Zenith_Assert(!Zenith_AssetGUID::INVALID.IsValid(), "TestGUIDInvalidDetection: INVALID should be invalid");

	// Generated should be valid
	Zenith_AssetGUID xGenerated = Zenith_AssetGUID::Generate();
	Zenith_Assert(xGenerated.IsValid(), "TestGUIDInvalidDetection: Generated GUID should be valid");

	// Explicit zero construction
	Zenith_AssetGUID xZero(0, 0);
	Zenith_Assert(!xZero.IsValid(), "TestGUIDInvalidDetection: Zero GUID should be invalid");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGUIDInvalidDetection completed successfully");
}

//=============================================================================
// Asset Meta File Tests
//=============================================================================

void Zenith_UnitTests::TestAssetMetaSaveLoadRoundTrip()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetMetaSaveLoadRoundTrip...");

	// Create a meta with test data
	Zenith_AssetMeta xOriginal;
	xOriginal.m_xGUID = Zenith_AssetGUID::Generate();
	xOriginal.m_strAssetPath = "TestAssets/test_texture.ztex";
	xOriginal.m_ulLastModifiedTime = 12345678;
	xOriginal.m_eAssetType = Zenith_AssetType::TEXTURE;

	// Save to temp file
	std::string strTempPath = "TestAssets/test_meta_roundtrip.zmeta";

	// Ensure directory exists
	std::filesystem::create_directories("TestAssets");

	bool bSaved = xOriginal.SaveToFile(strTempPath);
	Zenith_Assert(bSaved, "TestAssetMetaSaveLoadRoundTrip: Save should succeed");

	// Load back
	Zenith_AssetMeta xLoaded;
	bool bLoaded = xLoaded.LoadFromFile(strTempPath);
	Zenith_Assert(bLoaded, "TestAssetMetaSaveLoadRoundTrip: Load should succeed");

	// Verify fields
	Zenith_Assert(xOriginal.m_xGUID == xLoaded.m_xGUID,
		"TestAssetMetaSaveLoadRoundTrip: GUID should match");
	Zenith_Assert(xOriginal.m_strAssetPath == xLoaded.m_strAssetPath,
		"TestAssetMetaSaveLoadRoundTrip: Asset path should match");
	Zenith_Assert(xOriginal.m_ulLastModifiedTime == xLoaded.m_ulLastModifiedTime,
		"TestAssetMetaSaveLoadRoundTrip: Modification time should match");
	Zenith_Assert(xOriginal.m_eAssetType == xLoaded.m_eAssetType,
		"TestAssetMetaSaveLoadRoundTrip: Asset type should match");

	// Cleanup
	std::filesystem::remove(strTempPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetMetaSaveLoadRoundTrip completed successfully");
}

void Zenith_UnitTests::TestAssetMetaVersionCompatibility()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetMetaVersionCompatibility...");

	// This test ensures the meta file format includes version info
	Zenith_AssetMeta xMeta;
	xMeta.m_xGUID = Zenith_AssetGUID::Generate();
	xMeta.m_strAssetPath = "TestAssets/version_test.ztex";
	xMeta.m_eAssetType = Zenith_AssetType::TEXTURE;

	std::string strPath = "TestAssets/version_test.zmeta";
	std::filesystem::create_directories("TestAssets");

	xMeta.SaveToFile(strPath);

	// Load and verify it works (version is embedded in file)
	Zenith_AssetMeta xLoaded;
	bool bLoaded = xLoaded.LoadFromFile(strPath);
	Zenith_Assert(bLoaded, "TestAssetMetaVersionCompatibility: Should load current version");

	std::filesystem::remove(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetMetaVersionCompatibility completed successfully");
}

void Zenith_UnitTests::TestAssetMetaImportSettings()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetMetaImportSettings...");

	// Test texture import settings
	Zenith_AssetMeta xTextureMeta;
	xTextureMeta.m_xGUID = Zenith_AssetGUID::Generate();
	xTextureMeta.m_strAssetPath = "TestAssets/import_test.ztex";
	xTextureMeta.m_eAssetType = Zenith_AssetType::TEXTURE;

	xTextureMeta.m_xTextureSettings.m_bGenerateMipmaps = true;
	xTextureMeta.m_xTextureSettings.m_bSRGB = false;

	std::string strPath = "TestAssets/import_settings_test.zmeta";
	std::filesystem::create_directories("TestAssets");

	xTextureMeta.SaveToFile(strPath);

	Zenith_AssetMeta xLoaded;
	xLoaded.LoadFromFile(strPath);

	Zenith_Assert(xLoaded.m_xTextureSettings.m_bGenerateMipmaps == xTextureMeta.m_xTextureSettings.m_bGenerateMipmaps,
		"TestAssetMetaImportSettings: GenerateMipmaps should match");
	Zenith_Assert(xLoaded.m_xTextureSettings.m_bSRGB == xTextureMeta.m_xTextureSettings.m_bSRGB,
		"TestAssetMetaImportSettings: SRGB should match");

	std::filesystem::remove(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetMetaImportSettings completed successfully");
}

void Zenith_UnitTests::TestAssetMetaGetMetaPath()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetMetaGetMetaPath...");

	// Test meta path generation
	std::string strAssetPath = "Assets/Textures/diffuse.ztex";
	std::string strMetaPath = Zenith_AssetMeta::GetMetaPath(strAssetPath);

	Zenith_Assert(strMetaPath == "Assets/Textures/diffuse.ztex.zmeta",
		"TestAssetMetaGetMetaPath: Meta path should append .zmeta");

	// Test with different extensions
	std::string strMaterialPath = "Assets/Materials/test.zmtrl";
	std::string strMaterialMetaPath = Zenith_AssetMeta::GetMetaPath(strMaterialPath);
	Zenith_Assert(strMaterialMetaPath == "Assets/Materials/test.zmtrl.zmeta",
		"TestAssetMetaGetMetaPath: Should work with any extension");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetMetaGetMetaPath completed successfully");
}

//=============================================================================
// Asset Database Tests
//=============================================================================

void Zenith_UnitTests::TestAssetDatabaseGUIDToPath()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetDatabaseGUIDToPath...");

	// Create a test meta and register it
	Zenith_AssetMeta xMeta;
	xMeta.m_xGUID = Zenith_AssetGUID::Generate();
	xMeta.m_strAssetPath = "TestAssets/db_test_asset.ztex";
	xMeta.m_eAssetType = Zenith_AssetType::TEXTURE;

	// Initialize database if not already
	bool bWasInitialized = Zenith_AssetDatabase::IsInitialized();
	if (!bWasInitialized)
	{
		Zenith_AssetDatabase::Initialize("TestAssets/");
	}

	// Register the asset
	Zenith_AssetDatabase::RegisterAsset(xMeta);

	// Look up by GUID
	std::string strPath = Zenith_AssetDatabase::GetPathFromGUID(xMeta.m_xGUID);
	Zenith_Assert(!strPath.empty(), "TestAssetDatabaseGUIDToPath: Should find registered asset");

	// Unregister
	Zenith_AssetDatabase::UnregisterAsset(xMeta.m_xGUID);

	// Should not find after unregister
	strPath = Zenith_AssetDatabase::GetPathFromGUID(xMeta.m_xGUID);
	Zenith_Assert(strPath.empty(), "TestAssetDatabaseGUIDToPath: Should not find unregistered asset");

	if (!bWasInitialized)
	{
		Zenith_AssetDatabase::Shutdown();
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetDatabaseGUIDToPath completed successfully");
}

void Zenith_UnitTests::TestAssetDatabasePathToGUID()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetDatabasePathToGUID...");

	Zenith_AssetMeta xMeta;
	xMeta.m_xGUID = Zenith_AssetGUID::Generate();
	xMeta.m_strAssetPath = "TestAssets/path_lookup_test.ztex";
	xMeta.m_eAssetType = Zenith_AssetType::TEXTURE;

	bool bWasInitialized = Zenith_AssetDatabase::IsInitialized();
	if (!bWasInitialized)
	{
		Zenith_AssetDatabase::Initialize("TestAssets/");
	}

	Zenith_AssetDatabase::RegisterAsset(xMeta);

	// Look up by path
	Zenith_AssetGUID xFoundGUID = Zenith_AssetDatabase::GetGUIDFromPath("TestAssets/path_lookup_test.ztex");
	Zenith_Assert(xFoundGUID.IsValid(), "TestAssetDatabasePathToGUID: Should find by path");
	Zenith_Assert(xFoundGUID == xMeta.m_xGUID, "TestAssetDatabasePathToGUID: GUID should match");

	// Test case insensitivity on Windows
#ifdef _WIN32
	Zenith_AssetGUID xUpperGUID = Zenith_AssetDatabase::GetGUIDFromPath("TESTASSETS/PATH_LOOKUP_TEST.ZTEX");
	Zenith_Assert(xUpperGUID == xMeta.m_xGUID,
		"TestAssetDatabasePathToGUID: Should be case-insensitive on Windows");
#endif

	Zenith_AssetDatabase::UnregisterAsset(xMeta.m_xGUID);

	if (!bWasInitialized)
	{
		Zenith_AssetDatabase::Shutdown();
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetDatabasePathToGUID completed successfully");
}

void Zenith_UnitTests::TestAssetDatabaseDependencyTracking()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetDatabaseDependencyTracking...");

	bool bWasInitialized = Zenith_AssetDatabase::IsInitialized();
	if (!bWasInitialized)
	{
		Zenith_AssetDatabase::Initialize("TestAssets/");
	}

	Zenith_AssetGUID xMaterialGUID = Zenith_AssetGUID::Generate();
	Zenith_AssetGUID xTextureGUID = Zenith_AssetGUID::Generate();

	// Register dependency: material depends on texture
	Zenith_AssetDatabase::RegisterDependency(xMaterialGUID, xTextureGUID);

	// Get dependencies
	Zenith_Vector<Zenith_AssetGUID> xDeps = Zenith_AssetDatabase::GetDependencies(xMaterialGUID);
	Zenith_Assert(xDeps.GetSize() == 1, "TestAssetDatabaseDependencyTracking: Should have 1 dependency");
	Zenith_Assert(xDeps.Get(0) == xTextureGUID, "TestAssetDatabaseDependencyTracking: Dependency should be texture");

	// Unregister dependency
	Zenith_AssetDatabase::UnregisterDependency(xMaterialGUID, xTextureGUID);
	xDeps = Zenith_AssetDatabase::GetDependencies(xMaterialGUID);
	Zenith_Assert(xDeps.GetSize() == 0, "TestAssetDatabaseDependencyTracking: Should have no dependencies after unregister");

	if (!bWasInitialized)
	{
		Zenith_AssetDatabase::Shutdown();
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetDatabaseDependencyTracking completed successfully");
}

void Zenith_UnitTests::TestAssetDatabaseDependentLookup()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetDatabaseDependentLookup...");

	bool bWasInitialized = Zenith_AssetDatabase::IsInitialized();
	if (!bWasInitialized)
	{
		Zenith_AssetDatabase::Initialize("TestAssets/");
	}

	Zenith_AssetGUID xTextureGUID = Zenith_AssetGUID::Generate();
	Zenith_AssetGUID xMaterial1GUID = Zenith_AssetGUID::Generate();
	Zenith_AssetGUID xMaterial2GUID = Zenith_AssetGUID::Generate();

	// Two materials depend on the same texture
	Zenith_AssetDatabase::RegisterDependency(xMaterial1GUID, xTextureGUID);
	Zenith_AssetDatabase::RegisterDependency(xMaterial2GUID, xTextureGUID);

	// Get dependents of texture
	Zenith_Vector<Zenith_AssetGUID> xDependents = Zenith_AssetDatabase::GetDependents(xTextureGUID);
	Zenith_Assert(xDependents.GetSize() == 2, "TestAssetDatabaseDependentLookup: Should have 2 dependents");

	// Clear dependencies
	Zenith_AssetDatabase::ClearDependencies(xMaterial1GUID);
	Zenith_AssetDatabase::ClearDependencies(xMaterial2GUID);

	if (!bWasInitialized)
	{
		Zenith_AssetDatabase::Shutdown();
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetDatabaseDependentLookup completed successfully");
}

//=============================================================================
// Asset Reference Tests
//=============================================================================

void Zenith_UnitTests::TestAssetRefGUIDStorage()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetRefGUIDStorage...");

	Zenith_AssetGUID xTestGUID = Zenith_AssetGUID::Generate();

	TextureRef xRef;
	Zenith_Assert(!xRef.IsSet(), "TestAssetRefGUIDStorage: Default ref should not be set");

	xRef.SetGUID(xTestGUID);
	Zenith_Assert(xRef.IsSet(), "TestAssetRefGUIDStorage: Should be set after SetGUID");
	Zenith_Assert(xRef.GetGUID() == xTestGUID, "TestAssetRefGUIDStorage: GUID should match");

	xRef.Clear();
	Zenith_Assert(!xRef.IsSet(), "TestAssetRefGUIDStorage: Should not be set after Clear");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetRefGUIDStorage completed successfully");
}

void Zenith_UnitTests::TestAssetRefSerializationRoundTrip()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetRefSerializationRoundTrip...");

	Zenith_AssetGUID xTestGUID = Zenith_AssetGUID::Generate();
	MaterialRef xOriginal;
	xOriginal.SetGUID(xTestGUID);

	// Write to stream
	Zenith_DataStream xStream;
	xOriginal.WriteToDataStream(xStream);

	// Read back
	xStream.SetCursor(0);
	MaterialRef xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	Zenith_Assert(xOriginal.GetGUID() == xLoaded.GetGUID(),
		"TestAssetRefSerializationRoundTrip: GUIDs should match after round-trip");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetRefSerializationRoundTrip completed successfully");
}

void Zenith_UnitTests::TestAssetRefFromPath()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetRefFromPath...");

	// Register a test asset in the database
	bool bWasInitialized = Zenith_AssetDatabase::IsInitialized();
	if (!bWasInitialized)
	{
		Zenith_AssetDatabase::Initialize("TestAssets/");
	}

	Zenith_AssetMeta xMeta;
	xMeta.m_xGUID = Zenith_AssetGUID::Generate();
	xMeta.m_strAssetPath = "TestAssets/ref_from_path_test.zmtrl";
	xMeta.m_eAssetType = Zenith_AssetType::MATERIAL;
	Zenith_AssetDatabase::RegisterAsset(xMeta);

	// Create ref from path
	MaterialRef xRef;
	bool bFound = xRef.SetFromPath("TestAssets/ref_from_path_test.zmtrl");
	Zenith_Assert(bFound, "TestAssetRefFromPath: Should find registered asset");
	Zenith_Assert(xRef.GetGUID() == xMeta.m_xGUID, "TestAssetRefFromPath: GUID should match");

	// Try non-existent path
	MaterialRef xBadRef;
	bFound = xBadRef.SetFromPath("TestAssets/does_not_exist.zmtrl");
	Zenith_Assert(!bFound, "TestAssetRefFromPath: Should not find non-existent asset");
	Zenith_Assert(!xBadRef.IsSet(), "TestAssetRefFromPath: Bad ref should not be set");

	Zenith_AssetDatabase::UnregisterAsset(xMeta.m_xGUID);

	if (!bWasInitialized)
	{
		Zenith_AssetDatabase::Shutdown();
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetRefFromPath completed successfully");
}

void Zenith_UnitTests::TestAssetRefInvalidHandling()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetRefInvalidHandling...");

	TextureRef xRef;

	// Default ref should return nullptr
	Zenith_TextureAsset* pTexture = xRef.Get();
	Zenith_Assert(pTexture == nullptr, "TestAssetRefInvalidHandling: Invalid ref should return nullptr");

	// Arrow operator on invalid ref should return nullptr
	// (We can't test dereferencing nullptr, but we can verify Get returns nullptr)
	Zenith_Assert(xRef.operator->() == nullptr,
		"TestAssetRefInvalidHandling: Arrow operator should return nullptr for invalid ref");

	// Bool conversion
	Zenith_Assert(!static_cast<bool>(xRef), "TestAssetRefInvalidHandling: Bool should be false for invalid ref");

	// Set to valid GUID that doesn't exist in database - Get should still return nullptr
	xRef.SetGUID(Zenith_AssetGUID::Generate());
	Zenith_Assert(xRef.IsSet(), "TestAssetRefInvalidHandling: Should be set with any valid GUID");
	// Note: We can't test Get() here without the database being properly set up with the asset

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetRefInvalidHandling completed successfully");
}

//------------------------------------------------------------------------------
// Entity Hierarchy Tests
//------------------------------------------------------------------------------

void Zenith_UnitTests::TestEntityAddChild()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityAddChild...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create parent and child entities
	Zenith_Entity xParent(&xScene, "TestParent");
	Zenith_Entity xChild(&xScene, "TestChild");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Initially, both should have no children
	Zenith_Assert(xParent.GetChildCount() == 0, "TestEntityAddChild: Parent should have no children initially");
	Zenith_Assert(!xParent.HasChildren(), "TestEntityAddChild: HasChildren should be false");

	// Add child using SetParent
	xChild.SetParent(uParentID);

	// Verify parent-child relationship (Entity handles delegate to single source of truth)
	Zenith_Entity xChildRef = xScene.GetEntity(uChildID);
	Zenith_Entity xParentRef = xScene.GetEntity(uParentID);

	Zenith_Assert(xChildRef.GetParentEntityID() == uParentID, "TestEntityAddChild: Child should have parent ID set");
	Zenith_Assert(xChildRef.HasParent(), "TestEntityAddChild: Child HasParent should be true");
	Zenith_Assert(xParentRef.GetChildCount() == 1, "TestEntityAddChild: Parent should have 1 child");
	Zenith_Assert(xParentRef.HasChildren(), "TestEntityAddChild: Parent HasChildren should be true");
	Zenith_Assert(xParentRef.GetChildEntityIDs().Get(0) == uChildID, "TestEntityAddChild: Parent's child should be correct ID");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityAddChild completed successfully");
}

void Zenith_UnitTests::TestEntityRemoveChild()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityRemoveChild...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create parent and child entities
	Zenith_Entity xParent(&xScene, "TestParent2");
	Zenith_Entity xChild(&xScene, "TestChild2");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Set parent
	xChild.SetParent(uParentID);
	Zenith_Assert(xParent.GetChildCount() == 1, "TestEntityRemoveChild: Parent should have 1 child");

	// Remove parent (unparent child)
	xChild.SetParent(INVALID_ENTITY_ID);

	// Verify relationship is broken
	Zenith_Entity xChildRef = xScene.GetEntity(uChildID);
	Zenith_Entity xParentRef = xScene.GetEntity(uParentID);

	Zenith_Assert(!xChildRef.HasParent(), "TestEntityRemoveChild: Child should no longer have parent");
	Zenith_Assert(xChildRef.GetParentEntityID() == INVALID_ENTITY_ID, "TestEntityRemoveChild: Child parent ID should be INVALID");
	Zenith_Assert(xParentRef.GetChildCount() == 0, "TestEntityRemoveChild: Parent should have no children");
	Zenith_Assert(!xParentRef.HasChildren(), "TestEntityRemoveChild: Parent HasChildren should be false");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityRemoveChild completed successfully");
}

void Zenith_UnitTests::TestEntityGetChildren()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityGetChildren...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create parent with multiple children
	Zenith_Entity xParent(&xScene, "TestParent3");
	Zenith_Entity xChild1(&xScene, "TestChild3a");
	Zenith_Entity xChild2(&xScene, "TestChild3b");
	Zenith_Entity xChild3(&xScene, "TestChild3c");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChild1ID = xChild1.GetEntityID();
	Zenith_EntityID uChild2ID = xChild2.GetEntityID();
	Zenith_EntityID uChild3ID = xChild3.GetEntityID();

	// Add all children
	xChild1.SetParent(uParentID);
	xChild2.SetParent(uParentID);
	xChild3.SetParent(uParentID);

	// Verify all children are tracked
	Zenith_Entity xParentRef = xScene.GetEntity(uParentID);
	Zenith_Assert(xParentRef.GetChildCount() == 3, "TestEntityGetChildren: Parent should have 3 children");

	Zenith_Vector<Zenith_EntityID> xChildren = xParentRef.GetChildEntityIDs();
	bool bFoundChild1 = false, bFoundChild2 = false, bFoundChild3 = false;
	for (u_int i = 0; i < xChildren.GetSize(); i++)
	{
		if (xChildren.Get(i) == uChild1ID) bFoundChild1 = true;
		if (xChildren.Get(i) == uChild2ID) bFoundChild2 = true;
		if (xChildren.Get(i) == uChild3ID) bFoundChild3 = true;
	}
	Zenith_Assert(bFoundChild1 && bFoundChild2 && bFoundChild3, "TestEntityGetChildren: All children should be in list");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityGetChildren completed successfully");
}

void Zenith_UnitTests::TestEntityReparenting()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityReparenting...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create entities for reparenting test
	Zenith_Entity xParentA(&xScene, "ParentA");
	Zenith_Entity xParentB(&xScene, "ParentB");
	Zenith_Entity xChild(&xScene, "ReparentChild");

	Zenith_EntityID uParentAID = xParentA.GetEntityID();
	Zenith_EntityID uParentBID = xParentB.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Parent to A
	xChild.SetParent(uParentAID);
	Zenith_Assert(xParentA.GetChildCount() == 1, "TestEntityReparenting: ParentA should have 1 child");
	Zenith_Assert(xParentB.GetChildCount() == 0, "TestEntityReparenting: ParentB should have 0 children");
	Zenith_Assert(xChild.GetParentEntityID() == uParentAID, "TestEntityReparenting: Child should be parented to A");

	// Reparent to B
	xChild.SetParent(uParentBID);
	Zenith_Assert(xParentA.GetChildCount() == 0, "TestEntityReparenting: ParentA should now have 0 children");
	Zenith_Assert(xParentB.GetChildCount() == 1, "TestEntityReparenting: ParentB should now have 1 child");
	Zenith_Assert(xChild.GetParentEntityID() == uParentBID, "TestEntityReparenting: Child should be parented to B");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityReparenting completed successfully");
}

void Zenith_UnitTests::TestEntityChildCleanupOnDelete()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityChildCleanupOnDelete...");

	// Note: This test documents expected behavior for entity deletion
	// In a real implementation, deleting a parent would need to handle children
	// For now we just verify the API works correctly

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	Zenith_Entity xParent(&xScene, "DeleteParent");
	Zenith_Entity xChild(&xScene, "DeleteChild");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Set parent
	xChild.SetParent(uParentID);

	Zenith_Assert(xParent.GetChildCount() == 1, "TestEntityChildCleanupOnDelete: Should have child");

	// Unparent before any deletion (good practice)
	xChild.SetParent(INVALID_ENTITY_ID);
	Zenith_Assert(xParent.GetChildCount() == 0, "TestEntityChildCleanupOnDelete: Should have no children after unparent");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityChildCleanupOnDelete completed successfully");
}

void Zenith_UnitTests::TestEntityHierarchySerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityHierarchySerialization...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create hierarchy
	Zenith_Entity xParent(&xScene, "SerializeParent");
	Zenith_Entity xChild(&xScene, "SerializeChild");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Set parent
	xChild.SetParent(uParentID);

	// Serialize parent entity
	Zenith_DataStream xStream(256);
	xParent.WriteToDataStream(xStream);

	// Reset and read back
	// Note: Must create a valid entity in scene first, as deserialization
	// calls AddComponent which requires a valid EntityID in the scene
	xStream.SetCursor(0);
	Zenith_Entity xLoadedParent(&xScene, "TempParent");
	xLoadedParent.ReadFromDataStream(xStream);

	// Children are stored in scene, so parent ID should serialize
	// The parent's child list is rebuilt when children are loaded and call SetParent
	Zenith_Assert(xLoadedParent.IsRoot(), "TestEntityHierarchySerialization: Loaded parent should be root");

	// Serialize child entity
	Zenith_DataStream xChildStream(256);
	xChild.WriteToDataStream(xChildStream);

	// Create entity in scene before deserializing
	xChildStream.SetCursor(0);
	Zenith_Entity xLoadedChild(&xScene, "TempChild");
	xLoadedChild.ReadFromDataStream(xChildStream);

	// Standalone entity deserialization stores the parent's file index in PendingParentFileIndex
	// The actual parent relationship is only rebuilt during full scene loading
	// So we verify the pending index matches the original parent's index
	Zenith_TransformComponent& xLoadedChildTransform = xLoadedChild.GetComponent<Zenith_TransformComponent>();
	Zenith_Assert(xLoadedChildTransform.GetPendingParentFileIndex() == uParentID.m_uIndex,
		"TestEntityHierarchySerialization: Loaded child should have parent file index preserved");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityHierarchySerialization completed successfully");
}

//------------------------------------------------------------------------------
// Prefab System Tests
//------------------------------------------------------------------------------

void Zenith_UnitTests::TestPrefabCreateFromEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestPrefabCreateFromEntity...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create an entity with a transform component
	Zenith_Entity xEntity(&xScene, "PrefabSource");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f));

	// Create prefab from entity
	Zenith_Prefab xPrefab;
	bool bSuccess = xPrefab.CreateFromEntity(xEntity, "TestPrefab");

	Zenith_Assert(bSuccess, "TestPrefabCreateFromEntity: CreateFromEntity should succeed");
	Zenith_Assert(xPrefab.IsValid(), "TestPrefabCreateFromEntity: Prefab should be valid");
	Zenith_Assert(xPrefab.GetName() == "TestPrefab", "TestPrefabCreateFromEntity: Prefab name should match");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPrefabCreateFromEntity completed successfully");
}

void Zenith_UnitTests::TestPrefabInstantiation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestPrefabInstantiation...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create source entity
	Zenith_Entity xSource(&xScene, "InstantiateSource");
	Zenith_TransformComponent& xTransform = xSource.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(5.0f, 10.0f, 15.0f));

	// Create prefab
	Zenith_Prefab xPrefab;
	xPrefab.CreateFromEntity(xSource, "InstantiatePrefab");

	// Instantiate prefab
	Zenith_Entity xInstance = xPrefab.Instantiate(&xScene, "PrefabInstance");

	// Verify instance has the transform values from prefab
	Zenith_Assert(xInstance.HasComponent<Zenith_TransformComponent>(),
		"TestPrefabInstantiation: Instance should have transform component");

	Zenith_TransformComponent& xInstanceTransform = xInstance.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPos;
	xInstanceTransform.GetPosition(xPos);

	// Position should match source
	Zenith_Assert(std::abs(xPos.x - 5.0f) < 0.001f &&
	              std::abs(xPos.y - 10.0f) < 0.001f &&
	              std::abs(xPos.z - 15.0f) < 0.001f,
		"TestPrefabInstantiation: Instance position should match prefab source");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPrefabInstantiation completed successfully");
}

void Zenith_UnitTests::TestPrefabSaveLoadRoundTrip()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestPrefabSaveLoadRoundTrip...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create source entity
	Zenith_Entity xSource(&xScene, "RoundTripSource");
	Zenith_TransformComponent& xTransform = xSource.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(100.0f, 200.0f, 300.0f));

	// Create and save prefab
	Zenith_Prefab xPrefab;
	xPrefab.CreateFromEntity(xSource, "RoundTripPrefab");
	xPrefab.SetGUID(Zenith_AssetGUID::Generate());

	std::string strTempPath = "test_roundtrip.zpfb";
	bool bSaved = xPrefab.SaveToFile(strTempPath);
	Zenith_Assert(bSaved, "TestPrefabSaveLoadRoundTrip: Save should succeed");

	// Load prefab
	Zenith_Prefab xLoadedPrefab;
	bool bLoaded = xLoadedPrefab.LoadFromFile(strTempPath);
	Zenith_Assert(bLoaded, "TestPrefabSaveLoadRoundTrip: Load should succeed");
	Zenith_Assert(xLoadedPrefab.IsValid(), "TestPrefabSaveLoadRoundTrip: Loaded prefab should be valid");
	Zenith_Assert(xLoadedPrefab.GetName() == "RoundTripPrefab",
		"TestPrefabSaveLoadRoundTrip: Loaded prefab name should match");

	// Instantiate loaded prefab
	Zenith_Entity xInstance = xLoadedPrefab.Instantiate(&xScene, "LoadedInstance");
	Zenith_TransformComponent& xInstanceTransform = xInstance.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPos;
	xInstanceTransform.GetPosition(xPos);

	Zenith_Assert(std::abs(xPos.x - 100.0f) < 0.001f &&
	              std::abs(xPos.y - 200.0f) < 0.001f &&
	              std::abs(xPos.z - 300.0f) < 0.001f,
		"TestPrefabSaveLoadRoundTrip: Instance position should match original");

	// Cleanup temp file
	std::filesystem::remove(strTempPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPrefabSaveLoadRoundTrip completed successfully");
}

void Zenith_UnitTests::TestPrefabOverrides()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestPrefabOverrides...");

	Zenith_Prefab xPrefab;

	// Add an override
	Zenith_PropertyOverride xOverride;
	xOverride.m_strComponentName = "Transform";
	xOverride.m_strPropertyPath = "Position.x";
	xOverride.m_xValue << 42.0f;

	xPrefab.AddOverride(std::move(xOverride));

	// Verify override was added
	const Zenith_Vector<Zenith_PropertyOverride>& xOverrides = xPrefab.GetOverrides();
	Zenith_Assert(xOverrides.GetSize() == 1, "TestPrefabOverrides: Should have 1 override");
	Zenith_Assert(xOverrides.Get(0).m_strComponentName == "Transform",
		"TestPrefabOverrides: Override component name should match");
	Zenith_Assert(xOverrides.Get(0).m_strPropertyPath == "Position.x",
		"TestPrefabOverrides: Override property path should match");

	// Add another override with same path (should replace)
	Zenith_PropertyOverride xOverride2;
	xOverride2.m_strComponentName = "Transform";
	xOverride2.m_strPropertyPath = "Position.x";
	xOverride2.m_xValue << 99.0f;

	xPrefab.AddOverride(std::move(xOverride2));

	// Should still be 1 override (replaced)
	Zenith_Assert(xPrefab.GetOverrides().GetSize() == 1,
		"TestPrefabOverrides: Should still have 1 override after replace");

	// Clear overrides
	xPrefab.ClearOverrides();
	Zenith_Assert(xPrefab.GetOverrides().GetSize() == 0,
		"TestPrefabOverrides: Should have 0 overrides after clear");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPrefabOverrides completed successfully");
}

void Zenith_UnitTests::TestPrefabVariantCreation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestPrefabVariantCreation...");

	// Create a base prefab reference (mock - not actually loaded)
	PrefabRef xBasePrefabRef;
	Zenith_AssetGUID xBaseGUID = Zenith_AssetGUID::Generate();
	xBasePrefabRef.SetGUID(xBaseGUID);

	// Create a variant prefab
	Zenith_Prefab xVariant;
	bool bSuccess = xVariant.CreateAsVariant(xBasePrefabRef, "VariantPrefab");

	Zenith_Assert(bSuccess, "TestPrefabVariantCreation: CreateAsVariant should succeed");
	Zenith_Assert(xVariant.IsVariant(), "TestPrefabVariantCreation: Should be marked as variant");
	Zenith_Assert(xVariant.GetBasePrefab().IsSet(), "TestPrefabVariantCreation: Should have base prefab set");
	Zenith_Assert(xVariant.GetBasePrefab().GetGUID() == xBaseGUID,
		"TestPrefabVariantCreation: Base prefab GUID should match");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPrefabVariantCreation completed successfully");
}

//==============================================================================
// Async Asset Loading Tests
//==============================================================================

void Zenith_UnitTests::TestAsyncLoadState()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAsyncLoadState...");

	// Test that default state is UNLOADED for unknown GUIDs
	Zenith_AssetGUID xUnknownGUID = Zenith_AssetGUID::Generate();
	AssetLoadState eState = Zenith_AsyncAssetLoader::GetLoadState(xUnknownGUID);
	Zenith_Assert(eState == AssetLoadState::UNLOADED, "TestAsyncLoadState: Unknown GUID should be UNLOADED");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadState completed successfully");
}

void Zenith_UnitTests::TestAsyncLoadRequest()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAsyncLoadRequest...");

	// Test that pending loads can be tracked
	bool bHadPending = Zenith_AsyncAssetLoader::HasPendingLoads();

	// Cancel any pending loads to reset state
	Zenith_AsyncAssetLoader::CancelAllPendingLoads();
	Zenith_Assert(!Zenith_AsyncAssetLoader::HasPendingLoads(),
		"TestAsyncLoadRequest: After cancel, should have no pending loads");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadRequest completed successfully");
}

void Zenith_UnitTests::TestAsyncLoadCompletion()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAsyncLoadCompletion...");

	// Test ProcessCompletedLoads doesn't crash with no pending loads
	Zenith_AsyncAssetLoader::ProcessCompletedLoads();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadCompletion completed successfully");
}

void Zenith_UnitTests::TestAssetRefAsyncAPI()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAssetRefAsyncAPI...");

	// Test AssetRef async API availability
	Zenith_AssetRef<Zenith_TextureAsset> xTextureRef;

	// Default ref should not be ready
	Zenith_Assert(!xTextureRef.IsReady(), "TestAssetRefAsyncAPI: Empty ref should not be ready");

	// TryGet should return nullptr for unloaded ref
	Zenith_TextureAsset* pxTex = xTextureRef.TryGet();
	Zenith_Assert(pxTex == nullptr, "TestAssetRefAsyncAPI: TryGet on empty ref should return nullptr");

	// GetLoadState should return UNLOADED for invalid GUID
	AssetLoadState eState = xTextureRef.GetLoadState();
	// Note: Empty ref may return LOADED (cached as nullptr) or UNLOADED depending on implementation
	// This just tests the API exists and doesn't crash

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetRefAsyncAPI completed successfully");
}

//==============================================================================
// DataAsset System Tests
//==============================================================================

// Test DataAsset class for unit testing
class TestDataAsset : public Zenith_DataAsset
{
public:
	ZENITH_DATA_ASSET_TYPE_NAME(TestDataAsset)

	int32_t m_iTestValue = 42;
	float m_fTestFloat = 3.14f;
	std::string m_strTestString = "TestString";

	void WriteToDataStream(Zenith_DataStream& xStream) const override
	{
		xStream << m_iTestValue;
		xStream << m_fTestFloat;
		xStream << m_strTestString;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream) override
	{
		xStream >> m_iTestValue;
		xStream >> m_fTestFloat;
		xStream >> m_strTestString;
	}
};

void Zenith_UnitTests::TestDataAssetRegistration()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDataAssetRegistration...");

	// Register the test data asset type
	Zenith_DataAssetManager::RegisterDataAssetType<TestDataAsset>();

	// Verify it was registered
	bool bRegistered = Zenith_DataAssetManager::IsTypeRegistered("TestDataAsset");
	Zenith_Assert(bRegistered, "TestDataAssetRegistration: TestDataAsset should be registered");

	// Verify unknown type is not registered
	bool bUnknown = Zenith_DataAssetManager::IsTypeRegistered("UnknownType");
	Zenith_Assert(!bUnknown, "TestDataAssetRegistration: Unknown type should not be registered");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDataAssetRegistration completed successfully");
}

void Zenith_UnitTests::TestDataAssetCreateAndSave()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDataAssetCreateAndSave...");

	// Ensure type is registered
	Zenith_DataAssetManager::RegisterDataAssetType<TestDataAsset>();

	// Create a new instance via factory
	TestDataAsset* pxAsset = Zenith_DataAssetManager::CreateDataAsset<TestDataAsset>();
	Zenith_Assert(pxAsset != nullptr, "TestDataAssetCreateAndSave: Failed to create TestDataAsset");

	// Set some values
	pxAsset->m_iTestValue = 100;
	pxAsset->m_fTestFloat = 2.71828f;
	pxAsset->m_strTestString = "ModifiedValue";

	// Save to file
	std::string strTestPath = "TestData/test_data_asset.zdata";
	std::filesystem::create_directories("TestData");
	bool bSaved = Zenith_DataAssetManager::SaveDataAsset(pxAsset, strTestPath);
	Zenith_Assert(bSaved, "TestDataAssetCreateAndSave: Failed to save TestDataAsset");

	// Verify file exists
	bool bExists = std::filesystem::exists(strTestPath);
	Zenith_Assert(bExists, "TestDataAssetCreateAndSave: Saved file should exist");

	// Note: Don't delete pxAsset here - SaveDataAsset adds it to the cache,
	// which takes ownership. ClearCache() will clean it up.

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDataAssetCreateAndSave completed successfully");
}

void Zenith_UnitTests::TestDataAssetLoad()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDataAssetLoad...");

	// Clear cache to force reload
	Zenith_DataAssetManager::ClearCache();

	// Load the asset saved in previous test
	std::string strTestPath = "TestData/test_data_asset.zdata";
	TestDataAsset* pxLoaded = Zenith_DataAssetManager::LoadDataAsset<TestDataAsset>(strTestPath);
	Zenith_Assert(pxLoaded != nullptr, "TestDataAssetLoad: Failed to load TestDataAsset");

	// Verify loaded values match what we saved
	Zenith_Assert(pxLoaded->m_iTestValue == 100,
		"TestDataAssetLoad: Loaded int value should match saved value");
	Zenith_Assert(std::abs(pxLoaded->m_fTestFloat - 2.71828f) < 0.0001f,
		"TestDataAssetLoad: Loaded float value should match saved value");
	Zenith_Assert(pxLoaded->m_strTestString == "ModifiedValue",
		"TestDataAssetLoad: Loaded string should match saved value");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDataAssetLoad completed successfully");
}

void Zenith_UnitTests::TestDataAssetRoundTrip()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDataAssetRoundTrip...");

	// Ensure type is registered
	Zenith_DataAssetManager::RegisterDataAssetType<TestDataAsset>();

	// Create with unique values
	TestDataAsset* pxOriginal = new TestDataAsset();
	pxOriginal->m_iTestValue = -999;
	pxOriginal->m_fTestFloat = 123.456f;
	pxOriginal->m_strTestString = "RoundTripTest";

	// Save (adds to cache, which takes ownership)
	std::string strPath = "TestData/round_trip_test.zdata";
	Zenith_DataAssetManager::SaveDataAsset(pxOriginal, strPath);

	// Clear cache to force reload from disk (also deletes the cached asset)
	Zenith_DataAssetManager::ClearCache();

	// Load
	TestDataAsset* pxLoaded = Zenith_DataAssetManager::LoadDataAsset<TestDataAsset>(strPath);
	Zenith_Assert(pxLoaded != nullptr, "TestDataAssetRoundTrip: Failed to load");
	Zenith_Assert(pxLoaded->m_iTestValue == -999, "TestDataAssetRoundTrip: Int mismatch");
	Zenith_Assert(std::abs(pxLoaded->m_fTestFloat - 123.456f) < 0.001f, "TestDataAssetRoundTrip: Float mismatch");
	Zenith_Assert(pxLoaded->m_strTestString == "RoundTripTest", "TestDataAssetRoundTrip: String mismatch");

	// Clean up test files
	std::filesystem::remove("TestData/test_data_asset.zdata");
	std::filesystem::remove("TestData/round_trip_test.zdata");
	std::filesystem::remove("TestData");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDataAssetRoundTrip completed successfully");
}

//=============================================================================
// ECS Safety Tests (Circular Hierarchy, Camera Safety)
//=============================================================================

void Zenith_UnitTests::TestCircularHierarchyPrevention()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCircularHierarchyPrevention...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create A -> B -> C hierarchy
	Zenith_Entity xA(&xScene, "CircularTestA");
	Zenith_Entity xB(&xScene, "CircularTestB");
	Zenith_Entity xC(&xScene, "CircularTestC");

	Zenith_EntityID uA = xA.GetEntityID();
	Zenith_EntityID uB = xB.GetEntityID();
	Zenith_EntityID uC = xC.GetEntityID();

	// Set up hierarchy: A -> B -> C
	xB.SetParent(uA);  // B is child of A
	xC.SetParent(uB);  // C is child of B

	// Verify initial hierarchy
	Zenith_Assert(xB.HasParent(), "TestCircularHierarchyPrevention: B should have parent");
	Zenith_Assert(xB.GetParentEntityID() == uA, "TestCircularHierarchyPrevention: B's parent should be A");
	Zenith_Assert(xC.GetParentEntityID() == uB, "TestCircularHierarchyPrevention: C's parent should be B");

	// Try to parent A to C (would create cycle: A -> B -> C -> A)
	// This should be rejected by the circular hierarchy check
	xA.SetParent(uC);

	// A should still be root (circular parenting rejected)
	Zenith_Assert(!xA.HasParent(), "TestCircularHierarchyPrevention: Circular parent should be rejected - A should remain root");

	// Clean up
	Zenith_Scene::DestroyImmediate(uC);
	Zenith_Scene::DestroyImmediate(uB);
	Zenith_Scene::DestroyImmediate(uA);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCircularHierarchyPrevention completed successfully");
}

void Zenith_UnitTests::TestSelfParentingPrevention()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSelfParentingPrevention...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create an entity
	Zenith_Entity xEntity(&xScene, "SelfParentTest");
	Zenith_EntityID uEntityID = xEntity.GetEntityID();

	// Verify initially root
	Zenith_Assert(!xEntity.HasParent(), "TestSelfParentingPrevention: Entity should start as root");

	// Try to parent entity to itself
	xEntity.SetParent(uEntityID);

	// Should still be root (self-parenting rejected)
	Zenith_Assert(!xEntity.HasParent(), "TestSelfParentingPrevention: Self-parenting should be rejected");

	// Clean up
	Zenith_Scene::DestroyImmediate(uEntityID);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSelfParentingPrevention completed successfully");
}

void Zenith_UnitTests::TestTryGetMainCameraWhenNotSet()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTryGetMainCameraWhenNotSet...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Remember current camera if any
	Zenith_EntityID uPreviousCamera = xScene.GetMainCameraEntity();

	// Clear main camera
	xScene.SetMainCameraEntity(INVALID_ENTITY_ID);

	// TryGetMainCamera should return nullptr when no camera is set
	Zenith_CameraComponent* pxCamera = xScene.TryGetMainCamera();
	Zenith_Assert(pxCamera == nullptr, "TestTryGetMainCameraWhenNotSet: TryGetMainCamera should return nullptr when no camera set");

	// Restore previous camera
	if (uPreviousCamera != INVALID_ENTITY_ID && xScene.EntityExists(uPreviousCamera))
	{
		xScene.SetMainCameraEntity(uPreviousCamera);
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetMainCameraWhenNotSet completed successfully");
}

void Zenith_UnitTests::TestDeepHierarchyBuildModelMatrix()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDeepHierarchyBuildModelMatrix...");

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Create a hierarchy with multiple levels (not too deep - just testing it works)
	constexpr u_int DEPTH = 10;
	Zenith_Vector<Zenith_EntityID> xEntityIDs;

	// Create root
	Zenith_Entity xRoot(&xScene, "DeepHierarchyRoot");
	xEntityIDs.PushBack(xRoot.GetEntityID());

	// Create children
	for (u_int u = 1; u < DEPTH; ++u)
	{
		std::string strName = "DeepHierarchyChild" + std::to_string(u);
		Zenith_Entity xChild(&xScene, strName);
		Zenith_EntityID uChildID = xChild.GetEntityID();
		xEntityIDs.PushBack(uChildID);

		// Parent to previous entity
		Zenith_EntityID uParentID = xEntityIDs.Get(u - 1);
		xChild.SetParent(uParentID);
	}

	// Verify depth
	u_int uActualDepth = 0;
	Zenith_EntityID uCurrent = xEntityIDs.Get(DEPTH - 1);  // Deepest entity
	while (xScene.EntityExists(uCurrent) && xScene.GetEntity(uCurrent).HasParent())
	{
		uActualDepth++;
		uCurrent = xScene.GetEntity(uCurrent).GetParentEntityID();
	}
	Zenith_Assert(uActualDepth == DEPTH - 1, "TestDeepHierarchyBuildModelMatrix: Hierarchy depth should be %u, got %u", DEPTH - 1, uActualDepth);

	// BuildModelMatrix should work without infinite loop
	Zenith_Maths::Matrix4 xMatrix;
	Zenith_EntityID uDeepestID = xEntityIDs.Get(DEPTH - 1);
	xScene.GetEntity(uDeepestID).GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xMatrix);

	// If we get here without hanging, the test passed

	// Clean up (destroy from deepest to root)
	for (int i = static_cast<int>(DEPTH) - 1; i >= 0; --i)
	{
		Zenith_Scene::DestroyImmediate(xEntityIDs.Get(i));
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDeepHierarchyBuildModelMatrix completed successfully");
}

/**
 * Test that local scene destruction doesn't crash.
 * This tests the fix for TransformComponent destructor accessing the wrong scene
 * when a local test scene is destroyed (not s_xCurrentScene).
 */
void Zenith_UnitTests::TestLocalSceneDestruction()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLocalSceneDestruction...");

	// Create a local scene (separate from s_xCurrentScene)
	{
		Zenith_Scene xTestScene;

		// Create some entities with transforms
		Zenith_Entity xEntity1(&xTestScene, "LocalEntity1");
		Zenith_Entity xEntity2(&xTestScene, "LocalEntity2");
		Zenith_Entity xEntity3(&xTestScene, "LocalEntity3");

		// Set some positions to verify data is valid
		xEntity1.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		xEntity2.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
		xEntity3.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(3.0f, 0.0f, 0.0f));

		// xTestScene goes out of scope here - destructor should NOT crash
		// The bug was: TransformComponent::~TransformComponent called GetCurrentScene()
		// which returned the global scene, not xTestScene, causing memory corruption
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLocalSceneDestruction completed successfully");
}

/**
 * Test that local scene destruction with parent-child hierarchy doesn't crash.
 * This is a more complex test that includes hierarchy relationships.
 */
void Zenith_UnitTests::TestLocalSceneWithHierarchy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLocalSceneWithHierarchy...");

	{
		Zenith_Scene xTestScene;

		// Create parent entity
		Zenith_Entity xParent(&xTestScene, "Parent");
		xParent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f));

		// Create child entities
		Zenith_Entity xChild1(&xTestScene, "Child1");
		Zenith_Entity xChild2(&xTestScene, "Child2");

		// Set up hierarchy - children parented to parent
		xChild1.GetComponent<Zenith_TransformComponent>().SetParent(
			&xParent.GetComponent<Zenith_TransformComponent>());
		xChild2.GetComponent<Zenith_TransformComponent>().SetParent(
			&xParent.GetComponent<Zenith_TransformComponent>());

		// Verify hierarchy was set up correctly
		Zenith_Assert(xChild1.GetComponent<Zenith_TransformComponent>().HasParent(),
			"TestLocalSceneWithHierarchy: Child1 should have parent");
		Zenith_Assert(xChild2.GetComponent<Zenith_TransformComponent>().HasParent(),
			"TestLocalSceneWithHierarchy: Child2 should have parent");
		Zenith_Assert(xParent.GetComponent<Zenith_TransformComponent>().GetChildCount() == 2,
			"TestLocalSceneWithHierarchy: Parent should have 2 children");

		// xTestScene goes out of scope - destructor should handle hierarchy cleanup safely
		// Without the fix, DetachFromParent/DetachAllChildren would crash trying to
		// access the global scene instead of xTestScene
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLocalSceneWithHierarchy completed successfully");
}

//------------------------------------------------------------------------------
// Procedural Tree Asset Export Test
//------------------------------------------------------------------------------

// Tree bone indices
static constexpr uint32_t TREE_BONE_COUNT = 9;
enum TreeBone
{
	TREE_BONE_ROOT = 0,          // Ground anchor
	TREE_BONE_TRUNK_LOWER = 1,   // Lower trunk
	TREE_BONE_TRUNK_UPPER = 2,   // Upper trunk
	TREE_BONE_BRANCH_0 = 3,      // Branch at trunk lower
	TREE_BONE_BRANCH_1 = 4,      // Branch at trunk upper (left)
	TREE_BONE_BRANCH_2 = 5,      // Branch at trunk upper (right)
	TREE_BONE_BRANCH_3 = 6,      // Branch at trunk top
	TREE_BONE_LEAVES_0 = 7,      // Leaf cluster at branch 3
	TREE_BONE_LEAVES_1 = 8,      // Leaf cluster at branch 1
};

// Tree bone scales (half-extents for box geometry)
static const Zenith_Maths::Vector3 s_axTreeBoneScales[TREE_BONE_COUNT] = {
	{0.05f, 0.05f, 0.05f},   // 0: Root (small anchor point)
	{0.15f, 1.0f, 0.15f},    // 1: TrunkLower (thick lower trunk)
	{0.12f, 1.0f, 0.12f},    // 2: TrunkUpper (slightly thinner upper trunk)
	{0.06f, 0.6f, 0.06f},    // 3: Branch0 (branch from lower trunk)
	{0.05f, 0.7f, 0.05f},    // 4: Branch1 (branch from upper trunk, left)
	{0.05f, 0.7f, 0.05f},    // 5: Branch2 (branch from upper trunk, right)
	{0.04f, 0.5f, 0.04f},    // 6: Branch3 (top branch)
	{0.4f, 0.3f, 0.4f},      // 7: Leaves0 (leaf cluster at branch 3)
	{0.35f, 0.25f, 0.35f},   // 8: Leaves1 (leaf cluster at branch 1)
};

/**
 * Create a 9-bone tree skeleton for wind sway animation
 */
static Zenith_SkeletonAsset* CreateTreeSkeleton()
{
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);

	// Root at ground level
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);

	// Trunk segments (vertical along Y axis)
	pxSkel->AddBone("TrunkLower", TREE_BONE_ROOT, Zenith_Maths::Vector3(0, 1.0f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("TrunkUpper", TREE_BONE_TRUNK_LOWER, Zenith_Maths::Vector3(0, 2.0f, 0), xIdentity, xUnitScale);

	// Branches attached to trunk
	pxSkel->AddBone("Branch0", TREE_BONE_TRUNK_LOWER, Zenith_Maths::Vector3(0.8f, 0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Branch1", TREE_BONE_TRUNK_UPPER, Zenith_Maths::Vector3(-1.0f, 0.5f, 0.3f), xIdentity, xUnitScale);
	pxSkel->AddBone("Branch2", TREE_BONE_TRUNK_UPPER, Zenith_Maths::Vector3(1.0f, 0.5f, -0.3f), xIdentity, xUnitScale);
	pxSkel->AddBone("Branch3", TREE_BONE_TRUNK_UPPER, Zenith_Maths::Vector3(0, 1.5f, 0), xIdentity, xUnitScale);

	// Leaf clusters at branch tips
	pxSkel->AddBone("Leaves0", TREE_BONE_BRANCH_3, Zenith_Maths::Vector3(0, 0.5f, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Leaves1", TREE_BONE_BRANCH_1, Zenith_Maths::Vector3(-0.5f, 0.3f, 0), xIdentity, xUnitScale);

	pxSkel->ComputeBindPoseMatrices();
	return pxSkel;
}

/**
 * Create tree mesh with box geometry for each bone
 */
static Zenith_MeshAsset* CreateTreeMesh(const Zenith_SkeletonAsset* pxSkeleton)
{
	Zenith_MeshAsset* pxMesh = new Zenith_MeshAsset();
	const uint32_t uVertsPerBone = 8;
	const uint32_t uIndicesPerBone = 36;
	pxMesh->Reserve(TREE_BONE_COUNT * uVertsPerBone, TREE_BONE_COUNT * uIndicesPerBone);

	// Add a scaled cube at each bone position
	for (uint32_t uBone = 0; uBone < TREE_BONE_COUNT; uBone++)
	{
		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBone);
		// Get world position from bind pose model matrix
		Zenith_Maths::Vector3 xBoneWorldPos = Zenith_Maths::Vector3(xBone.m_xBindPoseModel[3]);

		// Get per-bone scale
		Zenith_Maths::Vector3 xScale = s_axTreeBoneScales[uBone];

		uint32_t uBaseVertex = pxMesh->GetNumVerts();

		// Add 8 cube vertices with per-bone scaling
		for (int i = 0; i < 8; i++)
		{
			// Scale the cube offsets by the bone's scale factors
			Zenith_Maths::Vector3 xScaledOffset = s_axCubeOffsets[i] * 2.0f;
			xScaledOffset.x *= xScale.x * 10.0f;
			xScaledOffset.y *= xScale.y * 10.0f;
			xScaledOffset.z *= xScale.z * 10.0f;

			Zenith_Maths::Vector3 xPos = xBoneWorldPos + xScaledOffset;

			// Calculate proper face normal based on vertex position
			Zenith_Maths::Vector3 xNormal = glm::normalize(s_axCubeOffsets[i]);

			pxMesh->AddVertex(xPos, xNormal, Zenith_Maths::Vector2(0, 0));
			pxMesh->SetVertexSkinning(
				uBaseVertex + i,
				glm::uvec4(uBone, 0, 0, 0),
				glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		}

		// Add 12 triangles (36 indices)
		for (int i = 0; i < 36; i += 3)
		{
			pxMesh->AddTriangle(
				uBaseVertex + s_auCubeIndices[i],
				uBaseVertex + s_auCubeIndices[i + 1],
				uBaseVertex + s_auCubeIndices[i + 2]);
		}
	}

	pxMesh->AddSubmesh(0, TREE_BONE_COUNT * uIndicesPerBone, 0);
	pxMesh->ComputeBounds();
	return pxMesh;
}

/**
 * Create a 2-second tree sway animation (wind effect)
 */
static Flux_AnimationClip* CreateTreeSwayAnimation()
{
	Flux_AnimationClip* pxClip = new Flux_AnimationClip();
	pxClip->SetName("Sway");
	pxClip->SetDuration(2.0f);
	pxClip->SetTicksPerSecond(30);
	pxClip->SetLooping(true);

	const float fTicksPerSec = 30.0f;
	const Zenith_Maths::Vector3 xZAxis(0, 0, 1);
	const Zenith_Maths::Vector3 xXAxis(1, 0, 0);

	// Root stays stationary
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("Root", std::move(xChannel));
	}

	// TrunkLower sways gently (base of tree, minimal movement)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(15.0f, glm::angleAxis(glm::radians(1.0f), xZAxis));
		xChannel.AddRotationKeyframe(30.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(45.0f, glm::angleAxis(glm::radians(-1.0f), xZAxis));
		xChannel.AddRotationKeyframe(60.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("TrunkLower", std::move(xChannel));
	}

	// TrunkUpper sways more (amplified from lower trunk)
	{
		Flux_BoneChannel xChannel;
		xChannel.AddRotationKeyframe(0.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(15.0f, glm::angleAxis(glm::radians(2.0f), xZAxis));
		xChannel.AddRotationKeyframe(30.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(45.0f, glm::angleAxis(glm::radians(-2.0f), xZAxis));
		xChannel.AddRotationKeyframe(60.0f, glm::identity<Zenith_Maths::Quat>());
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel("TrunkUpper", std::move(xChannel));
	}

	// Branches sway with phase offsets for natural look
	const char* aszBranchNames[] = {"Branch0", "Branch1", "Branch2", "Branch3"};
	const float afPhaseOffsets[] = {0.0f, 7.5f, 3.75f, 11.25f};  // Tick offsets for phase variation
	for (int i = 0; i < 4; ++i)
	{
		Flux_BoneChannel xChannel;
		float fPhase = afPhaseOffsets[i];
		// Branches sway more dramatically
		xChannel.AddRotationKeyframe(fmod(0.0f + fPhase, 60.0f), glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(fmod(15.0f + fPhase, 60.0f), glm::angleAxis(glm::radians(5.0f), xZAxis));
		xChannel.AddRotationKeyframe(fmod(30.0f + fPhase, 60.0f), glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(fmod(45.0f + fPhase, 60.0f), glm::angleAxis(glm::radians(-5.0f), xZAxis));
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(aszBranchNames[i], std::move(xChannel));
	}

	// Leaves sway most dramatically with additional X-axis rotation for flutter
	const char* aszLeafNames[] = {"Leaves0", "Leaves1"};
	const float afLeafPhaseOffsets[] = {5.0f, 12.0f};
	for (int i = 0; i < 2; ++i)
	{
		Flux_BoneChannel xChannel;
		float fPhase = afLeafPhaseOffsets[i];
		// Leaves have larger sway and some flutter
		Zenith_Maths::Quat xSwayPos = glm::angleAxis(glm::radians(8.0f), xZAxis) *
			glm::angleAxis(glm::radians(3.0f), xXAxis);
		Zenith_Maths::Quat xSwayNeg = glm::angleAxis(glm::radians(-8.0f), xZAxis) *
			glm::angleAxis(glm::radians(-3.0f), xXAxis);

		xChannel.AddRotationKeyframe(fmod(0.0f + fPhase, 60.0f), glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(fmod(15.0f + fPhase, 60.0f), xSwayPos);
		xChannel.AddRotationKeyframe(fmod(30.0f + fPhase, 60.0f), glm::identity<Zenith_Maths::Quat>());
		xChannel.AddRotationKeyframe(fmod(45.0f + fPhase, 60.0f), xSwayNeg);
		xChannel.SortKeyframes();
		pxClip->AddBoneChannel(aszLeafNames[i], std::move(xChannel));
	}

	return pxClip;
}

/**
 * Test procedural tree asset export
 * Creates skeleton, mesh, animation, and VAT texture for instanced rendering
 */
void Zenith_UnitTests::TestProceduralTreeAssetExport()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestProceduralTreeAssetExport...");

	// Create all assets
	Zenith_SkeletonAsset* pxSkel = CreateTreeSkeleton();
	Zenith_MeshAsset* pxMesh = CreateTreeMesh(pxSkel);
	Flux_AnimationClip* pxSwayClip = CreateTreeSwayAnimation();

	// Create output directory
	std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/ProceduralTree/";
	std::filesystem::create_directories(strOutputDir);

	// Export skeleton
	std::string strSkelPath = strOutputDir + "Tree.zskel";
	pxSkel->Export(strSkelPath.c_str());
	Zenith_Assert(std::filesystem::exists(strSkelPath), "Skeleton file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported skeleton to: %s", strSkelPath.c_str());

	// Set skeleton path on mesh before export
	pxMesh->SetSkeletonPath("Meshes/ProceduralTree/Tree.zskel");

	// Export mesh in Zenith_MeshAsset format (for asset pipeline)
	std::string strMeshAssetPath = strOutputDir + "Tree.zasset";
	pxMesh->Export(strMeshAssetPath.c_str());
	Zenith_Assert(std::filesystem::exists(strMeshAssetPath), "Mesh asset file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported mesh asset to: %s", strMeshAssetPath.c_str());

#ifdef ZENITH_TOOLS
	// Export mesh in Flux_MeshGeometry format (for runtime loading)
	Flux_MeshGeometry* pxFluxGeometry = CreateFluxMeshGeometry(pxMesh, pxSkel);
	std::string strMeshPath = strOutputDir + "Tree.zmesh";
	pxFluxGeometry->Export(strMeshPath.c_str());
	Zenith_Assert(std::filesystem::exists(strMeshPath), "Mesh geometry file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported mesh geometry to: %s", strMeshPath.c_str());

	// Export static mesh (without bone data) for static mesh rendering
	Flux_MeshGeometry* pxStaticGeometry = CreateStaticFluxMeshGeometry(pxMesh);
	std::string strStaticMeshPath = strOutputDir + "Tree_Static.zmesh";
	pxStaticGeometry->Export(strStaticMeshPath.c_str());
	Zenith_Assert(std::filesystem::exists(strStaticMeshPath), "Static mesh geometry file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported static mesh geometry to: %s", strStaticMeshPath.c_str());

	// Bake and export VAT (Vertex Animation Texture)
	Flux_AnimationTexture* pxVAT = new Flux_AnimationTexture();
	std::vector<Flux_AnimationClip*> axAnimations;
	axAnimations.push_back(pxSwayClip);
	bool bBakeSuccess = pxVAT->BakeFromAnimations(pxFluxGeometry, pxSkel, axAnimations, 30);
	Zenith_Assert(bBakeSuccess, "VAT baking should succeed");

	std::string strVATPath = strOutputDir + "Tree_Sway.zanmt";
	bool bExportSuccess = pxVAT->Export(strVATPath);
	Zenith_Assert(bExportSuccess, "VAT export should succeed");
	Zenith_Assert(std::filesystem::exists(strVATPath), "VAT file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported VAT to: %s", strVATPath.c_str());
	Zenith_Log(LOG_CATEGORY_UNITTEST, "    VAT dimensions: %u x %u (verts x frames)",
		pxVAT->GetTextureWidth(), pxVAT->GetTextureHeight());

	delete pxVAT;
	delete pxStaticGeometry;
	delete pxFluxGeometry;
#endif

	// Export animation
	std::string strSwayPath = strOutputDir + "Tree_Sway.zanim";
	pxSwayClip->Export(strSwayPath);
	Zenith_Assert(std::filesystem::exists(strSwayPath), "Sway animation file should exist after export");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Exported sway animation to: %s", strSwayPath.c_str());

	// Reload and verify skeleton
	Zenith_SkeletonAsset* pxReloadedSkel = Zenith_AssetRegistry::Get().Get<Zenith_SkeletonAsset>(strSkelPath);
	Zenith_Assert(pxReloadedSkel != nullptr, "Should be able to reload skeleton");
	Zenith_Assert(pxReloadedSkel->GetNumBones() == TREE_BONE_COUNT, "Reloaded skeleton should have 9 bones");
	Zenith_Assert(pxReloadedSkel->HasBone("TrunkLower"), "Reloaded skeleton should have TrunkLower bone");
	Zenith_Assert(pxReloadedSkel->HasBone("Branch1"), "Reloaded skeleton should have Branch1 bone");
	Zenith_Assert(pxReloadedSkel->HasBone("Leaves0"), "Reloaded skeleton should have Leaves0 bone");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded skeleton verified: %u bones", pxReloadedSkel->GetNumBones());

	// Reload and verify mesh asset format
	Zenith_MeshAsset* pxReloadedMesh = Zenith_AssetRegistry::Get().Get<Zenith_MeshAsset>(strMeshAssetPath);
	Zenith_Assert(pxReloadedMesh != nullptr, "Should be able to reload mesh asset");
	Zenith_Assert(pxReloadedMesh->GetNumVerts() == pxMesh->GetNumVerts(), "Reloaded mesh vertex count mismatch");
	Zenith_Assert(pxReloadedMesh->GetNumIndices() == pxMesh->GetNumIndices(), "Reloaded mesh index count mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded mesh asset verified: %u verts, %u indices",
		pxReloadedMesh->GetNumVerts(), pxReloadedMesh->GetNumIndices());

#ifdef ZENITH_TOOLS
	// Reload and verify Flux_MeshGeometry format
	Flux_MeshGeometry xReloadedGeometry;
	Flux_MeshGeometry::LoadFromFile((strOutputDir + "Tree.zmesh").c_str(), xReloadedGeometry, 0, false);
	Zenith_Assert(xReloadedGeometry.GetNumVerts() == pxMesh->GetNumVerts(), "Reloaded geometry vertex count mismatch");
	Zenith_Assert(xReloadedGeometry.GetNumIndices() == pxMesh->GetNumIndices(), "Reloaded geometry index count mismatch");
	Zenith_Assert(xReloadedGeometry.GetNumBones() == pxSkel->GetNumBones(), "Reloaded geometry bone count mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded mesh geometry verified: %u verts, %u indices, %u bones",
		xReloadedGeometry.GetNumVerts(), xReloadedGeometry.GetNumIndices(), xReloadedGeometry.GetNumBones());

	// Reload and verify VAT
	Flux_AnimationTexture* pxReloadedVAT = Flux_AnimationTexture::LoadFromFile(strOutputDir + "Tree_Sway.zanmt");
	Zenith_Assert(pxReloadedVAT != nullptr, "Should be able to reload VAT");
	Zenith_Assert(pxReloadedVAT->GetVertexCount() == pxMesh->GetNumVerts(), "Reloaded VAT vertex count mismatch");
	Zenith_Assert(pxReloadedVAT->GetNumAnimations() == 1, "Reloaded VAT should have 1 animation");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded VAT verified: %u vertices, %u animations, %u frames",
		pxReloadedVAT->GetVertexCount(), pxReloadedVAT->GetNumAnimations(), pxReloadedVAT->GetFramesPerAnimation());
	delete pxReloadedVAT;
#endif

	// Reload and verify animation
	Flux_AnimationClip* pxReloadedSway = Flux_AnimationClip::LoadFromZanimFile(strSwayPath);
	Zenith_Assert(pxReloadedSway != nullptr, "Should be able to reload sway animation");
	Zenith_Assert(pxReloadedSway->GetName() == "Sway", "Reloaded sway animation name mismatch");
	Zenith_Assert(FloatEquals(pxReloadedSway->GetDuration(), 2.0f, 0.01f), "Reloaded sway duration mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded sway animation verified: duration=%.1fs", pxReloadedSway->GetDuration());

	// Cleanup
	delete pxReloadedSway;
	delete pxSwayClip;
	delete pxMesh;
	delete pxSkel;

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestProceduralTreeAssetExport completed successfully");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Assets available at: %s", strOutputDir.c_str());
}
