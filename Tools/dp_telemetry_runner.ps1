# dp_telemetry_runner.ps1 -- run the DP heuristic-bot playthrough,
# collect the resulting telemetry artifacts from %TEMP%, and print a
# summary the developer / CI can grep.
#
# Phase 5 of the telemetry / verification system (2026-05-16).
#
# Usage (from repo root):
#   pwsh ./Tools/dp_telemetry_runner.ps1
#   pwsh ./Tools/dp_telemetry_runner.ps1 -OutDir Build/artifacts/telemetry
#   pwsh ./Tools/dp_telemetry_runner.ps1 -Build       # build DP first
#   pwsh ./Tools/dp_telemetry_runner.ps1 -SkipRun -OutDir my_dir
#                                                       # just summarize existing files
#
# The bot test (Test_DPHeuristicBotPlaythrough) writes telemetry to the
# system temp directory (so the test itself is portable across CI vs
# dev machines). This script wraps the test invocation, copies the
# resulting .ztlm + .json out of %TEMP% into a stable, version-able
# location, and prints summary stats so triage doesn't require opening
# the JSON manually.
#
# Exit codes:
#   0  -- bot test passed AND artifacts copied successfully.
#   1  -- bot test failed (artifacts may or may not exist; check OutDir).
#   2  -- bot test passed but telemetry artifacts were missing.
#   3  -- couldn't find the bot exe (build with -Build first).
#
# ASCII-only script body so Windows PowerShell 5.1 + pwsh 7+ parse it
# without UTF-8 / CP1252 mojibake. See run_dp_tests.ps1's preamble for
# the rationale (Q-2026-05-12-005).

[CmdletBinding()]
param(
    [string]$Exe       = "Games/DevilsPlayground/Build/output/win64/vulkan_vs2022_debug_win64_true/devilsplayground.exe",
    [string]$OutDir    = "Build/artifacts/telemetry",
    [switch]$Headless  = $true,
    [switch]$Build     = $false,
    [switch]$SkipRun   = $false,
    # Bot needs more than the harness default 600-frame cap to walk
    # the GameLevel map. Pre-possess + scene load = ~60 frames; pathing
    # to 5 objectives + the pentagram via a multi-possession chain is
    # ~3-5 min of bot time at jog speed. 18300 frames = 5 min, matches
    # Test_DPHeuristicBotPlaythrough's kMaxFrames so the bot gets its
    # full budget when invoked through this runner.
    [int]$ExitAfterFrames = 18300,
    [string]$RunnerScript = "Tools/run_dp_tests.ps1",
    # When -Visualise is set, post-process the captured telemetry into
    # a top-down PNG (Tools/dp_telemetry_visualise.ps1). Useful for
    # spotting "bot stood in a corner" failure modes that pass the
    # analyzer's pipeline-health criteria but didn't really play the
    # game.
    [switch]$Visualise,
    [string]$VisualiserScript = "Tools/dp_telemetry_visualise.ps1"
)

$ErrorActionPreference = 'Stop'

function Write-Section($msg) {
    Write-Host ""
    Write-Host "==== $msg ====" -ForegroundColor Cyan
}

# ----------------------------------------------------------------------
# 0. Pre-flight.
# ----------------------------------------------------------------------
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if ($Build) {
    Write-Section "Building DevilsPlayground"
    $msbuild = "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    if (-not (Test-Path $msbuild)) {
        Write-Warning "MSBuild not found at $msbuild; assuming PATH"
        $msbuild = "msbuild"
    }
    & $msbuild 'Games/DevilsPlayground/devilsplayground_win64.sln' `
        '-target:DevilsPlayground' `
        '-property:Configuration=Vulkan_vs2022_Debug_Win64_True' `
        '-property:Platform=x64' `
        '-maxCpuCount' `
        '-nologo' `
        '-verbosity:minimal'
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed (exit $LASTEXITCODE)"
        exit 1
    }
}

if (-not (Test-Path $Exe)) {
    Write-Error "exe not found: $Exe  (try -Build, or supply -Exe <path>)"
    exit 3
}

# Resolve OS temp dir. Matches the test's std::filesystem::temp_directory_path.
$tempDir = $env:TEMP
if (-not $tempDir) { $tempDir = $env:TMP }
if (-not $tempDir) { $tempDir = '/tmp' }

# Bot test writes to <tmp>/dp_bot_playthrough.{ztlm,json}.
$srcBin  = Join-Path $tempDir 'dp_bot_playthrough.ztlm'
$srcJson = Join-Path $tempDir 'dp_bot_playthrough.json'

# ----------------------------------------------------------------------
# 1. Run the bot playthrough.
# ----------------------------------------------------------------------
$testExit = 0
if (-not $SkipRun) {
    Write-Section "Running Test_DPHeuristicBotPlaythrough"
    if (-not (Test-Path $RunnerScript)) {
        Write-Error "runner script not found: $RunnerScript"
        exit 1
    }
    $runnerArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', $RunnerScript,
        '-Filter', 'Test_DPHeuristicBotPlaythrough',
        '-ExitAfterFrames', $ExitAfterFrames)
    if ($Headless) { $runnerArgs += '-Headless' }
    & pwsh.exe @runnerArgs
    $testExit = $LASTEXITCODE
    Write-Host "run_dp_tests exit: $testExit"
}

# ----------------------------------------------------------------------
# 2. Collect artifacts.
# ----------------------------------------------------------------------
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

$destBin  = Join-Path $OutDir 'bot_playthrough.ztlm'
$destJson = Join-Path $OutDir 'bot_playthrough.json'

$haveArtifacts = $true
if (Test-Path $srcBin) {
    Copy-Item $srcBin $destBin -Force
    Write-Host "Copied binary  : $srcBin -> $destBin"
} else {
    Write-Warning "Telemetry binary not found at $srcBin"
    $haveArtifacts = $false
}

if (Test-Path $srcJson) {
    Copy-Item $srcJson $destJson -Force
    Write-Host "Copied JSON    : $srcJson -> $destJson"
} else {
    Write-Warning "Telemetry JSON not found at $srcJson"
    $haveArtifacts = $false
}

if (-not $haveArtifacts) {
    if ($testExit -eq 0) {
        Write-Warning "Test passed but artifacts were not present in $tempDir."
        exit 2
    }
    Write-Warning "Test failed AND artifacts missing. Re-run with verbose tracing."
    exit 1
}

# ----------------------------------------------------------------------
# 3. Summarize.
# ----------------------------------------------------------------------
Write-Section "Telemetry summary"

$binSize = (Get-Item $destBin).Length
$jsonSize = (Get-Item $destJson).Length

# Parse JSON for headline numbers. ConvertFrom-Json handles either order
# of fields. The exporter's schema is:
#   { "header": {...}, "frames": [...], "events": [...] }
$jsonContent = Get-Content $destJson -Raw
try {
    $telemetry = $jsonContent | ConvertFrom-Json
} catch {
    Write-Warning "JSON parse failed: $_"
    exit 1
}

$frameCount = 0
$eventCount = 0
if ($telemetry.frames) { $frameCount = ($telemetry.frames | Measure-Object).Count }
if ($telemetry.events) { $eventCount = ($telemetry.events | Measure-Object).Count }

# Bucket events by name (set by the DPEventTypeToString resolver in the
# exporter). Useful for "what mechanics actually fired".
$eventBuckets = @{}
if ($telemetry.events) {
    foreach ($evt in $telemetry.events) {
        $key = 'Unknown'
        if ($evt.PSObject.Properties.Name -contains 'name') {
            $key = $evt.name
        } else {
            $key = "type_$($evt.type)"
        }
        if (-not $eventBuckets.ContainsKey($key)) { $eventBuckets[$key] = 0 }
        $eventBuckets[$key] = $eventBuckets[$key] + 1
    }
}

Write-Host ("Scene name     : {0}" -f $telemetry.header.sceneName)
Write-Host ("Seed           : 0x{0:X}" -f [int64]$telemetry.header.seed)
Write-Host ("Sample period  : {0} frames" -f $telemetry.header.samplePeriodFrames)
Write-Host ("Binary size    : {0} bytes" -f $binSize)
Write-Host ("JSON size      : {0} bytes" -f $jsonSize)
Write-Host ("Frame samples  : {0}" -f $frameCount)
Write-Host ("Total events   : {0}" -f $eventCount)
if ($eventBuckets.Count -gt 0) {
    Write-Host "Event breakdown:"
    foreach ($k in ($eventBuckets.Keys | Sort-Object)) {
        Write-Host ("  {0,-22} {1}" -f $k, $eventBuckets[$k])
    }
} else {
    Write-Host "No events recorded."
}

# ----------------------------------------------------------------------
# 3a. Optional visualisation. Runs the post-processor on the JSON we
#     just copied into $OutDir, producing a sibling .png next to the
#     binary + JSON artifacts.
# ----------------------------------------------------------------------
if ($Visualise) {
    Write-Section "Visualising"
    if (-not (Test-Path $VisualiserScript)) {
        Write-Warning "Visualiser script not found: $VisualiserScript -- skipping."
    } else {
        $pngPath = Join-Path $OutDir 'bot_playthrough.png'
        & pwsh.exe -NoProfile -ExecutionPolicy Bypass `
            -File $VisualiserScript `
            -JsonPath $destJson `
            -OutPath $pngPath `
            -Quiet
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "Visualiser exited $LASTEXITCODE."
        } elseif (Test-Path $pngPath) {
            $kb = [int]((Get-Item $pngPath).Length / 1024)
            Write-Host "Wrote PNG     : $pngPath ($kb KB)"
        }
    }
}

# ----------------------------------------------------------------------
# 4. Final verdict.
# ----------------------------------------------------------------------
Write-Section "Verdict"
if ($testExit -eq 0) {
    Write-Host "Bot playthrough PASSED. Artifacts in $OutDir" -ForegroundColor Green
} else {
    Write-Host "Bot playthrough FAILED (exit $testExit). Artifacts in $OutDir" -ForegroundColor Red
}
exit $testExit
