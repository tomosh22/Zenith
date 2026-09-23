#Requires -Version 7.0
<#
  Test-ZagentClient.ps1 — unit tests for the `zagent` client helpers.

  A plain test script rather than Pester, matching
  Tools/Test_T0Harness_SessionCloseFlagsExist.ps1: the only PowerShell
  this repo requires is the pwsh every gate already needs, and adding a
  module dependency to run a hundred lines of tests is a worse trade
  than a few dozen lines of asserts.

  `zagent.ps1` is now a thin dispatch shim into the Vault CLI
  (Tools/Vault/cli/client.mjs), and `ZagentClient.psm1` keeps only the
  handful of pure helpers `build_prompt.ps1` still calls standalone.
  Everything that used to exercise the legacy HTTP transport — argument
  parsing, the docs-tree upload, drift/citation checks, the wire
  contract, `doctor`'s client-side checks, and so on — went with it;
  Tools/Vault/cli/tick-checks.mjs and the Vault CLI's own tests are the
  live equivalent for anything still needed.

  What remains here guards one bug that was found the hard way:
  `ConvertTo-PosixPath` — a separator mismatch makes an uploaded docs
  tree match nothing, which mirrors zero pages and reports success.

  Usage:
    pwsh -NoProfile -File Tools/zagent/Test-ZagentClient.ps1

  Exit codes:
    0 -- every assertion passed.
    1 -- one or more failed (each is printed with its expectation).
#>

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:failures = 0
$script:count = 0

function Assert-That {
    param([string]$Name, [scriptblock]$Body)
    $script:count++
    try {
        $ok = & $Body
        if ($ok) {
            Write-Host ("  ok   {0}" -f $Name) -ForegroundColor Green
        } else {
            Write-Host ("  FAIL {0}" -f $Name) -ForegroundColor Red
            $script:failures++
        }
    } catch {
        Write-Host ("  FAIL {0} -- threw: {1}" -f $Name, $_.Exception.Message) -ForegroundColor Red
        $script:failures++
    }
}

function New-TempDir {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("zagent-test-" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    return $dir
}

# ── The module under test, plus a parse-check of the script that uses it.
$here = $PSScriptRoot
Import-Module (Join-Path $here 'ZagentClient.psm1') -Force

Write-Host "`n=== both files parse ===" -ForegroundColor Cyan
foreach ($file in @('zagent.ps1', 'ZagentClient.psm1')) {
    Assert-That "$file parses cleanly" {
        $errors = $null
        [void][System.Management.Automation.Language.Parser]::ParseFile(
            (Join-Path $here $file), [ref]$null, [ref]$errors)
        # A parse error here means every gate stalls on a broken client,
        # and the failure would first appear as a permission prompt.
        $null -eq $errors -or $errors.Count -eq 0
    }
}

Write-Host "`n=== ConvertTo-PosixPath ===" -ForegroundColor Cyan
Assert-That 'converts backslashes' { (ConvertTo-PosixPath 'C:\dev\Zenith\Docs') -eq 'C:/dev/Zenith/Docs' }
Assert-That 'leaves forward slashes alone' { (ConvertTo-PosixPath 'C:/dev/Zenith') -eq 'C:/dev/Zenith' }
Assert-That 'trims a trailing slash' { (ConvertTo-PosixPath 'C:/dev/Zenith/') -eq 'C:/dev/Zenith' }
Assert-That 'trims repeated trailing slashes' { (ConvertTo-PosixPath 'C:/dev/Zenith///') -eq 'C:/dev/Zenith' }
Assert-That 'is idempotent' {
    # The board keys its uploaded docs tree on these strings; a value that
    # normalises differently on a second pass would match nothing.
    $once = ConvertTo-PosixPath 'C:\dev\Zenith\'
    (ConvertTo-PosixPath $once) -eq $once
}

Write-Host "`n=== Find-ClientRepo ===" -ForegroundColor Cyan
$repo = New-TempDir
New-Item -ItemType Directory -Path (Join-Path $repo 'Games\Zenithmon') -Force | Out-Null
'{ "project": "ZEN", "baseBranch": "master" }' | Set-Content (Join-Path $repo 'zagent.project.json') -Encoding utf8

Assert-That 'finds the file in the repo root' { (Find-ClientRepo $repo) -eq $repo }
Assert-That 'walks UP from a subdirectory' {
    # `zagent owns` from Games/Zenithmon must read the same gates as one
    # run from the root.
    (Find-ClientRepo (Join-Path $repo 'Games\Zenithmon')) -eq $repo
}
Assert-That 'returns null outside any client repo' {
    # Not an error: a fresh checkout with no project file yet must not
    # make every downstream call throw.
    $null -eq (Find-ClientRepo (New-TempDir))
}

Write-Host "`n=== Get-ScratchRoot ===" -ForegroundColor Cyan
Assert-That 'is .zagent/run under the client repo' {
    # `.zagent/` is gitignored so the tick's dirty-tree precondition still
    # sees a clean tree.
    (Get-ScratchRoot 'C:\x') -eq (Join-Path (Join-Path 'C:\x' '.zagent') 'run')
}

# Every category that declares `paths` must also declare `gates`, or the
# union it exists for silently adds nothing. The board treats a gateless
# category as inert by design (a category may legitimately own no build),
# but in THIS repo a paths-without-gates entry is always a mistake, and
# it is invisible: the gates that do run all pass. Not a ZagentClient
# helper test -- a project-file sanity check that happens to live here.
Write-Host "`n=== zagent.project.json sanity ===" -ForegroundColor Cyan
Assert-That 'every zagent.project.json category with paths also has gates' {
    $projectRepo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Get-Content (Join-Path $projectRepo 'zagent.project.json') -Raw | ConvertFrom-Json
    $bad = @()
    foreach ($category in $file.categories.PSObject.Properties) {
        $paths = $category.Value.PSObject.Properties['paths']
        $gates = $category.Value.PSObject.Properties['gates']
        if ($paths -and @($paths.Value).Count -gt 0) {
            if (-not $gates -or @($gates.Value).Count -eq 0) { $bad += $category.Name }
        }
    }
    if ($bad) { Write-Host ("       categories with paths and no gates: " + ($bad -join ', ')) }
    $bad.Count -eq 0
}

# An engine change can break a game it never mentions. Games/Combat --
# which used to be the only game the Engine gate list built -- references
# Zenith_TerrainComponent in ZERO source files, while Zenithmon,
# CityBuilder and RenderTest reference it in 12, 4 and 4. ZEN-5 made
# members of that header private and every Engine gate went green.
#
# The `paths` union cannot catch that: the diff is inside Engine's OWN
# paths, so there is no foreign category to pull in. A public header's
# blast radius is everything that INCLUDES it, which no directory mapping
# expresses -- so the gate list has to name every game, and THIS is what
# stops a newly-added game from being silently left out of it.
Assert-That 'the Engine gate list builds every game that has a .zproj' {
    $projectRepo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Get-Content (Join-Path $projectRepo 'zagent.project.json') -Raw | ConvertFrom-Json
    $gates = @($file.categories.Engine.gates) -join "`n"

    $games = @(Get-ChildItem -Path (Join-Path $projectRepo 'Games') -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName ("{0}.zproj" -f $_.Name)) } |
        ForEach-Object { $_.Name })
    if ($games.Count -eq 0) { Write-Host '       no .zproj games found -- cannot assert'; return $false }

    $missing = @($games | Where-Object { $gates -notmatch [regex]::Escape("build $_ ") })
    if ($missing.Count) {
        Write-Host ("       games missing from the Engine gate list: " + ($missing -join ', '))
    }
    $missing.Count -eq 0
}

# Compile before test: a compile break in ANY game should stop the run
# before a minute goes into one game's suite. If a `test` line drifts
# above a `build` line the list still passes, just slower and less
# usefully, so this is a cheap ordering guard rather than a correctness
# one.
Assert-That 'every build line comes before the first test line' {
    $projectRepo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Get-Content (Join-Path $projectRepo 'zagent.project.json') -Raw | ConvertFrom-Json
    $gates = @($file.categories.Engine.gates)
    $lastBuild = -1; $firstTest = $gates.Count
    for ($i = 0; $i -lt $gates.Count; $i++) {
        if ($gates[$i] -match '\bbuild\s') { $lastBuild = $i }
        if ($gates[$i] -match '\btest\s' -and $firstTest -eq $gates.Count) { $firstTest = $i }
    }
    $lastBuild -lt $firstTest
}

# ─── THE EXIT CODE, RECORDED WHERE A PIPE CANNOT REACH IT ────────────
#
# `tick.md`'s most-broken rule is "never pipe a call whose exit code you
# intend to read". These pin the file that retires it. Measured before
# the fix: a piped `zagent owns` reported 0 while the real code was 5.
Write-Host "`n=== Write-LastExit ===" -ForegroundColor Cyan

Assert-That '** the exit code is written to .zagent/last.exit' {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("zagent-exit-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    try {
        Write-LastExit -Code 5 -Repo $dir
        $read = Get-Content -LiteralPath (Join-Path $dir '.zagent/last.exit') -Raw
        [int]($read.Trim()) -eq 5
    } finally { Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a later exit OVERWRITES the previous one rather than appending' {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("zagent-exit-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    try {
        Write-LastExit -Code 4 -Repo $dir
        Write-LastExit -Code 0 -Repo $dir
        $read = (Get-Content -LiteralPath (Join-Path $dir '.zagent/last.exit') -Raw).Trim()
        $read -eq '0'
    } finally { Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'recording an exit code NEVER throws -- an empty or bad repo is a no-op' {
    # It runs on the way out of a failure path. A throw here would replace
    # the real exit code with a crash, which is strictly worse than the bug.
    try {
        Write-LastExit -Code 1 -Repo ''
        Write-LastExit -Code 1 -Repo $null
        Write-LastExit -Code 1 -Repo 'Q:
o\such\drivenywhere'
        $true
    } catch { $false }
}

# ─── ROUTING FIXES (zagent.project.json + cli/legacy-pure/routing.mjs) ──────
#
# The mechanism itself is proven in Tools/Vault/test/category-routing.test.mjs
# against the real selector; these three are cheap config-shape sanity checks
# that belong beside the other zagent.project.json assertions above.
Write-Host "`n=== category-routing config sanity ===" -ForegroundColor Cyan

Assert-That 'Tooling declares suppressLanguageConventions: true' {
    $projectRepo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Get-Content (Join-Path $projectRepo 'zagent.project.json') -Raw | ConvertFrom-Json
    $flag = $file.categories.Tooling.PSObject.Properties['suppressLanguageConventions']
    $null -ne $flag -and $flag.Value -eq $true
}

Assert-That 'the root conventions/languageConventions split covers both halves of the pre-split string' {
    # "Root CLAUDE.md is binding." and "Never run Sharpmake in a linked
    # worktree unless it is a registered lane." are universal and must stay
    # in `conventions`, never move to `languageConventions` -- that is the
    # whole point of the split. The Sharpmake clause was reworded from "in a
    # worktree" (an absolute ban -- wrong, since it also forbade Sharpmake in
    # the MAIN checkout) to "in a linked worktree unless it is a registered
    # lane" when parallel lanes landed (Tools/zagent/protocol/TICK.md's
    # "Lane mode" section) -- a registered lane IS a linked worktree where
    # Tools/lane_init.ps1 legitimately runs regen, so the plain "in a
    # worktree" ban was never what was meant.
    $projectRepo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Get-Content (Join-Path $projectRepo 'zagent.project.json') -Raw | ConvertFrom-Json
    ($file.conventions -match 'Root CLAUDE\.md is binding') -and
        ($file.conventions -match 'Never run Sharpmake in a linked worktree unless it is a registered lane') -and
        ($file.conventions -notmatch 'std::') -and
        ($file.languageConventions -match 'std::') -and
        ($file.languageConventions -notmatch 'Sharpmake in a linked worktree unless it is a registered lane')
}

Assert-That 'Engine declares a decisionLog + decisionIdPrefix, and the log file exists' {
    $projectRepo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Get-Content (Join-Path $projectRepo 'zagent.project.json') -Raw | ConvertFrom-Json
    $engine = $file.categories.Engine
    ($engine.decisionIdPrefix -eq 'ZEN-D-') -and
        ($engine.decisionLog) -and
        (Test-Path (Join-Path $projectRepo $engine.decisionLog))
}

Assert-That 'Engine paths include Build/** as well as Zenith/**' {
    $projectRepo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Get-Content (Join-Path $projectRepo 'zagent.project.json') -Raw | ConvertFrom-Json
    $paths = @($file.categories.Engine.paths)
    ($paths -contains 'Zenith/**') -and ($paths -contains 'Build/**')
}

Write-Host "`n=== harness artifacts are not stale (Build-HarnessArtifacts.ps1 -Check) ===" -ForegroundColor Cyan
# Was: a hand-rolled byte-identity check between .claude/commands/tick.md
# and Tools/zagent/protocol/TICK.md, marked TEMPORARY pending Stage 3's
# stub-vs-inline decision. Stage 3 answered that (deliveryMode: "inline"
# for claude -- see Build-HarnessArtifacts.ps1's own header) and replaced
# the whole hand-maintained-pair shape: .claude/commands/tick.md is now
# ONE of several generated files (alongside the five per-tier
# .claude/agents/zagent-{worker,reviewer}-*.md role files and the two
# resolved charters under Tools/zagent/protocol/roles/generated/), and
# a bespoke assertion here would have to grow a special case per new
# generated file. Delegating to the generator's OWN -Check is what
# `zenith regen --check` does for Sharpmake output, for the same reason:
# the generator is the one place that knows the full, current set of
# artifacts it owns.
#
# Tools/zagent/Test-HarnessParity.ps1 is NOT on the Tooling gate list
# (adding it needs a human edit to a protected zagent.project.json), so
# this assertion is what actually catches staleness on a gate that runs
# today -- Test-ZagentClient.ps1 already is one.
Assert-That 'Build-HarnessArtifacts.ps1 -Check reports no stale harness artifacts' {
    $projectRepo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $scriptPath = Join-Path $PSScriptRoot 'Build-HarnessArtifacts.ps1'

    $output = & pwsh -NoProfile -File $scriptPath -Check 2>&1
    $exitCode = $LASTEXITCODE

    if ($exitCode -ne 0) {
        Write-Host "       Build-HarnessArtifacts.ps1 -Check exited $exitCode -- stdout/stderr follows:"
        foreach ($line in $output) { Write-Host "       $line" }
    }
    $exitCode -eq 0
}

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
