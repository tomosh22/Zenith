# ANIM design: test catalogue

Section file of `Zenith2/Docs/Design/ANIM.md`. Every test ANIM's requirements name, with its file and the case it asserts. Test IDs follow `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"` (subsection "Test naming"); every failure scenario is its own test case ([Z2-N-CONTRACT-412]). Fixtures are generated at test time: no test reads a committed binary file ("Fixtures and generators").

## Scope of this file

| Group | Heading |
|---|---|
| targets and gate steps | "Test targets" |
| skeleton, clip and graph files | "Asset and format tests" |
| compression | "Compression tests" |
| import and cook | "Import and cook tests" |
| graph compile and evaluation | "Graph tests", "Blend and pose tests", "State machine tests" |
| sync, events and root motion | "Sync event and root motion tests" |
| ragdoll and lifecycle | "Ragdoll and lifecycle tests" |
| IK, warping and retargeting | "IK motion warping and retarget tests" |
| facial and streaming | "Facial and streaming tests" |
| budgeting and extraction | "Budgeting and extraction tests" |
| module, bindings and failures | "Module binding and failure tests" |
| the motion matcher and node kind 15 | "Motion matching tests" |
| determinism | "Determinism tests" |
| cross-system scenarios | "Integration scenarios" |
| fuzzing | "Fuzz targets" |
| measurement | "Benchmarks" |
| fixtures | "Fixtures and generators" |

## Test targets

| Target | Sources | Links | Gate step |
|---|---|---|---|
| `ZenithAnimation_Tests` | `Zenith2/Engine/ZenithAnimation/Tests/**` | `ZenithAnimation`, its declared dependencies, `ZenithPhysics_Jolt` (for the tests that need a physics world), `ZenithECS_Custom`, `ZenithTestSupport` | `Test` (win-debug, win-release; android-arm64 builds only) |
| `ZenithCook_Tests` (ANIM's files) | `Zenith2/Tools/ZenithCook/Tests/Animation/**` | ZenithCook's test target | `Test` |
| `ZenithAuthoring_Tests` (ANIM's files) | `Zenith2/Tools/Authoring/Tests/Animation/**` (the graph compiler and document tests; migration fixtures under `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Animation/`) | the tool library `ZenithAuthoring`'s test target (ruling Q84 (a), X257; `Zenith2/Docs/Design/ANIM.md → "ZenithAuthoring ANIM files"`) | `Test` (`win`, the library's `ZENITH_TOOLS_HOST` matrix) |
| `Zenith_Integration_Animation` | `Zenith2/Tests/Animation/**` | `ZenithRuntime`, `ZenithAnimation`, `ZenithPhysics` with `ZenithPhysics_Jolt`, every ECS backend the preset supports, `ZenithCinematics` from Stage 4 | `Test`; the determinism cases also in stage acceptance |
| fuzz targets | `Zenith2/Engine/ZenithAnimation/Tests/Fuzz/*.cpp` → `Zenith_Fuzz_<Stem>` | the module's internals | `Sanitizers -Preset win-asan` (change-triggered under `Zenith2Memory` when ZenithAnimation is touched through the job-graph paths), stage acceptance at full duration |
| benchmarks | `Zenith2/Benchmarks/AnimEvaluation/`, `Zenith2/Benchmarks/ClipSampling/`, `Zenith2/Benchmarks/ClipCompression/` (one `zenith_add_benchmark` call each, [Z2-N-ARCH-157]), `Zenith2/Benchmarks/Budgets/ANIM/`, `Zenith2/Benchmarks/MotionMatching/` | as each needs | `Test` runs each with `--smoke` |

Every test runs headless (no GPU device): ANIM produces no GPU work, and the skinned rendering of its poses is GEOM's render test (`Test_GPUScene_SkinnedSameFrameAllPasses`, `Zenith2/Docs/Design/GEOM/Skinning.md → "Skinning tests"`), so no ANIM ticket is `needs-gpu`. Tests use real objects throughout: a real `Zenith_AssetRegistry` over generated cooked bytes, a real Jolt physics world, real ECS worlds; the only seam a test supplies is ANIM's own mesh-resolver interface, whose fixture implementation exists by design ("Extraction algorithm" of `Zenith2/Docs/Design/ANIM/Runtime.md`). `Zenith_AnimAttachTarget` (`Internal/Module/Zenith_AnimWorldState.h`) is ANIM's Internal adapter over the concrete `Zenith_WorldAttachContext`, implemented by the module alone.

The header-only test world `Zenith_AnimTestWorld` (`Tests/Support/Zenith_AnimTestWorld.h`) is a driver over a real headless runtime (ruling ANIM-R2-N5): it boots through `Zenith_AnimBootTestRuntime` (Physics on Jolt, Animation, ECS Custom) with its generated content and startup scene mounted through `Zenith_RuntimeTestAccess::AddTestMountSource`, runs tick-keyed actions from the test-only module's phase-2 system `AnimTest.Driver` (which records commands like any system), and runs tick-keyed assertions from `Zenith_RuntimeTestHooks::m_pfnAfterFrame`, reaching the world through `Zenith_RuntimeTestAccess::GetWorldSimulation`. The world, its `Zenith_ModulePublicationSink` and its `Zenith_AsyncResultTable` are therefore the objects `Zenith_WorldHost_CreateWorld` builds (`Zenith2/Docs/Design/MOD/RuntimeSeams.md → "Sink binding at world creation"`): no concrete class is doubled. Worker count, chunk capacity and forced compaction are `--set jobs.workerCount`, `ecs.chunkCapacityRows` and `ecs.forceCompaction`.

`Internal/Debug/Zenith_AnimationTestAccess.h` (included by `ZenithAnimation_Tests` only) exposes exactly: `GetResolveCallsOutsidePublication(const Zenith_AnimationWorld&)`, `GetCompletionIDsAllocatedOutsideMerge(const Zenith_AnimationWorld&)` (counters kept in every build), `SetRecoverFraction(Zenith_AnimationWorld&, uint64 ulSimulationID, float fFraction)` (overrides the fraction of an active recovery for the pivot oracle), `GetInFlightCounts(const Zenith_AnimationWorld&, uint32& uBindingLoads, uint32& uChunkReads)` (the residency's two in-flight counters), `GetUncompletedCompletionIDCount(const Zenith_AnimationWorld&)` (the IDs ANIM allocated and has not completed) and `ComputeScrewCoefficients(double fTheta, double fExponent, bool bSeries)` (forwards to `Zenith_ComputeScrewCoefficients`, `Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationMath header"`).

## Asset and format tests

File `Tests/Zenith_SkeletonAsset_Tests.cpp`, `Tests/Zenith_AnimationClip_Tests.cpp`, `Tests/Zenith_AnimationGraphAsset_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_SkeletonAsset_LoadsValidFile` | a generated 60-joint skeleton loads; joint count, parents, rest local and recomputed rest model match the generator; `GetResidentBytes` equals the block size |
| `Test_SkeletonAsset_RejectsJointLimit` | J = 0 and J = 1,025 fail with `ANIMATION_ERROR_CORRUPT`, J = 1,024 loads |
| `Test_SkeletonAsset_RejectsParentOrder` | a parent index ≥ its child fails |
| `Test_SkeletonAsset_RejectsBadRestTransform` | a NaN translation, a non-unit rotation and a scale of 1e-5 each fail |
| `Test_SkeletonAsset_RejectsInvalidRig` | a role naming joint J, a chain whose end is not a descendant, an IK definition with a broken chain each fail |
| `Test_SkeletonAsset_RejectsAsymmetricMirror` | a mirror table with `mirror(mirror(j)) ≠ j` fails |
| `Test_SkeletonAsset_RejectsShellRange` | a shell distance of 1e-4 m fails |
| `Test_SkeletonAsset_RejectsCorruptOffsets` | a name string offset past the section, a truncated section and a non-zero reserved field each fail, never assert |
| `Test_SkeletonAsset_FindJointBinarySearch` | every joint is found by its hash; an absent hash returns `uANIM_INVALID_INDEX` |
| `Test_AnimationClip_LoadsRawClip` | a generated 60-joint, 31-frame raw clip loads and samples to the generator's values exactly at every frame |
| `Test_AnimationClip_RejectsInconsistentFrames` | F inconsistent with D and rate, and a missing segment, each fail |
| `Test_AnimationClip_RejectsBadBitRate` | a bit-rate index of 19 and a bit-stream size mismatch each fail |
| `Test_AnimationClip_RejectsUnsortedEvents` | unsorted events, an event past D and a marker pair at one time each fail |
| `Test_AnimationClip_RejectsStreamTableGap` | a stream table that skips a segment fails |
| `Test_AnimationClip_StreamedHeaderSetOnly` | a streamed clip activates with no segment resident and maps every segment to its chunk |
| `Test_ClipSampling_PartialLastInterval` | for D = 1.01 s at 30 Hz, F = 32; sampling at 1.005 s interpolates frames 30 and 31 at α = 0.5 |
| `Test_ClipSampling_LoopWrap` | a loop clip sampled at D + 0.25 equals the sample at 0.25 |
| `Test_ClipSampling_NotResidentLeavesOutput` | a null segment returns `ANIMATION_ERROR_CLIP_NOT_RESIDENT` and leaves the output bytes unchanged |
| `Test_ClipSampling_SegmentBoundary` | sampling at frame 15 + 0.5 interpolates frame 15 (segment 0) and frame 16 (segment 1) and equals the raw value; with segment 1 null it returns not-resident; for a streamed clip whose chunk ends at segment 0, the prefetch window at t = 15.5 / rate contains the next chunk |
| `Test_AnimationGraphAsset_RejectsChildAfterParent` | a child index ≥ its parent fails |
| `Test_AnimationGraphAsset_RejectsOverlappingInstanceState` | two nodes with overlapping instance offsets fail |
| `Test_AnimationGraphAsset_RejectsPoseStackMismatch` | a stored depth unequal to the recomputed one fails |
| `Test_AnimationGraphAsset_ParameterLookup` | parameters are found by hash; reserved parameters are flagged |
| `Test_AnimationFormats_LimitsAtAndAboveLimit` | each limit of `Zenith2/Docs/Design/ANIM/Formats.md → "Parser limits"` loads at the limit and fails at limit + 1; the clip frame and segment limits are the implied ones, met at the limit by a generated one-joint raw clip of D = 3,600 s at 120 Hz (432,001 frames, 27,001 segments) and exceeded by a file declaring 432,002 frames or 27,002 segments, which fails as `ANIMATION_ERROR_CORRUPT` (CV1 or the limit, whichever the loader checks first) |

## Compression tests

File `Zenith2/Tools/ZenithCook/Tests/Animation/Zenith_ClipCompression_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_ClipCompression_ClassifiesTracks` | a fixture with rest, constant and animated channels classifies each as `DEFAULT`, `CONSTANT`, `ANIMATED` |
| `Test_ClipCompression_RangeReductionContainsSegment` | for 10,000 random segment ranges, the quantised segment range contains every value |
| `Test_ClipCompression_DequantiseSharedWithRuntime` | the cook's verifier and `Zenith_SampleClip` decode one segment to identical bits |
| `Test_ClipCompression_ErrorBoundHeldEveryFrame` | a property test over 200 generated rigs (10 … 200 joints, random hierarchies and lengths) and clips (random rotations, translations and scales, 1 … 5 s): the measured hierarchical error of every joint at every frame is ≤ ε, and the stored maximum equals the recomputed one |
| `Test_ClipCompression_Deterministic` | two cooks of one intermediate are byte-identical; a cook with 1 and with 8 workers is byte-identical |
| `Test_ClipCompression_RawFallbackWhenUnattainable` | with ε forced to 0 through the test settings every animated track is stored at index 18, the clip decodes to the raw f32 values and the step succeeds (every joint-frame meets its target `max(ε, E_raw)`) |
| `Test_ClipCompression_ClassificationErrorAccumulates` | a chain of five joints whose rotation channels are each constant within ε / 2 alone but whose summed displacement at the chain's end is 3 ε: cumulative classification keeps at least one of them `ANIMATED`, the combined classification error is ≤ ε / 2 at every joint and frame, and the clip cooks within ε |
| `Test_ClipCompression_RepairTerminatesAtRawFloor` | with ε = 1e-5 m on a clip whose root translates 1,000 m (f32 rounding above ε): the repair loop ends within its iteration bound, the affected joints leave the open set with every chain track raw, and verification passes against `E_raw` |
| `Test_ClipCompression_ClipConstantComponent` | a track rotating about +Y only (so x and z of every frame's canonicalised quaternion are 0, clip extent 0) and a translation track constant in y: at every bit-rate index 0 … 17 the cook writes both segment-range bytes of each clip-constant component as 0 and its bit-stream values as 0, divides by no zero extent (the step's floating-point environment raises no invalid-operation or divide-by-zero flag), the file passes CV3, and the decoder returns the clip minimum exactly for those components; a hand-built file with a non-zero byte for a clip-constant component fails CV3 with `ANIMATION_ERROR_CORRUPT`; an extent of 1e-31 is stored as 0 |
| `Test_ClipCompression_DemotionRestartsFromSegmentZero` | a fault point that forces one demotion while segment 2 of a 4-segment clip is optimised: the optimisation restarts from segment 0 with the demoted channel pinned at index 18 in every segment, segments 0 and 1 are re-optimised against the new set, verification passes, and the iteration count stays within `(1 + 3 × J) × S × (18 × A + J + 1)` |
| `Test_ClipCompression_SegmentAtClipMaximum` | a component whose segment values all equal the clip maximum quantises with `qMin = 254`, `qExt = 1` and decodes within ε; a constant segment at the maximum chooses index 0 only when its decoded constant meets the target; the segment-range part holds pairs for bit-rate indices 0 … 17 and its size passes CV3 |
| `Test_ClipCompression_AdditiveBoundOnReference` | an additive clip's verification measures the delta applied to its reference pose: the decoded delta applied at weight 1 to P stays within ε of the source frames at every virtual vertex, and a fault point that perturbs one delta rotation by 2 ε / d_j fails the step |
| `Test_ClipCompression_CurveErrorBound` | every animated curve decodes within ε_c at every frame |
| `Test_ClipCompression_RootMotionRaw` | the root-motion section equals the extracted motion bit for bit |
| `Test_ClipCompression_ViolationFailsStep` | a fault point that lowers one chosen rate after optimisation makes the step fail with `ANIM_COOK_DETAIL_ERROR_BOUND_VIOLATED` naming the joint and frame |
| `Test_ClipCompression_ShellDistanceFromMesh` | with a rig listing one mesh in `shellMeshes` whose published `skinShellDistance[j]` exceeds the rig's distance, the clip step measures joint j at the mesh's distance; a mesh not listed changes nothing; changing the listed mesh's property changes the clip's key and not the skeleton's; the skeleton step reads no mesh property (its recorded cook inputs) |
| `Test_ClipCompression_RatioReported` | the cook report member `animation.compression` holds raw bytes, cooked bytes, ratio, ε and the measured maximum |

## Import and cook tests

Files under `Zenith2/Tools/ZenithCook/Tests/Animation/` (`ZenithCook_Tests`), except the graph compiler and document tests, whose files are under `Zenith2/Tools/Authoring/Tests/Animation/` (`ZenithAuthoring_Tests`, the library that holds the compiler and the registration, ruling Q84 (a)):

| Test | File | Asserts |
|---|---|---|
| `Test_AnimImport_GroupsSkinsIntoSkeletons` | `Zenith_AnimImport_Tests.cpp` | two skins sharing a joint form one skeleton; a third, disjoint skin forms another |
| `Test_AnimImport_LowestCommonAncestorRoot` | same | without a `skeleton` node, the root is the lowest common ancestor |
| `Test_AnimImport_SyntheticRoot` | same | joints with no common ancestor get the identity `Root` |
| `Test_AnimImport_RestPoseFromInverseBinds` | same | the rest model of every joint equals the inverse of its inverse bind matrix within 1e-6; intermediate nodes derive from their children |
| `Test_AnimImport_RestPoseConflictRefused` | same | two skins binding one joint with different inverse binds fail with `ANIM_IMPORT_DETAIL_REST_POSE_CONFLICT` naming both skins and the joint, and no intermediate, mesh or skeleton product is written for the group |
| `Test_AnimImport_DuplicateJointNameRefused` | same | two joints named alike fail |
| `Test_AnimImport_ShearRefused` | same | a rest model with shear fails with `ANIM_IMPORT_DETAIL_REST_POSE_SHEAR` |
| `Test_AnimImport_ProductKeysStable` | same | reimporting with a renamed animation keeps the GUIDs of untouched products (ASSET's identity rules) |
| `Test_CookClip_ResampleStepLinearCubic` | `Zenith_CookClip_Tests.cpp` | each glTF interpolation resamples to the closed-form values (cubic spline with tangents scaled by the key interval) |
| `Test_CookClip_PartialLastFrame` | same | frame times are k / rate with the last at D |
| `Test_CookClip_LoopClosedAndWarned` | same | a loop clip's in-place last frame equals its first; a 10 × ε discontinuity reports the warning |
| `Test_CookClip_LoopRootMotionKeepsPerLoopMotion` | same | a looping walk travelling 1.2 m per loop along +Z (mode 1): `Q_{F−1}` and `Q(D)` translate 1.2 m within 1e-6, the in-place frame F − 1 equals frame 0, and the delta over two wraps is 2.4 m |
| `Test_CookClip_RootMotionModes` | same | modes 1, 2 and 3 produce the analytic Q for a fixture walking a quarter circle; the in-place pose stays at frame 0's virtual root |
| `Test_CookClip_RootMotionAncestorAnimatedRefused` | same | a motion joint with an animated ancestor fails |
| `Test_CookClip_AdditiveReferences` | same | first-frame, rest-pose and other-clip references produce deltas whose application to the reference reproduces the source |
| `Test_CookClip_EventsAndMarkersFromAnnotation` | same | the annotation's events and markers reach the cooked sections sorted, with their strings |
| `Test_CookClip_RetargetedProduct` | same | a retarget document yields a clip for the target skeleton equal to `Zenith_RetargetMap::Apply` frame by frame (in place); the product's root motion equals the source's Q with its translation scaled by the root scale and its rotation unchanged, within 1e-5; `anim.rawClip` holds the pre-extraction frames and the resolved settings; the product takes the source's loop flag, additive settings and compression settings, and a product importer setting in the retarget document's `.meta` overriding ε replaces the source's value for that product alone |
| `Test_CookClip_StreamsAboveThreshold` | `Zenith_CookClip_Tests.cpp` | a clip above the streaming threshold with streaming allowed is written with a stream table of 4-segment chunks; one below the threshold, or with streaming disallowed, is written resident |
| `Test_CookSkeleton_PublishesProperties` | `Zenith_CookSkeleton_Tests.cpp` | the nine published properties hold the documented bytes (`shellMeshes` empty without a rig member) |
| `Test_CookSkeleton_RigDocumentApplied` | same | roles, chains, IK definitions, sockets, mirror pairs, shell distances and curves of a rig document appear in the sections |
| `Test_CookSkeleton_RejectsEachRule` | same | one fixture per rule SV1–SV9 fails with the rule's detail |
| `Test_GraphCompiler_PostOrderAndSubtreeSizes` | `Zenith2/Tools/Authoring/Tests/Animation/Zenith_GraphCompiler_Tests.cpp` | nodes are in post-order with correct subtree sizes |
| `Test_GraphCompiler_PoseStackDepth` | same | the computed depth matches the formula of "Compile steps" step 5 for one fixture per node rule (a 3-child blend, a 6-state machine, a slot over a clip, a pose blend, a two-layer graph) and the loader recomputes the same integer; a graph of depth 17 fails |
| `Test_GraphCompiler_SharedNodeRefused` | same | a node with two parents fails; an orphan warns and is dropped |
| `Test_GraphCompiler_ParametersSortedAndReserved` | same | parameters are sorted by hash; a `Zenith.`-prefixed document parameter fails; referenced reserved parameters are added |
| `Test_GraphCompiler_AllErrorsInOneCompile` | same | a document with five distinct errors reports all five, sorted |
| `Test_GraphCompiler_MaskCompiled` | same | recursive entries and the entries after them apply in order; an unknown joint fails with `ANIM_GRAPH_DIAG_MASK` |
| `Test_GraphCompiler_SkeletonMismatchRefused` | same | a clip of another skeleton, not retargeted, fails |
| `Test_GraphCompiler_Deterministic` | same | two compiles are byte-identical |
| `Test_MotionMatch_DatabaseCookInputs` | `Zenith2/Tools/ZenithCook/Tests/Animation/Zenith_CookGraph_Tests.cpp` | for a graph with a node of kind 15 over two clips, the cooked `ANIM_SECTION_GRAPH_MOTION_DATABASE` equals `Zenith_MotionMatchDatabase::BuildFromFrames` over the clips' `anim.rawClip` frames after extraction and closure, bit for bit; the graph's key records each clip's `anim.rawClip` as a cook input, so editing one clip's motion changes the key and the database; `Build` over runtime clips cooked raw (index 18) equals `BuildFromFrames` over their decoded frames bit for bit |
| `Test_AnimDocuments_CanonicalRoundTrip` | `Zenith2/Tools/Authoring/Tests/Animation/Zenith_AnimDocuments_Tests.cpp` | each of the five document types round-trips byte for byte through the canonical writer (rule C11) |
| `Test_AnimDocuments_MigrationFixtures` | same | every `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Animation/<Type>/v<N>.json` migrates to its expected output ([Z2-N-CONTRACT-548]) |
| `Test_AnimDocuments_UnknownFieldsPreserved` | same | an unknown member survives open and save at its anchor with one warning |
| `Test_AnimDocuments_RegisteredToolSide` | same | the five types register into a tool `Zenith_MigrationRegistry` through `ZenithAuthoring`'s `Zenith_RegisterAnimationDocumentTypes`; `ZenithAnimation` links no `ZenithDocument` symbol (Libgraph); a graph document whose nodes and states carry `editor` members and whose root carries `editorComments` round-trips canonically with no unknown-field warning and compiles to sections and a graph hash identical to the same document without them (row ED17) |
| `Test_AnimImport_CurveClipIntermediateMatchesReader` | `Zenith_AnimImport_Tests.cpp` | a two-channel curve clip (a rotation with linear keys, a curve `viseme.AA` with step keys) written by ZenithAuthoring's `Zenith_WriteCurveClipIntermediate` reads back through ZenithCook's `Zenith_ReadClipIntermediate` channel for channel, bit for bit, flag bit 0 clear; the bytes cook through `anim.clip` with the cook test access and sample at three times within the clip's `curveError` |
| `Test_AnimAuthoring_CurveClipRefusesBadInput` | `Zenith2/Tools/Authoring/Tests/Animation/Zenith_AnimCurveClipWriter_Tests.cpp` (`ZenithAuthoring_Tests`) | `Zenith_WriteCurveClipIntermediate` refuses, appending nothing: times not strictly increasing (`ANIMATION_ERROR_INVALID_ASSET`), a value span of the wrong width, a NaN value, a duration of 0 and of 3,601 s, an empty name, path 5, interpolation 3, and 65,537 channels (`ANIMATION_ERROR_LIMIT_EXCEEDED`) |
| `Test_AnimContentRules_MeasuresRegistered` | `Zenith_AnimContentRules_Tests.cpp` | after `Zenith_RegisterAnimationCookSteps` the step registry holds exactly the three rules (Skeleton `bones`, Skeleton `ikChains`, AnimationGraph `layers`); over a cooked 70-joint skeleton with 3 IK definitions and a 2-layer graph the measures return 70, 3 and 2; a skeleton file without its info section makes the measure fail with `COOK_ERROR_STEP_FAILED` |
| `Test_AnimContentRules_BonesLimitRefusesCook` | `Zenith_AnimContentRules_Tests.cpp` | a fixture rule table whose row `CHARACTER_CLASS_CROWD` (selector `character-crowd`) limits `bones` to 64 fails the cook of a 70-joint skeleton labelled `character-crowd` with `COOK_ERROR_RULE` naming the table, the class, 64 and 70; the same skeleton unlabelled (class `*`, no limit) cooks |
| `Test_CookClip_PublishesMarkersProperty` | `Zenith_CookClip_Tests.cpp` | a clip with markers at 0.5 s and 0.1 s, one notify state and two instant events publishes `anim.clip.markers` of exactly `8 + 12 × 2 + 16 × 2` bytes, markers and events in ascending time, the notify state absent, payloads as their `i32` bit patterns; a clip with none publishes 8 zero bytes; a dependent test step reading it records a cook-input edge, so editing the annotation re-runs that step |
| `Test_AnimImport_ProductFingerprints` | `Zenith_AnimImport_Tests.cpp` | every declared `skel:` and `anim:` product gets `SetProductFingerprint` with the value of "Product fingerprints and rename recognition" of Formats.md, computed in the test from the tag and the identity description (a 3-joint skeleton and a 4-channel 1.5 s clip, both hand-computed); reordering the source's joints leaves the skeleton fingerprint unchanged |
| `Test_AnimImport_RenameRecognition` | `Zenith_AnimImport_Tests.cpp` | over previous products: a renamed root joint (one old, one new key, equal fingerprints) keeps the skeleton's GUID through `RenameProduct` and warns `ANIM_COOK_WARNING_RENAME_ASSUMED`; a renamed animation whose channel count changed gets a new GUID and `ANIM_COOK_WARNING_RENAME_NO_EVIDENCE`; two old and two new clip keys rename nothing and warn `ANIM_COOK_WARNING_RENAME_AMBIGUOUS` listing both sets |

## Graph tests

File `Tests/Zenith_AnimGraph_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_AnimGraph_UpdatePassAdvancesTimes` | clip times advance by Δt × rate × time scale; loop clips wrap and count wraps; non-loop clips clamp |
| `Test_AnimGraph_WeightCullingKeepsTimesAndEvents` | a blend whose child weight is 5e-5 produces the same times, events and root motion as with culling disabled |
| `Test_AnimGraph_ParameterCommandAppliedNextPhase` | a phase-2 `SetParameter` is seen by phase 3 of the same tick; a phase-5 one by the next tick |
| `Test_AnimGraph_UnknownParameterCommandDropped` | an unknown name and a wrong kind are dropped at playback and counted, never asserted |
| `Test_AnimGraph_TriggerHoldAndConsume` | a trigger with hold 2 survives one evaluation without a taker and clears after two; a taking transition consumes it |
| `Test_AnimGraph_HotReloadKeepsStateAndParameters` | a recompiled graph keeps the current state, node times and parameter values by stable ID; a removed state falls back to entry with an inertial take |
| `Test_BlendSpace1D_Weights` | weights at, between and outside samples match the closed form |
| `Test_BlendSpace2D_TriangulationDeterministic` | the triangulation of a shuffled sample list is identical; it is Delaunay (no sample inside any circumcircle) |
| `Test_BlendSpace2D_DegenerateRefused` | collinear samples and duplicate points fail to compile |
| `Test_BlendSpace2D_OutsideHullProjects` | a point outside the hull takes the nearest edge's linear weights |
| `Test_BlendSpace2D_WeightsContinuousAcrossEdges` | weights change by less than 1e-5 across a triangle edge and across the hull |
| `Test_SwitchNode_InertialOnChange` | a switch change with the inertial flag switches child at once, requests an instance take of the blend seconds, and the offset decays to 0 at the blend time |
| `Test_TimeScaleNode_ScalesAdvance` | a 0.5 time scale halves the advance of every clip below it |
| `Test_AnimationPreview_MatchesWorldEvaluation` | a preview and a world instance given the same parameters and Δt produce bit-identical poses, events and root motion for 600 evaluations |

## Blend and pose tests

File `Tests/Zenith_AnimationPose_Tests.cpp` and `Tests/Zenith_AnimationMath_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_PoseBlend_JointOnPivotFixed` | a point at a joint's origin that two poses place identically stays within 1e-6 m at weights 0, 0.25, 0.5, 0.75 and 1, for a 90° shoulder difference |
| `Test_PoseBlend_NWayMatchesPairwise` | a two-input N-way blend equals `Blend` bit for bit |
| `Test_PoseBlend_DegenerateRotationCounted` | opposing rotations yield the first input's rotation and increment `anim.blend.degenerate_delta` |
| `Test_AdditivePose_LocalRoundTrip` | applying a clip's local additive to its reference reproduces the source within 1e-6 |
| `Test_AdditivePose_ModelSpaceAboutJointOrigin` | a model-space additive rotates each joint about its own model origin |
| `Test_PoseMasked_ModelRotationWorldStable` | a model-space override layer keeps the masked joints' model rotations while the hips turn |
| `Test_LocalToModel_MatchesReference` | composition equals a `double` reference within 1e-5 for 1,000 random 100-joint poses |
| `Test_MirrorPose_Involution` | mirroring twice returns the pose exactly |
| `Test_DualQuaternion_RigidRoundTrip` | rigid → dual quaternion → rigid round-trips within 1e-6 |
| `Test_DualQuaternion_BlendKeepsSharedPivot` | two rotations about one pivot P (30° and 120°) blended at 0, 0.25, 0.5, 0.75, 1 leave P within 1e-6 m |
| `Test_DualQuaternion_LinearTranslationBlendMovesPivot` | the negative control: slerped rotation with linearly blended translation moves P by more than 0.05 m at 0.5 for the same inputs, so the pivot test can fail |
| `Test_DualQuaternion_PowComposesExactly` | `Pow(D, 1/k)` composed k times equals D within 1e-5 for k = 2, 4, 8 and screw motions with pitch |
| `Test_DualQuaternion_BlendSignAlignment` | inputs given as q and −q blend as the same rotation |
| `Test_DualQuaternion_PowCanonicalisesSign` | for 1,000 random screws, `Pow(dq, e)` and `Pow(−dq, e)` are bit-identical at e = 0.25, 0.5, 0.75, including `real.w = 0` inputs; a 350° turn about +Y powers at 0.5 to a −5° turn (the short way), within 1e-6 |
| `Test_DualQuaternion_PowNearIdentityFinite` | screws built in `double` from an angle θ ∈ {1e-9, 1e-6, 0.999e-3, 1.001e-3} rad, a random unit axis l, a point p on the axis line with |p| ≤ 1 m and a pitch giving a 10 m translation, rounded to a float input, at e ∈ {0, 0.25, 0.5, 1, 1.5}: every result is finite; its rotation is within 1e-6 rad of the reference rotation e θ about l, and its translation, decomposed in `double` from the float result, is within `1e-6 × |t| + 1e-7 m` (1.01e-5 m) of the reference `e × pitch × l + (I − R(e θ)) p` computed in `double` (one f32 ulp at 10 m is 9.5e-7 m, and the float input and output each round once, so no tighter bound is attainable); a pure translation (θ = 0) powers to `e t` within 1 ulp per component; e = 0 gives the identity exactly |
| `Test_DualQuaternion_PowSeriesBranchContinuity` | through `Zenith_AnimationTestAccess`, `Zenith_ComputeScrewCoefficients` evaluates both branches in `double` at the same θ ∈ {1e-3, 0.999e-3, 0.5e-3} for e ∈ {−0.5, 0, 0.5, 1, 1.5, 2}: the translation difference per metre of |t|, `|Δc1| + |Δc2| × s² + |Δc3| × s` (s = sin(θ/2)), is at most 1e-12 (the series' measured error is below 5e-14); the rotation factor k differs by at most 1e-14 |
| `Test_DualQuaternion_PowHalfTurn` | a 180° screw with pitch: `Pow(dq, 0.5)` composed twice equals dq within 1e-6 |

## State machine tests

File `Tests/Zenith_AnimStateMachine_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_StateMachine_AnyStateFirstThenPriority` | an any-state and a state transition both passing: the any-state one is taken; two state transitions: the first in priority order |
| `Test_StateMachine_ConditionOperators` | each of the ten operators passes and fails on boundary values, exactly |
| `Test_StateMachine_ExitTimeCrossing` | an exit time of 0.9 passes in the evaluation whose advance crosses 0.9 and not before, including across a loop wrap |
| `Test_StateMachine_InterruptSources` | `NONE`, `CURRENT`, `NEXT`, `CURRENT_THEN_NEXT` and `NEXT_THEN_CURRENT` each admit exactly their candidate lists |
| `Test_StateMachine_CrossfadeWeightsSumToOne` | with three stacked records, the state weights sum to 1 and match the fold |
| `Test_StateMachine_FifthInterruptBecomesInertial` | a fifth interrupt clears the records, leaves the machine `IDLE` in the destination and requests an instance take of the candidate's duration; the jump of every joint at the take, `|out − x_prev|`, is at most the quintic's own first step `max over channels |x0 − x(Δt)|` plus 1e-5 m |
| `Test_StateMachine_ZeroDurationChainBounded` | a cycle of zero-duration transitions takes four per evaluation and never loops |
| `Test_StateMachine_ForceStateAndReset` | `ForceState` enters the named state with an instance take of its seconds (none for 0 s); `ResetGraph` returns to the entry state with times 0 and clears the pending take and the active offset |
| `Test_StateMachine_StateChangedEventEmitted` | each take emits one `Zenith.Animation.StateChanged` with the from and to hashes |
| `Test_InertialBlend_QuinticMatchesReference` | the offset curve equals the closed-form quintic at 100 sample times |
| `Test_InertialBlend_OffsetReachesZeroAtT1` | the offset and its velocity are 0 at t1 and stay 0 |
| `Test_InertialBlend_VelocityFromHistoryAtReducedRate` | a source joint moving at a constant 2 m/s, evaluated at k = 4 (Δt_last = 4 ticks): an inertial take records `v0` equal to the closed-form velocity along the offset within 1e-5, reading the current and previous history poses before they rotate; the offsets of the next three evaluations equal `x(t)` recomputed from the stored `(x0, v0, t1)` at their elapsed times within 1e-6; the buffer is `48 × J + 4 × C + 16` bytes, one per instance |
| `Test_InertialBlend_NoPopUnderOverrideLayer` | a state machine in layer 0 taking an inertial transition (T = 0.2 s) between two static poses under an override layer of weight 0.5 masked to the arms (pose L): the recorded offsets equal `x_prev − G` with G the layered output, and the jump of every joint at the take, `|out − x_prev|`, is at most the quintic's first step `max over channels |x0 − x(Δt)|` plus 1e-5 m, masked arm joints included (a machine-level offset would add `0.5 × (x_prev − L)` there); with an additive layer above the machine instead, the additive appears once in the output at the take |
| `Test_InertialBlend_ForceStateWithoutInertialTransitions` | a graph whose state machine has no inertial transition: `ForceState(machine, state, 0.2)` takes through the instance's inertial buffer (allocated because the graph `Inertialises()`), the jump bound of the previous test holds, the offset is 0 after 0.2 s, and a graph of clip and blend nodes only has no buffer (`Inertialises()` false) |

## Sync event and root motion tests

Files `Tests/Zenith_AnimSync_Tests.cpp`, `Tests/Zenith_AnimEvents_Tests.cpp`, `Tests/Zenith_AnimRootMotion_Tests.cpp`, `Tests/Zenith_AnimSlots_Tests.cpp`, `Tests/Zenith_AnimLayers_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_SyncGroup_LeaderHighestWeight` | the leader is the highest weight, ties to the lower node index |
| `Test_SyncGroup_NormalisedCadence` | a walk (1.0 s) and run (0.6 s) blend at 0.5 advance with `D_eff = 0.8 s` and keep equal phase |
| `Test_SyncGroup_MarkerAlignment` | a follower with markers at other times sits at the same marker pair and fraction as the leader |
| `Test_SyncGroup_MarkerFallbackCounted` | a follower lacking a marker name falls back to normalised time and increments the counter |
| `Test_SyncGroup_JoinerEntersInStep` | a member whose weight rises from 0 starts at the group's position |
| `Test_AnimEvents_LoopWrapFiresOncePerCrossing` | an event at 0.1 s in a 1 s loop advanced 2.5 s from 0.05 fires three times |
| `Test_AnimEvents_BackwardPlayback` | a negative rate fires events in `(t', t]` in descending time order |
| `Test_AnimEvents_FirstEvaluationIncludesZero` | an event at exactly 0 fires on the clip's first evaluation |
| `Test_AnimEvents_NotifyStateEndOnExit` | a notify state open when its state is left fires its end in that evaluation |
| `Test_AnimEvents_WeightGate` | a `WEIGHTED` event with minimum 0.25 fires at path weight 0.3 and not at 0.2 |
| `Test_AnimEvents_LeaderOnly` | a `LEADER_ONLY` event fires from the sync leader only |
| `Test_AnimEvents_OrderAndCap` | events come in (layer, node, crossing, index) order; the 65th is dropped and counted |
| `Test_AnimEvents_DeliveredInPhaseNine` | a fired event is delivered in phase 9 of the same tick with its payload and time into tick |
| `Test_RootMotion_DeltaWithWraps` | the delta over two wraps of a loop clip equals the composed closed form |
| `Test_RootMotion_BackwardIsInverse` | the backward delta from t' to t is the inverse of the forward one |
| `Test_RootMotion_BlendViaDualQuaternions` | blending two turning clips' deltas keeps the shared pivot (the arc's centre) fixed |
| `Test_RootMotion_PhaseThreeNeverApplies` | `Zenith.Animation.RootMotion` holds the delta after phase 3 and no phase-3 or phase-6 ANIM node writes the entity's `Zenith.LocalTransform` |
| `Test_RootMotion_AppliedInPhaseFourWithoutController` | an entity with root motion and no character controller (rotation 30° about +Y, scale 2) moves at sync point 4 to exactly `p + r · Δp` with rotation `normalize(r · Δq)`, where the published `DeltaPosition` Δp already equals the clip's model-space delta times s; phase 5 sees the moved transform; a reset (teleport) delta moves nothing; `IGNORE` mode moves nothing |
| `Test_RootMotion_ControllerEntitiesLeftToPhysics` | an entity carrying `Zenith.Physics.CharacterController` (the PHYS type, assumption pending PHYS) receives no command from `Animation.ApplyRootMotion`; the compiled schedule has no edge between `Animation.ApplyRootMotion` and `Physics.CharacterMove` and no hazard between them |
| `Test_RootMotion_IgnoreModeZero` | `IGNORE` publishes the identity |
| `Test_RootMotion_EntityScaleAppliedOnce` | two root entities of scale (2, 2, 2) playing a clip that travels 1 m along +Z, one without a character controller and one with PHYS's `Zenith.Physics.CharacterController` on flat ground (assumption pending PHYS, row AN6): `DeltaPosition` reads 2 m for both, and each entity moves 2 m within 1e-5 m, so the scale is applied once whichever node moves it; a reduced-rate instance (k = 4) publishes steps whose translations sum to 2 m |
| `Test_AnimSlot_PlayAfterBinding` | a `PlaySlot` of an unbound clip starts the tick after its publication; of a cached clip, in the same evaluation |
| `Test_AnimSlot_BlendInOut` | the slot weight follows the smoothstep ramps and the slot empties at the end |
| `Test_AnimSlot_ClipChangeIsInertial` | a `PlaySlot` while the slot plays another clip switches to the new clip at once with the weight kept, requests an instance take of blendIn seconds, and the jump of every joint at the change is within the quintic's first step plus 1e-5 m; the slot state stays 32 bytes |
| `Test_AnimSlot_SetTimeSkipsEvents` | `SetSlotTime` moves the time without firing the events between |
| `Test_AnimSlot_RootMotionWeighted` | a slot at weight 0.5 contributes half the root-motion blend |
| `Test_AnimLayers_OverrideAndAdditiveOrder` | layers apply in order; swapping two layers changes the result as defined |
| `Test_AnimLayers_ZeroWeightLayerSkipped` | a weight-0 layer's machine time does not advance |
| `Test_AnimMask_RecursiveEntriesOverride` | an entry overrides a recursive entry before it |
| `Test_LayeredNode_MatchesLayers` | a `LAYERED` node with the layers' overlays equals the layer stack |
| `Test_MirrorNode_MirrorsPoseAndRootMotion` | a mirrored clip's pose is `Zenith_MirrorPose` of the clip and its root motion is reflected |

## Ragdoll and lifecycle tests

Files `Tests/Zenith_AnimRagdoll_Tests.cpp` (with a Jolt world), `Tests/Zenith_AnimLifecycle_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_RagdollOwnership_PrecedenceCinematicRagdollAnimation` | with all three writers, the cinematic value wins; without it, the ragdoll's; otherwise 0 |
| `Test_RagdollEnter_SnapshotsFinalPose` | the bodies are created at the final pose of the tick of the command's sync point |
| `Test_RagdollBlend_LocalBlendKeepsPivots` | at w = 0.5, every joint shared by both poses at the same model position stays there |
| `Test_RagdollRecover_FacingAndTrigger` | a prone ragdoll sets `Zenith.RagdollFacing = 1` and the trigger; the get-up slot plays the payload's clip |
| `Test_RagdollRecover_OffsetBlendKeepsHipsPivot` | an independent oracle: a fixture whose offset O is a known screw (a 90° rotation about a vertical axis through a known point P, 1 m from the model origin, plus a 0.1 m pitch along that axis), with the recovery fraction set through `Zenith_AnimationTestAccess::SetRecoverFraction` and the arbitrated weight held at 0 so the final pose is the offset animated pose; at fractions 0, 0.25, 0.5, 0.75, 1 the final hips equal the closed form (rotation (1 − p) × 90° about the axis through P, translation (1 − p) × 0.1 m along it, applied to `H_anim`) within 1e-5 m, P maps onto the axis at height (1 − p) × 0.1 m, and every inter-joint distance of the final pose equals the animated pose's within 1e-6 m |
| `Test_RagdollRecover_LinearOffsetBlendMovesPivot` | the negative control on the same fixture: blending O towards the identity with `Zenith_Nlerp` rotation and linearly scaled translation moves P off the axis by more than 0.05 m at p = 0.5 (0.29 m for the 1 m lever), so the pivot test can fail |
| `Test_RagdollRecover_NoDoubleOffsetAtFirstTick` | in the Jolt world, through PHYS's real `Physics.RagdollBlend` (recover blend 0.5 s at 30 Hz, from ACTIVE; PHYS's recover weight premise of "Ragdoll transitions", pending PHYS revision 1): at the first `Animation.PostPhysicsBlend` after the event the weight is exactly 1, p = 0, and the final pose equals the ragdoll pose bit for bit (no offset applied to the ragdoll's contribution), with A's hips equal to `H_ragdoll` within 1e-5 m; at the next tick the weight is `1 − 1/15`; at a middle tick it equals `Blend(Pow(O, 1 − p) · anim, ragdoll, w)` with `p = 1 − w / w_1`; at the tick PHYS's weight reaches 0 it equals the animated pose bit for bit and the offset state is cleared; a recover from ENTERING at w = 0.6 stores `w_1 = 0.6` and also ends with p = 1 at w = 0; a recover blend of 0 s from ACTIVE gives, at the first tick, w = 1, p = 0 and a final pose equal to the ragdoll pose bit for bit, and at the second tick state NONE, the offset state cleared and a final pose equal to the animated pose bit for bit |
| `Test_RagdollRecover_LargeOffsetRejectedCounted` | an offset of 0.6 m, and one of 100°, recover with no offset and count `anim.ragdoll.recover_offset_rejected_delta` once; an offset of 0.4 m and 80° is applied |
| `Test_RagdollOwnership_ReadsThisTickWeight` | `Animation.RagdollOwnership` runs after PHYS's `Physics.RagdollBlend` in the compiled schedule, and the arbitrated weight of tick t equals PHYS's weight of tick t (no one-tick lag) |
| `Test_AnimLifecycle_TeleportResetsRootAndHistory` | after a teleport: root-motion spread state zero, graph reset, history reset in the next extraction |
| `Test_AnimLifecycle_TeleportKeepPhase` | with `TELEPORT_FLAG_KEEP_ANIMATION_PHASE` the graph times are kept |
| `Test_AnimLifecycle_CinematicSkipInertialNoPop` | at skip, cinematic slots empty and the instance takes a 0.15 s instance-level inertial take; the jump of every joint at the skip is at most the quintic's first step plus 1e-5 m, and the offset is 0 after 0.15 s |
| `Test_AnimLifecycle_CinematicInterruptBlendsOut` | at interrupt, cinematic slots blend out over their blend-out times |
| `Test_AnimInstance_LifecycleTransitions` | every row of the instance state table |
| `Test_AnimInstance_TableOrderIndependentOfLayout` | with forced compaction and two chunk capacities, instance creation order and completion IDs are identical |
| `Test_AnimInstance_CapRefusesInSimulationOrder` | with `animation.maxInstances = 4` and six animators, the four lowest simulation IDs run |

## IK motion warping and retarget tests

Files `Tests/Zenith_AnimIK_Tests.cpp`, `Tests/Zenith_AnimMotionWarp_Tests.cpp`, `Tests/Zenith_AnimRetarget_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_TwoBoneIK_ReachesInsideRange` | for 10,000 random reachable targets the end lands within 1e-4 m and bone lengths are kept within 1e-5 |
| `Test_TwoBoneIK_ClampsOutOfReach` | an unreachable target straightens the chain toward it and reports the error |
| `Test_TwoBoneIK_PoleSelectsBendPlane` | the middle joint bends toward the pole |
| `Test_TwoBoneIK_DegenerateInputsStable` | a target on the root, a pole along the chain and a straight chain produce finite rotations |
| `Test_TwoBoneIK_SoftnessLimitsSnap` | with softness 0.05 m the end's velocity near full extension stays bounded |
| `Test_ChainIK_ConvergesWithinTolerance` | a 6-bone chain reaches reachable targets within 1 mm in at most 10 iterations |
| `Test_ChainIK_StretchesWhenUnreachable` | an unreachable target gives a straight chain toward it |
| `Test_ChainIK_PreservesTwist` | the joints' twist about their bones is unchanged |
| `Test_LookAt_SharesAndLimits` | the shares split the aim; no joint exceeds 70° |
| `Test_FootPlacement_PelvisAndAnkles` | on a 10° slope fixture, both soles meet the ground within 5 mm and the pelvis lowers by the lower foot's offset |
| `Test_FootPlacement_MissLeavesFoot` | a probe miss leaves the foot and pelvis unchanged |
| `Test_IKStage_OrderFootFirst` | foot placement runs before hand goals, which see the moved pelvis |
| `Test_MotionWarp_ReachesStaticTarget` | the entity, moved by the published deltas through a test integrator, ends the window within 1 cm and 1° of the target |
| `Test_MotionWarp_TracksMovingTarget` | a target moving at 1 m/s is reached within 2 cm |
| `Test_MotionWarp_OutsideWindowUntouched` | outside the window the delta equals the clip's |
| `Test_Retarget_RestToRestIsIdentity` | the source rest pose maps to the target rest pose exactly |
| `Test_Retarget_TPoseToAPose` | a T-posed source clip on an A-posed target keeps the target's rest at the source's rest and carries an arm raise by the same model-space angle |
| `Test_Retarget_RoleAndChainMapping` | joints map by role, then by chain position, then overrides win |
| `Test_Retarget_RootScaleByHipsHeight` | the root translation scales by the hips-height ratio |
| `Test_Retarget_NoMeshOrRestPoseChange` | retargeting leaves both skeleton assets' bytes and every mesh untouched (the API takes poses only) |
| `Test_Retarget_UnsupportedWithoutProfile` | an incomplete profile with no overrides fails `Create` with `ANIMATION_ERROR_RETARGET_UNSUPPORTED` |
| `Test_Retarget_GraphClipThroughBindingMap` | a graph on skeleton B playing a clip of skeleton A flagged retargeted gives, at every frame, the pose `Apply` gives on the sampled source; two instances share one map; the map is freed with its last binding |

## Facial and streaming tests

Files `Tests/Zenith_AnimFacial_Tests.cpp`, `Tests/Zenith_AnimStreaming_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_FacialCurves_BlendLikeJoints` | curves blend, mask and layer by the documented rules |
| `Test_MorphRemap_ByNameHash` | curves reach the mesh targets with the same name hash; targets without a curve take 0 |
| `Test_PoseLibrary_AdditiveFromNeutral` | a viseme at weight 1 reproduces its library frame; two at 0.5 add their deltas |
| `Test_CurveOverride_EightAndReplace` | a ninth override replaces the lowest-weight one |
| `Test_PoseFollower_UsesLeaderPose` | a follower mesh's skinned pose equals its leader's |
| `Test_PoseFollower_RefusesOtherSkeleton` | a follower of another skeleton is refused and counted |
| `Test_AnimStreaming_WindowRequestsChunks` | playing a 30 s streamed clip requests the chunks covering the next 2 s in play order |
| `Test_AnimStreaming_StallHoldsTime` | a delayed chunk holds the clip time and fires no event until it publishes |
| `Test_AnimStreaming_EvictsOutsideWindowLRU` | chunks outside every window for 60 ticks are freed, least recently used first |
| `Test_AnimStreaming_CacheFullDefers` | with the cache full of in-window chunks, a new request defers and counts |
| `Test_AnimStreaming_FailurePublishedHolds` | a failed chunk read holds the time at the chunk boundary and logs once |
| `Test_AnimStreaming_CancelOnDetach` | detaching a world cancels its reads and frees nothing before they complete |
| `Test_AnimStreaming_PrefetchMakesClipReady` | a prefetched streamed clip becomes ready before it plays; `IsClipReady` and a `CLIP_READY` transition agree; the window entry expires after 300 ticks unplayed |
| `Test_AnimStreaming_CancelReloadStress1000` | `z2-stress`: 1,000 cycles of starting a streamed clip, cancelling at a random stage (request, read, publication, residency) and reloading the world leave no instance, reference, operation or allocation behind ([Z2-N-CONTRACT-339]), and after every cycle `Zenith_AnimationTestAccess::GetUncompletedCompletionIDCount` is 0 (every completion ID ANIM allocated was completed once, the abandoned ones with the abandoned payload) |

## Budgeting and extraction tests

Files `Tests/Zenith_AnimSignificance_Tests.cpp`, `Tests/Zenith_AnimExtract_Tests.cpp`, `Tests/Zenith_AnimBindings_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_AnimSignificance_TiersFromDistance` | distances 10, 20, 50 and 100 m give k = 1, 2, 4, 8 at the default keys; a bias of 4 moves 50 m to k = 2 |
| `Test_AnimSignificance_MainCameraByPriority` | with two active MAIN cameras, the nearer of lower priority and the farther of higher priority, distances are measured to the higher-priority one (`Flux_SelectMainCamera`'s choice), whatever the chunk order |
| `Test_AnimSignificance_PhaseOffsetsSpread` | 64 instances at k = 4 spread within one of 16 per tick |
| `Test_AnimSignificance_ForcedFullRateCases` | a ragdoll, a cinematic slot, a warp window, the first evaluation and the force flag each give k = 1 |
| `Test_AnimSignificance_CapDemotesLowest` | with a cap of 10 and 12 due instances, the two least significant demote, ties by higher simulation ID |
| `Test_AnimSignificance_IgnoresQualityTier` | changing `quality.tier` and the thermal state changes no rate decision and no tick hash; the budget keys refuse the quality-tier and user layers |
| `Test_AnimSignificance_ReducedRootMotionComposesExactly` | the k published steps compose to the span's delta within 1e-6 |
| `Test_AnimSignificance_TriggersSurviveSkippedTicks` | a trigger set while an instance skips evaluations is seen at its next evaluation |
| `Test_AnimExtract_KeyMatchesInstanceExtractor` | the pose key equals `Render.ExtractInstances`' key for the same entity |
| `Test_AnimExtract_InterpolationFullRate` | at α = 0.5 the extracted pose is the local midpoint of the last two final poses |
| `Test_AnimExtract_InterpolationReducedRate` | at k = 4 the extracted pose moves continuously across evaluation ticks |
| `Test_AnimExtract_HistoryResetOnTeleport` | a teleported instance is written with the history-reset flag and no interpolation |
| `Test_AnimExtract_SkeletonMismatchRefused` | a mesh of another skeleton gets no pose and one count |
| `Test_AnimExtract_SlotFullCounted` | a full slot arena drops the pose and counts |
| `Test_AnimExtract_RemapCacheWorkerIndependent` | with `animation.extract.maxRemapTables = 16` and 40 distinct (mesh, skeleton) pairs cycling over 600 frames (the fixture resolver): extracted morph weights, the remap counters (`remap_overflow` included) and the cache's contents after each frame are identical with 1 and 8 workers; tables are built, stamped and evicted only in the extractor node's body before its leaves run; a skeleton-mismatch refusal is counted and logged once per (entity, mesh) whatever the worker count; runs under `win-asan` with no report |
| `Test_AnimExtract_HeadlessWritesNothing` | headless, the production resolver yields no runtime mesh and no pose is written, with no per-frame retry cost beyond the availability check |
| `Test_AnimBinding_PublishedAtTickBoundary` | an animator's graph binds at a tick boundary after its load completes, never inside a phase |
| `Test_AnimBinding_CompletionIDsInSimulationOrder` | completion IDs are allocated in ascending simulation ID order for any chunk layout, all inside the merge node of `Animation.Significance` (`Zenith_AnimationTestAccess::GetCompletionIDsAllocatedOutsideMerge()` is 0); the compiled order `Animation.Significance` → its merge node → `Animation.Evaluate` is asserted by `Test_AnimationModule_ScheduleCompiles` ("Integration scenarios") |
| `Test_AnimBinding_MismatchFailsToRestPose` | a graph whose clip has another rest pose hash fails binding and the extractor writes the rest pose |
| `Test_AnimBinding_CacheBindsImmediately` | a second animator of a bound graph binds in `Animation.Significance` of the same tick without a request or completion ID, its instance block allocated there; a forced allocation failure makes it `DISABLED` |
| `Test_AnimBinding_EventNamesPerWorld` | two worlds with different string tables bind one clip: the clip's bytes are unchanged after both bindings, and each world's `Zenith.Animation.Notify` events carry that world's interned name IDs |
| `Test_AnimBinding_InFlightCapDefersInOrder` | with `animation.bindingQueueCapacity = 64` and 70 animators of distinct graphs created in one tick: the 64 lowest simulation IDs issue on the first tick, the other six issue on subsequent ticks in ascending simulation ID as publications free the counter, `anim.bindings.deferred_delta` counts the deferrals, no completion ID is allocated for a deferred need, `anim.loads.w<tag>.full` stays 0, and the completion IDs, issue ticks and bind ticks are identical with 1 and 8 workers; with `animation.streaming.maxReadsInFlight = 16` and one clip needing 20 chunks, 16 reads issue and four defer in (clip GUID, chunk) order, each releasing its reservation (`Zenith_AnimationTestAccess::GetInFlightCounts` never exceeds either bound) |
| `Test_AnimBinding_NoResolveInPhaseNodes` | `Zenith_AnimationTestAccess::GetResolveCallsOutsidePublication()` is 0 after 600 ticks of a populated world |
| `Test_AnimSignificance_RateDivisorMatchesTiers` | `Zenith_AnimComputeRateDivisor` at the default distances returns 1 for r = 0, 15 (inclusive), 2 for 15.001 and 40, 4 for 40.001 and 80, 8 for 80.001 and 10,000; bias 2 halves r (d = 30 → 1); `bForceFullRate` gives 1 at any distance; a NaN or negative distance gives 1; bias 0 and 1,000 clamp to 0.01 and 100; a non-increasing triple (40, 15, 80) collapses (d = 30 → 1, d = 60 → 4); `Animation.Significance` assigns every instance exactly the function's value for 100 instances at random distances (it calls the function, not a copy) |
| `Test_AnimSignificance_FullRateSlotTagForcesRate1` | an instance at 200 m (divisor 8) playing a slot with tag `ANIM_SLOT_TAG_FULL_RATE` is evaluated every tick from the binding's publication until the slot's blend-out completes, then returns to 8 at its next due tick; the same with `ANIM_SLOT_TAG_CINEMATIC`; a tag of bit 13 alone does not force |

## Module binding and failure tests

Files `Tests/Zenith_AnimationModule_Tests.cpp`, `Tests/Zenith_AnimationBindings_Tests.cpp`, `Tests/Zenith_AnimSave_Tests.cpp`, `Tests/Zenith_AnimDiagnostics_Tests.cpp`, `Tests/Zenith_AnimationMemory_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_AnimationModule_Create_FailsAtRegisterTypes1_Unwinds`, `Test_AnimationModule_Create_FailsAtRegisterTypes2_Unwinds`, `Test_AnimationModule_Create_FailsAtStart1_Unwinds`, `Test_AnimationModule_Create_FailsAtStart2_Unwinds`, `Test_AnimationModule_Create_FailsAtStart3_Unwinds`, `Test_AnimationModule_Create_FailsAtStart4_Unwinds`, `Test_AnimationModule_Create_FailsAtAttachWorld1_Unwinds`, `Test_AnimationModule_Create_FailsAtAttachWorld2_Unwinds`, `Test_AnimationModule_Create_FailsAtAttachWorld3_Unwinds`, `Test_AnimationModule_Create_FailsAtAttachWorld4_Unwinds` | ten tests, one per step of the entry-point table (`Zenith2/Docs/Design/ANIM.md → "Zenith_Module_Animation header"`), each failing that step alone, asserting the error, zero leaks and no registration, configuration callback, world resource or save participant left ([Z2-N-CONTRACT-341]) |
| `Test_AnimationModule_HeadlessRuns` | a headless world evaluates animators, publishes root motion and events, and registers the extractor |
| `Test_AnimationModule_AuthorityTableRegistered` | every property's authority and the arbitration declaration match "Field authority table" |
| `Test_AnimationBindings_SettersBufferCommands` | each setter records its command; a failing invocation discards it |
| `Test_AnimationBindings_GettersReadCommitted` | getters read the latest committed evaluation, not in-flight state |
| `Test_AnimationBindings_MisuseRaisesLuauError` | each error row of "Luau bindings" raises a controlled Luau error naming the argument, never an assert, with its code ZS2001 … ZS2013 and reason text ("Misuse codes" of `Zenith2/Docs/Design/ANIM/Runtime.md`) |
| `Test_AnimationBindings_EventsReachScripts` | a Late-scripts behaviour receives `Zenith.Animation.Notify` with typed fields |
| `Test_AnimSave_RecordRoundTrip` | save and restore of 20 animators (blend spaces, a crossfade and an inertial blend in progress, a slot playing, two reduced-rate instances not due at the first restored tick, one recovering ragdoll) reproduce the next 600 tick hashes of the uninterrupted run, and the pose bytes of every instance |
| `Test_AnimSave_RestoreBindsBeforeFirstTick` | restored into a world whose asset registry holds none of the assets: `IsRestoreReady` returns false until the loads complete, the world's first tick runs only after it returns true, and at that tick every saved `BOUND` instance is `BOUND`; a second case runs the restore through SAVE's synchronous restore driver with no frame running, servicing only the handles `ListRestoreWaits` returns (restore-queue loads in ascending GUID, then restore chunk reads in (GUID, chunk) order), and the restore completes with the same instance states; `ListRestoreWaits` returns 0 once ready; a third case restores a save with no ANIM section (`m_bSectionPresent` false): no instance is restored, `IsRestoreReady` is true at once, and the animators bind through the ordinary path from the first tick |
| `Test_AnimSave_RestoreAllocatesNoCompletionID` | the world's next completion ID after restore equals the uninterrupted run's at the same tick, and ANIM's publication sink records no allocation during restore |
| `Test_AnimSave_InFlightReissuedWithRecordedIDs` | a save taken with one binding and two chunk reads in flight re-issues exactly those three under their recorded completion IDs, which publish at the recorded ticks in a replay |
| `Test_AnimSave_ParkedCompletionPublishesAfterRestore` | a save in a tick whose phase 3 parked one binding success, one chunk-read success and one chunk-read failure: SAVE's async section holds the three as unpublished; after restore the binding's asset is ACTIVE and the success chunk's block filled before the first restored tick, all three publish at the boundary of t + 1 exactly as in the uninterrupted run (same bindings, residency and stall counters), the next 300 tick hashes equal the uninterrupted run's, and no completion ID is allocated or re-issued for them |
| `Test_AnimSave_ReissueDeclaredAndMarked` | the participant's registration declares `GetReissuedSourceKinds() == 1u << ASYNC_SOURCE_ANIMATION` (bit 9) and `GetFlags() == 0`; a restore of a save with one binding and two chunk reads issued and not completed calls `MarkReissued` for each of the three before re-issuing it, and SAVE completes none of them with the abandoned payload; a save whose prologue lists a state-0 entry absent from SAVE's async section (`WasInFlight` false) fails `RestoreSection` with `SAVE_ERROR_CORRUPT`; an ANIM in-flight entry forced out of the prologue stays unmarked, SAVE completes it with the zero-length payload, and ANIM's handler drops it as a failure with no request |
| `Test_AnimSave_StreamResidencyRestored` | the stream cache after restore holds the saved RESIDENT chunks with their last-use ticks, so the first restored evaluation stalls exactly where the uninterrupted run did (nowhere) |
| `Test_AnimSave_RestoreBindFailureCounted` | a save whose recorded clip has become corrupt restores the instance `BINDING_FAILED`, logs once, counts `anim.save.restore_bind_failed_delta` and completes the restore; a saved RESIDENT chunk whose restore-time read fails becomes `FAILED`, counted the same way; a parked binding success whose asset fails at restore is dropped by the handler at t + 1 |
| `Test_AnimSave_RestoreAfterGraphEdit` | a record restored onto an edited graph keeps matching nodes by stable ID |
| `Test_AnimCounters_Published` | every counter of "Counters" is emitted with its kind and `_delta` naming; counters and `GetStats` are identical with 1 and 8 workers (leaf records merged in leaf order) |
| `Test_AnimLog_ReportedKeysBounded` | the first 4,096 distinct report keys log once each; the 4,097th and later count without logging |
| `Test_AnimDump_ContainsState` | `anim.dump` writes the machine, state, transitions, parameters and rate of an entity; `GetDebugState` fills every member of `Zenith_AnimDebugState` for a two-layer graph in transition, sets `m_bTruncated` for a graph of 300 nodes, and returns `ANIMATION_ERROR_NOT_AVAILABLE` in Shipping |
| `Test_AnimationMemory_ReportsResidentBytes` | the consumer's bytes equal the sum of the documented parts |
| `Test_AnimationMemory_ForcedCriticalShedsNothing` | 600 tick hashes are identical with and without a forced CRITICAL; ANIM's row in `Zenith_MemorySheddingCases.h` (`MEMORY_SUBSYSTEM_SIMULATION`, consumer `anim`, nothing shed) is the memory suite's case for the module |
| `Test_AnimFailure_MissingClipRestPose` | a graph whose clip failed to load (NULL_OBJECT) evaluates that node as the rest pose and counts it |
| `Test_AnimFailure_CorruptAssetsFailLoad` | corrupt skeleton, clip and graph files fail their loads with their codes and leave animators in `BINDING_FAILED` |
| `Test_AnimationBindings_GetMarkerTime` | `Animation.GetMarkerTime(clip, "char.lipsync.fr")` returns the marker's time for a bound clip, nil for an absent marker name, and raises ZS2013 for an unbound clip and for an empty name, with the code and text of "Misuse codes" of Runtime.md |
| `Test_AnimationBindings_PlaySlotTagOption` | `Animation.PlaySlot(entity, slot, clip, { tag = 16384 })` records a `Zenith_AnimSlotPlayDesc` whose `m_uTag` is 16384 (`ANIM_SLOT_TAG_FULL_RATE`); an omitted tag records 0; `tag = 32768` and `tag = -1` raise ZS2009 and record nothing |

## Motion matching tests

File `Tests/Zenith_AnimMotionMatch_Tests.cpp`. The first three land with the harness ticket (`Z2-R-ANIM-178`, Stage 3), the fourth with node kind 15 (`Z2-R-ANIM-179`, Stage 4), whose cook-side database test `Test_MotionMatch_DatabaseCookInputs` is in "Import and cook tests":

| Test | Asserts |
|---|---|
| `Test_MotionMatch_PrunedSearchEqualsBruteForce` | over a 5,000-frame database built from generated clips and 10,000 random normalised queries, `Search` and `SearchBruteForce` return the same frame and the bit-identical cost, with and without a current frame, and `Search` tests fewer frames than F on average |
| `Test_MotionMatch_ContinuationWinsTies` | in a database holding two identical frames, the lower index wins a tie; when the current frame's continuation ties the best, the continuation is returned; a continuation that crosses into another clip is not a continuation |
| `Test_MotionMatch_DatabaseDeterministic` | two builds from the same skeleton, clips and desc are bit-identical; a constant feature dimension stores a deviation of 1; a non-loop clip contributes no frame from its last second |
| `Test_MotionMatch_SwitchIsInertial` | a graph with node kind 15 switching to a frame in another clip requests an instance take of the node's blend seconds (the jump of every joint at the switch is at most the quintic's first step plus 1e-5 m), and a non-finite desired-velocity parameter keeps the current frame and is counted |

## Determinism tests

`Zenith2/Tests/Animation/Zenith_AnimDeterminism_Tests.cpp` (integration suite, every ECS backend):

| Test | Asserts |
|---|---|
| `Test_AnimDeterminism_WorkerAndLayoutIndependent` | a 1,200-tick replay of 100 animated characters (blend spaces, state machines, layers, slots, events, root motion, IK and reduced rates from Stage 4) gives bit-identical tick hashes, pose bytes and event sequences with 1, 2, 8 and all workers, with forced chunk compaction and two chunk capacities, per backend ([Z2-N-CONTRACT-356]) |
| `Test_AnimDeterminism_ReplayBitIdentical` | recording and replaying the run reproduces every tick hash, including binding and streaming publications with I/O delayed differently in the replay |
| `Test_AnimStreaming_StallsAreReplayed` | as "Clip streaming" states |

## Integration scenarios

`Zenith2/Tests/Animation/Zenith_AnimIntegration_Tests.cpp`:

| Test | Asserts |
|---|---|
| `Test_AnimIntegration_RagdollRoundTrip` | a character animates, enters ragdoll (Jolt) mid-locomotion, settles, recovers into a get-up and resumes locomotion with no joint jump above 1e-3 m between ticks |
| `Test_AnimIntegration_RootMotionThroughMovement` | root-motion deltas published in phase 3 move, in the same tick, an entity without a character controller through `Animation.ApplyRootMotion` and an entity with one through PHYS's `Physics.CharacterMove` against a Jolt floor (assumption pending PHYS, row AN6); after 120 ticks of a straight walk both have travelled the clip's root motion within 1 mm |
| `Test_AnimationModule_ScheduleCompiles` | a world with the Physics and Animation modules freezes and compiles its schedule with no diagnostic; each ANIM world resource has exactly one writer node per stage in the compiled graph (the merge node of `Animation.Significance` for `Zenith.Animation.Residency`, the merge node of `Animation.Evaluate` for `Zenith.Animation.World`, `Animation.PostPhysicsBlend` for `Zenith.Animation.FinalPoses`); the compiled phase-3 order is `Animation.Significance` → its merge node → `Animation.Evaluate`; no ANIM system names an `Arbitrate.*` node; `Animation.PostPhysicsBlend` runs after `Arbitrate.Zenith.Animation.Pose.PhysicsWeight` and after every `Arbitrate.Zenith.LocalTransform.*` node present. A second case, added by the Stage 4 ticket that adds `ZenithCinematics` to this suite, attaches Cinematics as well (so `Arbitrate.Zenith.LocalTransform.Scale` exists) and asserts the same |
| `Test_AnimBinding_AbandonedRequestCompletesItsID` | an animator destroyed while its graph load is in flight, one rebound to another graph while its first load is in flight, and a world detached with one binding load and one chunk read in flight: each abandoned completion ID is completed exactly once with the zero-length abandoned payload in the `Animation.Significance` run that abandons it (or by `DetachWorld` before it destroys any resource), its publication is dropped, ASSET's subsequent `CANCELLED` record is ignored, and the world's asynchronous table (through the runtime's test access, `GetInFlightCount()`) holds no ANIM ID in flight at the next tick boundary or before the detach destroys the resources; 2,000 create and destroy cycles of an animator whose load never completes before its destruction never reach `ASYNC_TABLE_ERROR_FULL` ([Z2-N-SCRIPT-931], core-doc item X255, and GAME's row GA19) |
| `Test_AnimIntegration_SkinnedPoseInRenderWorld` | the sealed render world holds one skinned pose per animated mesh with the instance's key (headless, with the fixture resolver) |
| `Test_AnimIntegration_SaveDuringStreamingCoherent` | ANIM's part of the SAVE scenario: a save at the snapshot point while clips stream and characters ragdoll restores animator state coherent with physics (same tick), and the replay from the save matches (row AN8) |

ANIM also contributes its assertions to Architecture's scenarios `Test_Scenario_TeleportDuringRagdoll` and `Test_Scenario_SaveAtSnapshotDuringStreaming` (`Zenith2/Docs/Architecture/LifecycleCommands.md → "Scenario tests"`), whose suite is GAME's.

## Fuzz targets

| Target | Input | Asserts |
|---|---|---|
| `Zenith_Fuzz_SkeletonFile` | arbitrary bytes as a `.zskel` after a valid ASSET header | the loader fails or succeeds, never crashes, leaks or asserts |
| `Zenith_Fuzz_ClipFile` | arbitrary sections of a `.zanim` | as above, and sampling a loaded clip at random times stays finite |
| `Zenith_Fuzz_GraphFile` | arbitrary sections of a `.zagraph` | as above, and 100 evaluations of a loaded graph stay finite |
| `Zenith_Fuzz_AnimIntermediates` | arbitrary `ZISK` and `ZICL` blobs | the cook steps fail with a cook error, never crash |
| `Zenith_Fuzz_AnimDocuments` | arbitrary JSON under the five schema tags | the readers and the graph compiler report diagnostics, never crash |

Each runs 60 s in the change-triggered `win-asan` line and 1 hour in the Stage 6 acceptance.

## Benchmarks

| Benchmark | Location | Measures |
|---|---|---|
| `Zenith_Bench_AnimEvaluation` | `Zenith2/Benchmarks/AnimEvaluation/` | phase-3 and phase-6 cost of 100 characters at 60 joints (the W-Characters content scale) with the representative graph, unreduced and reduced; reports MP-CPUZONE-shaped spans |
| `Zenith_Bench_ClipSampling` | `Zenith2/Benchmarks/ClipSampling/` | sampling cost per joint, raw and quantised |
| `Zenith_Bench_ClipCompression` | `Zenith2/Benchmarks/ClipCompression/` (`DEPENDS ZenithCook_Objects`, `ZENITH_TOOLS_HOST`) | compression time and ratio over the generated corpus |
| the Stage 1 per-system budget harness for ANIM | `Zenith2/Benchmarks/Budgets/ANIM/` | the reference engine's `Flux/MeshAnimation` skeletal update at the W-Characters scale on the reference desktop and the S24, the basis of the ruling's allocation (`Zenith2/Docs/Budgets.md → "Per-system producer shape"`) |
| the motion-matching harness | `Zenith2/Benchmarks/MotionMatching/` | D-SEL-MOTIONMATCH (open, Stage 3): search cost and memory for a test database at the W-Characters scale |

## Fixtures and generators

No ANIM test reads a committed binary file. `Tests/Support/Zenith_AnimTestFixtures.h` is header-only (`inline` generator functions, no `.cpp`; [Z2-N-ARCH-255]) and is included by `ZenithAnimation_Tests`, ANIM's ZenithCook and ZenithAuthoring tests and the integration suite; it generates, from seeds printed by each test:

| Generator | Produces |
|---|---|
| `Zenith_MakeTestSkeleton(J, seed)` | a random hierarchy with bone lengths in [0.05, 0.5] m and rest rotations, cooked into `.zskel` bytes in memory |
| `Zenith_MakeHumanoidSkeleton(eRestPose)` | a 60-joint humanoid with a complete role profile, in T-pose or A-pose |
| `Zenith_MakeTestClip(skeleton, duration, rate, seed, flags)` | sinusoidal tracks per channel, optional root motion, events and markers, cooked raw or quantised in memory |
| `Zenith_MakeTestGraph(document JSON)` | a graph compiled in memory from a JSON text in the test source |
| `Zenith_MakeTestGltf(…)` | glTF JSON text with base64 data URIs for skins and animations, for the import tests |

Cook-level tests that need cooked files on disk get them from one fixture set registered by the test registry as [Z2-N-ARCH-257] states: the setup entry `ANIM_CookedFixtures` (the doctest case `Test_AnimFixtures_CookSet` of `ZenithCook_Tests`, suite `z2-fixture-setup`, which empties `${ZENITH_OUT}/test-fixtures/ANIM/` and cooks the generated skeletons, clips (raw, quantised, streamed, additive, retargeted) and graphs into it), fixture name `AnimCookedFixtures`, environment variable `ZENITH2_ANIM_COOKED_FIXTURES`, and one consuming suite `z2-anim-cooked` per consuming target (`ZenithAnimation_Tests[z2-anim-cooked]`, `Zenith_Integration_Animation[z2-anim-cooked]`) with `FIXTURES_REQUIRED AnimCookedFixtures`; the entries exist only where `ZenithCook_Tests` is built (`win`). Every other consumer, the fuzz smokes and the `win-asan` and Android runs among them, builds its inputs in memory from the header-only generators above and requires no fixture. Nothing is committed; migration fixtures are JSON text under `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/Animation/<Type>/`.

<!-- elaboration -->
The set always holds four products other areas' suites name by persistent ID (A5 cross-area row GmB30): the humanoid skeleton `Zenith_MakeHumanoidSkeleton(ANIM_TEST_REST_POSE_T)` and three raw clips over it, cooked from `Zenith_MakeTestClip` with fixed seeds. Their catalogue entry names and GUIDs are fixed:

| Catalogue entry | GUID | Product |
|---|---|---|
| `Fixtures/Humanoid60.skel` | `a2f30000-0000-4000-8000-000000000001` | the 60-joint humanoid skeleton, T-pose |
| `Fixtures/Humanoid60_Idle.clip` | `a2f30000-0000-4000-8000-000000000002` | a 2 s looping clip at 30 Hz, seed 1, no root motion |
| `Fixtures/Humanoid60_Walk.clip` | `a2f30000-0000-4000-8000-000000000003` | a 1 s looping clip at 30 Hz, seed 2, root motion mode `xzYaw` advancing 1.4 m |
| `Fixtures/Humanoid60_GetUp.clip` | `a2f30000-0000-4000-8000-000000000004` | a 1.5 s non-looping clip at 30 Hz, seed 3, no root motion |

A change to any row is a change of every suite that names it, made in the same diff.
