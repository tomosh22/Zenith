# ZenithCli — the `zenith` command-line module

User-facing reference: **[Docs/BuildSystem.md](../../Docs/BuildSystem.md)** §2
(commands, flags, exit codes). This file is for working ON the CLI.

## Layout

```
zenith.bat                     (repo root) -> powershell (5.1) -> Tools/zenith.ps1
Tools/zenith.ps1               dispatcher: imports the module, splats @args into
                               Invoke-ZenithCli, exits with its last pipeline value
Tools/ZenithCli/
  ZenithCli.psm1               all commands + the switch dispatcher
  ZenithTestHarness.psm1       THE automated-test engine (zenith test's backend)
  Tests/
    run_cli_tests.ps1          dependency-free assert-runner (zenith selftest)
    name_validation_cases.txt  shared vectors pinning PS <-> C++ hub validators
Build/zenith_buildsystem.psm1  descriptor/codegen/name rules + shared ops
                               (Get-ZenithDefaultConfig, ConvertTo-ZenithOutputDir,
                               Get-ZenithGameExePath, Stop-ZenithBuildProcesses,
                               Repair-ZenithRuntimeDlls, Test-ZenithRegenDrift,
                               Remove-ZenithOrphanGameArtifacts)
Build/zenith_config.psd1       central config DATA (read via accessors only)
```

Logic placement rule: name/descriptor/codegen/config/process/DLL logic lives in
`zenith_buildsystem.psm1` (single source of truth — the CLI, the test harness,
regen.ps1, and the selftests all import it); `ZenithCli.psm1` owns only arg
parsing, orchestration, and console UX. The test protocol lives ONLY in
`ZenithTestHarness.psm1`.

## Conventions

- **ASCII-only bodies; must parse and run identically under Windows PowerShell
  5.1 AND pwsh 7** (zenith.bat launches 5.1; CI invokes scripts with pwsh).
  No ternary / null-coalescing / PS7-only syntax.
- `Set-StrictMode -Version Latest` is on in the modules — initialize every
  variable, guard property access.
- Exit codes are the module constants `$script:EXIT_*` (0 ok / 1 usage /
  2 validation / 3 generation / 4 build-or-test / 5 not-found). A command
  function RETURNS the code; `zenith.ps1` turns the last pipeline value into
  the process exit code, passing any earlier pipeline output (e.g.
  `list --json`) through to stdout.
- Adding a command: write `Invoke-Zenith<Name> -CmdArgs` (manual `for` loop
  arg parsing — `--flag` / `--opt value`, unknown `--*` returns EXIT_USAGE),
  add the `'^<name>$'` dispatcher line + usage text, and add exit-code cases
  to `Tests/run_cli_tests.ps1`. Selftest coverage is mandatory.

## Hard-won gotchas (violate these and the selftests will tell you)

- **Splat, don't pass arrays positionally.** PS 5.1 does not flatten an array
  into a `ValueFromRemainingArguments` parameter (pwsh 7 does) —
  `Invoke-ZenithCli @('open','X')` arrives as ONE nested element under 5.1.
  `zenith.ps1` splats `@args`; the tests splat via `Invoke-CliCode`.
- **Nested imports need `-Global`.** `Import-Module -Force` inside module B of
  a module the caller already imported globally DISPLACES the global import
  into B's private scope — the caller's next call hits "not recognized".
  `ZenithTestHarness.psm1` imports the buildsystem module `-Force -Global` for
  exactly this reason.
- **Nothing may leak onto a function's pipeline.** Function output IS the
  return value: native-exe output needs `| Out-Host` (or capture), including
  after `Tee-Object` (tee passes through!). The historical failure mode: the
  whole engine log returned from `Invoke-ZenithGameTests`, and StrictMode threw
  `PropertyNotFoundStrict` when the caller member-accessed the result.
- **No comma-trick returns.** `return , $list.ToArray()` + `@(...)` at the call
  site yields a 1-element wrapper around an empty array. Return the plain
  enumerated array; callers normalize with `@(...)` ($null-safe).
- `$Args` is an automatic variable — use `$CmdArgs`. `switch -Regex` falls
  through on multiple matches — dispatcher patterns are anchored `'^name$'`
  and every branch returns.
- Config names are PascalCase for `/p:Configuration=`; output dirs are the
  LOWERCASED name. Only `ConvertTo-ZenithOutputDir` may encode that fact.

## ZenithTestHarness.psm1

`Invoke-ZenithGameTests` = pre-run JSON wipe → `Repair-ZenithRuntimeDlls` →
headless discovery (`ConvertFrom-ZenithTestListOutput` ANSI-strips and parses
the `Registered automated tests:` block) → Filter/Tier trim (`@(...)` on every
reassignment — a single match would otherwise unwrap to a scalar string) →
batch (`--all-automated-tests`) or per-process (forced by
PerProcess/FailFast/Filter) → tally (`Read-ZenithTestResults`:
PASS/FAIL/MISSING/UNPARSEABLE + skipped tag; a non-zero engine exit fails the
batch regardless of JSONs) → timing report. Throws on setup errors (missing
exe, zero tests) — callers map to exit codes. Pure parsers/tally are separate
exported functions so the selftests cover them with fixtures, no engine needed.
