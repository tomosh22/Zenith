# run_dp_tests.ps1 -- discover and run every registered DevilsPlayground
# automated test, aggregate JSON results, and exit non-zero if any failed.
#
# Usage (from repo root):
#   pwsh ./Tools/run_dp_tests.ps1
#   pwsh ./Tools/run_dp_tests.ps1 -Filter Hello_Test
#   pwsh ./Tools/run_dp_tests.ps1 -ResultsDir build/dp_test_results
#   pwsh ./Tools/run_dp_tests.ps1 -PerProcess              # legacy: spawn one process per test
#   pwsh ./Tools/run_dp_tests.ps1 -Tier 0                  # only Tier-0 tests (Test_T0*)
#   pwsh ./Tools/run_dp_tests.ps1 -Tier 1                  # only Tier-1 tests (Test_P1*)
#   pwsh ./Tools/run_dp_tests.ps1 -FailFast                # stop on first failure (forces per-process mode)
#   pwsh ./Tools/run_dp_tests.ps1 -AssertionsLog dp.log    # append every failure to dp.log
#
# Default mode (batch): one process runs every test sequentially via the
# engine's --all-automated-tests flag. Cuts total runtime by ~70% vs the
# per-test-process mode because engine boot (~20s) only happens once.
#
# -PerProcess mode: legacy fallback that forks per test for full process
# isolation (slower, but bullet-proof against state leaks). Used to be the
# default -- keep available for diagnosing test interactions.
#
# -Filter applies in both modes; in batch mode it falls back to per-process
# spawning because the engine batch flag has no built-in filter.
#
# -Tier N filters tests by name prefix:
#   -Tier 0  -> keep tests starting with 'Test_T0' (harness sanity / smoke).
#   -Tier 1  -> keep tests starting with 'Test_P1' (Phase 1 features).
#   -Tier 2..4 -> Test_P2.., Test_P3.., Test_P4..
# Per TestPlan.md the test corpus follows Test_T0Harness_<X> / Test_P<N><Topic>_<X>;
# tier filtering uses the prefix substring. Combine with -Filter for finer slicing.
#
# -FailFast forces per-process mode and aborts the batch on the first FAIL.
# In batch mode the engine runs every test in one process regardless of
# individual outcomes, so honoring FailFast requires the per-process path.
#
# -AssertionsLog <path> appends a one-line summary for each FAIL across the
# batch -- test name + result JSON path + any failure message captured from
# the JSON. Intended for grep-driven triage when a long-form test plan spits
# out dozens of failures across many files. The file is OPENED FOR APPEND;
# delete it manually between runs if you want a clean log.
#
# ASCII-only script body (no em-dashes, smart quotes, section signs, etc.) so
# Windows PowerShell 5.1 can parse it without UTF-8/CP1252 mojibake -- see
# Q-2026-05-12-005 for the diagnosis.

[CmdletBinding()]
param(
    [string]$Exe         = "Games/DevilsPlayground/Build/output/win64/vs2022_debug_win64_true/devilsplayground.exe",
    [string]$ResultsDir  = "build/dp_test_results",
    [string]$Filter      = "",
    # Bumped 2026-05-17 from 600 to 8500 to give the
    # PersonalityPlaythrough_* tests (max-frames 6000-8000 each) room to
    # run end-to-end. Each per-test budget is still governed by the
    # test's own `m_iMaxFrames`, so short tests stay short -- this only
    # raises the per-batch ceiling. Stealth is the slowest in practice
    # at ~3600 frames; 8500 leaves comfortable headroom.
    [int]$ExitAfterFrames = 8500,
    [double]$FixedDt     = 0.01666,
    [switch]$Headless,
    [switch]$PerProcess,

    # MVP-0.0.4 additions:
    [ValidateRange(0, 4)]
    [Nullable[int]]$Tier = $null,
    [switch]$FailFast,
    [string]$AssertionsLog = ""
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Exe)) {
    Write-Error "Executable not found: $Exe (build vs2022_Debug_Win64_True first)"
    exit 1
}

if (-not (Test-Path $ResultsDir)) {
    New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null
}

# Wipe stale per-test JSON files from a prior run BEFORE invoking the engine.
# Two distinct failure modes this prevents:
#   1. Renamed/removed tests leaving orphan JSON that the tally loop happily
#      counts as passes.
#   2. An engine crash mid-batch leaving the last few tests' JSONs untouched
#      from the previous run -- without a wipe, the script tallies those
#      stale passes alongside this run's partial results and reports
#      success.
# We only nuke *.json directly inside $ResultsDir to avoid clobbering
# unrelated artefacts the caller may have placed there.
Get-ChildItem -Path $ResultsDir -Filter "*.json" -File -ErrorAction SilentlyContinue |
    ForEach-Object { Remove-Item $_.FullName -Force }

# Self-heal missing slang DLLs. The Sharpmake post-build only copies
# slang.dll, but slang.dll has its own dependency tree (slang-rt, slang-glslang,
# slang-glsl-module, slang-llvm, slang-compiler, gfx). When any of those are
# missing, the exe terminates with STATUS_DLL_NOT_FOUND (0xC0000135) before
# the harness ever runs. Copy them into the output dir if they're not already
# present.
$exeDir = Split-Path -Parent (Resolve-Path $Exe).Path
$slangBinDir = "Middleware/slang/bin"
if (Test-Path $slangBinDir) {
    $slangDlls = Get-ChildItem "$slangBinDir/*.dll" -ErrorAction SilentlyContinue
    foreach ($dll in $slangDlls) {
        $destPath = Join-Path $exeDir $dll.Name
        if (-not (Test-Path $destPath)) {
            Copy-Item $dll.FullName -Destination $destPath -Force
            Write-Host "[run_dp_tests] copied $($dll.Name) -> $exeDir" -ForegroundColor DarkGray
        }
    }
}

# Discover tests (always, so we can print the list before the run and
# decode batch results into per-test pass/fail tally).
Write-Host "[run_dp_tests] Discovering tests..." -ForegroundColor Cyan
# --headless is required on GPU-less CI runners; --list-automated-tests
# would otherwise hang on vkEnumeratePhysicalDevices. Pass it whenever
# the caller asked for -Headless.
$listArgs = @('--list-automated-tests', '--skip-tool-exports', '--skip-unit-tests')
if ($Headless) { $listArgs += '--headless' }
$listOutput = & $Exe @listArgs 2>&1
$tests = @()
$inList = $false
foreach ($line in $listOutput) {
    $s = "$line"
    if ($s -match "^Registered automated tests:") { $inList = $true; continue }
    if ($inList) {
        if ($s -match "^\s+(\S+)\s*$") { $tests += $matches[1] }
        elseif ($s -notmatch "^\s") { $inList = $false }
    }
}

if ($Filter -ne "") {
    $tests = $tests | Where-Object { $_ -like "*$Filter*" }
}

# -Tier filter. Tier 0 is harness-sanity (Test_T0*); Tier N for N>=1 is
# Test_P<N>* per TestPlan.md naming convention.
if ($null -ne $Tier) {
    if ($Tier -eq 0) {
        $tierPrefix = 'Test_T0'
    } else {
        $tierPrefix = "Test_P$Tier"
    }
    $beforeCount = $tests.Count
    $tests = $tests | Where-Object { $_ -like "$tierPrefix*" }
    Write-Host "[run_dp_tests] -Tier $Tier filter: $beforeCount -> $($tests.Count) test(s) (prefix $tierPrefix)" -ForegroundColor Cyan
}

if ($tests.Count -eq 0) {
    Write-Error "No tests discovered (Filter='$Filter', Tier=$Tier)"
    exit 1
}

Write-Host "[run_dp_tests] Found $($tests.Count) test(s):" -ForegroundColor Cyan
$tests | ForEach-Object { Write-Host "    $_" }

# -Filter and -FailFast both force per-process mode:
#   -Filter: engine's --all-automated-tests flag has no test-name filter.
#   -FailFast: in batch mode the engine runs every test regardless of
#              individual outcomes, so honoring FailFast requires per-process.
# -Tier filters the test list BEFORE batch dispatch, so it does NOT force
#   per-process by itself -- unless combined with FailFast/Filter.
$useBatch = -not $PerProcess -and $Filter -eq "" -and -not $FailFast

# AssertionsLog helper: append a one-line failure summary for a given test.
function Append-AssertionLog {
    param([string]$Name, [string]$JsonPath, [string]$Extra)
    if ($AssertionsLog -eq "") { return }
    $stamp = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
    $entry = "$stamp  FAIL  $Name  json=$JsonPath  $Extra"
    Add-Content -Path $AssertionsLog -Value $entry
}

$passed = 0
$failed = @()

if ($useBatch) {
    # ---------------------------------------------------------------------
    # Batch mode: ONE process runs every test sequentially.
    # ---------------------------------------------------------------------
    Write-Host ""
    Write-Host "[run_dp_tests] Running all tests in a single process (batch mode)..." -ForegroundColor Yellow
    $args = @(
        '--all-automated-tests',
        '--skip-tool-exports',
        '--skip-unit-tests',
        '--exit-after-frames', $ExitAfterFrames,
        '--fixed-dt', $FixedDt,
        '--test-results-dir', $ResultsDir
    )
    if ($Headless) { $args += '--headless' }

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    # Tee output into a variable for post-mortem on non-zero exits. The Out-Null
    # used to suppress live output -- swapped for direct console pass-through so
    # CI surfaces engine assertions / crash output as it happens.
    & $Exe @args 2>&1 | Tee-Object -Variable runOutput
    $exitCode = $LASTEXITCODE
    $stopwatch.Stop()
    Write-Host "[run_dp_tests] Batch run finished in $([int]$stopwatch.Elapsed.TotalSeconds)s (exit=$exitCode)" -ForegroundColor Cyan
    if ($exitCode -ne 0) {
        Write-Host "[run_dp_tests] Last 80 lines of engine output:" -ForegroundColor Yellow
        $runOutput | Select-Object -Last 80 | ForEach-Object { Write-Host "    $_" }
    }

    # Propagate a non-zero engine exit as a hard failure even if every JSON
    # the tally loop sees happens to say passed=true. The engine reports
    # 1 on any test failure, 2 on harness setup error, 3 on unrecoverable
    # config error -- combined with the pre-run JSON wipe above, this stops
    # a partial batch (crashed mid-suite) from masking as success when the
    # last few result files weren't written.
    if ($exitCode -ne 0) {
        Write-Host "[run_dp_tests] Engine exited non-zero -- flagging batch as failed even if individual JSONs pass." -ForegroundColor Red
        $failed += "<batch:exit=$exitCode>"
        Append-AssertionLog -Name "<batch>" -JsonPath "" -Extra "engine exited $exitCode"
    }

    # Tally from per-test JSON files written by the engine.
    foreach ($name in $tests) {
        $jsonPath = Join-Path $ResultsDir "$name.json"
        if (-not (Test-Path $jsonPath)) {
            Write-Host "    MISSING $name (no JSON written)" -ForegroundColor Red
            $failed += $name
            Append-AssertionLog -Name $name -JsonPath $jsonPath -Extra "JSON missing"
            continue
        }
        try {
            $obj = Get-Content $jsonPath -Raw | ConvertFrom-Json
        } catch {
            Write-Host "    UNPARSEABLE $name ($jsonPath)" -ForegroundColor Red
            $failed += $name
            Append-AssertionLog -Name $name -JsonPath $jsonPath -Extra "json unparseable"
            continue
        }
        if ($obj.passed) {
            Write-Host "    PASS $name" -ForegroundColor Green
            $passed++
        } else {
            $detail = ""
            if ($obj.failures) { $detail = "failures=$($obj.failures.Count) frames=$($obj.frames)" }
            Write-Host "    FAIL $name ($jsonPath)" -ForegroundColor Red
            $failed += $name
            Append-AssertionLog -Name $name -JsonPath $jsonPath -Extra $detail
        }
    }
}
else {
    # ---------------------------------------------------------------------
    # Per-process mode: spawn one process per test (legacy / -Filter / -PerProcess / -FailFast).
    # ---------------------------------------------------------------------
    foreach ($name in $tests) {
        $jsonPath = Join-Path $ResultsDir "$name.json"
        Write-Host ""
        Write-Host "[run_dp_tests] Running $name..." -ForegroundColor Yellow
        $args = @(
            '--automated-test', $name,
            '--skip-tool-exports',
            '--skip-unit-tests',
            '--exit-after-frames', $ExitAfterFrames,
            '--fixed-dt', $FixedDt,
            '--test-results', $jsonPath
        )
        if ($Headless) { $args += '--headless' }
        & $Exe @args 2>&1 | Out-Null
        $code = $LASTEXITCODE
        if ($code -eq 0) {
            Write-Host "    PASS ($jsonPath)" -ForegroundColor Green
            $passed++
        } else {
            Write-Host "    FAIL exit=$code ($jsonPath)" -ForegroundColor Red
            $failed += $name
            Append-AssertionLog -Name $name -JsonPath $jsonPath -Extra "exit=$code"
            if ($FailFast) {
                Write-Host "[run_dp_tests] -FailFast: aborting batch after first failure ($name)" -ForegroundColor Red
                break
            }
        }
    }
}

# 3. Summary ---------------------------------------------------------------
Write-Host ""
$failCount = if ($null -eq $failed) { 0 } else { @($failed).Count }
Write-Host "[run_dp_tests] Summary: $passed passed, $failCount failed" -ForegroundColor Cyan

# 3a. Slowest-tests report. Reads "durationMs" from each per-test JSON;
# rolls them up + prints the top N. Helps spot outliers dragging suite
# runtime down. Graceful if a JSON lacks the field (pre-timing-feature
# results files).
$timings = @()
foreach ($name in $tests) {
    $jsonPath = Join-Path $ResultsDir "$name.json"
    if (-not (Test-Path $jsonPath)) { continue }
    try {
        $obj = Get-Content $jsonPath -Raw | ConvertFrom-Json
    } catch { continue }
    if ($obj.PSObject.Properties.Name -contains 'durationMs') {
        $timings += [PSCustomObject]@{
            Name       = $name
            DurationMs = [double]$obj.durationMs
            Frames     = if ($obj.PSObject.Properties.Name -contains 'frames') { [int]$obj.frames } else { 0 }
            Skipped    = if ($obj.PSObject.Properties.Name -contains 'skipped') { [bool]$obj.skipped } else { $false }
        }
    }
}
if ($timings.Count -gt 0) {
    $totalMs   = ($timings | Measure-Object -Property DurationMs -Sum).Sum
    $countRan  = ($timings | Where-Object { -not $_.Skipped }).Count
    $avgMs     = if ($countRan -gt 0) { ($timings | Where-Object { -not $_.Skipped } | Measure-Object -Property DurationMs -Average).Average } else { 0 }
    Write-Host ""
    Write-Host ("[run_dp_tests] Timing: {0} tests measured, total = {1:N0} ms, avg = {2:N1} ms" `
        -f $timings.Count, $totalMs, $avgMs) -ForegroundColor Cyan
    $topN = [Math]::Min(10, $timings.Count)
    Write-Host "[run_dp_tests] Slowest $topN tests:" -ForegroundColor Cyan
    $timings |
        Sort-Object -Property DurationMs -Descending |
        Select-Object -First $topN |
        ForEach-Object {
            $tag = if ($_.Skipped) { ' (skipped)' } else { '' }
            Write-Host ("    {0,7:N1} ms  {1,6} frames  {2}{3}" -f $_.DurationMs, $_.Frames, $_.Name, $tag)
        }
}

if ($failCount -gt 0) {
    Write-Host "Failed tests:" -ForegroundColor Red
    $failed | ForEach-Object { Write-Host "    $_" }
    if ($AssertionsLog -ne "" -and (Test-Path $AssertionsLog)) {
        Write-Host "Assertion log appended to: $AssertionsLog" -ForegroundColor Yellow
    }
    exit 1
}
exit 0
