# Zenith2 rendering: GPU lifetime, GPU scene tables and pools, recovery

Section file of `Zenith2/Docs/Rendering.md`. It details the entry-file sections "GPU lifetime and synchronisation", "GPU scene tables and pools" and "Recovery". Normative statements share the `REND` tag family of the entry file. Code lives in `Zenith2/Engine/ZenithRHI*/` (queues, timelines, memory) and `Zenith2/Engine/ZenithFlux/Internal/{Lifetime,Upload,GPUScene,Streaming,Recovery}/`.

## Queues and timelines

| Queue class (`Flux_QueueClass`) | Vulkan | D3D12 | Present when |
|---|---|---|---|
| `FLUX_QUEUE_GRAPHICS` | a queue of a family with `GRAPHICS \| COMPUTE \| TRANSFER` and present support | `D3D12_COMMAND_LIST_TYPE_DIRECT` | always |
| `FLUX_QUEUE_ASYNC_COMPUTE` | a queue of a compute-capable family without `GRAPHICS`, or a second queue of the graphics family | `D3D12_COMMAND_LIST_TYPE_COMPUTE` | `m_bAsyncComputeQueue`; otherwise async-flagged passes run on graphics |
| `FLUX_QUEUE_COPY` | a queue of a transfer-only family | `D3D12_COMMAND_LIST_TYPE_COPY` | `m_bCopyQueue`; otherwise copy-flagged passes run on graphics |

```cpp
struct Flux_QueueLastUse          { uint64 m_aulValue[FLUX_QUEUE_COUNT]; };   // 0 = never used on that queue
struct Flux_QueueCompletedValues  { uint64 m_aulValue[FLUX_QUEUE_COUNT]; };

class Zenith_RHITimeline          // one per queue; Vulkan timeline semaphore, D3D12 ID3D12Fence
{
public:
	uint64 GetCompletedValue() const;
	uint64 GetLastSignalledValue() const;
	Zenith_Result<Zenith_RHIWaitStatus, Zenith_RHIError> WaitCPU(uint64 ulValue, uint64 ulTimeoutNanoseconds);   // render-submit thread; the main thread at a render-idle point
};
```

A render-idle point is a stretch of the main thread during which no render subgraph runs and none can start until it returns (`Zenith2/Docs/Design/RHI.md → "Render-idle points"`): renderer creation and destruction, shutdown step 5, the frame prologue before `RunFrame`, the Android surface-release callback after the in-flight render subgraph completed, and device-loss recovery while the render subgraph is empty.

<!-- elaboration -->
| Main-thread wait at a render-idle point | Where | Bound |
|---|---|---|
| swapchain drain, and the `WaitIdle` of a frames-in-flight change | "Swapchain recreation" step 2 | `rhi.gpuWaitTimeoutMs` per wait and the loss check |
| Android surface release | the platform's surface-release callback | the handshake's remaining budget of [Z2-N-ARCH-916], with no loss declaration |
| device-loss recovery | the new device's first frame preparation, which waits on nothing | as the first row |
| shutdown | shutdown step 5's `WaitIdle` | as the first row |
| a full-ring retirement `Enqueue` | a render-idle `Enqueue` that finds the deferred-deletion ring full ([Z2-N-REND-490]) | as the first row |

- [Z2-N-REND-485] Every queue has exactly one timeline (`Zenith_RHITimeline`), every submission batch signals that queue's timeline with a strictly increasing value, and the value a batch signals is recorded as the last use, on that queue, of every resource and descriptor slot the batch accessed.
- [Z2-N-REND-486] The render-submit thread is the one thread that waits on a GPU timeline from the CPU during a frame, and the main thread waits exclusively at a render-idle point for the five waits of the table above, none of them a frame wait; every such wait carries a timeout of 2 s, after which the device status is checked for loss (see "Device loss"); no job and no pass lambda waits on the GPU (see `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`).
- [Z2-N-REND-487] Swapchain acquire and present use the binary semaphores (Vulkan) or the swapchain's own fence (D3D12) that presentation requires; every other GPU-GPU and GPU-CPU dependency uses the queue timelines.

## Retirement

```cpp
class Flux_DeferredDeletion   // ZenithFlux/Internal/Lifetime; one per device
{
public:
	void Enqueue(const Flux_RetiredObject& xObject, const Flux_QueueLastUse& xLastUse);   // bounded; see below
	void RetireCompleted(const Flux_QueueCompletedValues& xCompleted);                      // frame begin, render-submit thread
	uint32 GetPendingCount() const;
};
```

| Retired object kinds | Examples |
|---|---|
| RHI resources | textures, buffers, placed resources, heaps, query pools, pipelines replaced by a reload |
| descriptor slots | every index-space slot |
| GPU scene table slots | instance, primitive, material-slot and light slots; material-constants blocks |
| pool slots | skinned-vertex, particle, procedural-geometry, VT physical tiles |
| upload-ring chunks | chunks released by shrinking |

- [Z2-N-REND-488] A resource, descriptor slot, table slot, pool slot or upload chunk is retired (freed or returned to its free list) only once every queue's completed timeline value is at or past that object's last-use value on that queue; no object is reused before its retirement.
- [Z2-N-REND-489] `RetireCompleted` runs at the start of every frame on the render-submit thread, reads each queue's completed value once, and retires every satisfied entry, not just the oldest.
- [Z2-N-REND-490] The deferred-deletion queue holds at most `uFLUX_DEFERRED_DELETE_CAPACITY` = 65,536 entries, a constant realised as the ZenithRHI retirement ring's `uZENITH_RHI_RETIREMENT_CAPACITY` with `uFLUX_DEFERRED_DELETE_CAPACITY` its ZenithFlux alias (`inline constexpr uint32 uFLUX_DEFERRED_DELETE_CAPACITY = uZENITH_RHI_RETIREMENT_CAPACITY;`); when it is full, `Enqueue` on the render-submit thread waits (through `WaitCPU`) for the oldest entry's last-use values and retires it first, which is the queue's full policy (see `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"`), and every such wait increments `Flux.Lifetime.DeletionQueueFullWaits`.
- [Z2-N-REND-491] An object that was never used on the GPU (all last-use values 0) is retired at the next `RetireCompleted`.

## Frames in flight

- [Z2-N-REND-492] The number of GPU frames in flight is `r.framesInFlight`, 2 or 3 (default 2), within the adopted `Z2-B-RHI-008` Frames in flight row, fixed at device creation and changeable only through swapchain recreation by the procedure of the table "Changing the frames in flight" below; frame N's graph build starts only after the graphics timeline has reached the value signalled by the final graphics batch of the frame last submitted in its frame slot.

### Changing the frames in flight

The change runs at the frame prologue's render-idle point, after the next frame's preparation already ran on the old slot, so the procedure discards that preparation and re-makes it on slot 0 (`Zenith2/Docs/Design/RHI.md → "Changing the frames in flight"`).

| Order | Action |
|---|---|
| 1 | drain every swapchain, then `Zenith_RHIDevice::WaitIdle`: a CPU wait on every queue's last-signalled timeline value (never `vkDeviceWaitIdle`), after which the completed values equal the last-signalled values |
| 2 | `Zenith_RHIDevice::SetFramesInFlight(uFrames)`: the device creates its new per-slot objects (command pools, split-barrier events, timestamp pools, acquire semaphores, breadcrumb regions) and then destroys the old ones; a failure keeps the old count and objects and nothing below runs |
| 3 | `Flux_FrameLifetime::OnFramesInFlightChanged(uFrames)` on the main thread: publish the timestamps still pending in the old slots, release the old upload segments, retire the old timestamp pools, complete every pending readback, create the new slot records, pools and segments, reset the submitted-frame count to 0, and re-prepare the frame about to build on slot 0 with a fresh reservation (the exact need of a deferred previous frame, or the prediction) |
| 4 | recreate every swapchain with the new image count (frames in flight + 1) |

The frame index continues: the frame about to build is still the next frame, now on slot 0.
- [Z2-N-REND-493] Per-frame-in-flight resources are the upload-ring segment of that frame, one timestamp query pool per queue, the command pools or allocators of every recording partition, and the frame's readback staging; each is reused only after its frame's final batch on every queue has completed.
- [Z2-N-REND-484] A frame that submits nothing (deferred or failed, "Frame deferral" and "Frame failures") consumes a frame index but not a frame slot: the next frame prepares the same slot again, whose record keeps the final values of the frame last submitted in it.
- [Z2-N-REND-494] GPU frames in flight are independent of the one frame of CPU render lag of frame pipelining (Z2-D-050): simulation N+1 overlaps the build and record of render frame N, and the GPU may additionally be executing frames N−1 and N−2.

## Upload ring

```cpp
struct Flux_UploadAllocation
{
	void* m_pCPU;                         // write-combined, host-visible mapping
	Zenith_RHIBufferHandle m_xBuffer;     // the chunk
	Zenith_RHIDescriptorHandle m_xBufferSRV;   // the chunk's FLUX_DESCRIPTOR_SPACE_BUFFER_SRV descriptor
	uint64 m_ulOffset;
	uint64 m_ulSize;
};

enum Flux_UploadError : uint8
{
	FLUX_UPLOAD_ERROR_TOO_LARGE,                // larger than Z2-B-RHI-006 and not splittable; from ExtendReservation, a frame failure
	FLUX_UPLOAD_ERROR_RESERVATION_EXHAUSTED,    // Allocate beyond the frame reservation (a programmer error)
	FLUX_UPLOAD_ERROR_QUEUE_FULL,               // RequestUpload with the request queue full
	FLUX_UPLOAD_ERROR_DEVICE_LOST,
};

struct Flux_UploadRequestDesc
{
	Zenith_Span<const uint8> m_xSource;       // owned by the requester; valid until the operation reaches STAGED
	Flux_UploadDestination m_xDestination;     // a persistent buffer range, or a texture subresource region, not yet referenced by any table
	bool m_bSplittable;                         // the request may be split into pieces across frames
	uint64 m_ulSplitGranularity;               // piece alignment: a mip, a tile row, or a buffer granule
};

enum Flux_UploadStatus : uint8 { FLUX_UPLOAD_STATUS_QUEUED, FLUX_UPLOAD_STATUS_STAGED, FLUX_UPLOAD_STATUS_COMPLETE, FLUX_UPLOAD_STATUS_FAILED, FLUX_UPLOAD_STATUS_CANCELLED };

class Flux_UploadOperation   // an operation handle; see Contracts.md "Handle kinds"
{
public:
	Flux_UploadStatus GetStatus() const;
	uint64 GetCompletionValue() const;   // the copy-queue timeline value that completes it, once STAGED
	void Cancel();                                       // honoured while QUEUED; afterwards the copy completes and is discarded
};

class Flux_UploadRing   // ZenithFlux/Internal/Upload; one per device
{
public:
	// render-submit thread only; may stall there
	Zenith_Result<uint64, Flux_UploadError> ReserveFrame(uint64 ulBytes);
	void BeginFrame(uint32 uFrameSlot, const Flux_QueueCompletedValues& xCompleted);

	// the build node: per-frame data is declared at setup, the reservation extended after compile, the data written after that
	Flux_FrameDataRange DeclareFrameData(uint64 ulBytes, uint32 uAlignment, const char* szOwner);   // szOwner: a string literal
	uint64 GetDeclaredFrameDataBytes() const;
	Zenith_Result<Flux_ReservationOutcome, Flux_UploadError> ExtendReservation(uint64 ulExactBytes);   // never waits
	Flux_UploadAllocation GetFrameData(Flux_FrameDataRange xRange);   // after FITS or EXTENDED
	Flux_RGBuffer ImportFrameChunk(Flux_RenderGraph& xGraph);        // setup: the frame reservation's chunk; the same handle on every call of a frame

	// any frame thread after ExtendReservation (record jobs); lock-free bump inside the frame reservation, never stalls
	Zenith_Result<Flux_UploadAllocation, Flux_UploadError> Allocate(uint64 ulSize, uint32 uAlignment);

	// any worker job; queued, serviced by the render-submit thread at frame begin
	Zenith_Result<Flux_UploadOperation, Flux_UploadError> RequestUpload(const Flux_UploadRequestDesc& xDesc);

	uint64 GetCapacityBytes() const;
};

struct Flux_FrameDataRange { uint32 m_uIndex = 0; };   // a ticket from DeclareFrameData, valid for its frame

enum Flux_ReservationOutcome : uint8
{
	FLUX_RESERVATION_OUTCOME_FITS = 1,      // the prepared reservation already covers the exact need
	FLUX_RESERVATION_OUTCOME_EXTENDED,      // extended in place, or through a new chunk below the cap
	FLUX_RESERVATION_OUTCOME_DEFERRED,      // the need cannot be met without a wait: the frame is deferred
};
```

| Property | Rule |
|---|---|
| Structure | a list of chunks, each one host-visible buffer of `uFLUX_UPLOAD_CHUNK_BYTES` = 16 MiB (or larger when a single reservation needs it) with one `BUFFER_SRV` descriptor; allocations bump linearly within the frame reservation |
| Memory | Vulkan `HOST_VISIBLE \| HOST_COHERENT`, preferring `DEVICE_LOCAL` where exposed (ReBAR, UMA such as the S24); D3D12 `D3D12_HEAP_TYPE_UPLOAD` |
| Initial capacity | 64 MiB on `FLUX_CAPABILITY_TIER_DESKTOP`, 32 MiB on `FLUX_CAPABILITY_TIER_MOBILE` |
| Hard cap | `Z2-B-RHI-006` Upload ring hard cap (pending); frames with a stall or a deferral are budgeted by `Z2-B-RHI-007` Upload ring stall frames (pending) |
| Release | a frame's allocations become reusable when that frame retires |
| Shrink | chunks beyond the initial capacity unused for 600 consecutive frames are retired |

### Upload paths

| Client | Path | When sized |
|---|---|---|
| parameter structs of every pass | `Allocate` from the frame reservation, in record jobs | at graph compile: the sum over passes of the params size × (1 + `ExpectedDraws`) for raster passes, which bounds the `BeginDrawParams` calls of each raster lambda |
| GPU-scene scatter records, shadow-view records, cluster configuration, transparent entries, other per-frame data | `DeclareFrameData(bytes, align, owner)` at graph setup, written through `GetFrameData(ticket)` after the frame's `ExtendReservation` returned `FITS` or `EXTENDED`; passes read it through `ImportFrameChunk` | at graph compile (the declared sum), reserved at the previous `ReserveFrame` and extended at compile |
| streaming staging (textures, mesh regions, material-constants blocks, VT tiles) | `RequestUpload` from streaming jobs | when the request is serviced |

A pass reads a frame allocation in one way: the graph imports each upload-ring chunk once per frame as a buffer that no pass writes (host-written before submit), and the pass's field takes `SRV(chunk, range)` of it, so the read is graph-tracked like any other buffer read. The one exception is the parameter struct itself, which the push constant locates by the chunk's raw `BUFFER_SRV` index and offset ("The push constant" in the entry file).

| Step (render-submit thread, frame begin) | Action |
|---|---|
| 1 | `BeginFrame` retires the chunks of completed frames. |
| 2 | `ReserveFrame(n)`: n is the exact need a deferred previous frame recorded, else the prediction (the maximum compiled exact need over the last 8 built frames × 1.25 + 1 MiB, clamped to the hard cap); bump-reserve from free space; grow by a chunk of max(16 MiB, n) while below the hard cap; at the hard cap, wait (`WaitCPU`) for the oldest in-flight frame's retirement and retry; `Flux.Upload.Exhausted` increments and `Flux.Upload.StallMs` accumulates for every wait. n never exceeds the cap, because a need above it is a frame failure and never a deferral, so `FLUX_UPLOAD_ERROR_TOO_LARGE` is unreachable here. |
| 3 | Service the upload-request queue in FIFO order within the space left under the cap: each request copies its source into ring space and records its copy on the copy queue (the `StreamingUpload` pass), reaching `STAGED`; a splittable request larger than the space stages the pieces that fit and continues next frame; a non-splittable request larger than the cap fails with `FLUX_UPLOAD_ERROR_TOO_LARGE`; a request that does not fit this frame stays `QUEUED`. |
| 4 | An operation reaches `COMPLETE` when the copy timeline passes its completion value. |

- [Z2-N-REND-495] The upload ring grows only up to `Z2-B-RHI-006` Upload ring hard cap; at the cap it stalls until space frees, a single request larger than the cap is split into pieces when the request is splittable and is otherwise refused with `FLUX_UPLOAD_ERROR_TOO_LARGE`, and every exhaustion increments the `Flux.Upload.Exhausted` telemetry counter, as the tables above specify.
- [Z2-N-REND-496] Stalls happen exclusively on the render-submit thread, inside `ReserveFrame`; no `Allocate` and no ring write of a frame precedes that frame's `ExtendReservation` (an early one asserts); `Allocate` never stalls, allocating lock-free inside the frame's reservation, and returns `FLUX_UPLOAD_ERROR_RESERVATION_EXHAUSTED` (a programmer error, because the reservation is extended to the frame's compiled exact need) instead of growing; a raster lambda that issues more `BeginDrawParams` calls than its `ExpectedDraws` fails the frame in development builds with `FLUX_RECORD_CHECK_DRAW_PARAMS_EXCEEDED` and has the excess draws dropped in shipping builds.
- [Z2-N-REND-497] Worker jobs that upload data use `RequestUpload`, which enqueues into a bounded multi-producer request queue of `uFLUX_UPLOAD_REQUEST_CAPACITY` = 4,096 entries whose full policy is to reject with `FLUX_UPLOAD_ERROR_QUEUE_FULL` (the requester retries at its next tick), returns an operation handle, and never blocks the job; the requester keeps its source memory valid until the handle reports `STAGED`, and the upload-ring tests `Test_UploadRing_ReserveStallsOnSubmitThreadOnly`, `Test_UploadRing_AllocateNeverStalls`, `Test_UploadRing_RequestQueueFullRejects`, `Test_UploadRing_SplittableRequestCrossesFrames`, `Test_UploadRing_UnsplittableOverCapRefused` and `Test_UploadRing_DrawParamsBoundEnforced` exist and pass.

### Build-node upload order

The deferral below depends on one invariant: nothing of a frame is bumped or written before its reservation is final. The `Flux.BuildGraph` node therefore runs in this order (`Zenith2/Docs/Design/RHI/Memory.md → "Build-node upload order"`):

| Order | Action | Ring access |
|---|---|---|
| 1 | `Flux_GPUScene::ApplyDeltas` merges the render world's deltas into the GPU scene's persistent pending store | none |
| 2 | graph setup: every pass declares; every build-node producer of per-frame data calls `DeclareFrameData(bytes, align, owner)` and keeps the ticket; passes read ring data through `ImportFrameChunk`, a read-only import whose access never changes | none |
| 3 | compile; exact need = the parameter bytes of [Z2-N-REND-496]'s sizing rule + `GetDeclaredFrameDataBytes()` | none |
| 4 | `ExtendReservation(exact)`: `FITS` or `EXTENDED` → order 5; `DEFERRED` → "Frame deferral"; `FLUX_UPLOAD_ERROR_TOO_LARGE` → "Frame failures" | the reservation only |
| 5 | the producers write through `GetFrameData(ticket)`; the frame's transient view descriptors are allocated and written ([Z2-N-REND-052]); the frame-dump model is copied | frame-data block |
| after | `Flux.Record` jobs `Allocate` parameter structs | bump |

- [Z2-N-REND-507] The `Flux.BuildGraph` node performs the five orders of the table above in that order, every build-node producer of per-frame upload data declares its bytes at setup and writes them exclusively through `GetFrameData` at order 5, and a frame that leaves at order 4 by deferral or failure has written no ring byte; `Test_UploadRing_AllocateBeforeExtendAsserts` and `Test_UploadRing_DeferredFrameReservesExactNext` exist and pass.

### Frame deferral

A deferred frame is one whose exact need the ring would meet after a wait and no sooner: `ExtendReservation` returns `DEFERRED`, and the next frame preparation reserves that exact need on the render-submit thread, where the stall belongs.

| Element | Effect of a deferred frame f |
|---|---|
| recording, submission, present | none: the frame records, submits and presents nothing (the previous image stays on screen), writes no timestamps, and `Flux.Upload.ReservationDeferredFrames` increments |
| GPU-scene deltas | kept in the GPU scene's persistent pending store: last write wins per slot, frees and table growth stay pending, and nothing is committed until the next submitted frame, so a delta replay still reproduces the tables ([Z2-N-REND-008]) |
| view histories | not written: the ping-pong index does not swap and `m_bHistoryValid` is unchanged, so an instance updated in f and again in f + 1 carries a one-frame velocity error in f + 1 (its velocity spans one frame instead of two), for that frame alone |
| readbacks | every readback declared in f moves to `FAILED` with `FLUX_READBACK_ERROR_FRAME_DEFERRED` and its pool range is freed at once; the requester re-requests in a subsequent frame, as for `FLUX_READBACK_ERROR_POOL_FULL` |
| frame index and slot | the frame index advances; the frame slot does not ([Z2-N-REND-484]) |
| transient descriptors, imported states, staged uploads | no transient view descriptor is allocated or written; the imported-state table keeps the last submitted frame's states; staged upload requests are carried to the next frame that submits |

- [Z2-N-REND-508] A deferred frame has exactly the effects of the table above, the next frame preparation reserves the deferred frame's recorded exact need, and the GPU-scene tables after any sequence of submitted and deferred frames equal those of the same render-world sequence with no deferral (`Test_GPUScene_DeferredFrameKeepsDeltas`).

### Frame failures

A frame whose exact need exceeds `Z2-B-RHI-006` Upload ring hard cap, or whose transient heap or history allocation still fails after the critical memory-pressure release, is cancelled: `ExtendReservation` returns `FLUX_UPLOAD_ERROR_TOO_LARGE` (`FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP`), or the allocation fails (`FLUX_FRAME_FAILURE_DEVICE_OOM`), and every element of the deferral table behaves as for a deferred frame, except that the frame's readbacks fail with `FLUX_READBACK_ERROR_FRAME_FAILED` and the next preparation reserves the prediction, not the failed need.

| Element | A failed frame |
|---|---|
| counters and log | `FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP`: `RHI.Upload.OverCap` increments and one `flux.upload` error line names the largest contributor's owner string, once per contributor; `FLUX_FRAME_FAILURE_DEVICE_OOM`: `Flux.Memory.OOMSkippedFrames` increments |
| consecutive failures | a submitted frame resets the count; a deferred frame neither counts nor resets it; the 30th consecutive failed frame (`uFLUX_MAX_CONSECUTIVE_FAILED_FRAMES` = 30) stores the frame-failure exit request |
| the exit | the first frame prologue after the request takes the exit of "Device loss" step 6 with the failure's message ("The renderer could not fit a frame's uploads into memory. Your progress has been saved." or "The graphics device ran out of memory. Your progress has been saved."), exit code `EXIT_CODE_DEVICE_LOST` (6); the device is not lost, so no device-loss report is written and `RHI.Device.Losses` is unchanged |

- [Z2-N-REND-509] A failed frame records, submits and presents nothing, keeps the GPU-scene pending store and the view histories as a deferred frame does, fails its readbacks with `FLUX_READBACK_ERROR_FRAME_FAILED`, is never retried as a deferral, and the 30th consecutive failed frame ends the run through the frame-failure exit of the table above with `EXIT_CODE_DEVICE_LOST` (6) (`Test_UploadRing_NeedAboveCapFailsFrame`).

## Readbacks

```cpp
enum Flux_ReadbackStatus : uint8 { FLUX_READBACK_STATUS_PENDING, FLUX_READBACK_STATUS_READY, FLUX_READBACK_STATUS_FAILED, FLUX_READBACK_STATUS_CANCELLED };

class Flux_ReadbackHandle   // an operation handle; see Contracts.md "Handle kinds"
{
public:
	Flux_ReadbackStatus GetStatus() const;
	Zenith_Span<const uint8> GetData() const;   // non-empty exactly while READY
	void Cancel();
	void Release();
};
```

- [Z2-N-REND-498] A readback is an operation handle owned by the requester: it becomes `READY` after the timeline value of the frame that recorded its copy has completed, its data stays valid from `READY` until the requester calls `Release()`, and the graph never frees readback memory the requester still holds.
- [Z2-N-REND-499] Readback memory comes from a bounded readback pool sized by `Z2-B-RHI-011` GPU readback pool size (pending); `AddReadback` returns `FLUX_READBACK_ERROR_POOL_FULL` when the pool cannot hold the request (`FLUX_READBACK_ERROR_TOO_LARGE` for a range above the pool size), a readback declared in a deferred or failed frame fails with `FLUX_READBACK_ERROR_FRAME_DEFERRED` or `FLUX_READBACK_ERROR_FRAME_FAILED` and frees its range at once, and unreleased handles at renderer shutdown are reported as leaks with their request names.
- [Z2-N-REND-500] A readback whose result feeds simulation (for example editor picking) is published to simulation at a tick boundary in completion-ID order (Z2-D-053), never mid-tick.
- [Z2-N-REND-501] Device loss moves every pending readback to `FAILED`, a deferred or failed declaring frame moves its readbacks to `FAILED` with the two codes of [Z2-N-REND-499], and `Cancel()` moves a pending readback to `CANCELLED` and releases its pool space once its copy retires.

## Tracked resource kinds

| Kind | How it is tracked | Specified in |
|---|---|---|
| imported resources | the imported-state table carries state and queue across frames | `Zenith2/Docs/Rendering/RenderGraph.md` "Resource declaration and versioning" |
| persistent histories | view-owned, double-buffered (ping-pong), imported every frame; the current half is written, the previous half read | "Render views" in the entry file |
| subresource ranges | per (mip, layer) runs for textures, declared ranges for buffers | `Zenith2/Docs/Rendering/RenderGraph.md` "Barrier state machine" |
| read-modify-write access | one access that reads version v and produces v+1 | `Zenith2/Docs/Rendering/RenderGraph.md` "Resource declaration and versioning" |
| queue-family ownership transfers | release/acquire pairs for exclusive textures at queue boundaries | `Zenith2/Docs/Rendering/RenderGraph.md` "Barrier emission" |
| aliasing barriers | first-use discards covering every earlier occupant | `Zenith2/Docs/Rendering/RenderGraph.md` "Aliasing placement" |

- [Z2-N-REND-502] The render graph tracks each of the six resource kinds of the table above by the mechanism the table names, and no other mechanism orders GPU access to them.

## GPU memory rules

### Heap alignment and placement

| API | Rule |
|---|---|
| Vulkan | alignment and memory type from `vkGetDeviceImageMemoryRequirements`/`vkGetDeviceBufferMemoryRequirements`; transient groups separate linear and optimal resources, so `bufferImageGranularity` never applies inside a group; persistent resources through VMA pools per usage class |
| D3D12 | 64 KiB placement alignment by default; 4 KiB `D3D12_SMALL_RESOURCE_PLACEMENT_ALIGNMENT` for textures that qualify; heaps are split into buffers, render-target/depth-stencil textures and other textures on every resource heap tier |

- [Z2-N-REND-503] Every placed resource respects the alignment and heap-category rules of the table above on its API, and a placement that violates them is a programmer error caught by the placement unit tests.

### Residency budgets

- [Z2-N-REND-504] Flux reads the per-heap memory budget and usage every frame (`vmaGetHeapBudgets` over `VK_EXT_memory_budget`, `IDXGIAdapter3::QueryVideoMemoryInfo` for local and non-local segments) and reports GPU memory to the memory governor per category: render targets and transients, GPU scene tables, pools, streamed textures, meshes, upload ring, PSOs and other (see `Zenith2/Docs/Architecture.md → "Memory governor"`).
- [Z2-N-REND-505] Each category's limit is its memory row: persistent render targets and view histories `Z2-B-RHI-001`, transient peak `Z2-B-RHI-010`, per-heap residency `Z2-B-RHI-003`, descriptor heaps `Z2-B-RHI-002`, GPU scene tables `Z2-B-RPROD-001`, unified vertex and index pools `Z2-B-GEOM-001`, streamed textures `Z2-B-GEOM-002`, PSOs `Z2-B-RPROD-002`, upload ring `Z2-B-RHI-006`, VT `Z2-B-ADVGFX-001`, particle pools `Z2-B-VFX-001`, all pending, within the adopted totals `Z2-B-CORE-003` Windows VRAM and `Z2-B-CORE-001` S24 process RSS (see `Zenith2/Docs/Budgets.md → "Memory budgets"`).

### Responses to memory pressure

The shedding actions are exactly the thirteen of the Rendering row of the table "Shedding per subsystem" in `Zenith2/Docs/Architecture.md → "Memory governor"`, which is their single source; this section states the Flux mechanism of each and adds no action.

<!-- elaboration -->
| # | Level | Action (Architecture's Rendering row) | Mechanism inside Flux |
|---|---|---|---|
| 1 | warning | the texture-streaming pool target drops by 25% | the residency pool target Flux reports to the texture streamer is multiplied by 0.75 |
| 2 | warning | cached physical transients unused for 2 frames are released | the graph caches' retirement age `uFLUX_RG_CACHE_RETIRE_FRAMES` takes the value 2 instead of 8; retired resources go through deferred deletion |
| 3 | warning | transient heaps shrink at the next graph compile instead of after 300 frames | `uFLUX_RG_HEAP_SHRINK_FRAMES` takes the value 0, so the next compile reallocates each oversized group heap at its current requirement |
| 4 | warning | upload-ring chunks above the initial capacity are retired as soon as they are free | the upload ring's 600-frame shrink rule takes the value 0 frames |
| 5 | warning | the streamed-texture mip bias rises by 1 | the residency targets Flux reports to the streamer are computed with a mip bias of +1 |
| 6 | warning | capture and texture-view update intervals double | the round-robin scheduler of [Z2-N-REND-609] doubles the requested update interval of every `VIEW_KIND_CAPTURE` and `VIEW_KIND_TEXTURE` request |
| 7 | critical | every unused cached physical resource is released and group heaps are reallocated at the next graph compile | every graph-cache entry, placed resource and dedicated resource not used in the current frame is released through deferred deletion, and the next compile reallocates every group heap at its requirement |
| 8 | critical | streamed textures outside the current visible set drop to their minimum resident mips | the residency targets are computed with a mip bias of +2 |
| 9 | critical | the quality overrides disable SSR, depth of field and motion blur | `Flux_QualityOverrides` clears `FLUX_VIEW_FEATURE_SSR`, `FLUX_VIEW_FEATURE_DEPTH_OF_FIELD` and `FLUX_VIEW_FEATURE_MOTION_BLUR` for every view |
| 10 | critical | volumetric-fog resolution drops one step | the froxel grid of every view is the next lower quality tier's grid (the lowest tier's grid stays) |
| 11 | critical | the dynamic-resolution scale goes to its floor | every view's `m_fDynamicResolutionScale` is `Z2-B-SHIP-011` Dynamic-resolution floor |
| 12 | critical | capture and preview view histories are dropped | `Flux_ViewRegistry` releases the persistent state of every `VIEW_KIND_CAPTURE` and `VIEW_KIND_PREVIEW` view through deferred deletion; each such view renders its next frame with an invalid history |
| 13 | critical | no new persistent render target is created until the level falls | `Flux_PersistentResources::CreateTexture` refuses render-target and storage usages with `FLUX_RESOURCE_ERROR_MEMORY_PRESSURE`, and `Flux_ViewRegistry` defers the persistent state of a view that appears during the level, rendering it with an invalid history every frame until the level falls |

- [Z2-N-REND-506] Flux implements each of the thirteen actions of Architecture's Rendering row by the mechanism of the table above, as `Zenith2/Docs/Architecture.md → "Memory governor"` times them (within one frame of the level change, reverted one frame after the level falls), performs no other shedding action, and the memory-pressure tests `Test_Memory_WarningShedding` (asserting actions 1 to 6) and `Test_Memory_CriticalShedding` (asserting actions 1 to 13) force each level.

## GPU scene tables

The GPU scene (`Flux_GPUScene`, `ZenithFlux/Internal/GPUScene/`) owns four persistent, device-local table buffers. The render world's deltas (see "Purpose and scope" in the entry file) are the sole input that changes them.

### Instance record (128 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 48 | `m_xLocalToWorld` | float3x4, row-major, world from local |
| 48 | 48 | `m_xPrevLocalToWorld` | the previous frame's value; equal to the current value on the instance's first frame and after a history reset |
| 96 | 4 | `m_uPrimitiveBase` | first primitive record of the instance's mesh (LOD 0, section 0) |
| 100 | 4 | `m_uPrimitiveLayout` | bits 0–7 section count S, bits 8–11 LOD count L, bits 12–31 reserved; primitive = base + LOD × S + section |
| 104 | 4 | `m_uMaterialSlotBase` | first material-slot record; slot = base + section |
| 108 | 4 | `m_uFlags` | `FLUX_INSTANCE_FLAG_CASTS_SHADOW` (bit 0), `_SKINNED` (1), `_PROCEDURAL` (2), `_HISTORY_RESET` (3), `_HIDDEN_IN_MAIN` (4), `_TRANSPARENT` (5) |
| 112 | 4 | `m_uPoolSlot` | skinned-vertex or procedural pool slot; `0xFFFFFFFF` when none |
| 116 | 4 | `m_fMaxScale` | largest axis scale of `m_xLocalToWorld`, for bounding-sphere radius |
| 120 | 4 | `m_uLayerMask` | bits 0–23 render layers, bits 24–31 lighting channels |
| 124 | 4 | `m_uCustomData` | per-instance data a material may read (seed, tint index); when the instance's primitive carries `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE`, bits 16–31 are the instance's cull distance in metres (65,535: no limit) and bits 0–15 stay material data |

### Primitive record (128 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 12 | `m_xPositionScale` | SNORM16 position dequantisation scale (local units) |
| 12 | 4 | `m_uFirstIndex` | first index in the unified 16-bit index pool |
| 16 | 12 | `m_xPositionBias` | dequantisation bias |
| 28 | 4 | `m_uIndexCount` | |
| 32 | 8 | `m_xUV0Scale` | UNORM16 UV0 scale |
| 40 | 8 | `m_xUV0Bias` | UV0 bias |
| 48 | 12 | `m_xBoundsCentre` | local-space bounding-sphere centre |
| 60 | 4 | `m_fBoundsRadius` | local-space radius |
| 64 | 12 | `m_xBoundsExtent` | local-space AABB half-extent (about the centre) |
| 76 | 4 | `m_iVertexOffset` | base vertex in the unified stream pools (int32) |
| 80 | 4 | `m_uPositionOnlyFirstIndex` | first index of the position-only index buffer; `0xFFFFFFFF` when none |
| 84 | 4 | `m_uMeshletFirst` | first meshlet (visibility-buffer path) |
| 88 | 4 | `m_uMeshletCount` | |
| 92 | 4 | `m_uStreamSetAndLOD` | bits 0–15 stream-set ID (the raster code), bits 16–23 LOD index, bits 24–31 the primitive flags `FLUX_PRIMITIVE_FLAG_*`: flag bit n is bit 24 + n; flag bit 5 is `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` (set by GEOM's mesh cook for a mesh whose document requests it); flag bits 0–4, 6 and 7 are zero |
| 96 | 8 | `m_xUV1Scale` | UNORM16 UV1 scale |
| 104 | 8 | `m_xUV1Bias` | UV1 bias |
| 112 | 4 | `m_fLODScreenSize` | projected bounding-sphere diameter, as a fraction of the view height, below which the next LOD is selected |
| 116 | 12 | reserved | zero |

### Material-slot record (32 bytes)

A material slot is the binding of one material variant to one mesh section; instances with the same mesh and the same material assignment share slots.

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | `m_uMaterialVariant` | dense material-variant index (visibility-buffer classification bins) |
| 4 | 4 | `m_uFlags` | domain (bits 0–1), two-sided (2), alpha-tested (3), casts shadow (4) |
| 8 | 4 | `m_uConstantsBuffer` | `BUFFER_SRV` index of the material-constants buffer |
| 12 | 4 | `m_uConstantsOffset` | byte offset of this variant's constants block |
| 16 | 4 | `m_fAlphaCutoff` | |
| 20 | 4 | `m_uDepthBucket` | indirect bucket (pipeline, stream-set) for depth-writing passes |
| 24 | 4 | `m_uColourBucket` | indirect bucket for the colour or G-buffer pass |
| 28 | 4 | `m_uShadowBucket` | indirect bucket for shadow depth |

### Light record (64 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 12 | `m_xPositionWS` | metres |
| 12 | 4 | `m_fRange` | metres; attenuation reaches zero at the range |
| 16 | 12 | `m_xDirectionWS` | unit |
| 28 | 4 | `m_uType` | 0 directional, 1 point, 2 spot; further types are specified in `Zenith2/Docs/Design/LIGHT.md` |
| 32 | 12 | `m_xColourIntensity` | linear Rec.709 colour × intensity (candela for point and spot, lux for directional) |
| 44 | 4 | `m_fSourceRadius` | metres |
| 48 | 4 | `m_fSpotCosInner` | |
| 52 | 4 | `m_fSpotCosOuter` | |
| 56 | 4 | `m_uShadowIndex` | first shadow-view record of this light this frame; `0xFFFFFFFF` when unshadowed |
| 60 | 4 | `m_uFlags` | casts shadow (0), contact shadows (1), volumetric (2), lighting-channel mask (8–15) |

- [Z2-N-REND-535] The four table buffers hold records with exactly the byte layouts of the four tables above, identical on both APIs; the records are declared `[FluxShared]` in `Zenith2/Shaders/Common/GPUScene.slang`, and their C++ declarations are generated into `Flux_SharedTypes.generated.h` with a size and offset assert per field, so the CPU and GPU layouts cannot diverge (Z2-D-058, Z2-D-015).
- [Z2-N-REND-536] The tables are imported into the render graph every frame as write-restricted external buffers through `Flux_GPUScene::ImportTables`, passes read them through ordinary parameter-struct fields so the graph tracks them like any other buffer, and a write declared on them by any pass other than the scatter-upload and growth passes is `FLUX_RG_ERROR_TABLE_WRITE`.
- [Z2-N-REND-537] The initial capacities are 262,144 instances, 65,536 primitives, 65,536 material slots and 4,096 lights on `FLUX_CAPABILITY_TIER_DESKTOP`, and a quarter of each on `FLUX_CAPABILITY_TIER_MOBILE`; the maximum capacities follow from the memory row `Z2-B-RPROD-001` GPU scene tables (pending) divided by the record sizes above.
- [Z2-N-REND-538] Table slots are allocated from per-table free lists on the CPU; a freed slot is recycled only after the retirement fence of every frame that could have read it, as UE5 does with deferred instance-range frees.
- [Z2-N-REND-539] Transparent instances are sorted on the CPU per view by the view-space depth of their bounds centre plus the material's `sortBias` and receive one non-compacted indirect command each in that order, emitted by the transparent step of `Zenith2/Docs/Rendering/Culling.md` "Transparent commands", whose instance count the GPU culling writes (0 when culled); the CPU learns nothing of which transparent instances were culled.

## Scatter upload

```cpp
struct Flux_ScatterRecord    // 16 bytes, in the upload ring; declared [FluxShared] in Common/GPUScene.slang, generated
{
	uint32 m_uTable;          // 0 instance, 1 primitive, 2 material slot, 3 light
	uint32 m_uFirstSlot;
	uint32 m_uSlotCount;
	uint32 m_uWordBase;       // prefix sum of 16-byte words over the frame's records
};
```

| Step | Action |
|---|---|
| 1 | During extraction consumption, `Flux_GPUScene` applies the render world's deltas to its CPU-side slot allocators and writes each changed record once into a frame-local staging list, coalescing multiple writes to one slot so that the last write wins. |
| 2 | The staging list is written into the upload ring as `Flux_ScatterRecord`s followed by the record data, 16-byte aligned. |
| 3 | The scatter pass (`Passes/GPUScene/Scatter.slang`) runs one thread per 16-byte word across all records (256 threads per group); each thread finds its record by binary search over `m_uWordBase` and copies its word into the destination table. |
| 4 | The pass declares a UAV write on exactly the tables that have records this frame, and runs first in the frame, before any consumer (common prologue step 1). |

- [Z2-N-REND-540] The CPU never writes table memory directly: every change reaches a table through a scatter record in the frame's upload ring and the scatter-upload graph pass, which declares its writes on the tables and is ordered before every reader by the graph.
- [Z2-N-REND-541] Records of one frame never target the same slot twice (step 1 coalesces them), so the scatter pass has no write race and its result is independent of GPU thread order.
- [Z2-N-REND-542] Reads of a table by earlier frames are ordered before a subsequent frame's scatter write by the read-to-write barrier the graph emits for the tracked external buffer; queue order alone is not relied on, and consumers on other queues are covered by the per-queue timelines and ownership rules, so no CPU-side table versioning exists.
- [Z2-N-REND-543] A frame with no changed records adds no scatter pass.

## Table growth

- [Z2-N-REND-544] When a slot allocation would exceed a table's capacity, the GPU scene creates a table buffer of twice the capacity (clamped to the maximum capacity row), adds a resize-and-copy graph pass (`GPUSceneGrow`, a buffer copy of the old contents) before that frame's scatter pass, switches every subsequent import to the new buffer, and retires the old buffer through deferred deletion.
- [Z2-N-REND-545] An allocation beyond a table's maximum capacity fails with `FLUX_GPU_SCENE_ERROR_TABLE_FULL`, the instance or light is not rendered, and `Flux.GPUScene.TableFull` increments; no existing slot is evicted.

## Persistent resources

```cpp
class Flux_PersistentReadOnlyTexture   // a device handle (generational); see Contracts.md "Handle kinds"
{
public:
	Zenith_RHIDescriptorHandle GetSRV() const;     // allocated once, at creation
	bool IsResident() const;                 // false until the initial upload has completed
};
class Flux_PersistentReadOnlyBuffer { /* same shape; GetSRV returns the BUFFER_SRV descriptor */ };

class Flux_PersistentResources         // ZenithFlux; one per device
{
public:
	Zenith_Result<Flux_PersistentReadOnlyTexture, Flux_ResourceError> CreateTexture(const Flux_PersistentTextureDesc& xDesc, Flux_UploadSource xInitialData);
	Zenith_Result<Flux_PersistentReadOnlyBuffer, Flux_ResourceError> CreateBuffer(const Flux_PersistentBufferDesc& xDesc, Flux_UploadSource xInitialData);
	void Release(Flux_PersistentReadOnlyTexture xTexture);   // deferred deletion after the current frame's values on every queue
	void Release(Flux_PersistentReadOnlyBuffer xBuffer);
};
```

- [Z2-N-REND-546] Everything a table references (textures, the unified mesh and vertex pools, the index pools, material constants) is a `Flux_PersistentReadOnly` resource: it stays in the steady `PERSISTENT_READ` state after its initial upload, and the graph does not track it per frame.
- [Z2-N-REND-547] A `Flux_PersistentReadOnly` resource cannot hand out a UAV, render-target or depth-stencil view to any pass: the graph API has no such overload (the `UAV` overloads for these types are deleted), and the graph additionally rejects, with `FLUX_RG_ERROR_PERSISTENT_WRITE`, any frame in which the same RHI resource is imported writable; so two tables, or a table and a direct field, may reference the same resource, because every such use is a read.
- [Z2-N-REND-548] A persistent resource's initial contents, and any subsequent sub-allocation into a persistent pool (a new mesh into the unified stream pools, a new material-constants block), are written on the copy queue into regions no table entry references; the resource or region becomes visible through the graph's one-time acquire list in the first frame after the copy timeline passes the upload, and is referenced by a table only after that acquire, through the scatter upload.
- [Z2-N-REND-549] Referenced resources are reference-counted by the asset system, and a released persistent resource is destroyed through deferred deletion with the last-use values of the frame in which it was released on every queue.
- [Z2-N-REND-550] Resources that are written during the frame (VT physical and page-table updates, VT feedback, GPU simulation state, streaming uploads in flight) are never reached through tables; they are ordinary graph resources in the passes that write and read them.

## Pools

| Pool | Graph residency | Producer pass (declares the write) | Consumer passes (declare a read) | Layout |
|---|---|---|---|---|
| skinned-vertex | persistent-imported, double-buffered (current and previous) | compute skinning, then morph targets | every depth, prepass, G-buffer, visibility, colour and shadow pass drawing skinned instances; velocity (previous) | per slot: positions FLOAT16x4 object space (FLOAT32x4 fallback layout), QTangent SNORM16x4 |
| particle | persistent-imported | particle emit, simulate, compact and sort keys, sort | particle draw passes | particle state, trail samples, event records (sort keys are graph transients) |
| procedural-geometry | persistent-imported | procedural geometry regeneration | depth, colour and shadow passes drawing procedural instances | the default stream layout of `Zenith2/Docs/Rendering/VertexData.md` |
| VT physical and page-table | persistent-imported textures: `Flux_FrameShared::m_xVTPhysical`, one 2D-array texture with sRGB and UNORM views, and the page table | VT tile upload (copy queue) and page-table update | material passes sampling virtual textures | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Memory accounting and pressure" (Stage 4) |
| VT record buffer | a persistent read-only buffer sub-allocated slot by slot under [Z2-N-REND-548] | VT activation | material passes sampling virtual textures | `Zenith2/Docs/Design/ADVGFX/VirtualTexturing.md` → "Virtual texture records" (Stage 4) |

- [Z2-N-REND-551] Dynamic content reaches its consumers exclusively through the four pools of the table above: a table entry references either a `Flux_PersistentReadOnly` resource directly or a slot within a pool resource, the producer pass declares a write on the pool, every consumer binds the pool as a parameter-struct field (or vertex stream) declaring a read, and the table supplies only the slot index, so the graph always sees producer-to-consumer ordering and tables stay read-only.
- [Z2-N-REND-552] The skinned-vertex pool is double-buffered: each frame the halves swap, skinning writes the current half, and velocity for skinned and morphed geometry reads the previous half; an instance with `FLUX_INSTANCE_FLAG_HISTORY_RESET` uses its current positions as its previous positions for that frame.
- [Z2-N-REND-553] Pool slots are allocated on the CPU and retired through deferred deletion like table slots, and a pool whose capacity is exhausted grows by the resize-and-copy pattern of table growth, bounded by its memory row (`Z2-B-VFX-001` for the particle pool; the skinned-vertex and procedural-geometry pools within `Z2-B-GEOM-001`; VT within `Z2-B-ADVGFX-001`).

## Texture streaming swap

| Step | Action |
|---|---|
| 1 | The streaming system decides a texture's new resident mip range and creates a new `Flux_PersistentReadOnlyTexture` (not yet resident) with its own descriptor slot. |
| 2 | The streaming upload pass fills the new texture on the copy queue from staging in the upload ring (splittable requests, one piece per mip or per tile row). |
| 3 | The copy batch signals the copy timeline. |
| 4 | In the first frame whose start finds the copy timeline past that value, the texture is in `PERSISTENT_READ`: on Vulkan the image is concurrent-shared across the graphics, async-compute and copy families, so no ownership transfer exists, and the end of the copy batch records the layout transition `TRANSFER_DST_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` with source stage `COPY` and destination stage `NONE` (a transfer queue cannot name shader stages), the consumer queues' timeline wait on the copy value (stage `ALL_COMMANDS`) making it visible; on D3D12 the copy queue leaves it in `COMMON` and the graph's one-time acquire list transitions `COMMON` → `GENERIC_READ` on the direct queue. |
| 5 | A new material-constants block holding the new descriptor is uploaded (step 2 and 3 for a buffer region), and the scatter upload rewrites the referencing material-slot records' `m_uConstantsOffset` to it. |
| 6 | The old texture, its descriptor slot and the old constants block are released through deferred deletion with the last-use values of the last frame that could reference them. |

- [Z2-N-REND-554] Texture streaming uses reallocate-and-swap as the table above specifies: the new resource is filled on the copy queue, swapped into the table by a scatter upload after the copy fence completes, and the old one retires through deferred deletion; a texture in use is never written in place.

## GPU scene tests

- [Z2-N-REND-555] The GPU scene tests below exist in `Zenith2/Engine/ZenithFlux/Tests/GPUScene/`, run on WARP and lavapipe with synchronization validation and GPU-based validation clean throughout, and pass.

| Test | Asserts |
|---|---|
| `Test_GPUScene_SkinnedSameFrameAllPasses` | a skinned mesh is drawn in the depth, base (colour or G-buffer) and shadow passes in the same frame it is skinned, with correct positions in each |
| `Test_GPUScene_ParticlesSimulateThenDraw` | GPU particles simulated this frame are drawn this frame |
| `Test_GPUScene_ProceduralRegeneratedMidFrame` | procedural geometry regenerated between two consumers is seen by the second consumer only |
| `Test_GPUScene_VTTileUploadThenSample` | a VT tile uploaded this frame is sampled correctly after its acquire |
| `Test_GPUScene_SharedTextureDuringStreamingSwap` | two tables referencing the same texture while a streaming swap is in flight both render correctly before, during and after the swap |
| `Test_GPUScene_SlotReuseAcrossFramesInFlight` | a freed slot reused while earlier frames are in flight never shows the new content in an old frame |
| `Test_GPUScene_PersistentReadOnlyWriteRejected` | a pass that tries to write a persistent read-only resource is rejected (`FLUX_RG_ERROR_PERSISTENT_WRITE`) and the deleted overloads fail to compile in a compile-fail fixture |
| `Test_GPUScene_ScatterCoalescesSlot` | two same-frame writes to one slot leave the last value |
| `Test_GPUScene_GrowPreservesContents` | growth preserves every live record bit for bit |
| `Test_GPUScene_DeltaReplayReproducesTables` | replaying a recorded delta stream from frame 0 gives tables bit-identical to the live run |
| `Test_GPUScene_HistoryResetZeroVelocity` | a teleported instance produces zero velocity that frame |

## Swapchain recreation

| Trigger | Detection |
|---|---|
| window resize, DPI change, output change | platform window events (see `Zenith2/Docs/Architecture.md → "Platform layer"`) |
| `VK_ERROR_OUT_OF_DATE_KHR`, `VK_SUBOPTIMAL_KHR` | acquire or present result |
| DXGI resize or occlusion | `DXGI_STATUS_OCCLUDED`, window size change |
| Android surface loss and restore | `APP_CMD_TERM_WINDOW`, `APP_CMD_INIT_WINDOW` (through SDL3) |
| HDR output toggle, present-mode change, frames-in-flight change | settings |

| Step | Action |
|---|---|
| 1 | Stop acquiring swapchain images; finish building nothing new for the swapchain. |
| 2 | Wait (main thread at the frame prologue's render-idle point, `WaitCPU`) for the graphics timeline values of every in-flight frame that used a swapchain image; on D3D12, which requires every back-buffer reference released before `ResizeBuffers`, wait for the direct queue to finish all frames. A frames-in-flight change instead drains every swapchain of the device and idles every queue with `Zenith_RHIDevice::WaitIdle` (a wait on each queue's last-signalled timeline value), then runs "Changing the frames in flight". |
| 3 | Vulkan: create the new swapchain with `oldSwapchain` set and retire the old one through deferred deletion; D3D12: `ResizeBuffers` (or recreate for a format or colour-space change). Android surface loss: destroy the swapchain and surface and produce no frames until the surface returns, keeping the device and every resource. |
| 4 | Update the output extent of every view whose output is the swapchain; views whose output extent changed get their histories invalidated ([Z2-N-REND-599]); transients re-key naturally through the graph caches. |
| 5 | Resume. The first frame after recreation is a normal frame. |

- [Z2-N-REND-983] Swapchain recreation follows the steps of the table above for every trigger listed, never waits for device idle on Vulkan (`vkDeviceWaitIdle`) and waits for every queue's last-signalled timeline value (`Zenith_RHIDevice::WaitIdle`) for the frames-in-flight trigger alone, never destroys the device, and invalidates the histories of exactly the views whose output extent changed.
- [Z2-N-REND-984] A zero-sized or minimised output produces no frames and records no GPU work until the output returns, and the rest of the engine keeps running per `Zenith2/Docs/Architecture.md → "Platform layer"`.
- [Z2-N-REND-985] The swapchain formats (SDR `B8G8R8A8_SRGB`, HDR10 `A2B10G10R10_UNORM` with the ST.2084 colour space, scRGB `R16G16B16A16_SFLOAT`) are all in the boot PSO group for every pass that writes the swapchain, so a format change on recreation causes no PSO miss.

## Device loss

### Detection and breadcrumbs

| Source | Mechanism |
|---|---|
| Vulkan | `VK_ERROR_DEVICE_LOST` from submit, wait or present; a `WaitCPU` timeout of 2 s followed by a status check; `VK_EXT_device_fault` details where present |
| D3D12 | `DXGI_ERROR_DEVICE_REMOVED`, `_RESET`, `_HUNG` and `GetDeviceRemovedReason`; DRED auto-breadcrumbs and page-fault data in development builds |
| breadcrumbs | a host-visible breadcrumb buffer per queue: each pass writes its pass index as a "begin" marker before its body and an "end" marker after it (`vkCmdWriteBufferMarker2AMD` or `VK_NV_device_diagnostic_checkpoints` where present, otherwise `vkCmdFillBuffer` outside rendering scopes, so a merged scope reports at scope granularity; D3D12 `WriteBufferImmediate`) |

- [Z2-N-REND-986] Every queue carries GPU breadcrumbs as the table above specifies, and on device loss the crash report names, per queue, the last pass whose end marker completed and the first pass whose begin marker was written without an end, together with the frame index, the device-fault or DRED data, the driver version and the frame dumps of the last three frames kept in memory (see `Zenith2/Docs/Design/DEVT.md` for the crash-report format).

### Recover-or-clean-exit policy

| Step | Action |
|---|---|
| 1 | On the first device loss, stop rendering; simulation pauses at the next tick boundary. |
| 2 | Write the device-loss report. |
| 3 | Destroy every GPU object (generational handles held elsewhere become stale and fail safely), recreate the device on the same backend and adapter, and recreate the swapchain; for a requested recreation (`Flux_Renderer::RequestDeviceRecreation`, Stage 7, the XR device-interop provider's case) the device is recreated from the desc's interop request, which may name another adapter or a runtime-created instance, and the same-adapter clause does not apply. |
| 4 | Reload every resident GPU resource through the asset system's residency set, recreate the boot, global and current-scene PSO groups (the persistent pipeline cache makes this fast), and rebuild the GPU scene from a full-state render-world extraction instead of deltas, through `Flux_RenderWorldRing::RequestFullExtraction` ("Purpose and scope" in the entry file, subsection "Render world resync"). |
| 5 | Invalidate every view history, then resume simulation and rendering. |
| 6 | If recovery fails, or a second device loss happens within 60 s of the first, request an autosave through the save system at the next tick boundary (the save path touches no GPU state), show the message "The graphics device stopped responding and could not be recovered. Your progress has been saved." through the platform message box (the UI cannot render), and request shutdown with `EXIT_CODE_DEVICE_LOST` (6) from the exit-code table of `Zenith2/Docs/Architecture.md → "Composition root and Player"` ("the graphics device was lost and could not be recovered, or 30 consecutive frames could not be rendered; autosave requested"); `EXIT_CODE_DEVICE` (4) stays the device-creation refusal at startup; if the autosave and shutdown have not finished within 10 s, `ZENITH_FATAL(FATAL_CODE_DEVICE_LOST, …)` ends the process, the fatal being the fallback alone. The frame-failure exit of "Frame failures" takes this step with its own message and the same code. |

- [Z2-N-REND-987] Device loss is handled by the recover-or-clean-exit policy of the table above, selected by `rhi.deviceLossPolicy` (`recover`, the default in every build, or `exit`, which skips steps 3 to 5); no build continues to submit to a lost device. A requested recreation is not a loss: `rhi.deviceLossPolicy` does not apply to it, it enters `RECOVERING` from `NORMAL` after a device wait-idle, every world is paused at its next tick boundary exactly as for a loss and unpaused at step 5, it is not counted towards the 60-second second-loss rule, a failure takes `EXITING`, and it is counted in `RHI.Device.RequestedRecreations` beside `RHI.Device.Recoveries`.
- [Z2-N-REND-991] The clean exit of device loss and of the frame-failure exit is `EXIT_CODE_DEVICE_LOST` (6), never `EXIT_CODE_DEVICE` (4), and `FATAL_CODE_DEVICE_LOST` is raised exclusively when that clean exit has not finished within 10 s.
- [Z2-N-REND-988] Device-loss recovery is tested by `Test_Recovery_DeviceLossRecover` and `Test_Recovery_SecondLossExitsCleanly`, using the RHI test hook `Zenith_RHITestHooks::SimulateDeviceLost` (compiled into test executables exclusively) on both backends and `ID3D12Device5::RemoveDevice` on WARP; the swapchain paths are tested by `Test_Recovery_ResizeInvalidatesHistories` and `Test_Recovery_AndroidSurfaceLossRestore` (the latter a `needs-human` device step on the S24).

## Pipeline cache invalidation

| Invalidation trigger | Detected by |
|---|---|
| driver update | driver version in the cache key |
| different GPU or adapter | vendor and device IDs in the key |
| Vulkan cache incompatibility | `pipelineCacheUUID` in the key |
| Agility SDK change (D3D12) | Agility SDK version in the key |
| shader container version change (global layout, root signature, static samplers, container format) | container version in the key |
| corrupt or truncated payload | payload hash in the file header |
| driver rejection | `D3D12_ERROR_DRIVER_VERSION_MISMATCH`, `D3D12_ERROR_ADAPTER_NOT_FOUND`, or a Vulkan cache the driver ignores |

- [Z2-N-REND-989] Every trigger in the table above discards the affected persistent pipeline cache and rebuilds it from empty, and a discarded cache is reported once per run in the log and as the cold-PSO-cache flag of the run's telemetry, so the cold-versus-warm cache measurement of `Zenith2/Docs/Budgets.md → "Measured in every workload run"` knows which runs were cold.
- [Z2-N-REND-990] The last driver version seen is persisted per game in the user cache directory, and a driver change is logged as "graphics driver changed: pipeline caches rebuilding" at startup.
