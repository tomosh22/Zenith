#Requires -Version 7.0
<#
  Test-TickWiring.ps1 — unit tests for the four TickChecks.psm1 rules as
  WIRED into zagent.ps1 (the `finish` excursion/commit refusals, the
  `update --label blocked-scope` refusal, `guard`'s freshness fold, and the
  client-composed `abandon` verb) -- plus the two checks ported FROM the
  Vault backend (Tools/Vault/cli/tick-checks.mjs): durable findings at
  `finish`, and the `needs-gpu` companion-label requirement at
  `update --label`. Those two exist so a `backend.json` flip cannot
  silently drop either check the way the durable-findings rule was
  silently dropped once already -- see ZagentClient.psm1's own comments
  on Get-DurableFindingsAnswer and Test-NeedsGpuCompanion for the incident.

  A SEPARATE file from Test-ZagentClient.ps1 and Test-TickChecks.ps1 on
  purpose — a second agent may be running either of those concurrently,
  and this repo's own file-ownership rule for this ticket says not to
  collide with them.

  zagent.ps1 itself runs a command the moment it is invoked (see its own
  module header comments on this), so nothing in ITS main body is
  reachable from a test without a live board. The wiring this file adds
  there is therefore kept to the smallest possible glue, with every actual
  DECISION pulled into small, pure, exported functions in
  ZagentClient.psm1 (Get-LabelList, Get-UnexplainedExcursions,
  Get-ForbiddenAbandonFlags, Test-GuardNeedsFreshnessOverride) that this
  file calls directly — the same pattern Get-UnmetDoneCriteria,
  Get-GuardTicketKey and Test-NeedsChangedSet already establish for
  exactly this reason.

  What is NOT re-tested here: the four TickChecks.psm1 functions'  own
  filesystem/git behaviour (missing file, unparsable JSON, stale headSha,
  and so on) — that is Test-TickChecks.ps1's job, and it is green at
  30/30. This file tests the layer ABOVE it: does zagent.ps1's wiring
  combine those answers into the right refusal, for the scenarios the
  ticket names explicitly.

  Usage:
    pwsh -NoProfile -File Tools/zagent/Test-TickWiring.ps1

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
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("zagent-wiring-" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    return $dir
}

$here = $PSScriptRoot
# ★ ORDER MATTERS. TickChecks.psm1 imports ZagentClient.psm1 internally as
# a NESTED module while it loads; importing ZagentClient.psm1 first and
# TickChecks.psm1 second leaves ZagentClient's functions invisible outside
# TickChecks (its own Export-ModuleMember never re-exports them), and the
# nested `-Force` reload replaces the earlier top-level module entry
# entirely -- `Get-Module` afterwards shows only TickChecks, and every
# ZagentClient function this file needs (Get-LabelList and the rest)
# comes back "not recognized". Importing TickChecks FIRST and
# ZagentClient SECOND leaves both loaded and both exporting, because the
# second, top-level import re-establishes ZagentClient globally after the
# nested one. Measured while writing this file; get the order backwards
# and every assertion in sections A/C/D/E fails on a name-not-found
# rather than a real mismatch.
Import-Module (Join-Path $here 'TickChecks.psm1') -Force
Import-Module (Join-Path $here 'ZagentClient.psm1') -Force

Write-Host "`n=== both new files parse, and zagent.ps1 still parses with the new wiring ===" -ForegroundColor Cyan
foreach ($file in @('zagent.ps1', 'ZagentClient.psm1', 'TickChecks.psm1')) {
    Assert-That "$file parses cleanly" {
        $errors = $null
        [void][System.Management.Automation.Language.Parser]::ParseFile(
            (Join-Path $here $file), [ref]$null, [ref]$errors)
        $null -eq $errors -or $errors.Count -eq 0
    }
}

# ─── A. Excursion explanations (Get-UnexplainedExcursions) ───────────

Write-Host "`n=== A. excursion explanations are mandatory ===" -ForegroundColor Cyan

Assert-That '** an unexplained excursion is refused (comes back non-empty)' {
    $unexplained = Get-UnexplainedExcursions -Excursions @('Flux/Stray.cpp') `
        -ExplanationText "## Work Log`n`nDid the ticket's ordinary work, nothing unusual."
    (@($unexplained).Count -eq 1) -and (@($unexplained)[0] -eq 'Flux/Stray.cpp')
}

Assert-That '** the SAME excursion, with its path named in the comment/worklog text, is allowed' {
    $unexplained = Get-UnexplainedExcursions -Excursions @('Flux/Stray.cpp') `
        -ExplanationText "## Excursions`n`n- Flux/Stray.cpp -- needed to fix a shared header the ticket's file list omitted."
    (@($unexplained).Count -eq 0)
}

Assert-That 'the explanation is accepted ANYWHERE in the text, not just under an ## Excursions heading' {
    # The ticket is explicit: accept the path appearing anywhere in either
    # file. A worker that explains itself in prose under Deviations, or in
    # the --comment file rather than the --worklog, must not be refused.
    $unexplained = Get-UnexplainedExcursions -Excursions @('Tools/Helper.ps1') `
        -ExplanationText "**Deviations / follow-ups**`n`n- Also touched Tools/Helper.ps1 to fix a shared regex bug."
    (@($unexplained).Count -eq 0)
}

Assert-That 'the match is case-insensitive, the same as a Windows path comparison' {
    $unexplained = Get-UnexplainedExcursions -Excursions @('Flux/Stray.cpp') `
        -ExplanationText 'touched flux/stray.cpp for a shared header'
    (@($unexplained).Count -eq 0)
}

Assert-That 'two excursions, one explained and one not, reports only the unexplained one' {
    $unexplained = Get-UnexplainedExcursions -Excursions @('a/one.cpp', 'b/two.cpp') `
        -ExplanationText 'fixed a/one.cpp along the way'
    (@($unexplained).Count -eq 1) -and (@($unexplained)[0] -eq 'b/two.cpp')
}

Assert-That 'no excursions at all is a clean, empty result' {
    # NOT `@(Get-UnexplainedExcursions ...)`. The function already returns
    # `,@()` so a direct assignment keeps "empty" empty; wrapping the CALL
    # in `@()` on top of that undoes the trick and turns the empty array
    # into a ONE-element array holding an empty array -- the exact trap
    # TickChecks.psm1's own header warns about, measured here while
    # writing this test.
    $result = Get-UnexplainedExcursions -Excursions @() -ExplanationText 'anything'
    $result.Count -eq 0
}

Assert-That 'a $null Excursions list (nothing to explain) is a clean, empty result, not a throw' {
    $result = Get-UnexplainedExcursions -Excursions $null -ExplanationText 'anything'
    $result.Count -eq 0
}

Assert-That 'empty or $null explanation text leaves every excursion unexplained, not a throw' {
    (@(Get-UnexplainedExcursions -Excursions @('a.cpp') -ExplanationText '')).Count -eq 1 -and
    (@(Get-UnexplainedExcursions -Excursions @('a.cpp') -ExplanationText $null)).Count -eq 1
}

Assert-That 'returns an ARRAY for a single unexplained excursion, not an unrolled string' {
    (Get-UnexplainedExcursions -Excursions @('a.cpp') -ExplanationText '') -is [array]
}

Write-Host "`n=== A2. a missing filelist.txt is UNVERIFIED, never coerced into ""clean"" ===" -ForegroundColor Cyan
# This is the fact zagent.ps1's finish interception keys its warn-vs-refuse
# branch on: `if (-not $excursion.ListFound) { warn } elseif (...)`. If
# ListFound:$false ever collapsed into Excursions:@() (clean), the elseif
# would silently pass every ticket with no filelist.txt at all -- the exact
# fail-open shape TickChecks.psm1's own header names as the thing this
# whole module exists to prevent.
function New-WiringGitRepo {
    $dir = New-TempDir
    & git -C $dir init --quiet 2>$null | Out-Null
    & git -C $dir config user.email 't@t.invalid' 2>$null | Out-Null
    & git -C $dir config user.name 'T' 2>$null | Out-Null
    Set-Content (Join-Path $dir 'tracked.txt') 'x'
    & git -C $dir add -A 2>$null | Out-Null
    & git -C $dir commit -qm init 2>$null | Out-Null
    return $dir
}

Assert-That '** a missing filelist.txt reports ListFound:$false and Excursions:$null -- never Excursions:@()' {
    $repo = New-WiringGitRepo
    try {
        $result = Test-FileListExcursion -Key 'ZM-1' -Repo $repo
        (-not $result.ListFound) -and ($null -eq $result.Excursions) -and ($null -ne $result.Reason)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'the wiring branch that reads ListFound never falls through to the excursion-explanation check' {
    # Mirrors zagent.ps1's own `if (-not $excursion.ListFound) { warn }
    # elseif (@($excursion.Excursions).Count -gt 0) { ... }` -- proves the
    # two arms are mutually exclusive for the state that matters: a
    # missing list takes the WARN arm, full stop, regardless of what the
    # working tree changed.
    $repo = New-WiringGitRepo
    try {
        New-Item -ItemType Directory -Path (Join-Path $repo 'New') -Force | Out-Null
        Set-Content (Join-Path $repo 'New/Untracked.cpp') 'x'
        $excursion = Test-FileListExcursion -Key 'ZM-1' -Repo $repo
        $tookWarnArm = -not $excursion.ListFound
        $tookExplanationArm = $excursion.ListFound -and (@($excursion.Excursions).Count -gt 0)
        $tookWarnArm -and (-not $tookExplanationArm)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

# ─── B. Commit completeness, gated to Done only ──────────────────────

Write-Host "`n=== B. commit completeness is refused at Done ===" -ForegroundColor Cyan

function New-CommitCompletenessRepo {
    <# A repo where `changed.txt` (what step 6.4 recorded) names a file
       that a `commit -am`-style commit then leaves behind -- the exact
       failure tick.md:1496 names. #>
    $repo = New-TempDir
    & git -C $repo init --quiet 2>$null | Out-Null
    & git -C $repo config user.email 't@t.invalid' 2>$null | Out-Null
    & git -C $repo config user.name 'T' 2>$null | Out-Null
    Set-Content (Join-Path $repo 'existing.cpp') 'int x;'
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm init 2>$null | Out-Null

    Set-Content (Join-Path $repo 'existing.cpp') 'int x; // modified'
    Set-Content (Join-Path $repo 'created.cpp') 'int y;'
    $runDir = Join-Path $repo '.zagent/run/ZM-1'
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $runDir 'changed.txt') "existing.cpp`ncreated.cpp`n" -Encoding utf8

    # `commit -am`-equivalent: only the tracked modification is staged, so
    # `created.cpp` never reaches the commit.
    & git -C $repo add existing.cpp 2>$null | Out-Null
    & git -C $repo commit -qm "ZM-1: partial" 2>$null | Out-Null
    return $repo
}

Assert-That '** a commit missing a created file is refused when the claimed status is Done' {
    $repo = New-CommitCompletenessRepo
    try {
        $check = Test-CommitCompleteness -Key 'ZM-1' -Repo $repo
        $statusNorm = 'Done'.Trim().ToLowerInvariant()
        $refuses = ($statusNorm -eq 'done') -and (@($check.MissingFromCommit).Count -gt 0)
        $refuses -and (@($check.MissingFromCommit) -contains 'created.cpp')
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'the same incomplete commit does NOT refuse a Blocked finish -- Blocked may legitimately carry no commit at all' {
    # tick.md:1480-1487: a disputed-value ticket is explicitly REVERTED to
    # a clean tree rather than committed, so gating Blocked on commit
    # completeness would refuse a ticket that did exactly what the
    # protocol asked. The wiring only ever evaluates this at Done.
    $repo = New-CommitCompletenessRepo
    try {
        $check = Test-CommitCompleteness -Key 'ZM-1' -Repo $repo
        foreach ($claimed in @('Blocked', 'In Review', 'in review')) {
            $statusNorm = $claimed.Trim().ToLowerInvariant()
            $wouldRefuse = ($statusNorm -eq 'done') -and (@($check.MissingFromCommit).Count -gt 0)
            if ($wouldRefuse) { return $false }
        }
        # Sanity: the underlying check DID find the omission -- this is
        # testing the status gate, not the detector.
        (@($check.MissingFromCommit).Count -gt 0)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'the Done comparison is case- and whitespace-insensitive, matching zagent.ps1''s own normalisation' {
    foreach ($claimed in @(' Done ', 'DONE', 'done', 'DoNe')) {
        if ($claimed.Trim().ToLowerInvariant() -ne 'done') { return $false }
    }
    $true
}

Assert-That 'a complete commit is NOT refused at Done' {
    $repo = New-TempDir
    try {
        & git -C $repo init --quiet 2>$null | Out-Null
        & git -C $repo config user.email 't@t.invalid' 2>$null | Out-Null
        & git -C $repo config user.name 'T' 2>$null | Out-Null
        Set-Content (Join-Path $repo 'a.cpp') 'x'
        & git -C $repo add -A 2>$null | Out-Null
        & git -C $repo commit -qm init 2>$null | Out-Null
        Set-Content (Join-Path $repo 'created.cpp') 'y'
        $runDir = Join-Path $repo '.zagent/run/ZM-2'
        New-Item -ItemType Directory -Path $runDir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $runDir 'changed.txt') "created.cpp`n" -Encoding utf8
        & git -C $repo add -A 2>$null | Out-Null
        & git -C $repo commit -qm "ZM-2: complete" 2>$null | Out-Null
        $check = Test-CommitCompleteness -Key 'ZM-2' -Repo $repo
        $statusNorm = 'Done'.Trim().ToLowerInvariant()
        -not (($statusNorm -eq 'done') -and (@($check.MissingFromCommit).Count -gt 0))
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That '** a MISSING changed.txt at Done does not crash and does not falsely refuse' {
    # Regression for a real bug the SMOKE TEST caught (not any unit test):
    # `MissingFromCommit` is $null (never an empty array) when changed.txt
    # is absent -- TickChecks.psm1's own documented contract. `@($null)`
    # is NOT an empty array: it wraps $null into a ONE-element array
    # (Count 1), so a naive `@($check.MissingFromCommit).Count -gt 0`
    # reads TRUE for a ticket with nothing recorded at all, and the
    # message that used to follow crashed calling `.Count` directly on
    # $null under Set-StrictMode -- a real `finish ... --status Done`
    # threw "The property 'Count' cannot be found on this object" instead
    # of either passing or refusing cleanly.
    $repo = New-TempDir
    try {
        & git -C $repo init --quiet 2>$null | Out-Null
        & git -C $repo config user.email 't@t.invalid' 2>$null | Out-Null
        & git -C $repo config user.name 'T' 2>$null | Out-Null
        Set-Content (Join-Path $repo 'a.cpp') 'x'
        & git -C $repo add -A 2>$null | Out-Null
        & git -C $repo commit -qm init 2>$null | Out-Null
        # No .zagent/run/ZM-3/changed.txt at all.
        $check = Test-CommitCompleteness -Key 'ZM-3' -Repo $repo

        # The naive shape really does misread this as "one item", which is
        # why the fix has to check for $null explicitly rather than trust
        # @() wrapping alone -- if this ever stopped being true the bug
        # this test guards would no longer exist, which is fine, but the
        # assertion below is what actually matters.
        $naiveWrapMisreadsIt = (@($check.MissingFromCommit)).Count -eq 1

        # Mirrors zagent.ps1's own fixed branch order exactly: check $null
        # FIRST (warn, never refuse), only fall through to a real Count
        # check once MissingFromCommit is confirmed to be an array.
        $wiringRefuses = $false
        if ($null -eq $check.MissingFromCommit) {
            $wiringRefuses = $false
        } elseif (@($check.MissingFromCommit).Count -gt 0) {
            $wiringRefuses = $true
        }

        $naiveWrapMisreadsIt -and (-not $wiringRefuses) -and ($null -eq $check.MissingFromCommit)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

# ─── C. `blocked-scope` must be defensible ────────────────────────────

Write-Host "`n=== C. blocked-scope is refused after a red verdict, allowed after exit 10 ===" -ForegroundColor Cyan

function New-TickGatesRepo {
    param([int]$ExitCode, [string]$State = 'complete')
    $repo = New-TempDir
    $runDir = Join-Path $repo '.zagent/run/ZM-9'
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    $verdict = [ordered]@{ state = $State; exitCode = $ExitCode; headSha = 'deadbeef' }
    ($verdict | ConvertTo-Json) | Set-Content -LiteralPath (Join-Path $runDir 'tick_gates.json') -Encoding utf8
    return $repo
}

Assert-That '** --label blocked-scope is refused after a RED verdict (exit 4 -- a gate line failed)' {
    $repo = New-TickGatesRepo -ExitCode 4
    try {
        $labels = Get-LabelList -Value 'blocked-scope'
        $scopeCheck = Test-BlockedScopeAllowed -Key 'ZM-9' -Repo $repo
        ($labels -contains 'blocked-scope') -and (-not $scopeCheck.Allowed)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That '** --label blocked-scope is ALLOWED after exit 10 (a pin needs bumping -- not a failure)' {
    $repo = New-TickGatesRepo -ExitCode 10
    try {
        $labels = Get-LabelList -Value 'blocked-scope'
        $scopeCheck = Test-BlockedScopeAllowed -Key 'ZM-9' -Repo $repo
        ($labels -contains 'blocked-scope') -and $scopeCheck.Allowed
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a comma-separated --label list is split correctly, and blocked-scope is found among others' {
    $labels = Get-LabelList -Value 'needs-gpu, blocked-scope ,deferred'
    ($labels.Count -eq 3) -and ($labels -contains 'blocked-scope') -and
    ($labels -contains 'needs-gpu') -and ($labels -contains 'deferred')
}

Assert-That 'a --label list WITHOUT blocked-scope never triggers the check (no key needed)' {
    $labels = Get-LabelList -Value 'needs-human,deferred'
    -not ($labels -contains 'blocked-scope')
}

Assert-That 'Get-LabelList tolerates $null and empty values without throwing' {
    ((Get-LabelList -Value $null).Count -eq 0) -and ((Get-LabelList -Value '').Count -eq 0) -and
    ((Get-LabelList -Value ' , ,').Count -eq 0)
}

Assert-That 'Get-LabelList returns an ARRAY for one label, not an unrolled string' {
    (Get-LabelList -Value 'blocked-scope') -is [array]
}

# ─── D. guard's freshness fold ────────────────────────────────────────

Write-Host "`n=== D. guard fails on a stale headSha, but a board-side failure surfaces as itself ===" -ForegroundColor Cyan

Assert-That '** a stale headSha overrides a board PASS (exitCode 0) into a failure' {
    Test-GuardNeedsFreshnessOverride -BoardExitCode 0 -Fresh $false
}

Assert-That '** a board-side guard FAILURE (non-zero) is never touched, fresh or not -- it already carries its own reason' {
    (-not (Test-GuardNeedsFreshnessOverride -BoardExitCode 4 -Fresh $false)) -and
    (-not (Test-GuardNeedsFreshnessOverride -BoardExitCode 5 -Fresh $true))
}

Assert-That 'a board PASS with a FRESH verdict is left alone' {
    -not (Test-GuardNeedsFreshnessOverride -BoardExitCode 0 -Fresh $true)
}

Assert-That 'end to end: a verdict at a DIFFERENT headSha is not fresh, and that overrides a board pass' {
    $repo = New-TempDir
    try {
        & git -C $repo init --quiet 2>$null | Out-Null
        & git -C $repo config user.email 't@t.invalid' 2>$null | Out-Null
        & git -C $repo config user.name 'T' 2>$null | Out-Null
        Set-Content (Join-Path $repo 'a.txt') 'x'
        & git -C $repo add -A 2>$null | Out-Null
        & git -C $repo commit -qm init 2>$null | Out-Null
        $runDir = Join-Path $repo '.zagent/run/ZM-10'
        New-Item -ItemType Directory -Path $runDir -Force | Out-Null
        $verdict = [ordered]@{ state = 'complete'; exitCode = 0; headSha = 'not-the-real-sha' }
        ($verdict | ConvertTo-Json) | Set-Content -LiteralPath (Join-Path $runDir 'tick_gates.json') -Encoding utf8

        $fresh = Test-GateVerdictFresh -Key 'ZM-10' -Repo $repo
        (-not $fresh.Fresh) -and (Test-GuardNeedsFreshnessOverride -BoardExitCode 0 -Fresh $fresh.Fresh)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'end to end: a verdict matching current HEAD is fresh, and a board pass is left alone' {
    $repo = New-TempDir
    try {
        & git -C $repo init --quiet 2>$null | Out-Null
        & git -C $repo config user.email 't@t.invalid' 2>$null | Out-Null
        & git -C $repo config user.name 'T' 2>$null | Out-Null
        Set-Content (Join-Path $repo 'a.txt') 'x'
        & git -C $repo add -A 2>$null | Out-Null
        & git -C $repo commit -qm init 2>$null | Out-Null
        $head = (& git -C $repo rev-parse HEAD).Trim()

        $runDir = Join-Path $repo '.zagent/run/ZM-11'
        New-Item -ItemType Directory -Path $runDir -Force | Out-Null
        $verdict = [ordered]@{ state = 'complete'; exitCode = 0; headSha = $head }
        ($verdict | ConvertTo-Json) | Set-Content -LiteralPath (Join-Path $runDir 'tick_gates.json') -Encoding utf8

        $fresh = Test-GateVerdictFresh -Key 'ZM-11' -Repo $repo
        $fresh.Fresh -and (-not (Test-GuardNeedsFreshnessOverride -BoardExitCode 0 -Fresh $fresh.Fresh))
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

# ─── E. the `abandon` verb ─────────────────────────────────────────────

Write-Host "`n=== E. abandon rejects --status/--assignee/--label, and dispatches comment ===" -ForegroundColor Cyan

Assert-That '** --status is rejected' {
    (Get-ForbiddenAbandonFlags -Argv @('abandon', 'ZM-1', '--status', 'Done', '--comment', 'c.md')) -contains 'status'
}

Assert-That '** --assignee is rejected' {
    (Get-ForbiddenAbandonFlags -Argv @('abandon', 'ZM-1', '--assignee', 'none', '--comment', 'c.md')) -contains 'assignee'
}

Assert-That '** --label is rejected' {
    (Get-ForbiddenAbandonFlags -Argv @('abandon', 'ZM-1', '--label', 'needs-human', '--comment', 'c.md')) -contains 'label'
}

Assert-That 'the --name=value spelling is caught too, not just --name value' {
    (Get-ForbiddenAbandonFlags -Argv @('abandon', 'ZM-1', '--status=Done')) -contains 'status'
}

Assert-That 'a bare trailing flag with no value is still caught (Get-FlagValue would return $null here)' {
    (Get-ForbiddenAbandonFlags -Argv @('abandon', 'ZM-1', '--comment', 'c.md', '--assignee')) -contains 'assignee'
}

Assert-That 'a legitimate abandon call (--comment only) has NO forbidden flags' {
    (Get-ForbiddenAbandonFlags -Argv @('abandon', 'ZM-1', '--comment', 'c.md', '--json')).Count -eq 0
}

Assert-That 'multiple forbidden flags are ALL reported, not just the first' {
    $flags = Get-ForbiddenAbandonFlags -Argv @('abandon', 'ZM-1', '--status', 'Done', '--assignee', 'none')
    ($flags -contains 'status') -and ($flags -contains 'assignee') -and ($flags.Count -eq 2)
}

Assert-That 'returns an ARRAY when empty, not $null -- a caller can always .Count it' {
    $flags = Get-ForbiddenAbandonFlags -Argv @('abandon', 'ZM-1', '--comment', 'c.md')
    ($flags -is [array]) -and ($flags.Count -eq 0)
}

Write-Host "`n=== E2. abandon rewrites its argv into a `comment` call, unchanged otherwise ===" -ForegroundColor Cyan
# zagent.ps1 does `$argv = @('comment', $key) + @($argv | Select-Object -Skip 2)`
# once Get-ForbiddenAbandonFlags comes back empty. Exercised directly here
# so the rewrite itself -- not just the flag scan -- has a test, without
# needing to invoke zagent.ps1's main body (which would need a live board).
Assert-That '** abandon <KEY> --comment <file> --json rewrites to comment <KEY> --comment <file> --json' {
    $argv = @('abandon', 'ZM-1', '--comment', 'c.md', '--json')
    $key = $argv[1]
    $rewritten = @('comment', $key) + @($argv | Select-Object -Skip 2)
    ($rewritten.Count -eq 5) -and ($rewritten[0] -eq 'comment') -and ($rewritten[1] -eq 'ZM-1') -and
    ($rewritten[2] -eq '--comment') -and ($rewritten[3] -eq 'c.md') -and ($rewritten[4] -eq '--json')
}

Assert-That 'the rewritten argv is still recognised as path-valued for --comment (Get-FileFlags)' {
    # Confirms the rewrite lands somewhere the existing file-slurp logic
    # already understands, rather than needing its own special case.
    $rewritten = @('comment', 'ZM-1', '--comment', 'c.md')
    (Get-FileFlags -Argv $rewritten) -contains 'comment'
}

# ─── F. durable findings must be answered, not skipped ────────────────
# Ported FROM Tools/Vault/cli/tick-checks.mjs's `durableFindingsAnswer` --
# this rule used to live ONLY in this client, tick.md documented it as
# enforced, and it silently stopped running in production the moment
# `backend.json` cut over to Vault, because the other side never grew an
# equivalent. Get-DurableFindingsAnswer is the faithful port back.

Write-Host "`n=== F. durable findings must be answered, not skipped ===" -ForegroundColor Cyan

Assert-That '** a work log with NO durable-findings line refuses (Present:$false)' {
    $r = Get-DurableFindingsAnswer -WorkLog "## Work Log`n`n**Outcome**: merged`n"
    (-not $r.Present) -and ($null -eq $r.Answer) -and ($null -ne $r.Reason)
}

Assert-That '** "none" passes (Answer: none)' {
    $r = Get-DurableFindingsAnswer -WorkLog "**Durable findings**: none`n"
    $r.Present -and ($r.Answer -eq 'none') -and ($null -eq $r.Reason)
}

Assert-That '** a ZM-D-208-style decision reference passes (Answer: references)' {
    $r = Get-DurableFindingsAnswer -WorkLog "**Durable findings**: ZM-D-208`n"
    $r.Present -and ($r.Answer -eq 'references') -and ($r.References -contains 'ZM-D-208')
}

Assert-That '** a malformed answer (neither none nor a reference) refuses' {
    $r = Get-DurableFindingsAnswer -WorkLog "**Durable findings**: probably nothing important`n"
    $r.Present -and ($null -eq $r.Answer) -and ($r.Reason -match 'names neither')
}

Assert-That 'a heading form with the answer on the NEXT line is accepted' {
    $r = Get-DurableFindingsAnswer -WorkLog "## Durable findings`n`nZM-D-208, ZEN-D-3`n"
    ($r.Answer -eq 'references') -and ($r.References.Count -eq 2) -and
    ($r.References -contains 'ZM-D-208') -and ($r.References -contains 'ZEN-D-3')
}

Assert-That 'multiple references are all captured' {
    $r = Get-DurableFindingsAnswer -WorkLog "**Durable findings**: see ZM-D-1 and ZM-D-2`n"
    ($r.References.Count -eq 2) -and ($r.References -contains 'ZM-D-1') -and ($r.References -contains 'ZM-D-2')
}

Assert-That 'the label line has NO content at all and nothing follows -- refuses distinctly from "absent"' {
    $r = Get-DurableFindingsAnswer -WorkLog "**Durable findings**:`n"
    $r.Present -and ($null -eq $r.Answer) -and ($r.Reason -match 'no content')
}

Assert-That 'a plain "none." with a trailing period is still accepted (matches the JS reference exactly)' {
    $r = Get-DurableFindingsAnswer -WorkLog "**Durable findings**: none.`n"
    $r.Answer -eq 'none'
}

Assert-That 'matching is case-insensitive on the label text itself' {
    $r = Get-DurableFindingsAnswer -WorkLog "**durable FINDINGS**: none`n"
    $r.Answer -eq 'none'
}

Assert-That 'the label is found ANYWHERE in the work log, not only at the top' {
    $r = Get-DurableFindingsAnswer -WorkLog "## Work Log`n`n**Outcome**: merged`n`n**Durable findings**: none`n`n**Deviations**`n`n- none`n"
    $r.Answer -eq 'none'
}

Assert-That 'a $null work log is Present:$false, not a throw' {
    $r = Get-DurableFindingsAnswer -WorkLog $null
    (-not $r.Present) -and ($null -ne $r.Reason)
}

# ─── G. needs-gpu requires a machine-readable companion ───────────────
# Ported FROM Tools/Vault/cli/tick-checks.mjs's `needsGpuCompanion` and
# its GPU_REASON_LABELS vocabulary.

Write-Host "`n=== G. needs-gpu requires a machine-readable companion ===" -ForegroundColor Cyan

Assert-That '** needs-gpu with ZERO companions refuses, naming the vocabulary' {
    $r = Test-NeedsGpuCompanion -LabelsApplied (Get-LabelList -Value 'needs-gpu')
    (-not $r.Ok) -and ($r.Reason -match 'requires exactly one companion') -and
    ($r.Reason -match 'gpu-reason:render-capture') -and ($r.Reason -match 'gpu-reason:legacy-scene-reauthor')
}

Assert-That '** needs-gpu with TWO companions refuses, naming both' {
    $r = Test-NeedsGpuCompanion -LabelsApplied (Get-LabelList -Value 'needs-gpu,gpu-reason:visual-measure,gpu-reason:render-capture')
    (-not $r.Ok) -and ($r.Reason -match 'not 2') -and
    ($r.Reason -match 'gpu-reason:visual-measure') -and ($r.Reason -match 'gpu-reason:render-capture')
}

Assert-That '** needs-gpu with EXACTLY ONE companion passes' {
    foreach ($reason in (Get-GpuReasonLabels)) {
        $r = Test-NeedsGpuCompanion -LabelsApplied (Get-LabelList -Value "needs-gpu,$reason")
        if (-not $r.Ok) { return $false }
    }
    $true
}

Assert-That '** applying an UNRELATED label is unaffected (no needs-gpu in this write)' {
    $r = Test-NeedsGpuCompanion -LabelsApplied (Get-LabelList -Value 'deferred')
    $r.Ok -and ($null -eq $r.Reason)
}

Assert-That 'not retroactive: this only ever judges the labels of ONE write, never a fetched ticket state' {
    # Test-NeedsGpuCompanion takes exactly the parsed --label value and
    # nothing else -- there is no parameter through which a ticket's
    # already-saved labels could reach it, so a ticket that already
    # carries needs-gpu from before this rule existed cannot be touched
    # by a write that only applies an unrelated label.
    $r = Test-NeedsGpuCompanion -LabelsApplied (Get-LabelList -Value 'needs-human')
    $r.Ok
}

Assert-That 'needs-gpu detection is case-insensitive, matching the JS reference''s toLowerCase()' {
    $r = Test-NeedsGpuCompanion -LabelsApplied (Get-LabelList -Value 'Needs-GPU')
    -not $r.Ok
}

Assert-That 'a gpu-reason label with different casing still counts as a companion' {
    $r = Test-NeedsGpuCompanion -LabelsApplied @('needs-gpu', 'GPU-REASON:VISUAL-MEASURE')
    $r.Ok
}

Assert-That 'Get-GpuReasonLabels returns the fixed four-item vocabulary as an array' {
    $labels = Get-GpuReasonLabels
    ($labels -is [array]) -and ($labels.Count -eq 4) -and
    ($labels -contains 'gpu-reason:render-capture') -and ($labels -contains 'gpu-reason:visual-measure') -and
    ($labels -contains 'gpu-reason:requires-graphics-test') -and ($labels -contains 'gpu-reason:legacy-scene-reauthor')
}

Assert-That 'Test-NeedsGpuCompanion tolerates an empty or $null labels list' {
    ((Test-NeedsGpuCompanion -LabelsApplied @()).Ok) -and ((Test-NeedsGpuCompanion -LabelsApplied $null).Ok)
}

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
