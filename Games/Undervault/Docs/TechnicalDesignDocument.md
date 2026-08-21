# Undervault — Technical Design Document

**Status:** v0.1 — pre-production reference TDD; sections become binding as
the §20 roadmap reaches them
**Author:** Design (Claude), from the user-approved plan, 2026-08-21
**Companion:** [GameDesignDocument.md](GameDesignDocument.md) — the design this
document implements. Its §19 scope-tier matrix is the binding scope gate.
**Ground truth:** every "the engine today does X" claim in this document names
the file it was verified against (session of 2026-08-21). If code and doc
disagree later, re-verify against the file, then fix the doc in the same
commit.

---

## Executive summary — required engine work

Undervault is buildable on Zenith today for its *simulation* (ECS, task
system, save stack, and the CityBuilder architectural patterns cover it), but
the engine needs deliberate growth before the game can ship on Android —
five small P0 unblocks now, and a P1 program dominated by one large absence
(audio). Full table with sizes, phases, and fallbacks in **§2**.

- **P0 (before the vertical slice):** fix + expose the **orthographic
  camera** (public init API and a missing Vulkan Y-flip — the whole game
  renders through it); a **gesture layer** (pinch/pan/tap/long-press — the
  engine has multi-touch plumbing but zero gesture vocabulary); a
  **priority-queue collection** (A* open list; `std::` is banned and
  `Zenith/Collections` has no heap); raise the **1024-quad UI budget**; and
  a device **spike on Vulkan surface recreation** across Android
  background/resume (the surface is created exactly once today — unverified
  ship risk).
- **P1 (before Android ship):** an **audio subsystem** — the engine has
  *none*, on any platform (largest single item); an **ASTC texture path**
  (BCn-only today → uncompressed textures on Mali/PowerVR); **save-on-
  background** and the **surface-recreation fix**; a **frame-rate
  cap** (hard-coded FIFO-60 today); **render-scale reachable on Android**
  (it is TAA-coupled and unreachable on device today); **safe-area insets**;
  on-device **config plumbing**; **device-gate relaxation** (the renderer
  currently hard-rejects GPUs lacking tessellation, which this game never
  uses); a **9-slice widget**; an **Android CI leg**; **AAB packaging**.
- **P2:** slim tile-instance path, width-aware UI scaling, quality
  tiers/thermal, IME (designed around permanently — never built).

---

## 1. Architecture overview

### 1.1 Module map

```
Games/Undervault/
  Undervault.zproj             # descriptor ("android": true)
  Undervault.cpp               # Project_* contract: component registration
                               #   (orders 100+), UV_Bindings install, graphics
                               #   kill-list, render-feature registration
  Source/                      # PURE logic — headless-testable, no g_xEngine
    UV_Bindings.h              #   THE action table (ZM_Bindings.h pattern)
    Sim/      UV_SimClock, UV_ElementTable, UV_FluidSim, UV_ThermalSim,
              UV_Telemetry
    Grid/     UV_CellGrid, UV_SolidMutation, UV_Rooms
    Nav/      UV_Regions, UV_Pathfinder
    Jobs/     UV_JobBoard, UV_Designations
    Agents/   UV_VaulterSim
    Networks/ UV_ConduitNetworks
    Gen/      UV_WorldGen
    Save/     UV_SaveSchema, UV_SaveSlots
    Render/   UV_TileInstances, UV_CellRenderFeature, UV_Overlays
              (pure packing/cull helpers split into own headers → headless-testable)
    UI/       UV_UI_* screens (ZM_UI_Bag pattern)
    Data/     UV_BuildingDefs, UV_TraitDefs, UV_ResearchDefs (constexpr tables)
  Components/                  # THIN ECS orchestrators only
  Tests/                       # units (*.Tests.inl) + automated e2e sessions
  Assets/                      # baked procedural kit, scenes, (later) audio
  Android/                     # Gradle tree (copy Games/Zenithmon/Android)
  Docs/                        # this doc set → grows the ZM living-doc skeleton
```

**Tiering rule** (CityBuilder precedent, `Games/CityBuilder/Source/CLAUDE.md`):
everything in `Source/` is a plain class constructible without a running
engine, unit-tested in the `Null_` config. `Components/` wire lifecycle, ECS
queries, and per-frame pumping — nothing else. `Source/Render/` is the one
Flux-facing folder; its pure halves (record packing, chunk culling) live in
separate headers so they still test headlessly.

### 1.2 Components (ECS orders 100+)

| Component | Order | Owns |
|---|---|---|
| `UV_WorldComponent` | 100 | **Everything by value**: `UV_CellGrid` + every Sim/Grid/Nav/Jobs/Networks system (the `CB_CityManagerComponent`/`CB_TerrainHeightfield` ownership pattern). `OnUpdate` pumps `UV_SimClock` and dispatches tick phases |
| `UV_VaulterComponent` | 110 | Per-colonist entity: identity, needs mirror, render binding. ≤16 entities |
| `UV_BuildingComponent` | 120 | Def id, occupied cells, construction progress, network endpoints, inventory |
| `UV_CameraHostComponent` | 130 | 2D pan/zoom state → engine ortho camera + picking |
| `UV_UIRootComponent` | 140 | Screen-stack orchestration over `Zenith_UIComponent` canvases |

Cells are **not** entities (150k-entity scenes would fight the per-entity heap
cost and scene serialization; see `Zenith_EntityStore.h` slot layout). Items
are **not** entities either — `UV_ItemStore` plain records inside
`UV_WorldComponent` (dig spam would churn ~3 heap allocations + a name string
per debris pile); items render through the instanced path and are referenced
by id from sweep/deliver errands.

Registration follows the template contract (`Build/Templates/NewGame/`):
components registered from `Undervault.cpp` at file scope AND `#include`d
there (MSVC dead-strip pitfall — a never-referenced `.obj` loses its static
registrar; `Zenith/EntityComponent/CLAUDE.md`).

### 1.3 House rules this TDD is bound by

- **No `std::` containers** — `Zenith_Vector`/`Zenith_HashMap`/etc. only.
- **Every new type/method ships dedicated unit coverage** (user mandate).
- **No legacy/back-compat code** — migrate callers + delete old surface in
  the same commit.
- Tabs; `PascalCase` functions; scope+type Hungarian per root `CLAUDE.md`;
  no pimpl; tools-only code under `#ifdef ZENITH_TOOLS`.
- **Headless is a build config, not a flag** (`Null_vs2022_Debug_Win64_True`);
  headless runs may CREATE a `.zscen` but never CHANGE a committed one.
- Scene setup via `Zenith_EditorAutomation::AddStep_*`; first run after
  scaffold must be a `*_True` config (it authors `Main.zscen`).
- Movement note: Vaulters are grid-sim movers rendered via interpolation —
  positions are written by the sim, not the physics engine. This is the
  sanctioned pattern for non-physical movers (they have no Jolt bodies), not
  a violation of the no-teleport rule (which governs physical characters).

---

## 2. ★ Engine gap analysis

**This section is the answer to "what must change in Zenith".** Sizes:
S ≈ a day · M ≈ a week · L = multi-week · XL = program. Phases: **P0** =
before the vertical slice can exist · **P1** = before the Android ship ·
**P2** = quality/post-launch.

| # | Change | Why Undervault needs it | Where | Size | Phase | Fallback if deferred |
|---|---|---|---|---|---|---|
| 1 | **Ortho camera**: public `InitialiseOrthographic()` + type/plane setters; add the missing Vulkan Y-flip `xOut[1][1] *= -1` to the ortho branch; unit tests | The whole game renders through it and engine passes consume the projection — unconditionally mandatory under 3D-in-ortho; `ScreenSpaceToWorldSpace` picking must agree with what is drawn | `Zenith/EntityComponent/Components/Zenith_CameraComponent.cpp` — verified: `:26` hardcodes `CAMERA_TYPE_PERSPECTIVE` (only `InitialisePerspective` exists), `:55` flips perspective, `:58-60` ortho branch does **not** flip | S | **P0** | None viable |
| 2 | **`Zenith_Gestures`**: tap / long-press / one-finger drag / pinch / two-finger pan / flick recognizers built on `Zenith_Pointers` claims; `Zenith_InputSimulator`-tested | The engine's only gesture primitive is `Zenith_Pointers::WasTapThisFrame` (300 ms/15 px). Pinch-zoom, long-press, drag-paint do not exist anywhere in the repo — and pan/pinch IS this game's camera | `Zenith/Input/` (beside `Zenith_Pointers`) | M | **P0** (minimum: pan/pinch/tap) | Game-side `UV_Gestures` (S) shipping only the minimum set; promote to engine later (move + delete same commit) |
| 3 | **`Zenith_PriorityQueue<T>`** — binary heap over `Zenith_Vector` | A* open list. `Zenith/Collections/` has Vector/HashMap/HashSet/CircularQueue/MemoryPool and **no heap**; `std::priority_queue` is banned | `Zenith/Collections/` | S | **P0** | Game-side heap in `Source/Nav/`, promote later |
| 4 | **Quad budget**: `FLUX_MAX_QUADS_PER_FRAME` 1024 → per-game configurable (4096) or split UI/world budgets | The build menu + HUD + roster grids will exceed 1024 quads, and the budget is shared with every UI canvas (canvas `Render()` batches into `Flux_QuadsImpl`); overflow silently drops newest quads | `Zenith/Flux/Quads/Flux_QuadsImpl.h:12` | S | **P0** | Aggressive list virtualization in every scrolling screen |
| 5 | **Vulkan surface recreation** across `APP_CMD_TERM_WINDOW` → `INIT_WINDOW`: verify on device, then fix | `VkSurfaceKHR` is created exactly once (`Zenith_Vulkan.cpp:287`, inside `Initialise`) and there is **no `vkDestroySurfaceKHR` anywhere in the repo**; on resume, `RecreateSwapchain()` queries capabilities of a surface whose `ANativeWindow` died. Backgrounding is the single most common thing a phone does to a game | `Zenith/Vulkan/Zenith_Vulkan.cpp` + `Zenith/Android/Zenith_Android_Main.cpp:75-77` | M (verify + fix) | **P0 spike (verify) / P1 (fix)** | None — ship blocker if broken |
| 6 | **Audio subsystem MVP**: new `Zenith/Audio/` on **miniaudio** (MIT, single-header; WASAPI on Windows, AAudio/OpenSL on Android); OGG via stb_vorbis; 16-voice mixer; SFX/Ambience/UI/Music buses; one-shot + looping streams; main-thread API; `Null_`-config no-op backend so CI links and runs silent | The engine has **no audio whatsoever** — verified: only `Zenith_AudioBus` (`Zenith/Core/Zenith_AudioBus.h`), a test-recording stub that compiles to `(void)` casts in shipping builds; zero audio backends, zero decoders, zero audio assets in git, no audio lib in the Android link list | new `Zenith/Audio/` + `Middleware/miniaudio` + `Build/Sharpmake_*` link lines | L | **P1** (the slice ships silent) | Ship-silent — unacceptable for v1 |
| 7 | **ASTC texture path**: astcenc in FluxCompiler/ZenithTools export + `TEXTURE_FORMAT_ASTC_*` in the enum + loader/memory-manager support + per-platform pipeline flag | Compressed formats today are BCn only (`Zenith/Flux/Flux_Enums.h:62-66` — BC1/3/5/7). Mali/PowerVR have no BC support → every texture ships **uncompressed** there: ~16.8 MB vs ~4.2 MB per 2048² atlas, 4× the bandwidth | `Tools/Zenith_Tools_TextureExport.*`, `Flux_Enums.h`, `AssetHandling/Zenith_TextureAsset`, `Zenith/Vulkan/Zenith_Vulkan_MemoryManager_Textures` | M | **P1** | Uncompressed on Android — slice-viable on 4 GB devices, wrong for ship |
| 8 | **Save-on-background**: handle `APP_CMD_SAVE_STATE`/`APP_CMD_PAUSE` → an engine app-event hook games subscribe to | `OnAppCmd` today does not handle `APP_CMD_SAVE_STATE` and there is no pause hook; Android process death is routine and a lost colony is the 1-star review | `Zenith/Android/Zenith_Android_Main.cpp` + a small engine event surface | S | **P1** | 60 s real-time autosave bounds loss (GDD §16.2) |
| 9 | **Frame cap / pacing**: 30 fps cap (`ANativeWindow_setFrameRate` or Swappy) + battery posture | Present mode is hard-coded FIFO (`Zenith_Vulkan_Swapchain.cpp` `ChooseSwapPresentMode` — loops for `eFifo`, asserts otherwise) and `Zenith_Window::SetVSync` stores a bool nothing reads on Android: the game runs at panel rate, 60+ Hz, burning battery on a sim that idles the GPU | Vulkan present path + `Zenith/Android/` | M | **P1** | Run at 60 — battery cost, thermal risk |
| 10 | **Render scale decoupled from TAA + reachable on Android** (or: evaluate TAA-on-mobile deliberately) | Under 3D-in-ortho the deferred pipeline stays ON, so G-buffer at 0.7–0.8× is the main GPU relief valve. Today render scale exists only as the TAA-coupled `m_fRenderScaleRequested` (`Flux_GraphicsImpl`, clamped [0.5, 1.0]) driven by a debug variable or `--render-scale` — and **neither is reachable on Android** (no ImGui, and `Zenith_CommandLine::Parse` never runs there) | `Zenith/Flux/Flux_Graphics.cpp` / TAA chain + gap #12 plumbing | M | **P1** | Native-res only; lean on the 30 fps cap |
| 11 | **Safe-area / DisplayCutout**: manifest cutout mode + inset query → UI canvas margins | Zero cutout/inset handling in repo (bare `NativeActivity` manifests); a full-bleed landscape HUD clips under punch-holes | `Zenith/Android/` + `Zenith/UI/` canvas | S–M | **P1** | Fixed conservative edge margins |
| 12 | **Android config plumbing**: asset-packaged config file (or intent extras) parsed into the `Zenith_CommandLine` surface at boot | No way to set any flag on device — every CLI-gated knob is inert on Android. Needed for QA A/B, render scale, tier overrides | `Zenith/Android/` + `Zenith/Core/Zenith_CommandLine` | S | **P1** | Hardcode per-build in `Project_*` init |
| 13 | **Device-gate relaxation**: drop `tessellationShader` + `fillModeNonSolid` from the hard gate for games that don't use them (make the gate per-project data); review the bindless UAB floor (1000) against low-end Mali | `s_bIsPhysicalDeviceHardSuitable` (`Zenith/Vulkan/Zenith_Vulkan.cpp:979-1028`) **rejects outright** any GPU lacking tessellation, non-solid fill, descriptor-indexing UAB, etc., with no fallback path. Undervault never tessellates; every rejected low-end phone is lost market for no benefit | `Zenith_Vulkan.cpp:979-1028` + `QueryDescriptorIndexingLimits` | S code + M device-matrix validation | **P1** | Accept reduced device coverage (most 2019+ chips pass) |
| 14 | **9-slice UI widget** | Panel/frame art without stretch artifacts; the UI framework's 11 widgets have no sliced image | `Zenith/UI/` | S | **P1** | Tiled `Zenith_UIRect`s + borders |
| 15 | **Android CI leg**: arm64 AGDE compile+link gate + APK assembly job (the `Null_` unit suite already covers logic host-side) | Android has **zero CI** (`Docs/BuildSystem.md` §8) with a documented history of silent bit-rot (MSVC-isms, a UAF found only at a later bring-up) | CI workflows + a runner with the AGDE toolchain | M | **P1** | Manual per-release device checklist (§16.5) |
| 16 | **AAB + Play packaging**: `zenith package` grows an Android target producing a signed AAB | Store submission needs AAB; today the pipeline stops at APK and `zenith package` is Windows-only | `Build/`, `Tools/` | M | **P1 (store gate)** | Internal-track/sideload APK for beta |
| 17 | **Slim instance path** for static tiles: position+tint (≈16 B) records instead of full 4×4 matrix + anim + bounds (≈96 B) with per-frame re-upload | `Flux_InstanceGroup::UpdateGPUBuffers` uploads transform+anim buffers **every frame** (`Flux_InstanceGroup.h:99-116`); ~24.6k tile instances ≈ 2.4 MB/frame ≈ 72 MB/s of bus traffic for tiles that almost never move | `Zenith/Flux/InstancedMeshes/` or the UnifiedMesh object path | M | P2 (after measurement) | Ship full-fat instances — inside budget (§15), just wasteful |
| 18 | **UI canvas scaling mode**: width-aware / match-shorter-side (today height-only: `m_fScaleFactor = size.y / reference.y`, `Zenith_UICanvas.cpp:489`) | Aspect spread 20:9 phones ↔ 4:3 tablets drifts horizontal anchoring | `Zenith/UI/` | S–M | P2 | Generous anchors + margins (MVP does this anyway) |
| 19 | **Quality tiers + thermal awareness** (`AThermal` headroom), optional dynamic resolution | Ship polish: fixed 30 fps + render scale covers v1 | `Zenith/Core` + `Flux` + `Android` | M | P2 | Static settings |
| 20 | IME / soft keyboard | **Never built** — the game designs around it permanently (GDD §15: zero text entry). Documented so nobody "helpfully" adds a text field | (would be GameActivity/InputConnection — XL) | XL | P2/never | The design-around IS the plan |

**Dropped relative to the 2D-sprite design** (the rendering fork the user
decided against — Appendix A): a G-buffer/deferred opt-out (the deferred
pipeline is now load-bearing, not overhead) and a sprite/atlas/flipbook
system (the engine's existing skeletal-animation path replaces it).

**Sequencing note:** P0 items #1/#3/#4 are ~3 days combined and unblock
everything; #2 can start game-side in parallel; #5's *verify* half is a
half-day device spike that must happen in Phase 0 because its answer decides
whether a multi-week fix enters the P1 plan. #6 (audio) is the long pole of
P1 — start it during Alpha, not after.

---

## 3. Cell grid & data layout

### 3.1 SoA arrays

Standard map 128×192 = 24,576 cells. Flat full-grid arrays (`index = y*W + x`,
row 0 at the bottom) so neighbour access is `±1 / ±W` — **chunks are metadata
only**, never storage.

```cpp
// UV_CellGrid — owned BY VALUE inside UV_WorldComponent
u_int8   m_aElement[2][N];      // element id (≤255), double-buffered
float    m_aMass[2][N];         // kg, double-buffered
float    m_aTemperature[2][N];  // K, double-buffered
u_int8   m_aFlags[N];           // phase cache | revealed | ladder | door | roomBoundary
u_int16  m_aOccupancy[N];       // building registry index, 0xFFFF = none
```

≈24 B/cell aligned → **0.59 MB** standard map, 1.33 MB Deep (192×288).
Double buffering: gather passes read buffer `t`, write `t^1`; the swap is a
single index flip at the end of tick phase 4.

### 3.2 Chunks (32×32) — metadata only

4×6 = 24 chunks on the standard map, each carrying: `bSettled` (fluid sim may
skip), `bRenderDirty`, `bPathDirty`, `bRoomDirty`, plus the §7 region label
table. Dirty flags are set by `UV_SolidMutation` and by any fluid pass that
moved mass across a chunk border (border moves un-settle both sides).

### 3.3 Mutation discipline

All topology changes (dig completes, build completes, deconstruct, door
state) are **queued** during a tick and applied by `UV_SolidMutation` at the
start of the next tick, sorted by cell index. Workers therefore never observe
a mid-tick topology change, and the apply order is deterministic regardless
of which Vaulter finished first within the tick. Dig yields 50% of cell mass
as an `UV_ItemStore` debris record.

---

## 4. Simulation algorithms

### 4.1 Fluid sim (`UV_FluidSim`) — gather flux + parity swaps

One element per cell keeps this integer-indexed and branch-light. Per tick:

1. **Flux pass (gather, parallel):** for every non-solid cell, compare mass
   with right and up neighbours only (2 faces/cell = every face once).
   Same-element faces move `(m_a − m_b) / 8` clamped to ≥0 remaining;
   gas-into-vacuum moves `m_a / 4`. Writes go to the back buffer;
   antisymmetry (`flow(a,b) == −flow(b,a)`) holds by construction because
   each face is computed exactly once.
2. **Swap passes (parity-coloured, parallel):** two checkerboard passes
   (even then odd cells) resolve **displacement**: liquid falls into
   gas/vacuum below; denser liquid sinks through lighter; lighter gas rises
   through heavier (hydrogen ceilings, CO₂ pits). A cell swaps with at most
   one neighbour per pass, and parity colouring means no two swaps touch the
   same cell in the same pass — deterministic under any worker count.
3. **Liquid levelling:** lateral halving of differences for liquid cells
   with headroom, folded into the flux pass; per-cell max ~1,000 kg with
   overpressure tolerance at depth.

Cost: ~2 face evaluations + ≤2 swap checks per cell per tick — O(N), ~50k
face ops on the standard map (budget §15). `bSettled` chunks (no motion for
16 ticks, no border traffic) skip passes 1–3 entirely.

### 4.2 Thermal sim (`UV_ThermalSim`)

Single gather pass over the same 2-face scheme:
`q = k_harm(a,b) · (T_a − T_b) · fDt`, with `k_harm` the harmonic mean of the
two elements' conductivities (correct behaviour at insulator boundaries —
Nullrock kills the face). Explicit Euler with a stability clamp
(`|ΔT| ≤ |T_a − T_b| / 2` per face) so extreme conductivity/mass ratios can't
overshoot. Vacuum faces conduct nothing. Buildings exchange with the cells
they occupy via their def's heat output (§8 networks apply machine heat here).

### 4.3 Phase transitions

Checked in the same pass, against `UV_ElementTable` transition rows, with
**±3 K hysteresis** (a cell that just froze must warm 6 K to re-melt) so
boundary cells never flicker. MVP: instant transition, mass preserved,
energy *ledgered* (recorded, not yet paid — the conservation oracle §16.2
tracks the ledger); v1 pays latent heat for real.

### 4.4 Determinism rules (binding)

- Fixed phase order (§5.2); mutations pre-sorted by cell index (§3.3).
- Gather formulation + parity-coloured swaps ⇒ **worker-count invariance**
  (digests identical at 0/1/7 workers — a pinned unit test, §16.2).
- All stochastic decisions use `hash(seed, uSimTick, uCellIdx)` (PCG-style
  mix), never a sequential RNG draw whose order workers could change.
- Sim TUs compile with `/fp:precise` and (clang/Android) `-ffp-contract=off`
  — FMA contraction differences are the known cross-ISA digest hazard. Hard
  determinism guarantee is **same-platform**; Windows-x64 ↔ Android-arm64
  digest equality is a tested goal, not a promise (risk register §19).
- No wall-clock anywhere in `Source/` (the engine's `--fixed-dt` automated
  runs must be bit-stable).

---

## 5. Sim clock & scheduling

### 5.1 `UV_SimClock`

Base tick **5 Hz** (`fSTEP = 0.2 s`) × speed `{0, 1, 2, 3}` → 0/5/10/15 Hz.
The accumulator copies the engine physics pattern
(`Zenith/Physics/Zenith_Physics.cpp:479-540` — the ZM-D-184 post-mortem is
the reason): cap **4 substeps per frame**, **discard** the remainder on cap
with a `Zenith_Warning` — a hitch loses sim time rather than death-spiralling.
Exposes `GetInterpolationAlpha() = m_fAccum / fSTEP` for render lerp.

The engine's own `OnFixedUpdate` path is **not** used: its 50 Hz step is a
private member with no setter (`Zenith_SceneSystem.h:532`) and no game
exercises it — the wrong rate, uncontrollable. `UV_WorldComponent::OnUpdate`
pumps the clock instead (CityBuilder pumps per-frame the same way; we replace
its frame-counted tick with this real accumulator).

### 5.2 Tick phase order (fixed, deterministic)

| # | Phase | Runs on |
|---|---|---|
| 1 | `UV_SolidMutation` apply (sorted) | Main |
| 2 | Fluid flux + parity swaps | Workers (row bands) → wait |
| 3 | Thermal conduction | Workers → wait |
| 4 | Phase transitions + buffer swap | Workers → wait |
| 5 | `UV_ConduitNetworks` (power, then pipes) | Main |
| 6 | `UV_Rooms` (only if dirty, ≤1 Hz) | Main |
| 7 | `UV_JobBoard` (errand gen, claim GC) | Main |
| 8 | `UV_VaulterSim` (needs, FSM, movement; collect path requests) | Main |
| 9 | `UV_Pathfinder::FindPathsBatch` | Workers → wait |
| 10 | `UV_Telemetry` + conservation ledgers | Main |

Worker phases use `Zenith_DataParallelTask(zone, pfn, pData, uBands,
/*bCallingThreadParticipates=*/true)` + `WaitUntilComplete()` — the engine
task pool is **flat** (no dependency graph, by design:
`Zenith/TaskSystem/Zenith_TaskSystem.h:9-11`), so phases sequence by
blocking, and the main thread works a band instead of idling. Bands =
`max(1, GetNumWorkerThreads())` row stripes; double buffering means bands
never write each other's cells.

### 5.3 Cadences & interpolation

Fluid/thermal/networks/needs: every tick. Rooms: on-dirty, ≤1 Hz. Autosave:
cycle boundary + 60 s real-time (GDD §16.2). Pathfinding: on demand, batched
per tick. **Interpolation contract:** every sim mover (Vaulters, falling
debris) publishes prev+curr tick poses; render lerps by alpha. The cell grid
renders current-state uninterpolated (5 Hz liquid steps are genre-authentic
and cheap). Camera is frame-rate, never tick-rate.

---

## 6. Agents & jobs

### 6.1 `UV_JobBoard`

Central, plain C++, main-thread — **deliberately not behaviour graphs**: the
scheduler is a cross-agent, sortable, claim-arbitrated computation, and the
graph runtime has no utility scoring and no cross-agent arbitration (verified
against `Zenith/Scripting/CLAUDE.md`; graphs are per-entity and event-driven).
Errand records: `{eType, xTarget(cell|building|item), uPriority 1-9,
eSkillFamily, xClaim}` bucketed by type + chunk.

Scoring (GDD §6.2, in order): personal chore-family setting (▲/●/▼/✕) →
player priority 1–9 → path-distance estimate (region-gated). Claims are
single-owner, exclusive; released on completion, death, interrupt, or
unreachability; claim GC each tick drops claims whose owner or target died.
Ties break on lowest cell index (determinism).

### 6.2 `UV_VaulterSim`

Per-Vaulter FSM: `Idle → SeekJob → Travel → Work → (repeat)`, with interrupt
overrides in priority order: **Breathe** (flee to nearest breathable region
cell — pre-empts everything) → Bladder → Sleep block → Eat → scheduled work.
Needs decay per tables in `UV_ElementTable`-style constexpr data; schedule =
12-block row (GDD §5.6). Movement: grid mover consuming a path (§7), position
= `(uCell, fProgress)` + prev/curr for render interpolation.

Optional cosmetic layer (post-slice): fire a `"VaulterTick"` custom event
into a per-Vaulter behaviour graph for barks/reactions — the DevilsPlayground
villager pattern (×17 in production, `DP_Villager.bgraph`) — with all
authoritative decisions staying in C++.

---

## 7. Movement & pathfinding

### 7.1 Traversal model

Node = cell; edges derived lazily from `m_aFlags` + solidity: **WALK**
(lateral onto floor-supported cells, ±1 step-up), **LADDER** (vertical on
ladder-flagged cells), **JUMP1** (1-cell gap cross at floor level). Costs:
walk 1.0, ladder 1.25, jump 1.5. Heuristic: Manhattan × min-cost
(admissible). Per-agent capability mask reserved (all Vaulters identical at
MVP).

### 7.2 Regions (`UV_Regions`)

RimWorld-style chunked connectivity: per-chunk flood labels + a border
adjacency graph between chunk-local regions; global reachability = BFS over
that small graph, cached per (region, region) query generation. Dig/build
dirties only the touched chunk(s) → re-flood 32×32 locally + rebuild that
chunk's border links. Every path request is region-checked first — an
unreachable target is rejected **O(1)** and the errand flagged, so A* never
burns a worst-case search on a walled-off order (the genre's classic perf
death).

### 7.3 A* (`UV_Pathfinder`)

Binary-heap A* on the new `Zenith_PriorityQueue` (gap #3), with
**generation-stamped** open/closed/g-score arrays (a `u_int16` stamp per
cell; bump one counter per search instead of clearing 24k entries). Batch
API mirrors the engine idiom (`Zenith_Pathfinding::FindPathsBatch` +
`Zenith_DataParallelTask`, `Zenith/EntityComponent/Zenith_AIWorldHooksInstall.cpp`):
all path requests collected in phase 8 run in parallel in phase 9 (each
worker gets its own stamp arrays). Result type copies the engine shape:
`{SUCCESS | PARTIAL | FAILED, waypoints}`. Node cap 4,096 per search
(regions make blowups near-impossible; the cap makes them impossible).
Repath policy: on claim, on topology dirty under the remaining path, and on
a 2 s stall.

Nothing from `Zenith/AI/Navigation` is reused: the navmesh is a 3D
offline-baked polygon graph ("do offline or at load") — wrong for terrain
that changes every few seconds — and its agent is a 3D waypoint follower.
CityBuilder's `CB_Traffic::FindPath` (O(V²) open-set scan over a spline
graph) is likewise a shape to avoid, not reuse.

---

## 8. Conduit networks (`UV_ConduitNetworks`)

### 8.1 Power

Circuits rebuilt on topology dirty by connected flood over wire cells +
endpoints — the `CB_Conduits::Energize` model
(`Games/CityBuilder/Source/CB_Conduits.h`) upgraded to per-network identity.
Per tick, per circuit: `fSupply = Σ generators (activity-gated)`,
`fDemand = Σ enabled consumers`; batteries discharge to cover deficit /
charge on surplus (round-robin across batteries for determinism); consumers
brown-out in reverse-priority order on true deficit. **Overload:** load above
the circuit's weakest wire rating for **6 consecutive ticks** → one
deterministic wire segment (lowest cell index among max-load spans) becomes a
damaged-wire repair errand.

### 8.2 Pipes

Per-segment single packet `{u_int8 uElement, float fMass, float fTemp}` —
**10 kg liquid / 1 kg gas** capacity. Segments advance one hop per tick in
**reverse-topological order** from consumers back toward pumps (a packet
vacates before its upstream neighbour advances — no compression logic
needed). Junctions arbitrate **strict round-robin** (persisted counter →
deterministic and save-stable). Pumps draw from their cell (500 g/s), vents
emit into the world through the §3.3 mutation queue. Pipe temperature
exchanges with the hosting cell at a low k; freezing/boiling in-pipe is a v1
break event.

Both nets are main-thread at MVP scale (hundreds of segments, §15 budget);
per-network `Zenith_DataParallelTask` fan-out is the escape hatch if profiles
demand it.

---

## 9. Rooms (`UV_Rooms`)

Flood over non-solid cells bounded by solid/door flags, abandoned above
**128 cells** (unenclosed). Classification from size + contained building
tags against a constexpr table (GDD §8.6 rows). Runs only when `bRoomDirty`
chunks exist, capped 1 Hz. Room ids stamp back into a per-cell `u_int8` lane
for the overlay and morale queries.

---

## 10. World generation (`UV_WorldGen`)

Pure `seed → UV_CellGrid` function, stage pipeline, one PCG32 stream **per
stage** (adding a stage never perturbs earlier stages' draws):

1. Biome banding (value noise over band template — GDD §3.2)
2. Per-biome element fill (weighted tables)
3. Ore-vein random walkers; Nullrock seam walkers
4. Fluid pockets (water/polluted/O₂/hydrogen chambers)
5. Start chamber carve: guaranteed breathable buffer (≥40 kg O₂ in reach),
   water pocket within 20 cells, Stasis Vault placement, 3 spawn points
6. Validation: reachability from spawn to water/algae/dig-frontier;
   breathable minimum; on failure re-roll affected stage, ≤8 attempts, then
   fall back to next seed (logged)

Digest-tested: fixed seed → CRC32 over all grid arrays = pinned baseline
(§16.2).

---

## 11. Save format

### 11.1 Layers (the Zenithmon two-layer pattern)

- **`UV_SaveSchema`** — pure module-framed codec, transactional:
  deserialize **every** module into a candidate, validate all, publish only
  if all pass (`ZM_SaveSchema` precedent, `Games/Zenithmon/Source/Core/`).
- **`UV_SaveSlots`** — wraps `Zenith_SaveData` (magic `ZENS`, format+game
  version, CRC32; engine calls `Initialise` exactly once — a game must
  never re-init it). **Verify re-probe after every write** because
  `Zenith_FileAccess::WriteFile` returns void (the ZM_SaveSlots lesson).
  Slots: `Save0/1/2/Auto`, `_Test`-suffixed under automation so tests can't
  eat real saves.

### 11.2 Modules

Each module: fourcc + `u_int16` version + `Zenith_StreamEnvelope` length
framing → **skip-unknown** on read. Per-module versions are the
forward-compat policy (deliberately *not* CityBuilder's single-version
refusal of old saves).

| Module | Contents |
|---|---|
| `META` | seed, map size, cycle, settings |
| `CLCK` | sim tick, accumulator, speed |
| `GRID` | cell arrays, **RLE by (element, run)** — mass/temp runs collapse hard post-worldgen |
| `VLTR` | Vaulters: identity, needs, skills, schedule, FSM, position |
| `BLDG` | building instances + inventories + progress |
| `ITEM` | debris/item records |
| `NETW` | circuits, batteries, pipe packets, junction counters |
| `JOBS` | designations, priorities, claims |
| `RSCH` | research state |
| `CAMR` | camera + open sheet + armed tool (resume-exactly, GDD §16.2) |

Typical size: grid raw ≈221 KB → RLE **30–80 KB**; everything else ≈20 KB;
cap 1 MB. Write cost <5 ms main-thread at MVP scale — no async needed.

---

## 12. Rendering (3D meshes in orthographic view)

### 12.1 Camera

Engine ortho camera post-fix (gap #1): `UV_CameraHostComponent` writes
`left/right/top/bottom = ±fHalfHeight·fAspect·fZoom / ±fHalfHeight·fZoom`
around the pan centre each frame; near/far bracket the shallow 3D depth
(≈ −8..+8 world units of relief). Picking via the existing
`ScreenSpaceToWorldSpace` (already generic over the inverse view-proj —
`Zenith_CameraComponent.cpp:67-111`). Fixed side view; zoom 8→96 px/cell.

### 12.2 Engine pipeline configuration

`Project_SetGraphicsOptions` (the `Games/CityBuilder/CityBuilder.cpp:337-352`
kill-list precedent + `Games/TilePuzzle/TilePuzzle.cpp:1478` as the
shipped-Android precedent): **OFF** — Shadows, SSR, SSGI, SSAO (Tier-A
re-enable candidate), HiZ, Grass, Terrain, Fog, Skybox, IBL. TAA off
initially via `Flux_TAAImpl::SetEnabled(false)` (revisit with gap #10 —
TAA is currently the only render-scale vehicle). **ON:** DynamicLights
(clustered — lamps/machines/magma are the look, GDD §13.2), DeferredShading +
HDR (the mandatory floor: `HDR_ToneMapping` is the only final-RT writer),
UnifiedMesh, Quads/Text (UI), Present. Remaining engine passes force-disabled
by owner via `Flux_RenderGraph::SetOwnerForceDisabled` where no options bool
exists.

### 12.3 World geometry through the existing pipeline

- **Tiles** — `UV_TileInstances` bridges the cell grid to
  `Flux_InstanceGroup` (`Zenith/Flux/InstancedMeshes/Flux_InstanceGroup.h`,
  `uMAX_INSTANCES = 131072` ≫ 24,576 cells): **one group per tile material**,
  one instance per solid cell, drawn by the UnifiedMesh GPU-cull + indirect
  path into the G-buffer like any opaque mesh. Per-instance
  `SetInstanceColor` is the overlay/selection tint lane. **Rule: instances
  are toggled with `SetInstanceEnabled`, never `RemoveInstance`** — removal
  is swap-and-pop and **shifts instance IDs** (verified,
  `Flux_InstanceGroup.h:67-68`), which would scramble a cell→instance map;
  the grid keeps a stable cell→(group, instance) table built at load.
  Full-fat instances cost ~96 B × 24.6k ≈ 2.4 MB re-uploaded per frame
  (`UpdateGPUBuffers` uploads every frame) — inside budget, and gap #17 is
  the P2 slim-down.
- **Buildings** — baked meshes via `Zenith_ModelComponent` (one entity per
  building — hundreds, fine) or instance groups for the mass-produced ones
  (ladders, tiles-as-buildings, wire posts). Powered-state emissive via
  material param; per-object tint lane carries selection/overlay.
- **Vaulters** — skeletal meshes on the existing GPU compute-skinning path
  (`Zenith_AnimatorComponent` → UnifiedMesh Stage-5 skinning): one shared
  procedural rig, per-Vaulter palette; animation states driven from the FSM
  (§6.2). ≤16 skinned characters is far below engine precedent.

### 12.4 `UV_CellRenderFeature` (the one custom pass)

Registered via `Zenith_GameRenderFeatures::Register` — the
`Games/DevilsPlayground/Source/DPFogPass.cpp:187-193` template (name
`"UV_Cells"`, `m_szRunAfter = "HDR"`), drawing after tonemap into the final
RT (post-lighting compositing, exactly where engine Quads/Text sit). Inputs:
a per-cell SSBO uploaded for **dirty chunks only** — record 12 B:
`{u_int16 uTileId, u_int16 uVariantOrFrame, u_int32 uOverlayRGBA,
u_int16 uFillLevel, u_int16 uFlags}` (24,576 × 12 B × 2 in-flight ≈ 0.59 MB).
Draw layers, vertex-buffer-free (`SV_VertexID` quad expansion — the
`Flux/Vegetation` idiom; `drawIndirectFirstInstance` is guaranteed by the
device gate):

1. **Liquids** — per-cell fill quads (`uFillLevel` → meniscus height,
   animated wobble), element-coloured, alpha-blended.
2. **Gas haze** — low-contrast tint + drifting motes for non-O₂ gases.
3. **Fog-of-unexplored** — revealed-bit with neighbour-sampled soft edge.
4. **Designation/selection tints** + building ghosts.
5. **Overlay tint** — active-overlay colour ramp (LUT in `UV_Overlays`)
   over everything (only when an overlay is armed).

This is deliberately the *only* custom GPU surface in the game — everything
mesh-like rides the engine's existing pipeline.

### 12.5 Bandwidth honesty (why this is P1-watched)

The deferred floor is real: 4 G-buffer MRTs (~24 B/px:
`Flux_GraphicsImpl.h` — RGBA8 + RGBA16F + RGBA8 + RGBA16F + D32) + RGBA16F
HDR scene + 64 bpp final RT, no subpass/tile-memory merge in the Vulkan
backend. At 1080p ≈ 150–200 MB/frame ≈ **4.5–6 GB/s at 30 fps** against
~14 GB/s LPDDR4X on Tier B — workable but the single biggest GPU cost, which
is why render-scale reachability (gap #10) and the 30 fps cap (gap #9) are
P1, and why Mali-class Tier B devices are the §16.5 checklist's first
citizens. Vertex load is trivial (24.6k boxes × 12 tris, GPU-culled).

### 12.6 Art pipeline

Procedural kit generated + baked by `ZENITH_TOOLS` builds (Zenithmon
pillar-2 pattern): tile meshes + materials per biome recipe, building meshes,
one Vaulter rig + animation clips — all deterministic from seeds, re-bake
byte-identical, committed under `Assets/` per the asset-manifest discipline.
No hand-made art, no Assimp imports at runtime (Android has no tools build).

---

## 13. Input & gestures

### 13.1 Bindings

`Source/UV_Bindings.h` — the one production file allowed to spell a raw key,
pad button, or virtual-source id (`ZM_Bindings.h` pattern, input program C2).
Profiles: `P_TOUCH {TOUCH}` (Android boot), `P_DESKTOP {KEYBOARD, MOUSE}`
(Windows dev: WASD pan, wheel zoom, LMB = touch-primary via the engine's
touch→mouse B3 symmetry), `P_GAMEPAD` deferred to post. Actions (from
`uINPUT_ACTION_FIRST_GAME_ID`): `Pause`, `Speed1/2/3`, `CancelTool`,
`CycleOverlay`, `SystemBack` (Android Back closes top sheet — the ZM
mask-exempt pattern). Camera + tools are **gesture-fed, not action-fed**.

### 13.2 Gesture recognizer

Engine `Zenith_Gestures` (gap #2; game-side `UV_Gestures` fallback with
identical API). State machine per pointer over `Zenith_Pointers` (8 slots,
`(slot, generation)` handles): DOWN arms; movement beyond slop (15 logical
px) commits **PAN** (default) or **PAINT** (tool armed) and **claims** the
pointer (owner id `UV_GESTURE`) so UI can't steal mid-gesture; second DOWN
while armed/panning promotes to **PINCH** (continuous centroid + separation
deltas, claims both); stationary 500 ms → **LONG_PRESS**; UP within 300
ms/15 px unclaimed → **TAP** (per-pointer — the global
`Zenith_Pointers::WasTapThisFrame` is insufficient for multi-touch UI
coexistence); release velocity → **FLICK** inertia (reuse the ScrollView
deceleration constant for feel consistency). Claims released via
`ReleaseAllClaimsForOwner` on cancel and on the engine's
`INPUT_EVENT_LIFECYCLE_RESET` barrier (backgrounding cancels gestures
cleanly — the engine already guarantees pointer cancel delivery).

Ordering: runs at the frame contract's gameplay-input step, **after** UI
claims — a finger that pressed a HUD button never pans the camera
(first-claim-wins is the engine rule; we inherit it).

### 13.3 Testability

Every recognizer transition unit-tested headlessly via
`Zenith_InputSimulator::SimulateTouchDown/Move/Up/Cancel(iPointerId, x, y)` —
tap vs drag slop, pinch promotion, claim suppression, lifecycle cancel.

---

## 14. UI

Built entirely on `Zenith/UI/` (11 widgets — the engine's most mature,
touch-native area): Rect/Text/Image/Button/Toggle/Overlay/ScrollView (has
inertia)/LayoutGroup/GridLayoutGroup/VirtualStick/VirtualButton. Patterns:
non-ECS POD presentation classes per screen (`ZM_UI_Bag.{h,cpp}` template),
widgets authored once at bake, re-resolved by name each frame via
`FindElement<T>` (never cache element pointers — canvas may relocate),
geometry as `static constexpr` shared with the single placement site, pure
statics unit-tested headlessly.

Screen → widget mapping (GDD §11.6): HUD strips = Rect/Text/Image/Button;
build menu = Overlay + tabs + ScrollView + GridLayoutGroup of Buttons;
inspector = Overlay bottom-sheet + LayoutGroup; priorities screen = Grid of
Toggle-cycled cells; schedule painter = **composite**: GridLayoutGroup cells
+ pointer-claim drag painting (same mechanism as world paint tools);
roster = ScrollView rows. New widget needed: **9-slice** (gap #14). Canvas
scaling: height-only today (`Zenith_UICanvas.cpp:489`) — MVP authors with
generous anchors; width-aware mode is gap #18.

**Quad budget accounting:** every widget renders through `Flux_QuadsImpl`
(shared `FLUX_MAX_QUADS_PER_FRAME = 1024`, drop-newest on overflow). Build
menu (~40 cards × ~6 quads) + HUD (~80) + sheet (~60) + roster (~120)
comfortably exceeds it → gap #4 is P0. Until it lands: virtualize every
scroll list (only visible rows exist as widgets).

---

## 15. Performance & memory budgets

### 15.1 Device tiers

| Tier | Reference | Floor |
|---|---|---|
| **A** (target) | Snapdragon 778G / Dimensity 1080 class, Adreno 642L / Mali-G68, 6 GB, 2021+ | 30 fps native, 60 fps opt-in, SSAO opt-in |
| **B** (floor) | Snapdragon 680 / Adreno 610, Mali-G52, 4 GB, Vulkan 1.1 | 30 fps, render scale 0.75, standard map |

Tier B admission depends on gap #13 (the current device gate would reject
some of it outright). Emulator (x86_64) is a **functional** target only —
timings are diagnostic, never a gate (house rule, `Zenith/Android/CLAUDE.md`).

### 15.2 CPU (standard map, 24,576 cells, mid-tier big core)

| Item | Cost/tick | Notes |
|---|---|---|
| Fluid (flux + swaps) | 0.5–1.0 ms single-core → ~0.3 ms banded | ~50k face ops + parity swaps; settled chunks skip |
| Thermal | 0.3–0.5 ms | single gather pass |
| Phase transitions | ~0.3 ms | branchy scan |
| Networks + rooms + jobs + needs | ≤0.5 ms | MVP scale |
| Pathfinding (12 Vaulters) | ≤1 ms batched on workers | region-gated, 4,096-node cap |
| **Total** | **≈2–4 ms/tick** | vs 8 ms budget → 2× headroom → 3× speed (15 Hz) sustainable at 30 fps |

Frame @33.3 ms: input+UI 2 · sim 0.4 amortised at 1× (≤8 worst tick-frame) ·
render record ≤4 · engine ~3 · slack absorbs tick frames. GPU: §12.5.

### 15.3 Memory (runtime RSS target < 400 MB Tier B)

Engine RT floor ~90 MB (G-buffer + HDR + final + depth at 1080p — the
deferred cost of the 3D-ortho decision) · cell arrays ~0.6 MB + render
records ~0.6 MB + nav ~0.2 MB · baked mesh/material kit ~40 MB · textures
8–16 MB ASTC (33 MB uncompressed until gap #7 — the P1 argument in numbers)
· audio 10–20 MB (post-gap #6) · UI/fonts ~10 MB · code+misc remainder.

### 15.4 Measurement methodology

`UV_Telemetry` per-tick ring (tick ms by phase, per-element mass totals, job
throughput) surfaced in the TOOLS debug panel and dumped by automated runs;
on-device numbers only from real hardware (adb + the engine's log sink);
budgets in this section are re-baselined at each roadmap gate — the Phase-0
device spike (§20) replaces these estimates with measurements before
anything depends on them.

---

## 16. Testing & CI

### 16.1 Conventions (repo law)

`ZENITH_TEST` units in `*.Tests.inl`, `#include`d from an **always-linked
TU** (MSVC dead-strips unreferenced registrars — root-cause documented in
`Zenith_TestFramework.h:263`); run at every boot of the `Null_` exe.
Baselines exact-match (`ran == baseline`) in `Tools/unit_baselines.json`,
**pinned only from a `Null_` run** (Vulkan exes report differently — house
rule), bumped in the same commit as any added test. Automated tests via
`Zenith_AutomatedTest` (+ `--fixed-dt` for determinism), driven by
`zenith test Undervault`.

### 16.2 Per-system suites (the mandate: every new type gets coverage)

- **Fluid:** exact per-tick mass conservation (double-precision sums);
  flux antisymmetry; equilibrium convergence; no-flow across solid/vacuum;
  **worker-count invariance** (identical digests at 0/1/7 workers).
- **Thermal:** energy conservation vs ledger; no vacuum conduction;
  harmonic-mean insulator cases; hysteresis (no flicker at boundary);
  stability clamp under extreme ratios.
- **WorldGen:** fixed seed → pinned CRC digest; start-chamber invariants;
  re-roll cap.
- **Regions/Pathfinder:** known-map shortest paths (walk/ladder/jump);
  incremental region repair ≡ full re-flood under mutation fuzz; O(1)
  unreachable reject; node cap.
- **JobBoard:** claim exclusivity under contention; scoring truth table;
  claim GC on death/unreachable.
- **VaulterSim:** needs decay; interrupt precedence (Breathe pre-empts all);
  schedule transitions.
- **Networks:** wattage/battery/brown-out; 6-tick overload determinism;
  packet advance order; junction round-robin stability across save/load;
  split/merge on cut.
- **SaveSchema:** round-trip bit-equality; per-module version skew
  (unknown module skipped, load succeeds); CRC-corrupt reject;
  validate-all-then-publish (failing module mutates nothing); slots
  re-probe-after-write.
- **SimClock:** cap-and-discard + warning; speeds; alpha bounds.
- **Render pure halves:** record packing; chunk cull rects; overlay LUTs.
- **Gestures:** §13.3.
- **Determinism master:** fixed seed + 2,000-tick headless run → digest of
  (grid + Vaulters + RNG cursors + networks) = pinned baseline; matrix over
  worker counts; Windows-x64 vs Android-arm64 as a tracked (non-gating)
  comparison.
- **Conservation oracles:** per-element mass and energy vs explicit
  source/sink ledgers — `Zenith_Assert` per tick in Debug, telemetry counter
  in Release, 100k-tick nightly soak.

### 16.3 End-to-end

`Zenith_AutomatedTest` + `Zenith_InputSimulator` scripted colony session
(the `CB_HumanSession` shape): boot → new game fixed seed →
`SimulateTouchDown/Move/Up`: pan, pinch (two pointers), open build menu,
place ladder, paint a dig rect, run 300 ticks, save, reload, assert state
digest. Runs headless in `Null_` (state asserts, no pixels); a `Vulkan_`
windowed smoke on Windows confirms boot + draw.

### 16.4 CI

Windows legs from day one (`Null_` unit gate + automated batch — the
existing per-game pattern; new `Undervault` row in `unit_baselines.json`
pinned at first landing). **Android leg is gap #15** (P1): arm64 AGDE
compile+link + APK assembly; logic stays host-gated.

### 16.5 Device checklist (what headless cannot see — per milestone)

Cold boot · background/resume ×10 (the gap-#5 surface risk) · process-kill
restore (≤60 s loss) · 60-min battery/thermal soak (≤12%/hr Tier A) ·
gesture feel/latency · overlay legibility at min zoom + CVD simulation ·
cutout devices · 3-device spread (Adreno + Mali + one Tier B).

---

## 17. Android platform notes

- **Scaffold:** `zenith new Undervault` → set `"android": true` in
  `Undervault.zproj`; copy the Gradle tree pattern from
  `Games/Zenithmon/Android/` (`com.zenith.undervault`), multi-ABI via the
  shared `Build/zenith_android_abis.gradle` axis (arm64-v8a devices +
  x86_64 emulator — the only ABI an ARM-less dev box can execute).
  **Until then, no `Games/Undervault/` folder may exist**: the descriptor
  scan errors on a game folder with no `.zproj`
  (`Build/zenith_buildsystem.psm1:306`) and `regen.ps1` then fails with
  "nothing regenerated" — which is why this doc set lives at
  `Docs/Undervault/` today and moves to `Games/Undervault/Docs/` at
  scaffold time.
- **Assets:** AAssetManager path (empty `GAME_ASSETS_DIR` under AGDE);
  Gradle asset dirs must include game `Assets/`, engine `Zenith/Assets`,
  **and `Zenith/Flux/Shaders`** — pre-compiled `.spv` + `.spv.refl` are the
  only shader path on device (no Slang runtime); a game that omits the
  shader dir renders nothing (only 3 of 5 current games bundle it — copy
  Zenithmon's build.gradle, not Combat's).
- **Manifest:** landscape-locked (game-design choice, ZM precedent —
  surface rotation itself is solved engine-side via IDENTITY preTransform);
  add cutout mode with gap #11.
- **Packaging law:** static STL `cpp_static` (AGDE's exact token), 16 KB
  page alignment flags, AGP ≥ 8.5.1 — all already encoded in the shared
  Gradle/Sharpmake config; don't hand-roll.
- **Install:** `adb install -r -t` (debug APKs are testOnly).
- **Lifecycle:** the engine already delivers the input barrier
  (release-held-keys/cancel-pointers) on pause/resume; Undervault adds:
  pause sim + save on the background hook (gap #8), gesture cancel via the
  lifecycle claim release (§13.2).
- **No CLI on device:** every knob must reach the game via gap #12's config
  file or hardcoded `Project_*` defaults until it lands.

---

## 18. Telemetry & diagnostics

`UV_Telemetry` counters (tick ms by phase, mass/energy ledgers, job
throughput, deaths, alerts) ring-buffered; TOOLS-only debug panel
(`#ifdef ZENITH_TOOLS`): sim inspector (cell probe: element/mass/temp/flags),
overlay force-modes, tick single-step, conservation ledger view, gesture
visualizer, touch-target audit overlay (GDD §17). Automated runs dump the
ring beside test results (`Build/artifacts/` convention — never committed).

---

## 19. Risk register

| Risk | Exposure | Validation / mitigation |
|---|---|---|
| **Surface recreation on resume** (gap #5) | App unusable after backgrounding — ship blocker | Phase-0 device spike ×20 cycles; fix scheduled P1; §16.5 every milestone |
| **Deferred bandwidth on Mali Tier B** (§12.5) | 30 fps miss on the floor tier | Phase-0 spike measures the real floor; levers: render scale (gap #10), 30 fps cap (#9), map size, Tier-B feature trim |
| **Audio is greenfield** (gap #6) | L-sized engine subsystem on the ship path | Start during Alpha; miniaudio keeps backend scope thin; slice ships silent by plan |
| **Cross-ISA determinism** (§4.4) | x64↔arm64 digest drift → platform-split saves/tests | fp-contract policy; same-platform guarantee only; tracked comparison, non-gating |
| **Sim cost estimates wrong** (§15.2) | Tick budget blown at 3× | Estimates are 2× headroom; settled-chunk skipping unimplemented reserve; Phase-0 measures |
| **Gesture feel** | The whole UX thesis | Vertical-slice hands-on test is the gate (GDD §20); recognizer params data-driven for tuning |
| **Instance-path churn** (§12.3) | 2.4 MB/frame upload + enable/disable churn on big digs | Budgeted; gap #17 is the measured escape hatch |

---

## 20. Roadmap

**Phase 0 — Spikes (1–2 wk).** ① Device-reality spike: scaffold
(`zenith new Undervault`), kill-list applied, 24.6k instanced boxes + a stub
cell SSBO pass on Tier A + Tier B phones → measure frame/memory (replaces
§15 estimates). ② Surface-lifecycle spike (gap #5 verify). ③ P0 engine trio
(#1 ortho, #3 heap, #4 quads — ~3 days). ④ Fluid-feel prototype on Windows
(tune flow constants at 5 Hz). Exit: measured budgets, go/no-go on Tier B.

**Vertical Slice (6–8 wk, Windows-primary + Android boot demo).** CellGrid →
SimClock → FluidSim (O₂/CO₂ + water) → TileInstances + CellRenderFeature →
dig/mutations → Regions/A* → JobBoard → VaulterSim (breath/calories) →
camera+gestures (game-side minimum ok) → save/load → e2e test. Exit: GDD
§20's gesture test passes on-device; 30-min playable loop; determinism +
conservation suites green; `Undervault` baseline row pinned.

**Alpha (10–12 wk).** MVP feature-complete per GDD §19 MVP column: power +
plumbing networks, food chain, hygiene, schedules/morale/rooms, thermal +
transitions, worldgen biomes, all MVP overlays + screens, research rungs,
Warden Protocols; **audio subsystem lands** (gap #6). Exit: 2-hour session
no soft-locks; ≤8 ms/tick on Tier A device; 100k-tick soak clean.

**Android Beta (6–8 wk).** All P1 gaps closed (#5 fix, #7–#16); device
matrix green; Play internal track. Exit: Tier B 30 fps at 3×; battery
≤12%/hr Tier A; restore-after-kill ≤60 s loss; crash-free ≥99.5%.

**v1.** GDD v1 column content; store submission (AAB); P2 items only as
Beta data demands. Exit: release checklist + store approval.

---

## 21. Appendices

### A. The 2D sprite/tilemap alternative (not chosen)

The rendering fork the user decided against, kept for the record: a single
game render feature drawing the *entire* world as textured quads (chunked
12 B/tile SSBO, vertex-buffer-free expansion, bindless atlas) after HDR,
with flipbook sprite animation for Vaulters. Trades: −G-buffer bandwidth
(the deferred floor becomes pure overhead → an opt-out gap would be P2),
−3D art pipeline, +a sprite/atlas/flipbook system the engine lacks, +a new
2D animation asset path, −the engine's existing lighting (lamp glow would be
shader-faked). Sim, input, UI, save, and testing sections of this document
are identical under either fork — the seam is `Source/Render/` only.

### B. Element table schema (code authority: `UV_ElementTable`)

`{u_int8 uId, const char* szName, ePhase, fSpecificHeat (J/kg·K),
fConductivity (W/m·K), fDensity, fMeltK, fBoilK, u_int8 uMeltsTo,
u_int8 uBoilsTo, fMaxCellMass, uAtlasTile, uHardness (dig ticks)}` —
`constexpr` array, GDD §4.2 is its design mirror.

### C. Building def schema (code authority: `UV_BuildingDefs`)

`{uId, szName, eDomain, uW, uH, fPowerW (+gen/−draw), fHeatW,
aBuildCost[{eElementClass, fKg}], eResearchRung, uRoomTag, ePlacement
(floor/wall/free), uPriorityDefault}` — one `constexpr` table, all tuning in
one place (CityBuilder `CB_BuildingDefs.h` precedent, GDD §7 mirror).

### D. Save module quick-reference

§11.2 table; envelope = `Zenith_StreamEnvelope` (magic/version/type/length,
non-destructive peek); slot layer = `Zenith_SaveData` (`ZENS`, CRC32).

### E. Glossary

**Vaulter** colonist · **Wardstone** indestructible world border ·
**Nullrock** insulator mineral · **Stasis Vault** population source ·
**cycle** 300 s sim shift (1,500 ticks) · **block** 25 s schedule unit ·
**errand** one claimable unit of work · **designation** painted player
order · **region** connectivity component for O(1) reachability ·
**settled chunk** fluid-quiescent 32×32 area the sim skips.

### F. iOS (future work, out of scope)

The engine has no iOS platform layer, no Metal/MoltenVK path, and no Apple
build pipeline; supporting iOS is an engine *program* (platform layer +
windowing/input/lifecycle + MoltenVK bring-up + toolchain), not a game task.
Nothing in this TDD's game architecture blocks it later: the sim is
platform-pure, rendering rides Flux's backend abstraction, and gestures sit
on the neutral pointer table. Revisit only if the engine grows the layer.
