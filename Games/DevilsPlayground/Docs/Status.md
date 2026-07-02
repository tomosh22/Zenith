# DP Status

**Last updated:** 2026-07-01 — **six-gap "toward AAA" engineering pass** (fog memory GPU fix, minimap, priest stuck-in-buildings fix, third-person camera mode, metagame v1, + an engine `Zenith_HashMap` churn fix that un-broke the batch test runner). Details in the "What's new since 2026-05-27" section below. Previous anchor: 2026-05-27 — personality unification (`0813cff6`, no buffs/nerfs across the 8 bot personalities), noise-machine 20m → 19m (`2987034a`), procgen door-yaw + navmesh-OBB fixes (`f8e21f78`), **doors-at-DoorPoints + matrix balance pass** (`00bb2382`, 2026-05-26 — two doors per corridor, sticky unlock, iron auto-scales, priest opens doors, bot bootstrap mandatory, 80 % matrix wins), and the DP convention cleanup + DP_Win_Test fix (`22343d69`). Engine-wide `*Impl` removal + `g_xEngine.X()` facade collapse landed 2026-05-27 (`9e4253ca`) — no gameplay impact, but reshapes engine-subsystem call sites; the canonical thread-assert spelling is now `g_xEngine.Threading().IsMainThread()`. Cosmetic: priest mesh tint is red (`57443e53`). Previous game-balance pass (PR #141, 2026-05-22), procgen migration (PRs #96-#117, 2026-05-19), telemetry v3 + seed-matrix tooling (#120/121, 2026-05-20), and priest-navmesh overhaul (#123/125) remain the prior anchors.

**Build:** ✅ DP target builds clean (`vs2022_Debug_Win64_True` 0 warnings 0 errors; `Debug_False` + `Release_False` configs also building cleanly since 2026-05-10).
**Tests:** **133 `ZENITH_AUTOMATED_TEST_REGISTER` invocations across 113 .cpp files** at HEAD (verified 2026-05-27 via `grep -c ZENITH_AUTOMATED_TEST_REGISTER Games/DevilsPlayground/Tests/*.cpp`). Some subset is `#ifdef ZENITH_INPUT_SIMULATOR`-gated. Headless skips graphics-only tests by `m_bRequiresGraphics`; per-test wall-clock timing reported in JSON (`durationMs`) + slowest-10 surfaced after every batch run. **Note (2026-05-27):** a local headless run at HEAD wrote only 25 JSON files before the engine appears to have hung mid-batch, with `PriestBBBridge_Test` and `PriestPursuit_Test` reporting `passed: false`. Treat the suite's pass-rate as "see latest green CI" until this is investigated. Likely tied to the uncommitted `Zenith/Flux/RenderGraph/*` working-tree changes (engine WIP at the time of this doc refresh) rather than to committed DP code.

**Balance criteria (ratified 2026-05-21, met under unified-personality model post `0813cff6`):**
1. Every personality WR ∈ (0%, 100%) ✅ — canonical 10-seed matrix at HEAD lands every personality strictly between 0 % and 100 % (range 60 – 90 %). All 8 personalities are now mechanically identical at the bot level (no buffs, no nerfs); the personality is purely a different *strategy* applied to the same toolkit.
2. Every level winnable by ≥1 personality ✅ — canonical seeds `1, 5, 7, 42, 100, 12345, 55555, 99999, 250000, 4276994270` all have multiple winners. Seed 0 excluded (procgen-unsolvable; see Shortfalls.md §3.10).

Aggregate matrix at HEAD: **80 % wins (64 / 80 cells)**. Per-personality / per-seed snapshot: `Docs/GameBalance_2026-05-22.md` covers the pre-unification state; the post-2026-05-23 unified state lives in `Docs/DecisionLog.md` (2026-05-23 entries) and the live `Build/dp_telemetry/seed_matrix/REPORT.md`.

**Operating mode:** Direct-to-master with auto-merge on green CI. Branch protection disabled per Tomos 2026-05-15 direction (CI still runs on every push via `push: branches: [master]` triggers). Worktrees in `.claude/worktrees/` are sandbox-only; main work happens at `C:/dev/Zenith` on master.

## What's new since 2026-05-27 (2026-07-01 six-gap pass)

| Area | Status | Notes |
|---|---|---|
| **Fog of war (memory)** | ✅ shipped | The verified root cause was that the per-tile memory table (`NeverSeen/VisitedVisible/VisitedDim/VisitedHidden`) was fully computed on the CPU but **never uploaded to the shader** — only instantaneous holes rendered. Now: `DP_Fog::RasterizeMemoryVisibility` bakes the table into a 256² R8 texture each frame (`DPFogPass::UpdateMemoryTexture`, staged via `UpdateTextureVRAM`), `DP_Fog.slang` samples it as a second density term (new `g_xMemoryRect` CBV field, 992→1008 B; `Generated/Fog.h` regenerated via FluxCompiler). Continuous visibility curve (`MemoryVisibilityForAge`, new `memory_dim_visibility`/`memory_hidden_fade_s` tuning) shared by fog + minimap. Tests: `Test_P2Fog_MemoryVisibilityCurve`, `Test_P2Fog_MemoryRasterizeGrid`, `Test_DPFogPass_VisualOutput` (screenshot A/B pixel diff via the new `DP_TestTGAHelpers.h` reader — the first rendered-pixel assertion in the suite). Telemetry: periodic `FogMemorySample` event + `FogMemoryAges` analyzer criterion. |
| **Minimap** | ✅ shipped (v1) | 2D vector minimap (bottom-right, 220 px): per-room `Zenith_UIRect`s from `DPProcLevel_LevelLayout`, revealed/aged through the SAME fog-memory curve as the world, gold possessed-villager icon + remembered-visible villager icons. Priest deliberately NOT drawn (threat info stays fog-gated). New `DPMinimap_Component` (order 105) + pure `DP_Minimap` namespace. v1 draws yawed rooms as AABBs (`Zenith_UIRect` has no rotation — follow-up). Tests: `Test_T0Minimap_PureMaths`, `Test_DP_Minimap_RevealAndTracking`. |
| **Priest stuck in buildings** | ✅ shipped | Root cause confirmed: `DPDoor::StitchNavMeshPortal` failures were log-only and seal a room for pathfinding regardless of door state (~9 doors/level on the default seed, mostly yawed-wall doors). Fixes: probe set widened 2×3 → 4 axes×7 distances (originals first); failures now distinguish benign already-connected no-ops from genuine seals (`DP_AI::ArePositionsConnected` BFS oracle) and count via `DP_AI::GetUnstitchedDoorCount`. Standing gate: `Test_ProcLevel_PriestReachability` — all 10 canonical seeds: 0 sealed doors + every room centre path-connected to the priest spawn. Patrol 1-in-5 sampling untouched (tuned balance knob). Suspicion-radius widening NOT needed after the stitch fix. |
| **Camera (birds-eye ↔ third-person)** | ✅ shipped | `DPOrbitCamera_Component` gains `DPCameraMode` + a 0.4 s pose blend: possession auto-engages a third-person follow (behind-the-villager pose per the RenderTest follow-cam derivation, quat*+Z forward), possession loss blends back; **C** manually toggles until the next possession change. Blend lerps position+look-at and re-solves yaw/pitch (no ±π wraparound case). At blend 0 the ORIGINAL orbit path runs byte-identically (menu/gym regression guarantee). Possession is POLLED (no this-capturing subscriptions — relocation-safe by construction). `OrbitCameraStaysFixed_Test` → `OrbitCameraFollowsPossession_Test` (old test pinned the retired never-follow contract). Tests: `Test_DPCamera_ModeTransition` + the rename. Telemetry: `CameraModeChanged` event. |
| **Metagame v1 (GDD §5.4)** | ✅ shipped (v1 scope) | `DP_MetaSave` (magic `'DPMS'`, versioned, fail-soft; persists to DISK from day one via `Zenith_SaveData` slot "meta" — `Zenith_SaveData::Initialise` now called at DP boot; see `Docs/MetaSaveFormat.md`). `DP_Knots` per-run earning (1/reagent + hand-off-chain bonus; chain breaks on villager death; banks once per run on victory OR loss). Liminal hub scene (build index 2, FrontEnd "The Liminal" button → `DP_MainMenu.bgraph` → `LoadSceneByIndex(2)`): 3×12-node shrine columns + `DPLiminalHub_Component` (order 113) spend logic (prefix-ordered nodes, linear cost curve, persisted). Live effect hooks: Breath = sprint-drain + possession-cooldown scales; Eye = fog-memory duration scale — all 1.0 on a fresh profile so the ratified balance matrix is untouched. Tests: `Test_P1MetaSave_*` ×3, `Test_T0MetaSave_DiskHooks`, `Test_T0Knots_*` ×2, `Test_DP_LiminalHub_Spend`. **Deferred:** Forge-track gameplay effects (crafting is instant — nothing to scale until a craft channel exists), +1 crafted-item inventory slot (single-slot held-item model), stoneborn spawn gating, seer priest-ping, run-results screen on the in-run HUD banner (the Liminal shows the last-run Knot readout instead), hound Knots (no hounds; stub keeps the schema stable). |
| **Batch test runner (engine)** | ✅ fixed | The 2026-05-27 note below ("batch run wrote only 25 JSONs then hung") was a `Zenith_HashMap` growth bug: insert/remove churn counted tombstones toward the load factor and ALWAYS doubled capacity, so the FULL-tier memory tracker's allocation map hit the 2^25×216 B capacity-overflow assert during procgen scene reloads. Fixed in `Zenith_HashMap::EnsureCapacityForInsert` (rehash in place unless live entries need growth) + `TestHashMapChurnCapacityBounded` unit test. Reproduced at clean HEAD before the fix (NOT caused by this pass's changes); batch mode completes again. |

**Post-pass gates (2026-07-01/02):** full batch suite **155 / 155 passed** (batch mode restored by the HashMap fix); `Vulkan_` Debug_True + Debug_False + Release_False all build 0-error; 10-seed × 8-personality matrix re-run on Release_False: **both ratified balance criteria still met** (win-rate range 50–90 %, every seed winnable; aggregate 54/80 = 67.5 % vs the pre-fix 80 % — expected direction, the priest can now actually enter every building). Note: `Tools/dp_seed_matrix_run.ps1`'s exe paths were stale since the `Vulkan_` config-prefix rename (every cell 0xC0000005 against a pre-rename exe) — fixed in the same pass.

**Adversarial review round (2026-07-02):** a 23-agent review of the full diff confirmed 10 findings; ALL were fixed in-pass: **(critical)** the run-end Knot-banking subscriptions were wiped by the boot-time engine unit tests' `ClearAllSubscriptions` calls in any launch without `--skip-unit-tests` — `DP_Knots::Initialise` is now handle-tracked re-entrant and re-subscribes from the bootstrap at every run start (proven end-to-end by a win-golden playthrough run WITHOUT `--skip-unit-tests` banking successfully; note the same boot wipe still affects the pre-existing DP_Tutorial/DP_Particles boot subscriptions — pre-existing, tracked as follow-up); **(major ×2)** test/matrix runs could overwrite + delete the player's real `%APPDATA%` meta save — all meta-save read/write/delete now routes through `DP_MetaSave::SlotName()`, which redirects automated-test runs to a throwaway `meta_test` slot; **(minors)** stale fog memory-window after scene unload (reset from the fog component's OnDestroy), mid-blend camera yaw whirl at the degenerate horizontal pinch (hold-last-yaw guard), `Test_ProcLevel_PriestReachability` could pass vacuously on the synthetic-quad navmesh fallback (new Gate 0 asserts a real navmesh), personality telemetry sampled the orbit-derived eye instead of the real camera (now reads the live component), minimap room sampling missed perimeter walks (5→9 probes), minimap reveal test could flake on villager/probe coincidence (now deterministic), and a PRE-EXISTING `Zenith_HashMap::CopyFromOther` bug where dropping tombstones at fixed slots severed probe chains, making live keys unreachable in copies (now re-inserts; `TestHashMapCopyPreservesProbeChains` pins it).

Follow-ups queued from this pass: minimap rotated-room rects; third-person camera wall-clipping avoidance; Forge-track effects once crafting has a duration; economy tuning of the Knot cost curve after playtests; make Zenith_EditorAutomation copy its string args (see the LIFETIME GOTCHA in AuthorLiminalScene); FrontEnd orbit-camera usage confirmation was folded into the never-possessed camera tests.

## What's new since 2026-05-22

### Personality unification + doors-at-DoorPoints (2026-05-23 → 2026-05-26)

Five layered changes converged the 80%-matrix-wins state at HEAD. Full design rationale in `Docs/DecisionLog.md`; this is the operational summary:

| Date / commit | Subsystem | Notes |
|---|---|---|
| 2026-05-23 `0813cff6` | Bot personalities | Removed `PersonalityConfig::bSkipBootstrap` + the F-mash interact toggle from PR #128's Zealot. All 8 personalities now run the same bot mechanics; personalities are pure *strategies*, not buffs/nerfs. Per user feedback: "Personalities must not be buffs or nerfs and must not give the bot any extra abilities. The personalities are supposed to purely simulate how different human beings might play the exact same game." |
| 2026-05-23 `2987034a` | Tuning | Noise-machine `radius_m`: 20 → 19. The personality unification made Heretic (deliberate-noise-first) 10/10 wins, violating criterion 1; bisecting found 19 m is the threshold where Heretic drops to 9/10 *and* seed 250000 stays winnable (by Casual / Stealth / Zealot via the noise-machine's bootstrap-chain use, not Heretic's deliberate-first use). 12 / 15 / 17 m all rendered seed 250000 unwinnable. |
| 2026-05-23 `f8e21f78` | Procgen | Door yaw now derives from the wall it sits on, not from the corridor delta. Plus a navmesh OBB fix in the same series. |
| 2026-05-25 `b753aea1` | DP state | Static state refactoring — file-scope statics migrated onto components (possession / win / night → `DPPlayerController`); JSON-parser internals consolidated into `Source/DP_Json`. Phase 5.2 / Phase 2a-2b of the cleanup audit; Phase 4 of the audit (`e7132e96`) was skipped because the measure-first gate failed. |
| 2026-05-26 `00bb2382` | Doors-at-DoorPoints | **The big one.** Two wall-aligned doors per corridor (one per DoorPoint, integer-derived yaw); brown debug markers gone; navmesh portals stitched at the geometric door centre; corner-anchored door collider becomes a sensor when not Closed so the swinging arm doesn't shove the player capsule; priest opens any closed unlocked door within 4 m; priest spawn shifted 1.5 m inside the room from an unlocked doorpoint so `OpenNearbyDoorsFor` catches the door from frame 1; procgen `PickPriestRoom` enforces ≥1 corridor; bot bootstrap mandatory for all 8 personalities. **80 % matrix wins (64 / 80 cells)** post-PR, up from the pre-PR 60 % baseline. Seed-1 specific fixes (forge near pent + `DPDoor::IsPentagramInRange` deference + bot opportunistic-delivery pivot) layered on top — see [Shortfalls.md §3.10](Shortfalls.md). |
| 2026-05-26 `22343d69` | DP cleanup | Audit-driven convention cleanup; `DP_Win_Test` fix. |
| 2026-05-27 `9e4253ca` | Engine-wide | `*Impl` suffix dropped + static/namespace facades collapsed onto `g_xEngine.X()`. DP unaffected (already accessed engine subsystems via `g_xEngine`); doc-side: `Zenith_Multithreading::IsMainThread()` → `g_xEngine.Threading().IsMainThread()`. |
| 2026-05-27 `57443e53` | DP cosmetic | Priest mesh debug tint: red. Pre-Mixamo stand-in; not a design pivot. |

### What's new since 2026-05-16 (kept for the prior snapshot's framing)

The last Status snapshot covered Phase 1 + Phase 2 + most of Phase 4 landing. Major work since:

### Procgen migration (PRs #96 → #117, 2026-05-17 → 19)

The hand-authored `GameLevel.zscen` + the 4 gym scenes + the `Tools/dp_export/` UE-bridge pipeline have been **fully removed**. The single gameplay surface is now `ProcLevel`, built at runtime by `DPProcLevel::Generate(seed, cfg)`.

| PR | Subsystem | Notes |
|---|---|---|
| #96 | BSP layout generator + offline visualiser | The initial procgen scaffolding -- BSP partitioning into rectangular rooms, plus a Python visualiser for offline iteration. |
| #97 | Rectangular (non-square) rooms via aspect-ratio sampling | First substantive layout-variation pass. |
| #98 | Wall segment emission with door gaps | Walls carved with door cuts; doors emit as wall edges with a marker. |
| #99 | Game-element placement + solvability check | Forge / door / chest / noise / pentagram / iron / objectives placed via a constraint satisfier; BFS solvability check rejects unreachable layouts. |
| #100 | Pentagram corridor gating + offset for stacked elements | Each pentagram approach goes through at least one keyed door. |
| #101 | AI placement (villagers + priest + patrol nodes) | Villagers spawned per-room with archetype assignment; priest patrol nodes traverse the layout. |
| #102 | Outdoor villager + objective placement | Villagers spawn in walk-able outdoor space between buildings, not jammed against walls. |
| #103-107 | Bootstrap behaviour + spawn pipeline | `DPProcLevelBootstrap_Behaviour` orchestrates entity spawning from the `LevelLayout` struct. |
| #109 | Procgen floor + walls at full size | Fixed half-size visual bug. |
| #110 | Camera auto-frames to level bounds | Orbit camera auto-positions on scene load. |
| #111 | Procgen characters render at correct size | Fixed invisibly-tiny villager bug. |
| #112 | Procgen characters not tumbling + bot/priest not walking through walls | Physics setup pass for procgen-spawned entities. |
| #114 | Procgen characters not rotating + HUD not stuck at "He sees you" | State-machine bugs from procgen spawn timing. |
| #115 | Removed omniscient-priest test fallback | The priest's "fall back to `DP_Player::GetPossessedVillager()` when no perception target qualifies" hack is gone. Tests that need priest acquisition now seed perception properly. |
| #116 | Procgen generator rewritten with integer-coord internals | Bit-deterministic across `/fp:fast` Debug + Release builds (verified by `Test_ProcLevel_DeterminismCheck`). Float values only appear at the LevelLayout output boundary; the conversion is pinned to `c * 0.001f` so the optimiser can't substitute a divide. |
| #117 | Remove UE-exported GameLevel + gym scenes + dp_export | Tombstone for the UE5 bridge pipeline. ProcLevel is now the only gameplay surface. |

The remaining test entries for `Test_GameLevelScene.cpp` (referenced in older docs) were removed in #117; `Test_ProcLevelScene.cpp` is the replacement.

### Telemetry v3 + seed-matrix tooling (PRs #120 / #121, 2026-05-19 → 20)

| PR | Subsystem | Notes |
|---|---|---|
| #120 | Telemetry v3 | Adds AI intent (priest BT branch), life timer per villager, held-item tag, camera state, per-frame perf ms. 12 new event types (Apprehend channel start / complete / interrupt with reason enum, PerceptionContactBegin / End, etc). Recorder magic bumped; reader handles v2 streams too. |
| #121 | Seed-matrix runner + analyser | `Tools/dp_seed_matrix_run.ps1` cycles N seeds × 4 personalities, writes per-(seed, personality) telemetry artifacts + PNG overlays. `Tools/dp_seed_matrix_analyse.py` aggregates into `REPORT.md`. `DP_PROCGEN_SEED` env var exports per-cell. |

### Personality matrix balance + bot fixes (PRs #122 → #128, 2026-05-19 → 20)

The 10-seed × 4-personality matrix exposed several issues that landed as follow-up PRs:

| PR | Issue | Fix |
|---|---|---|
| #122 | Apprehend channel pursue-during-channel + repossess cap edge cases + Stealth-noise doc clarification | Priest now pursues during channel (was planted); repossess cap raised; Methodical personality dropped (it was duplicative of Casual on every metric); doc clarified that Stealth's walk-quiet only halves footstep loudness, not fixed-loudness forge / chest / door noises. |
| #123 | NavMeshAgent steering would write Y from the path | XZ-only steering; SetDestination doesn't reset velocity. |
| #125 | NavMeshAgent transform-write race vs Jolt physics | NavMeshAgent now drives motion via `Zenith_Physics::SetLinearVelocity` when the agent has a dynamic body; falls back to transform writes for transform-only test fixtures. |
| #126 | Personality matrix: 0/40 wins, sprint personalities dying 2x more, bots getting stuck in procgen doorways | Sprint life-cost 3.0 → 1.5 (`movement.sprint_life_cost_extra_per_s`); path grid 120x120 (1.0 m) → 240x240 (0.5 m); side-step recovery (90° rotate for 0.5 s) on stuck-detect. |
| #127 | Cells ending at 85-90 s without using their 141.6 s budget; per-objective F-press wasted when villager dies mid-walk holding the item | Cross-possession memory: rewind to `ObjLoopFind` when current villager isn't holding expected tag; re-aim at closest item to current villager (2 m hysteresis). Lifted phase-retry caps: `kRepossessAttemptCap` 240 → 1200, `kMaxObjAttempts` 4 → 16, removed walk-budget truncation on stuck-replans. |
| #128 | Berserker personality was statistically identical to Speedrunner -- only the F-mash Interact-count differed, with no gameplay-outcome impact | Removed all bMashInteract / kBerserkerMashFrames code. Replaced Berserker with **Zealot**: skips the entire iron / forge / door / chest / noise bootstrap chain and runs straight to the objective-deliver loop. Time-to-first-objective drops from ~50 s (Speedrunner) to ~9 s median. |

**Matrix results across the four iterations (10 seeds × 4 personalities = 40 cells, vs2022_Debug_Win64_False):**

|                          | Before #126 | After #126 | After #127 | After #128 |
|--------------------------|------------:|-----------:|-----------:|-----------:|
| Objectives delivered     |          25 |         43 |         84 |         77 |
| Wins (≥5 objectives)     |        0/40 |       0/40 |       2/40 |       3/40 |
| Speedrunner deaths       |         130 |         69 |        101 |        102 |
| Time-to-first-obj (med, Zealot) |    n/a |        n/a |        n/a |       9 s |

The personality matrix is now a meaningful design-tuning instrument — the gap between "Casual + Speedrunner achieve some wins" and "Stealth + Zealot get 0 wins" exposes the bootstrap-chain-vs-time-budget tradeoff that the procgen layout difficulty modulates.

## Operating mode

**Direct-to-master with auto-merge on green CI.** Branch protection disabled per Tomos 2026-05-15. CI still runs on every push via the workflows:

- `dp-build` -- DP project build, `vs2022_Debug_Win64_True`
- `dp-tests` -- `Tools/run_dp_tests.ps1 -Headless`
- `complexity-gate` -- automated code-complexity gate
- `doc-lint` -- Markdown link / claim consistency checks

`gh pr create --title ... --body ...` then `gh pr checks <n> --watch` to wait for green, then `gh pr merge <n> --squash --admin`.

Worktrees in `.claude/worktrees/<name>/` are transient sandboxes for the harness; Sharpmake bakes the cwd absolute path into vcxprojs so committing them from a worktree breaks every other checkout. Always work at `C:\dev\Zenith\` for shared state and treat worktree placements as ephemeral.

## What's still open

### MVP gate

| Item | Status | Notes |
|---|---|---|
| **MVP-4.3.4** Tomos plays demo end-to-end | 🚧 HUMAN_GATE | The only MVP task left. The Phase-5 instructional HUD work makes this more productive when Tomos sits down. |

### Personality matrix surfaces these as next-up

| Question | Why it matters |
|---|---|
| Why does Zealot deliver fewer total objectives than Speedrunner despite saving ~30 s of bootstrap? | Counter-intuitive matrix result. Hypothesis: bootstrap centralises the bot's position (forge / door / chest are typically map-centre), Speedrunner's poss-2+ start from a more productive position. Could also be Speedrunner unlocking doors that gate later objectives (DoorOpened=3 vs Zealot's 0). |
| `reagents_required_for_victory` 5 → 3 for MVP playtest? | At 5, even the best bot gets 5/5 in only 2-3 of 40 cells. 3 would put ~18/40 cells over the win bar. Tomos-decision: keep 5 for shipping, but consider 3 for MVP playtest builds. |
| Procgen-distance constraint: cap iron-spawner ↔ pentagram distance? | Seeds 12345 + 55555 produce 0 objectives across all personalities — the bot can't complete the loop in 30 s. A 60 m total-distance cap on the procgen layout would make every seed solvable. |

### Phase 3 (assets) — mostly unblocked but unstarted

- **MVP-3.0.1** Mixamo Y-Bot spike — 🚧 HUMAN_GATE (Mixamo login). Once Tomos drops an FBX into `Vendor/Mixamo/`, the agent can run the spike.
- All other Phase 3 tasks depend on 3.0.1.

### Engine / tooling improvements

- **P1 timer-cluster amortisation** — 9 tests in the P1 family burn 5-6 s each waiting on real game timers. Migrating to accelerated test-only timers would drop the suite from ~120 s to ~50 s. Tracked as a "want, not need" until the suite gets slow enough to hurt iteration.
- **Replay-debug UI** — the telemetry recorder captures everything but there's no in-engine UI to scrub through a recording. Per GDD §13.
- **Long-form playthrough analysis** — the `Tools/dp_seed_matrix_analyse.py` script handles aggregate matrix runs; a `--single-cell <ztlm>` mode for deep-diving one cell would be useful.

## Suite snapshot

Historical baseline (2026-05-20, master @ `30c775f5`):

```
[run_dp_tests] Summary: 117 passed, 0 failed
[run_dp_tests] Timing: 117 tests measured, total = ~120,000 ms, avg = ~1,025 ms

Slowest 10 tests:
   ~25,000 ms    8500 frames  PersonalityPlaythrough_Speedrunner
   ~20,000 ms    8500 frames  PersonalityPlaythrough_Casual
   ~17,000 ms    8500 frames  PersonalityPlaythrough_Stealth
   ~13,000 ms    8500 frames  PersonalityPlaythrough_Zealot
    1,400 ms      13 frames  Test_T1NavMesh_GeneratorPerfOnProcLevel
    1,320 ms     551 frames  Test_P2Reagent_BogWaterEvaporates
    1,290 ms      70 frames  Test_P1Sprint_WinsTieOverWalkQuiet
    1,270 ms      17 frames  VillagerDeath_Test
    1,160 ms      11 frames  PriestBBBridge_Test
    1,135 ms      67 frames  Test_P2Archetype_BeggarIgnoredByAelfric
```

The 4 personality tests dominated the wall-clock back then (~75 s of the ~120 s total). At HEAD the suite has 8 personalities (Casual / Stealth / Speedrunner / Zealot + Magpie / Relay / Heretic / Trickster); the wall-clock budget for personality tests is now closer to ~150 s of a ~200 s total, but this needs a fresh baseline run on a clean working tree (see the test-status caveat in the **Tests** line at the top).

## Architectural surface added since 2026-05-16

| API | PR | Notes |
|---|---|---|
| `DPProcLevel::Generate(seed, cfg, ...)` + LevelLayout struct | #96-#117 | Runtime layout generation with bit-deterministic integer-coord internals. |
| `DP_PROCGEN_SEED` env var | #121 | Per-run seed override. Used by the seed-matrix runner. |
| `Zenith_Telemetry::Recorder::SetEventTypeResolver` (v3 hook) | #120 | Lets DP supply a per-event-type name -> string resolver for JSON export. |
| `DPTelemetry::Hooks` v3 fields | #120 | AI intent / life / held item / camera / perf in EntitySnapshot; 12 new event types. |
| `Zenith_NavMeshAgent::SetDestination` no-reset semantics | #123 | Doesn't zero linear velocity on call. |
| `Zenith_NavMeshAgent` physics-path | #125 | Drives motion via SetLinearVelocity when a dynamic body is present. |
| `Test_PersonalityPlaythrough` cross-possession memory | #127 | ObjLoopWalkPentagram rewinds on item-loss; ObjLoopWalk re-targets to closest. |
| `PersonalityConfig::bSkipBootstrap` + `Personality::Zealot` | #128 | New personality skipping the iron/forge/door/chest chain. **Subsequently removed** in `0813cff6` — see entry below. |
| `PersonalityConfig::bSkipBootstrap` / `bMashInteract` REMOVED | `0813cff6` | All 8 personalities mechanically identical at the bot level (no buffs / nerfs). |
| `DPProcLevel_Generator::PickPriestRoom` ≥1-corridor constraint | `00bb2382` | Procgen picks a priest spawn room with at least one corridor; spawn shifted 1.5 m inside the room from an unlocked doorpoint so `OpenNearbyDoorsFor` catches the door from frame 1. |
| `DPDoor::IsPentagramInRange` | `00bb2382` | Door's Open → Closing transition defers if a pentagram is in F-range of the same villager. Order-independent against the pentagram/door subscription order. |
| Two-doors-per-corridor invariant | `00bb2382` | Procgen now spawns one door per `DoorPoint` (two per corridor), wall-aligned via integer-derived yaw. Door collider becomes a `Sensor` when not Closed so the swinging arm doesn't shove the player capsule. |
| `g_xEngine.<Subsystem>().<Method>(...)` canonical form | `9e4253ca` | Engine-wide collapse of static / namespace facades; old `Zenith_<Subsystem>::<Method>` spellings are gone, replaced by instance methods on the engine singleton. DP unaffected at the gameplay level. |

## Notes for the next agent

1. **Operating mode:** direct-to-master with auto-merge on green CI. Run the matrix script to validate balance changes, not just the test suite. The 8 personality tests are integration-grade — they catch class-of-bug that unit tests don't.
2. **First action:** Verify baseline. `Tools/run_dp_tests.ps1 -Headless` against `master @ HEAD`. Expect a fully-green local run + matching latest-CI run; if the local run reports failures, cross-check `Build/dp_test_results/*.json` against CI to distinguish a fresh-local issue (uncommitted working-tree changes / cache state) from a genuine regression.
3. **Hard invariants:** (a) commits keep the full suite green; (b) per-test `durationMs` mustn't regress > 20% without justification (`Tools/check_acceptance_drift.ps1`); (c) procgen output stays bit-deterministic — every shape-determining decision is integer math (the giant comment at the top of `DPProcLevel_Generator.cpp` is the contract).
4. **Personality matrix as instrument:** when changing balance, run the 10-seed matrix and check the `Build/dp_telemetry/seed_matrix/REPORT.md` before / after. The matrix has caught: sprint balance dominating negative outcomes, bots getting stuck in narrow procgen doorways, Berserker being a cosmetic variant of Speedrunner, the bootstrap chain providing non-obvious value, etc.
5. **Engine surface to be aware of:**
   - `Zenith_BTNode::m_eLastStatus` must be assigned by leaf nodes before returning.
   - `Zenith_TransformComponent::SetPosition` routes through Jolt when a body exists. STATIC bodies don't activate; use DYNAMIC for anything moved by NavMeshAgent or transform writes.
   - `Zenith_PerceptionSystem` clamps hearing range at `min(emit_radius, agent_max_range)`. For map-wide stimuli use `DP_AI::NotifyAllPriestsOfInvestigatePos` to bypass.
   - `DP_Player::ResetForNewRun()` is the canonical reset.
   - Procgen scenes don't have hand-tuned spatial relationships — tests that need specific priest/villager arrangements should spawn fresh entities, not teleport procgen-spawned ones.
   - **Engine subsystem access** post 2026-05-27 (`9e4253ca`): the canonical form is `g_xEngine.<Subsystem>().<Method>(...)`. Examples: `g_xEngine.Threading().IsMainThread()`, `g_xEngine.Physics().SetLinearVelocity(...)`. The historical `Zenith_Multithreading::IsMainThread()` / `Zenith_Physics::SetLinearVelocity(...)` free-function spellings were collapsed into the singleton facade; old references in non-DP docs (engine CLAUDE.md tree) may still use the pre-refactor names — the meaning is the same.

## Last completed

Most recent at top. Anything older than 2026-05-22 see `git log`.

- **`9e4253ca`** (merged 2026-05-27) — Engine-wide refactor: drop `*Impl` suffix + collapse static/namespace facades to `g_xEngine.X()` (Phases 1-5e).
- **`57443e53`** (merged 2026-05-27) — Priest mesh debug tint: red.
- **`22343d69`** (merged 2026-05-26) — `audit(dp)`: convention cleanup + `DP_Win_Test` fix.
- **`00bb2382`** (merged 2026-05-26) — `feat(dp)`: doors-at-DoorPoints + matrix balance pass (80 % wins, all criteria met).
- **`b753aea1`** (merged 2026-05-25) — Static state refactoring.
- **`2987034a`** (merged 2026-05-23) — `balance(dp)`: noise-machine radius 20 m → 19 m to break Heretic 100 % on canonical matrix.
- **`0813cff6`** (merged 2026-05-23) — `fix(dp,test)`: personalities must not buff/nerf the bot; unify walk-budget + retry cap.
- **`f8e21f78`** (merged 2026-05-23) — `fix(dp)`: procgen door yaw must align with the wall, not with the corridor delta.
- **Phase 1-5 series** (merged 2026-05-22) — `f7a1a035` migrate possession/win/night state onto DPPlayerController (I3), `9f9901e2` split PublicInterfaces.{h,cpp} per namespace (I2), `b6cd9145` `std::unordered_map` → `Zenith_HashMap` (C1, C3), `01cf37d5` swap DP_Json internals to `Zenith_Vector`, `456f3dc6` extract shared JSON parser into `Source/DP_Json.{h,cpp}`, `f09992d6` `Zenith_HashMap` docs / main-thread asserts / `Zenith_Vector` mesh cache.
- **PR #128** (merged 2026-05-20) — Replace Berserker with Zealot personality + remove F-mash code.
- **PR #127** (merged 2026-05-20) — Cross-possession memory + lift retry caps in PersonalityPlaythrough.
- **PR #126** (merged 2026-05-20) — Bot pathing + sprint tuning from 10-seed personality matrix.
- **PR #125** (merged 2026-05-20) — refactor: NavMeshAgent drives motion via physics + handles non-planar paths.
- **PR #123** (merged 2026-05-20) — fix(ai): NavMeshAgent XZ-only steering + don't reset velocity on SetDestination.
- **PR #122** (merged 2026-05-20) — fix(dp): apprehend pursue + repossess cap + drop Methodical + Stealth-noise doc.
- **PR #121** (merged 2026-05-20) — feat(dp): DP_PROCGEN_SEED env var + seed-matrix runner / analyser.
- **PR #120** (merged 2026-05-19) — feat(telemetry): v3 -- AI intent / life timer / held item / camera / perf / new events.
- **PR #119** (merged 2026-05-19) — chore(dp): drop ProcLevelDemo personality + fix telemetry scene label.
- **PR #118** (merged 2026-05-19) — chore(dp): drop dead PFX_Witch surface + rename navmesh perf test.
- **PR #117** (merged 2026-05-19) — refactor(dp): remove UE-exported GameLevel + gym scenes + dp_export.
- **PR #116** (merged 2026-05-19) — refactor(dp): rewrite procgen generator with integer-coord internals.
- **PR #115** (merged 2026-05-19) — refactor(dp): remove omniscient-priest test fallback, use real perception.
- **PRs #96-#114** (merged 2026-05-17 → 19) — procgen P0 through P5 + bug fixes.

For PRs older than 2026-05-15 see git log and the prior Status.md commit history.
