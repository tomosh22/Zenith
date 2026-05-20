# DP Status

**Last updated:** 2026-05-20 — procgen migration complete (PRs #96-#117), telemetry v3 + seed-matrix tooling shipped (#120/121), personality-matrix-driven balance + bot fixes (#122/126/127/128), priest navmesh integration overhaul (#123/125). Master HEAD `30c775f5`. Full suite **117 PASSED, 0 FAILED** locally via `Tools/run_dp_tests.ps1 -Headless` (~120 s headless; longest single test ~25 s — `PersonalityPlaythrough_Speedrunner`).

**Build:** ✅ DP target builds clean (`vs2022_Debug_Win64_True` 0 warnings 0 errors; `Debug_False` + `Release_False` configs also building cleanly since 2026-05-10).
**Tests:** Full local suite green. Headless skips graphics-only tests by `m_bRequiresGraphics`; all compute-only tests pass. Per-test wall-clock timing reported in JSON (`durationMs`) + slowest-10 surfaced after every batch run. 118 registered automated tests across 104 .cpp files (was 122 at 2026-05-16; consolidations during the procgen migration and the Berserker → Zealot personality swap net -4).

**Operating mode:** Direct-to-master with auto-merge on green CI. Branch protection disabled per Tomos 2026-05-15 direction (CI still runs on every push via `push: branches: [master]` triggers). Worktrees in `.claude/worktrees/` are sandbox-only; main work happens at `C:/dev/Zenith` on master.

## What's new since 2026-05-16

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

The 4 personality tests dominate the wall-clock (~75 s of the ~120 s total). They're full procgen playthroughs running 8500 game-frames each (the 141.6 s game-time budget).

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
| `PersonalityConfig::bSkipBootstrap` + `Personality::Zealot` | #128 | New personality skipping the iron/forge/door/chest chain. |

## Notes for the next agent

1. **Operating mode:** direct-to-master with auto-merge on green CI. Run the matrix script to validate balance changes, not just the test suite. The 4 personality tests are integration-grade — they catch class-of-bug that unit tests don't.
2. **First action:** Verify baseline. `Tools/run_dp_tests.ps1 -Headless` on `master @ 30c775f5`. Expect **117 PASSED / 0 FAILED**.
3. **Hard invariants:** (a) commits keep the full suite green; (b) per-test `durationMs` mustn't regress > 20% without justification (`Tools/check_acceptance_drift.ps1`); (c) procgen output stays bit-deterministic — every shape-determining decision is integer math (the giant comment at the top of `DPProcLevel_Generator.cpp` is the contract).
4. **Personality matrix as instrument:** when changing balance, run the 10-seed matrix and check the `Build/dp_telemetry/seed_matrix/REPORT.md` before / after. The matrix has caught: sprint balance dominating negative outcomes, bots getting stuck in narrow procgen doorways, Berserker being a cosmetic variant of Speedrunner, the bootstrap chain providing non-obvious value, etc.
5. **Engine surface to be aware of:**
   - `Zenith_BTNode::m_eLastStatus` must be assigned by leaf nodes before returning.
   - `Zenith_TransformComponent::SetPosition` routes through Jolt when a body exists. STATIC bodies don't activate; use DYNAMIC for anything moved by NavMeshAgent or transform writes.
   - `Zenith_PerceptionSystem` clamps hearing range at `min(emit_radius, agent_max_range)`. For map-wide stimuli use `DP_AI::NotifyAllPriestsOfInvestigatePos` to bypass.
   - `DP_Player::ResetForNewRun()` is the canonical reset.
   - Procgen scenes don't have hand-tuned spatial relationships — tests that need specific priest/villager arrangements should spawn fresh entities, not teleport procgen-spawned ones.

## Last completed

Most recent at top. Anything older than 2026-05-15 see `git log`.

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
