# Test_DocLintRosterStamps.ps1 -- assertions for doc_lint.ps1's C10 check
# (Zenithmon's prop-roster row count, and EVERY discovered `uZM_*GEN_VERSION`
# generator stamp, agree with the C++ headers that actually define them).
#
# A plain assert script, matching Tools/Test_DocLintKeyCitations.ps1 and
# Tools/Test_TickGates.ps1: the only PowerShell this repo requires is the
# `pwsh` every gate already needs. It does NOT pin a check count anywhere
# else -- it prints its own assertion count, same as its siblings.
#
# doc_lint.ps1 runs all of its checks unconditionally at the bottom of the
# file and then `exit`s -- there is no flag to stop it short. So, like
# Test_DocLintKeyCitations.ps1, this treats doc_lint.ps1 as a BLACK BOX: it
# builds a small fixture repo (a fake ZM_PropData.h roster header, plus
# whichever Source/Gen/ZM_*Gen.h generator headers the scenario names, plus a
# Games/<Game>/Docs/AssetManifest.md) and invokes
# `pwsh -File doc_lint.ps1 -RepoRoot <fixture> -Game <scenario>` in a child
# process, then asserts on its stdout/stderr text and exit code. Every
# fixture doc is built to avoid tripping C1-C9 (no Tests/, MVPScope.md,
# MvpRoadmap.md, [SUPERSEDED] text, "does not exist" claims, markdown links,
# Status.md, ArtBrief.md, or bracketed ticket citations), so a clean run's
# exit code is a meaningful signal about C10 alone, not noise from an
# unrelated check.
#
# Usage: pwsh -NoProfile -File Tools/Test_DocLintRosterStamps.ps1
# Exit:  0 all pass, 1 otherwise.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$docLintPath = Join-Path $PSScriptRoot 'doc_lint.ps1'

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

# Builds a fixture ZM_PropData.h whose ZM_PROP_ID enum holds exactly the given
# roster row names (real names, no ZM_PROP_ prefix needed by the caller -- it
# is added here) before the ZM_PROP_COUNT terminator.
function New-FixturePropDataHeader {
    param([string[]]$RowNames)
    $rows = ($RowNames | ForEach-Object { "ZM_PROP_$_" }) -join ", "
    return @"
#pragma once
enum ZM_PROP_ID : u_int
{
	$rows,

	ZM_PROP_COUNT,
	ZM_PROP_NONE = ZM_PROP_COUNT   // "no prop" sentinel
};
"@
}

# Builds ONE fixture generator header. Deliberately parametrised on both the
# FILE name and the CONSTANT name -- unlike the single hardcoded
# ZM_PropGen.h/uZM_PROPGEN_VERSION pair this test used before C10 was
# generalised, a scenario can now name any Source/Gen/ZM_*Gen.h file and any
# uZM_..._VERSION constant, which is what lets a test prove the check
# DISCOVERS generators rather than assuming PropGen is the only one.
function New-FixtureGeneratorHeader {
    param([string]$ConstantName, [int]$Version)
    return @"
#pragma once
constexpr u_int $ConstantName = ${Version}u;
"@
}

# One fixture root per scenario (unlike the C9 test's shared root) because
# each scenario needs its OWN header set, not just its own Docs/.
#
# -Generators takes zero or more @{ File = 'ZM_XGen.h'; Const = 'uZM_X_VERSION'; Version = N }
# hashtables, each written to its own file under Source/Gen -- so a scenario
# can populate ONE generator (the common case), several at once (to prove
# per-generator isolation), or none (the "no headers" scenario builds its own
# fixture without this helper).
function New-FixtureRoot {
    param(
        [string]$GameName,
        [string[]]$RosterRows,
        [array]$Generators,
        [string]$AssetManifestContent
    )
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("doclint_c10_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    $gameDir = Join-Path $dir "Games/$GameName"
    New-Item -ItemType Directory -Path (Join-Path $gameDir 'Source/Data') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $gameDir 'Source/Gen') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $gameDir 'Docs') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $gameDir 'Source/Data/ZM_PropData.h') -Value (New-FixturePropDataHeader -RowNames $RosterRows) -Encoding utf8
    foreach ($gen in $Generators) {
        Set-Content -LiteralPath (Join-Path $gameDir "Source/Gen/$($gen.File)") -Value (New-FixtureGeneratorHeader -ConstantName $gen.Const -Version $gen.Version) -Encoding utf8
    }
    Set-Content -LiteralPath (Join-Path $gameDir 'Docs/AssetManifest.md') -Value $AssetManifestContent -Encoding utf8
    return $dir
}

function Invoke-DocLint {
    param([string]$Root, [string]$GameName)
    $output = & pwsh -NoProfile -File $docLintPath -RepoRoot $Root -Game $GameName -ShowPassed 2>&1 | Out-String
    return [PSCustomObject]@{ Output = $output; ExitCode = $LASTEXITCODE }
}

# Shorthand for the common single-generator case (PropGen), used by most
# scenarios below -- only the generalisation-specific scenarios further down
# build a -Generators array by hand.
function New-PropGenOnly {
    param([int]$Version)
    return @(@{ File = 'ZM_PropGen.h'; Const = 'uZM_PROPGEN_VERSION'; Version = $Version })
}

$roots = New-Object System.Collections.Generic.List[string]
try {

    Write-Host "`n=== a stale version stamp IS reported (roster count correct, so this isolates C10's version half) ===" -ForegroundColor Cyan

    Assert-That 'doc claims uZM_PROPGEN_VERSION = 2 while the header says 5 -- reported, and the roster line (which matches) is not' {
        $root = New-FixtureRoot -GameName 'StaleVersion' -RosterRows @('A', 'B', 'C') -Generators (New-PropGenOnly 5) -AssetManifestContent @'
Family total: **3 props x 8 = 24 files**.

Determinism / version stamp: golden-pinned (`uZM_PROPGEN_VERSION` = 2).
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'StaleVersion'
        ($r.ExitCode -eq 1) `
            -and ($r.Output -match 'VIOLATION \[C10\][^\r\n]*claims uZM_PROPGEN_VERSION = 2[^\r\n]*has 5') `
            -and ($r.Output -notmatch 'claims 3 prop roster')
    }

    Write-Host "`n=== a stale roster count IS reported (version correct, so this isolates C10's roster half) ===" -ForegroundColor Cyan

    Assert-That 'doc claims 4 prop roster rows while the header enumerates 3 -- reported, and the version line (which matches) is not' {
        $root = New-FixtureRoot -GameName 'StaleRoster' -RosterRows @('A', 'B', 'C') -Generators (New-PropGenOnly 5) -AssetManifestContent @'
ZM_PropGen turns each of the 4 prop roster rows into a static model.

Determinism / version stamp: golden-pinned (`uZM_PROPGEN_VERSION` = 5).
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'StaleRoster'
        ($r.ExitCode -eq 1) `
            -and ($r.Output -match 'VIOLATION \[C10\][^\r\n]*claims 4 prop roster row\(s\)[^\r\n]*has 3') `
            -and ($r.Output -notmatch 'claims uZM_PROPGEN_VERSION')
    }

    Write-Host "`n=== understating is caught too (EQUALITY both directions, same reasoning as C7) ===" -ForegroundColor Cyan

    Assert-That 'doc claims 2 prop roster rows while the header enumerates 3 -- understating is reported, not just overstating' {
        $root = New-FixtureRoot -GameName 'UnderstateRoster' -RosterRows @('A', 'B', 'C') -Generators (New-PropGenOnly 5) -AssetManifestContent @'
ZM_PropGen turns each of the 2 prop roster rows into a static model.

Determinism / version stamp: golden-pinned (`uZM_PROPGEN_VERSION` = 5).
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'UnderstateRoster'
        ($r.ExitCode -eq 1) -and ($r.Output -match 'VIOLATION \[C10\][^\r\n]*claims 2 prop roster row\(s\)[^\r\n]*has 3')
    }

    Write-Host "`n=== a correct doc passes, including a roster row whose name is a PREFIX of the terminator ===" -ForegroundColor Cyan

    # ** THE LOAD-BEARING ASSERTION. ZM_PROP_COUNTER is a real roster row
    # (Games/Zenithmon/Source/Data/ZM_PropData.h) whose name starts with the
    # literal text "ZM_PROP_COUNT" -- the first implementation of
    # Get-ZmPropRosterCount used a non-word-boundary match up to that text and
    # stopped at ZM_PROP_COUNTER's own prefix instead of the real ZM_PROP_COUNT
    # terminator, undercounting 32 rows as 17 with no error raised. Putting
    # COUNTER before the true end of a 4-row fixture pins that fix: without the
    # \b, this scenario would report a false MISMATCH (doc says 4, buggy count
    # says 1) even though the doc is correct.
    Assert-That '** a roster row named ...COUNTER does not truncate the count (pins the \b fix; doc claims 4 and the header truly has 4)' {
        $root = New-FixtureRoot -GameName 'CounterPrefix' -RosterRows @('A', 'COUNTER', 'B', 'C') -Generators (New-PropGenOnly 5) -AssetManifestContent @'
ZM_PropGen turns each of the 4 prop roster rows into a static model.

Determinism / version stamp: golden-pinned (`uZM_PROPGEN_VERSION` = 5).
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'CounterPrefix'
        ($r.ExitCode -eq 0) -and ($r.Output -match 'PASS \[C10\][^\r\n]*prop roster \(4\)') -and ($r.Output -match 'ALL CHECKS PASS')
    }

    Write-Host "`n=== no headers under this game -- degrades gracefully, never fails ===" -ForegroundColor Cyan

    Assert-That 'a game with no ZM_PropData.h and no Source/Gen/ZM_*Gen.h passes C10 outright, not by accident' {
        $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("doclint_c10_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        $docsDir = Join-Path $dir 'Games/NoPropHeaders/Docs'
        New-Item -ItemType Directory -Path $docsDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $docsDir 'AssetManifest.md') -Value "Nothing prop-shaped here." -Encoding utf8
        $roots.Add($dir)
        $r = Invoke-DocLint -Root $dir -GameName 'NoPropHeaders'
        ($r.ExitCode -eq 0) -and ($r.Output -match 'PASS \[C10\][^\r\n]*no ZM_PropData\.h / Source/Gen/ZM_\*Gen\.h headers under NoPropHeaders')
    }

    Write-Host "`n=== the violation names path:line, matching the house style (C7, C9, ...) ===" -ForegroundColor Cyan

    Assert-That 'the violation line names AssetManifest.md and its 1-based line number' {
        $root = New-FixtureRoot -GameName 'LineNumber' -RosterRows @('A', 'B', 'C') -Generators (New-PropGenOnly 5) -AssetManifestContent @'
Filler line one.
Filler line two.
Determinism / version stamp: golden-pinned (`uZM_PROPGEN_VERSION` = 999).
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'LineNumber'
        $r.Output -match 'AssetManifest\.md:3: claims uZM_PROPGEN_VERSION = 999'
    }

    Write-Host "`n=== GENERALISATION: a generator OTHER than PropGen is covered (ZEN feedback: widening beyond one name) ===" -ForegroundColor Cyan

    # ** THE SECOND LOAD-BEARING ASSERTION. Before C10 was generalised it only
    # ever read Source/Gen/ZM_PropGen.h and the uZM_PROPGEN_VERSION constant
    # by NAME -- which is exactly the "hand-maintained list" shape this repo
    # calls a defect everywhere else (rm_packed, the old 4-site pin). This
    # fixture's generator is named ZM_WidgetGen.h / uZM_WIDGETGEN_VERSION,
    # neither of which appears anywhere in doc_lint.ps1's source: if the check
    # ever regresses back to a hardcoded PropGen-only path, this is the
    # assertion that goes red, not a coincidental pass.
    Assert-That '** a stale stamp for a generator that is NOT PropGen (ZM_WidgetGen.h / uZM_WIDGETGEN_VERSION) is caught' {
        $root = New-FixtureRoot -GameName 'OtherGenStale' -RosterRows @('A', 'B', 'C') `
            -Generators @(@{ File = 'ZM_WidgetGen.h'; Const = 'uZM_WIDGETGEN_VERSION'; Version = 7 }) `
            -AssetManifestContent @'
Family total: **3 props x 8 = 24 files**.

Determinism / version stamp: golden-pinned (`uZM_WIDGETGEN_VERSION` = 3).
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'OtherGenStale'
        ($r.ExitCode -eq 1) -and ($r.Output -match 'VIOLATION \[C10\][^\r\n]*claims uZM_WIDGETGEN_VERSION = 3[^\r\n]*has 7')
    }

    Write-Host "`n=== GENERALISATION: several generators at once, each checked against its OWN header (no cross-contamination) ===" -ForegroundColor Cyan

    Assert-That '** two generators, one stale (GizmoGen) and one correct (PropGen) -- only the stale one is reported' {
        $root = New-FixtureRoot -GameName 'TwoGensOneStale' -RosterRows @('A', 'B', 'C') `
            -Generators @(
                @{ File = 'ZM_PropGen.h'; Const = 'uZM_PROPGEN_VERSION'; Version = 5 },
                @{ File = 'ZM_GizmoGen.h'; Const = 'uZM_GIZMOGEN_VERSION'; Version = 4 }
            ) `
            -AssetManifestContent @'
Determinism / version stamp: golden-pinned (`uZM_PROPGEN_VERSION` = 5).

Determinism / version stamp: golden-pinned (`uZM_GIZMOGEN_VERSION` = 2).
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'TwoGensOneStale'
        $violations = @([regex]::Matches($r.Output, 'VIOLATION \[C10\][^\r\n]*'))
        ($r.ExitCode -eq 1) `
            -and ($violations.Count -eq 1) `
            -and ($violations[0].Value -match 'claims uZM_GIZMOGEN_VERSION = 2[^\r\n]*has 4') `
            -and ($r.Output -notmatch 'claims uZM_PROPGEN_VERSION')
    }

    Write-Host "`n=== GENERALISATION: a newly-added generator is picked up with NO code change to doc_lint.ps1 ===" -ForegroundColor Cyan

    # This is the design intent C10 exists to satisfy: "a fifth
    # generator added later should be covered on the day it lands, without
    # anyone remembering to extend this check." ZM_SprocketGen.h is invented
    # for this test alone and Get-ZmGeneratorVersions finds it purely by
    # globbing Source/Gen/ZM_*Gen.h -- there is no name it needs to already
    # know.
    Assert-That '** a brand-new generator header (ZM_SprocketGen.h, invented for this test) is discovered and its correct stamp passes' {
        $root = New-FixtureRoot -GameName 'NewGenerator' -RosterRows @('A', 'B', 'C') `
            -Generators @(@{ File = 'ZM_SprocketGen.h'; Const = 'uZM_SPROCKETGEN_VERSION'; Version = 1 }) `
            -AssetManifestContent @'
Determinism / version stamp: golden-pinned (`uZM_SPROCKETGEN_VERSION` = 1).
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'NewGenerator'
        ($r.ExitCode -eq 0) `
            -and ($r.Output -match 'PASS \[C10\][^\r\n]*uZM_SPROCKETGEN_VERSION=1') `
            -and ($r.Output -match 'ALL CHECKS PASS')
    }

} finally {
    foreach ($r in $roots) { Remove-Item -LiteralPath $r -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
