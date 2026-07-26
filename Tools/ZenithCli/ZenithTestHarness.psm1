# ZenithTestHarness.psm1
# =============================================================================
# THE implementation of the Zenith automated-test protocol; `zenith test` is
# its only entry point. Owns: test discovery (+ ANSI strip), the engine flag
# protocol (--all-automated-tests / --automated-test /
# --test-results[-dir] / --skip-*), the pre-run JSON wipe, the
# runtime-DLL self-heal, batch vs per-process mode selection, the per-test
# JSON tally, and the slowest-tests timing report.
#
# Mode selection (merged DP + CB semantics):
#   named-order iff -TestNames is non-empty: ONE process runs exactly those
#     tests, in the given order, via the engine's --automated-tests. This is
#     the predecessor->victim probe used to root-cause cross-test state leaks;
#     duplicates are preserved on purpose ("A,A" probes self-contamination).
#   per-process iff -PerProcess OR -FailFast OR -Filter is non-empty
#     (-Filter: the engine's --all-automated-tests has no name filter;
#      -FailFast: batch mode runs every test regardless of outcomes;
#      a single filtered test therefore runs exactly like CB's old
#      single-test fast path.)
#   batch otherwise: ONE process runs every registered test. -BatchOrder
#     ('reverse' | 'rotate:<N>') reorders that suite without changing its set.
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
    # backend-agnostic (the registry is identical in every config), so callers
    # ALWAYS pass the game's NULL exe: a Vulkan listing boots the renderer /
    # streams the world, which is slow and interleaves log lines -- and on a
    # GPU-less runner it hangs outright in vkEnumeratePhysicalDevices. That is
    # why this stayed `--headless` before headless became a build config; the
    # requirement did not change, only how it is expressed.
    [CmdletBinding()]
    [OutputType([string[]])]
    param([Parameter(Mandatory)][string]$Exe)

    $listArgs = @('--list-automated-tests', '--skip-tool-exports', '--skip-unit-tests')
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
                # An infrastructure skip means the test never RAN. It is neither a
                # pass nor a test failure -- classifying it as either would hide a
                # harness fault behind ordinary-looking results.
                $skipReason = ''
                if ($obj.PSObject.Properties.Name -contains 'skipReason') { $skipReason = "$($obj.skipReason)" }
                if ($skipped -and $skipReason -eq 'infrastructure') {
                    $status = 'INFRA_SKIPPED'
                    $detail = 'not run (harness could not build a clean world)'
                }
                elseif ($obj.passed) {
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
        # Exe used ONLY for the --list-automated-tests discovery step. Must be a
        # Null-backend build (see Get-ZenithRegisteredTests). Defaults to $Exe,
        # which is correct when the run itself is already a Null run.
        [string]$DiscoveryExe = '',
        [Parameter(Mandatory)][string]$ResultsDir,
        [string]$Filter = '',
        [Nullable[int]]$Tier = $null,
        # Ordered, comma-separated test names run in ONE process via the
        # engine's --automated-tests. Duplicates are preserved deliberately
        # ("A,A" is a self-contamination probe). Mutually exclusive with
        # -Filter/-Tier (which select a SET, not an order) and -BatchOrder.
        [string]$TestNames = '',
        # --batch-order spec ('reverse' | 'rotate:<N>') for the full-suite
        # batch run. Only meaningful in batch mode.
        [string]$BatchOrder = '',
        [switch]$PerProcess,
        [switch]$FailFast,
        # Per-batch frame ceiling. 8500 covers the slowest known suite (DP's
        # PersonalityPlaythrough_* run 6000-8000 frames each); per-test budgets
        # are still governed by each test's own m_iMaxFrames, so this is a
        # runaway backstop, not a target -- one value serves every game.
        [int]$ExitAfterFrames = 8500,
        [double]$FixedDt = 0.01666,
        [switch]$NoSkipToolExports,
        [switch]$NoSkipUnitTests,
        [string]$AssertionsLog = '',
        [string]$Tag = 'zenith test'
    )

    # Argument validation first -- pure, so it stays testable without an exe
    # and a typo costs nothing rather than a full engine boot.
    if ($TestNames -ne '') {
        if ($Filter -ne '' -or $null -ne $Tier) { throw "-TestNames states an explicit ORDER; combine it with neither -Filter nor -Tier (those select a set)" }
        if ($PerProcess -or $FailFast) { throw "-TestNames runs one ordered process; it cannot be combined with -PerProcess or -FailFast" }
        if ($BatchOrder -ne '') { throw "-BatchOrder reorders the full suite; it cannot be combined with -TestNames" }
    }
    if ($BatchOrder -ne '') {
        if ($PerProcess -or $FailFast -or $Filter -ne '') { throw "-BatchOrder requires the full-suite batch run (no -PerProcess / -FailFast / -Filter)" }
    }

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

    # Automated-test save sandbox. The engine will NOT delete inside a directory
    # it was merely pointed at -- it requires an ownership marker, and the runner
    # is the only thing that writes one, only under the artifacts root. That is
    # what makes "wipe the .zsave files in here between tests" safe.
    #
    # Per-run-id directories keep concurrent invocations mutually invisible. The
    # engine never removes the directory itself; cleanup below is ours.
    $saveRunId = [guid]::NewGuid().ToString('N').Substring(0, 12)
    $saveRootBase = Join-Path (Get-ZenithRepoRoot) ((Get-ZenithBuildConfigData).ArtifactsRoot + "/savedata")
    $saveRoot = Join-Path $saveRootBase $saveRunId
    New-Item -ItemType Directory -Force -Path $saveRoot | Out-Null
    [System.IO.File]::WriteAllText(
        (Join-Path $saveRoot '.zenith_test_save_root'),
        "ZENITH_TEST_SAVE_ROOT`n$saveRunId`n$((Get-Date).ToString('o'))`n",
        (New-Object System.Text.UTF8Encoding($false)))
    $saveFlags = @('--test-save-root', $saveRoot, '--test-save-run-id', $saveRunId)

    # Age-prune previous runs' sandboxes so the artifacts tree does not grow
    # without bound. Only directories that carry OUR marker are ever removed.
    Get-ChildItem -Path $saveRootBase -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -ne $saveRoot -and (Test-Path (Join-Path $_.FullName '.zenith_test_save_root')) } |
        Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-1) } |
        ForEach-Object { Remove-Item -Recurse -Force $_.FullName -ErrorAction SilentlyContinue }

    # Runtime-DLL self-heal (slang dependency tree + sibling game output dirs).
    $exeDir = Split-Path -Parent (Resolve-Path $Exe).Path
    foreach ($dll in @(Repair-ZenithRuntimeDlls -ExeDir $exeDir)) {
        Write-Host "[$Tag] copied $dll -> exe dir" -ForegroundColor DarkGray
    }

    # Discovery (always, so the list prints before the run and batch results
    # decode into a per-test tally). Runs against the NULL exe -- a Vulkan
    # listing hangs on a GPU-less runner.
    if ([string]::IsNullOrEmpty($DiscoveryExe)) { $DiscoveryExe = $Exe }
    if (-not (Test-Path $DiscoveryExe)) {
        throw "discovery exe not found: $DiscoveryExe -- build the Null config first ('zenith build <Game> --headless')"
    }
    Write-Host "[$Tag] Discovering tests..." -ForegroundColor Cyan
    $tests = @(Get-ZenithRegisteredTests -Exe $DiscoveryExe)

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

    # Resolve -TestNames against the discovered registry. Validating here (as
    # well as engine-side) turns a typo into a fast, explicit failure instead
    # of an engine boot that exits 2 with the reason buried in its log.
    $orderedNames = @()
    if ($TestNames -ne '') {
        $orderedNames = @($TestNames -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' })
        if ($orderedNames.Count -eq 0) { throw "-TestNames was empty after trimming" }
        $unknown = @($orderedNames | Where-Object { $tests -notcontains $_ } | Select-Object -Unique)
        if ($unknown.Count -gt 0) { throw "unknown test name(s): $($unknown -join ', ')" }
    }

    # Common engine flags. Tool exports + unit tests are skipped by default for
    # speed; -NoSkip* flips them back on for phases that need them.
    $commonFlags = @()
    if (-not $NoSkipToolExports) { $commonFlags += '--skip-tool-exports' }
    if (-not $NoSkipUnitTests) { $commonFlags += '--skip-unit-tests' }

    function Add-AssertionLogEntry {
        param([string]$Name, [string]$JsonPath, [string]$Extra)
        if ($AssertionsLog -eq '') { return }
        $stamp = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')
        Add-Content -Path $AssertionsLog -Value "$stamp  FAIL  $Name  json=$JsonPath  $Extra"
    }

    $useNamed = ($TestNames -ne '')
    $useBatch = (-not $useNamed) -and (-not $PerProcess) -and (-not $FailFast) -and ($Filter -eq '')

    $passed = 0
    $failedNames = New-Object System.Collections.Generic.List[string]
    $engineExit = 0
    # Which names the tally / timing report cover. Batch = everything
    # discovered; -TestNames = the named set, deduped (repeated occurrences
    # overwrite the same <name>.json, so tallying them twice would
    # double-count one result).
    $reportTests = $tests

    if ($useNamed -or $useBatch) {
        Write-Host ""
        $tailArgs = @(
            '--exit-after-frames', $ExitAfterFrames,
            '--fixed-dt', $FixedDt,
            '--test-results-dir', $ResultsDir
        ) + $saveFlags + $commonFlags

        if ($useNamed) {
            Write-Host "[$Tag] Running $($orderedNames.Count) named test(s) in one process, in order: $($orderedNames -join ' -> ')" -ForegroundColor Yellow
            $runArgs = @('--automated-tests', ($orderedNames -join ',')) + $tailArgs
            $reportTests = @($orderedNames | Select-Object -Unique)
        }
        else {
            $orderTag = if ($BatchOrder -ne '') { " [order: $BatchOrder]" } else { '' }
            Write-Host "[$Tag] Running all tests in a single process (batch mode)$orderTag..." -ForegroundColor Yellow
            $runArgs = @('--all-automated-tests') + $tailArgs
            if ($BatchOrder -ne '') { $runArgs += @('--batch-order', $BatchOrder) }
        }

        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        # Tee output for post-mortem on non-zero exits; Out-Host streams it to
        # the console live (CI surfaces engine assertions / crashes as they
        # happen) AND keeps it off this function's pipeline -- without it, the
        # whole engine log would leak into the caller's return value.
        & $Exe @runArgs 2>&1 | Tee-Object -Variable runOutput | Out-Host
        $engineExit = $LASTEXITCODE
        $stopwatch.Stop()
        $runLabel = if ($useNamed) { 'Named run' } else { 'Batch run' }
        Write-Host "[$Tag] $runLabel finished in $([int]$stopwatch.Elapsed.TotalSeconds)s (exit=$engineExit)" -ForegroundColor Cyan

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

        # Infrastructure failure: the harness could not build a clean world, so
        # the remaining tests were never RUN. Report that once, and suppress the
        # per-test noise + the synthetic batch-exit marker that would otherwise
        # bury the actual cause under a wall of red.
        $infraPath = Join-Path $ResultsDir '_infrastructure.json'
        $infra = $null
        if (Test-Path -LiteralPath $infraPath) {
            try { $infra = Get-Content -LiteralPath $infraPath -Raw | ConvertFrom-Json } catch { }
        }
        if ($null -ne $infra) {
            Write-Host ""
            Write-Host "[$Tag] INFRASTRUCTURE FAILURE in $($infra.phase): $($infra.reason)" -ForegroundColor Red
            Write-Host "[$Tag]   before test: $($infra.beforeTest)" -ForegroundColor Red
            Write-Host "[$Tag]   Tests after that point were NOT RUN -- this is a harness fault, not a test failure." -ForegroundColor Red
            $failedNames.Clear()
            $failedNames.Add("<infrastructure:$($infra.reason)>")
            Add-AssertionLogEntry -Name '<infrastructure>' -JsonPath $infraPath -Extra "$($infra.reason) before $($infra.beforeTest)"
        }

        $tally = Read-ZenithTestResults -ResultsDir $ResultsDir -Tests $reportTests
        $passed = $tally.Passed
        if ($null -ne $infra) {
            # Everything downstream of the fault is unrun; only the pre-fault
            # results are meaningful, and the run has already been marked failed.
            Write-Host "[$Tag] $passed test(s) completed before the fault." -ForegroundColor Yellow
            $timings = @(Get-ZenithTestTimings -ResultsDir $ResultsDir -Tests $reportTests)
            return [PSCustomObject]@{
                Passed      = $passed
                Failed      = $failedNames.Count
                FailedNames = $failedNames.ToArray()
                EngineExit  = $engineExit
                Tests       = $reportTests
            }
        }
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
                'INFRA_SKIPPED' {
                    Write-Host "    NOT RUN $($e.Name) (infrastructure fault)" -ForegroundColor Yellow
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
            ) + $saveFlags + $commonFlags
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

    $timings = @(Get-ZenithTestTimings -ResultsDir $ResultsDir -Tests $reportTests)
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

    # Sandbox cleanup: drop it on success, KEEP it on failure so a failing save
    # test's actual .zsave files are available for triage. Stale keeps are
    # age-pruned at the start of the next run.
    if ($failedNames.Count -eq 0) {
        Remove-Item -Recurse -Force $saveRoot -ErrorAction SilentlyContinue
    }
    else {
        Write-Host "[$Tag] Save sandbox kept for triage: $saveRoot" -ForegroundColor DarkGray
    }

    return [PSCustomObject]@{
        Passed      = $passed
        Failed      = $failedNames.Count
        FailedNames = $failedNames.ToArray()
        EngineExit  = $engineExit
        Tests       = $reportTests
    }
}

Export-ModuleMember -Function @(
    'ConvertFrom-ZenithTestListOutput',
    'Get-ZenithRegisteredTests',
    'Read-ZenithTestResults',
    'Get-ZenithTestTimings',
    'Invoke-ZenithGameTests'
)
