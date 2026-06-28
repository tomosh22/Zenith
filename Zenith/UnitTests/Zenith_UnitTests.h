#pragma once
#include <fstream>
#include "UnitTests/Zenith_AITests.h"

class Zenith_UnitTests
{
public:
	// Test infrastructure exposed publicly so RAII helpers in the test .inl files'
	// anonymous namespace can call them (anonymous-namespace types are not
	// friended by `friend class Zenith_UnitTests` declarations elsewhere).
	struct PerFrameSnapshot;
	static void SnapshotPerFrameAndReset(PerFrameSnapshot& xOut);
	static void RestorePerFrame         (const PerFrameSnapshot& xIn);

public:
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

	// HashMap / HashSet tests
	static void TestHashMapBasic();
	static void TestHashMapCollisions();
	static void TestHashMapRehash();
	static void TestHashMapTombstones();
	static void TestHashMapIterator();
	static void TestHashMapIteratorInvalidation();
	static void TestHashMapCopyMove();
	static void TestHashMapSerialization();
	static void TestHashMapOperatorBracket();
	static void TestHashSetBasic();

	// DataStream edge case tests
	static void TestDataStreamBoundsCheck();

	// Stream envelope (reusable DataStream header) tests
	static void TestStreamEnvelopeRoundTrip();

	// Scene serialization tests
	static void TestSceneSerialization();
	static void TestComponentSerialization();
	static void TestEntitySerialization();
	static void TestSceneRoundTrip();
	static void TestSceneLoadValidation();
	static void TestSceneBodyCorruptionFailsGracefully();
	static void TestSceneComponentSchemaVersion();
	static void TestSceneDisableDestroyHelpers();

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
	static void TestBlendTreeWriteReadChildNode();
	static void TestBlendTreeEvaluateChildOrReset();
	static void TestBlendTreeSelectGetSelectedChild();
	static void TestFABRIKSolver();

	// IK helper refactoring tests
	static void TestIKSafeNormalize();
	static void TestIKFindPerpendicularAxis();
	static void TestIKConstrainBoneLength();

	// IK engine-plumbing tests added by the foot-placement plan
	static void TestIKResolveBoneIndices();
	static void TestIKComputeBoneLengths();
	static void TestComputeModelSpaceMatricesFromSkeleton();
	static void TestIKSolveReachableTarget();
	static void TestIKSolveUnreachableTarget();
	static void TestIKSolveDisabledTarget();
	static void TestIKLazyResolveOnFirstSolve();
	static void TestIKWorldMatrixTransform();
	static void TestIKHingeConstraintProjectsSegmentDirection();
	static void TestIKPoleVectorBiasesKnee();
	static void TestIKMultiChainBoneDisjoint();
	static void TestIKConvergence();
	static void TestIKWeightZeroIsNoop();
	static void TestIKDegenerateChainSizes();
	static void TestIKSolveOnReloadedAsset();
	// Reproductions of the "feet dragging behind body" bug report
	static void TestIKWithPlayerLikeWorldRotation();
	static void TestIKWithPlayerLikeWorldRotationAndPole();
	static void TestIKEachCardinalRotation();
	static void TestIKOverManyFramesWalkingAndRotating();
	static void TestIKFootBindMatchesCapsuleBottomForPlayerOnGround();
	static void TestIKModelSpaceTargetAvoidsPhysicsLagDrag();
	static void TestIKModelSpaceTargetEqualsWorldSpaceTargetWhenNoLag();
	static void TestIKLocksFootXZAcrossFramesAtFullWeight();
	static void TestIKLetsAnimationDriveFootXZAtZeroWeight();
	static void TestIKAsymmetricFeetAtSpawn();

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
	static void TestStickFigureMeshJointAlignment();
	static void TestStickFigureIdleAnimation();
	static void TestStickFigureWalkAnimation();
	static void TestStickFigureRunAnimation();
	static void TestStickFigureAnimationBlending();

	// Stick figure IK tests
	static void TestStickFigureArmIK();
	static void TestStickFigureArmIKEndEffectorOrientation();
	static void TestStickFigureSupportHandIKReachesForegrip();
	static void TestStickFigureLegIK();
	static void TestStickFigureIKWithAnimation();

	// Stick figure asset export
	static void TestStickFigureAssetExport();

	// Procedural tree asset export (for instanced mesh testing)
	static void TestProceduralTreeAssetExport();

	// ECS bug fix tests (Phase 1)
	static void TestComponentRemovalIndexUpdate();
	static void TestComponentSwapAndPop();
	static void TestSwapAndPopMovesIndex();
	static void TestQueryNestedReentrancy();
	static void TestBenchECSSmoke();
	static void TestMultipleComponentRemoval();
	static void TestComponentRemovalWithManyEntities();
	static void TestEntityNameFromScene();
	static void TestEntityCopyPreservesAccess();

	// Render-phase boundary signal (always-compiled atomic)
	static void TestRenderPhaseTransitions();

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

	// WS10 sparse-set keystone: fuzz cross-check that the sparse-set query read
	// path returns the EXACT same matched-entity set as the legacy scan path
	// (and matches an independent ground-truth oracle), across ~5000 random
	// Add/Remove/Destroy+recreate/cross-scene-move ops.
	static void TestQuerySparseLegacyEquivalence();

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

	// Phase 1 scene-graph transform-cache tests. Cover the revision-checked
	// BuildModelMatrix cache (hit==recompute + sentinel first-call), invalidation by
	// every pose setter + structural edit + ancestor edit, the leaf-safe slot
	// hierarchy-revision propagation (subtree bumped, siblings untouched), the
	// worker-thread cache-write contract (a worker miss leaves the shared cache
	// unmutated), and the physics → transform sync (post-physics sweep for sim moves +
	// the teleport hook for immediate invalidation).
	static void TestTransformCacheReturnsRecomputedValue();
	static void TestTransformCacheInvalidatedBySetters();
	static void TestTransformCacheInvalidatedByAncestorEdit();
	static void TestTransformCacheInvalidatedByReparent();
	static void TestTransformCacheInvalidatedByDetachAllChildren();
	static void TestHierRevisionPropagatesToSubtree();
	static void TestTransformCacheWorkerContractLeavesCacheUnwritten();
	static void TestPhysicsSweepInvalidatesTransformCache();
	static void TestTeleportInvalidatesTransformCacheImmediately();
	// Adversarial-review coverage gaps: the sweep must be a NO-OP for an unmoved body (no
	// spurious epsilon trip re-invalidating every static collider's subtree each frame), and
	// the production worker path (main builds the cache, workers concurrently READ the hit)
	// must leave the cache intact.
	static void TestPhysicsSweepNoOpForUnmovedBody();
	static void TestTransformCacheWorkerConcurrentReadHits();

	// Phase 2 scene-graph render-snapshot + lifetime-epoch tests. Cover the
	// Zenith_SceneSystem render-mutation epoch (NotifyRenderMutation / GetRenderMutationEpoch),
	// and the engine-owned Flux_RenderSceneSnapshot's pure mechanics: Rebuild stamps the
	// epoch + fills items + bumps the generation; IsCurrent tracks the epoch (false after a
	// mutation, true after the next rebuild for it); Reset clears entries + invalidates
	// (epoch 0); and the diagnostics are pointer-free.
	static void TestRenderMutationEpochIncrements();
	static void TestSnapshotRebuildStampsEpochAndItems();
	static void TestSnapshotIsCurrentTracksEpoch();
	static void TestSnapshotResetClearsAndInvalidates();
	static void TestSnapshotGenerationBumpsEachRebuild();

	// Phase 3 scene-graph culling tests. Cover the snapshot camera-frustum stamp +
	// the AABB-frustum cull DECISION (the core of camera culling: inside visible,
	// outside culled, straddling conservatively visible), and the entity world-AABB
	// computation the snapshot fill performs (TransformAABB of the local union bounds).
	static void TestSnapshotCameraFrustumCullDecision();
	static void TestWorldAABBTransform();
	// Round-2 review fix: the snapshot frustum-valid flag gates culling (false until the
	// owner stamps a valid camera; cleared each Rebuild) so a camera-invalid frame culls
	// nothing instead of culling against an identity/stale frustum.
	static void TestSnapshotCameraFrustumValidGate();

	// Prefab system tests
	static void TestPrefabCreateFromEntity();
	static void TestPrefabInstantiation();
	static void TestPrefabSaveLoadRoundTrip();
	static void TestPrefabOverrides();
	static void TestPrefabVariantCreation();
	static void TestPrefabVariantInstantiate();
	static void TestPrefabVariantCycleRejected();
	static void TestPrefabVariantNestedPathSkipped();
	static void TestPrefabVariantOverrideApplies();
	static void TestPrefabVariantChain();
	// Coverage gaps from the test audit
	static void TestPrefabApplyToEntity();
	static void TestPrefabApplyVariantToEntity();
	static void TestPrefabMoveConstructor();
	static void TestPrefabMoveAssignment();
	static void TestPrefabVariantRoundTripWithOverrides();
	static void TestPrefabMultipleOverridesSameComponent();
	static void TestPrefabClearOverridesReverts();
	static void TestPrefabCreateAsVariantWithUnsetHandle();
	static void TestPrefabInstantiateNullSceneData();
	static void TestPrefabLoadCorruptedFile();
	static void TestPrefabLoadFromDeletedFile();
	static void TestPrefabSelfVariantRejected();
	static void TestPrefabInstantiateNamesEntity();
	static void TestPrefabVariantInstantiateLifecycleOnceAtTop();
	static void TestPrefabVariantPositionOverrideSyncsPhysicsBody();
	static void TestPrefabVariantScaleOverrideRebuildsCollider();
	// Regression: RebuildCollider must rebuild at the body's current transform,
	// not a stale cached one (Engine change 1 of the prefab-instantiate work).
	static void TestColliderRebuildKeepsMovedTransform();
	static void TestDataParallelTaskCallingThreadParticipates();
	static void TestTaskReuseAfterWait();

	// Wave 8.3 - release-survivable check tier + task-queue overflow grace
	static void TestCheckTierReleaseSurvivable();
	static void TestQueueFullSurfacesError();

	// RenderGraph diagnostic accessor
	static void TestRenderGraphPassOrderDescription();

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
	static void TestBTParallelAllRunning();
	static void TestBTParallelNeitherPolicyMet();
	static void TestBTParallelRequireOneAbortsRunning();
	// Regression guard for the BTComposites refactor: when both policies are
	// REQUIRE_ONE and a single tick returns SUCCESS and FAILURE simultaneously,
	// SUCCESS must win (it's checked first). The refactor preserved this order
	// but no prior test pinned it — flagged in code review.
	static void TestBTParallelRequireOneSuccessWinsOverSimultaneousFailure();
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
	static void TestNavMeshFindNearestPolygonInCell();
	static void TestNavMeshComputePolygonBounds();
	static void TestNavMeshGetRandomReachablePointInRadius();
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
	static void TestPathfindingSmootherRejectsCarvedShortcut();
	static void TestPathfindingSmootherAcceptsValidShortcut();

	// NavMesh Generator helper tests
	static void TestCountWalkableSpans();
	static void TestHasSufficientClearance();
	static void TestMergeOverlappingSpans();

	// Physics mesh generator helper tests
	static void TestFindExtremeVertexIndices();
	static void TestComputeAABBFromPositions();
	static void TestComputeVertexNormals();

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

	// AI System tests - Tactical Point refactoring tests
	static void TestGetEntityPositionValid();
	static void TestGetEntityPositionInvalid();
	static void TestFindBestPointNoMatches();
	static void TestFindBestPointSelectsHighest();
	static void TestScoreCoverDistance();
	static void TestScoreFlankAngle();
	static void TestScoreOverwatchElevation();

	// AI System tests - Debug Variables
	static void TestTacticalPointDebugColor();
	static void TestFindBestPointNoPointsActive();
	static void TestFindBestPointOutOfRange();
	static void TestSquadDebugRoleColor();

	// Squad refactoring tests (FindSharedTargetIndex, formation slot assignment)
	static void TestSharedTargetUpdate();
	static void TestSharedTargetUnknown();
	static void TestFormationSlotsLeaderFirst();
	static void TestFormationSlotsRoleMatching();

	// Squad order helper and alive status refactoring tests
	static void TestSquadPositionOrder();
	static void TestSquadTargetOrderClearsPosition();
	static void TestSquadSimpleOrderClearsAll();
	static void TestSquadDeadMemberTriggersLeaderReassign();
	static void TestSquadAliveMemberPreservesLeader();

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

	// Model Instance Material tests (GBuffer rendering bug fix)
	static void TestModelInstanceMaterialSetAndGet();
	static void TestMaterialHandleCopyPreservesCachedPointer();

	// Any-State Transition tests
	static void TestAnyStateTransitionFires();
	static void TestAnyStateTransitionSkipsSelf();
	static void TestAnyStateTransitionPriority();

	// AnimatorStateInfo tests
	static void TestStateInfoStateName();
	static void TestStateInfoNormalizedTime();

	// CrossFade tests
	static void TestCrossFadeToState();
	static void TestCrossFadeToCurrentState();

	// Sub-State Machine tests
	static void TestSubStateMachineCreation();
	static void TestSubStateMachineSharedParameters();

	// Animation Layer tests
	static void TestLayerCreation();
	static void TestLayerWeightZero();

	// Tween system tests - Easing functions
	static void TestEasingLinear();
	static void TestEasingEndpoints();
	static void TestEasingQuadOut();
	static void TestEasingBounceOut();

	// Tween system tests - TweenInstance
	static void TestTweenInstanceProgress();
	static void TestTweenInstanceCompletion();
	static void TestTweenInstanceDelay();

	// Tween system tests - TweenComponent
	static void TestTweenComponentScaleTo();
	static void TestTweenComponentPositionTo();
	static void TestTweenComponentMultiple();
	static void TestTweenComponentCallback();
	static void TestTweenComponentLoop();
	static void TestTweenComponentPingPong();
	static void TestTweenComponentCancel();

	// Sub-SM transition evaluation (BUG 1 regression test)
	static void TestSubStateMachineTransitionEvaluation();

	// CrossFade edge cases
	static void TestCrossFadeNonExistentState();
	static void TestCrossFadeInstant();

	// Tween rotation
	static void TestTweenComponentRotation();

	// Bug regression tests (from code review)
	static void TestTriggerNotConsumedOnPartialConditionMatch();
	static void TestResolveClipReferencesRecursive();
	static void TestTweenDelayWithLoop();
	static void TestTweenCallbackReentrant();
	static void TestTweenDuplicatePropertyCancels();

	// Code review round 2 - bug fix regression tests
	static void TestSubStateMachineTransitionBlendPose();
	static void TestRotationTweenShortestPath();
	static void TestTransitionInterruption();
	static void TestTransitionNonInterruptible();
	static void TestCancelByPropertyKeepsOthers();
	static void TestCrossFadeWhileTransitioning();
	static void TestTweenLoopValueReset();

	// Code review round 3 - Bug 1 regression test + serialization round-trips
	static void TestTriggerNotConsumedWhenBlockedByPriority();
	static void TestAnimationLayerSerialization();
	static void TestAnyStateTransitionSerialization();
	static void TestSubStateMachineSerialization();

	// Code review round 4 - bug fix validation tests
	static void TestHasAnimationContentWithLayers();
	static void TestInitializeRetroactiveLayerPoses();
	static void TestResolveClipReferencesBlendSpace2D();
	static void TestResolveClipReferencesSelect();
	static void TestLayerCompositionOverrideBlend();

	// Code review round 5 - additional coverage
	static void TestLayerCompositionAdditiveBlend();
	static void TestLayerMaskedOverrideBlend();
	static void TestPingPongAsymmetricEasing();
	static void TestTransitionCompletionFramePose();
	static void TestStateMachineUpdateNoStates();
	static void TestStateMachineAutoInitDefaultState();

	// Scene Management tests moved to Zenith_SceneTests.h/.cpp

	// Terrain streaming tests
	static void TestChunkDistanceSymmetry();
	static void TestChunkDistanceZero();
	// Per-component streaming-state isolation. Each Zenith_TerrainComponent
	// owns its own Flux_TerrainStreamingState; touching one state's dirty
	// flag or residency must not affect another's.
	static void TestTerrainStreamingDirtyFlagPerState();
	static void TestTerrainStreamingResidencyIsolatedBetweenStates();
	// SquaredHysteresis(linear) returns linear*linear so the eviction
	// threshold matches the documented linear radius (the previous bug
	// applied a linear ratio directly to a squared threshold).
	static void TestTerrainHysteresisSquaredDistance();
	// RebuildActiveChunkSet sorts by squared distance to camera so the
	// per-frame streaming budget favours nearby chunks.
	static void TestTerrainActiveSetSortedNearestFirst();
	static void TestTerrainActiveSetCenterIndexFirst();
	static void TestTerrainActiveSetUsesNearestAABBForOffsetTerrain();
	static void TestTerrainChunkDataNoLowZeroWhenLowResident();
	// Wave-18 ownership-relocation: Zenith_TerrainComponent now has an explicit
	// move ctor + move assignment that steal the owned Flux_TerrainStreamingState
	// (+ physics geometry + handles), null the source, and repoint the state's
	// m_pxOwner back-pointer at the moved-to component. Regression guard for the
	// latent double-free the implicit (shallow) move would have caused on a
	// component-pool relocation (swap-and-pop / Grow).
	static void TestTerrainComponentMoveStealsState();
	static void TestTerrainComponentMoveAssignmentStealsState();

	// Wave-19 ownership-relocation: Zenith_AnimatorComponent is now a thin
	// forwarding handle into Flux_AnimationControllerStore (keyed by EntityID
	// slot). These pin the relocation invariants: a component move (ctor +
	// assign) shares the SAME store-owned controller with the moved-to instance
	// and leaves the moved-from safe (no double-Destroy); a real pool swap-and-pop
	// relocation and a cross-scene MoveEntityToScene both keep the cached
	// controller pointer valid (heap-stable) and produce EXACTLY ONE controller
	// per entity.
	static void TestAnimatorControllerStoreMoveCtorSharesController();
	static void TestAnimatorControllerStoreMoveAssignReleasesDest();
	static void TestAnimatorControllerStoreSurvivesPoolRelocation();
	static void TestAnimatorControllerStoreSurvivesCrossSceneMove();
	static void TestAnimatorControllerStoreDestroyIsIdempotent();
	static void TestAnimatorControllerStoreValidatesGeneration();

	// Gizmo math helper tests (ZENITH_TOOLS only)
	static void TestGizmosLineLineParallel();
	static void TestGizmosLineLinePerpendicular();
	static void TestGizmosTangentFrame();

	// Gizmo Unity-parity tests — verify GetEditableTransform resolves via the target
	// entity's OWN scene, not the active scene (audit §3.17).
	// See: https://docs.unity3d.com/ScriptReference/SceneManagement.SceneManager.GetActiveScene.html
	static void TestGizmoEditsPersistentEntityAcrossSceneLoad();
	static void TestGizmoEditsEntityInAdditiveScene();
	static void TestGizmoDragSurvivesActiveSceneChange();
	static void TestGizmoGetEditableTransform_ReturnsNullForInvalidTarget();

	// Animation state machine helper tests
	static void TestParamSerializationFloat();
	static void TestParamSerializationBoolTrigger();
	static void TestCompareNumericGreater();
	static void TestCompareNumericLessEqual();
	static void TestPriorityInsertionMiddle();
	static void TestPriorityInsertionEmpty();

	// Vulkan Memory Manager refactoring tests
	static void TestImageViewType3D();
	static void TestImageViewTypeCube();
	static void TestImageViewTypeDefault2D();
	static void TestDestroySkipsInvalidHandle();

	// UIStyle tests
	static void TestUIStyleDefaultValues();
	static void TestUIStyleLerpIdentity();
	static void TestUIStyleLerpHalfway();
	static void TestUIStyleLerpEndpoints();
	static void TestUIStyleLerpShadowBool();

	// UIText alignment helper tests
	static void TestUITextHorizontalAlignment();
	static void TestUITextVerticalAlignment();

	// Slang reflection v2 round-trip tests
	static void TestReflectionV2RoundTrip();
	static void TestReflectionV2EmptyRoundTrip();
	static void TestReflectionV2UnboundedDescriptorCount();
	static void TestReflectionV2NamedLookupAfterDeserialise();

	// Codegen determinism tests (Slang -> C++ header generator)
	static void TestCodegenDeterministicDoubleRun();
	static void TestCodegenContainsBindingMetadata();
	static void TestCodegenEmitsCBStructWithStaticAsserts();
	static void TestCodegenSanitisesIdentifiers();
	static void TestCodegenScalarHungarianPrefixes();
	static void TestCodegenInsertsTrailingPadding();
	static void TestCodegenInsertsInteriorPadding();

	// Flux render-graph tests (declaration-phase only — compiling the graph
	// requires Vulkan). The tests exercise pass registration, handle validity,
	// generation-counter bumping on Clear(), and SetEnabled toggling.
	static void TestRenderGraphEmpty();
	static void TestRenderGraphPassHandles();
	static void TestRenderGraphTransientGeneration();
	static void TestRenderGraphSetEnabled();
	// Generic pass identity + effective-enabled force-disable overlay (the
	// game-render-feature support). FindPass lookup; owner-tag population;
	// force-disable by owner/name flips EFFECTIVE not BASE; override persists
	// across Clear(); base bit preserved under override.
	static void TestRenderGraphFindPass();
	static void TestRenderGraphForceDisableOverlay();
	static void TestRenderGraphForceDisablePersistsClear();
	static void TestRenderGraphForceDisableBasePreserved();
	static void TestRenderGraphBufferBarrierRMW();
	// Compute-write -> indirect-arg-read barrier (terrain culling writes
	// indirect/count buffers, GBuffer pass reads them via DrawIndexedIndirectCount).
	static void TestRenderGraphIndirectArgBarrier();
	// Compute-write -> read-only structured-buffer barrier (terrain culling
	// writes the LOD level buffer, GBuffer vertex shader samples it as
	// StructuredBuffer<uint>).
	static void TestRenderGraphStorageBufferSRVBarrier();
	// Cross-frame cyclic barrier seed: a persistent buffer written then read in a
	// frame must get a barrier on its FIRST access (the write) sourced from its
	// LAST access (the read), so frame N+1's write waits for frame N's read
	// (the instanced-mesh reset-vs-prior-draw flicker regression).
	static void TestRenderGraphCyclicBufferBarrier();
	// Cyclic seed is "adds-only": a read-only buffer (two readers, no writer) must
	// collapse to ZERO barriers on its first access (read-after-read), proving the
	// seed injects nothing spurious onto the engine's read-only persistent buffers.
	static void TestRenderGraphCyclicReadOnlyNoBarrier();

	// WS7 keystone (C1C2): InstancedMeshes Prepare-gather determinism / thread-safety
	// regression. With GPU culling forced OFF (the path that used to double-call
	// UpdateGPUBuffers across two concurrent record callbacks), the per-group CPU
	// visibility bookkeeping (ComputeVisibleIndices + m_uVisibleCount) is now produced
	// by a SINGLE main-thread Prepare writer. The test seeds a fixed instanced layout,
	// captures a legacy serial-reference hash, then asserts every repeated re-seed +
	// recompute is byte-identical (zero divergence == the cross-worker race is gone).
	// Device-independent (CPU SoA only) so it runs in the headless suite.
	static void TestInstancedMeshesPrepareDeterminism();

	// Transient-aliasing signature tests. The signature is pure pointer math
	// over the transient descriptor; no Vulkan required.
	static void TestAliasSignatureIdenticalDescs();
	static void TestAliasSignatureDifferentFormat();
	static void TestAliasSignatureDifferentMemoryFlags();
	static void TestAliasSignatureDifferentTextureType();
	static void TestAliasSignatureDepthVsColour();
	static void TestAliasSignatureIgnoresDimensions();

	// Transient-aliasing lifetime + packer tests. These bypass Compile()
	// (which allocates VRAM) by manually populating m_xExecutionOrder via
	// the Zenith_UnitTests friend access, then invoking the lifetime and
	// packing phases directly on the CPU. Cover regressions for: pass-
	// declaration-index-vs-topological-order confusion in the packer
	// (the SSR/SSGI bug), multi-write transient last-use tracking,
	// disabled-pass exclusion from lifetimes, and idempotency of the
	// recompute path used at Execute when m_bEnabledMaskDirty.
	static void TestRenderGraphLifetimeUsesTopologicalOrder();
	static void TestRenderGraphAliasingTopoOrderInterleaved();
	static void TestRenderGraphMultiWriteLastUse();
	static void TestRenderGraphAliasingMultiWriteOverlap();
	static void TestRenderGraphDisabledPassExcludedFromLifetimes();
	static void TestRenderGraphLifetimeRecomputeIdempotent();
	static void TestRenderGraphAliasingBarrierUsesTopologicalLastUse();

	// FrameContext frame-index tests. Exercise the engine frame index and
	// ring-index modulo via g_xEngine.Frame().AdvanceFrameIndex(). Tests
	// save/restore the live frame index so they don't disturb the running
	// frame loop that hosts them.
	static void TestFluxPerFrameFrameCounterAdvances();
	static void TestFluxPerFrameRingIndexWraps();

	// AssetRegistry path-normalization tests (verify SetGameAssetsDir /
	// SetEngineAssetsDir → NormalizeAssetDirPath pipeline)
	static void TestAssetRegistryNormalizeBackslashes();
	static void TestAssetRegistryStripsTrailingForwardSlash();
	static void TestAssetRegistryStripsTrailingBackslash();
	static void TestAssetRegistryMixedSeparators();
	static void TestAssetRegistryEmptyStringNoOp();
	static void TestAssetRegistryNoChangeNeeded();
	static void TestAssetRegistrySingleSlashStripsToEmpty();
	static void TestAssetRegistryGameAndEngineDirsSeparate();

	// Flux_RootMotion::SamplePosition/Rotation Delta tests (verify the
	// shared SampleRootMotionDeltas templated helper handles edge cases:
	// empty list, single keyframe, between-frames lerp, identical-timestamp
	// guard, past-end clamp).
	static void TestRootMotionPositionEmpty();
	static void TestRootMotionPositionDisabled();
	static void TestRootMotionPositionSingleKeyframe();
	static void TestRootMotionPositionInterpolatesBetween();
	static void TestRootMotionPositionPastEnd();
	static void TestRootMotionPositionIdenticalTimestamps();
	static void TestRootMotionPositionExactKeyframeMatch();
	static void TestRootMotionRotationEmpty();
	static void TestRootMotionRotationSingleKeyframe();
	static void TestRootMotionRotationInterpolatesBetween();
	static void TestRootMotionRotationPastEnd();
	// Flux_BoneChannel end-of-clip clamp (regression for the instanced-tree leaf
	// "teleport": GetRotationIndex/Position/Scale used to extrapolate the first
	// segment at/past the last keyframe, corrupting the final VAT-baked frame).
	static void TestBoneChannelRotationClampsAtClipEnd();
	static void TestBoneChannelPositionClampsAtClipEnd();
	static void TestBoneChannelScaleClampsAtClipEnd();

	// RenderTest input-simulator tests (RenderTest_FollowCameraComponent +
	// RenderTest_PlayerComponent driven via Zenith_InputSimulator). Each test
	// owns a fresh test scene + minimal Player/GameManager entities. See
	// RenderTest_PlayerComponent.Tests.inl.
	static void TestRenderTestCameraYawDecreasesOnMouseRight();
	static void TestRenderTestCameraPitchDecreasesOnMouseDown();
	static void TestRenderTestCameraPitchClampedAtFloor();
	static void TestRenderTestCameraPitchClampedAtCeiling();
	static void TestRenderTestCameraYawWrapsToZeroToTwoPi();
	static void TestRenderTestPlayerMovesForwardOnW();
	static void TestRenderTestPlayerNoRotationOnBackward();
	static void TestRenderTestPlayerRotatesWithCameraYawWhenAiming();
	static void TestRenderTestAimingFlagSetByRMB();
	static void TestRenderTestFireDecrementsAmmo();
	static void TestRenderTestFireRespectsCooldown();
	static void TestRenderTestAutoReloadOnEmptyClick();
	static void TestRenderTestHipfireReloadRaisesAimLayer();
	static void TestRenderTestReloadBlocksFire();
	// IK helper robustness — fixture has no animator, helper must early-out cleanly
	static void TestRenderTestIKHelperEarlyOutsWithoutAnimator();

	// StickFigure procedural-clip tests (Aim/Fire/Reload/Jump animation factories
	// in Tools/Zenith_Tools_TestAssetExport.cpp). Each test constructs the clip,
	// asserts on metadata + bone-channel presence + a representative sample.
	static void TestStickFigureAimClipMetadata();
	static void TestStickFigureAimClipBoneChannelsExist();
	static void TestStickFigureAimClipRightArmRotation();
	static void TestStickFigureFireClipMetadata();
	static void TestStickFigureFireClipReturnsToAimPoseAtEnd();
	static void TestStickFigureFireClipPeakRecoil();
	static void TestStickFigureReloadClipMetadata();
	static void TestStickFigureReloadClipFiveKeyframesOnLeftArm();
	static void TestStickFigureReloadClipReturnsToAimPoseAtEnd();
	static void TestStickFigureJumpClipMetadata();
	static void TestStickFigureJumpClipBothLegsHaveKeyframes();
	static void TestStickFigureJumpClipReturnsToIdentityAtEnd();

	// StickFigure human body mesh contracts (the lofted, atlas-textured,
	// smooth-skinned body in Tools/Zenith_Tools_TestAssetExport.cpp).
	static void TestStickFigureBodyMeshInvariants();
	static void TestStickFigureBodySmoothSkinning();

	// Flux_BlendTreeNode_BlendSpace1D / 2D nearest-blend-point tests (verify
	// the shared FindNearestBlendPoint templated helper).
	static void TestBlendSpace1DEmptyReturnsZero();
	static void TestBlendSpace1DSingleBlendPoint();
	static void TestBlendSpace1DSelectsNearestPoint();
	static void TestBlendSpace1DSelectsNearestUnsorted();
	static void TestBlendSpace2DEmptyReturnsZero();
	static void TestBlendSpace2DSingleBlendPoint();
	static void TestBlendSpace2DSelectsNearestByEuclideanDistance();
};
