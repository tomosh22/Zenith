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

1. **Deterministic.** Fixed dt 1/60 for anything input- or time-driven; every
   RNG consumer takes an explicit seed (see convention C8). Same build + same
   seed = same result, every run.
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
`ZM_Tower` (S2), `ZM_Gen` (S4), `ZM_Grass` / `ZM_Encounter` (S5), `ZM_UI`
(S6), `ZM_Save` (S7). New systems get a new category, never a grab-bag.

---

## 2. Harness conventions (all mandatory)

Each rule with its one-line why. Violations are review blockers.

- **C1 -- Step uses ONLY `Zenith_InputSimulator` state-setters**
  (`SimulateMousePosition` / `SimulateMouseButtonDown` / `SimulateMouseButtonUp`
  / `SimulateKeyPress` / `SetKeyHeld` / `SimulateMouseWheel`).
  *Why:* Step runs inside the main loop; anything that re-enters the loop
  (`StepFrame`, `SimulateMouseClick`) deadlocks in `vkWaitForFences`.
- **C2 -- Fixed dt 1/60 for input-driven tests.**
  *Why:* frame counts convert to seconds deterministically, so timing asserts
  are exact instead of flaky.
- **C3 -- Every ownerless global gets a reset in the between-tests hook**
  (registered in `Zenithmon.cpp` via
  `Zenith_AutomatedTestRunner::RegisterBetweenTestsHook`; currently
  `Zenith_SaveData::ClearForTest` -- the hook grows in the SAME PR as each new
  global system lands).
  *Why:* batched tests share one process; state that no entity owns leaks
  silently into the next test.
- **C4 -- Rely on the scene-0 force-reload between batched tests** for
  entity-owned state; clean up via OnDestroy, not via the hook.
  *Why:* the harness reloads scene 0 between tests, so entity-owned state has
  a guaranteed teardown path already.
- **C5 -- Set `m_bRequiresGraphics = true` on any test that touches Flux**
  (rendered output, grass instances, screenshot probes).
  *Why:* headless runs skip Flux entirely; such tests auto-skip instead of
  crashing on the GPU-less CI runner.
- **C6 -- Asset- or scene-dependent tests exists-guard their inputs and call
  `RequestSkip(szReason)` when absent.**
  *Why:* baked assets are git-ignored, so a fresh CI checkout has NO
  `Assets/` -- a hard dependency would fail every PR (the engine-wide CI-fix
  pattern from commit `94813489`).
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

The unified harness -- there is NO per-game runner script (the old
`Tools/run_*_tests.ps1` scripts were deleted engine-wide at `c29e28f8`):

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
| `--headless` | No window, Flux skipped; `m_bRequiresGraphics` tests auto-skip |
| `--results-dir <dir>` | Per-test JSON output location |
| `--config <cfg>` | Build configuration override (default `Vulkan_vs2022_Debug_Win64_True`) |
| `--per-process` | One process per test -- slower, bullet-proof against state leaks |
| `--fail-fast` | Abort the batch on first failure |

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
fallback when chasing a suspected cross-test leak.

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
`D3D12_vs2022_Debug_Win64_False` (backend-neutrality link proof) -> DLL copies
-> headless boot check -> **boot unit-test gate** (`Tools/run_unit_gate.ps1` --
the ONLY CI step that runs the ZENITH_TEST T0 suite, since `zenith test` and the
boot check both pass `--skip-unit-tests`) -> `zenith.bat test Zenithmon
--headless --results-dir Build/artifacts/test_results/zenithmon` (the P1
automated suite) -> upload artifact `zm-test-results` (`if: always()`).

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

### 5.0 S0 -- skeleton (SHIPPED)

- T0: `ZM_Boot` x2 (`ProjectNameIsZenithmon`, `GameAssetsDirectoryIsNonEmpty`)
  in `Tests/ZM_Tests_Boot.cpp`.
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

  All seven are engine-side `ZENITH_TEST` cases and are count-ratcheted into
  both the shared engine unit gate (**1078** registered) and Zenithmon's CI
  boot unit gate (**1728** registered). The latter expects 1727 passed,
  0 failed and the one quarantined skip.
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
  cases raise the shared engine baseline 1075 -> 1078 and Zenithmon's exact
  boot baseline 1725 -> 1728.
- **Engine regression:** the empty asset set still resolves the unchanged
  legacy `Terrain/` layout; RenderTest boot and terrain-editor smoke remain
  the local regression oracle.
- **P1 `ZM_VillageWalk_Test` (windowed):** walk Home Village via input
  state-setters, enter the player home, warp round trip back out; asserts
  player position, active scene index, spawn-tag placement.
- Stage-gate manual visual check: terrain/grass/camera.

### 5.4 S4 -- asset generators

Category `ZM_Gen` (headless, in-memory -- no disk dependency, CI-safe):

- **Same-seed determinism:** generating any species/human/building twice from
  the same seed is byte-identical (mesh buffers, texture texels, clip data).
- **Structural validity:** winding (CCW), bounds non-degenerate, skinning
  weights sum to 1 within tolerance, bone counts within caps (<= 30 creature,
  <= 100 engine max), clip channel sets match the archetype skeleton exactly.
- **Shiny differs:** shiny texture differs from base while mesh/skeleton are
  identical.
- **P1 gallery smokes (windowed, asset-dependent -> C6):** batched
  species-gallery scenes showing every species animating; asserts model +
  animator resolve and pose advances. Visual sign-off on a sampled dozen at
  the gate.

### 5.5 S5 -- battle integration slice

- Units: `ZM_Grass` (density-map CPU sampling, tile-transition roll
  gating, clear-on-interior), `ZM_Encounter` (table selection, rate rolls
  with rigged RNG), E5 engine units (grass reset on scene load).
- **P1 encounter round trip (windowed):** walk grass until a rigged encounter
  fires -> additive battle scene loads at the -2000 m offset -> win via
  scripted input -> assert exp applied and EXACT overworld resume (position,
  scene, pause state).
- **P1 catch test:** rigged catch succeeds; monster lands in party; dex
  updates.
- **P1 bleed-through screenshot check:** scripted capture during battle
  asserting the overworld does not render into the battle view (the one
  pixel-adjacent test; `m_bRequiresGraphics`, stage-gate only).

### 5.6 S6 -- UI flows

- Units: `ZM_UI` state-machine tests (menu-stack push/pop, dialogue-box
  paging, grid navigation orders) on the headless-safe widget state.
- P1 automated flows via focus navigation: talk to an NPC, buy/sell at the
  mart, heal at the Care Center, open every top-level menu (party/bag/dex/
  box/options) and back out cleanly.

### 5.7 S7 -- save/load, story flags, trainers

- Units (`ZM_Save`): full round-trip equality, corruption robustness,
  version-mismatch rejection, **canned-blob migrations** (one test per
  historical schema version, blobs compiled into the test TU), sanity-cap
  rejects -- the full obligation list lives in [SaveFormat.md](SaveFormat.md).
  Plus story-flag gating units and trainer sight-cone/defeat-flag units.
- **P1 `ZM_SaveContinue_Test`:** save -> quit to FrontEnd -> continue restores
  position/party/flags exactly.
- P1 trainer battle: sight cone -> forced approach -> dialogue -> battle ->
  defeat flag + prize money.

### 5.8 S8 -- vertical slice

- **`ZM_Slice_Test`:** mini-playthrough new game -> Badge 1
  (CB_HumanSession-style flat action script + probe snapshots, ~4-6k frames,
  windowed). Runs at the gate via `--filter`; joins the batch only if it fits
  the budget (section 6).

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
