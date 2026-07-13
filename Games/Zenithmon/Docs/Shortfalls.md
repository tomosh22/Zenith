# Zenithmon -- Shortfalls & Gap Analysis

**Document purpose:** a frank, gap-by-gap audit of where the current `Games/Zenithmon` tree falls short of the shipping vision in [GameDesignDocument.md](GameDesignDocument.md). This is the most-accurate-current-state doc; cross-check [Status.md](Status.md) before acting on any single line.

**Scope note (2026-07-09, S0):** at S0 essentially EVERYTHING is a gap -- the project is a booting skeleton. This doc is deliberately structured per major system with a one-line current-state each, so later sessions UPDATE lines in place rather than restructure. When a stage lands, replace that system's status line and add a dated note; do not reorder sections.

**Verdict at a glance (updated 2026-07-13, first S3 terrain scene COMPLETE, ZM-D-053):**
- **What works today:** boot skeleton + the complete S1 data core and deterministic headless battle engine, including feature-complete breeding/gender and Battle Tower logic. S3 E1/E2 supply backward-compatible isolated terrain sets, safe staged/rectangular authoring, and terminal sparse-HIGH streaming (ZM-D-051/052). Dawnmere is the first real outdoor consumer: an ignored deterministic 16x16 terrain family and ignored preview scene load/reload with scene-owned grass regeneration (ZM-D-053). Current game-unit inventory is **264 `ZM_Data` + 384 `ZM_Battle` + 2 `ZM_Boot` + 3 `ZM_TerrainAuthoring` + 1 `ZM_Grass`**; the full boot gate is **1732 ran / 1731 passed / 0 failed / 1 skipped**. There is still no integrated playable loop: the rest of S3/S5 remains.
- **What's designed but unbuilt:** the remaining S3 three-scene bake measurement (Dawnmere is only sample 1/3), player/controller/camera/warp, and the remaining outdoor terrain recipes; asset generators (S4); overworld<->battle integration + UI (S5/S6); save schema (S7); and broader world/story content and connectivity (S8-S12). See [Roadmap.md](Roadmap.md).
- **Locked cuts (not gaps -- see Scope.md):** no audio (engine has none), no networking/multiplayer/trading, no Dynamax-analog, battle format singles only, documented volatile-status cuts (Substitute/Encore/Transform/weight moves).

---

## 1. Per-system gaps (update in place)

### 1.1 Data core

**Status: COMPLETE (S1, 2026-07-10).**
`Source/Data/` holds the full data core as compiled `const` C arrays: the 18-type chart (golden-locked), the 152-species dex (roster + DERIVED base stats + DERIVED level-up learnsets), 218 moves over a 57-kind `ZM_MOVE_EFFECT` enum, 90 items over a 34-kind `ZM_ITEM_EFFECT` enum (incl. 25 TMs -> real moves), 50 abilities (roster + `ZM_ABILITY_HOOK` surface bitmask), 25 natures (exact 5x5 grid), `ZM_StatCalc` (Gen-III+ integer formulas) + `ZM_BattleRNG` (PCG32, deterministic), `ZM_WorldSpec` (schema + 8-scene proving set), and `ZM_DataRegistry` (name->ID lookups + cross-table enforcer). **Gaps carried FORWARD (by design, tracked):** move execution and all 50 ability hooks are live in S2; remaining item/bag/held/TM context is downstream. Base stats + learnsets remain systematic placeholders for S11 balance (ZM-D-021/023), and `ZM_WorldSpec` remains the 8-scene skeleton until S9/S10 (ZM-D-029).

### 1.2 Battle engine

**Status: S2 COMPLETE -- all battle logic + the stage gate PASSED (2026-07-12, ZM-D-047). Post-gate, feature-complete breeding + gender is DONE (user-directed, ZM-D-048/049/050): gender + ratios, real egg groups, GLOOPET Ditto-analog, gendered compatibility, ability + hidden-ability + egg-move inheritance, and derived hatch cycles all shipped across SC-A/B/C. After that, the next battle-adjacent work is S5 integration, not the engine.**
`Source/Battle/` now contains the deterministic append-only battle engine; the complete move/status/catch/switch executor; weather and all 50 ability realizations; `ZM_ExpAndLevel`; `ZM_BattleAI`; `ZM_Breeding` + `ZM_Daycare`; and `ZM_BattleTower`. Box 4 adds four integer curves, derived progression accessors, current cumulative EXP, modern per-opponent party share, capped EV accumulation, mid-battle level/stat/move learning, and terminal level-evolution queuing/pure mutation (67 tests; award-off event/state/RNG identity preserved). Box 5 adds `ZM_BattleAI` -- a pure, side-effect-free four-tier chooser (`ZM_ChooseAction`) that reads state `const`, draws only its own RNG (RANDOM tier only), and perturbs no battle RNG/state/event (box-1..4 goldens byte-identical); 28 `ZM_Battle` tests. Box 6 SC1 adds `ZM_Breeding` + `ZM_Daycare` -- pure deterministic egg-gen (offspring = base-evo of the mother; K-IV inheritance 3/5; Stasis-Stone nature lock; mother ability; base-evo L1 learnset) + daycare (deposit <=2, 1 exp/step, 256-step egg threshold); a reduced model on the shipped data (archetype = egg-group proxy; no gender/egg-groups/egg-moves, Q-2026-07-12-004); 33 `ZM_Data` tests. Box 6 SC2 adds `ZM_BattleTower` -- pure logic that produces L50-clamped teams + a streak-scaled AI tier (consuming `ZM_AI_TIER`) + a battle config and settles a win/loss streak (a caller runs the battle); procedural-by-seed 3-mon opponent teams; 25 tests (23 `ZM_Data` + 2 `ZM_Battle` engine smokes). The battle suite is **384 `ZM_Battle` tests**; `ZM_Data` is now **209**; the full boot baseline is **1663**. **Still unbuilt (this stage):** the S2 automated stage gate (the scenario-battle + 2,000-battle fuzz-soak verification pass).
**Tracked deferrals (mechanics gaps, faithfully noted -- not bugs):**
- **Conditional ball bonuses not applied (SC6):** the net/dusk/quick/heal orbs use their base catch param (all x1.0 in the data) -- the type/time/turn/heal conditional multipliers are not yet computed. Faithful to the current `ZM_ItemData` (only great/ultra/prime differ), and the catch core 4-shake math is exact; revisit when the bag/overworld provides the missing context (species types are available, but time-of-day/turn-count/heal-on-catch belong to S5+ integration).
- **High-crit MOVE flag under-applied:** a move's inherent `m_uCritStage==1` (high-crit) still crits at box-1's 1/24, not 1/8 -- it is NOT folded into the `RAISE_CRIT` `m_iCritStage` counter (`{0->1/24,1->1/8,2->1/2,>=3->always}`). Deferred because the two scales differ (move `2`=guaranteed vs counter `2`=1/2) so a correct fold is a scale-reconciliation decision; land it when high-crit/RAISE_CRIT moves get real battle coverage. Golden-invisible today (every tested move is `m_uCritStage 0`).
- **Self-targeting damaging-secondary dropped on a KO:** the one self-buff secondary (Primeval Might, `RAISE_ALL` chance-10) gates its E3 proc on the DEFENDER being alive (per ZM-D-033), so a KO drops the user's self-boost. Contract-consistent (no golden desync) but debatable vs mainline; revisit when self-target secondaries get coverage.

### 1.3 Overworld

**Status: S3 active; the first outdoor terrain scene exists, but it is not a playable or connected overworld yet (ZM-D-053).**
Per-scene terrain isolation, safe staged targeting, bounded export, and sparse streaming exist (ZM-D-051/052). Dawnmere now has a workspace-local ignored 772-file terrain family plus `Dawnmere.zscen`; its deterministic recipe spans world `0..1024`, exports 16x16 chunks, and regenerates grass on load/reload. It intentionally has no trees. There is still no player controller (the engine has no character controller -- proper approach is a Jolt capsule + velocity), follow camera, input-action wrapper, warp/spawn-tag system, persistent `ZM_GameStateManager`, building/prop dressing, or world connectivity. The next task is the still-open three-real-scene bake-time measurement, not a claim that terrain scale-up is proven.

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

**Status: broader gameplay/world connectivity not started -- slice at S8, buildout at S9/S10.**
Dawnmere's generated terrain preview now exists alongside FrontEnd, but it is not a connected or playable town: there are no routes, gyms, interiors, NPCs, trainers, story flags, badges, rival arc, League, warps, or spawn-tag traversal. The remaining ~38 scene/content families still depend on shared WorldSpec-driven authoring plus the unfinished S3 controller/connectivity work.

### 1.9 Post-game

**Status: Battle Tower LOGIC exists (S2 box 6, 2026-07-12); the rest lands at S11.**
`ZM_BattleTower` supplies the deterministic tower logic (L50 clamp, streak scaling, AI escalation via `ZM_AI_TIER`, procedural-by-seed opponent teams, streak settlement) as of box 6. Still unbuilt: the Battle Tower SCENE + presentation/HUD, the BP/reward shop, Super/Ultra tiers, named tower trainers + lobby, the Champion rematch, and the balance-simulation tooling -- all S11.

### 1.10 Playthrough tests

**Status: terrain/grass lifecycle smoke exists; traversal starts later.**
Current automated coverage is two registered P1 tests: `ZM_Boot_Test` and the graphics-required, asset-guarded `ZM_GrassRegeneration_Test`. The headless batch reports 2/2 outcomes with the grass test skipped as designed; the windowed grass test loads and reloads Dawnmere, checks exact regeneration/no accumulation, then verifies FrontEnd cleanup. There are still no traversal tests, battle smokes, UI flows, segment tests, `ZM_AutoTests_Slice`, or the full new-game -> Champion `ZM_AutoTests_Playthrough` bot (CB_HumanSession pattern). Suite-runtime budgets remain unproven at playthrough scale.

---

## 2. Known engine gaps being tracked (E1-E6)

All additive + back-compatible; each lands with unit tests + a RenderTest boot regression check. Sizes and rationale from the approved plan.

| # | Gap | Why it blocks Zenithmon | Lands at |
|---|-----|------------------------|----------|
| E1 | **RESOLVED 2026-07-12 (ZM-D-051):** serialized strict terrain-set name, empty exact legacy layout, all runtime/editor/automation paths routed, safe staged bake cleanup, v1-v4 compatibility | Zenithmon can isolate one terrain asset family per outdoor scene without regressing existing games | S3 |
| E2 | **RESOLVED 2026-07-13 (ZM-D-052):** inclusive anchor-containing bounded export, safe stale-mesh cleanup, and terminal/bounded missing-or-invalid HIGH streaming | Makes one cropped asset family per outdoor scene plausible; bake time/file volume still must be MEASURED with three real scenes (Q-2026-07-09-002) | S3 |
| E3 | `Zenith_UIText` has no typewriter reveal (visible-glyph-count property) | Every dialogue line and battle-text line needs it; belongs in the widget, not per-game hacks | S5 |
| E4 | No `Zenith_UIGridLayoutGroup` (existing LayoutGroup is horizontal/vertical only) | Bag / box / dex grids are core UI surfaces | S6 |
| E5 | Grass singleton state persists across SINGLE scene loads (`ResetRenderSystems` never calls `Grass().Reset()`, and `Reset()` leaves instances/flags/density map intact) | Leakage would put tall grass inside gyms and interiors; fix = wire Reset into ResetRenderSystems + widen Reset | S5 |
| E6 | **DEFERRED, post-Zenithmon TODO.** Terrain world-space extent is a global compile-time constant (`Flux_TerrainConfig::CHUNK_GRID_SIZE`/`CHUNK_SIZE_WORLD`/`TERRAIN_SIZE`, `Flux_TerrainConfig.h:27-36`) -- every terrain is a fixed 4096x4096 m grid; density is likewise a fixed constant (`fLowLODDensity`, `Zenith_TerrainComponent.cpp:493`), not a per-instance field. E2's rect export only crops that same fixed grid, it does not resize it, so a tiny route and a large city are forced to the same world-space size today. Fix requires the grid constants to become per-instance serialized fields + dynamic GPU/CPU buffers in the streaming manager + a decoupled density field -- explicitly out of scope for Zenithmon per the E2 rationale ("compile-time constants pervade streaming/grass"). **Keep in mind on every terrain-touching task through the rest of development -- do not build content-side workarounds assuming this changes mid-project.** Revisit as a dedicated engine initiative after Zenithmon ships. | Post-S12 |

---

## 3. Cross-cutting risks (tracked, not yet biting)

- **Terrain bake time / file volume** -- Dawnmere's 16x16 family is the first measured sample (**63.671 s** cold, 772 files), but the required three-real-scene measurement remains open before scale-up. One terrain set per outdoor scene/route is a hard requirement (user directive 2026-07-11); fallback if too slow = optimize the bake pipeline, not shared terrain sheets (Questions.md Q-2026-07-09-002).
- **Encounter-transition hitch** -- additive battle design avoids terrain reload; S5 gate asserts the round-trip frame budget.
- **Battle-scene render bleed at offset** -- S5 screenshot gate; fallback SINGLE + snapshot (Questions.md Q-2026-07-09-003).
- **Test-suite runtime blowup** -- battle correctness stays headless; long playthroughs are `m_bManualOnly`; slowest-10 report + hard budget at every gate.
- **Save-schema churn** -- from S7, any schema change requires a version bump + canned-blob migration test in the same commit (gate rule).
- **Content volume (~40 scenes)** -- everything flows from `ZM_WorldSpec` + shared authoring helpers; WorldSpec integrity tests catch dangling references pre-bake.
- **Creature generator underestimation** -- its own stage (S4) with a gallery gate; archetype count can flex 8 -> 6 without touching the dex data model.
