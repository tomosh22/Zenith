# RHI requirements — the render hardware interface and the render graph's GPU back end

## Purpose

This catalogue breaks the RHI inventory items of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" (heading "RHI RHI & render graph") into testable requirements `Z2-R-RHI-NNN`. The design that specifies each requirement is `Zenith2/Docs/Design/RHI.md` and its split files under `Zenith2/Docs/Design/RHI/`; the specification the design elaborates is `Zenith2/Docs/Rendering.md` with `Zenith2/Docs/Rendering/GpuResources.md`, `Zenith2/Docs/Rendering/RenderGraph.md` and `Zenith2/Docs/Rendering/ShaderSystem.md`. The stage of a requirement is the stage whose acceptance it serves; Stage 2 requirements that depend on no Stage 1 decision are pulled forward into the Stage 1 sprint (`Zenith2/Docs/Design/RHI.md` → "Stage 1 pull-forward and API-only slices"). "WARP and lavapipe" means the headless CI devices of `Zenith2/Docs/Design/RHI.md` → "Headless CI devices"; every GPU test named here runs with the validation of `Zenith2/Docs/Design/RHI.md` → "The sync-validation-clean rule".

## Requirements

### Z2-R-RHI-001 One timeline per physical queue

- **Statement:** Every physical queue has exactly one timeline (a Vulkan timeline semaphore or a D3D12 fence), every submission batch signals it with a strictly increasing value, and a queue class the device lacks aliases the graphics timeline.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConformance_TimelinesStrictlyIncrease` and `Test_RHIConformance_AbsentQueueAliasesGraphics` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHIQueue header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Submission and timelines"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Queues and fences"

### Z2-R-RHI-002 Last use recorded per batch

- **Statement:** The value a batch signals is recorded as the last use, on that queue, of every imported resource and every descriptor slot the frame accessed on that queue.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_ImportedState_CarriesStateAcrossFrames` and `Test_GPUScene_SlotReuseAcrossFramesInFlight` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Submission"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Imported-state table"

### Z2-R-RHI-003 Retirement after every queue completed

- **Statement:** A resource, descriptor slot, table slot, pool slot or upload chunk is retired only once every queue's completed value is at or past its last-use value on that queue, and is never reused before its retirement.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIRetirementQueue_RetiresOnlyWhenEveryQueueCompleted` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Retirement rule"; `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"

### Z2-R-RHI-004 Every satisfied entry retired at frame begin

- **Statement:** Retirement runs at the start of every frame on the render-submit thread, reads each queue's completed value once and retires every satisfied entry, not only the oldest; an entry never used on the GPU retires at the first call.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIRetirementQueue_RetiresEverySatisfiedEntry` and `Test_RHIRetirementQueue_NeverUsedRetiresNextFrame` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"

### Z2-R-RHI-005 Retirement ring capacity and full policy

- **Statement:** The retirement ring holds at most 65,536 entries; an enqueue into a full ring on the render-submit thread waits for the oldest entry's values, retires it first and increments `Flux.Lifetime.DeletionQueueFullWaits`; the ring publishes the queue counters `rhi.retirement.full`, `rhi.retirement.dropped` (always 0) and `rhi.retirement.high_water` every frame.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIRetirementQueue_FullWaitsOnSubmitThread` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI.md` → "Bounded queues and full policies"

### Z2-R-RHI-006 Release requests from any thread

- **Statement:** A release requested from any thread carries an RHI resource alone, optionally with the descriptor slot released with it, and goes into a bounded request queue whose capacity is the sum of the resource-table capacities, which a correct program cannot fill because each live resource is released once; the render-submit thread drains it after the current frame's submissions with the last-signalled value of every queue, freeing any carried descriptor slot with the same values and moving requests only while the retirement ring holds fewer than its capacity minus `uFLUX_RETIREMENT_DRAIN_HEADROOM` entries, never waiting, so a request retires only after every frame that could have referenced it.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIRetirementQueue_ReleaseRequestTakesDrainValues`, `Test_RHIRetirementQueue_DrainNeverWaits` and `Test_RHIRetirementQueue_ConcurrentReleaseRequests` in the `Test` gate step on WARP and lavapipe (win-debug, win-release) and under `Sanitizers -Preset win-asan`
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHILifetime header"

### Z2-R-RHI-007 GPU waits only on the render-submit thread

- **Statement:** Only the render-submit thread waits on a GPU timeline from the CPU, except the five permitted main-thread waits (the swapchain drain and the idle wait of a frames-in-flight change, the Android surface release, the first frame preparation of a recovered device, shutdown, and a full-ring retirement `Enqueue` at a render-idle point), each while no render subgraph is in flight; every wait carries the `rhi.gpuWaitTimeoutMs` timeout (default 2 s) and is followed by a device-status check on timeout, and no job or pass lambda waits on the GPU.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameLifetime_WaitTimeoutChecksLoss` in the `Test` gate step on WARP and lavapipe (win-debug, win-release); review of the node affinities
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"; `Zenith2/Docs/Design/RHI.md` → "Thread affinity per type"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device loss detection"; `Zenith2/Docs/Design/RHI.md` → "Permitted main-thread GPU waits"

### Z2-R-RHI-008 Binary semaphores only for presentation

- **Statement:** Swapchain acquire and present use binary semaphores on Vulkan and DXGI's own ordering on D3D12, and every other GPU-GPU and GPU-CPU dependency uses the queue timelines.
- **Covers:** Z2-I-RHI-01, Z2-I-RHI-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConformance_CrossQueueTransfer` and `Test_Swapchain_CreatesEveryOutput` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Submission and timelines"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Swapchain and presentation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Queues and fences"

### Z2-R-RHI-009 Resources destroyed only through retirement

- **Statement:** The public RHI surface has no immediate destroy: every resource is destroyed by the device's retirement queue after its retirement, and destroying the device reports every live handle of every kind.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_DeviceHandle_LiveHandlesReportedAtDestruction` in the `Test` gate step on WARP and lavapipe (win-debug, win-release); the ApiContractCheck gate step for the device contract block
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Rules across the API"; `Zenith2/Docs/Design/RHI/Memory.md` → "Device resource table"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header"

### Z2-R-RHI-010 Device handle tests

- **Statement:** Device handles carry the handle tests `Test_DeviceHandle_NullDoesNotResolve`, `Test_DeviceHandle_StaleAfterDestroy_DoesNotResolve`, `Test_DeviceHandle_ForeignOwner_DoesNotResolve`, `Test_DeviceHandle_GenerationExhaustion_RetiresSlot` and `Test_DeviceHandle_KindMismatchIsStale`, each passing in every configuration including `win-release`.
- **Covers:** Z2-I-RHI-01, Z2-I-RHI-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** the named tests in the `Test` gate step (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_DeviceHandle header"; `Zenith2/Docs/Design/RHI/Memory.md` → "Device resource table"; `Zenith2/Docs/Design/RHI/Tests.md` → "RHI unit tests"

### Z2-R-RHI-011 Descriptor slot reuse after retirement

- **Statement:** A freed descriptor slot enters its space's pending list with its per-queue last-use values and returns to the free list only after every queue passed them; `Free` runs only on the render-submit thread or at a render-idle point with final values that thread knows, and a slot released from any other thread (a persistent resource at reference count zero) travels with its resource's release request and is freed by the drain with the drain values ([Z2-N-REND-059]).
- **Covers:** Z2-I-RHI-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIDescriptorHeap_FreedSlotReusedAfterRetirement`, `Test_RHIDescriptorHeap_ReleasedWithResourceTakesDrainValues` and `Test_Binding_SlotRetirementAcrossFrames` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Descriptor heap and slot allocation"; `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHILifetime header"

### Z2-R-RHI-012 Full descriptor space refused

- **Statement:** Allocating from a full index space returns `ZENITH_RHI_ERROR_DESCRIPTOR_SPACE_FULL` (mapped to `FLUX_DESCRIPTOR_ERROR_SPACE_FULL`), increments `Flux.Descriptors.Exhausted`, and never evicts or reuses a live slot.
- **Covers:** Z2-I-RHI-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** Z2-B-RHI-009
- **Acceptance evidence:** `Test_RHIDescriptorHeap_FullSpaceRefused` and `Test_DescriptorAllocator_ExhaustionCounted` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Descriptor heap and slot allocation"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHIDescriptors header"

### Z2-R-RHI-013 Null substitute in slot zero

- **Statement:** Slot 0 of every index space holds the null substitute (a 1×1 `(1, 0, 1, 1)` texture or a 256-byte zero buffer), written at device creation and never allocated.
- **Covers:** Z2-I-RHI-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIDescriptorHeap_SlotZeroNeverAllocated` and `Test_Binding_NullSlotZero` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Descriptor heap and slot allocation"; `Zenith2/Docs/Design/RHI.md` → "Device creation sequence"

### Z2-R-RHI-014 Descriptor writes once per allocation

- **Statement:** A descriptor slot is written at most once between allocation and free, on the `Flux.BuildGraph` node after compile (the views of the frame's graph) or on the creating thread at resource creation, never inside a pass lambda or a `Flux.Record` job; Vulkan writes are serialised by `uLOCK_LEVEL_RHI_DESCRIPTOR_WRITE`.
- **Covers:** Z2-I-RHI-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIDescriptorHeap_DoubleWriteAsserts` (z2-death-assert) and `Test_RHIConformance_DescriptorUpdateAfterBind` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Descriptor heap and slot allocation"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Descriptor heap realisation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Descriptor heaps"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Recording"

### Z2-R-RHI-015 Heap capacities and device refusal

- **Statement:** The descriptor spaces are created with the capability tier's capacities (the proposed values until `Z2-B-RHI-009` is adopted, configurable through `rhi.heap.*`), and a device whose update-after-bind limits or shader-visible heap size are below them is refused with `ZENITH_RHI_ERROR_PROFILE_UNSUPPORTED`.
- **Covers:** Z2-I-RHI-02, Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013, Z2-D-054
- **Budgets:** Z2-B-RHI-009, Z2-B-RHI-002
- **Acceptance evidence:** `Test_DescriptorHeap_CapacitiesBelowProfileRefused` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHICapabilities header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Baseline profile check"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Baseline checks"

### Z2-R-RHI-016 One global layout bound per partition

- **Statement:** Each backend creates exactly one descriptor layout (one Vulkan set layout, set and pipeline layout; one D3D12 root signature and shader-visible heap pair, with CPU-only RTV and DSV heaps of 8,192 and 2,048 descriptors), and each command-list partition binds it once per bind point in its prologue.
- **Covers:** Z2-I-RHI-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** none
- **Acceptance evidence:** `Test_DescriptorHeap_GlobalSetBoundOncePerPartition` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Descriptor heap realisation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Root signature and command signatures"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Recording"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Descriptor heaps"

### Z2-R-RHI-017 Heap realisation after D-SLANG

- **Statement:** The Vulkan descriptor-set layout follows the alternative D-SLANG (open, Stage 1) rules, and until it is ruled engine code depends only on the 8-byte `Zenith_RHIDescriptorHandle`, the generated `Zenith_RHISRV<T>`/`Zenith_RHIUAV<T>` fields and the `Bindless.slang` accessors.
- **Covers:** Z2-I-RHI-02, Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-054, D-SLANG (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** the D-SLANG ruling entry in `Zenith2/Docs/DecisionLog.md` and `Test_BindlessProbe_HandleLayout` in the probe's smoke run
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Descriptor heap realisation"; `Zenith2/Docs/Design/RHI.md` → "Stage 1 pull-forward and API-only slices"

### Z2-R-RHI-018 Frames in flight configurable

- **Statement:** The number of GPU frames in flight is `r.framesInFlight`, 2 or 3 (default 2), set at device creation and changed only by `Zenith_RHIDevice::SetFramesInFlight` inside a swapchain recreation that drains every swapchain, waits every queue idle through `Zenith_RHIDevice::WaitIdle` (a CPU wait on each queue's last-signalled timeline value, never `vkDeviceWaitIdle`; pending X89), rebuilds the device and Flux per-slot objects and re-prepares the frame about to build on slot 0, the swapchain image count following the device's value; it is recorded in every evidence record.
- **Covers:** Z2-I-RHI-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RHI-008
- **Acceptance evidence:** `Test_FrameLifetime_FramesInFlightBound`, `Test_FrameLifetime_FramesInFlightChangeRecreates` (hidden window) and `Test_RHIDevice_SetFramesInFlightRebuildsSlots` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"; `Zenith2/Docs/Design/RHI.md` → "Changing the frames in flight"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Recreation state machine"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header"

### Z2-R-RHI-019 Frame build waits for its slot

- **Statement:** Frame N's graph build starts only after every queue's timeline reached the final value of frame N − framesInFlight, a wait performed on the render-submit thread at the tail of the previous frame's present.
- **Covers:** Z2-I-RHI-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RHI-008
- **Acceptance evidence:** `Test_FrameLifetime_FramesInFlightBound` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"

### Z2-R-RHI-020 Per-slot resources reused after completion

- **Statement:** A frame slot's command pools, split-barrier events, upload segment, timestamp pools and readback staging are reset only after that slot's previous frame completed on every queue.
- **Covers:** Z2-I-RHI-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameLifetime_SlotResourcesReusedAfterCompletion` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Command recording mapping"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Command recording mapping"

### Z2-R-RHI-021 Upload ring chunks

- **Statement:** The upload ring is a list of host-visible chunks of 16 MiB (or larger for a single larger reservation) in `UPLOAD` memory preferring device-local memory, each with one `BUFFER_SRV` descriptor, starting at 64 MiB on desktop and 32 MiB on mobile.
- **Covers:** Z2-I-RHI-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-054
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_UploadRing_GrowsToCapAndNoFurther` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals"; `Zenith2/Docs/Design/RHI/Memory.md` → "Memory classes and allocation"

### Z2-R-RHI-022 Upload stalls on the render-submit thread only

- **Statement:** The upload ring waits for space only on the render-submit thread, inside the frame preparation's reservation; `Allocate` from any frame thread is a lock-free bump that never waits and returns `FLUX_UPLOAD_ERROR_RESERVATION_EXHAUSTED` beyond the reservation.
- **Covers:** Z2-I-RHI-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** Z2-B-RHI-007
- **Acceptance evidence:** `Test_UploadRing_ReserveStallsOnSubmitThreadOnly` and `Test_UploadRing_AllocateNeverStalls` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals"

### Z2-R-RHI-023 Upload ring growth capped

- **Statement:** The ring grows by one chunk while below `Z2-B-RHI-006` and never beyond it; at the cap it waits for the oldest in-flight frame to retire.
- **Covers:** Z2-I-RHI-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_UploadRing_GrowsToCapAndNoFurther` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals"

### Z2-R-RHI-024 Compile-time reservation extension and frame deferral

- **Statement:** The render-submit thread predicts each frame's upload reservation as the maximum of the compile's exact need over the last 8 built frames × 1.25 + 1 MiB, clamped to the cap; the graph compile extends the reservation to its exact need without waiting, and a frame whose need cannot be met without a wait is deferred (not recorded, submitted or presented; no ring byte written; its GPU-scene deltas stay pending and its readbacks fail with `FLUX_READBACK_ERROR_FRAME_DEFERRED`) and the next frame preparation reserves exactly that need, counting `Flux.Upload.ReservationDeferredFrames`.
- **Covers:** Z2-I-RHI-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-007
- **Acceptance evidence:** `Test_UploadRing_DeferredFrameReservesExactNext`, `Test_UploadRing_PredictionRule` and `Test_GPUScene_DeferredFrameKeepsDeltas` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals"; `Zenith2/Docs/Design/RHI/Memory.md` → "Frame deferral"; `Zenith2/Docs/Design/RHI.md` → "Relationship to the specification"; `Zenith2/Docs/Design/RHI/Memory.md` → "Deferred frames and the pending store"

### Z2-R-RHI-025 Upload request queue

- **Statement:** Worker jobs upload through `RequestUpload`, a bounded queue of 4,096 requests with the reject policy, which returns an operation handle, never blocks the job, and reads the requester's source only until the operation reports `STAGED`.
- **Covers:** Z2-I-RHI-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UploadRing_RequestQueueFullRejects` and `Test_UploadRing_SourceNotReadAfterStaged` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Upload request state machine"; `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals"

### Z2-R-RHI-026 Split oversized splittable requests

- **Statement:** A splittable request larger than the space under the cap stages the granularity-aligned pieces that fit each frame until complete.
- **Covers:** Z2-I-RHI-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_UploadRing_SplittableRequestCrossesFrames` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Splitting algorithm"

### Z2-R-RHI-027 Refuse oversized unsplittable requests

- **Statement:** A non-splittable upload request larger than `Z2-B-RHI-006` fails with `FLUX_UPLOAD_ERROR_TOO_LARGE` (BR-REFUSE); a frame whose exact need exceeds the cap is a frame failure (Z2-R-RHI-154), never a deferral.
- **Covers:** Z2-I-RHI-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_UploadRing_UnsplittableOverCapRefused` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Upload request state machine"; `Zenith2/Docs/Design/RHI/Memory.md` → "Frame failures"

### Z2-R-RHI-028 Upload exhaustion telemetry

- **Statement:** Every upload-ring wait increments `Flux.Upload.Exhausted` and accumulates `Flux.Upload.StallMs`, both published to the on-screen profiler and telemetry.
- **Covers:** Z2-I-RHI-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** Z2-B-RHI-007
- **Acceptance evidence:** `Test_UploadRing_ExhaustionCounted` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Counters"; `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals"

### Z2-R-RHI-029 Upload ring shrink

- **Statement:** Chunks above the initial capacity that stay unused for 600 consecutive frames retire through the retirement queue, and under memory pressure as soon as they are free.
- **Covers:** Z2-I-RHI-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UploadRing_ShrinkAfter600Frames` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals"

### Z2-R-RHI-030 Draw-parameter bound enforced

- **Statement:** A raster lambda issuing more `BeginDrawParams` calls than its `ExpectedDraws` fails the frame in development builds with `FLUX_RECORD_CHECK_DRAW_PARAMS_EXCEEDED` and drops the excess draws in shipping builds.
- **Covers:** Z2-I-RHI-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UploadRing_DrawParamsBoundEnforced` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Upload ring internals"

### Z2-R-RHI-031 Upload cancellation

- **Statement:** `Cancel()` on a queued upload moves it to `CANCELLED` with nothing staged; after `STAGED` the copy completes and is discarded; 1,000 issue-then-cancel cycles at random stages leave no ring bytes, operations or destination regions.
- **Covers:** Z2-I-RHI-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_UploadRing_CancelQueuedReleasesNothingStaged` and `Test_UploadRing_CancelStress` (z2-stress) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Upload request state machine"

### Z2-R-RHI-032 Readbacks are operation handles

- **Statement:** A readback is a move-only operation handle owned by the requester that becomes `READY` after the timeline value of the frame that recorded its copy completed.
- **Covers:** Z2-I-RHI-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_Readback_ReadyAfterFence` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Readback state machine"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "RHI-owned ZenithFlux public headers"

### Z2-R-RHI-033 Readback data lifetime

- **Statement:** A ready readback's data span stays valid until the requester calls `Release()` or destroys the handle, and handles unreleased at renderer shutdown are logged as leaks with their request names.
- **Covers:** Z2-I-RHI-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Readback_DataValidUntilRelease` and `Test_Readback_LeakReportedAtShutdown` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Readback pool"

### Z2-R-RHI-034 Readback pool bounded

- **Statement:** Readback memory comes from one pool of `Z2-B-RHI-011` bytes; a request that does not fit returns `FLUX_READBACK_ERROR_POOL_FULL`, increments `Flux.Readback.Deferred` and is retried by its requester in a subsequent frame (BR-DEFER).
- **Covers:** Z2-I-RHI-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-011
- **Acceptance evidence:** `Test_Readback_PoolFullDefers` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Readback pool"

### Z2-R-RHI-035 Readbacks published at tick boundaries

- **Statement:** A readback whose result feeds simulation (editor picking) is published to simulation at a tick boundary in completion-ID order.
- **Covers:** Z2-I-RHI-06
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_Readback_PublishedAtTickBoundary` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Readback state machine"

### Z2-R-RHI-036 Readback cancellation and loss

- **Statement:** `Cancel()` moves a pending readback to `CANCELLED` and frees its range once the copy retires, device loss moves every pending readback to `FAILED`, a readback declared in a frame that submits nothing fails as Z2-R-RHI-155 states, and 1,000 issue-then-cancel cycles leave no pool bytes or operations.
- **Covers:** Z2-I-RHI-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Readback_CancelReleasesAfterRetire`, `Test_Readback_DeviceLossFails` and `Test_Readback_CancelStress` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Readback state machine"

### Z2-R-RHI-037 Imported-state table

- **Statement:** An imported resource's state, layout and owning queue per subresource run at the end of a frame are recorded in the imported-state table and the next frame's first access transitions from them; an import of an unregistered resource asserts.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_ImportedState_CarriesStateAcrossFrames` and `Test_ImportedState_UnregisteredImportAsserts` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Imported-state table"

### Z2-R-RHI-038 Initial states registered by creators

- **Statement:** Every resource that is imported (persistent render targets, histories, pools, tables, swapchain images, readback buffers, persistent read-only resources) is registered with its initial state by its creator on the render subgraph or at a render-idle point, a persistent resource created by an asset loader job being registered when it joins the acquire list; it is unregistered when it retires, through the retirement listener, before its table slot can be reused.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_ImportedState_UnregisteredAtRetirement` and `Test_RHIRetirementQueue_ListenerCalledBeforeSlotFree` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Imported-state table"; `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_DeferredDeletion"; `Zenith2/Docs/Design/RHI/Memory.md` → "Persistent resource creation and the acquire list"

### Z2-R-RHI-039 Persistent histories and subresource runs

- **Statement:** Persistent histories are tracked as two imported ping-pong entries, and texture state is tracked in runs over the mip-layer index that split on per-mip writes and merge when equal.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_ImportedState_RunsSplitAndMerge` and `Test_RenderGraph_SubresourceMipChain` (RPROD's, `Zenith2/Docs/Design/RPROD/Tests.md` → "Graph unit tests") in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Tracked resource kinds realised"

### Z2-R-RHI-040 Queue-family ownership transfers

- **Statement:** A texture accessed on two queues in a frame gets a release half on the producer queue and an acquire half on the consumer queue on Vulkan, and a layout transition on a queue type supporting both layouts on D3D12, each paired with a timeline wait; graph buffers and persistent read-only textures need no transfer.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_CrossQueueTransfer` and `Test_RHIConformance_CrossQueueTransfer` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Queue-family ownership transfers"

### Z2-R-RHI-041 Queue order never orders memory

- **Statement:** Every cross-queue dependency carries a timeline wait and the matching barrier; no memory ordering relies on queue submission order.
- **Covers:** Z2-I-RHI-07, Z2-I-RHI-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphBackend_BatchesCutAtCrossQueueEdges` with validation clean in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 6 queue assignment"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Queue-family ownership transfers"

### Z2-R-RHI-042 Neutral barrier vocabulary and one call per batch

- **Statement:** Synthesised barriers reach the backends as neutral batches that each backend records as exactly one `vkCmdPipelineBarrier2` or one `ID3D12GraphicsCommandList7::Barrier` call, non-transfer buffer barriers folded into one global barrier.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConformance_BarrierBatchOneCall` and `Test_GraphBackend_BufferBarriersFoldIntoGlobal` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIBarrier header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Barrier translation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Enhanced barrier translation"

### Z2-R-RHI-043 Barrier mapping tables

- **Statement:** `Flux_ToRHIAccess` and each backend's one mapping function together give exactly the Vulkan and D3D12 columns of the access-state table of the specification.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphBackend_ToRHIBarrierTable` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Neutral barrier translation"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Barrier translation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Enhanced barrier translation"

### Z2-R-RHI-044 Barriers reachable from the graph alone

- **Statement:** The command-list header is includable only by `ZenithFlux/Internal/**`, the backends and RHI tests, and the barrier header only by `ZenithFlux/Internal/Graph/**`, the backends and RHI tests, so no pass context or feature can record a barrier.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** the ForbiddenCheck gate step's rule F20 with a failing fixture
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Rules across the API"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHICommandList header"

### Z2-R-RHI-045 Split barriers

- **Statement:** A split barrier records its begin half with `vkCmdSetEvent2` or `D3D12_BARRIER_SYNC_SPLIT` after the producer's rendering scope and its end half before the consumer, both in one command list, and a split candidate whose halves would cross batches (step 6) or partitions (step 7) is demoted through RPROD's `Flux_DemoteSplit`, recorded as `FLUX_RG_INFO_SPLIT_DEMOTED`.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConformance_SplitBarrierHalves`, `Test_GraphBackend_SplitHalvesInOnePartition` and `Test_RenderGraph_SplitBarrierNotAcrossBatches` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 7 command-list partitions"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Barrier translation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Enhanced barrier translation"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 6 queue assignment"

### Z2-R-RHI-046 Aliasing discards realised

- **Statement:** A first-use texture barrier with the discard flag records `oldLayout = UNDEFINED` on Vulkan and `LAYOUT_UNDEFINED` with `D3D12_TEXTURE_BARRIER_FLAG_DISCARD` on D3D12, with a source scope covering every earlier occupant of the memory, and placed resources in one heap alias correctly.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** Z2-B-RHI-010
- **Acceptance evidence:** `Test_RHIConformance_PlacedResourcesAlias` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Tracked resource kinds realised"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Barrier translation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Enhanced barrier translation"

### Z2-R-RHI-047 GPU scene table layouts

- **Statement:** The four GPU scene tables hold records of exactly 128, 128, 32 and 64 bytes with the specification's offsets on both APIs, declared once as shared Slang structs.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-015
- **Budgets:** Z2-B-RPROD-001
- **Acceptance evidence:** `Test_GPUScene_RecordLayoutsMatchSlang` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_GPUScene"

### Z2-R-RHI-048 Tables imported write-restricted

- **Statement:** The tables are imported every frame as write-restricted external buffers, and a write declared by any pass other than the scatter and growth passes is `FLUX_RG_ERROR_TABLE_WRITE`.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_TableWriteRejected` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_GPUScene"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Imported-state table"

### Z2-R-RHI-049 Scatter upload with coalescing

- **Statement:** Every table change reaches the GPU through the frame's upload ring and the scatter pass, which runs first in the frame, with repeated writes to one slot coalesced so the last write wins.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_ScatterCoalescesSlot` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Scatter staging and coalescing"

### Z2-R-RHI-050 No scatter pass without changes

- **Statement:** A frame with no changed records adds no scatter pass.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_NoRecordsNoScatterPass` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_GPUScene"

### Z2-R-RHI-051 Table growth and full tables

- **Statement:** A table at capacity grows by a resize-and-copy pass to twice its capacity up to the maximum derived from `Z2-B-RPROD-001`, preserving every live record, and an allocation beyond the maximum fails with `FLUX_GPU_SCENE_ERROR_TABLE_FULL` without evicting a slot.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-RPROD-001
- **Acceptance evidence:** `Test_GPUScene_GrowPreservesContents` and `Test_GPUScene_TableFullRefusesWithoutEviction` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Growth"

### Z2-R-RHI-052 Initial table capacities

- **Statement:** The tables start at 262,144 instances, 65,536 primitives, 65,536 material slots and 4,096 lights on desktop and a quarter of each on mobile.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-RPROD-001
- **Acceptance evidence:** `Test_GPUScene_InitialCapacities` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_GPUScene"

### Z2-R-RHI-053 Table slot recycling after retirement

- **Statement:** A freed table or pool slot is recycled only after every frame that could have read it retired, so a reused slot never shows new content in an old frame.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_SlotReuseAcrossFramesInFlight` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Slot allocators"

### Z2-R-RHI-054 Persistent read-only resources

- **Statement:** Everything a table references is a persistent read-only resource in the steady read state, which no pass can bind as a UAV, attachment or writable import.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_PersistentReadOnlyWriteRejected` and its compile-fail fixture in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Persistent resource creation and the acquire list"

### Z2-R-RHI-055 Persistent uploads through the acquire list

- **Statement:** A persistent resource's initial contents are written on the copy queue and become visible through the one-time acquire list in the first frame after the copy completes, and tables reference it only after that frame.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_PersistentVisibleAfterAcquire` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Persistent resource creation and the acquire list"

### Z2-R-RHI-056 Pools with producer and consumer passes

- **Statement:** Dynamic content reaches consumers through pools their owners create with `Flux_GPUScene::CreatePool` (GEOM's skinned-vertex and procedural-geometry pools, VFX's particle pool) and through the VT pool, each written by a declared producer pass and read by declared consumers; RHI fixes no granule size or layout, and a double-buffered pool (the skinned pool, for velocity) gives each slot the same range in both halves.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_SkinnedSameFrameAllPasses`, `Test_GPUScene_ParticlesSimulateThenDraw`, `Test_GPUScene_ProceduralRegeneratedMidFrame` and `Test_GPUScene_HistoryResetZeroVelocity` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Pool allocation"

### Z2-R-RHI-057 Pool growth bounded by memory rows

- **Statement:** A pool region at capacity grows to twice its granules, up to the maximum its owner sets from its memory row, by one resize-and-copy that copies every region independently to its recomputed offset; a pool whose region is at its maximum refuses the allocation.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** Z2-B-GEOM-001, Z2-B-VFX-001
- **Acceptance evidence:** `Test_GPUScene_PoolGrowthBoundedByRow` and `Test_PoolAllocator_TwoRegionsGrowIndependently` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Pool allocation"

### Z2-R-RHI-058 VT tile upload then sample

- **Statement:** A VT tile uploaded in a frame is sampled correctly after its acquire in that frame.
- **Covers:** Z2-I-RHI-08
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-043
- **Budgets:** Z2-B-ADVGFX-001
- **Acceptance evidence:** `Test_GPUScene_VTTileUploadThenSample` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Pool allocation"

### Z2-R-RHI-059 Streaming swap

- **Statement:** Texture streaming uses reallocate-and-swap through `Flux_StreamingSwap`, which releases the old texture and its slot no earlier than the last frame its client reports could reference them, and two tables referencing one texture render correctly before, during and after a swap; the texture-swap transition reservation and the material-block rewrite belong to the client.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_SharedTextureDuringStreamingSwap` and `Test_GPUScene_SwapReleasesOldAfterClientFrame` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Streaming swap integration"

### Z2-R-RHI-060 Delta replay reproduces tables

- **Statement:** Replaying a recorded render-world delta stream from frame 0 gives tables bit-identical to the live run, including across deferred frames, and a full-state render world (`m_bFullState`, after device loss or a frame-executor cancel) gives the same tables.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_DeltaReplayReproducesTables`, `Test_GPUScene_FullStateWorldRebuilds` and `Test_GPUScene_DeferredFrameKeepsDeltas` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Flux_GPUScene"; `Zenith2/Docs/Design/RHI/Memory.md` → "Deferred frames and the pending store"

### Z2-R-RHI-061 One device allocator per API

- **Statement:** Vulkan device memory is allocated through VMA alone (with the memory-budget flag when available) and D3D12 memory through D3D12MA alone, with host allocations of both routed to the general heap.
- **Covers:** Z2-I-RHI-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** review of `Internal/`; `Test_RHIConformance_CopyBufferRoundTrip` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Memory through VMA"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Memory through D3D12MA"

### Z2-R-RHI-062 Memory classes

- **Statement:** Every resource is created in one of the four memory classes (device local, device dedicated, upload, readback) mapped per backend as specified, and buffers carry the union of their declared uses.
- **Covers:** Z2-I-RHI-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConformance_TextureUploadEveryFormat` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Memory classes and allocation"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHITypes header"

### Z2-R-RHI-063 Device resource tables

- **Statement:** Each resource kind has a paged generational table with its capacity; creation at capacity returns `ZENITH_RHI_ERROR_TABLE_FULL` and resolution never asserts.
- **Covers:** Z2-I-RHI-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIResourceTable_FullReturnsTableFull`, `Test_RHIResourceTable_FreedSlotReusedLast` and `Test_RHIResourceTable_ConcurrentAllocateFree` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Device resource table"

### Z2-R-RHI-064 Requirements before creation and placed resources

- **Statement:** Memory requirements (size, alignment, placement group) are queried for a desc without creating the resource, and placed textures and buffers are created in heaps at given offsets through VMA aliasing or D3D12MA aliasing resources.
- **Covers:** Z2-I-RHI-09, Z2-I-RHI-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** Z2-B-RHI-010
- **Acceptance evidence:** `Test_RHIConformance_PlacedResourcesAlias` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Heap alignment and placement"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Memory through VMA"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Memory through D3D12MA"

### Z2-R-RHI-065 Budget query and category report

- **Statement:** Every frame the renderer reads each device heap's budget and usage and reports GPU memory to the memory governor in the eight categories, the rendering consumer's resident bytes being their sum.
- **Covers:** Z2-I-RHI-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-001, Z2-B-RHI-002, Z2-B-RHI-010
- **Acceptance evidence:** `Test_Memory_CategoriesReportedToGovernor` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Residency reporting"

### Z2-R-RHI-066 Per-heap residency cap

- **Statement:** A device-local heap whose usage exceeds its budget (the API's, or `Z2-B-RHI-003` once adopted, whichever is lower) reports warning pressure, and above 95% of it critical pressure.
- **Covers:** Z2-I-RHI-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-003
- **Acceptance evidence:** `Test_Memory_PerHeapCapRaisesPressure` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Residency reporting"

### Z2-R-RHI-067 Device-local usage to the governor

- **Statement:** The RHI's device-local usage is reported to the memory governor every frame prologue, so VRAM thresholds drive the pressure level on Windows.
- **Covers:** Z2-I-RHI-10
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-CORE-003
- **Acceptance evidence:** `Test_Memory_CategoriesReportedToGovernor` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Residency reporting"

### Z2-R-RHI-068 Placement rules per API

- **Statement:** Every placed resource respects its reported alignment and placement group (Vulkan memory type with linear and optimal separated; D3D12 64 KiB or 4 KiB with the three heap categories on every tier), and a violation is `ZENITH_RHI_ERROR_INVALID_DESC`.
- **Covers:** Z2-I-RHI-11
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_Memory_PlacementAlignmentRules` and `Test_RHIConformance_PlacementGroupMismatchRejected` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Heap alignment and placement"

### Z2-R-RHI-069 Rendering memory consumer

- **Statement:** The renderer registers one memory consumer for the rendering subsystem, which applies the thirteen shedding actions of the Rendering row within one frame of a level change and reverts them one frame after the level falls.
- **Covers:** Z2-I-RHI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-001, Z2-B-RHI-010
- **Acceptance evidence:** `Test_Memory_WarningShedding` and `Test_Memory_CriticalShedding` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Memory pressure responder"

### Z2-R-RHI-070 Warning-level shedding

- **Statement:** At warning pressure actions 1 to 6 of the Rendering shedding row take effect.
- **Covers:** Z2-I-RHI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Memory_WarningShedding` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Memory pressure responder"

### Z2-R-RHI-071 Critical-level shedding

- **Statement:** At critical pressure actions 1 to 13 of the Rendering shedding row take effect, including the refusal of new persistent render targets with `FLUX_RESOURCE_ERROR_MEMORY_PRESSURE`.
- **Covers:** Z2-I-RHI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-SHIP-011
- **Acceptance evidence:** `Test_Memory_CriticalShedding` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Memory pressure responder"

### Z2-R-RHI-072 Device memory exhaustion

- **Statement:** A device-heap allocation failure reports critical pressure, leaves a persistent asset non-resident or, after one release-and-retry, fails the frame (`FLUX_FRAME_FAILURE_DEVICE_OOM`: nothing recorded or presented, the pending store kept), and 30 consecutive failed frames take the frame-failure exit of Z2-R-RHI-160 with the message `The graphics device ran out of memory. Your progress has been saved.` and `EXIT_CODE_DEVICE_LOST` (6), writing no device-loss report.
- **Covers:** Z2-I-RHI-12
- **Stage:** S4
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Memory_DeviceOOMSkipsFrameThenRecovers`, `Test_Memory_ThirtyOOMFramesExitCleanly` and `Test_Memory_PersistentOOMLeavesAssetNonResident` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Device memory exhaustion"; `Zenith2/Docs/Design/RHI/Memory.md` → "Frame failures"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine"

### Z2-R-RHI-073 Swapchain outputs

- **Statement:** The swapchain is created in SDR sRGB, HDR10 ST.2084 or scRGB per `rhi.output`, falling back to SDR with a warning when the surface lacks the pair, and every output format is in the boot PSO group of every pass that writes it; every creation and recreation reports which output colour spaces the surface supports (`m_uOutputColourSpacesSupported`, SDR always set).
- **Covers:** Z2-I-RHI-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Swapchain_CreatesEveryOutput` (hidden window) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Present modes and formats"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Swapchain and presentation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Swapchain and presentation"; `Zenith2/Docs/Design/RHI.md` → "CI windows for GPU tests"

### Z2-R-RHI-074 Present modes

- **Statement:** `rhi.presentMode` selects FIFO, FIFO relaxed, mailbox or immediate, an unsupported mode falls back and the mode in use is reported through the swapchain info.
- **Covers:** Z2-I-RHI-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Swapchain_PresentModeFallbackReported` (hidden window) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Present modes and formats"; `Zenith2/Docs/Design/RHI.md` → "CI windows for GPU tests"

### Z2-R-RHI-075 Acquire and present

- **Statement:** Acquire runs in `Flux.Submit` and present in `Flux.Present` on the render-submit thread; an out-of-date, lost-surface or timed-out acquire renders the frame into a stand-in, skips the present and marks the swapchain for recreation.
- **Covers:** Z2-I-RHI-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Swapchain_OutOfDateRendersStandIn` (hidden window) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Acquire and present"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Submission"; `Zenith2/Docs/Design/RHI.md` → "CI windows for GPU tests"

### Z2-R-RHI-076 Frame-pacing hooks

- **Statement:** The swapchain provides present waits, the latency waitable slot, present timings on the monotonic clock, the maximum frame latency, a desired present time (`SetDesiredPresentTime`: `VkPresentTimeGOOGLE` with `VK_GOOGLE_display_timing`, else `FEATURE_UNAVAILABLE`) and a swap interval (`SetSwapInterval`, 1 to 4: the D3D12 FIFO sync interval, recorded without effect on Vulkan), on the render-submit thread, for RPROD's pacing controller, and declares the HDR metadata call `SetHDRMetadata`, whose realisation and acceptance are Z2-R-RHI-162.
- **Covers:** Z2-I-RHI-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RPROD-020
- **Acceptance evidence:** `Test_Swapchain_PacingHooks` (hidden window) in the `Test` gate step on WARP and lavapipe (win-debug, win-release); RPROD's pacing tests
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Frame pacing hooks"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHISwapchain header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Swapchain and presentation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Swapchain and presentation"

### Z2-R-RHI-077 Zero-sized output

- **Statement:** A minimised or zero-sized window produces no frames and records no GPU work until it has a size again.
- **Covers:** Z2-I-RHI-13, Z2-I-RHI-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Swapchain_ZeroSizeProducesNoFrames` (hidden window) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Recreation state machine"; `Zenith2/Docs/Design/RHI.md` → "CI windows for GPU tests"

### Z2-R-RHI-078 One swapchain per rendering window

- **Statement:** Each rendering window (up to 16 in the editor) has its own swapchain, recreated by its own window's events alone.
- **Covers:** Z2-I-RHI-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Swapchain_OneSwapchainPerWindow` (two hidden windows) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Swapchain model"

### Z2-R-RHI-079 Clip-space convention

- **Statement:** Clip space is +Y up with depth [0, 1] on both APIs, the Vulkan backend flipping the viewport with a negative height.
- **Covers:** Z2-I-RHI-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-061
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConformance_Y_UpClipSpace` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Command recording mapping"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Swapchain model"

### Z2-R-RHI-080 Android pre-rotation

- **Statement:** On Android the swapchain uses the surface's current transform and the present pass rotates the final image by the reported transform.
- **Covers:** Z2-I-RHI-13
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** the S24 presentation device step in `Zenith2/Docs/Design/RHI/Tests.md` "Device steps"
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Swapchain model"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Android specifics"

### Z2-R-RHI-081 Vulkan baseline profile

- **Statement:** The Vulkan backend creates its device through Vulkan-Profiles against `VP_ZENITH2_baseline` (with `shaderFloat16` and the per-platform capability blocks) and refuses every device the profile rejects.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_VulkanProfile_LavapipePasses` and `Test_VulkanProfile_GeneratedMatchesJson` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Baseline profile check"

### Z2-R-RHI-082 Exact refusal message

- **Statement:** A refused device produces the exact refusal line of its backend listing every missing feature, extension, limit, format and version (Vulkan) or every failed baseline row (D3D12) in table order, logged and copied into the device report, and startup exits with `EXIT_CODE_DEVICE` (4) printing it; in development builds the device report is also written as `rhi_report.json` (`schemaVersion` 1: backend, adapter, driver, tier, every capability, the refusal lines) after startup step 12.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_VulkanProfile_RefusalListsEveryMissingItem`, `Test_D3D12Baseline_RefusalListsEveryFailedRow` and `Test_RHIDevice_ReportJsonWritten` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Refusal message"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Baseline checks"; `Zenith2/Docs/Design/RHI.md` → "Runtime file formats"

### Z2-R-RHI-083 D3D12 baseline

- **Statement:** The D3D12 backend refuses an adapter lacking Shader Model 6.6, binding tier 3, Enhanced Barriers, native 16-bit ops, wave ops, the typed UAV loads, `WriteBufferImmediate` or the pinned Agility SDK.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_D3D12Baseline_WarpPasses` and `Test_D3D12Baseline_RefusalListsEveryFailedRow` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/D3D12.md` → "Baseline checks"

### Z2-R-RHI-084 Format-feature table

- **Statement:** `FormatFeatures.table` holds the specification's required format rows (with the `platform=` qualifier for BC7 and ASTC), generates `Zenith_FormatFeatures.h` at build time, fails the build on a malformed line, and equals the profile's formats section.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_FormatFeatures_TableMatchesRendering`, `Test_FormatFeatures_GeneratorRejectsMalformedLine` and `Test_FormatFeatures_ProfileMatchesHeader` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_FormatFeatures header"

### Z2-R-RHI-085 Format features checked on both backends

- **Statement:** Both backends check every non-optional format row for the running platform at device creation and refuse a device lacking a letter.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_FormatFeatures_DeviceRefusesMissingFeature` (Vulkan and D3D12) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Baseline profile check"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Baseline checks"

### Z2-R-RHI-086 Capabilities filled once

- **Statement:** `Zenith_RHICapabilities` is filled once at device creation and is the only source of device properties for Flux.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConformance_CreateDeviceOnSoftwareAdapter` in the `Test` gate step on WARP and lavapipe (win-debug, win-release); review
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHICapabilities header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Physical device selection"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Baseline checks"

### Z2-R-RHI-087 Capability tier and D24S8 fallback

- **Statement:** The capability tier is mobile on Android and desktop on Windows, and `m_bDepthD24S8` reports whether D24S8 is usable so the device-wide default depth format falls back to D32F_S8.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-061
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConformance_CreateDeviceOnSoftwareAdapter` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHICapabilities header"

### Z2-R-RHI-088 Runtime backend choice

- **Statement:** Windows games link both backends and choose one at startup from `rhi.backend`; Android games link Vulkan alone; a `d3d12` value there is refused with a logged warning and Vulkan runs; the factory pointer type is `Zenith_PfnCreateDevice`.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConfig_KeysAndDefaults` in the `Test` gate step on WARP and lavapipe (win-debug, win-release); MOD's composition tests
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Device factories and the factory pointer type"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIConfig header"

### Z2-R-RHI-089 Adapter selection

- **Statement:** Adapter selection discards baseline failures, honours the software preference and the name filter, orders by adapter type and device-local memory, and returns `ZENITH_RHI_ERROR_NO_ADAPTER` when nothing remains.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConformance_CreateDeviceOnSoftwareAdapter` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Physical device selection"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Adapter selection"

### Z2-R-RHI-090 volk dispatch

- **Statement:** Every Vulkan entry point is loaded through volk from the platform's loader, and every device-level call goes through the device's own volk table.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-026
- **Budgets:** none
- **Acceptance evidence:** `Test_Vulkan_DeviceDispatchThroughTable` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Logical device and volk"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Instance creation"

### Z2-R-RHI-091 Agility SDK exports and version check

- **Statement:** `Zenith_RHI_D3D12.h` carries the Agility SDK version and path constants that equal the manifest pin, games export them, and a device reporting another SDK version fails creation with `ZENITH_RHI_ERROR_LOADER_UNAVAILABLE`.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Test_D3D12_AgilityConstantsMatchManifest` and `Test_D3D12_AgilityRuntimeLoaded` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Backend factory headers"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Agility SDK loading"

### Z2-R-RHI-092 Third-party headers confined

- **Statement:** No public header of the three RHI libraries includes a Vulkan, D3D12, DXGI, volk, VMA, D3D12MA, Vulkan-Profiles or RenderDoc header, and consumers other than the composition root and test, benchmark and tool executables include only the API library.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-082, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the Libgraph gate step (leakage and swap-contract report for the RHI family) and `Test_Vulkan_NoThirdPartyInPublicHeaders`
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Rules across the API"; `Zenith2/Docs/Design/RHI.md` → "Libraries"

### Z2-R-RHI-093 Device creation unwinds every step

- **Statement:** A device desc outside its domain (a null platform or heap, frames in flight outside 2 to 3, a heap capacity outside 1,024 to 1,048,576, a wait timeout outside 100 to 60,000 ms, a capture tool in Shipping) is refused with `INVALID_DESC` before any API call; each of the thirteen device-creation steps (step 4 being the debug-utils messenger on Vulkan and the debug and DRED settings interfaces on D3D12) unwinds every earlier step on failure, and each has a failure-injection test asserting the error, zero leaks and no leftover thread or registration.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIDevice_DescValidation` and the `Test_RHIDevice_Create_FailsAt<Step>_Unwinds` tests of both backends in `ZenithRHI_Conformance`, the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Device creation sequence"; `Zenith2/Docs/Design/RHI/Tests.md` → "Failure-injection tests"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header"

### Z2-R-RHI-094 S24 profile confirmation

- **Statement:** The Stage 1 S24 profile ticket confirms `VP_ZENITH2_baseline` on the Adreno 750, lavapipe and the RTX 3070, a missing requirement being filed as a flagged proposal rather than weakening the profile.
- **Covers:** Z2-I-RHI-14
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-013, Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** `Zenith2/Docs/Research/RHI/S24Profile/<date>.md` committed by the needs-human device step
- **Design:** `Zenith2/Docs/Design/RHI/Tests.md` → "Device steps"; `Zenith2/Docs/Design/RHI.md` → "Stage 1"

### Z2-R-RHI-095 Half-precision check

- **Statement:** `Test_HalfPrecision_BitExact` compares half add, multiply, fused multiply-add, conversion and storage bit for bit against a CPU oracle on WARP and lavapipe in CI and on the S24 and the RTX 3070 by the device step, and half-precision shaders do not enter CI until it passes on both CI devices.
- **Covers:** Z2-I-RHI-14
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-013
- **Budgets:** none
- **Acceptance evidence:** the probe's smoke run in the `Test` gate step and its full report under `Zenith2/Docs/Research/DSlang/`
- **Design:** `Zenith2/Docs/Design/RHI/Tests.md` → "Half-precision check"

### Z2-R-RHI-096 Headless CI devices

- **Statement:** Every GPU test runs on WARP and on lavapipe, selected through `rhi.adapter=software`, and records the skip reason of any capability-dependent case it cannot run; a test that needs a swapchain first creates a hidden window through `Zenith_Window` (lavapipe presenting through its Win32 WSI), and every other GPU test creates a device with no window.
- **Covers:** Z2-I-RHI-14, Z2-I-RHI-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-008
- **Budgets:** none
- **Acceptance evidence:** the JUnit reports of the `Test` gate step listing both devices, and `Test_Swapchain_HiddenWindowPresents` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Headless CI devices"; `Zenith2/Docs/Design/RHI.md` → "CI windows for GPU tests"

### Z2-R-RHI-097 Capability fallbacks tested

- **Statement:** An optional capability absent on WARP or lavapipe (mesh shaders, present wait, calibrated timestamps) is exercised through its fallback in CI.
- **Covers:** Z2-I-RHI-14
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the mesh-pipeline fallback cases of the visibility-buffer tests in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Headless CI devices"; `Zenith2/Docs/Design/RHI.md` → "Stage 5"

### Z2-R-RHI-098 Mesh pipelines

- **Statement:** Mesh pipelines and indirect mesh dispatch are created when mesh shaders are supported and return `ZENITH_RHI_ERROR_FEATURE_UNAVAILABLE` otherwise.
- **Covers:** Z2-I-RHI-14
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the visibility-buffer path's mesh-shader and fallback tests in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHIPipelines header"; `Zenith2/Docs/Design/RHI.md` → "Stage 5"

### Z2-R-RHI-099 Bindless probe harness

- **Statement:** `Zenith_Bench_BindlessProbe` implements every probe case of Decision D-SLANG for each lowering alternative on Vulkan and D3D12, with a CTest smoke run on WARP and lavapipe.
- **Covers:** Z2-I-RHI-14, Z2-I-RHI-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, D-SLANG (open, Stage 1)
- **Budgets:** none
- **Acceptance evidence:** the probe's smoke test in the `Test` gate step
- **Design:** `Zenith2/Docs/Design/RHI/Tests.md` → "Bindless probe"

### Z2-R-RHI-100 D-SLANG ruling

- **Statement:** The D-SLANG ruling ticket runs the full probe on an RTX GPU, the S24, WARP and lavapipe, commits the reports, and appends the ruling (Slang version and alternative) to `Zenith2/Docs/DecisionLog.md` with an explicit `Z2-D-NNN`.
- **Covers:** Z2-I-RHI-02, Z2-I-RHI-14
- **Stage:** S1
- **Classification:** research
- **Decisions:** Z2-D-084, Z2-D-097, D-SLANG (open, Stage 1)
- **Budgets:** Z2-B-RHI-009
- **Acceptance evidence:** the probe reports under `Zenith2/Docs/Research/DSlang/` and the DecisionLog entry, checked by the coverage gate step in acceptance mode for Stage 1
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Stage 1"; `Zenith2/Docs/Design/RHI.md` → "Open decisions"

### Z2-R-RHI-101 Validation modes

- **Statement:** `rhi.validation` enables the Khronos layer or the D3D12 debug layer (`standard`), with synchronization validation or synchronized queue validation (`sync`), or GPU-based validation (`gpu`), and Shipping loads no layer.
- **Covers:** Z2-I-RHI-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_Validation_SinkReceivesMessages` and `Test_Validation_ShippingIgnoresKey` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Validation layers and the clean requirement"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Instance creation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Debug layer and validation"

### Z2-R-RHI-102 Validation clean in every GPU test

- **Statement:** Every GPU test runs with Vulkan synchronization validation or D3D12 GPU-based validation and fails on any unsuppressed validation warning or error.
- **Covers:** Z2-I-RHI-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_Validation_TestSinkFailsOnMessage` and `Test_Validation_SyncValidationCatchesMissingBarrier` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "The sync-validation-clean rule"; `Zenith2/Docs/Design/RHI.md` → "Validation layers and the clean requirement"

### Z2-R-RHI-103 Suppressions need decisions

- **Statement:** A validation suppression exists only in `ValidationSuppressions.json` with one message ID and a `Z2-D-` decision per entry, and an entry without a decision fails loading.
- **Covers:** Z2-I-RHI-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_Validation_SuppressionNeedsDecision` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Committed data files"

### Z2-R-RHI-104 Validation sink

- **Statement:** Validation messages from layers and driver threads reach the `Zenith_RHIValidationSink` given in the device desc, the runtime's sink logging them in `rhi.validation` and the test sink failing the test.
- **Covers:** Z2-I-RHI-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Validation_SinkReceivesMessages` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDebug header"

### Z2-R-RHI-105 Missing layer handling

- **Statement:** A requested layer that is absent degrades a development run to `off` with a warning and fails a test executable at start-up.
- **Covers:** Z2-I-RHI-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Validation_ModeDegradesWithoutLayer` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Validation layers and the clean requirement"

### Z2-R-RHI-106 Debug names and labels

- **Statement:** With `rhi.debugNames` resources, pipelines, queues and command buffers are named and every pass, partition and batch is a debug label, and Shipping records none.
- **Covers:** Z2-I-RHI-15, Z2-I-RHI-17
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Debug_NamesAndLabels` in the `Test` gate step on WARP and lavapipe (win-debug, win-release); review of captures in development
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Debug utils and validation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Command recording mapping"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Recording"

### Z2-R-RHI-107 Frame dump back-end sections

- **Statement:** Every frame dump carries the back-end keys (per-pass queue, partition and barriers, batches, device and heap budgets), written on a background job in development builds and never in Shipping.
- **Covers:** Z2-I-RHI-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameDump_BackEndSections` and `Test_FrameDump_WrittenOffTheCriticalPath` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Frame dump back-end sections"

### Z2-R-RHI-108 Last frames kept for device loss

- **Statement:** Development builds keep the last three frames' dump models so a device-loss report can write them.
- **Covers:** Z2-I-RHI-16, Z2-I-RHI-19
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Recovery_ReportJsonSchema` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Frame dump back-end sections"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss report"

### Z2-R-RHI-109 Graph conformance test access

- **Statement:** `Flux_RenderGraphTestAccess` provides raw compute passes, raw buffer accesses, raw orderings, a device-free compile of steps 1 to 3 and core node indices, built on the production declaration path.
- **Covers:** Z2-I-RHI-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraphTestAccess_RawPathMatchesFieldTablePath`, `Test_RenderGraphTestAccess_CompileWithoutDevice` and the Flux builder rows of `ZenithGraph_Conformance` in the `Test` gate step
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Flux_RenderGraphTestAccess"

### Z2-R-RHI-110 Timestamp pools and calibration

- **Statement:** The RHI provides per-queue timestamp pools per frame slot (at most 8,192 queries), their period, resolution and host reset, and CPU-GPU clock calibration, for per-pass GPU times on the on-screen profiler; each submitted frame's ticks are read and handed to RPROD's `Flux_PassTimingPublisher::Publish` once per queue, at the preparation that reuses the frame's slot or, for the old slots, before a frames-in-flight change retires their pools, and the pools are reset only after the read.
- **Covers:** Z2-I-RHI-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConformance_TimestampsMonotonic` and `Test_FrameLifetime_TimestampsPublishedAtSlotReuse` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIQueries header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Timestamps and calibration"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Timestamps and calibration"; `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"

### Z2-R-RHI-111 RenderDoc capture

- **Statement:** With `rhi.capture=renderdoc` in a development build both backends load the RenderDoc in-application API before device creation and bracket requested frames with frame captures; a missing library leaves captures unavailable with a log line.
- **Covers:** Z2-I-RHI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Capture_UnavailableWithoutTool` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "RenderDoc capture on Vulkan"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Captures"

### Z2-R-RHI-112 PIX capture

- **Statement:** With `rhi.capture=pix` in a development build the D3D12 backend loads the PIX GPU capturer before the first D3D12 call and brackets requested frames through `IDXGraphicsAnalysis`.
- **Covers:** Z2-I-RHI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Capture_UnavailableWithoutTool` PIX case in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/D3D12.md` → "PIX and RenderDoc capture"

### Z2-R-RHI-113 Capture trigger and shipping exclusion

- **Statement:** `r.capture <frames>` and the profiler button capture 1 to 8 whole frames, and Shipping builds load no capture library and expose no capture command.
- **Covers:** Z2-I-RHI-17
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** review of the Shipping link closure and `Test_Capture_UnavailableWithoutTool` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Captures"

### Z2-R-RHI-114 Provisional queue and gap rule

- **Statement:** An async-flagged pass runs on the async-compute queue only when async compute is enabled, a dedicated queue exists and a graphics pass lies strictly between its producers and consumers; copy-flagged passes use the copy queue when one exists.
- **Covers:** Z2-I-RHI-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_AsyncGapRule` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 3 queues lifetimes and reachability"

### Z2-R-RHI-115 Lifetimes and reachability

- **Statement:** Each resource's lifetime is the set of its live accesses' (queue, position) pairs and its global interval, and each pass's reachability row holds exactly the passes ordered before it by graph edges or queue order.
- **Covers:** Z2-I-RHI-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-057
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphBackend_ReachabilityIncludesWaits` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 3 queues lifetimes and reachability"

### Z2-R-RHI-116 Submission batches and waits

- **Statement:** Each queue's passes are cut into batches at every cross-queue dependency, each cross-queue edge becomes a signal and a wait reduced to one wait per source queue, and batches are submitted in an order respecting every wait.
- **Covers:** Z2-I-RHI-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphBackend_BatchesCutAtCrossQueueEdges` and `Test_GraphBackend_SubmitOrderRespectsWaits` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 6 queue assignment"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Submission"

### Z2-R-RHI-117 Command-list partitions

- **Statement:** Each queue's passes are partitioned into command lists of at most 4,096 cost units without splitting a merged scope or a batch, at most 64 per queue (the target raised and counted beyond), each recorded by its own job.
- **Covers:** Z2-I-RHI-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** Z2-B-RHI-005
- **Acceptance evidence:** `Test_GraphBackend_PartitionTargetCost` and `Test_GraphBackend_PartitionCapRaisesTarget` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 7 command-list partitions"

### Z2-R-RHI-118 Parallel recording equals serial

- **Statement:** Partitions are recorded by parallel jobs that share no mutable state, and parallel and serial recording produce identical images.
- **Covers:** Z2-I-RHI-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** Z2-B-RHI-005
- **Acceptance evidence:** `Test_RenderGraph_ParallelRecordMatchesSerial` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Parallel recording jobs"

### Z2-R-RHI-119 Per-pass recording order

- **Statement:** Each pass is recorded as its split-barrier ends, its barrier batch outside any rendering scope, its begin breadcrumb and timestamp, its body inside its merged scope, its end timestamp and breadcrumb, then its split-barrier begins.
- **Covers:** Z2-I-RHI-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_GraphBackend_PerPassRecordingOrder` (the ten-step order), `Test_RenderGraph_TimestampsPerPass` (RPROD's, `Zenith2/Docs/Design/RPROD/Tests.md` → "Graph unit tests") and `Test_Recovery_BreadcrumbsNamePasses` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Per-pass recording order"

### Z2-R-RHI-120 Swapchain writes on graphics only

- **Statement:** A pass on a queue other than graphics that writes the swapchain image is the graph compile error `FLUX_RG_ERROR_SWAPCHAIN_QUEUE`.
- **Covers:** Z2-I-RHI-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RenderGraph_Error_SwapchainQueue` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Queue-family ownership transfers"

### Z2-R-RHI-121 GPU breadcrumbs

- **Statement:** Every queue carries per-pass begin and end breadcrumbs (AMD buffer markers, NV checkpoints or fills on Vulkan; `WriteBufferImmediate` on D3D12) from which the last completed and first incomplete pass of each queue are read after loss; a queue-frame of more than 4,096 passes wraps its index at 4,095 and counts `RHI.Breadcrumbs.Overflow`.
- **Covers:** Z2-I-RHI-19, Z2-I-RHI-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Recovery_BreadcrumbsNamePasses` and `Test_Recovery_BreadcrumbOverflowCounted` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Breadcrumbs"

### Z2-R-RHI-122 Device-loss report

- **Statement:** On device loss a `zenith2.device-loss` report naming per queue the last completed and first incomplete pass, the frame, the driver, the device-fault or DRED data and the recovery outcome is written from a reserved arena and attached to the crash report.
- **Covers:** Z2-I-RHI-19
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Recovery_ReportJsonSchema` and `Test_Recovery_ReportWrittenUnderHeapPressure` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss report"

### Z2-R-RHI-123 Device fault information

- **Statement:** `CollectDeviceFaultReport` adds `VK_EXT_device_fault` data on Vulkan and DRED breadcrumbs and page-fault data on D3D12 when available.
- **Covers:** Z2-I-RHI-19
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the hardware device-loss device step in `Zenith2/Docs/Design/RHI/Tests.md` "Device steps"
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Device loss and fault reports"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Device removal and DRED"

### Z2-R-RHI-124 Recovery telemetry

- **Statement:** Device losses, recoveries, their reasons, swapchain recreations and the cold-PSO-cache flag are published to telemetry and the on-screen profiler.
- **Covers:** Z2-I-RHI-19
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-005
- **Budgets:** none
- **Acceptance evidence:** `Test_PipelineCache_EveryTriggerDiscards` (the cold flag in telemetry) and `Test_Recovery_DeviceLossRecover` (the loss and recovery counters and the telemetry record) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Telemetry"; `Zenith2/Docs/Design/RHI.md` → "Counters"

### Z2-R-RHI-125 Swapchain recreation steps

- **Statement:** Every recreation trigger but a frames-in-flight change drains only the frames that used the swapchain (the whole direct queue on D3D12) and calls no `WaitIdle`; a frames-in-flight change idles every queue through `WaitIdle`, a CPU wait on each queue's last-signalled timeline value (pending X89); no trigger calls `vkDeviceWaitIdle`, none destroys the device, and each resumes with a normal frame.
- **Covers:** Z2-I-RHI-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Recovery_RecreationNeverWaitsDeviceIdle` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Recreation state machine"; `Zenith2/Docs/Design/RHI.md` → "Changing the frames in flight"

### Z2-R-RHI-126 Histories invalidated by extent

- **Statement:** A recreation invalidates the histories of exactly the views whose render extent changed.
- **Covers:** Z2-I-RHI-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-059
- **Budgets:** none
- **Acceptance evidence:** `Test_Recovery_ResizeInvalidatesHistories` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Recreation state machine"

### Z2-R-RHI-127 Recreation failure retry

- **Statement:** A failed recreation is retried at the next frame prologue and three consecutive failures follow the device-recovery policy.
- **Covers:** Z2-I-RHI-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Swapchain_RecreationFailureRetries` (hidden window) in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Recreation state machine"

### Z2-R-RHI-128 Android surface loss and restore

- **Statement:** On surface loss the swapchain and surface are released after the in-flight frames that used the window, within the handshake's remaining budget of its 2,000 ms, the device and resources stay alive, acquire and present report out of date until the surface returns, and the surface and swapchain are recreated on restore; a wait that runs out of the budget returns without releasing, declares no loss, and releases the stale swapchain once its frames complete.
- **Covers:** Z2-I-RHI-20
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Test_Recovery_SurfaceReleaseTimeoutKeepsDevice` in the `Test` gate step on WARP and lavapipe (win-debug, win-release); `Test_Recovery_AndroidSurfaceLossRestore` as the S24 device step (50 cycles, no validation error)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Android surface loss"; `Zenith2/Docs/Design/RHI.md` → "Permitted main-thread GPU waits"

### Z2-R-RHI-129 Device-loss detection

- **Statement:** Device loss is detected from every submit, wait, acquire and present result and from a status check after a wait timeout, five consecutive timeouts on one value declaring a GPU hang, and the device then refuses every call.
- **Covers:** Z2-I-RHI-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameLifetime_WaitTimeoutChecksLoss` and `Test_Recovery_NoSubmitAfterLoss` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Device loss detection"; `Zenith2/Docs/Design/RHI.md` → "Device status state machine"

### Z2-R-RHI-130 Recover on first loss

- **Statement:** With `rhi.deviceLossPolicy=recover` a first loss pauses the worlds at a tick boundary, writes the report, recreates the device and swapchains, reloads the resident resources and the PSO groups, rebuilds the GPU scene from a full-state render-world extraction requested through `Flux_RenderWorldRing::RequestFullExtraction`, invalidates every history and resumes.
- **Covers:** Z2-I-RHI-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Recovery_DeviceLossRecover` (hidden window) on both backends (simulated) and on WARP through `RemoveDevice`, and `Test_GPUScene_FullStateWorldRebuilds`, in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Recover-or-clean-exit policy"

### Z2-R-RHI-131 Clean exit on repeated loss

- **Statement:** A second loss within 60 s, a failed recovery, or the `exit` policy requests an autosave at the next tick boundary, shows the user message and exits with `EXIT_CODE_DEVICE_LOST` (6), distinct from the creation refusal's `EXIT_CODE_DEVICE` (4); a clean exit that cannot finish in 10 s terminates with `FATAL_CODE_DEVICE_LOST`.
- **Covers:** Z2-I-RHI-21
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_Recovery_SecondLossExitsCleanly` and `Test_Recovery_ExitPolicySkipsRecovery` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine"

### Z2-R-RHI-132 Device loss on reference hardware

- **Statement:** Device-loss recovery is demonstrated on the RTX 3070 and the S24 with the report written and rendering recovered.
- **Covers:** Z2-I-RHI-21
- **Stage:** S6
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the hardware device-loss device step recorded as Stage 6 acceptance evidence
- **Design:** `Zenith2/Docs/Design/RHI/Tests.md` → "Device steps"

### Z2-R-RHI-133 Pipeline cache key and file

- **Statement:** Each game's pipeline cache is one file per backend named by vendor, device, driver version, pipeline-cache UUID or Agility SDK version and shader container version, with the Zenith2 header and payload hash.
- **Covers:** Z2-I-RHI-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_CacheRejectsDriverMismatch` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Cache key and file naming"

### Z2-R-RHI-134 Invalidation never fails the device

- **Statement:** Every invalidation trigger (driver, adapter, UUID, Agility, container version, corrupt payload, driver rejection) discards the cache and rebuilds from empty, logs once, sets the cold-cache flag, and never fails device creation.
- **Covers:** Z2-I-RHI-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_PipelineCache_EveryTriggerDiscards` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Load and invalidation"

### Z2-R-RHI-135 Journaled cache writes

- **Statement:** The cache is written by a background job through the journaled atomic write at clean shutdown and at most once per 60 s after new PSOs, an interrupted write keeping the previous file.
- **Covers:** Z2-I-RHI-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** `Test_PSO_CacheInterruptedWriteKeepsPrevious` and `Test_PipelineCache_WriteCadence` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Write cadence"

### Z2-R-RHI-136 Stale caches and the size cap

- **Statement:** Stale cache files of a game are deleted at startup, a driver change is logged as `graphics driver changed: pipeline caches rebuilding`, and a cache above `Z2-B-RPROD-030` is rebuilt from empty.
- **Covers:** Z2-I-RHI-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** Z2-B-RPROD-030
- **Acceptance evidence:** `Test_PipelineCache_OldKeysDeleted` and `Test_PipelineCache_OverCapRebuilds` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Load and invalidation"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Write cadence"

### Z2-R-RHI-137 Pipeline cache used by every creation

- **Statement:** Every pipeline creation uses the persistent cache (Vulkan pipeline cache; D3D12 pipeline library load, else create and store), returns whether it hit the cache in `Zenith_RHIPipelineCreateResult::m_bCacheHit`, and counts `RHI.Pipelines.CacheHits` and `RHI.Pipelines.CacheMisses`.
- **Covers:** Z2-I-RHI-22
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** Z2-B-RPROD-002
- **Acceptance evidence:** `Test_RHIConformance_PipelineCacheRoundTrip` and `Test_Pipeline_CacheHitFlagReported` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Vulkan.md` → "Pipeline layout and pipelines"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Pipelines and the PSO library"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header"; `Zenith2/Docs/Design/RHI.md` → "Counters"

### Z2-R-RHI-138 RHI conformance suite

- **Statement:** `ZenithRHI_Conformance` links both backends and runs every conformance case on each, comparing readbacks bit for bit with CPU expectations, as the RHI half of backend parity.
- **Covers:** Z2-I-RHI-14, Z2-I-RHI-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-001
- **Budgets:** none
- **Acceptance evidence:** every `Test_RHIConformance_*` case, `[Vulkan]` and `[D3D12]`, in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Tests.md` → "RHI conformance suite"; `Zenith2/Docs/Design/RHI.md` → "Backend parity RHI half"

### Z2-R-RHI-139 Backend parity gate

- **Statement:** From Stage 3 the BackendParity gate step runs with the RHI conformance suite as its RHI half on every diff reaching the `Zenith2Render` paths.
- **Covers:** Z2-I-RHI-15
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-031
- **Budgets:** Z2-B-PROC-027
- **Acceptance evidence:** the BackendParity step green in the Stage 3 acceptance
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Backend parity RHI half"; `Zenith2/Docs/Design/RHI.md` → "Stage 3"

### Z2-R-RHI-140 RHI budget harness

- **Statement:** `Zenith_Bench_Budgets_RHI` measures the RHI rows' inputs on the RTX 3070 and the S24 with 10 repetitions, medians and 95% confidence intervals, and its smoke test runs one repetition.
- **Covers:** Z2-I-RHI-04, Z2-I-RHI-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-084
- **Budgets:** Z2-B-RHI-001, Z2-B-RHI-002, Z2-B-RHI-003, Z2-B-RHI-006, Z2-B-RHI-007, Z2-B-RHI-009, Z2-B-RHI-010, Z2-B-RHI-011
- **Acceptance evidence:** `Test_BudgetHarness_RHI_Smoke` in the `Test` gate step
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Measurement harness"

### Z2-R-RHI-141 RHI budget rows allocated

- **Statement:** The Stage 1 per-system budget ticket for RHI allocates every pending RHI row from the adopted totals, commits the allocation table under `Zenith2/Docs/Research/Budgets/RHI/` and appends the ruling to the DecisionLog.
- **Covers:** Z2-I-RHI-04, Z2-I-RHI-10
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012, Z2-D-097
- **Budgets:** Z2-B-RHI-001, Z2-B-RHI-002, Z2-B-RHI-003, Z2-B-RHI-004, Z2-B-RHI-005, Z2-B-RHI-006, Z2-B-RHI-007, Z2-B-RHI-009, Z2-B-RHI-010, Z2-B-RHI-011
- **Acceptance evidence:** the rows `adopted` in `Zenith2/Docs/Budgets.md`, checked by the coverage gate step in acceptance mode for Stage 1
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Rows RHI owns"

### Z2-R-RHI-142 Recording and submit CPU budget

- **Statement:** The `Flux.BuildGraph` back-end steps, `Flux.Record` and `Flux.Submit` zones are measured in every workload run against `Z2-B-RHI-004` and `Z2-B-RHI-005`.
- **Covers:** Z2-I-RHI-18
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** Z2-B-RHI-004, Z2-B-RHI-005
- **Acceptance evidence:** the Stage 3 workload measurements on the reference desktop and the S24
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Rows RHI owns"

### Z2-R-RHI-143 Configuration keys

- **Statement:** The RHI registers the keys of its configuration table with types, ranges and defaults, and the composition root fills the device desc from them at startup step 12.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIConfig_KeysAndDefaults` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIConfig header"

### Z2-R-RHI-144 Shutdown order

- **Statement:** At shutdown every queue is waited idle and every retirement completed (step 5), the renderer is destroyed before the job system, and the device is destroyed at step 9 with its live-handle report.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Renderer_ShutdownOrder` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Startup and shutdown"

### Z2-R-RHI-145 Lock rows declared

- **Statement:** The RHI's mutexes use the levels 400, 410, 450, 460, 470 and 480 of its lock rows, added to the lock ordering table by their landing tickets.
- **Covers:** Z2-I-RHI-01, Z2-I-RHI-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** the lock-order checker in every Debug test run (no `FATAL_CODE_LOCK_ORDER`), review of the Contracts table diff
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Lock-order rows added by RHI"

### Z2-R-RHI-146 Headless runs create no device

- **Statement:** A headless run creates no RHI device, swapchain or renderer.
- **Covers:** Z2-I-RHI-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Runtime_HeadlessSkipsDevice` (MOD's, `Zenith2/Docs/Design/MOD.md` → "Runtime and composition tests") in the `Test` gate step
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Startup and shutdown"

### Z2-R-RHI-147 No public barrier API

- **Statement:** No public header other than the Graph-only barrier header declares a barrier, layout-transition, ownership-transfer or aliasing-barrier operation.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** rule F20 of the ForbiddenCheck gate step and the ApiContractCheck surface
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Explicit exclusions"

### Z2-R-RHI-148 No runtime shader compilation

- **Statement:** No RHI target links Slang or DXC or compiles shader source; pipelines are created from precompiled blobs alone.
- **Covers:** Z2-I-RHI-14
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-025
- **Budgets:** none
- **Acceptance evidence:** the Libgraph gate step's link-surface stage
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Explicit exclusions"

### Z2-R-RHI-149 No MSAA

- **Statement:** No attachment, pipeline or rendering scope has a sample count other than one.
- **Covers:** Z2-I-RHI-13
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** review; `Test_RHIConformance_MultisampleRejected` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Explicit exclusions"

### Z2-R-RHI-150 Client retirements batched per frame

- **Statement:** Table, pool, upload-chunk, material-constants and view-state slots a frame frees are collected per kind and retire as at most one retirement entry per kind per submitted frame, enqueued with the frame's final values, so freeing every slot of a maximum-capacity table in one frame causes no fatal and no full-ring wait.
- **Covers:** Z2-I-RHI-01, Z2-I-RHI-08
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIRetirementQueue_MassReleaseInOneFrame` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI/Memory.md` → "Slot allocators"

### Z2-R-RHI-151 Retirement never waits on unsubmitted work

- **Statement:** `Zenith_RHIRetirementQueue::Enqueue` asserts that its last-use values were already signalled, so the full-ring wait always waits on submitted work; the wait increments `RHI.Retirement.FullWaits`, which Flux re-publishes as `Flux.Lifetime.DeletionQueueFullWaits`.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIRetirementQueue_EnqueueAboveSignalledAsserts` (z2-death-assert) and `Test_RHIRetirementQueue_FullWaitsOnSubmitThread` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Retirement queue"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHILifetime header"

### Z2-R-RHI-152 A deferred frame loses no GPU-scene delta

- **Statement:** The GPU scene merges each render world's deltas into a persistent pending store (last write wins per slot); records, frees and growth stay pending across a deferred or failed frame, a further growth of a table while one is pending replaces the pending buffer and copies once from the original, freed slots are not reused before the frame that uploads them retires, and the next submitted frame uploads the coalesced records once.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_DeferredFrameKeepsDeltas` and `Test_UploadRing_DeferredFrameReservesExactNext` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Deferred frames and the pending store"; `Zenith2/Docs/Design/RHI/Memory.md` → "Scatter staging and coalescing"

### Z2-R-RHI-153 Build-node upload order

- **Statement:** No `Allocate` of a frame and no write into its ring memory precedes the frame's `ExtendReservation`: build-node producers declare their per-frame data bytes at setup and write them only after the reservation is final, and an early `Allocate` or `GetFrameData` asserts.
- **Covers:** Z2-I-RHI-04
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_UploadRing_AllocateBeforeExtendAsserts` (z2-death-assert) and `Test_UploadRing_DeferredFrameReservesExactNext` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Build-node upload order"; `Zenith2/Docs/Design/RHI.md` → "Frame-graph nodes and declared access"

### Z2-R-RHI-154 Frame need above the cap fails the frame

- **Statement:** A frame whose exact upload need exceeds `Z2-B-RHI-006` is cancelled (nothing recorded, submitted or presented; the pending store kept; histories untouched), increments `RHI.Upload.OverCap`, logs one `flux.upload` error line per contributor, is never deferred, and 30 consecutive failed frames take the frame-failure exit of Z2-R-RHI-160 with the message `The renderer could not fit a frame's uploads into memory. Your progress has been saved.` and `EXIT_CODE_DEVICE_LOST` (6).
- **Covers:** Z2-I-RHI-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_UploadRing_NeedAboveCapFailsFrame` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Frame failures"; `Zenith2/Docs/Design/RHI.md` → "Failure handling"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine"

### Z2-R-RHI-155 Readbacks of a frame that submits nothing fail

- **Statement:** A readback declared in a deferred frame moves to `FAILED` with `FLUX_READBACK_ERROR_FRAME_DEFERRED`, one declared in a failed frame with `FLUX_READBACK_ERROR_FRAME_FAILED`; its range is freed at once, no copy is recorded, and it is never re-declared into another frame.
- **Covers:** Z2-I-RHI-06
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** Z2-B-RHI-011
- **Acceptance evidence:** `Test_Readback_DeferredFrameFails` and `Test_UploadRing_DeferredFrameReservesExactNext` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Readback state machine"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "RHI-owned ZenithFlux public headers"

### Z2-R-RHI-156 Retirement drain headroom

- **Statement:** The release-request drain stops `uFLUX_RETIREMENT_DRAIN_HEADROOM` (512) entries short of the retirement ring's capacity, which is at least the frames-in-flight maximum times the derived per-interval bound on direct enqueues (105, its image term from `uFLUX_SWAPCHAIN_MAX_IMAGES` = 4, asserted at every swapchain creation), so no `Enqueue` of a correct program waits on a full ring while at most one prologue with render-idle enqueues falls in each submitted frame's interval; beyond that assumption the full-ring wait terminates and is never fatal.
- **Covers:** Z2-I-RHI-01
- **Stage:** S2
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_RHIRetirementQueue_MassReleaseInOneFrame` and `Test_RHIRetirementQueue_DrainStopsAtHeadroom` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Drain headroom"; `Zenith2/Docs/Design/RHI.md` → "Bounded queues and full policies"; `Zenith2/Docs/Design/RHI.md` → "Permitted main-thread GPU waits"

### Z2-R-RHI-157 Render-idle points

- **Statement:** Every call this area reserves for the render-submit thread that also runs on the main thread (recreation, a frames-in-flight change, surface release, recovery, renderer creation and destruction) does so only at a render-idle point, named in the type's contract block, and every Flux entry point run there asserts that no render subgraph is active; the flag it reads is cleared at the end of every frame's `Flux.Present`, including a frame whose device was lost, so the device-loss handler's entry assert holds at the next prologue.
- **Covers:** Z2-I-RHI-01, Z2-I-RHI-20
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-006
- **Budgets:** none
- **Acceptance evidence:** `Test_FrameLifetime_RenderIdleEntryAssertsInsideSubgraph` (z2-death-assert) and `Test_Recovery_DeviceLossRecover` (hidden window; the first prologue after a loss) in the `Test` gate step on WARP and lavapipe (win-debug, win-release); the ApiContractCheck gate step for the contract blocks
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Render-idle points"; `Zenith2/Docs/Design/RHI/PublicAPI.md` → "Zenith_RHILifetime header"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHIDevice header"; `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device loss detection"

### Z2-R-RHI-158 Frames-in-flight change re-prepares on slot 0

- **Statement:** A frames-in-flight change discards the preparation already made for the next frame on its old slot (its upload segment released, the old per-slot objects retired), re-runs the preparation for slot 0 without a wait (reserving the recorded need when the previous frame was deferred, else the prediction), reads and publishes the timestamps still pending in the old slots before retiring their pools, keeps the frame index, and the next frame builds on slot 0 with a fresh reservation and records only into command pools created by the change; slots advance only on submitted frames.
- **Covers:** Z2-I-RHI-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-050
- **Budgets:** Z2-B-RHI-008
- **Acceptance evidence:** `Test_FrameLifetime_FramesInFlightChangeRecreates` (hidden window) and `Test_FrameLifetime_SlotCounterSkipsUnsubmittedFrames` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI.md` → "Changing the frames in flight"; `Zenith2/Docs/Design/RHI.md` → "Frame-in-flight rotation"

### Z2-R-RHI-159 Imported-state table has a single writer

- **Statement:** The imported-state table holds no lock: every member runs on the render subgraph's sequential nodes or at a render-idle point, asset loader jobs never call it, and a development-build overlap detector asserts on any concurrent use.
- **Covers:** Z2-I-RHI-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-056
- **Budgets:** none
- **Acceptance evidence:** `Test_ImportedState_ConcurrentLoaderCreationDuringBuild` (z2-stress) in the `Test` gate step on WARP and lavapipe (win-debug, win-release) and under `Sanitizers -Preset win-asan`
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Imported-state table"; `Zenith2/Docs/Design/RHI.md` → "Lock-order rows added by RHI"

### Z2-R-RHI-160 Frame-failure exit

- **Statement:** The 30th consecutive failed frame stores its reason as the frame-failure exit request on the build node; the device-loss handler's next prologue call moves `NORMAL` to `EXITING`, requests the autosave, shows the reason's message (`The renderer could not fit a frame's uploads into memory. Your progress has been saved.` for an upload need above the cap, `The graphics device ran out of memory. Your progress has been saved.` for device memory exhaustion) and exits with `EXIT_CODE_DEVICE_LOST` (6), writes no device-loss report, leaves `RHI.Device.Losses` and `RHI.Device.Recoveries` unchanged, sends the reason and count to telemetry, and no further frame renders.
- **Covers:** Z2-I-RHI-05
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-045
- **Budgets:** Z2-B-RHI-006
- **Acceptance evidence:** `Test_UploadRing_NeedAboveCapFailsFrame` in the `Test` gate step on WARP and lavapipe (win-debug, win-release), and from Stage 4 `Test_Memory_ThirtyOOMFramesExitCleanly` for the device-memory reason
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Frame failures"; `Zenith2/Docs/Design/RHI/Recovery.md` → "Device-loss state machine"; `Zenith2/Docs/Design/RHI.md` → "Render-idle points"; `Zenith2/Docs/Design/RHI.md` → "Telemetry"

### Z2-R-RHI-161 Back-end compile entries

- **Statement:** The graph back end exposes exactly the three entries RPROD's compile driver calls — `Flux_GraphBuildQueuePlan` (step 3), `Flux_GraphAssignQueues` (step 6) and `Flux_GraphPartition` (step 7) with the signatures of the design — each allocating from the graph arena alone and reporting graph errors as diagnostics in the compiled graph, never through a return value, so one compile reports every error.
- **Covers:** Z2-I-RHI-18
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-055
- **Budgets:** Z2-B-RHI-004
- **Acceptance evidence:** `Test_GraphBackend_EntriesReportThroughDiagnostics` and `Test_RenderGraphTestAccess_CompileWithoutDevice` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Ownership of the compile steps"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 6 queue assignment"; `Zenith2/Docs/Design/RHI/RenderGraphBackend.md` → "Step 7 command-list partitions"

### Z2-R-RHI-162 HDR metadata and colour-space report

- **Statement:** Every swapchain creation and recreation reports, per window, the output colour spaces its surface supports, whether its display reports HDR and the display's maximum luminance where the API gives it; the swapchain manager sends LIGHT's HDR10 static metadata to an HDR10 swapchain through `SetHDRMetadata` (`vkSetHdrMetadataEXT` under the optional `VK_EXT_hdr_metadata`, `IDXGISwapChain4::SetHDRMetaData`) at once and after every recreation, sends none to SDR and scRGB swapchains, and without the extension logs once and continues.
- **Covers:** Z2-I-RHI-13
- **Stage:** S5
- **Classification:** required
- **Decisions:** none
- **Budgets:** none
- **Acceptance evidence:** `Test_Swapchain_CreatesEveryOutput` (hidden window) in the `Test` gate step on WARP and lavapipe (win-debug, win-release); the S24 presentation device step for a panel that reports HDR
- **Design:** `Zenith2/Docs/Design/RHI/Recovery.md` → "Present modes and formats"; `Zenith2/Docs/Design/RHI/PublicAPIDevice.md` → "Zenith_RHISwapchain header"; `Zenith2/Docs/Design/RHI/Vulkan.md` → "Swapchain and presentation"; `Zenith2/Docs/Design/RHI/D3D12.md` → "Swapchain and presentation"

### Z2-R-RHI-163 Material-slot range listener

- **Statement:** `Flux_GPUScene` reports every material-slot range it creates and frees to the one listener set through `SetRangeListener` (main thread, render-idle point; set after `Create` and before the first frame, cleared with `nullptr` before the listener and the GPU scene are destroyed): `OnMaterialRangeCreated(xMesh, axMaterials, uFirstSlot, axImages)` once per range created while the listener is set, on the thread running `ApplyDeltas`, with the S images arriving as the not-drawn image (every field 0, the three bucket IDs `uFLUX_RG_INVALID`) and staged as the listener leaves them; and `OnMaterialRangeFreed(uFirstSlot)` once when a range reported to that listener reaches a reference count of zero. A range created while no listener is set is never reported later; a full-state rebuild reports every release before any creation; the destructor makes no call; and a replacement without a clear, a call from inside a callback and destruction with a listener set each fail an assert.
- **Covers:** Z2-I-RHI-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-058, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_GPUScene_RangeListener_CreatedOncePerKey`, `Test_GPUScene_RangeListener_ImagesStagedFromListener`, `Test_GPUScene_RangeListener_FreedOnLastRelease`, `Test_GPUScene_RangeListener_UnreportedRangeNeverForwarded`, `Test_GPUScene_RangeListener_FullStateFreesBeforeCreates`, `Test_GPUScene_RangeListener_TableFullReportsNothing` and the three `z2-death-assert` cases `Test_GPUScene_RangeListener_ReplaceWithoutClearAsserts`, `Test_GPUScene_RangeListener_ReentrantCallAsserts`, `Test_GPUScene_RangeListener_DestroyWithListenerAsserts` in the `Test` gate step on WARP and lavapipe (win-debug, win-release)
- **Design:** `Zenith2/Docs/Design/RHI/Memory.md` → "Material-slot range listener"; `Zenith2/Docs/Design/RHI/Memory.md` → "Slot allocators"

## Coverage of inventory items

A requirement may sit at a subsequent stage to the item it covers when it serves that stage's acceptance; the item's own stage is then served by the requirements the Note column names, which alone satisfy that stage's acceptance.

| Item | Requirements | Note |
|---|---|---|
| Z2-I-RHI-01 | Z2-R-RHI-001, Z2-R-RHI-002, Z2-R-RHI-003, Z2-R-RHI-004, Z2-R-RHI-005, Z2-R-RHI-006, Z2-R-RHI-007, Z2-R-RHI-008, Z2-R-RHI-009, Z2-R-RHI-010, Z2-R-RHI-144, Z2-R-RHI-145, Z2-R-RHI-150, Z2-R-RHI-151, Z2-R-RHI-156, Z2-R-RHI-157 | — |
| Z2-I-RHI-02 | Z2-R-RHI-011, Z2-R-RHI-012, Z2-R-RHI-013, Z2-R-RHI-014, Z2-R-RHI-015, Z2-R-RHI-016, Z2-R-RHI-017, Z2-R-RHI-099, Z2-R-RHI-100 | — |
| Z2-I-RHI-03 | Z2-R-RHI-018, Z2-R-RHI-019, Z2-R-RHI-020, Z2-R-RHI-158 | — |
| Z2-I-RHI-04 | Z2-R-RHI-021, Z2-R-RHI-022, Z2-R-RHI-023, Z2-R-RHI-024, Z2-R-RHI-025, Z2-R-RHI-029, Z2-R-RHI-030, Z2-R-RHI-031, Z2-R-RHI-140, Z2-R-RHI-141, Z2-R-RHI-153 | — |
| Z2-I-RHI-05 | Z2-R-RHI-026, Z2-R-RHI-027, Z2-R-RHI-028, Z2-R-RHI-154, Z2-R-RHI-160 | — |
| Z2-I-RHI-06 | Z2-R-RHI-032, Z2-R-RHI-033, Z2-R-RHI-034, Z2-R-RHI-035, Z2-R-RHI-036, Z2-R-RHI-155 | item stage S2 served by Z2-R-RHI-032, Z2-R-RHI-033, Z2-R-RHI-034, Z2-R-RHI-036, Z2-R-RHI-155; the S3 requirement serves Stage 3's editor picking |
| Z2-I-RHI-07 | Z2-R-RHI-037, Z2-R-RHI-038, Z2-R-RHI-039, Z2-R-RHI-040, Z2-R-RHI-041, Z2-R-RHI-042, Z2-R-RHI-043, Z2-R-RHI-044, Z2-R-RHI-045, Z2-R-RHI-046, Z2-R-RHI-147, Z2-R-RHI-159 | — |
| Z2-I-RHI-08 | Z2-R-RHI-047, Z2-R-RHI-048, Z2-R-RHI-049, Z2-R-RHI-050, Z2-R-RHI-051, Z2-R-RHI-052, Z2-R-RHI-053, Z2-R-RHI-054, Z2-R-RHI-055, Z2-R-RHI-056, Z2-R-RHI-057, Z2-R-RHI-058, Z2-R-RHI-059, Z2-R-RHI-060, Z2-R-RHI-150, Z2-R-RHI-152, Z2-R-RHI-163 | item stage S3 served by Z2-R-RHI-047, Z2-R-RHI-048, Z2-R-RHI-049, Z2-R-RHI-050, Z2-R-RHI-051, Z2-R-RHI-052, Z2-R-RHI-053, Z2-R-RHI-054, Z2-R-RHI-055, Z2-R-RHI-056, Z2-R-RHI-057, Z2-R-RHI-059, Z2-R-RHI-060, Z2-R-RHI-152, Z2-R-RHI-163; the S4 requirement serves Stage 4's virtual texturing |
| Z2-I-RHI-09 | Z2-R-RHI-010, Z2-R-RHI-061, Z2-R-RHI-062, Z2-R-RHI-063, Z2-R-RHI-064, Z2-R-RHI-145 | — |
| Z2-I-RHI-10 | Z2-R-RHI-065, Z2-R-RHI-066, Z2-R-RHI-067, Z2-R-RHI-140, Z2-R-RHI-141 | — |
| Z2-I-RHI-11 | Z2-R-RHI-064, Z2-R-RHI-068 | — |
| Z2-I-RHI-12 | Z2-R-RHI-069, Z2-R-RHI-070, Z2-R-RHI-071, Z2-R-RHI-072 | — |
| Z2-I-RHI-13 | Z2-R-RHI-008, Z2-R-RHI-073, Z2-R-RHI-074, Z2-R-RHI-075, Z2-R-RHI-076, Z2-R-RHI-077, Z2-R-RHI-078, Z2-R-RHI-079, Z2-R-RHI-080, Z2-R-RHI-146, Z2-R-RHI-149, Z2-R-RHI-162 | item stage S2 served by Z2-R-RHI-008, Z2-R-RHI-073, Z2-R-RHI-074, Z2-R-RHI-075, Z2-R-RHI-077, Z2-R-RHI-078, Z2-R-RHI-079, Z2-R-RHI-080, Z2-R-RHI-146, Z2-R-RHI-149; the S3 requirement serves Stage 3's frame pacing and the S5 requirement Stage 5's HDR output |
| Z2-I-RHI-14 | Z2-R-RHI-015, Z2-R-RHI-017, Z2-R-RHI-081, Z2-R-RHI-082, Z2-R-RHI-083, Z2-R-RHI-084, Z2-R-RHI-085, Z2-R-RHI-086, Z2-R-RHI-087, Z2-R-RHI-088, Z2-R-RHI-089, Z2-R-RHI-090, Z2-R-RHI-091, Z2-R-RHI-092, Z2-R-RHI-093, Z2-R-RHI-094, Z2-R-RHI-095, Z2-R-RHI-096, Z2-R-RHI-097, Z2-R-RHI-098, Z2-R-RHI-099, Z2-R-RHI-100, Z2-R-RHI-138, Z2-R-RHI-143, Z2-R-RHI-148 | — |
| Z2-I-RHI-15 | Z2-R-RHI-096, Z2-R-RHI-101, Z2-R-RHI-102, Z2-R-RHI-103, Z2-R-RHI-104, Z2-R-RHI-105, Z2-R-RHI-106, Z2-R-RHI-138, Z2-R-RHI-139 | — |
| Z2-I-RHI-16 | Z2-R-RHI-107, Z2-R-RHI-108, Z2-R-RHI-109, Z2-R-RHI-110 | — |
| Z2-I-RHI-17 | Z2-R-RHI-106, Z2-R-RHI-111, Z2-R-RHI-112, Z2-R-RHI-113 | — |
| Z2-I-RHI-18 | Z2-R-RHI-041, Z2-R-RHI-114, Z2-R-RHI-115, Z2-R-RHI-116, Z2-R-RHI-117, Z2-R-RHI-118, Z2-R-RHI-119, Z2-R-RHI-120, Z2-R-RHI-142, Z2-R-RHI-161 | — |
| Z2-I-RHI-19 | Z2-R-RHI-108, Z2-R-RHI-121, Z2-R-RHI-122, Z2-R-RHI-123, Z2-R-RHI-124 | — |
| Z2-I-RHI-20 | Z2-R-RHI-077, Z2-R-RHI-125, Z2-R-RHI-126, Z2-R-RHI-127, Z2-R-RHI-128, Z2-R-RHI-157 | — |
| Z2-I-RHI-21 | Z2-R-RHI-121, Z2-R-RHI-129, Z2-R-RHI-130, Z2-R-RHI-131, Z2-R-RHI-132 | stage S3 per Roadmap Stage 3 acceptance ("Resize and device-loss recovery"), served by Z2-R-RHI-121, Z2-R-RHI-129, Z2-R-RHI-130, Z2-R-RHI-131; inventory row S6 pending the change rule (X79) |
| Z2-I-RHI-22 | Z2-R-RHI-133, Z2-R-RHI-134, Z2-R-RHI-135, Z2-R-RHI-136, Z2-R-RHI-137 | — |

## Excluded and deferred

No RHI inventory item is excluded or deferred; every one is required, at the stages above. The capabilities below are outside RHI's scope by decision and carry no requirement here:

| Capability | Classification | Decision | Where it lives |
|---|---|---|---|
| hardware ray tracing and virtualized geometry | deferred to Stage 7 | Z2-D-043 | ADVGFX's deferred tree, which extends the RHI interfaces |
| stereo views and multi-view rendering | deferred to Stage 7 | Z2-D-072 | SCOPE (XR) and RPROD |
| console RHI backends | deferred to Stage 7, gated on SDK access | Z2-D-071 | SCOPE |
| MSAA | excluded | Z2-D-035 | none: Z2-R-RHI-149 enforces its absence |
| tile-local reads and subpass merging | not implemented until D-TBDR (open, Stage 5) | Z2-D-036 | RPROD |
| a GPU backend on the `linux-tsan` host | excluded | Z2-D-010 | none: GPU libraries exclude `linux-tsan` |
| frame pacing (the controller) | required, owned elsewhere | Z2-D-025 | RPROD (Z2-I-RPROD-18); the RHI supplies the hooks of Z2-R-RHI-076 |
