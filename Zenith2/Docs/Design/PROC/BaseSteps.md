# PROC design — the shared gate module and the base steps

Companion to `Zenith2/Docs/Design/PROC.md`. This file specifies the shared module every gate step imports, the common step lifecycle, and the steps that are not structural rule engines: Configure, Build, Test, SourceLayout, CompileCoverage, Format, HeaderSelfContained, ApiContractCheck, ReviewSignals, ShaderChecks, EcsConformance, Sweep, Run-Committed and IntegrationSmoke, and the gate self-test suite. The rule engines (ForbiddenCheck, TestSeparationCheck, Libgraph, Complexity) are in `Zenith2/Docs/Design/PROC/StructuralGates.md`; Coverage, the filer, ApkCheck, Sanitizers and the parity steps are in `Zenith2/Docs/Design/PROC/ExtendedGates.md`.

## Scope of this file

Every script named here lives under `Zenith2/Tools/Gates/` and is protected. Every script sets `Set-StrictMode -Version Latest` and `$ErrorActionPreference = 'Stop'`, reads optional JSON members through `PSObject.Properties[...]`, returns collections with `,@(...)` so a one-element list stays a list, never writes `@($list)` over a `List[object]` (it calls `.ToArray()`), and is ASCII-only in its body, the three PowerShell rules this repository's own assert scripts record. Every script runs under `pwsh` 7 (the `pwsh` pin of `Zenith2/ThirdParty/manifest.json`), never under Windows PowerShell 5.1.

## Shared gate module

### Module files

| File | Exports |
|---|---|
| `Zenith2/Tools/Gates/Common/Zenith2Gates.psm1` | every function of the table below |
| `Zenith2/Tools/Gates/Common/Zenith2GateReport.psm1` | `New-Zenith2GateReport`, `Write-Zenith2GateReport`, `Read-Zenith2GateReport` (the report format of `Zenith2/Docs/Design/PROC.md` → "The gate step report") |

`Zenith2Gates.psm1` imports SHIP's `Zenith2/Tools/Setup/Zenith2Env.psm1` for `Get-Zenith2CheckoutHash` and the environment setup ([Z2-N-ARCH-226]); it never re-implements the checkout hash.

### Functions

| Function | Behaviour |
|---|---|
| `Get-Zenith2RepoRoot` | `Resolve-Path "$PSScriptRoot/../../../.."` of the module file (the directory that holds `Zenith2/`), returned with `/` separators and no trailing slash; never derived from the current directory |
| `Get-Zenith2OutRoot [-Linux]` | Windows: `<repo>/Zenith2/out/<checkout-hash>`; with `-Linux`: the string `$HOME/.zenith2/out/<checkout-hash>` as the WSL side sees it ([Z2-N-ARCH-160]) |
| `Enter-Zenith2GateStep -Name -Preset -Parameters` | the lifecycle entry of "Step lifecycle": resolves the roots, imports the pinned environment by dot-sourcing `Zenith2/Tools/Setup/Enter-Zenith2Env.ps1` when `$env:ZENITH2_ENV_ENTERED` is not `1`, verifies the tools the step declares (each step passes its own `-RequiredTools` list, for example `cmake`, `ninja`, `clang-format`), creates `${ZENITH_OUT}/gates/` and `${ZENITH_OUT}/gates/logs/`, resolves the timeout, starts the watchdog, and returns the context object |
| `Exit-Zenith2GateStep -Context -ExitCode -Verdict` | stops the watchdog, sweeps (`Invoke-Zenith2ExeSweep`), writes the report, prints the verdict line, and calls `exit` with the code |
| `Add-Zenith2Finding -Context -File -Line -Rule -Message [-Severity]` | appends one finding (default severity `fail`) and prints it in the `<Step>: <file>:<line>: <rule>: <message>` form |
| `Get-Zenith2StepTimeoutMinutes -Step [-Preset]` | the algorithm of "Timeout resolution" below |
| `Read-Zenith2BudgetRow -Id` | parses `Zenith2/Docs/Budgets.md` → "Row index": finds the one table line whose first cell is `Id`, splits it on `|`, and returns `{ Id, Name, Status, Area, Value, Unit, Producer }`; a missing row or a duplicate is exit 3 |
| `Get-Zenith2ScanFiles [-Extensions] [-Under]` | `git -C <repo> ls-files -c -o --exclude-standard -z -- Zenith2` split on NUL (tracked plus untracked, never ignored), minus files absent on disk (deleted but still indexed), minus `Zenith2/out/**`, `Zenith2/ThirdParty/**` and `Zenith2/Tools/Gates/Tests/**`, filtered by extension and by `-Under` prefixes, byte-ordered |
| `Get-Zenith2FileApiReply -ConfigurePreset` | reads `<binaryDir>/.cmake/api/v1/reply/index-*.json` (the newest by name), resolves the `codemodel-v2`, `cache-v2`, `cmakeFiles-v1` and `toolchains-v1` objects the query of "Configure" requested, and returns them; an absent reply is exit 3 with `run the Configure step first` |
| `Get-Zenith2DeclaredDeps` | parses `${ZENITH_OUT}/declared_deps.json` and checks `schema` is `zenith2.declared-deps` with `schemaVersion` 1 (`Zenith2/Docs/Architecture/BuildSystem.md` → "The declared_deps.json record") |
| `Get-Zenith2ActiveGateLines [-ExcludeCategory]` | parses the repository's `zagent.project.json`, takes every category whose `project` is `Z2`, and returns `{ category, line, measure, host }` for each element of its `gates`, in category declaration order then array order; `measure` is true when the line is an element of the category's `measureSteps`; `host` is `linux` for a line containing `-Preset linux-tsan` and `windows` otherwise |
| `Get-Zenith2ChangedPaths` | when `$env:ZAGENT_GATES_JSON` names an existing file, returns its `changed` array (repo-relative, `/` separators); otherwise `$null`, which every consumer reads as "no ticket context: run everything" |
| `Get-Zenith2TestEnvironment` | returns `@{ VK_DRIVER_FILES = <the pinned lavapipe ICD manifest path from the manifest's lavapipe pin>; VK_LAYER_PATH = <the pinned validation layer directory> }` and, on Windows, prepends the staged WARP and `d3d12SDKLayers.dll` directory to `PATH` ([Z2-N-CONTRACT-434]) |
| `Invoke-Zenith2Process -FilePath -ArgumentList -TimeoutMinutes [-WorkingDirectory] [-Environment]` | starts the child with redirected output appended to the step log, waits up to the timeout (or the step's remaining time, whichever is smaller), kills the process tree on expiry (`taskkill /T /F /PID` on Windows; `kill -TERM -<pgid>`, then after 10 s `kill -KILL -<pgid>` on Linux), and returns `{ ExitCode; Output; Seconds; TimedOut }`, with `ExitCode` 124 when it timed out |
| `Invoke-Zenith2ExeSweep` | enumerates processes (`Get-CimInstance Win32_Process` on Windows, `/proc/*/exe` on Linux) whose executable path is under `${ZENITH_OUT}` (case-insensitive prefix compare after normalising separators), kills each with its tree, and returns the killed paths; it never kills by image name, so a Zenith exe or another checkout's process is never touched |
| `Invoke-Zenith2Configure -ConfigurePreset <c>` | writes the File API query of "Configure" item 4 and runs `cmake --preset <c>` with `ZENITH_CHECKOUT_HASH` set; a no-op when `<binaryDir>/CMakeCache.txt` exists and the cold record of `Invoke-Zenith2Build` is current; returns `{ ExitCode; Ran; Seconds; Tail }` (`Tail` is the last 40 output lines) |
| `Invoke-Zenith2Build -Preset <p> [-Target <t>]` | the one build path of every step: checks `p` against the preset table of "Build", runs the cold detection of "Build" item 3 and, when cold, appends `cold <p> <reason>` to the file `$env:ZAGENT_COLD_SIGNAL` names (when set), calls `Invoke-Zenith2Configure` for the preset's configure tree, builds `win-tools` first unless `p` is `win-tools` or an Android preset, prints `sccache --show-stats` before and after, runs `cmake --build --preset <p> --parallel <logical processors>` (with `--target <t>` when given), rewrites the cold record on success, and returns `{ ExitCode; Cold; Reasons; Seconds; FirstError }`. Every step that builds (`Build`, `Test`, `Libgraph`'s Shipping build, `Sanitizers`, `ApkCheck`, `BackendParity`, `PathParity`, `IntegrationSmoke`, `EcsConformance`, `ShaderChecks`) calls it, so every building step signals a cold tree, not only `Build.ps1` |
| `Get-Zenith2AcceptanceStage` | the integer of `$env:ZENITH_ACCEPTANCE_STAGE` when `$env:ZENITH2_ACCEPTANCE` is `1` (both set only by `Run-Committed.ps1 -Acceptance`), otherwise `$null` |
| `New-Zenith2HscOverlay -Library <Lib>` | builds the include overlay of "HeaderSelfContained" for one library and returns its root; an addition to the contract's function table (reconciliation R17) |
| `Get-Zenith2MsvcEnvironment`, `New-Zenith2HeaderTU`, `Get-Zenith2CompileFlags` | as `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" (table "Shared gate environment") specifies; `Get-Zenith2CompileFlags` also returns the compile group's `compileCommandFragments` beside `includes[]` and `defines[]` (reconciliation R17) |

### Timeout resolution

`Get-Zenith2StepTimeoutMinutes -Step S [-Preset P]`:

1. Find the entry of `Zenith2/Tools/Gates/step_timeouts.json` whose `step` equals `S` and whose `preset` equals `P` (or is `null` when `P` is absent); no entry is exit 3 naming the step.
2. `Read-Zenith2BudgetRow -Id <entry.row>`.
3. Status `adopted`: return the row's `Value` as an integer number of minutes (a non-integer or a unit other than `min` is exit 3), with source `adopted <row>`.
4. Status `pending`: return `entry.provisionalMinutes` with source `provisional <row> (row pending)`.

`step_timeouts.json`:

```json
{
    "schema": "zenith2.step-timeouts",
    "schemaVersion": 1,
    "comment": "Step timeouts read from Zenith2/Docs/Budgets.md Row index; provisionalMinutes applies only while the row is pending (Zenith2/Docs/Design/PROC/BaseSteps.md, Timeout resolution).",
    "steps": [
        { "step": "Configure", "preset": null, "row": "Z2-B-PROC-007", "provisionalMinutes": 20 },
        { "step": "Build", "preset": "win-debug", "row": "Z2-B-PROC-008", "provisionalMinutes": 90 },
        { "step": "Build", "preset": "win-release", "row": "Z2-B-PROC-009", "provisionalMinutes": 90 },
        { "step": "Build", "preset": "win-shipping-test", "row": "Z2-B-PROC-009", "provisionalMinutes": 90 },
        { "step": "Build", "preset": "win-shipping", "row": "NEW-ROW-BUILD-WIN-SHIPPING", "provisionalMinutes": 90 },
        { "step": "Build", "preset": "android-arm64", "row": "Z2-B-PROC-010", "provisionalMinutes": 90 },
        { "step": "Build", "preset": "win-asan", "row": "Z2-B-PROC-025", "provisionalMinutes": 120 },
        { "step": "Build", "preset": "android-arm64-hwasan", "row": "Z2-B-PROC-023", "provisionalMinutes": 90 },
        { "step": "Test", "preset": null, "row": "Z2-B-PROC-011", "provisionalMinutes": 60 },
        { "step": "SourceLayout", "preset": null, "row": "Z2-B-PROC-012", "provisionalMinutes": 5 },
        { "step": "CompileCoverage", "preset": null, "row": "Z2-B-PROC-013", "provisionalMinutes": 10 },
        { "step": "Libgraph", "preset": null, "row": "Z2-B-PROC-014", "provisionalMinutes": 20 },
        { "step": "Complexity", "preset": null, "row": "Z2-B-PROC-015", "provisionalMinutes": 15 },
        { "step": "ForbiddenCheck", "preset": null, "row": "Z2-B-PROC-016", "provisionalMinutes": 20 },
        { "step": "Format", "preset": null, "row": "Z2-B-PROC-017", "provisionalMinutes": 10 },
        { "step": "HeaderSelfContained", "preset": null, "row": "Z2-B-PROC-018", "provisionalMinutes": 30 },
        { "step": "ApiContractCheck", "preset": null, "row": "Z2-B-PROC-019", "provisionalMinutes": 30 },
        { "step": "TestSeparationCheck", "preset": null, "row": "Z2-B-PROC-020", "provisionalMinutes": 20 },
        { "step": "ReviewSignals", "preset": null, "row": "Z2-B-PROC-021", "provisionalMinutes": 5 },
        { "step": "Coverage", "preset": null, "row": "Z2-B-PROC-022", "provisionalMinutes": 20 },
        { "step": "ApkCheck", "preset": null, "row": "Z2-B-PROC-023", "provisionalMinutes": 60 },
        { "step": "ShaderChecks", "preset": null, "row": "Z2-B-PROC-024", "provisionalMinutes": 20 },
        { "step": "Sanitizers", "preset": "win-asan", "row": "Z2-B-PROC-025", "provisionalMinutes": 120 },
        { "step": "Sanitizers", "preset": "linux-tsan", "row": "Z2-B-PROC-026", "provisionalMinutes": 120 },
        { "step": "BackendParity", "preset": null, "row": "Z2-B-PROC-027", "provisionalMinutes": 60 },
        { "step": "IntegrationSmoke", "preset": null, "row": "Z2-B-PROC-028", "provisionalMinutes": 60 },
        { "step": "PathParity", "preset": null, "row": "Z2-B-PROC-029", "provisionalMinutes": 90 },
        { "step": "EcsConformance", "preset": null, "row": "Z2-B-PROC-031", "provisionalMinutes": 30 },
        { "step": "SecurityConformance", "preset": null, "row": "NEW-ROW-SECURITYCONFORMANCE", "provisionalMinutes": 30 }
    ],
    "warmPresets": [ "win-debug", "win-release", "win-shipping-test", "win-shipping" ],
    "acceptance": { "row": "Z2-B-PROC-006", "provisionalMinutes": 240 }
}
```

- The `Build -Preset win-asan`, `Build -Preset android-arm64-hwasan` and `Build -Preset win-shipping` entries are the builds that `Sanitizers`, `ApkCheck` and `Libgraph` (rule LG9) run inside their own steps through `Invoke-Zenith2Build`. The first two share the owning step's row. The Shipping build gets a row of its own, because it runs on nearly every engine diff and would otherwise be charged to `Libgraph`'s 20-minute timeout. The row "Step timeout Build win-shipping" (PROC, min, pending) is new (reconciliation R14). The `SecurityConformance` row is the third row of X415. Both are requested from the core-doc batch, which applies X440 (b) and X415 before A8a. The two `NEW-ROW-*` placeholders are replaced by the IDs that batch assigns before Z2T-PROC-008 lands the file. Z2T-PROC-008's first DoD item checks both rows exist, and `Tests/Test-StepTimeouts.ps1` resolves every `row` of the file against `Zenith2/Docs/Budgets.md` → "Row index" and fails on a missing row or a `NEW-ROW-` prefix, so the failure is caught at landing, never at a step's first timeout lookup.
- `warmPresets` is the list `Provision-Lane.ps1 -WarmOnly` warms (`Zenith2/Docs/Design/PROC/Harness.md` → "The Z2 step"); every PROC ticket that activates a building step appends its presets here.
- `Sweep` and `Run-Committed` have no `steps` entry: `Sweep` is bounded by its process enumeration, and `Run-Committed` by the lines it runs (each with its own timeout) and, with `-Acceptance`, by the acceptance watchdog of "-Acceptance -Stage N".
- **Which limit binds.** A step's effective limit is the smaller of its step timeout and the remaining phase cap. Inside a warm tick phase the 30-minute caps of Z2-B-PROC-002 and -003 bind, so the larger step timeouts (the builds' 90, `Sanitizers` 120, `ApkCheck` 60) apply only in a cold phase (the cold cap of Z2-B-PROC-005), in CI and in the acceptance run. The warm-up of `warmPresets` is what keeps a lane's first build of each of those trees out of a warm verify phase.
- **Re-allocation.** A step-timeout row and the acceptance row are re-allocated at every stage acceptance, raise-only (`Zenith2/Docs/Design/PROC.md` → "Budget rows").

## Step lifecycle

### States and events

| State | Event | Next | Action |
|---|---|---|---|
| Starting | the script begins | Entering | parse parameters; an unknown or missing parameter prints usage and exits 2 before anything else |
| Entering | `Enter-Zenith2GateStep` | Ready | roots, environment, tool checks, timeout, watchdog |
| Entering | a required tool or the environment is missing | Exiting(3) | finding `GATE-ENVIRONMENT` naming the tool and the setup command |
| Ready | the step's checks run | Running | children through `Invoke-Zenith2Process` alone |
| Running | the watchdog's deadline passes | Exiting(124) | the running child's tree is killed; finding `GATE-TIMEOUT` naming the timeout and its source |
| Running | an exception escapes | Exiting(1) | finding `GATE-INTERNAL` with the exception message and the script position |
| Running | the checks finish | Exiting(0 or 1) | exit 1 exactly when a `fail` finding exists or a child the step requires to succeed did not |
| Exiting(code) | `Exit-Zenith2GateStep` | Exited | sweep, report, verdict line, `exit code` |

The watchdog is a `System.Threading.Timer` created by `Enter-Zenith2GateStep` that, on expiry, kills the tree of the child `Invoke-Zenith2Process` is waiting on and sets a flag the waiting loop reads; `Invoke-Zenith2Process` waits in 1-second slices so the flag is seen within one second. The step's own timeout is always smaller than the phase cap in a warm phase (the phase cap is the backstop), and `Invoke-Zenith2Process` never waits longer than the remaining step time.

### Rules every step follows

- Self-sufficient and order-independent ([Z2-N-CONTRACT-573]): a step that needs a configure tree configures it (by the shared module's `Invoke-Zenith2Configure -ConfigurePreset <c>`, in-process), and a step that needs a build builds the preset (by `Invoke-Zenith2Build -Preset <p>`), each a no-op when the tree is current.
- Writes only under `${ZENITH_OUT}`; a step that needs scratch space uses `${ZENITH_OUT}/gates/scratch/<Step>/`, emptied at entry.
- Never runs `git add`, `git commit`, `git checkout` or any command that changes the index, `HEAD` or a tracked file; the only git commands a step runs are `ls-files`, `rev-parse`, `diff --name-only`, `cat-file` and `hash-object` without `-w`.
- Never calls `zagent`: the board is not a gate input, and a board write between measure and guard is forbidden by the tick protocol.
- Reads its inputs from the working tree, so a `-Phase verify` re-run certifies post-measure edits.

## Configure

`Configure.ps1` (no parameters).

1. Enter the step; required tools `cmake`, `ninja`.
2. The configure presets to run are those of `Zenith2/CMakePresets.json` whose `condition` holds on the host ([Z2-N-ARCH-193]): on a Windows host `win`, `win-asan`, `android-arm64`, `android-x86_64`; on a Linux host `linux-tsan`. A preset whose condition is false is reported as `skipped (condition: hostSystemName)`. `linux-tsan` on a Windows host is configured inside WSL by `Sanitizers -Preset linux-tsan`, never here.
3. An Android preset whose NDK pin is not installed (`Setup-Zenith2.ps1 -Check` reports the `ndk` key missing) is skipped with a `report` finding `CONFIGURE-ANDROID-NOT-PROVISIONED` while no Android gate line is active (`Get-Zenith2ActiveGateLines` has no `-Preset android-arm64` line), and is a `fail` once one is.
4. Before each configure, write the File API query `<binaryDir>/.cmake/api/v1/query/client-zenith2/query.json`:

```json
{ "requests": [ { "kind": "codemodel", "version": 2 }, { "kind": "cache", "version": 2 }, { "kind": "cmakeFiles", "version": 1 }, { "kind": "toolchains", "version": 1 } ] }
```

   The query file is rewritten only when its bytes differ, so an unchanged tree is not re-queried (MOD-P4: `Test_GameModel_MinimalCompileGroup_IsGeneratedTU` reads the codemodel reply).
5. Run `cmake --preset <p>` with `ZENITH_CHECKOUT_HASH` set from `Get-Zenith2CheckoutHash` (the scaffold's `zenith_begin_build` refuses a missing hash, [Z2-N-ARCH-158]); a non-zero exit is a `fail` finding `CONFIGURE-FAILED` with the last 40 lines of CMake's output.
6. After all presets, check that `${ZENITH_OUT}/declared_deps.json` exists and parses (`Get-Zenith2DeclaredDeps`), and that every preset wrote identical `declared_deps.json` bytes ([Z2-N-ARCH-183]); the helpers write one shared file, so the step hashes it after each preset and fails `CONFIGURE-DECLARED-DEPS-UNSTABLE` when two presets produced different bytes.

## Build

`Build.ps1 -Preset <p>`.

`Build.ps1` is a thin wrapper: it enters the step, calls `Invoke-Zenith2Build -Preset <p>` (the shared module, "Functions"), turns the result into findings and exits. Items 3 to 7 below are what that function does for every caller, so `Test`, `Libgraph`, `Sanitizers`, `ApkCheck` and the parity steps detect and signal a cold tree exactly as `Build` does.

1. Enter the step with `-Preset`; required tools `cmake`, `ninja`, `sccache`.
2. `p` must be a row of the preset table below; any other value is exit 2.
3. Cold detection ([Z2-N-ARCH-223]): the binary directory is cold when `CMakeCache.txt` is missing, or when the SHA-256 of `Zenith2/CMakePresets.json`, of `Zenith2/ThirdParty/manifest.json`, or of the sorted `path sha256` lines of every file under `Zenith2/CMake/` differs from the values recorded in `<binaryDir>/zenith2-gate-cold.json` by the previous successful build. When cold, the step records `cold: { cold: true, reason }` in its report and, when `$env:ZAGENT_COLD_SIGNAL` is set, writes one line `cold <preset> <reason>` to that file (append mode), which is how `tick_gates.ps1` learns to apply the cold phase cap (`Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode").
4. When `p` is not `win-tools`, build `win-tools` first ([Z2-N-ARCH-190]) with `cmake --build --preset win-tools --parallel <logical processors>`.
5. Print `sccache --show-stats`, run `cmake --build --preset <p> --parallel <logical processors>`, print `sccache --show-stats` again ([Z2-N-ARCH-233]).
6. A non-zero build exit is a `fail` finding `BUILD-FAILED` whose message is the first compiler or linker error line of the log (the first line matching `error [A-Z]+[0-9]+:` or `error:`), and the full log is in `gates/logs/Build.<p>.log`.
7. On success, rewrite `<binaryDir>/zenith2-gate-cold.json` with the three hashes.

| `-Preset` | Configure tree | Build preset | Note |
|---|---|---|---|
| `win-debug` | `win` | `win-debug` | libgraph's symbol stage reads these objects (`/Gy-`, Z2-D-028) |
| `win-release` | `win` | `win-release` | |
| `win-shipping-test` | `win` | `win-shipping-test` | builds `zenith_tests` in `ShippingTest` |
| `win-shipping` | `win` | `win-shipping` | builds `zenith_players` in `Shipping`; run by `Libgraph` for rule LG9 |
| `android-arm64` | `android-arm64` | `android-arm64` | Release; Players, libraries and tests compiled only |
| `win-asan` | `win-asan` | `win-asan` | run by `Sanitizers` |
| `android-arm64-hwasan` | `android-arm64` | `android-arm64-hwasan` | run by `ApkCheck` when its HWASan condition holds |
| `ps5`, `xbox-series`, `switch2` (from Z2T-PROC-092a to -092c) | the overlay's own configure preset, from `Zenith2/Platforms/<Platform>/CMakePresets.json` through the developer-local `Zenith2/CMakeUserPresets.json` | the same name | run only in the overlay's own checkout; when `Zenith2/Platforms/<Platform>/CMakeLists.txt` is absent the step exits 3 with `Build: environment: overlay Zenith2/Platforms/<Platform>/ absent; run the overlay setup` (`Test_Build_ConsolePresetAbsentOverlayExits3`; row SP17) |

## Test

`Test.ps1 [-Label <label>] [-Preset <p>]`.

### Without -Label

1. Enter the step; required tools `cmake`, `ctest`.
2. The test presets are `win-debug`, `win-release` and `win-shipping-test` (SHIP-P1, DEVT-P7, row X216), or the one named by `-Preset`.
   - **The Shipping test leg** runs on every diff that selects the `Zenith2` category, and every Z2 diff does. It is not path-selected, because a Shipping-only branch can sit in any library and no path list could name them all. The tree it needs is built in the measure phase by base row 4 (`Build.ps1 -Preset win-shipping-test`).
   - Its time is part of the `Test` row Z2-B-PROC-011, re-allocated raise-only at each acceptance.
   - It lands with the `Test` step itself in Z2T-PROC-008 (Stage 0), so no further ticket is reserved for it.
   - A ticket whose Definition of Done holds a Shipping-only test names it as "`Test` step (`win-shipping-test` preset)", for example AI's Shipping cases and PLAT's `Test_VirtualPath_UserDevtoolsRefusedInShipping`. The worker's local equivalent is `Test.ps1 -Preset win-shipping-test`.
3. For each, ensure its build is current (`Invoke-Zenith2Build`, a no-op after measure).
4. Run `ctest --preset <p> --output-on-failure --output-junit <ZENITH_OUT>/gates/tests/ctest-<p>.xml --parallel <logical processors> --label-exclude "^(z2-parity-backend|z2-parity-path|z2-acceptance-.*)$"` with the environment of `Get-Zenith2TestEnvironment` merged in. The parity labels belong to `BackendParity` and `PathParity`, and the acceptance suites to `Run-Committed.ps1 -Acceptance` ([Z2-N-ARCH-179]).
5. Every failing test is a `fail` finding `TEST-FAILED` with the test name and the first 20 lines of its output; a test preset that runs zero tests while `zenith_tests` names at least one test target is `TEST-NONE-RAN` (the presets' `noTestsAction: error` makes CTest itself fail; the finding names the cause).
6. Death, sanitizer-defect, fixture-setup and acceptance suites are excluded by the test main and the registry, never by this step's arguments; the step adds no `-E` exclusion of its own beyond the labels in item 4.
7. A test CTest reports as skipped or not run (its `SKIP_RETURN_CODE`, a `DISABLED` property, or a failed `REQUIRED_FILES` condition) is never counted as passed: each is a `report` finding `TEST-NOT-RUN` naming the test and its labels, and the report's `summary` carries `passed`, `failed` and `notRun` counts separately, and `presets: [ { "preset", "seconds", "passed", "failed", "notRun" } ]` with one entry per test preset run, the per-leg figure MAINT's headroom ticket cites. Expected Stage 0 figure: the three legs together are below one third of the warm verify cap of Z2-B-PROC-003 (10 of its 30 minutes), because the other twelve verify lines share it. If a stage's measured `Test` time passes that, the headroom ticket moves `Test` into a measure-phase leg of its own, as reconciliation R4 did for the `win-shipping-test` build. This is how a hardware-dependent test, for example one labelled `requiresHardwareVideo` that skips while `ZENITH2_CINE_VIDEO_CLIPS` is unset (CINE-P3), stays visible as missing evidence instead of reading as a pass.

### With -Label

`Test.ps1 -Label <label>` runs one test preset (`win-debug`, or the one `-Preset` names) with `--label-regex "^<label>$"` in place of the exclusion in item 4, and with `--no-tests=ignore`, so the `Zenith2Memory` hand-over line passes with a report line `Test: no test carries label memory yet` before any library declares `TEST_LABELS memory` ([Z2-N-ARCH-251]). `<label>` must match `^[a-z][a-z0-9-]*$` (exit 2 otherwise).

## SourceLayout

`SourceLayout.ps1`. Input: `Get-Zenith2ScanFiles` over every file (no extension filter), which includes untracked files, so a new file the worker created is seen. A compiled-language source is one with extension `.cpp`, `.c`, `.h`, `.hpp` or `.inl`.

| Rule | Check | Source |
|---|---|---|
| SL1 | a compiled-language source under `Zenith2/Engine/`, `Zenith2/Tools/`, `Zenith2/Player/`, `Zenith2/Benchmarks/` or `Zenith2/Tests/` lies inside a directory that a helper call globs: `Engine/<Lib>/` with a `CMakeLists.txt`; `Tools/<Dir>/` with one; `Player/` with one; `Benchmarks/<Harness>/` or `Benchmarks/Budgets/<AREA>/` with one; `Tests/<Suite>/` with one | `Zenith2/Docs/Architecture/BuildSystem.md` → "Hygiene gates fed by the build" |
| SL2 | no compiled-language source under `Zenith2/Games/` | [Z2-N-ARCH-205] |
| SL3 | a directory named `Windows`, `Android` or `Linux` that holds a compiled-language source sits directly below an `Internal/` directory; the one exception is `Zenith2/Engine/ZenithCore/Platform/<Platform>/`, which holds exactly one file, the per-platform threading header `Zenith_PlatformThreading.h` that ZenithCore's public synchronisation headers alias (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "platform_headers.json") | [Z2-N-ARCH-162] |
| SL4 | inside `Engine/<Lib>/`, a compiled-language source sits at the library root, under `Internal/` or under `Tests/`, or is `Zenith2/Engine/ZenithCore/Platform/<Platform>/Zenith_PlatformThreading.h` (`<Platform>` one of `Windows`, `Android`, `Linux`; the SL3 exception) | [Z2-N-ARCH-161] |
| SL5 | `Engine/<Lib>/Tests/Support/` holds no `.cpp` or `.c` file unless `<Lib>` is `ZenithCore` | [Z2-N-ARCH-255] |
| SL6 | every `CMakeLists.txt` below `Engine/`, `Player/`, `Tools/`, `Benchmarks/`, `Tests/` and `Games/` contains exactly one command, which is one of `zenith_add_library`, `zenith_add_tool`, `zenith_add_benchmark`, `zenith_add_integration_tests`, `zenith_add_game` (comments and whitespace aside; the parser tokenises CMake's command syntax, so a command split over lines counts once) | [Z2-N-ARCH-157] |
| SL7 | no `CMakeLists.txt` directly inside a group directory (`Benchmarks/Budgets/CMakeLists.txt`) | [Z2-N-ARCH-156] |
| SL8 | test code lives under a `Tests/` directory: a file outside `Tests/` and outside `Zenith2/Benchmarks/` that includes `<doctest/doctest.h>` or declares `TEST_CASE`, `SUBCASE` or `TEST_SUITE` | `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates"; condition 3 of TestSeparationCheck reports the same files from the target side |
| SL9 | `Zenith2/Tools/Gates/` contains no `CMakeLists.txt` at any depth and no compiled-language source outside `Zenith2/Tools/Gates/Tests/**/Fixtures/**`, and no CMake file outside it names a path under it, with one exception: `Zenith2/CMake/ZenithSecurity.cmake` executes `Zenith2/Tools/Gates/Security/Test-Zenith2UpdateKeys.ps1` at configure (`zenith_security_resolve_update_key`, with the script in `CMAKE_CONFIGURE_DEPENDS`; ruling SEC-R2 m5). The configure runs that script and never builds, generates into or edits the directory, and the script imports nothing from `Gates/Common/` | this design: the gate directory is never part of the build |
| SL10 | a compile-fail source is named `*_CompileFail.cpp.in` and lies under a library's `Tests/` | [Z2-N-ARCH-249] |

Fixtures (`Zenith2/Tools/Gates/Tests/SourceLayout/Fixtures/`): `SL1-UnglobbedDir` (a `.cpp` in `Engine/ZenithFoo/Extra/` — also an SL4 hit, so `expect.json` lists both), `SL1-NoCMakeLists`, `SL2-GameCpp`, `SL3-PlatformOutsideInternal`, `SL5-SupportCpp`, `SL6-TwoCommands`, `SL7-GroupCMakeLists`, `SL8-TestCaseOutsideTests`, `SL9-GateCMakeLists`, `SL9-CMakeNamesGatePath` (a CMake file other than `ZenithSecurity.cmake` naming a `Tools/Gates/` path), `Pass-SecurityConfigureEdge` (`ZenithSecurity.cmake` executing `Security/Test-Zenith2UpdateKeys.ps1`), `SL10-CompileFailMisnamed`, `SL4-PlatformDirOtherFile` (a second header beside `Zenith_PlatformThreading.h` in `ZenithCore/Platform/Windows/`: SL3 and SL4), `Pass-PlatformThreadingHeader` (the three per-platform threading headers in place), `Pass-Minimal` (a library with root header, `Internal/`, `Internal/Windows/`, `Tests/`), `Pass-UntrackedNewFile` (an untracked `.cpp` in a correct place: the self-test creates it at test time and asserts no finding, then moves it to `Extra/` and asserts SL1).

## CompileCoverage

`CompileCoverage.ps1`. It proves every `.cpp` and `.c` produces an object in each configuration of its target's matrix built in this phase ([Z2-N-ARCH-171]).

1. Enter the step.
2. Determine the built configurations: with ticket context (`$env:ZAGENT_GATES_JSON` set), the build presets whose `Build.ps1 -Preset <p>` line is in the gates file's `gates` array; without it, the build presets whose `${ZENITH_OUT}/gates/Build.<p>.json` reports `exitCode` 0 and `headSha` equal to `git rev-parse HEAD`. `linux-tsan`, `win-asan` and the HWASan configuration are covered only where their steps run (`Sanitizers`, `ApkCheck`, acceptance), never here.
3. For each built preset `p` (configure tree `t`, configuration `c`): read the codemodel of `t` for configuration `c` and `declared_deps.json`.
4. For every record of `libraries`, `tools`, `benchmarks` and `integrationSuites` whose `supported` contains `t`: compute the expected source set by applying the glob-partition table of `Zenith2/Docs/Architecture/BuildSystem.md` → "Directory rules and globbing" to the files under the record's `dir` (`.cpp` and `.c` only), keeping the partitions that build into a target in `t` (the `Internal/<Platform>/` partition only when `<Platform>` is `t`'s `ZENITH_TARGET_OS`).
5. Each expected source must appear in a compile group of the codemodel target the partition names (the library, `<Name>_Tests`, `<Name>_Objects`, a conformance executable, `ZenithTestSupport`); a missing one is finding `CC1` (`<file> is globbed by <record> but compiled by no target in <t>/<c>`).
6. Each compile-group source must have its object at `<binaryDir>/<target.paths.build>/CMakeFiles/<target name>.dir/<c>/<source path relative to target.paths.source>.obj` (`.o` in the Android tree); a missing object after the build is finding `CC2`.
7. For each `games` record whose `platforms` (within `ZENITH_GPU`) contain `t`'s platform: the generated composition TU `${ZENITH_OUT}/Games/<Name>/<t>/Zenith_GameComposition.generated.cpp` must appear in the game executable's compile group and have its object (finding `CC3`; row M20 reads the matrix from the `games` array).
8. A source in an Android `Internal/Android/` directory of a library whose `supported` lacks every Android preset is finding `CC4` (`dead platform code`).

Fixtures: `CC1-FilteredOut` (a codemodel fixture where `Internal/Windows/X.cpp` is absent from the `win` compile group), `CC2-NoObject`, `CC3-GameTU`, `CC4-DeadAndroidCode`, `Pass-Minimal`. The fixtures are hand-written codemodel reply JSON plus a `declared_deps.json` and an empty object tree the self-test creates; no CMake runs in the self-test.

## Format

`Format.ps1`. Required tool: the pinned `clang-format` (the `llvm` pin).

1. Files: `Get-Zenith2ScanFiles -Extensions .cpp,.c,.h,.hpp,.inl` plus every `*_CompileFail.cpp.in`, excluding `Zenith2/ThirdParty/**` ([Z2-N-CONTRACT-396]).
2. Check `Zenith2/.clang-format` exists; absent is `FORMAT-NO-STYLE` (`fail`).
3. Run `clang-format --dry-run --Werror --style=file:<repo>/Zenith2/.clang-format <files…>` in batches of at most 64 files (command-line length), collecting `<file>:<line>:<col>: error: code should be clang-formatted` lines; each is a finding `FORMAT` at that file and line.
4. A `// clang-format off` without a matching `// clang-format on` in the same file is finding `FORMAT-OFF-UNCLOSED` ([Z2-N-CONTRACT-397]).

Fixtures: `FORMAT-Bad` (a header with spaces for indentation), `FORMAT-OFF-UNCLOSED`, `Pass-Formatted`.

## HeaderSelfContained

`HeaderSelfContained.ps1` runs the four steps of `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" (subsection "HeaderSelfContained"), with these realisations:

- Headers: every `Zenith2/Engine/<Lib>/*.h` of every `libraries` record of `declared_deps.json` whose `supported` contains `win`.
- The include path "limited to the public include directories of `<Lib>` and its declared direct dependencies (excluding every `Internal/` directory)" is realised by `New-Zenith2HscOverlay` (an addition to the contract's "Shared gate environment" functions, reconciliation R17; the TU itself is still written by `New-Zenith2HeaderTU` at `${ZENITH_OUT}/hsc/<Lib>/<Header>.hsc.cpp`) as an overlay tree `${ZENITH_OUT}/hsc/overlay/<Lib>/`, rebuilt when any copied file is newer, holding copies of the root headers and root `.inl` files of `<Lib>` and of each library in its `depends`, at `<Dep>/<Header>`, and, when `<Lib>` is or depends on ZenithCore, the per-platform threading header of the compile's platform (`Windows` for the MSVC compile, `Android` for step 3), copied from `Zenith2/Engine/ZenithCore/Platform/<Platform>/Zenith_PlatformThreading.h` to `ZenithCore/Platform/<Platform>/` in the overlay, the one non-root header a public header may reach; the compile's `-I` list is that overlay root alone, plus the overlay's `ZenithCore/Platform/<Platform>/` directory when the codemodel's `includes[]` of `<Lib>` names the per-platform directory, plus the generated directory of `ZenithShadersGenerated` when `<Lib>` depends on it, plus the MSVC and Windows SDK system directories of `Get-Zenith2MsvcEnvironment`; no third-party directory is on it (a public header that needs one is a leakage defect).
- Compile flags: the `defines[]` and `compileCommandFragments` of `<Lib>`'s `win-debug` compile group (`Get-Zenith2CompileFlags`), with `/c /Fo<scratch>` and warnings as errors; include flags from the codemodel are dropped in favour of the overlay.
- Step 3 (Android flags) runs when `Get-Zenith2ActiveGateLines` contains `Build.ps1 -Preset android-arm64`: clang from the NDK with `--target=aarch64-linux-android34 --sysroot=<NDK sysroot> -fsyntax-only`, `<Lib>`'s android compile fragments, and the same overlay.
- The per-platform threading headers are compiled as headers too: `ZenithCore/Platform/Windows/Zenith_PlatformThreading.h` under the MSVC flags, `ZenithCore/Platform/Android/Zenith_PlatformThreading.h` under step 3's flags when step 3 runs; no Linux compile runs in this step, so the Linux header is proved by the `linux-tsan` build. The Android and Linux headers include `<pthread.h>` and `<semaphore.h>`, admitted to them alone by ForbiddenCheck's `platform_headers.json` exception.
- A compile error is finding `HSC` at the header, with the first error line.

Fixtures: `HSC-MissingInclude` (a header using `uint32` without including `ZenithCore/Zenith_Types.h`), `Pass-PlatformThreadingHeader` (a ZenithCore root header that includes the per-platform threading header through the per-platform include directory), `HSC-InternalInclude` (a header including its own `Internal/` header: the overlay has no `Internal/`, so the compile fails), `HSC-DepUndeclared` (a header including a library not in `depends`), `Pass-SelfContained`.

## ApiContractCheck

`ApiContractCheck.ps1` runs the steps of `Zenith2/Docs/Contracts.md` → "Contract blocks per type" (subsection "ApiContractCheck behaviour") that check contract blocks, deviations, examples and `.inl` declarations, including their rule IDs, the front-end command and the self-test fixtures (one failing fixture header per rule ID and the two passing fixtures), under `Zenith2/Tools/Gates/Tests/ApiContractCheck/`. It checks no exception specification and no attribute: a public function is not required to be `noexcept` and no `[[nodiscard]]` is required anywhere, so the step has no `noexcept` step and emits neither `ACC-NOEXCEPT-MISSING` nor `ACC-NOEXCEPT-CONDITIONAL`, and has no fixture for either; the passing fixture `Pass-NoExceptionSpecifications` (public functions, a constructor, a `= default` member and a function-pointer alias without `noexcept`, and a `Zenith_Result` return without `[[nodiscard]]`) yields no finding. A `noexcept` or `[[nodiscard]]` that does appear is ForbiddenCheck's rule F25 (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"). This design adds only the realisations those steps leave open:

- The JSON AST reader is `Zenith2/Tools/Gates/Common/Zenith2ClangAst.psm1`, which streams clang's `-ast-dump=json` output through `System.Text.Json.Utf8JsonReader` (the AST of a header with the standard library included runs to tens of megabytes, which `ConvertFrom-Json` cannot hold), carrying the sticky `file` member forward as step 3 requires.
- Headers parse in parallel, at most `[Environment]::ProcessorCount` clang processes at a time.
- The report is `${ZENITH_OUT}/gates/ApiContractCheck.json` in the step-report format, whose `findings` carry the step's own rule IDs.
- Step 7's "directly above a member or function declaration" covers every member a public type declares: a function, a data member (`FieldDecl`, `VarDecl` of a static member) and an enumerator of a public enumeration (`EnumConstantDecl`). MAINT's deprecation markers sit on all three (MAINT's `Version` clause checks, cross-area note MT3 (g)), so the step keeps those three node kinds for the deviation test. Passing fixtures `Pass-DeviationOnDataMember` and `Pass-DeviationOnEnumerator` pin the behaviour.

## ReviewSignals

`ReviewSignals.ps1` reports and never fails (exit 0, except 3 and 124). Every finding has severity `report`.

| Signal | Computation | Report rule |
|---|---|---|
| comment ratio | per changed C++ file (all files without ticket context): comment lines divided by code lines, counted by the ForbiddenCheck tokeniser; above 0.5 reported | `RS-COMMENT-RATIO` |
| near the file-length cap | a file whose code lines exceed 90% of `max-file-loc` of the Zenith2 profile | `RS-FILE-NEAR-CAP` |
| near the complexity caps | a function at or above 90% of `max-cc`, `max-cognitive`, nesting or `max-loc`, read from the Complexity step's report when it ran in this phase | `RS-FUNCTION-NEAR-CAP` |
| new exemption | a changed line that adds an entry to any allowlist or profile-exception list in the diff | `RS-NEW-EXEMPTION` |
| shortcut marker | the words `HACK`, `XXX` or `FIXME` in a changed comment | `RS-SHORTCUT-MARKER` |

The report is the reviewer's input: the reviewer conventions name it.

## ShaderChecks

`ShaderChecks.ps1` refuses Vulkan-specific syntax in Slang sources, aggregates the shader lints (`Zenith2/Docs/Rendering/ShaderSystem.md` → "Shader lints") and proves the build produced current outputs ([Z2-N-REND-112], [Z2-N-REND-127]).

1. Enter the step.
2. **NoVulkanAttributes.** Every file of `Get-Zenith2ScanFiles -Extensions .slang` (all of `Zenith2/` outside `out/`, `ThirdParty/` and the gate fixtures, so a Slang source under `Zenith2/Shaders/`, a library's `Tests/` or a tool is covered alike) is read with its `//` and `/* … */` comments and its string literals blanked, line numbers kept. Each occurrence of the identifier `vk` (matched as a whole identifier, as ForbiddenCheck matches tokens) followed, after optional blanks, by `::` is a `fail` finding `ShaderChecks.NoVulkanAttributes` at its file and line: a `[[vk::binding(…)]]`, `[vk::push_constant]` or other `vk::` attribute, and a `vk::` intrinsic or type. Push and root constants are entry-point `uniform` parameters and resources use `ParameterBlock` or `ConstantBuffer` with Slang's reflection-driven layout, so no Slang source needs one. The scan runs no compiler and comes before item 3, so it covers a tree that has no shader sources yet; nothing exempts it (item 7).
3. When no `*.slang` file exists under `Zenith2/Shaders/` and `declared_deps.json` has no `ZenithShadersGenerated` record, print `ShaderChecks: no shader sources yet` and end the step, which then fails only on findings of item 2 ([Z2-N-BUDGET-348]).
4. The compiler is `${ZENITH_HOST_TOOLS_DIR}/ZenithShaderCompiler.exe` (the `win-tools` output); absent while shader sources exist is `SHADER-NO-COMPILER` (`fail`).
5. Run `ZenithShaderCompiler --check --shader-root <repo>/Zenith2/Shaders --out-dir <ZENITH_OUT>/generated/ZenithShadersGenerated --header-dir <ZENITH_OUT>/generated/ZenithShadersGenerated --target all --tier all --config debug --container-magic <magic> --container-version <version>`, the magic and version read from the `ZenithShadersGenerated` record's recorded compile command in the codemodel (the step never restates the numbers); exit 3 from the compiler (stale outputs) is finding `SHADER-STALE` listing each stale output.
6. Read every `Lint/**/*.lint.json` under the out directory; each `results` element with `severity` `fail` and `allowlistedBy` null is a `fail` finding with rule `<lint>` (for example `ShaderChecks.DiscardOnlyInAlphaTest`) at its `file` and `line`; each `report` result is a `report` finding.
7. Validate `Zenith2/Shaders/ShaderChecks_allowlist.json`: every entry has `lint`, `program`, `decision` (a `Z2-D-` ID present as a heading in `Zenith2/Docs/DecisionLog.md`) and `reason`; a malformed entry, and an entry whose `lint` is `NoVulkanAttributes` (that rule admits no exemption), is `SHADER-ALLOWLIST`; an `allowlistedBy` naming no entry is `SHADER-ALLOWLIST-DANGLING`.

Fixtures: a lint directory with one `fail` result (`SHADER-LINT`), one allowlisted `fail` (passes), one `report` (passes with a report finding), an allowlist entry without a decision, `Pass-NoShaders`, and for item 2 `NoVulkanAttributes-Binding` (a `.slang` file under `Zenith2/Shaders/` with `[[vk::binding(0, 1)]]`), `NoVulkanAttributes-PushConstant` (`[vk::push_constant]` on a struct), `NoVulkanAttributes-Intrinsic` (a `vk::RawBufferLoad` call in a `.slang` file outside `Zenith2/Shaders/`), `NoVulkanAttributes-NoCompiler` (a `vk::` attribute in a tree with no compiler and no `ZenithShadersGenerated` record: the finding is reported before the no-shaders exit), `NoVulkanAttributes-Allowlisted` (an allowlist entry naming the lint: `SHADER-ALLOWLIST`, and the finding stays) and `Pass-NoVulkanSyntax` (`vk::` only in a comment and a string literal, the identifiers `vkLayout` and `m_vk`, and push constants as an entry-point `uniform` parameter).

## EcsConformance

`EcsConformance.ps1` runs the ECS conformance suite against the three backend targets (Z2-D-039; `Zenith2/Docs/Design/ECS/Tests.md` → "Conformance suite").

1. When `declared_deps.json` has no record with `backendOf` equal to `ZenithECS`, print `EcsConformance: no ECS backend target yet` and pass ([Z2-N-BUDGET-348]).
2. Ensure `win-debug` is built; the conformance executable is `ZenithECS_Conformance`.
3. Run `ctest --preset win-debug --output-on-failure --tests-regex "^ZenithECS/.*\[(Custom|Flecs|EnTT)\]$" --output-junit <ZENITH_OUT>/gates/tests/ctest-ecs-conformance.xml`.
4. For every backend record of the family, at least one test with its `[<BACKEND_NAME>]` suffix must have run; a backend target that exists and contributed no test is finding `ECS-BACKEND-NO-TESTS`.
5. A failing test is `TEST-FAILED` as in "Test".

## Sweep

`Sweep.ps1` is the Z2 categories' `cleanup` line. It calls `Invoke-Zenith2ExeSweep` for this checkout, prints each killed path, removes nothing under `Zenith2/out/`, and exits 0. Lane mode skips `cleanup`, so only a main-mode Z2 tick reaches it (`Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 8). It enters no step context (it writes no report) because it must run even when the environment is broken.

## Run-Committed

`Run-Committed.ps1` is CI's entry point and the acceptance runner (Z2-D-031, Z2-D-032, Z2-D-083). It reads the committed gate lines through `Get-Zenith2ActiveGateLines` and runs the same step scripts a tick runs. Its logic lives in `Zenith2RunCommitted.psm1` so the self-test can drive it without running a gate.

### -ListJobs -Json

Prints one JSON object and exits 0; `plan` job of the CI matrix (`Zenith2/Docs/Design/SHIP/Qualification.md` → "CI matrix"):

```json
{ "schema": "zenith2.ci-jobs", "schemaVersion": 1, "windows": true, "linux": false, "windowsTimeoutMinutes": 410, "linuxTimeoutMinutes": 0 }
```

- `windows` is true while any non-acceptance line has host `windows`; `linux` is true exactly when a `Sanitizers.ps1 -Preset linux-tsan` line is active ([Z2-N-ARCH-221]).
- Each timeout is the sum, over the lines the job runs (the CI-only lines included for `windows`), of `Get-Zenith2StepTimeoutMinutes` for each line's step and preset, plus 15 minutes for the job's checkout and setup check.

### -Host windows and -Host linux

1. Lines: every active line of every Z2 category except `Zenith2Acceptance`, whose host equals `-Host`, de-duplicated by exact string in category declaration order (`Zenith2` first, then `Zenith2Render`, `Zenith2ECS`, `Zenith2Memory`), then, for `windows`, the CI-only lines (`$CiOnlyLines`, empty until Z2T-PROC-051).
2. No ticket context exists, so `Get-Zenith2ChangedPaths` returns `$null` and every path-selected step runs fully: CI is the superset (Z2-D-031).
3. Each line runs as a child `pwsh` process through `Invoke-Zenith2Process` with the line's own step timeout; a failing line does not stop the run, every line runs, and the exit code is 1 when any line failed.
4. On a Linux host the `linux-tsan` line runs natively: `Sanitizers.ps1` detects `$IsLinux` and runs CMake and CTest directly instead of through `wsl.exe`.
5. Output: `${ZENITH_OUT}/gates/ci-<host>.json` listing each line, its exit code, its seconds and its report path, which the workflow uploads with `gates/**`.

### -Acceptance -Stage N

The acceptance run of stage `N` (0 … 7), run by the human in the main checkout with every lane idle ([Z2-N-BUDGET-345]). Parameters: `-Acceptance -Stage <N> [-WriteBaseline] [-InsideSlot]`; `-InsideSlot` is internal (item 2) and refused with exit 2 unless `$env:ZENITH2_BUILD_SLOT_HELD` is `1`.

1. Preconditions, each a refusal with exit 1 naming the failed condition: `zagent lane which --json` reports `mode: main`; `git symbolic-ref --short HEAD` is `master`; `git status --porcelain --untracked-files=all` is empty; no `.zagent/run/*/tick_gates.json` in the main checkout or in any registered lane's checkout (`.git/vault/lanes.json` paths) has `state: running`; `Setup-Zenith2.ps1 -Check` exits 0.
2. Acquire the build slot by re-running itself under the slot: `pwsh -NoProfile -File Tools/build_lock.ps1 -Run "pwsh -NoProfile -File Zenith2/Tools/Gates/Run-Committed.ps1 -Acceptance -Stage <N> -InsideSlot"` (with `-WriteBaseline` appended when given), with `ZENITH2_BUILD_SLOT_HELD=1` in the child's environment; a busy slot past `buildSlotWaitMinutes` is exit 11.
3. Make the run cold: rename `${ZENITH_OUT}` to `${ZENITH_OUT}.prev` (replacing an older `.prev`) and, for the run's duration, point `SCCACHE_DIR` at an empty directory under `${ZENITH_OUT}/sccache-acceptance/`, so every line's `seconds` in `run.json` is a cold measurement (the input of the re-allocation of `Zenith2/Docs/Design/PROC.md` → "Budget rows"). The `-WriteBaseline` run skips this item.
4. Export `ZENITH_ACCEPTANCE_STAGE=<N>` and `ZENITH2_ACCEPTANCE=1` into the environment of every line it runs (row SP10 (a); MOD-P2's `CMake_Declarations_MirrorMatches`; SCOPE's register check), and leave `ZAGENT_GATES_JSON` unset, so every path-selected step runs fully.
5. Start the acceptance watchdog of stage `N` (below) and run the lines of `Zenith2/Tools/Gates/acceptance_suites.json` (below) for every stage `k` from 0 to `N` in order, skipping a line already run in this invocation. `Coverage.ps1 -Stage <N>` is not a line of this run: the acceptance procedure runs it after the fuzz campaign (ruling PROC-R2 M6), so it can read the campaign's record. Each line runs as a child `pwsh` through `Invoke-Zenith2Process` with its step timeout, or under the watchdog alone when its script has no `step_timeouts.json` entry; a failing line does not stop the run.
6. Write `${ZENITH_OUT}/gates/acceptance/stage-<N>/run.json`:

```json
{
    "schema": "zenith2.acceptance-run",
    "schemaVersion": 1,
    "stage": 3,
    "headSha": "<sha>",
    "treeSha": "<git write-tree of a scratch index>",
    "cold": true,
    "watchdogMinutes": 610,
    "startedUtc": "…", "finishedUtc": "…",
    "lines": [ { "stage": 0, "line": "…", "step": "Configure", "preset": null, "exitCode": 0, "seconds": 41.2, "report": "gates/Configure.json" } ],
    "result": "pass"
}
```

7. Exit 0 when every line passed, 1 otherwise, 124 when the watchdog fired (the running line's tree is killed and the remaining lines are recorded with `exitCode` null). The device, visual and human evidence the acceptance ticket lists is collected by the human outside this run and attached to the ticket with `run.json`.

**The acceptance watchdog of stage N** is `max(A, ceil(1.25 × T))` minutes:

- `A` is the value of Z2-B-PROC-006 when adopted, or `acceptance.provisionalMinutes` of `step_timeouts.json` while pending.
- `T` is the sum of the timeouts of every line the run will execute for stage `N` (each line's step timeout from `Get-Zenith2StepTimeoutMinutes`; a line with no step entry counts 30).

The watchdog therefore grows with the stage's suite instead of being one number for Stage 0 and Stage 6, and `run.json` records the value used. The fuzz campaign is not a line of this run. SEC's campaigns take up to 11.3 h (Stages 2 to 5) and 79.5 h (Stage 6) (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "Campaign wall-time estimate"), and SEC makes them a separate `needs-human` step of the acceptance. The acceptance procedure therefore runs the campaign on its own, outside the build slot, bounded by SEC's two campaign rows of X415 (`Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance procedure" step 3).

### -Acceptance -Stage N -WriteBaseline

The re-pin of `libgraph_baseline.json`: MAINT's step A2, "surface growth review and baseline re-pin" (`Zenith2/Docs/Design/MAINT/GateData.md` → "Baseline re-pinning"; ruling MAINT-R1 R5). It is the only way any gate run writes a tracked file, and the human's one command for it is `pwsh -NoProfile -File Zenith2\Tools\Gates\Run-Committed.ps1 -Acceptance -Stage <N> -WriteBaseline`.

1. Preconditions: those of item 1 above, which include a clean tree, plus:
   - `${ZENITH_OUT}/gates/acceptance/stage-<N>/run.json` exists with `result: pass`;
   - every path of `git diff --name-only <run.headSha> HEAD` is under `Zenith2/Docs/` or `Zenith2/Tools/Maintenance/`, or is `Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h`. The acceptance's own commits (the budget re-allocation and MAINT's growth report) are the only change allowed since the suites ran. Any other commit since `run.headSha` voids `run.json`, the campaign and the Coverage run, and the acceptance restarts at its run step (`Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance procedure");
   - from Stage 2, MAINT's surface growth report for stage `N` (`Zenith2/Docs/Research/Libgraph/stage-<N>/surface_growth.md`, written by `New-Zenith2SurfaceGrowthReport.ps1`) is committed, and MAINT's `RES-INCOMPLETE` check (`Zenith2/Docs/Design/MAINT/Tests.md` → "Research record rules") passes on it, which is the "disposition filled" test (row MT8).
2. Acquire the slot as item 2 above, and export the two variables of item 4.
3. Run `Libgraph.ps1 -Acceptance -WriteBaseline`. It configures and builds `win-debug` when needed and computes the surface with no ticket context. It fails on any rule LG1 to LG11, including `LG10-UNREVIEWED`. Only when it passes does it rewrite `Zenith2/Tools/Gates/libgraph_baseline.json` from `surface.json`, with `acceptedAt: "Z2 S<N> stage acceptance"` and `headSha` set to `HEAD`.
4. Write `${ZENITH_OUT}/gates/acceptance/stage-<N>/baseline-run.json` (`schema` `zenith2.acceptance-baseline-run`; members `headSha`, `previousHeadSha` of the old baseline, `librariesBefore`, `librariesAfter`, `pairsBefore`, `pairsAfter`) and exit 0. The human commits the baseline in the acceptance ticket's diff, where its review record covers it (`Zenith2/Docs/Design/PROC/Harness.md` → "4. Finish refusal").

`Libgraph.ps1 -Acceptance` is refused with exit 2 unless `Get-Zenith2AcceptanceStage` returns a stage, and `-WriteBaseline` without `-Acceptance` is exit 2. No tick and no CI run can therefore rewrite the baseline. The suites run of item 5 leaves the tree byte-identical: the `treeSha` of `run.json` equals `HEAD`'s tree.

### acceptance_suites.json

`Zenith2/Tools/Gates/acceptance_suites.json` (protected) holds each stage's suite as data, so the acceptance run executes exactly what `Zenith2/Docs/Design/PROC/Acceptance.md` lists:

```json
{
    "schema": "zenith2.acceptance-suites",
    "schemaVersion": 1,
    "comment": "Per-stage acceptance suites; mirrored by the stage tables of Zenith2/Docs/Design/PROC/Acceptance.md (Test-AcceptanceSuites.ps1).",
    "stages": [
        { "stage": 0, "lines": [
            { "line": "pwsh -NoProfile -File Zenith2\\Tools\\Setup\\Setup-Zenith2.ps1 -Check", "when": null },
            { "line": "@category-lines", "when": null },
            { "line": "pwsh -NoProfile -File Zenith2\\Tools\\Gates\\Tests\\Run-GateSelfTests.ps1", "when": null }
        ] }
    ]
}
```

- `line` is an exact command line run from the repository root, or one of two tokens:
  - `@category-lines`: every active line of every Z2 category except `Zenith2Acceptance`, de-duplicated in category declaration order as `-Host windows` selects them, plus the `linux-tsan` line when it is active;
  - `@acceptance-labels`: `Invoke-Zenith2Build -Preset win-release`, then `ctest --preset win-release --label-regex "^z2-acceptance-" --output-on-failure` with `Get-Zenith2TestEnvironment`.
- A line may carry the placeholders `{stage}` (the run's `N`) and `{out}` (`${ZENITH_OUT}`), which are substituted before it runs.
- `when` is `null` (always) or one predicate from a closed set: `lineActive:<exact gate line>` (true when `Get-Zenith2ActiveGateLines` returns that line) or `fileExists:<repo-relative path>`. Anything else is exit 3 at load.
- A stage entry may carry `coverageDeferred`, an array of Draft-IDs that `Coverage.ps1 -Stage N` does not require Done (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage acceptance mode" item 2). Its values are `[ "Z2T-PROC-062" ]` for stage 4, `[ "Z2T-PROC-073" ]` for 5, `[ "Z2T-PROC-083" ]` for 6, `[ "Z2T-PROC-095" ]` for 7, and absent otherwise. Each is the stage's complexity-profile apply ticket, which applies a ruling that measures the very tree the acceptance accepts. `Test-AcceptanceSuites.ps1` fails when an entry is not an apply ticket of that stage.
- Each stage's lines are the rows of that stage's "Run-Committed suite" table in `Acceptance.md`, in order. `Test-AcceptanceSuites.ps1` asserts three things: that mirror; that every line names an existing script; and that every `z2-acceptance-*` label an area's acceptance evidence names is selected by `@acceptance-labels` from the stage that owns it. The areas register the labels in their own CMake files, so the test reads `ctest --preset win-release --show-only=json-v1` and compares labels.

### Test-AcceptanceRecord

`Zenith2/Tools/Gates/Acceptance/Test-AcceptanceRecord.ps1 -Stage <N>` (protected; Z2T-PROC-012; read-only) ties every piece of a stage's acceptance evidence to the tree it accepted (ruling PROC-R2 M6). It checks:

1. `run.json` of stage `N` has `result: pass`, and its `treeSha` equals the tree of `HEAD` with the allowed paths of "-Acceptance -Stage N -WriteBaseline" item 1 (`Zenith2/Docs/**`, `Zenith2/Tools/Maintenance/**`, `Zenith_EngineVersion.h`, `libgraph_baseline.json`) taken from `run.headSha` instead.
2. For `N ≥ 2`, the campaign's evidence record passes `Test-Zenith2EvidenceRecord` (`Zenith2/Docs/Design/DEVT/Evidence.md` → "Zenith2Evidence module"), and its fingerprint is that of `run.headSha`'s tree. Every inventory target ran for its tier's duration, and no reproduced finding is open other than an ACCEPTED one (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "Finding lifecycle").
3. `baseline-run.json` agrees with the committed `libgraph_baseline.json`: `headSha`, `acceptedAt` and the pair counts.
4. `allocation-stage-<N>.json` matches the adopted values of its rows in `Zenith2/Docs/Budgets.md` → "Row index".
5. The newest review block of the acceptance ticket has a valid hash over the current blobs of its paths.
6. The tracked copies of item 3 of "Coverage acceptance mode" exist under `Zenith2/Docs/Research/Acceptance/stage-<N>/` and equal their sources byte for byte.

It writes `${ZENITH_OUT}/gates/acceptance/stage-<N>/record.json` (`schema` `zenith2.acceptance-record`; one `{ check, result, detail }` per item) and prints its SHA-256. Exit 0 when every check passes, 1 otherwise, 3 when an input is missing. Fixtures: one failing case per check and `Pass-Stage2Record`.

### Invoke-ClangTidy

`Zenith2/Tools/Gates/Acceptance/Invoke-ClangTidy.ps1` (protected; Z2T-PROC-012) is the acceptance line for [Z2-N-CONTRACT-393], whose configuration is DEVT's `Zenith2/.clang-tidy` (`Zenith2/Docs/Design/DEVT/QualityTools.md` → "Static analysis"):

1. `Invoke-Zenith2Configure -ConfigurePreset win` with `CMAKE_EXPORT_COMPILE_COMMANDS` on; the compile database is `<binaryDir>/compile_commands.json`.
2. Run the pinned `run-clang-tidy` over every translation unit of the database whose file is under `Zenith2/Engine/`, `Zenith2/Player/` or `Zenith2/Tools/` and outside `Zenith2/ThirdParty/` and the Luau firewall's two `_EH` compile groups, with `-config-file=Zenith2/.clang-tidy`, `-warnings-as-errors=*`, `-header-filter=^.*/Zenith2/(Engine|Player|Tools)/` and `-j <logical processors>`.
3. Every diagnostic is a `fail` finding `TIDY` with its check name. The script has the shared lifecycle but no step-timeout entry: like every acceptance line without one, it runs under the acceptance watchdog alone.

## IntegrationSmoke

`IntegrationSmoke.ps1 [-Game <Name>]`, a CI-only step of `Run-Committed.ps1` from Stage 3 (`Zenith2/Docs/Roadmap.md` → "CI integration smoke").

1. The games: every `Zenith2/Games/<Game>/Workload/*.workload.json` whose `scriptedPath` is non-empty ([Z2-N-BUDGET-192]), or the one `-Game` names.
2. Ensure `win-release` is built and each game's cooked content exists (`cmake --build --preset win-release --target <Game>_Content`).
3. Run `<ZENITH_OUT>/win/Release/bin/<Game>.exe --headless --assets-root <ZENITH_OUT>/Cooked/<Game>/windows --replay <scriptedPath>` when the scripted path ends `.zrpl`, or `--input-script <scriptedPath>` when it ends `.json`, plus `--exit-after-ticks 300 --timeout-seconds <step timeout × 60 divided by the game count, rounded down>` (Z2-B-PROC-030; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Command-line options").
4. Exit code 0 passes; any other exit code is finding `SMOKE-FAILED` with the last 40 lines of the game's log; 124 is `SMOKE-TIMEOUT`.
5. Timings are never budget evidence ([Z2-N-BUDGET-356]).

## Gate self-tests

### Runner

`Zenith2/Tools/Gates/Tests/Run-GateSelfTests.ps1` runs every script matching its two discovery globs, `Tests/Test-*.ps1` (the cross-cutting tests below and `Test-SecurityConformance.ps1`) and `Tests/*/Test-*.ps1` (the per-step self-tests, and SEC's `Tests/Security/Test-Security.ps1`, which SEC names to match it, ruling SEC-R2 m5), prints each script's pass count, and exits 1 when any fails. Each `Test-<Step>.ps1` runs its step against each fixture directory by setting `ZENITH2_GATE_FIXTURE_ROOT=<fixture dir>`, which `Get-Zenith2RepoRoot`, `Get-Zenith2ScanFiles`, `Get-Zenith2DeclaredDeps` and `Get-Zenith2FileApiReply` honour (the fixture directory stands in for the repository root and holds its own `declared_deps.json` and codemodel reply), and asserts the exit code and rule set of `expect.json`. The variable is read by those four functions and by nothing else, and a step run with it set writes its report under `${ZENITH_OUT}/gates/selftest/<Step>/<Case>/`.

### Cross-cutting tests

| Test | Asserts |
|---|---|
| `Test-CategoryAgreement.ps1` | the five Z2 categories of the committed `zagent.project.json` agree on every member except `paths`, `gates`, `phaseTimeoutMinutes` and `$comment`; every Z2 category has a non-empty `gates`; every gate and cleanup line starts `pwsh -NoProfile -File Zenith2\Tools\Gates\`; a sample ticket's resolved rules (through `node Tools/Vault/cli/client.mjs --vault <repo> gates <fixture key> --file <changed list>` against a fixture vault) are identical under each category except `gates` |
| `Test-LocalSubsetOfCi.ps1` | for every one of 40 generated changed sets (one per category path glob, one per pair of categories, the empty set, and `Zenith2/**` alone), the union `routing.mjs` selects is a subset of `Run-Committed.ps1`'s CI line set (Z2-D-031); the CI-only lines are excluded from the comparison |
| `Test-TreeStability.ps1` | in a scratch clone of the repository at `HEAD`, a full `tick_gates.ps1 <fixture key> -Phase verify -ReuseGateList` over a `gates.json` naming every active base line leaves `treeShaStart == treeShaEnd` (Stage 0 deliverable 8; Z2-D-119) |
| `Test-GateScriptsExist.ps1` | every active gate line, `measureSteps` line, cleanup line and CI-only line names a script that exists under `Zenith2/Tools/Gates/` (the activation state machine of `Zenith2/Docs/Design/PROC.md` → "Gate line activation state machine"). The hand-over lines name scripts that do not exist until Z2T-PROC-013 lands, so this test is first run by Z2T-PROC-012 after Z2T-PROC-013 (which blocks it) and is an evidence row of the Stage 0 acceptance |
| `Test-AcceptanceSuites.ps1` | the mirror, script-existence, label and `coverageDeferred` checks of "acceptance_suites.json" |
| `Test-StepTimeouts.ps1` | every `row` of `step_timeouts.json` resolves to a row of `Zenith2/Docs/Budgets.md` → "Row index" with unit `min`; no `NEW-ROW-` value remains; every gate line's step has an entry |
| `Test-SanitizerPathsMatchCategory.ps1` | the `Zenith2Memory` category `paths` equal the union of `sanitizer_paths.json` (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizer path selection"); lands with Z2T-PROC-041, and the runner skips it with a report line while `sanitizer_paths.json` does not exist |

### Per-step self-test names

Each step's self-test asserts, besides its fixtures: `<Step>_UnknownParameter_Exits2`; `<Step>_MissingEnvironment_Exits3` (run with `PATH` stripped of the pinned tools); `<Step>_InternalError_Exits1` (a fixture whose input makes the step's parser throw); `<Step>_Timeout_Exits124` (run with `ZENITH2_GATE_TIMEOUT_OVERRIDE_SECONDS=1` against a fixture whose child sleeps, the one override the timeout resolution honours, read only when `ZENITH2_GATE_FIXTURE_ROOT` is also set); `<Step>_ReportWrittenOnEveryPath` (the four cases above each leave a report file with the matching `exitCode`).
