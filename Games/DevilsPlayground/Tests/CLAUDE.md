# DevilsPlayground/Tests

138 registered automated tests across 116 .cpp files (as of 2026-06-12).
The full list is `devilsplayground.exe --list-automated-tests`. The DP
suite is the primary quality gate; the bar for any new change is "the
full headless batch stays green."

## Running the suite

```
# Build first (Debug_True for full instrumentation; Debug_False for
# matrix runs that need to mimic CI; Release_False for perf checks).
cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Build\zenith_win64.sln /t:DevilsPlayground /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount'

# Run the suite headless (default mode -- batch all tests in one process).
powershell -NoProfile -File Tools/run_dp_tests.ps1 -Headless

# Filter to one test (forces per-process mode).
powershell -NoProfile -File Tools/run_dp_tests.ps1 -Filter Sprint -Headless

# Force per-process mode for everything (slower but bulletproof against
# state leaks across tests).
powershell -NoProfile -File Tools/run_dp_tests.ps1 -PerProcess -Headless
```

Default is **batch mode** — `--all-automated-tests` registers every
test, the engine boots once (~25 s), and each test runs sequentially
with a between-tests reset. Total wall-clock ~120 s for ~120 tests vs
~10 minutes in per-process mode.

Between batched tests the harness:

1. Force-reloads scene 0 (FrontEnd) — so entity-managed side-tables
   clear via `OnDestroy`.
2. Fires every hook registered with
   `Zenith_AutomatedTestRunner::RegisterBetweenTestsHook`.
   `DevilsPlayground.cpp`'s hook resets `DP_Player`, `DP_Win`,
   `DP_Fog`, `DP_AI`, and `DP_Night`'s persistent globals.
3. Clears the input simulator's held-keys + key-press queue.

## Test naming convention

| Prefix | Meaning |
|---|---|
| `Test_Hello`, `Test_MouseWheel` | Harness smoke -- no gameplay dependency |
| `Test_P1<Subsystem>_<scenario>` | Phase-1 gameplay (possession, life timer, sprint, pause, navmesh, etc) |
| `Test_P2<Subsystem>_<scenario>` | Phase-2 gameplay (archetypes, reagents, fog memory, HUD) |
| `Test_P4Playthrough_*` | Phase-4 full-loop integration tests |
| `Test_ProcLevel_*` | Procgen generator unit tests |
| `Test_DPTelemetry*` / `Test_Telemetry*` | Telemetry recorder / analyser / hooks |
| `Test_DPHeuristicBot_*` | Heuristic bot pathing / goal dispatch |
| `PersonalityPlaythrough_*` | The 8-personality matrix tests (Casual / Stealth / Speedrunner / Zealot + Magpie / Relay / Heretic / Trickster) |
| `Test_<Subsystem>` (no prefix) | Smoke + contract tests authored pre-Phase-1 |

The convention isn't strict — `Test_P1Apprehend_*` and
`Test_P1NavMesh_*` are sub-clusters within Phase 1, etc — but the
prefix lets the runner's `-Tier 1` flag filter by phase.

## Headless gating

Some tests need a GPU (model rendering, material upload, etc) and
register with `m_bRequiresGraphics = true`. In headless mode the
harness counts these as PASSED-skipped — the runner reports `35/35`
when 24 actually executed and 11 skipped.

**Manual-only tests** (the 8 `PersonalityPlaythrough_*` balance harnesses)
set `m_bManualOnly = true` and are excluded from the `--all-automated-tests`
batch (CI + `run_dp_tests.ps1`): the harness marks them SKIPPED (counts as
passed), regardless of headless, because they are long-running
(~30–145 s each, ~12 min combined) seed-matrix/balance runs with no
per-commit signal. A direct `--automated-test <name>` still runs them in
full — so `Tools/dp_seed_matrix_run.ps1` (which invokes each personality by
name) is unaffected. The batch therefore reports them SKIPPED while the
seed-matrix tooling still executes them.

The headless boot in `Zenith_Main` branches on
`Zenith_CommandLine::IsHeadless()` to:

- Skip `Flux::EarlyInitialise` / `LateInitialise` / `Shutdown` / GPU
  wait.
- Short-circuit Editor::WaitForGPUAndFlushDeferred.
- Guard every `vmaCreate*` with a null-allocator check.
- Loosen view-creation asserts to allow null VRAM handles.
- Soften `SET_MODEL_MATERIAL` to warn-and-skip on missing model.

## The personality framework

`Test_PersonalityPlaythrough.cpp` registers **8 tests** sharing a Setup
/ Step / Verify trio + a per-personality `g_xActiveCfg`. Each personality
toggles a small bundle of input-layer flags — adding a new one is
~30 lines (enum value + `kPersonality_X` constexpr + Setup wrapper +
`ZENITH_AUTOMATED_TEST_REGISTER`).

```
PersonalityPlaythrough_Casual       Walks normally, single F-press, runs the pause overlay
                                    test, engages every system. Reference recording.
PersonalityPlaythrough_Stealth      Holds Ctrl while walking (walk-quiet, 0.875x speed of
                                    jog as of 2026-05-22, halved footstep loudness x0.25).
                                    Skips the noise machine entirely.
PersonalityPlaythrough_Speedrunner  Adaptive sprint -- holds Shift only while the next
                                    target is > 5 m away; walks close approaches. Runs
                                    the full bootstrap chain. Sprint life-cost is now
                                    1.5/s (was 1.0 -- bumped 2026-05-22 because cheap
                                    sprint let Speedrunner win 100% of seeds).
PersonalityPlaythrough_Zealot       Skips the entire iron/forge/door/chest/noise
                                    bootstrap -- jumps straight from possession to the
                                    objective-deliver loop. Adaptive sprint between
                                    targets. (Replaced Berserker on 2026-05-20.)
PersonalityPlaythrough_Magpie       (NEW 2026-05-21) Opportunistic objective ordering --
                                    picks the closest uncollected objective each iter
                                    instead of fixed Obj1->5 order. Runs full bootstrap.
                                    Empirical question: how much of Zealot's underperform-
                                    ance was fixed-order traversal cost?
PersonalityPlaythrough_Relay        (NEW 2026-05-21) Voluntary-switch + drop-handoff. When
                                    life timer drops below 5 s while holding an obj, drops
                                    it at the foot of the nearest healthy villager and
                                    click-possesses them (voluntary-faint, not death).
                                    Skips bootstrap.
PersonalityPlaythrough_Heretic      (NEW 2026-05-21) Walks to + F-presses the noise machine
                                    BEFORE the objective loop (priest distraction bait),
                                    then jumps to obj loop. Skips iron/forge/door/chest.
                                    Distract-frame count was 90 originally; dropped to 0
                                    in 2026-05-21 because lingering pulled priest INTO the
                                    bot rather than away.
PersonalityPlaythrough_Trickster    (NEW 2026-05-21 PR #140) Magpie's any-order pick +
                                    Relay's voluntary-switch + Casual's bootstrap chain +
                                    Speedrunner's adaptive sprint. The combo predicted
                                    strongest by the 7p x 10s matrix.
```

Each test drives the procgen scene through `Zenith_InputSimulator` —
mouse click to possess, WASD camera-relative to walk, Shift / Ctrl
modifier keys, F to interact, G to drop, Esc / R / Q for pause /
restart / quit. **No teleporting, no `*ForTest` bypass, no
`SetPossessedVillager`** — every action goes through the same input
path the player uses.

### PersonalityConfig fields

**Design principle (ratified 2026-05-23):** personalities are *decision
profiles* that simulate how different humans play the same game. They
choose which legitimate in-game actions to use (sprint vs walk,
walk-quiet vs walk, drop+switch vs hold, any-order vs fixed-order
objectives, noise-bait vs ignore). They **MUST NOT** differ in
mechanical capabilities the simulated player wouldn't have control
over: life-timer length, walk speed, pickup radius, dawn-timer length,
repossession latency, test-harness retry budgets, etc.

If a new personality "needs more time" or "needs more patience" to win
its target seeds, the right answer is to either tune the underlying
game (life timer / dawn / walk-quiet multiplier in `Config/Tuning.json`,
or procgen size in `GenConfig`) so that a real human playing that
style could win — or accept that the style has lower win-rate ceiling
on hard seeds, as long as the balance criteria (every personality
strictly between 0 % and 100 %; every seed winnable by ≥1 personality)
are met.

The personality test reads these per-personality decision flags off
`g_xActiveCfg`. Adding a new flag means: (1) add to the
`PersonalityConfig` struct + (2) initialise it on all 8 existing
configs + (3) read it from the Step machine.

| Field | Meaning |
|---|---|
| `bHoldSprint` | Hold Shift for the whole walk (always sprint) |
| `bHoldQuiet` | Hold Ctrl for the whole walk (walk-quiet) |
| `bAdaptiveSprint` | Sprint only while target > `kSprintMinDistanceM` (5 m) away |
| `bSkipBootstrap` | Skip iron / forge / door / chest / noise -- jump straight to obj loop |
| `bRunNoiseMachine` | Walk to + engage the noise machine on the bootstrap path |
| `bRunPauseTest` | Run the Esc pause-overlay phases at all |
| `iPauseCycles` | How many open/close cycles when `bRunPauseTest` |
| `bAnyOrderObjectives` | (Magpie) Pick closest uncollected obj each iter, not fixed Obj1->5 |
| `bUseRelayDrop` | (Relay) Drop + voluntary-switch when life < `kRelayLifeThresholdSec` |
| `bDeliberateNoiseFirst` | (Heretic) F-press noise machine FIRST, before bootstrap |

The test-harness internals `kWalkFrameBudget` (per-walk-goal frame
cap) and `kObjAttemptCap` (per-objective retry cap) are file-scope
constants that apply uniformly to every personality. They used to vary
per personality (Stealth had 2x walk budget, retry caps ranged 12–24)
but were unified on 2026-05-23 because varying them per personality
is a hidden buff/nerf with no human-player equivalent.

Path-finding for the bot uses a 240×240 grid (0.5 m cells) over the
playable area. The grid is rebuilt on door open / scene reload via the
`g_bPathGridBuilt` flag. A* between current villager position and
target; side-step recovery (90° rotate for 0.5 s) on stuck-detect.

> **Known follow-up**: this grid-A* duplicates the engine's
> `Zenith_Pathfinding::FindPath` over the navmesh (which the priest
> uses). A 2026-05-22 refactor attempt found that FindPath returns
> FAILED for ~99% of bot queries on procgen layouts even with explicit
> polygon-centre snapping -- suggests engine-side issue in
> `Zenith_NavMeshGenerator` polygon coverage. Reverted; deferred as
> engine work. The door collider physics fix that landed in the same
> PR brings the bot's grid into parity with the player's capsule
> physics (both now correctly block on closed doors), regardless of
> which pathfinding system the bot uses.

Cross-possession memory (PR #127) — when a villager dies mid-walk
holding an objective, the bot:

- Rewinds to `ObjLoopFind` so the new villager re-picks the item.
- Re-aims at the closest item to the new villager (2 m hysteresis to
  avoid flip-flop).
- Doesn't increment the per-objective attempt counter (the failure was
  the death, not the delivery attempt).

### Win condition (since 2026-05-22)

`DP_Win::NotifyObjectiveCollected` fires `DP_OnVictory` when
`popcount(collected_mask) >= night.reagents_required_for_victory`.
Default tuning is **3-of-5**; the test bot reads the same mask via
`DP_Win::GetCollectedObjectivesMask` + `DP_Win::HasWon`. Changing the
tuning value flips the win bar without a code change.

## Seed-matrix tooling

Across the 8 personalities × N procgen seeds. Default `-Parallelism 16`
uses one engine process per HW thread (the runner spawns concurrent
DP processes; matrix wall-clock is ~20 min for 80 cells on a 16-thread
box, vs ~50 min serial).

```
# Quick smoke (3 default seeds)
powershell -NoProfile -File Tools/dp_seed_matrix_run.ps1

# Canonical 10-seed test set (ratified 2026-05-22 -- excludes seed 0
# because its procgen layout is unsolvable; see Docs/Shortfalls.md).
pwsh -Command "& 'Tools/dp_seed_matrix_run.ps1' -Seeds @(1,5,7,42,100,12345,55555,99999,250000,4276994270) -Parallelism 16"

# Cross-personality summary (win-rate / ObjsAvg / deaths / etc).
python Tools/dp_personality_compare.py

# Regenerate the legacy analyser report from existing telemetry.
python Tools/dp_seed_matrix_analyse.py
```

### Balance criteria (ratified 2026-05-21 by user)

1. Every personality has win rate strictly between 0% and 100% (each
   wins some games AND loses some).
2. Every level (procgen seed in the canonical 10-seed set) is winnable
   by at least one personality.

Both currently met -- per-personality details in
`Docs/GameBalance_2026-05-22.md`.

Each cell writes a per-(seed, personality) `.ztlm` + `.json` to
`Build/dp_telemetry/seed_matrix/seed_<seed>/<personality>.*`, plus a
`.png` overlay via the visualiser. The analyser cross-checks the
matrix into `Build/dp_telemetry/seed_matrix/REPORT.md`.

The procgen seed for any DP run is overridable via the
`DP_PROCGEN_SEED` env var (PR #121). The matrix runner exports it per
cell.

## Adding a new test

```cpp
#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
// ... other includes

static void Setup_Foo()                  { /* ... */ }
static bool Step_Foo(int iFrame)         { return iFrame < 10; }
static bool Verify_Foo()                 { return /* ... */; }

static const Zenith_AutomatedTest g_xFooTest = {
    "Foo_Test",
    &Setup_Foo,
    &Step_Foo,
    &Verify_Foo,
    /*maxFrames*/ 60,
    /*requiresGraphics*/ false  // set true if the test asserts on rendered output
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xFooTest);

#endif // ZENITH_INPUT_SIMULATOR
```

Drop into `Tests/`, regen Sharpmake
(`cmd /c '.\Sharpmake_Build.bat < nul'` from `Build/`), build. The
harness's boot ordering ensures Setup runs **after** editor-automation
drains, the initial scene is loaded, OnAwake/OnStart have fired, and
Playing mode is set. Step is called every main-loop frame until it
returns false or the test's `m_iMaxFrames` is hit.

If your test needs the gameplay scene, call
`Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE)` — that
is ProcLevel. Use `DP_Query::ForEachComponentInActiveScene<T>` to find
priest / villager / item entities by type; do **not** bake in entity
names or world positions, because procgen reshuffles those per seed.

## Engine pitfalls writing DP tests

- **`static const Zenith_AutomatedTest g_xT` lands in read-only
  memory.** The registry uses a separate writable
  `Zenith_AutomatedTestNode` — don't `const_cast` and write back to
  the test struct itself.
- **`SimulateClickOnUIElement` asserts on missing element**, killing
  the process. If you write a UI-click test, ensure the named element
  exists in the active scene's UICanvas at the moment of the click.
- **Game components added mid-frame are awoken by the next scene
  Update (wave-drain), not synchronously at `AddComponent<T>()`.**
  A test that registers into a component-owned side table immediately
  after attaching must either wait a frame or call the component's
  `OnAwake()` by hand (safe only when the scene is unloaded before any
  Update — see Test_PublicInterfaces.cpp). `TryGetComponent<T>()` is
  the null-safe lookup (returns nullptr when absent).
- **Procgen geometry is not tuned for hand-tuned priest tests.** Tests
  that need a specific priest/villager spatial arrangement (e.g.
  teleport priest to 1.5 m east of villager) may fail on procgen
  because the navmesh agent steers the priest back to its nearest
  patrol-node polygon. Stage such tests by spawning fresh entities at
  known clear positions rather than teleporting procgen-spawned
  entities.
- **Telemetry recorder needs explicit Begin/End.** The personality
  test's `Setup_HumanPlaythrough` defers `Begin()` to `kHP_CaptureRefs`
  so the FrontEnd-boot frames don't pollute the recording; `Verify`
  always calls `End()` regardless of pass/fail so the artifacts are
  always available for offline inspection.
- **Fixed-dt pin.** The personality test calls
  `Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f)` in Setup so that
  the same frame-counted Q-hold produces the same camera rotation in
  Debug at 30 fps wall-clock and in Release_False at 200 fps. Released
  in Verify (no Teardown hook). Tests that don't pin dt may see
  flaky behaviour at different wall-clock speeds.

## Test index by category

### Harness smoke

`Test_Hello`, `Test_MouseWheel`, `Test_PublicInterfaces` (5 namespace
APIs + source-bug guards: `DP_HeldItem_Test`, `DP_FindItemByTag_Test`,
`DP_Win_Test`, `DP_Fog_Test`, `DP_Unlock_Test`).

### Phase 1 -- foundation gameplay

| Cluster | Tests |
|---|---|
| Pause | `Test_P1Pause_TimerStopsOnEscape`, `Test_P1Pause_InputSimDuringPause` |
| NavMesh | `Test_P1NavMesh_PathRespectsWalls`, `Test_P1NavMesh_ClosedDoorBlocksPath`, `Test_T1NavMesh_GeneratorPerfOnProcLevel`, `Test_T1NavMesh_BTUnitsCanFollowRealPath` |
| Apprehend | `Test_P1Apprehend_SwitchBreaksChannel`, `Test_P1Apprehend_OutOfRangeIgnored`, `Test_P1Apprehend_PriestStandsStillDuringChannel` |
| Faint / Switch | `Test_P1Faint_RecoversToIdle`, `Test_P1Faint_SystemPossessBypassesGate`, `Test_P1Switch_FaintNotDie`, `Test_P1Switch_BurnOutDoesDie` |
| Drop | `Test_P1Drop_GoesToGroundAtBodyPosition`, `Test_P1Drop_PickupChainHandoff` |
| Cooldown | `Test_P1Cooldown_CannotPossessFor1pt5s`, `Test_P1Cooldown_NotAffectedByDeath` |
| Demon-scent | `Test_P1Scent_AccumulatesOnPossession`, `Test_P1Scent_DecaysOverTime`, `Test_P1Scent_HighestWinsInBlackboard`, `Test_P1Scent_NoBumpWhilePossessing`, `Test_P1Scent_NotificationToBlackboard` |
| Sprint | `Test_P1Sprint_DrainsLifeFaster`, `Test_P1Sprint_NoDrainWhenNotMoving`, `Test_P1Sprint_WinsTieOverWalkQuiet` |
| Walk-quiet | `Test_P1WalkQuiet_*` (4 tests) |
| Possession range | `Test_P1Range_RefusedOutOfRange`, `Test_P1Range_AcceptedInRange`, `Test_P1Range_AnchorMovesWithEachHop` |
| Priest perception | `Test_P1Priest_DoesNotChasePossessedOutOfSight`, `Test_P1Priest_PursuesAfterLineOfSight` |
| Save / load | `Test_P1Save_RoundTripMeta`, `Test_P1Save_RobustToCorruption`, `Test_P1Save_VersionMismatchFallsBackToDefault` |
| Dawn | `Test_P1Dawn_DispatchesRunLost`, `Test_P1Dawn_NoFireWhenNotStarted` |
| NoVessels | `Test_P1NoVessels_DispatchesRunLost` |
| Tuning | `Test_P1Tuning_LoadsAndValuesInBand`, `Test_P1Tuning_*ValuesMatchConfig` (3 tests) |

### Phase 2 -- depth

| Cluster | Tests |
|---|---|
| Archetypes | `Test_P2Archetype_TimersMatchSpec`, `Test_P2Archetype_DevoutChannel*`, `Test_P2Archetype_ChildCannotCarryTools`, `Test_P2Archetype_BeggarIgnoredByAelfric` |
| Reagents | `Test_P2Reagent_UniquePickupChannel`, `Test_P2Reagent_BogWaterEvaporates`, `Test_P2Reagent_BellSoulRingsBell`, `Test_P2BellSoul_*` |
| Forge | `Test_P2Forge_*` (3 tests: recipe, audible-at-30m, priest-hears-the-hammer) |
| Fog memory | `Test_P2Fog_AelfricNotRevealed`, `Test_P2Fog_LightAddsHole`, `Test_P2Fog_MemoryDimsAfter10s` |
| HUD | `Test_P2HUD_TutorialHint` (9 cases), `Test_P2HUD_DetailedReadouts` (9 clusters) |
| Pause / Menu | `Test_P2Menu_PauseAndMainMenuShortcuts`, `Test_P2Pause_RestartActuallyReloadsScene` |

### Phase 4 -- acceptance

`Test_P4Playthrough_Night1WinGolden` (MVP-DoD gate),
`Test_P4Playthrough_LossByApprehend`,
`Test_P4Playthrough_LossByDawn`,
`Test_P4Playthrough_LossByNoVessels`.

### Procgen + bot + telemetry

| Cluster | Tests |
|---|---|
| Procgen | `Test_ProcLevel_BSP`, `Test_ProcLevel_DeterminismCheck`, `Test_ProcLevelScene`, `Test_ProcLevelBootstrap`, `Test_ProcLevel_BuildingWallClosure` |
| Telemetry | `Test_TelemetryRoundTrip`, `Test_TelemetryEdgeCases`, `Test_DPTelemetryHooks`, `Test_DPTelemetryAnalyzer` |
| Heuristic bot | `Test_DPHeuristicBotPlaythrough`, `Test_DPHeuristicBot_Pathing`, `Test_DPHeuristicBot_GoalDispatch` |
| Personality matrix | `PersonalityPlaythrough_Casual`, `_Stealth`, `_Speedrunner`, `_Zealot`, `_Magpie`, `_Relay`, `_Heretic`, `_Trickster` |

### Smoke + per-system tests

`Test_GameRenderHook`, `Test_DPFogPass`, `Test_DimLightsCutFog`,
`Test_Materials`, `Test_HearingFlow`, `Test_LifeTimer`,
`Test_ItemPickup`, `Test_DoubleDoorAndForge`, `Test_GameplaySystems`,
`Test_VisualWiring`, `Test_FullPlaythrough`, `Test_FrontEndPlay`,
`Test_PriestBBBridge`, `Test_PriestPursuit`, etc.
