# Test_T0Harness_RunnerFlagsExist.ps1
#
# Smoke test for Tools/run_dp_tests.ps1 -- proves the script parses under
# Windows PowerShell 5.1 AND declares the MVP-0.0.4 flag set (-Tier,
# -FailFast, -AssertionsLog).
#
# Usage:
#   powershell -NoProfile -File Tools/Test_T0Harness_RunnerFlagsExist.ps1
#   pwsh -NoProfile -File Tools/Test_T0Harness_RunnerFlagsExist.ps1
#
# Exit codes:
#   0 -- runner script parses cleanly AND all expected flags are declared.
#   1 -- one or more expected flags missing, OR the runner failed to parse.
#
# Why this approach: invoking `run_dp_tests.ps1` for-real would either spin
# up the whole engine (slow + needs GPU + needs DLLs) or exit 1 because no
# tests matched a synthetic filter. The static parse-check approach via
# Get-Command does load the script's param block (so a parser-level error
# WOULD fail this test), but doesn't run the body. That's the right shape
# for a flag-existence smoke test that has to be invokable in any
# environment, including the no-GPU CI runner that blocks the broader
# dp-tests workflow today.
#
# ASCII-only script body so Windows PowerShell 5.1 (default codepage CP1252)
# can read it without UTF-8 mojibake. See Q-2026-05-12-005 history.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$runner = Join-Path $PSScriptRoot 'run_dp_tests.ps1'
if (-not (Test-Path $runner)) {
    Write-Error "Runner not found at $runner"
    exit 1
}

try {
    $cmd = Get-Command $runner -ErrorAction Stop
} catch {
    Write-Error "Failed to parse $runner -- $($_.Exception.Message)"
    exit 1
}

$expected = @('Tier', 'FailFast', 'AssertionsLog')
$missing = @()
$present = @()
foreach ($flag in $expected) {
    if ($cmd.Parameters.ContainsKey($flag)) {
        $param = $cmd.Parameters[$flag]
        $present += "$flag ($($param.ParameterType.Name))"
    } else {
        $missing += $flag
    }
}

Write-Host "Test_T0Harness_RunnerFlagsExist"
Write-Host "  runner: $runner"
foreach ($p in $present) {
    Write-Host "  [OK]   -$p"
}
foreach ($m in $missing) {
    Write-Host "  [FAIL] -$m  (not declared on param block)" -ForegroundColor Red
}

if ($missing.Count -gt 0) {
    Write-Host ""
    Write-Host "FAIL: $($missing.Count) expected flag(s) missing: $($missing -join ', ')" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "PASS: all 3 MVP-0.0.4 flags present" -ForegroundColor Green
exit 0
