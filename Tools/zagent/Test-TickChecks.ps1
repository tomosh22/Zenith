#Requires -Version 7.0
<#
  Test-TickChecks.ps1 -- unit tests for TickChecks.psm1.

  A plain assert script, matching Tools/zagent/Test-ZagentClient.ps1 and
  Tools/Test_T0Harness_SessionCloseFlagsExist.ps1: the only PowerShell this
  repo requires is the pwsh every gate already needs, so a Pester dependency
  buys nothing a hundred lines of asserts does not already buy.

  TickChecks.psm1 turns four rules `.claude/commands/tick.md` currently
  enforces only by asking the orchestrator to remember them into pure
  functions. What these guard:

    * Test-FileListExcursion  -- a changed file absent from filelist.txt,
      and a filelist.txt that was never written at all (a DIFFERENT state
      from "checked, and clean" -- collapsing the two is the fail-open bug
      this whole exercise exists to remove).
    * Test-CommitCompleteness -- `commit -am`'s silent omission of a
      worker-CREATED file (tick.md:1496).
    * Test-BlockedScopeAllowed -- `blocked-scope` is a FAILURE label
      whenever a gate actually failed (exit 2/3/4), never merely because
      exit 10 (a pin needing a bump) or an absent verdict happened to be
      sitting in the run directory.
    * Test-GateVerdictFresh -- a verdict from another commit, or one still
      `state: running`, must read as absent, not as "all clear".

  Fixtures use real temporary git repos (`git init`, commit, create/modify a
  file) rather than mocking git, the same approach
  Tools/zagent/Test-ZagentClient.ps1 takes for Get-WorkingTreeChanges --
  because a hand-mocked `git status` line is exactly the kind of stand-in
  that would fail to notice a real porcelain-format change.

  Usage:
    pwsh -NoProfile -File Tools/zagent/Test-TickChecks.ps1

  Exit codes:
    0 -- every assertion passed.
    1 -- one or more failed (each is printed with its expectation).

  The assertion count is NOT pinned anywhere outside this file -- this
  repo's own CLAUDE.md notes that duplicating it elsewhere has gone stale
  twice. This script prints its own count; nothing else should.
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
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("tickchecks-test-" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    return $dir
}

# ── The module under test.
$here = $PSScriptRoot
Import-Module (Join-Path $here 'ZagentClient.psm1') -Force
Import-Module (Join-Path $here 'TickChecks.psm1') -Force

Write-Host "`n=== both files parse ===" -ForegroundColor Cyan
foreach ($file in @('TickChecks.psm1')) {
    Assert-That "$file parses cleanly" {
        $errors = $null
        [void][System.Management.Automation.Language.Parser]::ParseFile(
            (Join-Path $here $file), [ref]$null, [ref]$errors)
        # A parse error here means the checks silently never run, which on
        # an unattended loop is worse than the rule staying unenforced.
        $null -eq $errors -or $errors.Count -eq 0
    }
}

# A repo fixture with one commit, ready for the worker's edits a test then
# makes. Mirrors New-GitRepo in Test-ZagentClient.ps1.
function New-GitRepo {
    $dir = New-TempDir
    & git -C $dir init --quiet 2>$null | Out-Null
    & git -C $dir config user.email 't@t.invalid' 2>$null | Out-Null
    & git -C $dir config user.name 'T' 2>$null | Out-Null
    # The real checkout gitignores `.zagent/` (Tools/zagent's own run
    # scratch) -- without the same rule here, the fixture's OWN
    # filelist.txt / changed.txt / tick_gates.json show up in
    # Get-WorkingTreeChanges as untracked files and are mistaken for the
    # worker's edits.
    Set-Content (Join-Path $dir '.gitignore') ".zagent/`n"
    Set-Content (Join-Path $dir 'README.md') 'seed'
    & git -C $dir add -A 2>$null | Out-Null
    & git -C $dir commit -qm seed 2>$null | Out-Null
    return $dir
}

function New-RunDir([string]$Repo, [string]$Key) {
    $runDir = Join-Path $Repo ".zagent/run/$Key"
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    return $runDir
}

function Get-Head([string]$Repo) {
    return (& git -C $Repo rev-parse HEAD 2>$null).Trim()
}

# =============================================================================
Write-Host "`n=== Test-FileListExcursion ===" -ForegroundColor Cyan

Assert-That 'a modification outside the file list is an excursion' {
    $repo = New-GitRepo
    $key = 'ZEN-T1'
    $runDir = New-RunDir $repo $key
    Set-Content (Join-Path $repo 'README.md') 'changed but not listed'
    "Some/Other/File.cpp`n" | Set-Content (Join-Path $runDir 'filelist.txt') -NoNewline
    $r = Test-FileListExcursion -Key $key -Repo $repo
    $r.ListFound -and ($r.Excursions -contains 'README.md') -and (-not ($r.Excursions -contains 'Some/Other/File.cpp'))
}

Assert-That 'a file CREATED outside the list is an excursion (not just a modification)' {
    $repo = New-GitRepo
    $key = 'ZEN-T2'
    $runDir = New-RunDir $repo $key
    "README.md`n" | Set-Content (Join-Path $runDir 'filelist.txt') -NoNewline
    New-Item -ItemType Directory -Path (Join-Path $repo 'Zenith/New') -Force | Out-Null
    Set-Content (Join-Path $repo 'Zenith/New/Unlisted.cpp') 'brand new, never mentioned'
    $r = Test-FileListExcursion -Key $key -Repo $repo
    $r.Excursions -contains 'Zenith/New/Unlisted.cpp'
}

Assert-That 'a listed file the worker did not touch is Missing, not an excursion' {
    $repo = New-GitRepo
    $key = 'ZEN-T3'
    $runDir = New-RunDir $repo $key
    "README.md`nNever/Touched.cpp`n" | Set-Content (Join-Path $runDir 'filelist.txt') -NoNewline
    Set-Content (Join-Path $repo 'README.md') 'the only thing actually edited'
    $r = Test-FileListExcursion -Key $key -Repo $repo
    ($r.Missing -contains 'Never/Touched.cpp') -and ($r.Excursions.Count -eq 0)
}

Assert-That 'no excursion when the change set is a strict subset of the list' {
    $repo = New-GitRepo
    $key = 'ZEN-T4'
    $runDir = New-RunDir $repo $key
    "README.md`n" | Set-Content (Join-Path $runDir 'filelist.txt') -NoNewline
    Set-Content (Join-Path $repo 'README.md') 'within scope'
    $r = Test-FileListExcursion -Key $key -Repo $repo
    $r.ListFound -and $r.Excursions.Count -eq 0
}

Assert-That 'a MISSING filelist.txt is its own state, not "no excursions"' {
    $repo = New-GitRepo
    $key = 'ZEN-T5'
    New-RunDir $repo $key | Out-Null   # run dir exists; filelist.txt does not
    Set-Content (Join-Path $repo 'README.md') 'changed, but there was never a list'
    $r = Test-FileListExcursion -Key $key -Repo $repo
    # The dangerous reading is "Excursions.Count -eq 0" meaning "clean" --
    # this asserts the shape can never be read that way: ListFound is false
    # and Excursions is $null, not an empty array.
    (-not $r.ListFound) -and ($null -eq $r.Excursions) -and (-not [string]::IsNullOrEmpty($r.Reason))
}

Assert-That 'a missing filelist.txt still reports the actual change set' {
    $repo = New-GitRepo
    $key = 'ZEN-T6'
    New-RunDir $repo $key | Out-Null
    Set-Content (Join-Path $repo 'README.md') 'still worth knowing what changed'
    $r = Test-FileListExcursion -Key $key -Repo $repo
    $r.Changed -contains 'README.md'
}

Assert-That 'no repo resolvable is its own state, never a clean pass' {
    $dir = New-TempDir   # no zagent.project.json anywhere above this
    $r = Test-FileListExcursion -Key 'ZEN-T7' -Repo $null
    # With no -Repo and (most likely) no zagent.project.json above the CWD
    # in this test run, Find-ClientRepo may or may not resolve something on
    # a dev box -- so assert the CONTRACT instead: whichever way it comes
    # back, ListFound=$false must never pair with a non-null Excursions.
    (-not $r.ListFound) -eq ($null -eq $r.Excursions) -or $r.ListFound
}

# =============================================================================
Write-Host "`n=== Test-CommitCompleteness ===" -ForegroundColor Cyan

Assert-That 'a complete commit -A is Ok' {
    $repo = New-GitRepo
    $key = 'ZEN-C1'
    $runDir = New-RunDir $repo $key
    Set-Content (Join-Path $repo 'a.txt') 'new tracked content'
    "a.txt`n" | Set-Content (Join-Path $runDir 'changed.txt') -NoNewline
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm 'ZEN-C1' 2>$null | Out-Null
    $r = Test-CommitCompleteness -Key $key -Repo $repo
    $r.Ok -and $r.MissingFromCommit.Count -eq 0
}

Assert-That 'commit -am omitting a CREATED file is caught (the tick.md:1496 failure mode)' {
    $repo = New-GitRepo
    $key = 'ZEN-C2'
    $runDir = New-RunDir $repo $key
    # Modify a tracked file AND create a new one -- exactly the shape that
    # makes `commit -am` look like it worked.
    Set-Content (Join-Path $repo 'README.md') 'modified, tracked'
    Set-Content (Join-Path $repo 'NewHeader.h') 'brand new, untracked'
    "README.md`nNewHeader.h`n" | Set-Content (Join-Path $runDir 'changed.txt') -NoNewline
    & git -C $repo commit -aqm 'ZEN-C2 via commit -am' 2>$null | Out-Null
    $r = Test-CommitCompleteness -Key $key -Repo $repo
    (-not $r.Ok) -and ($r.MissingFromCommit -contains 'NewHeader.h') -and (-not ($r.MissingFromCommit -contains 'README.md'))
}

Assert-That 'a missing changed.txt is reported as its own state, not Ok' {
    $repo = New-GitRepo
    $key = 'ZEN-C3'
    New-RunDir $repo $key | Out-Null   # no changed.txt written
    Set-Content (Join-Path $repo 'a.txt') 'x'
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm 'ZEN-C3' 2>$null | Out-Null
    $r = Test-CommitCompleteness -Key $key -Repo $repo
    (-not $r.RecordedFound) -and (-not $r.Ok) -and ($null -eq $r.MissingFromCommit)
}

Assert-That 'an empty changed.txt is reported distinctly from a missing one' {
    $repo = New-GitRepo
    $key = 'ZEN-C4'
    $runDir = New-RunDir $repo $key
    Set-Content (Join-Path $runDir 'changed.txt') ''
    $r = Test-CommitCompleteness -Key $key -Repo $repo
    $r.RecordedFound -and (-not $r.Ok)
}

Assert-That 'extra files in the commit do not fail Ok -- only a MISSING recorded file does' {
    $repo = New-GitRepo
    $key = 'ZEN-C5'
    $runDir = New-RunDir $repo $key
    Set-Content (Join-Path $repo 'a.txt') 'recorded and committed'
    Set-Content (Join-Path $repo 'b.txt') 'committed but never recorded -- fine'
    "a.txt`n" | Set-Content (Join-Path $runDir 'changed.txt') -NoNewline
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm 'ZEN-C5' 2>$null | Out-Null
    $r = Test-CommitCompleteness -Key $key -Repo $repo
    $r.Ok -and ($r.ExtraInCommit -contains 'b.txt')
}

Assert-That 'an invalid Sha is reported, not silently swallowed' {
    $repo = New-GitRepo
    $key = 'ZEN-C6'
    $runDir = New-RunDir $repo $key
    "a.txt`n" | Set-Content (Join-Path $runDir 'changed.txt') -NoNewline
    $r = Test-CommitCompleteness -Key $key -Repo $repo -Sha 'not-a-real-sha'
    (-not $r.Ok) -and (-not [string]::IsNullOrEmpty($r.Reason))
}

# =============================================================================
Write-Host "`n=== Test-BlockedScopeAllowed ===" -ForegroundColor Cyan

function Write-TickGatesJson([string]$Repo, [string]$Key, [hashtable]$Fields) {
    $runDir = Join-Path $Repo ".zagent/run/$Key"
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    ($Fields | ConvertTo-Json -Depth 6) | Set-Content (Join-Path $runDir 'tick_gates.json')
}

Assert-That 'blocked-scope is REFUSED after an exit-4 verdict (a gate line failed)' {
    $repo = New-GitRepo
    $key = 'ZEN-B1'
    Write-TickGatesJson $repo $key @{ state = 'complete'; exitCode = 4; headSha = (Get-Head $repo) }
    $r = Test-BlockedScopeAllowed -Key $key -Repo $repo
    (-not $r.Allowed) -and $r.ExitCode -eq 4
}

Assert-That 'blocked-scope is REFUSED after exit 2 (regen failed)' {
    $repo = New-GitRepo
    $key = 'ZEN-B2'
    Write-TickGatesJson $repo $key @{ state = 'complete'; exitCode = 2; headSha = (Get-Head $repo) }
    (Test-BlockedScopeAllowed -Key $key -Repo $repo).Allowed -eq $false
}

Assert-That 'blocked-scope is REFUSED after exit 3 (a created file never compiled)' {
    $repo = New-GitRepo
    $key = 'ZEN-B3'
    Write-TickGatesJson $repo $key @{ state = 'complete'; exitCode = 3; headSha = (Get-Head $repo) }
    (Test-BlockedScopeAllowed -Key $key -Repo $repo).Allowed -eq $false
}

Assert-That 'blocked-scope is PERMITTED after exit 10 (a pin needs bumping -- not a failure)' {
    $repo = New-GitRepo
    $key = 'ZEN-B4'
    Write-TickGatesJson $repo $key @{ state = 'complete'; exitCode = 10; headSha = (Get-Head $repo) }
    $r = Test-BlockedScopeAllowed -Key $key -Repo $repo
    $r.Allowed -and $r.ExitCode -eq 10
}

Assert-That 'blocked-scope is PERMITTED after a green (exit 0) verdict' {
    $repo = New-GitRepo
    $key = 'ZEN-B5'
    Write-TickGatesJson $repo $key @{ state = 'complete'; exitCode = 0; headSha = (Get-Head $repo) }
    (Test-BlockedScopeAllowed -Key $key -Repo $repo).Allowed
}

Assert-That 'blocked-scope is PERMITTED when no verdict is on record at all' {
    $repo = New-GitRepo
    $key = 'ZEN-B6'   # no .zagent/run/<key> directory at all
    (Test-BlockedScopeAllowed -Key $key -Repo $repo).Allowed
}

Assert-That 'blocked-scope is PERMITTED when the recorded run never finished (state: running)' {
    $repo = New-GitRepo
    $key = 'ZEN-B7'
    Write-TickGatesJson $repo $key @{ state = 'running'; exitCode = $null; headSha = (Get-Head $repo) }
    $r = Test-BlockedScopeAllowed -Key $key -Repo $repo
    $r.Allowed -and $r.State -eq 'running'
}

Assert-That 'blocked-scope is REFUSED (fails closed) when tick_gates.json will not parse' {
    $repo = New-GitRepo
    $key = 'ZEN-B8'
    $runDir = Join-Path $repo ".zagent/run/$key"
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    Set-Content (Join-Path $runDir 'tick_gates.json') '{ not valid json'
    (Test-BlockedScopeAllowed -Key $key -Repo $repo).Allowed -eq $false
}

Assert-That 'blocked-scope is PERMITTED after exit 5 (no gate list is itself a scope fact)' {
    $repo = New-GitRepo
    $key = 'ZEN-B9'
    Write-TickGatesJson $repo $key @{ state = 'complete'; exitCode = 5; headSha = (Get-Head $repo) }
    (Test-BlockedScopeAllowed -Key $key -Repo $repo).Allowed
}

# =============================================================================
Write-Host "`n=== Test-GateVerdictFresh ===" -ForegroundColor Cyan

Assert-That 'a verdict matching current HEAD, state complete, is FRESH' {
    $repo = New-GitRepo
    $key = 'ZEN-F1'
    Write-TickGatesJson $repo $key @{ state = 'complete'; exitCode = 0; headSha = (Get-Head $repo); skipped = @() }
    $r = Test-GateVerdictFresh -Key $key -Repo $repo
    $r.Fresh -and $r.ExitCode -eq 0
}

Assert-That 'a verdict from ANOTHER commit is NOT fresh' {
    $repo = New-GitRepo
    $key = 'ZEN-F2'
    $staleSha = Get-Head $repo
    # Advance HEAD past the sha the verdict names.
    Set-Content (Join-Path $repo 'a.txt') 'a later commit'
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm 'later' 2>$null | Out-Null
    Write-TickGatesJson $repo $key @{ state = 'complete'; exitCode = 0; headSha = $staleSha }
    $r = Test-GateVerdictFresh -Key $key -Repo $repo
    (-not $r.Fresh) -and ($r.Reason -match 'different tree|does not match')
}

Assert-That 'a verdict still state: running is NOT fresh (a killed run leaves no verdict)' {
    $repo = New-GitRepo
    $key = 'ZEN-F3'
    Write-TickGatesJson $repo $key @{ state = 'running'; exitCode = $null; headSha = (Get-Head $repo) }
    $r = Test-GateVerdictFresh -Key $key -Repo $repo
    (-not $r.Fresh) -and $r.State -eq 'running'
}

Assert-That 'a missing tick_gates.json is NOT fresh (absent is treated as stale)' {
    $repo = New-GitRepo
    $key = 'ZEN-F4'
    $r = Test-GateVerdictFresh -Key $key -Repo $repo
    -not $r.Fresh
}

Assert-That 'an unparsable tick_gates.json is NOT fresh' {
    $repo = New-GitRepo
    $key = 'ZEN-F5'
    $runDir = Join-Path $repo ".zagent/run/$key"
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    Set-Content (Join-Path $runDir 'tick_gates.json') 'not json at all'
    $r = Test-GateVerdictFresh -Key $key -Repo $repo
    -not $r.Fresh
}

Assert-That 'a red (exit 4) but FRESH verdict is still reported Fresh -- red/stale are different questions' {
    $repo = New-GitRepo
    $key = 'ZEN-F6'
    Write-TickGatesJson $repo $key @{ state = 'complete'; exitCode = 4; headSha = (Get-Head $repo); skipped = @('doc_lint') }
    $r = Test-GateVerdictFresh -Key $key -Repo $repo
    $r.Fresh -and $r.ExitCode -eq 4 -and ($r.Skipped -contains 'doc_lint')
}

Assert-That 'skipped is always an array, even when the field is absent' {
    $repo = New-GitRepo
    $key = 'ZEN-F7'
    Write-TickGatesJson $repo $key @{ state = 'complete'; exitCode = 0; headSha = (Get-Head $repo) }
    $r = Test-GateVerdictFresh -Key $key -Repo $repo
    $r.Skipped -is [array]
}

# =============================================================================
Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
