# ANIM requirement catalogue: Stages 4 to 6

## Purpose

The second file of the ANIM catalogue: the requirements whose `Stage` is S4 or S6, in the same format and the same `Z2-R-ANIM-NNN` numbering as `Zenith2/Docs/Requirements/ANIM.md`, which holds the purpose of the whole catalogue, the Stage 1 and Stage 3 requirements, the coverage of inventory items (over both files) and the exclusions.

## Requirements

### Z2-R-ANIM-009 Stage 4 components

- **Statement:** The components `Zenith.Animation.IKGoals` (eight goals), `Zenith.Animation.MotionWarp` (four targets) and `Zenith.Animation.PoseFollower` (one leader) are registered with the property IDs, authority and ranges of "Zenith_AnimationComponents header".
- **Covers:** Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationModule_AuthorityTableRegistered` in the Stage 4 configuration; `Test_AnimationBindings_SettersBufferCommands` for the IK and warp setters (`Test` step). The registering ticket adds the types' rows to `Zenith2/Docs/Research/Phases/AuthorityCensus.md` in the same diff ("Component registration"), checked by GAME's `Test_AuthorityCensus_CharacterModules` (the census test of the suite that links ZenithAnimation; `Test_AuthorityCensus_OpenWorldModules` at Stage 4; row GA20) (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Zenith_AnimationComponents header"; Zenith2/Docs/Design/ANIM.md → "Component registration"

### Z2-R-ANIM-024 Rig chains IK definitions and mirroring

- **Statement:** The rig sections carry chains with chain roles, IK definitions (two-bone, chain, look-at, foot), sockets and a symmetric mirror table with its mirror axis, which retargeting, IK and the mirror node read and the cook validates.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-11, Z2-I-ANIM-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_SkeletonAsset_RejectsAsymmetricMirror`, `Test_SkeletonAsset_RejectsInvalidRig` and `Test_MirrorPose_Involution` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Rig sections"; Zenith2/Docs/Design/ANIM/Formats.md → "Rig document schema"

### Z2-R-ANIM-036 Track classification

- **Statement:** The compressor classifies each joint channel as `DEFAULT` (the skeleton's rest value; the identity for an additive clip), `CONSTANT` or `ANIMATED` over the clip, cumulatively in (joint, rotation, translation, scale) order with every earlier classification applied, so the combined error of all classified channels stays within ε / 2 at every joint and frame, and stores `DEFAULT` and `CONSTANT` channels without per-frame data.
- **Covers:** Z2-I-ANIM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_ClipCompression_ClassifiesTracks` and `Test_ClipCompression_ClassificationErrorAccumulates` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Track classification"

### Z2-R-ANIM-037 Range reduction

- **Statement:** Animated channels are range-reduced over the clip and again per 16-frame segment, in double precision, with a u8 segment minimum (clamped to [0, 254]) and extent (at least 1), so the segment range is never empty and always contains every sample of its segment, including a segment at the clip maximum; a segment-constant component (bit-rate index 0) stores its rounded minimum and an extent of 0; a clip-constant component (clip extent 0, or below 1e-30 and stored as 0) stores both bytes 0 and bit-stream values 0 at every index 0 … 17, is never divided by, decodes to the clip minimum exactly, and passes CV3, which refuses any other bytes for it.
- **Covers:** Z2-I-ANIM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_ClipCompression_RangeReductionContainsSegment`, `Test_ClipCompression_SegmentAtClipMaximum` and `Test_ClipCompression_ClipConstantComponent` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Range reduction"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip validation"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip range section"

### Z2-R-ANIM-038 Bit-rate table and shared dequantisation

- **Statement:** Each animated channel of a segment uses one of the 19 bit rates of "Bit-rate table" (0, 3 to 16, 18, 20, 23, 32 bits per component), its segment-range bytes are present exactly for bit-rate indices 0 to 17 (CV3 checks the part's size), index 0 is chosen only when its decoded constant meets the error target, and the cook's verification and the runtime sampler share one dequantisation function.
- **Covers:** Z2-I-ANIM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_ClipCompression_DequantiseSharedWithRuntime`, `Test_ClipCompression_SegmentAtClipMaximum` and `Test_AnimationClip_RejectsBadBitRate` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Bit-rate table"; Zenith2/Docs/Design/ANIM/Formats.md → "Segment section"

### Z2-R-ANIM-039 Hierarchical error metric

- **Statement:** Compression error is measured hierarchically as the model-space displacement of virtual vertices at each joint's shell distance `d_j = max(r_j, g_j)`, where `r_j` is the rig document's distance (else its default, else the longest child bone, at least 0.03 m) and `g_j` the largest `skinShellDistance` of the meshes the rig lists in `shellMeshes`, read by the `anim.clip` step alone as cook-input edges (no cook cycle with GEOM); a parent's error counts at every descendant.
- **Covers:** Z2-I-ANIM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_ClipCompression_ShellDistanceFromMesh` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Error metric"

### Z2-R-ANIM-040 Error bound held every frame

- **Statement:** A compressed clip's decoded pose differs from the double-precision resampled pose by at most its target `max(ε, E_raw)` (ε default 0.0002 m, the cinematic preset tighter; `E_raw` the raw encoding's own f32 floor) at every virtual vertex of every frame, an additive clip measured as its delta applied to its reference pose; the step verifies every frame after optimisation (the bound is claimed at frames only) and fails with the clip's cook detail when a frame violates it.
- **Covers:** Z2-I-ANIM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_ClipCompression_ErrorBoundHeldEveryFrame`, `Test_ClipCompression_AdditiveBoundOnReference` and `Test_ClipCompression_ViolationFailsStep` (`ZenithCook_Tests`, `Test` step); the pending tolerance row "Clip compression error" records the measured maximum.
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Error bound verification"; Zenith2/Docs/Design/ANIM/Formats.md → "Bit-rate optimisation"; Zenith2/Docs/Design/ANIM/Formats.md → "Error metric"

### Z2-R-ANIM-041 Deterministic compression

- **Statement:** The greedy bit-rate optimiser visits channels and rates in a fixed order with fixed tie-breaking, so the same inputs give byte-identical cooked clips on every machine and worker count.
- **Covers:** Z2-I-ANIM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_ClipCompression_Deterministic` (`ZenithCook_Tests`, `Test` step); ASSET's cook equivalence run over the cooked clips.
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Bit-rate optimisation"

### Z2-R-ANIM-042 Raw fallback

- **Statement:** The repair loop raises a chain track's rate, else demotes a classified channel of the chain to raw (pinned at index 18 in every segment, after which the optimisation restarts from segment 0), else accepts the joint at the raw encoding's own error, so it ends within `(1 + 3 × J) × S × (18 × A + J + 1)` iterations per clip (A ≤ 3 J the final animated-track count, S the segment count) and compression never fails a clip that the raw encoding represents.
- **Covers:** Z2-I-ANIM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_ClipCompression_RawFallbackWhenUnattainable`, `Test_ClipCompression_RepairTerminatesAtRawFloor` and `Test_ClipCompression_DemotionRestartsFromSegmentZero` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Bit-rate optimisation"; Zenith2/Docs/Design/ANIM/Formats.md → "Bit-rate table"

### Z2-R-ANIM-043 Curve compression

- **Statement:** Curve tracks compress with the same segment scheme against their own bound ε_c (default 0.001 in the curve's unit), verified per frame.
- **Covers:** Z2-I-ANIM-02, Z2-I-ANIM-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-004
- **Budgets:** none
- **Acceptance evidence:** `Test_ClipCompression_CurveErrorBound` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Curve compression"

### Z2-R-ANIM-044 Root motion stays raw

- **Statement:** The root-motion track is never quantised, so the deltas root motion publishes are exact samples of the extracted motion.
- **Covers:** Z2-I-ANIM-02, Z2-I-ANIM-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ClipCompression_RootMotionRaw` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Root motion section"; Zenith2/Docs/Design/ANIM/Formats.md → "Track classification"

### Z2-R-ANIM-045 Compression reported and measured

- **Statement:** The cook report carries `animation.compression` per clip (raw bytes, compressed bytes, ratio, measured maximum error) and the compression benchmark measures cook time and ratio over the generated clip set.
- **Covers:** Z2-I-ANIM-02
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_ClipCompression_RatioReported` (`ZenithCook_Tests`); `Zenith_Bench_ClipCompression` with `--smoke` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/ANIM/Tests.md → "Benchmarks"

### Z2-R-ANIM-046 Stream table

- **Statement:** A clip above `anim.streaming.thresholdBytes` with streaming allowed is cooked with a stream table that groups its segments into chunks, each segment in exactly one chunk; its header set stays resident while the clip is active.
- **Covers:** Z2-I-ANIM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_AnimationClip_RejectsStreamTableGap` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Stream table section"; Zenith2/Docs/Design/ANIM/Runtime.md → "Residency model"

### Z2-R-ANIM-047 Per-world stream cache

- **Statement:** Each world holds one stream cache bounded by `animation.streaming.cacheBytes` (default 16 MiB, a member of `Z2-B-ANIM-001`); when full it evicts least recently used chunks in no prefetch window (ties by clip GUID, then chunk index) and defers, counting `anim.stream.deferred_delta`, when none can be evicted.
- **Covers:** Z2-I-ANIM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_AnimStreaming_EvictsOutsideWindowLRU` and `Test_AnimStreaming_CacheFullDefers` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Residency model"; Zenith2/Docs/Design/ANIM/Runtime.md → "Streaming state machine"

### Z2-R-ANIM-048 Prefetch window

- **Statement:** For each instance and each streamed clip it plays, the chunks covering `[t, t + animation.streaming.prefetchSeconds]` (default 2.0 s) along the play direction, wrapping for a loop, are requested from `Animation.Significance` in ascending simulation ID, with high priority for a chunk containing a current playback time.
- **Covers:** Z2-I-ANIM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimStreaming_WindowRequestsChunks` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Prefetch window"; Zenith2/Docs/Design/ANIM/Runtime.md → "Streaming state machine"

### Z2-R-ANIM-049 Streaming stall holds time

- **Statement:** When an evaluation needs a segment that is not resident, the clip node holds its previous time (no advance, no events, no root motion) and counts `anim.stream.stalls_delta`; it resumes on the tick after the chunk publishes.
- **Covers:** Z2-I-ANIM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimStreaming_StallHoldsTime` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Prefetch window"

### Z2-R-ANIM-050 Chunk arrival at tick boundaries

- **Statement:** A chunk becomes resident only when its publication is handled at a tick boundary in completion-ID order, through the same sink and source kind as bindings (`ASYNC_SOURCE_ANIMATION`, `Zenith2/Docs/Scripting.md → "Asynchronous source kind registry"`), so a run and its replay with differently delayed I/O stall identically.
- **Covers:** Z2-I-ANIM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimStreaming_StallsAreReplayed` in `Zenith_Integration_Animation` (`Test` step and stage acceptance).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Publication"; Zenith2/Docs/Design/ANIM/Runtime.md → "Asset bindings"

### Z2-R-ANIM-051 Chunk read failure

- **Statement:** A failed chunk read publishes failure: the chunk becomes `FAILED`, its block is freed, the clip's instances hold their times at the chunk boundary, the failure is logged once, and the chunk is requested again only after the clip asset reloads.
- **Covers:** Z2-I-ANIM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimStreaming_FailurePublishedHolds` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Streaming state machine"; Zenith2/Docs/Design/ANIM.md → "Failure table"

### Z2-R-ANIM-052 Cancellation on detach

- **Statement:** Detaching a world cancels its outstanding chunk reads, completes every completion ID ANIM allocated and has not completed with the abandoned payload before it destroys any resource (pending X255), and frees each read's block only after the read reports complete, so no read writes freed memory and no ANIM completion ID is left in flight.
- **Covers:** Z2-I-ANIM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimStreaming_CancelOnDetach` (`Test` step), run under `win-asan`; the detach case of `Test_AnimBinding_AbandonedRequestCompletesItsID` (integration suite).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Streaming state machine"; Zenith2/Docs/Design/ANIM.md → "Cancellation"

### Z2-R-ANIM-053 Cancel and reload stress

- **Statement:** One thousand cycles of world attach, streaming playback, clip reload and detach complete with no leak, no use after free, no stale publication applied to a new instance and no ANIM completion ID left uncompleted after any cycle.
- **Covers:** Z2-I-ANIM-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimStreaming_CancelReloadStress1000` under the `win-asan` sanitizer line.
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Streaming state machine"; Zenith2/Docs/Design/ANIM.md → "Cancellation"

### Z2-R-ANIM-054 Prefetch commands and clip readiness

- **Statement:** `Zenith_RecordPrefetchClip` (and `Animation.PrefetchClip`) adds a clip and start time to an instance's prefetch windows, and the `CLIP_READY` condition and `Animation.IsClipReady` report true only when the clip is bound and its next chunk resident.
- **Covers:** Z2-I-ANIM-03, Z2-I-ANIM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimStreaming_WindowRequestsChunks` and `Test_AnimationBindings_SettersBufferCommands` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationCommands header"; Zenith2/Docs/Design/ANIM/Runtime.md → "Prefetch window"; Zenith2/Docs/Design/ANIM/Graph.md → "Transition evaluation"

### Z2-R-ANIM-120 Retarget map construction

- **Statement:** `Zenith_RetargetMap::Create` maps each target joint by override, then by joint role, then by normalised position along the matching chain role, leaving the rest unmapped, in O(J_s + J_t + chain lengths).
- **Covers:** Z2-I-ANIM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_Retarget_RoleAndChainMapping` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Retarget map construction"

### Z2-R-ANIM-121 Rest-relative retargeting

- **Statement:** A mapped target joint takes the model rotation `q_s · S_s⁻¹ · T_j`, carrying the source's rotation away from its rest onto the target's rest, so a rest-to-rest retarget is the identity and a T-pose clip plays correctly on an A-pose skeleton.
- **Covers:** Z2-I-ANIM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Retarget_RestToRestIsIdentity` and `Test_Retarget_TPoseToAPose` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Retarget map construction"; Zenith2/Docs/Design/ANIM/Runtime.md → "Runtime retarget application"

### Z2-R-ANIM-122 Root scale by hips height

- **Statement:** Root translation is scaled by `hipsHeight(target) / hipsHeight(source)` in `RETARGET_ROOT_SCALE_BY_HIPS_HEIGHT` mode (1 when either is 0), copied in `COPY` mode and dropped in `NONE` mode.
- **Covers:** Z2-I-ANIM-11, Z2-I-ANIM-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_Retarget_RootScaleByHipsHeight` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Retarget map construction"

### Z2-R-ANIM-123 Retargeting never edits a mesh

- **Statement:** Retargeting changes rotations and root translation scale only; it never edits a mesh, re-poses a rest pose or rebinds a skin, and a clip of another skeleton without a retarget profile is refused.
- **Covers:** Z2-I-ANIM-11
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_Retarget_NoMeshOrRestPoseChange` and `Test_Retarget_UnsupportedWithoutProfile` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Retargeting"; Zenith2/Docs/Design/ANIM.md → "Design rulings owned by ANIM"

### Z2-R-ANIM-124 Retarget documents and cooked products

- **Statement:** A retarget document (`.animretarget.json`, schema version 1) names source and target skeletons, settings and clips; the `anim.retarget` importer declares one retargeted clip product per listed clip, cooked from the source's raw clip.
- **Covers:** Z2-I-ANIM-11, Z2-I-ANIM-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-038, D-SEL-JSON (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** `Test_CookClip_RetargetedProduct` (`ZenithCook_Tests`) and `Test_AnimDocuments_RegisteredToolSide` (`ZenithAuthoring_Tests`) in the Stage 4 configuration (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Retarget document schema"; Zenith2/Docs/Design/ANIM/Formats.md → "Retargeted clip cooking"

### Z2-R-ANIM-125 Two-bone IK

- **Statement:** `Zenith_SolveTwoBoneIK` reaches targets inside reach within 1e-4 m (reporting reached within 1e-3 m), clamps out-of-reach targets to `a + b − 1e-4`, bends in the pole plane, stays finite for degenerate inputs, and limits the straightening snap with soft IK; O(1).
- **Covers:** Z2-I-ANIM-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_TwoBoneIK_ReachesInsideRange`, `Test_TwoBoneIK_ClampsOutOfReach`, `Test_TwoBoneIK_PoleSelectsBendPlane`, `Test_TwoBoneIK_DegenerateInputsStable` and `Test_TwoBoneIK_SoftnessLimitsSnap` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Two-bone solver"

### Z2-R-ANIM-126 Chain IK

- **Statement:** `Zenith_SolveChainIK` (FABRIK) converges within its tolerance in at most its iteration count (default 10), stretches straight toward an unreachable target, and preserves each joint's twist when converting positions to rotations.
- **Covers:** Z2-I-ANIM-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_ChainIK_ConvergesWithinTolerance`, `Test_ChainIK_StretchesWhenUnreachable` and `Test_ChainIK_PreservesTwist` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "FABRIK solver"

### Z2-R-ANIM-127 Look-at IK

- **Statement:** `Zenith_SolveLookAt` distributes an aim over up to four joints by their shares (summing to 1) with per-joint angle limits.
- **Covers:** Z2-I-ANIM-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_LookAt_SharesAndLimits` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Look-at solver"

### Z2-R-ANIM-128 Foot placement

- **Statement:** Foot placement probes the ground with a physics ray per foot, lowers the pelvis by the most negative foot offset (clamped to [−0.5, +0.2] m, smoothed by a critically damped spring of half-life 0.05 s on the fixed tick), solves each leg to its ankle target and tilts the ankle toward the ground normal by at most 30°; a miss leaves the foot unchanged.
- **Covers:** Z2-I-ANIM-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-003
- **Budgets:** Z2-B-ANIM-002, Z2-B-PHYS-002
- **Acceptance evidence:** `Test_FootPlacement_PelvisAndAnkles` and `Test_FootPlacement_MissLeavesFoot` with a real Jolt world (`Test` step), `blocks`-linked from PHYS's ray-cast ticket (row AN6).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Foot placement"

### Z2-R-ANIM-129 IK stage order

- **Statement:** IK steps run after layers and motion warping in record order with foot placement first, then two-bone and chain goals, then look-at, each reading the previous step's output.
- **Covers:** Z2-I-ANIM-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_IKStage_OrderFootFirst` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "IK stage order"

### Z2-R-ANIM-130 Motion warping

- **Statement:** Inside an active warp window the clip's root-motion delta is corrected every evaluation toward the target so the entity reaches a static target within 1 cm at the window's end, tracks a moving target, and is untouched outside the window.
- **Covers:** Z2-I-ANIM-13, Z2-I-ANIM-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_MotionWarp_ReachesStaticTarget`, `Test_MotionWarp_TracksMovingTarget` and `Test_MotionWarp_OutsideWindowUntouched` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Motion warping"

### Z2-R-ANIM-131 Curves blend with the pose

- **Statement:** Curves blend with the same weights as joints in every node, and up to eight curve overrides per instance replace or blend a curve's value by their weight.
- **Covers:** Z2-I-ANIM-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_FacialCurves_BlendLikeJoints` and `Test_CurveOverride_EightAndReplace` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Curves and morph targets"

### Z2-R-ANIM-132 Morph remap

- **Statement:** The extractor maps skeleton curves to a mesh's morph targets by name hash through a per (mesh, skeleton) remap table, a target with no curve taking zero, cached up to `animation.extract.maxRemapTables` (row AN4).
- **Covers:** Z2-I-ANIM-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-ANIM-001
- **Acceptance evidence:** `Test_MorphRemap_ByNameHash` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Morph remap"; Zenith2/Docs/Design/ANIM/Runtime.md → "Curves and morph targets"

### Z2-R-ANIM-133 Pose libraries

- **Statement:** A pose library clip applies named facial poses as additives from its neutral frame, weighted by curves.
- **Covers:** Z2-I-ANIM-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_PoseLibrary_AdditiveFromNeutral` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Pose libraries"

### Z2-R-ANIM-134 Pose followers

- **Statement:** An entity with `Zenith.Animation.PoseFollower` renders its leader's final pose without an instance of its own, and a follower whose mesh binds another skeleton is refused and counted.
- **Covers:** Z2-I-ANIM-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_PoseFollower_UsesLeaderPose` and `Test_PoseFollower_RefusesOtherSkeleton` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Pose followers"

### Z2-R-ANIM-135 Mirroring

- **Statement:** The mirror node mirrors its child's pose through the skeleton's mirror table and axis and mirrors its root motion, and mirroring twice is the identity.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_MirrorNode_MirrorsPoseAndRootMotion` and `Test_MirrorPose_Involution` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Node catalogue"; Zenith2/Docs/Design/ANIM/Graph.md → "Root motion sampling"

### Z2-R-ANIM-140 Significance from simulation state

- **Statement:** `Animation.Significance` computes each instance's significance from the previous tick's distance to the main camera that RPROD's `Flux_SelectMainCamera` selects and the animator's bias, and its rate tier k ∈ {1, 2, 4, 8} from the distance keys (15, 40, 80 m), reading no render state, quality tier or thermal state.
- **Covers:** Z2-I-CHAR-04 (CHAR)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-012
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_AnimSignificance_TiersFromDistance`, `Test_AnimSignificance_MainCameraByPriority` and `Test_AnimSignificance_IgnoresQualityTier` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Significance"; Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets"

### Z2-R-ANIM-141 Phase offsets spread evaluations

- **Statement:** Reduced-rate instances evaluate on the ticks where `(tick + offset) mod k = 0`, with the offset hashed from the simulation ID, so the evaluations of a tier spread evenly across its k ticks.
- **Covers:** Z2-I-CHAR-04 (CHAR)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_AnimSignificance_PhaseOffsetsSpread` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets"

### Z2-R-ANIM-142 Forced full rate

- **Statement:** `ANIMATOR_FLAG_FORCE_FULL_RATE`, an active ragdoll, an active cinematic slot, an active warp window and an instance's first evaluation force k = 1.
- **Covers:** Z2-I-CHAR-04 (CHAR)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimSignificance_ForcedFullRateCases` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets"

### Z2-R-ANIM-143 Full-rate evaluation cap

- **Statement:** When more instances are due at full rate than `animation.budget.maxFullRateEvaluationsPerTick`, the lowest-significance ones (ties by higher simulation ID) are demoted to k = 2 for that tick.
- **Covers:** Z2-I-CHAR-04 (CHAR)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-012
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_AnimSignificance_CapDemotesLowest` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Evaluation cap"

### Z2-R-ANIM-144 Reduced-rate root motion

- **Statement:** An instance extracting root motion at reduced rate publishes its span delta D over k ticks as `Pow(D, 1/k)` k − 1 times and the exact remainder, so the steps compose to D exactly.
- **Covers:** Z2-I-CHAR-04 (CHAR), Z2-I-ANIM-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimSignificance_ReducedRootMotionComposesExactly` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Root motion under reduced rates"

### Z2-R-ANIM-145 Triggers survive skipped ticks

- **Statement:** A trigger set for a reduced-rate instance survives the ticks it skips and is seen by its next evaluation.
- **Covers:** Z2-I-CHAR-04 (CHAR), Z2-I-ANIM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimSignificance_TriggersSurviveSkippedTicks` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Triggers"; Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets"

### Z2-R-ANIM-146 Reduced-rate measurement

- **Statement:** The ANIM Stage 4 budget ruling ticket measures the animation phase CPU with significance reduction enabled in W-Characters and W-OpenWorld (reported beside `Z2-B-ANIM-002`, which is measured unreduced), the stream cache bytes and stalls, and the compression tolerance, allocates the five Stage 4 pending rows, and sets `animation.budget.maxFullRateEvaluationsPerTick` and `animation.streaming.cacheBytes` per platform from them.
- **Covers:** Z2-I-CHAR-04 (CHAR)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** The `needs-human` ANIM Stage 4 budget ruling ticket's measured report under `Zenith2/Docs/Research/` and its row values (pending rows "Animation budgeting reduced-rate CPU", "Animation full-rate evaluations per tick cap", "Animation stream cache bytes", "Animation stream stalls per run", "Clip compression error"; row AN12, X212), in Stage 4 acceptance.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Pending row needs"; Zenith2/Docs/Design/ANIM/Runtime.md → "Evaluation cap"; Zenith2/Docs/Design/ANIM.md → "Configuration keys"

### Z2-R-ANIM-149 Save during streaming and ragdoll

- **Statement:** A save taken at the snapshot point while clips stream and characters ragdoll restores animator state coherent with physics at the same tick, and the replay from the save matches.
- **Covers:** Z2-I-ANIM-03, Z2-I-ANIM-15
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-048, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimIntegration_SaveDuringStreamingCoherent` and ANIM's assertions in `Test_Scenario_SaveAtSnapshotDuringStreaming` (`Zenith_Integration_Animation`, `Test` step and stage acceptance).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Save state"; Zenith2/Docs/Design/ANIM/Tests.md → "Integration scenarios"

### Z2-R-ANIM-150 Save record migration

- **Statement:** Every `Zenith_AnimatorSaveRecord` version increment ships one forward migration step with a fixture, and the shipped versions load under the compatibility policy.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** SAVE's migration suite over ANIM's save-record fixtures in Stage 6 acceptance.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Migration and re-cook"; Zenith2/Docs/Design/ANIM/Runtime.md → "Save state"

### Z2-R-ANIM-164 Fuzzing at full duration

- **Statement:** The five fuzz targets run at full duration with no finding before shipping.
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-02, Z2-I-ANIM-03, Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-06, Z2-I-ANIM-07, Z2-I-ANIM-08, Z2-I-ANIM-09, Z2-I-ANIM-10, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-ANIM-14, Z2-I-ANIM-15, Z2-I-ANIM-16
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** The full-duration fuzz run record in Stage 6 acceptance.
- **Design:** Zenith2/Docs/Design/ANIM/Tests.md → "Fuzz targets"; Zenith2/Docs/Design/ANIM.md → "Stage table"

### Z2-R-ANIM-167 Determinism of Stage 4 features

- **Statement:** The determinism run with IK, motion warping, retargeting, reduced rates and streaming enabled stays bit-identical across workers, layouts and replays, including streaming stalls.
- **Covers:** Z2-I-ANIM-03, Z2-I-ANIM-11, Z2-I-ANIM-12, Z2-I-ANIM-13, Z2-I-CHAR-04 (CHAR)
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimDeterminism_WorkerAndLayoutIndependent` in its Stage 4 configuration and `Test_AnimStreaming_StallsAreReplayed` (stage acceptance).
- **Design:** Zenith2/Docs/Design/ANIM/Tests.md → "Determinism tests"; Zenith2/Docs/Design/ANIM/Runtime.md → "Publication"

### Z2-R-ANIM-171 Preview matches the world

- **Statement:** `Zenith_AnimationPreview` evaluates a graph document compiled in memory with the runtime evaluator, so a preview with the same parameters produces the same pose as a play world.
- **Covers:** Z2-I-ANIM-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationPreview_MatchesWorldEvaluation` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationPreview header"; Zenith2/Docs/Design/ANIM.md → "Editor surfaces"

### Z2-R-ANIM-172 Graph hot reload

- **Statement:** Reloading a graph asset keeps each instance's parameters and the state of nodes matched by stable ID, resets unmatched nodes, and covers the change with an instance-level inertial take of 0.15 s measured at the reloaded graph's first evaluation.
- **Covers:** Z2-I-ANIM-16, Z2-I-ANIM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimGraph_HotReloadKeepsStateAndParameters` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Graph hot reload"; Zenith2/Docs/Design/ANIM/Graph.md → "Inertial blending"

### Z2-R-ANIM-173 Visual authoring workflow

- **Statement:** EDIT's animation panels (graph, blend space, mask, rig, clip timeline, retarget, preview, live debug) drive ANIM's documents and APIs end to end, so a character goes from import through rig, retarget and graph to in-game playback without hand-edited JSON (row AN11).
- **Covers:** Z2-I-ANIM-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-079
- **Budgets:** Z2-B-CHAR-001
- **Acceptance evidence:** CHAR's timed walkthrough measured against `Z2-B-CHAR-001` in W-Editor (Stage 4 acceptance), with the pending row "Animation graph preview latency" from EDIT's W-Editor tickets.
- **Design:** Zenith2/Docs/Design/ANIM.md → "What ANIM authors"; Zenith2/Docs/Design/ANIM.md → "Editor surfaces"

### Z2-R-ANIM-179 Motion-matching node

- **Statement:** Node kind 15 builds its database at cook with `Zenith_MotionMatchDatabase::BuildFromFrames` over each of its clips' `anim.rawClip` frames after root-motion extraction and loop closure (cook-input edges of the `anim.graph` key, so a clip edit re-cooks the database), the same core `Build` runs over runtime clips, searches it with `Zenith_MotionMatcher` every search interval for the best pose-and-trajectory match (the continuation winning ties), and switches to a frame in another clip or more than 0.2 s away under an instance-level inertial take.
- **Covers:** Z2-I-ANIM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** D-SEL-MOTIONMATCH (open, Stage 3), Z2-D-084
- **Budgets:** Z2-B-ANIM-001, Z2-B-ANIM-002
- **Acceptance evidence:** `Test_MotionMatch_SwitchIsInertial` and `Test_MotionMatch_DatabaseCookInputs` (`Test` step) and `Test_AnimDeterminism_WorkerAndLayoutIndependent` with a motion-matching graph; the tickets are `blocks`-linked from the D-SEL-MOTIONMATCH ruling ticket.
- **Design:** Zenith2/Docs/Design/ANIM/Graph.md → "Motion matching node"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationMotionMatch header"; Zenith2/Docs/Design/ANIM.md → "Classification decisions ANIM owns"; Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys"

### Z2-R-ANIM-180 W-Characters sustained on the S24

- **Statement:** W-Characters runs sustained on the S24 within ANIM's rows and W-Characters' frame-time rows.
- **Covers:** Z2-I-ANIM-04, Z2-I-ANIM-05, Z2-I-ANIM-15, Z2-I-CHAR-04 (CHAR)
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-021, D-SEL-PHYSX-ANDROID (open, Stage 1)
- **Budgets:** Z2-B-ANIM-001, Z2-B-ANIM-002, Z2-B-ANIM-003, Z2-B-SHIP-001, Z2-B-SHIP-002
- **Acceptance evidence:** The S24 sustained device run record in Stage 6 acceptance.
- **Design:** Zenith2/Docs/Design/ANIM.md → "Device and acceptance runs"; Zenith2/Docs/Design/ANIM.md → "Stage table"

### Z2-R-ANIM-181 Curve clip intermediate writer

- **Statement:** ZenithAuthoring's `Zenith_WriteCurveClipIntermediate(const Zenith_AnimCurveClipDesc&, Zenith_Array<uint8>&)` writes the `ZICL` clip intermediate of a curve-and-pose source with no glTF node tables (step and linear channels, flag bit 0 clear) in exactly the layout ZenithCook's reader reads, refusing non-increasing times, wrong value widths, non-finite values and counts past the intermediate limits with nothing written, so CHAR's lip-sync and pose-book importers produce clips through ANIM's own writer (row CH15).
- **Covers:** Z2-I-ANIM-14, Z2-I-ANIM-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimImport_CurveClipIntermediateMatchesReader` (`ZenithCook_Tests`) and `Test_AnimAuthoring_CurveClipRefusesBadInput` (`ZenithAuthoring_Tests`) in the `Test` step.
- **Design:** Zenith2/Docs/Design/ANIM.md → "ZenithAuthoring ANIM files"; Zenith2/Docs/Design/ANIM/Formats.md → "Clip intermediate"

### Z2-R-ANIM-182 Rate divisor function

- **Statement:** `Zenith_AnimComputeRateDivisor(fDistanceMetres, fBias, bForceFullRate, const Zenith_AnimBudgetDistances&)` in `Zenith_AnimationWorld.h` is the one function that turns a camera distance and a significance bias into the rate divisor 1, 2, 4 or 8 by the steps of "Rate tiers and phase offsets" (forcing, a non-finite distance to 1, the bias clamp, inclusive tier bounds, a non-increasing triple collapsing tiers), and `Animation.Significance` calls it for every instance, so a tool predicting a divisor (CHAR's budget panel, row CH15) uses the same code.
- **Covers:** Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** Z2-B-ANIM-002
- **Acceptance evidence:** `Test_AnimSignificance_RateDivisorMatchesTiers` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets"; Zenith2/Docs/Design/ANIM/Runtime.md → "Zenith_AnimationWorld header"

### Z2-R-ANIM-183 Content-rule measures

- **Statement:** ANIM's cook-step registration registers, through ASSET's `RegisterRule`, the content-rule measures of its kinds: Skeleton `bones` (the joint count) and `ikChains` (the IK definition count) and AnimationGraph `layers` (the layer count), each read from the info section of the file the step wrote, so the character-class rows of ContentSpecs limit them at cook and no other area reads ANIM's cooked bytes (row CH15).
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimContentRules_MeasuresRegistered` and `Test_AnimContentRules_BonesLimitRefusesCook` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM.md → "Cook steps and derived-data keys"

### Z2-R-ANIM-184 Marker lookup and the full-rate slot tag

- **Statement:** The Luau `Animation.GetMarkerTime(clip, name)` returns a bound clip's marker time in seconds, or nil when the clip has no marker of that name, pure on the binding cache; `Animation.PlaySlot`'s options take `tag` (0 … 32767); and an active slot whose tag has `ANIM_SLOT_TAG_FULL_RATE` (bit 14) forces rate divisor 1 as `ANIM_SLOT_TAG_CINEMATIC` (bit 15) does, so a lip-sync play is evaluated every tick within its window (row CH16).
- **Covers:** Z2-I-ANIM-14, Z2-I-CHAR-04
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimationBindings_GetMarkerTime`, `Test_AnimationBindings_PlaySlotTagOption` and `Test_AnimSignificance_FullRateSlotTagForcesRate1` (`Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Runtime.md → "Luau bindings"; Zenith2/Docs/Design/ANIM/Runtime.md → "Rate tiers and phase offsets"

### Z2-R-ANIM-185 Published clip markers

- **Statement:** The `anim.clip` step publishes `anim.clip.markers` (the clip's markers `{name hash, time}` and instant events `{name hash, time, payload}`, little-endian, ascending time, notify states excluded) through `Zenith_CookContext::PublishProperty`, so a dependent cook step (CINE's lip-sync window table, row CN27) reads them with `ReadDependencyOutput` as a keyed cook-input edge without parsing ANIM's cooked format.
- **Covers:** Z2-I-ANIM-09, Z2-I-ANIM-14
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_CookClip_PublishesMarkersProperty` (`ZenithCook_Tests`, `Test` step).
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Clip published properties"

### Z2-R-ANIM-186 Skeleton and clip product fingerprints

- **Statement:** ANIM's import helpers record a fingerprint for every `skel:` and `anim:` product (the first 8 bytes of SHA-256 over `ZFPS`, the joint count and the sorted joint names; over `ZFPC`, the channel count and the duration bits) and recognise a rename only when exactly one previous and one new key of a prefix carry equal fingerprints, otherwise orphaning the old key with a warning, so a re-import keeps a renamed skeleton's or clip's GUID and never rebinds an unrelated product (CONTENT row CT25 (c)).
- **Covers:** Z2-I-ANIM-01, Z2-I-ANIM-16
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_AnimImport_ProductFingerprints` and `Test_AnimImport_RenameRecognition` (`ZenithCook_Tests`, `Test` step). Pending ASSET (CONTENT row CT22): `SetProductFingerprint` and the fingerprint member of previous products.
- **Design:** Zenith2/Docs/Design/ANIM/Formats.md → "Product fingerprints and rename recognition"
