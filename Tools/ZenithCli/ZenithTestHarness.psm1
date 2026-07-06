# ZenithTestHarness.psm1
# =============================================================================
# ONE implementation of the Zenith automated-test protocol, shared by
# `zenith test`, Tools/run_dp_tests.ps1, and Tools/run_cb_tests.ps1 (both now
# thin forwarders). Owns: headless test discovery (+ ANSI strip), the engine
# flag protocol (--all-automated-tests / --automated-test / --test-results[-dir]
# / --skip-* / --headless), the pre-run JSON wipe, the runtime-DLL self-heal,
# batch vs per-process mode selection, the per-test JSON tally, and the
# slowest-tests timing report.
#
# Mode selection (merged DP + CB semantics):
#   per-process iff -PerProcess OR -FailFast OR -Filter is non-empty
#     (-Filter: the engine's --all-automated-tests has no name filter;
#      -FailFast: batch mode runs every test regardless of outcomes;
#      a single filtered test therefore runs exactly like CB's old
#      single-test fast path.)
#   batch otherwise: ONE process runs every registered test.
#   -Tier filters the list BEFORE dispatch, so it does not force per-process.
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Shared build-system helpers (Repair-ZenithRuntimeDlls, Get-ZenithRepoRoot).
# -Global: a plain -Force re-import here would DISPLACE a global import done by
# the CLI/forwarders into this module's private scope, breaking their calls.
Import-Module (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'Build/zenith_buildsystem.psm1') -Force -Global

function ConvertFrom-ZenithTestListOutput {
    # Pure parser for `--list-automated-tests` output. Strips ANSI colour codes
    # (a windowed engine interleaves coloured log lines that corrupt the name
    # parse) and walks the "Registered automated tests:" block: indented
    # single-token lines are test names; the first non-indented line ends it.
    [CmdletBinding()]
    [OutputType([string[]])]
    param([object[]]$Lines)

    $esc = [char]27
    $tests = New-Object System.Collections.Generic.List[string]
    $inList = $false
    foreach ($line in @($Lines)) {
        $s = ("$line" -replace "$esc\[[0-9;]*m", "")
        if ($s -match "^Registered automated tests:") { $inList = $true; continue }
        if ($inList) {
            if ($s -match "^\s+(\S+)\s*$") { $tests.Add($matches[1]) }
            elseif ($s -notmatch "^\s") { $inList = $false }
        }
    }
    return $tests.ToArray()
}

function Get-ZenithRegisteredTests {
    # Discover the registered automated tests of a game exe. Discovery is
    # mode-agnostic (the registry is identical headless or windowed), so ALWAYS
    # list headless: a windowed listing boots the renderer / streams the world,
    # which is slow and interleaves log lines -- and on GPU-less CI runners a
    # non-headless listing hangs in vkEnumeratePhysicalDevices.
    [CmdletBinding()]
    [OutputType([string[]])]
    param([Parameter(Mandatory)][string]$Exe)

    $listArgs = @('--list-automated-tests', '--skip-tool-exports', '--skip-unit-tests', '--headless')
    $listOutput = & $Exe @listArgs 2>&1
    return (ConvertFrom-ZenithTestListOutput -Lines $listOutput)
}

function Read-ZenithTestResults {
    # Tally per-test JSON results written by the engine. Returns
    # @{ Passed; FailedNames; Entries } where each entry is
    # @{ Name; Status (PASS|FAIL|MISSING|UNPARSEABLE); Skipped; JsonPath; Detail }.
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ResultsDir,
        [Parameter(Mandatory)][string[]]$Tests
    )

    $passed = 0
    $failedNames = New-Object System.Collections.Generic.List[string]
    $entries = New-Object System.Collections.Generic.List[object]

    foreach ($name in $Tests) {
        $jsonPath = Join-Path $ResultsDir "$name.json"
        $status = ''
        $skipped = $false
        $detail = ''
        if (-not (Test-Path -LiteralPath $jsonPath)) {
            $status = 'MISSING'
            $failedNames.Add($name)
        }
        else {
            $obj = $null
            try { $obj = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json } catch { }
            if ($null -eq $obj) {
                $status = 'UNPARSEABLE'
                $failedNames.Add($name)
            }
            else {
                if ($obj.PSObject.Properties.Name -contains 'skipped' -and $obj.skipped) { $skipped = $true }
                if ($obj.passed) {
                    $status = 'PASS'
                    $passed++
                }
                else {
                    $status = 'FAIL'
                    $failedNames.Add($name)
                    if ($obj.PSObject.Properties.Name -contains 'failures' -and $obj.failures) {
                        $frames = if ($obj.PSObject.Properties.Name -contains 'frames') { $obj.frames } else { '?' }
                        $detail = "failures=$(@($obj.failures).Count) frames=$frames"
                    }
                }
            }
        }
        $entries.Add([PSCustomObject]@{ Name = $name; Status = $status; Skipped = $skipped; JsonPath = $jsonPath; Detail = $detail })
    }

    return [PSCustomObject]@{
        Passed      = $passed
        FailedNames = $failedNames.ToArray()
        Entries     = $entries.ToArray()
    }
}

function Get-ZenithTestTimings {
    # Roll up "durationMs" from each per-test JSON (graceful when a JSON lacks
    # the field -- pre-timing-feature result files).
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ResultsDir,
        [Parameter(Mandatory)][string[]]$Tests
    )
    $timings = New-Object System.Collections.Generic.List[object]
    foreach ($name in $Tests) {
        $jsonPath = Join-Path $ResultsDir "$name.json"
        if (-not (Test-Path -LiteralPath $jsonPath)) { continue }
        $obj = $null
        try { $obj = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json } catch { continue }
        if ($null -eq $obj) { continue }
        if ($obj.PSObject.Properties.Name -contains 'durationMs') {
            $timings.Add([PSCustomObject]@{
                Name       = $name
                DurationMs = [double]$obj.durationMs
                Frames     = if ($obj.PSObject.Properties.Name -contains 'frames') { [int]$obj.frames } else { 0 }
                Skipped    = if ($obj.PSObject.Properties.Name -contains 'skipped') { [bool]$obj.skipped } else { $false }
            })
        }
    }
    return $timings.ToArray()
}

function Invoke-ZenithGameTests {
    # Run a game's automated tests end-to-end. Throws on setup problems (exe
    # missing, zero tests discovered); returns
    # @{ Passed; Failed; FailedNames; EngineExit; Tests } otherwise.
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Exe,
        [Parameter(Mandatory)][string]$ResultsDir,
        [string]$Filter = '',
        [Nullable[int]]$Tier = $null,
        [switch]$PerProcess,
        [switch]$FailFast,
        [switch]$Headless,
        [int]$ExitAfterFrames = 6000,
        [double]$FixedDt = 0.01666,
        [switch]$NoSkipToolExports,
        [switch]$NoSkipUnitTests,
        [string]$AssertionsLog = '',
        [string]$Tag = 'zenith test'
    )

    if (-not (Test-Path $Exe)) {
        throw "executable not found: $Exe (build the game first)"
    }

    if (-not (Test-Path $ResultsDir)) {
        New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null
    }

    # Wipe stale per-test JSON BEFORE invoking the engine: renamed/removed tests
    # or a crash mid-batch must not leave orphan passes that mask a regression.
    # Only *.json directly inside $ResultsDir -- never unrelated artifacts.
    Get-ChildItem -Path $ResultsDir -Filter '*.json' -File -ErrorAction SilentlyContinue |
        ForEach-Object { Remove-Item $_.FullName -Force }

    # Runtime-DLL self-heal (slang dependency tree + sibling game output dirs).
    $exeDir = Split-Path -Parent (Resolve-Path $Exe).Path
    foreach ($dll in @(Repair-ZenithRuntimeDlls -ExeDir $exeDir)) {
        Write-Host "[$Tag] copied $dll -> exe dir" -ForegroundColor DarkGray
    }

    # Discovery (always, so the list prints before the run and batch results
    # decode into a per-test tally).
    Write-Host "[$Tag] Discovering tests..." -ForegroundColor Cyan
    $tests = @(Get-ZenithRegisteredTests -Exe $Exe)

    # @(...) is load-bearing on every reassignment: a filter matching exactly
    # one test would otherwise unwrap to a scalar string.
    if ($Filter -ne '') { $tests = @($tests | Where-Object { $_ -like "*$Filter*" }) }
    if ($null -ne $Tier) {
        # Tier 0 is harness-sanity (Test_T0*); Tier N>=1 is Test_P<N>* per the
        # DP TestPlan.md naming convention.
        $tierPrefix = if ($Tier -eq 0) { 'Test_T0' } else { "Test_P$Tier" }
        $before = $tests.Count
        $tests = @($tests | Where-Object { $_ -like "$tierPrefix*" })
        Write-Host "[$Tag] -Tier $Tier filter: $before -> $($tests.Count) test(s) (prefix $tierPrefix)" -ForegroundColor Cyan
    }

    if ($tests.Count -eq 0) {
        throw "no tests discovered (Filter='$Filter', Tier=$Tier)"
    }

    Write-Host "[$Tag] Found $($tests.Count) test(s):" -ForegroundColor Cyan
    $tests | ForEach-Object { Write-Host "    $_" }

    # Common engine flags. Tool exports + unit tests are skipped by default for
    # speed; -NoSkip* flips them back on for phases that need them.
    $commonFlags = @()
    if (-not $NoSkipToolExports) { $commonFlags += '--skip-tool-exports' }
    if (-not $NoSkipUnitTests) { $commonFlags += '--skip-unit-tests' }
    if ($Headless) { $commonFlags += '--headless' }

    function Add-AssertionLogEntry {
        param([string]$Name, [string]$JsonPath, [string]$Extra)
        if ($AssertionsLog -eq '') { return }
        $stamp = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
        Add-Content -Path $AssertionsLog -Value "$stamp  FAIL  $Name  json=$JsonPath  $Extra"
    }

    $useBatch = (-not $PerProcess) -and (-not $FailFast) -and ($Filter -eq '')

    $passed = 0
    $failedNames = New-Object System.Collections.Generic.List[string]
    $engineExit = 0

    if ($useBatch) {
        Write-Host ""
        Write-Host "[$Tag] Running all tests in a single process (batch mode)..." -ForegroundColor Yellow
        $runArgs = @(
            '--all-automated-tests',
            '--exit-after-frames', $ExitAfterFrames,
            '--fixed-dt', $FixedDt,
            '--test-results-dir', $ResultsDir
        ) + $commonFlags

        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        # Tee output for post-mortem on non-zero exits; direct console
        # pass-through so CI surfaces engine assertions / crash output live.
        & $Exe @runArgs 2>&1 | Tee-Object -Variable runOutput
        $engineExit = $LASTEXITCODE
        $stopwatch.Stop()
        Write-Host "[$Tag] Batch run finished in $([int]$stopwatch.Elapsed.TotalSeconds)s (exit=$engineExit)" -ForegroundColor Cyan

        if ($engineExit -ne 0) {
            Write-Host "[$Tag] Last 80 lines of engine output:" -ForegroundColor Yellow
            $runOutput | Select-Object -Last 80 | ForEach-Object { Write-Host "    $_" }
            # Propagate a non-zero engine exit as a hard failure even if every
            # JSON says passed=true: combined with the pre-run wipe, this stops
            # a partial batch (crashed mid-suite) from masking as success.
            Write-Host "[$Tag] Engine exited non-zero -- flagging batch as failed even if individual JSONs pass." -ForegroundColor Red
            $failedNames.Add("<batch:exit=$engineExit>")
            Add-AssertionLogEntry -Name '<batch>' -JsonPath '' -Extra "engine exited $engineExit"
        }

        $tally = Read-ZenithTestResults -ResultsDir $ResultsDir -Tests $tests
        $passed = $tally.Passed
        foreach ($e in $tally.Entries) {
            switch ($e.Status) {
                'PASS' {
                    $skipTag = if ($e.Skipped) { ' (skipped)' } else { '' }
                    Write-Host "    PASS $($e.Name)$skipTag" -ForegroundColor Green
                }
                'FAIL' {
                    Write-Host "    FAIL $($e.Name) ($($e.JsonPath))" -ForegroundColor Red
                    Add-AssertionLogEntry -Name $e.Name -JsonPath $e.JsonPath -Extra $e.Detail
                }
                'MISSING' {
                    Write-Host "    MISSING $($e.Name) (no JSON written)" -ForegroundColor Red
                    Add-AssertionLogEntry -Name $e.Name -JsonPath $e.JsonPath -Extra 'JSON missing'
                }
                'UNPARSEABLE' {
                    Write-Host "    UNPARSEABLE $($e.Name) ($($e.JsonPath))" -ForegroundColor Red
                    Add-AssertionLogEntry -Name $e.Name -JsonPath $e.JsonPath -Extra 'json unparseable'
                }
            }
        }
        foreach ($n in $tally.FailedNames) { $failedNames.Add($n) }
    }
    else {
        foreach ($name in $tests) {
            $jsonPath = Join-Path $ResultsDir "$name.json"
            Write-Host ""
            Write-Host "[$Tag] Running $name..." -ForegroundColor Yellow
            $runArgs = @(
                '--automated-test', $name,
                '--exit-after-frames', $ExitAfterFrames,
                '--fixed-dt', $FixedDt,
                '--test-results', $jsonPath
            ) + $commonFlags
            & $Exe @runArgs 2>&1 | Out-Null
            $code = $LASTEXITCODE
            if ($code -eq 0) {
                Write-Host "    PASS ($jsonPath)" -ForegroundColor Green
                $passed++
            }
            else {
                Write-Host "    FAIL exit=$code ($jsonPath)" -ForegroundColor Red
                $failedNames.Add($name)
                Add-AssertionLogEntry -Name $name -JsonPath $jsonPath -Extra "exit=$code"
                if ($FailFast) {
                    Write-Host "[$Tag] -FailFast: aborting after first failure ($name)" -ForegroundColor Red
                    break
                }
            }
        }
    }

    # Summary + slowest-tests report.
    Write-Host ""
    Write-Host "[$Tag] Summary: $passed passed, $($failedNames.Count) failed" -ForegroundColor Cyan

    $timings = @(Get-ZenithTestTimings -ResultsDir $ResultsDir -Tests $tests)
    if ($timings.Count -gt 0) {
        $totalMs = ($timings | Measure-Object -Property DurationMs -Sum).Sum
        $ran = @($timings | Where-Object { -not $_.Skipped })
        $avgMs = if ($ran.Count -gt 0) { ($ran | Measure-Object -Property DurationMs -Average).Average } else { 0 }
        Write-Host ""
        Write-Host ("[$Tag] Timing: {0} tests measured, total = {1:N0} ms, avg = {2:N1} ms" -f $timings.Count, $totalMs, $avgMs) -ForegroundColor Cyan
        $topN = [Math]::Min(10, $timings.Count)
        Write-Host "[$Tag] Slowest $topN tests:" -ForegroundColor Cyan
        $timings |
            Sort-Object -Property DurationMs -Descending |
            Select-Object -First $topN |
            ForEach-Object {
                $skipTag = if ($_.Skipped) { ' (skipped)' } else { '' }
                Write-Host ("    {0,7:N1} ms  {1,6} frames  {2}{3}" -f $_.DurationMs, $_.Frames, $_.Name, $skipTag)
            }
    }

    if ($failedNames.Count -gt 0) {
        Write-Host "Failed tests:" -ForegroundColor Red
        $failedNames | ForEach-Object { Write-Host "    $_" }
        if ($AssertionsLog -ne '' -and (Test-Path $AssertionsLog)) {
            Write-Host "Assertion log appended to: $AssertionsLog" -ForegroundColor Yellow
        }
    }

    return [PSCustomObject]@{
        Passed      = $passed
        Failed      = $failedNames.Count
        FailedNames = $failedNames.ToArray()
        EngineExit  = $engineExit
        Tests       = $tests
    }
}

Export-ModuleMember -Function @(
    'ConvertFrom-ZenithTestListOutput',
    'Get-ZenithRegisteredTests',
    'Read-ZenithTestResults',
    'Get-ZenithTestTimings',
    'Invoke-ZenithGameTests'
)
