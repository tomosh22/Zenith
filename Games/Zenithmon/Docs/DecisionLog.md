# Zenithmon -- Decision Log

**Purpose:** Append-only record of every non-trivial decision made during
Zenithmon development. Future agents grep this when investigating "why was X
done this way?". Scope changes MUST land here as a user decision before any
implementation (see [Scope.md](Scope.md) Section 4).

**Format:** One entry per decision, **newest first**. Fields per entry:
date, id, decision, why, tests-that-lock-it, reversibility. Ids are `ZM-D-NNN`,
assigned in chronological order (so the highest number is at the top of the file).

**What counts as non-trivial:** anything involving trade-offs, anything another
system depends on, any engine-side change, any scope or convention ruling.
Tuning-value changes go in git history, not here.

---

## 2026-07-10 -- ZM-D-016 -- Master branch protection CREATED with `zm-tests` as the sole machine-enforced required check

- **Decision:** master had NO branch protection and no rulesets at all (the
  repo's "required checks" had been purely conventional). On the user's
  direction ("Add zm-tests yourself"), classic branch protection was created
  via the API: required status checks `[zm-tests]`, `strict=false`,
  `enforce_admins=false`, no required reviews.
- **Why:** the S0 gate requires zm-tests to actually block merges;
  `enforce_admins=false` preserves the owner's established direct-push
  workflow (agents always land via PRs, so agents are always gated). Other
  gates stay blocking-by-discipline because several are path-filtered and a
  required check that never reports deadlocks a PR.
- **Tests that lock it:** none (GitHub configuration); verified by
  `gh api repos/tomosh22/Zenith/branches/master/protection`.
- **Reversibility:** trivial (delete/edit the protection rule); recorded in
  CIPolicy.md section 4 + ManualSetupChecklist.md.

## 2026-07-10 -- ZM-D-015 -- Three pre-existing master-red CI gates fixed as a prerequisite PR rather than inherited red

- **Decision:** engine-gate, layering-gate, and scaffold-smoke had been red on
  master since 2026-07-07/08 (before Zenithmon existed). Rather than merging
  S0 with inherited red checks, they were fixed in a dedicated PR (#144,
  `0844689e`): unit baseline 1053->1068 single-sourced in
  `Tools/run_unit_gate.ps1` (test_scaffold.ps1 reuses it), `Flux_HDR.cpp`
  g_xEngine reaches reduced via the established local-hoist idiom (fixed, not
  allow-listed), and regen.ps1 given a dotnet-exec fallback on the tracked
  Sharpmake dll (+ scaffold-smoke got `lfs: true` and the standard
  `/p:WindowsTargetPlatformVersion=10.0` build override).
- **Why:** "nothing merges red" is only meaningful if master itself can go
  green; every future Zenithmon stage PR needs a green baseline.
- **Tests that lock it:** the gates themselves (all 9 checks green on #144;
  all 10 green on the rebased #143); scaffold smoke 11/0 locally.
- **Reversibility:** each fix is independent and small; the baseline bump is
  a ratchet (future engine-test additions bump it again in ONE place).

## 2026-07-09 -- ZM-D-014 -- Engine name-validation narrowed to a PascalCase word boundary so 'Zenithmon' is a legal game name

- **Decision:** `zenith new Zenithmon` was rejected by the blanket
  `Zenith*`/`Sentinel*` reserved-prefix rule in BOTH game-name validators: PS
  `Test-ZenithGameNameSyntax` in `Build/zenith_buildsystem.psm1` and C++
  `ZenithHub_GameScan::ValidateName`. Both were narrowed to a PascalCase word
  boundary: reject `Zenith`/`Sentinel` alone or followed by an uppercase letter
  or digit; a lowercase continuation (e.g. `Zenithmon`) is a distinct word and
  valid.
- **Why:** the reservation exists to protect engine/test module names
  (`ZenithECS`, `SentinelAI`, ...); `Zenithmon` collides with none of them --
  the blanket rule was broader than its intent.
- **Tests that lock it:** `Build/Tests/run_buildsystem_tests.ps1` (suite 45
  passed / 0 failed) + the shared pinned vectors in
  `Tools/ZenithCli/Tests/name_validation_cases.txt` (consumed by both
  validators) + the ZenithHub selftest.
- **Reversibility:** reverting the validators would orphan this project --
  reversible only by renaming the game.

## 2026-07-09 -- ZM-D-013 -- No per-game runner script; the unified `zenith test` harness is the only test runner

- **Decision:** Zenithmon never gets a `run_zm_tests.ps1`. All test execution --
  local, stage gates, and CI (`.github/workflows/zm-tests.yml`) -- goes through
  `zenith test Zenithmon` (`Tools/ZenithCli/ZenithCli.psm1` ->
  `ZenithTestHarness.psm1`; flags `--filter/--headless/--results-dir/--config/
  --per-process/--fail-fast`; exit codes 0 OK / 1 usage / 2 validation /
  3 generation / 4 build-or-test / 5 not-found).
- **Why:** the old per-game `Tools/run_*_tests.ps1` scripts were DELETED at
  commit `c29e28f8` in favor of the unified harness; a per-game script would be
  legacy surface on day one (repo mandate: no legacy/compat code).
- **Tests that lock it:** the `zm-tests` CI workflow invokes
  `zenith.bat test Zenithmon --headless` directly; every stage gate in
  [Roadmap.md](Roadmap.md) cites the same command.
- **Reversibility:** none needed -- a per-game script would be a policy
  violation, not an option.

## 2026-07-09 -- ZM-D-012 -- Scene build index 0 = FrontEnd.zscen, boot-authored title screen (DP convention)

- **Decision:** the game boots into `FrontEnd.zscen` at build index 0: camera +
  "Zenithmon" title text + the game component. The scene is boot-authored by
  tools builds (editor-automation steps re-author it every tools boot) and the
  baked `.zscen` is git-ignored. The build-index table follows the plan:
  0 FrontEnd, 1 Battle, 2-12 towns, 20-34 routes + Victory Road, 40+ interiors,
  95 Tower (exact per-scene assignments TBD at S9/S10 via ZM_WorldSpec).
- **Why:** matches the proven DevilsPlayground convention (index 0 = FrontEnd,
  boot-authored, reloaded between batched tests by the harness).
- **Tests that lock it:** `Tests/ZM_AutoTests_Boot.cpp` (`ZM_Boot_Test`) + the 2
  boot unit tests in `Tests/ZM_Tests_Boot.cpp`.
- **Reversibility:** index remaps are cheap until S3, when warps start
  referencing build indices through ZM_WorldSpec.

## 2026-07-09 -- ZM-D-011 -- S0 keeps the scaffold placeholder ZM_GameComponent (bobbing cube)

- **Decision:** `ZM_GameComponent` (registered `"ZM_Game"`, serialization
  order 100) retains the `zenith new` scaffold's bobbing-cube behaviour as the
  S0 placeholder until the S1 data core and S3 overworld systems land.
- **Why:** S0 is skeleton/harness/CI/docs only; a live registered component
  proves the registration + serialization + between-tests plumbing without
  inventing gameplay ahead of its stage.
- **Tests that lock it:** `Tests/ZM_Tests_Boot.cpp` (2 unit tests) +
  `ZM_Boot_Test` -- these pin boot health, not the cube; the placeholder is
  free to be replaced.
- **Reversibility:** trivial -- it is a placeholder by design.

## 2026-07-08 -- ZM-D-010 -- Battle engine is headless C++ (not Behaviour Graphs) with an append-only event stream

- **Decision:** the battle turn loop is a seeded, deterministic C++ state
  machine (`ZM_BattleEngine`): `Begin(config,seed)` -> `SubmitAction` ->
  `ResolveTurn()` -> append-only `ZM_BattleEvent` stream, the single source of
  truth for both tests and presentation; the engine never formats strings or
  touches UI. Behaviour graphs are glue only (menu flow, NPC events, cutscene
  beats). Origin: the approved plan, `zenithmon-pok-mon-nested-puddle.md`.
- **Why:** rule-based logic needs exact-replay determinism and full headless
  unit-test coverage; no in-repo turn-based graph reference exists.
- **Tests that lock it:** S2 gate (~370 unit tests incl. scripted seeded
  battles with exact expected event streams + 2,000-battle fuzz soak).
- **Reversibility:** low -- reversing means rewriting S2; do not revisit.

## 2026-07-08 -- ZM-D-009 -- Game data = compiled const C-array tables, not disk assets

- **Decision:** species/moves/items/abilities/natures/type chart/encounters/
  trainers/dex text live as `const` C arrays in `Source/Data/*.cpp`; the
  "assets baked to disk" mandate covers meshes/textures/anims only. Origin: the
  approved plan.
- **Why:** compile-time validated, diffable in review, zero file I/O in
  headless CI tests.
- **Tests that lock it:** the `ZM_Tests_Data` validation suite +
  `ZM_DataRegistry` integrity tests (S1 gate).
- **Reversibility:** mechanical to move tables to files later, but it would
  sacrifice the zero-I/O headless-CI property -- user decision required.

## 2026-07-08 -- ZM-D-008 -- Battle format: singles only

- **Decision:** all battles are 1v1 singles; doubles is an explicit scope cut
  (see [Scope.md](Scope.md)). Struct layout does not preclude doubles later.
  Origin: the approved plan.
- **Why:** doubles is roughly 2x targeting/AI/UI complexity for marginal value.
- **Tests that lock it:** the entire S2 battle suite assumes single active
  monster per side.
- **Reversibility:** additive later behind a new scope decision; nothing to
  unwind now.

## 2026-07-08 -- ZM-D-007 -- Overworld-to-battle = ADDITIVE battle scene at world offset (0, -2000, 0) with overworld pause

- **Decision:** encounters load the battle scene ADDITIVE at (0, -2000, 0),
  `SetScenePaused(overworld, true)`, switch camera/HUD, and `UnloadScene` on
  exit; one battle scene with ~6 swappable biome dressing sets, enclosed by a
  backdrop dome. Documented fallback if visual isolation fails: SINGLE load +
  world-state snapshot. Origin: the approved plan.
- **Why:** a SINGLE reload resets render systems + physics and re-streams
  terrain -- seconds of hitch at wild-encounter frequency.
- **Tests that lock it:** S5 gate windowed round-trip tests (exact overworld
  resume) + screenshot check for overworld bleed-through at the offset.
- **Reversibility:** medium -- the fallback path is designed and documented;
  switching costs the S5 transition work only.

## 2026-07-08 -- ZM-D-006 -- Door/route-edge transitions = SINGLE loads with spawn tags; player/camera are NOT persistent entities

- **Decision:** `ZM_WarpTrigger_Component {targetBuildIndex, spawnTag}` -> fade
  -> SINGLE load; the persistent `ZM_GameStateManager` (`DontDestroyOnLoad`)
  respawns player + follow camera at the tagged `ZM_SpawnPoint`. One-time
  placement at load is not gameplay teleportation. Origin: the approved plan.
- **Why:** SINGLE loads reset physics and would orphan a persistent Jolt body;
  respawn-at-tag is the safe pattern.
- **Tests that lock it:** S3 gate windowed door/warp round-trip test; later
  per-region traversal tests walk every warp edge (S9/S10).
- **Reversibility:** medium -- changing to persistent player entities requires
  engine work on physics-across-SINGLE-loads first.

## 2026-07-08 -- ZM-D-005 -- ZM_WorldSpec is the keystone world table

- **Decision:** one declarative compiled table describes the whole world --
  scenes (name, build index, kind, terrain set, encounter table + rate),
  connections/spawn tags, trainers, shops, gyms, story beats. Tools walk it to
  author terrains/scenes/graphs; runtime walks it for warps/encounters/gating.
  Origin: the approved plan.
- **Why:** ~40 scenes without 40 bespoke authoring functions; one source of
  truth keeps authoring, runtime, and tests in agreement.
- **Tests that lock it:** WorldSpec referential-integrity unit tests (every
  warp target, spawn tag, species, and trainer resolves) run on every PR.
- **Reversibility:** low -- everything from S3 on flows through it; treat as
  load-bearing.

## 2026-07-08 -- ZM-D-004 -- Tall grass samples a game-owned CPU copy of the density map

- **Decision:** `ZM_TallGrassSystem` loads the baked `GrassDensity.ztxtr` per
  outdoor scene, feeds `g_xEngine.Grass().SetDensityMap(...)` for rendering,
  and keeps its OWN CPU copy for gameplay sampling; player XZ quantized to 1 m
  tiles, encounter roll on tile transition where density >= 0.5; density map
  cleared on interiors/battle. Origin: the approved plan.
- **Why:** engine `SampleDensityMap` returns 1.0 when no map is set -- gameplay
  must not inherit that trap, and the grass singleton is render-owned state.
- **Tests that lock it:** S5 gate encounter tests (walk grass until encounter
  with rigged RNG); grass-state assertions at S9/S10 gates.
- **Reversibility:** low cost -- swapping to engine-side sampling is a small,
  local change if the engine semantics ever harden.

## 2026-07-08 -- ZM-D-003 -- Baked assets are git-ignored, regenerated under per-family manifest guards

- **Decision:** everything under `Games/Zenithmon/Assets/` is git-ignored (repo
  norm) and regenerated by tools builds; per-family manifest guards =
  generator-version stamp + file-existence (hardened RenderTest pattern).
  Consequence: a fresh CI checkout has NO assets, so every asset/scene-dependent
  automated test must exists-guard and `RequestSkip` (the CI-fix pattern from
  commit `94813489`). Origin: the approved plan.
- **Why:** ~30-50 min cold bake output does not belong in git; determinism makes
  the repo the recipe, not the artifact.
- **Tests that lock it:** bake-determinism gate (re-run tools boot -> zero
  diffs, byte-identical re-bake) + the CI headless suite passing on an
  assets-absent runner.
- **Reversibility:** policy-level; committing baked assets would need a repo-
  wide user decision.

## 2026-07-08 -- ZM-D-002 -- Engine changes scoped to E1-E5, all additive and back-compatible

- **Decision:** the only engine-level changes this project makes are: E1
  per-component serialized terrain-set name (replaces 6 hard-coded `Terrain/`
  path sites); E2 `AddStep_TerrainExportChunksRect` + streaming-path
  missing-chunk tolerance check; E3 `Zenith_UIText` typewriter reveal; E4
  `Zenith_UIGridLayoutGroup`; E5 grass singleton reset hygiene (wire
  `Grass().Reset()` into `ResetRenderSystems` + clear instances/flags/density
  map). Each lands with unit tests + a RenderTest boot regression check.
  Origin: the approved plan.
- **Why:** verified engine gaps (one-terrain-per-game, full-grid bake volume,
  no typewriter/grid widgets, grass state leaking across SINGLE loads) --
  scoping them up front prevents ad-hoc engine sprawl.
- **Tests that lock it:** per-change unit tests + RenderTest still boots green
  (default-path untouched) + DP/CB suites stay green.
- **Reversibility:** per-change -- each is additive with a legacy-default path,
  so individually revertable before Zenithmon content depends on it.

## 2026-07-03 -- ZM-D-001 -- Scope lock (user decisions)

- **Decision:** the in/out scope for Zenithmon is locked as recorded in
  [Scope.md](Scope.md): ~150-species dex / 18 types / 3-stage lines / rarity;
  classic 8-gym world with no Wild Area; the full battle core; extras =
  abilities, natures, IVs/EVs, weather + terrain effects, breeding; post-game =
  Champion rematch + Battle Tower. Out: audio, networking/multiplayer/trading,
  Dynamax-analog, doubles, Substitute/Encore/Transform/weight moves, open Wild
  Area.
- **Why:** product of multiple prior iteration rounds with the user; frozen to
  prevent scope creep across a ~13-stage build.
- **Tests that lock it:** [Scope.md](Scope.md) is the binding gate; stage gates
  audit shipped content against it.
- **Reversibility:** user decision only, recorded as a new entry in this log
  (Scope.md change-control rule).
