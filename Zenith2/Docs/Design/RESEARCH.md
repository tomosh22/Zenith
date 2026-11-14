# RESEARCH design: the measurement methodology for architecture research and measured decisions

This is the entry document of the RESEARCH design family. RESEARCH owns the single methodology by which Zenith2 measures and decides: the ECS-backend and shading-path research that plan section 5.5 makes a deliverable (Z2-D-064), and every selection, justification, budget and probe decision that needs a measurement (Z2-D-084). It owns no engine code. Its deliverables are the methodology, the research data formats and their retention, the decision-record template, the CI procedure for research, and the PowerShell tooling and checks that make all of them mechanical.

## Purpose and scope

### What RESEARCH owns

| Concern | Where it is specified |
|---|---|
| the measurement profiles every decision ticket follows (RA architecture research, RS selection, RJ justification evidence, RB budget allocation by harness, RP probe, RQ qualitative ruling) | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Measurement profiles" |
| study pre-registration: hypothesis, alternatives, cells, metrics, comparisons, acceptance rule, fixed repetitions | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Study design and pre-registration" |
| native-versus-adapter measurement of the ECS backends and the shading-path cell matrix | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Native versus adapter measurement" |
| the run procedure: sessions, warm-up, replication, the interleaved schedule, retries | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Run procedure" |
| noise control on the reference desktop and on the S24: clocks, thermal state, background load | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Noise control on the reference desktop", → "Noise control on the S24" |
| the statistics: medians and their intervals, variance, drift, the exact rank-sum test, the shift estimate and its interval, margins, verdicts, with every threshold a number | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Statistics" |
| the acceptance-rule operators and how each open decision's phrasing maps onto them | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Acceptance rule operators" |
| revalidation triggers and the research regression rule | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Revalidation triggers" |
| the research tree, study, record, series, trace, raw and analysis formats, the size caps, the large-file tier (git LFS objects under each run's `archive/`) and retention | `Zenith2/Docs/Design/RESEARCH/Formats.md` |
| the report template, the decision ticket template and the DecisionLog research fields | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report template", → "Decision ticket template", → "DecisionLog research fields" |
| the PowerShell module, the research runner, the conformance checks, the CI procedure, the hand-offs to PROC | `Zenith2/Docs/Design/RESEARCH/Tooling.md` |
| the mapping of every decision onto the methodology | `Zenith2/Docs/Design/RESEARCH/Instances.md` |
| the test catalogue | `Zenith2/Docs/Design/RESEARCH/Tests.md` |

### Explicit exclusions

- **No harness.** Every measurement harness is its measured area's: ECS's `Zenith_Bench_ECS`, `Zenith_Bench_ECSNative`, `Zenith_Bench_ECSWorkload` and the report assembler (`Zenith2/Docs/Design/ECS/Tests.md` → "Benchmarks and research scenarios"), RPROD's `Zenith_Bench_ShadingPaths` (`Zenith2/Docs/Design/RPROD/Tests.md` → "Benchmarks and harnesses"), TWOD's D-SEL-2DPHYS harness (`Zenith2/Docs/Design/TWOD/Tests.md` → "D-SEL-2DPHYS cost procedure"), and every harness `Zenith2/Docs/Design/RESEARCH/Instances.md` lists. RESEARCH re-specifies none of them; it reads their outputs through declared selectors.
- **No decision.** RESEARCH rules no selection; each decision is ruled by its owning area's ruling ticket, which this methodology governs. The ECS research conclusion and flagged proposal P1 (D-SEL-EnTT-SHIPPING, cited only as a flagged proposal, Z2-D-130) are ECS's; the shading-path conclusion is RPROD's (Z2-D-035 keeps Forward+ the default until a decision changes it).
- **No budget procedure.** Frame-rate definitions, the measured interval, qualification repetitions, budget breach consequences and budget regression BUGs are `Zenith2/Docs/Budgets.md`'s; RESEARCH applies the research half of `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure" and adds what it leaves open.
- **No evidence fingerprint.** The fingerprint and the workload runner are DEVT's (`Zenith2/Docs/Design/DEVT/Evidence.md` → "Fingerprint algorithm", → "Workload runner"); RESEARCH records and reuses them.
- **No gate script.** Every gate change RESEARCH needs is a PROC hand-off (`Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC").
- **No Luau surface.** RESEARCH adds no script binding, so it reserves no misuse-code block.

### Inventory items covered

| Item | Stage | Covered by |
|---|---|---|
| Z2-I-RESEARCH-01 the research methodology for ECS backends | S1 | the methodology, the ECS cell matrix and classification, the Stage 5 conformance |
| Z2-I-RESEARCH-02 the research methodology for shading paths | S1 | the methodology, the shading-path cell matrix and report sections |
| Z2-I-RESEARCH-03 native vs adapter measurement | S5 | "Native versus adapter measurement" and its analysis |
| Z2-I-RESEARCH-04 repetition procedure | S1 | the run procedure and the statistics |
| Z2-I-RESEARCH-05 CI procedure for research runs | S1 | controlled conditions and their checks, the CI procedure, revalidation |
| Z2-I-RESEARCH-06 raw-data retention | S5 | the formats, the caps, the large-file tier and retention; the formats land in Stage 1 because the Stage 1 rulings commit raw data |
| Z2-I-RESEARCH-07 conclusions as decisions | S5 | the decision ticket template, the report template, the DecisionLog research fields, the Stage 1 and Stage 5 conformance |

The full mapping of items to requirements is `Zenith2/Docs/Requirements/RESEARCH.md` → "Coverage of inventory items".

### Document family

| File | Headings |
|---|---|
| `Zenith2/Docs/Design/RESEARCH/Methodology.md` | Scope of this file; Measurement profiles (Profile rule table); Study design and pre-registration (Factors cells and metrics, Confound check, Hypothesis and acceptance rule, Fixed repetition count); Native versus adapter measurement (ECS cell matrix, Adapter overhead and share, Architectural and adapter classification, Shading path cell matrix); Run procedure (Sessions, Warm-up, Replication and interleaving, Schedule algorithm, Invalid repetitions and retries); Noise control on the reference desktop (Desktop session preflight, Desktop repetition checks, Desktop restoration, Desktop calibration); Noise control on the S24 (S24 session preflight, S24 repetition checks, Fixed-performance mode unavailable); Statistics (Arithmetic rules, Per-launch statistic, Median and its interval, Robust coefficient of variation, Drift test, Rank-sum test with ties, Shift estimate and its interval, Effect size, Multiplicity, Metric classes and thresholds, Verdicts, Deterministic metrics); Acceptance rule operators (Operator table, Rule grammar, Rule evaluation, Instance phrase mapping); Revalidation triggers (Trigger vocabulary, Trigger evaluation, Research regression rule); Procedure parameter list |
| `Zenith2/Docs/Design/RESEARCH/Formats.md` | Scope of this file; Research tree layout (Study directory, Run directory, Machines registry); study.json (study.json fields, Cell fields, Metric fields, Comparison fields, Extraction selectors); record.json (record.json fields, Invalidity reasons); Series files; Trace files; Raw files; analysis.json (Canonical JSON, analysis.json fields); Size caps and the text rule (Volume per study); Large-file tier (Large-file layout, Large-file verification); Retention; Format versioning; Report template (Report headings, Generated blocks, Study-specific sections); Decision ticket template (Template lines, Profile requirements for the template lines, Definition of Done lines); DecisionLog research fields; Justification record evidence line; revalidations.json |
| `Zenith2/Docs/Design/RESEARCH/Tooling.md` | Scope of this file; Files (PowerShell rules of the module, Result objects); Format functions; Statistics functions; Analysis and report functions; Research runner (Command shim); Large-file functions; Conformance checks (Check finding codes, Decision ticket check); Revalidation scan (Run comparison); Regression filing; CI procedure (Which benchmarks run in which gate, Regression thresholds, Registration in the test registry); Hand-offs to PROC |
| `Zenith2/Docs/Design/RESEARCH/Instances.md` | Scope of this file; Open decision instances; Research rulings without an open decision name; Qualitative rulings; Instance obligations |
| `Zenith2/Docs/Design/RESEARCH/Tests.md` | Scope of this file; Fixture sets; Statistics cases; Rule cases; Format cases; Check cases; Runner cases (Check-to-test matrix); Revalidation cases; Native versus adapter cases |

## Dependencies

### Areas and documents RESEARCH relies on

| Area or document | What RESEARCH uses | Heading |
|---|---|---|
| Budgets (core) | warm-up, repetitions and the order-statistic interval, controlled and ordinary conditions, the clock-lock and fixed-performance commands, the evidence record and per-frame CSV, the procedure parameters, reference hardware, the separation from qualification, the breach consequences | `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure", → "Reference hardware", → "Separation from research measurements", → "Breach consequences", → "Procedure parameters" |
| Rendering (core) | the shading-path report contents and the six-step procedure | `Zenith2/Docs/Rendering.md` → "Shading path research methodology" |
| DecisionLog (core) | the entry template, the ruling procedure, the statuses, the open decisions | `Zenith2/Docs/DecisionLog.md` → "How this log is written", → "Open decisions" |
| Architecture (core) | the named selection decisions, the ownership of `Benchmarks/` | `Zenith2/Docs/Architecture.md` → "Named selection decisions", → "Ownership per directory" |
| Contracts (core) | the custom infrastructure justification record | `Zenith2/Docs/Contracts.md` → "Custom infrastructure justification record" |
| Roadmap (core) | the Stage 1 deliverable "the research methodology", the integration cadence, board conventions | `Zenith2/Docs/Roadmap.md` → "Stage 1 deliverables", → "Integration cadence", → "Board conventions" |
| DEVT | `Get-Zenith2EvidenceFingerprint`, `Get-Zenith2TreeSha`, `Run-Workload.ps1` | `Zenith2/Docs/Design/DEVT/Evidence.md` → "Zenith2Evidence module", → "Workload runner" |
| SHIP | `zenith_register_pwsh_test`; `Get-Zenith2ManifestHashes`; the pinned `pwsh` (the module declares 7.4 or later; its rules are stated for 7.6.7 and proved by `Tests/Probes/Probe-PwshRules.ps1`); git-lfs installed by the setup script, on the reference desktop before the first Stage 1 research run | `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "zenith_register_script_test" |
| MOD | the hosted executable APK that runs a benchmark on the S24 | `Zenith2/Docs/Design/MOD/GameBuild.md` → "Hosted executable APKs" |
| build system | the benchmark helper and its `--smoke` CTest | `Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites" |
| ECS, RPROD, TWOD and every owner in the instance map | their harness outputs, read through selectors | `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances" |
| PROC | the Coverage step, `Zenith2/.gitattributes`, the repository-root `.lfsconfig`, the setup script's `filter.lfs.required`, the mid-sprint check ticket, the protected-path list | hand-offs P-RS1, P-RS2, P-RS4, P-RS5 and P-RS6 (`Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC") |

<!-- assumption: pending DEVT -->
`Get-Zenith2EvidenceFingerprint` is assumed to accept a benchmark executable target with `-Game` and `-Workload` omitted, returning `null` for `content` and `workloadDefinition` (cross-area row RS6). Until DEVT confirms, the runner fills those two members with `null` itself and computes the other five through the function's documented steps.
<!-- /assumption -->

### Interfaces RESEARCH declares for other areas

- **The study protocol**, `Zenith2/Docs/Research/<Study>/study.json`, which every harness ticket of an RA, RS, RJ, RB or RP decision writes (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json").
- **The extraction selectors**, through which a harness's own report schema is read with no harness code change (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "Extraction selectors").
- **The decision ticket template lines** and the two DoD lines every decision ticket carries.
- **The DecisionLog research fields** every research ruling appends.
- **The module functions** of `Zenith2/Docs/Design/RESEARCH/Tooling.md`, which the PROC Coverage step imports for P-RS1 and P-RS2.

## Library and file layout

### Directories

| Path | Content | Owner |
|---|---|---|
| `Zenith2/Benchmarks/Research/Zenith2Research.psm1`, `Zenith2Research.psd1` | the module | RESEARCH |
| `Zenith2/Benchmarks/Research/Run-Research.ps1` | the research runner | RESEARCH |
| `Zenith2/Benchmarks/Research/Invoke-ResearchCheck.ps1` | the conformance checks | RESEARCH |
| `Zenith2/Benchmarks/Research/Templates/Report.md.tmpl`, `RegressionTicket.md.tmpl` | templates | RESEARCH |
| `Zenith2/Benchmarks/Research/Tests/Zenith2Research.Tests.ps1`, `Tests/Fixtures/**` | assert cases and fixtures | RESEARCH |
| `Zenith2/Benchmarks/Research/Tests/Probes/Probe-PwshRules.ps1` | the runnable proof of every `pwsh` rule of the module | RESEARCH |
| `Zenith2/CMake/Tests/ZenithCMakeTests.cmake`, the RESEARCH block | one `zenith_register_pwsh_test` line per case | RESEARCH (the file is SHIP's) |
| `Zenith2/Docs/Research/Index.md`, `Machines/**` | the research index and the machine registry | RESEARCH |
| `Zenith2/Docs/Research/<Study>/**` | each study's protocol, runs, reports and revalidation log | the study's owning area (`Zenith2/Docs/Design/RESEARCH/Instances.md`); the format is RESEARCH's |
| `Zenith2/Benchmarks/<Harness>/**` | each harness | the measured area ([Z2-N-ARCH-021], `Zenith2/Docs/Architecture.md` → "Ownership per directory") |

There is no `Internal/`, `Internal/Windows|Android|Linux` or public header: RESEARCH compiles nothing. The directory `Zenith2/Benchmarks/Research/` holds no `CMakeLists.txt`, so `zenith_add_subdirectories(Benchmarks GROUPS Budgets)` adds no target for it.

### Supported matrix

No `ZENITH_SUPPORTED` declaration exists, because no target exists. The research tests register through `zenith_register_pwsh_test`, which registers in the `win` preset only; the runner runs on the reference desktop and drives the S24 over `adb` from it.

## Public API

### No C++ surface

RESEARCH declares no C++ type, function or header, and therefore no contract block, no handle kind and no factory. Its public surface is the module's exported functions, the two entry scripts, and the file formats.

### Module surface

Every exported function, its parameters and its return value are specified in `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Format functions", → "Statistics functions", → "Analysis and report functions", → "Large-file functions", → "Conformance checks" and → "Revalidation scan"; the PowerShell rules every function follows are → "PowerShell rules of the module". Naming follows DEVT's module: `<Verb>-Zenith2Research<Noun>` with approved PowerShell verbs; `FunctionsToExport` in the manifest lists exactly those functions, and a function absent from that list is private.

| Group | Functions |
|---|---|
| formats | `ConvertTo-Zenith2CanonicalJson`, `Read-Zenith2ResearchJson`, `Read-Zenith2ResearchStudy`, `Test-Zenith2ResearchStudy`, `Read-Zenith2ResearchRecord`, `Test-Zenith2ResearchRecord`, `Read-Zenith2ResearchSeries`, `ConvertTo-Zenith2ResearchTrace`, `Copy-Zenith2ResearchRaw`, `Get-Zenith2ResearchExtractedValue`, `Get-Zenith2MachineIdentity`, `Get-Zenith2MachineState`, `Update-Zenith2ResearchMachineState` |
| statistics | `Get-Zenith2ResearchMedian`, `Get-Zenith2ResearchPercentile`, `Get-Zenith2ResearchMedianInterval`, `Get-Zenith2ResearchRobustCv`, `Get-Zenith2ResearchDrift`, `Get-Zenith2ResearchRankSum`, `Get-Zenith2ResearchShift`, `Get-Zenith2ResearchCliffDelta`, `Get-Zenith2ResearchHolm`, `Get-Zenith2ResearchMargin`, `Get-Zenith2ResearchVerdict`, `New-Zenith2ResearchSchedule`, `Get-Zenith2ResearchSplitMix64`, `Invoke-Zenith2ResearchRule`, `Invoke-Zenith2ResearchStudyRule` |
| analysis and report | `Invoke-Zenith2ResearchAnalysis`, `New-Zenith2ResearchReport`, `Update-Zenith2ResearchReport`, `Test-Zenith2ResearchReport`, `New-Zenith2ResearchDecisionFields` |
| large files | `Test-Zenith2ResearchLargeFiles`, `Get-Zenith2ResearchStudyStorage` |
| checks | `Invoke-Zenith2ResearchCheck`, `Test-Zenith2DecisionTicket`, `Test-Zenith2DecisionLogResearchEntries`, `Test-Zenith2ResearchInstanceMap` |
| revalidation | `Get-Zenith2ResearchRevalidation`, `Compare-Zenith2ResearchRuns`, `Add-Zenith2ResearchRevalidation`, `New-Zenith2ResearchRegressionTicket` |
| runner internals exported for tests | `Invoke-Zenith2ResearchCommand`, `Get-Zenith2ResearchNowMs` |

### Entry scripts

| Script | Command line | Exit codes |
|---|---|---|
| `Run-Research.ps1` | `-Study <dir> -Platform Win|S24 -AmbientC <int> [-Session <index>] [-Serial] [-DisplayChecked] [-UpdatesPaused] [-Resume <RunId>] [-RestoreOnly] [-DryRun] [-Calibrate]` | "Runner exit codes" |
| `Invoke-ResearchCheck.ps1` | `-Mode committed|decisionlog|instances|tickets [-Path] [-Repo] [-Out] [-CurrentInputs] [-Staged]` | 0 clean, 1 findings, 2 usage or unreadable input |

### File formats as interfaces

| Format | Schema | Specified in |
|---|---|---|
| study protocol | `zenith2.research-study` 1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json" |
| run record | `zenith2.research-record` 1 | → "record.json" |
| series | `zenith2.research-series` v1 | → "Series files" |
| trace | `zenith2.research-trace` v1 | → "Trace files" |
| analysis | `zenith2.research-analysis` 1 | → "analysis.json" |
| machine registry | `zenith2.research-machine` 1, `zenith2.research-machines-current` 1 | → "Machines registry" |
| revalidation log | `zenith2.research-revalidations` 1 | → "revalidations.json" |
| revalidation scan output | `zenith2.research-revalidation-scan` 1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Revalidation scan" |
| report | the report template | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report template" |

## Data structures and formats

Every format is text, canonical and integer-only; the full specification, with every field, pattern and example, is `Zenith2/Docs/Design/RESEARCH/Formats.md`. The data model in one table:

| Entity | Identity | Holds | Lifetime |
|---|---|---|---|
| study | its path `<Study>` under `Zenith2/Docs/Research/` and its `studyVersion` | the protocol | for the life of the repository |
| run | `<YYYYMMDD>T<HHMM>Z-<win|s24>-<machine8>` | one session's record, series, raw outputs, large files and analysis | for the life of the repository |
| cell | `^[a-z0-9][a-z0-9-]{0,63}$` within the study | one configuration and its launch | the study version |
| series | (cell, metric) within a run | one integer per executed slot (one launch) | the run |
| comparison | its id within the study | candidate, baseline, metric, factor, primary flag | the study version |
| report | `Reports/<Z2-D-NNN>.md` | one ruling's evidence | for the life of the repository |
| machine | `machineId`, 12 hex | hashed hardware identity | while registered |
| large file | its LFS oid (SHA-256) | a trace, a Budgets per-frame CSV, a binary capture or an over-cap text file, under the run's `archive/` | for the life of the repository |

Byte sizes: a series row is at most 120 bytes; the longest series (20 repetitions, each retried 3 times, and the warm-up) has 81 rows, under 9.5 KiB. A trace row is 36 to 40 bytes, and a desktop launch at 144 to 300 fps over the 120 s measured interval has 17,280 to 36,000 frames, so a trace is 0.62 to 1.44 MB and the launch's per-frame CSV beside it 1.0 to 2.3 MB; both are LFS objects, and the volume per study is `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Volume per study".

## Algorithms and state machines

### Algorithm index

| Algorithm | Complexity | Specified in |
|---|---|---|
| schedule (SplitMix64 Fisher–Yates, a fresh shuffle per repetition) | O(cells × n) | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Schedule algorithm" |
| median and order-statistic interval | O(n log n) | → "Median and its interval" |
| robust coefficient of variation | O(n log n) | → "Robust coefficient of variation" |
| drift test (exact Kendall inversions) | O(n³) | → "Drift test" |
| exact rank-sum test with ties | O(G × m × S × t), about 5,600 states at m = n = 20 | → "Rank-sum test with ties" |
| shift estimate and exact interval | O(mn log mn + m²n²) | → "Shift estimate and its interval" |
| verdicts and the rule evaluation | linear in comparisons and terms | → "Verdicts", → "Rule evaluation" |
| adapter share and classification | per (scenario, backend pair) constant | → "Adapter overhead and share", → "Architectural and adapter classification" |
| confound check | per comparison, constant | → "Confound check" |
| canonical JSON writer | linear in the output | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Canonical JSON" |
| decision ticket check | linear in the ticket bytes | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check" |
| revalidation scan | linear in entries × triggers, plus one `git diff` per `target` trigger | → "Revalidation scan" |

### Session state machine

| State | Event | Next state | Action |
|---|---|---|---|
| Idle | start | Validating | read the study, the tree and the executables |
| Validating | inputs valid | Locking | — |
| Validating | input error, or git-lfs absent or not wired for `archive/` (`RESEARCH_ERROR_NOT_LFS`) | Refused (exit 2) | print the error |
| Locking | build slot taken (Windows) or not needed (S24) | Identifying | — |
| Locking | slot held elsewhere | Refused (exit 3) | — |
| Identifying | registered reference machine, no unrestored state | Preflight | create the work directory |
| Identifying | unregistered machine | Refused (exit 3) | — |
| Identifying | unrestored state | Refused (exit 5) | `-RestoreOnly` restores |
| Preflight | every check passes | Scheduled | write the schedule |
| Preflight | a check fails | Restoring then Refused (exit 3) | restore the conditions |
| Scheduled | next slot | Slot (the slot state machine) | — |
| Slot | slot ends, slots remain | Scheduled | — |
| Slot | last slot ends | Restoring | — |
| Slot or Scheduled | device lost, operator abort or 12-hour cap | Restoring then Aborted (exit 4) | keep the work directory for `-Resume` on the same boot |
| Restoring | restored | Assembling | record `session.restored` |
| Assembling | non-LFS files within the run cap | Done (exit 0, or 1 if a cell is INSUFFICIENT) | write the run directory with `archive/`, the analysis; print `git add` and the staged check for the human |
| Assembling | non-LFS files over the run cap | Refused (exit 2, `RESEARCH_ERROR_CAP`) | nothing under `Zenith2/Docs/`; the work directory is kept and the study is split into sessions |

### Slot state machine

| State | Event | Next state | Action |
|---|---|---|---|
| Pending | start | PreChecks | — |
| PreChecks | checks pass | Running | launch |
| PreChecks | thermal not reached within the wait cap, clocks drifted, background busy, battery low | Invalid | write the reason; append a retry if fewer than 3 for this (cell, repetition) |
| Running | exit 0 within the timeout | PostChecks | — |
| Running | non-zero exit or timeout | Invalid (`EXIT`) | as above |
| PostChecks | checks pass | Extracting | pull the output (S24) |
| PostChecks | thermal status ≥ 2 (S24) or device reset (desktop) | Invalid (`THERMAL`, `DEVICE`) | as above |
| Extracting | every selector unique and integer | Valid | write the series rows |
| Extracting | a selector fails | Invalid (`EXTRACT`) | as above |

### Research decision lifecycle

| State | Event | Next state | Action |
|---|---|---|---|
| Open (DecisionLog `### D-NAME`) | harness ticket Done | Pre-registered | `study.json` committed and valid |
| Pre-registered | run ticket Done (or the ruling ticket's runs) | Measured | run directories committed, the check passing |
| Measured | ruling ticket Done | Ruled | the report and the entry with the research fields committed, the open entry `Ruled as Z2-D-NNN` |
| Ruled | a trigger fires | Revalidating | the mid-sprint check schedules a revalidation run |
| Revalidating | `stands` or `stands-drifted` | Ruled | a `revalidations.json` entry |
| Revalidating | `regressed` | Re-ruling | a re-ruling ticket filed |
| Re-ruling | re-ruling ticket Done | Ruled (the new entry; the old one superseded through its Decision text) | — |

## Threading lifetime and error contract

### Execution model

Every RESEARCH script runs in one single-threaded `pwsh` process; no function starts a job, a runspace or a thread. External commands run synchronously through `Invoke-Zenith2ResearchCommand`. The analysis and checks hold no state between calls: each reads files and returns a value. No frame-graph node, no engine thread and no lock-order table entry is involved, because no RESEARCH code runs inside an engine process.

### Locks and exclusivity

- A desktop session holds the machine-wide build slot (`Tools/build_lock.ps1 -Run`) from step 2 of the runner to its exit, so no lane build or gate run shares the measuring machine.
- Two runner instances on one machine are excluded by the same slot; on the S24, the runner refuses a second session to a device whose `adb shell getprop debug.zenith2.research` reads non-empty (the runner sets it to the RunId at session start with `setprop` and clears it at exit).

### Runner error codes

| Code | Meaning |
|---|---|
| `RESEARCH_ERROR_SCHEMA` | a file fails its schema or names an unknown version |
| `RESEARCH_ERROR_RULE` | the acceptance rule does not parse |
| `RESEARCH_ERROR_DIRTY_TREE` | the measured tree has uncommitted changes |
| `RESEARCH_ERROR_NOT_BUILT` | an executable a cell launches is absent from the Release output |
| `RESEARCH_ERROR_NOT_LFS` | git-lfs is not installed, `filter.lfs.clean` is unset, `filter.lfs.required` is not `true`, or the run's `archive/` path does not carry `filter=lfs` |
| `RESEARCH_ERROR_SLOT_BUSY` | the machine-wide build slot is held, or the S24 has a session marker |
| `RESEARCH_ERROR_NOT_REFERENCE` | the machine is not a registered reference machine for the platform |
| `RESEARCH_ERROR_UNRESTORED` | a previous session's clock locks or power scheme are still in place |
| `RESEARCH_ERROR_PREFLIGHT` | a session preflight check failed (named) |
| `RESEARCH_ERROR_BOOT_CHANGED` | `-Resume` on a different boot |
| `RESEARCH_ERROR_CAP` | the non-LFS files of a run would pass the run-directory cap |
| `RESEARCH_ERROR_DEVICE_LOST` | `adb` lost the device, or the GPU required a reset outside a slot |
| `RESEARCH_ERROR_SESSION_CAP` | the session plan exceeds 12 hours, or the session passed 12 hours |

The check finding codes are `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Check finding codes"; the slot invalidity reasons are `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Invalidity reasons"; the decision ticket codes are `RESEARCH_TICKET_LINE_<LABEL>`, `RESEARCH_TICKET_NEEDS_HUMAN`, `RESEARCH_TICKET_STUDY_PRODUCER`, `RESEARCH_TICKET_WORKER_EVIDENCE`, `RESEARCH_TICKET_DOD` and `RESEARCH_TICKET_TRIGGER`.

### Runner exit codes

| Exit | Meaning | Written under `Zenith2/Docs/` |
|---|---|---|
| 0 | every cell has its n valid repetitions | the run directory |
| 1 | at least one cell is INSUFFICIENT | the run directory (its INVALID comparisons shown) |
| 2 | input error, git-lfs missing or unwired (`RESEARCH_ERROR_NOT_LFS`), a session plan over the cap, or a run over the run-directory cap | nothing |
| 3 | refused before measuring: preflight, busy slot (the build slot's own exit 11 is mapped to 3; every other exit of the wrapped runner passes through), unregistered machine | nothing |
| 4 | aborted session (device lost, operator abort, session cap) | nothing; the work directory is kept |
| 5 | unrestored machine state | nothing |

### Errors versus assertions

There are no assertions: every unexpected input is a result object with an error code, every refused run exits with its code and message, and only a programmer error (a parameter of the wrong type) throws. A check never throws on a malformed committed file; it reports the file.

## Diagnostics and profiling

- The runner writes `Zenith2/out/<checkout-hash>/research/<RunId>/runner.log`, one line per event (`<unixMs> <slot> <event> <detail>`), and commits it under the run's `archive/` as kind `log` at assembly.
- Progress: one console line per slot, `slot <k>/<total> <cell> r<rep> <valid|invalid reason> <value> <unit>`, and one summary line per cell at the end with n, median and state.
- The checks write `Zenith2/out/<checkout-hash>/research/check-<mode>.json` with every finding; the revalidation scan writes `revalidation.json` there.
- The report is the human diagnostic: its "Exclusions and threats to validity" section lists every invalid repetition by reason, `notApplicable` cell, fixed-performance unavailability and archive loss.
- Dashboards: research data is not evidence for a budget row, so DEVT's regression dashboards read `Zenith2/Docs/Research/Budgets/**` alone; a research series trend is read across the `revalidations.json` entries of a study.

## Authoring and cooking

RESEARCH has no editor surface and no cook step. Its authoring is text written by tickets: `study.json` by the harness ticket (a worker can write it, and `Pwsh_Research_StudyFilesValid` proves it), the report's written sections and the DecisionLog fields by the human in the ruling ticket (`New-Zenith2ResearchReport` and `New-Zenith2ResearchDecisionFields` generate the rest), and `Machines/*.json` by the human who registers a reference machine. Research files are evidence, never migrated authoring documents (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "Format versioning").

## Failure handling

| Failure | Behaviour |
|---|---|
| missing or corrupt `study.json`, record or series | the reader returns `RESEARCH_ERROR_SCHEMA` naming the file and line; the runner exits 2; the check reports `RESEARCH_CHECK_SCHEMA` and the Test step fails |
| a harness crash or timeout during a slot | the slot is invalid with `EXIT` and retried, at most 3 times; then the cell is INSUFFICIENT |
| a harness output that a selector cannot read | the slot is invalid with `EXTRACT`; the study's selector is the defect, fixed by a harness ticket before a new run |
| the S24 disconnects or the desktop GPU resets | the session aborts with exit 4 after restoring, nothing is written under `Zenith2/Docs/`, the work directory remains for `-Resume` on the same boot |
| thermal limits not reached | the slot is invalid with `THERMAL` and retried in the next slot group, after that group's own slots, giving the device time to cool; a cell losing more than 20% of its slots, or 10 points more than the cell it is compared with, carries `INVALID_RATE` |
| fixed-performance mode refused by the S24 | the run continues `controlled` with the stricter limits and the report says so |
| the runner is killed before restoration | the next start refuses with exit 5 until `-RestoreOnly` restores |
| a committed run over its cap | the runner refuses to assemble one (exit 2); a hand-edited one fails the check |
| an `archive/` file committed as a plain blob | `RESEARCH_CHECK_NOT_LFS` (git alone, no git-lfs needed); `filter.lfs.required` and the runner's precondition make it unreachable from a correctly set-up clone |
| an LFS object missing from the store, or a pointer that disagrees with the record | `RESEARCH_CHECK_SHA_MISMATCH` or `RESEARCH_CHECK_MISSING_FILE`; the store is the repository's LFS remote, restored like any LFS asset, and no series value depends on it |
| an analysis or report edited by hand | `RESEARCH_CHECK_ANALYSIS_STALE` or `RESEARCH_CHECK_REPORT_STALE` fails the Test step |
| a DecisionLog entry disagreeing with its analysis | `RESEARCH_CHECK_DECISION_FIELDS` fails the Test step |
| an acceptance rule evaluating to INVALID | the outcome is `blocked`; the ruling ticket stays Blocked naming the terms (a term whose comparison lies in none of the ruling's runs is INVALID too) |
| a pending budget row in a `meets` term | the term is INVALID; the ruling ticket is blocked by the row's producer through its `blocked_by` |
| allocation failure, device loss inside the engine | not RESEARCH's: a harness or Player failure surfaces as `EXIT` |
| a fired revalidation trigger left unhandled at a stage acceptance | the scan with `-Final` exits 1 and the acceptance does not complete |

## Test strategy

Every test is a `Pwsh_Research_<Case>` CTest entry of the `Test` step on the `win` preset, listed with the case it asserts in `Zenith2/Docs/Design/RESEARCH/Tests.md`: the statistics cases with the worked values of the methodology, the rule cases, the format cases, the check cases (fixtures and the real committed tree), the runner cases against command shims, the archive and revalidation cases, and the native-versus-adapter cases. No test measures anything: the measurements are the `needs-human` ruling and run tickets' (WORKER-EVIDENCE), and every expected value is derivable by hand or computed by an independent oracle inside the test. The gate mapping is `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate".

## Budget rows

### Rows RESEARCH proposes

| Proposed row | Name | Status | Value | Unit | Procedure | Platform | Tolerance | Breach | Producer |
|---|---|---|---|---|---|---|---|---|---|
| RESEARCH-001 | Research committed file size cap | pending | pending (provisional 4 MiB) | bytes | MP-RESEARCHSTORE | Repo | 0 (hard) | BR-TEST | Stage 1 per-system budget ticket for RESEARCH |
| RESEARCH-002 | Research committed run directory size cap | pending | pending (provisional 32 MiB, non-LFS bytes) | bytes | MP-RESEARCHSTORE | Repo | 0 (hard) | BR-TEST | Stage 1 per-system budget ticket for RESEARCH |
| RESEARCH-003 | Research large-file storage per study | pending | pending (provisional 2 GiB) | bytes | MP-RESEARCHSTORE | Repo | default | BR-REPORT | Stage 1 per-system budget ticket for RESEARCH |

- The rows are recorded as pending with their producer named; cross-document item X416 adds them to `Zenith2/Docs/Budgets.md` with the IDs RESEARCH-001 to RESEARCH-003 under the `Z2-B-` prefix, the procedure `MP-RESEARCHSTORE` (the committed byte sizes `Invoke-ResearchCheck.ps1 -Mode committed` computes, `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Size caps and the text rule"), and the RESEARCH row of "Per-system producer shape" (no harness half, basis `published`).
- The producer is a single `needs-human` ruling ticket, blocked by no other allocation; text research files are not binary content, so the LFS tracking threshold row Z2-B-CONTENT-001 does not bound them.
- Until the rows are adopted the check uses the provisional values, declared as module constants; the ruling confirms or changes them.

### Rows RESEARCH is measured against

| Row | Why |
|---|---|
| Z2-B-SHIP-003, Z2-B-SHIP-004, Z2-B-SHIP-005 | a frame-workload research repetition is valid only when its evidence record meets the run-length and exclusion rows (`EVIDENCE` invalidity) |

Every row an acceptance rule names in a `meets` term (Z2-B-CORE-001, Z2-B-PHYS-002, Z2-B-TWOD-004, Z2-B-UI-002 and the rest of `Zenith2/Docs/Design/RESEARCH/Instances.md`) belongs to its owning area and is read, never owned, by RESEARCH.

## Stage plan

### Stage 1

Fix version `Z2 Stage 1`, Stage 1 sprint (Roadmap deliverable 18, "the research methodology"):

1. API-only slice: the formats and their validators (`study.json`, `record.json`, series, trace, analysis, canonical JSON, the machine registry), the fixtures, the format cases, `Tests/Probes/Probe-PwshRules.ps1` and its case; the RESEARCH block of the test registry. It lands first, so the Stage 1 harness tickets can write `study.json` against a validator.
2. The statistics functions, the rule grammar and evaluator (`Invoke-Zenith2ResearchRule` and the study-level `Invoke-Zenith2ResearchStudyRule`), the schedule, with their cases.
3. The analysis, the report template and functions, `Invoke-ResearchCheck.ps1` with the `committed`, `decisionlog`, `instances` and `tickets` modes, the decision ticket check, the instance map check.
4. The runner with the command shim, the desktop and S24 preflight and repetition checks, restoration, assembly, and the runner cases.
5. The large-file functions (pointer and object verification, the git-only pointer and attribute check, the storage measurement) and their cases.
6. The revalidation scan, the run comparison, the regression ticket body, and their cases.
7. The Stage 1 per-system budget ticket for RESEARCH (`needs-human`), allocating the two rows.
8. The Stage 1 research conformance integration ticket (`needs-human`): the desktop calibration (`Run-Research.ps1 -Calibrate`, `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop calibration"), and the Stage 1 rulings' committed runs and entries passing the checks in the Stage 1 acceptance run.

Every Stage 1 measured ruling (D-SLANG, D-SEL-HEAP, D-SEL-HASH, D-SEL-JSON, D-SEL-PHYSX-ANDROID, D-JOLT-PARALLEL-STEP, D-LUAU-STATELESS, D-SCRIPT-TXN, the per-system budget rulings with a harness half) is blocked by slices 1 to 5, because its ruling diff must pass `Pwsh_Research_CheckCommitted`. The PROC hand-offs P-RS1, P-RS2, P-RS4, P-RS5 and P-RS6 are Stage 0 or Stage 1 PROC tickets; P-RS4 (the `.gitattributes` block, the committed `.lfsconfig`, `filter.lfs.required` and git-lfs on the reference desktop) and P-RS6 land in Stage 1 before the first research run is committed, not with CONTENT's Stage 3 setup.

### Stage 2 to Stage 4

- Stage 2: the justification-record rulings (RJ) and the regression rule's first use at the Stage 2 mid-sprint check; the D-SEL-2DPHYS, D-SEL-CODEC, D-SEL-VIDEO, D-SEL-CRASH and D-CLASS-SIM rulings follow the methodology.
- Stage 3: D-VTX-TANGENTLESS (Forward+), D-SEL-MOTIONMATCH.
- Stage 4: D-SEL-FBX.
- Every stage: the mid-sprint research steps and the stage acceptance's revalidation scan.

### Stage 5

Fix version `Z2 Stage 5`: the native-versus-adapter analysis (the classification and adapter tables, the EnTT variants) and the shading-path report sections land as one implementation slice, pulled forward into the sprint of the first ECS research harness that writes `study.json` (the pull-forward rule, `Zenith2/Docs/Roadmap.md` → "Pull-forward rule"); the Stage 5 research conformance integration ticket checks the ECS and shading-path research, D-TBDR, D-SEL-UPSCALER and the Stage 5 D-VTX-TANGENTLESS runs in the Stage 5 acceptance.

### Stage 6 and Stage 7

No new RESEARCH code: the revalidation scan runs in the Stage 6 and Stage 7 acceptances, re-reading every research conclusion against the shipping toolchain and hardware, and every regression is filed through "Regression filing".

## Decisions

### Settled decisions governing RESEARCH

| Decision | What it binds here |
|---|---|
| Z2-D-064 | the methodology itself: native and adapter measurement, controlled conditions, repetitions, medians with 95% intervals, raw data committed, conclusions as decision records |
| Z2-D-084 | the harness and ruling split; measurements are `needs-human` |
| Z2-D-097 | rulings are appended to `Zenith2/Docs/DecisionLog.md` in the ruling ticket's diff |
| Z2-D-012 | Budgets v1: the warm-up, repetition and condition rules RESEARCH applies |
| Z2-D-065 | evidence fingerprints, recorded per cell |
| Z2-D-035 | Forward+ stays the default until a decision changes it |
| Z2-D-039 | three ECS backends, measured natively and through the adapter |
| Z2-D-041 | justification records with measured acceptance criteria |
| Z2-D-130 | flagged proposals P1 and P2 recorded, not applied |
| Z2-D-126 | `human-gate` is not used for Z2 decisions |
| Z2-D-083 | stage acceptance runs outside the loop with every lane idle |

### Open decisions RESEARCH depends on

- D-SEL-JSON (open, Stage 1) is not a dependency: the tooling parses JSON with `pwsh`'s own parser, which is why it is PowerShell and not C++.
- D-SEL-EnTT-SHIPPING (open, Stage 5, only as flagged proposal P1): whether the ECS research report feeds a shipping ruling; the report is produced either way.
- Every other open decision is an instance, not a dependency (`Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances").

### Selection decisions RESEARCH owns

None. The methodology's own numbers (margins, noise limits, drift level, family cap) are procedure parameters, adopted into `Zenith2/Docs/Budgets.md` → "Procedure parameters" by cross-document item X416 and changed thereafter only by a decision with a workload version bump ([Z2-N-BUDGET-003]).

<!-- informative -->
Two core documents disagree on the interval of a research median: `Zenith2/Docs/Budgets.md` → "Repetitions and statistics" gives the distribution-free order-statistic interval, and the Rendering bullet [Z2-N-REND-958] gives a percentile bootstrap. RESEARCH adopts the order-statistic interval, which is exact and reliable at 10 launches where a bootstrap of a median is not, and cross-document item X417 aligns the Rendering bullet. Retention changes no settled decision: raw research data stays committed under `Zenith2/Docs/Research/` as Z2-D-064 and [Z2-N-BUDGET-079] say, with traces, per-frame CSVs and binary captures as git LFS objects under each run's `archive/`, the mechanism [Z2-N-BUDGET-177] already provides for large binary content.
<!-- /informative -->

## Requirement index

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-RESEARCH-001 | One methodology for every measured decision | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Measurement profiles"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Template lines"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check" |
| Z2-R-RESEARCH-002 | Profile rule table | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Profile rule table"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json fields" |
| Z2-R-RESEARCH-003 | Instance map complete | S1 | `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Conformance checks" |
| Z2-R-RESEARCH-004 | Workers never measure | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Profile requirements for the template lines"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC" |
| Z2-R-RESEARCH-005 | Research separate from qualification | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Operator table"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields" |
| Z2-R-RESEARCH-006 | Study committed before the run | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Study design and pre-registration"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json" |
| Z2-R-RESEARCH-007 | Study schema | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json fields"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Cell fields"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Metric fields"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Comparison fields" |
| Z2-R-RESEARCH-008 | Hypothesis alternatives and rule | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Hypothesis and acceptance rule"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Rule grammar" |
| Z2-R-RESEARCH-009 | Families and the primary comparison cap | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Multiplicity"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Factors cells and metrics" |
| Z2-R-RESEARCH-010 | Confound check by factor | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Confound check" |
| Z2-R-RESEARCH-011 | Incumbent rule | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Hypothesis and acceptance rule"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json fields" |
| Z2-R-RESEARCH-012 | Fixed repetition count | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Fixed repetition count" |
| Z2-R-RESEARCH-013 | Not-applicable cells reported | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Factors cells and metrics"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Cell fields" |
| Z2-R-RESEARCH-014 | Reference hardware only | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 session preflight"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner" |
| Z2-R-RESEARCH-015 | ECS native and adapter cells | S5 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "ECS cell matrix" |
| Z2-R-RESEARCH-016 | Adapter overhead estimate | S5 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Adapter overhead and share" |
| Z2-R-RESEARCH-017 | Adapter share bound | S5 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Adapter overhead and share" |
| Z2-R-RESEARCH-018 | Architectural comparison | S5 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Architectural and adapter classification" |
| Z2-R-RESEARCH-019 | Difference classification | S5 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Architectural and adapter classification"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "analysis.json fields" |
| Z2-R-RESEARCH-020 | Job-scheduling overhead per backend | S5 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "ECS cell matrix" |
| Z2-R-RESEARCH-021 | EnTT adapter cost rows reported | S5 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "ECS cell matrix" |
| Z2-R-RESEARCH-022 | Shading path cells share inputs | S5 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Shading path cell matrix"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Confound check" |
| Z2-R-RESEARCH-023 | Shading path metrics | S5 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Shading path cell matrix" |
| Z2-R-RESEARCH-024 | Shading path report sections | S5 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings" |
| Z2-R-RESEARCH-025 | Deterministic metrics compared by value | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Deterministic metrics" |
| Z2-R-RESEARCH-026 | Warm-up slot | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Warm-up" |
| Z2-R-RESEARCH-027 | Launch is the unit of replication | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Replication and interleaving"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Extraction selectors" |
| Z2-R-RESEARCH-028 | Interleaved deterministic schedule | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Schedule algorithm" |
| Z2-R-RESEARCH-029 | Invalid slots retried inside the interleaving | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Invalid repetitions and retries"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Invalidity reasons" |
| Z2-R-RESEARCH-030 | Insufficient cells | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Invalid repetitions and retries" |
| Z2-R-RESEARCH-031 | One session one boot | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions" |
| Z2-R-RESEARCH-032 | Session plan within the cap | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json fields" |
| Z2-R-RESEARCH-033 | Exclusive desktop | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions"; `Zenith2/Docs/Design/RESEARCH.md` → "Locks and exclusivity" |
| Z2-R-RESEARCH-034 | Integer arithmetic only | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Arithmetic rules" |
| Z2-R-RESEARCH-035 | JSON read as lexemes | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "PowerShell rules of the module"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Extraction selectors" |
| Z2-R-RESEARCH-036 | Ordinal order and the no-BOM LF writer | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "PowerShell rules of the module" |
| Z2-R-RESEARCH-037 | Per-launch statistic | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Per-launch statistic" |
| Z2-R-RESEARCH-038 | Median and order-statistic interval | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Median and its interval" |
| Z2-R-RESEARCH-039 | Run-to-run variance reported | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Robust coefficient of variation" |
| Z2-R-RESEARCH-040 | Noise bands | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Metric classes and thresholds"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Verdicts" |
| Z2-R-RESEARCH-041 | Drift test | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Drift test" |
| Z2-R-RESEARCH-042 | Exact rank-sum test with ties | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Rank-sum test with ties" |
| Z2-R-RESEARCH-043 | Shift estimate and Bonferroni interval | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Shift estimate and its interval"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Multiplicity" |
| Z2-R-RESEARCH-044 | No interval rank available | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Shift estimate and its interval" |
| Z2-R-RESEARCH-045 | Ties and spread reasons | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Shift estimate and its interval"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings" |
| Z2-R-RESEARCH-046 | Effect size and Holm values reported | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Effect size"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Multiplicity" |
| Z2-R-RESEARCH-047 | Margins per metric class | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Metric classes and thresholds" |
| Z2-R-RESEARCH-048 | Verdict rules | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Verdicts" |
| Z2-R-RESEARCH-049 | Acceptance rule operators | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Operator table" |
| Z2-R-RESEARCH-050 | Rule evaluation | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Rule evaluation"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Statistics functions" |
| Z2-R-RESEARCH-051 | Instance phrase mapping | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Instance phrase mapping"; `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances" |
| Z2-R-RESEARCH-052 | Byte-stable analysis | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Canonical JSON"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Arithmetic rules" |
| Z2-R-RESEARCH-053 | Desktop power plan locked | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop session preflight" |
| Z2-R-RESEARCH-054 | Desktop GPU clocks locked and verified | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop session preflight"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop repetition checks" |
| Z2-R-RESEARCH-055 | Desktop background load bounded | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop session preflight"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop repetition checks" |
| Z2-R-RESEARCH-056 | Desktop thermal state bounded | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop repetition checks" |
| Z2-R-RESEARCH-057 | Desktop clock events during a slot | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop repetition checks" |
| Z2-R-RESEARCH-058 | Desktop slot exit and reset | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop repetition checks" |
| Z2-R-RESEARCH-059 | Desktop display and update state | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop session preflight" |
| Z2-R-RESEARCH-060 | Desktop restoration | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop restoration" |
| Z2-R-RESEARCH-061 | Pre-session reboot and idle | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions" |
| Z2-R-RESEARCH-062 | Desktop calibration | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop calibration" |
| Z2-R-RESEARCH-063 | S24 fixed-performance mode | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 session preflight"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Fixed-performance mode unavailable" |
| Z2-R-RESEARCH-064 | S24 thermal gating | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 repetition checks" |
| Z2-R-RESEARCH-065 | S24 device state controlled | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 session preflight"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 repetition checks" |
| Z2-R-RESEARCH-066 | S24 reference device only | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 session preflight"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields" |
| Z2-R-RESEARCH-067 | Machine identity | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Machines registry" |
| Z2-R-RESEARCH-068 | Software state recorded | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Machines registry" |
| Z2-R-RESEARCH-069 | Current machine state | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Machines registry" |
| Z2-R-RESEARCH-070 | Build identity recorded | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner" |
| Z2-R-RESEARCH-071 | Run directory layout | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Run directory"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Research tree layout" |
| Z2-R-RESEARCH-072 | Record schema | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields" |
| Z2-R-RESEARCH-073 | Series format | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Series files" |
| Z2-R-RESEARCH-074 | Trace format | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Trace files" |
| Z2-R-RESEARCH-075 | Raw outputs with normalised line endings | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Raw files" |
| Z2-R-RESEARCH-076 | Extraction selectors | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Extraction selectors" |
| Z2-R-RESEARCH-077 | Text rule | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Size caps and the text rule" |
| Z2-R-RESEARCH-078 | Committed size caps | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Size caps and the text rule" |
| Z2-R-RESEARCH-079 | Placement by kind | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Run directory"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Size caps and the text rule" |
| Z2-R-RESEARCH-080 | Large-file storage reported | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Size caps and the text rule"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Volume per study" |
| Z2-R-RESEARCH-081 | Large-file tier in git LFS | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file tier"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file layout" |
| Z2-R-RESEARCH-082 | Large-file verification without the objects | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file verification" |
| Z2-R-RESEARCH-083 | Retention for the life of the repository | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Retention" |
| Z2-R-RESEARCH-084 | Reproducibility from the committed tier | S5 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file tier"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Generated blocks" |
| Z2-R-RESEARCH-085 | Format versioning | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Format versioning" |
| Z2-R-RESEARCH-086 | Raw data retention for the Stage 5 research | S5 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file verification"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Run directory" |
| Z2-R-RESEARCH-087 | Decision ticket template lines | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Template lines"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check" |
| Z2-R-RESEARCH-088 | Study producer blocks the ruling | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Profile requirements for the template lines" |
| Z2-R-RESEARCH-089 | Decision ticket Definition of Done | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Definition of Done lines" |
| Z2-R-RESEARCH-090 | Report template | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report template"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings" |
| Z2-R-RESEARCH-091 | Generated report blocks | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Generated blocks" |
| Z2-R-RESEARCH-092 | DecisionLog research fields | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Conformance checks" |
| Z2-R-RESEARCH-093 | No silent default change | S5 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields" |
| Z2-R-RESEARCH-094 | Justification record evidence line | S2 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Justification record evidence line" |
| Z2-R-RESEARCH-095 | Probe result table | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings" |
| Z2-R-RESEARCH-096 | Deviations declared | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Study design and pre-registration" |
| Z2-R-RESEARCH-097 | Budget allocation runs in research format | S1 | `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Research rulings without an open decision name"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Study directory" |
| Z2-R-RESEARCH-098 | Committed research check in the Test step | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Conformance checks"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Check finding codes" |
| Z2-R-RESEARCH-099 | Decision ticket check on the board | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check" |
| Z2-R-RESEARCH-100 | Research index | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Research tree layout" |
| Z2-R-RESEARCH-101 | Gate membership of research checks | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Registration in the test registry" |
| Z2-R-RESEARCH-102 | Revalidation trigger vocabulary | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Trigger vocabulary" |
| Z2-R-RESEARCH-103 | Revalidation scan | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Revalidation scan"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Trigger evaluation" |
| Z2-R-RESEARCH-104 | Revalidation log | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "revalidations.json" |
| Z2-R-RESEARCH-105 | Research regression rule | S2 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Research regression rule"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Run comparison" |
| Z2-R-RESEARCH-106 | Regression filing | S2 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Regression filing" |
| Z2-R-RESEARCH-107 | Mid-sprint machine state recorded | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC" |
| Z2-R-RESEARCH-108 | Stage acceptance revalidation scan | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate" |
| Z2-R-RESEARCH-109 | Research regressions separate from budget regressions | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Regression thresholds" |
| Z2-R-RESEARCH-110 | PROC hand-offs filed | S0 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC" |
| Z2-R-RESEARCH-111 | Runner command shim | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Command shim" |
| Z2-R-RESEARCH-112 | Runner completion exit codes | S1 | `Zenith2/Docs/Design/RESEARCH.md` → "Runner exit codes"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner" |
| Z2-R-RESEARCH-113 | Runner never builds | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner" |
| Z2-R-RESEARCH-114 | Dry run changes nothing | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner" |
| Z2-R-RESEARCH-115 | Stage 1 research conformance | S1 | `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate" |
| Z2-R-RESEARCH-116 | Stage 5 research conformance | S5 | `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Research rulings without an open decision name"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings" |
| Z2-R-RESEARCH-117 | End-to-end synthetic study | S1 | `Zenith2/Docs/Design/RESEARCH/Tests.md` → "Fixture sets" |
| Z2-R-RESEARCH-118 | Shading path conclusion as decision | S5 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields"; `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Research rulings without an open decision name" |
| Z2-R-RESEARCH-119 | ECS conclusion as decision | S5 | `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Architectural and adapter classification" |
| Z2-R-RESEARCH-120 | Machine registry change by commit only | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Machines registry"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner" |
| Z2-R-RESEARCH-121 | Revalidation in the shipping stages | S6 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Trigger evaluation" |
| Z2-R-RESEARCH-122 | Study-level outcome in the report | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Analysis and report functions"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Generated blocks"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields" |
| Z2-R-RESEARCH-123 | Rule definitions | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Rule grammar"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json fields" |
| Z2-R-RESEARCH-124 | Invalid rate flagged | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Invalid repetitions and retries"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings" |
| Z2-R-RESEARCH-125 | Archive blobs are LFS pointers | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file verification"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Large-file functions" |
| Z2-R-RESEARCH-126 | Runner refuses without git-lfs | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"; `Zenith2/Docs/Design/RESEARCH.md` → "Runner error codes" |
| Z2-R-RESEARCH-127 | Research objects excluded from every fetch | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file layout"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC" |
| Z2-R-RESEARCH-128 | Research path lengths | S1 | `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Study directory"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Check finding codes" |
| Z2-R-RESEARCH-129 | PowerShell rules proved by a probe | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "PowerShell rules of the module"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Files" |
| Z2-R-RESEARCH-130 | Decision ticket check reads committed outputs | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Profile requirements for the template lines" |
| Z2-R-RESEARCH-131 | Each session committed before the next | S1 | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner" |
| Z2-R-RESEARCH-132 | Mid-sprint revalidation scan | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Revalidation scan" |
| Z2-R-RESEARCH-133 | Mid-sprint revalidation runs | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Run comparison" |
| Z2-R-RESEARCH-134 | Stage acceptance current-inputs check | S1 | `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Conformance checks"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate" |
| Z2-R-RESEARCH-135 | Runner refusal exit codes | S1 | `Zenith2/Docs/Design/RESEARCH.md` → "Runner exit codes"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner" |
| Z2-R-RESEARCH-136 | Runner abort and unrestored exit codes | S1 | `Zenith2/Docs/Design/RESEARCH.md` → "Runner exit codes"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner" |
