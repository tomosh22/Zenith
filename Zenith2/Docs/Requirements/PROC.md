# PROC requirement catalogue

This is the requirement catalogue of the PROC area (`Z2 PROC — Process & gates`). Each requirement is one testable obligation with its inventory coverage, stage, classification, governing decisions, budget rows, acceptance evidence and the design headings of `Zenith2/Docs/Design/PROC.md` and its split files that specify it.

## Purpose

The requirements of the PROC area (Process & gates): every protected harness, configuration and gate edit of the Zenith2 programme, each delivered as a `needs-human` PROC ticket, and the stage acceptance and landing procedures PROC owns. The design is `Zenith2/Docs/Design/PROC.md` and its split files. Every requirement below has the eight labelled lines; evidence names the test, run or record that proves it. Revision 1 applied the orchestrator's ruling PROC-R1-RULINGS and the post-round applied PROC-R2-RULINGS. No requirement is attached to a ticket whose stage comes after its own.

## Requirements

### Z2-R-PROC-001 Stage 0 waits for the ZEN tooling tickets without links

- **Statement:** No Z2 ticket is linked to or from a ZEN ticket (Z2-D-112); the gate "ZEN-114 to ZEN-119 Done before any Zenith2 work" is enforced by the Roadmap Stage 0 entry criterion, by the human's `sprint start` of Stage 0, and by the first Definition-of-Done item of Z2T-PROC-001, six read-only `zagent show ZEN-<n> --json` probes that each require `status: Done`; every other Stage 0 PROC ticket is `blocks`-linked from Z2T-PROC-001.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-112, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** Z2T-PROC-001's work log holding the six probe outputs, and `zagent links` on Z2T-PROC-001 showing no cross-project link, both checked at the Stage 0 acceptance
- **Design:** `Zenith2/Docs/Design/PROC.md` → "Reconciliations with the core documents"; `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "Order of the landing"; `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "After the seed"

### Z2-R-PROC-002 Global guardrails gain the gate directory, the repository guide and the research evidence

- **Statement:** Both `policy.board.guardrails.protectedPaths` and `policy.resolved.guardrails.protectedPaths` of `vault.config.json` end with `Zenith2/Tools/Gates/**`, `Zenith2/CLAUDE.md`, `Zenith2/Docs/Research/**/runs/**` and `Zenith2/Docs/Research/Machines/**` (RESEARCH's P-RS6), and no Z2 project or category carries a `protectedPaths` key.
- **Covers:** Z2-I-PROC-01, Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-118, Z2-D-089
- **Budgets:** none
- **Acceptance evidence:** Z2T-PROC-001 diff; `zagent guard` refusing a list naming `Zenith2/Tools/Gates/Configure.ps1` and one naming `Zenith2/Docs/Research/Machines/x.json`; Stage 0 acceptance
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Guardrail additions"

### Z2-R-PROC-003 The project-file guardrail copy stays a subset

- **Statement:** The top-level `protectedPaths` of `zagent.project.json` gains the same four strings and stays a strict subset of the resolved list.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-089
- **Budgets:** none
- **Acceptance evidence:** `Test-HarnessParity.ps1` and `Test-ZagentClient.ps1` passing in Z2T-PROC-001
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Guardrail additions"

### Z2-R-PROC-004 The parity gate requires the new protections

- **Statement:** `Test-HarnessParity.ps1` `$mustBeProtected` lists the four guardrail strings, so dropping any of them from the resolved list fails the parity gate.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-118
- **Budgets:** none
- **Acceptance evidence:** a negative run of `Test-HarnessParity.ps1` against a scratch config without the entries, recorded in Z2T-PROC-001
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Guardrail additions"

### Z2-R-PROC-005 Every protocol or guardrail change regenerates all three harnesses

- **Statement:** A PROC ticket that changes `TICK.md`, a role charter, the generator or the resolved protected-path list regenerates the claude, opencode and codex artifacts in the same commit, re-derives the tier-mention allowlist when line numbers shift, and leaves `Build-HarnessArtifacts.ps1 -Check` clean for each harness.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-118, Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `-Check` exit 0 three times and `Test-HarnessParity.ps1` green in each of Z2T-PROC-001, -003, -016, -017, -019, -030, -031
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Harness regeneration"

### Z2-R-PROC-006 External mode splits lines by measureSteps

- **Statement:** For a ticket whose own category declares `buildSystem: "external"`, `tick_gates.ps1` makes the elements of `measureSteps` the measure lines and every other line a verify line, with no Sharpmake line classification.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028, Z2-D-117
- **Budgets:** none
- **Acceptance evidence:** `Test_TickGates_BasePlusAddOnAccepted` in `Tools/Test_TickGates.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"

### Z2-R-PROC-007 External mode skips the Sharpmake proofs

- **Statement:** In external mode `tick_gates.ps1` runs no created-file regen, no `.obj` proof and no `.inl` proof, and never exits 10.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-105
- **Budgets:** none
- **Acceptance evidence:** `Test_TickGates_ExternalNeverRegens`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"

### Z2-R-PROC-008 measureSteps are validated against the selected union

- **Statement:** Before any line runs, an external union whose `measureSteps` is empty or names a line absent from the selected gate list is refused with exit 8, the offending line recorded in `invalidMeasureSteps`.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-086
- **Budgets:** none
- **Acceptance evidence:** `Test_TickGates_AddOnOnlyUnionRefused`, `Test_TickGates_MeasureStepMissingFromUnionRefused`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"

### Z2-R-PROC-009 A mixed union needs the needs-human opt-in (pending Q8)

- **Statement:** (pending Q8; default draft: the orchestrator's recommendation (a)) A union of an external and a Sharpmake category is refused with exit 5 before any line runs unless `gates.json` has `mixedUnionAllowed` true, which holds exactly when the ticket carries `needs-human`; the refusal covers both directions of the mix.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-106
- **Budgets:** none
- **Acceptance evidence:** `Test_TickGates_MixedUnionRefused`, `Test_TickGates_MixedUnionAllowedForNeedsHuman`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"; `Zenith2/Docs/Design/PROC.md` → "Reconciliations with the core documents"

### Z2-R-PROC-010 Mixed classification when allowed (pending Q8)

- **Statement:** (pending Q8) In an allowed mixed union the measure lines are the `measureSteps` plus the Sharpmake build lines, and the Sharpmake-only proofs apply only to created files outside every external category's `paths`; if Q8 is ruled (b) this requirement is withdrawn with its fixture.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-106
- **Budgets:** none
- **Acceptance evidence:** `Test_TickGates_MixedUnionAllowedForNeedsHuman` asserting the planned lines and the regen scope
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"

### Z2-R-PROC-011 No flake re-run for measure lines or kills

- **Statement:** `Invoke-GateLineFlakeAware` never re-runs a `measureSteps` line or a line whose exit code is 124.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** Z2-B-PROC-004
- **Acceptance evidence:** `Test_TickGates_MeasureStepNotFlakeRetried`, `Test_TickGates_PhaseCapKillNotFlakeRetried`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"

### Z2-R-PROC-012 The warm phase cap kills and records the line

- **Statement:** When a phase exceeds `phaseTimeoutMinutes`, `tick_gates.ps1` kills the running line's process tree, records the line in `steps` with exit 124, sweeps, and fails the phase with exit 4; the tree-change retry gets its own cap.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** Z2-B-PROC-002, Z2-B-PROC-003, Z2-B-PROC-004
- **Acceptance evidence:** `Test_TickGates_PhaseCapKillRecordedInSteps`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"

### Z2-R-PROC-013 The cold cap applies after a cold signal

- **Statement:** A gate line that writes to the file `ZAGENT_COLD_SIGNAL` names switches the phase's cap to `coldPhaseTimeoutMinutes` from the phase start, and `tick_gates.json` records `cold.applied`, the line and the reasons.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** Z2-B-PROC-005
- **Acceptance evidence:** `Test_TickGates_ColdSignalSwitchesCap`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Build"

### Z2-R-PROC-014 Gate lines receive the ticket context

- **Statement:** Every gate line `tick_gates.ps1` starts receives `ZAGENT_GATES_JSON`, `ZAGENT_TICKET`, and fresh `ZAGENT_COLD_SIGNAL` and `ZAGENT_REVIEW_SIGNAL` paths outside `.zagent/run/`, and a non-empty review signal becomes `reviewRequest` in `tick_gates.json`.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `Test_TickGates_ReviewSignalRecorded`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"

### Z2-R-PROC-015 The verdict machinery is unchanged

- **Statement:** External mode keeps the build slot, tree SHAs, `-ReuseGateList`, `Write-Result` and the exit ladder 0/2/3/4/5/8/10/11/12, and its verdict is accepted by `integrate` as GREEN; a record without `buildSystem` behaves byte-identically to before.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** none
- **Acceptance evidence:** `Test_TickGates_ExternalVerdictAcceptedByIntegrate`, `Test_TickGates_ExternalHonoursReuseGateList`, `Test_TickGates_SharpmakeRecordUnchanged`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"

### Z2-R-PROC-016 tick_gates fixtures cover external mode

- **Statement:** `Tools/Test_TickGates.ps1` carries the fourteen external-mode fixtures named in the design and passes in the Tooling category gates.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** none
- **Acceptance evidence:** the Tooling gate run of Z2T-PROC-003
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode"

### Z2-R-PROC-017 resolveRules surfaces the external keys

- **Statement:** `resolveRules` returns `buildSystem`, `measureSteps`, `phaseTimeoutMinutes`, `coldPhaseTimeoutMinutes`, `reviewPaths`, `reviewerConventions` and `postFinish`, category value first, then project, else null.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-117
- **Budgets:** none
- **Acceptance evidence:** `test/category-routing.test.mjs` "routes external keys from the real zagent.project.json"
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Vault pass-through"

### Z2-R-PROC-018 zagent gates writes the external fields

- **Statement:** `zagent gates` writes into `gates.json` the surfaced keys, the union of `measureSteps`, each unioned category's `buildSystem`, `review.requiredByDiff` with `review.paths`, and `mixedUnionAllowed`.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-117, Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `test/gates-external-fields.test.mjs`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Vault pass-through"

### Z2-R-PROC-019 The command matrix records the payload change

- **Statement:** `Tools/Vault/spec/command-matrix.md` lists the new `gates` members in the same diff as the code, as the reviewed contract change its own rows require.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-117
- **Budgets:** none
- **Acceptance evidence:** the Z2T-PROC-002 diff
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Vault pass-through"

### Z2-R-PROC-020 Existing projects keep their gates payload

- **Statement:** For every ZEN and ZM ticket the `gates` payload differs from before only by the new members set to null or false.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-044
- **Budgets:** none
- **Acceptance evidence:** "existing ZEN payload byte-identical apart from null members" and the Stage 0 mid-sprint comparison
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Vault pass-through"

### Z2-R-PROC-021 The analyzer honours include_roots

- **Statement:** A profile `include_roots` key restricts the analyzer's walk to the named directories, and `engine-ci` carries `Zenith`, `ZenithHub`, `Tests`, `FluxCompiler` and `Docs` with an unchanged analysed file set.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `Test_IncludeRoots_EngineCiFileSetUnchanged` in `Tools/tests/test_complexity.py`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Analyzer keys"

### Z2-R-PROC-022 The analyzer gains max-file-loc and exceptions

- **Statement:** The analyzer accepts the `fail_on` key `max-file-loc` and a profile `exceptions` list whose entries each name a key, an entity, a limit and a decision ID, an entry without a valid decision being a configuration error.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `Test_MaxFileLoc_FailsAboveLimit`, `Test_Exceptions_RequireDecision`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Analyzer keys"

### Z2-R-PROC-023 The scanner audit fixes every scanner that sees Zenith2

- **Statement:** The two Vault grep-guard tests, `Test-ZenithProjectShapingPath`, `doc_lint.ps1` C5 and C9, `check_project_membership.ps1` and `check_boot_unit_assets.ps1` are fixed generically, each with its test, before any Zenith2 source lands.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-100, Z2-D-044
- **Budgets:** none
- **Acceptance evidence:** the tests named in the scanner table, green in Z2T-PROC-005
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Scanner audit"

### Z2-R-PROC-024 Existing workflows gain positive paths filters

- **Statement:** Every existing `.github/workflows/*.yml` gains a `paths:` filter naming the roots its jobs read, so a push touching only `Zenith2/**` triggers none of them.
- **Covers:** Z2-I-PROC-02, Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-031
- **Budgets:** none
- **Acceptance evidence:** `Test_Workflows_PositivePathFilters` in `Test-ZagentClient.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Scanner audit"

### Z2-R-PROC-025 The root delegation line

- **Statement:** The root `CLAUDE.md` and the universal `conventions` string state "`Zenith2/**` is governed by `Zenith2/CLAUDE.md`.", and the blindness assertion allows `Zenith2/` outside `Zenith2/` in exactly `vault.config.json`, `zagent.project.json` and `CLAUDE.md`.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-030
- **Budgets:** none
- **Acceptance evidence:** `git grep -nE` result and `Test-ZagentClient.ps1` in Z2T-PROC-006
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Root delegation"

### Z2-R-PROC-026 libgraph reads the CMake File API

- **Statement:** `Tools/libgraph/cmake_fileapi.py` builds libgraph's model from a File API reply and `declared_deps.json`, taking declared edges from the declared record, never from the transitive link list.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `test_declared_edges_from_declared_deps_not_link_closure`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "libgraph CMake front end"

### Z2-R-PROC-027 libgraph has a gate mode

- **Statement:** `libgraph.py --gate <policy>` evaluates a policy file and exits 4 on any violation, writing `violations.json`, while the default Sharpmake front end is unchanged.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `test_gate_mode_exit_4`, `test_sharpmake_default_unchanged`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "libgraph CMake front end"

### Z2-R-PROC-028 The Libgraph step enforces the Z2 rules

- **Statement:** The `Libgraph` step fails on rules LG1 to LG9 and LG11 over the `win-debug` build and reports LG10 as specified.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007, Z2-D-042
- **Budgets:** Z2-B-PROC-014
- **Acceptance evidence:** the Libgraph fixtures LG1 to LG11 and `Pass-Minimal`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph"

### Z2-R-PROC-029 Surface growth is measured per ticket and covered by review

- **Statement:** With ticket context, LG10 measures growth against the parent's cached surface keyed by the surface inputs only (never `Zenith2/Docs/**`), or on a miss against the pairs whose header, consumer path or changed-header include closure is in the changed set; a pair listed in a pass-verdict review block's `pairs` is covered whatever subsequent edits touch its consumers; uncovered pairs are requested by identity and path; without ticket context an uncovered pair against the baseline fails; LG10 is report-only while the baseline is unset.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `LG10-GrowthWithContext`, `LG10-GrowthCoveredByRecord`, `LG10-ReviewedPairNotRerequested`, `LG10-ParentCacheHit`, `LG10-BoardOnlyCommitHits`, `LG10-ParentCacheMiss`, `LG10-ParentCacheMissTransitiveHeader`, `LG10-GrowthNoContext`, `LG10-BaselineNullReportOnly`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "The inbound-surface review rule"

### Z2-R-PROC-030 Shipping closure and forbidden symbols

- **Statement:** The `Libgraph` step fails a Shipping closure holding a compiler, test or tool target, and a Shipping game executable defining a `shipping_forbidden_symbols.json` symbol, running the symbol check whenever a Shipping input changed or no ticket context exists.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `LG8-CompilerInShipping`, `LG9-ForbiddenSymbol`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph"; `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph policy files"

### Z2-R-PROC-031 Swap-contract reports

- **Statement:** Libgraph writes `swap_<family>.json` for ecs, rhi, physics, compression and online on every run and fails a consumer using a family symbol outside the API library.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `LG11-SwapBreak`; the reports attached to the Stage 3 acceptance
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph"

### Z2-R-PROC-032 libgraph front-end tests

- **Statement:** The front end carries its contract section and the six tests named in the design under `Tools/libgraph/tests/`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `python -m pytest Tools/libgraph/tests` green in Z2T-PROC-007
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "libgraph CMake front end"

### Z2-R-PROC-033 The shared gate module

- **Statement:** `Common/Zenith2Gates.psm1` exports the functions of the design with the stated behaviour, including the three functions of the shared gate environment and `Get-Zenith2TestEnvironment` setting `VK_DRIVER_FILES` and the validation layer.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the module self-test `Test-Zenith2Gates.ps1` run by `Run-GateSelfTests.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Shared gate module"

### Z2-R-PROC-034 The step lifecycle and report

- **Statement:** Every step follows the lifecycle state machine, writes its report in the `zenith2.gate-step` format on every exit path, prints its verdict line, writes only under `${ZENITH_OUT}`, and sweeps its own executables by path.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `<Step>_ReportWrittenOnEveryPath` for every step
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Step lifecycle"; `Zenith2/Docs/Design/PROC.md` → "The gate step report"; `Zenith2/Docs/Design/PROC.md` → "Gate step exit codes"

### Z2-R-PROC-035 Step timeouts come from the budget rows

- **Statement:** A step's timeout is the adopted value of its row in `Budgets.md` → "Row index", or the provisional value of `step_timeouts.json` while the row is pending, and the report names the source.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** Z2-B-PROC-007, Z2-B-PROC-011
- **Acceptance evidence:** `<Step>_Timeout_Exits124`; the report's `timeoutSource`
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Shared gate module"

### Z2-R-PROC-036 Configure

- **Statement:** The `Configure` step configures every preset whose condition holds on the host with the File API query written first, and fails on a configure error or on `declared_deps.json` bytes that differ between presets.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** Z2-B-PROC-007
- **Acceptance evidence:** Configure self-test fixtures; the scaffold tick
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Configure"

### Z2-R-PROC-037 Build

- **Statement:** The `Build` step builds `win-tools` first, detects and signals a cold tree, prints sccache statistics, and fails with the first error line of a failing build.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** Z2-B-PROC-008, Z2-B-PROC-009
- **Acceptance evidence:** Build self-test; the scaffold tick's `cold` record
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Build"

### Z2-R-PROC-038 Test runs the three Windows test presets

- **Statement:** The `Test` step runs the `win-debug`, `win-release` and `win-shipping-test` test presets on every Z2 diff (the Shipping leg is not path-selected, so a Shipping-only test is gated as "`Test` step (`win-shipping-test` preset)"), and `-Label <l>` runs exactly the tests carrying that label in one preset.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** Z2-B-PROC-011
- **Acceptance evidence:** Test self-test; the scaffold tick
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Test"

### Z2-R-PROC-039 Test excludes parity and acceptance labels

- **Statement:** The `Test` step excludes the labels `z2-parity-backend`, `z2-parity-path` and every `z2-acceptance-*` label, which only their own steps and the acceptance run select.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-063, Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** Test self-test fixture with labelled stub tests
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Test"

### Z2-R-PROC-040 SourceLayout

- **Statement:** The `SourceLayout` step enforces rules SL1 to SL10 over tracked and untracked files.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** Z2-B-PROC-012
- **Acceptance evidence:** SourceLayout fixtures
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "SourceLayout"

### Z2-R-PROC-041 CompileCoverage

- **Statement:** The `CompileCoverage` step proves rules CC1 to CC4 for the build presets of the phase, reading each game's matrix from `declared_deps.json`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-001
- **Budgets:** Z2-B-PROC-013
- **Acceptance evidence:** CompileCoverage fixtures
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "CompileCoverage"

### Z2-R-PROC-042 Format

- **Statement:** The `Format` step runs the pinned clang-format with `Zenith2/.clang-format` over every C and C++ file outside `ThirdParty/` and fails on any difference or an unclosed `clang-format off`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** Z2-B-PROC-017
- **Acceptance evidence:** Format fixtures
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Format"

### Z2-R-PROC-043 HeaderSelfContained

- **Statement:** The `HeaderSelfContained` step compiles every public header alone against an overlay of its library's and declared dependencies' root headers (plus, for ZenithCore and its dependants, the per-platform threading header of the compile's platform), and repeats it with Android flags once an Android build line is active.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** Z2-B-PROC-018
- **Acceptance evidence:** HeaderSelfContained fixtures
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "HeaderSelfContained"

### Z2-R-PROC-044 ApiContractCheck

- **Statement:** The `ApiContractCheck` step runs the contract-block, deviation, example and `.inl` steps of the contract with a streaming AST reader and the self-test fixtures one per rule ID, and checks no `noexcept` and no `[[nodiscard]]`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-042
- **Budgets:** Z2-B-PROC-019
- **Acceptance evidence:** ApiContractCheck fixtures
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "ApiContractCheck"

### Z2-R-PROC-045 TestSeparationCheck

- **Statement:** The `TestSeparationCheck` step proves conditions TS1 to TS8, condition 6 covering test-access headers and condition 7 tool-access headers.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** Z2-B-PROC-020
- **Acceptance evidence:** TestSeparationCheck fixtures TS1 to TS8
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "TestSeparationCheck"

### Z2-R-PROC-046 ForbiddenCheck scan and lexing

- **Statement:** ForbiddenCheck scans tracked and untracked Zenith2 C and C++ files, classifies each, lexes comments and literals out, and matches tokens as whole identifiers.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** Z2-B-PROC-016
- **Acceptance evidence:** `Pass-TokensInComments`, `Pass-RawString`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck"

### Z2-R-PROC-047 ForbiddenCheck rules F1 to F23 and F25

- **Statement:** ForbiddenCheck enforces F1 to F21 of the contract table (F11 with exactly the contract's macro list), F22 (`LoadLibrary`/`dlopen`), F23 (third-party headers outside the owner) and F25 (`noexcept` and `[[nodiscard]]` outside a `// ZENITH-ALLOW: <reason>` line, `u_int*`, `int8_t` to `uint64_t` outside `Zenith_Types.h` and the `f25Boundary` files, and `final` on a class with no base, in runtime and tool code), each in its rejected and allowed scope.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-041, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** one failing fixture per rule
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"

### Z2-R-PROC-048 ForbiddenCheck firewall rules

- **Statement:** ForbiddenCheck enforces FS1 to FS7 with their Scripting diagnostics, reporting a match two rules share as one finding with both IDs.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-017
- **Budgets:** none
- **Acceptance evidence:** FS fixtures and `Test_Forbidden_DoubleRuleReportedOnce`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"

### Z2-R-PROC-049 Exception and RTTI flags

- **Statement:** ForbiddenCheck fails any compile group with exception or RTTI flags outside the firewall groups and the Luau targets, and any non-firewall group lacking the exceptions-off flags.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-017, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `F17-EHscFlag`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"

### Z2-R-PROC-050 The forbidden allowlist

- **Statement:** `forbidden_allowlist.json` holds entries with `id`, `rule`, `path`, `decision` and optional `token` and `comment`, lands with F1-01 to F1-03 and FSAN-01, the `f21Scope` list, an empty `f25Boundary` list and the twenty-five-element `profilerReachable` list, refuses F3, F8, F25 and F26 entries, malformed or duplicate `profilerReachable` elements and any `f25Boundary` file outside its third-party owner's `Internal/`, and never holds an entry whose `decision` is not an existing `Z2-D-NNN`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Pass-Allowlisted`, `Test_Forbidden_AllowlistNoDecisionFails`, `Test_Forbidden_AllowlistF3Refused`, `Test_Forbidden_AllowlistF25Refused`, `Test_Forbidden_AllowlistF26Refused`, `Test_Forbidden_F25BoundaryOutsideInternalFails`, `Test_Forbidden_ProfilerReachableCommitted`, `F26-ListMalformedElement`, `F26-ListDuplicatePath`, `Test_Forbidden_NoUndecidedEntry`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck allowlist files"

### Z2-R-PROC-051 Standard and platform header lists

- **Statement:** `std_headers.json` lists the C++20 standard headers and the thirteen runtime headers matching the contract table, and `platform_headers.json` carries the CORE, RHI and SCENE entries and the `publicPlatformHeaders` exception admitting `<pthread.h>` and `<semaphore.h>` in the Android and Linux per-platform threading headers alone.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-041
- **Budgets:** none
- **Acceptance evidence:** `Test_Forbidden_StdHeadersMatchContracts`, `F18-WindowsHInCore`, `F18-PthreadInPublicHeader`, `Pass-PlatformThreadingHeader`, `Test_Forbidden_PlatformExceptionShape`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck allowlist files"

### Z2-R-PROC-052 The writable-data scan checks the signpost

- **Statement:** Rule F16 scans the `win-debug` objects; each allowlisted symbol passes only with a complete entry and line (1) of its declaration comment block exact above its definition, while a missing or different line (2) to (4) is a `report` finding.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `F16-MutableGlobal`, `F16-MissingSignpost`, `F16-IncompleteEntry`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Writable-data scan"

### Z2-R-PROC-053 The five writable-data entries

- **Statement:** `writable_data_allowlist.json` carries exactly the five entries of the contract with their extensions and comments.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_Forbidden_WritableEntriesMatchContracts` (each entry equal, member for member, to its Contracts row) and `Test_Forbidden_AllowlistSourcesMatchDesign`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Writable-data scan"

### Z2-R-PROC-054 Transcendental calls are forbidden programme-wide

- **Statement:** Rule F21 refuses CRT transcendental calls in runtime code, `Tools/Authoring/**`, `Tools/ZenithCook/**` and the `f21Scope` directories, admitting `sqrt` and allowlisted host-comparison tests.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `F21-SinInCook`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"

### Z2-R-PROC-055 ForbiddenCheck fixtures

- **Statement:** ForbiddenCheck ships one failing fixture per rule and the passing fixtures named in the design, and its self-test asserts each rule set exactly.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test-ForbiddenCheck.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck fixtures"

### Z2-R-PROC-056 Complexity

- **Statement:** The `Complexity` step runs the analyzer with `complexity_profile.json`, whose thresholds are hard failures and whose exceptions each carry a decision ID.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** Z2-B-PROC-015
- **Acceptance evidence:** Complexity fixtures
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Complexity"

### Z2-R-PROC-057 ReviewSignals reports only

- **Statement:** The `ReviewSignals` step reports the five signals as `report` findings and never fails on them.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** Z2-B-PROC-021
- **Acceptance evidence:** ReviewSignals self-test
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "ReviewSignals"

### Z2-R-PROC-058 ShaderChecks

- **Statement:** The `ShaderChecks` step fails any `vk::` in a `.slang` file (`NoVulkanAttributes`, never exempt), passes with a report line while no shader exists, then runs `ZenithShaderCompiler --check` and fails every non-allowlisted `fail` lint and every allowlist entry without a decision.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-014, Z2-D-025
- **Budgets:** Z2-B-PROC-024
- **Acceptance evidence:** ShaderChecks fixtures
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "ShaderChecks"

### Z2-R-PROC-059 EcsConformance

- **Statement:** The `EcsConformance` step passes with a report line while no ECS backend exists, then runs `ZenithECS_Conformance` for every backend and fails a backend that contributed no test.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-039
- **Budgets:** Z2-B-PROC-031
- **Acceptance evidence:** EcsConformance self-test
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "EcsConformance"

### Z2-R-PROC-060 Sweep

- **Statement:** The cleanup line `Sweep.ps1` kills only processes whose image lies under this checkout's out tree and always exits 0.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-029
- **Budgets:** none
- **Acceptance evidence:** Sweep self-test with a stub process
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Sweep"

### Z2-R-PROC-061 Run-Committed for CI

- **Statement:** `Run-Committed.ps1 -ListJobs -Json` emits the job plan and `-Host windows|linux` runs every active non-acceptance line of that host plus the CI-only lines, all lines even after a failure.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-031, Z2-D-032
- **Budgets:** none
- **Acceptance evidence:** Run-Committed self-test; the green CI run of the Stage 0 acceptance
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Run-Committed"

### Z2-R-PROC-062 Run-Committed for acceptance

- **Statement:** `Run-Committed.ps1 -Acceptance -Stage N` checks its preconditions, holds the build slot, runs the cumulative suites of stages 0 to N under the acceptance timeout and writes `run.json`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** Z2-B-PROC-006
- **Acceptance evidence:** Run-Committed self-test; each stage acceptance's `run.json`
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Run-Committed"

### Z2-R-PROC-063 Gate self-tests

- **Statement:** The four cross-cutting tests (category agreement, local subset of CI, tree stability, scripts exist) and every step's self-test pass under `Run-GateSelfTests.ps1`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-031, Z2-D-086, Z2-D-119
- **Budgets:** none
- **Acceptance evidence:** `Run-GateSelfTests.ps1` output in Z2T-PROC-012 and every stage acceptance
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Gate self-tests"

### Z2-R-PROC-064 step_timeouts.json and the Stage 0 allocation

- **Statement:** `step_timeouts.json` maps every step and building preset to its row with a provisional value, lists `warmPresets` and the acceptance row, and the Stage 0 acceptance adopts Z2-B-PROC-001, -005 to -009, -011 to -022, -006 and the Shipping-build row from three cold runs each by the allocation rule, with a floor of one third of each provisional step timeout.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028, Z2-D-012
- **Budgets:** Z2-B-PROC-001, Z2-B-PROC-005, Z2-B-PROC-006, Z2-B-PROC-007, Z2-B-PROC-022
- **Acceptance evidence:** `allocation-stage-0.json`, the `Budgets.md` diff of Z2T-PROC-024 and the Stage 0 acceptance evidence row that lists them
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Shared gate module"; `Zenith2/Docs/Design/PROC.md` → "Budget rows"

### Z2-R-PROC-065 The Stage 0 category edits

- **Statement:** Z2T-PROC-008, -016, -017, -019 and -024 make exactly the category edits of the design, keeping the five Z2 categories identical except in `paths`, `gates` and `phaseTimeoutMinutes`.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-086, Z2-D-032
- **Budgets:** none
- **Acceptance evidence:** `Test-CategoryAgreement.ps1` after each ticket
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket"

### Z2-R-PROC-066 Coverage checks the backlog

- **Statement:** `Coverage.ps1` reads the inventory, catalogues, board and documents read-only, writes its outputs only under `${ZENITH_OUT}/coverage/`, and fails on every `fail` rule of `Coverage/rules.json`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-067, Z2-D-068
- **Budgets:** Z2-B-PROC-022
- **Acceptance evidence:** Coverage fixtures
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage"; `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage rules"

### Z2-R-PROC-067 Coverage failing fixtures

- **Statement:** Coverage ships one fixture board per failure mode and the passing and acceptance fixtures, and its self-test asserts each is detected.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-067
- **Budgets:** none
- **Acceptance evidence:** `Test_Coverage_EachFixtureDetected`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage fixtures"

### Z2-R-PROC-068 Coverage acceptance mode

- **Statement:** `Coverage.ps1 -Stage N` requires every reference of a stage-≤N ticket to exist, every stage-≤N requirement's tickets Done, every evidence fingerprint valid, and no open acceptance breach.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-065, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the `Accept-*` fixtures; each acceptance run
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage acceptance mode"

### Z2-R-PROC-069 Normative grammar and Normative.md

- **Statement:** Coverage fails a normative keyword outside a tagged bullet or informative block in the five scanned documents, a duplicate or foreign tag, and drift of the tracked `Normative.md`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-066
- **Budgets:** none
- **Acceptance evidence:** `CV-NORMATIVE-*` fixtures
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage rules"

### Z2-R-PROC-070 Stale cites are reported, not failed

- **Statement:** Coverage reports every cite whose hash changed or that no longer resolves in `stale_cites.json` without failing.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-068
- **Budgets:** none
- **Acceptance evidence:** `Pass-StaleCiteReported`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage rules"

### Z2-R-PROC-071 Coverage runs DEVT evidence assert scripts

- **Statement:** Coverage imports `Zenith2Evidence.psm1` and runs DEVT's three assert scripts when present, and acceptance mode from Stage 1 fails when the module is absent.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** Coverage self-test with stub scripts
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage rules"

### Z2-R-PROC-072 Coverage enforces protected work

- **Statement:** Coverage fails a ticket without `needs-human` whose Definition of Done or Implementation Contract names a protected path.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-099
- **Budgets:** none
- **Acceptance evidence:** `CV-PROTECTED-WORK` fixture
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage rules"

### Z2-R-PROC-073 The filer files in two passes

- **Statement:** `File-Backlog.ps1` files sprints, epics and leaves in topological order with create-time routing and per-reason links, verifies each body by hash, and designates sprint gates in pass 2, every write naming `--project Z2`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-113, Z2-D-090, Z2-D-111
- **Budgets:** none
- **Acceptance evidence:** `Test-FileBacklog.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Production filer"

### Z2-R-PROC-074 Reconcile owns only the managed region

- **Statement:** Reconcile rewrites only the managed region with a fresh `Spec-Hash:`, preserves every other byte, and refuses a hand-edited region unless `--force-spec` names that ticket.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-068
- **Budgets:** none
- **Acceptance evidence:** `Test_Filer_CommentsAndWorklogSurvive`, `Test_Filer_HandEditRefused`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Production filer"

### Z2-R-PROC-075 Reconcile touches only its own metadata and links

- **Statement:** Reconcile changes only fields recorded in `Managed-Fields:` and only links whose reason starts `backlog:`, and an interrupted run converges.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-068
- **Budgets:** none
- **Acceptance evidence:** `Test_Filer_HandAddedLinksSurvive`, `Test_Filer_ManagedFieldsOnly`, `Test_Filer_InterruptedRunConverges`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Production filer"

### Z2-R-PROC-076 Filer trailer order and Coverage.md

- **Statement:** The filer writes the trailer lines in the fixed order (`Requirements:`, `Decisions:`, `Open-Decisions:`, `Kind:`, `Managed-Fields:`, `Draft-ID:`, `Spec-Maturity:`, `Cites:`, `Spec-Hash:`), reconciles a ticket without `Managed-Fields:` body-only, and `--write-coverage` writes `Coverage.md` from the `coverageMdPreamble` and `areaOrder` of `Coverage/rules.json`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-102, Z2-D-129
- **Budgets:** none
- **Acceptance evidence:** `Test_Filer_MarkStaleUsesMaster`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Production filer"; `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage.md format"

### Z2-R-PROC-077 Filer tests and Coverage.md

- **Statement:** The filer carries the eight named tests, writes the `Kind:` trailer, and `--write-coverage` writes `Coverage.md` in the fixed format.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-067
- **Budgets:** none
- **Acceptance evidence:** `Test-FileBacklog.ps1`, `Test_Coverage_RenderMatchesTracked`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Production filer"; `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage"

### Z2-R-PROC-078 The CI workflow

- **Statement:** `.github/workflows/zenith2-gates.yml` implements the CI matrix SHIP specifies through `Run-Committed.ps1`, and a push containing the scaffold runs it green.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-031
- **Budgets:** none
- **Acceptance evidence:** the CI run attached to the Stage 0 acceptance; `Pwsh_Ship_CiWorkflowShape` from Stage 1
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "CI workflow"

### Z2-R-PROC-079 reviewPaths and requiredByDiff

- **Statement:** A generic `reviewPaths` key makes `zagent gates` write `review.requiredByDiff` and `review.paths`, and the Z2 value carries the five globs including `licence_policy.json`.
- **Covers:** Z2-I-PROC-01, Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `requiredByDiff true only for reviewPaths changes`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "PROC-API review records"

### Z2-R-PROC-080 TICK.md step 6.3 dispatches on the diff

- **Statement:** `TICK.md` step 6.3 dispatches the reviewer when `review.required`, `review.requiredByDiff` or a non-empty `reviewRequest` holds, naming the paths in the prompt.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** the Z2T-PROC-016 diff and `Test-HarnessParity.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "PROC-API review records"

### Z2-R-PROC-081 zagent review record

- **Statement:** `zagent review record` appends a review block holding the verdict, the reviewer tier and the SHA-256 over the reviewed paths' current blobs.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `records hash over reviewPaths blobs`, `deleted path hashed as deleted`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "PROC-API review records"

### Z2-R-PROC-082 finish refuses an unreviewed diff

- **Statement:** `zagent finish --status Done` refuses when a review is required by the diff or by a gate and no review block's hash matches the current blobs, naming the differing paths.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `post-review edit to a reviewed header is refused at finish`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "PROC-API review records"

### Z2-R-PROC-083 PROC-API tests

- **Statement:** The Vault tests for each PROC-API element pass, and reconcile and `specOf` preserve and ignore review blocks.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `test/review-record.test.mjs`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "PROC-API review records"

### Z2-R-PROC-084 reviewerConventions is inlined

- **Statement:** A generic `reviewerConventions` key reaches `ticket.json`, and `TICK.md` step 6.3 inlines it verbatim in the reviewer prompt.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `surfaces reviewerConventions`; `Test-HarnessParity.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Reviewer conventions"

### Z2-R-PROC-085 The Z2 reviewer conventions

- **Statement:** Every Z2 category carries the reviewer-conventions string of the design, instructing reviewers to challenge behaviour beyond the author's tests and check the Review focus list.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `Test-CategoryAgreement.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket"; `Zenith2/Docs/Design/PROC/Harness.md` → "Reviewer conventions"

### Z2-R-PROC-086 postFinish runs after finish

- **Statement:** A generic `postFinish` key and `TICK.md` step 8a run each line after a Done finish without changing the ticket's status, from the main checkout in lane mode.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-102
- **Budgets:** none
- **Acceptance evidence:** `surfaces postFinish`; `Test-HarnessParity.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "postFinish"

### Z2-R-PROC-087 The Z2 postFinish command

- **Statement:** Every Z2 category carries `postFinish` naming `File-Backlog.ps1 --mark-stale`.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-102
- **Budgets:** none
- **Acceptance evidence:** `Test-CategoryAgreement.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket"

### Z2-R-PROC-088 The Stage 0 reproducibility manifest

- **Statement:** Z2T-PROC-018 lands `ThirdParty/manifest.json` with the full toolchain section and the doctest entry, `licence_policy.json`, the licence texts and the setup scripts exactly as SHIP's Stage 0 specification states, every pin downloaded and verified by the human.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-008, Z2-D-082
- **Budgets:** none
- **Acceptance evidence:** `Setup-Zenith2.ps1 -Check` reports on the acceptance machine and the CI runners
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map"; `Zenith2/Docs/Design/PROC/GateList.md` → "Hand-off ledger"

### Z2-R-PROC-089 A cited decision selects its ruling

- **Statement:** The rulings matcher scores a directly cited decision ID at least the threshold and sorts cited entries first, so no cited ruling is lost to the 25-entry limit.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-098
- **Budgets:** none
- **Acceptance evidence:** `bare citation selects its entry`, `25-entry limit keeps every cited entry`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Rulings citation weight"

### Z2-R-PROC-090 The lane provisioning hook

- **Statement:** `lane add` passes `-Project` to `lane_init.ps1`, which applies the project's `lanes.provision` entry (`copyIgnored`, `regen`, `provisionGames`, `steps`) and behaves as today without one.
- **Covers:** Z2-I-PROC-01, Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-029
- **Budgets:** none
- **Acceptance evidence:** `Test_LaneInit_ProvisionEntryHonoured`, `Test_LaneInit_NoEntryUnchanged`, `passes -Project to lane_init`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Lane provisioning hook"

### Z2-R-PROC-091 The Z2 lane provisioning step

- **Statement:** `Provision-Lane.ps1` pulls LFS content, checks the setup, warms the three Windows builds and measures the lane's storage.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-029
- **Budgets:** Z2-B-PROC-001
- **Acceptance evidence:** `lane-storage.json` from the Z2 lane registration
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Lane provisioning hook"

### Z2-R-PROC-092 The Z2 lane

- **Statement:** `zagent lane add Z2` registers the dedicated Z2 lane before the scaffold with its warm-up skipped, trust is accepted in it, and the ticket commits nothing; the lane is warmed and measured by `Provision-Lane.ps1 -WarmOnly` at the Stage 0 acceptance.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-029
- **Budgets:** none
- **Acceptance evidence:** `zagent lane list --json` and `zagent lane which --json` in Z2T-PROC-022's work log, with the `Provision-Lane: no scaffold yet; warm-up skipped` line
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Z2 lane registration"

### Z2-R-PROC-093 Stage 0 acceptance

- **Statement:** The Stage 0 acceptance collects every evidence row of its table, including the ZEN probes, the scaffold ticked end to end in the Z2 lane with `integrate` GREEN, `Test-GateScriptsExist.ps1` green, a green CI run, the re-pinned baseline and the review record.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-128, Z2-D-083
- **Budgets:** Z2-B-PROC-001, Z2-B-PROC-005, Z2-B-PROC-006
- **Acceptance evidence:** the Z2T-PROC-024 work log and `run.json`
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Stage 0 acceptance"

### Z2-R-PROC-094 Stage 0 mid-sprint check

- **Statement:** The Stage 0 mid-sprint check regenerates and checks the three harnesses, runs doctor and the two client gates, and compares the ZEN and ZM gate payloads byte for byte.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-044
- **Budgets:** none
- **Acceptance evidence:** the Z2T-PROC-023 work log
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Mid-sprint integration checks"

### Z2-R-PROC-095 Android build and ApkCheck activation

- **Statement:** Z2T-PROC-040 adds `Build -Preset android-arm64` to the base gates and every `measureSteps` and `ApkCheck` to the base gates, and adopts Z2-B-PROC-010 and -023.
- **Covers:** Z2-I-PROC-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010, Z2-D-028
- **Budgets:** Z2-B-PROC-010, Z2-B-PROC-023
- **Acceptance evidence:** the Z2T-PROC-040 diff and allocation
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Gate list by stage"; `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket"

### Z2-R-PROC-096 ApkCheck alignment

- **Statement:** `ApkCheck` fails any `.so` with a `LOAD` alignment below 0x4000, any release APK failing `zipalign -c -P 16 -v 4`, and a game `.so` without `SDL_main`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-010
- **Budgets:** Z2-B-PROC-023
- **Acceptance evidence:** ApkCheck fixtures A1 to A3
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "ApkCheck"

### Z2-R-PROC-097 ApkCheck dex contents

- **Statement:** `ApkCheck` fails a game APK missing a selected module's Java class and any dex class outside `org.libsdl.app` and `org.zenith2`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** none
- **Acceptance evidence:** ApkCheck fixtures A4 and A5
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "ApkCheck"

### Z2-R-PROC-098 ApkCheck builds the APKs

- **Statement:** `ApkCheck` builds `zenith_apks` for release, and for HWASan whenever an Android packaging input changed or no ticket context exists.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** ApkCheck self-test
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "ApkCheck"

### Z2-R-PROC-099 Sanitizer activation

- **Statement:** Z2T-PROC-041 adds the two `Sanitizers` lines to `Zenith2Memory`, sets its `paths` to the union of `sanitizer_paths.json`, and adopts Z2-B-PROC-025 and -026.
- **Covers:** Z2-I-PROC-02
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011, Z2-D-028
- **Budgets:** Z2-B-PROC-025, Z2-B-PROC-026
- **Acceptance evidence:** the Z2T-PROC-041 diff and allocation
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket"; `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizers"

### Z2-R-PROC-100 Per-preset sanitizer path selection

- **Statement:** Each `Sanitizers` line runs only when the changed set reaches its preset's glob list (which from Z2T-PROC-041 includes `Zenith2/Tools/ContentCore/**` and every fuzz-owning directory), runs fully without ticket context, and `Test-SanitizerPathsMatchCategory` keeps the category paths equal to the union.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_Sanitizers_NoMatchPasses`, `Test_Sanitizers_MatchRuns`, `Test-SanitizerPathsMatchCategory`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizer path selection"

### Z2-R-PROC-101 win-asan run

- **Statement:** `Sanitizers -Preset win-asan` builds and runs every test of the preset, including integration suites, defect suites and 60-second fuzz smokes.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** Z2-B-PROC-025
- **Acceptance evidence:** the Stage 2 acceptance sanitizer reports
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizers"

### Z2-R-PROC-102 linux-tsan run

- **Statement:** `Sanitizers -Preset linux-tsan` refuses when `vm.mmap_rnd_bits` is above 28, runs through WSL with the fixed options locally and natively in CI, with the binary directory on the Linux filesystem.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** Z2-B-PROC-026
- **Acceptance evidence:** `Test_Sanitizers_MmapBitsRefused`; the CI linux job
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizers"

### Z2-R-PROC-103 Suppressions need a decision

- **Statement:** A suppression line not directly preceded by a comment naming an existing decision fails the `Sanitizers` step.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test_Sanitizers_SuppressionNeedsDecision`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizers"

### Z2-R-PROC-104 BackendParity activation

- **Statement:** Z2T-PROC-050 adds `BackendParity` to `Zenith2Render` and adopts Z2-B-PROC-027.
- **Covers:** Z2-I-PROC-02
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** Z2-B-PROC-027
- **Acceptance evidence:** the Z2T-PROC-050 diff and allocation
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Gate list by stage"

### Z2-R-PROC-105 BackendParity step

- **Statement:** `BackendParity` runs the `z2-parity-backend` cases in `win-release` on WARP and lavapipe and retains failure artefacts.
- **Covers:** Z2-I-PROC-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** Z2-B-PROC-027
- **Acceptance evidence:** BackendParity self-test; Stage 3 acceptance
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "BackendParity"

### Z2-R-PROC-106 IntegrationSmoke

- **Statement:** Z2T-PROC-051 adds `IntegrationSmoke` to the CI-only list, running every scripted workload game headless for 300 ticks, and adopts Z2-B-PROC-028.
- **Covers:** Z2-I-PROC-02, Z2-I-PROC-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-031
- **Budgets:** Z2-B-PROC-028, Z2-B-PROC-030
- **Acceptance evidence:** the CI windows job with the smoke
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "IntegrationSmoke"; `Zenith2/Docs/Design/PROC/GateList.md` → "Gate list by stage"

### Z2-R-PROC-107 PathParity activation

- **Statement:** Z2T-PROC-070 adds `PathParity` to `Zenith2Render` and adopts Z2-B-PROC-029.
- **Covers:** Z2-I-PROC-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** Z2-B-PROC-029
- **Acceptance evidence:** the Z2T-PROC-070 diff and allocation
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Gate list by stage"

### Z2-R-PROC-108 PathParity step

- **Statement:** `PathParity` runs the `z2-parity-path` cases in `win-release` and retains failure artefacts.
- **Covers:** Z2-I-PROC-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-063
- **Budgets:** Z2-B-PROC-029
- **Acceptance evidence:** Stage 5 acceptance
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "PathParity"

### Z2-R-PROC-109 Acceptance ticket shape

- **Statement:** Each of the eight acceptance tickets has the shape of the design: category `Zenith2Acceptance`, `needs-human`, the sprint gate, `blocked_by` only its mid-sprint check and its stage's PROC tickets (Stage 0: Z2T-PROC-023, -025 and the scaffold), the procedure steps in order, no requirement attached to a ticket whose stage comes after its own, closed by a human.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-083, Z2-D-127
- **Budgets:** none
- **Acceptance evidence:** `CV-ACCEPTANCE` clean
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance ticket shape"

### Z2-R-PROC-110 Acceptance requirement lists are derived

- **Statement:** Each acceptance ticket's `Requirements:` line is every requirement whose stage is the ticket's, across all catalogues, recomputed by the filer.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-067
- **Budgets:** none
- **Acceptance evidence:** `CV-ACCEPTANCE` clean after filing and reconcile
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Requirement list rule"

### Z2-R-PROC-111 Stage 1 acceptance evidence

- **Statement:** The Stage 1 acceptance collects the evidence of its table.
- **Covers:** Z2-I-PROC-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** Z2T-PROC-033 work log
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Stage 1 acceptance"

### Z2-R-PROC-112 Stage 2 acceptance evidence

- **Statement:** The Stage 2 acceptance collects the evidence of its table, runs the full sanitizer set, clang-tidy and the fuzz campaign, and adopts Z2-B-PROC-024 and -031.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** Z2-B-PROC-024, Z2-B-PROC-031
- **Acceptance evidence:** Z2T-PROC-043 work log
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Stage 2 acceptance"

### Z2-R-PROC-113 Stage 3 acceptance evidence

- **Statement:** The Stage 3 acceptance collects the evidence of its table.
- **Covers:** Z2-I-PROC-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** Z2T-PROC-053 work log
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Stage 3 acceptance"

### Z2-R-PROC-114 Stage 4 acceptance evidence

- **Statement:** The Stage 4 acceptance collects the evidence of its table.
- **Covers:** Z2-I-PROC-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** Z2T-PROC-061 work log
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Stage 4 acceptance"

### Z2-R-PROC-115 Stage 5 acceptance evidence

- **Statement:** The Stage 5 acceptance collects the evidence of its table, including the path-tracer and approximation-error suites.
- **Covers:** Z2-I-PROC-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** Z2T-PROC-072 work log
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Stage 5 acceptance"

### Z2-R-PROC-116 Stage 6 acceptance evidence

- **Statement:** The Stage 6 acceptance collects the evidence of its table, including the security hardening evidence.
- **Covers:** Z2-I-PROC-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** Z2T-PROC-082 work log
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Stage 6 acceptance"

### Z2-R-PROC-117 Stage 7 acceptance evidence

- **Statement:** The Stage 7 acceptance collects, per expansion delivered, its conformance, runtime evidence and budget rows.
- **Covers:** Z2-I-PROC-03
- **Stage:** S7
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** Z2T-PROC-094 work log
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Stage 7 acceptance"

### Z2-R-PROC-118 Mid-sprint checks every stage

- **Statement:** Every stage sprint, Stage 1 to Stage 7, carries one mid-sprint integration check that measures the playthroughs or workloads on both reference platforms and files regressions as BUG tickets; the requirement is staged at S7, the last stage whose check it describes.
- **Covers:** Z2-I-PROC-03
- **Stage:** S7
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the shape check of Z2T-PROC-025 and each mid-sprint ticket's work log, the last at the Stage 7 acceptance
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Mid-sprint integration checks"

### Z2-R-PROC-119 needs-revalidation affects claiming

- **Statement:** Z2T-PROC-030 makes the queue skip and a targeted claim refuse a `needs-revalidation` ticket, and the filer then stops pairing it with `deferred`.
- **Covers:** Z2-I-PROC-01
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-102
- **Budgets:** none
- **Acceptance evidence:** `needs-revalidation skipped by queue`, `targeted claim of needs-revalidation refused`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "Revalidation claim predicate"

### Z2-R-PROC-121 ApkCheck over bundle splits

- **Statement:** From Stage 6 `ApkCheck` builds `Minimal_Bundle` and checks its bundletool split APKs' alignment.
- **Covers:** Z2-I-PROC-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-027
- **Budgets:** Z2-B-PROC-023
- **Acceptance evidence:** Stage 6 acceptance
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "ApkCheck"

### Z2-R-PROC-122 Stage 7 expansion gate lines

- **Statement:** Each Stage 7 expansion that brings tooling adds its fixed lines through its own PROC ticket: the dedicated-server presets, the `z2-xr` line against SCOPE's scripted OpenXR runtime, and the `ps5`, `xbox-series` and `switch2` build lines (Z2T-PROC-090, -091, -092a to -092c), each blocked by the expansion's preset or SDK-access ticket.
- **Covers:** Z2-I-PROC-02
- **Stage:** S7
- **Classification:** required
- **Decisions:** Z2-D-071, Z2-D-072, Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** Z2T-PROC-090 to -092 diffs
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Gate list by stage"

### Z2-R-PROC-123 A8a procedure

- **Statement:** A8a lands the documents, the board registration and the one `metadata.md` row in one commit by the procedure stated, after ZEN-119 is committed and every lane is idle.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-115, Z2-D-088, Z2-D-119
- **Budgets:** none
- **Acceptance evidence:** `verify.ps1` all PASS and `doctor` ok after the commit
- **Design:** `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "Before A8a"; `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "A8a docs and config commit"

### Z2-R-PROC-124 Journal archival before A6

- **Statement:** Before A6 the human archives every complete Vault journal outside `vault/operations` by the stated procedure, after recovery ran clean.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-110
- **Budgets:** none
- **Acceptance evidence:** the `moved=… kept=…` line and `doctor` ok
- **Design:** `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "Journal archival before A6"

### Z2-R-PROC-125 A6 dry run

- **Statement:** A 50-ticket dry run on a scratch branch measures seconds per operation and proves the squash path before filing.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-110, Z2-D-069
- **Budgets:** none
- **Acceptance evidence:** `timing.json`
- **Design:** `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "A6 dry run"

### Z2-R-PROC-126 A6 filing

- **Statement:** The filing runs on `z2-board-seed` in the main checkout with every lane idle, resumable by Draft-ID, and its diff touches only the board.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-069, Z2-D-087, Z2-D-096
- **Budgets:** none
- **Acceptance evidence:** `git diff master...z2-board-seed --name-only` and the filed-mode checker
- **Design:** `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "A6 filing"

### Z2-R-PROC-127 A8b seed commit

- **Statement:** A8b is one squash commit `board: seed Z2 backlog` with a `Vault-Operation:` line, holding only board files.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-101, Z2-D-115
- **Budgets:** none
- **Acceptance evidence:** `git diff --cached --name-only` before the commit
- **Design:** `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "A8b seed commit"

### Z2-R-PROC-128 A8c coverage commit

- **Statement:** A8c commits `Coverage.md` in the fixed format with the real keys before any lane resumes.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-115, Z2-D-067
- **Budgets:** none
- **Acceptance evidence:** the A8c diff
- **Design:** `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "A8c coverage commit"

### Z2-R-PROC-129 After the seed

- **Statement:** After A8c the seed branch is deleted after recovery, the six ZEN-to-Z2 links are made, and the lanes resume.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-112
- **Budgets:** none
- **Acceptance evidence:** `zagent blocked --project Z2`
- **Design:** `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "After the seed"

### Z2-R-PROC-130 Every landing step has a rollback

- **Statement:** Each landing step is undone only by the rollback the design states, and never by deleting a branch before recovery or rewriting `master` after a lane fired.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-069
- **Budgets:** none
- **Acceptance evidence:** review of each step's record
- **Design:** `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "Rollback"

### Z2-R-PROC-131 Every PROC hand-off is answered

- **Statement:** Every PROC hand-off of every area, including the late SEC, RESEARCH, NET, SCOPE, MAINT, CONTENT, PACK, CHAR and CINE rows, has a disposition in the ledger: a PROC ticket carrying it verbatim, the ruling or ticket that answered it, or a stated reason for declining it.
- **Covers:** Z2-I-PROC-01, Z2-I-PROC-02, Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the ID-by-ID audit table in Z2T-PROC-025's work log, an evidence row of the Stage 0 acceptance
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Hand-off ledger"

### Z2-R-PROC-132 PROC tickets are human-run and explicit

- **Statement:** Every PROC ticket is `needs-human`, names the file and the change in its Implementation Contract, and is closed by a human actor.
- **Covers:** Z2-I-PROC-01, Z2-I-PROC-02, Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-103, Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** `zagent check` showing `needsHuman: true` on every PROC ticket at hand-over
- **Design:** `Zenith2/Docs/Design/PROC.md` → "PROC ticket state machine"

### Z2-R-PROC-133 Gate lines live under the gate directory

- **Statement:** Every Z2 gate and cleanup line is `pwsh -NoProfile -File Zenith2\Tools\Gates\<Step>.ps1 …`, and names an existing script from Z2T-PROC-013's landing on; the hand-over lines are the one exception before then.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-032
- **Budgets:** none
- **Acceptance evidence:** `Test-GateScriptsExist.ps1` (first run by Z2T-PROC-012 after Z2T-PROC-013) and `Test-CategoryAgreement.ps1`
- **Design:** `Zenith2/Docs/Design/PROC.md` → "Gate step command lines"; `Zenith2/Docs/Design/PROC/GateList.md` → "Gate list by stage"

### Z2-R-PROC-134 Libgraph and Complexity carry DEVT requirements

- **Statement:** The Stage 0 tickets landing `Libgraph` and `Complexity` list Z2-R-DEVT-103 and -104, and the Stage 0 acceptance proves both.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the Z2T-PROC-010 body and Stage 0 acceptance
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph"; `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Complexity"

### Z2-R-PROC-135 The CORE justification-record allowlist entries

- **Statement:** The Stage 1 PROC ticket Z2T-PROC-034, blocked by Z2T-CORE-035 and blocking Z2T-CORE-036 and Z2T-CORE-042, adds both entries, F4-01 (`<charconv>` in `ZenithCore/Internal/Text/**`) and F11-01 (`Zenith_CompilerSupport.h`), each with the decision Z2T-CORE-035 creates.
- **Covers:** Z2-I-PROC-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-040
- **Budgets:** none
- **Acceptance evidence:** ForbiddenCheck green on Z2T-CORE-042 (F4-01) and on Z2T-CORE-036 (F11-01), and `Test_Forbidden_NoUndecidedEntry` after the Z2T-PROC-034 diff
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "forbidden_allowlist.json"; `Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map"

### Z2-R-PROC-136 The SecurityConformance step

- **Statement:** Z2T-PROC-044 lands `SecurityConformance.ps1` with SEC's checks SC01 to SC16, its fixtures and assert script, adds its line to the `Zenith2` gates from Stage 2, lists it in `Zenith2/CLAUDE.md`, and has `Coverage` run its assert script.
- **Covers:** Z2-I-PROC-02, Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-128, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Test-SecurityConformance.ps1` green (after Z2T-PROC-046) and the Z2T-PROC-044 diff
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Late hand-offs"; `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Additional runs"

### Z2-R-PROC-137 Confined OS APIs rule F24

- **Statement:** ForbiddenCheck rule F24 rejects a call or address-of of a `confined_apis.json` token outside the directories its allow records name, reports a match F22 shares as one finding with both IDs, and lands with SEC's three fixtures.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `F24-FileApiOutsideAllowlist`, `F24-SocketInPlatformDevServicesPasses`, `F24-MethodNamedOpenPasses`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"

### Z2-R-PROC-138 Sanitizer smoke selection and budget

- **Statement:** With ticket context, `Sanitizers -Preset win-asan` runs only the fuzz smokes SEC's selection rules pick from the changed set, with at least J-way parallelism, shortening each smoke when the smoke share would exceed 960 s.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011, Z2-D-028
- **Budgets:** Z2-B-PROC-025
- **Acceptance evidence:** `Test_Sanitizers_SmokeSelectionByDiff`, `Test_Sanitizers_SmokeBudgetShortens`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Smoke selection and budget"

### Z2-R-PROC-139 Fuzz-owning directories select win-asan

- **Statement:** Z2T-PROC-041 adds `Zenith2/Tools/ContentCore/**` and every fuzz-owning library and tool directory of SEC's inventory to the `win-asan` list and to the `Zenith2Memory` paths.
- **Covers:** Z2-I-PROC-02, Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-011
- **Budgets:** none
- **Acceptance evidence:** `Test-SanitizerPathsMatchCategory` after the Z2T-PROC-041 diff
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizer path selection"

### Z2-R-PROC-140 Shipping exposure layers

- **Statement:** From Z2T-PROC-054, rule LG9 runs SEC's protected audit script `Zenith2/Tools/Gates/Security/Test-Zenith2ShippingExposure.ps1` on every Shipping game image for layers E2 to E4, runs the positive controls once per run against the Release `Minimal` image, and creates `shipping_forbidden_strings.json` with SEC's initial entries.
- **Covers:** Z2-I-PROC-03
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** the Libgraph exposure reports and a run that fails on a planted string, recorded in Z2T-PROC-054
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Shipping exposure layers"

### Z2-R-PROC-141 Consolidated forbidden-symbol entries

- **Statement:** Z2T-PROC-010 lands `shipping_forbidden_symbols.json` with PROC's table plus SEC's consolidated entries, `Zenith_NetTransport::Create` and CINE's two capture symbols.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `LG9-ForbiddenSymbol` and the Z2T-PROC-010 diff
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "shipping_forbidden_symbols.json"

### Z2-R-PROC-142 The late review paths

- **Statement:** Every Z2 category's `reviewPaths` also carries `Zenith2/Tools/Maintenance/*.json`, the four `Zenith2/Tools/Security` files (`update_keys.json`, `parser_inventory.json`, `path_matrix.json`, `android_permissions.json`) and `Zenith2/Tools/Backlog/**`.
- **Covers:** Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `Test-CategoryAgreement.ps1` and `requiredByDiff true only for reviewPaths changes`
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket"

### Z2-R-PROC-143 Decision-ticket and research checks in Coverage

- **Statement:** `Coverage` runs RESEARCH's decision-template check on every `Kind: decision` ticket, and acceptance mode fails a stage's unruled research requirement and runs `Invoke-ResearchCheck.ps1 -Mode decisionlog`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-067, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Accept-ResearchUnruled` and the Coverage self-test with stub scripts
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Additional runs"; `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage acceptance mode"

### Z2-R-PROC-144 The acceptance procedure

- **Statement:** Every stage acceptance runs its procedure in order: MAINT A1 from Stage 2, the cold `Run-Committed` run, the fuzz campaign from Stage 2, budget re-allocation, MAINT A2 with the re-pin, A3 from Stage 4, A4, human evidence, A5 for Stages 2 to 6, the review records and the close.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Test-AcceptanceRecord.ps1 -Stage N` exit 0 with its SHA-256 in the `finish` comment, for each stage
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance procedure"

### Z2-R-PROC-145 The Stage 4 complexity-profile apply ticket

- **Statement:** Z2T-PROC-062, blocked by MAINT's T9 calibration ruling, copies the proposed profile over the protected profile only when the recorded `baseSha256` matches, runs `Complexity`, and is Stage 4's `coverageDeferred` entry.
- **Covers:** Z2-I-PROC-03
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the Z2T-PROC-062 diff and `Complexity` run; Done before the Stage 5 acceptance's Coverage
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "complexity_profile.json"; `Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map"

### Z2-R-PROC-146 Libgraph tool changes keep the baseline's meaning

- **Statement:** A PROC ticket changing `Tools/libgraph/**` attaches `surface.json` from its base and result trees, never lands while a lane runs, and when they differ is scheduled last before the next acceptance re-pin.
- **Covers:** Z2-I-PROC-01, Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the two `surface.json` files attached to the ticket and named in its review record, and `LG10-UNREVIEWED` clean at the next acceptance
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "The inbound-surface review rule"

### Z2-R-PROC-147 Acceptance suites as data

- **Statement:** `acceptance_suites.json` holds every stage's lines with `when` predicates from the closed set, `Run-Committed -Acceptance` executes exactly them, and `Test-AcceptanceSuites.ps1` keeps them equal to the stage tables and checks the acceptance labels.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Test-AcceptanceSuites.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "acceptance_suites.json"

### Z2-R-PROC-148 Acceptance runs export the stage

- **Statement:** `Run-Committed -Acceptance -Stage N` exports `ZENITH_ACCEPTANCE_STAGE=N` and `ZENITH2_ACCEPTANCE=1` to every line, runs cold, under the build slot through `build_lock.ps1 -Run "<command> -InsideSlot"`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** Run-Committed self-test with a stub line that records its environment
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N"

### Z2-R-PROC-149 The baseline re-pin goes through Run-Committed only

- **Statement:** Only `Run-Committed.ps1 -Acceptance -Stage N -WriteBaseline`, after a passing suite run and MAINT's growth report, rewrites `libgraph_baseline.json`; `Libgraph.ps1 -Acceptance` or `-WriteBaseline` anywhere else exits 2.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007, Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** `Test_Libgraph_WriteBaselineRefusedOutsideAcceptance` and each acceptance's `baseline-run.json`
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N -WriteBaseline"

### Z2-R-PROC-150 Libgraph surface and surface delta formats

- **Statement:** Libgraph writes `surface.json` (`zenith2.libgraph-surface`) and `surface_delta.json` (`zenith2.libgraph-surface-delta`) in the fixed formats MAINT's growth report reads.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `Test_Libgraph_SurfaceSchema`, `Test_Libgraph_SurfaceDeltaSchema`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph outputs"

### Z2-R-PROC-151 The parent-surface cache

- **Statement:** Every Libgraph run stores its surface keyed by the surface key (at most 64 files), and a ticket-context run reads the parent's entry by `HEAD`'s surface key, recording `parentSurface: cache` or `changed-set` and listing unattributed miss-mode pairs.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `LG10-ParentCacheHit`, `LG10-BoardOnlyCommitHits`, `LG10-UnattributedListed`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "The inbound-surface review rule"

### Z2-R-PROC-152 Raise-only re-allocation of step timeouts

- **Statement:** Every stage acceptance re-measures every active step cold and raises, never lowers, any step-timeout row and Z2-B-PROC-006 whose rule value exceeds it, recording each raise with its decision.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** Z2-B-PROC-006, Z2-B-PROC-007
- **Acceptance evidence:** each acceptance's `allocation-stage-<N>.json` and `Budgets.md` diff
- **Design:** `Zenith2/Docs/Design/PROC.md` → "Budget rows"

### Z2-R-PROC-153 The per-stage acceptance watchdog

- **Statement:** The acceptance run's watchdog is the larger of Z2-B-PROC-006 and 1.25 times the sum of the timeouts of the stage's lines, recorded in `run.json`, and the fuzz campaign runs outside it.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-083, Z2-D-028
- **Budgets:** Z2-B-PROC-006
- **Acceptance evidence:** Run-Committed self-test of the watchdog arithmetic and `run.json` `watchdogMinutes`
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N"

### Z2-R-PROC-154 Cold detection in every building step

- **Statement:** Every step that builds does so through `Invoke-Zenith2Build`, which detects a cold tree and signals the cold cap.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** Z2-B-PROC-005
- **Acceptance evidence:** `Test_TickGates_ColdSignalSwitchesCap` driven through a Libgraph and a Sanitizers stub build
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Functions"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Build"

### Z2-R-PROC-155 Lane warm-up

- **Statement:** `Provision-Lane.ps1` skips its warm-up when no scaffold exists, `-WarmOnly` warms every preset of `warmPresets`, and every PROC ticket that activates a building step extends the list and warms the Z2 lane.
- **Covers:** Z2-I-PROC-01, Z2-I-PROC-02
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-029
- **Budgets:** Z2-B-PROC-001
- **Acceptance evidence:** `lane-storage.json` before and after the scaffold
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "The Z2 step"

### Z2-R-PROC-156 Skipped tests are never passes

- **Statement:** The `Test` step reports every skipped or not-run test as `TEST-NOT-RUN` with its labels and counts it apart from the passes.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** a Test self-test fixture with a test returning the skip code
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Test"

### Z2-R-PROC-157 Human-run tickets are reviewed like ticks

- **Statement:** A PROC or acceptance ticket whose diff reaches `reviewPaths` dispatches the high-tier reviewer on its staged diff and records the verdict before each such commit, so `finish --human` succeeds only on a matching record.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-049
- **Budgets:** none
- **Acceptance evidence:** the review blocks of each acceptance ticket
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "4. Finish refusal"

### Z2-R-PROC-158 How a PROC ticket is gated before its scripts exist

- **Statement:** Until the scaffold Z2T-MOD-056 has landed (the presence of `Zenith2/CMakePresets.json` on `master`), no PROC ticket is run through `tick_gates.ps1`: the human runs its self-tests, `Run-GateSelfTests.ps1` once it exists and the Tooling category's gate lines by hand; from the scaffold on, PROC tickets run `tick_gates.ps1 -Phase all`.
- **Covers:** Z2-I-PROC-01
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the work logs of every PROC ticket landing before Z2T-MOD-056, and `tick_gates.json` of Z2T-PROC-025
- **Design:** `Zenith2/Docs/Design/PROC.md` → "PROC ticket state machine"

### Z2-R-PROC-159 Mid-sprint MAINT and research steps

- **Statement:** Every mid-sprint check, Stage 1 to Stage 7, runs MAINT's steps M1 to M4 and RESEARCH's four steps; the requirement is staged at S7.
- **Covers:** Z2-I-PROC-03
- **Stage:** S7
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** the shape check of Z2T-PROC-025 and each mid-sprint ticket's work log
- **Design:** `Zenith2/Docs/Design/PROC/Acceptance.md` → "Mid-sprint integration checks"

### Z2-R-PROC-160 Coverage outputs live under the checkout's out tree

- **Statement:** Every Coverage output is written under `${ZENITH_OUT}/coverage/`, and the tracked `Coverage.md` and `Normative.md` are never written by the step.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-067
- **Budgets:** none
- **Acceptance evidence:** `Test_Coverage_NeverWritesTracked` and the step report's `artifacts`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Outputs"

### Z2-R-PROC-161 The Stage 5 complexity-profile apply ticket

- **Statement:** Z2T-PROC-073, blocked by MAINT's T11 ruling, applies the S5 calibration under the same `baseSha256` check and is Stage 5's `coverageDeferred` entry.
- **Covers:** Z2-I-PROC-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the Z2T-PROC-073 diff and `Complexity` run
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "complexity_profile.json"; `Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map"

### Z2-R-PROC-162 The Stage 6 complexity-profile apply ticket

- **Statement:** Z2T-PROC-083, blocked by MAINT's T12 ruling, applies the S6 calibration under the same `baseSha256` check and is Stage 6's `coverageDeferred` entry.
- **Covers:** Z2-I-PROC-03
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the Z2T-PROC-083 diff and `Complexity` run
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "complexity_profile.json"; `Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map"

### Z2-R-PROC-163 The Stage 7 complexity-profile apply ticket

- **Statement:** Z2T-PROC-095, blocked by MAINT's T13 ruling, applies the S7 calibration under the same `baseSha256` check and is Stage 7's `coverageDeferred` entry.
- **Covers:** Z2-I-PROC-03
- **Stage:** S7
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** the Z2T-PROC-095 diff and `Complexity` run
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "complexity_profile.json"; `Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map"

### Z2-R-PROC-164 The protected Security gate directory

- **Statement:** Z2T-PROC-046 creates `Zenith2/Tools/Gates/Security/` with SEC's `Zenith2SecurityGates.psm1`, `Test-Zenith2ShippingExposure.ps1` and `Test-Zenith2UpdateKeys.ps1`, their assert script `Tests/Security/Test-Security.ps1` and its fixtures with one negative control per scan.
- **Covers:** Z2-I-PROC-03
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-128, Z2-D-042
- **Budgets:** none
- **Acceptance evidence:** `Run-GateSelfTests.ps1` running `Tests/Security/Test-Security.ps1` green after the Z2T-PROC-046 diff
- **Design:** `Zenith2/Docs/Design/PROC.md` → "Coordination with SEC"; `Zenith2/Docs/Design/PROC/GateList.md` → "Late hand-offs"

### Z2-R-PROC-165 The analyzer entity dump

- **Statement:** The analyzer's generic `--dump-entities <path>` writes every measured function and file after excludes in the `complexity-entities` schema, with excepted entities flagged and their values kept, and changes nothing else about the run.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-007
- **Budgets:** none
- **Acceptance evidence:** `Test_DumpEntities_EveryFunctionListed`, `Test_DumpEntities_ExceptedKeepsValues`, `Test_DumpEntities_FileLinesMatchMaxFileLoc`, `Test_DumpEntities_SchemaAndOrder`
- **Design:** `Zenith2/Docs/Design/PROC/Harness.md` → "dump-entities"

### Z2-R-PROC-166 The automatic headroom ticket

- **Statement:** The Z2 `postFinish` runs `File-Backlog.ps1 --check-headroom`, which records each finished ticket's phase times and files MAINT's headroom ticket after three consecutive tickets above 80 % of a phase cap, never twice for one open ticket.
- **Covers:** Z2-I-PROC-01, Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028, Z2-D-102
- **Budgets:** Z2-B-PROC-002, Z2-B-PROC-003
- **Acceptance evidence:** `Test_Filer_HeadroomThreeConsecutive`, `Test_Filer_HeadroomNoDuplicate`, `Test_Filer_HeadroomColdPhaseUsesColdCap`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "--check-headroom"; `Zenith2/Docs/Design/PROC/Harness.md` → "postFinish"

### Z2-R-PROC-167 Coverage acceptance exempts the accepted ticket and the deferred apply ticket

- **Statement:** `Coverage.ps1 -Stage N` does not require the stage-N acceptance ticket or the Draft-IDs of the stage's `coverageDeferred` list to be Done, and fails a requirement attached to a ticket whose stage comes after its own.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-067, Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Accept-OwnTicketInProgress`, `Accept-DeferredApply`, `Accept-StageOrder`
- **Design:** `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage acceptance mode"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "acceptance_suites.json"

### Z2-R-PROC-168 The acceptance record

- **Statement:** `Test-AcceptanceRecord.ps1 -Stage N` checks `run.json`, the campaign record and its fingerprint, the baseline, the allocation, the review hash and the tracked copies under `Zenith2/Docs/Research/Acceptance/stage-<N>/`, and the next stage's Coverage requires those copies.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-065, Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** the record script's failing fixtures and `Pass-Stage2Record`; `Accept-PreviousRecordMissing`
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Test-AcceptanceRecord"; `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage acceptance mode"

### Z2-R-PROC-169 Step-timeout rows resolve before use

- **Statement:** `Test-StepTimeouts.ps1` fails when a `row` of `step_timeouts.json` does not resolve to a `min` row of `Budgets.md` → "Row index" or still reads `NEW-ROW-`, and Z2T-PROC-008 checks both new rows exist before it lands.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** none
- **Acceptance evidence:** `Test-StepTimeouts.ps1` in `Run-GateSelfTests.ps1`
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Timeout resolution"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Cross-cutting tests"

### Z2-R-PROC-170 Console lines stay in their overlays

- **Statement:** The console build lines of Z2T-PROC-092a to -092c are in no main-board category and run only in the overlay's own checkout, and `Build.ps1 -Preset <console-preset>` exits 3 with the overlay message when the overlay is absent.
- **Covers:** Z2-I-PROC-02, Z2-I-PROC-03
- **Stage:** S7
- **Classification:** required
- **Decisions:** Z2-D-073
- **Budgets:** none
- **Acceptance evidence:** `Test_Build_ConsolePresetAbsentOverlayExits3` and `Test-CategoryAgreement.ps1` showing no console line
- **Design:** `Zenith2/Docs/Design/PROC/GateList.md` → "Stage 7 lines"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Build"

### Z2-R-PROC-171 The one configure edge into the gate directory

- **Statement:** SourceLayout rule SL9 refuses any CMake file naming a path under `Zenith2/Tools/Gates/` except `Zenith2/CMake/ZenithSecurity.cmake`'s execution of `Security/Test-Zenith2UpdateKeys.ps1`.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** `SL9-GateCMakeLists`, `SL9-CMakeNamesGatePath`, `Pass-SecurityConfigureEdge`
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "SourceLayout"

### Z2-R-PROC-172 The Shipping build preset and its timeout row

- **Statement:** `win-shipping` is a preset of the build table, built by `Libgraph` for LG9 through `Invoke-Zenith2Build`, with its own step-timeout row allocated at the Stage 0 acceptance.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-028
- **Budgets:** none
- **Acceptance evidence:** `step_timeouts.json` resolving its row (`Test-StepTimeouts.ps1`) and the Stage 0 allocation
- **Design:** `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Build"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Timeout resolution"

### Z2-R-PROC-173 ForbiddenCheck rule F26: profiler-reachable code and the hook dispatch

- **Statement:** ForbiddenCheck rule F26 refuses a profiled `Zenith_Mutex`, `Zenith_ConditionVariable` or `Zenith_Semaphore` (the alias names as whole tokens, or a platform class template instantiated with `true`) in the runtime sources of `Zenith2/Engine/ZenithProfiling/**`, of `Zenith2/Engine/ZenithCore/Internal/Profiling/**` and of every file in the `profilerReachable` list of `forbidden_allowlist.json`, and refuses a direct use of the hook table (`Zenith_GetProfilerHooks().m_pfn…`, or a call of a `m_pfnOn…` member) in every source except `Zenith2/Engine/ZenithCore/Zenith_ProfilerHooks.h` and tests, so profiler code cannot lock through a primitive that calls the profiler and no hook call bypasses the dispatch functions; the `_NoProfiling` aliases pass, `profilerReachable` is data that starts with the twenty-five sources a hook dispatch reaches (ZenithCore's dispatch header, clock, `Zenith_SpscRing.h`, thread identity, failure path and logging, and the two `Zenith_CrashLogMirror` log-sink sources of ZenithPlatform), the log-record members stay outside clause (ii), and no allowlist entry exempts the rule.
- **Covers:** Z2-I-PROC-03
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-042, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `F26-ProfiledMutexInProfiler`, `F26-ProfiledCondVarInCoreProfiling`, `F26-ProfiledTemplateTrue`, `F26-ProfiledSemaphoreInListedFile`, `F26-DirectHookCallInEcs`, `F26-GetHooksMemberInJobs`, `F26-ListMalformedElement`, `F26-ListGlobPath`, `F26-ListDuplicatePath`, `Pass-F26-NoProfilingInProfiler`, `Pass-F26-DispatchCall`, `Pass-F26-HookTableInstall`, `Pass-F26-HooksHeaderAndTests`, `Pass-F26-ProfiledOutsideScope`, `Pass-F26-ListMissingFileReports`, `Test_Forbidden_AllowlistF26Refused`, `Test_Forbidden_ProfilerReachableCommitted`
- **Design:** `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"; `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck allowlist files"

## Coverage of inventory items

| Item | Requirements |
|---|---|
| Z2-I-PROC-01 | Z2-R-PROC-001, Z2-R-PROC-002, Z2-R-PROC-004, Z2-R-PROC-005, Z2-R-PROC-006, Z2-R-PROC-007, Z2-R-PROC-008, Z2-R-PROC-009, Z2-R-PROC-010, Z2-R-PROC-011, Z2-R-PROC-012, Z2-R-PROC-013, Z2-R-PROC-014, Z2-R-PROC-015, Z2-R-PROC-016, Z2-R-PROC-017, Z2-R-PROC-018, Z2-R-PROC-019, Z2-R-PROC-020, Z2-R-PROC-079, Z2-R-PROC-080, Z2-R-PROC-081, Z2-R-PROC-082, Z2-R-PROC-083, Z2-R-PROC-084, Z2-R-PROC-086, Z2-R-PROC-089, Z2-R-PROC-090, Z2-R-PROC-092, Z2-R-PROC-119, Z2-R-PROC-131, Z2-R-PROC-132, Z2-R-PROC-146, Z2-R-PROC-155, Z2-R-PROC-157, Z2-R-PROC-158, Z2-R-PROC-166 |
| Z2-I-PROC-02 | Z2-R-PROC-002, Z2-R-PROC-003, Z2-R-PROC-024, Z2-R-PROC-025, Z2-R-PROC-065, Z2-R-PROC-078, Z2-R-PROC-079, Z2-R-PROC-085, Z2-R-PROC-087, Z2-R-PROC-090, Z2-R-PROC-091, Z2-R-PROC-095, Z2-R-PROC-099, Z2-R-PROC-104, Z2-R-PROC-106, Z2-R-PROC-107, Z2-R-PROC-122, Z2-R-PROC-123, Z2-R-PROC-124, Z2-R-PROC-125, Z2-R-PROC-126, Z2-R-PROC-127, Z2-R-PROC-128, Z2-R-PROC-129, Z2-R-PROC-130, Z2-R-PROC-131, Z2-R-PROC-132, Z2-R-PROC-136, Z2-R-PROC-139, Z2-R-PROC-142, Z2-R-PROC-155, Z2-R-PROC-170 |
| Z2-I-PROC-03 | Z2-R-PROC-021, Z2-R-PROC-022, Z2-R-PROC-023, Z2-R-PROC-024, Z2-R-PROC-026, Z2-R-PROC-027, Z2-R-PROC-028, Z2-R-PROC-029, Z2-R-PROC-030, Z2-R-PROC-031, Z2-R-PROC-032, Z2-R-PROC-033, Z2-R-PROC-034, Z2-R-PROC-035, Z2-R-PROC-036, Z2-R-PROC-037, Z2-R-PROC-038, Z2-R-PROC-039, Z2-R-PROC-040, Z2-R-PROC-041, Z2-R-PROC-042, Z2-R-PROC-043, Z2-R-PROC-044, Z2-R-PROC-045, Z2-R-PROC-046, Z2-R-PROC-047, Z2-R-PROC-048, Z2-R-PROC-049, Z2-R-PROC-050, Z2-R-PROC-051, Z2-R-PROC-052, Z2-R-PROC-053, Z2-R-PROC-054, Z2-R-PROC-055, Z2-R-PROC-056, Z2-R-PROC-057, Z2-R-PROC-058, Z2-R-PROC-059, Z2-R-PROC-060, Z2-R-PROC-061, Z2-R-PROC-062, Z2-R-PROC-063, Z2-R-PROC-064, Z2-R-PROC-066, Z2-R-PROC-067, Z2-R-PROC-068, Z2-R-PROC-069, Z2-R-PROC-070, Z2-R-PROC-071, Z2-R-PROC-072, Z2-R-PROC-073, Z2-R-PROC-074, Z2-R-PROC-075, Z2-R-PROC-076, Z2-R-PROC-077, Z2-R-PROC-088, Z2-R-PROC-093, Z2-R-PROC-094, Z2-R-PROC-096, Z2-R-PROC-097, Z2-R-PROC-098, Z2-R-PROC-100, Z2-R-PROC-101, Z2-R-PROC-102, Z2-R-PROC-103, Z2-R-PROC-105, Z2-R-PROC-106, Z2-R-PROC-108, Z2-R-PROC-109, Z2-R-PROC-110, Z2-R-PROC-111, Z2-R-PROC-112, Z2-R-PROC-113, Z2-R-PROC-114, Z2-R-PROC-115, Z2-R-PROC-116, Z2-R-PROC-117, Z2-R-PROC-118, Z2-R-PROC-121, Z2-R-PROC-131, Z2-R-PROC-132, Z2-R-PROC-133, Z2-R-PROC-134, Z2-R-PROC-135, Z2-R-PROC-136, Z2-R-PROC-137, Z2-R-PROC-138, Z2-R-PROC-139, Z2-R-PROC-140, Z2-R-PROC-141, Z2-R-PROC-143, Z2-R-PROC-144, Z2-R-PROC-145, Z2-R-PROC-146, Z2-R-PROC-147, Z2-R-PROC-148, Z2-R-PROC-149, Z2-R-PROC-150, Z2-R-PROC-151, Z2-R-PROC-152, Z2-R-PROC-153, Z2-R-PROC-154, Z2-R-PROC-156, Z2-R-PROC-159, Z2-R-PROC-160, Z2-R-PROC-161, Z2-R-PROC-162, Z2-R-PROC-163, Z2-R-PROC-164, Z2-R-PROC-165, Z2-R-PROC-166, Z2-R-PROC-167, Z2-R-PROC-168, Z2-R-PROC-169, Z2-R-PROC-170, Z2-R-PROC-171, Z2-R-PROC-172, Z2-R-PROC-173 |

## Excluded and deferred

- No PROC requirement is excluded.
- **Z2-R-PROC-120 N lanes per project on approval of P2** — status research, deferred pending the human's approval of flagged proposal P2 (Z2-D-130; ruling PROC-R1 (4)). Its statement, if approved: "When the human approves flagged proposal P2, `lane add` supports up to `maxPerProject` lanes per project keyed by lane name, each holding one ticket, with Z2 set to 3." It is kept out of the machine catalogue on purpose. A catalogued requirement needs tickets, and every stage acceptance must list it, but nothing realising P2 is filed (the P1 precedent) and no acceptance depends on it. Approval files its PROC ticket and adds the requirement to the catalogue in the stage the approval names. Rejection records it as `excluded` with the rejection's decision ID. The specification is the assumption block of `Zenith2/Docs/Design/PROC/Harness.md` → "N lanes per project".
- Misuse codes: PROC has no Luau binding and reserves no misuse-code block (ruling MISUSE-BLOCKS-NEW-AREAS-2).
