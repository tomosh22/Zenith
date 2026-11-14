# Zenith2 requirements: Research (RESEARCH)

## Purpose

This catalogue breaks the RESEARCH inventory items of `Zenith2/Docs/Requirements/Inventory.md` → "Inventory v1" into testable requirements. RESEARCH owns the one measurement methodology every measured decision follows (Z2-D-064, Z2-D-084): the profiles, the study pre-registration, native-versus-adapter measurement, the run procedure, noise control on the reference desktop and the S24, the statistics and verdicts, the acceptance-rule operators, the research data formats with their retention, the decision-record template, the CI procedure and the conformance checks. The design is `Zenith2/Docs/Design/RESEARCH.md` and its family. Every requirement has one owner, RESEARCH; a requirement that names another area's ruling (the ECS and shading-path research conclusions) is owned here for its methodology conformance, and the owning area's ruling ticket is its proof.

Classifications: `required` requirements are proved by RESEARCH implementation tickets and their integration tickets; `research` requirements are proved by the harness tickets and `needs-human` ruling tickets of `Zenith2/Docs/Design/RESEARCH/Instances.md`.

## Requirements

### Z2-R-RESEARCH-001 One methodology for every measured decision

- **Statement:** Every ticket of kind decision follows exactly one measurement profile (RA, RS, RJ, RB, RP or RQ) of the RESEARCH methodology and states it in its Research-Profile template line; no measured decision follows a procedure of its own.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-04, Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DecisionTicketValidPerProfile`, `Pwsh_Research_DecisionTicketFailures`, `Pwsh_Research_DecisionTicketNonDecisionPasses` (Test step); the Coverage step decision-template check (hand-off P-RS1)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Measurement profiles"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Template lines"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check"

### Z2-R-RESEARCH-002 Profile rule table

- **Statement:** Each profile applies exactly the rules its column of the profile rule table marks, and the tool refuses a study or record that omits a rule its profile requires.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_StudyInvalidCases`, `Pwsh_Research_RecordInvalidCases` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Profile rule table"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json fields"

### Z2-R-RESEARCH-003 Instance map complete

- **Statement:** Every open decision of the DecisionLog appears in the RESEARCH instance map with its profile, harness, run and ruling tickets, study and operator-form rule, and every map row names an open or ruled decision.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_InstanceMapFixture`, `Pwsh_Research_CheckInstanceMap` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Conformance checks"

### Z2-R-RESEARCH-004 Workers never measure

- **Statement:** No ticket without needs-human has a committed output (its files-edited line, its Outputs: line or a committed Definition of Done item) under Zenith2/Docs/Research/**/runs/ or Zenith2/Docs/Research/Machines/, no harness ticket has one, and every measurement is a needs-human run, ruling or revalidation ticket or a harness smoke run whose timings are never evidence; Budgets evidence committed outside the RESEARCH layout is guarded by its own fingerprint check, not by these paths.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084, Z2-D-126
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DecisionTicketNeedsHuman`, `Pwsh_Research_DecisionTicketHarnessNamesRuns`, `Pwsh_Research_DecisionTicketNamesRunsAsInput` (Test step); the protected paths of hand-off P-RS6 refusing a loop diff (zagent guard)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Profile requirements for the template lines"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC"

### Z2-R-RESEARCH-005 Research separate from qualification

- **Statement:** A research record of profile RA, RS or RJ carries conditions class controlled, and the meets operator on a frame-rate row is a research reading that never replaces a qualification run.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RecordInvalidCases` (an ordinary-class RS record refused), `Pwsh_Research_RuleMeetsFrameRateReadingOnly` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Operator table"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields"

### Z2-R-RESEARCH-006 Study committed before the run

- **Statement:** Every RA, RS, RJ, RB and RP decision has a study.json under Zenith2/Docs/Research/<Study>/ committed by its harness ticket in the same diff as the harness, and every run records the SHA-256 of the study.json it read.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_StudyFilesValid`, `Pwsh_Research_RecordInvalidCases` (study drift) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Study design and pre-registration"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json"

### Z2-R-RESEARCH-007 Study schema

- **Statement:** study.json carries exactly the fields of the study.json fields table and the cell, metric and comparison fields with their patterns, and is refused with RESEARCH_CHECK_SCHEMA otherwise.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_StudyValidSynthetic`, `Pwsh_Research_StudyInvalidCases` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json fields"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Cell fields"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Metric fields"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Comparison fields"

### Z2-R-RESEARCH-008 Hypothesis alternatives and rule

- **Statement:** Every study states one hypothesis sentence of 20 to 400 characters, at least two alternatives with exactly one incumbent for RS and RJ, an acceptance rule in the operator grammar and the outcome when the rule is false.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_StudyInvalidCases`, `Pwsh_Research_RuleGrammarParse` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Hypothesis and acceptance rule"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Rule grammar"

### Z2-R-RESEARCH-009 Families and the primary comparison cap

- **Statement:** Every primary comparison belongs to the family its study familyBy key selects (study, scenario or scenario+metric), and each family holds at most 12 primary comparisons.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_FamilyByScenario`, `Pwsh_Research_StudyInvalidCases` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Multiplicity"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Factors cells and metrics"

### Z2-R-RESEARCH-010 Confound check by factor

- **Statement:** Two cells of a comparison differ only in the inputs members its factor allows, a native or cross-harness pair only when listed in comparesHarnesses with a justification, and any other difference makes the comparison INVALID with reason CONFOUNDED naming the member.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-03
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_VerdictConfounded`, `Pwsh_Research_VerdictApiCrossHarnessAllowed`, `Pwsh_Research_VerdictApiCrossHarnessStillConfounded` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Confound check"

### Z2-R-RESEARCH-011 Incumbent rule

- **Statement:** A study names at most one incumbent; a study with none has a human or blocked false-outcome or an ordered rule ending in a clause that always holds, a plain rule is accepted only in a two-alternative study with exactly one incumbent, and keep names the incumbent only.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_StudyNoIncumbentRequiresHuman`, `Pwsh_Research_StudyNoIncumbentWithKeepRefused` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Hypothesis and acceptance rule"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json fields"

### Z2-R-RESEARCH-012 Fixed repetition count

- **Statement:** Every cell repetition count is fixed in study.json between 10 and 20 inclusive, the runner never adds repetitions after a result is seen, and a new run is recorded as a deviation with the earlier run kept committed.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_StudyInvalidCases` (9 and 21 refused) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Fixed repetition count"

### Z2-R-RESEARCH-013 Not-applicable cells reported

- **Statement:** A cell a platform cannot run is declared notApplicable with a reason of 10 to 200 characters and appears as NOT_APPLICABLE in analysis.json and in the report, never omitted.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ShadingPathNotApplicableS24` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Factors cells and metrics"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Cell fields"

### Z2-R-RESEARCH-014 Reference hardware only

- **Statement:** A timed research cell is measured on the registered reference desktop or the registered S24 Ultra only; WARP, lavapipe, the linux-tsan host and sanitizer builds are never research timing platforms, and the runner refuses an unregistered machine.
- **Covers:** Z2-I-RESEARCH-04, Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerS24ModelRefused` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 session preflight"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"

### Z2-R-RESEARCH-015 ECS native and adapter cells

- **Statement:** The ECS research study measures every scenario of the ECS research scenario list natively on each backend (EnTT in both its view and owning-group variants) and through Zenith_ECS on each backend, declaring notApplicable exactly the cells the ECS harness list excludes.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-03
- **Stage:** S5
- **Classification:** research
- **Decisions:** Z2-D-064, Z2-D-039
- **Budgets:** none
- **Acceptance evidence:** the committed ECS study and run directories of the Stage 5 ECS research ruling, checked by `Pwsh_Research_CheckCommitted`
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "ECS cell matrix"

### Z2-R-RESEARCH-016 Adapter overhead estimate

- **Statement:** For every scenario and backend the analysis reports the shift estimate of the adapter series against the native series with its interval, the relative overhead in ppm and the verdict of the adapter costing more than the margin.
- **Covers:** Z2-I-RESEARCH-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_AdapterShareFixture` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Adapter overhead and share"

### Z2-R-RESEARCH-017 Adapter share bound

- **Statement:** For every EnTT scenario with a native half the analysis reports the adapter share in ppm with the interval-arithmetic bounds clamped to plus or minus one million, against each native variant, keeping negative shares, and reports a share of 0 with reason ZERO_MEDIAN when a divisor is 0.
- **Covers:** Z2-I-RESEARCH-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-130
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_AdapterShareFixture`, `Pwsh_Research_AdapterShareNegativeKept`, `Pwsh_Research_EnttBothVariantsReported`, `Pwsh_Research_AdapterShareZeroMedian` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Adapter overhead and share"

### Z2-R-RESEARCH-018 Architectural comparison

- **Statement:** For every scenario and backend pair the analysis compares the native series with each other and the through-API series with each other under the shift statistics and verdict rules.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ClassificationTable` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Architectural and adapter classification"

### Z2-R-RESEARCH-019 Difference classification

- **Statement:** Every native and through-API verdict pair is classified ARCHITECTURAL, ADAPTER-MASKED, ADAPTER-REVERSED, ADAPTER-INTRODUCED, NONE or UNRESOLVED by the classification table, with the adapter-artefact magnitude and its conservative bounds.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ClassificationTable` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Architectural and adapter classification"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "analysis.json fields"

### Z2-R-RESEARCH-020 Job-scheduling overhead per backend

- **Statement:** Every through-API ECS cell carries a jobs-overhead series from the jobs.overhead_ns counter, reported with its median and interval per scenario and backend.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_EnttBothVariantsReported` (the fixture carries the series) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "ECS cell matrix"

### Z2-R-RESEARCH-021 EnTT adapter cost rows reported

- **Statement:** EnTT through-API cells carry the component-bytes, change-bytes, adapter-waste-bytes and query-refresh secondary series of the ECS adapter cost model, reported as secondary comparisons.
- **Covers:** Z2-I-RESEARCH-03
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_EnttBothVariantsReported` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "ECS cell matrix"

### Z2-R-RESEARCH-022 Shading path cells share inputs

- **Statement:** Every shading-path comparison passes the confound check, so its cells differ only in the shading path, with equal workload definition and content fingerprint members and equal quality keys apart from the path.
- **Covers:** Z2-I-RESEARCH-02
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_VerdictConfounded` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Shading path cell matrix"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Confound check"

### Z2-R-RESEARCH-023 Shading path metrics

- **Statement:** The shading-path study measures main-view GPU frame time as its primary metric against Forward+, and GPU time per pass, frame time, transient peaks with aliasing on and off, persistent target and pool memory, read and write bandwidth and the FLIP mean against Forward as secondary or parity metrics.
- **Covers:** Z2-I-RESEARCH-02
- **Stage:** S5
- **Classification:** research
- **Decisions:** Z2-D-064, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** the committed ShadingPaths study and run directories of Z2T-RPROD-122, checked by `Pwsh_Research_CheckCommitted`
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Shading path cell matrix"

### Z2-R-RESEARCH-024 Shading path report sections

- **Statement:** Every shading-path report carries the section Shading path report sections with the five contents of the core shading-path report table, its per-pass tables generated and its capability exclusions listed.
- **Covers:** Z2-I-RESEARCH-02, Z2-I-RESEARCH-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ReportHeadingsAndBlocks` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings"

### Z2-R-RESEARCH-025 Deterministic metrics compared by value

- **Statement:** Image, count and size series are compared by value, any variation between valid repetitions makes the series NONDETERMINISTIC, and image metrics are judged against their parity tolerance rows only.
- **Covers:** Z2-I-RESEARCH-02
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DeterministicCount` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Deterministic metrics"

### Z2-R-RESEARCH-026 Warm-up slot

- **Statement:** Every cell runs one complete warm-up in schedule slot 0 before repetition 1 on the same boot, recorded with repetition 0, valid 0 and reason WARMUP, and never entering a statistic.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_SeriesHeaderRules`, `Pwsh_Research_RunnerDryRunSchedule` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Warm-up"

### Z2-R-RESEARCH-027 Launch is the unit of replication

- **Statement:** Each launch of a cell contributes exactly one value to each of its series, a harness that measures internal repetitions reduces them to that one value with the metric per-launch statistic, and internal values are never separate samples.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ExtractionSelectors` (the array reduction case) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Replication and interleaving"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Extraction selectors"

### Z2-R-RESEARCH-028 Interleaved deterministic schedule

- **Statement:** The schedule draws a fresh SplitMix64-driven Fisher-Yates shuffle of the cells, swapping P[i] with P[j] for j = z mod (i+1), for the warm-up group and for every repetition, records it before the first launch, and is identical for identical seeds and cell lists.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_SplitMix64Reference`, `Pwsh_Research_ScheduleExpectedPermutations` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Schedule algorithm"

### Z2-R-RESEARCH-029 Invalid slots retried inside the interleaving

- **Statement:** An invalid slot is written with valid 0 and its reason code and retried in the next slot group, after that group's own slots, at most 3 times per repetition, and every order-dependent statistic orders values by repetition rather than slot.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerThermalRetry`, `Pwsh_Research_ScheduleRetryPlacement`, `Pwsh_Research_DriftOrdersByRepetition` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Invalid repetitions and retries"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Invalidity reasons"

### Z2-R-RESEARCH-030 Insufficient cells

- **Statement:** A cell left with fewer than n valid launches after its retries is INSUFFICIENT, every comparison using it is INVALID, and its run is still assembled and committed with the INVALID comparisons shown.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerInsufficient` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Invalid repetitions and retries"

### Z2-R-RESEARCH-031 One session one boot

- **Statement:** A run is one session on one machine, one boot and one platform, and a resume is refused with RESEARCH_ERROR_BOOT_CHANGED when the boot identifier differs.
- **Covers:** Z2-I-RESEARCH-04, Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerResumeBootChanged` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions"

### Z2-R-RESEARCH-032 Session plan within the cap

- **Statement:** Every run plan computes cells times launches plus warm-up times the estimated slot seconds and the cooldown allowance, a plan over 12 hours is refused with RESEARCH_ERROR_SESSION_CAP, and a study too large declares sessions whose comparisons stay inside one session.
- **Covers:** Z2-I-RESEARCH-04, Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerSessionPlanCap`, `Pwsh_Research_StudyInvalidCases` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json fields"

### Z2-R-RESEARCH-033 Exclusive desktop

- **Statement:** A desktop session holds the machine-wide build slot for its whole duration and runs with every lane idle, and the runner refuses to start when the slot is held.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerSlotBusy` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions"; `Zenith2/Docs/Design/RESEARCH.md` → "Locks and exclusivity"

### Z2-R-RESEARCH-034 Integer arithmetic only

- **Statement:** Every sample is a non-negative integer in its unit base, every arithmetic operation on a sample-derived value is a BigInteger operation, ratios are integer ppm, probabilities exact rationals, and the analysis stores, parses and compares no floating-point number.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ArithmeticNoFloat`, `Pwsh_Research_ArithmeticBeyond2Pow53`, `Pwsh_Research_CanonicalJsonRefusesFloat` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Arithmetic rules"

### Z2-R-RESEARCH-035 JSON read as lexemes

- **Statement:** Every JSON input is read with System.Text.Json JsonDocument through its ReadOnlyMemory overload into case-sensitive ordered dictionaries, a harness output with its UTF-8 byte-order mark removed and nesting beyond 64 levels refused, numbers only as their raw lexemes converted by the exact decimal rules, strings never coerced to dates, duplicate member names refused, and ConvertFrom-Json never used.
- **Covers:** Z2-I-RESEARCH-04, Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_JsonReaderRules`, `Pwsh_Research_ModuleStaticRules` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "PowerShell rules of the module"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Extraction selectors"

### Z2-R-RESEARCH-036 Ordinal order and the no-BOM LF writer

- **Statement:** Every ordering this design calls byte order is ordinal, and every committed or compared file is written as UTF-8 without a byte-order mark with LF line endings through WriteAllText, never through Set-Content, Out-File or Sort-Object.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CanonicalSortOrdinal`, `Pwsh_Research_WriterNoBomLf`, `Pwsh_Research_ModuleStaticRules` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "PowerShell rules of the module"

### Z2-R-RESEARCH-037 Per-launch statistic

- **Statement:** Each series value is the per-launch statistic its metric declares (lower median, nearest-rank p95 or p99 in integer form, max, min, total or single) over the launch raw measurements, recomputed from the trace or raw file when present.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_MedianLowerRank`, `Pwsh_Research_PercentileIntegerRank`, `Pwsh_Research_TraceConversion` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Per-launch statistic"

### Z2-R-RESEARCH-038 Median and order-statistic interval

- **Statement:** The median of a series is its value at rank (n+1) div 2 and its 95% interval is the distribution-free order-statistic interval of the binomial quantiles, defined for n from 6 to 20, with the ranks of the methodology table for n from 10 to 20.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_MedianIntervalN10`, `Pwsh_Research_MedianIntervalTable` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Median and its interval"

### Z2-R-RESEARCH-039 Run-to-run variance reported

- **Statement:** Every series reports its robust coefficient of variation in ppm with its minimum and maximum valid values.
- **Covers:** Z2-I-RESEARCH-04, Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RobustCvFixture` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Robust coefficient of variation"

### Z2-R-RESEARCH-040 Noise bands

- **Statement:** A series above its class rCV warning band is NOISY and above its noisy limit NOISY_INVALID, and a NOISY_INVALID series never yields EQUIVALENT but still yields BETTER or WORSE when its interval decides.
- **Covers:** Z2-I-RESEARCH-04, Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RobustCvFixture`, `Pwsh_Research_VerdictNoisy` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Metric classes and thresholds"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Verdicts"

### Z2-R-RESEARCH-041 Drift test

- **Statement:** Every series is tested for drift with execution order by the exact Kendall inversion test, a two-sided p at or below 1/100 marks it DRIFT, and a DRIFT series downgrades every verdict it takes part in to INCONCLUSIVE.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DriftMonotonic`, `Pwsh_Research_DriftMixed`, `Pwsh_Research_DriftBoundaryN10`, `Pwsh_Research_DriftTiesRoundTowardMean`, `Pwsh_Research_VerdictDriftDowngrade` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Drift test"

### Z2-R-RESEARCH-042 Exact rank-sum test with ties

- **Statement:** Every comparison reports the exact two-sided Wilcoxon-Mann-Whitney p-value computed by the tie-conditional dynamic programme over doubled mid-ranks.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RankSumSeparated`, `Pwsh_Research_RankSumTies` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Rank-sum test with ties"

### Z2-R-RESEARCH-043 Shift estimate and Bonferroni interval

- **Statement:** Every comparison reports the Hodges-Lehmann shift as the lower median of the pairwise differences with its exact interval at level 1 minus 0.05 over the size of its family.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ShiftSeparated`, `Pwsh_Research_ShiftTies`, `Pwsh_Research_ShiftKTable`, `Pwsh_Research_FamilyByScenario` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Shift estimate and its interval"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Multiplicity"

### Z2-R-RESEARCH-044 No interval rank available

- **Statement:** A comparison for which no interval rank k of at least 1 exists at its family level is INVALID with reason INSUFFICIENT.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ShiftKTable` (family 13 refused) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Shift estimate and its interval"

### Z2-R-RESEARCH-045 Ties and spread reasons

- **Statement:** A comparison whose exact tie-conditional p and interval disagree on significance carries TIES_DISAGREE, one whose larger rCV exceeds twice the smaller in integer arithmetic (a zero minimum counting as exceeded when the maximum is non-zero) carries SPREAD_DIFFERS, and the report lists both under threats to validity.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_TiesDisagreeReason`, `Pwsh_Research_SpreadDiffersReason`, `Pwsh_Research_SpreadDiffersIntegerRule` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Shift estimate and its interval"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings"

### Z2-R-RESEARCH-046 Effect size and Holm values reported

- **Statement:** Every comparison reports Cliff delta as an integer pair and its Holm-adjusted p-value as a rational, and neither decides a verdict.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CliffDelta`, `Pwsh_Research_HolmAdjustment` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Effect size"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Multiplicity"

### Z2-R-RESEARCH-047 Margins per metric class

- **Statement:** The margin of a comparison is the larger of the class relative margin times the baseline median and the class floor of the metric class table, overridden only by a justified study override.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_MarginClasses`, `Pwsh_Research_RelativeShiftZeroBaseline` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Metric classes and thresholds"

### Z2-R-RESEARCH-048 Verdict rules

- **Statement:** Every comparison verdict is INVALID, INCONCLUSIVE, BETTER, WORSE or EQUIVALENT by the verdict table evaluated in order on the gain interval and the margin, naming UNDERPOWERED when the interval is wider than twice the margin.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_VerdictBetter`, `Pwsh_Research_VerdictEquivalent`, `Pwsh_Research_VerdictUnderpowered`, `Pwsh_Research_VerdictNoisy`, `Pwsh_Research_VerdictHigherIsBetter` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Verdicts"

### Z2-R-RESEARCH-049 Acceptance rule operators

- **Statement:** Acceptance rules use only the operators better, notWorse, equivalent, indistinguishable, medianBelow, medianAbove, medianAtMost, medianAtLeast, disjoint, beyondSpread, meets, misses, lower, equal, pass and judgement with the semantics of the operator table, each evaluated on every platform the study measures.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RuleMeetsUpperAndFloor`, `Pwsh_Research_RuleDisjointAndBeyondSpread`, `Pwsh_Research_RuleMedianBelow`, `Pwsh_Research_RuleEveryPlatform`, `Pwsh_Research_RuleGrammarParse`, `Pwsh_Research_RuleIndistinguishable` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Operator table"

### Z2-R-RESEARCH-050 Rule evaluation

- **Statement:** A study's rule is evaluated once over all the runs its ruling names, each term looked up in the run whose platform and session hold its cells, with three-valued logic, the first true clause of an ordered rule taken, and an INVALID clause before any true clause (a term in none of the runs included) yielding blocked naming the INVALID terms.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RuleThreeValued`, `Pwsh_Research_RuleOrderedBlocked`, `Pwsh_Research_RuleAcrossRuns`, `Pwsh_Research_RuleAcrossSessions` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Rule evaluation"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Statistics functions"

### Z2-R-RESEARCH-051 Instance phrase mapping

- **Statement:** Each acceptance-rule phrase of the open decisions is translated into the operator the phrase mapping table names, and a phrase outside the table carries a ruleNote explaining its translation.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_StudyFilesValid` parses every committed rule, `Pwsh_Research_RuleSoleEligibleAdopted` (Test step); the reviewer of each harness ticket compares the rule with the mapping table and the ruleNote
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Instance phrase mapping"; `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances"

### Z2-R-RESEARCH-052 Byte-stable analysis

- **Statement:** Running the analysis twice on the same committed record and series yields byte-identical canonical analysis.json on any machine.
- **Covers:** Z2-I-RESEARCH-04, Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_AnalysisByteStable`, `Pwsh_Research_CanonicalJsonFixture`, `Pwsh_Research_CanonicalRationalRules`, `Pwsh_Research_CanonicalJsonArrayLine` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Canonical JSON"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Arithmetic rules"

### Z2-R-RESEARCH-053 Desktop power plan locked

- **Statement:** A desktop session runs under the Zenith2 Research power scheme with minimum and maximum processor state 99% on AC, verified by powercfg, with every percent-processor-performance sample of the idle check at most 100.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerPreflightD1` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop session preflight"

### Z2-R-RESEARCH-054 Desktop GPU clocks locked and verified

- **Statement:** A desktop session locks the GPU core and memory clocks to their default application values from an elevated shell, a refused lock fails the preflight with its message, and the reported clocks stay within 15 MHz core and 50 MHz memory of the locks before the session and before every slot.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerPreflightD2`, `Pwsh_Research_RunnerSlotClocksDrift` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop session preflight"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop repetition checks"

### Z2-R-RESEARCH-055 Desktop background load bounded

- **Statement:** A desktop session starts only after a 30-sample idle check with mean CPU at most 3%, every GPU sample at most 2% and no foreign process above 1% CPU, and every slot starts after a 5-second sample with mean CPU at most 3%.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerPreflightD3`, `Pwsh_Research_RunnerSlotBackground` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop session preflight"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop repetition checks"

### Z2-R-RESEARCH-056 Desktop thermal state bounded

- **Statement:** Every desktop slot starts with the GPU at or below 50 degrees Celsius after waiting up to 300 s, the session ambient is 20 to 26 degrees Celsius, and a slot that cannot reach the limit is invalid with THERMAL.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerPreflightD4`, `Pwsh_Research_RunnerPreflightD7`, `Pwsh_Research_RunnerThermalRetry` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop repetition checks"

### Z2-R-RESEARCH-057 Desktop clock events during a slot

- **Statement:** A background sampler records the GPU clock-event reasons once per second during every desktop slot, and any active power-cap, slowdown or power-brake reason makes the slot invalid with CLOCKS.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerSlotClockEvent` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop repetition checks"

### Z2-R-RESEARCH-058 Desktop slot exit and reset

- **Statement:** A desktop slot whose launch exits non-zero or times out is invalid with EXIT, and one after which the GPU reports a required reset is invalid with DEVICE.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerSlotDeviceReset` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop repetition checks"

### Z2-R-RESEARCH-059 Desktop display and update state

- **Statement:** A desktop session starts only with the operator attestations DisplayChecked and UpdatesPaused, a read display resolution of 2560x1440 and no pending reboot, each recorded in the run record.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerPreflightD5`, `Pwsh_Research_RunnerPreflightD6` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop session preflight"

### Z2-R-RESEARCH-060 Desktop restoration

- **Statement:** After the last slot and on every runner exit the GPU clock locks are reset and the previous power scheme restored, and a runner that finds unrestored state refuses with RESEARCH_ERROR_UNRESTORED until a restore-only run.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerUnrestored`, `Pwsh_Research_RunnerPreflightD3` (the restoration after a refused preflight) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop restoration"

### Z2-R-RESEARCH-061 Pre-session reboot and idle

- **Statement:** Every desktop session starts between 300 s and 3 hours after its boot, and every S24 session follows a reboot and 120 s of idle after boot completion, recorded through the boot identifier.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerPreflightD9` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions"

### Z2-R-RESEARCH-062 Desktop calibration

- **Statement:** The Stage 1 research conformance ticket runs the desktop calibration once, committing the idle samples and the cooldown curve, and proposes changed procedure parameters when the idle CPU mean exceeds 2%, the GPU never cools below 48 degrees Celsius or the cooldown to 50 degrees Celsius takes more than 240 s.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerCalibrate` (Test step); the calibration file committed by the needs-human Stage 1 integration ticket
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop calibration"

### Z2-R-RESEARCH-063 S24 fixed-performance mode

- **Statement:** An S24 session enables fixed-performance mode and verifies it by its exit status and a dumpsys power read-back, and when either fails the run stays controlled with fixedPerformance unavailable under the stricter thermal limits.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerFixedPerfUnavailable` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 session preflight"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Fixed-performance mode unavailable"

### Z2-R-RESEARCH-064 S24 thermal gating

- **Statement:** Every S24 slot starts at thermal status 0 with battery temperature at or below 33.0 degrees Celsius after waiting up to 600 s, and a slot that ends at thermal status 2 or above is invalid with THERMAL.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerS24SlotThermal`, `Pwsh_Research_RunnerS24EndThermal`, `Pwsh_Research_RunnerS24ThermalStart`, `Pwsh_Research_RunnerFixedPerfUnavailable` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 repetition checks"

### Z2-R-RESEARCH-065 S24 device state controlled

- **Statement:** An S24 session runs with airplane mode and do-not-disturb on, brightness 128 of 255 read back with adaptive brightness off, the screen held on, background apps killed and the second of two cpuinfo reports 10 s apart below 5% total, battery level 50 to 100 at the start and at least 40 at every slot.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerS24Radios`, `Pwsh_Research_RunnerS24Screen`, `Pwsh_Research_RunnerS24Background`, `Pwsh_Research_RunnerS24BatteryStart`, `Pwsh_Research_RunnerS24BatteryLow`, `Pwsh_Research_RunnerS24Ambient`, `Pwsh_Research_RunnerAdbCrlf` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 session preflight"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 repetition checks"

### Z2-R-RESEARCH-066 S24 reference device only

- **Statement:** S24 cells run only on a device whose product model starts SM-S928, and every S24 slot records its start and end thermal status, battery temperature, level and charging state and is invalid with EXIT when its pulled output is shorter than the device size.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-010
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerS24ModelRefused`, `Pwsh_Research_RunnerS24PullSize` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "S24 session preflight"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields"

### Z2-R-RESEARCH-067 Machine identity

- **Statement:** Every run records the machineId, the first 12 hex digits of the SHA-256 of the machine identity lines, of a machine registered under Zenith2/Docs/Research/Machines/ for the platform, and writes no raw serial, UUID, user name or host name.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CanonicalSortOrdinal` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Machines registry"

### Z2-R-RESEARCH-068 Software state recorded

- **Statement:** Every run records the GPU driver, OS build, BIOS or Android fingerprint, SDK, kernel and the Vulkan and D3D12 driver versions of its machine.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RecordValidSynthetic` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Machines registry"

### Z2-R-RESEARCH-069 Current machine state

- **Statement:** Machines/current.json holds each reference machine and its software state as the last mid-sprint integration check recorded it, written by Update-Zenith2ResearchMachineState.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RevalidationDependencyFires` (the current.json fixture pair) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Machines registry"

### Z2-R-RESEARCH-070 Build identity recorded

- **Statement:** Every run records the clean tree SHA, the Release preset and configuration, the manifest toolchain and dependency hashes, the versions of the study dependencies and each cell inputs including its launch target.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RecordValidSynthetic` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"

### Z2-R-RESEARCH-071 Run directory layout

- **Statement:** Every run is the directory Zenith2/Docs/Research/<Study>/runs/<RunId>/ holding exactly record.json, analysis.json and the series, raw and archive directories, any other file inside it being reported as RESEARCH_CHECK_STRAY_FILE.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RecordInvalidCases` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Run directory"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Research tree layout"

### Z2-R-RESEARCH-072 Record schema

- **Statement:** record.json carries the fields of the record.json fields table under schema zenith2.research-record version 1, with every committed file of the run listed with its SHA-256 and size.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RecordValidSynthetic`, `Pwsh_Research_RecordInvalidCases`, `Pwsh_Research_RecordEvidenceInvalid` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields"

### Z2-R-RESEARCH-073 Series format

- **Statement:** Each series file is the zenith2.research-series v1 CSV with its header lines and one row per executed slot, warm-up and invalid rows included.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_SeriesHeaderRules` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Series files"

### Z2-R-RESEARCH-074 Trace format

- **Statement:** Each frame-workload repetition trace is the zenith2.research-trace v1 CSV of its measured-interval frames in integer nanoseconds, converted exactly from the Budgets per-frame CSV.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_TraceConversion` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Trace files"

### Z2-R-RESEARCH-075 Raw outputs with normalised line endings

- **Statement:** Every text output a selector read is committed under raw/ within the per-file cap, byte-identical to the harness output except that CRLF becomes LF and a final newline is added, with sourceSha256 of the original recorded when the bytes changed.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RawCrlfNormalised` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Raw files"

### Z2-R-RESEARCH-076 Extraction selectors

- **Statement:** Series values are extracted from harness outputs only through the harness-json, harness-json-array, evidence-row, frames-csv and probe-case selectors, a non-unique, non-integer or exponent lexeme being EXTRACT.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ExtractionSelectors` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Extraction selectors"

### Z2-R-RESEARCH-077 Text rule

- **Statement:** Every committed non-LFS file of a study directory, of Index.md and of Machines/** is UTF-8 text without NUL bytes or a byte-order mark, with LF line endings, a final newline and the extension .json, .csv, .md or .txt, and files elsewhere under Zenith2/Docs/Research/ are outside the RESEARCH check.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RecordInvalidCases`, `Pwsh_Research_TextRuleScope` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Size caps and the text rule"

### Z2-R-RESEARCH-078 Committed size caps

- **Statement:** No committed non-LFS research file exceeds the research committed file size cap row and no run directory non-LFS total exceeds the research committed run directory size cap row, measured by MP-RESEARCHSTORE, with the provisional values 4 MiB and 32 MiB used until the rows are adopted.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-012
- **Budgets:** the pending row RESEARCH-001 "Research committed file size cap" (producer: Stage 1 per-system budget ticket for RESEARCH); the pending row RESEARCH-002 "Research committed run directory size cap" (producer: Stage 1 per-system budget ticket for RESEARCH)
- **Acceptance evidence:** `Pwsh_Research_RecordInvalidCases`, `Pwsh_Research_StorageMeasurement` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Size caps and the text rule"

### Z2-R-RESEARCH-079 Placement by kind

- **Statement:** The runner places record.json, analysis.json, the series and text outputs within the per-file cap outside archive/, and every trace, Budgets per-frame CSV, binary capture and over-cap text under archive/, refusing a run whose non-LFS total passes the run cap.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerAssemblePlacement` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Run directory"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Size caps and the text rule"

### Z2-R-RESEARCH-080 Large-file storage reported

- **Statement:** The sum of the large-file tier of each study is measured by MP-RESEARCHSTORE and reported against the research large-file storage per study row, provisional 2 GiB.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-012
- **Budgets:** the pending row RESEARCH-003 "Research large-file storage per study" (producer: Stage 1 per-system budget ticket for RESEARCH)
- **Acceptance evidence:** `Pwsh_Research_StorageMeasurement` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Size caps and the text rule"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Volume per study"

### Z2-R-RESEARCH-081 Large-file tier in git LFS

- **Statement:** Traces, Budgets per-frame CSVs, binary captures and over-cap text are committed as git LFS objects under the run archive/ directory and pinned in record.json by their LFS oid and size, so no research data lives outside the repository.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_LargeFilePointerAndObject`, `Pwsh_Research_RunnerAssemblePlacement` (Test step); the gitattributes block and the committed .lfsconfig of hand-off P-RS4
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file tier"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file layout"

### Z2-R-RESEARCH-082 Large-file verification without the objects

- **Statement:** The check verifies every archive/ file against its record entry from the LFS pointer oid and size, or from the bytes when the object is fetched, so it runs in CI and in lanes without git lfs or the objects, none of which hold the objects.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_LargeFilePointerAndObject` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file verification"

### Z2-R-RESEARCH-083 Retention for the life of the repository

- **Statement:** Every committed research file, LFS objects included, is kept for the life of the repository and removed only by a DecisionLog entry naming the paths and the reason.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** review only: a removal is a needs-human diff judged against the DecisionLog entry it must carry; no check enforces the rule (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "Retention")
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Retention"

### Z2-R-RESEARCH-084 Reproducibility from the committed tier

- **Statement:** Every number of every research report is recomputable from the committed record and series alone, without fetching any LFS object.
- **Covers:** Z2-I-RESEARCH-06, Z2-I-RESEARCH-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CheckCommitted` over the Stage 5 research runs (Test step and the Stage 5 acceptance)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file tier"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Generated blocks"

### Z2-R-RESEARCH-085 Format versioning

- **Statement:** Every research format carries its schema and version, a reader refuses an unknown version with RESEARCH_ERROR_SCHEMA, committed runs are never migrated, and every analysis version a committed file names stays implemented.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_StudyInvalidCases` (an unknown version) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Format versioning"

### Z2-R-RESEARCH-086 Raw data retention for the Stage 5 research

- **Statement:** The ECS and shading-path research runs of Stage 5 are committed under the canonical layout with their large-file tier, and the large-file verification passes for them at the Stage 5 acceptance.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S5
- **Classification:** research
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** the Stage 5 research ruling tickets (Z2T-RPROD-122 and the ECS research ruling) and `Pwsh_Research_CheckCommitted` in the Stage 5 acceptance
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file verification"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Run directory"

### Z2-R-RESEARCH-087 Decision ticket template lines

- **Statement:** Every decision ticket, every budget or revalidation ticket that runs a measurement and every run-role ticket carries the template lines Research-Profile, Decision-Name, Study, Hypothesis, Alternatives, Primary metrics, Acceptance rule, Platforms, Conditions, Repetitions, Outputs, Revalidation triggers and Deviations, in that order, as the first block of its Implementation Contract.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-068
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DecisionTicketValidPerProfile`, `Pwsh_Research_DecisionTicketFailures` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Template lines"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check"

### Z2-R-RESEARCH-088 Study producer blocks the ruling

- **Statement:** Every RA, RS, RJ, RB and RP decision ticket is blocked by the harness ticket that creates the study.json its Study line names, and a run-role ticket blocks its ruling ticket.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DecisionTicketFailures` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Profile requirements for the template lines"

### Z2-R-RESEARCH-089 Decision ticket Definition of Done

- **Statement:** Every ruling ticket Definition of Done names the passing Pwsh_Research_CheckCommitted check over the committed runs, report and DecisionLog entry, and a failure scenario in which a false or blocked rule records its outcome and changes no default.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DecisionTicketFailures` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Definition of Done lines"

### Z2-R-RESEARCH-090 Report template

- **Statement:** Every research ruling commits Zenith2/Docs/Research/<Study>/Reports/<Z2-D-NNN>.md with exactly the report headings in order, its profile-specific sections, and no remaining write marker.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ReportHeadingsAndBlocks`, `Pwsh_Research_ReportFromTemplate` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report template"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings"

### Z2-R-RESEARCH-091 Generated report blocks

- **Statement:** Every data, analysis, classification, adapter, per-pass and probe table of a report is a generated block byte-equal to its regeneration from analysis.json.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ReportHeadingsAndBlocks` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Generated blocks"

### Z2-R-RESEARCH-092 DecisionLog research fields

- **Statement:** Every DecisionLog entry ruling an RA, RS, RJ, RB or RP decision carries the Research, Analysis and Revalidation fields after Related, with its Analysis outcome and verdicts equal to the analysis of its runs, and an RQ entry carries Revalidation.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DecisionLogFields`, `Pwsh_Research_CheckDecisionLog` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Conformance checks"

### Z2-R-RESEARCH-093 No silent default change

- **Statement:** The Analysis field of a research DecisionLog entry ends with default change none or the follow-up ticket that changes the default, and no ruling diff changes a default.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-035
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DecisionLogFields` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields"

### Z2-R-RESEARCH-094 Justification record evidence line

- **Statement:** Every RJ ruling completes its provisional justification record Measured evidence line in the run, platform, median, interval and verdict form, and adds the research fields to the same entry.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-041, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DecisionLogFields` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Justification record evidence line"

### Z2-R-RESEARCH-095 Probe result table

- **Statement:** Every RP report replaces the Data and Analysis sections with a generated case-by-device pass or fail table.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_ReportHeadingsAndBlocks` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings"

### Z2-R-RESEARCH-096 Deviations declared

- **Statement:** Every departure of a run from its study.json or from the methodology is listed with a justification in the run record deviations and shown in the report Method section.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RecordInvalidCases` (study drift without a deviation) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json fields"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Study design and pre-registration"

### Z2-R-RESEARCH-097 Budget allocation runs in research format

- **Statement:** Every Stage 1 per-system budget ruling with a harness half commits its harness runs under the research layout beside its allocation.json, the Budgets formats unchanged.
- **Covers:** Z2-I-RESEARCH-06, Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CheckCommitted` over Zenith2/Docs/Research/Budgets/ (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Research rulings without an open decision name"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Study directory"

### Z2-R-RESEARCH-098 Committed research check in the Test step

- **Statement:** The CTest Pwsh_Research_CheckCommitted validates every committed study, run, analysis, report, revalidation log, Index.md, the text rule, the path lengths and the caps, and fails the Test step on any finding.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CheckCommitted`, `Pwsh_Research_CheckCommittedFixtures`, `Pwsh_Research_AnalysisStaleDetected`, `Pwsh_Research_PathLengthCap` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Conformance checks"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Check finding codes"

### Z2-R-RESEARCH-099 Decision ticket check on the board

- **Statement:** The Coverage step runs the decision ticket check over every board ticket, selecting decision, budget, revalidation and run-role tickets by their managed Kind trailer or Research-Role line, and fails on any finding.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-067
- **Budgets:** none
- **Acceptance evidence:** the PROC needs-human Coverage hand-off ticket for P-RS1, over the fixtures of `Pwsh_Research_DecisionTicketValidPerProfile`
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check"

### Z2-R-RESEARCH-100 Research index

- **Statement:** Zenith2/Docs/Research/Index.md lists every study once with its profile, decision and latest report.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CheckCommitted` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Research tree layout"

### Z2-R-RESEARCH-101 Gate membership of research checks

- **Statement:** The Test step runs every harness smoke run and every Pwsh_Research case on the win preset, and no gate compares a benchmark timing with a threshold.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-031
- **Budgets:** none
- **Acceptance evidence:** the RESEARCH block of the test registry; `Pwsh_Research_CheckCommitted` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Registration in the test registry"

### Z2-R-RESEARCH-102 Revalidation trigger vocabulary

- **Statement:** Every research DecisionLog entry lists revalidation triggers from the vocabulary toolchain, hardware, dependency, driver, os, workload, row, target, stage and margin, with toolchain and hardware implied for RA, RS, RJ, RB and RP and at least one more for RA, RS and RJ.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RevalidationImpliedTokens`, `Pwsh_Research_DecisionLogFields` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Trigger vocabulary"

### Z2-R-RESEARCH-103 Revalidation scan

- **Statement:** The revalidation scan evaluates every trigger of every settled research entry against the checkout without building, marks a trigger handled by a matching revalidations.json entry, and with -Final exits 1 while a fired trigger is unhandled.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RevalidationDependencyFires`, `Pwsh_Research_RevalidationFinalExit`, `Pwsh_Research_RevalidationMarginStage` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Revalidation scan"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Trigger evaluation"

### Z2-R-RESEARCH-104 Revalidation log

- **Statement:** Every handled trigger is one appended entry of Zenith2/Docs/Research/<Study>/revalidations.json with its decision, trigger, recorded and observed values, run and outcome.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RevalidationsSchema` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "revalidations.json"

### Z2-R-RESEARCH-105 Research regression rule

- **Statement:** A revalidation run is compared with the original run, giving stands when verdicts are unchanged and shifts within the bound, stands-drifted when a shift moved beyond it, and regressed when any primary verdict or the rule outcome changed.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CompareRunsOutcomes` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Research regression rule"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Run comparison"

### Z2-R-RESEARCH-106 Regression filing

- **Statement:** A regressed outcome produces a ten-section needs-human re-ruling ticket body listing the dependent tickets and the exact zagent create command, and the tool never writes to the board.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-07
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-102
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RegressionTicketBody` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Regression filing"

### Z2-R-RESEARCH-107 Mid-sprint machine state recorded

- **Statement:** Every stage sprint mid-sprint integration check, Stage 1 included, records the reference machines software state in Machines/current.json for both platforms.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** the mid-sprint integration check tickets carrying hand-off P-RS5 (needs-human), with their revalidations.json entries committed
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC"

### Z2-R-RESEARCH-108 Stage acceptance revalidation scan

- **Statement:** Every stage acceptance runs the revalidation scan with -Final, and the acceptance does not complete while a fired trigger is unhandled.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-083, Z2-D-065
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RevalidationFinalExit` (Test step); the stage acceptance tickets (Zenith2Acceptance, needs-human)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"

### Z2-R-RESEARCH-109 Research regressions separate from budget regressions

- **Statement:** A budget-row breach is filed under the Budgets breach consequences and never through research regression filing, and a research regression never files a budget BUG.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-012
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RegressionTicketBody` (asserting the printed command is a needs-human TASK and never a BUG, and that a budget-row regression produces no command) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Regression thresholds"

### Z2-R-RESEARCH-110 PROC hand-offs filed

- **Statement:** The five RESEARCH hand-offs (the board decision check, research acceptance coverage, the research gitattributes block with the committed .lfsconfig fetch exclusion and filter.lfs.required, the mid-sprint research steps and the protected research paths) exist as PROC needs-human tickets.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-07
- **Stage:** S0
- **Classification:** required
- **Decisions:** Z2-D-128
- **Budgets:** none
- **Acceptance evidence:** the PROC hand-off tickets on the board
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC"

### Z2-R-RESEARCH-111 Runner command shim

- **Statement:** Every external command, wait, operator prompt and time read of the runner goes through the command shim functions, which expand the work, runId, slot and out placeholders in command keys, return scripted per-call responses that may write fixture files, replace the DEVT tree and fingerprint functions with stubs, and advance a simulated clock when ZENITH_RESEARCH_COMMAND_SHIM names a shim file, so every runner step is tested without hardware and within the CTest timeout.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerSlotLifecycle`, `Pwsh_Research_RunnerThermalRetry`, `Pwsh_Research_RunnerS24SlotThermal`, `Pwsh_Research_RunnerS24BatteryLow`, `Pwsh_Research_RunnerDryRunSchedule` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Command shim"

### Z2-R-RESEARCH-112 Runner completion exit codes

- **Statement:** The runner exits 0 when every cell has its n valid launches and 1 when a cell is INSUFFICIENT, assembling the run directory in both cases.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerSlotLifecycle`, `Pwsh_Research_RunnerInsufficient` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH.md` → "Runner exit codes"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"

### Z2-R-RESEARCH-113 Runner never builds

- **Statement:** The runner measures only executables that already exist in the Release output of a clean measured tree, refusing a dirty tree or a missing executable, and never invokes a build.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerDirtyTreeRefused`, `Pwsh_Research_RunnerNotBuiltRefused` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"

### Z2-R-RESEARCH-114 Dry run changes nothing

- **Statement:** A dry run prints the schedule, launches nothing and changes no machine setting.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerDryRunSchedule` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"

### Z2-R-RESEARCH-115 Stage 1 research conformance

- **Statement:** At the Stage 1 acceptance every Stage 1 research ruling (D-SLANG, D-SEL-HEAP, D-SEL-HASH, D-SEL-JSON, D-SEL-PHYSX-ANDROID, D-JOLT-PARALLEL-STEP, D-LUAU-STATELESS, D-SCRIPT-TXN and the per-system budget rulings with a harness half) has committed runs, a report and DecisionLog research fields passing the research checks.
- **Covers:** Z2-I-RESEARCH-04, Z2-I-RESEARCH-05, Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CheckCommitted` and `Pwsh_Research_CheckDecisionLog` in the Stage 1 acceptance run (the RESEARCH Stage 1 integration ticket)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"

### Z2-R-RESEARCH-116 Stage 5 research conformance

- **Statement:** At the Stage 5 acceptance the ECS and shading-path research reports exist under the canonical layout with the RA report sections, and their DecisionLog entries pass the research checks.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-02, Z2-I-RESEARCH-03, Z2-I-RESEARCH-06, Z2-I-RESEARCH-07
- **Stage:** S5
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CheckCommitted` and `Pwsh_Research_CheckDecisionLog` in the Stage 5 acceptance run (the RESEARCH Stage 5 integration ticket)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Research rulings without an open decision name"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings"

### Z2-R-RESEARCH-117 End-to-end synthetic study

- **Statement:** A committed synthetic study under the test fixtures passes through schedule, extraction, analysis, report and every check with the hand-derived expected outputs.
- **Covers:** Z2-I-RESEARCH-04, Z2-I-RESEARCH-06, Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_AnalysisByteStable`, `Pwsh_Research_ReportFromTemplate`, `Pwsh_Research_CheckCommittedFixtures` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tests.md` → "Fixture sets"

### Z2-R-RESEARCH-118 Shading path conclusion as decision

- **Statement:** The shading-path research conclusion is a DecisionLog entry with the research fields, and Forward+ stays the default unless that entry names a follow-up ticket changing it.
- **Covers:** Z2-I-RESEARCH-02, Z2-I-RESEARCH-07
- **Stage:** S5
- **Classification:** research
- **Decisions:** Z2-D-064, Z2-D-035, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** the shading-path ruling ticket Z2T-RPROD-122 (needs-human), checked by `Pwsh_Research_CheckDecisionLog`
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields"; `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Research rulings without an open decision name"

### Z2-R-RESEARCH-119 ECS conclusion as decision

- **Statement:** The ECS research conclusion is a DecisionLog entry with the research fields recording the classification and the adapter share, and flagged proposal P1 is ruled only if the human approves it.
- **Covers:** Z2-I-RESEARCH-01, Z2-I-RESEARCH-03, Z2-I-RESEARCH-07
- **Stage:** S5
- **Classification:** research
- **Decisions:** Z2-D-064, Z2-D-130, Z2-D-022
- **Budgets:** none
- **Acceptance evidence:** the ECS research ruling ticket (Z2T-ECS-102 when P1 is approved, needs-human), checked by `Pwsh_Research_CheckDecisionLog`
- **Design:** `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Architectural and adapter classification"

### Z2-R-RESEARCH-120 Machine registry change by commit only

- **Statement:** A reference machine is registered only by a committed Machines/<machineId>.json naming its platforms, and the runner never registers a machine.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerNotReference` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Machines registry"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"

### Z2-R-RESEARCH-121 Revalidation in the shipping stages

- **Statement:** The revalidation scan runs in the Stage 6 and Stage 7 acceptances as in every other stage, so research conclusions are re-read against the shipping toolchain and hardware.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-07
- **Stage:** S6
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** the Stage 6 acceptance run (Zenith2Acceptance, needs-human)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"; `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Trigger evaluation"

### Z2-R-RESEARCH-122 Study-level outcome in the report

- **Statement:** The study-level outcome of a ruling is computed over its named runs, stored in the report generated analysis block and quoted by the DecisionLog Analysis field, while each run analysis.json carries a null outcome with reason PARTIAL whenever its rule needs another run.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-097
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RuleAcrossRuns`, `Pwsh_Research_ReportFromTemplate`, `Pwsh_Research_DecisionLogFields` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Analysis and report functions"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Generated blocks"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields"

### Z2-R-RESEARCH-123 Rule definitions

- **Statement:** A study rule may reference a named expression of the study definitions object with @name, each definition using only earlier definitions, and an unknown, self or later reference is refused with RESEARCH_ERROR_RULE.
- **Covers:** Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RuleDefinitions`, `Pwsh_Research_RuleGrammarParse` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Rule grammar"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json fields"

### Z2-R-RESEARCH-124 Invalid rate flagged

- **Statement:** A comparison carries INVALID_RATE, listed under threats to validity with its verdict unchanged, when a cell loses more than 20% of its executed slots or the two cells invalid percentages differ by more than 10 points.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_InvalidRateReason` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Invalid repetitions and retries"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report headings"

### Z2-R-RESEARCH-125 Archive blobs are LFS pointers

- **Statement:** Every path under a run archive/ directory carries the lfs filter attribute and is stored in git as an LFS pointer whose oid and size equal its record entry, checked with git alone against the index before the commit and against the committed tree in the Test step, any other blob failing RESEARCH_CHECK_NOT_LFS.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_LargeFileNotPointer` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file verification"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Large-file functions"

### Z2-R-RESEARCH-126 Runner refuses without git-lfs

- **Statement:** The runner refuses to start with RESEARCH_ERROR_NOT_LFS and exit 2 unless git-lfs is installed, its clean filter is configured, filter.lfs.required is true and the run archive/ path carries the lfs filter attribute.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerLfsMissingRefused` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"; `Zenith2/Docs/Design/RESEARCH.md` → "Runner error codes"

### Z2-R-RESEARCH-127 Research objects excluded from every fetch

- **Statement:** A committed repository-root .lfsconfig excludes Zenith2/Docs/Research from every LFS fetch of every clone and worktree, the research check refusing a committed run while it or the research gitattributes block is missing, and a human fetches a study objects with an explicit include.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_LfsConfigPresent` (Test step); the `.lfsconfig` of hand-off P-RS4
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file layout"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Hand-offs to PROC"

### Z2-R-RESEARCH-128 Research path lengths

- **Statement:** A study path is at most 64 characters and every file under a run directory has a repo-relative path of at most 200 characters, a longer one failing RESEARCH_CHECK_PATH_LENGTH.
- **Covers:** Z2-I-RESEARCH-06
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_PathLengthCap` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Study directory"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Check finding codes"

### Z2-R-RESEARCH-129 PowerShell rules proved by a probe

- **Statement:** Every PowerShell rule of the module has a runnable proof in the committed Probe-PwshRules.ps1, which the Test step runs so that a pwsh update changing one of those behaviours fails a test.
- **Covers:** Z2-I-RESEARCH-04
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_PwshRulesProbe`, `Pwsh_Research_ModuleStaticRules` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "PowerShell rules of the module"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Files"

### Z2-R-RESEARCH-130 Decision ticket check reads committed outputs

- **Statement:** The decision ticket check judges a research path only among a ticket committed outputs, its files-edited line, its Outputs: line and its committed Definition of Done items, so a ticket that only reads a run passes.
- **Covers:** Z2-I-RESEARCH-05, Z2-I-RESEARCH-07
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-084
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_DecisionTicketNamesRunsAsInput`, `Pwsh_Research_DecisionTicketNeedsHuman` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check"; `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Profile requirements for the template lines"

### Z2-R-RESEARCH-131 Each session committed before the next

- **Statement:** The human commits each run directory, after its staged large-file check, before starting the next session of the same study, so every session measures a clean tree whose SHA its record holds.
- **Covers:** Z2-I-RESEARCH-04, Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerDirtyTreeRefused`, `Pwsh_Research_RunnerSlotLifecycle` (the printed git add and staged-check lines) (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"

### Z2-R-RESEARCH-132 Mid-sprint revalidation scan

- **Statement:** Every stage sprint mid-sprint integration check, Stage 1 included, runs the revalidation scan over the settled research entries.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** the mid-sprint integration check tickets carrying hand-off P-RS5 (needs-human), with the scan output in their records; `Pwsh_Research_RevalidationDependencyFires` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Revalidation scan"

### Z2-R-RESEARCH-133 Mid-sprint revalidation runs

- **Statement:** Every mid-sprint integration check runs a revalidation measurement for each fired trigger whose decision governs a ticket of the next stage and commits its revalidations.json entry.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** the mid-sprint integration check tickets carrying hand-off P-RS5 (needs-human), with their revalidations.json entries committed; `Pwsh_Research_CompareRunsOutcomes` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Run comparison"

### Z2-R-RESEARCH-134 Stage acceptance current-inputs check

- **Statement:** Every stage acceptance runs the committed research check with -CurrentInputs, reporting changed cell inputs as information that feeds the target and dependency triggers and a changed member with no changed input as a finding.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064, Z2-D-083
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_CheckCurrentInputsInfo` (Test step); the stage acceptance tickets (Zenith2Acceptance, needs-human)
- **Design:** `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Conformance checks"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"

### Z2-R-RESEARCH-135 Runner refusal exit codes

- **Statement:** The runner exits 2 on an input error, a missing or unwired git-lfs, a session plan over the cap or a run over the run-directory cap, and 3 on a refused preflight, an unregistered machine or a held build slot, mapping the build slot exit 11 to 3 and passing every other wrapped exit through.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerSessionPlanCap`, `Pwsh_Research_RunnerLfsMissingRefused`, `Pwsh_Research_RunnerNotReference`, `Pwsh_Research_RunnerSlotBusy` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH.md` → "Runner exit codes"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"

### Z2-R-RESEARCH-136 Runner abort and unrestored exit codes

- **Statement:** The runner exits 4 on an aborted session with nothing written under Zenith2/Docs and the work directory kept, and 5 on unrestored machine state until -RestoreOnly restores it.
- **Covers:** Z2-I-RESEARCH-05
- **Stage:** S1
- **Classification:** required
- **Decisions:** Z2-D-064
- **Budgets:** none
- **Acceptance evidence:** `Pwsh_Research_RunnerAbortWritesNothing`, `Pwsh_Research_RunnerOperatorAbort`, `Pwsh_Research_RunnerUnrestored` (Test step)
- **Design:** `Zenith2/Docs/Design/RESEARCH.md` → "Runner exit codes"; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Research runner"

## Coverage of inventory items

| Item | Inventory text | Stage | Requirements |
|---|---|---|---|
| Z2-I-RESEARCH-01 | the research methodology for ECS backends | S1 | Z2-R-RESEARCH-001, -002, -003, -005, -006, -007, -008, -009, -010, -013, -015, -018, -019, -020, -048, -049, -116, -119 |
| Z2-I-RESEARCH-02 | the research methodology for shading paths | S1 | Z2-R-RESEARCH-001, -002, -003, -005, -006, -007, -008, -009, -010, -013, -022, -023, -024, -025, -048, -049, -116, -118 |
| Z2-I-RESEARCH-03 | native vs adapter measurement | S5 | Z2-R-RESEARCH-010, -015, -016, -017, -018, -019, -020, -021, -116, -119 |
| Z2-I-RESEARCH-04 | repetition procedure | S1 | Z2-R-RESEARCH-001, -002, -006, -007, -012, -014, -026, -027, -028, -029, -030, -031, -032, -034, -035, -037, -038, -039, -040, -041, -042, -043, -044, -045, -046, -047, -048, -052, -115, -117, -124, -129, -131 |
| Z2-I-RESEARCH-05 | CI procedure for research runs | S1 | Z2-R-RESEARCH-004, -005, -014, -031, -032, -033, -039, -040, -053, -054, -055, -056, -057, -058, -059, -060, -061, -062, -063, -064, -065, -066, -068, -069, -070, -098, -101, -102, -103, -104, -105, -106, -107, -108, -109, -110, -111, -112, -113, -114, -115, -120, -121, -126, -130, -131, -132, -133, -134, -135, -136 |
| Z2-I-RESEARCH-06 | raw-data retention | S5 | Z2-R-RESEARCH-035, -036, -052, -067, -068, -069, -070, -071, -072, -073, -074, -075, -076, -077, -078, -079, -080, -081, -082, -083, -084, -085, -086, -097, -098, -100, -104, -116, -117, -120, -125, -126, -127, -128 |
| Z2-I-RESEARCH-07 | conclusions as decisions | S5 | Z2-R-RESEARCH-001, -003, -004, -008, -011, -024, -049, -050, -051, -084, -087, -088, -089, -090, -091, -092, -093, -094, -095, -096, -097, -099, -102, -105, -106, -110, -115, -116, -117, -118, -119, -121, -122, -123, -130 |

Items 03, 06 and 07 are staged S5 in the inventory, the stage of the ECS and shading-path research reports. Their formats, templates and checks are required from Stage 1, because the Stage 1 rulings already commit raw data and decision records; those requirements carry stage S1, and the S5 requirements carry the research reports themselves.

## Excluded and deferred

- Nothing of the RESEARCH scope is excluded or deferred.
- Not RESEARCH's, and therefore not catalogued here: every measurement harness (its measured area's catalogue), every selection ruling (its owning area's), the budget-run procedure and budget breach consequences (Budgets), the evidence fingerprint and the workload runner (DEVT), and every gate-script edit (PROC, through the hand-offs P-RS1, P-RS2, P-RS4, P-RS5 and P-RS6 that Z2-R-RESEARCH-110 tracks).
- RESEARCH has no Luau binding and reserves no misuse-code block.

