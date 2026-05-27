# Devil's Playground — Shortfalls & Gap Analysis

**Document purpose:** A frank, gap-by-gap audit of where the current `DevilsPlayground` prototype falls short of the shipping vision described in [GameDesignDocument.md](GameDesignDocument.md). Written for producers, leads, and external partners assessing the runway from "skeleton-grade port" to "ship-ready PC/console title."

**Scope:** Everything in the current `Games/DevilsPlayground/` tree. **Note (2026-05-20):** this doc was written against the 2026-05-11 skeleton-grade port. Phase 1 + Phase 2 + most of Phase 4 (4.1.1, 4.3.1, 4.3.2, 4.3.3 -- only 4.3.4 HUMAN_GATE remains) + a substantial Phase 5 telemetry / verification system + instructional HUD + the procgen migration (PRs #96-#117 retiring the UE-exported GameLevel + gym scenes) + telemetry v3 + seed-matrix tooling + the personality-matrix-driven balance pass have all shipped since (see [Status.md](Status.md) for the current state) — many "missing" items listed below are now landed. Treat the headline gaps in §1 as historical snapshots and cross-check against Status.md before acting on any one.

**Updated 2026-05-11** (post peer-review reconciliation), **2026-05-16** (test-count + telemetry), **2026-05-20** (procgen migration + matrix tooling + Zealot personality), **2026-05-22** (game-balance pass + door collider physics fix + 4 new bot personalities; see Status.md + DecisionLog.md), **2026-05-26** (doors-at-DoorPoints PR -- two doors per corridor, unlocked-by-default, sticky-unlock + iron-auto-scales; sensor toggle on open-door collider; priest opens doors; priest spawn near unlocked door; bot bootstrap mandatory for all personalities), **2026-05-27** (engine-wide `*Impl` removal + `g_xEngine.X()` facade collapse `9e4253ca` — no DP gameplay impact; priest mesh debug tint set to red `57443e53` — cosmetic only):

- **2026-05-26 doors-at-DoorPoints landed.** Replaces the single corridor-midpoint door with TWO wall-aligned doors per corridor (one per DoorPoint, integer-derived yaw); brown debug markers gone; navmesh portals stitched at the geometric door centre; corner-anchored door collider becomes a sensor when not Closed so the swinging arm doesn't shove the player capsule. Priest opens any closed unlocked door within 4 m (was pursuit-gated; pursuit-gating left the priest stuck in its spawn room on patrol-only seeds, telemetry-confirmed seed 5 = 27 unique positions over 200 s). Priest spawn position shifted 1.5 m inside the room from an unlocked doorpoint so OpenNearbyDoorsFor catches the door from frame 1. Procgen `PickPriestRoom` enforces ≥1 corridor on the picked room. Note: the priest opens its first door at frame 1 — before the personality test's telemetry recorder starts (`Begin()` is deferred to `kHP_CaptureRefs` so FrontEnd boot frames don't pollute the recording), so a per-seed "doors opened by priest" telemetry filter under-counts the priest's true door interactions. Add an early `Begin()` to verify.
- **2026-05-26 balance criteria fully met.** Every personality 70-90% wins, every canonical-seed level winnable by ≥1 personality (seed 1 previously unwinnable; fixed via three layered changes -- forge placement, pentagram-defer in DPDoor, opportunistic delivery in the bot -- see §3.10 history). Current canonical 10-seed set yields **80 % matrix wins** (64 / 80 cells, up from the 60 % pre-PR baseline).
- **2026-05-22 balance criteria ratified + met:** every personality WR strictly in (0%, 100%), every canonical-seed level winnable by ≥1 personality. PR #141. Full writeup in `Docs/GameBalance_2026-05-22.md`.
- **2026-05-22 door collider physics fix** -- prior to this, closed doors were 1m cubes at floor level that neither the bot's grid raycast nor the player's capsule physics actually collided with. The priest was the only AI that respected door state, via the navmesh BLOCKED flag. Player + bot now physically blocked by closed doors.
- **2026-05-21 three new bot personalities** (Magpie / Relay / Heretic, PR #139) exercising design axes the 4-personality matrix didn't cover. PR #140 added Trickster (Magpie+Relay+bootstrap+adaptive sprint combo) + four follow-up fixes (Heretic emit-and-flee, walk-quiet rebalance, Relay click reliability, footstep loudness).
- **2026-05-22 new follow-ups added** to §3 below: procgen seed 0 unsolvable (`ValidateSolvability` warns instead of reject+retry), navmesh / Pathfinding bot integration deferred (engine-side issue in `Zenith_NavMeshGenerator` polygon coverage).

- Prototype originally contained 24 test `.cpp` files registering 34 tests; as of 2026-05-27 the suite is **133 `ZENITH_AUTOMATED_TEST_REGISTER` invocations across 113 .cpp files** at HEAD (verified via `grep -c ZENITH_AUTOMATED_TEST_REGISTER Games/DevilsPlayground/Tests/*.cpp`). Net growth from 2026-05-20's 117 reflects the 4 new bot personalities (Magpie / Relay / Heretic / Trickster) + the doors-at-DoorPoints regression tests + the convention-cleanup test split-out. Some subset is `#ifdef ZENITH_INPUT_SIMULATOR`-gated; the headless-pass count is the load-bearing number rather than the macro count (see Status.md for the live caveat).
- Telemetry v3 shipped 2026-05-19 (PR #120) -- adds AI intent / per-villager life timer / held-item tag / camera state / per-frame perf ms + 12 new event types (Apprehend channel start/complete/interrupted, PerceptionContactBegin/End, etc).
- Seed-matrix tooling shipped 2026-05-20 (PR #121) -- `Tools/dp_seed_matrix_run.ps1` cycles N seeds × 4 personalities; `Tools/dp_seed_matrix_analyse.py` aggregates into `REPORT.md`.
- Single gameplay scene is **ProcLevel** (build index 1), built at runtime by `DPProcLevel::Generate(seed, cfg)`. The hand-authored GameLevel + the 4 gym scenes + `Tools/dp_export/` have been removed (PR #117). Villager spawn count is now procgen-variable (typically 12-18 per layout depending on room count).
- `DPFogPass.cpp` is substantially implemented (~312 lines with `SetupDPFog`/`ExecuteDPFog`/`BuildPipelines`). The shader file `Zenith/Flux/Shaders/Fog/DP_Fog.slang` **also exists** (with compiled `.spv` reflection files) — corrected 2026-05-12 round-4 peer review; earlier rounds claimed this file needed authoring (MVP-2.4.1) but it's already in the repo. The remaining MVP-2.4 work is *test coverage + memory-fog implementation*, not shader authoring.
- Non-tools builds (`*_False` configurations) were **fixed as of 2026-05-10** per project memory; previous "broken" claims in this doc are obsolete (see §3.8 below).

**Verdict at a glance:**
- **What works today:** ~25% of the shipping scope. The bones are real.
- **What's stubbed but architected:** ~30%. Hook points exist, behaviour is placeholder.
- **What's not yet imagined in code:** ~45%. Net-new design from the GDD.

**Estimated time to bridge:** ~18 months with a team of 12 (see GDD §12). The systems gaps are tractable; the **content volume** and **art-direction** gaps dwarf everything else.

---

## 1. Headline gaps (the things that block shipping today)

These are the issues that, if left as-is, prevent the prototype from being demonstrable to a publisher as anything more than a tech sample.

### 1.1 The witch-finder cannot end the run

**Status:** ✅ RESOLVED 2026-05-14. **GDD ref:** §4.5.

PR #41 (`MVP-1.3.1+1.3.2`) shipped the `Apprehend` BT branch + `DP_OnRunLost{Apprehended}` event. PRs #42, #54 (NoVessels), #57 (Dawn) added the other two loss causes. PR #75 wired the HUD banner per cause. Coverage tests `Test_P1Apprehend_PriestCatchesPlayer`, `Test_P1Apprehend_SwitchBreaksChannel`, `Test_P1Apprehend_OutOfRangeIgnored`, `Test_P1Apprehend_PriestStandsStillDuringChannel` pin the contract.

_Historical text (kept for record):_ The prototype's `Priest_Behaviour` perceives, pursues, and navigates — but **there is no collision check between the priest and the possessed villager** and no apprehend logic. The player literally cannot lose to the antagonist.

### 1.2 No real navmesh — only a single 200×200 m polygon

**Status:** ✅ RESOLVED 2026-05-14. **GDD ref:** §6.1, §4.5.

Generated-from-colliders path (option a) shipped via PRs #32 (walls block via six-face emit + obstruction-span clearance check), #33 (O(N) adjacency, ~850 ms generate), #36 (portal stitch + ColliderComponent opt-out for runtime-blockable obstacles), #37 (region-keyed vertex dedup), #39 (`Priest_Behaviour` refreshes live navmesh + reactive-selector hack), #40 (closed-door + regen-on-swap tests). `Test_P1NavMesh_PathRespectsWalls`, `Test_P1NavMesh_ClosedDoorBlocksPath`, `Test_P1NavMesh_RegenerationOnSceneSwap`, `Test_T1NavMesh_BTUnitsCanFollowRealPath`, `PriestPursuit_Test` all pin the contract.

_Historical text:_ The prototype's `DP_AI::GetOrBuildLevelNavMesh` returned a synthetic flat quad; the priest could walk through any wall. Bridge cost was estimated at ~6 engineering weeks; actual delivery was ~2 days across 6 PRs.

### 1.3 Pause is a lie

**Status:** ✅ RESOLVED. **GDD ref:** §7.2.

PR #30 (`MVP-1.1` Real pause) wired Esc to `Zenith_SceneManager::SetScenePaused` on the gameplay scene; controller migrates to the persistent scene so it keeps ticking. PR #71 added R/Q shortcuts (restart/quit-to-menu) while paused. PR #74 caught a state-leak where `HandleRestart` flipped the flag without actually reloading the scene; extracted `ResetAllRunStateBeforeReload()` helper. Tests `Test_P1Pause_TimerStopsOnEscape`, `Test_P1Pause_PriestStopsOnEscape`, `Test_P1Pause_InputSimDuringPause`, `Test_P2Menu_PauseAndMainMenuShortcuts`, `Test_P2Pause_RestartActuallyReloadsScene` pin the chain. Audio ducking pass still pending.

### 1.4 No villager animations at all

**Status:** Critical-cosmetic. **GDD ref:** §8.3.

Villagers are static capsule colliders. They don't walk; their position is set by velocity each frame. There are no idle animations, no walk cycles, no possession-channel animations. The held-item visual is a tinted cube floating 1.6 m above the body. Everything in `DPMaterials.h/.cpp` is procedural tint generation on top of placeholder cubes.

This is not "polish" — it's the difference between "code prototype" and "game." No publisher demo can hide it.

**Bridge cost:** ~8 character-art weeks for 24 archetypes × 12 animations + integration. Engine animation system (`Flux/MeshAnimation/`) is mature; the work is asset authoring + retargeting.

### 1.5 Items, doors, chests are cubes with tints

**Status:** Critical-cosmetic. **GDD ref:** §4.3, §4.4, §8.

The item-tag → tint-colour system (`DPMaterials::GetOrCreateColouredVariant`) is clever debug rendering but it's the entire current visual language of items. Iron, key, skeleton key, and all five objectives are visually distinguished by hue alone. Doors are scaled cubes. The chest's lid does not actually open (`m_fOpenT` is incremented but no transform applied).

**Bridge cost:** ~6 art-weeks for the full prop set (~40 unique props at shipping). Includes modelling, texturing, light-baked variants.

---

## 2. Mechanics gaps (the design surface the prototype hasn't touched)

These are systems described in the GDD that don't exist in code at all yet. None are "blockers" individually, but the cumulative absence is what makes the prototype 25%-complete rather than 60%.

### 2.1 Possession depth

| GDD feature | Prototype state | 2026-05-15 status |
|---|---|---|
| 24 villager archetypes with distinct timers/abilities | 1 archetype. All villagers identical. | ✅ 4 MVP archetypes shipped (Farmhand/Beggar/Devout/Child) via PRs #19, #20, #58, #59, #62. Remaining 20 are post-MVP per `Config/Archetypes.json` `"mvp": false` filter. |
| Possession range tied to last death anchor | No range limit; any villager anywhere on the map can be clicked. | ✅ Shipped in PR #44 (`MVP-1.8` 15 m anchor; anchor moves on each successful hop). |
| Possession channel (0.8 s for Devout) | Instant for all. | ✅ Shipped in PR #62 (`MVP-2.1.1+2` Devout channel + priest interrupt). |
| Possession cooldown (1.5 s after voluntary switch) | None. Players can chain-click. | ✅ Shipped in PR #43 (`MVP-1.5` 1.5 s cooldown after voluntary switch; death/apprehend bypass). |
| Demon-scent (per-body, decays) | No tracking of possession history per body. | ✅ Shipped in PR #45 (`MVP-1.6` table + decay + write to priest BB). |
| Voluntary switch (faint vs. die) | Cannot voluntarily switch — switching during life-timer just transfers state immediately, body stays put. | ✅ Shipped in PR #55 (`MVP-1.4.1–3` Idle/Possessed/Fainted/Dead state machine with 10 s recovery); coverage gap closed in PR #60. |
| Sprint as life-cost mechanic | Single speed. No sprint, no walk. | ✅ Shipped in PRs #46 (sprint), #47 (walk-quiet), #53 (Aelfric effective hearing halved by walk-quiet). |

**Bridge cost (original estimate ~6+4 weeks):** actual delivery was ~2 days of autonomous-loop work across 11 PRs.

### 2.2 The witch-finder is half a character

The prototype's priest covers perception (sight + hearing), patrolling, investigating, and pursuing. That's roughly 60% of the GDD's antagonist. Missing:

- **Apprehend** (see §1.1).
- **Hounds** (Aram & Tobit). Separate agents with their own perception cones. No prototype for any of this.
- **Demon-scent tracking.** Tied to §2.1.
- **Three personality variants** (Cautious / Cruel / Drunk). Single configuration in code.
- **Memory.** Last-investigated waypoint, last-suspicious-villager. Currently each tick is stateless beyond the BB.
- **Mid-Night escalation** (lantern → hounds → flintlock → bless → exorcise).
- **Whisper-line narration.** The priest's behaviour is not surfaced to the player at all. Players cannot understand why he's at a given location.

**Bridge cost:** ~10 engineering weeks for all of the above, plus a parallel design pass on tuning.

### 2.3 No meta-progression / Liminal

The GDD's roguelite layer — the **Liminal** hub, three hermit trees, the Knot currency, permanent unlocks — does not exist in the prototype. There's no save system for run results, no currency, no unlock graph, no menu surfacing.

**Bridge cost:** ~8 engineering weeks. Save/load + menu integration is the lion's share. The unlocks themselves leverage existing systems (timer modifications, ability flags on villagers).

### 2.4 Procedural Night-to-Night variance

**Status:** ✅ SUBSTANTIVELY RESOLVED 2026-05-19. **GDD ref:** §6.2.

The procgen migration (PRs #96-#117) replaced the hand-authored GameLevel with `DPProcLevel::Generate(seed, cfg, ...)` -- a BSP-based generator with integer-coord internals (bit-deterministic across `/fp:fast` configs, verified by `Test_ProcLevel_DeterminismCheck`). Each seed produces a different room graph, wall layout, door placement, game-element placement, villager / priest spawn, and patrol nodes. Seeds are overridable via the `DP_PROCGEN_SEED` env var; the seed-matrix runner (PR #121) cycles 10+ seeds to expose layout-personality interactions.

What's still pending under "shuffler" framing:
- **Reagent spawn shuffler.** Procgen does place 5 objectives at varied spawners per seed, but the *which-reagent-where* mapping isn't yet driven by the per-Night design (e.g. "Night 3 always has BogWater"). Tied to design budget.
- **Aelfric variant picker.** A single Aelfric configuration (the GDD's three variants -- Cautious / Cruel / Drunk -- are post-MVP).
- **Weather state.** Out of MVP scope.

**Bridge cost remaining:** ~1-2 engineering weeks for the per-Night reagent / archetype mapping; the procgen generator itself is shipped.

### 2.5 Reagent system is five duplicates of one thing

`DP_ItemTag::Objective1` through `Objective5` are five interchangeable identifiers. The GDD's reagent design (each with a unique pickup channel, a candidate-spawn-set, a special behaviour like the bog-water evaporating in 8 s) does not exist. The pentagram accepts any objective tag without distinguishing them.

**Bridge cost:** ~3 engineering weeks + ~4 design weeks (per-reagent flavour).

### 2.6 Crafting is a single hardcoded recipe

`DPForge_Behaviour` does Iron → Key. The GDD's six-recipe table (Iron+Wood, Iron+Brass, etc.) requires a recipe-data system and craft UI. The forge has no animation, no time cost, no sound — the player taps F and the item appears.

**Bridge cost:** ~3 engineering weeks.

### 2.7 No charms, no distractions beyond noise machines

The GDD posits charms (rowan, salt, St. George medal), distraction props (music box, tin whistle, tinder-bundle), and a confessional hide-mechanic. None of this exists in code. The single noise machine (`DummyNoiseMachine_Behaviour`) is the entire surface area of "player tools to manipulate the priest."

**Bridge cost:** ~5 engineering weeks for the systems, ~3 design weeks for tuning.

### 2.8 No alternate win conditions, no Joan Trew, no narrative

The prototype's win condition is: 5 objectives → pentagram → victory event. The GDD's Night 5 ("free Joan from the longhouse") and Night 7 ("the chapel is sealed") require:
- Non-possessable NPCs.
- NPC-state-driven objectives (Joan's location, Joan's restraint state).
- Narrative scaffolding (cutscenes, between-Night interstitials, branching ending state).

**Bridge cost:** ~6 engineering weeks for narrative-scaffolding systems + writing/cutscene authoring is its own production discipline (~12 weeks).

### 2.9 Camera is intentionally locked

The orbit camera is fixed-pitch at ~1.2 rad (~69°). The GDD requires adjustable pitch (0°–80° accessibility), an optional follow mode, and console-input-mapped rotation. The prototype supports Q/E rotate, mouse-wheel zoom, no follow, no tilt.

**Bridge cost:** ~2 engineering weeks.

### 2.10 Inventory is one slot — but with no "drop" or "give" verb

Held items are managed via `DP_Player::SetHeldItem` / `RemoveHeldItem`. There is no drop-on-ground verb (G key in GDD). There's no give-to-other-villager (which the GDD doesn't strictly require but hand-off chains depend on dropping in a location for a later possession to grab).

**Bridge cost:** ~2 engineering weeks. Inventory & pickup are well-architected; drop just needs a UI verb + a 3D world-pickup re-anchor.

---

## 3. Engine / tech gaps

Issues in the engine or its DevilsPlayground integration that limit shipping quality.

### 3.1 Volumetric fog shader is not implemented

`DPFogPass.h/.cpp` registers a no-op render pass and disables the engine's existing 6-pass fog system. The GDD's fog-of-war design (memory fog, light-driven visibility, fog-of-war on Aelfric only by edge visibility, weather variants) needs an actual shader. The `Flux/Fog/CLAUDE.md` system probably has reusable volumetric-fog code; the question is whether re-using the engine fog and overlaying fog-of-war as a separate post-pass is cheaper than the current "disable engine fog, do everything in DP" approach.

**Bridge cost:** ~6 engineering weeks for a custom shader pass with memory-fog and per-villager visibility holes.

### 3.2 Particle effects are placeholder

`DPFogPass::RegisterParticleConfigs` exists, the witch's spawn coordinate is exposed, and a single PFX_Witch config is registered. There is no actual particle work — possession transitions, frost trails, lantern dust-motes, hearth smoke, hound breath in cold air. The GDD calls for ~25 distinct particle systems.

**Bridge cost:** ~6 art-weeks (FX artist) + ~2 engineering weeks for system hooks.

### 3.3 Audio is silent

The prototype has no audio integration in DP-specific code (engine has a sound system but DP doesn't drive it). No SFX for doors, footsteps, forge, bell, Aelfric mutters, hounds. No music. No spatialised sound.

**Bridge cost:** ~12 weeks (audio lead + composer) for a complete bible.

### 3.4 Editor automation system is the right tool, used the wrong way

`Project_RegisterEditorAutomationSteps` authors scenes declaratively via `AddStep_*` calls. This is the **canonical pattern** for Zenith scenes (per memory: "Marble.cpp is the canonical template"). The DP project uses it correctly today.

But for shipping, scenes are loaded from `.zscen` files, not authored at runtime. The Sharpmake configurations gate this on `ZENITH_TOOLS`. Currently the project is well-architected for tools-mode authoring but the **non-tools-mode pre-baked scene assets are out of date** the moment the authoring code changes. Workflow risk.

**Bridge cost:** ~1 engineering week to add a "bake all scenes" CI step that re-runs the editor-automation chain and writes `.zscen` files automatically. Also confirms scenes load correctly without `ZENITH_TOOLS`.

### 3.5 Save / load doesn't exist

There's no save-game system in DP or visible in the broader Zenith engine touched by this project. For a roguelite with permanent meta-progression, save/load is foundational. Cross-platform cloud save adds complexity.

**Bridge cost:** ~4 engineering weeks for local save + ~3 weeks for cloud save (Steam, Xbox Live, PSN).

### 3.6 No localisation pipeline

All strings are hardcoded `const char*` literals. Whisper-line narration, item names, Aelfric VO subtitles, menu text, archetype names — all need to flow through a localisation system. The GDD targets 8 languages at launch.

**Bridge cost:** ~3 engineering weeks for the loc table system + per-language localisation budget (~$15k/language for ~5000 words).

### 3.7 Performance is not validated for console targets

The prototype runs in debug builds on PC and has automated test coverage at ~60 fps with fixed dt. The shipping targets (Switch 2 at 30 fps, Series S at 60 fps with dynamic res) have not been profiled. The fog-hole gather (up to DP_FOG_MAX_HOLES = 60 entries/frame, scene-wide light enumeration) and the perception system's stimulus broadcast (currently O(agents × stimuli)) are both candidates for hot-path optimisation.

**Bridge cost:** ~6 engineering weeks for a full perf pass, allocated across the 18-month dev runway.

### 3.8 Non-tools build dead-strip — RESOLVED 2026-05-10

Per memory: `feedback_msvc_static_init_dead_strip.md` documents that `ZENITH_REGISTER_COMPONENT` doesn't fire if the `.obj` is unreferenced. This bit DP because component registration (e.g., `Zenith_AIAgentComponent`) required runtime force-add — already worked around in `Priest_Behaviour::OnAwake`.

**Status (2026-05-10):** Memory note `project_nontools_build_broken.md` records that `vs2022_Release_Win64_False` configurations now build and DP tests run in them. The remaining work is **verification** (rerun the full test batch in non-tools mode and confirm green), not **recovery**. Bridge cost reduced to ~3 engineering days (one CI pipeline addition + a green-run confirmation).

**Action item:** Verify in MVP-0.0.7 (the smoke PR) that the CI workflow includes a non-tools-build sanity check.

### 3.9 No console-specific platform layer for DP

The project supports Android (per Sharpmake configs) but the GDD targets Switch 2 / Series X|S / PS5. Each requires a platform layer in `Zenith_<Platform>` (akin to `Zenith_Windows_*`). Game-specific work is minor; engine-layer work is significant and outside the DP project's scope.

**Bridge cost:** ~6 months engine-team work across all three platforms. Out of scope for this analysis but on the critical path.

### 3.10 Procgen `ValidateSolvability` warns instead of reject + retry

`DPProcLevel_Generator.cpp::ValidateSolvability` emits a warning when a generated layout is unsolvable but lets `Generate` return the layout anyway. Per the 2026-05-22 8-personality matrix, **seed 0** produces such a layout -- the pentagram is behind multiple locked-door corridors, the bot only forges 1 key per run, and no personality can deliver any objectives. Excluded from the canonical 10-seed test set.

**Seed 1 history (2026-05-26):** the doors-at-DoorPoints PR initially made seed 1 unwinnable (1 placement per cell across all 8 personalities; 0 wins). Diagnosed and fixed in three layered changes, none of which were the "reject-and-retry" deferred solution:

1. **Option B (forge near pent):** procgen's `PlaceGameElements_I` now picks the forge in the spawn-side room minimising distance to the pentagram, instead of the 2nd-nearest-to-spawn room. Shrank seed 1's bootstrap chain from 134 m → 86 m.
2. **`DPDoor::IsPentagramInRange` deference:** F-press at the pentagram was ALSO toggling the adjacent pent-side door (door's logical centre is ~1.5 m from a typical bot pent-approach point, inside the 2 m InteractRadius). Telemetry showed `ObjectivePlaced` and `DoorClosed` firing in the same frame; subsequent villagers wasted a life-timer re-opening the door before each delivery. DPDoor's Open→Closing transition now defers if a pentagram is in F-range of the same villager. Order-independent against the pentagram/door subscription order.
3. **Bot opportunistic-delivery pivot:** if a villager auto-picks-up an Objective during `kHP_WalkChest` or `kHP_WalkNoise` (auto-proximity at 1.5 m grabs anything we walk near), divert to `kHP_ObjLoopWalkPentagram` immediately instead of continuing the bootstrap side-trips.

Seed 1 now wins on 6 / 8 personalities (only Speedrunner + Relay still <3 placements). Seed 0 remains the only unwinnable seed in the canonical set (its problem is layout reachability, not cost).

**Remaining fix (still deferred for seed 0):** make `ValidateSolvability` reject + retry with the next seed-rotation when reachability fails (the generator already supports re-seeding internally). Seed-cost gates aren't needed any more given the v33 bot-side + door-side fixes above.

**Bridge cost:** ~1 engineering day (procgen change + add a test that confirms `Generate` retries on a known-bad seed and produces a different solvable layout).

### 3.11 Bot pathfinding duplicates the engine navmesh

`Test_PersonalityPlaythrough.cpp` runs its own 240×240 grid A* (~1000 lines: `BuildPathGrid` + downward-raycast walkability classifier + custom A* + stuck recovery + waypoint follower). The priest uses `Zenith_Pathfinding::FindPath` over `Zenith_NavMesh`. Two parallel pathfinding systems traversing the same world is the architectural smell that surfaced the **door collider physics bug** (fixed PR #141): the bot ignored closed doors because its raycast threshold missed the short door mesh.

A 2026-05-22 refactor attempt to replace the bot's grid with `Zenith_Pathfinding::FindPath` calls (so bot + priest share a single source of truth) found that `FindPath` returns FAILED for ~99% of bot queries on procgen layouts, even with explicit `FindNearestPolygon` polygon-centre snap. Suggests an engine-side issue in `Zenith_NavMeshGenerator` polygon coverage or `FindPolygonContaining` thresholds -- the navmesh has 118k polygons on the procgen output but doesn't reliably contain the bot's query endpoints.

**Fix:** engine-side debugging of `Zenith_NavMeshGenerator` polygon generation on DP's procgen scene + `FindPolygonContaining`'s vertical/horizontal tolerances. After that, replace the bot's grid with `Zenith_Pathfinding::FindPath` to consolidate the two systems.

**Bridge cost:** ~1-2 engineering weeks (engine navmesh investigation + bot refactor + verifying the matrix still produces the same balance outcomes).

---

## 4. Content volume gaps

The GDD describes a world. The prototype contains a fraction of that world.

| Content category | GDD target | Prototype state | Gap |
|---|---|---|---|
| Map | 1 × hand-authored 200×200 m, 6 districts | 1 × 100×100 m, undistricted | 4× area, 6 districts to author. |
| Villager archetypes | 24 designed, 14–18 per Night | 1 type, 14 instances | 23 designs + asset work. |
| Reagents | 14 unique, 5 per Night | 5 interchangeable | 9 new designs + asset work. |
| Interactables (props) | ~12 unique kinds + variants | 6 (door, double door, chest, forge, pentagram, noise machine) | 6+ new kinds. |
| Aelfric variants | 3 | 1 | 2 new BT configurations. |
| Reagent pickup variants | Per-reagent (e.g., bog-water evaporates) | Single behaviour | 14 special behaviours. |
| Particle systems | ~25 | 1 (witch placeholder) | 24 to author. |
| Distinct SFX | ~80 (per interactable + Aelfric VO + ambient) | 0 | 80. |
| Music | 2 dynamic tracks + interstitial bed | 0 | All. |
| Cutscenes | ~14 (2 per Night) | 0 | All. |
| Tutorial moments | 6 in Night 1 | 0 | All. |
| Localisation | 8 languages × ~5000 words | 0 | ~40k words to translate + LQA. |
| Menus | Main, Liminal, Pause, Settings, Daily, Hauntings | Main (1 button), Pause (overlay text) | 4 net-new menus. |

The honest summary: **the prototype contains the engineering pattern for each of these categories, but ~3% of the content volume.**

---

## 5. QA & production gaps

### 5.1 Automated test coverage is strong — for what's there

The 133-registration harness (`Tests/`, 113 .cpp files as of 2026-05-27; originally 34) is genuinely impressive for a skeleton-grade port. Tests cover: scene loading, possession round-trip, life timer, pickup, victory, perception bridge, pursuit, fog-pass contract, material side-table, gym scenes for each subsystem, plus the Phase 5 additions (telemetry round-trip, edge cases, DP event hooks, analyzer verdict criteria, bot pathing + goal dispatch, HUD formatters, full playthrough integration). The harness supports batch mode for fast iteration + per-test `durationMs` reporting.

This is the project's largest **strength** carried over from the M0/M0.5/M1 milestones, and shipping should preserve it. The pattern (gym scenes per subsystem + integration tests for the full game level + pure formatters / dispatch tables under TestSurface namespaces) generalises directly.

**Risk:** As content scales, gym scenes will lag. Discipline needed to keep them in sync.

### 5.2 Human-driven QA — telemetry stub shipped, playtest pipeline still pending

Automated tests catch regressions in implemented mechanics; they don't catch design issues, balance issues, or "the priest feels unfair" issues.

**Telemetry stub:** ✅ shipped 2026-05-15..16 (commits `1bb46802` through `3cb99e84`). `Zenith/Telemetry/Zenith_Telemetry` is an engine-side binary recorder + JSON exporter; `Games/DevilsPlayground/Source/DPTelemetry.h` defines DP-specific event types + a RAII `Hooks` helper that routes 9 DP events into the recorder; `DPHeuristicBot` drives `Zenith_InputSimulator` for unattended playthroughs; `DPTelemetryAnalyzer` reads a recording + applies pass/fail criteria; `Tools/dp_telemetry_runner.ps1` orchestrates a bot run + collects artifacts. Captures all player + entity movement + game events; reads back into a usable verdict. The pieces are in place; the "last 10 seconds" replay-debug UI is not yet wired (engine `--telemetry-runner` flag could surface that).

**Still pending:**
- Replay-debug UI surfacing the last-N-seconds buffer per GDD §13.
- Playtest pipeline -- still a producer task; ~$50k/year budget for external playtester sessions.
- Procgen Phase A (16 baked seed levels) + multi-seed verification using the existing telemetry / analyzer infrastructure.

### 5.3 No CI for `.zscen` re-baking

See §3.4. Without this, content drift between authoring code and baked assets is a constant low-grade risk.

### 5.4 No certification pre-flight for consoles

The console certification checklists (TCR for Xbox, TRC for Switch, TRC for PS) impose hundreds of requirements (controller-disconnect behaviour, suspend/resume, friend-list integration, achievement plumbing, save-data corruption recovery). None of this is present in DP today. **Bridge: 3 months of pre-cert work in months 14–18 of the dev runway.**

---

## 6. Design risks the prototype has already surfaced

These aren't gaps so much as **warning lights** the prototype has illuminated:

### 6.1 The 30-second timer might be wrong

The prototype's tests had to bump villager move speed from 4 m/s to 8 m/s because the `HumanPlaythrough_Test` couldn't finish within budget at 4 m/s. The implication: at the original tuning, just **traversing the map within one possession** was hard. At 8 m/s it's easier but feels less weighty.

**Risk:** the 30-second timer is the game's defining parameter. It needs ~50 hours of playtesting to land. The prototype suggests it may need to be context-modulated (longer in the chapel, shorter on the moor, etc.). Plan accordingly in design budget.

### 6.2 Priest's "knows who is possessed" fallback

**Status:** ✅ RESOLVED 2026-05-19 (PR #115).

The omniscient-priest fallback in `Priest_Behaviour::BridgePerceptionToBlackboard` is gone. Tests that need the priest to acquire a target now call `Zenith_PerceptionSystem::RegisterTarget(villager, /*hostile=*/true)` and teleport the villager into the priest's sight cone -- same code path production uses. Removed alongside the omniscient-fallback's `DP_Player::SetTestOmniscientFallback` toggle (which was a test backdoor that leaked into every build because `ZENITH_INPUT_SIMULATOR` is defined unconditionally).

### 6.3 Held-item visual is a floating cube above the body

A small bug-shaped tell: the prototype's held-item visual hovers 1.6 m above the villager rather than being held in their hand. When the villager mesh + animation system arrives, this needs to migrate to a bone-attached socket. Currently the position is set every frame in `DPVillager_Behaviour::PositionHeldItemVisual`.

**Cost:** ~1 engineering day, but worth flagging now so the animation team plans for it.

### 6.4 Villager count — procgen-variable as of 2026-05-19

**Status:** ✅ obsolete after procgen migration. Villager count is no longer a fixed-by-hand number; the procgen generator places villagers per-room with a configurable density (currently ~12-18 per layout depending on room count). Each villager is assigned to one of the 4 MVP archetypes (Farmhand / Beggar / Devout / Child) at spawn time.

_Historical:_ The pre-procgen hand-authored GameLevel spawned 17 villagers. The CLAUDE.md docs once said "14" in one section + "17" in another; the inconsistency was a stale value from earlier port milestones. The procgen migration retired this concern entirely.

### 6.4.1 Updated `AgentBriefing.md` claim about non-tools build

The previous claim "non-tools builds are broken" in `AgentBriefing.md` §4.9 is **out of date as of 2026-05-10**. The pitfall list now reads "*Verify* non-tools builds before claiming green — they were fixed in May 2026 but easy to regress."

### 6.5 No item drop on possession switch

When the player switches possession, the old body still holds its item. The GDD assumes drop-on-the-ground is a verb (G key). Currently held items follow the villager forever, including after death. This is the **biggest single design surface missing** from hand-off chains.

---

## 7. Bridge plan — phased

A suggested phasing for the 18-month runway, prioritising shippability:

### Phase 1 — Foundation (months 0–4)

- Real navmesh generation (§1.2).
- Apprehend / run-lose state (§1.1).
- Real pause (§1.3).
- Voluntary possession switch + drop verb (§6.5, §2.10).
- Possession cooldown & demon-scent (§2.1).
- `.zscen` re-bake CI (§3.4, §5.3).
- Remove priest's omniscience fallback (§6.2).
- Save/load scaffolding (§3.5).

**Output:** A 5-minute demo that demonstrates the *core loop* with real stakes. Not pretty. Not deep. But losable.

### Phase 2 — Depth (months 4–10)

- Villager archetypes (5 to start, 12 by end of phase) (§2.1).
- Witch-finder hounds + variants + escalation (§2.2).
- Reagent uniqueness (§2.5).
- Crafting expansion (§2.6).
- Charms & distractions (§2.7).
- Liminal hub + meta-progression for one track (§2.3).
- Animation + character art baseline for 5 villagers + Aelfric (§1.4).
- Volumetric fog shader (§3.1).
- Music dev start (§3.3).

**Output:** A 30-minute vertical slice playable by external partners. Demonstrably *the game*, just not full content.

### Phase 3 — Content (months 10–14)

- All 24 villager archetypes (§4 content).
- All 14 reagents (§4 content).
- Vexholme map full polish (§4 content).
- Procedural shufflers (§2.4).
- Particles (§3.2).
- SFX bible (§3.3).
- Localisation pipeline (§3.6) + first 3 languages.

**Output:** Content-complete build. All systems shipped, all archetypes implemented.

### Phase 4 — Ship (months 14–18)

- Cutscenes / narrative (§2.8).
- Tutorialisation (§7.3 in GDD).
- Console platform layers (§3.9) — done in parallel by engine team months 8–18.
- Console certification (§5.4).
- Localisation finalisation.
- Performance optimisation pass (§3.7).
- Beta + playtest cycle.
- Day-1 patch backlog.

**Output:** Ship. Day-1 build for all four platforms.

---

## 8. What we should keep

Not all of the prototype is a list of things to fix. The following are genuine **assets** that the shipping team should preserve:

1. **The `Project_*` lifecycle hooks** (9 entry points, mirroring Combat). Clean architecture. Don't deviate.
2. **The `DP_*` namespace public-interface contract** (Source/PublicInterfaces.h/cpp). The discipline of behaviours-talk-only-via-namespace is preserved-worthy.
3. **The automated test harness** (133 registered tests across 113 .cpp files as of 2026-05-27, batch mode, gym scenes, per-test wall-clock timing). One of the strongest QA stories of any in-development game I've seen at this stage. Build on it; don't replace it.
4. **The editor-automation scene authoring pattern.** Declarative `AddStep_*` calls reading like a recipe. Bake into `.zscen` for ship, but keep the authoring DSL for live iteration.
5. **The perception-system bridge pattern** in `Priest_Behaviour::BridgePerceptionToBlackboard`. The right shape for the GDD's Aelfric variants — just needs the omniscience-fallback removed.
6. **The fog-hole clear-and-rebuild strategy.** Simple, deterministic, no subscription overhead. Keep.
7. **The material tinting system** for debug visualisation. The shipping prop set will replace it visually, but keep the tinting code path as a debug-only build-flag mode for tooling.
8. **The source-bug guards.** `RemoveHeldItem` null-deref fix, `FindItemByTag` miss-return, `OnExitRange` event-handle correctness. Each is the kind of subtle correctness work that pays back for the entire dev cycle. Preserve.

---

## 9. Summary

**The prototype is good at being a prototype.** It demonstrates that the core loop *can* be built on the Zenith engine. The system architecture, namespace contracts, test harness, and authoring pattern are shipping-quality patterns at a non-shipping content level.

**The prototype is not good at being a game.** Every single one of the game's load-bearing dramatic moments — the priest catching you, the body burning out at the wrong moment, the chapel bell ringing for the first time, the hand-off chain succeeding by 2 seconds — is currently a placeholder, a no-op, or a thing you'd have to imagine while looking at debug capsules and tinted cubes.

The work between here and ship is real, sized, and tractable: **roughly 18 months and a team of 12**, with the majority of effort going into content, animation, audio, and the antagonist's full character — *not* into rearchitecting what's there. The bones are real. The flesh is what's missing.
