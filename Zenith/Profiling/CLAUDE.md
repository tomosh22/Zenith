# Profiling System

## Overview

Lock-free, low-overhead CPU profiler with hierarchical timing zones, a unified
in-engine ImGui visualizer (tools builds), and a compile-out switch for shipping.
Held on `g_xEngine.Profiling()`.

## Files

- `Zenith_Profiling.h` — public API: zone macros, the `ScopeZone` RAII helper, the
  per-thread ring + snapshot types, the timebase, the `ZENITH_PROFILING_ENABLED` gate.
- `Zenith_Profiling.cpp` — lock-free hot path, the consumer drain, the zone registry,
  and the ImGui visualizer.

## Recording zones

There is **no central enum** — register a zone anywhere by string name:

```cpp
void Foo()
{
    ZENITH_PROFILE_SCOPE("Foo");          // RAII scope; name interned once (static-local)
    ...
}

// Manual begin/end (when control flow precludes RAII):
Zenith_Profiling::ScopeZone xScope(ZENITH_PROFILE_ZONE("Bar"));
// or:  g_xEngine.Profiling().BeginProfileZone(ZENITH_PROFILE_ZONE("Bar")); ... EndProfileZone(...)

// Tasks take a zone id by value:
new Zenith_Task(ZENITH_PROFILE_ZONE("Animation Update"), Fn, pData);

// Wrap a single call:
ZENITH_PROFILING_FUNCTION_WRAPPER(g_xEngine.Physics().Update, ZENITH_PROFILE_ZONE("Physics"), fDt);
```

`ZENITH_PROFILE_SCOPE("Name")` / `ZENITH_PROFILE_ZONE("Name")` register the name once
(thread-safe static-local) and cache a dense `Zenith_ProfileZoneID`. The hot path stores
only that id; **never** call `RegisterZone` per frame in a hot loop — cache the id (the
macros do this; render passes cache it on the pass object). `RegisterZone` interns the
name (content-deduped) into an owned arena, so any caller string is lifetime-safe.

## Architecture

### Lock-free hot path (SPSC rings)
Each producing thread owns a bounded **single-producer/single-consumer ring**
(`Zenith_Profiling::ThreadBuffer`, ~256-260 KiB) published into a fixed atomic table by stable
thread id and cached in a `thread_local`. `BeginProfileZone` pushes onto a producer-private
in-flight stack; `EndProfileZone` writes a complete event and publishes it with **one
release-store** — no lock, no hashmap, no `g_xEngine` reach (~64 ns/scope, dominated by the
two `QueryPerformanceCounter` reads). Nesting beyond `uMAX_PROFILE_DEPTH` and unmatched ends
are release-safe (suppressed-depth + a counter, no underflow); ring overflow drops + counts.

### Consumer (main thread)
The main thread is the **sole consumer**. At the frame boundary `EndFrame` drains every
ring into a heap `Snapshot` (the frame accumulator) and hands it off to the display by an
**O(1) pointer swap** (never container assignment, which would free+reallocate). Live
reports (`WriteTextReport`) drain into the *same* accumulator, so they are non-destructive
to the published frame. Diagnostics (drops, unmatched ends, >0.5 s events) are warned from
the consumer, never the hot path. Pause is consumer-side — producers never read a flag.

### Timebase
`Zenith_Profiling_Detail::GetTimestamp()` returns raw `u_int64` monotonic ticks
(`high_resolution_clock`); `GetTicksToNs()` converts at display time (folds to 1.0 on MSVC).
Events store ticks, not chrono time-points.

## GPU per-pass timing (Flux)

Alongside the CPU zones, the profiler holds a **GPU channel**: one timing per
`Flux_RenderGraph` pass, in execution order, measured on the GPU.

- The Vulkan backend brackets each pass in `Flux_RenderGraph::RecordPassInto` with two
  `vkCmdWriteTimestamp` calls (`Zenith_Vulkan_CommandBuffer::Begin/EndGPUTimer`) into a
  per-frame-in-flight `VkQueryPool`. Query slots are claimed atomically during the
  parallel recording; worker 0 cmd-resets the pool at the head of its (first-submitted)
  command buffer so the reset precedes every write on the GPU timeline.
- **Execution order.** Slots are claimed in record-*race* order (whichever worker wins the
  atomic `fetch_add`), so each pass would otherwise land in a different slot every frame and
  the list would shuffle. `RecordPassInto` therefore receives the pass's stable
  execution-order index (the global index in the topologically-ordered pending-pass list),
  stores it with the timer, and the readback **sorts by it** — so the report and the viz are
  always in deterministic `Flux_RenderGraph` execution order. The index is threaded through
  `RecordPassInto(... , u_int uExecutionIndex)` and both backends' callers
  (Vulkan `ProcessRender/ComputePass`, D3D12 `RecordFrame`).
- Results are read back **deferred** — when the slot's fence signals in `BeginFrame`
  (`MAX_FRAMES_IN_FLIGHT` frames later, same guarantee the deferred-deletion drains use),
  converted with `limits.timestampPeriod`, and pushed via
  `BeginGPUCapture()` / `AddGPUPass(name, ms, execIndex)` / `EndGPUCapture()` (main-thread only).
- Disabled cleanly when the graphics queue reports `timestampValidBits == 0`; the D3D12
  null backend's `Begin/EndGPUTimer` are no-ops (GPU list stays empty). The whole path is
  gated on `ZENITH_FLUX_PROFILING` (mirrors the RenderDoc debug-marker pattern).
- **GPU tab visualization** (`RenderGPUView`): a scrolling **GPU frame-time history** graph;
  a **time-proportional timeline strip** — passes laid end-to-end on a cumulative-ms axis
  (width ∝ GPU ms) in execution order, stable per-name hash colour, in-bar labels,
  scroll-to-zoom / middle-drag-pan / hover tooltip (name, exec index, ms, % of frame); and a
  **detail table** (colour swatch · pass · ms · % · share bar) with *Sort by cost* and
  *Group by category* (name-prefix buckets — `HiZ`, `Shadow`, `HDR`, `SSR`, …) toggles. The
  strip + default table always stay in execution order; sorting only reorders a local index.
- Also surfaced in `WriteTextReport` (a `=== GPU Passes ===` section, exec-index column).
  `GetGPUPasses()` / `GetGPUTotalMs()` expose it to tools.

## Text report content (`WriteTextReport`)

The report has four parts: (1) a **frame-time headline** — `Frame: X ms (Y FPS, last complete)`;
because the live report runs mid-frame while `TOTAL_FRAME` is still open, the frame ms is taken
from the *display* snapshot (the previous complete frame) while the per-zone table is the in-flight
frame. (2) the **per-zone CPU table** (total/calls/avg/min/max, sorted by total). (3) the **GPU
per-pass** section (execution order, exec-index column). (4) a **per-pass CPU-record-vs-GPU table**
— events carrying a runtime label (the `Flux Record Pass` zone, labelled with the pass `DebugName`)
are aggregated by label and joined 1:1 with the GPU pass cost (both keys come from `pxPass->DebugName()`),
so you can see which pass is the CPU *recording* hotspot vs its GPU cost (e.g. a geometry pass that's
cheap on the GPU but expensive to record, recorded once per g-buffer + once per shadow cascade).

## Main-loop coverage

The main loop is fully zoned: `Flux PlatformAPI Begin Frame`, `Touch Update`, `Editor Update`
(tools), `Property Tuning` + `Graph Reload` (tools), `Physics`, `Scene Update`, `AI Update`
(when `Zenith_AI::IsEngineTickEnabled()`), `UI Update`, `ImGUI`, render record/submit, `Flux …
End Frame`. **`Scene Update` is one zone** (the ECS dispatch is in the pristine ZenithECS leaf,
which deliberately doesn't depend on the profiler), so the heaviest engine-side sub-system —
skeletal **`Animation Update`** (state-machine eval + clip blend + IK + skinning + bone upload) —
is zoned at its engine-side source, `Zenith_AnimatorComponent::OnUpdate` (fires once per animated
entity; aggregated by zone id). Game-specific component costs stay inside `Scene Update`; a game
zones its own heavy components.

## `--profiling-dump` diagnostic

Passing `--profiling-dump` to a windowed build dumps the live profiling report (frame time + CPU
zones across all threads + the per-pass GPU section + the per-pass CPU-record table) every 120
frames to stdout **and** to a truncated, `fflush`/`fclose`'d `zenith_profiling_dump.txt` in the
working dir. The dump runs before `EndFrame`, so the live drain is non-destructive to the in-engine
timeline.

## Compile-out (`ZENITH_PROFILING_ENABLED`)

Defaults to 1. Every config that exists today defines it to 1 (wired in `Sharpmake_Common.cs`);
a future shipping/Retail axis will flip it to 0 there to strip the profiler for zero overhead. When 0,
the zone macros and `ScopeZone` compile to nothing, the per-task profiling calls drop out,
and every subsystem method becomes a no-op stub. The subsystem object is still allocated
(inert) so engine/editor wiring needs no per-call-site gating.

## Visualization (ZENITH_TOOLS only)

`RenderToImGui()` (the "Profiling" editor window) shows:
- A **frame-time history graph** (256 frames) + worst-frame, **pause-on-spike** (auto-pin +
  freeze the first frame over a threshold — debug-variables builds), and Reset Worst.
- **Timeline** — flame view, per-thread lanes (the main thread reads "Main"), zoom/pan,
  depth filter; zone colours derived from the id, names via `GetZoneName`, render-pass
  events carry their `DebugName()` as a label.
- **Statistics** — per-zone total/avg/max/calls, sorted, with a substring filter.
- **Thread Breakdown** — per-thread hierarchical self/total tree.

## Key types & invariants

- `Zenith_ProfileZoneID` — dense `u_int`; `ZENITH_PROFILE_ZONE_NULL` / `_OVERFLOW` are
  reserved sentinels resolved by `GetZoneName`.
- `Zenith_Profiling::Event` — `{begin, end ticks, zone id, depth, optional label}`.
- The thread-buffer table never reallocates; the zone descriptor table is fixed-capacity
  with an atomic-published count, so the UI reads it lock-free while a worker appends.
- Teardown: producer threads `UnregisterThread()` on exit; `Shutdown()` frees the main ring
  and leaves any still-live producer (e.g. the FileWatcher) allocated to avoid a UAF.
