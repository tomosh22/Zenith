#Requires -Version 7.0
<#
  Test-BuildPrompt.ps1 — unit tests for Tools/zagent/build_prompt.ps1.

  Plain assert script, matching Test-ZagentClient.ps1's own conventions
  (its Assert-That helper, its section headers, its final
  "N/N assertions passed." + PASS/FAIL + exit code) rather than pulling in
  Pester: the only PowerShell this repo requires is the pwsh every gate
  already needs.

  build_prompt.ps1 is invoked as a REAL separate `pwsh -File` process for
  every assertion, never dot-sourced or called with `&` in-process --
  `exit` inside a script invoked that way does not stop at the script
  boundary the way a function return does, and a stray `exit 1` inside the
  thing under test would tear down this test runner along with it. Paying
  one process-start per assertion is the same trade the interface itself
  makes: `pwsh -NoProfile -File Tools/zagent/build_prompt.ps1 ...` IS how
  the orchestrator will call it.

  Usage:
    pwsh -NoProfile -File Tools/zagent/Test-BuildPrompt.ps1

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
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("bp-test-" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    return $dir
}

$here = $PSScriptRoot
$script:BuildPromptScript = Join-Path $here 'build_prompt.ps1'

<#
  Fixture repo builder. Every assertion gets its OWN temp repo (never a
  shared one) so a test cannot see another test's ticket.json/body.md --
  the same isolation Test-ZagentClient.ps1 gives Find-ClientRepo's own
  fixtures.
#>
function New-FixtureRepo {
    param(
        [string]$Key = 'ZM-999',
        [string]$Body,
        [string]$Conventions = 'Root CLAUDE.md is binding. Tabs. Braces on new lines.',
        [hashtable]$ExtraTicketFields = @{}
    )
    $repo = New-TempDir
    '{ "project": "ZEN", "baseBranch": "master" }' | Set-Content (Join-Path $repo 'zagent.project.json') -Encoding utf8

    $runDir = Join-Path $repo ".zagent/run/$Key"
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null

    $ticket = [ordered]@{ key = $Key }
    if ($Conventions) { $ticket['conventions'] = $Conventions }
    foreach ($k in $ExtraTicketFields.Keys) { $ticket[$k] = $ExtraTicketFields[$k] }
    ($ticket | ConvertTo-Json -Depth 20) | Set-Content (Join-Path $runDir 'ticket.json') -Encoding utf8 -NoNewline

    [System.IO.File]::WriteAllText((Join-Path $runDir 'body.md'), $Body, [System.Text.UTF8Encoding]::new($false))

    return [PSCustomObject]@{ Repo = $repo; RunDir = $runDir; Key = $Key }
}

function Invoke-BuildPrompt {
    <#
      Runs build_prompt.ps1 as a genuine child process (see file header)
      and returns its exit code, stdout and stderr without ever touching
      $?/$LASTEXITCODE across a pipe -- the same rule ZagentClient.psm1's
      Write-LastExit exists to enforce, applied to this test harness too.
    #>
    param([string[]]$ScriptArgs)

    $stderrFile = Join-Path ([System.IO.Path]::GetTempPath()) ("bp-stderr-" + [guid]::NewGuid().ToString('N'))
    try {
        $stdout = & pwsh -NoProfile -File $script:BuildPromptScript @ScriptArgs 2> $stderrFile
        $code = $LASTEXITCODE
        $stderr = if (Test-Path -LiteralPath $stderrFile) { Get-Content -LiteralPath $stderrFile -Raw -ErrorAction SilentlyContinue } else { '' }
        return [PSCustomObject]@{
            Code   = $code
            Stdout = ($stdout -join "`n")
            Stderr = if ($stderr) { $stderr } else { '' }
        }
    } finally {
        Remove-Item -LiteralPath $stderrFile -ErrorAction SilentlyContinue
    }
}

# ─── both files parse ─────────────────────────────────

Write-Host "`n=== both files parse ===" -ForegroundColor Cyan
foreach ($file in @('build_prompt.ps1', 'Test-BuildPrompt.ps1')) {
    Assert-That "$file parses cleanly" {
        $errors = $null
        [void][System.Management.Automation.Language.Parser]::ParseFile(
            (Join-Path $here $file), [ref]$null, [ref]$errors)
        $null -eq $errors -or $errors.Count -eq 0
    }
}

# ─── a clean run succeeds and writes both files ───────

Write-Host "`n=== a clean dispatch ===" -ForegroundColor Cyan
$cleanBody = @"
## Goal

Wire the reward path.

## Definition of Done

- [ ] Item one
- [ ] Item two

## Some Other Section

Extra prose that belongs in the rest of the body, not in the DoD.
"@

$fx = New-FixtureRepo -Body $cleanBody
$r = Invoke-BuildPrompt -ScriptArgs @('-Key', $fx.Key, '-Repo', $fx.Repo, '-Files', 'Games/Zenithmon/Source/Foo.cpp', '-Boundary', 'Stop once both items are wired.')

Assert-That 'exits 0 on a clean dispatch' { $r.Code -eq 0 }
Assert-That 'writes prompt.md' { Test-Path -LiteralPath (Join-Path $fx.RunDir 'prompt.md') }
Assert-That 'writes filelist.txt' { Test-Path -LiteralPath (Join-Path $fx.RunDir 'filelist.txt') }
Assert-That 'never touches ticket.json or body.md' {
    (Get-Content -LiteralPath (Join-Path $fx.RunDir 'body.md') -Raw) -eq $cleanBody
}

$promptText = Get-Content -LiteralPath (Join-Path $fx.RunDir 'prompt.md') -Raw
Assert-That 'opens with the DO NOT clause' { $promptText.TrimStart().StartsWith('**DO NOT**') }
Assert-That 'carries the Files clause' { $promptText -match '\*\*Files you may edit:\*\* Games/Zenithmon/Source/Foo\.cpp\.' }
Assert-That 'carries the Boundary text' { $promptText -match [regex]::Escape('Stop once both items are wired.') }
Assert-That 'carries the Goal section' { $promptText -match '## Goal(\r?\n)+Wire the reward path\.' }
Assert-That 'carries the rest of the body after the DoD' { $promptText -match 'Some Other Section' }
Assert-That 'the rest-of-body section appears AFTER the Definition of Done heading' {
    $promptText.IndexOf('## Definition of Done') -lt $promptText.IndexOf('Some Other Section')
}

# ─── ★ the incident this script exists to prevent ─────

Write-Host "`n=== DoD prose survives (ZM-23) ===" -ForegroundColor Cyan
$dodBody = @"
## Goal

Fix the thing.

## Definition of Done

- [ ] Item one
- [ ] Item two

* ITEMS 1 AND 2 ARE ONE INDIVISIBLE COMMIT

- [ ] Item three
"@

$fx2 = New-FixtureRepo -Body $dodBody -ExtraTicketFields @{
    # The array shape a real claim payload carries -- checkbox TEXT only,
    # deliberately missing the indivisible-commit line. If the script ever
    # fell back to this, the assertion below would still pass on Item
    # one/two/three but FAIL on the starred line -- which is exactly the
    # incident.
    definitionOfDone = @(
        @{ text = 'Item one'; done = $false },
        @{ text = 'Item two'; done = $false },
        @{ text = 'Item three'; done = $false }
    )
}
$r2 = Invoke-BuildPrompt -ScriptArgs @('-Key', $fx2.Key, '-Repo', $fx2.Repo, '-Files', 'Foo.cpp', '-Boundary', 'Stop after item three.')
$prompt2 = Get-Content -LiteralPath (Join-Path $fx2.RunDir 'prompt.md') -Raw

Assert-That 'exits 0' { $r2.Code -eq 0 }
Assert-That 'the prose BETWEEN checkboxes survives into prompt.md' {
    $prompt2 -match [regex]::Escape('ITEMS 1 AND 2 ARE ONE INDIVISIBLE COMMIT')
}
Assert-That 'the checkbox after the prose also survives' { $prompt2 -match 'Item three' }

# ─── missing DoD heading fails loudly, no silent fallback ───

Write-Host "`n=== no Definition of Done heading -> loud failure ===" -ForegroundColor Cyan
$noDodBody = @"
## Goal

Fix the thing, but the body forgot its own DoD heading.
"@
$fx3 = New-FixtureRepo -Body $noDodBody -ExtraTicketFields @{
    definitionOfDone = @(@{ text = 'Item one'; done = $false })
}
$r3 = Invoke-BuildPrompt -ScriptArgs @('-Key', $fx3.Key, '-Repo', $fx3.Repo, '-Files', 'Foo.cpp', '-Boundary', 'n/a')

Assert-That 'exits non-zero' { $r3.Code -ne 0 }
Assert-That 'names the missing heading on stderr' { $r3.Stderr -match 'Definition of Done' }
Assert-That 'does NOT write prompt.md on failure' { -not (Test-Path -LiteralPath (Join-Path $fx3.RunDir 'prompt.md')) }

# ─── conventions copied byte-for-byte ─────────────────

Write-Host "`n=== conventions are verbatim ===" -ForegroundColor Cyan
$weirdConventions = "Tabs, not spaces.`nSecond line with a `"quote`" and an em dash — and a trailing period."
$fx4 = New-FixtureRepo -Body $cleanBody -Conventions $weirdConventions
$r4 = Invoke-BuildPrompt -ScriptArgs @('-Key', $fx4.Key, '-Repo', $fx4.Repo, '-Files', 'Foo.cpp', '-Boundary', 'n/a')
$prompt4 = Get-Content -LiteralPath (Join-Path $fx4.RunDir 'prompt.md') -Raw

Assert-That 'exits 0' { $r4.Code -eq 0 }
Assert-That 'the conventions string appears byte-for-byte' {
    $prompt4.Contains($weirdConventions)
}

# ─── filelist.txt: one line per -Files entry, forward-slashed ────────

Write-Host "`n=== filelist.txt shape ===" -ForegroundColor Cyan
$fx5 = New-FixtureRepo -Body $cleanBody
$filesIn = @('Games\Zenithmon\Source\Foo.cpp', 'Games/Zenithmon/Source/Bar.h', 'Zenith\Core\Zenith_Thing.cpp')
# Passed as ONE comma-joined argv token -- the shape a real `-Files
# a,b,c` invocation actually arrives in once it has crossed the
# `pwsh -File` process boundary (see build_prompt.ps1's own comment on
# this). Passing three genuinely separate argv strings here would be
# testing an invocation shape the documented interface cannot produce.
$r5 = Invoke-BuildPrompt -ScriptArgs @('-Key', $fx5.Key, '-Repo', $fx5.Repo, '-Boundary', 'n/a', '-Files', ($filesIn -join ','))
$filelistRaw = [System.IO.File]::ReadAllText((Join-Path $fx5.RunDir 'filelist.txt'))
$filelistLines = $filelistRaw -split "`n" | Where-Object { $_ -ne '' }

Assert-That 'exits 0' { $r5.Code -eq 0 }
Assert-That 'exactly one line per -Files entry' { $filelistLines.Count -eq $filesIn.Count }
Assert-That 'every line is forward-slashed' { -not ($filelistLines | Where-Object { $_ -match '\\' }) }
Assert-That 'no header and no blank lines' {
    $nonEmpty = ($filelistRaw -split "`n") | Where-Object { $_ -ne '' }
    $nonEmpty.Count -eq $filesIn.Count -and -not ($nonEmpty | Where-Object { $_ -match '^\s*#' })
}
Assert-That 'LF endings, no CR anywhere' { -not $filelistRaw.Contains("`r") }
Assert-That 'the values match, only slash-normalised' {
    $expected = @($filesIn | ForEach-Object { $_ -replace '\\', '/' })
    (Compare-Object $expected $filelistLines) -eq $null
}

# ─── a missing optional field never throws under StrictMode ──────────

Write-Host "`n=== missing optional payload field ===" -ForegroundColor Cyan
$fx6 = New-FixtureRepo -Body $cleanBody -Conventions $null
# New-FixtureRepo only ever writes 'key' plus whatever is passed -- passing
# $null for Conventions means ticket.json carries NO 'conventions' property
# at all, matching a payload that legally omits it (Get-ClientChecks'
# baseBranch is the precedent this mirrors).
$ticketJson = Get-Content -LiteralPath (Join-Path $fx6.RunDir 'ticket.json') -Raw
Assert-That 'fixture actually omits conventions' { $ticketJson -notmatch 'conventions' }

$r6 = Invoke-BuildPrompt -ScriptArgs @('-Key', $fx6.Key, '-Repo', $fx6.Repo, '-Files', 'Foo.cpp', '-Boundary', 'n/a')
Assert-That 'does not throw / exits 0 with conventions absent' { $r6.Code -eq 0 }
Assert-That 'stderr carries no StrictMode exception' { $r6.Stderr -notmatch 'PropertyNotFoundException|cannot be retrieved' }

# ─── rulings are inlined verbatim when given ──────────

Write-Host "`n=== rulings inlined ===" -ForegroundColor Cyan
$fx7 = New-FixtureRepo -Body $cleanBody
$rulingPath = Join-Path $fx7.Repo 'ruling.md'
$rulingText = 'ZM-D-208 Ruling 2: prefer a by-ROW seam over a by-id one.'
Set-Content -LiteralPath $rulingPath -Value $rulingText -Encoding utf8 -NoNewline
$r7 = Invoke-BuildPrompt -ScriptArgs @('-Key', $fx7.Key, '-Repo', $fx7.Repo, '-Files', 'Foo.cpp', '-Boundary', 'n/a', '-Rulings', $rulingPath)
$prompt7 = Get-Content -LiteralPath (Join-Path $fx7.RunDir 'prompt.md') -Raw

Assert-That 'exits 0' { $r7.Code -eq 0 }
Assert-That 'carries the Rulings heading' { $prompt7 -match '## Rulings that govern this work' }
Assert-That 'carries the ruling text verbatim' { $prompt7.Contains($rulingText) }
Assert-That 'no Rulings heading when none were passed' { -not $promptText.Contains('## Rulings that govern this work') }

# ─── missing scratch files fail loudly rather than creating them ─────

Write-Host "`n=== missing claim scratch ===" -ForegroundColor Cyan
$fx8 = New-TempDir
'{ "project": "ZEN", "baseBranch": "master" }' | Set-Content (Join-Path $fx8 'zagent.project.json') -Encoding utf8
$r8 = Invoke-BuildPrompt -ScriptArgs @('-Key', 'ZM-000', '-Repo', $fx8, '-Files', 'Foo.cpp', '-Boundary', 'n/a')
Assert-That 'exits non-zero when ticket.json is missing' { $r8.Code -ne 0 }
Assert-That 'never creates the run directory it could not find' { -not (Test-Path -LiteralPath (Join-Path $fx8 '.zagent/run/ZM-000')) }

# ─── re-dispatch overwrites prompt.md and says so ─────

Write-Host "`n=== re-dispatch ===" -ForegroundColor Cyan
$fx9 = New-FixtureRepo -Body $cleanBody
$first = Invoke-BuildPrompt -ScriptArgs @('-Key', $fx9.Key, '-Repo', $fx9.Repo, '-Files', 'Foo.cpp', '-Boundary', 'first')
$second = Invoke-BuildPrompt -ScriptArgs @('-Key', $fx9.Key, '-Repo', $fx9.Repo, '-Files', 'Foo.cpp', '-Boundary', 'second')
$finalPrompt = Get-Content -LiteralPath (Join-Path $fx9.RunDir 'prompt.md') -Raw

Assert-That 'both dispatches succeed' { $first.Code -eq 0 -and $second.Code -eq 0 }
Assert-That 'the second write wins' { $finalPrompt -match [regex]::Escape('second') -and $finalPrompt -notmatch [regex]::Escape('first On reaching') }
Assert-That 'the overwrite is announced on stderr' { $second.Stderr -match 'already exists' }

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
