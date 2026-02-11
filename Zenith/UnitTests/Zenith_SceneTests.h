#pragma once

/**
 * Zenith_SceneTests - Comprehensive unit tests for the scene management system
 *
 * Tests cover:
 * - Scene handles (validity, equality, getters)
 * - Scene creation and queries
 * - Synchronous and asynchronous loading
 * - Scene unloading
 * - Entity persistence (DontDestroyOnLoad)
 * - Scene merging and combining
 * - Pause system
 * - Build index registry
 * - Event callbacks
 * - Entity destruction
 * - Camera management
 */
class Zenith_SceneTests
{
public:
	static void RunAllTests();

private:
	//==========================================================================
	// Helper Functions
	//==========================================================================
	static void CreateTestSceneFile(const std::string& strPath, const std::string& strEntityName = "TestEntity");
	static void CleanupTestSceneFile(const std::string& strPath);
	static void PumpUntilComplete(class Zenith_SceneOperation* pxOp, float fTimeoutSeconds = 5.0f);

	//==========================================================================
	// Scene Handle Tests (existing, moved from Zenith_UnitTests)
	//==========================================================================
	static void TestSceneHandleInvalid();
	static void TestSceneHandleEquality();
	static void TestSceneHandleGetters();
	static void TestSceneHandleRootCount();

	//==========================================================================
	// Scene Count Query Tests (existing)
	//==========================================================================
	static void TestSceneCountInitial();
	static void TestSceneCountAfterLoad();
	static void TestSceneCountAfterUnload();

	//==========================================================================
	// Scene Creation Tests (existing)
	//==========================================================================
	static void TestCreateEmptySceneName();
	static void TestCreateEmptySceneHandle();
	static void TestCreateMultipleEmptyScenes();

	//==========================================================================
	// Scene Query Tests (existing)
	//==========================================================================
	static void TestGetActiveSceneValid();
	static void TestGetSceneAtIndex();
	static void TestGetSceneByName();
	static void TestGetSceneByPath();

	//==========================================================================
	// Synchronous Loading Tests (existing)
	//==========================================================================
	static void TestLoadSceneSingle();
	static void TestLoadSceneAdditive();
	static void TestLoadSceneReturnsHandle();

	//==========================================================================
	// Scene Unloading Tests (existing)
	//==========================================================================
	static void TestUnloadSceneValid();
	static void TestUnloadSceneEntitiesDestroyed();

	//==========================================================================
	// Scene Management Operation Tests (existing)
	//==========================================================================
	static void TestSetActiveSceneValid();
	static void TestMoveEntityToScene();

	//==========================================================================
	// Entity Persistence Tests (existing)
	//==========================================================================
	static void TestMarkEntityPersistent();
	static void TestPersistentEntitySurvivesLoad();
	static void TestPersistentSceneAlwaysLoaded();

	//==========================================================================
	// Event Callback Tests (existing)
	//==========================================================================
	static void TestSceneLoadedCallbackFires();
	static void TestActiveSceneChangedCallbackFires();

	//==========================================================================
	// Scene Data Access Tests (existing)
	//==========================================================================
	static void TestGetSceneDataValid();
	static void TestGetSceneDataInvalid();
	static void TestSceneDataEntityCreation();

	//==========================================================================
	// Integration Tests (existing)
	//==========================================================================
	static void TestSceneLoadUnloadCycle();
	static void TestMultiSceneEntityInteraction();

	//==========================================================================
	// NEW: Async Loading Operation Tests
	//==========================================================================
	static void TestLoadSceneAsyncReturnsOperation();
	static void TestLoadSceneAsyncProgress();
	static void TestLoadSceneAsyncIsComplete();
	static void TestLoadSceneAsyncActivationPause();
	static void TestLoadSceneAsyncActivationResume();
	static void TestLoadSceneAsyncCompletionCallback();
	static void TestLoadSceneAsyncGetResultScene();
	static void TestLoadSceneAsyncPriority();
	static void TestLoadSceneAsyncByIndexValid();
	static void TestLoadSceneAsyncMultiple();
	static void TestLoadSceneAsyncSingleMode();
	static void TestLoadSceneAsyncAdditiveMode();

	//==========================================================================
	// NEW: Async Unloading Operation Tests
	//==========================================================================
	static void TestUnloadSceneAsyncReturnsOperation();
	static void TestUnloadSceneAsyncProgress();
	static void TestUnloadSceneAsyncComplete();
	static void TestUnloadSceneAsyncBatchDestruction();
	static void TestUnloadSceneAsyncActiveSceneSelection();

	//==========================================================================
	// NEW: Build Index System Tests
	//==========================================================================
	static void TestRegisterSceneBuildIndex();
	static void TestGetSceneByBuildIndex();
	static void TestGetSceneByBuildIndexInvalid();
	static void TestLoadSceneByIndexSync();
	static void TestGetBuildSceneCount();
	static void TestClearBuildIndexRegistry();

	//==========================================================================
	// NEW: Scene Pause System Tests
	//==========================================================================
	static void TestSetScenePaused();
	static void TestIsScenePaused();
	static void TestPausedSceneSkipsUpdate();
	static void TestPauseDoesNotAffectOtherScenes();

	//==========================================================================
	// NEW: Scene Combining/Merging Tests
	//==========================================================================
	static void TestMergeScenes();
	static void TestMergeScenesPreservesComponents();

	//==========================================================================
	// NEW: Additional Callback Tests
	//==========================================================================
	static void TestSceneUnloadingCallbackFires();
	static void TestSceneUnloadedCallbackFires();
	static void TestSceneLoadStartedCallbackFires();
	static void TestEntityPersistentCallbackFires();
	static void TestCallbackUnregister();
	static void TestCallbackUnregisterDuringCallback();
	static void TestMultipleCallbacksFireInOrder();
	static void TestCallbackHandleInvalid();

	//==========================================================================
	// NEW: Entity Destruction Tests
	//==========================================================================
	static void TestDestroyDeferred();
	static void TestDestroyImmediate();
	static void TestDestroyParentOrphansChildren();  // Unity parity: Children ARE cascade-destroyed
	static void TestMarkForDestructionFlag();

	//==========================================================================
	// NEW: Stale Handle Detection Tests
	//==========================================================================
	static void TestStaleHandleAfterUnload();
	static void TestStaleHandleGenerationMismatch();
	static void TestGetSceneDataStaleHandle();

	//==========================================================================
	// NEW: Camera Management Tests
	//==========================================================================
	static void TestSetMainCameraEntity();
	static void TestGetMainCameraEntity();
	static void TestGetMainCameraComponent();
	static void TestTryGetMainCameraNull();

	//==========================================================================
	// NEW: Scene Query Edge Case Tests
	//==========================================================================
	static void TestGetSceneByNameFilenameMatch();
	static void TestGetTotalSceneCount();

	//==========================================================================
	// NEW: Unity Parity & Bug Fix Tests
	//==========================================================================
	static void TestCannotUnloadLastScene();
	static void TestInvalidScenePropertyAccess();
	static void TestOperationIdAfterCleanup();
	static void TestMoveEntityToSceneSameScene();
	static void TestConcurrentAsyncUnloads();
	static void TestWasLoadedAdditively();
	static void TestAsyncLoadCircularDetection();
	static void TestSyncUnloadDuringAsyncUnload();

	//==========================================================================
	// NEW: Bug Fix Verification Tests (from code review)
	//==========================================================================
	static void TestMoveEntityToSceneMainCamera();
	static void TestMoveEntityToSceneDeepHierarchy();
	static void TestMarkEntityPersistentNonRoot();
	static void TestPausedSceneSkipsAllLifecycle();
	static void TestSceneLoadedCallbackOrder();

	//==========================================================================
	// NEW: Code Review Tests (from 2024-02 review)
	//==========================================================================
	static void TestAsyncLoadPriorityOrdering();
	static void TestAsyncLoadCancellation();
	static void TestAsyncAdditiveWithoutLoading();
	static void TestBatchSizeValidation();

	//==========================================================================
	// NEW: Test Coverage Additions (from 2025-02 review)
	//==========================================================================
	static void TestCircularAsyncLoadFromLifecycle();      // Scene A OnAwake loads B, B OnAwake loads A
	static void TestAsyncLoadDuringAsyncUnloadSameScene(); // Race condition: unload then load same scene
	static void TestEntitySpawnDuringOnDestroy();          // Component OnDestroy spawns new entity
	static void TestCallbackExceptionHandling();           // Callback throws - other callbacks still fire
	static void TestMalformedSceneFile();                  // Invalid .zscen file handling
	static void TestMaxConcurrentAsyncLoadWarning();       // Warning at max concurrent threshold

	//==========================================================================
	// NEW: Bug Fix Verification Tests (from 2026-02 code review)
	//==========================================================================

	// Bug 1: SetEnabled hierarchy check
	static void TestSetEnabledUnderDisabledParentNoOnEnable();
	static void TestSetEnabledUnderEnabledParentFiresOnEnable();
	static void TestDisableParentPropagatesOnDisableToChildren();
	static void TestEnableParentPropagatesOnEnableToEnabledChildren();
	static void TestDoublePropagationGuard();

	// Bug 2+11: EventSystem dispatch safety
	static void TestEventDispatchSubscribeDuringCallback();
	static void TestEventDispatchUnsubscribeDuringCallback();

	// Bug 3: sceneUnloaded handle validity
	static void TestSceneUnloadedCallbackHandleValid();

	// Bug 4: GetName/GetPath return const ref
	static void TestSceneGetNameReturnsRef();
	static void TestSceneGetPathReturnsRef();

	// Bug 6: Awake called immediately for entities created during Awake
	static void TestEntityCreatedDuringAwakeGetsAwakeImmediately();

	// Bug 7: activeInHierarchy caching
	static void TestActiveInHierarchyCacheValid();
	static void TestActiveInHierarchyCacheInvalidatedOnSetEnabled();
	static void TestActiveInHierarchyCacheInvalidatedOnSetParent();

	//==========================================================================
	// NEW: Bug Fix Regression Tests (from 2026-02 code review - batch 2)
	//==========================================================================

	// Fix 1: DispatchPendingStarts validates entity before clearing flag
	static void TestPendingStartSurvivesSlotReuse();
	static void TestPendingStartSkipsStaleEntity();

	// Fix 2: Slot reuse resets activeInHierarchy cache
	static void TestSlotReuseResetsActiveInHierarchy();
	static void TestSlotReuseDirtyFlagReset();

	// Fix 3: Async unload batch count includes recursive children
	static void TestAsyncUnloadBatchCountsChildren();
	static void TestAsyncUnloadProgressWithHierarchy();

	// Fix 4: MoveEntity transfers timed destructions
	static void TestMoveEntityTransfersTimedDestruction();
	static void TestMoveEntityTimedDestructionNotInSource();

	// Fix 5: MoveEntity adjusts pending start count
	static void TestMoveEntityAdjustsPendingStartCount();
	static void TestMoveEntityAlreadyStartedNoPendingCountChange();

	// Fix 6: Active scene selection prefers build index
	static void TestActiveSceneSelectionPrefersBuildIndex();
	static void TestActiveSceneSelectionFallsBackToTimestamp();

	//==========================================================================
	// NEW: Code Review Fix Verification Tests (2026-02 review - batch 3)
	//==========================================================================

	// B1: Runtime OnEnable uses IsActiveInHierarchy (not just IsEnabled)
	static void TestRuntimeEntityUnderDisabledParentNoOnEnable();
	static void TestRuntimeEntityUnderEnabledParentGetsOnEnable();

	// B2: PendingStart only consumed when Start actually dispatches
	static void TestDisabledEntityEventuallyGetsStart();
	static void TestDisabledEntityPendingStartCountConsistent();

	// B4: IsActiveInHierarchy null safety during teardown
	static void TestIsActiveInHierarchyDuringTeardown();

	// P1: isLoaded false before async activation
	static void TestAsyncLoadIsLoadedFalseBeforeActivation();

	// P3: GetLoadedSceneCount minimum is 1
	static void TestLoadedSceneCountMinimumOne();

	// P5+I3: Timed destruction cleanup for dead entities
	static void TestTimedDestructionEarlyCleanup();

	//==========================================================================
	// NEW: API Simplification Verification Tests (2026-02 simplification)
	//==========================================================================
	static void TestTryGetEntityValid();
	static void TestTryGetEntityInvalid();
	static void TestScenePathCanonicalization();
	static void TestFixedTimestepConfig();
	static void TestAsyncBatchSizeConfig();
	static void TestMaxConcurrentLoadsConfig();
	static void TestLoadSceneNonExistentFile();
	static void TestLoadSceneAsyncNonExistentFile();
	static void TestPersistentSceneInvisibleWhenEmpty();
	static void TestMarkPersistentWalksToRoot();
	static void TestGetSceneAtSkipsUnloadingScene();
	static void TestMergeScenesSourceBecomesActive();

	//==========================================================================
	// Cat 1: Entity Lifecycle - Awake/Start Ordering
	//==========================================================================
	static void TestAwakeFiresBeforeStart();
	static void TestStartDeferredToNextFrame();
	static void TestEntityCreatedInAwakeGetsFullLifecycle();
	static void TestAwakeWaveDrainMultipleLevels();
	static void TestUpdateNotCalledBeforeStart();
	static void TestFixedUpdateNotCalledBeforeStart();
	static void TestDestroyDuringAwakeSkipsStart();
	static void TestDisableDuringAwakeSkipsOnEnable();
	static void TestEntityWithNoScriptComponent();

	//==========================================================================
	// Cat 2: Entity Lifecycle - Destruction Ordering
	//==========================================================================
	static void TestOnDestroyCalledBeforeComponentRemoval();
	static void TestOnDisableCalledBeforeOnDestroy();
	static void TestDestroyChildrenBeforeParent();
	static void TestDoubleDestroyNoDoubleFree();
	static void TestDestroyedEntityAccessibleUntilProcessed();
	static void TestDestroyParentAndChildSameFrame();
	static void TestOnDestroySpawnsEntity();
	static void TestDestroyImmediateDuringIteration();
	static void TestTimedDestructionCountdown();
	static void TestTimedDestructionOnPausedScene();

	//==========================================================================
	// Cat 3: Entity Movement Between Scenes
	//==========================================================================
	static void TestMoveEntityComponentDataIntegrity();
	static void TestMoveEntityQueryConsistency();
	static void TestMoveEntityThenDestroySameFrame();
	static void TestMoveEntityRootCacheInvalidation();
	static void TestMoveEntityPreservesEntityID();
	static void TestMoveEntityWithPendingStartTransfers();
	static void TestMoveEntityDeepHierarchyIntegrity();
	static void TestMoveEntityMainCameraConflict();
	static void TestMoveEntityInvalidTarget();

	//==========================================================================
	// Cat 4: Async Operations Edge Cases
	//==========================================================================
	static void TestSyncLoadCancelsAsyncLoads();
	static void TestAsyncLoadProgressMonotonic();
	static void TestAsyncLoadSameFileTwice();
	static void TestAsyncUnloadThenReload();
	static void TestOperationCleanupAfter60Frames();
	static void TestIsOperationValidAfterCleanup();
	static void TestAsyncLoadSingleModeCleansUp();
	static void TestCancelAsyncLoadBeforeActivation();

	//==========================================================================
	// Cat 5: Callback Re-entrancy & Ordering
	//==========================================================================
	static void TestSceneLoadedCallbackLoadsAnotherScene();
	static void TestSceneUnloadedCallbackLoadsScene();
	static void TestActiveSceneChangedCallbackChangesActive();
	static void TestCallbackFiringDepthTracking();
	static void TestRegisterCallbackDuringDispatch();
	static void TestSingleModeCallbackOrder();
	static void TestMultipleCallbacksSameType();

	//==========================================================================
	// Cat 6: Scene Handle & Generation Counters
	//==========================================================================
	static void TestHandleReuseAfterUnload();
	static void TestOldHandleInvalidAfterReuse();
	static void TestSceneHashDifferentGenerations();
	static void TestMultipleCreateDestroyGenerations();

	//==========================================================================
	// Cat 7: Persistent Scene
	//==========================================================================
	static void TestPersistentSceneSurvivesSingleLoad();
	static void TestMultipleEntitiesPersistent();
	static void TestPersistentSceneVisibilityToggle();
	static void TestGetPersistentSceneAlwaysValid();
	static void TestPersistentEntityChildrenMoveWithRoot();

	//==========================================================================
	// Cat 8: FixedUpdate System
	//==========================================================================
	static void TestFixedUpdateMultipleCallsPerFrame();
	static void TestFixedUpdateZeroDt();
	static void TestFixedUpdateAccumulatorResetOnSingleLoad();
	static void TestFixedUpdatePausedSceneSkipped();
	static void TestFixedUpdateTimestepConfigurable();

	//==========================================================================
	// Cat 9: Scene Merge Deep Coverage
	//==========================================================================
	static void TestMergeScenesEntityIDsPreserved();
	static void TestMergeScenesHierarchyPreserved();
	static void TestMergeScenesEmptySource();
	static void TestMergeScenesMainCameraConflict();
	static void TestMergeScenesActiveSceneTransfer();

	//==========================================================================
	// Cat 10: Root Entity Cache
	//==========================================================================
	static void TestRootCacheInvalidatedOnCreate();
	static void TestRootCacheInvalidatedOnDestroy();
	static void TestRootCacheInvalidatedOnReparent();
	static void TestRootCacheCountMatchesVector();

	//==========================================================================
	// Cat 11: Serialization Round-Trip
	//==========================================================================
	static void TestSaveLoadEntityCount();
	static void TestSaveLoadHierarchy();
	static void TestSaveLoadTransformData();
	static void TestSaveLoadMainCamera();
	static void TestSaveLoadTransientExcluded();
	static void TestSaveLoadEmptyScene();

	//==========================================================================
	// Cat 12: Query Safety
	//==========================================================================
	static void TestQueryDuringEntityCreation();
	static void TestQueryDuringEntityDestruction();
	static void TestQueryEmptyScene();
	static void TestQueryAfterEntityMovedOut();

	//==========================================================================
	// Cat 13: Multi-Scene Independence
	//==========================================================================
	static void TestDestroyInSceneANoEffectOnSceneB();
	static void TestDisableInSceneANoEffectOnSceneB();
	static void TestIndependentMainCameras();
	static void TestIndependentRootCaches();

	//==========================================================================
	// Cat 14: Error Handling / Guard Rails
	//==========================================================================
	static void TestMoveNonRootEntity();
	static void TestSetActiveSceneInvalid();
	static void TestSetActiveSceneUnloading();
	static void TestUnloadPersistentScene();
	static void TestLoadSceneEmptyPath();

	//==========================================================================
	// Cat 15: Entity Slot Recycling & Generation Integrity
	//==========================================================================
	static void TestSlotReuseAfterDestroy();
	static void TestHighChurnSlotRecycling();
	static void TestStaleEntityIDAfterSlotReuse();
	static void TestEntitySlotPoolGrowth();
	static void TestEntityIDPackedRoundTrip();

	//==========================================================================
	// Cat 16: Component Management at Scene Level
	//==========================================================================
	static void TestAddRemoveComponent();
	static void TestAddOrReplaceComponent();
	static void TestComponentPoolGrowth();
	static void TestComponentSlotReuse();
	static void TestMultiComponentEntityMove();
	static void TestGetAllOfComponentType();
	static void TestComponentHandleValid();
	static void TestComponentHandleStaleAfterSlotReuse();

	//==========================================================================
	// Cat 17: Entity Handle Validity Edge Cases
	//==========================================================================
	static void TestDefaultEntityInvalid();
	static void TestEntityGetSceneDataAfterUnload();
	static void TestEntityGetSceneReturnsCorrectScene();
	static void TestEntityEqualityOperator();
	static void TestEntityValidAfterMove();
	static void TestEntityInvalidAfterDestroyImmediate();

	//==========================================================================
	// Cat 18: FindEntityByName
	//==========================================================================
	static void TestFindEntityByNameExists();
	static void TestFindEntityByNameNotFound();
	static void TestFindEntityByNameDuplicate();
	static void TestEntitySetNameGetName();

	//==========================================================================
	// Cat 19: Parent-Child Hierarchy in Scene Context
	//==========================================================================
	static void TestSetParentGetParent();
	static void TestUnparentEntity();
	static void TestReparentEntity();
	static void TestHasChildrenAndCount();
	static void TestIsRootEntity();
	static void TestDeepHierarchyActiveInHierarchy();
	static void TestSetParentAcrossScenes();

	//==========================================================================
	// Cat 20: Entity Enable/Disable Lifecycle
	//==========================================================================
	static void TestDisabledEntitySkipsUpdate();
	static void TestDisabledEntityComponentsAccessible();
	static void TestToggleEnableDisableMultipleTimes();
	static void TestIsEnabledVsIsActiveInHierarchy();
	static void TestEntityEnabledStatePreservedOnMove();

	//==========================================================================
	// Cat 21: Transient Entity Behavior
	//==========================================================================
	static void TestSetTransientIsTransient();
	static void TestTransientEntityNotSaved();
	static void TestNewEntityDefaultTransient();

	//==========================================================================
	// Cat 22: Camera Destruction & Edge Cases
	//==========================================================================
	static void TestMainCameraDestroyedThenQuery();
	static void TestSetMainCameraToNonCameraEntity();
	static void TestMainCameraPreservedOnSceneSave();

	//==========================================================================
	// Cat 23: Scene Merge Edge Cases
	//==========================================================================
	static void TestMergeScenesDisabledEntities();
	static void TestMergeScenesWithPendingStarts();
	static void TestMergeScenesWithTimedDestructions();
	static void TestMergeScenesMultipleRoots();

	//==========================================================================
	// Cat 24: Scene Load/Save with Entity State
	//==========================================================================
	static void TestSaveLoadDisabledEntity();
	static void TestSaveLoadEntityNames();
	static void TestSaveLoadMultipleComponentTypes();
	static void TestSaveLoadParentChildOrder();

	//==========================================================================
	// Cat 25: Lifecycle During Async Unload
	//==========================================================================
	static void TestAsyncUnloadingSceneSkipsUpdate();
	static void TestSceneUnloadingCallbackDataAccess();
	static void TestEntityExistsDuringAsyncUnload();

	//==========================================================================
	// Cat 26: Stress & Volume Tests
	//==========================================================================
	static void TestCreateManyEntities();
	static void TestRapidSceneCreateUnloadCycle();
	static void TestManyEntitiesPerformanceGuard();
	static void TestComponentPoolGrowthMultipleTypes();

	//==========================================================================
	// Cat 27: DontDestroyOnLoad Edge Cases
	//==========================================================================
	static void TestDontDestroyOnLoadIdempotent();
	static void TestPersistentEntityLifecycleContinues();
	static void TestPersistentEntityDestroyedManually();

	//==========================================================================
	// Cat 28: Update Ordering & Delta Time
	//==========================================================================
	static void TestUpdateReceivesCorrectDt();
	static void TestLateUpdateAfterUpdate();
	static void TestMultiSceneUpdateOrder();
	static void TestEntityCreatedDuringUpdateGetsNextFrameLifecycle();

	//==========================================================================
	// Cat 29: Lifecycle Edge Cases - Start Interactions
	//==========================================================================
	static void TestEntityCreatedDuringStart();
	static void TestDestroyDuringOnStart();
	static void TestDisableDuringOnStart();

	//==========================================================================
	// Cat 30: Lifecycle Interaction Combinations
	//==========================================================================
	static void TestSetParentDuringOnAwake();
	static void TestAddComponentDuringOnAwake();
	static void TestRemoveComponentDuringOnUpdate();
	static void TestDontDestroyOnLoadDuringOnAwake();
	static void TestMoveEntityToSceneDuringOnStart();
	static void TestToggleEnabledDuringOnAwake();
	static void TestEntityCreatedDuringOnFixedUpdate();
	static void TestEntityCreatedDuringOnLateUpdate();
	static void TestDestroyImmediateDuringSelfOnUpdate();

	//==========================================================================
	// Cat 31: Destruction Edge Cases
	//==========================================================================
	static void TestDestroyGrandchildThenGrandparent();
	static void TestDestroyImmediateDuringAnotherAwake();
	static void TestTimedDestructionZeroDelay();
	static void TestTimedDestructionCancelledBySceneUnload();
	static void TestMultipleTimedDestructionsSameEntity();

	//==========================================================================
	// Cat 32: Scene Operation State Machine
	//==========================================================================
	static void TestGetResultSceneBeforeCompletion();
	static void TestSetActivationAllowedAfterComplete();
	static void TestSetPriorityAfterCompletion();
	static void TestHasFailedOnNonExistentFileAsync();
	static void TestCancelAlreadyCompletedOperation();
	static void TestIsCancellationRequestedTracking();

	//==========================================================================
	// Cat 33: Component Handle System
	//==========================================================================
	static void TestComponentHandleSurvivesEnableDisable();
	static void TestTryGetComponentFromHandleData();
	static void TestTryGetComponentNullForMissing();
	static void TestGetComponentHandleForMissing();

	//==========================================================================
	// Cat 34: Cross-Feature Interactions
	//==========================================================================
	static void TestMergeSceneWithPersistentEntity();
	static void TestPausedSceneEntityGetsStartOnUnpause();
	static void TestAdditiveSetActiveUnloadOriginal();
	static void TestDontDestroyOnLoadDuringOnDestroy();
	static void TestMoveEntityToUnloadingScene();

	//==========================================================================
	// Cat 35: Untested Public Method Coverage
	//==========================================================================
	static void TestUnloadUnusedAssetsNoCrash();
	static void TestGetSceneDataForEntity();
	static void TestGetSceneDataByHandle();
	static void TestGetRootEntitiesVectorOutput();
	static void TestSceneGetHandleAndGetBuildIndex();

	//==========================================================================
	// Cat 36: Entity Event System
	//==========================================================================
	static void TestEntityCreatedEventNotFired();
	static void TestEntityDestroyedEventNotFired();
	static void TestComponentAddedEventNotFired();
	static void TestComponentRemovedEventNotFired();
	static void TestEventSubscriberCountTracking();

	//==========================================================================
	// Cat 37: Hierarchy Edge Cases
	//==========================================================================
	static void TestCircularHierarchyPreventionGrandchild();
	static void TestSelfParentPrevention();
	static void TestDetachFromParent();
	static void TestDetachAllChildren();
	static void TestForEachChildDuringChildDestruction();
	static void TestReparentDuringForEachChild();
	static void TestDeepHierarchyBuildModelMatrix();

	//==========================================================================
	// Cat 38: Path Canonicalization
	//==========================================================================
	static void TestCanonicalizeDotSlashPrefix();
	static void TestCanonicalizeParentResolution();
	static void TestCanonicalizeDoubleSlash();
	static void TestCanonicalizeAlreadyCanonical();
	static void TestGetSceneByPathNonCanonical();

	//==========================================================================
	// Cat 39: Stress & Boundary
	//==========================================================================
	static void TestRapidCreateDestroyEntitySlotIntegrity();
	static void TestSceneHandlePoolIntegrityCycles();
	static void TestMoveEntityThroughMultipleScenes();
	static void TestManyTimedDestructionsExpireSameFrame();
	static void TestMaxConcurrentAsyncOperationsEnforced();

	//==========================================================================
	// Cat 40: Scene Lifecycle State Verification
	//==========================================================================
	static void TestIsLoadedAtEveryStage();
	static void TestStaleHandleEveryMethodGraceful();
	static void TestSyncLoadSingleModeTwice();
	static void TestAdditiveLoadAlreadyLoadedScene();

	//==========================================================================
	// Cat 41: OnEnable/OnDisable Precise Semantics
	//==========================================================================
	static void TestInitialOnEnableFiresOnce();
	static void TestDisableThenEnableSameFrame();
	static void TestEnableChildWhenParentDisabled();
	static void TestRecursiveEnableMixedHierarchy();

	//==========================================================================
	// Cat 42: Deferred Scene Load (Unity Parity)
	//==========================================================================
	static void TestLoadSceneDeferredDuringUpdate();
	static void TestLoadSceneSyncOutsideUpdate();
};
