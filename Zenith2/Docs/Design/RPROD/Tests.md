# RPROD design: tests, parity suites and harnesses

This file is part of the RPROD design family; the entry document is `Zenith2/Docs/Design/RPROD.md`. It names every RPROD test with the case it asserts, the parity suites and their scenes, the synchronization-validation rule, the benchmarks and harnesses, and the device steps. Every GPU test runs on WARP and lavapipe with Vulkan synchronization validation and D3D12 GPU-based validation, and any unsuppressed message fails it (`Zenith2/Docs/Design/RHI.md → "The sync-validation-clean rule"`, [Z2-N-REND-382], [Z2-N-REND-395]).

## Scope of this file

The required test lists of the specification are `Zenith2/Docs/Rendering/RenderGraph.md → "Graph tests"`, "Graph errors", `Zenith2/Docs/Rendering/Culling.md → "Culling tests"`, `Zenith2/Docs/Rendering.md → "Render views"` (heading "Render view tests"), "Shading paths and capability matrix" (heading "Capability matrix tests"), "Validation roles", `Zenith2/Docs/Rendering/ShaderSystem.md` (headings "Shader system tests", "PSO counters and tests") and the lint fixtures of "GPU performance rules and lints". The graph back-end tests (`Test_RenderGraph_AsyncGapRule`, `Test_RenderGraph_CrossQueueTransfer`, `Test_RenderGraph_SplitBarrierNotAcrossBatches`, `Test_RenderGraph_ParallelRecordMatchesSerial`, `Test_PSO_CacheRejectsDriverMismatch`, `Test_PSO_CacheInterruptedWriteKeepsPrevious`) are RHI's (`Zenith2/Docs/Design/RHI/Tests.md → "Graph back-end tests"`).

## Test files

| Directory | Target | Gate step |
|---|---|---|
| `Zenith2/Engine/ZenithFlux/Tests/Graph/` | `ZenithFlux_Tests` | `Test` (win-debug, win-release), `Sanitizers -Preset win-asan` in stage acceptance |
| `Zenith2/Engine/ZenithFlux/Tests/Culling/` | `ZenithFlux_Tests` | `Test` |
| `Zenith2/Engine/ZenithFlux/Tests/Views/`, `Tests/Paths/`, `Tests/Quality/`, `Tests/Pacing/`, `Tests/PSO/`, `Tests/Renderer/` | `ZenithFlux_Tests` | `Test` |
| `Zenith2/Engine/ZenithFlux/Tests/Render/` (render tests with captured images) | `ZenithFlux_Tests` | `Test` |
| `Zenith2/Engine/ZenithFlux/Tests/Parity/` (`Scenes/`, `Support/Flux_Flip.*`, `Support/Flux_ParityRunner.*`) | `ZenithFlux_Tests`, labels `z2-parity-backend` and `z2-parity-path` | `BackendParity` (Stage 3), `PathParity` (Stage 5) |
| `Zenith2/Engine/ZenithFlux/Tests/Examples/` | `ZenithFlux_Tests` | `Test` (examples compiled as tests) |
| `Zenith2/Tools/ZenithShaderCompiler/Tests/` (`Fixtures/Lints/`, `Fixtures/Programs/`) | `ZenithShaderCompiler_Tests` | `Test` (win) and `ShaderChecks` |
| `Zenith2/Tools/ZenithCook/Tests/PSO/` | `ZenithCook_Tests` | `Test` (win) |
| `Zenith2/Engine/ZenithRuntime/Tests/EngineExtractors/` (the engine extractors' tests of `Zenith2/Docs/Design/RPROD/Extractors.md → "Extractor tests"`; the directory joins MOD's `ZenithRuntime_Tests` by reconciliation row RP26) | `ZenithRuntime_Tests` | `Test` (win-debug, win-release), `Sanitizers -Preset win-asan` in stage acceptance |
| `Zenith2/Benchmarks/Budgets/RPROD/`, `ShadingPaths/`, `TBDR/` | `Zenith_Bench_Budgets_RPROD`, `Zenith_Bench_ShadingPaths`, `Zenith_Bench_TBDR` | the CTest smoke run in `Test` |

<!-- assumption: pending MOD -->
The two parity labels use the CTest label mechanism JOBS also needs (row J12); the parity cases are excluded from the `Test` step and selected by the PROC-owned `BackendParity` and `PathParity` step scripts. Row RP10 records the label names for PROC.
<!-- /assumption -->

## Graph unit tests

| Test | Asserts |
|---|---|
| `Test_RenderGraph_OrderFromVersionsOnly` | two declaration orders of the same passes compile to the same dependency-respecting order and identical images ([Z2-N-REND-293]) |
| `Test_RenderGraph_CullNoConsumer`, `Test_RenderGraph_SideEffectNeverCulled`, `Test_RenderGraph_MarkOutputKeepsChain` | culling roots and the `noConsumer` reason ([Z2-N-REND-299]) |
| `Test_RenderGraph_EmptyRasterPassCulled`, `Test_RenderGraph_ClearFoldedIntoLoadOp`, `Test_RenderGraph_ClearKeptWhenNextUserIsCompute` | sub-steps 1a and 1e; the `noDraws` reason; the folded user's load op is `CLEAR` with the clear value ([Z2-N-REND-915]) |
| `Test_RenderGraph_MergeIdenticalAttachments` | two adjacent raster passes with identical attachments and `STORE`→`LOAD` become one scope with each pass's timestamps |
| `Test_RenderGraph_MergeBlockedByBarrier` | A writes a UAV counter B reads between two identical-attachment raster passes: no merge, reason `barrierBetween`, and the dump's `scopes[].blocked` names it ([Z2-N-REND-309]) |
| `Test_RenderGraph_MergeBlockedByLoadStore` | a `DONT_CARE` store blocks with `loadStoreIncompatible` |
| `Test_RenderGraph_MergeBlockedBySplitBegin` | a split begin half after A blocks the merge ("Split-barrier candidates") |
| `Test_RenderGraph_SplitBarrierPlaced` | a transition with 3 passes between producer and consumer on one queue is split: begin after the producer's scope, end before the consumer |
| `Test_RenderGraph_SubresourceMipChain` | a bloom-style chain gets one barrier per mip transition and none redundant, and compiles with no core first-write diagnostic ("First-access widening") |
| `Test_RenderGraph_FirstWriteAddsNoEdge` | see "Widened first write test" below |
| `Test_RenderGraph_IndependentFacesNoEdge` | six passes each first-writing one face of one transient cube, and a side-effect reader of the whole cube, compile through step 2 with no edge between any two face writers and one `RAW` edge from each writer to the reader; the core raises no first-write diagnostic ([Z2-N-ARCH-346]) |
| `Test_RenderGraph_TransientsDeclaredPerSubresource` | every transient is declared to the core with `GRAPH_RESOURCE_FLAG_REQUIRE_DEFINED_READS | GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE` and every access with its real mode over its real range: a mip-0 first write is `WRITE_DISCARD` over subresource 0 alone |
| `Test_RenderGraph_ReadRunLookAhead` | a write followed by a compute read and a pixel read gets one barrier whose destination covers both stages |
| `Test_RenderGraph_AliasPlacementFirstFit` | placement offsets of a fixed fixture set equal the pseudocode's hand-computed values ([Z2-N-REND-435]) |
| `Test_RenderGraph_AliasNeverAcrossUnorderedQueues` | two transients on different queues without a wait path never share memory ([Z2-N-REND-436]) |
| `Test_RenderGraph_AliasDiscardSourcesEarlierOccupants` | the first use's discard names every earlier overlapping occupant's last-use stages ([Z2-N-REND-438]) |
| `Test_RenderGraph_AliasToggleNextCompile` | toggling applies at the next compile and changes nothing but placement; both modes carry the same discards ([Z2-N-REND-439], [Z2-N-REND-440]) |
| `Test_RenderGraph_HeapShrinkAfter300Frames` | a group heap shrinks after 300 frames below half its size, and at once under pressure action 3 ([Z2-N-REND-437]) |
| `Test_RenderGraph_PoisonDetectsFalseFullCoverage` | a fixture shader under-covering a `[FullCoverage]` UAV produces NaN downstream under poisoning ([Z2-N-REND-442]) |
| `Test_RenderGraph_BarrierVerifierCatchesMissingBarrier` | a corrupted barrier list fails the verifier ([Z2-N-REND-397]) |
| `Test_RenderGraph_TimestampsPerPass`, `Test_RenderGraph_FrameDumpSchema` | two queries per live pass (merged scopes too); every front-end dump key present for every pass, resource, scope and batch, culled passes with their reason ([Z2-N-REND-311], [Z2-N-REND-314]) |
| `Test_RenderGraph_SettingsReadOncePerCompile` | a setting changed during setup applies at the next frame only ([Z2-N-REND-290]) |
| `Test_RenderGraph_CompileReportsEveryError` | a fixture with three independent errors returns count 3 and all three diagnostics ([Z2-N-REND-291]) |
| `Test_RenderGraph_TrivialCaptureRequired` | a compile-fail fixture with a non-trivially-destructible capture does not compile ([Z2-N-REND-288]) |
| `Test_RenderGraph_CacheRetiresUnused` | a physical resource unused for 8 frames retires through the retirement queue ([Z2-N-REND-287]) |
| `Test_RenderGraph_ImportFirstAccessFromRecordedState` | an import's first access transitions from the imported-state table's run ([Z2-N-REND-294]) |
| `Test_RenderGraph_UAVAfterUAVBarrier` | two consecutive UAV passes on one subresource are always separated ([Z2-N-REND-386]) |
| `Test_RenderGraph_DeterministicCompile` | 100 compiles of one setup give byte-identical compiled graphs |
| `Test_RenderGraph_Error_<Code>` for each of the 22 codes of `Flux_RGError` except `SWAPCHAIN_QUEUE` (RHI's) | each failing fixture raises exactly its code with the pass, resource, version and range where they apply ([Z2-N-REND-446]) |
| `Test_FormatFeatures_GraphRejectsUngrantedUse` | a storage write of a format without W fails with `FORMAT_FEATURE_MISSING` ([Z2-N-REND-042]) |
| `Test_RecordCheck_<Check>` for each of the four `Flux_RecordCheck` values and `Test_RecordCheck_DepthWritableUnused` | each fixture triggers exactly its check, the call is dropped and the frame sink fails ([Z2-N-REND-297], [Z2-N-REND-920]) |
| `Test_PassContext_ExposesNoBarrierAPI` | a compile-fail fixture calling a command-list or barrier function through a context does not compile ([Z2-N-REND-300]) |
| `Test_PassContext_FailedBindDropsDraws` | draws after a false `BindPipeline` record nothing ([Z2-N-REND-302]) |

### Widened first write test

`Test_RenderGraph_FirstWriteAddsNoEdge` (RenderGraphFrontEnd.md "First-access widening", "Consequence of the widening"; the heading keeps its name for its citations) compiles each fixture through step 6 on WARP and on lavapipe, with a device-backed `Flux_GraphCaches` (the deviceless cache of RenderGraphFrontEnd.md "Flux_GraphCaches header" serves steps 1 to 3 alone, and the cross-queue case needs step 6's final queues), and derives the reference edges from the setup tables alone:

| Element | Exact rule |
|---|---|
| node set N | the live passes after sub-steps 1a to 1e: passes culled by 1a (`noDraws`) and by the core (`noConsumer`) removed, each version a removed pass would have produced replaced by the version it read (the version-alias table), and each clear-only pass folded by 1e removed with its predecessors contracted onto its user, exactly as the compile does |
| ranges | a texture access covers the subresources `mip × layerCount + layer` of its range; a buffer access covers the granules `[floor(offset / 256), ceil((offset + size) / 256))`, the resolution of "Step 2 ordering through ZenithGraph", so an edge the granule mapping adds is in both sets |
| reference edges D | for every resource, over its accesses by passes of N in version order, one edge a → b for every ordered pair (a before b) whose ranges overlap and at least one of which writes (RAW, WAR, WAW) |
| compiled edges C | the compiled graph's direct edges between passes of N |
| equality | the transitive closure of C over N equals the transitive closure of D over N |
| cross-queue | with the final queues of `Flux_CompiledGraph::m_xQueuePlan` after step 6, every edge of C joining passes on different queues has a path in D |
| first-write declarations | every transient carries `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`, and the core raises no `GRAPH_ERROR_FIRST_WRITE_NOT_DISCARD` for any fixture |

Fixtures that must pass: the Forward+ frame fixture of each tier, the shadow-atlas fixture (a whole-atlas clear then tile writes by four shadow views) and the bloom-chain fixture. The independent-face fixture must pass too: six passes W0 to W5 each first-write one face of one transient cube with no whole-resource clear, and a seventh pass reads the whole cube and is flagged as a side effect, so none of the seven is culled by `noConsumer` and N holds all seven. D is the six edges Wi → reader (the faces are disjoint, so no Wi–Wj pair overlaps), and the closure of C equals it: no Wi → Wj edge exists. The test proves it can see an added edge with a seeded fault: the same fixture compiled with a test-only extra `Flux_GraphInternal_AddOrdering(W0, W1)` must fail the equality naming the pair (W0, W1).

## View tests

The nine tests of [Z2-N-REND-611] (`Test_View_IDStableAcrossListOrder`, `Test_View_EditorHistoryIndependentOfMain`, `Test_View_CameraCutResetsHistory`, `Test_View_OutputExtentChangeResetsHistory`, `Test_View_PersistentStateRetired`, `Test_View_DepthModePrecisionCheck`, `Test_View_D32UnjustifiedRejected`, `Test_View_ListOrderDeterministic`, `Test_View_TextureViewEdgeToMain`), plus:

| Test | Asserts |
|---|---|
| `Test_View_DepthModeOpsMatchSlang` | CPU and Slang depth-mode operations agree at 1,000 depths per mode |
| `Test_View_CapabilityFallbackTakesD32` | a device without D24S8 gives D32F_S8 with reason `CAPABILITY` and the parity report records it ([Z2-N-REND-029]) |
| `Test_View_CapsDeferExtraViews` | requests over the tier cap are deferred and counted; the main and shadow views never are ([Z2-N-BUDGET-262]) |
| `Test_View_RoundRobinOrder` | due requests are taken by (overdue desc, ID asc) up to the cap ([Z2-N-REND-609]) |
| `Test_View_ShadowedLightSelection` | with LIGHT's lighting system, a fixture light set yields shadow views in LIGHT's priority order, local cap and key tie-break, after the main view, and their tiles reach the light records through `StageLightRecord` and the frame's scatter before any shadow view renders ([Z2-N-REND-604]) |
| `Test_View_StereoRejected` | `VIEW_KIND_STEREO` is rejected with `KIND_UNAVAILABLE` ([Z2-N-REND-602]) |
| `Test_View_InvalidRequestRejected` | zero extent, NaN camera and missing output are dropped and counted |
| `Test_View_PressureReleasesCaptureHistories` | pressure action 12 releases capture and preview states; action 13 defers creation |
| `Test_View_GroupSharesCullingWithoutCrossTalk` | Stage 5: a two-view group renders each view identically to rendering it alone |
| `Test_View_TextureCycleRejected` | Stage 5: a texture-view cycle is rejected and each view keeps its last output |
| `Test_View_ExtensionOrderAndPoints` | extensions run at their points in registration order for their view kinds; the 33rd registration fails |
| `Test_View_ShadowViewsFromLightPlans` | with LIGHT's real `Flux_LightingSystem` and two shadowed spot lights, its plans become shadow views in plan order, after the main view, with the plans' tiles as `m_xAtlasTile` and standard Z; a `PlanShadows` error leaves no shadow view |
| `Test_View_DynamicResolutionScaleFromController` | a view with a LIGHT controller takes its scale, every other view takes 1.0, and pressure action 11 pins every controller to the floor while the level lasts |
| `Test_View_CameraLightingCopied` | a main request's and an editor request's `m_xCameraLighting` reach their views' `m_xCameraLighting` unchanged, and a request without the component gives the default-constructed settings (LIGHT row L36) |
| `Test_View_RegistryCreateOutOfMemory` | an injected allocation failure in `Flux_ViewRegistry::Create` returns `FLUX_VIEW_ERROR_OUT_OF_MEMORY` |

## Culling tests

The eleven tests of `Zenith2/Docs/Rendering/Culling.md → "Culling tests"` ([Z2-N-REND-659]), `Test_HiZ_FullyOccludedZeroDraws` among them (a fully occluded object records zero draws in both phases, asserted through the culling counters and the draw-argument readback), plus:

| Test | Asserts |
|---|---|
| `Test_HiZ_ShaderMatchesReference` | 4,096 random projections and roundings agree bit for bit between the GPU and the CPU reference |
| `Test_HiZ_InvalidHistoryDrawsEverything` | on a camera cut every frustum-visible instance draws ([Z2-N-REND-646]) |
| `Test_Culling_DrawKeysSeparateMaterialSlots` | two instances of one primitive with different material slots in one bucket draw with their own materials ("Draw keys and command regions") |
| `Test_Culling_RegionRebuildOnBucketChange` | adding and removing a material rebuilds bases with no stale draw |
| `Test_Culling_OpaqueBeforeAlphaTested` | depth passes draw opaque buckets first ([Z2-N-REND-722]) |
| `Test_Culling_PositionOnlyIndexForShadows` | shadow and depth-only opaque draws use the position-only range ([Z2-N-REND-651]); two skinned meshes differing only in their skin format (J8W8 and J16W16) share one shadow bucket and one raster pool, and two meshes with different raster codes never share a shadow bucket (GEOM row GM21) |
| `Test_Culling_ShadowSkipsPhase1OnMove` | a shadow view moved past its snapping step skips the phase-1 HiZ test ([Z2-N-REND-656]) |
| `Test_Culling_CountersReadBack` | the counters equal a CPU recount of the fixture scene |
| `Test_Culling_TwoSidedTransparentPair` | a `backThenFront` material emits the pair and breaks the run on the cull mode |
| `Test_Culling_TransparentOverCapFailsFrame` | with the upload ring's cap set to a test value (RHI's test support of `Test_UploadRing_NeedAboveCapFailsFrame`, `Zenith2/Docs/Design/RHI/Tests.md → "Lifetime and upload tests"`), a view whose `16 × n` transparent bytes exceed it: the frame fails with `FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP`, RHI's log names `Flux.Transparent` as the largest contributor, the `flux.culling` warning names the view once, `Flux.Culling.TransparentEntries[<view>]` equals n, nothing is recorded or presented, and the visibility-bit history is unchanged |
| `Test_Culling_BuildNodeDataDeclaredBeforeExtension` | the transparent entries, frustum planes and `BucketScatter` records are declared at setup and written only after `ExtendReservation`; a frame forced to defer writes none of them and leaves the visibility-bit history unchanged (`Zenith2/Docs/Design/RHI/Memory.md → "Build-node upload order"`) |
| `Test_Culling_CustomCullDistance` | an instance of a flagged primitive beyond `m_uCustomData >> 16` metres of the LOD reference view is culled in the main view and in a shadow view; 65,535 never culls; an unflagged primitive ignores the bits (pending RP21) |
| `Test_Culling_BucketScatterOrderedBeforeCull` | in a frame that rebuilds the bases, the compiled graph orders `BucketScatter` before every `InstanceCull` pass, and the region bases the cull reads equal the CPU's |
| `Test_Culling_MeshletOverflowFallsBack` | Stage 5: survivors beyond 2²⁵ draw through the fallback and are counted ([Z2-N-REND-654]) |

## Path and pass tests

The five tests of [Z2-N-REND-697] (`Test_Paths_MatrixMatchesData`, `Test_Paths_MobileForcesForwardPlus`, `Test_Paths_PreviewUsesForward`, `Test_Paths_RuntimeSwitchInvalidatesHistory`, `Test_Paths_DisabledFeatureAddsNoPass`; `Test_Paths_PreviewUsesForward` lands with the Stage 5 Forward path, and until then `Test_Paths_PreviewUsesForwardPlusUntilForwardPath` pins the interim rule of `Zenith2/Docs/Design/RPROD/Views.md → "Flux_ShadingPath header"`), plus:

| Test | Asserts |
|---|---|
| `Test_Paths_PresentPassesInFrameTableOrder` (Forward+; `Test_Paths_<Path>PresentPassesInOrder` for Forward, Deferred and VisibilityBuffer) | the passes present in the compiled graph appear in the relative order of the path's FrameAnatomy table, and every RPROD-owned pass of that table not on `axFLUX_RPROD_STUBBED_PASSES` is present ([Z2-N-REND-723]; ruling Q69 (a): other areas' functions link as no-pass stubs until their implementations land); the full-list equality is the render integration tests' `Test_Render_<Path>FrameOrderMatchesFrameAnatomy` |
| `Test_Paths_AsyncFlagsMatchFrameTables` | exactly the A passes carry the async flag ([Z2-N-REND-791]) |
| `Test_Paths_ColourPassesEqualDepthNoDiscard` | every colour pass after the depth passes binds depth read-only with `EQUAL` ([Z2-N-REND-729]) |
| `Test_Paths_NoMSAAAnywhere` | no pass of any path declares a sample count above one ([Z2-N-REND-010]) |
| `Test_Paths_OneSubmitThreadNoRecordOnSubmit` | recording zones never run on the render-submit thread (cross-doc item X53) |
| `Test_Present_SwapchainSideEffect`, `Test_Present_TextureOutputMarked` | the present rules of Passes.md |

## PSO and shader tests

The tests of [Z2-N-REND-133], [Z2-N-REND-137] and [Z2-N-REND-213] (RPROD's share: `Test_PSO_KeyIncludesFormatsFromGraph`, `Test_PSO_KeyEqualityIsBytewise`, `Test_PSO_MissingShippingSkipsBucket`, `Test_PSO_MissingDevelopmentDrawsMagenta`, `Test_PSO_MissingComputeClearsOutputs`, `Test_PSO_NeverSyncCreateOnSubmitThread`, `Test_PSO_RecordedSetRoundTrip`, `Test_PSO_UnresolvedRecordedEntryWarns`, `Test_Cook_PSOPredictedSetCoversWorkloads`), plus:

| Test | Asserts |
|---|---|
| `Test_PSO_GroupGatesSceneActivation` | a scene's handle completes only after its group's PSOs are `READY` ([Z2-N-REND-202]) |
| `Test_PSO_ReloadKeepsOldUntilReady` | a reload swaps only after the replacement is `READY` ([Z2-N-REND-129]) |
| `Test_PSO_WriteMaskZeroForUnwrittenOutputs` | the builder zeroes never-written outputs ([Z2-N-REND-921]) |
| `Test_PSO_FrontFacesCounterClockwise` | both backends rasterise a CCW fixture triangle and cull a CW one ([Z2-N-REND-198]) |
| `Test_PSO_PrecacheFileRefused` | a corrupt `pso_precache.zbin` fails renderer creation in Shipping and records every PSO in development |
| `Test_ShaderLibrary_ContainerRefused` | a mismatched magic, version or tool hash is refused with `FLUX_SHADER_ERROR_CONTAINER_VERSION` naming the file ([Z2-N-REND-121]) |
| `Test_ShaderLibrary_CorruptBlobFailsPSO` | a blob whose hash mismatches puts every PSO entry created from it in `FLUX_PSO_STATE_FAILED` with reason `FLUX_PSO_FAIL_REASON_BLOB`, logs once with `(blob)`, and over the next 10 frames of misses queues no creation job ([Z2-N-REND-122]; PSO.md "PSO table and states") |
| `Test_PSO_StateMachine` | every row of the PSO state table of PSO.md, including `ABSENT` queueing exactly one job, a `FAILED` entry never re-queued on the same device, a reload returning either reason to `ABSENT`, and device-loss recovery resetting a `DEVICE` failure to `ABSENT` (an injected `PIPELINE_CREATE` on the first device, success on the recreated one) while keeping a `BLOB` failure `FAILED` |
| `Test_PSO_NullStreamBoundForAbsentAttributes` | on Vulkan (lavapipe) and D3D12 (WARP), a fixture program declaring UV1 draws a mesh whose code lacks UV1: the PSO's table has stream 7 at stride 0, the context binds `Flux_NullVertexStream` at binding 7, validation is clean and the shader reads UV1 as (0, 0) (GEOM row GM25) |
| `Test_PSOLint_ColourPassListMatchesTables` | the colour-pass program table of PSO.md "PSO lints" matches the per-pass section files |
| `Test_ShaderCompiler_ExitCodes` | each exit code of the tool table for a fixture |
| `Test_ShaderCompiler_PermutationLimitExit5` | a fixture above the limit exits 5 ([Z2-N-REND-126]) |
| `Test_ShaderCompiler_StreamSetTables` | GEOM's per-stream-set tables and link-time constants are emitted, the aggregate header is byte-identical across two runs, a program declaring UV1 specialised for a code without UV1 carries the null-stream attribute at stream 7, offset 0, stride 0; and one failing fixture per code asserts `ZSC_ERROR_STREAM_SET_CODE`, `_MISMATCH`, `_DUPLICATE`, `_LOCATION`, `_MISSING`, `_UNREACHABLE` and `_NAME` with the names each message carries (PSO.md "Compiler error codes") |
| `Test_ShaderCompiler_GeneratedHeaderSelfContained` | a generated header compiles alone and has a contract block per type ([Z2-N-CONTRACT-114]) |

## Lint fixtures

One failing fixture per lint: `Test_ShaderChecks_<Lint>_Fails` for each of the twelve shader lints (including `Test_ShaderChecks_MaterialImplicitDerivative_Fails` of [Z2-N-REND-255]), `Test_PSOLint_<Lint>_Fails` for the four PSO lints, `Test_GraphCheck_D32Unjustified_Fails`, `Test_GraphCheck_32BitUnjustified_Fails` and `Test_GraphCheck_UnwrittenAttachment_Fails` for the graph checks; each has a passing twin proving the check accepts the compliant form. The ShaderChecks gate step runs the shader-lint fixtures through the tool's `Tests/Fixtures/Lints/` and asserts each fails with its lint ([Z2-N-REND-127]).

## Frame pacing tests

| Test | Asserts |
|---|---|
| `Test_FramePacer_SwapIntervalFromRefresh` | 60 Hz → n = 1, T = 16.67 ms; 120 Hz → n = 2; 90 Hz → n = 1; 144 Hz → n = 2; 30 fps target at 60 Hz → n = 2 |
| `Test_FramePacer_DesiredTimeSequence` | synthetic timings produce `D(f) = A(f − 1) + T` and never a time in the past |
| `Test_FramePacer_HitchDoesNotCatchUp` | after a 4 × T interval the next desired time follows `now` |
| `Test_FramePacer_StateMachine` | every row of the pacer state table, including an `r.vsync` change returning to `WARMUP` |
| `Test_FramePacer_NoPacingPresentModeWaits` | with `r.vsync = 0` (MAILBOX, IMMEDIATE, sync interval 0) and n = 1 and n = 2, the decision's CPU wait is `D(f) − r.pacing.presentMarginUs` and the swap interval is 0 |
| `Test_FramePacer_DeviationStatistic` | the p95 of `|interval − T|` over a synthetic run equals the hand-computed value (the `Z2-B-RPROD-020` statistic) |
| `Test_FramePacer_LatencySlotBoundsRunAhead` | on WARP and lavapipe, frame k's build never starts before present k − framesInFlight completed |
| `Test_Latency_AttributionCarriesSequence` | an injected input's sequence reaches the present record and a latency sample is published |

## Quality tier tests

| Test | Asserts |
|---|---|
| `Test_Quality_TierIndexFromList` | the active tier ID maps to its index in the platform list; an unknown ID falls back per PLAT |
| `Test_Quality_ChangeAppliesNextFrame` | a `render.quality.*` change applies at the next build and invalidates the affected histories |
| `Test_Quality_OutOfRangeKept` | an out-of-range value keeps the previous one and logs once |
| `Test_Quality_TierFilesMatchBudgetRows` | the per-tier view caps and light cap equal the generated budget table once adopted |
| `Test_Quality_OverridesApplied` | texture-view overrides and pressure actions 9 and 10 reach `m_pxQuality`; action 11 calls `SetPressureFloor(true)` on exactly the views for which `HasDynamicResolution` is true, and `SetPressureFloor(false)` when the level falls |
| `Test_Quality_LightAggregateFilled` | a tier file setting one RPROD key and one LIGHT key yields both in the resolved `Flux_QualitySettings` (the LIGHT key in `m_xLight`), and the lighting system receives `m_xLight` |
| `Test_Quality_NoSimulationKey` | no `render.quality.*` key is simulation-affecting ([Z2-N-ARCH-919]) |

## Renderer tests

| Test | Asserts |
|---|---|
| `Test_FluxRenderer_Create_FailsAt<Step>_Unwinds` (one per step of "Create sequence") | the injected failure returns its error and every earlier step is destroyed in reverse |
| `Test_FluxRenderer_ConfigKeysRegistered` | every key of "Configuration keys" is registered with its type, range, layers and restart flag |
| `Test_FluxRenderer_ConsumesWorldInOrder` | two worlds' sections are consumed in world-tag order and the render world is unchanged afterwards |
| `Test_FluxRenderer_FullStateRebuildsDrawKeys` | after a world with `m_bFullState`, the bucket reference counts and draw-key bases equal those of a delta replay from the first frame |
| `Test_EngineExtractor_*`, `Test_RenderComponents_Registered`, `Test_InstanceDelta_Layout` | the tests of `Zenith2/Docs/Design/RPROD/Extractors.md → "Extractor tests"` (`ZenithRuntime_Tests`, the layout test in `ZenithFlux_Tests`), `Test_EngineExtractor_EnvironmentSetOnce` among them (a fixture `PHASE_INPUT` resource `Zenith.Environment.RenderBlocks`) |
| `Test_FluxRenderer_ProceduralRecordsBeforeDeltas` | a new procedural instance's record and its delta in one render world: `Flux_ProceduralApplyRecords` runs before `ApplyDeltas`, so the GPU scene resolves its pool slot and stream-set in that frame, and `Flux_ProceduralBuildFrame`'s frame reaches `Flux_AddProceduralGeometry` |
| `Test_FluxRenderer_LightFrameCallsOrdered` | a real `Flux_Renderer` with LIGHT's real `Flux_LightingSystem` (never a double of that concrete class) and a capturing profiler-hook table: in a `FITS` frame the zone `Flux.BuildGraph.LightWriteFrameData` follows `Flux.BuildGraph.ExtendReservation` and `Flux.BuildGraph.LightEndFrame` is the last LIGHT zone; in a deferred and in a failed frame no `LightWriteFrameData` zone appears and `Flux.Lighting.FramesNotSubmitted` rises by one; after a compile failure skipped in the shipping configuration `LightEndFrame` still appears once |
| `Test_FluxRenderer_CompileFailureSkipsFrameInShipping` | in the shipping configuration a compile error (a per-view test-hook pass with an unset required field, `FLUX_RG_ERROR_FIELD_UNSET`) skips the frame, increments `Flux.Graph.CompileFailures`, and records and submits nothing; the Stage 2 frame writes no swapchain image, so keeping the last image presented is not asserted here |
| `Test_FluxRenderer_CountersPublished` | every counter of `Zenith2/Docs/Design/RPROD/Diagnostics.md → "Counters"` is emitted in a frame |
| `Test_FluxRenderer_ConsoleTogglesNextFrame` | each console toggle applies at the next frame and is ignored in Shipping |
| `Test_HiZ_OverlayAddsNoPassWhenOff` | with `r.debug.hiz` off no overlay pass or bounds write exists; on, one pass exists |
| `Test_Parity_ArtefactsRetained` | a forced parity failure writes both images, the FLIP map, the difference images and both dumps |

## Render tests

| Test | Asserts |
|---|---|
| `Test_Render_ForwardPlusFrameFixture` | the full Forward+ frame of the fixture scene, rendered on both backends in one run, agrees across backends within the strictest Stage 3 parity-scene tolerance and meets four analytic expectations (clear-colour region, zero draws for the occluded instance, the null-stream UV1 colour, the transparent blend), with validation clean and no committed golden (ruling Q55) |
| `Test_Render_ForwardPlusFrameOrderMatchesFrameAnatomy` | in the fixture's fourth frame (one main view, two shadowed spot lights, no skinned instance, no Environment module, `r.debug.hiz` off, `QUALITY_TIER_WIN_DEFAULT`, every desktop main-view feature bit set except depth of field and motion blur), the compiled Forward+ frame's pass names, with view suffixes removed and runs of consecutive equal names collapsed, equal the literal sequence `axEXPECTED_FORWARD_PLUS_STAGE3` derived from FrameAnatomy's common prologue, per-view prologue, Forward+ and common epilogue tables (ruling Q69 (a)) |
| `Test_Render_FullyOccludedZeroDraws` | the render form of `Test_HiZ_FullyOccludedZeroDraws` in each path (Stage 5: all four) |
| `Test_Render_EditorViewportsIndependent` | two editor viewports and the main view render with independent TAA ([Z2-N-REND-598]) |
| `Test_Render_SkippedShadowLeaksOneFrame` | a shipping-mode shadow-bucket miss affects exactly one frame ([Z2-N-REND-211]) |

## Parity suites

| Suite | Compared | Metric and threshold | Gate |
|---|---|---|---|
| aliasing parity `Test_AliasingParity_<Scene>` | aliasing on vs off, poisoning on in both, per backend | bit-identical images, validation clean in both ([Z2-N-REND-433], [Z2-N-REND-444]) | `Test` (Stage 2 onward) |
| async parity `Test_AsyncParity_<Scene>` | `m_bAsyncCompute` on vs off | bit-identical ([Z2-N-REND-791]) | `Test` |
| backend parity `Test_BackendParity_<Scene>` | Vulkan vs D3D12, same path | surface-data channels near-exact (≤ 1 quantum), final images within `Z2-B-RPROD-031`; the depth format each view took is recorded ([Z2-N-REND-029], [Z2-N-REND-940]) | `BackendParity` (Stage 3) |
| path parity `Test_PathParity_<Scene>_<PathA>_<PathB>` | each path against the Forward reference | surface channels near-exact, final images within `Z2-B-RPROD-031`, per-feature exceptions exactly as the capability matrix states ([Z2-N-REND-941], [Z2-N-REND-696]) | `PathParity` (Stage 5) |

| Runner element | Specification |
|---|---|
| reproducibility | fixed camera path, jitter sequence, noise seeds per frame index, exposure and capture frames ([Z2-N-REND-942]) |
| surface-data capture | each path's debug output of albedo, normal, roughness, metallic, AO, emissive, velocity and depth, written to R16G16B16A16F or R32F readbacks |
| FLIP | the pinned `flip` header (NVIDIA LDR-FLIP, BSD-3-Clause; a manifest row owned by ZenithFlux tests, row RP11) at the scene's pixels-per-degree 67 |
| artefacts | on failure: both images, the FLIP map, per-channel difference images and both runs' graph dumps under `Zenith2/out/<checkout-hash>/Parity/<scene>/` ([Z2-N-REND-943]) |
| tolerances | `Zenith2/Engine/ZenithFlux/Tests/Parity/Tolerances.json` generated from `Z2-B-RPROD-031` once adopted; while pending, the suite reports and does not fail on the final-image stage, and fails on the surface-data stage |

## Parity scenes

`Zenith2/Engine/ZenithFlux/Tests/Parity/Scenes/` holds cooked-at-build scenes, each a JSON scene of instances, lights and a camera path:

| Scene | Exercises |
|---|---|
| `Opaque` | opaque and alpha-tested buckets, early-Z order, HiZ culling, TAA sequence |
| `Lights` | directional cascades, spot and point shadows, clustered lights (≤ 8 per object so Forward compares) |
| `Transparent` | sorted transparency, two-sided pairs, refraction, particles |
| `PostChain` | exposure, bloom, depth of field, motion blur, tonemap SDR |
| `Skinned` | skinned and morphed meshes in depth, colour and shadow passes, velocity |
| `Decals` | clustered, deferred and per-object decals with the matrix's per-feature tolerances |
| `DepthModes` | a 2 km outdoor view (reversed Z), an orthographic cascade and a D24 near view |
| `Views` | the main view, two editor viewports, a texture view sampled by the main view, a capture |

Every pass that appears in more than one path is exercised by at least one scene ([Z2-N-REND-797]); `Test_Parity_ScenesCoverSharedPasses` asserts it from the frame dumps.

### Parity scene path-trace object and HDR capture

<!-- elaboration -->
Stage 4 extends the scene schema and the runner for ADVGFX's references and its VT scene (row AV1 (g); `Zenith2/Docs/Design/ADVGFX/PathTracer.md → "Reference scenes"`, "Raster capture"):

| Element | Rule |
|---|---|
| `pathTrace` | an optional top-level object of a parity scene document, whose keys and domains are ADVGFX's table in "Reference scenes"; the parity scene reader validates its shape (every key present with its type and range) and passes it through to the path tracer's scene builder unread; a scene without it is unchanged; an invalid object fails the scene's cook naming the key |
| `VirtualTexture` | ADVGFX's scene `Zenith2/Engine/ZenithFlux/Tests/Parity/Scenes/VirtualTexture.scene.json` joins the scene list, run by the backend parity suite from Stage 4 and the path parity suite from Stage 5 with VT in `full` residency mode settled before frame 0 |
| HDR capture point | the runner's surface-data capture gains `hdrAfterTAA`: a readback of `Flux_ViewTargets::m_xSceneColour` after `TemporalAA` in its own `R11G11B10_UFLOAT` format, decoded to `float` on the CPU and divided by the frame's pre-exposure; requested by a capture description member, never by default |
| tests | `Test_ParityScene_PathTraceObjectValidated` (a valid object passes through, each invalid key fails naming it), `Test_ParityRunner_HDRAfterTAACapture` (a constant-radiance scene reads back its radiance within the format's rounding bound of "Raster capture") |

## Sync validation

Every graph, render, parity and GPU scene test installs RHI's test validation sink; suppressions exist only in `Zenith2/Engine/ZenithFlux/Tests/ValidationSuppressions.json` with a decision per entry ([Z2-N-REND-396]); the barrier verifier runs in every graph test ([Z2-N-REND-397]).

## Benchmarks and harnesses

| Harness | Measures | Ruling |
|---|---|---|
| `Zenith_Bench_Budgets_RPROD` (`Zenith2/Benchmarks/Budgets/RPROD/`, the Stage 1 per-system budget harness for RPROD) | the reference engine's Flux culling, prepass and forward passes (Budgets' measurement basis) on the RTX 3070 and the S24: every RPROD GPU pass row, the GPU-scene memory row, the PSO memory and on-disk cache rows, view counts per tier, the D24 far step of each workload view, the pacing deviation, the permutation count of each workload, the HiZ store-and-reload bandwidth on the S24 | the Stage 1 per-system budget ticket for RPROD |
| `Zenith_Bench_ShadingPaths` (`Zenith2/Benchmarks/ShadingPaths/`) | each path on the same workload versions: GPU time per pass, memory and bandwidth, image differences, exclusions ([Z2-N-REND-955], [Z2-N-REND-960]) | the Stage 5 shading-path research ruling (RESEARCH's methodology, Z2-D-064) |
| `Zenith_Bench_TBDR` (`Zenith2/Benchmarks/TBDR/`) | on the S24: bandwidth (Adreno counters) and GPU time of the Forward+ frame with each measured configuration of D-TBDR alternatives (1), (2) and (3) (entry document "Selection decisions RPROD owns"), including the `Z2-B-RPROD-026` store-and-reload traffic, 10 repetitions, 95% confidence intervals | the D-TBDR (open, Stage 5) ruling, under the DecisionLog acceptance rule |

Each harness has a CTest smoke run (`--smoke`: one repetition, one frame, WARP) in the `Test` step ([Z2-N-REND-960], Z2-D-084); timings on WARP and lavapipe are never budget evidence.

## Device steps

`needs-human` device steps: the RPROD budget measurement on the RTX 3070 and the S24; the Forward+ S24 bring-up capture and the S24 backend-parity device record; the D-TBDR measurement; the shading-path research runs; the physical input-to-photon confirmation of Stage 6 ([Z2-N-BUDGET-379]); each records its report under `Zenith2/Docs/Research/`.
