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

Write-Host "`n=== the planned/deferred split (composed from the two classifying predicates) ===" -ForegroundColor Cyan
# `$script:DeferredLines` itself is populated ONLY inside measure's phase body,
# which sits behind `if ($NoRun) { return }` at the bottom of tick_gates.ps1
# -- reaching it means actually running a phase (a real `zagent gates` call,
# a real regen/build), which this harness deliberately never does (see the
# -NoRun header comment). That is out of reach here without a real gate run.
#
# What IS reachable without running anything is the classification measure's
# own code applies to decide planned vs. deferred: a gate line is PLANNED iff
# it is a build line (Test-IsZenithBuildLine, already exercised above) or a
# unit line (the literal regex 'run_unit_gate\.ps1'); everything else is what
# would be deferred. This does NOT prove $script:DeferredLines is correctly
# populated at runtime -- only that the two predicates it is built from
# correctly separate a synthetic gate list. Recorded as a shortfall in this
# ticket's report rather than silently skipped.

Assert-That 'the literal unit-line regex the composition below assumes is still in the source' {
    # Pins the literal string the next assertion re-derives, so a change to
    # the real regex in tick_gates.ps1 (around the `$unitLines =` line) makes
    # THIS fail rather than letting the composition drift from the source
    # silently.
    $srcPath = Join-Path $PSScriptRoot 'tick_gates.ps1'
    $src = Get-Content -LiteralPath $srcPath -Raw
    $src -match [regex]::Escape("'run_unit_gate\.ps1'")
}

Assert-That 'a build line and a unit line both classify as PLANNED; a third (Tooling''s npm test) line is neither -- the shape measure defers' {
    $gateLines = @(
        'pwsh -NoProfile -File Tools\zenith.ps1 build Zenithmon --headless',
        'pwsh -NoProfile -ExecutionPolicy Bypass -File Tools\run_unit_gate.ps1 -Exe x -Game Zenithmon',
        'pwsh -NoProfile -Command "Set-Location Tools/Vault; npm test"'
    )
    $unitRegex = 'run_unit_gate\.ps1'
    $planned = @($gateLines | Where-Object { (Test-IsZenithBuildLine $_) -or ($_ -match $unitRegex) })
    $wouldDefer = @($gateLines | Where-Object { -not ((Test-IsZenithBuildLine $_) -or ($_ -match $unitRegex)) })
    ($planned.Count -eq 2) -and ($wouldDefer.Count -eq 1) -and ($wouldDefer[0] -like '*npm test*')
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

Write-Host "`n=== Get-AutomatedRegistryCount ===" -ForegroundColor Cyan
# The paren-anchored regex is load-bearing -- doc_lint.ps1 check C7's own
# reasoning, mirrored here rather than re-derived: a bare
# `ZENITH_AUTOMATED_TEST_REGISTER` (no trailing paren) also matches a
# paren-less MENTION in a comment. Three real ones live in
# Games/Zenithmon/Tests/ZM_AutoTests_TrainerSight.cpp today -- this fixture
# reproduces that shape rather than depending on it staying there.
# -RepoRoot is a parameter (added alongside the `registry` field this ticket
# adds), the same reasoning Get-CreatedSourceFiles's own header comment gives:
# a helper only testable against the live checkout is a helper nobody tests.

function New-RegistryFixtureRoot {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("tickgates_registry_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    return $dir
}

Assert-That 'counts a real call site but NOT a paren-less mention in a comment' {
    # Fails the moment the regex loses its trailing `\(` (e.g. reverting to a
    # bare `ZENITH_AUTOMATED_TEST_REGISTER` grep) -- that would count BOTH
    # lines below instead of only the one that is an actual call site.
    $fixture = New-RegistryFixtureRoot
    try {
        $dir = Join-Path $fixture 'Games/Probe/Tests'
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $dir 'Probe.cpp') -Value @(
            '// ZENITH_AUTOMATED_TEST_REGISTER: this comment only MENTIONS the macro',
            'ZENITH_AUTOMATED_TEST_REGISTER(g_xProbeTest);'
        ) -Encoding utf8
        (Get-AutomatedRegistryCount -Game 'Probe' -RepoRoot $fixture) -eq 1
    } finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'sums call sites across every .cpp under the game''s Tests directory' {
    $fixture = New-RegistryFixtureRoot
    try {
        $dir = Join-Path $fixture 'Games/Probe/Tests'
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $dir 'A.cpp') -Value 'ZENITH_AUTOMATED_TEST_REGISTER(g_xA);' -Encoding utf8
        Set-Content -LiteralPath (Join-Path $dir 'B.cpp') -Value @(
            'ZENITH_AUTOMATED_TEST_REGISTER(g_xB1);',
            'ZENITH_AUTOMATED_TEST_REGISTER(g_xB2);'
        ) -Encoding utf8
        (Get-AutomatedRegistryCount -Game 'Probe' -RepoRoot $fixture) -eq 3
    } finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'returns $null -- NOT 0 -- for a game with no Tests directory at all' {
    # $null means "never measured this game" (no Tests dir to read); 0 means
    # "measured, registered nothing". Collapsing the two would make an
    # unmeasured game indistinguishable from one whose registry is empty.
    $fixture = New-RegistryFixtureRoot
    try {
        $result = Get-AutomatedRegistryCount -Game 'NoSuchGame' -RepoRoot $fixture
        $null -eq $result
    } finally { Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'returns 0, not $null, when the Tests directory exists but registers nothing' {
    $fixture = New-RegistryFixtureRoot
    try {
        $dir = Join-Path $fixture 'Games/Probe/Tests'
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $dir 'Empty.cpp') -Value '// no registrations here' -Encoding utf8
        $result = Get-AutomatedRegistryCount -Game 'Probe' -RepoRoot $fixture
        ($null -ne $result) -and ($result -eq 0)
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
    $script:Steps.Clear(); $script:PlannedLines.Clear(); $script:DeferredLines.Clear(); $script:Flakes.Clear(); $script:RegistryCounts.Clear()
    if ($Setup) { & $Setup }
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    Write-Result $Code $Verdict $Extra
    $json = Get-Content -LiteralPath $resultPath -Raw -Encoding utf8 | ConvertFrom-Json
    $script:Steps.Clear(); $script:PlannedLines.Clear(); $script:DeferredLines.Clear(); $script:Flakes.Clear(); $script:RegistryCounts.Clear()
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
    # Tools/zagent/protocol/TICK.md) is 0/1/3/4/5/6/7, and 6 there already means
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

Write-Host "`n=== Write-Result emits the registry map (added by this ticket) ===" -ForegroundColor Cyan
# `registry` is ADDITIVE to the payload -- every existing field above is
# untouched. Probed the same way as `deferred`/`flakes`: through
# $script:RegistryCounts, which the registry-count loop near the bottom of
# tick_gates.ps1 populates and Write-Result now serialises verbatim.

Assert-That 'registry is present and an EMPTY map when no game was measured -- not an absent key' {
    # Fails if the field is ever made conditional (e.g. `if
    # ($script:RegistryCounts.Count -gt 0) { registry = ... }`), which would
    # make the key vanish instead of reporting an empty map -- indistinguishable
    # from an older script that never wrote it at all.
    $json = Invoke-WriteResultProbe -Setup {
        $script:Steps.Add([PSCustomObject]@{ Line = 'x'; Label = 'build'; ExitCode = 0; Seconds = 0.1 })
    } -Code 0 -Verdict 'all gates green' -Extra $null
    ($null -ne $json.registry) -and (@($json.registry.PSObject.Properties).Count -eq 0)
}

Assert-That 'registry carries a populated per-game count through to the JSON' {
    $json = Invoke-WriteResultProbe -Setup {
        $script:Steps.Add([PSCustomObject]@{ Line = 'x'; Label = 'build'; ExitCode = 0; Seconds = 0.1 })
        $script:RegistryCounts['Zenithmon'] = 73
    } -Code 0 -Verdict 'all gates green' -Extra $null
    $json.registry.Zenithmon -eq 73
}

Assert-That 'registry carries a $null entry through -- a measured game whose Tests directory does not exist' {
    $json = Invoke-WriteResultProbe -Setup {
        $script:Steps.Add([PSCustomObject]@{ Line = 'x'; Label = 'build'; ExitCode = 0; Seconds = 0.1 })
        $script:RegistryCounts['NoTestsGame'] = $null
    } -Code 0 -Verdict 'all gates green' -Extra $null
    (@($json.registry.PSObject.Properties.Name) -contains 'NoTestsGame') -and ($null -eq $json.registry.NoTestsGame)
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

Write-Host "`n=== Get-GateList -ReuseGateList ===" -ForegroundColor Cyan
# WP4: skip the `zagent gates` shell-out and read the ticket's own recorded
# gates.json instead. There is no existing precedent in this suite for
# stubbing `zagent` -- every other test's own header warns never to call the
# REAL one -- so this builds a fake one: a temp dir holding a `zagent.cmd`
# that writes a marker file if invoked, prepended to PATH. -ReuseGateList
# reaching the shell-out at all would be the regression; the marker's
# absence is the proof it did not.
New-Item -ItemType Directory -Path $runDir -Force | Out-Null

function New-FakeZagentOnPath {
    param([string]$MarkerPath)
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("tickgates_fakezagent_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $cmdPath = Join-Path $dir 'zagent.cmd'
    Set-Content -LiteralPath $cmdPath -Value @(
        '@echo off',
        "echo marker > ""$MarkerPath"""
        'exit /b 1'
    ) -Encoding ascii
    return $dir
}

Assert-That '-ReuseGateList reads gates.json directly -- the zagent shell-out marker never appears' {
    $marker = Join-Path ([System.IO.Path]::GetTempPath()) ("tickgates_marker_" + [guid]::NewGuid().ToString('N') + '.txt')
    $fakeDir = New-FakeZagentOnPath -MarkerPath $marker
    $priorPath = $env:PATH
    $gatesJsonPath = Join-Path $runDir 'gates.json'
    try {
        $env:PATH = "$fakeDir;$priorPath"
        Set-Content -LiteralPath $gatesJsonPath -Value (@{ gates = @('pwsh -NoProfile -Command "exit 0"', 'pwsh -NoProfile -Command "exit 0"') } | ConvertTo-Json) -Encoding utf8

        # Exercises the SAME function both real call sites (measure's initial
        # read, verify's re-derive) invoke with -ReuseGateList:$ReuseGateList
        # -- there is nothing left to factor out beyond this switch, since
        # Get-GateList was already the one place the gate-list decision lives.
        $lines = Get-GateList -ReuseGateList

        (@($lines).Count -eq 2) -and (-not (Test-Path -LiteralPath $marker))
    }
    finally {
        $env:PATH = $priorPath
        Remove-Item -LiteralPath $fakeDir -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $marker -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $gatesJsonPath -Force -ErrorAction SilentlyContinue
    }
}

Assert-That '-ReuseGateList with NO gates.json on disk returns $null -- the same shape the exit-5 path checks' {
    $gatesJsonPath = Join-Path $runDir 'gates.json'
    Remove-Item -LiteralPath $gatesJsonPath -Force -ErrorAction SilentlyContinue
    $null -eq (Get-GateList -ReuseGateList)
}

Write-Host "`n=== Get-WorkingTreeSha ===" -ForegroundColor Cyan
# `git write-tree` from a temp copy of the real index -- never the real index
# itself. Confirmed two ways: the result equals a real commit's tree, and the
# real index file is byte-identical before and after the call.

function Test-WorkingTreeShaScenario {
    <#
      Dirties $RepoPath (a modified tracked file + an untracked one), calls
      Get-WorkingTreeSha, and checks it against an INDEPENDENTLY-produced
      tree: `git add -A; git commit`, then `rev-parse HEAD^{tree}` -- done
      AFTER the byte-identical check below, so that check is not proving the
      function against its own side effect.
    #>
    param([Parameter(Mandatory)][string]$RepoPath)

    Add-Content -LiteralPath (Join-Path $RepoPath 'Games/Probe/Tests/Existing.cpp') -Value '// dirtied for tree-sha test'
    Set-Content -LiteralPath (Join-Path $RepoPath 'Games/Probe/Tests/Untracked_TreeSha.cpp') -Value '// untracked' -Encoding utf8

    $indexOut = (& git -C $RepoPath rev-parse --git-path index 2>$null)
    $indexOut = ([string]$indexOut).Trim()
    $realIndex = if ([System.IO.Path]::IsPathRooted($indexOut)) { $indexOut } else { Join-Path $RepoPath $indexOut }

    $beforeHash = if (Test-Path -LiteralPath $realIndex) { (Get-FileHash -LiteralPath $realIndex -Algorithm SHA256).Hash } else { $null }

    $sha = Get-WorkingTreeSha -RepoRoot $RepoPath

    $afterHash = if (Test-Path -LiteralPath $realIndex) { (Get-FileHash -LiteralPath $realIndex -Algorithm SHA256).Hash } else { $null }
    $indexUnchanged = ($beforeHash -eq $afterHash)

    & git -C $RepoPath add -A 2>$null | Out-Null
    & git -C $RepoPath commit -m 'tree-sha test commit' --quiet 2>$null | Out-Null
    $expected = (& git -C $RepoPath rev-parse 'HEAD^{tree}' 2>$null)
    $expected = ([string]$expected).Trim()

    return ($null -ne $sha) -and ($sha -eq $expected) -and $indexUnchanged
}

Assert-That 'equals `git write-tree` of a real commit, and the real index is byte-identical before/after' {
    $repo = New-GitFixture
    try {
        Test-WorkingTreeShaScenario -RepoPath $repo
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'the SAME check from inside a LINKED WORKTREE -- git-path index there is ABSOLUTE, not .git/index' {
    $repo = New-GitFixture
    $worktree = Join-Path ([System.IO.Path]::GetTempPath()) ("tickgates_wt_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    try {
        & git -C $repo worktree add --detach $worktree HEAD 2>$null | Out-Null
        Test-WorkingTreeShaScenario -RepoPath $worktree
    }
    finally {
        & git -C $repo worktree remove --force $worktree 2>$null | Out-Null
        Remove-Item -LiteralPath $worktree -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "`n=== Select-SweepTargets (pure -- never touches a real process) ===" -ForegroundColor Cyan
# ** NEVER CALL Invoke-ExeSweep FROM A TEST (see this file's own warning
# above). This asserts the SELECTION only, against synthetic process objects.

Assert-That 'requires name AND path -- a name match under an unrelated path is not swept' {
    # PLAIN ASSIGNMENT, never `@(...)` around the call -- Select-SweepTargets
    # returns `, $result.ToArray()`, the same "carry an empty array through
    # intact" contract Get-CreatedSourceFiles/Get-CreatedIncludeFiles use
    # (see their own header comments). Wrapping the CALL in `@()` re-wraps
    # that envelope: the function already emits ONE pipeline object (the
    # array), so `@(...)` around it produces a 1-element array whose element
    # IS that array -- `.Count` is then always 1, never 0, no matter how many
    # real hits there were. Measured while writing this test.
    $procs = @(
        [PSCustomObject]@{ Name = 'zenithmon'; Path = 'C:\dev\Zenith-lanes\ZM\Games\Zenithmon\Build\output\win64\x\zenithmon.exe' }
    )
    $hits = Select-SweepTargets -Processes $procs -RepoRoot 'C:\dev\Zenith' -Names @('zenithmon')
    $hits.Count -eq 0
}

Assert-That 'a directory-boundary sibling (Zenith-lanes) is NOT matched by root Zenith -- prefix must respect the boundary' {
    # The whole point of the path scope: `C:\dev\Zenith-lanes\ZM\...` starts
    # with the literal string `C:\dev\Zenith`, but is not UNDER it.
    $procs = @(
        [PSCustomObject]@{ Name = 'combat'; Path = 'C:\dev\Zenith-lanes\ZM\Games\Combat\Build\output\win64\x\combat.exe' },
        [PSCustomObject]@{ Name = 'combat'; Path = 'C:\dev\Zenith\Games\Combat\Build\output\win64\x\combat.exe' }
    )
    $hits = Select-SweepTargets -Processes $procs -RepoRoot 'C:\dev\Zenith' -Names @('combat')
    ($hits.Count -eq 1) -and ($hits[0].Path -eq 'C:\dev\Zenith\Games\Combat\Build\output\win64\x\combat.exe')
}

Assert-That 'a $null path is SKIPPED -- never treated as a match' {
    $procs = @([PSCustomObject]@{ Name = 'zenithmon'; Path = $null })
    $hits = Select-SweepTargets -Processes $procs -RepoRoot 'C:\dev\Zenith' -Names @('zenithmon')
    $hits.Count -eq 0
}

Assert-That 'a path under the root with the WRONG name is skipped' {
    $procs = @([PSCustomObject]@{ Name = 'msbuild'; Path = 'C:\dev\Zenith\Build\output\msbuild.exe' })
    $hits = Select-SweepTargets -Processes $procs -RepoRoot 'C:\dev\Zenith' -Names @('zenithmon', 'combat')
    $hits.Count -eq 0
}

Assert-That 'the repo root itself (no trailing separator) still matches -- a game exe could sit directly at the root' {
    $procs = @([PSCustomObject]@{ Name = 'zenithmon'; Path = 'C:\dev\Zenith' })
    $hits = Select-SweepTargets -Processes $procs -RepoRoot 'C:\dev\Zenith' -Names @('zenithmon')
    $hits.Count -eq 1
}

Write-Host "`n=== the build slot (Tools/build_lock.ps1, dot-sourced transitively via tick_gates.ps1) ===" -ForegroundColor Cyan
# Enter-ZenithBuildSlot / Exit-ZenithBuildSlot are defined by build_lock.ps1,
# which tick_gates.ps1 dot-sources unconditionally near its own top (before
# the -NoRun gate) -- so they are already in scope here via this file's own
# `. tick_gates.ps1 -NoRun` at the top. These tests acquire the REAL
# machine-wide `Local\ZenithBuildSlot` mutex, so every one of them releases
# what it acquired in a `finally`, and the diagnostics-file tests clean up
# after themselves in THIS repo's own .git directory (Enter-ZenithBuildSlot's
# repoRoot always resolves to wherever Tools/build_lock.ps1 itself lives,
# never a test fixture -- it is a real machine-wide resource by design).
#
# A holder on a genuinely separate OS thread is required for the blocking
# tests -- NOT a PowerShell scriptblock run via [Threading.Thread] (that
# throws "no Runspace available": a scriptblock delegate always needs the
# engine, even calling only .NET types), and NOT a second Mutex object
# acquired on THIS SAME thread (a named Windows mutex is reentrant for its
# OWNING THREAD regardless of which .NET Mutex object represents it, so a
# same-thread "holder" would let this process's own Enter-ZenithBuildSlot
# re-acquire immediately -- measured while writing this test). Add-Type
# compiles a tiny C# holder whose Run() is invoked as a real ThreadStart
# delegate on its own OS thread, sidestepping both problems.
Add-Type -TypeDefinition @'
using System.Threading;
public class ZenithTestMutexHolder {
    private string _name;
    private ManualResetEvent _ready;
    private ManualResetEvent _release;
    public ZenithTestMutexHolder(string name, ManualResetEvent ready, ManualResetEvent release) {
        _name = name; _ready = ready; _release = release;
    }
    public void Run() {
        var m = new Mutex(false, _name);
        m.WaitOne();
        _ready.Set();
        _release.WaitOne();
        m.ReleaseMutex();
    }
}
'@ -ErrorAction SilentlyContinue

function Start-ZenithTestMutexHolderThread {
    # Returns @{ Thread; Ready; Release } -- Release.Set() then Thread.Join()
    # to tear down.
    $ready = New-Object System.Threading.ManualResetEvent($false)
    $release = New-Object System.Threading.ManualResetEvent($false)
    $holder = New-Object ZenithTestMutexHolder('Local\ZenithBuildSlot', $ready, $release)
    $del = [Delegate]::CreateDelegate([System.Threading.ThreadStart], $holder, 'Run')
    $thread = [System.Threading.Thread]::new($del)
    $thread.IsBackground = $true
    $thread.Start()
    $ready.WaitOne(10000) | Out-Null
    return [PSCustomObject]@{ Thread = $thread; Ready = $ready; Release = $release }
}

function Stop-ZenithTestMutexHolderThread {
    param($Holder)
    $Holder.Release.Set() | Out-Null
    $Holder.Thread.Join(10000) | Out-Null
}

Assert-That 'no re-acquire under ZENITH_BUILD_SLOT_HELD=1, even while the mutex is held elsewhere -- returns an INHERITED handle immediately' {
    $holder = Start-ZenithTestMutexHolderThread
    $priorHeld = $env:ZENITH_BUILD_SLOT_HELD
    try {
        $env:ZENITH_BUILD_SLOT_HELD = '1'
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $h = Enter-ZenithBuildSlot -WaitMinutes 1 -PollMilliseconds 5000
        $sw.Stop()
        $ok = ($null -ne $h) -and ($h.Inherited -eq $true) -and ($null -eq $h.Mutex) -and ($sw.ElapsedMilliseconds -lt 4000)
        Exit-ZenithBuildSlot $h   # must be a safe no-op -- never touches the mutex the holder owns
        $ok
    }
    finally {
        if ($null -eq $priorHeld) { Remove-Item Env:\ZENITH_BUILD_SLOT_HELD -ErrorAction SilentlyContinue } else { $env:ZENITH_BUILD_SLOT_HELD = $priorHeld }
        Stop-ZenithTestMutexHolderThread $holder
    }
}

Assert-That 'a bounded wait with a tiny -WaitMinutes and small -PollMilliseconds returns $null, not a hang' {
    $holder = Start-ZenithTestMutexHolderThread
    try {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $h = Enter-ZenithBuildSlot -WaitMinutes 0.02 -PollMilliseconds 200
        $sw.Stop()
        ($null -eq $h) -and ($sw.ElapsedMilliseconds -lt 10000)
    }
    finally {
        Stop-ZenithTestMutexHolderThread $holder
    }
}

Assert-That 'an ABANDONED mutex (holder killed) counts as ACQUIRED on the next WaitOne' {
    $marker = Join-Path ([System.IO.Path]::GetTempPath()) ("tickgates_abandon_" + [guid]::NewGuid().ToString('N') + '.marker')
    $childScriptPath = Join-Path ([System.IO.Path]::GetTempPath()) ("tickgates_abandon_child_" + [guid]::NewGuid().ToString('N') + '.ps1')
    Set-Content -LiteralPath $childScriptPath -Value @(
        '$m = New-Object System.Threading.Mutex($false, ''Local\ZenithBuildSlot'')',
        '$null = $m.WaitOne()',
        "Set-Content -LiteralPath '$marker' -Value '1'",
        'Start-Sleep -Seconds 300'
    ) -Encoding utf8
    $child = $null
    $handle = $null
    try {
        $child = Start-Process -FilePath 'pwsh' -ArgumentList @('-NoProfile', '-File', $childScriptPath) -PassThru -WindowStyle Hidden
        $deadline = (Get-Date).AddSeconds(15)
        while (-not (Test-Path -LiteralPath $marker) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
        $markerSeen = Test-Path -LiteralPath $marker
        Stop-Process -Id $child.Id -Force -ErrorAction SilentlyContinue
        $child.WaitForExit(10000) | Out-Null

        $handle = Enter-ZenithBuildSlot -WaitMinutes 1 -PollMilliseconds 200
        $markerSeen -and ($null -ne $handle) -and (-not $handle.Inherited)
    }
    finally {
        Exit-ZenithBuildSlot $handle
        if ($child -and -not $child.HasExited) { Stop-Process -Id $child.Id -Force -ErrorAction SilentlyContinue }
        Remove-Item -LiteralPath $marker, $childScriptPath -Force -ErrorAction SilentlyContinue
    }
}

function Invoke-BuildLockRunChild {
    <#
      Runs `pwsh -File Tools/build_lock.ps1 -Run <cmd> ...` as a CHILD process
      and returns its exit code. Does NOT pass the arguments through
      Start-Process's `-ArgumentList` (a string array) directly -- that array
      is joined into a raw command line WITHOUT quoting elements containing
      spaces, so a multi-word -Run value (e.g. 'exit 7') gets torn apart:
      `-Run` binds only the first word, and the remainder ('7') lands on the
      next declared parameter by POSITION (`-WaitMinutes`), silently -- no
      error, a completely different run. Measured while writing this test:
      `-Run 'exit 7'` this way returned exit code 0 (a bare `exit` inside the
      wrapper), not 7. The fix used everywhere else in this repo for exactly
      this class of problem (Invoke-GateLine's own header comment; build_lock.ps1's
      own -Run implementation) is to write the invocation to a wrapper .ps1
      and run THAT through `-File` -- parsed by the PowerShell parser, which
      quotes correctly, rather than reconstructed by naive array-joining.
    #>
    param(
        [Parameter(Mandatory)][string]$BuildLockPath,
        [Parameter(Mandatory)][string]$RunCommand,
        [double]$WaitMinutes,
        [int]$PollMilliseconds,
        [string]$RedirectStandardOutput
    )
    $outerWrapper = Join-Path ([System.IO.Path]::GetTempPath()) ('tickgates_buildlock_outer_' + [guid]::NewGuid().ToString('N') + '.ps1')
    $argsLine = "-Run '$($RunCommand -replace "'", "''")'"
    if ($PSBoundParameters.ContainsKey('WaitMinutes')) { $argsLine += " -WaitMinutes $WaitMinutes" }
    if ($PSBoundParameters.ContainsKey('PollMilliseconds')) { $argsLine += " -PollMilliseconds $PollMilliseconds" }
    $body = "& pwsh -NoProfile -File '$BuildLockPath' $argsLine`nexit `$LASTEXITCODE"
    Set-Content -LiteralPath $outerWrapper -Value $body -Encoding utf8
    try {
        $spArgs = @{
            FilePath     = 'pwsh'
            ArgumentList = @('-NoProfile', '-File', $outerWrapper)
            PassThru     = $true
            Wait         = $true
            WindowStyle  = 'Hidden'
        }
        if ($RedirectStandardOutput) { $spArgs['RedirectStandardOutput'] = $RedirectStandardOutput }
        $proc = Start-Process @spArgs
        return $proc.ExitCode
    }
    finally {
        Remove-Item -LiteralPath $outerWrapper -Force -ErrorAction SilentlyContinue
    }
}

Assert-That 'build_lock.ps1 -Run exits 11 while the slot is held elsewhere' {
    $holder = Start-ZenithTestMutexHolderThread
    try {
        $buildLockPath = Join-Path $repoRoot 'Tools/build_lock.ps1'
        $code = Invoke-BuildLockRunChild -BuildLockPath $buildLockPath -RunCommand 'exit 0' -WaitMinutes 0.02 -PollMilliseconds 200
        $code -eq 11
    }
    finally {
        Stop-ZenithTestMutexHolderThread $holder
    }
}

Assert-That 'build_lock.ps1 -Run propagates the wrapped command''s own exit code when the slot is free' {
    $buildLockPath = Join-Path $repoRoot 'Tools/build_lock.ps1'
    $code = Invoke-BuildLockRunChild -BuildLockPath $buildLockPath -RunCommand 'exit 7'
    $code -eq 7
}

Assert-That 'a Start-Process pwsh child spawned by -Run sees ZENITH_BUILD_SLOT_HELD=1 -- the handshake propagates by environment inheritance' {
    $buildLockPath = Join-Path $repoRoot 'Tools/build_lock.ps1'
    $outFile = Join-Path ([System.IO.Path]::GetTempPath()) ("tickgates_slotenv_" + [guid]::NewGuid().ToString('N') + '.out')
    try {
        $code = Invoke-BuildLockRunChild -BuildLockPath $buildLockPath -RunCommand 'Write-Output $env:ZENITH_BUILD_SLOT_HELD' -RedirectStandardOutput $outFile
        $out = if (Test-Path -LiteralPath $outFile) { (Get-Content -LiteralPath $outFile -Raw) } else { '' }
        ($code -eq 0) -and ($out -match '1')
    }
    finally {
        Remove-Item -LiteralPath $outFile -Force -ErrorAction SilentlyContinue
    }
}

Assert-That 'the diagnostics file exists while the slot is held and is gone after release' {
    $h = Enter-ZenithBuildSlot -WaitMinutes 1 -Reason 'Test_TickGates diagnostics probe'
    $existedWhileHeld = ($null -ne $h) -and ($null -ne $h.DiagPath) -and (Test-Path -LiteralPath $h.DiagPath)
    $diagPath = $h.DiagPath
    Exit-ZenithBuildSlot $h
    $goneAfterRelease = (-not [string]::IsNullOrEmpty($diagPath)) -and (-not (Test-Path -LiteralPath $diagPath))
    $existedWhileHeld -and $goneAfterRelease
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
