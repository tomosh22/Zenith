# RHI design: memory, lifetime and the GPU scene

This file is part of the RHI design family. The entry document is `Zenith2/Docs/Design/RHI.md`. It specifies allocation and residency, the device resource table, placement rules, the retirement queue, the descriptor heap's slot lists, the upload ring's and readback pool's internals, residency reporting, the memory-pressure responder, device-memory exhaustion, and the implementation of the GPU scene tables and pools.

## Scope of this file

The specification is `Zenith2/Docs/Rendering/GpuResources.md → "Retirement"`, "Upload ring", "Readbacks", "GPU memory rules", "GPU scene tables", "Scatter upload", "Table growth", "Persistent resources", "Pools" and "Texture streaming swap", and `Zenith2/Docs/Rendering.md → "Binding model"` (heading "Descriptor slot allocation"). Files are in `Zenith2/Engine/ZenithRHI/Internal/` (the neutral RHI parts) and `Zenith2/Engine/ZenithFlux/Internal/{Lifetime,Upload,Readback,Bindless,Memory,GPUScene}/` (the Flux parts this area owns).

## Memory classes and allocation

| Rule | Detail |
|---|---|
| one allocator per API | VMA on Vulkan ([Z2-N-REND-022]), D3D12MA on D3D12 ([Z2-N-REND-025]); no other code allocates device memory |
| classes | `Zenith_RHIMemoryClass` (`Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_RHITypes header"`); realised per `Zenith2/Docs/Design/RHI/Vulkan.md → "Memory through VMA"` and `Zenith2/Docs/Design/RHI/D3D12.md → "Memory through D3D12MA"` |
| who uses which class | persistent render targets and view histories: `DEVICE_DEDICATED`; transient group heaps: heaps from `CreateHeap` in the placement group of their members; `Flux_PersistentReadOnly` textures and pools, GPU scene tables: `DEVICE_LOCAL`; upload-ring chunks and staging: `UPLOAD`; readback pool and breadcrumb buffers: `READBACK` |
| usage unions | a buffer's usage is the union of every declared use ([Z2-N-REND-866] and `Zenith2/Docs/Rendering/VertexData.md → "Buffer usage unions"`); the union is part of the graph caches' physical-resource key |
| accounting | every allocation records its bytes against one of the eight categories of [Z2-N-REND-504] in `Zenith_RHIAllocationStats` (Internal), read by the residency reporter |

## Device resource table

Device handles have their own layout (32-bit slot, 24-bit generation, 8-bit kind), so the device keeps its own table per kind rather than a `Zenith_SlotTable` (`Zenith2/Docs/Design/CORE/Memory.md → "Zenith_SlotTable"`).

```cpp
// Zenith2/Engine/ZenithRHI/Internal/Zenith_RHIResourceTable.h
template<typename TRecord>
class Zenith_RHIResourceTable
{
public:
	explicit Zenith_RHIResourceTable(Zenith_DeviceHandleKind eKind, uint32 uCapacity, Zenith_Heap& xHeap);
	Zenith_Result<uint64, Zenith_RHIError> Allocate(TRecord&& xRecord);   // handle bits
	Zenith_Result<TRecord*, Zenith_RHIError> Resolve(uint64 ulBits);        // STALE_HANDLE on null, stale, foreign kind
	void Free(uint64 ulBits);                                                             // retirement only
	uint32 GetLiveCount() const;
	template<typename TVisitor> void ForEachLive(TVisitor&& xVisitor);
};
```

<!-- elaboration: the capacities -->
| Kind | Capacity (desktop / mobile) | Record |
|---|---|---|
| texture | 131,072 / 32,768 | API image or resource, allocation, desc, per-view cache (attachment views, RTV/DSV indices) |
| buffer | 131,072 / 32,768 | API buffer or resource, allocation, desc, mapped pointer, GPU address |
| pipeline | 32,768 / 16,384 | API pipeline or PSO, bind point, key hash |
| heap | 4,096 / 1,024 | allocation, size, placement group |
| query pool | 64 / 64 | API query pool or heap, readback buffer (D3D12), count |

Algorithm: paged storage (pages of 1,024 records from the general heap) so records never move; per-slot generation, starting at 1; a FIFO free list so a freed slot is reused as late as possible; a slot whose 24-bit generation reaches `0xFFFFFF` is retired and never reissued; `Allocate` at capacity returns `ZENITH_RHI_ERROR_TABLE_FULL` ([Z2-N-BUDGET-261] analogue for resources); `Resolve` checks kind bits, slot range, liveness and generation, and never asserts ([Z2-N-CONTRACT-062]). Allocation and free take `uLOCK_LEVEL_RHI_RESOURCE_TABLE` (410, short-hold); `Resolve` is lock-free (the page pointer array is published with release ordering and pages never move). Destroying the device walks every table and logs each live handle with its debug name at `LOG_LEVEL_ERROR` in `rhi.device`, then destroys them ([Z2-N-CONTRACT-089]); `Test_DeviceHandle_LiveHandlesReportedAtDestruction` asserts the report.

## Heap alignment and placement

The rules are `Zenith2/Docs/Rendering/GpuResources.md → "GPU memory rules"` (heading "Heap alignment and placement"); this design adds the neutral contract the placement algorithm (RPROD's) relies on:

| Contract | Rule |
|---|---|
| requirement query before creation | `GetTextureMemoryRequirements` and `GetBufferMemoryRequirements` return size, alignment and placement group for the exact desc a subsequent `CreatePlaced*` will use, without creating anything (maintenance4 on Vulkan, `GetResourceAllocationInfo2` on D3D12) |
| placement groups | two resources may share a heap only when their groups are equal; groups are Vulkan (memory type, linear/optimal) and D3D12 (buffers, RT/DS textures, other textures) on every tier |
| heap sizes | multiples of 64 KiB; group heaps are rounded to 4 MiB by placement ([Z2-N-REND-435] pseudocode) |
| offsets | every placed resource's offset is a multiple of its reported alignment; `CreatePlaced*` with a misaligned offset or a group mismatch returns `ZENITH_RHI_ERROR_INVALID_DESC` and asserts in development builds ([Z2-N-REND-503]) |

## Retirement queue

`Zenith_RHIRetirementQueue` (`Zenith2/Docs/Design/RHI/PublicAPI.md → "Zenith_RHILifetime header"`) is the one mechanism by which a GPU object is freed: RHI resources, descriptor slots (through the descriptor heap's own pending list, below), GPU scene table slots, pool slots and upload-ring chunks ([Z2-N-REND-488] table).

<!-- elaboration -->
| Structure | Detail |
|---|---|
| pending list | a ring of `uZENITH_RHI_RETIREMENT_CAPACITY` = 65,536 entries of `{Zenith_RHIRetiredEntry, Zenith_RHIQueueValues}` (40 bytes; 2.5 MiB) in enqueue order |
| release requests | a bounded MPSC `Zenith_BoundedQueue` (`Zenith2/Docs/Design/CORE/Containers.md → "Zenith_BoundedQueue"`) of `RESOURCE` entries alone (a request with a descriptor slot carries it in the entry's `m_uDescriptorSpace` and `m_uIndex`), capacity = the sum of the device resource-table capacities (299,072 on desktop), policy `QUEUE_FULL_POLICY_FATAL`, counters `rhi.release_requests.full`, `.dropped`, `.high_water` ([Z2-N-CONTRACT-345]). The bound is provable: a request names a live resource, a resource is released once, and at most the table capacities are live, so a correct program never fills it ([Z2-N-CONTRACT-342]) |
| client batches | `CLIENT` entries never use the request queue: Flux collects the slots a frame frees into one batch per client kind (Slot allocators, below) and the render-submit thread enqueues at most one entry per kind per submitted frame (`uFLUX_RETIRED_KIND_COUNT` = 8 kinds, so at most 8 client entries per frame however many slots a scene unload frees) |
| listener | at most one `Zenith_RHIRetirementListener`, set by the Flux renderer at its creation and cleared before its destruction (both render-idle points); `Flux_DeferredDeletion` implements it |

| Operation | Algorithm |
|---|---|
| `Enqueue(entry, lastUse)` | render-submit thread, or the main thread at a render-idle point (entry document "Render-idle points"); `ZENITH_ASSERT(lastUse.IsSatisfiedBy(GetLastSignalledValues()))`, because values above the last submission would make the full-ring wait below wait for work never submitted (a deadlock the 2 s timeout would misreport as device loss); when the ring is full, `WaitCPU` on each queue for the oldest entry's value (timeout `rhi.gpuWaitTimeoutMs`, then the loss check), retire it, increment `rhi.retirement.full` (profiler alias `RHI.Retirement.FullWaits`, which `Flux_DeferredDeletion` re-publishes as `Flux.Lifetime.DeletionQueueFullWaits`, [Z2-N-REND-490]); then append. The oldest entry's values always belong to a submitted frame, so the wait always completes; "Drain headroom" below proves that a correct program never reaches it |
| `RequestRelease(entry)`, `RequestReleaseWithDescriptor(entry, space, slot)` | any thread, `RESOURCE` entries; push onto the request queue (the second form records `1 + space` in `m_uDescriptorSpace` and the slot in `m_uIndex`); never blocks |
| `DrainReleaseRequests(uHeadroom)` | render-submit thread, Submit step 4: read `GetLastSignalledValues()` once (after this frame's submissions, so they cover every submission that could have referenced the object: the object had no reference left when the request was made, so no subsequent frame records it); while `GetPendingCount() < uZENITH_RHI_RETIREMENT_CAPACITY − uHeadroom`, pop a request, free its descriptor slot with those values when `m_uDescriptorSpace` is non-zero (`Zenith_RHIDescriptorHeap::Free(space, slot, values)`, lock 450, released before the append), and append the resource entry with those values. The drain never waits; requests that do not fit stay queued, in order, for the next frame's drain, so a mass release (a scene unload freeing 100,000 textures) spreads over frames instead of triggering full-ring waits |
| `RetireCompleted(completed, out)` | render-submit thread at frame begin, or the main thread at a render-idle point: walk the whole ring (not only its head, [Z2-N-REND-489]) with the `completed` values read once by the caller, and for every entry whose values are satisfied: a `RESOURCE` entry calls the listener's `OnResourceRetired(handle)` (lock 450 not held), then is destroyed through the backend (API object destroyed, allocation freed, table slot freed); a `CLIENT` entry is appended to `out`; the ring is compacted in place, preserving order. An entry whose values are all 0 (never used on the GPU) is satisfied at the first call ([Z2-N-REND-491]) |

Complexity: `RetireCompleted` is O(pending) per frame, bounded by 65,536. Lock: `uLOCK_LEVEL_RHI_RETIREMENT` (450) guards the ring and the request queue's consumer side against the device's own destruction path; the render-submit thread is its only regular user, and the main thread uses it only at render-idle points, where the render subgraph cannot run.

### Drain headroom

<!-- elaboration: the headroom constant and its proof -->
```cpp
// Zenith2/Engine/ZenithFlux/Internal/Lifetime/Flux_DeferredDeletion.h
inline constexpr uint32 uFLUX_DEFERRED_DELETE_CAPACITY = uZENITH_RHI_RETIREMENT_CAPACITY;   // Rendering's name ([Z2-N-REND-490])
inline constexpr uint32 uFLUX_RETIRED_KIND_COUNT = 8;                       // client kinds, one batch entry each per frame
inline constexpr uint32 uFLUX_RETIREMENT_DIRECT_RESOURCES_PER_FRAME = 24;   // explicit-value RESOURCE Enqueues per interval (reset at Submit step 4)
inline constexpr uint32 uFLUX_MAX_FRAMES_IN_FLIGHT = 3;                     // Z2-B-RHI-008
inline constexpr uint32 uFLUX_SWAPCHAIN_MAX_IMAGES = uFLUX_MAX_FRAMES_IN_FLIGHT + 1;   // 4; asserted at every swapchain creation and recreation
inline constexpr uint32 uFLUX_RETIREMENT_MAX_SWAPCHAINS = 16;               // uPLATFORM_MAX_WINDOWS, one swapchain per rendering window
inline constexpr uint32 uFLUX_RETIREMENT_RENDER_IDLE_ENTRIES =
	uFLUX_RETIREMENT_MAX_SWAPCHAINS * uFLUX_SWAPCHAIN_MAX_IMAGES + uFLUX_MAX_FRAMES_IN_FLIGHT * 3;   // 16 × 4 + 3 slots × 3 queues' timestamp pools = 73
inline constexpr uint32 uFLUX_RETIREMENT_DIRECT_PER_INTERVAL =
	uFLUX_RETIRED_KIND_COUNT + uFLUX_RETIREMENT_DIRECT_RESOURCES_PER_FRAME + uFLUX_RETIREMENT_RENDER_IDLE_ENTRIES;   // 105
inline constexpr uint32 uFLUX_RETIREMENT_DRAIN_HEADROOM = 512;
static_assert(uFLUX_RETIREMENT_DRAIN_HEADROOM >= uFLUX_MAX_FRAMES_IN_FLIGHT * uFLUX_RETIREMENT_DIRECT_PER_INTERVAL);
```

A direct entry is one appended by `Enqueue` with explicit values; drained entries are the ones `DrainReleaseRequests` appends. An interval is the time from one Submit step 4 to the next. Direct entries have exactly three sources, each bounded per interval:

| Source | Bound per interval | Why |
|---|---|---|
| the frame's client batches at Submit step 3 | `uFLUX_RETIRED_KIND_COUNT` = 8 | one batch per kind per submitted frame; a deferred or failed frame enqueues none |
| `RESOURCE` objects `Flux_DeferredDeletion::Enqueue` receives with explicit values on the render-submit thread (the GPU scene's replaced table buffers, a grown pool's old buffer) | `uFLUX_RETIREMENT_DIRECT_RESOURCES_PER_FRAME` = 24 | `Flux_DeferredDeletion` counts them per interval: the count resets at Submit step 4, right after the drain, which every built frame runs (a frame that submits nothing runs step 4 alone), so "per frame" and "per interval" are the same count; the 25th and every further one of a frame are routed to `RequestRelease` instead, which is always safe because the drain values of the same frame are at or above any explicit value the frame could carry (the frame's own final values), so the object retires no earlier than its last use |
| the main thread's render-idle Enqueues (swapchain recreation's old images, the old timestamp pools of a frames-in-flight change) | `uFLUX_RETIREMENT_RENDER_IDLE_ENTRIES` = 73, per prologue | at most `uFLUX_RETIREMENT_MAX_SWAPCHAINS` = 16 swapchains ([Z2-N-REND-983] per window, `uPLATFORM_MAX_WINDOWS`) with at most `uFLUX_SWAPCHAIN_MAX_IMAGES` = 4 images each, and at most 3 slots × 3 queues of timestamp pools. The image term holds because the image count is F + 1 clamped to the surface's `[minImageCount, maxImageCount]`, and `Flux_SwapchainManager` does `ZENITH_ASSERT(GetInfo().m_uImageCount <= uFLUX_SWAPCHAIN_MAX_IMAGES)` after every creation and recreation; a surface whose `minImageCount` exceeds 4 (no supported device has one) trips it in development builds and, without asserts, can reach the full-ring wait below, never a fatal |

Every other release goes through `RequestRelease`. Proof that no `Enqueue` waits in a correct program, with C = 65,536, H = `uFLUX_RETIREMENT_DRAIN_HEADROOM` and F ≤ 3:

1. A direct entry of an interval carries values at most the final values of the frame submitted in that interval (`Enqueue` asserts values ≤ last signalled; render-idle entries carry the values of frames already drained). `PrepareNextFrame(f)` waits for the final values of the frame last submitted in its slot, F submitted frames earlier, and step 3 retires every entry they satisfy; so when frame f's Submit step 3 enqueues, the pending direct entries belong to at most the last F intervals: E ≤ F × 105 ≤ 315.
2. Drained entries D only enter while the total is below C − H, so at the end of any drain D ≤ C − H; between drains D does not grow.
3. So at every `Enqueue`, total = D + E ≤ (C − H) + 315 ≤ C − 512 + 315 < C: the ring is never full, the wait is never taken, and `RHI.Retirement.FullWaits` stays 0. The full-ring wait remains as the specified full policy ([Z2-N-REND-490]) for a program that breaks a bound, which the assert of step 1 and the per-frame count make detectable.

The assumption of step 1: E ≤ F × 105 assumes that the last F submitted frames span at most F prologues that make render-idle Enqueues — at most one prologue per interval of a submitted frame. The client and direct-resource terms are per submitted frame by construction and by enforcement; the render-idle term is per prologue, and a frame that submits nothing (deferred or failed) still has a prologue while its preparation retires nothing (step 5 of `Zenith2/Docs/Design/RHI.md → "Frame-in-flight rotation"` re-runs no `RetireCompleted`). Frames that submit nothing therefore extend the span: a program that recreates swapchains in more than one prologue of a run of such frames (resize spam during a 30-frame over-cap run) can hold more than 315 pending direct entries and take the full-ring wait. That wait terminates — the oldest entry's values were signalled, which `Enqueue` asserts ([Z2-N-REND-490]) — is counted in `rhi.retirement.full`, is the fifth main-thread wait of `Zenith2/Docs/Design/RHI.md → "Permitted main-thread GPU waits"` when the `Enqueue` is a render-idle one, and is never a fatal. The `static_assert` protects the stated case alone.

`Test_RHIRetirementQueue_MassReleaseInOneFrame` drives exactly this worst case with a fake timeline that completes nothing, and `Test_RHIRetirementQueue_DrainStopsAtHeadroom` checks step 2.

### Flux_DeferredDeletion

`Flux_DeferredDeletion` (the interface of `Zenith2/Docs/Rendering/GpuResources.md → "Retirement"`) is the Flux adapter and implements `Zenith_RHIRetirementListener`. `Enqueue(Flux_RetiredObject, Flux_QueueLastUse)` translates an RHI resource into a `RESOURCE` entry (explicit values on the render-submit thread within the per-frame quota above, else `RequestRelease`) and appends a client object (a table slot, pool slot, upload chunk, material-constants block or view state) to the current frame's batch of its kind; at Submit step 3 each non-empty batch becomes one `CLIENT` entry (`m_uClientKind` = `FLUX_RETIRED_KIND_INSTANCE_SLOT`, `_PRIMITIVE_SLOT`, `_MATERIAL_SLOT`, `_LIGHT_SLOT`, `_POOL_SLOT`, `_UPLOAD_CHUNK`, `_MATERIAL_CONSTANTS`, `_VIEW_STATE`; `m_uIndex` = the batch's index in Flux's batch pool; `m_ulPayload` = its entry count) enqueued with the frame's final values. `RetireCompleted(Flux_QueueCompletedValues)` calls the RHI queue and returns each retired batch's objects to the free lists their kind names, in order, then returns the batch buffer to the pool (a `Zenith_Array` per batch, capacity kept, from the general heap). `OnResourceRetired(handle)` forwards to `Flux_ImportedStateTable::Unregister(handle)` (a no-op for a handle with no entry), which is how an imported resource's state leaves the table before its slot can be reused (`Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Imported-state table"`). Thread affinity: `RENDER_SUBMIT_THREAD`, or `MAIN_THREAD` at a render-idle point, for `Enqueue` with explicit values and `RetireCompleted`; `Enqueue` of a resource without explicit values is `ANY_THREAD` (it is a `RequestRelease`).

## Descriptor heap and slot allocation

`Zenith_RHIDescriptorHeap` owns, per index space:

<!-- elaboration -->
| Structure | Detail |
|---|---|
| free list | a LIFO stack of `uint32` indices, initialised with `capacity − 1 … 1` (slot 0 is never on it, [Z2-N-REND-031]) |
| pending list | a ring of `{index, Zenith_RHIQueueValues}` (32 bytes), capacity equal to the space capacity (a slot is pending at most once) |
| occupancy | `capacity − 1 − free − pending`, published as `RHI.Descriptors.Occupancy.<Space>` |

| Operation | Rule |
|---|---|
| `Allocate(space)` | any thread; lock 450; pop the free list; empty: `ZENITH_RHI_ERROR_DESCRIPTOR_SPACE_FULL`, counter `Flux.Descriptors.Exhausted` (incremented by `Flux_DescriptorAllocator`), no eviction ([Z2-N-REND-060]) |
| `Write` | the backend's write (Vulkan.md "Descriptor heap realisation", D3D12.md "Descriptor heaps"); the slot's generation of writes is tracked in development builds, and a second write before `Free` is `ZENITH_ASSERT` ([Z2-N-REND-052]) |
| `Free(space, slot, lastUse)` | render-submit thread, or the main thread at a render-idle point; lock 450; append to the pending list with `lastUse` |
| `RetireCompleted(completed)` | render-submit thread at frame begin, or the main thread at a render-idle point; move every satisfied pending slot to the free list ([Z2-N-REND-059]); on Vulkan the slot's image view is destroyed here |

`Free` has exactly three callers, each on a thread that knows the final values it passes:

| Caller | Slot | Values |
|---|---|---|
| Submit step 3 (render-submit thread) | the frame's transient view descriptors | the frame's final values |
| `DrainReleaseRequests` at Submit step 4 (render-submit thread) | a slot released with its resource through `RequestReleaseWithDescriptor` (persistent textures and buffers, persistent render targets and histories) | the drain values, which are the resource's own last-use values |
| a render-idle point (main thread) | slots of objects a frames-in-flight change or a recreation releases | `GetLastSignalledValues()` after the point's wait |

This is how [Z2-N-REND-059] holds for a slot freed from any thread: the thread that drops the last reference (an asset loader job at reference count zero) never frees the slot itself; it requests the release, and the slot enters its space's pending list with the per-queue last-use values of its resource — the values after the submissions of the frame in which the request was drained, which include every frame that could still read the slot — and returns to the free list only after every queue passed them. `Test_RHIDescriptorHeap_ReleasedWithResourceTakesDrainValues` asserts it.

`Flux_DescriptorAllocator` (`Zenith2/Docs/Rendering.md → "Binding model"`, heading "Descriptor slot allocation") is the Flux wrapper: `Allocate` maps `DESCRIPTOR_SPACE_FULL` to `FLUX_DESCRIPTOR_ERROR_SPACE_FULL`, `Free` converts `Flux_QueueLastUse` (render-submit thread), `GetOccupancy` forwards. Graph-patched descriptors of transient views (one per view per frame) are allocated and written on the `Flux.BuildGraph` node after the frame's upload reservation is final ("Build-node upload order", below; [Z2-N-REND-052]) and freed with the frame's final values at Submit step 3, so a transient's descriptor retires with its frame; a deferred or failed frame allocates none.

## Upload ring internals

The ring's interface, sizing and steps are `Zenith2/Docs/Rendering/GpuResources.md → "Upload ring"`. This design specifies the data structures, where each step runs, and the algorithms.

```cpp
// Zenith2/Engine/ZenithFlux/Internal/Upload/Flux_UploadRingInternal.h
struct Flux_UploadChunk { Zenith_RHIBufferHandle m_xBuffer; Zenith_RHIDescriptorHandle m_xSRV; uint8* m_pMapped; uint64 m_ulBytes; uint64 m_ulLastUsedFrame; };
struct Flux_UploadSegment            // one per frame slot
{
	uint32 m_uChunk;                 // the chunk the reservation lives in (a reservation never spans chunks)
	uint64 m_ulBegin;                // offset of the reservation in the chunk
	uint64 m_ulReserved;             // bytes reserved for the frame
	std::atomic<uint64> m_ulCursor;  // bump cursor, relative to m_ulBegin
	uint64 m_ulFrameIndex;
	bool m_bFinal;                    // set by ExtendReservation on FITS or EXTENDED; Allocate and GetFrameData require it
};
struct Flux_StagedRange              // ring space holding a serviced upload request's piece
{
	uint32 m_uChunk;
	uint64 m_ulBegin;
	uint64 m_ulBytes;
	Flux_QueueLastUse m_xLastUse;     // stamped at Submit step 3 of the frame whose StreamingUpload pass copies it; 0 until then
};
struct Flux_FrameDataRange { uint32 m_uIndex = 0; };   // a ticket from DeclareFrameData, valid for its frame

enum Flux_ReservationOutcome : uint8
{
	FLUX_RESERVATION_OUTCOME_FITS = 1,      // the prepared reservation already covers the exact need
	FLUX_RESERVATION_OUTCOME_EXTENDED,      // extended in place or through a new chunk below the cap
	FLUX_RESERVATION_OUTCOME_DEFERRED,      // the need cannot be met without a wait: the frame is deferred
};

// Additions to Flux_UploadRing (Zenith2/Docs/Rendering/GpuResources.md "Upload ring" declares the rest).
// Build node, graph setup: records a per-frame data need; allocates and writes nothing. szOwner is a string literal
// (a pass name, or "GPUSceneScatter") used by the over-cap log of "Frame failures".
Flux_FrameDataRange DeclareFrameData(uint64 ulBytes, uint32 uAlignment, const char* szOwner);
uint64 GetDeclaredFrameDataBytes() const;   // the aligned sum, part of the compile's exact need
// Build node, after compile; never waits; takes no lock (the segment is the build node's alone until record starts).
// ZENITH_ASSERT(m_ulCursor == 0 && !m_bFinal): nothing of the frame was allocated before it ("Build-node upload order").
// On FITS or EXTENDED: sets m_bFinal and allocates the frame-data block (the declared ranges, in declaration order) at the
// start of the reservation. Errors: FLUX_UPLOAD_ERROR_TOO_LARGE when ulExactBytes exceeds Z2-B-RHI-006 (a frame failure,
// "Frame failures"; never a deferral); FLUX_UPLOAD_ERROR_DEVICE_LOST.
Zenith_Result<Flux_ReservationOutcome, Flux_UploadError> ExtendReservation(uint64 ulExactBytes);
// Build node, after ExtendReservation returned FITS or EXTENDED: the declared range's mapped span, buffer and offset.
// ZENITH_ASSERT(m_bFinal).
Flux_UploadAllocation GetFrameData(Flux_FrameDataRange xRange);
uint64 PredictReservation() const;          // render-submit thread: the step-2 prediction
Flux_RGBuffer ImportFrameChunk(Flux_RenderGraph& xGraph);   // setup: the frame reservation's chunk, bound at step 4
```

<!-- elaboration: the prediction rule, the compile-time extension, the build-node order, the deferral and the failure -->
| Where | Step |
|---|---|
| render-submit, `PrepareNextFrame(f)` | 1 `BeginFrame`: the slot's segment is released (its frame completed, the slot wait of "Frame-in-flight rotation"); staged ranges whose `m_xLastUse` is non-zero and satisfied are released; chunks unused for 600 frames above the initial capacity retire through the retirement queue (0 frames under memory pressure, action 4 of "Memory pressure responder") |
| same | 2 `ReserveFrame(n)`: n is the exact need recorded by `DeferFrame` when frame f − 1 was deferred, else `PredictReservation()` = the maximum of the compile's exact need over the last 8 built frames, × 1.25, + 1 MiB, clamped to `Z2-B-RHI-006` (the initial capacity on the first frame); bump-reserve from a chunk with room; else grow by one chunk of max(16 MiB, n) while the ring is below `Z2-B-RHI-006`; else wait for the oldest in-flight segment's frame (`WaitCPU`) and retry, incrementing `Flux.Upload.Exhausted` and accumulating `Flux.Upload.StallMs`. n never exceeds the cap (a need above it is a frame failure, never a deferral), so the `FLUX_UPLOAD_ERROR_TOO_LARGE` of [Z2-N-REND-495] is unreachable here and asserted |
| same | 3 service the request queue (below) within the space left under the cap; each staged piece gets a `Flux_StagedRange` recorded against no slot, so a slot reset (a frames-in-flight change) leaves it intact; a staged range whose frame submits nothing (deferred or failed) is carried to the next frame that submits, whose `StreamingUpload` pass records its copy and whose Submit step 3 stamps `m_xLastUse` |
| build node | 4 the order of "Build-node upload order" below, ending in `ExtendReservation(exact)` and the frame-data writes. This step never waits |
| any frame thread | 5 `Allocate(size, align)` (record jobs' parameter structs): `ZENITH_ASSERT(m_bFinal)`; `m_ulCursor.fetch_add(alignedSize, relaxed)` with a pre-check loop so an over-run returns `FLUX_UPLOAD_ERROR_RESERVATION_EXHAUSTED` without moving the cursor ([Z2-N-REND-496]) |

### Build-node upload order

<!-- elaboration: the invariant the deferral depends on -->
The invariant: no `Allocate` of frame f, and no write into frame f's ring memory, precedes frame f's `ExtendReservation`. The deferral depends on it twice: an extension served by growing a new chunk moves the reservation to that chunk (a reservation never spans chunks), which is sound only while nothing has been bumped in the old segment; and an `Allocate` measured against the predicted rather than the exact reservation could fail `FLUX_UPLOAD_ERROR_RESERVATION_EXHAUSTED` (a programmer error, [Z2-N-REND-496]) where the frame should have been deferred. The `Flux.BuildGraph` node therefore runs, in this order:

| Order | Action | Ring access |
|---|---|---|
| 1 | `Flux_GPUScene::ApplyDeltas(world)` merges the render world's deltas into the pending store | none |
| 2 | graph setup: every pass declares; every build-node producer of per-frame data (the GPU scene's scatter records and data, shadow-view records, cluster configuration, RPROD's culling entries) calls `DeclareFrameData(bytes, align, owner)` and keeps the ticket; `AddScatterAndGrowthPasses` sorts and run-merges the pending records into frame-arena arrays and declares their bytes; passes read ring data through `ImportFrameChunk`, a read-only import whose access never changes (host-written before submit), so no barrier depends on which chunk step 4 binds | none |
| 3 | compile steps 1 to 7; exact need = the parameter bytes of [Z2-N-REND-496]'s sizing rule + `GetDeclaredFrameDataBytes()` | none |
| 4 | `ExtendReservation(exact)`: `FITS` or `EXTENDED` → order 5; `DEFERRED` → "Frame deferral"; `FLUX_UPLOAD_ERROR_TOO_LARGE` → "Frame failures". A frame leaving here by deferral or failure has written no ring byte | the reservation only |
| 5 | the producers write through `GetFrameData(ticket)`; the transient view descriptors are allocated and written ([Z2-N-REND-052]); the frame-dump model is copied | frame-data block |
| after | `Flux.Record` jobs `Allocate` parameter structs | bump |

Mechanism: `ExtendReservation` asserts that the cursor is 0 and the segment not final, and `Allocate` and `GetFrameData` assert that it is final, so any producer that writes early fails at its own call site. `Test_UploadRing_AllocateBeforeExtendAsserts` (`z2-death-assert`) violates the invariant (an `Allocate` on the build node before `ExtendReservation`) and `Test_UploadRing_DeferredFrameReservesExactNext` asserts that a deferred frame's segment wrote no byte (cursor 0, a poisoned ring range unchanged).

### Frame deferral

A deferred frame is one whose exact need could not be met without a wait: `ExtendReservation` returns `DEFERRED`, the build node records the need through `Flux_FrameLifetime::DeferFrame(ulNeed)`, calls `Flux_GPUScene::OnFrameDeferred()` and `Flux_ReadbackPool::OnFrameSubmittedNothing(FLUX_READBACK_ERROR_FRAME_DEFERRED)`, the frame records and submits nothing, `Flux.Upload.ReservationDeferredFrames` increments, and the next `PrepareNextFrame` reserves that exact amount, waiting on the render-submit thread as step 2 describes. Waits therefore happen on the render-submit thread alone, and a frame's allocations always fit its reservation. `Z2-B-RHI-007` (upload ring stall frames) counts frames with a stall or a deferral (pending X79).

The deferral decision is made after `Flux_GPUScene::ApplyDeltas` has run, so the GPU scene keeps every mutation of a deferred frame pending and intact ("Deferred frames and the pending store", below): nothing a render world carried as a delta is lost, and replaying the delta stream still reproduces the tables ([Z2-N-REND-008]). What a deferred frame does to everything else:

| Element | Effect of a deferred frame f |
|---|---|
| render-world slot | consumed as usual (its deltas now live in the GPU scene's pending store); GPU particle emitter commands move to the particle system's pending command list and apply in the next submitted frame; skinned poses, view requests, UI lists and debug draws are per-frame state, replaced by frame f + 1's |
| frame index and frame slot | the frame index advances; the slot does not (`m_ulSubmittedFrames` is unchanged): frame f + 1 prepares the same slot again, and the slot record's final values stay those of the frame last submitted in that slot |
| readbacks | every readback declared in f moves to `FAILED` with `FLUX_READBACK_ERROR_FRAME_DEFERRED` and its pool range is freed at once, because no copy was recorded; the requester re-requests in a subsequent frame, the shape of `POOL_FULL` ("Readback state machine") |
| view histories | not written: the ping-pong index does not swap and `m_bHistoryValid` is unchanged |
| temporal state | the tables are preserved exactly (the invariant of "Deferred frames and the pending store"); temporal state is not: an instance updated in f and again in f + 1 is staged with previous transform T(f) and current T(f + 1), while the last rendered frame showed T(f − 1), so its velocity in frame f + 1 spans one frame instead of two, for that frame alone (a bounded error the TAA and motion-blur owners accept; X90) |
| transient view descriptors | neither allocated nor written: they come at order 5, after the decision |
| imported-state table | `Commit` does not run; every entry keeps the state of the last submitted frame, which is the state the GPU last left it in |
| staged upload requests | kept staged; carried to the next frame that submits (step 3) |
| present and timestamps | nothing is presented (the previous image stays on screen); no timestamps are written, and the profiler shows the frame as deferred |
| release requests | drained as usual at Submit step 4 (with the last-signalled values, which the deferred frame did not change) |

### Frame failures

<!-- elaboration: the frame-failure path, shared by the over-cap need and device-memory exhaustion -->
```cpp
// Zenith2/Engine/ZenithFlux/Internal/Lifetime/Flux_FrameLifetime.h
enum Flux_FrameFailure : uint8
{
	FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP = 1,   // the compile's exact upload need exceeds Z2-B-RHI-006
	FLUX_FRAME_FAILURE_DEVICE_OOM,            // a transient group heap or history could not be allocated ("Device memory exhaustion")
};
inline constexpr uint32 uFLUX_MAX_CONSECUTIVE_FAILED_FRAMES = 30;
```

A failed frame is cancelled: nothing is recorded, submitted or presented, and every element of the deferral table behaves as for a deferred frame (the pending store kept, histories untouched, no transient descriptor, no `Commit`, staged requests carried), with four differences:

| Element | A failed frame |
|---|---|
| the build node | calls `Flux_FrameLifetime::FailFrame(eReason)`, `Flux_GPUScene::OnFrameDeferred()` and `Flux_ReadbackPool::OnFrameSubmittedNothing(FLUX_READBACK_ERROR_FRAME_FAILED)` |
| the next preparation | reserves `PredictReservation()` (clamped to the cap), never the failed need: a reservation above the cap cannot exist, so an over-cap frame is never a deferral |
| counters and log | `FLUX_FRAME_FAILURE_UPLOAD_OVER_CAP`: `RHI.Upload.OverCap` increments and one `flux.upload` error line `upload need <n> bytes above the cap <cap> bytes; largest contributor '<owner>' (<bytes> bytes)` names the pass or frame-data owner with the most bytes; the line is logged once per contributor (a `Zenith_HashSet<uint64>` of logged owner-name hashes of capacity 256, after which nothing more is logged while the counter still counts), which is the "once per asset" rule. `FLUX_FRAME_FAILURE_DEVICE_OOM`: `Flux.Memory.OOMSkippedFrames` increments |
| consecutive failures | `m_uConsecutiveFailedFrames` counts failed frames and is reset to 0 by a submitted frame (a deferred frame neither counts nor resets); the `FailFrame` that raises it to `uFLUX_MAX_CONSECUTIVE_FAILED_FRAMES` = 30 also stores `m_eExitRequest = eReason` (release, on the build node), the frame-failure exit request |
| the exit | the device is not lost, so neither `OnDeviceLost` nor the device-loss flag fires; the hand-off is the request itself: `Flux_DeviceLossHandler::RunPrologue()` (main thread, prologue step 4; `Zenith2/Docs/Design/RHI.md → "Render-idle points"`) reads `Flux_FrameLifetime::GetExitRequest()` at the first prologue after that frame and takes the `NORMAL` → `EXITING` row "exit requested by a frame failure" of `Zenith2/Docs/Design/RHI/Recovery.md → "Device-loss state machine"`: the autosave request, the reason's message (`The renderer could not fit a frame's uploads into memory. Your progress has been saved.` for the over-cap reason, `The graphics device ran out of memory. Your progress has been saved.` for device OOM) through the platform's message box, shutdown with `EXIT_CODE_DEVICE_LOST` (6, X72 with the clause of "Reconciliation items" R3) and the 10 s fatal fallback; no device-loss report is written, `RHI.Device.Losses` and `RHI.Device.Recoveries` are unchanged, and the reason (the last failure's) and the count go to telemetry. From that prologue on `GetDeviceLossState()` is `EXITING`, so no further frame builds |

An over-cap need does not shrink by itself (the pending store keeps every delta, and the passes are the same next frame), so the bounded count is what turns it into a defined exit rather than a loop. `Test_UploadRing_NeedAboveCapFailsFrame` asserts the cancel, the counter, the single log line per contributor, the prediction-sized next reservation, the reset by a submitted frame, and the exit after 30 frames: the request stored by the 30th `FailFrame`, the `EXITING` state at the next prologue, the over-cap message, exit code 6, no device-loss report written and `RHI.Device.Losses` unchanged.

### Upload request state machine

| State | Event | Next | Action |
|---|---|---|---|
| `QUEUED` | serviced, fits | `STAGED` | copy the source (or the next piece) into ring space; record the copy into the frame's `StreamingUpload` pass on the copy queue; completion value = that batch's value |
| `QUEUED` | serviced, splittable, partial fit | `QUEUED` | stage the pieces that fit (piece size = `m_ulSplitGranularity` multiples), remember the offset; `SetProgress(staged / total)` |
| `QUEUED` | serviced, not splittable, larger than the cap | `FAILED` | `FLUX_UPLOAD_ERROR_TOO_LARGE`, BR-REFUSE ([Z2-N-REND-495]) |
| `QUEUED` | serviced, does not fit this frame | `QUEUED` | nothing |
| `QUEUED` | `Cancel()` | `CANCELLED` | removed at the next service; staged pieces of a split request are discarded with their frame |
| `STAGED` | copy timeline ≥ completion value (checked at `PrepareNextFrame`) | `COMPLETE` | the destination becomes visible through the one-time acquire list ([Z2-N-REND-548]) |
| `STAGED` | `Cancel()` | `STAGED` | honoured only while `QUEUED`: the copy completes and its result is discarded (the destination region is returned to its allocator) |
| any pending | device loss | `FAILED` | `FLUX_UPLOAD_ERROR_DEVICE_LOST` |

`Flux_UploadOperation` wraps a `Zenith_OperationHandle` issued from the ring's `Zenith_OperationTable` (capacity 4,096, the request-queue capacity; `Zenith2/Docs/Design/CORE/Threading.md → "Zenith_OperationHandle implementation"`): `QUEUED` ↔ `OPERATION_STATUS_PENDING`, `STAGED` ↔ `RUNNING`, `COMPLETE` ↔ `SUCCEEDED`, `FAILED` ↔ `FAILED` (failure code = the `Flux_UploadError`), `CANCELLED` ↔ `CANCELLED`. The request queue is a bounded MPMC `Zenith_BoundedQueue` of 4,096 entries with `QUEUE_FULL_POLICY_REJECT` ([Z2-N-REND-497]); the requester's source span stays valid until `STAGED` and is not read afterwards. Cancellation points: while queued, and between pieces of a split request; a cancelled operation releases its staged pieces' destination regions before it reports `CANCELLED` ([Z2-N-CONTRACT-335]).

### Splitting algorithm

For a splittable request of `n` bytes with granularity `g`, serviced with `f` bytes free under the cap: stage `min(n − staged, floor(f / g) × g)` bytes this frame (at least `g` when `f ≥ g`, none otherwise); a texture request's granularity is one mip (or, for a mip larger than 4 MiB, one tile row of 64 texel rows), a buffer's is 64 KiB. The pieces are recorded as consecutive copies into the same destination, so `COMPLETE` is reached when the last piece's copy completes.

## Readback pool

The readback interface and rules are `Zenith2/Docs/Rendering/GpuResources.md → "Readbacks"`.

<!-- elaboration: the pool's sizes, granularity and operation count -->
The pool (`ZenithFlux/Internal/Readback/Flux_ReadbackPool.h`) is one `READBACK`-class buffer of `Z2-B-RHI-011` bytes (pending; proposed 64 MiB desktop, 16 MiB mobile), sub-allocated by a first-fit free-range list at 256-byte granularity, plus an operation table of 1,024 operations. Besides `AddReadback` it has one member this design adds, `void OnFrameSubmittedNothing(Flux_ReadbackError eCode)`, called on the build node of a deferred or failed frame with `FLUX_READBACK_ERROR_FRAME_DEFERRED` or `FLUX_READBACK_ERROR_FRAME_FAILED`; it fails every readback that frame declared, as the state machine below states.

### Readback state machine

| State | Event | Next | Action |
|---|---|---|---|
| (none) | `AddReadback(name, source, range)` on the build node | `PENDING` | allocate the range from the pool (none free: `FLUX_READBACK_ERROR_POOL_FULL`, `Flux.Readback.Deferred` increments, the requester retries next frame: BR-DEFER); a range above the pool size: `FLUX_READBACK_ERROR_TOO_LARGE`; declare a readback pass (a copy into the pool range) |
| `PENDING` | the frame's batch completed (checked at `PrepareNextFrame`) | `READY` | `GetData()` returns the mapped span |
| `PENDING` | the declaring frame is deferred (`OnFrameSubmittedNothing(FLUX_READBACK_ERROR_FRAME_DEFERRED)`, build node) | `FAILED` | failure code `FLUX_READBACK_ERROR_FRAME_DEFERRED`; the range is freed at once, because no copy was recorded; the requester re-requests (BR-DEFER, the shape of `POOL_FULL`); `Flux.Readback.Deferred` increments |
| `PENDING` | the declaring frame fails ("Frame failures") | `FAILED` | failure code `FLUX_READBACK_ERROR_FRAME_FAILED`; as the row above |
| `PENDING` | `Cancel()` | `CANCELLED` | the range is freed once the copy's frame retires ([Z2-N-REND-501]) |
| `READY` | `Release()` or handle destruction | (freed) | the range returns to the free list |
| `PENDING` | device loss | `FAILED` | [Z2-N-REND-501] |

A readback is never re-declared into another frame: its copy source is a virtual handle of the graph that declared it, which does not exist in the next frame (the graph is rebuilt every frame, Z2-D-055). A readback whose result feeds simulation is published by its consumer at a tick boundary in completion-ID order ([Z2-N-REND-500]); the pool exposes the completion ID (`Zenith_OperationHandle::GetIssueID()`). At renderer shutdown every handle not yet released is logged with its request name as a leak and its range freed ([Z2-N-REND-499]).

## Residency reporting

<!-- elaboration: the steps below, and the 95% critical threshold of step 5 -->

`Flux_GpuMemoryReporter` (`ZenithFlux/Internal/Memory/`) runs at `PrepareNextFrame`:

1. `QueryHeapBudgets` into a 16-entry stack array; `GetDeviceLocalUsageBytes`.
2. Sum `Zenith_RHIAllocationStats` per category: render targets and transients, GPU scene tables, pools, streamed textures, meshes, upload ring, PSOs, other ([Z2-N-REND-504]).
3. Publish the counters of the entry document's "Counters" table and the on-screen profiler's memory panel.
4. Report to the governor: `Flux_MemoryConsumer::GetResidentBytes()` returns the rendering subsystem's resident bytes (the sum of step 2), and the composition root's prologue calls `Zenith_MemoryGovernor::ReportProcessUsage(process, deviceLocalUsage)` with the RHI's device-local usage (`Zenith2/Docs/Architecture.md → "Memory governor"`, table "Pressure levels").
5. Per-heap cap `Z2-B-RHI-003`: when any device-local heap's usage exceeds its budget (the API's budget, or the row once adopted, whichever is lower), `Zenith_MemoryGovernor::ReportOSPressure(MEMORY_PRESSURE_WARNING)`; above 95% of it, `CRITICAL`. The row is a per-heap cap the RHI enforces and is not summed into the subsystem budget ([Z2-N-ARCH-823] table).

## Memory pressure responder

`Flux_MemoryConsumer` implements `Zenith_MemoryConsumer` for `MEMORY_SUBSYSTEM_RENDERING` and is registered by the composition root at startup step 13. `OnPressureLevelChanged(eOld, eNew)` runs on the main thread in the frame prologue and sets `Flux_PressureState { Zenith_MemoryPressureLevel m_eLevel; uint64 m_ulChangedFrame; }` (`Zenith2/Docs/Architecture.md → "Memory governor"`), which each owning component reads at its next use; the actions and their mechanisms are exactly the thirteen of `Zenith2/Docs/Rendering/GpuResources.md → "GPU memory rules"` (heading "Responses to memory pressure", [Z2-N-REND-506]), applied within one frame of the level change and reverted one frame after the level falls. This area owns the dispatch and actions 2, 3, 4, 7 and 13 (graph caches, heaps, upload ring, persistent render targets); actions 1, 5 and 8 are read by the texture streamer, 6, 9, 10, 11 and 12 by the view system and the quality overrides (RPROD, LIGHT), each reading the same state.

## Device memory exhaustion

<!-- elaboration -->
| Failure | Behaviour |
|---|---|
| persistent resource creation (`Flux_PersistentResources::CreateTexture`, `CreateBuffer`) returns `OUT_OF_DEVICE_MEMORY` | `FLUX_RESOURCE_ERROR_OUT_OF_DEVICE_MEMORY` to the asset loader, which leaves the asset non-resident and retries after the next level change; `ReportOSPressure(CRITICAL)`; counter `Flux.Memory.DeviceOOM` |
| a transient group heap or a history cannot be allocated at compile | `ReportOSPressure(CRITICAL)`; the graph releases every unused cached physical resource (action 7 immediately), retries the allocation once; still failing: the frame fails with `FLUX_FRAME_FAILURE_DEVICE_OOM` ("Frame failures": nothing recorded or presented, histories untouched, the pending store kept), `Flux.Memory.OOMSkippedFrames` increments |
| 30 consecutive failed frames | the frame-failure exit of "Frame failures" with reason `FLUX_FRAME_FAILURE_DEVICE_OOM`: the `NORMAL` → `EXITING` row of `Zenith2/Docs/Design/RHI/Recovery.md → "Device-loss state machine"`, the message `The graphics device ran out of memory. Your progress has been saved.`, the autosave, `EXIT_CODE_DEVICE_LOST` (6, pending X72), no device-loss report; the count is shared with the over-cap failure |
| an upload-ring chunk cannot be allocated below the cap | treated as the cap: the stall path of step 2 |
| host allocation failure inside the API or VMA/D3D12MA callbacks | `Zenith_Heap`'s OOM handler (`Zenith2/Docs/Contracts/Memory.md` → "OOM handler"); the API call returns `OUT_OF_HOST_MEMORY`, surfaced as `ZENITH_RHI_ERROR_OUT_OF_HOST_MEMORY` |

## GPU scene implementation

The GPU scene's records, scatter protocol, growth, persistent resources, pools and streaming swap are `Zenith2/Docs/Rendering/GpuResources.md` headings "GPU scene tables" to "Texture streaming swap". This area implements them (`Z2-I-RHI-08`) in `Zenith2/Engine/ZenithFlux/Internal/GPUScene/`.

### Flux_GPUScene

```cpp
class Flux_GPUScene        // one per device; owned by Flux_Renderer
{
public:
	static Zenith_Result<Zenith_Owned<Flux_GPUScene>, Flux_GPUSceneError> Create(Zenith_RHIDevice& xDevice, Flux_PersistentResources& xResources, Zenith_RHICapabilityTier eTier, uint64 ulMaxTableBytes, Zenith_Heap& xHeap);
	void ApplyDeltas(const Flux_RenderWorld& xWorld);                                  // build node: merges the deltas into the pending store
	Flux_FrameShared ImportTables(Flux_RenderGraph& xGraph);           // write-restricted imports ([Z2-N-REND-536])
	void AddScatterAndGrowthPasses(Flux_RenderGraph& xGraph, Flux_UploadRing& xRing); // setup: declares the passes and DeclareFrameData; none when nothing changed ([Z2-N-REND-543])
	void WriteScatterData(Flux_UploadRing& xRing);                                     // build node, order 5 of "Build-node upload order": GetFrameData, then the writes
	void OnFrameSubmitted(const Flux_QueueLastUse& xFinalValues);                      // Submit step 3: commits the uploaded pending state
	void OnFrameDeferred();                                                            // the frame submits nothing (deferred or failed): everything stays pending
	uint32 GetLiveCount(Flux_GPUSceneTable eTable) const;
	uint64 GetTableBytes() const;                                       // for Z2-B-RPROD-001
	void SetRangeListener(Flux_GPUSceneRangeListener* pxListener);                    // main thread, render-idle; "Material-slot range listener"
};

enum Flux_GPUSceneError : uint8 { FLUX_GPU_SCENE_ERROR_TABLE_FULL = 1, FLUX_GPU_SCENE_ERROR_OUT_OF_DEVICE_MEMORY, FLUX_GPU_SCENE_ERROR_OUT_OF_HOST_MEMORY };
enum Flux_GPUSceneTable : uint8 { FLUX_GPU_SCENE_TABLE_INSTANCE = 0, FLUX_GPU_SCENE_TABLE_PRIMITIVE = 1, FLUX_GPU_SCENE_TABLE_MATERIAL_SLOT = 2, FLUX_GPU_SCENE_TABLE_LIGHT = 3 };
```

A world whose `m_bFullState` is set (the render-world resync of `Zenith2/Docs/Rendering.md → "Render world resync"`, used after a frame-executor cancel and by device-loss recovery through `Flux_RenderWorldRing::RequestFullExtraction`) is consumed by the same `ApplyDeltas`: it first discards the CPU-side slot state and the pending store, then re-allocates every slot from the full-state records and stages every record (step 4 of that table), so the tables equal a delta replay from the first frame.

<!-- elaboration: the maximum-capacity shares -->
Tables: four `DEVICE_LOCAL` buffers with usage `STORAGE_READ | STORAGE_WRITE | TRANSFER_SRC | TRANSFER_DST`, initial capacities per [Z2-N-REND-537] (262,144 / 65,536 / 65,536 / 4,096 records on desktop, a quarter on mobile), each registered in the imported-state table flagged write-restricted (at renderer creation, a render-idle point, and for a grown buffer on the build node). The maximum capacity of table t is `floor(share(t) × ulMaxTableBytes / recordSize(t))` with shares 0.60, 0.25, 0.10, 0.05 of `Z2-B-RPROD-001`.

Extension point (Stage 5): a fifth table — LIGHT's 16-byte instance-lighting record, one per instance slot (cross-area note L14; cross-document item X106, pending its ruling) — is appended to `Flux_GPUSceneTable` as `FLUX_GPU_SCENE_TABLE_INSTANCE_LIGHTING = 4`, indexed by the instance index with the instance table's initial capacity; it takes its share out of the instance share (0.60 split 128 : 16, so both reach the same maximum slot count), and extends the pending-store bound below, the `Create` failure-injection steps and `Test_GPUScene_InitialCapacities` in the ticket that lands it (`Zenith2/Docs/Design/RHI.md → "Stage 5"`). The enumerator, the share list, the pending-store bound, the `Create` steps and that test are the only places that count the tables.

### Slot allocators

Each table's slots come from a `Flux_TableSlotAllocator` (a LIFO free list of `uint32`, a live bit array); a freed slot is appended to the pending store's free list of its table; when the frame that uploads the free is submitted, the slots join that frame's client batch of their kind (one `CLIENT` retirement entry per kind per frame, "Retirement queue") and return to the free list only when that batch retires ([Z2-N-REND-538], the range-freeing of UE5 the specification cites). Instances map to slots through `Zenith_HashMap<Flux_RenderInstanceKey, uint32>`; primitives and material slots are allocated per mesh and per (mesh, material assignment) with reference counts, so instances sharing a mesh and assignment share slots. The creation and release of each material-slot range is reported to the range listener of "Material-slot range listener" below.

### Material-slot range listener

<!-- elaboration: cross-area row RPB23. RPROD's Flux_MaterialRangeForwarder is the one implementation; GEOM's Flux_MaterialSlotCache, behind it, fills the records and owns their buckets -->
A material-slot range is the S consecutive material-slot records the GPU scene allocates for one (mesh, material assignment) key ("Slot allocators"): S is the mesh's section count and the key holds the S material IDs after the instance delta's overrides. The GPU scene owns each range's allocation, reference count, staging and retirement, and none of its contents. It reports every range it creates and frees through one borrowed listener, declared beside the class in `Zenith2/Engine/ZenithFlux/Internal/GPUScene/Flux_GPUScene.h`:

```cpp
/// @contract interface Flux_GPUSceneRangeListener
/// Ownership: Implemented outside RHI's files (by RPROD's Flux_MaterialRangeForwarder, Zenith2/Docs/Design/RPROD/Renderer.md
///   "Render world consumption"); borrowed by Flux_GPUScene from SetRangeListener(pxListener) until SetRangeListener(nullptr),
///   which the owner calls before the listener is destroyed and before the GPU scene is destroyed ([Z2-N-CONTRACT-325]).
///   Held as a member of the GPU scene, never in global state.
/// Thread affinity: Both members run on the thread that runs the GPU scene member that calls them: Flux_GPUScene::ApplyDeltas
///   on the Flux.BuildGraph node (JOB_AFFINITY_ANY), and, in tests alone, the Flux_GPUSceneTestAccess call on the test thread.
///   Never two calls at once, and never concurrently with any other Flux_GPUScene member.
/// Invalidation: axMaterials and axImages are valid only for the duration of the call; the listener keeps no pointer into either.
/// Units: None. uFirstSlot is a material-slot table index; the range is [uFirstSlot, uFirstSlot + axMaterials.Size()), and
///   axImages.Size() == axMaterials.Size() == S (1 to 255).
/// Errors: None; the listener cannot fail. An image it leaves untouched keeps the not-drawn image (below).
/// Reentrancy: NONE. A callback may not call any Flux_GPUScene member, SetRangeListener included (asserted).
/// Version: since Z2 Stage 3 (elaboration: cross-area row RPB23).
/// @endcontract
class Flux_GPUSceneRangeListener
{
public:
	virtual ~Flux_GPUSceneRangeListener() = default;
	// Once per range created while this listener is set, after its S slots are allocated and before its S records are
	// staged; axImages arrives filled with the not-drawn image and is staged as the listener leaves it.
	virtual void OnMaterialRangeCreated(Flux_MeshID xMesh, Zenith_Span<const Flux_MaterialID> axMaterials, uint32 uFirstSlot, Zenith_Span<Flux_MaterialSlotRecord> axImages) = 0;
	// Once per range whose creation was reported to this listener, when its reference count reaches zero, after its S
	// slots joined the pending free list (they are reused only after their client batch retires, [Z2-N-REND-538]).
	virtual void OnMaterialRangeFreed(uint32 uFirstSlot) = 0;
};
```

`Flux_MaterialSlotRecord` is the generated shared struct of `Zenith2/Shaders/Common/GPUScene.slang` (RPROD's module); the not-drawn image is that record with every field 0 except `m_uDepthBucket`, `m_uColourBucket` and `m_uShadowBucket`, which hold `uFLUX_RG_INVALID` (`Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md`), so a slot whose image no listener filled is drawn in no bucket.

| `SetRangeListener(pxListener)` | Rule |
|---|---|
| thread and timing | `MAIN_THREAD` at a render-idle point (`Zenith2/Docs/Design/RHI.md → "Render-idle points"`). The renderer sets it in `Flux_Renderer::Create` step 7, after the GPU scene's `Create` (step 4) and before the first frame, and clears it with `SetRangeListener(nullptr)` in the unwinding of step 7 and in the renderer's destructor, before it destroys the listener and then the GPU scene (`Zenith2/Docs/Design/RPROD/Renderer.md → "Create sequence"`) |
| a non-null listener while one is set | `ZENITH_ASSERT` ("clear the range listener first"); without asserts the new listener replaces the old one, and ranges reported to the old one are treated as never reported |
| `nullptr` | clears the listener; no callback is made for the live ranges, and no range is reported until a listener is set again |
| a range created while no listener is set | never reported later: setting a listener afterwards makes no `OnMaterialRangeCreated` for it, and its release makes no `OnMaterialRangeFreed`; its records stage the not-drawn image |
| the listener epoch | `uint32 m_uListenerEpoch`, 0 at `Create`, incremented by every `SetRangeListener` call; a range stores the epoch under which its creation was reported, 0 when none was, and its release calls `OnMaterialRangeFreed` only when that value is non-zero and equals the current epoch with a listener set. So a release never reaches a listener that did not see the creation |
| called inside a callback or while `ApplyDeltas` runs | `ZENITH_ASSERT` (the in-callback flag and the atomic in-apply flag that `ApplyDeltas` sets for its duration) |
| `~Flux_GPUScene` | asserts the listener is null; makes no callback |

The calls, made inside `ApplyDeltas` (step 1 of "Scatter staging and coalescing") in the render world's section and delta order:

1. For an instance delta that is applied (an ADD, an UPDATE, or either as the other, `Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderInstanceDelta layout"`), compute its key K. When the instance already holds K, nothing below runs.
2. Acquire K (private `AcquireMaterialRange`). A live range with key K gains one reference. Otherwise allocate S consecutive material slots; at the table's maximum the allocation fails with `FLUX_GPU_SCENE_ERROR_TABLE_FULL` as in "Growth" (no range, no callback, the instance is not rendered). Then record the range (key, first slot, count 1, epoch 0), fill S images in a frame-arena array with the not-drawn image, call `OnMaterialRangeCreated(mesh, the range's stored S material IDs, uFirstSlot, the S images)` when a listener is set and store the current epoch in the range, and stage the S images at `(FLUX_GPU_SCENE_TABLE_MATERIAL_SLOT, uFirstSlot + i)` in the pending store (last write wins).
3. Release the key the instance held before (private `ReleaseMaterialRange`), for an UPDATE that changed it (after step 2 acquired the new one, so a key shared by both is never freed and re-created) and for a REMOVE. One reference is dropped; at zero the S slots are appended to the pending free list, the range is removed from the key map, and `OnMaterialRangeFreed(uFirstSlot)` is called under the epoch rule above.
4. A world with `m_bFullState` first calls `OnMaterialRangeFreed` for every live reported range in ascending first-slot order under the epoch rule (private `DiscardSlotState`), then discards the slot state and the pending store, then re-allocates through step 2, so every release of the rebuild precedes every creation, and the listener's ranges after it equal those of a delta replay. Device-loss recovery reaches the same path through `Flux_RenderWorldRing::RequestFullExtraction`.

The calls happen when the deltas apply, whether the frame later submits or not: a deferred frame keeps the staged images pending ("Deferred frames and the pending store") and reports nothing again. The listener therefore sees, per first slot u, `OnMaterialRangeCreated(u)` and `OnMaterialRangeFreed(u)` alternate, starting with a creation; between them no other creation covers a slot of the range; and a slot of a freed range reappears in a creation only after the frees' client batch retired, or in the rebuild of step 4.

Test access: `Flux_GPUScene` declares `friend class Flux_GPUSceneTestAccess;`, which adds no code or data to the library. `Zenith2/Engine/ZenithFlux/Tests/GPUScene/Flux_GPUSceneTestAccess.h` is header-only, included by test sources alone, and forwards each member to the private member of the same name that `ApplyDeltas` calls, so a test drives steps 2 to 4 on the real object without a GEOM mesh:

```cpp
class Flux_GPUSceneTestAccess
{
public:
	static Zenith_Result<uint32, Flux_GPUSceneError> AcquireMaterialRange(Flux_GPUScene& xScene, Flux_MeshID xMesh, Zenith_Span<const Flux_MaterialID> axMaterials);   // step 2; the first slot
	static void ReleaseMaterialRange(Flux_GPUScene& xScene, Flux_MeshID xMesh, Zenith_Span<const Flux_MaterialID> axMaterials);   // step 3
	static void DiscardSlotState(Flux_GPUScene& xScene);                                                                           // step 4, before re-allocation
	static uint32 GetMaterialRangeRefCount(const Flux_GPUScene& xScene, uint32 uFirstSlot);                     // 0: no live range starts there
	static const Flux_MaterialSlotRecord* FindPendingMaterialSlotRecord(const Flux_GPUScene& xScene, uint32 uSlot); // null: nothing staged for the slot
};
```

### Scatter staging and coalescing

<!-- elaboration: the pending store -->
1. `ApplyDeltas` (build node, order 1) walks the render world's sections in world-tag order and their deltas in order; each changed record is written into the GPU scene's pending store (general-heap storage owned by `Flux_GPUScene`, not the frame arena; its bound is below), whose `Zenith_HashMap<uint64, uint32>` keyed by `(table << 32) | slot` maps to the record's pending index, so a second write to the same slot, in this frame or a subsequent one, overwrites the first (last write wins, [Z2-N-REND-541]).
2. At setup (order 2 of "Build-node upload order"), `AddScatterAndGrowthPasses` sorts the pending records by (table, slot) with `Zenith_Sort` (stable) into a frame-arena index array, merges runs of consecutive slots into `Flux_ScatterRecord`s (frame arena), computes `m_uWordBase` as the prefix sum of 16-byte words, and declares one frame-data range for the records and one for the data (`DeclareFrameData`, 16-byte aligned, owner `GPUSceneScatter`); nothing is written into the ring. At order 5, after `ExtendReservation` returned `FITS` or `EXTENDED`, `WriteScatterData` copies the records and the data into the two ranges through `GetFrameData`.
3. The scatter pass declares a UAV write on exactly the tables that have records and an SRV read of the ring range; dispatch size `ceil(totalWords / 256)` groups.
4. `m_axHistoryResets` sets `FLUX_INSTANCE_FLAG_HISTORY_RESET` and copies the current transform into `m_xPrevLocalToWorld` in the staged record ([Z2-N-REND-009]); every other updated instance copies its previous current transform into `m_xPrevLocalToWorld`.

<!-- elaboration: the pending store's bound and the row it counts under -->
Bound of the pending store: at most one staged record per slot of every table, so the record storage is at most Σ over the four tables of maxCapacity(t) × recordSize(t), which is at most `ulMaxTableBytes` (the `Z2-B-RPROD-001` value the tables' maximum capacities are derived from, "Flux_GPUScene" above); the `(table << 32) | slot` index map holds at most Σ maxCapacity(t) entries of 12 bytes at a load factor of at most 7/8 (at most 14 bytes per slot); the pending frees at most Σ maxCapacity(t) × 4 bytes. A full-state world reaches the bound. The storage grows from the general heap on demand and keeps its capacity. These are CPU bytes under `MEMORY_SUBSYSTEM_RENDERING`, reported through `Flux_MemoryConsumer::GetResidentBytes()`; they count under the pending Budgets row "GPU-scene CPU pending store" (a member of MEM-WIN-RAM and MEM-S24, producer the Stage 1 per-system budget ticket for RHI; cross-document item X91, pending) and, until it is adopted, under the unattributed residual `Z2-B-CORE-004`. `Z2-B-RPROD-001` is a VRAM row and does not cover them.

### Deferred frames and the pending store

<!-- elaboration -->
| Pending element | Kept until | On `OnFrameSubmitted(values)` | On `OnFrameDeferred()` |
|---|---|---|---|
| staged records (last write wins per slot) | the scatter of a submitted frame uploaded them | cleared | kept; the next frame's deltas coalesce into them |
| freed slots | a submitted frame uploaded the record changes that freed them | moved into that frame's client batches with `values` | kept; not returned to any free list, so no slot is reused while a free is pending |
| growth (a new table buffer and the old one to copy from) | a submitted frame ran its `GPUSceneGrow` copy | the old buffer is released to the retirement queue with `values` | kept; the next frame declares the copy pass again from the same old buffer. A further growth of the same table while one is pending (f deferred growing C → 2C, f + 1 needing 4C) creates the 4C buffer, releases the pending 2C buffer through `RequestRelease` (never used on the GPU, so it retires at the first `RetireCompleted` after the next drain) and keeps the single copy pass from the same old buffer into the newest; at most one pending new buffer exists per table |
| history resets | as staged records (they are staged records) | cleared | kept |

The invariant is that the tables after any sequence of submitted, deferred and failed frames equal the tables of the same render-world sequence with no deferral, which `Test_GPUScene_DeferredFrameKeepsDeltas` and the extended `Test_UploadRing_DeferredFrameReservesExactNext` assert bit for bit.

### Growth

When an allocation would exceed a table's capacity: create a buffer of `min(2 × capacity, max)` records, add the `GPUSceneGrow` copy pass (old contents into the new buffer) before the scatter pass, switch the import to the new buffer from this frame on, and release the old buffer to the retirement queue with this frame's values ([Z2-N-REND-544]). The build node creates the new buffer and registers it in the imported-state table; the release is `OnFrameSubmitted` at Submit step 3 with the frame's final values, one of the per-frame direct `RESOURCE` enqueues of "Drain headroom". At the maximum, the allocation fails with `FLUX_GPU_SCENE_ERROR_TABLE_FULL`, the instance or light is not rendered, `Flux.GPUScene.TableFull` increments, and no slot is evicted ([Z2-N-REND-545]).

### Persistent resource creation and the acquire list

`Flux_PersistentResources::CreateTexture(desc, initialData)`:

1. Create the texture (`DEVICE_LOCAL`, usage `SAMPLED | TRANSFER_DST | CONCURRENT_QUEUES`, queue mask all three), allocate its `TEXTURE_SRV` slot and write the descriptor on the creating thread (the slot exists at creation, [Z2-N-REND-546] and `GetSRV`). The creating thread (an asset loader job) touches no imported-state entry.
2. `RequestUpload` of the initial data to the copy queue (splittable per mip); the texture is not resident.
3. When the upload's operation reaches `COMPLETE` (checked at `PrepareNextFrame`), the texture joins the frame's one-time acquire list (lock 480); the `Flux.BuildGraph` node takes the list at graph Begin and registers each texture in the imported-state table flagged `PERSISTENT_READ_ONLY`, with the state the copy left (Vulkan `SHADER_READ` layout, D3D12 `COMMON`) — the single-writer rule of `Zenith2/Docs/Design/RHI/RenderGraphBackend.md → "Imported-state table"`, which no graph import can precede because a table references the texture only after its acquire ([Z2-N-REND-548]); the graph then records, at the start of the frame's first graphics batch, the transition to `PERSISTENT_READ` (Vulkan: the copy batch's own `TRANSFER_DST → SHADER_READ_ONLY` transition with destination stage `NONE` already happened, so only the timeline wait remains; D3D12: `COMMON → GENERIC_READ`), per `Zenith2/Docs/Rendering/GpuResources.md → "Texture streaming swap"` step 4.
4. `IsResident()` becomes true after that frame's submission; tables may reference it from the next scatter ([Z2-N-REND-548]).

`Release` (any thread; the asset system calls it when the asset's reference count reaches zero) removes the texture from the pending-upload and acquire bookkeeping under lock 480 and calls `RequestReleaseWithDescriptor(texture, TEXTURE_SRV, slot)`: the render-submit thread drains it at the next Submit step 4 with the values after that frame's submissions — the last-use values of the frame in which the release took effect on every queue ([Z2-N-REND-549]) — placing the slot on its pending list with them ([Z2-N-REND-059]) and the texture in the retirement ring; the imported-state entry is removed when the texture retires (`OnResourceRetired`, "Flux_DeferredDeletion" above), before its table slot can be reused. A texture released before its acquire was never registered, and its retirement finds no entry. The graph API has no UAV overload for persistent types, and any frame that imports a persistent resource writable is `FLUX_RG_ERROR_PERSISTENT_WRITE` ([Z2-N-REND-547]).

### Pool allocation

<!-- elaboration: the generic allocator; each owner sets its granule size and layout -->
| Pool | Allocator | Slot size | Growth bound |
|---|---|---|---|
| owner-defined pools (GEOM's skinned-vertex and procedural-geometry pools, VFX's particle pool) | `Flux_PoolAllocator`, a best-fit range allocator per region over owner-set granules (`Flux_PoolRegionDesc::m_uGranuleBytes`; GEOM uses 16-byte granules, so a FLOAT32 skinned slot of n vertices occupies `ceil(24n / 16)` granules); a pool has one or more regions, each with its own granule size, alignment, initial and maximum granules, and a double-buffered pool gives each slot the same range in both halves | the owner's slot size in granules | the owner's memory row (`Z2-B-GEOM-001`, `Z2-B-VFX-001`) |
| particle | per emitter: the emitter's capacity × particle stride, plus alive lists and indirect arguments | from the emitter description | `Z2-B-VFX-001` |
| VT physical and page table | `Zenith2/Docs/Design/ADVGFX.md` (Stage 4) | tiles | `Z2-B-ADVGFX-001` |

A pool is created by its owner through `Flux_GPUScene::CreatePool(const Flux_PoolDesc&)`; RHI fixes no granularity or layout. A region at capacity grows to `min(2 × its granules, its maximum)` by one resize-and-copy into a new buffer, in which every region is copied independently to its recomputed offset `align(Σ preceding region bytes, region alignment)` (GEOM's procedural layout moves region 1 to `align(8C, 256)` when C grows); the old buffer is released through the retirement queue. A pool whose region is at its maximum refuses the allocation with `FLUX_GPU_SCENE_ERROR_TABLE_FULL` ([Z2-N-REND-553]); pool slots retire through the retirement queue like table slots. A double-buffered pool's halves swap once per submitted frame, skinning writes the current half and velocity reads the previous one ([Z2-N-REND-552]).

### Streaming swap integration

`Flux_StreamingSwap` realises steps 1–4 and 6 of `Zenith2/Docs/Rendering/GpuResources.md → "Texture streaming swap"` with the pieces above: step 1 is `CreateTexture` with the new mip range, steps 2 and 3 its upload, step 4 the acquire list; when the new texture is resident the swap calls its client's `OnNewTextureResident`, and step 6 releases the old texture and its slot with the values of the last frame the client reports could reference them ([Z2-N-REND-554]). Step 5 — the material-constants block rewrite and the scatter of the referencing material-slot records — and the `TRANSITION_PEAK_TEXTURE_SWAP` reservation around the whole swap belong to GEOM's texture streamer and material manager (`Zenith2/Docs/Design/GEOM/Textures.md` → "Reallocate-and-swap realisation"; `Zenith2/Docs/Design/GEOM/Materials.md` → "Material slots and constants"); RHI makes no governor reservation.
