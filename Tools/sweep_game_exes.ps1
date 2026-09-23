# sweep_game_exes.ps1 -- the project's `cleanup` line: kill stray game exes
# left running under THIS checkout's own tree only.
# =============================================================================
# zagent.project.json's `cleanup` array is a committed STRING that may be run
# by a POSIX shell OR by PowerShell -- so it must be `$`-free, and its root
# must be derived at RUN TIME (this script's own $PSScriptRoot), never a
# literal path baked into the committed line.
#
# It used to be an inline `Get-Process | Stop-Process` one-liner that matched
# by NAME ALONE, across the WHOLE MACHINE. That was safe with one checkout;
# it is not once a lane exists, because a lane and the main checkout (or two
# lanes) can each have their own zenithmon.exe/combat.exe/devilsplayground.exe
# running at the same time, and the old line would kill all of them from
# whichever one happened to run its cleanup step first.
#
# This script fixes that by reusing the path-scoped, directory-boundary-safe
# killer every gate phase already uses: it dot-sources Tools/tick_gates.ps1
# with -NoRun (exactly the pattern Tools/Test_TickGates.ps1 uses to reach the
# same pure helpers without running a single gate line) to pull in
# Invoke-ExeSweep/Select-SweepTargets, then calls Invoke-ExeSweep against
# THIS script's own repo root -- so a lane's cleanup can never reach across
# into the main checkout or a sibling lane's own game exes, and vice versa.
#
# Lane mode skips this step entirely at its own step 9 (see the Lane mode
# section of Tools/zagent/protocol/TICK.md) -- a main-mode tick's step 9 runs
# after `finish` releases I5, while a lane may still be mid-build, and a
# sweep run against a build in progress is exactly the hazard the
# path-scoping above exists to avoid.
#
# Exit: always 0. A sweep that finds nothing to sweep is a success -- see the
# `$cleanup` note in zagent.project.json -- and a cleanup line that reports
# failure for the common "nothing running" case is a trap for anything that
# ever branches on the result.
#
# Usage:
#   pwsh -NoProfile -File Tools/sweep_game_exes.ps1
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

$ErrorActionPreference = 'Stop'

try {
    $repoRoot = Split-Path -Parent $PSScriptRoot

    # -NoRun: dot-sourcing a script RUNS it, and this line exists to sweep
    # stray processes, not to run a ticket's gates. Tools/Test_TickGates.ps1
    # reaches tick_gates.ps1's pure helpers the identical way.
    . (Join-Path $PSScriptRoot 'tick_gates.ps1') -Key 'SWEEP' -RepoRoot $repoRoot -NoRun

    Invoke-ExeSweep -RepoRoot $repoRoot
} catch {
    Write-Host "[sweep_game_exes] $($_.Exception.Message)" -ForegroundColor Yellow
}

exit 0
