# run_cb_tests.ps1 -- CityBuilder automated-test runner (thin forwarder).
# =============================================================================
# The test engine lives in Tools/ZenithCli/ZenithTestHarness.psm1
# (Invoke-ZenithGameTests) -- ONE shared implementation of discovery, DLL
# self-heal, batch/per-process dispatch, JSON tally, and the timing report,
# shared with run_dp_tests.ps1 and `zenith test`. This forwarder keeps the
# historical CB param surface (CI calls it with these exact flags).
#
# Modes: default = batch (one process, every test); -Filter forces per-process
# (a single filtered test therefore runs exactly like the old single-test fast
# path). Tool exports + unit tests are skipped by default for speed; flip via
# -NoSkip* when a phase needs them.
#
# Exit: 0 = all pass, 1 = any failure / setup error. Results land in
# Build/artifacts/test_results/citybuilder/ by default (gitignored artifact
# root -- see AGENTS.md).
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param(
    [string]$Exe            = "",
    [string]$ResultsDir     = "Build/artifacts/test_results/citybuilder",
    [string]$Filter         = "",
    [int]$ExitAfterFrames   = 6000,
    [double]$FixedDt        = 0.01666,
    [switch]$Headless,
    [switch]$NoSkipToolExports,
    [switch]$NoSkipUnitTests
)

$ErrorActionPreference = "Stop"

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Import-Module (Join-Path $here 'ZenithCli/ZenithTestHarness.psm1') -Force
Import-Module (Join-Path (Split-Path -Parent $here) 'Build/zenith_buildsystem.psm1') -Force

if ([string]::IsNullOrEmpty($Exe)) {
    $Exe = Get-ZenithGameExePath -Name 'CityBuilder' -Config (Get-ZenithDefaultConfig)
}

try {
    $result = Invoke-ZenithGameTests `
        -Exe $Exe -ResultsDir $ResultsDir -Filter $Filter `
        -Headless:$Headless -ExitAfterFrames $ExitAfterFrames -FixedDt $FixedDt `
        -NoSkipToolExports:$NoSkipToolExports -NoSkipUnitTests:$NoSkipUnitTests `
        -Tag 'run_cb_tests'
}
catch {
    Write-Error "[run_cb_tests] $($_.Exception.Message)"
    exit 1
}

if ($result.Failed -gt 0) { exit 1 }
exit 0
