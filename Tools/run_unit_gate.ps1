# run_unit_gate.ps1 -- boot a game exe and assert the engine
# unit-test baseline. CI's dedicated unit gate (engine-gate.yml); mirrors the
# tolerance logic of Tools/test_scaffold.ps1: a clean "<N> ran, <N> passed" OR
# the single known layout-sensitive flake (GraphComponent::
# RegistryWideNodeRoundTrip, task_726cc81d) as the SOLE failure.
#
# Usage:  pwsh ./Tools/run_unit_gate.ps1 -Exe <Null-config game exe> [-Baseline N]
#
# The exe MUST be a Null-backend build (Null_vs2022_*): headless is a build
# config now, not a flag. A Vulkan exe hangs in vkEnumeratePhysicalDevices on a
# GPU-less runner, and its unit count differs (it also compiles the Vulkan-only
# backend tests, which the Null build has no equivalent of).
# Exit:   0 = baseline met, 1 = anything else (missing exe, timeout, failures).
#
# ★ -TimeoutSec IS NOW A REAL HANG GUARD, NOT THE RUN LENGTH. The exe is launched
# with --exit-after-unit-tests and terminates ITSELF the moment Zenith_Init returns
# (the boot ZENITH_TEST batch lives inside it), so a healthy run exits on its own
# and costs only what the suite actually takes. Raising the timeout no longer costs
# anything on a passing run; it only widens the window before a genuinely wedged
# boot is killed.
#
# It did NOT used to work that way. The exe was launched with
# `--exit-after-frames 120`, which LOOKS like "run 120 frames then quit" but is
# parsed by Zenith_AutomatedTestRunner and consumed only inside its Stepping phase
# -- so with no --automated-test selection flag the runner is inactive, Tick()
# early-outs, and the flag does nothing whatsoever. The game therefore idled
# forever and the watchdog kill was the ONLY thing ending the process, which meant
# every run -- pass or fail -- burned the entire -TimeoutSec. zm-tests.yml passing
# 600 was 10 minutes per run, always.
#
# Two failure modes still surface as "no 'Unit tests complete' line", and neither
# is a slow suite. Check them BEFORE raising the timeout:
#   * an EMPTY log usually means STATUS_DLL_NOT_FOUND (exit 0xC0000135) -- the exe
#     died before main(). Fix with Repair-ZenithRuntimeDlls (Build/zenith_buildsystem.psm1);
#     `zenith test` heals automatically, this script does not.
#   * a truncated log means the boot genuinely wedged. That is what the guard is for.
# See ZM-D-163.
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Exe,
    # Engine unit count at boot. EXACT equality is asserted below, so this must
    # be bumped in the SAME commit as any change to the registered-test set --
    # adding tests reddens the gate exactly as loudly as deleting them.
    # 1271 -> 1284: +13 for Flux/Flux_SwapchainPolicy.Tests.inl (observed
    # 2026-08-04 on a Null_ Combat build).
    # 1284 -> 1289: +5 CommandLine ParseArgs characterization tests, added with
    # the complexity-gate remediation's table-driven rewrite of
    # Zenith_CommandLine::Parse (observed 2026-08-05 on a Null_ Combat build).
    [int]$Baseline = 1289,
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

# Boot far enough for units-at-boot to complete, then exit -- which the exe now
# does BY ITSELF via --exit-after-unit-tests. The watchdog below stays as a guard
# against a genuinely wedged boot; on a healthy run it never fires.
#
# Deliberately NO --skip-tool-exports: the engine unit suite includes asset-export
# tests (ProceduralTree::*, StickFigure*) that reload GenerateTestAssets output from
# disk. Zenith/Assets/ is gitignored, so on a from-scratch CI checkout that output
# only exists if this boot generates it; --skip-tool-exports left those tests loading
# missing assets and wedging the units-at-boot gate. The exports are CPU-only and
# backend-independent. This script is invoked ONLY by engine-gate.yml, so the change is
# scoped here -- the game test gates (cb/dp-tests) keep --skip-tool-exports via their
# own harness (they run --skip-unit-tests, so they need no generated engine assets).
Write-Host "[unit_gate] Booting $Exe (baseline $Baseline, timeout ${TimeoutSec}s)..." -ForegroundColor Cyan
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = (Resolve-Path $Exe).Path
$psi.Arguments = '--exit-after-unit-tests'
$psi.WorkingDirectory = Split-Path (Resolve-Path $Exe).Path
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$proc = [System.Diagnostics.Process]::Start($psi)
$stdout = $proc.StandardOutput.ReadToEndAsync()
$stderr = $proc.StandardError.ReadToEndAsync()
if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
    # A healthy run exits on its own (--exit-after-unit-tests). Reaching here means
    # the boot wedged, or died before main() -- see the DLL note in the header.
    Write-Host "[unit_gate] timeout after ${TimeoutSec}s; killing (boot did not self-terminate)" -ForegroundColor Yellow
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
# "<N> ran, <N-1> passed, 0 failed, 1 skipped".
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
