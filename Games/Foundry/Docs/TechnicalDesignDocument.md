# Foundry — Technical Design Document

**Status:** DRAFT for review · **Date:** 2026-08-21
**Companion:** [Game Design Document](GDD.md) — vision, systems, touch UX, scope.
**Ground truth:** every engine capability or limit stated here was verified against the tree on 2026-08-21 and carries its source path. Line numbers drift; symbol names are the durable reference.

---

## 0. Summary

Foundry is a Factorio-class factory sim targeting **Android first** at **gigantic scale** (100k+ placed buildings, 250k+ items in flight). Three findings shape everything below:

1. **The engine ECS cannot host the simulation** at this entity count (per-entity meta dispatch + hashmap-backed component index — §2). The sim is therefore a self-contained, deterministic, integer-only core (**SimCore**) with the ECS reduced to a handful of manager entities, exactly the CityBuilder orchestration pattern at a larger scale.
2. **Floating-point determinism is unavailable by construction** (`/fp:fast` on all Windows configs with documented `/Od`-vs-`/O2` divergence, different FP semantics on Android clang — §4). SimCore is integer/fixed-point only, which makes cross-platform bit-exactness *cheap* instead of impossible.
3. **The renderer's instanced pipeline is architecturally right and quantitatively unproven** (GPU-driven culling + indirect draws exist and run on Android, but the per-frame path is full-rebuild/full-upload and the largest instance count ever exercised in-tree is 2,520 — §15). The design bounds render cost by *visible-set extraction* so factory size never reaches the renderer, and files an engine improvement to remove the remaining flat cost.

The engine improvements Foundry requires are consolidated in **§19 — the engine-change register** (16 items, ranked, with effort and deadline milestones).

---

## 1. Targets & budgets

### Reference hardware

| Tier | Device class | Commitment |
|---|---|---|
| Floor | Snapdragon 7-series / Dimensity 8000 class, 8 GB, Adreno or Mali, 2024-era | 30 UPS sim + 60 fps render at default settings; 30 fps battery mode |
| Comfort | Snapdragon 8-series class | 30 UPS + 60 fps with all effects |
| Desktop | Any Vulkan 1.1+ dev machine | 30 UPS + uncapped render |

Mali is a first-class target and currently a silent risk: the texture pipeline is BC-only and `textureCompressionBC` is not checked in device suitability (`Zenith/Vulkan/Zenith_Vulkan.cpp`, `s_bIsPhysicalDeviceHardSuitable`) — register #5.

### Budgets (tracked from M1; op-count-gated in CI from M0)

| Budget | Target |
|---|---|
| Sim tick (30 UPS ⇒ 33.3 ms period) | **≤ 8 ms on the floor device** big core; ≤ 5 ms steady-state, worst case during a max wave ≤ 8 ms |
| Per-system tick slices (steady / worst) | belts 2.0 · machines+wheel 1.0 · power 0.3 · fluids 0.3 · trains 0.5 · drones 0.5 · circuits 0.4 · threat 1.0/3.5 · PostTickExtract 1.0 (ms) |
| Render frame (floor device) | ≤ 16.6 ms @ 60 fps; instance re-extract+upload ≤ 1.5 ms at the 20k visible cap (§15) |
| Memory | ≤ 1.5 GB total app; SimCore state ≤ 150 MB at promise scale (§5 estimate ~60 MB) |
| Save file | ≤ 30 MB at promise scale; autosave snapshot phase ≤ 100 ms; pause-save ≤ 300 ms |
| Battery | ≤ 12%/30 min at battery settings (measured M2+) |

**Simulation rate is 30 UPS everywhere, permanently.** No 60-UPS desktop variant: rates are stored per-tick in content tables and a rate fork would fork save/replay compatibility. `TICK_RATE = 30` appears in exactly one header. Validation that 30 is sufficient: at 1/256-tile positions, Factorio-parity belt speeds are integer-exact per tick (Mk1 1.875 tiles/s = 16 subtile-units/tick, Mk2 = 32, Mk3 = 48), and input→sim latency worst case (33 ms + one render frame) is imperceptible in a builder. Costs accepted: circuit timers tick at 30 Hz and inserter swing granularity is coarser than Factorio's 60 — noted, not blocking.

---

## 2. Architecture: three layers, one manager component

```
┌────────────────────────────────────────────────────────────────────┐
│ Shell            gestures (Zenith_Pointers) · UI screens · bindings │
│                  emits COMMANDS, reads presentation state            │
├────────────────────────────────────────────────────────────────────┤
│ Presentation     visible-set extraction → Flux_InstanceGroups ·     │
│                  camera · ghosts · alerts · interpolation buffers    │
│                  reads SimCore via PostTickExtract snapshots         │
├────────────────────────────────────────────────────────────────────┤
│ SimCore          deterministic integer factory sim · own containers  │
│  (L0-only)       ticked at 30 UPS · mutated ONLY via command queue   │
└────────────────────────────────────────────────────────────────────┘
```

### Why the ECS is not the sim

Measured mechanics of `ZenithECS` (all verified in source):

- Per-entity, per-frame lifecycle dispatch walks the **entire sorted component-meta list** calling `HasComponent` per registered type (`Zenith/ZenithECS/Internal/Zenith_ComponentMeta.cpp`, `DispatchHookForEntities` — the loop at ~400-412). With ~32 registered metas (16 engine + AIAgent + game), 100k entities would cost ≈ 6.4M hashmap probes + scene re-resolutions per hook phase per frame *before any game code runs*, twice per frame (Update + LateUpdate).
- The authoritative per-entity component index is a **per-entity `Zenith_HashMap`** (`Zenith/ZenithECS/Internal/Zenith_EntityStore.h`, `m_axEntityComponents`) — ≈144 B and 3 heap blocks per entity that owns any component; the dense-pool sparse-set (`Zenith_ComponentPool.h`) is a query fast path, not the authority.
- `Zenith_EntitySlot` is ≈100 B plus a `std::string` name plus an eagerly-allocated child vector (`Zenith_Vector`'s default constructor allocates 8 elements — `Zenith/Collections/Zenith_Vector.h`). 100k entities ≈ 30 MB and ~400k allocations of pure bookkeeping.
- `Zenith_SceneData::Update` snapshots every active entity ID per frame, and again per `FixedUpdate` substep (`Zenith/ZenithECS/Zenith_SceneData.*`). Queries are main-thread-asserted; component pointers/indices are unstable (pool growth relocates, removal swap-and-pops).
- The engine's own benchmark (`Zenith/Core/Zenith_BenchECS.h`, `--bench-ecs`) has never measured beyond 50k entities.

None of this is a defect for its design load (games with hundreds-to-thousands of entities); it is simply the wrong substrate for 100k sim entities. **Foundry registers ~3 ECS components total** (manager, camera, touch-layout — orders 100/101/102 in the game's own order space) and the world's factory entities never exist as ECS entities.

### The manager component (the one legal per-frame hook)

There is no `Project_Update` — the project contract is boot/shutdown only (`Zenith/Core/Zenith_ProjectHooks.h`). A game's per-frame entry is an ECS component's `OnUpdate(float)`; the proven shape is CityBuilder's single orchestrator (`Games/CityBuilder/Components/CB_CityManagerComponent.h`).

**Foundry copies the pattern but not the layout.** `CB_CityManagerComponent` owns its subsystems *by value* and documents the consequence: ECS pools relocate on growth/swap-and-pop, so every captured member address must be re-wired by a ~35-line hand-written move constructor — a standing bug factory. `FD_GameComponent` instead owns exactly two pointers:

```cpp
class FD_GameComponent   // ECS order 100; the only sim-facing component
{
public:
    void OnUpdate(float fDt);   // accumulator + tick loop + PostTickExtract (§3)
    void OnStart();             // new FD_Sim / FD_Presentation (fresh or from save)
    void OnDestroy();           // delete both
private:
    FD_Sim*          m_pxSim         = nullptr;  // SimCore root (heap; never relocates)
    FD_Presentation* m_pxPresentation = nullptr;
};
```

Moves are trivial pointer transfers; the automated-test harness's scene-reload/`OnStart` refire becomes delete+new instead of a reset checklist. This is direct ownership, not pimpl (no forwarding shim; callers reach `m_pxSim->Belts()` etc. directly) — compliant with the repo's no-pimpl mandate.

### The SimCore boundary: ZenithBase-L0-only, lint-enforced

SimCore is **not** "engine-free" — that boundary would ban `Zenith_Vector`/`Zenith_HashMap`/`Zenith_DataStream` (all route through `Zenith_MemoryManagement`, which is L0) and put the sim at war with the repo's no-`std::`-containers convention. The correct precedent is the **ZenithECS leaf**: depends on ZenithBase (L0) only, zero `g_xEngine` reaches, enforced by ratchet (`Tools/analyze_code_complexity.py`).

SimCore's allowlist: L0 collections, `Zenith_DataStream`, `Zenith_Noise` integer entry points, C++ standard headers (non-float). Banned: `g_xEngine`, Flux, ECS, Input, UI, `Zenith_Maths` float types, `float`/`double` in any sim-state-affecting expression, wall-clock, `std::` containers. Enforced by a CI grep-lint over `Source/SimCore/` (§18) — SimCore stays a **source subdirectory of the game project**, not a second static lib (the `.zproj` machinery owns the build shape of `Games/<Name>/`; a second project there is not a supported descriptor form).

### Relationship to engine systems Foundry does NOT use

Stated explicitly so their absence is legible as a decision:

- **Jolt physics — unused.** The factory is grid logic; collision is tile occupancy. Biters move by sim-side steering, not rigid bodies (Jolt's 65,536-body cap — `Zenith/Physics/Zenith_Physics.h` — would also be exceeded by design). The repo's "no teleportation — use `Zenith_Physics`, not `SetPosition`" mandate governs physics-simulated character movement; Foundry has no physics bodies to teleport. Physics stays initialized but empty.
- **Engine AI / Navigation / Squad — unused.** Navmesh pathfinding per-unit at horde scale is the wrong tool; biter movement is SimCore's hierarchical chunk-graph pathfinding (§13).
- **Behaviour Graphs — unused** for sim logic (determinism + scale); available to Shell-side ambience if ever convenient.
- **Engine Terrain — unused.** It is a 4 km heightmapped streaming system with 320 MB of dedicated buffers (`Zenith/Flux/Terrain/Flux_TerrainConfig.h`); Foundry's flat ground is a few tiled meshes (§15).

---

## 3. Fixed tick, accumulator, and the command queue

### The engine gives you wall-clock float dt and nothing else

`Zenith_Core::UpdateTimers` (`Zenith/Core/Zenith_Core.cpp`) produces an **unclamped wall-clock float** dt into `g_xEngine.Frame()`; the engine's two fixed-step accumulators are unusable for a game tick (the ECS one is hard-coded private 50 Hz with a 16-substep burst policy — `Zenith/ZenithECS/Zenith_SceneSystem.h`; the physics one is 1/60 with an 8-substep cap that *discards* remainder — `Zenith/Physics/Zenith_Physics.cpp`). Foundry rolls its own accumulator inside `FD_GameComponent::OnUpdate`, which also owns policies the engine loop shouldn't: sim speed, pause, and catch-up.

```cpp
void FD_GameComponent::OnUpdate(float fDt)
{
    m_fAccumulator += ClampDt(fDt);                    // hitch guard
    u_int uTicks = 0;
    while (m_fAccumulator >= fTICK_PERIOD && uTicks < uMAX_TICKS_PER_FRAME)  // 3
    {
        m_pxSim->Tick();                               // drains commands first
        m_fAccumulator -= fTICK_PERIOD; ++uTicks;
    }
    if (m_fAccumulator >= fTICK_PERIOD)                // still in debt: discard
    {
        m_fAccumulator = 0.0f;                         // logged; sim runs slow, never spirals
    }
    m_pxSim->PostTickExtract(*m_pxPresentation, Alpha());  // §14
}
```

- **Debt clamp:** at most 3 ticks per frame; remaining debt is discarded with a log line. A hitch produces momentary slow-motion, never a death spiral.
- **Android resume discards ALL debt** unconditionally — a phone returning from 10 minutes backgrounded must not "catch up" (and per the GDD, backgrounded = paused anyway).
- The accumulator is presentation-side state (float is fine here; it never touches sim state). Sim speed ×0/×1 is a Shell concept — pause simply stops calling `Tick()`.

### The command queue is the ONLY write path into SimCore

Frame ordering hazard, verified: `UI().Update` runs **after** `Scenes().Update` in the frame (`Zenith/Core/Zenith_Core.cpp`, `SubmitRenderWork` vs `UpdateGameLogic`), so UI button callbacks fire *outside* the tick. All mutations — UI callbacks, gestures, objective scripts, debug tools — are therefore expressed as **commands** (`PlaceBuilding{archetype, tile, rot}`, `SetRecipe{handle, recipe}`, `StampBlueprint{...}`, `MarkDeconstruct{rect, filter}` …) appended to a queue that `FD_Sim::Tick()` drains **first**, in order.

This single rule buys four things at once:
1. **Determinism** — sim state changes only at tick boundaries, in recorded order.
2. **Replay** — a log of `(tick, command)` pairs + the world seed reproduces a session bit-for-bit; this is the digest-test harness (§18) and the debugging story.
3. **Undo** — every command carries its inverse; the undo stack is 50 command groups (GDD §5.12).
4. **The multiplayer door** — lockstep MP is "share the command stream", deferred but not foreclosed.

Automated tests pin `Zenith_InputSimulator::SetFixedDt(1.f/30.f)` so one frame = exactly one tick and frame counts equal tick counts (the harness's fixed-dt seam exists precisely for this — its default-pinning history is recorded on commit `e2f5796c`).

### Tick phase order (fixed, documented, versioned)

```
Tick(): 1 drain commands → 2 timer wheel (craft/fuel/swing completions)
      → 3 electric networks (aggregate → satisfaction) → 4 belts (sharded)
      → 5 splitters/sideloads → 6 inserter transitions → 7 fluids
      → 8 trains → 9 drones → 10 circuits (evaluate; publish to back buffer)
      → 11 threat (pollution CA every 8 ticks · spawners · groups · combat)
      → 12 swap circuit buffers · advance RNG streams · ++m_uTick
```

Circuits read the **previous** tick's network values (double-buffered, §12), giving the fixed 1-tick propagation delay and making their phase position non-load-bearing.

---

## 4. Determinism

### Why integers (the engine makes floats a trap)

- All 12 win64 configurations compile `/fp:fast`, and the tree documents measured `/Od`-vs-`/O2` divergence for trivial expressions, with an MSVC-only `float_control` escape macro pair scoped to authoring (`Zenith/Core/Zenith.h`, the block at ~81-114).
- Android (AGDE/clang) carries no FloatingPointModel at all — different FP semantics from MSVC by default.
- Wall-clock float dt reaches all engine update paths (§3), and the repo has already shipped a dt-poisoning determinism bug (`e2f5796c`).

A float sim can therefore never be bit-identical across Debug/Release, let alone Windows/Android. **SimCore holds no floats.** Integer arithmetic is exact on every compiler at every optimization level — cross-platform determinism becomes free, and the digest tests in §18 enforce it forever.

### The rules

1. **Representations:** tile coords `int32`; sub-tile positions in **1/256-tile units**; rates/speeds **Q16.16** per-tick fixed point, converted from designer per-second values at content-table compile time (§17); accumulations in 64-bit intermediates before narrowing.
2. **RNG:** `Zenith_TerrainNoise::XorShift32` (`Zenith/Maths/Zenith_Noise.h` — the engine's one deterministic, stream-portable PRNG), one named stream per system (worldgen, threat, drones…), all seeded from the world seed; streams advance only inside the tick.
3. **World noise:** the integer avalanche hashes (`HashUInt`/`HashCoords`) are bit-exact and allowed; the float `ValueNoise`/`FBM` in the same header are **banned** (explicitly not bit-exact across targets). SimCore reimplements the 2–3 octaves of value noise worldgen needs in integer fixed point (~10 lines over `HashCoords`), digest-pinned.
4. **Iteration order:** sim-visible iteration only over dense arrays / slot tables in index order. `Zenith_HashMap` iteration is slot-order and capacity-history-dependent (`Zenith/Collections/CLAUDE.md`) — hashmaps are for point lookup only (the chunk map); deterministic scans use the separately-maintained sorted chunk list (§5).
5. **Serialization:** field-by-field, explicit little-endian, never struct memcpy — padding must not reach bytes or digests (the ZM codec discipline, §16).
6. **No UB in arithmetic:** wrapping math on unsigned or explicit 64-bit; C++20-defined signed division/shift semantics only.
7. **Enforcement:** the SimCore lint (bans `float`, `double`, `Zenith_Maths::`, `g_xEngine` tokens in `Source/SimCore/`) + digest gates: **`/Od`-vs-`/O2` digest equality per commit** (the two configs the tree documents as diverging — the cheapest canary), **Windows-vs-Android digest per device-test session** (M2+). Test builds emit **per-module sub-digests every 64 ticks** so a mismatch bisects to (tick, module) instead of "somewhere in 10,000 ticks".

---

## 5. World data model

- **Chunks are 32×32 tiles.** `Zenith_HashMap<u_int64 /*packed s32,s32*/, FD_Chunk*>` for point lookup + a **sorted-by-key chunk list** (rebuilt on chunk create, rare) for all deterministic whole-world scans (pollution, spawner logic, save).
- `FD_Chunk` layers, allocated lazily:
  - **Terrain**: 1 KB of tile bytes (ground type, resource id) — always present, regenerable from seed; only *deltas* (mined-out amounts, cleared trees) are saved.
  - **Occupancy**: `u_int32` entity handle per tile (4 KB), allocated on first construction in the chunk. At a realistic ~4k built chunks ≈ 16 MB. Multi-tile buildings write their handle into every covered tile; footprints come from the archetype table.
  - **Threat**: pollution accumulator (u32), nest/worm list, per-chunk pathing cost byte.
- **Resource patches** are generated from the integer hashes at chunk-first-touch; a patch is also registered in a global patch table (drill placement queries, map view).
- **Entity handles** are `u_int32`: `archetype(6) | generation(6) | slot(20)` — 64 archetypes, 1M slots per archetype, generation-checked deref (§7). Handle 0 is null.
- Memory at promise scale (100k buildings, 250k items, ~4k built chunks): occupancy 16 MB + machines ~10 MB + belts ~1 MB + terrain deltas + threat ≈ **~60 MB SimCore total** — comfortably inside budget.

---

## 6. Belts (the load-bearing deep dive)

Belts are the item-count giant (250k+ items) and the system whose data structure decides whether 30 UPS is cheap or impossible. The design is the transport-line compression model (the same asymptotic idea Factorio's transport lines use):

### Data structure

- Contiguous straight-through belt tiles merge into **transport lines** (per lane: two lines per belt run), capped at **128 tiles** per line (caps splice cost and gap range; longer runs chain).
- A line stores: `u_int32 m_uHeadGap` (1/256-tile distance from line front to the first item), a **growable POD ring** of `{u_int8 uItem, u_int8 uFlags, u_int16 uGapToPrev}` — **4 bytes per item**, so 250k items ≈ 1 MB — plus `{speed, length, state, wake links}`.
- `u_int16` gap in 1/256-tile covers 256 tiles — double the line cap; `u_int8` item id covers the ~90-item roster with headroom (a second item-page flag bit exists in `uFlags` if the roster ever exceeds 255).

### Tick semantics — O(moving lines), not O(items)

- An **unblocked line** advances by decrementing `m_uHeadGap` by its per-tick speed (16/32/48). **One integer op**; body items are gap-relative and never touched.
- When the head reaches the line end it transfers (next line / splitter / machine / drops to blocked). A **blocked line sleeps entirely** and is woken by its consumer (the downstream line/splitter/inserter that frees space) — the compression trick that makes megabases affordable. A naive per-item advance at 250k items/tick would blow the entire sim budget on belts alone.
- **Splitters/sideloads** are nodes in the line graph with explicit, serialized round-robin state (deterministic fairness). **Circular runs** are detected at merge time and assigned a deterministic anchor tile (no head).
- **Inserter access** at an arbitrary offset: binary search over prefix gaps, then O(line) memmove splice — bounded by the 128-tile cap (worst case ~500 B). Inserters cache `(line id, offset)` targets, invalidated by belt edits via the line graph.

### The container gap

The engine has **no deque, growable ring, slot map, or usable pool** (`Zenith/Collections/` holds exactly `Zenith_Vector`, `Zenith_HashMap`/`Set`, a compile-time-capacity `Zenith_MemoryPool` (inline arrays + mutex per op), and a compile-time-capacity `Zenith_CircularQueue` that destroys/reconstructs per pop). SimCore therefore ships its own **power-of-two growable ring** (~150 lines, POD-only, no default-alloc — `Zenith_Vector`'s default constructor mallocs 8 elements, a policy SimCore's SoA tables must not inherit) plus a slot-map and a bitset. These live in `Source/SimCore/Containers/`, unit-tested like any engine collection; promotion into `Zenith/Collections/` is offered opportunistically once a second consumer exists (per the register's game-side list).

---

## 7. Machines

- **SoA archetype tables** (drills, furnaces, assemblers, labs, inserters, turrets, ports…): parallel arrays per field, slot-map with per-archetype freelists, generation-checked handles (§5). No virtual dispatch; per-archetype tick functions.
- **Timer wheel:** any machine whose next state change is at a known tick (craft completion, fuel exhaustion, inserter swing end, drone charge) sits in a bucketed wheel keyed by absolute tick — **zero cost until due**. At 50k active assemblers averaging 0.5 crafts/s ≈ 800 completions/tick: trivial.
- **Inserters are event-scheduled, never integrated:** a swing is `{eState, uStartTick, uDuration}`; the sim touches an inserter only at transitions (pick, swing-end, place, return-end). Presentation interpolates arm poses for *visible* inserters only (§15). This is the single largest CPU win in the machine layer — 30k inserters polled per tick would dominate the frame; event-scheduled they cost ~1k transitions/tick.
- **Wake/sleep:** machines sleep when input-starved / output-blocked / unpowered-below-threshold; producers wake consumers through explicit wake links (belt→machine, machine→inserter…). **No wake storms:** power satisfaction changes never traverse sleeper lists — machines read the per-network satisfaction scalar when they next act; only full network on/off transitions wake.
- Crafting math in integers: progress accumulates `speed × satisfaction` (Q16.16) per active tick against a per-recipe target; ingredients debit on craft start, products credit on completion (transactional against output-full).

## 8. Electric networks

- **Union-find over pole connectivity** on placement (poles union by wire reach; machines map to the covering pole's root). **Removal** marks the network dirty; end-of-tick **BFS rebuild over the pole graph only** (even a megabase network is 5–10k poles — sub-ms, and deferring to tick end makes mid-tick edits safe).
- Per network per tick: sum production capacity and demand (both maintained incrementally as machines change state, not rescanned) → **satisfaction = min(1, supply/demand)** in Q16.16 → consumers scale by it (GDD's brownout model). Accumulator charge/discharge resolves in the same pass with fixed priority (solar → accumulator → steam).
- **Stats rings designed in now** (retrofit is painful): per network, 64-sample rings at three cadences (tick/second/minute) of production/consumption per category — the data behind the power graph screen.

## 9. Fluid networks

- Pipes/undergrounds union into **fluid networks**; a network holds `(fluid type, volume, capacity)` and equalizes instantly — the per-network volume model, which is essentially what Factorio 2.0 shipped after abandoning per-segment flow. One fluid type per network, enforced at connect time.
- Endpoints (pumps, boilers, refineries, tanks, wagons loading) are **rate-limited connections** (per-tick Q16.16 throughput caps) — machine I/O is bounded even though the network interior is instant.
- **Extent cap ~320 tiles** per network (mirrors Factorio 2.0; keeps "instant" plausible and networks small). **Deterministic split on deconstruction:** volume apportions to sub-networks proportional to capacity, integer division, remainder to the lowest network id.

## 10. Trains

- **Discrete rail-piece catalog** (straight, 45°, fixed-radius curve) so the rail graph is exact; visual curves are presentation-only splines. Graph: nodes at signals/stations/junctions, edges = track blocks with integer lengths in 1/256-tile.
- **Block signaling:** a block is owned by at most one train; **chain signals** perform lookahead reservation through consecutive blocks. Chain-signal semantics + deadlock behavior are a **separately staged deliverable** inside M3b (plain blocks + manual schedules land first) — this is the system with the notorious hidden 20% tail.
- **Kinematics:** per train, `(path, distance-along-path, speed)` in Q16.16 with 64-bit intermediates; acceleration/braking curves from content tables; braking-distance reservation lookahead.
- **Pathfinding:** A* over the rail graph on demand (dispatch, replan on block closure), amortized (1–2 replans/tick cap; trains hold reservations while waiting).
- Wagons are inventories (bulk item movement); stations are icon-named (no text — GDD §5.8).

## 11. Logistics drones

- Ports union into **logistics networks** (coverage overlap). Per tick, a **matcher budget** (K requests round-robin per network) pairs requesters with providers — never a full O(requests × providers) scan.
- A flight is `(src, dst, cargo, departure tick, duration)` — straight line, no collision, no per-tick integration (position is derived for rendering only, §15). Charging is simplified and event-scheduled: land at nearest port with a free slot, occupy it for `charge_ticks`, resume — preserves the "roboport density matters" constraint without queue simulation.

## 12. Circuit network

- Red/green wires union into **circuit networks** (union-find again; the same rebuild-on-remove discipline as §8).
- A network's value set is a **sorted vector of `(u_int16 signal, int32 count)`**; readers sum contributions. **Double-buffered:** all evaluation this tick reads the previous tick's published values; writes go to the back buffer; buffers swap at phase 12 — the fixed 1-tick delay that makes evaluation order-independent and deterministic.
- Combinators evaluate in creation-order index (stable, serialized). Cost is noise at any plausible count (thousands of combinators ≈ tens of µs).

## 13. Threat: pollution, biters, defense

- **Pollution** is a per-chunk `u_int32` cellular automaton: every **8 ticks**, produce (from machine activity, batched per chunk), diffuse to 4-neighbors (integer fractions, deterministic order from the sorted chunk list), absorb (trees, nests). Cost at ~10k live chunks: a few hundred µs, 1/8 amortized.
- **Nests** absorb pollution into an attack budget; on threshold, spawn an **attack group** (20–50 units) targeting the polluting region. **Evolution** = f(elapsed ticks, total pollution produced, nests destroyed) shifts spawn tables.
- **Pathfinding is per-GROUP, never per-unit:** hierarchical A* over the chunk-level cost graph (chunk cost = base + player-structure density from the occupancy layer), refined to a tile corridor; **1–2 group repaths per tick, hard cap**, staggered. Units follow the group corridor with **local steering**: spatial-hash separation (incrementally re-binned, 8-neighbor cell query) + integer steering toward the corridor. Melee/spitter attacks resolve against the occupancy layer and machine HP fields.
- **Turret target acquisition is event-driven:** the unit spatial hash publishes cell enter/exit events; turrets subscribe to their range cells — no per-tick range scans over 2k turrets.
- **Budgets:** active units hard-capped at **5,000** (wave scheduling holds excess; off-screen groups far from any player structure advance along their corridor in bulk — teleport-along-path — instead of steering). Estimated tick cost at the cap on the floor device: separation+steering ≈ 0.8–1.3 ms, combat+turrets ≈ 0.5 ms, path amortization small ⇒ ~2.5–3.5 ms worst case (§1 budget).
- **The path-provider seam:** group movement queries go through a narrow interface (`GetCorridorDirection(group, tile)`), so if M5 profiling shows separation quality needs more iterations than budgeted, **per-corridor cached flow fields** (one 32×32 direction byte-field per corridor chunk, recomputed on wall edits) slot in behind the same interface without touching spawn/combat code. Planned fallback, not v1 machinery.

---

## 14. Threading

**v1 model: main-thread orchestrated data-parallel fan-outs.** The engine task system is a flat pool — no dependencies/continuations, 128-slot mutexed queue, `min(hw−1, 16)` workers, with `Zenith_DataParallelTask` as a fetch-add-claimed parallel-for (`Zenith/TaskSystem/Zenith_TaskSystem.h`). That is sufficient for coarse shards and insufficient for a general job graph — so the tick stays on the main thread and fans out only where the data is embarrassingly parallel:

- **Belts:** lines pre-partitioned by connected line-graph component into shards; **2–3× more shards than workers** so fetch-add claiming self-balances around big.LITTLE little-core stragglers (no engine affinity support exists — register #12 only if profiling demands).
- **Threat steering:** unit shards by spatial-hash region.
- **Determinism rule for every fan-out:** shards are pre-partitioned (never dynamically split), workers write only shard-local state + a per-shard command/effect queue, queues merge on the main thread **in fixed shard index order**, no shared atomics in game logic. The nondeterministic claim order of `Zenith_DataParallelTask` is thereby invisible.
- Cross-system phases stay serial (they're cheap — §1 slices).

**Rejected for v1: a dedicated sim thread.** It would require hand-rolling a full double-buffer of render-visible state plus lifetime coordination with scene reloads and the editor, for headroom a 33 ms tick period doesn't need (budget says ≤ 8 ms). The escape hatch is built anyway: **PostTickExtract** (§15) is an explicit end-of-tick phase producing presentation-owned snapshot buffers — simultaneously the interpolation source, the renderer's only input, and the exact seam at which the tick could move to a worker later as a contained change.

ECS queries are main-thread-asserted (`Zenith/ZenithECS/Zenith_Query.h`) — irrelevant to SimCore (which never touches ECS) and a reminder that Presentation/Shell code stays on the main thread.

---

## 15. Rendering

### What exists (verified)

- `Flux_InstanceGroup` (`Zenith/Flux/InstancedMeshes/Flux_InstanceGroup.h`): up to **131,072 instances per group**, per-instance transform + 16-byte anim/color record (RGBA8 tint — the state-tint channel the GDD uses), swap-and-pop unstable IDs, grow-only capacity.
- The **UnifiedMesh GPU-driven pipeline** (`Zenith/Flux/UnifiedMesh/`): compute reset → frustum-cull → one `DrawIndexedIndirect` per mesh+material bucket, shared cull output feeding the shadow cascades, mesh de-dup via `Flux_MeshGeometryRegistry`, running on Android including `drawIndirectCount` fallback tiers. HiZ occlusion culling is **not** wired to it (frustum only — `Zenith/Flux/HiZ/CLAUDE.md` lists it as future).
- The costs: the per-frame path **re-extracts every enabled instance of every group** (`Flux_GPUSceneBuilder.cpp`, `SyncUnifiedBucketsFromSnapshot`/`ExtractInstanceGroupBuckets` — ~176 B of CPU writes per instance per frame) and `Flux_InstanceGroup::UpdateGPUBuffers` **uploads full capacity every frame regardless of dirtiness** (frame-indexed buffers make per-instance dirty-skip unsafe as built). The largest instance count ever exercised in-tree is **2,520** (terrain-editor trees).
- Nothing else can draw item-scale sprites at count: screen Quads cap at 1024/frame (`Zenith/Flux/Quads/Flux_QuadsImpl.h`), GPU particles at 4096 total with emitter semantics (`Zenith/Flux/Particles/Flux_ParticleGPUImpl.h`), Primitives are one draw call per shape (`Zenith/Flux/Primitives/CLAUDE.md`).
- Camera: orthographic projection exists in the type system but is unreachable from game code and its matrix branch lacks the Vulkan Y-flip the perspective branch applies (`Zenith/EntityComponent/Components/Zenith_CameraComponent.cpp`) — register #13, explicitly **not needed**: Foundry ships the steep-pitch *perspective* near-top-down camera CityBuilder proves (`Games/CityBuilder/Source/CB_CameraController.h` — pure-math orbit/zoom controller, reused with pinch/pan sources).

### The design: factory size never reaches the renderer

**Visible-set extraction.** Only entities inside the camera rect (+1-chunk margin, + a shadow-length pad toward the sun) have render instances *at all*. Presentation materializes/dematerializes per chunk on visibility enter/exit, drawing from pooled per-mesh instance budgets. Hard caps, enforced by representation switching rather than clipping:

| Zoom tier (GDD §6.2) | Representation | Instance budget |
|---|---|---|
| Workshop | full machine meshes + belt items + inserter arms + drones | ≤ ~8k buildings + ~10k items |
| Logistics | simplified building meshes; belts as tinted ribbon segments (no per-item instances); trains/drones as markers | ≤ ~15k |
| Map | one unit-quad InstanceGroup, one instance per chunk, per-instance tint = chunk summary color; overlays same trick | ≤ ~10k quads |

- **Caps are load-bearing because capacity is grow-only:** a single frame at 60k materialized instances would ratchet that group's full-capacity upload forever. Tier switches happen *before* budgets inflate.
- **Slot ledger:** `RemoveInstance` swap-and-pops without reporting which ID moved; Presentation keeps a per-group `slot → owner` array and patches the tail owner on each removal (it knows the pre-remove count). **Never** the clear-and-respawn-per-frame pattern (CityBuilder does that at 256 instances and says so; at 20k it would be the extraction cost again).
- **Interpolation:** PostTickExtract publishes positions/phases for visible movers (belt heads near inserters, arms, drones, trains, biters) at tick rate; render frames interpolate between the last two extracts (alpha from the accumulator). Belt item positions are derived per visible line from `head_gap` + prefix gaps only for lines intersecting the view.
- **Cost check** at the 20k worst-case visible set with today's full-rebuild engine path: ~3.5 MB CPU writes + ~5 MB upload per frame ≈ 0.5–1.5 ms on the floor device — acceptable at 30 fps, and the reason register **#6 (dirty-range/static-bucket upload)** is scheduled for M3: at 60 fps that flat cost is 6–9% of the frame doing nothing. The design works without the engine fix; it gets cheap with it.
- **Ghosts/previews:** tinted opaque instances (pulsing tint) — InstanceGroups draw opaque into the G-buffer; translucent instancing does not exist. The handful of active drag-ghosts may also use the Primitives *gameplay* channel (unlit, drained unconditionally — `Zenith/Flux/Primitives/CLAUDE.md`).
- **Ground:** a small set of large tiled quad meshes with a detail texture (engine Terrain ruled out — §2); build-mode grid as a shader overlay on the ground material, not primitive lines (a 100×100 `AddGrid` would be 200 draw calls).
- **Shadows:** sun-only, cascade count/resolution tuned down on mobile; the unified cull already feeds cascades.
- **World-space text does not exist** (`Flux_Text` is screen-space pixels — `Zenith/Flux/Text/Flux_TextImpl.h`); floating labels (station icons, alert markers) render as screen-space UI projected from world positions each frame — game-side, no engine change (register's game-side list).

---

## 16. Save/load & Android lifecycle

### Format: the Zenithmon module codec, adopted verbatim

`Games/Zenithmon/Source/Core/ZM_SaveSchema.{h,cpp}` + `Games/Zenithmon/Docs/SaveFormat.md` is the proven pattern and Foundry adopts its shape wholesale: engine `Zenith_SaveData` owns the file envelope (magic/CRC/versions/timestamp); the game payload is `FDSV` = header + **length-framed modules** `{moduleId, moduleVersion, byteLength, payload}` — one module per sim system (world-deltas, belts, machines, power, fluids, rail, drones, circuits, threat, research, blueprints/undo, presentation-prefs). Length framing lets a migration absorb a dropped trailing field without touching sibling modules; readers must land exactly on `byteLength`; every read bounds-checks before allocating; writes stage + validate into a private stream before `Zenith_SaveData::Save` is ever called; reads parse into a temporary and publish only on complete success. **Every schema change ships its version bump + a compiled-blob migration test in the same commit** (ZM's binding rule).

- **Chunk deltas over procedural regen:** terrain and patches regenerate from the seed; only deltas persist. Save size at promise scale ≈ tens of MB (dominated by machine tables + belts ≈ the §5 memory numbers) — inside the 30 MB target with even trivial compression.
- Saves land in the engine's per-game sandbox (`%APPDATA%/Zenith/Foundry/` on Windows, `internalDataPath` on Android — `Zenith/SaveData/`, `Zenith/FileAccess/`).

### Two save paths with opposite requirements

1. **Lifecycle pause-save — synchronous by design.** Android may kill the process any time after backgrounding; `APP_CMD_PAUSE` is currently **log-only** in the platform layer (`Zenith/Android/Zenith_Android_Main.cpp`, `OnAppCmd`) and `APP_CMD_SAVE_STATE` is unhandled — **register #3** adds a project lifecycle callback. Foundry's handler runs a full synchronous save (≤ 300 ms budget); a blocking hitch while backgrounding is invisible and correctness-critical.
2. **Periodic autosave — snapshot-then-async.** Every 2 minutes: serialize SimCore into a pooled memory buffer on the main thread between ticks (**≤ 100 ms, a tracked metric from M1**), then compress + write on a task thread. `Zenith_SaveData::Save` is fully synchronous today (fills a stream, copies it again behind a CRC, blocking `WriteToFile` — `Zenith/SaveData/Zenith_SaveData.cpp`) — **register #7** adds the async variant (or a sanctioned game-side write path to the slot directory).

---

## 17. Content pipeline

- **Compiled `constexpr` tables** (the CityBuilder/Zenithmon precedent): items, recipes, archetypes (footprint, HP, power, pollution), techs, biter spawn tables — plain structs in `Source/Content/`, unit-tested for referential integrity (every recipe ingredient exists, every tech unlock resolves, every archetype mesh id resolves).
- **Designer units are per-second/per-tile; stored units are per-tick Q16.16** — conversion happens in `consteval` helpers at table definition, so `TICK_RATE` appears once and content reads naturally.
- No data files, no modding in v1 (GDD §12).
- **Art path:** meshes/textures/materials through the standard tools pipeline (`Zenith/AssetHandling/`, FluxCompiler for shaders). Constraint to carry: texture export is **BC-only today** (`Tools/Zenith_Tools_TextureExport.h`) and BC support is not device-checked — uncompressed RGBA8 is the M0–M2 stopgap (ZM's approach), **ASTC/ETC2 (register #5) before real art mass lands**.
- Icon atlas for ~90 items feeds both UI and the item-nugget meshes; colorblind-safe palette enforced at bake by a checker tool (game-side).

---

## 18. Testing & CI

SimCore's engine-freedom makes Foundry the most testable game in the repo: **the entire factory sim constructs and ticks inside a plain unit test** — no scene, no GPU, no harness.

| Tier | What | Mechanism |
|---|---|---|
| SimCore units | belts/machines/power/fluids/rail/drones/circuits/threat logic, containers, content-table integrity | `ZENITH_TEST` boot units; SimCore instantiated directly (repo mandate: every new type gets dedicated coverage) |
| **Golden digests** | canned command scripts build factories → run N ticks → FNV-1a over serialized state == pinned digest | boot units (fast, in-process); the determinism gate |
| **Op-count pins** | the same runs assert exact deterministic counters: belt head-advances == N, machines woken == M, path expansions == K | catches sleeping-machine regressions and accidental O(placed) loops as **flake-free CI failures** — deliberately not wall-clock (this repo has already shipped a wall-clock CI flake, commit `9bdc28be`) |
| Config-divergence gate | the digest suite runs in `/Od` and `/O2` Windows configs and must match | per-commit canary for float leaks into SimCore |
| Android digest | same digests on-device | per device-test session from M2 (no Android CI lane exists) |
| Automated tests | `FD_*_Test` via the harness: boot, place-and-verify through real input, touch gestures (pointer-event injection on the Null backend, per ZM's `ZM_AutoTests_TouchControls.cpp` precedent), save/load round-trips | `--fixed-dt` pinned to 1/30 |
| Perf report | synthetic megafactory generators (parameterized: belt city / train web / siege) dump per-system tick timings | report-only artifact in CI; gating stays op-count |
| SimCore lint | the L0-only / no-float boundary (§2, §4) | grep gate in the game workflow |

Process obligations (all verified conventions):
- **Unit-baseline row:** `Tools/unit_baselines.json` gains a `"Foundry": N` row where N is read from an **observed** `Null_vs2022_Debug_Win64_True` run (never computed, never from a Vulkan exe), plus either a workflow gate (`Tools/run_unit_gate.ps1 -Game Foundry`, generous `-TimeoutSec`) or a declared advisory entry — the manifest test enforces one-or-the-other.
- **CI workflow** `foundry-tests.yml` mirrors `zm-tests.yml`: regen → Vulkan build (compile proof, never executed on GPU-less runners) → Null build (everything runs on this) → D3D12 build (link-neutrality proof) → boot smoke → unit gate → `zenith test Foundry --headless`.
- **MSVC dead-strip:** every game component header is `#include`d from `Foundry.cpp` or its registrar never runs (`ZENITH_REGISTER_COMPONENT` hazard, documented in `Zenith/EntityComponent/CLAUDE.md`).
- **Scene rule:** headless runs may CREATE `.zscen`, never CHANGE (enforced in `Zenith_Editor::SaveActiveScene` — `Zenith/Editor/Zenith_Editor.cpp`). Foundry's single boot scene (camera + manager entity) is authored once, windowed; everything else is procedural, so the game is nearly immune to the scene-churn class of defect.
- `doc-lint.yml`'s game array gains `Foundry` when these docs move into `Games/Foundry/Docs/`.

---

## 19. Engine-change register

The explicit deliverable: what must change in Zenith for Foundry, ranked by blocking severity. Effort classes S (≤ ~2 days), M (~1–2 weeks), L (multi-week). "Needed by" names the milestone (§20) where absence starts blocking.

| # | Change | Effort | Needed by |
|---|---|---|---|
| 1 | **Audio system** | L | start M3 · land M6 |
| 2 | **UI DPI scaling revival** | S–M | M2 |
| 3 | **Android lifecycle save hook** | S | M2 |
| 4 | **Frame pacing + thermal/battery hooks** | M | M2 basics · M5 ladder |
| 5 | **ASTC/ETC2 texture pipeline + BC capability check** | M | M3 |
| 6 | **Instancing dirty-range upload / static buckets / capacity trim** | M | M3 (60 fps) else M5 |
| 7 | **Async save path** | S–M | M3 |
| 8 | **ScrollView defect fixes** | S | M2–M3 |
| 9 | **Dynamic 2D texture region-update API** | S–M | M3–M4 |
| 10 | **Safe-area insets** | S | M2 |
| 11 | **Haptics (Android vibrator)** | S | M6 |
| 12 | **big.LITTLE worker affinity** | M | only if M5 profiling demands |
| 13 | **Ortho camera fix (setter + Y-flip)** | S | not needed — file the bug |
| 14 | **IME / text input** | M | post-1.0 (icon naming dodges it) |
| 15 | **Android keycode table** | S | post-1.0 |
| 16 | **App icon / manifest / signing polish** | S | ship |

Details and acceptance criteria:

1. **Audio system — the only from-scratch subsystem.** The engine has no audio: no backend on any platform, no decoder, no mixer, no middleware in `Middleware/`, no `-lOpenSLES`/`-laaudio` in the Android link line. The one asset is `Zenith/Core/Zenith_AudioBus.{h,cpp}` — a test-only recording bus whose shipping body compiles to nothing, self-described as the seam "until the post-MVP audio playback layer lands". **Proposal:** keep `EmitSound(name, position, loudness, radius)` as the game-facing API (Foundry emits through it from M1, so content never blocks); build the backend behind it — decode (WAV + ogg/vorbis), mixer with buses (SFX/ambient/music/UI) and ducking, distance attenuation from the listener (camera), platform output via AAudio (Android) / WASAPI (Windows); voice cap + priority eviction for the machine-hum density mix. Acceptance: DP's existing `EmitSound` tests still pass; a Foundry soak scene with 200 emitters mixes within budget on the floor device.
2. **UI DPI scaling.** `Zenith_UICanvas::UpdateSize` computes `m_fScaleFactor` from the reference resolution and **nothing reads it** (`Zenith/UI/Zenith_UICanvas.cpp`) — every element lays out in raw authored pixels, so desktop-authored panels are physically tiny on phone panels. **Proposal:** apply the scale factor in `RecalculateScreenBounds`, hit-testing, and font sizing (opt-in per canvas to avoid disturbing shipped games); align with the existing density-scaled touch-target floor (`ResolveTouchTargetRect`, 57 logical px). Acceptance: one authored layout is tap-correct on a 1080×2400 phone and a 1080p desktop window.
3. **Lifecycle save hook.** `APP_CMD_PAUSE` currently only resets input; `APP_CMD_SAVE_STATE` is unhandled (`Zenith/Android/Zenith_Android_Main.cpp`). **Proposal:** a `Project_OnLifecyclePause()` hook (declared beside the others in `Zenith_ProjectHooks.h`) invoked from the pause-class commands before the poll loop blocks. Acceptance: kill-after-background on device loses ≤ one command batch.
4. **Frame pacing / thermal.** Present mode is FIFO, hard-asserted (`Zenith/Vulkan/Zenith_Vulkan_Swapchain.cpp`, `ChooseSwapPresentMode`); no Swappy, no `ANativeWindow_setFrameRate`, no frame limiter, no thermal/battery reads anywhere. **Proposal (M2 basics):** target-framerate control (30/60) via paced present + `setFrameRate` hint; **(M5)** `getThermalHeadroom` polling surfaced to the game for its degradation ladder. Acceptance: stable 33.3/16.7 ms cadence on a 120 Hz panel; ladder engages in a thermal soak.
5. **ASTC/ETC2.** Texture export offers Uncompressed/BC1/BC3/BC5(+BC7 in enums) only; no mobile format exists in the tree, and device suitability never checks `textureCompressionBC` — a Mali device passes selection and fails at first BC image creation. **Proposal:** ASTC (LDR 4×4/6×6) export path in `Zenith_Tools_TextureExport` + format selection per platform at cook, + the capability check with a clear early failure. Acceptance: Foundry's atlas renders on a Mali device; BC-only content fails at boot with a named error, not a crash.
6. **Instancing incremental upload.** `Flux_InstanceGroup::UpdateGPUBuffers` uploads full capacity every frame by design (frame-indexed buffers defeat naive dirty flags), `SyncUnifiedBucketsFromSnapshot` re-extracts every instance every frame, and capacity never shrinks. **Proposal:** per-frame-in-flight dirty-range tracking (ranges accumulate until each frame-indexed buffer has consumed them), a static/dynamic bucket split so write-once building transforms upload only on change, `Trim()`/`SetCapacity` to release peaks, and `RemoveInstance` returning the moved-slot id (kills the game-side ledger's bookkeeping hazard). Acceptance: 20k instances with 500 moving costs an upload proportional to 500, not 20k; existing games byte-identical output.
7. **Async save.** `Zenith_SaveData::Save` is synchronous with ~2× payload peak memory (stream + header copy) and a blocking write. **Proposal:** `SaveAsync(slot, version, ownedBuffer)` — caller hands an owned snapshot buffer; CRC+header+write run on a task; completion callback on main; same envelope, same slot semantics; concurrent-save-to-same-slot serialized. Acceptance: a 30 MB autosave costs the main thread only the snapshot serialize.
8. **ScrollView.** Exists, unused by any game, untested, with three verified defects: child hit-testing ignores the scroll offset (input lands where rows were at scroll 0), a drag started on a child button can never become a scroll (the child claims first), and the textured-quad path (`SubmitQuadWithUV`) is not clipped to the viewport (`Zenith/UI/Zenith_UIScrollView.cpp`, `Zenith_UICanvas.cpp`). **Proposal:** apply scroll offset in the input walk, candidate-arbitration window before child claims commit (the standard scroll-view gesture dance), clip + UV-adjust textured quads; add the missing unit tests. Acceptance: a list of buttons scrolls by dragging anywhere and taps hit the visually-correct row.
9. **Dynamic texture region updates.** For the Map-tier minimap/pollution overlays: a CPU-writable texture with sub-region upload, sampleable by UI/world quads. Streaming machinery exists internally (terrain chunk uploads); this exposes a small clean API. Fallback if deferred: the per-chunk instanced-quad map view (§15) covers v1, so this is M3–M4 convenience.
10. **Safe-area insets.** No `WindowInsets`/cutout handling anywhere; UI lays out against the raw surface and a punch-hole camera overlaps the HUD. Expose the safe rect on the canvas; Foundry anchors HUD chrome inside it.
11. **Haptics.** No vibration path exists (no permission, no `Vibrator` use). Tiny API: `Haptic(eImpulse)` → Android `VibrationEffect`; no-op elsewhere. Placement ticks, error buzz, first-contact alert.
12. **big.LITTLE affinity.** No thread affinity/priority code exists; Zenith and Jolt each spawn `hw−1` workers (≈15 threads on 8 cores). Foundry mitigates game-side first (shard oversubscription §14); this register item activates only if M5 profiling shows little-core stragglers dominating tick tails.
13. **Ortho camera.** Unreachable + latent Y-flip bug (§15). Not needed by Foundry (steep-pitch perspective). File the bug so the next user doesn't trip it.
14. **IME/text input.** No soft-keyboard path, no text-field widget. Foundry's design dodges it entirely (icon naming — GDD §5.8, §6.7); the register records it for whatever game needs typing.
15. **Android keycode table.** Only `AKEYCODE_BACK` is translated (`Zenith/Android/Zenith_Android_Window.cpp`); BT keyboards do nothing. Post-1.0 convenience.
16. **Packaging polish.** Every game currently ships the default Android launcher icon; `versionCode`/`versionName` are hardcoded `1`/`1.0`; only a debug signing config exists. Ship-blocking but mechanical.

**Explicitly game-side (not engine changes):** the gesture recognizer — pinch/two-finger-pan/long-press/double-tap built on `Zenith_Pointers`' verified primitives (8 slots, per-pointer start/excursion/down-time, claims) publishing through `Zenith_InputActions::PublishVirtualButton/PublishVirtualAxis` (`Zenith/Input/Zenith_InputActions.h` — zero engine enum changes; 32 virtual sources, budget fine); SimCore containers (§6); world-space labels via screen-projected text; the instanced-quad minimap fallback. Promotion of any of these into the engine is opportunistic, per the repo's second-consumer rule.

---

## 20. Milestones

Each milestone names its exit criteria; op-count/digest gates accumulate monotonically.

**M0 — Two parallel spikes (the de-risk milestone).**
- *SimCore spike:* belts + inserters + a machine archetype, command queue, digest + op-count harness. **Exit:** a generated 10k-line/50k-item belt city ticks < 2 ms on desktop; digests match across `/Od`/`/O2`; op-counts pinned.
- *Device render spike:* **the single riskiest assumption in the project** — the in-tree instancing record is 2,520 instances, desktop. Harness: 20k building instances + 5k moving item instances + tint/anim churn on the floor phone (Adreno) and one Mali device, uncompressed textures, measuring extract/upload/cull/draw. **Exit:** 60 fps at the §15 budgets, or the fallback decision (chunk-merged static meshes / bespoke belt-item renderer) is taken *now*, while it's cheap.

**M1 — Vertical slice (Windows, mouse-as-touch).** Mine→smelt→assemble→research loop; Core/Reserve/Fabricator; power; save/load with migration tests; audio *emissions* wired through the stub. **Exit:** the GDD's first-15-minutes script is playable; autosave snapshot ≤ 100 ms at slice scale; all digests green.

**M2 — Touch & device.** Gesture recognizer; DPI-scaled UI (register #2); build/inspect flows per GDD §6; lifecycle save (#3); pacing basics (#4); safe-area (#10); ScrollView fixes (#8) landed or scheduled; **Android digest run green on device**. **Exit:** the slice is fully playable by touch on the floor phone, survives kill-after-background, holds 60 fps.

**M3a — Fluids + circuits + construction tools.** Oil arc, fluid networks; circuit networks + combinators; blueprints/copy-paste/undo/deconstruction riding the command queue. Audio backend work (#1) **starts** in parallel; ASTC (#5) and async save (#7) land. **Exit:** oil-processing playable; blueprint stamp/undo digest-tested; autosave hitch-free.

**M3b — Trains + drones.** Rail graph, block signals, schedules; then chain signals as a **separately staged deliverable**; logistics networks + carrier drones. **Exit:** a two-line rail base with intersections runs deadlock-free for a 2-hour soak; drone throughput matches content-table spec exactly (op-counts).

**M4 — Threat.** Pollution CA, nests/evolution, group pathfinding + steering, turrets/walls/ammo, assault drones, alerts UX. **Exit:** scripted max-wave siege (5k units) inside the §1 threat budget on device; peaceful toggle verified; evolution digest-stable.

**M5 — Gigafactory hardening.** Megafactory generators at promise scale (100k buildings / 250k items); thermal ladder (#4); dirty-range instancing (#6) if not already landed; flow-field fallback decision (§13); memory/battery soak. **Exit:** promise-scale factory holds 30 UPS ≤ 8 ms + 60 fps render on the floor device through a 2-hour thermal soak; save ≤ 30 MB / load ≤ 5 s.

**M6 — Content complete & polish.** Full tech tree/recipe roster, Ark endgame, audio backend integration (#1) + full mix, haptics (#11), icons/packaging (#16), balance from telemetry soaks. **Exit:** GDD §13 promises all measured true; first-win playthrough complete on device.

---

## 21. Top risks

| # | Risk | Mitigation |
|---|---|---|
| 1 | **Renderer scale on device is unproven by an order of magnitude** (in-tree max 2,520 instances, desktop; Foundry needs ~20k sustained on Adreno *and* Mali, with full per-frame re-upload today, BC textures unchecked on Mali, HiZ unwired) | M0 device spike **before** presentation architecture hardens; hard visible-set caps by design (§15); registers #5/#6; named fallback (chunk-merged statics + bespoke item renderer) |
| 2 | **Audio is a from-scratch engine subsystem on the critical path of feel** | Emit through the existing `EmitSound` seam from M1 (content never blocks); backend starts M3; M6 is integration, not construction |
| 3 | **Determinism erosion** — one float or hashmap-order iteration in SimCore breaks cross-config saves/replays months later (`/fp:fast` divergence is documented in-tree; ZM-D-183 precedent) | L0-only + no-float lint; `/Od`-vs-`/O2` digest per commit; Android digest per device session; per-module sub-digests for bisection; command-queue-only mutation |
| 4 | **Mobile frame stability**: tick + extract + record share big cores, FIFO-only present amplifies judder, thermal throttling turns a 20-min pass into a 2-h fail | Register #4 at M2; sim ≤ 8 ms tracked from M1; PostTickExtract seam (§14) as the pressure valve (tick can move off-thread as a contained change); M5 thermal ladder + 2-h soaks as exit criteria |
| 5 | **Hidden 20%-tails**: chain-signal semantics/deadlocks, biter separation quality, blueprint/undo UI depth — each system's last fifth is half its work, and M3→M6 has no slack | Chain signals + drone charging staged with named cut lines; flow-field fallback pre-designed behind the path-provider seam; blueprints ride the command queue (inverse commands exist from M1); op-count gates surface perf regressions at commit time, not at M5 |
