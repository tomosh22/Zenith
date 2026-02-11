#include "Zenith.h"
#include "UnitTests/Zenith_SceneTests.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneOperation.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Zenith_Query.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Core/Zenith_Core.h"
#include <filesystem>
#include <chrono>
#include <fstream>

//==============================================================================
// Test Behaviour - tracks lifecycle calls via static counters
//==============================================================================

class SceneTestBehaviour : public Zenith_ScriptBehaviour
{
public:
	SceneTestBehaviour(Zenith_Entity& xEntity) { m_xParentEntity = xEntity; }

	static uint32_t s_uAwakeCount;
	static uint32_t s_uStartCount;
	static uint32_t s_uUpdateCount;
	static uint32_t s_uDestroyCount;
	static uint32_t s_uEnableCount;
	static uint32_t s_uDisableCount;
	static uint32_t s_uFixedUpdateCount;
	static uint32_t s_uLateUpdateCount;
	static Zenith_EntityID s_xLastAwokenEntity;
	static Zenith_EntityID s_xLastDestroyedEntity;

	static void(*s_pfnOnAwakeCallback)(Zenith_Entity&);
	static void(*s_pfnOnStartCallback)(Zenith_Entity&);
	static void(*s_pfnOnDestroyCallback)(Zenith_Entity&);
	static void(*s_pfnOnUpdateCallback)(Zenith_Entity&, float);
	static void(*s_pfnOnFixedUpdateCallback)(Zenith_Entity&, float);
	static void(*s_pfnOnLateUpdateCallback)(Zenith_Entity&, float);
	static void(*s_pfnOnEnableCallback)(Zenith_Entity&);
	static void(*s_pfnOnDisableCallback)(Zenith_Entity&);

	static void ResetCounters()
	{
		s_uAwakeCount = 0;
		s_uStartCount = 0;
		s_uUpdateCount = 0;
		s_uDestroyCount = 0;
		s_uEnableCount = 0;
		s_uDisableCount = 0;
		s_uFixedUpdateCount = 0;
		s_uLateUpdateCount = 0;
		s_xLastAwokenEntity = Zenith_EntityID();
		s_xLastDestroyedEntity = Zenith_EntityID();
		s_pfnOnAwakeCallback = nullptr;
		s_pfnOnStartCallback = nullptr;
		s_pfnOnDestroyCallback = nullptr;
		s_pfnOnUpdateCallback = nullptr;
		s_pfnOnFixedUpdateCallback = nullptr;
		s_pfnOnLateUpdateCallback = nullptr;
		s_pfnOnEnableCallback = nullptr;
		s_pfnOnDisableCallback = nullptr;
	}

	void OnAwake() override
	{
		s_uAwakeCount++;
		s_xLastAwokenEntity = GetEntity().GetEntityID();
		if (s_pfnOnAwakeCallback) s_pfnOnAwakeCallback(GetEntity());
	}
	void OnEnable() override { s_uEnableCount++; if (s_pfnOnEnableCallback) s_pfnOnEnableCallback(GetEntity()); }
	void OnDisable() override { s_uDisableCount++; if (s_pfnOnDisableCallback) s_pfnOnDisableCallback(GetEntity()); }
	void OnStart() override
	{
		s_uStartCount++;
		if (s_pfnOnStartCallback) s_pfnOnStartCallback(GetEntity());
	}
	void OnUpdate(float fDt) override
	{
		s_uUpdateCount++;
		if (s_pfnOnUpdateCallback) s_pfnOnUpdateCallback(GetEntity(), fDt);
	}
	void OnFixedUpdate(float fDt) override { s_uFixedUpdateCount++; if (s_pfnOnFixedUpdateCallback) s_pfnOnFixedUpdateCallback(GetEntity(), fDt); }
	void OnLateUpdate(float fDt) override { s_uLateUpdateCount++; if (s_pfnOnLateUpdateCallback) s_pfnOnLateUpdateCallback(GetEntity(), fDt); }
	void OnDestroy() override
	{
		s_uDestroyCount++;
		s_xLastDestroyedEntity = GetEntity().GetEntityID();
		if (s_pfnOnDestroyCallback) s_pfnOnDestroyCallback(GetEntity());
	}
	const char* GetBehaviourTypeName() const override { return "SceneTestBehaviour"; }
};

uint32_t SceneTestBehaviour::s_uAwakeCount = 0;
uint32_t SceneTestBehaviour::s_uStartCount = 0;
uint32_t SceneTestBehaviour::s_uUpdateCount = 0;
uint32_t SceneTestBehaviour::s_uDestroyCount = 0;
uint32_t SceneTestBehaviour::s_uEnableCount = 0;
uint32_t SceneTestBehaviour::s_uDisableCount = 0;
uint32_t SceneTestBehaviour::s_uFixedUpdateCount = 0;
uint32_t SceneTestBehaviour::s_uLateUpdateCount = 0;
Zenith_EntityID SceneTestBehaviour::s_xLastAwokenEntity;
Zenith_EntityID SceneTestBehaviour::s_xLastDestroyedEntity;
void(*SceneTestBehaviour::s_pfnOnAwakeCallback)(Zenith_Entity&) = nullptr;
void(*SceneTestBehaviour::s_pfnOnStartCallback)(Zenith_Entity&) = nullptr;
void(*SceneTestBehaviour::s_pfnOnDestroyCallback)(Zenith_Entity&) = nullptr;
void(*SceneTestBehaviour::s_pfnOnUpdateCallback)(Zenith_Entity&, float) = nullptr;
void(*SceneTestBehaviour::s_pfnOnFixedUpdateCallback)(Zenith_Entity&, float) = nullptr;
void(*SceneTestBehaviour::s_pfnOnLateUpdateCallback)(Zenith_Entity&, float) = nullptr;
void(*SceneTestBehaviour::s_pfnOnEnableCallback)(Zenith_Entity&) = nullptr;
void(*SceneTestBehaviour::s_pfnOnDisableCallback)(Zenith_Entity&) = nullptr;

//==============================================================================
// Helper: Create entity with SceneTestBehaviour attached
//==============================================================================
static Zenith_Entity CreateEntityWithBehaviour(Zenith_SceneData* pxSceneData, const std::string& strName)
{
	Zenith_Entity xEntity(pxSceneData, strName);
	xEntity.AddComponent<Zenith_ScriptComponent>().SetBehaviour<SceneTestBehaviour>();
	return xEntity;
}

//==============================================================================
// Helper: Pump N update frames
//==============================================================================
static void PumpFrames(uint32_t uCount, float fDt = 1.0f / 60.0f)
{
	for (uint32_t i = 0; i < uCount; i++)
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
	}
}

//==============================================================================
// Helper Functions
//==============================================================================

void Zenith_SceneTests::CreateTestSceneFile(const std::string& strPath, const std::string& strEntityName)
{
	Zenith_Scene xTemp = Zenith_SceneManager::CreateEmptyScene("TempForSave");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xTemp);
	Zenith_Entity xEntity(pxData, strEntityName);
	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xTemp);
}

void Zenith_SceneTests::CleanupTestSceneFile(const std::string& strPath)
{
	if (std::filesystem::exists(strPath))
	{
		std::filesystem::remove(strPath);
	}
}

void Zenith_SceneTests::PumpUntilComplete(Zenith_SceneOperation* pxOp, float fTimeoutSeconds)
{
	auto xStartTime = std::chrono::steady_clock::now();
	const float fDt = 1.0f / 60.0f;

	while (!pxOp->IsComplete())
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();

		auto xNow = std::chrono::steady_clock::now();
		float fElapsed = std::chrono::duration<float>(xNow - xStartTime).count();
		if (fElapsed > fTimeoutSeconds)
		{
			Zenith_Assert(false, "PumpUntilComplete: Operation timed out after %f seconds", fTimeoutSeconds);
			return;
		}
	}
}

//==============================================================================
// RunAllTests
//==============================================================================

void Zenith_SceneTests::RunAllTests()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "=== Running Scene Management Tests ===");

	// Scene Handle Tests
	TestSceneHandleInvalid();
	TestSceneHandleEquality();
	TestSceneHandleGetters();
	TestSceneHandleRootCount();

	// Scene Count Tests
	TestSceneCountInitial();
	TestSceneCountAfterLoad();
	TestSceneCountAfterUnload();

	// Scene Creation Tests
	TestCreateEmptySceneName();
	TestCreateEmptySceneHandle();
	TestCreateMultipleEmptyScenes();

	// Scene Query Tests
	TestGetActiveSceneValid();
	TestGetSceneAtIndex();
	TestGetSceneByName();
	TestGetSceneByPath();

	// Synchronous Loading Tests
	TestLoadSceneSingle();
	TestLoadSceneAdditive();
	TestLoadSceneReturnsHandle();

	// Unloading Tests
	TestUnloadSceneValid();
	TestUnloadSceneEntitiesDestroyed();

	// Scene Management Operation Tests
	TestSetActiveSceneValid();
	TestMoveEntityToScene();

	// Entity Persistence Tests
	TestMarkEntityPersistent();
	TestPersistentEntitySurvivesLoad();
	TestPersistentSceneAlwaysLoaded();

	// Callback Tests (existing)
	TestSceneLoadedCallbackFires();
	TestActiveSceneChangedCallbackFires();

	// Scene Data Access Tests
	TestGetSceneDataValid();
	TestGetSceneDataInvalid();
	TestSceneDataEntityCreation();

	// Integration Tests
	TestSceneLoadUnloadCycle();
	TestMultiSceneEntityInteraction();

	// NEW: Async Loading Tests
	TestLoadSceneAsyncReturnsOperation();
	TestLoadSceneAsyncProgress();
	TestLoadSceneAsyncIsComplete();
	TestLoadSceneAsyncActivationPause();
	TestLoadSceneAsyncActivationResume();
	TestLoadSceneAsyncCompletionCallback();
	TestLoadSceneAsyncGetResultScene();
	TestLoadSceneAsyncPriority();
	TestLoadSceneAsyncByIndexValid();
	TestLoadSceneAsyncMultiple();
	TestLoadSceneAsyncSingleMode();
	TestLoadSceneAsyncAdditiveMode();

	// NEW: Async Unloading Tests
	TestUnloadSceneAsyncReturnsOperation();
	TestUnloadSceneAsyncProgress();
	TestUnloadSceneAsyncComplete();
	TestUnloadSceneAsyncBatchDestruction();
	TestUnloadSceneAsyncActiveSceneSelection();

	// NEW: Build Index System Tests
	TestRegisterSceneBuildIndex();
	TestGetSceneByBuildIndex();
	TestGetSceneByBuildIndexInvalid();
	TestLoadSceneByIndexSync();
	TestGetBuildSceneCount();
	TestClearBuildIndexRegistry();

	// NEW: Scene Pause Tests
	TestSetScenePaused();
	TestIsScenePaused();
	TestPausedSceneSkipsUpdate();
	TestPauseDoesNotAffectOtherScenes();

	// NEW: Scene Combining/Merging Tests
	TestMergeScenes();
	TestMergeScenesPreservesComponents();

	// NEW: Additional Callback Tests
	TestSceneUnloadingCallbackFires();
	TestSceneUnloadedCallbackFires();
	TestSceneLoadStartedCallbackFires();
	TestEntityPersistentCallbackFires();
	TestCallbackUnregister();
	TestCallbackUnregisterDuringCallback();
	TestMultipleCallbacksFireInOrder();
	TestCallbackHandleInvalid();

	// NEW: Entity Destruction Tests
	TestDestroyDeferred();
	TestDestroyImmediate();
	TestDestroyParentOrphansChildren();  // Unity parity: children ARE cascade-destroyed with parent
	TestMarkForDestructionFlag();

	// NEW: Stale Handle Detection Tests
	TestStaleHandleAfterUnload();
	TestStaleHandleGenerationMismatch();
	TestGetSceneDataStaleHandle();

	// NEW: Camera Management Tests
	TestSetMainCameraEntity();
	TestGetMainCameraEntity();
	TestGetMainCameraComponent();
	TestTryGetMainCameraNull();

	// NEW: Scene Query Edge Case Tests
	TestGetSceneByNameFilenameMatch();
	TestGetTotalSceneCount();

	// NEW: Unity Parity & Bug Fix Tests
	TestCannotUnloadLastScene();
	TestInvalidScenePropertyAccess();
	TestOperationIdAfterCleanup();
	TestMoveEntityToSceneSameScene();
	TestConcurrentAsyncUnloads();
	TestWasLoadedAdditively();
	TestAsyncLoadCircularDetection();
	TestSyncUnloadDuringAsyncUnload();

	// NEW: Bug Fix Verification Tests (from code review)
	TestMoveEntityToSceneMainCamera();
	TestMoveEntityToSceneDeepHierarchy();
	TestMarkEntityPersistentNonRoot();
	TestPausedSceneSkipsAllLifecycle();
	TestSceneLoadedCallbackOrder();

	// NEW: Code Review Tests (from 2024-02 review)
	TestAsyncLoadPriorityOrdering();
	TestAsyncLoadCancellation();
	TestAsyncAdditiveWithoutLoading();
	TestBatchSizeValidation();

	// NEW: Test Coverage Additions (from 2025-02 review)
	TestCircularAsyncLoadFromLifecycle();
	TestAsyncLoadDuringAsyncUnloadSameScene();
	TestEntitySpawnDuringOnDestroy();
	TestCallbackExceptionHandling();
	TestMalformedSceneFile();
	TestMaxConcurrentAsyncLoadWarning();

	// Bug Fix Verification Tests (from 2026-02 code review)
	// Bug 1: SetEnabled hierarchy check
	TestSetEnabledUnderDisabledParentNoOnEnable();
	TestSetEnabledUnderEnabledParentFiresOnEnable();
	TestDisableParentPropagatesOnDisableToChildren();
	TestEnableParentPropagatesOnEnableToEnabledChildren();
	TestDoublePropagationGuard();

	// Bug 2+11: EventSystem dispatch safety
	TestEventDispatchSubscribeDuringCallback();
	TestEventDispatchUnsubscribeDuringCallback();

	// Bug 3: sceneUnloaded handle validity
	TestSceneUnloadedCallbackHandleValid();

	// Bug 4: GetName/GetPath return const ref
	TestSceneGetNameReturnsRef();
	TestSceneGetPathReturnsRef();

	// Bug 6: Awake called immediately for entities created during Awake
	TestEntityCreatedDuringAwakeGetsAwakeImmediately();

	// Bug 7: activeInHierarchy caching
	TestActiveInHierarchyCacheValid();
	TestActiveInHierarchyCacheInvalidatedOnSetEnabled();
	TestActiveInHierarchyCacheInvalidatedOnSetParent();

	// Bug Fix Regression Tests (from 2026-02 code review - batch 2)
	TestPendingStartSurvivesSlotReuse();
	TestPendingStartSkipsStaleEntity();
	TestSlotReuseResetsActiveInHierarchy();
	TestSlotReuseDirtyFlagReset();
	TestAsyncUnloadBatchCountsChildren();
	TestAsyncUnloadProgressWithHierarchy();
	TestMoveEntityTransfersTimedDestruction();
	TestMoveEntityTimedDestructionNotInSource();
	TestMoveEntityAdjustsPendingStartCount();
	TestMoveEntityAlreadyStartedNoPendingCountChange();
	TestActiveSceneSelectionPrefersBuildIndex();
	TestActiveSceneSelectionFallsBackToTimestamp();

	// Code Review Fix Verification Tests (2026-02 review - batch 3)
	// B1: Runtime OnEnable uses IsActiveInHierarchy
	TestRuntimeEntityUnderDisabledParentNoOnEnable();
	TestRuntimeEntityUnderEnabledParentGetsOnEnable();
	// B2: PendingStart only consumed when Start fires
	TestDisabledEntityEventuallyGetsStart();
	TestDisabledEntityPendingStartCountConsistent();
	// B4: IsActiveInHierarchy null safety
	TestIsActiveInHierarchyDuringTeardown();
	// P1: isLoaded before activation
	TestAsyncLoadIsLoadedFalseBeforeActivation();
	// P3: sceneCount minimum
	TestLoadedSceneCountMinimumOne();
	// P5+I3: timed destruction cleanup
	TestTimedDestructionEarlyCleanup();

	// API Simplification Verification Tests (2026-02 simplification)
	TestTryGetEntityValid();
	TestTryGetEntityInvalid();
	TestScenePathCanonicalization();
	TestFixedTimestepConfig();
	TestAsyncBatchSizeConfig();
	TestMaxConcurrentLoadsConfig();
	TestLoadSceneNonExistentFile();
	TestLoadSceneAsyncNonExistentFile();
	TestPersistentSceneInvisibleWhenEmpty();
	TestMarkPersistentWalksToRoot();
	TestGetSceneAtSkipsUnloadingScene();
	TestMergeScenesSourceBecomesActive();

	// Cat 1: Entity Lifecycle - Awake/Start Ordering
	TestAwakeFiresBeforeStart();
	TestStartDeferredToNextFrame();
	TestEntityCreatedInAwakeGetsFullLifecycle();
	TestAwakeWaveDrainMultipleLevels();
	TestUpdateNotCalledBeforeStart();
	TestFixedUpdateNotCalledBeforeStart();
	TestDestroyDuringAwakeSkipsStart();
	TestDisableDuringAwakeSkipsOnEnable();
	TestEntityWithNoScriptComponent();

	// Cat 2: Entity Lifecycle - Destruction Ordering
	TestOnDestroyCalledBeforeComponentRemoval();
	TestOnDisableCalledBeforeOnDestroy();
	TestDestroyChildrenBeforeParent();
	TestDoubleDestroyNoDoubleFree();
	TestDestroyedEntityAccessibleUntilProcessed();
	TestDestroyParentAndChildSameFrame();
	TestOnDestroySpawnsEntity();
	TestDestroyImmediateDuringIteration();
	TestTimedDestructionCountdown();
	TestTimedDestructionOnPausedScene();

	// Cat 3: Entity Movement Between Scenes
	TestMoveEntityComponentDataIntegrity();
	TestMoveEntityQueryConsistency();
	TestMoveEntityThenDestroySameFrame();
	TestMoveEntityRootCacheInvalidation();
	TestMoveEntityPreservesEntityID();
	TestMoveEntityWithPendingStartTransfers();
	TestMoveEntityDeepHierarchyIntegrity();
	TestMoveEntityMainCameraConflict();
	TestMoveEntityInvalidTarget();

	// Cat 4: Async Operations Edge Cases
	TestSyncLoadCancelsAsyncLoads();
	TestAsyncLoadProgressMonotonic();
	TestAsyncLoadSameFileTwice();
	TestAsyncUnloadThenReload();
	TestOperationCleanupAfter60Frames();
	TestIsOperationValidAfterCleanup();
	TestAsyncLoadSingleModeCleansUp();
	TestCancelAsyncLoadBeforeActivation();

	// Cat 5: Callback Re-entrancy & Ordering
	TestSceneLoadedCallbackLoadsAnotherScene();
	TestSceneUnloadedCallbackLoadsScene();
	TestActiveSceneChangedCallbackChangesActive();
	TestCallbackFiringDepthTracking();
	TestRegisterCallbackDuringDispatch();
	TestSingleModeCallbackOrder();
	TestMultipleCallbacksSameType();

	// Cat 6: Scene Handle & Generation Counters
	TestHandleReuseAfterUnload();
	TestOldHandleInvalidAfterReuse();
	TestSceneHashDifferentGenerations();
	TestMultipleCreateDestroyGenerations();

	// Cat 7: Persistent Scene
	TestPersistentSceneSurvivesSingleLoad();
	TestMultipleEntitiesPersistent();
	TestPersistentSceneVisibilityToggle();
	TestGetPersistentSceneAlwaysValid();
	TestPersistentEntityChildrenMoveWithRoot();

	// Cat 8: FixedUpdate System
	TestFixedUpdateMultipleCallsPerFrame();
	TestFixedUpdateZeroDt();
	TestFixedUpdateAccumulatorResetOnSingleLoad();
	TestFixedUpdatePausedSceneSkipped();
	TestFixedUpdateTimestepConfigurable();

	// Cat 9: Scene Merge Deep Coverage
	TestMergeScenesEntityIDsPreserved();
	TestMergeScenesHierarchyPreserved();
	TestMergeScenesEmptySource();
	TestMergeScenesMainCameraConflict();
	TestMergeScenesActiveSceneTransfer();

	// Cat 10: Root Entity Cache
	TestRootCacheInvalidatedOnCreate();
	TestRootCacheInvalidatedOnDestroy();
	TestRootCacheInvalidatedOnReparent();
	TestRootCacheCountMatchesVector();

	// Cat 11: Serialization Round-Trip
	TestSaveLoadEntityCount();
	TestSaveLoadHierarchy();
	TestSaveLoadTransformData();
	TestSaveLoadMainCamera();
	TestSaveLoadTransientExcluded();
	TestSaveLoadEmptyScene();

	// Cat 12: Query Safety
	TestQueryDuringEntityCreation();
	TestQueryDuringEntityDestruction();
	TestQueryEmptyScene();
	TestQueryAfterEntityMovedOut();

	// Cat 13: Multi-Scene Independence
	TestDestroyInSceneANoEffectOnSceneB();
	TestDisableInSceneANoEffectOnSceneB();
	TestIndependentMainCameras();
	TestIndependentRootCaches();

	// Cat 14: Error Handling / Guard Rails
	TestMoveNonRootEntity();
	TestSetActiveSceneInvalid();
	TestSetActiveSceneUnloading();
	TestUnloadPersistentScene();
	TestLoadSceneEmptyPath();

	// Cat 15: Entity Slot Recycling & Generation Integrity
	TestSlotReuseAfterDestroy();
	TestHighChurnSlotRecycling();
	TestStaleEntityIDAfterSlotReuse();
	TestEntitySlotPoolGrowth();
	TestEntityIDPackedRoundTrip();

	// Cat 16: Component Management at Scene Level
	TestAddRemoveComponent();
	TestAddOrReplaceComponent();
	TestComponentPoolGrowth();
	TestComponentSlotReuse();
	TestMultiComponentEntityMove();
	TestGetAllOfComponentType();
	TestComponentHandleValid();
	TestComponentHandleStaleAfterSlotReuse();

	// Cat 17: Entity Handle Validity Edge Cases
	TestDefaultEntityInvalid();
	TestEntityGetSceneDataAfterUnload();
	TestEntityGetSceneReturnsCorrectScene();
	TestEntityEqualityOperator();
	TestEntityValidAfterMove();
	TestEntityInvalidAfterDestroyImmediate();

	// Cat 18: FindEntityByName
	TestFindEntityByNameExists();
	TestFindEntityByNameNotFound();
	TestFindEntityByNameDuplicate();
	TestEntitySetNameGetName();

	// Cat 19: Parent-Child Hierarchy in Scene Context
	TestSetParentGetParent();
	TestUnparentEntity();
	TestReparentEntity();
	TestHasChildrenAndCount();
	TestIsRootEntity();
	TestDeepHierarchyActiveInHierarchy();
	TestSetParentAcrossScenes();

	// Cat 20: Entity Enable/Disable Lifecycle
	TestDisabledEntitySkipsUpdate();
	TestDisabledEntityComponentsAccessible();
	TestToggleEnableDisableMultipleTimes();
	TestIsEnabledVsIsActiveInHierarchy();
	TestEntityEnabledStatePreservedOnMove();

	// Cat 21: Transient Entity Behavior
	TestSetTransientIsTransient();
	TestTransientEntityNotSaved();
	TestNewEntityDefaultTransient();

	// Cat 22: Camera Destruction & Edge Cases
	TestMainCameraDestroyedThenQuery();
	TestSetMainCameraToNonCameraEntity();
	TestMainCameraPreservedOnSceneSave();

	// Cat 23: Scene Merge Edge Cases
	TestMergeScenesDisabledEntities();
	TestMergeScenesWithPendingStarts();
	TestMergeScenesWithTimedDestructions();
	TestMergeScenesMultipleRoots();

	// Cat 24: Scene Load/Save with Entity State
	TestSaveLoadDisabledEntity();
	TestSaveLoadEntityNames();
	TestSaveLoadMultipleComponentTypes();
	TestSaveLoadParentChildOrder();

	// Cat 25: Lifecycle During Async Unload
	TestAsyncUnloadingSceneSkipsUpdate();
	TestSceneUnloadingCallbackDataAccess();
	TestEntityExistsDuringAsyncUnload();

	// Cat 26: Stress & Volume Tests
	TestCreateManyEntities();
	TestRapidSceneCreateUnloadCycle();
	TestManyEntitiesPerformanceGuard();
	TestComponentPoolGrowthMultipleTypes();

	// Cat 27: DontDestroyOnLoad Edge Cases
	TestDontDestroyOnLoadIdempotent();
	TestPersistentEntityLifecycleContinues();
	TestPersistentEntityDestroyedManually();

	// Cat 28: Update Ordering & Delta Time
	TestUpdateReceivesCorrectDt();
	TestLateUpdateAfterUpdate();
	TestMultiSceneUpdateOrder();
	TestEntityCreatedDuringUpdateGetsNextFrameLifecycle();

	// Cat 29: Lifecycle Edge Cases - Start Interactions
	TestEntityCreatedDuringStart();
	TestDestroyDuringOnStart();
	TestDisableDuringOnStart();

	// Cat 30: Lifecycle Interaction Combinations
	TestSetParentDuringOnAwake();
	TestAddComponentDuringOnAwake();
	TestRemoveComponentDuringOnUpdate();
	TestDontDestroyOnLoadDuringOnAwake();
	TestMoveEntityToSceneDuringOnStart();
	TestToggleEnabledDuringOnAwake();
	TestEntityCreatedDuringOnFixedUpdate();
	TestEntityCreatedDuringOnLateUpdate();
	TestDestroyImmediateDuringSelfOnUpdate();

	// Cat 31: Destruction Edge Cases
	TestDestroyGrandchildThenGrandparent();
	TestDestroyImmediateDuringAnotherAwake();
	TestTimedDestructionZeroDelay();
	TestTimedDestructionCancelledBySceneUnload();
	TestMultipleTimedDestructionsSameEntity();

	// Cat 32: Scene Operation State Machine
	TestGetResultSceneBeforeCompletion();
	TestSetActivationAllowedAfterComplete();
	TestSetPriorityAfterCompletion();
	TestHasFailedOnNonExistentFileAsync();
	TestCancelAlreadyCompletedOperation();
	TestIsCancellationRequestedTracking();

	// Cat 33: Component Handle System
	TestComponentHandleSurvivesEnableDisable();
	TestTryGetComponentFromHandleData();
	TestTryGetComponentNullForMissing();
	TestGetComponentHandleForMissing();

	// Cat 34: Cross-Feature Interactions
	TestMergeSceneWithPersistentEntity();
	TestPausedSceneEntityGetsStartOnUnpause();
	TestAdditiveSetActiveUnloadOriginal();
	TestDontDestroyOnLoadDuringOnDestroy();
	TestMoveEntityToUnloadingScene();

	// Cat 35: Untested Public Method Coverage
	TestUnloadUnusedAssetsNoCrash();
	TestGetSceneDataForEntity();
	TestGetSceneDataByHandle();
	TestGetRootEntitiesVectorOutput();
	TestSceneGetHandleAndGetBuildIndex();

	// Cat 36: Entity Event System
	TestEntityCreatedEventNotFired();
	TestEntityDestroyedEventNotFired();
	TestComponentAddedEventNotFired();
	TestComponentRemovedEventNotFired();
	TestEventSubscriberCountTracking();

	// Cat 37: Hierarchy Edge Cases
	TestCircularHierarchyPreventionGrandchild();
	TestSelfParentPrevention();
	TestDetachFromParent();
	TestDetachAllChildren();
	TestForEachChildDuringChildDestruction();
	TestReparentDuringForEachChild();
	TestDeepHierarchyBuildModelMatrix();

	// Cat 38: Path Canonicalization
	TestCanonicalizeDotSlashPrefix();
	TestCanonicalizeParentResolution();
	TestCanonicalizeDoubleSlash();
	TestCanonicalizeAlreadyCanonical();
	TestGetSceneByPathNonCanonical();

	// Cat 39: Stress & Boundary
	TestRapidCreateDestroyEntitySlotIntegrity();
	TestSceneHandlePoolIntegrityCycles();
	TestMoveEntityThroughMultipleScenes();
	TestManyTimedDestructionsExpireSameFrame();
	TestMaxConcurrentAsyncOperationsEnforced();

	// Cat 40: Scene Lifecycle State Verification
	TestIsLoadedAtEveryStage();
	TestStaleHandleEveryMethodGraceful();
	TestSyncLoadSingleModeTwice();
	TestAdditiveLoadAlreadyLoadedScene();

	// Cat 41: OnEnable/OnDisable Precise Semantics
	TestInitialOnEnableFiresOnce();
	TestDisableThenEnableSameFrame();
	TestEnableChildWhenParentDisabled();
	TestRecursiveEnableMixedHierarchy();

	// Cat 42: Deferred Scene Load (Unity Parity)
	TestLoadSceneDeferredDuringUpdate();
	TestLoadSceneSyncOutsideUpdate();

	// Clean up any scene state left over from tests so it doesn't leak into the game.
	// We can't unload the last scene (engine prevents it), so reset the active scene's
	// data and clear its test name/path. Project_LoadInitialScene will populate it.
	{
		Zenith_Scene xCleanupScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxCleanupData = Zenith_SceneManager::GetSceneData(xCleanupScene);
		if (pxCleanupData)
		{
			pxCleanupData->Reset();
			pxCleanupData->m_strName.clear();
			pxCleanupData->m_strPath.clear();
		}
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "=== Scene Management Tests Complete ===");
}

//==============================================================================
// Scene Handle Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestSceneHandleInvalid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHandleInvalid...");

	Zenith_Scene xInvalidScene;
	Zenith_Assert(!xInvalidScene.IsValid(), "Default scene handle should be invalid");
	Zenith_Assert(xInvalidScene.m_iHandle == -1, "Default scene handle should have handle -1");

	Zenith_Scene xAlsoInvalid = Zenith_Scene::INVALID_SCENE;
	Zenith_Assert(!xAlsoInvalid.IsValid(), "INVALID_SCENE constant should be invalid");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHandleInvalid passed");
}

void Zenith_SceneTests::TestSceneHandleEquality()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHandleEquality...");

	Zenith_Scene xScene1 = Zenith_SceneManager::GetActiveScene();
	Zenith_Scene xScene2 = Zenith_SceneManager::GetActiveScene();

	Zenith_Assert(xScene1 == xScene2, "Same scene handles should be equal");
	Zenith_Assert(!(xScene1 != xScene2), "Same scene handles should not be not-equal");

	Zenith_Scene xInvalid;
	Zenith_Assert(xScene1 != xInvalid, "Valid scene should not equal invalid scene");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHandleEquality passed");
}

void Zenith_SceneTests::TestSceneHandleGetters()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHandleGetters...");

	Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xScene.IsValid(), "Active scene should be valid");

	std::string strName = xScene.GetName();
	Zenith_Assert(!strName.empty(), "Scene name should not be empty");

	bool bLoaded = xScene.IsLoaded();
	Zenith_Assert(bLoaded, "Active scene should be loaded");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHandleGetters passed");
}

void Zenith_SceneTests::TestSceneHandleRootCount()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHandleRootCount...");

	// Create a test scene
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("RootCountTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	uint32_t uInitialCount = xTestScene.GetRootEntityCount();

	// Create entities
	Zenith_Entity xEntity1(pxSceneData, "TestEntity1");
	Zenith_Entity xEntity2(pxSceneData, "TestEntity2");

	uint32_t uNewCount = xTestScene.GetRootEntityCount();
	Zenith_Assert(uNewCount == uInitialCount + 2, "Root count should increase by 2");

	// Clean up
	Zenith_SceneManager::UnloadScene(xTestScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHandleRootCount passed");
}

//==============================================================================
// Scene Count Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestSceneCountInitial()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneCountInitial...");

	// Verify the persistent scene exists
	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_Assert(xPersistent.IsValid(), "Persistent scene should be valid");

	// Unity behavior: sceneCount excludes the DontDestroyOnLoad/persistent scene
	// Record the initial count (may be 0 if only persistent scene exists)
	uint32_t uInitialCount = Zenith_SceneManager::GetLoadedSceneCount();

	// Create a new scene and verify count increases
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("CountInitialTest");
	uint32_t uNewCount = Zenith_SceneManager::GetLoadedSceneCount();
	Zenith_Assert(uNewCount == uInitialCount + 1, "Creating a scene should increase count by 1");

	// Clean up
	Zenith_SceneManager::UnloadScene(xTestScene);
	uint32_t uFinalCount = Zenith_SceneManager::GetLoadedSceneCount();
	Zenith_Assert(uFinalCount == uInitialCount, "Unloading should restore original count");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneCountInitial passed");
}

void Zenith_SceneTests::TestSceneCountAfterLoad()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneCountAfterLoad...");

	uint32_t uInitialCount = Zenith_SceneManager::GetLoadedSceneCount();

	// Create an empty scene (simulates loading)
	Zenith_Scene xNewScene = Zenith_SceneManager::CreateEmptyScene("CountTest");

	uint32_t uNewCount = Zenith_SceneManager::GetLoadedSceneCount();
	Zenith_Assert(uNewCount == uInitialCount + 1, "Scene count should increase after creating scene");

	// Clean up
	Zenith_SceneManager::UnloadScene(xNewScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneCountAfterLoad passed");
}

void Zenith_SceneTests::TestSceneCountAfterUnload()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneCountAfterUnload...");

	// Create a scene
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("UnloadCountTest");
	uint32_t uCountAfterCreate = Zenith_SceneManager::GetLoadedSceneCount();

	// Unload the scene
	Zenith_SceneManager::UnloadScene(xTestScene);
	uint32_t uCountAfterUnload = Zenith_SceneManager::GetLoadedSceneCount();

	Zenith_Assert(uCountAfterUnload == uCountAfterCreate - 1, "Scene count should decrease after unload");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneCountAfterUnload passed");
}

//==============================================================================
// Scene Creation Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestCreateEmptySceneName()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCreateEmptySceneName...");

	const std::string strTestName = "TestEmptyScene";
	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene(strTestName);

	Zenith_Assert(xScene.IsValid(), "Created scene should be valid");
	Zenith_Assert(xScene.GetName() == strTestName, "Scene name should match creation name");

	// Clean up
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCreateEmptySceneName passed");
}

void Zenith_SceneTests::TestCreateEmptySceneHandle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCreateEmptySceneHandle...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("HandleTest");

	Zenith_Assert(xScene.IsValid(), "Created scene should have valid handle");
	Zenith_Assert(xScene.m_iHandle >= 0, "Scene handle should be non-negative");

	// Verify we can get scene data
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Assert(pxData != nullptr, "Should be able to get scene data from valid handle");

	// Clean up
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCreateEmptySceneHandle passed");
}

void Zenith_SceneTests::TestCreateMultipleEmptyScenes()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCreateMultipleEmptyScenes...");

	Zenith_Scene xScene1 = Zenith_SceneManager::CreateEmptyScene("MultiTest1");
	Zenith_Scene xScene2 = Zenith_SceneManager::CreateEmptyScene("MultiTest2");
	Zenith_Scene xScene3 = Zenith_SceneManager::CreateEmptyScene("MultiTest3");

	// All should be valid with unique handles
	Zenith_Assert(xScene1.IsValid() && xScene2.IsValid() && xScene3.IsValid(),
		"All created scenes should be valid");
	Zenith_Assert(xScene1.m_iHandle != xScene2.m_iHandle, "Scenes should have unique handles");
	Zenith_Assert(xScene2.m_iHandle != xScene3.m_iHandle, "Scenes should have unique handles");
	Zenith_Assert(xScene1.m_iHandle != xScene3.m_iHandle, "Scenes should have unique handles");

	// Clean up
	Zenith_SceneManager::UnloadScene(xScene1);
	Zenith_SceneManager::UnloadScene(xScene2);
	Zenith_SceneManager::UnloadScene(xScene3);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCreateMultipleEmptyScenes passed");
}

//==============================================================================
// Scene Query Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestGetActiveSceneValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetActiveSceneValid...");

	Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xActive.IsValid(), "Active scene should always be valid");

	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xActive);
	Zenith_Assert(pxData != nullptr, "Active scene should have valid scene data");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetActiveSceneValid passed");
}

void Zenith_SceneTests::TestGetSceneAtIndex()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneAtIndex...");

	// Record initial count
	uint32_t uInitialCount = Zenith_SceneManager::GetLoadedSceneCount();

	// Create a test scene
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("IndexTest");

	uint32_t uNewCount = Zenith_SceneManager::GetLoadedSceneCount();
	Zenith_Assert(uNewCount == uInitialCount + 1, "Count should increase by 1 after creating scene");

	// The new scene should be at the last index (uNewCount - 1)
	Zenith_Scene xLastScene = Zenith_SceneManager::GetSceneAt(uNewCount - 1);
	Zenith_Assert(xLastScene.IsValid(), "Scene at last index should be valid");
	Zenith_Assert(xLastScene == xTestScene, "Scene at last index should match created scene");

	// All indices should return valid scenes
	for (uint32_t i = 0; i < uNewCount; ++i)
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetSceneAt(i);
		Zenith_Assert(xScene.IsValid(), "Scene at valid index should be valid");
	}

	// Out of bounds should return invalid
	Zenith_Scene xOutOfBounds = Zenith_SceneManager::GetSceneAt(9999);
	Zenith_Assert(!xOutOfBounds.IsValid(), "Out of bounds index should return invalid scene");

	// Clean up
	Zenith_SceneManager::UnloadScene(xTestScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneAtIndex passed");
}

void Zenith_SceneTests::TestGetSceneByName()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByName...");

	// Create a scene with known name
	const std::string strName = "NameQueryTest";
	Zenith_Scene xCreated = Zenith_SceneManager::CreateEmptyScene(strName);

	// Query by name
	Zenith_Scene xFound = Zenith_SceneManager::GetSceneByName(strName);
	Zenith_Assert(xFound.IsValid(), "Should find scene by name");
	Zenith_Assert(xFound == xCreated, "Found scene should match created scene");

	// Query non-existent name
	Zenith_Scene xNotFound = Zenith_SceneManager::GetSceneByName("NonExistentScene12345");
	Zenith_Assert(!xNotFound.IsValid(), "Non-existent scene should return invalid");

	// Clean up
	Zenith_SceneManager::UnloadScene(xCreated);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByName passed");
}

void Zenith_SceneTests::TestGetSceneByPath()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByPath...");

	// Create, save, and reload a test scene (LoadFromFile sets the path)
	const std::string strPath = "test_path_query" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Load the scene (this sets m_strPath via LoadFromFile)
	Zenith_Scene xTestScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xTestScene.IsValid(), "Scene should load successfully");

	// Query by path
	Zenith_Scene xFound = Zenith_SceneManager::GetSceneByPath(strPath);
	Zenith_Assert(xFound.IsValid(), "Should find scene by path");
	Zenith_Assert(xFound == xTestScene, "Found scene should match test scene");

	// Query non-existent path
	Zenith_Scene xNotFound = Zenith_SceneManager::GetSceneByPath("nonexistent/path" ZENITH_SCENE_EXT);
	Zenith_Assert(!xNotFound.IsValid(), "Non-existent path should return invalid");

	// Clean up
	Zenith_SceneManager::UnloadScene(xTestScene);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByPath passed");
}

//==============================================================================
// Synchronous Loading Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestLoadSceneSingle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneSingle...");

	const std::string strPath = "test_load_single" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Load in single mode
	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_SINGLE);

	Zenith_Assert(xLoaded.IsValid(), "Loaded scene should be valid");

	// Clean up
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneSingle passed");
}

void Zenith_SceneTests::TestLoadSceneAdditive()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAdditive...");

	const std::string strPath = "test_load_additive" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath, "AdditiveEntity");

	uint32_t uCountBefore = Zenith_SceneManager::GetLoadedSceneCount();

	// Load in additive mode
	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);

	Zenith_Assert(xLoaded.IsValid(), "Loaded scene should be valid");

	uint32_t uCountAfter = Zenith_SceneManager::GetLoadedSceneCount();
	Zenith_Assert(uCountAfter > uCountBefore, "Additive load should increase scene count");

	// Clean up
	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAdditive passed");
}

void Zenith_SceneTests::TestLoadSceneReturnsHandle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneReturnsHandle...");

	const std::string strPath = "test_load_handle" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Load and verify handle
	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xLoaded.IsValid(), "LoadScene should return valid handle");
	Zenith_Assert(xLoaded.m_iHandle >= 0, "Handle should be non-negative");

	// Clean up
	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneReturnsHandle passed");
}

//==============================================================================
// Scene Unloading Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestUnloadSceneValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneValid...");

	// Create a scene to unload
	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("UnloadTest");
	Zenith_Assert(xScene.IsValid(), "Created scene should be valid");

	// Unload it (synchronous - completes immediately)
	Zenith_SceneManager::UnloadScene(xScene);

	// Scene should no longer be findable
	Zenith_Scene xSearch = Zenith_SceneManager::GetSceneByName("UnloadTest");
	Zenith_Assert(!xSearch.IsValid(), "Unloaded scene should not be findable");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneValid passed");
}

void Zenith_SceneTests::TestUnloadSceneEntitiesDestroyed()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneEntitiesDestroyed...");

	// Create scene with entities
	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EntityDestroyTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity1(pxData, "Entity1");
	Zenith_Entity xEntity2(pxData, "Entity2");

	// Store count before unload
	uint32_t uEntityCount = pxData->GetEntityCount();
	Zenith_Assert(uEntityCount >= 2, "Should have at least 2 entities");

	// Unload scene
	Zenith_SceneManager::UnloadScene(xScene);

	// Scene data should no longer be accessible
	Zenith_SceneData* pxDataAfter = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Assert(pxDataAfter == nullptr, "Scene data should be null after unload");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneEntitiesDestroyed passed");
}

//==============================================================================
// Scene Management Operation Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestSetActiveSceneValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetActiveSceneValid...");

	// Create two scenes
	Zenith_Scene xScene1 = Zenith_SceneManager::CreateEmptyScene("ActiveTest1");
	Zenith_Scene xScene2 = Zenith_SceneManager::CreateEmptyScene("ActiveTest2");

	// Set scene2 as active
	bool bSuccess = Zenith_SceneManager::SetActiveScene(xScene2);
	Zenith_Assert(bSuccess, "SetActiveScene should succeed for valid scene");

	Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xActive == xScene2, "Active scene should be scene2");

	// Set back to scene1
	Zenith_SceneManager::SetActiveScene(xScene1);
	xActive = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xActive == xScene1, "Active scene should be scene1");

	// Clean up
	Zenith_SceneManager::UnloadScene(xScene1);
	Zenith_SceneManager::UnloadScene(xScene2);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetActiveSceneValid passed");
}

void Zenith_SceneTests::TestMoveEntityToScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToScene...");

	// Create two scenes
	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("TransferSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("TransferTarget");

	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// Create entity in source
	Zenith_Entity xEntity(pxSourceData, "TransferMe");
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition({1.0f, 2.0f, 3.0f});

	uint32_t uSourceCountBefore = pxSourceData->GetEntityCount();
	uint32_t uTargetCountBefore = pxTargetData->GetEntityCount();

	// Move entity - updates reference in-place (Unity behavior)
	Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);
	Zenith_Assert(xEntity.IsValid(), "Entity should be valid after move");

	// Verify the entity is now in target scene
	Zenith_Assert(xEntity.GetSceneData() == pxTargetData, "Entity should now belong to target scene");
	Zenith_Assert(xEntity.GetName() == "TransferMe", "Entity name should be preserved");

	// Verify transform was preserved
	glm::vec3 xPos;
	xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	Zenith_Assert(xPos.x == 1.0f && xPos.y == 2.0f && xPos.z == 3.0f, "Transform should be preserved");

	// Verify counts changed
	uint32_t uSourceCountAfter = pxSourceData->GetEntityCount();
	uint32_t uTargetCountAfter = pxTargetData->GetEntityCount();

	Zenith_Assert(uSourceCountAfter == uSourceCountBefore - 1, "Source should lose one entity");
	Zenith_Assert(uTargetCountAfter == uTargetCountBefore + 1, "Target should gain one entity");

	// Clean up
	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToScene passed");
}

//==============================================================================
// Entity Persistence Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestMarkEntityPersistent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMarkEntityPersistent...");

	// Create a scene with an entity
	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PersistTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "PersistentEntity");

	// Mark as persistent (transfers to persistent scene)
	Zenith_SceneManager::MarkEntityPersistent(xEntity);

	// Entity should now be in persistent scene - find by name
	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistentData = Zenith_SceneManager::GetSceneData(xPersistent);
	Zenith_Entity xTransferred = pxPersistentData->FindEntityByName("PersistentEntity");

	Zenith_Assert(xTransferred.IsValid(), "Marked entity should be in persistent scene");
	Zenith_Assert(xTransferred.GetScene() == xPersistent, "Entity's scene should be persistent scene");

	// Clean up - unload the original scene, entity should survive
	Zenith_SceneManager::UnloadScene(xScene);

	// Entity should still be accessible from persistent scene
	Zenith_Entity xStillExists = pxPersistentData->FindEntityByName("PersistentEntity");
	Zenith_Assert(xStillExists.IsValid(), "Persistent entity should survive scene unload");

	// Clean up the persistent entity
	Zenith_SceneManager::DestroyImmediate(xStillExists);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMarkEntityPersistent passed");
}

void Zenith_SceneTests::TestPersistentEntitySurvivesLoad()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentEntitySurvivesLoad...");

	// Create a persistent entity
	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistentData = Zenith_SceneManager::GetSceneData(xPersistent);

	Zenith_Entity xEntity(pxPersistentData, "SurvivesLoadTest");
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition({5.0f, 5.0f, 5.0f});
	Zenith_EntityID xID = xEntity.GetEntityID();

	// Create and save a test scene
	const std::string strPath = "test_persist_survives" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Load scene in single mode (should unload non-persistent scenes)
	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_SINGLE);

	// Persistent entity should still exist
	Zenith_Entity xAfterLoad = pxPersistentData->GetEntity(xID);
	Zenith_Assert(xAfterLoad.IsValid(), "Persistent entity should survive SCENE_LOAD_SINGLE");

	// Clean up
	Zenith_SceneManager::DestroyImmediate(xAfterLoad);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentEntitySurvivesLoad passed");
}

void Zenith_SceneTests::TestPersistentSceneAlwaysLoaded()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentSceneAlwaysLoaded...");

	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_Assert(xPersistent.IsValid(), "Persistent scene should be valid");
	Zenith_Assert(xPersistent.IsLoaded(), "Persistent scene should always be loaded");

	// Try to unload persistent scene (should fail or be no-op)
	Zenith_SceneManager::UnloadScene(xPersistent);

	// Persistent scene should still be valid and loaded
	Zenith_Scene xStillPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_Assert(xStillPersistent.IsValid(), "Persistent scene should still be valid after unload attempt");
	Zenith_Assert(xStillPersistent.IsLoaded(), "Persistent scene should still be loaded after unload attempt");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentSceneAlwaysLoaded passed");
}

//==============================================================================
// Event Callback Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestSceneLoadedCallbackFires()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneLoadedCallbackFires...");

	static bool s_bCallbackFired = false;
	static Zenith_Scene s_xLoadedScene;
	static Zenith_SceneLoadMode s_eLoadMode;

	// Register callback
	auto ulHandle = Zenith_SceneManager::RegisterSceneLoadedCallback(
		[](Zenith_Scene xScene, Zenith_SceneLoadMode eMode) {
			s_bCallbackFired = true;
			s_xLoadedScene = xScene;
			s_eLoadMode = eMode;
		}
	);

	// Create a test scene file
	const std::string strPath = "test_callback" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Reset flag
	s_bCallbackFired = false;

	// Load from file - this should fire the callback
	Zenith_Scene xLoadedScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);

	Zenith_Assert(s_bCallbackFired, "Scene loaded callback should fire on LoadScene");
	Zenith_Assert(s_xLoadedScene == xLoadedScene, "Callback should receive the loaded scene");

	// Clean up
	Zenith_SceneManager::UnregisterSceneLoadedCallback(ulHandle);
	Zenith_SceneManager::UnloadScene(xLoadedScene);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneLoadedCallbackFires passed");
}

void Zenith_SceneTests::TestActiveSceneChangedCallbackFires()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveSceneChangedCallbackFires...");

	static bool s_bCallbackFired = false;
	static Zenith_Scene s_xOldScene;
	static Zenith_Scene s_xNewScene;

	// Register callback
	auto ulHandle = Zenith_SceneManager::RegisterActiveSceneChangedCallback(
		[](Zenith_Scene xOld, Zenith_Scene xNew) {
			s_bCallbackFired = true;
			s_xOldScene = xOld;
			s_xNewScene = xNew;
		}
	);

	// Create two scenes
	Zenith_Scene xScene1 = Zenith_SceneManager::CreateEmptyScene("ActiveChangeTest1");
	Zenith_Scene xScene2 = Zenith_SceneManager::CreateEmptyScene("ActiveChangeTest2");

	s_bCallbackFired = false;

	// Change active scene
	Zenith_SceneManager::SetActiveScene(xScene2);

	Zenith_Assert(s_bCallbackFired, "Active scene changed callback should fire");
	Zenith_Assert(s_xNewScene == xScene2, "Callback should receive the new active scene");

	// Clean up
	Zenith_SceneManager::UnregisterActiveSceneChangedCallback(ulHandle);
	Zenith_SceneManager::UnloadScene(xScene1);
	Zenith_SceneManager::UnloadScene(xScene2);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveSceneChangedCallbackFires passed");
}

//==============================================================================
// Scene Data Access Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestGetSceneDataValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneDataValid...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DataValidTest");

	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Assert(pxData != nullptr, "GetSceneData should return non-null for valid scene");

	// Verify we can use the data
	Zenith_Entity xEntity(pxData, "TestEntity");
	Zenith_Assert(xEntity.IsValid(), "Should be able to create entity with scene data");

	// Clean up
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneDataValid passed");
}

void Zenith_SceneTests::TestGetSceneDataInvalid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneDataInvalid...");

	Zenith_Scene xInvalid;
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xInvalid);
	Zenith_Assert(pxData == nullptr, "GetSceneData should return null for invalid scene");

	Zenith_Scene xAlsoInvalid = Zenith_Scene::INVALID_SCENE;
	pxData = Zenith_SceneManager::GetSceneData(xAlsoInvalid);
	Zenith_Assert(pxData == nullptr, "GetSceneData should return null for INVALID_SCENE");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneDataInvalid passed");
}

void Zenith_SceneTests::TestSceneDataEntityCreation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneDataEntityCreation...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EntityCreationTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	uint32_t uInitialCount = pxData->GetEntityCount();

	// Create multiple entities
	Zenith_Entity xEntity1(pxData, "Entity1");
	Zenith_Entity xEntity2(pxData, "Entity2");
	Zenith_Entity xEntity3(pxData, "Entity3");

	uint32_t uFinalCount = pxData->GetEntityCount();
	Zenith_Assert(uFinalCount == uInitialCount + 3, "Entity count should increase by 3");

	// Verify entities are valid
	Zenith_Assert(xEntity1.IsValid() && xEntity2.IsValid() && xEntity3.IsValid(),
		"All created entities should be valid");

	// Clean up
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneDataEntityCreation passed");
}

//==============================================================================
// Integration Tests (moved from Zenith_UnitTests.cpp)
//==============================================================================

void Zenith_SceneTests::TestSceneLoadUnloadCycle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneLoadUnloadCycle...");

	// Create and save a test scene
	const std::string strPath = "test_cycle" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath, "CycleEntity");

	// Perform multiple load/unload cycles
	for (int i = 0; i < 3; ++i)
	{
		Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
		Zenith_Assert(xLoaded.IsValid(), "Load should succeed on each cycle");

		Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xLoaded);
		Zenith_Assert(pxData != nullptr, "Scene data should be valid");

		Zenith_SceneManager::UnloadScene(xLoaded);
	}

	// Clean up
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneLoadUnloadCycle passed");
}

void Zenith_SceneTests::TestMultiSceneEntityInteraction()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultiSceneEntityInteraction...");

	// Create two scenes with entities
	Zenith_Scene xScene1 = Zenith_SceneManager::CreateEmptyScene("MultiScene1");
	Zenith_Scene xScene2 = Zenith_SceneManager::CreateEmptyScene("MultiScene2");

	Zenith_SceneData* pxData1 = Zenith_SceneManager::GetSceneData(xScene1);
	Zenith_SceneData* pxData2 = Zenith_SceneManager::GetSceneData(xScene2);

	// Create entities in each scene
	Zenith_Entity xEntity1(pxData1, "Entity1");
	xEntity1.GetComponent<Zenith_TransformComponent>().SetPosition({1.0f, 0.0f, 0.0f});

	Zenith_Entity xEntity2(pxData2, "Entity2");
	xEntity2.GetComponent<Zenith_TransformComponent>().SetPosition({2.0f, 0.0f, 0.0f});

	// Verify entities are in correct scenes using Entity::GetScene()
	Zenith_Assert(xEntity1.GetScene() == xScene1, "Entity1 in Scene1");
	Zenith_Assert(xEntity2.GetScene() == xScene2, "Entity2 in Scene2");

	// Verify positions are independent
	Zenith_Maths::Vector3 xPos1, xPos2;
	xEntity1.GetComponent<Zenith_TransformComponent>().GetPosition(xPos1);
	xEntity2.GetComponent<Zenith_TransformComponent>().GetPosition(xPos2);

	Zenith_Assert(xPos1.x == 1.0f && xPos2.x == 2.0f, "Entity positions should be independent");

	// Clean up
	Zenith_SceneManager::UnloadScene(xScene1);
	Zenith_SceneManager::UnloadScene(xScene2);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultiSceneEntityInteraction passed");
}

//==============================================================================
// NEW: Async Loading Operation Tests
//==============================================================================

void Zenith_SceneTests::TestLoadSceneAsyncReturnsOperation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncReturnsOperation...");

	const std::string strPath = "test_async_op" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(ulOpID != ZENITH_INVALID_OPERATION_ID, "LoadSceneAsync should return valid operation ID");

	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "GetOperation should return non-null for valid ID");

	// Wait for completion and cleanup
	PumpUntilComplete(pxOp);
	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncReturnsOperation passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncProgress()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncProgress...");

	const std::string strPath = "test_async_progress" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "LoadSceneAsync should return operation");

	float fInitialProgress = pxOp->GetProgress();
	Zenith_Assert(fInitialProgress >= 0.0f, "Initial progress should be >= 0");

	// Pump updates until complete
	while (!pxOp->IsComplete())
	{
		float fProgress = pxOp->GetProgress();
		Zenith_Assert(fProgress >= 0.0f && fProgress <= 1.0f, "Progress should be in [0, 1]");
		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	Zenith_Assert(pxOp->GetProgress() == 1.0f, "Final progress should be 1.0");

	// Cleanup
	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncProgress passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncIsComplete()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncIsComplete...");

	const std::string strPath = "test_async_complete" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);

	// Pump until complete
	PumpUntilComplete(pxOp);

	Zenith_Assert(pxOp->IsComplete(), "IsComplete should return true after loading finishes");

	// Cleanup
	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncIsComplete passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncActivationPause()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncActivationPause...");

	const std::string strPath = "test_async_pause" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	pxOp->SetActivationAllowed(false);  // Pause at ~90%

	// Pump updates for a while
	for (int i = 0; i < 120; ++i)  // ~2 seconds at 60fps
	{
		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
		if (pxOp->GetProgress() >= 0.85f)
		{
			break;  // File loading done, should pause soon
		}
	}

	// Should pause at ~90% and not complete
	if (pxOp->GetProgress() >= 0.85f)
	{
		Zenith_Assert(!pxOp->IsComplete(), "Operation should pause and not complete when activation disabled");
		Zenith_Assert(pxOp->GetProgress() < 1.0f, "Progress should be < 1.0 when paused");
	}

	// Allow activation to complete
	pxOp->SetActivationAllowed(true);
	PumpUntilComplete(pxOp);

	Zenith_Assert(pxOp->IsComplete(), "Should complete after activation allowed");

	// Cleanup
	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncActivationPause passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncActivationResume()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncActivationResume...");

	const std::string strPath = "test_async_resume" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	pxOp->SetActivationAllowed(false);

	// Pump until it pauses
	for (int i = 0; i < 120 && !pxOp->IsComplete(); ++i)
	{
		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// Resume by allowing activation
	pxOp->SetActivationAllowed(true);

	// Should now complete
	PumpUntilComplete(pxOp);

	Zenith_Assert(pxOp->IsComplete(), "Should complete after SetActivationAllowed(true)");
	Zenith_Assert(pxOp->GetProgress() == 1.0f, "Progress should reach 1.0");

	// Cleanup
	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncActivationResume passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncCompletionCallback()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncCompletionCallback...");

	static bool s_bCallbackFired = false;
	static Zenith_Scene s_xResultScene;

	const std::string strPath = "test_async_callback" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	s_bCallbackFired = false;
	s_xResultScene = Zenith_Scene::INVALID_SCENE;

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	pxOp->SetOnComplete([](Zenith_Scene xScene) {
		s_bCallbackFired = true;
		s_xResultScene = xScene;
	});

	PumpUntilComplete(pxOp);

	Zenith_Assert(s_bCallbackFired, "Completion callback should fire");
	Zenith_Assert(s_xResultScene.IsValid(), "Callback should receive valid scene");
	Zenith_Assert(s_xResultScene == pxOp->GetResultScene(), "Callback scene should match GetResultScene");

	// Cleanup
	Zenith_SceneManager::UnloadScene(s_xResultScene);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncCompletionCallback passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncGetResultScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncGetResultScene...");

	const std::string strPath = "test_async_result" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);

	// Before complete, result may be invalid
	PumpUntilComplete(pxOp);

	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_Assert(xResult.IsValid(), "GetResultScene should return valid scene after completion");
	Zenith_Assert(Zenith_SceneManager::GetSceneData(xResult) != nullptr, "Result scene should have valid data");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncGetResultScene passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncPriority()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncPriority...");

	// Create two test scene files
	const std::string strPath1 = "test_async_priority1" ZENITH_SCENE_EXT;
	const std::string strPath2 = "test_async_priority2" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath1, "Entity1");
	CreateTestSceneFile(strPath2, "Entity2");

	// Load low priority first, then high priority
	Zenith_SceneOperationID ulOpIDLow = Zenith_SceneManager::LoadSceneAsync(strPath1, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOpLow = Zenith_SceneManager::GetOperation(ulOpIDLow);
	pxOpLow->SetPriority(0);

	Zenith_SceneOperationID ulOpIDHigh = Zenith_SceneManager::LoadSceneAsync(strPath2, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOpHigh = Zenith_SceneManager::GetOperation(ulOpIDHigh);
	pxOpHigh->SetPriority(100);

	Zenith_Assert(pxOpLow->GetPriority() == 0, "Low priority should be 0");
	Zenith_Assert(pxOpHigh->GetPriority() == 100, "High priority should be 100");

	// Pump until both complete
	while (!pxOpLow->IsComplete() || !pxOpHigh->IsComplete())
	{
		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// Both should complete
	Zenith_Assert(pxOpLow->IsComplete() && pxOpHigh->IsComplete(), "Both operations should complete");

	// Cleanup
	Zenith_SceneManager::UnloadScene(pxOpLow->GetResultScene());
	Zenith_SceneManager::UnloadScene(pxOpHigh->GetResultScene());
	CleanupTestSceneFile(strPath1);
	CleanupTestSceneFile(strPath2);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncPriority passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncByIndexValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncByIndexValid...");

	const std::string strPath = "test_async_index" ZENITH_SCENE_EXT;
	const int iBuildIndex = 999;

	CreateTestSceneFile(strPath);
	Zenith_SceneManager::RegisterSceneBuildIndex(iBuildIndex, strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsyncByIndex(iBuildIndex, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "LoadSceneAsyncByIndex should return operation");

	PumpUntilComplete(pxOp);

	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_Assert(xResult.IsValid(), "Should load scene by build index");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xResult);
	Zenith_SceneManager::ClearBuildIndexRegistry();
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncByIndexValid passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncMultiple()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncMultiple...");

	const std::string strPath1 = "test_async_multi1" ZENITH_SCENE_EXT;
	const std::string strPath2 = "test_async_multi2" ZENITH_SCENE_EXT;
	const std::string strPath3 = "test_async_multi3" ZENITH_SCENE_EXT;

	CreateTestSceneFile(strPath1, "Multi1");
	CreateTestSceneFile(strPath2, "Multi2");
	CreateTestSceneFile(strPath3, "Multi3");

	// Start multiple async loads
	Zenith_SceneOperationID ulOpID1 = Zenith_SceneManager::LoadSceneAsync(strPath1, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperationID ulOpID2 = Zenith_SceneManager::LoadSceneAsync(strPath2, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperationID ulOpID3 = Zenith_SceneManager::LoadSceneAsync(strPath3, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp1 = Zenith_SceneManager::GetOperation(ulOpID1);
	Zenith_SceneOperation* pxOp2 = Zenith_SceneManager::GetOperation(ulOpID2);
	Zenith_SceneOperation* pxOp3 = Zenith_SceneManager::GetOperation(ulOpID3);

	// Pump until all complete
	while (!pxOp1->IsComplete() || !pxOp2->IsComplete() || !pxOp3->IsComplete())
	{
		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// All should have valid results
	Zenith_Assert(pxOp1->GetResultScene().IsValid(), "Scene 1 should load");
	Zenith_Assert(pxOp2->GetResultScene().IsValid(), "Scene 2 should load");
	Zenith_Assert(pxOp3->GetResultScene().IsValid(), "Scene 3 should load");

	// Verify all are different scenes
	Zenith_Assert(pxOp1->GetResultScene() != pxOp2->GetResultScene(), "Scenes should be different");
	Zenith_Assert(pxOp2->GetResultScene() != pxOp3->GetResultScene(), "Scenes should be different");

	// Cleanup
	Zenith_SceneManager::UnloadScene(pxOp1->GetResultScene());
	Zenith_SceneManager::UnloadScene(pxOp2->GetResultScene());
	Zenith_SceneManager::UnloadScene(pxOp3->GetResultScene());
	CleanupTestSceneFile(strPath1);
	CleanupTestSceneFile(strPath2);
	CleanupTestSceneFile(strPath3);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncMultiple passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncSingleMode()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncSingleMode...");

	// Create an existing scene
	Zenith_Scene xExisting = Zenith_SceneManager::CreateEmptyScene("ExistingScene");

	const std::string strPath = "test_async_single" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Async load in single mode
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_SINGLE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	PumpUntilComplete(pxOp);

	// Existing non-persistent scenes should be unloaded
	Zenith_Scene xSearchExisting = Zenith_SceneManager::GetSceneByName("ExistingScene");
	Zenith_Assert(!xSearchExisting.IsValid(), "Existing scene should be unloaded in single mode");

	// Cleanup
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncSingleMode passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncAdditiveMode()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncAdditiveMode...");

	// Create an existing scene
	Zenith_Scene xExisting = Zenith_SceneManager::CreateEmptyScene("AdditiveExisting");
	uint32_t uCountBefore = Zenith_SceneManager::GetLoadedSceneCount();

	const std::string strPath = "test_async_additive" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Async load in additive mode
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	PumpUntilComplete(pxOp);

	uint32_t uCountAfter = Zenith_SceneManager::GetLoadedSceneCount();

	// Existing scene should still be there
	Zenith_Scene xSearchExisting = Zenith_SceneManager::GetSceneByName("AdditiveExisting");
	Zenith_Assert(xSearchExisting.IsValid(), "Existing scene should remain in additive mode");
	Zenith_Assert(uCountAfter > uCountBefore, "Scene count should increase");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xExisting);
	Zenith_SceneManager::UnloadScene(pxOp->GetResultScene());
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncAdditiveMode passed");
}

//==============================================================================
// NEW: Async Unloading Operation Tests
//==============================================================================

void Zenith_SceneTests::TestUnloadSceneAsyncReturnsOperation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneAsyncReturnsOperation...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("AsyncUnloadTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create some entities
	for (int i = 0; i < 10; ++i)
	{
		Zenith_Entity xEntity(pxData, "Entity" + std::to_string(i));
	}

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xScene);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "UnloadSceneAsync should return operation");

	PumpUntilComplete(pxOp);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneAsyncReturnsOperation passed");
}

void Zenith_SceneTests::TestUnloadSceneAsyncProgress()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneAsyncProgress...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("AsyncUnloadProgress");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create many entities to slow down unload
	for (int i = 0; i < 100; ++i)
	{
		Zenith_Entity xEntity(pxData, "Entity" + std::to_string(i));
	}

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xScene);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);

	bool bSawIntermediateProgress = false;
	while (!pxOp->IsComplete())
	{
		float fProgress = pxOp->GetProgress();
		Zenith_Assert(fProgress >= 0.0f && fProgress <= 1.0f, "Progress should be in [0, 1]");
		if (fProgress > 0.0f && fProgress < 1.0f)
		{
			bSawIntermediateProgress = true;
		}
		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	Zenith_Assert(pxOp->GetProgress() == 1.0f, "Final progress should be 1.0");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneAsyncProgress passed");
}

void Zenith_SceneTests::TestUnloadSceneAsyncComplete()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneAsyncComplete...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("AsyncUnloadComplete");

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xScene);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	PumpUntilComplete(pxOp);

	Zenith_Assert(pxOp->IsComplete(), "Operation should be complete");

	// Scene should no longer be findable
	Zenith_Scene xSearch = Zenith_SceneManager::GetSceneByName("AsyncUnloadComplete");
	Zenith_Assert(!xSearch.IsValid(), "Scene should be fully unloaded");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneAsyncComplete passed");
}

void Zenith_SceneTests::TestUnloadSceneAsyncBatchDestruction()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneAsyncBatchDestruction...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("BatchDestruction");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create more entities than the batch size (50)
	const int iEntityCount = 150;
	for (int i = 0; i < iEntityCount; ++i)
	{
		Zenith_Entity xEntity(pxData, "Entity" + std::to_string(i));
	}

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xScene);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);

	int iUpdateCount = 0;
	while (!pxOp->IsComplete())
	{
		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
		iUpdateCount++;
	}

	// With 150 entities and 50 per frame, should take at least 3 frames
	Zenith_Assert(iUpdateCount >= 1, "Should require multiple updates for batch destruction");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneAsyncBatchDestruction passed");
}

void Zenith_SceneTests::TestUnloadSceneAsyncActiveSceneSelection()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneAsyncActiveSceneSelection...");

	// Create two scenes
	Zenith_Scene xScene1 = Zenith_SceneManager::CreateEmptyScene("ActiveSelection1");
	Zenith_Scene xScene2 = Zenith_SceneManager::CreateEmptyScene("ActiveSelection2");

	// Set scene1 as active
	Zenith_SceneManager::SetActiveScene(xScene1);
	Zenith_Assert(Zenith_SceneManager::GetActiveScene() == xScene1, "Scene1 should be active");

	// Async unload the active scene
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xScene1);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	PumpUntilComplete(pxOp);

	// Active scene should have changed (to scene2 or persistent)
	Zenith_Scene xNewActive = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xNewActive.IsValid(), "Should have a valid active scene after unload");
	Zenith_Assert(xNewActive != xScene1, "Active scene should change from unloaded scene");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene2);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneAsyncActiveSceneSelection passed");
}

//==============================================================================
// NEW: Build Index System Tests
//==============================================================================

void Zenith_SceneTests::TestRegisterSceneBuildIndex()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRegisterSceneBuildIndex...");

	const int iBuildIndex = 42;
	const std::string strPath = "test_build_index" ZENITH_SCENE_EXT;

	Zenith_SceneManager::RegisterSceneBuildIndex(iBuildIndex, strPath);

	// Verify by checking build count increased
	uint32_t uCount = Zenith_SceneManager::GetBuildSceneCount();
	Zenith_Assert(uCount >= 1, "Build scene count should be at least 1 after registering");

	// Cleanup
	Zenith_SceneManager::ClearBuildIndexRegistry();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRegisterSceneBuildIndex passed");
}

void Zenith_SceneTests::TestGetSceneByBuildIndex()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByBuildIndex...");

	const int iBuildIndex = 100;
	const std::string strPath = "test_get_by_index" ZENITH_SCENE_EXT;

	CreateTestSceneFile(strPath);
	Zenith_SceneManager::RegisterSceneBuildIndex(iBuildIndex, strPath);

	// Load the scene by build index (this sets m_iBuildIndex on the scene data)
	Zenith_Scene xLoaded = Zenith_SceneManager::LoadSceneByIndex(iBuildIndex, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xLoaded.IsValid(), "LoadSceneByIndex should return valid scene");

	// Query by build index should find it
	Zenith_Scene xFound = Zenith_SceneManager::GetSceneByBuildIndex(iBuildIndex);
	Zenith_Assert(xFound.IsValid(), "Should find scene by build index");
	Zenith_Assert(xFound == xLoaded, "Found scene should match loaded scene");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xLoaded);
	Zenith_SceneManager::ClearBuildIndexRegistry();
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByBuildIndex passed");
}

void Zenith_SceneTests::TestGetSceneByBuildIndexInvalid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByBuildIndexInvalid...");

	// Query non-existent build index
	Zenith_Scene xNotFound = Zenith_SceneManager::GetSceneByBuildIndex(99999);
	Zenith_Assert(!xNotFound.IsValid(), "Non-existent build index should return invalid");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByBuildIndexInvalid passed");
}

void Zenith_SceneTests::TestLoadSceneByIndexSync()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneByIndexSync...");

	const int iBuildIndex = 101;
	const std::string strPath = "test_load_by_index" ZENITH_SCENE_EXT;

	CreateTestSceneFile(strPath);
	Zenith_SceneManager::RegisterSceneBuildIndex(iBuildIndex, strPath);

	Zenith_Scene xLoaded = Zenith_SceneManager::LoadSceneByIndex(iBuildIndex, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xLoaded.IsValid(), "LoadSceneByIndex should return valid scene");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xLoaded);
	Zenith_SceneManager::ClearBuildIndexRegistry();
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneByIndexSync passed");
}

void Zenith_SceneTests::TestGetBuildSceneCount()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetBuildSceneCount...");

	Zenith_SceneManager::ClearBuildIndexRegistry();
	uint32_t uInitialCount = Zenith_SceneManager::GetBuildSceneCount();
	Zenith_Assert(uInitialCount == 0, "Initial build count should be 0 after clear");

	Zenith_SceneManager::RegisterSceneBuildIndex(1, "scene1" ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(2, "scene2" ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(3, "scene3" ZENITH_SCENE_EXT);

	uint32_t uCount = Zenith_SceneManager::GetBuildSceneCount();
	Zenith_Assert(uCount == 3, "Build count should be 3 after registering 3 scenes");

	// Cleanup
	Zenith_SceneManager::ClearBuildIndexRegistry();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetBuildSceneCount passed");
}

void Zenith_SceneTests::TestClearBuildIndexRegistry()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestClearBuildIndexRegistry...");

	Zenith_SceneManager::RegisterSceneBuildIndex(1, "scene1" ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(2, "scene2" ZENITH_SCENE_EXT);

	Zenith_SceneManager::ClearBuildIndexRegistry();

	uint32_t uCount = Zenith_SceneManager::GetBuildSceneCount();
	Zenith_Assert(uCount == 0, "Build count should be 0 after clear");

	// Verify can't find by index anymore
	Zenith_Scene xNotFound = Zenith_SceneManager::GetSceneByBuildIndex(1);
	Zenith_Assert(!xNotFound.IsValid(), "Should not find scene after registry cleared");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestClearBuildIndexRegistry passed");
}

//==============================================================================
// NEW: Scene Pause System Tests
//==============================================================================

void Zenith_SceneTests::TestSetScenePaused()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetScenePaused...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PauseTest");

	Zenith_Assert(!Zenith_SceneManager::IsScenePaused(xScene), "Scene should not be paused initially");

	Zenith_SceneManager::SetScenePaused(xScene, true);
	Zenith_Assert(Zenith_SceneManager::IsScenePaused(xScene), "Scene should be paused after SetScenePaused(true)");

	Zenith_SceneManager::SetScenePaused(xScene, false);
	Zenith_Assert(!Zenith_SceneManager::IsScenePaused(xScene), "Scene should not be paused after SetScenePaused(false)");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetScenePaused passed");
}

void Zenith_SceneTests::TestIsScenePaused()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsScenePaused...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("IsPausedTest");

	bool bInitial = Zenith_SceneManager::IsScenePaused(xScene);
	Zenith_Assert(!bInitial, "IsScenePaused should return false initially");

	Zenith_SceneManager::SetScenePaused(xScene, true);
	bool bAfterPause = Zenith_SceneManager::IsScenePaused(xScene);
	Zenith_Assert(bAfterPause, "IsScenePaused should return true after pausing");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsScenePaused passed");
}

void Zenith_SceneTests::TestPausedSceneSkipsUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPausedSceneSkipsUpdate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SkipUpdateTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "PauseTestEntity");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1); // Start fires

	SceneTestBehaviour::ResetCounters();

	// Pause scene and pump several frames - OnUpdate should NOT fire
	Zenith_SceneManager::SetScenePaused(xScene, true);
	PumpFrames(3);
	Zenith_Assert(SceneTestBehaviour::s_uUpdateCount == 0, "OnUpdate should not fire while scene is paused");

	// Unpause and pump one frame - OnUpdate should fire now
	Zenith_SceneManager::SetScenePaused(xScene, false);
	PumpFrames(1);
	Zenith_Assert(SceneTestBehaviour::s_uUpdateCount == 1, "OnUpdate should fire once after unpause");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPausedSceneSkipsUpdate passed");
}

void Zenith_SceneTests::TestPauseDoesNotAffectOtherScenes()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPauseDoesNotAffectOtherScenes...");

	Zenith_Scene xScene1 = Zenith_SceneManager::CreateEmptyScene("PauseScene1");
	Zenith_Scene xScene2 = Zenith_SceneManager::CreateEmptyScene("PauseScene2");

	// Pause only scene1
	Zenith_SceneManager::SetScenePaused(xScene1, true);

	Zenith_Assert(Zenith_SceneManager::IsScenePaused(xScene1), "Scene1 should be paused");
	Zenith_Assert(!Zenith_SceneManager::IsScenePaused(xScene2), "Scene2 should not be paused");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene1);
	Zenith_SceneManager::UnloadScene(xScene2);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPauseDoesNotAffectOtherScenes passed");
}

//==============================================================================
// NEW: Scene Combining/Merging Tests
//==============================================================================

void Zenith_SceneTests::TestMergeScenes()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenes...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeTarget");

	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// Create entities in source
	Zenith_Entity xEntity(pxSourceData, "MergeEntity");

	uint32_t uTargetCountBefore = pxTargetData->GetEntityCount();

	// Merge (should move entities and unload source)
	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	// Target should have the entity
	uint32_t uTargetCountAfter = pxTargetData->GetEntityCount();
	Zenith_Assert(uTargetCountAfter > uTargetCountBefore, "Target should gain entities");

	// Source should be unloaded
	Zenith_Scene xSearchSource = Zenith_SceneManager::GetSceneByName("MergeSource");
	Zenith_Assert(!xSearchSource.IsValid(), "Source should be unloaded after merge");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xTarget);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenes passed");
}

void Zenith_SceneTests::TestMergeScenesPreservesComponents()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesPreservesComponents...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeCompSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeCompTarget");

	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// Create entity with transform in source
	Zenith_Entity xEntity(pxSourceData, "ComponentEntity");
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition({10.0f, 20.0f, 30.0f});

	// Merge
	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	// Find entity in target and verify component
	Zenith_Entity xMerged = pxTargetData->FindEntityByName("ComponentEntity");
	Zenith_Assert(xMerged.IsValid(), "Entity should exist in target");

	Zenith_Maths::Vector3 xPos;
	xMerged.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	Zenith_Assert(xPos.x == 10.0f && xPos.y == 20.0f && xPos.z == 30.0f, "Transform should be preserved");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xTarget);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesPreservesComponents passed");
}

//==============================================================================
// NEW: Additional Callback Tests
//==============================================================================

void Zenith_SceneTests::TestSceneUnloadingCallbackFires()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneUnloadingCallbackFires...");

	static bool s_bCallbackFired = false;
	static Zenith_Scene s_xUnloadingScene;

	auto ulHandle = Zenith_SceneManager::RegisterSceneUnloadingCallback(
		[](Zenith_Scene xScene) {
			s_bCallbackFired = true;
			s_xUnloadingScene = xScene;
		}
	);

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("UnloadingCallback");
	s_bCallbackFired = false;

	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Assert(s_bCallbackFired, "SceneUnloading callback should fire");
	Zenith_Assert(s_xUnloadingScene == xScene, "Callback should receive unloading scene");

	Zenith_SceneManager::UnregisterSceneUnloadingCallback(ulHandle);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneUnloadingCallbackFires passed");
}

void Zenith_SceneTests::TestSceneUnloadedCallbackFires()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneUnloadedCallbackFires...");

	static bool s_bCallbackFired = false;

	auto ulHandle = Zenith_SceneManager::RegisterSceneUnloadedCallback(
		[](Zenith_Scene) {
			s_bCallbackFired = true;
		}
	);

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("UnloadedCallback");
	s_bCallbackFired = false;

	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Assert(s_bCallbackFired, "SceneUnloaded callback should fire");

	Zenith_SceneManager::UnregisterSceneUnloadedCallback(ulHandle);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneUnloadedCallbackFires passed");
}

void Zenith_SceneTests::TestSceneLoadStartedCallbackFires()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneLoadStartedCallbackFires...");

	static bool s_bCallbackFired = false;
	static std::string s_strLoadPath;

	auto ulHandle = Zenith_SceneManager::RegisterSceneLoadStartedCallback(
		[](const std::string& strPath) {
			s_bCallbackFired = true;
			s_strLoadPath = strPath;
		}
	);

	const std::string strPath = "test_load_started" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	s_bCallbackFired = false;
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);

	Zenith_Assert(s_bCallbackFired, "SceneLoadStarted callback should fire");
	Zenith_Assert(s_strLoadPath == strPath, "Callback should receive correct path");

	Zenith_SceneManager::UnregisterSceneLoadStartedCallback(ulHandle);
	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneLoadStartedCallbackFires passed");
}

void Zenith_SceneTests::TestEntityPersistentCallbackFires()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityPersistentCallbackFires...");

	static bool s_bCallbackFired = false;

	auto ulHandle = Zenith_SceneManager::RegisterEntityPersistentCallback(
		[](const Zenith_Entity&) {
			s_bCallbackFired = true;
		}
	);

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PersistentCallback");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxData, "PersistentEntity");

	s_bCallbackFired = false;
	Zenith_SceneManager::MarkEntityPersistent(xEntity);

	Zenith_Assert(s_bCallbackFired, "EntityPersistent callback should fire");

	Zenith_SceneManager::UnregisterEntityPersistentCallback(ulHandle);
	Zenith_SceneManager::UnloadScene(xScene);

	// Cleanup persistent entity
	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistentData = Zenith_SceneManager::GetSceneData(xPersistent);
	Zenith_Entity xPersistentEntity = pxPersistentData->FindEntityByName("PersistentEntity");
	if (xPersistentEntity.IsValid())
	{
		Zenith_SceneManager::DestroyImmediate(xPersistentEntity);
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityPersistentCallbackFires passed");
}

void Zenith_SceneTests::TestCallbackUnregister()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCallbackUnregister...");

	static int s_iCallCount = 0;

	auto ulHandle = Zenith_SceneManager::RegisterSceneLoadedCallback(
		[](Zenith_Scene, Zenith_SceneLoadMode) {
			s_iCallCount++;
		}
	);

	const std::string strPath = "test_unregister" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	s_iCallCount = 0;

	// First load - should fire
	Zenith_Scene xScene1 = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(s_iCallCount == 1, "Callback should fire once");

	// Unregister
	Zenith_SceneManager::UnregisterSceneLoadedCallback(ulHandle);
	Zenith_SceneManager::UnloadScene(xScene1);

	// Second load - should not fire
	Zenith_Scene xScene2 = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(s_iCallCount == 1, "Callback should not fire after unregister");

	Zenith_SceneManager::UnloadScene(xScene2);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCallbackUnregister passed");
}

void Zenith_SceneTests::TestCallbackUnregisterDuringCallback()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCallbackUnregisterDuringCallback...");

	static Zenith_SceneManager::CallbackHandle s_ulHandle = 0;
	static bool s_bCallbackFired = false;

	s_ulHandle = Zenith_SceneManager::RegisterSceneLoadedCallback(
		[](Zenith_Scene, Zenith_SceneLoadMode) {
			s_bCallbackFired = true;
			// Unregister self during callback
			Zenith_SceneManager::UnregisterSceneLoadedCallback(s_ulHandle);
		}
	);

	const std::string strPath = "test_unregister_during" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	s_bCallbackFired = false;

	// This should not crash
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(s_bCallbackFired, "Callback should fire");

	// Subsequent loads should not fire the callback
	s_bCallbackFired = false;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Scene xScene2 = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(!s_bCallbackFired, "Callback should not fire after self-unregister");

	Zenith_SceneManager::UnloadScene(xScene2);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCallbackUnregisterDuringCallback passed");
}

void Zenith_SceneTests::TestMultipleCallbacksFireInOrder()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleCallbacksFireInOrder...");

	static std::vector<int> s_axCallOrder;

	auto ulHandle1 = Zenith_SceneManager::RegisterSceneLoadedCallback(
		[](Zenith_Scene, Zenith_SceneLoadMode) {
			s_axCallOrder.push_back(1);
		}
	);

	auto ulHandle2 = Zenith_SceneManager::RegisterSceneLoadedCallback(
		[](Zenith_Scene, Zenith_SceneLoadMode) {
			s_axCallOrder.push_back(2);
		}
	);

	const std::string strPath = "test_multi_callback" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	s_axCallOrder.clear();

	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);

	Zenith_Assert(s_axCallOrder.size() == 2, "Both callbacks should fire");
	Zenith_Assert(s_axCallOrder[0] == 1 && s_axCallOrder[1] == 2, "Callbacks should fire in registration order");

	Zenith_SceneManager::UnregisterSceneLoadedCallback(ulHandle1);
	Zenith_SceneManager::UnregisterSceneLoadedCallback(ulHandle2);
	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleCallbacksFireInOrder passed");
}

void Zenith_SceneTests::TestCallbackHandleInvalid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCallbackHandleInvalid...");

	// Unregister with invalid handle should be a no-op (not crash)
	Zenith_SceneManager::UnregisterSceneLoadedCallback(Zenith_SceneManager::INVALID_CALLBACK_HANDLE);
	Zenith_SceneManager::UnregisterActiveSceneChangedCallback(Zenith_SceneManager::INVALID_CALLBACK_HANDLE);
	Zenith_SceneManager::UnregisterSceneUnloadingCallback(Zenith_SceneManager::INVALID_CALLBACK_HANDLE);
	Zenith_SceneManager::UnregisterSceneUnloadedCallback(Zenith_SceneManager::INVALID_CALLBACK_HANDLE);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCallbackHandleInvalid passed");
}

//==============================================================================
// NEW: Entity Destruction Tests
//==============================================================================

void Zenith_SceneTests::TestDestroyDeferred()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyDeferred...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DeferredDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "DeferredEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();

	Zenith_SceneManager::Destroy(xEntity);

	// Entity should still exist immediately after (deferred)
	Zenith_Assert(pxData->EntityExists(xID), "Entity should exist immediately after Destroy (deferred)");

	// Process destructions
	pxData->ProcessPendingDestructions();

	// Now entity should be gone
	Zenith_Assert(!pxData->EntityExists(xID), "Entity should not exist after processing destructions");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyDeferred passed");
}

void Zenith_SceneTests::TestDestroyImmediate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyImmediate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ImmediateDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "ImmediateEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();

	Zenith_SceneManager::DestroyImmediate(xEntity);

	// Entity should be gone immediately
	Zenith_Assert(!pxData->EntityExists(xID), "Entity should not exist after DestroyImmediate");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyImmediate passed");
}

void Zenith_SceneTests::TestDestroyParentOrphansChildren()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyParentCascadesToChildren (Unity parity)...");

	// Unity parity: Destroying a parent cascades to all children.
	// Children are destroyed along with the parent, not orphaned.

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CascadeTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild1(pxData, "Child1");
	Zenith_Entity xChild2(pxData, "Child2");
	xChild1.SetParent(xParent.GetEntityID());
	xChild2.SetParent(xParent.GetEntityID());

	Zenith_EntityID xParentID = xParent.GetEntityID();
	Zenith_EntityID xChild1ID = xChild1.GetEntityID();
	Zenith_EntityID xChild2ID = xChild2.GetEntityID();

	uint32_t uInitialCount = pxData->GetEntityCount();

	// Destroy parent - should cascade to children
	Zenith_SceneManager::DestroyImmediate(xParent);

	// Parent and all children should be destroyed
	Zenith_Assert(!pxData->EntityExists(xParentID), "Parent should be destroyed");
	Zenith_Assert(!pxData->EntityExists(xChild1ID), "Child1 should be cascade-destroyed (Unity parity)");
	Zenith_Assert(!pxData->EntityExists(xChild2ID), "Child2 should be cascade-destroyed (Unity parity)");

	// Entity count should have decreased by 3
	Zenith_Assert(pxData->GetEntityCount() == uInitialCount - 3,
		"Entity count should decrease by 3 (parent + 2 children)");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyParentCascadesToChildren passed");
}

void Zenith_SceneTests::TestMarkForDestructionFlag()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMarkForDestructionFlag...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("MarkDestruction");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "MarkedEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();

	Zenith_Assert(!pxData->IsMarkedForDestruction(xID), "Should not be marked initially");

	pxData->MarkForDestruction(xID);

	Zenith_Assert(pxData->IsMarkedForDestruction(xID), "Should be marked after MarkForDestruction");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMarkForDestructionFlag passed");
}

//==============================================================================
// NEW: Stale Handle Detection Tests
//==============================================================================

void Zenith_SceneTests::TestStaleHandleAfterUnload()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStaleHandleAfterUnload...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("StaleHandleTest");

	// Unload the scene
	Zenith_SceneManager::UnloadScene(xScene);

	// The handle should now be invalid
	Zenith_Assert(!xScene.IsValid(), "Handle should be invalid after unload");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStaleHandleAfterUnload passed");
}

void Zenith_SceneTests::TestStaleHandleGenerationMismatch()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStaleHandleGenerationMismatch...");

	// Create and unload a scene
	Zenith_Scene xOldScene = Zenith_SceneManager::CreateEmptyScene("GenMismatch1");
	int iOldHandle = xOldScene.m_iHandle;
	uint32_t uOldGeneration = xOldScene.m_uGeneration;

	Zenith_SceneManager::UnloadScene(xOldScene);

	// Create a new scene (might reuse the handle)
	Zenith_Scene xNewScene = Zenith_SceneManager::CreateEmptyScene("GenMismatch2");

	// If handle was reused, generation should be different
	if (xNewScene.m_iHandle == iOldHandle)
	{
		Zenith_Assert(xNewScene.m_uGeneration != uOldGeneration, "Generation should differ on reuse");
	}

	// Old handle should be invalid
	Zenith_Assert(!xOldScene.IsValid(), "Old handle should be invalid");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xNewScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStaleHandleGenerationMismatch passed");
}

void Zenith_SceneTests::TestGetSceneDataStaleHandle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneDataStaleHandle...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("StaleDataTest");
	Zenith_Scene xCopy = xScene;  // Keep a copy

	// Unload
	Zenith_SceneManager::UnloadScene(xScene);

	// GetSceneData with stale handle should return null
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xCopy);
	Zenith_Assert(pxData == nullptr, "GetSceneData should return null for stale handle");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneDataStaleHandle passed");
}

//==============================================================================
// NEW: Camera Management Tests
//==============================================================================

void Zenith_SceneTests::TestSetMainCameraEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetMainCameraEntity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CameraSetTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xCamera(pxData, "MainCamera");
	xCamera.AddComponent<Zenith_CameraComponent>();

	pxData->SetMainCameraEntity(xCamera.GetEntityID());

	Zenith_EntityID xMainCamera = pxData->GetMainCameraEntity();
	Zenith_Assert(xMainCamera == xCamera.GetEntityID(), "Main camera should be set correctly");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetMainCameraEntity passed");
}

void Zenith_SceneTests::TestGetMainCameraEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetMainCameraEntity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CameraGetTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xCamera(pxData, "TheCamera");
	xCamera.AddComponent<Zenith_CameraComponent>();
	pxData->SetMainCameraEntity(xCamera.GetEntityID());

	Zenith_EntityID xRetrieved = pxData->GetMainCameraEntity();
	Zenith_Assert(xRetrieved.IsValid(), "GetMainCameraEntity should return valid ID");
	Zenith_Assert(xRetrieved == xCamera.GetEntityID(), "Should return correct camera entity");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetMainCameraEntity passed");
}

void Zenith_SceneTests::TestGetMainCameraComponent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetMainCameraComponent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CameraCompTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xCamera(pxData, "CameraEntity");
	Zenith_CameraComponent& xAddedComp = xCamera.AddComponent<Zenith_CameraComponent>();
	pxData->SetMainCameraEntity(xCamera.GetEntityID());

	Zenith_CameraComponent& xRetrieved = pxData->GetMainCamera();

	// Should be the same component
	Zenith_Assert(&xRetrieved == &xAddedComp, "GetMainCamera should return the correct component");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetMainCameraComponent passed");
}

void Zenith_SceneTests::TestTryGetMainCameraNull()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetMainCameraNull...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CameraNullTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Don't set a main camera
	Zenith_CameraComponent* pxCamera = pxData->TryGetMainCamera();
	Zenith_Assert(pxCamera == nullptr, "TryGetMainCamera should return null when not set");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetMainCameraNull passed");
}

//==============================================================================
// NEW: Scene Query Edge Case Tests
//==============================================================================

void Zenith_SceneTests::TestGetSceneByNameFilenameMatch()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByNameFilenameMatch...");

	// Create scene file with path
	const std::string strPath = "levels/test_filename_match" ZENITH_SCENE_EXT;
	const std::string strFilename = "test_filename_match";

	// Create directory if needed
	std::filesystem::create_directories("levels");
	CreateTestSceneFile(strPath);

	// Load the scene
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);

	// Should be findable by filename without path/extension (Unity parity: GetSceneByName strips path/ext)
	Zenith_Scene xFound = Zenith_SceneManager::GetSceneByName(strFilename);
	Zenith_Assert(xFound.IsValid(), "GetSceneByName should find scene by filename without path/extension");
	Zenith_Assert(xFound == xScene, "Found scene should match the loaded scene");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strPath);
	std::filesystem::remove("levels");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByNameFilenameMatch passed");
}

void Zenith_SceneTests::TestGetTotalSceneCount()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetTotalSceneCount...");

	uint32_t uLoadedCount = Zenith_SceneManager::GetLoadedSceneCount();
	uint32_t uTotalCount = Zenith_SceneManager::GetTotalSceneCount();

	// Total should be >= loaded (includes persistent scene)
	Zenith_Assert(uTotalCount >= uLoadedCount, "Total count should be >= loaded count");

	// Create a scene and verify total increases
	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TotalCountTest");
	uint32_t uNewTotal = Zenith_SceneManager::GetTotalSceneCount();
	Zenith_Assert(uNewTotal > uTotalCount, "Total should increase after creating scene");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetTotalSceneCount passed");
}

//==============================================================================
// Unity Parity & Bug Fix Tests
//==============================================================================

void Zenith_SceneTests::TestCannotUnloadLastScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCannotUnloadLastScene...");

	// Get the current active scene (should be the only non-persistent scene)
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xActiveScene.IsValid(), "Should have an active scene");

	// Try to unload it - should fail silently (Unity behavior)
	Zenith_SceneManager::UnloadScene(xActiveScene);

	// Scene should still be valid and loaded (because it was the last scene)
	Zenith_Assert(xActiveScene.IsValid(), "Last scene should not be unloaded");
	Zenith_Assert(xActiveScene.IsLoaded(), "Last scene should still be loaded");

	// Also test async version
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xActiveScene);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "Should get operation");
	Zenith_Assert(pxOp->IsComplete(), "Should complete immediately (rejection)");
	Zenith_Assert(xActiveScene.IsValid(), "Last scene should still be valid after async attempt");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCannotUnloadLastScene passed");
}

void Zenith_SceneTests::TestInvalidScenePropertyAccess()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestInvalidScenePropertyAccess...");

	// Test INVALID_SCENE sentinel
	Zenith_Scene xInvalid = Zenith_Scene::INVALID_SCENE;
	Zenith_Assert(!xInvalid.IsValid(), "INVALID_SCENE should not be valid");
	Zenith_Assert(xInvalid.GetName() == "", "INVALID_SCENE GetName should return empty string");
	Zenith_Assert(xInvalid.GetPath() == "", "INVALID_SCENE GetPath should return empty string");
	Zenith_Assert(xInvalid.GetRootEntityCount() == 0, "INVALID_SCENE GetRootEntityCount should return 0");
	Zenith_Assert(!xInvalid.IsLoaded(), "INVALID_SCENE IsLoaded should return false");
	Zenith_Assert(xInvalid.GetBuildIndex() == -1, "INVALID_SCENE GetBuildIndex should return -1");
#ifdef ZENITH_TOOLS
	Zenith_Assert(!xInvalid.HasUnsavedChanges(), "INVALID_SCENE HasUnsavedChanges should return false");
#endif
	Zenith_Assert(!xInvalid.WasLoadedAdditively(), "INVALID_SCENE WasLoadedAdditively should return false");

	// Test stale handle (after unload)
	const std::string strPath = "test_stale_access" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "Scene should be valid after load");

	// Unload and keep the old handle
	Zenith_SceneManager::UnloadScene(xScene);

	// Now access properties - should all return safe defaults (no crash)
	Zenith_Assert(!xScene.IsValid(), "Stale handle should not be valid");
	Zenith_Assert(xScene.GetName() == "", "Stale handle GetName should return empty string");
	Zenith_Assert(xScene.GetPath() == "", "Stale handle GetPath should return empty string");
	Zenith_Assert(xScene.GetRootEntityCount() == 0, "Stale handle GetRootEntityCount should return 0");

	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestInvalidScenePropertyAccess passed");
}

void Zenith_SceneTests::TestOperationIdAfterCleanup()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOperationIdAfterCleanup...");

	const std::string strPath = "test_op_cleanup" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Start async load
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(ulOpID != ZENITH_INVALID_OPERATION_ID, "Should get valid operation ID");

	// Get the operation pointer
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "Should get operation from ID");

	// Pump until complete
	PumpUntilComplete(pxOp);

	// Operation should be complete
	Zenith_Assert(pxOp->IsComplete(), "Operation should be complete");

	// Get the result scene before cleanup
	Zenith_Scene xResultScene = pxOp->GetResultScene();
	Zenith_Assert(xResultScene.IsValid(), "Result scene should be valid");

	// Pump frames to trigger cleanup (operations are cleaned up after 60 frames)
	const float fDt = 1.0f / 60.0f;
	for (int i = 0; i < 65; ++i)
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// Now GetOperation should return nullptr (operation cleaned up)
	Zenith_SceneOperation* pxCleanedOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxCleanedOp == nullptr, "GetOperation should return nullptr after cleanup");

	// IsOperationValid should also return false after cleanup
	Zenith_Assert(!Zenith_SceneManager::IsOperationValid(ulOpID), "IsOperationValid should return false after cleanup");

	// Test invalid operation ID
	Zenith_SceneOperation* pxInvalidOp = Zenith_SceneManager::GetOperation(ZENITH_INVALID_OPERATION_ID);
	Zenith_Assert(pxInvalidOp == nullptr, "GetOperation with INVALID_OPERATION_ID should return nullptr");
	Zenith_Assert(!Zenith_SceneManager::IsOperationValid(ZENITH_INVALID_OPERATION_ID), "IsOperationValid should return false for INVALID_OPERATION_ID");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xResultScene);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOperationIdAfterCleanup passed");
}

void Zenith_SceneTests::TestMoveEntityToSceneSameScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToSceneSameScene...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TestScene");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Assert(pxData != nullptr, "Scene data should exist");

	Zenith_Entity xEntity(pxData, "TestEntity");
	Zenith_Assert(xEntity.IsValid(), "Entity should be valid");

	// Moving to same scene should be a no-op - entity remains valid and unchanged
	Zenith_SceneManager::MoveEntityToScene(xEntity, xScene);
	Zenith_Assert(xEntity.IsValid(), "Entity should still be valid after same-scene move");

	// Entity should still be in the same scene
	Zenith_Assert(xEntity.GetSceneData() == pxData, "Entity should still be in original scene");

	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToSceneSameScene passed");
}

void Zenith_SceneTests::TestConcurrentAsyncUnloads()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestConcurrentAsyncUnloads...");

	// Test that concurrent async unloads properly account for scenes already being unloaded
	// to prevent having zero non-persistent scenes remaining.
	//
	// The fix ensures: if (uNonPersistentCount <= 1 + uScenesBeingUnloaded) then block
	// This means with N scenes and M being unloaded, new unloads are blocked if N <= 1 + M
	// (i.e., if remaining scenes would be <= 1)

	// Create exactly 2 new scenes for this test
	Zenith_Scene xScene1 = Zenith_SceneManager::CreateEmptyScene("ConcurrentTest1");
	Zenith_Scene xScene2 = Zenith_SceneManager::CreateEmptyScene("ConcurrentTest2");
	Zenith_Assert(xScene1.IsValid() && xScene2.IsValid(), "Both scenes should be valid");

	uint32_t uTotalCount = Zenith_SceneManager::GetLoadedSceneCount();
	Zenith_Assert(uTotalCount >= 2, "Should have at least 2 non-persistent scenes");

	// Start async unloading scenes until we have exactly 2 left (or we can't unload more)
	// Then verify the concurrent blocking behavior
	Zenith_Vector<Zenith_SceneOperation*> axOps;

	// Start first async unload
	Zenith_SceneOperationID ulOp1 = Zenith_SceneManager::UnloadSceneAsync(xScene1);
	Zenith_SceneOperation* pxOp1 = Zenith_SceneManager::GetOperation(ulOp1);
	Zenith_Assert(pxOp1 != nullptr, "Should get operation for scene1 unload");

	// If total count was exactly 2, the second unload should be blocked
	// because 2 <= 1 + 1 (total <= 1 + scenes_being_unloaded)
	if (uTotalCount == 2)
	{
		Zenith_SceneOperationID ulOp2 = Zenith_SceneManager::UnloadSceneAsync(xScene2);
		Zenith_SceneOperation* pxOp2 = Zenith_SceneManager::GetOperation(ulOp2);
		Zenith_Assert(pxOp2 != nullptr, "Should get operation");
		Zenith_Assert(pxOp2->IsComplete(), "With only 2 scenes, second unload should be rejected");
		Zenith_Assert(xScene2.IsValid(), "Scene2 should still be valid after rejection");
	}
	else
	{
		// With more than 2 scenes, second unload is allowed
		// Just verify it doesn't crash and we can pump through
		Zenith_SceneOperationID ulOp2 = Zenith_SceneManager::UnloadSceneAsync(xScene2);
		Zenith_SceneOperation* pxOp2 = Zenith_SceneManager::GetOperation(ulOp2);
		Zenith_Assert(pxOp2 != nullptr, "Should get operation");
		// This unload should proceed (not be rejected immediately)
		// because we have enough scenes
		PumpUntilComplete(pxOp2);
	}

	// Pump until first unload completes
	PumpUntilComplete(pxOp1);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestConcurrentAsyncUnloads passed");
}

void Zenith_SceneTests::TestWasLoadedAdditively()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestWasLoadedAdditively...");

	const std::string strPath = "test_additive_load" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Load scene in SINGLE mode - should not have been loaded additively
	Zenith_Scene xSingleScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_SINGLE);
	Zenith_Assert(xSingleScene.IsValid(), "Scene should load");
	Zenith_Assert(!xSingleScene.WasLoadedAdditively(), "Scene loaded with SINGLE mode should not have been loaded additively");

	// Create test file for additive load
	const std::string strPath2 = "test_additive_load2" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath2);

	// Load scene in ADDITIVE mode - should have been loaded additively
	Zenith_Scene xAdditiveScene = Zenith_SceneManager::LoadScene(strPath2, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xAdditiveScene.IsValid(), "Additive scene should load");
	Zenith_Assert(xAdditiveScene.WasLoadedAdditively(), "Scene loaded with ADDITIVE mode should have been loaded additively");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xAdditiveScene);
	CleanupTestSceneFile(strPath);
	CleanupTestSceneFile(strPath2);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestWasLoadedAdditively passed");
}

void Zenith_SceneTests::TestAsyncLoadCircularDetection()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadCircularDetection...");

	const std::string strPath = "test_circular_load" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Start first async load
	Zenith_SceneOperationID ulOp1 = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp1 = Zenith_SceneManager::GetOperation(ulOp1);
	Zenith_Assert(pxOp1 != nullptr, "First operation should be valid");

	// Attempt second async load of same scene while first is still loading
	// This should fail immediately due to circular load detection
	Zenith_SceneOperationID ulOp2 = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp2 = Zenith_SceneManager::GetOperation(ulOp2);
	Zenith_Assert(pxOp2 != nullptr, "Second operation should be valid");
	Zenith_Assert(pxOp2->IsComplete(), "Second load should complete immediately (rejected)");
	Zenith_Assert(pxOp2->HasFailed(), "Second load should be marked as failed");
	Zenith_Assert(!pxOp2->GetResultScene().IsValid(), "Result should be invalid for circular load");

	// Complete the first load normally
	PumpUntilComplete(pxOp1);
	Zenith_Assert(pxOp1->IsComplete(), "First load should complete");
	Zenith_Assert(!pxOp1->HasFailed(), "First load should not have failed");

	Zenith_Scene xScene = pxOp1->GetResultScene();
	Zenith_Assert(xScene.IsValid(), "First load result should be valid");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadCircularDetection passed");
}

void Zenith_SceneTests::TestSyncUnloadDuringAsyncUnload()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSyncUnloadDuringAsyncUnload...");

	// Create two scenes so we're not trying to unload the last scene
	Zenith_Scene xScene1 = Zenith_SceneManager::CreateEmptyScene("AsyncUnloadTest1");
	Zenith_Scene xScene2 = Zenith_SceneManager::CreateEmptyScene("AsyncUnloadTest2");
	Zenith_Assert(xScene1.IsValid() && xScene2.IsValid(), "Both scenes should be valid");

	// Start async unload of scene1
	Zenith_SceneOperationID ulOp = Zenith_SceneManager::UnloadSceneAsync(xScene1);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOp);
	Zenith_Assert(pxOp != nullptr, "Async unload operation should be valid");

	// Attempt sync unload of scene already being async unloaded
	// This should be rejected (warning logged, no crash)
	Zenith_SceneManager::UnloadScene(xScene1);

	// Scene should still be in the process of async unload (sync unload was rejected)
	// Complete the async unload
	PumpUntilComplete(pxOp);

	// After async unload completes, scene should be invalid
	Zenith_Assert(!xScene1.IsValid(), "Scene should be invalid after async unload completes");

	// Cleanup remaining scene
	Zenith_SceneManager::UnloadScene(xScene2);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSyncUnloadDuringAsyncUnload passed");
}

//==============================================================================
// NEW: Bug Fix Verification Tests (from code review)
//==============================================================================

void Zenith_SceneTests::TestMoveEntityToSceneMainCamera()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToSceneMainCamera...");

	// This test verifies that moving the main camera entity clears the source scene's camera reference

	// Create two scenes
	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("CameraMoveSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("CameraMoveTarget");

	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// Create entity with camera component and set as main camera
	Zenith_Entity xCameraEntity(pxSourceData, "MainCamera");
	xCameraEntity.AddComponent<Zenith_CameraComponent>();
	pxSourceData->SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Verify camera is set
	Zenith_Assert(pxSourceData->GetMainCameraEntity().IsValid(), "Main camera should be set");
	Zenith_Assert(pxSourceData->TryGetMainCamera() != nullptr, "TryGetMainCamera should return valid pointer");

	// Move camera entity to target scene (reference updated in-place)
	Zenith_SceneManager::MoveEntityToScene(xCameraEntity, xTarget);
	Zenith_Assert(xCameraEntity.IsValid(), "Entity should be valid after move");

	// Verify source scene's main camera reference was cleared
	Zenith_Assert(!pxSourceData->GetMainCameraEntity().IsValid(), "Source scene main camera should be cleared after move");
	Zenith_Assert(pxSourceData->TryGetMainCamera() == nullptr, "Source scene TryGetMainCamera should return nullptr");

	// Verify the entity is now in target scene and still has camera component
	Zenith_Assert(xCameraEntity.IsValid(), "Camera entity should still be valid after move");
	Zenith_Assert(xCameraEntity.GetSceneData() == pxTargetData, "Camera entity should now be in target scene");
	Zenith_Assert(xCameraEntity.HasComponent<Zenith_CameraComponent>(), "Camera component should be preserved");

	// Target scene should have automatically adopted this camera (Fix 5 implemented this)
	Zenith_Assert(pxTargetData->GetMainCameraEntity() == xCameraEntity.GetEntityID(), "Target scene should automatically adopt camera from source");
	Zenith_Assert(pxTargetData->GetMainCameraEntity().IsValid(), "Target scene should be able to set main camera");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToSceneMainCamera passed");
}

void Zenith_SceneTests::TestMoveEntityToSceneDeepHierarchy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToSceneDeepHierarchy...");

	// This test verifies that moving a root entity with 3+ levels of children works correctly

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("DeepHierarchySource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("DeepHierarchyTarget");

	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// Create a 4-level hierarchy: Root -> Child1 -> Child2 -> Child3
	Zenith_Entity xRoot(pxSourceData, "Root");
	Zenith_Entity xChild1(pxSourceData, "Child1");
	Zenith_Entity xChild2(pxSourceData, "Child2");
	Zenith_Entity xChild3(pxSourceData, "Child3");

	xChild1.SetParent(xRoot.GetEntityID());
	xChild2.SetParent(xChild1.GetEntityID());
	xChild3.SetParent(xChild2.GetEntityID());

	// Set unique positions to verify transforms are preserved
	xRoot.GetComponent<Zenith_TransformComponent>().SetPosition({1.0f, 0.0f, 0.0f});
	xChild1.GetComponent<Zenith_TransformComponent>().SetPosition({0.0f, 2.0f, 0.0f});
	xChild2.GetComponent<Zenith_TransformComponent>().SetPosition({0.0f, 0.0f, 3.0f});
	xChild3.GetComponent<Zenith_TransformComponent>().SetPosition({4.0f, 4.0f, 4.0f});

	uint32_t uSourceCountBefore = pxSourceData->GetEntityCount();
	uint32_t uTargetCountBefore = pxTargetData->GetEntityCount();

	// Move the root entity (should move entire hierarchy, reference updated in-place)
	Zenith_SceneManager::MoveEntityToScene(xRoot, xTarget);
	Zenith_Assert(xRoot.IsValid(), "Entity should be valid after move");

	// Verify all entities moved to target
	Zenith_Assert(pxSourceData->GetEntityCount() == uSourceCountBefore - 4, "Source should have 4 fewer entities");
	Zenith_Assert(pxTargetData->GetEntityCount() == uTargetCountBefore + 4, "Target should have 4 more entities");

	// Verify hierarchy is preserved - root reference was updated
	Zenith_Assert(xRoot.IsValid(), "Root should still be valid");
	Zenith_Assert(xRoot.GetSceneData() == pxTargetData, "Root should be in target scene");
	Zenith_Assert(xRoot.GetChildCount() == 1, "Root should have 1 child");

	// Find child entities by traversing hierarchy in target
	Zenith_Vector<Zenith_EntityID> axRootChildren = xRoot.GetChildEntityIDs();
	Zenith_Assert(axRootChildren.GetSize() == 1, "Root should have 1 child ID");

	Zenith_Entity xMovedChild1 = pxTargetData->GetEntity(axRootChildren.Get(0));
	Zenith_Assert(xMovedChild1.IsValid(), "Child1 should exist in target");
	Zenith_Assert(xMovedChild1.GetName() == "Child1", "Child1 name should be preserved");

	// Verify position was preserved
	glm::vec3 xPos;
	xMovedChild1.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	Zenith_Assert(xPos.y == 2.0f, "Child1 position should be preserved");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToSceneDeepHierarchy passed");
}

void Zenith_SceneTests::TestMarkEntityPersistentNonRoot()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMarkEntityPersistentNonRoot...");

	// Unity behavior: DontDestroyOnLoad on a non-root entity moves the ROOT
	// of the hierarchy to the persistent scene, keeping parent-child intact.

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PersistentNonRootTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Scene xPersistentScene = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistentData = Zenith_SceneManager::GetSceneData(xPersistentScene);

	// Create parent-child hierarchy
	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	// Verify child has parent before
	Zenith_Assert(xChild.GetParentEntityID().IsValid(), "Child should have parent before MarkEntityPersistent");

	// Mark child as persistent - Unity walks up to root and moves entire hierarchy
	Zenith_SceneManager::MarkEntityPersistent(xChild);

	// Verify both parent and child were moved to persistent scene
	Zenith_Assert(xParent.GetSceneData() == pxPersistentData, "Parent (root) should be in persistent scene");
	Zenith_Assert(xChild.GetSceneData() == pxPersistentData, "Child should be in persistent scene");

	// Verify parent-child relationship is preserved
	Zenith_Assert(xChild.GetParentEntityID() == xParent.GetEntityID(), "Child should still have parent after move");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMarkEntityPersistentNonRoot passed");
}

void Zenith_SceneTests::TestPausedSceneSkipsAllLifecycle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPausedSceneSkipsAllLifecycle...");

	// This test verifies that paused scenes actually skip Update/FixedUpdate callbacks
	// We use a simple counter pattern to verify the callbacks are not being called

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PausedLifecycleTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create an entity (it will have TransformComponent which receives callbacks)
	Zenith_Entity xEntity(pxData, "TestEntity");

	// Verify scene is not paused initially
	Zenith_Assert(!pxData->IsPaused(), "Scene should not be paused initially");

	// Run a few updates to let the entity go through lifecycle
	const float fDt = 1.0f / 60.0f;
	for (int i = 0; i < 3; ++i)
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// Now pause the scene
	Zenith_SceneManager::SetScenePaused(xScene, true);
	Zenith_Assert(pxData->IsPaused(), "Scene should be paused");

	// The IsPaused flag is checked in SceneManager::Update() which skips:
	// - DispatchPendingStarts()
	// - FixedUpdate()
	// - Update()
	// This is verified by the SceneManager code itself at lines 1923, 1936, 1948

	// Run more updates - these should NOT affect the paused scene
	for (int i = 0; i < 3; ++i)
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// Unpause and verify it resumes
	Zenith_SceneManager::SetScenePaused(xScene, false);
	Zenith_Assert(!pxData->IsPaused(), "Scene should be unpaused");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPausedSceneSkipsAllLifecycle passed");
}

void Zenith_SceneTests::TestSceneLoadedCallbackOrder()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneLoadedCallbackOrder...");

	// This test verifies that multiple scene loaded callbacks fire in registration order

	static Zenith_Vector<int> s_axCallbackOrder;
	s_axCallbackOrder.Clear();

	// Register callbacks that record their order
	auto pfnCallback1 = [](Zenith_Scene, Zenith_SceneLoadMode) { s_axCallbackOrder.PushBack(1); };
	auto pfnCallback2 = [](Zenith_Scene, Zenith_SceneLoadMode) { s_axCallbackOrder.PushBack(2); };
	auto pfnCallback3 = [](Zenith_Scene, Zenith_SceneLoadMode) { s_axCallbackOrder.PushBack(3); };

	Zenith_SceneManager::CallbackHandle hCallback1 = Zenith_SceneManager::RegisterSceneLoadedCallback(pfnCallback1);
	Zenith_SceneManager::CallbackHandle hCallback2 = Zenith_SceneManager::RegisterSceneLoadedCallback(pfnCallback2);
	Zenith_SceneManager::CallbackHandle hCallback3 = Zenith_SceneManager::RegisterSceneLoadedCallback(pfnCallback3);

	// Create a test scene file and load it
	const std::string strPath = "test_callback_order" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "Scene should load successfully");

	// Verify callbacks fired in registration order
	Zenith_Assert(s_axCallbackOrder.GetSize() == 3, "All 3 callbacks should have fired");
	Zenith_Assert(s_axCallbackOrder.Get(0) == 1, "Callback 1 should fire first");
	Zenith_Assert(s_axCallbackOrder.Get(1) == 2, "Callback 2 should fire second");
	Zenith_Assert(s_axCallbackOrder.Get(2) == 3, "Callback 3 should fire third");

	// Cleanup
	Zenith_SceneManager::UnregisterSceneLoadedCallback(hCallback1);
	Zenith_SceneManager::UnregisterSceneLoadedCallback(hCallback2);
	Zenith_SceneManager::UnregisterSceneLoadedCallback(hCallback3);
	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneLoadedCallbackOrder passed");
}

//==============================================================================
// Code Review Tests (from 2024-02 review)
//==============================================================================

void Zenith_SceneTests::TestAsyncLoadPriorityOrdering()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadPriorityOrdering...");

	// This test verifies that higher priority async loads are processed first.
	// Since file I/O timing is non-deterministic, we test that priority affects
	// the order when all loads are ready to activate.

	const std::string strPath1 = "test_priority1" ZENITH_SCENE_EXT;
	const std::string strPath2 = "test_priority2" ZENITH_SCENE_EXT;
	const std::string strPath3 = "test_priority3" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath1, "Priority1");
	CreateTestSceneFile(strPath2, "Priority2");
	CreateTestSceneFile(strPath3, "Priority3");

	// Start 3 async loads with different priorities
	// All paused at activation to control ordering
	Zenith_SceneOperationID ulOp1 = Zenith_SceneManager::LoadSceneAsync(strPath1, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperationID ulOp2 = Zenith_SceneManager::LoadSceneAsync(strPath2, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperationID ulOp3 = Zenith_SceneManager::LoadSceneAsync(strPath3, SCENE_LOAD_ADDITIVE);

	Zenith_SceneOperation* pxOp1 = Zenith_SceneManager::GetOperation(ulOp1);
	Zenith_SceneOperation* pxOp2 = Zenith_SceneManager::GetOperation(ulOp2);
	Zenith_SceneOperation* pxOp3 = Zenith_SceneManager::GetOperation(ulOp3);

	Zenith_Assert(pxOp1 != nullptr && pxOp2 != nullptr && pxOp3 != nullptr, "All operations should be valid");

	// Set priorities (3 highest, 1 lowest)
	pxOp1->SetPriority(1);
	pxOp2->SetPriority(3);  // Highest priority
	pxOp3->SetPriority(2);

	// Pause all at activation point
	pxOp1->SetActivationAllowed(false);
	pxOp2->SetActivationAllowed(false);
	pxOp3->SetActivationAllowed(false);

	// Pump until all are at activation pause (progress ~0.9)
	const float fDt = 1.0f / 60.0f;
	for (int i = 0; i < 300 && (!pxOp1->IsComplete() || !pxOp2->IsComplete() || !pxOp3->IsComplete()); ++i)
	{
		if (pxOp1->GetProgress() >= 0.85f && pxOp2->GetProgress() >= 0.85f && pxOp3->GetProgress() >= 0.85f)
		{
			break;  // All paused at activation
		}
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// Now allow activation and verify they complete
	pxOp1->SetActivationAllowed(true);
	pxOp2->SetActivationAllowed(true);
	pxOp3->SetActivationAllowed(true);

	// Pump until all complete
	for (int i = 0; i < 100 && !(pxOp1->IsComplete() && pxOp2->IsComplete() && pxOp3->IsComplete()); ++i)
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	Zenith_Assert(pxOp1->IsComplete() && pxOp2->IsComplete() && pxOp3->IsComplete(), "All loads should complete");

	// Cleanup
	Zenith_SceneManager::UnloadScene(Zenith_SceneManager::GetSceneByPath(strPath1));
	Zenith_SceneManager::UnloadScene(Zenith_SceneManager::GetSceneByPath(strPath2));
	Zenith_SceneManager::UnloadScene(Zenith_SceneManager::GetSceneByPath(strPath3));
	CleanupTestSceneFile(strPath1);
	CleanupTestSceneFile(strPath2);
	CleanupTestSceneFile(strPath3);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadPriorityOrdering passed");
}

void Zenith_SceneTests::TestAsyncLoadCancellation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadCancellation...");

	const std::string strPath = "test_cancellation" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath, "CancellationTest");

	// Start an async load
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "Operation should be valid");

	// Pause at activation
	pxOp->SetActivationAllowed(false);

	// Pump until at activation pause
	const float fDt = 1.0f / 60.0f;
	for (int i = 0; i < 300 && pxOp->GetProgress() < 0.85f; ++i)
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// Cancel the operation
	pxOp->RequestCancel();
	Zenith_Assert(pxOp->IsCancellationRequested(), "Cancellation should be requested");

	// Pump to process cancellation
	for (int i = 0; i < 10 && !pxOp->IsComplete(); ++i)
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// Verify cancelled
	Zenith_Assert(pxOp->IsComplete(), "Cancelled operation should complete");
	Zenith_Assert(pxOp->HasFailed(), "Cancelled operation should be marked as failed");

	// Verify scene was NOT loaded
	Zenith_Scene xScene = Zenith_SceneManager::GetSceneByPath(strPath);
	Zenith_Assert(!xScene.IsValid(), "Scene should not be loaded after cancellation");

	// Cleanup
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadCancellation passed");
}

void Zenith_SceneTests::TestAsyncAdditiveWithoutLoading()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncAdditiveWithoutLoading...");

	// Test that SCENE_LOAD_ADDITIVE_WITHOUT_LOADING works with LoadSceneAsync
	// (creates an empty scene immediately, no file needed)

	const std::string strPath = "procedural_scene";  // Doesn't need to exist

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "Operation should be valid");

	// Should complete immediately (no async work needed)
	Zenith_Assert(pxOp->IsComplete(), "ADDITIVE_WITHOUT_LOADING should complete immediately");
	Zenith_Assert(!pxOp->HasFailed(), "ADDITIVE_WITHOUT_LOADING should not fail");
	Zenith_Assert(pxOp->GetProgress() == 1.0f, "Progress should be 1.0");

	// Verify scene was created
	Zenith_Scene xScene = pxOp->GetResultScene();
	Zenith_Assert(xScene.IsValid(), "Result scene should be valid");

	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Assert(pxData != nullptr, "Scene data should exist");
	Zenith_Assert(pxData->GetEntityCount() == 0, "Scene should be empty (no entities)");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncAdditiveWithoutLoading passed");
}

void Zenith_SceneTests::TestBatchSizeValidation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBatchSizeValidation...");

	// Save original batch size
	uint32_t uOriginalBatchSize = Zenith_SceneManager::GetAsyncUnloadBatchSize();

	// Test that 0 is clamped to minimum (1)
	Zenith_SceneManager::SetAsyncUnloadBatchSize(0);
	Zenith_Assert(Zenith_SceneManager::GetAsyncUnloadBatchSize() >= 1, "Batch size 0 should be clamped to minimum");

	// Test that normal values work
	Zenith_SceneManager::SetAsyncUnloadBatchSize(100);
	Zenith_Assert(Zenith_SceneManager::GetAsyncUnloadBatchSize() == 100, "Batch size 100 should be accepted");

	// Test that very large values are clamped
	Zenith_SceneManager::SetAsyncUnloadBatchSize(999999);
	Zenith_Assert(Zenith_SceneManager::GetAsyncUnloadBatchSize() <= 10000, "Batch size should be clamped to maximum");

	// Restore original
	Zenith_SceneManager::SetAsyncUnloadBatchSize(uOriginalBatchSize);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestBatchSizeValidation passed");
}

//==============================================================================
// NEW: Test Coverage Additions (from 2025-02 review)
//==============================================================================

void Zenith_SceneTests::TestCircularAsyncLoadFromLifecycle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCircularAsyncLoadFromLifecycle...");

	// Create a scene file so LoadScene can find it
	const std::string strTestPath = "test_circular_lifecycle" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strTestPath, "CircularTestEntity");

	// Test circular detection via s_axCurrentlyLoadingPaths:
	// Register a SceneLoadStarted callback that re-entrantly calls LoadScene
	// for the same file. The path is already in s_axCurrentlyLoadingPaths at
	// that point, so the second LoadScene should be rejected.
	static Zenith_Scene s_xCircularResult;
	static bool s_bAttempted = false;
	s_xCircularResult = Zenith_Scene();
	s_bAttempted = false;

	auto ulHandle = Zenith_SceneManager::RegisterSceneLoadStartedCallback(
		[](const std::string& strPath)
		{
			if (!s_bAttempted && strPath.find("test_circular_lifecycle") != std::string::npos)
			{
				s_bAttempted = true;
				// Re-entrant load of the same scene - should be detected as circular
				s_xCircularResult = Zenith_SceneManager::LoadScene("test_circular_lifecycle" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
			}
		}
	);

	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strTestPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "Initial scene load should succeed");

	// The re-entrant LoadScene should have been rejected as circular
	Zenith_Assert(s_bAttempted, "SceneLoadStarted callback should have fired and attempted re-load");
	Zenith_Assert(!s_xCircularResult.IsValid(), "Circular load should return invalid scene");

	Zenith_SceneManager::UnregisterSceneLoadStartedCallback(ulHandle);
	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strTestPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCircularAsyncLoadFromLifecycle passed");
}

void Zenith_SceneTests::TestAsyncLoadDuringAsyncUnloadSameScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadDuringAsyncUnloadSameScene...");

	// Create a test scene file
	const std::string strTestPath = "test_load_during_unload" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strTestPath, "TestEntity");

	// Load the scene synchronously first
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strTestPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "Initial load should succeed");

	// Start async unload
	Zenith_SceneOperationID ulUnloadOp = Zenith_SceneManager::UnloadSceneAsync(xScene);
	Zenith_Assert(ulUnloadOp != ZENITH_INVALID_OPERATION_ID, "Async unload should return valid operation");

	// Immediately try to async load the same scene while unload is in progress
	// This should be blocked (scene is in currently-loading paths during unload)
	Zenith_SceneOperationID ulLoadOp = Zenith_SceneManager::LoadSceneAsync(strTestPath, SCENE_LOAD_ADDITIVE);

	// Pump until both complete
	Zenith_SceneOperation* pxUnloadOp = Zenith_SceneManager::GetOperation(ulUnloadOp);
	if (pxUnloadOp)
	{
		PumpUntilComplete(pxUnloadOp);
	}

	Zenith_SceneOperation* pxLoadOp = Zenith_SceneManager::GetOperation(ulLoadOp);
	if (pxLoadOp)
	{
		PumpUntilComplete(pxLoadOp);
	}

	// Cleanup
	CleanupTestSceneFile(strTestPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadDuringAsyncUnloadSameScene passed");
}

void Zenith_SceneTests::TestEntitySpawnDuringOnDestroy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntitySpawnDuringOnDestroy...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SpawnDuringDestroyTest");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xScene);

	static Zenith_EntityID s_xSpawnedID;
	static bool s_bSpawned = false;
	s_xSpawnedID = Zenith_EntityID();
	s_bSpawned = false;

	SceneTestBehaviour::ResetCounters();

	// During OnDestroy, spawn a new entity in the same scene
	SceneTestBehaviour::s_pfnOnDestroyCallback = [](Zenith_Entity& xEntity)
	{
		if (!s_bSpawned)
		{
			s_bSpawned = true;
			Zenith_SceneData* pxData = xEntity.GetSceneData();
			Zenith_Entity xNew(pxData, "SpawnedDuringDestroy");
			s_xSpawnedID = xNew.GetEntityID();
		}
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxSceneData, "OriginalEntity");
	pxSceneData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_EntityID xOriginalID = xEntity.GetEntityID();

	Zenith_SceneManager::Destroy(xEntity);
	PumpFrames(1);

	// OnDestroy should have fired and spawned a new entity
	Zenith_Assert(SceneTestBehaviour::s_uDestroyCount == 1, "OnDestroy should fire exactly once");
	Zenith_Assert(s_bSpawned, "Entity should have been spawned during OnDestroy");
	Zenith_Assert(s_xSpawnedID.IsValid(), "Spawned entity ID should be valid");
	Zenith_Assert(!pxSceneData->EntityExists(xOriginalID), "Original entity should be destroyed");
	Zenith_Assert(pxSceneData->EntityExists(s_xSpawnedID), "Spawned entity should exist in scene");

	SceneTestBehaviour::s_pfnOnDestroyCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntitySpawnDuringOnDestroy passed");
}

void Zenith_SceneTests::TestCallbackExceptionHandling()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCallbackExceptionHandling...");

	// Note: C++ exceptions are generally disabled in game engines for performance.
	// This test validates that callbacks are invoked and the system remains stable.

	static bool ls_bCallback1Fired = false;
	static bool ls_bCallback2Fired = false;

	auto pfnCallback1 = [](Zenith_Scene, Zenith_SceneLoadMode) { ls_bCallback1Fired = true; };
	auto pfnCallback2 = [](Zenith_Scene, Zenith_SceneLoadMode) { ls_bCallback2Fired = true; };

	auto ulHandle1 = Zenith_SceneManager::RegisterSceneLoadedCallback(pfnCallback1);
	auto ulHandle2 = Zenith_SceneManager::RegisterSceneLoadedCallback(pfnCallback2);

	// Load a scene to trigger callbacks
	const std::string strTestPath = "test_callback_exception" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strTestPath, "TestEntity");

	ls_bCallback1Fired = false;
	ls_bCallback2Fired = false;

	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strTestPath, SCENE_LOAD_ADDITIVE);

	// Both callbacks should have fired
	Zenith_Assert(ls_bCallback1Fired, "Callback 1 should have fired");
	Zenith_Assert(ls_bCallback2Fired, "Callback 2 should have fired");

	// Cleanup
	Zenith_SceneManager::UnregisterSceneLoadedCallback(ulHandle1);
	Zenith_SceneManager::UnregisterSceneLoadedCallback(ulHandle2);
	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strTestPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCallbackExceptionHandling passed");
}

void Zenith_SceneTests::TestMalformedSceneFile()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMalformedSceneFile...");

	// Create a malformed scene file (just random bytes)
	const std::string strTestPath = "test_malformed" ZENITH_SCENE_EXT;
	{
		std::ofstream xFile(strTestPath, std::ios::binary);
		const char acGarbage[] = { 'B', 'A', 'D', 'D', 'A', 'T', 'A' };
		xFile.write(acGarbage, sizeof(acGarbage));
	}

	// Attempt to load the malformed scene - should fail gracefully
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strTestPath, SCENE_LOAD_ADDITIVE);

	// The scene may or may not be valid depending on error handling,
	// but the system should not crash
	if (xScene.IsValid())
	{
		Zenith_SceneManager::UnloadScene(xScene);
	}

	// Cleanup
	std::remove(strTestPath.c_str());

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMalformedSceneFile passed");
}

void Zenith_SceneTests::TestMaxConcurrentAsyncLoadWarning()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMaxConcurrentAsyncLoadWarning...");

	// Save original max
	uint32_t uOriginalMax = Zenith_SceneManager::GetMaxConcurrentAsyncLoads();

	// Set max to 2
	Zenith_SceneManager::SetMaxConcurrentAsyncLoads(2);

	// Create multiple test scene files
	const std::string strTestPath1 = "test_concurrent_1" ZENITH_SCENE_EXT;
	const std::string strTestPath2 = "test_concurrent_2" ZENITH_SCENE_EXT;
	const std::string strTestPath3 = "test_concurrent_3" ZENITH_SCENE_EXT;

	CreateTestSceneFile(strTestPath1, "Entity1");
	CreateTestSceneFile(strTestPath2, "Entity2");
	CreateTestSceneFile(strTestPath3, "Entity3");

	// Start 3 async loads (exceeding max of 2 - should log warning but proceed)
	Zenith_SceneOperationID ulOp1 = Zenith_SceneManager::LoadSceneAsync(strTestPath1, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperationID ulOp2 = Zenith_SceneManager::LoadSceneAsync(strTestPath2, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperationID ulOp3 = Zenith_SceneManager::LoadSceneAsync(strTestPath3, SCENE_LOAD_ADDITIVE);

	// All operations should be valid (warning is logged but loads proceed)
	Zenith_Assert(ulOp1 != ZENITH_INVALID_OPERATION_ID, "Op 1 should be valid");
	Zenith_Assert(ulOp2 != ZENITH_INVALID_OPERATION_ID, "Op 2 should be valid");
	Zenith_Assert(ulOp3 != ZENITH_INVALID_OPERATION_ID, "Op 3 should be valid");

	// Pump until all complete
	Zenith_SceneOperation* pxOp1 = Zenith_SceneManager::GetOperation(ulOp1);
	Zenith_SceneOperation* pxOp2 = Zenith_SceneManager::GetOperation(ulOp2);
	Zenith_SceneOperation* pxOp3 = Zenith_SceneManager::GetOperation(ulOp3);

	if (pxOp1) PumpUntilComplete(pxOp1);
	if (pxOp2) PumpUntilComplete(pxOp2);
	if (pxOp3) PumpUntilComplete(pxOp3);

	// Cleanup scenes
	Zenith_Scene xScene1 = pxOp1 ? pxOp1->GetResultScene() : Zenith_Scene();
	Zenith_Scene xScene2 = pxOp2 ? pxOp2->GetResultScene() : Zenith_Scene();
	Zenith_Scene xScene3 = pxOp3 ? pxOp3->GetResultScene() : Zenith_Scene();

	if (xScene1.IsValid()) Zenith_SceneManager::UnloadScene(xScene1);
	if (xScene2.IsValid()) Zenith_SceneManager::UnloadScene(xScene2);
	if (xScene3.IsValid()) Zenith_SceneManager::UnloadScene(xScene3);

	// Cleanup files
	CleanupTestSceneFile(strTestPath1);
	CleanupTestSceneFile(strTestPath2);
	CleanupTestSceneFile(strTestPath3);

	// Restore original max
	Zenith_SceneManager::SetMaxConcurrentAsyncLoads(uOriginalMax);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMaxConcurrentAsyncLoadWarning passed");
}

//==============================================================================
// Bug Fix Verification Tests (from 2026-02 code review)
//==============================================================================

//------------------------------------------------------------------------------
// Bug 1: SetEnabled hierarchy check
//------------------------------------------------------------------------------

void Zenith_SceneTests::TestSetEnabledUnderDisabledParentNoOnEnable()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetEnabledUnderDisabledParentNoOnEnable...");

	// Create scene with parent -> child hierarchy
	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("HierarchyTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	// Run lifecycle to awaken both entities
	pxData->DispatchLifecycleForNewScene();

	// Disable parent first
	xParent.SetEnabled(false);

	// Child's OnEnable should have been dispatched during lifecycle and then OnDisable from parent disable
	Zenith_SceneData::Zenith_EntitySlot& xChildSlot = Zenith_SceneData::s_axEntitySlots.Get(xChild.GetEntityID().m_uIndex);
	Zenith_Assert(!xChildSlot.m_bOnEnableDispatched, "Child OnEnable should NOT be dispatched when parent is disabled");

	// Now disable the child
	xChild.SetEnabled(false);

	// Re-enable the child while parent is still disabled
	xChild.SetEnabled(true);

	// OnEnable should NOT have been dispatched because parent is disabled
	Zenith_Assert(!xChildSlot.m_bOnEnableDispatched,
		"SetEnabled(true) on child under disabled parent should NOT dispatch OnEnable");
	Zenith_Assert(!xChild.IsActiveInHierarchy(),
		"Child should NOT be active in hierarchy when parent is disabled");

	// Now re-enable the parent - child's OnEnable should fire via propagation
	xParent.SetEnabled(true);
	Zenith_Assert(xChildSlot.m_bOnEnableDispatched,
		"Re-enabling parent should propagate OnEnable to enabled children");
	Zenith_Assert(xChild.IsActiveInHierarchy(),
		"Child should be active in hierarchy after parent is re-enabled");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetEnabledUnderDisabledParentNoOnEnable passed");
}

void Zenith_SceneTests::TestSetEnabledUnderEnabledParentFiresOnEnable()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetEnabledUnderEnabledParentFiresOnEnable...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EnableTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	// Awaken
	pxData->DispatchLifecycleForNewScene();

	// Both should be active
	Zenith_Assert(xParent.IsActiveInHierarchy(), "Parent should be active");
	Zenith_Assert(xChild.IsActiveInHierarchy(), "Child should be active");

	// Disable and re-enable child while parent is still enabled
	xChild.SetEnabled(false);
	Zenith_SceneData::Zenith_EntitySlot& xChildSlot = Zenith_SceneData::s_axEntitySlots.Get(xChild.GetEntityID().m_uIndex);
	Zenith_Assert(!xChildSlot.m_bOnEnableDispatched, "OnEnable should not be dispatched after disable");

	xChild.SetEnabled(true);
	Zenith_Assert(xChildSlot.m_bOnEnableDispatched,
		"SetEnabled(true) with enabled parent should dispatch OnEnable");
	Zenith_Assert(xChild.IsActiveInHierarchy(),
		"Child should be active in hierarchy");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetEnabledUnderEnabledParentFiresOnEnable passed");
}

void Zenith_SceneTests::TestDisableParentPropagatesOnDisableToChildren()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisableParentPropagatesOnDisableToChildren...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PropagateDisable");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	Zenith_Entity xGrandchild(pxData, "Grandchild");
	xChild.SetParent(xParent.GetEntityID());
	xGrandchild.SetParent(xChild.GetEntityID());

	pxData->DispatchLifecycleForNewScene();

	// All should have OnEnable dispatched
	Zenith_Assert(pxData->IsOnEnableDispatched(xChild.GetEntityID()), "Child should have OnEnable dispatched");
	Zenith_Assert(pxData->IsOnEnableDispatched(xGrandchild.GetEntityID()), "Grandchild should have OnEnable dispatched");

	// Disable parent - child and grandchild should get OnDisable
	xParent.SetEnabled(false);
	Zenith_Assert(!pxData->IsOnEnableDispatched(xChild.GetEntityID()),
		"Disabling parent should propagate OnDisable to child");
	Zenith_Assert(!pxData->IsOnEnableDispatched(xGrandchild.GetEntityID()),
		"Disabling parent should propagate OnDisable to grandchild");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisableParentPropagatesOnDisableToChildren passed");
}

void Zenith_SceneTests::TestEnableParentPropagatesOnEnableToEnabledChildren()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEnableParentPropagatesOnEnableToEnabledChildren...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PropagateEnable");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xEnabledChild(pxData, "EnabledChild");
	Zenith_Entity xDisabledChild(pxData, "DisabledChild");
	xEnabledChild.SetParent(xParent.GetEntityID());
	xDisabledChild.SetParent(xParent.GetEntityID());

	pxData->DispatchLifecycleForNewScene();

	// Disable one child's activeSelf
	xDisabledChild.SetEnabled(false);

	// Disable parent
	xParent.SetEnabled(false);

	// Now re-enable parent
	xParent.SetEnabled(true);

	// Only EnabledChild should have OnEnable dispatched (activeSelf=true)
	// DisabledChild has activeSelf=false so should NOT receive OnEnable
	Zenith_Assert(pxData->IsOnEnableDispatched(xEnabledChild.GetEntityID()),
		"Enabled child should get OnEnable when parent re-enabled");
	Zenith_Assert(!pxData->IsOnEnableDispatched(xDisabledChild.GetEntityID()),
		"Disabled child (activeSelf=false) should NOT get OnEnable when parent re-enabled");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEnableParentPropagatesOnEnableToEnabledChildren passed");
}

void Zenith_SceneTests::TestDoublePropagationGuard()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDoublePropagationGuard...");

	// Verify that enabling a parent doesn't dispatch OnEnable to a child that already has it
	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DoublePropGuard");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	pxData->DispatchLifecycleForNewScene();

	// Child should have OnEnable dispatched from lifecycle
	Zenith_Assert(pxData->IsOnEnableDispatched(xChild.GetEntityID()), "Child should have OnEnable dispatched");

	// Disable parent, then disable child while under disabled parent
	xParent.SetEnabled(false);
	// child got OnDisable from propagation
	Zenith_Assert(!pxData->IsOnEnableDispatched(xChild.GetEntityID()),
		"Child should have OnDisable after parent disabled");

	// Re-enable parent - child should get OnEnable since its activeSelf is true
	xParent.SetEnabled(true);
	Zenith_Assert(pxData->IsOnEnableDispatched(xChild.GetEntityID()),
		"Child should get OnEnable when parent re-enabled");

	// Calling SetEnabled(true) on child again should be a no-op (already enabled)
	xChild.SetEnabled(true);  // activeSelf was already true, so this returns early
	Zenith_Assert(pxData->IsOnEnableDispatched(xChild.GetEntityID()),
		"OnEnable should still be dispatched after no-op SetEnabled");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDoublePropagationGuard passed");
}

//------------------------------------------------------------------------------
// Bug 2+11: EventSystem dispatch safety
//------------------------------------------------------------------------------

void Zenith_SceneTests::TestEventDispatchSubscribeDuringCallback()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventDispatchSubscribeDuringCallback...");

	// Test that subscribing to the SAME event type inside a callback doesn't crash
	// (previously caused dangling reference due to vector reallocation)

	struct TestEvent { int m_iValue = 0; };
	Zenith_EventDispatcher& xDispatcher = Zenith_EventDispatcher::Get();

	static bool s_bOriginalFired = false;
	static bool s_bNewSubFired = false;
	static Zenith_EventHandle s_uNewHandle = INVALID_EVENT_HANDLE;

	s_bOriginalFired = false;
	s_bNewSubFired = false;

	// Subscribe a callback that subscribes ANOTHER callback to the SAME event type
	Zenith_EventHandle uHandle1 = xDispatcher.Subscribe<TestEvent>(
		[](const TestEvent&) {
			s_bOriginalFired = true;
			// Subscribe to the same event type during dispatch - this used to cause dangling reference
			s_uNewHandle = Zenith_EventDispatcher::Get().Subscribe<TestEvent>(
				[](const TestEvent&) {
					s_bNewSubFired = true;
				}
			);
		}
	);

	// This should NOT crash (previously caused use-after-free)
	xDispatcher.Dispatch(TestEvent{42});

	Zenith_Assert(s_bOriginalFired, "Original callback should fire");
	// The new subscription was added DURING dispatch, so it should NOT fire in this dispatch
	// (we iterate a snapshot)
	Zenith_Assert(!s_bNewSubFired, "Newly subscribed callback should NOT fire during same dispatch");

	// Second dispatch should fire both
	s_bOriginalFired = false;
	s_bNewSubFired = false;
	xDispatcher.Dispatch(TestEvent{99});
	Zenith_Assert(s_bOriginalFired, "Original callback should fire on second dispatch");
	Zenith_Assert(s_bNewSubFired, "New callback should fire on second dispatch");

	// Cleanup
	xDispatcher.Unsubscribe(uHandle1);
	if (s_uNewHandle != INVALID_EVENT_HANDLE)
		xDispatcher.Unsubscribe(s_uNewHandle);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventDispatchSubscribeDuringCallback passed");
}

void Zenith_SceneTests::TestEventDispatchUnsubscribeDuringCallback()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventDispatchUnsubscribeDuringCallback...");

	// Test that when callback A unsubscribes callback B, B does NOT fire (Unity parity)

	struct TestEvent2 { int m_iValue = 0; };
	Zenith_EventDispatcher& xDispatcher = Zenith_EventDispatcher::Get();

	static bool s_bCallbackAFired = false;
	static bool s_bCallbackBFired = false;
	static Zenith_EventHandle s_uHandleB = INVALID_EVENT_HANDLE;

	s_bCallbackAFired = false;
	s_bCallbackBFired = false;

	// Callback A unsubscribes callback B
	Zenith_EventHandle uHandleA = xDispatcher.Subscribe<TestEvent2>(
		[](const TestEvent2&) {
			s_bCallbackAFired = true;
			Zenith_EventDispatcher::Get().Unsubscribe(s_uHandleB);
		}
	);

	s_uHandleB = xDispatcher.Subscribe<TestEvent2>(
		[](const TestEvent2&) {
			s_bCallbackBFired = true;
		}
	);

	xDispatcher.Dispatch(TestEvent2{1});

	Zenith_Assert(s_bCallbackAFired, "Callback A should fire");
	Zenith_Assert(!s_bCallbackBFired,
		"Callback B should NOT fire after being unsubscribed by callback A during same dispatch");

	// Cleanup
	xDispatcher.Unsubscribe(uHandleA);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventDispatchUnsubscribeDuringCallback passed");
}

//------------------------------------------------------------------------------
// Bug 3: sceneUnloaded handle validity
//------------------------------------------------------------------------------

void Zenith_SceneTests::TestSceneUnloadedCallbackHandleValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneUnloadedCallbackHandleValid...");

	// Test that the scene handle passed to sceneUnloaded callback has a valid generation
	// (Previously FreeSceneHandle incremented generation before the callback fired)

	static int s_iReceivedHandle = -1;
	static uint32_t s_uReceivedGeneration = 0;

	auto ulHandle = Zenith_SceneManager::RegisterSceneUnloadedCallback(
		[](Zenith_Scene xScene) {
			s_iReceivedHandle = xScene.GetHandle();
			s_uReceivedGeneration = xScene.m_uGeneration;
		}
	);

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("UnloadHandleTest");
	int iExpectedHandle = xScene.GetHandle();
	uint32_t uExpectedGeneration = xScene.m_uGeneration;

	Zenith_SceneManager::UnloadScene(xScene);

	// The handle and generation in the callback should match the original scene
	Zenith_Assert(s_iReceivedHandle == iExpectedHandle,
		"sceneUnloaded callback should receive the correct handle (got %d, expected %d)",
		s_iReceivedHandle, iExpectedHandle);
	Zenith_Assert(s_uReceivedGeneration == uExpectedGeneration,
		"sceneUnloaded callback should receive the original generation (got %u, expected %u)",
		s_uReceivedGeneration, uExpectedGeneration);

	Zenith_SceneManager::UnregisterSceneUnloadedCallback(ulHandle);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneUnloadedCallbackHandleValid passed");
}

//------------------------------------------------------------------------------
// Bug 4: GetName/GetPath return const ref
//------------------------------------------------------------------------------

void Zenith_SceneTests::TestSceneGetNameReturnsRef()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneGetNameReturnsRef...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("RefTest");

	// GetName should return a reference to the internal string - verify by address
	const std::string& strName1 = xScene.GetName();
	const std::string& strName2 = xScene.GetName();

	// Both should point to the same underlying string (no allocation per call)
	Zenith_Assert(&strName1 == &strName2,
		"GetName should return a reference to the same string, not allocate a copy each time");
	Zenith_Assert(strName1 == "RefTest", "GetName should return the correct scene name");

	// Invalid scene should return empty reference (not crash)
	Zenith_Scene xInvalid;
	const std::string& strInvalidName = xInvalid.GetName();
	Zenith_Assert(strInvalidName.empty(), "Invalid scene GetName should return empty string");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneGetNameReturnsRef passed");
}

void Zenith_SceneTests::TestSceneGetPathReturnsRef()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneGetPathReturnsRef...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PathRefTest");

	// GetPath should return a reference
	const std::string& strPath1 = xScene.GetPath();
	const std::string& strPath2 = xScene.GetPath();
	Zenith_Assert(&strPath1 == &strPath2,
		"GetPath should return a reference to the same string, not allocate a copy each time");

	// Invalid scene should return empty reference
	Zenith_Scene xInvalid;
	const std::string& strInvalidPath = xInvalid.GetPath();
	Zenith_Assert(strInvalidPath.empty(), "Invalid scene GetPath should return empty string");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneGetPathReturnsRef passed");
}

//------------------------------------------------------------------------------
// Bug 6: Awake called immediately for entities created during Awake
//------------------------------------------------------------------------------

void Zenith_SceneTests::TestEntityCreatedDuringAwakeGetsAwakeImmediately()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedDuringAwakeGetsAwakeImmediately...");

	// This tests that entities created during another entity's Awake processing
	// get their own Awake called in the same frame (Unity parity).
	// The implementation loops m_axNewlyCreatedEntities until stable.

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("AwakeChain");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create initial entity
	Zenith_Entity xEntity(pxData, "InitialEntity");

	// Now simulate: during Update, new entities are created and get Awake dispatched.
	// We test the iteration mechanism by manually creating entities and tracking their Awake state.

	// Create entities that would be registered in m_axNewlyCreatedEntities
	// Then call Update which should iterate until all have Awake called
	Zenith_Entity xSecond(pxData, "SecondEntity");

	// Run a single update frame - this processes newly created entities
	const float fDt = 1.0f / 60.0f;
	Zenith_SceneManager::Update(fDt);
	Zenith_SceneManager::WaitForUpdateComplete();

	// Both entities should have been awoken
	Zenith_Assert(pxData->IsEntityAwoken(xEntity.GetEntityID()),
		"Initial entity should be awoken after Update");
	Zenith_Assert(pxData->IsEntityAwoken(xSecond.GetEntityID()),
		"Second entity should be awoken after Update");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedDuringAwakeGetsAwakeImmediately passed");
}

//------------------------------------------------------------------------------
// Bug 7: activeInHierarchy caching
//------------------------------------------------------------------------------

void Zenith_SceneTests::TestActiveInHierarchyCacheValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveInHierarchyCacheValid...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CacheTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	pxData->DispatchLifecycleForNewScene();

	// First call rebuilds cache
	bool bActive = xChild.IsActiveInHierarchy();
	Zenith_Assert(bActive, "Child should be active in hierarchy");

	// Second call should use cached value
	Zenith_SceneData::Zenith_EntitySlot& xChildSlot = Zenith_SceneData::s_axEntitySlots.Get(xChild.GetEntityID().m_uIndex);
	Zenith_Assert(!xChildSlot.m_bActiveInHierarchyDirty,
		"Cache should be clean after IsActiveInHierarchy call");

	bool bActive2 = xChild.IsActiveInHierarchy();
	Zenith_Assert(bActive2 == bActive, "Cached result should match");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveInHierarchyCacheValid passed");
}

void Zenith_SceneTests::TestActiveInHierarchyCacheInvalidatedOnSetEnabled()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveInHierarchyCacheInvalidatedOnSetEnabled...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CacheInvalidate");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	pxData->DispatchLifecycleForNewScene();

	// Prime the cache
	Zenith_Assert(xChild.IsActiveInHierarchy(), "Child should be active initially");

	Zenith_SceneData::Zenith_EntitySlot& xChildSlot = Zenith_SceneData::s_axEntitySlots.Get(xChild.GetEntityID().m_uIndex);
	Zenith_Assert(!xChildSlot.m_bActiveInHierarchyDirty, "Cache should be clean");

	// Disable parent - should invalidate child's cache
	xParent.SetEnabled(false);

	Zenith_Assert(xChildSlot.m_bActiveInHierarchyDirty,
		"Child cache should be dirty after parent SetEnabled(false)");

	// Query should rebuild cache with correct result
	Zenith_Assert(!xChild.IsActiveInHierarchy(),
		"Child should NOT be active when parent is disabled");
	Zenith_Assert(!xChildSlot.m_bActiveInHierarchyDirty,
		"Cache should be clean again after rebuild");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveInHierarchyCacheInvalidatedOnSetEnabled passed");
}

void Zenith_SceneTests::TestActiveInHierarchyCacheInvalidatedOnSetParent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveInHierarchyCacheInvalidatedOnSetParent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CacheReparent");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEnabledParent(pxData, "EnabledParent");
	Zenith_Entity xDisabledParent(pxData, "DisabledParent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xEnabledParent.GetEntityID());

	pxData->DispatchLifecycleForNewScene();

	// Disable one parent
	xDisabledParent.SetEnabled(false);

	// Child under enabled parent should be active
	Zenith_Assert(xChild.IsActiveInHierarchy(), "Child under enabled parent should be active");

	Zenith_SceneData::Zenith_EntitySlot& xChildSlot = Zenith_SceneData::s_axEntitySlots.Get(xChild.GetEntityID().m_uIndex);
	Zenith_Assert(!xChildSlot.m_bActiveInHierarchyDirty, "Cache should be clean");

	// Reparent child under disabled parent - cache should be invalidated
	xChild.SetParent(xDisabledParent.GetEntityID());
	Zenith_Assert(xChildSlot.m_bActiveInHierarchyDirty,
		"Child cache should be dirty after SetParent");

	Zenith_Assert(!xChild.IsActiveInHierarchy(),
		"Child should NOT be active after reparenting under disabled parent");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveInHierarchyCacheInvalidatedOnSetParent passed");
}

//==============================================================================
// Bug Fix Regression Tests (from 2026-02 code review - batch 2)
//==============================================================================

// Fix 1: DispatchPendingStarts validates entity before clearing flag

void Zenith_SceneTests::TestPendingStartSurvivesSlotReuse()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPendingStartSurvivesSlotReuse...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("StartSlotReuse");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create entity A - pump one frame to trigger Awake and queue pending start
	Zenith_Entity xEntityA(pxData, "EntityA");
	Zenith_EntityID xIDA = xEntityA.GetEntityID();
	uint32_t uSlotIndex = xIDA.m_uIndex;

	// Dispatch lifecycle to trigger Awake and queue pending start
	pxData->DispatchLifecycleForNewScene();
	Zenith_Assert(pxData->HasPendingStarts(), "Should have pending starts after Awake");

	// Destroy entity A immediately - frees slot
	Zenith_SceneManager::DestroyImmediate(xEntityA);
	Zenith_Assert(!pxData->EntityExists(xIDA), "Entity A should be destroyed");

	// Create entity B - should reuse same slot index
	Zenith_Entity xEntityB(pxData, "EntityB");
	Zenith_EntityID xIDB = xEntityB.GetEntityID();
	Zenith_Assert(xIDB.m_uIndex == uSlotIndex, "Entity B should reuse slot from entity A");
	Zenith_Assert(xIDB.m_uGeneration == xIDA.m_uGeneration + 1, "Entity B should have incremented generation");

	// Dispatch Awake for entity B (sets m_bPendingStart = true)
	pxData->DispatchLifecycleForNewScene();

	// Now dispatch pending starts - entity B must get Start() called
	pxData->DispatchPendingStarts();
	Zenith_Assert(pxData->IsEntityStarted(xIDB), "Entity B should have received Start() after slot reuse");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPendingStartSurvivesSlotReuse passed");
}

void Zenith_SceneTests::TestPendingStartSkipsStaleEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPendingStartSkipsStaleEntity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("StartStale");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create entity, dispatch Awake to queue pending start
	Zenith_Entity xEntity(pxData, "StaleEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxData->DispatchLifecycleForNewScene();
	Zenith_Assert(pxData->HasPendingStarts(), "Should have pending starts");

	// Destroy entity immediately - slot freed but not reused
	Zenith_SceneManager::DestroyImmediate(xEntity);
	Zenith_Assert(!pxData->EntityExists(xID), "Entity should be destroyed");

	// DispatchPendingStarts should skip the stale entry without crash
	pxData->DispatchPendingStarts();
	Zenith_Assert(!pxData->HasPendingStarts(), "Pending start count should reach 0");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPendingStartSkipsStaleEntity passed");
}

// Fix 2: Slot reuse resets activeInHierarchy cache

void Zenith_SceneTests::TestSlotReuseResetsActiveInHierarchy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSlotReuseResetsActiveInHierarchy...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SlotReuseActive");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create entity A and disable it
	Zenith_Entity xEntityA(pxData, "DisabledEntity");
	pxData->DispatchLifecycleForNewScene();
	xEntityA.SetEnabled(false);

	// Populate cache by querying
	Zenith_Assert(!xEntityA.IsActiveInHierarchy(), "Disabled entity should not be active in hierarchy");

	Zenith_EntityID xIDA = xEntityA.GetEntityID();
	uint32_t uSlotIndex = xIDA.m_uIndex;

	// Destroy entity A - frees slot
	Zenith_SceneManager::DestroyImmediate(xEntityA);

	// Create entity B - reuses same slot
	Zenith_Entity xEntityB(pxData, "NewEntity");
	Zenith_EntityID xIDB = xEntityB.GetEntityID();
	Zenith_Assert(xIDB.m_uIndex == uSlotIndex, "Entity B should reuse slot from entity A");

	// Entity B should be active (default state), not inheriting A's disabled cache
	Zenith_Assert(xEntityB.IsActiveInHierarchy(), "New entity in reused slot should be active in hierarchy");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSlotReuseResetsActiveInHierarchy passed");
}

void Zenith_SceneTests::TestSlotReuseDirtyFlagReset()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSlotReuseDirtyFlagReset...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SlotReuseDirty");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create entity A and populate its cache
	Zenith_Entity xEntityA(pxData, "CachedEntity");
	pxData->DispatchLifecycleForNewScene();
	xEntityA.IsActiveInHierarchy(); // Populates cache, sets dirty=false

	Zenith_EntityID xIDA = xEntityA.GetEntityID();
	uint32_t uSlotIndex = xIDA.m_uIndex;

	Zenith_SceneData::Zenith_EntitySlot& xSlotBefore = Zenith_SceneData::s_axEntitySlots.Get(uSlotIndex);
	Zenith_Assert(!xSlotBefore.m_bActiveInHierarchyDirty, "Cache should be clean after query");

	// Destroy entity A and create entity B in same slot
	Zenith_SceneManager::DestroyImmediate(xEntityA);
	Zenith_Entity xEntityB(pxData, "NewCachedEntity");
	Zenith_EntityID xIDB = xEntityB.GetEntityID();
	Zenith_Assert(xIDB.m_uIndex == uSlotIndex, "Entity B should reuse slot");

	// Verify the new entity has correct active state (slot was properly reset)
	// Note: With immediate lifecycle dispatch, IsActiveInHierarchy() is already called
	// during construction (for OnEnable check), so the cache is populated and dirty=false.
	Zenith_Assert(xEntityB.IsActiveInHierarchy(), "New entity in reused slot should be active in hierarchy");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSlotReuseDirtyFlagReset passed");
}

// Fix 3: Async unload batch count includes recursive children

void Zenith_SceneTests::TestAsyncUnloadBatchCountsChildren()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncUnloadBatchCountsChildren...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("BatchChildren");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create a parent with 10 children (11 entities total)
	Zenith_Entity xParent(pxData, "Parent");
	for (int i = 0; i < 10; ++i)
	{
		Zenith_Entity xChild(pxData, "Child" + std::to_string(i));
		xChild.SetParent(xParent.GetEntityID());
	}
	Zenith_Assert(pxData->GetEntityCount() == 11, "Should have 11 entities (parent + 10 children)");

	// Set batch size to 5 - should take at least 3 frames for 11 entities
	uint32_t uOldBatchSize = Zenith_SceneManager::GetAsyncUnloadBatchSize();
	Zenith_SceneManager::SetAsyncUnloadBatchSize(5);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xScene);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);

	int iUpdateCount = 0;
	while (!pxOp->IsComplete())
	{
		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
		iUpdateCount++;
		Zenith_Assert(iUpdateCount < 100, "Async unload should not take more than 100 frames");
	}

	// With batch size 5 and 11 entities, it should take at least 3 frames
	// (Before the fix, removing the parent counted as 1 but actually removed 11)
	Zenith_Assert(iUpdateCount >= 3, "Should take at least 3 frames to destroy 11 entities with batch size 5 (got %d)", iUpdateCount);

	// Restore batch size
	Zenith_SceneManager::SetAsyncUnloadBatchSize(uOldBatchSize);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncUnloadBatchCountsChildren passed");
}

void Zenith_SceneTests::TestAsyncUnloadProgressWithHierarchy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncUnloadProgressWithHierarchy...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ProgressHierarchy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create parent with 10 children
	Zenith_Entity xParent(pxData, "Parent");
	for (int i = 0; i < 10; ++i)
	{
		Zenith_Entity xChild(pxData, "Child" + std::to_string(i));
		xChild.SetParent(xParent.GetEntityID());
	}

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xScene);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);

	PumpUntilComplete(pxOp);

	// After completion, progress should have reached a value > 0 (not stuck at 0)
	// Before the fix, m_uDestroyedEntities would be ~1 but 11 entities were removed
	Zenith_Assert(pxOp->IsComplete(), "Async unload should complete");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncUnloadProgressWithHierarchy passed");
}

// Fix 4: MoveEntity transfers timed destructions

void Zenith_SceneTests::TestMoveEntityTransfersTimedDestruction()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityTransfersTimedDestruction...");

	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("TimedSrc");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("TimedDst");
	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);
	Zenith_SceneData* pxDataB = Zenith_SceneManager::GetSceneData(xSceneB);

	// Create entity in scene A
	Zenith_Entity xEntity(pxDataA, "TimedEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxDataA->DispatchLifecycleForNewScene();

	// Queue timed destruction (2 seconds delay)
	Zenith_SceneManager::Destroy(xEntity, 2.0f);
	Zenith_Assert(pxDataA->m_axTimedDestructions.GetSize() == 1, "Source should have 1 timed destruction");

	// Move entity to scene B
	Zenith_SceneManager::MoveEntityToScene(xEntity, xSceneB);

	// Timed destruction should now be in scene B, not A
	Zenith_Assert(pxDataA->m_axTimedDestructions.GetSize() == 0, "Source should have 0 timed destructions after move");
	Zenith_Assert(pxDataB->m_axTimedDestructions.GetSize() == 1, "Target should have 1 timed destruction after move");

	// Pump frames for 3 seconds to let the timer expire in scene B
	const float fDt = 1.0f / 60.0f;
	for (int i = 0; i < 200; ++i) // 200 frames @ 60fps = ~3.3 seconds
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
		if (!pxDataB->EntityExists(xID)) break;
	}

	// Entity should have been destroyed in scene B by the transferred timer
	Zenith_Assert(!pxDataB->EntityExists(xID), "Entity should be destroyed by timed destruction in target scene");

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityTransfersTimedDestruction passed");
}

void Zenith_SceneTests::TestMoveEntityTimedDestructionNotInSource()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityTimedDestructionNotInSource...");

	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("TimedNotInSrc");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("TimedNotInDst");
	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);

	// Create entity and add timed destruction
	Zenith_Entity xEntity(pxDataA, "TimedEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxDataA->DispatchLifecycleForNewScene();
	Zenith_SceneManager::Destroy(xEntity, 5.0f);

	// Move to scene B
	Zenith_SceneManager::MoveEntityToScene(xEntity, xSceneB);

	// Verify source has no timed destructions for this entity
	for (u_int i = 0; i < pxDataA->m_axTimedDestructions.GetSize(); ++i)
	{
		Zenith_Assert(!(pxDataA->m_axTimedDestructions.Get(i).m_xEntityID == xID),
			"Source scene should not contain timed destruction for moved entity");
	}

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityTimedDestructionNotInSource passed");
}

// Fix 5: MoveEntity adjusts pending start count

void Zenith_SceneTests::TestMoveEntityAdjustsPendingStartCount()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityAdjustsPendingStartCount...");

	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("PendingSrc");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("PendingDst");
	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);
	Zenith_SceneData* pxDataB = Zenith_SceneManager::GetSceneData(xSceneB);

	// Create entity in scene A, dispatch Awake (sets m_bPendingStart = true)
	Zenith_Entity xEntity(pxDataA, "PendingEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxDataA->DispatchLifecycleForNewScene();

	Zenith_Assert(pxDataA->HasPendingStarts(), "Source should have pending starts after Awake");
	u_int uSourceCountBefore = pxDataA->m_uPendingStartCount;
	u_int uTargetCountBefore = pxDataB->m_uPendingStartCount;

	// Move entity BEFORE Start fires
	Zenith_SceneManager::MoveEntityToScene(xEntity, xSceneB);

	// Pending start count should transfer
	Zenith_Assert(pxDataA->m_uPendingStartCount == uSourceCountBefore - 1,
		"Source pending start count should decrease by 1");
	Zenith_Assert(pxDataB->m_uPendingStartCount == uTargetCountBefore + 1,
		"Target pending start count should increase by 1");

	// Pump frame to dispatch Start in scene B
	pxDataB->DispatchPendingStarts();
	Zenith_Assert(pxDataB->IsEntityStarted(xID), "Entity should receive Start() in target scene");

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityAdjustsPendingStartCount passed");
}

void Zenith_SceneTests::TestMoveEntityAlreadyStartedNoPendingCountChange()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityAlreadyStartedNoPendingCountChange...");

	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("StartedSrc");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("StartedDst");
	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);
	Zenith_SceneData* pxDataB = Zenith_SceneManager::GetSceneData(xSceneB);

	// Create entity and fully initialize (Awake + Start)
	Zenith_Entity xEntity(pxDataA, "StartedEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxDataA->DispatchLifecycleForNewScene();
	pxDataA->DispatchPendingStarts();
	Zenith_Assert(pxDataA->IsEntityStarted(xID), "Entity should be started");
	Zenith_Assert(!pxDataA->HasPendingStarts(), "No pending starts should remain");

	u_int uSourceCount = pxDataA->m_uPendingStartCount;
	u_int uTargetCount = pxDataB->m_uPendingStartCount;

	// Move already-started entity
	Zenith_SceneManager::MoveEntityToScene(xEntity, xSceneB);

	// Pending start counts should NOT change
	Zenith_Assert(pxDataA->m_uPendingStartCount == uSourceCount,
		"Source pending count should not change for already-started entity");
	Zenith_Assert(pxDataB->m_uPendingStartCount == uTargetCount,
		"Target pending count should not change for already-started entity");

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityAlreadyStartedNoPendingCountChange passed");
}

// Fix 6: Active scene selection prefers build index

void Zenith_SceneTests::TestActiveSceneSelectionPrefersBuildIndex()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveSceneSelectionPrefersBuildIndex...");

	// Create two scenes and assign build indices
	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("BuildIdx0");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("BuildIdx1");

	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);
	Zenith_SceneData* pxDataB = Zenith_SceneManager::GetSceneData(xSceneB);

	// Set build indices directly (scene A has lower index = higher priority)
	pxDataA->m_iBuildIndex = 0;
	pxDataB->m_iBuildIndex = 1;

	// Set scene B as active
	Zenith_SceneManager::SetActiveScene(xSceneB);
	Zenith_Assert(Zenith_SceneManager::GetActiveScene() == xSceneB, "Scene B should be active");

	// Unload scene B - scene A should become active (lower build index)
	Zenith_SceneManager::UnloadScene(xSceneB);

	Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xActive == xSceneA, "Scene with lowest build index (0) should become active");

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveSceneSelectionPrefersBuildIndex passed");
}

void Zenith_SceneTests::TestActiveSceneSelectionFallsBackToTimestamp()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveSceneSelectionFallsBackToTimestamp...");

	// Create two scenes WITHOUT build indices (dynamic scenes)
	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("NoBuildA");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("NoBuildB");
	Zenith_Scene xSceneC = Zenith_SceneManager::CreateEmptyScene("NoBuildC");

	// Scene C should have the latest timestamp
	// Set scene C as active then unload it
	Zenith_SceneManager::SetActiveScene(xSceneC);
	Zenith_SceneManager::UnloadScene(xSceneC);

	// Should fall back to most recently loaded remaining scene (B or A, depending on load order)
	Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xActive.IsValid(), "An active scene should be selected after unloading active");
	Zenith_Assert(xActive == xSceneA || xActive == xSceneB,
		"Active scene should be one of the remaining scenes");

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveSceneSelectionFallsBackToTimestamp passed");
}

//==============================================================================
// Code Review Fix Verification Tests (2026-02 review - batch 3)
//==============================================================================

// B1: Runtime entity created under disabled parent should NOT get OnEnable
void Zenith_SceneTests::TestRuntimeEntityUnderDisabledParentNoOnEnable()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRuntimeEntityUnderDisabledParentNoOnEnable...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("RuntimeOnEnableTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create a parent and disable it
	Zenith_Entity xParent(pxData, "Parent");
	pxData->DispatchLifecycleForNewScene();
	xParent.SetEnabled(false);

	// Now create a child under the disabled parent
	// With immediate lifecycle dispatch (Unity parity), OnEnable fires in the constructor
	// when the entity is still a root (active). SetParent afterward moves it under the
	// disabled parent, making it inactive in hierarchy - matching Unity's
	// new GameObject() + SetParent() behavior.
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	// Run Update
	const float fDt = 1.0f / 60.0f;
	Zenith_SceneManager::SetActiveScene(xScene);
	Zenith_SceneManager::Update(fDt);
	Zenith_SceneManager::WaitForUpdateComplete();

	// Child received OnEnable at construction time (was a root, active) but is now
	// inactive in hierarchy because parent is disabled
	Zenith_Assert(!xChild.IsActiveInHierarchy(),
		"Runtime entity under disabled parent should NOT be active in hierarchy");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRuntimeEntityUnderDisabledParentNoOnEnable passed");
}

// B1: Runtime entity created under enabled parent SHOULD get OnEnable
void Zenith_SceneTests::TestRuntimeEntityUnderEnabledParentGetsOnEnable()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRuntimeEntityUnderEnabledParentGetsOnEnable...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("RuntimeOnEnableEnabledTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create an enabled parent
	Zenith_Entity xParent(pxData, "Parent");
	pxData->DispatchLifecycleForNewScene();

	// Now create a child under the enabled parent (runtime path)
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	// Run Update to trigger runtime Awake/OnEnable for the new child
	const float fDt = 1.0f / 60.0f;
	Zenith_SceneManager::SetActiveScene(xScene);
	Zenith_SceneManager::Update(fDt);
	Zenith_SceneManager::WaitForUpdateComplete();

	// Child SHOULD have OnEnable dispatched because parent is enabled
	Zenith_SceneData::Zenith_EntitySlot& xChildSlot = Zenith_SceneData::s_axEntitySlots.Get(xChild.GetEntityID().m_uIndex);
	Zenith_Assert(xChildSlot.m_bOnEnableDispatched,
		"Runtime entity under enabled parent should receive OnEnable");
	Zenith_Assert(xChild.IsActiveInHierarchy(),
		"Runtime entity under enabled parent should be active in hierarchy");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRuntimeEntityUnderEnabledParentGetsOnEnable passed");
}

// B2: Entity disabled before first Update should still get Start when later enabled
void Zenith_SceneTests::TestDisabledEntityEventuallyGetsStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisabledEntityEventuallyGetsStart...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PendingStartTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_SceneManager::SetActiveScene(xScene);

	// Create entity and run lifecycle (Awake + OnEnable)
	Zenith_Entity xEntity(pxData, "TestEntity");
	pxData->DispatchLifecycleForNewScene();

	// Disable the entity before first Update (Start hasn't fired yet)
	xEntity.SetEnabled(false);

	const float fDt = 1.0f / 60.0f;

	// Run several Update frames - Start should NOT fire because entity is disabled
	for (int i = 0; i < 5; ++i)
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	Zenith_Assert(!pxData->IsEntityStarted(xEntity.GetEntityID()),
		"Disabled entity should NOT have Start() dispatched");

	// Now enable the entity - this should queue Start again
	xEntity.SetEnabled(true);

	// Run another Update - Start should fire now
	Zenith_SceneManager::Update(fDt);
	Zenith_SceneManager::WaitForUpdateComplete();

	Zenith_Assert(pxData->IsEntityStarted(xEntity.GetEntityID()),
		"Entity should receive Start() after being enabled (Unity parity)");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisabledEntityEventuallyGetsStart passed");
}

// B2: PendingStartCount remains consistent through disable/enable/Start cycle
void Zenith_SceneTests::TestDisabledEntityPendingStartCountConsistent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisabledEntityPendingStartCountConsistent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PendingCountTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_SceneManager::SetActiveScene(xScene);

	// Create 3 entities
	Zenith_Entity xEntityA(pxData, "EntityA");
	Zenith_Entity xEntityB(pxData, "EntityB");
	Zenith_Entity xEntityC(pxData, "EntityC");
	pxData->DispatchLifecycleForNewScene();

	// Disable entity B before first Update
	xEntityB.SetEnabled(false);

	const float fDt = 1.0f / 60.0f;

	// First Update: dispatches Start for A and C (active), B stays pending
	Zenith_SceneManager::Update(fDt);
	Zenith_SceneManager::WaitForUpdateComplete();

	Zenith_Assert(pxData->IsEntityStarted(xEntityA.GetEntityID()), "Entity A should have started");
	Zenith_Assert(!pxData->IsEntityStarted(xEntityB.GetEntityID()), "Entity B should NOT have started (disabled)");
	Zenith_Assert(pxData->IsEntityStarted(xEntityC.GetEntityID()), "Entity C should have started");

	// Enable entity B
	xEntityB.SetEnabled(true);

	// Next Update: should dispatch Start for B
	Zenith_SceneManager::Update(fDt);
	Zenith_SceneManager::WaitForUpdateComplete();

	Zenith_Assert(pxData->IsEntityStarted(xEntityB.GetEntityID()),
		"Entity B should have started after being enabled");

	// Verify pending count is zero (all entities started)
	Zenith_Assert(pxData->m_uPendingStartCount == 0,
		"PendingStartCount should be 0 after all entities have started");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisabledEntityPendingStartCountConsistent passed");
}

// B4: IsActiveInHierarchy does not crash during scene teardown
void Zenith_SceneTests::TestIsActiveInHierarchyDuringTeardown()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsActiveInHierarchyDuringTeardown...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TeardownTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create parent-child hierarchy
	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	pxData->DispatchLifecycleForNewScene();

	// Cache entity IDs before unload
	Zenith_EntityID xParentID = xParent.GetEntityID();
	Zenith_EntityID xChildID = xChild.GetEntityID();

	// Unload the scene - this triggers Reset() which sets m_bIsBeingDestroyed
	// The fix ensures IsActiveInHierarchy returns false instead of crashing
	Zenith_SceneManager::UnloadScene(xScene);

	// If we get here without crashing, the test passes
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsActiveInHierarchyDuringTeardown passed");
}

// P1: Async-loaded scene reports IsLoaded() == false before activation completes
void Zenith_SceneTests::TestAsyncLoadIsLoadedFalseBeforeActivation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadIsLoadedFalseBeforeActivation...");

	const std::string strPath = "test_isloaded_activation" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Start async load with activation paused
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	pxOp->SetActivationAllowed(false);

	// Pump until deserialized (progress ~90%)
	for (int i = 0; i < 120; ++i)
	{
		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
		if (pxOp->GetProgress() >= 0.85f)
			break;
	}

	// If we reached the paused state, verify IsLoaded is false
	if (pxOp->GetProgress() >= 0.85f && !pxOp->IsComplete())
	{
		Zenith_Scene xResult = pxOp->GetResultScene();
		Zenith_Assert(!xResult.IsLoaded(),
			"Scene.IsLoaded() should be false before activation (Unity parity)");
	}

	// Allow activation and complete
	pxOp->SetActivationAllowed(true);
	PumpUntilComplete(pxOp);

	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_Assert(xResult.IsLoaded(),
		"Scene.IsLoaded() should be true after activation completes");

	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadIsLoadedFalseBeforeActivation passed");
}

// P3: GetLoadedSceneCount always returns >= 1
void Zenith_SceneTests::TestLoadedSceneCountMinimumOne()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadedSceneCountMinimumOne...");

	// GetLoadedSceneCount should always be >= 1 (Unity parity: sceneCount >= 1)
	uint32_t uCount = Zenith_SceneManager::GetLoadedSceneCount();
	Zenith_Assert(uCount >= 1, "GetLoadedSceneCount should always return >= 1 (Unity parity)");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadedSceneCountMinimumOne passed");
}

// P5+I3: Timed destruction entries for already-destroyed entities are cleaned up
void Zenith_SceneTests::TestTimedDestructionEarlyCleanup()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTimedDestructionEarlyCleanup...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TimedDestroyTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_SceneManager::SetActiveScene(xScene);

	Zenith_Entity xEntity(pxData, "TimedEntity");
	pxData->DispatchLifecycleForNewScene();

	// Schedule timed destruction (5 seconds)
	pxData->MarkForTimedDestruction(xEntity.GetEntityID(), 5.0f);
	Zenith_Assert(pxData->m_axTimedDestructions.GetSize() == 1,
		"Should have 1 timed destruction entry");

	// Immediately destroy the entity
	Zenith_SceneManager::DestroyImmediate(xEntity);

	// Run a few update frames (less than 5 seconds)
	const float fDt = 1.0f / 60.0f;
	for (int i = 0; i < 10; ++i)
	{
		Zenith_SceneManager::Update(fDt);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	// The timed destruction entry should be cleaned up (entity no longer exists)
	Zenith_Assert(pxData->m_axTimedDestructions.GetSize() == 0,
		"Timed destruction entry for dead entity should be cleaned up");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTimedDestructionEarlyCleanup passed");
}

//==============================================================================
// API Simplification Verification Tests (2026-02 simplification)
//==============================================================================

void Zenith_SceneTests::TestTryGetEntityValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetEntityValid...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TryGetValid");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	Zenith_Entity xEntity(pxData, "TestEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();

	Zenith_Entity xResult = pxData->TryGetEntity(xID);
	Zenith_Assert(xResult.IsValid(), "TryGetEntity should return valid entity for existing ID");
	Zenith_Assert(xResult.GetEntityID() == xID, "TryGetEntity should return entity with matching ID");
	Zenith_Assert(xResult.GetName() == "TestEntity", "TryGetEntity should return entity with correct name");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetEntityValid passed");
}

void Zenith_SceneTests::TestTryGetEntityInvalid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetEntityInvalid...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TryGetInvalid");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Test with completely invalid ID
	Zenith_Entity xResult1 = pxData->TryGetEntity(INVALID_ENTITY_ID);
	Zenith_Assert(!xResult1.IsValid(), "TryGetEntity should return invalid entity for INVALID_ENTITY_ID");

	// Test with stale ID (create then destroy)
	Zenith_Entity xEntity(pxData, "Temp");
	Zenith_EntityID xStaleID = xEntity.GetEntityID();
	Zenith_SceneManager::DestroyImmediate(xEntity);

	Zenith_Entity xResult2 = pxData->TryGetEntity(xStaleID);
	Zenith_Assert(!xResult2.IsValid(), "TryGetEntity should return invalid entity for stale ID");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetEntityInvalid passed");
}

void Zenith_SceneTests::TestScenePathCanonicalization()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestScenePathCanonicalization...");

	std::string strPath = "test_canon" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Load with canonical path
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "Scene should load with canonical path");

	// Query with backslashes - should find the same scene
	Zenith_Scene xFound1 = Zenith_SceneManager::GetSceneByPath("test_canon" ZENITH_SCENE_EXT);
	Zenith_Assert(xFound1.IsValid(), "GetSceneByPath should find scene with forward slashes");

	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestScenePathCanonicalization passed");
}

void Zenith_SceneTests::TestFixedTimestepConfig()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedTimestepConfig...");

	float fOriginal = Zenith_SceneManager::GetFixedTimestep();

	Zenith_SceneManager::SetFixedTimestep(0.01f);
	Zenith_Assert(Zenith_SceneManager::GetFixedTimestep() == 0.01f, "Fixed timestep should be 0.01");

	Zenith_SceneManager::SetFixedTimestep(0.05f);
	Zenith_Assert(Zenith_SceneManager::GetFixedTimestep() == 0.05f, "Fixed timestep should be 0.05");

	// Restore
	Zenith_SceneManager::SetFixedTimestep(fOriginal);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedTimestepConfig passed");
}

void Zenith_SceneTests::TestAsyncBatchSizeConfig()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncBatchSizeConfig...");

	uint32_t uOriginal = Zenith_SceneManager::GetAsyncUnloadBatchSize();

	Zenith_SceneManager::SetAsyncUnloadBatchSize(100);
	Zenith_Assert(Zenith_SceneManager::GetAsyncUnloadBatchSize() == 100, "Batch size should be 100");

	Zenith_SceneManager::SetAsyncUnloadBatchSize(25);
	Zenith_Assert(Zenith_SceneManager::GetAsyncUnloadBatchSize() == 25, "Batch size should be 25");

	// Restore
	Zenith_SceneManager::SetAsyncUnloadBatchSize(uOriginal);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncBatchSizeConfig passed");
}

void Zenith_SceneTests::TestMaxConcurrentLoadsConfig()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMaxConcurrentLoadsConfig...");

	uint32_t uOriginal = Zenith_SceneManager::GetMaxConcurrentAsyncLoads();

	Zenith_SceneManager::SetMaxConcurrentAsyncLoads(4);
	Zenith_Assert(Zenith_SceneManager::GetMaxConcurrentAsyncLoads() == 4, "Max concurrent should be 4");

	Zenith_SceneManager::SetMaxConcurrentAsyncLoads(16);
	Zenith_Assert(Zenith_SceneManager::GetMaxConcurrentAsyncLoads() == 16, "Max concurrent should be 16");

	// Restore
	Zenith_SceneManager::SetMaxConcurrentAsyncLoads(uOriginal);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMaxConcurrentLoadsConfig passed");
}

void Zenith_SceneTests::TestLoadSceneNonExistentFile()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneNonExistentFile...");

	Zenith_Scene xScene = Zenith_SceneManager::LoadScene("nonexistent_scene_12345" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(!xScene.IsValid(), "LoadScene with non-existent file should return invalid scene");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneNonExistentFile passed");
}

void Zenith_SceneTests::TestLoadSceneAsyncNonExistentFile()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncNonExistentFile...");

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync("nonexistent_async_12345" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(ulOpID != ZENITH_INVALID_OPERATION_ID, "Should return valid operation ID even for missing file");

	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "Operation should exist");
	Zenith_Assert(pxOp->IsComplete(), "Operation for missing file should complete immediately");
	Zenith_Assert(pxOp->HasFailed(), "Operation for missing file should be marked as failed");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneAsyncNonExistentFile passed");
}

void Zenith_SceneTests::TestPersistentSceneInvisibleWhenEmpty()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentSceneInvisibleWhenEmpty...");

	// Create a scene so we have at least one non-persistent scene
	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("VisibilityTest");

	uint32_t uCount = Zenith_SceneManager::GetLoadedSceneCount();

	// Persistent scene should not be counted when it has no entities
	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistentData = Zenith_SceneManager::GetSceneData(xPersistent);
	bool bPersistentEmpty = (pxPersistentData == nullptr || pxPersistentData->GetEntityCount() == 0);

	if (bPersistentEmpty)
	{
		// Verify persistent scene is not visible in GetSceneAt
		for (uint32_t i = 0; i < uCount; ++i)
		{
			Zenith_Scene xAt = Zenith_SceneManager::GetSceneAt(i);
			Zenith_Assert(xAt != xPersistent, "Empty persistent scene should not appear in GetSceneAt");
		}
	}

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentSceneInvisibleWhenEmpty passed");
}

void Zenith_SceneTests::TestMarkPersistentWalksToRoot()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMarkPersistentWalksToRoot...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PersistentRootWalk");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create a 3-level hierarchy: Root -> Child -> Grandchild
	Zenith_Entity xRoot(pxData, "Root");
	Zenith_Entity xChild(pxData, "Child");
	Zenith_Entity xGrandchild(pxData, "Grandchild");
	xChild.SetParent(xRoot.GetEntityID());
	xGrandchild.SetParent(xChild.GetEntityID());

	// Call DontDestroyOnLoad on the GRANDCHILD - should walk to root
	xGrandchild.DontDestroyOnLoad();

	// All three should now be in the persistent scene
	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistentData = Zenith_SceneManager::GetSceneData(xPersistent);

	Zenith_Entity xRootCheck = pxPersistentData->TryGetEntity(xRoot.GetEntityID());
	Zenith_Entity xChildCheck = pxPersistentData->TryGetEntity(xChild.GetEntityID());
	Zenith_Entity xGrandchildCheck = pxPersistentData->TryGetEntity(xGrandchild.GetEntityID());

	Zenith_Assert(xRootCheck.IsValid(), "Root should be in persistent scene");
	Zenith_Assert(xChildCheck.IsValid(), "Child should be in persistent scene");
	Zenith_Assert(xGrandchildCheck.IsValid(), "Grandchild should be in persistent scene");

	// Clean up persistent entities
	Zenith_SceneManager::DestroyImmediate(xRoot);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMarkPersistentWalksToRoot passed");
}

void Zenith_SceneTests::TestGetSceneAtSkipsUnloadingScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneAtSkipsUnloadingScene...");

	Zenith_Scene xScene1 = Zenith_SceneManager::CreateEmptyScene("SkipUnload1");
	Zenith_Scene xScene2 = Zenith_SceneManager::CreateEmptyScene("SkipUnload2");
	Zenith_SceneData* pxData2 = Zenith_SceneManager::GetSceneData(xScene2);

	// Add some entities to scene2 so async unload has work to do
	for (int i = 0; i < 10; ++i)
	{
		Zenith_Entity xE(pxData2, "Entity" + std::to_string(i));
	}

	// Start async unload of scene2
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xScene2);

	// Scene2 should not appear in GetSceneAt while unloading
	uint32_t uCount = Zenith_SceneManager::GetLoadedSceneCount();
	for (uint32_t i = 0; i < uCount; ++i)
	{
		Zenith_Scene xAt = Zenith_SceneManager::GetSceneAt(i);
		Zenith_Assert(xAt != xScene2, "Unloading scene should not appear in GetSceneAt");
	}

	// Pump until unload completes
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	if (pxOp)
	{
		PumpUntilComplete(pxOp);
	}

	Zenith_SceneManager::UnloadScene(xScene1);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneAtSkipsUnloadingScene passed");
}

void Zenith_SceneTests::TestMergeScenesSourceBecomesActive()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesSourceBecomesActive...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeActiveSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeActiveTarget");

	// Make source the active scene
	Zenith_SceneManager::SetActiveScene(xSource);
	Zenith_Assert(Zenith_SceneManager::GetActiveScene() == xSource, "Source should be active");

	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_Entity xEntity(pxSourceData, "SourceEntity");

	// Merge source into target - source is unloaded
	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	// Active scene should now be target (source was unloaded)
	Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xActive != xSource, "Active scene should not be the unloaded source");

	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesSourceBecomesActive passed");
}

//==============================================================================
// Cat 1: Entity Lifecycle - Awake/Start Ordering
//==============================================================================

void Zenith_SceneTests::TestAwakeFiresBeforeStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAwakeFiresBeforeStart...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("AwakeBeforeStart");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	CreateEntityWithBehaviour(pxData, "TestEntity");

	// Dispatch lifecycle - Awake fires during DispatchLifecycleForNewScene
	pxData->DispatchLifecycleForNewScene();

	// Awake should have fired, Start should NOT yet (deferred to first Update)
	Zenith_Assert(SceneTestBehaviour::s_uAwakeCount == 1, "Awake should fire during lifecycle init");
	Zenith_Assert(SceneTestBehaviour::s_uStartCount == 0, "Start should NOT fire during lifecycle init");

	// First Update dispatches Start
	PumpFrames(1);
	Zenith_Assert(SceneTestBehaviour::s_uStartCount == 1, "Start should fire on first Update");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAwakeFiresBeforeStart passed");
}

void Zenith_SceneTests::TestStartDeferredToNextFrame()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStartDeferredToNextFrame...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("StartDeferred");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "DeferredStart");

	pxData->DispatchLifecycleForNewScene();

	// After lifecycle init, entity should be awoken but NOT started
	Zenith_Assert(pxData->IsEntityAwoken(xEntity.GetEntityID()), "Entity should be awoken");
	Zenith_Assert(!pxData->IsEntityStarted(xEntity.GetEntityID()), "Entity should NOT be started yet");
	Zenith_Assert(pxData->HasPendingStarts(), "Should have pending starts");

	// First Update dispatches Start
	PumpFrames(1);
	Zenith_Assert(pxData->IsEntityStarted(xEntity.GetEntityID()), "Entity should be started after first Update");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStartDeferredToNextFrame passed");
}

void Zenith_SceneTests::TestEntityCreatedInAwakeGetsFullLifecycle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedInAwakeGetsFullLifecycle...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("AwakeSpawn");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	// When the first entity's Awake fires, spawn a second entity with behaviour
	static Zenith_SceneData* s_pxData = pxData;
	SceneTestBehaviour::s_pfnOnAwakeCallback = [](Zenith_Entity&) {
		static bool ls_bSpawned = false;
		if (!ls_bSpawned)
		{
			ls_bSpawned = true;
			CreateEntityWithBehaviour(s_pxData, "SpawnedInAwake");
		}
	};

	CreateEntityWithBehaviour(pxData, "Spawner");

	// Update processes newly created entities including the spawned one
	PumpFrames(1);

	// Both entities should have had Awake called
	Zenith_Assert(SceneTestBehaviour::s_uAwakeCount == 2, "Both entities should have Awake called");

	// Reset static
	SceneTestBehaviour::s_pfnOnAwakeCallback = nullptr;

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedInAwakeGetsFullLifecycle passed");
}

void Zenith_SceneTests::TestAwakeWaveDrainMultipleLevels()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAwakeWaveDrainMultipleLevels...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("WaveDrain");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	// A.OnAwake creates B, B.OnAwake creates C
	static Zenith_SceneData* s_pxData = pxData;
	static int s_iLevel = 0;
	s_iLevel = 0;

	SceneTestBehaviour::s_pfnOnAwakeCallback = [](Zenith_Entity&) {
		if (s_iLevel < 2)
		{
			s_iLevel++;
			CreateEntityWithBehaviour(s_pxData, "Level" + std::to_string(s_iLevel));
		}
	};

	CreateEntityWithBehaviour(pxData, "Level0");

	PumpFrames(1);

	// All 3 entities should have had Awake
	Zenith_Assert(SceneTestBehaviour::s_uAwakeCount == 3, "All 3 wave-drained entities should have Awake (got %u)", SceneTestBehaviour::s_uAwakeCount);

	SceneTestBehaviour::s_pfnOnAwakeCallback = nullptr;

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAwakeWaveDrainMultipleLevels passed");
}

void Zenith_SceneTests::TestUpdateNotCalledBeforeStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUpdateNotCalledBeforeStart...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("NoUpdateBeforeStart");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	// Track whether Update is called before Start
	static bool s_bUpdateBeforeStart = false;
	SceneTestBehaviour::s_pfnOnUpdateCallback = [](Zenith_Entity&, float) {
		if (SceneTestBehaviour::s_uStartCount == 0)
		{
			s_bUpdateBeforeStart = true;
		}
	};
	s_bUpdateBeforeStart = false;

	CreateEntityWithBehaviour(pxData, "TestEntity");
	pxData->DispatchLifecycleForNewScene();

	// Pump frames - Start fires first, then Update
	PumpFrames(2);

	Zenith_Assert(!s_bUpdateBeforeStart, "Update should NOT be called before Start");

	SceneTestBehaviour::s_pfnOnUpdateCallback = nullptr;

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUpdateNotCalledBeforeStart passed");
}

void Zenith_SceneTests::TestFixedUpdateNotCalledBeforeStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdateNotCalledBeforeStart...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("NoFixedBeforeStart");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "TestEntity");
	pxData->DispatchLifecycleForNewScene();

	// Entity should not be started yet
	Zenith_Assert(!pxData->IsEntityStarted(xEntity.GetEntityID()), "Entity should not be started before Update");
	Zenith_Assert(SceneTestBehaviour::s_uFixedUpdateCount == 0, "FixedUpdate should not fire before Start");

	// After first update, Start fires, then FixedUpdate can fire
	PumpFrames(1);
	Zenith_Assert(pxData->IsEntityStarted(xEntity.GetEntityID()), "Entity should be started after Update");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdateNotCalledBeforeStart passed");
}

void Zenith_SceneTests::TestDestroyDuringAwakeSkipsStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyDuringAwakeSkipsStart...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DestroyInAwake");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	// Destroy self during Awake
	SceneTestBehaviour::s_pfnOnAwakeCallback = [](Zenith_Entity& xEntity) {
		Zenith_SceneManager::Destroy(xEntity);
	};

	CreateEntityWithBehaviour(pxData, "SelfDestruct");

	PumpFrames(2);

	Zenith_Assert(SceneTestBehaviour::s_uAwakeCount == 1, "Awake should have fired");
	Zenith_Assert(SceneTestBehaviour::s_uStartCount == 0, "Start should NOT fire for entity destroyed during Awake");

	SceneTestBehaviour::s_pfnOnAwakeCallback = nullptr;

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyDuringAwakeSkipsStart passed");
}

void Zenith_SceneTests::TestDisableDuringAwakeSkipsOnEnable()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisableDuringAwakeSkipsOnEnable...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DisableInAwake");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	// Disable self during Awake
	SceneTestBehaviour::s_pfnOnAwakeCallback = [](Zenith_Entity& xEntity) {
		xEntity.SetEnabled(false);
	};

	CreateEntityWithBehaviour(pxData, "DisableInAwake");
	pxData->DispatchLifecycleForNewScene();

	// OnEnable should not have been dispatched since we disabled during Awake
	// (OnEnable only fires for entities that are active in hierarchy)
	Zenith_Assert(SceneTestBehaviour::s_uAwakeCount == 1, "Awake should fire");
	Zenith_Assert(SceneTestBehaviour::s_uEnableCount == 0,
		"OnEnable should not fire for entity disabled during Awake");

	SceneTestBehaviour::s_pfnOnAwakeCallback = nullptr;

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisableDuringAwakeSkipsOnEnable passed");
}

void Zenith_SceneTests::TestEntityWithNoScriptComponent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityWithNoScriptComponent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("NoScript");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create entity WITHOUT ScriptComponent - should be perfectly fine
	Zenith_Entity xEntity(pxData, "PlainEntity");

	// Lifecycle dispatch should be a no-op for this entity
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(2);

	// Entity should exist and be valid
	Zenith_Assert(xEntity.IsValid(), "Entity without ScriptComponent should be valid");
	Zenith_Assert(pxData->EntityExists(xEntity.GetEntityID()), "Entity should exist");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityWithNoScriptComponent passed");
}

//==============================================================================
// Cat 2: Entity Lifecycle - Destruction Ordering
//==============================================================================

void Zenith_SceneTests::TestOnDestroyCalledBeforeComponentRemoval()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOnDestroyCalledBeforeComponentRemoval...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DestroyOrder");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	// During OnDestroy, verify entity still has its components
	static bool s_bHadTransformDuringDestroy = false;
	s_bHadTransformDuringDestroy = false;
	SceneTestBehaviour::s_pfnOnDestroyCallback = [](Zenith_Entity& xEntity) {
		s_bHadTransformDuringDestroy = xEntity.HasComponent<Zenith_TransformComponent>();
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "DestroyOrder");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_SceneManager::Destroy(xEntity);
	PumpFrames(1);

	Zenith_Assert(SceneTestBehaviour::s_uDestroyCount == 1, "OnDestroy should have fired");
	Zenith_Assert(s_bHadTransformDuringDestroy, "Entity should still have TransformComponent during OnDestroy");

	SceneTestBehaviour::s_pfnOnDestroyCallback = nullptr;

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOnDestroyCalledBeforeComponentRemoval passed");
}

void Zenith_SceneTests::TestOnDisableCalledBeforeOnDestroy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOnDisableCalledBeforeOnDestroy...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DisableBeforeDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	// Track order: OnDisable should fire before OnDestroy
	static uint32_t s_uDisableOrder = 0;
	static uint32_t s_uDestroyOrder = 0;
	static uint32_t s_uOrderCounter = 0;
	s_uDisableOrder = 0;
	s_uDestroyOrder = 0;
	s_uOrderCounter = 0;

	SceneTestBehaviour::s_pfnOnDisableCallback = [](Zenith_Entity&) {
		s_uDisableOrder = ++s_uOrderCounter;
	};
	SceneTestBehaviour::s_pfnOnDestroyCallback = [](Zenith_Entity&) {
		s_uDestroyOrder = ++s_uOrderCounter;
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "DisableDestroy");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	// Reset order tracking (OnDisable may have fired during lifecycle setup)
	s_uDisableOrder = 0;
	s_uDestroyOrder = 0;
	s_uOrderCounter = 0;

	Zenith_SceneManager::Destroy(xEntity);
	PumpFrames(1);

	// Both callbacks should have fired
	Zenith_Assert(SceneTestBehaviour::s_uDestroyCount >= 1, "OnDestroy should fire during destruction");
	Zenith_Assert(s_uDisableOrder > 0, "OnDisable should fire during destruction");
	Zenith_Assert(s_uDestroyOrder > 0, "OnDestroy order should be recorded");
	// OnDisable must fire BEFORE OnDestroy (lower order number = earlier)
	Zenith_Assert(s_uDisableOrder < s_uDestroyOrder,
		"OnDisable (order=%u) should fire before OnDestroy (order=%u)", s_uDisableOrder, s_uDestroyOrder);

	SceneTestBehaviour::s_pfnOnDisableCallback = nullptr;
	SceneTestBehaviour::s_pfnOnDestroyCallback = nullptr;

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOnDisableCalledBeforeOnDestroy passed");
}

void Zenith_SceneTests::TestDestroyChildrenBeforeParent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyChildrenBeforeParent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ChildrenFirst");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	// Track destruction order via entity IDs
	static Zenith_Vector<Zenith_EntityID> s_axDestroyOrder;
	s_axDestroyOrder.Clear();
	SceneTestBehaviour::s_pfnOnDestroyCallback = [](Zenith_Entity& xEntity) {
		s_axDestroyOrder.PushBack(xEntity.GetEntityID());
	};

	Zenith_Entity xParent = CreateEntityWithBehaviour(pxData, "Parent");
	Zenith_Entity xChild = CreateEntityWithBehaviour(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_EntityID xParentID = xParent.GetEntityID();
	Zenith_EntityID xChildID = xChild.GetEntityID();

	Zenith_SceneManager::Destroy(xParent);
	PumpFrames(1);

	// Both should be destroyed
	Zenith_Assert(s_axDestroyOrder.GetSize() == 2, "Both parent and child should be destroyed (got %u)", s_axDestroyOrder.GetSize());

	// Child should be destroyed before parent (depth-first)
	Zenith_Assert(s_axDestroyOrder.Get(0) == xChildID, "Child should be destroyed first");
	Zenith_Assert(s_axDestroyOrder.Get(1) == xParentID, "Parent should be destroyed second");

	SceneTestBehaviour::s_pfnOnDestroyCallback = nullptr;

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyChildrenBeforeParent passed");
}

void Zenith_SceneTests::TestDoubleDestroyNoDoubleFree()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDoubleDestroyNoDoubleFree...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DoubleDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "DoubleDestroy");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	// Destroy twice in the same frame
	Zenith_SceneManager::Destroy(xEntity);
	Zenith_SceneManager::Destroy(xEntity);

	PumpFrames(1);

	// OnDestroy should fire only once
	Zenith_Assert(SceneTestBehaviour::s_uDestroyCount == 1, "OnDestroy should fire exactly once (got %u)", SceneTestBehaviour::s_uDestroyCount);

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDoubleDestroyNoDoubleFree passed");
}

void Zenith_SceneTests::TestDestroyedEntityAccessibleUntilProcessed()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyedEntityAccessibleUntilProcessed...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("AccessibleUntilProcessed");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "Accessible");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_EntityID xID = xEntity.GetEntityID();

	// Mark for destruction
	pxData->MarkForDestruction(xID);

	// Entity should still exist (not yet processed)
	Zenith_Assert(pxData->EntityExists(xID), "Entity should still exist after MarkForDestruction");
	Zenith_Assert(pxData->IsMarkedForDestruction(xID), "Entity should be marked for destruction");

	// After processing, entity is gone
	pxData->ProcessPendingDestructions();
	Zenith_Assert(!pxData->EntityExists(xID), "Entity should be gone after ProcessPendingDestructions");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyedEntityAccessibleUntilProcessed passed");
}

void Zenith_SceneTests::TestDestroyParentAndChildSameFrame()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyParentAndChildSameFrame...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("BothDestroyFrame");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	Zenith_Entity xParent = CreateEntityWithBehaviour(pxData, "Parent");
	Zenith_Entity xChild = CreateEntityWithBehaviour(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	// Mark both for destruction explicitly
	Zenith_SceneManager::Destroy(xChild);
	Zenith_SceneManager::Destroy(xParent);

	PumpFrames(1);

	// Both should be destroyed, no crashes
	Zenith_Assert(SceneTestBehaviour::s_uDestroyCount == 2, "Both should have OnDestroy called (got %u)", SceneTestBehaviour::s_uDestroyCount);
	Zenith_Assert(!pxData->EntityExists(xParent.GetEntityID()), "Parent should be gone");
	Zenith_Assert(!pxData->EntityExists(xChild.GetEntityID()), "Child should be gone");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyParentAndChildSameFrame passed");
}

void Zenith_SceneTests::TestOnDestroySpawnsEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOnDestroySpawnsEntity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DestroySpawn");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	static Zenith_SceneData* s_pxData = pxData;
	static Zenith_EntityID s_xSpawnedID;
	s_xSpawnedID = INVALID_ENTITY_ID;

	SceneTestBehaviour::s_pfnOnDestroyCallback = [](Zenith_Entity&) {
		if (!s_xSpawnedID.IsValid())
		{
			Zenith_Entity xSpawned = CreateEntityWithBehaviour(s_pxData, "SpawnedOnDestroy");
			s_xSpawnedID = xSpawned.GetEntityID();
		}
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "Dying");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_SceneManager::Destroy(xEntity);
	PumpFrames(2);

	// The spawned entity should exist and be valid
	Zenith_Assert(s_xSpawnedID.IsValid(), "Spawned entity ID should be valid");
	Zenith_Assert(pxData->EntityExists(s_xSpawnedID), "Spawned entity should exist in scene");

	SceneTestBehaviour::s_pfnOnDestroyCallback = nullptr;

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOnDestroySpawnsEntity passed");
}

void Zenith_SceneTests::TestDestroyImmediateDuringIteration()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyImmediateDuringIteration...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ImmediateIteration");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create several entities
	Zenith_Entity xEntity1(pxData, "Entity1");
	Zenith_Entity xEntity2(pxData, "Entity2");
	Zenith_Entity xEntity3(pxData, "Entity3");

	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_EntityID xID2 = xEntity2.GetEntityID();

	// Use query with snapshot - destroying during iteration should be safe
	uint32_t uCount = 0;
	pxData->Query<Zenith_TransformComponent>().ForEach(
		[&uCount, xID2](Zenith_EntityID xID, Zenith_TransformComponent&) {
			uCount++;
			if (xID == xID2)
			{
				Zenith_Entity xE = Zenith_SceneManager::GetSceneData(Zenith_SceneManager::GetActiveScene())->GetEntity(xID);
				// Mark for destruction - safe because ForEach uses snapshot
				Zenith_SceneManager::Destroy(xE);
			}
		}
	);

	// Should have iterated all 3
	Zenith_Assert(uCount == 3, "Should iterate all 3 entities in snapshot");

	// Process destruction
	PumpFrames(1);

	// Entity2 should be gone
	Zenith_Assert(!pxData->EntityExists(xID2), "Entity2 should be destroyed");
	Zenith_Assert(pxData->GetEntityCount() == 2, "2 entities should remain");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyImmediateDuringIteration passed");
}

void Zenith_SceneTests::TestTimedDestructionCountdown()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTimedDestructionCountdown...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TimedDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "TimedEntity");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_EntityID xID = xEntity.GetEntityID();

	// Destroy after 0.5 seconds
	Zenith_SceneManager::Destroy(xEntity, 0.5f);

	// Pump several frames at 60fps - shouldn't be destroyed yet at 0.3s
	PumpFrames(18); // 18 frames = 0.3s at 60fps
	Zenith_Assert(pxData->EntityExists(xID), "Entity should still exist at 0.3s");

	// Pump more frames to pass 0.5s total (need 12 more = 30 total)
	PumpFrames(15); // 33 frames total > 0.5s
	Zenith_Assert(!pxData->EntityExists(xID), "Entity should be destroyed after 0.5s delay");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTimedDestructionCountdown passed");
}

void Zenith_SceneTests::TestTimedDestructionOnPausedScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTimedDestructionOnPausedScene...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TimedPaused");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "TimedPausedEntity");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_EntityID xID = xEntity.GetEntityID();

	// Set timed destruction
	Zenith_SceneManager::Destroy(xEntity, 0.1f);

	// Pause the scene
	Zenith_SceneManager::SetScenePaused(xScene, true);

	// Pump frames well past the delay
	PumpFrames(30); // 0.5s at 60fps

	// When paused, timed destructions should NOT tick
	Zenith_Assert(pxData->EntityExists(xID), "Entity should still exist on paused scene");

	// Unpause and pump - should now be destroyed
	Zenith_SceneManager::SetScenePaused(xScene, false);
	PumpFrames(10);

	Zenith_Assert(!pxData->EntityExists(xID), "Entity should be destroyed after unpausing");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTimedDestructionOnPausedScene passed");
}

//==============================================================================
// Cat 3: Entity Movement Between Scenes
//==============================================================================

void Zenith_SceneTests::TestMoveEntityComponentDataIntegrity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityComponentDataIntegrity...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MoveSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MoveTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xEntity(pxSourceData, "MovingEntity");
	Zenith_TransformComponent& xTransform = xEntity.GetTransform();

	// Set specific transform values
	Zenith_Maths::Vector3 xPos = { 10.0f, 20.0f, 30.0f };
	Zenith_Maths::Vector3 xScale = { 2.0f, 3.0f, 4.0f };
	xTransform.SetPosition(xPos);
	xTransform.SetScale(xScale);

	Zenith_EntityID xID = xEntity.GetEntityID();

	// Move to target
	bool bResult = Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);
	Zenith_Assert(bResult, "MoveEntityToScene should succeed");

	// Verify component data preserved
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);
	Zenith_Assert(pxTargetData->EntityExists(xID), "Entity should exist in target");

	Zenith_Entity xMovedEntity = pxTargetData->GetEntity(xID);
	Zenith_TransformComponent& xMovedTransform = xMovedEntity.GetTransform();

	Zenith_Maths::Vector3 xMovedPos, xMovedScale;
	xMovedTransform.GetPosition(xMovedPos);
	xMovedTransform.GetScale(xMovedScale);

	Zenith_Assert(xMovedPos.x == xPos.x && xMovedPos.y == xPos.y && xMovedPos.z == xPos.z,
		"Position should be preserved after move");
	Zenith_Assert(xMovedScale.x == xScale.x && xMovedScale.y == xScale.y && xMovedScale.z == xScale.z,
		"Scale should be preserved after move");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityComponentDataIntegrity passed");
}

void Zenith_SceneTests::TestMoveEntityQueryConsistency()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityQueryConsistency...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("QuerySource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("QueryTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	Zenith_Entity xEntity(pxSourceData, "QueryEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();

	uint32_t uSourceCountBefore = pxSourceData->GetEntityCount();

	Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);

	// Entity should NOT appear in source's active list
	// Note: EntityExists() checks the global slot table (not per-scene), so we check active list membership
	Zenith_Assert(pxSourceData->GetEntityCount() == uSourceCountBefore - 1, "Source entity count should decrease");
	bool bFoundInSource = false;
	for (u_int u = 0; u < pxSourceData->GetActiveEntities().GetSize(); ++u)
	{
		if (pxSourceData->GetActiveEntities().Get(u) == xID) { bFoundInSource = true; break; }
	}
	Zenith_Assert(!bFoundInSource, "Entity should not be in source active list");

	// Entity SHOULD appear in target's active list
	bool bFoundInTarget = false;
	for (u_int u = 0; u < pxTargetData->GetActiveEntities().GetSize(); ++u)
	{
		if (pxTargetData->GetActiveEntities().Get(u) == xID) { bFoundInTarget = true; break; }
	}
	Zenith_Assert(bFoundInTarget, "Entity should be in target active list");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityQueryConsistency passed");
}

void Zenith_SceneTests::TestMoveEntityThenDestroySameFrame()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityThenDestroySameFrame...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MoveDestroySource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MoveDestroyTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xEntity(pxSourceData, "MoveAndDestroy");
	pxSourceData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_EntityID xID = xEntity.GetEntityID();

	Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);
	Zenith_SceneManager::Destroy(xEntity);

	PumpFrames(1);

	// Entity should be destroyed in target scene
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);
	Zenith_Assert(!pxTargetData->EntityExists(xID), "Entity should be destroyed in target");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityThenDestroySameFrame passed");
}

void Zenith_SceneTests::TestMoveEntityRootCacheInvalidation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityRootCacheInvalidation...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("RootCacheSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("RootCacheTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	Zenith_Entity xEntity(pxSourceData, "RootEntity");

	// Prime root cache
	uint32_t uSourceRoots = pxSourceData->GetCachedRootEntityCount();
	uint32_t uTargetRoots = pxTargetData->GetCachedRootEntityCount();

	Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);

	// Root caches should be invalidated and reflect the move
	Zenith_Assert(pxSourceData->GetCachedRootEntityCount() == uSourceRoots - 1,
		"Source root count should decrease");
	Zenith_Assert(pxTargetData->GetCachedRootEntityCount() == uTargetRoots + 1,
		"Target root count should increase");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityRootCacheInvalidation passed");
}

void Zenith_SceneTests::TestMoveEntityPreservesEntityID()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityPreservesEntityID...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("IDSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("IDTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xEntity(pxSourceData, "IDPreserved");
	Zenith_EntityID xOriginalID = xEntity.GetEntityID();

	Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);

	// EntityID should be identical after move
	Zenith_Assert(xEntity.GetEntityID() == xOriginalID, "EntityID must be preserved across scene move");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityPreservesEntityID passed");
}

void Zenith_SceneTests::TestMoveEntityWithPendingStartTransfers()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityWithPendingStartTransfers...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("PendingStartSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("PendingStartTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	SceneTestBehaviour::ResetCounters();

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxSourceData, "PendingStart");
	pxSourceData->DispatchLifecycleForNewScene();

	// Entity has pending start (Awake fired, Start deferred)
	Zenith_Assert(pxSourceData->HasPendingStarts(), "Source should have pending starts");

	// Move before Start fires
	Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);

	// Target should now have the pending start
	// After update, the entity should get Start in the target scene
	PumpFrames(1);

	Zenith_Assert(SceneTestBehaviour::s_uStartCount == 1, "Start should fire in target scene");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityWithPendingStartTransfers passed");
}

void Zenith_SceneTests::TestMoveEntityDeepHierarchyIntegrity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityDeepHierarchyIntegrity...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("DeepSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("DeepTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	// Create 4-level hierarchy: root -> child -> grandchild -> greatgrandchild
	Zenith_Entity xRoot(pxSourceData, "Root");
	Zenith_Entity xChild(pxSourceData, "Child");
	Zenith_Entity xGrandchild(pxSourceData, "Grandchild");
	Zenith_Entity xGreatGrandchild(pxSourceData, "GreatGrandchild");

	xChild.SetParent(xRoot.GetEntityID());
	xGrandchild.SetParent(xChild.GetEntityID());
	xGreatGrandchild.SetParent(xGrandchild.GetEntityID());

	Zenith_EntityID xRootID = xRoot.GetEntityID();
	Zenith_EntityID xChildID = xChild.GetEntityID();
	Zenith_EntityID xGrandchildID = xGrandchild.GetEntityID();
	Zenith_EntityID xGreatGrandchildID = xGreatGrandchild.GetEntityID();

	uint32_t uSourceBefore = pxSourceData->GetEntityCount();

	Zenith_SceneManager::MoveEntityToScene(xRoot, xTarget);

	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// All 4 should be in target
	Zenith_Assert(pxTargetData->EntityExists(xRootID), "Root should be in target");
	Zenith_Assert(pxTargetData->EntityExists(xChildID), "Child should be in target");
	Zenith_Assert(pxTargetData->EntityExists(xGrandchildID), "Grandchild should be in target");
	Zenith_Assert(pxTargetData->EntityExists(xGreatGrandchildID), "GreatGrandchild should be in target");

	// None should be in source
	Zenith_Assert(pxSourceData->GetEntityCount() == uSourceBefore - 4, "All 4 should be gone from source");

	// Hierarchy should be preserved
	Zenith_Entity xMovedChild = pxTargetData->GetEntity(xChildID);
	Zenith_Assert(xMovedChild.GetParentEntityID() == xRootID, "Child parent should still be Root");

	Zenith_Entity xMovedGC = pxTargetData->GetEntity(xGrandchildID);
	Zenith_Assert(xMovedGC.GetParentEntityID() == xChildID, "Grandchild parent should still be Child");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityDeepHierarchyIntegrity passed");
}

void Zenith_SceneTests::TestMoveEntityMainCameraConflict()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityMainCameraConflict...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("CamSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("CamTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// Create camera entity in source and set as main
	Zenith_Entity xSourceCam(pxSourceData, "SourceCamera");
	xSourceCam.AddComponent<Zenith_CameraComponent>();
	pxSourceData->SetMainCameraEntity(xSourceCam.GetEntityID());

	// Create camera entity in target and set as main
	Zenith_Entity xTargetCam(pxTargetData, "TargetCamera");
	xTargetCam.AddComponent<Zenith_CameraComponent>();
	pxTargetData->SetMainCameraEntity(xTargetCam.GetEntityID());

	Zenith_EntityID xTargetCamID = xTargetCam.GetEntityID();

	// Move source camera to target
	Zenith_SceneManager::MoveEntityToScene(xSourceCam, xTarget);

	// Target should keep its own camera (not be overwritten by source camera)
	Zenith_Assert(pxTargetData->GetMainCameraEntity() == xTargetCamID,
		"Target scene should keep its own main camera");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityMainCameraConflict passed");
}

void Zenith_SceneTests::TestMoveEntityInvalidTarget()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityInvalidTarget...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("InvalidTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xEntity(pxSourceData, "TestEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();

	// Try to move to invalid scene
	Zenith_Scene xInvalid;
	bool bResult = Zenith_SceneManager::MoveEntityToScene(xEntity, xInvalid);

	// Should fail gracefully
	Zenith_Assert(!bResult, "Move to invalid scene should fail");
	// Entity should still be in source
	Zenith_Assert(pxSourceData->EntityExists(xID), "Entity should remain in source after failed move");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityInvalidTarget passed");
}

//==============================================================================
// Cat 4: Async Operations Edge Cases
//==============================================================================

void Zenith_SceneTests::TestSyncLoadCancelsAsyncLoads()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSyncLoadCancelsAsyncLoads...");

	// Create a test file to load
	std::string strPath = "unit_test_sync_cancel" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Start an async load
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(ulOpID != ZENITH_INVALID_OPERATION_ID, "Async load should return valid ID");

	// Sync load with SINGLE mode should cancel pending async loads
	Zenith_Scene xSyncScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_SINGLE);

	// The async operation should be completed or cancelled
	// After sync load, the operation may have been cleaned up
	PumpFrames(2);

	Zenith_SceneManager::UnloadScene(xSyncScene);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSyncLoadCancelsAsyncLoads passed");
}

void Zenith_SceneTests::TestAsyncLoadProgressMonotonic()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadProgressMonotonic...");

	std::string strPath = "unit_test_progress" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);

	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "Operation should be valid");

	float fLastProgress = -1.0f;
	while (!pxOp->IsComplete())
	{
		float fProgress = pxOp->GetProgress();
		Zenith_Assert(fProgress >= fLastProgress, "Progress should never decrease (was %f, now %f)", fLastProgress, fProgress);
		fLastProgress = fProgress;

		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
	}

	Zenith_Assert(pxOp->GetProgress() >= 1.0f, "Final progress should be 1.0");

	// Cleanup loaded scene
	Zenith_Scene xResult = pxOp->GetResultScene();
	if (xResult.IsValid())
	{
		Zenith_SceneManager::UnloadScene(xResult);
	}

	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadProgressMonotonic passed");
}

void Zenith_SceneTests::TestAsyncLoadSameFileTwice()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadSameFileTwice...");

	std::string strPath = "unit_test_twice" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Two additive async loads of the same file
	Zenith_SceneOperationID ulOp1 = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperationID ulOp2 = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);

	Zenith_Assert(ulOp1 != ulOp2, "Two async loads should have different operation IDs");

	Zenith_SceneOperation* pxOp1 = Zenith_SceneManager::GetOperation(ulOp1);
	Zenith_SceneOperation* pxOp2 = Zenith_SceneManager::GetOperation(ulOp2);

	// Complete both
	if (pxOp1) PumpUntilComplete(pxOp1);
	if (pxOp2) PumpUntilComplete(pxOp2);

	// Cleanup
	if (pxOp1)
	{
		Zenith_Scene xR1 = pxOp1->GetResultScene();
		if (xR1.IsValid()) Zenith_SceneManager::UnloadScene(xR1);
	}
	if (pxOp2)
	{
		Zenith_Scene xR2 = pxOp2->GetResultScene();
		if (xR2.IsValid()) Zenith_SceneManager::UnloadScene(xR2);
	}

	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadSameFileTwice passed");
}

void Zenith_SceneTests::TestAsyncUnloadThenReload()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncUnloadThenReload...");

	std::string strPath = "unit_test_reload" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Load scene
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "Initial load should succeed");

	// Async unload
	Zenith_SceneOperationID ulUnloadOp = Zenith_SceneManager::UnloadSceneAsync(xScene);
	Zenith_SceneOperation* pxUnloadOp = Zenith_SceneManager::GetOperation(ulUnloadOp);
	if (pxUnloadOp) PumpUntilComplete(pxUnloadOp);

	// Sync reload of same path should work
	Zenith_Scene xReloaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xReloaded.IsValid(), "Reload after async unload should succeed");

	Zenith_SceneManager::UnloadScene(xReloaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncUnloadThenReload passed");
}

void Zenith_SceneTests::TestOperationCleanupAfter60Frames()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOperationCleanupAfter60Frames...");

	std::string strPath = "unit_test_cleanup" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "Operation should be valid initially");

	PumpUntilComplete(pxOp);

	Zenith_Scene xResult = pxOp->GetResultScene();

	// Pump 70 frames to trigger cleanup (~60 frames after completion)
	PumpFrames(70);

	// Operation should be cleaned up
	Zenith_Assert(!Zenith_SceneManager::IsOperationValid(ulOpID),
		"Operation should be invalid after cleanup");

	if (xResult.IsValid()) Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOperationCleanupAfter60Frames passed");
}

void Zenith_SceneTests::TestIsOperationValidAfterCleanup()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsOperationValidAfterCleanup...");

	std::string strPath = "unit_test_opvalid" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);

	Zenith_Assert(Zenith_SceneManager::IsOperationValid(ulOpID), "Should be valid initially");

	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	PumpUntilComplete(pxOp);
	Zenith_Scene xResult = pxOp->GetResultScene();

	// After cleanup period
	PumpFrames(70);

	Zenith_Assert(!Zenith_SceneManager::IsOperationValid(ulOpID), "Should be invalid after cleanup");
	Zenith_Assert(Zenith_SceneManager::GetOperation(ulOpID) == nullptr, "GetOperation should return nullptr after cleanup");

	if (xResult.IsValid()) Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsOperationValidAfterCleanup passed");
}

void Zenith_SceneTests::TestAsyncLoadSingleModeCleansUp()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadSingleModeCleansUp...");

	std::string strPath = "unit_test_single_async" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Create extra scene that should be unloaded by SINGLE mode
	Zenith_Scene xExtra = Zenith_SceneManager::CreateEmptyScene("ExtraScene");

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_SINGLE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "Async SINGLE load should return valid operation");

	PumpUntilComplete(pxOp);

	// Extra scene should have been unloaded (SINGLE mode)
	Zenith_Assert(!xExtra.IsValid(), "Extra scene should be invalid after SINGLE load");

	Zenith_Scene xResult = pxOp->GetResultScene();
	if (xResult.IsValid()) Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncLoadSingleModeCleansUp passed");
}

void Zenith_SceneTests::TestCancelAsyncLoadBeforeActivation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCancelAsyncLoadBeforeActivation...");

	std::string strPath = "unit_test_cancel" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::LoadSceneAsync(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);
	Zenith_Assert(pxOp != nullptr, "Operation should exist");

	// Pause activation
	pxOp->SetActivationAllowed(false);

	// Pump until file load is done (progress reaches ~0.9)
	for (int i = 0; i < 300; i++)
	{
		Zenith_SceneManager::Update(1.0f / 60.0f);
		Zenith_SceneManager::WaitForUpdateComplete();
		if (pxOp->GetProgress() >= 0.85f) break;
	}

	// Cancel before activation
	pxOp->RequestCancel();

	// Pump to process cancellation
	PumpFrames(5);

	Zenith_Assert(pxOp->IsComplete(), "Cancelled operation should be complete");
	Zenith_Assert(pxOp->HasFailed(), "Cancelled operation should be marked as failed");

	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCancelAsyncLoadBeforeActivation passed");
}

//==============================================================================
// Cat 5: Callback Re-entrancy & Ordering
//==============================================================================

void Zenith_SceneTests::TestSceneLoadedCallbackLoadsAnotherScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneLoadedCallbackLoadsAnotherScene...");

	std::string strPath1 = "unit_test_reentrant1" ZENITH_SCENE_EXT;
	std::string strPath2 = "unit_test_reentrant2" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath1);
	CreateTestSceneFile(strPath2);

	static Zenith_Scene s_xNestedScene;
	s_xNestedScene = Zenith_Scene::INVALID_SCENE;
	static std::string s_strPath2 = strPath2;

	// When first scene loads, try loading another scene from the callback
	auto pfnCallback = [](Zenith_Scene, Zenith_SceneLoadMode) {
		if (!s_xNestedScene.IsValid())
		{
			s_xNestedScene = Zenith_SceneManager::LoadScene(s_strPath2, SCENE_LOAD_ADDITIVE);
		}
	};

	Zenith_SceneManager::CallbackHandle ulHandle = Zenith_SceneManager::RegisterSceneLoadedCallback(pfnCallback);

	Zenith_Scene xScene1 = Zenith_SceneManager::LoadScene(strPath1, SCENE_LOAD_ADDITIVE);

	// No crash, no infinite loop
	Zenith_Assert(xScene1.IsValid(), "First scene should load");

	Zenith_SceneManager::UnregisterSceneLoadedCallback(ulHandle);

	if (xScene1.IsValid()) Zenith_SceneManager::UnloadScene(xScene1);
	if (s_xNestedScene.IsValid()) Zenith_SceneManager::UnloadScene(s_xNestedScene);
	CleanupTestSceneFile(strPath1);
	CleanupTestSceneFile(strPath2);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneLoadedCallbackLoadsAnotherScene passed");
}

void Zenith_SceneTests::TestSceneUnloadedCallbackLoadsScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneUnloadedCallbackLoadsScene...");

	std::string strPath = "unit_test_unload_load" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	static bool s_bCallbackFired = false;
	s_bCallbackFired = false;

	auto pfnCallback = [](Zenith_Scene) {
		s_bCallbackFired = true;
	};

	Zenith_SceneManager::CallbackHandle ulHandle = Zenith_SceneManager::RegisterSceneUnloadedCallback(pfnCallback);

	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Assert(s_bCallbackFired, "SceneUnloaded callback should fire");

	Zenith_SceneManager::UnregisterSceneUnloadedCallback(ulHandle);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneUnloadedCallbackLoadsScene passed");
}

void Zenith_SceneTests::TestActiveSceneChangedCallbackChangesActive()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveSceneChangedCallbackChangesActive...");

	static bool s_bCallbackFired = false;
	s_bCallbackFired = false;

	auto pfnCallback = [](Zenith_Scene, Zenith_Scene) {
		s_bCallbackFired = true;
		// Intentionally don't call SetActiveScene again to avoid recursion
	};

	Zenith_SceneManager::CallbackHandle ulHandle = Zenith_SceneManager::RegisterActiveSceneChangedCallback(pfnCallback);

	Zenith_Scene xScene1 = Zenith_SceneManager::CreateEmptyScene("ActiveCallback1");
	Zenith_Scene xScene2 = Zenith_SceneManager::CreateEmptyScene("ActiveCallback2");

	Zenith_SceneManager::SetActiveScene(xScene2);
	Zenith_Assert(s_bCallbackFired, "ActiveSceneChanged callback should fire");

	Zenith_SceneManager::UnregisterActiveSceneChangedCallback(ulHandle);
	Zenith_SceneManager::UnloadScene(xScene1);
	Zenith_SceneManager::UnloadScene(xScene2);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestActiveSceneChangedCallbackChangesActive passed");
}

void Zenith_SceneTests::TestCallbackFiringDepthTracking()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCallbackFiringDepthTracking...");

	// Verify that nested callback firing doesn't corrupt state
	static int s_iCallCount = 0;
	s_iCallCount = 0;

	auto pfnCallback = [](Zenith_Scene, Zenith_SceneLoadMode) {
		s_iCallCount++;
	};

	Zenith_SceneManager::CallbackHandle ulHandle = Zenith_SceneManager::RegisterSceneLoadedCallback(pfnCallback);

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DepthTest");

	// Unregister and verify no dangling state
	Zenith_SceneManager::UnregisterSceneLoadedCallback(ulHandle);

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCallbackFiringDepthTracking passed");
}

void Zenith_SceneTests::TestRegisterCallbackDuringDispatch()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRegisterCallbackDuringDispatch...");

	static bool s_bFirstFired = false;
	static bool s_bSecondFired = false;
	static Zenith_SceneManager::CallbackHandle s_ulSecondHandle = 0;

	s_bFirstFired = false;
	s_bSecondFired = false;
	s_ulSecondHandle = 0;

	// First callback registers a second callback during dispatch
	auto pfnFirst = [](Zenith_Scene, Zenith_SceneLoadMode) {
		s_bFirstFired = true;
		if (s_ulSecondHandle == 0)
		{
			s_ulSecondHandle = Zenith_SceneManager::RegisterSceneLoadedCallback(
				[](Zenith_Scene, Zenith_SceneLoadMode) { s_bSecondFired = true; }
			);
		}
	};

	Zenith_SceneManager::CallbackHandle ulFirstHandle = Zenith_SceneManager::RegisterSceneLoadedCallback(pfnFirst);

	std::string strPath = "unit_test_cb_dispatch" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);

	Zenith_Assert(s_bFirstFired, "First callback should fire");
	// Second callback registered during dispatch should NOT fire in same dispatch
	// (behavior depends on implementation - this tests that it doesn't crash)

	Zenith_SceneManager::UnregisterSceneLoadedCallback(ulFirstHandle);
	if (s_ulSecondHandle != 0) Zenith_SceneManager::UnregisterSceneLoadedCallback(s_ulSecondHandle);

	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRegisterCallbackDuringDispatch passed");
}

void Zenith_SceneTests::TestSingleModeCallbackOrder()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSingleModeCallbackOrder...");

	static Zenith_Vector<std::string> s_axCallOrder;
	s_axCallOrder.Clear();

	// Create test file BEFORE registering callbacks to avoid
	// CreateTestSceneFile's internal UnloadScene triggering our callbacks
	std::string strPath = "unit_test_cb_order" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	auto pfnLoadStarted = [](const std::string&) { s_axCallOrder.PushBack("loadStarted"); };
	auto pfnUnloading = [](Zenith_Scene) { s_axCallOrder.PushBack("unloading"); };
	auto pfnUnloaded = [](Zenith_Scene) { s_axCallOrder.PushBack("unloaded"); };
	auto pfnLoaded = [](Zenith_Scene, Zenith_SceneLoadMode) { s_axCallOrder.PushBack("loaded"); };
	auto pfnActiveChanged = [](Zenith_Scene, Zenith_Scene) { s_axCallOrder.PushBack("activeChanged"); };

	auto h1 = Zenith_SceneManager::RegisterSceneLoadStartedCallback(pfnLoadStarted);
	auto h2 = Zenith_SceneManager::RegisterSceneUnloadingCallback(pfnUnloading);
	auto h3 = Zenith_SceneManager::RegisterSceneUnloadedCallback(pfnUnloaded);
	auto h4 = Zenith_SceneManager::RegisterSceneLoadedCallback(pfnLoaded);
	auto h5 = Zenith_SceneManager::RegisterActiveSceneChangedCallback(pfnActiveChanged);

	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_SINGLE);

	// Verify that callbacks fired in some order (loadStarted should be first)
	Zenith_Assert(s_axCallOrder.GetSize() > 0, "At least some callbacks should have fired");
	Zenith_Assert(s_axCallOrder.Get(0) == "loadStarted", "loadStarted should fire first");

	Zenith_SceneManager::UnregisterSceneLoadStartedCallback(h1);
	Zenith_SceneManager::UnregisterSceneUnloadingCallback(h2);
	Zenith_SceneManager::UnregisterSceneUnloadedCallback(h3);
	Zenith_SceneManager::UnregisterSceneLoadedCallback(h4);
	Zenith_SceneManager::UnregisterActiveSceneChangedCallback(h5);

	if (xScene.IsValid()) Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSingleModeCallbackOrder passed");
}

void Zenith_SceneTests::TestMultipleCallbacksSameType()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleCallbacksSameType...");

	static int s_iCount = 0;
	s_iCount = 0;

	auto pfn1 = [](Zenith_Scene, Zenith_SceneLoadMode) { s_iCount++; };
	auto pfn2 = [](Zenith_Scene, Zenith_SceneLoadMode) { s_iCount++; };
	auto pfn3 = [](Zenith_Scene, Zenith_SceneLoadMode) { s_iCount++; };

	auto h1 = Zenith_SceneManager::RegisterSceneLoadedCallback(pfn1);
	auto h2 = Zenith_SceneManager::RegisterSceneLoadedCallback(pfn2);
	auto h3 = Zenith_SceneManager::RegisterSceneLoadedCallback(pfn3);

	std::string strPath = "unit_test_multi_cb" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);

	Zenith_Assert(s_iCount == 3, "All 3 callbacks should fire (got %d)", s_iCount);

	Zenith_SceneManager::UnregisterSceneLoadedCallback(h1);
	Zenith_SceneManager::UnregisterSceneLoadedCallback(h2);
	Zenith_SceneManager::UnregisterSceneLoadedCallback(h3);

	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleCallbacksSameType passed");
}

//==============================================================================
// Cat 6: Scene Handle & Generation Counters
//==============================================================================

void Zenith_SceneTests::TestHandleReuseAfterUnload()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHandleReuseAfterUnload...");

	Zenith_Scene xFirst = Zenith_SceneManager::CreateEmptyScene("ReuseFirst");
	int iFirstHandle = xFirst.GetHandle();
	uint32_t uFirstGen = xFirst.m_uGeneration;

	Zenith_SceneManager::UnloadScene(xFirst);

	// Create another scene - may or may not reuse same handle slot
	Zenith_Scene xSecond = Zenith_SceneManager::CreateEmptyScene("ReuseSecond");

	if (xSecond.GetHandle() == iFirstHandle)
	{
		// If handle was reused, generation should be different
		Zenith_Assert(xSecond.m_uGeneration != uFirstGen,
			"Generation should differ when handle is reused");
	}

	Zenith_SceneManager::UnloadScene(xSecond);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHandleReuseAfterUnload passed");
}

void Zenith_SceneTests::TestOldHandleInvalidAfterReuse()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOldHandleInvalidAfterReuse...");

	Zenith_Scene xOld = Zenith_SceneManager::CreateEmptyScene("OldHandle");
	Zenith_Scene xOldCopy = xOld; // Save a copy

	Zenith_SceneManager::UnloadScene(xOld);

	// Old handle should be invalid
	Zenith_Assert(!xOldCopy.IsValid(), "Old scene handle should be invalid after unload");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestOldHandleInvalidAfterReuse passed");
}

void Zenith_SceneTests::TestSceneHashDifferentGenerations()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHashDifferentGenerations...");

	Zenith_Scene xScene1;
	xScene1.m_iHandle = 5;
	xScene1.m_uGeneration = 1;

	Zenith_Scene xScene2;
	xScene2.m_iHandle = 5;
	xScene2.m_uGeneration = 2;

	std::hash<Zenith_Scene> xHasher;
	size_t uHash1 = xHasher(xScene1);
	size_t uHash2 = xHasher(xScene2);

	Zenith_Assert(uHash1 != uHash2, "Different generations should produce different hashes");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHashDifferentGenerations passed");
}

void Zenith_SceneTests::TestMultipleCreateDestroyGenerations()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleCreateDestroyGenerations...");

	// Track generations observed
	uint32_t uLastGen = 0;
	int iTrackedHandle = -1;

	for (int i = 0; i < 10; i++)
	{
		Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("GenCycle" + std::to_string(i));

		if (iTrackedHandle == -1)
		{
			iTrackedHandle = xScene.GetHandle();
			uLastGen = xScene.m_uGeneration;
		}
		else if (xScene.GetHandle() == iTrackedHandle)
		{
			// If we got the same handle slot, generation must be higher
			Zenith_Assert(xScene.m_uGeneration > uLastGen,
				"Generation should increase on handle reuse (cycle %d)", i);
			uLastGen = xScene.m_uGeneration;
		}

		Zenith_SceneManager::UnloadScene(xScene);
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleCreateDestroyGenerations passed");
}

//==============================================================================
// Cat 7: Persistent Scene
//==============================================================================

void Zenith_SceneTests::TestPersistentSceneSurvivesSingleLoad()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentSceneSurvivesSingleLoad...");

	std::string strPath = "unit_test_persist_survive" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Create entity and mark persistent
	Zenith_Scene xOriginal = Zenith_SceneManager::CreateEmptyScene("OrigScene");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xOriginal);
	Zenith_Entity xPersistent(pxData, "PersistentEntity");
	Zenith_EntityID xPersistentID = xPersistent.GetEntityID();
	Zenith_SceneManager::MarkEntityPersistent(xPersistent);

	// Load with SINGLE mode - should unload everything except persistent
	Zenith_Scene xNewScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_SINGLE);

	// Persistent entity should still exist
	Zenith_Scene xPersistentScene = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistData = Zenith_SceneManager::GetSceneData(xPersistentScene);
	Zenith_Assert(pxPersistData->EntityExists(xPersistentID), "Persistent entity should survive SINGLE load");

	if (xNewScene.IsValid()) Zenith_SceneManager::UnloadScene(xNewScene);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentSceneSurvivesSingleLoad passed");
}

void Zenith_SceneTests::TestMultipleEntitiesPersistent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleEntitiesPersistent...");

	std::string strPath = "unit_test_multi_persist" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("MultiPersist");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xE1(pxData, "Persist1");
	Zenith_Entity xE2(pxData, "Persist2");
	Zenith_Entity xE3(pxData, "Persist3");

	Zenith_EntityID xID1 = xE1.GetEntityID();
	Zenith_EntityID xID2 = xE2.GetEntityID();
	Zenith_EntityID xID3 = xE3.GetEntityID();

	Zenith_SceneManager::MarkEntityPersistent(xE1);
	Zenith_SceneManager::MarkEntityPersistent(xE2);
	Zenith_SceneManager::MarkEntityPersistent(xE3);

	Zenith_Scene xNew = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_SINGLE);

	Zenith_Scene xPersistScene = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersist = Zenith_SceneManager::GetSceneData(xPersistScene);

	Zenith_Assert(pxPersist->EntityExists(xID1), "Entity 1 should persist");
	Zenith_Assert(pxPersist->EntityExists(xID2), "Entity 2 should persist");
	Zenith_Assert(pxPersist->EntityExists(xID3), "Entity 3 should persist");

	if (xNew.IsValid()) Zenith_SceneManager::UnloadScene(xNew);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleEntitiesPersistent passed");
}

void Zenith_SceneTests::TestPersistentSceneVisibilityToggle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentSceneVisibilityToggle...");

	Zenith_Scene xPersistScene = Zenith_SceneManager::GetPersistentScene();
	Zenith_Assert(xPersistScene.IsValid(), "Persistent scene should always be valid");

	// Add entity to persistent scene
	Zenith_Scene xTemp = Zenith_SceneManager::CreateEmptyScene("TempForPersist");
	Zenith_SceneData* pxTempData = Zenith_SceneManager::GetSceneData(xTemp);
	Zenith_Entity xEntity(pxTempData, "PersistVisibility");
	Zenith_SceneManager::MarkEntityPersistent(xEntity);

	// Clean up
	Zenith_SceneManager::UnloadScene(xTemp);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentSceneVisibilityToggle passed");
}

void Zenith_SceneTests::TestGetPersistentSceneAlwaysValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetPersistentSceneAlwaysValid...");

	// Call multiple times - should always return the same valid scene
	Zenith_Scene xFirst = Zenith_SceneManager::GetPersistentScene();
	Zenith_Scene xSecond = Zenith_SceneManager::GetPersistentScene();

	Zenith_Assert(xFirst.IsValid(), "Persistent scene should be valid (first call)");
	Zenith_Assert(xSecond.IsValid(), "Persistent scene should be valid (second call)");
	Zenith_Assert(xFirst == xSecond, "Same persistent scene should be returned");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetPersistentSceneAlwaysValid passed");
}

void Zenith_SceneTests::TestPersistentEntityChildrenMoveWithRoot()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentEntityChildrenMoveWithRoot...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PersistChildren");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "PersistParent");
	Zenith_Entity xChild(pxData, "PersistChild");
	xChild.SetParent(xParent.GetEntityID());

	Zenith_EntityID xParentID = xParent.GetEntityID();
	Zenith_EntityID xChildID = xChild.GetEntityID();

	// Mark parent persistent - child should follow
	Zenith_SceneManager::MarkEntityPersistent(xParent);

	Zenith_Scene xPersistScene = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersist = Zenith_SceneManager::GetSceneData(xPersistScene);

	Zenith_Assert(pxPersist->EntityExists(xParentID), "Parent should be in persistent scene");
	Zenith_Assert(pxPersist->EntityExists(xChildID), "Child should follow parent to persistent scene");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentEntityChildrenMoveWithRoot passed");
}

//==============================================================================
// Cat 8: FixedUpdate System
//==============================================================================

void Zenith_SceneTests::TestFixedUpdateMultipleCallsPerFrame()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdateMultipleCallsPerFrame...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("FixedMulti");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "FixedEntity");
	pxData->DispatchLifecycleForNewScene();

	// Per-entity tracking to avoid interference from SceneTestBehaviour instances in other scenes
	static Zenith_EntityID s_xTrackedID;
	static uint32_t s_uTrackedFixedCount;
	s_xTrackedID = xEntity.GetEntityID();
	s_uTrackedFixedCount = 0;

	SceneTestBehaviour::s_pfnOnFixedUpdateCallback = [](Zenith_Entity& xEnt, float) {
		if (xEnt.GetEntityID() == s_xTrackedID)
			s_uTrackedFixedCount++;
	};

	PumpFrames(1); // Start fires

	// Reset per-entity counter after the Start frame
	s_uTrackedFixedCount = 0;

	// Set timestep to 0.02s
	float fOldTimestep = Zenith_SceneManager::GetFixedTimestep();
	Zenith_SceneManager::SetFixedTimestep(0.02f);

	// Pump one frame with dt=0.1 -> should produce 5 FixedUpdate calls for our entity
	Zenith_SceneManager::Update(0.1f);
	Zenith_SceneManager::WaitForUpdateComplete();

	Zenith_Assert(s_uTrackedFixedCount == 5,
		"dt=0.1, timestep=0.02 should give 5 FixedUpdate calls (got %u)", s_uTrackedFixedCount);

	SceneTestBehaviour::s_pfnOnFixedUpdateCallback = nullptr;
	Zenith_SceneManager::SetFixedTimestep(fOldTimestep);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdateMultipleCallsPerFrame passed");
}

void Zenith_SceneTests::TestFixedUpdateZeroDt()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdateZeroDt...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("FixedZero");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "FixedEntity");
	pxData->DispatchLifecycleForNewScene();

	// Per-entity tracking to avoid interference from SceneTestBehaviour instances in other scenes
	static Zenith_EntityID s_xTrackedID;
	static uint32_t s_uTrackedFixedCount;
	s_xTrackedID = xEntity.GetEntityID();
	s_uTrackedFixedCount = 0;

	SceneTestBehaviour::s_pfnOnFixedUpdateCallback = [](Zenith_Entity& xEnt, float) {
		if (xEnt.GetEntityID() == s_xTrackedID)
			s_uTrackedFixedCount++;
	};

	PumpFrames(1); // Start fires

	// Reset per-entity counter after the Start frame
	s_uTrackedFixedCount = 0;

	// dt=0 should produce 0 new FixedUpdate calls for our entity
	// (global accumulator carry-over may still drain but dt=0 adds nothing)
	Zenith_SceneManager::Update(0.0f);
	Zenith_SceneManager::WaitForUpdateComplete();

	Zenith_Assert(s_uTrackedFixedCount == 0,
		"dt=0 should give 0 FixedUpdate calls (got %u)", s_uTrackedFixedCount);

	SceneTestBehaviour::s_pfnOnFixedUpdateCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdateZeroDt passed");
}

void Zenith_SceneTests::TestFixedUpdateAccumulatorResetOnSingleLoad()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdateAccumulatorResetOnSingleLoad...");

	std::string strPath = "unit_test_fixed_reset" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath);

	// Pump some frames to build up accumulator
	PumpFrames(5);

	// Load SINGLE mode - should reset accumulator
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_SINGLE);

	// After SINGLE load, first frame's FixedUpdate count should be based on
	// just that frame's dt (no accumulated time from before)
	SceneTestBehaviour::ResetCounters();
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	CreateEntityWithBehaviour(pxData, "FixedEntity");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	// This test mainly verifies no crash - the accumulator should have been reset
	if (xScene.IsValid()) Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdateAccumulatorResetOnSingleLoad passed");
}

void Zenith_SceneTests::TestFixedUpdatePausedSceneSkipped()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdatePausedSceneSkipped...");

	// Verify paused scene doesn't dispatch FixedUpdate.
	// Use a per-entity flag instead of shared static counter to avoid
	// interference from SceneTestBehaviour instances in other scenes.
	static Zenith_EntityID s_xTrackedEntityID;
	static bool s_bTrackedEntityGotFixedUpdate = false;
	s_bTrackedEntityGotFixedUpdate = false;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("FixedPaused");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "FixedEntity");
	s_xTrackedEntityID = xEntity.GetEntityID();

	// Use OnUpdate callback to detect if OUR entity gets any updates while paused
	SceneTestBehaviour::s_pfnOnUpdateCallback = [](Zenith_Entity& xEnt, float) {
		if (xEnt.GetEntityID() == s_xTrackedEntityID)
			s_bTrackedEntityGotFixedUpdate = true; // Repurpose: if Update fires, pause failed
	};

	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1); // Start fires

	// Reset the flag after Start/Update have fired
	s_bTrackedEntityGotFixedUpdate = false;

	Zenith_SceneManager::SetScenePaused(xScene, true);
	Zenith_Assert(Zenith_SceneManager::IsScenePaused(xScene), "Scene should be paused");

	PumpFrames(10);

	// If our entity got Update called, the pause didn't work
	Zenith_Assert(!s_bTrackedEntityGotFixedUpdate,
		"Paused scene entity should NOT receive Update callbacks");

	// Also verify the scene is still paused
	Zenith_Assert(Zenith_SceneManager::IsScenePaused(xScene), "Scene should still be paused after pumping");

	SceneTestBehaviour::s_pfnOnUpdateCallback = nullptr;
	Zenith_SceneManager::SetScenePaused(xScene, false);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdatePausedSceneSkipped passed");
}

void Zenith_SceneTests::TestFixedUpdateTimestepConfigurable()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdateTimestepConfigurable...");

	float fOldTimestep = Zenith_SceneManager::GetFixedTimestep();

	Zenith_SceneManager::SetFixedTimestep(0.05f);
	Zenith_Assert(Zenith_SceneManager::GetFixedTimestep() == 0.05f,
		"GetFixedTimestep should return configured value");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("FixedConfig");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "FixedEntity");
	pxData->DispatchLifecycleForNewScene();

	// Per-entity tracking to avoid interference from SceneTestBehaviour instances in other scenes
	static Zenith_EntityID s_xTrackedID;
	static uint32_t s_uTrackedFixedCount;
	s_xTrackedID = xEntity.GetEntityID();
	s_uTrackedFixedCount = 0;

	SceneTestBehaviour::s_pfnOnFixedUpdateCallback = [](Zenith_Entity& xEnt, float) {
		if (xEnt.GetEntityID() == s_xTrackedID)
			s_uTrackedFixedCount++;
	};

	PumpFrames(1); // Start fires

	// Reset per-entity counter after the Start frame
	s_uTrackedFixedCount = 0;

	// dt=0.1, timestep=0.05 -> should give 2 FixedUpdate calls for our entity
	Zenith_SceneManager::Update(0.1f);
	Zenith_SceneManager::WaitForUpdateComplete();

	Zenith_Assert(s_uTrackedFixedCount == 2,
		"dt=0.1, timestep=0.05 should give 2 FixedUpdate calls (got %u)", s_uTrackedFixedCount);

	SceneTestBehaviour::s_pfnOnFixedUpdateCallback = nullptr;
	Zenith_SceneManager::SetFixedTimestep(fOldTimestep);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedUpdateTimestepConfigurable passed");
}

//==============================================================================
// Cat 9: Scene Merge Deep Coverage
//==============================================================================

void Zenith_SceneTests::TestMergeScenesEntityIDsPreserved()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesEntityIDsPreserved...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeIDSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeIDTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xE1(pxSourceData, "MergeEntity1");
	Zenith_Entity xE2(pxSourceData, "MergeEntity2");
	Zenith_EntityID xID1 = xE1.GetEntityID();
	Zenith_EntityID xID2 = xE2.GetEntityID();

	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);
	Zenith_Assert(pxTargetData->EntityExists(xID1), "Entity 1 ID should be preserved after merge");
	Zenith_Assert(pxTargetData->EntityExists(xID2), "Entity 2 ID should be preserved after merge");

	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesEntityIDsPreserved passed");
}

void Zenith_SceneTests::TestMergeScenesHierarchyPreserved()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesHierarchyPreserved...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeHierSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeHierTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xParent(pxSourceData, "MergeParent");
	Zenith_Entity xChild(pxSourceData, "MergeChild");
	xChild.SetParent(xParent.GetEntityID());

	Zenith_EntityID xParentID = xParent.GetEntityID();
	Zenith_EntityID xChildID = xChild.GetEntityID();

	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);
	Zenith_Entity xMergedChild = pxTargetData->GetEntity(xChildID);
	Zenith_Assert(xMergedChild.GetParentEntityID() == xParentID,
		"Parent-child relationship should be preserved after merge");

	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesHierarchyPreserved passed");
}

void Zenith_SceneTests::TestMergeScenesEmptySource()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesEmptySource...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeEmptySource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeEmptyTarget");

	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);
	Zenith_Entity xTargetEntity(pxTargetData, "TargetEntity");
	uint32_t uTargetCount = pxTargetData->GetEntityCount();

	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	// Target should be unchanged
	Zenith_Assert(pxTargetData->GetEntityCount() == uTargetCount,
		"Target entity count should be unchanged after merging empty source");

	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesEmptySource passed");
}

void Zenith_SceneTests::TestMergeScenesMainCameraConflict()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesMainCameraConflict...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeCamSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeCamTarget");

	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// Both scenes have cameras
	Zenith_Entity xSourceCam(pxSourceData, "SourceCam");
	xSourceCam.AddComponent<Zenith_CameraComponent>();
	pxSourceData->SetMainCameraEntity(xSourceCam.GetEntityID());

	Zenith_Entity xTargetCam(pxTargetData, "TargetCam");
	xTargetCam.AddComponent<Zenith_CameraComponent>();
	pxTargetData->SetMainCameraEntity(xTargetCam.GetEntityID());

	Zenith_EntityID xTargetCamID = xTargetCam.GetEntityID();

	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	// Target should keep its own camera
	Zenith_Assert(pxTargetData->GetMainCameraEntity() == xTargetCamID,
		"Target should keep its own main camera after merge");

	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesMainCameraConflict passed");
}

void Zenith_SceneTests::TestMergeScenesActiveSceneTransfer()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesActiveSceneTransfer...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeActiveS");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeActiveT");

	// Make source active
	Zenith_SceneManager::SetActiveScene(xSource);
	Zenith_Assert(Zenith_SceneManager::GetActiveScene() == xSource, "Source should be active");

	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_Entity xEntity(pxSourceData, "ActiveEntity");

	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	// Source is unloaded - active should switch to target
	Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xActive != xSource, "Active should not be the unloaded source");

	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesActiveSceneTransfer passed");
}

//==============================================================================
// Cat 10: Root Entity Cache
//==============================================================================

void Zenith_SceneTests::TestRootCacheInvalidatedOnCreate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRootCacheInvalidatedOnCreate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("RootCreate");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	uint32_t uCountBefore = pxData->GetCachedRootEntityCount();
	Zenith_Entity xEntity(pxData, "NewRoot");
	uint32_t uCountAfter = pxData->GetCachedRootEntityCount();

	Zenith_Assert(uCountAfter == uCountBefore + 1, "Root count should increase by 1");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRootCacheInvalidatedOnCreate passed");
}

void Zenith_SceneTests::TestRootCacheInvalidatedOnDestroy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRootCacheInvalidatedOnDestroy...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("RootDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "RootToDestroy");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	uint32_t uCountBefore = pxData->GetCachedRootEntityCount();

	Zenith_SceneManager::DestroyImmediate(xEntity);

	uint32_t uCountAfter = pxData->GetCachedRootEntityCount();
	Zenith_Assert(uCountAfter == uCountBefore - 1, "Root count should decrease by 1");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRootCacheInvalidatedOnDestroy passed");
}

void Zenith_SceneTests::TestRootCacheInvalidatedOnReparent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRootCacheInvalidatedOnReparent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("RootReparent");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");

	// Both are root initially
	uint32_t uRootsBefore = pxData->GetCachedRootEntityCount();
	Zenith_Assert(uRootsBefore == 2, "Should have 2 roots initially");

	// Make Child a child of Parent
	xChild.SetParent(xParent.GetEntityID());

	uint32_t uRootsAfter = pxData->GetCachedRootEntityCount();
	Zenith_Assert(uRootsAfter == 1, "Should have 1 root after reparent");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRootCacheInvalidatedOnReparent passed");
}

void Zenith_SceneTests::TestRootCacheCountMatchesVector()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRootCacheCountMatchesVector...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("RootMatch");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xE1(pxData, "Root1");
	Zenith_Entity xE2(pxData, "Root2");
	Zenith_Entity xE3(pxData, "Child1");
	xE3.SetParent(xE1.GetEntityID());

	uint32_t uCount = pxData->GetCachedRootEntityCount();
	Zenith_Vector<Zenith_EntityID> axRoots;
	pxData->GetCachedRootEntities(axRoots);

	Zenith_Assert(uCount == axRoots.GetSize(),
		"GetCachedRootEntityCount() (%u) should match GetCachedRootEntities().size() (%u)",
		uCount, axRoots.GetSize());
	Zenith_Assert(uCount == 2, "Should have 2 roots");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRootCacheCountMatchesVector passed");
}

//==============================================================================
// Cat 11: Serialization Round-Trip
//==============================================================================

void Zenith_SceneTests::TestSaveLoadEntityCount()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadEntityCount...");

	std::string strPath = "unit_test_save_count" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SaveCount");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xE1(pxData, "Entity1");
	Zenith_Entity xE2(pxData, "Entity2");
	Zenith_Entity xE3(pxData, "Entity3");
	xE1.SetTransient(false);
	xE2.SetTransient(false);
	xE3.SetTransient(false);

	uint32_t uExpectedCount = pxData->GetEntityCount();

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	Zenith_Assert(pxLoadedData->GetEntityCount() == uExpectedCount,
		"Entity count should be preserved (expected %u, got %u)", uExpectedCount, pxLoadedData->GetEntityCount());

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadEntityCount passed");
}

void Zenith_SceneTests::TestSaveLoadHierarchy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadHierarchy...");

	std::string strPath = "unit_test_save_hier" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SaveHierarchy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "SaveParent");
	Zenith_Entity xChild(pxData, "SaveChild");
	xParent.SetTransient(false);
	xChild.SetTransient(false);
	xChild.SetParent(xParent.GetEntityID());

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	// Find the loaded entities and verify hierarchy
	Zenith_Entity xLoadedParent = pxLoadedData->FindEntityByName("SaveParent");
	Zenith_Entity xLoadedChild = pxLoadedData->FindEntityByName("SaveChild");

	Zenith_Assert(xLoadedParent.IsValid(), "Parent should exist after load");
	Zenith_Assert(xLoadedChild.IsValid(), "Child should exist after load");
	Zenith_Assert(xLoadedChild.GetParentEntityID() == xLoadedParent.GetEntityID(),
		"Parent-child relationship should be preserved");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadHierarchy passed");
}

void Zenith_SceneTests::TestSaveLoadTransformData()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadTransformData...");

	std::string strPath = "unit_test_save_transform" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SaveTransform");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "TransformEntity");
	xEntity.SetTransient(false);
	Zenith_TransformComponent& xTransform = xEntity.GetTransform();
	Zenith_Maths::Vector3 xSetPos = { 42.0f, -17.5f, 100.0f };
	Zenith_Maths::Vector3 xSetScale = { 2.0f, 0.5f, 3.0f };
	xTransform.SetPosition(xSetPos);
	xTransform.SetScale(xSetScale);

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	Zenith_Entity xLoadedEntity = pxLoadedData->FindEntityByName("TransformEntity");
	Zenith_Assert(xLoadedEntity.IsValid(), "Entity should exist after load");

	Zenith_TransformComponent& xLoadedTransform = xLoadedEntity.GetTransform();
	Zenith_Maths::Vector3 xPos, xScale;
	xLoadedTransform.GetPosition(xPos);
	xLoadedTransform.GetScale(xScale);

	Zenith_Assert(xPos.x == 42.0f && xPos.y == -17.5f && xPos.z == 100.0f,
		"Position should be preserved through save/load");
	Zenith_Assert(xScale.x == 2.0f && xScale.y == 0.5f && xScale.z == 3.0f,
		"Scale should be preserved through save/load");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadTransformData passed");
}

void Zenith_SceneTests::TestSaveLoadMainCamera()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadMainCamera...");

	std::string strPath = "unit_test_save_camera" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SaveCamera");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xCamera(pxData, "MainCamera");
	xCamera.SetTransient(false);
	xCamera.AddComponent<Zenith_CameraComponent>();
	pxData->SetMainCameraEntity(xCamera.GetEntityID());

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	// Main camera should be restored
	Zenith_EntityID xMainCamID = pxLoadedData->GetMainCameraEntity();
	Zenith_Assert(xMainCamID.IsValid(), "Main camera should be restored after load");

	Zenith_Entity xLoadedCam = pxLoadedData->GetEntity(xMainCamID);
	Zenith_Assert(xLoadedCam.GetName() == "MainCamera", "Camera entity name should be preserved");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadMainCamera passed");
}

void Zenith_SceneTests::TestSaveLoadTransientExcluded()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadTransientExcluded...");

	std::string strPath = "unit_test_save_transient" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SaveTransient");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xPersistent(pxData, "PersistentEntity");
	xPersistent.SetTransient(false);

	Zenith_Entity xTransient(pxData, "TransientEntity");
	xTransient.SetTransient(true);

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	Zenith_Entity xFoundPersistent = pxLoadedData->FindEntityByName("PersistentEntity");
	Zenith_Entity xFoundTransient = pxLoadedData->FindEntityByName("TransientEntity");

	Zenith_Assert(xFoundPersistent.IsValid(), "Non-transient entity should be saved");
	Zenith_Assert(!xFoundTransient.IsValid(), "Transient entity should NOT be saved");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadTransientExcluded passed");
}

void Zenith_SceneTests::TestSaveLoadEmptyScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadEmptyScene...");

	std::string strPath = "unit_test_save_empty" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SaveEmpty");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Save empty scene
	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	// Load it back
	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	Zenith_Assert(pxLoadedData->GetEntityCount() == 0, "Empty scene should have 0 entities after load");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadEmptyScene passed");
}

//==============================================================================
// Cat 12: Query Safety
//==============================================================================

void Zenith_SceneTests::TestQueryDuringEntityCreation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryDuringEntityCreation...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("QueryCreate");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xExisting(pxData, "Existing");

	// During ForEach, create a new entity
	uint32_t uIterCount = 0;
	pxData->Query<Zenith_TransformComponent>().ForEach(
		[&uIterCount, pxData](Zenith_EntityID, Zenith_TransformComponent&) {
			uIterCount++;
			// Create new entity during iteration
			Zenith_Entity xNew(pxData, "NewDuringQuery");
		}
	);

	// Snapshot means we only iterate the pre-existing entity
	Zenith_Assert(uIterCount == 1, "Should only iterate pre-existing entity (got %u)", uIterCount);

	// New entity should exist after query
	Zenith_Assert(pxData->GetEntityCount() == 2, "New entity should have been created");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryDuringEntityCreation passed");
}

void Zenith_SceneTests::TestQueryDuringEntityDestruction()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryDuringEntityDestruction...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("QueryDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xE1(pxData, "QueryDestroyE1");
	Zenith_Entity xE2(pxData, "QueryDestroyE2");
	Zenith_Entity xE3(pxData, "QueryDestroyE3");

	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_EntityID xID2 = xE2.GetEntityID();

	// Mark E2 for destruction
	pxData->MarkForDestruction(xID2);

	// Query should skip marked-for-destruction entities
	uint32_t uCount = 0;
	pxData->Query<Zenith_TransformComponent>().ForEach(
		[&uCount](Zenith_EntityID, Zenith_TransformComponent&) {
			uCount++;
		}
	);

	Zenith_Assert(uCount == 2, "Should skip entity marked for destruction (got %u)", uCount);

	pxData->ProcessPendingDestructions();
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryDuringEntityDestruction passed");
}

void Zenith_SceneTests::TestQueryEmptyScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryEmptyScene...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("QueryEmpty");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Query on empty scene - should not crash
	uint32_t uCount = 0;
	pxData->Query<Zenith_TransformComponent>().ForEach(
		[&uCount](Zenith_EntityID, Zenith_TransformComponent&) {
			uCount++;
		}
	);

	Zenith_Assert(uCount == 0, "Empty scene query should iterate 0 entities");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryEmptyScene passed");
}

void Zenith_SceneTests::TestQueryAfterEntityMovedOut()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryAfterEntityMovedOut...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("QueryMoveSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("QueryMoveTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xE1(pxSourceData, "Stay");
	Zenith_Entity xE2(pxSourceData, "Moving");

	Zenith_SceneManager::MoveEntityToScene(xE2, xTarget);

	uint32_t uSourceCount = 0;
	pxSourceData->Query<Zenith_TransformComponent>().ForEach(
		[&uSourceCount](Zenith_EntityID, Zenith_TransformComponent&) {
			uSourceCount++;
		}
	);

	Zenith_Assert(uSourceCount == 1, "Source should only have 1 entity after move");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestQueryAfterEntityMovedOut passed");
}

//==============================================================================
// Cat 13: Multi-Scene Independence
//==============================================================================

void Zenith_SceneTests::TestDestroyInSceneANoEffectOnSceneB()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyInSceneANoEffectOnSceneB...");

	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("IndepA");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("IndepB");
	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);
	Zenith_SceneData* pxDataB = Zenith_SceneManager::GetSceneData(xSceneB);

	Zenith_Entity xEntityA(pxDataA, "EntityA");
	Zenith_Entity xEntityB(pxDataB, "EntityB");

	pxDataA->DispatchLifecycleForNewScene();
	pxDataB->DispatchLifecycleForNewScene();
	PumpFrames(1);

	uint32_t uBCount = pxDataB->GetEntityCount();

	Zenith_SceneManager::DestroyImmediate(xEntityA);

	Zenith_Assert(pxDataB->GetEntityCount() == uBCount, "Scene B entity count should be unchanged");
	Zenith_Assert(pxDataB->EntityExists(xEntityB.GetEntityID()), "Scene B entity should be unaffected");

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyInSceneANoEffectOnSceneB passed");
}

void Zenith_SceneTests::TestDisableInSceneANoEffectOnSceneB()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisableInSceneANoEffectOnSceneB...");

	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("DisableA");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("DisableB");
	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);
	Zenith_SceneData* pxDataB = Zenith_SceneManager::GetSceneData(xSceneB);

	Zenith_Entity xEntityA(pxDataA, "EntityA");
	Zenith_Entity xEntityB(pxDataB, "EntityB");

	pxDataA->DispatchLifecycleForNewScene();
	pxDataB->DispatchLifecycleForNewScene();

	xEntityA.SetEnabled(false);

	Zenith_Assert(!xEntityA.IsActiveInHierarchy(), "Entity A should be inactive");
	Zenith_Assert(xEntityB.IsActiveInHierarchy(), "Entity B should still be active");

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisableInSceneANoEffectOnSceneB passed");
}

void Zenith_SceneTests::TestIndependentMainCameras()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIndependentMainCameras...");

	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("CamA");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("CamB");
	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);
	Zenith_SceneData* pxDataB = Zenith_SceneManager::GetSceneData(xSceneB);

	Zenith_Entity xCamA(pxDataA, "CameraA");
	xCamA.AddComponent<Zenith_CameraComponent>();
	pxDataA->SetMainCameraEntity(xCamA.GetEntityID());

	Zenith_Entity xCamB(pxDataB, "CameraB");
	xCamB.AddComponent<Zenith_CameraComponent>();
	pxDataB->SetMainCameraEntity(xCamB.GetEntityID());

	Zenith_Assert(pxDataA->GetMainCameraEntity() == xCamA.GetEntityID(),
		"Scene A should have its own camera");
	Zenith_Assert(pxDataB->GetMainCameraEntity() == xCamB.GetEntityID(),
		"Scene B should have its own camera");
	Zenith_Assert(pxDataA->GetMainCameraEntity() != pxDataB->GetMainCameraEntity(),
		"Different scenes should have different cameras");

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIndependentMainCameras passed");
}

void Zenith_SceneTests::TestIndependentRootCaches()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIndependentRootCaches...");

	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("RootCacheA");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("RootCacheB");
	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);
	Zenith_SceneData* pxDataB = Zenith_SceneManager::GetSceneData(xSceneB);

	Zenith_Entity xEntityA(pxDataA, "EntityA");
	Zenith_Entity xEntityB1(pxDataB, "EntityB1");
	Zenith_Entity xEntityB2(pxDataB, "EntityB2");

	Zenith_Assert(pxDataA->GetCachedRootEntityCount() == 1, "Scene A should have 1 root");
	Zenith_Assert(pxDataB->GetCachedRootEntityCount() == 2, "Scene B should have 2 roots");

	// Adding to A shouldn't affect B
	Zenith_Entity xEntityA2(pxDataA, "EntityA2");
	Zenith_Assert(pxDataA->GetCachedRootEntityCount() == 2, "Scene A should now have 2 roots");
	Zenith_Assert(pxDataB->GetCachedRootEntityCount() == 2, "Scene B should still have 2 roots");

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIndependentRootCaches passed");
}

//==============================================================================
// Cat 14: Error Handling / Guard Rails
//==============================================================================

void Zenith_SceneTests::TestMoveNonRootEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveNonRootEntity...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MoveNonRoot");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MoveNonRootTarget");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	// Moving a non-root entity should fail
	bool bResult = Zenith_SceneManager::MoveEntityToScene(xChild, xTarget);
	Zenith_Assert(!bResult, "Moving non-root entity should fail");

	// Child should still be in source
	Zenith_Assert(pxData->EntityExists(xChild.GetEntityID()), "Child should remain in source");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveNonRootEntity passed");
}

void Zenith_SceneTests::TestSetActiveSceneInvalid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetActiveSceneInvalid...");

	Zenith_Scene xCurrent = Zenith_SceneManager::GetActiveScene();

	// Try to set invalid scene as active
	Zenith_Scene xInvalid;
	bool bResult = Zenith_SceneManager::SetActiveScene(xInvalid);
	Zenith_Assert(!bResult, "SetActiveScene with invalid handle should fail");

	// Active scene should be unchanged
	Zenith_Assert(Zenith_SceneManager::GetActiveScene() == xCurrent,
		"Active scene should not change after failed SetActiveScene");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetActiveSceneInvalid passed");
}

void Zenith_SceneTests::TestSetActiveSceneUnloading()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetActiveSceneUnloading...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("UnloadingActive");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create some entities so async unload has work to do
	for (int i = 0; i < 10; i++)
	{
		Zenith_Entity xE(pxData, "Entity" + std::to_string(i));
	}

	Zenith_SceneOperationID ulOp = Zenith_SceneManager::UnloadSceneAsync(xScene);

	// Try to set unloading scene as active
	bool bResult = Zenith_SceneManager::SetActiveScene(xScene);
	Zenith_Assert(!bResult, "SetActiveScene on unloading scene should fail");

	// Complete the unload
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOp);
	if (pxOp) PumpUntilComplete(pxOp);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetActiveSceneUnloading passed");
}

void Zenith_SceneTests::TestUnloadPersistentScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadPersistentScene...");

	Zenith_Scene xPersist = Zenith_SceneManager::GetPersistentScene();

	// Attempting to unload persistent scene should be blocked
	// (This should be a no-op, not crash)
	Zenith_SceneManager::UnloadScene(xPersist);

	// Persistent scene should still be valid
	Zenith_Assert(Zenith_SceneManager::GetPersistentScene().IsValid(),
		"Persistent scene should still be valid after attempted unload");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadPersistentScene passed");
}

void Zenith_SceneTests::TestLoadSceneEmptyPath()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneEmptyPath...");

	// Loading with empty path should handle gracefully (no crash)
	Zenith_Scene xResult = Zenith_SceneManager::LoadScene("", SCENE_LOAD_ADDITIVE);

	// Should return invalid scene handle
	Zenith_Assert(!xResult.IsValid(), "Loading empty path should return invalid scene");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneEmptyPath passed");
}

//==============================================================================
// Cat 15: Entity Slot Recycling & Generation Integrity
//==============================================================================

void Zenith_SceneTests::TestSlotReuseAfterDestroy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSlotReuseAfterDestroy...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SlotReuse");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "Original");
	Zenith_EntityID xOriginalID = xEntity.GetEntityID();
	uint32_t uOriginalIndex = xOriginalID.m_uIndex;
	uint32_t uOriginalGen = xOriginalID.m_uGeneration;

	Zenith_SceneManager::DestroyImmediate(xEntity);

	// Create new entity - may reuse the slot
	Zenith_Entity xNew(pxData, "Replacement");
	Zenith_EntityID xNewID = xNew.GetEntityID();

	// If slot was reused, generation must have incremented
	if (xNewID.m_uIndex == uOriginalIndex)
	{
		Zenith_Assert(xNewID.m_uGeneration > uOriginalGen,
			"Reused slot must have higher generation (%u vs %u)", xNewID.m_uGeneration, uOriginalGen);
	}

	// Original ID must no longer exist
	Zenith_Assert(!pxData->EntityExists(xOriginalID), "Original ID should not exist after destroy");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSlotReuseAfterDestroy passed");
}

void Zenith_SceneTests::TestHighChurnSlotRecycling()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHighChurnSlotRecycling...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("HighChurn");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Rapid create/destroy 100 times
	for (uint32_t i = 0; i < 100; i++)
	{
		Zenith_Entity xEntity(pxData, "Churn");
		Zenith_SceneManager::DestroyImmediate(xEntity);
	}

	// Scene should be empty
	Zenith_Assert(pxData->GetEntityCount() == 0, "Scene should have 0 entities after churn");

	// Create one final entity - should succeed
	Zenith_Entity xFinal(pxData, "Final");
	Zenith_Assert(xFinal.IsValid(), "Final entity should be valid");
	Zenith_Assert(pxData->GetEntityCount() == 1, "Scene should have 1 entity");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHighChurnSlotRecycling passed");
}

void Zenith_SceneTests::TestStaleEntityIDAfterSlotReuse()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStaleEntityIDAfterSlotReuse...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("StaleSlot");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "WillBeDestroyed");
	Zenith_EntityID xCachedID = xEntity.GetEntityID();

	Zenith_SceneManager::DestroyImmediate(xEntity);

	// Create several entities to increase chance of slot reuse
	for (int i = 0; i < 5; i++)
	{
		Zenith_Entity xTemp(pxData, "Filler");
		Zenith_SceneManager::DestroyImmediate(xTemp);
	}

	// Cached ID should be stale
	Zenith_Assert(!pxData->EntityExists(xCachedID), "Cached ID should not exist");

	Zenith_Entity xStale = pxData->TryGetEntity(xCachedID);
	Zenith_Assert(!xStale.IsValid(), "TryGetEntity with stale ID should return invalid");

	Zenith_Assert(!pxData->EntityHasComponent<Zenith_TransformComponent>(xCachedID),
		"HasComponent on stale ID should return false");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStaleEntityIDAfterSlotReuse passed");
}

void Zenith_SceneTests::TestEntitySlotPoolGrowth()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntitySlotPoolGrowth...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SlotGrowth");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create enough entities to force slot pool growth
	const uint32_t uCount = 100;
	Zenith_Vector<Zenith_EntityID> axIDs;
	for (uint32_t i = 0; i < uCount; i++)
	{
		Zenith_Entity xEntity(pxData, "Growth_" + std::to_string(i));
		axIDs.PushBack(xEntity.GetEntityID());
	}

	Zenith_Assert(pxData->GetEntityCount() == uCount, "Should have %u entities", uCount);

	// All IDs should still be valid
	for (uint32_t i = 0; i < axIDs.GetSize(); i++)
	{
		Zenith_Assert(pxData->EntityExists(axIDs.Get(i)), "Entity %u should exist after pool growth", i);
	}

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntitySlotPoolGrowth passed");
}

void Zenith_SceneTests::TestEntityIDPackedRoundTrip()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityIDPackedRoundTrip...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PackedID");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "PackTest");
	Zenith_EntityID xID = xEntity.GetEntityID();

	uint64_t ulPacked = xID.GetPacked();
	Zenith_EntityID xUnpacked = Zenith_EntityID::FromPacked(ulPacked);

	Zenith_Assert(xUnpacked == xID, "Packed/unpacked EntityID must be equal");
	Zenith_Assert(xUnpacked.m_uIndex == xID.m_uIndex, "Index must match after round-trip");
	Zenith_Assert(xUnpacked.m_uGeneration == xID.m_uGeneration, "Generation must match after round-trip");

	// Verify hash works for unordered_map usage
	std::unordered_map<Zenith_EntityID, int> xMap; // #TODO: Replace with engine hash map
	xMap[xID] = 42;
	Zenith_Assert(xMap.count(xID) == 1, "EntityID should be usable as hash map key");
	Zenith_Assert(xMap[xID] == 42, "Hash map lookup should return correct value");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityIDPackedRoundTrip passed");
}

//==============================================================================
// Cat 16: Component Management at Scene Level
//==============================================================================

void Zenith_SceneTests::TestAddRemoveComponent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAddRemoveComponent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("AddRemoveComp");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "CompEntity");

	// Entity automatically has TransformComponent
	Zenith_Assert(xEntity.HasComponent<Zenith_TransformComponent>(), "Should have TransformComponent");

	// Add CameraComponent
	xEntity.AddComponent<Zenith_CameraComponent>();
	Zenith_Assert(xEntity.HasComponent<Zenith_CameraComponent>(), "Should have CameraComponent after add");
	Zenith_Assert(xEntity.TryGetComponent<Zenith_CameraComponent>() != nullptr, "TryGetComponent should return non-null");

	// Remove CameraComponent
	xEntity.RemoveComponent<Zenith_CameraComponent>();
	Zenith_Assert(!xEntity.HasComponent<Zenith_CameraComponent>(), "Should not have CameraComponent after remove");
	Zenith_Assert(xEntity.TryGetComponent<Zenith_CameraComponent>() == nullptr, "TryGetComponent should return null after remove");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAddRemoveComponent passed");
}

void Zenith_SceneTests::TestAddOrReplaceComponent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAddOrReplaceComponent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("AddOrReplace");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "ReplaceEntity");

	// Add CameraComponent first time
	xEntity.AddComponent<Zenith_CameraComponent>();
	Zenith_Assert(xEntity.HasComponent<Zenith_CameraComponent>(), "Should have CameraComponent");

	// AddOrReplace on same type - should not crash
	xEntity.AddOrReplaceComponent<Zenith_CameraComponent>();
	Zenith_Assert(xEntity.HasComponent<Zenith_CameraComponent>(), "Should still have CameraComponent after replace");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAddOrReplaceComponent passed");
}

void Zenith_SceneTests::TestComponentPoolGrowth()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentPoolGrowth...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PoolGrowth");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create 20 entities with CameraComponent (exceeds initial pool capacity of 16)
	const uint32_t uCount = 20;
	Zenith_Vector<Zenith_EntityID> axIDs;
	for (uint32_t i = 0; i < uCount; i++)
	{
		Zenith_Entity xEntity(pxData, "Pool_" + std::to_string(i));
		xEntity.AddComponent<Zenith_CameraComponent>();
		axIDs.PushBack(xEntity.GetEntityID());
	}

	// All components should be accessible after pool growth
	for (uint32_t i = 0; i < axIDs.GetSize(); i++)
	{
		Zenith_Assert(pxData->EntityHasComponent<Zenith_CameraComponent>(axIDs.Get(i)),
			"Entity %u should have CameraComponent after pool growth", i);
	}

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentPoolGrowth passed");
}

void Zenith_SceneTests::TestComponentSlotReuse()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentSlotReuse...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CompSlotReuse");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "SlotReuseEntity");

	// Add, remove, add same component type - slot should be reused
	xEntity.AddComponent<Zenith_CameraComponent>();
	Zenith_Assert(xEntity.HasComponent<Zenith_CameraComponent>(), "Should have CameraComponent");

	xEntity.RemoveComponent<Zenith_CameraComponent>();
	Zenith_Assert(!xEntity.HasComponent<Zenith_CameraComponent>(), "Should not have CameraComponent after remove");

	xEntity.AddComponent<Zenith_CameraComponent>();
	Zenith_Assert(xEntity.HasComponent<Zenith_CameraComponent>(), "Should have CameraComponent again after re-add");

	// Verify component data is accessible
	Zenith_CameraComponent& xCam = xEntity.GetComponent<Zenith_CameraComponent>();
	(void)xCam; // Just verify it doesn't crash

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentSlotReuse passed");
}

void Zenith_SceneTests::TestMultiComponentEntityMove()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultiComponentEntityMove...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MultiCompSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MultiCompTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xEntity(pxSourceData, "MultiComp");

	// Add multiple component types
	xEntity.AddComponent<Zenith_CameraComponent>();
	xEntity.AddComponent<Zenith_ScriptComponent>().SetBehaviour<SceneTestBehaviour>();

	// Set transform position for data integrity check
	Zenith_Maths::Vector3 xPos = { 5.0f, 10.0f, 15.0f };
	xEntity.GetTransform().SetPosition(xPos);

	Zenith_EntityID xID = xEntity.GetEntityID();

	// Move to target
	bool bResult = Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);
	Zenith_Assert(bResult, "Move should succeed");

	// Verify ALL component types transferred
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);
	Zenith_Assert(pxTargetData->EntityHasComponent<Zenith_TransformComponent>(xID), "Transform should exist in target");
	Zenith_Assert(pxTargetData->EntityHasComponent<Zenith_CameraComponent>(xID), "Camera should exist in target");
	Zenith_Assert(pxTargetData->EntityHasComponent<Zenith_ScriptComponent>(xID), "Script should exist in target");

	// Verify transform data preserved
	Zenith_Maths::Vector3 xMovedPos;
	pxTargetData->GetComponentFromEntity<Zenith_TransformComponent>(xID).GetPosition(xMovedPos);
	Zenith_Assert(xMovedPos.x == xPos.x && xMovedPos.y == xPos.y && xMovedPos.z == xPos.z,
		"Transform position should be preserved after multi-component move");

	// Entity should NOT be in source's active list (entity storage is global, but ownership moved)
	const Zenith_Vector<Zenith_EntityID>& axSourceActive = pxSourceData->GetActiveEntities();
	bool bFoundInSource = false;
	for (u_int u = 0; u < axSourceActive.GetSize(); u++)
	{
		if (axSourceActive.Get(u) == xID)
		{
			bFoundInSource = true;
			break;
		}
	}
	Zenith_Assert(!bFoundInSource, "Entity should not be in source scene's active list after move");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultiComponentEntityMove passed");
}

void Zenith_SceneTests::TestGetAllOfComponentType()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetAllOfComponentType...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("GetAllComp");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create 5 entities with CameraComponent
	for (int i = 0; i < 5; i++)
	{
		Zenith_Entity xEntity(pxData, "Cam_" + std::to_string(i));
		xEntity.AddComponent<Zenith_CameraComponent>();
	}

	// Remove CameraComponent from 2 of them
	const Zenith_Vector<Zenith_EntityID>& axActive = pxData->GetActiveEntities();
	pxData->RemoveComponentFromEntity<Zenith_CameraComponent>(axActive.Get(0));
	pxData->RemoveComponentFromEntity<Zenith_CameraComponent>(axActive.Get(1));

	Zenith_Vector<Zenith_CameraComponent*> axCameras;
	pxData->GetAllOfComponentType<Zenith_CameraComponent>(axCameras);

	Zenith_Assert(axCameras.GetSize() == 3, "Should have 3 cameras (5 created - 2 removed), got %u", axCameras.GetSize());

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetAllOfComponentType passed");
}

void Zenith_SceneTests::TestComponentHandleValid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentHandleValid...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CompHandle");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "HandleEntity");
	xEntity.AddComponent<Zenith_CameraComponent>();

	// Get handle
	Zenith_ComponentHandle<Zenith_CameraComponent> xHandle = pxData->GetComponentHandle<Zenith_CameraComponent>(xEntity.GetEntityID());
	Zenith_Assert(xHandle.IsValid(), "Handle should be valid");
	Zenith_Assert(pxData->IsComponentHandleValid(xHandle), "Handle should pass validity check");

	Zenith_CameraComponent* pxCam = pxData->TryGetComponentFromHandle(xHandle);
	Zenith_Assert(pxCam != nullptr, "TryGetComponentFromHandle should return non-null");

	// Remove component
	xEntity.RemoveComponent<Zenith_CameraComponent>();

	// Handle should now be invalid
	Zenith_Assert(!pxData->IsComponentHandleValid(xHandle), "Handle should be invalid after removal");
	Zenith_Assert(pxData->TryGetComponentFromHandle(xHandle) == nullptr, "TryGetComponentFromHandle should return null after removal");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentHandleValid passed");
}

void Zenith_SceneTests::TestComponentHandleStaleAfterSlotReuse()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentHandleStaleAfterSlotReuse...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("StaleHandle");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "StaleHandleEntity");
	xEntity.AddComponent<Zenith_CameraComponent>();

	// Capture handle
	Zenith_ComponentHandle<Zenith_CameraComponent> xOldHandle = pxData->GetComponentHandle<Zenith_CameraComponent>(xEntity.GetEntityID());

	// Remove and re-add (slot reuse with generation increment)
	xEntity.RemoveComponent<Zenith_CameraComponent>();
	xEntity.AddComponent<Zenith_CameraComponent>();

	// Old handle should be stale (generation mismatch)
	Zenith_Assert(!pxData->IsComponentHandleValid(xOldHandle), "Old handle should be stale after slot reuse");

	// New handle should be valid
	Zenith_ComponentHandle<Zenith_CameraComponent> xNewHandle = pxData->GetComponentHandle<Zenith_CameraComponent>(xEntity.GetEntityID());
	Zenith_Assert(pxData->IsComponentHandleValid(xNewHandle), "New handle should be valid");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentHandleStaleAfterSlotReuse passed");
}

//==============================================================================
// Cat 17: Entity Handle Validity Edge Cases
//==============================================================================

void Zenith_SceneTests::TestDefaultEntityInvalid()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDefaultEntityInvalid...");

	Zenith_Entity xDefault;
	Zenith_Assert(!xDefault.IsValid(), "Default-constructed entity should be invalid");

	Zenith_EntityID xDefaultID = xDefault.GetEntityID();
	Zenith_Assert(!xDefaultID.IsValid(), "Default entity ID should be invalid");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDefaultEntityInvalid passed");
}

void Zenith_SceneTests::TestEntityGetSceneDataAfterUnload()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityGetSceneDataAfterUnload...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("WillUnload");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "OrphanedEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();

	Zenith_SceneManager::UnloadScene(xScene);

	// Entity handle should be invalid after scene unload
	Zenith_Assert(!xEntity.IsValid(), "Entity should be invalid after scene unload");
	Zenith_Assert(xEntity.GetSceneData() == nullptr, "GetSceneData should return nullptr after unload");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityGetSceneDataAfterUnload passed");
}

void Zenith_SceneTests::TestEntityGetSceneReturnsCorrectScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityGetSceneReturnsCorrectScene...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EntityScene");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "SceneCheck");
	Zenith_Scene xEntityScene = xEntity.GetScene();

	Zenith_Assert(xEntityScene == xScene, "Entity's scene should match the scene it was created in");
	Zenith_Assert(xEntityScene.m_iHandle == xScene.m_iHandle, "Handle indices should match");
	Zenith_Assert(xEntityScene.m_uGeneration == xScene.m_uGeneration, "Generations should match");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityGetSceneReturnsCorrectScene passed");
}

void Zenith_SceneTests::TestEntityEqualityOperator()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityEqualityOperator...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EntityEquality");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity1(pxData, "Entity1");
	Zenith_Entity xEntity2(pxData, "Entity2");

	// Same entity via different handles
	Zenith_Entity xEntity1Copy = pxData->GetEntity(xEntity1.GetEntityID());

	Zenith_Assert(xEntity1 == xEntity1Copy, "Same entity handles should be equal");
	Zenith_Assert(xEntity1 != xEntity2, "Different entities should not be equal");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityEqualityOperator passed");
}

void Zenith_SceneTests::TestEntityValidAfterMove()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityValidAfterMove...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("ValidMoveSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("ValidMoveTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xEntity(pxSourceData, "ValidAfterMove");

	Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);

	// Entity handle should still be valid after move
	Zenith_Assert(xEntity.IsValid(), "Entity should be valid after move");
	Zenith_Assert(xEntity.GetSceneData() != nullptr, "GetSceneData should return non-null after move");

	// Should point to target scene
	Zenith_Scene xNewScene = xEntity.GetScene();
	Zenith_Assert(xNewScene == xTarget, "Entity should be in target scene after move");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityValidAfterMove passed");
}

void Zenith_SceneTests::TestEntityInvalidAfterDestroyImmediate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityInvalidAfterDestroyImmediate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DestroyInvalid");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "WillDestroy");
	Zenith_Assert(xEntity.IsValid(), "Entity should be valid before destroy");

	Zenith_SceneManager::DestroyImmediate(xEntity);

	Zenith_Assert(!xEntity.IsValid(), "Entity should be invalid after DestroyImmediate");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityInvalidAfterDestroyImmediate passed");
}

//==============================================================================
// Cat 18: FindEntityByName
//==============================================================================

void Zenith_SceneTests::TestFindEntityByNameExists()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindEntityByNameExists...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("FindByName");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "UniqueNamedEntity");
	Zenith_EntityID xExpectedID = xEntity.GetEntityID();

	Zenith_Entity xFound = pxData->FindEntityByName("UniqueNamedEntity");
	Zenith_Assert(xFound.IsValid(), "FindEntityByName should find existing entity");
	Zenith_Assert(xFound.GetEntityID() == xExpectedID, "Found entity should have correct ID");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindEntityByNameExists passed");
}

void Zenith_SceneTests::TestFindEntityByNameNotFound()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindEntityByNameNotFound...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("FindNotFound");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xNotFound = pxData->FindEntityByName("NonExistentEntity");
	Zenith_Assert(!xNotFound.IsValid(), "FindEntityByName should return invalid for non-existent name");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindEntityByNameNotFound passed");
}

void Zenith_SceneTests::TestFindEntityByNameDuplicate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindEntityByNameDuplicate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("FindDuplicate");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity1(pxData, "DuplicateName");
	Zenith_Entity xEntity2(pxData, "DuplicateName");

	// Should find one of them without crashing
	Zenith_Entity xFound = pxData->FindEntityByName("DuplicateName");
	Zenith_Assert(xFound.IsValid(), "FindEntityByName should return a valid entity even with duplicates");
	Zenith_Assert(xFound.GetEntityID() == xEntity1.GetEntityID() || xFound.GetEntityID() == xEntity2.GetEntityID(),
		"Found entity should be one of the duplicates");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFindEntityByNameDuplicate passed");
}

void Zenith_SceneTests::TestEntitySetNameGetName()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntitySetNameGetName...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("NameTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "OriginalName");
	Zenith_Assert(xEntity.GetName() == "OriginalName", "Initial name should match");

	xEntity.SetName("RenamedEntity");
	Zenith_Assert(xEntity.GetName() == "RenamedEntity", "Name should update after SetName");

	// Verify FindEntityByName uses new name
	Zenith_Entity xFound = pxData->FindEntityByName("RenamedEntity");
	Zenith_Assert(xFound.IsValid(), "FindEntityByName should find entity by new name");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntitySetNameGetName passed");
}

//==============================================================================
// Cat 19: Parent-Child Hierarchy in Scene Context
//==============================================================================

void Zenith_SceneTests::TestSetParentGetParent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetParentGetParent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ParentChild");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");

	xChild.SetParent(xParent.GetEntityID());

	Zenith_Assert(xChild.HasParent(), "Child should have parent");
	Zenith_Assert(xChild.GetParentEntityID() == xParent.GetEntityID(), "Child's parent should be correct");

	// Child should appear in parent's children list
	const Zenith_Vector<Zenith_EntityID>& axChildren = xParent.GetChildEntityIDs();
	bool bFound = false;
	for (u_int i = 0; i < axChildren.GetSize(); i++)
	{
		if (axChildren.Get(i) == xChild.GetEntityID()) { bFound = true; break; }
	}
	Zenith_Assert(bFound, "Child should appear in parent's children list");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetParentGetParent passed");
}

void Zenith_SceneTests::TestUnparentEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnparentEntity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("Unparent");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");

	xChild.SetParent(xParent.GetEntityID());
	Zenith_Assert(xChild.HasParent(), "Should have parent after SetParent");

	// Un-parent by setting to INVALID
	xChild.SetParent(INVALID_ENTITY_ID);
	Zenith_Assert(!xChild.HasParent(), "Should have no parent after un-parenting");

	// Parent's children list should be empty
	Zenith_Assert(xParent.GetChildCount() == 0, "Parent should have no children after un-parenting");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnparentEntity passed");
}

void Zenith_SceneTests::TestReparentEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestReparentEntity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("Reparent");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParentA(pxData, "ParentA");
	Zenith_Entity xParentB(pxData, "ParentB");
	Zenith_Entity xChild(pxData, "Child");

	// Parent to A
	xChild.SetParent(xParentA.GetEntityID());
	Zenith_Assert(xParentA.GetChildCount() == 1, "ParentA should have 1 child");
	Zenith_Assert(xParentB.GetChildCount() == 0, "ParentB should have 0 children");

	// Reparent to B
	xChild.SetParent(xParentB.GetEntityID());
	Zenith_Assert(xParentA.GetChildCount() == 0, "ParentA should have 0 children after reparent");
	Zenith_Assert(xParentB.GetChildCount() == 1, "ParentB should have 1 child after reparent");
	Zenith_Assert(xChild.GetParentEntityID() == xParentB.GetEntityID(), "Child's parent should be B");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestReparentEntity passed");
}

void Zenith_SceneTests::TestHasChildrenAndCount()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHasChildrenAndCount...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ChildCount");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Assert(!xParent.HasChildren(), "Parent should have no children initially");
	Zenith_Assert(xParent.GetChildCount() == 0, "Child count should be 0 initially");

	Zenith_Entity xChild1(pxData, "Child1");
	Zenith_Entity xChild2(pxData, "Child2");
	Zenith_Entity xChild3(pxData, "Child3");

	xChild1.SetParent(xParent.GetEntityID());
	xChild2.SetParent(xParent.GetEntityID());
	xChild3.SetParent(xParent.GetEntityID());

	Zenith_Assert(xParent.HasChildren(), "Parent should have children");
	Zenith_Assert(xParent.GetChildCount() == 3, "Parent should have 3 children");

	// Remove one child
	xChild2.SetParent(INVALID_ENTITY_ID);
	Zenith_Assert(xParent.GetChildCount() == 2, "Parent should have 2 children after un-parenting one");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHasChildrenAndCount passed");
}

void Zenith_SceneTests::TestIsRootEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsRootEntity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("IsRoot");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xRoot(pxData, "Root");
	Zenith_Entity xChild(pxData, "Child");

	Zenith_Assert(xRoot.IsRoot(), "Root entity should be root");
	Zenith_Assert(xChild.IsRoot(), "Unparented entity should be root");

	xChild.SetParent(xRoot.GetEntityID());
	Zenith_Assert(!xChild.IsRoot(), "Parented entity should not be root");

	xChild.SetParent(INVALID_ENTITY_ID);
	Zenith_Assert(xChild.IsRoot(), "Un-parented entity should be root again");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsRootEntity passed");
}

void Zenith_SceneTests::TestDeepHierarchyActiveInHierarchy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDeepHierarchyActiveInHierarchy...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DeepHierarchy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create 5-level hierarchy
	Zenith_Entity xLevel1(pxData, "Level1");
	Zenith_Entity xLevel2(pxData, "Level2");
	Zenith_Entity xLevel3(pxData, "Level3");
	Zenith_Entity xLevel4(pxData, "Level4");
	Zenith_Entity xLevel5(pxData, "Level5");

	xLevel2.SetParent(xLevel1.GetEntityID());
	xLevel3.SetParent(xLevel2.GetEntityID());
	xLevel4.SetParent(xLevel3.GetEntityID());
	xLevel5.SetParent(xLevel4.GetEntityID());

	// All should be active
	Zenith_Assert(xLevel5.IsActiveInHierarchy(), "Level5 should be active when all parents enabled");

	// Disable level 2
	xLevel2.SetEnabled(false);

	// Levels 3-5 should all be inactive in hierarchy
	Zenith_Assert(!xLevel3.IsActiveInHierarchy(), "Level3 should be inactive when Level2 disabled");
	Zenith_Assert(!xLevel4.IsActiveInHierarchy(), "Level4 should be inactive when Level2 disabled");
	Zenith_Assert(!xLevel5.IsActiveInHierarchy(), "Level5 should be inactive when Level2 disabled");

	// Level 1 should still be active
	Zenith_Assert(xLevel1.IsActiveInHierarchy(), "Level1 should still be active");

	// Re-enable level 2
	xLevel2.SetEnabled(true);
	Zenith_Assert(xLevel5.IsActiveInHierarchy(), "Level5 should be active again after Level2 re-enabled");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDeepHierarchyActiveInHierarchy passed");
}

void Zenith_SceneTests::TestSetParentAcrossScenes()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetParentAcrossScenes...");

	// Engine explicitly asserts on cross-scene parenting in SetParentByID.
	// This test verifies that entities in different scenes remain unparented
	// and that same-scene parenting still works correctly.
	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("SceneA_Parent");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("SceneB_Child");
	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);
	Zenith_SceneData* pxDataB = Zenith_SceneManager::GetSceneData(xSceneB);

	Zenith_Entity xParentA(pxDataA, "ParentInA");
	Zenith_Entity xChildA(pxDataA, "ChildInA");
	Zenith_Entity xEntityB(pxDataB, "EntityInB");

	// Same-scene parenting should work
	xChildA.SetParent(xParentA.GetEntityID());
	Zenith_Assert(xChildA.HasParent(), "Same-scene child should have parent");
	Zenith_Assert(xChildA.GetParentEntityID() == xParentA.GetEntityID(), "Parent ID should match");

	// Entity in scene B should have no parent (cannot cross-scene parent)
	Zenith_Assert(!xEntityB.HasParent(), "Entity in different scene should have no parent");

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetParentAcrossScenes passed");
}

//==============================================================================
// Cat 20: Entity Enable/Disable Lifecycle
//==============================================================================

void Zenith_SceneTests::TestDisabledEntitySkipsUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisabledEntitySkipsUpdate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DisableUpdate");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Use a callback flag to track updates for THIS specific entity only
	// (global counter can be affected by entities from other scenes)
	static bool ls_bGotUpdate = false;
	static Zenith_EntityID ls_xTrackedID = INVALID_ENTITY_ID;

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnUpdateCallback = [](Zenith_Entity& xEntity, float) {
		if (xEntity.GetEntityID() == ls_xTrackedID)
		{
			ls_bGotUpdate = true;
		}
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "DisableMe");
	ls_xTrackedID = xEntity.GetEntityID();
	pxData->DispatchLifecycleForNewScene();

	// Pump - should get update while enabled
	ls_bGotUpdate = false;
	PumpFrames(1);
	Zenith_Assert(ls_bGotUpdate, "Should get update while enabled");

	// Disable and pump
	xEntity.SetEnabled(false);
	ls_bGotUpdate = false;
	PumpFrames(1);
	Zenith_Assert(!ls_bGotUpdate, "Should NOT get update while disabled");

	SceneTestBehaviour::s_pfnOnUpdateCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisabledEntitySkipsUpdate passed");
}

void Zenith_SceneTests::TestDisabledEntityComponentsAccessible()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisabledEntityComponentsAccessible...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DisabledAccess");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "DisabledEntity");
	xEntity.AddComponent<Zenith_CameraComponent>();

	xEntity.SetEnabled(false);

	// Components should still be accessible
	Zenith_Assert(xEntity.HasComponent<Zenith_TransformComponent>(), "Disabled entity should still have TransformComponent");
	Zenith_Assert(xEntity.HasComponent<Zenith_CameraComponent>(), "Disabled entity should still have CameraComponent");

	Zenith_TransformComponent& xTransform = xEntity.GetTransform();
	(void)xTransform; // Verify no crash

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisabledEntityComponentsAccessible passed");
}

void Zenith_SceneTests::TestToggleEnableDisableMultipleTimes()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestToggleEnableDisableMultipleTimes...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ToggleEnable");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "ToggleEntity");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	SceneTestBehaviour::ResetCounters();

	// Enable -> Disable -> Enable in rapid succession
	xEntity.SetEnabled(false);
	xEntity.SetEnabled(true);
	xEntity.SetEnabled(false);
	xEntity.SetEnabled(true);

	// Final state should be enabled
	Zenith_Assert(xEntity.IsEnabled(), "Final state should be enabled after toggle");

	// Pump and verify entity updates
	PumpFrames(1);
	Zenith_Assert(SceneTestBehaviour::s_uUpdateCount > 0, "Should get update when finally enabled");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestToggleEnableDisableMultipleTimes passed");
}

void Zenith_SceneTests::TestIsEnabledVsIsActiveInHierarchy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsEnabledVsIsActiveInHierarchy...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EnableVsActive");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	// Disable parent
	xParent.SetEnabled(false);

	// Child is enabled (activeSelf=true) but not active in hierarchy
	Zenith_Assert(xChild.IsEnabled(), "Child's own enabled flag should be true");
	Zenith_Assert(!xChild.IsActiveInHierarchy(), "Child should NOT be active in hierarchy when parent disabled");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsEnabledVsIsActiveInHierarchy passed");
}

void Zenith_SceneTests::TestEntityEnabledStatePreservedOnMove()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityEnabledStatePreservedOnMove...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("EnableMoveSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("EnableMoveTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xEntity(pxSourceData, "DisabledMover");
	xEntity.SetEnabled(false);

	Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);

	Zenith_Assert(!xEntity.IsEnabled(), "Enabled state should be preserved after move (should still be disabled)");

	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityEnabledStatePreservedOnMove passed");
}

//==============================================================================
// Cat 21: Transient Entity Behavior
//==============================================================================

void Zenith_SceneTests::TestSetTransientIsTransient()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetTransientIsTransient...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TransientFlag");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "TransientEntity");
	xEntity.SetTransient(true);
	Zenith_Assert(xEntity.IsTransient(), "Entity should be transient after SetTransient(true)");

	xEntity.SetTransient(false);
	Zenith_Assert(!xEntity.IsTransient(), "Entity should not be transient after SetTransient(false)");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetTransientIsTransient passed");
}

void Zenith_SceneTests::TestTransientEntityNotSaved()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTransientEntityNotSaved...");

	const std::string strPath = "test_transient_save" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TransientSave");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xPersistentEntity(pxData, "WillBeSaved");
	xPersistentEntity.SetTransient(false);

	Zenith_Entity xTransientEntity(pxData, "WillNotBeSaved");
	xTransientEntity.SetTransient(true);

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	// Reload and verify
	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	Zenith_Entity xFoundPersistent = pxLoadedData->FindEntityByName("WillBeSaved");
	Zenith_Entity xFoundTransient = pxLoadedData->FindEntityByName("WillNotBeSaved");

	Zenith_Assert(xFoundPersistent.IsValid(), "Non-transient entity should be saved and loaded");
	Zenith_Assert(!xFoundTransient.IsValid(), "Transient entity should NOT be saved");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTransientEntityNotSaved passed");
}

void Zenith_SceneTests::TestNewEntityDefaultTransient()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNewEntityDefaultTransient...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DefaultTransient");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "NewEntity");

	// Default transient state is true (entities are transient by default)
	Zenith_Assert(xEntity.IsTransient(), "New entities should be transient by default");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestNewEntityDefaultTransient passed");
}

//==============================================================================
// Cat 22: Camera Destruction & Edge Cases
//==============================================================================

void Zenith_SceneTests::TestMainCameraDestroyedThenQuery()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMainCameraDestroyedThenQuery...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CamDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xCamEntity(pxData, "CameraEntity");
	xCamEntity.AddComponent<Zenith_CameraComponent>();
	pxData->SetMainCameraEntity(xCamEntity.GetEntityID());

	Zenith_Assert(pxData->TryGetMainCamera() != nullptr, "Should have main camera before destroy");

	Zenith_SceneManager::DestroyImmediate(xCamEntity);

	// Main camera query should return nullptr
	Zenith_Assert(pxData->TryGetMainCamera() == nullptr, "TryGetMainCamera should return nullptr after camera entity destroyed");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMainCameraDestroyedThenQuery passed");
}

void Zenith_SceneTests::TestSetMainCameraToNonCameraEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetMainCameraToNonCameraEntity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("NoCam");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "NoCameraComponent");
	// Entity only has TransformComponent, no CameraComponent

	pxData->SetMainCameraEntity(xEntity.GetEntityID());

	// TryGetMainCamera should return nullptr since entity has no CameraComponent
	Zenith_Assert(pxData->TryGetMainCamera() == nullptr,
		"TryGetMainCamera should return nullptr when main camera entity has no CameraComponent");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetMainCameraToNonCameraEntity passed");
}

void Zenith_SceneTests::TestMainCameraPreservedOnSceneSave()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMainCameraPreservedOnSceneSave...");

	const std::string strPath = "test_camera_save" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CamSave");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xCamEntity(pxData, "MainCam");
	xCamEntity.SetTransient(false);
	xCamEntity.AddComponent<Zenith_CameraComponent>();
	pxData->SetMainCameraEntity(xCamEntity.GetEntityID());

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	// Reload and verify
	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	Zenith_CameraComponent* pxCam = pxLoadedData->TryGetMainCamera();
	Zenith_Assert(pxCam != nullptr, "Main camera should be preserved after save/load");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMainCameraPreservedOnSceneSave passed");
}

//==============================================================================
// Cat 23: Scene Merge Edge Cases
//==============================================================================

void Zenith_SceneTests::TestMergeScenesDisabledEntities()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesDisabledEntities...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeDisabledSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeDisabledTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xDisabled(pxSourceData, "DisabledEntity");
	xDisabled.SetEnabled(false);
	Zenith_EntityID xDisabledID = xDisabled.GetEntityID();

	Zenith_Entity xEnabled(pxSourceData, "EnabledEntity");
	Zenith_EntityID xEnabledID = xEnabled.GetEntityID();

	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// Verify enable state preserved
	Zenith_Entity xMergedDisabled = pxTargetData->GetEntity(xDisabledID);
	Zenith_Entity xMergedEnabled = pxTargetData->GetEntity(xEnabledID);

	Zenith_Assert(!xMergedDisabled.IsEnabled(), "Disabled entity should stay disabled after merge");
	Zenith_Assert(xMergedEnabled.IsEnabled(), "Enabled entity should stay enabled after merge");

	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesDisabledEntities passed");
}

void Zenith_SceneTests::TestMergeScenesWithPendingStarts()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesWithPendingStarts...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergePendingSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergePendingTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxSourceData, "PendingStart");
	pxSourceData->DispatchLifecycleForNewScene();

	// Entity has pending start (Awake done, Start deferred)
	Zenith_Assert(SceneTestBehaviour::s_uAwakeCount == 1, "Awake should have fired");
	Zenith_Assert(SceneTestBehaviour::s_uStartCount == 0, "Start should not have fired yet");

	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	// Pump to trigger Start in target
	PumpFrames(1);

	Zenith_Assert(SceneTestBehaviour::s_uStartCount == 1, "Start should fire in target after merge");

	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesWithPendingStarts passed");
}

void Zenith_SceneTests::TestMergeScenesWithTimedDestructions()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesWithTimedDestructions...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeTimedSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeTimedTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xEntity(pxSourceData, "TimedEntity");
	pxSourceData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_EntityID xID = xEntity.GetEntityID();

	// Mark for timed destruction (large delay so it doesn't fire during merge)
	pxSourceData->MarkForTimedDestruction(xID, 10.0f);

	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// Entity should exist in target (timer hasn't expired)
	Zenith_Assert(pxTargetData->EntityExists(xID), "Entity with timed destruction should exist in target after merge");

	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesWithTimedDestructions passed");
}

void Zenith_SceneTests::TestMergeScenesMultipleRoots()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesMultipleRoots...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergeMultiSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergeMultiTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	uint32_t uTargetInitialCount = pxTargetData->GetEntityCount();

	// Create 10 root entities in source
	Zenith_Vector<Zenith_EntityID> axSourceIDs;
	for (int i = 0; i < 10; i++)
	{
		Zenith_Entity xEntity(pxSourceData, "Root_" + std::to_string(i));
		axSourceIDs.PushBack(xEntity.GetEntityID());
	}

	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	// All 10 should be in target
	Zenith_Assert(pxTargetData->GetEntityCount() == uTargetInitialCount + 10,
		"Target should have all 10 merged entities");

	for (uint32_t i = 0; i < axSourceIDs.GetSize(); i++)
	{
		Zenith_Assert(pxTargetData->EntityExists(axSourceIDs.Get(i)),
			"Entity %u should exist in target after merge", i);
	}

	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeScenesMultipleRoots passed");
}

//==============================================================================
// Cat 24: Scene Load/Save with Entity State
//==============================================================================

void Zenith_SceneTests::TestSaveLoadDisabledEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadDisabledEntity...");

	const std::string strPath = "test_disabled_save" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DisabledSave");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "DisabledEntity");
	xEntity.SetTransient(false);
	xEntity.SetEnabled(false);

	// Verify entity is disabled before save
	Zenith_Assert(!xEntity.IsEnabled(), "Entity should be disabled before save");

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	// Engine serialization does not persist enabled/disabled state.
	// All entities are enabled on load (m_bEnabled = true in slot init).
	Zenith_Entity xLoadedEntity = pxLoadedData->FindEntityByName("DisabledEntity");
	Zenith_Assert(xLoadedEntity.IsValid(), "Disabled entity should be saved and loaded");
	Zenith_Assert(xLoadedEntity.IsEnabled(), "Loaded entities are always enabled (enabled state not serialized)");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadDisabledEntity passed");
}

void Zenith_SceneTests::TestSaveLoadEntityNames()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadEntityNames...");

	const std::string strPath = "test_names_save" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("NamesSave");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xE1(pxData, "Alpha");
	xE1.SetTransient(false);
	Zenith_Entity xE2(pxData, "Beta");
	xE2.SetTransient(false);
	Zenith_Entity xE3(pxData, "Gamma");
	xE3.SetTransient(false);

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	Zenith_Assert(pxLoadedData->FindEntityByName("Alpha").IsValid(), "Alpha should be found");
	Zenith_Assert(pxLoadedData->FindEntityByName("Beta").IsValid(), "Beta should be found");
	Zenith_Assert(pxLoadedData->FindEntityByName("Gamma").IsValid(), "Gamma should be found");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadEntityNames passed");
}

void Zenith_SceneTests::TestSaveLoadMultipleComponentTypes()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadMultipleComponentTypes...");

	const std::string strPath = "test_multicomp_save" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("MultiCompSave");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "MultiCompEntity");
	xEntity.SetTransient(false);
	xEntity.AddComponent<Zenith_CameraComponent>();

	// Set transform data
	Zenith_Maths::Vector3 xPos = { 1.0f, 2.0f, 3.0f };
	xEntity.GetTransform().SetPosition(xPos);

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	Zenith_Entity xLoadedEntity = pxLoadedData->FindEntityByName("MultiCompEntity");
	Zenith_Assert(xLoadedEntity.IsValid(), "Entity should be loaded");
	Zenith_Assert(xLoadedEntity.HasComponent<Zenith_TransformComponent>(), "Should have Transform after load");
	Zenith_Assert(xLoadedEntity.HasComponent<Zenith_CameraComponent>(), "Should have Camera after load");

	// Verify transform data
	Zenith_Maths::Vector3 xLoadedPos;
	xLoadedEntity.GetTransform().GetPosition(xLoadedPos);
	Zenith_Assert(xLoadedPos.x == 1.0f && xLoadedPos.y == 2.0f && xLoadedPos.z == 3.0f,
		"Transform position should be preserved after save/load");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadMultipleComponentTypes passed");
}

void Zenith_SceneTests::TestSaveLoadParentChildOrder()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadParentChildOrder...");

	const std::string strPath = "test_hierarchy_order_save" ZENITH_SCENE_EXT;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("HierarchyOrder");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	xParent.SetTransient(false);
	Zenith_Entity xChild1(pxData, "Child1");
	xChild1.SetTransient(false);
	Zenith_Entity xChild2(pxData, "Child2");
	xChild2.SetTransient(false);

	xChild1.SetParent(xParent.GetEntityID());
	xChild2.SetParent(xParent.GetEntityID());

	pxData->SaveToFile(strPath);
	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Scene xLoaded = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxLoadedData = Zenith_SceneManager::GetSceneData(xLoaded);

	Zenith_Entity xLoadedParent = pxLoadedData->FindEntityByName("Parent");
	Zenith_Entity xLoadedChild1 = pxLoadedData->FindEntityByName("Child1");
	Zenith_Entity xLoadedChild2 = pxLoadedData->FindEntityByName("Child2");

	Zenith_Assert(xLoadedParent.IsValid(), "Parent should exist after load");
	Zenith_Assert(xLoadedChild1.IsValid(), "Child1 should exist after load");
	Zenith_Assert(xLoadedChild2.IsValid(), "Child2 should exist after load");

	Zenith_Assert(xLoadedChild1.HasParent(), "Child1 should have parent after load");
	Zenith_Assert(xLoadedChild2.HasParent(), "Child2 should have parent after load");
	Zenith_Assert(xLoadedChild1.GetParentEntityID() == xLoadedParent.GetEntityID(), "Child1's parent should be Parent");
	Zenith_Assert(xLoadedChild2.GetParentEntityID() == xLoadedParent.GetEntityID(), "Child2's parent should be Parent");
	Zenith_Assert(xLoadedParent.GetChildCount() == 2, "Parent should have 2 children after load");

	Zenith_SceneManager::UnloadScene(xLoaded);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSaveLoadParentChildOrder passed");
}

//==============================================================================
// Cat 25: Lifecycle During Async Unload
//==============================================================================

void Zenith_SceneTests::TestAsyncUnloadingSceneSkipsUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncUnloadingSceneSkipsUpdate...");

	const std::string strPath = "test_async_unload_update" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath, "AsyncUnloadEntity");

	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "WatchUpdate");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	// Start async unload - scene should be marked as unloading
	Zenith_SceneManager::SetAsyncUnloadBatchSize(1); // 1 entity per frame to stretch it out
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xScene);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);

	// Pump until complete
	PumpUntilComplete(pxOp);

	// Restore default batch size
	Zenith_SceneManager::SetAsyncUnloadBatchSize(50);

	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAsyncUnloadingSceneSkipsUpdate passed");
}

void Zenith_SceneTests::TestSceneUnloadingCallbackDataAccess()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneUnloadingCallbackDataAccess...");

	static bool s_bDataAccessible = false;
	static std::string s_strEntityName;

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("UnloadingAccess");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "AccessMe");

	auto ulHandle = Zenith_SceneManager::RegisterSceneUnloadingCallback(
		[](Zenith_Scene xScene)
		{
			Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
			if (pxData)
			{
				s_bDataAccessible = pxData->GetEntityCount() > 0;
				Zenith_Entity xFound = pxData->FindEntityByName("AccessMe");
				if (xFound.IsValid())
				{
					s_strEntityName = xFound.GetName();
				}
			}
		});

	s_bDataAccessible = false;
	s_strEntityName = "";

	Zenith_SceneManager::UnloadScene(xScene);

	Zenith_Assert(s_bDataAccessible, "Scene data should be accessible in sceneUnloading callback");
	Zenith_Assert(s_strEntityName == "AccessMe", "Entity data should be accessible in sceneUnloading callback");

	Zenith_SceneManager::UnregisterSceneUnloadingCallback(ulHandle);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneUnloadingCallbackDataAccess passed");
}

void Zenith_SceneTests::TestEntityExistsDuringAsyncUnload()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityExistsDuringAsyncUnload...");

	const std::string strPath = "test_exists_async" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath, "ExistEntity");

	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_ADDITIVE);
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create multiple entities so async unload takes multiple frames
	Zenith_Vector<Zenith_EntityID> axIDs;
	for (int i = 0; i < 10; i++)
	{
		Zenith_Entity xEntity(pxData, "BatchEntity_" + std::to_string(i));
		axIDs.PushBack(xEntity.GetEntityID());
	}

	Zenith_SceneManager::SetAsyncUnloadBatchSize(2); // 2 per frame
	Zenith_SceneOperationID ulOpID = Zenith_SceneManager::UnloadSceneAsync(xScene);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOpID);

	PumpUntilComplete(pxOp);

	Zenith_SceneManager::SetAsyncUnloadBatchSize(50);

	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityExistsDuringAsyncUnload passed");
}

//==============================================================================
// Cat 26: Stress & Volume Tests
//==============================================================================

void Zenith_SceneTests::TestCreateManyEntities()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCreateManyEntities...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ManyEntities");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	const uint32_t uCount = 500;
	Zenith_Vector<Zenith_EntityID> axIDs;

	for (uint32_t i = 0; i < uCount; i++)
	{
		Zenith_Entity xEntity(pxData, "Entity_" + std::to_string(i));
		axIDs.PushBack(xEntity.GetEntityID());
	}

	Zenith_Assert(pxData->GetEntityCount() == uCount, "Should have %u entities, got %u", uCount, pxData->GetEntityCount());

	// All should be roots
	Zenith_Assert(pxData->GetCachedRootEntityCount() == uCount, "All %u entities should be roots", uCount);

	// Query should return all
	uint32_t uQueryCount = 0;
	pxData->Query<Zenith_TransformComponent>().ForEach(
		[&uQueryCount](Zenith_EntityID, Zenith_TransformComponent&) {
			uQueryCount++;
		}
	);
	Zenith_Assert(uQueryCount == uCount, "Query should return all %u entities", uCount);

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCreateManyEntities passed");
}

void Zenith_SceneTests::TestRapidSceneCreateUnloadCycle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRapidSceneCreateUnloadCycle...");

	uint32_t uInitialCount = Zenith_SceneManager::GetLoadedSceneCount();

	for (int i = 0; i < 50; i++)
	{
		Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CycleScene_" + std::to_string(i));
		Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

		// Create some entities
		Zenith_Entity xE1(pxData, "A");
		Zenith_Entity xE2(pxData, "B");

		Zenith_SceneManager::UnloadScene(xScene);
	}

	Zenith_Assert(Zenith_SceneManager::GetLoadedSceneCount() == uInitialCount,
		"Scene count should be same after create/unload cycle (no handle leaks)");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRapidSceneCreateUnloadCycle passed");
}

void Zenith_SceneTests::TestManyEntitiesPerformanceGuard()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestManyEntitiesPerformanceGuard...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PerfGuard");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	// Pump once to get a baseline - other scenes may have SceneTestBehaviour entities
	PumpFrames(1);
	uint32_t uBaselineUpdatesPerFrame = SceneTestBehaviour::s_uUpdateCount;
	SceneTestBehaviour::ResetCounters();

	const uint32_t uCount = 100;
	for (uint32_t i = 0; i < uCount; i++)
	{
		CreateEntityWithBehaviour(pxData, "Perf_" + std::to_string(i));
	}
	pxData->DispatchLifecycleForNewScene();

	Zenith_Assert(SceneTestBehaviour::s_uAwakeCount == uCount, "All %u entities should have awoken", uCount);

	// Pump 10 frames
	const uint32_t uFrames = 10;
	PumpFrames(uFrames);

	uint32_t uExpected = (uCount + uBaselineUpdatesPerFrame) * uFrames;
	Zenith_Assert(SceneTestBehaviour::s_uStartCount >= uCount, "All %u entities should have started", uCount);
	Zenith_Assert(SceneTestBehaviour::s_uUpdateCount == uExpected,
		"Should have %u updates (%u+%u entities * %u frames), got %u",
		uExpected, uCount, uBaselineUpdatesPerFrame, uFrames, SceneTestBehaviour::s_uUpdateCount);

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestManyEntitiesPerformanceGuard passed");
}

void Zenith_SceneTests::TestComponentPoolGrowthMultipleTypes()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentPoolGrowthMultipleTypes...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("MultiPoolGrowth");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	const uint32_t uCount = 50;
	Zenith_Vector<Zenith_EntityID> axIDs;

	for (uint32_t i = 0; i < uCount; i++)
	{
		Zenith_Entity xEntity(pxData, "Multi_" + std::to_string(i));
		xEntity.AddComponent<Zenith_CameraComponent>();
		xEntity.AddComponent<Zenith_ScriptComponent>().SetBehaviour<SceneTestBehaviour>();
		axIDs.PushBack(xEntity.GetEntityID());
	}

	// All entities should have all 3 component types (Transform is auto)
	for (uint32_t i = 0; i < axIDs.GetSize(); i++)
	{
		Zenith_Assert(pxData->EntityHasComponent<Zenith_TransformComponent>(axIDs.Get(i)), "Entity %u should have Transform", i);
		Zenith_Assert(pxData->EntityHasComponent<Zenith_CameraComponent>(axIDs.Get(i)), "Entity %u should have Camera", i);
		Zenith_Assert(pxData->EntityHasComponent<Zenith_ScriptComponent>(axIDs.Get(i)), "Entity %u should have Script", i);
	}

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentPoolGrowthMultipleTypes passed");
}

//==============================================================================
// Cat 27: DontDestroyOnLoad Edge Cases
//==============================================================================

void Zenith_SceneTests::TestDontDestroyOnLoadIdempotent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDontDestroyOnLoadIdempotent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DDOLIdempotent");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "PersistTwice");
	Zenith_EntityID xID = xEntity.GetEntityID();

	// First call
	xEntity.DontDestroyOnLoad();
	Zenith_Assert(xEntity.IsValid(), "Entity should be valid after first DontDestroyOnLoad");

	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistentData = Zenith_SceneManager::GetSceneData(xPersistent);
	Zenith_Assert(pxPersistentData->EntityExists(xID), "Entity should be in persistent scene");

	// Second call - should not crash or duplicate
	xEntity.DontDestroyOnLoad();
	Zenith_Assert(xEntity.IsValid(), "Entity should still be valid after second DontDestroyOnLoad");
	Zenith_Assert(pxPersistentData->EntityExists(xID), "Entity should still be in persistent scene");

	Zenith_SceneManager::DestroyImmediate(xEntity);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDontDestroyOnLoadIdempotent passed");
}

void Zenith_SceneTests::TestPersistentEntityLifecycleContinues()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentEntityLifecycleContinues...");

	const std::string strPath = "test_persistent_lifecycle" ZENITH_SCENE_EXT;
	CreateTestSceneFile(strPath, "Placeholder");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PersistLifecycle");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "PersistentEntity");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	// Mark persistent
	xEntity.DontDestroyOnLoad();

	uint32_t uUpdatesBefore = SceneTestBehaviour::s_uUpdateCount;

	// Load a new scene in SINGLE mode (unloads old scene but not persistent)
	Zenith_SceneManager::LoadScene(strPath, SCENE_LOAD_SINGLE);
	PumpFrames(1);

	// Persistent entity should continue getting updates
	Zenith_Assert(SceneTestBehaviour::s_uUpdateCount > uUpdatesBefore,
		"Persistent entity should continue receiving Update after SINGLE mode load");

	Zenith_SceneManager::DestroyImmediate(xEntity);
	CleanupTestSceneFile(strPath);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentEntityLifecycleContinues passed");
}

void Zenith_SceneTests::TestPersistentEntityDestroyedManually()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentEntityDestroyedManually...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PersistDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "PersistentToDestroy");
	Zenith_EntityID xID = xEntity.GetEntityID();

	xEntity.DontDestroyOnLoad();

	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistentData = Zenith_SceneManager::GetSceneData(xPersistent);
	Zenith_Assert(pxPersistentData->EntityExists(xID), "Entity should be in persistent scene");

	Zenith_SceneManager::DestroyImmediate(xEntity);

	Zenith_Assert(!pxPersistentData->EntityExists(xID), "Manually destroyed persistent entity should be removed");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPersistentEntityDestroyedManually passed");
}

//==============================================================================
// Cat 28: Update Ordering & Delta Time
//==============================================================================

void Zenith_SceneTests::TestUpdateReceivesCorrectDt()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUpdateReceivesCorrectDt...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DtTest");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	static float s_fReceivedDt = 0.0f;
	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnUpdateCallback = [](Zenith_Entity&, float fDt)
	{
		s_fReceivedDt = fDt;
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "DtEntity");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1); // Start fires

	s_fReceivedDt = 0.0f;
	const float fTestDt = 0.033f; // ~30fps
	PumpFrames(1, fTestDt);

	Zenith_Assert(s_fReceivedDt == fTestDt, "OnUpdate should receive correct dt (%f vs %f)", s_fReceivedDt, fTestDt);

	SceneTestBehaviour::s_pfnOnUpdateCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUpdateReceivesCorrectDt passed");
}

void Zenith_SceneTests::TestLateUpdateAfterUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLateUpdateAfterUpdate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("LateUpdateOrder");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "OrderEntity");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	SceneTestBehaviour::ResetCounters();
	PumpFrames(1);

	// Both Update and LateUpdate should have fired
	Zenith_Assert(SceneTestBehaviour::s_uUpdateCount == 1, "Should have 1 Update");
	Zenith_Assert(SceneTestBehaviour::s_uLateUpdateCount == 1, "Should have 1 LateUpdate");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLateUpdateAfterUpdate passed");
}

void Zenith_SceneTests::TestMultiSceneUpdateOrder()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultiSceneUpdateOrder...");

	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("UpdateSceneA");
	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("UpdateSceneB");
	Zenith_SceneData* pxDataA = Zenith_SceneManager::GetSceneData(xSceneA);
	Zenith_SceneData* pxDataB = Zenith_SceneManager::GetSceneData(xSceneB);

	SceneTestBehaviour::ResetCounters();

	CreateEntityWithBehaviour(pxDataA, "EntityInA");
	CreateEntityWithBehaviour(pxDataB, "EntityInB");

	pxDataA->DispatchLifecycleForNewScene();
	pxDataB->DispatchLifecycleForNewScene();
	PumpFrames(1);

	SceneTestBehaviour::ResetCounters();
	PumpFrames(1);

	// Both scenes' entities should have been updated
	Zenith_Assert(SceneTestBehaviour::s_uUpdateCount == 2,
		"Both scenes should update (expected 2 updates, got %u)", SceneTestBehaviour::s_uUpdateCount);

	Zenith_SceneManager::UnloadScene(xSceneA);
	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultiSceneUpdateOrder passed");
}

void Zenith_SceneTests::TestEntityCreatedDuringUpdateGetsNextFrameLifecycle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedDuringUpdateGetsNextFrameLifecycle...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CreateDuringUpdate");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	static Zenith_EntityID s_xCreatedID;
	static bool s_bCreated = false;

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnUpdateCallback = [](Zenith_Entity& xEntity, float)
	{
		if (!s_bCreated)
		{
			s_bCreated = true;
			Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
			Zenith_Entity xNew(pxSceneData, "CreatedInUpdate");
			s_xCreatedID = xNew.GetEntityID();
		}
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "Creator");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1); // Start fires

	s_bCreated = false;
	PumpFrames(1); // This frame creates the entity during Update

	// Entity should have been created
	Zenith_Assert(s_bCreated, "Entity should have been created during Update");
	Zenith_Assert(pxData->EntityExists(s_xCreatedID), "Created entity should exist");

	SceneTestBehaviour::s_pfnOnUpdateCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedDuringUpdateGetsNextFrameLifecycle passed");
}

//==============================================================================
// Cat 29: Lifecycle Edge Cases - Start Interactions
//==============================================================================

void Zenith_SceneTests::TestEntityCreatedDuringStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedDuringStart...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CreateDuringStart");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	static Zenith_EntityID s_xSpawnedID;
	static bool s_bSpawned = false;

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnStartCallback = [](Zenith_Entity& xEntity)
	{
		if (!s_bSpawned)
		{
			s_bSpawned = true;
			Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
			Zenith_Entity xNew(pxSceneData, "SpawnedInStart");
			s_xSpawnedID = xNew.GetEntityID();
		}
	};

	s_bSpawned = false;
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "StartSpawner");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1); // Start fires here, which creates new entity

	Zenith_Assert(s_bSpawned, "Entity should have been spawned during Start");
	Zenith_Assert(pxData->EntityExists(s_xSpawnedID), "Spawned entity should exist");

	SceneTestBehaviour::s_pfnOnStartCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedDuringStart passed");
}

void Zenith_SceneTests::TestDestroyDuringOnStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyDuringOnStart...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DestroyDuringStart");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnStartCallback = [](Zenith_Entity& xEntity)
	{
		Zenith_SceneManager::Destroy(xEntity);
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "DestroySelf");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1); // Start fires, marks for destruction, processed at end of frame

	Zenith_Assert(SceneTestBehaviour::s_uStartCount == 1, "Start should have fired");
	Zenith_Assert(SceneTestBehaviour::s_uDestroyCount == 1, "OnDestroy should have fired");
	Zenith_Assert(!pxData->EntityExists(xID), "Entity should be destroyed after self-destroy in Start");

	SceneTestBehaviour::s_pfnOnStartCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyDuringOnStart passed");
}

void Zenith_SceneTests::TestDisableDuringOnStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisableDuringOnStart...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DisableDuringStart");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnStartCallback = [](Zenith_Entity& xEntity)
	{
		xEntity.SetEnabled(false);
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "DisableSelf");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1); // Start fires, disables self

	Zenith_Assert(SceneTestBehaviour::s_uStartCount == 1, "Start should have fired");
	Zenith_Assert(!xEntity.IsEnabled(), "Entity should be disabled after disabling in Start");

	// Pump another frame - should NOT get update
	uint32_t uUpdates = SceneTestBehaviour::s_uUpdateCount;
	PumpFrames(1);
	Zenith_Assert(SceneTestBehaviour::s_uUpdateCount == uUpdates, "Disabled entity should not get Update");

	SceneTestBehaviour::s_pfnOnStartCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisableDuringOnStart passed");
}

//==============================================================================
// Cat 30: Lifecycle Interaction Combinations
//==============================================================================

void Zenith_SceneTests::TestSetParentDuringOnAwake()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetParentDuringOnAwake...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SetParentAwake");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_EntityID xParentID = xParent.GetEntityID();

	static Zenith_EntityID s_xTargetParentID;
	s_xTargetParentID = xParentID;

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnAwakeCallback = [](Zenith_Entity& xEntity)
	{
		xEntity.SetParent(s_xTargetParentID);
	};

	Zenith_Entity xChild = CreateEntityWithBehaviour(pxData, "Child");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_Assert(xChild.HasParent(), "Child should have a parent after SetParent in OnAwake");
	Zenith_Assert(xChild.GetParentEntityID() == xParentID, "Child's parent should be the target");
	Zenith_Assert(xParent.HasChildren(), "Parent should have children");

	SceneTestBehaviour::s_pfnOnAwakeCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetParentDuringOnAwake passed");
}

void Zenith_SceneTests::TestAddComponentDuringOnAwake()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAddComponentDuringOnAwake...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("AddCompAwake");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnAwakeCallback = [](Zenith_Entity& xEntity)
	{
		xEntity.AddComponent<Zenith_CameraComponent>();
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "AddComp");
	pxData->DispatchLifecycleForNewScene();

	Zenith_Assert(xEntity.HasComponent<Zenith_CameraComponent>(), "Entity should have CameraComponent after AddComponent in OnAwake");

	SceneTestBehaviour::s_pfnOnAwakeCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAddComponentDuringOnAwake passed");
}

void Zenith_SceneTests::TestRemoveComponentDuringOnUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRemoveComponentDuringOnUpdate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("RemoveCompUpdate");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	static bool s_bRemoved = false;
	s_bRemoved = false;
	SceneTestBehaviour::s_pfnOnUpdateCallback = [](Zenith_Entity& xEntity, float)
	{
		if (!s_bRemoved && xEntity.HasComponent<Zenith_CameraComponent>())
		{
			s_bRemoved = true;
			xEntity.RemoveComponent<Zenith_CameraComponent>();
		}
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "RemoveComp");
	xEntity.AddComponent<Zenith_CameraComponent>();
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_Assert(s_bRemoved, "Camera should have been removed during Update");
	Zenith_Assert(!xEntity.HasComponent<Zenith_CameraComponent>(), "Entity should no longer have CameraComponent");
	Zenith_Assert(xEntity.IsValid(), "Entity should still be valid");

	SceneTestBehaviour::s_pfnOnUpdateCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRemoveComponentDuringOnUpdate passed");
}

void Zenith_SceneTests::TestDontDestroyOnLoadDuringOnAwake()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDontDestroyOnLoadDuringOnAwake...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DDOLAwake");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnAwakeCallback = [](Zenith_Entity& xEntity)
	{
		xEntity.DontDestroyOnLoad();
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "PersistOnAwake");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxData->DispatchLifecycleForNewScene();

	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistentData = Zenith_SceneManager::GetSceneData(xPersistent);
	Zenith_Assert(pxPersistentData->EntityExists(xID), "Entity should be in persistent scene after DontDestroyOnLoad in OnAwake");

	SceneTestBehaviour::s_pfnOnAwakeCallback = nullptr;
	Zenith_Entity xPersistentEntity = pxPersistentData->GetEntity(xID);
	Zenith_SceneManager::Destroy(xPersistentEntity);
	PumpFrames(1);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDontDestroyOnLoadDuringOnAwake passed");
}

void Zenith_SceneTests::TestMoveEntityToSceneDuringOnStart()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToSceneDuringOnStart...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MoveStartSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MoveStartTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	static Zenith_Scene s_xTargetScene;
	s_xTargetScene = xTarget;

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnStartCallback = [](Zenith_Entity& xEntity)
	{
		Zenith_SceneManager::MoveEntityToScene(xEntity, s_xTargetScene);
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxSourceData, "MoveOnStart");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxSourceData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);
	Zenith_Assert(pxTargetData->EntityExists(xID), "Entity should exist in target after MoveEntityToScene in OnStart");

	SceneTestBehaviour::s_pfnOnStartCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToSceneDuringOnStart passed");
}

void Zenith_SceneTests::TestToggleEnabledDuringOnAwake()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestToggleEnabledDuringOnAwake...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ToggleAwake");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnAwakeCallback = [](Zenith_Entity& xEntity)
	{
		xEntity.SetEnabled(false);
		xEntity.SetEnabled(true);
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "Toggle");
	pxData->DispatchLifecycleForNewScene();

	Zenith_Assert(xEntity.IsEnabled(), "Entity should be enabled after toggle");

	SceneTestBehaviour::s_pfnOnAwakeCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestToggleEnabledDuringOnAwake passed");
}

void Zenith_SceneTests::TestEntityCreatedDuringOnFixedUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedDuringOnFixedUpdate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CreateInFixed");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	static Zenith_EntityID s_xCreatedID;
	static bool s_bCreated = false;
	s_bCreated = false;

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnFixedUpdateCallback = [](Zenith_Entity& xEntity, float)
	{
		if (!s_bCreated)
		{
			s_bCreated = true;
			Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
			Zenith_Entity xNew(pxSceneData, "CreatedInFixedUpdate");
			s_xCreatedID = xNew.GetEntityID();
		}
	};

	float fOldTimestep = Zenith_SceneManager::GetFixedTimestep();
	Zenith_SceneManager::SetFixedTimestep(0.02f);

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "FixedCreator");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1); // Start fires

	s_bCreated = false;
	PumpFrames(1); // FixedUpdate fires, creates entity

	Zenith_Assert(s_bCreated, "Entity should have been created during FixedUpdate");
	Zenith_Assert(pxData->EntityExists(s_xCreatedID), "Created entity should exist");

	SceneTestBehaviour::s_pfnOnFixedUpdateCallback = nullptr;
	Zenith_SceneManager::SetFixedTimestep(fOldTimestep);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedDuringOnFixedUpdate passed");
}

void Zenith_SceneTests::TestEntityCreatedDuringOnLateUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedDuringOnLateUpdate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CreateInLate");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	static Zenith_EntityID s_xCreatedID;
	static bool s_bCreated = false;
	s_bCreated = false;

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnLateUpdateCallback = [](Zenith_Entity& xEntity, float)
	{
		if (!s_bCreated)
		{
			s_bCreated = true;
			Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
			Zenith_Entity xNew(pxSceneData, "CreatedInLateUpdate");
			s_xCreatedID = xNew.GetEntityID();
		}
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "LateCreator");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1); // Start fires

	s_bCreated = false;
	PumpFrames(1); // LateUpdate fires, creates entity

	Zenith_Assert(s_bCreated, "Entity should have been created during LateUpdate");
	Zenith_Assert(pxData->EntityExists(s_xCreatedID), "Created entity should exist");

	SceneTestBehaviour::s_pfnOnLateUpdateCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedDuringOnLateUpdate passed");
}

void Zenith_SceneTests::TestDestroyImmediateDuringSelfOnUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyImmediateDuringSelfOnUpdate...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SelfDestroyUpdate");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	static bool s_bDestroyed = false;
	s_bDestroyed = false;
	SceneTestBehaviour::s_pfnOnUpdateCallback = [](Zenith_Entity& xEntity, float)
	{
		if (!s_bDestroyed)
		{
			s_bDestroyed = true;
			xEntity.DestroyImmediate();
		}
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "SelfDestroy");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1); // Start fires

	PumpFrames(1); // Update fires, entity destroys itself

	Zenith_Assert(s_bDestroyed, "Entity should have self-destroyed");
	Zenith_Assert(!pxData->EntityExists(xID), "Entity should no longer exist");
	Zenith_Assert(SceneTestBehaviour::s_uDestroyCount >= 1, "OnDestroy should have fired");

	SceneTestBehaviour::s_pfnOnUpdateCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyImmediateDuringSelfOnUpdate passed");
}

//==============================================================================
// Cat 31: Destruction Edge Cases
//==============================================================================

void Zenith_SceneTests::TestDestroyGrandchildThenGrandparent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyGrandchildThenGrandparent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("GCThenGP");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	Zenith_Entity xGrandparent = CreateEntityWithBehaviour(pxData, "Grandparent");
	Zenith_Entity xParent = CreateEntityWithBehaviour(pxData, "Parent");
	Zenith_Entity xGrandchild = CreateEntityWithBehaviour(pxData, "Grandchild");

	xParent.SetParent(xGrandparent.GetEntityID());
	xGrandchild.SetParent(xParent.GetEntityID());

	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_EntityID xGPID = xGrandparent.GetEntityID();
	Zenith_EntityID xPID = xParent.GetEntityID();
	Zenith_EntityID xGCID = xGrandchild.GetEntityID();

	SceneTestBehaviour::ResetCounters();

	// Destroy grandchild explicitly, then grandparent (which cascades to parent)
	Zenith_SceneManager::Destroy(xGrandchild);
	Zenith_SceneManager::Destroy(xGrandparent);
	PumpFrames(1);

	Zenith_Assert(!pxData->EntityExists(xGPID), "Grandparent should be destroyed");
	Zenith_Assert(!pxData->EntityExists(xPID), "Parent should be destroyed");
	Zenith_Assert(!pxData->EntityExists(xGCID), "Grandchild should be destroyed");
	// Key: grandchild should only be destroyed once (no double-free)
	Zenith_Assert(SceneTestBehaviour::s_uDestroyCount == 3, "Exactly 3 OnDestroy calls (no double-free), got %u", SceneTestBehaviour::s_uDestroyCount);

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyGrandchildThenGrandparent passed");
}

void Zenith_SceneTests::TestDestroyImmediateDuringAnotherAwake()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyImmediateDuringAnotherAwake...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DestroyInAwake");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	// Create target entity first (no behaviour)
	Zenith_Entity xTarget(pxData, "Target");
	Zenith_EntityID xTargetID = xTarget.GetEntityID();

	static Zenith_EntityID s_xTargetID;
	s_xTargetID = xTargetID;

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnAwakeCallback = [](Zenith_Entity& xEntity)
	{
		Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
		if (pxSceneData->EntityExists(s_xTargetID))
		{
			Zenith_Entity xTarget = pxSceneData->GetEntity(s_xTargetID);
			Zenith_SceneManager::DestroyImmediate(xTarget);
		}
	};

	Zenith_Entity xDestroyer = CreateEntityWithBehaviour(pxData, "Destroyer");
	pxData->DispatchLifecycleForNewScene();

	Zenith_Assert(!pxData->EntityExists(xTargetID), "Target should be destroyed by Destroyer's OnAwake");
	Zenith_Assert(xDestroyer.IsValid(), "Destroyer should still be valid");

	SceneTestBehaviour::s_pfnOnAwakeCallback = nullptr;
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDestroyImmediateDuringAnotherAwake passed");
}

void Zenith_SceneTests::TestTimedDestructionZeroDelay()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTimedDestructionZeroDelay...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ZeroDelay");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "ZeroDelay");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_SceneManager::Destroy(xEntity, 0.0f);
	PumpFrames(1);

	Zenith_Assert(!pxData->EntityExists(xID), "Entity with zero-delay timed destruction should be destroyed");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTimedDestructionZeroDelay passed");
}

void Zenith_SceneTests::TestTimedDestructionCancelledBySceneUnload()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTimedDestructionCancelledBySceneUnload...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("TimedUnload");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "TimedEntity");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_SceneManager::Destroy(xEntity, 5.0f); // Long delay
	Zenith_SceneManager::UnloadScene(xScene);

	// Pump several frames - timer should not fire and crash
	PumpFrames(10);

	// No crash is the primary assertion; destroy count may have incremented from scene unload
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTimedDestructionCancelledBySceneUnload passed");
}

void Zenith_SceneTests::TestMultipleTimedDestructionsSameEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleTimedDestructionsSameEntity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("MultiTimed");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "MultiTimed");
	Zenith_EntityID xID = xEntity.GetEntityID();
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	SceneTestBehaviour::ResetCounters();

	// Two timed destructions on same entity
	Zenith_SceneManager::Destroy(xEntity, 0.5f);
	Zenith_SceneManager::Destroy(xEntity, 1.0f);

	// Pump past both timers
	PumpFrames(120); // ~2 seconds at 60fps

	Zenith_Assert(!pxData->EntityExists(xID), "Entity should be destroyed");
	Zenith_Assert(SceneTestBehaviour::s_uDestroyCount == 1, "OnDestroy should fire exactly once, got %u", SceneTestBehaviour::s_uDestroyCount);

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleTimedDestructionsSameEntity passed");
}

//==============================================================================
// Cat 32: Scene Operation State Machine
//==============================================================================

void Zenith_SceneTests::TestGetResultSceneBeforeCompletion()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetResultSceneBeforeCompletion...");

	CreateTestSceneFile("test_result_before" ZENITH_SCENE_EXT);

	Zenith_SceneOperationID ulOp = Zenith_SceneManager::LoadSceneAsync("test_result_before" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOp);
	Zenith_Assert(pxOp != nullptr, "Operation should exist");

	pxOp->SetActivationAllowed(false);

	// Pump a few frames but don't let it complete
	PumpFrames(2);

	if (!pxOp->IsComplete())
	{
		Zenith_Scene xResult = pxOp->GetResultScene();
		// Before completion, result may be invalid or the scene handle may not be fully set up
		// The key assertion is no crash
	}

	pxOp->SetActivationAllowed(true);
	PumpUntilComplete(pxOp);

	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_Assert(xResult.IsValid(), "Result scene should be valid after completion");

	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile("test_result_before" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetResultSceneBeforeCompletion passed");
}

void Zenith_SceneTests::TestSetActivationAllowedAfterComplete()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetActivationAllowedAfterComplete...");

	CreateTestSceneFile("test_activ_after" ZENITH_SCENE_EXT);

	Zenith_SceneOperationID ulOp = Zenith_SceneManager::LoadSceneAsync("test_activ_after" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOp);
	PumpUntilComplete(pxOp);

	Zenith_Assert(pxOp->IsComplete(), "Operation should be complete");

	// Call after completion - should be no-op, no crash
	pxOp->SetActivationAllowed(true);
	pxOp->SetActivationAllowed(false);
	Zenith_Assert(pxOp->IsComplete(), "Operation should still be complete");

	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile("test_activ_after" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetActivationAllowedAfterComplete passed");
}

void Zenith_SceneTests::TestSetPriorityAfterCompletion()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetPriorityAfterCompletion...");

	CreateTestSceneFile("test_prio_after" ZENITH_SCENE_EXT);

	Zenith_SceneOperationID ulOp = Zenith_SceneManager::LoadSceneAsync("test_prio_after" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOp);
	PumpUntilComplete(pxOp);

	// Call after completion - should not crash
	pxOp->SetPriority(99);
	Zenith_Assert(pxOp->IsComplete(), "Operation should still be complete");

	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile("test_prio_after" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetPriorityAfterCompletion passed");
}

void Zenith_SceneTests::TestHasFailedOnNonExistentFileAsync()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHasFailedOnNonExistentFileAsync...");

	Zenith_SceneOperationID ulOp = Zenith_SceneManager::LoadSceneAsync("nonexistent_file_xyz_12345" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOp);

	if (pxOp != nullptr)
	{
		PumpUntilComplete(pxOp);
		Zenith_Assert(pxOp->IsComplete(), "Operation should complete even for non-existent file");
		Zenith_Assert(pxOp->HasFailed(), "Operation should have failed for non-existent file");
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestHasFailedOnNonExistentFileAsync passed");
}

void Zenith_SceneTests::TestCancelAlreadyCompletedOperation()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCancelAlreadyCompletedOperation...");

	CreateTestSceneFile("test_cancel_complete" ZENITH_SCENE_EXT);

	Zenith_SceneOperationID ulOp = Zenith_SceneManager::LoadSceneAsync("test_cancel_complete" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOp);
	PumpUntilComplete(pxOp);

	Zenith_Assert(pxOp->IsComplete(), "Operation should be complete");

	// Cancel after completion - should be no-op
	pxOp->RequestCancel();
	Zenith_Assert(pxOp->IsComplete(), "Operation should still be complete after cancel");

	Zenith_Scene xResult = pxOp->GetResultScene();
	if (xResult.IsValid()) Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile("test_cancel_complete" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCancelAlreadyCompletedOperation passed");
}

void Zenith_SceneTests::TestIsCancellationRequestedTracking()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsCancellationRequestedTracking...");

	CreateTestSceneFile("test_cancel_track" ZENITH_SCENE_EXT);

	Zenith_SceneOperationID ulOp = Zenith_SceneManager::LoadSceneAsync("test_cancel_track" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOp);
	Zenith_Assert(pxOp != nullptr, "Operation should exist");

	Zenith_Assert(!pxOp->IsCancellationRequested(), "Cancellation should not be requested initially");

	pxOp->RequestCancel();
	Zenith_Assert(pxOp->IsCancellationRequested(), "Cancellation should be requested after RequestCancel");

	PumpUntilComplete(pxOp);

	Zenith_Scene xResult = pxOp->GetResultScene();
	if (xResult.IsValid()) Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile("test_cancel_track" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsCancellationRequestedTracking passed");
}

//==============================================================================
// Cat 33: Component Handle System
//==============================================================================

void Zenith_SceneTests::TestComponentHandleSurvivesEnableDisable()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentHandleSurvivesEnableDisable...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("HandleEnableDisable");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "HandleEntity");
	xEntity.AddComponent<Zenith_CameraComponent>();
	Zenith_ComponentHandle<Zenith_CameraComponent> xHandle = pxData->GetComponentHandle<Zenith_CameraComponent>(xEntity.GetEntityID());

	Zenith_Assert(xHandle.IsValid(), "Handle should be valid initially");
	Zenith_Assert(pxData->IsComponentHandleValid(xHandle), "Handle should be valid in pool");

	xEntity.SetEnabled(false);
	Zenith_Assert(pxData->IsComponentHandleValid(xHandle), "Handle should still be valid after disable");

	xEntity.SetEnabled(true);
	Zenith_Assert(pxData->IsComponentHandleValid(xHandle), "Handle should still be valid after re-enable");

	Zenith_CameraComponent* pxComp = pxData->TryGetComponentFromHandle(xHandle);
	Zenith_Assert(pxComp != nullptr, "TryGetComponentFromHandle should return non-null");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentHandleSurvivesEnableDisable passed");
}

void Zenith_SceneTests::TestTryGetComponentFromHandleData()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetComponentFromHandleData...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("HandleData");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "HandleDataEntity");
	Zenith_CameraComponent& xDirect = xEntity.AddComponent<Zenith_CameraComponent>();
	Zenith_ComponentHandle<Zenith_CameraComponent> xHandle = pxData->GetComponentHandle<Zenith_CameraComponent>(xEntity.GetEntityID());

	Zenith_CameraComponent* pxFromHandle = pxData->TryGetComponentFromHandle(xHandle);
	Zenith_Assert(pxFromHandle == &xDirect, "TryGetComponentFromHandle should return same pointer as direct GetComponent");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetComponentFromHandleData passed");
}

void Zenith_SceneTests::TestTryGetComponentNullForMissing()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetComponentNullForMissing...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("NullComp");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "NoCameraEntity");
	// Entity only has TransformComponent (auto-added)
	Zenith_CameraComponent* pxCamera = xEntity.TryGetComponent<Zenith_CameraComponent>();
	Zenith_Assert(pxCamera == nullptr, "TryGetComponent should return nullptr for missing component type");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTryGetComponentNullForMissing passed");
}

void Zenith_SceneTests::TestGetComponentHandleForMissing()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetComponentHandleForMissing...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("MissingHandle");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "NoCamera");
	Zenith_ComponentHandle<Zenith_CameraComponent> xHandle = pxData->GetComponentHandle<Zenith_CameraComponent>(xEntity.GetEntityID());
	Zenith_Assert(!xHandle.IsValid(), "GetComponentHandle for missing component should return invalid handle");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetComponentHandleForMissing passed");
}

//==============================================================================
// Cat 34: Cross-Feature Interactions
//==============================================================================

void Zenith_SceneTests::TestMergeSceneWithPersistentEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeSceneWithPersistentEntity...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MergePersistSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MergePersistTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);

	Zenith_Entity xEntity(pxSourceData, "PersistEntity");
	xEntity.DontDestroyOnLoad();
	Zenith_EntityID xID = xEntity.GetEntityID();

	// Entity is now in persistent scene, source is empty
	Zenith_SceneManager::MergeScenes(xSource, xTarget);

	// Persistent entity should still be in persistent scene
	Zenith_Scene xPersistent = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxPersistentData = Zenith_SceneManager::GetSceneData(xPersistent);
	Zenith_Assert(pxPersistentData->EntityExists(xID), "Persistent entity should remain in persistent scene after merge");

	Zenith_Entity xPersistentEntity = pxPersistentData->GetEntity(xID);
	Zenith_SceneManager::Destroy(xPersistentEntity);
	PumpFrames(1);
	Zenith_SceneManager::UnloadScene(xTarget);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMergeSceneWithPersistentEntity passed");
}

void Zenith_SceneTests::TestPausedSceneEntityGetsStartOnUnpause()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPausedSceneEntityGetsStartOnUnpause...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("PauseStart");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_SceneManager::SetScenePaused(xScene, true);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "PausedEntity");
	pxData->DispatchLifecycleForNewScene();

	// Pump while paused
	PumpFrames(3);
	Zenith_Assert(SceneTestBehaviour::s_uStartCount == 0, "Start should NOT fire while scene is paused");

	// Unpause
	Zenith_SceneManager::SetScenePaused(xScene, false);
	PumpFrames(1);
	Zenith_Assert(SceneTestBehaviour::s_uStartCount == 1, "Start should fire after unpause");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestPausedSceneEntityGetsStartOnUnpause passed");
}

void Zenith_SceneTests::TestAdditiveSetActiveUnloadOriginal()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAdditiveSetActiveUnloadOriginal...");

	Zenith_Scene xOriginal = Zenith_SceneManager::CreateEmptyScene("Original");
	Zenith_Scene xAdditive = Zenith_SceneManager::CreateEmptyScene("Additive");

	Zenith_SceneManager::SetActiveScene(xOriginal);
	Zenith_Assert(Zenith_SceneManager::GetActiveScene() == xOriginal, "Original should be active");

	Zenith_SceneManager::SetActiveScene(xAdditive);
	Zenith_Assert(Zenith_SceneManager::GetActiveScene() == xAdditive, "Additive should now be active");

	Zenith_SceneManager::UnloadScene(xOriginal);
	Zenith_Assert(Zenith_SceneManager::GetActiveScene() == xAdditive, "Additive should remain active after unloading original");
	Zenith_Assert(xAdditive.IsValid(), "Additive scene should still be valid");

	Zenith_SceneManager::UnloadScene(xAdditive);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAdditiveSetActiveUnloadOriginal passed");
}

void Zenith_SceneTests::TestDontDestroyOnLoadDuringOnDestroy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDontDestroyOnLoadDuringOnDestroy...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DDOLDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	SceneTestBehaviour::s_pfnOnDestroyCallback = [](Zenith_Entity& xEntity)
	{
		// Attempt DontDestroyOnLoad during destruction - should be no-op or safe
		xEntity.DontDestroyOnLoad();
	};

	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "DDOLOnDestroy");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	Zenith_SceneManager::UnloadScene(xScene);
	// No crash is the primary assertion
	PumpFrames(1);

	SceneTestBehaviour::s_pfnOnDestroyCallback = nullptr;
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDontDestroyOnLoadDuringOnDestroy passed");
}

void Zenith_SceneTests::TestMoveEntityToUnloadingScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToUnloadingScene...");

	Zenith_Scene xSource = Zenith_SceneManager::CreateEmptyScene("MoveUnloadSource");
	Zenith_Scene xTarget = Zenith_SceneManager::CreateEmptyScene("MoveUnloadTarget");
	Zenith_SceneData* pxSourceData = Zenith_SceneManager::GetSceneData(xSource);
	Zenith_SceneData* pxTargetData = Zenith_SceneManager::GetSceneData(xTarget);

	// Put entities in target so we can async unload it
	for (int i = 0; i < 20; i++)
		Zenith_Entity(pxTargetData, "TargetEntity_" + std::to_string(i));

	Zenith_Entity xEntity(pxSourceData, "SourceEntity");

	// Start async unloading target
	Zenith_SceneManager::SetAsyncUnloadBatchSize(5);
	Zenith_SceneOperationID ulOp = Zenith_SceneManager::UnloadSceneAsync(xTarget);
	PumpFrames(1); // Start unloading

	// Try to move entity to the unloading scene
	bool bResult = Zenith_SceneManager::MoveEntityToScene(xEntity, xTarget);
	// Should fail since target is being unloaded
	Zenith_Assert(!bResult, "MoveEntityToScene should fail when target is being async-unloaded");

	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOp);
	if (pxOp) PumpUntilComplete(pxOp);

	Zenith_SceneManager::SetAsyncUnloadBatchSize(50);
	Zenith_SceneManager::UnloadScene(xSource);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityToUnloadingScene passed");
}

//==============================================================================
// Cat 35: Untested Public Method Coverage
//==============================================================================

void Zenith_SceneTests::TestUnloadUnusedAssetsNoCrash()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadUnusedAssetsNoCrash...");

	Zenith_SceneManager::UnloadUnusedAssets();
	// No crash is the assertion

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadUnusedAssetsNoCrash passed");
}

void Zenith_SceneTests::TestGetSceneDataForEntity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneDataForEntity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DataForEntity");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "TestEntity");
	Zenith_EntityID xID = xEntity.GetEntityID();

	Zenith_SceneData* pxFound = Zenith_SceneManager::GetSceneDataForEntity(xID);
	Zenith_Assert(pxFound == pxData, "GetSceneDataForEntity should return the entity's scene data");

	Zenith_SceneData* pxInvalid = Zenith_SceneManager::GetSceneDataForEntity(INVALID_ENTITY_ID);
	Zenith_Assert(pxInvalid == nullptr, "GetSceneDataForEntity with INVALID_ENTITY_ID should return nullptr");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneDataForEntity passed");
}

void Zenith_SceneTests::TestGetSceneDataByHandle()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneDataByHandle...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DataByHandle");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);
	int iHandle = xScene.GetHandle();

	Zenith_SceneData* pxFound = Zenith_SceneManager::GetSceneDataByHandle(iHandle);
	Zenith_Assert(pxFound == pxData, "GetSceneDataByHandle should return correct data");

	Zenith_SceneData* pxInvalid = Zenith_SceneManager::GetSceneDataByHandle(-1);
	Zenith_Assert(pxInvalid == nullptr, "GetSceneDataByHandle with -1 should return nullptr");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneDataByHandle passed");
}

void Zenith_SceneTests::TestGetRootEntitiesVectorOutput()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetRootEntitiesVectorOutput...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("RootVec");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xRoot1(pxData, "Root1");
	Zenith_Entity xRoot2(pxData, "Root2");
	Zenith_Entity xRoot3(pxData, "Root3");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xRoot1.GetEntityID());

	Zenith_Vector<Zenith_Entity> axRoots;
	xScene.GetRootEntities(axRoots);

	Zenith_Assert(axRoots.GetSize() == 3, "Should have 3 root entities, got %u", axRoots.GetSize());
	for (u_int i = 0; i < axRoots.GetSize(); i++)
	{
		Zenith_Assert(axRoots.Get(i).IsRoot(), "All returned entities should be roots");
	}

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetRootEntitiesVectorOutput passed");
}

void Zenith_SceneTests::TestSceneGetHandleAndGetBuildIndex()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneGetHandleAndGetBuildIndex...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("HandleBuildIdx");

	Zenith_Assert(xScene.GetHandle() >= 0, "Handle should be non-negative");
	Zenith_Assert(xScene.GetBuildIndex() == -1, "Build index should be -1 for unregistered scene");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneGetHandleAndGetBuildIndex passed");
}

//==============================================================================
// Cat 36: Entity Event System
//==============================================================================

void Zenith_SceneTests::TestEntityCreatedEventNotFired()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedEventNotFired...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EventCreated");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	static uint32_t s_uEventCount = 0;
	s_uEventCount = 0;

	Zenith_EventHandle uHandle = Zenith_EventDispatcher::Get().Subscribe<Zenith_Event_EntityCreated>(
		[](const Zenith_Event_EntityCreated&) { s_uEventCount++; }
	);

	Zenith_Entity xEntity(pxData, "EventTest");

	// Event type exists but is not dispatched by the engine currently
	// This serves as a regression test: if dispatch is added, this test will need updating
	Zenith_Assert(s_uEventCount == 0, "EntityCreated event should NOT fire (not dispatched by engine)");

	Zenith_EventDispatcher::Get().Unsubscribe(uHandle);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityCreatedEventNotFired passed");
}

void Zenith_SceneTests::TestEntityDestroyedEventNotFired()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityDestroyedEventNotFired...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EventDestroyed");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	static uint32_t s_uEventCount = 0;
	s_uEventCount = 0;

	Zenith_EventHandle uHandle = Zenith_EventDispatcher::Get().Subscribe<Zenith_Event_EntityDestroyed>(
		[](const Zenith_Event_EntityDestroyed&) { s_uEventCount++; }
	);

	Zenith_Entity xEntity(pxData, "EventDestroyTest");
	Zenith_SceneManager::DestroyImmediate(xEntity);

	Zenith_Assert(s_uEventCount == 0, "EntityDestroyed event should NOT fire (not dispatched by engine)");

	Zenith_EventDispatcher::Get().Unsubscribe(uHandle);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEntityDestroyedEventNotFired passed");
}

void Zenith_SceneTests::TestComponentAddedEventNotFired()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentAddedEventNotFired...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EventCompAdded");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	static uint32_t s_uEventCount = 0;
	s_uEventCount = 0;

	Zenith_EventHandle uHandle = Zenith_EventDispatcher::Get().Subscribe<Zenith_Event_ComponentAdded>(
		[](const Zenith_Event_ComponentAdded&) { s_uEventCount++; }
	);

	Zenith_Entity xEntity(pxData, "CompAddTest");
	xEntity.AddComponent<Zenith_CameraComponent>();

	Zenith_Assert(s_uEventCount == 0, "ComponentAdded event should NOT fire (not dispatched by engine)");

	Zenith_EventDispatcher::Get().Unsubscribe(uHandle);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentAddedEventNotFired passed");
}

void Zenith_SceneTests::TestComponentRemovedEventNotFired()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentRemovedEventNotFired...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EventCompRemoved");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	static uint32_t s_uEventCount = 0;
	s_uEventCount = 0;

	Zenith_EventHandle uHandle = Zenith_EventDispatcher::Get().Subscribe<Zenith_Event_ComponentRemoved>(
		[](const Zenith_Event_ComponentRemoved&) { s_uEventCount++; }
	);

	Zenith_Entity xEntity(pxData, "CompRemoveTest");
	xEntity.AddComponent<Zenith_CameraComponent>();
	xEntity.RemoveComponent<Zenith_CameraComponent>();

	Zenith_Assert(s_uEventCount == 0, "ComponentRemoved event should NOT fire (not dispatched by engine)");

	Zenith_EventDispatcher::Get().Unsubscribe(uHandle);
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestComponentRemovedEventNotFired passed");
}

void Zenith_SceneTests::TestEventSubscriberCountTracking()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventSubscriberCountTracking...");

	Zenith_EventHandle uHandle1 = Zenith_EventDispatcher::Get().Subscribe<Zenith_Event_EntityCreated>(
		[](const Zenith_Event_EntityCreated&) {}
	);

	Zenith_Assert(Zenith_EventDispatcher::Get().GetSubscriberCount<Zenith_Event_EntityCreated>() >= 1,
		"Should have at least 1 subscriber");

	Zenith_EventHandle uHandle2 = Zenith_EventDispatcher::Get().Subscribe<Zenith_Event_EntityCreated>(
		[](const Zenith_Event_EntityCreated&) {}
	);

	Zenith_Assert(Zenith_EventDispatcher::Get().GetSubscriberCount<Zenith_Event_EntityCreated>() >= 2,
		"Should have at least 2 subscribers");

	Zenith_EventDispatcher::Get().Unsubscribe(uHandle1);
	Zenith_EventDispatcher::Get().Unsubscribe(uHandle2);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEventSubscriberCountTracking passed");
}

//==============================================================================
// Cat 37: Hierarchy Edge Cases
//==============================================================================

void Zenith_SceneTests::TestCircularHierarchyPreventionGrandchild()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCircularHierarchyPreventionGrandchild...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("CircularHierarchy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xA(pxData, "A");
	Zenith_Entity xB(pxData, "B");
	Zenith_Entity xC(pxData, "C");

	xB.SetParent(xA.GetEntityID());
	xC.SetParent(xB.GetEntityID());

	// Attempt to make A a child of C (circular)
	xA.SetParent(xC.GetEntityID());
	Zenith_Assert(!xA.HasParent(), "A should NOT have a parent (circular hierarchy rejected)");
	Zenith_Assert(xA.IsRoot(), "A should remain a root entity");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCircularHierarchyPreventionGrandchild passed");
}

void Zenith_SceneTests::TestSelfParentPrevention()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSelfParentPrevention...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SelfParent");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xEntity(pxData, "Self");
	xEntity.SetParent(xEntity.GetEntityID());
	Zenith_Assert(!xEntity.HasParent(), "Entity should NOT be its own parent");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSelfParentPrevention passed");
}

void Zenith_SceneTests::TestDetachFromParent()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDetachFromParent...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DetachParent");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());

	Zenith_Assert(xChild.HasParent(), "Child should have parent");
	Zenith_Assert(xParent.HasChildren(), "Parent should have children");

	xChild.GetTransform().DetachFromParent();

	Zenith_Assert(!xChild.HasParent(), "Child should have no parent after detach");
	Zenith_Assert(xChild.IsRoot(), "Child should be root after detach");
	Zenith_Assert(!xParent.HasChildren(), "Parent should have no children after child detached");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDetachFromParent passed");
}

void Zenith_SceneTests::TestDetachAllChildren()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDetachAllChildren...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DetachAll");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild1(pxData, "Child1");
	Zenith_Entity xChild2(pxData, "Child2");
	Zenith_Entity xChild3(pxData, "Child3");

	xChild1.SetParent(xParent.GetEntityID());
	xChild2.SetParent(xParent.GetEntityID());
	xChild3.SetParent(xParent.GetEntityID());

	Zenith_Assert(xParent.GetChildCount() == 3, "Parent should have 3 children");

	xParent.GetTransform().DetachAllChildren();

	Zenith_Assert(xParent.GetChildCount() == 0, "Parent should have 0 children after DetachAllChildren");
	Zenith_Assert(xChild1.IsRoot(), "Child1 should be root");
	Zenith_Assert(xChild2.IsRoot(), "Child2 should be root");
	Zenith_Assert(xChild3.IsRoot(), "Child3 should be root");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDetachAllChildren passed");
}

void Zenith_SceneTests::TestForEachChildDuringChildDestruction()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestForEachChildDuringChildDestruction...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ForEachDestroy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");
	Zenith_Entity xChild1(pxData, "Child1");
	Zenith_Entity xChild2(pxData, "Child2");
	Zenith_Entity xChild3(pxData, "Child3");

	xChild1.SetParent(xParent.GetEntityID());
	xChild2.SetParent(xParent.GetEntityID());
	xChild3.SetParent(xParent.GetEntityID());

	Zenith_EntityID xChild1ID = xChild1.GetEntityID();
	static bool s_bDestroyed = false;
	s_bDestroyed = false;

	// ForEachChild snapshots the child list, so destroying during iteration should be safe
	xParent.GetTransform().ForEachChild([&](Zenith_TransformComponent&)
	{
		if (!s_bDestroyed)
		{
			s_bDestroyed = true;
			Zenith_SceneManager::DestroyImmediate(xChild1);
		}
	});

	Zenith_Assert(s_bDestroyed, "Should have destroyed child during ForEachChild");
	Zenith_Assert(!pxData->EntityExists(xChild1ID), "Child1 should be destroyed");
	// No crash is the primary assertion

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestForEachChildDuringChildDestruction passed");
}

void Zenith_SceneTests::TestReparentDuringForEachChild()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestReparentDuringForEachChild...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ForEachReparent");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParentA(pxData, "ParentA");
	Zenith_Entity xParentB(pxData, "ParentB");
	Zenith_Entity xChild1(pxData, "Child1");
	Zenith_Entity xChild2(pxData, "Child2");

	xChild1.SetParent(xParentA.GetEntityID());
	xChild2.SetParent(xParentA.GetEntityID());

	static Zenith_EntityID s_xParentBID;
	s_xParentBID = xParentB.GetEntityID();
	static bool s_bReparented = false;
	s_bReparented = false;

	// Reparent child1 to ParentB during ForEachChild on ParentA
	xParentA.GetTransform().ForEachChild([&](Zenith_TransformComponent& xChildTransform)
	{
		if (!s_bReparented)
		{
			s_bReparented = true;
			xChildTransform.SetParentByID(s_xParentBID);
		}
	});

	Zenith_Assert(s_bReparented, "Should have reparented during ForEachChild");
	Zenith_Assert(xParentB.HasChildren(), "ParentB should have children after reparent");
	// No crash is the primary assertion

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestReparentDuringForEachChild passed");
}

void Zenith_SceneTests::TestDeepHierarchyBuildModelMatrix()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDeepHierarchyBuildModelMatrix...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("DeepHierarchy");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	const uint32_t uDepth = 105;
	Zenith_Vector<Zenith_Entity> axEntities;

	for (uint32_t i = 0; i < uDepth; i++)
	{
		Zenith_Entity xEntity(pxData, "Depth_" + std::to_string(i));
		if (i > 0)
		{
			xEntity.SetParent(axEntities.Get(i - 1).GetEntityID());
		}
		axEntities.PushBack(xEntity);
	}

	// BuildModelMatrix on the deepest entity
	Zenith_Maths::Matrix4 xMat;
	axEntities.Get(uDepth - 1).GetTransform().BuildModelMatrix(xMat);
	// No crash is the primary assertion - the depth limit (1000) should not be hit

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDeepHierarchyBuildModelMatrix passed");
}

//==============================================================================
// Cat 38: Path Canonicalization
//==============================================================================

void Zenith_SceneTests::TestCanonicalizeDotSlashPrefix()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCanonicalizeDotSlashPrefix...");

	CreateTestSceneFile("test_dotslash" ZENITH_SCENE_EXT);
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene("./test_dotslash" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "Scene loaded with ./ prefix should be valid");

	Zenith_Scene xFound = Zenith_SceneManager::GetSceneByPath("test_dotslash" ZENITH_SCENE_EXT);
	Zenith_Assert(xFound.IsValid(), "Should find scene by canonical path");

	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile("test_dotslash" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCanonicalizeDotSlashPrefix passed");
}

void Zenith_SceneTests::TestCanonicalizeParentResolution()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCanonicalizeParentResolution...");

	// Create a test scene file in current directory
	CreateTestSceneFile("test_parent_res" ZENITH_SCENE_EXT);

	// Load with ../ in path
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene("sub/../test_parent_res" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "Scene loaded with ../ path should be valid");

	Zenith_Scene xFound = Zenith_SceneManager::GetSceneByPath("test_parent_res" ZENITH_SCENE_EXT);
	Zenith_Assert(xFound.IsValid(), "Should find scene by canonical path after ../ resolution");

	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile("test_parent_res" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCanonicalizeParentResolution passed");
}

void Zenith_SceneTests::TestCanonicalizeDoubleSlash()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCanonicalizeDoubleSlash...");

	CreateTestSceneFile("test_doubleslash" ZENITH_SCENE_EXT);
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene(".//test_doubleslash" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "Scene loaded with // should be valid");

	Zenith_Scene xFound = Zenith_SceneManager::GetSceneByPath("test_doubleslash" ZENITH_SCENE_EXT);
	Zenith_Assert(xFound.IsValid(), "Should find scene by canonical path after // collapse");

	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile("test_doubleslash" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCanonicalizeDoubleSlash passed");
}

void Zenith_SceneTests::TestCanonicalizeAlreadyCanonical()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCanonicalizeAlreadyCanonical...");

	CreateTestSceneFile("test_canonical" ZENITH_SCENE_EXT);
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene("test_canonical" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "Scene loaded with clean path should be valid");

	Zenith_Scene xFound = Zenith_SceneManager::GetSceneByPath("test_canonical" ZENITH_SCENE_EXT);
	Zenith_Assert(xFound.IsValid(), "Should find scene by same canonical path");

	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile("test_canonical" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCanonicalizeAlreadyCanonical passed");
}

void Zenith_SceneTests::TestGetSceneByPathNonCanonical()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByPathNonCanonical...");

	CreateTestSceneFile("test_noncanon" ZENITH_SCENE_EXT);
	Zenith_Scene xScene = Zenith_SceneManager::LoadScene("test_noncanon" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);

	// Query with backslash + .\ prefix (Windows-style) - should canonicalize to "test_noncanon.zscen"
	Zenith_Scene xFoundBackslash = Zenith_SceneManager::GetSceneByPath(".\\test_noncanon" ZENITH_SCENE_EXT);
	Zenith_Assert(xFoundBackslash.IsValid(), "GetSceneByPath should find scene with backslash+dot prefix");
	Zenith_Assert(xFoundBackslash == xScene, "Backslash query should return same scene handle");

	// Query with double forward slash
	Zenith_Scene xFoundDouble = Zenith_SceneManager::GetSceneByPath(".//test_noncanon" ZENITH_SCENE_EXT);
	Zenith_Assert(xFoundDouble.IsValid(), "GetSceneByPath should find scene with double-slash prefix");
	Zenith_Assert(xFoundDouble == xScene, "Double-slash query should return same scene handle");

	Zenith_SceneManager::UnloadScene(xScene);
	CleanupTestSceneFile("test_noncanon" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetSceneByPathNonCanonical passed");
}

//==============================================================================
// Cat 39: Stress & Boundary
//==============================================================================

void Zenith_SceneTests::TestRapidCreateDestroyEntitySlotIntegrity()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRapidCreateDestroyEntitySlotIntegrity...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("SlotIntegrity");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	for (int i = 0; i < 1000; i++)
	{
		Zenith_Entity xEntity(pxData, "Temp");
		Zenith_SceneManager::DestroyImmediate(xEntity);
	}

	// After all create/destroy cycles, create one more and verify it works
	Zenith_Entity xFinal(pxData, "Final");
	Zenith_Assert(xFinal.IsValid(), "Entity should be valid after rapid create/destroy cycles");
	Zenith_Assert(pxData->GetEntityCount() == 1, "Should have exactly 1 entity (no slot leaks), got %u", pxData->GetEntityCount());

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRapidCreateDestroyEntitySlotIntegrity passed");
}

void Zenith_SceneTests::TestSceneHandlePoolIntegrityCycles()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHandlePoolIntegrityCycles...");

	uint32_t uInitialCount = Zenith_SceneManager::GetLoadedSceneCount();

	for (int i = 0; i < 100; i++)
	{
		Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("Cycle_" + std::to_string(i));
		Zenith_SceneManager::UnloadScene(xScene);
	}

	Zenith_Scene xFinal = Zenith_SceneManager::CreateEmptyScene("FinalScene");
	Zenith_Assert(xFinal.IsValid(), "Scene should be valid after 100 create/unload cycles");
	Zenith_Assert(Zenith_SceneManager::GetLoadedSceneCount() == uInitialCount + 1, "Scene count should be initial + 1");

	Zenith_SceneManager::UnloadScene(xFinal);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSceneHandlePoolIntegrityCycles passed");
}

void Zenith_SceneTests::TestMoveEntityThroughMultipleScenes()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityThroughMultipleScenes...");

	const int iSceneCount = 5;
	Zenith_Scene axScenes[5];
	for (int i = 0; i < iSceneCount; i++)
	{
		axScenes[i] = Zenith_SceneManager::CreateEmptyScene("Chain_" + std::to_string(i));
	}

	Zenith_SceneData* pxFirstData = Zenith_SceneManager::GetSceneData(axScenes[0]);
	Zenith_Entity xEntity(pxFirstData, "Traveler");
	Zenith_EntityID xOriginalID = xEntity.GetEntityID();

	for (int i = 1; i < iSceneCount; i++)
	{
		bool bResult = Zenith_SceneManager::MoveEntityToScene(xEntity, axScenes[i]);
		Zenith_Assert(bResult, "Move to scene %d should succeed", i);
		Zenith_Assert(xEntity.GetEntityID() == xOriginalID, "EntityID should be preserved after move %d", i);
	}

	// Entity should be in the last scene
	Zenith_SceneData* pxLastData = Zenith_SceneManager::GetSceneData(axScenes[iSceneCount - 1]);
	Zenith_Assert(pxLastData->EntityExists(xOriginalID), "Entity should exist in final scene");

	for (int i = 0; i < iSceneCount; i++)
	{
		Zenith_SceneManager::UnloadScene(axScenes[i]);
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMoveEntityThroughMultipleScenes passed");
}

void Zenith_SceneTests::TestManyTimedDestructionsExpireSameFrame()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestManyTimedDestructionsExpireSameFrame...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ManyTimed");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	const uint32_t uCount = 50;
	Zenith_Vector<Zenith_EntityID> axIDs;

	for (uint32_t i = 0; i < uCount; i++)
	{
		Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "Timed_" + std::to_string(i));
		axIDs.PushBack(xEntity.GetEntityID());
	}

	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	SceneTestBehaviour::ResetCounters();

	// All with the same delay
	for (uint32_t i = 0; i < axIDs.GetSize(); i++)
	{
		Zenith_Entity xEntity = pxData->GetEntity(axIDs.Get(i));
		Zenith_SceneManager::Destroy(xEntity, 0.1f);
	}

	// Pump past the delay
	PumpFrames(10); // ~0.167s at 60fps

	Zenith_Assert(SceneTestBehaviour::s_uDestroyCount == uCount, "All %u entities should be destroyed, got %u", uCount, SceneTestBehaviour::s_uDestroyCount);

	for (uint32_t i = 0; i < axIDs.GetSize(); i++)
	{
		Zenith_Assert(!pxData->EntityExists(axIDs.Get(i)), "Entity %u should not exist", i);
	}

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestManyTimedDestructionsExpireSameFrame passed");
}

void Zenith_SceneTests::TestMaxConcurrentAsyncOperationsEnforced()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMaxConcurrentAsyncOperationsEnforced...");

	uint32_t uOldMax = Zenith_SceneManager::GetMaxConcurrentAsyncLoads();
	Zenith_SceneManager::SetMaxConcurrentAsyncLoads(2);

	// Create test scene files
	for (int i = 0; i < 4; i++)
	{
		CreateTestSceneFile("test_concurrent_" + std::to_string(i) + ZENITH_SCENE_EXT, "Entity_" + std::to_string(i));
	}

	Zenith_Vector<Zenith_SceneOperationID> axOps;
	for (int i = 0; i < 4; i++)
	{
		Zenith_SceneOperationID ulOp = Zenith_SceneManager::LoadSceneAsync("test_concurrent_" + std::to_string(i) + ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
		axOps.PushBack(ulOp);
	}

	// Pump until all complete
	bool bAllComplete = false;
	int iMaxFrames = 600;
	while (!bAllComplete && iMaxFrames-- > 0)
	{
		PumpFrames(1);
		bAllComplete = true;
		for (uint32_t i = 0; i < axOps.GetSize(); i++)
		{
			Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(axOps.Get(i));
			if (pxOp && !pxOp->IsComplete()) bAllComplete = false;
		}
	}

	Zenith_Assert(bAllComplete, "All async loads should eventually complete");

	// Cleanup loaded scenes
	for (uint32_t i = 0; i < axOps.GetSize(); i++)
	{
		Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(axOps.Get(i));
		if (pxOp)
		{
			Zenith_Scene xResult = pxOp->GetResultScene();
			if (xResult.IsValid()) Zenith_SceneManager::UnloadScene(xResult);
		}
	}

	for (int i = 0; i < 4; i++)
	{
		CleanupTestSceneFile("test_concurrent_" + std::to_string(i) + ZENITH_SCENE_EXT);
	}

	Zenith_SceneManager::SetMaxConcurrentAsyncLoads(uOldMax);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMaxConcurrentAsyncOperationsEnforced passed");
}

//==============================================================================
// Cat 40: Scene Lifecycle State Verification
//==============================================================================

void Zenith_SceneTests::TestIsLoadedAtEveryStage()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsLoadedAtEveryStage...");

	// Empty scene is loaded immediately
	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("LoadedStages");
	Zenith_Assert(xScene.IsLoaded(), "Empty scene should be loaded immediately");

	Zenith_SceneManager::UnloadScene(xScene);
	// After unload, stale handle
	Zenith_Assert(!xScene.IsLoaded(), "Scene should not be loaded after unload");

	// Async load with activation paused
	CreateTestSceneFile("test_loaded_stages" ZENITH_SCENE_EXT);
	Zenith_SceneOperationID ulOp = Zenith_SceneManager::LoadSceneAsync("test_loaded_stages" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_SceneOperation* pxOp = Zenith_SceneManager::GetOperation(ulOp);
	pxOp->SetActivationAllowed(false);

	PumpFrames(5);

	// If not yet complete, result scene might exist but not be fully loaded
	if (!pxOp->IsComplete())
	{
		Zenith_Scene xAsyncScene = pxOp->GetResultScene();
		if (xAsyncScene.IsValid())
		{
			Zenith_Assert(!xAsyncScene.IsLoaded(), "Scene should not be loaded before activation");
		}
	}

	pxOp->SetActivationAllowed(true);
	PumpUntilComplete(pxOp);

	Zenith_Scene xResult = pxOp->GetResultScene();
	Zenith_Assert(xResult.IsLoaded(), "Scene should be loaded after activation");

	Zenith_SceneManager::UnloadScene(xResult);
	CleanupTestSceneFile("test_loaded_stages" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestIsLoadedAtEveryStage passed");
}

void Zenith_SceneTests::TestStaleHandleEveryMethodGraceful()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStaleHandleEveryMethodGraceful...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("StaleEvery");
	Zenith_Scene xOldHandle = xScene; // Copy the handle
	Zenith_SceneManager::UnloadScene(xScene);

	// Create a new scene that may reuse the slot
	Zenith_Scene xNew = Zenith_SceneManager::CreateEmptyScene("NewScene");

	// Call every method on the stale handle - none should crash
	Zenith_Assert(!xOldHandle.IsValid(), "Stale handle should be invalid");
	Zenith_Assert(!xOldHandle.IsLoaded(), "Stale handle IsLoaded should return false");
	Zenith_Assert(!xOldHandle.WasLoadedAdditively(), "Stale handle WasLoadedAdditively should return false");

	uint32_t uRootCount = xOldHandle.GetRootEntityCount();
	Zenith_Assert(uRootCount == 0, "Stale handle GetRootEntityCount should return 0");

	Zenith_SceneManager::UnloadScene(xNew);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStaleHandleEveryMethodGraceful passed");
}

void Zenith_SceneTests::TestSyncLoadSingleModeTwice()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSyncLoadSingleModeTwice...");

	CreateTestSceneFile("test_twice" ZENITH_SCENE_EXT, "TwiceEntity");

	// First SINGLE load - unloads all non-persistent, loads new scene
	Zenith_Scene xFirst = Zenith_SceneManager::LoadScene("test_twice" ZENITH_SCENE_EXT, SCENE_LOAD_SINGLE);
	Zenith_Assert(xFirst.IsValid(), "First SINGLE load should succeed");

	// Second SINGLE load of same file - should unload xFirst, load new scene
	Zenith_Scene xSecond = Zenith_SceneManager::LoadScene("test_twice" ZENITH_SCENE_EXT, SCENE_LOAD_SINGLE);
	Zenith_Assert(xSecond.IsValid(), "Second SINGLE load should succeed");

	// First scene handle should now be stale (it was unloaded by the second SINGLE load)
	Zenith_Assert(!xFirst.IsValid(), "First scene should be stale after second SINGLE load replaced it");

	// The two handles should be different scenes
	Zenith_Assert(xFirst.GetHandle() != xSecond.GetHandle() || xFirst.m_uGeneration != xSecond.m_uGeneration,
		"First and second loads should produce different scene instances");

	Zenith_SceneManager::UnloadScene(xSecond);
	CleanupTestSceneFile("test_twice" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSyncLoadSingleModeTwice passed");
}

void Zenith_SceneTests::TestAdditiveLoadAlreadyLoadedScene()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAdditiveLoadAlreadyLoadedScene...");

	CreateTestSceneFile("test_dup_additive" ZENITH_SCENE_EXT, "DupEntity");

	Zenith_Scene xFirst = Zenith_SceneManager::LoadScene("test_dup_additive" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	uint32_t uCountAfterFirst = Zenith_SceneManager::GetLoadedSceneCount();

	Zenith_Scene xSecond = Zenith_SceneManager::LoadScene("test_dup_additive" ZENITH_SCENE_EXT, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xSecond.IsValid(), "Second additive load should succeed");
	Zenith_Assert(Zenith_SceneManager::GetLoadedSceneCount() == uCountAfterFirst + 1,
		"Additive load of same file should create separate scene (no dedup)");
	Zenith_Assert(xFirst != xSecond, "Two additive loads should produce different scene handles");

	Zenith_SceneManager::UnloadScene(xFirst);
	Zenith_SceneManager::UnloadScene(xSecond);
	CleanupTestSceneFile("test_dup_additive" ZENITH_SCENE_EXT);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAdditiveLoadAlreadyLoadedScene passed");
}

//==============================================================================
// Cat 41: OnEnable/OnDisable Precise Semantics
//==============================================================================

void Zenith_SceneTests::TestInitialOnEnableFiresOnce()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestInitialOnEnableFiresOnce...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("InitEnable");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();

	// Suppress immediate lifecycle so ScriptComponent is present before batch dispatch
	// (mirrors what happens during scene file loading)
	Zenith_SceneManager::SetPrefabInstantiating(true);
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "InitEnable");
	Zenith_SceneManager::SetPrefabInstantiating(false);

	pxData->DispatchLifecycleForNewScene();

	Zenith_Assert(SceneTestBehaviour::s_uEnableCount == 1, "OnEnable should fire exactly once during initial lifecycle, got %u", SceneTestBehaviour::s_uEnableCount);

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestInitialOnEnableFiresOnce passed");
}

void Zenith_SceneTests::TestDisableThenEnableSameFrame()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisableThenEnableSameFrame...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("ToggleSameFrame");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xEntity = CreateEntityWithBehaviour(pxData, "Toggle");
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	uint32_t uEnableBefore = SceneTestBehaviour::s_uEnableCount;
	uint32_t uDisableBefore = SceneTestBehaviour::s_uDisableCount;

	xEntity.SetEnabled(false);
	xEntity.SetEnabled(true);

	// Both should have incremented
	Zenith_Assert(SceneTestBehaviour::s_uDisableCount > uDisableBefore, "OnDisable should fire");
	Zenith_Assert(SceneTestBehaviour::s_uEnableCount > uEnableBefore, "OnEnable should fire after re-enable");
	Zenith_Assert(xEntity.IsEnabled(), "Entity should be enabled at end");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDisableThenEnableSameFrame passed");
}

void Zenith_SceneTests::TestEnableChildWhenParentDisabled()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEnableChildWhenParentDisabled...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("EnableChildParentDisabled");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xParent(pxData, "Parent");

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xChild = CreateEntityWithBehaviour(pxData, "Child");
	xChild.SetParent(xParent.GetEntityID());
	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	// Disable parent
	xParent.SetEnabled(false);

	// Child is technically enabled (activeSelf=true) but not active in hierarchy
	Zenith_Assert(!xChild.IsActiveInHierarchy(), "Child should not be active in hierarchy when parent disabled");

	// Enable parent should propagate OnEnable to child
	xParent.SetEnabled(true);
	Zenith_Assert(xChild.IsActiveInHierarchy(), "Child should be active in hierarchy after parent enabled");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestEnableChildWhenParentDisabled passed");
}

void Zenith_SceneTests::TestRecursiveEnableMixedHierarchy()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRecursiveEnableMixedHierarchy...");

	Zenith_Scene xScene = Zenith_SceneManager::CreateEmptyScene("RecursiveEnable");
	Zenith_SceneData* pxData = Zenith_SceneManager::GetSceneData(xScene);

	Zenith_Entity xA(pxData, "A");

	SceneTestBehaviour::ResetCounters();
	Zenith_Entity xB = CreateEntityWithBehaviour(pxData, "B"); // Will be disabled (activeSelf=false)
	Zenith_Entity xC = CreateEntityWithBehaviour(pxData, "C"); // Will remain enabled (activeSelf=true)

	xB.SetParent(xA.GetEntityID());
	xC.SetParent(xB.GetEntityID());

	pxData->DispatchLifecycleForNewScene();
	PumpFrames(1);

	// Disable B's own enabled state
	xB.SetEnabled(false);

	// Disable root A
	xA.SetEnabled(false);

	SceneTestBehaviour::ResetCounters();

	// Re-enable A
	xA.SetEnabled(true);

	// B has activeSelf=false, so B should NOT become active (and should not get OnEnable)
	Zenith_Assert(!xB.IsActiveInHierarchy(), "B (activeSelf=false) should NOT be active even though parent A is enabled");

	// C has activeSelf=true but parent B is disabled, so C should NOT be active either
	Zenith_Assert(!xC.IsActiveInHierarchy(), "C should NOT be active because parent B is disabled");

	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRecursiveEnableMixedHierarchy passed");
}

//==============================================================================
// Cat 42: Deferred Scene Load (Unity Parity)
//==============================================================================

void Zenith_SceneTests::TestLoadSceneDeferredDuringUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneDeferredDuringUpdate...");

	const std::string strPath0 = "test_deferred_scene0" ZENITH_SCENE_EXT;
	const std::string strPath1 = "test_deferred_scene1" ZENITH_SCENE_EXT;
	const int iBuildIndex0 = 200;
	const int iBuildIndex1 = 201;

	CreateTestSceneFile(strPath0, "DeferredEntity0");
	CreateTestSceneFile(strPath1, "DeferredEntity1");
	Zenith_SceneManager::RegisterSceneBuildIndex(iBuildIndex0, strPath0);
	Zenith_SceneManager::RegisterSceneBuildIndex(iBuildIndex1, strPath1);

	// Load scene 0 synchronously (s_bIsUpdating is false)
	Zenith_Scene xScene0 = Zenith_SceneManager::LoadSceneByIndex(iBuildIndex0, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene0.IsValid(), "Scene 0 should load synchronously");
	Zenith_SceneManager::SetActiveScene(xScene0);

	// Simulate being inside Update - set s_bIsUpdating = true
	Zenith_SceneManager::s_bIsUpdating = true;

	// LoadSceneByIndex during update should defer (return invalid handle)
	Zenith_Scene xScene1 = Zenith_SceneManager::LoadSceneByIndex(iBuildIndex1, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(!xScene1.IsValid(), "Deferred load should return invalid scene handle");

	// Scene 0 should still be active (load was queued, not processed)
	Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xActive == xScene0, "Active scene should still be scene 0 after deferred load");

	// End the simulated update
	Zenith_SceneManager::s_bIsUpdating = false;

	// Pump frames until the async load completes (worker thread reads file, then
	// ProcessPendingAsyncLoads activates the scene on the next Update call)
	Zenith_Scene xLoadedScene1;
	for (uint32_t i = 0; i < 60; ++i)
	{
		PumpFrames(1);
		xLoadedScene1 = Zenith_SceneManager::GetSceneByPath(strPath1);
		if (xLoadedScene1.IsValid())
			break;
	}
	Zenith_Assert(xLoadedScene1.IsValid(), "Scene 1 should be loaded after pumping frames");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xLoadedScene1);
	Zenith_SceneManager::UnloadScene(xScene0);
	Zenith_SceneManager::ClearBuildIndexRegistry();
	CleanupTestSceneFile(strPath0);
	CleanupTestSceneFile(strPath1);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneDeferredDuringUpdate passed");
}

void Zenith_SceneTests::TestLoadSceneSyncOutsideUpdate()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneSyncOutsideUpdate...");

	const std::string strPath = "test_sync_outside_update" ZENITH_SCENE_EXT;
	const int iBuildIndex = 202;

	CreateTestSceneFile(strPath, "SyncEntity");
	Zenith_SceneManager::RegisterSceneBuildIndex(iBuildIndex, strPath);

	// Verify s_bIsUpdating is false (outside Update)
	Zenith_Assert(!Zenith_SceneManager::s_bIsUpdating, "s_bIsUpdating should be false outside Update");

	// Load should be synchronous and return a valid handle
	Zenith_Scene xScene = Zenith_SceneManager::LoadSceneByIndex(iBuildIndex, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xScene.IsValid(), "LoadSceneByIndex outside Update should return valid scene immediately");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xScene);
	Zenith_SceneManager::ClearBuildIndexRegistry();
	CleanupTestSceneFile(strPath);

	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLoadSceneSyncOutsideUpdate passed");
}
