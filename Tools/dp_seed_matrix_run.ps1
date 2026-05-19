# dp_seed_matrix_run.ps1 -- run all 5 personality playthroughs against
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
# ASCII-only.

[CmdletBinding()]
param(
    [string]$ConfigName    = "Debug_False",
    [string]$OutRoot       = "Build/dp_telemetry/seed_matrix",
    [int]$ExitAfterFrames  = 8500,
    [switch]$Headless      = $true,
    [uint64[]]$Seeds       = @(0, 12345, 99999)
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

$personalities = @("Casual","Stealth","Speedrunner","Berserker","Methodical")
$tempDir = $env:TEMP
if (-not $tempDir) { $tempDir = $env:TMP }

$summary = @()

foreach ($seed in $Seeds) {
    $destDir = Join-Path $OutRoot "seed_$seed"
    New-Item -ItemType Directory -Path $destDir -Force | Out-Null
    Write-Host ""
    Write-Host "==================== SEED $seed ====================" -ForegroundColor Magenta

    foreach ($p in $personalities) {
        $testName = "PersonalityPlaythrough_$p"
        Write-Host ""
        Write-Host "==== seed=$seed  $testName ====" -ForegroundColor Cyan

        # Wipe %TEMP% telemetry from prior runs so we don't accidentally
        # copy stale artifacts on a failed run.
        $srcBin    = Join-Path $tempDir "dp_personality_${p}_playthrough.ztlm"
        $srcJson   = Join-Path $tempDir "dp_personality_${p}_playthrough.json"
        $srcFrames = Join-Path $tempDir "dp_personality_${p}_frames.csv"
        $srcEvents = Join-Path $tempDir "dp_personality_${p}_events.csv"
        Remove-Item $srcBin,$srcJson,$srcFrames,$srcEvents -ErrorAction SilentlyContinue

        # Per-test result JSON. Use a scratch dir per cell so we don't
        # contaminate Build/dp_test_results across cells.
        $resultDir = Join-Path $destDir "_result_$p"
        if (Test-Path $resultDir) { Remove-Item -Recurse -Force $resultDir }
        New-Item -ItemType Directory -Path $resultDir -Force | Out-Null
        $resultJson = Join-Path $resultDir "$testName.result.json"

        # Set the seed env var for THIS process invocation only -- restore
        # the prior value afterwards so the outer shell's env stays clean.
        $oldSeedEnv = $env:DP_PROCGEN_SEED
        $env:DP_PROCGEN_SEED = "$seed"

        $startTime = Get-Date
        $procArgs = @(
            "--automated-test", $testName,
            "--exit-after-frames", "$ExitAfterFrames",
            "--fixed-dt", "0.01666",
            "--test-results", $resultJson
        )
        if ($Headless) { $procArgs += "--headless" }
        & $exe @procArgs 2>&1 | Out-Null
        $testExit = $LASTEXITCODE
        $elapsed = (Get-Date) - $startTime

        $env:DP_PROCGEN_SEED = $oldSeedEnv

        Write-Host ("  exit={0}  elapsed={1:F1}s" -f $testExit, $elapsed.TotalSeconds)

        # Copy artifacts (always, so a failed run leaves diagnosable data).
        $dstBin    = Join-Path $destDir "$p.telemetry.ztlm"
        $dstJson   = Join-Path $destDir "$p.telemetry.json"
        $dstFrames = Join-Path $destDir "$p.frames.csv"
        $dstEvents = Join-Path $destDir "$p.events.csv"
        $dstRes    = Join-Path $destDir "$p.result.json"

        if (Test-Path $srcBin)    { Copy-Item $srcBin    $dstBin    -Force }
        if (Test-Path $srcJson)   { Copy-Item $srcJson   $dstJson   -Force }
        if (Test-Path $srcFrames) { Copy-Item $srcFrames $dstFrames -Force }
        if (Test-Path $srcEvents) { Copy-Item $srcEvents $dstEvents -Force }
        if (Test-Path $resultJson) { Copy-Item $resultJson $dstRes -Force }

        # PNG (best-effort).
        if (Test-Path $dstJson) {
            $dstPng = Join-Path $destDir "$p.png"
            & pwsh.exe -NoProfile -ExecutionPolicy Bypass `
                -File Tools/dp_telemetry_visualise.ps1 `
                -JsonPath $dstJson `
                -OutPath $dstPng `
                -Quiet 2>&1 | Out-Null
        }

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

        $summary += [pscustomobject]@{
            Seed        = $seed
            Personality = $p
            Passed      = $passed
            Frames      = $frames
            Events      = $events
            ElapsedSec  = [math]::Round($elapsed.TotalSeconds,1)
            ExitCode    = $testExit
        }

        # Clean up the per-cell _result dir; the .result.json is already
        # copied next to the telemetry artifacts.
        Remove-Item -Recurse -Force $resultDir -ErrorAction SilentlyContinue
    }
}

Write-Host ""
Write-Host "==================== Summary ====================" -ForegroundColor Magenta
$summary | Format-Table -AutoSize | Out-String | Write-Host

$summaryJsonPath = Join-Path $OutRoot "matrix_summary.json"
$summary | ConvertTo-Json -Depth 4 | Set-Content $summaryJsonPath -Encoding utf8
Write-Host "Wrote summary JSON: $summaryJsonPath"
