# Zenith2 Roadmap

The delivery plan for Zenith2, from tooling bring-up to shipping qualification and the deferred expansions: eight stages, each one sprint and one fix version on the Z2 board, each closed by a designated acceptance ticket that needs production evidence.

## Purpose

This document is the stage-by-stage plan the Z2 board implements. For every stage it states the goal, the exact sprint and fix-version strings, the entry criteria, the full deliverable list, the gate steps that become active, the decisions ruled, the budget rows allocated, the evidence the stage acceptance ticket collects, the mid-sprint integration check, and which work of subsequent stages is pulled forward into its sprint.

It is the authority for:

- the order of the stages and what each one delivers;
- the board conventions the filer and every ticket use (epics, sprints, fix versions, labels, ticket types, routing, categories);
- the integration cadence that runs throughout development;
- the acceptance evidence catalogue;
- the flagged proposals awaiting the human.

Related documents, cited by their exact `##` headings:

| Topic | Where |
|---|---|
| Budget rows, workloads, measurement procedures, production-quality acceptance | `Zenith2/Docs/Budgets.md` → "Row index", "Workload definitions", "Production-quality acceptance" |
| Settled and open decisions | `Zenith2/Docs/DecisionLog.md` → "Settled decisions", "Open decisions" |
| Capability inventory | `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" |
| Engineering contracts | `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates", "Review and change management" |
| Architecture | `Zenith2/Docs/Architecture.md` → "Repository layout", "Build system and presets" |
| Rendering | `Zenith2/Docs/Rendering.md` → "Shading paths and capability matrix", "Validation roles" |
| Scripting | `Zenith2/Docs/Scripting.md` → "Required conformance tests", "Fault-matrix tests" |

Scope boundaries are recorded decisions and are not reopened here: consoles are deferred to Stage 7 and gated on SDK access (Z2-D-071); VR/XR is deferred to Stage 7 through OpenXR, with multi-view and input kept XR-ready (Z2-D-072); networked multiplayer and dedicated servers are deferred to Stage 7 with Stage 1 foundations and a Stage 3 loopback proof (Z2-D-073); the Steam and Google Play online-service backends land in Stage 6, with the adapter API and its offline implementation required in Stage 4 (Z2-D-074); modding is excluded (Z2-D-075); local multiplayer and split-screen are excluded (Z2-D-076); quest, dialogue, inventory and interaction are Luau gameplay modules inside OpenWorldSlice, never engine features (Z2-D-077); 2D is first class and hair/groom is excluded (Z2-D-078).

## Stage overview

**Sprints schedule the work; fix versions are milestones.** A ticket's fix version is the stage whose acceptance it serves. Its sprint is where it is scheduled, which may be earlier (see "Pull-forward rule").

| Stage | Name | Sprint | Fix version | Goal in one line | New gate steps | Open decisions ruled |
|---|---|---|---|---|---|---|
| 0 | Tooling bring-up | `Stage 0 — Tooling bring-up` | `Z2 Stage 0` | the /tick loop runs a Z2 ticket end to end in the Z2 lane | the base list: Configure, Build win-debug/win-release, Test, SourceLayout, CompileCoverage, Libgraph, Complexity, ForbiddenCheck, Format, HeaderSelfContained, ApiContractCheck, TestSeparationCheck, Coverage, ReviewSignals; plus the hand-over lines ShaderChecks (`Zenith2Render`), EcsConformance (`Zenith2ECS`) and `Test -Label memory` (`Zenith2Memory`) | none (implements settled decisions) |
| 1 | Architecture validation | `Stage 1 — Architecture validation` | `Z2 Stage 1` | every Stage 2 consumer's governing decision, budget and spec is concrete | none | D-SLANG (open, Stage 1), D-SEL-HEAP (open, Stage 1), D-SEL-PHYSX-ANDROID (open, Stage 1), D-LUAU-STATELESS (open, Stage 1), D-LUAUDBG (open, Stage 1), D-PHASES refinement (open, Stage 1), D-SCRIPT-TXN refinement (open, Stage 1), D-SEL-JSON (open, Stage 1), D-SEL-HASH (open, Stage 1); D-SEL-NAV (open, Stage 1 or 2), D-SEL-TEXT (open, Stage 1 or 2), D-SEL-UIRT (open, Stage 1 or 2) |
| 2 | Foundation | `Stage 2 — Foundation` | `Z2 Stage 2` | the engine's foundations build and pass on Windows and Android | Build android-arm64 and ApkCheck (base, every ticket); `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` (`Zenith2Memory`) | D-SEL-CODEC (open, Stage 2), D-SEL-VIDEO (open, Stage 2), D-SEL-CRASH (open, Stage 2), D-SEL-2DPHYS (open, Stage 2), D-CLASS-SIM (open, Stage 2); any of D-SEL-NAV (open, Stage 1 or 2), D-SEL-TEXT (open, Stage 1 or 2), D-SEL-UIRT (open, Stage 1 or 2) not ruled in Stage 1 |
| 3 | Playable authoring slice | `Stage 3 — Playable authoring slice` | `Z2 Stage 3` | a playable, authorable Forward+ game on both APIs and the S24 | BackendParity (`Zenith2Render`); IntegrationSmoke (a CI-only `Run-Committed.ps1` step, no category) | D-SEL-MOTIONMATCH (open, Stage 3), D-VTX-TANGENTLESS (open, Stage 3) |
| 4 | Production content and worlds | `Stage 4 — Production content and worlds` | `Z2 Stage 4` | production content pipelines and the first playable milestones of all three flagship slices | none | none |
| 5 | Rendering and simulation completion | `Stage 5 — Rendering and simulation completion` | `Z2 Stage 5` | all four shading paths, the path tracer, advanced lighting, VFX and specialist simulations | PathParity (`Zenith2Render`) | D-TBDR (open, Stage 5) |
| 6 | Shipping qualification | `Stage 6 — Shipping qualification` | `Z2 Stage 6` | packaged, qualified Windows and S24 builds of all three slices | none | none (the Play base-module limit is pinned as budget row Z2-B-PACK-001) |
| 7 | Later expansion | `Stage 7 — Later expansion` | `Z2 Stage 7` | networked multiplayer, RT and virtualized geometry, XR, consoles | per expansion, by its own PROC ticket | per expansion, by its own decision tickets |

<!-- elaboration -->
Every stage's deliverables are drafted in full detail, including Stages 3 to 7, with exact files, signatures and tests (Z2-D-070). The accepted consequence is that Stage 1 and 2 rulings trigger revalidation rewrites of many downstream tickets; that cost is carried by the revalidation tickets and the filer's reconcile mode (see "Board conventions").

### Stage acceptance mechanics common to every stage

Every stage N has exactly one designated acceptance ticket:

<!-- elaboration -->
- titled `Z2 S<N> stage acceptance` (a ticket title, not a sprint name: sprints are named `Stage <N> — …`, see "Board conventions" → "Sprints and fix versions");
- category `Zenith2Acceptance` (the only category the filer gives acceptance tickets), label `needs-human`, fix version `Z2 Stage <N>`, sprint `Stage <N> — …`;
- designated as the sprint's gate with `zagent sprint gate`; `zagent sprint complete` refuses to complete a sprint whose gate ticket is not Done; that enforcement is a Vault change filed on ZEN (human ruling Q6 (b), 2026-10-16), one of the ZEN tooling tickets that are Done before Stage 0 starts ("Stage 0 entry criteria" item 4), and the human completes the sprint only after the acceptance ticket is Done (see "Board conventions" → "Sprints and fix versions");
- closed by a human actor, like every `needs-human` ticket (Z2-D-103): `zagent move <KEY> "In Progress" --human`, then `zagent move <KEY> Done --human`, or `zagent finish <KEY> --status Done --human` with its evidence files;
- `blocks`-linked from every ticket of its stage (fix version `Z2 Stage <N>`) and from the stage's mid-sprint integration check;
- its body lists **every requirement of its stage**, which the coverage gate step checks;
- run by the human, never by `/tick` (which refuses a `needs-human` ticket even by name), through `pwsh -NoProfile -File Zenith2\Tools\Gates\Run-Committed.ps1 -Acceptance -Stage <N>` in the **main checkout**, while every lane is **idle** (lanes are simply not fired; there is no pause mechanism), holding the machine-wide build slot through `Tools/build_lock.ps1 -Run`;
- the run executes the full acceptance suites under the `Zenith2Acceptance` category's longer `phaseTimeoutMinutes` (budget row Z2-B-PROC-006), exempt from the 30-minute warm cap, and **also runs the full sanitizer set**;
- it runs the coverage gate step in **acceptance mode** (`-Stage <N>`): every decision, fixture and budget row referenced by a stage ≤ N ticket must exist (the decision in `DecisionLog.md`, the fixture on disk, the row in `Budgets.md` with status `adopted`), and implemented coverage is read from the generated `Zenith2/out/coverage/implemented.md`;
- every evidence record it relies on must carry an input fingerprint that matches the accepted tree (see `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure");
- any open BR-ACCEPT budget breach against a row the stage measures blocks it until fixed or re-budgeted by a decision;
- the evidence is cumulative: stage N's acceptance re-runs every earlier stage's acceptance suites as well as its own.

## Stage 0 Tooling bring-up

### Stage 0 goal

Bring up the Zenith-side and Z2-side tooling so that the /tick loop can claim, build, gate, integrate and finish a Z2 ticket end to end in the dedicated Z2 lane, with CI running the same active gate set. Stage 0 is a **closed DAG**: nothing in it depends on a subsequent stage.

### Stage 0 sprint and fix version

- Sprint: `Stage 0 — Tooling bring-up`
- Fix version: `Z2 Stage 0`
<!-- elaboration -->
- Epics: every Stage 0 ticket's parent is `Z2 PROC — Process & gates`, except the scaffold, whose parent is `Z2 MOD — Engine modules & projects`.

### Stage 0 entry criteria

1. The A8a docs-and-config commit, the A8b board seed commit (`board: seed Z2 backlog`) and the A8c coverage report commit have landed on `master` (see "Board conventions" → "Filing and the three-commit landing procedure").
2. The board verification passed:
   - `zagent doctor` ok, and every sprint (Stages 0 to 7) PLANNED;
   - `zagent queue --project Z2` lists the To Do rows, **every one `claimable:false`**, each with a `blockedBy` or out-of-stage reason (`claimable` and `blockedBy` are queue-row fields); the scaffold's and every downstream row's `blockedBy` is non-empty;
   - `zagent check` payloads: `needsHuman: true` and `contractValid: false` on every `needs-human` ticket; `contractValid: false` on every blocked ticket and every epic; `contractValid: true` on the remainder; a `contractError` naming the protected path on every PROC ticket whose DoD names one; no `boardRefWarning` and no `markerWarning` on any ticket;
   - `zagent blocked --project Z2` shows a reason on every block, and `zagent report epic --project Z2` lists every area epic (both `--project Z2` reads need the `jira.projects` row in `metadata.md`, Z2-D-088, which the human approved on 2026-10-16 (ruling Q1 (a)) and makes as a human edit in A8a).
3. Other projects are unaware of Zenith2: `Test-ZagentClient.ps1`, `Test-HarnessParity.ps1`, the Vault tests and `zagent doctor` pass; `zagent gates` output for existing ZEN and ZM tickets is unchanged; `git grep -nE '\bZenith2/'` outside `Zenith2/` matches only `vault.config.json` and `zagent.project.json`.
4. The ZEN tooling tickets filed under the human's rulings Q5 (a) and Q6 (b) of 2026-10-16 are Done: the side defects found while verifying the tick protocol (`languageConventions` missing from every category's resolved conventions; `version create` rows without `project_id`; the stale TICK.md and repository-guide statements; the reviewer charter's claim that the diff is inlined) and the Vault change that makes `zagent sprint complete` refuse a sprint whose gate ticket is not Done. Zenith2 Stage 0 does not start until every one of them is Done: at A6 the filer links each of them `blocks` to the Stage 0 kickoff, so no Stage 0 ticket is claimable before they are.
5. A human runs `zagent sprint start` for `Stage 0 — Tooling bring-up`. Nothing on the Z2 board is claimable before this.

### Stage 0 deliverables

Every Stage 0 ticket except the scaffold edits a protected or Zenith-side path, so each is a PROC `needs-human` ticket, executed by a human-driven session, and fully specified like any other ticket: its Implementation Contract names the file and the change.

1. **First: the global guardrail additions.**
   - Add `Zenith2/Tools/Gates/**` and `Zenith2/CLAUDE.md` to the **global** guardrail lists in `vault.config.json` (never a per-project `protectedPaths` for Z2: `guardrailsOf` is a fallback chain, so a project-level list would *replace* the global one).
   - `.github/**` is already protected, which covers `zenith2-gates.yml`.
   - Because the protected-path list feeds the worker charter's `{{PROTECTED_PATHS}}` substitution, regenerate every harness: `Build-HarnessArtifacts.ps1 -Harness claude`, `-Harness opencode`, `-Harness codex`, then `-Check`.
2. **The generic `tick_gates.ps1` external-build mode**, inside its existing phase machinery (build slot, tree-SHA start and end, retry-once, `Write-Result`, the exit ladder 0/2/3/4/5/8/10/11/12):
   - a category with `buildSystem: "external"` changes only three things: no Sharpmake line classification (lines split into measure and verify by the category's `measureSteps`); no created-file regen and no `.obj` proof; `PlannedLines` from that split;
   - `measureSteps` is checked **when the gate list is selected**, not per category at config load: every Z2 category carries the same non-empty `measureSteps` (Configure and the Build lines), and the selected union's `measureSteps` is valid when it is non-empty and every line of it appears in the **union** of gate lines the ticket runs; the base `Zenith2` category is in every Z2 union, so the add-on categories (`Zenith2Render`, `Zenith2ECS`, `Zenith2Memory`), whose own `gates` hold one line each, stay legal with the identical settings Z2-D-086 requires; a union whose `measureSteps` is empty, or names a line absent from the union, is refused with exit 8 before any line runs, with the offending line named in `tick_gates.json`; Test_TickGates fixtures cover an add-on-only union (refused: no base lines), a base-plus-add-on union (accepted) and a `measureSteps` line missing from the union (refused);
   - a mixed union of an external and a Sharpmake category exits 5;
   - the one-shot flake re-run (`Invoke-GateLineFlakeAware`) is skipped for `measureSteps` lines;
   - the generic **`phaseTimeoutMinutes`** key: on expiry the running line's process tree is killed through the existing watchdog path (`taskkill /T /F`, the exe sweep, exit 124), the phase fails with exit 4, the killed line is recorded in `steps`, and a phase-cap kill is never flake-retried;
   - `Test_TickGates.ps1` fixtures: external mode never invokes `regen.ps1`; it honours `-ReuseGateList`; it writes a verdict that `integrate` accepts as GREEN.
3. **The Vault pass-through**, generic: `resolveRules` (`Tools/Vault/cli/legacy-pure/routing.mjs`) surfaces `buildSystem`, `measureSteps`, `phaseTimeoutMinutes` and `reviewPaths`; `zagent gates` writes them into `gates.json`, together with each unioned category's `buildSystem` for the mixed-union check. Vault tests for both.
4. **`include_roots` for `analyze_code_complexity.py`**, with a test, and **the scanner audit** of `.github/**`, `doc_lint.ps1`, `Test-HarnessParity.ps1`, the analyzer, and every `**/*.cpp` or `**/*.md` glob, each fixed generically before any Zenith2 source lands.
5. **The root delegation line** (Z2-D-030): the root `CLAUDE.md` and the universal convention string in `zagent.project.json` gain "`Zenith2/**` is governed by `Zenith2/CLAUDE.md`.", allowlisted in the blindness check.
6. **libgraph's generic CMake File API front end**, alongside its Sharpmake/vcxproj front end, reading `Zenith2/out/<hash>/declared_deps.json` (the declared direct dependencies written by `zenith_add_library()`), with tests.
7. **A generic `max-file-loc` key** for `analyze_code_complexity.py`, with a test. The Zenith2 profile itself lives in `Zenith2/Tools/Gates/complexity_profile.json`.
<!-- elaboration -->
8. **The base Z2 gate step scripts** under `Zenith2/Tools/Gates/` — `Configure`, `Build`, `Test`, `SourceLayout`, `CompileCoverage`, `Libgraph`, `Complexity`, `ForbiddenCheck`, `ApiContractCheck`, `TestSeparationCheck`, `ReviewSignals` — plus `Format` and `HeaderSelfContained` (in the base list as an elaboration: they are structural checks); **`ShaderChecks.ps1`** and **`EcsConformance.ps1`** (the ECS conformance suite against the custom, flecs and EnTT backend targets), whose lines sit in `Zenith2Render`'s and `Zenith2ECS`'s gates from hand-over and which pass with a report line while no shader source or ECS backend target exists; `Test.ps1`'s `-Label <label>` parameter (runs only the CTest tests carrying that label), which `Zenith2Memory`'s hand-over line uses; **`Sweep.ps1`**, the categories' `cleanup` line (`pwsh -NoProfile -File Zenith2\Tools\Gates\Sweep.ps1`), which kills the Z2 Player and test exes scoped to the checkout path and removes nothing under `Zenith2/out/` (lane mode skips `cleanup`, so only a main-mode Z2 tick reaches it); and **`Run-Committed.ps1`**, CI's entry point, which reads the union of every Z2 category's gate lines straight from the committed `zagent.project.json`. Each step is self-sufficient and order-independent, writes only under `Zenith2/out/<checkout-hash>/`, never writes into `.zagent/run/`, and sweeps its own exes scoped to the checkout path. A fixture runs a full verify and asserts `treeShaStart == treeShaEnd`.
9. **The production coverage checker** (`Coverage.ps1`, protected, read-only) with **failing fixtures**: one fixture board per failure mode, and a test asserting each is detected. It regenerates into `Zenith2/out/coverage/`, fails on drift from the tracked `Coverage.md`, runs the normative-grammar scan over the five scanned docs, recomputes cited-section hashes and reports stale cites without failing on them.
10. **The production filer** (`Zenith2/Tools/Backlog/File-Backlog.ps1`) with **reconcile mode**: it owns only the managed specification region (the ten template sections plus a trailing `Spec-Hash:` line), preserves everything else byte for byte, refuses a conflicting hand edit (`--force-spec` overrides one ticket at a time), touches only links whose reason starts `backlog:`, is idempotent across interrupted runs, supports `--dry-run` and `--mark-stale`, and generates `Docs/Coverage.md` explicitly. Required tests: comments and worklog survive byte for byte; a conflicting hand edit is refused; an interrupted run followed by a re-run converges; hand-added links survive.
11. **`.github/workflows/zenith2-gates.yml`** (Z2-D-031), allowlisted: runs only when the user pushes; triggered by `Zenith2/**`, the workflow itself, `zagent.project.json`, `vault.config.json`, `Tools/tick_gates.ps1`, `Tools/Vault/**`, `Tools/build_lock.ps1`; runs the union of every Z2 category's active gates **excluding `Zenith2Acceptance`**; runs the Linux job only if a `linux-tsan` line is active; a test in `Zenith2/Tools/Gates/Tests` asserts that for any diff the local path-selected set is a subset of the CI set.
12. **PROC-API** (Z2-D-049), `needs-human` because it edits protected files:
    1. a generic `reviewPaths` category key; `zagent gates` (at measure, with the changed set) writes `review.requiredByDiff` into `gates.json`;
    2. a `TICK.md` step 6.3 edit: the reviewer is dispatched when `review.required` OR `requiredByDiff` is true, followed by `Build-HarnessArtifacts.ps1` regeneration for every harness and a re-derived tier-mention allowlist;
    3. a new generic verb `zagent review record <KEY> --verdict <pass|fixes-applied> --reviewer <tier>`, written into the ticket's history with a hash over the current blobs of every changed path in the `reviewPaths` scope;
    4. `zagent finish --status Done` refuses when `requiredByDiff` is true and no review record's hash still matches the current blobs;
    5. a test that a post-review edit to a reviewed header is refused at `finish`;
    6. Vault tests for each of these.
13. **The generic per-category `postFinish` command** in `TICK.md`, with harness regeneration; Z2 declares one command, `pwsh -NoProfile -File Zenith2/Tools/Backlog/File-Backlog.ps1 --mark-stale`, which applies `needs-revalidation` **and** `deferred` to every ticket with a stale cite and drops its `Spec-Maturity` to `draft` — `deferred` is what removes the ticket from the queue, because nothing reads `needs-revalidation` today (Z2-D-102). The pairing stands until a Vault PROC ticket adds `needs-revalidation` to the claim predicates.
14. **The reproducibility manifest and toolchain pins** (Z2-D-008): `Zenith2/ThirdParty/manifest.json` with hashes for MSVC, LLVM (clang-tidy/clang), CMake, Ninja, Slang, DXC (offline shader tool only), WARP NuGet, lavapipe, NDK r27c, JDK 17, Gradle, AGP, and the SDK platform and build-tools; the clean-machine setup script under `Zenith2/Tools/Setup/`; the pinned sccache shared across lanes.
15. **The generic `reviewerConventions` category field**, inlined into the reviewer prompt; the Z2 value instructs reviewers to challenge behaviour beyond the author's tests and to check each ticket's "Review focus" list.
16. **`zagent lane add Z2`** (Z2-D-029): the dedicated Z2 lane, one lane per project, so Z2 runs one ticket at a time; CMake presets and the WSL `binaryDir` are keyed per checkout path. A Stage 0 PROC ticket ahead of it adds a generic per-project provisioning hook to `zagent lane add` (ruling Q3 (a), 2026-10-16), so the Z2 lane provisions the Z2 toolchain instead of every Zenith game. The lane checkout's storage cost is measured into budget row Z2-B-PROC-001.
<!-- elaboration -->
17. **Finally, the scaffold ticket**, the first ticket `/tick` runs, once the external mode and the base gate steps exist. It is an ordinary (not `needs-human`) ticket, parent `Z2 MOD — Engine modules & projects`:
    - `Zenith2/CMakeLists.txt`, `Zenith2/CMakePresets.json` with the presets `win-debug`, `win-release`, `win-tools`, `win-asan`, `linux-tsan` (`condition: hostSystemName`), `android-arm64`, `android-x86_64` (emulator smoke only), Ninja Multi-Config;
    - the shared `zenith_add_library()` helper (globbed sources with `CONFIGURE_DEPENDS`, `Internal/Windows|Android|Linux/` filtering, `Tests/` feeding the separate test target, `ZENITH_SUPPORTED` matrices, `declared_deps.json`) and `zenith_add_game()`;
    - `win-debug` built with `/Gy-`; the r27 toolchain with `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON` and `-Wl,-z,max-page-size=16384`;
    - **the `android-arm64` preset and a Player stub**, which the Stage 1 D-SLANG (open, Stage 1) arm64 probe needs;
    - a first empty engine library and its doctest target, so every base gate step has something to check.

<!-- elaboration -->
One generic Vault change recorded in `Zenith2/Docs/DecisionLog.md` → "Open decisions" (its Stage 0 PROC notes for the rulings scan) is also a Stage 0 PROC `needs-human` ticket, filed ahead of the scaffold like items 1–16: **a direct decision-ID citation is self-sufficient** — `build_prompt`'s rulings matcher weights a direct `Z2-D-NNN` citation in a ticket's Goal or DoD at `RULINGS_FILE_MIN_SCORE`, so the citation alone selects its entry (today it scores 2, half the threshold of 4, and never selects), and cited IDs sort above uncited candidates so a multi-citation ticket does not saturate the 25-entry list; with a Vault test for each. The second note there, an array-valued `decisionLog`, is not filed, because the log is never split.

### Stage 0 gate steps

Stage 0 activates the base list in the `Zenith2` category's `gates` in `zagent.project.json` (the one source of truth, Z2-D-032). Every line has the form `pwsh -NoProfile -File Zenith2\Tools\Gates\<Step>.ps1 …`.

| Step | Category | Measure or verify | Budget row (step timeout) |
|---|---|---|---|
| Configure | Zenith2 | measure | Z2-B-PROC-007 |
| Build -Preset win-debug | Zenith2 | measure | Z2-B-PROC-008 |
| Build -Preset win-release | Zenith2 | measure | Z2-B-PROC-009 |
| Test | Zenith2 | verify | Z2-B-PROC-011 |
| SourceLayout | Zenith2 | verify | Z2-B-PROC-012 |
| CompileCoverage | Zenith2 | verify | Z2-B-PROC-013 |
| Libgraph | Zenith2 | verify | Z2-B-PROC-014 |
| Complexity | Zenith2 | verify | Z2-B-PROC-015 |
| ForbiddenCheck | Zenith2 | verify | Z2-B-PROC-016 |
| Format | Zenith2 | verify | Z2-B-PROC-017 |
| HeaderSelfContained | Zenith2 | verify | Z2-B-PROC-018 |
| ApiContractCheck | Zenith2 | verify | Z2-B-PROC-019 |
| TestSeparationCheck | Zenith2 | verify | Z2-B-PROC-020 |
| Coverage | Zenith2 | verify | Z2-B-PROC-022 |
| ReviewSignals (report only) | Zenith2 | verify | Z2-B-PROC-021 |

<!-- elaboration -->
The measure/verify split above is the `measureSteps` list the Z2 categories declare: the build-producing steps are measured, everything else verifies. The phase caps (Z2-B-PROC-002, -003, -004) and the cold cap (Z2-B-PROC-005) apply from the first gate run.

### Stage 0 category gate lists at hand-over

Every Z2 category has a non-empty `gates` list from A1 (a `Test-ZagentClient.ps1` requirement). `measureSteps` is checked **when the gate list is selected**, not per category at config load: every Z2 category carries the same non-empty `measureSteps` (Configure and the Build lines), and the selected union's `measureSteps` is valid when it is non-empty and every line of it appears in the **union** of gate lines the ticket runs; the base `Zenith2` category is in every Z2 union, so the add-on categories (`Zenith2Render`, `Zenith2ECS`, `Zenith2Memory`), whose own `gates` hold one line each, stay legal with the identical settings Z2-D-086 requires (see Stage 0 deliverable 2). What each list holds at hand-over, and what PROC tickets add afterwards:

| Category | `gates` at hand-over | Added afterwards, by a `needs-human` PROC ticket | Budget rows (step timeouts) |
|---|---|---|---|
| `Zenith2` | the base list above | `Build -Preset android-arm64` and `ApkCheck` (Stage 2) | the base rows; Z2-B-PROC-010, -023 |
| `Zenith2Render` | `ShaderChecks` | `BackendParity` (Stage 3); `PathParity` (Stage 5) | Z2-B-PROC-024; -027; -029 |
| `Zenith2ECS` | `EcsConformance` (`pwsh -NoProfile -File Zenith2\Tools\Gates\EcsConformance.ps1`) | — | Z2-B-PROC-031 |
| `Zenith2Memory` | `Test -Label memory` (see below) | `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` (Stage 2) | Z2-B-PROC-011; -025, -026 |
| `Zenith2Acceptance` | the `Run-Committed.ps1 -Acceptance -Stage <N>` lines | — | Z2-B-PROC-006 |

The scripts behind the hand-over lines (`ShaderChecks.ps1`, `EcsConformance.ps1`, `Test.ps1 -Label`) are written by Stage 0 deliverable 8, so every line a Render, ECS or Memory diff can union in exists before the first such diff lands.

<!-- elaboration -->
`Zenith2Memory`'s `gates` list at hand-over is `pwsh -NoProfile -File Zenith2\Tools\Gates\Test.ps1 -Label memory`. It re-runs the base Test step filtered to the CTest tests labelled `memory` (allocator, container, threading and job-graph tests) in `win-debug`, so it adds no coverage the base `Test` line lacks: it is there so the category's `gates` list is non-empty from A1 and so the Stage 2 PROC has a line to extend. The Stage 2 PROC adds `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` and keeps it. Its step timeout is Z2-B-PROC-011.

### Stage 0 decisions

No open decision is ruled in Stage 0. Its tickets implement settled decisions: Z2-D-007 (libgraph and complexity hard gates), Z2-D-008 (reproducibility), Z2-D-028 (gate budget), Z2-D-029 (Z2 lane), Z2-D-030 (root delegation), Z2-D-031 (CI), Z2-D-032 (gate activation), Z2-D-042 (hard gates), Z2-D-044 (process), Z2-D-049 (API review from the diff), Z2-D-066 (normative traceability), Z2-D-067 (coverage), Z2-D-068 (ticket standard), Z2-D-086 (identical category settings).

### Stage 0 budget rows

Allocated by Stage 0 producers (see `Zenith2/Docs/Budgets.md` → "Row index"): Z2-B-PROC-001 (lane checkout storage), Z2-B-PROC-005 (cold phase cap), Z2-B-PROC-006 (acceptance phase timeout), the base step timeouts Z2-B-PROC-007 to -009 and -011 to -022, and the hand-over step timeouts Z2-B-PROC-024 (ShaderChecks) and Z2-B-PROC-031 (EcsConformance), which are allocated once a first real run exists. Adopted from the start: Z2-B-PROC-002, -003, -004 (the 30-minute warm caps and retry cap).

### Stage 0 acceptance ticket

`Z2 S0 stage acceptance` collects:

- the scaffold ticket's run, ticked end to end in the Z2 lane: claim, worker, measure, verify GREEN, `integrate` GREEN, `finish --status Done`;
- the scaffold's `tick_gates.json` with every base step green and `treeShaStart == treeShaEnd`;
- a green `zenith2-gates.yml` run on a push containing the scaffold;
- `Test_TickGates.ps1`, the Vault tests, `Test-ZagentClient.ps1`, `Test-HarnessParity.ps1` and `Build-HarnessArtifacts.ps1 -Check` for all three harnesses passing;
- the coverage gate step clean in acceptance mode `-Stage 0`, including its failing fixtures each detected;
- the filer's reconcile-mode tests passing and a `--dry-run` over the live board printing no unexpected change;
- the PROC-API refusal test (a post-review edit refused at `finish`);
- `zagent gates` for existing ZEN and ZM tickets unchanged from before Stage 0 (other projects still unaware);
- every budget row with a Stage 0 producer adopted in `Budgets.md`.

### Stage 0 mid-sprint integration check

<!-- elaboration -->
`Z2 S0 mid-sprint integration check` (`needs-human`), run once the guardrail, external-mode and Vault pass-through tickets are Done:

- regenerate and `-Check` all three harness artifacts;
- run `zagent doctor`, `Test-HarnessParity.ps1` and `Test-ZagentClient.ps1`;
- confirm `zagent gates` output for a sample of existing ZEN and ZM tickets is byte-identical to its pre-Stage-0 capture;
- file any drift as a PROC BUG ticket in the same sprint.

There is no engine yet, so no workload is measured.

### Stage 0 pull-forward

None. Stage 0 is a closed DAG and every Stage 1 ticket depends on the scaffold's presets or on the gate steps.

## Stage 1 Architecture validation

### Stage 1 goal

Make every Stage 2 consumer's governing decision, budget row and specification concrete, through decision and measurement tickets, so that the Stage 1 revalidation passes can mark Stage 2 tickets `ready`. Stage 1 is mostly `needs-human` decisions and device probes; the independent Stage 2 foundation tickets are pulled forward into its sprint to keep the single Z2 lane busy. Stage 1 stays a **closed DAG**: its revalidation tickets depend only on decisions from Stages ≤ 1.

### Stage 1 sprint and fix version

- Sprint: `Stage 1 — Architecture validation`
- Fix version: `Z2 Stage 1`

### Stage 1 entry criteria

1. `Z2 S0 stage acceptance` is Done and `Stage 0 — Tooling bring-up` is complete.
2. The scaffold's presets (including `android-arm64`) and Player stub build in the Z2 lane.
3. The reference desktop (RTX 3070) and the S24 are available to the human for the device probes.
4. A human starts `Stage 1 — Architecture validation`.

### Stage 1 deliverables

Every decision that needs measurement is split into an **ordinary harness ticket** (the loop runs it; it builds the harness under `Zenith2/Benchmarks/`, never under the protected gate directory, plus a short CTest smoke run inside the normal Test step) and a **`needs-human` ruling ticket** blocked by it (the human runs the full measurement on the reference hardware, commits the report under `Zenith2/Docs/Research/`, and appends the ruling to `Zenith2/Docs/DecisionLog.md` in its own diff with the next explicit `Z2-D-NNN`, per `Zenith2/Docs/DecisionLog.md` → "How this log is written" and Z2-D-097), per Z2-D-084. Dependents are blocked by the ruling ticket.

1. **Ownership, allocator and container contracts** (CORE):
   - `Zenith_Owned<T>` as `{T*}`, `Zenith_New`/`Zenith_Delete`/`Zenith_TryNew`, the general heap, arenas and pools (Z2-D-040);
   - the **ASan poisoning design** for arenas and pools and the HWASan tagging design (16-byte granules, short-granule encoding, tags 16–255, retag on free or reset);
   - the required intentional-defect tests (overflow past an arena block, use after arena reset, use after pool free, 1-, 15-, 16- and 17-byte pool allocations);
   - a decision record for each custom container, allocator, heap wrapper or threading primitive, stating what it improves, its maintenance cost and measured acceptance criteria (Z2-D-041).
2. **D-SLANG** (open, Stage 1) (RHI and RPROD) and **the arm64 bindless probe**:
   - pin the Slang version; select the Vulkan `DescriptorHandle` lowering (mutable descriptors in the profile; a custom per-kind typed-array lowering; or `uint` indices plus generated accessors);
   - acceptance: non-uniform indexing, descriptor limits and generated layouts pass on an RTX GPU, the S24 (Adreno 750), WARP and lavapipe;
   - the `android-arm64` bindless probe runs as a `needs-human` device step.
3. **The S24 profile** (RHI): the Vulkan 1.3 profile with the custom `shaderFloat16` addition (Z2-D-013), verified on the Adreno 750, and the **Stage 1 `shaderFloat16` check on WARP and lavapipe**, without which CI cannot run half-precision shaders.
4. **The `linux-tsan` host and LLVM** (DEVT): the pinned WSL2 distro invoked through `wsl.exe -d <pinned distro> -e bash -lc`, `binaryDir` on the Linux filesystem, `vm.mmap_rnd_bits ≤ 28` set by the Setup script, TSan + UBSan, every C/C++ dependency built from source with the sanitizer's flags (Z2-D-011).
5. **Per-system budget rows**: per area with pending rows, the `needs-human` ruling TASK "Stage 1 per-system budget ticket for <AREA>" writes the values, the area's `allocation.json` and the DecisionLog entry in one diff, after running `Check-Allocation.ps1`. Where Stage 1 has something to measure — CORE, JOBS, GRAPH, ECS, SCRIPT, RHI, RPROD, LIGHT, GEOM, ANIM, PHYS, PLAT, COMPRESS (whose harness half is the ticket delivering its benchmark `Zenith_Bench_Compression` under `Zenith2/Benchmarks/Compression/`) and ASSET (whose harness half is the ticket delivering its benchmark `Zenith_Bench_AssetLoading` under `Zenith2/Benchmarks/AssetLoading/`) — an ordinary harness TASK "Stage 1 per-system budget harness for <AREA>" (the loop builds the measurement harness under `Zenith2/Benchmarks/Budgets/<AREA>/`, with a CTest smoke run; the CORE harness also builds `Zenith2/Benchmarks/Budgets/Check-Allocation.ps1` and its fixtures, and the PLAT harness builds the MP-MEM S24 sampler and its device test; the ECS harness is blocked by the W-Dense scale ticket and the ANIM harness by the W-Characters one) blocks the ruling, and the human runs its measurement on the reference hardware; every other area's ruling is a single ticket citing reference-engine or published data. The areas are (ADVGFX, AI, ANIM, ASSET, AUDIO, CHAR, CINE, COMPRESS, CONTENT, CORE, DEVT, ECS, EDIT, ENV, GAME, GEOM, GRAPH, JOBS, LIGHT, PACK, PHYS, PLAT, RHI, RPROD, SAVE, SCENE, SCRIPT, SHIP, STREAM, TWOD, UI, VFX), each allocating its rows from the adopted totals under the allocation rule of `Zenith2/Docs/Budgets.md` → "Purpose versioning and change rule". The exact row lists are in `Zenith2/Docs/Budgets.md` → "Row index".
6. **The ECS, job and Luau lifetime specifications** (ECS, JOBS, SCRIPT): structural changes through command buffers, epoch scoping, view invalidation, change-tracking versions; the **exception firewall** (`/EHs`, not `/EHsc`, and `-fexceptions` only in `ZenithScript/Internal`, including verifying whether Luau's API has C or C++ linkage in the pinned version, Z2-D-017); the **VM model** (one VM per worker thread per world for chunk systems, behaviours on the world main VM, Z2-D-018).
7. **D-LUAUDBG** (open, Stage 1) (SCRIPT): the DAP client choice for the server built over `debugbreak`/`debugstep`/interrupt callbacks and `lua_breakpoint`, fanning breakpoints out to every VM.
8. **D-LUAU-STATELESS** (open, Stage 1) (SCRIPT): a prototype and an **adversarial corpus** in which every module must be rejected at load time, covering captured locals, nested tables, imported modules, mutable userdata and stateful library functions; plus **the behaviour-state model** (declared reflected state table, dirty-only checkpoint at most once per tick, `OnRecover(state)`, Z2-D-023).
9. **The determinism rules** (ECS, SCRIPT; Z2-D-019): merge order (system order, logical invocation ID, sequence), spawn IDs at playback, event order, fixed-order reduction trees, counter-based per-invocation randomness keyed by stable simulation ID.
10. **The CPU frame-graph and `ZenithGraph` specification** (GRAPH; Z2-D-006, Z2-D-050): access model, hazard rules, schedule compilation, dynamic sub-jobs, pipelining through render extraction.
11. **D-PHASES refinement** (open, Stage 1) (GAME, with JOBS and ECS; pre-decided contract Z2-D-080): the twelve phases, per-field authority, arbitration rules and lifecycle-command semantics; and **D-SCRIPT-TXN refinement** (open, Stage 1) (SCRIPT; pre-decided boundary Z2-D-051): the entity transaction boundary, safepoint budgets (Z2-D-052) including the interpreted-versus-native safepoint conformance test, merge-time capacity, asynchronous publication (Z2-D-053).
12. **Production-quality definitions**: `Zenith2/Docs/QualityMatrix.md` (Stage 1 QualityMatrix ticket), `Zenith2/Docs/ContentSpecs.md` (Stage 1 ContentSpecs ticket), the W-Editor targets (five "Stage 1 W-Editor row ticket for <operation>" tickets), and the three flagship slices' acceptance definitions ("Stage 1 flagship acceptance ticket for OpenWorldSlice / CinematicSlice / Game2D"), each to the exact schemas of `Zenith2/Docs/Budgets.md` → "Production-quality acceptance".
13. **The evidence-fingerprint specification** (DEVT; Z2-D-065) and **the data-defined component schema format** (ECS; `*.schema.json`, Z2-D-020).
14. **D-SEL-HEAP** (open, Stage 1) (CORE) and **D-SEL-PHYSX-ANDROID** (open, Stage 1) (PHYS), each a harness ticket plus a ruling ticket.
15. **The security architecture** (SEC): trust boundaries for scripts, assets, remote tools and updates; parser limits; path-traversal protection; authenticated update manifests; no debugger exposure in shipping builds.
16. **The multiplayer foundations** (NET; Z2-D-073): stable network IDs, the server-authoritative authority model, fixed ticks, replayable commands shared with input recording, the headless configuration.
17. **The static-module system and the in-repo game model** (MOD; Z2-D-002, Z2-D-024, Z2-D-081): `zenith_add_game`, `game.json`, composition-root registration.
18. **The research methodology** (RESEARCH; Z2-D-064): native versus adapter measurement, controlled conditions, repetitions, raw-data retention, conclusions as decision records.
19. **The per-workload scale definitions**: a "Stage 1 workload scale ticket for <workload>" for W-Indoor, W-Outdoor, W-Characters, W-Dense and W-UI2D, each a planned producer that blocks its consumers. Each is a `needs-human` ruling TASK under the owning area's epic (W-Indoor and W-Outdoor: RPROD; W-Characters: ANIM; W-Dense: ECS; W-UI2D: UI) that commits `Zenith2/Docs/Research/Workloads/<W>.md` (numeric scale, scripted-path specification, start-scene description, loop rule) and the workload's `*.workload.json` scale fields; the scene and replay are authored to that specification by the Stage 3 focused workload-game tickets it blocks (`Zenith2/Docs/Budgets.md` → "Workload definitions").
20. **The SCOPE decision tickets**: consoles, XR, modding, local multiplayer, dedicated servers and online backends, each recorded with its rationale (Z2-D-071 to Z2-D-076).
21. **The selection and classification tickets that block Stage 2 consumers**: D-SEL-JSON (open, Stage 1), D-SEL-HASH (open, Stage 1), and D-SEL-NAV (open, Stage 1 or 2), D-SEL-TEXT (open, Stage 1 or 2), D-SEL-UIRT (open, Stage 1 or 2), each listing candidates, criteria and an acceptance rule.
22. **The revalidation tickets** for Stage 1, one per (area, stage, decision set), each `needs-human`, blocked by its governing decisions, with the DoD "every dependent body is consistent with the rulings, and every budget it cites is concrete".

### Stage 1 gate steps

No new gate step. Harness tickets prove their harness with a CTest smoke inside the existing Test step. The `linux-tsan` host is brought up in Stage 1, but the sanitizer gate lines are activated in Stage 2.

### Stage 1 decisions

Ruled in Stage 1: D-SLANG (open, Stage 1), D-SEL-HEAP (open, Stage 1), D-SEL-PHYSX-ANDROID (open, Stage 1), D-LUAU-STATELESS (open, Stage 1), D-LUAUDBG (open, Stage 1), D-PHASES refinement (open, Stage 1) (confirming or refining Z2-D-080), D-SCRIPT-TXN refinement (open, Stage 1) (confirming or refining Z2-D-051), D-SEL-JSON (open, Stage 1), D-SEL-HASH (open, Stage 1). Ruled in Stage 1 or 2: D-SEL-NAV (open, Stage 1 or 2), D-SEL-TEXT (open, Stage 1 or 2), D-SEL-UIRT (open, Stage 1 or 2). Every ruling is appended to `Zenith2/Docs/DecisionLog.md` in its ruling ticket's own diff with the next explicit `Z2-D-NNN`; `zagent decide --ID Z2-D-NNN` is an optional knowledge-row mirror and never writes the log.

### Stage 1 budget rows

Every row whose producer is a Stage 1 ticket is allocated: the per-system rows of the 32 areas above (SHIP's Z2-B-SHIP-015 to -018 excepted: their producer is the Stage 3 per-system budget ticket for SHIP, [Z2-N-BUDGET-482]), the five W-Editor iteration rows, and the three flagship bug-bar caps (Z2-B-SHIP-020 to -022). Each allocation bumps `Budgets.md`'s minor version and appends its allocating decision to `Zenith2/Docs/DecisionLog.md` in the same diff. The two headroom rows and the reserve row (Z2-B-CORE-011, Z2-B-JOBS-003, Z2-B-RPROD-028) are allocated first, by their areas' rulings, because every other allocation is checked against them: every other per-system ruling ticket is `blocks`-linked from the CORE, JOBS and RPROD ruling tickets; the RPROD ruling is `blocks`-linked from the CORE and JOBS rulings; the CORE and JOBS rulings are blocked by no other allocation. The "Producer tickets" table of `Zenith2/Docs/Budgets.md` → "Row index" lists each producer's harness half and blockers.

### Stage 1 acceptance ticket

`Z2 S1 stage acceptance` collects:

- every Stage 1 decision present in `DecisionLog.md`, and every research report committed under `Zenith2/Docs/Research/` with raw data, repetitions, medians and 95% confidence intervals;
- the D-SLANG (open, Stage 1) probe results on the RTX GPU, the S24, WARP and lavapipe, and the `shaderFloat16` check green on WARP and lavapipe;
- the D-LUAU-STATELESS (open, Stage 1) adversarial corpus rejected at load, every module;
- the `linux-tsan` host building and running a TSan smoke under WSL2;
- every Stage 1 budget row `adopted`, with its allocation table under `Zenith2/Docs/Research/Budgets/`;
- `QualityMatrix.md` and `ContentSpecs.md` present and parsing against their schemas; each flagship slice's acceptance package files present under `Zenith2/Games/<Slice>/Acceptance/`;
- every Stage 1 revalidation ticket Done, so every Stage 2 ticket's `Spec-Maturity` is `ready` or it is still blocked by a named Stage 2 decision;
- the coverage gate step clean in acceptance mode `-Stage 1`;
- the Stage 0 acceptance suites re-run green.

### Stage 1 mid-sprint integration check

<!-- elaboration -->
`Z2 S1 mid-sprint integration check` (`needs-human`):

- run every harness that exists at the midpoint (the D-SLANG (open, Stage 1) probe, the heap and PhysX harnesses) on the reference desktop and the S24;
- run the pulled-forward Stage 2 foundation code through the Test step on both platforms;
- check the provisional per-system allocations against the adopted totals of `Zenith2/Docs/Budgets.md`;
- file any regression or inconsistency as a ticket in the sprint.

### Stage 1 pull-forward

The Stage 2 foundation tickets that depend on no unresolved Stage 1 decision are scheduled in `Stage 1 — Architecture validation` while keeping fix version `Z2 Stage 2`:

- Core containers (their allocator dependency is the `Zenith_Heap` interface, not the backing D-SEL-HEAP (open, Stage 1) selects);
- `ZenithGraph` (once the CPU frame-graph and `ZenithGraph` specification ticket is Done);
- `ZenithCompression` with its zlib and zstd backends;
- the `ZenithJobs` API-only slices — the public headers with their contract blocks and the library target, the node launch seam header `Internal/Jobs/Zenith_JobNodeLaunch.h`, the injection-queue API in that seam, and the `Zenith_SimulationClock` header — once the Stage 1 "ECS, job and Luau lifetime specifications" ticket (deliverable 6) is Done; that ticket is their only gate, because declaration-only slices need none of CORE's `Zenith_Multithreading` implementation tickets. The behavioural `ZenithJobs` slices also wait for CORE's `Zenith_Multithreading` tickets, under the other-candidates rule below. A slice already filed into `Stage 2 — Foundation` moves with `zagent sprint add` and keeps fix version `Z2 Stage 2` ("Board conventions" → "Sprints and fix versions").

<!-- elaboration -->
Other candidates, each pulled forward only if its governing decisions are all settled: the `ZenithVertexFormat` codec (QTangent is the default until D-VTX-TANGENTLESS (open, Stage 3), so the codec does not wait on it) and `Zenith_Multithreading` primitives.

## Stage 2 Foundation

### Stage 2 goal

Build the engine's foundations so that they compile, pass their conformance suites, and run on Windows and Android: platform, jobs, reflection, the three ECS backends, serialization and basic assets, both RHIs, the render graph and shader system, view identity, the in-repo game model, the CPU frame graph, compression, data-defined components, and the on-screen profiler core.

### Stage 2 sprint and fix version

- Sprint: `Stage 2 — Foundation`
- Fix version: `Z2 Stage 2`

### Stage 2 entry criteria

1. `Z2 S1 stage acceptance` is Done and `Stage 1 — Architecture validation` is complete.
2. Every Stage 2 ticket is `ready` or blocked only by a Stage 2 decision ticket.
3. A human starts `Stage 2 — Foundation`.

### Stage 2 deliverables

1. **Reproducible builds and setup** (SHIP): the setup script provisions a clean machine from `Zenith2/ThirdParty/manifest.json`; every C/C++ dependency builds from source in the sanitizer presets.
2. **Platform, Jobs, Reflect** (PLAT, JOBS, ECS): SDL3 platform layer; the work-stealing scheduler on `Zenith_Multithreading`; one reflection system with explicit registration and stable type and property IDs.
3. **All three ECS backends** (ECS; Z2-D-039): `ZenithECS_Custom`, `ZenithECS_Flecs`, `ZenithECS_EnTT` (through the adapter), each exporting its own factory, all passing the shared conformance suite.
4. **Serialization and basic assets** (ASSET, SCENE): versioned binary cooked data, JSON authoring, canonical serialization (stable key order, one ID-keyed child per line).
5. **Both RHIs, the render graph and the shader system** (RHI, RPROD): `ZenithRHI_Vulkan` and `ZenithRHI_D3D12`; the per-frame arena-built render graph with automatic barrier synthesis and transient aliasing with its runtime toggle; `ZenithShaderCompiler` offline, generated parameter structs; PSO keying.
6. **Basic view identity** (RPROD; Z2-D-059): a view ID, per-view resources and independent per-view histories, which shadow views and editor previews need from Stage 3.
7. **The in-repo game model** (MOD): `zenith_add_game`, `game.json` parsing, static-module selection and registration, and backend selection, proven by a minimal game under `Zenith2/Games/` that builds on Windows and Android.
8. **`ZenithGraph`, the CPU frame graph and scheduler** (GRAPH, JOBS): schedule compilation, dynamic sub-jobs, pipelining; and **`ZenithCompression`** with zlib and zstd (COMPRESS).
9. **Runtime type registration and data-defined components** in ZenithReflect and all three ECS backends (ECS); **the authoring-document migration framework** (SCENE, ASSET; Z2-D-048); **`ZenithVertexFormat`** (GEOM); **the virtual filesystem, async I/O and configuration layering** (PLAT); **the memory governor core** (CORE).
10. **The on-screen profiler core** (DEVT; Z2-D-005): the CPU graph timeline, frame-time history, counters and memory, with GPU per-pass timings as soon as the render graph lands.

### Stage 2 gate steps

Each added by a `needs-human` PROC ticket in Stage 2:

<!-- elaboration -->
| Step | Category | Trigger | Budget row (step timeout) |
|---|---|---|---|
| Build -Preset android-arm64 | Zenith2 (base) | every ticket | Z2-B-PROC-010 |
| ApkCheck (16 KB alignment: `zipalign -c -P 16 -v 4`; `llvm-readelf -l` PT_LOAD ≥ 0x4000 for every `.so`) | Zenith2 (base) | every ticket | Z2-B-PROC-023 |
| `Sanitizers -Preset win-asan` | Zenith2Memory | change-triggered | Z2-B-PROC-025 |
| `Sanitizers -Preset linux-tsan` | Zenith2Memory | change-triggered | Z2-B-PROC-026 |

ShaderChecks and EcsConformance are not added here: their lines sit in `Zenith2Render`'s and `Zenith2ECS`'s gates from hand-over (see "Stage 0 category gate lists at hand-over"), and start checking real content as soon as the first shader source and the first ECS backend target land.

### Stage 2 decisions

Ruled in Stage 2: D-SEL-CODEC (open, Stage 2), D-SEL-VIDEO (open, Stage 2), D-SEL-CRASH (open, Stage 2), D-SEL-2DPHYS (open, Stage 2), D-CLASS-SIM (open, Stage 2), and any of D-SEL-NAV (open, Stage 1 or 2), D-SEL-TEXT (open, Stage 1 or 2), D-SEL-UIRT (open, Stage 1 or 2) not ruled in Stage 1.

### Stage 2 budget rows

Allocated: the Stage 2 step timeouts (Z2-B-PROC-010, -023, -025, -026); and, from their first real runs, Z2-B-PROC-024 and -031. First confirmed by measurement: the rows of the systems Stage 2 delivers (JOBS, GRAPH, ECS, RHI, CORE, DEVT rows) in whatever workloads exist.

### Stage 2 acceptance ticket

`Z2 S2 stage acceptance` collects:

- Debug and Release builds on Windows and `android-arm64`; `win-asan`; `linux-tsan`; HWASan on the S24; clang-tidy clean (`Run-Committed.ps1 -Acceptance` runs the manifest's pinned `clang-tidy` over the `win-debug` compile database with `Zenith2/.clang-tidy`, warnings as errors);
- all three ECS conformance suites green, including the determinism replays (1, 2, 8 and all workers; forced compaction; a different chunk capacity) bit-identical per backend;
- header isolation (`HeaderSelfContained`), test separation, and the allocator-instrumentation defect tests caught under ASan and HWASan;
- fuzzing of the cooked-format and JSON parsers; allocation-failure injection for every factory's construction steps;
- every render-graph unit test and render test clean under Vulkan synchronization validation and D3D12 GPU-based validation, in both aliasing modes, with poisoning on;
- the minimal game building and booting on Windows and the S24;
- the 16 KB checks on every `.so` and APK;
- the coverage gate step clean in acceptance mode `-Stage 2`; the Stage 0 and 1 suites re-run green.

### Stage 2 mid-sprint integration check

`Z2 S2 mid-sprint integration check` (`needs-human`): run the minimal game and every workload game that exists at the midpoint on the RTX 3070 desktop and the S24, measure them against `Zenith2/Docs/Budgets.md`, and file regressions as tickets.

### Stage 2 pull-forward

<!-- elaboration -->
Stage 3 tickets that depend on no unresolved Stage 2 decision are scheduled in `Stage 2 — Foundation` with fix version `Z2 Stage 3`. Typical candidates: the editor foundation (authoring identities, command system, undo transactions, document ownership, selection), which depends on SCENE and Reflect only; and the `ZenithPhysics` neutral API header, whose backend tickets wait for the ruling of D-SEL-PHYSX-ANDROID (open, Stage 1).

## Stage 3 Playable authoring slice

### Stage 3 goal

A playable, authorable game: Forward+ on both APIs and the S24 with backend parity in the gates, the rendering-production core, the editor foundation and a minimal editor, physics with both backends, a character controller, animation, audio, UI, the core 2D module, Luau gameplay, save/load, and the multiplayer loopback proof.

### Stage 3 sprint and fix version

- Sprint: `Stage 3 — Playable authoring slice`
- Fix version: `Z2 Stage 3`

### Stage 3 entry criteria

1. `Z2 S2 stage acceptance` is Done and `Stage 2 — Foundation` is complete.
2. A human starts `Stage 3 — Playable authoring slice`.

### Stage 3 deliverables

1. **Forward+ on both APIs and the S24** (RPROD, LIGHT; Z2-D-035), with backend parity in the gates, and **resize and device-loss recovery** (RHI): breadcrumbs and the recover-or-clean-exit policy of Z2-I-RHI-21 (moved to Stage 3 by Z2-D-131).
2. **The RPROD core**:
   - **two-phase HiZ occlusion culling** (Z2-D-060) with the linear RG16F min/max pyramid, in every main, editor and texture view;
   - GPU-driven indirect draw compaction;
   - PSO precaching with the defined missing-PSO fallback (skip in shipping, magenta in development, counters, never a synchronous compile; Z2-D-025);
   - frame pacing;
   - shader-permutation limits (budget row Z2-B-RPROD-024).
3. **The editor foundation, before any authoring tool** (EDIT): stable authoring identities, a command system, undo transactions, document ownership, and selection.
4. **`ZenithPhysics` with both the Jolt and PhysX backends** and the shared conformance suite (PHYS; Z2-D-003), PhysX on Android per the ruling of D-SEL-PHYSX-ANDROID (open, Stage 1).
5. **A minimal editor, input, physics and character controller, animation, audio, UI** (EDIT, PLAT, PHYS, ANIM, AUDIO, UI).
6. **The core 2D module** (TWOD).
7. **Luau gameplay, and save/load** (SCRIPT, GAME, SAVE), including every required transaction and fault-matrix test of `Zenith2/Docs/Scripting.md` → "Required conformance tests" and "Fault-matrix tests".
8. **The multiplayer loopback proof** (NET): a two-process loopback replicating transforms and commands.
<!-- elaboration -->
9. **The focused workload games** with their scripted paths (W-Indoor, W-Outdoor, W-Characters, W-Dense, W-UI2D), so every frame-rate row can be measured: each ticket, blocked by its Stage 1 workload scale ticket, authors the start scene and the replay to `Zenith2/Docs/Research/Workloads/<W>.md` and fills `startScene` and `scriptedPath` of the workload's `*.workload.json`. W-Outdoor is authored at its full scale, so the `WorkloadOutdoor` ticket is `blocks`-linked from the Stage 3 terrain and environment slices (Z2-D-131), each with fix version `Z2 Stage 3`:
   - **terrain geometry** (GEOM; Z2-I-GEOM-19): node selection, morphing without cracks, the terrain cook and the terrain renderer API;
   - **terrain sculpting, foliage placement, atmosphere, day/night and procedural placement** (ENV; Z2-I-ENV-01, -03, -06, -09, -10): the module skeleton, the terrain document with height brushes, the scatter brush, edits, undo and the live preview, the terrain cook, runtime and height queries, the terrain material, placed foliage with tier thinning and the instance cap, procedural placement, the atmosphere document with time of day and the sun and moon slots.
   The reimport, missing-asset and corrupt-asset handling that Stage 3 acceptance evidence requires (ASSET; Z2-I-ASSET-04, -12, -13) is Stage 3 work of the asset pipeline for the same reason (Z2-D-131).
   The glTF 2.0, image and audio importers out of process, with stable product keys, the import conventions and parser limits (CONTENT; Z2-I-CONTENT-01, -02, -04, -06, -07, -15, -22) are Stage 3 work for the same reason (Z2-D-132).

### Stage 3 gate steps

<!-- elaboration -->
| Step | Category | Trigger | Budget row (step timeout) |
|---|---|---|---|
| BackendParity (Vulkan versus D3D12) | Zenith2Render | change-triggered | Z2-B-PROC-027 |

**IntegrationSmoke is not a category gate line**, so it has no category: it is a CI-only step of `Run-Committed.ps1` (every workload and flagship game that has a scripted path boots in the device-free headless configuration and runs 300 frames). The "Stage 3 PROC ticket adding IntegrationSmoke to Run-Committed.ps1" adds `IntegrationSmoke.ps1` to `Run-Committed.ps1`'s CI-only step list; `/tick` never runs it, and its step timeout Z2-B-PROC-028 bounds the CI job, outside the 30-minute warm caps. See "Integration cadence" → "CI integration smoke".

### Stage 3 decisions

Ruled in Stage 3: D-SEL-MOTIONMATCH (open, Stage 3) (motion matching in scope, or blend trees only) and D-VTX-TANGENTLESS (open, Stage 3) (a harness ticket measuring FLIP against the QTangent reference, derivative artefacts and ALU versus bandwidth on the S24 and the RTX 3070, plus a ruling ticket: adopt per quality tier, per material class, or not at all).

### Stage 3 budget rows

Allocated: Z2-B-PROC-027 and -028, and Z2-B-SHIP-015 to -018 by the Stage 3 per-system budget ticket for SHIP, measured with DEVT's `Run-Workload.ps1` ([Z2-N-BUDGET-482]). First confirmed by measurement: every frame-rate, memory-total, hitch and per-system row in the focused workloads, on both platforms, except STREAM's rows Z2-B-STREAM-001 to -006 and Z2-B-CORE-006, first confirmed at Stage 4 in W-Outdoor and W-OpenWorld (the Stage 3 WorkloadOutdoor game does not select the `Streaming` module).

### Stage 3 acceptance ticket

`Z2 S3 stage acceptance` collects:

- BackendParity green: surface-data channels near-exact, final images within FLIP and per-channel thresholds, AMD taking the D32F_S8 path recorded;
- the HiZ render test in which a fully occluded object records zero draws, and the projection and mip-selection unit tests;
- temporal sequences (TAA history, per-view independence of editor viewports);
- resize and device-loss recovery; failure artifacts retained;
- S24 suspend/resume and audio interruption;
- save/load with migration fixtures; interrupted writes; undo/redo; independent play worlds;
- the Luau conformance and fault-matrix suites bit-identical per backend;
- the loopback proof;
- the focused workloads measured per `Zenith2/Docs/Budgets.md` on the reference desktop (both backends) and the S24;
- the coverage gate step clean in acceptance mode `-Stage 3`; all earlier suites re-run green.

### Stage 3 mid-sprint integration check

`Z2 S3 mid-sprint integration check` (`needs-human`): run every workload game's scripted path on the RTX 3070 desktop and the S24, measure against the budgets, and file regressions as tickets.

### Stage 3 pull-forward

<!-- elaboration -->
Stage 4 tickets that depend on no unresolved Stage 3 decision (for example asset-pipeline and streaming foundations, prefabs, localization extraction) are scheduled in `Stage 3 — Playable authoring slice` with fix version `Z2 Stage 4`.

## Stage 4 Production content and worlds

### Stage 4 goal

Production content pipelines and worlds, and the first playable milestones of all three flagship slices, each accepted against its playthrough script so far.

### Stage 4 sprint and fix version

- Sprint: `Stage 4 — Production content and worlds`
- Fix version: `Z2 Stage 4`

### Stage 4 entry criteria

1. `Z2 S3 stage acceptance` is Done and `Stage 3 — Playable authoring slice` is complete.
2. A human starts `Stage 4 — Production content and worlds`.

### Stage 4 deliverables

1. **The full asset pipeline and streaming, including virtual texturing** (ASSET, STREAM, ADVGFX; virtual texturing required here per Z2-D-043).
2. **Prefabs, AI and navigation, and the remaining environment features** (SCENE, AI, ENV): terrain painting and splat blending, foliage wind and the vertex modifier, detail foliage, water, clouds, weather, collision and navigation generation and environment streaming integration (Z2-I-ENV-02, -04, -05, -07, -08, -11, -12, -13); terrain geometry and the Stage 3 environment slices land in Stage 3 (Z2-D-131).
3. **Localization, cinematics, editor workflows** (UI, CINE, EDIT), including complete authoring-document migration coverage (upgrade preview, backup, journaled write, unknown-field preservation).
4. **Content production, world authoring, character production** (CONTENT, WORLD, CHAR), including the supported-DCC matrix, recoverable batch operations, semantic merges of authoring documents, and the timed character workflow.
5. **The 2D production features** (TWOD).
6. **The online-services adapter with its offline implementation** (ONLINE; Z2-D-074).
7. **The first playable milestones** of **OpenWorldSlice** (including the Luau quest, dialogue and inventory systems), **CinematicSlice** and **Game2D**, each accepted against its playthrough script so far.

### Stage 4 gate steps

No new gate step. The integration smoke grows to cover each flagship slice's scripted playthrough as it becomes available.

### Stage 4 decisions

Ruled in Stage 4: D-SEL-FBX (open, Stage 4) (glTF 2.0 only, or FBX through an out-of-process conversion path: FBX2glTF, headless Blender or the Autodesk FBX SDK). CONTENT's harness ticket builds `Zenith_Bench_FbxFidelity` (`Zenith2/Benchmarks/FbxFidelity/`, the converter prototypes and the fidelity comparison over the supported-DCC test assets, with a `--smoke` CTest run), and a `needs-human` ruling ticket runs the full comparison, includes the licence review and appends the ruling to `Zenith2/Docs/DecisionLog.md` (`Zenith2/Docs/Design/CONTENT/DccMatrix.md` → "FBX route").

### Stage 4 budget rows

First confirmed by measurement: streaming, VT, asset, AI, animation, UI, cinematic, audio and editor iteration rows in the flagship workloads that exist; the walkthrough rows (Z2-B-EDIT-007 to -010, Z2-B-CHAR-001, Z2-B-AUDIO-005).

### Stage 4 acceptance ticket

`Z2 S4 stage acceptance` collects:

- content: reimport; clean versus incremental cook equivalence; missing or corrupt content; streaming cancellation (1,000 cancel/reload cycles leaving no entities, references or allocations); unload/reload stress;
- persistence and editor: save migration; interrupted writes; undo/redo; crash recovery; independent play worlds;
- each flagship slice's playthrough so far, run on both platforms, with its feature checklist items for the stage satisfied and its bug bar met;
- the editor iteration and walkthrough rows measured;
- the coverage gate step clean in acceptance mode `-Stage 4`; all earlier suites re-run green.

### Stage 4 mid-sprint integration check

`Z2 S4 mid-sprint integration check` (`needs-human`): run the flagship slices' playthrough scripts, as far as they exist, on the RTX 3070 desktop and the S24, measure them against the budgets, and file regressions as tickets.

### Stage 4 pull-forward

<!-- elaboration -->
Stage 5 tickets that depend on no unresolved decision (advanced lighting and material work that does not wait on D-TBDR (open, Stage 5), VFX features, path tracer foundations) are scheduled in `Stage 4 — Production content and worlds` with fix version `Z2 Stage 5`.

## Stage 5 Rendering and simulation completion

### Stage 5 goal

Complete rendering and simulation: the Forward, Deferred and Visibility-buffer paths with path parity, the RPROD completion, the path tracer and physical-accuracy suite, advanced lighting and materials, VFX, the in-scope specialist simulations, and D-TBDR (open, Stage 5).

### Stage 5 sprint and fix version

- Sprint: `Stage 5 — Rendering and simulation completion`
- Fix version: `Z2 Stage 5`

### Stage 5 entry criteria

1. `Z2 S4 stage acceptance` is Done and `Stage 4 — Production content and worlds` is complete.
2. A human starts `Stage 5 — Rendering and simulation completion`.

### Stage 5 deliverables

1. **The Forward, Deferred and Visibility-buffer paths, with path parity** (RPROD; Deferred and Visibility buffer desktop only).
2. **RPROD completion**: advanced multi-view (many simultaneous captures and probes, stereo-ready paths) and per-platform quality tiers.
3. **The compute path tracer and the physical-accuracy suite** (ADVGFX; Z2-D-037), on WARP and lavapipe, with the per-scene caps of `Zenith2/Docs/Budgets.md` → "Rendering rows".
4. **Advanced lighting and materials, VFX, and the in-scope specialist simulations** (LIGHT, GEOM, VFX, PHYS per the classification of D-CLASS-SIM (open, Stage 2)).
5. **D-TBDR** (open, Stage 5): measured on the S24, including the HiZ mid-frame depth store and reload bandwidth row (Z2-B-RPROD-026); no tile-local merging before the ruling (Z2-D-036).
<!-- elaboration -->
6. **The ECS and shading-path research reports** (RESEARCH): every scenario measured natively and through `Zenith_ECS`, each path on the same workload versions, committed under `Zenith2/Docs/Research/`, conclusions as decision records.

### Stage 5 gate steps

| Step | Category | Trigger | Budget row (step timeout) |
|---|---|---|---|
| PathParity (cross-path consistency) | Zenith2Render | change-triggered | Z2-B-PROC-029 |

### Stage 5 decisions

Ruled in Stage 5: D-TBDR (open, Stage 5), under the acceptance rule of its entry in `Zenith2/Docs/DecisionLog.md` → "Open decisions" (at least 5% lower median main-view GPU frame time on the S24 with non-overlapping 95% confidence intervals and bit-identical parity images), and D-SEL-UPSCALER (open, Stage 5), the third-party temporal upscaler behind `FLUX_UPSCALER_EXTERNAL` (LIGHT; a harness ticket plus a `needs-human` ruling ticket). The D-TBDR (open, Stage 5) revalidation is a Stage 5 ticket and blocks only Stage 5+ rendering tickets. Flagged proposal P1 would add a ruling ticket after the Stage 5 research report (D-SEL-EnTT-SHIPPING); it exists only if the human approves P1 (see "Flagged proposals").

### Stage 5 budget rows

Allocated: Z2-B-PROC-029. First confirmed by measurement: the Deferred and Visibility-buffer pass rows, the path tracer rows, the VT rows, the per-tier view-count rows.

### Stage 5 acceptance ticket

`Z2 S5 stage acceptance` collects:

- PathParity and BackendParity green across every path;
- the physical-accuracy suite: the tracer to its convergence criterion against controlled reference scenes (analytic lights, IBL furnace, area-light shadows, VT-backed terrain materials), within each scene's error tolerance;
- the independent analytical material tests (white furnace, reciprocity, closed-form cases);
- Vulkan sync validation and D3D12 GBV clean for every path;
- the research reports and their decision records;
- the D-TBDR (open, Stage 5) ruling and its S24 measurements;
- all workloads measured per `Zenith2/Docs/Budgets.md`;
- the coverage gate step clean in acceptance mode `-Stage 5`; all earlier suites re-run green.

### Stage 5 mid-sprint integration check

`Z2 S5 mid-sprint integration check` (`needs-human`): run the flagship slices' playthrough scripts on the RTX 3070 desktop and the S24 on every supported path, measure them against the budgets, and file regressions as tickets.

### Stage 5 pull-forward

<!-- elaboration -->
Stage 6 tickets that depend on no unresolved decision (packaging scripts, release manifests, symbol retention, the Steam and Google Play backend scaffolding against the Stage 4 adapter) are scheduled in `Stage 5 — Rendering and simulation completion` with fix version `Z2 Stage 6`.

## Stage 6 Shipping qualification

### Stage 6 goal

Packaged, qualified builds of all three flagship slices on Windows and the S24, with sustained and thermal runs, recovery, updates, patching, migration compatibility, the platform online-service backends, security hardening evidence, and the per-kind compatibility policy demonstrated.

### Stage 6 sprint and fix version

- Sprint: `Stage 6 — Shipping qualification`
- Fix version: `Z2 Stage 6`

### Stage 6 entry criteria

1. `Z2 S5 stage acceptance` is Done and `Stage 5 — Rendering and simulation completion` is complete.
2. A human starts `Stage 6 — Shipping qualification`.

### Stage 6 deliverables

1. **Packaged Windows builds, and S24 builds**: signed APKs (sideload and development), plus **AAB + bundletool split-APK tests + Play Asset Delivery asset packs + installed-update migration** (PACK; Z2-D-027), all 16 KB verified.
2. **The Play base-module limit pinned** into budget row Z2-B-PACK-001 from Google Play's current documentation, by the Stage 6 PACK ticket pinning the Play base-module limit.
3. **Sustained and thermal runs** per `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure" (workload set WS-SUSTAIN), and the physical confirmation of input-to-photon latency.
4. **Recovery, content updates and patching, migration compatibility, release docs** (SHIP, PACK, SAVE), including interrupted content installation (a PAD pack download interrupted, then resumed or rolled back, the game consistent).
5. **The Steam and Google Play online-service backends** (ONLINE; Z2-D-074).
6. **Security hardening evidence** (SEC).
7. **The per-kind compatibility policy demonstrated with migration fixtures** (Z2-D-048): source API deprecation, statically linked modules, save forward migration from every shipped save version of the current major release, cooked re-cook and mismatch refusal, authoring-document migration with preview, backup, journaled write and unknown-field preservation.

### Stage 6 gate steps

No new gate step.

### Stage 6 decisions

No open decision is scheduled for Stage 6.

### Stage 6 budget rows

Allocated: Z2-B-PACK-001. Confirmed by measurement: every row, including the sustained rows (Z2-B-SHIP-006 to -010), the package and install-size rows (Z2-B-PACK-001 to -004), the flagship bug-bar rows (Z2-B-SHIP-019 to -022), and the playthrough duration row (Z2-B-SHIP-023).

### Stage 6 acceptance ticket

`Z2 S6 stage acceptance` collects:

- the packaged builds, AAB, split-APK tests, PAD packs and installed-update migration tests, all 16 KB verified;
- the sustained 30-minute S24 runs of WS-SUSTAIN at or above 45 fps (1-s rolling average), unplugged, 25 °C ambient, from 80% battery;
- S24 memory-pressure and thermal runs; suspend/resume; audio interruption;
- each flagship slice's full playthrough, feature checklist, per-platform budgets, bug bar and every human sign-off item accepted;
- recovery, patching and interrupted-install evidence; migration fixtures per kind;
- the security hardening evidence (fuzzing, parser limits, path traversal, authenticated manifests, no debugger exposure in shipping builds);
- the coverage gate step clean in acceptance mode `-Stage 6`; all earlier suites re-run green.

### Stage 6 mid-sprint integration check

`Z2 S6 mid-sprint integration check` (`needs-human`): run the flagship slices' packaged builds' playthrough scripts on the RTX 3070 desktop and the S24 in ordinary conditions, measure them against the budgets, and file regressions as tickets.

### Stage 6 pull-forward

<!-- elaboration -->
Stage 7 tickets that depend on no unresolved decision and no SDK access (the networked-multiplayer requirement tree built on the Stage 1 foundations and the Stage 3 loopback, XR tickets specified against OpenXR) may be scheduled in `Stage 6 — Shipping qualification` with fix version `Z2 Stage 7`, only when they do not compete with qualification work for the lane.

## Stage 7 Later expansion

### Stage 7 goal

The deferred expansions, each with its full requirement tree ticketed now: networked multiplayer and dedicated servers, RT and virtualized geometry, VR/XR through OpenXR, and consoles gated on SDK access.

### Stage 7 sprint and fix version

- Sprint: `Stage 7 — Later expansion`
- Fix version: `Z2 Stage 7`

### Stage 7 entry criteria

1. `Z2 S6 stage acceptance` is Done and `Stage 6 — Shipping qualification` is complete.
2. A human starts `Stage 7 — Later expansion`.
3. For each console: its `needs-human` SDK-access ticket is Done; every console ticket is `blocks`-linked from its platform's SDK-access ticket.

### Stage 7 deliverables

1. **Networked multiplayer and dedicated servers** (NET; Z2-D-073): transport and session, replication, prediction and reconciliation, interest management, serialization, dedicated server builds, network simulation, diagnostics.
2. **RT and virtualized geometry** (ADVGFX; Z2-D-043).
3. **VR/XR through OpenXR** (SCOPE; Z2-D-072): `VIEW_KIND_STEREO` multi-view rendering with shared culling, XR input actions.
4. **Consoles, gated on SDK access** (SCOPE; Z2-D-071): tickets specified to the level possible without an NDA, each blocked by its platform's SDK-access ticket.

### Stage 7 gate steps

Each expansion that brings new tooling (a console toolchain, an XR runtime, a dedicated-server preset) adds its gate steps through its own `needs-human` PROC ticket in Stage 7.

### Stage 7 decisions

Each expansion's selection decisions (transport library, OpenXR runtime targets, RT API subset) are decision tickets inside Stage 7, split into harness and ruling tickets where they need measurement.

### Stage 7 budget rows

Each expansion allocates its own rows through a Stage 7 producer ticket; a new row needs a decision and bumps `Budgets.md`'s version.

### Stage 7 acceptance ticket

`Z2 S7 stage acceptance` collects, per expansion delivered: its conformance suites, its device or runtime evidence (`needs-human`), the budget rows it adds measured, and the coverage gate step clean in acceptance mode `-Stage 7`, with all earlier suites re-run green.

### Stage 7 mid-sprint integration check

`Z2 S7 mid-sprint integration check` (`needs-human`): run the flagship slices' playthrough scripts on the RTX 3070 desktop and the S24 to confirm the expansions regress nothing, and file regressions as tickets.

### Stage 7 pull-forward

Not applicable: Stage 7 is the last stage.

## Integration cadence

Integration runs throughout development, not only at stage exits.

### CI integration smoke

- Every workload and flagship game boots in the **headless runtime configuration** — device-free: every simulation phase runs, with no Flux, no window, no RHI device and no audio output device (`Zenith2/Docs/Architecture.md` → "Multiplayer foundations", [Z2-N-ARCH-785]) — runs **300 frames** of its scripted playthrough (budget row Z2-B-PROC-030), and must meet its correctness asserts.
- WARP and lavapipe serve the GPU test executables and the render tests, never this smoke: those create an RHI device directly on WARP (D3D12) or lavapipe (Vulkan, through the pinned ICD manifest), a test that needs a swapchain first creates a hidden window through `Zenith_Window` (`SDL_WINDOW_HIDDEN`), and every other GPU test creates no window ([Z2-N-ARCH-793], [Z2-N-REND-044]); the pinned lavapipe build includes its Win32 WSI for the hidden-window case, recorded as a note in `Zenith2/ThirdParty/manifest.json`.
- It is a **CI-only step of `Run-Committed.ps1`, outside every category's `gates` list**, so it has no category and `/tick` never runs it. The Stage 3 PROC ticket that activates it edits `Run-Committed.ps1`'s CI-only step list.
<!-- elaboration -->
- "Change-triggered for engine paths" means it runs in `zenith2-gates.yml` when the pushed change touches `Zenith2/Engine/**`, `Zenith2/Player/**`, `Zenith2/Shaders/**`, `Zenith2/Tools/ZenithCook/**`, `Zenith2/Games/**` (a workload's or slice's scripted path or content) or `Zenith2/ThirdParty/manifest.json`.
- Its step timeout (Z2-B-PROC-028) bounds the CI job; it is outside the 30-minute warm phase caps.
- Its timings are never frame-rate evidence; the smoke host and the WARP and lavapipe GPU-test hosts are correctness hosts only.

### CI gate union

- `zenith2-gates.yml` runs only when the user pushes, and runs the **union** of every Z2 category's active gates, **excluding `Zenith2Acceptance`**, plus `Run-Committed.ps1`'s CI-only step list (IntegrationSmoke is its only member).
- Z2-D-031 and Z2-D-032 ("CI and local runs always execute the same active set") hold for the category gate lines, which are the only lines a local `/tick` runs; the CI-only list is the one explicit, documented addition on the CI side.
- A local `/tick` runs the path-selected subset of the category lines; for any diff, the local set is a subset of the CI set, and a test in `Zenith2/Tools/Gates/Tests` asserts it (the CI-only list does not affect the assertion).
- The Linux job runs only if a `linux-tsan` line is active.

### Mid-sprint integration check

<!-- elaboration -->
- Each stage sprint carries a `needs-human` ticket at its midpoint, titled `Z2 S<N> mid-sprint integration check`.
- It runs the flagship slices' playthrough scripts (before Stage 4, the workloads that exist) on the RTX 3070 desktop and the S24, measures them against `Zenith2/Docs/Budgets.md`, and files regressions as tickets, so drift is caught long before the stage acceptance.
<!-- elaboration -->
- It is due once half of the sprint's story points are Done; it blocks the stage acceptance ticket.
- Its category is `Zenith2`, its fix version the stage's.

### Stage acceptance

- Stage acceptance suites run outside the loop (Z2-D-083): `Run-Committed.ps1 -Acceptance -Stage N` in the main checkout, lanes idle, holding the build slot.
- They are exempt from the 30-minute cap and use the `Zenith2Acceptance` timeout row (Z2-B-PROC-006).

## Acceptance evidence

<!-- elaboration -->
The catalogue every stage acceptance draws from. Each item names when it first becomes part of a stage acceptance; from then on every subsequent stage re-runs it.

### Builds

| Evidence | First required at |
|---|---|
| Debug and Release builds (Windows presets) | Stage 0 |
| Release `android-arm64` build | Stage 2 |
| `win-asan` (MSVC ASan + libFuzzer) | Stage 2 |
| `linux-tsan` (TSan + UBSan, WSL2 host, no GPU libraries) | Stage 2 |
| HWASan on the S24 (separate debuggable build type, `wrap.sh`, `extractNativeLibs=true`; ELF alignment still checked) | Stage 2 |
| clang-tidy clean (run by `Run-Committed.ps1 -Acceptance` over the `win-debug` compile database with `Zenith2/.clang-tidy`, warnings as errors) | Stage 2 |

### Architecture checks

| Evidence | First required at |
|---|---|
| libgraph (encapsulation, undeclared edges, cycles, leakage, no Slang/DXC in runtime links, inbound-surface baseline) and the complexity gate | Stage 0 |
| Header isolation (`HeaderSelfContained`) and test separation (`TestSeparationCheck`) | Stage 0 |
| All three ECS conformance suites | Stage 2 |
| Allocator-instrumentation defect tests (arena overflow, use after arena reset, use after pool free, 1/15/16/17-byte pool allocations) under ASan and HWASan | Stage 2 |
| Backend swap-contract reports (libgraph) for ECS, RHI, physics and compression | Stage 3 |

### Robustness

| Evidence | First required at |
|---|---|
| Fuzzing (parsers, cooked formats, save files, update manifests) | Stage 2 |
| Allocation-failure injection (every construction step of every factory) | Stage 2 |

### Rendering

| Evidence | First required at |
|---|---|
| Vulkan synchronization validation and D3D12 GPU-based validation clean, aliasing on and off, poisoning on | Stage 2 |
| Cross-path consistency: BackendParity | Stage 3 |
| Cross-path consistency: PathParity across all four paths | Stage 5 |
| Physical-accuracy suite (path tracer against controlled reference scenes) and independent analytical material tests | Stage 5 |
| Temporal sequences (TAA, histories, per-view independence) | Stage 3 |
| Resize and device-loss recovery | Stage 3 |
| Failure artifacts retained (images, diffs, frame dumps, breadcrumbs) | Stage 2 |

### S24

| Evidence | First required at |
|---|---|
| Suspend/resume | Stage 3 |
| Audio interruption (audio focus) | Stage 3 |
| Memory pressure (each governor level forced) | Stage 3 |
| Thermal runs (the sustained 30-minute runs) | Stage 6 |

### Content

| Evidence | First required at |
|---|---|
| Reimport (preserving authored overrides) | Stage 3 (GUID and product-key survival: ASSET, CONTENT); Stage 4 (CONTENT's product overrides and generated-prefab entity identity) |
| Clean versus incremental cook equivalence | Stage 3 |
| Missing or corrupt content | Stage 3 |
| Streaming cancellation (1,000 cancel/reload cycles, nothing left behind) | Stage 4 |
| Unload/reload stress | Stage 4 |

### Persistence and editor

| Evidence | First required at |
|---|---|
| Save migration (a fixture per version) | Stage 3 |
| Interrupted writes | Stage 3 |
| Undo/redo | Stage 3 |
| Crash recovery (editor autosave and recovery) | Stage 4 |
| Independent play worlds | Stage 3 |

### Performance

| Evidence | First required at |
|---|---|
| Workloads measured per `Zenith2/Docs/Budgets.md` (both backends on the reference desktop, and the S24) | Stage 3 (focused workloads); Stage 4 (flagship slices so far) |
| Sustained thermal runs, package and install sizes, physical latency confirmation | Stage 6 |

### Backlog

| Evidence | First required at |
|---|---|
| A clean vault scan (`zagent doctor` ok) | Stage 0 |
| Coverage green (the coverage gate step, acceptance mode `-Stage N`) | Stage 0 |
| Dependencies valid (no dangling links, no `blocks` cycles, every pending reference blocked by its producer) | Stage 0 |
| Routing correct (category, epic, sprint, fix version, complexity, risk, points on every ticket) | Stage 0 |
| Existing projects unchanged (`zagent gates` for ZEN and ZM tickets unchanged; the blindness grep) | Stage 0 |

### Evidence validity

- Every evidence record states the tested tree SHA and build configuration, the workload version, the platform and backend, the result and the artifact location, and carries an input fingerprint.
- Acceptance mode recomputes the fingerprint and rejects any mismatch; recency never validates evidence (Z2-D-065).
- Device and visual evidence is `needs-human`.
- The record format is defined in `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure".

## Pull-forward rule

- A ticket's **fix version** is the stage whose acceptance it serves.
- Its **sprint** is where it is scheduled, which may be earlier.
- **A ticket that does not depend on any unresolved decision of the current stage is pulled forward into the current stage's sprint**, while keeping its own (subsequent) fix version.
- It matters most in Stage 1, which is mostly `needs-human` decisions and device probes: pulling the independent Stage 2 foundation tickets (Core containers, `ZenithGraph`, compression, the `ZenithJobs` API-only slices) forward keeps the single Z2 lane busy.
- Pulling forward never changes a ticket's blocks links: a pulled-forward ticket is still claimable only when its blockers are Done and its `Spec-Maturity` is `ready`.
<!-- elaboration -->
- The filer applies the rule at filing time from the dependency graph; any reshuffle after filing is done with `zagent sprint add` / `sprint remove`, never by changing the fix version.
- Because Z2 runs one ticket at a time (one lane per project), a wide dependency graph buys pull-forward flexibility, not concurrency.

## Flagged proposals

Challenges to settled choices, recorded with evidence and a trade-off (Z2-D-022). **Not applied. Status of each: awaiting the human.** Nothing here changes the plan until it is approved.

| # | Settled choice | Concrete concern | Proposal | Trade-off | Status |
|---|---|---|---|---|---|
| P1 | All three ECS backends, including EnTT through an adapter | **Data-defined components** (now required) conflict with EnTT's compile-time-typed storage. The EnTT backend needs a dynamic-storage adapter, so a growing share of what the §5.5 research measures is the adapter rather than EnTT. The cost is also paid in every conformance run | Keep EnTT, but **scope its research claim** to C++-registered components. After the Stage 5 research report, a **ruling ticket** decides whether EnTT stays a supported shipping backend or becomes research-only | Less backend-maintenance burden, versus the original intent of three equal shipping backends | awaiting the human |
| P2 | One lane per project, so Z2 runs one ticket at a time | The Z2 backlog is thousands of tickets. With one lane, calendar throughput is bound by serial ticks, and wide dependency graphs don't help | A generic PROC ticket makes `laneAdd` support **N lanes per project**, with per-lane build directories (already keyed per checkout) and the shared build slot. Z2 would then run 2–3 lanes on independent libraries | Needs protected Vault and lane-tooling changes and more build-slot contention, versus much higher Z2 throughput | awaiting the human |

<!-- informative -->
In P1, "the §5.5 research" is the ECS and shading-path research methodology, now specified in `Zenith2/Docs/Rendering.md` → "Shading path research methodology". If P1 is approved, its ruling ticket is named D-SEL-EnTT-SHIPPING and sits in Stage 5; until then it is cited only as a flagged proposal, never as a decision.
<!-- /informative -->

Further proposals raised while writing the documentation are recorded in `C:/dev/Zenith2-drafts/findings/FLAGGED_PROPOSALS.md` and reach this table only when the human accepts them for the record.

## Board conventions

### The Z2 board

- Board project `Z2`, root `Zenith2/Docs/Board` (registered in `vault.config.json`); the board is this checkout, and every board mutation is a commit.
- Other projects stay unaware of Zenith2 (Z2-D-044); the only deliberate mention on the Zenith side is the root delegation line (Z2-D-030).
- `workflow.md`, `labels.md` and `versions.md` under `Zenith2/Docs/Board/` are generated fresh for Z2 (with Z2's own `project_id` and status IDs) and checked by `zagent doctor`, which verifies parsing, statuses and references but not the graph. The eight stage versions are authored into `versions.md` by A1 with `project_id`, because `version create` rows carry none and are invisible to `version list` (Z2-D-094).
- The `jira.projects` row for Z2 in `metadata.md`, needed by every `--project Z2` read (Z2-D-088), is approved by the human (ruling Q1 (a), 2026-10-16) and made by the human as a hand edit in A8a; A1 adds `Zenith2/.gitattributes` with `Docs/Board/** -text`, so the Z2 board is byte-exact like every other board (Z2-D-108).
- Every Z2 board write names `--project Z2` explicitly: the Vault never maps a category to a project, and `create`, `sprint` and `version` otherwise fall back to `ZEN` (Z2-D-090).

### Epics

One epic per area code, type EPIC, titled `Z2 <CODE> — <Area name>`; EPIC is used for these 41 tickets and nothing else. Epics never get a sprint or fix version and are never claimable. Every non-epic ticket's parent is exactly one area epic, which is the subsystem owner responsible for its maintenance tickets.

| Epic title | Code |
|---|---|
| `Z2 CORE — Core & mathematics` | CORE (includes the memory governor) |
| `Z2 JOBS — Jobs & frame scheduling` | JOBS |
| `Z2 PLAT — Platform & input` | PLAT |
| `Z2 ECS — Reflection & ECS` | ECS |
| `Z2 SCENE — Scenes & prefabs` | SCENE |
| `Z2 STREAM — World streaming` | STREAM |
| `Z2 ASSET — Asset pipeline` | ASSET |
| `Z2 PACK — Packaging & content updates` | PACK |
| `Z2 RHI — RHI & render graph` | RHI |
| `Z2 GEOM — Geometry & materials` | GEOM |
| `Z2 LIGHT — Lighting & image quality` | LIGHT |
| `Z2 ENV — Environment` | ENV |
| `Z2 ANIM — Animation` | ANIM |
| `Z2 PHYS — Physics` | PHYS (includes the extended row) |
| `Z2 AI — AI & navigation` | AI (includes the extended row) |
| `Z2 GAME — Gameplay framework` | GAME (includes the extended row) |
| `Z2 SAVE — Save games & replay` | SAVE (includes the extended row) |
| `Z2 AUDIO — Audio` | AUDIO (includes the extended row) |
| `Z2 UI — UI, localization, accessibility` | UI (includes the extended row) |
| `Z2 VFX — VFX` | VFX |
| `Z2 CINE — Cinematics & media` | CINE (includes the extended row) |
| `Z2 EDIT — Editor & collaboration` | EDIT |
| `Z2 DEVT — Developer tools & quality` | DEVT |
| `Z2 SHIP — Shipping & maintenance` | SHIP |
| `Z2 ADVGFX — Advanced GFX` | ADVGFX |
| `Z2 NET — Multiplayer` | NET |
| `Z2 MOD — Engine modules & projects` | MOD |
| `Z2 GRAPH — Graph core & CPU frame graph` | GRAPH |
| `Z2 COMPRESS — Compression` | COMPRESS |
| `Z2 SCRIPT — Scripting` | SCRIPT |
| `Z2 TWOD — 2D` | TWOD |
| `Z2 CONTENT — Content production` | CONTENT |
| `Z2 WORLD — World authoring` | WORLD |
| `Z2 RPROD — Rendering production` | RPROD |
| `Z2 CHAR — Character production` | CHAR |
| `Z2 ONLINE — Online services` | ONLINE |
| `Z2 SEC — Security & robustness` | SEC |
| `Z2 MAINT — Engine maintenance` | MAINT |
| `Z2 RESEARCH — Research` | RESEARCH |
| `Z2 SCOPE — Platform scope` | SCOPE |
| `Z2 PROC — Process & gates` | PROC |

### Sprints and fix versions

| Stage | Sprint name (exact string) | Sprint slug | Fix version (exact string) |
|---|---|---|---|
| 0 | `Stage 0 — Tooling bring-up` | `z2-stage-0-tooling-bring-up` | `Z2 Stage 0` |
| 1 | `Stage 1 — Architecture validation` | `z2-stage-1-architecture-validation` | `Z2 Stage 1` |
| 2 | `Stage 2 — Foundation` | `z2-stage-2-foundation` | `Z2 Stage 2` |
| 3 | `Stage 3 — Playable authoring slice` | `z2-stage-3-playable-authoring-slice` | `Z2 Stage 3` |
| 4 | `Stage 4 — Production content and worlds` | `z2-stage-4-production-content-and-worlds` | `Z2 Stage 4` |
| 5 | `Stage 5 — Rendering and simulation completion` | `z2-stage-5-rendering-and-simulation-completion` | `Z2 Stage 5` |
| 6 | `Stage 6 — Shipping qualification` | `z2-stage-6-shipping-qualification` | `Z2 Stage 6` |
| 7 | `Stage 7 — Later expansion` | `z2-stage-7-later-expansion` | `Z2 Stage 7` |

- Sprint names carry no leading "Z2": the Vault slug is `z2-` + slug(name), so a "Z2 …" name would slug to `z2-z2-…`.
- The ticket titles `Z2 S<N> stage acceptance` and `Z2 S<N> mid-sprint integration check` are **ticket titles, not sprint names**; no sprint carries a `Z2 S<N>` prefix.

- Sprints are created first by the filer (`zagent sprint create --project Z2`); tickets pass the slug in the table above to `create --sprint`. The filer computes each slug as `z2-` + slug(name) and never looks it up (Z2-D-093), because `sprint create` returns the slug only on first creation and `sprint list` never shows it.
- One ACTIVE sprint per project; every Z2 sprint is PLANNED at hand-over, and a human starts each one.
- A ticket in a PLANNED sprint while another is ACTIVE is skipped by the queue and still claimable by name, so the sprint itself is the deferral.
- Each sprint's acceptance ticket is designated with `zagent sprint gate`, which checks only that the ticket exists. `zagent sprint complete` refuses to complete a sprint whose gate ticket is not Done; that enforcement is a Vault change filed on ZEN (human ruling Q6 (b), 2026-10-16), one of the ZEN tooling tickets that are Done before Stage 0 starts ("Stage 0 entry criteria" item 4); a human completes a sprint only after its acceptance ticket is Done.
- `sprint start` and `sprint complete` are run by a human only.
- A pull-forward decided after a ticket is filed (for example the `ZenithJobs` API-only slices of "Stage 1 pull-forward") moves the ticket with `zagent sprint add` into the earlier sprint; its fix version never changes, and its `blocks` links are unchanged.

### Ticket types

| Type | Used for |
|---|---|
| EPIC | the 41 area epics only; a rollup container, never claimable |
| TASK | every leaf: implementation, API-only, harness, decision ruling, revalidation, budget producer, PROC, integration, acceptance, mid-sprint check and maintenance tickets |
| BUG | a defect, including every budget regression filed under the regression rule |
| SUBTASK | a step of a TASK that cannot land on its own |

**STORY is not used on Z2.** `zagent board status` reports every EPIC or STORY that `Roadmap.md` does not list as drift and exits 1, so leaf work is TASK and `board status` stays a usable check (Z2-D-109).

Every ticket draft also carries a `kind:` front-matter field, one of `implementation`, `api`, `decision`, `harness`, `budget`, `revalidation`, `integration`, `acceptance`, `proc`, `maintenance`; the session coverage checker's rules key on it.

### Labels

<!-- elaboration -->
| Label | Meaning on Z2 |
|---|---|
| `needs-human` | never claimed, by the queue or by a targeted claim. Every PROC ticket that edits a protected path, every ruling ticket, every revalidation ticket, every device step, every mid-sprint check and every stage acceptance ticket carries it |
| `needs-gpu` | gates nothing and changes nothing mechanically on Z2: the ticket is claimed like any other and the gate steps run identically (Z2 has no headless build configuration to switch). It is informational for routing and for the evidence the ticket names, and it needs exactly one `gpu-reason:*` label. A test that runs on WARP or lavapipe inside the `Test` gate step does not make a ticket `needs-gpu`; only a step that needs a physical GPU or device does |
| `gpu-reason:render-capture` | the deliverable is a backend or path parity result, a golden capture or a GPU capture |
| `gpu-reason:visual-measure` | the deliverable is a GPU timing, counter or visual measurement |
| `gpu-reason:requires-graphics-test` | the deliverable includes a test that needs a graphics device |
| `deferred` | skipped by the queue, still taken by a targeted claim: "not yet", not "not ever". `File-Backlog.ps1 --mark-stale` also applies it, paired with `needs-revalidation` |
| `api-change` | the ticket's contract adds or changes a public root header or generated public API output; informational only, because API review is required by the diff through `reviewPaths`, never by a label (Z2-D-049) (elaboration) |
| `blocked-scope` | applied when a ticket was not buildable as filed (a scope gap, a missing file list, a ruling nobody has made), as opposed to a run that failed; it does not count toward `maxConsecutiveBlocked` |
| `integration-conflict` | applied when a lane landing fails on a board-file conflict, `TREE_UNSTABLE`, or a second `STALE_VERDICT` in one firing |
| `needs-revalidation` | applied by `File-Backlog.ps1 --mark-stale` (the Z2 `postFinish` command) to every ticket with a stale cite, together with `deferred`; it drops `Spec-Maturity` to `draft`. The label alone does not stop the queue — nothing reads it — so `deferred` is what removes the ticket from it, and reconcile mode removes both labels when it restores `ready` (Z2-D-102) |
| `human-gate` | not used on Z2 tickets (see "Routing rules"); a ticket carrying it stops at In Review |
| `approved` | the human's release of a `human-gate` ticket in lane mode; it has no use on Z2 while `human-gate` is not used |

Device steps are always `needs-human`. `labels.md` carries a row per label for the UI and for provenance. The `gpu-reason:*` values are a fixed set the Vault validates; `labels.md` carries exactly the three above, and the fourth validated value, `gpu-reason:legacy-scene-reauthor`, is Zenith-specific and not used on Z2.

### Categories

| Category | Paths | Adds | Used by the filer for |
|---|---|---|---|
| `Zenith2` | `Zenith2/**` | the base steps | every Z2 ticket except stage acceptance |
| `Zenith2Render` | ZenithFlux, ZenithRHI*, Shaders, Tools/ZenithShaderCompiler, the Cook mesh/texture/material importers | ShaderChecks (from hand-over), BackendParity (from Stage 3), PathParity (from Stage 5) | never (added through the path union) |
| `Zenith2ECS` | ZenithECS* | EcsConformance (from hand-over): the conformance suite against all three backend targets | never (added through the path union) |
| `Zenith2Memory` | ZenithCore allocators/containers/threading, the job graph, ZenithScript's VM glue | `Test -Label memory` (from hand-over: a re-run of the base Test step filtered to the `memory` label); `Sanitizers -Preset win-asan` and `Sanitizers -Preset linux-tsan` (from Stage 2), change-triggered | never (added through the path union) |
| `Zenith2Acceptance` | none; selected explicitly | the full acceptance suites (`-Stage N`), longer `phaseTimeoutMinutes`, exempt from the 30-minute cap, excluded from CI's union | stage acceptance tickets only |

Every Z2 category carries identical settings except `paths`, `gates` and `phaseTimeoutMinutes` (Z2-D-086), including an identical `measureSteps`, which is validated against the selected union of gate lines rather than against each category's own `gates` (see "Stage 0 category gate lists at hand-over"): `project: Z2`; `buildSystem: "external"` and `measureSteps`; the same `conventions` (naming `Zenith2/CLAUDE.md` and `Zenith2/Docs/WorkerGuide.md`); `suppressLanguageConventions: true`; `decisionLog: Zenith2/Docs/DecisionLog.md`; `decisionIdPrefix: Z2-D-`; `branching: direct`. Every Z2 category has a non-empty `gates` array; every gate line starts `pwsh -NoProfile`. `Zenith2/Tools/Gates/Tests` checks the categories agree on every other setting.

### Ticket body standard

Every ticket body has, in order (Z2-D-068):

1. `## Goal` (scope and exclusions)
2. `## Implementation Contract` (exact repo-root-relative files, exact public declarations in Zenith Hungarian with signatures, tests with file, names and cases including failure scenarios, at most three doc sections cited as path plus exact heading)
3. `## Dependencies` (prerequisites and cross-system dependencies)
4. `## Compatibility` (saved data, cooked data, public API, migration)
5. `## Definition of Done` (a `- [ ]` checkbox list, success and failure scenarios, observable checks)
6. `## Validation` (automated checks, fixtures, diagnostic evidence, and a "Review focus" list)
7. `## Budgets` (row IDs from `Zenith2/Docs/Budgets.md`)
8. `## Ownership` (the owning area epic and subsystem owner)
9. `Requirements:` and `Decisions:` lines (the governing `Z2-D-*` IDs)
10. `Draft-ID:`, `Spec-Maturity:` (`draft` | `decision-pending` | `ready`) and `Cites:` (doc path, heading and normative IDs, each with its content hash at filing time)

Body rules (Z2-D-098, Z2-D-099): the ten sections are `##` headings, and no `##` or `###` appears inside `## Goal` or `## Definition of Done`; no heading is named `Gates` anywhere; DoD items are `- [ ] text` with indented continuations; the Goal or DoD text both cites the governing `Z2-D-NNN` **and** names at least one identifier-shaped noun from that decision's Nouns line in `Zenith2/Docs/DecisionLog.md` (a `Zenith_*` or `Flux_*` type, another PascalCase type, or a `Test_*` name), because the rulings matcher reads only the Goal and DoD and a bare ID citation scores below its threshold today; DoD items avoid "the blocker" and "the blocking/linked/related/parent <ticket>"; tickets that are not `needs-human` avoid human-only phrasing ("human sign-off", "a human must…", "no agent may…", "manual", "visual sign-off"); no protected path appears in the DoD or Contract of a non-PROC ticket; no title or link reason starts with `--`.

There is no `## Gates` section. No body leaves an item undecided without naming its owner: a pending item names its decision or budget-row producer ticket, which blocks it.

### Routing rules

- **Routing on every non-epic ticket**: category, parent epic, sprint slug, fix version, complexity, risk, points and labels. Epics never get a sprint or fix version.
- **Complexity**: `SIMPLE` is mechanical work against a fully specified contract; `MODERATE` is ordinary implementation; `COMPLEX` is algorithmic or concurrency-heavy work.
- **Risk**: `HIGH` where a defect is silent (memory, sync, determinism), or where the change is a destructive migration, a security boundary, a release or packaging path, or a save-format change. `HIGH` escalates the model and makes review required. Otherwise `MEDIUM` or `LOW`.
<!-- elaboration -->
- **Points** size the sprint and are independent of complexity and risk, which pick the model. The scale is 1, 2, 3, 5, 8; a ticket sized above 8 is split along the decomposition rule.
- **A DoD states observable checks**, and avoids phrasing that forces review unnecessarily ("provably", "no new allocations") unless review is genuinely wanted; it avoids "human sign-off" phrasing and board references such as "the decision ticket", naming governing rulings by `Z2-D-*` ID instead.
- **Sizing for one dispatch** (guidance): typically one library, about 8 files, a diff of roughly 300–500 lines; a coherent atomic change may exceed this and the ticket states why. Tests land in the same ticket as the code they test.
- **Decomposition**: every substantive capability is split into bounded tickets per applicable facet: runtime behaviour; authoring and cooking; diagnostics; failure handling; integration evidence.
- **Decisions needing measurement** are split into an ordinary harness ticket and a `needs-human` ruling ticket blocked by it (Z2-D-084); dependents are blocked by the ruling ticket.
- **`human-gate` is not used**: the loop would run such a ticket and park it at In Review, but reference-hardware measurement needs a human to run it anyway, so it is modelled as the harness-plus-ruling split instead.
- **Protected work is explicit**: any ticket whose work edits a protected path is a PROC `needs-human` ticket naming the file and the change in its Implementation Contract; ordinary tickets may run protected checks and name them by step ("the coverage gate step"), never by path.
- **Blocks links** are minimal and precise, each with `--reason`. Filer-owned links carry reasons starting `backlog:`.
- **Interfaces land first** as small API-only tickets, so their dependents become claimable early; ticket order within a sprint follows the dependency graph and the critical path, prioritising tickets that unblock the most downstream work.

### Readiness and revalidation

- A downstream ticket is `blocks`-linked from the decision tickets and budget-row producer tickets that govern it, and from a spec-revalidation ticket scoped to (area, stage, decision set).
- A revalidation ticket for stage N depends only on decisions from stages ≤ N; it is `needs-human` and itself blocked by the governing decisions.
- After any Z2 ticket finishes Done, the tick orchestrator runs the Z2 `postFinish` command, which applies `needs-revalidation` and `deferred` to every ticket with a stale cite and drops it to `draft`.
- `deferred` is what keeps such a ticket out of the queue; `needs-revalidation` records why. Neither the label nor `Spec-Maturity` affects claiming on its own (Z2-D-102).
- The filer's reconcile mode updates the bodies, restores `ready`, and removes both labels; only then does the ticket return to the queue.
- The pairing stands until a Vault PROC ticket adds `needs-revalidation` to the claim predicates (Z2-D-102).

### Living docs

- A ticket that changes a contract, API or design edits its `Zenith2/Docs/Design/<AREA>.md` section in the same diff, and its DoD says so (Z2-D-085, for the same-diff rule).
- The docs are unprotected, apart from `Zenith2/CLAUDE.md`.
- A ruling is appended to `Zenith2/Docs/DecisionLog.md` in the ruling ticket's own diff with the next explicit `Z2-D-NNN`, as `Zenith2/Docs/DecisionLog.md` → "How this log is written" describes (Z2-D-097). `zagent decide` writes only a knowledge row in `metadata.md` and never the log; `zagent decide --ID Z2-D-NNN` is an optional mirror.

### Filing and the three-commit landing procedure

The whole backlog is drafted, signed off and filed **before** the scaffold ticket runs (Z2-D-033), so the coverage gate step is green from its first run. Filing lands every ticket on `master` in one commit (Z2-D-069). Exactly three commits land on `master`, each with the human's approval:

1. **A8a — the docs and config commit**, before filing: `Zenith2/**` except `Docs/Board/tickets`, plus `vault.config.json`, `zagent.project.json` and the one `metadata.md` `jira.projects` row, which the human approved (ruling Q1 (a), 2026-10-16) and makes as a human edit (Z2-D-088). A normal commit, made while the lanes are idle, so in-flight lane tickets regate once. The seed branch is cut from it.
2. **A8b — the board seed commit**: `board: seed Z2 backlog`, with a `Vault-Operation: z2-seed-<date>` trailer, containing every ticket and nothing but `Zenith2/Docs/Board/**`.
3. **A8c — the coverage report commit**: `Zenith2/Docs/Coverage.md`, generated after the seed because it needs the real ticket keys; made **before the lanes resume**, because it is not board-only.

The filing procedure between A8a and A8b:

1. Keep every lane idle for the whole filing run (no lane is fired; the other /tick session is told first). While main is not on `master`, a lane `integrate` returns `MAIN_NOT_ON_BASE`.
2. In the main checkout, with a clean tree, `git switch -c z2-board-seed`, branched from `master` after A8a. (A worktree does not work: its vault is redirected to the main checkout.)
3. Run the session filer through the normal zagent: sprints and versions first, then tickets in topological order with `--blocked-by … --reason`, `--sprint`, `--fix-version`, `--parent`, `--label` at create time, bodies passed with `create --file <path>` after checking the file exists and its SHA-256 matches the draft (a missing path would be used as the body text, Z2-D-111); every write names `--project Z2`; every argument, including titles such as `Z2 UI — UI, localization, accessibility`, is passed as its own argv element, never through a shell string, so commas and dashes in titles are safe (Vault titles permit commas); tickets whose blockers need distinct reasons are created without blockers and linked with `zagent link … blocks … --reason` right after; a second pass does only `sprint gate`.
4. Verify on the seed branch: `zagent doctor` ok, the session coverage checker clean, `git diff master...z2-board-seed --name-only` touching only `Zenith2/Docs/Board/**`.
5. `git switch master`, `git merge --squash z2-board-seed`, commit A8b.
6. Generate and commit A8c, delete the seed branch, and resume the lanes.

Failure recovery: filing is resumable (keyed on Draft-ID; a re-run skips existing tickets); a half-filed board lives only on `z2-board-seed`; to abandon, first switch back to `z2-board-seed` and run `zagent check <KEY>` (or any write) so any Vault journal recovery completes — `zagent doctor` never takes the lock and never recovers (Z2-D-095) — and only then switch to `master` and delete the branch. A dry run of 50 tickets on a scratch branch comes first, to measure operations per second and confirm the squash procedure end to end. Before A6 the human archives the complete Vault journals once, by hand, following the exact procedure the orchestrator supplies (ruling Q2 (a), 2026-10-16), and the dry run measures the filing rate after that archival (Z2-D-110).

### Board state at hand-over

- Every sprint (Stages 0 to 7) is PLANNED; none is started.
- Stage 0 is a closed DAG.
- Every ticket after Stage 0 is filed in full detail and blocked by its governing decisions, budget-row producers and revalidation tickets.
- `zagent queue --project Z2` lists the To Do rows, every one `claimable:false` with a `blockedBy` or out-of-stage reason; `contractValid` is false on every `needs-human`, blocked and epic ticket.
- `create` validates the link graph, never the ticket contract; the session coverage checker owns the contract checks before filing (Z2-D-091).
- `Zenith2/Docs/Coverage.md` holds requirement coverage only (a pure function of the filed backlog); implemented coverage is generated into the untracked `Zenith2/out/coverage/implemented.md`.

### Starting Stage 0

A human runs `zagent sprint start` for `Stage 0 — Tooling bring-up`, then works the Stage 0 PROC tickets in a human-driven session, starting with the global guardrail additions. The scaffold ticket is the first ticket `/tick` runs, in the Z2 lane, once the external mode and the base gate steps exist.
