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
	static void TestMemoryPool();

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

	// ECS bug fix tests (Phase 1)
	static void TestComponentRemovalIndexUpdate();
	static void TestComponentSwapAndPop();
	static void TestMultipleComponentRemoval();
	static void TestComponentRemovalWithManyEntities();
	static void TestEntityIsTrivialSize();
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
};

// Include editor tests separately as they are only available in ZENITH_TOOLS builds
#ifdef ZENITH_TOOLS
#include "Zenith_EditorTests.h"
#endif