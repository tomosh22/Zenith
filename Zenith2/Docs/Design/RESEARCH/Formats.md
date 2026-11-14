# RESEARCH design: formats, retention and templates

This file is part of the RESEARCH design family; the entry document is `Zenith2/Docs/Design/RESEARCH.md`. It specifies every file the methodology of `Zenith2/Docs/Design/RESEARCH/Methodology.md` reads or writes: the research tree, the study protocol, the run record, the series, trace and raw files, the analysis output, the size caps and the text rule, the large-file tier, retention, versioning, the report template, the decision ticket template, the DecisionLog research fields and the revalidation log.

## Scope of this file

- Raw research data is committed under `Zenith2/Docs/Research/` (Z2-D-064, [Z2-N-BUDGET-079]): every measured value, every per-frame trace and every capture of a run is in git. Text within the caps is ordinary git content; binary captures and text over the per-file cap are in the large-file tier, git LFS objects under the run's `archive/` directory ([Z2-N-BUDGET-177]: large binary content is LFS-tracked). Nothing a run produced lives outside the repository.
- Ruling Q55 governs what a worker's diff can carry, and permits a binary artefact recorded by a `needs-human` ticket; every research run and ruling ticket is `needs-human`, and no worker produces any file of this format family (WORKER-EVIDENCE, `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Profile rule table").
- Every format here is text: UTF-8 without a byte-order mark, LF line endings, and a final newline. JSON files are canonical JSON ("Canonical JSON"); CSV files follow "Series files" and "Trace files". No format here holds a floating-point number (`Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Arithmetic rules").
- Formats owned elsewhere are carried as raw files with their line endings normalised ("Raw files"): the Budgets evidence record and per-frame CSV (`Zenith2/Docs/Budgets.md` → "Evidence records and the input fingerprint"), the Budgets allocation file (`Zenith2/Docs/Budgets.md` → "Allocation file and check"), and each harness's own report schema (`zenith2.bench.<name>.v1`).

## Research tree layout

```
Zenith2/Docs/Research/
  Index.md                               the list of studies, one row each (study, profile, decision, latest report)
  Machines/<machineId>.json              one file per registered reference machine
  Machines/<machineId>.calibration.json  the desktop calibration ("Desktop calibration" of the methodology)
  Machines/current.json                  the reference machines and their software state now
  Workloads/<W>.md                       workload scale definitions (Budgets' format, [Z2-N-BUDGET-195]); not a study
  Walkthroughs/<RowName>.md              walkthrough scripts (Budgets' format, [Z2-N-BUDGET-326]); not a study
  <Study>/
    study.json                           the protocol (harness ticket)
    runs/<RunId>/                        one directory per run ("Run directory")
    Reports/<Z2-D-NNN>.md                one report per ruling that used the study ("Report template")
    revalidations.json                   the revalidation log ("revalidations.json")
    allocation.json                      profile RB only, Budgets' allocation file, where Budgets places it
```

### Study directory

- `<Study>` is one or more path segments matching `^[A-Z][A-Za-z0-9]{1,47}$` each, so a study can sit where Budgets already places its data: `Budgets/Stage1/CORE` (profile RB, [Z2-N-BUDGET-031]), `Compression/Stage1` ([Z2-N-BUDGET-235]), `ShadingPaths`, `ECS`, `HeapSelection`, `Physics2DSelection`.
- A path segment named `runs`, `Reports`, `Machines`, `Workloads`, `Walkthroughs` or `archive` is reserved and never a study name.
- Path lengths: the study path (its segments joined by `/`) is at most 64 characters, and every file under a run directory has a repo-relative path of at most 200 characters; the check reports either as `RESEARCH_CHECK_PATH_LENGTH`, so a lane root of up to 60 characters plus the path stays inside the 260-character Windows path limit without long paths enabled.
- Where a core document or an earlier design writes a date segment (`ShadingPaths/<date>/` in [Z2-N-REND-958], `ECS/<date>/`), the run directory `runs/<RunId>/` is that directory: the RunId begins with the run's UTC date.

### Run directory

```
<Study>/runs/<RunId>/
  record.json                            the run record ("record.json")
  series/<cell>.<metric>.csv             one series per (cell, metric) ("Series files")
  raw/<relative path>                    harness outputs and evidence records within the per-file cap ("Raw files")
  archive/<name>                         the large-file tier: traces, Budgets per-frame CSVs, binary captures, over-cap text (LFS)
  analysis.json                          generated ("analysis.json")
```

- RunId: `<YYYYMMDD>T<HHMM>Z-<win|s24>-<first 8 hex of machineId>`, the UTC start of the session, for example `20261102T0910Z-win-3f9a1c2b`.
- Nothing else is allowed in a run directory; `Invoke-Zenith2ResearchCheck` reports any other file as `RESEARCH_CHECK_STRAY_FILE`.
- Placement is by kind, not by remaining space: `record.json`, `analysis.json`, the series and every text harness output within the per-file cap are ordinary git files; every trace, every Budgets per-frame CSV, every binary capture and every text file over the per-file cap is in `archive/`.

### Machines registry

`Machines/<machineId>.json`:

```json
{
	"schema": "zenith2.research-machine",
	"schemaVersion": 1,
	"machineId": "3f9a1c2b7d10",
	"kind": "desktop",
	"identity": { "board": "ASUSTeK COMPUTER INC. TUF GAMING X570-PLUS", "cores": "8", "cpu": "AMD Ryzen 7 5800X 8-Core Processor", "gpu": "NVIDIA GeForce RTX 3070", "gpuMemMiB": "8192", "ramGiB": "32", "threads": "16", "uid": "9c41e0d27a5f3b18" },
	"registeredUnixMs": 1793606400000,
	"referenceFor": ["Win"]
}
```

- `identity` holds strings only, keys in ordinal order (`[StringComparer]::Ordinal`, the UTF-16 code unit order, which for these ASCII keys is byte order). The `machineId` is the first 12 lowercase hex digits of the SHA-256 of the lines `key=value\n` of `identity` in that order, UTF-8.
- Desktop keys and sources: `cpu` (`Win32_Processor.Name`, trimmed), `cores` (`NumberOfCores`), `threads` (`NumberOfLogicalProcessors`), `ramGiB` (`Win32_ComputerSystem.TotalPhysicalMemory` ÷ 2^30, rounded to nearest), `gpu` and `gpuMemMiB` (`nvidia-smi --query-gpu=name,memory.total`), `board` (`Win32_BaseBoard` `Manufacturer` and `Product`, one space), `uid` (the first 16 hex of the SHA-256 of `Win32_ComputerSystemProduct.UUID`).
- S24 keys: `model` (`ro.product.model`), `soc` (`ro.soc.model`), `uid` (the first 16 hex of the SHA-256 of `ro.serialno`).
- No raw serial number, UUID, user name, host name or absolute path outside the repository is ever written to any research file; identifiers are hashed as above.

`Machines/current.json` names the registered reference machine per platform and its software state when the last mid-sprint integration check recorded it:

```json
{
	"schema": "zenith2.research-machines-current",
	"schemaVersion": 1,
	"platforms": [
		{ "platform": "Win", "machineId": "3f9a1c2b7d10", "gpuDriver": "566.36", "gpuDriverMajor": "566", "osBuild": "26100.2605", "osFamily": "26100", "recordedUnixMs": 1793606400000 },
		{ "platform": "S24", "machineId": "a1b2c3d4e5f6", "gpuDriver": "512.762.0", "gpuDriverMajor": "512", "osBuild": "UP1A.231005.007", "osFamily": "14", "recordedUnixMs": 1793606400000 }
	]
}
```

## study.json

```json
{
	"schema": "zenith2.research-study",
	"schemaVersion": 1,
	"study": "HeapSelection",
	"studyVersion": 1,
	"profile": "RS",
	"decision": "D-SEL-HEAP",
	"stage": 1,
	"hypothesis": "mimalloc raises allocation throughput of the W-Dense and W-Characters traces beyond the run-to-run spread on both reference platforms without raising S24 process RSS",
	"alternatives": [
		{ "id": "system", "text": "the system allocator", "incumbent": true },
		{ "id": "mimalloc", "text": "mimalloc behind Zenith_Heap", "incumbent": false }
	],
	"platforms": [ { "platform": "Win", "backends": [null] }, { "platform": "S24", "backends": [null] } ],
	"conditions": "controlled",
	"repetitions": 10,
	"scheduleSeed": "11400714819323198485",
	"familyBy": "study",
	"dependencies": ["mimalloc"],
	"comparesHarnesses": [],
	"sessions": [],
	"cells": ["…"],
	"metrics": ["…"],
	"comparisons": ["…"],
	"definitions": {},
	"rule": "beyondSpread(mimalloc, system, wdense-throughput) and beyondSpread(mimalloc, system, wchar-throughput) and notWorse(mimalloc, system, s24-rss) and meets(mimalloc, Z2-B-CORE-001, s24-rss) and meets(mimalloc, Z2-B-CORE-004, heap-residual)",
	"ruleNote": "",
	"outcomeWhenFalse": "keep(system)",
	"revalidation": ["dependency:mimalloc", "os:android"],
	"overrides": []
}
```

### study.json fields

| Field | Type | Rule |
|---|---|---|
| `schema`, `schemaVersion` | string, integer | exactly `zenith2.research-study`, 1 |
| `study` | string | equals the study's path under `Zenith2/Docs/Research/` |
| `studyVersion` | integer ≥ 1 | incremented for a changed protocol (`Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Study design and pre-registration") |
| `profile` | string | `RA`, `RS`, `RJ`, `RB` or `RP` (an RQ ruling has no study) |
| `decision` | string | the D-NAME, or the `Z2-D-NNN` of a provisional justification record (RJ), or the budget producer name (RB) |
| `stage` | integer 0 … 7 | the stage whose acceptance the ruling serves |
| `hypothesis` | string, 20 … 400 characters | one sentence |
| `alternatives` | array, ≥ 2 | `id` (cell candidate token), `text`, `incumbent` (boolean); at most one `incumbent: true`; with none, `outcomeWhenFalse` is `human(…)` or `blocked(…)` or the rule is an ordered rule ending `true -> <outcome>`, and `keep(…)` is refused |
| `platforms` | array | `platform` `Win` or `S24`; `backends` the backend tokens measured there (`vulkan`, `d3d12`, or `null` for a backend-free cell) |
| `conditions` | string | `controlled`; `ordinary` only for profile RB under a procedure that names ordinary conditions, and for profile RP when the probed behaviour is the device's own adaptation (thermal headroom), which controlled conditions would suppress |
| `repetitions` | integer 10 … 20 | launches per cell; RP: 1, or 3 for `flaky-device` cases |
| `scheduleSeed` | decimal string of a uint64 | the seed of the schedule algorithm |
| `familyBy` | string | `study` (default), `scenario` or `scenario+metric` ("Multiplicity" of the methodology) |
| `dependencies` | array of manifest entry names | recorded per run with their versions |
| `comparesHarnesses` | array | `{ a, b, justification }`: the launch-target pairs a comparison may cross, justification 20 … 200 characters ("Confound check") |
| `sessions` | array of arrays of cell ids | empty: one session holds every cell; otherwise each inner list is one session, every comparison's two cells sit in one session, and every cell in exactly one |
| `cells`, `metrics`, `comparisons` | arrays | the tables below |
| `definitions` | object | name (`^[a-z][a-z0-9-]{0,31}$`) to rule expression, expanded by `@<name>` in `rule` ("Rule grammar" of the methodology); `{}` when unused |
| `rule`, `ruleNote`, `outcomeWhenFalse` | strings | the grammar of `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Rule grammar" |
| `revalidation` | array of trigger tokens | the vocabulary of `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Trigger vocabulary"; copied into the DecisionLog entry |
| `overrides` | array | `{ comparison, field, value, justification }`: a δ_rel override, a per-cell timeout or cooldown allowance; the justification is 20 … 400 characters |

### Cell fields

Shown wrapped for reading; in `study.json` each cell is one compact object ("Canonical JSON").

```json
{ "id": "wdense-mimalloc", "candidate": "mimalloc", "scenario": "wdense", "api": null, "backend": null, "variant": null,
  "launch": { "kind": "bench-host", "target": "Zenith_Bench_HeapSelection", "args": ["--candidate", "mimalloc", "--trace", "wdense", "--repetitions", "1", "--out", "{out}"] },
  "timeoutSeconds": 900, "estimatedSlotSeconds": 120, "cooldownAllowanceSeconds": 300,
  "notApplicable": [], "exclusions": [] }
```

| Field | Rule |
|---|---|
| `id` | `^[a-z0-9][a-z0-9-]{0,63}$`, unique in the study |
| `candidate`, `scenario`, `api`, `backend`, `variant` | tokens of the same pattern or `null`; `candidate` names an alternative `id` |
| `launch.kind` | `bench-host` (a Windows benchmark executable from the `win-release` tree), `bench-device` (the same target's hosted executable APK on the S24, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Hosted executable APKs"), `test-native` (a library's `_Tests` executable with `--bench-native`, ECS's custom native cells), `workload` (`Zenith2/Benchmarks/Run-Workload.ps1` with `-Repetitions 1 -FirstRepetition <r> -Conditions controlled -TimeoutSeconds <timeoutSeconds>`, `Zenith2/Docs/Design/DEVT/Evidence.md` → "Workload runner"), or `probe` (a probe executable with one case per launch) |
| `launch.target` | `bench-*` and `probe`: the executable `Zenith_Bench_<Name>`, where `<Name>` is a `name` that `declared_deps.json`'s `benchmarks[]` lists (the `zenith_add_benchmark(<Name> …)` argument); `test-native`: `<Lib>_Tests`; `workload`: `<Game>/<W>` |
| `launch.args` | strings; placeholders `{out}` (the slot's output path), `{repetition}`, `{slot}`, `{seed}` (the slot's 64-bit seed: the SplitMix64 draw number `slot` of a generator seeded with `scheduleSeed` XOR 0x5A5A5A5A5A5A5A5A), `{backend}` |
| `timeoutSeconds` | 30 … 7,200; for `workload` it is passed as `-TimeoutSeconds`, and the unmeasured warm-cache launch `Run-Workload.ps1` adds when no cache exists counts outside the slot's measured values but inside its timeout |
| `estimatedSlotSeconds` | 1 … 7,200, the human's estimate of one launch for the session plan |
| `cooldownAllowanceSeconds` | default 300 (desktop) or 600 (S24); a value of 30 or more set through `overrides` with a justification |
| `notApplicable` | platforms where the cell does not run, with a reason of 10 … 200 characters |
| `exclusions` | capability-matrix exclusions that apply (shading paths), strings |

### Metric fields

Shown wrapped for reading, like the cell above.

```json
{ "id": "wdense-throughput", "cell": "wdense-mimalloc", "unit": "bytes_per_s", "class": "throughput", "direction": "higher", "statistic": "median",
  "source": { "kind": "harness-json", "file": "{out}", "select": { "array": "/cases", "match": { "name": "wdense" }, "value": "/medianBytesPerSecond" } } }
```

A metric entry binds one series; the same metric `id` on several cells (the same quantity on each candidate) is written once per cell. `unit` is `ns`, `bytes`, `count`, `bytes_per_s`, `micro` or `pass`; `class` one of `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Metric classes and thresholds"; `statistic` the per-launch statistic of → "Per-launch statistic", applied to the launch's raw measurements (an array source, a trace) and `single` for a source that yields one value.

### Comparison fields

```json
{ "id": "throughput-wdense", "candidate": "wdense-mimalloc", "baseline": "wdense-system", "metric": "wdense-throughput", "factor": "candidate", "primary": true }
```

The family key of a primary comparison follows from `familyBy` and the cells' `scenario` and the `metric`; `analysis.json` records it.

### Extraction selectors

The runner turns each launch's harness output into one series value per metric with a selector, so no harness needs code for the research format.

| `source.kind` | Fields | Value |
|---|---|---|
| `harness-json` | `file`; `select.array` (a JSON pointer, RFC 6901, to an array); `select.match` (an object of member names to string or integer values, every one equal); `select.value` (a pointer relative to the matched element) | the integer at `value`; zero or several matches, or a value that is not an integer lexeme, is `EXTRACT` |
| `harness-json-array` | as `harness-json`, `select.value` pointing at an integer array | the launch's internal repetitions, reduced to one value by the metric's `statistic`; the array is kept in the raw file |
| `evidence-row` | `row` (a `Z2-B-` ID) | the `measured` lexeme of that row's entry in the launch's Budgets evidence record, converted to the metric's unit by the exact decimal rules below |
| `frames-csv` | `column` (`frameMs`, `cpuSimMs`, `cpuRenderMs`, `gpuMs`) | the metric's statistic over the non-excluded rows of the launch's per-frame CSV ([Z2-N-BUDGET-104]) |
| `probe-case` | `case` | 1 when the probe report lists the case passed, 0 when failed; absent is `EXTRACT` |

- Every JSON source is parsed with `[System.Text.Json.JsonDocument]::Parse([System.ReadOnlyMemory[byte]]::new($bytes))` after a leading UTF-8 byte-order mark `EF BB BF` is removed (rule P1 of `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "PowerShell rules of the module"); nesting deeper than 64 levels is `EXTRACT`. It is walked through `EnumerateObject()` and `EnumerateArray()`; a number is read only as its lexeme, `JsonElement.GetRawText()`, and never through `GetInt64`, `GetDouble` or `ConvertFrom-Json`, so no value passes through `[double]` and no string becomes a `[datetime]`. A JSON object with a repeated member name is `EXTRACT` (the parser accepts duplicates, so the walk checks for them).
- An integer lexeme (`0` or `[1-9][0-9]*`, at most 19 digits) becomes a `BigInteger` with `BigInteger::Parse(<lexeme>, NumberStyles.None, CultureInfo.InvariantCulture)`; a sign, an exponent or a lexeme above 2^63 − 1 is `EXTRACT`.
- Exact decimal conversion of a lexeme with a fraction (`15.9`, `16.712`, `15.900`): split at the one `.`; the fraction may have at most the unit's decimals (6 for ms to ns, 6 for MB to bytes, 9 for GB to bytes); the value is the integer part × 10^d plus the fraction padded with zeros to d digits; more decimals than the unit allows, a sign or an exponent is `EXTRACT`. `16.712` ms becomes 16,712,000 ns; `15.9` ms 15,900,000 ns; `16.7125` ms 16,712,500 ns; `16.7125001` ms (seven decimals) is `EXTRACT`.
- JSON pointers use the `~0` and `~1` escapes; array indices are decimal.

## record.json

```json
{
	"schema": "zenith2.research-record",
	"schemaVersion": 1,
	"study": "HeapSelection",
	"studyVersion": 1,
	"studySha256": "<64 hex: SHA-256 of study.json as read>",
	"runId": "20261102T0910Z-win-3f9a1c2b",
	"profile": "RS",
	"platform": "Win",
	"machine": { "machineId": "3f9a1c2b7d10", "gpuDriver": "566.36", "osBuild": "26100.2605", "bios": "4801", "vulkanDriver": "566.36.0.0", "d3d12Driver": "32.0.15.6636", "androidFingerprint": null, "androidSdk": null, "kernel": null },
	"build": {
		"treeSha": "<40 hex>",
		"preset": "win-release",
		"configuration": "Release",
		"dependencies": [ { "entry": "mimalloc", "version": "2.1.7", "sourceSha256": "<64 hex>" } ],
		"toolchainHash": "<64 hex>",
		"thirdPartyHash": "<64 hex>"
	},
	"conditions": { "class": "controlled", "desktop": { "powerScheme": "Zenith2 Research", "processorMinPct": 99, "processorMaxPct": 99, "processorPerformanceMaxPct": 100, "gpuCoreMHz": 1500, "gpuMemMHz": 7001, "idleCpuPermille": 11, "idleGpuPct": 0, "ambientC": 22, "display": "2560x1440@144", "displayChecked": true, "updatesPaused": true, "bootAgeSeconds": 1260 }, "android": null },
	"session": { "bootId": "1793605800", "startUnixMs": 1793606400000, "endUnixMs": 1793639000000, "buildSlotHeld": true, "lanesIdle": true, "restored": true, "resumed": 0, "cells": ["wdense-system", "wdense-mimalloc"] },
	"procedure": {
		"repetitions": 10,
		"warmup": 1,
		"scheduleSeed": "11400714819323198485",
		"order": [ { "slot": 1, "cell": "wdense-system", "repetition": 0 } ],
		"retries": []
	},
	"cells": [
		{ "cell": "wdense-system", "inputs": { "launchTarget": "Zenith_Bench_HeapSelection", "closure": "<64 hex>", "toolchain": "<64 hex>", "thirdParty": "<64 hex>", "shaderBlobs": null, "content": null, "workloadDefinition": null, "harness": "Zenith_Bench_HeapSelection/1", "qualityKeys": [], "command": "Zenith_Bench_HeapSelection --candidate system --trace wdense --repetitions 1 --out {out}" }, "notApplicable": false, "exclusions": [] }
	],
	"series": [
		{ "cell": "wdense-system", "metric": "wdense-throughput", "file": "series/wdense-system.wdense-throughput.csv", "sha256": "<64 hex>", "valid": 10, "invalid": 0 }
	],
	"preflight": [ { "check": "D3", "slot": null, "value": "11", "limit": "30", "pass": true, "atUnixMs": 1793606410000 } ],
	"slots": [
		{ "slot": 1, "cell": "wdense-system", "repetition": 0, "startUnixMs": 1793606700000, "endUnixMs": 1793606760000, "exit": 0, "startTempC": 41, "endTempC": 47, "clockEvents": 0, "thermalStart": null, "thermalEnd": null, "batteryDeciC": null, "charging": null, "valid": true, "reason": "" }
	],
	"raw": [
		{ "file": "raw/wdense-system.r01.json", "sha256": "<64 hex>", "bytes": 4210, "sourceSha256": "<64 hex>", "schema": "zenith2.bench.heap-selection.v1" }
	],
	"archive": [
		{ "file": "archive/wdense-system.r01.trace.csv", "sha256": "<64 hex: the LFS oid>", "bytes": 698112, "kind": "research-trace", "sourceSha256": null }
	],
	"deviations": []
}
```

### record.json fields

| Field | Rule |
|---|---|
| `machine` | software state of the run, strings or `null`: Windows `gpuDriver` (`nvidia-smi --query-gpu=driver_version`), `osBuild` (`CurrentBuild` + `.` + `UBR` of `HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion`), `bios` (`Win32_BIOS.SMBIOSBIOSVersion`), `vulkanDriver` and `d3d12Driver` (from the first slot's harness or Player device report, `null` when the cell set has no GPU cell); S24 `androidFingerprint` (`ro.build.fingerprint`), `androidSdk` (`ro.build.version.sdk`), `kernel` (`uname -r`), `gpuDriver` (the Vulkan `driverVersion` the harness reports) |
| `build` | `treeSha` from `Get-Zenith2TreeSha` of a clean tree (a dirty tree refuses the run); `preset` `win-release` or `android-arm64`; `configuration` `Release` ([Z2-N-BUDGET-108]); `dependencies` the study's `dependencies` resolved from `Zenith2/ThirdParty/manifest.json`; the two manifest hashes from SHIP's `Get-Zenith2ManifestHashes` |
| `conditions` | `class` and exactly one non-null platform object; Android: `{ fixedPerformance: "on" | "unavailable", airplane: true, brightness: 128, batteryStartPct, batteryStartDeciC, thermalStart, ambientC }` |
| `session` | `bootId` per `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Sessions"; `buildSlotHeld` and `lanesIdle` true for a desktop run; `resumed` the number of same-boot resumptions; `cells` the session's cells (one `sessions` entry of the study, or every cell) |
| `procedure.order` | the full schedule, retry slots included where they were inserted |
| `cells[].inputs` | the confound-check inputs: `launchTarget`, the seven fingerprint members from `Get-Zenith2EvidenceFingerprint` for the cell's executable target (`null` members where the cell has no workload or content, cross-area row RS6), `qualityKeys`, `command` |
| `slots` | one entry per executed slot, in execution order; `clockEvents` the number of R5 samples reporting an active clock event |
| `raw`, `archive` | every committed file of those directories: `sha256` and `bytes` of the committed bytes (for `archive/`, the LFS oid and size), and `sourceSha256` of the harness's original bytes when normalisation changed them ("Raw files"), else `null` |
| `deviations` | `{ rule, description, justification }` for every departure from `study.json` or from this methodology; a non-empty list is shown in the report's "Method" section |

### Invalidity reasons

| Code | Meaning |
|---|---|
| `WARMUP` | slot group 0, never a statistic input |
| `THERMAL` | a thermal check failed (R1, S1, S3) |
| `CLOCKS` | the desktop clock locks drifted (R2) or a clock event was active during the slot (R5) |
| `BACKGROUND` | the background-load check failed (R3) |
| `BATTERY` | the S24 battery level fell below the limit (S2) |
| `EXIT` | the launch exited non-zero, timed out, or its output was missing or truncated (R4, S4) |
| `DEVICE` | the GPU required a reset (R4), or the device disconnected |
| `EXTRACT` | a selector found no unique integer |
| `EVIDENCE` | the launch's Budgets evidence record is `invalid` |
| `OPERATOR` | the operator aborted the slot (`Ctrl+C` during a slot marks it and stops the session) |

## Series files

`series/<cell>.<metric>.csv`:

```
# zenith2.research-series v1
# cell=wdense-system metric=wdense-throughput unit=bytes_per_s class=throughput direction=higher statistic=median
repetition,slot,valid,reason,value,withinCount,withinMin,withinP50,withinP95,withinP99,withinMax
0,1,0,WARMUP,812345678,,,,,,
1,3,1,,815002113,,,,,,
```

- Line 1 is exactly `# zenith2.research-series v1`; line 2 carries the six `key=value` pairs in that order, values matching the study; line 3 is exactly the header shown.
- One row per executed slot of the cell, in slot order, warm-up and invalid rows included. A slot is one launch, so a row is one launch; `repetition` is 0 for the warm-up and 1 … n otherwise, `slot` the schedule slot, `valid` 0 or 1, `reason` an invalidity code or empty, `value` a decimal integer without sign or leading zeros (`0` alone allowed), or empty for an `EXIT` row.
- The `within*` columns hold the count and the nearest-rank statistics of the launch's raw measurements (its trace frames, or its internal repetitions) when the source is an array or a trace, and are empty otherwise.
- Each repetition 1 … n appears with `valid` 1 at most once; a retried repetition has earlier rows with `valid` 0.

## Trace files

`archive/<cell>.r<NN>.trace.csv`, the measured-interval frames of one frame-workload launch, derived from the Budgets per-frame CSV by `ConvertTo-Zenith2ResearchTrace` (`<NN>` the two-digit repetition, `00` for the warm-up):

```
# zenith2.research-trace v1
# cell=forward-plus-windoor-vulkan repetition=3 source=archive/forward-plus-windoor-vulkan.r03.frames.csv
frame,frameNs,cpuSimNs,cpuRenderNs,gpuNs
612,16712000,5123000,3456000,12345000
```

- Only rows with `excluded = 0` are written; each millisecond value becomes integer nanoseconds by the exact decimal conversion of "Extraction selectors".
- Traces are always in `archive/` (the large-file tier); the series rows hold every per-launch statistic, so the analysis reads the trace only to recompute those statistics when the LFS object is present.
- The trace duplicates the per-frame CSV's non-excluded rows and is kept anyway: the exclusion filter and the exact decimal conversion run once, at assembly, by the runner that read the source, so every later recomputation reads integer nanoseconds and cannot apply a different conversion. Its cost is counted in "Volume per study".

## Raw files

- `raw/` holds every text output a selector read that is within the per-file cap, named `<cell>.r<NN>.<extension>`, plus for `workload` cells the Budgets evidence record (`<cell>.r<NN>.evidence.json`, schema `zenith2.evidence.v1`). A text output over the cap, every Budgets per-frame CSV (`<cell>.r<NN>.frames.csv`) and every binary output go to `archive/` under the same naming.
- A raw text file is byte-identical to the harness's output except for line endings: on copy the runner removes a leading UTF-8 byte-order mark, replaces every `\r\n` by `\n` and appends a final `\n` when it is missing, records `sha256` and `bytes` of the committed bytes, and records `sourceSha256` of the original bytes when they differ. Binary outputs are copied unchanged. Harnesses write their reports in binary mode with `\n` endings (cross-area row RS2), so normalisation is the exception.
- Allowed extensions under `raw/`: `.json`, `.csv`, `.txt`, `.md`. Under `archive/`: those, plus `.png`, `.exr` (images), `.gfxtrace`, `.csv.counters` (GPU counter captures as their tools write them) and `.log`.

## analysis.json

Generated by `Invoke-Zenith2ResearchAnalysis` from `record.json` and the series files; never edited by hand.

```json
{
	"schema": "zenith2.research-analysis",
	"schemaVersion": 1,
	"study": "HeapSelection",
	"runId": "20261102T0910Z-win-3f9a1c2b",
	"tool": "Zenith2Research/1",
	"series": [
		{ "cell": "wdense-system", "metric": "wdense-throughput", "n": 10, "median": 815002113, "low": 812990001, "high": 817100450, "j": 2, "k": 9, "coverageNum": 501, "coverageDen": 512, "rcvPpm": 1933, "min": 811000000, "max": 818500000, "driftInversions": 21, "driftPNum": 3628800, "driftPDen": 3628800, "drift": false, "state": "OK", "reasons": [] }
	],
	"comparisons": [
		{ "id": "throughput-wdense", "primary": true, "familyKey": "study", "family": 4, "k": 18, "crossHarness": false, "shift": 30112000, "shiftLow": 25000000, "shiftHigh": 35000000, "gainLow": 25000000, "gainHigh": 35000000, "delta": 24450064, "relPpm": 36947, "pNum": 2, "pDen": 184756, "holmNum": 2, "holmDen": 46189, "cliffNum": 100, "cliffDen": 100, "significant": true, "verdict": "BETTER", "reasons": [] }
	],
	"rule": {
		"expression": "…",
		"terms": [ { "term": "beyondSpread(mimalloc, system, wdense-throughput)", "platform": "Win", "value": "true" } ],
		"outcome": null,
		"reasons": ["PARTIAL"]
	}
}
```

### Canonical JSON

Every JSON file this design writes is canonical, so regeneration is byte-identical. The grammar, with `TAB` one tab character per nesting level and `NL` one line feed:

```
document   := value NL
document   := object NL                                              ; the top-level object is always expanded
value      := object | compact | array | scalar
scalar     := string | integer | "true" | "false" | "null"
object     := "{" NL member { "," NL member } NL indent "}"         ; expanded: used when any member value is a non-empty array of objects or an object holding one
member     := indent TAB string ": " value
compact    := "{ " string ": " cvalue { ", " string ": " cvalue } " }" | "{}"   ; used when every member value is a scalar, an array of scalars, an empty array or a compact object
cvalue     := scalar | "[" [ scalar { ", " scalar } ] "]" | compact
array      := "[]" | "[" scalar { ", " scalar } "]"                 ; scalars inline, no inner spaces
            | "[ " compact { ", " compact } " ]"                     ; compact objects on one line, when that whole line is at most 160 characters
            | "[" NL indent TAB value { "," NL indent TAB value } NL indent "]"   ; one element per line: any element an expanded object, or the one-line form over 160 characters
string     := '"' { char | "\\\"" | "\\\\" | "\\n" | "\\t" | "\\u00" hex hex } '"'   ; other control characters as \u00XX; nothing else escaped
integer    := "0" | [ "-" ] digit19 { digit }                        ; decimal, no exponent, no fraction, no leading zero
```

- The 160-character test counts the whole output line: its leading tabs (one character each), the member name and `": "` when the array is a member value, and the trailing comma when one follows.
- Members appear in the order of their schema's field table in this file (the order of the examples, which are abbreviated only where they show `"…"`).
- Integers are formatted with `BigInteger.ToString(CultureInfo.InvariantCulture)`; no floating-point number is ever written (`ConvertTo-Zenith2CanonicalJson` throws on a `[double]`, a `[single]` or a `[decimal]`).
- A p-value is stored as `count / total`, unreduced; every other rational is reduced to lowest terms with a positive denominator. The normative example is the fixture `Zenith2/Benchmarks/Research/Tests/Fixtures/Json/expected.json`.

### analysis.json fields

| Field | Meaning |
|---|---|
| `series[].state` | `OK`, `NOISY` (warning band), `NOISY_INVALID`, `INSUFFICIENT`, `NONDETERMINISTIC`, `NOT_APPLICABLE` |
| `series[].low`, `high`, `j`, `k`, `coverageNum`, `coverageDen` | the median interval |
| `comparisons[].familyKey`, `family`, `k` | the family key, the number of primary comparisons in it and the rank used |
| `comparisons[].crossHarness` | the comparison crosses a `comparesHarnesses` pair |
| `comparisons[].delta`, `relPpm` | the margin and the shift relative to the baseline median |
| `comparisons[].verdict`, `reasons` | `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Verdicts"; reasons from `DRIFT`, `NOISY`, `UNDERPOWERED`, `CONFOUNDED`, `INSUFFICIENT`, `NONDETERMINISTIC`, `TIES_DISAGREE`, `SPREAD_DIFFERS`, `INVALID_RATE`, `ZERO_MEDIAN` |
| `rule` | `expression`; `terms`, each term this run can evaluate alone with its value per platform (`true`, `false`, `INVALID`); `outcome`, the rule's outcome when the rule needs no other run, otherwise `null`; `reasons`, `["PARTIAL"]` when `outcome` is `null`, else `[]`. The study-level outcome over all of a ruling's runs is computed by `Invoke-Zenith2ResearchStudyRule` and lives in the report's generated `analysis` block ("Generated blocks") |
| `classification` | RA ECS studies only: one object per (scenario, backend pair, EnTT variant) with `nativeVerdict`, `adapterVerdict`, `class`, `artefactShift`, `artefactLow`, `artefactHigh` |
| `adapter` | RA ECS studies only: per (scenario, backend[, variant]) `overheadShift`, its interval, `overheadPpm`, `sharePpm`, `shareLowPpm`, `shareHighPpm`, `reasons` (`ZERO_MEDIAN`) |

## Size caps and the text rule

| Item | Measured as | Provisional value used by the check until the row is adopted | Row |
|---|---|---|---|
| one committed non-LFS file in scope of the text rule | its committed byte size | 4,194,304 bytes (4 MiB) | the pending row RESEARCH-001 "Research committed file size cap" |
| one run directory, non-LFS files | the sum of the byte sizes of its files outside `archive/` | 33,554,432 bytes (32 MiB) | the pending row RESEARCH-002 "Research committed run directory size cap" |
| one study's large-file tier | the sum of `bytes` of every `archive[]` entry of every committed record of the study | 2,147,483,648 bytes (2 GiB), reported only | the pending row RESEARCH-003 "Research large-file storage per study" |

- The measurement is the procedure the rows name, `MP-RESEARCHSTORE`: `Invoke-ResearchCheck.ps1 -Mode committed` computes the three quantities from the committed files and records (for LFS files, from the pointer or object size), so it runs on any checkout with or without the LFS objects fetched; cross-document item X416 adds the procedure and the three rows (with the IDs RESEARCH-001 to RESEARCH-003 under the `Z2-B-` prefix), breach codes BR-TEST, BR-TEST and BR-REPORT.
- The rows are allocated by the "Stage 1 per-system budget ticket for RESEARCH", a single `needs-human` ruling with no harness half and basis `published` (the hosting service's per-file limits and the measured sizes of the Stage 1 runs); it is blocked by no other allocation. Until it lands, `Zenith2Research.psm1` uses the provisional values above, declared as the module constants `ulRESEARCH_PROVISIONAL_FILE_CAP`, `ulRESEARCH_PROVISIONAL_RUN_CAP` and `ulRESEARCH_PROVISIONAL_STUDY_LFS`; once a row is adopted the check reads its value from `Zenith2/Docs/Budgets.md` → "Row index" and the constant is unused. The ruling may confirm or change the values.
- Scope: the text rule, the per-file cap and the path lengths apply to the study directories (a directory holding `study.json`, with its `runs/<RunId>/` files outside `archive/`, `Reports/`, `revalidations.json` and `allocation.json`), to `Index.md` and to `Machines/**`. Every other file under `Zenith2/Docs/Research/` (another area's report, DEVT's `LinuxTsanHost/<utc>/junit.xml`, `Workloads/`, `Walkthroughs/`) is outside the RESEARCH check and governed by its owner; `RESEARCH_CHECK_STRAY_FILE` applies inside run directories only.
- The text rule: every committed non-LFS file in scope is valid UTF-8 with no NUL byte, no byte-order mark, LF line endings, a final newline, and one of the extensions `.json`, `.csv`, `.md`, `.txt`. The check fails any other file in scope (`RESEARCH_CHECK_NOT_TEXT`).

### Volume per study

Frame counts are the measured interval of [Z2-N-BUDGET-070] at the frame rates research runs reach: at least 120 s and 7,200 frames, uncapped on the desktop (144 to 300 fps, so 17,280 to 36,000 frames) and display-limited on the S24 (at most 120 Hz, 14,400 frames). A trace row is 36 to 40 bytes and a per-frame CSV row ([Z2-N-BUDGET-104]) 56 to 64 bytes, so one frame-workload launch commits a trace and a CSV of about 1.6 to 3.7 MB together on the desktop and about 1.4 MB on the S24.

| Study | Session plan | Slots | Large-file tier per run | Non-LFS per run |
|---|---|---|---|---|
| ShadingPaths, desktop | one session per backend: 4 paths × 2 workloads × 11 | 88 per session | 88 × (trace and CSV) 1.6 to 3.7 MB = 141 to 326 MB | about 1 MB (raw reports of about 10 KB each, series) |
| ShadingPaths, S24 | 2 paths × 2 workloads × 11 | 44 | 44 × 1.4 MB = about 64 MB | under 1 MB |
| D-TBDR, S24 | one session per workload: 3 configurations × 11 | 33 per session | 33 × 1.4 MB = 46 MB, plus counter captures of about 2 MB each = about 112 MB | under 1 MB |
| ECS, per platform | sessions by scenario (`sessions`), about 55 cells × 11 | 605 | none (no traces) | about 3 MB of raw reports |
| a micro-benchmark selection (HEAP, HASH, JSON) | one session | 20 to 60 | none | under 1 MB |

The heaviest study, ShadingPaths, holds about 350 to 720 MB in the large-file tier per full measurement (two desktop sessions and one S24 session), so the proposed 2 GiB per study covers the first ruling and one to four revalidation measurements; the row is reported only, and the RESEARCH budget ruling sets its value from the Stage 1 runs' measured sizes.

## Large-file tier

Binary captures, traces, Budgets per-frame CSVs and every text output over the per-file cap are committed as git LFS objects under the run's `archive/` directory. An LFS object is addressed by the SHA-256 of its content, so the pin in `record.json` is the LFS oid itself.

### Large-file layout

- `Zenith2/.gitattributes` carries the block (hand-off P-RS4):

```
# BEGIN zenith-research
Docs/Research/** text eol=lf
Docs/Research/**/archive/** filter=lfs diff=lfs merge=lfs -text
# END zenith-research
```

  The second line comes after the first, so it wins for files under `archive/`: they are LFS objects and never line-normalised.
- `record.json` `archive[]` lists every file of `archive/` with `sha256` equal to its LFS oid and `bytes` equal to its LFS size.
- A committed repository-root `.lfsconfig` holds the exclusion (hand-off P-RS4):

```
[lfs]
	fetchexclude = Zenith2/Docs/Research
```

  It applies to every clone and worktree alike (lanes, CI clones, the setup scripts' `git lfs pull`, the reference desktop), so no checkout downloads a research object by default. Whoever needs a study's objects runs `git lfs pull --include="Zenith2/Docs/Research/<Study>/**"`, whose command-line include overrides the exclusion.
- `filter.lfs.required true` (which `git lfs install` writes; the setup script runs it and sets the key explicitly, hand-off P-RS4) makes `git add` fail when the filter is configured but the git-lfs binary is missing, instead of committing an `archive/` file as a plain blob. A clone where git-lfs was never installed has no filter configured at all; there the runner's large-file precondition refuses to start, and step 4 of "Large-file verification" fails any plain blob that reaches the index or a commit.

| `kind` | Content |
|---|---|
| `research-trace` | a trace file |
| `budgets-frames-csv` | a Budgets per-frame CSV |
| `gpu-counter-capture` | an Adreno or desktop vendor counter capture |
| `image` | a captured frame, a FLIP heat map |
| `oversize-text` | a raw text output over the per-file cap |
| `log` | the runner log and the harness logs of a session |

### Large-file verification

`Test-Zenith2ResearchLargeFiles -RunDirectory <dir> [-Staged]` reads each file of `archive/` as bytes:

1. When the file is an LFS pointer (it begins with `version https://git-lfs.github.com/spec/v1` and holds the lines `oid sha256:<64 hex>` and `size <n>`), the oid and size must equal the `archive[]` entry's.
2. Otherwise (the object is fetched), the SHA-256 of its bytes and its length must equal them.
3. Every `archive[]` entry has its file, and every file has its entry.
4. Git alone, no git-lfs: for every path under `archive/` that `git ls-files -s` lists (with `-Staged`, the index) or `git ls-tree -r HEAD` lists (otherwise, the committed tree), `git check-attr filter -- <path>` must print `filter: lfs`, and the blob (`git cat-file --batch-check` size at most 1,024 bytes, its content read with `git cat-file --batch`) must begin with the pointer line `version https://git-lfs.github.com/spec/v1` and carry the `oid` and `size` of the `archive[]` entry; otherwise `RESEARCH_CHECK_NOT_LFS`. A trace committed as a plain blob passes steps 1 to 3 on a machine with git-lfs, because the working-tree bytes hash correctly, and fails this step everywhere. The runner's last lines print `git add <run dir>` and `Invoke-ResearchCheck.ps1 -Mode committed -Staged`, so the human runs this step before the commit.

The check needs neither `git lfs` nor the objects, so it runs in CI and in lanes; a mismatch of steps 1 to 3 is `RESEARCH_CHECK_SHA_MISMATCH`. Because `.lfsconfig` excludes the research objects, no gate host holds them and steps 1 to 3 read pointers there; the re-hash of a fetched object runs only where a human fetched it. Objects themselves are as durable as the repository's LFS store, the same store that holds every other LFS-tracked asset ([Z2-N-BUDGET-177]).

## Retention

- Every committed research file, LFS objects included, is kept for the life of the repository; git history keeps every superseded run.
- A committed research file is removed only by a DecisionLog entry naming the paths and the reason (a study withdrawn before any ruling used it). No check enforces this: a removal is a human `needs-human` diff (P-RS6 protects the run and machine paths) and is judged in review.
- Runner work files under `Zenith2/out/<checkout-hash>/research/` are kept until the next `zenith clean` of the tree.

## Format versioning

- Every format carries `schema` and `schemaVersion` (JSON) or a `v<N>` first line (CSV). A reader accepts exactly the versions it knows and refuses others with `RESEARCH_ERROR_SCHEMA`, naming the file and the version.
- A change that adds an optional member keeps the version; a change of a member's meaning, type or requiredness, or a new required member, increments it. The tool then reads every version that a committed file still uses, so committed runs are never rewritten to migrate them (research files are evidence, not authoring documents).
- `analysis.json` is regenerated, not migrated: a new tool version that changes its bytes for an unchanged input increments `tool`, and the check compares against the analysis of the tool version named in the file (each analysis version stays implemented while a committed file names it).

## Report template

`Zenith2/Docs/Research/<Study>/Reports/<Z2-D-NNN>.md`, one per ruling, created by `New-Zenith2ResearchReport` and completed by the human in the ruling ticket.

### Report headings

The `##` headings, verbatim and in this order:

| Heading | Content |
|---|---|
| `## Question and hypothesis` | the decision (D-NAME with its stage), the hypothesis sentence of `study.json` |
| `## Alternatives and acceptance rule` | the alternatives, the incumbent or "no incumbent", the decision's acceptance rule from `Zenith2/Docs/DecisionLog.md` → "Open decisions" quoted, and the study's `rule` expression with its `ruleNote` |
| `## Method` | profile, study version and SHA-256, cells, metrics, primary and secondary comparisons with their families, `comparesHarnesses` pairs, conditions, launches, schedule seed, sessions, overrides with justifications, deviations |
| `## Machines and builds` | per run: machineId, the software state, tree SHA, preset, the dependency versions |
| `## Data` | per series: n, median, interval, rCV, min, max, drift, state; generated |
| `## Analysis` | per comparison: family, shift, interval, gain, δ, relative shift, verdict and reasons, p, Holm p, Cliff's delta; the rule's terms over every run the report names and the study-level outcome; generated |
| `## Exclusions and threats to validity` | `notApplicable` cells, capability exclusions, invalid launches by reason, fixed-performance availability, `TIES_DISAGREE`, `SPREAD_DIFFERS` and `INVALID_RATE` comparisons, the human's notes |
| `## Conclusion` | the outcome, the `judgement(…)` terms with their reasoning, and whether a default changes (and the follow-up ticket that changes it) |
| `## Revalidation triggers` | the trigger list, implied triggers included |

RA studies add, after `## Analysis`, `## Architectural and adapter differences` (ECS: the classification and adapter tables, generated) or `## Shading path report sections` (the five sections of `Zenith2/Docs/Rendering.md` → "What each shading-path report contains", the per-pass tables generated, the exclusions written). RP studies replace `## Data` and `## Analysis` with `## Probe results` (a table of case × device: pass or fail; generated).

### Generated blocks

Tables under `## Data`, `## Analysis`, `## Architectural and adapter differences`, the per-pass tables of `## Shading path report sections` and `## Probe results` sit between the lines `<!-- research:generated begin <block> -->` and `<!-- research:generated end <block> -->`, with `<block>` one of `data`, `analysis`, `classification`, `adapter`, `passes`, `probe`. `Update-Zenith2ResearchReport` regenerates them from the named runs' `analysis.json`, the `analysis` block's rule part through `Invoke-Zenith2ResearchStudyRule`; the check regenerates them and fails a report whose block bytes differ (`RESEARCH_CHECK_REPORT_STALE`), so no number in a report is typed by hand. Numbers in generated tables are integers in the series unit, with ratios as percentages to two decimals computed from ppm by integer division (`relPpm ÷ 10,000` and its remainder).

### Study-specific sections

A section outside the template (a licence review, the scenario oracles of D-SEL-2DPHYS) is a `###` under `## Conclusion` or `## Exclusions and threats to validity`; no `##` heading other than those of "Report headings" appears.

## Decision ticket template

Every ticket with `kind: decision`, every `kind: budget` and `kind: revalidation` ticket that runs a measurement, and every ticket carrying a `Research-Role:` line carries, as the first block of its `## Implementation Contract`, the template lines below, one per line, each a label, a colon and a value. The check `Test-Zenith2DecisionTicket` reads them (`Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Decision ticket check").

### Template lines

| Label | Value |
|---|---|
| `Research-Profile:` | `RA`, `RS`, `RJ`, `RB`, `RP` or `RQ` |
| `Research-Role:` | `ruling` (default when absent), `run` (a measurement-run ticket that commits run directories and no DecisionLog entry) or `revalidation` (a revalidation run that commits run directories and a `revalidations.json` entry) |
| `Decision-Name:` | the D-NAME, or `Z2-D-NNN` of the provisional record (RJ), or the budget producer name (RB), or a short noun phrase (RQ without a D-NAME) |
| `Study:` | `Zenith2/Docs/Research/<Study>/study.json`; `none` for RQ |
| `Hypothesis:` | one sentence; RQ: the question |
| `Alternatives:` | `(1) … (2) …`, at least two, the incumbent (if any) marked `(incumbent)`; RQ and RP may write the classification options |
| `Primary metrics:` | the metric ids of the primary comparisons; `none` for RP and RQ |
| `Acceptance rule:` | the study's `rule` expression for RA, RS, RJ, RB; RP: `pass(...)` terms; RQ: the decision's acceptance rule quoted |
| `Platforms:` | from `Win-Vulkan`, `Win-D3D12`, `Win`, `S24`, `Host`; `none` for RQ |
| `Conditions:` | `controlled`, `ordinary` (RB, and RP under the rule of "study.json fields") or `none` |
| `Repetitions:` | `10 + 1 warm-up` … `20 + 1 warm-up`; RP `1` or `3`; RQ `none` |
| `Outputs:` | the paths the diff commits: `Zenith2/Docs/Research/<Study>/runs/<RunId>/`, `Zenith2/Docs/Research/<Study>/Reports/<Z2-D-NNN>.md`, `Zenith2/Docs/DecisionLog.md` (`ruling` role); RB adds `allocation.json` and `Zenith2/Docs/Budgets.md`; `revalidation` adds `revalidations.json` |
| `Revalidation triggers:` | the trigger tokens, `; `-separated |
| `Deviations:` | `none`, or the deviations from the study and this methodology the ticket already knows |

### Profile requirements for the template lines

- Every ticket whose committed outputs (the "Files edited in one diff" line, the `Outputs:` line, a Definition of Done item that says "committed") include a path under `Zenith2/Docs/Research/**/runs/` or `Zenith2/Docs/Research/Machines/`, of any kind, carries `needs-human`; a `kind: harness` ticket never has one as an output, though it may read one (WORKER-EVIDENCE: a worker never runs or records a measurement). The diff-level guard is hand-off P-RS6: both globs are protected paths, so a loop ticket whose diff touches them is refused.
- An RA, RS, RJ, RB or RP ticket is blocked by the harness ticket that commits its `study.json`, and its `Study:` path names a file that ticket's Implementation Contract creates.
- A `run` role ticket's `Outputs:` names run directories only; the paired `ruling` ticket is blocked by it.

### Definition of Done lines

A ruling ticket's Definition of Done contains, at least:

- `- [ ] The run directories, the report Zenith2/Docs/Research/<Study>/Reports/<Z2-D-NNN>.md and the DecisionLog entry are committed, and the research check Pwsh_Research_CheckCommitted passes in the Test step (Z2-D-064, Z2-D-097).`
- `- [ ] Failure scenario: when the acceptance rule is false or blocked, the outcome is recorded as the rule's false-outcome or as Blocked naming the INVALID terms, and no default changes in this diff.`

A `run` or `revalidation` role ticket carries the first line with its own outputs and the failure scenario "an INSUFFICIENT cell is committed with its INVALID comparisons shown and is re-run in a new run".

## DecisionLog research fields

A DecisionLog entry that rules a research decision (profiles RA, RS, RJ, RB, RP) adds three fields after **Related**, in this order (cross-document item X418 adds them to `Zenith2/Docs/DecisionLog.md` → "Entry template"); an RQ entry adds **Revalidation** alone:

```
- **Research:** profile RS; study `Zenith2/Docs/Research/HeapSelection/study.json` (version 1, sha256 <64 hex>); runs `20261102T0910Z-win-3f9a1c2b`, `20261104T1300Z-s24-a1b2c3d4`; report `Zenith2/Docs/Research/HeapSelection/Reports/Z2-D-NNN.md`
- **Analysis:** outcome `keep(system)`; throughput-wdense Win EQUIVALENT, S24 INCONCLUSIVE; throughput-wchar Win EQUIVALENT, S24 EQUIVALENT; rss-s24 S24 EQUIVALENT; default change: none
- **Revalidation:** toolchain; hardware; dependency:mimalloc; os:android
```

- **Research:** the profile, the study path with its version and SHA-256, every run used, and the report path.
- **Analysis:** the study-level outcome exactly as the report's generated `analysis` block gives it (`Invoke-Zenith2ResearchStudyRule` over the runs of **Research:**), then each primary comparison with its verdict per platform, then `default change:` followed by `none` or the Draft-ID or key of the follow-up ticket that changes the default (a research conclusion never changes a default in its own diff, Z2-D-064).
- **Revalidation:** the trigger tokens, implied ones first.

## Justification record evidence line

A profile RJ ruling completes the provisional record's `Measured evidence:` line ([Z2-N-CONTRACT-278], [Z2-N-CONTRACT-292]) in this form, one line per platform:

```
- Measured evidence: Zenith_Bench_CoreContainers, study CoreContainers v1, run 20261102T0910Z-win-3f9a1c2b, Win, flat-map-lookup median 41210 ns [40990, 41530] against std::unordered_map 52004 ns [51700, 52410], BETTER; report Zenith2/Docs/Research/CoreContainers/Reports/Z2-D-NNN.md
```

and adds the three research fields of "DecisionLog research fields" to the same entry (cross-document item X418 (b)).

## revalidations.json

`Zenith2/Docs/Research/<Study>/revalidations.json`, the log of handled triggers:

```json
{
	"schema": "zenith2.research-revalidations",
	"schemaVersion": 1,
	"study": "HeapSelection",
	"entries": [
		{ "decision": "Z2-D-NNN", "trigger": "dependency:mimalloc", "observed": "2.1.9", "recordedValue": "2.1.7", "firedTreeSha": "<40 hex>", "run": "20270310T0900Z-win-3f9a1c2b", "outcome": "stands", "rerulingTicket": null, "note": "", "recordedUnixMs": 1804320000000 }
	]
}
```

`outcome` is `stands`, `stands-drifted` or `regressed` (`Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Research regression rule"); `rerulingTicket` names the filed ticket for `regressed`. Entries are appended, never edited; the file is written by the `needs-human` revalidation ticket that ran the re-measure.
