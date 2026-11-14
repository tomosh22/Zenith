# ADVGFX design: test strategy and catalogue

Section file of `Zenith2/Docs/Design/ADVGFX.md`. It lists every named ADVGFX test with the case it asserts, its file, its target and the gate step that runs it; the fixtures; the benchmarks and harnesses; and the device runs. Test cases are doctest cases registered as their own CTests ([Z2-N-ARCH-179]), named `"<Library>/<Type>/<Behaviour>"` with the ticket ID `Test_<Type>_<Behaviour>` (`Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`); every name here is unique programme-wide (ruling TEST-NAME-UNIQUE), prefixed with ADVGFX's types. GPU tests run on WARP and lavapipe with synchronization validation and GPU-based validation clean ([Z2-N-REND-034], [Z2-N-REND-382]); they are not `needs-gpu` (ruling X170). No test double replaces a concrete engine class (ruling FINAL-DOUBLES): VT tests drive the real `Flux_VirtualTextureSystem` inside a real `Flux_Renderer` on the headless WARP and lavapipe devices.

## Test targets and gate steps

| Target | Path | Contents | Gate step |
|---|---|---|---|
| `ZenithFlux_Tests` | `Zenith2/Engine/ZenithFlux/Tests/VirtualTexture/` | VT runtime unit and render tests | `Test` |
| `ZenithFlux_Tests` with `TEST_DEPENDS ZenithPathTracer` | `Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/` and its `Support/` test-support sources | the metric, runner, reference freshness and artefact tests; LIGHT's and ENV's comparison tests call the runner from here and from their own directories under `Zenith2/Engine/ZenithFlux/Tests/` | `Test` (gate mode), Stage 5 acceptance (acceptance mode) |
| `ZenithPathTracer_Tests` | `Zenith2/Tools/ZenithPathTracer/Tests/` | tracer unit, self-check and reference-format tests | `Test` |
| `ZenithCook_Tests` | `Zenith2/Tools/ZenithCook/Tests/VirtualTexture/` | the VT cook, and the setup case of the VT fixture set ("Fixtures") | `Test` |
| `ZenithShaderCompiler_Tests` | `Zenith2/Tools/ZenithShaderCompiler/Tests/` | the reservoir visibility case and its fixtures | `Test` |
| fuzz | `Zenith2/Engine/ZenithFlux/Tests/Fuzz/VirtualTextureFile.cpp` (`Zenith_Fuzz_VirtualTextureFile`), `Zenith2/Tools/ZenithPathTracer/Tests/Fuzz/PathTraceReference.cpp` (`Zenith_Fuzz_PathTraceReference`) | the two readers | `Sanitizers -Preset win-asan` (libFuzzer smoke); stage acceptance (long runs) |
| parity | `Zenith2/Engine/ZenithFlux/Tests/Parity/Scenes/VirtualTexture.scene.json` (RPROD's layout, row AV1 (g)) | `Test_AliasingParity_VirtualTexture`, `Test_BackendParity_VirtualTexture`, `Test_PathParity_VirtualTexture_<PathA>_<PathB>` in RPROD's suites, VT in `full` mode | `Test`, `BackendParity` (Stage 3 step, Stage 4 scene), `PathParity` (Stage 5) |
| shader lints | `VirtualTexture.slang`, the VT passes | `ShaderChecks` (`NoMin16Float`, `HalfRatioReport`) | `ShaderChecks` |

Every case of a target that runs in the `Test` step finishes inside the CTest timeout of 60 seconds ([Z2-N-CONTRACT-409]), its fixture setup included; no case of this catalogue carries the `z2-stress` suite, and the one large VT, 16,384 × 16,384, is streamed only in `Zenith_Bench_VirtualTexture` ("Benchmarks and harnesses"); the 16,384 × 2,048 success case of `Test_CookVirtualTexture_ShapeRulesEnforced` runs in the `Test` step through the synthetic block-grid path, which encodes no texel.

## Virtual texture tests

File `Zenith2/Engine/ZenithFlux/Tests/VirtualTexture/Flux_VirtualTexture_Tests.cpp` unless stated.

| Test | Asserts |
|---|---|
| `Test_VirtualTexture_EntryEncodingRoundTrip` | `Flux_EncodeVTEntry` and its decoders, CPU and Slang (through a compute harness), round-trip every field at 0 and at its limit (x 29, y 29, layer 63, mip 15) and set bit 31; bits 20–30 stay zero |
| `Test_VirtualTexture_RequestEncodingRoundTrip` | the request codecs round-trip slot 4,095, mip 8, x 255, y 255 and their zeros on both sides; no valid request equals `0xFFFFFFFF` |
| `Test_VirtualTexture_MaterialHandleEncoding` | `Flux_MakeVTMaterialHandle` puts the tail texture's SRV index in word 0 and the slot (bits 0–11) and tag `0xA` (bits 28–31) in word 1, bits 12–27 zero; `Flux_IsVTMaterialHandle` is false for every ordinary handle (word 1 zero) and true for every VT handle; a tag-blind sample of a VT handle (word 0 as an ordinary texture) returns the tail's texels |
| `Test_VirtualTexture_RegionAllocatorBuddy` | on a 2,048 pool: allocate sides 128, 32, 32, 128 in that order and get the lowest-address aligned blocks; freeing all merges back to one free 2,048 block; the same sequence twice gives the same origins |
| `Test_VirtualTexture_RegionAllocatorFullRefuses` | on a 1,024 pool with 64 regions of side 128 allocated, a 65th returns `FLUX_VT_ERROR_PAGE_TABLE_FULL` and changes nothing |
| `Test_VirtualTexture_MirrorMapUnmap` | on a 4,096 VT (M = 6): map a mip-3 page, then a mip-1 descendant, then unmap the mip-3 page; every mirror entry of the subtree equals the analytic expectation after each step (descendant entries keep the mip-1 page; the rest fall to the root) |
| `Test_VirtualTexture_MirrorMatchesGPU` | a scripted sequence of 200 maps and 120 evictions on a fixture VT; after each submitted frame every used entry of every row not marked dirty in the GPU page table read back equals the mirror bit for bit, and every used entry does once no row is dirty |
| `Test_VirtualTexture_FeedbackSortUniqueDeterministic` | 4 feedback buffers of 57,600 requests with duplicates: the (request, count) list is sorted and exact, and identical with 1, 2 and 8 workers and with the buffers presented in either order within a frame |
| `Test_VirtualTexture_FeedbackValidationRejects` | requests naming a free slot, a quarantined slot, a mip ≥ M and coordinates outside the mip are dropped and counted in `Flux.VT.FeedbackRejected`; none becomes a load |
| `Test_VirtualTexture_TouchWalksAncestors` | a request for a non-resident mip-0 page touches its resident mip-2 ancestor and the root, and no other page |
| `Test_VirtualTexture_RefinementStep` | with only the root (mip 5) resident, a mip-0 request loads the mip-3 page; with mip 3 resident, the mip-1 page; with mip 1 resident, the mip-0 page |
| `Test_VirtualTexture_PriorityOrderDeterministic` | 1,000 candidates with ties at every key part are issued in the documented key order; shuffling the feedback input gives the same issued list |
| `Test_VirtualTexture_EvictionLRUAndGrace` | with the cache full, the victim is the least recently touched page outside the 8-frame grace window, ties finer mip first then (slot, mip, y, x); with every page inside the window nothing is evicted and `Flux.VT.EvictionStarved` counts the waiting tile |
| `Test_VirtualTexture_EvictedPageNotSampledDuringRefill` | the cache filled with page-ID tiles; an eviction and a refill of one page across a deferred frame: every sampled texel of every frame decodes to the page ID its entry names |
| `Test_VirtualTexture_PageTableRecordsRunLength` | a map whose subtree rows hold two finer resident pages encodes each dirty row into the minimal runs, and the update pass writes exactly those entries |
| `Test_VirtualTexture_PageTableRecordCapDefers` | a row is cut whole or not at all, and the cut stops at the first row that does not fit; an unloaded VT's dirty rows are discarded with their epochs, so the applied epoch advances past them; 70,000 dirty runs in one frame: 65,536 are written, rows carrying an epoch first in ascending epoch order, the rest in the next submitted frame; at the boundary between them every row not marked dirty equals the mirror and the deferred rows do not, and after the next one every row does; a page whose unmap rows were deferred stays `DRAINING` until its epoch is applied |
| `Test_VirtualTexture_SplitUnmapRowsDrainByEpoch` | "Draining rule" (VirtualTexturing.md): pages whose unmap rows the cap splits over three frames, with a deferred frame between, each leave `DRAINING` at `EndFrame(true)` of the frame that applies their last row and not before; the published applied epoch never decreases |
| `Test_VirtualTexture_UnloadDrainsAtFirstSubmittedFrame` | a resident VT unloaded before a deferred frame: its pages stay `DRAINING` through the deferred frame and become `FREE` at `EndFrame(true)` of the first submitted frame after the step-3c drain that processed the unload |
| `Test_VirtualTexture_SampleMatchesCPUReference` | `Flux_SampleVirtual` on a compute harness against a CPU reference of the algorithm of "Flux_SampleVirtual algorithm" (VirtualTexturing.md) at 16,384 (u, gradient) pairs, with full residency and with only ancestors resident: equal within 1/255 per channel |
| `Test_VirtualTexture_FootprintStaysInsidePage` | the cache filled with a page-ID pattern; 65,536 random (u, gradient) pairs never return a texel of another page |
| `Test_VirtualTexture_FilteringMatchesStreamedTexture` | the tolerance of "Filtering tolerance against streamed textures" (VirtualTexturing.md): 2/255 isotropic, 6/255 anisotropic, against GEOM's streamed texture of the same source |
| `Test_VirtualTexture_WrapAndClampAddressing` | a wrap VT sampled at u = −0.25, 1.25 and across the seam equals the wrapped streamed texture; a clamp VT equals the clamped one |
| `Test_VirtualTexture_TailSampling` | λ ≥ M samples the tail; λ ∈ (M − 1, M) blends the root page and tail mip 0 by frac(λ); the transition is continuous within 1/255 |
| `Test_VirtualTexture_FeedbackWrittenOncePerBlock` | a full-screen VT plane at 2,560 × 1,440, s = 8: each frame exactly one pixel per block writes, at the Bayer position of the frame index; over 64 frames every position of a block writes once |
| `Test_VirtualTexture_FeedbackReservoirUniform` | a material sampling four VTs: over 64,000 pixel-frames each VT is chosen 25% ± 1% |
| `Test_VirtualTexture_EveryMaterialPassBindsPools` | a compiled Forward+ frame (Stage 4) and a Deferred and a visibility-buffer frame (Stage 5) with VT enabled: every pass whose programs evaluate materials of the `opaque` or `alphaTest` domains reads `m_xVTPhysical` and `m_xVTPageTable` and sets the pools-bound flag, the transparent and decal passes read neither, and exactly the four writer passes write the view's feedback buffer |
| `Test_VirtualTexture_UnboundPoolsFallToTail` | a VT handle bound to a material in a pass that binds no pool (a transparent-pass harness with the handle written by a runtime override): `Flux_SampleMaterial` takes the ordinary path and returns the tail texture's texels, never the null-substitute pools' |
| `Test_VirtualTextureShader_ReservoirNotNameableFromModules` | in `ZenithShaderCompiler_Tests` (the pattern of GEOM's `Test_ShaderChecks_MaterialImplicitDerivative_Fails`), over two fixtures outside `Shaders/Tests/`: `VTReservoirVisibility_CompileFail.slang.in`, a material module that imports `VirtualTexture` and names `s_uChosen`, must fail with Slang's visibility diagnostic for an inaccessible declaration, and the case fails if the diagnostic is any other (an undefined identifier included); the positive control `VTReservoirVisibility_Control.slang.in`, the same line in a file declared `implementing VirtualTexture;`, must compile. Both run with the pinned Slang version, so the case also proves that version accepts `module` and `internal` |
| `Test_VirtualTexture_NonWritingInvocationRecordsNothing` | in a compute harness, an invocation whose pixel is not its block's writer this frame, or whose binding has bit 0 clear, leaves the reservoir count at 0 after any number of `Flux_VTRecordRequest` calls, and `Flux_VTWriteFeedback` stores nothing |
| `Test_VirtualTexture_ActivationReachesResident` | a fixture VT loaded through `Zenith_AssetKindLoaderHarness` (ruling ASSET-F25) on a real registry: ACTIVE, then RESIDENT after its tail, record and root uploads complete and the initial fill executed; `GetMaterialHandle` is `{0, 0}` before and the VT handle after |
| `Test_VirtualTexture_ActivationFailuresFallBack` | one case per failure: slots full, page table full, pool full, an upload `FAILED`, a corrupt root tile, `render.vt.enabled` false; each ends with residency FAILED (or `Fail` before activation), everything released, and the material binding GEOM's usage fallback |
| `Test_VirtualTexture_LoadUnload1000Cycles` | "Unload and slot quarantine" (VirtualTexturing.md): 1,000 randomised load and unload cycles, with `Unload` called from a second thread while the build node runs, leave nothing behind; its sanitizer run is the `win-asan` preset |
| `Test_VirtualTexture_UnloadAfterDeviceLossReleasesNothing` | an object whose `Unload` carries a device generation older than the last recovery is acknowledged by the drain and releases no slot, region or page |
| `Test_VirtualTexture_SlotQuarantine` | a slot freed at frame f is not reassigned before f + 9; a request read back from frame f − 1 naming it is rejected |
| `Test_VirtualTexture_StreamToResidentScene` | a plane carrying the synthetic 4,096 × 4,096 VT ("Fixtures"), camera at 2 m: in `stream` mode the requested pages become resident within 48 frames (three hops above the 21-frame floor of "Request latency floor" at F = 3), and the frame 64 image equals the `full` mode image within 1/255 |
| `Test_VirtualTexture_TailReservationRefusesActivation` | with `render.vt.maxTailBytes` filled by the tails of synthetic 2,048 × 256 VTs, the next activation fails with `FLUX_VT_ERROR_TAIL_BYTES_FULL`, the material binds the usage fallback and nothing else changes; an unload returns the bytes |
| `Test_VirtualTexture_HostCapRefusesActivation` | "Host memory bound" (VirtualTexturing.md): with `render.vt.maxIndexBytes` filled by synthetic VTs, the next activation fails with `FLUX_VT_ERROR_HOST_BYTES_FULL`, the material binds the usage fallback and nothing else changes; an unload returns the bytes and the activation then succeeds |
| `Test_VirtualTexture_FullModeSettles` | in `full` mode every tile becomes resident, `IsSettled()` turns true, and a VT larger than the cache is FAILED with `FLUX_VT_ERROR_POOL_FULL` |
| `Test_VirtualTexture_DeferredFrameKeepsPageTable` | "Deferred and failed frames" (VirtualTexturing.md): a 12-frame sequence with frame 5 deferred, and again with frame 5 failed, ends with the GPU page table equal to the mirror and equal to the undeferred sequence's; the deferred frame's feedback is dropped |
| `Test_VirtualTexture_PressureBiasCoarsensRequests` | forced WARNING raises every request by one mip and CRITICAL by two and halves the read and upload limits; both revert one frame after the level falls; the cache's size never changes (with Rendering's `Test_Memory_WarningShedding`) |
| `Test_VirtualTexture_DeviceLossRebuilds` | "Device loss recovery" (VirtualTexturing.md), on both backends |
| `Test_VirtualTexture_DisabledBindsFallback` | with `render.vt.enabled` false: no device resource is created, a VT load fails with `FLUX_VT_ERROR_NOT_ENABLED`, the material binds the usage fallback and no pass declares a VT field |
| `Test_VirtualTexture_CameraCutBoost` | a camera cut raises the upload and decode limits from `render.vt.tilesPerFrame` to the `Z2-B-ADVGFX-002` value for 30 frames, never past that value and never ahead of the `BLOCKING` class; a `tilesPerFrame` above half the row value is refused with `FLUX_VT_ERROR_INVALID_DESC` |
| `Test_VirtualTexture_UploadStarvedCounted` | with the upload ring filled by higher streaming classes, a frame in which `VT_TILE` receives fewer tiles than the smaller of its floor and the decoded tiles waiting counts one `Flux.VT.UploadStarvedFrames`; with GEOM's floor applied (row AV19) the class receives at least the floor |
| `Test_VirtualTexture_ReadAndDecodeFailures` | a failed read returns the tile to `IDLE` and counts it; a tile with a mutated stored byte fails its check, counts `Flux.VT.CorruptTiles`, marks only that page corrupt, and the VT stays RESIDENT |
| `Test_VirtualTexture_ReaderRejectsCorruption` | one mutated fixture per reader row V1 … V10 (VTCook.md "Reader validation") returns its error |
| `Test_VirtualTexture_LimitsAtAndPastLimit` | every row of "Parser limits" (VTCook.md) at the limit and at limit + 1 |
| `Test_VirtualTexture_LoaderContract` | the loader's members through `Zenith_AssetKindLoaderHarness` (cancel before activation, unload in the queue, unload resident, a device-recovery load) |
| `Test_VirtualTexture_HeadlessValidateOnly` | a headless registry serves kind 18 with the validate-only loader and creates no GPU object |
| `Test_VirtualTexture_CountersEmitted` | one frame emits every counter of "Diagnostics and profiling" (`Zenith2/Docs/Design/ADVGFX.md`) once through a recording profiler hook |
| `Test_VirtualTexture_ConfigKeysRegistered` | every `render.vt.*` key registers with its type, default per platform, domain and restart flag; out-of-domain values are refused by the schema |
| `Test_VirtualTexture_BudgetRefusesOversizedPools` | layers and extent whose GPU total exceeds the device budget value, and a configuration whose host worst case exceeds the host row, each return `FLUX_VT_ERROR_BUDGET_EXCEEDED` and create nothing |
| `Test_VirtualTextureSystem_Create_FailsAtPhysicalCache_Unwinds`, `…FailsAtPageTable_Unwinds`, `…FailsAtRecordBuffer_Unwinds`, `…FailsAtStagingPool_Unwinds` | injection at that step returns its error and releases exactly the earlier steps ([Z2-N-CONTRACT-341]) |
| `Test_VirtualTexture_DevToolsRegistered` | `Flux_RegisterVirtualTextureDevTools` adds the four console commands and the overlay tab through the area tokens; a fifth registration beyond capacity is refused with one log line |
| `Test_VirtualTexture_TileUploadThenSample` | a real tile read, decoded and uploaded through `Flux_StreamingUploader::Enqueue` in class `VT_TILE`: once the upload is `COMPLETE` and the page mapped, sampling the page returns the tile's texels; before the mapping the ancestor's (Rendering's GPU scene test `Test_GPUScene_VTTileUploadThenSample`, [Z2-N-REND-555], is RHI's and is cited, not owned, here) |

## VT cook tests

File `Zenith2/Tools/ZenithCook/Tests/VirtualTexture/Zenith_CookVirtualTexture_Tests.cpp`; inputs through ASSET's cook test-access header (ruling GEOM-N2).

| Test | Asserts |
|---|---|
| `Test_CookVirtualTexture_ShapeRulesEnforced` | a 1,000-texel width, a 128-texel axis, a 32,768 × 4,096 shape, a 16 : 1 aspect, two layers and a `HEIGHT` usage each fail with their error before any image is allocated; 256 × 256 (encoder path) and 16,384 × 2,048 (synthetic block-grid path) succeed |
| `Test_CookVirtualTexture_SyntheticBlockGridPath` | `Zenith_VTCookTestAccess::CookFromBlockGrid` of a 2,048 × 2,048 page-ID grid gives a `.zvt` that passes every reader row, with tiles equal to a direct cut of the grid by the rules of "Tile assembly" (VTCook.md) |
| `Test_CookVirtualTexture_TileBordersMatchNeighbours` | in a cooked 512 × 512 VT every tile's border blocks equal the adjacent interior blocks of its neighbour byte for byte |
| `Test_CookVirtualTexture_ClampRing` | a clamp VT's edge-tile borders decode to edge-replicated texels |
| `Test_CookVirtualTexture_WrapBorders` | a wrap VT's edge-tile borders equal the blocks across the opposite edge |
| `Test_CookVirtualTexture_TileCompressionAndStoreFallback` | a compressible tile carries zstd with a smaller stored size; a random-noise tile is stored `NONE`; both decode to the assembled bytes |
| `Test_CookVirtualTexture_TailAndRootSections` | the tail holds mips M … end with the computed sizes; the root section holds exactly the root tiles; the tile index offsets point into the right sections |
| `Test_CookVirtualTexture_Deterministic` | two cooks, clean and incremental, give byte-identical files |
| `Test_CookVirtualTexture_ReportEntry` | the cook report's `virtualTextures` entry carries every documented field with the right values |
| `Test_CookVirtualTexture_EngineFallbacksDeclared` | the step declares roles 0 … 4 with the documented paths, and a cook with one fallback source removed fails with `COOK_ERROR_FALLBACK_MISSING` |
| `Test_CookVirtualTexture_MipBiasRange` | `texture.virtualMipBias` −1 and 2 succeed, −1.5 and 2.5 fail with `ADVGFX_COOK_ERROR_MIP_BIAS` |
| `Test_CookVirtualTexture_FormatsPerPlatform` | Windows cooks give BC7 (sRGB for colour), Android cooks ASTC 4 × 4; the runtime reader refuses the other platform's file with `FLUX_VT_ERROR_FORMAT_UNSUPPORTED` |

## Path tracer tests

File `Zenith2/Tools/ZenithPathTracer/Tests/Flux_PathTracer_Tests.cpp` unless stated.

| Test | Asserts |
|---|---|
| `Test_PTBVH_BuildDeterministic` | two builds of a 100,000-triangle fixture are byte-identical, and identical whatever order the triangles' instances were added in when the triangle order is fixed |
| `Test_PTBVH_SAHLeafRules` | no leaf holds more than 4 triangles; a leaf is made exactly when no split beats the leaf cost; every triangle appears in exactly one leaf; every node bounds its subtree |
| `Test_PTBVH_LimitsRefused` | 16,777,217 triangles (synthetic counts) and a depth-65 degenerate input return `FLUX_PT_ERROR_BVH_LIMIT` |
| `Test_PTTraversal_WatertightEdges` | 2²⁰ rays aimed at shared edges and vertices of a closed mesh hit exactly one triangle each; none passes through |
| `Test_PTTraversal_ClosestHitMatchesBruteForce` | for 65,536 random rays the closest hit equals a brute-force CPU search |
| `Test_PTTraversal_AnyHitShadow` | shadow rays report occlusion iff a brute-force search finds a hit before t_light; an alpha-tested quad with a half-transparent texture occludes exactly where its opacity is above the cutoff |
| `Test_PTSampling_GGXVNDFPdfIntegratesToOne` | the VNDF pdf integrates to 1 ± 1e-3 over the hemisphere at α ∈ {0.002, 0.05, 0.5, 1} |
| `Test_PTSampling_CosinePdf` | the cosine sampler's histogram matches cos θ / π within 1% over 2²⁰ samples |
| `Test_PTSampling_MISWeightsSumToOne` | for every (light kind, BSDF) pair, the light-sample and BSDF-sample power-heuristic weights of one direction sum to 1 ± 1e-6 |
| `Test_PTSampling_AliasTableMatchesDistribution` | the environment alias table reproduces its weights within 0.5% over 2²⁴ draws |
| `Test_PTSampling_SphereLightSolidAngle` | the irradiance from a sphere light estimated by cone sampling equals the analytic form factor within 0.5% |
| `Test_PTRandom_SequencesIndependentOfBatch` | the per-pixel results after 256 samples are bit-identical with batch sizes 16, 64 and 256 |
| `Test_PTAccumulate_DoubleSumsAndStandardError` | the CPU accumulator's mean and standard error equal a direct double computation over the same samples |
| `Test_PTConvergence_CriterionAndCaps` | a synthetic noise source converges after the expected batch; a wall-clock cap of 0 ms returns `NOT_CONVERGED` after the first batch; reaching the maximum samples returns `NOT_CONVERGED` |
| `Test_PathTracer_RunIsDeterministic` | two runs of `PTSelf_SphereLightFormFactor` on one device return bit-identical results |
| `Test_PathTracer_SelfWhiteFurnace` | every pixel within 3 SE of 1 |
| `Test_PathTracer_SelfLambertPlane` | every pixel within 3 SE of the closed-form `ρ/π × I cos θ / d²` |
| `Test_PathTracer_SelfSphereLightFormFactor` | every pixel within 3 SE of the analytic irradiance × ρ/π |
| `Test_PathTracer_SelfRayDifferentials` | the primary-hit UV gradients of a tilted textured plane equal the visibility buffer's analytic gradients within 1e-5 relative |
| `Test_PathTracer_IntegratorAmbientVisibilityAnalytic` | a point at distance d from an infinite wall with radius R: visibility equals the closed form within 3 SE |
| `Test_PathTracer_IntegratorLightVisibilityAnalytic` | a sphere light half-occluded by a plane's edge gives 0.5 within 3 SE at the edge's projection |
| `Test_PathTracer_IntegratorSpecularMirror` | a perfect mirror (roughness 0.045, F0 1) reflecting a uniform emitter returns the emitter radiance × E_tot within 3 SE |
| `Test_PathTracer_BatchRunsOutsideFrameLoop` | "Execution substrate" (PathTracer.md): with no render frame running, a `Run` of `PTSelf_WhiteFurnace` completes, each batch is one `RunToolGraph` call whose readbacks are `READY` on return, and the upload ring, readback pool, deferred-deletion queue and material-slot table are idle after the tracer is destroyed |
| `Test_PathTracer_CancelReleases` | `Cancel` during a run returns `CANCELLED` at the next batch and leaves no allocation or readback range behind |
| `Test_PathTracer_Create_FailsAtPathBuffers_Unwinds`, `…Run_FailsAtSceneUpload_Unwinds`, `…Run_FailsAtAccumulators_Unwinds`, `…Run_FailsAtPSOs_Unwinds`, `…Run_FailsAtReadbacks_Unwinds` | each injection point returns its error and releases what the call allocated |
| `Test_PTReference_RecordingSetSizeRefused` | the recording harness's writer, given a reference set whose stored total exceeds 16 MiB, writes nothing and returns its error |
| `Test_PTReference_RoundTrip` | write then read a reference of each integrator: every field and every pixel round-trips (planes through `float16`) |
| `Test_PTReference_ReaderRejectsCorruption` | one mutated file per validation rule returns `FLUX_PT_ERROR_REFERENCE_CORRUPT` |
| `Test_PTReference_FingerprintCoversInputs` | each of the seven fingerprint inputs changed alone changes the fingerprint; unchanged inputs give the same fingerprint |

## Physical-accuracy runner tests

File `Zenith2/Engine/ZenithFlux/Tests/PhysicalAccuracy/Flux_PhysicalAccuracy_Tests.cpp` unless stated.

| Test | Asserts |
|---|---|
| `Test_PathTracer_VirtualTextureFullMode` | moved here from the tracer tests because it needs the relative-luminance metric of the runner support and the VT fixtures: a plane with a VT material, traced with no raster frame (the tracer pumps VT settlement), and the same texture streamed, trace to images whose relative luminance error is below 1% |
| `Test_PTSceneDocument_MatchesParityLoader` | for every reference scene, the scene-document helper and RPROD's parity loader return identical instance, light and camera lists |
| `Test_PhysicalAccuracy_ReferencesFresh` | every row of `References.json` names an existing file whose header fingerprint equals the recomputed one, or carries a well-formed `pendingRecording` key; an unmarked mismatch names the scene and the first differing input; the stored `bytes` sum to at most 16 MiB |
| `Test_PhysicalAccuracy_PendingRecordingProtocol` | "Stale reference landing protocol" (PathTracer.md) on a fixture manifest: a marked stale entry skips with its reason and skips its comparison, an unmarked one fails naming the input, a marker on a fresh entry fails, and acceptance mode fails while any marker remains |
| `Test_PhysicalAccuracy_RunnerModes` | on `PTSelf_LambertPlane` the gate mode compares against a fixture reference written in the test and the acceptance mode traces to convergence then compares |
| `Test_PTMetrics_FlipMatchesReferenceImplementation`, `Test_PTMetrics_RelativeLuminanceAnalytic`, `Test_PTMetrics_MaskRespected`, `Test_PTMetrics_ZScoreSyntheticNoise`, `Test_PTMetrics_DownsampledReferenceSE` | "Error metrics" and "Gate and acceptance runs" (PathTracer.md); file `Flux_PTMetrics_Tests.cpp` in the same directory |
| `Test_PhysicalAccuracy_FailureRetainsArtefacts` | a forced tolerance failure writes the report and every artefact of "Reports and artefacts" (PathTracer.md) |
| `Test_PhysicalAccuracy_RasterCaptureUnits` | the raster HDR capture (`R11G11B10_UFLOAT`, "Raster capture" of PathTracer.md) of a Lambert plane under a point light, divided by the pre-exposure, equals the closed form within 2% per pixel (the B channel's 1.56% quantisation plus TAA's resolve error) |

## Fixtures

| Fixture | Produced by | Consumers |
|---|---|---|
| encoder-path VT fixtures (page-ID pattern, checker and smooth gradient at 256 × 256 and 512 × 512, a 2,048 × 256 non-square at the 8 : 1 aspect, wrap and clamp) | the VT fixture set ([Z2-N-ARCH-257]; the binary-artefacts ruling's test-time generation): the setup case `Test_VirtualTexture_CookFixtures` in `ZenithCook_Tests`, suite `z2-fixture-setup`, registered as the entry `ADVGFX_VTFixtures` with `FIXTURES_SETUP AdvgfxVTFixtures`, cooks procedural images through `Cook_VirtualTexture` with the real block encoder into `${ZENITH_OUT}/test-fixtures/ADVGFX/`, passed to consumers as `ZENITH2_ADVGFX_VT_FIXTURES`; under 4 million texels with borders and mips per platform format, and the setup entry itself runs under the 60-second CTest timeout, which bounds it by name; registered in the `win` presets only, where `ZenithCook_Tests` builds | the sampling, filtering, addressing, border and format tests |
| synthetic VT fixtures (2,048 × 2,048, 4,096 × 4,096, 4,096 × 512) | the same setup through `Zenith_VTCookTestAccess::CookFromBlockGrid` (`Zenith2/Tools/ZenithCook/Internal/VirtualTexture/Zenith_VTCookTestAccess.h`, a test-only friend of the cook step): a deterministic page-ID block grid of the target extent goes straight to tile assembly and compression, so no texel is encoded (at most 1,365 tiles, 25 MB assembled) | the residency, eviction, streaming, page-table and latency tests; every `ZenithFlux_Tests` case that reads a fixture of either row carries the suite `z2-advgfx-cooked` (`FIXTURES_REQUIRED AdvgfxVTFixtures`), and the codec, allocator, mirror, reader and limit cases carry none, so they run in every preset |
| mutated `.zvt` files | built in code, one mutation per reader row, from a minimal valid 256 × 256 file that the header-only helper `Zenith2/Engine/ZenithFlux/Tests/Support/VirtualTexture/Flux_VTTestFile.h` writes (synthetic tiles, `NONE` compression); the fuzz target builds its seed corpus the same way, so neither needs the fixture set in presets without its setup entry | `Test_VirtualTexture_ReaderRejectsCorruption`, the fuzz seed corpus |
| self-check scenes | built in code through `Flux_PathTraceSceneBuilder` | the tracer tests |
| the `VirtualTexture` parity scene | committed JSON in RPROD's parity layout, its VT cooked at build | RPROD's parity suites |
| committed references | the `needs-human` recording ticket ("Reference capture procedure" of PathTracer.md) | the gate mode, freshness |

No ADVGFX ticket commits a binary artefact in a worker diff (the binary-artefacts ruling: test-time generation, a `needs-human` recording only where a committed golden is required); the references are the recording ticket's.

## Benchmarks and harnesses

| Harness | Path | CTest smoke | Full run |
|---|---|---|---|
| `Zenith_Bench_PathTraceReference` | `Zenith2/Benchmarks/PathTraceReference/` | record one 32 × 32 self-check scene at 64 spp and read it back | the `needs-human` recording ticket |
| `Zenith_Bench_VirtualTexture` (Stage 4, with the VT implementation) | `Zenith2/Benchmarks/VirtualTexture/` | 60 frames of a scripted fly-over of a VT-backed terrain fixture, asserting no failure counter rises | the Stage 4 budget run measures `Z2-B-ADVGFX-001` … `-003`, the host row, the two latency rows and the S24 VT-branch row on W-Outdoor and W-OpenWorld, and runs the 16,384 × 16,384 synthetic VT stream-to-resident case; the Stage 1 budget ticket for ADVGFX allocates the rows without it |
| `Zenith_Bench_RayTracing` (Stage 7) | `Zenith2/Benchmarks/RayTracing/` | one frame of each RT feature on a fixture | the D-RT-SCOPE (open, Stage 7) measurement |
| `Zenith_Bench_VirtualGeometry` (Stage 7) | `Zenith2/Benchmarks/VirtualGeometry/` | one frame of the VG fixture | the D-VGEO-RASTER (open, Stage 7) measurement |

## Device runs

| Run | Content | Label |
|---|---|---|
| RTX 3070 VT budget run | W-Outdoor and W-OpenWorld with VT-backed terrain and props: `Z2-B-ADVGFX-001`, `-002`, `-003`, the host row and the two latency rows | `needs-human`, `gpu-reason:visual-measure` |
| S24 VT run | the same workloads on the S24 with the mobile parameters, plus the A/B of "Cost of the VT branch" (`ForwardPlusOpaque` GPU time on a VT-free scene, `render.vt.enabled` true versus false) for the S24 VT-branch row, unplugged, the thermal procedure of `Zenith2/Docs/Budgets.md → "Frame-rate definitions and measurement procedure"` | `needs-human`, `gpu-reason:visual-measure` |
| S24 VT correctness | `Test_VirtualTexture_StreamToResidentScene` and `Test_VirtualTexture_FilteringMatchesStreamedTexture` through the test-APK route (ruling X208-device-fixtures) | `needs-human` device step |
| RT and VG runs (Stage 7) | RayTracing.md "RT validation", VirtualizedGeometry.md "VG validation" | `needs-human`, `gpu-reason:visual-measure` |
