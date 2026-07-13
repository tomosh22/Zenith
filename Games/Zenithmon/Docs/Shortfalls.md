# Zenithmon -- Shortfalls & Gap Analysis

**Document purpose:** a frank, gap-by-gap audit of where the current `Games/Zenithmon` tree falls short of the shipping vision in [GameDesignDocument.md](GameDesignDocument.md). This is the most-accurate-current-state doc; cross-check [Status.md](Status.md) before acting on any single line.

**Scope note (2026-07-09, S0):** at S0 essentially EVERYTHING is a gap -- the project is a booting skeleton. This doc is deliberately structured per major system with a one-line current-state each, so later sessions UPDATE lines in place rather than restructure. When a stage lands, replace that system's status line and add a dated note; do not reorder sections.

**Verdict at a glance (updated 2026-07-13, S3 input/controller/camera milestone COMPLETE, ZM-D-055):**
- **What works today:** boot skeleton + the complete S1 data core and deterministic headless battle engine, including feature-complete breeding/gender and Battle Tower logic. S3 E1/E2 supply backward-compatible isolated terrain sets, safe staged/rectangular authoring, and terminal sparse-HIGH streaming (ZM-D-051/052). Dawnmere is the first controller-traversable preview-scene consumer with scene-owned grass (ZM-D-053) and the order-102/103 Player/follow-camera pair (ZM-D-055); the measurement registry adds distinct deterministic Thornacre and Route1 terrain recipes/families and closes the bake-risk question (ZM-D-054). Current game-unit inventory is **264 `ZM_Data` + 384 `ZM_Battle` + 2 `ZM_Boot` + 3 `ZM_TerrainAuthoring` + 5 `ZM_TerrainRecipeSet` + 1 `ZM_Grass` + 20 `ZM_Overworld*` = 679 game tests**; the full boot gate is **1757 ran / 1756 passed / 0 failed / 1 skipped**. There is still no integrated playable loop: persistence, warps, an interior round trip, and S5 remain.
- **What's designed but unbuilt:** persistent GameState, spawn tags/warps, PlayerHome, the remaining outdoor terrain recipes and all playable Thornacre/Route1 scene content; asset generators (S4); overworld<->battle integration + UI (S5/S6); save schema (S7); and broader world/story content and connectivity (S8-S12). The eventual all-assets cold bake remains unmeasured. See [Roadmap.md](Roadmap.md).
- **Locked cuts (not gaps -- see Scope.md):** no audio (engine has none), no networking/multiplayer/trading, no Dynamax-analog, battle format singles only, documented volatile-status cuts (Substitute/Encore/Transform/weight moves).

---

## 1. Per-system gaps (update in place)

### 1.1 Data core

**Status: COMPLETE (S1, 2026-07-10).**
`Source/Data/` holds the full data core as compiled `const` C arrays: the 18-type chart (golden-locked), the 152-species dex (roster + DERIVED base stats + DERIVED level-up learnsets), 218 moves over a 57-kind `ZM_MOVE_EFFECT` enum, 90 items over a 34-kind `ZM_ITEM_EFFECT` enum (incl. 25 TMs -> real moves), 50 abilities (roster + `ZM_ABILITY_HOOK` surface bitmask), 25 natures (exact 5x5 grid), `ZM_StatCalc` (Gen-III+ integer formulas) + `ZM_BattleRNG` (PCG32, deterministic), `ZM_WorldSpec` (schema + 8-scene proving set), and `ZM_DataRegistry` (name->ID lookups + cross-table enforcer). **Gaps carried FORWARD (by design, tracked):** move execution and all 50 ability hooks are live in S2; remaining item/bag/held/TM context is downstream. Base stats + learnsets remain systematic placeholders for S11 balance (ZM-D-021/023), and `ZM_WorldSpec` remains the 8-scene skeleton until S9/S10 (ZM-D-029).

### 1.2 Battle engine

**Status: S2 COMPLETE -- all battle logic + the stage gate PASSED (2026-07-12, ZM-D-047). Post-gate, feature-complete breeding + gender is DONE (user-directed, ZM-D-048/049/050): gender + ratios, real egg groups, GLOOPET Ditto-analog, gendered compatibility, ability + hidden-ability + egg-move inheritance, and derived hatch cycles all shipped across SC-A/B/C. After that, the next battle-adjacent work is S5 integration, not the engine.**
`Source/Battle/` now contains the deterministic append-only battle engine; the complete move/status/catch/switch executor; weather and all 50 ability realizations; `ZM_ExpAndLevel`; `ZM_BattleAI`; `ZM_Breeding` + `ZM_Daycare`; and `ZM_BattleTower`. Box 4 adds four integer curves, derived progression accessors, current cumulative EXP, modern per-opponent party share, capped EV accumulation, mid-battle level/stat/move learning, and terminal level-evolution queuing/pure mutation (67 tests; award-off event/state/RNG identity preserved). Box 5 adds `ZM_BattleAI` -- a pure, side-effect-free four-tier chooser (`ZM_ChooseAction`) that reads state `const`, draws only its own RNG (RANDOM tier only), and perturbs no battle RNG/state/event (box-1..4 goldens byte-identical); 28 `ZM_Battle` tests. Box 6 adds deterministic breeding/daycare and Battle Tower setup/settlement logic; the later feature-complete expansion supplies gender ratios, real egg groups, GLOOPET compatibility, hidden-ability and egg-move inheritance, and hatch cycles. The current inventories are **384 `ZM_Battle`** and **264 `ZM_Data`** tests. The S2 stage gate is complete; remaining battle work is integration/presentation in S5+.
**Tracked deferrals (mechanics gaps, faithfully noted -- not bugs):**
- **Conditional ball bonuses not applied (SC6):** the net/dusk/quick/heal orbs use their base catch param (all x1.0 in the data) -- the type/time/turn/heal conditional multipliers are not yet computed. Faithful to the current `ZM_ItemData` (only great/ultra/prime differ), and the catch core 4-shake math is exact; revisit when the bag/overworld provides the missing context (species types are available, but time-of-day/turn-count/heal-on-catch belong to S5+ integration).
- **High-crit MOVE flag under-applied:** a move's inherent `m_uCritStage==1` (high-crit) still crits at box-1's 1/24, not 1/8 -- it is NOT folded into the `RAISE_CRIT` `m_iCritStage` counter (`{0->1/24,1->1/8,2->1/2,>=3->always}`). Deferred because the two scales differ (move `2`=guaranteed vs counter `2`=1/2) so a correct fold is a scale-reconciliation decision; land it when high-crit/RAISE_CRIT moves get real battle coverage. Golden-invisible today (every tested move is `m_uCritStage 0`).
- **Self-targeting damaging-secondary dropped on a KO:** the one self-buff secondary (Primeval Might, `RAISE_ALL` chance-10) gates its E3 proc on the DEFENDER being alive (per ZM-D-033), so a KO drops the user's self-boost. Contract-consistent (no golden desync) but debatable vs mainline; revisit when self-target secondaries get coverage.

### 1.3 Overworld

**Status: S3 active; the Dawnmere input/controller/camera milestone is complete, but persistence, warps and an interior round trip remain (ZM-D-055).**
Per-scene terrain isolation, safe staged targeting, bounded export, and sparse streaming exist (ZM-D-051/052). The ignored Dawnmere/Thornacre/Route1 terrain families total 896 chunks, 2,700 files, and 672,354,172 bytes; calibrated walls are 59.035 / 69.979 / 80.804 seconds, with an all-warm 16.874-second zero-terrain-recipe queue. Dawnmere alone has `Dawnmere.zscen`, grass regeneration, `ZM_InputActions`, an order-102 velocity-driven dynamic capsule controller and an order-103 fixed-yaw spring/collision follow camera. Walk/run retain 4/7 m/s horizontal-world speed; invalid/nonpositive dt is a true no-op; 45-degree slopes are inclusive; and grounded walkable downslopes receive only tangent adhesion velocity, preserving stronger falls and positive step-assist rises. Qualified steps receive bounded velocity-only assistance. The scene authors the capsule centre at `(512,26.9,480)` from the real baked surface sample. The camera rejects a still-live cached target moved across scenes and generation-safely reacquires its same-scene Player after SINGLE reload. Thornacre and Route1 remain measurement terrain families only: no corresponding authored playable scenes, trees, dressing, NPCs, traversal or connectivity. The next task is persistent `ZM_GameStateManager` plus `ZM_SpawnPoint`/`ZM_WarpTrigger`; PlayerHome and the door round trip follow.

### 1.4 Asset generators

**Status: not started -- lands at S4.**
No creature/human/building/prop generators, no texture synth, and no non-terrain family bake manifest. The creature generator (8 archetypes, mesh+skeleton+6 clips+textures per species, shiny variants, ~7-8.5k lines) is the single biggest work item in the project. Because those families do not exist, the documented 30-50 minute **all-assets** cold-bake budget is still unmeasured even though the terrain component now has a three-recipe projection. Master reference: the StickFigure pipeline in `Tools/Zenith_Tools_TestAssetExport.cpp`.

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
Dawnmere's generated terrain/grass preview is now controller-traversable alongside FrontEnd, but it is not a connected gameplay town. Thornacre and Route1 have measured ignored terrain families, **not** scene/content implementations. There are no routes, gyms, interiors, NPCs, trainers, story flags, badges, rival arc, League, warps, or spawn-tag traversal. The remaining ~38 scene/content families still depend on shared WorldSpec-driven authoring plus the unfinished S3 persistence/connectivity work.

### 1.9 Post-game

**Status: Battle Tower LOGIC exists (S2 box 6, 2026-07-12); the rest lands at S11.**
`ZM_BattleTower` supplies the deterministic tower logic (L50 clamp, streak scaling, AI escalation via `ZM_AI_TIER`, procedural-by-seed opponent teams, streak settlement) as of box 6. Still unbuilt: the Battle Tower SCENE + presentation/HUD, the BP/reward shop, Super/Ultra tiers, named tower trainers + lobby, the Champion rematch, and the balance-simulation tooling -- all S11.

### 1.10 Playthrough tests

**Status: controller/camera and terrain/grass lifecycle smokes exist; connected traversal starts next.**
Current automated coverage is four registered P1 tests. Headless passes `ZM_Boot_Test` and the asset-free `ZM_ControllerHarness_Test`; graphics-required `ZM_GrassRegeneration_Test` and the asset-guarded `ZM_DawnmerePlayerCamera_Test` skip as designed. Windowed evidence is **117 frames / 4990.3 ms** for real Dawnmere movement, camera reacquisition, grass and teardown, plus **11 frames / 1924.3 ms** for the focused grass lifecycle. There is still no warp/interior traversal test, battle smoke, UI flow, segment test, `ZM_AutoTests_Slice`, or full new-game -> Champion `ZM_AutoTests_Playthrough` bot. The hard S3 human gate waits until persistence, PlayerHome/door round trip and the full S3 automated gate are complete; suite-runtime budgets remain unproven at playthrough scale.

---

## 2. Known engine gaps being tracked (E1-E6)

All additive + back-compatible; each lands with unit tests + a RenderTest boot regression check. Sizes and rationale from the approved plan.

| # | Gap | Why it blocks Zenithmon | Lands at |
|---|-----|------------------------|----------|
| E1 | **RESOLVED 2026-07-12 (ZM-D-051):** serialized strict terrain-set name, empty exact legacy layout, all runtime/editor/automation paths routed, safe staged bake cleanup, v1-v4 compatibility | Zenithmon can isolate one terrain asset family per outdoor scene without regressing existing games | S3 |
| E2 | **RESOLVED 2026-07-13 (ZM-D-052):** inclusive anchor-containing bounded export, safe stale-mesh cleanup, and terminal/bounded missing-or-invalid HIGH streaming | Makes one cropped asset family per outdoor scene plausible; ZM-D-054 subsequently completed the three-real-recipe bake/file measurement and resolved Q-2026-07-09-002 | S3 |
| E3 | `Zenith_UIText` has no typewriter reveal (visible-glyph-count property) | Every dialogue line and battle-text line needs it; belongs in the widget, not per-game hacks | S5 |
| E4 | No `Zenith_UIGridLayoutGroup` (existing LayoutGroup is horizontal/vertical only) | Bag / box / dex grids are core UI surfaces | S6 |
| E5 | Grass singleton state persists across SINGLE scene loads (`ResetRenderSystems` never calls `Grass().Reset()`, and `Reset()` leaves instances/flags/density map intact) | Leakage would put tall grass inside gyms and interiors; fix = wire Reset into ResetRenderSystems + widen Reset | S5 |
| E6 | **DEFERRED, post-Zenithmon TODO.** Terrain world-space extent is a global compile-time constant (`Flux_TerrainConfig::CHUNK_GRID_SIZE`/`CHUNK_SIZE_WORLD`/`TERRAIN_SIZE`, `Flux_TerrainConfig.h:27-36`) -- every terrain is a fixed 4096x4096 m grid; density is likewise a fixed constant (`fLowLODDensity`, `Zenith_TerrainComponent.cpp:493`), not a per-instance field. E2's rect export only crops that same fixed grid, it does not resize it, so a tiny route and a large city are forced to the same world-space size today. Fix requires the grid constants to become per-instance serialized fields + dynamic GPU/CPU buffers in the streaming manager + a decoupled density field -- explicitly out of scope for Zenithmon per the E2 rationale ("compile-time constants pervade streaming/grass"). **Keep in mind on every terrain-touching task through the rest of development -- do not build content-side workarounds assuming this changes mid-project.** Revisit as a dedicated engine initiative after Zenithmon ships. | Post-S12 |

---

## 3. Cross-cutting risks (tracked, not yet biting)

- **Terrain bake time / file volume -- MEASUREMENT COMPLETE (ZM-D-054):** calibrated Dawnmere/Thornacre/Route1 walls are **59.035 / 69.979 / 80.804 s** for 772 / 772 / 1,156 files. The 25-set planning model projects 24,676 files / 5.933 GB and 30m 40.833s repeated or 23m 55.857s one-boot/net; the exact-GDD 26-set sensitivity is 25,832 files / 6.196 GB and 32m 01.637s / 24m 59.787s. This closes Q-2026-07-09-002 without triggering optimization, but the full all-assets bake is still unmeasured, no byte ceiling exists, warm "seconds" is qualitative, and only one route was sampled. One set per outdoor scene remains mandatory; optimize the pipeline rather than share sets if later evidence breaches budget.
- **Encounter-transition hitch** -- additive battle design avoids terrain reload; S5 gate asserts the round-trip frame budget.
- **Battle-scene render bleed at offset** -- S5 screenshot gate; fallback SINGLE + snapshot (Questions.md Q-2026-07-09-003).
- **Test-suite runtime blowup** -- battle correctness stays headless; long playthroughs are `m_bManualOnly`; slowest-10 report + hard budget at every gate.
- **Save-schema churn** -- from S7, any schema change requires a version bump + canned-blob migration test in the same commit (gate rule).
- **Content volume (~40 scenes)** -- everything flows from `ZM_WorldSpec` + shared authoring helpers; WorldSpec integrity tests catch dangling references pre-bake.
- **Creature generator underestimation** -- its own stage (S4) with a gallery gate; archetype count can flex 8 -> 6 without touching the dex data model.
