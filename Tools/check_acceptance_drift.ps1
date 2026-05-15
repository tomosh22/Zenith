# check_acceptance_drift.ps1 -- MVP-4.3.1 acceptance criteria gate.
#
# Implements MvpRoadmap.md MVP-4.3.1:
#   "confirm Test_P4Playthrough_Night1WinGolden completes in < 9000 frames,
#    Test_P4Playthrough_LossByApprehend triggers within 240 frames, and the
#    three loss-state tests fire their expected DP_OnRunLost causes.
#    If any of those drift by > 20% in frame count vs. their initial
#    passing run, investigate; otherwise the loop is acceptably tuned."
#
# What this script does:
#   1. Invokes run_dp_tests.ps1 -Headless -Filter on each of the 4
#      playthrough tests, producing per-test JSON results.
#   2. Reads `frames` from each JSON.
#   3. Compares against:
#      a) Hard absolute bound (from MVPScope/Roadmap spec). Fails on
#         breach -- this is the MVP-DoD gate.
#      b) Soft drift bound (+/- 20% vs reference frame count recorded
#         here from each test's initial passing run). Warns on breach;
#         does NOT fail. This is the "investigate" gate, not a hard
#         rejection -- some variance is expected.
#   4. Exits 0 if all hard bounds are respected, 1 otherwise.
#
# Update reference frame counts when a deliberate change to a test's
# scenario shifts its frame count. The script's job is to catch
# UNINTENDED drift, not to lock the suite against intentional changes.
#
# Usage (from repo root):
#   pwsh ./Tools/check_acceptance_drift.ps1
#   pwsh ./Tools/check_acceptance_drift.ps1 -SkipRun   # use existing JSONs
#
# Required dir: build/dp_test_results/ (run_dp_tests.ps1 default).

[CmdletBinding()]
param(
    [string]$Exe         = "Games/DevilsPlayground/Build/output/win64/vs2022_debug_win64_true/devilsplayground.exe",
    [string]$ResultsDir  = "build/dp_test_results",
    [switch]$SkipRun
)

$ErrorActionPreference = "Stop"

# Acceptance table.
#   Name      = test name string (matches ZENITH_AUTOMATED_TEST_REGISTER's first field)
#   HardCap   = absolute frame bound from MVPScope/MvpRoadmap. EXCEED = FAIL.
#   Reference = initial passing-run frame count. The script's +/-20% drift
#               check compares the live `frames` JSON field against this.
#               Update when a deliberate change shifts the frame count.
$accept = @(
    @{ Name = "Test_P4Playthrough_Night1WinGolden";     HardCap = 9000; Reference = 29  },
    @{ Name = "Test_P4Playthrough_LossByApprehend";     HardCap = 240;  Reference = 200 },
    @{ Name = "Test_P4Playthrough_LossByDawn";          HardCap = 240;  Reference = 95  },
    @{ Name = "Test_P4Playthrough_LossByNoVessels";     HardCap = 240;  Reference = 9   }
)

if (-not $SkipRun) {
    if (-not (Test-Path $Exe)) {
        Write-Error "Executable not found: $Exe -- build vs2022_Debug_Win64_True first"
        exit 1
    }
    if (-not (Test-Path $ResultsDir)) {
        New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null
    }
    # We invoke the engine directly per test instead of going through
    # run_dp_tests.ps1, because the runner wipes the entire ResultsDir
    # on each invocation -- running 4 filtered invocations sequentially
    # would leave only the last test's JSON. Direct invocation lets us
    # keep all 4 JSONs in the same directory.
    Write-Host "[acceptance] Running the 4 playthrough tests directly..." -ForegroundColor Cyan
    foreach ($entry in $accept) {
        $name = $entry.Name
        $jsonPath = Join-Path $ResultsDir "$name.json"
        Write-Host "[acceptance]   $name" -ForegroundColor DarkGray
        $args = @(
            '--headless',
            '--skip-tool-exports',
            '--skip-unit-tests',
            '--automated-test', $name,
            '--exit-after-frames', 600,
            '--fixed-dt', 0.01666,
            '--test-results', $jsonPath
        )
        & $Exe @args 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[acceptance]   $name -- engine exited $LASTEXITCODE (test itself may have failed)" -ForegroundColor Red
        }
    }
}

# Read results, compare against bounds.
$hardFails = 0
$softWarns = 0
$missing   = 0
Write-Host ""
Write-Host "[acceptance] Bound check vs MVP-4.3.1 / MVPScope.md §1.5:" -ForegroundColor Cyan
foreach ($entry in $accept) {
    $name      = $entry.Name
    $hardCap   = [int]$entry.HardCap
    $reference = [int]$entry.Reference
    $jsonPath  = Join-Path $ResultsDir "$name.json"

    if (-not (Test-Path $jsonPath)) {
        Write-Host "  MISSING $name (no JSON at $jsonPath)" -ForegroundColor Red
        $missing++
        continue
    }
    try {
        $obj = Get-Content $jsonPath -Raw | ConvertFrom-Json
    } catch {
        Write-Host "  UNPARSEABLE $name ($jsonPath)" -ForegroundColor Red
        $missing++
        continue
    }
    if (-not $obj.passed) {
        Write-Host "  PRE-FAIL $name (passed=false; acceptance check is meaningless until test passes)" -ForegroundColor Red
        $hardFails++
        continue
    }

    $frames = [int]$obj.frames
    $hardOk = $frames -le $hardCap

    # +/-20% drift bound around the reference.
    $low  = [int]([math]::Floor($reference * 0.8))
    $high = [int]([math]::Ceiling($reference * 1.2))
    $driftOk = ($frames -ge $low -and $frames -le $high)
    $driftPct = if ($reference -gt 0) { [math]::Round(($frames - $reference) / $reference * 100.0, 1) } else { 0.0 }

    if (-not $hardOk) {
        Write-Host ("  HARD-FAIL $name -- frames=$frames > cap=$hardCap (drift {0:+0.0;-0.0;0}% vs reference $reference)" -f $driftPct) -ForegroundColor Red
        $hardFails++
    }
    elseif (-not $driftOk) {
        Write-Host ("  WARN-DRIFT $name -- frames=$frames vs reference $reference (drift {0:+0.0;-0.0;0}%, bound +/-20%). INVESTIGATE per MVP-4.3.1 spec." -f $driftPct) -ForegroundColor Yellow
        $softWarns++
    }
    else {
        Write-Host ("  PASS $name -- frames=$frames cap=$hardCap reference=$reference drift={0:+0.0;-0.0;0}%" -f $driftPct) -ForegroundColor Green
    }
}

Write-Host ""
Write-Host "[acceptance] Summary: $($accept.Count) tests, $hardFails hard-fail, $softWarns drift-warn, $missing missing" -ForegroundColor Cyan
if ($hardFails -gt 0 -or $missing -gt 0) {
    exit 1
}
exit 0
