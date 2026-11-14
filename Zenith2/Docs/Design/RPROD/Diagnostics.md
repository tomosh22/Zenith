# RPROD design: diagnostics and profiling

This file is part of the RPROD design family; the entry document is `Zenith2/Docs/Design/RPROD.md`. It lists every counter, on-screen profiler row, log category, console command, debug view, frame-dump entry, telemetry field and profiling zone RPROD produces. The on-screen profiler itself, its overlay and the graph viewer are DEVT's (`Zenith2/Docs/Design/DEVT.md`, pending); RPROD supplies the data through the profiler hooks and the frame dump.

## Scope of this file

The specification is `Zenith2/Docs/Rendering/RenderGraph.md → "GPU timestamps"`, "Frame dump", `Zenith2/Docs/Rendering/Culling.md → "Culling diagnostics"`, `Zenith2/Docs/Rendering/ShaderSystem.md → "PSO counters and tests"` and `Zenith2/Docs/Budgets.md → "Measured in every workload run"`. Every counter is emitted once per frame through the profiler hooks' counter callback (`Zenith2/Docs/Contracts.md → "No mutable global state"`) from `Flux.BuildGraph` unless the table says otherwise; delta counters carry the `_delta` meaning of cross-doc item X55 until the counter-kind marker lands.

## Counters

| Counter | Kind | Meaning |
|---|---|---|
| `Flux.Graph.CompileMs` | gauge | steps 0–10 of the compile ([Z2-N-REND-308]) |
| `Flux.Graph.Passes`, `.PassesCulled`, `.ClearsFolded`, `.PassesMerged`, `.Scopes`, `.SplitsDemoted` | gauge | the compiled graph's shape |
| `Flux.Graph.Resources`, `.Transients`, `.Imports` | gauge | resource counts |
| `Flux.Graph.CompileFailures` | cumulative | Shipping frames skipped for a compile error |
| `Flux.Graph.TransientBytesDedicated`, `.TransientBytesAliased`, `.AliasingSavingsBytes`, `.AliasingSavingsPercent`, `.TransientPeakBytes300` | gauge | [Z2-N-REND-445]; the aliasing savings the memory panel shows |
| `Flux.Graph.CachedResources` | gauge | physical cache entries |
| `Flux.Record.CheckFailures` | cumulative | record-time checks failed (development) |
| `Flux.Culling.Tested`, `.FrustumCulled`, `.OcclusionCulled`, `.Drawn` per `[<view>][phase <n>]` | gauge | [Z2-N-REND-657]; published when the readback resolves |
| `Flux.Culling.MeshletsTested`, `.MeshletsConeCulled`, `.MeshletsOcclusionCulled`, `.MeshletsDrawn` | gauge | visibility-buffer path |
| `Flux.Culling.MeshletOverflow`, `.BucketsFull` | cumulative | overflow paths of Culling.md |
| `Flux.Culling.TransparentEntries[<view>]` | gauge | the view's transparent entry count n; `16 × n` bytes are its upload-ring declaration (Culling.md "Transparent collection and emission") |
| `Flux.RenderWorld.Dropped` | cumulative | render-world records the writer refused with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL` ([Z2-N-REND-008]); every extractor's refusals, the engine extractors' retried by `Zenith2/Docs/Design/RPROD/Extractors.md → "Slot-full handling"` |
| `Render.Extract.*` | as that file | the engine extractors' counters (`Zenith2/Docs/Design/RPROD/Extractors.md → "Extraction counters and zones"`), emitted from their phase-12 nodes rather than from `Flux.BuildGraph` |
| `Flux.Views.Count`, `.Deferred`, `.Rejected`, `.PersistentBytes` | gauge / cumulative / cumulative / gauge | [Z2-N-REND-608] |
| `Flux.Shadows.Views` | gauge | shadow views in the frame's view list (the dropped-light counter `Flux.Shadows.Dropped` is LIGHT's, `Zenith2/Docs/Design/LIGHT/Shadows.md → "Atlas allocation"`) |
| `Flux.PSO.CreatedBoot`, `.CreatedGlobal`, `.CreatedScene`, `.CreatedStreaming`, `.CacheHits`, `.CacheMisses`, `.CreateMsP95`, `.MissBuckets`, `.MissInstances`, `.MissPasses`, `.SyncCreates`, `.TwoSided` | as the specification | [Z2-N-REND-212]; cache hits and misses are derived from the RHI's per-creation hit flag (cross-doc item X71 f) |
| `Flux.Pacing.IntervalUs`, `.DeviationUs`, `.SwapInterval`, `.TargetIntervalUs`, `.LatencySlotWaitUs` | gauge | the frame pacer (Renderer.md "Frame pacer") |
| `Flux.Latency.InputToPhotonUs` | gauge (per resolved present) | [Z2-N-BUDGET-378] |
| `Flux.Quality.Tier` | gauge | the active tier index |

Every counter name also appears in `Flux_RendererStats` for tests.

## On-screen profiler rows

| Row | Source | Consumer |
|---|---|---|
| GPU time per pass per queue, with the view suffix | the timestamps published by `Flux_PassTimingPublisher` ([Z2-N-REND-312]) | DEVT's GPU timeline (MP-GPUPASS) |
| transient memory: with aliasing, without, savings in bytes and percent, per group, 300-frame peak | `Flux_TransientMemoryStats` | DEVT's memory panel ([Z2-N-REND-445]) |
| culling per view and phase: tested, frustum-culled, occlusion-culled, drawn | the culling counters | DEVT's counter rows ([Z2-N-REND-657]) |
| PSO misses (buckets, instances, passes) and sync creations | the PSO counters | DEVT's counter rows ([Z2-N-REND-210]) |
| draws per frame (indirect commands visible after compaction; the sum of `Drawn`) | culling counters | the plan's "counters (draws, culled instances)" row of Z2-D-005 |
| frame pacing: interval, deviation, swap interval, latency | pacing counters | DEVT's frame-time row |
| virtual texturing (Stage 4): ADVGFX's `VT` row in the rendering group and its `vt` tab | ADVGFX's `Flux.VT.*` counters | registered by ADVGFX's dev tools, not by RPROD (row AV10) |

Who writes these rows (row DV31): the Stage 2 rows of this table are transcribed into `Zenith2/Engine/ZenithProfiling/Internal/Panels/Zenith_PanelsRPROD.cpp` by DEVT's Stage 2 panel-catalogue tickets; from Stage 3, an RPROD ticket that adds a row to this table adds it to that file's table in the same diff.

## Log categories

`flux.graph`, `flux.views`, `flux.culling`, `flux.pso`, `flux.shaders`, `flux.pacing`, `flux.quality`, and the engine extractors' `render.extract` (in ZenithRuntime), each declared with `ZENITH_LOG_CATEGORY` (`Zenith2/Docs/Design/CORE/Diagnostics.md → "Structured logging"`). Fixed messages: every graph diagnostic text of `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Diagnostic text"`; `pso: miss <key names> (<shipping|development> fallback)`; `pso: creation failed <key names> (<blob|device>): <shader or RHI error>` (the `Flux_PSOFailReason`, PSO.md "PSO table and states"); `culling: view <view suffix> transparent entries <n> (<bytes> bytes) above its share <s> bytes of the upload cap` (once per view per run); `extract: light <key>: refused`, `extract: camera <entity>: invalid lens` (`render.extract`, once per key or entity); `shaders: container '<file>' refused: <reason>`; `views: request of <kind> owner <id> rejected: <reason>`; `pacing: latency slot timed out`; `quality: <key> value <v> outside [<min>, <max>], kept <old>`.

## Console commands and debug UI

| Command (development builds) | Effect |
|---|---|
| `r.graph.dump <N>` | write the next N frame dumps ([Z2-N-REND-290]) |
| `r.graph.aliasTransients`, `…poisonTransients`, `…asyncCompute`, `…splitBarriers`, `…mergePasses`, `…parallelRecord`, `…timestamps` | toggle the graph settings at the next compile |
| `r.shadingPath <path>` | switch the desktop path ([Z2-N-REND-692]) |
| `r.debug.hiz <mip\|auto> <max\|min>` | the HiZ debug overlay ([Z2-N-REND-658]) |
| `r.debug.culling 0\|1` | freeze the culling camera of the main view while the render camera moves |
| `r.pso.dumpMisses` | write the miss list ("PSO recording in development" of PSO.md) |
| `r.pacing.mode`, `r.pacing.targetFps` | pacing toggles |

The on-screen debug UI exposes the same toggles as checkboxes and a "Dump graph" button ([Z2-N-REND-431]); the UI itself is DEVT's and EDIT's.

## Debug views

| View | Realisation |
|---|---|
| HiZ pyramid and culled bounds | `Zenith2/Docs/Design/RPROD/Culling.md → "HiZ debug overlay"` |
| surface-data channels | the per-path debug output the parity suite captures ("Parity suites" of Tests.md), selectable with `r.debug.surface <channel>` |
| PSO miss magenta | the development fallback itself |
| poisoned transients | NaN and magenta under `r.graph.poisonTransients` |

## Frame dump front-end content

The keys RPROD fills are `Zenith2/Docs/Design/RPROD/RenderGraphFrontEnd.md → "Front-end frame dump keys"`; the writer, the back-end keys and the device-loss copies are RHI's. The dump is the one input of DEVT's graph viewer ([Z2-N-REND-314]), and a parity failure writes the dumps of both runs ([Z2-N-REND-943]).

## Telemetry

Per run: the shading path per view kind, the active tier, the aliasing mode, the mean and peak transient memory with and without aliasing, the PSO created and missed counts per group, the cold- or warm-cache flag of the pipeline cache (RHI's), the pacing statistics (p50, p95, p99 of the interval and the deviation) and the input-to-photon samples, all into the evidence record of `Zenith2/Docs/Budgets.md → "Evidence records and the input fingerprint"`.

## Profiling zones

`Flux.BuildGraph` contains the child zones `Flux.ConsumeWorld`, `Flux.ViewList`, `Flux.Setup`, `Flux.Compile.Cull`, `Flux.Compile.Order`, `Flux.Compile.Queues` (RHI), `Flux.Compile.Place`, `Flux.Compile.Barriers`, `Flux.Compile.Assign` (RHI), `Flux.Compile.Merge`, `Flux.Compile.Partition` (RHI), `Flux.Compile.Verify`; `Flux.Record` sub-jobs are `Flux.Record/P<n>`; `Flux.Present` contains `Flux.Pacing.Wait` and `Flux.Pacing.LatencySlot`; PSO creation jobs are `Flux.PSO.Create/<group>`. `Z2-B-RPROD-003` Render extraction CPU is measured on the four engine extractor node zones of phase 12, `Render.ExtractInstances`, `Render.ExtractLights`, `Render.ExtractCameras` and `Render.ExtractEnvironment` (each named after its node with the world suffix, [Z2-N-BUDGET-223], as ENV names its own extractor zones): the row is the per-frame sum of the four zones over every world (MP-CPUZONE), which RPROD's budget harness reports beside `Z2-B-RHI-004` and `Z2-B-RHI-005`. Module extractors' zones (ENV's, ANIM's) count against their modules' rows.
