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
	static void TestFABRIKSolver();
	static void TestAnimationEvents();
	static void TestBoneMasking();

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

	// GUID system tests
	static void TestGUIDGeneration();
	static void TestGUIDStringRoundTrip();
	static void TestGUIDSerializationRoundTrip();
	static void TestGUIDComparisonOperators();
	static void TestGUIDHashDistribution();
	static void TestGUIDInvalidDetection();

	// Asset meta file tests
	static void TestAssetMetaSaveLoadRoundTrip();
	static void TestAssetMetaVersionCompatibility();
	static void TestAssetMetaImportSettings();
	static void TestAssetMetaGetMetaPath();

	// Asset database tests
	static void TestAssetDatabaseGUIDToPath();
	static void TestAssetDatabasePathToGUID();
	static void TestAssetDatabaseDependencyTracking();
	static void TestAssetDatabaseDependentLookup();

	// Asset reference tests
	static void TestAssetRefGUIDStorage();
	static void TestAssetRefSerializationRoundTrip();
	static void TestAssetRefFromPath();
	static void TestAssetRefInvalidHandling();

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
	static void TestPrefabNestedPrefabs();

	// Async asset loading tests
	static void TestAsyncLoadState();
	static void TestAsyncLoadRequest();
	static void TestAsyncLoadCompletion();
	static void TestAssetRefAsyncAPI();

	// DataAsset system tests
	static void TestDataAssetRegistration();
	static void TestDataAssetCreateAndSave();
	static void TestDataAssetLoad();
	static void TestDataAssetRoundTrip();
};

// Include editor tests separately as they are only available in ZENITH_TOOLS builds
#ifdef ZENITH_TOOLS
#include "Zenith_EditorTests.h"
#endif