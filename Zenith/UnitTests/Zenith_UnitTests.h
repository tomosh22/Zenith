#pragma once
#include <fstream>

class Zenith_UnitTests
{
public:
	static void RunAllTests();
private:
	static void TestDataStream();
	static void TestMemoryManagement();
	static void TestProfiling();
	static void TestVector();
	static void TestVectorFind();
	static void TestVectorErase();
	static void TestVectorZeroCapacityResize();
	static void TestMemoryPool();
	static void TestMemoryPoolExhaustion();

	// CircularQueue tests
	static void TestCircularQueueBasic();
	static void TestCircularQueueWrapping();
	static void TestCircularQueueFull();
	static void TestCircularQueueNonPOD();

	// Vector edge case tests
	static void TestVectorSelfAssignment();
	static void TestVectorRemoveSwap();

	// DataStream edge case tests
	static void TestDataStreamBoundsCheck();

	// Scene serialization tests
	static void TestSceneSerialization();
	static void TestComponentSerialization();
	static void TestEntitySerialization();
	static void TestSceneRoundTrip();

	// Animation system tests
	static void TestBoneLocalPoseBlending();
	static void TestSkeletonPoseOperations();
	static void TestAnimationParameters();
	static void TestTransitionConditions();
	static void TestAnimationStateMachine();
	static void TestIKChainSetup();
	static void TestAnimationSerialization();
	static void TestBlendTreeNodes();
	static void TestCrossFadeTransition();

	// Additional animation tests
	static void TestAnimationClipChannels();
	static void TestBlendSpace1D();
	static void TestBlendSpace2D();
	static void TestBlendTreeEvaluation();
	static void TestBlendTreeSerialization();
	static void TestFABRIKSolver();
	static void TestAnimationEvents();
	static void TestBoneMasking();

	// Animation state machine integration tests
	static void TestStateMachineUpdateLoop();
	static void TestTriggerConsumptionInTransitions();
	static void TestExitTimeTransitions();
	static void TestTransitionPriority();
	static void TestStateLifecycleCallbacks();
	static void TestMultipleTransitionConditions();

	// Asset pipeline tests
	static void TestMeshAssetLoading();
	static void TestBindPoseVertexPositions();
	static void TestAnimatedVertexPositions();

	// Stick figure animation tests
	static void TestStickFigureSkeletonCreation();
	static void TestStickFigureMeshCreation();
	static void TestStickFigureIdleAnimation();
	static void TestStickFigureWalkAnimation();
	static void TestStickFigureRunAnimation();
	static void TestStickFigureAnimationBlending();

	// Stick figure IK tests
	static void TestStickFigureArmIK();
	static void TestStickFigureLegIK();
	static void TestStickFigureIKWithAnimation();

	// Stick figure asset export
	static void TestStickFigureAssetExport();

	// Procedural tree asset export (for instanced mesh testing)
	static void TestProceduralTreeAssetExport();

	// ECS bug fix tests (Phase 1)
	static void TestComponentRemovalIndexUpdate();
	static void TestComponentSwapAndPop();
	static void TestMultipleComponentRemoval();
	static void TestComponentRemovalWithManyEntities();
	static void TestEntityNameFromScene();
	static void TestEntityCopyPreservesAccess();

	// ECS reflection system tests (Phase 2)
	static void TestComponentMetaRegistration();
	static void TestComponentMetaSerialization();
	static void TestComponentMetaDeserialization();
	static void TestComponentMetaTypeIDConsistency();

	// ECS lifecycle hooks tests (Phase 3)
	static void TestLifecycleHookDetection();
	static void TestLifecycleOnAwake();
	static void TestLifecycleOnStart();
	static void TestLifecycleOnUpdate();
	static void TestLifecycleOnDestroy();
	static void TestLifecycleDispatchOrder();
	static void TestLifecycleEntityCreationDuringCallback();
	static void TestDispatchFullLifecycleInit();

	// ECS query system tests (Phase 4)
	static void TestQuerySingleComponent();
	static void TestQueryMultipleComponents();
	static void TestQueryNoMatches();
	static void TestQueryCount();
	static void TestQueryFirstAndAny();

	// ECS event system tests (Phase 5)
	static void TestEventSubscribeDispatch();
	static void TestEventUnsubscribe();
	static void TestEventDeferredQueue();
	static void TestEventMultipleSubscribers();
	static void TestEventClearSubscriptions();

	// Entity hierarchy tests
	static void TestEntityAddChild();
	static void TestEntityRemoveChild();
	static void TestEntityGetChildren();
	static void TestEntityReparenting();
	static void TestEntityChildCleanupOnDelete();
	static void TestEntityHierarchySerialization();

	// ECS safety tests (circular hierarchy, deferred creation, camera safety)
	static void TestCircularHierarchyPrevention();
	static void TestSelfParentingPrevention();
	static void TestTryGetMainCameraWhenNotSet();
	static void TestDeepHierarchyBuildModelMatrix();
	static void TestLocalSceneDestruction();
	static void TestLocalSceneWithHierarchy();

	// Prefab system tests
	static void TestPrefabCreateFromEntity();
	static void TestPrefabInstantiation();
	static void TestPrefabSaveLoadRoundTrip();
	static void TestPrefabOverrides();
	static void TestPrefabVariantCreation();

	// Async asset loading tests
	static void TestAsyncLoadState();
	static void TestAsyncLoadRequest();
	static void TestAsyncLoadCompletion();

	// DataAsset system tests
	static void TestDataAssetRegistration();
	static void TestDataAssetCreateAndSave();
	static void TestDataAssetLoad();
	static void TestDataAssetRoundTrip();

	// AI System tests - Blackboard
	static void TestBlackboardBasicTypes();
	static void TestBlackboardVector3();
	static void TestBlackboardEntityID();
	static void TestBlackboardHasKey();
	static void TestBlackboardClear();
	static void TestBlackboardDefaultValues();
	static void TestBlackboardOverwrite();
	static void TestBlackboardSerialization();

	// AI System tests - Behavior Tree
	static void TestBTSequenceAllSuccess();
	static void TestBTSequenceFirstFails();
	static void TestBTSequenceRunning();
	static void TestBTSelectorFirstSucceeds();
	static void TestBTSelectorAllFail();
	static void TestBTSelectorRunning();
	static void TestBTParallelRequireOne();
	static void TestBTParallelRequireAll();
	static void TestBTInverter();
	static void TestBTRepeaterCount();
	static void TestBTCooldown();
	static void TestBTSucceeder();
	static void TestBTNodeOwnership();

	// AI System tests - NavMesh
	static void TestNavMeshPolygonCreation();
	static void TestNavMeshAdjacency();
	static void TestNavMeshFindNearestPolygon();
	static void TestNavMeshIsPointOnMesh();
	static void TestNavMeshRaycast();
	static void TestPathfindingStraightLine();
	static void TestPathfindingAroundObstacle();
	static void TestPathfindingNoPath();
	static void TestPathfindingSmoothing();

	// AI System tests - NavMesh Agent
	static void TestNavAgentSetDestination();
	static void TestNavAgentMovement();
	static void TestNavAgentArrival();
	static void TestNavAgentStop();
	static void TestNavAgentSpeedSettings();
	static void TestNavAgentRemainingDistanceBounds();
	static void TestPathfindingNoDuplicateWaypoints();
	static void TestPathfindingBatchProcessing();
	static void TestPathfindingPartialPath();

	// AI System tests - Perception
	static void TestSightConeInRange();
	static void TestSightConeOutOfRange();
	static void TestSightConeOutOfFOV();
	static void TestSightAwarenessGain();
	static void TestHearingStimulusInRange();
	static void TestHearingStimulusAttenuation();
	static void TestHearingStimulusOutOfRange();
	static void TestMemoryRememberTarget();
	static void TestMemoryDecay();

	// AI System tests - Squad
	static void TestSquadAddRemoveMember();
	static void TestSquadRoleAssignment();
	static void TestSquadLeaderSelection();
	static void TestFormationLine();
	static void TestFormationWedge();
	static void TestFormationWorldPositions();
	static void TestSquadSharedKnowledge();

	// AI System tests - Tactical Points
	static void TestTacticalPointRegistration();
	static void TestTacticalPointCoverScoring();
	static void TestTacticalPointFlankScoring();

	// AI System tests - Debug Variables
	static void TestTacticalPointDebugColor();
	static void TestSquadDebugRoleColor();

	// Asset Handle tests (operator bool fix for procedural assets)
	static void TestAssetHandleProceduralBoolConversion();
	static void TestAssetHandlePathBasedBoolConversion();
	static void TestAssetHandleEmptyBoolConversion();
	static void TestAssetHandleSetStoresRef();
	static void TestAssetHandleCopySemantics();
	static void TestAssetHandleMoveSemantics();
	static void TestAssetHandleSetPathReleasesRef();
	static void TestAssetHandleClearReleasesRef();
	static void TestAssetHandleProceduralComparison();
};

// Include editor tests separately as they are only available in ZENITH_TOOLS builds
#ifdef ZENITH_TOOLS
#include "Zenith_EditorTests.h"
#endif