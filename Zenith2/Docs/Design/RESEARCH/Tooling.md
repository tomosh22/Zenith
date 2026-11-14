# RESEARCH design: tooling, checks and the CI procedure

This file is part of the RESEARCH design family; the entry document is `Zenith2/Docs/Design/RESEARCH.md`. It specifies the PowerShell module that implements the methodology and the formats, the research runner the human runs on the reference hardware, the conformance checks the gates run, the CI procedure (which benchmarks run in which gate, the regression thresholds and how a regression becomes a ticket), and the hand-offs to PROC.

## Scope of this file

RESEARCH has no C++ library: its code is one PowerShell module, two entry scripts and their assert tests, in the RESEARCH-owned directory `Zenith2/Benchmarks/Research/` (`Zenith2/Docs/Architecture.md` → "Ownership per directory": `Benchmarks/` is RESEARCH's, each harness the measured area's). PowerShell is chosen because every input is JSON or CSV text, the statistics are exact integer arithmetic (`System.Numerics.BigInteger`), the evidence functions it reuses are PowerShell (`Zenith2/Docs/Design/DEVT/Evidence.md` → "Zenith2Evidence module"), and D-SEL-JSON (open, Stage 1) is itself an instance of this methodology: a C++ analyser built on the engine's JSON reader would need the outcome of a ruling it has to analyse, or a JSON parser of its own with a manifest pin and a build.

## Files

| Path | Kind | Content |
|---|---|---|
| `Zenith2/Benchmarks/Research/Zenith2Research.psm1` | module | every function of "Format functions" to "Revalidation scan" |
| `Zenith2/Benchmarks/Research/Zenith2Research.psd1` | manifest | `ModuleVersion` (the tool version written into `analysis.json` as `Zenith2Research/<major>`), `PowerShellVersion = '7.4'`, `RequiredModules` none, `FunctionsToExport` the exact list of this file |
| `Zenith2/Benchmarks/Research/Run-Research.ps1` | entry script | the research runner ("Research runner") |
| `Zenith2/Benchmarks/Research/Invoke-ResearchCheck.ps1` | entry script | the conformance checks ("Conformance checks") |
| `Zenith2/Benchmarks/Research/Tests/Zenith2Research.Tests.ps1` | assert script | every `Pwsh_Research_*` case, selected by `-Case` (`Zenith2/Docs/Design/RESEARCH/Tests.md`) |
| `Zenith2/Benchmarks/Research/Tests/Probes/Probe-PwshRules.ps1` | probe script | one runnable proof per rule of "PowerShell rules of the module"; prints `P<n> OK` or `P<n> FAIL <detail>` per rule and exits 1 on any `FAIL` |
| `Zenith2/Benchmarks/Research/Tests/Fixtures/**` | fixtures | hand-written JSON, CSV and Markdown, every expected value derivable from the specification |
| `Zenith2/Benchmarks/Research/Templates/Report.md.tmpl` | template | the report skeleton of `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Report template" |
| `Zenith2/Benchmarks/Research/Templates/RegressionTicket.md.tmpl` | template | the re-ruling ticket body ("Regression filing") |
| `Zenith2/Docs/Research/Index.md`, `Zenith2/Docs/Research/Machines/**` | data | the research tree's index and machine registry |

The directory has no `CMakeLists.txt` and no compiled source, so no target globs it and `SourceLayout` has nothing to check there ([Z2-N-ARCH-021] covers compiled-language sources only).

### PowerShell rules of the module

Each rule below states a behaviour of `pwsh` 7.6.7 (the pinned shell). `Zenith2/Benchmarks/Research/Tests/Probes/Probe-PwshRules.ps1` holds one runnable proof per row, and `Pwsh_Research_PwshRulesProbe` runs it in the `Test` step, so a `pwsh` update that changes one of these behaviours fails a test instead of silently changing the module's output.

| # | Rule | Why |
|---|---|---|
| P1 | JSON is read with `[System.Text.Json.JsonDocument]::Parse([System.ReadOnlyMemory[byte]]::new($bytes))`, `$bytes` from `[System.IO.File]::ReadAllBytes`, the document disposed after the walk; it is walked with `EnumerateObject()` and `EnumerateArray()` into the module's own ordered dictionaries, each constructed as `[System.Collections.Specialized.OrderedDictionary]::new()` (case-sensitive keys; `[ordered]@{}` is never used for parsed data); member presence is `.Contains(<name>)`. A harness output that begins with the UTF-8 BOM `EF BB BF` has those three bytes removed before parsing; a committed research file with a BOM is `RESEARCH_CHECK_NOT_TEXT`. Nesting deeper than the default `MaxDepth` of 64 is refused by the parser and reported as `EXTRACT` (a harness output) or `RESEARCH_ERROR_SCHEMA` (a research file). `ConvertFrom-Json` is never used, and `PSObject.Properties[...]` is never applied to parsed data | `Parse($bytes)` with a `byte[]` is refused as an ambiguous overload, while the `ReadOnlyMemory[byte]` form parses; the parser rejects a leading BOM and a depth of 65; `[ordered]@{}` compares keys case-insensitively, so `{"A":1,"a":2}` would read as a duplicate; `ConvertFrom-Json` turns ISO-8601 strings into `[datetime]` and fractional numbers into `[double]`; on a hashtable `PSObject.Properties['a']` is `$null` for a present key |
| P2 | A JSON number is read only as `JsonElement.GetRawText()` and converted by the lexeme rules of `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Extraction selectors" into `[System.Numerics.BigInteger]`; a string is read with `GetString()` and stays a string | `GetRawText()` of `15.900` is `15.900`; `11400714819323198485` parses exactly as a `BigInteger` |
| P3 | Duplicate member names are detected during the walk and refused (`RESEARCH_ERROR_SCHEMA`); names differing only in case are different members | `JsonDocument` accepts `{"a":1,"a":2}` |
| P4 | Every arithmetic operation on a sample-derived value is a `BigInteger` operation; quotients and remainders use `[System.Numerics.BigInteger]::Divide` and `::DivRem`; `/`, `%` and `[math]::` are never applied to such a value | on `Int32` or `Int64` operands `7/2` is the `[double]` 3.5 and `[long]::MaxValue * 2` silently becomes `[double]`; a `BigInteger` `/` stays `BigInteger` but truncates toward zero, so the explicit floor, ceiling and away-from-zero rules of `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Arithmetic rules" are built from `DivRem` rather than left to an operator whose type depends on its operands |
| P5 | Every ordering of strings that this design calls byte order uses `[System.Array]::Sort(<string[]>, [System.StringComparer]::Ordinal)`; every ordering of sample values uses `[System.Array]::Sort(<BigInteger[]>)`; `Sort-Object` is never used | `Sort-Object` is culture-sensitive: it orders `_x,Board,board,GPU,gpu` where ordinal order is `Board,GPU,_x,board,gpu` |
| P6 | Every committed or compared file is written with `[System.IO.File]::WriteAllText(<path>, <text>, [System.Text.UTF8Encoding]::new($false))` (or `WriteAllBytes`) with LF line endings already in the text, and read with `[System.IO.File]::ReadAllBytes` for hashing; `Set-Content`, `Out-File`, `>` and `Get-Content` are never used on such files | `Set-Content -Value "abc`n"` writes `61 62 63 0a 0d 0a`; `WriteAllText` with a no-BOM encoder writes `61 62 63 0a` |
| P7 | Integers are formatted with `.ToString([System.Globalization.CultureInfo]::InvariantCulture)`; `ConvertTo-Json` is never called | `ConvertTo-Json` truncates at depth 2 and prints `1E+20` and `0.30000000000000004` |
| P8 | SHA-256 is `[System.Security.Cryptography.SHA256]::HashData(<bytes>)` rendered with `[System.Convert]::ToHexString(…).ToLowerInvariant()` | one hash function, no `Get-FileHash` culture or encoding questions |
| P9 | The module runs under `Set-StrictMode -Version Latest`, returns collections with `,@(...)` so a one-element list stays a list, and starts no job, runspace or thread | DEVT's module conventions |
| P10 | The standard output of every `adb shell` command (`settings get` read-backs, `dumpsys battery`, `getprop`, `stat`) is split into lines on the pattern `\r?\n` and each line trimmed before it is parsed | `adb shell` output on Windows ends lines with CR LF, while the command shim returns clean text, so only this rule keeps the shim cases and the device in agreement |

`Pwsh_Research_ModuleStaticRules` parses `Zenith2Research.psm1`, `Run-Research.ps1` and `Invoke-ResearchCheck.ps1` with `[System.Management.Automation.Language.Parser]::ParseFile` and walks the syntax tree. It fails on a command whose name, compared case-insensitively, is `ConvertFrom-Json`, `ConvertTo-Json`, `Set-Content`, `Add-Content`, `Out-File`, `Get-Content`, `Sort-Object` or `Start-Sleep`, or one of their aliases (`sort`, `gc`, `cat`, `type`, `sc`, `ac`, `sleep`); on a file redirection (`>`, `>>`, `2>`); on a type expression `[math]`; and on a member access named `psobject` in any case. A `/` or `%` applied to sample-derived values is checked by review and by `Pwsh_Research_ArithmeticBeyond2Pow53`.

### Result objects

Every function returns either a value or a result object `[pscustomobject]@{ Ok = <bool>; Value = <object or $null>; Errors = ,@(<error>…) }`, an error being `[pscustomobject]@{ Code = 'RESEARCH_…'; File = <repo-relative path or $null>; Line = <int or 0>; Message = <string> }`. Only a programmer error (a parameter of the wrong type, a `[double]` handed to the canonical writer) throws.

## Format functions

| Function | Parameters | Returns |
|---|---|---|
| `ConvertTo-Zenith2CanonicalJson` | `-Value <OrderedDictionary or array>` `-Schema <string>` (selects the member order table) | `[string]` canonical JSON with a final LF (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "Canonical JSON") |
| `Read-Zenith2ResearchJson` | `-Path` | result; `Value` the ordered dictionary of rule P1, numbers as `BigInteger`, strings as strings |
| `Read-Zenith2ResearchStudy` | `-Path <study.json>` | result; `Value` the study |
| `Test-Zenith2ResearchStudy` | `-Path <study.json>` `[-DeclaredDeps <declared_deps.json>]` | result; every rule of "study.json fields", "Cell fields", "Metric fields", "Comparison fields", the rule grammar, the incumbent rule, the 12-per-family cap, every comparison's cells differing only in the members their factor allows, the `sessions` partition, the session plan within the cap, and, with `-DeclaredDeps`, every `bench-*` and `probe` target `Zenith_Bench_<Name>` matching a `benchmarks[]` `name` |
| `Read-Zenith2ResearchRecord` | `-RunDirectory <dir>` | result; `Value` the record |
| `Test-Zenith2ResearchRecord` | `-RunDirectory <dir>` `[-CurrentInputs]` | result; the record schema, every listed file present with its SHA-256 and size, the series headers and rows, the trace format, the text rule, the caps, the stray-file rule, the large-file verification, `studySha256` equal to the committed study's blob or listed in `deviations`; with `-CurrentInputs`, see "Conformance checks" |
| `Read-Zenith2ResearchSeries` | `-Path <csv>` | result; `Value` `@{ Header; Rows }` with `BigInteger` values |
| `ConvertTo-Zenith2ResearchTrace` | `-FramesCsv <path>` `-Cell <id>` `-Repetition <int>` `-OutPath <path>` | result; writes the trace by the exact decimal conversion |
| `Copy-Zenith2ResearchRaw` | `-Source <path>` `-Destination <path>` `-Text <bool>` | result; `Value` `@{ Sha256; Bytes; SourceSha256 }`: for a text file, `\r\n` replaced by `\n` and a final `\n` added, `SourceSha256` set when the bytes changed; a binary file copied unchanged |
| `Get-Zenith2ResearchExtractedValue` | `-Source <OrderedDictionary>` `-SlotOutput <path>` `-Statistic <string>` | result; `Value` one `BigInteger` ("Extraction selectors") |
| `Get-Zenith2MachineIdentity` | `-Platform Win|S24` `[-Serial <adb serial>]` | ordered dictionary `{ machineId, identity }` per "Machines registry" |
| `Get-Zenith2MachineState` | `-Platform Win|S24` `[-Serial]` | the record's `machine` object |
| `Update-Zenith2ResearchMachineState` | `-Platform` `[-Serial]` `-Repo <root>` | writes `Machines/current.json` (the mid-sprint integration check calls it) |

## Statistics functions

Pure functions over `BigInteger[]`; each implements one section of `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Statistics" exactly.

| Function | Parameters | Returns |
|---|---|---|
| `Get-Zenith2ResearchMedian` | `-Values` | `BigInteger`, rank (n + 1) ÷ 2 |
| `Get-Zenith2ResearchPercentile` | `-Values` `-Percent 95|99` | `BigInteger`, rank (pk + 99) ÷ 100 |
| `Get-Zenith2ResearchMedianInterval` | `-Values` | `@{ Low; High; J; K; CoverageNum; CoverageDen }`; n outside 6 … 20 throws |
| `Get-Zenith2ResearchRobustCv` | `-Values` `-Class` | `@{ Ppm; Mad; State }` with State `OK`, `NOISY`, `NOISY_INVALID` |
| `Get-Zenith2ResearchDrift` | `-ValuesInRepetitionOrder` | `@{ Inversions; PNum; PDen; Drift }` |
| `Get-Zenith2ResearchRankSum` | `-Candidate` `-Baseline` | `@{ W2; U2; PNum; PDen }` |
| `Get-Zenith2ResearchShift` | `-Candidate` `-Baseline` `-Family <1…12>` | `@{ Shift; Low; High; K }` or an `INSUFFICIENT` marker |
| `Get-Zenith2ResearchCliffDelta` | `-Candidate` `-Baseline` | `@{ Num; Den }` |
| `Get-Zenith2ResearchHolm` | `-PValues <array of @{Num;Den}>` | the adjusted rationals in input order, reduced |
| `Get-Zenith2ResearchMargin` | `-Class` `-BaselineMedian` `[-RelPpmOverride]` | `BigInteger` δ |
| `Get-Zenith2ResearchVerdict` | `-Shift <result>` `-Direction lower|higher` `-Delta` `-CandidateState` `-BaselineState` `-CandidateDrift` `-BaselineDrift` `-RankSumP <@{Num;Den}>` `-Family` `-CandidateRcvPpm` `-BaselineRcvPpm` `[-Confounded <member>]` | `@{ Verdict; Reasons; GainLow; GainHigh; Significant }` |
| `New-Zenith2ResearchSchedule` | `-Cells <OrderedDictionary[]>` `-Repetitions <10…20>` `-Seed <string>` | the ordered `(slot, cell, repetition)` list of "Schedule algorithm" |
| `Get-Zenith2ResearchSplitMix64` | `-State <BigInteger>` | `@{ Next; Value }`, one SplitMix64 draw modulo 2^64 |
| `Invoke-Zenith2ResearchRule` | `-Expression <string>` `-Analyses <OrderedDictionary[]>` `-Study <OrderedDictionary>` | `@{ Terms; Outcome }` over the given runs' comparisons ("Rule evaluation"); a parse error is `RESEARCH_ERROR_RULE` naming the column |
| `Invoke-Zenith2ResearchStudyRule` | `-Study <dir>` `-Runs <RunId[]>` | `@{ Terms; Outcome; Reasons }`, the study-level outcome: reads the named runs' `analysis.json` (one per (platform, session)), refuses two runs covering the same (platform, session) (`RESEARCH_ERROR_SCHEMA`), and calls `Invoke-Zenith2ResearchRule` once over all of them |

## Analysis and report functions

| Function | Parameters | Effect |
|---|---|---|
| `Invoke-Zenith2ResearchAnalysis` | `-RunDirectory` `[-Check]` | computes every series and comparison of the run (families from `familyBy`), the RA classification and adapter tables when the study is the ECS RA study (profile RA with an `api` factor), and the rule's terms the run can evaluate alone (`rule.outcome` is `null` with reason `PARTIAL` when the rule needs another run, "Rule evaluation"); writes `analysis.json`; with `-Check`, writes nothing and returns `RESEARCH_CHECK_ANALYSIS_STALE` when the committed file's bytes differ from the regenerated ones |
| `New-Zenith2ResearchReport` | `-StudyDirectory` `-Runs <RunId[]>` `-Decision <Z2-D-NNN>` | writes `Reports/<Z2-D-NNN>.md` from the template, the generated blocks filled (the `analysis` block holding the study-level outcome of `Invoke-Zenith2ResearchStudyRule` over `-Runs`), the written sections holding a `<!-- research:write -->` marker line each |
| `Update-Zenith2ResearchReport` | `-Path <report>` | regenerates the generated blocks only |
| `Test-Zenith2ResearchReport` | `-Path <report>` | result; the heading list and order, no remaining `<!-- research:write -->` marker, every generated block byte-equal to its regeneration (the `analysis` block through `Invoke-Zenith2ResearchStudyRule` over the runs the report names), every run it names existing |
| `New-Zenith2ResearchDecisionFields` | `-Report <path>` | the three DecisionLog field lines of `Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields", printed for the human to paste into the entry |

## Research runner

```
pwsh -NoProfile -File Zenith2/Benchmarks/Research/Run-Research.ps1
	-Study <Zenith2/Docs/Research/<Study>> -Platform Win|S24 -AmbientC <int>
	[-Session <index>] [-Serial <adb serial>] [-DisplayChecked] [-UpdatesPaused]
	[-Resume <RunId>] [-RestoreOnly] [-DryRun] [-Calibrate]
```

Steps:

1. **Inputs.** `Test-Zenith2ResearchStudy` with `declared_deps.json` of the `win-release` tree; refuse (exit 2) on any error. `-Session` selects one entry of the study's `sessions` (required when the list is non-empty). `Get-Zenith2TreeSha` must report a clean tree (exit 2, `RESEARCH_ERROR_DIRTY_TREE`). Every executable the session's cells launch must exist in the `win-release` or `android-arm64` Release output (exit 2, `RESEARCH_ERROR_NOT_BUILT`); the runner never builds. Large-file precondition (exit 2, `RESEARCH_ERROR_NOT_LFS`): `git lfs version` exits 0, `git config --get filter.lfs.clean` prints a non-empty value, `git config --get filter.lfs.required` prints `true`, and `git check-attr filter -- Zenith2/Docs/Research/<Study>/runs/<RunId>/archive/probe.bin` prints `filter: lfs`, so a run whose `archive/` would be committed as plain blobs is never started.
2. **Lock.** On Windows, the runner re-executes itself inside the machine-wide build slot, `pwsh -NoProfile -File Tools/build_lock.ps1 -Run "<its own command line with -InsideSlot>" -WaitMinutes 0`; `build_lock.ps1` exits 11 when the slot is held and otherwise returns the wrapped command's own exit code, so the outer process maps 11 to exit 3 with `RESEARCH_ERROR_SLOT_BUSY` and passes every other code through unchanged. On the S24 it sets `adb shell setprop debug.zenith2.research <RunId>` and refuses a device whose property is non-empty.
3. **Unrestored state.** If the GPU's application clocks differ from the defaults or the active power scheme is `Zenith2 Research`, exit 5 with `RESEARCH_ERROR_UNRESTORED` unless `-RestoreOnly`, which restores (`Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Desktop restoration") and exits 0.
4. **Identity.** `Get-Zenith2MachineIdentity`; the machineId must have a `Machines/<machineId>.json` whose `referenceFor` lists the platform (exit 3, `RESEARCH_ERROR_NOT_REFERENCE`; registering a machine is a committed change by a human, never done by the runner). Compute the RunId; create `Zenith2/out/<checkout-hash>/research/<RunId>/` as the work directory.
5. **Conditions and preflight.** Apply the controlled conditions and run D1 … D9 or A1 … A8; any failure exits 3 with `RESEARCH_ERROR_PREFLIGHT` naming the check, after restoring.
6. **Schedule.** `New-Zenith2ResearchSchedule` (`Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Schedule algorithm"), written into the work record before the first launch.
7. **Slots.** For each slot: the repetition checks R1 … R3 or S1 … S2; on the desktop, the R5 clock-event sampler started; the launch (`bench-host` and `test-native`: the executable with the expanded arguments, standard output and error to `logs/`; `bench-device`: install once per session through the hosted executable APK of `Zenith2/Docs/Design/MOD/GameBuild.md` → "Hosted executable APKs", launch, wait, `adb pull` the output; `workload`: `Run-Workload.ps1 -Game <G> -Workload <W> -Platform <p> -Backend <b> -Repetitions 1 -FirstRepetition <r> -Conditions controlled -CacheState warm -TimeoutSeconds <timeoutSeconds> -EvidenceRoot <work>/workload/` of `Zenith2/Docs/Design/DEVT/Evidence.md` → "Workload runner"; `probe`: one launch per case); the post-slot checks R4 and R5 or S3 and S4 (operator input, such as the S2 charging confirmation or an abort, is read only through the shim-able command `operator`); extraction of every metric of the cell into its series row; the cell inputs computed once per cell with `Get-Zenith2EvidenceFingerprint`.
8. **Retries.** Invalid slots are re-inserted per "Invalid repetitions and retries".
9. **Restore.** Restore the conditions; record the result.
10. **Assemble.** Write `record.json`, the series, `raw/` (through `Copy-Zenith2ResearchRaw`) and `archive/` (traces, Budgets per-frame CSVs, binary outputs and over-cap text) into `Zenith2/Docs/Research/<Study>/runs/<RunId>/`; refuse with `RESEARCH_ERROR_CAP` when the non-LFS files would pass the run-directory cap (the study's cells are split into sessions instead); run `Invoke-Zenith2ResearchAnalysis` and `Test-Zenith2ResearchRecord`. The runner's last lines print `git add Zenith2/Docs/Research/<Study>/runs/<RunId>` and `Invoke-ResearchCheck.ps1 -Mode committed -Staged`; the human runs both, then commits the run directory before starting the study's next session ("Sessions" of the methodology); git LFS stores `archive/` through the `.gitattributes` block, and the staged check proves it did.
11. **Exit.** 0 when every cell has n valid launches; 1 when some cell is `INSUFFICIENT` (the run is still assembled, and its analysis shows the INVALID comparisons); 4 when the session was aborted (device lost, operator abort, the 12-hour cap), in which case nothing is written under `Zenith2/Docs/` and the work directory holds the partial run for `-Resume` on the same boot.

`-DryRun` performs steps 1, 4 and 6, prints the schedule and the session plan in hours, launches nothing and changes no machine setting. `-Calibrate` performs "Desktop calibration" of the methodology and writes `Machines/<machineId>.calibration.json` only.

### Command shim

Every external command (`nvidia-smi`, `powercfg`, `Get-Counter`, `adb`, `build_lock.ps1`, the launches) and every wait goes through `Invoke-Zenith2ResearchCommand -Name <name> -Arguments <string[]>`, and the runner reads the time only through `Get-Zenith2ResearchNowMs`; it never calls `Start-Sleep` or `[DateTime]::UtcNow` directly. When `$env:ZENITH_RESEARCH_COMMAND_SHIM` names a JSON file, the shim replaces all three:

```json
{
	"schema": "zenith2.research-shim",
	"schemaVersion": 1,
	"startUnixMs": 1793606400000,
	"commands": {
		"nvidia-smi --query-gpu=temperature.gpu --format=csv,noheader,nounits": { "responses": [ { "exitCode": 0, "stdout": "55" }, { "exitCode": 0, "stdout": "55" }, { "exitCode": 0, "stdout": "44" } ] },
		"powercfg /getactivescheme": { "responses": [ { "exitCode": 0, "stdout": "Power Scheme GUID: … (Zenith2 Research)" } ] }
	}
}
```

- A command key is the name and the arguments joined by one space; its `responses` are returned in order, and the last one repeats once the list is exhausted. A command without a key fails the case with `SHIM_UNSCRIPTED <key>`.
- The reserved name `sleep` (`Invoke-Zenith2ResearchCommand -Name sleep -Arguments <seconds>`) advances the shim's clock by the seconds and returns at once; `Get-Zenith2ResearchNowMs` returns `startUnixMs` plus every simulated sleep and 1 ms per command call. So a 300 s cooldown or a 900 s S24 wait runs in milliseconds, and every runner case fits the 120 s CTest timeout of `zenith_register_pwsh_test`.
- A command key may contain the placeholders `{work}` (the work directory), `{runId}`, `{slot}` (the slot index) and `{out}` (the slot's expanded output path); the shim expands them from the runner's current context before matching, so a fixture is written without knowing the checkout hash or the start time.
- A response may carry `writes: [ { "path": "{out}", "fixture": "Fixtures/Harness/<name>.json" } ]`: before returning, the shim copies each fixture file byte for byte to its expanded path, so a launch, an `adb pull` or a workload run produces the output the extraction then reads.
- The reserved name `operator` (`Invoke-Zenith2ResearchCommand -Name operator -Arguments confirm-charge|abort|continue`) is the runner's only way to read operator input; outside the shim it prompts on the console, under the shim it is scripted like any other key.
- A top-level `stubs` map (`{ "Get-Zenith2TreeSha": { "fixture": "Fixtures/Stubs/tree-clean.json" }, "Get-Zenith2EvidenceFingerprint": { "fixture": "Fixtures/Stubs/fingerprint.json" } }`) replaces the two DEVT functions while the shim is active, each returning the fixture's object; no runner case calls the real ones, which need git, the CMake File API and `ninja`.
- The shim logs every call with its simulated time to `<work>/shim.log`, which the runner cases assert against. `Pwsh_Research_RunnerSlotLifecycle` drives one cell through three launches whose responses write harness outputs, so extraction, series rows and assembly all run without hardware.

## Large-file functions

| Function | Parameters | Effect |
|---|---|---|
| `Test-Zenith2ResearchLargeFiles` | `-RunDirectory` `[-Staged]` | result; the four steps of `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file verification": steps 1 to 3 read the working tree (an LFS pointer or a fetched object), step 4 reads git alone — the index with `-Staged`, the committed tree otherwise — and needs no git-lfs |
| `Get-Zenith2ResearchStudyStorage` | `-StudyDirectory` | `@{ NonLfsBytesPerRun; LfsBytes }`, the measurement `MP-RESEARCHSTORE` of the three storage rows |

## Conformance checks

```
pwsh -NoProfile -File Zenith2/Benchmarks/Research/Invoke-ResearchCheck.ps1 -Mode <mode> [-Path <path>] [-Repo <root>] [-Out <json>] [-CurrentInputs] [-Staged]
```

| Mode | What it checks | Function |
|---|---|---|
| `committed` | every study under `Zenith2/Docs/Research/` (a directory holding `study.json`): `Test-Zenith2ResearchStudy`; every run: `Test-Zenith2ResearchRecord` (including the large-file verification, step 4 against the index with `-Staged`) and `Invoke-Zenith2ResearchAnalysis -Check`; every report: `Test-Zenith2ResearchReport`; every `revalidations.json` against its schema; `Index.md` lists every study once; the text rule, the path lengths and the three storage quantities against their caps; once any run directory is committed, the repository-root `.lfsconfig` carries `fetchexclude = Zenith2/Docs/Research` under `[lfs]` and `Zenith2/.gitattributes` carries the `zenith-research` block (`RESEARCH_CHECK_LFS_CONFIG`). The scope is the study directories, `Index.md` and `Machines/**` only (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "Size caps and the text rule"); any other file under `Zenith2/Docs/Research/`, such as DEVT's `LinuxTsanHost/<utc>/junit.xml`, is not examined | `Invoke-Zenith2ResearchCheck` |
| `decisionlog` | every entry of `Zenith2/Docs/DecisionLog.md` carrying **Research:**: the study, runs and report exist; the **Analysis** outcome equals the one `Invoke-Zenith2ResearchStudyRule` regenerates over the entry's runs, and its verdicts equal those of the runs' `analysis.json`; every **Revalidation** token belongs to the vocabulary and the implied ones are present; an RA, RS, RJ, RB or RP open decision marked `Ruled as Z2-D-NNN` points at an entry carrying **Research:** | `Test-Zenith2DecisionLogResearchEntries` |
| `instances` | every open decision of `Zenith2/Docs/DecisionLog.md` → "Open decisions" appears in `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances" with a profile, and every row there names an open or ruled decision; the rule cells are documentation and are not parsed | `Test-Zenith2ResearchInstanceMap` |
| `tickets` | every ticket file under `-Path` (a drafts directory or `Zenith2/Docs/Board/tickets`) that the check selects passes "Decision ticket check" | `Test-Zenith2DecisionTicket` |

`-CurrentInputs` (the stage acceptance run) recomputes each research run's cell inputs against the current tree, not the run's own tree: a member that differs is reported as `INFO RESEARCH_INPUTS_CHANGED <run> <cell> <member>` and feeds the `target:` and `dependency:` triggers of the revalidation scan; it is a finding only when `git diff --name-only <run treeSha> HEAD` lists none of that member's inputs, which would mean the recorded value was wrong.

Exit codes: 0 no findings; 1 findings (each printed as `RESEARCH_CHECK_<CODE> <file>:<line> <message>` and written to `-Out`, default `Zenith2/out/<checkout-hash>/research/check-<mode>.json`); 2 usage or an unreadable input.

### Check finding codes

| Code | Meaning |
|---|---|
| `RESEARCH_CHECK_SCHEMA` | a file fails its schema |
| `RESEARCH_CHECK_SHA_MISMATCH` | a listed file's SHA-256 or size, or an LFS pointer's oid or size, differs |
| `RESEARCH_CHECK_MISSING_FILE` | a listed or required file is absent |
| `RESEARCH_CHECK_STRAY_FILE` | a file inside a run directory that its layout does not allow |
| `RESEARCH_CHECK_NOT_TEXT` | the text rule fails |
| `RESEARCH_CHECK_CAP` | a file or run directory passes its cap |
| `RESEARCH_CHECK_NOT_LFS` | a path under `archive/` whose `filter` attribute is not `lfs`, or whose blob is not an LFS pointer with the `archive[]` entry's oid and size |
| `RESEARCH_CHECK_LFS_CONFIG` | a run directory is committed while `.lfsconfig` lacks the research `fetchexclude` or `Zenith2/.gitattributes` lacks the `zenith-research` block |
| `RESEARCH_CHECK_PATH_LENGTH` | a study path longer than 64 characters, or a file under a run directory whose repo-relative path is longer than 200 characters |
| `RESEARCH_CHECK_ANALYSIS_STALE` | `analysis.json` differs from its regeneration |
| `RESEARCH_CHECK_REPORT_STALE` | a generated block differs from its regeneration |
| `RESEARCH_CHECK_REPORT_HEADINGS` | the report's `##` headings differ from the template |
| `RESEARCH_CHECK_REPORT_UNWRITTEN` | a `<!-- research:write -->` marker remains |
| `RESEARCH_CHECK_STUDY_DRIFT` | `studySha256` differs from the committed study and no deviation explains it |
| `RESEARCH_CHECK_DECISION_FIELDS` | a DecisionLog research field is missing, malformed or disagrees with the analysis |
| `RESEARCH_CHECK_TRIGGER` | an unknown revalidation token, or an implied one missing |
| `RESEARCH_CHECK_INSTANCE_MAP` | an open decision without an instance row, or a row without a decision |
| `RESEARCH_CHECK_INDEX` | `Index.md` misses or repeats a study |
| `RESEARCH_CHECK_INPUTS` | `-CurrentInputs` found a changed member with no changed input |

### Decision ticket check

`Test-Zenith2DecisionTicket -Path <file> -Mode draft|board`:

1. Select a ticket: in `draft` mode, front matter `kind:` of `decision`, `budget` or `revalidation`, or a body line `Research-Role:`; in `board` mode, the managed trailer's `Kind:` line (written by PROC's production filer) with the same values, or a `Research-Role:` line. Any other ticket is checked by step 4 alone.
2. For a selected ticket, read the first block of `## Implementation Contract`: every label of `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Template lines" present once (`Research-Role:` optional), in that order, each value matching its rule; `RESEARCH_TICKET_LINE_<LABEL>` names a missing or malformed line. A `budget` or `revalidation` ticket that runs no measurement (its `Outputs:` names nothing under `runs/`) may carry `Research-Profile: RQ` and `Study: none`.
3. Profile rules ("Profile requirements for the template lines"): a non-RQ ticket's `Study:` path created by the Implementation Contract of a ticket in its `blocked_by` (draft mode, read from that ticket's file; board mode, read from the blocking ticket's body) (`RESEARCH_TICKET_STUDY_PRODUCER`); the Definition of Done holds the lines of "Definition of Done lines" in substance — an item naming `Pwsh_Research_CheckCommitted` and a failure-scenario item (`RESEARCH_TICKET_DOD`); the `Revalidation triggers:` tokens parse (`RESEARCH_TICKET_TRIGGER`).
4. For every ticket, selected or not, the check reads the ticket's committed outputs only: the Implementation Contract's "Files edited in one diff" line, its `Outputs:` template line, and every Definition of Done item containing the word "committed". An output path under `Zenith2/Docs/Research/**/runs/` or `Zenith2/Docs/Research/Machines/` needs `needs-human` (`RESEARCH_TICKET_NEEDS_HUMAN`), and a `kind: harness` ticket with such an output fails (`RESEARCH_TICKET_WORKER_EVIDENCE`). A path named anywhere else (an assembler that reads `runs/<RunId>/raw/`, a Goal that says where the ruling's run will land) is read context and passes.

The check reads Markdown only and runs in under 1 s per 1,000 tickets (one pass per file). It examines ticket text; the diff itself is guarded by P-RS6: both globs are protected paths, so `zagent guard` refuses a loop ticket whose diff touches them, and only a `needs-human` ticket, which the loop never claims, can commit there.

## Revalidation scan

```
pwsh -NoProfile -Command "Import-Module ./Zenith2/Benchmarks/Research/Zenith2Research.psd1; Get-Zenith2ResearchRevalidation -Repo . [-Stage <N>] [-Final]"
```

1. Parse every `### Z2-D-NNN` entry of `Zenith2/Docs/DecisionLog.md` whose **Status** is `Settled` and which carries **Revalidation:**.
2. For each token, read the value now (the manifest hashes and entries through SHIP's `Get-Zenith2ManifestHashes` and the manifest file; `Machines/current.json`; the workload definitions; `Zenith2/Docs/Budgets.md` → "Row index"; `git diff --name-only <run treeSha> HEAD -- <dir>`), compare with the value the entry's runs recorded, and decide fired or not (`Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Trigger vocabulary"); `stage:<N>` fires when `-Stage` equals N; `margin` fires at the first `-Stage` above the ruling's stage.
3. Mark each fired trigger handled when the study's `revalidations.json` has an entry with the same (decision, trigger, observed value).
4. Write `Zenith2/out/<checkout-hash>/research/revalidation.json` (`{ schema: "zenith2.research-revalidation-scan", schemaVersion: 1, stage, decisions: [ { decision, study, triggers: [ { trigger, recorded, observed, fired, handled } ] } ] }`) and print one line per fired trigger.
5. Exit 0; with `-Final`, exit 1 while any fired trigger is unhandled.

The scan builds nothing and launches nothing; it reads committed files and git.

### Run comparison

`Compare-Zenith2ResearchRuns -Baseline <run dir> -Candidate <run dir>` requires the same study version and platform, and returns per primary comparison `{ id, baselineVerdict, candidateVerdict, baselineShift, candidateShift, bound, movedBeyondBound }` and the outcome `stands`, `stands-drifted` or `regressed` of `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Research regression rule", plus `rule.outcome` of both. `Add-Zenith2ResearchRevalidation -Study -Decision -Trigger -Observed -Recorded -Run -Outcome [-RerulingTicket]` appends the `revalidations.json` entry.

## Regression filing

`New-Zenith2ResearchRegressionTicket -Comparison <result> -Decision <Z2-D-NNN> -Study <dir> -OutFile <path>` writes a complete ten-section ticket body from `Templates/RegressionTicket.md.tmpl` and prints the command that files it; it never writes to the board itself:

```
zagent create --project Z2 --category Zenith2 --type TASK --title "Re-rule <D-NAME or title>: research revalidation regression (<trigger>)" --file <OutFile> --complexity MODERATE --risk MEDIUM --points 3 --label needs-human
```

- The body's front-matter routing for the filer: epic the owning area's (from `Zenith2/Docs/Design/RESEARCH/Instances.md` → "Open decision instances"), sprint and fix version the current stage's, `kind: decision`, the template lines of the original ruling with `Deviations: none`, `Revalidation triggers:` copied.
- The Goal names the superseded entry, the trigger and both runs; the DoD carries the two template DoD lines; `## Dependencies` lists the tickets whose `Decisions:` line cites the entry, found by a read-only scan of `Zenith2/Docs/Board/tickets/*.md`, so the human can apply `needs-revalidation` with `deferred` to them (Z2-D-102).
- A budget-row regression is never filed this way: it is a BR-ACCEPT BUG under `Zenith2/Docs/Budgets.md` → "Breach consequences" ([Z2-N-BUDGET-040]).

## CI procedure

### Which benchmarks run in which gate

| Gate or run | What runs | Evidence it produces | Who |
|---|---|---|---|
| `Test` step, every ticket and the CI union (Z2-D-031) | every `zenith_add_benchmark` harness's `--smoke` CTest (registered by the helper, `Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites"); every `Pwsh_Research_*` case; `Pwsh_Research_CheckCommitted`, `Pwsh_Research_CheckDecisionLog`, `Pwsh_Research_CheckInstanceMap`, `Pwsh_Research_StudyFilesValid` | pass or fail only; a smoke run's timings are never evidence ([Z2-N-BUDGET-125]) | the loop and CI |
| `Coverage` step | `Test-Zenith2DecisionTicket -Mode board` over the board (hand-off P-RS1); in acceptance mode, the existence of every research ruling of the stage (hand-off P-RS2) | pass or fail | the loop and the acceptance run |
| `zagent guard` | the protected paths of P-RS6 | refusal of a loop diff under `runs/` or `Machines/` | the loop |
| mid-sprint integration check of each stage sprint, Stage 1 included (`Zenith2/Docs/Roadmap.md` → "Integration cadence") | `Update-Zenith2ResearchMachineState` for both platforms; `Get-Zenith2ResearchRevalidation`; one revalidation run per fired trigger whose decision governs a ticket of the next stage | committed revalidation runs and `revalidations.json` entries | the human (`needs-human`) |
| research ruling, run and revalidation tickets | `Run-Research.ps1` on the reference hardware, `New-Zenith2ResearchReport`, the DecisionLog entry | committed run directories, reports and entries | the human (`needs-human`, Z2-D-084) |
| stage acceptance (`Zenith2Acceptance`, Z2-D-083) | `Get-Zenith2ResearchRevalidation -Stage N -Final`; `Invoke-ResearchCheck.ps1 -Mode committed -CurrentInputs` | pass or fail, and the `INFO` input changes | the human |

No benchmark timing is ever compared with a threshold inside the loop or in CI: the gate hosts are not reference hardware ([Z2-N-BUDGET-125]), and a gate that passed or failed on WARP timings would be noise. The only research data a gate judges is committed data, which the human produced on the reference hardware.

### Regression thresholds

| What regressed | Threshold | Consequence |
|---|---|---|
| a primary verdict of a research conclusion on re-measure | any change of verdict, or of the rule's outcome | `regressed`: a re-ruling ticket ("Regression filing") |
| a primary shift on re-measure, verdict unchanged | moved by more than `max(δ, half the original gain interval width)` | `stands-drifted`: recorded, reported in the study's next report |
| a committed research file | any check finding | the `Test` step fails, so the ruling or run diff cannot land |
| a budget row | `Zenith2/Docs/Budgets.md` → "Breach consequences" | Budgets' BUG filing, not this procedure |

### Registration in the test registry

The RESEARCH block of `Zenith2/CMake/Tests/ZenithCMakeTests.cmake` holds one line per case of `Zenith2/Docs/Design/RESEARCH/Tests.md`, through SHIP's function (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "zenith_register_script_test"):

```cmake
zenith_register_pwsh_test(Pwsh_Research_MedianIntervalTable SCRIPT Zenith2/Benchmarks/Research/Tests/Zenith2Research.Tests.ps1 ARGS -Case MedianIntervalTable LABELS research)
zenith_register_pwsh_test(Pwsh_Research_CheckCommitted SCRIPT Zenith2/Benchmarks/Research/Invoke-ResearchCheck.ps1 ARGS -Mode committed -Repo ${ZENITH_REPO_ROOT} LABELS research TIMEOUT 600)
```

The function registers in the `win` preset alone, so the research tests run in the Windows `Test` step locally and in CI's Windows job; they need no build output and no LFS object.

## Hand-offs to PROC

Gate and protected edits are PROC `needs-human` work; RESEARCH states what it needs and PROC's tickets own the edits (cross-area row RS7). PROC's design does not yet list them, so RS7 also asks for a PROC patch row naming them.

| # | Edit | Why |
|---|---|---|
| P-RS1 | the `Coverage` step runs `Test-Zenith2DecisionTicket -Mode board` over every board ticket and fails on any finding (the `Kind:` trailer it selects on is already PROC's production-filer design) | the template and WORKER-EVIDENCE text rules are enforced on the board, where the loop claims from |
| P-RS2 | the `Coverage` step's acceptance mode `-Stage N` fails when a requirement of classification `research` of a stage ≤ N has no DecisionLog entry carrying **Research:** among its tickets' rulings, and runs `Invoke-ResearchCheck.ps1 -Mode decisionlog` | research coverage is evidence, not only tickets |
| P-RS4 | before the first Stage 1 research run: `Zenith2/.gitattributes` gains the `zenith-research` block of `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Large-file layout"; a committed repository-root `.lfsconfig` holds `[lfs]` `fetchexclude = Zenith2/Docs/Research`; git-lfs is installed on the reference desktop, and the setup script runs `git lfs install` and `git config filter.lfs.required true` | LF text in git, LFS for `archive/`; a clone whose git-lfs binary is missing fails `git add` instead of committing a trace as a plain blob (a clone that never installed git-lfs is caught by the runner's precondition and by the git-only pointer check); and no lane, CI clone, setup `git lfs pull` or gate host downloads a research object, because `.lfsconfig` applies to every clone and worktree alike |
| P-RS5 | the mid-sprint integration check ticket of every stage sprint, Stage 1 included, lists the three research steps of "Which benchmarks run in which gate" | the revalidation triggers are evaluated every stage |
| P-RS6 | `Zenith2/Docs/Research/**/runs/**` and `Zenith2/Docs/Research/Machines/**` join `policy.board.guardrails.protectedPaths` and `policy.resolved.guardrails.protectedPaths` in `vault.config.json` (and the documentation copy in `zagent.project.json`), by the same route and harness regeneration as Z2T-PROC-001's `Zenith2/Tools/Gates/**` | WORKER-EVIDENCE at the diff: a loop ticket can never commit a run or register a machine; `needs-human` tickets are never claimed by the loop, so the guard stops exactly what the rule forbids. P-RS6 covers the RESEARCH layout only: Budgets evidence committed outside it (`Budgets/Stage<N>/<workload>/` of [Z2-N-BUDGET-101], DEVT's Stage 3 records) is guarded by its own fingerprint check (`Test-Zenith2EvidenceRecord`, [Z2-N-BUDGET-099]) and by "Decision ticket check" step 4, not by a protected path, because `Budgets/Stage1/<AREA>/study.json` must stay writable by harness tickets |

The drafts-side template rule is not a PROC hand-off: the session checker under `C:/dev/Zenith2-drafts/checker/` is an orchestrator tool outside the repository, so the orchestrator adds a Node `decision-template` rule written from `Zenith2/Docs/Design/RESEARCH/Formats.md` → "Template lines" if it wants the drafts checked before filing.
