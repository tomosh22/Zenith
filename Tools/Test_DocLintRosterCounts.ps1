# Test_DocLintRosterCounts.ps1 -- assertions for doc_lint.ps1's C11 check
# (Zenithmon's SPECIES / HUMAN / BUILDING / PROP roster row counts agree with
# the `ZM_<FAMILY>_ID` enums that actually define them).
#
# A plain assert script, matching Tools/Test_DocLintRosterStamps.ps1 (C10's own
# test) and Tools/Test_DocLintKeyCitations.ps1: the only PowerShell this repo
# requires is the `pwsh` every gate already needs. It does NOT pin a check
# count anywhere else -- it prints its own assertion count, same as its
# siblings.
#
# doc_lint.ps1 runs all of its checks unconditionally and then `exit`s -- there
# is no flag to stop it short. So, like its siblings, this treats doc_lint.ps1
# as a BLACK BOX: it builds a small fixture repo (whichever
# Games/<Game>/Source/Data/ZM_<Family>Data.h roster headers a scenario needs,
# plus a Games/<Game>/Docs/AssetManifest.md and/or TestPlan.md) and invokes
# `pwsh -File doc_lint.ps1 -RepoRoot <fixture> -Game <scenario>` in a child
# process, then asserts on its stdout/stderr text and exit code. A scenario
# only creates the roster header(s) it actually exercises -- Check-RosterCounts
# treats a family with no header as "nothing claimed" (its expected count is
# $null and every pattern for that family is skipped), so a fixture never
# needs all four just to isolate one. Every fixture doc is built to avoid
# tripping C1-C9 (no Tests/, MVPScope.md, MvpRoadmap.md, [SUPERSEDED] text,
# "does not exist" claims, markdown links, Status.md, ArtBrief.md, or
# bracketed ticket citations), so a clean run's exit code is a meaningful
# signal about C11 alone, not noise from an unrelated check.
#
# Usage: pwsh -NoProfile -File Tools/Test_DocLintRosterCounts.ps1
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

# Maps a roster FAMILY name to the header file C11 (and, for PROP, C10 too)
# reads it from.
$script:familyFile = @{
    'SPECIES'  = 'ZM_SpeciesData.h'
    'HUMAN'    = 'ZM_HumanData.h'
    'BUILDING' = 'ZM_BuildingData.h'
    'PROP'     = 'ZM_PropData.h'
}

# Builds a fixture `ZM_<Family>Data.h` whose `ZM_<Family>_ID` enum holds
# exactly the given roster row names (real names, no `ZM_<Family>_` prefix
# needed by the caller -- it is added here) before the `ZM_<Family>_COUNT`
# terminator. Mirrors Test_DocLintRosterStamps.ps1's
# New-FixturePropDataHeader, generalised over FAMILY.
function New-FixtureRosterHeader {
    param([string]$Family, [string[]]$RowNames)
    $rows = ($RowNames | ForEach-Object { "ZM_${Family}_$_" }) -join ", "
    return @"
#pragma once
enum ZM_${Family}_ID : u_int
{
	$rows,

	ZM_${Family}_COUNT,
	ZM_${Family}_NONE = ZM_${Family}_COUNT   // sentinel
};
"@
}

# One fixture root per scenario. -Rosters is a hashtable of
# { FAMILY -> [string[]] row names }; only the families named are given a
# header file, which is deliberate -- see the file header comment above.
function New-FixtureRoot {
    param(
        [string]$GameName,
        [hashtable]$Rosters,
        [string]$AssetManifestContent = $null,
        [string]$TestPlanContent = $null
    )
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("doclint_c11_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    $gameDir = Join-Path $dir "Games/$GameName"
    New-Item -ItemType Directory -Path (Join-Path $gameDir 'Source/Data') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $gameDir 'Docs') -Force | Out-Null
    foreach ($family in $Rosters.Keys) {
        $fileName = $script:familyFile[$family]
        Set-Content -LiteralPath (Join-Path $gameDir "Source/Data/$fileName") -Value (New-FixtureRosterHeader -Family $family -RowNames $Rosters[$family]) -Encoding utf8
    }
    if ($null -ne $AssetManifestContent) {
        Set-Content -LiteralPath (Join-Path $gameDir 'Docs/AssetManifest.md') -Value $AssetManifestContent -Encoding utf8
    }
    if ($null -ne $TestPlanContent) {
        Set-Content -LiteralPath (Join-Path $gameDir 'Docs/TestPlan.md') -Value $TestPlanContent -Encoding utf8
    }
    return $dir
}

function Invoke-DocLint {
    param([string]$Root, [string]$GameName)
    $output = & pwsh -NoProfile -File $docLintPath -RepoRoot $Root -Game $GameName -ShowPassed 2>&1 | Out-String
    return [PSCustomObject]@{ Output = $output; ExitCode = $LASTEXITCODE }
}

$roots = New-Object System.Collections.Generic.List[string]
try {

    Write-Host "`n=== a stale HUMAN claim is reported (a phrasing C10 never reads) ===" -ForegroundColor Cyan

    # ★ DELIBERATELY ONE PHYSICAL LINE. The real AssetManifest.md sentence this
    # phrasing is modelled on ("...re-skinning all 34\nhumans onto the wrong
    # joints.") line-wraps between the number and "humans" -- every check in
    # this file reads one line at a time (see this check's own "what it does
    # NOT do" comment), so that real sentence is NOT machine-checked by this
    # pattern and was fixed by hand for this ticket. This fixture proves the
    # pattern works when the claim is not split by a markdown line wrap.
    Assert-That 'doc claims "all 4 humans" while ZM_HUMAN_ID enumerates 3 -- reported, naming HUMAN and the header' {
        $root = New-FixtureRoot -GameName 'StaleHuman' -Rosters @{ HUMAN = @('A', 'B', 'C') } -AssetManifestContent @'
So a StickFigure reorder reds this game instead of silently re-skinning all 4 humans onto the wrong joints.
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'StaleHuman'
        ($r.ExitCode -eq 1) `
            -and ($r.Output -match 'VIOLATION \[C11\][^\r\n]*claims 4 for the HUMAN roster[^\r\n]*"all N humans"[^\r\n]*ZM_HUMAN_ID \(Games/StaleHuman/Source/Data/ZM_HumanData\.h\) has 3')
    }

    Write-Host "`n=== a stale PROP claim, in a phrasing C10 does NOT match, is reported by C11 alone ===" -ForegroundColor Cyan

    # ** THE OVERLAP-SAFETY ASSERTION. C10 (Check-PropGenStamps) already reads
    # this same AssetManifest.md/TestPlan.md pair for "N prop roster rows",
    # "all N rows self-index" and "N props x M =". This fixture's claim --
    # "Props (N models)." -- is deliberately NONE of those three, so this
    # proves the new phrasing is caught, and caught ONCE (by C11, not C10).
    Assert-That 'doc claims "Props (4 models)." while ZM_PROP_ID enumerates 3 -- exactly one violation, tagged C11' {
        $root = New-FixtureRoot -GameName 'StaleProp' -Rosters @{ PROP = @('A', 'B', 'C') } -AssetManifestContent @'
**Props (4 models).** `ZM_PropGen` (SHIPPED): fences, signs, lamps.
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'StaleProp'
        $c11 = @([regex]::Matches($r.Output, 'VIOLATION \[C11\][^\r\n]*'))
        $c10 = @([regex]::Matches($r.Output, 'VIOLATION \[C10\][^\r\n]*'))
        ($r.ExitCode -eq 1) -and ($c11.Count -eq 1) -and ($c10.Count -eq 0) `
            -and ($c11[0].Value -match 'claims 4 for the PROP roster[^\r\n]*"Props \(N models\)" subheading[^\r\n]*has 3')
    }

    Write-Host "`n=== a PROP claim in a phrasing C10 OWNS is reported by C10 alone -- C11 does not duplicate it ===" -ForegroundColor Cyan

    # ** THE COMPLEMENTARY OVERLAP-SAFETY ASSERTION. "N prop roster rows" is one
    # of C10's own three phrasings (Check-PropGenStamps). If C11's pattern list
    # ever grew a fourth PROP entry that also matched this text, this line
    # would report BOTH [C10] and [C11] for the same drift -- which is exactly
    # the shape the ticket's own boundary condition forbids.
    Assert-That 'doc claims "4 prop roster rows" (a C10 phrasing) while ZM_PROP_ID enumerates 3 -- C10 alone reports it' {
        $root = New-FixtureRoot -GameName 'C10OwnsThis' -Rosters @{ PROP = @('A', 'B', 'C') } -AssetManifestContent @'
ZM_PropGen turns each of the 4 prop roster rows into a static model.
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'C10OwnsThis'
        $c11 = @([regex]::Matches($r.Output, 'VIOLATION \[C11\][^\r\n]*'))
        $c10 = @([regex]::Matches($r.Output, 'VIOLATION \[C10\][^\r\n]*'))
        ($r.ExitCode -eq 1) -and ($c10.Count -eq 1) -and ($c11.Count -eq 0)
    }

    Write-Host "`n=== understating is caught too (EQUALITY both directions, same reasoning as C7/C10) ===" -ForegroundColor Cyan

    Assert-That 'doc claims "2 species x 4 files =" while ZM_SPECIES_ID enumerates 3 -- understating is reported' {
        $root = New-FixtureRoot -GameName 'UnderstateSpecies' -Rosters @{ SPECIES = @('A', 'B', 'C') } -AssetManifestContent @'
Creature species file sets: 2 species x 4 files = 8 (see 1.2).
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'UnderstateSpecies'
        ($r.ExitCode -eq 1) -and ($r.Output -match 'VIOLATION \[C11\][^\r\n]*claims 2 for the SPECIES roster[^\r\n]*has 3')
    }

    Write-Host "`n=== a stale BUILDING claim is reported, proving the generalisation reaches a THIRD family ===" -ForegroundColor Cyan

    Assert-That 'doc claims "4 building roster rows" while ZM_BUILDING_ID enumerates 3 -- reported, naming BUILDING' {
        $root = New-FixtureRoot -GameName 'StaleBuilding' -Rosters @{ BUILDING = @('A', 'B', 'C') } -TestPlanContent @'
`ZM_BuildingGen` turns each of the 4 building roster rows into a deterministic
STATIC model.
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'StaleBuilding'
        ($r.ExitCode -eq 1) -and ($r.Output -match 'VIOLATION \[C11\][^\r\n]*claims 4 for the BUILDING roster[^\r\n]*has 3')
    }

    Write-Host "`n=== a roster row named ...COUNTER does not truncate the count, generalised to a family OTHER than PROP ===" -ForegroundColor Cyan

    # ** THE GENERALISED \b-FIX PIN. C10's own test pins this for PROP
    # (ZM_PROP_COUNTER). This fixture puts the identical shape on BUILDING
    # instead -- ZM_BUILDING_COUNTER -- to prove Get-ZmRosterCount's \b
    # requirement was carried into the generalised function, not just kept
    # true by coincidence for the one family that already had a test.
    Assert-That '** a roster row named ...COUNTER does not truncate the BUILDING count (doc claims 4 and the header truly has 4)' {
        $root = New-FixtureRoot -GameName 'CounterPrefixBuilding' -Rosters @{ BUILDING = @('A', 'COUNTER', 'B', 'C') } -TestPlanContent @'
`ZM_BuildingGen` turns each of the 4 building roster rows into a deterministic
STATIC model.
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'CounterPrefixBuilding'
        ($r.ExitCode -eq 0) -and ($r.Output -match 'PASS \[C11\][^\r\n]*BUILDING=4') -and ($r.Output -match 'ALL CHECKS PASS')
    }

    Write-Host "`n=== a correct doc across all four families passes, with a summary naming every family ===" -ForegroundColor Cyan

    Assert-That 'all four rosters match their headers -- one PASS [C11] line, no violations' {
        $root = New-FixtureRoot -GameName 'AllCorrect' -Rosters @{
            SPECIES  = @('A', 'B')
            HUMAN    = @('A', 'B', 'C')
            BUILDING = @('A', 'B', 'C', 'D')
            PROP     = @('A')
        } -AssetManifestContent @'
Creature species file sets: 2 species x 15 files = 30 (see 1.2).

## 2. Humans (~3 models)

## 3. Buildings (4) and props (1)
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'AllCorrect'
        ($r.ExitCode -eq 0) `
            -and ($r.Output -match 'PASS \[C11\][^\r\n]*BUILDING=4[^\r\n]*HUMAN=3[^\r\n]*PROP=1[^\r\n]*SPECIES=2') `
            -and ($r.Output -match 'ALL CHECKS PASS')
    }

    Write-Host "`n=== no roster headers under this game -- degrades gracefully, never fails ===" -ForegroundColor Cyan

    Assert-That 'a game with none of the four roster headers passes C11 outright, not by accident' {
        $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("doclint_c11_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        $docsDir = Join-Path $dir 'Games/NoRosterHeaders/Docs'
        New-Item -ItemType Directory -Path $docsDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $docsDir 'AssetManifest.md') -Value "Nothing roster-shaped here." -Encoding utf8
        $roots.Add($dir)
        $r = Invoke-DocLint -Root $dir -GameName 'NoRosterHeaders'
        ($r.ExitCode -eq 0) -and ($r.Output -match 'PASS \[C11\][^\r\n]*no ZM_\{Species,Human,Building,Prop\}Data\.h roster headers under NoRosterHeaders')
    }

    Write-Host "`n=== the violation names path:line, matching the house style (C7, C9, C10, ...) ===" -ForegroundColor Cyan

    Assert-That 'the violation line names AssetManifest.md and its 1-based line number' {
        $root = New-FixtureRoot -GameName 'LineNumber' -Rosters @{ HUMAN = @('A', 'B', 'C') } -AssetManifestContent @'
Filler line one.
Filler line two.
Humans (~9 models)
'@
        $roots.Add($root)
        $r = Invoke-DocLint -Root $root -GameName 'LineNumber'
        $r.Output -match 'AssetManifest\.md:3: claims 9 for the HUMAN roster'
    }

} finally {
    foreach ($r in $roots) { Remove-Item -LiteralPath $r -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
