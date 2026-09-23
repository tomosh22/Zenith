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

    # A REAL git repo, not just a directory carrying zagent.project.json --
    # build_prompt.ps1 now writes predispatch.json (the WORKER-MOVED-NOTHING
    # baseline), which reads `git rev-parse HEAD`/`--abbrev-ref HEAD`/`status
    # --porcelain` against `-Repo`. Every real invocation of this script runs
    # inside an actual checkout; a fixture that is not one would make every
    # assertion below fail on the git calls rather than on what it means to
    # test.
    & git -C $repo init -q 2>$null | Out-Null
    & git -C $repo config user.name 'Build prompt tests' 2>$null | Out-Null
    & git -C $repo config user.email 'build-prompt-tests@example.test' 2>$null | Out-Null
    # `.zagent/` is gitignored in the real checkout (CLAUDE.md: "Run scratch
    # ... (gitignored)"), which is what keeps `git status --porcelain` clean
    # while a ticket is in flight -- mirror that here, or the run scratch
    # this fixture is about to write under it (ticket.json, body.md, and now
    # predispatch.json itself) shows up as an untracked line and no fixture
    # ever reports a clean porcelain set.
    '.zagent/' | Set-Content (Join-Path $repo '.gitignore') -Encoding utf8
    'seed' | Set-Content (Join-Path $repo 'seed.txt') -Encoding utf8
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm 'seed' 2>$null | Out-Null

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
# The survey clause is the worker's FIRST instruction (ZEN-89): the file list
# is a hypothesis it must falsify before editing, and a prompt that opens with
# the prohibitions instead buries the one control that catches an under-scoped
# list. Order is asserted, not just presence, because "somewhere in the
# prompt" is how it stops being step zero.
Assert-That 'opens with the blast-radius survey clause' {
    $promptText.TrimStart().StartsWith('**Step zero, before you edit anything.**')
}
Assert-That 'the survey clause demands a STOP on any hit outside the list' {
    $promptText -match 'STOP and report the complete corrected list before making a single edit'
}
# E2: every path in the prompt is relative to SOMETHING, and step zero is
# the worker's first instruction -- so the repo root belongs right after
# the clause's bold opener, POSIX-normalised the same way filelist.txt and
# the Files clause already are (ConvertTo-PosixPath), not the fixture's
# native Windows path.
$expectedRepoRoot = ($fx.Repo -replace '\\', '/') -replace '/+$', ''
Assert-That 'the survey clause states the repository root, POSIX-normalised' {
    $promptText.Contains($expectedRepoRoot)
}
Assert-That 'the repository-root sentence sits immediately after the bold opener' {
    $promptText.TrimStart().StartsWith('**Step zero, before you edit anything.** Every path in this prompt')
}
Assert-That 'the repository-root sentence appears before the Files clause' {
    $promptText.IndexOf($expectedRepoRoot) -lt $promptText.IndexOf('**Files you may edit:**')
}
Assert-That 'carries the DO NOT clause, after the survey' {
    ($promptText -match '\*\*DO NOT\*\*') -and
    ($promptText.IndexOf('**Step zero') -lt $promptText.IndexOf('**DO NOT**'))
}
Assert-That 'carries the Files clause' { $promptText -match '\*\*Files you may edit:\*\* Games/Zenithmon/Source/Foo\.cpp\.' }
Assert-That 'carries the Boundary text' { $promptText -match [regex]::Escape('Stop once both items are wired.') }
Assert-That 'carries the Goal section' { $promptText -match '## Goal(\r?\n)+Wire the reward path\.' }
Assert-That 'carries the rest of the body after the DoD' { $promptText -match 'Some Other Section' }
Assert-That 'the rest-of-body section appears AFTER the Definition of Done heading' {
    $promptText.IndexOf('## Definition of Done') -lt $promptText.IndexOf('Some Other Section')
}

# ─── predispatch.json -- the WORKER-MOVED-NOTHING baseline ────────────
#
# Tools/Vault/cli/tick-checks.mjs's `workerMovedNothing` treats a missing or
# incomplete baseline as UNVERIFIED, never `clean` -- so every field it reads
# must actually be present, not merely "the file exists".

Write-Host "`n=== predispatch.json ===" -ForegroundColor Cyan
$predispatchPath = Join-Path $fx.RunDir 'predispatch.json'
Assert-That 'writes predispatch.json' { Test-Path -LiteralPath $predispatchPath }
$predispatch = Get-Content -LiteralPath $predispatchPath -Raw | ConvertFrom-Json
$expectedHead = (& git -C $fx.Repo rev-parse HEAD 2>$null)
$expectedBranch = (& git -C $fx.Repo rev-parse --abbrev-ref HEAD 2>$null)

Assert-That 'predispatch.json head matches the repo HEAD at dispatch time' { $predispatch.head -eq $expectedHead }
Assert-That 'predispatch.json branch matches the current branch' { $predispatch.branch -eq $expectedBranch }
Assert-That 'predispatch.json carries a porcelain array (empty on a clean tree)' {
    $null -ne $predispatch.PSObject.Properties['porcelain'] -and @($predispatch.porcelain).Count -eq 0
}
Assert-That 'predispatch.json carries a boardRow object' { $null -ne $predispatch.PSObject.Properties['boardRow'] }
Assert-That 'predispatch.json carries an "at" timestamp' { -not [string]::IsNullOrWhiteSpace($predispatch.at) }
Assert-That 'predispatch.json harness is JSON null when the ticket carries none' {
    $prop = $predispatch.PSObject.Properties['harness']
    ($null -ne $prop) -and ($null -eq $prop.Value)
}

Write-Host "`n=== predispatch.json carries a claimed harness ===" -ForegroundColor Cyan
$fxH = New-FixtureRepo -Body $cleanBody -ExtraTicketFields @{ harness = 'claude'; status = 'In Progress'; assignee = 'agent@example.test' }
$rH = Invoke-BuildPrompt -ScriptArgs @('-Key', $fxH.Key, '-Repo', $fxH.Repo, '-Files', 'Foo.cpp', '-Boundary', 'n/a')
Assert-That 'exits 0' { $rH.Code -eq 0 }
$predispatchH = Get-Content -LiteralPath (Join-Path $fxH.RunDir 'predispatch.json') -Raw | ConvertFrom-Json
Assert-That 'predispatch.json harness is copied from ticket.json' { $predispatchH.harness -eq 'claude' }
Assert-That 'predispatch.json boardRow.status is copied from ticket.json' { $predispatchH.boardRow.status -eq 'In Progress' }
Assert-That 'predispatch.json boardRow.assignee is copied from ticket.json' { $predispatchH.boardRow.assignee -eq 'agent@example.test' }

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

# ─── E1: -Rulings a,b fails under `pwsh -File` unless split like -Files ──
#
# Mirrors the '-Files' comma-joined case above: a nested
# `pwsh -File build_prompt.ps1 -Rulings a.md,b.md` arrives as ONE argv
# token with the comma still embedded, so a naive `[string[]]$Rulings`
# binds a single element holding the literal text "a.md,b.md" -- which
# then fails Test-Path outright ('-Rulings path ... does not exist').
Write-Host "`n=== rulings: comma-joined -Rulings a,b (E1) ===" -ForegroundColor Cyan
$fx7b = New-FixtureRepo -Body $cleanBody
$rulingPathA = Join-Path $fx7b.Repo 'rulingA.md'
$rulingPathB = Join-Path $fx7b.Repo 'rulingB.md'
$rulingTextA = 'Ruling A: prefer the by-row seam.'
$rulingTextB = 'Ruling B: never bake a pose change.'
Set-Content -LiteralPath $rulingPathA -Value $rulingTextA -Encoding utf8 -NoNewline
Set-Content -LiteralPath $rulingPathB -Value $rulingTextB -Encoding utf8 -NoNewline
$rulingPaths = @($rulingPathA, $rulingPathB)
# Passed as ONE comma-joined argv token -- the shape a real
# `-Rulings a.md,b.md` invocation actually arrives in once it has crossed
# the `pwsh -File` process boundary, same reasoning as the -Files case.
$r7b = Invoke-BuildPrompt -ScriptArgs @('-Key', $fx7b.Key, '-Repo', $fx7b.Repo, '-Files', 'Foo.cpp', '-Boundary', 'n/a', '-Rulings', ($rulingPaths -join ','))
$prompt7b = Get-Content -LiteralPath (Join-Path $fx7b.RunDir 'prompt.md') -Raw

Assert-That 'exits 0 with a comma-joined -Rulings value' { $r7b.Code -eq 0 }
Assert-That 'carries the Rulings heading' { $prompt7b -match '## Rulings that govern this work' }
Assert-That 'both ruling files are inlined' { $prompt7b.Contains($rulingTextA) -and $prompt7b.Contains($rulingTextB) }
Assert-That 'ruling A appears before ruling B, in order' { $prompt7b.IndexOf($rulingTextA) -lt $prompt7b.IndexOf($rulingTextB) }

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
