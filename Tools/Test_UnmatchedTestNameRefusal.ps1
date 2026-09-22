# Test_UnmatchedTestNameRefusal.ps1 -- process-level proof that the
# --automated-test refusal wiring in BuildHarnessExecutionList
# (Zenith/EngineComposition/Zenith_AutomatedTest.cpp) is live end to end.
#
# WHY A PROCESS TEST, NOT A UNIT (ZEN-106 / ZEN-D-032). ExitHarness is
# [[noreturn]] -- it calls Zenith_Core::Zenith_FullShutdown() then
# std::exit(iCode) -- so no ZENITH_TEST can ever reach the three-line call
# site in BuildHarnessExecutionList:
#
#   const SingleTestResolution xResolution = BuildSingleTestResolution(...);
#   if (!xResolution.m_bFound)
#   {
#       std::printf("%s", xResolution.m_acMessage);
#       ExitHarness(xResolution.m_iExitCode);
#   }
#
# ZEN-93 extracted the DECISION (BuildSingleTestResolution) into a plain,
# unit-testable struct, and Zenith_AutomatedTest.Tests.inl (suite
# AutomatedTestSelection) pins its contents. That closes the gap ZEN-93 was
# filed over -- but it does NOT pin the wiring above: an edit that forgot the
# ExitHarness call, or inverted the `!xResolution.m_bFound` check, would pass
# every one of those units and reproduce ZEN-93's exact symptom (a bogus
# --automated-test name exits 0 having reported nothing) with zero red tests
# anywhere. Only a real process boot can see that, which is what this script
# is.
#
# ★ -Exe MUST BE A *_False (ToolsEnabled=False) CONFIG, e.g.
#   Games\<Game>\build\output\win64\null_vs2022_debug_win64_false\<game>.exe
# THIS IS LOAD-BEARING, not a style preference, and the script refuses a path
# that looks like a *_True config rather than silently running it. A *_True
# exe runs Zenith_Engine::InitialiseAssets() inside Zenith_Init --
# UNCONDITIONALLY, and BEFORE --automated-test is even parsed (Tools/CLAUDE.md:
# "This is a boot phase, not a separate program") -- and that phase's
# Project_RegisterEditorAutomationSteps then AUTHORS/re-publishes every one of
# that game's tracked .zscen files as part of the SAME boot, on EITHER render
# backend. That is board ruling ZEN-D-004: "ANY --automated-test boot authors
# scenes, on EITHER backend -- not just a windowed one." ZEN rulings live ON
# THE BOARD, not on disk -- grep root CLAUDE.md for that sentence and you find
# nothing, because it was never there. Retrieve it with
# `zagent rows decisions --json`. Both arms of this script
# would therefore dirty the tracked tree on every run under a *_True exe --
# and a dirty tree is exactly what the tick protocol's precondition check
# treats as fatal, silently blocking whichever ticket runs next. A *_False
# config never calls InitialiseAssets at all (the whole phase compiles out
# under #ifdef ZENITH_TOOLS -- see Tools/CLAUDE.md), so it only LOADS
# committed assets and authors nothing: this script has zero side effects
# when pointed at one, in EITHER arm.
#
# As a second line of defence -- belt-and-braces, in case that refusal is
# ever weakened or a *_True exe reaches this script some other way -- it also
# snapshots `git status --porcelain` before and after every invocation and
# FAILS LOUDLY if the tree moved. It does not attempt to restore the drift
# itself: silently discarding a scene-authoring diff could hide a genuine
# defect (e.g. the very ZM-D-179 / ZM-D-183 nondeterminism classes root
# CLAUDE.md and Games/Zenithmon/CLAUDE.md document), and this script's job is
# to report, not to repair. A caller that sees that assertion fail should
# re-run against a *_False exe and inspect the drift by hand.
#
# The registered name used for the anti-vacuity arm is DISCOVERED at runtime
# via --list-automated-tests -- a run-then-exit flag that PrintRegisteredTests
# straight after Zenith_Init and exits 0 through ExitHarness before any scene
# loads or any frame steps (Zenith_AutomatedTestRunner::ParseCommandLine) --
# rather than hardcoded per game. Different games register different tests
# (ZM_Boot_Test, ScriptTest_Boot_Test, RT_PhotoTour, ...) and a hardcoded name
# would silently stop testing anything the day that one test was renamed or
# retired in whichever game this script happened to be pointed at.
#
# ★ Do not assert the exact full refusal message text. ZEN-93 added
# "Did you mean" suggestions that only appear when the live registry holds a
# name IsCloseNameMatch judges close to the one asked for, so the exact text
# is registry-dependent -- i.e. it depends on which game's exe this script is
# run against. Assert on the stable fragment ("not found in registry") and
# the exit code instead, exactly as Zenith_AutomatedTest.Tests.inl's own
# BuildSingleTestResolutionUnmatchedIsExitTwoWithMessage unit does.
#
# Usage: pwsh -NoProfile -File Tools/Test_UnmatchedTestNameRefusal.ps1 -Exe <path to a *_False game exe>
# Exit:  0 all assertions pass, 1 otherwise (including "exe not found" and
#        "-Exe looks like a *_True config").
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Exe,
    # A hang guard, not the expected run length -- see Tools/run_unit_gate.ps1's
    # own header comment for the same distinction. Every invocation this script
    # makes either exits itself in well under a second (--list-automated-tests,
    # the unregistered-name refusal) or runs at most one simulated frame of a
    # real test (--exit-after-frames 1) before its own normal teardown, so a
    # healthy run never approaches this budget on any of the three.
    [int]$TimeoutSec = 60
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:count = 0
$script:failures = 0
function Assert-That {
    param([string]$Name, [scriptblock]$Body)
    $script:count++
    $ok = $false
    try { $ok = [bool](& $Body) } catch { Write-Host "       threw: $($_.Exception.Message)" -ForegroundColor Yellow }
    if ($ok) { Write-Host "  ok   $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL $Name" -ForegroundColor Red; $script:failures++ }
}

if (-not (Test-Path -LiteralPath $Exe)) {
    Write-Error "[unmatched-refusal] executable not found: $Exe"
    exit 1
}
$resolvedExe = (Resolve-Path -LiteralPath $Exe).Path

# Refuse a *_True exe outright rather than running it and hoping the
# git-status net below catches the resulting scene-authoring drift -- see the
# header comment. Checked per PATH SEGMENT (the lowercased config-leaf
# directory, e.g. "vulkan_vs2022_debug_win64_true"), not as a raw substring
# match against the whole path, so a game or exe stem that merely CONTAINS
# "true" cannot trip a false refusal.
$toolsTrueSegment = @($resolvedExe -split '[\\/]') | Where-Object { $_ -match '(?i)_true$' }
if ($toolsTrueSegment) {
    Write-Error ("[unmatched-refusal] '$resolvedExe' looks like a ToolsEnabled=True config ('$($toolsTrueSegment[0])'). " +
        "Pass a *_False exe instead (e.g. null_vs2022_debug_win64_false) -- see this script's header comment: " +
        "a *_True boot authors every tracked scene on EVERY invocation (ZEN-D-004), which this script must not do.")
    exit 1
}

$repoRoot = Split-Path -Parent $PSScriptRoot

function Get-GitStatusSnapshot {
    param([string]$RepoRoot)
    return ((& git -C $RepoRoot status --porcelain) | Out-String)
}

# Launches $resolvedExe with the given arguments and returns its exit code,
# combined stdout+stderr, and whether the watchdog had to kill it. Mirrors
# Tools/run_unit_gate.ps1's own process-launch shape.
function Invoke-GameExe {
    param([string[]]$Arguments, [int]$TimeoutSec)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $resolvedExe
    $psi.Arguments = ($Arguments -join ' ')
    $psi.WorkingDirectory = Split-Path -Parent $resolvedExe
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $proc = [System.Diagnostics.Process]::Start($psi)
    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()
    $exited = $proc.WaitForExit($TimeoutSec * 1000)
    $exitCode = $null
    if (-not $exited) {
        try { $proc.Kill() } catch { }
    } else {
        $exitCode = $proc.ExitCode
    }
    return [PSCustomObject]@{
        ExitCode = $exitCode
        Output   = $stdoutTask.Result + "`n" + $stderrTask.Result
        TimedOut = -not $exited
    }
}

$gitBefore = Get-GitStatusSnapshot -RepoRoot $repoRoot

Write-Host "`n=== discovering a registered automated test (--list-automated-tests) ===" -ForegroundColor Cyan
# --list-automated-tests is itself a run-then-exit flag (ExitHarness(0) right
# after PrintRegisteredTests, before any scene load or Setup/Step) -- so this
# discovery step carries the same zero-side-effect guarantee as the two arms
# below, on a *_False exe.
$listResult = Invoke-GameExe -Arguments @('--skip-unit-tests', '--list-automated-tests') -TimeoutSec $TimeoutSec

$registeredName = $null
if (-not $listResult.TimedOut) {
    # A REGEX split ('\r?\n', backslashes -- not the backtick CR/LF escapes
    # used elsewhere in this file for literal Write-Host newlines).
    $lines = [regex]::Split($listResult.Output, '\r?\n')
    $headerIdx = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -eq 'Registered automated tests:') { $headerIdx = $i; break }
    }
    if ($headerIdx -ge 0 -and $headerIdx + 1 -lt $lines.Count -and $lines[$headerIdx + 1] -match '^  (.+)$') {
        # PrintRegisteredTests emits "  %s\n" per line; the first one is
        # enough -- any registered name proves the anti-vacuity arm's point.
        $registeredName = $Matches[1].Trim()
    }
}

Assert-That '--list-automated-tests exits 0 and discovers at least one registered test' {
    (-not $listResult.TimedOut) -and ($listResult.ExitCode -eq 0) -and ($null -ne $registeredName) -and ($registeredName.Length -gt 0)
}
if ($registeredName) {
    Write-Host "       discovered: $registeredName" -ForegroundColor DarkGray
} else {
    Write-Host "       --- --list-automated-tests output (for triage) ---" -ForegroundColor Yellow
    Write-Host $listResult.Output -ForegroundColor Yellow
}

Write-Host "`n=== the refusal arm: an UNREGISTERED name must exit 2 and name the token ===" -ForegroundColor Cyan
# A fixed sentinel, not a random one: it must never coincide with a real
# registered name, and it must never look close enough to one to trip
# IsCloseNameMatch's substring test and add a "Did you mean" line this script
# does not assert on either way.
$sentinel = 'Zenith_NoSuchAutomatedTest_ZZZ_UnmatchedRefusalProbe'
$refusalResult = Invoke-GameExe -Arguments @('--skip-unit-tests', '--automated-test', $sentinel) -TimeoutSec $TimeoutSec

Assert-That 'an unregistered --automated-test name exits 2, not 0 -- ZEN-93''s exact symptom was a silent exit 0' {
    (-not $refusalResult.TimedOut) -and ($refusalResult.ExitCode -eq 2)
}
Assert-That 'the refusal message names the token that was rejected' {
    $refusalResult.Output -like "*$sentinel*"
}
Assert-That 'the refusal message carries the stable fragment "not found in registry" (never asserted verbatim -- see header)' {
    $refusalResult.Output -like '*not found in registry*'
}

Write-Host "`n=== the anti-vacuity arm: a REGISTERED name must NOT produce the refusal ===" -ForegroundColor Cyan
# Without this arm the three assertions above could also be satisfied by a
# defect at the opposite extreme -- e.g. BuildHarnessExecutionList exiting 2
# and printing "not found in registry" for EVERY name, matched or not -- and
# nothing above would catch it, because nothing above ever runs a name that
# IS registered.
if ($registeredName) {
    $vacuityResult = Invoke-GameExe -Arguments @('--skip-unit-tests', '--automated-test', $registeredName, '--exit-after-frames', '1') -TimeoutSec $TimeoutSec

    Assert-That 'a REGISTERED name does not exit 2' {
        (-not $vacuityResult.TimedOut) -and ($vacuityResult.ExitCode -ne 2)
    }
    Assert-That 'a REGISTERED name does not print the "not found in registry" refusal' {
        $vacuityResult.Output -notlike '*not found in registry*'
    }
} else {
    # Discovery already failed above and is reported there; this keeps the
    # assertion count honest rather than silently running fewer checks.
    Assert-That 'anti-vacuity arm could not run -- no registered test was discovered (see the discovery section above)' { $false }
}

$gitAfter = Get-GitStatusSnapshot -RepoRoot $repoRoot
Assert-That 'the tree is exactly as clean afterwards as it was before -- a *_False exe must author nothing (ZEN-D-004)' {
    if ($gitBefore -ne $gitAfter) {
        Write-Host "       --- git status diff (unexpected residue) ---" -ForegroundColor Yellow
        Write-Host "       before:`n$gitBefore" -ForegroundColor Yellow
        Write-Host "       after:`n$gitAfter" -ForegroundColor Yellow
    }
    $gitBefore -eq $gitAfter
}

Write-Host ''
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) {
    Write-Host 'PASS' -ForegroundColor Green
    exit 0
}
Write-Host "$($script:failures) assertion(s) failed" -ForegroundColor Red
exit 1
