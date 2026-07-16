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
`ZM_Encounter` (S5), `ZM_UI` (S6), `ZM_Save` (S7). New systems get a new
category, never a grab-bag.

---

## 2. Harness conventions (all mandatory)

Each rule with its one-line why. Violations are review blockers.

- **C1 -- Step uses ONLY `Zenith_InputSimulator` state-setters**
  (`SimulateMousePosition` / `SimulateMouseButtonDown` / `SimulateMouseButtonUp`
  / `SimulateKeyPress` / `SetKeyHeld` / `SimulateMouseWheel`).
  *Why:* Step runs inside the main loop; anything that re-enters the loop
  (`StepFrame`, `SimulateMouseClick`) deadlocks in `vkWaitForFences`.
- **C2 -- Input-driven tests set an explicit fixed dt (1/60 by default).** A
  different fixed presentation rate must be named and justified by the test;
  `ZM_PlayerHomeRoundTrip_Test` uses 1/30 while normal physics substeps remain
  enabled.
  *Why:* frame counts convert to seconds deterministically, so timing asserts
  are exact instead of flaky; long routes need not trade determinism for an
  excessive presentation-frame budget.
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
  boot unit gate (**1773** registered). The latter expects 1772 passed,
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
  cases historically raised the shared engine baseline 1075 -> 1078 and
  Zenithmon's exact boot baseline 1725 -> 1728; Dawnmere's four game units
  then raised only the Zenithmon baseline to 1732, and the five measurement-
  registry units below raised it to 1737; the 20 overworld input/controller/
  physics/camera/ECS units below raised it by 20; the first 12 traversal units
  raised it to 1769, and the four fade/round-trip units below raise it to the
  current **1773**.
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
- **Input/controller/camera unit tests (SHIPPED -- exactly 20):** all live in
  `Tests/ZM_Tests_Overworld.cpp` and split **5 / 4 / 5 / 4 / 2**:

  | Category (count) | Locked contract |
  |---|---|
  | `ZM_OverworldInput` (5) | WASD and arrow aliases; opposite-axis cancellation; pressed-edge Enter/Space confirm, Escape/Backspace cancel, M/Tab menu; either Shift held for run |
  | `ZM_OverworldController` (4) | Camera-forward flattening + normalized diagonals; 4/7 m/s horizontal-world speed with vertical preservation; walkable-downhill tangent adhesion preserves stronger falls and positive step-assist rises; inclusive 45-degree classification and steeper-uphill blocking; step qualification requires lower obstruction, upper clearance, a walkable landing and rise <=0.40 m |
  | `ZM_OverworldPhysics` (5) | Real dynamic generic capsule grounds/falls/stays upright; walk/run/release drives real velocity; invalid/nonpositive dt is a full observable/animation/body/facing no-op; static wall blocks; Jolt ramp normals classify slopes; low-step query accepts while tall obstacle rejects without reboosting an existing rise |
  | `ZM_OverworldCamera` (4) | Fixed-heading desired pose; omega-8 critical spring has no overshoot; collision padding/minimum-arm clamp; a real occluder pushes inward and recovery moves outward |
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
  | 2 | `PlacementUsesMarkerFeetPlusScaledCapsuleHalfExtentAndZeroesMotion`, `WarpResolutionFreezesThenResetsOnMissingDuplicateAndGenerationChange`: feet-marker centre derivation, one-time teleport, zero linear/angular velocity, reset-enable-idle completion, destination `OnStart` freeze, missing/duplicate marker hold, and entity/scene generation changes without stale-ID acceptance |
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
tools-only). The whole `ZM_Gen` T0 category is now **135** units, eight of which
(the six generator bake smokes plus the two `ZM_BakeManifest` tools-only cases)
are `ZENITH_TOOLS`-only, so `_False`/Android configs register them as empty TUs.

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

`ZM_PropGen` turns each of the 25 prop roster rows into a deterministic STATIC
model -- like buildings, a box composition via `ZM_StaticMesh` with NO skeleton
and NO animation, baked as a 4-file static bundle per prop. Determinism is
golden-pinned (`uZM_PROPGEN_VERSION` = 1). It ships **8** pure/all-config
`ZM_Gen` units in `Tests/ZM_Tests_PropGen.cpp` (SC4), plus 1 `ZENITH_TOOLS`-only
bake smoke (SC5) in `Tests/ZM_Tests_PropBake.cpp`:

- **Roster + purity + static contract** (`Tests/ZM_Tests_PropGen.cpp`, 5 units,
  mirroring BuildingGen's SC1): `PropGen_RosterTotality` (all 25 rows self-index +
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
so `.github/workflows/zm-tests.yml` now runs `run_unit_gate.ps1 -Baseline 1913`
(see section 5.5).

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

- **T0 `ZM_BattleArena` units (SHIPPED -- 5)** in `Tests/ZM_Tests_BattleArena.cpp`,
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

**Remaining S5 items (item 2 SC1 [engine E5 grass reset] + SC4 [windowed integration], items 3-5, planning):**

- SC1 engine E5 units (grass singleton reset clears on scene load); SC4 P1 windowed
  (walk grass -> rigged `ZM_OnWildEncounter` fires; grass cleared on interior via a
  per-scene instance-count assertion).
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
