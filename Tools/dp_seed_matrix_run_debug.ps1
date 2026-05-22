# Debug variant of dp_seed_matrix_run.ps1 -- captures per-cell stdout
# to a log file and tracks engine processes directly (no Start-Job
# layer) so we can post-mortem hangs.
#
# Used 2026-05-21 to investigate the P=4 Stealth hang. Root cause:
# unit tests run on every engine boot and write fixture files with
# hardcoded paths into the cwd. 4 concurrent boots race on those
# files; one process asserts on "Reading past end of DataStream"
# or blocks reading a partially-written file. Production runner
# passes --skip-unit-tests. To re-reproduce the race for regression
# testing, run with -SkipUnitTests:$false.
#
# Usage:
#   # Verify fix is good (should complete cleanly in ~28s):
#   pwsh Tools/dp_seed_matrix_run_debug.ps1 -Parallelism 4
#
#   # Re-reproduce the bug (intermittent -- may need multiple runs):
#   pwsh Tools/dp_seed_matrix_run_debug.ps1 -Parallelism 4 -IncludeUnitTests

[CmdletBinding()]
param(
    [string]$ConfigName     = "Debug_False",
    [string]$OutRoot        = "Build/dp_telemetry/seed_matrix_debug",
    [int]$ExitAfterFrames   = 8500,
    [switch]$Headless       = $true,
    [uint64[]]$Seeds        = @(0),
    [int]$Parallelism       = 4,
    # Cap on wall-clock per cell. Cells exceeding this are killed so a
    # hung Stealth doesn't block the whole batch for 90 minutes.
    [int]$CellTimeoutSec    = 240,
    # Pass -IncludeUnitTests to RE-INCLUDE unit tests on each engine
    # boot, which reproduces the 2026-05-21 P=4 unit-test fixture
    # race (intermittently). The production runner always passes
    # --skip-unit-tests. Switch defaults to off, so by default this
    # debug runner matches production and just completes cleanly.
    [switch]$IncludeUnitTests
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
$exeAbs = (Resolve-Path $exe).Path
$personalities = @("Casual","Stealth","Speedrunner","Zealot")

New-Item -ItemType Directory -Path $OutRoot -Force | Out-Null

$cells = @()
foreach ($seed in $Seeds) {
    foreach ($p in $personalities) {
        $cells += @{ Seed = $seed; Personality = $p }
    }
}

Write-Host ("DEBUG matrix: {0} cells, P={1}, CellTimeout={2}s" -f $cells.Count, $Parallelism, $CellTimeoutSec) -ForegroundColor Magenta
Write-Host ("Output dir: {0}" -f $OutRoot) -ForegroundColor Magenta

function Start-Cell {
    param($Cell)
    $prefix = "seed{0}_{1}" -f $Cell.Seed, $Cell.Personality
    $logPath    = Join-Path $OutRoot ("{0}.log"     -f $prefix)
    $errPath    = Join-Path $OutRoot ("{0}.err.log" -f $prefix)
    $resultJson = Join-Path $OutRoot ("{0}.result.json" -f $prefix)
    Remove-Item $logPath, $errPath, $resultJson -ErrorAction SilentlyContinue

    $testName = "PersonalityPlaythrough_$($Cell.Personality)"
    $procArgs = @(
        "--automated-test", $testName,
        "--exit-after-frames", "$ExitAfterFrames",
        "--fixed-dt", "0.01666",
        "--test-results", $resultJson
    )
    if ($Headless.IsPresent) { $procArgs += "--headless" }
    if (-not $IncludeUnitTests.IsPresent) { $procArgs += "--skip-unit-tests" }

    # Per-cell env vars: seed + temp prefix for parallel isolation.
    $env:DP_PROCGEN_SEED    = "$($Cell.Seed)"
    $env:DP_TEST_TMP_PREFIX = $prefix

    $proc = Start-Process -FilePath $exeAbs -ArgumentList $procArgs `
        -RedirectStandardOutput $logPath `
        -RedirectStandardError  $errPath `
        -NoNewWindow -PassThru

    return [pscustomobject]@{
        Seed        = $Cell.Seed
        Personality = $Cell.Personality
        Proc        = $proc
        Pid         = $proc.Id
        StartTime   = Get-Date
        LogPath     = $logPath
        ErrPath     = $errPath
        ResultJson  = $resultJson
        TimedOut    = $false
    }
}

$running = @()
$summary = @()
$cellIndex = 0
$batchStart = Get-Date

# Prime the pool.
while ($running.Count -lt $Parallelism -and $cellIndex -lt $cells.Count) {
    $c = $cells[$cellIndex]; $cellIndex++
    $entry = Start-Cell $c
    Write-Host ("  [start] seed={0} personality={1} pid={2}" -f $entry.Seed, $entry.Personality, $entry.Pid) -ForegroundColor Cyan
    $running += $entry
}

# Main poll loop.
while ($running.Count -gt 0) {
    Start-Sleep -Milliseconds 1000
    $finished = @()
    foreach ($entry in $running) {
        $elapsed = (Get-Date) - $entry.StartTime
        if ($entry.Proc.HasExited) {
            $finished += $entry
            continue
        }
        if ($elapsed.TotalSeconds -gt $CellTimeoutSec) {
            Write-Host ("  [TIMEOUT] seed={0} personality={1} pid={2} elapsed={3:F0}s -- killing" `
                -f $entry.Seed, $entry.Personality, $entry.Pid, $elapsed.TotalSeconds) -ForegroundColor Red
            try {
                # Snapshot the call stack of the process tree if Sysinternals procdump is on PATH.
                $procdump = Get-Command procdump.exe -ErrorAction SilentlyContinue
                if ($procdump) {
                    $dumpPath = Join-Path $OutRoot ("seed{0}_{1}.dmp" -f $entry.Seed, $entry.Personality)
                    Write-Host ("    procdump -> {0}" -f $dumpPath) -ForegroundColor DarkYellow
                    & procdump.exe -accepteula -ma $entry.Pid $dumpPath 2>&1 | Out-Null
                }
            } catch {}
            try { Stop-Process -Id $entry.Pid -Force -ErrorAction SilentlyContinue } catch {}
            $entry.TimedOut = $true
            $finished += $entry
        }
    }

    foreach ($entry in $finished) {
        $running = @($running | Where-Object { $_.Pid -ne $entry.Pid })
        $elapsed = (Get-Date) - $entry.StartTime
        $exitCode = if ($entry.Proc.HasExited) { $entry.Proc.ExitCode } else { -1 }
        $logTail = if (Test-Path $entry.LogPath) {
            (Get-Content $entry.LogPath -Tail 6 -ErrorAction SilentlyContinue) -join " | "
        } else { "(no log)" }
        $errTail = if ((Test-Path $entry.ErrPath) -and ((Get-Item $entry.ErrPath).Length -gt 0)) {
            (Get-Content $entry.ErrPath -Tail 6 -ErrorAction SilentlyContinue) -join " | "
        } else { "" }
        $status = if ($entry.TimedOut) { "TIMEOUT" } elseif ($exitCode -eq 0) { "OK" } else { "EXIT=$exitCode" }
        $color = if ($entry.TimedOut) { "Red" } elseif ($exitCode -eq 0) { "Green" } else { "Yellow" }
        Write-Host ("  [done]  seed={0} personality={1} wall={2:F1}s status={3} pid={4}" `
            -f $entry.Seed, $entry.Personality, $elapsed.TotalSeconds, $status, $entry.Pid) -ForegroundColor $color
        if ($logTail.Length -gt 0) {
            $trunc = if ($logTail.Length -gt 220) { $logTail.Substring($logTail.Length - 220) } else { $logTail }
            Write-Host ("    out: " + $trunc) -ForegroundColor DarkGray
        }
        if ($errTail.Length -gt 0) {
            $trunc = if ($errTail.Length -gt 220) { $errTail.Substring($errTail.Length - 220) } else { $errTail }
            Write-Host ("    err: " + $trunc) -ForegroundColor DarkMagenta
        }
        $summary += [pscustomobject]@{
            Seed        = $entry.Seed
            Personality = $entry.Personality
            WallSec     = [math]::Round($elapsed.TotalSeconds, 1)
            Status      = $status
            ExitCode    = $exitCode
            LogPath     = $entry.LogPath
        }
    }

    # Refill the pool.
    while ($running.Count -lt $Parallelism -and $cellIndex -lt $cells.Count) {
        $c = $cells[$cellIndex]; $cellIndex++
        $entry = Start-Cell $c
        Write-Host ("  [start] seed={0} personality={1} pid={2}" -f $entry.Seed, $entry.Personality, $entry.Pid) -ForegroundColor Cyan
        $running += $entry
    }
}

$totalElapsed = (Get-Date) - $batchStart
Write-Host ""
Write-Host ("Total: {0:F1}s" -f $totalElapsed.TotalSeconds) -ForegroundColor Magenta
$summary | Format-Table -AutoSize | Out-String | Write-Host

# Persist summary so caller can grep it.
$summary | ConvertTo-Json | Set-Content -Path (Join-Path $OutRoot "summary.json")
