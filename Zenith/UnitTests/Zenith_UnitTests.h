#pragma once
#include <fstream>
#include "UnitTests/Zenith_SceneTests.h"
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

	// Scene serialization tests
	static void TestSceneSerialization();
	static void TestComponentSerialization();
	static void TestEntitySerialization();
	static void TestSceneRoundTrip();
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
	static void TestTaskArrayCallingThreadParticipates();
	static void TestTaskReuseAfterWait();

	// RenderGraph diagnostic accessor
	static void TestRenderGraphPassOrderDescription();

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
	static void TestRenderGraphBufferBarrierRMW();
	// Compute-write -> indirect-arg-read barrier (terrain culling writes
	// indirect/count buffers, GBuffer pass reads them via DrawIndexedIndirectCount).
	static void TestRenderGraphIndirectArgBarrier();
	// Compute-write -> read-only structured-buffer barrier (terrain culling
	// writes the LOD level buffer, GBuffer vertex shader samples it as
	// StructuredBuffer<uint>).
	static void TestRenderGraphStorageBufferSRVBarrier();

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

	// Flux_ShaderBinder name-cache tests. Exercise the pointer-identity cache
	// inside Flux_ShaderBinder via a synthetic Flux_ShaderReflection (no live
	// Vulkan device required — the resolver path takes a reflection pointer
	// directly so tests bypass the Flux_Shader wrapper). Friended in.
	static void TestBinderNameCacheFirstLookupMisses();
	static void TestBinderNameCacheRepeatLookupHits();
	static void TestBinderNameCacheDifferentReflectionMisses();
	static void TestBinderNameCacheDifferentNameMisses();
	static void TestBinderNameCacheRoundRobinReplacement();
	static void TestBinderNameCacheTypeStoredCorrectly();

	// Flux_PerFrame ring-scheduler tests. Exercise the frame counter,
	// ring-index modulo, and the begin/end callback dispatch logic. Tests
	// save/restore live engine state so they don't disturb the running
	// frame loop that hosts them.
	static void TestFluxPerFrameFrameCounterAdvances();
	static void TestFluxPerFrameRingIndexWraps();
	static void TestFluxPerFrameBeginCallbackFires();
	static void TestFluxPerFrameEndCallbackFires();
	static void TestFluxPerFrameCallbackOrderPreserved();
	static void TestFluxPerFrameCallbackUserDataPassed();
	static void TestFluxPerFrameRingIndexInsideCallback();
};
