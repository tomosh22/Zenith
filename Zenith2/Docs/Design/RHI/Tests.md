# RHI design: tests

This file is part of the RHI design family. The entry document is `Zenith2/Docs/Design/RHI.md`. It lists every test file and every named test of the RHI area with the case it asserts and the gate step that runs it. Test names follow `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"` (heading "Test naming"); a test ID `Test_<Type>_<Behaviour>` maps to the doctest name `"<Library>/<Type>/<Behaviour>"`. Every GPU test runs on both headless CI devices (WARP for D3D12, lavapipe for Vulkan) with validation on (entry document "The sync-validation-clean rule") unless its row says otherwise, and records the skip reason when a capability is absent ([Z2-N-REND-034]). A row marked **(hidden window)** creates a swapchain and so a hidden window first (entry document "CI windows for GPU tests"); every other GPU test creates a device with no window.

## Test files

| File | Target | Contents |
|---|---|---|
| `Zenith2/Engine/ZenithRHI/Tests/Zenith_RHITypes_Tests.cpp` | `ZenithRHI_Tests` | value types, queue values, error strings and categories |
| `Zenith2/Engine/ZenithRHI/Tests/Zenith_DeviceHandle_Tests.cpp` | `ZenithRHI_Tests` | the handle tests of [Z2-N-CONTRACT-096] against a device-free table |
| `Zenith2/Engine/ZenithRHI/Tests/Zenith_RHIResourceTable_Tests.cpp` | `ZenithRHI_Tests` | the table's allocate, resolve, free, exhaustion |
| `Zenith2/Engine/ZenithRHI/Tests/Zenith_RHIRetirementQueue_Tests.cpp` | `ZenithRHI_Tests` | retirement against a fake timeline |
| `Zenith2/Engine/ZenithRHI/Tests/Zenith_RHIDescriptorHeap_Tests.cpp` | `ZenithRHI_Tests` | slot lists against a fake timeline |
| `Zenith2/Engine/ZenithRHI/Tests/Zenith_FormatFeatures_Tests.cpp` | `ZenithRHI_Tests` | the generated table and the generator's error messages |
| `Zenith2/Engine/ZenithRHI/Tests/Zenith_RHIConfig_Tests.cpp` | `ZenithRHI_Tests` | the key table and `Zenith_ReadRHIConfig` |
| `Zenith2/Engine/ZenithRHI/Tests/Support/Zenith_RHITestHooks.h` | header-only, included by RHI, backend and Flux tests | `Zenith_RHITestHooks : Zenith_RHIFaultInjector` (per-site countdowns, `SimulateDeviceLost()`), `Zenith_RHITestValidationSink` (counts messages, fails the test on any unsuppressed warning or error) |
| `Zenith2/Engine/ZenithRHI/Tests/Examples/Zenith_RHIDevice_Example.cpp` | `ZenithRHI_Tests` | the compiled example of the device contract block |
| `Zenith2/Engine/ZenithRHI/Tests/Conformance/Zenith_RHIConformance_*.cpp` | `ZenithRHI_Conformance` | the conformance suite, run once per backend (`[Vulkan]`, `[D3D12]`) |
| `Zenith2/Engine/ZenithRHI_Vulkan/Tests/Zenith_Vulkan_Tests.cpp`, `Zenith_VulkanProfile_Tests.cpp` | `ZenithRHI_Vulkan_Tests` | backend-specific tests |
| `Zenith2/Engine/ZenithRHI_D3D12/Tests/Zenith_D3D12_Tests.cpp` | `ZenithRHI_D3D12_Tests` | backend-specific tests |
| `Zenith2/Engine/ZenithFlux/Tests/Lifetime/`, `Upload/`, `Readback/`, `Bindless/`, `Memory/`, `Recovery/`, `PipelineCache/`, `Graph/Backend/`, `GPUScene/` | `ZenithFlux_Tests` | the Flux-side tests of this area |
| `Zenith2/Engine/ZenithFlux/Tests/GPUScene/Flux_GPUSceneTestAccess.h` | header-only, included by GPU-scene test sources alone | `Flux_GPUSceneTestAccess`, the friend of `Flux_GPUScene` that drives the material-slot range steps (`Zenith2/Docs/Design/RHI/Memory.md → "Material-slot range listener"`) |
| `Zenith2/Benchmarks/BindlessProbe/` | `Zenith_Bench_BindlessProbe` | the D-SLANG (open, Stage 1) probe and the half-precision check, with a CTest smoke run |
| `Zenith2/Benchmarks/Budgets/RHI/` | `Zenith_Bench_Budgets_RHI` | the Stage 1 budget harness |

Gate steps: the `Test` step runs every `*_Tests` and `ZenithRHI_Conformance` target in the `win` presets (`win-debug`, `win-release`) with WARP and lavapipe; `Sanitizers -Preset win-asan` runs them under ASan when a diff reaches the `Zenith2Memory` paths; `ShaderChecks` and `BackendParity` (Stage 3) come from the `Zenith2Render` category, whose paths include `ZenithRHI*` (Z2-D-044 categories, `Zenith2/Docs/Contracts.md → "Enforceability matrix"`). Android presets build every test target and never register them with CTest ([Z2-N-ARCH-180]); on-device runs are the device steps below.

Tests owned by another area that serve this catalogue's evidence are named with their owner: `Test_RenderGraph_SubresourceMipChain` and `Test_RenderGraph_TimestampsPerPass` (RPROD's, `Zenith2/Docs/Design/RPROD/Tests.md → "Graph unit tests"`, also listed in "Graph back-end tests" below), `Test_RenderGraph_ParallelRecordMatchesSerial` and `Test_FormatFeatures_GraphRejectsUngrantedUse` (RPROD's, as their rows say), and `Test_Runtime_HeadlessSkipsDevice` (MOD's, `Zenith2/Docs/Design/MOD.md → "Runtime and composition tests"`).

## RHI unit tests

| Test | Asserts |
|---|---|
| `Test_RHIError_ToStringAndCategory` | every enumerator has a name and the category of the table in PublicAPI.md "Zenith_RHIError header" |
| `Test_RHIQueueValues_IsSatisfiedBy` | per-queue comparison, zero last-use satisfied by anything, `Merge` is the per-queue maximum |
| `Test_DeviceHandle_NullDoesNotResolve`, `Test_DeviceHandle_StaleAfterDestroy_DoesNotResolve`, `Test_DeviceHandle_ForeignOwner_DoesNotResolve`, `Test_DeviceHandle_GenerationExhaustion_RetiresSlot` | [Z2-N-CONTRACT-096] for device handles; foreign means another kind's bits and a slot beyond the table; exhaustion starts a slot at generation `0xFFFFFE` through the table's test constructor argument |
| `Test_DeviceHandle_KindMismatchIsStale` | a buffer handle's bits forced into a texture lookup returns `STALE_HANDLE` in `win-release` |
| `Test_DeviceHandle_LiveHandlesReportedAtDestruction` | destroying a table with live records logs each with its name |
| `Test_RHIResourceTable_FullReturnsTableFull` | capacity reached: `ZENITH_RHI_ERROR_TABLE_FULL`, table unchanged |
| `Test_RHIResourceTable_FreedSlotReusedLast` | FIFO reuse order |
| `Test_RHIRetirementQueue_RetiresOnlyWhenEveryQueueCompleted` | an entry with last use {5, 3, 0} retires at completed {5, 3, 0} and not at {5, 2, 9} ([Z2-N-REND-488]) |
| `Test_RHIRetirementQueue_RetiresEverySatisfiedEntry` | out-of-order satisfaction retires all satisfied entries, not only the oldest ([Z2-N-REND-489]) |
| `Test_RHIRetirementQueue_NeverUsedRetiresNextFrame` | all-zero values retire at the first call ([Z2-N-REND-491]) |
| `Test_RHIRetirementQueue_FullWaitsOnSubmitThread` | the full policy, driven past every bound by direct enqueues alone: the 65,537th enqueue waits for the oldest and increments the wait counter ([Z2-N-REND-490]); the frame's counters read `rhi.retirement.full` 1 (its alias `RHI.Retirement.FullWaits` the same), `rhi.retirement.dropped` 0 and `rhi.retirement.high_water` 65,536 |
| `Test_RHIRetirementQueue_ReleaseRequestTakesDrainValues` | a request made before a submit retires only after that submit's value completes |
| `Test_RHIRetirementQueue_ClientEntriesReturnedInOrder` | client entries come back in enqueue order |
| `Test_RHIRetirementQueue_MassReleaseInOneFrame` | deterministic, with a fake timeline that completes nothing until the test says so: frame 1 frees every slot of a maximum-capacity instance table and releases 100,000 textures, so its 8 client entries go in and the drain with `uFLUX_RETIREMENT_DRAIN_HEADROOM` fills the ring to 65,536 − 512; frames 2, 3 and 4 each make the per-interval maximum of direct enqueues (`uFLUX_RETIREMENT_DIRECT_PER_INTERVAL` = 105: 8 client, 24 resource, 73 render-idle) with nothing completed, and none of those enqueues waits or asserts (`GetFullWaitCount()` stays 0, no `WaitCPU` call on the fake timeline); the test then completes every value, and the leftover requests drain over the following frames with every object retired once |
| `Test_RHIRetirementQueue_EnqueueAboveSignalledAsserts` | `z2-death-assert`: an `Enqueue` whose last use exceeds `GetLastSignalledValues()` on a queue |
| `Test_RHIRetirementQueue_DrainNeverWaits` | a drain with headroom H into a ring holding 65,536 − H − 10 entries moves 10 requests, leaves the rest queued in order, and calls no `WaitCPU` |
| `Test_RHIRetirementQueue_DrainStopsAtHeadroom` | with headroom 512 and 100,000 queued requests, one drain leaves exactly 65,024 pending; a drain into a ring already at or above 65,024 moves none |
| `Test_RHIRetirementQueue_ListenerCalledBeforeSlotFree` | every retired `RESOURCE` entry calls `OnResourceRetired` exactly once, before its table slot is freed (a resource allocated in the listener cannot reuse the slot), and never for a `CLIENT` entry; the listener runs with lock 450 released |
| `Test_RHIDescriptorHeap_ReleasedWithResourceTakesDrainValues` | a slot released with its resource through `RequestReleaseWithDescriptor` before a submit enters the pending list at the drain with that submit's values, and is not reissued before every queue passes them ([Z2-N-REND-059]); a slot released after the drain waits for the next frame's values |
| `Test_RHIDescriptorHeap_SlotZeroNeverAllocated` | [Z2-N-REND-031] |
| `Test_RHIDescriptorHeap_FullSpaceRefused` | `DESCRIPTOR_SPACE_FULL` at capacity, no eviction ([Z2-N-REND-060], BR-REFUSE of [Z2-N-BUDGET-041]) |
| `Test_RHIDescriptorHeap_FreedSlotReusedAfterRetirement` | a freed slot is not reissued before every queue passes its values ([Z2-N-REND-059]) |
| `Test_RHIDescriptorHeap_DoubleWriteAsserts` | `z2-death-assert`: a second write before free |
| `Test_FormatFeatures_TableMatchesRendering` | every row of Rendering's table appears once with its letters and platform |
| `Test_FormatFeatures_GeneratorRejectsMalformedLine` | each malformed-line case gives `FormatFeatures.table:<line>: <reason>` |
| `Test_RHIDevice_DescValidation` | each out-of-domain `Zenith_RHIDeviceDesc` field (a null platform or heap, frames in flight 1 and 4, a heap capacity of 1,023 and of 1,048,577, a wait timeout of 99 ms and 60,001 ms, a capture tool in a Shipping configuration) returns `INVALID_DESC` before any API call (the fault injector records no step) |
| `Test_RHIConfig_KeysAndDefaults` | every key of the table in PublicAPIDevice.md "Zenith_RHIConfig header" is registered with its type, range and default; `d3d12` on Android is rejected |

## RHI conformance suite

`ZenithRHI_Conformance` links both backends ([Z2-N-ARCH-269]) and runs every case once per backend on the software adapter with `SYNC` (Vulkan) or `GPU` (D3D12) validation; each case reads its results back and compares them with a CPU expectation, bit for bit. These cases are the RHI half of backend parity: the same neutral command stream gives identical results on both APIs.

| Test | Asserts |
|---|---|
| `Test_RHIConformance_CreateDeviceOnSoftwareAdapter` | device creation succeeds; capabilities name a software adapter; the capability tier is desktop |
| `Test_RHIConformance_TimelinesStrictlyIncrease` | each submit returns last + 1 per queue; completed values reach them |
| `Test_RHIConformance_AbsentQueueAliasesGraphics` | with dedicated queues disabled by the injector, async and copy timelines are the graphics one |
| `Test_RHIConformance_CopyBufferRoundTrip` | upload, copy, readback of 1 MiB of a pattern |
| `Test_RHIConformance_TextureUploadEveryFormat` | each sampled format row is uploaded and read back per mip |
| `Test_RHIConformance_ClearValues` | colour and depth clears read back the clear value in each attachment format |
| `Test_RHIConformance_MultisampleRejected` | a texture desc and a graphics pipeline desc with a sample count of 4 each return `INVALID_DESC` on both backends, and every attachment the suite creates reports a sample count of 1 |
| `Test_RHIConformance_ComputeDispatchWritesStorage` | a compute pipeline writes a buffer through a bindless UAV |
| `Test_RHIConformance_PushConstantLocatesParams` | the 8-byte push constant reaches the params struct in the ring on both APIs |
| `Test_RHIConformance_DrawIndexMatchesCommand` | an indirect-count draw of 1,000 commands writes each command's index ([Z2-N-REND-070], the RHI half of `Test_Binding_DrawIndexParity`) |
| `Test_RHIConformance_DirectDrawIndexIsZero` | [Z2-N-REND-072] |
| `Test_RHIConformance_BarrierBatchOneCall` | a batch of textures, buffers and a global barrier records one API barrier call (counted through the backend's recording statistics) ([Z2-N-REND-389]) |
| `Test_RHIConformance_SplitBarrierHalves` | a split barrier's halves in one list order a write before a read |
| `Test_RHIConformance_CrossQueueTransfer` | a texture written on async compute and sampled on graphics reads correctly with release/acquire halves and a wait ([Z2-N-REND-391]) |
| `Test_RHIConformance_PlacedResourcesAlias` | two placed buffers at one offset in one heap, used in order with a discard, read correctly |
| `Test_RHIConformance_PlacementGroupMismatchRejected` | a placed texture in a buffer group returns `INVALID_DESC` |
| `Test_RHIConformance_DescriptorUpdateAfterBind` | a slot written while a pending list references the set is visible to the next submission |
| `Test_RHIConformance_TimestampsMonotonic` | timestamps read after completion increase through a frame; the period converts to nanoseconds |
| `Test_RHIConformance_PipelineCacheRoundTrip` | serialise, recreate from the payload, create the same pipeline: a hit on both APIs |
| `Test_RHIConformance_Y_UpClipSpace` | a triangle in the upper half of clip space lands in the upper half of the image on both APIs |
| `Test_RHIConformance_WaitTimeoutIsStatus` | a wait on a never-signalled value returns `TIMEOUT` status, not an error |

## Format feature and profile tests

| Test | File | Asserts |
|---|---|---|
| `Test_FormatFeatures_ProfileMatchesHeader` | `ZenithRHI_Vulkan/Tests/` | the profile JSON's formats section equals the header generated from `Zenith2/Engine/ZenithRHI/Internal/FormatFeatures.table` ([Z2-N-REND-042]) |
| `Test_FormatFeatures_DeviceRefusesMissingFeature` | `ZenithRHI_Vulkan/Tests/`, `ZenithRHI_D3D12/Tests/` | a mocked capability report lacking one bit is refused with `PROFILE_UNSUPPORTED` naming the format and letter |
| `Test_VulkanProfile_GeneratedMatchesJson` | `ZenithRHI_Vulkan/Tests/` | the generated files' recorded hash equals the JSON's and their recorded `vulkan-profiles` version equals the manifest pin |
| `Test_VulkanProfile_RefusalListsEveryMissingItem` | `ZenithRHI_Vulkan/Tests/` | a mocked device missing a feature, an extension, a limit and a format produces the exact refusal line of Vulkan.md "Refusal message" |
| `Test_VulkanProfile_LavapipePasses` | `ZenithRHI_Vulkan/Tests/` | the pinned lavapipe passes `VP_ZENITH2_baseline_windows` |
| `Test_D3D12Baseline_RefusalListsEveryFailedRow` | `ZenithRHI_D3D12/Tests/` | a mocked adapter failing three rows produces the exact refusal line |
| `Test_RHIDevice_ReportJsonWritten` | `ZenithRHI_Conformance` | a development-build device creation on each software adapter returns, through `Zenith_WriteRHIDeviceReportJson`, the bytes of `rhi_report.json` (written by MOD's startup to `user:/logs/rhi_report.json`) with `schemaVersion` 1, the backend, adapter, driver, tier and every `Zenith_RHICapabilities` member; a refused mock device's file carries its refusal lines |
| `Test_D3D12Baseline_WarpPasses` | `ZenithRHI_D3D12/Tests/` | WARP passes every row |
| `Test_D3D12_AgilityConstantsMatchManifest` | `ZenithRHI_D3D12/Tests/` | the header constants equal the manifest's Agility pin |
| `Test_D3D12_AgilityRuntimeLoaded` | `ZenithRHI_D3D12/Tests/` | the device reports the pinned SDK version |
| `Test_Vulkan_DeviceDispatchThroughTable` | `ZenithRHI_Vulkan/Tests/` | every device-level call site uses the device table (a source scan of `Internal/` for global device-level volk names) ([Z2-N-REND-021]) |
| `Test_Vulkan_NoThirdPartyInPublicHeaders` | `ZenithRHI_Vulkan/Tests/` | a compile of the public header with the third-party include paths removed succeeds (the Libgraph step proves the same across the tree) |
| `Test_FormatFeatures_GraphRejectsUngrantedUse` | `ZenithFlux/Tests/Graph/` | RPROD's graph check (listed for completeness; owned by RPROD) |

## Bindless probe

`Zenith_Bench_BindlessProbe` (`Zenith2/Benchmarks/BindlessProbe/`) is the Stage 1 harness of D-SLANG (open, Stage 1): a self-contained executable with its own minimal Vulkan and D3D12 code under `Internal/` (it runs before `ZenithRHI` exists), probe shaders under `Zenith2/Benchmarks/BindlessProbe/Shaders/` compiled at build time by the pinned `slangc` host tool for each lowering alternative, and a report writer. The direct `slangc` call is a named exemption from Z2-D-025 and [Z2-N-REND-101] (only `ZenithShaderCompiler` compiles Slang): the probe is a Stage 1 harness under `Zenith2/Benchmarks/`, never a runtime or shipped target, and it runs before `ZenithShaderCompiler` exists; the ticket that lands `ZenithShaderCompiler` routes the probe's shaders through it and retires the exemption. Its cases are exactly the table of `Zenith2/Docs/Rendering.md → "Decision D-SLANG"`:

`Test_BindlessProbe_NonUniformSampled`, `Test_BindlessProbe_NonUniformStorage`, `Test_BindlessProbe_MaxIndex`, `Test_BindlessProbe_PartiallyBound`, `Test_BindlessProbe_UpdateAfterBind`, `Test_BindlessProbe_HandleLayout`, `Test_BindlessProbe_DrawIndex`, `Test_BindlessProbe_DrawIndexLowering`, `Test_BindlessProbe_LinkTimeMaterial`, `Test_BindlessProbe_StorageFormats`.

| Mode | Where | Output |
|---|---|---|
| `--smoke` (CTest) | the `Test` step on WARP and lavapipe | every case for the lowering alternative built as default (2), pass or fail |
| `--full --alternative <1\|2\|3> --report <path>` | the needs-human ruling ticket on the RTX GPU, WARP, lavapipe and the S24 (the `android-arm64` build installed as an APK) | `Zenith2/Docs/Research/DSlang/<date>/probe_<device>.json`: device, driver, Slang version, alternative, pass or fail per case ([Z2-N-REND-973]) |

## Half-precision check

`Test_HalfPrecision_BitExact` (a case of the probe executable): a compute shader computes `half` add, multiply, fused multiply-add, float/half conversion and storage over 65,536 input pairs covering every half exponent, normals, denormals flushed per the device's reported behaviour, ±0, ±∞ and NaN, and compares each result bit for bit against a CPU oracle; it runs in the smoke mode on WARP and lavapipe and in the full mode on the S24 and the RTX 3070 ([Z2-N-REND-033]). Half-precision shaders do not enter CI until it passes on both CI devices.

## Lifetime and upload tests

| Test | File | Asserts |
|---|---|---|
| `Test_FrameLifetime_FramesInFlightBound` | `ZenithFlux/Tests/Lifetime/` | with 2 and 3 frames in flight, frame N's build never starts before frame N − F's final values complete (timeline trace) ([Z2-N-REND-492]) |
| `Test_FrameLifetime_SlotResourcesReusedAfterCompletion` | same | a slot's command pools, upload segment and timestamp pool are reset only after completion ([Z2-N-REND-493]) |
| `Test_FrameLifetime_FramesInFlightChangeRecreates` | same | (hidden window) changing `r.framesInFlight` from 2 to 3 and back drains every swapchain, calls `WaitIdle` (whose backend statistics record timeline waits and no `vkDeviceWaitIdle`) then `SetFramesInFlight`, rebuilds the Flux per-slot objects, recreates the swapchain with the new image count; the preparation made for the old slot is discarded (its upload segment released holding no byte, the old timestamp pools retired); the first frame after the change keeps its frame index, builds on slot 0 with a fresh reservation of `PredictReservation()`, and records into no command pool created before `SetFramesInFlight` (every `AcquireCommandList` names a new pool); a second case changes F right after a deferred frame, and the re-preparation reserves exactly the need `DeferFrame` recorded, so the frame after the change is not deferred again; the timestamps of the frames still pending in the old slots are read and handed to a fake `Flux_PassTimingPublisher` once per queue before the old pools retire (every submitted frame's record published exactly once); upload requests staged before the change complete; the next frames use slots 0 to F − 1 with validation clean |
| `Test_FrameLifetime_TimestampsPublishedAtSlotReuse` | same | with a fake `Flux_PassTimingPublisher`, each submitted frame's ticks are published once per queue with a non-zero pair count, at step 4 of the preparation that reuses its slot, with the record's frame index, pair counts and pass names (views into the slot's pool, valid after the frame's graph arena was reset); a deferred or failed frame writes no record and publishes nothing; the pools are reset only after the read |
| `Test_FrameLifetime_SlotCounterSkipsUnsubmittedFrames` | same | a deferred frame and a failed frame advance the frame index but not `m_ulSubmittedFrames`; the frame after each builds on the same slot, and step 1 reads the values of the frame last submitted in it |
| `Test_FrameLifetime_RenderIdleEntryAssertsInsideSubgraph` | same | `z2-death-assert`: `OnFramesInFlightChanged` called between `OnSubgraphBegin` and the `OnSubgraphEnd` that ends the frame's `Flux.Present` |
| `Test_RHIDevice_SetFramesInFlightRebuildsSlots` | `ZenithRHI_Conformance` | `SetFramesInFlight(3)` after `WaitIdle` makes `GetFramesInFlight()` 3, `BeginFrameSlot(2)` valid and the breadcrumb regions 3 slots long; 1 and 4 return `INVALID_DESC` with F unchanged |
| `Test_FrameLifetime_WaitTimeoutChecksLoss` | same | a stuck value times out at 2 s, checks status, and after 5 timeouts declares loss |
| `Test_Renderer_ShutdownOrder` | same | (hidden window) on WARP and lavapipe: shutdown waits every queue idle and retires every entry before the renderer is destroyed, destroys the renderer before the job system, and destroys the device last with an empty live-handle report |
| `Test_UploadRing_ReserveStallsOnSubmitThreadOnly`, `Test_UploadRing_AllocateNeverStalls`, `Test_UploadRing_RequestQueueFullRejects`, `Test_UploadRing_SplittableRequestCrossesFrames`, `Test_UploadRing_UnsplittableOverCapRefused`, `Test_UploadRing_DrawParamsBoundEnforced` | `ZenithFlux/Tests/Upload/` | [Z2-N-REND-497]'s named tests |
| `Test_UploadRing_GrowsToCapAndNoFurther` | same | growth stops at the cap row's configured value; the next need stalls ([Z2-N-REND-495]) |
| `Test_UploadRing_ExhaustionCounted` | same | every stall increments `Flux.Upload.Exhausted` and accumulates `Flux.Upload.StallMs` |
| `Test_UploadRing_DeferredFrameReservesExactNext` | same | a compile need above the reservation at the cap defers the frame and the next preparation reserves exactly the need; the deferred frame wrote no ring byte (its segment's cursor is 0 and a poisoned ring range is unchanged), allocated no transient view descriptor and ran no imported-state `Commit`; the GPU-scene tables after the deferred frame and its successor equal, bit for bit, those of the same render-world sequence run with no deferral (as `Test_GPUScene_DeltaReplayReproducesTables`); a readback declared in the deferred frame is `FAILED` with `FLUX_READBACK_ERROR_FRAME_DEFERRED` and its range free, and no history advances |
| `Test_UploadRing_AllocateBeforeExtendAsserts` | same | `z2-death-assert`: an `Allocate` on the build node before the frame's `ExtendReservation` (the violation of "Build-node upload order"); a second case calls `GetFrameData` before it |
| `Test_UploadRing_NeedAboveCapFailsFrame` | same | a compile need above the configured cap fails the frame: nothing recorded, submitted or presented, the pending store kept, `RHI.Upload.OverCap` incremented, one `flux.upload` error line naming the largest contributor (a second failed frame with the same contributor logs nothing more), the next preparation reserves `PredictReservation()` and not the need, a readback of the frame is `FAILED` with `FLUX_READBACK_ERROR_FRAME_FAILED`; a submitted frame between failures resets the count; 30 consecutive failed frames: the 30th `FailFrame` stores the exit request (`GetExitRequest()` is `FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP`), the next prologue's `Flux_DeviceLossHandler::RunPrologue` moves `NORMAL` → `EXITING` (`GetDeviceLossState()` reads `EXITING`, so no further frame builds), the platform message box receives exactly `The renderer could not fit a frame's uploads into memory. Your progress has been saved.`, the autosave is requested and the exit code is `EXIT_CODE_DEVICE_LOST` (6); no device-loss report file is written, `RHI.Device.Losses` and `RHI.Device.Recoveries` are unchanged, and the run's telemetry record carries the reason and the count 30 |
| `Test_UploadRing_StagedRequestsSurviveDeferral` | same | a request staged in the preparation of a frame that is then deferred is copied by the next submitted frame, released only after that frame retires, and reaches `COMPLETE` once |
| `Test_UploadRing_PredictionRule` | same | the step-2 prediction is the maximum of the compile's exact need over the last 8 built frames × 1.25 + 1 MiB, clamped to the cap, over a scripted need sequence |
| `Test_UploadRing_ShrinkAfter600Frames` | same | chunks above the initial capacity retire after 600 idle frames, and at once under warning pressure |
| `Test_UploadRing_CancelQueuedReleasesNothingStaged` | same | cancel while queued → `CANCELLED`, no copy recorded |
| `Test_UploadRing_CancelStress` | same, `z2-stress` | 1,000 issue-then-cancel cycles at random stages leave no ring bytes, operations or destination regions ([Z2-N-CONTRACT-339]) |
| `Test_UploadRing_SourceNotReadAfterStaged` | same | the source span is poisoned after `STAGED` and the result is still correct |

## Readback tests

| Test | Asserts |
|---|---|
| `Test_Readback_ReadyAfterFence` | `READY` exactly after the recording frame's value completes ([Z2-N-REND-498]) |
| `Test_Readback_DataValidUntilRelease` | the span stays valid across frames until `Release` |
| `Test_Readback_PoolFullDefers` | a request beyond the pool returns `POOL_FULL` and increments `Flux.Readback.Deferred` (BR-DEFER) ([Z2-N-REND-499]) |
| `Test_Readback_DeferredFrameFails` | a readback declared in a deferred frame moves to `FAILED` with `FLUX_READBACK_ERROR_FRAME_DEFERRED`, one declared in a failed frame with `FLUX_READBACK_ERROR_FRAME_FAILED`; each range is free at once, no copy is recorded, `Flux.Readback.Deferred` increments, and a re-request in the next submitted frame reaches `READY` |
| `Test_Readback_CancelReleasesAfterRetire` | `CANCELLED`, space reclaimed after the copy's frame retires ([Z2-N-REND-501]) |
| `Test_Readback_DeviceLossFails` | simulated loss moves pending readbacks to `FAILED` |
| `Test_Readback_LeakReportedAtShutdown` | an unreleased handle is logged by name |
| `Test_Readback_PublishedAtTickBoundary` | an editor-picking readback reaches simulation at a tick boundary in completion-ID order ([Z2-N-REND-500]) |
| `Test_Readback_CancelStress` (`z2-stress`) | 1,000 issue-then-cancel cycles leave no pool bytes or operations |

## Descriptor tests

The binding-model tests of [Z2-N-REND-076], owned and implemented here in `ZenithFlux/Tests/Bindless/`: `Test_Binding_PushConstantSize`, `Test_Binding_DrawIndexParity`, `Test_Binding_NullSlotZero`, `Test_Binding_SlotRetirementAcrossFrames`, `Test_Binding_ViewTypeMismatchRejected` (the last with RPROD's graph check). Plus:

| Test | Asserts |
|---|---|
| `Test_DescriptorAllocator_ExhaustionCounted` | `Flux.Descriptors.Exhausted` increments at the full space |
| `Test_DescriptorHeap_CapacitiesBelowProfileRefused` | a device whose update-after-bind limits are below the configured capacities is refused ([Z2-N-REND-030]) |
| `Test_DescriptorHeap_GlobalSetBoundOncePerPartition` | recording statistics show one bind per bind point per partition ([Z2-N-REND-054]) |

## GPU scene tests

The eleven tests of `Zenith2/Docs/Rendering/GpuResources.md → "GPU scene tests"` ([Z2-N-REND-555]), implemented in `ZenithFlux/Tests/GPUScene/` and run with validation clean: `Test_GPUScene_SkinnedSameFrameAllPasses`, `Test_GPUScene_ParticlesSimulateThenDraw`, `Test_GPUScene_ProceduralRegeneratedMidFrame`, `Test_GPUScene_VTTileUploadThenSample`, `Test_GPUScene_SharedTextureDuringStreamingSwap`, `Test_GPUScene_SlotReuseAcrossFramesInFlight`, `Test_GPUScene_PersistentReadOnlyWriteRejected`, `Test_GPUScene_ScatterCoalescesSlot`, `Test_GPUScene_GrowPreservesContents`, `Test_GPUScene_DeltaReplayReproducesTables`, `Test_GPUScene_HistoryResetZeroVelocity`. Additionally:

| Test | Asserts |
|---|---|
| `Test_GPUScene_NoRecordsNoScatterPass` | a frame without changes adds no scatter pass ([Z2-N-REND-543]) |
| `Test_GPUScene_TableFullRefusesWithoutEviction` | at the maximum capacity: `TABLE_FULL`, the counter, every existing slot intact ([Z2-N-REND-545]) |
| `Test_GPUScene_TableWriteRejected` | a pass writing a table other than scatter or growth: `FLUX_RG_ERROR_TABLE_WRITE` ([Z2-N-REND-536]) |
| `Test_GPUScene_PersistentVisibleAfterAcquire` | a new persistent texture is referenced by a table only after its acquire frame ([Z2-N-REND-548]) |
| `Test_GPUScene_RecordLayoutsMatchSlang` | the generated shared structs' sizes are 128, 128, 32 and 64 bytes with every offset of the tables ([Z2-N-REND-535]) |
| `Test_GPUScene_DeferredFrameKeepsDeltas` | a frame deferred after `ApplyDeltas` keeps its staged records, frees and growth pending; frees are not reused before the uploading frame retires; the next submitted frame uploads the coalesced records once; a second growth of the same table while one is pending (C → 2C deferred, then 4C) releases the unused 2C buffer and copies once from the original buffer into the 4C one |
| `Test_GPUScene_InitialCapacities` | on the desktop tier the four tables start at 262,144, 65,536, 65,536 and 4,096 records, on the mobile tier at a quarter of each ([Z2-N-REND-537]) |
| `Test_GPUScene_PoolGrowthBoundedByRow` | the skinned-vertex, particle and procedural pools grow by resize-and-copy with their contents preserved, stop at their memory row's configured value and then refuse with `FLUX_GPU_SCENE_ERROR_TABLE_FULL` ([Z2-N-REND-553]) |
| `Test_GPUScene_FullStateWorldRebuilds` | a render world with `m_bFullState` discards the slot state and pending store and reproduces the tables of a delta replay ([Z2-N-REND-015]) |
| `Test_GPUScene_SwapReservesTexturePeak` | a streaming swap holds a texture-swap transition reservation for its duration, and a refused reservation postpones the swap |

The range-listener tests of `Zenith2/Docs/Design/RHI/Memory.md → "Material-slot range listener"`, in `ZenithFlux/Tests/GPUScene/Flux_GPUSceneRangeListener_Tests.cpp`, run on WARP and lavapipe against the real `Flux_GPUScene` through `Flux_GPUSceneTestAccess`; their listener `Flux_TestRangeListener` is a plain `final` implementation of `Flux_GPUSceneRangeListener` in the test file that records every call in order and writes bucket IDs 100 + i, 200 + i and 300 + i into image i:

| Test | Asserts |
|---|---|
| `Test_GPUScene_RangeListener_CreatedOncePerKey` | acquiring (mesh M, materials {A, B}) twice calls `OnMaterialRangeCreated` once, with M, {A, B}, the range's first slot and two images that arrive as the not-drawn image; the reference count is 2; the assignment {A, C} creates a second range at a different first slot |
| `Test_GPUScene_RangeListener_ImagesStagedFromListener` | the pending material-slot records of a reported range equal the images the listener wrote; with no listener set they equal the not-drawn image, all three bucket IDs `uFLUX_RG_INVALID` |
| `Test_GPUScene_RangeListener_FreedOnLastRelease` | the first release of a range held twice calls nothing; the second calls `OnMaterialRangeFreed(uFirstSlot)` exactly once, with the reference count 0 and the range's slots pending free; acquiring the same key again before any retirement creates a range at a different first slot |
| `Test_GPUScene_RangeListener_UnreportedRangeNeverForwarded` | a range acquired before `SetRangeListener` is reported neither when the listener is set nor at its release; after `SetRangeListener(nullptr)` a release of a reported range calls nothing; a listener set afterwards receives no `OnMaterialRangeFreed` for a range reported to the previous one (the epoch rule) |
| `Test_GPUScene_RangeListener_FullStateFreesBeforeCreates` | with three live reported ranges, `DiscardSlotState` calls `OnMaterialRangeFreed` for each in ascending first-slot order and no `OnMaterialRangeCreated` before the last of them; re-acquiring the three keys reports three creations |
| `Test_GPUScene_RangeListener_TableFullReportsNothing` | acquiring distinct 255-material keys until `AcquireMaterialRange` returns `FLUX_GPU_SCENE_ERROR_TABLE_FULL`: the refused acquire calls no listener member, `Flux.GPUScene.TableFull` increments, and the creation count equals the number of successful acquires |
| `Test_GPUScene_RangeListener_ReplaceWithoutClearAsserts` | `SetRangeListener(&xSecond)` while `&xFirst` is set fails its assert (`z2-death-assert`) |
| `Test_GPUScene_RangeListener_ReentrantCallAsserts` | a listener that calls `SetRangeListener(nullptr)` from inside `OnMaterialRangeCreated` fails the in-callback assert (`z2-death-assert`) |
| `Test_GPUScene_RangeListener_DestroyWithListenerAsserts` | destroying the GPU scene while a listener is set fails the destructor's assert (`z2-death-assert`) |

## Memory tests

| Test | Asserts |
|---|---|
| `Test_Memory_WarningShedding` | forced `WARNING`: actions 1 to 6 of [Z2-N-REND-506] observed |
| `Test_Memory_CriticalShedding` | forced `CRITICAL`: actions 1 to 13 observed, reverted one frame after the level falls |
| `Test_Memory_PlacementAlignmentRules` | every placement of a fixture set respects alignment and group per API ([Z2-N-REND-503]) |
| `Test_Memory_CategoriesReportedToGovernor` | the eight categories sum to the resident bytes the consumer reports ([Z2-N-REND-504]) |
| `Test_Memory_PerHeapCapRaisesPressure` | usage above the per-heap budget reports `WARNING`, above 95% `CRITICAL` |
| `Test_Memory_DeviceOOMSkipsFrameThenRecovers` | an injected allocation failure at compile skips one frame, the retry after release succeeds |
| `Test_Memory_ThirtyOOMFramesExitCleanly` | 30 consecutive injected OOM failures: the 30th stores the exit request `FLUX_FRAME_FAILURE_DEVICE_OOM`, the next prologue enters `EXITING` from `NORMAL`, the message box receives exactly `The graphics device ran out of memory. Your progress has been saved.`, the autosave is requested, the exit code is `EXIT_CODE_DEVICE_LOST` (6); no device-loss report is written and `RHI.Device.Losses` is unchanged; the count is shared with over-cap failures (15 of each in a row also exit, with the last failure's message) |
| `Test_Memory_PersistentOOMLeavesAssetNonResident` | a persistent creation failure returns `OUT_OF_DEVICE_MEMORY`, reports `CRITICAL` and leaves no partial resource |

## Presentation and recovery tests

| Test | Asserts |
|---|---|
| `Test_Swapchain_CreatesEveryOutput` | (hidden window) SDR is always created; HDR10 and scRGB are created where supported and fall back to SDR with the warning otherwise; `GetInfo().m_uOutputColourSpacesSupported` always has the SDR bit and has a space's bit exactly when that space's creation succeeds without fallback; `m_bDisplayHdr` and `m_fDisplayMaxLuminanceNits` are read (false and 0 on both software adapters); on an HDR10 swapchain `SetHDRMetadata` reaches the API once (the backend's statistics count one `vkSetHdrMetadataEXT` or `SetHDRMetaData` call with the scaled values) where `m_bHdrMetadata` holds and returns `FEATURE_UNAVAILABLE` otherwise, a recreation of it re-sends the recorded metadata, an SDR swapchain is sent none, and a non-finite member returns `INVALID_DESC` |
| `Test_Swapchain_PacingHooks` | (hidden window) `WaitForLatencySlot` returns `REACHED` at once without present wait and waits on the latency object on D3D12; `WaitForPresent` returns `FEATURE_UNAVAILABLE` on D3D12 and without `VK_KHR_present_wait`; `SetMaximumFrameLatency(n)` is recorded; `GetPresentTimings` returns 0 entries where unsupported and monotonic-clock values otherwise; `SetDesiredPresentTime` returns `FEATURE_UNAVAILABLE` on D3D12 and without `VK_GOOGLE_display_timing` (lavapipe), and with it chains one `VkPresentTimeGOOGLE` into the matching present (backend statistics; a mocked extension on Windows); `SetSwapInterval(2)` makes the next D3D12 FIFO `Present` use sync interval 2 and succeeds on Vulkan with no change to the present call, `GetInfo().m_uSwapInterval` reads 2 and survives a recreation, and 0 and 5 return `INVALID_DESC` |
| `Test_Swapchain_OneSwapchainPerWindow` | (two hidden windows) each window gets its own swapchain; resizing one recreates that swapchain alone and the other keeps presenting |
| `Test_Swapchain_RecreationFailureRetries` | (hidden window) an injected `SWAPCHAIN_CREATE` failure returns the swapchain to `PENDING` and retries at the next prologue; the third consecutive failure enters the device-recovery policy |
| `Test_Swapchain_PresentModeFallbackReported` | (hidden window) an unsupported mode is replaced and reported by `GetInfo` |
| `Test_Swapchain_ZeroSizeProducesNoFrames` | (hidden window) a minimised window records no GPU work ([Z2-N-REND-984]) |
| `Test_Swapchain_OutOfDateRendersStandIn` | (hidden window) an injected out-of-date acquire keeps the frame's non-swapchain results and skips the present |
| `Test_Recovery_ResizeInvalidatesHistories` | (hidden window) [Z2-N-REND-988]: only the views whose extent changed lose their histories |
| `Test_Recovery_RecreationNeverWaitsDeviceIdle` | (hidden window) for every trigger of the recreation table the Vulkan backend records no `vkDeviceWaitIdle` and no `vkQueueWaitIdle` (backend statistics); every trigger but a frames-in-flight change records no `WaitIdle` call and waits only on the frames that used the swapchain; a frames-in-flight change records exactly one `WaitIdle`, realised as one timeline wait per queue |
| `Test_Recovery_DeviceLossRecover` | (hidden window) [Z2-N-REND-988]: a simulated loss is reported, the device recreated, resources reloaded, histories invalidated, rendering resumed, on both backends; on WARP also through `ID3D12Device5::RemoveDevice`; `RHI.Device.Losses` and `RHI.Device.Recoveries` each increment by one and the run's telemetry record carries the loss with its reason; recovery enqueues nothing on the lost device's retirement queue; for a loss injected at submit and for one injected into `PrepareNextFrame`'s step-2 wait, `IsRenderIdle()` is true at the first prologue after the loss, `Flux_DeviceLossHandler::RunPrologue`'s entry assert does not fire (the test runs with asserts enabled), `GetDeviceLossState()` reads `LOST` in that prologue, and the frames from `LOST` to `RESUMING` run with `bRunRender = false` (an empty render subgraph) |
| `Test_Recovery_SecondLossExitsCleanly` | (hidden window) a second loss within 60 s requests the autosave and exits with `EXIT_CODE_DEVICE_LOST` (6), and `FATAL_CODE_DEVICE_LOST` is raised only when the fault injector holds the autosave past 10 s |
| `Test_Swapchain_HiddenWindowPresents` | (hidden window) on WARP and lavapipe, a hidden `Zenith_Window` yields a surface, 10 frames acquire and present with validation clean, and `GetInfo()` reports the image count `GetFramesInFlight()` + 1 clamped |
| `Test_Recovery_BreadcrumbOverflowCounted` | a queue-frame of 5,000 fixture passes writes indices wrapped at 4,095 and increments `RHI.Breadcrumbs.Overflow` once per frame |
| `Test_Recovery_ReportJsonSchema` | the report written for a simulated loss parses as JSON and carries every key of Recovery.md "Device-loss report" with `schemaVersion` 1; in a development build the three `device_loss_<utc>_frame_<n>.json` dumps of the last frames lie beside it and parse |
| `Test_Recovery_SurfaceReleaseTimeoutKeepsDevice` | a simulated `OnReleaseSurface` (called directly on Windows with a hidden window) whose frames a fault-injected timeline never completes returns within the handshake's remaining budget without releasing, counts `RHI.Wait.Timeouts`, declares no loss, enters `SURFACE_STALE`, and releases the swapchain and surface at the first prologue after the frames complete |
| `Test_Pipeline_CacheHitFlagReported` | the second creation of one pipeline with a warm cache returns `m_bCacheHit` true and increments `RHI.Pipelines.CacheHits`, the first `RHI.Pipelines.CacheMisses` |
| `Test_Recovery_ExitPolicySkipsRecovery` | `rhi.deviceLossPolicy=exit` goes straight to the exit steps |
| `Test_Recovery_NoSubmitAfterLoss` | after loss the backend's submit counter does not change |
| `Test_Recovery_BreadcrumbsNamePasses` | a fixture pass that hangs (an injected infinite loop shader on the software adapter, timeout-bounded) is reported as the first incomplete pass with the pass before it as last completed |
| `Test_Recovery_ReportWrittenUnderHeapPressure` | the report is written with the general heap's fault injector failing every allocation |
| `Test_Recovery_AndroidSurfaceLossRestore` | the needs-human S24 device step (below) |
| `Test_PSO_CacheRejectsDriverMismatch`, `Test_PSO_CacheInterruptedWriteKeepsPrevious` | the persistent-cache cases of [Z2-N-REND-213] (owned here) |
| `Test_PipelineCache_EveryTriggerDiscards` | each row of `Zenith2/Docs/Rendering/GpuResources.md → "Pipeline cache invalidation"` discards the file, logs once and sets the cold flag, which reaches the run's telemetry record ([Z2-N-REND-989]) |
| `Test_PipelineCache_OldKeysDeleted` | stale files of the game are deleted at startup and the driver-change message is logged ([Z2-N-REND-990]) |
| `Test_PipelineCache_OverCapRebuilds` | a payload above the size cap deletes the file |
| `Test_PipelineCache_WriteCadence` | at most one write per 60 s, one at clean shutdown |
| `Test_Capture_UnavailableWithoutTool` | `rhi.capture=renderdoc` without the library leaves `GetCaptureTool()` null and logs |
| `Test_Debug_NamesAndLabels` | with `m_bDebugNames` set, `SetDebugName` reaches the API (the backend's naming statistics count one name per named resource, pipeline, queue and command buffer) and recording statistics show one debug label per pass, partition and batch; with it clear (the Shipping default) both counts are 0 |

## Validation tests

| Test | Asserts |
|---|---|
| `Test_Validation_SinkReceivesMessages` | a deliberate misuse in a fixture (a descriptor write with a mismatched view) reaches the sink with ID and severity, on both backends |
| `Test_Validation_TestSinkFailsOnMessage` | the test sink turns any unsuppressed warning or error into a test failure |
| `Test_Validation_SuppressionNeedsDecision` | a suppression entry without a `Z2-D-` ID fails loading ([Z2-N-REND-396]) |
| `Test_Validation_SyncValidationCatchesMissingBarrier` | a fixture recording a write then a read with no barrier produces a synchronization-validation error on lavapipe and a GPU-based-validation error on WARP |
| `Test_Validation_ModeDegradesWithoutLayer` | with the layer path cleared, `STANDARD` degrades to `OFF` with the warning (and a test executable fails its start-up check) |
| `Test_Validation_ShippingIgnoresKey` | the Shipping configuration never enables a layer |

## Graph back-end tests

| Test | Asserts |
|---|---|
| `Test_RenderGraph_AsyncGapRule`, `Test_RenderGraph_CrossQueueTransfer`, `Test_RenderGraph_SplitBarrierNotAcrossBatches`, `Test_RenderGraph_ParallelRecordMatchesSerial` | the back-end rows of `Zenith2/Docs/Rendering/RenderGraph.md → "Graph tests"`, owned here |
| `Test_GraphBackend_BatchesCutAtCrossQueueEdges` | batches start and end exactly at cross-queue dependencies; redundant waits removed |
| `Test_GraphBackend_SubmitOrderRespectsWaits` | the submitter never submits a batch before the batches it waits on |
| `Test_GraphBackend_PartitionTargetCost` | partitions respect 4,096 cost units, never split a merged scope or a batch |
| `Test_GraphBackend_SplitHalvesInOnePartition` | a split candidate whose halves would cross partitions is demoted through `Flux_DemoteSplit` in step 7 (an ordinary barrier in the consumer's batch, one `FLUX_RG_INFO_SPLIT_DEMOTED` record naming the two passes); a candidate whose halves fall in different batches is demoted the same way in step 6 (row 6e); a candidate inside one partition stays split |
| `Test_GraphBackend_PerPassRecordingOrder` | recording statistics of a fixture pass that has a split-barrier end due before it, a barrier batch, breadcrumbs and timestamps on, a debug label, sits first in a merged scope and produces a split begin, list the calls in exactly the ten orders of RenderGraphBackend.md "Per-pass recording order" (split end, barrier batch outside any rendering scope, begin breadcrumb, begin timestamp, label, `BeginRendering`, body, `EndRendering` at the scope's last pass, end label, end timestamp and end breadcrumb, split begin after the scope ended); a pass inside the scope writes its timestamps and breadcrumbs inside it and records no barrier there |
| `Test_RenderGraph_SubresourceMipChain`, `Test_RenderGraph_TimestampsPerPass` | RPROD's (`Zenith2/Docs/Design/RPROD/Tests.md → "Graph unit tests"`), listed for completeness and run by this area's gate step with its own: one barrier per mip transition of a bloom-style chain, and two queries per live pass |
| `Test_GraphBackend_PartitionCapRaisesTarget` | a frame needing more than 64 partitions raises the target and counts it |
| `Test_GraphBackend_ReachabilityIncludesWaits` | reachability rows match a brute-force closure on random graphs |
| `Test_GraphBackend_ToRHIBarrierTable` | every `Flux_ResourceAccess` maps to the neutral row of RenderGraphBackend.md, and the backend mappings give Rendering's Vulkan and D3D12 columns |
| `Test_GraphBackend_BufferBarriersFoldIntoGlobal` | non-transfer buffer barriers become one global barrier ([Z2-N-REND-390]) |
| `Test_RenderGraph_Error_SwapchainQueue` | a pass on async compute that writes the swapchain image fails with `FLUX_RG_ERROR_SWAPCHAIN_QUEUE` |
| `Test_GraphBackend_EntriesReportThroughDiagnostics` | RPROD's compile driver calls `Flux_GraphBuildQueuePlan`, `Flux_GraphAssignQueues` and `Flux_GraphPartition` in the order of RenderGraphBackend.md "Ownership of the compile steps"; a fixture with a swapchain-queue error and a second, front-end error reports both in one compile (the 6d diagnostic in `GetDiagnostics()`, step 7 still run); with the general heap's fault injector failing every allocation, the three entries still succeed, allocating from the graph arena alone |
| `Test_ImportedState_CarriesStateAcrossFrames` | a texture left in `SHADER_READ` on async compute is transitioned from there next frame with an ownership transfer |
| `Test_ImportedState_UnregisteredImportAsserts` | `z2-death-assert` |
| `Test_ImportedState_RunsSplitAndMerge` | per-mip writes split runs and equal states merge |
| `Test_ImportedState_UnregisteredAtRetirement` | a released resource's entry stays until the resource retires and is gone, through `OnResourceRetired`, before its table slot is reused; a persistent texture released before its acquire never had an entry |
| `Test_ImportedState_ConcurrentLoaderCreationDuringBuild` | `z2-stress`: 8 asset loader jobs create and release persistent textures for 300 frames while frames build, record and submit; the overlap detector never fires, each resident texture has exactly one entry from its acquire frame until it retires, and no entry outlives its retirement |
| `Test_FrameDump_BackEndSections` | a dump carries `batches`, `device`, `memory.heapBudgets` and per-pass queue, partition and barriers |
| `Test_FrameDump_WrittenOffTheCriticalPath` | the dump job runs after compile without delaying submit |
| `Test_RenderGraphTestAccess_RawPathMatchesFieldTablePath` | a raw compute pass with raw accesses compiles to the same core schedule as the equivalent generated-params pass |
| `Test_RenderGraphTestAccess_CompileWithoutDevice` | steps 1 to 3 run with no device |

## Failure-injection tests

Every construction step of every factory has `Test_<Type>_Create_FailsAt<Step>_Unwinds` ([Z2-N-CONTRACT-341]), asserting the error, zero leaks on the test heap, no thread left running and no registration left:

| Type | Steps (entry document "Device creation sequence" for the device) |
|---|---|
| `RHIDevice` (per backend, `ZenithRHI_Conformance`; step 4 is the debug-utils messenger on Vulkan and the debug and DRED settings interfaces on D3D12, both real fallible calls) | `CaptureTool`, `Loader`, `Instance`, `Messenger`, `Adapter`, `Device`, `Queues`, `Allocator`, `ResourceTables`, `DescriptorHeap`, `PipelineLayout`, `NullDescriptors`, `Breadcrumbs` |
| `Swapchain` (hidden window) | `Surface`, `Swapchain`, `Images`, `SyncObjects` |
| `UploadRing` | `InitialChunk`, `OperationTable`, `RequestQueue` |
| `ReadbackPool` | `PoolBuffer`, `OperationTable` |
| `GPUScene` | `InstanceTable`, `PrimitiveTable`, `MaterialSlotTable`, `LightTable`, `SlotAllocators` |
| `PipelineCacheStore` | `ReadFile`, `CreateCache` (a failure leaves an empty cache, never a failed device) |

## Stress and sanitizer runs

- `Test_RHIRetirementQueue_ConcurrentReleaseRequests` (`z2-stress`): 8 threads request 100,000 releases while the submit thread drains; every entry retires once. It runs under `Sanitizers -Preset win-asan`; the `linux-tsan` host has no GPU library ([Z2-N-ARCH-173]), so the RHI's concurrency paths that need no device (the retirement queue, the descriptor lists, the resource table) are covered by device-free tests that also build for `linux-tsan` through a device-free fixture.
- `Test_RHIResourceTable_ConcurrentAllocateFree` (`z2-stress`): 8 threads allocate and free 1,000,000 records.
- `Test_ImportedState_ConcurrentLoaderCreationDuringBuild` (`z2-stress`, "Graph back-end tests"), under `Sanitizers -Preset win-asan`.
- The upload and readback cancel stresses above.

## Device steps

These are `needs-human` runs, recorded under `Zenith2/Docs/Research/` or as acceptance evidence:

| Step | Device | Evidence |
|---|---|---|
| S24 profile confirmation | S24 (Adreno 750), RTX 3070, lavapipe | `Zenith2/Docs/Research/RHI/S24Profile/<date>.md`: `VP_ZENITH2_baseline_android` and `_windows` pass ([Z2-N-REND-023]) |
| D-SLANG full probe | RTX, S24, WARP, lavapipe | the probe reports |
| half-precision check | S24, RTX 3070 | the probe report's half section |
| Android surface loss and restore | S24 | 50 background and foreground cycles recover the surface with no validation error ([Z2-N-REND-988], `Zenith2/Docs/Design/PLAT/Android.md` device-run row) |
| S24 presentation | S24 | the swapchain uses the surface transform, the rotated present shows the image upright in both orientations, and HDR10 falls back or presents as the panel supports, with no validation error |
| device-loss recovery on hardware | RTX 3070 (a driver-reset tool, TDR), S24 | the report JSON and recovered rendering (Stage 6 acceptance) |
| BackendParity RHI half on hardware | RTX 3070 | the conformance suite green on the hardware adapter on both backends; AMD hardware, where available to the human, records the D32F_S8 path |

## Benchmarks

`Zenith_Bench_Budgets_RHI` (`Zenith2/Benchmarks/Budgets/RHI/`, entry document "Measurement harness") with `Test_BudgetHarness_RHI_Smoke` in the `Test` step, and `Zenith_Bench_BindlessProbe` above.
