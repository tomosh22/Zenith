# RESEARCH design: test catalogue

This file is part of the RESEARCH design family; the entry document is `Zenith2/Docs/Design/RESEARCH.md`. It names every test of the RESEARCH tooling and the case it asserts. Every case is a `-Case <Name>` of `Zenith2/Benchmarks/Research/Tests/Zenith2Research.Tests.ps1`, registered once as the CTest `Pwsh_Research_<Name>` with `zenith_register_pwsh_test` in the RESEARCH block of `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` (`Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Registration in the test registry"), so every one runs in the `Test` step on the `win` preset. Test names are unique programme-wide through the `Pwsh_Research_` prefix.

## Scope of this file

- Every expected value is derivable from the specification by hand (`Zenith2/Docs/Design/RESEARCH/Methodology.md` gives the worked values) or from an independent oracle computed inside the test (SHA-256 through `[System.Security.Cryptography.SHA256]::HashData`), never recorded from a run.
- Fixtures are text files under `Zenith2/Benchmarks/Research/Tests/Fixtures/`; a test that needs a directory tree or a git repository builds it at test time under `$env:TEMP/zenith2-research-tests/<case>/` and deletes it afterwards.
- The runner cases run the real `Run-Research.ps1` with `ZENITH_RESEARCH_COMMAND_SHIM` pointing at a fixture shim (`Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Command shim"), so no hardware, `adb` or `nvidia-smi` is touched and every simulated wait takes no real time; each runner case finishes well inside the 120 s CTest timeout.

## Fixture sets

| Fixture | Content |
|---|---|
| `Fixtures/Series/baseline-b.csv` | B = 1003, 998, 1010, 1001, 995, 1007, 1000, 1004, 999, 1006 in repetition order, class `bench-time`, unit `ns`, direction `lower` |
| `Fixtures/Series/separated-a.csv` | A = 962, 958, 970, 955, 966, 960, 964, 957, 968, 961 |
| `Fixtures/Series/shifted-a.csv` | A = B + 2, element by element |
| `Fixtures/Series/wide-a.csv` | A = 1040, 960, 1010, 980, 1030, 990, 1000, 1020, 970, 1050 |
| `Fixtures/Series/noisy-a.csv` | A = 1000, 1200, 900, 1100, 800, 1300, 950, 1150, 1050, 700 |
| `Fixtures/Series/ties-a.csv`, `ties-b.csv` | A = 5, 5, 6, 6, 7, 7, 8, 8, 9, 9; B = 7, 7, 8, 8, 9, 9, 10, 10, 11, 11 (class `count` is not used for them: they are `bench-time` series whose ties come from coarse values) |
| `Fixtures/Studies/Synthetic/` | a complete committed study (`study.json`, one run directory with `record.json`, the series above, an `archive/` holding one LFS pointer file and one plain object, `analysis.json` written by hand from the worked values, one report `Reports/Z2-D-NNN.md`) |
| `Fixtures/Studies/Broken/<Code>/` | one study per check finding code, each breaking exactly one rule |
| `Fixtures/Tickets/Draft/` and `Fixtures/Tickets/Board/` | one valid ticket per profile, one `budget`, one `revalidation` and one `run`-role ticket, and one ticket per `RESEARCH_TICKET_*` code, in the drafts' front-matter form and in the board's managed-trailer form |
| `Fixtures/DecisionLog/` | a DecisionLog fragment with valid and broken research entries, and a matching open-decision list |
| `Fixtures/Revalidation/` | a manifest pair (`before.json`, `after.json` differing in one dependency version), a `Machines/current.json` pair, a `revalidations.json` |
| `Fixtures/Json/` | `input.json` (a document with duplicate-free members, a BigInteger lexeme, a fractional lexeme, an ISO-8601 string), `duplicate.json` (a repeated member), `case-keys.json` (`{"A":1,"a":2}`), `bom.json` (`input.json` with a leading `EF BB BF`), `depth64.json` and `depth65.json` (arrays nested 64 and 65 deep), `expected.json` (the canonical rendering of the synthetic analysis input, including a member array of compact objects that fits 160 characters and one that does not) |
| `Fixtures/Studies/MultiRun/` | a two-platform study with one Win run and one S24 run, and a one-platform study split into two sessions with one run each, every `analysis.json` written by hand |
| `Fixtures/Harness/` | harness outputs the shim's `writes` copies to a launch's `{out}` path (`heap-r<NN>.json` with the values of `baseline-b.csv`, a per-frame CSV, a PNG, an over-cap text file) |
| `Fixtures/Stubs/` | `tree-clean.json`, `tree-dirty.json` and `fingerprint.json`, the objects the shim's `stubs` map returns for `Get-Zenith2TreeSha` and `Get-Zenith2EvidenceFingerprint` |
| `Fixtures/Shims/<Case>.json` | one command-shim script per runner case |

## Statistics cases

| Case | Asserts |
|---|---|
| `MedianLowerRank` | the median of 5, 1, 4, 2, 3, 6 is 3 (rank 3 of 6) and of 5, 1, 4, 2, 3 is 3 |
| `PercentileIntegerRank` | p95 of 1 … 20 is 19 (rank (95 × 20 + 99) ÷ 100 = 19) and p99 is 20; p95 of 1 … 7,200 is 6,840 |
| `MedianIntervalN10` | baseline B: median 1001, interval [998, 1007], j 2, k 9, coverage 501/512 |
| `MedianIntervalTable` | the tool's j and k for n = 10 … 20 equal the table of `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Median and its interval"; n = 6 gives j 1, k 6; n = 5 and n = 21 throw |
| `RobustCvFixture` | B: MAD 3, rCV 4,443 ppm, state `OK`; noisy A: rCV 148,260 ppm, state `NOISY_INVALID` for class `bench-time`; an all-zero series rCV 0 |
| `DriftMonotonic` | 100 … 109 in repetition order: 0 inversions, p 2/3,628,800, drift true |
| `DriftMixed` | 100, 104, 101, 108, 103, 106, 102, 109, 105, 107: 13 inversions, p 393,048/3,628,800, drift false |
| `DriftBoundaryN10` | 4, 3, 2, 1, 6, 5, 7, 9, 8, 10 (8 inversions): p 33,198/3,628,800, drift true; 4, 3, 2, 1, 6, 5, 8, 7, 10, 9 (9 inversions): drift false |
| `DriftTiesRoundTowardMean` | a series with one tie has its doubled inversion count rounded toward the mean, never away |
| `DriftOrdersByRepetition` | a series whose repetition 3 was retried last in slot order is ordered by repetition: the retried value takes place 3 and the inversion count is that of the repetition order |
| `RankSumSeparated` | separated A against B: W2 110, U2 0, p 2/184,756 |
| `RankSumTies` | ties A against ties B: W2 146, U2 36, p 2,552/184,756 |
| `ShiftSeparated` | separated A against B: shift −40, interval [−45, −35], k 24; in a family of 3: k 19, interval [−46, −34] |
| `ShiftTies` | ties A against ties B: shift −2, interval [−4, 0] |
| `TiesDisagreeReason` | ties A against ties B in a family of 1: p 2,552/184,756 below 0.05, interval containing 0, so `significant` false and the reason `TIES_DISAGREE` |
| `SpreadDiffersReason` | noisy A (148,260 ppm) against B (4,443 ppm): the reason `SPREAD_DIFFERS` |
| `ShiftKTable` | k for m = n = 10 at families 1, 2, 3, 4, 5, 6, 8, 10, 12 equals 24, 21, 19, 18, 17, 16, 15, 14, 14; family 13 is refused |
| `FamilyByScenario` | a study with `familyBy: "scenario"`, two scenarios of 12 and 3 primary comparisons: the first family uses k = 14, the second k = 19, and Holm values are computed within each family |
| `CliffDelta` | separated: −100/100; ties: −64/100 |
| `HolmAdjustment` | p = 2/184,756, 2,552/184,756, 1/2 in one family give 3/92,378, 116/4,199, 1/2 |
| `MarginClasses` | `bench-time` median 1001 gives δ 31; `gpu-time` median 100,000 gives 10,000 (the floor); `memory` median 10^8 gives 1,000,000; `bandwidth` median 10^6 gives 1,048,576 (the floor); a δ_rel override of 1% on median 1001 gives 11 |
| `VerdictBetter` | separated A against B: gain [35, 45], δ 31, `BETTER`, significant |
| `VerdictEquivalent` | shifted A against B: shift interval [−3, 7], `EQUIVALENT` |
| `VerdictUnderpowered` | wide A against B with the 1% override: shift interval [−24, 30], `INCONCLUSIVE` with reason `UNDERPOWERED` |
| `VerdictNoisy` | noisy A against B: shift interval [−104, 150], `INCONCLUSIVE` with reasons `NOISY` and `UNDERPOWERED`; noisy A shifted by −400 against B is still `BETTER` (a noisy series is not discarded) |
| `VerdictDriftDowngrade` | separated A reordered to drift: `INCONCLUSIVE` with reason `DRIFT` |
| `VerdictHigherIsBetter` | the separated pair declared `higher`: `WORSE` |
| `VerdictConfounded` | two cells whose `workloadDefinition` inputs differ under factor `candidate`: `INVALID` with reason `CONFOUNDED` naming the member |
| `VerdictApiCrossHarnessAllowed` | a native cell (`Zenith_Bench_ECSNative`) and an adapter cell (`Zenith_Bench_ECS`) with equal `toolchain` and `thirdParty` and different `closure`, `harness`, `launchTarget` and command, the pair listed in `comparesHarnesses`: not `CONFOUNDED`, `crossHarness` true |
| `VerdictApiCrossHarnessStillConfounded` | the same pair with different `workloadDefinition`: `CONFOUNDED` naming it; the same pair not listed in `comparesHarnesses`: `CONFOUNDED` naming `launchTarget` |
| `DeterministicCount` | a `count` series 7, 7, 7, 8, 7, 7, 7, 7, 7, 7 is `NONDETERMINISTIC`; ten 7s compare `equal` to ten 7s |
| `ArithmeticNoFloat` | analysing the synthetic study, no number in `analysis.json` contains `.`, `e` or `E` |
| `ArithmeticBeyond2Pow53` | the ppm of 68,719,476,736 × 10^6 ÷ 3 is 22,906,492,245,333,333 exactly, and a median of values near 2^62 is exact |
| `SplitMix64Reference` | seed 0 gives 0xE220A8397B1DCDAF, 0x6E789E6AA1B965F4, 0x06C45D188009454F, the published reference outputs |
| `ScheduleExpectedPermutations` | groups for r = 0 … 4: seed 0, cells A, B, C: CAB, CAB, CAB, BAC, BAC; seed 1: ABC, CBA, BCA, CBA, BCA; seed 11400714819323198485: CBA, ACB, CBA, ABC, ABC; the same seed with A, B, C, D: CDBA, CBAD, BDCA, ACDB, ABCD |
| `ScheduleRetryPlacement` | a retry of repetition 2 runs in group 3, after group 3's own slots, and a retry of the last group runs immediately after it |
| `InvalidRateReason` | a cell with 3 invalid of 13 executed slots (`5 × 3 > 13`) against one with 3 of 13 carries `INVALID_RATE`; 2 of 12 (16.7%) against 0 of 10 differ by more than 10 points and carry it; 1 of 11 (9.1%) against 0 of 10 carry none; the verdict is unchanged in each |
| `RelativeShiftZeroBaseline` | a baseline series of ten 0s against a candidate of ten 5s (class `bench-time`): `relPpm` 0 with reason `ZERO_MEDIAN`, δ 0, and the verdict computed from the shift interval alone |
| `SpreadDiffersIntegerRule` | rCVs 9,000 and 4,500 ppm do not carry `SPREAD_DIFFERS` (`9,000 > 9,000` is false); 9,001 and 4,500 do; 1 and 0 do; 0 and 0 do not |

## Rule cases

| Case | Asserts |
|---|---|
| `RuleGrammarParse` | each operator of "Operator table" parses with its argument forms, `medianBelow`, `medianAbove`, `medianAtMost`, `medianAtLeast` and `indistinguishable` included, and `@<name>` parses as a reference; `better(a, b)` (a missing metric) and `first(x -> adopt(a)` (unclosed) fail with `RESEARCH_ERROR_RULE` naming the column |
| `RuleThreeValued` | `false and INVALID` is false, `true or INVALID` is true, `true and INVALID` is INVALID, `not INVALID` is INVALID |
| `RuleOrderedBlocked` | `first(x -> adopt(a); true -> keep(b))` with x INVALID gives `blocked` naming x; with x false gives `keep(b)` |
| `RuleMeetsUpperAndFloor` | an upper-limit row 1,000 with tolerance 10 is met by an interval [950, 1,010] and not by [950, 1,011]; a floor row 500 is met by [500, 600] and not by [499, 600]; a pending row makes the term INVALID |
| `RuleDisjointAndBeyondSpread` | separated A and B are disjoint on a `lower` metric; shifted A and B are not; `beyondSpread` returns the same values |
| `RuleMedianBelow` | candidate median 950 against baseline 1001 with rel 5: true (100 × 950 = 95,000 ≤ 95 × 1001 = 95,095); candidate 951: false (95,100 > 95,095); `medianAtMost(c, seek-time, 5000000)` true for a median of 5,000,000 and false for 5,000,001 |
| `RuleEveryPlatform` | `better` on a two-platform study is true only when both platforms are `BETTER` |
| `RuleAcrossRuns` | over the `MultiRun` two-platform study, `Invoke-Zenith2ResearchStudyRule` with both runs gives `adopt(<candidate>)` when both platforms are `BETTER` and the false outcome when the S24 run's comparison is `EQUIVALENT`; each run's own `analysis.json` holds `rule.outcome` null with reason `PARTIAL`; with the S24 run omitted from `-Runs` the S24 terms are INVALID and the outcome is `blocked` naming them; two runs of the same platform and session are refused |
| `RuleAcrossSessions` | over the `MultiRun` two-session study, a rule `medianBelow(…) and disjoint(…)` on two scenarios, one per session, is evaluated from the two runs together and equals the outcome computed by hand |
| `RuleIndistinguishable` | separated A against B is not `indistinguishable` (the intervals [957, 968] and [998, 1007] do not overlap); shifted A against B is; wide A against B is `indistinguishable` and not `equivalent` under the 1% override; a two-platform study is `indistinguishable` only when the intervals overlap on both platforms |
| `RuleDefinitions` | `@eligible-a` expands the study's `definitions` entry; a definition naming a later or its own definition, and an unknown name, fail with `RESEARCH_ERROR_RULE` |
| `RuleSoleEligibleAdopted` | the D-SEL-NAV rule shape of `Zenith2/Docs/Design/RESEARCH/Instances.md` with fixture terms: only Recast eligible gives `adopt(recast)` even when custom's maintenance numbers are lower; only custom eligible gives `adopt(custom)`; both eligible and indistinguishable on all four shares gives `adopt(recast)`; neither eligible gives `blocked` |
| `RuleMeetsFrameRateReadingOnly` | a `meets` term on a WS-FRAME row is evaluated from the per-launch p95 series of the run's cells (each launch's own p95 by nearest rank) and needs no qualification record; the analysis reads none and the run writes none (the run directory holds nothing under `Zenith2/Docs/Research/Budgets/`) |
| `StudyNoIncumbentRequiresHuman` | a study with no incumbent and `outcomeWhenFalse: "human(…)"` passes; with an ordered rule ending `true -> blocked(…)` passes |
| `StudyNoIncumbentWithKeepRefused` | a study with no incumbent and `outcomeWhenFalse: "keep(yyjson)"` fails; two incumbents fail; a plain (unordered) rule in a two-alternative study with no incumbent fails `RESEARCH_ERROR_RULE`, and in a three-alternative study with one incumbent fails too |

## Format cases

| Case | Asserts |
|---|---|
| `CanonicalJsonFixture` | `ConvertTo-Zenith2CanonicalJson` of the parsed `Fixtures/Json/input.json` equals `Fixtures/Json/expected.json` byte for byte (member order, tabs, compact objects, arrays, escapes, final LF) |
| `CanonicalJsonRefusesFloat` | a `[double]`, a `[single]` and a `[decimal]` each throw a programmer error |
| `CanonicalRationalRules` | a p-value 2/184,756 is written unreduced; a Holm value 4/184,756 is written 1/46,189; a coverage 1002/1024 is written 501/512 |
| `CanonicalSortOrdinal` | identity keys `_x`, `Board`, `board`, `gpu`, `GPU`, `gpuMemMiB`, `uid` hash in the order `Board, GPU, _x, board, gpu, gpuMemMiB, uid`, and the machineId equals the test's own SHA-256 of those lines |
| `JsonReaderRules` | `Read-Zenith2ResearchJson` on `input.json` returns the BigInteger 11400714819323198485, the lexeme of `15.900` converted to 15,900,000 ns as a `ms` value, and the ISO-8601 member as a `[string]`; `duplicate.json` is refused with `RESEARCH_ERROR_SCHEMA`; `case-keys.json` reads as two members `A` and `a`, not a duplicate; `Get-Zenith2ResearchExtractedValue` on `bom.json` as a harness output reads the same values as `input.json`, while `bom.json` committed in a run fails `RESEARCH_CHECK_NOT_TEXT`; `depth64.json` parses and `depth65.json` is `EXTRACT` |
| `PwshRulesProbe` | `Tests/Probes/Probe-PwshRules.ps1` exits 0 and prints `P1 OK` … `P10 OK`: among them `Parse` of a `byte[]` refused as an ambiguous overload and of `ReadOnlyMemory[byte]` accepted, the BOM and depth-65 refusals, `[ordered]@{}` case-insensitive and `OrderedDictionary::new()` case-sensitive, a `BigInteger` `/` returning a truncated `BigInteger`, `[Array]::Sort` on a `BigInteger[]`, ordinal string order, `Set-Content`'s bytes against `WriteAllText`'s, and a CR LF line split by `\r?\n` |
| `CanonicalJsonArrayLine` | an array of compact objects whose one-line form is exactly 160 characters is written on one line, and at 161 characters one element per line; a member object holding such an array is expanded |
| `WriterNoBomLf` | every file the module writes in the synthetic run starts with no `EF BB BF`, contains no `0D` byte and ends with `0A` |
| `ModuleStaticRules` | the parser walk of `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "PowerShell rules of the module" finds none of the forbidden commands, aliases, redirections, `[math]` or `psobject` in the three scripts; fixture scripts using `.PSObject.Properties`, `sort`, `gc` and `> out.txt` are each reported, and a string containing `"Sort-Object"` is not |
| `StudyValidSynthetic` | the synthetic `study.json` passes |
| `StudyInvalidCases` | each `Fixtures/Studies/Broken/Study*` fails with exactly its code: a 13th primary comparison in one family, a launch count of 9 and of 21, a comparison whose cells differ outside the factor, an unknown metric class, an override without justification, a rule parse error, a `bench-host` target `Zenith_Bench_<Name>` whose `<Name>` is absent from the fixture `declared_deps.json`, a `comparesHarnesses` entry without justification, a `sessions` list that splits a comparison, a session plan over 12 hours, an unknown schema version |
| `RecordValidSynthetic` | the synthetic run passes `Test-Zenith2ResearchRecord`, with every desktop condition field present |
| `RecordInvalidCases` | broken runs fail with `RESEARCH_CHECK_SHA_MISMATCH`, `RESEARCH_CHECK_MISSING_FILE`, `RESEARCH_CHECK_STRAY_FILE`, `RESEARCH_CHECK_NOT_TEXT` (a NUL byte; a CRLF line; a `.png` file outside `archive/`), `RESEARCH_CHECK_CAP` (with the caps set by the test's parameter to 1 KiB and 4 KiB), `RESEARCH_CHECK_STUDY_DRIFT`; an RS record with `conditions.class` `ordinary` fails `RESEARCH_CHECK_SCHEMA` |
| `RecordEvidenceInvalid` | a workload cell whose evidence record reads `result: "invalid"` gives a slot row with reason `EVIDENCE` |
| `SeriesHeaderRules` | a wrong first line, a pair out of order and a negative value each fail; a warm-up row with `valid` 1 fails; a repetition appearing twice with `valid` 1 fails |
| `TraceConversion` | `16.712` becomes 16,712,000 ns, `0.000` becomes 0, `16.7125` becomes 16,712,500 ns; `16.7125001` (seven decimals) is `EXTRACT`; excluded rows are not written |
| `RawCrlfNormalised` | a harness JSON written with `\r\n` and no final newline is committed with `\n` and a final newline, `sha256` of the committed bytes and `sourceSha256` of the original; an LF original has `sourceSha256` null; a binary output is copied unchanged |
| `ExtractionSelectors` | a `harness-json` selector with one match returns its integer; zero and two matches are `EXTRACT`; an exponent lexeme is `EXTRACT`; `harness-json-array` reduces the launch's array with the metric's statistic to one value; `evidence-row` converts `15.9` ms to 15,900,000 ns; `frames-csv` takes the statistic of the non-excluded rows; `probe-case` maps passed and failed to 1 and 0 |
| `LargeFileNotPointer` | in a temporary repository with the `zenith-research` attributes, a file under `archive/` staged as a plain blob (the test writes the blob with `git hash-object -w` and `git update-index`, needing no git-lfs) fails `RESEARCH_CHECK_NOT_LFS` with `-Staged`; a staged pointer blob with the entry's oid and size passes; an `archive/` path whose attributes are removed fails `RESEARCH_CHECK_NOT_LFS` |
| `LfsConfigPresent` | in a temporary repository holding one committed run, a missing `.lfsconfig`, an `.lfsconfig` without the research `fetchexclude`, and a `Zenith2/.gitattributes` without the `zenith-research` block each fail `RESEARCH_CHECK_LFS_CONFIG`; with both present it passes; with no run committed the check does not apply |
| `PathLengthCap` | a study path of 65 characters and a series file whose repo-relative path is 201 characters each fail `RESEARCH_CHECK_PATH_LENGTH`; 64 and 200 pass |
| `TextRuleScope` | a `junit.xml` under `Zenith2/Docs/Research/LinuxTsanHost/<utc>/` (outside any study) passes the committed check; the same file inside a study directory fails `RESEARCH_CHECK_NOT_TEXT`; a stray file outside every run directory is not `RESEARCH_CHECK_STRAY_FILE` |
| `LargeFilePointerAndObject` | in the synthetic `archive/`, the pointer file's oid and size and the plain object's SHA-256 and length both match their `archive[]` entries; a pointer with a different oid gives `RESEARCH_CHECK_SHA_MISMATCH`; an `archive/` file without an entry gives `RESEARCH_CHECK_STRAY_FILE` |
| `StorageMeasurement` | `Get-Zenith2ResearchStudyStorage` of the synthetic study returns the non-LFS bytes per run and the LFS bytes summed from `archive[]`, and the check compares them with the provisional constants |
| `AnalysisByteStable` | `Invoke-Zenith2ResearchAnalysis` on the synthetic run equals the hand-written `analysis.json` byte for byte, twice in a row |
| `AnalysisStaleDetected` | after one changed series value, `-Check` reports `RESEARCH_CHECK_ANALYSIS_STALE` |
| `ReportHeadingsAndBlocks` | the synthetic report passes; a report with a missing `## Method`, one with the headings reordered, one with an edited number inside a generated block, one with a remaining `<!-- research:write -->` marker, an RA shading report without `## Shading path report sections` and an RP report without `## Probe results` fail with their codes |
| `ReportFromTemplate` | `New-Zenith2ResearchReport` on the synthetic run writes every heading of the template in order, the generated blocks filled and one write marker per written section; over the `MultiRun` two-platform study its `analysis` block holds the study-level outcome of `RuleAcrossRuns` |
| `RevalidationsSchema` | an appended entry with an unknown outcome fails; entries are read in order |

## Check cases

| Case | Asserts |
|---|---|
| `CheckCommittedFixtures` | `Invoke-ResearchCheck.ps1 -Mode committed` over `Fixtures/Studies/` reports every broken study with its code and exits 1; over the synthetic study alone exits 0 |
| `CheckCommitted` | `-Mode committed` over the real `Zenith2/Docs/Research/` exits 0 (with no study yet committed it checks `Index.md` and `Machines/` alone) |
| `CheckCurrentInputsInfo` | in a temporary repository, a run whose recorded `closure` differs from the current one while `git diff` lists a changed closure file gives `INFO RESEARCH_INPUTS_CHANGED` and exit 0; with no changed file it gives `RESEARCH_CHECK_INPUTS` and exit 1 |
| `StudyFilesValid` | every committed `Zenith2/Docs/Research/**/study.json` passes `Test-Zenith2ResearchStudy` |
| `DecisionTicketValidPerProfile` | each valid fixture ticket of profiles RA, RS, RJ, RB, RP and RQ, the `budget` ticket, the `revalidation` ticket and the `run`-role ticket pass in draft and in board mode |
| `DecisionTicketFailures` | each failure fixture fails with exactly its code: a missing `Research-Profile:` line, lines out of order, a `Study:` path no blocker creates, a DoD without the check item, an unknown trigger token |
| `DecisionTicketNeedsHuman` | an `implementation`, a `budget` and a `revalidation` ticket whose files-edited line, `Outputs:` line or a "committed" DoD item names `Zenith2/Docs/Research/HeapSelection/runs/` without `needs-human` each fail `RESEARCH_TICKET_NEEDS_HUMAN`; the same with `Machines/` fails too |
| `DecisionTicketHarnessNamesRuns` | a `kind: harness` ticket with a `runs/` path among its committed outputs fails `RESEARCH_TICKET_WORKER_EVIDENCE` even with `needs-human` |
| `DecisionTicketNamesRunsAsInput` | an `implementation` ticket whose Implementation Contract reads `Zenith2/Docs/Research/ECS/runs/<RunId>/raw/` and whose files-edited line names no research path passes; a `kind: harness` ticket whose Goal says where the ruling's run will land passes |
| `DecisionTicketNonDecisionPasses` | an `implementation` ticket without template lines that names no research path passes |
| `DecisionLogFields` | valid research entries pass; an entry whose **Analysis** outcome differs from the one `Invoke-Zenith2ResearchStudyRule` regenerates over its runs, an entry whose **Analysis** verdict disagrees with its run's analysis, an entry missing **Revalidation**, an entry naming a missing report, and a `Ruled as` open decision whose entry lacks **Research:** each fail with `RESEARCH_CHECK_DECISION_FIELDS` or `RESEARCH_CHECK_TRIGGER` |
| `CheckDecisionLog` | `-Mode decisionlog` over the real `Zenith2/Docs/DecisionLog.md` exits 0 |
| `InstanceMapFixture` | a fixture open-decision list with one decision missing from the fixture map, and a map row naming no decision, each fail with `RESEARCH_CHECK_INSTANCE_MAP` |
| `CheckInstanceMap` | `-Mode instances` over the real DecisionLog and `Zenith2/Docs/Design/RESEARCH/Instances.md` exits 0 |

## Runner cases

Each case is one shim fixture; "passes" means the session reaches its first slot (or completes, where stated) and the shim log shows the check's commands with their simulated times. A case that completes a slot scripts the launch with a `writes` response copying a `Fixtures/Harness/` output to `{out}` (and, on the S24, the `adb pull` and `adb shell stat` keys with the `{out}` placeholder); `Get-Zenith2TreeSha` and `Get-Zenith2EvidenceFingerprint` come from the `stubs` map in every case, and operator input from the `operator` key.

| Case | Check | Asserts |
|---|---|---|
| `RunnerDryRunSchedule` | — | `-DryRun` with the synthetic study prints the schedule of `ScheduleExpectedPermutations` and the session plan in hours, launches nothing (the shim log holds no launch) and changes no setting |
| `RunnerSessionPlanCap` | plan | a study whose plan is 12.5 hours exits 2 with `RESEARCH_ERROR_SESSION_CAP`; with `-Session 0` of a two-session split it passes |
| `RunnerDirtyTreeRefused` | D8 | a dirty temporary repository exits 2 with `RESEARCH_ERROR_DIRTY_TREE` |
| `RunnerNotBuiltRefused` | D8 | a missing harness executable exits 2 with `RESEARCH_ERROR_NOT_BUILT` |
| `RunnerSlotBusy` | lock | `build_lock.ps1 -WaitMinutes 0` exiting 11 (the slot held) makes the runner exit 3 with `RESEARCH_ERROR_SLOT_BUSY`; the wrapped runner's own exit 1 passes through as 1; an S24 with a non-empty `debug.zenith2.research` property exits 3 |
| `RunnerLfsMissingRefused` | LFS | `git lfs version` exiting 1, an empty `filter.lfs.clean`, `filter.lfs.required` unset, and `check-attr` printing `filter: unspecified` for the run's `archive/probe.bin` each exit 2 with `RESEARCH_ERROR_NOT_LFS`; all four as required pass |
| `RunnerSlotLifecycle` | slots | one cell, three launches whose responses write `Fixtures/Harness/heap-r<NN>.json`: three series rows with the scripted values, `raw/` copies with their SHA-256, `analysis.json` written, and the last lines printing `git add <run dir>` and the staged check; exit 0 |
| `RunnerAdbCrlf` | P10 | `adb shell settings get system screen_brightness` returning `1\r\n` reads as 1 and passes A4; `dumpsys battery` lines ending `\r\n` parse to the same level and temperature as LF lines |
| `RunnerResumeBootChanged` | resume | `-Resume` with a different simulated boot ID exits 2 with `RESEARCH_ERROR_BOOT_CHANGED`; with the same boot ID it continues from the first unexecuted slot |
| `RunnerNotReference` | identity | a machine with no `Machines/<machineId>.json` exits 3 with `RESEARCH_ERROR_NOT_REFERENCE` |
| `RunnerUnrestored` | restoration | locked application clocks exit 5 with `RESEARCH_ERROR_UNRESTORED`; with `-RestoreOnly` exit 0 after the two reset commands and the scheme restore |
| `RunnerPreflightD1` | D1 | `powercfg /query` reporting 100 for PROCTHROTTLEMAX fails D1; a `% Processor Performance` sample of 112 fails D1; 99/99 and samples ≤ 100 pass |
| `RunnerPreflightD2` | D2 | a lock command refused ("not supported") fails D2 with its message; a core clock 20 MHz off fails; 10 MHz off passes |
| `RunnerPreflightD3` | D3 | a 7% mean CPU fails D3 and the shim log shows the restoration; a foreign process at 2% fails; 1% mean passes |
| `RunnerPreflightD4` | D4 | GPU at 51 °C fails D4; 50 °C passes |
| `RunnerPreflightD5` | D5 | no `-DisplayChecked` fails D5; a 1920×1080 read fails D5 |
| `RunnerPreflightD6` | D6 | no `-UpdatesPaused` fails D6; a present `RebootRequired` key fails D6 |
| `RunnerPreflightD7` | D7 | `-AmbientC 27` fails D7; 26 passes |
| `RunnerPreflightD9` | D9 | a boot age of 200 s fails D9; 4 hours fails; 1,260 s passes |
| `RunnerThermalRetry` | R1 | a GPU at 55 °C for 31 polls: the slot is written `valid` 0 `THERMAL` after 300 simulated seconds, a retry slot is inserted, and the run completes with exit 0; a GPU reading 50 °C at the 21st poll starts the slot after 200 simulated seconds |
| `RunnerSlotClocksDrift` | R2 | a core clock 30 MHz off before slot 4: slot 4 invalid `CLOCKS` and retried |
| `RunnerSlotBackground` | R3 | a 6% background sample before slot 2: invalid `BACKGROUND` |
| `RunnerSlotDeviceReset` | R4 | `reset_required` reading `Yes` after slot 5: invalid `DEVICE`; a non-zero launch exit: invalid `EXIT` |
| `RunnerSlotClockEvent` | R5 | one sampler line with `hw_thermal_slowdown` `Active` during slot 3: invalid `CLOCKS`, `clockEvents` 1 |
| `RunnerInsufficient` | retries | a cell failing one repetition 4 times: the cell is `INSUFFICIENT`, the run exits 1, and the run directory is still assembled |
| `RunnerOperatorAbort` | abort | an operator abort during slot 6: the slot is `OPERATOR`, the session restores, exits 4 and writes nothing under the fixture `Zenith2/Docs/` |
| `RunnerAbortWritesNothing` | abort | a simulated device disconnect exits 4 and writes nothing under the fixture `Zenith2/Docs/` |
| `RunnerS24ModelRefused` | A1 | `SM-S921B` exits 3 with `RESEARCH_ERROR_PREFLIGHT` naming A1 |
| `RunnerFixedPerfUnavailable` | A2 | a refused command, and an accepted command whose `dumpsys power` lacks the line, each record `fixedPerformance: "unavailable"`; a slot starting at battery 32.0 °C then waits, because the limit is 31.0 °C, and starts when the shim's fourth reading is 30.9 °C |
| `RunnerS24Radios` | A3 | `airplane-mode enable` exiting 1 fails A3 |
| `RunnerS24Screen` | A4 | a read-back brightness of 255 fails A4 |
| `RunnerS24Background` | A5 | a second `cpuinfo` report with `TOTAL` 7% fails A5; a first report of 9% and a second of 3% passes |
| `RunnerS24BatteryStart` | A6 | level 45 fails A6; temperature 335 fails A6 |
| `RunnerS24ThermalStart` | A7 | `Thermal Status: 1` fails A7 |
| `RunnerS24Ambient` | A8 | `-AmbientC 22` fails A8 |
| `RunnerS24SlotThermal` | S1 | a thermal status 1 for 61 polls: the slot is invalid `THERMAL` after 600 simulated seconds |
| `RunnerS24BatteryLow` | S2 | a level of 39 before slot 7: the session pauses; the `operator` key answering `confirm-charge` with a level of 51 resumes it; with the `operator` key answering `continue` until the cap the remaining slots are invalid `BATTERY` |
| `RunnerS24EndThermal` | S3 | an end thermal status of 2 after slot 2: invalid `THERMAL` |
| `RunnerS24PullSize` | S4 | a pulled output shorter than the `stat` size: invalid `EXIT` |
| `RunnerCalibrate` | calibration | `-Calibrate` writes `Machines/<machineId>.calibration.json` with the 30 simulated minutes of samples and the cooldown curve, and measures nothing |
| `RunnerAssemblePlacement` | assembly | traces, a Budgets per-frame CSV, a PNG and a raw text over the per-file cap land in `archive/` with their entries; the series and the small raw reports land outside it; the non-LFS total over the test cap exits with `RESEARCH_ERROR_CAP` |

### Check-to-test matrix

| Check | Failing case | Passing case |
|---|---|---|
| D1 power plan | `RunnerPreflightD1` | `RunnerPreflightD1` |
| D2 GPU clocks | `RunnerPreflightD2` | `RunnerPreflightD2` |
| D3 idle load | `RunnerPreflightD3` | `RunnerPreflightD3` |
| D4 GPU temperature | `RunnerPreflightD4` | `RunnerPreflightD4` |
| D5 display | `RunnerPreflightD5` | `RecordValidSynthetic` |
| D6 updates and reboot | `RunnerPreflightD6` | `RecordValidSynthetic` |
| D7 ambient | `RunnerPreflightD7` | `RunnerPreflightD7` |
| D8 tree and build | `RunnerDirtyTreeRefused`, `RunnerNotBuiltRefused` | `RunnerDryRunSchedule` |
| D9 boot age | `RunnerPreflightD9` | `RunnerPreflightD9` |
| R1 slot temperature | `RunnerThermalRetry` | `RunnerThermalRetry` |
| R2 slot clocks | `RunnerSlotClocksDrift` | `RunnerThermalRetry` |
| R3 slot background | `RunnerSlotBackground` | `RunnerThermalRetry` |
| R4 exit and reset | `RunnerSlotDeviceReset` | `RunnerThermalRetry` |
| R5 clock events | `RunnerSlotClockEvent` | `RunnerThermalRetry` |
| A1 model | `RunnerS24ModelRefused` | `RunnerS24Radios` (reaches A3) |
| A2 fixed performance | `RunnerFixedPerfUnavailable` | `RunnerS24SlotThermal` (mode on) |
| A3 radios | `RunnerS24Radios` | `RunnerS24Screen` (reaches A4) |
| A4 screen | `RunnerS24Screen` | `RunnerS24Background` (reaches A5) |
| A5 background | `RunnerS24Background` | `RunnerS24Background` |
| A6 battery | `RunnerS24BatteryStart` | `RunnerS24ThermalStart` (reaches A7) |
| A7 thermal | `RunnerS24ThermalStart` | `RunnerS24Ambient` (reaches A8) |
| A8 ambient | `RunnerS24Ambient` | `RunnerS24SlotThermal` |
| S1 slot thermal | `RunnerS24SlotThermal` | `RunnerFixedPerfUnavailable` |
| S2 battery level | `RunnerS24BatteryLow` | `RunnerS24EndThermal` |
| S3 end thermal | `RunnerS24EndThermal` | `RunnerS24PullSize` |
| S4 pull size | `RunnerS24PullSize` | `RunnerS24EndThermal` |
| large-file precondition | `RunnerLfsMissingRefused` | `RunnerSlotLifecycle` |
| build slot | `RunnerSlotBusy` | `RunnerSlotLifecycle` |
| operator input | `RunnerOperatorAbort` | `RunnerS24BatteryLow` |

## Revalidation cases

| Case | Asserts |
|---|---|
| `RevalidationDependencyFires` | the fixture entry with `dependency:mimalloc` fires against `after.json` and not against `before.json`; a matching `revalidations.json` entry marks it handled |
| `RevalidationFinalExit` | `-Final` exits 1 with one unhandled fired trigger and 0 once handled |
| `RevalidationMarginStage` | a ruling whose BETTER gain low end was 1.5δ fires `margin` at the first stage above the ruling's stage and not at its own |
| `RevalidationImpliedTokens` | an RS entry without `toolchain` and `hardware` is reported, and the scan evaluates both anyway |
| `CompareRunsOutcomes` | identical verdicts and a shift within the bound give `stands`; a shift moved by 2δ gives `stands-drifted`; BETTER to INCONCLUSIVE gives `regressed` |
| `RegressionTicketBody` | the generated body has the ten sections in order, the template lines, the two DoD lines, the dependent tickets of a fixture board, and the printed `zagent create` line, which carries `--type TASK` and `--label needs-human` and never `--type BUG`; a budget-row regression produces no command; the function writes no file under the fixture board directory |

## Native versus adapter cases

| Case | Asserts |
|---|---|
| `AdapterShareFixture` | through medians 200 (interval [190, 210]) against native 150 ([140, 160]): share 250,000 ppm, low 157,895 ppm ((190 − 160)/190 rounded away from zero), high 333,334 ppm ((210 − 140)/210) |
| `AdapterShareNegativeKept` | through 140 against native 150 gives a negative share, kept |
| `AdapterShareZeroMedian` | an adapter series whose median interval low end is 0 gives share and bounds 0 with reason `ZERO_MEDIAN`, and no division is attempted |
| `ClassificationTable` | each row of `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Architectural and adapter classification" from constructed verdict pairs, including UNRESOLVED for an INVALID native verdict |
| `EnttBothVariantsReported` | an ECS fixture study with `native-entt-view` and `native-entt-group` cells reports two adapter rows for EnTT and picks neither, and carries the `jobs-overhead` series |
| `ShadingPathNotApplicableS24` | the fixture shading-path study's Deferred and visibility-buffer cells on the S24 appear as `NOT_APPLICABLE` with their reason in `analysis.json` and in the report |
