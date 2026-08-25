# Zenithmon -- Test Plan

**Document purpose:** the load-bearing test contract for Zenithmon. Every
system in [GameDesignDocument.md](GameDesignDocument.md) ships with tests
described here; every stage gate in [Roadmap.md](Roadmap.md) requires the
stage's tests green before it passes. Tests are written against the unified
`zenith test <Game>` harness and produce machine-readable pass/fail results --
no human watches a screen for any batch-suite test to pass or fail.

**Status:** normative from S0 (2026-07-09). Per-system sections below are the
plan of record; counts are targets from the approved project plan. Sections
for future stages describe intent -- cross-check against
`Games/Zenithmon/Tests/` before assuming a named test exists.

**Companion docs:** [Roadmap.md](Roadmap.md) (stage gates),
[Scope.md](Scope.md) (what is in/out), [SaveFormat.md](SaveFormat.md)
(save-schema contract + its test obligations), [CIPolicy.md](CIPolicy.md)
(required checks), [AgentBriefing.md](AgentBriefing.md) (session workflow).

---

## 0. Verification contract

Every test in this plan satisfies:

1. **Deterministic.** Input/time-driven tests set an explicit fixed dt (1/60 by
   default); every RNG consumer takes an explicit seed (see convention C8).
   Same build + same seed = same result, every run. The S3 full door round trip
   deliberately uses a fixed 1/30 presentation dt while retaining the engine's
   normal fixed physics substeps, keeping the multi-scene route bounded without
   making collision integration frame-sized.
2. **Bounded.** Automated tests declare `m_iMaxFrames`; Step returns false on
   completion. The harness never blocks indefinitely.
3. **Self-cleaning.** Entity-owned state cleans itself when scene 0 reloads
   between batched tests; ownerless globals are wiped by the between-tests
   hook (convention C3).
4. **Machine-readable.** Unit tests report through the boot summary ("Unit
   tests complete", failure count); automated tests emit per-test JSON into
   the results dir. An agent parses results without screenshots.

What is assertable:

- Exact values (ints, floats-with-tolerance, entity IDs, enums).
- **Battle correctness = the `ZM_BattleEvent` stream.** The battle engine is
  headless and emits an append-only event stream that is the single source of
  truth for both tests and presentation. Scripted battles assert the exact
  expected stream; the fuzz soak asserts invariants over it.
- Byte equality of baked/generated artifacts (generator determinism).
- Save-blob round-trip equality ([SaveFormat.md](SaveFormat.md)).

What is NOT assertable by the suite: pixel output, "looks right", game feel.
Those are covered by the per-stage **manual visual check** listed in each
Roadmap.md gate (plus the one scripted screenshot capture at S5 -- see 5.6).
There is no audio in the engine, so nothing audio-shaped exists to test.

---

## 1. Tiers and naming

| Tier | What | Mechanism | Naming | Runs |
|---|---|---|---|---|
| **T0 -- boot unit tests** | Pure-logic tests of headless C++ (data tables, battle engine, stats, AI, breeding, tower, save schema, generators-in-memory, WorldSpec integrity) | `ZENITH_TEST(<Category>, <Case>)`, executed at every engine boot before the initial scene loads | Category `ZM_<System>` (e.g. `ZM_Boot`, `ZM_Data`, `ZM_Battle`, `ZM_Save`); files `Tests/ZM_Tests_<System>.cpp` | Always -- every boot, every config, headless and windowed. The CI backbone. |
| **P1 -- automated scene tests** | Scene/flow tests driving the running game through simulated input (traversal, encounter round trips, UI flows, battle smokes) | `Zenith_AutomatedTest` + `ZENITH_AUTOMATED_TEST_REGISTER`; files `Tests/ZM_AutoTests_<Area>.cpp` | `ZM_<Name>_Test` (e.g. `ZM_Boot_Test`, `ZM_VillageWalk_Test`) | `zenith test Zenithmon` batch; graphics-dependent ones auto-skip headless (C5), asset-dependent ones RequestSkip on CI (C6) |
| **Segment playthroughs (S8+)** | Chapter-sized scripted playthroughs (new game to Badge 1, per-region segments) in the CB_HumanSession style: flat action script + probe snapshots + Verify asserts | `Zenith_AutomatedTest`, windowed, thousands of frames | `ZM_Slice_Test` (S8), `ZM_Segment_<Chapter>_Test` (S12) | In the batch suite where budget allows; run explicitly at stage gates via `--filter` |
| **Full playthrough bot (S12)** | New game to Champion, driven end-to-end through input simulation with `zm_instant_battles` | `Zenith_AutomatedTest` with `m_bManualOnly` | `ZM_Playthrough_Test` | NEVER in the default batch or CI -- run explicitly at the S12 gate and on demand |

Unit-test categories planned per stage: `ZM_Boot` (S0, shipped), `ZM_Data` /
`ZM_Stats` / `ZM_World` (S1), `ZM_Battle` / `ZM_AI` / `ZM_Breeding` /
`ZM_Tower` (S2), `ZM_TerrainAuthoring` / `ZM_TerrainRecipeSet` / `ZM_Grass` /
`ZM_OverworldInput` / `ZM_OverworldController` / `ZM_OverworldPhysics` /
`ZM_OverworldCamera` / `ZM_OverworldECS` / `ZM_WorldTraversal` (S3,
shipped), `ZM_Gen` (S4),
`ZM_Encounter` (S5), `ZM_UI` (S6), `ZM_Save` / `ZM_Nav` (S7),
`ZM_Starter` (S8 -- see 5.8). New systems get a
new category, never a grab-bag.

---

## 2. Harness conventions (all mandatory)

Each rule with its one-line why. Violations are review blockers.

- **C1 -- Step uses ONLY `Zenith_InputSimulator` state-setters**
  (`SimulateMousePosition` / `SimulateMouseButtonDown` / `SimulateMouseButtonUp`
  / `SimulateKeyPress` / `SimulateKeyDown` / `SimulateKeyUp` /
  `SimulateTouchDown` / `SimulateTouchMove` / `SimulateTouchUp` /
  `SimulateTouchCancel` / `SimulateGamepad*` / `SimulateMouseWheel`).
  *Why:* Step runs inside the main loop; anything that re-enters the loop
  (`StepFrame`, `SimulateMouseClick`) deadlocks in `vkWaitForFences`.
  **★ C1a -- `SetKeyHeld` IS NO LONGER A USABLE HOLD, AND ITS ABSENCE IS SILENT.**
  It writes the simulator's LEVEL array (`s_abKeyState`) and nothing else, while
  every gameplay reader now goes through the ACTION layer, whose key rows are fed
  by ORDERED TRANSITIONS. A key "held" with it therefore reaches `IsKeyDown` and
  NOTHING ELSE: the player does not move, no assertion fires at the call site, and
  the test fails much later with an unrelated message. Use
  `SimulateKeyDown` / `SimulateKeyUp` -- they set the same level AND queue the
  transition. (The input program's WP3b converted all 150 ZM call sites; this note
  exists so the next one is not re-derived from a red suite.)
  **★ C1b -- A Step CANNOT READ AN EDGE IT INJECTED IN THE SAME STEP.** `Step()`
  runs at `PumpAutomatedTest`, which is BEFORE frame-contract step 7 (the
  injection) and before the action layer closes at 10e. What a Step injects is
  consumed by game logic LATER IN THAT SAME FRAME, and the earliest a Step can
  OBSERVE it is the NEXT frame. Inject on one frame, assert on the next. A test
  that must call a consumer synchronously (driving a component by hand, to pin a
  call site) has to run the contract itself --
  `ZM_BindingsTest::CloseEngineActionFrame()` in `Tests/ZM_BindingsTestRig.h`.
- **C2 -- Input-driven tests set an explicit fixed dt (1/60 by default).** A
  different fixed presentation rate must be named and justified by the test;
  `ZM_PlayerHomeRoundTrip_Test` uses 1/30 while normal physics substeps remain
  enabled.
  *Why:* frame counts convert to seconds deterministically, so timing asserts
  are exact instead of flaky; long routes need not trade determinism for an
  excessive presentation-frame budget.
- **C3 -- Every ownerless global gets a reset in the between-tests hook**
  (registered in `Zenithmon.cpp` via
  `Zenith_AutomatedTestRunner::RegisterBetweenTestsHook` -- the hook grows in the
  SAME PR as each new global system lands. As of ZM-D-177 it runs
  **eleven** resets, in order --
  `ZM_BattleTransition` / `ZM_UI_MenuStack` / `ZM_InteractionRuntime` /
  `ZM_TrainerEngagementLatch` / `ZM_TrainerCinematicLatch` /
  `ZM_GraphNodeTestCounters` / `ZM_GameStateManager::ResetRuntimeStateForTests`,
  then `ZM_GameStateManager::ResetGameStateForTests`,
  `ZM_SetInstantBattlesForTests(false)`, `ZM_SaveSlots::DeleteAllSlotsForTests`
  and finally `Zenith_SaveData::ClearForTest`. Read the hook, not this list, when
  it matters: the ORDER is load-bearing -- `DeleteAllSlotsForTests` precedes
  `ClearForTest` because the latter wipes only the in-memory write log and
  readback stash and deliberately deletes no files, so a `.zsave` written by one
  test would otherwise survive into the next test AND the next process).
  *Why:* batched tests share one process; state that no entity owns leaks
  silently into the next test.
- **C4 -- Rely on the scene-0 force-reload between batched tests** for
  entity-owned state; clean up via OnDestroy, not via the hook.
  *Why:* the harness reloads scene 0 between tests, so entity-owned state has
  a guaranteed teardown path already.
- **C5 -- Set `m_bRequiresGraphics = true` on any test that reads back RENDERED
  OUTPUT** (a `Flux_Screenshot::RequestDump` pixel probe, or state only a real
  GPU produces).
  *Why:* NOT "headless skips Flux" -- a `Null_*` build runs every render path
  against no-op backend calls (see section 3). What it does not do is produce
  PIXELS: `Flux_Screenshot::ConsumePendingDump` has exactly ONE consumer,
  `Zenith_Vulkan_Swapchain::EndFrame`, so on Null the dump is silently never
  written. Such tests auto-skip rather than assert against a file that does not
  exist.
  **★ AND THE FLAG IS NOT FREE -- A SKIP COUNTS AS A PASS.** Setting it makes a
  test CI-INVISIBLE, so anything provable without pixels should be proved by a
  second `m_bRequiresGraphics = false` test that CI can actually see. The
  ZM-D-176 pair is the worked example: `ZM_InteriorTint_Test` (headless,
  material-level) is the gate; `ZM_InteriorTintPixels_Test` (windowed, pixel)
  closes the last mile and is carried by the local gate alone. Reaching for the
  flag because a test "touches rendering" is how coverage goes quiet.
- **C6 -- Asset- or scene-dependent tests exists-guard their inputs and call
  `RequestSkip(szReason)` when absent.**
  *Why:* baked assets are git-ignored -- apart from the six tracked files
  below, a fresh CI checkout has NO baked `Assets/` content, and a hard
  dependency would fail every PR (the engine-wide CI-fix pattern from commit
  `94813489`).
  **DEVIATION -- the two COMMITTED asset families do NOT skip (ZM-D-147).**
  `.gitignore:103-104` re-includes `**/*.znavmesh` and `**/*.zscen` at any
  depth, so `Assets/Navmesh/Dawnmere.znavmesh` and **all five**
  `Assets/Scenes/*.zscen` -- `Battle`, `Dawnmere`, `FrontEnd`, `PlayerHome`,
  `ProfLab` (the fifth added by ZM-D-174) -- are TRACKED. On CI they are always
  present and their absence is a DEFECT, not a condition to tolerate.
  `ZM_NavmeshAsset_Test`, `ZM_DawnmereHeadless_Test` and `ZM_ProfLabWarp_Test`
  therefore carry NO `RequestSkip`. Skipping would also be self-defeating: a
  skip counts as a PASS, so the one gate proving the committed navmesh loads
  would go quiet exactly when it broke. C6 continues to apply to every
  git-ignored family.
  **A DEFENSIVE guard over a tracked file is allowed, and must say so.**
  `ZM_InteriorTint_Test` and `ZM_InteriorTintPixels_Test` DO `RequestSkip` when
  `PlayerHome.zscen`/`ProfLab.zscen` are missing, with the reason stated at the
  callsite: the guard is expected NEVER to fire, and exists so a mangled
  checkout reports a NAMED prerequisite instead of scanning an empty scene and
  reporting a clean subset. That is the opposite of tolerating absence -- but
  it is still a skip-as-pass, so it is only defensible because a second clause
  reds on a scan that observed nothing (see 5.8).
- **C7 -- Test TUs compile directly into the game exe** (`Tests/*.cpp` are
  project sources, never a static lib).
  *Why:* MSVC dead-strips static registrars in unreferenced library objects,
  so lib-hosted tests silently never run.
- **C8 -- Deterministic seeds only.** Battle tests pass explicit seeds to
  `ZM_BattleEngine::Begin(config, seed)`; encounter tests rig the roll;
  generator tests derive seeds from IDs/names. No wall-clock seeding anywhere.
  *Why:* a test that can roll differently on rerun cannot be a gate.

Authoring template: `Tests/ZM_AutoTests_Boot.cpp` is the minimal live example
(Setup zeroes state, Step polls with a frame cap, Verify logs a diagnostic
line on failure). Match its shape: state in an anonymous namespace, Setup
fully re-initialises it (batch mode reuses the process), Verify is read-only
and logs every captured value on failure so an agent can localise the fault
from the log alone.

---

## 3. Runner reference

The unified harness -- there is NO per-game runner script (`c29e28f8` replaced
the old `Tools/run_*_tests.ps1` with thin forwarders onto the shared module and
`ac985ab3` then DELETED them; only `Tools/run_unit_gate.ps1`, which is the T0
gate and not a per-game runner, survives in `Tools/`):

```
zenith test Zenithmon --headless                      # full batch (default mode)
zenith test Zenithmon --filter ZM_Boot_Test           # one test (forces per-process)
zenith test Zenithmon --headless --results-dir <dir>  # explicit JSON output dir
```

Implementation: `Tools/ZenithCli/ZenithCli.psm1` (`Invoke-ZenithTest`) ->
`Tools/ZenithCli/ZenithTestHarness.psm1`.

| Flag | Effect |
|---|---|
| `--filter <substr>` | Run matching tests only; **forces per-process mode** (the in-engine batch flag has no filter) |
| `--headless` | **A CONFIG SELECTOR, not a runtime flag** (`ZenithCli.psm1`): it swaps the exe for the game's `Null_*` build, which defines `ZENITH_NULL_RENDERER` and creates its window hidden. Every render path still RUNS, against no-op backend calls -- Flux is NOT skipped. Only `m_bRequiresGraphics` tests auto-skip. An explicit `--config` always wins |
| `--results-dir <dir>` | Per-test JSON output location |
| `--config <cfg>` | Build configuration override (default `Vulkan_vs2022_Debug_Win64_True`) |
| `--per-process` | One process per test -- slower, bullet-proof against state leaks |
| `--fail-fast` | Abort the batch on first failure |
| `--tests A,B,C` | Ordered single-process run of exactly these tests: the cross-test-leak probe ("does A contaminate B?" in one boot). Composes with nothing else |
| `--batch-order reverse\|rotate:N` | Reorder the full batch without changing its set -- the other half of the leak probe. Requires the full batch (no `--filter`/`--per-process`/`--fail-fast`) |
| `--tier <N>` / `--build` / `--exit-after-frames <N>` / `--assertions-log <f>` | Tier select; build first; per-test frame ceiling (default 8500); assertion-log path |

| Exit code | Meaning |
|---|---|
| 0 | All tests passed (or skipped) |
| 1 | Usage error |
| 2 | Validation error |
| 3 | Generation (regen) failure |
| 4 | Build or test failure |
| 5 | Game not found |

**Batch vs per-process:** batch (default) boots the engine once and runs every
registered test in one process -- minutes instead of tens of minutes -- which
is why conventions C3/C4 (state hygiene) are load-bearing. Per-process is the
fallback when chasing a suspected cross-test leak. **Test DISCOVERY always uses
the Null exe** regardless of `--config`, because listing from a Vulkan build
hangs in `vkEnumeratePhysicalDevices` on a GPU-less runner -- so a windowed run
still needs `zenith build Zenithmon --headless` to have happened at least once.

### ★ STANDING HAZARD -- a red run's diagnostic is NOT in its result JSON

Two independent facts compose into a trap that has now cost two sessions
(first booked at ZM-D-174, hit again at ZM-D-176/177):

1. **The engine hardcodes `"failures": []`.** `Zenith_AutomatedTest.cpp:778`
   writes that literal into every per-test JSON, pass or fail; the header says
   so in as many words ("there is no per-test mechanism for structured failure
   messages yet, only the pass/fail bool. Tests that need detail today print to
   stdout instead"). A JSON with `"passed": false` and `"failures": []` is the
   NORMAL shape of a failure, not a corrupt artifact.
2. **The per-process branch discards child stdout.**
   `ZenithTestHarness.psm1:435` runs the exe as `2>&1 | Out-Null`. Because
   `--filter` FORCES per-process, the single most common way to run one test
   is exactly the way that throws its `Zenith_Log`/`Zenith_Error` output away.
   (The batch branch, line 348, tees to host and does keep it.)

**Consequence, stated so nobody re-derives it:** a focused windowed
`--filter` run tells you only *whether* it failed. To read WHY, run it without
`--filter` (batch, output teed) or invoke the exe directly with
`--automated-test <name>`. A test whose only evidence is a number it printed
must have that number recorded by hand at the gate, or the test has told nobody
anything -- which is why the calibrated figures in 5.8's pixel probe are
labelled as measured OUT OF BAND.

Unit tests run before scene load on an ordinary game boot, but `zenith test`
deliberately launches the game with `--skip-unit-tests`; the harness does not
exercise T0. The dedicated `Tools/run_unit_gate.ps1` boot is therefore the T0
gate. It asserts the exact registered count as well as zero failures from the
"Unit tests complete" log line.

---

## 4. CI -- the `zm-tests` gate

Workflow: `.github/workflows/zm-tests.yml` (clone of `dp-tests.yml`), active
since S0. Steps: checkout -> `zenith-setup` action (Vulkan SDK 1.3.290.0,
Slang 2026.1) -> `Build/regen.ps1 -UseDotnet` -> build
`Vulkan_vs2022_Debug_Win64_True` (`/t:Zenithmon`) -> build
**`Null_vs2022_Debug_Win64_True`** (THE headless/CI build -- the exe every
later step actually runs) -> build `D3D12_vs2022_Debug_Win64_False`
(backend-neutrality link proof) -> DLL copies -> headless boot check
(`--list-automated-tests --skip-tool-exports --skip-unit-tests` on the Null exe)
-> **boot unit-test gate** (`Tools/run_unit_gate.ps1 -Exe <Null exe> -Baseline N
-TimeoutSec 600` -- the ONLY CI step that runs the ZENITH_TEST T0 suite, since
`zenith test` and the boot check both pass `--skip-unit-tests`) ->
`zenith.bat test Zenithmon --headless --results-dir
Build/artifacts/test_results/zenithmon` (the P1 automated suite) -> upload
artifact `zm-test-results` (`if: always()`). **THREE configurations are built,
not two** -- omitting the Null build from a mental model of this gate is how one
concludes the headless suite runs on the Vulkan binary, which it never does.

`-TimeoutSec 600` is **not optional** and the helper's own 180 s default is
actively wrong here: the watchdog kills the known tools-build idle only AFTER
the units line is logged, so it doubles as a ceiling on how long the suite may
take, and the suite has been measured straddling 180 s (ZM-D-163). Losing that
race reports "no 'Unit tests complete' line in boot output", which reads like a
crash.

The CI runner is GPU-less and asset-less (assets are git-ignored), so the CI
backbone is exactly the T0 suites plus the headless-safe P1 tests; everything
graphics- or asset-dependent skips via C5/C6 and runs locally at stage gates
instead. Registering `zm-tests` as a required branch-protection check is a
manual GitHub-UI step -- tracked in
[ManualSetupChecklist.md](ManualSetupChecklist.md); policy detail in
[CIPolicy.md](CIPolicy.md).

---

## 5. Per-system test specs

Counts are targets from the approved plan; they set the bar for stage gates,
not a hard ceiling. Test names below are illustrative until their stage lands.

**★ THIS SECTION IS NOT AN INVENTORY, AND THE GAP RUNS BOTH WAYS.** The document
header warns that a named test may not exist yet. The commoner failure is the
reverse: **a registered test this document never names.** `Tools/doc_lint.ps1`
hardcodes DevilsPlayground's `Docs/` directory and never reads Zenithmon's, so a
green doc-lint is ZERO evidence about this file -- it is unlinted and drifts
silently. As of ZM-D-177, **eleven** of the then-56 registered automated tests appear
nowhere below: `ZM_MenuOpenClose_Test`, `ZM_DialogueTalk_Test`,
`ZM_PartyScreen_Test`, `ZM_DexScreen_Test`, `ZM_BagScreen_Test`,
`ZM_ShopScreen_Test`, `ZM_CareCenterHeal_Test` (all
`Tests/ZM_AutoTests_UI.cpp`), `ZM_BattleHUD_Test`,
`ZM_BattleArenaOwnScene_Test`, `ZM_GameStatePersistence_Test` and
`ZM_TerrainGrassResumeRegen_Test`. Their contracts live in their own files'
header comments. (The registry is **55** since ZM-D-181 deleted
`ZM_NpcRenderedPalette_Test`; the eleven above are unaffected.) **The authoritative registry is
`grep -c ZENITH_AUTOMATED_TEST_REGISTER Games/Zenithmon/Tests/*.cpp`, and the
authoritative unit inventory is `grep -c "ZENITH_TEST("` -- never a total quoted
in prose here, including a total quoted in this paragraph.**

### 5.0 S0 -- skeleton (SHIPPED)

- T0: `ZM_Boot` x2 (`ProjectNameIsZenithmon`, `GameAssetsDirectoryIsNonEmpty`)
  in `Tests/ZM_Tests_Boot.cpp`. `GameAssetsDirectoryIsNonEmpty` asserts the
  DESKTOP contract; on Android it asserts the inverse (`GAME_ASSETS_DIR` is
  compiled in as `""` so AAssetManager can address APK assets relatively), as
  does `ZM_Nav::DawnmereNavmeshBakePathIsAbsoluteAndTyped`.
- P1: `ZM_Boot_Test` (`Tests/ZM_AutoTests_Boot.cpp`) -- FrontEnd scene boots
  and `ZM_GameComponent` resolves.
- Verified at S0: build green; `zenith test Zenithmon --headless` = 1 passed /
  0 failed; boot unit summary 0 failed.

### 5.1 S1 -- data core (~90 unit tests)

Category `ZM_Data` / `ZM_Stats` / `ZM_World`:

- **Type-chart matrix vs golden file:** the full 18x18 effectiveness matrix
  asserted against a golden table compiled into the test TU (not a disk
  asset), so any chart edit is a deliberate two-place change.
- **Stat formula vectors:** Gen-III+ stat formula against hand-computed
  vectors (level 1 / 50 / 100 x IV 0/31 x EV 0/252 x nature up/down/neutral,
  HP vs non-HP special cases).
- **Exp formula vectors:** all 4 exp-curve families at known levels; curve
  monotonicity; level-from-exp inverse consistency.
- **Registry integrity (`ZM_DataRegistry`):** every species/move/item/ability/
  nature row has a valid name, in-range IDs, no duplicate names, every species
  move-learn entry references a real move, every evolution target exists,
  base-stat and rarity ranges valid.
- **WorldSpec referential integrity:** every warp target build index and spawn
  tag resolves, every encounter-table species exists, every trainer's team is
  valid, every gym/shop/story reference resolves. This suite is the schema
  enforcer that keeps ~40 scenes honest before anything is baked.

### 5.2 S2 -- battle engine (~370 unit tests)

Category `ZM_Battle` / `ZM_AI` / `ZM_Breeding` / `ZM_Tower`. The single
biggest suite; all headless, all seeded (C8).

- **Scripted seeded scenarios with exact event streams:** fixed teams + fixed
  seed + submitted actions -> assert the exact `ZM_BattleEvent` sequence
  (damage numbers, order, status procs, faints, exp awards). These are the
  characterization bedrock for every later battle change.
- **2000-battle fuzz soak:** random-but-seeded teams/actions; per battle
  assert termination in < 500 turns and invariants throughout: HP in
  [0, max], PP in [0, max], stat stages in [-6, +6], no acting fainted
  monster, event stream well-formed. One test, thousands of battles, minutes
  budget -- the soak owns the long tail that scripted cases cannot enumerate.
- **Per-status units:** each major status (sleep/poison/toxic/burn/paralysis/
  freeze) and each volatile (confused/flinch/leech seed/protect/charge/
  semi-invulnerable/recharge/lock/trap/taunt): application, block conditions,
  turn-end effects, cure paths, persistence rules.
- **Per-move-effect units:** each of the ~60 `ZM_MOVE_EFFECT` kinds gets at
  least one dedicated scenario (plus representative table rows through it).
- **Per-ability units:** each shipped ability hook (OnSwitchIn/OnModifyStat/
  OnModifyDamage/OnStatusTry/OnContact/OnTurnEnd/OnFaint/OnAccuracy/...)
  proven to fire and to not fire.
- **Damage/catch/turn-order math:** damage pipeline stages (STAB, type
  effectiveness, crit 1/24, 85-100 roll, burn, weather, screens) each
  isolated; catch formula 4-shake checks against known probabilities with a
  rigged RNG; priority brackets + speed ties resolved by seeded rng.
- **AI tier properties:** RANDOM (uniform over legal), GREEDY (picks max
  expected damage x accuracy in constructed positions), SMART (takes the KO,
  switches out of hopeless matchups, uses heals), CHAMPION (2-ply lookahead
  beats the greedy line in trap positions). Property-style asserts on
  constructed states, not win-rates.
- **Exp/EV/level/evolution:** exp splits, EV caps, mid-battle level-up +
  move-learn, post-battle evolution checks (pure `Evolve()`).
- **Breeding:** egg-group compatibility, mother's-base-evo species, 3-IV
  inheritance (5 with destiny knot), everstone nature lock, step counting +
  hatch.
- **Battle Tower logic:** level-50 clamp, rental/opponent generation scaling
  with streak, AI-tier escalation, boss every 7th, streak update rules.

### 5.3 S3 -- first overworld

- **E1 engine unit tests (SHIPPED -- exactly seven):**

  | Test | Contract covered |
  |---|---|
  | `Terrain::AssetSetDefaultUsesLegacyMeshDirectory` | Empty-set legacy path, strict name grammar and boundary cases, path containment, transactional rejection |
  | `Terrain::AssetSetNamedDirectoryPropagatesToStreaming` | Named direct-child resolution and propagation into streaming state |
  | `Terrain::AssetSetIsolatedAcrossComponentsAndMove` | Per-component isolation plus move construction/assignment authority |
  | `Terrain::AssetSetSerializationRoundTrip` | Exact v4 append-after-v3 layout, named round trip, invalid-v4 safe fallback |
  | `Terrain::AssetSetLegacyV3DefaultsEmpty` | Exact v1/v2/v3 reads default to the empty legacy set without over-consuming framed data |
  | `Terrain::EditorAssetSetResolvesLegacyAndNamedBakeDirectories` | Legacy/named mesh and texture targets, safe mesh-only cleanup, missing-map defaults, dirty-session resume and target reset |
  | `Terrain::EditorAutomationTerrainAssetSetActionOwnsArgument` | Owned automation argument, executed set action, fresh-component stamping and scene serialization |

  All seven are engine-side `ZENITH_TEST` cases. At the S3 closure they were
  count-ratcheted into the then-current shared engine unit gate (**1078**
  registered) and Zenithmon CI boot unit gate (**1773** registered); the latter
  expected 1772 passed, 0 failed and the one quarantined skip. Those are
  historical S3 values. **This document does not restate the current ones** --
  the pins are `Tools/run_unit_gate.ps1`'s default (engine-only) and
  `.github/workflows/zm-tests.yml`'s `-Baseline` (combined), with `Status.md`'s
  CURRENT BASELINE block as the readable record. The S7 item 1 SC2 figures that
  used to be quoted here as "current" (1103 / 2392) were 2026-07-21 values and
  had drifted badly by 2026-08-01.
- **E2 engine unit tests (SHIPPED -- exactly three):**

  | Test | Contract covered |
  |---|---|
  | `TerrainEditor::ChunkExportRectUsesInclusiveBounds` | Transactional inclusive bounds `0 <= min <= max < 64` containing anchor `(0,0)`; compact fixed-grid coordinate enumeration; exact `3 * area` file count; signed automation payload/routing; invalid preflight has no editor, component, directory, cleanup, or streaming-state side effects |
  | `Terrain::StreamingMissingHighLODSourceDoesNotEvictOrAllocate` | Missing or malformed HIGH source is parsed through the bounded canonical terrain-mesh reader, returns without assertion/allocation/eviction, and preserves LOW residency, allocators, stats, and dirty state |
  | `Terrain::StreamingUnavailableHighLODDoesNotRetryOrStarve` | Missing HIGH sources become terminal `SOURCE_UNAVAILABLE`; the 32-probe source budget is independent of the eight-upload budget, retries are suppressed, and a later valid candidate is not starved |

  Rect export is a crop of the fixed 64x64 grid, not a resize (E6 remains
  deferred). The production editor path validates the rectangle before any
  mutation, resolves a canonical contained target, removes only direct
  generated `.zmesh` children, then writes exactly the HIGH, LOW and physics
  file for every selected chunk. Streaming warns once when classifying a
  missing/invalid HIGH source; terrain teardown/regeneration resets the
  terminal state so a newly baked source can be tried again. The three E2
  cases historically raised the shared engine baseline 1075 -> 1078 and
  Zenithmon's exact boot baseline 1725 -> 1728; Dawnmere's four game units
  then raised only the Zenithmon baseline to 1732, and the five measurement-
  registry units below raised it to 1737; the 20 overworld input/controller/
  physics/camera/ECS units below raised it by 20; the first 12 traversal units
  raised it to 1769, and the four fade/round-trip units below raised it to the
  then-current S3 closure value of **1773**.
- **Dawnmere terrain/grass unit tests (SHIPPED -- exactly four):**

  | Test | Contract covered |
  |---|---|
  | `ZM_TerrainAuthoring::DawnmereRecipeIdentityAndBounds` | WorldSpec identity; stable seed `0x7BF32CA4`; authored world `0..1024`; inclusive 16x16 export; contained paths/pads/landforms/material/camera bounds |
  | `ZM_TerrainAuthoring::DawnmereRecipePlanIsDeterministicAndContained` | Byte-stable ordered reset/set/procedural/landform/flatten/erosion/splat/grass/terminal plan; no out-of-world dab; grass erase remains terminal density phase |
  | `ZM_TerrainAuthoring::DawnmereManifestRequiresEveryOutput` | Exact 256-each `Render`/`Render_LOW`/`Physics` + 3-texture enumeration; all 771 required non-empty files; 12-byte `ZMTR` v1/count-771 marker; every missing output and wrong marker field invalidates warm state; prepared-path containment |
  | `ZM_Grass::GrassDensityMapValidatesAndSamples` | Canonical named-set density path; exact 1024x1024 `R32_SFLOAT` decode; clamped 4096 m world sampling; malformed input clears state |

  A cold/forced bake publishes the marker only after all 771 outputs validate,
  making the ignored terrain family exactly **772 files** including the marker;
  scene authoring is deferred until a valid warm boot and writes ignored
  `Dawnmere.zscen`. ZM-D-053's historical first observations remain **63.671 s**
  cold and **14.614 s** warm graphics. The calibrated three-recipe study later
  reran Dawnmere under a common harness at **59.035 s**; it does not rewrite
  that original result. No trees are generated or tested in this deliverable.
- **Three-recipe measurement unit tests (SHIPPED -- exactly five):**

  | Test | Contract covered |
  |---|---|
  | `ZM_TerrainRecipeSet::RegistryHasExactlyThreeWorldSpecRecipesInFixedOrder` | Exact Dawnmere/Thornacre/Route1 registry count and WorldSpec order; town/route kinds, build indices, asset-set names, stable seeds, crop classes, required-output counts |
  | `ZM_TerrainRecipeSet::RecipesCarryDistinctDocumentedOutdoorPlans` | Distinct town/route authored geometry, paths, pads, material/auto-splat rules, grass fields, landmarks, and cameras remain real recipe content rather than cloned measurement fixtures |
  | `ZM_TerrainRecipeSet::PlansAreDeterministicContainedAndEndWithGrassErase` | Rebuilt plans compare exactly; every operation stays inside its authored world; density-path erase remains before the checked terminal bake |
  | `ZM_TerrainRecipeSet::OutputsAreUniqueSetContainedAndQueuePolicyIsPure` | Every output is unique and contained; AUTO_MISSING, FORCE_ALL, and exact-case FORCE_SELECTED parsing/selection are pure; invalid and duplicate selector forms fail closed |
  | `ZM_TerrainRecipeSet::ManifestsEncodePerRecipeCountsAndInvalidateMissingOrEmptyOutputs` | Town markers encode count 771, Route1 encodes 1,155; all required files must be non-empty; missing/empty output and malformed marker invalidate warm state |

- **Three-recipe measurement commands/evidence (2026-07-13, ZM-D-054):**
  run the default Tools-enabled Vulkan executable serially and record process
  wall time around each direct boot. The calibrated invocations were equivalent
  to the following PowerShell command shapes (without `--skip-tool-exports`, so
  every wall includes the same normal boot/export overhead):

  ```powershell
  $exe = 'Games/Zenithmon/Build/output/win64/vulkan_vs2022_debug_win64_true/zenithmon.exe'
  & $exe --zm-force-terrain-bake=Dawnmere --skip-unit-tests *> Build/artifacts/terrain_measure_dawnmere.log
  & $exe --zm-force-terrain-bake=Thornacre --skip-unit-tests *> Build/artifacts/terrain_measure_thornacre.log
  & $exe --zm-force-terrain-bake=Route1 --skip-unit-tests *> Build/artifacts/terrain_measure_route1.log
  & $exe --skip-unit-tests *> Build/artifacts/terrain_measure_all_warm.log
  ```

  | Set/run | Process wall | Production recipe timer | Chunks | Family files | Family bytes | Result |
  |---|---:|---:|---:|---:|---:|---|
  | Dawnmere selected cold | **59.035 s** | **42.588 s** | 256 | 772 | 204,684,116 | exit 0; validated marker |
  | Thornacre selected cold | **69.979 s** | **53.657 s** | 256 | 772 | 204,684,116 | exit 0; validated marker |
  | Route1 selected cold | **80.804 s** | **64.541 s** | 384 | 1,156 | 262,985,940 | exit 0; validated marker |
  | All warm | **16.874 s** | n/a | 896 total | 2,700 total | 672,354,172 total | exit 0; warm mask `0x7`, queue mask `0x0` |

  The 11-town + 14-route planning model projects **24,676 files /
  5,933,328,436 bytes**, conservative repeated-process **30m 40.833s**, and
  one-boot/net **23m 55.857s**. The GDD 11-town + 15-route sensitivity projects
  **25,832 files / 6,196,314,376 bytes**, **32m 01.637s** repeated and **24m
  59.787s** net. The 30-50 minute target is eventual **all-assets** cold time,
  not a terrain-only acceptance ceiling; no byte cap exists, warm "seconds" is
  qualitative, and one measured route does not create a statistical upper
  bound. Thornacre/Route1 have no authored playable scenes or trees.
- **Input/controller/camera unit tests (SHIPPED -- exactly 22):** all live in
  `Tests/ZM_Tests_Overworld.cpp` and split **5 / 4 / 5 / 6 / 2**.
  The camera category moved 4 -> 6 at ZM-D-173; both totals are counted from
  that file's actual `ZENITH_TEST` inventory rather than carried forward from
  a plan:

  | Category (count) | Locked contract |
  |---|---|
  | `ZM_OverworldInput` (5) | WASD and arrow aliases; opposite-axis cancellation; pressed-edge Enter/Space confirm, Escape/Backspace cancel, M/Tab menu; either Shift held for run |
  | `ZM_OverworldController` (4) | Camera-forward flattening + normalized diagonals; 4/7 m/s horizontal-world speed with vertical preservation; walkable-downhill tangent adhesion preserves stronger falls and positive step-assist rises; inclusive 45-degree classification and steeper-uphill blocking; step qualification requires lower obstruction, upper clearance, a walkable landing and rise <=0.40 m |
  | `ZM_OverworldPhysics` (5) | Real dynamic generic capsule grounds/falls/stays upright; walk/run/release drives real velocity; invalid/nonpositive dt is a full observable/animation/body/facing no-op; static wall blocks; Jolt ramp normals classify slopes; low-step query accepts while tall obstacle rejects without reboosting an existing rise |
  | `ZM_OverworldCamera` (6) | Fixed-heading desired pose; omega-8 critical spring has no overshoot; collision padding/minimum-arm clamp; a real occluder pushes inward and recovery moves outward; **(ZM-D-173)** a SENSOR volume straddling the arm ray neither clamps it nor reports the camera constrained; a SOLID occluder behind a sensor still clamps -- to the distance derived from the SOLID's near face, which is only reachable if the ray passed through the sensor |
  | `ZM_OverworldECS` (2) | Version-1-only component serialization; unique orders 102/103 plus lifecycle, generation-safe same-scene target reacquisition, rejection of still-live cross-scene cached targets, and missing-dependency safety |

  The runtime under test uses the transform scale to create a dynamic upright
  Jolt capsule, never a gameplay `SetPosition`; it drives camera-relative
  horizontal-world speed at 4 m/s walk or 7 m/s run, accepts slopes through 45
  degrees, and applies only a bounded upward-velocity assist to qualified steps.
  Grounded walkable downslopes receive velocity-only tangent adhesion without
  replacing a stronger fall or positive step-assist rise; invalid/nonpositive
  dt is a true controller no-op.
  The fixed-yaw follow camera uses a critical spring and collision arm, and
  resolves `Player` through a generation-bearing `EntityID` in its own scene,
  rejecting a cached target whose still-live ID has moved to another scene.
- **Traversal/fade unit tests (SHIPPED -- exactly 16):** all live in
  `Tests/ZM_Tests_WorldTraversal.cpp` under `ZM_WorldTraversal`:

  | Count | Exact cases / locked contract |
  |---:|---|
  | 4 | `ManagerSingletonLookupRejectsMissingAndDuplicates`, `ManagerRequestValidationIsTransactional`, `ManagerStateMachineDefersSingleLoadOneTick`, `ManagerPersistenceKeepsEntityIDAcrossSceneGeneration`: unique generation-bearing singleton, duplicate retirement, WorldSpec validation, FrontEnd build-0 playerless exception, immediate source freeze, exactly-one deferred SINGLE request, explicit waiting states, and persistent full-ID identity across scene-slot reuse |
  | 3 | `SpawnPointTagValidationBoundaries`, `SpawnPointLookupRequiresUniqueSameSceneMatch`, `SpawnPointSerializationVersionRoundTripAndLegacyFallback`: 1-31 printable ASCII bytes in a fixed 32-byte buffer, exact-case unique same-scene lookup, duplicate/missing failure, and fixed v1 scene-component stream |
  | 3 | `WarpTriggerConfigurationAndVersionRoundTrip`, `WarpTriggerFiltersSensorOtherBodyAndNonPlayer`, `WarpTriggerLatchRequestsExactlyOnceAndResetsForNextOverlap`: validated fixed v1 stream, sensor reassertion, real sensor pass-through, only the unique active-scene valid dynamic-capsule Player accepted, one request per overlap, and latch reset only for the exact full generation-bearing ID after the manager is reset |
  | 2 | `PlacementUsesMarkerFeetPlusContractCapsuleHalfExtentAndZeroesMotion`, `WarpResolutionFreezesThenResetsOnMissingDuplicateAndGenerationChange`: feet-marker centre derivation (and that it is SCALE-INDEPENDENT -- re-scaling the player must not move it), one-time teleport, zero linear/angular velocity, reset-enable-idle completion, destination `OnStart` freeze, missing/duplicate marker hold, and entity/scene generation changes without stale-ID acceptance |
  | 1 | `FadeAdvanceClampsInvalidDtAndRuntimeReset`: 0.20 s alpha policy, invalid/nonpositive-dt no-op, persistent `WarpFade` authoring/reload reassertion and reset; plus the production global quad queue's ascending cross-canvas sort, stable equal-key order, actual `Zenith_UICanvas` sort-key forwarding, 1024-quad drop-newest capacity guard, highest-sort text-overlay clip arbitration, complete pending-queue + bg/fg/total-counter + clip drain across legacy/render-graph Text disabled/reset paths, and clean re-enable; also proves `Flux_ModelInstance` material-handle retain/release and registry reclamation after procedural empty-model `Zenith_ModelComponent` deserialization, plus direct zero-duration UIOverlay Show/Hide synchronous opacity/visibility/interaction behavior |
  | 1 | `FadeOutBlocksSingleLoadUntilOpaqueAndIssuesExactlyOnce`: the source freezes immediately, no SINGLE request issues below alpha 1, a 0.25 s one-tick opacity crossing renders the real manager canvas inside the load callback and requires an actual sort-10000 alpha-1 fade quad before permission, and opaque waiting updates cannot issue twice |
  | 1 | `PlacementAndCameraReadinessStayLockedBeforeFadeIn`: placement/motion reset occurs behind black; missing, unstarted, duplicate, non-main, or wrong-target cameras hold opacity/input; exactly one main Camera + `ZM_FollowCamera` targeting the replacement generation begins fade-in |
  | 1 | `FadeInUnlocksAndRuntimeStateIsNotSerializedWithMissingDependenciesSafe`: camera/readiness loss returns to opaque lock, a missing exact overlay fails closed, input unlocks only at alpha 0, and v1 component serialization still omits all live fade/transition state |

  The manager-only FrontEnd `ZM_GameStateRoot` is the sole persistent entity;
  scene-owned Player/camera are replaced by SINGLE loads. Traversal component
  streams are v1 authoring data only: runtime transition state is intentionally
  absent and this does not implement the S7 `ZM_SaveSchema`.
- **P1 `ZM_ControllerHarness_Test` (SHIPPED, headless-safe):** builds an
  asset-free isolated floor/player/camera fixture, settles the dynamic capsule,
  drives 60-frame walk and run phases through input state-setters, verifies
  release stops horizontal motion, and checks follow-camera acquisition and
  finite numeric invariants. It is one of the two P1 tests that execute and
  pass on the headless runner.
- **P1 `ZM_DawnmerePlayerCamera_Test` (SHIPPED, windowed):** graphics-required
  and exists-guarded for the ignored Dawnmere scene/terrain family. It verifies
  the authored Player near centre `(512,26.88577,480)`, grounded movement on the
  real baked surface, camera FOV/arm/acquisition, grass readiness, a SINGLE
  Dawnmere reload with new entity generations and same-scene reacquisition,
  resumed movement, and FrontEnd teardown. Final S3 gate: **117 frames /
  6212.128 ms** (**18.712 s** process wall).
- **P1 `ZM_GrassRegeneration_Test` (SHIPPED, windowed):** exists-guards the
  ignored Dawnmere scene/terrain family and is tagged graphics-required. It
  loads Dawnmere, verifies the CPU/Flux 1024-square density-map contract and
  density scale 0.15, observes exactly **200,159 blades from 5,133 terrain
  triangles**, reloads the same scene and requires the identical count with no
  accumulation, then returns to FrontEnd and requires the Flux density map and
  visible grass chunks to be clear. Final S3 gate: **11 frames / 2579.674
  ms** (**15.125 s** process wall).
- **P1 `ZM_WarpInfrastructure_Test` (SHIPPED, windowed):** graphics-required
  and exists-guarded. It starts playerless in FrontEnd build 0, requests
  `(2,"TownCenter")` directly through the persistent manager, proves the
  SINGLE request remains blocked until the 0.20 s fade reaches full opacity,
  then proves the replacement destination Player stayed frozen through
  placement/camera readiness, arrived at exact centre
  `(512,26.88577,480)`, had zero linear/angular velocity, was reset-enabled,
  and left the same generation-bearing manager idle after fade-in. This remains
  the focused infrastructure case; the live trigger route is covered by the
  next test.
  Final S3 gate: **29 frames / 2008.714 ms** (**14.869 s** process wall); the
  synchronous overlay handoff removes the former extra UI-update frame.
- **P1 `ZM_PlayerHomeRoundTrip_Test` (SHIPPED, windowed):** graphics-required,
  max 1,800 frames, and exists-guarded for FrontEnd/Dawnmere/PlayerHome plus the
  ignored terrain family. At fixed 1/30 presentation dt it bootstraps through
  the manager, drives the real Dawnmere capsule with input state-setters to the
  authored `HomeDoorTrigger`, SINGLE-loads build 40 at `Door`, drives through
  `PlayerHomeExitTrigger`, and returns to Dawnmere at `FromHome`. Every leg
  proves collision latch, fade-out/opaque-load/camera-barrier/fade-in ordering,
  exact scene/entity generation replacement, spawn feet/centre placement,
  zero controller/body motion before unlock, persistent manager identity,
  terrain/grass absence inside, and grass/camera recovery outside. Final terrain
  contact settlement permits only 5 cm downward Y while XZ remain exact. Final
  S3 gate: **673 frames / 14662.601 ms** (**27.514 s** process wall).
- **Automated registry/headless result:** all **6** P1 tests register. Headless
  passes `ZM_Boot_Test` and `ZM_ControllerHarness_Test`; graphics-required
  `ZM_WarpInfrastructure_Test`, `ZM_GrassRegeneration_Test`, and
  `ZM_DawnmerePlayerCamera_Test`, and `ZM_PlayerHomeRoundTrip_Test` skip as
  designed: **2 semantic passes + exactly 4 graphics-required skips**. The
  definitive headless batch ran **6/6 in 1.590 s** wall: Boot **1 frame / 0.018
  ms** and ControllerHarness **142 / 25.100 ms** passed semantically.
- **Definitive post-overlay-hitch regression evidence:** the full boot gate is
  **1773 ran / 1772 passed / 0 failed / 1 skipped**, with **180.640 s** helper
  wall under the canonical watchdog. The major gate builds all four Vulkan
  Debug/Release x Tools true/false configurations plus D3D12 Debug Tools=false:
  regen **2.401 s**, then Vulkan Debug true, Debug false, Release true, Release
  false, and D3D12 Debug false build walls of **11.225 / 11.755 / 11.213 /
  11.031 / 7.656 s** respectively. Because the fade/lifecycle fix is
  engine-global,
  RenderTest rebuilt in **6.192 s**;
  `EngineBootShutdownSmoke` passed **1 / 28.606 ms** (**40.622 s** wall) and
  `TerrainEditorSmoke` **151 / 5291.193 ms** (**46.025 s** wall). The ignored
  `Build/artifacts/zenithmon/s3/final/post_overlay_hitch_fix/` root contains **12
  parsed JSON / 12 passed / 0 failed**, exactly four being graphics-required
  headless skips. Direct instantaneous-overlay Show/Hide and
  real-quad-before-load assertions extend existing T0 cases, so baselines remain
  **1773 units / 16 `ZM_WorldTraversal` / 6 P1**.
- Stage-gate manual visual check: terrain/grass/camera and the PlayerHome
  blockout, using the ignored captures under
  `Build/artifacts/zenithmon/s3/visual/`. Capture
  `capture_final_posthitch_20260713_183717` passed the definitive binary's round
  trip in **673 frames / 14619.2 ms** with exit 0 and produced three valid,
  ignored, inspected 1280x720 PNGs whose SHA-256 values are recorded in
  Status.md and AssetManifest.md. The automated gate is green; human visual
  acceptance remains deliberately unchecked.

### 5.4 S4 -- asset generators

Category `ZM_Gen` (headless, in-memory -- no disk dependency, CI-safe). The S4
foundation (`ZM_GenCommon` + `ZM_TextureSynth`, ZM-D-059) shipped **31** `ZM_Gen`
units (boot gate 1773 -> 1804); the creature generator below adds **43** more
(1804 -> 1847), the creature-animation generator below adds **19** more
(1847 -> 1866), the human generator below adds **20** more (1866 -> 1886), the
building generator below adds **10** more (1886 -> 1896; 9 units + 1 bake smoke),
the prop generator below adds **9** more (1896 -> 1905; 8 units + 1 bake
smoke), and `ZM_BakeManifest` below adds the final **3** more (1905 -> 1908;
`EnumerationMatchesRoster` all-config + `RebakeByteIdentical`/`GuardWarmStale`
tools-only). That accounting totals **135** units at the S4 close, eight of which
(the six generator bake smokes plus the two `ZM_BakeManifest` tools-only cases)
are `ZENITH_TOOLS`-only, so `_False`/Android configs register them as empty TUs.
**The live `ZM_Gen` count is 137** -- known-limit W4 (ZM-D-160, section 5.7)
appended `HumanGen_PaletteTotality` and `HumanGen_PaletteDistinctness` to
`Tests/ZM_Tests_HumanGen.cpp`, so its `ZM_Gen` inventory is 20, not the 18 the
human-generator box below itemises. The category is not frozen at S4; count it
from the files.

#### ZM_Gen -- creature generator (SHIPPED)

`ZM_CreatureGen` (ZM-D-060..065) turns any `ZM_SPECIES_ID` into a deterministic
bundle (skinned mesh + skeleton + BC1 albedo + hue-rotated shiny + flat dex
icon); a tools build additionally bakes the full 15-file per-species bundle --
the 9-file core plus the 6 `.zanim` clips added by `ZM_CreatureAnimGen`
([AssetManifest.md](AssetManifest.md) 1.2). **All 8 archetype builders are
wired, so every one of the 152 species builds a valid creature and a complete
bundle.** Determinism is golden-pinned (`uZM_CREATUREGEN_VERSION` = 3). All units are
pure/headless (the `.zmesh`/`.ztxtr` bake bridges are compiled out) unless marked:

- **Generic 12-invariant harness** (`Tests/ZM_Tests_CreatureGen.cpp`, 18 units):
  a contract-driven harness authored against the frozen seam (never a specific
  archetype `.cpp`) that loops every species with a wired builder -- now all 152
  -- and asserts the twelve universal creature invariants:
  1. same-seed determinism -- `ZM_CreatureBuildEqual` + equal
     `ZM_CreatureContentHash` + `ZM_CreatureMeshEqual` (curated subset, for cost);
  2. per-domain seed isolation -- each `m_aulDomainSeed[d]` == the frozen
     `ZM_GenDeriveSeed(...)`, pairwise-distinct (all 152);
  3. outward winding (CCW, `cross(C-A, B-A)` faces outward);
  4. non-degenerate bounds inside a sane world box;
  5. skin weights sum to 1;
  6. <= 2 non-zero bone influences per vertex;
  7. bone caps -- <= 30 (`uZM_GEN_CREATURE_BONE_CAP`) AND <= 100 (engine max);
  8. in-range bone indices + a well-formed single-root, parent-before-child,
     name-resolvable skeleton;
  9. shiny differs from base albedo at matching dims over a single shared mesh;
  10. dex icon non-empty, 128^2, >= 2 distinct texels;
  11. seed/evo sensitivity -- two distinct species differ; stage-1 != stage-3;
  12. skeleton topology IDENTICAL across evo stages (equal bone count + equal
      per-index names, over every multi-stage buildable family -- the index-keyed
      clip-transfer precondition).

  Plus the SC1-core cases: recipe-resolution purity, pure+total archetype
  dispatch (`CreatureGen_ArchetypeDispatch`), and the shared
  `ZM_CreatureArchetypeCommon` kit-helper units (spine tube / limb / tail / horn
  / ellipsoid head). The cheap mesh-only structural pass (3-8) runs over EVERY
  buildable species; the heavy full-bundle passes (1, 9-12) reuse a single build
  over a curated representative subset for cost.

- **All-152 coverage gate** (`CreatureGen_AllSpeciesBuildable`): asserts EVERY
  `ZM_SPECIES_ID` resolves to a non-null builder -- proving the
  `ZM_GetArchetypeBuilder` switch covers every `ZM_ARCHETYPE` and no species is
  left un-buildable (this would have failed on FLOATER_PLANTOID before SC5a).

- **Per-archetype structural tests**
  (`Tests/ZM_Tests_CreatureArchetype_<Name>.cpp`, one file per archetype, 3 units
  each = **24**): each runs the universal mesh contract + the `ZM_ValidateCreature`
  rollup + same-seed determinism over its own species set, plus an
  `ExpectedBoneSet` assert locking that archetype's bone topology:

  | Archetype | Bones | Locked structural shape (`ExpectedBoneSet`) |
  |---|---:|---|
  | QUADRUPED | 18 | single Spine root, >= 4 leg `...Up` roots, a Tail, a Head |
  | BIPED | 14 | Spine root chain -> Head, ArmL/R + LegL/R Up/Lo, dorsal Crest |
  | AVIAN | 13 | Spine root -> Head/Beak, WingL/R, LegL/R Up/Lo, Tail |
  | SERPENT | 12 | single Spine root + Spine/Tail chain + Head; ZERO limb `...Up` bones |
  | AQUATIC | 8 | Spine root -> Head, dorsal + 2 pectoral + caudal fin bones |
  | INSECTOID | 19 | single Spine root + Head + EXACTLY 6 leg `...Up` roots + antennae, <= 30 |
  | BLOB | 4 | single root, total bones in [2,4], zero limb bones |
  | FLOATER_PLANTOID | 10 | Spine root (floating) + Head + 6 radial Tendrils; mesh min-Y > 0 |

- **Golden-locks** (`Tests/ZM_Tests_CreatureGen.cpp`, pure, compiled in ALL
  configs): the size-class scale curve (TINY .45 / SMALL .70 / MEDIUM 1.00 /
  LARGE 1.50 / HUGE 2.20, strictly increasing), the asset-path ref scheme
  (`game:Creatures/<Name>/<Name><suffix>.<ext>` + too-small-cap -> false
  truncation), the `ZM_FormatBoneName` zero-padded 2-digit suffix, and the shiny
  hue band [80, 280) degrees. A change to any of these forces a
  `uZM_CREATUREGEN_VERSION` bump + a cold family re-bake.

- **Bake smoke** (`Tests/ZM_Tests_CreatureBake.cpp`, `ZENITH_TOOLS`-only, 1
  unit): `CreatureBake_BundleFilesLand` bakes FERNFAWN via `ZM_BakeCreature` and
  asserts all FIFTEEN bundle files land on disk non-empty -- the 9-file core
  (mesh, skeleton, albedo, shiny, icon, base + shiny `.zmtrl`, base + shiny
  `.zmodel`) plus the 6 `.zanim` clips, since the loop now covers all 15 asset
  kinds (`ZM_CREATURE_ASSET_KIND_COUNT`); `ZENITH_SKIP`s if
  the bake environment is unavailable. The full byte-identical re-bake invariant
  is deferred to the later `ZM_BakeManifest` box. In `_False`/Android configs the
  whole TU is empty (the header no-op returns false), so it does not register.

- **P1 species-gallery visual gate** (`Tests/ZM_AutoTests_Gallery.cpp`,
  `ZM_CreatureGallery_Test`, windowed): **the S4 visual-gate artifact.** It bakes
  a diverse sampled dozen (>= 1 per archetype; Zenithrax shown SHINY), places
  their `.zmodel` models in a 4x3 grid under a framed camera + key/fill lights,
  and dumps three swapchain TGAs to `Build/artifacts/zenithmon/s4/visual/` for the
  human sign-off. It is also a real regression test: it asserts all twelve
  `.zmodel` bundles loaded into renderable instances and that every capture file
  was written. `m_bRequiresGraphics = true` + asset-guarded (`RequestSkip` when
  the git-ignored `Assets/Creatures` tree is absent), so it skips the headless CI
  batch (C5/C6) and runs only on a windowed `_True` build. This registers as a new
  P1 automated test on top of the six S3 left.

#### ZM_Gen -- creature-animation generator (SHIPPED)

`ZM_CreatureAnimGen` (SC1..SC6) authors 6 rotation-only clips per archetype
(Idle / Walk / Attack / Special / Hit / Faint) against the frozen creature
skeletons -- 24 ticks/sec, pure `f(archetype, clip-id)`, so a clip is
byte-identical across every species of an archetype -- and (SC6) bakes them to
disk as `.zanim` inside each species' bundle, with each `.zmodel` self-listing
its 6 clips via `AddAnimationPath` in IDLE..FAINT order
([AssetManifest.md](AssetManifest.md) 1.2). It ships **19** `ZM_Gen` units
(boot gate 1847 -> 1866):

- **Generic all-config units** (`Tests/ZM_Tests_CreatureAnimGen.cpp`, 10 units,
  compiled in ALL configs): `ChannelsMatchSkeleton` (clip bone channels match the
  archetype skeleton), `ValidationPasses`, `ClipMetadataGolden` (clip
  count/order/tick-rate golden-pinned), `SameArchetypeByteIdentical` (every
  species of an archetype yields byte-identical clips), `SameInputsDeterminism`,
  `ClipsDistinct` (the 6 clips differ from one another), `LoopingClipsWrapCleanly`
  (Idle/Walk loop seamlessly), `FaintSettlesAndClamps`, `OneShotClipsEndNeutral`
  (Attack/Special/Hit end on the neutral pose), and `AllArchetypesHaveAnimBuilder`
  (every `ZM_ARCHETYPE` resolves to an anim builder).

- **Per-archetype structural units**
  (`Tests/ZM_Tests_CreatureAnimArchetype_<Quadruped|Biped|Avian|Serpent|Aquatic|Insectoid|Blob|FloaterPlantoid>.cpp`,
  one file per archetype, 1 unit each = **8**): each
  `<Archetype>Anim_ExpectedChannels` locks that archetype's animated bone-channel
  set.

- **Bake smoke** (`Tests/ZM_Tests_CreatureAnimBake.cpp`, `ZENITH_TOOLS`-only, 1
  unit): `CreatureAnimBake_ClipsLandAndModelReferences` bakes FERNFAWN and asserts
  the 6 `.zanim` land on disk non-empty and the baked `.zmodel` self-lists all 6
  refs in IDLE..FAINT order; `ZENITH_SKIP`s when the bake environment is
  unavailable. In `_False`/Android configs the whole TU is empty, so it does not
  register.

#### ZM_Gen -- human generator (SHIPPED)

`ZM_HumanGen` (SC1..SC5) turns each of the ~34 humanoid NPC roster rows into a
deterministic model, but INVERTS the creature layout: every human binds ONE
fixed shared 16-bone skeleton and reuses ONE shared 9-clip animation set
(Idle / Walk / Run / Talk / Wave / Point / Cheer / Hurt / Faint) -- per-model
variation is mesh-loft + texture ONLY, with NO per-model skeleton and NO
per-model clips (contrast creatures, which bake their own skeleton + own 6
clips EACH). Determinism is golden-pinned (`uZM_HUMANGEN_VERSION` = 1). It ships
**20** `ZM_Gen` units (boot gate 1866 -> 1886) -- 18 pure/all-config in
`Tests/ZM_Tests_HumanGen.cpp`, plus 2 `ZENITH_TOOLS`-only bake smokes in the new
`Tests/ZM_Tests_HumanBake.cpp`:

- **SC1 -- roster + shared 16-bone skeleton + frozen seam**
  (`Tests/ZM_Tests_HumanGen.cpp`, 6 units): `RosterTotality` (all 34 rows build +
  full `ZM_ValidateHuman`), `SharedSkeletonWellFormed` (16 bones, single root,
  parent < child, identity bind rotation), `RecipePurity` (pure f(id) + distinct
  per-human seeds), `AssetPathScheme` (golden shared + per-model refs;
  too-small-buffer -> false), `ClipMetadataGolden` (the frozen 9 clips'
  count/order/tick-rate -- extended in SC4 to all nine schedules), and
  `BuildDeterminism` (reflexive equality/hash + PlayerM-vs-Bram non-degeneracy).

- **SC2 -- per-model humanoid mesh loft** (`Tests/ZM_Tests_HumanGen.cpp`, 4
  units): `StructuralInvariants`, `PerModelBonesMatchShared`,
  `SameSeedDeterminism`, and `Sensitivity` (the MESH domain changes the mesh;
  every non-MESH domain does not; cross-id difference).

- **SC3 -- deterministic appearance + silhouettes**
  (`Tests/ZM_Tests_HumanGen.cpp`, 4 units): `AppearanceAlbedoStructural`,
  `AppearanceDomainIsolation`, `HairStyleSilhouettes`, and
  `AttachmentSilhouettes`.

- **SC4 -- shared 9-clip rotation-only curves** (`Tests/ZM_Tests_HumanGen.cpp`,
  4 units): `HumanGen_ClipChannelsMatchSharedSkeleton`,
  `HumanGen_ClipTimingAndPlaybackPolicy`,
  `HumanGen_ClipDeterminismAndSensitivity`, and
  `HumanGen_ClipSetSharedAcrossRoster`. Clips are rotation-only, pure f(clip),
  byte-identical across all 34 models; looping clips (Idle/Walk/Run/Talk) close
  their loop seams, one-shots (Wave/Point/Cheer/Hurt) return to identity, and
  Faint settles into and holds its final pose.

- **SC5 -- tools disk bake** (`Tests/ZM_Tests_HumanBake.cpp`,
  `ZENITH_TOOLS`-only, 2 units): `HumanBake_SharedAndModelFilesLand` (bakes the
  shared rig + 9 clips + PlayerM's per-model bundle and asserts all 10 shared
  files [1 `.zskel` + 9 `.zanim`] and all 4 per-model files
  [`.zmesh`/`_albedo.ztxtr`/`.zmtrl`/`.zmodel`] land non-empty) and
  `HumanBake_ModelBindsSharedRigAndClips` (hermetically loads the baked `.zmodel`
  via stream + `ParseStream`, asserts it binds the SHARED skeleton ref -- proving
  NO per-model rig -- and self-lists exactly the 9 SHARED clip refs in
  IDLE..FAINT order). Both `ZENITH_SKIP` when the bake environment is
  unavailable; in `_False`/Android configs the whole TU is empty, so they do not
  register.

#### ZM_Gen -- building generator (SHIPPED)

`ZM_BuildingGen` turns each of the 30 building roster rows into a deterministic
STATIC model -- a box composition lofted via `ZM_StaticMesh` with NO skeleton and
NO animation (contrast creatures/humans, which carry rigs + clips) -- and bakes a
4-file static bundle per building. Per-model variation is shell geometry + facade
texture only. Determinism is golden-pinned (`uZM_BUILDINGGEN_VERSION` = 1). It
ships **9** pure/all-config `ZM_Gen` units in `Tests/ZM_Tests_BuildingGen.cpp`
(SC1..SC3), plus 1 `ZENITH_TOOLS`-only bake smoke (SC5) in
`Tests/ZM_Tests_BuildingBake.cpp`:

- **SC1 -- roster + recipe purity + static mesh contract**
  (`Tests/ZM_Tests_BuildingGen.cpp`, 5 units): `BuildingGen_RosterTotality` (all
  30 rows self-index + build + `ZM_ValidateBuilding` pass + static contract +
  gym/non-gym theme-type contract), `BuildingGen_RecipePurity` (pure f(id) +
  pairwise-distinct synthetic seeds + MESH != ALBEDO seed),
  `BuildingGen_AssetPathScheme` (golden `game:Buildings/<Name>/...` refs +
  too-small-buffer -> false truncation), `BuildingGen_BuildDeterminism`
  (reflexive byte-identity/hash + distinct-ids-differ), and
  `BuildingGen_StaticMeshContract` (zero bones, empty skin buffers, outward
  winding, finite in-range UVs).

- **SC2 -- parametric shell** (`Tests/ZM_Tests_BuildingGen.cpp`, 2 units):
  `BuildingGen_ShellStructural` (exact per-roof-kind vert/tri counts -- GABLE
  38/18, HIP 36/16, FLAT 48/24 -- + static validity + grounded y=0 + roof apex
  above wall height) and `BuildingGen_MeshSensitivity` (the MESH seed perturbs
  the mesh; a non-MESH seed does not; distinct ids differ).

- **SC3 -- facade texture** (`Tests/ZM_Tests_BuildingGen.cpp`, 2 units):
  `BuildingGen_FacadeStructural` (facade non-empty for all 30 rows; window/door
  pixels land in the wall band not the roof band; >= 3 distinct colours;
  deterministic) and `BuildingGen_FacadeDomainIsolation` (the ALBEDO seed changes
  the facade; the MESH seed does not; distinct palette/theme ids differ;
  same-palette gyms diverge via theme tint).

- **Bake smoke** (`Tests/ZM_Tests_BuildingBake.cpp`, `ZENITH_TOOLS`-only, 1
  unit): `BuildingBake_StaticModelFilesLandAndNoRig` bakes CareCenter via
  `ZM_BakeBuilding` and asserts the 4 per-model files land on disk non-empty,
  then hermetically re-parses the baked `.zmodel` and asserts it is STATIC --
  `GetSkeletonPath().empty()` + `HasSkeleton()` == false +
  `GetNumAnimations()` == 0; `ZENITH_SKIP`s when the bake environment is
  unavailable. In `_False`/Android configs the whole TU is empty, so it does not
  register.

#### ZM_Gen -- prop generator (SHIPPED)

`ZM_PropGen` turns each of the 28 prop roster rows into a deterministic STATIC
model -- like buildings, a box composition via `ZM_StaticMesh` with NO skeleton
and NO animation, baked as a 4-file static bundle per prop. Determinism is
golden-pinned (`uZM_PROPGEN_VERSION` = 2). It ships **9** pure/all-config
`ZM_Gen` units in `Tests/ZM_Tests_PropGen.cpp` (SC4), plus 1 `ZENITH_TOOLS`-only
bake smoke (SC5) in `Tests/ZM_Tests_PropBake.cpp`:

- **Roster + purity + static contract** (`Tests/ZM_Tests_PropGen.cpp`, 5 units,
  mirroring BuildingGen's SC1): `PropGen_RosterTotality` (all 28 rows self-index +
  build + `ZM_ValidateProp` pass + static contract + biome contract: DRESSING
  rows carry a real biome, all others NONE), `PropGen_RecipePurity` (pure f(id) +
  pairwise-distinct synthetic seeds + MESH != ALBEDO seed),
  `PropGen_AssetPathScheme` (golden `game:Props/<Name>/...` refs +
  too-small-buffer -> false truncation), `PropGen_BuildDeterminism` (reflexive
  byte-identity/hash + distinct-ids-differ), and `PropGen_StaticMeshContract`
  (zero bones, empty skin buffers, outward winding, finite in-range UVs).

- **Biome coverage + domain isolation** (`Tests/ZM_Tests_PropGen.cpp`, 3 units):
  `PropGen_BiomeDressingCoverage` (every real battle-dome biome has >= 1 dressing
  prop), `PropGen_MeshSensitivity` (the MESH seed perturbs the mesh; a non-MESH
  seed does not; distinct ids differ), and `PropGen_TextureDomainIsolation` (the
  ALBEDO seed changes the texture; the MESH seed does not; distinct palette/biome
  ids differ).

- **Bake smoke** (`Tests/ZM_Tests_PropBake.cpp`, `ZENITH_TOOLS`-only, 1 unit):
  `PropBake_StaticModelFilesLandAndNoRig` bakes LampPost via `ZM_BakeProp` and
  asserts the 4 per-model files land, then re-parses the baked `.zmodel` and
  asserts it is STATIC -- no skeleton, 0 anims; `ZENITH_SKIP`s when the bake
  environment is unavailable. In `_False`/Android configs the whole TU is empty,
  so it does not register.

#### ZM_Gen -- bake manifest (SHIPPED)

`ZM_BakeManifest` (ZM-D-085) is the per-family bake guard: a 12-byte `ZMBM`
stamp (ASCII magic + u32-LE generator version + u32-LE expected-file count)
written atomically to `game:<Family>/.manifest` after a successful
`ZM_BakeAll*`, and read fail-open by `ZM_BakeManifestCheck` (a family is WARM
iff the stamp is current AND every enumerated file is present non-empty). It
mirrors the terrain `ZMTR` marker (section 5.3 / [AssetManifest.md](AssetManifest.md)
4.3). It ships **3** `ZM_Gen` units (boot gate 1905 -> 1908):
`EnumerationMatchesRoster` (all-config -- each family's enumerated file set
matches its roster) plus the `ZENITH_TOOLS`-only `RebakeByteIdentical` and
`GuardWarmStale`; the two tools-only cases register as empty TUs in
`_False`/Android configs.

#### S4 visual gate -- ZM_AssetGallery_Test (SIGNED OFF)

The S4 stage gate is the windowed `ZM_AssetGallery_Test`
(`Tests/ZM_AutoTests_AssetGallery.cpp`, `#ifdef ZENITH_INPUT_SIMULATOR`,
`m_bRequiresGraphics`, tools-gated `ZM_BakeAllAssets()`): it bakes all four
families then shows 26 representatives across them (8 creatures
one-per-archetype, 6 humans, 6 buildings, 6 props) on a reflective floor,
asserting all 26 `.zmodel` bundles loaded into renderable instances and that
three angle-TGA dumps landed on disk. It skips headless/CI (graphics-required
C5 + asset-guarded C6) and is **NOT** in the boot unit gate. It passed windowed
and the S4 visual gate was **SIGNED OFF 2026-07-16 (ZM-D-088)**; the first
capture was rejected for buildings intersecting (height-only scale overlap) and
fixed via a width-budget `AGFitScale` (ZM-D-087) before approval.

Boot unit-gate baseline after the full S4 creature + creature-animation + human +
building + prop + bake-manifest work: **1908** (was 1773 at the S3 gate, 1847
after the creature generator, 1866 after the creature-animation generator, 1886
after the human generator; the building generator adds **+10** [9 units + 1 bake
smoke] to reach 1896, the prop generator adds **+9** [8 units + 1 bake smoke] to
reach 1905, and `ZM_BakeManifest` adds the final **+3** [`EnumerationMatchesRoster`
all-config + `RebakeByteIdentical`/`GuardWarmStale` tools-only] to reach 1908).
S5 item 1 (`ZM_BattleArena`, ZM-D-089) then adds **+5** T0 units to reach **1913**,
and `.github/workflows/zm-tests.yml` was bumped to `-Baseline 1913` at that
commit (see section 5.5). **That is a 2026-07-16 value and this document does
not restate the current pin** -- per the standing rule in 5.3, the pins are
`.github/workflows/zm-tests.yml`'s `-Baseline` (Zenithmon, combined engine+ZM)
and `Tools/run_unit_gate.ps1`'s default (engine-only), with `Status.md`'s CURRENT
BASELINE block as the readable record. They are DIFFERENT numbers and the
engine-only default must never be made to track the Zenithmon one.

**All S4 generator families are now built** (creatures, creature animation,
humans, buildings, props), gated by the per-family `ZM_BakeManifest` marker
above; the shared invariant template (same-seed determinism, structural
validity, domain-seed isolation) governs each. **S4 is COMPLETE** -- every code
box shipped and the full-family `ZM_AssetGallery_Test` visual gate is signed off
(2026-07-16, ZM-D-088).

### 5.5 S5 -- battle integration slice

**#### ZM_BattleArena -- battle-arena component (item 1, SHIPPED ZM-D-089)**

`ZM_BattleArena` (serialization order 108) manages the always-visible battle
arena at world Y = -2000 -- a dome + 2 platforms + 6 per-biome dressing prop
sets, exactly one shown at a time. Determinism/placement are golden-pinned
(`uSERIALIZATION_VERSION` = 1, `fARENA_WORLD_Y` = -2000, `uBIOME_COUNT` = 6).

- **T0 `ZM_BattleArena` units (5 at ZM-D-089; the file now holds 6)** in
  `Tests/ZM_Tests_BattleArena.cpp` -- a later commit added
  `ChildCountMatchesArenaComposition`, which is not itemised below,
  pure/all-config (no disk, no GPU, no entity construction -- only the `static`
  helpers + the compiled `ZM_PropData`/`ZM_WorldSpec` tables): `BiomeEnumCoverage`
  (the 6-biome roster agrees across the battle enum, the component constant, the
  real-prop-biome count, and the DRESSING roster span), `DressingMappingContract`
  (`DressingPropForBiome` maps every biome to a distinct real DRESSING prop whose
  roster biome tag matches `MEADOW+e`; out-of-range -> `ZM_PROP_NONE`),
  `VisibilityExactlyOne` (`VisibilityMaskForBiome` is one-hot `1u<<e`, popcount 1;
  out-of-range -> 0 -- the "exactly one dressing shown" invariant), `ArenaConstants`
  (golden `fARENA_WORLD_Y` / `uSERIALIZATION_VERSION`), and `WorldSpecBattleRowContract`
  (Battle is build index 1, kind BATTLE, empty terrain set). These raise the boot
  unit baseline **1908 -> 1913** (bumped in `.github/workflows/zm-tests.yml`).
- **P1 `ZM_BattleArena_Test` (SHIPPED, windowed)** in `Tests/ZM_AutoTests_BattleArena.cpp`,
  `m_bRequiresGraphics = true`, max 240 frames. Warm-bake-guarded on the PROP family
  (`ZM_BakeAllAssets` in tools / `ZM_BakeManifestCheck(ZM_ASSET_FAMILY_PROPS, ...)`
  otherwise) and exists-guarded on `Battle.zscen`, both `RequestSkip` when absent
  (C5/C6). It registers build index 1, additively loads Battle over the running
  game, lets the arena's `OnStart` build, then drives `SetBiome(ZM_BATTLE_BIOME_VOLCANIC)`
  and verifies a unique arena, `IsBuilt()` + `IsFullyBuilt()` (all 9 child entities
  spawned), `SetBiome` true, `GetActiveBiome()` == VOLCANIC, and the arena-root entity
  at world Y within 0.5 of `fARENA_WORLD_Y`; cleanup unloads the additive scene and
  restores the prior active scene. It auto-skips the headless CI batch and runs at the
  local gate (end-to-end PASS with real warm assets, 1/0, 31 frames).

**#### ZM_Encounter -- wild-encounter selector (item 2 SC2, SHIPPED ZM-D-090)**

Pure/headless units for `ZM_EncounterZone` (rate gate -> weighted slot pick ->
inclusive level band, all from a caller-owned seeded `ZM_BattleRNG`; no
entity/scene/Flux state). In `Tests/ZM_Tests_Encounter.cpp` (category
`ZM_Encounter`, 10 units) + `Tests/ZM_Tests_WorldSpec.cpp` (+1 `ZM_Data`):
`SelectSlotIndex_WeightedDeterminism` / `_ProportionalHistogram` (1:3:6 over
10000 draws) / `_SingleSlot`; `RollStep_RateGateExtremes` (0 never / 256 always) /
`_EmptyTable` / `_LevelBandInclusive` (both endpoints reachable) / `_Determinism` /
`_InertAndMissDoNotPerturbRng` (raw `Next()` position: inert step draws 0, miss
draws exactly 1 -- the rig-stability lock); `RollStepForScene_NonRouteNoEncounter` /
`_RouteYieldsRosterSpecies` (Route1 hits validated against the LIVE slot table);
and `WorldSpec_EncounterRateColumn` (route-with-slots rate > 0, all else 0, <= 256).
Boot unit baseline **1913 -> 1924** (Zenithmon-only; engine default 1078 unchanged).
The `ZM_OnWildEncounter` event is defined but not yet dispatched (SC3 emits it).

**#### ZM_Grass -- tall-grass system static surface (item 2 SC3, SHIPPED ZM-D-091)**

Pure/headless T0 units for `ZM_TallGrassSystem`'s (order 109) three static helpers
(`Tests/ZM_Tests_TallGrass.cpp`, category `ZM_Grass`, 6 units): `QuantizeToTile_FloorSemantics`
(per-axis `std::floor`; negatives round toward -inf), `QuantizeToTile_AxesIndependent`,
`IsTileTransition_FirstTileNever` (no last tile -> never a transition), `_SameTile`,
`_ChangedTile` (x-only / z-only / both), `IsGrassDensity_Threshold` (inclusive >= 0.5
gate; `fGRASS_DENSITY_THRESHOLD == 0.5`). No component instance is constructed (the ctor
needs a `Zenith_Entity&` and OnAwake/OnUpdate touch scene/Flux state). Boot unit baseline
**1924 -> 1930** (Zenithmon-only; engine default 1078 unchanged). The runtime behaviour
(density load, tile-transition roll -> `ZM_OnWildEncounter` emission, clear-on-interior)
is the SC4 windowed test.

**#### Flux_Grass -- engine E5 grass-singleton reset (item 2 SC1, SHIPPED ZM-D-092)**

3 ENGINE `Flux_Grass` units (`Zenith/Flux/Vegetation/Flux_Grass.Tests.inl`, hosted in
the always-linked `Flux_Grass.cpp` under `ZENITH_TESTING`, headless CPU-only via
`g_xEngine.Grass()`): `Reset_ClearsAllSceneData` (Reset zeroes the CPU instance array +
generated/uploaded flags + density map + chunks + counters), `Reset_IsIdempotent`,
`Reset_NoAccumulationAcrossSetup` (a second scene's setup doesn't pile on the first --
the per-scene instance-count lock). Engine units run in EVERY game's boot, so these bump
BOTH the engine default `1078 -> 1081` (`Tools/run_unit_gate.ps1`; engine-gate + scaffold-smoke)
AND Zenithmon `1930 -> 1933` (`zm-tests.yml`). Cross-game regression all green (Combat 1081/0,
Zenithmon 1933/0 + windowed `ZM_GrassRegeneration_Test`, DP 158/0, CityBuilder 45/0);
RenderTest is pre-existingly red here (missing terrain -- Questions.md Q-2026-07-16-001) and
E5 was proven non-regressing against it by a stash-revert diagnostic.

**#### ZM_TallGrass windowed integration (item 2 SC4, SHIPPED ZM-D-093) -- S5 ITEM 2 COMPLETE**

2 windowed P1 tests (`Tests/ZM_AutoTests_TallGrass.cpp`, both `m_bRequiresGraphics`, skip
headless CI + asset-guarded -> 0 boot-unit-baseline change, stays 1933): `ZM_TallGrassEncounter_Test`
(loads Dawnmere, runtime-attaches `ZM_TallGrassSystem` to the terrain entity + manual `OnAwake()`,
subscribes to `ZM_OnWildEncounter` under `ScopedTestIsolation`, arms the explicit-species force
`ForceEncounterOnNextTransitionForTests(FERNFAWN, 5)`, data-drives the walk direction from the
density map, walks the player onto grass, and asserts `ZM_OnWildEncounter{FERNFAWN,5,DAWNMERE}`
fired) and `ZM_TallGrassInteriorClear_Test` (Dawnmere grass generates -> SINGLE-load PlayerHome
interior -> `Grass().GetGeneratedInstanceCount()==0`, the E5 reset with an INTERIOR target). Both
PASSED windowed (2/0; the encounter test 206 frames). **S5 item 2 is COMPLETE** (SC2 encounter roll +
SC3 tall-grass component + SC1 engine E5 + SC4 integration); the tall-grass -> encounter -> event
path is proven end-to-end (emit-only; item 3 wires the additive-battle subscriber).

**#### ZM_BattleTransition windowed round trip (item 3 SC5, SHIPPED ZM-D-099) -- S5 ITEM 3 COMPLETE**

2 windowed P1 tests in `Tests/ZM_AutoTests_BattleTransition.cpp` (both `m_bRequiresGraphics = true`,
skip headless CI + asset-guarded -> 0 boot-unit-baseline change, stays **1946**). `ZM_BattleEncounterLatch_Test`
(SC3b, 900-frame cap) drives the Dawnmere player onto a real grass tile and proves the persistent
`ZM_BattleTransition` singleton (order 110) OBSERVED the `ZM_OnWildEncounter` that `ZM_TallGrassSystem`
dispatched -- i.e. its `OnStart` subscription is bound to the LIVE dispatcher end to end -- then asserts
accepting the encounter hands the screen to the machine (`OwnsFade(state)`; PASS 207 frames).
`ZM_BattleRoundTrip_Test` (SC5, 2200-frame cap, fixed dt 1/30) is the item-3 END-TO-END GATE: walk grass ->
forced `FERNFAWN` L5 encounter -> assert the additive load only issues behind an OPAQUE fade -> in battle
assert active build index == 1, the battle camera is live (`FindMainCameraEntityAcrossScenes` == the Battle
scene's own main camera), the overworld is PAUSED (and `IsOverworldPausedInState(IN_BATTLE)` agrees), grass == 0,
the arena is `IsFullyBuilt()` with all `uCHILD_COUNT`(9) children owned by the BATTLE scene, biome == MEADOW,
issued-load count == 1 -> call `RequestBattleEnd()` (the SOLE exit) -> assert EXACT resume: build index 2,
unpaused, Battle unloaded, zero arena instances, movement re-enabled, zero aborts, grass restored to the entry
blade count, and drift from `GetParkedPlayerPosition()` < 0.05 m. Constructs NO `ScopedTestIsolation` (it needs
the game's own live subscriber -- the subject under test). PASS windowed (146 frames, `skipped:false`, `failures:[]`).
**S5 item 3 is COMPLETE** (SC1 own-scene arena + SC2 grass-restore seam + SC3a/SC3b component + SC4 state machine +
SC5 gate); the overworld<->battle round trip is proven end-to-end.

**#### UIText -- engine E3 typewriter reveal (item 4 SC1, SHIPPED ZM-D-100)**

7 ENGINE `UIText` units (`Zenith/UI/Zenith_UIText.Tests.inl`, hosted unconditionally at the `.cpp`
tail; pure/headless -- never call `Render()`): `ClipToVisibleGlyphs_Boundaries` (0 / mid / exact /
over-length / negative), `ClipToVisibleGlyphs_CountsSpacesAndNewlines` (spaces + `\n` each count;
empty stays empty), `DefaultVisibleGlyphCount_IsFullyRevealed` (-1 default, whole string), `SetGetVisibleGlyphCount_RoundTripsRawValue`
(no clamp: 3 / -1 / 1000 verbatim), `GetTotalGlyphCount_MatchesTextLengthNoWrap` (== display length;
0 for empty), `Serialization_RoundTripsVisibleGlyphCount` (v3 preserves 4 and the -1 sentinel), and
`Serialization_PreV3BlobDefaultsToRevealed` (hand-built v2 blob -> glyph count defaults -1 AND text reads
back intact -- the additive back-compat proof). Engine units run in every game's boot, so BOTH baselines
bumped +7: engine default `1081 -> 1088` (`Tools/run_unit_gate.ps1`) AND Zenithmon `1946 -> 1953`
(`zm-tests.yml`). Cross-game regression all green (Combat 1088/0, DP 1089/0, CityBuilder 1089/0,
RenderTest 1179 units/0 + builds clean; RenderTest's pre-existing red is a separate windowed terrain
test, Q-2026-07-16-001). Builds: Vulkan Debug True + Debug False + D3D12 Debug False. The runtime
typewriter drive (HP text log revealing glyph-by-glyph, `zm_instant_battles` collapsing it) is exercised
by the item-4 `ZM_UI_BattleHUD` windowed tests (SC4+).

**#### ZM_BattleDirector -- battle-presenter driver (item 4 SC2, SHIPPED ZM-D-101)**

9 pure/headless `ZM_BattleDirector` units in `Tests/ZM_Tests_BattleDirector.cpp` (no ECS/scene/graphics,
no baked assets -> no RequestSkip) for `ZM_BattleDirectorCore` (`Source/Battle/ZM_BattleDirectorCore.{h,cpp}`):
`MapEventToOp_TotalOverEveryKind` (every one of the 39 `ZM_BATTLE_EVENT` kinds maps to a defined op;
framing kinds -> `ZM_POP_NONE`, all others non-NONE -- the no-kind-dropped lock), `MapEventToOp_TextKindsCarryALine`,
`MapEventToOp_HpKindsAreTweens`, `InstantBattles_DrainsWholeTurnInOneTick` (with `zm_instant_battles` on, one
`Tick(0)` drains the intro then a full turn -- never mid-turn), `TimedBattles_AdvancesGraduallyNotInstant`
(off: a 0.01 s tick leaves the intro mid-range; a large tick drains it), `SubmitPlayerAction_RejectedOutsideAwaitInput`
(the AWAIT_INPUT precondition, checked non-fatally), `Resolution_SignalsRequestEndExactlyOnce` (a full instant
battle latches `ShouldRequestEnd()` at OVER), `BuildWildEnemySpec_DerivesLearnsetMoves`, and
`AiRngUnperturbing_DirectorDriveMatchesManualDrive` (a director drive is byte-identical to a raw-engine hand
drive with the same seed / player picks / identically-seeded AI rng -- the non-perturbation proof). Game-only,
so `zm-tests.yml` bumped **1953 -> 1962** (engine default 1088 unchanged). The runtime drive of a REAL battle
in the Battle scene (model placement + `RequestBattleEnd`) is the SC3 windowed `ZM_BattleDirectorRoundTrip_Test`.

**Historical S5 item 4/item 5 planning snapshot (now resolved):** at this point,
the remaining work was the `ZM_BattleDirector` component (SC3, order 111, driving
the core in the Battle scene + placing creature models + calling
`RequestBattleEnd()`), `ZM_UI_BattleHUD` (SC4-SC5), the windowed win gate (SC6),
and catch/exp/faint/whiteout applied to GameState (item 5), followed by the S5
visual gate. All of that work subsequently shipped and the S5 visual gate was
user-approved; this paragraph preserves the earlier planning boundary only.
- **P1 encounter round trip (windowed) -- SHIPPED `ZM_BattleRoundTrip_Test` (item 3, see above):**
  walk grass until a rigged encounter fires -> additive battle scene loads at the
  -2000 m offset -> assert opaque-fade-gated load, in-battle invariants, and
  EXACT overworld resume (scene, pause state, parked-body drift < 0.05 m).
  At this item-3 snapshot, applying exp on a real win + party mutation was item
  5's scope (that gate ended the battle via the item-4 `RequestBattleEnd()` seam,
  with no resolution). Item 5 subsequently shipped.
- **P1 catch test -- SHIPPED as `ZM_BattleMenuCatch_Test`** (item 5 SC4,
  `Tests/ZM_AutoTests_BattleMenu.cpp`, `m_bRequiresGraphics`): forces a DISTINCT
  wild KINDLET, installs a guaranteed-catch `ZM_ITEM_PRIMEORB`, drives the menu to
  Catch, and asserts the core ends with the PLAYER as winner while the persistent
  GameState gains a party member AND a marked caught-set entry. Its three
  siblings in the same file -- `ZM_BattleMenuWin_Test`, `ZM_BattleMenuRun_Test`,
  `ZM_BattleMenuWhiteout_Test` -- are equally shipped and equally undocumented
  here; read the file's header comment for their contracts.
- **P1 bleed-through screenshot check -- STILL NOT WRITTEN.** Scripted capture
  during battle asserting the overworld does not render into the battle view
  (`m_bRequiresGraphics`, stage-gate only). Note the framing "the one
  pixel-adjacent test" is itself superseded: the suite now has four pixel-reading
  probes (5.7's `ZM_NpcRenderedPalette_Test`, the `ZM_RivalVesperAuthored_Test`
  marker clause and the `ZM_BattleMenuRun_Test` arms, plus 5.8's
  `ZM_InteriorTintPixels_Test`) and their conventions apply to this one when it
  is written.

### 5.6 S6 -- UI flows

- **Shipped surface AT THE S6 CLOSE.** The overworld root menu has exactly
  **Party / Bag / Dex / Exit**. Dialogue, the buy-only **Trade Post** screen,
  and the Care Center yes/no heal prompt are reached through the same menu
  stack. Box is deferred to S7. `ZM_ShopLogic` selling exists and is
  pure-unit-covered, but S6 does not claim a sell UI flow.
  **SUPERSEDED on both counts.** (1) The root menu is now **SIX** entries, not
  four: S7 item 2 SC4 (ZM-D-140) INSERTED `SAVE` and `QUIT` *before* `EXIT`
  (which moved 3 -> 5), so `Components/ZM_UI_MenuStack.h` declares
  `ZM_MENU_ROOT_PARTY/BAG/DEX/SAVE/QUIT/EXIT` with
  `ZM_MENU_ROOT_ITEM_COUNT = 6u`. The insertion point is load-bearing, not
  cosmetic: `ZM_AutoTests_UI`'s focus walk drives DOWN/UP off the ENUM ordinal,
  so an entry placed visually above EXIT but numbered after it would oscillate
  to its deadline. (2) The Box screen was NOT delivered in S7 -- ZM-D-165 /
  Q-2026-07-29-001 **re-deferred it to S9** (`Roadmap.md:223`, the `ZM_UI_Box`
  line). The original S6 deferral at `Roadmap.md:98` already carries that
  annotation; this copy did not until now. The storage MODEL is not
  outstanding -- `ZM_SaveSchema` has persisted 16x30 boxes since ZM-D-136 --
  only the presenter is.
- **T0 units.** Headless-safe `ZM_UI` units cover stack push/pop, dialogue
  paging/choice latches, focus order, party/bag/dex presenters, atomic shop
  buy/sell logic, and interaction dispatch. `ZM_NpcWalkerLogic` units cover
  deterministic XZ steering, arrival/dwell, halt, invalid inputs, and
  preservation of the body's vertical velocity. The walker has no RNG,
  navmesh, scene, UI, or ECS dependency.
- **Authored NPC surface AT THE S6 CLOSE -- four NPCs:**
  `Npc_Villager`, `Npc_TradePostClerk`, `Npc_Caretaker`, and
  `Npc_Wanderer`. The Wanderer follows an authored, deterministic two-point
  patrol; opening its own dialogue halts it and closing the dialogue resumes
  it. NPC roles dispatch in C++ through `ZM_Interactable` for S6.
  `ZM_GraphAuthoring` and terrain-backed navmesh integration are deferred to
  S7.
  **SUPERSEDED -- the roster is SIX rows, not four.** S7 appended
  `ZM_NPC_ROUTE_WARDEN` (item 2 SC1, the first story-gated dialogue) and
  `ZM_NPC_RIVAL_VESPER` (item 3 SC8), so `ZM_NPC_COUNT == 6` in
  `Source/Data/ZM_NpcData.h`. Section 5.7's W4 box depends on that six: it pins
  that the six rows wear FIVE appearances, `Npc_Wanderer` and `Npc_Warden`
  sharing `ZM_HUMAN_TOWN_ELDER`. Read the enum, not this paragraph.
- **P1 walk-up proofs** (`Tests/ZM_AutoTests_NpcTalk.cpp` and
  `Tests/ZM_AutoTests_NpcServices.cpp`) reach the real authored entities using
  physics movement and `ZENITH_KEY_E`:
  - `ZM_NpcTalk_Test` walks to `Npc_Villager` and proves that row's own
    dialogue.
  - `ZM_NpcShop_Test` walks to `Npc_TradePostClerk`, proves the screen carries
    that row's runtime-read stock, buys the selected entry through focus input,
    and checks exact live money/bag deltas.
  - `ZM_NpcHeal_Test` walks to `Npc_Caretaker` and proves both YES
    (party restored) and NO (damage retained), including the completed-choice
    latch guard.
  - `ZM_S6InteractGate_Test` performs talk + buy + heal + every actual root
    menu entry in one uninterrupted session, with no static raise seam.
  - `ZM_NpcWander_Test` proves body-driven patrol motion, waypoint advance,
    moving-target interaction, 30 consecutive dialogue-halt samples, explicit
    close input, and resumed motion.
  Shared walk machinery uses a two-leg camera-basis probe, an out-of-range
  negative, target-entity identity, a bounded progress watchdog, and live
  camera-relative steering. It never calls `SetPosition` or
  `SetFocusedElement`.
- **S6 closure evidence (2026-07-21).** All five configurations built green,
  serially: Vulkan Debug/Release x Tools True/False and D3D12 Debug
  Tools=False. The unit gate was **2343 ran / 2342 passed / 0 failed /
  1 skipped**. The headless automated registry was **36 passed / 0 failed**:
  3 semantic tests executed and 33 graphics-required tests skipped as
  expected. The six exact-name windowed filters all ran non-skipped and
  passed: `ZM_S6UIGate_Test` **158 frames**, `ZM_NpcTalk_Test` **85**,
  `ZM_NpcShop_Test` **286**, `ZM_NpcHeal_Test` **315**,
  `ZM_S6InteractGate_Test` **749**, and `ZM_NpcWander_Test` **830**.
  The full windowed registry was **36 passed / 0 failed / 0 skipped**, and no
  test reported zero frames.
- **Authority and next gate.** Under the direct-master policy, this full local
  gate is the pre-commit/pre-push authority; headless `zm-tests` is the
  post-push backstop, not a reason to hold a locally proven Roadmap tick.
  Graphics-required flows remain intentionally invisible to headless CI, so
  the non-skipped local filters are their evidence. `ZM_NpcDispatch_Test` is
  the headless semantic dispatch proof and now asserts the role-specific
  screen/action rather than merely any raised screen. S6 has no visual or
  human gate. **("S7 is next" was true when written on 2026-07-21 and is not
  now: S7 COMPLETE 2026-07-29, and S8 item 1 is in progress -- see 5.8.
  `Roadmap.md` is the authority on stage state; this document is not.)**

### 5.7 S7 -- save/load, story flags, trainers

- **SC1 durable-model freeze (COMPLETE 2026-07-21, ZM-D-135):** 18 new pure
  `ZM_Save` units pin the complete in-memory inventory before bytes are written:
  monster friendship/nickname and caught-ability normalization; deterministic
  transactional 16x30 boxes with party-first catch overflow; seen/caught dex;
  4096 story bits; 8 badges; full-width money behavior; daycare-owned egg
  progress; tower current/best/seed; unset world position; NORMAL options; and
  starter defaults. `SaveFormat.md` is reconciled to that inventory, but SC1
  deliberately implements no codec, schema/module version, golden, migration,
  or save-slot I/O.
- **SC2 schema-v1 codec (COMPLETE 2026-07-21, ZM-D-136):** **29** pure
  `ZM_Save` cases in `ZM_Tests_SaveSchema.cpp` lock the complete explicit-LE
  11-module format, 61-byte monster record, maximal/empty/egg-only round trips,
  append-write and exact-length read transactionality, status mapping, every
  truncation boundary, exact framing, all field domains/sanity caps, raw move
  and IEEE-float bytes, StoryFlags high-water encoding, older/current/newer Dex
  roster-count policy, and counted Options TLVs including bounded unknown-field
  skipping and unknown-only rejection. **2** more cases in
  `ZM_Tests_SaveMigration.cpp` compare the canonical writer with an independent
  literal **824-byte v1 golden**, then decode/re-encode that literal and assert
  every represented field. They are initial v1 compatibility tests, not a fake
  v0 migration; future format changes owe a real version bump + historical
  literal migration in the same commit.
- **SC2 observed Zenithmon gate:** regen green; Vulkan Debug/Release x Tools
  True/False plus D3D12 Debug Tools=False all green; units **2392 ran / 2391
  passed / 0 failed / 1 skipped**; engine-only reference **1103**; headless
  registry **36/0**; full windowed registry **36/0/0**, with 36 JSON results,
  no skips and no zero-frame tests. Registry count remains 36 and SC2 has no
  visual/human gate.
- **SC2 engine-seam regression evidence (complete):** regenerate passed;
  SentinelECS, SentinelPhysics and SentinelAI built and ran green; Combat's
  Vulkan build, **1103 / 1102 / 0 / 1** boot gate and headless **14/0** suite
  passed; DevilsPlayground Vulkan/D3D12 builds, **1104 / 1103 / 0 / 1** boot
  gate and **158/0** suite (29 expected skips) passed; CityBuilder
  Vulkan/D3D12 builds, **1104 / 1103 / 0 / 1** boot gate and **45/0** suite (6
  expected skips) passed. Focused windowed RenderTest canaries each emitted
  exactly one unskipped passing JSON: `EngineBootShutdownSmoke` **1 frame** and
  `TerrainEditorSmoke` **151 frames**. Scaffold smoke finished **11 passed / 0
  failed** with its embedded **1103** unit baseline met; teardown regeneration
  was green and left git status unchanged.
- **Item 2 SC1 story-flag identity/gating (COMPLETE 2026-07-21, ZM-D-137):**
  **33** `ZM_Save` units pin the append-only dense-from-zero flag registry,
  name/identity/bit access and total fail-closed story gates. The first runtime
  consumer is gated `Npc_Warden` dialogue, with both branches mutation-pinned on
  the CI-visible `ZM_NpcDispatch_Test`. Observed boot **2425 / 2424 / 0 / 1**;
  headless/windowed registry remained 36.
- **Item 2 SC2 slot/disk layer (COMPLETE 2026-07-21, ZM-D-138):** **33**
  `ZM_Save` units pin Save0-2 + Auto identity, the `_Test` interlock, uncached
  `EMPTY / READY / DAMAGED` probing, transactional reads/writes, the explicit
  little-endian ZMSV-length prefix, verify re-probe, damaged-slot preservation,
  occupancy/readiness and all sixteen save-blocker combinations. Observed boot
  **2458 / 2457 / 0 / 1**; headless/windowed registry remained 36; save directory
  empty.
- **Item 2 SC3 capture/resume/quit/autosave foundation (COMPLETE 2026-07-21,
  ZM-D-139):** **27** pure `ZM_Save` units pin resume validation in scene -> tag
  -> transform order, capsule-centre world-position construction, yaw round trips,
  transform-first/spawn-tag-fallback placement and the autosave predicate.
  `ZM_ResumePlacement_Test` (**236 frames**) proves a captured pose 10.477 m from
  the marker survives a real scene reload and lands at planar/vertical/yaw error
  0.0000; `ZM_QuitToFrontEnd_Test` (**38 frames**) proves the two-barrier playerless
  transition. Observed boot **2485 / 2484 / 0 / 1**, headless **38/0**, full
  windowed **38/0/0**, save directory empty.
- **Item 2 SC4 save/manual/root flow (COMPLETE 2026-07-22, ZM-D-140):** **23**
  `ZM_Save` units in `ZM_Tests_SaveSlotScreen.cpp` pin the complete SAVE/LOAD x
  slot x `EMPTY / READY / DAMAGED` action matrix, row/label/name totality,
  uncached reprobes, Auto as manual-read-only but LOAD-visible, and damaged-row
  non-mutation. **5** `ZM_MenuStack` units pin the singleton refusal seam plus the
  six-item root resolver/screen/enum-order contract. `ZM_SaveMenuFlow_Test` (**98
  frames**) drives real input through an EMPTY write and confirmed READY overwrite,
  rejects invalid targets, and proves the second canonical blocker check wins at
  the irreversible boundary before `CaptureWorldPosition -> WriteState`.
  `ZM_RootQuitAndBlockedSave_Test` (**146 frames**) drives Quit No then Yes,
  focuses Save before a live WARP blocker, proves immediate focus rehome to Quit
  plus live Up/Down/Accept traversal, and reaches a READY Auto row in FrontEnd
  LOAD without writing it.
- **SC4 observed gate:** regen green; Vulkan tools-debug build green; boot **2513
  ran / 2512 passed / 0 failed / 1 documented skip**, +28 from clean SC3 (**23 +
  5**); automated registrations **38 -> 40**; headless discovery/gate **40/40**;
  both focused tests green at the frame counts above; full windowed **40/40 passed,
  0 failed, 0 skipped, 0 zero-frame**; save directory empty; final exact-diff
  check green. No commit, push or CI result is claimed yet.
- **Item 2 SC5 title menu + Continue (COMPLETE 2026-07-24, ZM-D-141):** **6**
  `ZM_Title` units pin the title name/action totality, Continue's
  occupied-visibility matrix (DAMAGED counts, EMPTY hides), reopen refresh and
  malformed-snapshot fail-closed; **2** `ZM_MenuStack` units pin the title
  routing and the `ZM_LoadConfirmState` arm/resolve/reset matrix.
  `ZM_SaveContinue_Test` (**247 frames**, graphics-required) is the
  disk-authentic gate: real-input New Game publishes a fresh starter over an
  installed canary; a busy queue refuses `RequestContinue` with exactly one
  READ + `QUEUE_FULL`; Continue stays visible with ONLY a DAMAGED slot on disk;
  the Auto fixture is restored from pre-deletion bytes; DAMAGED/EMPTY rows
  refuse with a plain line, never an armed choice; the pre-Yes live state is
  still the scramble; the Yes window performs exactly ONE `READ_STATE` on AUTO
  and ZERO writes (via the new `ZM_SaveSlots` operation observer); the
  published state equals the saved fixture and NOT the scramble; the restored
  pose lands within 0.05 planar / 0.10 vertical / 0.05 yaw of saved, >= 2 m
  from both TownCenter and the scramble pose. The extended
  `ZM_RootQuitAndBlockedSave_Test` (**158 frames**) pins the Auto-only FrontEnd
  TITLE contract and the armed-then-ESCAPEd load prompt. **Stack-frame rule:**
  the test's 29 phases were split into per-phase driver functions after a
  measured 1.3 MB `Step` frame overflowed the exe's 1 MB stack reserve
  (`ZM_GameState` locals are ~150-200 KB each) -- any multi-phase automated
  test touching `ZM_GameState` must use the per-phase-function shape.
- **SC5 observed gate:** regen green; Vulkan tools-debug build green; boot
  **2521 ran / 2520 passed / 0 failed / 1 documented skip** (+8 units);
  automated registry **40 -> 41**; headless **41/41**; focused
  `ZM_SaveContinue_Test` **247** / `ZM_RootQuitAndBlockedSave_Test` **158** /
  `ZM_SaveMenuFlow_Test` **98** frames; full windowed **41/41 passed, 0 failed,
  0 skipped, 0 zero-frame**; save directory empty; adversarial review CLEAN.
- **Item 2 SC6 milestone-autosave test closure (COMPLETE 2026-07-24, ZM-D-142):**
  test-only -- no production code shipped. The new windowed
  `ZM_MilestoneAutosave_Test` (**134 frames**, graphics-required, wrapped in
  `#ifdef ZENITH_INPUT_SIMULATOR`) is the disk-authentic milestone-autosave gate
  and extends the SC3 `ZM_ResumePlacement_Test` / `ZM_QuitToFrontEnd_Test`
  coverage (which sampled only the autosave counter and the Auto slot STATUS
  enum) with five new proofs. After a real `SCENE_ENTERED` arrival it (1) reads
  the AUTO slot back FROM DISK and field-compares the decoded `ZM_GameState`
  against a scrambled live state (party/boxes/dex/story bits/badges/money/world
  position+yaw), not merely `ProbeSlot == READY`; (2) proves EXACTLY ONE
  `ZM_SAVE_SLOT_OPERATION` `WRITE_STATE` on AUTO (`== 1`, not `>= 1`), twinned
  with the +1 counter delta via the SC5 slot-operation observer; (3) attributes
  the write to the OnUpdate IDLE drain, not the fade-in tail, with menu-term
  isolation (`blocker == NONE` under the open ROOT pause menu); (4) proves an
  attributable BLOCKED real arrival -- a genuine arrival completing under a live
  non-transition blocker (ROOT menu held open) with a proven-capturable player
  writes NOTHING: byte-identical Auto file (direct `Zenith_FileAccess` read), zero
  `WRITE_STATE`, counter unmoved, `ProbeSlot(AUTO) == EMPTY`; (5) proves LATCH
  RE-ARM + consume-before-attempt -- a second arrival autosaves again (+1) while
  `NoRetryWatch` proves a refused attempt does not hammer disk on later IDLE
  frames. It is disk-authentic (`ProbeSlot`/`ReadState` fall through to the real
  file because the RAM readback stash is never staged) and uses 12 per-phase
  driver functions with file-scope-global `ZM_GameState` instances, avoiding the
  SC5 monolithic-`Step` stack overflow. A five-mutation battery (each rebuilt in
  isolation, all confirmed RED then restored GREEN) proves the asserts bite:
  DROP-CAPTURE reds the positive disk-content/scene-tag/resume-valid asserts (the
  SC3 status-only green hole); DROP MENU CONSULT reds `MenuOpenProbe` + the
  blocked-arrival byte/counter/trace asserts; CONSUME-AFTER-SUCCESS reds
  `NoRetryWatch`; DELETE THE DRAIN reds the `SamplePositive` poll deadline (45
  frames); DOUBLE-WRITE reds the observer exactly-once (`traceExact == 1`) assert.
- **SC6 observed gate:** the file adds only a `ZENITH_AUTOMATED_TEST_REGISTER`
  and zero `ZENITH_TEST` boot units, so the boot baseline is UNCHANGED at **2521
  ran / 2520 passed / 0 failed / 1 documented skip** and `zm-tests.yml` is NOT
  bumped. Automated registry **41 -> 42**; `ZM_MilestoneAutosave_Test` is
  graphics-required and SKIPS headless (skip == pass), so it is CI-invisible and
  carried by the local windowed gate only. Headless **42 passed, 0 failed**;
  focused windowed `ZM_MilestoneAutosave_Test` **134 frames, PASS**; full windowed
  **42/42 passed, 0 failed, 0 skipped, 0 zero-frame**; save directory
  `%APPDATA%/Zenith/Zenithmon` verified EMPTY afterward.
- **Item 2 state:** SC1-SC6 of six are complete -- **S7 item 2 is DONE.** The
  milestone-autosave test obligation is closed by `ZM_MilestoneAutosave_Test`
  (SC6, disk-authentic); the save -> quit -> scramble -> Continue
  exact-restoration gate shipped with SC5 as `ZM_SaveContinue_Test` (the
  `DontDestroyOnLoad` RAM-survival inoculation pattern: scramble, prove the
  scramble took, assert published == saved != scramble).
- **Item 3 SC1 navmesh terrain-source evaluation (COMPLETE 2026-07-24,
  ZM-D-144):** a pure headless spike evaluating a coverage-grid terrain source
  for `Zenith_NavMeshGenerator`. **4** `ZM_Nav` units in
  `Tests/ZM_Tests_NavEval.cpp` (backed by the new `Source/Nav/ZM_NavEval.{h,cpp}`)
  pin it: `DawnmereFlatGridYieldsWalkableNavmeshInBand` -- a flat coverage grid
  over Dawnmere's 1024 m export sub-rect at a 16 m cell feeds
  `Zenith_NavMeshGenerator::GenerateFromGeometry` and yields a walkable navmesh
  whose polygon count lands in the hand-bracketed band **3969..4489**
  (~4225 = a 65x65 voxel grid), every polygon upward-facing;
  `TooFineCellSizeIsRejectedFailClosed` -- a 0.3 m cell over the 1024 m domain is
  rejected fail-closed by the harvester (`m_bAttempted` false; generator grid dim
  in [3000,3500]; min-safe cell in [0.99,1.5]), with an 8 m CONTROL that IS
  attempted + walkable (defeats "rejects everything");
  `AllVerticalGridHasZeroWalkablePolygons` -- an all-vertical grid yields zero
  walkable polygons (generator returns null), with an upward CONTROL that IS
  walkable (proves verticality specifically strips walkability); and
  `DawnmereRectIsThe1024ExportSubRect` -- the harvested rect is Dawnmere's 1024 m
  sub-rect (domain in [1000,1100], < 2048, TownCenter (512,480) inside, ground in
  [20,30]), NOT the engine's 4096 m grid. This SC touches NO engine file, is
  disk-free, and never constructs a live `Zenith_TerrainComponent` (sidesteps
  Q-2026-07-21-001); the `.znavmesh` persistence + runtime routing are DEFERRED
  (Q-2026-07-24-002 Q-A) -- this is the evaluation only. Teeth mutation-proven: a
  winding flip reds units 1, 2-control and 3-control while unit 4 stays green.
- **SC1 observed gate:** the four `ZM_Nav` cases are all headless/pure
  `ZENITH_TEST` boot units (NOT windowed and NOT automated), so the boot baseline
  moves **2521 -> 2525** (+4 `ZM_Nav`) and `zm-tests.yml` is bumped to **2525**;
  the automated registry stays **42** (no windowed/automated test added) and the
  engine-only reference is unchanged at **1103**.
- **SC1b -- baked navmesh persistence (ZM-D-147).** The engine gained the
  reusable bake/load feature and Zenithmon became its first consumer, so this SC
  DOES touch engine files and owed the full engine gate. Two new automated tests
  take the registry **42 -> 44**, and **both RUN headless** (neither is
  `m_bRequiresGraphics`, neither `RequestSkip`s -- see the C6 deviation above):
  - **`ZM_NavmeshAsset_Test`** -- four per-phase drivers over the COMMITTED
    `Dawnmere.znavmesh`: (A) runtime adoption (`AddComponent` + `SetAssetRef`,
    then a settle frame, because `OnStart` is deferred to the first Update);
    (B) CONTENTS, not just non-null -- polygon count in **[3969, 4489]**, every
    normal facing up, bounds covering the TownCenter spawn in XZ, zero vertical
    extent (a FLAT coverage bake, so asserting 3D containment of y~26 would be
    asserting the wrong thing), and `FindNearestPolygon` succeeding at the spawn;
    (C) structural agreement against a FRESH in-memory bake of the same pure
    inputs -- the drift detector that reds if the committed bytes stop matching
    the source recipe; (D) fixture-scene unload with a component-count check.
  - **`ZM_DawnmereHeadless_Test`** -- direct-loads the COMMITTED
    `Dawnmere.zscen` (build index 2; a direct load records no warp arrival, so
    no autosave side effects) and proves the AUTHORED component came back with a
    live in-band mesh. This is the gate that catches the editor-registry mirror
    being missed, which would silently save a scene with no component at all.
    It makes no gameplay, height or terrain claims -- on CI the terrain chunks
    are absent by design.
  - **SC1b observed gate:** boot units **2515 -> 2546**, engine-only reference
    **1093 -> 1121**, automated registry **42 -> 44**, all six mutations proved
    teeth (see ZM-D-147).
- **Known-limit W1 -- forced replacement (COMPLETE 2026-07-28, ZM-D-157):** five
  new boot units pin both-side replacement, `TURN_END -> SWITCH_IN` ordering,
  invalid-policy lowest-live fallback, no extra normal turn, battle-RNG identity,
  RANDOM live-reserve-only AI with private-RNG use, tactical best-match selection,
  and the director-to-AI policy seam. Existing forced-switch, sand-chip, EXP and
  trainer-builder cases were updated to the new invariant. The existing
  `ZM_TrainerBattle_Test` now fields Rambler Perrin's complete authored two-member
  row and observes lead `FAINT`, exactly one reserve `SWITCH_IN`, eventual player
  win, flagless payout and one clean round trip. Observed gate: boot **2703 ->
  2708** (`2708 / 2707 / 0 / 1`), registry unchanged **47/47**, Null headless and
  full Vulkan windowed **47/47**, Vulkan and Null builds green. Required W1
  compatibility gate: engine boot **1164 / 1163 / 0 / 1**; Combat **14/14** plus
  its boot gate, CityBuilder **45/45**, DevilsPlayground **158/158**. Four
  exact-one-anchor mutations compiled fresh and redded the observed W1 suite:
  discard the policy's selected slot, reverse tactical AI scoring, restore the
  trainer cap to 1, and consume presented event indices without synchronizing the
  arena model (focused trainer gate **0/1**, restored **1/1**). A fifth candidate
  that merely restored the historical hard process break was rejected as an invalid
  mutation because it killed the process instead of producing a parseable unit result.
- **Known-limit W2 -- honest rival loss/whiteout (COMPLETE 2026-07-28,
  ZM-D-158):** new independent `ZM_RivalVesperWhiteout_Test` registration, runnable on
  Null, starts with the exact level-5 Fernfawn and physically
  walks to the committed authored Vesper. **(SYMBOL RENAMED, ZM-D-175: the seed was
  `ZM_MakeStarterGameState()` when this was written; that function is DELETED. Today
  the same composition is `ZM_MakeNewGameState()` followed by
  `ZM_ApplyStarterChoice(state, ZM_STARTER_CHOICE_FERNFAWN)` -- see 5.8. Any doc,
  comment or test still naming `ZM_MakeStarterGameState` is stale.)** Before input it captures the live director
  core and pins exact parties/vitals/PP/status plus Catch/Run refusal. It navigates the
  real HUD to a legitimate second learned move and requires the matching PLAYER
  `MOVE_USED` event, then a natural ENEMY winner; move slot 0 was empirically rejected
  because it produced a PLAYER win with 13 HP. The gate proves no money/flag/EXP,
  observes the pending-whiteout order boundary, dirties durable HP/PP/status, requires
  full healing plus exactly one manager/transition load, computes TownCenter placement
  independently from marker feet + capsule half-extent, reloads a sight-enabled WATCHING
  rival, and holds 200 no-input frames without re-engagement. Observed final gate:
  registry **47 -> 48**; Null **48/48** (**37 run / 11 expected skips / 0 zero-frame**);
  boot unchanged **2708 / 2707 / 0 / 1**; Vulkan build green; full windowed **48/48 run /
  0 skipped / 0 zero-frame**; focused test **510 frames** on both backends. Three
  exact-one-anchor compiled mutations red the focused result: HP-only whiteout heal
  (**510**), `FromHome` destination (**510**), inverted win-only reward guard (**311**);
  every restoration rebuilt and returned **1/1** green before the final full gates.
- **Known-limit W3 -- the visual spotted beat (COMPLETE 2026-07-28, ZM-D-159):**
  **+4 boot units, automated registry deliberately UNCHANGED at 48** -- the new live
  coverage rides the existing `ZM_TrainerSightWalkUp_Test` and
  `ZM_RivalVesperAuthored_Test` registrations, because both subjects already existed
  (the same "did the coverage ride an existing registration?" rule SC7 established).
  The three new FSM units pin the ordered beat (first sighting emits NO action and
  enters SPOTTED; the bark starts only when the duration elapses), the two-arm
  cancel/restart (lost sight AND closed gate, each clearing the partial timer and each
  counting a second distinct beat on re-entry), and the busy-pause plus degenerate
  fail-open (thirty busy seconds contribute exactly zero elapsed time; a zero duration
  then fails open to the bark). The fourth unit,
  `Interactable_SpottedIndicatorSubmitsOneReadableExclamationMark`, runs before the main
  loop so the renderer cannot drain the CPU queues between the synchronous `Add` calls
  and inspection: it asserts exactly one line and one sphere and NO cube/capsule/
  cylinder/triangle, every literal coordinate/thickness/radius/colour, dot-above-stem
  readability, the helper's MEASURED return value, its non-finite-centre refusal arm,
  and restores every queue to its prior contents even when the payload is wrong. The
  totality sweep grows from window PAIRS to independent TRIPLES (6,912 -> 41,472) and
  gains a genuine SPOTTED seed arm.
  **★ THE COVERAGE DEFECT THE REVIEW CAUGHT, and the reason this is not a proxy suite:**
  the per-frame indicator counter every automated assertion reads was first written as a
  bare `++` BESIDE the submit call, so deleting the call would have kept every test green
  with nothing drawn -- and the boot unit could not see it either, because it calls the
  static helper itself. The fix is structural, not another assertion:
  `SubmitTrainerSpottedIndicator` now returns a value MEASURED off Flux's own CPU
  instance queues and the caller `+=`s it, so a submission that never reached the
  renderer cannot be reported as one.
  Observed gate: boot **2708 -> 2712** (`2712 / 2711 / 0 / 1`), `zm-tests.yml` bumped
  from the OBSERVED line; registry unchanged **48**; engine reference **1164** untouched;
  Null headless **48/48**; full windowed **48/48 passed / 0 failed / 0 skipped / 0
  zero-frame**; `ZM_TrainerSightWalkUp_Test` **754 frames**,
  `ZM_RivalVesperAuthored_Test` **355 frames**; no new `.cpp`, scene, asset,
  serialization version or ECS order, and therefore no regen.
  **Six exact-one-anchor mutations**, each rebuilt with its exit code checked and each
  result parsed off the OBSERVED line, then restored: shipped duration `0.35f -> 0.0f`
  (**17 units**); SPOTTED cancellation `||` -> `&&` (**exactly 1 unit**); remove the
  single `AddSphere` (**exactly 1 unit**); invert the runtime post-`Step` submit guard
  (**both automated tests**, and correctly NO boot unit -- no boot unit drives
  `TickTrainerSight`, which is a coverage BOUNDARY, not a gap); invert the elapsed
  accumulation `+=` -> `-=` (**17 units**); and remove the live Flux submission
  (**1 boot unit AND both automated tests** -- the mutation that would have SURVIVED
  before the review fix, and the proof it no longer does).
- **Known-limit W4 -- rival visual distinctness (COMPLETE 2026-07-29, ZM-D-160):**
  **+4 boot units, automated registry deliberately UNCHANGED at 48.**
  `HumanGen_PaletteTotality` sweeps every valid `ZM_HUMAN_ID` plus `ZM_HUMAN_NONE`,
  `ZM_HUMAN_COUNT` and garbage under a `Zenith_AssertCaptureScope`, requiring finite
  in-[0,1] channels, a DEFINED fallback out of range, and ZERO asserts.
  `HumanGen_PaletteDistinctness` pins the palette-coupled half (colours stay >= 0.15
  apart); `Npc_RivalAppearanceIsDistinctFromEveryOtherRow` and
  `Npc_AuthoredAppearancesAreMutuallyDistinct` pin the ROSTER-coupled half, so an
  `m_eHuman` edit is what reds them. Both roster units use a GUARDED walk whose
  comparison count is itself asserted -- a walk that skipped rows would otherwise pass
  having compared nobody.
  **★ THE ROSTER COLLISION IS ENCODED HONESTLY RATHER THAN ASSUMED AWAY:** six authored
  rows wear five appearances (`Npc_Wanderer` and `Npc_Warden` share
  `ZM_HUMAN_TOWN_ELDER`), so the unit asserts that rows naming DIFFERENT humans are >=
  0.15 apart, rows naming the SAME human are EXACTLY equal (which is what makes the
  "different" arm a discriminator rather than a coincidence), and the roster keeps >= 5
  distinct appearances. Fixing the collision can only raise that count, so the honest
  encoding cannot obstruct the fix.
  `ZM_RivalVesperAuthored_Test` gains a material scan over the COMMITTED scene, logging
  `blocks / blocksOffGrey / npcBodies / npcStillGrey / otherNpcColours / vesperSampled /
  vesperExpected / paletteError / vsGrey / vsNearestNpc` and requiring the sampled colour
  on the authored rival to equal the compiled palette within 1e-4.
  **★ AN HONEST COVERAGE BOUNDARY, demonstrated rather than claimed:** `ZM_GreyboxVisual`
  is file-local to `Zenithmon.cpp` and cannot be named from a `Tests/` TU, so no boot unit
  can construct one -- the live scan is its only coverage. Mutation M1 shows exactly that.
  Observed gate: boot **2712 -> 2716** (`2716 / 2715 / 0 / 1`), pinned from the OBSERVED
  line; registry unchanged **48**; engine reference **1164** untouched; Null headless
  **48/48**; full windowed **48/48 passed / 0 failed / 0 skipped / 0 zero-frame**; no new
  `.cpp`/`.h`/folder, no ECS order, no serialization change, no scene byte, no regen.
  **Four exact-one-anchor mutations**, each rebuilt with its exit code checked and each
  result parsed off the OBSERVED line, then restored: drop the NPC wiring (**automated
  test RED at its full 355 frames, boot gate correctly GREEN** -- the boundary above);
  out-of-range returns roster row 0 instead of the fallback (**exactly 1 unit**); collapse
  the hair term into the outfit primary (**exactly the 2 distinctness units**, confirming
  same-outfit rows really are separated by hair rather than by comment); and replace the
  separation metric's sum of squares with a product (**all 3 distinctness units**, proving
  those clauses are live rather than vacuously satisfied by a large number).
- **Known-limit W5 -- per-NPC measured feet heights (COMPLETE 2026-07-29, ZM-D-161):**
  **+6 boot units and +1 automated registration (registry 48 -> 49).** The boot units pin
  spread, anchor distinctness, accessor totality (under `Zenith_AssertCaptureScope`),
  centre arithmetic with fail-closed half-extents, the wanderer's extra settle clearance,
  and the XZ derivations. **Their comment states in as many words that they are a
  SELF-CONSISTENCY claim about compiled constants and cannot prove those constants match
  the terrain** -- that is the probe's job, and the battery below proves the division is
  real rather than rhetorical.
  `ZM_DawnmereNpcGroundTruth_Test` (in the existing `ZM_AutoTests_NpcTalk.cpp`) is the
  ORACLE: a real downward raycast at each anchor's XZ against the baked terrain body, with
  **two deliberately separate clauses** -- constant-vs-terrain and committed-bytes-vs-terrain
  -- so moving the constants without re-authoring reds the second and re-baking the
  heightmap without re-measuring reds the first, and neither can mask the other.
  **★ ZM-D-185 (2026-08-07): the second clause carries an AUTHORED CLEARANCE term** and
  covers all six rows rather than five. Dawnmere's two DYNAMIC humans (the wanderer and
  rival Vesper) are authored one capsule half-extent above their resting centre on purpose
  (ZM-D-184), so the expectation is `terrain + halfExtent + clearance`. The clearance is
  obtained by subtracting each `*SpawnY` accessor from that NPC's resting centre rather
  than restated as a literal -- which both keeps it in step with the authoring and, because
  the two terms share one W5 feet height, **cancels the compiled table** so the clause stays
  a bytes-vs-TERRAIN claim. The wanderer's former ~1.2 m-wide band clause is deleted: it is
  strictly contained in the new window and could not have caught a missing re-author. It **logs
  every measured height at INFO on every run, pass or fail**, because that log IS the
  re-measurement workflow; a log-on-failure-only version would become useless the moment it
  went green. Its two-sided clause is `max(measured) - min(measured) >= 0.05` taken off the
  LIVE heightfield.
  **★ THE NEGATIVE CONTROL WAS BUILT INTO THE SEQUENCING AND IT FLIPPED.** The tree was
  first built with the anchor table still holding the shared value: gate **2722 / 2719 /
  2 failed**, exactly the spread and distinctness units, other four green. Pasting the seven
  measured constants and changing nothing else: **2722 / 2721 / 0**. Red-then-green on one
  binary shape.
  Observed gate: boot **2716 -> 2722**, pinned from the OBSERVED line; registry **48 -> 49**;
  engine **1164** untouched; Null headless **49/49**; full windowed **49/49 / 0 failed /
  0 skipped / 0 zero-frame**. `Dawnmere.zscen` re-authored under the full proof (two
  `AUTHOR_DAWNMERE` boots, identical SHA256, exactly one tracked asset moved, navmesh
  byte-unchanged, re-hash after the batches identical).
  **Four exact-one-anchor mutations**, each rebuilt with its exit code checked and each
  result parsed off the OBSERVED line, then restored. Three redded the boot gate:
  out-of-range returns roster row 0 (**1 unit**), centre = feet + half x 0.5 (**2 units**),
  wanderer drops its extra clearance (**1 unit**).
  **★ THE FOURTH SURVIVED THE BOOT GATE AND THE CAUSE WAS DETERMINED, not assumed** (fifth
  tripwire): reverting the WARDEN's single row to the shared value left the boot gate clean,
  because the spread unit still sees 1.509 m and the distinctness unit still sees 5 of 6
  rows against its 4-of-6 threshold. That is **correct layering, not a hole** -- the probe
  caught it instantly and named the exact defect (`the compiled feet height 25.98577 is
  1.36779 m off the terrain surface 24.61798`) while `centreError=0.00000` showed the
  committed-bytes clause still passing. A single-row revert is the ORACLE's job by design.
- P1 trainer battle: sight cone -> forced approach -> dialogue -> battle ->
  defeat flag + prize money.
- **★ THE SUITE'S FIRST PIXEL-LEVEL ASSERTIONS (2026-07-30, ZM-D-169). Registry 49 -> 50.**
  Until this commit **no test in this suite asserted anything about a rendered pixel**, which is
  why ZM-D-168's visual audit found four things a 49/49 green suite could not see. Two now do:
  - **`ZM_NpcRenderedPalette_Test`** (NEW registration, `m_bRequiresGraphics = true` so it SKIPS
    headless and runs windowed only). Additively loads the committed Dawnmere, resolves the six
    LIVE NPC model entities through their serialized `ZM_Interactable` rows, holds them in an
    eye-level lineup under deliberately hostile lighting (one overhead directional, IBL off,
    camera horizontal), takes a `Flux_Screenshot::RequestDump`, then maps the six projected body
    centres through the tools viewport into the real BGRA and requires all 15 pairwise separations
    >= 0.15. Observed: minimum **0.2001** (Wanderer/Vesper), per-body RGB 0.34-0.84 per channel.
  - **`ZM_RivalVesperAuthored_Test`** gains a marker clause: `Graphics/Primitives/Enabled` is held
    FALSE for the whole run and a frame-exact dump is taken from inside a real SPOTTED frame.
    Asserts presence (**118 marker-hue px**, 106 in batch) AND a tall-and-narrow span (7x28,
    height >= 2x width), which is what separates an upright exclamation mark from the old diagonal
    stroke. Gated on `Zenith_IsNullRenderer()` -- `Flux_Screenshot::RequestDump` has exactly one
    consumer (`Zenith_Vulkan_Swapchain::EndFrame`), so on Null it writes nothing and the clause is
    correctly skipped rather than failed. **Mutation-proven:** restoring
    `if (!m_bPrimitivesEnabled) return;` reds it with "reached Flux's queues but NOT the
    framebuffer: 0 marker-hue pixels", exit 1.
  **★ TWO CONVENTIONS THIS ESTABLISHES FOR EVERY FUTURE PIXEL TEST, both learned the hard way:**
  1. **Use the frame-exact engine dump, never a wall-clock screen scrape.**
     `Tools\capture_viewport.ps1 -IntervalMs 40` delivers an ACTUAL 206 ms at 2560x1440 (PNG encode
     dominates); a 0.35 s beat gets 1-2 samples.
  2. **Derive the colour predicate from the bytes the engine wrote, never from the colour you
     submitted.** An unlit marker submitting linear `(1.0, 0.82, 0.08)` lands at
     `RGB(208, 182, 97)` -- blue/red 0.47, not 0.08. Two "low blue" scans reported ZERO matches
     across 539 frames of a marker rendering perfectly, and nearly booked a render defect.
     Verify the hue is UNIQUE frame-wide before choosing a threshold.

- **★ THE THIRD PIXEL TEST, AND THE ONE WHOSE HOLE WAS SHARPEST (2026-07-30, ZM-D-170).
  Registry UNMOVED at 50** -- it extends `ZM_BattleMenuRun_Test` rather than registering anything.
  That test had ALREADY dwelt 90 frames in `ACTION_ROOT` (`iBM_RUN_VISUAL_DWELL_FRAMES`, capture at
  the midpoint) and ALREADY written a real swapchain TGA -- and asserted only `DiskFilePresent(...)`
  plus UI visibility and button text on it. **A capture sat on disk that no assertion opened:
  evidence produced and never read, which reads as coverage and is not.** Six arms now read it,
  closing both halves of ZM-D-168's audit finding 2 from ONE frame:
  - **The HUD.** Enemy HP bar chroma **G-R +0.428 / G-B +0.334**; the three root buttons at
    luminance **0.586-0.631** against the panel interior strip they sit on (delta
    **+0.310..+0.355**); the battle text log **522** glyph-white px against **0** in a same-sized
    negative-control box directly above it.
  - **The creature models.** Both platforms carry a rendered `Fernfawn`, projected through the LIVE
    battle camera off `ZM_BattleDirector::GetCreatureModelEntityID`: body vs local background 1 m to
    either side **0.191 / 0.234** and **0.219 / 0.241**; the two bodies read alike to **0.140**;
    each **0.918 / 1.052** clear of its own slab.
  **★ FOUR CONVENTIONS THIS ADDS FOR EVERY FUTURE PIXEL TEST:**
  1. **Threshold-setting is a TWO-SIDED measurement.** Every constant here is centred between an
     observed PASS and an observed FAIL (from the mutations), not set below whatever the green run
     produced. A threshold with only a pass sample behind it is a guess.
  2. **★ ONE ARM PER CLAIM IS NOT ENOUGH, and this is the concrete proof.** With both creature
     models dropped, the body-vs-background arm still **PASSED** on the player side (0.834 / 0.927)
     because that projected point lands on pale stone. The arm that caught it was written as a mere
     sample-placement guard and fired at 0.007. Half the defect would have shipped.
  3. **★ DO NOT TIGHTEN A THRESHOLD ONTO A PROPERTY OF THE BACKGROUND.** Suppressed, the enemy HP
     bar's rect reads the sky behind it at green **0.749** -- clearing the 0.60 level floor, so only
     the CHROMA arms fire. Raising the floor would make the clause turn on what happens to be behind
     the bar, which is not a property of the HUD. Level floor stays a sanity bound; chroma
     discriminates (28x margin).
  4. **Map geometry from the LIVE object, never by respelling the authoring constants.** Element
     rects come from each element's own `GetScreenBounds()`, the panel's reference strip from the
     panel's and first button's own bounds, and world points from the live camera's own matrices --
     then through the tools viewport rect as
     `pixel = viewportPos + canvas * (viewportSize / canvasSize)`. **And the latch is deliberately
     NOT gated on `IsVisible()`**: the visible flag is an INPUT to rendering, and gating on it made a
     hidden element red as "geometry could not be latched" instead of "this never reached the
     framebuffer".
  **The ImGui collision is real on this capture, not precautionary:** a frame-wide "bright green"
  scan matches the Console panel's tick marks at x[1089,1142] y[436,447] alongside the HP bar, so
  every region is clipped to the viewport rect. The glyph predicate is strict (all channels >= 220,
  spread <= 25) because the pale stone platform measures (228, 203, 199).
  **Mutation-proven x3**, each rebuilt with its exit code checked and each result parsed off the
  OBSERVED line: models dropped but entities kept reds three arms (0.007 / 0.068+0.001 / 0.851);
  log + enemy HP bar hidden reds exactly those two; the three root buttons hidden reds exactly those
  three. **A fourth was DISCARDED as too strong rather than recorded as a pass** -- removing the
  creature ENTITIES aborts the latch, so it reds before any pixel is read and proves a different arm.
  **No `Zenith_IsNullRenderer()` guard, and that is the right way round:** the test is
  `m_bRequiresGraphics = true`, so it SKIPS on Null and can never reach the clause with a dump that
  was never written.

- **★ THE FIFTH CONVENTION, AND THE ONE THAT COST A RETRACTION (2026-08-01, ZM-D-177).
  AN ABSOLUTE FRAMEBUFFER RATIO IS A PROPERTY OF THE SCENE'S LIGHTING, NOT OF THE
  MATERIAL. ONLY A RELATIVE SEPARATION BETWEEN TWO SURFACES IS A PROPERTY OF A TINT.**
  `ZM_InteriorTintPixels_Test` (5.8) originally carried, alongside its relative margin,
  two ABSOLUTE red/blue bounds, reasoned as: "the shipped blockout grey is COOL
  (B > G > R), therefore an untinted room lands BELOW 1.0 and a tinted room ABOVE it."
  The first windowed run disproved it -- ProfLab's UNTINTED floor measured red/blue
  **1.0742**, above the bound that demanded it stay under, and PlayerHome's tinted floor
  **1.3045**. **BOTH sit above 1.0.**
  The bound silently assumed **albedo ordering survives to the framebuffer.** It does
  not: a rendered pixel is albedo TIMES illuminant, and under ZM-D-171's
  physically-grounded lighting the illuminant is WARM (a sun key derived from atmosphere
  transmittance, plus ground-bounce IBL), so a neutral-to-cool albedo under a warm
  illuminant still renders WARM. Both rooms are open-topped seven-block shells with no
  ceiling, making the floor's top face the most sun-exposed surface in the room -- the
  0.887 figure the bound leaned on came from `ZM_ShellLighting_Test`'s SUN-AVERTED,
  ambient-only face, the opposite lighting regime.
  **So the bound would have failed on an UNTINTED room, before the tint existed.** It
  never encoded a property of the tint. It was RETRACTED, not weakened; the signed
  relative margin (PlayerHome MINUS ProfLab >= 0.15) survives because the only way to
  satisfy it is for the tinted room to render measurably warmer than the untinted one --
  a tint that never reaches the framebuffer collapses the gap, and a tint that LEAKED
  into the lab warms both rooms together and also collapses it.
  **Generalised rule for every future pixel test:** pin a DIFFERENCE between two
  surfaces measured in the same regime. If an absolute bound is genuinely needed, it is
  a *capture-sanity* band -- applied identically to both samples, deliberately generous,
  and labelled as NOT evidence of the property under test (that probe's 0.70-3.00
  red/blue band exists only to catch a framing regression that put one patch on open
  sky, which could widen the gap and pass falsely). An absolute ratio pinned tight is a
  tripwire for ordinary atmosphere re-tuning: it reds for a reason unrelated to the
  thing it claims to test. **This is also why the ZM-D-170 conventions above --
  especially "threshold-setting is a TWO-SIDED measurement" -- are necessary but not
  sufficient: the retracted bound HAD a measurement behind it. What it lacked was a
  measurement of the NEGATIVE case, which is the only thing that would have shown the
  premise was false.**
- **★ AND A PIXEL TEST'S NUMBERS MUST BE RECORDED BY HAND.** See the standing hazard in
  section 3: a `--filter` run is per-process, per-process discards child stdout, and the
  result JSON's `"failures"` is a hardcoded `[]`. Every calibrated figure in 5.8's pixel
  probe was therefore measured OUT OF BAND by the orchestrator off the dumped TGAs, and
  is labelled as such at the callsite. A pixel test whose observed values nobody
  transcribes has produced evidence and read none of it.

### 5.8 S8 -- vertical slice

#### ZM-D-181 -- the human bodies (12 new boot units, registry UNMOVED at 56)

**Boot baselines at this change: ZM 2861, engine (Null Combat) 1242.** Both moved because the
change touches `Zenith/` as well as the game.

| Case | File | What it pins |
|---|---|---|
| `HumanGen_BodyMetricsPinned` | `Tests/ZM_Tests_HumanGen.cpp` | Re-derives BOTH v2 anchor constants from a freshly built mesh; pins the canonical row is the 1.0 reference build AND still wears an attachment; varies every attachment slot and hair style and requires the BODY metric not to move by one float. |
| `HumanGen_BindSpaceCentreAnchored` | " | The anchor is a RIGID translation of rig + mesh. FK-resolves four bones, three and four joints deep, and requires each to be its v1 world height minus exactly the anchor -- which is what catches "subtracted from all 16 bones" (that compounds). The canonical body sits on the origin; every other model lands at `canonicalCentre * (heightScale - 1)`; accessories may exceed the body box, and at least one must, or the body-prefix measurement proves nothing. |
| `HumanVisual_BodyContractIsTheShippedBody` | `Tests/ZM_Tests_HumanVisual.cpp` | The migration's receipt: the compiled contract is BIT-IDENTICAL to what the retired scale-derived formula returned, plus the feet<->centre conversion and the ground-probe reach. |
| `HumanVisual_BlockoutIsUntouched` | " | A wall still gets the UNIT cube in the exact shipped grey, and never acquires a human model, an animator or an explicit body. |
| `HumanVisual_ColdFallbackShipsTheContractBlock` | " | Forced cold: the palette block draws at exactly 0.8 x 1.8 x 0.8 after the uniform authored scale, and its COLLIDER is the contract -- the cold path is a picture problem, never a gameplay one. |
| `HumanVisual_ColdStartIsIdempotent` | " | Three extra `OnStart`s append nothing (`AddMeshEntry` APPENDS). |
| `HumanVisual_AssetPolicyRestoresTheProductionDefault` | " | A scoped forced-cold override leaves the production policy AND its bake latch exactly as it found them. |
| `HumanVisual_RestartPreservesTheAnimatorRig` | " | **The only unit that reaches the WARM branch** (ZM-D-182). Under the PRODUCTION policy, a repeated `OnStart` must re-bind the controller without WIPING the rig (states go null) or DUPLICATING it (layer count grows) -- the two failure modes pull in opposite directions, so a fix for one that causes the other cannot pass. It is the first check of the claim `EnsureHumanAnimator` rests on: that `Flux_AnimationController::Initialize` re-initialises EXISTING layers rather than dropping them. On a cold tree it asserts the fallback's own invariants and logs which branch ran, so it is never vacuous; the body-contract clause is asserted on BOTH branches. ★ The only case in this file that reads disk. |
| `Collider::ColliderExplicitCapsuleDimensions` | `Zenith/Core/Zenith_UnitTests.Tests.inl` | Units MEASURED off the live Jolt body with raycasts, at a UNIFORM scale (the case that motivates the feature); rigid-body type preserved; the shape survives a later re-scale. |
| `Collider::ColliderExplicitBoxHalfExtents` | " | Half-extents ignore the transform scale entirely, carry a ZERO local offset, and the debug wireframe agrees because both route through `ComputeBoxDimensionsAndOffset`. |
| `Collider::ColliderExplicitDimensionsFailClosed` | " | No live body -> warn and leave NO state behind (OBSERVED: the collider added afterwards still comes out scale-derived); non-finite/non-positive input leaves the body untouched; a box setter refuses to guess between AABB and OBB. |
| `Collider::ColliderExplicitDimensionsNoOpOnMatch` | " | The no-op compares against the CLAMPED STORED values, so a request that clamps onto the current shape does not churn the body -- which is what keeps the body ID stable for `ApplyDrivenBodySetup`'s identity key. |
| `Collider::ColliderExplicitBoxSurvivesMove` | " | The explicit fields survive a component MOVE; a relocating pool would otherwise revert the body to scale-derived sizing at some later, unrelated moment. |

#### World-scale contracts (two new game-only boot units)

**Boot baseline at THIS change: ZM 2863; engine (Null Combat) stays 1242.** The added
contracts are pure generator/placement checks, so they do not move the 55-test
automated registry or the cross-game engine suite.
★ **SUPERSEDED -- the CURRENT baseline is ZM 3127, engine 1502**, registry unmoved
at **55**. OBSERVED 2026-08-10 on clean `Null_` builds:
`3127 ran, 3125 passed, 0 failed, 2 skipped`. Most recent move: +4 ENGINE units
(3123 -> 3127, engine 1498 -> 1502) from the TAA sky motion vectors -- the
`TAASkyVelocity` cases in `Flux/TAA/Flux_TAAJitter.Tests.inl`, pinning the w = 0
point-at-infinity reprojection the sky's velocity-only pass rests on (camera
TRANSLATION must not move the sky; a pure yaw must, linearly). Previous move: +6 ENGINE units
(3117 -> 3123, engine 1492 -> 1498) from the TAA disocclusion fix -- the suite in
`Flux/TAA/Flux_TAA_ResolveCPU.Tests.inl` added when TAA's disocclusion test was
found to compare the history's stored depth against the CURRENT frame's depth
instead of the reprojection its own contract specified, and against a POINT rather
than the neighbourhood depth RANGE. Before that: +11 ENGINE units
(3106 -> 3117, engine 1481 -> 1492) from the Flux screen-space-quality review
follow-ups -- six SSAO committed-selection / blur-constant units and five shadow
quality-flag units. A `SlangProbes` probe added alongside them moves NEITHER
number: that suite is `ZENITH_WINDOWS && ZENITH_VULKAN`-gated in
`Flux_MaterialTable.cpp` and both gates run `Null_` builds. Earlier derivation: ZM-D-183 added the two
committed-scene-bytes guards (2906 -> 2908); ZM-D-184 added the rival spawn-clearance
unit (2908 -> 2909); the 2026-08-05 CommandLine ParseArgs units added 5 ENGINE units
(engine 1284 -> 1289) and were NOT reflected here, leaving the ZM pin stale at 2909
against a real 2914; the terrain G-buffer pipeline-variant units added 6 more ENGINE
units (engine 1289 -> 1295), taking ZM to 2920; the `FluxTerrainSourceGrid` units --
the terrain exporter's border-chunk fix -- added 5 more ENGINE units
(engine 1295 -> 1300), taking ZM to 2925; the GPU-grass overhaul's Phase-1
`FluxGrassTypes` suite added 20 more ENGINE units (engine 1300 -> 1320),
taking ZM to 2945; its Phase-2 `FluxBufferReadback` units (the
`DownloadBufferData` headless zero-fill contract) added 2 more ENGINE units
(engine 1320 -> 1322), taking ZM to 2947; its Phase-3 TerrainEditor GrassType
units (hard-edged dab, 1-byte-texel undo round-trip, GrassType.ztxtr round-trip,
sculpt-notify latch) added 4 more ENGINE units (engine 1322 -> 1326), taking
ZM to 2951; its Phase-4 swap deleted the 3 legacy member-poking grass tests and
added 14 (shader mirrors / impl surface / type-table serialization), +11 net
ENGINE units (engine 1326 -> 1337), taking ZM to 2962; its Phase-5 shadow-casting
units added 3 more ENGINE units (engine 1337 -> 1340), taking ZM to 2965; its
Phase-6 displacement re-anchor units added 4 more ENGINE units (engine
1340 -> 1344), taking ZM to 2969; its Phase-7 types-authoring units added 6
more ENGINE units (engine 1344 -> 1350), taking ZM to 2975; and the 2026-08-07
debug-variable audit remediation added 2 more ENGINE units (engine
1350 -> 1352) -- the AI debug-draw routing pins that came with giving
`Zenith_AIDebugVariables::Initialise()` its first call site -- taking ZM to
2977; and the compressed-vertex work's Phase 1 added 15 more ENGINE units
(engine 1352 -> 1367) -- the `Flux_VertexCodec` suite, whose SNORM10 case
pins the terrain packer bit-for-bit against the copy it replaced -- taking ZM to
2992; and that work's Phase 2 T2.a added 10 more ENGINE units (engine
1367 -> **1377**) -- the `Flux_ShaderReflection` vertex-input suite that comes
with the v6 `.spv.refl` sidecar (tight-pack offsets/strides, per-binding
independence, the empty table, the v6 round-trip, the adopt-once stage merge,
and the pure `[VtxFmt]` vocabulary / inference / override validation) -- taking
ZM to 3002; and that work's Phase 2 T2.b added 11 more ENGINE units (engine
1377 -> **1388**) -- the `Flux_VertexLayoutDesc` equality/vocabulary suite plus
three Codegen emission units, arriving with the baked `kaxVertexAttribs[]` /
`kVertexLayout` constants every generated shader header now carries and the
`[VtxFmt("snorm10_10_10_2")]` annotations that make the terrain table read its
locked 28-byte stride -- taking ZM to 3013; and that work's Phase 2 T2.d review
hardening added 12 more ENGINE units (engine 1388 -> **1400**) -- the
vertex-layout-validation suite that drives the boot tripwire's pure comparator
through every branch real boots never take (the OK path, both no-input
spellings, and the COUNT / per-binding STRIDE / ELEMENT / unknown-SEMANTIC
mismatch categories) -- taking ZM to 3025; and that work's Phase 3 T3.a added 15
more ENGINE units (engine 1400 -> **1415**) -- the `Flux_PackVertices` suite that
arrives with the reflection-driven packer replacing the engine's hand-written
vertex-interleave loops (the whole-layout byte contract over two mixed-format
tables, the canonical attribute defaults, `(semantic, index)` source keying, the
4-lane TANGENT's derived handedness in both signs plus an authored w winning, the
SNORM10 pre-normalisation and its zero-length fallback, the lane pad/truncate
rules, the POSITION quant box, stride-advance across three vertices, the no-op
shapes, and binding-1 skipping) -- taking ZM to **3040**; and that work's Phase 3
T3.b added 4 NET more ENGINE units (engine 1415 -> **1419**) -- the mesh-family
replacement swapped four hand-written interleave loops for two single writers
(`Flux_PackStaticMeshVertices` through the reflected static table, and
`Flux_BuildSkinInputVertices` for the skin-input stream whose uint bone
lanes the packer refuses by design) and rewrote that suite from 3 hand-layout
tests to 7: three byte-for-byte memcmp goldens against a FROZEN transcription of
the loop each writer replaced -- the two mesh-asset streams over 7 adversarial
meshes (missing attributes, arrays one entry short of the vertex count, degenerate
frames, a short bone-weight array, negative zero), plus the procedural geometry
builder's DYNAMIC layout with and without its uint bone tail -- and the ported
layout/defaults expectations and the bind-pose position override -- taking ZM to
**3044**; and that work's Phase 4 T4.a added 5 NET more ENGINE units
(engine 1419 -> **1424**) -- the MESH COMPRESSION FLIP. The static-mesh vertex
went 72 -> 24 bytes (half4 position, half2 UV, snorm10 normal, snorm10 tangent
whose w carries the bitangent SIGN, unorm8x4 colour) and the compute-skinning
input 104 -> 32; the BINORMAL attribute is deleted outright and every consumer
rebuilds the vector as `cross(N, T.xyz) * T.w`. The two mesh-family memcmp
goldens are therefore re-derived: they now compare against an INDEPENDENT
transcription through `Flux_VertexCodec` (the frozen float32 loops describe bytes
the flip changes by design), joined by the skin-input/static prefix identity, the
mirrored-frame bitangent sign, the procedural mesh-pipeline pack, the packed
skin-vertex codec round trip and the one-byte bone sentinel -- taking ZM to
**3049**; and that flip's adversarial-review fix pass added 7 more ENGINE units
(engine 1424 -> **1431**) -- hand-derived half word pins (the one storage format
whose tests were previously all symmetric round trips), a frozen C++
transcription of the GPU-side Slang half codec swept against the CPU codec with
the licensed rounding-tie divergence pinned as numbers, the fast-math-safe
positive-finite predicate's branch proofs, the over-unity bone-weight
proportional pre-scale (the one-lane repair used to DELETE the dominant
influence at sums past ~1.5), the HALF4 half-range position guard
(capture-scoped), the skin-output-encoder-vs-static-packer byte identity
(fallback defaults included -- the tangent w pad had drifted exactly there), and
the mirrored (negative-determinant) blend flipping the packed bitangent sign for
parity with the uncompressed path -- taking ZM to **3056**; and the compressed-vertex
Phase 5 T5.a terrain compression flip added 2 more ENGINE units
(engine 1431 -> **1433**) -- the terrain vertex went 28 -> 20 bytes (SNORM16x4
position quantised against the AUTHORED terrain box + UNORM16x2 UV), which gave
`Flux_DequantPosition` (`Shaders/Common/VertexFormats.slang`) its first caller, so
its GPU<->CPU agreement is now pinned by a frozen C++ transcription of the Slang
function plus the fixed-function `VK_FORMAT_R16G16B16A16_SNORM` fetch conversion,
swept against the CPU codec over box corners / quantum boundaries / three boxes,
and by the out-of-box clamp -- taking ZM to **3058** (all three games' terrain bake
stamps moved in the same change: ZM manifest v3 -> v4, CityBuilder v6 -> v7,
RenderTest v9 -> v10); and the T5.a adversarial-review fix pass added 4 more ENGINE
units (engine 1433 -> **1437**), all in `Flux_Terrain.Tests.inl` -- the
TerrainConstants CB fill vs the authored box (VALUES, where the static_asserts pin
only layout), the quant-bridge writes landing at the SHADER's reflected offsets
(0xCD-sentinel decode), decode->re-encode word idempotence (the property the sculpt
hook's and CityBuilder carve's seam safety stands on), and the UV write/read round
trip including the integer snap -- taking ZM to **3062**; and compressed-vertex
Phase 6 T6.a, the per-feature instance-stream flips (Text 56 -> 36 B, Quads
72 -> 52 B, Particles 32 -> 20 B, Gizmos 24 -> 16 B), added 19 more ENGINE units
(engine 1437 -> **1456**) -- 3 in `Flux_VertexCodec.Tests.inl` for the new
`uint16x4` lane (order, in-range identity, and the load-bearing saturate-not-wrap
contract: every UI producer casts a screen float to u32 first, so an off-screen-left
rect arrives as ~4.29e9 and a modulo wrap could fold it back into view), and
4/5/4/3 in the new `Flux_TextVertex` / `Flux_QuadInstance` /
`Flux_ParticleInstance` / `Flux_GizmoVertex` `.Tests.inl`, each reading the packed
words back out of the raw object bytes at the GENERATED offsets (an offset
`static_assert` cannot see whether the writer put the right BITS in the lane) plus
the not-compressed rationales -- 4K pixel lanes, far-from-origin world positions,
and the Quads gradient's negative sentinel. The Gizmos three are `ZENITH_TOOLS`-only
like the feature, and every pinned baseline is a `*_True` config -- taking ZM to
**3081**; and the T6.a review fix pass added 1 more ENGINE unit (engine 1456 ->
**1457**) -- the SOURCE-TEXT pin `ParticleInstance.SlangWriterWordCountMatchesTheMirror`,
the only cross-language tie between `Flux_ParticleUpdate.slang`'s uINSTANCE_WORDS and
its C++ mirror (a plain `static const uint` reflects into nothing a static_assert can
reach, so the two spellings could drift with every build and unit green; the pin reads
the shader source and parses the literal, skipping itself with a log when the source
tree is absent) -- taking ZM to **3082**; and the scene-publish guard +
authoring-FP determinism fix (`89fa3647`) added 2 more ENGINE units (engine 1457 ->
**1459**) in `Zenith_Editor.Tests.inl` --
`SceneSaveDeltaClassifiesPublish`, which pins the four verdicts of
`Zenith_SceneData::CompareWithFile` (NO_FILE / IDENTICAL / DIFFERENT / DIFFERENT-and-
lossy) plus the rule that a transient entity never moves the serialized counts, and
`HeadlessSaveNeverRewritesSceneAsset`, which pins the policy end to end and asserts
something in EITHER backend (the file is byte-identical afterwards under `Null_`,
CHANGED under a real one) -- taking ZM to **3084**; and wiring the GPU-driven particle
path added 22 more ENGINE units (engine 1459 -> **1481**) in the new
`Flux/Particles/Flux_ParticleGPU.Tests.inl`. That pass was inert before: its instance
buffer had no reader, its `Initialise` had no caller, and no emitter ever registered,
so the `Flux_ParticleUpdate` compute shader wrote bytes nothing drew. The 22 cover the
spawn RING (7 -- unwrapped, the wrap split, the exact boundary, the over-capacity
clamp, zero capacity, a full-lap walk, occupancy saturation), blend-partition and
indirect-command ADDRESSING (4), the SOURCE-TEXT pin
`ParticleGPU.SlangIndirectWordCountMatchesTheMirror` (1 -- the twin of the
uINSTANCE_WORDS pin; uINDIRECT_WORDS likewise reflects into nothing), pool
registration LIFETIME driven against the live impl (6 -- disjoint carve, the kept
reservation, a reused slot restarting empty, over-capacity refusal, QueueSpawn
reaching only live registrations, `Reset` clearing rings but keeping reservations),
and the frame's CPU HALF (4 -- VRAM shapes, the spawn drain + frame arm, the empty-case
DISARM, the bounds-checked emitter count) -- taking ZM to **3106**. `zm-tests.yml`
`-Baseline` and `run_unit_gate.ps1`'s default both moved to match. The paragraph below is the
ZM-D-182-era snapshot, kept for the audit trail:

★ **The (ZM-D-182-era) baseline was ZM 2864** (ZM-D-182 added
`HumanVisual_RestartPreservesTheAnimatorRig`); engine and registry are unmoved at
1242 / 55. OBSERVED 2026-08-02 on a clean `Null_` build:
`2864 ran, 2862 passed, 0 failed, 2 skipped`. Status.md's top block is the live
figure -- read it, not this paragraph.

| Case | File | What it pins |
|---|---|---|
| `CreatureGen_MeasuredOriginPolicy` | `Tests/ZM_Tests_CreatureGen.cpp` | Measures every generated creature body in metres from a fresh in-memory mesh, checks non-degenerate deterministic bounds, and requires every model to land exactly on its declared grounded (0 m) or hovering (0.25 m) presentation floor. The existing `CreatureGen_SizeClassScaleGolden` test remains the artistic target-scale ladder. |
| `HomeExterior_EnvelopeAndEntranceMatchPlayerHomeContract` | `Tests/ZM_Tests_DawnmerePlacement.cpp` | Requires the Dawnmere facade to contain, but not grow beyond one cosmetic metre of, PlayerHome's authoritative wall envelope; pins the shared 4.0 x 2.5 m aperture, -Z facade entrance, and portal sensor. |

**`ZM_RivalVesperAuthored_Test`'s appearance sample is now WARM/COLD split**, keyed
off `ZM_AreHumanAssetsReady()` -- **the same call the runtime made**, never inferred
from what the scan happened to find (that inference would turn "the wiring is
severed" into "ah, must be cold"). Warm: every NPC wears the `.zmodel` its own row
names. Cold: the palette clauses this test has always run. The blockout half is
unconditional in both.

**`ZM_ProfLabWarp_Test`'s scene-bytes guard** now compares the authored player scale
against `fZM_HUMAN_VISUAL_SCALE`, so a missed re-author still reds.

**★ `ZM_NpcRenderedPalette_Test` WAS DELETED, NOT RE-BASELINED (registry 56 -> 55).**
It was a graphics-required framebuffer test whose `0.04` separation floor was
derived at ZM-D-171 against **palette-coloured blocks**. Those bodies are generated
human MODELS now, wearing baked textures, so the quantity it measured no longer
exists on screen. Re-baselining it would have meant guessing a floor against content
nobody had characterised -- a number that LOOKS like a check. Deleting it states the
gap instead of disguising it.

**What is therefore NOT covered any more:** nothing reads real swapchain pixels off
an NPC body. The surviving coverage is `ZM_RivalVesperAuthored_Test`'s appearance
block, which asserts on COMPONENT state (the model path when warm, the material
colour when cold) and runs for real headless -- so "the row reached the body" is
still gated, but "the six render as six visibly different people" is not.

**If it is wanted back, DERIVE it, do not guess.** ZM-D-171's method: run it, read
the logged separations, run the severed-wiring mutation, read THAT band, set the
floor strictly between the two, and record both measured numbers and the date. And
give it a name that is not about a palette.


**STATE, so nothing below is read as more than it is: S8 item 1 ("Intro -> lab
-> starter choice") is IN PROGRESS and its `Roadmap.md` box is deliberately
UNTICKED.** The professor and the starter-choice SCREEN are NOT built. What has
shipped is the ground under them: the lab interior and its warp (ZM-D-174), the
starter DATA and the seed split (ZM-D-175), and the New Game entry point plus
the bedroom's visual separation from the lab (ZM-D-176/177). The S8 go/no-go
gate follows all four `Roadmap.md` S8 boxes and requires a manual visual
playthrough sign-off; it is a HUMAN stop and no agent may sign it. **Do not
describe S8 as complete, and do not read this section's greenness as "the intro
works".**

These three sub-commits took the automated registry **53 -> 56** (see section 8
for the 51 -> 53 step) and added **+30** boot units, split
+8 `ZM_WorldTraversal` (ProfLab placement), +3 `ZM_WorldTraversal` (PlayerHome
interior), +13 `ZM_Starter` (a NEW category), +4 `ZM_Data` (new-game entry) and
+2 `ZM_Interaction` (the empty-party gate), against one renamed `ZM_Party` case
that nets zero.

**#### ZM_ProfLab -- Aster's Lab interior + build index 41 (item 1, SHIPPED ZM-D-174)**

`Assets/Scenes/ProfLab.zscen` becomes the fifth TRACKED scene; the interior is a
seven-block open-topped shell (floor, back/left/right walls, two front-wall stubs
flanking the doorway, lintel) authored entirely from the new pure header
`Source/World/ZM_ProfLabPlacement.h`. Build index **41** is spelled ONCE, in
`Source/Data/ZM_WorldSpec.cpp`; every consumer resolves it through
`m_uBuildIndex`.

- **T0 `ZM_WorldTraversal` units (SHIPPED -- exactly 8)** in
  `Tests/ZM_Tests_ProfLabPlacement.cpp`. PURE: no scene, no entity, no physics,
  no assets, no `g_xEngine` -- and, per the ZM-D-148 precondition, **they create
  NO entity**, because the boot unit suite allocates entities before scene
  authoring and one entity-creating unit would re-author different `.zscen`
  bytes and invalidate the two-boot identical-SHA256 proof.

  | Test | Contract covered |
  |---|---|
  | `ProfLab_WorldSpecRowIsTheInteriorWithNoTerrain` | The compiled row is kind INTERIOR with an empty terrain set and offers exactly the tag its one inbound connection names; no build-index literal appears anywhere but `ZM_WorldSpec.cpp` |
  | `ProfLab_BlockoutExtentsArePositiveAndFinite` | ANTI-VACUITY, and the clause that makes every other walk mean anything: finite centres and strictly positive extents. A zero/negative scale inverts the AABB (`Min()` past `Max()`), which every containment claim would then evaluate against nonsense while still returning a bool |
  | `ProfLab_DoorSpawnStandsOnTheFloorWithCameraClearance` | The Door spawn stands ON the floor, INSIDE the room, with arm room behind it -- including the feet-vs-centre convention that has bitten this project before (`ZM_SpawnPoint` markers are FEET; the warp adds the capsule half-extent, so a marker authored at a body centre drops the player half a body into the ceiling). The half-extent is MIRRORED from the shipped constant, never re-spelled |
  | `ProfLab_FollowCameraTrailsIntoTheRoomAtTheAuthoredYaw` | ZM-D-173 restated for an interior: one authored yaw for the whole scene, trailing toward -Z, so the doorway must be the +Z face and the room must open BEHIND the arriving player. The header's three mirrored camera constants are asserted against `ZM_FollowCamera`'s own getters, so re-tuning the camera without re-deriving the placement reds immediately instead of the room quietly becoming too short two commits later |
  | `ProfLab_HeaderSpawnTagMatchesTheWorldSpecRow` | The tag is spelled ONCE. Authoring installs `szZM_PROFLAB_SPAWN_TAG` on the marker and `IsWarpDestinationValid` compares against `ZM_WorldSpec`'s offered list; typed twice they could diverge by one byte and the only symptom would be a warp that never completes |
  | `ProfLab_DoorApertureAdmitsTheAuthoredPlayer` | The aperture is an ABSENCE of geometry -- the gap between the two front stubs, capped by the lintel -- so its clear size is measured from the boxes that bound it. Widen either stub until they meet and this says the doorway has been walled up |
  | `ProfLab_BlockNamesAreUniquePrintableLookupKeys` | The keys `ZM_ProfLabWarp_Test`'s clause I looks up. A duplicated name makes two rows resolve to one entity (so a genuinely misplaced block is compared against its twin and PASSES); a name with a space or control byte is a silent miss reported as "entity not found" with no cause |
  | `ProfLab_ShellEnclosesTheFloorOnEverySide` | The four bounding sides reach the slab's edges and stand tall enough that a player cannot see or step over them. A wall one scale component short leaves a corner gap no doorway-only measurement would notice |

  **★ THE BOUNDARY, STATED BY THE FILE ITSELF.** These eight run BEFORE the
  initial scene loads, so they can see NEITHER the scene registry NOR one byte of
  `ProfLab.zscen`. They cannot detect a missing `RegisterSceneBuildIndex` and
  cannot prove the committed bytes match the header. Those are clauses A and I of
  the automated test below. **Their greenness is not "ProfLab is reachable".**

- **P1 `ZM_ProfLabWarp_Test` (SHIPPED, headless-safe)** in
  `Tests/ZM_AutoTests_ProfLab.cpp`: `m_bRequiresGraphics = false`, max **1,200**
  frames, fixed dt **1/60** (C2 default -- it presses no key and drives no
  traversal; every phase waits on the warp machine, so the 30 Hz cadence
  `ZM_PlayerHomeRoundTrip_Test` justifies for its 128 m input drive would only
  halve the resolution of the fade/barrier observations). **NO asset probe and NO
  `RequestSkip` anywhere** -- `ProfLab.zscen` is TRACKED, so absence is a defect
  (the ZM-D-147 deviation under C6). It warps from the PLAYERLESS FrontEnd via
  `RequestWarp` rather than walking a Dawnmere trigger, so there is no git-ignored
  prerequisite left to guard and therefore no split-guard to get wrong. Per-phase
  driver functions (the ZM-D-141 stack rule) even though it holds no
  `ZM_GameState` local, and every placement value is READ from
  `ZM_ProfLabPlacement.h` -- the only numeric literals in the file are epsilons
  and frame deadlines. Three phases with budgets 180 / 600 / 120 summing to 900,
  **deliberately inside the 1,200 cap** so a NAMED deadline fires before the
  harness backstop.

  Ten verification clauses: **A** the active scene really is ProfLab (not merely
  "a warp completed"); **B** the player stands on the marker, at rest; **C** the
  camera is main + aimed and control is handed back; **D** the camera kept the yaw
  every clearance figure assumes; **E** no fade/opaque-load/camera barrier was
  skipped; **F** exactly one SINGLE load, target cleared; **G** the FrontEnd is
  gone; **H** an interior owns no terrain grass; **I** the committed scene bytes
  agree with the header across all seven shell blocks; **I2** the three authored
  entities that are NOT shell blocks (the arrival marker by NAME as well as by
  TAG -- pinning the two lookup keys to each other -- and the player's authored
  scale, from which the doorway width, headroom and capsule half-extent are all
  derived).

  **★ WHY THIS TEST EXISTS, AND WHY NO WorldSpec UNIT COULD REPLACE IT.** The
  ProfLab row had shipped in the compiled world table and all 12 `ZM_Data`
  WorldSpec units were GREEN against it, including the FrontEnd-reachability walk.
  None could see the live wedge, because `IsWarpDestinationValid` consults ONLY
  the compiled tag list and never `Zenith_SceneSystem`'s build-index registry: an
  UNREGISTERED destination passes validation, the warp is ACCEPTED, and the
  machine parks in `ZM_WARP_TRANSITION_WAITING_FOR_SPAWN` -- **which has no
  timeout** -- leaving the player frozen behind a permanently opaque fade. A
  silent hang, not a crash. The WarpIn deadline exists so the symptom is a named
  failure naming the missing `RegisterSceneBuildIndex` line rather than a bare
  frame cap that names nothing.

**#### ZM_Starter -- the starter seed split (item 1, SHIPPED ZM-D-175)**

`Source/Party/ZM_StarterChoice.{h,cpp}` ships the authored trio (Fernfawn/Grass,
Kindlet/Fire, Finlet/Water at `uZM_STARTER_LEVEL = 5`) in the ZM-D-009
compiled-const-table idiom, and **`ZM_MakeStarterGameState` is DELETED**, split
into two: `ZM_MakeNewGameState()` seeds a PARTYLESS new game (dex empty, economy
intact) and `ZM_ApplyStarterChoice(state, choice)` grants the one level-5 lead.
**Any doc, comment or test naming `ZM_MakeStarterGameState` is stale** -- the
between-tests hook's re-seed (C3) is now
`ZM_MakeNewGameState()` + `ZM_ApplyStarterChoice(..., ZM_STARTER_CHOICE_FERNFAWN)`.

- **T0 `ZM_Starter` units (SHIPPED -- exactly 13)** in
  `Tests/ZM_Tests_StarterChoice.cpp`. A NEW category, deliberately distinct from
  `ZM_Party` / `ZM_Save` / `ZM_Data` (per section 1's "new systems get a new
  category, never a grab-bag"): a shared category would bury a starter regression
  among the ~forty party-model units. All pure/headless -- compiled tables, free
  functions over a by-value `ZM_GameState`, and the frozen type chart -- so every
  fixture is hermetic and no `RequestSkip` is needed.

  | Count | Cases / locked contract |
  |---:|---|
  | 3 | `Table_RowsAreDenseAndSelfConsistent`, `Table_EveryStarterIsAStageOneSingleTypedRareSpecies`, `Table_SpeciesAndChoicesAreDistinct`: the roster is dense from zero and self-indexing, every row is a stage-1 single-typed rare species, and no species or choice repeats |
  | 1 | `Accessor_OutOfRangeFailsClosedAndNeverAsserts`: TOTAL accessors under a `Zenith_AssertCaptureScope`, so "it never asserts" is an ASSERTION rather than "the process happened not to die this time" |
  | 2 | `Counter_ColumnAgreesWithTheTypeChart`, `Counter_FormsOneCycleOverEveryChoice`: `m_eCounteredBy` is AUTHORED, not derived -- deriving it by querying `ZM_TypeChart` would make the first unit a tautology that can never fail; authored, it is a real tripwire over both the column and the chart. The second pins that the three counters form ONE cycle rather than degenerating |
  | 1 | `NewGame_IsPartylessAndDexEmptyButKeepsTheEconomy`: the split's whole point -- a new game grants NO monster |
  | 4 | `Apply_GrantsExactlyOneLevelFiveLeadForEveryChoice`, `Apply_MarksOnlyTheChosenSpeciesSeenAndCaught`, `Apply_RejectsAnOutOfRangeChoiceWithoutMutatingTheState` (also under a capture scope), `Apply_TouchesOnlyPartyAndDex` |
  | 1 | `CanEnterBattle_KeysOnPartyEmptinessAndIgnoresFainting`: emptiness, NOT health -- a fainted party can still enter |
  | 1 | `Roster_VesperBringsTheAuthoredCounterToTheFernfawnStarter`: closes half of the GDD counter-starter debt booked at ZM-D-156 |

  **★ THE HAZARD THIS FILE IS WRITTEN AROUND, and the reason its walks look
  defensive.** `ZM_GetSpeciesData`, `ZM_TypeChart::GetEffectiveness` and
  `ZM_BuildMonsterRecord` all `Zenith_Assert` on out-of-range input, and
  `Zenith_Assert` calls `Zenith_DebugBreak()` in EVERY configuration -- there is
  no build in which it degrades to a log. **One bad value does not fail one unit:
  it ENDS the whole ~2,800-unit boot run and takes the gate down.** Every walk
  therefore guards its own indices before dereferencing a table and bails on a red
  rather than pressing on. This applies to any future unit reaching those seams.

- **T0 `ZM_Interaction` units (SHIPPED -- exactly 2)** in
  `Tests/ZM_Tests_TrainerSightFsm.cpp`, for `ZM_MayTrainerEngage`'s new
  `bPlayerCanBattle` input (answered by exactly one seam, `ZM_CanEnterBattle`).
  **The gate LANDED INERT** -- nothing partyless is reachable until the
  starter-choice screen exists -- which is precisely why it needs units now rather
  than a live route later.
  - `Gate_PartylessPlayerIsClosedOnBothArms` walks **all four corners** of
    (defeatFlag, sessionLatch) for **one fixture of EACH kind** (the FLAGGED rival
    and the FLAGLESS rambler), including the two corners that answer TRUE for a
    party-bearing player. Walking only the corners that were already false would
    prove nothing about the new clause.
  - `Gate_PlayerCanBattleNeverOpensAnUnregisteredRow` pins the OUTCOME and
    **deliberately not the order**: the registration guard and the
    `bPlayerCanBattle` clause both return the same `false`, so their relative
    position is unobservable BY CONSTRUCTION -- no input distinguishes the two
    orderings. **Do not add an assertion that appears to check it; it would be
    checking nothing.**

**#### The committed-scene-bytes guard (ZM-D-183, SHIPPED) -- THREE tests, and none substitutes for another**

The `Dawnmere.zscen` "a boot must not dirty a tracked scene" invariant has now been
lost twice on the SAME two float fields (`Npc_RivalVesper`'s rotation y and w), each
time with every existing guard green. The coverage is now split by **what kind of
wrong** it catches, which is the only division that works here:

| Test | File | Catches | Comparison |
|---|---|---|---|
| `ZM_Interaction/Vesper_FacingIsDerivedFromTheTownCentreBearing` | `ZM_Tests_DawnmerePlacement.cpp` | a **STALE** constant -- right bits, wrong bearing (someone moved `fZM_DAWNMERE_VESPER_X/Z` or the town centre without re-freezing) | TOLERANCE (0.0005 on the forward vector) |
| `ZM_Interaction/Vesper_FrozenFacingIsExactlyTheDeclaredBits` | " | a **CORRUPT or BYPASSED** constant -- the accessor no longer returns the declared bits (e.g. someone reverted it to the computed form), or the bits are not a unit quaternion / not a pure yaw | BIT-EXACT vs the declared constants |
| `ZM_CommittedSceneBytes/DawnmereCarriesTheFrozenRivalFacingBitExactly` | `ZM_Tests_CommittedSceneBytes.cpp` (**new TU**) | a **DRIFTED ASSET** -- the committed file no longer carries the frozen rotation. **The regression test: it fails on BOTH historical breaks** (`a6c66b68`'s bytes, and the bytes a pre-ZM-D-183 Release tools boot wrote) | BIT-EXACT vs the committed FILE |

★ **The frozen-vs-DERIVED comparison is deliberately a TOLERANCE and making it
bit-exact would be a bug.** The whole ZM-D-183 finding is that the derivation's last
bits are build-configuration dependent, so a bit-exact frozen-vs-derived assert is
green in Debug and **permanently red in Release** -- a worse tripwire than none. The
bit-exact comparisons go where the value does not move with the config: against the
declared constants, and against the committed file.

**#### The rival's spawn clearance (ZM-D-184, SHIPPED) -- one unit, and it would have failed the day before**

| Test | File | Catches |
|---|---|---|
| `ZM_Interaction/Vesper_SpawnsClearOfTheGroundNotOnIt` | `ZM_Tests_DawnmerePlacement.cpp` | an authored DYNAMIC body placed at its RESTING centre instead of above it |

It asserts the rival's spawn is strictly above his resting centre, that the margin is
a **full capsule half-extent**, that he and the wanderer get the **same** clearance,
and that a degenerate half-extent stays finite. **It fails against the code as it
stood before ZM-D-184**, which is the point: he was authored at feet + one half-extent
(~13 mm of margin) and fell through the world intermittently.

★ **The engine half of ZM-D-184 -- the 8-substep cap in `Zenith_Physics::Update` --
has no unit of its own, deliberately.** What it prevents is a whole-frame timing
condition (a ~0.49 s load hitch draining ~29 consecutive substeps), which a boot unit
cannot construct and an automated test cannot reliably provoke. It is covered
observationally instead: every ZM automated test that loads Dawnmere exercises it, and
the cap logs when it engages (`Physics substep cap hit: 8 substeps ran and 0.085 s ...
DISCARDED`) -- expected exactly once, at boot.

★ **AND NOTE WHAT THE SUITE DID NOT DO HERE.** Every ZM test was green for the entire
period the fall-through was live -- `ZM_RivalVesperAuthored_Test` included, measuring
`idleMax=0.0002 m`. The defect only ever appeared in a windowed Release tools build
driven through the editor's Play mode and the in-game Continue flow, which no
automated test reaches. **A green suite was not evidence here**, and the fix was
confirmed by re-running the reported repro, not by the gate.

★ **Why the third one had to read disk.** Nothing headless checked the bytes that are
actually in git. The pre-save guard `ZM_VerifyAuthoredRivalFacingStep` is bit-exact
but (a) compares against a re-computation of itself in the same binary, so it cannot
see the computation move, and (b) only runs on the windowed `AUTHOR_DAWNMERE` boot,
which **CI never performs** -- `zm-tests.yml` builds `Vulkan_..._True` but RUNS
`Null_..._True`. The new TU is format-agnostic on purpose: it searches for the 16-byte
little-endian quaternion in serialized order rather than parsing the `.zscen`
container, so it does not rot at the next schema bump. It reports NOT APPLICABLE (a
warning, not a pass) when the asset is unreachable, e.g. a packaged `--assets-root`
run.

**#### ZM_PlayerHome -- New Game entry + the warm interior tint (item 1, SHIPPED ZM-D-176/177)**

USER RULING (ZM-D-176): a new run now begins at PlayerHome build **40** at tag
`"Door"` (`ZM_GameStateManager::uNEW_GAME_BUILD_INDEX` /
`szNEW_GAME_SPAWN_TAG`), and the bedroom is tinted slightly warm so it stops
reading as the same greybox room as Aster's lab. ProfLab is left EXACTLY as it
is. The tint is DERIVED at runtime from the entity name and is **not
serialized**, so the change moves no scene bytes.

- **T0 `ZM_Data` units (SHIPPED -- exactly 4)** in
  `Tests/ZM_Tests_NewGameEntry.cpp`, pure and entity-free:
  `NewGameEntry_DestinationIsThePlayerHomeDoor`,
  `NewGameEntry_DiffersFromTheWhiteoutDestination` (build 40/`"Door"` vs the
  whiteout's build 2/`"TownCenter"` -- the clause that keeps the two flows from
  silently re-merging), `NewGameEntry_FrontEndRowMirrorsTheNewGameConstants`, and
  `NewGameEntry_PlayerHomeConnectsBackToDawnmere`. **Nothing here re-spells 40 or
  `"Door"`**: every clause reads the manager's constants and reconciles them
  against the compiled world table, so a unit cannot compare a literal against
  itself.
  **★ THEIR BOUNDARY:** running before the initial scene loads, they cannot prove
  the destination is REGISTERED (an unregistered index passes
  `IsWarpDestinationValid`, is accepted, then parks forever in
  `WAITING_FOR_SPAWN`) and cannot prove a real Enter on the title goes there.
  That is `ZM_SaveContinue_Test`'s `AwaitPlayerHome` phase. **Their greenness is
  not "New Game works".**
- **T0 `ZM_WorldTraversal` units (SHIPPED -- exactly 3)** in
  `Tests/ZM_Tests_PlayerHomeInterior.cpp`, mirroring the ProfLab file:
  `PlayerHome_TintIsDistinctFromTheBlockoutGrey` (COMPUTES the separation between
  the two shipped colours with the shipped measuring function against the shipped
  `fZM_HUMAN_PALETTE_MIN_SEPARATION` = 0.15 margin -- it does not restate the
  expected pairing, which could not red a drift),
  `PlayerHome_BlockNamesAreTheTintedInventory` (the name predicate selects exactly
  the seven shell blocks), and `PlayerHome_BlockoutGeometryIsTheAuthoredRoom`.
  **★ THEIR BOUNDARY, stated by the file:** they prove the tint CONSTANT is warm,
  slight and far from the grey. They cannot prove it reached a material (that is
  `ZM_InteriorTint_Test`) and certainly cannot prove it reached a pixel (that is
  `ZM_InteriorTintPixels_Test`). **Their greenness is not "the bedroom is
  yellow".**
- **P1 `ZM_InteriorTint_Test` (SHIPPED, headless-safe)** in
  `Tests/ZM_AutoTests_InteriorTint.cpp`: `m_bRequiresGraphics = false`
  (**deliberately**), max **600** frames, fixed dt 1/60, with a `Teardown` that
  returns to FrontEnd.
  **THE ONLY CI-VISIBLE PROOF THAT THE TINT REACHED ANYTHING**, and headless
  deliberately: a skip counts as a PASS in `zm-tests`, so a graphics-required tint
  test would be silent exactly where it is needed. It keys on the MATERIAL NAME
  (`"ZM_Greybox"`) because `ZM_GreyboxVisual` is file-local to `Zenithmon.cpp` and
  unnameable from a test TU -- the same idiom `ZM_RivalVesperAuthored_Test`
  already runs headlessly, which is the evidence that `OnStart` really does build
  its model and material on Null. Both build indices and both expected colours are
  read from the compiled table and the shipped accessors; neither is re-spelled.
  Three arms that make each other non-vacuous:
  - **ARM 1** -- every one of PlayerHome's seven blockout materials carries the
    tint (tolerance 1e-4: a compiled colour reaching a runtime material is a
    float-noise claim).
  - **ARM 2** -- every one of ProfLab's seven is EXACTLY the shipped blockout
    grey, byte for byte. Exact equality deliberately: the ruling was that the lab
    is left exactly as it is, which is a claim about bytes that did not move at
    all, not bytes that moved a little.
  - **ARM 3** -- the separation **between the two SAMPLES**, not between the two
    constants. A clause measuring the constants would be the boot unit again, one
    layer further from the truth; this one measures what the two scenes actually
    put on their walls.

    Delete the tint and arms 1+3 red while 2 stays green; paint everything and
    arms 2+3 red while 1 stays green. **Neither mistake can pass.**
  - **The anti-vacuity clause that licenses its `RequestSkip`:** each room's scan
    must observe EXACTLY its seven blocks with zero overflow, and a truncated or
    empty scan REDS. Without it, "every sampled material carries the tint" is
    satisfied by sampling nothing. The failure text says so, and says that a zero
    count on Null must be booked as a coverage boundary with an explicit skip --
    **never left passing on an empty scan.**
- **P1 `ZM_InteriorTintPixels_Test` (SHIPPED, windowed)** in
  `Tests/ZM_AutoTests_PlayerHomeTintPixels.cpp`: `m_bRequiresGraphics = true`,
  max **1,100** frames, fixed dt 1/60. **The only test in the game that reads a
  PlayerHome pixel**, and honestly NOT the primary gate: the material ->
  framebuffer path is shared with every other greybox body and already pinned by
  `ZM_NpcRenderedPalette_Test` / `ZM_ShellLighting_Test`, and being
  graphics-required it SKIPS-as-passes in `zm-tests`. **It must be RUN WINDOWED
  and its numbers RECORDED BY HAND** (section 3's stdout hazard), or it has told
  nobody anything. It dumps one swapchain TGA per room to
  `Build/artifacts/zenithmon/visual_audit/`, touches NO graphics option
  (auto-exposure runs as shipped, captured only after 120 frames of adaptation),
  and derives every sample point and camera pose from the two placement headers as
  PROPORTIONS, so the 16x12x3.0 m bedroom and the 20x16x3.5 m lab are framed
  identically and the two measurements are comparable by construction.
  - **The statistic is the floor patch's RED / BLUE ratio** -- a chromaticity
    ratio, not a luminance, because exposure is a scalar that largely cancels, so
    two rooms that each adapted to their own auto-exposure remain comparable.
  - **ONE asserted property: the SIGNED separation** (PlayerHome MINUS ProfLab
    >= **0.15**). A lab that rendered warmer than the bedroom goes negative and
    fails. Calibrated on the first windowed run (2026-08-01): **1.3045** and
    **1.0742**, an observed gap of **0.2303**. Those three figures were measured
    OUT OF BAND off the two TGAs by averaging a floor band, not by the 9x9 patch
    the code reads, so they corroborate the RELATIVE property and are NOT a
    baseline for this probe's own OBSERVED line -- prefer the OBSERVED line.
  - **0.15 is neither weakened nor tightened.** It already clears the observed gap
    with ~1.5x headroom, and raising it to hug 0.2303 would make the clause a
    tripwire for ordinary lighting drift, redding for a reason unrelated to the
    tint.
  - **A capture-sanity band (red/blue in [0.70, 3.00]) applied to BOTH rooms
    IDENTICALLY, and it is NOT evidence of the tint.** It closes one real hole in
    a difference-only check: if a framing regression put one room's patch on open
    sky (strongly blue) the gap could widen and pass FALSELY. If a lighting
    re-tune moves a room outside this band, WIDEN THE BAND -- these bounds track
    lighting; the margin above does not.
  - **★ ZM-D-177 RETRACTED THIS TEST'S ABSOLUTE BOUNDS AS A FALSE PREMISE.** Full
    reasoning is recorded as the fifth pixel-test convention at the end of 5.7.
    Short form: the retracted pair asserted "grey below 1.0, tint above 1.0",
    which assumed albedo ordering survives to the framebuffer. It does not -- both
    rooms measured ABOVE 1.0 -- so the bound would have failed on an untinted
    room. **Do not add an absolute ratio back.**

- **`ZM_Slice_Test` (NOT YET WRITTEN):** mini-playthrough new game -> Badge 1
  (CB_HumanSession-style flat action script + probe snapshots, ~4-6k frames,
  windowed). Runs at the gate via `--filter`; joins the batch only if it fits
  the budget (section 6). It is `Roadmap.md`'s fourth S8 item and cannot be
  written until the three content items above it land.

### 5.9 S9/S10 -- world buildout

- **Per-region traversal tests:** for each region wave, an automated test
  walks EVERY warp edge in `ZM_WorldSpec` (doors, route mouths, gates) and
  asserts arrival scene + spawn tag; one scripted battle per route validates
  its encounter table end-to-end. WorldSpec integrity units (5.1) stay the
  first line; traversal catches what only runtime can.
- **Bake determinism re-run:** re-running the tools boot produces zero diffs
  against the existing bake (hash comparison) -- the generator-determinism
  invariant at world scale.
- S10 adds the remaining regions + an automated Elite-4 gauntlet with an
  overleveled scripted team.

### 5.10 S11 -- post-game

- Units (`ZM_Tower`): headless 100-streak simulation invariants (rental
  legality, opponent scaling monotonicity, boss cadence, streak accounting).
- P1: automated 7-battle tower run through the real UI.

### 5.11 S12 -- segments + full playthrough

- **Segment tests** (`ZM_Segment_<Chapter>_Test`): per-chapter scripted
  playthroughs sized to stay inside the batch budget; together they cover the
  full critical path.
- **`ZM_Playthrough_Test`:** full new-game -> Champion bot
  (CB_HumanSession pattern + `zm_instant_battles` DebugVariable to skip
  presentation timing). **`m_bManualOnly`** -- run explicitly at the S12 gate,
  never in the batch or CI.
- Perf pass: suite runtime vs budget, slowest-10 audit, save-migration audit.

### 5.9 Input program WP3b -- bindings, profiles and the on-screen controls (SHIPPED)

The engine input program's Zenithmon pilot. `Source/ZM_InputActions.h` is deleted;
every production reader goes through `Source/ZM_Bindings.h` and the engine action
layer, and `ZM_TouchLayoutController` (ECS order 114) retargets the four B9
on-screen controls to whatever the current UI state needs.

- **T0 `ZM_Bindings` (10) + `ZM_TouchLayout` (3)**, in `Tests/ZM_Tests_Bindings.cpp`,
  all driving a LOCAL `Zenith_InputActions` through the real frame contract via
  `Tests/ZM_BindingsTestRig.h` (see C1b for why a boot unit cannot use the
  engine's):

  | Test | Contract covered |
  |---|---|
  | `ZM_Bindings::ProfilesReplaceTheEngineDefaultsWithOneSchemeEach` | The first game `RegisterProfile` clears the engine defaults; P_KEYBOARD / P_TOUCH / P_GAMEPAD own exactly one scheme each and are pairwise disjoint; MOUSE owns none |
  | `ZM_Bindings::EveryActionIsRegisteredWithItsContractIdNameAndKind` | All eight ids, names and value kinds; name -> id resolution (what a widget's `SetAction` and a graph node use); every id above the engine-reserved line |
  | `ZM_Bindings::BindingTableMatchesTheC2ContractIncludingThePadColumn` | Every row of C2 including the PAD column: key sets, pad buttons, the left-stick row's `-1` y inversion, the d-pad composite, INTERACT and CONFIRM on DIFFERENT pad faces, and CANCEL as the sole owner of the mask-exempt SYSTEM_BACK row |
  | `ZM_Bindings::VirtualSourceIdsArePairwiseDistinct` | Six on-screen-reachable actions, six DISTINCT virtual sources -- the property that keeps the retargeting A button from holding INTERACT and CONFIRM at once |
  | `ZM_Bindings::MoveCompositeKeepsTheLegacyForwardCancelAndDiagonalRules` | +y FORWARD, arrows alias WASD, opposite keys cancel, diagonals UNNORMALISED (the pre-migration feel of walking) |
  | `ZM_Bindings::ConfirmCancelAndMenuFireExactlyOneEdgePerPress` | One edge per press for all three; a held key does not repeat; the ALTERNATE bound key rising beside the first fires nothing; both released == one release |
  | `ZM_Bindings::RunIsAHeldReadAndMenuVerticalIsAPerPressStep` | RUN is a level that survives swapping shifts; the battle cursor is per-press and an up+down in one frame cancels to zero |
  | `ZM_Bindings::TouchProfileMasksOutKeyboardRowsAndEnablesVirtualOnes` | Under P_TOUCH a keyboard row is dead and the stick's virtual axis reaches MOVE; a same-frame virtual tap fires BOTH edges and does NOT light the action that shares the physical button in another context |
  | `ZM_Bindings::SystemBackFiresCancelUnderEveryProfile` | Mask-exempt under all three profiles; PULSES (no held phase); does not move the active profile |
  | `ZM_Bindings::SimulatedGamepadDrivesMoveAndConfirmEndToEnd` | The sim-pad smoke: pad activity WINS the auto switch into P_GAMEPAD, the stick drives MOVE with the y inversion applied, A confirms without firing INTERACT, and the d-pad agrees with the keyboard composite's convention |
  | `ZM_TouchLayout::ContextResolutionRanksBattleThenDialogueThenTitle` | BATTLE outranks everything (it owns the screen through both fades); a raised DIALOGUE outranks the menu it stacked on; TITLE is identified by the top screen; a closed stack is the overworld whatever stale id came with it |
  | `ZM_TouchLayout::EachContextGivesOneSemanticsPerButton` | The B11 table verbatim, the shared DIALOGUE/MENU/BATTLE semantics, TITLE's deliberate absence of a cancel, the out-of-range fold to OVERWORLD, and distinct context names |
  | `ZM_TouchLayout::EveryLayoutTargetIsARegisteredActionWithAVirtualRow` | The seam neither side can prove alone: every context's four targets resolve to a registered action that carries exactly one VIRTUAL row of the right value kind. A layout naming an action without one produces a control that claims the finger and publishes nothing, silently |

- **P1 `Tests/ZM_AutoTests_TouchControls.cpp` (6, all `m_bRequiresGraphics = false`
  so CI actually sees them).** Each drives the REAL widgets on the persistent HUD
  with real pointer events and asserts on what the GAME did:
  `ZM_TouchStick_Test` (a diagonal tilt moves the player on both world axes, and
  lifting stops him), `ZM_TouchInteract_Test` (overworld A presses INTERACT, does
  NOT press CONFIRM, and the edge reaches `ZM_InteractionRuntime`'s latch),
  `ZM_TouchDialogueConfirm_Test` (the SAME button retargets to Confirm, the stick
  leaves the screen, and the CONFIRM reaches the box), `ZM_TouchMultiTouch_Test`
  (stick and button in ONE frame, and lifting one finger does not disturb the
  other), `ZM_TouchCancelNeutralisesHeld_Test` (a cancelled pointer releases both
  actions and stops the player -- the "phone was unlocked and the character kept
  walking" case), `ZM_TouchSystemBack_Test` (Back fires CANCEL as a pulse while the
  profile STAYS P_TOUCH, with the override deliberately CLEARED first so the
  assertion is not vacuous).
- **The five `ZM_OverworldInput` and five `ZM_Interaction::Keys_*` units MOVED, and
  did not change count.** The input five now close a real frame instead of polling
  the simulator; the collision five walk the LIVE binding table instead of the
  deleted key-set constants -- strictly stronger, since a constant spelled beside
  the table can drift from it and a row read out of it cannot.

### End state

| Bucket | Target |
|---|---|
| T0 unit tests | ~500+ (~370 battle + ~90 data + generators + save + world integrity) |
| P1 automated tests | ~60-100 (traversals, battle smokes, UI flows, segments) |
| Playthrough bots | 1 slice (S8) + 1 full (S12, manual-only) |
| Headless batch runtime | minutes (see budgets) |

---

## 6. Budgets

- **The bar:** DP runs ~140 automated tests in ~2 minutes headless batch.
  Zenithmon's headless batch (all T0 + headless-safe P1) must stay in
  single-digit minutes at end state. Unit tests are microseconds-to-
  milliseconds each; the fuzz soak and traversal tests are the budget items
  to watch.
- **Slowest-10 report reviewed at EVERY stage gate.** A test that balloons
  gets split, seeded down, or demoted to gate-only (`--filter`) -- the batch
  never quietly grows past budget.
- **Long playthroughs are `m_bManualOnly`.** The slice test may join the
  batch if it fits; segment tests are sized to fit; the full playthrough bot
  never does.
- Windowed + asset-dependent tests are a bounded set run locally at stage
  gates -- CI time is spent only on the headless backbone.
- Frame caps: give every P1 test an explicit `m_iMaxFrames` with margin, and
  keep single-mechanic tests <= 900 frames (15 s at 60 Hz) unless the scenario
  genuinely needs more.

---

## 7. Out of scope for this suite

- **Visual fidelity** -- per-stage manual visual checks (Roadmap.md gates) own
  "does it look right"; the suite asserts renderer inputs and the single S5
  bleed-through capture.
- **Audio** -- the engine has none ([Scope.md](Scope.md)).
- **Networking/multiplayer/trading** -- not in the game ([Scope.md](Scope.md)).
- **Fun/balance judgement** -- S11 uses headless AI-vs-AI simulation STATS to
  inform balance, but no automated test claims to assess fun.
- **Nintendo-content comparison** -- Zenithmon ships original species/names;
  tests assert OUR data tables, never external ones.

## 8. The camera-clearance guards (ZM-D-173) and what they do NOT cover

`Tests/ZM_AutoTests_CameraClearance.cpp` adds **two permanent P1 registrations**,
taking the automated registry **51 -> 53** at ZM-D-173. (**That is a delta, not
the current total: the registry is 56 as of ZM-D-177** -- S8's
`ZM_ProfLabWarp_Test`, `ZM_InteriorTint_Test` and `ZM_InteriorTintPixels_Test`
took it 53 -> 56, see 5.8. Count registrations from
`grep -c ZENITH_AUTOMATED_TEST_REGISTER Tests/*.cpp`, never from a prose total.)
Both load the COMMITTED Dawnmere
against the REAL baked terrain, both run on the **Null** backend
(`m_bRequiresGraphics = false`), and neither creates, moves or teleports anything.

| Test | What it asserts | Skips only when |
|---|---|---|
| `ZM_DawnmereHomeGroundTruth_Test` | The MEASUREMENT ORACLE. Casts a real downward ray at each of the ten Home placement columns and reds if a compiled row in `Source/World/ZM_DawnmerePlacement.h` has drifted from the surface the world actually has (same 0.15 m tolerance as the W5 NPC oracle). Logs every measured value at INFO on every run -- this is how those constants are re-obtained after a terrain recipe change **or a collision-density change** (ZM-D-182: the samples are real raycasts against the COLLISION mesh, so a physics-divisor change moves them just as a recipe change does). | The Dawnmere scene or the terrain bake is genuinely absent |
| `ZM_DawnmereCameraClearance_Test` | The CONTRACT GUARD. At every authoritative sample it runs the SHIPPED camera maths (`ComputeDesiredPosition` + `ClampArmDistance`) against the SHIPPED physics world and requires the clamped arm to keep **>= 50% of the authored 6.0008 m** pivot->camera distance. Also asserts the scene's captured yaw is still the authored 0, so a scene yaw edit cannot silently invalidate every sample direction. | as above |

**The two probes are filtered differently on purpose.** The oracle ignores the
Home SHELL at every column, because the shell is the thing the table POSITIONS
and the committed scene still carries its previous placement until the
re-authoring boot. The clearance guard ignores only the PLAYER, so a route or
approach that ends up under the building fails there, by name. Asking one probe
to do both jobs is what produced a 900-frame timeout blaming terrain streaming
for a column that was simply under a wall.

### ★ The enforceable boundary, stated so it cannot be oversold

The clearance guard covers a **named sample table -- 308 samples as of
ZM-D-173**:

- the town-centre -> door-staging drive, every 1.0 m (130);
- the staging -> door-trigger approach, every 0.25 m including both endpoints (17);
- both segments of the Home dirt path `(512,512) -> (454,486) -> (384,456)`, every 1.0 m (143);
- a 1.5 m ring plus centre around `FromHomeSpawn` (9) and around TownCenter (9).

**This is NOT a proof that every mathematically standable point in Dawnmere
satisfies the contract.** It is deterministic, actor-free coverage of the
critical movement areas and the interaction approaches.

It carries **no rings around NPCs, deliberately**: a live NPC can legitimately
occupy the camera ray, which would make a static-layout guard nondeterministic.

**A newly authored region must add ITS primary traversal paths, warp approaches
and actor-free interaction approaches to that table as part of authoring it.**
A region added without extending the table is unguarded, and the suite will not
say so.
