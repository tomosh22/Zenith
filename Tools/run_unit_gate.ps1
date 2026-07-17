# run_unit_gate.ps1 -- boot a game exe headless and assert the engine
# unit-test baseline. CI's dedicated unit gate (engine-gate.yml); mirrors the
# tolerance logic of Tools/test_scaffold.ps1: a clean "<N> ran, <N> passed" OR
# the single known layout-sensitive flake (GraphComponent::
# RegistryWideNodeRoundTrip, task_726cc81d) as the SOLE failure.
#
# Usage:  pwsh ./Tools/run_unit_gate.ps1 -Exe <game exe> [-Baseline 1081]
# Exit:   0 = baseline met, 1 = anything else (missing exe, timeout, failures).
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Exe,
    [int]$Baseline = 1088,
    [int]$TimeoutSec = 180,
    [string]$LogPath = ""
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $Exe)) {
    Write-Error "[unit_gate] executable not found: $Exe"
    exit 1
}

if ($LogPath -eq "") {
    $root = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { Join-Path (Split-Path -Parent $PSScriptRoot) 'Build/artifacts' }
    New-Item -ItemType Directory -Force -Path $root | Out-Null
    $LogPath = Join-Path $root 'unit_gate_boot.log'
}

# Boot far enough for units-at-boot to complete, then exit. Games can hang at
# shutdown (known), so a watchdog + kill guards the gate; the units line has
# already been written by then.
#
# Deliberately NO --skip-tool-exports: the engine unit suite includes asset-export
# tests (ProceduralTree::*, StickFigure*) that reload GenerateTestAssets output from
# disk. Zenith/Assets/ is gitignored, so on a from-scratch CI checkout that output
# only exists if this boot generates it; --skip-tool-exports left those tests loading
# missing assets and wedging the units-at-boot gate. The exports are CPU-only and
# headless-safe. This script is invoked ONLY by engine-gate.yml, so the change is
# scoped here -- the game test gates (cb/dp-tests) keep --skip-tool-exports via their
# own harness (they run --skip-unit-tests, so they need no generated engine assets).
Write-Host "[unit_gate] Booting $Exe (baseline $Baseline, timeout ${TimeoutSec}s)..." -ForegroundColor Cyan
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = (Resolve-Path $Exe).Path
$psi.Arguments = '--headless --exit-after-frames 120'
$psi.WorkingDirectory = Split-Path (Resolve-Path $Exe).Path
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$proc = [System.Diagnostics.Process]::Start($psi)
$stdout = $proc.StandardOutput.ReadToEndAsync()
$stderr = $proc.StandardError.ReadToEndAsync()
if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
    Write-Host "[unit_gate] timeout after ${TimeoutSec}s; killing (units line should already be logged)" -ForegroundColor Yellow
    try { $proc.Kill() } catch { }
}
$outText = $stdout.Result + "`n" + $stderr.Result
[System.IO.File]::WriteAllText($LogPath, $outText, (New-Object System.Text.UTF8Encoding($false)))

$unitsLine = ($outText -split "`n" | Where-Object { $_ -match 'Unit tests complete' } | Select-Object -Last 1)
if (-not $unitsLine) {
    Write-Error "[unit_gate] no 'Unit tests complete' line in boot output (log: $LogPath)"
    exit 1
}
Write-Host "[unit_gate] $($unitsLine.Trim())"

# Parse "<ran> ran, <passed> passed, <failed> failed[, <skipped> skipped]". The
# harness reports ran == total-registered (a ZENITH_SKIP moves a test from the
# passed bucket to the skipped bucket, never off the ran count). Deliberate skips
# are expected and are never a failure: RegistryWideNodeRoundTrip is quarantined
# (task_726cc81d intermittent heap corruption), so a clean boot is
# "1078 ran, 1077 passed, 0 failed, 1 skipped".
if ($unitsLine -notmatch '(\d+)\s+ran,\s+(\d+)\s+passed,\s+(\d+)\s+failed(?:,\s+(\d+)\s+skipped)?') {
    Write-Error "[unit_gate] could not parse the tally from '$($unitsLine.Trim())' (log: $LogPath)"
    exit 1
}
$ran     = [int]$Matches[1]
$passed  = [int]$Matches[2]
$failed  = [int]$Matches[3]
$skipped = if ($Matches[4]) { [int]$Matches[4] } else { 0 }
$skipNote = if ($skipped -gt 0) { " ($skipped skipped)" } else { "" }

# The full registered suite must have run (guards against tests silently vanishing).
$fullSuite = ($ran -eq $Baseline)

if ($fullSuite -and $failed -eq 0) {
    Write-Host "[unit_gate] PASS ($passed/$Baseline passed, 0 failed$skipNote)" -ForegroundColor Green
    exit 0
}
Write-Error "[unit_gate] baseline NOT met (wanted $Baseline ran, 0 failed; got '$($unitsLine.Trim())'; log: $LogPath)"
exit 1
