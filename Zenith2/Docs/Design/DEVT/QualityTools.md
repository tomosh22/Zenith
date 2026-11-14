# DEVT design: quality tools

This file is part of the DEVT design family; the entry document is `Zenith2/Docs/Design/DEVT.md`. It specifies the `linux-tsan` host as DEVT delivers it in Stage 1 (deliverable 4 of `Zenith2/Docs/Roadmap.md`), the libgraph and complexity gates as DEVT requirements, the fuzzing conventions and the campaign runner, the clang-tidy configuration, content-validation reports, asset profiling, the `ZenithDevTool` host tool and capture conversion.

## Scope of this file

| Concern | Owner | Specified here |
|---|---|---|
| the `linux-tsan` preset, its flags, binary directory, runtime options and `Sanitizers.ps1` | ARCH and PROC (`Zenith2/Docs/Architecture/BuildSystem.md` → "The linux-tsan host") | the host's provisioning requirements, the runner humans and recording tickets use, the smoke suite, verification |
| the setup script, the manifest `wsl` and `llvm` pins, WSL provisioning | SHIP (`Zenith2/Docs/Design/SHIP/Manifest.md` → "WSL provisioning") | what DEVT's host needs from them (row DV16) |
| the `Libgraph`, `Complexity`, `Sanitizers` gate steps | PROC (protected) | the DEVT requirements their tickets list (row DV17) |
| fuzz target creation | ARCH (`Zenith2/Docs/Architecture/BuildSystem.md` → "Test, conformance and fuzz targets") | corpora, dictionaries, regressions, the campaign runner |
| the content validator and cook report | ASSET | the reports DEVT renders |

## The linux-tsan host

The host is a WSL2 distro on the reference desktop (and a native Linux runner in CI, [Z2-N-ARCH-221]) where the `linux-tsan` preset configures, builds and runs the CPU libraries' tests under ThreadSanitizer and UndefinedBehaviorSanitizer (Z2-D-011). Its parts and owners:

| Part | Realised by | DEVT's requirement |
|---|---|---|
| the distro | SHIP's setup step 1 (`wsl --import` of the pinned root filesystem into `%LOCALAPPDATA%\Zenith2\wsl\<distro>`) | § "WSL distro provisioning" |
| packages | SHIP's WSL step 3, as root (pinned `apt` packages from the snapshot repository) | § "Toolchain and dependency mirror" |
| the kernel setting | SHIP's WSL step 3, as root (after the packages, before any user step) | § "Kernel setting" |
| toolchain | SHIP's WSL step 5, as `zenith` (the `pwsh` bootstrap, then the `linux` archives of `cmake`, `ninja`, `sccache`, the `llvm` pin and `lavapipe` under `$HOME/.zenith2/toolchain/`, verified on the Windows side by step 4) | § "Toolchain and dependency mirror" |
| dependency sources | SHIP's WSL step 5, as `zenith` (after the toolchain) into `$HOME/.zenith2/thirdparty/` | § "Toolchain and dependency mirror" |
| the toolchain file | `Zenith2/CMake/toolchains/clang-linux-x64.cmake` (SHIP's directory) | § "The linux-tsan toolchain file" |
| the gate invocation | PROC's `Sanitizers.ps1 -Preset linux-tsan` ([Z2-N-ARCH-217]) | none: DEVT never edits it |
| a human or recording run | DEVT's `Invoke-LinuxTsan.ps1` | § "Host runner" |
| proof the host detects races | DEVT's TSan smoke suite | § "TSan smoke suite" |
| proof the host is set up | DEVT's `Test-LinuxTsanHost.ps1`, called by `Setup-Zenith2.ps1 -Check` | § "Host verification" |

## WSL distro provisioning

SHIP's setup provisions the distro (`Zenith2/Docs/Design/SHIP/Manifest.md` → "WSL provisioning"), in this order, which is the order DEVT's host needs (row DV16 records each DEVT-required item):

| SHIP WSL step | What it does | DEVT requirement it meets |
|---|---|---|
| 1 | `wsl --import` of the pinned root filesystem into `%LOCALAPPDATA%\Zenith2\wsl\<distro>` (an existing distro is verified, never re-imported) | the distro |
| 2 | on first import, as root, creates the non-root user `zenith` and writes `/etc/wsl.conf` from DEVT's file (items 1 and 2 below), then `wsl --terminate <distro>`; this runs before every subsequent step because step 5 installs into `zenith`'s `$HOME` | items 1, 2, 3 |
| 3 | as root (`wsl.exe -d <distro> -u root`): the snapshot sources and the `wsl.packages` at their pinned versions, then `/etc/sysctl.d/99-zenith2-tsan.conf` with `vm.mmap_rnd_bits = 28` and `sysctl --system` | § "Toolchain and dependency mirror", § "Kernel setting" |
| 4 | on the Windows side: downloads and verifies every `linux` toolchain archive (`pwsh`, `cmake`, `ninja`, `sccache`, **`llvm`**, `lavapipe`) and every `linux` source archive | § "Toolchain and dependency mirror" |
| 5 | as `zenith`: the `pwsh` bootstrap, then `Install-Zenith2LinuxHost`, which extracts and publishes the verified archives (the pinned clang, clang-tidy, the sanitizer runtimes and `llvm-symbolizer` into `$HOME/.zenith2/toolchain/llvm/<version>/`), the `linux` dependency entries into `$HOME/.zenith2/thirdparty/` and the validation layer | § "Toolchain and dependency mirror" |
| 6 | deletes the verified downloads | none |
| 7 | `-Check` runs `Tools/DevTool/Scripts/Test-LinuxTsanHost.ps1` inside the distro as `zenith` | § "Host verification" |

The items DEVT requires of steps 2, 3 and 5:

1. (DV16) A non-root default user `zenith` created inside the distro (`useradd -m -s /bin/bash zenith`, passwordless `sudo` for `sysctl` alone through `/etc/sudoers.d/zenith2` containing `zenith ALL=(root) NOPASSWD: /usr/sbin/sysctl`), so builds never run as root and `$HOME` is `/home/zenith`.
2. (DV16) `/etc/wsl.conf` written from DEVT's `Zenith2/Tools/DevTool/Wsl/wsl.conf`:

```ini
[boot]
systemd=true

[user]
default=zenith

[automount]
enabled=true
options="metadata,umask=022"

[interop]
enabled=true
appendWindowsPath=false
```

   `systemd=true` makes `systemd-sysctl` apply `/etc/sysctl.d/99-zenith2-tsan.conf` at every distro start; `appendWindowsPath=false` keeps Windows tools (a Windows `cmake` or `clang`) off the Linux `PATH`, so only the pinned Linux toolchain is found; `metadata` lets the build read the checkout on DrvFS with sane permissions.
3. The distro is restarted (`wsl --terminate <distro>`) after writing `wsl.conf` and before anything is installed into `$HOME`, so the user and systemd settings apply to every step that follows.
4. The LLVM pin is installed into the distro by SHIP's WSL step 5 (verified on the Windows side by step 4) from the manifest's `llvm` `linux` archive, never from a distro package; `Test-LinuxTsanHost.ps1` check 4 verifies its version.
5. Re-running the setup is idempotent: an existing distro is verified, never re-imported; a mismatching package version is reinstalled at the pinned version.

## Kernel setting

TSan's shadow memory needs a bounded ASLR range: `vm.mmap_rnd_bits ≤ 28` ([Z2-N-ARCH-219]). WSL2's kernel defaults to 32 on current builds, and with it a TSan binary fails at start with `FATAL: ThreadSanitizer: unexpected memory mapping`. SHIP's WSL step 3, as root and before any user step, writes `vm.mmap_rnd_bits = 28` to `/etc/sysctl.d/99-zenith2-tsan.conf` and applies it (`sysctl --system`), so the setting is in force before any TSan program can run; `systemd-sysctl` reapplies it on each start (`wsl.conf` above); `Invoke-LinuxTsan.ps1` and `Sanitizers.ps1` read `/proc/sys/vm/mmap_rnd_bits` before running and refuse with `linux-tsan: vm.mmap_rnd_bits is <n>; run 'sudo sysctl -p /etc/sysctl.d/99-zenith2-tsan.conf' or Setup-Zenith2.ps1` when it is higher.

## Toolchain and dependency mirror

- Compiler: the `llvm` pin's `linux` archive extracted to `$HOME/.zenith2/toolchain/llvm/<version>/` (SHIP's WSL step 5; `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` names the path), providing `clang`, `clang++`, the sanitizer runtimes, `llvm-symbolizer` (used by TSan and UBSan reports, found through `TSAN_SYMBOLIZER_PATH` that the runner sets) and `clang-tidy`.
- Build tools: `cmake`, `ninja`, `sccache` and `pwsh` from the manifest's `linux` archives in `$HOME/.zenith2/toolchain/`; the sccache directory `$HOME/.zenith2/sccache` ([Z2-N-ARCH-198]).
- Dependencies: verified sources in `$HOME/.zenith2/thirdparty/<name>/<sha256>/` ([Z2-N-ARCH-204]); TSan needs every C++ dependency built from source with the same flags (Z2-D-011), which the `cmake-subproject` kind already does; a dependency whose `platforms` lack `linux` and that a CPU library needs makes the configure fail with the entry named (this is why Tracy needs `linux`, row DV2).
- Nothing is downloaded at configure or build time ([Z2-N-ARCH-196]).

## The linux-tsan toolchain file

DEVT relies on `clang-linux-x64.cmake` doing exactly what `Zenith2/Docs/Architecture/BuildSystem.md` states and SHIP's CMakeHelpers realise: the compiler path from the `llvm` pin, the version check against the pin ([Z2-N-ARCH-194]), the sccache launcher ([Z2-N-ARCH-197]). DEVT adds no toolchain file and no flag; the `Sanitize` configuration's flags (`-fsanitize=thread -fsanitize=undefined -fno-sanitize-recover=undefined`) are ARCH's. The runner's `-Check` mode (§ "Host verification") configures a scratch project with the toolchain file and fails when the configure's reported compiler differs from the pin.

## Host runner

`Zenith2/Tools/DevTool/Scripts/Invoke-LinuxTsan.ps1` runs on Windows and drives the distro; it is for humans and for `needs-human` recording tickets (golden recordings that COMPRESS, GEOM, SCENE and SCRIPT generate on the host), never for the gate (which is PROC's `Sanitizers.ps1`):

```
pwsh -NoProfile -File Zenith2/Tools/DevTool/Scripts/Invoke-LinuxTsan.ps1
	[-Configure] [-Build] [-Test] [-Target <cmake target>] [-TestRegex <ctest -R>] [-Label <ctest -L>]
	[-Exec "<command>"] [-CopyOut <linux path> -To <windows dir>] [-Check]
```

1. Resolve the distro from the manifest's `wsl.distro`; resolve the checkout hash with SHIP's `Get-Zenith2CheckoutHash`; translate the checkout path to `/mnt/<drive>/…` (lower-case drive, `/` separators).
2. Check the kernel setting (§ "Kernel setting").
3. Run each requested stage as one `wsl.exe -d <distro> -e bash -lc "<command>"` with `ZENITH_CHECKOUT_HASH=<hash>`, `TSAN_OPTIONS` and `UBSAN_OPTIONS` exactly as [Z2-N-ARCH-220] fixes them, `TSAN_SYMBOLIZER_PATH`, and `PATH` prefixed with `$HOME/.zenith2/toolchain/*/bin`:
   - `-Configure`: `cmake --preset linux-tsan -S <src> -B $HOME/.zenith2/out/<hash>/linux-tsan` ([Z2-N-ARCH-218]);
   - `-Build`: `cmake --build --preset linux-tsan --config Sanitize [--target <Target>]`;
   - `-Test`: `ctest --preset linux-tsan -C Sanitize --output-on-failure [-R <regex>] [-L <label>] --output-junit <bin>/junit.xml`;
   - `-Exec`: an arbitrary command inside the binary directory (for a recording executable with its arguments);
   - `-CopyOut`: `cp -r <linux path> /mnt/<drive>/<windows dir>` after the other stages.
4. Exit codes: 0 success; 1 a stage failed (its exit code printed); 2 usage; 3 the host is not set up (`Test-LinuxTsanHost.ps1` failed, its report printed).
5. Output: every stage's log is tee'd into `Zenith2/out/<hash>/linux-tsan-runner/<utc>/<stage>.log` on the Windows side.

The module `Zenith2LinuxTsan.psm1` holds the path translation, the command builder and the option strings, so the assert script `Tests/Test-Zenith2LinuxTsan.ps1` tests them without a distro (path translation of `C:\dev\Zenith` to `/mnt/c/dev/Zenith`, quoting of an `-Exec` command with spaces and quotes, the exact `TSAN_OPTIONS` text).

## TSan smoke suite

`Zenith2/Tests/TsanSmoke/` (`zenith_add_integration_tests(TsanSmoke DEPENDS ZenithCore SUPPORTED ZENITH_HOST)`) proves the host's sanitizers actually detect what they must, so a silently broken host cannot pass every TSan run:

| Test | Suite | Case | Pass condition |
|---|---|---|---|
| `Test_TsanSmoke_DataRaceDetected` | `z2-sanitizer-defect` | two `Zenith_Thread`s increment a plain `uint64` 10,000 times each without synchronisation | the output matches `WARNING: ThreadSanitizer: data race` and the process exits non-zero (row DV15 adds the TSan pattern to the suite's pass condition) |
| `Test_TsanSmoke_AtomicNoRace` | ordinary | the same with `std::atomic<uint64>` and relaxed increments | passes, no report |
| `Test_TsanSmoke_MutexNoRace` | ordinary | the same under a `Zenith_Mutex` | passes, no report |
| `Test_TsanSmoke_UbsanSignedOverflowDetected` | `z2-sanitizer-defect` | `INT64_MAX + 1` through a volatile | the output matches `runtime error: signed integer overflow` (row DV15) |
| `Test_TsanSmoke_LockOrderInversionDetected` | `z2-sanitizer-defect` | two threads take two raw `std::mutex`es in opposite orders (test code may use the STL) | the output matches `WARNING: ThreadSanitizer: lock-order-inversion` |

The defect tests are registered only in the `linux-tsan` preset (the registry's sanitizer-defect entries carry the preset's sanitizer); the suite's two ordinary tests run on every host preset.

## Host verification

`Zenith2/Tools/DevTool/Scripts/Test-LinuxTsanHost.ps1 [-Native]` (called by `Setup-Zenith2.ps1 -Check` and `Invoke-LinuxTsan.ps1 -Check`; row DV16 asks SHIP's `-Check` to call it, with `-Native` on a Linux host) checks, printing one line per check and exiting 0 only when all pass. It has two modes, chosen by `-Native` (SHIP's `-Check` passes it when `$IsLinux`, the native CI runner of `Zenith2/Docs/Design/SHIP/Manifest.md` → "Linux host mode"; without it on a Linux host the script refuses with exit 2 rather than invoking `wsl.exe`):

| Mode | Runs each check | Checks run |
|---|---|---|
| WSL (Windows host, default) | through `wsl.exe -d <distro> -e bash -lc "<check>"` | 1 to 8 |
| native (`-Native`, Linux host) | directly in the current shell | 3 to 8 (no distro, no `wsl.conf`; check 2 becomes "the current user is not root") |

1. `wsl.exe -l -q` lists the pinned distro and it starts (`wsl.exe -d <distro> -e true`).
2. The default user is `zenith` (`whoami`) and `/etc/wsl.conf` equals DEVT's file byte for byte.
3. `/proc/sys/vm/mmap_rnd_bits` ≤ 28.
4. `$HOME/.zenith2/toolchain/llvm/<version>/bin/clang --version` reports the pinned version; `llvm-symbolizer` exists.
5. `cmake`, `ninja`, `sccache`, `pwsh` report their pinned versions.
6. `which cmake` resolves inside `$HOME/.zenith2/toolchain/` (no Windows `PATH` leakage).
7. Every pinned `apt` package is installed at its pinned version (`dpkg-query -W -f='${Version}' <package>`).
8. A scratch configure with `clang-linux-x64.cmake` succeeds and a 3-line TSan program (a race) compiled with the pinned clang reports `ThreadSanitizer: data race` when run.

`Pwsh_LinuxTsan_HostCheckModes` (in `Tests/Test-Zenith2LinuxTsan.ps1`) asserts the command each check builds in each mode, that `-Native` never builds a `wsl.exe` command, and that a Linux host without `-Native` exits 2.

**The host bring-up is its own `needs-human` Stage 1 TASK**, "Bring up the linux-tsan WSL host" (`Zenith2/Docs/Design/DEVT.md` → "Stage 1"), not a step of the Stage 1 acceptance, because the golden-recording tickets that need the host (Z2T-COMPRESS-016, Z2T-GEOM-015 in the Stage 1 sprint, Z2T-SCENE-052 and Z2T-SCENE-082 in their own sprints, Z2T-SCRIPT-058) must be able to block on it, and the acceptance ticket runs last. The TASK is blocked by DEVT's four ordinary Stage 1 host tickets and by SHIP's setup-script ticket; a person runs `Setup-Zenith2.ps1` on the reference desktop (with WSL), then `Test-LinuxTsanHost.ps1`, then `Invoke-LinuxTsan.ps1 -Configure -Build -Test -Label tsan-smoke`, and commits the TSan smoke JUnit report and the host check output under `Zenith2/Docs/Research/LinuxTsanHost/<utc>/` (`junit.xml`, `host-check.txt`) in the TASK's diff. Every recording ticket above blocks on the TASK; the Stage 1 acceptance re-runs `Test-LinuxTsanHost.ps1` as one of its steps.

## Gates in CI

DEVT owns two requirements whose realisation is PROC's protected gate steps; the Stage 0 PROC tickets that land them list these requirement IDs (row DV17):

| Requirement | Gate step | What must hold |
|---|---|---|
| Z2-R-DEVT-103 | `Libgraph` | a hard CI gate on every diff touching `Zenith2/**`: layering against declared dependencies, the public/Internal boundary, used-but-undeclared edges, third-party confinement, the Shipping symbol check including DEVT's forbidden symbols (Console.md "Shipping absence of development services"); its report is written under `Zenith2/out/<checkout-hash>/libgraph/` (Z2-D-007) |
| Z2-R-DEVT-104 | `Complexity` | a hard CI gate: `analyze_code_complexity.py --profile zenith2` with every threshold of `complexity_profile.json`, including `max-file-loc` (Z2-D-007, [Z2-N-CONTRACT-418]); an exception is a profile entry with a decision ID |

DEVT adds no threshold and edits neither script.

## Fuzzing

Conventions every area's fuzz target follows (the targets themselves are the areas'; ARCH's registry creates them in `win-asan`):

- Source: `Zenith2/Engine/<Lib>/Tests/Fuzz/<Stem>.cpp` defining `extern "C" int LLVMFuzzerTestOneInput(const uint8_t* pData, size_t ulSize)`, which links `ZenithTestSupport` (the registry links it, `Zenith2/Docs/Design/SHIP/CMakeHelpers.md` names the rule), takes its heap from it, parses the input through the public parser under test, and returns 0; it never asserts on malformed input (a recoverable error is the expected outcome) and asserts only invariants (a parsed value re-serialises and re-parses to itself).
- Seed corpus: `Zenith2/Engine/<Lib>/Tests/Fuzz/Corpus/<Stem>/`, one input per file, named for the case, each at most 64 KiB, at most 256 files per target; PLAT's `Corpus/VirtualPath/` is the example (`Zenith2/Docs/Design/PLAT/Filesystem.md` → "Path-traversal fuzz corpus"). A corpus of binary formats is generated at test time by the area's `z2-fixture-setup` entry when committing binary seeds is impractical ([Z2-N-ARCH-257]).
- Dictionary: `Zenith2/Engine/<Lib>/Tests/Fuzz/<Stem>.dict` (libFuzzer format, tokens such as magic numbers and JSON keys), optional.
- Regressions: every input that once crashed a target is committed to `Zenith2/Engine/<Lib>/Tests/Fuzz/Regressions/<Stem>/<crash-sha1>` in the fix's diff, and the target's ordinary unit test file has `Test_<Stem>_Regressions` that replays every file of the directory through the same entry function in the `win-debug` and `linux-tsan` presets (so a regression is caught without libFuzzer).
- Smoke: the registry's CTest entry runs `Zenith_Fuzz_<Stem> -max_total_time=60 -rss_limit_mb=2048 -timeout=10 <corpus>` in `win-asan` (Z2-D-028); a crash fails the `Sanitizers -Preset win-asan` step and leaves the crashing input as `crash-<sha1>` in the binary directory.
- DEVT's own targets: `ProfilerCapture` (the `.zprof` reader), `DevConsoleLine` (the console tokeniser and parser), `BudgetRunRows` (the `rows.json` and `fingerprint.json` readers), `RemoteFrame` (the remote protocol decoder, in ZenithRuntime's tests), `CrashReport` (the finalization parser, in ZenithPlatform's tests).

## Fuzz campaign runner

`Zenith2/Benchmarks/Fuzz/Run-FuzzCampaign.ps1` runs the long campaigns of stage acceptance (Stage 6's security evidence "every fuzz target's full run without a finding", `Zenith2/Docs/Design/SHIP/Qualification.md` → "Security and compatibility items"):

```
pwsh -NoProfile -File Zenith2/Benchmarks/Fuzz/Run-FuzzCampaign.ps1 [-Targets <glob>] [-SecondsPerTarget 3600] [-Jobs <n>] [-Out <dir>]
```

1. Builds the `win-asan` preset's fuzz targets (`cmake --build --preset win-asan --target <every Zenith_Fuzz_*>`).
2. For each target, copies the seed corpus into a working corpus `<Out>/<Stem>/corpus/` (kept between campaigns on the same machine, so coverage accumulates), and runs `Zenith_Fuzz_<Stem> -max_total_time=<s> -jobs=<n> -workers=<n> -rss_limit_mb=2048 -timeout=10 -dict=<dict> -artifact_prefix=<Out>/<Stem>/ <working corpus> <seed corpus>`.
3. Collects `crash-*`, `leak-*`, `timeout-*` and `oom-*` artefacts; for each, re-runs the target on it once to confirm, and writes `<Out>/campaign.json` (`{ "schema": "zenith2.fuzz-campaign", "schemaVersion": 1, "targets": [ { "name", "seconds", "execs", "coverage", "findings": [ { "kind", "file", "sha1", "reproduced" } ] } ] }`) and a Markdown summary.
4. Exit code 0 when no target has a reproduced finding, 1 otherwise. The acceptance run passes the per-target duration SEC's design states (3,600 s when invoked without `-SecondsPerTarget`), run by a `needs-human` acceptance step; the campaign is evidence, not a gate.

The runner's logic lives in `Zenith2/Benchmarks/Fuzz/Zenith2FuzzCampaign.psm1` (argument validation, the libFuzzer command line, artefact collection, `campaign.json` and the summary), so the assert script `Zenith2/Benchmarks/Fuzz/Tests/Test-FuzzCampaign.ps1`, run by the `Coverage` step's script run (row DV17 (b)), tests it in Stage 2 without a fuzz target: `Pwsh_FuzzCampaign_CommandLine` (the exact argument list for a target with and without a dictionary), `Pwsh_FuzzCampaign_CollectsArtefacts` (a fixture output directory holding `crash-`, `leak-`, `timeout-` and `oom-` files and a stand-in target script that reproduces two of them yields a `campaign.json` listing four findings with the right `reproduced` flags) and `Pwsh_FuzzCampaign_ExitCodeOnReproducedFinding` (exit 1 with a reproduced finding, 0 with only unreproduced ones).

## Static analysis

`Zenith2/.clang-tidy` (run in each stage acceptance with the pinned `clang-tidy`, findings block the acceptance, [Z2-N-CONTRACT-393]):

```yaml
Checks: >
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  -bugprone-exception-escape,
  clang-analyzer-*,
  -clang-analyzer-optin.performance.Padding,
  concurrency-mt-unsafe,
  concurrency-thread-canceltype-asynchronous,
  performance-*,
  -performance-enum-size,
  modernize-use-nullptr,
  modernize-use-override,
  modernize-use-equals-delete,
  modernize-use-equals-default,
  readability-misleading-indentation,
  readability-redundant-declaration,
  readability-non-const-parameter,
  misc-redundant-expression,
  misc-unused-using-decls,
  cppcoreguidelines-slicing,
  cppcoreguidelines-virtual-class-destructor
WarningsAsErrors: '*'
HeaderFilterRegex: 'Zenith2/(Engine|Tools|Player|Benchmarks)/.*'
FormatStyle: none
CheckOptions:
  - key: bugprone-assert-side-effect.AssertMacros
    value: 'ZENITH_ASSERT,ZENITH_CHECK'
  - key: bugprone-assert-side-effect.CheckFunctionCalls
    value: 'true'
  - key: performance-unnecessary-value-param.AllowedTypes
    value: 'Zenith_StringView;Zenith_Span;Zenith_ByteSpan'
```

- `bugprone-assert-side-effect` with `ZENITH_ASSERT` and `ZENITH_CHECK` is [Z2-N-CONTRACT-469]'s rule; `bugprone-exception-escape` is disabled because only the Luau firewall has exceptions, and its compile groups use `/EHs` (Z2-D-017; `Zenith2/Docs/Architecture.md` flags table), never `/EHsc`, so a C function boundary there is not assumed nothrow; `easily-swappable-parameters` is disabled because Zenith Hungarian prefixes distinguish parameters.
- The run uses the compile database of the `win` preset (`compile_commands.json`, ARCH's) and of `linux-tsan`; third-party and generated sources are excluded by the header filter and by the file list (only files under `Zenith2/Engine`, `Tools`, `Player`, `Benchmarks`).
- A suppression is a `// NOLINTNEXTLINE(<check>): <reason with a decision ID>` comment; review rejects one without a decision ID.

## Content validation reports

ASSET's cook writes its validation report into `.cook/` (ASSET's file and schema); `ZenithDevTool content report --report <file> [--format markdown|junit] [--out <file>]` renders it:

- Markdown: a summary table (documents checked, errors, warnings per rule), then one section per rule with each finding's document path, line, message and the authoring fix ASSET's message states; findings sorted by severity, then path.
- JUnit: one `<testsuite>` per rule, one `<testcase>` per document checked against it, a `<failure>` per error, so the CI system shows content errors like test failures.
- The command exits 0 when the report has no error, 1 otherwise, so a stage acceptance can block on content errors.

## Asset profiling

- Live: ASSET's zones carry the asset GUID halves and the section index as zone values (`Zenith_ProfileZoneValue`, Profiler.md "Zenith_ProfileZone header"); the `assets` panel lists the frame's slowest loads by GUID with their decode and upload times, and a capture keeps every value (Profiler.md "Capture format", chunk `ZONE_VALUES`).
- Offline: `ZenithDevTool capture assets --capture <dir> [--cook-report <file>] [--top 50]` reads a capture's segments, groups ASSET's zones (`bg/Asset.Decode` with its GUID and section values, `Asset.KindLoader.<Kind>.Begin` and `.Sections`, `Zenith2/Docs/Design/ASSET.md` → "Profiler zones") by GUID, and prints per asset the count, total and maximum decode and kind-loader time, and, with the cook report, the asset's source path and cooked size; the output is a table on stdout and a CSV with `--csv <file>`.
- The cook side (cook time per importer and per asset) is read from ASSET's cook report by the dashboards (Evidence.md "Regression dashboards").

## ZenithDevTool

`Zenith2/Tools/DevTool/` (`zenith_add_tool(ZenithDevTool DEPENDS ZenithCore ZenithProfiling ZenithPlatform)`, `SUPPORTED ZENITH_TOOLS_HOST`; tool directory row DV1):

| Subcommand | Purpose | Specified in |
|---|---|---|
| `capture summary --capture <dir>` | frame count, lanes, top sites by total time (per site over the capture, by the rule of Profiler.md "Zone statistics": every zone of the site counted, a zone nested inside a zone of the same site on the same lane adding no time), dropped events, markers, and the top lock sites by total wait (per lock site over the capture's `LOCKS` chunks: acquisitions, contended acquisitions, total and maximum wait, total and maximum hold) | this file |
| `capture convert --capture <dir> --out <file.json>` | Chrome trace-event JSON | § "Capture conversion" |
| `capture merge --capture <dir> --out <file.zprof>` | concatenates segments into one file | Profiler.md "Capture format" |
| `capture assets` | § "Asset profiling" | this file |
| `crash symbolize`, `crash list` | Crash.md "Symbolization" | |
| `remote …` | Console.md "Remote client" | |
| `log view --log <dir> [--level warning] [--category <glob>] [--grep <text>] [--follow]` | prints JSON Lines log chunks as aligned text in sequence order across chunk files; `--follow` tails the newest chunk | Console.md "Log record schema" |
| `dashboard build` | Evidence.md "Regression dashboards" | |
| `content report` | § "Content validation reports" | |

- Every subcommand returns 0 on success, 1 on a failure of its work, 2 on usage; every reader refuses a newer schema or format version than it knows, naming both versions.
- The tool is the one ZenithProfiling consumer that runs off the engine: it creates a root heap, a log router with a console sink, and no profiler hooks; it links ZenithPlatform for file access. Its client socket is PLAT's `Zenith_DevClient` (`Zenith2/Docs/Design/PLAT/Platform.md` → "Loopback socket"), and its `adb` process launch is the tool's own Windows code under `Tools/DevTool/Internal/Windows/` (PLAT has no process API; `Zenith2/Docs/Design/DEVT/Console.md` → "Remote client"), which a `ZENITH_TOOLS_HOST` (`win`) target may use; its `main` lives in the `Internal/Zenith_<Name>Main.cpp` file of that rule (with `<Name>` the target name `ZenithDevTool`), which makes the directory an executable target (the `zenith_add_tool` row of the helper table in `Zenith2/Docs/Architecture/BuildSystem.md`: an executable when the directory has `Internal/Zenith_<Name>Main.cpp`).

## Capture conversion

`capture convert` writes the Chrome trace-event format (the JSON object form, loadable by Perfetto and `chrome://tracing`):

- One `"M"` (metadata) event per lane: `{"ph":"M","name":"thread_name","pid":1,"tid":<lane>,"args":{"name":"<lane name>"}}`; GPU lanes get `pid` 2 (`process_name` "GPU").
- One `"X"` (complete) event per zone: `{"ph":"X","name":"<site name>","pid":1|2,"tid":<lane>,"ts":<startNs/1000 relative to the capture base, microseconds with 3 decimals>,"dur":<duration µs>,"args":{"value":<first value>,"values":[…],"depth":<depth>}}`.
- One `"C"` (counter) event per changed counter per frame: `{"ph":"C","name":"<counter>","pid":1,"ts":<frame end>,"args":{"value":<frame value>}}`.
- One `"i"` (instant) event per frame boundary (`"name":"frame <index>"`, scope `g`) and per marker; log lines as instant events on a `log` lane when `--with-logs` is given.
- Memory snapshots as counter events per subsystem (`memory.<subsystem>`).
- Zones with `TRUNCATED_START` or `TRUNCATED_END` are written with `"args":{"truncated":true}`.
- The writer streams events (no whole-capture DOM) so a 1 GiB capture converts in bounded memory; `Test_CaptureConvert_TraceEventGolden` compares the conversion of a small fixed capture the test builds with a hand-written text golden.
