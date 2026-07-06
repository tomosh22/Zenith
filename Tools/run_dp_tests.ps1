# run_dp_tests.ps1 -- DevilsPlayground automated-test runner (thin forwarder).
# =============================================================================
# The test engine lives in Tools/ZenithCli/ZenithTestHarness.psm1
# (Invoke-ZenithGameTests) -- ONE shared implementation of discovery, DLL
# self-heal, batch/per-process dispatch, JSON tally, and the timing report,
# shared with run_cb_tests.ps1 and `zenith test`. This forwarder keeps the
# historical DP param surface (CI and docs call it with these exact flags).
#
# Modes (unchanged semantics):
#   default            batch -- ONE process runs every test
#   -PerProcess        one process per test (legacy)
#   -Filter <substr>   forces per-process (engine batch flag has no filter)
#   -FailFast          forces per-process, aborts on first failure
#   -Tier <0..4>       filters the list before dispatch (Test_T0*/Test_P<N>*)
#
# Exit: 0 = all pass, 1 = any failure / setup error. A non-zero ENGINE exit
# fails the batch even if every per-test JSON says passed (crash-mid-suite
# guard). Results land in Build/artifacts/test_results/devilsplayground/ by
# default (gitignored artifact root -- see AGENTS.md).
#
# ASCII-only script body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param(
    [string]$Exe         = "",
    [string]$ResultsDir  = "Build/artifacts/test_results/devilsplayground",
    [string]$Filter      = "",
    # 8500-frame batch ceiling: PersonalityPlaythrough_* tests run 6000-8000
    # frames each; per-test budgets still come from each test's m_iMaxFrames.
    [int]$ExitAfterFrames = 8500,
    [double]$FixedDt     = 0.01666,
    [switch]$Headless,
    [switch]$PerProcess,
    [ValidateRange(0, 4)]
    [Nullable[int]]$Tier = $null,
    [switch]$FailFast,
    [string]$AssertionsLog = ""
)

$ErrorActionPreference = "Stop"

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Import-Module (Join-Path $here 'ZenithCli/ZenithTestHarness.psm1') -Force
Import-Module (Join-Path (Split-Path -Parent $here) 'Build/zenith_buildsystem.psm1') -Force

if ([string]::IsNullOrEmpty($Exe)) {
    $Exe = Get-ZenithGameExePath -Name 'DevilsPlayground' -Config (Get-ZenithDefaultConfig)
}

try {
    $result = Invoke-ZenithGameTests `
        -Exe $Exe -ResultsDir $ResultsDir -Filter $Filter -Tier $Tier `
        -PerProcess:$PerProcess -FailFast:$FailFast -Headless:$Headless `
        -ExitAfterFrames $ExitAfterFrames -FixedDt $FixedDt `
        -AssertionsLog $AssertionsLog -Tag 'run_dp_tests'
}
catch {
    Write-Error "[run_dp_tests] $($_.Exception.Message)"
    exit 1
}

if ($result.Failed -gt 0) { exit 1 }
exit 0
