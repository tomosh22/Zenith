# DEVT design: budget evidence, telemetry and dashboards

This file is part of the DEVT design family; the entry document is `Zenith2/Docs/Design/DEVT.md`. It specifies the evidence-fingerprint specification (Stage 1 deliverable 13 of `Zenith2/Docs/Roadmap.md`, Z2-D-065) and its PowerShell module and invalidation tests, the workload runner `Zenith2/Benchmarks/Run-Workload.ps1`, the row source bindings, the Player-side budget-run record `Zenith_BudgetRunRecord` with its statistics and evidence writer, the per-frame capture, the measured interval, the area row emitters, run and frame telemetry, and the regression dashboards. The evidence record's fields are `Zenith2/Docs/Budgets.md` → "Evidence records and the input fingerprint" exactly; this file adds how each field is produced.

## Scope of this file

| Concern | Fixed by | Specified here |
|---|---|---|
| the evidence record block, the fingerprint components, the CSV header | `Zenith2/Docs/Budgets.md` → "Evidence records and the input fingerprint" ([Z2-N-BUDGET-095] to [Z2-N-BUDGET-104]) | the algorithm, the module, the writer |
| the procedures and their statistics | `Zenith2/Docs/Budgets.md` → "Measurement procedure catalogue" | which source each row binds to |
| the budget-run switches and the `render.quality.*` rule | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Command-line options" | the runner that passes them |
| workload definitions | `Zenith2/Docs/Budgets.md` → "Workload definitions" | the runner that reads them |
| the Release measurement package | SHIP (`Zenith2/Docs/Design/SHIP/Release.md` → "Release staging layout") | the runner's `-Package` mode (row SH10) |
| acceptance's recomputation of fingerprints | PROC's `Coverage` gate step | the module it imports (row DV17) |

## Evidence fingerprint specification

The fingerprint of a run is the seven-member `fingerprint` object of the evidence block. Its members, their inputs and their canonical encodings:

| Member | Input | Canonical bytes hashed (SHA-256, lowercase hex) |
|---|---|---|
| `closure` | every source file in the transitive target-dependency closure of the executable under test, every header those sources include in the measured preset, plus `composition.json` | the lines `"<repo-relative path> <git blob sha1>\n"` for every file, sorted by path bytes, UTF-8, LF |
| `toolchain` | the reproducibility manifest's `toolchain` section (`Zenith2/ThirdParty/manifest.json`) | SHIP's `toolchainHash`: the SHA-256 of the section's canonical form, computed by `Get-Zenith2ManifestHashes` (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Canonicalisation and hashing"); the configure step's version check ([Z2-N-ARCH-194]) makes the resolved compilers equal the pins, so the pins alone identify the toolchain |
| `thirdParty` | the manifest's `dependencies` section | SHIP's `dependenciesHash` from the same function; a whitespace-only edit of the manifest changes neither member |
| `shaderBlobs` | the cooked shader blob set of the build (`${ZENITH_OUT}/generated/ZenithShadersGenerated/` blob files) | the lines `"<file name> <sha256 of the file>\n"` sorted by name |
| `content` | the cooked content set of the workload's game | the mount manifest's `contentHash` (ASSET's `content:/mount-manifest.json`), as its ASCII hex string |
| `workloadDefinition` | `Zenith2/Games/<Game>/Workload/<W>.workload.json` | the file's bytes ([Z2-N-BUDGET-192]) |
| `harness` | the runner and record versions | the string `"Run-Workload/<runner version>;BudgetRunRecord/<record version>"`, both integers, not hashed |

The closure's file list comes from two outputs of the measured build itself, never from an undeclared output of a protected gate step: the CMake File API reply of the measured preset (codemodel v2: the executable target's `dependencies` transitively and each target's `sources`), and the header dependencies Ninja recorded while compiling those sources in the same preset and configuration (`ninja -C <binary dir> -f build-<Config>.ninja -t deps`, which prints, per object, every header the compiler reported through its depfile or `/showIncludes`), so a `win-release` measurement lists `Internal/Windows/*.h` and an `android-arm64` one lists `Internal/Android/*.h`; headers outside `Zenith2/` (the SDKs, the MSVC and NDK headers) are dropped, because the `toolchain` member covers them; plus `composition.json` (MOD's record of the linked modules and backends, so a backend swap changes the fingerprint). Generated files are represented by their generator inputs, which are sources in the closure. Every file's blob SHA is the blob `git add` would store for it: `git hash-object --stdin-paths` with filters applied (never `--no-filters`), which runs each path's attributes, its clean filter and the end-of-line normalisation that `.gitattributes` (`* text=auto`) and `core.autocrlf` select, so a text file checked out with CRLF on the Windows desktop and with LF on the Linux CI runner hashes to the same blob, and the hash equals the file's index blob. It is still computed from the working tree, so an uncommitted edit changes the fingerprint.

## Fingerprint algorithm

`Get-Zenith2EvidenceFingerprint -Preset <p> -Configuration <c> -Target <exe target> -Game <G> -Workload <W>` (module § "Zenith2Evidence module"):

1. Read the File API reply under `Zenith2/out/<checkout-hash>/<preset>/.cmake/api/v1/reply/` (the query file is written by `Configure`); find the target; walk `dependencies` to a fixed point; collect sources; run `ninja -t deps` for the configuration in the preset's binary directory and add every header listed for those sources' objects; add `composition.json` from the target's output directory; normalise every path to repo-relative with `/`; drop paths outside `Zenith2/` and duplicates. A source whose object has no deps entry (the build never compiled it in this configuration) makes the function throw `closure incomplete: <path> not built in <preset>/<configuration>`, because a fingerprint of an unbuilt tree would describe nothing measured.
2. For each file compute its blob with one batched `git hash-object --stdin-paths` (filters applied, no `--no-filters`; paths repo-relative, so each path's attributes apply); build the sorted lines; SHA-256 → `closure`. Self-check: for every closure path that the tree-SHA rule's index holds (the real index for a clean tree, the temporary index for a dirty one, § below), the blob must equal that index's stage-0 blob from `git ls-files -s -- <paths>`; a difference throws `closure blob mismatch: <path>` (a clean filter or attribute that `git add` and `hash-object` apply differently would otherwise make the fingerprint machine-dependent). Paths outside the index (the build's `composition.json`) are hashed the same way and need no check.
3. Compute the other members as the table states; `toolchain` and `thirdParty` are `(Get-Zenith2ManifestHashes).toolchainHash` and `.dependenciesHash` (SHIP's `Zenith2/Tools/Setup/Zenith2Env.psm1`, imported, never re-implemented).
4. Return `[ordered]@{ closure; toolchain; thirdParty; shaderBlobs; content; workloadDefinition; harness }` and write it, with the inputs' counts, as `fingerprint.json`:

```json
{ "schema": "zenith2.evidence-fingerprint", "schemaVersion": 1, "fingerprint": { … the seven members … },
  "inputs": { "closureFiles": 2148, "shaderBlobs": 412, "treeSha": "<40-hex>", "preset": "win-release", "configuration": "Release" } }
```

`treeSha` is `git write-tree` of the index after `git add -A --dry-run` is clean, or, for a dirty tree, the tree object of a temporary index built from the working tree (`GIT_INDEX_FILE` pointed at a scratch copy, `git add -A`, `git write-tree`), so the recorded tree is the one measured without touching the real index. Acceptance (`Test-Zenith2EvidenceRecord`) recomputes the fingerprint for the record's preset, configuration, target and workload and compares all seven members ([Z2-N-BUDGET-099]); a mismatch names the members that differ.

## Zenith2Evidence module

`Zenith2/Benchmarks/Evidence/Zenith2Evidence.psm1` exports:

| Function | Purpose |
|---|---|
| `Get-Zenith2EvidenceFingerprint` | § "Fingerprint algorithm" |
| `Get-Zenith2TreeSha` | the tree SHA rule above |
| `Test-Zenith2EvidenceRecord -Path <record>` | schema check (every field of the block present, [Z2-N-BUDGET-096]), the fingerprint recomputation, the run-length and exclusion validity rules ([Z2-N-BUDGET-069], [Z2-N-BUDGET-070]); returns `@{ valid; reasons[] }` |
| `Read-Zenith2Workload -Game <G> -Workload <W>` | parses and validates a workload definition (required fields of [Z2-N-BUDGET-192], version an integer ≥ 1) |

The module uses `pwsh` 7 built-ins, `git`, `ninja` and SHIP's `Zenith2Env.psm1` alone, sets `Set-StrictMode -Version Latest`, reads optional JSON members through `PSObject.Properties[...]`, and returns collections with `,@(...)` so a one-element list stays a list. PROC's `Coverage` gate step imports it for acceptance checks and runs its assert script (row DV17).

## Fingerprint invalidation tests

`Zenith2/Benchmarks/Evidence/Tests/Test-Zenith2Evidence.ps1` (a plain assert script printing its own count) builds a fixture repository at test time under `$env:TEMP` (`git init`, the files of `Tests/Fixtures/MiniTree/`, a fake File API reply, a fake `ninja -t deps` output read through the module's `-DepsText` test parameter, SHIP's fixture manifests `base.json`, `dependency-bump.json` and `toolchain-bump.json` from `Zenith2/CMake/Tests/Ship/FingerprintInputs/` (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Evidence invalidation"), a fake shader blob directory, a fake mount manifest and workload file), computes a baseline fingerprint, then applies each change and asserts exactly the expected members change:

| Case | Change | Members that must change | Members that must not |
|---|---|---|---|
| `Pwsh_Evidence_DependencyChange` | edit a source of a dependency two levels down | `closure` | the other six |
| `Pwsh_Evidence_UnrelatedChange` | edit a source outside the closure | none | all |
| `Pwsh_Evidence_ToolchainBump` | SHIP's `toolchain-bump.json` (one `clVersion` changed) in place of `base.json` | `toolchain` | `thirdParty` and the rest |
| `Pwsh_Evidence_MiddlewareBump` | SHIP's `dependency-bump.json` (one `source.sha256` changed) | `thirdParty` | `toolchain` and the rest |
| `Pwsh_Evidence_ManifestWhitespaceOnly` | reformat `base.json` (the test adds whitespace) | none | all |
| `Pwsh_Evidence_PresetOnlyHeader` | edit a header that only the Android deps output lists | `closure` of an `android-arm64` fingerprint | the `win-release` fingerprint's `closure` |
| `Pwsh_Evidence_ContentBump` | change the mount manifest's `contentHash` | `content` | the rest |
| `Pwsh_Evidence_ShaderChange` | change one blob's bytes | `shaderBlobs` | the rest |
| `Pwsh_Evidence_WorkloadBump` | change the workload definition | `workloadDefinition` | the rest |
| `Pwsh_Evidence_CompositionChange` | change `composition.json` (a backend swap) | `closure` | the rest |
| `Pwsh_Evidence_UncommittedEdit` | edit a closure file without committing | `closure` | the rest |
| `Pwsh_Evidence_RecordRejectedOnMismatch` | a record written before a dependency change | `Test-Zenith2EvidenceRecord` returns `valid = $false` naming `closure` | |
| `Pwsh_Evidence_RecordMissingField` | a record lacking `gfxSamples` | invalid, naming the field | |
| `Pwsh_Evidence_LineEndingsIndependent` | the fixture repository is created with `* text=auto` in `.gitattributes`; one closure file is written with CRLF line endings, then with LF, each fingerprinted under `core.autocrlf=true` and under `core.autocrlf=false` (the test passes `-c core.autocrlf=…` through the module's `-GitConfig` test parameter) | none: `closure` is identical in all four, and equals `git ls-files -s`'s blob after `git add` | all |

These are the four invalidation proofs of [Z2-N-BUDGET-100] (dependency, toolchain, middleware, content) plus the cases that keep the specification honest in the other direction, including machine independence (`Pwsh_Evidence_LineEndingsIndependent`), which acceptance's recomputation on another machine ([Z2-N-BUDGET-099]) relies on.

## Workload runner

```
pwsh -NoProfile -File Zenith2/Benchmarks/Run-Workload.ps1
	-Game <G> -Workload <W>
	[-Platform Win|S24] [-Backend Vulkan|D3D12] [-Preset win-release|android-arm64] [-Configuration Release]
	[-Package <release staging dir>]
	[-CacheState cold|warm] [-Conditions controlled|ordinary] [-Repetitions <n>] [-FirstRepetition <n>]
	[-EvidenceRoot <dir>] [-Serial <adb serial>] [-ProfilerOverheadPair] [-Research] [-TimeoutSeconds <n>]
```

Steps for each repetition:

1. Validate: `Read-Zenith2Workload`; the configuration must be `Release` ([Z2-N-BUDGET-108]); `-Platform S24` requires one attached device (or `-Serial`).
2. Choose the executable: without `-Package`, the tree's build output `Zenith2/out/<checkout-hash>/<preset>/Release/<Game>.exe` (Windows) or the `release` build type APK (Android); with `-Package <dir>` (SHIP's Release measurement package, `<channel>-<tree12>-release/` of `Zenith2/Docs/Design/SHIP/Release.md` → "Release staging layout"), read the package's `release.json`, refuse it unless `"configuration": "Release"` (a Shipping package has no budget-run switches, [Z2-N-ARCH-878]), refuse it unless its tree SHA equals `Get-Zenith2TreeSha` of the checkout (the fingerprint describes the checkout, so the two must be the same tree), and launch `windows/<Game>/<Game>.exe` or install the package's APK (`adb install -r -t`); the evidence record's `artifact` path and a `package` note in the run telemetry name the package directory.
3. Compute the fingerprint (§ "Fingerprint algorithm") for the preset, configuration, the game's executable target and the workload; write `fingerprint.json` and `rows.json` (§ "Budget-run inputs") into a new evidence directory `<EvidenceRoot>/<W>/<platform>-<backend>/<utc>-r<n>/` (default root `Zenith2/out/<checkout-hash>/evidence/`, [Z2-N-BUDGET-101]); on the S24 the directory is the app-specific external path `/sdcard/Android/data/<package>/files/evidence/<utc>-r<n>/`, created and filled with `adb push`.
4. Learn the registered quality keys (once per executable, before the first repetition): launch the same executable with `--headless --config-dump <dir>/config-dump.json --exit-after-ticks 1` plus one `--set` per `graphicsKeys` entry of `RowSources.json` (§ "Row source bindings") with the workload's value (not a budget run, so [Z2-N-ARCH-877]'s check does not apply to this launch; on the S24 the dump path is inside the app-specific external directory and is fetched with `adb pull`); read the `zenith2.config-dump` document (`Zenith2/Docs/Design/PLAT/Configuration.md` → "The configuration dump") and take every key of `keys` whose name begins with `render.quality.`. A key's value is the workload's value when `graphicsKeys` maps a `graphics` field to it, and otherwise the value of the highest-precedence entry of its `chains` array whose `layer` is neither `user` nor `commandLine` (the tier's, the game's, the platform's or the engine default's value, never the user's settings file, [Z2-N-BUDGET-122]). The runner writes the resolved list as `quality-keys.json` into the evidence directory and refuses the run when `graphicsKeys` names a key the dump lacks.
5. Build the command line: `--budget-run <W> --workload-version <v> --evidence-dir <dir> --cache-state <c> --conditions <k> --repetition <n>`; one `--set render.quality.<key>=<value>` for every key of step 4, so every registered key is set exactly once ([Z2-N-ARCH-877]); `--set rhi.backend=<backend>`; `--input-script <scriptedPath>` (the definition's path, resolved inside the game's content); `--set devtools.budgetRun.loop=true` when `loopUntilMinimums`; `--set devtools.profiler.enabled=false --set devtools.tracy.enabled=false` unless a row of the run reads profiler zones (§ "Budget-run sampling"); `--timeout-seconds` from `-TimeoutSeconds` (default 3,600; 2,700 for MP-SUSTAIN).
6. Cache state: `cold` deletes the pipeline cache and PSO-library files before launch (Windows: the user-mount `cache/` directory; S24: `adb shell run-as <package> rm -rf files/cache`); `warm` runs one unmeasured warm-up launch to `--exit-after-ticks 600` first when no cache exists.
7. Launch and time: Windows `Start-Process` with the measured `CreateProcess` return time (MP-BOOT) recorded by the runner; S24 `adb shell am start -W -n <package>/<activity> --es zenithArgs "<args>"` with its `ThisTime`/`TotalTime` lines recorded.
8. Wait for exit; exit code 0 means the record was written; 2 an input error (a bad budget-run switch, a quality key set twice, or a missing or malformed `rows.json` or `fingerprint.json` at startup step 7; the Player names it), 1 a record write failure, 124 the timeout. The step-7 budget-input failure is exit code 2, not MOD's generic step-7 code 4, because it is an input error like every other budget-run input ([Z2-N-ARCH-878]); row DV12 asks MOD's step-failure table for the row, and X245 (a) carries it into CompositionRoot.
9. Collect: on the S24 `adb pull` the evidence directory; check the record with `Test-Zenith2EvidenceRecord`; print one summary line per row.

With `-ProfilerOverheadPair` the runner runs each repetition twice, once with the overlay in `full` (`--set devtools.profiler.enabled=true --set devtools.profiler.visible=full`) and once disabled, and writes the overhead rows' records from the pair ([Z2-N-BUDGET-107], Z2-B-DEVT-002). `-Repetitions` defaults to 3 (qualification); `-Research` makes it 10 plus one discarded warm-up repetition ([Z2-N-BUDGET-074], [Z2-N-BUDGET-076]). The runner's profiler and Tracy switches are tested by `Pwsh_Workload_ProfilerAndTracySwitches` (a run with no `needsProfiler` row passes `devtools.profiler.enabled=false` and `devtools.tracy.enabled=false`; a run with one passes `devtools.profiler.enabled=true` and still `devtools.tracy.enabled=false`; the overhead pair's enabled half adds `devtools.profiler.visible=full`), and the key source by `Pwsh_Workload_QualityKeysEachOnce` against the hand-written dump fixture `Zenith2/Benchmarks/Evidence/Tests/Fixtures/ConfigDump/config-dump.json` (a tier layer, a user layer and a mapped key: the built command line names each `render.quality.*` key once, with the tier's value where the user layer differs).

## Row source bindings

`Zenith2/Benchmarks/Workloads/RowSources.json` (schema `zenith2.row-sources`, version 1) maps every row a workload can measure to a source the Player samples:

```json
{
	"schema": "zenith2.row-sources",
	"schemaVersion": 1,
	"graphicsKeys": { "tier": "render.quality.tier", "shadingPath": "render.quality.shadingPath", "dynamicResolution": "render.quality.dynamicResolution", "upscaler": "render.quality.upscaler" },
	"rows": {
		"Z2-B-SHIP-001": { "kind": "frameTime", "statistic": "p95", "unit": "ms" },
		"Z2-B-SHIP-011": { "kind": "dynres", "statistic": "min", "unit": "%" },
		"Z2-B-SHIP-012": { "kind": "hitch", "statistic": "count", "unit": "count" },
		"Z2-B-JOBS-001": { "kind": "counter", "name": "jobs.overhead_ns", "scale": 0.000001, "statistic": "p95", "unit": "ms" },
		"Z2-B-ECS-002": { "kind": "counter", "name": "ecs.playback_us_delta", "scale": 0.001, "statistic": "p95", "unit": "ms" },
		"Z2-B-DEVT-001": { "kind": "memorySubsystem", "subsystem": "other", "component": "devtools", "statistic": "max", "unit": "MB", "excludes": "capture arena, development-file writes, remote rings, the budget-run record" },
		"Z2-B-DEVT-002": { "kind": "zoneSum", "zones": ["DevTools.EndFrame", "DevTools.DrainDebugDraws", "DevTools.ExtractDebugDraws@W*"], "statistic": "p95", "unit": "ms", "needsProfiler": true },
		"Z2-B-DEVT-003": { "kind": "gpuPassSum", "passes": ["DebugOverlay", "HiZDebugOverlay"], "statistic": "p95", "unit": "ms" },
		"Z2-B-GEOM-001": { "kind": "emitted", "emitter": "GEOM", "statistic": "p95", "unit": "ms" }
	}
}
```

| Kind | Per-frame sample | Members |
|---|---|---|
| `frameTime` | the present-to-present interval (`Zenith_ProfilerFrameSummary::m_uPresentIntervalUs`, falling back to `m_uFrameUs`) | — |
| `hitch` | 1 when the frame time exceeds 33.3 ms | — |
| `dynres` | the main view's scale per axis, the minimum of the two | — |
| `nodeSpanSum` | the summed span of the named CPU frame-graph nodes from GRAPH's `GetLastTimings()` | `nodes` (names, `*` allowed) |
| `gpuPassSum` | the summed GPU time of the named passes: the frame totals of the matched GPU pass sites in the profiler's zone statistics (every occurrence of a pass in the GPU frame accumulated into its site's total, reset once per frame; `Zenith2/Docs/Design/DEVT/Profiler.md` → "Zone statistics"), which the profiler builds from the frame's GPU zones in either mode while `devtools.profiler.gpuTimings` is set (`Zenith2/Docs/Design/DEVT/Profiler.md` → "GPU timing ingestion"). GPU sites carry their view suffix ([Z2-N-REND-312]: `DebugOverlay[Main]`), so an entry `<name>` without brackets matches the site `<name>` and every site `<name>[<suffix>]`, summed across views; an entry with `[<suffix>]` matches that site exactly; `*` matches any run of characters, as in `zoneSum` (`DebugOverlay[*]` is every view of the pass). A frame in which no matched site has an entry samples NaN | `passes` |
| `counter` | the counter's frame value × `scale` | `name`, `scale` |
| `zoneSum` | the summed inclusive frame totals of the named sites (Profiler.md "Zone statistics": every closure of each site in the frame accumulated, from every call and every thread, a self-nested closure adding nothing, reset once per frame); a named site with no closure in the frame adds 0. Totals of different sites are summed as they are, so a row names sites that do not enclose one another | `zones`, `needsProfiler: true` |
| `memorySubsystem` | without `component`: the governor's resident bytes of the subsystem, minus the budget-run record's `GetResidentBytes()` when the subsystem is `other` (the record's own subsystem); with `component`: the GAUGE `<component>.memory_bytes`, which the component publishes every frame as the sum of its own consumers' `GetResidentBytes` (the MP-SUBMEM attribution rule for a component inside a shared subsystem, since `Zenith_MemoryConsumer` carries no name; DEVT's is `devtools.memory_bytes`, `Zenith2/Docs/Design/DEVT/Profiler.md` → "Memory layout and accounting"); bytes ÷ 10^6 for MB | `subsystem`, `component`, `excludes` (a note copied into the record's row entry) |
| `processMemory` | MP-MEM's process sample (PLAT's sampler, row DV13) minus the budget-run record's `GetResidentBytes()` (the measurement apparatus is not the game, § "Zenith_BudgetRunRecord header") | — |
| `gpuMemory` | MP-GPUMEM's device-local sample (RHI's `GetMemoryBudget`) | — |
| `emitted` | the value an area emitter adds (§ "Area row emitters") | `emitter` |

Each area's budget-emission ticket adds its rows here in the same diff as its emitter or counter; `RowSources.json` is DEVT's file and each row entry is the area's content. A row a workload's `rows` member names without an entry here is refused by the runner before launch.

## Budget-run inputs

`rows.json` (schema `zenith2.budget-run-rows`, version 1), written by the runner into the evidence directory:

```json
{
	"schema": "zenith2.budget-run-rows", "schemaVersion": 1,
	"workload": "W-Outdoor", "workloadVersion": 1, "platform": "Win", "backend": "Vulkan",
	"rows": [ { "id": "Z2-B-SHIP-001", "kind": "frameTime", "statistic": "p95", "unit": "ms", "value": 16.7, "tolerance": 0 } ],
	"loopUntilMinimums": true, "sustained": false, "latency": false
}
```

`value` and `tolerance` are the row's adopted values from `Zenith2/Docs/Budgets.md` → "Row index" for the platform (the runner parses the Value column with the grammar of [Z2-N-BUDGET-009]); a pending row has `"value": null` and its result is `report`. The Player reads `rows.json` and `fingerprint.json` from the evidence directory at startup step 7: <!-- assumption: pending PLAT --> PLAT's directory grant gains a read-open of a named file inside the evidence directory (`Zenith_OpenFileInCommandLineHostDirectory`, refusing names with path separators), beside the create-only `Zenith_CreateFileInCommandLineHostDirectory` (row DV13). <!-- /assumption -->

## Zenith_BudgetRunRecord header

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_BudgetRunRecord.h
#pragma once
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithProfiling/Zenith_Profiler.h"

inline constexpr uint32 uBUDGET_RUN_MAX_ROWS = 128;
inline constexpr uint32 uBUDGET_RUN_MAX_FRAMES = 216000;          // 60 min at 60 Hz
inline constexpr uint32 uBUDGET_RUN_RECORD_VERSION = 1;

enum Zenith_BudgetRunErrorCode : uint8 { /* the enumerators of Zenith2/Docs/Design/DEVT.md "Error enumerations" */ };

/// @contract value Zenith_BudgetRunError
/// Ownership: Plain value (the record form of [Z2-N-CONTRACT-480], 8 bytes).
/// Thread affinity: IMMUTABLE; members non-const: held as the error member of Zenith_Result, which callers assign.
/// Invalidation: None.
/// Units: m_uRowIndex is the 0-based index into rows.json's rows (0xFFFF when no row applies); m_uLine the 1-based line of
///   the input file the reader rejected (0 when not a parse error).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_BudgetRunError
{
	Zenith_BudgetRunErrorCode m_eCode;
	uint8 m_uReserved;
	uint16 m_uRowIndex;
	uint32 m_uLine;
};

/// @contract interface Zenith_EvidenceFileWriter
/// Ownership: Implemented by ZenithRuntime's budget-run driver over PLAT's Zenith_HostFile (one file created with
///   Zenith_CreateFileInCommandLineHostDirectory and kept open for the run) and by tests (an in-memory writer); borrowed by
///   the record for one Write or FlushFrames call.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: xBytes is valid for the duration of the call.
/// Units: Bytes.
/// Errors: Append and Flush return false when the host file refused the bytes; the record maps it to BUDGET_RUN_ERROR_WRITE.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3. Source API.
/// @endcontract
class Zenith_EvidenceFileWriter
{
public:
	virtual ~Zenith_EvidenceFileWriter() = default;
	virtual bool Append(Zenith_ByteSpan xBytes) = 0;   // appends at the end of the one file
	virtual bool Flush() = 0;
};
constexpr const char* Zenith_ToString(Zenith_BudgetRunErrorCode eCode);
constexpr Zenith_ErrorCategory Zenith_GetErrorCategory(Zenith_BudgetRunErrorCode eCode);

/// @contract value Zenith_BudgetRowSpec
/// Ownership: A view into the record's parsed rows.
/// Thread affinity: IMMUTABLE; members non-const: the rows.json parser fills each entry of the record's row table field
///   by field.
/// Invalidation: With the record.
/// Units: m_strUnit names the row's unit; m_dValue in that unit.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_BudgetRowSpec
{
	Zenith_StringView m_strRowID;
	Zenith_StringView m_strKind;
	Zenith_StringView m_strStatistic;
	Zenith_StringView m_strUnit;
	double m_dValue = 0.0;
	bool m_bPending = false;
	double m_dTolerance = 0.0;
};

/// @contract value Zenith_BudgetRunInputs
/// Ownership: Plain value filled by ZenithRuntime's budget-run driver from Zenith_CommandLine.
/// Thread affinity: IMMUTABLE.
/// Invalidation: The views live as long as the command line.
/// Units: None.
/// Errors: Create validates them against rows.json (BUDGET_RUN_ERROR_WORKLOAD_MISMATCH).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_BudgetRunInputs
{
	const Zenith_StringView m_strWorkload{};
	const uint32 m_uWorkloadVersion = 0;
	const uint32 m_uRepetition = 0;
	const uint8 m_uCacheState = 0;       // Zenith_BudgetRunCacheState value (ZenithRuntime's enum, passed as its value)
	const uint8 m_uConditions = 0;       // Zenith_BudgetRunConditions value
	const Zenith_ByteSpan m_xRowsJson{}; // rows.json bytes
	const Zenith_ByteSpan m_xFingerprintJson{};
	const Zenith_StringView m_strPreset{}, m_strConfiguration{}, m_strPlatform{}, m_strBackend{}, m_strTreeSha{};
	const uint32 m_uFramesInFlight = 0;
	const bool m_bLoopUntilMinimums = false;
};

/// @contract type Zenith_BudgetRunRecord
/// Ownership: Created by Create as a Zenith_Owned, owned by ZenithRuntime's Zenith_DevTools for a budget run alone.
///   Owns the per-frame sample store (the run's rows × uBUDGET_RUN_MAX_FRAMES floats, 4 bytes each, reserved at Create:
///   864,000 bytes per row), the 1 MiB CSV buffer and the 4,096-sample queue (CORE Zenith_BoundedQueue, 64-byte cells).
///   It is the measurement apparatus: a Zenith_MemoryConsumer of its own in MEMORY_SUBSYSTEM_OTHER, registered by the
///   budget-run driver at startup step 8, outside Z2-B-DEVT-001 (Zenith2/Docs/Design/DEVT/Profiler.md "Memory layout and
///   accounting"); the processMemory and memorySubsystem "other" samples subtract its GetResidentBytes() (Evidence.md
///   "Row source bindings"), so up to 128 rows × 864,000 bytes never inflate the run's own memory rows.
/// Thread affinity: MAIN_THREAD except AddSample, GetRows and FindRow (ANY_THREAD: the rows are immutable after Create, and
///   AddSample pushes onto CORE's Zenith_BoundedQueue of 4,096 samples, QUEUE_FULL_POLICY_REJECT, drained at SampleFrame).
/// Invalidation: None.
/// Units: Row units as rows.json states; times in ns of the present clock.
/// Errors: Create returns Zenith_Result<…, Zenith_BudgetRunError>; Write and FlushFrames return BUDGET_RUN_ERROR_WRITE.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 3. Source API. Never created in Shipping (the budget-run switches do not exist there,
///   [Z2-N-ARCH-878]). State is private members of this class.
/// @endcontract
class Zenith_BudgetRunRecord
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_BudgetRunRecord>, Zenith_BudgetRunError> Create(
		const Zenith_BudgetRunInputs& xInputs, Zenith_Heap& xHeap);

	Zenith_Span<const Zenith_BudgetRowSpec> GetRows() const;
	uint32 FindRow(Zenith_StringView strRowID) const;   // 0xFFFFFFFF when the run does not measure it

	// Area emitters, from any thread (a frame-graph node, Flux.BuildGraph, the main thread): one value for a row of kind
	// "emitted"; the last value per row since the previous SampleFrame wins, and SampleFrame attributes it to that frame.
	void AddSample(uint32 uRowIndex, double dValue);

	void MarkReady(uint64 ulPresentNs);      // the first present after PLAYER_EVENT_WORLD_READY
	void SampleFrame(const Zenith_Profiler& xProfiler, const Zenith_ProfilerFrameSummary& xFrame, uint64 ulPresentNs);
	void AddExternalSample(Zenith_StringView strKind, uint64 ulNs, double dValue);   // memory samplers, latency events
	bool HasMetMinimums() const;      // ≥ 120 s and ≥ 7,200 measured frames (30 min for a sustained run)
	// Every 4,096 sampled frames: appends the CSV buffer to the open frames file and flushes it.
	Zenith_Result<void, Zenith_BudgetRunError> FlushFrames(Zenith_EvidenceFileWriter& xFramesFile);
	// Shutdown step 4: the last CSV lines to xFramesFile, then the whole record (one Append and one Flush) to xRecordFile.
	Zenith_Result<void, Zenith_BudgetRunError> Write(Zenith_EvidenceFileWriter& xRecordFile,
		Zenith_EvidenceFileWriter& xFramesFile);
	uint64 GetResidentBytes() const;   // reported as budgetRun.recordBytes in the run telemetry
};
```

`Zenith_BudgetRunDriver` (ZenithRuntime) owns the run: it reads the two input files through PLAT's grant at startup step 7, creates the record, creates the two evidence files once each with `Zenith_CreateFileInCommandLineHostDirectory` (`<utc>-r<n>.frames.csv` at once, `<utc>-r<n>.json` at shutdown step 4; the function refuses an existing name, so each file is created exactly once and then only appended to through `Zenith_HostFile::Append` and `Flush`, `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Command-line host files") and wraps each in a `Zenith_EvidenceFileWriter`, never `Zenith_ProfilerOutput` (whose contract is whole-file writes below `user:/devtools/`), calls `MarkReady` on the first present after the world-ready event, `SampleFrame` at every frame-loop step 3, `AddExternalSample` from PLAT's process-memory sampler and RHI's GPU-memory sample, and when `HasMetMinimums()` (and the loop flag allows) requests shutdown; shutdown step 4 calls `Write`.

## Budget-run sampling

- A frame is sampled when it was presented; its frame time is the present interval; frames before `tReady` are not sampled (W-Load's boot row is the runner's MP-BOOT measurement).
- Each row's sample for the frame is computed from its source kind (§ "Row source bindings") and stored in the row's column; a source with no value this frame (a GPU frame not yet resolved) stores NaN, and NaN samples are excluded from the row's statistic (and counted in the record's `rows[]` entry as `missingFrames` in the run telemetry, never in the evidence block).
- Frames inside `[tReady, tReady + 10 s)` are flagged excluded ([Z2-N-BUDGET-066], [Z2-N-BUDGET-067]) and kept in the CSV with `excluded = 1`; no other interval is excluded.
- Memory samples: `Test_BudgetRunRecord_MemorySamplesExcludeRecord` feeds a `processMemory` and a `memorySubsystem` (`other`) row external samples and asserts each sampled value is the input minus the record's `GetResidentBytes()`, and that a `memorySubsystem` row of another subsystem is unchanged.
- Profiler state: the runner passes `devtools.profiler.enabled=false` unless a row has `needsProfiler`, and the overlay is hidden in every run except the overhead pair's enabled half, which is the [Z2-N-BUDGET-107] rule; counters are aggregated in counters-only mode, so `counter` rows work with the profiler disabled. GPU timings do not follow the zone mode: with `devtools.profiler.gpuTimings` set (its default outside Shipping, never changed by the runner) the profiler ingests every submission in counters-only mode too and builds the frame's GPU zones and per-pass sums, so `gpuPassSum` rows measure real timestamps while the profiler's zones and overlay are off, which is what [Z2-N-BUDGET-107] asks (the overlay off, the data the procedure reads still present; [Z2-N-REND-312] makes the timestamps the source of MP-GPUPASS). `Test_Profiler_CountersOnly_GpuTimingsStillIngested` and `Test_BudgetRunRecord_GpuPassSumInCountersOnly` assert it.
- Tracy is off in every budget run (`devtools.tracy.enabled=false` from the runner).

## Statistics

Nearest rank, no interpolation, no outlier removal (`Zenith2/Docs/Budgets.md` → "Procedure parameters"): for `n` non-excluded, non-NaN samples sorted ascending, the `p`-th percentile is the sample at rank `ceil(p/100 × n)` (1-based); `max` and `min` are the extremes; `count` the number of samples equal to 1 (hitch rows); `total` the sum; `median` rank `ceil(n/2)`; `rollingMin` (MP-SUSTAIN) the minimum over the run of the mean fps of each 1-s window of present timestamps; `totalWorkP95` (MP-CPUZONE) the p95 of the per-frame sum of the node's sub-job work. The functions live in `Internal/Evidence/Zenith_BudgetStatistics.cpp` and are tested against hand-computed fixtures (`Test_BudgetStatistics_NearestRank`).

## Evidence record writer

`Write` produces `<utc>-r<n>.json` in the evidence directory with exactly the block's fields in the block's order ([Z2-N-BUDGET-095], [Z2-N-BUDGET-096]), written with `Zenith_JsonWriter` pretty style:

| Field | Source |
|---|---|
| `schema`, `treeSha`, `preset`, `configuration`, `workload`, `workloadVersion`, `platform`, `backend`, `conditions`, `cacheState`, `repetition` | the inputs |
| `device` | PLAT's device information (CPU, GPU, RAM, driver, OS build) |
| `fingerprint` | `fingerprint.json` copied verbatim |
| `tReadyNs`, `excludedIntervals` | `MarkReady`; the single initial-load window |
| `measuredFrames`, `measuredSeconds`, `loops` | the sampled frames; the scripted input's loop count (PLAT's input-script loop counter, row DV13) |
| `dynresHistogram`, `framesAtFloor` | the `dynres` source in 1% buckets; frames at the floor |
| `framesInFlight` | the configured value |
| `hitchCount`, `maxFrameMs` | MP-HITCH over the measured interval |
| `gfxSource`, `gfxSamples`, `gfxSampleCostNs` | PLAT's S24 Graphics sampler; `null`, `[]`, `[]` on Windows |
| `sustained` | MP-SUSTAIN data, else `null` |
| `latencySamples`, `walkthroughSteps` | MP-LATENCY events; `[]` |
| `rows` | one entry per `rows.json` row: `id`, `statistic`, `measured`, `unit`, `value`, `tolerance`, `result` (`pass` when measured ≤ value + tolerance for upper bounds and ≥ for lower bounds, `fail`, or `report` for pending rows and `totalWorkP95`) |
| `artifact` | the CSV's file name |
| `result` | `pass` when every non-report row passes and the run is valid; `invalid` when the run-length or exclusion rule fails; `fail` otherwise |

## Per-frame capture CSV

`<utc>-r<n>.frames.csv` beside the record, header exactly `frameIndex,presentNs,frameMs,cpuSimMs,cpuRenderMs,gpuMs,scaleX,scaleY,excluded,hitch` ([Z2-N-BUDGET-104]), one line per presented frame after `tReady`, written incrementally into a 1 MiB buffer appended to the one open frames file (`FlushFrames`, a `Zenith_HostFile` created once and appended to, so the second flush never collides with the create-new rule) every 4,096 frames and at `Write`; numbers with `%.3f` for milliseconds and integers otherwise; `scaleX` and `scaleY` as fractions with 4 decimals.

## Measured interval and run length

- The run ends when both minimums of [Z2-N-BUDGET-070] are met after the exclusion window (30 min for MP-SUSTAIN, [Z2-N-BUDGET-094]); the driver then requests shutdown with exit code 0.
- `devtools.budgetRun.loop=true` (the definition's `loopUntilMinimums`) makes PLAT's input-script player restart the script at its end until `HasMetMinimums()` (row DV13: the input script's loop flag and its loop counter), so the loop count is reported ([Z2-N-BUDGET-072]).
- A script that ends before the minimums without the loop flag ends the run as `invalid`.
- `uBUDGET_RUN_MAX_FRAMES` (216,000) bounds the sample store; a run beyond it stops sampling and is reported `invalid` with the reason.

## Area row emitters

An area whose row's quantity is computed inside its own code (not a counter, zone or GPU pass) emits it into the record. DEVT supports both shapes the waiting tickets use, and an emitter of either shape finds its rows once with `FindRow` (ANY_THREAD) and calls `AddSample` (ANY_THREAD):

| Shape | Signature and caller | Who uses it | How it obtains the record |
|---|---|---|---|
| node-side | the area's own function with whatever arguments it needs, called from the area's own code on whatever thread that code runs (a frame-graph node on a worker, `Flux.BuildGraph`), once per frame | ENV (Z2T-ENV-070): `Zenith_EmitEnvironmentBudgetRows(Zenith_BudgetRunRecord&, const Zenith_EnvironmentFrameTimings&)` called by `Environment.ExtractEnvironment`, and `Flux_EmitEnvironmentGpuRows(Zenith_BudgetRunRecord&, const Flux_PassTimings&)` called on `Flux.BuildGraph` | a module through `Zenith_ModuleStartContext::GetBudgetRunRecord()` (`Zenith_BudgetRunRecord*`, null outside a budget run; row DV12, X245 (c)); ZenithFlux through `Flux_RendererDesc::m_pxBudgetRunRecord` (the same pointer, null outside a budget run; row DV5) |
| driver-called | `void (*)(Zenith_BudgetRunRecord&)`, called by ZenithRuntime's budget-run driver at frame-loop step 3 on the main thread for every registered emitter whose key appears in a measured row's `emitter` member | GEOM (Z2T-GEOM-118): `Flux_EmitGeomBudgetRows(Zenith_BudgetRunRecord&)` | registered at startup step 13 by ZenithRuntime for the libraries it links, keyed by the `emitter` member of `RowSources.json` (`GEOM`, …), and by modules through `Zenith_ModuleStartContext::RegisterBudgetEmitter(const char* szKey, void (*)(Zenith_BudgetRunRecord&))` |

- Attribution: `SampleFrame` (frame-loop step 3 of iteration k) drains the sample queue and gives each `emitted` row the last value added since the previous `SampleFrame`, so a node-side value emitted during iteration k's graph and a driver-called value emitted at the same step 3 land in the same frame's sample; a row with no value since the previous `SampleFrame` samples NaN (excluded from its statistic, § "Budget-run sampling").
- Cost and safety: `AddSample` copies 16 bytes into CORE's `Zenith_BoundedQueue` (no lock, no allocation); a full queue (more than 4,096 samples between two `SampleFrame` calls) refuses the sample and counts `devtools.evidence.samples_dropped_delta`.
- GPU rows need no emitter: a row whose quantity is a pass time binds as `gpuPassSum` with its pass names in `RowSources.json` and is measured from the profiler's GPU zones in either mode (§ "Budget-run sampling"). ENV's `Flux_EmitEnvironmentGpuRows` therefore duplicates a `gpuPassSum` binding of Z2-B-ENV-001; row DV26 tells ENV it may keep it (it works as a node-side emitter) or replace it with the binding, which removes a ZenithFlux file.
- `Test_BudgetRunRecord_GpuPassSumAcrossViews`: GPU zones `DebugOverlay[Main]` 0.10 ms, `DebugOverlay[Editor]` 0.05 ms and `DebugOverlayX[Main]` 0.20 ms give `DebugOverlay` 0.15 ms, `DebugOverlay[Main]` 0.10 ms and `DebugOverlay*` 0.35 ms; a frame without the pass samples NaN.
- `Test_BudgetRunRecord_EmitterAddSample` (two threads add samples to one row between two `SampleFrame` calls; the last value added wins, and a frame with none samples NaN) and `Test_BudgetRunRecord_StressAddSample` (2, 8 and the hardware concurrency threads, [Z2-N-CONTRACT-357]) cover both shapes.

## Run telemetry

```cpp
// Zenith2/Engine/ZenithProfiling/Zenith_Telemetry.h
#pragma once
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithProfiling/Zenith_Profiler.h"

/// @contract enum Zenith_TelemetryEventKind
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 2. The names are written into run.json; never renumbered.
/// @endcontract
enum Zenith_TelemetryEventKind : uint8
{
	TELEMETRY_EVENT_MARKER = 1, TELEMETRY_EVENT_DEVICE_LOST, TELEMETRY_EVENT_DEVICE_RECOVERED, TELEMETRY_EVENT_FRAME_FAILURE,
	TELEMETRY_EVENT_WORLD_TRANSITION, TELEMETRY_EVENT_LIFECYCLE, TELEMETRY_EVENT_MEMORY_LEVEL, TELEMETRY_EVENT_HITCH,
};

/// @contract value Zenith_TelemetryEvent
/// Ownership: Plain value copied by RecordEvent.
/// Thread affinity: IMMUTABLE; members non-const: the telemetry queue's pop assigns into the caller's element, and
///   producers fill m_acText after construction.
/// Invalidation: None.
/// Units: m_ulMonotonicNs nanoseconds of Zenith_GetMonotonicTimeNs; m_iValue as the kind defines (a loss count, a level).
/// Errors: None; m_acText is cut at 95 bytes and NUL-terminated.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_TelemetryEvent
{
	Zenith_TelemetryEventKind m_eKind = TELEMETRY_EVENT_MARKER;
	uint64 m_ulMonotonicNs = 0;
	uint64 m_ulFrameIndex = 0;
	char m_acText[96] = {};
	sint64 m_iValue = 0;
};

/// @contract value Zenith_RunTelemetryDesc
/// Ownership: Plain value copied by Create; the output and heap outlive the telemetry; the counter names are copied.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Create returns PROFILER_ERROR_NOT_IN_SHIPPING in Shipping, PROFILER_ERROR_INVALID_DESC for a null output or more
///   than 32 frame counters.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Zenith_RunTelemetryDesc
{
	Zenith_ProfilerOutput* const m_pxOutput = nullptr;              // the runtime's user-mount output
	const Zenith_StringView m_strRunStem{};
	const Zenith_Span<const Zenith_StringView> m_axFrameCounters{}; // devtools.telemetry.counters, at most 32
	Zenith_Heap* const m_pxHeap = nullptr;
};

/// @contract type Zenith_RunTelemetry
/// Ownership: Created by Create as a Zenith_Owned<Zenith_RunTelemetry>, owned by ZenithRuntime's Zenith_DevTools; never
///   created when devtools.telemetry.enabled is false (the Shipping value), and Create refuses in Shipping
///   (if constexpr (Zenith_IsShippingBuild()) { return Zenith_Err(PROFILER_ERROR_NOT_IN_SHIPPING); }). Owns the event queue
///   (CORE's Zenith_BoundedQueue of 256 events of 128 bytes in 192-byte cells, QUEUE_FULL_POLICY_REJECT), the run values and
///   the 512 KiB frame CSV buffer, all allocated by Create (589,824 bytes); state is private members of this class.
/// Thread affinity: MAIN_THREAD except RecordEvent (ANY_THREAD) and GetResidentBytes (ANY_THREAD).
/// Invalidation: None.
/// Units: As the members state.
/// Errors: Create returns Zenith_Result<…, Zenith_ProfilerError>; Flush returns PROFILER_ERROR_NO_OUTPUT after Detach and
///   otherwise success (a refused write is counted in telemetry.writes_refused_delta); a full event queue drops the event
///   and counts telemetry.events.dropped.
/// Reentrancy: NONE.
/// Version: since Z2 Stage 2. Source API.
/// @endcontract
class Zenith_RunTelemetry
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_RunTelemetry>, Zenith_ProfilerError> Create(const Zenith_RunTelemetryDesc& xDesc);
	~Zenith_RunTelemetry();
	void RecordEvent(const Zenith_TelemetryEvent& xEvent);            // ANY_THREAD
	void SetRunValue(Zenith_StringView strKey, Zenith_StringView strValue);   // MAIN_THREAD: per-run facts (shading path, aliasing mode, cold-PSO flag)
	void EndFrame(const Zenith_Profiler& xProfiler, const Zenith_ProfilerFrameSummary& xFrame);   // step 3
	Zenith_Result<void, Zenith_ProfilerError> Flush();  // writes run.json and the next frames chunk file
	void Detach();                                                  // MOD shutdown step 6: a last Flush, then no output
	uint64 GetResidentBytes() const;
};
```

- `user:/devtools/telemetry/<stem>.run.json` (schema `zenith2.run-telemetry`, version 1): build identity, composition hash, platform, backend, game, start and end UTC, frame count, the per-run values (RHI's device-loss and recovery counts and cold- or warm-PSO flag, RPROD's shading path per view kind, tier, aliasing mode, mean and peak transient memory, PSO counts, pacing p50/p95/p99, ECS's per-frame telemetry names, and in a budget run `budgetRun.recordBytes`), the events in order, and the frame-time summary (mean, p50, p95, p99, max, hitches).
- RHI records `TELEMETRY_EVENT_DEVICE_LOST`/`RECOVERED`/`FRAME_FAILURE` through the `Zenith_RunTelemetry*` the renderer receives (row DV5 adds `Flux_RendererDesc::m_pxTelemetry` beside `m_pxGpuTimingSink`); the runtime records world transitions, lifecycle changes and memory levels.
- `Flush` (which writes `run.json` and, when at least one row is buffered, the next frames chunk) runs as soon as the 512 KiB frame buffer cannot hold the next row, at `devtools.telemetry.flush`, at shutdown step 4 and from `Detach`; every 60 s `EndFrame` rewrites `run.json` alone, without cutting a chunk, so chunks are full-sized except the last and those an explicit flush cut. Neither blocks (writes go through the output, whose detach waits for them, `Zenith2/Docs/Design/DEVT.md` → "Cancellation and shutdown").
- Shipping: `devtools.telemetry.enabled` is false and its layer mask admits the engine default alone there (`Zenith2/Docs/Design/DEVT.md` → "Configuration keys"; `Test_DevToolsConfig_ShippingMasks` asserts the mask), so the runtime never creates the telemetry, `Create` itself refuses there, and no telemetry file is written in Shipping (`Test_RunTelemetry_RefusedInShipping` and `Test_DevTools_ShippingCreatesNoDevServices`, `ShippingTest` cases of `Zenith2/Docs/Design/DEVT/Tests.md` → "ShippingTest cases").

## Frame telemetry

Numbered chunk files `user:/devtools/telemetry/<stem>.frames.<NNNN>.csv` (`NNNN` the decimal chunk sequence from 0000), because the output, `Zenith_ProfilerOutput::WriteFile`, writes whole files atomically and cannot append (the log file sink's chunk rule, `Zenith2/Docs/Design/DEVT/Console.md` → "Log file sink"). Each chunk begins with the header `frameIndex,frameUs,cpuSimUs,cpuRenderUs,gpuUs,scaleX,flags,<counter 1>,…,<counter N>` and holds the frame rows buffered since the previous `Flush` (at most 512 KiB), one line per frame, where the counters are `devtools.telemetry.counters` (default `jobs.overhead_ns,ecs.playback_us_delta,sim.ticks,memory.process_bytes`, at most 32 names, unknown names written as empty cells). Every `Flush` with at least one buffered row writes the next chunk (the 60-s timer never does, § "Run telemetry"), so the chunk count follows the bytes written, not the session's length. The cap is by bytes: frame telemetry stops, with one warning and `framesTruncated: true` in `run.json`, when the next chunk would bring the run's chunk bytes above 64 MiB (67,108,864, at most 128 full chunks of 512 KiB), or when the chunk sequence would pass 9999 (`NNNN` stays four digits; only explicit flushes can cut that many small chunks). Readers (`ZenithDevTool log view` does not read them; `ZenithDevTool dashboard build` does) concatenate a run's chunks in sequence order, skipping each chunk's header after the first. `Test_RunTelemetry_EventsAndFlush` asserts that two flushes with rows between them produce `…frames.0000.csv` and `…frames.0001.csv`, each starting with the header, whose concatenated rows are every frame in order with none repeated.

## Regression dashboards

`ZenithDevTool dashboard build --evidence <dir>… --telemetry <dir>… --cook-report <file>… --out <dir>` reads every evidence record (validated with the same rules as `Test-Zenith2EvidenceRecord` except the fingerprint recomputation, which needs the tree), run telemetry (each run's `run.json` and its frame chunks in sequence order, § "Frame telemetry") and ASSET's cook reports (`.cook/cook-report.json`) and writes:

- `<out>/dashboard.json` (schema `zenith2.dashboard`, version 1): per (row, workload, platform, backend) the series of `{ treeSha, utc, measured, value, result }` ordered by UTC; per run-telemetry key its series; per cook report the asset counts, sizes and cook times by kind.
- `<out>/index.html`: one self-contained static page (inline SVG charts, no script from the network, no external fonts), one chart per row series with the row's value line and the regression boundary (10% over the value, `Zenith2/Docs/Budgets.md` → "Procedure parameters") marked; points over the boundary red; a table of the latest result per row.
- Committed research evidence under `Zenith2/Docs/Research/Budgets/**` is the long-term input; loop and CI evidence under `Zenith2/out/` is the short-term input. Dashboards are generated, never committed.
