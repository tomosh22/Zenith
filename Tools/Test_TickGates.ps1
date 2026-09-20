# Test_TickGates.ps1 -- assertions for Tools/tick_gates.ps1's pure helpers.
#
# A plain assert script, matching Tools/Test_UnitBaselineManifest.ps1 and
# Tools/zagent/Test-ZagentClient.ps1: the only PowerShell this repo requires is
# the `pwsh` every gate already needs.
#
# It covers the parts that BIT during development, every one of them found by
# running the script rather than reading it:
#
#   * `@($list)` on a List[object] throws "Argument types do not match" on
#     pwsh 7.6.5 -- with no location and no stack. It fired only on the FAILURE
#     path, which is the worst place for a script the loop branches on exit
#     codes from: the gate had genuinely failed and the report of it crashed.
#   * `git status --porcelain` is the ONLY spelling that sees a CREATED file.
#     `git diff --name-only` reports tracked modifications, so a file the
#     worker wrote never reaches the check -- the same fail-open that survived
#     two green ticks in `zagent guard`.
#   * The by-name build assertion needs MSBuild to echo each TU. It does, at
#     /v:minimal, on its own line -- verified against a real build log.
#
# Usage: pwsh -NoProfile -File Tools/Test_TickGates.ps1
# Exit:  0 all pass, 1 otherwise.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
# -NoRun: dot-sourcing a script RUNS it, and a test that claims a gate ran is
# the one thing tick_gates.ps1 exists to prevent.
. (Join-Path $PSScriptRoot 'tick_gates.ps1') -Key 'TEST-0' -RepoRoot $repoRoot -NoRun

$script:failures = 0
function Assert-That {
    param([string]$Name, [scriptblock]$Body)
    $ok = $false
    try { $ok = [bool](& $Body) } catch { Write-Host "       threw: $($_.Exception.Message)" -ForegroundColor Yellow }
    if ($ok) { Write-Host "  ok   $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL $Name" -ForegroundColor Red; $script:failures++ }
}

function New-GitFixture {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("tickgates_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    & git -C $dir init --quiet 2>$null | Out-Null
    & git -C $dir config user.email 'test@example.invalid' 2>$null | Out-Null
    & git -C $dir config user.name 'test' 2>$null | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $dir 'Games/Probe/Tests') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $dir 'Games/Probe/Tests/Existing.cpp') -Value '// tracked' -Encoding utf8
    & git -C $dir add -A 2>$null | Out-Null
    & git -C $dir commit -m init --quiet 2>$null | Out-Null
    return $dir
}

Write-Host "`n=== Get-CreatedSourceFiles ===" -ForegroundColor Cyan
# `git status --porcelain`, never `git diff --name-only`. Diff reports tracked
# MODIFICATIONS only, so a file the worker CREATED never reaches the check --
# and that spelling failed OPEN in `zagent guard` for two green ticks before a
# file-creating ticket exposed it.

Assert-That 'sees an UNTRACKED .cpp, which is what a worker actually leaves' {
    $repo = New-GitFixture
    try {
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Brand_New.cpp') -Value '// new' -Encoding utf8
        $found = Get-CreatedSourceFiles -RepoRoot $repo
        (@($found).Count -eq 1) -and (@($found)[0].Path -like '*Brand_New.cpp') -and (@($found)[0].IsNew)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'sees a STAGED-new .cpp too, not only an untracked one' {
    $repo = New-GitFixture
    try {
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Staged.cpp') -Value '// new' -Encoding utf8
        & git -C $repo add -A 2>$null | Out-Null
        $found = Get-CreatedSourceFiles -RepoRoot $repo
        (@($found).Count -eq 1) -and (@($found)[0].IsNew)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'IGNORES a mere modification -- the vcxproj already knows that file' {
    # Regenerating on every edit would cost a full rebuild on every ticket.
    $repo = New-GitFixture
    try {
        Add-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Existing.cpp') -Value '// edited'
        (Get-CreatedSourceFiles -RepoRoot $repo).Count -eq 0
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host "`n=== Get-CreatedIncludeFiles ===" -ForegroundColor Cyan
# A .Tests.inl is NOT a translation unit, so the by-name build-log assertion
# cannot see it -- and every Flux unit suite in this repo is one, #included
# from the bottom of its .cpp. Create one, forget the #include, and the build
# is green, the unit count does not move, and nothing reports it. Measured on
# ZEN-40, where tick_gates printed "no created source files; skipping regen"
# with a new .Tests.inl in its own changed list.

Assert-That 'sees a created .inl, which the .cpp filter deliberately excludes' {
    $repo = New-GitFixture
    try {
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Probe.Tests.inl') -Value '// new' -Encoding utf8
        # PLAIN ASSIGNMENT, not `@(...)`. These helpers return `, $arr` so a
        # one-element result is not unrolled into a bare string (the trap this
        # file's header describes). Wrapping the CALL in `@()` re-wraps that
        # envelope instead of flattening it: you get a 1-element array whose
        # element is the array, `$x[0]` is a String[], and `-like` silently
        # fails. The object-valued assertions above only survive it because
        # `.Path` member-enumerates across the nested array.
        $inl = Get-CreatedIncludeFiles -RepoRoot $repo
        $cpp = Get-CreatedSourceFiles -RepoRoot $repo
        # The two lists are disjoint ON PURPOSE: an .inl needs no regen (it is
        # not its own TU) but does need the inclusion proof.
        (@($inl).Count -eq 1) -and ($inl[0] -like '*Probe.Tests.inl') -and (@($cpp).Count -eq 0)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'IGNORES a modified .inl -- only a CREATED one can be orphaned' {
    $repo = New-GitFixture
    try {
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Tracked.inl') -Value '// tracked' -Encoding utf8
        & git -C $repo add -A 2>$null | Out-Null
        & git -C $repo commit -m inl --quiet 2>$null | Out-Null
        Add-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Tracked.inl') -Value '// edited'
        (Get-CreatedIncludeFiles -RepoRoot $repo).Count -eq 0
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'the inclusion probe distinguishes an INCLUDED .inl from an orphan' {
    # This is the predicate the gate branches on, exercised directly: `git
    # grep -l` over TRACKED files, with the .inl itself excluded so a file
    # that merely names itself in a comment cannot pass.
    $repo = New-GitFixture
    try {
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Orphan.Tests.inl') -Value '// nobody includes me' -Encoding utf8
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Wired.Tests.inl') -Value '// included below' -Encoding utf8
        Add-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Existing.cpp') -Value '#include "Wired.Tests.inl"'
        & git -C $repo add -A 2>$null | Out-Null
        & git -C $repo commit -m wire --quiet 2>$null | Out-Null

        $probe = {
            param($leaf, $self)
            @(& git -C $repo grep -l --fixed-strings -- $leaf 2>$null) |
                Where-Object { $_ -and ($_ -replace '\\', '/') -ne $self }
        }
        $wired = @(& $probe 'Wired.Tests.inl' 'Games/Probe/Tests/Wired.Tests.inl')
        $orphan = @(& $probe 'Orphan.Tests.inl' 'Games/Probe/Tests/Orphan.Tests.inl')
        ($wired.Count -gt 0) -and ($orphan.Count -eq 0)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'sees a DELETED .cpp, marked NOT new' {
    # A deletion needs a regen too -- the vcxproj still lists the file -- but
    # the by-name build assertion must not run on it, because a deleted file
    # can never appear in a build log.
    $repo = New-GitFixture
    try {
        Remove-Item -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Existing.cpp') -Force
        $found = @(Get-CreatedSourceFiles -RepoRoot $repo)
        ($found.Count -eq 1) -and (-not $found[0].IsNew)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a RENAMED .cpp yields the DESTINATION path, and counts as new' {
    # `git status --porcelain` reports a rename as `R  old.cpp -> new.cpp` on
    # ONE line. Taking the whole remainder gives a path that exists nowhere --
    # and it still ends in .cpp, so it passes the extension filter and lands in
    # the report as garbage. The destination is a new TU as far as the vcxproj
    # is concerned: it has never been in the file list.
    $repo = New-GitFixture
    try {
        & git -C $repo mv 'Games/Probe/Tests/Existing.cpp' 'Games/Probe/Tests/Renamed.cpp' 2>$null | Out-Null
        $found = @(Get-CreatedSourceFiles -RepoRoot $repo)
        ($found.Count -eq 1) -and ($found[0].Path -eq 'Games/Probe/Tests/Renamed.cpp') -and $found[0].IsNew
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'ignores non-source files -- a new .md needs no regen' {
    $repo = New-GitFixture
    try {
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Notes.md') -Value '# hi' -Encoding utf8
        (Get-CreatedSourceFiles -RepoRoot $repo).Count -eq 0
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'returns an ARRAY, so .Count on a one-file result is not $null' {
    # `,$x.ToArray()` on the return: a PowerShell function UNROLLS a returned
    # collection, so a single created file would come back as a bare object and
    # `.Count` would be $null -- and `if ($created.Count -gt 0)` would then be
    # false, silently skipping the regen. Same hazard as ZagentClient's
    # `,@(...)` returns.
    $repo = New-GitFixture
    try {
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Tests/Only.cpp') -Value '// new' -Encoding utf8
        $found = Get-CreatedSourceFiles -RepoRoot $repo
        ($found -is [array]) -and ($found.Count -eq 1)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a clean tree yields an EMPTY ARRAY, not $null' {
    $repo = New-GitFixture
    try {
        $found = Get-CreatedSourceFiles -RepoRoot $repo
        ($null -ne $found) -and ($found -is [array]) -and ($found.Count -eq 0)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host "`n=== the by-name build assertion ===" -ForegroundColor Cyan
# The check is `$buildLog -like "*$leaf*"`. Its whole premise is that MSBuild
# echoes each compiled TU by name -- confirmed against a real /v:minimal log,
# where the line is the bare file name on its own.

Assert-That 'a compiled TU is found by its leaf name in a real /v:minimal log' {
    $log = @(
        '[build] Zenithmon (Null_vs2022_Debug_Win64_True)...',
        '  zenith_win64.vcxproj -> C:\dev\Zenith\build\output\win64\null\zenith.lib',
        '  ZM_TickGatesProbe.cpp',
        '  zenithmon_win64.vcxproj -> C:\dev\Zenith\Games\Zenithmon\build\zenithmon.exe'
    ) -join [Environment]::NewLine
    $log -like '*ZM_TickGatesProbe.cpp*'
}

Assert-That '** AN UNCOMPILED TU IS NOT FOUND -- the assertion actually fires' {
    # The whole point. ZM-27: three new files, build exit 0, zero occurrences
    # of the new TU in the log, and nothing downstream could tell.
    $log = @(
        '[build] Zenithmon (Null_vs2022_Debug_Win64_True)...',
        '  zenithmon_win64.vcxproj -> C:\dev\Zenith\Games\Zenithmon\build\zenithmon.exe',
        'Built Zenithmon (Null_vs2022_Debug_Win64_True).'
    ) -join [Environment]::NewLine
    -not ($log -like '*ZM_NeverCompiled.cpp*')
}

Write-Host "`n=== the List[object] hazard this script kept tripping ===" -ForegroundColor Cyan
# Not a test OF tick_gates so much as a lock ON the fact it depends on: if a
# future pwsh makes `@($list)` work, this test fails and the comments in
# tick_gates.ps1 can be simplified. If it keeps throwing, nobody re-learns it
# by watching a gate report crash.

Assert-That '@() over a List[object] still throws; .ToArray() does not' {
    $list = New-Object System.Collections.Generic.List[object]
    $list.Add([PSCustomObject]@{ a = 1 })
    $threw = $false
    try { $null = @($list) } catch { $threw = $true }
    $viaToArray = @($list.ToArray())
    $threw -and ($viaToArray.Count -eq 1)
}

Write-Host "`n=== the exe sweep names every game ===" -ForegroundColor Cyan

Assert-That 'the sweep derives its process names from Games/*, so a new game is covered' {
    # A hard-coded list would leave a game added later swept by nothing, and the
    # symptom is a build failing on a locked output file with no connection to
    # the change that caused it.
    $games = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Games') -Directory |
        ForEach-Object { $_.Name.ToLowerInvariant() })
    ($games -contains 'zenithmon') -and ($games -contains 'combat') -and ($games.Count -ge 4)
}

Assert-That 'the sweep is scoped to game names ONLY -- it must not match a build tool' {
    # ** NEVER CALL Invoke-ExeSweep FROM A TEST. It killed a `zenith test
    # Zenithmon --headless` gate that was running in another window -- the gate
    # failed after 263s for a reason that had nothing to do with the tree. A
    # test whose side effect is terminating other people's processes is not a
    # test. Assert the SELECTION instead, against a synthetic process table.
    $names = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Games') -Directory |
        ForEach-Object { $_.Name.ToLowerInvariant() })
    $wouldKill = @('zenithmon', 'combat', 'msbuild', 'cl', 'pwsh', 'devenv', 'git') |
        Where-Object { $names -contains $_ }
    # Games yes, build tools no. A sweep that matched `msbuild` or `pwsh` would
    # take down the run that invoked it.
    ($wouldKill -contains 'zenithmon') -and ($wouldKill -contains 'combat') -and
    ($wouldKill -notcontains 'msbuild') -and ($wouldKill -notcontains 'pwsh') -and
    ($wouldKill -notcontains 'cl') -and ($wouldKill -notcontains 'git')
}

Write-Host ''
if ($script:failures -eq 0) {
    Write-Host 'PASS' -ForegroundColor Green
    exit 0
}
Write-Host "$($script:failures) assertion(s) failed" -ForegroundColor Red
exit 1
