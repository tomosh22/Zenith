# RESEARCH design: the measurement methodology

This file is part of the RESEARCH design family; the entry document is `Zenith2/Docs/Design/RESEARCH.md`. It specifies the one measurement methodology every measured decision in Zenith2 follows: the measurement profiles, the study pre-registration, native-versus-adapter measurement, the run procedure, noise control on the reference desktop and the S24, the statistics with every threshold as a number, the acceptance-rule operators, and the revalidation triggers. File formats are in `Zenith2/Docs/Design/RESEARCH/Formats.md`, the PowerShell tooling in `Zenith2/Docs/Design/RESEARCH/Tooling.md`, and the mapping of every existing decision ticket onto this methodology in `Zenith2/Docs/Design/RESEARCH/Instances.md`.

## Scope of this file

| Concern | Fixed by | Specified here |
|---|---|---|
| warm-up as the initial-load exclusion, one discarded warm-up repetition, at least 10 repetitions, the order-statistic interval of a median, controlled and ordinary conditions, the desktop clock locks and the S24 fixed-performance command | `Zenith2/Docs/Budgets.md` → "Frame-rate definitions and measurement procedure" ([Z2-N-BUDGET-073] to [Z2-N-BUDGET-085]) | how a study applies them, the interleaved schedule, the checks that verify each condition, and the numbers the core text leaves open |
| raw data of research runs committed under `Zenith2/Docs/Research/` | Z2-D-064, [Z2-N-BUDGET-079] | the run layout and the large-file tier inside git (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file tier") |
| the contents of a shading-path report | `Zenith2/Docs/Rendering.md` → "What each shading-path report contains" | the cell matrix and the verdicts |
| the ECS scenarios and the EnTT adapter cost rows | `Zenith2/Docs/Design/ECS/Tests.md` → "Benchmarks and research scenarios", `Zenith2/Docs/Design/ECS/Backends.md` → "Adapter cost model" | the native-versus-adapter statistics and the classification of each difference |
| the evidence fingerprint and the workload runner | `Zenith2/Docs/Design/DEVT/Evidence.md` → "Fingerprint algorithm", → "Workload runner" | how a research run records and reuses them |
| the harness and ruling split | Z2-D-084, `Zenith2/Docs/DecisionLog.md` → "Ruling an open decision" | the profile each decision follows |

<!-- elaboration -->
Where this file sets a number the core documents do not (a margin, a noise threshold, a drift level, a family cap, a temperature), that number is a procedure parameter in the sense of [Z2-N-BUDGET-003]. The parameters are listed once, in "Metric classes and thresholds" and "Procedure parameter list" below, and cross-document item X416 adds them to `Zenith2/Docs/Budgets.md` → "Procedure parameters", after which a change to one follows the workload change rule there.

## Measurement profiles

Every decision ticket (front matter `kind: decision`) follows exactly one profile, and so does every other ticket that runs or records a measurement (`kind: budget` for RB, `kind: revalidation` for a revalidation run, and any ticket carrying `Research-Role: run`). The profile decides which parts of this methodology apply; nothing outside a profile's column is optional for it.

| Profile | Name | What is decided | Instances (full list in `Zenith2/Docs/Design/RESEARCH/Instances.md`) |
|---|---|---|---|
| RA | Architecture research | a comparison between architectures that are all deliverables, reported as research (Z2-D-064) | the ECS backend research (D-SEL-EnTT-SHIPPING when P1 is approved), the shading-path research |
| RS | Selection by measurement | a choice between candidates, at least one of them measured | D-SEL-HEAP, D-SEL-HASH, D-SEL-JSON, D-SEL-NAV, D-SEL-TEXT, D-SEL-UIRT, D-SEL-CODEC, D-SEL-VIDEO, D-SEL-CRASH, D-SEL-2DPHYS, D-SEL-PHYSX-ANDROID, D-JOLT-PARALLEL-STEP, D-SEL-MOTIONMATCH, D-SEL-FBX, D-SEL-UPSCALER, D-VTX-TANGENTLESS, D-TBDR, D-SCRIPT-TXN's native policy |
| RJ | Justification evidence | whether a custom container, allocator, heap wrapper or threading primitive meets its acceptance criteria against the standard alternative (Z2-D-041) | the CORE container records, `Zenith_HashMap`, the JOBS deque and queue, the DEVT name table, the three ECS records |
| RB | Budget allocation by harness | the value of a pending budget row whose basis is a Stage 1 harness measurement | every "Stage 1 per-system budget ticket for <AREA>" with a harness half |
| RP | Probe | a pass or fail per case per device, no statistics | D-SLANG, the baseline-profile confirmation, D-LUAU-STATELESS's corpus, the SDL3 surface callback, the cross-platform hash record, the thermal cadence |
| RQ | Qualitative ruling | a classification, refinement or scale definition with no measurement | D-PHASES, D-CLASS-SIM, D-LUAUDBG, the workload scale tickets, the ECS and GRAPH specification rulings, design-ruling records |

### Profile rule table

| Rule | RA | RS | RJ | RB | RP | RQ |
|---|---|---|---|---|---|---|
| a committed `study.json` before the run ("Study design and pre-registration") | yes | yes | yes | yes | yes | no |
| controlled conditions and the noise checks of this file | yes | yes | yes | the conditions the row's procedure names; the checks of the named class | only for a timed case; `ordinary` when the probed behaviour is the device's own adaptation | no |
| at least 10 and at most 20 valid launches per cell, plus the warm-up | yes | yes | yes | the repetition count of the row's procedure, never fewer than 10 for a `controlled` harness run | 1 per (case, device), 3 for a case marked `flaky-device` | no |
| interleaved schedule | yes | yes | yes | yes when the harness measures two or more configurations | no | no |
| comparison statistics and verdicts | yes | yes | yes | no: the `meets` operator alone | no | no |
| the run directory and record format | yes | yes | yes | yes, beside Budgets' `allocation.json` | yes | no |
| the report template | yes, with the RA sections | yes | yes | yes | yes, the probe table | no report; the DecisionLog entry carries the evidence citations |
| the three DecisionLog research fields | yes | yes | yes | yes | yes | **Revalidation** only |
| revalidation triggers | yes | yes | yes | yes | yes | yes |
| `needs-human` on every ticket that commits under `runs/` | yes | yes | yes | yes | yes | — (an RQ ticket commits nothing under `runs/`) |

- A ticket whose decision needs measurement and a qualitative judgement (D-SEL-VIDEO's licence review, D-SEL-FBX's licence review, D-SEL-UIRT's visual authoring criterion) follows RS; the qualitative criterion is a `judgement(<text>)` term of its acceptance rule ("Rule grammar"), recorded in the report's "Conclusion" section with its reasoning, and never decides a comparison the measured terms already decided.
- A decision that is ruled from two harnesses (D-SEL-MOTIONMATCH from `Zenith_Bench_AnimMotionMatching` and `Zenith_Bench_CharMotionMatchAuthoring`) is one study with the cells of both; its comparisons that cross the two executables are listed in `comparesHarnesses` ("Confound check").
- A decision ruled from a separate `needs-human` measurement-run ticket and a ruling ticket (D-SEL-2DPHYS: the run Z2T-TWOD-014, the ruling Z2T-TWOD-015) commits the run directory in the run ticket and the report and DecisionLog entry in the ruling ticket; both tickets carry the template lines of `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Decision ticket template", the run ticket with `Research-Role: run`.

## Study design and pre-registration

A study is one question, measured by one or more runs. Its protocol is the file `Zenith2/Docs/Research/<Study>/study.json` (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "study.json"). The harness ticket of the decision commits it in the same diff as the harness; the ruling ticket runs it. The protocol is fixed before any measured launch exists, so the analysis cannot be chosen after the data is seen.

- The harness ticket is an ordinary loop ticket: `study.json` is text a worker writes from the decision's acceptance rule, and its validity is proven in the `Test` step by `Pwsh_Research_StudyFilesValid` (`Zenith2/Docs/Design/RESEARCH/Tests.md`).
- A ruling ticket that changes `study.json` (a cell added, a margin changed) records each change in the record's `deviations` and in the report's "Method" section; the change is allowed only before the first measured launch of the run it applies to, which the record proves by `studySha256` equal to the blob the run read.
- A study whose decision is re-ruled keeps one `study.json`; a new protocol for a re-ruling is a new study version, `schemaVersion` unchanged and `studyVersion` incremented, and runs record the `studyVersion` they followed.

### Factors cells and metrics

| Term | Meaning | Constraint |
|---|---|---|
| factor | the one input a comparison varies: `candidate` (the selection alternative, the ECS backend, the shading path, the allocator), `api` (`native` or `adapter`), `backend` (`vulkan`, `d3d12`), `variant` (EnTT `view` or `group`) | every comparison names exactly one factor |
| cell | one configuration launched: (scenario, candidate, api, backend, variant), each `null` where it does not apply | id `^[a-z0-9][a-z0-9-]{0,63}$`, unique in the study |
| metric | one measured quantity of a cell, with a unit, a direction (`lower` or `higher` is better), a metric class and a per-launch statistic | id `^[a-z0-9][a-z0-9-]{0,63}$`; a cell yields one or more metrics |
| series | the values of one (cell, metric) over the launches of one run, one value per launch | one series file per (cell, metric) |
| comparison | (candidate cell, baseline cell, metric, factor, primary or secondary, family key) | the two cells differ only in the members the factor allows ("Confound check") |
| family | the primary comparisons sharing one family key (`familyBy`, "Multiplicity") on one platform | at most 12 comparisons per family |

- Primary comparisons decide the acceptance rule or carry a reported classification; secondary comparisons are reported and never decide anything. A comparison whose verdict feeds a classification table (the ECS classification) is primary in its family.
- A cell whose platform cannot run it (Deferred and the visibility buffer on the S24; `Zenith_Bench_ECSWorkload` on the S24) is declared `notApplicable` with a reason and is reported as such, never omitted.

### Confound check

Every cell records an inputs object (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "record.json"): the tree SHA, preset and configuration, the seven fingerprint members of `Zenith2/Docs/Design/DEVT/Evidence.md` → "Fingerprint algorithm" (each `null` where the cell has no workload or no content), the quality keys passed, the launch target, and the cell's command line with its placeholders unexpanded. Two cells are comparable for a factor when every inputs member is equal except the members the factor allows to differ:

| Factor | Members allowed to differ | Always equal |
|---|---|---|
| `candidate` | the command line's candidate argument; `closure` when the candidate is a link-time choice (an allocator, an ECS backend in a workload game); additionally `launchTarget`, `harness` and the command's executable, for a cell pair listed in `comparesHarnesses` | `toolchain`, `thirdParty`, `shaderBlobs`, `content`, `workloadDefinition`, `qualityKeys` |
| `api` | `launchTarget`, `harness`, `closure` and the command line, for a cell pair listed in `comparesHarnesses` (a native harness and the through-API harness are always different executables) | as above |
| `backend` | the command line's backend argument, `rhi.backend` | as above, except `shaderBlobs` when the backend selects a different blob set |
| `variant` | the command line's variant argument | as above |

- `comparesHarnesses` in `study.json` lists each pair of launch targets a comparison may cross, `{ "a": "<target>", "b": "<target>", "justification": "<20 … 200 characters>" }`; a pair is unordered. The ECS study lists (`Zenith_Bench_ECSNative`, `Zenith_Bench_ECS`), (`ZenithECS_Custom_Tests`, `Zenith_Bench_ECS`), (`ZenithECS_Custom_Tests`, `Zenith_Bench_ECSNative`), (`Zenith_Bench_BudgetsECS`, `Zenith_Bench_ECSWorkload`), (`ZenithECS_Custom_Tests`, `Zenith_Bench_BudgetsECS`) and (`ZenithECS_Custom_Tests`, `Zenith_Bench_ECSWorkload`): the custom W-Dense native cell launches through `ZenithECS_Custom_Tests --bench-native WDense`, while flecs and EnTT run natively inside `Zenith_Bench_BudgetsECS`.
- Command-line normalisation: before two commands are compared, every `launch.args` element equal to the cell's own `candidate`, `backend` or `variant` token is replaced by `{candidate}`, `{backend}` or `{variant}`, and for a `comparesHarnesses` pair the executable names are replaced by `{target}`; the normalised commands must then be equal, element by element.
- A comparison that crosses harnesses is marked `crossHarness: true` in `analysis.json`, and the report's "Method" section lists its target pair and justification.
- A comparison whose cells differ in any member its factor does not allow is `INVALID` with reason `CONFOUNDED`, naming the member.

### Hypothesis and acceptance rule

- The study states one hypothesis sentence ("mimalloc raises allocation throughput on the W-Dense trace by more than 3% on both reference platforms"), the alternatives (at least two), the acceptance rule as an expression of the operators of "Acceptance rule operators", and the outcome when the rule is false.
- An alternative may be the incumbent (the established default the false-outcome keeps), and at most one is. A decision with no default between its candidates (D-SEL-JSON, D-SEL-CODEC, D-SEL-UIRT, D-SEL-HASH, D-SEL-2DPHYS, D-SEL-TEXT) has no incumbent; its `outcomeWhenFalse` is `human(<text>)` or `blocked(<text>)`, or its rule is an ordered `first(…)` whose last clause is `true -> <outcome>`. `keep(<x>)` names the incumbent only.
- A decision's acceptance rule in `Zenith2/Docs/DecisionLog.md` → "Open decisions" stays the authority; the study's expression is its translation, and "Instance phrase mapping" fixes how each phrase the open decisions use is translated.

### Fixed repetition count

- The launch count `n` of every cell is fixed in `study.json`, between 10 and 20 inclusive ([Z2-N-BUDGET-076]); the cap of 20 keeps every exact count of "Statistics" inside 2^63.
- No optional stopping: a run never adds launches because a result is inconclusive. An inconclusive result is a result, and the rule's false-outcome applies; a new run is a new run, declared as a deviation with its reason, and the earlier run stays committed and reported.

## Native versus adapter measurement

Profile RA's ECS study measures every scenario twice per backend, natively and through `Zenith_ECS`, so that the report separates what the storage architectures do from what the common API costs (Z2-D-064). The harnesses and scenarios are ECS's (`Zenith2/Docs/Design/ECS/Tests.md` → "Benchmarks and research scenarios"); this section fixes the cell matrix and the statistics.

### ECS cell matrix

| Scenario | Native cells | Adapter cells |
|---|---|---|
| `churn`, `iterate1`, `iterate2`, `iterate4`, `random-access`, `add-remove`, `hierarchy` | `<scenario>-native-custom` (launched through `ZenithECS_Custom_Tests --bench-native`), `<scenario>-native-flecs`, `<scenario>-native-entt-view`, `<scenario>-native-entt-group` (`Zenith_Bench_ECSNative`) | `<scenario>-adapter-custom`, `<scenario>-adapter-flecs`, `<scenario>-adapter-entt` (`Zenith_Bench_ECS`) |
| `wdense` | `wdense-native-custom` (`ZenithECS_Custom_Tests --bench-native WDense`, launch kind `test-native`), `wdense-native-flecs`, `wdense-native-entt-view`, `wdense-native-entt-group` (`Zenith_Bench_BudgetsECS`) | `wdense-adapter-<backend>` through `Zenith_Bench_ECSWorkload`, Windows only |
| `luau-behaviours-vs-chunk-systems` | `notApplicable` (needs the Luau runtime) | the two forms per backend, Windows only |
| `container-churn` | `notApplicable` | per backend, plus the per-block `Zenith_Heap` baseline cell |

Metrics per cell: `time` (class `bench-time`, the per-launch median epoch time of the scenario), `jobs-overhead` (class `cpu-time`, `jobs.overhead_ns`, adapter cells only), and for EnTT adapter cells the four secondary cost metrics of `Zenith2/Docs/Design/ECS/Backends.md` → "Adapter cost model" (`component-bytes`, `change-bytes`, `adapter-waste-bytes` of class `memory`; `query-refresh` of class `cpu-time`).

The ECS study declares `familyBy: "scenario"`: each scenario's `time` comparisons (at most 3 native pairs, 2 EnTT `group` rows, 3 adapter pairs and 4 adapter-overhead rows, 12 in all) form one family, so every classification label is produced at the family-wise level of "Multiplicity" with k = 14 at n = 10.

### Adapter overhead and share

For a scenario s and backend b with native series N and adapter series A (EnTT: once against each native variant, both reported, neither picked):

1. Adapter overhead: the shift estimate and its interval of "Shift estimate and its interval" with candidate A and baseline N (positive is slower through the adapter), and the verdict of `better(native, adapter, time)` read as "the adapter costs more than the margin".
2. Relative overhead: shift × 10^6 ÷ median(N), in ppm, with the interval endpoints divided the same way and rounded away from zero.
3. Adapter share, the quantity flagged proposal P1 asks for (`Zenith2/Docs/Design/ECS.md` → "Flagged proposal P1"): `(median(A) − median(N)) × 10^6 ÷ median(A)` in ppm, with the interval-arithmetic bounds of ECS's assembler, `low = (A_low − N_high) ÷ A_low`, `high = (A_high − N_low) ÷ A_high`, each in ppm rounded away from zero and clamped to [−1,000,000, 1,000,000], where `_low` and `_high` are the endpoints of each series' median interval. A negative share (the adapter faster than native) is kept. When `A_low`, `A_high` or median(A) is 0 the share and its bounds are 0 and the row carries the reason `ZERO_MEDIAN`; a relative overhead with median(N) = 0 is 0 with the same reason. The report's share is this one; the ECS assembler's own `double` share in its raw report is not reported (cross-area row RS5).
4. Job-scheduling overhead per backend: the median and interval of the `jobs-overhead` series, reported per scenario and backend.

### Architectural and adapter classification

For each scenario and each pair of backends (b1, b2), with EnTT's native series taken in its `view` variant and the `group` variant reported as a second row:

1. The architectural comparison: `native-b1` against `native-b2` on `time`, verdict V_native.
2. The through-API comparison: `adapter-b1` against `adapter-b2` on `time`, verdict V_adapter.
3. The difference is classified:

| V_native | V_adapter | Classification | Meaning in the report |
|---|---|---|---|
| BETTER or WORSE | the same verdict | ARCHITECTURAL | the storage architectures differ, and the common API preserves it |
| BETTER or WORSE | EQUIVALENT or INCONCLUSIVE | ADAPTER-MASKED | an architectural difference the adapter hides |
| BETTER or WORSE | the opposite verdict | ADAPTER-REVERSED | the adapter reverses the architectural order |
| EQUIVALENT | BETTER or WORSE | ADAPTER-INTRODUCED | a difference that exists only through the adapter |
| EQUIVALENT | EQUIVALENT | NONE | no difference either way |
| INCONCLUSIVE or INVALID in either (except the rows above) | any | UNRESOLVED | named with the verdict reasons; never classified |

4. The adapter-artefact magnitude of the pair is `shift_adapter − shift_native` with the conservative bounds `[low_adapter − high_native, high_adapter − low_native]`, reported beside the classification.

### Shading path cell matrix

The shading-path study's harness is RPROD's `Zenith_Bench_ShadingPaths` (`Zenith2/Docs/Design/RPROD/Tests.md` → "Benchmarks and harnesses"); its report contents are `Zenith2/Docs/Rendering.md` → "What each shading-path report contains".

| Dimension | Values |
|---|---|
| candidate | `forward-plus` (the incumbent default, Z2-D-035), `forward`, `deferred`, `visbuf` |
| workload | the workload versions the study names; every path runs the same versions (confound check: `workloadDefinition` and `content` equal) |
| backend | `vulkan` and `d3d12` on the reference desktop; `vulkan` on the S24 |
| platform availability | Deferred and the visibility buffer are `notApplicable` on the S24 with reason "desktop-only path" |

| Metric | Class | Per-launch statistic | Role |
|---|---|---|---|
| `gpu-frame` (main-view GPU frame time) | `gpu-time` | median over the measured interval | primary against `forward-plus` |
| `gpu-pass-<pass>` (one per pass of the path's frame anatomy) | `gpu-time` | median over the measured interval | secondary |
| `frame-time` | `cpu-time` | p95 by nearest rank | secondary |
| `transient-peak-aliased`, `transient-peak-dedicated` | `memory` | maximum over the run | secondary |
| `persistent-targets`, `pool-bytes` | `memory` | maximum | secondary |
| `bandwidth-read`, `bandwidth-write` | `bandwidth` | p95 per frame | secondary |
| `flip-mean-vs-forward` | `image` | one value per parity scene | parity criterion, compared with its tolerance row, never statistically |

The capability-matrix exclusions that applied to each cell are recorded in its record entry (`exclusions`) and listed in the report's study-specific section.

## Run procedure

### Sessions

- A run is one session: one machine, one boot, one platform, one study version. Its directory is `Zenith2/Docs/Research/<Study>/runs/<RunId>/` (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "Run directory"). Series and comparisons are computed within one run, and each run's `analysis.json` stands alone for them; a comparison's two cells are always in one run. A ruling that measured two platforms, or one platform in several sessions, cites one run per (platform, session), and its acceptance rule is evaluated once over all of them ("Rule evaluation").
- The human commits each run directory before starting the next session of the same study, so every session starts from a clean tree (D8) and records the tree SHA it measured.
- A desktop session is preceded by a reboot and 300 s idle after sign-in, checked by D9; an S24 session by a reboot and 120 s idle after `sys.boot_completed` reads `1`.
- A desktop session holds the machine-wide build slot for its whole duration (`pwsh -NoProfile -File Tools/build_lock.ps1 -Run "<the runner's command line>" -WaitMinutes 0`) and runs with every lane idle, because a lane build on the measuring machine is background load; the runner refuses to start when `build_lock.ps1` reports the slot held by another process. A desktop session is therefore scheduled by the human at a time no lane is fired, as stage acceptance runs are (Z2-D-083).
- A session lasts at most 12 hours (procedure parameter `research.sessionMaxHours`). `study.json` gives each cell an `estimatedSlotSeconds`; `Test-Zenith2ResearchStudy` and `-DryRun` compute `slots × (estimatedSlotSeconds + cooldownAllowanceSeconds)` per platform (cooldown allowance 300 s on the desktop, 600 s on the S24, slots = cells × (n + 1)) and refuse a run plan above the cap with `RESEARCH_ERROR_SESSION_CAP`. A study too large for one session declares `sessions` (each a list of cell ids whose comparisons stay inside it); each session is its own run.
- The runner may resume a session after its own crash only on the same boot: the record's `session.bootId` (Windows: the `LastBootUpTime` of `Win32_OperatingSystem` as UTC seconds; Android: `/proc/sys/kernel/random/boot_id`) must equal the current one, otherwise `-Resume` is refused with `RESEARCH_ERROR_BOOT_CHANGED`.

### Warm-up

- A frame-workload cell's warm-up inside each launch is the initial-load exclusion window of [Z2-N-BUDGET-073]; nothing else is excluded.
- Before the first measured launch, every cell of the session runs one complete warm-up launch in schedule slot group 0; its values are written to the series with `repetition` 0 and `valid` 0 and reason `WARMUP`, and never enter a statistic ([Z2-N-BUDGET-074], read per cell for an interleaved design; cross-document item X416 (b) records the reading).

### Replication and interleaving

- The unit of replication is the process launch, and each launch contributes exactly one value to each of its cell's series. Code placement, address-space layout and allocator state vary between launches as they would between runs of a shipped build, which is the independence the exact statistics of "Statistics" assume.
- A harness that measures several internal repetitions inside one launch (nanobench epochs, ECS's in-process repetitions, `samplesNs` arrays) reduces them to the one value of that launch with the metric's per-launch statistic ("Per-launch statistic"); the internal values are raw data, committed in the launch's raw file, and never separate samples.
- A cell whose launch is expensive (a workload boot, a navmesh bake) still runs n launches; its cost shows in `estimatedSlotSeconds` and may split the study into sessions.
- The schedule interleaves cells so that slow drift of the machine (temperature, background maintenance) spreads over every candidate instead of biasing the one measured last.

### Schedule algorithm

Input: the session's cell list C in study order (`notApplicable` cells removed), the launch count n, and the seed `scheduleSeed` (a 64-bit unsigned integer in `study.json`, the same for every run of the study version).

1. The generator is SplitMix64 with state `x = scheduleSeed`: each draw sets `x = x + 0x9E3779B97F4A7C15`, then `z = x`; `z = (z ^ (z >> 30)) × 0xBF58476D1CE4E5B9`; `z = (z ^ (z >> 27)) × 0x94D049BB133111EB`; `z = z ^ (z >> 31)`, all modulo 2^64, and returns z (so the first draw already adds the increment once).
2. A shuffle of C: copy C in study order into P; for i from |C| − 1 down to 1, draw z, let `j = z mod (i + 1)` and swap `P[i]` with `P[j]`.
3. Slot group 0 (the warm-up) is one shuffle; slot group r (1 to n) is the next shuffle drawn from the same generator, so every repetition has its own order and no cell has a fixed predecessor.
4. The schedule is the list of `(slot, cell, repetition)` triples in group order, slots numbered from 1; the runner writes it into the record (`procedure.order`) before the first launch.

Complexity O(|C| × n). The same seed and the same cell list always give the same schedule, so two runs of one study version with equal cell lists are comparable slot for slot. Worked values (fixture-derived): seed 0 and cells A, B, C give groups CAB, CAB, CAB, BAC, BAC for r = 0 … 4; seed 1 gives ABC, CBA, BCA, CBA, BCA; seed 11400714819323198485 gives CBA, ACB, CBA, ABC, ABC, and with cells A, B, C, D gives CDBA, CBAD, BDCA, ACDB, ABCD.

### Invalid repetitions and retries

A slot is invalid when any check of "Noise control on the reference desktop" or "Noise control on the S24" fails at its start or end, when its launch exits non-zero or times out, when its output fails extraction (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "Extraction selectors"), or, for a frame-workload cell, when its evidence record's `result` is `invalid` ([Z2-N-BUDGET-069], [Z2-N-BUDGET-070]).

1. The slot's row is written with `valid` 0 and the reason code ("Invalidity reasons" in `Zenith2/Docs/Design/RESEARCH/Formats.md`).
2. The runner inserts a retry slot for the same (cell, repetition) in the next slot group, after that group's own slots (the retry runs one group later, inside the interleaving rather than after it); a retry of the last group runs immediately after that group.
3. A (cell, repetition) is retried at most 3 times; after the third failure the cell has fewer than n valid launches, its series is `INSUFFICIENT`, and every comparison using it is `INVALID`.
4. Rows are never removed: invalid rows stay in the series file, so the report can show what failed.
5. Every statistic that depends on order (the drift test) orders the valid values by `repetition`, not by slot, so a retried launch takes its repetition's place.
6. Invalidation can depend on the candidate (a configuration that heats the GPU loses more slots to `THERMAL`, and its retries start cool). A comparison carries the reason `INVALID_RATE` when either cell's invalid launches exceed 20% of its executed slots, or when the two cells' invalid percentages differ by more than 10 percentage points (integer arithmetic: `5 × invalid > executed`, and `|100 × invalid_a × executed_b − 100 × invalid_b × executed_a| > 10 × executed_a × executed_b`; a 20-point difference would be implied by the 20% rule, so the differential uses 10); the report lists it under threats to validity, and the verdict is unchanged.

## Noise control on the reference desktop

The reference desktop is `Zenith2/Docs/Budgets.md` → "Reference desktop". Controlled conditions are [Z2-N-BUDGET-083]'s locks; this section adds how they are applied and verified. The runner runs in an elevated shell on the desktop (the clock locks need it); a non-elevated start fails D2.

### Desktop session preflight

| # | Check | Command or source | Pass |
|---|---|---|---|
| D1 | power plan | once, `powercfg /duplicatescheme 381b4222-f694-41f0-9685-ff5bb260df2e <guid>` and `powercfg /changename <guid> "Zenith2 Research"`; minimum and maximum processor state both 99% on AC (`powercfg /setacvalueindex <guid> SUB_PROCESSOR PROCTHROTTLEMIN 99` and `PROCTHROTTLEMAX 99`), active (`powercfg /setactive <guid>`) | `powercfg /query <guid> SUB_PROCESSOR` reports both values 99 and `powercfg /getactivescheme` names the scheme; during D3, every sample of `\Processor Information(_Total)\% Processor Performance` is at most 100, which shows boost is off on the reference CPU |
| D2 | GPU clocks | `nvidia-smi --lock-gpu-clocks=<base>,<base>` and `--lock-memory-clocks=<rated>,<rated>` with `<base>` and `<rated>` from `nvidia-smi --query-gpu=clocks.default_applications.graphics,clocks.default_applications.memory --format=csv,noheader,nounits` | both lock commands exit 0 (a refusal, including "not supported" for memory clocks on the installed part or a non-elevated shell, fails D2 with the command's message); after 10 s of D3, `clocks.gr` within 15 MHz and `clocks.mem` within 50 MHz of the locked values |
| D3 | idle background load | 30 samples at 1 Hz of `\Processor Information(_Total)\% Processor Utility` (`Get-Counter`) and `nvidia-smi --query-gpu=utilization.gpu` | mean CPU ≤ 3%, every GPU sample ≤ 2%, and no process other than the allowlist (`System`, `Idle`, `pwsh`, `conhost`, the Windows desktop shell processes, `nvidia-smi`, the runner itself) above 1% CPU in the `Get-Process` CPU-time delta over the window |
| D4 | GPU temperature | `nvidia-smi --query-gpu=temperature.gpu` | ≤ 50 °C |
| D5 | display | `-DisplayChecked` given: the operator attests one active display at 2560×1440, variable refresh off and HDR off; the runner reads `Win32_VideoController` and records resolution and refresh rate | the switch present and the read resolution 2560×1440 |
| D6 | pending reboot and updates | `-UpdatesPaused` given (Windows Update paused by the operator for the session); `HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired` absent | as stated |
| D7 | ambient temperature | `-AmbientC` | 20 °C to 26 °C |
| D8 | Release build present | the executable of every cell exists in the measured tree's `win-release` output, and `Get-Zenith2TreeSha` reports a clean tree (earlier runs of the study committed, "Sessions") | as stated |
| D9 | recent reboot | `now − LastBootUpTime` (`Win32_OperatingSystem`) | between 300 s and 3 hours |

A failed D1 to D9 refuses the session (`RESEARCH_ERROR_PREFLIGHT`, naming the check); nothing is measured.

### Desktop repetition checks

| # | When | Check | Failure |
|---|---|---|---|
| R1 | before each slot | GPU temperature ≤ 50 °C; when above, the runner waits, polling every 10 s, for up to 300 s | still above after 300 s: the slot is invalid with `THERMAL` |
| R2 | before each slot | `clocks.gr` within 15 MHz and `clocks.mem` within 50 MHz of the locks | invalid with `CLOCKS` |
| R3 | before each slot | a 5-s background sample: mean CPU ≤ 3% | invalid with `BACKGROUND` |
| R4 | after each slot | the launch exited 0 within the cell's timeout; the GPU was not reset (`nvidia-smi --query-gpu=reset_status.reset_required` reads `No`) | invalid with `EXIT` or `DEVICE` |
| R5 | after each slot | `nvidia-smi --query-gpu=clocks_event_reasons.sw_power_cap,clocks_event_reasons.hw_slowdown,clocks_event_reasons.hw_thermal_slowdown,clocks_event_reasons.sw_thermal_slowdown,clocks_event_reasons.hw_power_brake_slowdown` sampled once per second by a background sampler process for the slot's duration (`nvidia-smi --query-gpu=… --format=csv -lms 1000 -f <work file>`, started before and stopped after the launch) | any sample reporting `Active` for one of the five makes the slot invalid with `CLOCKS` |

### Desktop restoration

After the last slot, and on any exit of the runner, `nvidia-smi --reset-gpu-clocks` and `--reset-memory-clocks` run and the previously active power scheme is restored (`powercfg /setactive <recorded guid>`); the restoration result is recorded in `session.restored`. A runner killed before restoration leaves the locks in place; the next runner start detects a locked GPU (`clocks.applications` differing from the defaults) or the active scheme named `Zenith2 Research`, and refuses with `RESEARCH_ERROR_UNRESTORED` until `Run-Research.ps1 -RestoreOnly` has run.

### Desktop calibration

The thresholds of D3, D4 and R1 are asserted, not yet measured on the reference desktop. The Stage 1 research conformance integration ticket runs `Run-Research.ps1 -Calibrate` once: the runner applies D1 and D2, then records 30 minutes of the D3 and D4 samples idle and the R1 cooldown curve after a 5-minute GPU load (the `Zenith_Bench_BindlessProbe` full run), and writes `Zenith2/Docs/Research/Machines/<machineId>.calibration.json`. When the idle mean CPU exceeds 2%, the GPU never cools below 48 °C, or the cooldown from load to 50 °C takes more than 240 s, that ticket proposes changed values through the procedure-parameter rule of [Z2-N-BUDGET-003] in the same diff.

## Noise control on the S24

The reference Android device is `Zenith2/Docs/Budgets.md` → "Reference Android device". Controlled conditions enable fixed-performance mode ([Z2-N-BUDGET-084]); the device still overheats and still schedules other work ([Z2-N-BUDGET-081]), so the checks below bound both.

### S24 session preflight

| # | Check | Command | Pass |
|---|---|---|---|
| A1 | the device is the reference device | `adb shell getprop ro.product.model` | starts with `SM-S928` (Galaxy S24 Ultra); otherwise the record is never evidence ([Z2-N-BUDGET-124]) and the session is refused |
| A2 | fixed-performance mode | `adb shell cmd power set-fixed-performance-mode-enabled true`, then `adb shell dumpsys power` | the command exits 0 and the dump has a line matching `(?i)fixed.?performance.*(true|enabled)`; a refused command or no such line runs the session under "Fixed-performance mode unavailable" |
| A3 | radios and notifications | `adb shell cmd connectivity airplane-mode enable`; `adb shell cmd notification set_dnd priority` | both exit 0 |
| A4 | screen | `adb shell settings put system screen_brightness_mode 0`; `settings put system screen_brightness 128` (50% of 255, the procedure parameter of [Z2-N-BUDGET-084]); `svc power stayon usb`; screen on | each value read back equal with `settings get` |
| A5 | background apps | `adb shell am kill-all`; then `adb shell dumpsys cpuinfo` twice, 10 s apart | the `TOTAL` line of the second report below 5% |
| A6 | battery | `adb shell dumpsys battery`: `level` 50 to 100, `temperature` ≤ 330 (tenths of °C) | as stated |
| A7 | thermal status | `adb shell dumpsys thermalservice`: `Thermal Status: 0` | as stated |
| A8 | ambient | `-AmbientC` | 23 °C to 27 °C; the device lies screen-up on a non-insulating surface without a case |

### S24 repetition checks

| # | When | Check | Failure |
|---|---|---|---|
| S1 | before each slot | thermal status 0 and battery `temperature` ≤ 330; when not, wait polling every 10 s for up to 600 s | invalid with `THERMAL` |
| S2 | before each slot | `level` ≥ 40 | the session pauses until the operator confirms charging back above 50% within the session cap, else it ends with the remaining slots invalid `BATTERY` |
| S3 | after each slot | thermal status ≤ 1 (`LIGHT`) | a status of 2 (`MODERATE`) or above makes the slot invalid with `THERMAL`, because throttling began during it |
| S4 | after each slot | the launch exited 0 and its output was pulled (`adb pull`) with the size `adb shell stat -c %s` reported | invalid with `EXIT` |

The runner records, per slot, the start and end thermal status, battery temperature and level, and whether the USB connection was charging (`dumpsys battery` `USB powered`), so the report can show the thermal history.

### Fixed-performance mode unavailable

When A2 does not pass, the run is still `controlled`, with `conditions.android.fixedPerformance: "unavailable"`, and the stricter limits apply: S1's battery temperature limit becomes 310 (31.0 °C), S1's wait cap 900 s, and S3 requires thermal status 0. The report's "Exclusions and threats to validity" section states that clocks were governed by the device.

## Statistics

### Arithmetic rules

- Every sample value is a non-negative integer in its unit's base (`ns`, `bytes`, `count`, `bytes_per_s`, `micro` for a dimensionless quantity × 10^6, `pass` for 1 or 0), at most 2^63 − 1. No floating-point value is stored, parsed or compared anywhere in the analysis.
- Every arithmetic operation the module performs on a sample-derived value uses `[System.Numerics.BigInteger]`: sums, products, `BigInteger::Divide` and `BigInteger::DivRem` for quotients and remainders, and explicit floor, ceiling and away-from-zero rounding built from `DivRem`. The PowerShell `/` and `%` operators and `[math]::` are never applied to such a value (on `Int32` and `Int64` operands `/` returns `[double]` for an inexact quotient and `Int64` overflow silently becomes `[double]` in `pwsh` 7, and a `BigInteger` `/` truncates toward zero; rule P4 of `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "PowerShell rules of the module", proved by `Tests/Probes/Probe-PwshRules.ps1`).
- Ratios are integers in parts per million: `floor(numerator × 10^6 ÷ denominator)` for a non-negative ratio, rounded away from zero for an interval endpoint. Probabilities are exact rationals stored as a numerator and a denominator: a p-value as `count / total` unreduced, every other rational reduced to lowest terms with a positive denominator.
- With at most 20 launches per series every exact count stays below `C(40, 20) = 137,846,528,820` for rank sums and `20! = 2,432,902,008,176,640,000` for drift.
- The analysis output is a pure function of the committed record and series, so two runs of `Invoke-Zenith2ResearchAnalysis` on any machine produce byte-identical `analysis.json` (`Pwsh_Research_AnalysisByteStable`).

### Per-launch statistic

Each series holds one value per launch: the statistic of that launch's raw measurements that the metric declares.

| Statistic | Definition over the launch's k raw measurements sorted ascending |
|---|---|
| `median` | the value at 1-based rank `(k + 1) ÷ 2` (integer division: the lower median, the same rule as `Zenith2/Docs/Design/DEVT/Evidence.md` → "Statistics") |
| `p95`, `p99` | nearest rank `(95k + 99) ÷ 100`, `(99k + 99) ÷ 100` (integer division, equal to `ceil(0.95k)` and `ceil(0.99k)`, [Z2-N-BUDGET-063]) |
| `max`, `min` | the extremes |
| `total` | the sum |
| `single` | the one measurement (a deterministic quantity, a probe case) |

The raw measurements are the measured-interval frames of a frame-workload launch (the trace of `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Trace files"), or the internal repetitions or epochs of a harness launch; the runner computes the statistic at extraction, and the analysis recomputes it from the trace or the raw file.

### Median and its interval

For the n valid values of a series sorted ascending x(1) ≤ … ≤ x(n):

1. Median: x((n + 1) ÷ 2).
2. Interval: `[x(j), x(k)]` with j the largest integer such that `P(X ≤ j − 1) ≤ 1/40` for X ~ binomial(n, 1/2), and k = n + 1 − j ([Z2-N-BUDGET-077]). Coverage `1 − 2 × P(X ≤ j − 1)`. For n ≤ 5 no j ≥ 1 exists, so the function accepts n from 6 to 20 and refuses others; a study's series always hold 10 to 20.

| n | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| j | 2 | 2 | 3 | 3 | 3 | 4 | 4 | 5 | 5 | 5 | 6 |
| k | 9 | 10 | 10 | 11 | 12 | 12 | 13 | 13 | 14 | 15 | 15 |
| coverage | 501/512 | 253/256 | 1969/2048 | 1001/1024 | 4043/4096 | 247/256 | 32071/32768 | 31161/32768 | 7939/8192 | 64277/65536 | 125647/131072 |

The table is computed by the tool from the definition, never read from this file; `Pwsh_Research_MedianIntervalTable` asserts the tool's j and k for n = 10 … 20 against the table. These intervals replace the percentile-bootstrap wording of [Z2-N-REND-958] (cross-document item X417): with 10 launches a bootstrap interval of a median is unreliable, and two documents cannot both define one interval. At n = 10 the interval [x(2), x(9)] covers 97.85%, so `disjoint`, `beyondSpread` and D-TBDR's "intervals do not overlap" are stricter than they would be with a 95% bootstrap interval.

### Robust coefficient of variation

`rCV = floor(14826 × MAD × 10^6 ÷ (10000 × median))` ppm, where MAD is the lower median of `|x(i) − median|`; 1.4826 scales the MAD to a standard deviation for normal data. A series with median 0 has rCV 0 when every value is 0 and is `NOISY_INVALID` otherwise. The run-to-run variance the core documents require in every report ([Z2-N-BUDGET-076], [Z2-N-BUDGET-081]) is reported as rCV together with the minimum and maximum valid value.

### Drift test

The drift test asks whether a series trends with execution order, which interleaving cannot remove when the trend is strong.

1. Order the valid values by `repetition` (1 … n); let n be their count.
2. Count discordant pairs: for every pair i < j, add 2 when `v(i) > v(j)` and 1 when `v(i) = v(j)`; this is the doubled inversion count I2.
3. I = I2 ÷ 2 when I2 is even; otherwise I2 ÷ 2 rounded toward the mean `n(n − 1)/4` (conservative).
4. The null distribution is the Mahonian distribution of inversion counts of a random permutation of n, computed by the recurrence `M(1, 0) = 1`, `M(k, i) = Σ_{t=0}^{k−1} M(k − 1, i − t)`.
5. Two-sided p = `Σ_{i : |i − n(n−1)/4| ≥ |I − n(n−1)/4|} M(n, i) ÷ n!`.
6. DRIFT when p ≤ 1/100.

For n = 10 that is I ≤ 8 or I ≥ 37 (p = 33,198 ÷ 3,628,800 at the boundary, 0.0091). Complexity O(n²) for the count and O(n³) for the distribution. A DRIFT series downgrades every verdict it takes part in to `INCONCLUSIVE` with reason `DRIFT`.

### Rank-sum test with ties

The exact two-sided Wilcoxon–Mann–Whitney test, exact also under ties.

1. Pool candidate A (m values) and baseline B (n values), N = m + n, sort ascending; give each group of equal values the doubled mid-rank `first rank + last rank` (an integer).
2. W2 = the sum of A's doubled ranks; the reported statistic is U2 = W2 − m(m + 1) (twice the Mann–Whitney U).
3. The null distribution of W2 given the tie groups: a dynamic programme over groups g (size t_g, doubled rank r_g), `count[c + k][s + k·r_g] += count[c][s] × C(t_g, k)` for k = 0 … t_g, starting from `count[0][0] = 1`; the distribution is `count[m][·]`, totalling C(N, m).
4. With E2 = m(N + 1), p = `Σ_{s : |s − E2| ≥ |W2 − E2|} count[m][s] ÷ C(N, m)`.

The table holds at most N × m × 2N = 64,000 entries at N = 40; at m = n = 20 with no ties the programme visits about 5,600 non-zero states after its 40 groups, which `pwsh` computes in about 1.3 s. Worked example (fixture `rank-sum-separated`): A = 962, 958, 970, 955, 966, 960, 964, 957, 968, 961 and B = 1003, 998, 1010, 1001, 995, 1007, 1000, 1004, 999, 1006 give W2 = 110, U2 = 0, p = 2 ÷ 184,756.

### Shift estimate and its interval

1. Form the m × n differences `d = a − b`, sort ascending.
2. The Hodges–Lehmann shift estimate is d((mn + 1) ÷ 2) (the lower median of the differences).
3. The interval at level `1 − α_f`, with α_f = 0.05 ÷ (the number of primary comparisons in the comparison's family, "Multiplicity"), is `[d(k), d(mn + 1 − k)]`, where k is the largest integer ≥ 1 with `P0(U ≤ k − 1) ≤ α_f ÷ 2` under the no-ties null distribution of U for (m, n): `f(m, n, u) = f(m − 1, n, u − n) + f(m, n − 1, u)`, `f(0, n, 0) = f(m, 0, 0) = 1`, total C(m + n, m).
4. When no k ≥ 1 satisfies step 3 the comparison is `INVALID` with reason `INSUFFICIENT`.

| Primary comparisons in the family | 1 | 2 | 3 | 4 | 5 | 6 | 8 | 10 | 12 |
|---|---|---|---|---|---|---|---|---|---|
| k for m = n = 10 | 24 | 21 | 19 | 18 | 17 | 16 | 15 | 14 | 14 |

For m = n = 10 and a family of one, `P0(U ≤ 23) = 3,996 ÷ 184,756` and the interval is [d(24), d(77)] with coverage 1 − 7,992 ÷ 184,756 (0.957). In the worked example above the shift is −40 ns with interval [−45, −35]; in a family of three, k = 19 and the interval is [−46, −34].

- **Ties.** The p-value of "Rank-sum test with ties" is exact given the tie pattern; the interval uses the no-ties null of U, the continuous-data result. Under ties the two can disagree near the boundary: the fixture `ties` has p = 2,552/184,756 (0.0138) and the interval [−4, 0], which contains 0. The verdict and `significant` follow the interval alone; a comparison where the exact p is below α_f while the interval contains 0 (or the reverse) carries the reason `TIES_DISAGREE`, and the report's "Exclusions and threats to validity" names it.
- **Location model.** The interval assumes a pure location shift between the two series. When the two rCVs differ by more than a factor of 2, evaluated in integers as `max(rCV) > 2 × min(rCV)` with a zero minimum counting as above 2 when the maximum is non-zero, the comparison carries the reason `SPREAD_DIFFERS` and the report lists it under threats to validity (an allocator that changes the tail, not the centre, is the typical case).
- **Power.** For a `lower` metric BETTER needs `d(mn + 1 − k) < −δ`: at n = 10, at least 77 of the 100 pairwise differences below −δ in a family of one and 87 in a family of 12; at n = 20 (k = 128 in a family of one), 273 of 400. A study expecting a small effect against noisy series chooses n = 20.
- **Zero baseline.** The relative shift `relPpm = shift × 10^6 ÷ median(B)`, rounded away from zero, is 0 with the reason `ZERO_MEDIAN` when median(B) is 0; the margin is then δ_abs alone, and the verdict is unaffected.

### Effect size

Cliff's delta, `(#{a > b} − #{a < b}) ÷ mn`, reported as the integer pair; it is −100 ÷ 100 in the worked example. It describes the overlap of the two series and never decides a verdict.

### Multiplicity

- `familyBy` in `study.json` defines the family key of every primary comparison: `study` (default: every primary comparison of the platform is one family), `scenario` (one family per scenario) or `scenario+metric`. Each family holds at most 12 primary comparisons.
- Verdicts use the Bonferroni level α_f of "Shift estimate and its interval" within the family, so each family keeps its family-wise error at 0.05; a ruling's acceptance rule draws its terms from the families its study declares, and the report states the family of every term.
- The exact p-value of every comparison and its Holm-adjusted value within its family (sort ascending, `p'(i) = max_{j ≤ i} min(1, (m − j + 1) × p(j))`) are reported for reading; neither decides a verdict. Worked example: p = 2/184,756, 2,552/184,756 and 1/2 in one family give Holm values 3/92,378, 116/4,199 and 1/2.
- Secondary comparisons use α = 0.05 with no adjustment and are labelled secondary in every table.

### Metric classes and thresholds

| Class | Units | Margin δ_rel | Margin floor δ_abs | rCV warning (`NOISY`) | rCV noisy limit (`NOISY_INVALID`) |
|---|---|---|---|---|---|
| `gpu-time` | ns | 3% (30,000 ppm) | 10,000 ns | > 3% | > 10% |
| `cpu-time` | ns | 3% | 5,000 ns | > 3% | > 10% |
| `bench-time` | ns | 3% | 0 | > 3% | > 10% |
| `throughput` | bytes_per_s, or `count` per second | 3% | 0 | > 3% | > 10% |
| `memory` | bytes | 1% | 65,536 bytes | > 0.5% | > 2% |
| `bandwidth` | bytes | 5% | 1,048,576 bytes | > 5% | > 10% |
| `count` | count | 0 | 0 | any variation is `NONDETERMINISTIC` | — |
| `size` | bytes | 0 | 0 | any variation is `NONDETERMINISTIC` | — |
| `image` | micro | compared with its tolerance row | — | any variation is `NONDETERMINISTIC` | — |
| `verdict` | pass | — | — | — | — |

The margin of a comparison is `δ = max(ceil(δ_rel × median(B) ÷ 10^6), δ_abs)`, computed as `(δ_rel_ppm × median(B) + 999,999) ÷ 10^6` in integer division. A study may override δ_rel for a comparison only with a written justification in `study.json` (D-SEL-HEAP's record threshold); the override and its justification appear in the report's "Method" section.

### Verdicts

With gain g = −shift for a `lower` metric and g = shift for a `higher` metric, and gain interval [g_low, g_high]:

| Verdict | Condition (evaluated in this order) |
|---|---|
| `INVALID` | either series is `INSUFFICIENT` or `NONDETERMINISTIC`, the confound check failed (`CONFOUNDED`), or step 4 of "Shift estimate and its interval" found no k (`INSUFFICIENT`) |
| `INCONCLUSIVE` (reason `DRIFT`) | either series is DRIFT |
| `BETTER` | g_low > δ |
| `WORSE` | g_high < −δ |
| `EQUIVALENT` | −δ ≤ g_low and g_high ≤ δ, and neither series is `NOISY_INVALID` |
| `INCONCLUSIVE` | otherwise; with reason `NOISY` when a series is `NOISY_INVALID`, `UNDERPOWERED` when g_high − g_low > 2δ |

The reasons `TIES_DISAGREE`, `SPREAD_DIFFERS`, `INVALID_RATE` and `ZERO_MEDIAN` annotate a verdict without changing it.

A `NOISY_INVALID` series therefore still yields BETTER or WORSE when its wide interval decides, because the interval already carries the noise; it never yields EQUIVALENT. Worked examples on the fixture baseline B above (median 1001, class `bench-time`, δ = 31): the separated A gives gain [35, 45], `BETTER`; A = B + 2 gives shift interval [−3, 7], gain [−7, 3], `EQUIVALENT`; A = 1040, 960, 1010, 980, 1030, 990, 1000, 1020, 970, 1050 with δ_rel overridden to 1% (δ = 11) gives shift interval [−24, 30], `INCONCLUSIVE` reason `UNDERPOWERED`; A = 1000, 1200, 900, 1100, 800, 1300, 950, 1150, 1050, 700 has rCV 148,260 ppm, `NOISY_INVALID`, and against B gives `INCONCLUSIVE` with reasons `NOISY` and `UNDERPOWERED`.

A verdict also reports `significant` (the interval excludes 0) for reading.

### Deterministic metrics

`count`, `size` and `image` series are expected to repeat exactly: their statistic is the common value, and any two differing valid values make the series `NONDETERMINISTIC`, which is itself a finding the report states (a nondeterministic draw count is a defect, not noise). They are compared by value: `lower`, `equal` and the `meets` operator; image metrics are judged against the per-scene parity tolerance rows of `Zenith2/Docs/Rendering.md` → "Cross-path consistency", never statistically.

## Acceptance rule operators

### Operator table

| Operator | True when | Used for |
|---|---|---|
| `better(c, b, metric[, rel=<percent>])` | the verdict of (c against b) is `BETTER` on every platform the study measures the metric on, with δ_rel replaced by `rel` when given | "improves beyond the margin", "exceeds the record's threshold with 95% confidence" |
| `notWorse(c, b, metric[, rel=…])` | the verdict is `BETTER` or `EQUIVALENT` on every platform | "without raising" |
| `equivalent(c, b, metric[, rel=…])` | `EQUIVALENT` on every platform: the whole gain interval inside ±δ | demonstrated equivalence: "no measurable difference", "within the margin" |
| `medianBelow(c, b, metric, rel=<percent>)` | `100 × median(c) ≤ (100 − rel) × median(b)` on every platform (medians alone, integer arithmetic) | "its median is at least 5% lower" (D-TBDR) |
| `medianAbove(c, b, metric, rel=<percent>)` | `100 × median(c) ≥ (100 + rel) × median(b)` on every platform | the mirror for `higher` metrics |
| `medianAtMost(c, metric, <value>)` | median(c) ≤ value on every platform, the value an integer in the metric's unit | a fixed bound an area's rule states ("median seek time within 5 ms", D-SEL-CODEC) |
| `medianAtLeast(c, metric, <value>)` | median(c) ≥ value on every platform | the mirror |
| `indistinguishable(c, b, metric)` | on every platform the two median intervals overlap: c's interval does not lie entirely on the better side of b's, and b's does not lie entirely on the better side of c's (per platform, the negation of `disjoint` in both directions) | "differs by less than its 95% confidence interval" (D-SEL-NAV's tie); a weaker condition than `equivalent`, which needs the whole gain interval inside ±δ |
| `disjoint(c, b, metric)` | c's median interval lies entirely on the better side of b's median interval on every platform | "its 95% confidence interval and b's do not overlap", "non-overlapping 95% intervals" |
| `beyondSpread(c, b, metric)` | identical to `disjoint` | "beyond the run-to-run variance", "beyond the recorded run-to-run spread" |
| `meets(c, row, metric[, platform=…])` | for an upper-limit row, the upper end of c's median interval ≤ Value + Tolerance of the row; for a lower-limit row (name ending "floor" or value written `≥`, [Z2-N-BUDGET-011]), the lower end ≥ Value − Tolerance; a pending row makes the term `INVALID` | "meets Z2-B-…", "within the budget row", "fits" |
| `misses(c, row, metric)` | the whole interval is beyond the limit | reported breach |
| `lower(c, b, metric)` | median(c) < median(b) for a `lower` metric (> for `higher`), compared on medians alone; equal medians make it false | tie-breakers ("the lower p95 step time", "the higher median parse throughput") |
| `equal(c, b, metric)` | the common values of two deterministic series are equal | "bit-identical", "identical path results" |
| `pass(c, check)` | every case of the named probe or suite check passed for c on every device | RP probes, conformance-suite gates |
| `judgement(<text>)` | the ruling's report records the human judgement and its reasoning | licence reviews, visual-authoring fit; never the only term that differs between two otherwise tied candidates' measured terms |

`meets` on a frame-rate row of the WS-FRAME set uses the row's own statistic per launch (p95 by nearest rank) as the series and is a research reading only; it never replaces the qualification run of [Z2-N-BUDGET-078], which every repetition passes individually. A `meets` term on a row that the same harness run allocates (D-JOLT-PARALLEL-STEP and Z2-B-PHYS-002) is evaluated only after the allocating ruling has written the row's value, so the term is never computed against a pending or circular value ("Instance phrase mapping").

### Rule grammar

```
rule      := ordered | expr
ordered   := "first(" clause { ";" clause } ")"
clause    := expr "->" outcome
expr      := term | "not " expr | expr " and " expr | expr " or " expr | "(" expr ")" | "true" | "@" name
term      := operator "(" args ")"
name      := lower { lower | digit | "-" }                           ; a key of the study's `definitions`, at most 32 characters
operator  := "better" | "notWorse" | "equivalent" | "indistinguishable" | "medianBelow" | "medianAbove" | "medianAtMost" | "medianAtLeast" | "disjoint" | "beyondSpread"
           | "meets" | "misses" | "lower" | "equal" | "pass" | "judgement"
args      := arg { ", " arg }
arg       := token | token "=" token | quoted
outcome   := "adopt(" candidate ")" | "keep(" candidate ")" | "blocked(" quoted ")" | "human(" quoted ")"
```

`and` binds tighter than `or`; `not` tighter than both. `@<name>` expands the expression stored under that key of the study's `definitions` object (a definition may use earlier definitions, never itself or a later one), so an eligibility condition is written once and used in several clauses. A plain `expr` rule is accepted only when the study has exactly two alternatives and exactly one is the incumbent; it is read as `first(expr -> adopt(<the other alternative>); true -> <outcomeWhenFalse>)`. Any other study with a plain rule is refused (`RESEARCH_ERROR_RULE`), so a study with no incumbent always writes an ordered rule.

### Rule evaluation

The rule is evaluated once per study ruling, over the runs the ruling names, by `Invoke-Zenith2ResearchStudyRule` (`Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Analysis and report functions"):

1. Evaluate every term once per platform into `true`, `false` or `INVALID`. A term's comparison is looked up by id in the one named run whose platform and session contain its two cells (the `sessions` partition guarantees there is at most one); a term over an `INVALID` comparison, a pending row, a `notApplicable` cell or a comparison present in none of the named runs is `INVALID`.
2. Combine with three-valued logic: `false and INVALID = false`, `true or INVALID = true`, every other combination with `INVALID` is `INVALID`.
3. For an `ordered` rule, take the first clause whose expression is `true`; a clause evaluating to `INVALID` before any `true` clause stops evaluation with outcome `blocked(<the INVALID terms>)`.
4. The study-level outcome and every term's value are written into the report's generated `analysis` block, which the check regenerates from the named runs (`RESEARCH_CHECK_REPORT_STALE`), and the DecisionLog entry's **Analysis** field quotes that outcome. A run's own `analysis.json` holds `rule.terms` for the terms it can evaluate alone and `rule.outcome: null` with `rule.reasons: ["PARTIAL"]` whenever the study's rule needs more than that run; a single-run study writes its outcome there too.

Complexity linear in the number of terms. A `blocked` outcome keeps the ruling ticket Blocked naming the terms, which is the D-SEL-2DPHYS shape of ruling Q91 (a).

### Instance phrase mapping

| Phrase in an open decision's acceptance rule | Operator |
|---|---|
| "beyond the run-to-run variance (95% confidence intervals, at least 10 repetitions)" (D-SEL-HEAP) | `beyondSpread` |
| "exceeds the justification record's threshold with 95% confidence" (D-SEL-HEAP in `Zenith2/Docs/Architecture.md` → "Named selection decisions") | `better(…, rel=<threshold>)` |
| "its median main-view GPU frame time is at least 5% lower than (1)'s; its 95% confidence interval and (1)'s do not overlap" (D-TBDR) | `medianBelow(…, rel=5) and disjoint(…)` |
| "beyond run-to-run variance (non-overlapping 95% intervals)" (the shading-path ruling) | `disjoint` |
| "the lower … time beyond the recorded run-to-run spread wins" (D-SEL-TEXT, D-SEL-UIRT step 3) | `beyondSpread` |
| "meets `Z2-B-…`", "within `Z2-B-…`", "fits `Z2-B-…`" | `meets` |
| "at equal or lower `Z2-B-LIGHT-010` time" (D-SEL-UPSCALER) | `notWorse` on the GPU time series |
| "the fastest candidate", "the higher median throughput", "the lower p95 step time" (D-SEL-HASH, D-SEL-JSON, D-SEL-2DPHYS's breach fallback) | `lower` (medians alone, no margin) |
| "differs by less than its 95% confidence interval" (D-SEL-NAV's tie) | `indistinguishable` |
| "median seek time within 5 ms" (D-SEL-CODEC) | `medianAtMost(…, seek-time, 5000000)` |
| "bit-identical", "identical path results for identical inputs" | `equal` on the digest series |
| "passes the … conformance suite", "every case passes" | `pass` |
| "the human decides" | outcome `human(<text>)` |

A phrase not in this table is translated in the study's `rule` field with a `ruleNote` explaining the translation, and the reviewer of the harness ticket checks it. The rule cells of `Zenith2/Docs/Design/RESEARCH/Instances.md` are documentation of the translation; the parsed rule is the study's.

## Revalidation triggers

A research conclusion holds for the inputs it was measured on. Every research DecisionLog entry lists the triggers that reopen it (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields"), and the revalidation scan evaluates them.

### Trigger vocabulary

| Trigger | Fires when (compared with the inputs recorded in the entry's runs) | Implied for |
|---|---|---|
| `toolchain` | the manifest's toolchain hash (SHIP's `Get-Zenith2ManifestHashes` `toolchainHash`) differs | RA, RS, RJ, RB, RP |
| `hardware` | the registered reference machine of a platform the runs measured differs from the run's `machineId` (`Zenith2/Docs/Research/Machines/current.json`) | RA, RS, RJ, RB, RP |
| `dependency:<entry>` | the manifest entry's version or source digest differs | — |
| `driver:<desktop-gpu|s24-gpu>` | the major version of the GPU driver recorded in `Machines/current.json` differs | — |
| `os:<windows|android>` | the OS build family (Windows: the `CurrentBuild` value; Android: `ro.build.version.release`) recorded in `current.json` differs | — |
| `workload:<W>` | the workload's version differs | — |
| `row:<Z2-B-ID>` | the row's value or tolerance in `Zenith2/Docs/Budgets.md` → "Row index" differs | — |
| `target:<target>` | any file under the named target's directory changed between the runs' tree and the scanned tree (`git diff --name-only <runTree> HEAD -- <dir>`) | — |
| `stage:<N>` | the scan runs in the acceptance of stage N | — |
| `margin` | at the next stage acceptance after the ruling, when any primary verdict's gain interval came within δ of its decision boundary (BETTER with g_low ≤ 2δ, WORSE with g_high ≥ −2δ, EQUIVALENT with g_low < −δ ÷ 2 or g_high > δ ÷ 2) | — |

- RA, RS and RJ entries list at least one trigger beyond the implied ones; an RQ entry lists at least `stage:<N>` for the stage whose acceptance re-reads it, or the token `none` with a one-sentence reason.

### Trigger evaluation

`Get-Zenith2ResearchRevalidation` (`Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Revalidation scan") reads every DecisionLog entry carrying **Revalidation:** whose status is `Settled`, evaluates each trigger against the checkout without building it, and writes `Zenith2/out/<checkout-hash>/research/revalidation.json`. A fired trigger is handled when `Zenith2/Docs/Research/<Study>/revalidations.json` holds an entry with the same (decision, trigger, observed value); `-Final`, which every stage acceptance runs, exits 1 while any fired trigger is unhandled.

### Research regression rule

A fired trigger is handled by a `needs-human` revalidation run on the reference hardware under the same study version, compared with the original run by `Compare-Zenith2ResearchRuns`:

| Outcome of the comparison | Handling |
|---|---|
| every primary verdict unchanged and every primary shift within `max(δ, half the original gain interval width)` of the original | `stands`: one `revalidations.json` entry, the new run committed, no DecisionLog entry |
| every primary verdict unchanged, some shift moved beyond that bound | `stands-drifted`: as `stands`, plus the drift recorded in the entry and stated in the next report of the study |
| any primary verdict changed (any change, including INCONCLUSIVE to BETTER) or the rule's outcome changed | `regressed`: a re-ruling ticket is filed (`Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Regression filing"), the entry records it, and the conclusion stands until the re-ruling appends its own DecisionLog entry, which supersedes the old one through its **Decision** text |

A research regression is not a budget regression: a budget row breached in a qualification run is handled by `Zenith2/Docs/Budgets.md` → "Breach consequences" alone ([Z2-N-BUDGET-039], [Z2-N-BUDGET-040]).

## Procedure parameter list

Every number this file sets, listed once for cross-document item X416:

| Parameter | Value |
|---|---|
| launches per series | 10 to 20, fixed per study |
| warm-up | one launch per cell in slot group 0 |
| retries per (cell, repetition) | 3, each one slot group later |
| session cap | 12 hours; cooldown allowance in the plan 300 s (desktop), 600 s (S24) per slot |
| desktop pre-session idle | 300 s after sign-in; boot age 300 s to 3 hours |
| S24 pre-session idle | 120 s after boot completed |
| primary comparisons per family | at most 12 |
| median definition | lower median, rank (n + 1) ÷ 2 |
| family-wise level | 0.05, Bonferroni within the family |
| drift level | exact two-sided p ≤ 0.01 |
| spread ratio for `SPREAD_DIFFERS` | rCV ratio above 2 |
| margins and rCV limits | the table of "Metric classes and thresholds" |
| desktop power state | 99% minimum and maximum, `% Processor Performance` ≤ 100 |
| desktop clock tolerance | core within 15 MHz, memory within 50 MHz |
| desktop idle limits | CPU mean ≤ 3% over 30 s, GPU ≤ 2%, no foreign process > 1% |
| desktop GPU temperature at slot start | ≤ 50 °C, wait ≤ 300 s |
| desktop ambient | 20 °C to 26 °C |
| S24 battery at session start | level 50 to 100, temperature ≤ 33.0 °C |
| S24 slot start | thermal status 0, battery temperature ≤ 33.0 °C, wait ≤ 600 s; level ≥ 40 |
| S24 slot end | thermal status ≤ 1 |
| S24 without fixed-performance mode | ≤ 31.0 °C, wait ≤ 900 s, end status 0 |
| S24 ambient | 23 °C to 27 °C |
| S24 background load after kill-all | < 5%, the second of two reports 10 s apart |
