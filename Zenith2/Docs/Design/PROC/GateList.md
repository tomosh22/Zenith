# PROC design — the gate list, the ticket map and the hand-off ledger

Companion to `Zenith2/Docs/Design/PROC.md`. This file states every Z2 gate line as it grows stage by stage with its exact command line, the stage that activates it and the `needs-human` PROC ticket that adds it; the exact category JSON each PROC ticket writes; the PROC ticket map with Draft-IDs; and the ledger answering every PROC hand-off any area recorded.

## Scope of this file

| Section | Content |
|---|---|
| "Gate list by stage" | the line tables per category and stage, with measure or verify, budget row and activating ticket |
| "Category edits by ticket" | the verbatim JSON members each PROC ticket changes in `zagent.project.json` |
| "PROC ticket map" | every PROC ticket: Draft-ID, title, stage, sprint, `blocked_by`, files, requirements |
| "Hand-off ledger" | every row of every area's PROC hand-off list, and the ticket or ruling that answers it |

## Gate list by stage

### The line form

Every line is exactly `pwsh -NoProfile -File Zenith2\Tools\Gates\<Step>.ps1` followed by its parameters, with single spaces and backslash separators, because the union de-duplicates lines by exact string (`routing.mjs` `effectiveGates`) and `Test-HarnessParity.ps1` checks the `pwsh -NoProfile` prefix. A gate line that differs from the table by one character is a different line.

### Zenith2 base category

| # | Exact line | Measure or verify | Active from | Added by | Step timeout row |
|---|---|---|---|---|---|
| 1 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Configure.ps1` | measure | hand-over (A1) | A1; script by Z2T-PROC-008 | Z2-B-PROC-007 |
| 2 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Build.ps1 -Preset win-debug` | measure | hand-over | A1; script by Z2T-PROC-008 | Z2-B-PROC-008 |
| 3 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Build.ps1 -Preset win-release` | measure | hand-over | A1; script by Z2T-PROC-008 | Z2-B-PROC-009 |
| 4 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Build.ps1 -Preset win-shipping-test` | measure | Stage 0 | Z2T-PROC-008 | Z2-B-PROC-009 (the Release-class build row covers both Release-shaped trees; recorded as such by the Stage 0 acceptance allocation) |
| 5 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Build.ps1 -Preset android-arm64` | measure | Stage 2 | Z2T-PROC-040 | Z2-B-PROC-010 |
| 6 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Test.ps1` | verify | hand-over | A1; script by Z2T-PROC-008 | Z2-B-PROC-011 |
| 7 | `pwsh -NoProfile -File Zenith2\Tools\Gates\SourceLayout.ps1` | verify | hand-over | A1; script by Z2T-PROC-008 | Z2-B-PROC-012 |
| 8 | `pwsh -NoProfile -File Zenith2\Tools\Gates\CompileCoverage.ps1` | verify | hand-over | A1; script by Z2T-PROC-008 | Z2-B-PROC-013 |
| 9 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Libgraph.ps1` | verify | hand-over | A1; script by Z2T-PROC-010 | Z2-B-PROC-014 |
| 10 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Complexity.ps1` | verify | hand-over | A1; script by Z2T-PROC-010 | Z2-B-PROC-015 |
| 11 | `pwsh -NoProfile -File Zenith2\Tools\Gates\ForbiddenCheck.ps1` | verify | hand-over | A1; script by Z2T-PROC-009 | Z2-B-PROC-016 |
| 12 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Format.ps1` | verify | hand-over | A1; script by Z2T-PROC-011 | Z2-B-PROC-017 |
| 13 | `pwsh -NoProfile -File Zenith2\Tools\Gates\HeaderSelfContained.ps1` | verify | hand-over | A1; script by Z2T-PROC-011 | Z2-B-PROC-018 |
| 14 | `pwsh -NoProfile -File Zenith2\Tools\Gates\ApiContractCheck.ps1` | verify | hand-over | A1; script by Z2T-PROC-011 | Z2-B-PROC-019 |
| 15 | `pwsh -NoProfile -File Zenith2\Tools\Gates\TestSeparationCheck.ps1` | verify | hand-over | A1; script by Z2T-PROC-010 | Z2-B-PROC-020 |
| 16 | `pwsh -NoProfile -File Zenith2\Tools\Gates\ApkCheck.ps1` | verify | Stage 2 | Z2T-PROC-040 | Z2-B-PROC-023 |
| 17 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Coverage.ps1` | verify | hand-over | A1; script by Z2T-PROC-013 | Z2-B-PROC-022 |
| 18 | `pwsh -NoProfile -File Zenith2\Tools\Gates\ReviewSignals.ps1` | verify | hand-over | A1; script by Z2T-PROC-011 | Z2-B-PROC-021 |
| 19 | `pwsh -NoProfile -File Zenith2\Tools\Gates\SecurityConformance.ps1` | verify | Stage 2 | Z2T-PROC-044 (SEC-P1) | the new SecurityConformance row (X415) |

The array order in the committed file is rows 1 to 16, then 19, then 17 and 18 (the inactive rows absent), so `Coverage` and `ReviewSignals` run last: the coverage report reflects every earlier step's outputs, and the report-only step never hides a failing hard gate behind it.

### Add-on categories

| Category | # | Exact line | Active from | Added by | Step timeout row |
|---|---|---|---|---|---|
| `Zenith2Render` | 1 | `pwsh -NoProfile -File Zenith2\Tools\Gates\ShaderChecks.ps1` | hand-over | A1; script by Z2T-PROC-011 | Z2-B-PROC-024 |
| `Zenith2Render` | 2 | `pwsh -NoProfile -File Zenith2\Tools\Gates\BackendParity.ps1` | Stage 3 | Z2T-PROC-050 | Z2-B-PROC-027 |
| `Zenith2Render` | 3 | `pwsh -NoProfile -File Zenith2\Tools\Gates\PathParity.ps1` | Stage 5 | Z2T-PROC-070 | Z2-B-PROC-029 |
| `Zenith2ECS` | 1 | `pwsh -NoProfile -File Zenith2\Tools\Gates\EcsConformance.ps1` | hand-over | A1; script by Z2T-PROC-011 | Z2-B-PROC-031 |
| `Zenith2Memory` | 1 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Test.ps1 -Label memory` | hand-over | A1; script by Z2T-PROC-008 | Z2-B-PROC-011 |
| `Zenith2Memory` | 2 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Sanitizers.ps1 -Preset win-asan` | Stage 2 | Z2T-PROC-041 | Z2-B-PROC-025 |
| `Zenith2Memory` | 3 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Sanitizers.ps1 -Preset linux-tsan` | Stage 2 | Z2T-PROC-041 | Z2-B-PROC-026 |
| `Zenith2Acceptance` | 1–8 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Run-Committed.ps1 -Acceptance -Stage <N>` for N = 0 … 7 | hand-over | A1; script by Z2T-PROC-012 | Z2-B-PROC-006 |

### The measureSteps array by stage

Every Z2 category carries this identical array (Z2-D-086), validated against the selected union when the gate list is selected (`Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"):

| Stage | `measureSteps` |
|---|---|
| hand-over (A1) | lines 1, 2, 3 of the base table |
| from Z2T-PROC-008 (Stage 0) | lines 1, 2, 3, 4 |
| from Z2T-PROC-040 (Stage 2) | lines 1, 2, 3, 4, 5 |

### The Run-Committed CI-only list

| Stage | CI-only lines (`Run-Committed.ps1`'s `$CiOnlyLines`, never in a category) | Added by | Row |
|---|---|---|---|
| Stage 0 | none (the list exists, empty) | Z2T-PROC-012 | — |
| from Stage 3 | `pwsh -NoProfile -File Zenith2\Tools\Gates\IntegrationSmoke.ps1` | Z2T-PROC-051 | Z2-B-PROC-028 |

### Stage 7 lines

Each Stage 7 expansion that brings tooling adds its lines through its own PROC ticket (`Zenith2/Docs/Roadmap.md` → "Stage 7 gate steps"). The lines are fixed now, so the tickets are fully specified:

| Expansion | Line | Category | Ticket | Blocked by |
|---|---|---|---|---|
| dedicated server builds (NET) | `pwsh -NoProfile -File Zenith2\Tools\Gates\Build.ps1 -Preset linux-server` (a `measureSteps` line) and `pwsh -NoProfile -File Zenith2\Tools\Gates\Test.ps1 -Preset linux-server` | `Zenith2` (paths `Zenith2/**`, so every diff) | Z2T-PROC-090 | the NET ticket that adds the `linux-server` configure and build presets (its Draft-ID is named at NET's A5, whose tickets are not drafted yet) |
| VR/XR through OpenXR (SCOPE) | `pwsh -NoProfile -File Zenith2\Tools\Gates\Test.ps1 -Label z2-xr` (ZenithXR's tests against SCOPE's scripted OpenXR runtime, `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Gate steps per test group"; no null runtime of the loader exists, row SP10 (b)) | `Zenith2Render` | Z2T-PROC-091 | the SCOPE ticket that pins the OpenXR loader, adds the scripted runtime and the `z2-xr` label (its Draft-ID is named at SCOPE's A5 and written into this row then) |
| each console (SCOPE) | `pwsh -NoProfile -File Zenith2\Tools\Gates\Build.ps1 -Preset ps5`, `-Preset xbox-series`, `-Preset switch2` (row SP10 (c)) | none on the main board: the line runs only in the overlay's own checkout, through that overlay's own project configuration (row SP17 (b)) | Z2T-PROC-092a (`ps5`), -092b (`xbox-series`), -092c (`switch2`) | that platform's `needs-human` SDK-access ticket and its SCOPE preset ticket (Draft-IDs named at SCOPE's A5) |
| denied read paths (SCOPE, pending Q98) | none: with Q98 ruled (a), a `needs-human` PROC edit adds `Zenith2/Platforms/**` to the loop's denied read paths (row SP17 (d)); with (b), nothing is filed | — | filed only on Q98 (a) | the Q98 ruling |

`Build.ps1` and `Test.ps1` accept the new preset names through the preset table of their own scripts, which each of these tickets extends in the same diff. The console presets come from each overlay's own `Zenith2/Platforms/<Platform>/CMakePresets.json`, included from one developer-local `Zenith2/CMakeUserPresets.json`. `Build.ps1 -Preset <console-preset>` exits 3 (`GATE_EXIT_ENVIRONMENT`) with `Build: environment: overlay Zenith2/Platforms/<Platform>/ absent; run the overlay setup` when `Zenith2/Platforms/<Platform>/CMakeLists.txt` is missing, proven by `Test_Build_ConsolePresetAbsentOverlayExits3` (SP17 (a), (c)). No main-board category carries a console line, because one would fail every worker ticket on a host without the overlay.

## Category edits by ticket

Every edit below is a targeted change of `zagent.project.json` (protected) made by the named `needs-human` ticket with every lane idle (every lane reads the main checkout's configuration live). The file keeps its CRLF working-tree endings and two-space indentation; the human edits only the members shown and runs `pwsh -NoProfile -File Tools/zagent/Test-ZagentClient.ps1`, `pwsh -NoProfile -File Tools/zagent/Test-HarnessParity.ps1` and `pwsh -NoProfile -File Zenith2/Tools/Gates/Tests/Test-CategoryAgreement.ps1` (the last once it exists) afterwards.

### Z2T-PROC-008 Stage 0 category edit

In each of the five Z2 categories:

```json
"measureSteps": [
  "pwsh -NoProfile -File Zenith2\\Tools\\Gates\\Configure.ps1",
  "pwsh -NoProfile -File Zenith2\\Tools\\Gates\\Build.ps1 -Preset win-debug",
  "pwsh -NoProfile -File Zenith2\\Tools\\Gates\\Build.ps1 -Preset win-release",
  "pwsh -NoProfile -File Zenith2\\Tools\\Gates\\Build.ps1 -Preset win-shipping-test"
],
"coldPhaseTimeoutMinutes": 120,
```

In `Zenith2` alone, the `gates` array becomes base rows 1–4 and 6–15, 17, 18 in table order (the inactive rows 5, 16 and 19 absent). Each category's `$comment` gains the line `"coldPhaseTimeoutMinutes: PROVISIONAL placeholder for budget row Z2-B-PROC-005 (pending), replaced by the Stage 0 acceptance allocation."`.

The same ticket edits `Zenith2/CLAUDE.md` (protected since Z2T-PROC-001):
- The "Gate steps" line names `Build -Preset win-debug|win-release|win-shipping-test|android-arm64`.
- The layout line `Tools/{…}` gains `Authoring`, `EditorCore`, `Maintenance` and `Security`, when the orchestrator's pre-A8a edit has not already added them (EDIT-P1, row X387, SEC's `Tools/Security/`).

No other line of that file changes.

### Z2T-PROC-016 PROC-API edit

In each of the five Z2 categories, `reviewPaths` becomes:

```json
"reviewPaths": [
  "Zenith2/Engine/*/*.h",
  "Zenith2/Engine/*/*.inl",
  "Zenith2/Shaders/**/*.slang",
  "Zenith2/Tools/Gates/libgraph_baseline.json",
  "Zenith2/ThirdParty/licence_policy.json",
  "Zenith2/Tools/Maintenance/*.json",
  "Zenith2/Tools/Security/update_keys.json",
  "Zenith2/Tools/Security/parser_inventory.json",
  "Zenith2/Tools/Security/path_matrix.json",
  "Zenith2/Tools/Security/android_permissions.json",
  "Zenith2/Tools/Backlog/**"
],
```

The last four globs come from the late hand-offs:
- `Tools/Maintenance/*.json`: MT3 (c), X386.
- The four `Tools/Security` files: SEC-P8 (the data the protected scans of `Zenith2/Tools/Gates/Security/` read).
- `Tools/Backlog/**`: review finding 27. The filer runs after every Done through `postFinish` with board-write authority from the main checkout but is unprotected, so every change to it gets an independent, revision-bound review.

A glob naming a file that does not exist yet selects nothing until the file lands.

### Z2T-PROC-017 postFinish edit

In each of the five Z2 categories:

```json
"postFinish": [
  "pwsh -NoProfile -File Zenith2\\Tools\\Backlog\\File-Backlog.ps1 --mark-stale",
  "pwsh -NoProfile -File Zenith2\\Tools\\Backlog\\File-Backlog.ps1 --check-headroom"
],
```

### Z2T-PROC-019 reviewerConventions edit

In each of the five Z2 categories, `reviewerConventions` is exactly:

```json
"reviewerConventions": "You review a Zenith2 diff. Zenith2/** is governed by Zenith2/CLAUDE.md; the contract is Zenith2/Docs/Contracts.md. Challenge the behaviour independently of the author's own tests: for each changed function, name an input or ordering the tests do not exercise and say whether the code handles it. Check every item of the ticket's Review focus list in its ## Validation section and report each one as checked-clean, finding, or not checked. Report as findings: a public header without its contract block, an STL type in a public header, a type whose contract says Thread affinity: IMMUTABLE with a data member that could be const and is not, unless its Thread affinity line says why, a noexcept or [[nodiscard]] whose ZENITH-ALLOW reason names no language or standard-library mechanism that requires it, a [[noreturn]] on a function that can return, a writable global, a test that pins a count, a new shortcut without a refactor ticket, a contract or design change without the same-diff doc edit, and a narrowing static_cast. Name the categories you did not check."
```

### Z2T-PROC-044 SecurityConformance edit

In `Zenith2`, `gates` gains base row 19 at its position (after row 16, before row 17).

### Z2T-PROC-024 Stage 0 acceptance edit

In each of the five Z2 categories, `coldPhaseTimeoutMinutes` becomes the adopted value of Z2-B-PROC-005; in `Zenith2Acceptance`, `phaseTimeoutMinutes` becomes the adopted value of Z2-B-PROC-006; each `$comment`'s placeholder sentence is replaced by `"coldPhaseTimeoutMinutes: the adopted value of budget row Z2-B-PROC-005."` (and the acceptance's by the same sentence naming Z2-B-PROC-006).

### Z2T-PROC-040 Android edit

In each of the five Z2 categories, `measureSteps` gains `"pwsh -NoProfile -File Zenith2\\Tools\\Gates\\Build.ps1 -Preset android-arm64"` as its fifth element; in `Zenith2`, `gates` gains base rows 5 and 16 at their table positions.

### Z2T-PROC-041 Sanitizer edit

`Zenith2Memory` becomes:

```json
"paths": [ "<every glob of sanitizer_paths.json's win-asan and linux-tsan lists, de-duplicated, byte-ordered>" ],
"gates": [
  "pwsh -NoProfile -File Zenith2\\Tools\\Gates\\Test.ps1 -Label memory",
  "pwsh -NoProfile -File Zenith2\\Tools\\Gates\\Sanitizers.ps1 -Preset win-asan",
  "pwsh -NoProfile -File Zenith2\\Tools\\Gates\\Sanitizers.ps1 -Preset linux-tsan"
],
```

`Test-SanitizerPathsMatchCategory` (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizer path selection") fails when the category's `paths` differ from that union, so the two never drift.

### Z2T-PROC-050 and Z2T-PROC-070 Render edits

`Zenith2Render`'s `gates` gains `"pwsh -NoProfile -File Zenith2\\Tools\\Gates\\BackendParity.ps1"` (Z2T-PROC-050) and then `"pwsh -NoProfile -File Zenith2\\Tools\\Gates\\PathParity.ps1"` (Z2T-PROC-070), in that order after `ShaderChecks`. `Zenith2Render`'s `paths` are unchanged: ENV-P2's and RHI-P7's paths are already inside `Zenith2/Engine/ZenithFlux/**`, `Zenith2/Engine/ZenithRHI*/**` and `Zenith2/Shaders/**`.

### Vault-level edits

| Ticket | File | Exact change |
|---|---|---|
| Z2T-PROC-001 | `vault.config.json` | append `"Zenith2/Tools/Gates/**"`, `"Zenith2/CLAUDE.md"`, `"Zenith2/Docs/Research/**/runs/**"` and `"Zenith2/Docs/Research/Machines/**"`, in that order, to the end of both `policy.board.guardrails.protectedPaths` and `policy.resolved.guardrails.protectedPaths` (the last two are RESEARCH's P-RS6); nothing else in the file changes |
| Z2T-PROC-001 | `zagent.project.json` | append the same four strings to the end of the top-level `protectedPaths` (documentation only, kept a strict subset of the board's list) |
| Z2T-PROC-006 | `zagent.project.json` | the top-level `conventions` string gains the suffix ` Zenith2/** is governed by Zenith2/CLAUDE.md.` (one leading space) |
| Z2T-PROC-021 | `zagent.project.json` | `lanes` gains the `provision` object of `Zenith2/Docs/Design/PROC/Harness.md` → "Lane provisioning hook" |
| — | `zagent.project.json` | no `lanes.maxPerProject` edit is filed: it belongs to flagged proposal P2, which is not applied (Z2-D-130); `Zenith2/Docs/Design/PROC/Harness.md` → "N lanes per project" keeps the specification as an assumption block |

## PROC ticket map

Every ticket below has category `Zenith2` (except the acceptance tickets: `Zenith2Acceptance`), parent epic `Z2 PROC — Process & gates`, type TASK, label `needs-human`, kind `proc` (`acceptance` for acceptance tickets, `integration` for mid-sprint checks), and fix version equal to its sprint's stage. A ticket that allocates budget rows carries the rows' producer name ([Z2-N-BUDGET-462]) as its title prefix: the activation tickets' titles below are the producer names of `Zenith2/Docs/Budgets.md` → "Producer tickets", and the acceptance titles are the producer names X383 and its amendment assign. "Blocks" lists the tickets outside PROC that wait for it; every Stage 0 ticket after Z2T-PROC-001 carries `blocked_by: Z2T-PROC-001` with the reason `backlog: guardrails first: the gate directory and Zenith2/CLAUDE.md are protected before anything is written there`, in addition to the blockers listed. No ticket carries a link to or from a ticket of another project (Z2-D-112); a dependency on another board's work is a precondition check in the Definition of Done.

### Stage 0 tickets

| Draft-ID | Title | Further blocked_by | Files (summary) | Requirements | Blocks |
|---|---|---|---|---|---|
| Z2T-PROC-001 | Global guardrail additions and three-harness regeneration | — (its first DoD item is the read-only precondition check of `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "After the seed" item 3, the one-liner over ZEN-114 to ZEN-119 that exits 1 unless every probe exits 0 with `status` `Done`; a non-zero exit, an `error` member or a missing `status` stops the ticket, Q5 (a)) | `vault.config.json` and `zagent.project.json` (the four guardrail strings, with RESEARCH's P-RS6 globs), `Tools/zagent/Test-HarnessParity.ps1`, every generated harness file | Z2-R-PROC-001 to -005, -132, -158 | every other Stage 0 PROC ticket |
| Z2T-PROC-002 | Vault pass-through of the external category keys | — | `Tools/Vault/cli/legacy-pure/routing.mjs`, `compat.mjs`, `read.js`, `spec/command-matrix.md`, tests | Z2-R-PROC-017 to -020 | Z2T-PROC-003, -016, -017, -019 |
| Z2T-PROC-003 | tick_gates external-build mode, phase caps and the mixed-union rule | Z2T-PROC-002 | `Tools/tick_gates.ps1`, `Tools/Test_TickGates.ps1`, `TICK.md` exit-ladder text | Z2-R-PROC-006 to -016 | Z2T-MOD-056 |
| Z2T-PROC-004 | Analyzer include_roots, max-file-loc, exceptions and --dump-entities | — | `Tools/analyze_code_complexity.py`, `Tools/complexity_profiles.json`, `Tools/tests/test_complexity.py` | Z2-R-PROC-021, -022, -165 | Z2T-PROC-010; MAINT's T7 (its measurement harness waits for `--dump-entities`) |
| Z2T-PROC-005 | Scanner audit: generic fixes before any Zenith2 source lands | — | the scanner files of `Zenith2/Docs/Design/PROC/Harness.md` → "Scanner audit" | Z2-R-PROC-023, -024 | Z2T-MOD-056 |
| Z2T-PROC-006 | Root delegation line | — | root `CLAUDE.md`, `zagent.project.json` `conventions`, `Tools/zagent/Test-ZagentClient.ps1` (the new assertion `Test_Zenith2MentionsOutsideZenith2` with its three-file allowlist) | Z2-R-PROC-025 | — |
| Z2T-PROC-007 | libgraph CMake File API front end and gate mode | — | `Tools/libgraph/cmake_fileapi.py`, `model.py`, `libgraph.py`, `gate.py`, tests | Z2-R-PROC-026, -027, -032 | Z2T-PROC-010 |
| Z2T-PROC-008 | Shared gate module and build steps with the Stage 0 category edit | Z2T-PROC-018; precondition (first DoD item): `Zenith2/Docs/Budgets.md` → "Row index" holds the rows "Step timeout Build win-shipping" and "Step timeout SecurityConformance" (X440 (b), X415, applied by the core-doc batch before A8a), and `step_timeouts.json` contains no `NEW-ROW-` | `Zenith2/Tools/Gates/Common/*`, `Configure.ps1`, `Build.ps1`, `Test.ps1`, `Sweep.ps1`, `SourceLayout.ps1`, `CompileCoverage.ps1`, `step_timeouts.json` (with `warmPresets` and the acceptance member), `Tests/Test-StepTimeouts.ps1`, their tests; `zagent.project.json`; `Zenith2/CLAUDE.md` gate-step and layout lines | Z2-R-PROC-033 to -041, -064, -065, -133, -154, -156, -169, -171, -172 | Z2T-PROC-009 to -013, Z2T-MOD-056 |
| Z2T-PROC-009 | ForbiddenCheck with its allowlists and fixtures | Z2T-PROC-008 | `ForbiddenCheck.ps1`, `Forbidden/Zenith2Forbidden.psm1`, `std_headers.json`, `platform_headers.json`, `forbidden_allowlist.json`, `writable_data_allowlist.json`, fixtures | Z2-R-PROC-046 to -055, -173 | Z2T-MOD-056; CORE-P1, -P2, -P4, -P5, -P6 and DEVT-P5, -P10 dependents (Z2T-CORE-016, -019, -028, -036, -042, -081; Z2T-DEVT-018, -034, -071) |
| Z2T-PROC-010 | TestSeparationCheck, Libgraph and Complexity steps with their policy files | Z2T-PROC-004, -007, -008 | `TestSeparationCheck.ps1`, `Libgraph.ps1`, `Complexity.ps1`, `libgraph_policy.json`, `libgraph_baseline.json`, `shipping_forbidden_symbols.json` (with SEC-P5's, NT9's and CINE-P4's entries), `complexity_profile.json`, `complexity_lints_allowlist.txt` (empty), fixtures | Z2-R-PROC-028 to -031, -045, -056, -134, -141, -146, -150, -151 | Z2T-MOD-056; PLAT-P1/P6, RHI-P6, SCRIPT-P3, DEVT-P2 dependents (Z2T-PLAT-034, -072, -102; Z2T-RHI-096; Z2T-SCRIPT-083, -088, -108; Z2T-DEVT-025, -030, -050, -053, -067, -085, -103, -108) |
| Z2T-PROC-011 | Format, HeaderSelfContained, ApiContractCheck, ReviewSignals, ShaderChecks and EcsConformance steps | Z2T-PROC-008 | the six scripts, `Common/Zenith2ClangAst.psm1`, the ApiContractCheck front end and fixtures (with the two deviation fixtures) | Z2-R-PROC-042 to -044, -057 to -059 | Z2T-MOD-056 |
| Z2T-PROC-012 | Run-Committed.ps1, the acceptance suites and the gate self-test suite | Z2T-PROC-008 to -011, -013 | `Run-Committed.ps1`, `Zenith2RunCommitted.psm1`, `acceptance_suites.json` (with `coverageDeferred`), `Acceptance/Invoke-ClangTidy.ps1`, `Acceptance/Test-AcceptanceRecord.ps1`, `Tests/Run-GateSelfTests.ps1`, the cross-cutting tests (`Test-SanitizerPathsMatchCategory.ps1` excepted) | Z2-R-PROC-060 to -063, -147 to -149, -152, -153, -168 | Z2T-PROC-015, -025, Z2T-MOD-056 |
| Z2T-PROC-013 | Production coverage checker with failing fixtures | Z2T-PROC-008 | `Coverage.ps1`, `Coverage/*` (with `coverageMdPreamble`, `areaOrder`, `leafKinds` and `proofKinds`), fixture boards | Z2-R-PROC-066 to -072, -143, -160, -167 | Z2T-PROC-012, -014, -025, Z2T-MOD-056 |
| Z2T-PROC-014 | Production filer with reconcile mode and the headroom check | Z2T-PROC-013 | `Zenith2/Tools/Backlog/*` (with `--check-headroom`) | Z2-R-PROC-073 to -077 | Z2T-PROC-017 |
| Z2T-PROC-015 | CI workflow zenith2-gates.yml | Z2T-PROC-012 | `.github/workflows/zenith2-gates.yml` | Z2-R-PROC-078 | — |
| Z2T-PROC-016 | PROC-API: diff-derived review requirement and revision-bound review records | Z2T-PROC-002 | Vault CLI files and tests (the pair lines of the review block), `TICK.md` 6.3, generated harnesses, `zagent.project.json` `reviewPaths` | Z2-R-PROC-079 to -083, -142, -157 | — |
| Z2T-PROC-017 | Generic per-category postFinish command | Z2T-PROC-002, -014 | Vault routing and payload, `TICK.md` step 8, generated harnesses, `zagent.project.json` (the two Z2 `postFinish` lines) | Z2-R-PROC-086, -087, -166 | — |
| Z2T-PROC-018 | Reproducibility manifest, licence policy and setup scripts | — | the SHIP Stage 0 file set | Z2-R-PROC-088 | Z2T-PROC-008, Z2T-MOD-056 |
| Z2T-PROC-019 | Generic per-category reviewerConventions field | Z2T-PROC-002 | Vault routing and payload, `TICK.md` 6.3, generated harnesses, `zagent.project.json` | Z2-R-PROC-084, -085 | — |
| Z2T-PROC-020 | Direct decision-ID citation in the rulings matcher | — | `Tools/Vault/cli/legacy-pure/ticket.mjs`, tests | Z2-R-PROC-089 | — |
| Z2T-PROC-021 | Generic per-project lane provisioning hook | — | `Tools/lane_init.ps1`, `Tools/Vault/cli/lanes.mjs`, tests, `zagent.project.json` `lanes.provision`, `Zenith2/Tools/Setup/Provision-Lane.ps1` (with `-WarmOnly`, the scaffold condition and `lfs.fetchexclude`) | Z2-R-PROC-090, -091, -155 | Z2T-PROC-022 |
| Z2T-PROC-022 | Z2 lane registration | Z2T-PROC-003, -008, -018, -021 | none committed (the work log records `lane list`, `lane which` and the warm-up-skipped storage measurement) | Z2-R-PROC-092 | Z2T-MOD-056 |
| Z2T-PROC-023 | Z2 S0 mid-sprint integration check | Z2T-PROC-001, -002, -003 | none (evidence only) | the proof of Z2-R-PROC-094 (the leaf is Z2T-PROC-025) | Z2T-PROC-024, -025 |
| Z2T-PROC-024 | Z2 S0 stage acceptance | Z2T-PROC-023, -025, Z2T-MOD-056 (no stage-wide blocker list, `Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance ticket shape") | `Budgets.md`, `Budgets/GateRows.md`, `DecisionLog.md`, `allocation-stage-0.json`, `zagent.project.json` timeouts, `libgraph_baseline.json`, `Zenith2/Docs/Research/Acceptance/stage-0/` | every S0 requirement of every area (the proof) | — (the sprint gate) |
| Z2T-PROC-025 | Z2 landing and acceptance-shape verification | Z2T-PROC-013, -023; blocks Z2T-PROC-024 | none committed except the work log. DoD, one item each: (1) the A8a, archival, A6, A8b and A8c evidence is in the work log (`verify.ps1` all PASS, the `moved=… kept=…` line, `timing.json`, the filed-mode checker exit 0, the three `--name-only` diff lists, `zagent blocked --project Z2`); (2) `Coverage.ps1` reports no `CV-ACCEPTANCE`, `CV-ROUTING` or `CV-DRAFT-ID` finding for the sixteen acceptance and mid-sprint tickets, and no requirement is attached to a ticket whose stage comes after its own; (3) each of the eight sprints has its gate designated (`zagent sprint list --project Z2 --json`); (4) the hand-off ledger audit of `Zenith2/Docs/Design/PROC/Acceptance.md` → "Stage 0 acceptance"; (5) failure item: a missing sprint gate or a foreign requirement leaves the ticket In Progress | Z2-R-PROC-093, -094, -109, -110, -118, -123 to -131, -144, -159 | Z2T-PROC-024 |

The scaffold Z2T-MOD-056 carries `blocked_by` Z2T-PROC-003, -005, -008, -009, -010, -011, -012, -013, -018 and -022. MOD's precondition rewording names deliverables 2, 8, 14 and 16. The guardrail-first rule and the five step tickets make the remaining edges explicit, because the hand-over gate list already names `Coverage.ps1` and every base step: the scaffold's first tick finds every base line's script, and Z2T-PROC-012's `Test-GateScriptsExist.ps1` passes when it lands.

### Stage 1 to Stage 7 tickets

| Draft-ID | Title | Sprint | blocked_by | Requirements |
|---|---|---|---|---|
| Z2T-PROC-030 | needs-revalidation claim predicate | Stage 1 | Z2T-PROC-024 | Z2-R-PROC-111, -119 |
| Z2T-PROC-032 | Z2 S1 mid-sprint integration check | Stage 1 | — | the proof of Z2-R-PROC-118 and -159 for its stage (both S7; leaf Z2T-PROC-025) |
| Z2T-PROC-033 | Z2 S1 stage acceptance | Stage 1 | Z2T-PROC-030, -032, -034 | every S1 requirement (the proof) |
| Z2T-PROC-034 | Z2 CORE justification-record allowlist entries | Stage 1 | Z2T-CORE-035 (the ruling that creates the decision); blocks Z2T-CORE-036 and Z2T-CORE-042 | Z2-R-PROC-135 |
| Z2T-PROC-040 | Stage 2 PROC ticket activating Android build and ApkCheck | Stage 2 | Z2T-MOD-039, Z2T-SHIP-033 (offline Gradle home), Z2T-SHIP-034 (Android SDK), Z2T-AUDIO-061 | Z2-R-PROC-095 to -098, -112 |
| Z2T-PROC-041 | Stage 2 PROC ticket activating Sanitizers | Stage 2 | Z2T-DEVT-010, Z2T-SHIP-066, Z2T-SHIP-035 | Z2-R-PROC-099 to -103, -138, -139 |
| Z2T-PROC-042 | Z2 S2 mid-sprint integration check | Stage 2 | — | the proof of Z2-R-PROC-118 and -159 for its stage (both S7; leaf Z2T-PROC-025) |
| Z2T-PROC-043 | Z2 S2 stage acceptance | Stage 2 | Z2T-PROC-040, -041, -042, -044, -045, -046 | every S2 requirement (the proof) |
| Z2T-PROC-044 | Stage 2 PROC ticket landing SecurityConformance | Stage 2 | Z2T-PROC-046 and the SEC tickets that land `Zenith2/Tools/Security/parser_inventory.json` and the conformance check's inputs (Draft-IDs named at SEC's A5; SEC's tickets are not drafted yet) | Z2-R-PROC-136 |
| Z2T-PROC-045 | ForbiddenCheck rule F24 and confined_apis.json | Stage 2 | the SEC Stage 1 security-architecture ruling ticket, which confirms the confined-API rule (`Zenith2/Docs/Design/SEC/PathTraversal.md` → "Rules confirmed by the ruling"; Draft-ID named at SEC's A5) | Z2-R-PROC-137 |
| Z2T-PROC-046 | SEC-P9: the protected Security gate directory | Stage 2 | the SEC ticket that supplies the readers' and matchers' design inputs (Draft-ID named at SEC's A5); blocks Z2T-PROC-044 and -054 | Z2-R-PROC-164 |
| Z2T-PROC-050 | Stage 3 PROC ticket activating BackendParity | Stage 3 | Z2T-RPROD-100, Z2T-RHI-101 | Z2-R-PROC-104, -105, -113 |
| Z2T-PROC-051 | Stage 3 PROC ticket adding IntegrationSmoke to Run-Committed.ps1 | Stage 3 | the first focused workload-game ticket with a scripted path (Z2T-DEVT-097) | Z2-R-PROC-106 |
| Z2T-PROC-052 | Z2 S3 mid-sprint integration check | Stage 3 | — | the proof of Z2-R-PROC-118 and -159 for its stage (both S7; leaf Z2T-PROC-025) |
| Z2T-PROC-053 | Z2 S3 stage acceptance | Stage 3 | Z2T-PROC-050, -051, -052, -054 | every S3 requirement (the proof) |
| Z2T-PROC-054 | Libgraph Shipping exposure layers and the forbidden string list | Stage 3 | Z2T-PROC-046 (the exposure script and its matchers) and SEC's ticket for the scan half of SEC-P9 (Z2-R-SEC-106; Draft-ID named at SEC's A5) | Z2-R-PROC-140 |
| Z2T-PROC-060 | Z2 S4 mid-sprint integration check | Stage 4 | — | the proof of Z2-R-PROC-118 and -159 for its stage (both S7; leaf Z2T-PROC-025) |
| Z2T-PROC-061 | Z2 S4 stage acceptance | Stage 4 | Z2T-PROC-060 | every S4 requirement (the proof); Z2T-PROC-062 is in stage 4's `coverageDeferred` |
| Z2T-PROC-062 | Z2 PROC apply complexity profile S4 | Stage 4 | MAINT's ruling ticket T9 `Z2 S4 complexity calibration` (its Draft-ID is written here once MAINT's tickets are filed, cross-area row MT10) | Z2-R-PROC-114, -145 |
| Z2T-PROC-070 | Stage 5 PROC ticket activating PathParity | Stage 5 | Z2T-RPROD-120 | Z2-R-PROC-107, -108, -115 |
| Z2T-PROC-071 | Z2 S5 mid-sprint integration check | Stage 5 | — | the proof of Z2-R-PROC-118 and -159 for its stage (both S7; leaf Z2T-PROC-025) |
| Z2T-PROC-072 | Z2 S5 stage acceptance | Stage 5 | Z2T-PROC-070, -071 | every S5 requirement (the proof); Z2T-PROC-073 is in stage 5's `coverageDeferred` |
| Z2T-PROC-073 | Z2 PROC apply complexity profile S5 | Stage 5 | MAINT's ruling ticket T11 `Z2 S5 complexity calibration` (Draft-ID per MT10) | Z2-R-PROC-161 |
| Z2T-PROC-080 | ApkCheck over the App Bundle splits | Stage 6 | Z2T-PACK-057 (the `Minimal_Bundle` build) | Z2-R-PROC-116, -121 |
| Z2T-PROC-081 | Z2 S6 mid-sprint integration check | Stage 6 | — | the proof of Z2-R-PROC-118 and -159 for its stage (both S7; leaf Z2T-PROC-025) |
| Z2T-PROC-082 | Z2 S6 stage acceptance | Stage 6 | Z2T-PROC-080, -081 | every S6 requirement (the proof); Z2T-PROC-083 is in stage 6's `coverageDeferred` |
| Z2T-PROC-083 | Z2 PROC apply complexity profile S6 | Stage 6 | MAINT's ruling ticket T12 `Z2 S6 complexity calibration` (Draft-ID per MT10) | Z2-R-PROC-162 |
| Z2T-PROC-090, -091, -092a, -092b, -092c | Stage 7 expansion gate lines (dedicated server, OpenXR, and the `ps5`, `xbox-series`, `switch2` overlay lines) | Stage 7 | as "Stage 7 lines" states | Z2-R-PROC-117 (leaf: Z2T-PROC-090), -122, -170 |
| Z2T-PROC-093 | Z2 S7 mid-sprint integration check | Stage 7 | — | the proof of Z2-R-PROC-118 and -159 for its stage (both S7; leaf Z2T-PROC-025) |
| Z2T-PROC-094 | Z2 S7 stage acceptance | Stage 7 | Z2T-PROC-090 to -093 | every S7 requirement (the proof); Z2T-PROC-095 is in stage 7's `coverageDeferred` |
| Z2T-PROC-095 | Z2 PROC apply complexity profile S7 | Stage 7 | MAINT's ruling ticket T13 `Z2 S7 complexity calibration` (Draft-ID per MT10) | Z2-R-PROC-163 |

Each apply ticket is in its stage's `coverageDeferred` list (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "acceptance_suites.json"), because the ruling it applies measures the tree the acceptance accepts; it is Done before the roll (acceptance procedure, MAINT A3) and is checked Done by the next stage's cumulative Coverage. The four apply tickets each copy `complexity_profile.proposed.json` from MAINT's ruling over the protected profile, after checking the profile's SHA-256 against the ruling's recorded `baseSha256`, and run the `Complexity` step (`Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity calibration"; ruling MAINT-R1 R3 limits calibration to Stages 4 to 7). No Draft-ID is reserved for flagged proposal P2 (Z2-D-130): the human's approval files its ticket.

The mid-sprint tickets are category `Zenith2` (`Zenith2/Docs/Roadmap.md` → "Mid-sprint integration check"); each acceptance ticket is `blocks`-linked from its stage's mid-sprint ticket and, by the acceptance shape, from nothing stage-wide (`Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance ticket shape").

## Hand-off ledger

Every PROC hand-off recorded by an area writer, with its disposition. "PROC ticket" means the ticket carries the hand-off's content verbatim in its Implementation Contract; "answered" names what made a PROC ticket unnecessary.

### Pins and the reproducibility manifest

| Hand-off | Ask | Disposition |
|---|---|---|
| COMPRESS-P1 | zlib and zstd pins | answered: SHIP pinning ticket Z2T-SHIP-044 (`Zenith2/Docs/Design/SHIP.md` → "Pin landing order") |
| COMPRESS-P2 | the scaffold's `zenith-wrapper` and `cmake-subproject` modes | answered: SHIP's Stage 1 tickets T2/T3 and the scaffold Z2T-MOD-056 (no protected file) |
| CORE-P8 | mimalloc, xxHash, yyjson, simdjson pins | answered: Z2T-CORE-053 (`needs-human`) per SHIP's pin table |
| ECS-P1 | flecs and EnTT pins with second owners | answered: Z2T-SHIP-046 |
| GEOM-P1, ASSET-P3 | meshoptimizer, MikkTSpace, KTX-Software, fastgltf | answered: Z2T-SHIP-051 |
| LIGHT-P2 | upscaler candidate pins | answered: Z2T-SHIP-060 |
| MOD-P1 (c) | the doctest `dependencies` entry and the `llvm` pin | PROC ticket Z2T-PROC-018 (deliverable 14) |
| PLAT-P5 | SDL3 pin; the Ed25519 entry with SEC's admission | answered: Z2T-SHIP-047 (SDL3), Z2T-SHIP-061 (Ed25519) |
| RHI-P2 | Vulkan stack, Agility SDK, WARP, lavapipe, validation layer, Slang | answered: Z2T-SHIP-048, Z2T-SHIP-049 |
| RPROD-P1 | Slang and DXC host pins (one Slang entry shared with RHI-P2) | answered: Z2T-SHIP-049 |
| RPROD-P4 | the `flip` row | answered: Z2T-SHIP-052 |
| SCRIPT-P1 | the Luau entry | answered: Z2T-SHIP-045 |
| AUDIO-P1 | miniaudio and the D-SEL-CODEC candidates | answered: Z2T-SHIP-059 (miniaudio); the codec-candidate pins are a cross-area note to SHIP (no SHIP pinning ticket names opus, ogg or vorbis yet) |
| CINE-P1 | the `ffmpeg-libavcodec` D-SEL-VIDEO candidate pin (LGPL 2.1 libavcodec and libavutil, static, `win64` and `android-arm64`, owner `Zenith_Bench_VideoDecoderSelection`, usage `tools`) | answered, no protected edit: `Zenith2/ThirdParty/manifest.json` is not a protected path, so CINE's harness ticket Z2T-CINE-010 (or a SHIP pinning ticket SHIP names) adds the pin through SHIP's manifest procedure, and the licence class reaches `licence_policy.json`, whose edits are reviewed through `reviewPaths` (Z2T-PROC-016); cross-area note PG2 asked SHIP to name the pinning ticket |
| CINE-P2 | remove or re-own the `ffmpeg-libavcodec` entry when the harness retires | answered, no protected edit: Z2T-CINE-069 edits the unprotected manifest |
| RHI-P3 | shared gate environment: `VK_DRIVER_FILES`, the validation layer, WARP and `d3d12SDKLayers.dll` for test executables | PROC ticket Z2T-PROC-008 (`Get-Zenith2TestEnvironment`, [Z2-N-CONTRACT-434]); the files are staged by SHIP's T4 |

### Gate data file entries

| Hand-off | Ask | Disposition |
|---|---|---|
| CORE-P1 | F1 exemption for the three heap-backing platform files | Z2T-PROC-009 (`forbidden_allowlist.json` entries F1-01 … F1-03 of `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck allowlist files") |
| CORE-P2 | writable-data entries `g_pxRootHeap`, `g_xProfilerHooks`, `g_xDebugThreadState` | Z2T-PROC-009 (`writable_data_allowlist.json`) |
| CORE-P3, RHI-P4, SCENE-P1 | `platform_headers.json` entries | Z2T-PROC-009 |
| CORE-P4 | `<charconv>` in `ZenithCore/Internal/Text/` | the Stage 1 PROC ticket Z2T-PROC-034 adds entry F4-01 with the decision Z2T-CORE-035 creates; it is blocked by Z2T-CORE-035 and blocks Z2T-CORE-042, as CORE's drafted tickets expect ("the PROC allowlist ticket … with the record's ID"); no entry without an existing decision is committed (ruling PROC-R1 (8)) |
| CORE-P5 | F11 compiler-support exemption and `<immintrin.h>` | Z2T-PROC-034 adds F11-01 the same way and blocks Z2T-CORE-036; `<immintrin.h>` matches no rule (it is in neither header list), so it needs no entry |
| CORE-P6 | `<sanitizer/hwasan_interface.h>` in `ZenithCore/Internal/Android/Zenith_SanitizerHwasan_Android.cpp` | Z2T-PROC-009 (entry `FSAN-01`) |
| DEVT-P5 | `g_xProfilerThreadBuffers`, `g_pxCrashHandlerAnchor` with `extensions` and `comment` | Z2T-PROC-009 |
| DEVT-P10 | F16 step 5 checks line (1) of [Z2-N-CONTRACT-578] | Z2T-PROC-009 |
| ECS-P4 | F16 exemption for the schema-hook slot table | answered: ruling X174 (`m_pUserData`), no entry |
| PLAT-P1, PLAT-P6, RHI-P6, SCRIPT-P3, DEVT-P2 | `shipping_forbidden_symbols.json` entries and the Shipping-closure rule | Z2T-PROC-010 |
| RHI-P1 | rule F20 with a failing fixture | Z2T-PROC-009 |
| SCRIPT-P2 | the Scripting firewall rules with positive and negative fixtures | Z2T-PROC-009 |
| CHAR-P1, row X362 | rule F21 programme-wide | Z2T-PROC-009 |
| EDIT-P3, row X258 | the tool-access condition 7 | Z2T-PROC-010 (TestSeparationCheck; the condition is in `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates") |
| row X196 | test-separation condition 6 | Z2T-PROC-010 |

### Gate behaviour

| Hand-off | Ask | Disposition |
|---|---|---|
| MOD-P3, row M20 | `ApkCheck` builds `zenith_apks`; `CompileCoverage` reads each game's matrix from `declared_deps.json`'s `games` | Z2T-PROC-040 (ApkCheck); Z2T-PROC-008 (CompileCoverage) |
| MOD-P4 | Configure requests the codemodel reply | Z2T-PROC-008 |
| RPROD-P2 | the `ShaderChecks` step | Z2T-PROC-011 |
| RPROD-P3, RHI-P8, LIGHT-P3, ENV-P3 | `BackendParity` selects `z2-parity-backend` (the one name, ruling of 2026-10-02) and a timeout producer | Z2T-PROC-050 |
| RPROD-P5, ENV-P3 | `PathParity` selects `z2-parity-path` | Z2T-PROC-070 |
| LIGHT-P3 | the Stage 5 acceptance runs the `PT_*` and `AE_*` suites | Z2T-PROC-072 |
| SHIP-P1, DEVT-P7, row X216 | the `Test` step runs `win-shipping-test` | Z2T-PROC-008 |
| SHIP-P5 | the `ApkCheck` dex check | Z2T-PROC-040 |
| AUDIO-P2, row X233 | `ApkCheck` asserts each selected module's Java classes | Z2T-PROC-040 |
| DEVT-P3 | `Coverage` imports `Zenith2Evidence.psm1` and runs DEVT's assert scripts | Z2T-PROC-013 |
| DEVT-P1 | the Stage 0 `Libgraph` and `Complexity` tickets list Z2-R-DEVT-103 and -104 | Z2T-PROC-010 (and Z2T-PROC-024 proves both) |
| CORE-P7, ECS-P2 | the Sanitizers step runs the integration suites under `Zenith2/Tests/` too | Z2T-PROC-041 (every CTest test of the preset) |
| ENV-P4, ASSET-P2, CORE-P9, ECS-P3 | full-duration fuzz runs at stage acceptance | the acceptance tickets through `Run-Committed.ps1 -Acceptance`, durations from SEC |
| PK8, PACK-P1 | optionally `ApkCheck` over `Minimal_Bundle`'s splits from Stage 6 | Z2T-PROC-080, blocked by Z2T-PACK-057 |
| CINE-P3 | the `Test` step reports `requiresHardwareVideo` tests as not run, never as passed | Z2T-PROC-008: `Test.ps1` item 7 reports every skipped or not-run test as `TEST-NOT-RUN` (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "Test") |

### Category paths and gate lists

| Hand-off | Ask | Disposition |
|---|---|---|
| JOBS-P1, COMPRESS-P3, CORE-P7, PLAT-P3, ECS-P2, GEOM-P2, SCRIPT-P6, SCENE-P3, ASSET-P1, RHI-P5, DEVT-P8, SHIP-P2 | activate the two `Sanitizers` lines in Stage 2 | Z2T-PROC-041 (blocked by Z2T-DEVT-010 and Z2T-SHIP-066, per SHIP-P2) |
| C8, X50 | compression paths | already in the A1 hand-over paths |
| PLAT-P2, P13 | `ZenithPlatform/Internal/AsyncIO/**`, `Internal/Zenith_VirtualFileSystem*.cpp` | `sanitizer_paths.json` (Z2T-PROC-041) |
| RHI-P5 | `ZenithRHI/Internal/**`, `ZenithFlux/Internal/{Lifetime,Upload,Readback,Bindless,GPUScene}/**` | `sanitizer_paths.json`, `win-asan` list only (GPU libraries are absent from `linux-tsan`) |
| GEOM-P2 | `ZenithVertexFormat/**` (both), Flux `Internal/Geometry/**`, `Internal/Textures/**`, `Internal/Materials/**`, `Internal/Procedural/**` (`win-asan`) | `sanitizer_paths.json` |
| ASSET-P1, AS7 | `ZenithAssets/Internal/Loading/**`, `Internal/Registry/**` | `sanitizer_paths.json` (both) |
| ECS-P2 | `ZenithReflect/**`, `ZenithECS*/**`, `Zenith2/Tests/Reflection/**`, `Zenith2/Tests/Determinism/**` | `sanitizer_paths.json` (both) |
| SCRIPT-P4 | `ZenithScript/**`, `ZenithScriptCompiler/**`, `Zenith2/Tests/Script*/**` | `sanitizer_paths.json` (both) |
| SCENE-P2 | `ZenithDocument/**`, `ZenithScene/**`, `Zenith2/Tools/DocumentHost/**` | `sanitizer_paths.json` (both) |
| SAVE-P1, SV14 | the ZenithSave and runtime-bridge globs of the amended row | `sanitizer_paths.json` (both lists for ZenithSave; `win-asan` alone for the `ZenithRuntime` bridge files, a GPU library) |
| DEVT-P4, DV17 (d), X247 (c) | `ZenithProfiling/**`, `ZenithPlatform/Internal/CrashHandler/**`, `ZenithPlatform/Internal/*/Zenith_Crash*`, `ZenithRuntime/Internal/DevTools/**`, `Zenith2/Tests/DevTools/**`, `Zenith2/Tests/TsanSmoke/**` | `sanitizer_paths.json` (`ZenithRuntime` paths in `win-asan` alone) |
| AUDIO-P3 | `ZenithAudio/Internal/RealTime/**`, `ZenithAudio/Internal/Loading/Zenith_AudioRetirement*` | `sanitizer_paths.json`, `win-asan` alone (ZenithAudio is a GPU-matrix module) |
| AI-P1, AI7 | `ZenithAI/**` | `sanitizer_paths.json`, `win-asan` alone |
| PH16 | `ZenithPhysics*/**` | `sanitizer_paths.json`, `win-asan` alone |
| PK8 | `ZenithPackage/**` | `sanitizer_paths.json` (both) |
| SHIP-P4 | `CMake/ZenithFlags.cmake`, `CMake/ZenithThirdParty.cmake`, `ThirdParty/**` | `sanitizer_paths.json` (both) |
| ENV-P2, RHI-P7 | `Zenith2Render` paths | answered: already inside the hand-over `Zenith2Render` paths |
| SHIP-P3, SH16 | `reviewPaths` gains `licence_policy.json` | Z2T-PROC-016 |
| row X22 | direct decision-ID citation | Z2T-PROC-020 |
| CONTENT-P2 | `Zenith2/Tools/ContentCore/**` selects `win-asan` | `sanitizer_paths.json`, `win-asan` list (Z2T-PROC-041) |
| CONTENT-P3 | optional workflow job for the shared-store population | declined: nothing waits on it, CONTENT keeps population as operator configuration on a host (`Zenith2/Docs/Design/CONTENT/SharedCache.md` → "Deployment"), and a scheduled workflow would add a protected file that runs content tools with repository write access; reopened by a CONTENT hand-off if an operator host is ever unavailable |
| CONTENT-P4 | the `Zenith2/CLAUDE.md` layout line names `ContentCore` and `ZenithContent` | answered: the orchestrator's patch h |

### Documents and acceptance

| Hand-off | Ask | Disposition |
|---|---|---|
| EDIT-P1, row X387 | `Zenith2/CLAUDE.md` layout line gains `Authoring` and `EditorCore` (EDIT) and `Maintenance` (MAINT); SEC's `Security` joins them | the orchestrator's pre-A8a edit while the file is still unprotected (ruling PROTECTED-EDITS-BY-ORCHESTRATOR); if A8a lands without it, Z2T-PROC-008 makes the edit in its `Zenith2/CLAUDE.md` change (the layout line and the gate-step line in one diff) |
| DEVT-P6 | the repository guide's naming bullet and non-negotiable 3 | answered: the orchestrator's 2026-10-16 edit of `Zenith2/CLAUDE.md` |
| MOD-P1, MOD-P2, COMPRESS-P4, CORE-P9, PLAT-P4, RHI-P9, GEOM-P3, RPROD-P6, ASSET-P2, ECS-P3, LIGHT-P1, SCRIPT-P5, SCENE-P4, ENV-P1, EDIT-P2, EDIT-P4, SHIP-P6, SAVE-P2, AUDIO-P4, DEVT-P9, CONTENT-P1, PACK-P2, CHAR-P2, CINE-P5, row X62 | list every requirement of the stage and collect the named evidence | the acceptance tickets (`Zenith2/Docs/Design/PROC/Acceptance.md`), whose requirement lists follow the derivation rule and whose evidence tables reproduce each area's list |
| GRAPH | none beyond the Stage 2 sanitizer lines | Z2T-PROC-041 |
| Q78 ruling | Z2-R-SCENE-142 moves from the Stage 2 to the Stage 3 list | applied by the derivation rule (the catalogue holds S3) |

### Late hand-offs

Rows addressed to PROC after its draft, absorbed in revision 1 under ruling PROC-R1 (2):

| Hand-off | Ask | Disposition |
|---|---|---|
| SEC-P1 (SEC10) | the gate step `SecurityConformance` (SC01 to SC16) with fixtures and assert script, a base-category line from Stage 2, the `Zenith2/CLAUDE.md` step entry, the Coverage script run; core row X414 | Z2T-PROC-044: the script as `Zenith2/Docs/Design/SEC/ConformanceCheck.md` specifies it, with the shared lifecycle; base row 19; its timeout row (X415); the step-list entry in `Zenith2/CLAUDE.md` and in "Gate step command lines"; `Coverage` runs the assert script ("Additional runs") |
| SEC-P2 | smoke selection by the diff and the smoke budget | Z2T-PROC-041 (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Smoke selection and budget") |
| SEC-P3 | every fuzz-owning directory in `sanitizer_paths.json` and `Zenith2Memory` | Z2T-PROC-041 (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizer path selection") |
| SEC-P4 | the confined-API rule with `confined_apis.json` | Z2T-PROC-045, numbered F24 because PROC keeps F22 and F23 (ruling PROC-R1 (3)) |
| SEC-P5 | SEC's consolidated forbidden-symbol entries | Z2T-PROC-010 (Stage 0) |
| SEC-P6, SEC-P7 | `shipping_forbidden_strings.json`, the string scan, the listener-import scan and the positive controls | Z2T-PROC-054 (Stage 3), through SEC's audit script |
| SEC-P8 | `reviewPaths` gains `update_keys.json`, `parser_inventory.json`, `path_matrix.json` and `android_permissions.json` | Z2T-PROC-016 |
| SEC-P9 | the protected `Zenith2/Tools/Gates/Security/` directory: `Zenith2SecurityGates.psm1`, `Test-Zenith2ShippingExposure.ps1`, `Test-Zenith2UpdateKeys.ps1`, the assert script and fixtures (ruling SEC-R1 R9) | Z2T-PROC-046 (Stage 2), blocking -044 and -054; the configure-to-directory edge is SL9's one exception (SEC-R2 m5); the assert script at `Tests/Security/Test-Security.ps1` matches the runner's `Tests/*/Test-*.ps1` glob |
| SEC19 | F24's dynamic-loading allow records equal F22's; F24's tokens gain `_wopen`, `_open`, `fopen_s`, `_wfopen_s`, `CopyFileW`, `CopyFile2`, `MoveFileW`, `NtCreateFile`, `CreateProcessAsUserW`, `_wsystem`, `_spawn*`, `_wspawn*` and the member rule | Z2T-PROC-045 (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table", F24 row and fixtures) |
| P-RS1 (RS7) | the filer writes `Kind:`; `Coverage` runs `Test-Zenith2DecisionTicket -Mode board` on decision tickets | Z2T-PROC-014 (the trailer) and Z2T-PROC-013 (the run) |
| P-RS2 | Coverage acceptance mode fails an unruled research requirement and runs `Invoke-ResearchCheck.ps1 -Mode decisionlog` | Z2T-PROC-013 ("Coverage acceptance mode" item 5) |
| P-RS3 | the session checker's drafts mode gains `decision-template` before A6 | withdrawn by RESEARCH (ruling RESEARCH-R1-RULINGS, row RS12): the drafts checker is an orchestrator tool outside the repository; nothing for PROC |
| P-RS4 (extended, RS12) | the `zenith-research` block of `Zenith2/.gitattributes` (`Docs/Research/** text eol=lf`, then `Docs/Research/**/archive/** filter=lfs diff=lfs merge=lfs -text`), and `lfs.fetchexclude = Zenith2/Docs/Research/**` in every Z2 lane worktree | the `.gitattributes` block is the orchestrator's pre-A8a edit (`Zenith2/Docs/Design/PROC/HumanProcedures.md` → "Before A8a"); the fetch exclusion is `Provision-Lane.ps1` step 1 (Z2T-PROC-021) |
| P-RS6 (RS12) | `Zenith2/Docs/Research/**/runs/**` and `Zenith2/Docs/Research/Machines/**` join the global guardrail lists, with harness regeneration | Z2T-PROC-001 (the four guardrail strings, `$mustBeProtected`, the refusal check of `Zenith2/Docs/Design/PROC/Harness.md` → "Verification run by the human"); the acceptance evidence copies sit at `Docs/Research/Acceptance/stage-<N>/`, clear of both globs |
| P-RS5 | every mid-sprint check lists the four research steps | the mid-sprint tickets (`Zenith2/Docs/Design/PROC/Acceptance.md` → "Mid-sprint integration checks" item 3) |
| NT9 | `Zenith_NetTransport::Create` in `shipping_forbidden_symbols.json` at Stage 3 | Z2T-PROC-010, at Stage 0 (a ban on a symbol not yet defined is vacuous, so landing early satisfies "at Stage 3") |
| SP10 (a) | `Run-Committed.ps1 -Acceptance` exports `ZENITH_ACCEPTANCE_STAGE` | Z2T-PROC-012 (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N" item 4) |
| SP10 (b), (c) | the `z2-xr` line against SCOPE's scripted runtime; console presets `ps5`, `xbox-series`, `switch2` | Z2T-PROC-091, -092a to -092c ("Stage 7 lines") |
| MT3 (a) | MAINT's acceptance steps A1 to A5 and mid-sprint steps M1 to M4 | every acceptance and mid-sprint ticket (`Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance procedure", "Mid-sprint integration checks") |
| MT3 (b) | a `Tools/libgraph/**` change attaches surfaces before and after and is followed by a re-pin | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "The inbound-surface review rule" (last bullet), every PROC ticket editing `Tools/libgraph/**` |
| MT3 (c), X386 | `Zenith2/Tools/Maintenance/*.json` in `reviewPaths` | Z2T-PROC-016 |
| MT3 (d), X389 | one PROC complexity-profile apply ticket per stage, blocked by MAINT's ruling | Z2T-PROC-062, -073, -083, -095 (Stages 4 to 7, ruling MAINT-R1 R3) |
| MT3 (e) | the headroom ticket of [Z2-N-BUDGET-353] filed by the tick orchestrator from MAINT's template | answered by `postFinish` (ruling PROC-R2 M9): the Z2 `postFinish` runs `File-Backlog.ps1 --check-headroom` after every Done, which files MAINT's ticket on three consecutive exceedances of 80 % of a phase cap (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Production filer"); no `TICK.md` edit; the mid-sprint check stays the backstop |
| MT3 (f) | MAINT agrees with the Stage 0 contents of both gate files | confirmed: Z2T-PROC-010 lands them as `Zenith2/Docs/Design/MAINT/GateData.md` → "Stage 0 contents" records |
| MT3 (g) | no `ApiContractCheck` change for MAINT's `Version` clauses | confirmed, with the deviation-line coverage of data members and enumerators and its two fixtures (Z2T-PROC-011; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "ApiContractCheck") |
| MAINT-R1 R5 | `-WriteBaseline` through `Run-Committed.ps1` only; the libgraph `surface.json` and `surface_delta.json` schemas; the acceptance review-record rule | Z2T-PROC-012 and -010; `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph outputs"; `Zenith2/Docs/Design/PROC/Harness.md` → "4. Finish refusal" |
| CHAR-P1 (scope part) | F21 over `Zenith2/Tools/ZenithEditor/Internal/Character/` and `Zenith2/Benchmarks/CharacterAuthoring/` | Z2T-PROC-009 (`f21Scope`); CHAR's interim scan is CHAR's to retire |
| CINE-P4 | `Zenith_CinematicCapture::Begin`, `Zenith_ExportCinematicCapture` in the forbidden-symbol list | Z2T-PROC-010 |
| MT3 (h), (i), (k), (l), (m) | the deviation fixtures; the surface formats; the exact re-pin command; the acceptance review-record rule; no SourceLayout/Format/ForbiddenCheck exemption for `Tools/Maintenance/Tests/**` | (h) Z2T-PROC-011; (i) `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph outputs"; (k) `Run-Committed.ps1 -Acceptance -Stage N -WriteBaseline` (no `-Only` switch: the re-pin form runs only Libgraph); (l) no exemption, the human-run review procedure applies; (m) confirmed, no exemption needed |
| MT3 (j) | the analyzer's generic `--dump-entities <path>` (schema `complexity-entities`) | Z2T-PROC-004 (`Zenith2/Docs/Design/PROC/Harness.md` → "dump-entities"); unblocks MAINT's T7 |
| MT7 | only if Q97 is ruled (a): `File-Backlog.ps1 --mark-stale --keys <comma-separated keys>` | conditional: on Q97 (a), a PROC ticket adds the option, which applies `needs-revalidation` and `deferred` to exactly the listed To Do tickets without recomputing cite hashes; on (b), nothing is filed |
| MT8 | commit the growth report with its disposition before the re-pin, then the baseline alone; `RES-INCOMPLETE` is the "disposition filled" test | `Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance procedure" step 6; the `-WriteBaseline` precondition |
| MT10 | name MAINT's ruling tickets T9, T11, T12, T13 for Z2T-PROC-062, -073, -083, -095 | the rows name T9 to T13 now; their Draft-IDs are written in once MAINT's tickets are filed, and the blocking edges are drafted with the backlog |
| SP17 | console presets exit 3 without their overlay; console lines in no main-board category; presets from the overlays; Q98 (a) denied read paths | "Stage 7 lines"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Build" |
