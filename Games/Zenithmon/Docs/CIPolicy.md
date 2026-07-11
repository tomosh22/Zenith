# Zenithmon -- CI Policy

**Document purpose:** Records the CI gates that protect `master`, what the
Zenithmon gate (`zm-tests`) runs, the branch-protection requirement, the
runner constraints that shape the whole test strategy, and the escalation
path when CI is red.

**Companion docs:** ManualSetupChecklist.md (the manual required-check
registration step), TestPlan.md (which tests run headless vs windowed),
BuildEnvironment.md (the local mirror of the CI toolchain),
AssetManifest.md (why the runner has no assets).

**Status:** LIVING -- update whenever a gate is added/retired or branch
protection changes.

**Last updated:** 2026-07-11 (S2 SC5 -- unit baseline 1379 and
direct-master policy reconciliation).

---

## 1. The Zenithmon gate: `zm-tests`

`.github/workflows/zm-tests.yml` (a clone of the proven `dp-tests.yml`
pattern), active from S0. Required check name: **`zm-tests`**.

- **Triggers:** `pull_request` + `push` on master, plus `workflow_dispatch`
  with a runner override input. 60-minute timeout.
- **Concurrency:** superseded PR pushes cancel the in-flight run
  (`cancel-in-progress` on pull_request only); master pushes always
  complete.
- **Steps:**
  1. Checkout.
  2. `./.github/actions/zenith-setup` -- provisions MSBuild, vcpkg,
     Vulkan SDK **1.3.290.0**, Slang **2026.1**.
  3. `Build/regen.ps1 -UseDotnet` (regenerate-first policy; generated files
     are git-ignored so a fresh checkout has none).
  4. Build `Games\Zenithmon\zenithmon_win64.sln /t:Zenithmon`
     `Vulkan_vs2022_Debug_Win64_True`.
  5. Build `D3D12_vs2022_Debug_Win64_False` -- the backend-neutrality
     LINK proof (null backend; no rendering).
  6. Copy runtime DLLs into the exe dir (full Slang tree +
     vulkan-1.dll -- see BuildEnvironment.md section 5).
  7. Headless boot check: `zenithmon.exe --list-automated-tests
     --skip-tool-exports --skip-unit-tests --headless` must exit 0.
  8. **Boot unit tests** (`Tools/run_unit_gate.ps1 -Exe <zenithmon.exe>
     -Baseline N`): boots headless with tool-exports ON, runs the ZENITH_TEST
     suite (engine units + Zenithmon `ZM_*` cases), and fails on any failure or a
     count != baseline. Steps 7 and 9 both pass `--skip-unit-tests`, so this is
     the ONLY step that runs the unit suite -- the S1+ data-core gate backbone.
  9. `zenith.bat test Zenithmon --headless --results-dir
     Build/artifacts/test_results/zenithmon` (the automated/P1 suite).
  10. Upload the per-test JSON as artifact **`zm-test-results`**
     (`if: always()` -- results survive red runs).

**Unit-test baseline ratchet:** step 8's `-Baseline` is the exact registered
unit-test count of `zenithmon.exe` (engine units + `ZM_*` cases; currently
**1379**, of which 1 is the quarantined `RegistryWideNodeRoundTrip` skip). Every
commit that changes the `ZM_*` count -- and any engine change that changes the
engine unit count -- bumps this number in `zm-tests.yml` in the same commit.
This mirrors engine-gate's discipline and guards against unit tests silently vanishing; the
coupling-vs-simplicity trade-off is Questions.md Q-2026-07-10-004.

## 2. Runner constraints -- why headless pure-logic suites are the backbone

Two hard facts about GitHub-hosted `windows-latest` runners shape
everything:

1. **No GPU / no Vulkan ICD.** The engine boots with `--headless`
   (short-circuits Flux init so `vkEnumeratePhysicalDevices` never runs);
   tests tagged `m_bRequiresGraphics=true` are skipped by the harness
   (skip counts as pass). Windowed/graphics tests therefore run LOCALLY at
   stage gates, never in CI.
2. **No baked assets.** Everything under `Games/Zenithmon/Assets/` is
   git-ignored (AssetManifest.md), so a fresh CI checkout has NO Assets/
   directory. Every asset/scene-dependent automated test must exists-guard
   and `RequestSkip` (the commit-`94813489` pattern).

Consequence: **the CI backbone is the headless pure-logic suites** -- boot
unit tests and, as stages land, battle engine / data-table / stats / AI /
breeding / tower / save-schema / generator-determinism-in-memory /
WorldSpec-integrity tests. These touch no disk assets and no GPU, so they
run in FULL on every `zm-tests` invocation. This is a deliberate design
constraint on all new tests, not an accident (see TestPlan.md).

CI budget: the headless batch must stay in the minutes range (DP precedent:
~2 min for ~140 tests); the slowest-10 report is reviewed at every stage
gate.

## 3. Pre-existing repo gates (`.github/workflows/`, verified 2026-07-09)

| Workflow | Purpose |
|---|---|
| `dp-tests.yml` (`dp-tests`) | DevilsPlayground gate: Vulkan_True build + headless suite. The pattern zm-tests clones. |
| `cb-tests.yml` (`cb-tests`) | CityBuilder gate: same pattern (build + headless suite + results artifact). |
| `complexity.yml` (`complexity-gate`) | Engine-wide complexity-ceiling ratchet (analyze_code_complexity.py thresholds). |
| `layering-gate.yml` (`layering-gate`) | Architecture ratchet: layer-DAG direction, ECS-leaf purity, encapsulation + convention lints. |
| `memory-gate.yml` (`memory-gate`) | Memory-budget ratchet: committed baseline JSON validation, no build needed. |
| `engine-gate.yml` (`engine-gate`) | Engine-only proofs: Sentinel leaf-purity links (SentinelECS/Physics/AI) + the engine unit-test suite at boot (game workflows run `--skip-unit-tests`; this gate owns them). |
| `shader-validation.yml` (`shader-validation`) | Shader catalog validity + feature parity; fails if running FluxCompiler dirties the generated tree. |
| `doc-lint.yml` (`doc-lint`) | Cross-document consistency lint (currently DP-scoped docs via Tools/doc_lint.ps1). |
| `scaffold-smoke.yml` (`scaffold-smoke`) | Path-filtered proof that `zenith new` still produces a game that builds + boots; non-required burn-in. |
| `release-build.yml` (`release-build`) | Nightly Release-config build proof; deliberately NOT PR-blocking. |

The authoritative list of which checks are REQUIRED lives in GitHub branch
protection (Settings -> Branches), not in this file; keep the table above in
sync when gates change. `release-build` (nightly) and `scaffold-smoke`
(burn-in) are documented as non-blocking by their own headers.

## 4. Branch protection: `zm-tests` required (ACTIVE since 2026-07-10)

Master branch protection now exists and requires `zm-tests`. History + the
exact shape (because it differs from the original plan in two ways):

1. The first Zenithmon PR (#143, S0 skeleton) put a green `zm-tests` run on
   record; the follow-up CI-fix PR (#144) made the whole gate set green.
2. Master had **no branch protection and no rulesets at all** -- the
   "required checks" discipline had been purely conventional. Classic branch
   protection was CREATED (user-directed, via `gh api PUT
   .../branches/master/protection`): required status checks `[zm-tests]`
   with `strict=false`, `enforce_admins=false`, no required reviews.

Consequences of that shape:
- `zm-tests` BLOCKS every non-admin merge into master.
- `enforce_admins=false` means the repo owner's direct pushes bypass the
  check -- deliberate, matching the established direct-push workflow. Under
  ZM-D-031, agent work also lands by direct push: the full local gate is the
  pre-push authority and `zm-tests` is a post-push backstop.
- Only `zm-tests` is machine-enforced; the other gates in section 3 remain
  enforced BY LOCAL DISCIPLINE where relevant; a red post-push run is fixed
  forward (section 5). Add other checks to the protection contexts only
  deliberately -- several are path-filtered and a required check that never
  reports deadlocks non-admin merges.

## 5. Direct-master landing policy (ZM-D-031)

- **The full local gate must be green before every commit and direct push to
  `master`.** It is the authoritative quality bar: build, exact-baseline boot
  unit gate, headless suite, and the task/stage-scoped windowed checks.
- Do not create a branch or PR, and do not wait for CI. The master push starts
  `zm-tests` as a backstop; if it reports red, fix forward with another direct
  commit (never revert shipped history, force-push master, or rerun the old
  workflow execution).
- Artifacts: pull `zm-test-results` (per-test JSON, includes durations)
  from the run page to diagnose failures without reproducing locally.

## 6. Escalation path when CI is red

1. **Fix forward on `master`.** Reproduce locally, then make a new direct
   commit and push; never revert shipped history, force-push master, use
   `gh run rerun`, or remove the check. After three failed fix-forward attempts
   on the same failure, record the blocker in Questions.md, refresh Status.md,
   and stop that iteration.
2. **Reproduce locally first:** `zenith test Zenithmon --headless` mirrors the
   CI automated/P1 step (BuildEnvironment.md section 4). For a red UNIT test,
   reproduce the boot gate instead -- `Tools/run_unit_gate.ps1 -Exe
   <zenithmon.exe> -Baseline <N>`, or boot `zenithmon.exe --list-automated-tests
   --headless` WITHOUT `--skip-unit-tests` -- because `zenith test` skips unit
   tests. If green locally but red in CI, suspect the two runner constraints
   (section 2) -- an asset-dependent test missing its exists-guard/RequestSkip,
   or a graphics dependency missing its `m_bRequiresGraphics` tag.
3. **Get data, don't theorize:** pull the `zm-test-results` artifact and
   the step logs (the boot-check step prints head+tail of engine output on
   failure) before forming a theory.
4. **If CI infrastructure itself is broken** (runner outage, zenith-setup
   provisioning failure) -- not the code -- record it in Questions.md and
   surface it to the user. Do not rerun the old execution or alter branch
   protection; any user-directed protection change gets a DecisionLog.md entry.
