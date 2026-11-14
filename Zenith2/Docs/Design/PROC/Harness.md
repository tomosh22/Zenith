# PROC design — the Zenith-side harness, Vault and lane changes

Companion to `Zenith2/Docs/Design/PROC.md`. This file specifies every change PROC makes outside `Zenith2/`: the guardrail additions, the three-harness regeneration they and the protocol edits force, the `tick_gates.ps1` external-build mode, the Vault pass-through, the analyzer keys, the scanner audit, the root delegation line, the libgraph CMake File API front end, PROC-API, the reviewer conventions, `postFinish`, the rulings citation weight, the lane provisioning hook, the Z2 lane, the `needs-revalidation` claim predicate, the conditional N-lanes change and the CI workflow. Every change is generic: none names Zenith2 except the root delegation line (Z2-D-030) and the Z2 values inside `zagent.project.json` and `vault.config.json`. Line references are to commit `268b8ef6` and are anchors, not edit instructions: each ticket names functions and headings, which survive unrelated edits.

## Scope of this file

| Section | Ticket | Protected files it edits |
|---|---|---|
| "Guardrail additions", "Harness regeneration" | Z2T-PROC-001 | `vault.config.json`, `zagent.project.json`, `Tools/zagent/Test-HarnessParity.ps1`, every generated harness file |
| "tick_gates external-build mode" | Z2T-PROC-003 | none (`Tools/tick_gates.ps1` is not protected); `TICK.md` only for its exit-ladder and field text |
| "Vault pass-through" | Z2T-PROC-002 | none (`Tools/Vault/**` is not protected) |
| "Analyzer keys" | Z2T-PROC-004 | none |
| "Scanner audit" | Z2T-PROC-005 | `.github/workflows/*.yml` (the positive `paths:` filters) |
| "Root delegation" | Z2T-PROC-006 | `zagent.project.json` |
| "libgraph CMake front end" | Z2T-PROC-007 | none |
| "PROC-API review records" | Z2T-PROC-016 | `TICK.md`, generated harnesses, `zagent.project.json` |
| "Reviewer conventions" | Z2T-PROC-019 | `TICK.md`, generated harnesses, `zagent.project.json` |
| "postFinish" | Z2T-PROC-017 | `TICK.md`, generated harnesses, `zagent.project.json` |
| "Rulings citation weight" | Z2T-PROC-020 | none |
| "Lane provisioning hook" | Z2T-PROC-021 | `zagent.project.json` |
| "Z2 lane registration" | Z2T-PROC-022 | none (registry state under `.git/vault/`) |
| "Revalidation claim predicate" | Z2T-PROC-030 | `TICK.md` (the label table), generated harnesses |
| "N lanes per project" | none filed: flagged proposal P2 is not applied (Z2-D-130); the human's approval would file the ticket | `zagent.project.json`, `TICK.md` lane-mode text, generated harnesses (on approval only) |
| "CI workflow" | Z2T-PROC-015 | `.github/workflows/zenith2-gates.yml` |

Every ticket that edits `zagent.project.json`, `vault.config.json`, `Tools/tick_gates.ps1`, `Tools/build_lock.ps1` or `Tools/Vault/**` is executed with every lane idle, because every lane reads the main checkout's configuration and tooling live (Z2-D-096).

## Guardrail additions

The first Stage 0 PROC ticket (Z2-D-118; `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 1).

### The edits

| File | Member | Change |
|---|---|---|
| `vault.config.json` | `policy.board.guardrails.protectedPaths` | append, in this order, `"Zenith2/Tools/Gates/**"`, `"Zenith2/CLAUDE.md"`, `"Zenith2/Docs/Research/**/runs/**"` and `"Zenith2/Docs/Research/Machines/**"` (the last two are RESEARCH's P-RS6: committed research runs and machine records are evidence no worker may write) |
| `vault.config.json` | `policy.resolved.guardrails.protectedPaths` | the same four strings, appended in the same order |
| `zagent.project.json` | `protectedPaths` (top level) | the same four strings, appended; this copy is documentation only, because `guardrailsOf` resolves `project.protectedPaths ?? boardGuardrails ?? clientFile.protectedPaths` as a fallback chain and the board list always wins (Z2-D-089) |
| `Tools/zagent/Test-HarnessParity.ps1` | `$mustBeProtected` | gains the same four strings, so a subsequent edit that drops any of them from the resolved list fails the parity gate |

Both `vault.config.json` lists are edited because enforcement (`zagent guard`, `checkProtectedPaths` over `index.config.policy.resolved`) and contract reachability read the resolved list, while the board list is the floor the payload mirrors (Z2-D-118). Nothing else in either file changes: the human verifies with `git diff --stat` that each list gained four lines and the comma changes they need. The acceptance evidence copies live at `Zenith2/Docs/Research/Acceptance/stage-<N>/`, clear of both research globs, so the human's acceptance commit is not refused by them.

### Consequences the ticket checks

- From the commit on, a ticket whose Definition of Done names a path under `Zenith2/Tools/Gates/` or `Zenith2/CLAUDE.md` (with `\` or `/`) is contract-invalid (`reachabilityError` in `Tools/Vault/cli/legacy-pure/contract.mjs`), and `zagent guard` refuses a diff touching them in any ticket the loop runs.
- `zagent check` on every PROC ticket that names those paths in its DoD now reports the protected-path `contractError` (the expected Stage 0 state of `Zenith2/Docs/Roadmap.md` → "Stage 0 entry criteria" item 2).
- The worker charter's `{{PROTECTED_PATHS}}` substitution changes, so all three harnesses are regenerated in the same commit ("Harness regeneration").

### Verification run by the human

1. `pwsh -NoProfile -File Tools/zagent/Build-HarnessArtifacts.ps1 -Harness claude`, `-Harness opencode`, `-Harness codex`, then each with `-Check` (exit 0 three times).
2. `pwsh -NoProfile -File Tools/zagent/Test-HarnessParity.ps1` and `pwsh -NoProfile -File Tools/zagent/Test-ZagentClient.ps1` (both pass).
3. `pwsh -NoProfile -File Tools/zagent/zagent.ps1 doctor --json` is `ok`.
4. `pwsh -NoProfile -File Tools/zagent/zagent.ps1 guard <a ZEN ticket key> --file <a list naming Zenith2/Tools/Gates/Configure.ps1>` refuses, and so does the same call with a list naming `Zenith2/Docs/Research/Machines/x.json` (dry demonstrations; the ticket is not claimed, `guard` with `--file` reads the list).
5. The captured `zagent gates` output for the sample ZEN and ZM tickets of the Stage 0 mid-sprint check is byte-identical to its pre-Stage-0 capture.

## Harness regeneration

`TICK.md`, the role charters and the generator are canonical; every harness's entry point and role files are generated (`Tools/zagent/CLAUDE.md`). A PROC ticket that changes any of these regenerates every harness in the same commit:

| Trigger | Tickets |
|---|---|
| the resolved protected-path list (the worker charter's `{{PROTECTED_PATHS}}`) | Z2T-PROC-001 |
| `TICK.md` step 6.3 | Z2T-PROC-016, Z2T-PROC-019 |
| `TICK.md` step 8 | Z2T-PROC-017 |
| `TICK.md` label table (and the lane-mode section, only if P2 is approved) | Z2T-PROC-030 |
| `TICK.md` exit-ladder and `tick_gates.json` field text | Z2T-PROC-003 |

The procedure, in this order, in the ticket's own diff:

1. Edit the canonical file.
2. When the edit inserts or removes lines in `TICK.md`, re-derive `Tools/zagent/protocol/tier-mentions.allowlist` per its own header comment (it pins bare tier mentions by file and line).
3. Run `pwsh -NoProfile -File Tools/zagent/Build-HarnessArtifacts.ps1 -Harness claude`, then `-Harness opencode`, then `-Harness codex` (there is no all-harness mode).
4. Run the same three commands with `-Check`; each must exit 0.
5. Run `Tools/zagent/Test-HarnessParity.ps1` and `Tools/zagent/Test-ZagentClient.ps1`.
6. A command that `TICK.md` mandates and a harness must pre-authorise sits in inline backticks at least once, because the pre-authorisation audit ignores fenced blocks (`Tools/zagent/CLAUDE.md`, "Parity lessons").
7. Commit the canonical edit, the allowlist and every regenerated file together.

The orchestrator's uncommitted ZEN-119 edits (the stale protocol and reviewer-charter statements, regenerated for all three harnesses, `-Check` clean) are the precedent for this procedure; ZEN-119 lands as its own ZEN commit before A8a (`Zenith2/Docs/Design/PROC/HumanProcedures.md` → "Before A8a").

## tick_gates external-build mode

A generic change of `Tools/tick_gates.ps1` inside its existing phase machinery (Z2-D-028, Z2-D-105, Z2-D-106; `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 2). What does not change: the build slot (`Enter-ZenithBuildSlot`, exit 11), the tree SHAs and the one automatic verify retry (exit 12), `-ReuseGateList`, the per-line wrapper, `Write-Result` and its exit-8 guard, the top-level catch and finally, and the exit ladder 0/2/3/4/5/8/10/11/12 (`integrate` accepts only that verdict format). No new exit code is introduced, so `integrate.mjs`'s documented gate exits and `tick-checks.mjs`'s copy need no change.

### Reading the gate record

`Get-GateList` returns the parsed `gates.json` record instead of its `gates` array alone. The new members it reads (written by "Vault pass-through"):

| Member | Type | Meaning |
|---|---|---|
| `buildSystem` | `"external"` or null | the ticket's own category's build system |
| `measureSteps` | string array | the union, over the ticket's own category and every unioned category, of their `measureSteps`, de-duplicated, own category first |
| `phaseTimeoutMinutes` | integer or null | the ticket's own category's warm cap |
| `coldPhaseTimeoutMinutes` | integer or null | its cold cap |
| `unioned[].buildSystem` | `"external"` or null per unioned category | for the mixed-union rule |
| `mixedUnionAllowed` | bool | true exactly when the ticket carries the label `needs-human` (pending Q8; "Classification" item 1) |

A record without `buildSystem` (every non-Z2 ticket) takes the Sharpmake path unchanged, so existing ZEN and ZM tickets behave byte-identically (asserted by the Stage 0 mid-sprint check).

### Classification

1. **Mixed union (pending Q8; this draft follows the orchestrator's recommendation (a)).** Let `E` be the set of `buildSystem` values among the own category and the unioned ones. When `E` contains both `"external"` and null: if `mixedUnionAllowed` is false, `Write-Result 5 "mixed union: <own category> unions <names>; a ticket without needs-human may not mix build systems"` and exit 5 before any line runs (Z2-D-106); if true, use mixed classification (item 4).
   - Q8 (a) reads "never refuse when the ticket's own category is PROC-ish (label opt-in)". This design reads "PROC-ish" as "carries `needs-human`", because PROC tickets have no category of their own: they are filed in `Zenith2`, which every ordinary Z2 ticket shares, so a category test would allow the mix for every Z2 ticket.
   - The refusal is wider than Z2-D-106's text, which refuses only when the own category is external: it also refuses a Sharpmake-category ticket whose diff unions a Z2 category. That case has no legitimate author outside PROC either, and refusing both directions keeps the rule one sentence (reconciliation R5).
   - If the human rules Q8 (b) (refuse always), Z2T-PROC-003 implements the plain refusal, `mixedUnionAllowed` and item 4 are dropped with their two fixtures, and the PROC tickets whose diffs reach `Tools/**` as well as `Zenith2/**` are gated by hand ("PROC ticket state machine" in `Zenith2/Docs/Design/PROC.md`).
2. **External (no mixing).** The measure lines are the gate lines that are elements of `measureSteps`; every other line is a verify line. The union validity check runs here (Q12 as ruled by the orchestrator): `measureSteps` must be non-empty, and every element must appear in the selected gate list; otherwise `Write-Result 8 "measureSteps invalid: <reason>"`, with the offending line named in `tick_gates.json`'s `invalidMeasureSteps`, and exit 8 before any line runs. (An add-on-only union — `Zenith2Memory` without `Zenith2` — is refused this way, because its gate list lacks the Configure and Build lines; every real Z2 diff touches `Zenith2/**`, so the base category is always present.)
3. **Sharpmake (no mixing).** Unchanged: `Test-IsZenithBuildLine` and the `run_unit_gate.ps1` regex.
4. **Mixed (allowed).** The measure lines are the elements of `measureSteps` plus the Sharpmake build lines; the unit lines are the `run_unit_gate.ps1` lines; everything else verifies.

### Proofs in external and mixed modes

| Proof | External | Mixed |
|---|---|---|
| created-file regen (`Get-CreatedSourceFiles`, `Build\regen.ps1`) | skipped | run only for created files outside every external category's `paths` |
| `.obj` proof (`Find-CompiledObjectPath`) | skipped (CompileCoverage owns it) | as for regen |
| `.inl` include proof | skipped (CompileCoverage and SourceLayout own it) | as for regen |
| unit-count measurement and exit 10 | never (no `run_unit_gate.ps1` line exists) | only for `run_unit_gate.ps1` lines |
| exit 3 "created files and no build line" | never | only for the Sharpmake-side files |

`PlannedLines` is the measure set; `DeferredLines` the rest, so `skipped` stays correct on every exit path.

### Phase caps

1. A phase stopwatch starts when the phase begins (after the slot is acquired).
2. The effective cap is `phaseTimeoutMinutes` of the record (none when null: Sharpmake categories keep today's unbounded behaviour).
3. Before each line, `tick_gates.ps1` creates a fresh temporary file and passes its path as `ZAGENT_COLD_SIGNAL` in the line's environment; after the line, a non-empty file switches the effective cap of this phase to `coldPhaseTimeoutMinutes` (when set) and records `cold: { applied: true, by: <line>, reasons: [...] }` in `tick_gates.json`; the cold cap applies to the remainder of the phase from its start, so a cold measure gets the larger budget in full ([Z2-N-BUDGET-344]).
4. `Invoke-GateLine` waits for the line with a timeout equal to the remaining phase budget (and the per-line `-TimeoutMinutes` when that is smaller and non-zero). On expiry it kills the tree (`taskkill /T /F /PID`), runs the exe sweep, and, unlike today's per-line watchdog, adds the step to `$script:Steps` with `ExitCode` 124 and `PhaseCapKill = true` before returning, so `tick_gates.json` names the killed line in `steps` ([Z2-N-BUDGET-343]).
5. A 124 step fails the phase with `Write-Result 4 "phase cap: <line> killed after <n> min (cap <cap>, <warm|cold>)"`.
6. The single tree-change retry of verify is a new phase for the cap: it gets its own full budget (Z2-B-PROC-004).
7. The exe sweep in external mode is the category's `cleanup` line when the record names one that starts `pwsh -NoProfile -File` and ends `Sweep.ps1`, run once more after the kill, plus today's root-`Games/*` sweep (harmless for Z2).

### Flake exemptions

`Invoke-GateLineFlakeAware` skips its one-shot re-run when the line is an element of `measureSteps` (a failed CMake build is not run twice inside the gate budget), when the step's `ExitCode` is 124 (a phase-cap or step-timeout kill), and, as today, for a Sharpmake build line.

### Environment of every gate line

`tick_gates.ps1` sets, for every line it starts, in every mode (harmless for lines that ignore them):

| Variable | Value |
|---|---|
| `ZAGENT_GATES_JSON` | the absolute path of the ticket's `gates.json` |
| `ZAGENT_TICKET` | the ticket key |
| `ZAGENT_COLD_SIGNAL` | a fresh temporary file path per line (under `[IO.Path]::GetTempPath()`, never under `.zagent/run/`) |
| `ZAGENT_REVIEW_SIGNAL` | a fresh temporary file path per line |

After a line, a non-empty review-signal file adds its lines to `tick_gates.json` `reviewRequest: { paths: [...], pairs: [...], by: [<line>...] }` (sorted, de-duplicated across lines). A signal line is either a repo-relative path, or `pair <header> <symbol> <consumer libraries comma-joined>`, which goes into `pairs`. The file is the generic channel by which a gate asks for an independent review of paths outside `reviewPaths` ("PROC-API review records"; the Libgraph surface rule).

### New tick_gates.json members

`buildSystem`, `mode` (`external`, `sharpmake`, `mixed`), `phaseCapMinutes` (the effective cap at the end), `cold` (as above or null), `invalidMeasureSteps` (array, normally empty), `reviewRequest` (as above or null). Each is additive; `guard`'s freshness fold reads `state` and `headSha` alone and is unchanged.

### TICK.md text

The `TICK.md` step 6.2 code list ("Codes: `0` green · `2` …") gains "· `5` also: a mixed union of an external and a Sharpmake category on a ticket without `needs-human` · `8` also: an invalid `measureSteps` union" and one sentence: "In external mode a phase is capped at the category's `phaseTimeoutMinutes` (its `coldPhaseTimeoutMinutes` after a gate line signals a cold build); a killed line appears in `steps` with exit 124." The edit is followed by "Harness regeneration".

### Test_TickGates.ps1 fixtures

Through the `-NoRun` dot-source and a fake `zagent` on `PATH` (the existing `New-FakeZagentOnPath` helper) that writes a fixture `gates.json`: `Test_TickGates_ExternalNeverRegens` (an external record with a created `.cpp` runs no `regen.ps1` line), `Test_TickGates_ExternalHonoursReuseGateList`, `Test_TickGates_ExternalVerdictAcceptedByIntegrate` (the written `tick_gates.json` passes `integrate.mjs`'s verdict validator as GREEN), `Test_TickGates_AddOnOnlyUnionRefused` (exit 8), `Test_TickGates_BasePlusAddOnAccepted`, `Test_TickGates_MeasureStepMissingFromUnionRefused` (exit 8 naming the line), `Test_TickGates_MixedUnionRefused` (exit 5), `Test_TickGates_MixedUnionAllowedForNeedsHuman`, `Test_TickGates_PhaseCapKillRecordedInSteps` (a fixture line that sleeps past a 1-minute cap, the test passing `phaseTimeoutMinutes: 1`; the step has exit 124 and the phase exit 4), `Test_TickGates_PhaseCapKillNotFlakeRetried`, `Test_TickGates_MeasureStepNotFlakeRetried`, `Test_TickGates_ColdSignalSwitchesCap`, `Test_TickGates_ReviewSignalRecorded`, `Test_TickGates_SharpmakeRecordUnchanged` (a record with no `buildSystem` produces the same planned lines as before the change).

## Vault pass-through

The generic Vault change that surfaces the external keys (Z2-D-117; `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 3). Every surfaced key is "category value, else project value, else null".

| Function (`Tools/Vault/cli/…`) | Change |
|---|---|
| `legacy-pure/routing.mjs` `resolveRules` | returns, in addition, `buildSystem`, `measureSteps`, `phaseTimeoutMinutes`, `coldPhaseTimeoutMinutes`, `reviewPaths`, `reviewerConventions` and `postFinish` |
| `legacy-pure/routing.mjs` `effectiveGates` | each `unioned[]` record gains `buildSystem` and `measureSteps` of its category |
| `compat.mjs` `gateSelectionOf` | returns the surfaced keys, `measureSteps` as the de-duplicated union (own category first), and `review: { requiredByDiff, paths }`: `paths` is the byte-ordered list of changed paths matching any `reviewPaths` glob of the own category, `requiredByDiff` is `paths.length > 0` |
| `read.js` `dispatchGates` | writes every new member into the `gates` payload, plus `mixedUnionAllowed` (the ticket's labels contain `needs-human`); `client.mjs` writes `gates.json` verbatim, unchanged |
| `compat.mjs` `showPayload` | exposes `reviewPaths`, `reviewerConventions` and `postFinish` under the payload's `rules`, so `ticket.json` at claim carries them |
| `compat.mjs` `PROJECT_FILE_OWNED` | unchanged (the keys are category keys, read from the overlay's categories) |
| `spec/command-matrix.md` | the "gates returns exactly …" row lists the new members; the "no extra fields without a reviewed contract change" row names this change as that contract change |

Validation at config load is not added (A0 found no hook); validity is checked where it matters, by `tick_gates.ps1` at selection ("Classification").

Tests: `test/category-routing.test.mjs` gains `routes external keys from the real zagent.project.json` (a Z2 fixture category resolves every surfaced key; a ZEN category resolves them null); `test/compat-branches.test.mjs`'s `gateSelectionOf` shape test lists the new members; a new `test/gates-external-fields.test.mjs`: `writes measureSteps union`, `writes unioned buildSystem`, `requiredByDiff true only for reviewPaths changes`, `mixedUnionAllowed follows needs-human`, `existing ZEN payload byte-identical apart from null members`.

## Analyzer keys

Generic changes of `Tools/analyze_code_complexity.py` (Z2T-PROC-004; `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" items 4 and 7).

### include_roots

- A profile key `include_roots` (array of repo-relative directory paths). When present, `find_source_files` walks only those directories (each with its own `rglob`), applying `should_exclude` and the generated-file filter as today; when absent, behaviour is unchanged.
- `Tools/complexity_profiles.json` `engine-ci` gains `"include_roots": ["Zenith", "ZenithHub", "Tests", "FluxCompiler", "Docs"]` and `out` in `exclude_dirs`; the A0 measurement (925, 7, 14, 1 and 1 tracked sources under those roots) is the behaviour-preservation evidence, so `Test_IncludeRoots_EngineCiFileSetUnchanged` asserts the analysed file list equals the list obtained without the key, on the live tree, with every `Zenith2/` path removed.
- This fixes the `layering-gate` break (`max-unmapped-files: 0` would count every tracked `Zenith2/` source as unmapped).

### max-file-loc

- `_COMPLEXITY_FAIL_ON_KEYS` gains `max-file-loc`; the per-file loop compares each file's `code_lines` with it and records a violation `{ key: "max-file-loc", file, value, limit }`.
- `Test_MaxFileLoc_FailsAboveLimit` and `Test_MaxFileLoc_UnknownKeyStillRaises` (the existing unknown-key error is kept for every other key).

### exceptions

- A profile key `exceptions` (array of `{ key, entity, limit, decision, comment }`). An over-threshold entity whose `key` and `entity` match an entry and whose value is at most `limit` is not a violation; an entry whose `decision` does not match `^[A-Z][A-Z0-9]*-D-[0-9]{3,}$` is a configuration error (exit 2); an entry whose entity is no longer over the default threshold is listed as stale in the report.
- `Test_Exceptions_SuppressNamedEntity`, `Test_Exceptions_RequireDecision`, `Test_Exceptions_StaleReported`.

### dump-entities

A generic option `--dump-entities <path>` (MAINT's MT3 (j); `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity measurement harness"). It writes every measured entity after the profile's excludes are applied, with the fields MAINT's harness reads, verbatim:

```json
{
    "schema": "complexity-entities",
    "schemaVersion": 1,
    "functions": [ { "file": "<repo-relative>", "name": "<qualified name>", "startLine": 12, "cc": 4, "cognitive": 3, "nesting": 2, "loc": 31, "excepted": false } ],
    "files": [ { "path": "<repo-relative>", "lines": 412, "excepted": false } ]
}
```

- `functions` holds every function the gate measures (not the report's top-N lists), byte-ordered by `file`, then `startLine`; `files` holds every analysed file, byte-ordered by `path`; paths use `/`.
- `excepted` is true when an `exceptions` entry covers the entity; its measured values are kept, because the exception audit needs them.
- `lines` is the physical line count that `max-file-loc` measures.
- The option writes the file and changes nothing else: the exit code and the report stay those of the run without it.
- Tests: `Test_DumpEntities_EveryFunctionListed` (a fixture of 30 functions, where the report's top-N lists hold 10, dumps all 30), `Test_DumpEntities_ExceptedKeepsValues`, `Test_DumpEntities_FileLinesMatchMaxFileLoc`, `Test_DumpEntities_SchemaAndOrder`.

## Scanner audit

The generic fixes needed before any Zenith2 source lands (Z2T-PROC-005; `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 4). Each fix never names Zenith2.

| Scanner | Defect | Fix | Test |
|---|---|---|---|
| the Vault grep-guard test for the retired hosted product name (`Tools/Vault/test/`) | any tracked file whose text contains the four-letter fragment in any case fails, so an identifier such as an anti-aliasing sample count would red every Tooling ticket | a word-boundary pattern plus an explicit allowlist of the known anti-aliasing identifiers, and the board-directory exemption it already has kept | a fixture file holding a sample-count identifier passes; one holding the product name fails |
| the Vault legacy-phrase guard test (`Tools/Vault/test/`) | boards are not exempt | the same board-directory exemption as the first guard (`*/Docs/Board/**`) | a board fixture passes |
| `layering-gate` (via `Tools/CheckComplexity.bat`) | `max-unmapped-files: 0` counts Zenith2 sources | `include_roots` ("Analyzer keys") | as there |
| `Build/zenith_buildsystem.psm1` `Test-ZenithProjectShapingPath` | any `.cpp/.c/.h/.inl/.slang` outside `Middleware/` counts as a project-shaping change, so every Z2 source add or remove makes every checkout's next `zenith build` regenerate | match only paths under the Sharpmake `SourceRootPath`s declared by the Sharpmake projects (the set `regen.ps1` already computes) | a path outside every source root is not shaping; a path under `Zenith/` is |
| `Tools/doc_lint.ps1` C5 | the repo-wide leaf-name search walks the filesystem (including `out/` trees) | search `git ls-files` only | a file present only in an ignored directory is not found |
| `Tools/doc_lint.ps1` C9 | board keys hardcoded | derive them from `vault.config.json` `boardRoots` | a fixture config with an extra board is honoured |
| `Tools/check_project_membership.ps1` | its `.vcxproj` walk reads any `.vcxproj` under any top-level directory | skip directories that contain a `CMakeCache.txt` and accept only Sharpmake-generated projects (the generator banner) | a CMake-generated fixture `.vcxproj` is ignored |
| `Tools/check_boot_unit_assets.ps1` | `git ls-files` repo-wide | restrict to the Zenith roots the script's own constants name | a fixture outside them is ignored |
| `.github/workflows/*.yml` (protected) | every push runs every game job, and the complexity and layering jobs break on Z2 paths | each existing workflow gains a positive `paths:` filter naming the roots its jobs read (`Zenith/**`, `Games/**`, `Tools/**`, `Build/**`, the workflow file itself) | a push touching only `Zenith2/**` triggers none of them (checked by reading the workflow files in `Test-ZagentClient.ps1`'s new assertion `Test_Workflows_PositivePathFilters`) |
| `tick_gates.ps1` created-file regen and `Get-WorkingTreeSha` | covered by the external mode and the nested `Zenith2/.gitignore` | — | — |

## Root delegation

The one deliberate mention of Zenith2 on the Zenith side (Z2-D-030; `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 5).

| File | Edit |
|---|---|
| root `CLAUDE.md` | after the "Overview" section's newcomer note, one line: `` `Zenith2/**` is governed by `Zenith2/CLAUDE.md`. `` |
| `zagent.project.json` `conventions` | the suffix ` Zenith2/** is governed by Zenith2/CLAUDE.md.` |
| `Tools/zagent/Test-ZagentClient.ps1` | gains the new assertion `Test_Zenith2MentionsOutsideZenith2`: `git grep -lE '\bZenith2/'` over every tracked file outside `Zenith2/` returns exactly the allowlist `vault.config.json`, `zagent.project.json`, `CLAUDE.md` (today the only check is the manual `git grep` Z2-D-119 records, so this ticket creates it) |

The universal conventions string is inlined into every worker prompt on every board, so the edit changes every board's prompts by one sentence; the Stage 0 mid-sprint check confirms no other change. After the commit, `git grep -nE '\bZenith2/'` outside `Zenith2/` matches exactly those three files (Z2-D-119 records the two-file state before it).

## libgraph CMake front end

A generic front end for `Tools/libgraph` beside its Sharpmake and `.vcxproj` front ends (Z2-D-007; `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 6).

### Module

`Tools/libgraph/cmake_fileapi.py` exports `build_model_from_fileapi(reply_dir, declared_deps_path, source_root, config, third_party_path=None) -> Model`:

| Model element | Built from |
|---|---|
| `Node` per target | every codemodel target of `config` of type `STATIC_LIBRARY`, `EXECUTABLE`, `SHARED_LIBRARY`, `MODULE_LIBRARY` or `INTERFACE_LIBRARY`; `kind` from the `declared_deps.json` record (`lib` for libraries and backends, `game` for games, `tool` for tools, `test` for test targets); `include_dirs` from the target's compile groups; `out_lib` from `artifacts`; `function_level_linking` from the compile fragments (`/Gy-` present means false) |
| `FileOwner` per source | the compile-group sources (`how: "listed"`); headers owned by the library whose directory contains them (`how: "dir-fallback"`) |
| `DeclaredEdge` per dependency | `declared_deps.json` `depends` (and `testDepends` for test targets), never the File API's transitive link list |
| third-party ownership | `third_party_targets.json`: each entry's include directories and its `owner` |

### CLI

`libgraph.py` gains `--front-end sharpmake|cmake-fileapi` (default `sharpmake`, so existing invocations are unchanged), `--reply`, `--declared-deps`, `--third-party`, `--source-root`, `--gate <policy.json>`, `--shipping-symbols <json>`, `--shipping-config <config>`, `--shipping-reply <dir>` and `--write-baseline <path>`; with `--gate`, the run evaluates the policy's rules after the report and exits 4 when any violation exists, writing `violations.json`. The Zenith2 policy file and its rule set are `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph", "Libgraph policy files"; the front end knows nothing of them.

### Tests

`Tools/libgraph/tests/test_cmake_fileapi.py`: `test_targets_become_nodes`, `test_declared_edges_from_declared_deps_not_link_closure` (a fixture whose File API lists a transitive dependency the declared record lacks yields an undeclared-edge finding), `test_interface_library_has_no_symbol_stage`, `test_gy_minus_detected_from_fragments`, `test_gate_mode_exit_4`, `test_sharpmake_default_unchanged`; `tests/CONTRACT.md` gains the front end's contract section; the fixtures are hand-written reply JSON trees under `tests/fixtures/cmake_fileapi/`.

## PROC-API review records

API review enforced from the diff (Z2-D-049; `Zenith2/Docs/Contracts.md` → "Review and change management"). The six elements of that section's table, specified:

### 1. reviewPaths and requiredByDiff

- `reviewPaths` is a generic category key (array of globs). `zagent gates` (run at measure with the changed set) writes `review: { requiredByDiff, paths }` into `gates.json` ("Vault pass-through").
- The Z2 value is the five globs of `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket" (Z2T-PROC-016 edit), which adds `Zenith2/ThirdParty/licence_policy.json` (SHIP-P3) to the four A1 globs ([Z2-N-CONTRACT-492]).

### 2. TICK.md step 6.3

The step's first sentence becomes: "Run the REVIEWER when `review.required` is true in the payload, OR when `gates.json` has `review.requiredByDiff` true, OR when `tick_gates.json` has a non-empty `reviewRequest`, OR when your entry point's `Review policy` reads `always`." The prompt instruction gains: "Name in the prompt every path of `review.paths` and of `reviewRequest.paths`; the reviewer must read each in full in the working tree as well as in the patch." Followed by "Harness regeneration" and the allowlist re-derivation.

### 3. zagent review record

A new write verb:

```
zagent review record <KEY> --verdict pass|fixes-applied --reviewer <tier> [--comment <file>] [--json]
```

- `COMMAND_FLAGS` gains `review: ["verdict", "reviewer", "comment"]`; `isWriteCommand` and `dispatchWrite` route `review record`; `--reviewer` is one of the tier names `low`, `medium`, `high` of the reviewer roster; anything else is exit 1 (usage).
- The writer computes the hash of [Z2-N-CONTRACT-493] over the record's path set: the union of `gates.json`'s `review.paths` and `tick_gates.json`'s `reviewRequest.paths` for the ticket (read from `.zagent/run/<KEY>/` by the client), each line `<path> <git blob SHA-1 of the working-tree content>` (`git hash-object <path>`, or `deleted`), sorted by byte order, joined with `\n`, SHA-256 of the UTF-8 text.
- The record is appended to the ticket body as a sentinel block after the work-log block (and outside the spec, so `specOf` cuts the spec at the first of `<!-- agent:worklog:start` and `<!-- agent:review:start`):

```markdown
<!-- agent:review:start Z2-123 -->
- recordedAt: 2026-10-20T10:11:12Z
- verdict: pass
- reviewer: high
- headSha: 4be7…
- hash: 9f2c…
- paths:
  - Zenith2/Engine/ZenithCore/Zenith_Heap.h 3b18e512dba79e4c8300dd08aeb37f8e728b8dad
- pairs:
  - ZenithECS/Zenith_Query.h Zenith_Query::Next ZenithAI,ZenithScript
<!-- agent:review:end -->
```

- `pairs` lists, one per line as `<header> <symbol> <consumer libraries comma-joined>`, every inbound-surface pair of `tick_gates.json`'s `reviewRequest.pairs` the review covered; it is empty or absent when no surface growth was requested. A pair is covered by any pass-verdict (`pass` or `fixes-applied`) block of a tracked ticket that lists it, whatever subsequent edits touch its consumer files, so a reviewed pair is never re-requested (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "The inbound-surface review rule"). The `hash` over the path blobs, which `finish` checks ([Z2-N-CONTRACT-493]), is unchanged.

- Each `review record` appends a new block (records are never rewritten), in one board transaction like `finish`; `zagent history <KEY>` lists each record as a history entry `review-record`.
- The verb is refused (exit 4) when the ticket is not In Progress, or when the path set is empty.

### 4. Finish refusal

`zagent finish <KEY> --status Done` refuses with exit 4 when `requiredByDiff` is true or `reviewRequest` is non-empty and no review block's `hash` equals the hash recomputed now over the same path set; the refusal message names each path whose current blob differs from the newest record's line ([Z2-N-CONTRACT-495]). `--status Blocked` and `In Review` are never refused for this reason. A human closing with `--human` is refused the same way: the rule is about the diff, not the actor.

**Human-run tickets.** A PROC ticket or a stage acceptance ticket whose diff reaches `reviewPaths` (a stage acceptance always does: it commits `libgraph_baseline.json` and, from Stage 2, the MAINT release and roll change `Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h`) gets its review the same way a tick does, so no category is exempt:

1. With the ticket In Progress and its diff staged, the human runs `zagent gates <KEY>` (so `gates.json` names `review.paths`), writes the patch with `git diff --cached > .zagent/run/<KEY>/review.patch`, and dispatches the reviewer of the `high` tier from a Claude Code session in the main checkout (`zagent-reviewer-high`, prompt: the patch path and every path of `review.paths`, read in full).
2. When the reviewer reports findings, the human fixes them, re-stages and repeats step 1.
3. `zagent review record <KEY> --verdict pass --reviewer high` (or `fixes-applied` after step 2), then `zagent finish <KEY> --status Done --human` succeeds.

The acceptance tickets name this in their Definition of Done ("Acceptance ticket shape" of `Zenith2/Docs/Design/PROC/Acceptance.md`).

### 5 and 6. Tests

Vault tests in `test/review-record.test.mjs`: `records hash over reviewPaths blobs`, `finish refuses without a record`, `finish accepts a matching record`, `post-review edit to a reviewed header is refused at finish` (the required test of element 5: edit a header after `review record`, then `finish --status Done` exits 4 naming the header), `deleted path hashed as deleted`, `gate-requested review paths included`, `review blocks preserved by update --body` (a reconcile does not drop them), `specOf ignores review blocks`; and `Tools/Test_TickGates.ps1`'s `Test_TickGates_ReviewSignalRecorded`.

## Reviewer conventions

A generic per-category `reviewerConventions` field inlined into the reviewer prompt (`Zenith2/Docs/Contracts.md` → "Review and change management" [Z2-N-CONTRACT-496]; the A0 verification found that category conventions never reach the reviewer).

- The key surfaces through `resolveRules` and `showPayload` ("Vault pass-through") into `ticket.json` at claim.
- `TICK.md` step 6.3 gains: "When `ticket.json` carries `rules.reviewerConventions`, inline it verbatim in the REVIEWER prompt, before `review.reasons`, under the heading-free lead-in `Category reviewer conventions:`." The reviewer charter `Tools/zagent/protocol/roles/reviewer.md` gains one sentence: "Your prompt may carry category reviewer conventions; they bind you like this charter." Followed by "Harness regeneration".
- The Z2 value is the string of `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket" (Z2T-PROC-019 edit); it names the "Review focus" list of every ticket's `## Validation` section ([Z2-N-CONTRACT-497]) and the review-only obligations of [Z2-N-CONTRACT-576].
- Tests: `test/category-routing.test.mjs` `surfaces reviewerConventions`; `Test-HarnessParity.ps1` asserts the generated reviewer prompts contain the inlining instruction.

## postFinish

A generic per-category `postFinish` command run after `finish` (`Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 13; Z2-D-102).

- `postFinish` is an array of command lines, each starting with a pre-authorised prefix (`pwsh -NoProfile`), surfaced by "Vault pass-through".
- `TICK.md` step 8 gains, after the `zagent finish` call: "**8a. Post-finish.** When the ticket finished `Done` and `ticket.json` carries `rules.postFinish`, run each line in order from the repository root, foreground, with a 15-minute limit per line, appending its output to `.zagent/run/<KEY>/postfinish.log`. A non-zero exit is reported in the tick's final report and never changes the ticket's status (it is already Done) or the next disposition. In lane mode run the lines from the main checkout's root (`zagent lane which --json` `mainPath`), after `integrate` landed, because the lines read the base branch. These lines are board writes, so they come after `finish`, never between measure and guard." Followed by "Harness regeneration".
- The Z2 value is two lines (Z2T-PROC-017 edit): `pwsh -NoProfile -File Zenith2\Tools\Backlog\File-Backlog.ps1 --mark-stale` and `pwsh -NoProfile -File Zenith2\Tools\Backlog\File-Backlog.ps1 --check-headroom`. Their behaviour is `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Production filer" (subsections "--mark-stale" and "--check-headroom"). The second implements [Z2-N-BUDGET-353]'s automatic headroom ticket with no `TICK.md` edit (ruling PROC-R2 M9).
- Tests: Vault `surfaces postFinish`; `Test-HarnessParity.ps1` asserts each harness's entry point carries step 8a and pre-authorises its prefix.

## Rulings citation weight

A direct decision-ID citation is self-sufficient in the rulings matcher (row X22; `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables", the paragraph after item 17; Z2-D-098).

- `Tools/Vault/cli/legacy-pure/ticket.mjs` `matchGoverningRulings`: an entry whose ID is in `citedIds` (the `<prefix>-D-NNN` IDs cited in the Goal and Definition of Done) scores at least `minScore` regardless of its noun and identifier hits, so it is selected by both callers (`RULINGS_FILE_MIN_SCORE` 4 for `rulings.md`, `REVIEW_RULING_MIN_SCORE` for the review term).
- Ordering: cited entries sort before uncited ones (cited in citation order of first appearance), then the existing score order, before the 25-entry `limit` truncates, so a ticket citing many decisions never loses a cited one to a high-scoring uncited one.
- Tests in `test/claim-rulings-and-create-scan.test.mjs`: `bare citation selects its entry`, `cited entries sort first`, `25-entry limit keeps every cited entry` (a fixture citing 3 IDs among 30 strong uncited matches), `uncited behaviour unchanged`.

## Lane provisioning hook

A generic per-project provisioning hook for `zagent lane add` (the human's ruling Q3 (a) of 2026-10-16; `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 16).

### Configuration

`zagent.project.json` `lanes` gains `provision`, keyed by board project:

```json
"provision": {
  "Z2": {
    "copyIgnored": [],
    "regen": false,
    "provisionGames": false,
    "steps": [
      "pwsh -NoProfile -File Zenith2\\Tools\\Setup\\Provision-Lane.ps1"
    ]
  }
}
```

| Member | Default (project absent) | Meaning |
|---|---|---|
| `copyIgnored` | `["**"]` | globs of ignored files copied from the main checkout (today's copy set is every ignored file; a list narrows it; `[]` copies nothing) |
| `regen` | `true` | run `Build/regen.ps1 -AllowLinkedWorktree` |
| `provisionGames` | `true` | build and unit-gate every game of `Tools/unit_baselines.json` |
| `steps` | `[]` | command lines run after the steps above, from the lane root, each with a pre-authorised prefix, with `ZAGENT_LANE_ROOT`, `ZAGENT_MAIN_ROOT` and `ZAGENT_LANE_PROJECT` set; a non-zero exit fails provisioning |

### Changes

- `Tools/lane_init.ps1` gains `-Project <KEY>`; `Invoke-LaneInit` reads the lane's own `zagent.project.json` `lanes.provision.<KEY>` and applies it; `-SkipAssets` and `-SkipProvision` keep their meaning and win over the configuration (a human override). A project without an entry behaves exactly as today.
- `Tools/Vault/cli/lanes.mjs` `laneAdd` passes `-Project <PROJECT>` to `lane_init.ps1`.
- Tests: `Tools/Test_LaneInit.ps1` `Test_LaneInit_ProvisionEntryHonoured` (a fixture entry with `regen: false` runs no regen), `Test_LaneInit_NoEntryUnchanged`, `Test_LaneInit_StepFailureFails`; `Tools/Vault/test/lanes-add.test.mjs` `passes -Project to lane_init`.

### The Z2 step

`Zenith2/Tools/Setup/Provision-Lane.ps1` (unprotected; created by Z2T-PROC-021):

1. `git lfs install --local`, then `git config lfs.fetchexclude "Zenith2/Docs/Research/**"` (RESEARCH's extended P-RS4: the research archive's LFS objects are never fetched into a lane), then `git lfs pull` in the lane (ruling GIT-LFS-PREREQ; Z2-R-SHIP-154).
2. `Setup-Zenith2.ps1 -Check` (the toolchain and the dependency cache are machine-wide, so a new lane installs nothing); a failure prints the setup command and exits 1.
3. Compute the lane's checkout hash with `Get-Zenith2CheckoutHash` and print it.
4. Warm the lane, only when `Zenith2/CMakePresets.json` exists in the lane: `pwsh -NoProfile -File Zenith2/Tools/Gates/Configure.ps1`, then `Build.ps1 -Preset <p>` for every preset of the warm list (below). Without the presets file (the lane is registered before the scaffold creates it), the step prints `Provision-Lane: no scaffold yet; warm-up skipped` and continues with exit 0, so registration never depends on the scaffold it precedes.
5. Measure the lane's storage: the size of the worktree (`git ls-files` bytes plus the LFS objects) and of its `Zenith2/out/<hash>/`, written to `${ZENITH_OUT}/gates/lane-storage.json` `{ "schema": "zenith2.lane-storage", "schemaVersion": 1, "worktreeBytes", "outBytes", "warmed": [<presets>], "measuredUtc" }`. Before the scaffold, `outBytes` is small and `warmed` empty; the measurement Z2-B-PROC-001 is allocated from is the Stage 0 acceptance's re-run (below).

The script also runs on its own, from the lane root, as `pwsh -NoProfile -File Zenith2/Tools/Setup/Provision-Lane.ps1 -WarmOnly` (steps 3 to 5). The warm list is every preset an active gate line of the base category builds:

| From | Presets warmed |
|---|---|
| the scaffold (Stage 0) | `win-debug`, `win-release`, `win-shipping-test`, `win-shipping` (the `Libgraph` LG9 build) |
| Z2T-PROC-040 (Stage 2) | adds `android-arm64`, `android-arm64-hwasan` |
| Z2T-PROC-041 (Stage 2) | adds `win-asan`; `linux-tsan` is warmed in its WSL `binaryDir` |

The list is the `warmPresets` array of `Zenith2/Tools/Gates/step_timeouts.json` (protected), so the PROC ticket that activates a building step extends it in the same diff and its DoD runs `Provision-Lane.ps1 -WarmOnly` in the Z2 lane, which turns the first build of the new tree into a provisioning cost instead of a ticket's verify cost. The Stage 0 acceptance runs `-WarmOnly` in the Z2 lane after the scaffold's tick and allocates Z2-B-PROC-001 from that `lane-storage.json` (worktree plus out-tree bytes, in GB rounded up to one decimal, plus 20 % headroom).

## Z2 lane registration

Z2T-PROC-022, after Z2T-PROC-003, -008, -018 and -021 (Z2-D-029):

1. Every lane idle; the main checkout on `master`, clean.
2. `pwsh -NoProfile -File Tools/zagent/zagent.ps1 lane add Z2 --json` (the default root `C:/dev/Zenith-lanes/Z2`); it creates the worktree, runs `lane_init.ps1 -Project Z2` and registers the lane.
3. `zagent lane list --json` shows `Z2`; `zagent lane which --json` run inside the lane reports `mode: lane, project: Z2`.
4. Start `claude` once in the lane directory and accept the workspace-trust dialog (otherwise every `permissions.allow` entry is ignored there, root `CLAUDE.md` "Parallel lanes").
5. Record the output of steps 2 and 3 and the lane's `lane-storage.json` (warm-up skipped) in the work log. The ticket commits nothing to `Zenith2/`; Z2-B-PROC-001 is allocated by the Stage 0 acceptance after the scaffold has been ticked in this lane ("The Z2 step").

## Revalidation claim predicate

The Vault PROC change that makes `needs-revalidation` affect claiming (Z2-D-102; A0 candidate (s)), Stage 1 (Z2T-PROC-030):

- `compat.mjs`'s queue predicate and the targeted-claim predicate treat `needs-revalidation` like `deferred` for the queue (skipped) and like `needs-human` for a targeted claim (refused, exit 4, reason `needs revalidation: run the filer's reconcile mode`).
- The label table of `TICK.md` gains the row; root `CLAUDE.md`'s marker table gains it; "Harness regeneration".
- The Z2 `--mark-stale` keeps applying `deferred` as well until this lands; afterwards the filer stops applying `deferred` with it, in the same ticket's diff of `File-Backlog.ps1`, and reconcile removes only `needs-revalidation`.
- Tests: `test/cli-queue.test.mjs` `needs-revalidation skipped by queue`, `targeted claim of needs-revalidation refused`.

## N lanes per project

Flagged proposal P2 is recorded and not applied (Z2-D-130). Following the P1 precedent, nothing realising it is filed: the requirement Z2-R-PROC-120 is classified `research` (pending the human's approval of P2) and has no ticket, no stage acceptance lists it, and Z2-D-029 and `Zenith2/CLAUDE.md` ("one Z2 ticket runs at a time") stay the settled text. If the human approves P2, the approval files one PROC `needs-human` ticket with the specification below and moves the requirement to `required` in the stage the approval names; if the human rejects it, the requirement becomes `excluded` with the rejection's decision ID.

<!-- assumption: pending flagged proposal P2 -->

- **Registry key.** `.git/vault/lanes.json` keys lanes by lane name instead of project: `lanes: { "<laneName>": { project, path, createdAt, provisionedAt } }`; a lane name is `<PROJECT>` for the first lane and `<PROJECT>-<n>` (n = 2 … 9) for the others. A registry written by today's code (keyed by project) is read as lane names equal to projects, so no migration is needed.
- **Configuration.** `lanes.maxPerProject` (object, project → integer; default 1); Z2's value is 3.
- **Commands.** `zagent lane add <PROJECT> [--slot <n>]` (`LANE_ALLOWED_FLAGS.add` gains `slot`) creates lane `<PROJECT>-<n>` at `<root>/<PROJECT>-<n>`; refused when the project already has `maxPerProject` lanes. `lane remove <laneName>`; `lane which` reports `{ mode, project, lane, path, inFlight }`.
- **In-flight scoping.** The lane-mode in-flight predicate is per lane path: each lane may hold one agent-owned In Progress ticket of its project; `claim` and `next` in a lane never take a ticket another lane of the same project holds (the existing assignee rule already excludes it); `integrate` is unchanged (one landing at a time under the operation lock).
- **Conflicts.** Two lanes of one project editing overlapping files land in order; the second `integrate` reports `CONFLICT` and the existing `integration-conflict` handling applies. The Z2 mid-sprint checks record the conflict rate.
- **Builds.** Each lane has its own checkout hash, so its own `Zenith2/out/<hash>/` and WSL `binaryDir`; builds serialise on the one machine-wide slot (`Tools/build_lock.ps1`).
- **Protocol text.** `TICK.md`'s lane-mode section replaces "one lane per project" with the per-lane wording; root `CLAUDE.md`'s "Parallel lanes" section likewise; "Harness regeneration".
- **Tests.** `test/lanes-add.test.mjs` `second lane with --slot`, `maxPerProject enforced`, `project-keyed registry read as lane names`; `test/lanes-claim.test.mjs` `two lanes of one project each hold one ticket`.
<!-- /assumption -->

## CI workflow

`.github/workflows/zenith2-gates.yml` (Z2-D-031; Z2T-PROC-015) is written exactly as `Zenith2/Docs/Design/SHIP/Qualification.md` → "CI matrix" specifies (triggers, the `plan`, `windows` and `linux` jobs, their steps and invariants), calling `Run-Committed.ps1 -ListJobs -Json` and `-Host windows|linux` (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "Run-Committed"). The file is protected (`.github/**`), so the ticket is `needs-human`; SHIP's `Pwsh_Ship_CiWorkflowShape` (registered in Stage 1) asserts the shape, and every subsequent PROC ticket that edits the workflow keeps it green. The Stage 0 acceptance requires a green run on a push containing the scaffold.
