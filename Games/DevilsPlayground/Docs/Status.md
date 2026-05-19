# DP Status

**Last updated:** 2026-05-16 — Phase 1 + Phase 2 + Phase 4 substantively complete (4.3.4 is the lone HUMAN_GATE remaining); telemetry / verification system shipped (Phases 1-5 + 3b). Direct-to-master workflow active since 2026-05-15. Master HEAD `3cb99e84`. Full suite **122 PASSED, 0 FAILED** locally via `Tools/run_dp_tests.ps1 -Headless` (total ~283 s headless; longest single test 20 s).
**Build:** ✅ DP target builds clean (`vs2022_Debug_Win64_True`, 0 warnings, 0 errors).
**Tests:** Full local suite green. Headless skips graphics-only tests by m_bRequiresGraphics; all compute-only tests pass. Per-test wall-clock timing now reported in test JSON (`durationMs`) + slowest-10 surfaced after every batch run.

## Manual setup checklist gating

**⚠️ BEFORE STARTING THE FIRST AGENT SESSION,** the user (Tomos) must tick every box in [Docs/ManualSetupChecklist.md](ManualSetupChecklist.md). The orchestrator pattern depends on human-only setup (branch protection rules, Vulkan SDK install, pwsh.exe install, etc.) that no agent can perform. The orchestrator's session-start ritual checks this file and STOPS if any box is unchecked.

## Current state — wave-by-wave

### Phase 1 — every wave shipped

| Wave | Status | PRs | Notes |
|------|--------|-----|-------|
| **MVP-1.1** Real pause | ✅ landed | #30 | Pre-session |
| **MVP-1.2** Real navmesh integration | ✅ landed | #39, #40 | Closed-door + regen-on-swap; PriestPursuit stabilised |
| **MVP-1.3.1–4** Apprehend / run-loss | ✅ landed | #41, #42 | Event + BT branch + switch-breaks + out-of-range guards |
| **MVP-1.3.5** Dawn + NoVessels causes | ✅ landed | #54 (NoVessels), #57 (Dawn) | Both run-loss causes now wired + tested end-to-end |
| **MVP-1.4.1–3** Faint state machine | ✅ landed | #55, #60 (coverage gap) | Idle/Possessed/Fainted/Dead villager state; 10 s recovery; system-possess bypass |
| **MVP-1.4.4–5** Drop verb (G) | ✅ landed | #48 | Drop at foot pos + 0.5 s post-drop pickup cooldown |
| **MVP-1.4.6** Drop-pickup chain handoff | ✅ landed | #52 | Movement-sim test pinned the full chain |
| **MVP-1.5** Possession cooldown | ✅ landed | #43 | 1.5 s after voluntary switch; death/apprehend bypass it |
| **MVP-1.6** Demon-scent | ✅ landed | #45 | Table + decay + write to priest BB_KEY_HIGH_SCENT_TARGET |
| **MVP-1.7.1–3** Sprint | ✅ landed | #46 | Shift+move → 12 m/s + 3 s/s extra life cost |
| **MVP-1.7.4–5** Walk-quiet | ✅ landed | #47 | Ctrl+move → 4 m/s + halved footstep loudness |
| **MVP-1.7.6** Aelfric effective hearing halved | ✅ landed | #53 | Walk-quiet villager's footstep emits below priest hearing threshold |
| **MVP-1.8** Possession range | ✅ landed | #44 | 15 m anchor radius; anchor moves on each successful hop |
| **MVP-1.9** Priest omniscience opt-in | ✅ landed | #49 | Fallback gated behind ZENITH_INPUT_SIMULATOR + opt-in flag |
| **MVP-1.10** Save/load run state | ✅ landed | #56 | DP_RunState struct + serialisation + round-trip / corruption / version-mismatch tests |

### Phase 2 — substantively complete

| Wave | Status | PRs | Notes |
|------|--------|-----|-------|
| **MVP-2.1.1–2** Devout possession channel + priest interrupt | ✅ landed | #62 | Channel-mid switch breaks; coverage in P2 archetype tests |
| **MVP-2.1.4** Child cannot carry tools | ✅ landed | #59 | Item pickup rejected when villager archetype = Child |
| **MVP-2.1.6** Aelfric ignores Beggar | ✅ landed | #58 | Priest bridge skips Beggar candidates |
| **MVP-2.2.1–3** Reagent registry + per-tag pickup channel | ✅ landed | #64 | Registry-resolved channel duration; OnAwake + SetTag both re-resolve |
| **MVP-2.2.4–5** BogWater evaporates 8 s after drop | ✅ landed | #65 | Time-driven entity destruction |
| **MVP-2.2.6–7** BellSoul rings bell on pickup | ✅ landed | #66, **#76 (mapwide fix)** | Perception emit + new direct-BB fanout (PR #76) so priest hears from anywhere on the map, not just within 30 m hearing range |
| **MVP-2.3.2–3** Forge per-instance recipes + Wood/Spike tags | ✅ landed | #63 | Each forge entity owns a recipe; Iron→Key default, Wood→Spike new |
| **MVP-2.3.4** Forge audible across the village | ✅ landed | #67, #73 (perception emit fix) | Caught bug: forge originally only called AudioBus, not PerceptionSystem; PR #73 wired the perception emit alongside |
| **MVP-2.4.3–4** Fog GDD-contract regression guards | ✅ landed | #61 | Aelfric not revealed; dim lights cut fog holes |
| **MVP-2.4.5** Memory-fog state machine | ✅ landed | #68, #72 (integration) | Sparse hash map keyed by 1m grid cell; visible -> dim -> faded states |
| **MVP-2.5.1, 2.5.3** HUD whisper line + Aelfric awareness icon | ✅ landed | #70, #73 (integration) | State derived from priest BB each frame; integration test caught classifier issues |
| **MVP-2.5.2, 2.5.4** HUD scent + dawn gauge | ✅ landed | #69 | Both visible only when their owning state is active |
| **MVP-2.5.5–6** Pause-menu R/Q shortcuts + main-menu Quit | ✅ landed | #71, **#74 (state-leak fix)** | R reloads GameLevel; Q quits to FrontEnd. PR #74 caught: HandleRestart was setting flag but not actually reloading scene |

### Phase 4 — acceptance shipped except HUMAN_GATE

| Wave | Status | PRs / commits | Notes |
|------|--------|---------------|-------|
| **MVP-4.1.1** Golden playthrough test | ✅ landed | **#82** | `Test_P4Playthrough_Night1WinGolden` -- MVP-DoD gate. Bot scripts the canonical win path; asserts Victory event within frame budget. |
| **MVP-4.2** Run-lost HUD banner + playthroughs | ✅ landed | #75 | DPHUDController subscribes to DP_OnRunLost; per-cause copy ("CAUGHT BY AELFRIC" / "DAWN BREAKS" / "NO VESSELS REMAIN"); 3 integration playthrough tests for the 3 causes |
| **MVP-4.3.1** Acceptance-drift gate | ✅ landed | **#84** | `Tools/check_acceptance_drift.ps1` runs the 4 P4 playthroughs + asserts each test's frame count is within 20% of the baseline. CI-friendly. |
| **MVP-4.3.2** Restart prompt | ✅ landed | **#83** | Post-victory / post-loss "Press R to restart, Q to quit" overlay; R-key shortcut reloads scene + clears all run state. |
| **MVP-4.3.3** Demo packager | ✅ landed | **#85** | `Tools/package_mvp_demo.ps1` copies build output + assets to a single folder. Ready for distribution. |
| **MVP-4.3.4** Human-driven playthrough | 🚧 HUMAN_GATE | — | Tomos plays the demo end-to-end + ticks the MVP-DoD checkbox. The only MVP task left. |

### Phase 5 — instructional HUD + telemetry / verification system (post-MVP-quality work shipped 2026-05-15..16)

A self-contained feature train that landed direct-to-master after branch protection was disabled (per Tomos direction). Six independent commits, each green CI:

| Subsystem | Status | Commit | Notes |
|-----------|--------|--------|-------|
| **UI polish** -- bigger fonts, anchor-alignment + off-edge warnings | ✅ landed | #87 (`9af8435c`) | Engine off-edge UI warning fires when text renders past a canvas edge; anchor/alignment mismatch warning catches a class of authoring bugs. |
| **Detailed HUD** -- 8 new readouts | ✅ landed | `5242fc2e` | Archetype, life numeric, movement mode, vessels alive, priest distance (with red/amber/grey colour), run timer, interact hint, reagent help. |
| **Instructional HUD** -- tooltips / hotkeys / instructions | ✅ landed | `fad44f0c`, `acb4e63a` | ControlsHint (BottomRight hotkey cheat-sheet), TutorialHint (context-sensitive instruction), HelpOverlay ([H] toggle full-screen help), MenuHowTo (front-end primer). New `BuildTutorialHintForState` 9-case dispatcher. |
| **Telemetry recorder** -- engine-side | ✅ landed | `1bb46802` | `Zenith/Telemetry/Zenith_Telemetry.{h,cpp}` binary recorder + JSON exporter + round-trip + edge-case tests. |
| **DP event Hooks** | ✅ landed | `6954aaac` | `DPTelemetry::Hooks` RAII subscription holder; routes 9 DP event types into the recorder; name-resolver for JSON. |
| **Heuristic bot + playthrough test** | ✅ landed | `f207df84`, `aae4898c` | Goal-stack bot driving Zenith_InputSimulator. Phase 3a straight-line. Phase 3b upgraded to grid-A* with 60x60 walkability grid (2425/3600 walkable on GameLevel). |
| **Analyzer + Verdict** | ✅ landed | `c6c3db53` | `DPTelemetryAnalyzer` reads a recording + applies pass/fail criteria. 14-criterion stable enum; per-criterion reason strings. |
| **Runner tooling** | ✅ landed | `4beefe0a` | `Tools/dp_telemetry_runner.ps1` wraps the bot test + collects artifacts + prints summary. |
| **Test hardening** -- HUD + telemetry + bot pathing + JSON edges | ✅ landed | `d4a66f6d`, `0c9e6f2b` | Test_P2HUD_TutorialHint (9 cases), Test_P2HUD_DetailedReadouts (9 clusters), Test_TelemetryEdgeCases (6 cases inc. JSON-no-resolver), Test_DPTelemetryAnalyzer (7 clusters), Test_DPHeuristicBot_Pathing (8 clusters), Test_DPHeuristicBot_GoalDispatch (11 priority/edge cases), Test_DPTelemetryHooks (9 events). |
| **Harness timing** -- per-test wall clock + slowest-N | ✅ landed | `3cb99e84` | JSON now includes `durationMs`; stdout shows `(N frames, M.M ms)`; batch summary appends "Slowest 10". Runner script reads + reports too. Identified P1 timer cluster (~5 s each, wait on real game timers) as the suite's bulk runtime. |

### Recent quality-of-life landings

| Wave | Status | PRs | Notes |
|------|--------|-----|-------|
| **MVP-2.5.5** Rename ResetForTest → ResetForNewRun | ✅ landed | #77 | Misleading name (production HandleRestart uses it); backward-compat alias retained under ZENITH_INPUT_SIMULATOR |
| **MVP-1.3.2 coverage** Priest stillness during apprehend channel | ✅ landed | #78 | Pins "the priest plants and channels in place" GDD framing; samples post-Pursue-settle and asserts <0.1m drift to dispatch |
| **Status.md refresh** Phase 1+2 wave-by-wave + integration-test pattern | ✅ landed | #79 | Promoted every Phase-1 wave from deferred to landed; added Phase-2 + Phase-4 tables |

## Suite growth

Master 36 → 122 PASSED across 60+ merged PRs / direct-to-master commits in <96 h.

**Recent test highlights:**

```
Test_P1Drop_PickupChainHandoff           (PR #52)
Test_P1Faint_RecoversToIdle              (PR #55 + coverage gap PR #60)
Test_P1Faint_SystemPossessBypassesGate
Test_P1WalkQuiet_AelfricEffectiveHearingHalved (PR #53)
Test_P1Save_RoundTripMeta                (PR #56)
Test_P1Save_RobustToCorruption
Test_P1Save_VersionMismatchFallsBackToDefault
Test_P1Dawn_DispatchesRunLost            (PR #57)
Test_P1Dawn_NoFireWhenNotStarted
Test_P1NoVessels_DispatchesRunLost       (PR #54)
Test_P1Apprehend_PriestStandsStillDuringChannel  (PR #78 — coverage gap from Status.md)
Test_P2Archetype_*                       (PR #58, #59, #62 — Beggar / Child / Devout)
Test_P2Reagent_*                         (PR #64-66 — registry + BellSoul + BogWater)
Test_P2Forge_*                           (PR #63, #67, #73)
Test_P2Fog_Memory*                       (PR #68, #72)
Test_P2HUD_*                             (PR #69-70, #73)
Test_P2Menu_PauseAndMainMenuShortcuts    (PR #71)
Test_P2Pause_RestartActuallyReloadsScene (PR #74 — caught real state-leak)
Test_P2BellSoul_PriestHearsTheBell       (PR #72)
Test_P2BellSoul_AudibleAcrossMap         (PR #76 — pins map-wide fix)
Test_P4Playthrough_LossByApprehend       (PR #75)
Test_P4Playthrough_LossByDawn
Test_P4Playthrough_LossByNoVessels
Test_P4Playthrough_Night1WinGolden       (PR #82 -- MVP-DoD)
Test_TelemetryRoundTrip                  (Phase 1 -- 1bb46802)
Test_TelemetryEdgeCases                  (Phase 1 hardening -- d4a66f6d / 0c9e6f2b)
Test_DPTelemetryHooks                    (Phase 2 -- 6954aaac)
Test_DPHeuristicBotPlaythrough           (Phase 3a/b -- f207df84 + aae4898c)
Test_DPHeuristicBot_Pathing              (Phase 3b hardening -- 0c9e6f2b)
Test_DPHeuristicBot_GoalDispatch         (Phase 3b hardening -- 0c9e6f2b)
Test_DPTelemetryAnalyzer                 (Phase 4 -- c6c3db53)
Test_P2HUD_TutorialHint                  (Phase 5 hardening -- d4a66f6d)
Test_P2HUD_DetailedReadouts              (Phase 5 hardening -- d4a66f6d)
```

## Integration tests caught real bugs this run

The "write the integration test that pins the gameplay scenario, watch it find a bug" pattern paid off twice in 24 h:

1. **PR #73 (forge perception gap):** `Test_P2Forge_PriestHearsTheHammer` failed because `DPForge::HandleInteract` only called `Zenith_AudioBus::EmitSound` (test instrumentation), not `Zenith_PerceptionSystem::EmitSoundStimulus` (priest hearing path). These are DIFFERENT systems. Fix: emit to both.
2. **PR #74 (pause restart state-leak):** `Test_P2Pause_RestartActuallyReloadsScene` failed because `HandleRestart` did `LoadSceneByIndex` but didn't clear `DP_Player` / `DP_Win` / `DP_Fog` / `DP_Night` state. Fix: extracted `ResetAllRunStateBeforeReload()` helper (also calls the new `DP_Player::ResetForNewRun` from PR #77).

Both bugs would have shipped to the human-playthrough gate without the integration tests.

## Major engine + cross-system fixes landed in this run

1. **PriestPursuit_Test was failing for three stacked reasons** (PR #39):
   - **STATIC bodies silently ignore `SetPosition`.** Priest body was authored STATIC. Fixed by switching to DYNAMIC + gravity-off + locked rotations.
   - **Engine BT `MoveTo` returned FAILURE the same frame `SetDestination` was called.** Fixed by checking `pxNav->NeedsPath()` and returning RUNNING.
   - **`Zenith_BTSelector` is a memory-selector**, not reactive. Fixed by `m_xTree.Reset()` on rising edge of `BB_KEY_TARGET_WITH_DEVIL`.
2. **DP_AI navmesh cache** wired to `Zenith_NavMeshGenerator::GenerateFromScene` (PR #39).
3. **Engine BT leaf nodes must set `m_eLastStatus` explicitly** before returning (PR #41).
4. **Priest omniscient fallback compiled out of production** (PR #49).
5. **BellSoul audibility clamp** (PR #76): Zenith_PerceptionSystem clamps each agent's hearing at `min(emit_radius, agent_max_range)`, so a 200m BellSoul emit didn't reach priests >30m away. Added `DP_AI::NotifyAllPriestsOfInvestigatePos` direct-BB fanout that bypasses the perception clamp.

## Architectural surface added in this run

Public APIs new on `DP_Player`:

- `TryVoluntaryPossessSwitch(EntityID) -> bool` — player-driven path (cooldown + range gates).
- `TickPossessionCooldown(fDt)` / `GetPossessionCooldownRemaining()` — MVP-1.5.
- `GetDemonScent(EntityID)` / `TickDemonScent(fDt)` / `WriteHighestScentToBlackboard()` — MVP-1.6.
- ~~`SetTestOmniscientFallback(bool)` / `IsTestOmniscientFallbackEnabled()`~~ — removed 2026-05-19. The omniscient fallback in `Priest_Behaviour::BridgePerceptionToBlackboard` was a test backdoor that leaked into every build (`ZENITH_INPUT_SIMULATOR` is defined unconditionally). Replaced by real perception: tests that need the priest to acquire a target now call `Zenith_PerceptionSystem::RegisterTarget(villager, /*hostile=*/true)` and teleport the villager into the priest's sight cone — same code path production uses.
- `ResetForNewRun()` (PR #77) — clears every per-run state owner. Called by `DPPauseMenuController::HandleRestart/HandleQuit` AND the harness between-tests hook. Was named `ResetForTest`; renamed because the "ForTest" suffix was misleading after MVP-2.5.5 wired it into production. Backward-compat alias `ResetForTest()` retained under `ZENITH_INPUT_SIMULATOR`.

New on `DP_AI`:

- `BB_KEY_HIGH_SCENT_TARGET` — priest BB key the future hound BT will read.
- `NotifyAllPriestsOfInvestigatePos(Vec3)` (PR #76) — direct-BB fanout. Bypasses perception clamp. Used for "map-wide" stimuli like BellSoul.

New on `DP_Night` (PR #57):

- `StartNight(fDuration)` / `TickNight(fDt)` / `GetNightTimeRemaining()` / `IsNightActive()` / `Reset()`.
- Fires `DP_OnRunLost{Dawn}` exactly once on cross-zero.

New on `DP_Save` (PR #56):

- `DP_RunState` struct + binary round-trip. Schema version anchored; corruption + mismatch fall back to default.

New BT nodes in `Components/DP_BT_Nodes.h`:

- `DP_BTAction_Apprehend` (PR #41) — channels in apprehend range, dispatches `DP_OnRunLost{Apprehended}`.

New on `DPHUDController_Behaviour` (PR #75):

- Subscribes to `DP_OnRunLost`. Per-cause permanent banner via `SetStatusText`. Test accessors `DidRunLostHandlerFireForTest` / `LastRunLostCauseForTest` / `ResetRunLostForTest`.

## Ownership

**Per user direction 2026-05-12: engine work is in-scope for autonomous orchestrator + subagents.** No parallel human work track. The orchestrator drives both game-side and engine-side tasks sequentially. Mitigations on engine PRs:

- **Mandatory Reviewer subagent** on every engine PR (per OrchestratorPlaybook §5.4).
- **Full test suite + Combat smoke build** on every engine PR.
- **Design rationale in `DecisionLog.md` before the PR** for any net-new engine namespace or non-trivial algorithm choice.

Tomos's role: tick `ManualSetupChecklist.md` once before the first session; review high-stakes design rationale via `DecisionLog.md`; final eye-test on S2 art when it arrives.

## Notes for next agent

1. **Operating mode (updated 2026-05-15):** direct-to-master workflow per Tomos. Branch protection disabled on master; CI still runs on every push via `push: branches: [master]` triggers in `dp-pr.yml`, `dp-tests.yml`, `complexity.yml`, `doc-lint.yml`. No PR branch required; commit + `git push origin HEAD:master` from the main checkout. Worktrees in `.claude/worktrees/` are sandbox-only; main work happens at `C:/dev/Zenith` on master.
2. **Hard invariants (revised 2026-05-15):** (a) commits must keep the full suite green; (b) per-test JSON `durationMs` must not regress > 20% on any test without justification (use `Tools/check_acceptance_drift.ps1` to detect); (c) no docs without explicit user request -- Status.md / DecisionLog.md / MvpRoadmap.md updates ARE within scope when shipping new features.
3. **First action:** verify the baseline. Build the prototype, run the full suite via `Tools/run_dp_tests.ps1 -Headless`. Expect **122 PASSED / 0 FAILED** on `master @ 3cb99e84`.
4. **Next-up work** (Phase 1, 2 + most of Phase 4 done; Phase 5 telemetry shipped):
   - **MVP-4.3.4** 🚧 HUMAN_GATE -- Tomos plays the MVP demo end-to-end + ticks the box. The only MVP task left.
   - **Phase 3 (assets)** -- Mixamo Y-Bot spike (`MVP-3.0.1` 🚧 HUMAN_GATE), villager / priest skeletons. Most of Phase 3 needs human-provided FBX.
   - **Procgen Phase A** -- generator scaffolding designed in 2026-05-16 chat; not yet started. 16-seed pool of generated GameLevels to replace the UE placeholder; uses the existing WallSection / CornerWallSection / SM_Cube mesh kit. Would expand verification from 1 scene to 16.
   - **Bot Phase 3c** -- make the heuristic bot actually win the GameLevel (currently dies once + only triggers 2 events in 35 s). Would let the analyzer's `VictoryFired` / `AnySprintFrame` / `AnyWalkQuietFrame` criteria activate.
   - **P1 timer-cluster amortisation** -- 9 tests in the P1 family burn 5-6 s each waiting on real game timers; the slowest-N report identifies them. `Tools/run_dp_tests.ps1` reports total suite time + the top-10 slowest after every run. Migrating these to accelerated test-only timers would drop suite from 283 s to ~100 s.
   - **Integration test follow-ups:**
     - HUD banner stacking: Victory + DeathEvent + RunLost in the same frame -- does the right one win?
     - Save+reload+possess: after `LoadFromBlob`, does the possessed-villager handle still resolve?
5. **Local-run pitfalls** (caught 2026-05-15..16):
   - Direct `devilsplayground.exe --automated-test <name>` invocation differs from `Tools/run_dp_tests.ps1 -Filter <name>` results. The runner adds `--skip-tool-exports --skip-unit-tests` which the engine's automated-test driver requires for reliable batched test behaviour. Always run tests through the runner script, not direct exe invocation.
   - The `Sharpmake_Build.bat` script has a `pause` directive; running it non-interactively hangs. **Use `Build/run_sharpmake.bat` instead** -- shipped 2026-05-15 as the non-interactive equivalent. Returns ERRORLEVEL via exit code rather than the modal pause.
   - `Tools/run_dp_tests.ps1` defaults to `-ExitAfterFrames 600` (~10 s per test). The bot playthrough (`Test_DPHeuristicBotPlaythrough`) is bounded by this cap; `Tools/dp_telemetry_runner.ps1` overrides to 2100 (~35 s) for the bot test.
6. **Branching:** all work on `master`. Branch as `dp/mvp-<task-id>` or `dp/<scope>`. Per OrchestratorPlaybook Invariant 2 (no git worktrees), prefer `C:\dev\Zenith\` directly; if the harness places you in a `.claude/worktrees/<name>/` checkout, treat it as a transient sandbox and remember Sharpmake bakes the cwd path into generated vcxprojs.
7. **No external spend** unless you find a paid blocker (and even then, log it in Questions.md and pick a different task to unblock yourself).
8. **Engine surface to be aware of** for new tests/features:
   - `Zenith_BTNode::m_eLastStatus` must be assigned by leaf nodes before returning.
   - `Zenith_TransformComponent::SetPosition` routes through Jolt when a body exists. STATIC bodies don't activate; use DYNAMIC for anything that moves via NavMeshAgent or transform writes.
   - `DP_Player::TryVoluntaryPossessSwitch` is the player path (cooldown + range gates); `SetPossessedVillager` is the system path (death, apprehend, tests).
   - Villagers don't self-register with `Zenith_PerceptionSystem`. Sight-based tests need to call `Zenith_PerceptionSystem::RegisterTarget(villager_id, /*hostile=*/true)` manually.
   - **Perception hearing has a clamp**: `min(emit_radius, agent_max_range)`. For "map-wide" stimuli (BellSoul-class) use `DP_AI::NotifyAllPriestsOfInvestigatePos` to bypass.
   - `DP_Player::ResetForNewRun()` is the canonical reset (PR #77). `ResetForTest` is a backward-compat alias for tests only.

## Suggested next coverage gaps

Tests that would catch a SPECIFIC concrete regression. Most prior gaps from earlier Status.md revisions have been closed (Phase 5 hardening commits added ~50 internal cases across the bot, analyzer, and telemetry).

| Test | What it would catch | Effort |
|------|---------------------|--------|
| `Test_P2HUD_VictoryThenRunLostBannerWins` | Run-lost banner overwrites Victory banner (or vice versa) when both dispatch in the same frame | S |
| `Test_P1Save_PossessedVillagerHandleSurvivesReload` | `LoadFromBlob` re-resolves the possessed-villager handle (entity-index reuse after scene unload is a classic dangling-handle source) | M |
| `Test_P4Playthrough_VictoryThenRestartIsClean` | Pause-R after winning produces a clean second run (no leftover DP_Win state, no leftover held items) | M |
| `Test_P2Forge_RecipeRespectsHeldVillagerTag` | Forge crafting checks the holder's archetype tag (Child can't trigger forge) | S |
| `Test_P1TimerHarness_FastForward` | Speeding wall-clock waits in P1 tests via fixed-dt amortisation -- once landed, the P1 timer cluster drops from 5 s to <1 s each, suite from 283 s to ~100 s | M |

## Last completed

Most recent at top. Anything older than 2026-05-14 see `git log`.

Direct-to-master (post-#87, branch protection disabled per Tomos 2026-05-15):
- **`3cb99e84`** (2026-05-16) — Per-test timing in automated-test harness + slowest-N report.
- **`0c9e6f2b`** (2026-05-16) — Harden Phase 3b bot pathing + dispatch + telemetry JSON edge (3 new tests, +21 cases).
- **`aae4898c`** (2026-05-15) — Phase 3b telemetry: bot upgraded to grid-A* path follow.
- **`4beefe0a`** (2026-05-15) — Phase 5 telemetry: developer-facing runner script (`Tools/dp_telemetry_runner.ps1`).
- **`c6c3db53`** (2026-05-15) — Phase 4 telemetry: analyzer library + Verdict + bot integration.
- **`f207df84`** (2026-05-15) — Phase 3a telemetry: heuristic bot driver + playthrough test.
- **`6954aaac`** (2026-05-15) — Phase 2 telemetry: DP event enum + RAII Hooks helper.
- **`d4a66f6d`** (2026-05-15) — Harden HUD + telemetry coverage (3 new tests, +21 cases).
- **`1bb46802`** (2026-05-15) — Phase 1 telemetry recorder: binary stream + JSON export.
- **`acb4e63a`** (2026-05-15) — Main-menu HowTo title/body spacing fix.
- **`fad44f0c`** (2026-05-15) — Instructional HUD: tooltips, hotkeys, instructions ([H] toggle + FrontEnd primer).
- **`5242fc2e`** (2026-05-15) — Detailed HUD: 8 new readouts (archetype, life numeric, movement, vessels, priest dist, run timer, interact hint, reagent help).

Merged PRs (pre-direct-to-master):
- **PR #87** (merged `2026-05-15`) — UI polish + off-screen Zenith_Warning. Bigger HUD fonts; main-menu Center alignment; off-edge warning fires on rendering past any canvas edge.
- **PR #86** (merged `2026-05-15`) — Sprint C cleanup: DP_Tuning leak fix + roadmap/testplan/questions tidy.
- **PR #85** (merged `2026-05-15`) — MVP-4.3.3: `Tools/package_mvp_demo.ps1` demo packager.
- **PR #84** (merged `2026-05-15`) — MVP-4.3.1: acceptance criteria gate (`Tools/check_acceptance_drift.ps1`).
- **PR #83** (merged `2026-05-15`) — MVP-4.3.2: post-victory/post-loss restart prompt + R-key shortcut.
- **PR #82** (merged `2026-05-15`) — MVP-4.1.1: `Test_P4Playthrough_Night1WinGolden` (MVP-DoD gate).
- **PR #81** (merged `2026-05-15`) — FindHudText walks all loaded scenes (catches persistent-scene UI).
- **PR #80** (merged `2026-05-15`) — Exhaustive audit of DP/Docs against current master state.
- **PR #79** (merged `2026-05-15`) — Status.md refresh: Phase 1 + 2 complete, Phase 4 loss UI shipped.
- **PR #78** (merged `2026-05-15`) — `Test_P1Apprehend_PriestStandsStillDuringChannel` (Status.md coverage gap).
- **PR #77** (merged `2026-05-15`) — Rename `DP_Player::ResetForTest` → `ResetForNewRun`.
- **PR #76** (merged `2026-05-15T01:09Z`) — MVP-2.2.6 BellSoul truly map-wide.
- **PR #75** (merged `2026-05-15T00:49Z`) — MVP-4.2 run-lost HUD banner + 3 loss-state playthrough tests.
- **PR #74** (merged `2026-05-14T23:11Z`) — pause Restart actually clears run state + reloads.
- **PR #73** (merged `2026-05-14T22:52Z`) — Phase-2 integration tests; caught real forge → priest perception gap.

For PRs older than 2026-05-14 see git log.
