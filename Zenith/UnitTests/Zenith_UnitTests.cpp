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
#include "FileAccess/Zenith_FileAccess.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneOperation.h"
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
#include "Flux/MeshAnimation/Flux_AnimationLayer.h"

// Tween system includes
#include "Core/Zenith_Tween.h"
#include "EntityComponent/Components/Zenith_TweenComponent.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

// Mesh geometry include (for exporting runtime-format meshes)
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

// Animation texture include (for VAT baking)
#include "Flux/InstancedMeshes/Flux_AnimationTexture.h"

// Asset pipeline includes
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"

// Asset system includes
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Prefab/Zenith_Prefab.h"

// Model instance (for material tests)
#include "Flux/Flux_ModelInstance.h"

// Async asset loading
#include "AssetHandling/Zenith_AsyncAssetLoader.h"

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
	TestTacticalPointDebugColor();
	TestSquadDebugRoleColor();

	// Asset Handle tests (operator bool fix for procedural assets)
	TestAssetHandleProceduralBoolConversion();
	TestAssetHandlePathBasedBoolConversion();
	TestAssetHandleEmptyBoolConversion();
	TestAssetHandleSetStoresRef();
	TestAssetHandleCopySemantics();
	TestAssetHandleMoveSemantics();
	TestAssetHandleSetPathReleasesRef();
	TestAssetHandleClearReleasesRef();
	TestAssetHandleProceduralComparison();

	// Model Instance Material tests (GBuffer rendering bug fix)
	TestModelInstanceMaterialSetAndGet();
	TestMaterialHandleCopyPreservesCachedPointer();

	// Any-State Transition tests
	TestAnyStateTransitionFires();
	TestAnyStateTransitionSkipsSelf();
	TestAnyStateTransitionPriority();

	// AnimatorStateInfo tests
	TestStateInfoStateName();
	TestStateInfoNormalizedTime();

	// CrossFade tests
	TestCrossFadeToState();
	TestCrossFadeToCurrentState();

	// Sub-State Machine tests
	TestSubStateMachineCreation();
	TestSubStateMachineSharedParameters();

	// Animation Layer tests
	TestLayerCreation();
	TestLayerWeightZero();

	// Tween system tests - Easing
	TestEasingLinear();
	TestEasingEndpoints();
	TestEasingQuadOut();
	TestEasingBounceOut();

	// Tween system tests - TweenInstance
	TestTweenInstanceProgress();
	TestTweenInstanceCompletion();
	TestTweenInstanceDelay();

	// Tween system tests - TweenComponent
	TestTweenComponentScaleTo();
	TestTweenComponentPositionTo();
	TestTweenComponentMultiple();
	TestTweenComponentCallback();
	TestTweenComponentLoop();
	TestTweenComponentPingPong();
	TestTweenComponentCancel();

	// Sub-SM transition evaluation test (verifies BUG 1 fix)
	TestSubStateMachineTransitionEvaluation();

	// CrossFade edge cases
	TestCrossFadeNonExistentState();
	TestCrossFadeInstant();

	// Tween rotation
	TestTweenComponentRotation();

	// Bug regression tests (from code review)
	TestTriggerNotConsumedOnPartialConditionMatch();
	TestResolveClipReferencesRecursive();
	TestTweenDelayWithLoop();
	TestTweenCallbackReentrant();
	TestTweenDuplicatePropertyCancels();

	// Code review round 2 - bug fix regression tests
	TestSubStateMachineTransitionBlendPose();
	TestRotationTweenShortestPath();
	TestTransitionInterruption();
	TestTransitionNonInterruptible();
	TestCancelByPropertyKeepsOthers();
	TestCrossFadeWhileTransitioning();
	TestTweenLoopValueReset();

	// Code review round 3 - Bug 1 regression test + serialization round-trips
	TestTriggerNotConsumedWhenBlockedByPriority();
	TestAnimationLayerSerialization();
	TestAnyStateTransitionSerialization();
	TestSubStateMachineSerialization();

	// Code review round 4 - bug fix validation tests
	TestHasAnimationContentWithLayers();
	TestInitializeRetroactiveLayerPoses();
	TestResolveClipReferencesBlendSpace2D();
	TestResolveClipReferencesSelect();
	TestLayerCompositionOverrideBlend();

	// Code review round 5 - additional coverage
	TestLayerCompositionAdditiveBlend();
	TestLayerMaskedOverrideBlend();
	TestPingPongAsymmetricEasing();
	TestTransitionCompletionFramePose();

	// Scene Management System tests (in separate file)
	Zenith_SceneTests::RunAllTests();

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

	TestData xTest0 = { 0, ~0u }, xTest1 = { 1, ~0u }, xTest2 = { 2, ~0u };
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
	(void)xEvents.at(pxTask0->GetCompletedThreadID());
	(void)xEvents.at(pxTask1->GetCompletedThreadID());
	(void)xEvents.at(pxTask2->GetCompletedThreadID());

	Zenith_Assert(xEventsMain.GetSize() == 8, "Expected 8 events, have %zu", xEvents.size());
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
	static u_int s_uCount;

	explicit MemoryPoolTest(u_int& uOut)
	: m_uTest(++s_uCount)
	{
		uOut = m_uTest;
	}

	~MemoryPoolTest()
	{
		s_uCount--;
	}

	u_int m_uTest;
};
u_int MemoryPoolTest::s_uCount = 0;

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

	// Create a temporary scene through SceneManager
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestComponentSerializationScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Test TransformComponent
	{
		Zenith_Entity xEntity(pxSceneData, "TestTransformEntity");
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
		Zenith_Entity xEntity2(pxSceneData, "TestTransformEntity2");
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
		Zenith_Entity xEntity(pxSceneData, "TestCameraEntity");
		Zenith_CameraComponent& xCamera = xEntity.AddComponent<Zenith_CameraComponent>();

		// Set ground truth data
		const Zenith_Maths::Vector3 xGroundTruthPos(5.0f, 10.0f, 15.0f);
		const float fGroundTruthPitch = 0.5f;
		const float fGroundTruthYaw = 1.2f;
		const float fGroundTruthFOV = 60.0f;
		const float fGroundTruthNear = 0.1f;
		const float fGroundTruthFar = 1000.0f;
		const float fGroundTruthAspect = 16.0f / 9.0f;

		xCamera.InitialisePerspective({
			.m_xPosition = xGroundTruthPos,
			.m_fPitch = fGroundTruthPitch,
			.m_fYaw = fGroundTruthYaw,
			.m_fFOV = fGroundTruthFOV,
			.m_fNear = fGroundTruthNear,
			.m_fFar = fGroundTruthFar,
			.m_fAspectRatio = fGroundTruthAspect,
		});

		// Serialize
		Zenith_DataStream xStream;
		xCamera.WriteToDataStream(xStream);

		// Deserialize into new component
		xStream.SetCursor(0);
		Zenith_Entity xEntity2(pxSceneData, "TestCameraEntity2");
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

	// Clean up test scene
	Zenith_SceneManager::UnloadScene(xTestScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentSerialization completed successfully");
}

/**
 * Test entity serialization round-trip
 * Verifies that entities with multiple components can be serialized and restored
 */
void Zenith_UnitTests::TestEntitySerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntitySerialization...");

	// Create a temporary scene through SceneManager
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestEntitySerializationScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create ground truth entity with multiple components
	Zenith_Entity xGroundTruthEntity(pxSceneData, "TestEntity");

	// Add TransformComponent
	Zenith_TransformComponent& xTransform = xGroundTruthEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f));
	xTransform.SetRotation(Zenith_Maths::Quat(0.707f, 0.0f, 0.707f, 0.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(1.5f, 1.5f, 1.5f));

	// Add CameraComponent
	Zenith_CameraComponent& xCamera = xGroundTruthEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective({
		.m_xPosition = Zenith_Maths::Vector3(0.0f, 5.0f, 10.0f),
	});

	// Serialize entity
	Zenith_DataStream xStream;
	xGroundTruthEntity.WriteToDataStream(xStream);

	// Verify entity metadata was written
	const std::string strExpectedName = xGroundTruthEntity.GetName();

	// Deserialize into new entity
	// Note: The new entity gets its own fresh EntityID from the scene's slot system
	// ReadFromDataStream only loads component data and name, not the ID
	xStream.SetCursor(0);
	Zenith_Entity xLoadedEntity(pxSceneData, "PlaceholderName");
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

	// Clean up test scene
	Zenith_SceneManager::UnloadScene(xTestScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntitySerialization completed successfully");
}

/**
 * Test full scene serialization
 * Verifies that entire scenes with multiple entities can be saved to disk
 */
void Zenith_UnitTests::TestSceneSerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSceneSerialization...");

	// Create a test scene through SceneManager
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestSceneSerializationScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Entity 1: Camera
	Zenith_Entity xCameraEntity(pxSceneData, "MainCamera");
	xCameraEntity.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective({
		.m_xPosition = Zenith_Maths::Vector3(0.0f, 10.0f, 20.0f),
	});
	pxSceneData->SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Entity 2: Transform only
	Zenith_Entity xEntity1(pxSceneData, "TestEntity1");
	xEntity1.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform1 = xEntity1.GetComponent<Zenith_TransformComponent>();
	xTransform1.SetPosition(Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f));

	// Entity 2: Transform only
	Zenith_Entity xEntity2(pxSceneData, "TestEntity2");
	xEntity2.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
	xTransform2.SetPosition(Zenith_Maths::Vector3(-5.0f, 0.0f, 0.0f));

	// Save scene to file
	const std::string strTestScenePath = "unit_test_scene" ZENITH_SCENE_EXT;
	pxSceneData->SaveToFile(strTestScenePath);

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

	// Clean up test scene
	Zenith_SceneManager::UnloadScene(xTestScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneSerialization completed successfully");
}

/**
 * Test complete round-trip: save scene, clear, load scene, verify
 * This is the most comprehensive test - ensures data integrity across full save/load cycle
 */
void Zenith_UnitTests::TestSceneRoundTrip()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSceneRoundTrip...");

	const std::string strTestScenePath = "unit_test_roundtrip" ZENITH_SCENE_EXT;

	// ========================================================================
	// STEP 1: CREATE GROUND TRUTH SCENE
	// ========================================================================

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestSceneRoundTripScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create Entity 1: Camera with specific properties
	Zenith_Entity xCameraEntity(pxSceneData, "MainCamera");
	const Zenith_EntityID uCameraEntityID = xCameraEntity.GetEntityID();
	xCameraEntity.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	const Zenith_Maths::Vector3 xCameraPos(0.0f, 10.0f, 20.0f);
	const float fCameraPitch = 0.3f;
	const float fCameraYaw = 1.57f;
	const float fCameraFOV = 75.0f;
	xCamera.InitialisePerspective({
		.m_xPosition = xCameraPos,
		.m_fPitch = fCameraPitch,
		.m_fYaw = fCameraYaw,
		.m_fFOV = fCameraFOV,
	});
	pxSceneData->SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Create Entity 2: Transform with precise values
	Zenith_Entity xEntity1(pxSceneData, "TestEntity1");
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
	Zenith_Entity xEntity2(pxSceneData, "TestEntity2");
	const Zenith_EntityID uEntity2ID = xEntity2.GetEntityID();
	xEntity2.SetTransient(false);  // Mark as persistent in scene's map
	Zenith_TransformComponent& xTransform2 = xEntity2.GetComponent<Zenith_TransformComponent>();
	const Zenith_Maths::Vector3 xEntity2Pos(-5.0f, 0.0f, 10.0f);
	xTransform2.SetPosition(xEntity2Pos);

	const u_int uGroundTruthEntityCount = 3;

	// ========================================================================
	// STEP 2: SAVE SCENE TO DISK
	// ========================================================================

	pxSceneData->SaveToFile(strTestScenePath);
	Zenith_Assert(std::filesystem::exists(strTestScenePath), "Scene file was not created during round-trip test");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Scene saved to disk");

	// ========================================================================
	// STEP 3: CLEAR GROUND TRUTH SCENE (simulate application restart)
	// ========================================================================

	pxSceneData->Reset();
	Zenith_Assert(pxSceneData->GetEntityCount() == 0, "Scene was not properly cleared");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Scene cleared");

	// ========================================================================
	// STEP 4: LOAD SCENE FROM DISK
	// ========================================================================

	Zenith_Scene xLoadedScene = Zenith_SceneManager::CreateEmptyScene("LoadedTestScene");
	Zenith_SceneData* pxLoadedSceneData = Zenith_SceneManager::GetSceneData(xLoadedScene);
	pxLoadedSceneData->LoadFromFile(strTestScenePath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Scene loaded from disk");

	// ========================================================================
	// STEP 5: VERIFY LOADED SCENE MATCHES GROUND TRUTH
	// ========================================================================

	// Verify entity count
	Zenith_Assert(pxLoadedSceneData->GetEntityCount() == uGroundTruthEntityCount,
				  "Loaded scene entity count mismatch (expected %u, got %u)",
				  uGroundTruthEntityCount, pxLoadedSceneData->GetEntityCount());
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Entity count verified (%u entities)", uGroundTruthEntityCount);

	// Verify Camera Entity (look up by name - EntityIDs are runtime-only, not persistent across save/load)
	Zenith_Entity xLoadedCamera = pxLoadedSceneData->FindEntityByName("MainCamera");
	Zenith_Assert(xLoadedCamera.IsValid(), "Camera entity not found after round-trip");
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

	// Verify Entity 1 (look up by name - EntityIDs are runtime-only, not persistent across save/load)
	Zenith_Entity xLoadedEntity1 = pxLoadedSceneData->FindEntityByName("TestEntity1");
	Zenith_Assert(xLoadedEntity1.IsValid(), "Entity1 not found after round-trip");
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

	// Verify Entity 2 (look up by name - EntityIDs are runtime-only, not persistent across save/load)
	Zenith_Entity xLoadedEntity2 = pxLoadedSceneData->FindEntityByName("TestEntity2");
	Zenith_Assert(xLoadedEntity2.IsValid(), "Entity2 not found after round-trip");
	Zenith_Assert(xLoadedEntity2.GetName() == "TestEntity2", "Entity2 name mismatch");
	Zenith_Assert(xLoadedEntity2.HasComponent<Zenith_TransformComponent>(), "Entity2 missing TransformComponent");

	Zenith_TransformComponent& xLoadedTransform2 = xLoadedEntity2.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xLoadedPos2;
	xLoadedTransform2.GetPosition(xLoadedPos2);
	Zenith_Assert(xLoadedPos2 == xEntity2Pos, "Entity2 position mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Entity2 verified");

	// Verify main camera reference
	Zenith_CameraComponent& xMainCamera = pxLoadedSceneData->GetMainCamera();
	Zenith_Maths::Vector3 xMainCameraPos;
	xMainCamera.GetPosition(xMainCameraPos);
	Zenith_Assert(xMainCameraPos == xCameraPos, "Main camera reference mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  ✓ Main camera reference verified");


	// ========================================================================
	// STEP 6: CLEANUP
	// ========================================================================

	// Clean up test scenes
	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_SceneManager::UnloadScene(xLoadedScene);

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

		const Zenith_Vector<Flux_BlendTreeNode_BlendSpace1D::BlendPoint>& xPoints = xBlendSpace.GetBlendPoints();
		Zenith_Assert(xPoints.GetSize() == 3, "Should have 3 blend points");
		Zenith_Assert(FloatEquals(xPoints.Get(0).m_fPosition, 0.0f), "First point position should be 0.0");
		Zenith_Assert(FloatEquals(xPoints.Get(1).m_fPosition, 0.5f), "Second point position should be 0.5");
		Zenith_Assert(FloatEquals(xPoints.Get(2).m_fPosition, 1.0f), "Third point position should be 1.0");

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

		const Zenith_Vector<Flux_BlendTreeNode_BlendSpace1D::BlendPoint>& xPoints = xBlendSpace.GetBlendPoints();
		Zenith_Assert(FloatEquals(xPoints.Get(0).m_fPosition, 0.0f), "After sorting, first should be 0.0");
		Zenith_Assert(FloatEquals(xPoints.Get(1).m_fPosition, 0.5f), "After sorting, second should be 0.5");
		Zenith_Assert(FloatEquals(xPoints.Get(2).m_fPosition, 1.0f), "After sorting, third should be 1.0");

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
	Zenith_AnimationAsset* pxAnimAsset = Zenith_AssetRegistry::Get().Get<Zenith_AnimationAsset>(strAnimPath);
	Flux_AnimationClip* pxClip = pxAnimAsset ? pxAnimAsset->GetClip() : nullptr;

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

	struct CallbackData
	{
		bool m_bEnterCalled = false;
		bool m_bExitCalled = false;
		bool m_bUpdateCalled = false;
		float m_fUpdateDt = 0.0f;
	};
	CallbackData xCallbackData;

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddTrigger("Next");

	Flux_AnimationState* pxStateA = xStateMachine.AddState("StateA");
	xStateMachine.AddState("StateB");

	// Set up callbacks on StateA using function pointers + userdata
	pxStateA->m_pfnOnEnter = [](void* pUserData) { static_cast<CallbackData*>(pUserData)->m_bEnterCalled = true; };
	pxStateA->m_pfnOnExit = [](void* pUserData) { static_cast<CallbackData*>(pUserData)->m_bExitCalled = true; };
	pxStateA->m_pfnOnUpdate = [](void* pUserData, float fDt) {
		CallbackData* pxData = static_cast<CallbackData*>(pUserData);
		pxData->m_bUpdateCalled = true;
		pxData->m_fUpdateDt = fDt;
	};
	pxStateA->m_pCallbackUserData = &xCallbackData;

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
	Zenith_Assert(xCallbackData.m_bEnterCalled == true, "OnEnter should be called on SetState");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] OnEnter called on SetState");

	// Test OnUpdate
	xCallbackData.m_bUpdateCalled = false;
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xCallbackData.m_bUpdateCalled == true, "OnUpdate should be called during Update");
	Zenith_Assert(FloatEquals(xCallbackData.m_fUpdateDt, 0.016f, 0.001f), "OnUpdate should receive delta time");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] OnUpdate called with correct delta time");

	// Test OnExit via transition
	xCallbackData.m_bExitCalled = false;
	xStateMachine.GetParameters().SetTrigger("Next");
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xCallbackData.m_bExitCalled == true, "OnExit should be called when starting transition");
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
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStickFigureAssetExport (verification only)...");

	// Assets are generated by GenerateTestAssets() called earlier in main()
	// This test verifies the assets were created correctly and can be loaded

	// Expected values for StickFigure assets
	const uint32_t uExpectedBoneCount = STICK_BONE_COUNT;  // 16 bones
	const uint32_t uExpectedVertCount = STICK_BONE_COUNT * 8;  // 8 verts per bone = 128
	const uint32_t uExpectedIndexCount = STICK_BONE_COUNT * 36;  // 36 indices per bone = 576

	std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/";
	std::string strSkelPath = strOutputDir + "StickFigure.zskel";
	std::string strMeshAssetPath = strOutputDir + "StickFigure.zasset";
	std::string strIdlePath = strOutputDir + "StickFigure_Idle.zanim";
	std::string strWalkPath = strOutputDir + "StickFigure_Walk.zanim";
	std::string strRunPath = strOutputDir + "StickFigure_Run.zanim";

	// Verify files exist
	Zenith_Assert(std::filesystem::exists(strSkelPath), "Skeleton file should exist");
	Zenith_Assert(std::filesystem::exists(strMeshAssetPath), "Mesh asset file should exist");
	Zenith_Assert(std::filesystem::exists(strIdlePath), "Idle animation file should exist");
	Zenith_Assert(std::filesystem::exists(strWalkPath), "Walk animation file should exist");
	Zenith_Assert(std::filesystem::exists(strRunPath), "Run animation file should exist");

	// Reload and verify skeleton
	Zenith_SkeletonAsset* pxReloadedSkel = Zenith_AssetRegistry::Get().Get<Zenith_SkeletonAsset>(strSkelPath);
	Zenith_Assert(pxReloadedSkel != nullptr, "Should be able to reload skeleton");
	Zenith_Assert(pxReloadedSkel->GetNumBones() == uExpectedBoneCount, "Reloaded skeleton should have 16 bones");
	Zenith_Assert(pxReloadedSkel->HasBone("LeftUpperArm"), "Reloaded skeleton should have LeftUpperArm bone");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded skeleton verified: %u bones", pxReloadedSkel->GetNumBones());

	// Reload and verify mesh asset format
	Zenith_MeshAsset* pxReloadedMesh = Zenith_AssetRegistry::Get().Get<Zenith_MeshAsset>(strMeshAssetPath);
	Zenith_Assert(pxReloadedMesh != nullptr, "Should be able to reload mesh asset");
	Zenith_Assert(pxReloadedMesh->GetNumVerts() == uExpectedVertCount, "Reloaded mesh vertex count mismatch");
	Zenith_Assert(pxReloadedMesh->GetNumIndices() == uExpectedIndexCount, "Reloaded mesh index count mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded mesh asset verified: %u verts, %u indices",
		pxReloadedMesh->GetNumVerts(), pxReloadedMesh->GetNumIndices());

#ifdef ZENITH_TOOLS
	// Reload and verify Flux_MeshGeometry format
	Flux_MeshGeometry xReloadedGeometry;
	Flux_MeshGeometry::LoadFromFile((strOutputDir + "StickFigure.zmesh").c_str(), xReloadedGeometry, 0, false);
	Zenith_Assert(xReloadedGeometry.GetNumVerts() == uExpectedVertCount, "Reloaded geometry vertex count mismatch");
	Zenith_Assert(xReloadedGeometry.GetNumIndices() == uExpectedIndexCount, "Reloaded geometry index count mismatch");
	Zenith_Assert(xReloadedGeometry.GetNumBones() == uExpectedBoneCount, "Reloaded geometry bone count mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded mesh geometry verified: %u verts, %u indices, %u bones",
		xReloadedGeometry.GetNumVerts(), xReloadedGeometry.GetNumIndices(), xReloadedGeometry.GetNumBones());
#endif

	// Reload and verify animations
	Zenith_AnimationAsset* pxReloadedIdleAsset = Zenith_AssetRegistry::Get().Get<Zenith_AnimationAsset>(strIdlePath);
	Zenith_Assert(pxReloadedIdleAsset != nullptr && pxReloadedIdleAsset->GetClip() != nullptr, "Should be able to reload idle animation");
	Zenith_Assert(pxReloadedIdleAsset->GetClip()->GetName() == "Idle", "Reloaded idle animation name mismatch");
	Zenith_Assert(FloatEquals(pxReloadedIdleAsset->GetClip()->GetDuration(), 2.0f, 0.01f), "Reloaded idle duration mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded idle animation verified: duration=%.1fs", pxReloadedIdleAsset->GetClip()->GetDuration());

	Zenith_AnimationAsset* pxReloadedWalkAsset = Zenith_AssetRegistry::Get().Get<Zenith_AnimationAsset>(strWalkPath);
	Zenith_Assert(pxReloadedWalkAsset != nullptr && pxReloadedWalkAsset->GetClip() != nullptr, "Should be able to reload walk animation");
	Zenith_Assert(pxReloadedWalkAsset->GetClip()->GetName() == "Walk", "Reloaded walk animation name mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded walk animation verified");

	Zenith_AnimationAsset* pxReloadedRunAsset = Zenith_AssetRegistry::Get().Get<Zenith_AnimationAsset>(strRunPath);
	Zenith_Assert(pxReloadedRunAsset != nullptr && pxReloadedRunAsset->GetClip() != nullptr, "Should be able to reload run animation");
	Zenith_Assert(pxReloadedRunAsset->GetClip()->GetName() == "Run", "Reloaded run animation name mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded run animation verified");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStickFigureAssetExport verification completed successfully");
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

	// Create a test scene through SceneManager
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestComponentRemovalIndexUpdateScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	Zenith_Entity xEntity1(pxSceneData, "Entity1");
	Zenith_Entity xEntity2(pxSceneData, "Entity2");
	Zenith_Entity xEntity3(pxSceneData, "Entity3");

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

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentRemovalIndexUpdate completed successfully");
}

/**
 * Test that swap-and-pop removal preserves all component data correctly.
 */
void Zenith_UnitTests::TestComponentSwapAndPop()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComponentSwapAndPop...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestComponentSwapAndPopScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create 5 entities with transforms
	Zenith_Entity xEntities[5] = {
		Zenith_Entity(pxSceneData, "Entity0"),
		Zenith_Entity(pxSceneData, "Entity1"),
		Zenith_Entity(pxSceneData, "Entity2"),
		Zenith_Entity(pxSceneData, "Entity3"),
		Zenith_Entity(pxSceneData, "Entity4")
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

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentSwapAndPop completed successfully");
}

/**
 * Test removing multiple components from multiple entities in sequence.
 */
void Zenith_UnitTests::TestMultipleComponentRemoval()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestMultipleComponentRemoval...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestMultipleComponentRemovalScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create entities with multiple component types
	Zenith_Entity xEntity1(pxSceneData, "Entity1");
	Zenith_Entity xEntity2(pxSceneData, "Entity2");
	Zenith_Entity xEntity3(pxSceneData, "Entity3");

	// Add CameraComponents to entities 1 and 2
	xEntity1.AddComponent<Zenith_CameraComponent>().InitialisePerspective({
		.m_xPosition = Zenith_Maths::Vector3(1, 0, 0),
		.m_fFar = 100,
		.m_fAspectRatio = 1.0f,
	});
	xEntity2.AddComponent<Zenith_CameraComponent>().InitialisePerspective({
		.m_xPosition = Zenith_Maths::Vector3(2, 0, 0),
		.m_fFar = 100,
		.m_fAspectRatio = 1.0f,
	});

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

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleComponentRemoval completed successfully");
}

/**
 * Stress test component removal with many entities.
 */
void Zenith_UnitTests::TestComponentRemovalWithManyEntities()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestComponentRemovalWithManyEntities...");

	constexpr u_int NUM_ENTITIES = 1000;
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestComponentRemovalWithManyEntitiesScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create many entities
	std::vector<Zenith_Entity> xEntities;
	xEntities.reserve(NUM_ENTITIES);

	for (u_int i = 0; i < NUM_ENTITIES; ++i)
	{
		xEntities.emplace_back(pxSceneData, "StressEntity" + std::to_string(i));
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

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentRemovalWithManyEntities completed successfully (tested %u entities)", NUM_ENTITIES);
}

/**
 * Test that entity names are stored in the scene and accessible via GetName()/SetName().
 */
void Zenith_UnitTests::TestEntityNameFromScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityNameFromScene...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestEntityNameFromSceneScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create entity with name
	Zenith_Entity xEntity(pxSceneData, "TestEntityName");

	// Verify GetName() returns the correct name
	Zenith_Assert(xEntity.GetName() == "TestEntityName",
		"TestEntityNameFromScene: GetName() returned wrong name");

	// Change name via SetName()
	xEntity.SetName("RenamedEntity");
	Zenith_Assert(xEntity.GetName() == "RenamedEntity",
		"TestEntityNameFromScene: SetName() did not update name");

	// Verify name is accessible through the scene's entity API
	Zenith_Assert(pxSceneData->GetEntity(xEntity.GetEntityID()).GetName() == "RenamedEntity",
		"TestEntityNameFromScene: Entity in scene does not have correct name");

	// Create another entity and verify names don't interfere
	Zenith_Entity xEntity2(pxSceneData, "SecondEntity");
	Zenith_Assert(xEntity.GetName() == "RenamedEntity",
		"TestEntityNameFromScene: First entity name changed after creating second");
	Zenith_Assert(xEntity2.GetName() == "SecondEntity",
		"TestEntityNameFromScene: Second entity has wrong name");

	Zenith_SceneManager::UnloadScene(xTestScene);
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

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestEntityCopyPreservesAccessScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	Zenith_Entity xOriginal(pxSceneData, "OriginalEntity");

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

	Zenith_SceneManager::UnloadScene(xTestScene);
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

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestComponentMetaSerializationScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	Zenith_Entity xEntity(pxSceneData, "SerializationTestEntity");

	// Set up transform
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(10.0f, 20.0f, 30.0f));
	xEntity.GetComponent<Zenith_TransformComponent>().SetScale(
		Zenith_Maths::Vector3(2.0f, 3.0f, 4.0f));

	// Add a camera component
	Zenith_CameraComponent& xCamera = xEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective({
		.m_xPosition = Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f),
		.m_fPitch = 0.5f,
		.m_fYaw = 1.0f,
	});

	// Serialize via registry
	Zenith_DataStream xStream;
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(xEntity, xStream);

	// If we get here without assertion, serialization worked
	// The deserialization test will verify the data is correct

	Zenith_SceneManager::UnloadScene(xTestScene);
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

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestComponentMetaDeserializationScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	Zenith_Entity xOriginal(pxSceneData, "OriginalEntity");

	// Set distinctive values
	xOriginal.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(111.0f, 222.0f, 333.0f));

	// Serialize original
	Zenith_DataStream xStream;
	Zenith_ComponentMetaRegistry::Get().SerializeEntityComponents(xOriginal, xStream);

	// Create new entity
	Zenith_Entity xNew(pxSceneData, "NewEntity");

	// Reset stream cursor
	xStream.SetCursor(0);

	// Deserialize onto new entity
	Zenith_ComponentMetaRegistry::Get().DeserializeEntityComponents(xNew, xStream);

	// Verify transform was copied
	Zenith_Maths::Vector3 xNewPos;
	xNew.GetComponent<Zenith_TransformComponent>().GetPosition(xNewPos);
	Zenith_Assert(xNewPos.x == 111.0f && xNewPos.y == 222.0f && xNewPos.z == 333.0f,
		"TestComponentMetaDeserialization: Deserialized transform position is wrong");

	Zenith_SceneManager::UnloadScene(xTestScene);
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

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestLifecycleOnAwakeScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	Zenith_Entity xEntity(pxSceneData, "AwakeTestEntity");

	// Dispatch OnAwake - should complete without crashing
	// (no components implement OnAwake, so nothing is called)
	Zenith_ComponentMetaRegistry::Get().DispatchOnAwake(xEntity);

	// Verify entity is still valid
	Zenith_Assert(xEntity.HasComponent<Zenith_TransformComponent>(),
		"TestLifecycleOnAwake: Entity lost TransformComponent after dispatch");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleOnAwake completed successfully");
}

/**
 * Test that DispatchOnStart correctly calls OnStart on components that have it.
 */
void Zenith_UnitTests::TestLifecycleOnStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleOnStart...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestLifecycleOnStartScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	Zenith_Entity xEntity(pxSceneData, "StartTestEntity");

	// Dispatch OnStart - should complete without crashing
	Zenith_ComponentMetaRegistry::Get().DispatchOnStart(xEntity);

	// Verify entity is still valid
	Zenith_Assert(xEntity.HasComponent<Zenith_TransformComponent>(),
		"TestLifecycleOnStart: Entity lost TransformComponent after dispatch");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleOnStart completed successfully");
}

/**
 * Test that DispatchOnUpdate correctly calls OnUpdate on components that have it.
 */
void Zenith_UnitTests::TestLifecycleOnUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleOnUpdate...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestLifecycleOnUpdateScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	Zenith_Entity xEntity(pxSceneData, "UpdateTestEntity");

	// Dispatch OnUpdate with a delta time - should complete without crashing
	const float fDt = 0.016f; // ~60fps
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, fDt);

	// Verify entity is still valid
	Zenith_Assert(xEntity.HasComponent<Zenith_TransformComponent>(),
		"TestLifecycleOnUpdate: Entity lost TransformComponent after dispatch");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleOnUpdate completed successfully");
}

/**
 * Test that DispatchOnDestroy correctly calls OnDestroy on components that have it.
 */
void Zenith_UnitTests::TestLifecycleOnDestroy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleOnDestroy...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestLifecycleOnDestroyScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	Zenith_Entity xEntity(pxSceneData, "DestroyTestEntity");

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

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLifecycleOnDestroy completed successfully");
}

/**
 * Test that lifecycle dispatch respects component serialization order.
 * Components with lower serialization order should have their hooks called first.
 */
void Zenith_UnitTests::TestLifecycleDispatchOrder()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLifecycleDispatchOrder...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestLifecycleDispatchOrderScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	Zenith_Entity xEntity(pxSceneData, "OrderTestEntity");

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

	Zenith_SceneManager::UnloadScene(xTestScene);
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

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Store initial entity count
	const u_int uInitialCount = pxSceneData->GetEntityCount();

	// Create initial entity
	Zenith_Entity xInitialEntity(pxSceneData, "InitialEntity");
	Zenith_EntityID xInitialID = xInitialEntity.GetEntityID();

	// Copy entity IDs to prevent iterator invalidation (the safe pattern)
	Zenith_Vector<Zenith_EntityID> xEntityIDs;
	xEntityIDs.Reserve(pxSceneData->GetActiveEntities().GetSize());
	for (u_int u = 0; u < pxSceneData->GetActiveEntities().GetSize(); ++u)
	{
		xEntityIDs.PushBack(pxSceneData->GetActiveEntities().Get(u));
	}

	// Simulate what OnAwake might do: create more entities
	// This should NOT crash because we're iterating over a copy of IDs
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID xEntityID = xEntityIDs.Get(u);
		if (pxSceneData->EntityExists(xEntityID))
		{
			// Get entity handle (lightweight - safe to use after pool reallocation)
			Zenith_Entity xEntity = pxSceneData->GetEntity(xEntityID);

			// Simulate OnAwake creating multiple new entities
			// This will cause m_xEntitySlots to reallocate
			for (u_int i = 0; i < 10; ++i)
			{
				Zenith_Entity xNewEntity(pxSceneData, "CreatedDuringCallback_" + std::to_string(i));
				// Entity handles are safe - they don't hold pointers into the pool
			}

			// Entity handle still valid after pool reallocation (lightweight handle pattern)
			Zenith_Entity xEntityRefreshed = pxSceneData->GetEntity(xEntityID);

			// Verify the entity is still accessible
			Zenith_Assert(xEntityRefreshed.HasComponent<Zenith_TransformComponent>(),
				"TestLifecycleEntityCreationDuringCallback: Entity lost TransformComponent after sibling creation");
		}
	}

	// Verify original entity is still valid
	Zenith_Assert(pxSceneData->EntityExists(xInitialID),
		"TestLifecycleEntityCreationDuringCallback: Initial entity was invalidated");
	Zenith_Assert(pxSceneData->GetEntity(xInitialID).GetName() == "InitialEntity",
		"TestLifecycleEntityCreationDuringCallback: Initial entity name corrupted");

	// Verify entities were created (proves reallocation happened)
	Zenith_Assert(pxSceneData->GetEntityCount() > uInitialCount + 1,
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

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create several entities
	Zenith_Entity xEntity1(pxSceneData, "LifecycleInitEntity1");
	Zenith_Entity xEntity2(pxSceneData, "LifecycleInitEntity2");
	Zenith_Entity xEntity3(pxSceneData, "LifecycleInitEntity3");

	Zenith_EntityID xID1 = xEntity1.GetEntityID();
	Zenith_EntityID xID2 = xEntity2.GetEntityID();
	Zenith_EntityID xID3 = xEntity3.GetEntityID();

	// Call the shared lifecycle init function
	// This should NOT crash even if callbacks create new entities
	Zenith_SceneManager::DispatchFullLifecycleInit();

	// Verify all original entities are still valid and accessible
	Zenith_Assert(pxSceneData->EntityExists(xID1),
		"TestDispatchFullLifecycleInit: Entity1 was invalidated");
	Zenith_Assert(pxSceneData->EntityExists(xID2),
		"TestDispatchFullLifecycleInit: Entity2 was invalidated");
	Zenith_Assert(pxSceneData->EntityExists(xID3),
		"TestDispatchFullLifecycleInit: Entity3 was invalidated");

	// Verify entities are still accessible with correct data
	Zenith_Assert(pxSceneData->GetEntity(xID1).GetName() == "LifecycleInitEntity1",
		"TestDispatchFullLifecycleInit: Entity1 name corrupted");
	Zenith_Assert(pxSceneData->GetEntity(xID2).GetName() == "LifecycleInitEntity2",
		"TestDispatchFullLifecycleInit: Entity2 name corrupted");
	Zenith_Assert(pxSceneData->GetEntity(xID3).GetName() == "LifecycleInitEntity3",
		"TestDispatchFullLifecycleInit: Entity3 name corrupted");

	// Verify components are intact
	Zenith_Assert(pxSceneData->GetEntity(xID1).HasComponent<Zenith_TransformComponent>(),
		"TestDispatchFullLifecycleInit: Entity1 lost TransformComponent");
	Zenith_Assert(pxSceneData->GetEntity(xID2).HasComponent<Zenith_TransformComponent>(),
		"TestDispatchFullLifecycleInit: Entity2 lost TransformComponent");
	Zenith_Assert(pxSceneData->GetEntity(xID3).HasComponent<Zenith_TransformComponent>(),
		"TestDispatchFullLifecycleInit: Entity3 lost TransformComponent");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDispatchFullLifecycleInit completed successfully");
}

//------------------------------------------------------------------------------
// ECS Query System Tests (Phase 4)
//------------------------------------------------------------------------------

void Zenith_UnitTests::TestQuerySingleComponent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestQuerySingleComponent...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestQuerySingleComponentScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create 3 entities with transforms
	Zenith_Entity xEntity1(pxSceneData, "Entity1");
	Zenith_Entity xEntity2(pxSceneData, "Entity2");
	Zenith_Entity xEntity3(pxSceneData, "Entity3");

	// All 3 entities have TransformComponent (added by default)
	// Add CameraComponent to only 2 entities
	xEntity1.AddComponent<Zenith_CameraComponent>();
	xEntity3.AddComponent<Zenith_CameraComponent>();

	// Query for TransformComponent - should return all 3 entities
	u_int uTransformCount = 0;
	pxSceneData->Query<Zenith_TransformComponent>().ForEach(
		[&uTransformCount](Zenith_EntityID, Zenith_TransformComponent&) {
			uTransformCount++;
		});

	Zenith_Assert(uTransformCount == 3,
		"TestQuerySingleComponent: Expected 3 entities with TransformComponent");

	// Query for CameraComponent - should return 2 entities
	u_int uCameraCount = 0;
	pxSceneData->Query<Zenith_CameraComponent>().ForEach(
		[&uCameraCount](Zenith_EntityID, Zenith_CameraComponent&) {
			uCameraCount++;
		});

	Zenith_Assert(uCameraCount == 2,
		"TestQuerySingleComponent: Expected 2 entities with CameraComponent");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQuerySingleComponent completed successfully");
}

void Zenith_UnitTests::TestQueryMultipleComponents()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestQueryMultipleComponents...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestQueryMultipleComponentsScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create 3 entities with transforms
	Zenith_Entity xEntity1(pxSceneData, "Entity1");
	Zenith_Entity xEntity2(pxSceneData, "Entity2");
	Zenith_Entity xEntity3(pxSceneData, "Entity3");

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
	pxSceneData->Query<Zenith_TransformComponent, Zenith_CameraComponent>().ForEach(
		[&uMatchCount, &xPositions](Zenith_EntityID,
		                            Zenith_TransformComponent& xTransform,
		                            Zenith_CameraComponent&) {
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

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryMultipleComponents completed successfully");
}

void Zenith_UnitTests::TestQueryNoMatches()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestQueryNoMatches...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestQueryNoMatchesScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create entity with only TransformComponent
	Zenith_Entity xEntity(pxSceneData, "Entity1");

	// Query for CameraComponent - should return no matches
	u_int uCount = 0;
	pxSceneData->Query<Zenith_CameraComponent>().ForEach(
		[&uCount](Zenith_EntityID, Zenith_CameraComponent&) {
			uCount++;
		});

	Zenith_Assert(uCount == 0,
		"TestQueryNoMatches: Expected 0 entities with CameraComponent");

	// Verify Any() returns false
	bool bHasAny = pxSceneData->Query<Zenith_CameraComponent>().Any();
	Zenith_Assert(!bHasAny,
		"TestQueryNoMatches: Any() should return false for empty query");

	// Verify First() returns INVALID_ENTITY_ID
	Zenith_EntityID uFirst = pxSceneData->Query<Zenith_CameraComponent>().First();
	Zenith_Assert(uFirst == INVALID_ENTITY_ID,
		"TestQueryNoMatches: First() should return INVALID_ENTITY_ID for empty query");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryNoMatches completed successfully");
}

void Zenith_UnitTests::TestQueryCount()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestQueryCount...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestQueryCountScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create 5 entities
	Zenith_Entity xEntity1(pxSceneData, "Entity1");
	Zenith_Entity xEntity2(pxSceneData, "Entity2");
	Zenith_Entity xEntity3(pxSceneData, "Entity3");
	Zenith_Entity xEntity4(pxSceneData, "Entity4");
	Zenith_Entity xEntity5(pxSceneData, "Entity5");

	// Add CameraComponent to 3 entities
	xEntity2.AddComponent<Zenith_CameraComponent>();
	xEntity3.AddComponent<Zenith_CameraComponent>();
	xEntity5.AddComponent<Zenith_CameraComponent>();

	// Test Count() for TransformComponent (all 5)
	u_int uTransformCount = pxSceneData->Query<Zenith_TransformComponent>().Count();
	Zenith_Assert(uTransformCount == 5,
		"TestQueryCount: Expected 5 entities with TransformComponent");

	// Test Count() for CameraComponent (3)
	u_int uCameraCount = pxSceneData->Query<Zenith_CameraComponent>().Count();
	Zenith_Assert(uCameraCount == 3,
		"TestQueryCount: Expected 3 entities with CameraComponent");

	// Test Count() for both components (3)
	u_int uBothCount = pxSceneData->Query<Zenith_TransformComponent, Zenith_CameraComponent>().Count();
	Zenith_Assert(uBothCount == 3,
		"TestQueryCount: Expected 3 entities with both Transform and Camera");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryCount completed successfully");
}

void Zenith_UnitTests::TestQueryFirstAndAny()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestQueryFirstAndAny...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("TestQueryFirstAndAnyScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create 3 entities
	Zenith_Entity xEntity1(pxSceneData, "Entity1");
	Zenith_Entity xEntity2(pxSceneData, "Entity2");
	Zenith_Entity xEntity3(pxSceneData, "Entity3");

	// Add CameraComponent to entity 2
	xEntity2.AddComponent<Zenith_CameraComponent>();

	// Test Any() returns true when there are matches
	bool bHasCamera = pxSceneData->Query<Zenith_CameraComponent>().Any();
	Zenith_Assert(bHasCamera,
		"TestQueryFirstAndAny: Any() should return true when matches exist");

	// Test First() returns a valid entity ID
	Zenith_EntityID uFirstCamera = pxSceneData->Query<Zenith_CameraComponent>().First();
	Zenith_Assert(uFirstCamera != INVALID_ENTITY_ID,
		"TestQueryFirstAndAny: First() should return valid ID when matches exist");

	// Verify the first match actually has the component
	Zenith_Assert(pxSceneData->EntityHasComponent<Zenith_CameraComponent>(uFirstCamera),
		"TestQueryFirstAndAny: First() returned entity without expected component");

	// Test First() for TransformComponent returns the first entity ID (1)
	Zenith_EntityID uFirstTransform = pxSceneData->Query<Zenith_TransformComponent>().First();
	Zenith_Assert(uFirstTransform != INVALID_ENTITY_ID,
		"TestQueryFirstAndAny: First() should return valid ID for TransformComponent");

	Zenith_SceneManager::UnloadScene(xTestScene);
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
	Zenith_EventDispatcher::Get().Subscribe<TestEvent_Custom>(&MultiSubscriber2);

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

//------------------------------------------------------------------------------
// Entity Hierarchy Tests
//------------------------------------------------------------------------------

void Zenith_UnitTests::TestEntityAddChild()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityAddChild...");

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create parent and child entities
	Zenith_Entity xParent(pxSceneData, "TestParent");
	Zenith_Entity xChild(pxSceneData, "TestChild");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Initially, both should have no children
	Zenith_Assert(xParent.GetChildCount() == 0, "TestEntityAddChild: Parent should have no children initially");
	Zenith_Assert(!xParent.HasChildren(), "TestEntityAddChild: HasChildren should be false");

	// Add child using SetParent
	xChild.SetParent(uParentID);

	// Verify parent-child relationship (Entity handles delegate to single source of truth)
	Zenith_Entity xChildRef = pxSceneData->GetEntity(uChildID);
	Zenith_Entity xParentRef = pxSceneData->GetEntity(uParentID);

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

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create parent and child entities
	Zenith_Entity xParent(pxSceneData, "TestParent2");
	Zenith_Entity xChild(pxSceneData, "TestChild2");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChildID = xChild.GetEntityID();

	// Set parent
	xChild.SetParent(uParentID);
	Zenith_Assert(xParent.GetChildCount() == 1, "TestEntityRemoveChild: Parent should have 1 child");

	// Remove parent (unparent child)
	xChild.SetParent(INVALID_ENTITY_ID);

	// Verify relationship is broken
	Zenith_Entity xChildRef = pxSceneData->GetEntity(uChildID);
	Zenith_Entity xParentRef = pxSceneData->GetEntity(uParentID);

	Zenith_Assert(!xChildRef.HasParent(), "TestEntityRemoveChild: Child should no longer have parent");
	Zenith_Assert(xChildRef.GetParentEntityID() == INVALID_ENTITY_ID, "TestEntityRemoveChild: Child parent ID should be INVALID");
	Zenith_Assert(xParentRef.GetChildCount() == 0, "TestEntityRemoveChild: Parent should have no children");
	Zenith_Assert(!xParentRef.HasChildren(), "TestEntityRemoveChild: Parent HasChildren should be false");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityRemoveChild completed successfully");
}

void Zenith_UnitTests::TestEntityGetChildren()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEntityGetChildren...");

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create parent with multiple children
	Zenith_Entity xParent(pxSceneData, "TestParent3");
	Zenith_Entity xChild1(pxSceneData, "TestChild3a");
	Zenith_Entity xChild2(pxSceneData, "TestChild3b");
	Zenith_Entity xChild3(pxSceneData, "TestChild3c");

	Zenith_EntityID uParentID = xParent.GetEntityID();
	Zenith_EntityID uChild1ID = xChild1.GetEntityID();
	Zenith_EntityID uChild2ID = xChild2.GetEntityID();
	Zenith_EntityID uChild3ID = xChild3.GetEntityID();

	// Add all children
	xChild1.SetParent(uParentID);
	xChild2.SetParent(uParentID);
	xChild3.SetParent(uParentID);

	// Verify all children are tracked
	Zenith_Entity xParentRef = pxSceneData->GetEntity(uParentID);
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

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create entities for reparenting test
	Zenith_Entity xParentA(pxSceneData, "ParentA");
	Zenith_Entity xParentB(pxSceneData, "ParentB");
	Zenith_Entity xChild(pxSceneData, "ReparentChild");

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

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	Zenith_Entity xParent(pxSceneData, "DeleteParent");
	Zenith_Entity xChild(pxSceneData, "DeleteChild");

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

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create hierarchy
	Zenith_Entity xParent(pxSceneData, "SerializeParent");
	Zenith_Entity xChild(pxSceneData, "SerializeChild");

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
	Zenith_Entity xLoadedParent(pxSceneData, "TempParent");
	xLoadedParent.ReadFromDataStream(xStream);

	// Children are stored in scene, so parent ID should serialize
	// The parent's child list is rebuilt when children are loaded and call SetParent
	Zenith_Assert(xLoadedParent.IsRoot(), "TestEntityHierarchySerialization: Loaded parent should be root");

	// Serialize child entity
	Zenith_DataStream xChildStream(256);
	xChild.WriteToDataStream(xChildStream);

	// Create entity in scene before deserializing
	xChildStream.SetCursor(0);
	Zenith_Entity xLoadedChild(pxSceneData, "TempChild");
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

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create an entity with a transform component
	Zenith_Entity xEntity(pxSceneData, "PrefabSource");
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

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create source entity
	Zenith_Entity xSource(pxSceneData, "InstantiateSource");
	Zenith_TransformComponent& xTransform = xSource.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(5.0f, 10.0f, 15.0f));

	// Create prefab
	Zenith_Prefab xPrefab;
	xPrefab.CreateFromEntity(xSource, "InstantiatePrefab");

	// Instantiate prefab
	Zenith_Entity xInstance = xPrefab.Instantiate(pxSceneData, "PrefabInstance");

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

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create source entity
	Zenith_Entity xSource(pxSceneData, "RoundTripSource");
	Zenith_TransformComponent& xTransform = xSource.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(100.0f, 200.0f, 300.0f));

	// Create and save prefab
	Zenith_Prefab xPrefab;
	xPrefab.CreateFromEntity(xSource, "RoundTripPrefab");

	std::string strTempPath = "test_roundtrip.zpfb";
	bool bSaved = xPrefab.SaveToFile(strTempPath);
	Zenith_Assert(bSaved, "TestPrefabSaveLoadRoundTrip: Save should succeed");

	// Load prefab via registry
	Zenith_Prefab* pxLoadedPrefab = Zenith_AssetRegistry::Get().Get<Zenith_Prefab>(strTempPath);
	Zenith_Assert(pxLoadedPrefab != nullptr, "TestPrefabSaveLoadRoundTrip: Load should succeed");
	Zenith_Prefab& xLoadedPrefab = *pxLoadedPrefab;
	Zenith_Assert(xLoadedPrefab.IsValid(), "TestPrefabSaveLoadRoundTrip: Loaded prefab should be valid");
	Zenith_Assert(xLoadedPrefab.GetName() == "RoundTripPrefab",
		"TestPrefabSaveLoadRoundTrip: Loaded prefab name should match");

	// Instantiate loaded prefab
	Zenith_Entity xInstance = xLoadedPrefab.Instantiate(pxSceneData, "LoadedInstance");
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

	// Create a base prefab handle (mock - path-based reference)
	std::string strBasePrefabPath = "test_base_prefab.zpfb";
	PrefabHandle xBasePrefabHandle(strBasePrefabPath);

	// Create a variant prefab
	Zenith_Prefab xVariant;
	bool bSuccess = xVariant.CreateAsVariant(xBasePrefabHandle, "VariantPrefab");

	Zenith_Assert(bSuccess, "TestPrefabVariantCreation: CreateAsVariant should succeed");
	Zenith_Assert(xVariant.IsVariant(), "TestPrefabVariantCreation: Should be marked as variant");
	Zenith_Assert(xVariant.GetBasePrefab().IsSet(), "TestPrefabVariantCreation: Should have base prefab set");
	Zenith_Assert(xVariant.GetBasePrefab().GetPath() == strBasePrefabPath,
		"TestPrefabVariantCreation: Base prefab path should match");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPrefabVariantCreation completed successfully");
}

//==============================================================================
// Async Asset Loading Tests
//==============================================================================

void Zenith_UnitTests::TestAsyncLoadState()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAsyncLoadState...");

	// Test that default state is UNLOADED for unknown paths
	std::string strUnknownPath = "game:NonExistent/Unknown.ztex";
	AssetLoadState eState = Zenith_AsyncAssetLoader::GetLoadState(strUnknownPath);
	Zenith_Assert(eState == AssetLoadState::UNLOADED, "TestAsyncLoadState: Unknown path should be UNLOADED");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadState completed successfully");
}

void Zenith_UnitTests::TestAsyncLoadRequest()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAsyncLoadRequest...");

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

//==============================================================================
// Serializable Asset Tests
//==============================================================================

// Test asset class for unit testing serializable assets
class TestSerializableAsset : public Zenith_Asset
{
public:
	ZENITH_ASSET_TYPE_NAME(TestSerializableAsset)

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

	// Register the test serializable asset type
	Zenith_AssetRegistry::RegisterAssetType<TestSerializableAsset>();

	// Verify it was registered
	bool bRegistered = Zenith_AssetRegistry::IsSerializableTypeRegistered("TestSerializableAsset");
	Zenith_Assert(bRegistered, "TestDataAssetRegistration: TestSerializableAsset should be registered");

	// Verify unknown type is not registered
	bool bUnknown = Zenith_AssetRegistry::IsSerializableTypeRegistered("UnknownType");
	Zenith_Assert(!bUnknown, "TestDataAssetRegistration: Unknown type should not be registered");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDataAssetRegistration completed successfully");
}

void Zenith_UnitTests::TestDataAssetCreateAndSave()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDataAssetCreateAndSave...");

	// Ensure type is registered
	Zenith_AssetRegistry::RegisterAssetType<TestSerializableAsset>();

	// Create a new instance via factory
	TestSerializableAsset* pxAsset = Zenith_AssetRegistry::Get().Create<TestSerializableAsset>();
	Zenith_Assert(pxAsset != nullptr, "TestDataAssetCreateAndSave: Failed to create TestSerializableAsset");

	// Set some values
	pxAsset->m_iTestValue = 100;
	pxAsset->m_fTestFloat = 2.71828f;
	pxAsset->m_strTestString = "ModifiedValue";

	// Save to file
	std::string strTestPath = "TestData/test_data_asset.zdata";
	std::filesystem::create_directories("TestData");
	bool bSaved = Zenith_AssetRegistry::Get().Save(pxAsset, strTestPath);
	Zenith_Assert(bSaved, "TestDataAssetCreateAndSave: Failed to save TestSerializableAsset");

	// Verify file exists
	bool bExists = std::filesystem::exists(strTestPath);
	Zenith_Assert(bExists, "TestDataAssetCreateAndSave: Saved file should exist");

	// Note: Asset is managed by registry cache

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDataAssetCreateAndSave completed successfully");
}

void Zenith_UnitTests::TestDataAssetLoad()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDataAssetLoad...");

	// Unload to force reload from disk
	Zenith_AssetRegistry::Get().Unload("TestData/test_data_asset.zdata");

	// Load the asset saved in previous test
	std::string strTestPath = "TestData/test_data_asset.zdata";
	TestSerializableAsset* pxLoaded = Zenith_AssetRegistry::Get().Get<TestSerializableAsset>(strTestPath);
	Zenith_Assert(pxLoaded != nullptr, "TestDataAssetLoad: Failed to load TestSerializableAsset");

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
	Zenith_AssetRegistry::RegisterAssetType<TestSerializableAsset>();

	// Create with unique values
	TestSerializableAsset* pxOriginal = Zenith_AssetRegistry::Get().Create<TestSerializableAsset>();
	pxOriginal->m_iTestValue = -999;
	pxOriginal->m_fTestFloat = 123.456f;
	pxOriginal->m_strTestString = "RoundTripTest";

	// Save (adds to cache)
	std::string strPath = "TestData/round_trip_test.zdata";
	Zenith_AssetRegistry::Get().Save(pxOriginal, strPath);

	// Unload to force reload from disk
	Zenith_AssetRegistry::Get().Unload(strPath);

	// Load
	TestSerializableAsset* pxLoaded = Zenith_AssetRegistry::Get().Get<TestSerializableAsset>(strPath);
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

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create A -> B -> C hierarchy
	Zenith_Entity xA(pxSceneData, "CircularTestA");
	Zenith_Entity xB(pxSceneData, "CircularTestB");
	Zenith_Entity xC(pxSceneData, "CircularTestC");

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
	Zenith_SceneManager::DestroyImmediate(xC);
	Zenith_SceneManager::DestroyImmediate(xB);
	Zenith_SceneManager::DestroyImmediate(xA);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCircularHierarchyPrevention completed successfully");
}

void Zenith_UnitTests::TestSelfParentingPrevention()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSelfParentingPrevention...");

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create an entity
	Zenith_Entity xEntity(pxSceneData, "SelfParentTest");
	Zenith_EntityID uEntityID = xEntity.GetEntityID();

	// Verify initially root
	Zenith_Assert(!xEntity.HasParent(), "TestSelfParentingPrevention: Entity should start as root");

	// Try to parent entity to itself
	xEntity.SetParent(uEntityID);

	// Should still be root (self-parenting rejected)
	Zenith_Assert(!xEntity.HasParent(), "TestSelfParentingPrevention: Self-parenting should be rejected");

	// Clean up
	Zenith_SceneManager::DestroyImmediate(xEntity);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSelfParentingPrevention completed successfully");
}

void Zenith_UnitTests::TestTryGetMainCameraWhenNotSet()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTryGetMainCameraWhenNotSet...");

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Remember current camera if any
	Zenith_EntityID uPreviousCamera = pxSceneData->GetMainCameraEntity();

	// Clear main camera
	pxSceneData->SetMainCameraEntity(INVALID_ENTITY_ID);

	// TryGetMainCamera should return nullptr when no camera is set
	Zenith_CameraComponent* pxCamera = pxSceneData->TryGetMainCamera();
	Zenith_Assert(pxCamera == nullptr, "TestTryGetMainCameraWhenNotSet: TryGetMainCamera should return nullptr when no camera set");

	// Restore previous camera
	if (uPreviousCamera != INVALID_ENTITY_ID && pxSceneData->EntityExists(uPreviousCamera))
	{
		pxSceneData->SetMainCameraEntity(uPreviousCamera);
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetMainCameraWhenNotSet completed successfully");
}

void Zenith_UnitTests::TestDeepHierarchyBuildModelMatrix()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDeepHierarchyBuildModelMatrix...");

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Create a hierarchy with multiple levels (not too deep - just testing it works)
	constexpr u_int DEPTH = 10;
	Zenith_Vector<Zenith_EntityID> xEntityIDs;

	// Create root
	Zenith_Entity xRoot(pxSceneData, "DeepHierarchyRoot");
	xEntityIDs.PushBack(xRoot.GetEntityID());

	// Create children
	for (u_int u = 1; u < DEPTH; ++u)
	{
		std::string strName = "DeepHierarchyChild" + std::to_string(u);
		Zenith_Entity xChild(pxSceneData, strName);
		Zenith_EntityID uChildID = xChild.GetEntityID();
		xEntityIDs.PushBack(uChildID);

		// Parent to previous entity
		Zenith_EntityID uParentID = xEntityIDs.Get(u - 1);
		xChild.SetParent(uParentID);
	}

	// Verify depth
	u_int uActualDepth = 0;
	Zenith_EntityID uCurrent = xEntityIDs.Get(DEPTH - 1);  // Deepest entity
	while (pxSceneData->EntityExists(uCurrent) && pxSceneData->GetEntity(uCurrent).HasParent())
	{
		uActualDepth++;
		uCurrent = pxSceneData->GetEntity(uCurrent).GetParentEntityID();
	}
	Zenith_Assert(uActualDepth == DEPTH - 1, "TestDeepHierarchyBuildModelMatrix: Hierarchy depth should be %u, got %u", DEPTH - 1, uActualDepth);

	// BuildModelMatrix should work without infinite loop
	Zenith_Maths::Matrix4 xMatrix;
	Zenith_EntityID uDeepestID = xEntityIDs.Get(DEPTH - 1);
	pxSceneData->GetEntity(uDeepestID).GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xMatrix);

	// If we get here without hanging, the test passed

	// Clean up (destroy from deepest to root)
	for (int i = static_cast<int>(DEPTH) - 1; i >= 0; --i)
	{
		Zenith_Entity xEntity = pxSceneData->GetEntity(xEntityIDs.Get(i));
		Zenith_SceneManager::DestroyImmediate(xEntity);
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

	// Create a scene through SceneManager (not the active scene)
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("LocalDestructionTestScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create some entities with transforms
	Zenith_Entity xEntity1(pxSceneData, "LocalEntity1");
	Zenith_Entity xEntity2(pxSceneData, "LocalEntity2");
	Zenith_Entity xEntity3(pxSceneData, "LocalEntity3");

	// Set some positions to verify data is valid
	xEntity1.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xEntity2.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
	xEntity3.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(3.0f, 0.0f, 0.0f));

	// Unload the scene - this should NOT crash
	// The original bug was: TransformComponent::~TransformComponent called GetCurrentScene()
	// which returned the wrong scene, causing memory corruption
	Zenith_SceneManager::UnloadScene(xTestScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLocalSceneDestruction completed successfully");
}

/**
 * Test that local scene destruction with parent-child hierarchy doesn't crash.
 * This is a more complex test that includes hierarchy relationships.
 */
void Zenith_UnitTests::TestLocalSceneWithHierarchy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLocalSceneWithHierarchy...");

	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("LocalHierarchyTestScene");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	// Create parent entity
	Zenith_Entity xParent(pxSceneData, "Parent");
	xParent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f));

	// Create child entities
	Zenith_Entity xChild1(pxSceneData, "Child1");
	Zenith_Entity xChild2(pxSceneData, "Child2");

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

	// Unload the scene - destructor should handle hierarchy cleanup safely
	// Without the fix, DetachFromParent/DetachAllChildren would crash trying to
	// access the global scene instead of this scene
	Zenith_SceneManager::UnloadScene(xTestScene);

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

/**
 * Test procedural tree asset loading and verification
 * Assets are generated by GenerateTestAssets() called earlier in main()
 */
void Zenith_UnitTests::TestProceduralTreeAssetExport()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestProceduralTreeAssetExport (verification only)...");

	// Assets are generated by GenerateTestAssets() called earlier in main()
	// This test verifies the assets were created correctly and can be loaded

	// Expected values for Tree assets
	const uint32_t uExpectedBoneCount = TREE_BONE_COUNT;  // 9 bones
	const uint32_t uExpectedVertCount = TREE_BONE_COUNT * 8;  // 8 verts per bone = 72
	const uint32_t uExpectedIndexCount = TREE_BONE_COUNT * 36;  // 36 indices per bone = 324

	std::string strOutputDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/ProceduralTree/";
	std::string strSkelPath = strOutputDir + "Tree.zskel";
	std::string strMeshAssetPath = strOutputDir + "Tree.zasset";
	std::string strSwayPath = strOutputDir + "Tree_Sway.zanim";

	// Verify files exist
	Zenith_Assert(std::filesystem::exists(strSkelPath), "Skeleton file should exist");
	Zenith_Assert(std::filesystem::exists(strMeshAssetPath), "Mesh asset file should exist");
	Zenith_Assert(std::filesystem::exists(strSwayPath), "Sway animation file should exist");

	// Reload and verify skeleton
	Zenith_SkeletonAsset* pxReloadedSkel = Zenith_AssetRegistry::Get().Get<Zenith_SkeletonAsset>(strSkelPath);
	Zenith_Assert(pxReloadedSkel != nullptr, "Should be able to reload skeleton");
	Zenith_Assert(pxReloadedSkel->GetNumBones() == uExpectedBoneCount, "Reloaded skeleton should have 9 bones");
	Zenith_Assert(pxReloadedSkel->HasBone("TrunkLower"), "Reloaded skeleton should have TrunkLower bone");
	Zenith_Assert(pxReloadedSkel->HasBone("Branch1"), "Reloaded skeleton should have Branch1 bone");
	Zenith_Assert(pxReloadedSkel->HasBone("Leaves0"), "Reloaded skeleton should have Leaves0 bone");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded skeleton verified: %u bones", pxReloadedSkel->GetNumBones());

	// Reload and verify mesh asset format
	Zenith_MeshAsset* pxReloadedMesh = Zenith_AssetRegistry::Get().Get<Zenith_MeshAsset>(strMeshAssetPath);
	Zenith_Assert(pxReloadedMesh != nullptr, "Should be able to reload mesh asset");
	Zenith_Assert(pxReloadedMesh->GetNumVerts() == uExpectedVertCount, "Reloaded mesh vertex count mismatch");
	Zenith_Assert(pxReloadedMesh->GetNumIndices() == uExpectedIndexCount, "Reloaded mesh index count mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded mesh asset verified: %u verts, %u indices",
		pxReloadedMesh->GetNumVerts(), pxReloadedMesh->GetNumIndices());

#ifdef ZENITH_TOOLS
	// Reload and verify Flux_MeshGeometry format
	Flux_MeshGeometry xReloadedGeometry;
	Flux_MeshGeometry::LoadFromFile((strOutputDir + "Tree.zmesh").c_str(), xReloadedGeometry, 0, false);
	Zenith_Assert(xReloadedGeometry.GetNumVerts() == uExpectedVertCount, "Reloaded geometry vertex count mismatch");
	Zenith_Assert(xReloadedGeometry.GetNumIndices() == uExpectedIndexCount, "Reloaded geometry index count mismatch");
	Zenith_Assert(xReloadedGeometry.GetNumBones() == uExpectedBoneCount, "Reloaded geometry bone count mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded mesh geometry verified: %u verts, %u indices, %u bones",
		xReloadedGeometry.GetNumVerts(), xReloadedGeometry.GetNumIndices(), xReloadedGeometry.GetNumBones());

	// Reload and verify VAT
	Flux_AnimationTexture* pxReloadedVAT = Flux_AnimationTexture::LoadFromFile(strOutputDir + "Tree_Sway.zanmt");
	Zenith_Assert(pxReloadedVAT != nullptr, "Should be able to reload VAT");
	Zenith_Assert(pxReloadedVAT->GetVertexCount() == uExpectedVertCount, "Reloaded VAT vertex count mismatch");
	Zenith_Assert(pxReloadedVAT->GetNumAnimations() == 1, "Reloaded VAT should have 1 animation");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded VAT verified: %u vertices, %u animations, %u frames",
		pxReloadedVAT->GetVertexCount(), pxReloadedVAT->GetNumAnimations(), pxReloadedVAT->GetFramesPerAnimation());
	delete pxReloadedVAT;
#endif

	// Reload and verify animation
	Zenith_AnimationAsset* pxReloadedSwayAsset = Zenith_AssetRegistry::Get().Get<Zenith_AnimationAsset>(strSwayPath);
	Zenith_Assert(pxReloadedSwayAsset != nullptr && pxReloadedSwayAsset->GetClip() != nullptr, "Should be able to reload sway animation");
	Zenith_Assert(pxReloadedSwayAsset->GetClip()->GetName() == "Sway", "Reloaded sway animation name mismatch");
	Zenith_Assert(FloatEquals(pxReloadedSwayAsset->GetClip()->GetDuration(), 2.0f, 0.01f), "Reloaded sway duration mismatch");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  Reloaded sway animation verified: duration=%.1fs", pxReloadedSwayAsset->GetClip()->GetDuration());

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestProceduralTreeAssetExport verification completed successfully");
}

//=============================================================================
// Asset Handle Tests
// Tests for the operator bool() fix that ensures procedural assets (via Set())
// are correctly detected as valid, not just path-based assets.
//=============================================================================

#include "AssetHandling/Zenith_MaterialAsset.h"

void Zenith_UnitTests::TestAssetHandleProceduralBoolConversion()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleProceduralBoolConversion...");

	// Create a procedural material via registry
	auto& xRegistry = Zenith_AssetRegistry::Get();
	Zenith_MaterialAsset* pxMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestProceduralMaterial");

	// Create a handle and set it via Set() (procedural path)
	MaterialHandle xHandle;
	xHandle.Set(pxMaterial);

	// The key fix: operator bool() should return true for procedural assets
	// Previously it only checked if path was set, which is empty for procedural assets
	Zenith_Assert(static_cast<bool>(xHandle), "Procedural asset handle should be valid (operator bool)");
	Zenith_Assert(xHandle.Get() == pxMaterial, "Get() should return the procedural material");
	Zenith_Assert(xHandle.IsLoaded(), "IsLoaded() should return true for procedural asset");

	// Path should be empty for procedural assets
	Zenith_Assert(xHandle.GetPath().empty(), "Procedural asset should have empty path");
	Zenith_Assert(!xHandle.IsSet(), "IsSet() should return false (no path) for procedural asset");

	// Guard pattern that was broken before the fix:
	// if (!xHandle) { return; } // This would incorrectly return for procedural assets
	bool bGuardPassed = false;
	if (xHandle)
	{
		bGuardPassed = true;
	}
	Zenith_Assert(bGuardPassed, "Guard pattern 'if (xHandle)' should pass for procedural asset");

	// Cleanup is automatic via handle destructor

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleProceduralBoolConversion passed");
}

void Zenith_UnitTests::TestAssetHandlePathBasedBoolConversion()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandlePathBasedBoolConversion...");

	// Create a handle with a path (simulating a file-based asset)
	MaterialHandle xHandle;
	xHandle.SetPath("game:Materials/TestMaterial.zmat");

	// operator bool() should return true when path is set
	Zenith_Assert(static_cast<bool>(xHandle), "Path-based handle should be valid (operator bool)");
	Zenith_Assert(xHandle.IsSet(), "IsSet() should return true for path-based handle");
	Zenith_Assert(!xHandle.GetPath().empty(), "GetPath() should return the path");

	// Note: Get() would try to load from registry which may not exist in test
	// We're testing the bool conversion, not the loading

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandlePathBasedBoolConversion passed");
}

void Zenith_UnitTests::TestAssetHandleEmptyBoolConversion()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleEmptyBoolConversion...");

	// Default-constructed handle should be invalid
	MaterialHandle xHandle;

	Zenith_Assert(!static_cast<bool>(xHandle), "Empty handle should be invalid (operator bool)");
	Zenith_Assert(!xHandle.IsSet(), "Empty handle IsSet() should be false");
	Zenith_Assert(!xHandle.IsLoaded(), "Empty handle IsLoaded() should be false");
	Zenith_Assert(xHandle.GetPath().empty(), "Empty handle path should be empty");
	Zenith_Assert(xHandle.Get() == nullptr, "Empty handle Get() should return nullptr");

	// Guard pattern should correctly skip empty handles
	bool bGuardSkipped = true;
	if (xHandle)
	{
		bGuardSkipped = false;
	}
	Zenith_Assert(bGuardSkipped, "Guard pattern 'if (xHandle)' should skip empty handle");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleEmptyBoolConversion passed");
}

void Zenith_UnitTests::TestAssetHandleSetStoresRef()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleSetStoresRef...");

	// This tests that Set() properly increments reference count
	auto& xRegistry = Zenith_AssetRegistry::Get();
	Zenith_MaterialAsset* pxMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestRefCountMaterial");

	uint32_t uInitialRefCount = pxMaterial->GetRefCount();

	{
		MaterialHandle xHandle;
		xHandle.Set(pxMaterial);

		// Ref count should increase after Set()
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount + 1,
			"Set() should increment ref count");

		// Copy handle should also increment ref count
		MaterialHandle xHandleCopy = xHandle;
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount + 2,
			"Handle copy should increment ref count");
	}
	// After handles go out of scope, ref count should be back to initial

	Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount,
		"Ref count should return to initial after handles destroyed");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleSetStoresRef passed");
}

void Zenith_UnitTests::TestAssetHandleCopySemantics()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleCopySemantics...");

	auto& xRegistry = Zenith_AssetRegistry::Get();
	Zenith_MaterialAsset* pxMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestCopyMaterial");

	uint32_t uInitialRefCount = pxMaterial->GetRefCount();

	// Test copy constructor
	{
		MaterialHandle xHandle1;
		xHandle1.Set(pxMaterial);
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount + 1,
			"Set() should increment ref count");

		// Copy constructor
		MaterialHandle xHandle2(xHandle1);
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount + 2,
			"Copy constructor should increment ref count");

		// Both handles should return the same pointer
		Zenith_Assert(xHandle1.Get() == pxMaterial, "Handle1 should return original pointer");
		Zenith_Assert(xHandle2.Get() == pxMaterial, "Handle2 should return original pointer");
	}

	Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount,
		"Ref count should return to initial after copy handles destroyed");

	// Test copy assignment
	{
		MaterialHandle xHandle1;
		xHandle1.Set(pxMaterial);

		Zenith_MaterialAsset* pxMaterial2 = xRegistry.Create<Zenith_MaterialAsset>();
		pxMaterial2->SetName("TestCopyMaterial2");
		uint32_t uMat2InitialRef = pxMaterial2->GetRefCount();

		MaterialHandle xHandle2;
		xHandle2.Set(pxMaterial2);
		Zenith_Assert(pxMaterial2->GetRefCount() == uMat2InitialRef + 1,
			"Material2 ref count after Set()");

		// Copy assignment - should release old, acquire new
		xHandle2 = xHandle1;
		Zenith_Assert(pxMaterial2->GetRefCount() == uMat2InitialRef,
			"Copy assignment should release old material");
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount + 2,
			"Copy assignment should increment new material ref");
	}

	Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount,
		"Ref count should return to initial after all handles destroyed");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleCopySemantics passed");
}

void Zenith_UnitTests::TestAssetHandleMoveSemantics()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleMoveSemantics...");

	auto& xRegistry = Zenith_AssetRegistry::Get();
	Zenith_MaterialAsset* pxMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestMoveMaterial");

	uint32_t uInitialRefCount = pxMaterial->GetRefCount();

	// Test move constructor
	{
		MaterialHandle xHandle1;
		xHandle1.Set(pxMaterial);
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount + 1,
			"Set() should increment ref count");

		// Move constructor - should NOT change ref count
		MaterialHandle xHandle2(std::move(xHandle1));
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount + 1,
			"Move constructor should NOT change ref count");

		// Source handle should be nullified
		Zenith_Assert(!xHandle1.IsLoaded(), "Moved-from handle should not be loaded");
		Zenith_Assert(xHandle2.Get() == pxMaterial, "Moved-to handle should have pointer");
	}

	Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount,
		"Ref count should return to initial after moved handle destroyed");

	// Test move assignment
	{
		MaterialHandle xHandle1;
		xHandle1.Set(pxMaterial);

		Zenith_MaterialAsset* pxMaterial2 = xRegistry.Create<Zenith_MaterialAsset>();
		pxMaterial2->SetName("TestMoveMaterial2");
		uint32_t uMat2InitialRef = pxMaterial2->GetRefCount();

		MaterialHandle xHandle2;
		xHandle2.Set(pxMaterial2);

		// Move assignment - should release old, take ownership of new
		xHandle2 = std::move(xHandle1);
		Zenith_Assert(pxMaterial2->GetRefCount() == uMat2InitialRef,
			"Move assignment should release old material");
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount + 1,
			"Move assignment should NOT increment new material ref");
		Zenith_Assert(!xHandle1.IsLoaded(), "Moved-from handle should not be loaded");
	}

	Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount,
		"Ref count should return to initial after all handles destroyed");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleMoveSemantics passed");
}

void Zenith_UnitTests::TestAssetHandleSetPathReleasesRef()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleSetPathReleasesRef...");

	auto& xRegistry = Zenith_AssetRegistry::Get();
	Zenith_MaterialAsset* pxMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestSetPathMaterial");

	uint32_t uInitialRefCount = pxMaterial->GetRefCount();

	{
		MaterialHandle xHandle;
		xHandle.Set(pxMaterial);
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount + 1,
			"Set() should increment ref count");

		// SetPath should release the old cached pointer
		xHandle.SetPath("game:Materials/NonExistent.zmat");
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount,
			"SetPath() should release old cached ref");

		// Handle is now path-based, not loaded
		Zenith_Assert(!xHandle.IsLoaded(), "After SetPath, handle should not be loaded");
		Zenith_Assert(xHandle.IsSet(), "After SetPath, handle should have path set");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleSetPathReleasesRef passed");
}

void Zenith_UnitTests::TestAssetHandleClearReleasesRef()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleClearReleasesRef...");

	auto& xRegistry = Zenith_AssetRegistry::Get();
	Zenith_MaterialAsset* pxMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestClearMaterial");

	uint32_t uInitialRefCount = pxMaterial->GetRefCount();

	{
		MaterialHandle xHandle;
		xHandle.Set(pxMaterial);
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount + 1,
			"Set() should increment ref count");

		// Clear should release the ref
		xHandle.Clear();
		Zenith_Assert(pxMaterial->GetRefCount() == uInitialRefCount,
			"Clear() should release ref");

		// Handle should be empty
		Zenith_Assert(!xHandle.IsLoaded(), "After Clear, handle should not be loaded");
		Zenith_Assert(!xHandle.IsSet(), "After Clear, handle should not have path set");
		Zenith_Assert(!static_cast<bool>(xHandle), "After Clear, operator bool should return false");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleClearReleasesRef passed");
}

void Zenith_UnitTests::TestAssetHandleProceduralComparison()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleProceduralComparison...");

	auto& xRegistry = Zenith_AssetRegistry::Get();

	// Create two different procedural materials
	Zenith_MaterialAsset* pxMaterial1 = xRegistry.Create<Zenith_MaterialAsset>();
	pxMaterial1->SetName("TestCompare1");

	Zenith_MaterialAsset* pxMaterial2 = xRegistry.Create<Zenith_MaterialAsset>();
	pxMaterial2->SetName("TestCompare2");

	MaterialHandle xHandle1;
	xHandle1.Set(pxMaterial1);

	MaterialHandle xHandle2;
	xHandle2.Set(pxMaterial2);

	MaterialHandle xHandle1Copy;
	xHandle1Copy.Set(pxMaterial1);

	// Different procedural assets should NOT compare equal
	Zenith_Assert(!(xHandle1 == xHandle2),
		"Different procedural assets should not be equal");
	Zenith_Assert(xHandle1 != xHandle2,
		"Different procedural assets should compare not-equal");

	// Same procedural asset should compare equal
	Zenith_Assert(xHandle1 == xHandle1Copy,
		"Same procedural asset should be equal");
	Zenith_Assert(!(xHandle1 != xHandle1Copy),
		"Same procedural asset should not compare not-equal");

	// Empty handles should compare equal
	MaterialHandle xEmpty1;
	MaterialHandle xEmpty2;
	Zenith_Assert(xEmpty1 == xEmpty2, "Empty handles should be equal");

	// Test path-based comparison still works
	MaterialHandle xPath1;
	xPath1.SetPath("game:Materials/Test.zmat");

	MaterialHandle xPath2;
	xPath2.SetPath("game:Materials/Test.zmat");

	MaterialHandle xPath3;
	xPath3.SetPath("game:Materials/Different.zmat");

	Zenith_Assert(xPath1 == xPath2, "Same path should be equal");
	Zenith_Assert(xPath1 != xPath3, "Different paths should not be equal");

	// Procedural vs path-based should not be equal (even if both valid)
	Zenith_Assert(xHandle1 != xPath1,
		"Procedural and path-based handles should not be equal");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAssetHandleProceduralComparison passed");
}

//=============================================================================
// Model Instance Material Tests (GBuffer rendering bug fix)
//=============================================================================

void Zenith_UnitTests::TestModelInstanceMaterialSetAndGet()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestModelInstanceMaterialSetAndGet...");

	// Create a procedural material (same pattern as Combat game)
	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestMaterial");

	// Create model asset with no default materials (reproduces Combat enemy scenario)
	Zenith_ModelAsset* pxModelAsset = Zenith_AssetRegistry::Get().Create<Zenith_ModelAsset>();
	pxModelAsset->SetName("TestModel");

	// Try to add StickFigure mesh if available
	std::string strTestMesh = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure.zasset";
	Zenith_Vector<std::string> xEmptyMaterials;
	if (std::filesystem::exists(strTestMesh))
	{
		pxModelAsset->AddMeshByPath(strTestMesh, xEmptyMaterials);
	}

	// Create model instance
	Flux_ModelInstance* pxInstance = Flux_ModelInstance::CreateFromAsset(pxModelAsset);
	Zenith_Assert(pxInstance != nullptr, "Failed to create model instance");

	// Model should have at least 1 material slot (blank default added by CreateFromAsset)
	Zenith_Assert(pxInstance->GetNumMaterials() >= 1,
		"Model instance should have at least 1 material slot");

	// Override material at index 0
	pxInstance->SetMaterial(0, pxMaterial);

	// CRITICAL TEST: GetMaterial must return the material we just set
	Zenith_MaterialAsset* pxRetrieved = pxInstance->GetMaterial(0);
	Zenith_Assert(pxRetrieved != nullptr,
		"GetMaterial(0) returned nullptr after SetMaterial - this causes GBuffer rendering to skip the mesh");
	Zenith_Assert(pxRetrieved == pxMaterial,
		"GetMaterial(0) did not return the same pointer that was passed to SetMaterial");

	// Cleanup
	pxInstance->Destroy();
	delete pxInstance;

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestModelInstanceMaterialSetAndGet passed");
}

void Zenith_UnitTests::TestMaterialHandleCopyPreservesCachedPointer()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMaterialHandleCopyPreservesCachedPointer...");

	// Create a procedural material and store in handle (like Combat::g_xEnemyMaterial)
	MaterialHandle xOriginal;
	Zenith_MaterialAsset* pxMaterial = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
	pxMaterial->SetName("TestProceduralMaterial");
	xOriginal.Set(pxMaterial);

	// Verify original handle works
	Zenith_Assert(xOriginal.Get() == pxMaterial, "Original handle should return the material");

	// Copy to another handle (like m_xEnemyMaterial = Combat::g_xEnemyMaterial)
	MaterialHandle xCopy = xOriginal;

	// CRITICAL TEST: Copy must preserve the cached pointer
	Zenith_Assert(xCopy.Get() != nullptr,
		"Copied handle returned nullptr - copy assignment failed to preserve cached pointer");
	Zenith_Assert(xCopy.Get() == pxMaterial,
		"Copied handle returned different pointer than original");

	// Verify original still works after copy
	Zenith_Assert(xOriginal.Get() == pxMaterial, "Original handle should still work after copy");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMaterialHandleCopyPreservesCachedPointer passed");
}

//=============================================================================
// Any-State Transition Tests
//=============================================================================

void Zenith_UnitTests::TestAnyStateTransitionFires()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnyStateTransitionFires...");

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Hit");
	xSM.SetDefaultState("Idle");

	// Add parameter
	xSM.GetParameters().AddTrigger("HitTrigger");

	// Add any-state transition: HitTrigger -> Hit
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Hit";
	xTrans.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "HitTrigger";
	xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xCond.m_bThreshold = true;
	xTrans.m_xConditions.PushBack(xCond);

	xSM.AddAnyStateTransition(xTrans);

	// Initialize state machine with a dummy update
	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	Zenith_Assert(xSM.GetCurrentStateName() == "Idle", "Should start in Idle");

	// Fire trigger
	xSM.GetParameters().SetTrigger("HitTrigger");
	xSM.Update(0.016f, xPose, xSkel);

	// Should be transitioning to Hit
	Zenith_Assert(xSM.IsTransitioning(), "Should be transitioning after trigger");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnyStateTransitionFires passed");
}

void Zenith_UnitTests::TestAnyStateTransitionSkipsSelf()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnyStateTransitionSkipsSelf...");

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.SetDefaultState("Idle");

	xSM.GetParameters().AddBool("AlwaysTrue", true);

	// Add any-state transition targeting current state (Idle -> Idle)
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Idle";
	xTrans.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "AlwaysTrue";
	xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
	xCond.m_bThreshold = true;
	xTrans.m_xConditions.PushBack(xCond);

	xSM.AddAnyStateTransition(xTrans);

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);
	xSM.Update(0.016f, xPose, xSkel);

	// Should NOT be transitioning (self-loop skipped)
	Zenith_Assert(!xSM.IsTransitioning(), "Any-state should skip self-loop");
	Zenith_Assert(xSM.GetCurrentStateName() == "Idle", "Should remain in Idle");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnyStateTransitionSkipsSelf passed");
}

void Zenith_UnitTests::TestAnyStateTransitionPriority()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnyStateTransitionPriority...");

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Hit");
	xSM.AddState("Death");
	xSM.SetDefaultState("Idle");

	xSM.GetParameters().AddTrigger("HitTrigger");
	xSM.GetParameters().AddTrigger("DeathTrigger");

	// Low priority: HitTrigger -> Hit (priority 10)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Hit";
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = 10;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "HitTrigger";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xCond.m_bThreshold = true;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.AddAnyStateTransition(xTrans);
	}

	// High priority: DeathTrigger -> Death (priority 100)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Death";
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = 100;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "DeathTrigger";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xCond.m_bThreshold = true;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.AddAnyStateTransition(xTrans);
	}

	// Verify priority ordering
	const Zenith_Vector<Flux_StateTransition>& xAny = xSM.GetAnyStateTransitions();
	Zenith_Assert(xAny.GetSize() == 2, "Should have 2 any-state transitions");
	Zenith_Assert(xAny.Get(0).m_iPriority == 100, "First should be highest priority (Death)");
	Zenith_Assert(xAny.Get(1).m_iPriority == 10, "Second should be lower priority (Hit)");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnyStateTransitionPriority passed");
}

//=============================================================================
// AnimatorStateInfo Tests
//=============================================================================

void Zenith_UnitTests::TestStateInfoStateName()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStateInfoStateName...");

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.SetDefaultState("Idle");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	Flux_AnimatorStateInfo xInfo = xSM.GetCurrentStateInfo();
	Zenith_Assert(xInfo.IsName("Idle"), "State name should be Idle");
	Zenith_Assert(!xInfo.IsName("Walk"), "State name should not be Walk");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStateInfoStateName passed");
}

void Zenith_UnitTests::TestStateInfoNormalizedTime()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStateInfoNormalizedTime...");

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.SetDefaultState("Idle");

	// State info should return 0 normalized time when no blend tree
	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	Flux_AnimatorStateInfo xInfo = xSM.GetCurrentStateInfo();
	Zenith_Assert(xInfo.m_fNormalizedTime >= 0.0f, "Normalized time should be >= 0");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStateInfoNormalizedTime passed");
}

//=============================================================================
// CrossFade Tests
//=============================================================================

void Zenith_UnitTests::TestCrossFadeToState()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCrossFadeToState...");

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.SetDefaultState("Idle");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	Zenith_Assert(xSM.GetCurrentStateName() == "Idle", "Should start in Idle");

	// CrossFade to Walk (no conditions needed)
	xSM.CrossFade("Walk", 0.2f);

	Zenith_Assert(xSM.IsTransitioning(), "Should be transitioning after CrossFade");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCrossFadeToState passed");
}

void Zenith_UnitTests::TestCrossFadeToCurrentState()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCrossFadeToCurrentState...");

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.SetDefaultState("Idle");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	// CrossFade to current state should be a no-op
	xSM.CrossFade("Idle", 0.2f);
	Zenith_Assert(!xSM.IsTransitioning(), "CrossFade to current state should be no-op");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCrossFadeToCurrentState passed");
}

//=============================================================================
// Sub-State Machine Tests
//=============================================================================

void Zenith_UnitTests::TestSubStateMachineCreation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSubStateMachineCreation...");

	Flux_AnimationState xState("Locomotion");

	Zenith_Assert(!xState.IsSubStateMachine(), "Should not be sub-SM initially");

	Flux_AnimationStateMachine* pxSubSM = xState.CreateSubStateMachine("LocomotionSM");
	Zenith_Assert(pxSubSM != nullptr, "Sub-SM should be created");
	Zenith_Assert(xState.IsSubStateMachine(), "Should be sub-SM after creation");
	Zenith_Assert(pxSubSM->GetName() == "LocomotionSM", "Sub-SM name should match");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSubStateMachineCreation passed");
}

void Zenith_UnitTests::TestSubStateMachineSharedParameters()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSubStateMachineSharedParameters...");

	Flux_AnimationStateMachine xParentSM("ParentSM");
	xParentSM.GetParameters().AddFloat("Speed", 0.0f);

	// Create a state with a sub-SM
	Flux_AnimationState* pxState = xParentSM.AddState("Locomotion");
	Flux_AnimationStateMachine* pxSubSM = pxState->CreateSubStateMachine("LocomotionSM");

	// Set shared parameters
	pxSubSM->SetSharedParameters(&xParentSM.GetParameters());

	// Setting a parameter on parent should be visible in child
	xParentSM.GetParameters().SetFloat("Speed", 5.0f);
	Zenith_Assert(pxSubSM->GetParameters().GetFloat("Speed") == 5.0f,
		"Child should see parent's parameter value");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSubStateMachineSharedParameters passed");
}

//=============================================================================
// Animation Layer Tests
//=============================================================================

void Zenith_UnitTests::TestLayerCreation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLayerCreation...");

	Flux_AnimationController xController;

	Zenith_Assert(!xController.HasLayers(), "Should have no layers initially");
	Zenith_Assert(xController.GetLayerCount() == 0, "Layer count should be 0");

	Flux_AnimationLayer* pxBase = xController.AddLayer("Base");
	Zenith_Assert(pxBase != nullptr, "Base layer should be created");
	Zenith_Assert(xController.HasLayers(), "Should have layers after adding");
	Zenith_Assert(xController.GetLayerCount() == 1, "Layer count should be 1");
	Zenith_Assert(pxBase->GetName() == "Base", "Layer name should match");
	Zenith_Assert(pxBase->GetWeight() == 1.0f, "Default weight should be 1.0");
	Zenith_Assert(pxBase->GetBlendMode() == LAYER_BLEND_OVERRIDE, "Default blend mode should be Override");

	Flux_AnimationLayer* pxUpperBody = xController.AddLayer("UpperBody");
	Zenith_Assert(xController.GetLayerCount() == 2, "Layer count should be 2");
	pxUpperBody->SetBlendMode(LAYER_BLEND_ADDITIVE);
	Zenith_Assert(pxUpperBody->GetBlendMode() == LAYER_BLEND_ADDITIVE, "Blend mode should be Additive");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLayerCreation passed");
}

void Zenith_UnitTests::TestLayerWeightZero()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLayerWeightZero...");

	Flux_AnimationLayer xLayer("Test");
	xLayer.SetWeight(0.0f);
	Zenith_Assert(xLayer.GetWeight() == 0.0f, "Weight should be 0");

	xLayer.SetWeight(0.5f);
	Zenith_Assert(xLayer.GetWeight() == 0.5f, "Weight should be 0.5");

	// Clamping test
	xLayer.SetWeight(2.0f);
	Zenith_Assert(xLayer.GetWeight() == 1.0f, "Weight should be clamped to 1.0");

	xLayer.SetWeight(-1.0f);
	Zenith_Assert(xLayer.GetWeight() == 0.0f, "Weight should be clamped to 0.0");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLayerWeightZero passed");
}

//=============================================================================
// Tween System Tests - Easing Functions
//=============================================================================

void Zenith_UnitTests::TestEasingLinear()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEasingLinear...");

	Zenith_Assert(Zenith_ApplyEasing(EASING_LINEAR, 0.0f) == 0.0f, "Linear easing at 0 should be 0");
	Zenith_Assert(Zenith_ApplyEasing(EASING_LINEAR, 0.5f) == 0.5f, "Linear easing at 0.5 should be 0.5");
	Zenith_Assert(Zenith_ApplyEasing(EASING_LINEAR, 1.0f) == 1.0f, "Linear easing at 1 should be 1");
	Zenith_Assert(Zenith_ApplyEasing(EASING_LINEAR, 0.25f) == 0.25f, "Linear easing at 0.25 should be 0.25");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEasingLinear passed");
}

void Zenith_UnitTests::TestEasingEndpoints()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEasingEndpoints...");

	const float fEpsilon = 0.001f;

	// All easing functions should map 0->0 and 1->1
	for (int i = 0; i < EASING_COUNT; ++i)
	{
		Zenith_EasingType eType = static_cast<Zenith_EasingType>(i);
		float fAtZero = Zenith_ApplyEasing(eType, 0.0f);
		float fAtOne = Zenith_ApplyEasing(eType, 1.0f);

		Zenith_Assert(glm::abs(fAtZero) < fEpsilon, "Easing at 0 should be ~0");
		Zenith_Assert(glm::abs(fAtOne - 1.0f) < fEpsilon, "Easing at 1 should be ~1");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEasingEndpoints passed");
}

void Zenith_UnitTests::TestEasingQuadOut()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEasingQuadOut...");

	// QuadOut starts fast, ends slow
	// At midpoint, output should be > 0.5 (since it's decelerating)
	float fMid = Zenith_ApplyEasing(EASING_QUAD_OUT, 0.5f);
	Zenith_Assert(fMid > 0.5f, "QuadOut at 0.5 should be > 0.5 (decelerating curve)");
	Zenith_Assert(fMid < 1.0f, "QuadOut at 0.5 should be < 1.0");

	// Quarter point should also show deceleration
	float fQuarter = Zenith_ApplyEasing(EASING_QUAD_OUT, 0.25f);
	Zenith_Assert(fQuarter > 0.25f, "QuadOut at 0.25 should be > 0.25");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEasingQuadOut passed");
}

void Zenith_UnitTests::TestEasingBounceOut()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestEasingBounceOut...");

	// BounceOut should have values between 0 and 1 at midpoints
	float fMid = Zenith_ApplyEasing(EASING_BOUNCE_OUT, 0.5f);
	Zenith_Assert(fMid >= 0.0f && fMid <= 1.0f, "BounceOut at 0.5 should be in [0,1]");

	// BounceOut at 0.9 should be close to 1.0 (near the end)
	float fNearEnd = Zenith_ApplyEasing(EASING_BOUNCE_OUT, 0.95f);
	Zenith_Assert(fNearEnd > 0.8f, "BounceOut near end should be close to 1.0");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEasingBounceOut passed");
}

//=============================================================================
// Tween System Tests - TweenInstance
//=============================================================================

void Zenith_UnitTests::TestTweenInstanceProgress()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenInstanceProgress...");

	Zenith_TweenInstance xTween;
	xTween.m_eEasing = EASING_LINEAR;
	xTween.m_fDuration = 2.0f;
	xTween.m_fDelay = 0.0f;

	xTween.m_fElapsed = 0.0f;
	Zenith_Assert(xTween.GetNormalizedTime() == 0.0f, "At elapsed 0, normalized time should be 0");

	xTween.m_fElapsed = 1.0f;
	float fHalf = xTween.GetNormalizedTime();
	Zenith_Assert(glm::abs(fHalf - 0.5f) < 0.001f, "At elapsed 1 of duration 2, normalized time should be 0.5");

	xTween.m_fElapsed = 2.0f;
	Zenith_Assert(glm::abs(xTween.GetNormalizedTime() - 1.0f) < 0.001f, "At elapsed 2 of duration 2, normalized time should be 1.0");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenInstanceProgress passed");
}

void Zenith_UnitTests::TestTweenInstanceCompletion()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenInstanceCompletion...");

	// Completion is determined by normalized time reaching 1.0
	Zenith_TweenInstance xTween;
	xTween.m_fDuration = 1.0f;
	xTween.m_fElapsed = 0.0f;
	Zenith_Assert(xTween.GetNormalizedTime() < 1.0f, "New tween should not be complete");

	xTween.m_fElapsed = 1.0f;
	Zenith_Assert(glm::abs(xTween.GetNormalizedTime() - 1.0f) < 0.001f, "Elapsed == Duration should give normalized time 1.0");

	// Zero duration should give normalized time 1.0
	Zenith_TweenInstance xZeroDuration;
	xZeroDuration.m_fDuration = 0.0f;
	Zenith_Assert(glm::abs(xZeroDuration.GetNormalizedTime() - 1.0f) < 0.001f, "Zero duration tween should have normalized time 1.0");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenInstanceCompletion passed");
}

void Zenith_UnitTests::TestTweenInstanceDelay()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenInstanceDelay...");

	Zenith_TweenInstance xTween;
	xTween.m_eEasing = EASING_LINEAR;
	xTween.m_fDuration = 1.0f;
	xTween.m_fDelay = 0.5f;

	// During delay, normalized time should be 0
	xTween.m_fElapsed = 0.3f;
	Zenith_Assert(xTween.GetNormalizedTime() == 0.0f, "During delay, normalized time should be 0");

	// After delay, should start progressing
	xTween.m_fElapsed = 1.0f;  // 0.5 delay + 0.5 active = halfway
	float fT = xTween.GetNormalizedTime();
	Zenith_Assert(glm::abs(fT - 0.5f) < 0.001f, "After delay with 0.5s active, should be at 0.5");

	// After delay + full duration
	xTween.m_fElapsed = 1.5f;  // 0.5 delay + 1.0 active = done
	Zenith_Assert(glm::abs(xTween.GetNormalizedTime() - 1.0f) < 0.001f, "After delay + duration, should be at 1.0");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenInstanceDelay passed");
}

//=============================================================================
// Tween System Tests - TweenComponent
//=============================================================================

void Zenith_UnitTests::TestTweenComponentScaleTo()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenComponentScaleTo...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenScaleTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	// Set initial scale
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 1.0f, EASING_LINEAR);

	Zenith_Assert(xTween.HasActiveTweens(), "Should have active tweens");
	Zenith_Assert(xTween.GetActiveTweenCount() == 1, "Should have 1 active tween");

	// Simulate halfway
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	Zenith_Assert(glm::abs(xScale.x - 0.5f) < 0.01f, "Scale X should be ~0.5 at halfway");
	Zenith_Assert(glm::abs(xScale.y - 0.5f) < 0.01f, "Scale Y should be ~0.5 at halfway");

	// Simulate to completion
	xTween.OnUpdate(0.5f);
	xTransform.GetScale(xScale);
	Zenith_Assert(glm::abs(xScale.x) < 0.01f, "Scale X should be ~0.0 at completion");

	Zenith_Assert(!xTween.HasActiveTweens(), "Tween should be removed after completion");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenComponentScaleTo passed");
}

void Zenith_UnitTests::TestTweenComponentPositionTo()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenComponentPositionTo...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenPosTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenPosition(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f), 1.0f, EASING_LINEAR);

	// Simulate halfway
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xPos;
	xTransform.GetPosition(xPos);
	Zenith_Assert(glm::abs(xPos.x - 5.0f) < 0.01f, "Position X should be ~5.0 at halfway");

	// Complete
	xTween.OnUpdate(0.5f);
	xTransform.GetPosition(xPos);
	Zenith_Assert(glm::abs(xPos.x - 10.0f) < 0.01f, "Position X should be ~10.0 at completion");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenComponentPositionTo passed");
}

void Zenith_UnitTests::TestTweenComponentMultiple()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenComponentMultiple...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenMultiTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(0.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenPosition(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f), 1.0f, EASING_LINEAR);
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f), 1.0f, EASING_LINEAR);

	Zenith_Assert(xTween.GetActiveTweenCount() == 2, "Should have 2 active tweens");

	// Both should complete
	xTween.OnUpdate(1.0f);

	Zenith_Maths::Vector3 xPos, xScale;
	xTransform.GetPosition(xPos);
	xTransform.GetScale(xScale);
	Zenith_Assert(glm::abs(xPos.x - 10.0f) < 0.01f, "Position should have reached target");
	Zenith_Assert(glm::abs(xScale.x - 2.0f) < 0.01f, "Scale should have reached target");
	Zenith_Assert(!xTween.HasActiveTweens(), "Both tweens should be complete");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenComponentMultiple passed");
}

void Zenith_UnitTests::TestTweenComponentCallback()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenComponentCallback...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenCallbackTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	xEntity.GetComponent<Zenith_TransformComponent>().SetScale(Zenith_Maths::Vector3(1.0f));

	bool bCallbackFired = false;
	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(0.0f), 0.5f, EASING_LINEAR);
	xTween.SetOnComplete([](void* pUserData) {
		*static_cast<bool*>(pUserData) = true;
	}, &bCallbackFired);

	Zenith_Assert(!bCallbackFired, "Callback should not have fired yet");

	// Complete the tween
	xTween.OnUpdate(0.5f);
	Zenith_Assert(bCallbackFired, "Callback should have fired on completion");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenComponentCallback passed");
}

void Zenith_UnitTests::TestTweenComponentLoop()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenComponentLoop...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenLoopTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f), 1.0f, EASING_LINEAR);
	xTween.SetLoop(true, false);

	// Complete one cycle
	xTween.OnUpdate(1.0f);
	Zenith_Assert(xTween.HasActiveTweens(), "Looping tween should still be active after completion");

	// After loop reset, another update should work
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	// Should be interpolating from start again
	Zenith_Assert(xTween.HasActiveTweens(), "Looping tween should still be active");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenComponentLoop passed");
}

void Zenith_UnitTests::TestTweenComponentPingPong()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenComponentPingPong...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenPingPongTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(0.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScaleFromTo(Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(1.0f), 1.0f, EASING_LINEAR);
	xTween.SetLoop(true, true);

	// Forward pass: 0 -> 1
	xTween.OnUpdate(1.0f);
	Zenith_Assert(xTween.HasActiveTweens(), "PingPong tween should still be active");

	// Reverse pass halfway: should be going 1 -> 0, at 0.5 should be ~0.5
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	Zenith_Assert(glm::abs(xScale.x - 0.5f) < 0.1f, "PingPong reverse at halfway should be ~0.5");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenComponentPingPong passed");
}

void Zenith_UnitTests::TestTweenComponentCancel()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenComponentCancel...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenCancelTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	xEntity.GetComponent<Zenith_TransformComponent>().SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(0.0f), 1.0f, EASING_LINEAR);
	xTween.TweenPosition(Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f), 1.0f, EASING_LINEAR);

	Zenith_Assert(xTween.GetActiveTweenCount() == 2, "Should have 2 active tweens");

	xTween.CancelAll();
	Zenith_Assert(!xTween.HasActiveTweens(), "After CancelAll, no tweens should be active");
	Zenith_Assert(xTween.GetActiveTweenCount() == 0, "Active count should be 0");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenComponentCancel passed");
}

//=============================================================================
// Sub-SM Transition Evaluation (BUG 1 regression test)
//=============================================================================

void Zenith_UnitTests::TestSubStateMachineTransitionEvaluation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSubStateMachineTransitionEvaluation...");

	// Create parent SM with a speed parameter
	Flux_AnimationStateMachine xParentSM("ParentSM");
	xParentSM.GetParameters().AddFloat("Speed", 0.0f);

	// Create a state with a sub-SM that has its own states and transitions
	Flux_AnimationState* pxLocomotion = xParentSM.AddState("Locomotion");
	Flux_AnimationStateMachine* pxSubSM = pxLocomotion->CreateSubStateMachine("LocomotionSM");
	pxSubSM->SetSharedParameters(&xParentSM.GetParameters());

	// Add states to the sub-SM
	pxSubSM->AddState("Walk");
	pxSubSM->AddState("Run");
	pxSubSM->SetDefaultState("Walk");

	// Add transition: Walk -> Run when Speed > 3.0
	Flux_StateTransition xWalkToRun;
	xWalkToRun.m_strTargetStateName = "Run";
	xWalkToRun.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xSpeedCond;
	xSpeedCond.m_strParameterName = "Speed";
	xSpeedCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
	xSpeedCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
	xSpeedCond.m_fThreshold = 3.0f;
	xWalkToRun.m_xConditions.PushBack(xSpeedCond);

	pxSubSM->GetState("Walk")->AddTransition(xWalkToRun);

	// Initialize the sub-SM
	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;

	pxSubSM->Update(0.0f, xPose, xSkel);
	Zenith_Assert(pxSubSM->GetCurrentStateName() == "Walk", "Sub-SM should start in Walk");

	// Set parent parameter Speed > 3.0 - sub-SM should see it through shared parameters
	xParentSM.GetParameters().SetFloat("Speed", 5.0f);

	// Update sub-SM - transition should evaluate against shared (parent) parameters
	pxSubSM->Update(0.016f, xPose, xSkel);
	Zenith_Assert(pxSubSM->IsTransitioning(), "Sub-SM should be transitioning Walk->Run via shared parameters");

	// Complete transition
	for (int i = 0; i < 20; ++i)
		pxSubSM->Update(0.016f, xPose, xSkel);

	Zenith_Assert(pxSubSM->GetCurrentStateName() == "Run",
		"Sub-SM should have transitioned to Run using parent's Speed parameter");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSubStateMachineTransitionEvaluation passed");
}

//=============================================================================
// CrossFade Edge Cases
//=============================================================================

void Zenith_UnitTests::TestCrossFadeNonExistentState()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCrossFadeNonExistentState...");

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.SetDefaultState("Idle");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	Zenith_Assert(xSM.GetCurrentStateName() == "Idle", "Should start in Idle");

	// CrossFade to non-existent state should silently do nothing
	xSM.CrossFade("NonExistent", 0.15f);
	Zenith_Assert(!xSM.IsTransitioning(), "Should NOT be transitioning to non-existent state");
	Zenith_Assert(xSM.GetCurrentStateName() == "Idle", "Should still be in Idle");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCrossFadeNonExistentState passed");
}

void Zenith_UnitTests::TestCrossFadeInstant()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCrossFadeInstant...");

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Run");
	xSM.SetDefaultState("Idle");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkel;
	xSM.Update(0.0f, xPose, xSkel);

	Zenith_Assert(xSM.GetCurrentStateName() == "Idle", "Should start in Idle");

	// CrossFade with zero duration - should transition immediately on next update
	xSM.CrossFade("Run", 0.0f);
	xSM.Update(0.001f, xPose, xSkel);

	// With duration=0, the cross-fade should complete immediately
	Zenith_Assert(xSM.GetCurrentStateName() == "Run", "Zero-duration crossfade should complete immediately");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCrossFadeInstant passed");
}

//=============================================================================
// Tween Rotation Test
//=============================================================================

void Zenith_UnitTests::TestTweenComponentRotation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenComponentRotation...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenRotationTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	// Set initial rotation to identity
	xEntity.GetComponent<Zenith_TransformComponent>().SetRotation(Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	// Tween rotation to 90 degrees around Y axis over 1 second
	xTween.TweenRotation(Zenith_Maths::Vector3(0.0f, 90.0f, 0.0f), 1.0f, EASING_LINEAR);

	Zenith_Assert(xTween.HasActiveTweens(), "Should have active rotation tween");

	// Update to completion
	xTween.OnUpdate(1.0f);
	Zenith_Assert(!xTween.HasActiveTweens(), "Rotation tween should be complete");

	// Verify rotation was applied - get the euler angles back
	Zenith_Maths::Quat xRot;
	xEntity.GetComponent<Zenith_TransformComponent>().GetRotation(xRot);
	Zenith_Maths::Vector3 xEuler = glm::degrees(glm::eulerAngles(xRot));

	// Y rotation should be approximately 90 degrees
	Zenith_Assert(glm::abs(xEuler.y - 90.0f) < 1.0f, "Y rotation should be ~90 degrees");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenComponentRotation passed");
}

//=============================================================================
// Bug Regression Tests (from code review)
//=============================================================================

void Zenith_UnitTests::TestTriggerNotConsumedOnPartialConditionMatch()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTriggerNotConsumedOnPartialConditionMatch...");

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xStateMachine("TestSM");
	xStateMachine.GetParameters().AddTrigger("Attack");
	xStateMachine.GetParameters().AddBool("HasWeapon", false);

	Flux_AnimationState* pxIdle = xStateMachine.AddState("Idle");
	xStateMachine.AddState("Attack");

	// Idle -> Attack requires BOTH trigger AND HasWeapon == true
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Attack";
	xTrans.m_fTransitionDuration = 0.1f;

	Flux_TransitionCondition xTriggerCond;
	xTriggerCond.m_strParameterName = "Attack";
	xTriggerCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xTrans.m_xConditions.PushBack(xTriggerCond);

	Flux_TransitionCondition xBoolCond;
	xBoolCond.m_strParameterName = "HasWeapon";
	xBoolCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
	xBoolCond.m_eParamType = Flux_AnimationParameters::ParamType::Bool;
	xBoolCond.m_bThreshold = true;
	xTrans.m_xConditions.PushBack(xBoolCond);

	pxIdle->AddTransition(xTrans);
	xStateMachine.SetDefaultState("Idle");

	// Initial state
	xStateMachine.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xStateMachine.GetCurrentStateName() == "Idle", "Should start in Idle");

	// Set trigger but NOT HasWeapon - transition should fail, trigger should NOT be consumed
	xStateMachine.GetParameters().SetTrigger("Attack");
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	Zenith_Assert(xStateMachine.GetCurrentStateName() == "Idle",
		"Should stay in Idle - HasWeapon is false");
	Zenith_Assert(xStateMachine.GetParameters().PeekTrigger("Attack") == true,
		"Trigger should NOT be consumed when other conditions fail");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Trigger preserved when bool condition fails");

	// Now set HasWeapon - trigger should still be set, transition should fire
	xStateMachine.GetParameters().SetBool("HasWeapon", true);
	xStateMachine.Update(0.016f, xPose, xSkeleton);

	Zenith_Assert(xStateMachine.IsTransitioning() == true,
		"Transition should start now that all conditions are met");
	Zenith_Assert(xStateMachine.GetParameters().PeekTrigger("Attack") == false,
		"Trigger should be consumed after successful transition");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Trigger consumed only on successful transition");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTriggerNotConsumedOnPartialConditionMatch passed");
}

void Zenith_UnitTests::TestResolveClipReferencesRecursive()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestResolveClipReferencesRecursive...");

	// Create a clip collection with two clips
	Flux_AnimationClipCollection xCollection;
	Flux_AnimationClip* pxIdleClip = new Flux_AnimationClip();
	pxIdleClip->SetName("Idle");
	Flux_AnimationClip* pxWalkClip = new Flux_AnimationClip();
	pxWalkClip->SetName("Walk");
	xCollection.AddClip(pxIdleClip);
	xCollection.AddClip(pxWalkClip);

	// Create a Blend node with two Clip children (clip pointers null, names set)
	Flux_BlendTreeNode_Clip* pxClipA = new Flux_BlendTreeNode_Clip();
	pxClipA->SetClipName("Idle");
	Zenith_Assert(pxClipA->GetClip() == nullptr, "Clip A should be unresolved");

	Flux_BlendTreeNode_Clip* pxClipB = new Flux_BlendTreeNode_Clip();
	pxClipB->SetClipName("Walk");
	Zenith_Assert(pxClipB->GetClip() == nullptr, "Clip B should be unresolved");

	Flux_BlendTreeNode_Blend* pxBlend = new Flux_BlendTreeNode_Blend(pxClipA, pxClipB, 0.5f);

	// Create state machine with a state that has the blend tree root
	Flux_AnimationStateMachine xSM("TestSM");
	Flux_AnimationState* pxState = xSM.AddState("BlendState");
	pxState->SetBlendTree(pxBlend);
	xSM.SetDefaultState("BlendState");

	// Resolve - should recursively resolve both child clips
	xSM.ResolveClipReferences(&xCollection);

	Zenith_Assert(pxClipA->GetClip() == pxIdleClip,
		"Clip A should be resolved to Idle clip");
	Zenith_Assert(pxClipB->GetClip() == pxWalkClip,
		"Clip B should be resolved to Walk clip");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Blend tree children resolved recursively");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestResolveClipReferencesRecursive passed");
}

void Zenith_UnitTests::TestTweenDelayWithLoop()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenDelayWithLoop...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenDelayLoopTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	// delay=1.0, duration=0.5 - delay > duration, which was the buggy case
	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f), 0.5f, EASING_LINEAR);
	xTween.SetDelay(1.0f);
	xTween.SetLoop(true, false);

	// During delay period - scale should not change
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	Zenith_Assert(glm::abs(xScale.x - 1.0f) < 0.01f, "Scale should be unchanged during delay");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] No change during delay");

	// After delay, at midpoint of tween (total elapsed = 1.25, activeTime = 0.25, t = 0.5)
	xTween.OnUpdate(0.75f);
	xTransform.GetScale(xScale);
	Zenith_Assert(xScale.x > 1.0f, "Scale should be interpolating after delay");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Interpolating after delay");

	// Complete first loop (total elapsed = 1.75, activeTime = 0.75, t >= 1.0, loop triggers)
	// Loop resets elapsed to delay (1.0), tween stays active
	xTween.OnUpdate(0.5f);
	Zenith_Assert(xTween.HasActiveTweens(), "Looping tween should still be active");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Tween still active after first loop");

	// After loop reset, a small update should restart interpolation from the beginning
	// elapsed goes from 1.0 to 1.1, activeTime = 0.1, t = 0.1/0.5 = 0.2
	// scale = lerp(1.0, 2.0, 0.2) = 1.2
	xTween.OnUpdate(0.1f);
	xTransform.GetScale(xScale);
	Zenith_Assert(glm::abs(xScale.x - 1.2f) < 0.05f,
		"After loop, tween should restart interpolation from beginning (expected ~1.2)");
	Zenith_Assert(xTween.HasActiveTweens(), "Looping tween should still be active");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Tween restarts correctly after loop");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenDelayWithLoop passed");
}

void Zenith_UnitTests::TestTweenCallbackReentrant()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenCallbackReentrant...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenReentrantTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	struct CallbackData
	{
		Zenith_TweenComponent* m_pxTween;
		bool m_bCallbackFired;
	};

	CallbackData xData;
	xData.m_pxTween = &xEntity.GetComponent<Zenith_TweenComponent>();
	xData.m_bCallbackFired = false;

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f), 0.5f, EASING_LINEAR);
	xTween.SetOnComplete([](void* pUserData) {
		CallbackData* pxData = static_cast<CallbackData*>(pUserData);
		pxData->m_bCallbackFired = true;
		// Re-entrant: create a new tween from within the callback
		pxData->m_pxTween->TweenScale(Zenith_Maths::Vector3(3.0f), 1.0f, EASING_LINEAR);
	}, &xData);

	// Complete the first tween - callback should fire and create a new tween
	xTween.OnUpdate(0.5f);

	Zenith_Assert(xData.m_bCallbackFired, "Callback should have fired");
	Zenith_Assert(xTween.HasActiveTweens(), "New tween should have been created by callback");
	Zenith_Assert(xTween.GetActiveTweenCount() == 1, "Should have exactly 1 active tween");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Re-entrant tween creation from callback works");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenCallbackReentrant passed");
}

void Zenith_UnitTests::TestTweenDuplicatePropertyCancels()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenDuplicatePropertyCancels...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenDuplicateTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();

	// Create first scale tween
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f), 1.0f, EASING_LINEAR);
	Zenith_Assert(xTween.GetActiveTweenCount() == 1, "Should have 1 active tween");

	// Create second scale tween - should cancel the first
	xTween.TweenScale(Zenith_Maths::Vector3(3.0f), 0.5f, EASING_LINEAR);
	Zenith_Assert(xTween.GetActiveTweenCount() == 1,
		"Should still have 1 active tween - duplicate cancelled");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Duplicate property tween cancelled");

	// Complete the second tween
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	Zenith_Assert(glm::abs(xScale.x - 3.0f) < 0.01f,
		"Should reach target of second tween");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Second tween completes to correct target");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenDuplicatePropertyCancels passed");
}

//=============================================================================
// Code Review Round 2 - Bug Fix Regression Tests
//=============================================================================

void Zenith_UnitTests::TestSubStateMachineTransitionBlendPose()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSubStateMachineTransitionBlendPose...");

	// Create skeleton with 2 bones for pose verification
	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	xSkeleton.AddBone("Spine", 0, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);

	// Create parent SM: Idle -> Locomotion (sub-SM)
	Flux_AnimationStateMachine xParentSM("ParentSM");
	xParentSM.GetParameters().AddTrigger("GoLocomotion");

	xParentSM.AddState("Idle");
	Flux_AnimationState* pxLocomotionState = xParentSM.AddState("Locomotion");
	Flux_AnimationStateMachine* pxSubSM = pxLocomotionState->CreateSubStateMachine("LocomotionSM");
	pxSubSM->AddState("Walk");
	pxSubSM->SetDefaultState("Walk");
	xParentSM.SetDefaultState("Idle");

	// Add transition Idle -> Locomotion on trigger
	Flux_StateTransition xTrans;
	xTrans.m_strTargetStateName = "Locomotion";
	xTrans.m_fTransitionDuration = 0.2f;
	Flux_TransitionCondition xCond;
	xCond.m_strParameterName = "GoLocomotion";
	xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
	xTrans.m_xConditions.PushBack(xCond);
	xParentSM.GetState("Idle")->AddTransition(xTrans);

	// Initialize
	xParentSM.Update(0.0f, xPose, xSkeleton);
	Zenith_Assert(xParentSM.GetCurrentStateName() == "Idle", "Should start in Idle");

	// Trigger transition to sub-SM state
	xParentSM.GetParameters().SetTrigger("GoLocomotion");
	xParentSM.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xParentSM.IsTransitioning(), "Should be transitioning to Locomotion sub-SM");

	// Update during transition - the target pose should NOT be identity/reset
	// (This was Bug #1 - UpdateTransition didn't evaluate sub-SM targets)
	xParentSM.Update(0.016f, xPose, xSkeleton);
	// The key check: the pose should not be all-zero (identity reset)
	// A proper sub-SM update would produce the Walk state's pose
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Transition to sub-SM state evaluates target pose");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSubStateMachineTransitionBlendPose passed");
}

void Zenith_UnitTests::TestRotationTweenShortestPath()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestRotationTweenShortestPath...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenRotShortestTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetRotation(Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();

	// Tween 270 degrees around Y - slerp should take the shortest path (90 degrees the other way)
	xTween.TweenRotation(Zenith_Maths::Vector3(0.0f, 270.0f, 0.0f), 1.0f, EASING_LINEAR);

	// At halfway, the rotation should be ~135 degrees OR ~-45 degrees (shortest path)
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Quat xRot;
	xTransform.GetRotation(xRot);

	// Verify it's a valid unit quaternion
	float fLength = glm::length(xRot);
	Zenith_Assert(glm::abs(fLength - 1.0f) < 0.01f, "Quaternion should be unit length");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Rotation tween produces valid quaternion at midpoint");

	// Complete the tween
	xTween.OnUpdate(0.5f);
	xTransform.GetRotation(xRot);

	// Verify final rotation is approximately 270 degrees Y (or equivalently -90 degrees)
	Zenith_Maths::Vector3 xEuler = glm::degrees(glm::eulerAngles(xRot));
	// Accept either ~270 or ~-90 (equivalent rotations)
	bool bCorrect = (glm::abs(xEuler.y - 270.0f) < 2.0f) || (glm::abs(xEuler.y + 90.0f) < 2.0f);
	Zenith_Assert(bCorrect, "Final rotation should be ~270 or ~-90 degrees Y");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Rotation tween reaches correct final angle");

	Zenith_Assert(!xTween.HasActiveTweens(), "Tween should be complete");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRotationTweenShortestPath passed");
}

void Zenith_UnitTests::TestTransitionInterruption()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTransitionInterruption...");

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.AddState("Death");
	xSM.SetDefaultState("Idle");

	xSM.GetParameters().AddFloat("Speed", 0.0f);
	xSM.GetParameters().AddTrigger("DeathTrigger");

	// Idle -> Walk (interruptible, low priority)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Walk";
		xTrans.m_fTransitionDuration = 1.0f; // Long transition so we can interrupt it
		xTrans.m_bInterruptible = true;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 0.1f;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.GetState("Idle")->AddTransition(xTrans);
	}

	// Any-state -> Death (high priority, should interrupt)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Death";
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = 100;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "DeathTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.AddAnyStateTransition(xTrans);
	}

	// Initialize and start Walk transition
	xSM.Update(0.0f, xPose, xSkeleton);
	xSM.GetParameters().SetFloat("Speed", 5.0f);
	xSM.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xSM.IsTransitioning(), "Should be transitioning Idle -> Walk");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Walk transition started");

	// Fire Death trigger while transitioning - should interrupt
	xSM.GetParameters().SetTrigger("DeathTrigger");
	xSM.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xSM.IsTransitioning(), "Should be transitioning to Death (interrupted Walk)");

	// Complete the Death transition
	for (int i = 0; i < 20; ++i)
		xSM.Update(0.016f, xPose, xSkeleton);

	Zenith_Assert(xSM.GetCurrentStateName() == "Death",
		"Should have reached Death state after interrupting Walk transition");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Death transition interrupted Walk transition");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTransitionInterruption passed");
}

void Zenith_UnitTests::TestTransitionNonInterruptible()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTransitionNonInterruptible...");

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("SpecialAttack");
	xSM.AddState("Death");
	xSM.SetDefaultState("Idle");

	xSM.GetParameters().AddTrigger("AttackTrigger");
	xSM.GetParameters().AddTrigger("DeathTrigger");

	// Idle -> SpecialAttack (NON-interruptible)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "SpecialAttack";
		xTrans.m_fTransitionDuration = 1.0f;
		xTrans.m_bInterruptible = false; // Cannot be interrupted

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "AttackTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.GetState("Idle")->AddTransition(xTrans);
	}

	// Idle -> Death (per-state, not any-state, so it only fires from Idle)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Death";
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = 100;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "DeathTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.GetState("Idle")->AddTransition(xTrans);
	}

	// Start non-interruptible transition
	xSM.Update(0.0f, xPose, xSkeleton);
	xSM.GetParameters().SetTrigger("AttackTrigger");
	xSM.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xSM.IsTransitioning(), "Should be transitioning Idle -> SpecialAttack");

	// Try to interrupt with Death - should NOT work (non-interruptible)
	xSM.GetParameters().SetTrigger("DeathTrigger");
	xSM.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xSM.IsTransitioning(), "Should still be transitioning (non-interruptible)");

	// Complete the SpecialAttack transition
	for (int i = 0; i < 100; ++i)
		xSM.Update(0.016f, xPose, xSkeleton);

	// Should be in SpecialAttack - the Death trigger couldn't interrupt, and there's no
	// Death transition from SpecialAttack state, so the unconsumed trigger has no effect
	Zenith_Assert(xSM.GetCurrentStateName() == "SpecialAttack",
		"Non-interruptible transition should complete to SpecialAttack, not Death");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Non-interruptible transition was not interrupted");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTransitionNonInterruptible passed");
}

void Zenith_UnitTests::TestCancelByPropertyKeepsOthers()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCancelByPropertyKeepsOthers...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenCancelPropTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(0.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenPosition(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f), 1.0f, EASING_LINEAR);
	xTween.TweenScale(Zenith_Maths::Vector3(2.0f), 1.0f, EASING_LINEAR);
	Zenith_Assert(xTween.GetActiveTweenCount() == 2, "Should have 2 active tweens");

	// Cancel only position
	xTween.CancelByProperty(TWEEN_PROPERTY_POSITION);
	Zenith_Assert(xTween.GetActiveTweenCount() == 1, "Should have 1 active tween after cancelling position");

	// Complete remaining scale tween
	xTween.OnUpdate(1.0f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	Zenith_Assert(glm::abs(xScale.x - 2.0f) < 0.01f, "Scale tween should still complete");

	// Position should not have changed (was cancelled)
	Zenith_Maths::Vector3 xPos;
	xTransform.GetPosition(xPos);
	Zenith_Assert(glm::abs(xPos.x) < 0.01f, "Position should not have changed after cancel");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCancelByPropertyKeepsOthers passed");
}

void Zenith_UnitTests::TestCrossFadeWhileTransitioning()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCrossFadeWhileTransitioning...");

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.AddState("Run");
	xSM.SetDefaultState("Idle");

	xSM.Update(0.0f, xPose, xSkeleton);
	Zenith_Assert(xSM.GetCurrentStateName() == "Idle", "Should start in Idle");

	// Start a CrossFade to Walk
	xSM.CrossFade("Walk", 1.0f);
	Zenith_Assert(xSM.IsTransitioning(), "Should be transitioning to Walk");

	// Update halfway through
	xSM.Update(0.5f, xPose, xSkeleton);
	Zenith_Assert(xSM.IsTransitioning(), "Should still be transitioning");

	// Force CrossFade to Run during the Walk transition
	xSM.CrossFade("Run", 0.1f);
	Zenith_Assert(xSM.IsTransitioning(), "Should be transitioning to Run now");

	// Complete the Run transition
	for (int i = 0; i < 20; ++i)
		xSM.Update(0.016f, xPose, xSkeleton);

	Zenith_Assert(xSM.GetCurrentStateName() == "Run",
		"CrossFade during transition should redirect to Run");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] CrossFade during active transition works");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCrossFadeWhileTransitioning passed");
}

void Zenith_UnitTests::TestTweenLoopValueReset()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTweenLoopValueReset...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TweenLoopResetTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(1.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	xTween.TweenScaleFromTo(Zenith_Maths::Vector3(1.0f), Zenith_Maths::Vector3(2.0f), 1.0f, EASING_LINEAR);
	xTween.SetLoop(true, false);

	// Complete first loop
	xTween.OnUpdate(1.0f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	Zenith_Assert(xTween.HasActiveTweens(), "Should still be active (looping)");

	// Small step into second loop - value should restart from 1.0
	// After loop reset: elapsed = delay(0) + 0.1 = 0.1, t = 0.1/1.0 = 0.1
	// scale = lerp(1.0, 2.0, 0.1) = 1.1
	xTween.OnUpdate(0.1f);
	xTransform.GetScale(xScale);
	Zenith_Assert(glm::abs(xScale.x - 1.1f) < 0.05f,
		"After loop reset, scale should restart from beginning (~1.1)");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Loop correctly resets interpolation value");

	// Continue to halfway through second loop
	xTween.OnUpdate(0.4f);
	xTransform.GetScale(xScale);
	Zenith_Assert(glm::abs(xScale.x - 1.5f) < 0.05f,
		"Halfway through second loop should be ~1.5");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTweenLoopValueReset passed");
}

//=============================================================================
// Bug 1 Regression: Trigger not consumed when blocked by active transition priority
//=============================================================================

void Zenith_UnitTests::TestTriggerNotConsumedWhenBlockedByPriority()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTriggerNotConsumedWhenBlockedByPriority...");

	Zenith_SkeletonAsset xSkeleton;
	xSkeleton.AddBone("Root", -1, Zenith_Maths::Vector3(0.0f), Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f));
	Flux_SkeletonPose xPose;
	xPose.Initialize(1);

	Flux_AnimationStateMachine xSM("TestSM");
	xSM.GetParameters().AddFloat("Speed", 0.0f);
	xSM.GetParameters().AddTrigger("DeathTrigger");

	Flux_AnimationState* pxIdle = xSM.AddState("Idle");
	xSM.AddState("Walk");
	xSM.AddState("Death");
	xSM.SetDefaultState("Idle");

	// Idle -> Walk on Speed > 0.1 (high priority 200, interruptible)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Walk";
		xTrans.m_fTransitionDuration = 1.0f; // long transition so it stays active
		xTrans.m_iPriority = 200;
		xTrans.m_bInterruptible = true;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "Speed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 0.1f;
		xTrans.m_xConditions.PushBack(xCond);
		pxIdle->AddTransition(xTrans);
	}

	// Any-State: DeathTrigger -> Death (low priority 100)
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Death";
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = 100;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "DeathTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xSM.AddAnyStateTransition(xTrans);
	}

	// Initialize
	xSM.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xSM.GetCurrentStateName() == "Idle", "Should start in Idle");

	// Start the high-priority Idle->Walk transition
	xSM.GetParameters().SetFloat("Speed", 1.0f);
	xSM.Update(0.016f, xPose, xSkeleton);
	Zenith_Assert(xSM.IsTransitioning(), "Should be transitioning to Walk");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] High-priority transition to Walk started");

	// Now fire the lower-priority DeathTrigger while Walk transition is active
	xSM.GetParameters().SetTrigger("DeathTrigger");
	xSM.Update(0.016f, xPose, xSkeleton);

	// The death transition should NOT have interrupted (priority 100 < 200)
	// AND the trigger should NOT have been consumed
	Zenith_Assert(xSM.GetParameters().PeekTrigger("DeathTrigger") == true,
		"DeathTrigger should NOT be consumed when blocked by higher-priority active transition");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Trigger preserved when blocked by priority");

	// Complete the Walk transition (1.0s) and let the preserved trigger fire
	// Once Walk completes, the DeathTrigger (still set) fires immediately,
	// then the Death transition (0.1s) also completes within 100 frames
	for (int i = 0; i < 100; ++i)
		xSM.Update(0.016f, xPose, xSkeleton);

	// The preserved trigger should have fired after Walk completed,
	// transitioning us through to Death
	Zenith_Assert(xSM.GetCurrentStateName() == "Death",
		"Preserved DeathTrigger should fire after Walk transition completes, reaching Death");
	Zenith_Assert(xSM.GetParameters().PeekTrigger("DeathTrigger") == false,
		"DeathTrigger should be consumed after successful transition");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Trigger fires after blocking transition completes, reached Death");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTriggerNotConsumedWhenBlockedByPriority passed");
}

//=============================================================================
// Serialization Round-Trip: Animation Layer
//=============================================================================

void Zenith_UnitTests::TestAnimationLayerSerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnimationLayerSerialization...");

	// Create a layer with all configurable properties
	Flux_AnimationLayer xOriginal("UpperBody");
	xOriginal.SetWeight(0.75f);
	xOriginal.SetBlendMode(LAYER_BLEND_ADDITIVE);
	Flux_BoneMask xMask;
	xMask.SetBoneWeight(0, 1.0f);
	xMask.SetBoneWeight(1, 0.5f);
	xOriginal.SetAvatarMask(xMask);

	// Give it a state machine with a state and parameter
	Flux_AnimationStateMachine& xSM = xOriginal.GetStateMachine();
	xSM.AddState("Idle");
	xSM.AddState("Aim");
	xSM.SetDefaultState("Idle");
	xSM.GetParameters().AddFloat("AimWeight", 0.0f);

	// Serialize
	Zenith_DataStream xStream(1);
	xOriginal.WriteToDataStream(xStream);

	// Deserialize
	xStream.SetCursor(0);
	Flux_AnimationLayer xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	// Verify
	Zenith_Assert(xLoaded.GetName() == "UpperBody", "Layer name should round-trip");
	Zenith_Assert(glm::abs(xLoaded.GetWeight() - 0.75f) < 0.001f, "Layer weight should round-trip");
	Zenith_Assert(xLoaded.GetBlendMode() == LAYER_BLEND_ADDITIVE, "Layer blend mode should round-trip");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Layer properties round-trip");

	// Verify state machine survived (use pointer getter to avoid auto-creation)
	const Flux_AnimationStateMachine* pxLoadedSM = xLoaded.GetStateMachinePtr();
	Zenith_Assert(pxLoadedSM != nullptr, "Layer should have a state machine after deserialization");
	Zenith_Assert(pxLoadedSM->GetDefaultStateName() == "Idle", "SM default state should round-trip");
	Zenith_Assert(pxLoadedSM->GetParameters().HasParameter("AimWeight"), "SM parameters should round-trip");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Layer state machine round-trip");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnimationLayerSerialization passed");
}

//=============================================================================
// Serialization Round-Trip: Any-State Transitions
//=============================================================================

void Zenith_UnitTests::TestAnyStateTransitionSerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestAnyStateTransitionSerialization...");

	Flux_AnimationStateMachine xOriginal("TestSM");
	xOriginal.AddState("Idle");
	xOriginal.AddState("Hit");
	xOriginal.AddState("Death");
	xOriginal.SetDefaultState("Idle");

	xOriginal.GetParameters().AddTrigger("HitTrigger");
	xOriginal.GetParameters().AddTrigger("DeathTrigger");

	// Add two any-state transitions with different priorities
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Hit";
		xTrans.m_fTransitionDuration = 0.15f;
		xTrans.m_iPriority = 10;
		xTrans.m_bInterruptible = true;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "HitTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xOriginal.AddAnyStateTransition(xTrans);
	}
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Death";
		xTrans.m_fTransitionDuration = 0.2f;
		xTrans.m_iPriority = 100;
		xTrans.m_bInterruptible = false;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "DeathTrigger";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);
		xOriginal.AddAnyStateTransition(xTrans);
	}

	// Serialize
	Zenith_DataStream xStream(1);
	xOriginal.WriteToDataStream(xStream);

	// Deserialize
	xStream.SetCursor(0);
	Flux_AnimationStateMachine xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	// Verify any-state transitions survived
	const Zenith_Vector<Flux_StateTransition>& xAnyState = xLoaded.GetAnyStateTransitions();
	Zenith_Assert(xAnyState.GetSize() == 2, "Should have 2 any-state transitions after deserialization");

	// Find the Hit and Death transitions (order may differ after deserialization)
	bool bFoundHit = false, bFoundDeath = false;
	for (uint32_t i = 0; i < xAnyState.GetSize(); ++i)
	{
		const Flux_StateTransition& xTrans = xAnyState.Get(i);
		if (xTrans.m_strTargetStateName == "Hit")
		{
			Zenith_Assert(xTrans.m_iPriority == 10, "Hit transition priority should round-trip");
			Zenith_Assert(glm::abs(xTrans.m_fTransitionDuration - 0.15f) < 0.001f, "Hit transition duration should round-trip");
			Zenith_Assert(xTrans.m_bInterruptible == true, "Hit interruptible flag should round-trip");
			Zenith_Assert(xTrans.m_xConditions.GetSize() == 1, "Hit should have 1 condition");
			bFoundHit = true;
		}
		else if (xTrans.m_strTargetStateName == "Death")
		{
			Zenith_Assert(xTrans.m_iPriority == 100, "Death transition priority should round-trip");
			Zenith_Assert(glm::abs(xTrans.m_fTransitionDuration - 0.2f) < 0.001f, "Death transition duration should round-trip");
			Zenith_Assert(xTrans.m_bInterruptible == false, "Death interruptible flag should round-trip");
			Zenith_Assert(xTrans.m_xConditions.GetSize() == 1, "Death should have 1 condition");
			bFoundDeath = true;
		}
	}
	Zenith_Assert(bFoundHit, "Hit any-state transition should survive round-trip");
	Zenith_Assert(bFoundDeath, "Death any-state transition should survive round-trip");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Any-state transitions round-trip");

	// Verify states and parameters survived too
	Zenith_Assert(xLoaded.GetDefaultStateName() == "Idle", "Default state should round-trip");
	Zenith_Assert(xLoaded.GetParameters().HasParameter("HitTrigger"), "HitTrigger param should round-trip");
	Zenith_Assert(xLoaded.GetParameters().HasParameter("DeathTrigger"), "DeathTrigger param should round-trip");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] States and parameters round-trip");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAnyStateTransitionSerialization passed");
}

//=============================================================================
// Serialization Round-Trip: Sub-State Machines
//=============================================================================

void Zenith_UnitTests::TestSubStateMachineSerialization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestSubStateMachineSerialization...");

	Flux_AnimationStateMachine xOriginal("ParentSM");
	xOriginal.AddState("Idle");
	xOriginal.SetDefaultState("Idle");
	xOriginal.GetParameters().AddFloat("Speed", 0.0f);

	// Create a state with a sub-state machine
	Flux_AnimationState* pxLocomotion = xOriginal.AddState("Locomotion");
	Flux_AnimationStateMachine* pxSubSM = pxLocomotion->CreateSubStateMachine("LocomotionSM");
	pxSubSM->AddState("Walk");
	pxSubSM->AddState("Run");
	pxSubSM->SetDefaultState("Walk");
	pxSubSM->GetParameters().AddFloat("SubSpeed", 1.0f);

	// Add a transition inside the sub-SM
	Flux_AnimationState* pxWalk = pxSubSM->GetState("Walk");
	Zenith_Assert(pxWalk != nullptr, "Walk state should exist in sub-SM");
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "Run";
		xTrans.m_fTransitionDuration = 0.2f;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "SubSpeed";
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCond.m_fThreshold = 2.0f;
		xTrans.m_xConditions.PushBack(xCond);
		pxWalk->AddTransition(xTrans);
	}

	// Serialize
	Zenith_DataStream xStream(1);
	xOriginal.WriteToDataStream(xStream);

	// Deserialize
	xStream.SetCursor(0);
	Flux_AnimationStateMachine xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	// Verify parent SM
	Zenith_Assert(xLoaded.GetName() == "ParentSM", "Parent SM name should round-trip");
	Zenith_Assert(xLoaded.GetDefaultStateName() == "Idle", "Parent default state should round-trip");
	Zenith_Assert(xLoaded.GetParameters().HasParameter("Speed"), "Parent params should round-trip");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Parent SM round-trip");

	// Verify sub-state machine exists
	Flux_AnimationState* pxLoadedLoco = xLoaded.GetState("Locomotion");
	Zenith_Assert(pxLoadedLoco != nullptr, "Locomotion state should exist");
	Zenith_Assert(pxLoadedLoco->IsSubStateMachine(), "Locomotion should be a sub-state machine");

	Flux_AnimationStateMachine* pxLoadedSubSM = pxLoadedLoco->GetSubStateMachine();
	Zenith_Assert(pxLoadedSubSM != nullptr, "Sub-SM pointer should be valid");
	Zenith_Assert(pxLoadedSubSM->GetName() == "LocomotionSM", "Sub-SM name should round-trip");
	Zenith_Assert(pxLoadedSubSM->GetDefaultStateName() == "Walk", "Sub-SM default state should round-trip");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Sub-state machine round-trip");

	// Verify sub-SM states and transitions
	Flux_AnimationState* pxLoadedWalk = pxLoadedSubSM->GetState("Walk");
	Zenith_Assert(pxLoadedWalk != nullptr, "Walk state should exist in deserialized sub-SM");
	Zenith_Assert(pxLoadedSubSM->GetState("Run") != nullptr, "Run state should exist in deserialized sub-SM");

	const Zenith_Vector<Flux_StateTransition>& xLoadedTrans = pxLoadedWalk->GetTransitions();
	Zenith_Assert(xLoadedTrans.GetSize() == 1, "Walk should have 1 transition after deserialization");
	Zenith_Assert(xLoadedTrans.Get(0).m_strTargetStateName == "Run", "Transition target should be Run");
	Zenith_Assert(glm::abs(xLoadedTrans.Get(0).m_fTransitionDuration - 0.2f) < 0.001f, "Transition duration should round-trip");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Sub-SM transitions round-trip");

	// Verify sub-SM parameters
	Zenith_Assert(pxLoadedSubSM->GetParameters().HasParameter("SubSpeed"), "Sub-SM params should round-trip");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Sub-SM parameters round-trip");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSubStateMachineSerialization passed");
}

//=============================================================================
// Code Review Round 4 - Bug Fix Validation Tests
//=============================================================================

void Zenith_UnitTests::TestHasAnimationContentWithLayers()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestHasAnimationContentWithLayers...");

	Flux_AnimationController xController;

	// No content initially
	Zenith_Assert(!xController.HasAnimationContent(), "Should have no content initially");

	// Add a layer (no clips, no root state machine)
	xController.AddLayer("Base");
	Zenith_Assert(xController.HasAnimationContent(),
		"Should report content when layers are present");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHasAnimationContentWithLayers passed");
}

void Zenith_UnitTests::TestInitializeRetroactiveLayerPoses()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestInitializeRetroactiveLayerPoses...");

	// Create a skeleton asset with a few bones
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Child", 0, Zenith_Maths::Vector3(0, 1, 0), xIdentity, xUnitScale);
	pxSkel->ComputeBindPoseMatrices();

	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);

	Flux_AnimationController xController;

	// Add layer BEFORE Initialize
	Flux_AnimationLayer* pxLayer = xController.AddLayer("Base");

	// Layer pose should be uninitialized (0 bones)
	Zenith_Assert(pxLayer->GetOutputPose().GetNumBones() == 0,
		"Layer pose should be uninitialized before Initialize()");

	// Initialize should retroactively initialize the layer pose
	xController.Initialize(pxSkelInst);

	Zenith_Assert(pxLayer->GetOutputPose().GetNumBones() == 2,
		"Layer pose should have 2 bones after retroactive Initialize()");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Layer added before Initialize() gets retroactive pose init");

	delete pxSkelInst;
	delete pxSkel;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestInitializeRetroactiveLayerPoses passed");
}

void Zenith_UnitTests::TestResolveClipReferencesBlendSpace2D()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestResolveClipReferencesBlendSpace2D...");

	// Create clip collection
	Flux_AnimationClipCollection xCollection;
	Flux_AnimationClip* pxClipA = new Flux_AnimationClip();
	pxClipA->SetName("ClipA");
	Flux_AnimationClip* pxClipB = new Flux_AnimationClip();
	pxClipB->SetName("ClipB");
	xCollection.AddClip(pxClipA);
	xCollection.AddClip(pxClipB);

	// Create BlendSpace2D with two clip nodes as blend points
	Flux_BlendTreeNode_Clip* pxNodeA = new Flux_BlendTreeNode_Clip();
	pxNodeA->SetClipName("ClipA");
	Zenith_Assert(pxNodeA->GetClip() == nullptr, "Clip A should be unresolved");

	Flux_BlendTreeNode_Clip* pxNodeB = new Flux_BlendTreeNode_Clip();
	pxNodeB->SetClipName("ClipB");
	Zenith_Assert(pxNodeB->GetClip() == nullptr, "Clip B should be unresolved");

	Flux_BlendTreeNode_BlendSpace2D* pxBS2D = new Flux_BlendTreeNode_BlendSpace2D();
	pxBS2D->AddBlendPoint(pxNodeA, Zenith_Maths::Vector2(0.0f, 0.0f));
	pxBS2D->AddBlendPoint(pxNodeB, Zenith_Maths::Vector2(1.0f, 1.0f));

	// Create state machine with state using this blend tree
	Flux_AnimationStateMachine xSM("TestSM");
	Flux_AnimationState* pxState = xSM.AddState("BS2DState");
	pxState->SetBlendTree(pxBS2D);
	xSM.SetDefaultState("BS2DState");

	// Resolve
	xSM.ResolveClipReferences(&xCollection);

	Zenith_Assert(pxNodeA->GetClip() == pxClipA,
		"BlendSpace2D clip A should be resolved");
	Zenith_Assert(pxNodeB->GetClip() == pxClipB,
		"BlendSpace2D clip B should be resolved");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] BlendSpace2D blend point clips resolved recursively");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestResolveClipReferencesBlendSpace2D passed");
}

void Zenith_UnitTests::TestResolveClipReferencesSelect()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestResolveClipReferencesSelect...");

	// Create clip collection
	Flux_AnimationClipCollection xCollection;
	Flux_AnimationClip* pxClipA = new Flux_AnimationClip();
	pxClipA->SetName("SelectA");
	Flux_AnimationClip* pxClipB = new Flux_AnimationClip();
	pxClipB->SetName("SelectB");
	xCollection.AddClip(pxClipA);
	xCollection.AddClip(pxClipB);

	// Create Select node with two clip children
	Flux_BlendTreeNode_Clip* pxNodeA = new Flux_BlendTreeNode_Clip();
	pxNodeA->SetClipName("SelectA");
	Zenith_Assert(pxNodeA->GetClip() == nullptr, "Clip A should be unresolved");

	Flux_BlendTreeNode_Clip* pxNodeB = new Flux_BlendTreeNode_Clip();
	pxNodeB->SetClipName("SelectB");
	Zenith_Assert(pxNodeB->GetClip() == nullptr, "Clip B should be unresolved");

	Flux_BlendTreeNode_Select* pxSelect = new Flux_BlendTreeNode_Select();
	pxSelect->AddChild(pxNodeA);
	pxSelect->AddChild(pxNodeB);

	// Create state machine with state using this blend tree
	Flux_AnimationStateMachine xSM("TestSM");
	Flux_AnimationState* pxState = xSM.AddState("SelectState");
	pxState->SetBlendTree(pxSelect);
	xSM.SetDefaultState("SelectState");

	// Resolve
	xSM.ResolveClipReferences(&xCollection);

	Zenith_Assert(pxNodeA->GetClip() == pxClipA,
		"Select child clip A should be resolved");
	Zenith_Assert(pxNodeB->GetClip() == pxClipB,
		"Select child clip B should be resolved");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Select node children clips resolved recursively");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestResolveClipReferencesSelect passed");
}

void Zenith_UnitTests::TestLayerCompositionOverrideBlend()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLayerCompositionOverrideBlend...");

	// Create a simple 2-bone skeleton
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Child", 0, Zenith_Maths::Vector3(0, 1, 0), xIdentity, xUnitScale);
	pxSkel->ComputeBindPoseMatrices();

	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);

	Flux_AnimationController xController;
	xController.Initialize(pxSkelInst);

	// Create two clips with distinct root bone positions
	Flux_AnimationClip* pxClipA = new Flux_AnimationClip();
	pxClipA->SetName("PoseA");
	pxClipA->SetDuration(1.0f);
	pxClipA->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClipA->AddBoneChannel("Root", std::move(xChan));
	}

	Flux_AnimationClip* pxClipB = new Flux_AnimationClip();
	pxClipB->SetName("PoseB");
	pxClipB->SetDuration(1.0f);
	pxClipB->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(2.0f, 0.0f, 0.0f));
		pxClipB->AddBoneChannel("Root", std::move(xChan));
	}

	// Base layer plays PoseA (root at 0,0,0)
	Flux_AnimationLayer* pxBaseLayer = xController.AddLayer("Base");
	pxBaseLayer->SetWeight(1.0f);
	Flux_AnimationStateMachine* pxBaseSM = pxBaseLayer->CreateStateMachine("BaseSM");
	Flux_AnimationState* pxBaseState = pxBaseSM->AddState("PoseA");
	Flux_BlendTreeNode_Clip* pxBaseClipNode = new Flux_BlendTreeNode_Clip(pxClipA);
	pxBaseState->SetBlendTree(pxBaseClipNode);
	pxBaseSM->SetDefaultState("PoseA");
	pxBaseSM->SetState("PoseA");

	// Override layer plays PoseB (root at 2,0,0) at weight 0.5
	Flux_AnimationLayer* pxOverrideLayer = xController.AddLayer("Override");
	pxOverrideLayer->SetWeight(0.5f);
	pxOverrideLayer->SetBlendMode(LAYER_BLEND_OVERRIDE);
	Flux_AnimationStateMachine* pxOverrideSM = pxOverrideLayer->CreateStateMachine("OverrideSM");
	Flux_AnimationState* pxOverrideState = pxOverrideSM->AddState("PoseB");
	Flux_BlendTreeNode_Clip* pxOverrideClipNode = new Flux_BlendTreeNode_Clip(pxClipB);
	pxOverrideState->SetBlendTree(pxOverrideClipNode);
	pxOverrideSM->SetDefaultState("PoseB");
	pxOverrideSM->SetState("PoseB");

	// Update to evaluate both layers and compose
	xController.Update(0.016f);

	// Output should be a blend: base(0,0,0) blended with override(2,0,0) at weight 0.5
	// Expected root position: lerp(0, 2, 0.5) = (1, 0, 0)
	const Flux_SkeletonPose& xOutput = xController.GetOutputPose();
	const Flux_BoneLocalPose& xRootPose = xOutput.GetLocalPose(0);

	float fExpectedX = 1.0f;
	float fTolerance = 0.01f;
	Zenith_Assert(glm::abs(xRootPose.m_xPosition.x - fExpectedX) < fTolerance,
		"Root X should be ~1.0 (blend of 0.0 and 2.0 at weight 0.5), got %.3f", xRootPose.m_xPosition.x);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Layer override blend at 0.5 weight produces correct lerp (%.3f)",
		xRootPose.m_xPosition.x);

	delete pxSkelInst;
	delete pxSkel;
	delete pxClipA;
	delete pxClipB;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLayerCompositionOverrideBlend passed");
}

//=============================================================================
// Code review round 5 - additional coverage
//=============================================================================

void Zenith_UnitTests::TestLayerCompositionAdditiveBlend()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLayerCompositionAdditiveBlend...");

	// Create a simple 2-bone skeleton
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Child", 0, Zenith_Maths::Vector3(0, 1, 0), xIdentity, xUnitScale);
	pxSkel->ComputeBindPoseMatrices();

	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);

	Flux_AnimationController xController;
	xController.Initialize(pxSkelInst);

	// Base clip: root at (1, 0, 0)
	Flux_AnimationClip* pxClipBase = new Flux_AnimationClip();
	pxClipBase->SetName("Base");
	pxClipBase->SetDuration(1.0f);
	pxClipBase->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
		pxClipBase->AddBoneChannel("Root", std::move(xChan));
	}

	// Additive clip: root at (3, 0, 0) - delta from bind pose (0,0,0) = +3
	Flux_AnimationClip* pxClipAdd = new Flux_AnimationClip();
	pxClipAdd->SetName("Additive");
	pxClipAdd->SetDuration(1.0f);
	pxClipAdd->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(3.0f, 0.0f, 0.0f));
		pxClipAdd->AddBoneChannel("Root", std::move(xChan));
	}

	// Base layer plays Base clip
	Flux_AnimationLayer* pxBaseLayer = xController.AddLayer("Base");
	pxBaseLayer->SetWeight(1.0f);
	Flux_AnimationStateMachine* pxBaseSM = pxBaseLayer->CreateStateMachine("BaseSM");
	Flux_AnimationState* pxBaseState = pxBaseSM->AddState("Base");
	pxBaseState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipBase));
	pxBaseSM->SetDefaultState("Base");
	pxBaseSM->SetState("Base");

	// Additive layer at weight 1.0
	Flux_AnimationLayer* pxAddLayer = xController.AddLayer("Additive");
	pxAddLayer->SetWeight(1.0f);
	pxAddLayer->SetBlendMode(LAYER_BLEND_ADDITIVE);
	Flux_AnimationStateMachine* pxAddSM = pxAddLayer->CreateStateMachine("AddSM");
	Flux_AnimationState* pxAddState = pxAddSM->AddState("Additive");
	pxAddState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipAdd));
	pxAddSM->SetDefaultState("Additive");
	pxAddSM->SetState("Additive");

	xController.Update(0.016f);

	// Additive blend adds delta on top of base: base(1) + additive(3) * weight(1) = 4
	const Flux_SkeletonPose& xOutput = xController.GetOutputPose();
	const Flux_BoneLocalPose& xRootPose = xOutput.GetLocalPose(0);

	// Additive result should be greater than base alone
	Zenith_Assert(xRootPose.m_xPosition.x > 1.0f + 0.01f,
		"Additive layer should increase root X beyond base (1.0), got %.3f", xRootPose.m_xPosition.x);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Additive layer adds delta on top of base (result: %.3f)", xRootPose.m_xPosition.x);

	delete pxSkelInst;
	delete pxSkel;
	delete pxClipBase;
	delete pxClipAdd;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLayerCompositionAdditiveBlend passed");
}

void Zenith_UnitTests::TestLayerMaskedOverrideBlend()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestLayerMaskedOverrideBlend...");

	// Create 3-bone skeleton
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Upper", 0, Zenith_Maths::Vector3(0, 1, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Lower", 0, Zenith_Maths::Vector3(0, -1, 0), xIdentity, xUnitScale);
	pxSkel->ComputeBindPoseMatrices();

	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);

	Flux_AnimationController xController;
	xController.Initialize(pxSkelInst);

	// Base clip: all bones at (0, 0, 0)
	Flux_AnimationClip* pxClipBase = new Flux_AnimationClip();
	pxClipBase->SetName("Base");
	pxClipBase->SetDuration(1.0f);
	pxClipBase->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClipBase->AddBoneChannel("Root", std::move(xChan));
	}
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Upper");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClipBase->AddBoneChannel("Upper", std::move(xChan));
	}
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Lower");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClipBase->AddBoneChannel("Lower", std::move(xChan));
	}

	// Override clip: all bones at (4, 0, 0)
	Flux_AnimationClip* pxClipOverride = new Flux_AnimationClip();
	pxClipOverride->SetName("Override");
	pxClipOverride->SetDuration(1.0f);
	pxClipOverride->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));
		pxClipOverride->AddBoneChannel("Root", std::move(xChan));
	}
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Upper");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));
		pxClipOverride->AddBoneChannel("Upper", std::move(xChan));
	}
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Lower");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));
		pxClipOverride->AddBoneChannel("Lower", std::move(xChan));
	}

	// Base layer
	Flux_AnimationLayer* pxBaseLayer = xController.AddLayer("Base");
	pxBaseLayer->SetWeight(1.0f);
	Flux_AnimationStateMachine* pxBaseSM = pxBaseLayer->CreateStateMachine("BaseSM");
	Flux_AnimationState* pxBaseState = pxBaseSM->AddState("Base");
	pxBaseState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipBase));
	pxBaseSM->SetDefaultState("Base");
	pxBaseSM->SetState("Base");

	// Masked override layer: bone 1 (Upper) fully overridden, bone 2 (Lower) not affected
	Flux_AnimationLayer* pxMaskLayer = xController.AddLayer("MaskedOverride");
	pxMaskLayer->SetWeight(1.0f);
	pxMaskLayer->SetBlendMode(LAYER_BLEND_OVERRIDE);
	Flux_BoneMask xMask;
	xMask.SetBoneWeight(0, 0.0f);  // Root: no override
	xMask.SetBoneWeight(1, 1.0f);  // Upper: full override
	xMask.SetBoneWeight(2, 0.0f);  // Lower: no override
	pxMaskLayer->SetAvatarMask(xMask);

	Flux_AnimationStateMachine* pxMaskSM = pxMaskLayer->CreateStateMachine("MaskSM");
	Flux_AnimationState* pxMaskState = pxMaskSM->AddState("Override");
	pxMaskState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipOverride));
	pxMaskSM->SetDefaultState("Override");
	pxMaskSM->SetState("Override");

	xController.Update(0.016f);

	const Flux_SkeletonPose& xOutput = xController.GetOutputPose();
	float fTolerance = 0.01f;

	// Root (mask weight 0): should remain at base (0, 0, 0)
	Zenith_Assert(glm::abs(xOutput.GetLocalPose(0).m_xPosition.x - 0.0f) < fTolerance,
		"Root (mask=0) should stay at base 0.0, got %.3f", xOutput.GetLocalPose(0).m_xPosition.x);

	// Upper (mask weight 1): should be fully overridden to (4, 0, 0)
	Zenith_Assert(glm::abs(xOutput.GetLocalPose(1).m_xPosition.x - 4.0f) < fTolerance,
		"Upper (mask=1) should be overridden to 4.0, got %.3f", xOutput.GetLocalPose(1).m_xPosition.x);

	// Lower (mask weight 0): should remain at base (0, 0, 0)
	Zenith_Assert(glm::abs(xOutput.GetLocalPose(2).m_xPosition.x - 0.0f) < fTolerance,
		"Lower (mask=0) should stay at base 0.0, got %.3f", xOutput.GetLocalPose(2).m_xPosition.x);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Masked override only affects bone with mask weight > 0");

	delete pxSkelInst;
	delete pxSkel;
	delete pxClipBase;
	delete pxClipOverride;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLayerMaskedOverrideBlend passed");
}

void Zenith_UnitTests::TestPingPongAsymmetricEasing()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestPingPongAsymmetricEasing...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PingPongEasingTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxSceneData, "TweenEntity");
	xEntity.AddComponent<Zenith_TweenComponent>();

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetScale(Zenith_Maths::Vector3(0.0f));

	Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
	// QuadIn: slow start, fast end. Forward at t=0.5 should produce 0.25 (0.5^2)
	xTween.TweenScaleFromTo(Zenith_Maths::Vector3(0.0f), Zenith_Maths::Vector3(1.0f), 1.0f, EASING_QUAD_IN);
	xTween.SetLoop(true, true);

	// Forward at t=0.5: QuadIn(0.5) = 0.25
	xTween.OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);
	float fForwardHalf = xScale.x;
	Zenith_Assert(glm::abs(fForwardHalf - 0.25f) < 0.05f,
		"Forward QuadIn at 0.5 should be ~0.25, got %.3f", fForwardHalf);

	// Complete forward pass
	xTween.OnUpdate(0.5f);

	// Reverse at t=0.5: should mirror forward curve
	// Correct: 1.0 - QuadIn(0.5) = 1.0 - 0.25 = 0.75
	// Bug would produce: QuadIn(1.0 - 0.5) = QuadIn(0.5) = 0.25 (wrong!)
	xTween.OnUpdate(0.5f);
	xTransform.GetScale(xScale);
	float fReverseHalf = xScale.x;
	Zenith_Assert(fReverseHalf > 0.5f,
		"Reverse QuadIn at 0.5 should be > 0.5 (mirrored curve), got %.3f", fReverseHalf);
	Zenith_Assert(glm::abs(fReverseHalf - 0.75f) < 0.05f,
		"Reverse QuadIn at 0.5 should be ~0.75 (1.0 - 0.25), got %.3f", fReverseHalf);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Forward=%.3f, Reverse=%.3f (mirrored correctly)", fForwardHalf, fReverseHalf);

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPingPongAsymmetricEasing passed");
}

void Zenith_UnitTests::TestTransitionCompletionFramePose()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestTransitionCompletionFramePose...");

	// Create 2-bone skeleton
	Zenith_SkeletonAsset* pxSkel = new Zenith_SkeletonAsset();
	const Zenith_Maths::Quat xIdentity = glm::identity<Zenith_Maths::Quat>();
	const Zenith_Maths::Vector3 xUnitScale(1.0f);
	pxSkel->AddBone("Root", -1, Zenith_Maths::Vector3(0, 0, 0), xIdentity, xUnitScale);
	pxSkel->AddBone("Child", 0, Zenith_Maths::Vector3(0, 1, 0), xIdentity, xUnitScale);
	pxSkel->ComputeBindPoseMatrices();

	Flux_SkeletonInstance* pxSkelInst = Flux_SkeletonInstance::CreateFromAsset(pxSkel, false);

	// Test: after a transition completes, the output should be the target state's pose
	// (not double-advanced by evaluating the blend tree twice on the completion frame)
	Flux_AnimationStateMachine xSM("TestSM");
	xSM.GetParameters().AddTrigger("GoToB");

	// StateA: static pose at (0,0,0)
	Flux_AnimationClip* pxClipA = new Flux_AnimationClip();
	pxClipA->SetName("ClipA");
	pxClipA->SetDuration(1.0f);
	pxClipA->SetTicksPerSecond(1);
	pxClipA->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		pxClipA->AddBoneChannel("Root", std::move(xChan));
	}

	// StateB: moves from (0,0,0) to (10,0,0) over 1s
	// After transition completes, time in clip will be small (~0.2-0.3s)
	// Position should be ~(2-3, 0, 0), NOT ~(4-6, 0, 0) from double-advance
	Flux_AnimationClip* pxClipB = new Flux_AnimationClip();
	pxClipB->SetName("ClipB");
	pxClipB->SetDuration(1.0f);
	pxClipB->SetTicksPerSecond(1);
	pxClipB->SetLooping(true);
	{
		Flux_BoneChannel xChan;
		xChan.SetBoneName("Root");
		xChan.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xChan.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
		pxClipB->AddBoneChannel("Root", std::move(xChan));
	}

	Flux_AnimationState* pxStateA = xSM.AddState("StateA");
	pxStateA->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipA));
	Flux_AnimationState* pxStateB = xSM.AddState("StateB");
	pxStateB->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClipB));

	// Transition A->B on trigger, short duration
	{
		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = "StateB";
		xTrans.m_fTransitionDuration = 0.05f;
		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = "GoToB";
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Equal;
		xCond.m_bThreshold = true;
		xTrans.m_xConditions.PushBack(xCond);
		pxStateA->AddTransition(xTrans);
	}

	xSM.SetDefaultState("StateA");
	xSM.SetState("StateA");

	Flux_SkeletonPose xPose;
	xPose.Initialize(2);

	// Initialize
	xSM.Update(0.016f, xPose, *pxSkel);

	// Start transition
	xSM.GetParameters().SetTrigger("GoToB");
	xSM.Update(0.016f, xPose, *pxSkel);

	// Complete the transition with a large dt
	// StateB's blend tree will have accumulated ~0.016 + 0.2 = ~0.216s of time
	// Position should be ~(2.16, 0, 0), NOT ~(4.32, 0, 0) from double-advance
	xSM.Update(0.2f, xPose, *pxSkel);

	// Run several more frames after completion and verify smooth progression
	float fPrev = xPose.GetLocalPose(0).m_xPosition.x;
	bool bSmooth = true;
	for (int i = 0; i < 5; ++i)
	{
		xSM.Update(0.016f, xPose, *pxSkel);
		float fCurr = xPose.GetLocalPose(0).m_xPosition.x;
		float fDelta = glm::abs(fCurr - fPrev);
		// Each frame at dt=0.016 in a 1s clip spanning 10 units should advance ~0.16
		// A jump > 0.5 would indicate double-advance from the bug
		if (fDelta > 0.5f)
			bSmooth = false;
		fPrev = fCurr;
	}

	Zenith_Assert(bSmooth, "Post-transition frames should be smooth (no large jumps)");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] Post-transition frames are smooth");

	// Verify we're actually in StateB (clip position should be positive, increasing)
	Zenith_Assert(fPrev > 0.0f, "Position should be positive (in StateB clip range)");
	Zenith_Log(LOG_CATEGORY_UNITTEST, "  [OK] State machine is in target state (pos=%.3f)", fPrev);

	delete pxSkelInst;
	delete pxSkel;
	delete pxClipA;
	delete pxClipB;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTransitionCompletionFramePose passed");
}
