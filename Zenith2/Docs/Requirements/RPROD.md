# RPROD requirements — rendering production

## Purpose

This catalogue breaks the RPROD inventory items of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" (heading "RPROD Rendering production") into testable requirements `Z2-R-RPROD-NNN`. The design that specifies each requirement is `Zenith2/Docs/Design/RPROD.md` and its split files under `Zenith2/Docs/Design/RPROD/`; the specification the design elaborates is `Zenith2/Docs/Rendering.md` with `Zenith2/Docs/Rendering/RenderGraph.md`, `Zenith2/Docs/Rendering/Culling.md`, `Zenith2/Docs/Rendering/FrameAnatomy.md` and `Zenith2/Docs/Rendering/ShaderSystem.md`. The stage of a requirement is the stage whose acceptance it serves; the Stage 1 pull-forward slices are `Zenith2/Docs/Design/RPROD.md` → "Stage 1 pull-forward and API-only slices". "WARP and lavapipe" means the headless CI devices of `Zenith2/Docs/Design/RHI.md` → "Headless CI devices"; every GPU test named here runs with the validation of `Zenith2/Docs/Design/RPROD/Tests.md` → "Sync validation". The render graph back end (compile steps 3, 6 and 7, recording, submission) is RHI's catalogue; a requirement here that names an RHI test cites it as joint evidence of the front-end behaviour it checks.

## Requirements

### Z2-R-RPROD-001 ZenithFlux RPROD layout
- **Statement:** The RPROD-owned ZenithFlux public root headers, `Internal/` directories and `Tests/` directories are exactly those of the layout tables, the library declares `SUPPORTED ZENITH_GPU` (so `linux-tsan` builds no Flux source) and its dependency set equals Architecture's ZenithFlux row.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-001, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** the `SourceLayout`, `CompileCoverage` and `Libgraph` gate steps green on the landing diff (win-debug, android-arm64)
- **Design:** `Zenith2/Docs/Design/RPROD.md` → "ZenithFlux target"; `Zenith2/Docs/Design/RPROD.md` → "RPROD internal directories"; `Zenith2/Docs/Design/RPROD.md` → "Supported matrix"

### Z2-R-RPROD-002 Graph public API
- **Statement:** `Flux_RenderGraph.h`, `Flux_RasterTargets.h`, `Flux_PassContext.h`, `Flux_GraphSettings.h`, `Flux_GraphCaches.h` and `Flux_GraphErrors.h` declare exactly the specification's members plus this design's additions, one contract block per type, each header self-contained; the additions (five `Flux_RenderGraph` members, two `Flux_RGTextureDesc` members and the two headers) are the additions [Z2-N-REND-289] admits (cross-doc item X97, applied).
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-047, X97 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** the `HeaderSelfContained` and `ApiContractCheck` gate steps; `Test_RenderGraph_OrderFromVersionsOnly` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_RenderGraph header"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_RasterTargets header"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphErrors header"

### Z2-R-RPROD-003 Graph rebuilt per frame in an arena
- **Statement:** A `Flux_RenderGraph` is constructed, set up and compiled every frame on the Flux.BuildGraph node over the frame's graph arena, stores only trivially destructible captures, and nothing it allocates outlives the frame except the physical resources of `Flux_GraphCaches`.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** Z2-B-RHI-004
- **Acceptance evidence:** `Test_RenderGraph_TrivialCaptureRequired` and `Test_RenderGraph_DeterministicCompile` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_RenderGraph header"; `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux.BuildGraph"

### Z2-R-RPROD-004 Resources declared once through fields
- **Statement:** Every access of a pass is derived from its generated parameter struct's field table and its `Flux_RasterTargets`, unset non-optional fields raise `FLUX_RG_ERROR_FIELD_UNSET`, a tag mismatch raises `FLUX_RG_ERROR_VIEW_TYPE_MISMATCH`, and no separate read or write list exists.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_Error_FieldUnset` and `Test_RenderGraph_Error_ViewTypeMismatch` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Field-table walk"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Setup tables"

### Z2-R-RPROD-005 Versioned ordering
- **Statement:** Every write returns a new version through its handle, a write of a non-latest version raises `FLUX_RG_ERROR_STALE_WRITE`, and the pass order is the ZenithGraph order by descending critical-path cost then ascending declaration index, independent of declaration order for the same data flow.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-006, X134 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_OrderFromVersionsOnly`, `Test_RenderGraph_Error_StaleWrite` and `Test_RenderGraph_Error_Cycle` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 2 ordering through ZenithGraph"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "First-access widening"

### Z2-R-RPROD-006 Compile driver order
- **Statement:** `Compile()` runs steps 0 to 10 in the order of the driver table, calls RHI's steps 3, 6 and 7 through their entries, runs merging between steps 6 and 7, and reports `Flux.Graph.CompileMs`.
- **Covers:** Z2-I-RPROD-10, Z2-I-RPROD-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-056
- **Budgets:** Z2-B-RHI-004
- **Acceptance evidence:** `Test_RenderGraph_DeterministicCompile` and `Test_RenderGraph_SettingsReadOncePerCompile` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile driver"

### Z2-R-RPROD-007 Every error reported
- **Statement:** One compile returns every error the graph holds as `Flux_GraphErrorList` (first code, count, structure hash, no pointer) with the full list in `GetDiagnostics()`, and each diagnostic's text follows the diagnostic-text table.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_CompileReportsEveryError` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphErrors header"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Diagnostic text"

### Z2-R-RPROD-008 Graph error codes
- **Statement:** Each of the 21 RPROD-raised `Flux_RGError` codes is raised under exactly its condition with the pass, resource, version and range, and each has its failing fixture `Test_RenderGraph_Error_<Code>`; core diagnostics map through the core error table with their text unchanged.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** the 21 `Test_RenderGraph_Error_<Code>` fixtures in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphErrors header"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 2 ordering through ZenithGraph"

### Z2-R-RPROD-009 Settings read once per compile
- **Statement:** `Flux_GraphSettings` is read once at the start of `Compile()`, so a toggle made through the console, the debug UI or configuration applies at the next compile and never mid-frame.
- **Covers:** Z2-I-RPROD-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_SettingsReadOncePerCompile` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphSettings header"

### Z2-R-RPROD-010 Empty raster passes culled
- **Statement:** A raster pass with `ExpectedDraws(0)` and no side effect becomes clear-only when an attachment clears and is otherwise removed with its versions aliased, recorded `FLUX_RG_INFO_PASS_CULLED` with reason `noDraws`.
- **Covers:** Z2-I-RPROD-25
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_EmptyRasterPassCulled` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 1 culling and clear folding"

### Z2-R-RPROD-011 Unused passes culled
- **Statement:** Passes that produce nothing a side-effect pass or an output's final writer depends on are culled through the core with reason `noConsumer`, and side-effect passes and output chains are never culled.
- **Covers:** Z2-I-RPROD-25
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_CullNoConsumer`, `Test_RenderGraph_SideEffectNeverCulled` and `Test_RenderGraph_MarkOutputKeepsChain` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 1 culling and clear folding"

### Z2-R-RPROD-012 Clears folded into load ops
- **Statement:** A clear-only pass whose next user is a raster pass loading the version as an attachment is folded into that user's `CLEAR` load op with its predecessors contracted onto the user; any other next user keeps a standalone clear.
- **Covers:** Z2-I-RPROD-25
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_ClearFoldedIntoLoadOp` and `Test_RenderGraph_ClearKeptWhenNextUserIsCompute` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 1 culling and clear folding"

### Z2-R-RPROD-013 Merge only barrier-free compatible passes
- **Statement:** Consecutive graphics raster passes merge into one rendering scope exactly when the five conditions hold (Rendering's same-partition condition holding by construction, because step 7 never splits a merged scope), with no barrier, split half or release between them, and every blocked adjacency is recorded with its reason.
- **Covers:** Z2-I-RPROD-25
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-036, X98 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_MergeIdenticalAttachments`, `Test_RenderGraph_MergeBlockedByBarrier`, `Test_RenderGraph_MergeBlockedByLoadStore` and `Test_RenderGraph_MergeBlockedBySplitBegin` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Pass merging algorithm"

### Z2-R-RPROD-014 No tile-local merging
- **Statement:** No merge reads an attachment tile-locally and a tile-local field raises `FLUX_RG_ERROR_TILE_LOCAL_UNSUPPORTED` until D-TBDR is ruled.
- **Covers:** Z2-I-RPROD-25
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-036, D-TBDR (open, Stage 5)
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_Error_TileLocalUnsupported` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Pass merging algorithm"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Field-table walk"

### Z2-R-RPROD-015 Barrier state machine
- **Statement:** Barrier synthesis tracks every subresource run through the order and emits exactly the transition table's barrier for each access, using the neutral stages and access bits of `Flux_ToRHIAccess`, including a barrier between consecutive UAV passes.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_SubresourceMipChain` and `Test_RenderGraph_UAVAfterUAVBarrier` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 5 barrier synthesis realisation"

### Z2-R-RPROD-016 Compatible reads merged
- **Statement:** The barrier before the first reader of a version covers every subsequent same-layout read of that version on the queue, so subsequent readers get no barrier and no read is left unsynchronised.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_ReadRunLookAhead` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 5 barrier synthesis realisation"

### Z2-R-RPROD-017 One batch per pass
- **Statement:** Each pass's barriers form one batch recorded outside any rendering scope, and buffer records without a queue change are marked for the global barrier.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_SubresourceMipChain` and RHI's `Test_GraphBackend_BufferBarriersFoldIntoGlobal` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 5 barrier synthesis realisation"

### Z2-R-RPROD-018 First-use discards
- **Statement:** Every transient's first use in the frame is preceded by a discard whose source covers the last-use stages of every earlier occupant of overlapping memory, identically with aliasing on and off.
- **Covers:** Z2-I-RPROD-10, Z2-I-RPROD-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056, Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_AliasDiscardSourcesEarlierOccupants` and `Test_RenderGraph_AliasToggleNextCompile` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 5 barrier synthesis realisation"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 4 placement realisation"

### Z2-R-RPROD-019 Cross-queue records
- **Statement:** An access on a different queue than the subresource's state produces a record carrying both queues, listed as a release after the producer's last access, and an import left on another queue last frame starts from its recorded owner.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_ImportFirstAccessFromRecordedState` and RHI's `Test_RenderGraph_CrossQueueTransfer` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 5 barrier synthesis realisation"

### Z2-R-RPROD-020 Split-barrier candidates
- **Statement:** A same-queue transition with at least two passes between producer and consumer is emitted as a split candidate when split barriers are on, and `Flux_DemoteSplit` turns any candidate whose halves cross a batch or partition into an ordinary barrier recorded `FLUX_RG_INFO_SPLIT_DEMOTED`; the candidate form and the partition demotion are Rendering's since cross-doc item X98.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056, X98 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_SplitBarrierPlaced` and RHI's `Test_RenderGraph_SplitBarrierNotAcrossBatches` and `Test_GraphBackend_SplitHalvesInOnePartition` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Split-barrier candidates"

### Z2-R-RPROD-021 Independent barrier verifier
- **Statement:** In development builds and every graph test the verifier re-derives each subresource's state from the compiled barriers alone and fails the compile on any access its state does not satisfy.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_BarrierVerifierCatchesMissingBarrier` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Barrier verifier"

### Z2-R-RPROD-022 No barrier API in pass contexts
- **Statement:** `Flux_RasterContext` and `Flux_ComputeContext` expose exactly the operations of the specification's table and no command list, barrier, layout, ownership, aliasing, descriptor or resource operation.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_PassContext_ExposesNoBarrierAPI` (compile-fail fixture) and the ForbiddenCheck gate step's rule F20
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_PassContext header"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Pass context implementation"

### Z2-R-RPROD-023 Validation clean
- **Statement:** Every graph, render and parity test runs with Vulkan synchronization validation and D3D12 GPU-based validation and fails on any unsuppressed message.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** the RHI test validation sink in every `ZenithFlux_Tests` GPU case in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Tests.md` → "Sync validation"

### Z2-R-RPROD-024 First-fit aliased placement
- **Statement:** With aliasing on, live transients are grouped by placement group, sorted by (size desc, first use asc, index asc) and placed first-fit at aligned offsets across formats, and group heaps round to 4 MiB.
- **Covers:** Z2-I-RPROD-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** Z2-B-RHI-010
- **Acceptance evidence:** `Test_RenderGraph_AliasPlacementFirstFit` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 4 placement realisation"

### Z2-R-RPROD-025 Sharing only when ordered
- **Statement:** Two transients share memory only when every use of one reaches every use of the other through the reachability rows, so memory reuse across queues is always ordered by a wait.
- **Covers:** Z2-I-RPROD-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_AliasNeverAcrossUnorderedQueues` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 4 placement realisation"

### Z2-R-RPROD-026 Physical resource caches
- **Statement:** Group heaps, placed resources and dedicated resources are cached by the keys of the cache table, retire through the retirement queue after 8 unused frames (2 under pressure), and heaps shrink after 300 frames below half their size.
- **Covers:** Z2-I-RPROD-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057, Z2-D-055
- **Budgets:** Z2-B-RHI-010
- **Acceptance evidence:** `Test_RenderGraph_CacheRetiresUnused` and `Test_RenderGraph_HeapShrinkAfter300Frames` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Physical resource caches"

### Z2-R-RPROD-027 Aliasing toggle
- **Statement:** `r.graph.aliasTransients` switches placement at the next compile to dedicated cached allocations or back, with no restart and no other change to the graph.
- **Covers:** Z2-I-RPROD-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_AliasToggleNextCompile` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Step 4 placement realisation"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphSettings header"

### Z2-R-RPROD-028 First-write rule
- **Statement:** The front end rejects a partial first write of an unwritten transient region with `FLUX_RG_ERROR_PARTIAL_FIRST_WRITE` and any read of an unwritten region with `FLUX_RG_ERROR_READ_BEFORE_WRITE`, per subresource and per byte, and declares every transient to the core with `GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE` and every access with its real mode over its real range ([Z2-N-ARCH-346]).
- **Covers:** Z2-I-RPROD-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_Error_ReadBeforeWrite`, `Test_RenderGraph_Error_PartialFirstWrite`, `Test_RenderGraph_SubresourceMipChain` and `Test_RenderGraph_TransientsDeclaredPerSubresource` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "First-access widening"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "First-write rule and poisoning"

### Z2-R-RPROD-029 Poison on first use
- **Statement:** With poisoning on, every transient is filled with the pattern of its format class after its discard and before its first write, in both aliasing modes.
- **Covers:** Z2-I-RPROD-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_PoisonDetectsFalseFullCoverage` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "First-write rule and poisoning"

### Z2-R-RPROD-030 Aliasing parity
- **Statement:** Every parity scene renders bit-identical images with aliasing on and off, poisoning on in both, with validation clean in both modes.
- **Covers:** Z2-I-RPROD-11, Z2-I-RPROD-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057, Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_AliasingParity_<Scene>` for every parity scene in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites"

### Z2-R-RPROD-031 Transient memory reporting
- **Statement:** The graph publishes transient memory with aliasing, without aliasing, the savings in bytes and percent per group and in total and the 300-frame peak, in counters and in the frame dump's `memory` object.
- **Covers:** Z2-I-RPROD-11, Z2-I-RPROD-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057, Z2-D-005
- **Budgets:** Z2-B-RHI-010
- **Acceptance evidence:** `Test_RenderGraph_FrameDumpSchema` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Counters"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphCaches header"

### Z2-R-RPROD-032 Per-pass timestamps
- **Statement:** Every live pass gets a begin and end query index (two per queue position, merged passes included) when timestamps are on, and a queue with no timestamp bits reports its passes as n/a.
- **Covers:** Z2-I-RPROD-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_TimestampsPerPass` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Timestamp insertion and attribution"

### Z2-R-RPROD-033 Timings published per pass
- **Statement:** Resolved ticks are converted with the queue period, calibrated to the CPU timeline where the device allows, named with the view suffix and published to the profiler hooks and Tracy.
- **Covers:** Z2-I-RPROD-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_TimestampsPerPass` (the published zone names) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Timestamp insertion and attribution"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "On-screen profiler rows"

### Z2-R-RPROD-034 Timestamps in shipping follow the profiler
- **Statement:** Shipping builds insert timestamps only while the on-screen profiler is enabled; development builds always insert them.
- **Covers:** Z2-I-RPROD-19
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_TimestampsPerPass` in its shipping-configuration case in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Flux_GraphSettings header"

### Z2-R-RPROD-035 Front-end frame dump
- **Statement:** The front end fills every front-end key of the flux-graph-dump for every pass, resource, scope and error, culled passes with their reason, off the critical path.
- **Covers:** Z2-I-RPROD-25, Z2-I-RPROD-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_FrameDumpSchema` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Front-end frame dump keys"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Frame dump front-end content"

### Z2-R-RPROD-036 D32 needs a reason
- **Statement:** A D32F_S8 depth target whose depth reason is neither precision nor capability raises `FLUX_RG_ERROR_D32_UNJUSTIFIED`.
- **Covers:** Z2-I-RPROD-27
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-061, X97 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_View_D32UnjustifiedRejected` and `Test_GraphCheck_D32Unjustified_Fails` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks"

### Z2-R-RPROD-037 Justified 32-bit targets only
- **Statement:** A 32-bit-per-channel render target whose (format, use) pair is absent from the generated justified table raises `FLUX_RG_ERROR_32BIT_TARGET_UNJUSTIFIED`, and each list entry carries a decision ID.
- **Covers:** Z2-I-RPROD-28
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-014, X97 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphCheck_32BitUnjustified_Fails` and the generator's build failure on an entry without a decision in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks"

### Z2-R-RPROD-038 Unwritten attachments rejected
- **Statement:** A bound colour slot that no declared program writes, or a raster pass with no declared program, raises `FLUX_RG_ERROR_UNWRITTEN_ATTACHMENT` at compile from the permutation flags bits, and depth bound writable with no depth-writing pipeline raises `FLUX_RG_ERROR_DEPTH_WRITABLE_UNUSED` at record; the record-time placement and the flags encoding are Rendering's since cross-doc item X101.
- **Covers:** Z2-I-RPROD-29
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014, X101 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphCheck_UnwrittenAttachment_Fails` and `Test_RecordCheck_DepthWritableUnused` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Permutation flags bits"

### Z2-R-RPROD-039 Record-time checks
- **Statement:** In development builds and tests the contexts drop and count a program outside the declared set, an undeclared buffer, index or indirect form, a changed resource field, an unsupported blend and `BeginDrawParams` calls beyond `ExpectedDraws` (`FLUX_RECORD_CHECK_DRAW_PARAMS_EXCEEDED`, [Z2-N-REND-496]), and the frame sink fails the test.
- **Covers:** Z2-I-RPROD-29
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_RecordCheck_<Check>` for the four `Flux_RecordCheck` values, including `Test_RecordCheck_DrawParamsExceeded`, in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Record-time checks"

### Z2-R-RPROD-040 Format features at compile
- **Statement:** Attachments, storage writes, typed loads and samples of a format the format-feature table does not grant raise `FLUX_RG_ERROR_FORMAT_FEATURE_MISSING`.
- **Covers:** Z2-I-RPROD-28, Z2-I-RPROD-29
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013, Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_FormatFeatures_GraphRejectsUngrantedUse` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks"

### Z2-R-RPROD-041 Persistent and table writes rejected
- **Statement:** A write to a persistent read-only import raises `FLUX_RG_ERROR_PERSISTENT_WRITE` and a write to a GPU scene table by any pass but the scatter and growth passes raises `FLUX_RG_ERROR_TABLE_WRITE`.
- **Covers:** Z2-I-RPROD-10
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_Error_PersistentWrite` and `Test_RenderGraph_Error_TableWrite` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks"

### Z2-R-RPROD-042 No MSAA
- **Statement:** No pass, PSO key or target carries a sample count above one and `Resolve()` raises `FLUX_RG_ERROR_MSAA_UNSUPPORTED`.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_Error_MsaaUnsupported` and `Test_Paths_NoMSAAAnywhere` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Compile-time rule checks"

### Z2-R-RPROD-043 View types and contracts
- **Statement:** `Flux_View.h`, `Flux_ViewContext.h`, `Flux_DepthMode.h` and `Flux_ShadingPath.h` declare the specification's view types plus this design's additions, each with its contract block; `Flux_DepthModeOps` is a struct of static functions with the specification's operation names, `Flux_AtlasTile` is LIGHT's type and never re-declared, and `Flux_ViewRequest::m_xCameraLighting` is LIGHT's type (cross-doc item X103 (b), applied).
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059, X103 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** the `ApiContractCheck` and `HeaderSelfContained` gate steps; `Test_View_IDStableAcrossListOrder` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header"; `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_ViewContext header"; `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_AtlasTile include"

### Z2-R-RPROD-044 Stable view identity
- **Statement:** `Flux_MakeViewID` hashes (kind, owner, sub-index) per the owner table, so a view's ID is stable across frames, sessions and view-list order.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_View_IDStableAcrossListOrder` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header"

### Z2-R-RPROD-045 Independent per-view histories
- **Statement:** `Flux_ViewRegistry` owns each view's persistent state with the histories of its kind, no two views share a history resource, and an editor viewport never changes the main view's TAA history.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059, X103 (applied, core-doc batch 4), X105 (applied, core-doc batch 4)
- **Budgets:** Z2-B-RHI-001
- **Acceptance evidence:** `Test_View_EditorHistoryIndependentOfMain` and `Test_Render_EditorViewportsIndependent` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "View registry"; `Zenith2/Docs/Design/RPROD/Views.md` → "Persistent state by kind"

### Z2-R-RPROD-046 History invalidation
- **Statement:** A view's histories are invalid on its first frame, a camera cut, an output-extent change and a shading-path change, and every temporal consumer then treats them as absent; a render-extent change alone (a dynamic-resolution step) keeps them valid, each render-extent history being recreated at the new extent when its half next becomes current ([Z2-N-REND-599], [Z2-N-REND-612]).
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059, X104 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_View_CameraCutResetsHistory`, `Test_View_OutputExtentChangeResetsHistory`, `Test_View_RenderExtentChangeKeepsHistory` and `Test_Paths_RuntimeSwitchInvalidatesHistory` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "View registry"

### Z2-R-RPROD-047 Persistent state retired
- **Statement:** The state of a view absent for 60 consecutive frames is released through deferred deletion, and pressure actions 12 and 13 release capture and preview states and defer new ones.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_View_PersistentStateRetired` and `Test_View_PressureReleasesCaptureHistories` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "View registry"

### Z2-R-RPROD-048 Depth mode selection
- **Statement:** `Flux_SelectDepthMode` gives standard Z for orthographic and shadow views, reversed Z with D32F_S8 when the D24 far step exceeds the tolerance or the far plane is infinite, D24S8 otherwise, and D32F_S8 under the capability fallback.
- **Covers:** Z2-I-RPROD-08, Z2-I-RPROD-27
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-061
- **Budgets:** Z2-B-RPROD-029
- **Acceptance evidence:** `Test_View_DepthModePrecisionCheck` and `Test_View_CapabilityFallbackTakesD32` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_DepthMode header"

### Z2-R-RPROD-049 Depth-mode operations single-sourced
- **Statement:** Clear values, compare operators, projections and linearisation come from `Flux_DepthModeOps` (`GetClearDepth`, `GetOpaqueCompare`, `GetColourCompare`, `GetInvertedCompare`, `BuildProjection`, `LineariseDepth`, plus `GetFormat` and `GetClearValue`) and `DepthMode.slang` alone, and the two agree at every tested depth.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-061
- **Budgets:** none
- **Acceptance evidence:** `Test_View_DepthModeOpsMatchSlang` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_DepthMode header"

### Z2-R-RPROD-050 Deterministic view list
- **Statement:** `Flux_BuildViewList` rejects invalid and stereo requests, orders views as the specification's table with its tie-breaks, and fills each view's ID, path, depth mode, extent, feature mask, previous camera and camera lighting settings (copied from the request; cross-doc item X148, applied).
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059, Z2-D-072, X148 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_View_ListOrderDeterministic`, `Test_View_StereoRejected`, `Test_View_InvalidRequestRejected` and `Test_View_CameraLightingCopied` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction"

### Z2-R-RPROD-051 Render extent and dynamic-resolution floor
- **Statement:** Each view's render extent is `ceil(output × s)` per axis, where s is the scale LIGHT's dynamic-resolution controller of that view publishes (clamped by it to the floor) and 1.0 for a view without one (every view before LIGHT's Stage 3 lighting system exists); RPROD writes no other scale, pressure action 11 pins every controller to the floor, and passes before the upscaler run at render extent.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059, Z2-D-012
- **Budgets:** Z2-B-SHIP-011
- **Acceptance evidence:** `Test_View_DynamicResolutionScaleFromController` (including the extent arithmetic cases `ceil(output × s)` per axis) in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction"

### Z2-R-RPROD-052 Feature masks
- **Statement:** A view's feature mask is its kind's default intersected with the tier's features and the path's matrix features, then the request's set and clear bits, and a clear bit adds no pass.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Paths_DisabledFeatureAddsNoPass` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction"

### Z2-R-RPROD-053 Shadowed-light selection
- **Statement:** `Flux_BuildViewList` calls LIGHT's `PlanShadows` after building the non-shadow views and turns each `Flux_ShadowViewPlan` into one standard-Z shadow view after the main view, with the plan's tile as its output, and the tile rectangles reach the light records through LIGHT's `StageLightRecord` restaging and the frame's scatter before any shadow view renders; the selection, caps, tie-break and atlas allocator are LIGHT's, and `Flux_AtlasTile` and `Flux_ShadowViewPlan` come from LIGHT's leaf header `Flux_ShadowAtlas.h`.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_View_ShadowViewsFromLightPlans` and `Test_View_ShadowedLightSelection` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "Shadowed-light selection"

### Z2-R-RPROD-054 View caps and round-robin
- **Statement:** Views beyond the tier caps are deferred and counted, the main view and visible shadowed lights' views never are, and capture and texture requests are scheduled by (overdue desc, ID asc) at their intervals.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** Z2-B-RPROD-022, Z2-B-RPROD-023
- **Acceptance evidence:** `Test_View_CapsDeferExtraViews` and `Test_View_RoundRobinOrder` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "Round-robin scheduler"; `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction"

### Z2-R-RPROD-055 Views share one graph
- **Statement:** Per-view feature functions add each view's passes to the one frame graph, and cross-view reads go through handles the graph sees.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059, Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_View_TextureViewEdgeToMain` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions"; `Zenith2/Docs/Design/RPROD/Passes.md` → "Pass implementation rules"

### Z2-R-RPROD-056 View extensions
- **Statement:** Modules and the editor add passes only through `Flux_ViewExtension` at its four points, in registration order, for their view kinds, with at most 32 registrations.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059, Z2-D-024
- **Budgets:** none
- **Acceptance evidence:** `Test_View_ExtensionOrderAndPoints` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "View extensions"

### Z2-R-RPROD-057 Advanced multi-view
- **Statement:** View groups share CPU pre-culling and shadow views, texture views may read up to four texture views in a depth-2 acyclic chain, and every path renders a group's views as it renders them alone.
- **Covers:** Z2-I-RPROD-09
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-059, Z2-D-072
- **Budgets:** Z2-B-RPROD-022, Z2-B-RPROD-023
- **Acceptance evidence:** `Test_View_GroupSharesCullingWithoutCrossTalk` and `Test_View_TextureCycleRejected` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "Advanced multi-view"

### Z2-R-RPROD-058 Stereo stays deferred
- **Statement:** In Stage 7 a `VIEW_KIND_STEREO` request produces a two-view group whose eyes share phase-1 culling, each eye with its own view ID, its own histories and its own outputs, through the unchanged `Flux_BuildView` and feature-function signatures; until then the request is rejected with `FLUX_VIEW_ERROR_KIND_UNAVAILABLE`.
- **Covers:** Z2-I-RPROD-09
- **Stage:** S7
- **Classification:** deferred
- **Decisions:** Z2-D-072
- **Budgets:** none
- **Acceptance evidence:** the Stage 7 OpenXR acceptance run, and `Test_View_GroupSharesCullingWithoutCrossTalk` run on a two-eye stereo group in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "Advanced multi-view"

### Z2-R-RPROD-059 Shading path selection
- **Statement:** `Flux_SelectShadingPath` returns Forward+ for every mobile view, Forward for desktop preview and capture views, and the configured path otherwise, switchable at runtime on desktop.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Paths_MobileForcesForwardPlus`, `Test_Paths_PreviewUsesForward` and `Test_Paths_RuntimeSwitchInvalidatesHistory` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_ShadingPath header"

### Z2-R-RPROD-060 Capability matrix data
- **Statement:** `CapabilityMatrix.json` carries Rendering's matrix row for row, generates the constexpr matrix, and a test fails when the two disagree.
- **Covers:** Z2-I-RPROD-21, Z2-I-RPROD-22, Z2-I-RPROD-23, Z2-I-RPROD-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Paths_MatrixMatchesData` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Views.md` → "Capability matrix data file"

### Z2-R-RPROD-061 Feature function surface
- **Statement:** `Flux_Features.h` declares every per-view feature function with exactly its per-pass signature, and the prologue functions are declared internally and called by the renderer alone.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** the `HeaderSelfContained` and `ApiContractCheck` gate steps; `Test_Paths_DeclarationOrderMatchesFrameTables` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Flux_Features header"; `Zenith2/Docs/Design/RPROD/Passes.md` → "Prologue feature functions"

### Z2-R-RPROD-062 Frame declaration order
- **Statement:** Each path function calls its feature functions in its frame table's order, guarded by the view's features and the matrix, and the declared pass names equal the table.
- **Covers:** Z2-I-RPROD-21, Z2-I-RPROD-22, Z2-I-RPROD-23, Z2-I-RPROD-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Paths_DeclarationOrderMatchesFrameTables` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions"

### Z2-R-RPROD-063 Forward+ full frame
- **Statement:** The Forward+ frame runs every pass of its table on Vulkan, D3D12 and the S24 with validation clean and matches its golden images.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-RPROD-005, Z2-B-RPROD-006, Z2-B-RPROD-007, Z2-B-RPROD-008, Z2-B-RPROD-009, Z2-B-RPROD-010
- **Acceptance evidence:** `Test_Render_ForwardPlusFrameFixture` in the `Test` gate step on WARP and lavapipe (win-debug, win-release); the Stage 3 S24 bring-up capture (needs-human device step)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Render tests"

### Z2-R-RPROD-064 Forward full frame
- **Statement:** The Forward frame runs its table on Windows with per-object lists at most the per-object light cap per object, and matches Forward+ surface channels on scenes within the cap.
- **Covers:** Z2-I-RPROD-22
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-RPROD-011, Z2-B-RPROD-012, Z2-B-RPROD-013
- **Acceptance evidence:** `Test_PathParity_Lights_Forward_ForwardPlus` in the `PathParity` gate step (win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites"

### Z2-R-RPROD-065 Deferred full frame
- **Statement:** The Deferred frame runs its table on Windows with the G-buffer formats of the specification, deferred decals before any reader and compute lighting.
- **Covers:** Z2-I-RPROD-23
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-RPROD-014
- **Acceptance evidence:** `Test_PathParity_<Scene>_Deferred_Forward` for every parity scene in the `PathParity` gate step (win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions"

### Z2-R-RPROD-066 Visibility-buffer full frame
- **Statement:** The visibility-buffer frame runs its table on Windows with mesh shaders or the compute fallback, material classification and resolve, and velocity reconstruction.
- **Covers:** Z2-I-RPROD-24
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-RPROD-015, Z2-B-RPROD-016, Z2-B-RPROD-017, Z2-B-RPROD-018, Z2-B-RPROD-019
- **Acceptance evidence:** `Test_PathParity_<Scene>_VisibilityBuffer_Forward` in the `PathParity` gate step (win-release) and `Test_HiZ_MeshletFallbackMatchesMeshShader` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Path frame functions"

### Z2-R-RPROD-067 Early-Z order in every path
- **Statement:** Every depth-writing pass draws opaque buckets before alpha-tested ones, and every subsequent colour pass binds depth read-only with the equal compare and no discard.
- **Covers:** Z2-I-RPROD-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_Culling_OpaqueBeforeAlphaTested` and `Test_Paths_ColourPassesEqualDepthNoDiscard` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Pass implementation rules"

### Z2-R-RPROD-068 Async flags exactly as the tables
- **Statement:** Exactly the passes marked A carry the async flag, results are identical whichever queue runs them, and the copy-queue flag marks streaming uploads.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Paths_AsyncFlagsMatchFrameTables` and `Test_AsyncParity_<Scene>` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Async-compute flags"

### Z2-R-RPROD-069 Pass ownership
- **Statement:** Each pass is implemented in its directory by the area of its budget row, matches its per-pass table field for field, and edits that table in the same diff when it changes.
- **Covers:** Z2-I-RPROD-21, Z2-I-RPROD-22, Z2-I-RPROD-23, Z2-I-RPROD-24
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-085
- **Budgets:** none
- **Acceptance evidence:** review against `Zenith2/Docs/Rendering/PerPass/Common.md` in each pass ticket; `Test_Paths_DeclarationOrderMatchesFrameTables` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Pass ownership and files"; `Zenith2/Docs/Design/RPROD/Passes.md` → "Pass implementation rules"

### Z2-R-RPROD-070 Present pass
- **Statement:** A swapchain-output view ends with a side-effect `Present` pass transitioning the image to `PRESENT`; any other view marks its output instead.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** Z2-B-RPROD-020
- **Acceptance evidence:** `Test_Present_SwapchainSideEffect` and `Test_Present_TextureOutputMarked` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Present pass realisation"

### Z2-R-RPROD-071 Recording never on the submit thread
- **Statement:** Flux.Record sub-jobs run with any-worker affinity and the render-submit thread only submits and presents.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RHI-005
- **Acceptance evidence:** `Test_Paths_OneSubmitThreadNoRecordOnSubmit` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux.Record"; `Zenith2/Docs/Design/RPROD.md` → "Frame-graph nodes and declared access"

### Z2-R-RPROD-072 HiZ in every path and view
- **Statement:** Two-phase HiZ occlusion culling runs for every main, editor and texture view of every path, and for every capture view whose feature mask carries `FLUX_VIEW_FEATURE_HIZ_OCCLUSION`, inside the GPU-scene pipeline after frustum culling.
- **Covers:** Z2-I-RPROD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** Z2-B-RPROD-005, Z2-B-RPROD-008
- **Acceptance evidence:** `Test_HiZ_FullyOccludedZeroDraws` and `Test_HiZ_DisocclusionSameFrame` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Two-phase sequence per view"

### Z2-R-RPROD-073 Phase 1 inputs
- **Statement:** Phase 1 tests last frame's visible instances with current bounds, the previous view-projection, nearest depth in the previous camera space and last frame's unmodified pyramid.
- **Covers:** Z2-I-RPROD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** Z2-B-RPROD-005
- **Acceptance evidence:** `Test_HiZ_DisocclusionSameFrame` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence"

### Z2-R-RPROD-074 Phase 2 inputs
- **Statement:** Phase 2 tests every instance phase 1 did not draw against this frame's pyramid with the current view-projection.
- **Covers:** Z2-I-RPROD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** Z2-B-RPROD-008
- **Acceptance evidence:** `Test_HiZ_DisocclusionSameFrame` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence"

### Z2-R-RPROD-075 Visibility bits union
- **Statement:** The current visibility-bits half is cleared before phase 1 and set by both emits, so it holds the union for next frame.
- **Covers:** Z2-I-RPROD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** none
- **Acceptance evidence:** `Test_HiZ_DisocclusionSameFrame` (the next-frame bits) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence"

### Z2-R-RPROD-076 Invalid history culling
- **Statement:** With an invalid history phase 1 has no candidates and phase 2 tests against an all-infinity pyramid, so every frustum-visible instance draws.
- **Covers:** Z2-I-RPROD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** none
- **Acceptance evidence:** `Test_HiZ_InvalidHistoryDrawsEverything` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence"

### Z2-R-RPROD-077 Pyramid format
- **Statement:** The pyramid is RG16F linear view depth, max channel rounded up and min channel rounded down, with infinity for cleared, NaN and far texels, at the render extent with the full mip chain.
- **Covers:** Z2-I-RPROD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** Z2-B-RPROD-007
- **Acceptance evidence:** `Test_HiZ_RoundingConservative` and `Test_HiZ_InfinityRule` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZ pyramid resources"; `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZ build dispatch"

### Z2-R-RPROD-078 Single-pass downsampler
- **Statement:** The pyramid is built by one SPD dispatch with a cleared counter and full-coverage mips, folding odd edges so every mip stays conservative.
- **Covers:** Z2-I-RPROD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** Z2-B-RPROD-007
- **Acceptance evidence:** `Test_HiZ_OddDimensionFold` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZ build dispatch"

### Z2-R-RPROD-079 Persistent pyramid halves
- **Statement:** Each view's two pyramid halves are persistent imports: the post-phase-1 and final builds write the current half and the final version is marked output for next frame and SSR.
- **Covers:** Z2-I-RPROD-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** none
- **Acceptance evidence:** `Test_HiZ_DisocclusionSameFrame` and `Test_View_PersistentStateRetired` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZ pyramid resources"

### Z2-R-RPROD-080 Occlusion test
- **Statement:** The occlusion test is the five-step rule in the one shared module `HiZTest.slang`, keeping near-plane-crossing and degenerate candidates, identical in every depth mode; the module's location under `Zenith2/Shaders/Common/` is Rendering's since cross-doc item X100 (a).
- **Covers:** Z2-I-RPROD-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060, X100 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_HiZ_ProjectionAnalytic`, `Test_HiZ_MipSelection` and `Test_HiZ_DepthModeParity` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Culling files"; `Zenith2/Docs/Design/RPROD/Culling.md` → "CPU reference implementation"

### Z2-R-RPROD-081 CPU reference matches the GPU
- **Statement:** A test-only CPU reference of the projection, mip selection and rounding matches the shader bit for bit.
- **Covers:** Z2-I-RPROD-03, Z2-I-RPROD-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060, X100 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_HiZ_ShaderMatchesReference` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "CPU reference implementation"

### Z2-R-RPROD-082 Shadow-view culling
- **Statement:** Shadow views cull by frustum and distance, add two-phase HiZ against their own pyramid where the tier enables it, and skip phase-1 HiZ when moved past the snapping step or invalid.
- **Covers:** Z2-I-RPROD-04
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** Z2-B-LIGHT-002
- **Acceptance evidence:** `Test_Culling_ShadowSkipsPhase1OnMove` and `Test_Culling_PositionOnlyIndexForShadows` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Shadow view culling realisation"

### Z2-R-RPROD-083 Culling counters
- **Statement:** Tested, frustum-culled, occlusion-culled and drawn counts per view and phase are read back and published with the frame they describe, at most `r.framesInFlight` frames after it (the latency cross-doc item X100 (c) writes into [Z2-N-REND-657]).
- **Covers:** Z2-I-RPROD-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060, Z2-D-005, X100 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_Culling_CountersReadBack` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Culling counters readback"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Counters"

### Z2-R-RPROD-084 HiZ debug overlay
- **Statement:** `r.debug.hiz` overlays the chosen mip and channel and the culled rectangles coloured by phase, and adds no pass when off.
- **Covers:** Z2-I-RPROD-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** none
- **Acceptance evidence:** `Test_HiZ_OverlayAddsNoPassWhenOff` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZ debug overlay"

### Z2-R-RPROD-085 Zero-draw occlusion render test
- **Statement:** A fully occluded object records zero draws in both phases on both backends.
- **Covers:** Z2-I-RPROD-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** none
- **Acceptance evidence:** `Test_HiZ_FullyOccludedZeroDraws` and `Test_Render_FullyOccludedZeroDraws` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Tests.md` → "Culling tests"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Render tests"

### Z2-R-RPROD-086 Mobile store-and-reload budget
- **Statement:** On the S24 the depth store and reload between the prepass phases is measured against its bandwidth row by the RPROD harness and the D-TBDR harness.
- **Covers:** Z2-I-RPROD-01
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060, Z2-D-036, D-TBDR (open, Stage 5)
- **Budgets:** Z2-B-RPROD-026
- **Acceptance evidence:** the RPROD budget harness S24 report under `Zenith2/Docs/Research/Budgets/RPROD/` (needs-human device step)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Two-phase sequence per view"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Benchmarks and harnesses"

### Z2-R-RPROD-087 Meshlet two-phase culling
- **Statement:** The visibility-buffer path tests each surviving instance's meshlets for frustum, normal cone and HiZ in both phases and sorts survivors deterministically.
- **Covers:** Z2-I-RPROD-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** Z2-B-RPROD-015
- **Acceptance evidence:** `Test_HiZ_MeshletFallbackMatchesMeshShader` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Meshlet culling realisation"

### Z2-R-RPROD-088 Meshlet overflow
- **Statement:** Survivors beyond 2^25 keep the first in sorted order, the rest draw through the fallback, and the overflow is counted.
- **Covers:** Z2-I-RPROD-06
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-060
- **Budgets:** none
- **Acceptance evidence:** `Test_Culling_MeshletOverflowFallsBack` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Meshlet culling realisation"

### Z2-R-RPROD-089 Indirect compaction
- **Statement:** Culling writes compacted `Flux_IndirectDrawIndexed` arguments, draw records and a count per bucket, so culled objects never reach the vertex stage and the CPU learns no visibility.
- **Covers:** Z2-I-RPROD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060, Z2-D-054, X99 (applied, core-doc batch 4)
- **Budgets:** Z2-B-RPROD-005, Z2-B-RPROD-008
- **Acceptance evidence:** `Test_HiZ_FullyOccludedZeroDraws` and `Test_HiZ_CompactionDeterministic` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence"

### Z2-R-RPROD-090 Draw keys
- **Statement:** The unit of compaction is the draw key (material slot, LOD), with per-command counters (`m_xCommandCounters`), so instances of one primitive with different material slots draw with their own materials; the unit and the renamed field are Rendering's since cross-doc item X99.
- **Covers:** Z2-I-RPROD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, X99 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_Culling_DrawKeysSeparateMaterialSlots` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Draw keys and command regions"

### Z2-R-RPROD-091 Command regions sized on the CPU
- **Statement:** Each bucket's command region and each view's buffers are sized on the CPU from the registered slots and candidates, and a region rebuild is ordered before every cull pass.
- **Covers:** Z2-I-RPROD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, X99 (applied, core-doc batch 4)
- **Budgets:** Z2-B-RPROD-001
- **Acceptance evidence:** `Test_Culling_RegionRebuildOnBucketChange` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Draw keys and command regions"; `Zenith2/Docs/Design/RPROD/Culling.md` → "BucketScatter"

### Z2-R-RPROD-092 Deterministic compaction
- **Statement:** Indirect arguments and visible-instance lists are byte-identical over runs and across Vulkan and D3D12, through the per-command sort.
- **Covers:** Z2-I-RPROD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060, X99 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_HiZ_CompactionDeterministic` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Instance culling dispatch sequence"; `Zenith2/Docs/Design/RPROD/Culling.md` → "CullSort"

### Z2-R-RPROD-093 Bucket registry
- **Statement:** Buckets are dense reference-counted indices per bucket key, keyed by the program, the permutation with the raster stream-set code and the raster state, the shadow bucket by the shadow-depth program's position-only specialisation of the mesh's raster code, so two raster codes never share a bucket and one bucket binds one pool set; acquired by the material-slot cache, listed opaque before alpha-tested, capped at 65,535 with overflow counted; the raster-code reading is the GEOM ruling X94 records in Rendering.
- **Covers:** Z2-I-RPROD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-054, X94 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_Culling_OpaqueBeforeAlphaTested`, `Test_Culling_RegionRebuildOnBucketChange` and `Test_Culling_PositionOnlyIndexForShadows` (the skin-format and raster-code cases) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Bucket registry"

### Z2-R-RPROD-094 Transparent command path
- **Statement:** Transparent entries are collected, sorted back to front by bounds-centre depth plus `sortBias` with GEOM's two-sided pairs, grouped into runs by bucket and cull mode, emitted as fixed non-compacted slots with GPU-written instance counts and drawn run by run; the sort key's bias and tie-breaks and the cull-mode run break are Rendering's since cross-doc item X100 (d).
- **Covers:** Z2-I-RPROD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-060, X100 (applied, core-doc batch 4)
- **Budgets:** Z2-B-RPROD-010
- **Acceptance evidence:** `Test_HiZ_TransparentOrderPreserved` and `Test_Culling_TwoSidedTransparentPair` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Transparent collection and emission"

### Z2-R-RPROD-095 Compiler tool target
- **Statement:** `ZenithShaderCompiler` is a host tool in `ZENITH_TOOLS_HOST` linking Slang and DXC, with the file map of this design, and no runtime target links it, Slang or DXC.
- **Covers:** Z2-I-RPROD-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` gate step's link-surface stage and the `CompileCoverage` step
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "ZenithShaderCompiler files"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Ownership of the shader toolchain"

### Z2-R-RPROD-096 Offline compilation only
- **Statement:** Shaders are compiled by the tool at build time or in watch mode alone, and no game, editor or Player process compiles shader source.
- **Covers:** Z2-I-RPROD-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** the `Libgraph` gate step and `Test_PSO_NeverSyncCreateOnSubmitThread` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline"

### Z2-R-RPROD-097 Deterministic compiler
- **Statement:** Identical inputs, flags and tool versions produce byte-identical outputs, and headers are rewritten only when their bytes change.
- **Covers:** Z2-I-RPROD-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025, Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderCompiler_DeterministicOutput` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline"

### Z2-R-RPROD-098 Generated headers
- **Statement:** Every program yields a generated header with the parameter struct, size and offset asserts, field table, IDs, permutation struct, vertex tables and contract blocks, including only the three permitted headers.
- **Covers:** Z2-I-RPROD-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderCompiler_HeaderOffsetsMatchReflection` and `Test_ShaderCompiler_GeneratedHeaderSelfContained` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline"

### Z2-R-RPROD-099 Shared GPU structs
- **Statement:** `--shared-types` emits every `[FluxShared]` struct once with its asserts and rejects illegal member types, duplicates and misplaced declarations.
- **Covers:** Z2-I-RPROD-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-015
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderCompiler_SharedTypesLayoutMatchesReflection`, `Test_ShaderCompiler_SharedTypeRejected`, `Test_ShaderCompiler_SharedDuplicateRejected` and `Test_ShaderCompiler_SharedLocationRejected` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline"

### Z2-R-RPROD-100 Compiler errors and exit codes
- **Statement:** Every compiler error prints its ZSC code and maps to the exit code table.
- **Covers:** Z2-I-RPROD-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderCompiler_ExitCodes` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler error codes"

### Z2-R-RPROD-101 Stale output check
- **Statement:** `--check` exits 3 listing every stale output, and the ShaderChecks gate step runs it after the build.
- **Covers:** Z2-I-RPROD-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderCompiler_CheckDetectsStale` in the `Test` gate step (win-debug, win-release) and the `ShaderChecks` gate step
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation"

### Z2-R-RPROD-102 Watch mode and reload journal
- **Statement:** Watch mode recompiles affected programs after a 200 ms debounce, writes outputs by rename and appends one journal record per program, and a parameter-layout change of an engine program is never hot-loaded.
- **Covers:** Z2-I-RPROD-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** Z2-B-EDIT-003
- **Acceptance evidence:** `Test_ShaderCompiler_WatchJournalAtomic` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "ZenithShaderCompiler files"; `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO table and states"

### Z2-R-RPROD-103 Runtime shader library
- **Statement:** The runtime loads `.zshader` containers, refuses a mismatched version with `FLUX_SHADER_ERROR_CONTAINER_VERSION`, reads each permutation's flags bits, verifies blob hashes at first use and marks every PSO of a corrupt blob `FLUX_PSO_STATE_FAILED` with reason `FLUX_PSO_FAIL_REASON_BLOB` and no further creation job.
- **Covers:** Z2-I-RPROD-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025, Z2-D-038, X101 (applied, core-doc batch 4), X144 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderLibrary_ContainerRefused` and `Test_ShaderLibrary_CorruptBlobFailsPSO` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader blob loading"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Permutation flags bits"

### Z2-R-RPROD-104 GEOM compiler additions
- **Statement:** The compiler accepts `--stream-set-list`, emits the per-stream-set vertex tables and constants and the material reflection files, and knows GEOM's shared structs and attributes.
- **Covers:** Z2-I-RPROD-14, Z2-I-RPROD-32
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062, Z2-D-015, X138 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderCompiler_StreamSetTables` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler additions adopted from GEOM"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler error codes"

### Z2-R-RPROD-105 Enumerated permutations
- **Statement:** Every permutation is produced by the seven-step enumeration and compiled offline, the key formula is shared by C++ and the compiler, and a missing permutation at runtime is a missing PSO, never a compile.
- **Covers:** Z2-I-RPROD-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderCompiler_PermutationKeyFormula` and `Test_ShaderCompiler_PermutationRequiresCollapses` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline"

### Z2-R-RPROD-106 Permutation limit
- **Statement:** The compiler exits 5 and the cook fails when a game's permutation total exceeds the limit row, and while the row is pending the count is reported without failing.
- **Covers:** Z2-I-RPROD-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** Z2-B-RPROD-024
- **Acceptance evidence:** `Test_ShaderCompiler_PermutationLimitExit5` in the `Test` gate step (win-debug, win-release); GEOM's cook limit tests
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Permutation limit"

### Z2-R-RPROD-107 PSO keys
- **Statement:** A PSO is keyed by the 56-byte `Flux_PSOKey`, equality is bytewise, the hash is for lookup only, and attachment formats come from the pass's resolved targets.
- **Covers:** Z2-I-RPROD-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_KeyIncludesFormatsFromGraph` and `Test_PSO_KeyEqualityIsBytewise` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Flux_PSO header"

### Z2-R-RPROD-108 Fixed state tables
- **Statement:** Blend, stencil and depth-bias states are indices into the fixed tables, and changing a table bumps the shader container version.
- **Covers:** Z2-I-RPROD-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_KeyEqualityIsBytewise` (the index cases) in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Fixed state tables"

### Z2-R-RPROD-109 PSO table states
- **Statement:** Each key's entry moves through the four `Flux_PSOState` values exactly as the PSO state table gives, `FAILED` carrying a `Flux_PSOFailReason`, the state and the reason stored together as one atomic 16-bit state word: an `ABSENT` miss queues one creation job; a `BLOB` failure (a corrupt blob or a missing permutation) is never re-queued for the run and returns to `ABSENT` only on a shader reload; a `DEVICE` failure (`PIPELINE_CREATE`, `SHADER_BLOB_INVALID`) is never re-queued on the same device and returns to `ABSENT` when device-loss recovery recreates the device; each is logged once per key with its reason; record jobs read the table lock-free, a reload keeps the old pipeline until its replacement is ready, and recovery rebuilds the table; `FAILED` is Rendering's since cross-doc items X101 and X144.
- **Covers:** Z2-I-RPROD-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025, X101 (applied, core-doc batch 4), X144 (applied, core-doc batch 4)
- **Budgets:** Z2-B-RPROD-002
- **Acceptance evidence:** `Test_PSO_StateMachine` (including the device-loss reset of a `DEVICE` failure and the kept `BLOB` failure) and `Test_PSO_ReloadKeepsOldUntilReady` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO table and states"

### Z2-R-RPROD-110 PSO builder
- **Statement:** The builder expands a key into an RHI desc with the generated vertex input, fixed states, zero write masks for never-written outputs and counter-clockwise front faces.
- **Covers:** Z2-I-RPROD-16, Z2-I-RPROD-32
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025, Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_WriteMaskZeroForUnwrittenOutputs` and `Test_PSO_FrontFacesCounterClockwise` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO builder"

### Z2-R-RPROD-111 Precache cook step
- **Statement:** The cook builds each game's precache set as the union of its recorded and predicted sets in boot, global, scene and cell groups, warns on and drops unresolved recorded entries, and writes `pso_precache.zbin`.
- **Covers:** Z2-I-RPROD-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_RecordedSetRoundTrip`, `Test_PSO_UnresolvedRecordedEntryWarns` and `Test_Cook_PSOPredictedSetCoversWorkloads` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache cook step"

### Z2-R-RPROD-112 Precache file reader
- **Statement:** The runtime refuses a malformed precache file, failing renderer creation in Shipping and recording every PSO in development.
- **Covers:** Z2-I-RPROD-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_PrecacheFileRefused` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache file reader"

### Z2-R-RPROD-113 Creation schedule
- **Statement:** PSOs are created by background jobs per group, the first frame waits for the boot group, scenes and cells activate after their group, and no PSO is created on the submit thread or in a lambda.
- **Covers:** Z2-I-RPROD-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** Z2-B-RPROD-002
- **Acceptance evidence:** `Test_PSO_GroupGatesSceneActivation` and `Test_PSO_NeverSyncCreateOnSubmitThread` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO creation schedule realisation"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache API"

### Z2-R-RPROD-114 Precache API
- **Statement:** `Flux_PSOPrecacher::RequestGroup` returns an operation handle per group, reports unknown groups and a full request queue, and is callable from any thread.
- **Covers:** Z2-I-RPROD-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_GroupGatesSceneActivation` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache API"

### Z2-R-RPROD-115 PSO misses recorded in development
- **Statement:** With `--record-psos` every created key absent from the precache set is appended once to the misses file in the recorded-set format.
- **Covers:** Z2-I-RPROD-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_RecordedSetRoundTrip` (the misses file case) in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO recording in development"

### Z2-R-RPROD-116 Missing-PSO fallback per build
- **Statement:** A missing PSO skips the draw or bucket in Shipping, draws magenta in development, skips compute passes with cleared or poisoned outputs, never compiles or creates synchronously, and counts every fallback.
- **Covers:** Z2-I-RPROD-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025, X101 (applied, core-doc batch 4), X144 (applied, core-doc batch 4)
- **Budgets:** Z2-B-RPROD-025
- **Acceptance evidence:** `Test_PSO_MissingShippingSkipsBucket`, `Test_PSO_MissingDevelopmentDrawsMagenta` and `Test_PSO_MissingComputeClearsOutputs` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Missing-PSO fallback realisation"

### Z2-R-RPROD-117 Magenta fallback never misses
- **Statement:** The magenta fallback's PSOs are in the boot group for every format set and stream-set of the predicted set.
- **Covers:** Z2-I-RPROD-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_Cook_PSOPredictedSetCoversWorkloads` (the fallback group case) in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Precache cook step"

### Z2-R-RPROD-118 Zero misses in qualification
- **Statement:** A qualification run records zero missing-PSO events.
- **Covers:** Z2-I-RPROD-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025, Z2-D-012
- **Budgets:** Z2-B-RPROD-025
- **Acceptance evidence:** the Stage 3 workload evidence records (MP-COUNTER) in the Stage 3 acceptance
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Missing-PSO fallback realisation"

### Z2-R-RPROD-119 PSO counters
- **Statement:** The PSO counters exist, feed the profiler and telemetry, and are reported in every cold-versus-warm run.
- **Covers:** Z2-I-RPROD-16, Z2-I-RPROD-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025, Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_MissingShippingSkipsBucket` (counter cases) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Counters"

### Z2-R-RPROD-120 Early-Z lints
- **Statement:** `ShaderChecks.DiscardOnlyInAlphaTest`, `ShaderChecks.NoDepthExport` and `PSO.ColourPassEqualDepth` fail their fixtures and pass their compliant twins.
- **Covers:** Z2-I-RPROD-26
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** the lint fixtures in the `ShaderChecks` gate step and `Test_PSOLint_ColourPassEqualDepth_Fails` in the `Test` gate step (win-debug, win-release); `Test_PSOLint_ColourPassListMatchesTables` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation"; `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO lints"

### Z2-R-RPROD-121 Conditional output lint
- **Statement:** `ShaderChecks.ConditionalOutputWrite` and `PSO.UnwrittenOutputMaskZero` fail their fixtures.
- **Covers:** Z2-I-RPROD-29
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** the lint fixtures in the `ShaderChecks` gate step in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation"; `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO lints"

### Z2-R-RPROD-122 Half-precision lints
- **Statement:** `ShaderChecks.NoMin16Float` fails any minimum-precision type and `ShaderChecks.HalfRatioReport` reports the float/half ratio per entry point.
- **Covers:** Z2-I-RPROD-30
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** the lint fixtures in the `ShaderChecks` gate step
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation"

### Z2-R-RPROD-123 Back-face PSO lint
- **Statement:** `PSO.OpaqueCullBack` fails an opaque or alpha-tested PSO with culling off unless its material is two-sided, and two-sided PSOs are counted.
- **Covers:** Z2-I-RPROD-31
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** `Test_PSOLint_OpaqueCullBack_Fails` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO lints"

### Z2-R-RPROD-124 Binding and vocabulary lints
- **Statement:** `MaterialExplicitGradients`, `VertexFormatVocabulary`, `ParamsSize`, `BindlessThroughAccessors`, `NoGlobalBindings` and `TileLocalRejected` fail their fixtures.
- **Covers:** Z2-I-RPROD-32, Z2-I-RPROD-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-054, Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** the lint fixtures in the `ShaderChecks` gate step in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation"

### Z2-R-RPROD-125 Lint allowlist needs decisions
- **Statement:** A lint exception exists only as an allowlist entry with a decision ID.
- **Covers:** Z2-I-RPROD-26
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-014
- **Budgets:** none
- **Acceptance evidence:** the `ShaderChecks` gate step failing an entry without a decision
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Shader lint implementation"

### Z2-R-RPROD-126 Vertex input from generated tables
- **Statement:** Runtime PSO creation builds vertex input from the program's generated layout table for the key's stream-set, never from hand-written layouts.
- **Covers:** Z2-I-RPROD-32
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_FrontFacesCounterClockwise` (the layout case) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO builder"

### Z2-R-RPROD-127 Swap interval from refresh
- **Statement:** The pacer computes n = max(1, floor(T*/R + 1/64)) and T = n × R from the reported or fallback refresh.
- **Covers:** Z2-I-RPROD-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-RPROD-020
- **Acceptance evidence:** `Test_FramePacer_SwapIntervalFromRefresh` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer"

### Z2-R-RPROD-128 Desired present times
- **Statement:** In interval mode each frame's desired present time is the previous actual present plus T, never in the past, realised per platform, and a hitch does not trigger catch-up.
- **Covers:** Z2-I-RPROD-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-RPROD-020
- **Acceptance evidence:** `Test_FramePacer_DesiredTimeSequence` and `Test_FramePacer_HitchDoesNotCatchUp` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer"

### Z2-R-RPROD-129 Pacer state machine
- **Statement:** The pacer moves between `WARMUP`, `PACING` and `SUSPENDED` exactly as its table gives, a swapchain, present-mode, `r.vsync` or `r.pacing.*` change returning it to `WARMUP`.
- **Covers:** Z2-I-RPROD-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Test_FramePacer_StateMachine` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer"

### Z2-R-RPROD-130 Latency slot bounds run-ahead
- **Statement:** After each present the pacer waits for the latency slot, so a frame's build never starts before the present frames-in-flight earlier completed.
- **Covers:** Z2-I-RPROD-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RHI-008
- **Acceptance evidence:** `Test_FramePacer_LatencySlotBoundsRunAhead` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer"

### Z2-R-RPROD-131 Pacing deviation measured
- **Statement:** The pacer publishes the interval and its deviation from T, and the row's statistic is the p95 of the deviation over the measured interval.
- **Covers:** Z2-I-RPROD-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-RPROD-020
- **Acceptance evidence:** `Test_FramePacer_DeviationStatistic` in the `Test` gate step (win-debug, win-release); the Stage 3 workload records
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Counters"

### Z2-R-RPROD-132 Input-to-photon attribution
- **Statement:** Each present records its render world's input sequence and sample time and publishes the latency when the display time is known.
- **Covers:** Z2-I-RPROD-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-027, Z2-B-RPROD-021
- **Acceptance evidence:** `Test_Latency_AttributionCarriesSequence` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Input-to-photon attribution"

### Z2-R-RPROD-133 Quality settings struct
- **Statement:** `Flux_QualitySettings` holds RPROD's tier-dependent values, each read from its `render.quality.*` key, and LIGHT's `Flux_LightQualitySettings m_xLight` filled by LIGHT's reader; RPROD declares no upscaler enumeration, no resolution range and no anisotropy member, and owns the struct and the tier mechanism while each area owns the meaning of its fields.
- **Covers:** Z2-I-RPROD-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059, X103 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_Quality_TierIndexFromList` and `Test_Quality_LightAggregateFilled` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Quality settings"

### Z2-R-RPROD-134 Default tier per platform
- **Statement:** Until Stage 5 each platform runs one tier, `QUALITY_TIER_WIN_DEFAULT` or `QUALITY_TIER_S24_DEFAULT`, carrying the desktop and mobile values of the per-pass tables and of LIGHT's tier tables (the froxel grid 128 × 72 × 48 and 80 × 45 × 32 in three INT keys), with no light-cluster tier value.
- **Covers:** Z2-I-RPROD-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Test_Quality_TierIndexFromList` (default tiers) in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Quality settings"

### Z2-R-RPROD-135 Per-platform quality tiers
- **Statement:** Stage 5 ships every tier of QualityMatrix.md per platform with its engine tier files, and budgeted per-tier values equal their rows.
- **Covers:** Z2-I-RPROD-20
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-RPROD-022, Z2-B-RPROD-023, Z2-B-RPROD-013
- **Acceptance evidence:** `Test_Quality_TierFilesMatchBudgetRows` in the `Test` gate step (win-debug, win-release); the Stage 5 QualityMatrix goldens
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Quality settings"

### Z2-R-RPROD-136 Tier changes at the next frame
- **Statement:** A tier or quality-key change applies at the next build, invalidates only the affected histories, keeps previous values on out-of-range input and never touches simulation.
- **Covers:** Z2-I-RPROD-20
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_Quality_ChangeAppliesNextFrame`, `Test_Quality_OutOfRangeKept` and `Test_Quality_NoSimulationKey` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Quality tier selection"

### Z2-R-RPROD-137 Per-view overrides
- **Statement:** Texture-view overrides (feature clear mask, shadow-quality delta) and memory-pressure actions 9 and 10 reach each view's quality settings, and action 11 reaches LIGHT's dynamic-resolution controller of exactly every view for which `HasDynamicResolution` is true, set before `UpdateDynamicResolution` and cleared when the level falls.
- **Covers:** Z2-I-RPROD-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_Quality_OverridesApplied` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Quality tier selection"

### Z2-R-RPROD-138 Renderer creation and unwinding
- **Statement:** `Flux_Renderer::Create` runs its steps in order, LIGHT's lighting system before the view registry that borrows it, and a failure at any step unwinds every earlier step.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_FluxRenderer_Create_FailsAt<Step>_Unwinds` for each step in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Create sequence"

### Z2-R-RPROD-139 Renderer configuration keys
- **Statement:** `Zenith_RegisterFluxConfigKeys` registers every RPROD key of the key table with its type, range, layers and restart flag.
- **Covers:** Z2-I-RPROD-12, Z2-I-RPROD-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_FluxRenderer_ConfigKeysRegistered` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Configuration keys"

### Z2-R-RPROD-140 Render world consumption
- **Statement:** `BuildGraph` consumes the sealed render world in world-tag and merge order through the consumer table and writes nothing into it, and a world with `m_bFullState` is consumed first, the bucket reference counts and draw keys being rebuilt from its re-created material slots.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-003
- **Acceptance evidence:** `Test_FluxRenderer_ConsumesWorldInOrder` and `Test_FluxRenderer_FullStateRebuildsDrawKeys` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Render world consumption"

### Z2-R-RPROD-141 Render frame sequence
- **Statement:** The render subgraph runs BuildGraph, parallel Record, Submit and Present with the owners of the sequence tables, and a Shipping compile failure skips the frame and keeps presenting.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-055
- **Budgets:** Z2-B-RHI-004, Z2-B-RHI-005
- **Acceptance evidence:** `Test_FluxRenderer_CompileFailureSkipsFrameInShipping` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Render frame sequence"

### Z2-R-RPROD-142 One frame of CPU render lag
- **Statement:** Rendering of frame N runs concurrently with simulation of N+1 and never further behind.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-021
- **Acceptance evidence:** GRAPH's `Test_FrameGraph_SimulationNeverTwoFramesAhead` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD.md` → "Frame-graph nodes and declared access"

### Z2-R-RPROD-143 Counters and profiler rows
- **Statement:** RPROD publishes every counter of the counter table and supplies every profiler row of the rows table.
- **Covers:** Z2-I-RPROD-19, Z2-I-RPROD-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_FluxRenderer_CountersPublished` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Counters"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "On-screen profiler rows"

### Z2-R-RPROD-144 Console commands
- **Statement:** The development console commands and debug-UI toggles of the table change their settings at the next frame and do nothing in Shipping.
- **Covers:** Z2-I-RPROD-12
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_FluxRenderer_ConsoleTogglesNextFrame` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Console commands and debug UI"

### Z2-R-RPROD-145 Backend parity suite
- **Statement:** From Stage 3 every parity scene compares Vulkan and D3D12 surface channels near-exactly and final images within the parity row, recording each view's depth format.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** Z2-B-RPROD-031
- **Acceptance evidence:** `Test_BackendParity_<Scene>` in the `BackendParity` gate step (win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity scenes"

### Z2-R-RPROD-146 Path parity suite
- **Statement:** From Stage 5 every path is compared with the Forward reference on the parity scenes with exactly the capability matrix's exclusions and tolerances.
- **Covers:** Z2-I-RPROD-22, Z2-I-RPROD-23, Z2-I-RPROD-24
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063, Z2-D-035
- **Budgets:** Z2-B-RPROD-031
- **Acceptance evidence:** `Test_PathParity_<Scene>_<PathA>_<PathB>` in the `PathParity` gate step (win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites"

### Z2-R-RPROD-147 Parity reproducibility and artefacts
- **Statement:** Parity runs fix camera, jitter, seeds, exposure and frames, and a failure retains both images, the FLIP map, difference images and both dumps.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_Parity_ArtefactsRetained` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites"

### Z2-R-RPROD-148 Parity scenes cover shared passes
- **Statement:** The parity scenes exercise every pass that appears in more than one path.
- **Covers:** Z2-I-RPROD-21, Z2-I-RPROD-22, Z2-I-RPROD-23, Z2-I-RPROD-24
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** none
- **Acceptance evidence:** `Test_Parity_ScenesCoverSharedPasses` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity scenes"

### Z2-R-RPROD-149 Shading-path research harness
- **Statement:** `Zenith_Bench_ShadingPaths` measures each path's per-pass GPU time, memory, bandwidth, image differences and exclusions under RESEARCH's procedure, with a CTest smoke run.
- **Covers:** Z2-I-RPROD-22, Z2-I-RPROD-23, Z2-I-RPROD-24
- **Stage:** S5
- **Classification:** research
- **Decisions:** Z2-D-064, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** the harness smoke run in the `Test` gate step (win-debug, win-release); the Stage 5 research report under `Zenith2/Docs/Research/ShadingPaths/` (needs-human)
- **Design:** `Zenith2/Docs/Design/RPROD/Tests.md` → "Benchmarks and harnesses"

### Z2-R-RPROD-150 D-TBDR harness
- **Statement:** `Zenith_Bench_TBDR` measures on the S24 the bandwidth and GPU time of the Forward+ frame for the measured configurations of DecisionLog alternatives (1), (2) and (3) under the research procedure, with a CTest smoke run.
- **Covers:** Z2-I-RPROD-25
- **Stage:** S5
- **Classification:** research
- **Decisions:** Z2-D-036, Z2-D-084, D-TBDR (open, Stage 5)
- **Budgets:** Z2-B-RPROD-026
- **Acceptance evidence:** the harness smoke run in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Tests.md` → "Benchmarks and harnesses"; `Zenith2/Docs/Design/RPROD.md` → "Selection decisions RPROD owns"

### Z2-R-RPROD-151 D-TBDR ruling
- **Statement:** The D-TBDR ruling measures the configurations on the S24, commits the report and appends the ruling to the decision log per the DecisionLog acceptance rule (sharpened by X102 to at least 5% with non-overlapping 95% confidence intervals and bit-identical parity once X102 is applied), and no tile-local merging lands before it.
- **Covers:** Z2-I-RPROD-25
- **Stage:** S5
- **Classification:** research
- **Decisions:** Z2-D-036, Z2-D-097, D-TBDR (open, Stage 5), X102 (applied, core-doc batch 4)
- **Budgets:** Z2-B-RPROD-026
- **Acceptance evidence:** the report under `Zenith2/Docs/Research/TBDR/` and the DecisionLog entry (needs-human ruling ticket)
- **Design:** `Zenith2/Docs/Design/RPROD.md` → "Selection decisions RPROD owns"

### Z2-R-RPROD-152 RPROD budget harness
- **Statement:** `Zenith_Bench_Budgets_RPROD` measures every RPROD row's quantity on the RTX 3070 and the S24 with 10 repetitions and a CTest smoke run, including the four engine extractor zones per world, the transparent entry bytes per view of W-Indoor and W-Outdoor that the upload-ring cap's budget ticket reads, and the full-state render-world bytes of both workloads (instance deltas, light deltas, view requests) that the `Z2-B-CORE-005` budget ticket reads, a full-state world being the sizing case of the render-world slot arenas.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-RPROD-004, Z2-B-RPROD-020, Z2-B-RPROD-028
- **Acceptance evidence:** the harness smoke run in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD.md` → "Measurement harness"

### Z2-R-RPROD-153 RPROD budget rows allocated
- **Statement:** The Stage 1 per-system budget ticket for RPROD allocates every pending RPROD row, including the GPU headroom row before every other area's GPU rows, with MP-ALLOC passing.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-097
- **Budgets:** Z2-B-RPROD-013, Z2-B-RPROD-028, Z2-B-RPROD-029, Z2-B-RPROD-030, Z2-B-RPROD-031
- **Acceptance evidence:** the allocation diff and DecisionLog entry of the needs-human budget ticket
- **Design:** `Zenith2/Docs/Design/RPROD.md` → "Rows RPROD owns"

### Z2-R-RPROD-154 Rendering workload games
- **Statement:** W-Indoor and W-Outdoor exist as workload games with their scripted paths after their Stage 1 scale tickets, so every RPROD row can be measured.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-RPROD-005, Z2-B-RPROD-009
- **Acceptance evidence:** the Stage 3 workload evidence records
- **Design:** `Zenith2/Docs/Design/RPROD.md` → "Stage 3"

### Z2-R-RPROD-155 On-disk pipeline cache bound
- **Statement:** The persisted pipeline-cache files of one game per driver stay within their row after a warm run of every workload.
- **Covers:** Z2-I-RPROD-16
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** Z2-B-RPROD-030, Z2-B-RPROD-002
- **Acceptance evidence:** MP-DISKCACHE in the Stage 3 workload records
- **Design:** `Zenith2/Docs/Design/RPROD.md` → "Rows RPROD owns"

### Z2-R-RPROD-156 GPU scene memory measured
- **Statement:** The GPU scene tables stay within their memory row, measured through the GPU scene's table bytes.
- **Covers:** Z2-I-RPROD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-RPROD-001, Z2-B-RPROD-004
- **Acceptance evidence:** MP-SUBMEM in the Stage 3 workload records
- **Design:** `Zenith2/Docs/Design/RPROD.md` → "Rows RPROD is measured against"

### Z2-R-RPROD-157 Widened first write adds no ordering
- **Statement:** The first writes the front end declares to ZenithGraph per subresource (`GRAPH_RESOURCE_FLAG_FIRST_WRITE_DISCARD_PER_SUBRESOURCE`, each with its real mode over its real range) add no ordering to any frame the frame anatomy declares: over the live pass set after sub-steps 1a to 1e, with texture ranges per subresource and buffer ranges at 256-byte granules, the compiled edges' transitive closure equals the data-flow closure, no compiled edge crosses queues after step 6 without a data-flow path, and six passes each first-writing one face of one transient cube, read whole by a side-effect pass, compile with no edge between any two face writers. The title keeps its wording for its citations; no first write is widened.
- **Covers:** Z2-I-RPROD-10, Z2-I-RPROD-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_FirstWriteAddsNoEdge` (its exact derivation in `Zenith2/Docs/Design/RPROD/Tests.md` → "Widened first write test") and `Test_RenderGraph_IndependentFacesNoEdge` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "First-access widening"; `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Consequence of the widening"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Widened first write test"

### Z2-R-RPROD-158 Render environment single writer
- **Statement:** `Flux_RenderEnvironment` carries LIGHT's lighting environment, explicit sun and moon slots and ENV's blocks, and `Render.ExtractEnvironment` is its one writer, calling `SetEnvironment` once per world per frame, copying ENV's `PHASE_INPUT` world resource `Zenith.Environment.RenderBlocks` (ordered after its phase-1 writer by the phase boundary, with no ordering of its own) and LIGHT's world-lighting component, and writing defaults when no Environment module runs.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050, X125 (ruled; pending core-doc batch 4)
- **Budgets:** Z2-B-RPROD-003
- **Acceptance evidence:** `Test_EngineExtractor_EnvironmentSetOnce` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Render environment"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Environment extractor"

### Z2-R-RPROD-159 Per-instance cull distance
- **Statement:** An instance of a primitive flagged `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` farther than the metres in its custom data bits 16–31 from the LOD reference view is culled in every view, including shadow views, and counted as frustum-culled.
- **Covers:** Z2-I-RPROD-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058, X127 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_Culling_CustomCullDistance` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Per-instance cull distance"

### Z2-R-RPROD-160 Culling pass tables
- **Statement:** `CullClear`, `CullSort`, `BucketScatter` and `HiZDebugOverlay` each have a per-pass table with program, fields, queue, capability, budget row and permutations, added to Rendering's per-pass file by the ticket that lands the pass.
- **Covers:** Z2-I-RPROD-07, Z2-I-RPROD-05
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-055, Z2-D-060, X135 (applied, core-doc batch 4)
- **Budgets:** Z2-B-RPROD-004, Z2-B-DEVT-003
- **Acceptance evidence:** the per-pass file diff of each landing ticket; `Test_Culling_BucketScatterOrderedBeforeCull` and `Test_HiZ_OverlayAddsNoPassWhenOff` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Tables of the passes this file adds"; `Zenith2/Docs/Design/RPROD/Culling.md` → "CullClear"; `Zenith2/Docs/Design/RPROD/Culling.md` → "CullSort"; `Zenith2/Docs/Design/RPROD/Culling.md` → "BucketScatter"; `Zenith2/Docs/Design/RPROD/Culling.md` → "HiZDebugOverlay"

### Z2-R-RPROD-161 Build-node upload data declared first
- **Statement:** Every culling producer of per-frame upload data (transparent entries, frustum planes, bucket-scatter records and bases) declares its bytes at graph setup under its literal owner string (`Flux.Transparent`, `Flux.FrustumPlanes`, `Flux.BucketScatter`, `Flux.BucketCommandBase`) and writes them only after the frame's upload reservation succeeded, so a deferred frame writes none of them and advances no culling history; the transparent bytes per view of W-Indoor and W-Outdoor are measured by the RPROD harness as an input of the `Z2-B-RHI-006` budget ticket.
- **Covers:** Z2-I-RPROD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-RHI-006, Z2-B-RHI-007
- **Acceptance evidence:** `Test_Culling_BuildNodeDataDeclaredBeforeExtension` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Build-node upload data"; `Zenith2/Docs/Design/RPROD/Culling.md` → "Transparent collection and emission"; `Zenith2/Docs/Design/RPROD.md` → "Measurement harness"

### Z2-R-RPROD-162 Unpaced present modes
- **Statement:** With `r.vsync = 0` the pacer waits on the CPU until `D(f)` minus the present margin for every swap interval n ≥ 1, presents with swap interval 0, and returns to `WARMUP` whenever `r.vsync` changes.
- **Covers:** Z2-I-RPROD-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RPROD-020
- **Acceptance evidence:** `Test_FramePacer_NoPacingPresentModeWaits` and `Test_FramePacer_StateMachine` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Frame pacer"

### Z2-R-RPROD-163 Environment and SSGI frame slots
- **Statement:** The path functions call ENV's cloud layer before the sky, ENV's underwater pass after particles, ENV's Stage 5 prologue passes after procedural geometry and, on desktop tiers, LIGHT's SSGI after contact shadows, each adding no pass when its feature is absent.
- **Covers:** Z2-I-RPROD-22, Z2-I-RPROD-23, Z2-I-RPROD-24
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035, X127 (applied, core-doc batch 4), X105 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_Paths_DeclarationOrderMatchesFrameTables` over the frame tables as X127 and X105 amend them in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "Environment and lighting passes in the path functions"

### Z2-R-RPROD-164 LIGHT additions to RPROD passes
- **Statement:** RPROD's forward opaque and transparent passes declare the shadow atlas and shadow views `[Optional]`, the forward opaque passes attach the specular-weight target only when SSR is enabled, and from Stage 4 bind the image-based-lighting cube array, irradiance buffer and probe records.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035, X103 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_Render_ForwardPlusFrameFixture` with and without a shadowed light and with SSR on and off in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Passes.md` → "LIGHT additions to RPROD's pass tables"

### Z2-R-RPROD-165 Render instance key and delta
- **Statement:** `Flux_RenderWorld.h` declares `Flux_RenderInstanceKey` (entity keys with bit 63 clear from `Flux_MakeEntityInstanceKey`, module keys with bit 63 set), `Flux_InstanceFlags` with the GPU-scene record's bit positions and `Flux_RenderInstanceDelta` of exactly 184 bytes (key, op, producer flags, mesh ID, custom data, layer mask, interpolated matrix, world bounds, eight material overrides), and the writer clears every flag bit a producer may not set.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_InstanceDelta_Layout` in the `Test` gate step (win-debug, win-release); `Test_EngineExtractor_KeysDisjointAndStable` in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance keys"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance flags"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Flux_RenderInstanceDelta layout"

### Z2-R-RPROD-166 Engine render component types
- **Statement:** The engine's renderable-mesh component `Zenith.Render.MeshInstance` and camera component `Zenith.Render.Camera` are C++-registered structs declared in `Flux_RenderComponents.h` and registered by ZenithRuntime at startup step 6 with the stable names, property IDs, flags, defaults and asset lifecycle hooks of the registration table.
- **Covers:** Z2-I-RPROD-21, Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059, X108 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderComponents_Registered` in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Flux_RenderComponents header"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Render component registration"

### Z2-R-RPROD-167 Engine extractors registered without ordering
- **Statement:** ZenithRuntime's attach step registers `Render.ExtractInstances`, `Render.ExtractLights`, `Render.ExtractCameras` and `Render.ExtractEnvironment` for every world in that order and in every configuration, headless included, through `Zenith_WorldAttachContext::AddExtractor`, each declaring reads only and no ordering, so that `EmitInto`'s generated `Before("Render.Seal@W0", true)` orders each before the seal, which is registered in every configuration.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050, RP32 (pending MOD)
- **Budgets:** Z2-B-RPROD-003
- **Acceptance evidence:** `Test_EngineExtractor_OrderedBeforeSeal` (the headless composition) in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Extractor registration"

### Z2-R-RPROD-168 Instance deltas exact
- **Statement:** `Render.ExtractInstances` writes one ADD per entity entering the renderable query, one UPDATE per tracked entity whose declared properties were committed, in a tick or out of tick, after the extractor's previous run (a row version at or newer than the tracker's reference counting as changed, and a full walk whenever `GetPreviousReadVersion()` is 0), one REMOVE per entity that was destroyed, lost a tracked component or stopped being drawn, and nothing for an unchanged entity, holding an entity whose assets are not resolvable with its entity handle and dropping a held entity that no longer matches the query.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-058, RP31 (pending ECS)
- **Budgets:** Z2-B-RPROD-003
- **Acceptance evidence:** `Test_EngineExtractor_InstanceDeltasAddUpdateRemove` (including the out-of-tick edit on a paused world, the out-of-tick load and the restore to the current tick index) in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Delta tracking"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance extractor"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Engine extraction state"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Extraction resolver"

### Z2-R-RPROD-169 Interpolated and settled instance transforms
- **Statement:** Each extracted instance transform is `Zenith_InterpolateWorldTransform` of the previous and current world transforms at the frame's interpolation factor, a moving instance (its emitted transform hash differing from the stored one) is re-emitted in frames with no tick, and one settle UPDATE follows its last change, from the merge step or the moving-entries step, clearing MOVING so the GPU scene's previous transform catches up.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineExtractor_InterpolatedTransform` and `Test_EngineExtractor_InstanceDeltasAddUpdateRemove` (the settle case) in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Delta tracking"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance extractor"

### Z2-R-RPROD-170 Teleport history resets
- **Statement:** Every renderable entity teleported in any tick of a frame, a teleported ancestor's descendants included, receives one history reset and an UPDATE with its current transform in that frame and none in the next.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineExtractor_TeleportEmitsHistoryReset` in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance extractor"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Engine extraction state"

### Z2-R-RPROD-171 Light extraction
- **Statement:** `Render.ExtractLights` writes LIGHT's light deltas from the point, spot and directional light component types, one light per entity by the point, spot, directional precedence, sweeping all three trackers in any run in which one of them met a new row or a count mismatch so a precedence change is exact, with LIGHT's keys, degrees converted to radians, lumens to the stored unit (the unit selector is property 4), Kelvin or the normalised colour to the tint, the direction from the entity's world −Z, and every delta validated by `Flux_ValidateLightDelta` before it is written. The rect and disc lights are Z2-R-RPROD-185 (Stage 5).
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035, Z2-D-058, RP35 (pending LIGHT)
- **Budgets:** Z2-B-RPROD-003
- **Acceptance evidence:** `Test_EngineExtractor_LightDeltasMatchSchemas` (including the same-tick precedence swap of two entities) in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Light extractor"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Light conversions"

### Z2-R-RPROD-172 Camera view requests
- **Statement:** `Render.ExtractCameras` writes one main-view request per world for the camera `Flux_SelectMainCamera` selects (active, main kind, the highest priority, ties to the lowest simulation ID, independent of row order), the one function ENV's submersion test calls too, filling every `Flux_ViewRequest` member as the fill table gives, with the projections built by the view list from the request's lens and the view's depth mode, and writes no request for a camera with an invalid lens.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059, Z2-D-061, RP34 (pending ENV)
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineExtractor_CameraCutOnTeleport` (the lens cases) in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`; `Test_View_InvalidRequestRejected` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Camera extractor"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "View request fill"; `Zenith2/Docs/Design/RPROD/Views.md` → "View list construction"

### Z2-R-RPROD-173 Camera cuts
- **Statement:** A main view's request carries `m_bCameraCut` in exactly the frames in which its camera entity or an ancestor was teleported, its `CutRequest` changed or the main camera changed, the extractor clears its camera teleport set at the end of every run whether or not a main camera exists, and the view list copies the flag into the view's camera.
- **Covers:** Z2-I-RPROD-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineExtractor_CameraCutOnTeleport` in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Camera cuts"; `Zenith2/Docs/Design/RPROD/Views.md` → "Flux_View header"

### Z2-R-RPROD-174 Environment read from ENV's blocks resource
- **Statement:** `Render.ExtractEnvironment` declares a read of `Zenith.Environment.RenderBlocks` only when ENV registered the type, copies the resource whole into the blocks and its atmosphere, fog, stars, cloud shadow and slots into LIGHT's block and the sun and moon slots, points the local-fog-volume span into the section, leaves the per-view medium overrides to ENV's per-view records, and copies the last tick's values in a frame with no tick.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-050, X125 (ruled; pending core-doc batch 4)
- **Budgets:** Z2-B-RPROD-003
- **Acceptance evidence:** `Test_EngineExtractor_EnvironmentSetOnce` in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Environment extractor"; `Zenith2/Docs/Design/RPROD/Renderer.md` → "Render environment"

### Z2-R-RPROD-175 Full-state extraction
- **Statement:** When the writer requests a full extraction, the instance and light extractors clear their tracking and write an ADD for every live instance and light with no REMOVE and no history reset, the camera and environment extractors write as every frame, and the next frame returns to deltas.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineExtractor_FullStateWritesEveryLiveInstance` in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Full-state extraction"

### Z2-R-RPROD-176 Extraction determinism
- **Statement:** Each engine extractor's partition orders REMOVEs by key, then ADDs and UPDATEs by simulation ID, then history resets by key, each list rotated to a start that is a function of the previous frames' content, so the sealed sections are byte-identical at any worker count and independent of chunk order whenever the same assets resolve to the same IDs at the same frames.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineExtractor_DeterministicAcrossWorkerCounts` (with the fixture resolver, every asset resolved from frame 0) in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Determinism of extraction"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Delta tracking"

### Z2-R-RPROD-177 Render-world slot-full retry
- **Statement:** A record the writer refuses with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` is counted and never asserted on; after the first refusal of a run the extractor calls the writer no more in that run and keeps every remaining ADD, UPDATE, REMOVE and history reset for the next frame, and each write list starts at its last refused element so every record of a list of N is written within ⌈N / c⌉ frames under persistent overflow, so the delta stream's replay equals a run without refusals.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineExtractor_SlotFullDropsAndCounts` in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Slot-full handling"

### Z2-R-RPROD-178 Extraction zones and counters
- **Statement:** Each engine extractor node emits a profiler zone named after its node and the `Render.Extract.*` counters, and `Z2-B-RPROD-003` is measured as the per-frame sum of the four zones over every world.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005, Z2-D-084
- **Budgets:** Z2-B-RPROD-003
- **Acceptance evidence:** `Test_FluxRenderer_CountersPublished` in the `Test` gate step (win-debug, win-release) and the RPROD budget harness smoke run
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Extraction counters and zones"; `Zenith2/Docs/Design/RPROD/Diagnostics.md` → "Profiling zones"

### Z2-R-RPROD-179 Procedural instances consumed
- **Statement:** `BuildGraph` applies every section's procedural instance records through GEOM's `Flux_ProceduralApplyRecords` before the GPU scene applies the deltas and builds the `Flux_ProceduralFrame` before the common prologue, and `Flux_Renderer::GetProceduralGeometry` gives modules the system to register their sources before the first frame; the render-world members are Rendering's since cross-doc item X141.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, X141 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_FluxRenderer_ProceduralRecordsBeforeDeltas` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Render world consumption"; `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux.BuildGraph"; `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux_Renderer header"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Procedural instance records"

### Z2-R-RPROD-180 Null vertex stream bound
- **Statement:** A PSO whose specialised vertex table carries the null stream has binding 7 at stride 0, and the raster context binds GEOM's `Flux_NullVertexStream` there before the next draw on Vulkan and D3D12, so an attribute the program declares and the mesh lacks reads zero.
- **Covers:** Z2-I-RPROD-32
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-062
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_NullStreamBoundForAbsentAttributes` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md` → "Pass context implementation"; `Zenith2/Docs/Design/RPROD/PSO.md` → "PSO builder"

### Z2-R-RPROD-181 Stream-set compiler surface
- **Statement:** The compiler accepts the `[StreamSet(<code>)]` struct attribute, runs the `--stream-sets` aggregate command writing `Flux_StreamSets.generated.h` byte-deterministically, raises the seven `ZSC_ERROR_STREAM_SET_*` codes at the pipeline steps of the error table, emits the null stream in specialised tables and the stream-set link-time constants, and knows the procedural shared structs; the vocabulary half is Rendering's since cross-doc item X138.
- **Covers:** Z2-I-RPROD-13, Z2-I-RPROD-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-015, Z2-D-062, X138 (applied, core-doc batch 4)
- **Budgets:** none
- **Acceptance evidence:** `Test_ShaderCompiler_StreamSetTables` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler additions adopted from GEOM"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler error codes"; `Zenith2/Docs/Design/RPROD/PSO.md` → "Compiler pipeline"

### Z2-R-RPROD-182 Transparent over-cap failure attributable
- **Statement:** A frame whose upload need exceeds the ring's cap because of a view's transparent entries fails with `FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP`, draws nothing and keeps the visibility history, while RHI's log names the `Flux.Transparent` owner and RPROD publishes the view's entry count and warns once per view when the entries exceed the view's share of the cap.
- **Covers:** Z2-I-RPROD-07
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, RP27 (pending RHI)
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_Culling_TransparentOverCapFailsFrame` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Culling.md` → "Transparent collection and emission"; `Zenith2/Docs/Design/RPROD/Culling.md` → "Build-node upload data"

### Z2-R-RPROD-183 Held entities wait for asset availability
- **Statement:** The instance extractor resolves meshes and materials only through the extraction resolver, holds an entity whose asset is unresolved or has no runtime object, and retries such a hold only in a run whose availability epoch differs from the one the tracker stored, so a headless run, whose validate-only loader never publishes an object, re-resolves no held entity after its first frame, while a property change of a held entity is still rebuilt in its frame.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050, RP33 (pending ASSET), RP36 (pending GEOM)
- **Budgets:** none
- **Acceptance evidence:** `Test_EngineExtractor_HeldEntitiesWaitForAvailability` and `Test_EngineExtractor_ProductionResolverHoldsHeadless` in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Extraction resolver"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Delta tracking"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Instance extraction algorithm"

### Z2-R-RPROD-184 LIGHT frame data and frame end
- **Statement:** `Flux.BuildGraph` calls LIGHT's `WriteFrameData` only after the frame's `ExtendReservation` returned `FITS` or `EXTENDED`, never in a deferred or failed frame, and calls `EndFrame` with whether the frame submitted as the last LIGHT call on every path out of the node, the shipping compile-failure skip included.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-035, Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_FluxRenderer_LightFrameCallsOrdered` in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RPROD/Renderer.md` → "Flux.BuildGraph"

### Z2-R-RPROD-185 Rect and disc light extraction
- **Statement:** From Stage 5 `Render.ExtractLights` also writes LIGHT's light deltas from the rect and disc light component types, extending the one-light-per-entity precedence to point, spot, directional, rect, disc and the joint sweep to all five trackers, with the stored unit of both selected by property 4 (0: nits as authored; 1: lumens converted to nits by LIGHT's rect or disc conversion), the rect's extent and tangent and the disc's radius and barn cone copied as LIGHT's delta gives them, and every delta validated by `Flux_ValidateLightDelta` before it is written.
- **Covers:** Z2-I-RPROD-21
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-035, Z2-D-058, RP35 (pending LIGHT)
- **Budgets:** Z2-B-RPROD-003
- **Acceptance evidence:** `Test_EngineExtractor_RectAndDiscLights` (including a same-tick precedence swap between a rect and a disc light) in the `Test` gate step (win-debug, win-release) through `ZenithRuntime_Tests`
- **Design:** `Zenith2/Docs/Design/RPROD/Extractors.md` → "Light extractor"; `Zenith2/Docs/Design/RPROD/Extractors.md` → "Light conversions"

## Coverage of inventory items

A requirement may sit at a subsequent stage to the item it covers when it serves that stage's acceptance; the item's own stage is then served by the requirements the Note column names, which alone satisfy that stage's acceptance. A requirement at an earlier stage than its item is a prerequisite that the item's stage builds on.

| Item | Requirements | Note |
|---|---|---|
| Z2-I-RPROD-01 | Z2-R-RPROD-072, Z2-R-RPROD-073, Z2-R-RPROD-074, Z2-R-RPROD-075, Z2-R-RPROD-076, Z2-R-RPROD-086 | — |
| Z2-I-RPROD-02 | Z2-R-RPROD-077, Z2-R-RPROD-078, Z2-R-RPROD-079 | — |
| Z2-I-RPROD-03 | Z2-R-RPROD-080, Z2-R-RPROD-081 | — |
| Z2-I-RPROD-04 | Z2-R-RPROD-082 | — |
| Z2-I-RPROD-05 | Z2-R-RPROD-081, Z2-R-RPROD-083, Z2-R-RPROD-084, Z2-R-RPROD-085, Z2-R-RPROD-143, Z2-R-RPROD-160 | — |
| Z2-I-RPROD-06 | Z2-R-RPROD-087, Z2-R-RPROD-088 | — |
| Z2-I-RPROD-07 | Z2-R-RPROD-089, Z2-R-RPROD-090, Z2-R-RPROD-091, Z2-R-RPROD-092, Z2-R-RPROD-093, Z2-R-RPROD-094, Z2-R-RPROD-156, Z2-R-RPROD-159, Z2-R-RPROD-160, Z2-R-RPROD-161, Z2-R-RPROD-182 | Stage 3 served by Z2-R-RPROD-089, Z2-R-RPROD-090, Z2-R-RPROD-091, Z2-R-RPROD-092, Z2-R-RPROD-093, Z2-R-RPROD-094, Z2-R-RPROD-156, Z2-R-RPROD-160, Z2-R-RPROD-161, Z2-R-RPROD-182 |
| Z2-I-RPROD-08 | Z2-R-RPROD-043, Z2-R-RPROD-044, Z2-R-RPROD-045, Z2-R-RPROD-046, Z2-R-RPROD-047, Z2-R-RPROD-048, Z2-R-RPROD-049, Z2-R-RPROD-050, Z2-R-RPROD-051, Z2-R-RPROD-052, Z2-R-RPROD-053, Z2-R-RPROD-054, Z2-R-RPROD-055, Z2-R-RPROD-056, Z2-R-RPROD-166, Z2-R-RPROD-172, Z2-R-RPROD-173 | Stage 2 served by Z2-R-RPROD-043, Z2-R-RPROD-044, Z2-R-RPROD-045, Z2-R-RPROD-046, Z2-R-RPROD-047, Z2-R-RPROD-048, Z2-R-RPROD-049, Z2-R-RPROD-050, Z2-R-RPROD-052, Z2-R-RPROD-055, Z2-R-RPROD-166, Z2-R-RPROD-172, Z2-R-RPROD-173 |
| Z2-I-RPROD-09 | Z2-R-RPROD-057, Z2-R-RPROD-058 | Stage 5 served by Z2-R-RPROD-057 |
| Z2-I-RPROD-10 | Z2-R-RPROD-001, Z2-R-RPROD-002, Z2-R-RPROD-003, Z2-R-RPROD-004, Z2-R-RPROD-005, Z2-R-RPROD-006, Z2-R-RPROD-007, Z2-R-RPROD-008, Z2-R-RPROD-015, Z2-R-RPROD-016, Z2-R-RPROD-017, Z2-R-RPROD-018, Z2-R-RPROD-019, Z2-R-RPROD-020, Z2-R-RPROD-021, Z2-R-RPROD-022, Z2-R-RPROD-023, Z2-R-RPROD-041, Z2-R-RPROD-157 | — |
| Z2-I-RPROD-11 | Z2-R-RPROD-006, Z2-R-RPROD-018, Z2-R-RPROD-024, Z2-R-RPROD-025, Z2-R-RPROD-026, Z2-R-RPROD-028, Z2-R-RPROD-029, Z2-R-RPROD-030, Z2-R-RPROD-031, Z2-R-RPROD-035, Z2-R-RPROD-157 | — |
| Z2-I-RPROD-12 | Z2-R-RPROD-009, Z2-R-RPROD-027, Z2-R-RPROD-030, Z2-R-RPROD-031, Z2-R-RPROD-139, Z2-R-RPROD-144 | — |
| Z2-I-RPROD-13 | Z2-R-RPROD-095, Z2-R-RPROD-096, Z2-R-RPROD-097, Z2-R-RPROD-098, Z2-R-RPROD-099, Z2-R-RPROD-100, Z2-R-RPROD-101, Z2-R-RPROD-102, Z2-R-RPROD-103, Z2-R-RPROD-124, Z2-R-RPROD-181 | Stage 2 served by Z2-R-RPROD-095, Z2-R-RPROD-096, Z2-R-RPROD-097, Z2-R-RPROD-098, Z2-R-RPROD-099, Z2-R-RPROD-100, Z2-R-RPROD-101, Z2-R-RPROD-103, Z2-R-RPROD-124, Z2-R-RPROD-181 |
| Z2-I-RPROD-14 | Z2-R-RPROD-104, Z2-R-RPROD-105, Z2-R-RPROD-181 | Stage 2 served by Z2-R-RPROD-105, Z2-R-RPROD-181 |
| Z2-I-RPROD-15 | Z2-R-RPROD-106 | — |
| Z2-I-RPROD-16 | Z2-R-RPROD-107, Z2-R-RPROD-108, Z2-R-RPROD-109, Z2-R-RPROD-110, Z2-R-RPROD-111, Z2-R-RPROD-112, Z2-R-RPROD-113, Z2-R-RPROD-114, Z2-R-RPROD-115, Z2-R-RPROD-119, Z2-R-RPROD-155 | — |
| Z2-I-RPROD-17 | Z2-R-RPROD-116, Z2-R-RPROD-117, Z2-R-RPROD-118, Z2-R-RPROD-119 | — |
| Z2-I-RPROD-18 | Z2-R-RPROD-127, Z2-R-RPROD-128, Z2-R-RPROD-129, Z2-R-RPROD-130, Z2-R-RPROD-131, Z2-R-RPROD-132, Z2-R-RPROD-139, Z2-R-RPROD-162 | — |
| Z2-I-RPROD-19 | Z2-R-RPROD-032, Z2-R-RPROD-033, Z2-R-RPROD-034, Z2-R-RPROD-143 | — |
| Z2-I-RPROD-20 | Z2-R-RPROD-133, Z2-R-RPROD-134, Z2-R-RPROD-135, Z2-R-RPROD-136, Z2-R-RPROD-137 | — |
| Z2-I-RPROD-21 | Z2-R-RPROD-042, Z2-R-RPROD-059, Z2-R-RPROD-060, Z2-R-RPROD-061, Z2-R-RPROD-062, Z2-R-RPROD-063, Z2-R-RPROD-068, Z2-R-RPROD-069, Z2-R-RPROD-070, Z2-R-RPROD-071, Z2-R-RPROD-138, Z2-R-RPROD-140, Z2-R-RPROD-141, Z2-R-RPROD-142, Z2-R-RPROD-145, Z2-R-RPROD-147, Z2-R-RPROD-148, Z2-R-RPROD-152, Z2-R-RPROD-153, Z2-R-RPROD-154, Z2-R-RPROD-158, Z2-R-RPROD-164, Z2-R-RPROD-165, Z2-R-RPROD-166, Z2-R-RPROD-167, Z2-R-RPROD-168, Z2-R-RPROD-169, Z2-R-RPROD-170, Z2-R-RPROD-171, Z2-R-RPROD-174, Z2-R-RPROD-175, Z2-R-RPROD-176, Z2-R-RPROD-177, Z2-R-RPROD-178, Z2-R-RPROD-179, Z2-R-RPROD-183, Z2-R-RPROD-184, Z2-R-RPROD-185 | Stage 3 served by Z2-R-RPROD-059, Z2-R-RPROD-060, Z2-R-RPROD-061, Z2-R-RPROD-062, Z2-R-RPROD-063, Z2-R-RPROD-068, Z2-R-RPROD-069, Z2-R-RPROD-070, Z2-R-RPROD-145, Z2-R-RPROD-147, Z2-R-RPROD-154, Z2-R-RPROD-158, Z2-R-RPROD-164, Z2-R-RPROD-171, Z2-R-RPROD-174, Z2-R-RPROD-179, Z2-R-RPROD-184 |
| Z2-I-RPROD-22 | Z2-R-RPROD-060, Z2-R-RPROD-062, Z2-R-RPROD-064, Z2-R-RPROD-069, Z2-R-RPROD-146, Z2-R-RPROD-148, Z2-R-RPROD-149, Z2-R-RPROD-163 | — |
| Z2-I-RPROD-23 | Z2-R-RPROD-060, Z2-R-RPROD-062, Z2-R-RPROD-065, Z2-R-RPROD-069, Z2-R-RPROD-146, Z2-R-RPROD-148, Z2-R-RPROD-149, Z2-R-RPROD-163 | — |
| Z2-I-RPROD-24 | Z2-R-RPROD-060, Z2-R-RPROD-062, Z2-R-RPROD-066, Z2-R-RPROD-069, Z2-R-RPROD-146, Z2-R-RPROD-148, Z2-R-RPROD-149, Z2-R-RPROD-163 | — |
| Z2-I-RPROD-25 | Z2-R-RPROD-010, Z2-R-RPROD-011, Z2-R-RPROD-012, Z2-R-RPROD-013, Z2-R-RPROD-014, Z2-R-RPROD-035, Z2-R-RPROD-150, Z2-R-RPROD-151 | Stage 2 served by Z2-R-RPROD-010, Z2-R-RPROD-011, Z2-R-RPROD-012, Z2-R-RPROD-013, Z2-R-RPROD-014, Z2-R-RPROD-035 |
| Z2-I-RPROD-26 | Z2-R-RPROD-067, Z2-R-RPROD-120, Z2-R-RPROD-125 | — |
| Z2-I-RPROD-27 | Z2-R-RPROD-036, Z2-R-RPROD-048 | — |
| Z2-I-RPROD-28 | Z2-R-RPROD-037, Z2-R-RPROD-040 | — |
| Z2-I-RPROD-29 | Z2-R-RPROD-038, Z2-R-RPROD-039, Z2-R-RPROD-040, Z2-R-RPROD-121 | — |
| Z2-I-RPROD-30 | Z2-R-RPROD-122 | — |
| Z2-I-RPROD-31 | Z2-R-RPROD-123 | — |
| Z2-I-RPROD-32 | Z2-R-RPROD-104, Z2-R-RPROD-110, Z2-R-RPROD-124, Z2-R-RPROD-126, Z2-R-RPROD-180 | Stage 2 served by Z2-R-RPROD-110, Z2-R-RPROD-124, Z2-R-RPROD-126 |

## Excluded and deferred

No RPROD inventory item is excluded; every one is required at the stage above, and requirement Z2-R-RPROD-058 is deferred to Stage 7 with the stereo view kind. The capabilities below are outside RPROD's scope by decision and carry no requirement here beyond the ones named:

| Capability | Classification | Decision | Where it lives |
|---|---|---|---|
| MSAA | excluded | Z2-D-035 | none: no graph attachment or PSO key carries a sample count above 1 |
| tile-local merging and subpass reads | not implemented until D-TBDR (open, Stage 5) | Z2-D-036 | the D-TBDR harness and ruling requirements of this catalogue |
| split-screen and per-player views | excluded | Z2-D-076 | none: the view list builds one main view per window |
| stereo views | deferred to Stage 7 | Z2-D-072 | SCOPE (XR); the view signatures stay XR-ready |
| hardware ray tracing and virtualized geometry | deferred to Stage 7 | Z2-D-043 | ADVGFX's deferred tree |
| runtime shader compilation | excluded | Z2-D-025 | none: the Player links no compiler |
| lighting, shadows, post-processing and upscaling passes | required, owned elsewhere | Z2-D-035 | LIGHT; RPROD composes them into the path frames |
| the vertex format and the permutation cook step | required, owned elsewhere | Z2-D-062 | GEOM (Z2-I-GEOM-01 onwards) |
