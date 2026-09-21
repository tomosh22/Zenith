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
$script:count = 0
function Assert-That {
    param([string]$Name, [scriptblock]$Body)
    $script:count++
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

Write-Host "`n=== Resolve-GateConfigLeaf / Get-GateBuildGameName ===" -ForegroundColor Cyan
# The config leaf and game name Find-CompiledObjectPath needs to locate an
# .obj under IntermediatePath, derived the same way zenith.ps1's own flags
# pick a config (zenith_config.psd1: HeadlessConfigWin64 / DefaultConfigWin64)
# and Sharpmake lowercases it (Build/zenith_buildsystem.psm1's
# ConvertTo-ZenithOutputDir is the one other place that fact lives).

Assert-That '--headless resolves to the Null headless config, lowercased' {
    (Resolve-GateConfigLeaf 'pwsh -NoProfile -File Tools\zenith.ps1 build Zenithmon --headless') -eq 'null_vs2022_debug_win64_true'
}

Assert-That '--config <X> is used verbatim, only lowercased' {
    (Resolve-GateConfigLeaf 'pwsh -NoProfile -File Tools\zenith.ps1 build Combat --config D3D12_vs2022_Debug_Win64_False') -eq 'd3d12_vs2022_debug_win64_false'
}

Assert-That 'neither flag falls back to DefaultConfigWin64, lowercased' {
    (Resolve-GateConfigLeaf 'pwsh -NoProfile -File Tools\zenith.ps1 build Combat') -eq 'vulkan_vs2022_debug_win64_true'
}

Assert-That 'Get-GateBuildGameName reads the game out of a build line' {
    (Get-GateBuildGameName 'pwsh -NoProfile -File Tools\zenith.ps1 build Zenithmon --headless') -eq 'Zenithmon'
}

Assert-That 'Get-GateBuildGameName returns $null for `build engine` -- it builds no game project' {
    $null -eq (Get-GateBuildGameName 'pwsh -NoProfile -File Tools\zenith.ps1 build engine')
}

Write-Host "`n=== Test-IsZenithBuildLine ===" -ForegroundColor Cyan
# The ONE regex both the measure/verify classifier and the flake retry read --
# see the function's own comment on why a second copy would drift.

Assert-That 'matches a real build gate line' {
    Test-IsZenithBuildLine 'pwsh -NoProfile -File Tools\zenith.ps1 build Zenithmon --headless'
}

Assert-That 'does NOT match a unit-gate line' {
    -not (Test-IsZenithBuildLine 'pwsh -NoProfile -ExecutionPolicy Bypass -File Tools\run_unit_gate.ps1 -Exe Games\Zenithmon\Build\output\win64\null_vs2022_debug_win64_true\zenithmon.exe -Game Zenithmon -TimeoutSec 600')
}

Assert-That 'does NOT match Tooling''s npm test line -- this is the line the flake retry exists for' {
    -not (Test-IsZenithBuildLine 'pwsh -NoProfile -Command "Set-Location Tools/Vault; npm test"')
}

Write-Host "`n=== Find-CompiledObjectPath ===" -ForegroundColor Cyan
# The .obj-presence authority that replaced the by-name build-log grep.
# Exercised against synthetic obj trees rather than a real build -- the
# SHAPE under test is `Games/<Name>/Build/obj/win64/<config>/<leaf>.obj` for a
# game file and a broad `Build/obj/*/win64/<config>/<leaf>.obj` search for a
# shared-engine-tree one, both confirmed against the real checkout's actual
# build/obj layout while writing this check.

function New-ObjFixtureRoot {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("tickgates_obj_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    return $dir
}

Assert-That 'finds a game-project .obj at Games/<Name>/Build/obj/win64/<config>/<leaf>.obj' {
    $fixture = New-ObjFixtureRoot
    try {
        $objDir = Join-Path $fixture 'Games/Zenithmon/Build/obj/win64/null_vs2022_debug_win64_true'
        New-Item -ItemType Directory -Path $objDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $objDir 'ZM_Brand_New.obj') -Value 'fake' -Encoding utf8
        $built = @([PSCustomObject]@{ Game = 'Zenithmon'; Leaf = 'null_vs2022_debug_win64_true' })
        $hit = Find-CompiledObjectPath -RepoRoot $fixture -SourcePath 'Games/Zenithmon/Source/ZM_Brand_New.cpp' -BuiltConfigs $built
        ($null -ne $hit) -and ($hit -like '*ZM_Brand_New.obj')
    } finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'reports $null for a game-project file with no matching .obj -- the false-positive guard' {
    $fixture = New-ObjFixtureRoot
    try {
        $built = @([PSCustomObject]@{ Game = 'Zenithmon'; Leaf = 'null_vs2022_debug_win64_true' })
        $null -eq (Find-CompiledObjectPath -RepoRoot $fixture -SourcePath 'Games/Zenithmon/Source/ZM_Never_Compiled.cpp' -BuiltConfigs $built)
    } finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a game file falls back to every built leaf when no build line named ITS game' {
    # Should not happen for a coherent gate list, but a Tooling-style diff that
    # somehow carries a Games/ file alongside an `engine`-only build line must
    # not report unconditionally uncompiled just because the map has no entry
    # for that exact game.
    $fixture = New-ObjFixtureRoot
    try {
        $objDir = Join-Path $fixture 'Games/Combat/Build/obj/win64/vulkan_vs2022_debug_win64_true'
        New-Item -ItemType Directory -Path $objDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $objDir 'Combat_New.obj') -Value 'fake' -Encoding utf8
        $built = @([PSCustomObject]@{ Game = $null; Leaf = 'vulkan_vs2022_debug_win64_true' })
        $hit = Find-CompiledObjectPath -RepoRoot $fixture -SourcePath 'Games/Combat/Source/Combat_New.cpp' -BuiltConfigs $built
        $null -ne $hit
    } finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'finds a shared-engine-tree .obj by searching every lib under Build/obj' {
    $fixture = New-ObjFixtureRoot
    try {
        $objDir = Join-Path $fixture 'Build/obj/zenithflux/win64/null_vs2022_debug_win64_true'
        New-Item -ItemType Directory -Path $objDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $objDir 'Flux_BrandNew.obj') -Value 'fake' -Encoding utf8
        $built = @([PSCustomObject]@{ Game = $null; Leaf = 'null_vs2022_debug_win64_true' })
        $hit = Find-CompiledObjectPath -RepoRoot $fixture -SourcePath 'Zenith/Flux/Flux_BrandNew.cpp' -BuiltConfigs $built
        ($null -ne $hit) -and ($hit -like '*Flux_BrandNew.obj')
    } finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a shared-engine-tree miss (wrong config leaf present) reports $null, not a false positive' {
    $fixture = New-ObjFixtureRoot
    try {
        $objDir = Join-Path $fixture 'Build/obj/zenithflux/win64/vulkan_vs2022_debug_win64_true'
        New-Item -ItemType Directory -Path $objDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $objDir 'Flux_BrandNew.obj') -Value 'fake' -Encoding utf8
        # This phase only built the Null config -- the Vulkan .obj above must
        # not be treated as evidence for it.
        $built = @([PSCustomObject]@{ Game = $null; Leaf = 'null_vs2022_debug_win64_true' })
        $null -eq (Find-CompiledObjectPath -RepoRoot $fixture -SourcePath 'Zenith/Flux/Flux_BrandNew.cpp' -BuiltConfigs $built)
    } finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host "`n=== Write-Result refuses a green verdict with zero steps ===" -ForegroundColor Cyan
# Probes Write-Result directly against the REAL $resultPath this file's own
# dot-source already computed for -Key 'TEST-0' (.zagent/ is gitignored
# scratch, and TEST-0 is not a live ticket key). $script:Steps /
# $script:PlannedLines / $script:DeferredLines / $script:Flakes are reset
# before and after every probe so none of this leaks into another test.

function Invoke-WriteResultProbe {
    param([scriptblock]$Setup, [int]$Code, [string]$Verdict, $Extra)
    $script:Steps.Clear(); $script:PlannedLines.Clear(); $script:DeferredLines.Clear(); $script:Flakes.Clear()
    if ($Setup) { & $Setup }
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    Write-Result $Code $Verdict $Extra
    $json = Get-Content -LiteralPath $resultPath -Raw -Encoding utf8 | ConvertFrom-Json
    $script:Steps.Clear(); $script:PlannedLines.Clear(); $script:DeferredLines.Clear(); $script:Flakes.Clear()
    return $json
}

Assert-That 'a $Code of 0 with ZERO steps is rewritten to exit 8, not reported green' {
    # This is the Tooling-category bug itself: both $buildLines and $unitLines
    # classify empty, nothing runs, and the code used to fall through to
    # `Write-Result 0 'all gates green'` regardless.
    $json = Invoke-WriteResultProbe -Code 0 -Verdict 'all gates green' -Extra $null
    ($json.exitCode -eq 8) -and ($json.verdict -match 'no gate lines') -and ($json.state -eq 'complete')
}

Assert-That 'the zero-steps exit code is PINNED at 8, not 6 -- 6 is zagent''s circuit-breaker-open' {
    # zagent's own exit ladder (root CLAUDE.md; branched on throughout
    # .claude/commands/tick.md) is 0/1/3/4/5/6/7, and 6 there already means
    # CIRCUIT BREAKER OPEN. A tick reads both ladders in the same step, so
    # this script reusing 6 for "ran no gate lines" would make one number mean
    # two unrelated things depending which command printed it -- exactly what
    # "exit codes are the interface" is supposed to prevent. This assertion
    # exists so a future renumber back to 6 (it IS free within this file's own
    # ladder, which is how it got picked the first time) has to be deliberate.
    $json = Invoke-WriteResultProbe -Code 0 -Verdict 'all gates green' -Extra $null
    ($json.exitCode -eq 8) -and ($json.exitCode -ne 6)
}

Assert-That 'a $Code of 0 with at least one real step is left alone -- a genuine green stays green' {
    $json = Invoke-WriteResultProbe -Setup {
        $script:Steps.Add([PSCustomObject]@{ Line = 'pwsh -NoProfile -File Tools\zenith.ps1 build Combat --headless'; Label = 'build'; ExitCode = 0; Seconds = 1.2 })
    } -Code 0 -Verdict 'all gates green' -Extra $null
    ($json.exitCode -eq 0) -and ($json.verdict -eq 'all gates green')
}

Assert-That 'a NON-zero code with zero steps is untouched -- the guard is scoped to $Code -eq 0 only' {
    # exit 5 ("could not read the gate list") is already an honest red with
    # nothing having run; rewriting it would just be noise.
    $json = Invoke-WriteResultProbe -Code 5 -Verdict 'could not read the gate list; nothing ran' -Extra $null
    ($json.exitCode -eq 5) -and ($json.verdict -eq 'could not read the gate list; nothing ran')
}

Write-Host "`n=== state reflects a crashed run, not a fabricated 'complete' ===" -ForegroundColor Cyan

Assert-That 'state defaults to complete when Extra says nothing about it' {
    $json = Invoke-WriteResultProbe -Setup {
        $script:Steps.Add([PSCustomObject]@{ Line = 'x'; Label = 'build'; ExitCode = 0; Seconds = 0.1 })
    } -Code 0 -Verdict 'all gates green' -Extra $null
    $json.state -eq 'complete'
}

Assert-That 'Extra.state overrides the default -- the internal-error catch block uses exactly this to report state: error' {
    $json = Invoke-WriteResultProbe -Code 1 -Verdict 'internal error: boom' -Extra @{ state = 'error' }
    $json.state -eq 'error'
}

Assert-That 'the catch block itself passes state = ''error'' to Write-Result, not the default' {
    # A live throw is not exercised here (this script never runs the real
    # try/catch -- see the -NoRun header comment); this pins the SOURCE wiring
    # instead, which is what a future edit could silently drop.
    $srcPath = Join-Path $PSScriptRoot 'tick_gates.ps1'
    $src = Get-Content -LiteralPath $srcPath -Raw
    $catchIdx = $src.IndexOf('catch {')
    ($catchIdx -ge 0) -and ($src.Substring($catchIdx) -match "Write-Result 1 ""internal error[\s\S]{0,200}state\s*=\s*'error'")
}

Write-Host "`n=== PlannedLines is populated before any gate line runs in measure ===" -ForegroundColor Cyan

Assert-That 'measure sets PlannedLines/DeferredLines BEFORE the build loop, not after it' {
    # This script deliberately never runs a real build (see the -NoRun header
    # comment), so an actual mid-build exception is not reproducible here.
    # What IS checkable without running anything is the ORDERING the fix
    # depends on: `$script:PlannedLines.Clear()` must appear, in the source,
    # before the build loop's Invoke-GateLine call -- if a future edit moves
    # the population back down (where it used to live, after the build and
    # both file-provenance checks), this catches it without needing a live
    # build to prove `skipped` went wrong.
    $srcPath = Join-Path $PSScriptRoot 'tick_gates.ps1'
    $src = Get-Content -LiteralPath $srcPath -Raw
    $measureNeedle = 'if ($Phase -eq ''measure'' -or $Phase -eq ''all'')'
    $plannedNeedle = '$script:PlannedLines.Clear()'
    $buildLoopNeedle = 'Invoke-GateLine -Line $line -Label ''build'''
    $measureStart = $src.IndexOf($measureNeedle)
    $plannedIdx = $src.IndexOf($plannedNeedle, [Math]::Max($measureStart, 0))
    $buildLoopIdx = $src.IndexOf($buildLoopNeedle, [Math]::Max($measureStart, 0))
    ($measureStart -ge 0) -and ($plannedIdx -ge 0) -and ($buildLoopIdx -ge 0) -and ($plannedIdx -lt $buildLoopIdx)
}

Write-Host "`n=== Invoke-GateLineFlakeAware (producer half) ===" -ForegroundColor Cyan
# Runs REAL, trivial child processes (a bare `exit 0` / `exit 1` wrapper) --
# not an actual repo gate -- so this exercises the retry mechanism itself
# without the "never run a gate from a test" hazard the exe-sweep note above
# describes (a real gate can hold locks and run for minutes; these do
# neither). $runDir must exist because Invoke-GateLine writes its wrapper
# there, and -NoRun skipped creating it; cleaned up afterward either way.

New-Item -ItemType Directory -Path $runDir -Force | Out-Null

Assert-That 'a line that goes red and then green on rerun is recorded as a flake, but the step stays red' {
    $script:Steps.Clear(); $script:Flakes.Clear()
    try {
        $counterFile = Join-Path ([System.IO.Path]::GetTempPath()) ("tickgates_flake_" + [guid]::NewGuid().ToString('N') + '.marker')
        $line = "if (Test-Path -LiteralPath '$counterFile') { exit 0 } else { Set-Content -LiteralPath '$counterFile' -Value '1'; exit 1 }"
        $step = Invoke-GateLineFlakeAware -Line $line -Label 'test-flake'
        # The VERDICT never moves: $step is still the FIRST run (red), even
        # though $script:Flakes shows the rerun went green.
        ($step.ExitCode -eq 1) -and
        ($script:Flakes.Count -eq 1) -and
        ($script:Flakes[0].firstExit -eq 1) -and
        ($script:Flakes[0].rerunExit -eq 0) -and
        ($script:Steps.Count -eq 2)   # first attempt + the one rerun, both recorded
    } finally {
        Remove-Item -LiteralPath $counterFile -Force -ErrorAction SilentlyContinue
        $script:Steps.Clear(); $script:Flakes.Clear()
    }
}

Assert-That 'a line that is red BOTH times is still just one flake entry, no infinite retry' {
    $script:Steps.Clear(); $script:Flakes.Clear()
    $step = Invoke-GateLineFlakeAware -Line 'exit 1' -Label 'test-deterministic-red'
    $ok = ($step.ExitCode -eq 1) -and ($script:Flakes.Count -eq 1) -and
        ($script:Flakes[0].firstExit -eq 1) -and ($script:Flakes[0].rerunExit -eq 1) -and
        ($script:Steps.Count -eq 2)
    $script:Steps.Clear(); $script:Flakes.Clear()
    $ok
}

Assert-That 'a line that is green first time is never retried -- no flake entry, one step only' {
    $script:Steps.Clear(); $script:Flakes.Clear()
    $step = Invoke-GateLineFlakeAware -Line 'exit 0' -Label 'test-green'
    $ok = ($step.ExitCode -eq 0) -and ($script:Flakes.Count -eq 0) -and ($script:Steps.Count -eq 1)
    $script:Steps.Clear(); $script:Flakes.Clear()
    $ok
}

Assert-That 'a `zenith.ps1 build` line is NEVER retried, even red -- a build failure is not this flake class' {
    $script:Steps.Clear(); $script:Flakes.Clear()
    # The literal text just needs to MATCH Test-IsZenithBuildLine's regex; it
    # does not need to be a real build invocation (and must not be one, or
    # this test would take minutes and touch the real build tree).
    $line = "# pretend: Tools\zenith.ps1 build FakeGame --headless`nexit 1"
    $step = Invoke-GateLineFlakeAware -Line $line -Label 'test-build-red'
    $ok = ($step.ExitCode -eq 1) -and ($script:Flakes.Count -eq 0) -and ($script:Steps.Count -eq 1)
    $script:Steps.Clear(); $script:Flakes.Clear()
    $ok
}

Remove-Item -LiteralPath $runDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ''
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) {
    Write-Host 'PASS' -ForegroundColor Green
    exit 0
}
Write-Host "$($script:failures) assertion(s) failed" -ForegroundColor Red
exit 1
