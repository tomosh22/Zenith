# SEC design: fuzzing policy

This file is part of the SEC design family; the entry document is `Zenith2/Docs/Design/SEC.md`. It is the programme-wide fuzzing policy: what every fuzz target must do beyond DEVT's conventions, how targets are named and built (including tool-side parsers), which sanitizer configurations run which inputs, the run-time budget at each gate and acceptance, and how a finding becomes tickets. The conventions themselves (source location, corpus directory, dictionary, regressions, the 60-second smoke) are DEVT's (`Zenith2/Docs/Design/DEVT/QualityTools.md` → "Fuzzing"), the campaign runner is DEVT's (`Zenith2/Docs/Design/DEVT/QualityTools.md` → "Fuzz campaign runner"), and the generic fuzz rule that creates `Zenith_Fuzz_<Stem>` is the scaffold's (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Fuzz target registration"); this file cites them and adds only what they leave to SEC.

## Scope of this file

| Topic | Heading |
|---|---|
| the delta over DEVT's conventions | "What the policy adds to the DEVT conventions" |
| harness, corpus, dictionary and naming rules | "Harness rules", "Corpus and dictionary rules", "Stem naming rule" |
| fuzzing parsers that live in host tools | "Tool-side fuzz targets" |
| sanitizers | "Sanitizer configurations" |
| gate and acceptance time | "Run-time budgets" |
| findings to tickets | "Crash triage" |
| what the acceptance records | "Campaign evidence" |

## What the policy adds to the DEVT conventions

| # | Addition | Heading |
|---|---|---|
| A1 | every fuzz target is a row of the inventory, and every parser row has a target or a status that explains why not | `Zenith2/Docs/Design/SEC/ParserInventory.md` → "Inventory rules" |
| A2 | harness rules H1 to H10 | "Harness rules" |
| A3 | corpus rules C1 to C6, including regression inputs as `.hex` text | "Corpus and dictionary rules" |
| A4 | the stem rule | "Stem naming rule" |
| A5 | `zenith_tool_fuzz_target` for parsers in `ZENITH_TOOLS_HOST` targets | "Tool-side fuzz targets" |
| A6 | sanitizer options and the three replay configurations | "Sanitizer configurations" |
| A7 | smoke selection by the diff, the step budget formula and the tier durations of the acceptance campaigns | "Run-time budgets" |
| A8 | triage: deduplication, severity, the recording and fix tickets | "Crash triage" |

## Harness rules

| # | Rule |
|---|---|
| H1 | The source is `Tests/Fuzz/<Stem>.cpp` of the library (or tool, "Tool-side fuzz targets") that owns the parser, and defines exactly `extern "C" int LLVMFuzzerTestOneInput(const uint8_t* pData, size_t ulSize)` returning 0, as DEVT's convention states (libFuzzer's own signature, so its `uint8_t` is kept); `LLVMFuzzerInitialize` is not defined. |
| H2 | The input reaches the parser through its public entry function, or, where the inventory row names an internal parse function, through that function from the library's own tests directory; the harness never re-implements a check. |
| H3 | No file, socket, clock or environment access: the parser reads from the input span or from an in-memory source (ASSET's in-memory container source, PACK's `Zenith_MemoryInstallFileSystem`, a memory mount source), so an input's outcome depends on its bytes alone. |
| H4 | Every object the parser creates is created and destroyed inside the call; nothing persists across inputs. The harness compares the test heap's statistics (`Zenith2/Docs/Design/CORE/Memory.md` → "Statistics") before and after the input and fails `ZENITH_CHECK` on a difference, which libFuzzer reports as a crash of kind `leak` ("Finding records"); MSVC's AddressSanitizer has no leak detector, so this check is the leak evidence. |
| H5 | An allocation the parser sizes from a declared count or length is checked against the parser's limits before it is made ([Z2-N-ARCH-951]); the harness passes an arena of at most 1 GiB where the API takes one, and `-rss_limit_mb=2048` reports any growth past that as kind `oom`. |
| H6 | The harness asserts invariants only: for a parser with a writer, an accepted input re-encodes and re-parses to an equal value; for a validator, every rejection is a `Zenith_Result` error carrying a code of the parser's enumeration. Asserts are enabled in the `Sanitize` configuration, so any `ZENITH_ASSERT` or `ZENITH_CHECK` reached inside the parser is a finding of kind `assert`. |
| H7 | Randomness the harness needs (for example to sample a decoded clip at random times) is a `Zenith_RandomSource` seeded from the first 8 bytes of the input, so a crash input reproduces. |
| H8 | One input runs for at most 10 s (`-timeout=10`); a parser whose work can grow faster than linearly in its input enforces a limit that keeps a maximal input under 1 s on the reference desktop. |
| H9 | A source that drives several parsers selects one with `pData[0] % uParserCount` and passes `pData + 1`; each driven parser is its own inventory row naming the same stem, and the stem's tier is the lowest of those rows. |
| H10 | The maximum input length is the row's `maxLen` (registry field, default 65,536 bytes, at most 16,777,216), passed as `-max_len` to every run. |

## Corpus and dictionary rules

| # | Rule |
|---|---|
| C1 | At least one valid input per parser the target drives. |
| C2 | For a parser whose design enumerates its checks (for example U0 to U11, A1 to A7, VS1 to VS6), one seed per check, each failing exactly that check. |
| C3 | Text seeds (JSON, paths, Luau source, console lines) are committed under `Tests/Fuzz/Corpus/<Stem>/`, at most 64 KiB each and 256 per target (DEVT). |
| C4 | Binary seeds are never committed: a doctest case in suite `z2-fixture-setup` writes them to `${ZENITH_OUT}/test-fixtures/<AREA>/Fuzz/<Stem>/` ([Z2-N-ARCH-257]), and the area appends `FIXTURES_REQUIRED <Fixture>` and the extra corpus argument to `ZENITH_FUZZ_SMOKE_PROPERTIES_<Stem>` (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Registry tail"). |
| C5 | A regression input is committed as text, `Tests/Fuzz/Regressions/<Stem>/<sha1>.hex`: lower-case hexadecimal, 64 input bytes per line, LF endings, no other character, where `<sha1>` is the SHA-1 of the decoded bytes; `Test_<Stem>_Regressions` decodes each file with CORE's `Zenith_DecodeHexRegression` and replays it ("HWASan replay on the S24"; row SEC3 asks DEVT to adopt the `.hex` form, which keeps regressions inside the binary-artefact ruling and writable by a worker). An input of at most 4,096 bytes (8,192 digits in 64 lines) may instead be carried as `.hex` text inside the fix ticket's body, so the fix ticket's worker commits it and no recording ticket is needed ("Finding ticket body"); a larger input is always committed by the `needs-human` recording ticket. |
| C6 | A binary format with a magic number has a dictionary `Tests/Fuzz/<Stem>.dict` holding the magic, every enumerated version and kind value and every 4-byte section tag; a JSON parser's dictionary holds every key of its schema. |

## Stem naming rule

- A stem matches `^[A-Z][A-Za-z0-9]*(_[A-Z][A-Za-z0-9]*)*$`, is at most 48 characters, and does not begin with `Fuzz` or `Zenith`, because the generic rule already prefixes `Zenith_Fuzz_`.
- A stem names the format or the entry function it fuzzes (`SoundFile`, `VirtualPath`); an area prefix joined by one underscore is allowed (`Script_Manifest`).
- A stem is unique programme-wide; the generic rule already refuses a duplicate at configure time.
- SecurityConformance check SC08 enforces the rule (`Zenith2/Docs/Design/SEC/ConformanceCheck.md` → "Checks").

## Tool-side fuzz targets

A parser that lives in a `ZENITH_TOOLS_HOST` target (`ZenithCook`, `ZenithAuthoring`, `ZenithEditorCore`, `ZenithEditor`, `ZenithNewGameScaffold`) has no `win-asan` build of its own, so the generic rule never sees its `Tests/Fuzz/` sources. There is one registration form for them, `zenith_tool_fuzz_target`; ASSET's `zenith_cook_fuzz_target` (ticket Z2T-ASSET-035, called by Z2T-ASSET-035, -039 and -075, Z2T-ANIM-029 and Z2T-VFX-012) is a thin wrapper over it. SHIP owns the helper as one of the test registry's functions (`Zenith2/Docs/Design/SHIP/CMakeHelpers.md` → "Registry functions"; row SEC6 (g)), defined before or in Z2T-ASSET-035; this section is SEC's specification of it:

```cmake
zenith_tool_fuzz_target(<Stem>
	TOOL <ToolTarget>                     # a target of the table "Tool directories" below
	TOOL_DIR <directory>                  # that table's directory for <ToolTarget>, relative to Zenith2/
	SOURCES <relative path>...            # each under <TOOL_DIR>/Internal/, never the tool's main TU
	DEPENDS <Library>...                  # libraries the sources need; each must exist in win-asan (rule 5)
	[THIRD_PARTY <ManifestName>...])      # entries the sources include

# The ZenithCook wrapper, the form the ASSET, ANIM and VFX tickets call:
function(zenith_cook_fuzz_target STEM)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;DEPENDS;THIRD_PARTY")
	zenith_tool_fuzz_target(${STEM} TOOL ZenithCook TOOL_DIR Tools/ZenithCook
		SOURCES ${ARG_SOURCES} DEPENDS ${ARG_DEPENDS} THIRD_PARTY ${ARG_THIRD_PARTY})
endfunction()
```

Tool directories (`Zenith2/Docs/Architecture.md` → "Repository layout"):

| `TOOL` | `TOOL_DIR` | Kind |
|---|---|---|
| `ZenithCook` | `Tools/ZenithCook` | executable |
| `ZenithEditor` | `Tools/ZenithEditor` | executable |
| `ZenithAuthoring` | `Tools/Authoring` | static tool library |
| `ZenithEditorCore` | `Tools/EditorCore` | static tool library |
| `ZenithNewGameScaffold` | `Tools/NewGameScaffold` | static tool library |

Algorithm, run at configure in every preset:

1. Append `<Stem>|<ToolTarget>|<SOURCES joined by ;>` to the global property `ZENITH_TOOL_FUZZ_TARGETS` in every preset, so the declaration is recorded wherever it is configured ([Z2-N-ARCH-183]); `zenith_end_build` includes the test registry before it writes `declared_deps.json`, so the property is complete when it writes it as the array `toolFuzzTargets[]` of `{ stem, tool, sources }` objects sorted by `stem` (a new array, no `schemaVersion` change, in the form [Z2-N-ARCH-182] fixes; row SEC6 (c)).
2. Require `TOOL` to be a row of "Tool directories" and `TOOL_DIR` to equal that row's directory; fail with `Zenith2: zenith_tool_fuzz_target(<Stem>): TOOL_DIR <value> is not the directory of <ToolTarget> (<expected>)` otherwise, in every preset.
3. When the configure preset is not `win-asan` (`ZENITH_SANITIZER` unset), return.
4. Require `Zenith2/<TOOL_DIR>/Tests/Fuzz/<Stem>.cpp`; fail with `Zenith2: zenith_tool_fuzz_target(<Stem>): <path> does not exist` otherwise.
5. Require every `SOURCES` entry to begin `Internal/`, to exist under `Zenith2/<TOOL_DIR>/`, and not to be `Internal/Zenith_<ToolTarget>Main.cpp`; fail naming the entry otherwise.
6. Require every `DEPENDS` entry to be a library target present in this preset; fail with `Zenith2: zenith_tool_fuzz_target(<Stem>): <Library> is not built in win-asan` otherwise. A tool executable is never a `DEPENDS` entry: the sources it needs are named in `SOURCES`. A static tool library is a valid entry exactly when it is built in `win-asan` (below).
7. Create the executable `Zenith_Fuzz_<Stem>` from the fuzz source and the listed sources, with the include directories `Zenith2/Tools`, `Zenith2/<TOOL_DIR>` and `Zenith2/<TOOL_DIR>/Internal`, linking the `DEPENDS`, `THIRD_PARTY`, `ZenithTestSupport` and `/fsanitize=fuzzer` at compile and link, and register `Zenith_Fuzz_<Stem>.Smoke` with exactly the generic rule's properties (`-max_total_time=60`, the corpus `Zenith2/<TOOL_DIR>/Tests/Fuzz/Corpus/<Stem>` when it exists, label `z2-fuzz`, timeout 120, the pass and fail expressions, the `ENVIRONMENT` of "win-asan with libFuzzer", `ZENITH_FUZZ_SMOKE_PROPERTIES_<Stem>`).

Parsers hosted in a static tool library. `ZenithAuthoring`, `ZenithEditorCore` and `ZenithNewGameScaffold` are `ZENITH_TOOLS_HOST` (`win` only; `Zenith2/Docs/Architecture/BuildSystem.md` → "The ZENITH_SUPPORTED matrix"), so a parser inside them (ANIM's documents and graph compiler, TWOD's documents, MOD's `game.json` validator, CHAR's and WORLD's readers) cannot be linked in `win-asan`. SEC requests, through row SEC20 to SHIP and core-doc item X444, that these three static tool libraries take `ZENITH_WINDOWS` (`win` and `win-asan`), exactly as `ZenithContentCore` already does "whose fuzz targets need `win-asan`", while the tool executables stay `ZENITH_TOOLS_HOST`. Their own dependencies are `ZENITH_GPU` or `ZENITH_ALL` libraries, all present in `win-asan`, so the preset model admits the change. Once it lands, such a parser's target is `zenith_tool_fuzz_target(<Stem> TOOL ZenithAuthoring TOOL_DIR Tools/Authoring DEPENDS ZenithAuthoring …)` (with `SOURCES` empty or naming further `Internal/` files), and a `DEPENDS ZenithAuthoring` in a ZenithCook target (Z2T-ANIM-029) is valid. If SHIP's or Architecture's answer refuses the change, the parser's fuzz target moves instead to the engine library that owns the parsed format (ANIM's documents to `ZenithAnimation`, TWOD's to `Zenith2D`), with only the format-reading code moving there, and the inventory row's `library` changes in that diff. Until one of the two lands, `Z2T-ANIM-029`'s call (`DEPENDS … ZenithAnimation ZenithAuthoring`) fails rule 6 in `win-asan` and is listed as incompatible in finding F1 (`Zenith2/Docs/Design/SEC/ParserInventory.md` → "Conformance findings").

- `zenith_add_tool` keeps `Tests/Fuzz/**` out of `<Tool>_Tests` and `<Tool>_Objects` in every preset, as [Z2-N-ARCH-227] does for libraries (row SEC6 (g); Z2T-ASSET-035's Definition of Done already expects it for `MetaDocument.cpp`).
- The generic rule refuses a stem the helper already created, because both use the target name `Zenith_Fuzz_<Stem>`; the two never create the same target.
- SecurityConformance check SC10 passes a fuzz source in a `ZENITH_TOOLS_HOST` target exactly when its stem is in `toolFuzzTargets[]`. The calls of Z2T-ASSET-035, -039, -075 and Z2T-VFX-012 pass rules 2 to 6 through the wrapper as written; Z2T-ANIM-029's passes once the request above lands.

## Sanitizer configurations

### win-asan with libFuzzer

- The preset and flags are Architecture's (`Zenith2/Docs/Architecture/BuildSystem.md` → "MSVC flags for the win and win-asan presets"): the `Sanitize` configuration with `/fsanitize=address`, fuzz targets adding `/fsanitize=fuzzer`, asserts enabled.
- The `Sanitizers -Preset win-asan` step runs every test with PROC's `ASAN_OPTIONS=halt_on_error=1:detect_leaks=0:suppressions=<asan.supp>` (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "win-asan"). Every fuzz smoke and campaign run additionally needs `abort_on_error=1:allocator_may_return_null=1:symbolize=1`, so a heap request the allocator cannot satisfy returns null to the engine heap (whose failure path the parser must handle) instead of terminating the run. The generic fuzz rule and `zenith_tool_fuzz_target` set, on each `Zenith_Fuzz_<Stem>.Smoke` entry, the CTest property `ENVIRONMENT "ASAN_OPTIONS=halt_on_error=1:detect_leaks=0:suppressions=<repo>/Zenith2/Tools/Gates/Suppressions/asan.supp:abort_on_error=1:allocator_may_return_null=1:symbolize=1"`, built at configure from the cache variable `ZENITH_ASAN_SUPPRESSIONS` (default `${ZENITH_ROOT}/Tools/Gates/Suppressions/asan.supp`, the path the step passes), so the step, SHIP's rule and the helper read one value and a suppression PROC adds with a decision applies to the smokes too (row SEC6 (b)); DEVT's runner sets the same string for campaigns (row SEC3).
- libFuzzer options of a smoke: DEVT's (`-max_total_time=60 -rss_limit_mb=2048 -timeout=10`) plus `-max_len=<maxLen>`, and `-dict=<dict>` for each target that has a dictionary. A campaign run adds `-use_value_profile=1 -print_final_stats=1`.

### linux-tsan replay with UBSan

Fuzz executables are not built in `linux-tsan`. In every `ZENITH_HOST` preset where its library is built (`win` always; `linux-tsan` for the libraries that preset builds), each fuzzed library's `Test_<Stem>_Regressions` replays, besides `Regressions/<Stem>/`, every committed seed of `Corpus/<Stem>/` and every generated seed of the fixture set (row SEC3), through CORE's `Zenith_FuzzRegression.h` ("HWASan replay on the S24"); under `linux-tsan` it runs with the `UBSAN_OPTIONS` [Z2-N-ARCH-220] fixes, so undefined behaviour on any seed is caught without libFuzzer. The registry field `ubsan` is `true` exactly for the entries whose library `linux-tsan` builds (per `declared_deps.json` `supported`). Every entry whose `ubsan` is `false` (its library is not built in `linux-tsan`: every GPU-matrix library and module, `ZenithContentCore`, the tool targets) has no UBSan replay; SecurityConformance check SC02 compares the field with `declared_deps.json`, so it cannot drift; they rely on ASan during the smoke and the campaign and on the HWASan device replay, an accepted residual (R10 of `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Accepted residual risks").

### HWASan replay on the S24

At every stage acceptance from Stage 3, the HWASan test APK runs `Test_<Stem>_Regressions` of every tier 1 and tier 2 library that targets Android, as a `needs-human` device step recorded in the acceptance evidence. MOD's push of device files (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Test APK device files", core-doc item X262; the fixture root is X208's) pushes `Zenith2/**/Tests/Fixtures` and the generated test outputs; row SEC5 (d) asks MOD to push, besides, exactly these four, so the replay has every input on the device:

| Pushed | From | Why |
|---|---|---|
| `Tests/Fuzz/Regressions/<Stem>/` | every library's tree | the committed regressions |
| `Tests/Fuzz/Corpus/<Stem>/` | every library's tree | the committed text seeds |
| `Tests/Fuzz/<Stem>.count` | every library's tree | the expected total; for a stem with only generated binary seeds it is also the sentinel that proves the push happened |
| `${ZENITH_OUT}/test-fixtures/<AREA>/Fuzz/<Stem>/` | the host's `z2-fixture-setup` run before the push ([Z2-N-ARCH-257]) | the generated seeds |

The replay never passes vacuously. The shared helper of row SEC18 (CORE), `Zenith2/Engine/ZenithCore/Tests/Support/Zenith_FuzzRegression.h` (built into `ZenithTestSupport`), declares:

```cpp
// libFuzzer's LLVMFuzzerTestOneInput signature exactly, third-party types included, so &LLVMFuzzerTestOneInput converts.
using Zenith_PfnFuzzEntry = int (*)(const uint8_t* pData, size_t ulSize);

// Rule C5: refuses any character outside [0-9a-f\n], an odd digit count, a line other than the last whose length is not
// 128 digits, and a file name that is not the SHA-1 of the decoded bytes.
Zenith_Result<void, Zenith_FuzzRegressionError> Zenith_DecodeHexRegression(Zenith_StringView strText,
	Zenith_StringView strFileName, Zenith_Array<uint8>& axBytesOut);

// Replays Regressions/<Stem>/*.hex and Corpus/<Stem>/* under strLibraryDir, and every file of strGeneratedSeedDir (empty
// when the stem has no fixture set), through pfnEntry; returns the total number of inputs replayed.
Zenith_Result<uint32, Zenith_FuzzRegressionError> Zenith_ForEachRegression(Zenith_StringView strLibraryDir,
	Zenith_StringView strStem, Zenith_StringView strGeneratedSeedDir, Zenith_PfnFuzzEntry pfnEntry);

// Reads Tests/Fuzz/<Stem>.count under strLibraryDir: a missing file, a value that is not one decimal line, or 0 is an error.
Zenith_Result<uint32, Zenith_FuzzRegressionError> Zenith_ReadRegressionCount(Zenith_StringView strLibraryDir,
	Zenith_StringView strStem);
```

`Tests/Fuzz/<Stem>.count` has one definition: the total number of inputs the replay must see on every host and on the device, that is the committed regressions plus the committed corpus files plus the generated seeds the stem's fixture set writes. Every stem with a `fuzz.stem` in the registry commits the file, at least 1 (rule C1 already requires one valid input per parser), and every diff that adds or removes a committed input, or changes how many seeds its fixture set writes, updates it. Every `Test_<Stem>_Regressions` calls `Zenith_ReadRegressionCount` and `Zenith_ForEachRegression` under `ZENITH_REPO_ROOT` and fails when the file is missing, when the count is 0, or when the replayed total differs from it; so a stem with committed regressions can never report 0, and a device run with nothing pushed fails on the missing count file (the sentinel) instead of passing. SecurityConformance check SC07 checks the file's presence and its floor.

## Run-time budgets

### Gate smoke

Every selected target runs one smoke of 60 s in the `Sanitizers -Preset win-asan` step (Z2-D-028; DEVT's convention), change-triggered through the `Zenith2Memory` category and PROC's `sanitizer_paths.json` (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizer path selection"). Every library and tool directory that owns a fuzz target joins that file's `win-asan` list and so the category's `paths` (hand-off SEC-P3), so a diff to any parser triggers the step.

### Smoke selection

Once PROC's path selection has decided the step runs (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Path selection"), the step does not run every smoke. With ticket context, given the changed paths (`Get-Zenith2ChangedPaths`), it selects target `T` when any of these holds, and excludes the other targets' `.Smoke` entries from its `ctest` run with `-E` (hand-off SEC-P2):

1. a changed path is under the directory of `T`'s owning library or tool (`declared_deps.json` `dir`);
2. a changed path is `T`'s source, corpus directory, dictionary or regression directory;
3. a changed path is under the directory of a library named in `T`'s registry field `smokeAlso` (the parser's dependencies whose change can alter its behaviour: `ZenithCore` for the JSON and UTF-8 based parsers, `ZenithCompression`, `ZenithCompression_Zlib` and `ZenithCompression_Zstd` for section decoders);
4. `T`'s entry in `Zenith2/Tools/Security/parser_inventory.json` differs between the merge base (`git show <base>:Zenith2/Tools/Security/parser_inventory.json`) and the working file, compared entry by entry by `id`.

Without ticket context (CI, acceptance, a manual run) the step runs every smoke, as PROC's step already runs fully there.

### Smoke step budget

With `N` selected targets and `J = min(8, ⌊logical processors / 2⌋)` smokes run concurrently (the step's `ctest --parallel` is at least `J`), the smoke part of the step takes `⌈N / J⌉ × 60 s`. When `⌈N / J⌉ × 60 s` exceeds 960 s, each smoke runs `max(15, ⌊960 / ⌈N / J⌉⌋)` s instead, so the `⌈N / J⌉` waves take at most 960 s (while the 15 s floor is not reached), and the step logs `Sanitizers: fuzz smoke shortened to <s> s for <N> targets`. With `J = 8` (a host of at least 16 logical processors) the full inventory of 133 targets gives `⌈133 / 8⌉ × 60 = 1,020 s`, so a run of every smoke (CI, acceptance) shortens each to `⌊960 / 17⌋ = 56 s` (17 waves × 56 s = 952 s); a ticket's own selection is normally a few targets and runs them for the full 60 s. 960 s is more than half the 30-minute warm cap (Z2-D-028) before the fuzz build and the ASan suites, so `Z2-B-PROC-025` (Step timeout Sanitizers win-asan, pending) is allocated by its producer to cover the build of every fuzz target, plus 960 s, plus the ASan test suites (row SEC10).

### Stage acceptance campaigns

Each stage acceptance from Stage 2 runs DEVT's campaign runner over every target present in the tree, as a `needs-human` step, with the per-target duration of its tier. This table governs every fuzz duration in the programme: no area design states its own campaign duration (an area's "Fuzz targets" section cites its rows' tiers):

| Tier | Stages 2 to 5 | Stage 6 |
|---|---|---|
| 1 | 600 s | 3,600 s |
| 2 | 180 s | 1,800 s |
| 3 | 60 s | 600 s |

```
pwsh -NoProfile -File Zenith2/Benchmarks/Fuzz/Run-FuzzCampaign.ps1 -Inventory Zenith2/Tools/Security/parser_inventory.json -Stage <N> -Jobs 8 -Out <dir>
```

`-Inventory` and `-Stage` make the runner read each target's seconds from the registry's `durations` table and its `maxLen` and dictionary from the entry (row SEC3 asks DEVT to add the two parameters; until then the human runs the runner once per tier with `-Targets` set to that tier's stems and `-SecondsPerTarget` set to its duration). The Stage 6 durations are the "full duration" of [Z2-N-ARCH-952], [Z2-N-ARCH-960] and SHIP's qualification item S1, and the value SHIP's row SH8 asks SEC to state.

### Campaign wall-time estimate

From `Zenith2/Docs/Design/SEC/ParserInventory.md` → "Tier totals and campaign estimate": at most 11.3 h for a Stage 2 to 5 campaign and 79.5 h for the Stage 6 campaign on the reference desktop with `-Jobs 8`; these are upper bounds over the full inventory, and an earlier stage's campaign runs only the targets present in the tree. Both are measured and reported by the two pending rows of `Zenith2/Docs/Design/SEC.md` → "Pending new rows"; the campaign runs while lanes are idle or on a second machine with the same accepted tree and toolchain (its evidence record's fingerprint is what binds it to the tree, "Campaign evidence").

## Crash triage

### Finding records

A finding is one entry of `campaign.json`'s `findings` array, or a failed smoke in a gate run. SEC needs two fields beyond DEVT's `{ kind, file, sha1, reproduced }`: `log`, the path of the reproduction's standard error, and `topFrames`, the first three symbolised frames of the report whose module is not the sanitizer runtime or libFuzzer (row SEC3). The `kind` values are:

| Kind | Recognised by |
|---|---|
| `crash-write` | an AddressSanitizer report whose access line reads `WRITE of size` |
| `crash-read` | an AddressSanitizer report whose access line reads `READ of size` |
| `use-after-free` | `heap-use-after-free` |
| `double-free` | `attempting double-free` |
| `segv` | `SEGV on unknown address` |
| `assert` | the engine's assertion or `ZENITH_CHECK` failure line (`Zenith2/Docs/Contracts.md` → "Assertions") |
| `leak` | H4's heap statistics check |
| `timeout` | libFuzzer's `timeout-` artefact |
| `oom` | libFuzzer's `oom-` artefact |

### Deduplication key

`<Stem>|<kind>|<f1>|<f2>|<f3>`, where `f1` to `f3` are the function names of `topFrames` with offsets, addresses and template arguments removed. Two findings with the same key are one defect; the triage tool keeps the one with the smallest input.

### Severity mapping

| Kind | Tier 1 | Tier 2 | Tier 3 |
|---|---|---|---|
| `crash-write`, `use-after-free`, `double-free` | `S1_CRITICAL` | `S1_CRITICAL` | `S2_MAJOR` |
| `crash-read`, `segv` | `S1_CRITICAL` | `S2_MAJOR` | `S3_MINOR` |
| `assert`, `leak` | `S2_MAJOR` | `S2_MAJOR` | `S3_MINOR` |
| `timeout`, `oom` | `S2_MAJOR` | `S3_MINOR` | `S3_MINOR` |

### Finding ticket body

For each deduplicated finding, `New-Zenith2FuzzFindingTickets.ps1` (`Zenith2/Docs/Design/SEC.md` → "Release-time and acceptance tools") writes two ticket bodies and one `.hex` file into its output directory; it files nothing. When the input is at most 4,096 bytes it writes no recording body: the fix body carries the `.hex` text in its Implementation Contract and its Definition of Done commits it (rule C5):

| File | Ticket | Content |
|---|---|---|
| `<key-hash>.rec.md` | TASK, `kind: maintenance`, label `needs-human`, risk LOW | commit `Tests/Fuzz/Regressions/<Stem>/<sha1>.hex` (the attached file, rule C5) and add the regression to the owning area's design test table in the same diff (ruling TESTS-ROW-IN-DIFF) |
| `<key-hash>.fix.md` | BUG, severity from "Severity mapping", repro `ALWAYS`, environment `win-asan`, complexity MODERATE, risk HIGH, blocked by the recording ticket | the ten sections: Goal (the parser, the kind, the dedup key, the governing [Z2-N-ARCH-951]); Implementation Contract (the inventory row's `sources` files to edit, the regression file to replay); Dependencies; Compatibility (none, or a format version bump when the fix changes accepted input); Definition of Done (`Test_<Stem>_Regressions` passes with the new file; the input yields a `Zenith_Result` error or its correct result; no assert); Validation (the smoke of `Zenith_Fuzz_<Stem>`; Review focus: the same defect class in the sibling parsers of the inventory row's library); Budgets (none); Ownership (the inventory row's owner area epic) |
| `<sha1>.hex` | — | the input in rule C5's form |

The tool also writes `findings.json` (`schema` `zenith2.fuzz-findings`, `schemaVersion` 1, one object per finding: `key`, `stem`, `kind`, `tier`, `severity`, `sha1`, `inputBytes`, `duplicateOf`, `bodies`).

### Finding lifecycle

| State | Event | Next state | Action |
|---|---|---|---|
| NEW | reproduced once by the runner | TRIAGED | dedup key computed |
| NEW | not reproduced | DISCARDED | listed in the summary as unreproduced |
| TRIAGED | key equals an open finding's key | DUPLICATE | the smaller input replaces the larger in the open recording ticket |
| TRIAGED | key is new | FILED | the person files the two bodies with `zagent create` |
| FILED | recording ticket Done | RECORDED | the `.hex` file is in the tree; the fix ticket becomes claimable |
| RECORDED | fix ticket Done | FIXED | `Test_<Stem>_Regressions` passes in its gates |
| FIXED | the next campaign or smoke shows no finding with that key | VERIFIED | the acceptance's campaign item passes |
| FIXED | a finding with the same key recurs | FILED | a new fix ticket is filed, citing the first |
| FILED | a ruling accepts the finding (tier 3 `timeout` or `oom` only) | ACCEPTED | the ruling's `Z2-D-NNN` is recorded in the inventory row's `acceptedFindings` |

### Regression rule

- A reproduced finding of a tier 1 target blocks its stage acceptance until VERIFIED.
- A reproduced finding of a tier 2 or tier 3 target blocks the stage acceptance until VERIFIED or ACCEPTED.
- A smoke failure in a ticket's own gate run is that ticket's defect: the ticket fixes it in its own diff, and its input is committed as a regression in the same diff when the worker can reproduce it from the smoke log's input bytes (the gate prints the input as hex when it is at most 4,096 bytes); larger inputs go through the recording ticket.

## Campaign evidence

Each stage acceptance's campaign item records, through DEVT's evidence record (`Zenith2/Docs/Design/DEVT/Evidence.md` → "Evidence record writer"), the campaign's `campaign.json`, its summary, the inventory's SHA-256 and the evidence fingerprint (`Zenith2/Docs/Design/DEVT/Evidence.md` → "Evidence fingerprint specification"). The item passes when every target ran for its tier's duration, no reproduced finding is open other than ACCEPTED ones, and the fingerprint matches the accepted tree.
