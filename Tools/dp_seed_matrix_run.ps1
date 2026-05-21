# dp_seed_matrix_run.ps1 -- run all 4 personality playthroughs against
# multiple procgen seeds and collect v3 telemetry artifacts. Used to
# extract per-seed / per-personality insights from the captured data.
#
# Each (seed, personality) cell runs the DP exe once with
# DP_PROCGEN_SEED set in the environment, then copies the resulting
# .ztlm + .json + frames.csv + events.csv into
#   Build/dp_telemetry/seed_matrix/seed_<seed>/<personality>.{ext}
#
# Also generates per-cell PNGs via Tools/dp_telemetry_visualise.ps1.
#
# 2026-05-21: added -Parallelism N. Each parallel worker is a
# Start-Job that runs in its own PowerShell child process. Workers
# set DP_TEST_TMP_PREFIX to a unique value (seed{seed}_{personality})
# so the engine's HPTempPath puts each worker's telemetry artifacts
# in a non-colliding temp filename. The main script waits for batches
# of N workers, scoops their artifacts to the per-cell destination,
# then dispatches the next batch.
#
# Parallelism throughput (measured 2026-05-21 on a 1 seed x 4
# personality smoke test, vs2022_Debug_Win64_False):
#   -Parallelism 1 :  182.6 s  (baseline serial)
#   -Parallelism 2 :   57.7 s  (3.16x speedup)
#   -Parallelism 3 :   45.9 s  (3.98x speedup)
#   -Parallelism 4 :   27.9 s  (6.54x speedup, after --skip-unit-tests fix)
#
# P>=4 history: prior to 2026-05-21, P>=4 reliably hung. The hang
# was traced to the engine's unit-test suite, which runs on every
# boot (before the automated-test handler kicks in) and writes
# fixture files like test_nested_base.zpfb / TestData/round_trip_
# test.zdata into the cwd with hardcoded paths. With 4+ concurrent
# engine boots in the same cwd, two processes would race on a
# fixture file and one would either assert on
# "Reading past end of DataStream" or block on the file lock. The
# fix is to pass --skip-unit-tests to the engine, which is safe
# here because the dp-tests CI step runs unit tests independently.
#
# ASCII-only.

[CmdletBinding()]
param(
    [string]$ConfigName    = "Debug_False",
    [string]$OutRoot       = "Build/dp_telemetry/seed_matrix",
    [int]$ExitAfterFrames  = 8500,
    [switch]$Headless      = $true,
    [uint64[]]$Seeds       = @(0, 12345, 99999),
    # 2026-05-21: how many cells to run concurrently. Defaults to 4,
    # the empirical sweet spot post-fix (6.54x speedup over serial).
    # On a 12-core machine 6-8 is a reasonable next step to try --
    # each devilsplayground.exe is mostly single-threaded gameplay +
    # thread-pool job dispatch, so over-subscription helps the job
    # pool but starves the gameplay thread above ~N=cores/2.
    [int]$Parallelism      = 4
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$exeMap = @{
    "Debug_True"    = "Games/DevilsPlayground/Build/output/win64/vs2022_debug_win64_true/devilsplayground.exe"
    "Debug_False"   = "Games/DevilsPlayground/Build/output/win64/vs2022_debug_win64_false/devilsplayground.exe"
    "Release_False" = "Games/DevilsPlayground/Build/output/win64/vs2022_release_win64_false/devilsplayground.exe"
}
$exe = $exeMap[$ConfigName]
if (-not (Test-Path $exe)) {
    Write-Error "exe not found: $exe"
    exit 1
}
$exeAbs = (Resolve-Path $exe).Path

$personalities = @("Casual","Stealth","Speedrunner","Zealot","Magpie","Relay","Heretic")
$tempDir = $env:TEMP
if (-not $tempDir) { $tempDir = $env:TMP }

# Build the full cell list (seed * personality cross product).
$cells = New-Object System.Collections.Generic.List[hashtable]
foreach ($seed in $Seeds) {
    foreach ($p in $personalities) {
        $cells.Add(@{ Seed = $seed; Personality = $p })
    }
}

Write-Host ""
Write-Host ("Matrix run: {0} cells = {1} seeds x {2} personalities; Parallelism={3}; Config={4}" `
    -f $cells.Count, $Seeds.Count, $personalities.Count, $Parallelism, $ConfigName) `
    -ForegroundColor Magenta

# Ensure per-seed destination directories exist up front so workers
# can write into them without a race.
foreach ($seed in $Seeds) {
    $destDir = Join-Path $OutRoot "seed_$seed"
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
}

# -----------------------------------------------------------------------
# Worker script block. Runs in a Start-Job child PowerShell process.
#
# Inputs: a hashtable with $Seed, $Personality, $Exe, $ExitAfterFrames,
# $Headless, $TempDir, $OutRoot.
#
# Output: a hashtable with the cell metadata + the result-summary fields
# (Frames, Events, Passed, ElapsedSec, ExitCode). The main process
# reads it via Receive-Job.
# -----------------------------------------------------------------------
$worker = {
    # Note: parameter name MUST NOT be $Args -- that's a PowerShell
    # automatic variable that gets shadowed in confusing ways inside
    # Start-Job ScriptBlocks. Using $CellArgs throughout.
    param($CellArgs)
    $Seed             = $CellArgs.Seed
    $Personality      = $CellArgs.Personality
    $Exe              = $CellArgs.Exe
    $ExitAfterFrames  = $CellArgs.ExitAfterFrames
    $Headless         = $CellArgs.Headless
    $TempDir          = $CellArgs.TempDir
    $OutRoot          = $CellArgs.OutRoot

    # 2026-05-21: per-worker temp prefix. The engine's HPTempPath reads
    # DP_TEST_TMP_PREFIX and prepends it to the temp filename, so
    # concurrent workers don't clobber each other's artifacts.
    $prefix = "seed{0}_{1}" -f $Seed, $Personality
    $env:DP_PROCGEN_SEED   = "$Seed"
    $env:DP_TEST_TMP_PREFIX = $prefix

    $testName = "PersonalityPlaythrough_$Personality"
    $destDir  = Join-Path $OutRoot "seed_$Seed"

    # Source paths (per-worker, prefix-namespaced).
    $srcBin    = Join-Path $TempDir ("{0}_dp_personality_{1}_playthrough.ztlm" -f $prefix, $Personality)
    $srcJson   = Join-Path $TempDir ("{0}_dp_personality_{1}_playthrough.json" -f $prefix, $Personality)
    $srcFrames = Join-Path $TempDir ("{0}_dp_personality_{1}_frames.csv" -f $prefix, $Personality)
    $srcEvents = Join-Path $TempDir ("{0}_dp_personality_{1}_events.csv" -f $prefix, $Personality)

    # Wipe the four source files from prior runs so a failed run
    # doesn't accidentally copy stale data.
    Remove-Item $srcBin,$srcJson,$srcFrames,$srcEvents -ErrorAction SilentlyContinue

    # Per-cell results dir for the test-result JSON.
    $resultDir = Join-Path $destDir "_result_$Personality"
    if (Test-Path $resultDir) { Remove-Item -Recurse -Force $resultDir }
    New-Item -ItemType Directory -Path $resultDir -Force | Out-Null
    $resultJson = Join-Path $resultDir "$testName.result.json"

    $startTime = Get-Date
    $procArgs = @(
        "--automated-test", $testName,
        "--exit-after-frames", "$ExitAfterFrames",
        "--fixed-dt", "0.01666",
        "--test-results", $resultJson,
        # 2026-05-21: unit tests run on every engine boot and write
        # fixture files (test_nested_base.zpfb, TestData/round_trip_
        # test.zdata, ~25 others) to the cwd with hardcoded paths.
        # At P>=4, concurrent engine boots race on those fixtures
        # and one of them asserts/hangs on a partially-written file.
        # The dp-tests CI step runs unit tests independently, so
        # skipping them here loses no coverage.
        "--skip-unit-tests"
    )
    if ($Headless) { $procArgs += "--headless" }

    # Run the engine via direct call operator. We use this instead of
    # Start-Process because Start-Process -Wait inside a Start-Job's
    # ScriptBlock has known races (the job runspace can tear down
    # before Start-Process returns, swallowing the exit code). Direct
    # `& $exe @args` blocks the job's pipeline until the exe exits
    # and propagates $LASTEXITCODE cleanly.
    $exeException = ""
    try {
        & $Exe @procArgs 2>&1 | Out-Null
        $testExit = $LASTEXITCODE
    } catch {
        $exeException = $_.ToString()
        $testExit = -999
    }
    $elapsed = (Get-Date) - $startTime

    # Copy artifacts (always, so a failed run leaves diagnosable data).
    $dstBin    = Join-Path $destDir "$Personality.telemetry.ztlm"
    $dstJson   = Join-Path $destDir "$Personality.telemetry.json"
    $dstFrames = Join-Path $destDir "$Personality.frames.csv"
    $dstEvents = Join-Path $destDir "$Personality.events.csv"
    $dstRes    = Join-Path $destDir "$Personality.result.json"

    if (Test-Path $srcBin)    { Copy-Item $srcBin    $dstBin    -Force }
    if (Test-Path $srcJson)   { Copy-Item $srcJson   $dstJson   -Force }
    if (Test-Path $srcFrames) { Copy-Item $srcFrames $dstFrames -Force }
    if (Test-Path $srcEvents) { Copy-Item $srcEvents $dstEvents -Force }
    if (Test-Path $resultJson) { Copy-Item $resultJson $dstRes -Force }

    # Headline numbers for the summary table.
    $frames = 0; $events = 0; $passed = $false
    if (Test-Path $dstJson) {
        try {
            $tlm = (Get-Content $dstJson -Raw) | ConvertFrom-Json
            if ($tlm.frames) { $frames = ($tlm.frames | Measure-Object).Count }
            if ($tlm.events) { $events = ($tlm.events | Measure-Object).Count }
        } catch {}
    }
    if (Test-Path $dstRes) {
        try {
            $res = (Get-Content $dstRes -Raw) | ConvertFrom-Json
            $passed = [bool]$res.passed
        } catch {}
    }

    # Per-cell PNG via the visualiser. Best-effort (parallel-safe;
    # different cells write to different output filenames).
    if (Test-Path $dstJson) {
        $dstPng = Join-Path $destDir "$Personality.png"
        $visScript = Join-Path (Split-Path $Exe -Parent) "..\..\..\..\..\..\Tools\dp_telemetry_visualise.ps1"
        # Fall back to the repo-relative path that the main script can resolve.
        if (-not (Test-Path $visScript)) {
            $visScript = "Tools/dp_telemetry_visualise.ps1"
        }
        if (Test-Path $visScript) {
            try {
                & pwsh.exe -NoProfile -ExecutionPolicy Bypass `
                    -File $visScript `
                    -JsonPath $dstJson `
                    -OutPath $dstPng `
                    -Quiet 2>&1 | Out-Null
            } catch {}
        }
    }

    # Clean up the per-cell _result dir; the .result.json is already
    # copied next to the telemetry artifacts.
    Remove-Item -Recurse -Force $resultDir -ErrorAction SilentlyContinue

    return [pscustomobject]@{
        Seed        = $Seed
        Personality = $Personality
        Passed      = $passed
        Frames      = $frames
        Events      = $events
        ElapsedSec  = [math]::Round($elapsed.TotalSeconds,1)
        ExitCode    = $testExit
    }
}

# -----------------------------------------------------------------------
# Main dispatch loop. Maintains a pool of up to $Parallelism running
# jobs; whenever one completes, scoop its result + start the next
# pending cell.
# -----------------------------------------------------------------------
$summary    = New-Object System.Collections.Generic.List[object]
$running    = New-Object System.Collections.Generic.List[System.Management.Automation.Job]
$cellIndex  = 0
$batchStart = Get-Date

function Start-NextCell {
    param([ref]$Index)
    if ($Index.Value -ge $script:cells.Count) { return $null }
    $cell = $script:cells[$Index.Value]
    $Index.Value = $Index.Value + 1
    # NOT $args -- automatic-variable conflict with Start-Job's
    # arg-passing. Use $cellArgs.
    $cellArgs = @{
        Seed             = $cell.Seed
        Personality      = $cell.Personality
        Exe              = $script:exeAbs
        ExitAfterFrames  = $script:ExitAfterFrames
        Headless         = $script:Headless.IsPresent
        TempDir          = $script:tempDir
        OutRoot          = $script:OutRoot
    }
    $job = Start-Job -ScriptBlock $script:worker -ArgumentList $cellArgs
    Add-Member -InputObject $job -NotePropertyName CellInfo -NotePropertyValue $cell -Force
    Add-Member -InputObject $job -NotePropertyName CellStart -NotePropertyValue (Get-Date) -Force
    return $job
}

# Prime the pool.
while ($running.Count -lt $Parallelism -and $cellIndex -lt $cells.Count) {
    $j = Start-NextCell ([ref]$cellIndex)
    if ($j -ne $null) {
        $cellInfo = $j.CellInfo
        Write-Host ("  [start] seed={0} personality={1} (cell {2}/{3})" `
            -f $cellInfo.Seed, $cellInfo.Personality, $cellIndex, $cells.Count) `
            -ForegroundColor Cyan
        $running.Add($j) | Out-Null
    }
}

# Drain + refill loop.
while ($running.Count -gt 0) {
    $done = Wait-Job -Job $running -Any
    $running.Remove($done) | Out-Null
    $cellInfo = $done.CellInfo
    $elapsed = (Get-Date) - $done.CellStart

    # Receive returns the worker's return value (the summary pscustomobject).
    # Workers don't write any other output (no debug strings), so the
    # return is always either a single pscustomobject or null. Use -Keep
    # so we can inspect ChildJobs[0].Error for crash diagnostics if the
    # return value is missing.
    $allOutput = Receive-Job -Job $done -ErrorAction SilentlyContinue -Keep
    $cellResult = $null
    if ($allOutput -ne $null) {
        foreach ($item in @($allOutput)) {
            if ($item -ne $null -and ($item.PSObject.Properties.Match('Seed').Count -gt 0)) {
                $cellResult = $item
            }
        }
    }
    $childErrors = @()
    if ($done.ChildJobs.Count -gt 0) {
        $childErrors = $done.ChildJobs[0].Error
    }
    Remove-Job -Job $done -Force -ErrorAction SilentlyContinue

    if ($cellResult -ne $null) {
        # Receive-Job returns an array if the worker wrote multiple objects;
        # take the last one (the actual return value).
        if ($cellResult -is [System.Array]) { $cellResult = $cellResult[-1] }
        $summary.Add($cellResult) | Out-Null
        $color = "Yellow"
        if ($cellResult.ExitCode -eq 0) { $color = "Green" }
        Write-Host ("  [done]  seed={0} personality={1}  exit={2} elapsed={3:F1}s  cell-elapsed={4:F1}s" `
            -f $cellResult.Seed, $cellResult.Personality, $cellResult.ExitCode, `
               $cellResult.ElapsedSec, $elapsed.TotalSeconds) `
            -ForegroundColor $color
    } else {
        Write-Host ("  [done]  seed={0} personality={1}  (no result received -- worker crashed?)" `
            -f $cellInfo.Seed, $cellInfo.Personality) -ForegroundColor Red
        if ($childErrors.Count -gt 0) {
            foreach ($e in $childErrors) {
                Write-Host ("    err: " + $e.ToString()) -ForegroundColor Red
            }
        }
        $summary.Add([pscustomobject]@{
            Seed = $cellInfo.Seed; Personality = $cellInfo.Personality;
            Passed = $false; Frames = 0; Events = 0;
            ElapsedSec = [math]::Round($elapsed.TotalSeconds,1); ExitCode = -1
        }) | Out-Null
    }

    # Start the next cell if any remain.
    while ($running.Count -lt $Parallelism -and $cellIndex -lt $cells.Count) {
        $j = Start-NextCell ([ref]$cellIndex)
        if ($j -ne $null) {
            $cellInfo = $j.CellInfo
            Write-Host ("  [start] seed={0} personality={1} (cell {2}/{3})" `
                -f $cellInfo.Seed, $cellInfo.Personality, $cellIndex, $cells.Count) `
                -ForegroundColor Cyan
            $running.Add($j) | Out-Null
        }
    }
}

$batchElapsed = (Get-Date) - $batchStart

Write-Host ""
Write-Host "==================== Summary ====================" -ForegroundColor Magenta
$summary | Sort-Object -Property Seed,Personality | Format-Table -AutoSize | Out-String | Write-Host
Write-Host ("Total wall-clock: {0:F1}s  (cells={1}  parallelism={2})" `
    -f $batchElapsed.TotalSeconds, $summary.Count, $Parallelism) -ForegroundColor Magenta

$summaryJsonPath = Join-Path $OutRoot "matrix_summary.json"
$summary | Sort-Object -Property Seed,Personality | ConvertTo-Json -Depth 4 | Set-Content $summaryJsonPath -Encoding utf8
Write-Host "Wrote summary JSON: $summaryJsonPath"
