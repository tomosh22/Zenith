# Flux Render Graph

## Overview

The render graph is the core scheduling and synchronisation layer of the Flux renderer. Subsystems describe what they want to render as a set of passes with declared resource Reads/Writes; the graph computes execution order, allocates transient resources (with optional aliasing), and synthesises all GPU barriers automatically.

There is no caller-supplied ordering token. There is no `RenderOrder` enum. Pass order is the topological sort of the declared dependency adjacency.

## Files

| File | Responsibility |
|------|---------------|
| [Flux_RenderGraph.h](Flux_RenderGraph.h) | Public types (`Flux_RenderGraph`, `Flux_PassBuilder`, `Flux_RenderGraph_Pass`, barriers, transient handles); fluent builder API |
| [Flux_RenderGraph.cpp](Flux_RenderGraph.cpp) | `AddPass`, transient creation, dirty/Clear handling, `GetPassOrderDescription()` |
| [Flux_RenderGraph_Compilation.cpp](Flux_RenderGraph_Compilation.cpp) | Validation, adjacency building, Kahn's topological sort, barrier synthesis, transient lifetime + aliasing pack |
| [Flux_RenderGraph_Execution.cpp](Flux_RenderGraph_Execution.cpp) | Prepare callbacks, queue passes, `RecordPassInto` (per-pass direct recording invoked by the backend worker loop) |

Setup, Compile, and Execute are the three lifecycle phases — kept in separate translation units so each is small enough to read end-to-end.

## Lifecycle

```
Setup    Compile              Execute
-----    -------              -------
each     BuildResourceTraffic Prepare callbacks (CPU)
sub-     Validate             Queue passes (topo order)
system   TopologicalSort      RecordFrame: backend records each pass's callback
calls    AllocateTransients     directly into per-worker command buffers
AddPass  SynthesizeBarriers     (parallel slices; barriers inline)
```

Compile has more steps than shown (see the numbered list below for the full, exact order); the diagram lists only the major phases. Note `BuildResourceTraffic` runs first, `BuildAdjacency` is internal to `TopologicalSort` (`BuildAdjacencyFromTraffic`), and `SynthesizeBarriers` runs **after** `AllocateTransients`.

### Setup phase (per frame, after Initialise)

`Flux::SetupRenderGraph()` walks each subsystem and calls `xGraph.AddPass(name, pfnRecord)` returning a `Flux_PassBuilder`. The builder is fluent and `&&`-qualified so the chain has to be consumed in the same expression. Typical use:

```cpp
xGraph.AddPass("HiZ Mip", ExecuteHiZMip)
      .UserData(uMip)
      .Reads (GetHiZBuffer(), RESOURCE_ACCESS_READ_SRV,  uMip - 1, 1)
      .Writes(GetHiZBuffer(), RESOURCE_ACCESS_WRITE_UAV, uMip,     1);
```

`AddPass` returns the builder by value; chained methods return `Flux_PassBuilder&&`. Storing the chain in `auto&` is a compile error by design — the ref-qualifier rejects the bind. If you need conditional or loop-driven declarations, capture the `Flux_PassHandle` (implicit conversion from the builder) and call `xGraph.Read(...)`/`Write(...)` directly.

### Compile phase

`Flux_RenderGraph::Compile()` runs once per dirty cycle (after Setup, or after `MarkDirty()` from a `SetEnabled` toggle). The order is fixed:

1. `BuildResourceTraffic()` — walks every pass's declared Reads/Writes into the per-resource reader/writer traffic tables the later steps consume.
2. `Validate()` — orphaned reads, unused transients, attachment counts, memory-flag compatibility.
3. `TopologicalSort()` — internally calls `BuildAdjacencyFromTraffic()` (for every resource: every reader gets an edge from every writer; writers chain in declaration order to preserve write-write hazard ordering) then `AddExplicitDependencies()` (`DependsOn(handle)` declarations become extra edges), then runs Kahn's algorithm to produce `m_xExecutionOrder`. Cycles fail the compile loudly.
4. `ComputeResourceLifetimes()` — for each resource, record `(firstWrite, lastRead, lastWrite)` as topological-order indices. The aliasing packer compares these as time intervals.
5. `ComputeTransientLifetimes()` — derives the per-transient first-use/last-use intervals used by aliasing.
6. `AssignAliasingGroups()` — packs non-overlapping transients into shared aliasing pool slots.
7. `InferPassAttachments()` — resolves each pass's colour/depth attachment set from its declared Writes.
8. `MarkPassesAsComputeOrGraphics()` — classifies each pass as compute or graphics from its attachments/dispatch usage.
9. `ResolveClearFlags()` — marks the first writer of each attachment subresource as the clear owner.
10. `AllocateTransients()` — allocates transient VRAM (sharing aliased slots when `IsAliasingEnabled()` and the backend supports `SupportsTransientAliasing()`).
11. `SynthesizeBarriers()` — for each transition between consecutive passes that touch the same subresource with incompatible accesses, emit a `Flux_RenderGraph_Barrier` into the destination pass's prologue. The graph is the **sole** barrier authority — subsystems never insert manual transitions.
12. `SynthesizeAliasingBarriers()` — emits the aliasing hand-off memory barriers for transients reusing a pool slot this frame.

### Execute phase

`Flux_RenderGraph::Execute()` runs every frame:

1. `CallPrepareCallbacks()` on the main thread — passes that registered a `Prepare(pfn)` get a chance to run CPU work that must happen before recording (e.g., upload uniform buffers, build CPU-side draw lists).
2. `SubmitRecordedLists()` queues every enabled pass (with its attachments + synthesized barriers) onto the renderer in `m_xExecutionOrder` — no recording happens here.
3. `g_xEngine.FluxRenderer().RecordFrame()` drives the single direct-recording stage synchronously (still inside the render-task safe window, before the frame memory submit). The backend distributes the queued passes as contiguous topological slices across worker command buffers and, for each pass, emits the prologue barriers (image / buffer / aliasing) then calls `Flux_RenderGraph::RecordPassInto` — which sets the recording-pass TLS (so `AssertBoundResourceDeclared` validates each shader binding) and runs `pfnOnRecord` directly against the backend command buffer. Worker buffers are later submitted in order, so `m_xExecutionOrder` + inline barriers enforce dependencies. (D3D12 null backend: a serial loop into a no-op command buffer, so callback side effects still run.)

## Key concepts

### Resources
- `Flux_RenderAttachment` / `Flux_RenderAttachmentCube` — long-lived render targets owned by subsystems (HDR scene, G-buffer MRTs, shadow cascades, etc.).
- `Flux_Buffer` — long-lived buffers (vertex, index, structured).
- `Flux_TransientHandle` — graph-owned handles for resources whose lifetime fits inside a single Compile cycle. Transients can alias by pool; aliasing is opt-in (default on, debug toggle exposed).

### Access types
`ResourceAccess` (in `Flux_Enums.h`) enumerates the access patterns the graph understands: `READ_SRV`, `READ_DEPTH`, `WRITE_RTV`, `WRITE_DSV`, `WRITE_UAV`, `READWRITE_UAV`, `READ_INDIRECT_ARG`, `READ_BUFFER_SRV`, `HOST_TRANSFER_WRITE`, `UNDEFINED`. Stage discrimination (vertex vs fragment vs compute) is handled inside the access-to-Vulkan translator, not exposed at the graph level.

### Prepare vs Record callbacks
- **Prepare** runs on the main thread before any recording. Use for CPU-side work that must complete before any pass's record callback runs (e.g., uniform buffer uploads via `Flux_MemoryManager::UploadBufferDataAtOffset`, frustum culling, draw-list construction).
- **Record** runs on a worker thread. The callback receives a `Flux_CommandBuffer*` (the backend command buffer — record draws/dispatches/binds directly on it) and the pass's typed `UserData<T>`. Inside the callback, the recording-pass TLS is set so `Flux_ShaderBinder` calls can sanity-check that bound resources were declared.

### Barriers
The graph emits three barrier kinds before each pass's recording (outside any render pass):
- **Image transitions** — layout + access transitions for any image subresource whose access pattern differs from the previous pass that touched it.
- **Buffer barriers** — pure memory + execution barriers for buffers; no layout change.
- **Aliasing hand-offs** — when a transient that previously occupied an aliased pool slot is being reused this frame for a different transient, an aliasing memory barrier is emitted so the new content is published correctly.

Subsystems must never call `vkCmdPipelineBarrier` or insert `Flux_CommandImageTransition` manually. Doing so fights the graph and can mask correctness bugs that only show up under aliasing.

## Print Pass Order debug button

`Flux_RenderGraph::GetPassOrderDescription()` returns a human-readable dump of the compiled order:

```
TerrainGBuffer -> StaticMeshesGBuffer -> AnimatedMeshesGBuffer -> Foliage -> HiZ -> SSAO -> SSR Raymarch -> ... -> Tonemap
```

It is bound to a debug variable button at `Render/RenderGraph/Print Pass Order` (`Flux.cpp:345`). When you're trying to figure out why a pass runs where it does, click the button and read the resulting log line — it is the live source of truth, ahead of any documentation.

Passes that are not *effectively enabled* appear suffixed with `(disabled)`. The string is empty before the first successful Compile. Note: a pass that is **force-disabled** (see below) is excluded from the execution order entirely — it does not appear in this list at all. The `(disabled)` suffix is for a pass still in the order whose base bit is off (the `SetEnabled` cheap-toggle case).

## Game render features & generic pass disable

External game code modifies the graph through two generic mechanisms — no hardcoded stages, no fog-specific API.

### Render features (`Zenith_GameRenderFeatures`)
A game registers a feature `{name, init, setup, shutdown, runAfter}` (captureless free-fn trampolines). The engine drives the lifecycle: `InitialiseAllPending` at Flux init, `ShutdownAll` at Flux shutdown; late registration (after Flux is up) initialises immediately and requests a rebuild. During `Flux_FeatureRegistry::RunSetup`, after each engine setup step runs, every game feature anchored `runAfter="<that step's name>"` has its `SetupRenderGraph` invoked — so the feature's passes are declared at the right index (and therefore land in the correct same-resource write-chain position) without any enum. Cross-resource ordering uses `DependsOn(xGraph.FindPass("EnginePass"))`. Anchor names resolve to engine setup-step names only (v1); to order relative to another game feature, share the anchor (registration order tie-breaks) or use `DependsOn(FindPass(...))`.

### Stable pass identity (`FindPass`)
Every pass stores the `const char*` name passed to `AddPass` in **all** configs (not just tools), so `FindPass(name)` works in shipping. Names must be **static-lifetime** string literals (only the pointer is stored). `FindPass` returns a generation-stamped handle — resolve it in your `SetupRenderGraph` and use it immediately; never cache it (handles die on `Clear()`/rebuild). Duplicate names are a `Zenith_Check` error (enforced in `AddPass` under `ZENITH_RUNTIME_CHECKS`, which is on in shipping).

### Force-disable overlay (`SetOwnerForceDisabled` / `SetPassForceDisabled`)
Each pass carries a system-owned base `m_bEnabled` (written only by `SetEnabled`, owned by the subsystem) plus an auto-assigned **owner** = the setup-step name that added it (e.g. all 6 fog passes share owner `"Fog"`; the final-RT layout-transition pass has owner `"Present"` — the owner is the setup-step name, which for a feature is its `RegisterFeature` name). A game force-disables a whole feature group by owner, or a single pass by name. The graph schedules off **`IsPassEffectivelyEnabled = base && !ownerForceDisabled && !nameForceDisabled`** — read at every site that filters enabled passes — so the base bit is never mutated and lifting the override restores the subsystem's own state automatically. Changing an override calls `MarkDirty()` so the next `Compile` rebuilds the order (force-disabled passes drop out of it). The override set **persists across `Clear()`/rebuild** (a game-level decision must not be rebuild-fragile); a game clears its own override on feature shutdown. When no override is set, the predicate is exactly `m_bEnabled` — the engine pass order is byte-identical to having no overlay at all.

## Common gotchas

1. **`auto&`-binding the builder is a compile error**. The chain methods are `&&`-qualified. Capture the `Flux_PassHandle` via implicit conversion if you need it later:
   ```cpp
   Flux_PassHandle xPass = xGraph.AddPass("MyPass", Record).Writes(...);  // OK
   ```
2. **Stale transient handles** trip an assertion. Compile invalidates the generation counter; re-acquire transients from `xGraph.CreateTransient` after every recompile.
3. **Conditional passes** — call `xGraph.SetEnabled(handle, bEnabled)` and the pass's edges still exist in the adjacency, but the record dispatch is skipped. Do not delete the `AddPass` call; toggle the flag.
4. **`HOST_TRANSFER_WRITE` is synthetic** — a buffer-only `ResourceAccess` that never appears in a pass's read/write list. It marks the synthetic predecessor for a host-issued staging upload so the next reader gets a TransferWrite -> ShaderRead barrier in its prologue. (There is **no** `Flux_RenderGraph::MarkBufferHostWritten` function — that name in the `Flux_Enums.h` comment and the surrounding "no `MarkBufferHostWritten`" notes refers to an API that does not exist; frame-indexed buffers are intentionally invisible to the graph, see `Flux_FrameIndexedBufferBase`.)

## Cross-references

- [Flux/CLAUDE.md](../CLAUDE.md) — high-level Flux architecture, direct command recording, render pipeline overview.
- [Vulkan/CLAUDE.md](../../Vulkan/CLAUDE.md) — backend command buffer, VRAM, deferred deletion.
- [Docs/Onboarding/NewcomerMap.md](../../../Docs/Onboarding/NewcomerMap.md) — recommended reading path.
