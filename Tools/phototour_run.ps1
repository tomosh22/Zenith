<#
.SYNOPSIS
  Run one game's photo tour (RT_PhotoTour / ZM_PhotoTour_Test) windowed and crop
  the dumps to the editor viewport.

  pwsh -NoProfile -File Tools\phototour_run.ps1 -Game RenderTest -Tag baseline
  pwsh -NoProfile -File Tools\phototour_run.ps1 -Game Zenithmon  -Tag baseline -Compare baseline

  -Config   defaults to Vulkan_vs2022_Release_Win64_True (any Vulkan_*_True works;
            the tour needs the editor viewport query, so a tools build).
  -Compare  another tag under the same game's phototour dir: writes side-by-side
            compare_<shot>.png + compare.png into THIS run's directory. A
            comparison that does not pair EVERY shot on both sides FAILS, naming
            the unmatched ones -- a silently smaller comparison is how a missing
            capture goes unnoticed, and zero pairs used to exit 0 having compared
            nothing. Pass -AllowPartialCompare for a deliberate asymmetry (an `ab`
            run against a plain one). Comparing a tag with ITSELF is refused.

  A REUSED -Tag IS CLEARED FIRST. The cropper enumerates every .tga in the
  directory, so a tag must hold one run's captures and nothing else; stale files
  also satisfy the tour's own on-disk verification, which would let a run that
  captured nothing verify clean against the previous run's output.

  -ExtraArgs is forwarded to the exe verbatim. The ones worth knowing:

    --phototour-shadows=0|1|ab          the WHOLE shadow system
    --phototour-terrain-shadows=0|1|ab  the terrain caster only
        `ab` captures each pose TWICE from the same pose with only that feature
        flipped, writing <pose>__noshadow.tga / <pose>__noterrainshadow.tga
        beside the base shot. Use this rather than diffing two runs: wind is
        driven from process start with a variable boot frame count, so the
        measured run-to-run noise floor is 0.9-1.3x the effect being measured,
        against 2-7x for the in-run pair.

    --ds-debug=N                        ONE G-buffer/lighting term, no rebuild
        7 = world normal, 8 = NdotL, 9 = sun shadow factor (also 2 depth,
        3 albedo, 5 roughness, 6 AO, 10 shading model, 11 IBL ambient).
        ★ Reach for this BEFORE an A/B when something is MISSING rather than
        merely different -- it says why, where a diff only says that.
        See Zenith/Flux/Shadows/CLAUDE.md -> Diagnosing "X stopped casting a
        shadow", and Docs/design/Photorealism.md for the whole programme.

  EXIT CODE. Exactly ONE known teardown fault is forgiven, and only when its exact
  signature ("Worker N scratch buffer overflow") is in the log: it fires after the
  test reports PASSED and after every capture is on disk. Any OTHER nonzero exit is
  reported, with the tail of the log -- forgiving all of them once DONE appeared is
  a fail-open, and it hid a real grass-handle lifetime bug for as long as it existed.
  A held-asset warning at registry shutdown is surfaced too, and the crop step's own
  exit code propagates.
#>
param(
    [Parameter(Mandatory = $true)][ValidateSet('RenderTest', 'Zenithmon')][string]$Game,
    [string]$Tag = 'run',
    [string]$Config = 'Vulkan_vs2022_Release_Win64_True',
    [int]$Settle = 150,
    [string]$WindowSize = '3840x2160',
    [string]$Compare = '',
    [int]$TimeoutSec = 1500,
    # A comparison that does not pair up every shot on both sides FAILS by default
    # -- a silently smaller comparison is how a missing capture goes unnoticed.
    # Set this for a deliberately asymmetric pair (an `ab` run against a plain one).
    [switch]$AllowPartialCompare,
    [string[]]$ExtraArgs = @()
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$exeDir = Join-Path $repo "Games\$Game\Build\output\win64\$($Config.ToLower())"
$exe = Join-Path $exeDir "$($Game.ToLower()).exe"
if (-not (Test-Path $exe)) { throw "no exe at $exe -- build $Game --config $Config first" }

Import-Module (Join-Path $repo 'Build\zenith_buildsystem.psm1') -Force
Repair-ZenithRuntimeDlls -ExeDir $exeDir | Out-Null

$test = if ($Game -eq 'RenderTest') { 'RT_PhotoTour' } else { 'ZM_PhotoTour_Test' }
$outDir = Join-Path $repo "Build\artifacts\$($Game.ToLower())\phototour\$Tag"

# Comparing a tag with ITSELF is meaningless, and the clean below would delete the
# very captures being compared against. Refuse before anything is removed.
if ($Compare -and $Compare -eq $Tag) {
    throw "-Compare '$Compare' is the same tag as -Tag: nothing to compare, and the pre-run clean would delete it"
}

# The cropper downstream takes a DIRECTORY and enumerates every .tga in it, so a
# tag directory must hold THIS run's captures and nothing else.
#
# ★ A REUSED TAG USED TO KEEP THE PREVIOUS RUN'S FILES. Not merely a stale
# contact sheet: an `ab` run followed by a plain one left orphan __noshadow shots
# that the sheet presented as part of the new run, and -- worse -- the tour's own
# on-disk Verify (which file_size's every path it requested) would be SATISFIED BY
# A STALE FILE at that path. A run that captured nothing could verify clean against
# the previous run's output. Clear it, so "the file exists" means "this run wrote it".
New-Item -ItemType Directory -Force $outDir | Out-Null
$stale = @(Get-ChildItem -Path $outDir -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in '.tga', '.png', '.rect', '.log' })
if ($stale.Count -gt 0) {
    Write-Host "[phototour] tag '$Tag' already held $($stale.Count) artifact(s); removing them so this run stands alone"
    $stale | Remove-Item -Force
}
$log = Join-Path $outDir 'run.log'

$args = @('--automated-test', $test, '--skip-unit-tests', '--window-size', $WindowSize, "--phototour-tag=$Tag", "--phototour-settle=$Settle") + $ExtraArgs
Write-Host "[phototour] $exe $($args -join ' ')"
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $exeDir -PassThru -NoNewWindow `
    -RedirectStandardOutput $log -RedirectStandardError (Join-Path $outDir 'run.err.log')
while (-not $p.HasExited -and $sw.Elapsed.TotalSeconds -lt $TimeoutSec) { Start-Sleep -Seconds 2 }
if (-not $p.HasExited) { Write-Host "[phototour] TIMEOUT after $TimeoutSec s -- killing"; Stop-Process -Id $p.Id -Force }
$p.WaitForExit()
Write-Host "[phototour] exit=$($p.ExitCode) in $([int]$sw.Elapsed.TotalSeconds)s"
Select-String -Path $log -Pattern 'PhotoTour\]' | ForEach-Object { $_.Line } | Select-Object -Last 40

# ONE known teardown fault is forgiven, BY ITS EXACT SIGNATURE, and nothing else.
#
# The fault: the world-reset path records command-buffer scratch after the last
# per-frame reset, so it overflows a worker partition at any partition size. It
# fires AFTER the test reports PASSED and after every screenshot is on disk.
#
# ★ THIS USED TO FORGIVE *ANY* NONZERO EXIT once DONE appeared, which is a
# fail-open: DONE is logged before engine shutdown, so every teardown regression
# after it was invisible. It hid a real one -- grass texture handles outliving the
# asset registry (Flux_GrassImpl::ReleaseAssetReferences was never called), which
# the log had been reporting as "still held with 4 refs" the whole time. A
# suppression that is not pinned to a signature is a suppression of the future.
$doneLine = Select-String -Path $log -Pattern 'PhotoTour\] DONE' | Select-Object -Last 1
$exitCode = $p.ExitCode
$knownTeardown = 'Assertion failed: Worker \d+ scratch buffer overflow'
if ($doneLine -and $exitCode -ne 0) {
    $sig = Select-String -Path $log -Pattern $knownTeardown | Select-Object -Last 1
    if ($sig) {
        Write-Host "[phototour] tour completed; ignoring the KNOWN teardown fault (exit $exitCode): $($sig.Line.Trim())"
        $exitCode = 0
    } else {
        Write-Host "[phototour] tour reached DONE but exited $exitCode with no known-teardown signature -- reporting it."
        Write-Host "[phototour] last 15 log lines:"
        Get-Content $log -Tail 15 | ForEach-Object { Write-Host "    $_" }
    }
}

# A held asset at registry shutdown is a leaked handle, and the next thing it does
# is Release() into freed memory. Never fatal to the captures, always worth saying.
$heldRefs = Select-String -Path $log -Pattern 'still held with \d+ refs'
if ($heldRefs) {
    Write-Host "[phototour] WARNING: $($heldRefs.Count) asset(s) still held at registry shutdown:"
    $heldRefs | ForEach-Object { Write-Host "    $($_.Line.Trim())" }
}

# Checked AFTER the tour so a missing interpreter never costs a capture run, but
# reported as its own failure instead of a terminating error out of `& python`.
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    Write-Host "[phototour] ERROR: 'python' is not on PATH -- captures are in $outDir but were NOT cropped."
    Write-Host "[phototour] crop them later with: python Tools\phototour_crop.py ""$outDir"""
    exit 1
}

$cropArgs = @((Join-Path $repo 'Tools\phototour_crop.py'), $outDir)
if ($Compare) { $cropArgs += @('--compare', $outDir); $cropArgs[1] = Join-Path $repo "Build\artifacts\$($Game.ToLower())\phototour\$Compare" }
if ($AllowPartialCompare) { $cropArgs += '--allow-partial' }
& python @cropArgs
# The crop step FAILS when it found no readable captures. That is the second half
# of "a queued dump is not a written file": the tour now verifies its own shots on
# disk, and this catches the case where they are unreadable by the time we crop.
if ($LASTEXITCODE -ne 0) {
    Write-Host "[phototour] crop step failed (exit $LASTEXITCODE)"
    if ($exitCode -eq 0) { $exitCode = $LASTEXITCODE }
}
exit $exitCode
