<#
.SYNOPSIS
  Pure checks for four rules `.claude/commands/tick.md` currently enforces
  only by asking the orchestrator to remember them.

.DESCRIPTION
  `.claude/commands/tick.md:1274` states the principle this module exists to
  serve: "a rule enforced at a check that already fails is a rule, and the
  same rule in this paragraph is a hope." Four rules in that protocol are
  hopes today:

    * Test-FileListExcursion  -- did the worker touch a file the dispatch
      prompt never listed? (Excursions are ALLOWED; only going unexplained
      is the problem, and that refusal lives elsewhere, not here.)
    * Test-CommitCompleteness -- did every file step 6 recorded as changed
      actually make it into the commit? (`commit -am` silently drops a
      CREATED file -- tick.md:1496.)
    * Test-BlockedScopeAllowed -- is `blocked-scope` defensible, or did a
      gate actually fail? (tick.md:1457 onward -- the board excludes a
      scope block from `maxConsecutiveBlocked`, so mislabelling one lets a
      thrashing loop past its own breaker.)
    * Test-GateVerdictFresh -- does `tick_gates.json` describe a run that
      finished, against the tree that is HEAD right now? (tick.md:1107
      onward -- "state", "headSha" and "skipped" all have to be read
      correctly by eye today, and this directory has held a stale
      "all gates green" from an unrelated tree.)

  Every function here is read-only: it inspects `.zagent/run/<KEY>/` and the
  git working tree, and answers a question. None of them refuse anything,
  commit anything, or write anything -- wiring a refusal on top of an
  answer is a job for whatever calls in from `zagent.ps1`, not this module.

  Two conventions this file leans on, both learned the hard way on the
  client side of `zagent` (see Test-ZagentClient.ps1):

    * A PowerShell function UNROLLS a one-element collection into a bare
      scalar when it is the thing returned. Every function below returns a
      single [PSCustomObject], so this does not bite the TOP-level return --
      but any private helper that hands back a bare list (Read-PathListFile)
      still needs `,@(...)` for the same reason `Get-WorkingTreeChanges`
      does.
    * `Set-StrictMode -Version Latest` turns a reference to a property that
      is not there into a throw. `tick_gates.json` is written by a script
      that is still gaining fields (`deferred` is newer than `skipped`), so
      every read of it goes through `Get-OptionalProperty`, which is the
      `$obj.PSObject.Properties['name']` idiom the client-side README
      documents for exactly this file format.

  Every check here fails CLOSED: a missing input is reported as its own
  distinguishable state (`ListFound: $false`, `RecordedFound: $false`,
  `Fresh: $false` with a Reason), never silently folded into "nothing to
  report" -- because that shape is indistinguishable from "checked and
  clean", which is the failure mode every one of these four rules exists to
  remove.
#>

Set-StrictMode -Version Latest

Import-Module (Join-Path $PSScriptRoot 'ZagentClient.psm1') -Force

# ─── SHARED HELPERS (private) ────────────────────────────

<#
`filelist.txt` and `changed.txt` are the same format: one repo-relative,
forward-slashed path per line, written by `[System.IO.File]::WriteAllText`
elsewhere in this toolchain (see `Write-Results` in ZagentClient.psm1) with
no BOM and a trailing newline. Read once, the same way, everywhere one of
these is read -- and return $null (not an empty array) when the file does
not exist at all, because "no file" and "an empty file" are different facts
a caller has to be able to tell apart.
#>
function Read-PathListFile {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    $lines = @(Get-Content -LiteralPath $Path -Encoding utf8 -ErrorAction SilentlyContinue)
    # `,@(...)` on return -- see the module header. A one-line file would
    # otherwise unroll to a bare string and every `-notcontains` below would
    # compare against a length instead of a set.
    return , @($lines | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' } | Select-Object -Unique)
}

<#
`tick_gates.json` read once, shared by Test-BlockedScopeAllowed and
Test-GateVerdictFresh -- both have to answer "the file is missing" and "the
file did not parse" before they can answer their own question, and a rule
duplicated in two functions is a rule that drifts the day only one of them
gets fixed.
#>
function Get-TickGatesVerdict {
    param([string]$Repo, [string]$Key)
    $path = Join-Path $Repo ".zagent/run/$Key/tick_gates.json"
    if (-not (Test-Path -LiteralPath $path)) {
        return [PSCustomObject]@{ Found = $false; Path = $path; Verdict = $null; Error = $null }
    }
    try {
        $raw = Get-Content -LiteralPath $path -Raw -Encoding utf8 -ErrorAction Stop
        $parsed = $raw | ConvertFrom-Json -ErrorAction Stop
        return [PSCustomObject]@{ Found = $true; Path = $path; Verdict = $parsed; Error = $null }
    } catch {
        return [PSCustomObject]@{ Found = $true; Path = $path; Verdict = $null; Error = $_.Exception.Message }
    }
}

<#
`$obj.PSObject.Properties['name']` is the documented gotcha
(reference_zagent_sprint_admin_via_sql / the client README): under
Set-StrictMode a plain `$obj.name` throws the moment a field the writer has
not added yet is missing, and `tick_gates.json`'s schema has grown fields
over time (`deferred` came after `skipped`). Centralising the idiom means
it only has to be gotten right once.
#>
function Get-OptionalProperty {
    param($Object, [string]$Name, $Default = $null)
    if ($null -eq $Object) { return $Default }
    $prop = $Object.PSObject.Properties[$Name]
    if ($null -eq $prop) { return $Default }
    return $prop.Value
}

# Exit codes tick_gates.ps1 can leave in `tick_gates.json`, mirrored from its
# own header comment rather than re-derived, so the two do not drift apart:
#   0 green - 2 regen failed - 3 a created file never reached the compiler
#   4 a gate line failed - 5 no gate list - 10 MEASURED, not a failure.
# "Red" for the blocked-scope rule is exactly the three that mean a gate
# ACTUALLY FAILED; 5 ("no gate list") is a scope problem in its own right,
# not evidence against one, and 10 is explicitly carved out by the ticket.
$script:GATE_EXIT_RED = @(2, 3, 4)
$script:GATE_EXIT_MEASURED = 10

# ─── 1. Test-FileListExcursion ───────────────────────────

<#
.SYNOPSIS
  Reports which changed files were not on the worker's file list -- and, as
  distinctly, whether there was a list to check against at all.

.DESCRIPTION
  This does NOT refuse anything. tick.md's rule is "every excursion must be
  explained on the ticket", not "no excursions allowed" -- a worker may
  legitimately need to reach outside its list. The refusal belongs to
  whatever calls this and reads .Excursions; this function's only job is to
  make an excursion impossible to miss, including the case where nobody
  wrote a list to check against, which is a materially different state from
  "checked, and it was clean" and must never collapse into it.

.OUTPUTS
  [PSCustomObject] with:
    Key         [string]
    ListFound   [bool]    -- $false when filelist.txt does not exist.
    ListPath    [string]
    Listed      [string[]] -- paths from filelist.txt (empty when not found).
    Changed     [string[]] -- the current working-tree change set.
    Excursions  [string[]] or $null -- changed-but-not-listed paths; $null
                (never an empty array) when ListFound is $false, so a caller
                cannot mistake "unverified" for "verified clean".
    Missing     [string[]] -- listed-but-untouched paths. Informational, not
                an error: a worker legitimately not needing every listed
                file is normal.
    Reason      [string] or $null -- set when ListFound is $false or Repo
                could not be resolved.
#>
function Test-FileListExcursion {
    param(
        [Parameter(Mandatory)][string]$Key,
        [string]$Repo
    )
    if (-not $Repo) { $Repo = Find-ClientRepo }
    if (-not $Repo) {
        return [PSCustomObject]@{
            Key        = $Key
            ListFound  = $false
            ListPath   = $null
            Listed     = @()
            Changed    = @()
            Excursions = $null
            Missing    = @()
            Reason     = 'no repo: could not resolve a checkout containing zagent.project.json, and -Repo was not given'
        }
    }

    $listPath = Join-Path $Repo ".zagent/run/$Key/filelist.txt"
    $listed = Read-PathListFile -Path $listPath
    # NOT `@(Get-WorkingTreeChanges ...)`. It already returns `,@(...)` so a
    # direct assignment keeps a 1-element result an array -- wrapping the
    # CALL in `@()` on top of that undoes the trick instead of reinforcing
    # it: with N=0 it turns an empty array into a 1-element array holding an
    # empty array, and with N>=2 it collapses the whole result into a
    # single element that IS the real array, i.e. one level of nesting too
    # many. Measured while writing this module's own tests.
    $changed = Get-WorkingTreeChanges -Repo $Repo

    if ($null -eq $listed) {
        # A missing filelist.txt is not "nothing to report" -- it is "the
        # worker's edits were never bounded by a list in the first place",
        # which is the fail-open shape this whole check exists to remove.
        return [PSCustomObject]@{
            Key        = $Key
            ListFound  = $false
            ListPath   = $listPath
            Listed     = @()
            Changed    = $changed
            Excursions = $null
            Missing    = @()
            Reason     = "no filelist.txt at $listPath -- there is nothing to compare the change set against, so excursions are UNVERIFIED, not clean"
        }
    }

    $excursions = @($changed | Where-Object { $listed -notcontains $_ })
    $missing = @($listed | Where-Object { $changed -notcontains $_ })

    return [PSCustomObject]@{
        Key        = $Key
        ListFound  = $true
        ListPath   = $listPath
        Listed     = $listed
        Changed    = $changed
        Excursions = $excursions
        Missing    = $missing
        Reason     = $null
    }
}

# ─── 2. Test-CommitCompleteness ──────────────────────────

<#
.SYNOPSIS
  Checks that every file step 6 recorded as changed actually landed in the
  commit -- catching the `commit -am` failure mode tick.md:1496 warns about
  by name.

.DESCRIPTION
  `-a` stages modifications to files git already tracks and leaves a
  worker-CREATED file untracked and behind. The gates that certified the
  ticket green ran against a working tree where the file existed, so
  nothing else notices; the break "surfaces on somebody else's next clone."
  This compares `changed.txt` (written by step 6.4, from
  `Get-WorkingTreeChanges`) against `git diff-tree` on the commit that was
  supposed to carry it all.

.OUTPUTS
  [PSCustomObject] with:
    Key               [string]
    Ok                [bool] -- $true only when nothing recorded is absent
                       from the commit. Extra files in the commit do not
                       fail this: they are not the dangerous direction.
    RecordedFound     [bool] -- $false when changed.txt does not exist.
    Recorded          [string[]]
    Committed         [string[]]
    MissingFromCommit [string[]] or $null -- recorded but not committed, the
                       dangerous direction; $null (not empty) when there was
                       nothing to compare against.
    ExtraInCommit     [string[]] or $null
    Reason            [string] or $null
#>
function Test-CommitCompleteness {
    param(
        [Parameter(Mandatory)][string]$Key,
        [string]$Repo,
        [string]$Sha = 'HEAD'
    )
    if (-not $Repo) { $Repo = Find-ClientRepo }
    if (-not $Repo) {
        return [PSCustomObject]@{
            Key               = $Key
            Ok                = $false
            RecordedFound     = $false
            Recorded          = @()
            Committed         = @()
            MissingFromCommit = $null
            ExtraInCommit     = $null
            Reason            = 'no repo: could not resolve a checkout containing zagent.project.json, and -Repo was not given'
        }
    }

    $changedPath = Join-Path $Repo ".zagent/run/$Key/changed.txt"
    $recorded = Read-PathListFile -Path $changedPath

    if ($null -eq $recorded -or $recorded.Count -eq 0) {
        # Absent or empty are BOTH "nothing to compare against", and neither
        # is "the commit is complete" -- reporting Ok:$true here would be
        # exactly as silent as the bug this check exists to catch.
        $reason = if ($null -eq $recorded) {
            "no changed.txt at $changedPath -- step 6.4 has not run (or ownership was lost before it wrote), so commit completeness is UNVERIFIED"
        } else {
            "changed.txt at $changedPath is empty -- nothing was recorded as changed, so there is nothing to check the commit against"
        }
        return [PSCustomObject]@{
            Key               = $Key
            Ok                = $false
            RecordedFound     = ($null -ne $recorded)
            Recorded          = @()
            Committed         = @()
            MissingFromCommit = $null
            ExtraInCommit     = $null
            Reason            = $reason
        }
    }

    $committedRaw = & git -C $Repo diff-tree --no-commit-id --name-only -r $Sha 2>$null
    if ($LASTEXITCODE -ne 0) {
        return [PSCustomObject]@{
            Key               = $Key
            Ok                = $false
            RecordedFound     = $true
            Recorded          = $recorded
            Committed         = @()
            MissingFromCommit = $null
            ExtraInCommit     = $null
            Reason            = "'git diff-tree' against '$Sha' failed (exit $LASTEXITCODE) -- is that a valid commit in $Repo ?"
        }
    }
    $committed = @($committedRaw | Where-Object { $_ -ne '' })

    $missing = @($recorded | Where-Object { $committed -notcontains $_ })
    $extra = @($committed | Where-Object { $recorded -notcontains $_ })

    return [PSCustomObject]@{
        Key               = $Key
        Ok                = ($missing.Count -eq 0)
        RecordedFound     = $true
        Recorded          = $recorded
        Committed         = $committed
        MissingFromCommit = $missing
        ExtraInCommit     = $extra
        Reason            = if ($missing.Count -gt 0) {
            "commit '$Sha' is missing $($missing.Count) file(s) changed.txt recorded -- the 'commit -am' failure mode: a CREATED file silently left behind"
        } else { $null }
    }
}

# ─── 3. Test-BlockedScopeAllowed ─────────────────────────

<#
.SYNOPSIS
  Answers whether `blocked-scope` is defensible for this ticket right now,
  per tick.md:1457's rule that the label means "not buildable as filed", not
  "a run failed".

.DESCRIPTION
  Reads the most recent verdict this key's `tick_gates.json` carries. A RED
  verdict -- exit 2 (regen failed), 3 (a created file never reached the
  compiler) or 4 (a gate line failed) -- means a gate actually ran and
  actually failed, which is a FAILURE block, not a scope one, whatever else
  is also true of the ticket. Exit 10 (a pin needs bumping) is explicitly
  carved out by the ticket text as not a failure. A green verdict, an
  absent one, or one still `state: running` (a killed run, carrying no
  completed verdict at all) all permit the label -- there is nothing on
  record to contradict it.

  An unreadable (unparsable) file is the one case that fails the OTHER way:
  it neither confirms nor denies a gate failure, and "cannot tell" must not
  read as permission any more than it reads as refusal, so this returns
  Allowed:$false with a reason naming the parse error.

.OUTPUTS
  [PSCustomObject] with:
    Key      [string]
    Allowed  [bool]
    Reason   [string]
    State    [string] or $null -- the verdict's own `state`, when read.
    ExitCode [int] or $null
#>
function Test-BlockedScopeAllowed {
    param(
        [Parameter(Mandatory)][string]$Key,
        [string]$Repo
    )
    if (-not $Repo) { $Repo = Find-ClientRepo }
    if (-not $Repo) {
        return [PSCustomObject]@{
            Key = $Key; Allowed = $false; State = $null; ExitCode = $null
            Reason = 'no repo: could not resolve a checkout containing zagent.project.json, and -Repo was not given'
        }
    }

    $record = Get-TickGatesVerdict -Repo $Repo -Key $Key
    if (-not $record.Found) {
        # tick.md's rule is "only label what you can defend" -- there is no
        # verdict here to contradict a scope block, so nothing refuses it.
        return [PSCustomObject]@{
            Key = $Key; Allowed = $true; State = $null; ExitCode = $null
            Reason = "no tick_gates.json at $($record.Path) -- no verdict on record to contradict a scope block"
        }
    }
    if ($null -ne $record.Error) {
        # Fail closed the OTHER way here: an unreadable file is not evidence
        # of a clean run either, and a thrashing loop would otherwise learn
        # that a garbled verdict is always a green light.
        return [PSCustomObject]@{
            Key = $Key; Allowed = $false; State = $null; ExitCode = $null
            Reason = "tick_gates.json at $($record.Path) did not parse ($($record.Error)) -- cannot confirm the last run was not a gate failure"
        }
    }

    $verdict = $record.Verdict
    $state = Get-OptionalProperty $verdict 'state'
    $exitCode = Get-OptionalProperty $verdict 'exitCode'

    if ($state -eq 'running') {
        # A killed run leaves `running`, not a verdict -- tick.md:1124 makes
        # exactly this point for the freshness check, and it applies here
        # for the same reason: there is no completed verdict to contradict.
        return [PSCustomObject]@{
            Key = $Key; Allowed = $true; State = $state; ExitCode = $exitCode
            Reason = 'the recorded run never finished (state: running) -- a killed run leaves no verdict, so nothing contradicts a scope block'
        }
    }

    if ($null -ne $exitCode -and $script:GATE_EXIT_RED -contains [int]$exitCode) {
        return [PSCustomObject]@{
            Key = $Key; Allowed = $false; State = $state; ExitCode = $exitCode
            Reason = "the last gate run exited $exitCode -- a gate actually failed, so this is a FAILURE block, not a scope block"
        }
    }

    $reason = if ($exitCode -eq 0) {
        'the last gate run was green'
    } elseif ($exitCode -eq $script:GATE_EXIT_MEASURED) {
        'the last verdict was exit 10 (a pin needs bumping) -- explicitly not a failure'
    } else {
        "the last gate run's exit code ($exitCode) does not indicate a failed gate line"
    }
    return [PSCustomObject]@{ Key = $Key; Allowed = $true; State = $state; ExitCode = $exitCode; Reason = $reason }
}

# ─── 4. Test-GateVerdictFresh ────────────────────────────

<#
.SYNOPSIS
  Answers whether `tick_gates.json` describes a completed run against the
  CURRENT HEAD -- the three fields tick.md:1107 says step 6 has to read
  correctly "by eye": state, headSha, skipped.

.DESCRIPTION
  Not fresh when: the file is absent; it fails to parse; `state` is not
  `complete` (a killed run leaves `running`, which is not the previous
  run's verdict either -- it is no verdict); or `headSha` does not equal
  `git rev-parse HEAD` in this repo right now (tick.md: "the verdict
  describes a different tree -- treat it as absent"). `skipped` is not part
  of the freshness question -- it is returned as data so a caller can apply
  tick.md's other rule, "a red gate voids every gate behind it", once
  freshness itself has been settled.

.OUTPUTS
  [PSCustomObject] with:
    Key      [string]
    Fresh    [bool]
    Reason   [string] or $null -- set whenever Fresh is $false.
    State    [string] or $null
    ExitCode [int] or $null
    HeadSha  [string] or $null
    Skipped  [string[]] -- always an array, empty when the field is absent
             or the verdict itself is unreadable.
#>
function Test-GateVerdictFresh {
    param(
        [Parameter(Mandatory)][string]$Key,
        [string]$Repo
    )
    if (-not $Repo) { $Repo = Find-ClientRepo }
    if (-not $Repo) {
        return [PSCustomObject]@{
            Key = $Key; Fresh = $false; State = $null; ExitCode = $null; HeadSha = $null; Skipped = @()
            Reason = 'no repo: could not resolve a checkout containing zagent.project.json, and -Repo was not given'
        }
    }

    $record = Get-TickGatesVerdict -Repo $Repo -Key $Key
    if (-not $record.Found) {
        return [PSCustomObject]@{
            Key = $Key; Fresh = $false; State = $null; ExitCode = $null; HeadSha = $null; Skipped = @()
            Reason = "no tick_gates.json at $($record.Path) -- an absent verdict is treated the same as a stale one"
        }
    }
    if ($null -ne $record.Error) {
        return [PSCustomObject]@{
            Key = $Key; Fresh = $false; State = $null; ExitCode = $null; HeadSha = $null; Skipped = @()
            Reason = "tick_gates.json at $($record.Path) did not parse ($($record.Error)) -- treat as absent"
        }
    }

    $verdict = $record.Verdict
    $state = Get-OptionalProperty $verdict 'state'
    $exitCode = Get-OptionalProperty $verdict 'exitCode'
    $headSha = Get-OptionalProperty $verdict 'headSha'
    $skipped = @(Get-OptionalProperty $verdict 'skipped' @())

    if ($state -ne 'complete') {
        return [PSCustomObject]@{
            Key = $Key; Fresh = $false; State = $state; ExitCode = $exitCode; HeadSha = $headSha; Skipped = $skipped
            Reason = "state is '$state', not 'complete' -- a killed run leaves the file mid-write, and that is not a verdict to trust"
        }
    }

    $currentHead = $null
    $sha = & git -C $Repo rev-parse HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($sha)) { $currentHead = ([string]$sha).Trim() }

    if ([string]::IsNullOrEmpty($currentHead)) {
        return [PSCustomObject]@{
            Key = $Key; Fresh = $false; State = $state; ExitCode = $exitCode; HeadSha = $headSha; Skipped = $skipped
            Reason = "could not resolve current HEAD in $Repo -- cannot confirm the verdict describes this tree"
        }
    }

    if ([string]::IsNullOrEmpty($headSha) -or $headSha -ne $currentHead) {
        return [PSCustomObject]@{
            Key = $Key; Fresh = $false; State = $state; ExitCode = $exitCode; HeadSha = $headSha; Skipped = $skipped
            Reason = "verdict headSha ($headSha) does not match current HEAD ($currentHead) -- the verdict describes a different tree; treat as absent"
        }
    }

    return [PSCustomObject]@{
        Key = $Key; Fresh = $true; State = $state; ExitCode = $exitCode; HeadSha = $headSha; Skipped = $skipped
        Reason = $null
    }
}

Export-ModuleMember -Function Test-FileListExcursion, Test-CommitCompleteness, Test-BlockedScopeAllowed, Test-GateVerdictFresh
