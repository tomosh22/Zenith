# run_dp_tests.ps1 — discover and run every registered DevilsPlayground
# automated test, aggregate JSON results, and exit non-zero if any failed.
#
# Usage (from repo root):
#   pwsh ./Tools/run_dp_tests.ps1
#   pwsh ./Tools/run_dp_tests.ps1 -Filter Hello_Test
#   pwsh ./Tools/run_dp_tests.ps1 -ResultsDir build/dp_test_results
#   pwsh ./Tools/run_dp_tests.ps1 -PerProcess     # legacy: spawn one process per test
#
# Default mode (batch): one process runs every test sequentially via the
# engine's --all-automated-tests flag. Cuts total runtime by ~70% vs the
# per-test-process mode because engine boot (~20s) only happens once.
#
# -PerProcess mode: legacy fallback that forks per test for full process
# isolation (slower, but bullet-proof against state leaks). Used to be the
# default — keep available for diagnosing test interactions.
#
# -Filter applies in both modes; in batch mode it falls back to per-process
# spawning because the engine batch flag has no built-in filter.

[CmdletBinding()]
param(
    [string]$Exe         = "Games/DevilsPlayground/Build/output/win64/vs2022_debug_win64_true/devilsplayground.exe",
    [string]$ResultsDir  = "build/dp_test_results",
    [string]$Filter      = "",
    [int]$ExitAfterFrames = 600,
    [double]$FixedDt     = 0.01666,
    [switch]$Headless,
    [switch]$PerProcess
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
#      from the previous run — without a wipe, the script tallies those
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
$listOutput = & $Exe --list-automated-tests --skip-tool-exports --skip-unit-tests 2>&1
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

if ($tests.Count -eq 0) {
    Write-Error "No tests discovered (Filter='$Filter')"
    exit 1
}

Write-Host "[run_dp_tests] Found $($tests.Count) test(s):" -ForegroundColor Cyan
$tests | ForEach-Object { Write-Host "    $_" }

# -Filter forces per-process mode because the engine's --all-automated-tests
# flag has no test-name filter (it runs every registered test).
$useBatch = -not $PerProcess -and $Filter -eq ""

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
    & $Exe @args 2>&1 | Tee-Object -Variable runOutput | Out-Null
    $exitCode = $LASTEXITCODE
    $stopwatch.Stop()
    Write-Host "[run_dp_tests] Batch run finished in $([int]$stopwatch.Elapsed.TotalSeconds)s (exit=$exitCode)" -ForegroundColor Cyan

    # Propagate a non-zero engine exit as a hard failure even if every JSON
    # the tally loop sees happens to say passed=true. The engine reports
    # 1 on any test failure, 2 on harness setup error, 3 on unrecoverable
    # config error — combined with the pre-run JSON wipe above, this stops
    # a partial batch (crashed mid-suite) from masking as success when the
    # last few result files weren't written.
    if ($exitCode -ne 0) {
        Write-Host "[run_dp_tests] Engine exited non-zero — flagging batch as failed even if individual JSONs pass." -ForegroundColor Red
        $failed += "<batch:exit=$exitCode>"
    }

    # Tally from per-test JSON files written by the engine.
    foreach ($name in $tests) {
        $jsonPath = Join-Path $ResultsDir "$name.json"
        if (-not (Test-Path $jsonPath)) {
            Write-Host "    MISSING $name (no JSON written)" -ForegroundColor Red
            $failed += $name
            continue
        }
        try {
            $obj = Get-Content $jsonPath -Raw | ConvertFrom-Json
        } catch {
            Write-Host "    UNPARSEABLE $name ($jsonPath)" -ForegroundColor Red
            $failed += $name
            continue
        }
        if ($obj.passed) {
            Write-Host "    PASS $name" -ForegroundColor Green
            $passed++
        } else {
            Write-Host "    FAIL $name ($jsonPath)" -ForegroundColor Red
            $failed += $name
        }
    }
}
else {
    # ---------------------------------------------------------------------
    # Per-process mode: spawn one process per test (legacy / -Filter / -PerProcess).
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
        }
    }
}

# 3. Summary ---------------------------------------------------------------
Write-Host ""
Write-Host "[run_dp_tests] Summary: $passed passed, $($failed.Count) failed" -ForegroundColor Cyan
if ($failed.Count -gt 0) {
    Write-Host "Failed tests:" -ForegroundColor Red
    $failed | ForEach-Object { Write-Host "    $_" }
    exit 1
}
exit 0
