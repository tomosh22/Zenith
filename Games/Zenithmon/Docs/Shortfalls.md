# Zenithmon -- Shortfalls & Gap Analysis

**Document purpose:** a frank, gap-by-gap audit of where the current `Games/Zenithmon` tree falls short of the shipping vision in [GameDesignDocument.md](GameDesignDocument.md). This is the most-accurate-current-state doc; cross-check [Status.md](Status.md) before acting on any single line.

**Scope note (2026-07-09, S0):** at S0 essentially EVERYTHING is a gap -- the project is a booting skeleton. This doc is deliberately structured per major system with a one-line current-state each, so later sessions UPDATE lines in place rather than restructure. When a stage lands, replace that system's status line and add a dated note; do not reorder sections.

**Verdict at a glance (updated 2026-07-10, S1 complete):**
- **What works today:** boot skeleton + the **complete S1 data core** (§1.1) -- 18-type chart, 152-species dex, 218 moves, 90 items, 50 abilities, 25 natures, stat formulas + PCG32 RNG, WorldSpec skeleton, DataRegistry; 102 `ZM_Data` unit tests green. Still no gameplay: the data is inert until the S2 battle engine + S3 overworld consume it.
- **What's designed but unbuilt:** the battle engine (S2) and everything downstream -- see [Roadmap.md](Roadmap.md) for the S2-S12 sequence and gates.
- **Locked cuts (not gaps -- see Scope.md):** no audio (engine has none), no networking/multiplayer/trading, no Dynamax-analog, battle format singles only, documented volatile-status cuts (Substitute/Encore/Transform/weight moves).

---

## 1. Per-system gaps (update in place)

### 1.1 Data core

**Status: COMPLETE (S1, 2026-07-10).**
`Source/Data/` holds the full data core as compiled `const` C arrays: the 18-type chart (golden-locked), the 152-species dex (roster + DERIVED base stats + DERIVED level-up learnsets), 218 moves over a 57-kind `ZM_MOVE_EFFECT` enum, 90 items over a 34-kind `ZM_ITEM_EFFECT` enum (incl. 25 TMs -> real moves), 50 abilities (roster + `ZM_ABILITY_HOOK` surface bitmask), 25 natures (exact 5x5 grid), `ZM_StatCalc` (Gen-III+ integer formulas) + `ZM_BattleRNG` (PCG32, deterministic), `ZM_WorldSpec` (schema + 8-scene proving set), and `ZM_DataRegistry` (name->ID lookups + cross-table enforcer). 102 `ZM_Data` unit tests, all green. **Gaps carried FORWARD (by design, tracked):** the move/item effect enums + ability hooks are INERT data -- their executors are S2 (ZM-D-022/024/026); base stats + learnsets are systematic placeholders for the S11 balance pass (ZM-D-021/023); `ZM_WorldSpec` is a skeleton -- the full ~40-scene world is appended at S9/S10 (ZM-D-029).

### 1.2 Battle engine

**Status: not started -- lands at S2 (logic), S11 (tower balance).**
No battle state machine, executor, damage/catch/status logic, abilities, exp/level/evolution, trainer AI tiers, breeding, or Battle Tower logic. All of it is headless, seeded, deterministic C++ emitting an append-only `ZM_BattleEvent` stream; there is no in-repo turn-based reference to lean on, so this is designed fresh.

### 1.3 Overworld

**Status: not started -- lands at S3 (first scene), scaled at S9/S10.**
No player controller (engine has no character controller -- proper approach is a Jolt capsule + velocity), no follow camera, no input-action wrapper, no warp/spawn-tag system, no persistent `ZM_GameStateManager`, no terrain baked. Blocked first on engine changes E1+E2 (see section 2).

### 1.4 Asset generators

**Status: not started -- lands at S4.**
No creature/human/building/prop generators, no texture synth, no bake manifest. The creature generator (8 archetypes, mesh+skeleton+6 clips+textures per species, shiny variants, ~7-8.5k lines) is the single biggest work item in the project. Master reference: the StickFigure pipeline in `Tools/Zenith_Tools_TestAssetExport.cpp`.

### 1.5 Battle integration (overworld <-> battle)

**Status: not started -- lands at S5.**
No battle scene, no tall-grass encounter system, no additive-load/pause/camera/HUD round trip, no `ZM_BattleDirector` presentation layer. The additive-at-(0,-2000,0) design carries an open verification question (Questions.md Q-2026-07-09-003) with a documented SINGLE+snapshot fallback.

### 1.6 UI

**Status: not started -- battle HUD lands at S5, everything else at S6.**
No dialogue box, menu stack, battle HUD, party/bag/dex/box/shop screens. The engine UI framework is rich (canvas/text/image/button/focus navigation) but has NO dialogue-box, menu-stack, or grid widgets -- E3/E4 engine additions plus game-side composition are all pending.

### 1.7 Save / persistence

**Status: skeleton only -- schema lands at S7.**
`Zenith_SaveData::Initialise("Zenithmon")` runs at boot and the between-tests clear hook is registered (both from S0), but there is no `ZM_SaveSchema`: no slots, no versioned per-module Read/Write, no migration tests, no autosave.

### 1.8 Story / world content

**Status: not started -- slice at S8, buildout at S9/S10.**
No towns, routes, gyms, interiors, NPCs, trainers, story flags, badges, rival arc, League. Zero of the ~40 scenes exist (FrontEnd excepted). Content is WorldSpec rows + shared authoring recipes, so none of this can start before S1's WorldSpec skeleton and S3's authoring path.

### 1.9 Post-game

**Status: not started -- lands at S11.**
No Champion rematch, no Battle Tower (scene or logic), no balance-simulation tooling.

### 1.10 Playthrough tests

**Status: not started -- slice bot at S8, full bot + hardening at S12.**
Current automated coverage is exactly one boot test. No traversal tests, battle smokes, UI flows, segment tests, `ZM_AutoTests_Slice`, or the full new-game -> Champion `ZM_AutoTests_Playthrough` bot (CB_HumanSession pattern). Suite-runtime budgets (TestPlan.md) are untested against reality.

---

## 2. Known engine gaps being tracked (E1-E5)

All additive + back-compatible; each lands with unit tests + a RenderTest boot regression check. Sizes and rationale from the approved plan.

| # | Gap | Why it blocks Zenithmon | Lands at |
|---|-----|------------------------|----------|
| E1 | Terrain asset dir is hard-coded to `<gameassets>/Terrain/` at 6 sites (5 in `Zenith_TerrainComponent.cpp`, 1 in `Flux_TerrainStreamingManager.cpp`) = exactly ONE terrain per game today | Zenithmon needs ~25 terrain sets (one per outdoor scene); fix = serialized terrain-set name, default "" keeps legacy paths | S3 |
| E2 | No rect-bounded chunk export; a full 64x64 grid is ~12k files and minutes-hours per terrain (x25 = ~300k files) | `AddStep_TerrainExportChunksRect` + streaming-path missing-chunk tolerance makes ~25 terrains plausible (~25k files); bake time still to be MEASURED (Questions.md Q-2026-07-09-002) | S3 |
| E3 | `Zenith_UIText` has no typewriter reveal (visible-glyph-count property) | Every dialogue line and battle-text line needs it; belongs in the widget, not per-game hacks | S5 |
| E4 | No `Zenith_UIGridLayoutGroup` (existing LayoutGroup is horizontal/vertical only) | Bag / box / dex grids are core UI surfaces | S6 |
| E5 | Grass singleton state persists across SINGLE scene loads (`ResetRenderSystems` never calls `Grass().Reset()`, and `Reset()` leaves instances/flags/density map intact) | Leakage would put tall grass inside gyms and interiors; fix = wire Reset into ResetRenderSystems + widen Reset | S5 |

---

## 3. Cross-cutting risks (tracked, not yet biting)

- **Terrain bake time / file volume** -- measured at S3 before scale-up; fallback = shared terrain sheets (Questions.md Q-2026-07-09-002).
- **Encounter-transition hitch** -- additive battle design avoids terrain reload; S5 gate asserts the round-trip frame budget.
- **Battle-scene render bleed at offset** -- S5 screenshot gate; fallback SINGLE + snapshot (Questions.md Q-2026-07-09-003).
- **Test-suite runtime blowup** -- battle correctness stays headless; long playthroughs are `m_bManualOnly`; slowest-10 report + hard budget at every gate.
- **Save-schema churn** -- from S7, any schema change requires a version bump + canned-blob migration test in the same commit (gate rule).
- **Content volume (~40 scenes)** -- everything flows from `ZM_WorldSpec` + shared authoring helpers; WorldSpec integrity tests catch dangling references pre-bake.
- **Creature generator underestimation** -- its own stage (S4) with a gallery gate; archetype count can flex 8 -> 6 without touching the dex data model.
