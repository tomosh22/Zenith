# run_cb_tests.ps1 -- build-agnostic autonomous test gate for the CityBuilder
# game. Discovers every registered automated test, runs them in one process via
# --all-automated-tests, tallies per-test JSON, and exits non-zero if any failed
# (or the engine exited non-zero). Modeled on Tools/run_dp_tests.ps1.
#
# Usage (from repo root):
#   pwsh ./Tools/run_cb_tests.ps1 -Headless
#   pwsh ./Tools/run_cb_tests.ps1 -Filter CB_Boot -Headless
#   pwsh ./Tools/run_cb_tests.ps1                       # windowed (GPU tests run)
#
# The four terrain-deform tests (CB_Deform* / CB_Paint* / CB_Physics*) set
# m_bRequiresGraphics=true and are auto-skipped under -Headless; run windowed
# (omit -Headless) to exercise them.
#
# ASCII-only body so any PowerShell host parses it cleanly.

[CmdletBinding()]
param(
    [string]$Exe            = "Games/CityBuilder/Build/output/win64/vs2022_debug_win64_true/citybuilder.exe",
    [string]$ResultsDir     = "build/citybuilder_test_results",
    [string]$Filter         = "",
    [int]$ExitAfterFrames   = 6000,
    [double]$FixedDt        = 0.01666,
    [switch]$Headless,
    [switch]$NoSkipToolExports,
    [switch]$NoSkipUnitTests
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Exe)) {
    Write-Error "Executable not found: $Exe (build vs2022_Debug_Win64_True first)"
    exit 1
}

if (-not (Test-Path $ResultsDir)) {
    New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null
}

# Wipe stale per-test JSON so renamed/removed tests or a crashed mid-batch run
# can't leave orphan passes that mask a regression.
Get-ChildItem -Path $ResultsDir -Filter "*.json" -File -ErrorAction SilentlyContinue |
    ForEach-Object { Remove-Item $_.FullName -Force }

$exeDir = Split-Path -Parent (Resolve-Path $Exe).Path

# Self-heal #1: slang runtime DLLs. The Sharpmake post-build xcopy can miss
# slang's dependency tree (slang-rt, slang-glslang, ...). Without them the exe
# dies with STATUS_DLL_NOT_FOUND before the harness runs.
$slangBinDir = "Middleware/slang/bin"
if (Test-Path $slangBinDir) {
    foreach ($dll in Get-ChildItem "$slangBinDir/*.dll" -ErrorAction SilentlyContinue) {
        $destPath = Join-Path $exeDir $dll.Name
        if (-not (Test-Path $destPath)) {
            Copy-Item $dll.FullName -Destination $destPath -Force
            Write-Host "[run_cb_tests] copied $($dll.Name) -> exe dir" -ForegroundColor DarkGray
        }
    }
}

# Self-heal #2: any other runtime DLL (assimp, etc.) a sibling game already has
# in its output dir. CityBuilder's first build may not have them yet.
$siblingDirs = Get-ChildItem "Games/*/Build/output/win64/vs2022_debug_win64_true" -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -ne $exeDir }
foreach ($sib in $siblingDirs) {
    foreach ($dll in Get-ChildItem "$($sib.FullName)/*.dll" -ErrorAction SilentlyContinue) {
        $destPath = Join-Path $exeDir $dll.Name
        if (-not (Test-Path $destPath)) {
            Copy-Item $dll.FullName -Destination $destPath -Force
            Write-Host "[run_cb_tests] copied $($dll.Name) from $($sib.Name) -> exe dir" -ForegroundColor DarkGray
        }
    }
}

# Common engine flags. Tool exports + unit tests are skipped by default for
# speed; flip via -NoSkip* when a phase needs them (e.g. ZENITH_TEST unit tests).
$commonFlags = @()
if (-not $NoSkipToolExports) { $commonFlags += '--skip-tool-exports' }
if (-not $NoSkipUnitTests)   { $commonFlags += '--skip-unit-tests' }
if ($Headless)               { $commonFlags += '--headless' }

# Discover registered tests. Discovery is mode-agnostic (the test registry is the
# same headless or windowed), so always list HEADLESS: a windowed listing authors
# + streams the terrain, which both slows the listing and interleaves log lines
# that corrupt the name parse below. Strip ANSI colour codes for the same reason.
Write-Host "[run_cb_tests] Discovering tests..." -ForegroundColor Cyan
$listArgs = @('--list-automated-tests', '--skip-tool-exports', '--skip-unit-tests', '--headless')
$esc = [char]27
$listOutput = & $Exe @listArgs 2>&1 | ForEach-Object { ("$_" -replace "$esc\[[0-9;]*m", "") }
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
# @(...) is load-bearing: a filter that matches exactly one test would otherwise
# unwrap to a scalar string, so $tests[0] would index the STRING ('CB_Play'->'C').
if ($Filter -ne "") { $tests = @($tests | Where-Object { $_ -like "*$Filter*" }) }

if ($tests.Count -eq 0) {
    Write-Error "No tests discovered (Filter='$Filter'). Engine list output above."
    $listOutput | ForEach-Object { Write-Host "    $_" }
    exit 1
}

Write-Host "[run_cb_tests] Found $($tests.Count) test(s):" -ForegroundColor Cyan
$tests | ForEach-Object { Write-Host "    $_" }

# Run. A single filtered test uses --automated-test (one test, fast — handy for
# windowed iteration); otherwise the whole batch runs in one process.
$single = ($tests.Count -eq 1)
Write-Host ""
if ($single) {
    Write-Host "[run_cb_tests] Running single test '$($tests[0])'..." -ForegroundColor Yellow
    $runArgs = @(
        '--automated-test', $tests[0],
        '--exit-after-frames', $ExitAfterFrames,
        '--fixed-dt', $FixedDt,
        '--test-results', (Join-Path $ResultsDir "$($tests[0]).json")
    ) + $commonFlags
} else {
    Write-Host "[run_cb_tests] Running all tests in one process (batch mode)..." -ForegroundColor Yellow
    $runArgs = @(
        '--all-automated-tests',
        '--exit-after-frames', $ExitAfterFrames,
        '--fixed-dt', $FixedDt,
        '--test-results-dir', $ResultsDir
    ) + $commonFlags
}

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
& $Exe @runArgs 2>&1 | Tee-Object -Variable runOutput
$exitCode = $LASTEXITCODE
$stopwatch.Stop()
Write-Host "[run_cb_tests] Batch run finished in $([int]$stopwatch.Elapsed.TotalSeconds)s (exit=$exitCode)" -ForegroundColor Cyan

$passed = 0
$failed = @()

if ($exitCode -ne 0) {
    Write-Host "[run_cb_tests] Last 60 lines of engine output:" -ForegroundColor Yellow
    $runOutput | Select-Object -Last 60 | ForEach-Object { Write-Host "    $_" }
    Write-Host "[run_cb_tests] Engine exited non-zero -- flagging batch as failed." -ForegroundColor Red
    $failed += "<batch:exit=$exitCode>"
}

# Tally per-test JSON.
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
    $skipTag = if ($obj.PSObject.Properties.Name -contains 'skipped' -and $obj.skipped) { ' (skipped)' } else { '' }
    if ($obj.passed) {
        Write-Host "    PASS $name$skipTag" -ForegroundColor Green
        $passed++
    } else {
        Write-Host "    FAIL $name ($jsonPath)" -ForegroundColor Red
        $failed += $name
    }
}

Write-Host ""
$failCount = if ($null -eq $failed) { 0 } else { @($failed).Count }
Write-Host "[run_cb_tests] Summary: $passed passed, $failCount failed" -ForegroundColor Cyan

if ($failCount -gt 0) {
    Write-Host "Failed:" -ForegroundColor Red
    $failed | ForEach-Object { Write-Host "    $_" }
    exit 1
}
exit 0
