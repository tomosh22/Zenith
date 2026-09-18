#Requires -Version 7.0
<#
.SYNOPSIS
  zagent — the agent board's command line, as a thin remote client.

.DESCRIPTION
  The board is a web application that may run on an entirely different
  machine. This script knows three things and nothing else:

    * ZAGENT_URL   — where the board is
    * ZAGENT_TOKEN — a revocable per-machine bearer token
    * <repo>/zagent.project.json — this repo's own gates and conventions

  In particular it does NOT know where the board's source tree is, and
  there is deliberately no way to tell it. Everything that needs a
  database, a Yjs document or the Notion schema happens server-side.

  PowerShell rather than Node so this repo needs no JavaScript toolchain:
  pwsh is already a hard dependency of every gate and every zenith.ps1
  command, so the client adds no new requirement to a build machine.

.NOTES
  Exit codes ARE the control flow, and /tick branches on them:
    0 ok · 3 nothing to claim · 4 contract invalid · 5 ownership lost ·
    6 circuit breaker open · 1 real error · 7 cannot reach the board

  7 is this script's own, and it is distinct on purpose: "the board said
  no" and "I never reached the board" demand opposite responses from an
  unattended loop, and collapsing them into 1 is how a network blip gets
  written into a ticket as a contract failure.
#>

[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments = @()
)

Set-StrictMode -Version Latest

# MUST be initialised before any function that reads it. Set-StrictMode turns
# a reference to an unset variable into a THROW, and `--help` exits through
# Exit-Zagent long before the repo is resolved -- so leaving this to line ~175
# would crash the one path that never touches the network. Same
# module-boundary class this file's header already warns about.
$script:RepoForExit = $null
$ErrorActionPreference = 'Stop'

# The board speaks UTF-8 and this client's output is READ BY THINGS: a tick
# redirects it to a file, tick_gates.ps1 appends it to gates.log, and a work
# log quotes it. Windows PowerShell hands a redirected stream the OEM code
# page, so `·` (U+00B7) -- which the queue and doctor summaries use as a
# separator -- went out as the single byte 0xFA, which is not valid UTF-8 at
# all. The em dash was already being folded to `-` elsewhere, so the fold
# table looked complete while one character quietly corrupted every capture.
# Setting the encoding fixes the whole class rather than one more character.
try {
    [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
    $OutputEncoding = [System.Text.UTF8Encoding]::new($false)
} catch {
    # A redirected or absent console can refuse this. Losing a separator is
    # cosmetic; refusing to run the command is not.
}

$script:EXIT_ERROR = 1
$script:EXIT_UNREACHABLE = 7
$script:PROJECT_FILE = 'zagent.project.json'

# The protocol version THIS client speaks. Sent with every request as
# `protocol`; the board refuses a mismatch before dispatch runs, so an
# incompatible pair fails cleanly instead of a command being parsed
# under a stale argv grammar. See PROTOCOL_VERSION in the board's
# packages/agent/src/dispatch.ts — the two constants are pinned to each
# other by contract.json, which both sides' tests assert against.
$script:PROTOCOL_VERSION = 1

# Cutover is explicit and repository-owned. An invalid Vault configuration must
# never fall back to the legacy writer, which would create two authorities.
$vaultBackendPath = Join-Path $PSScriptRoot 'backend.json'
if (Test-Path -LiteralPath $vaultBackendPath) {
    try {
        $vaultBackend = Get-Content -LiteralPath $vaultBackendPath -Raw | ConvertFrom-Json
        if ($vaultBackend.backend -eq 'vault') {
            if (-not $vaultBackend.vaultRoot) { throw 'Vault backend requires vaultRoot.' }
            $vaultRootPath = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $vaultBackend.vaultRoot))
            $vaultClientPath = Join-Path $PSScriptRoot '../Vault/cli/client.mjs'
            if (-not (Test-Path -LiteralPath $vaultClientPath)) { throw 'Vault client is missing.' }
            & node $vaultClientPath --vault $vaultRootPath @Arguments
            exit $LASTEXITCODE
        }
        if ($vaultBackend.backend -ne 'legacy') { throw 'Unknown zagent backend; refusing writer fallback.' }
    } catch {
        [Console]::Error.WriteLine($_.Exception.Message)
        exit 1
    }
}

# ★★ ORDER IS LOAD-BEARING. TickChecks.psm1 imports ZagentClient.psm1
# INTERNALLY, as a nested module, while it loads. Importing ZagentClient
# first and TickChecks second leaves ZagentClient's exports invisible: the
# nested `-Force` reload replaces the earlier top-level module entry
# entirely, and TickChecks' own `Export-ModuleMember` never re-exports
# ZagentClient's functions, so every one of them -- `Test-HelpFlag`,
# `Get-ClientProject`, `Get-FlagValue`, all of it -- comes back "not
# recognized" the moment this script actually runs. `--help` and `doctor`
# both went straight from a green parse check and a green unit suite to
# a hard crash on the first real invocation, which is the exact shape
# this repo's own CLAUDE.md names by incident: a parse check plus green
# tests is not proof the client runs. Importing TickChecks FIRST and
# ZagentClient SECOND leaves both loaded and both exporting, because the
# second, top-level import re-establishes ZagentClient globally after
# the nested one. Test-TickWiring.ps1 pins this ordering.
#
# The four checks TickChecks.psm1 exports turn tick.md's "remember
# to..." paragraphs into something that actually fails -- see its own
# header for which rule each one closes. Pure and read-only; the
# refusals they feed live here, at the calls that already write
# status/labels/guard verdicts.
Import-Module (Join-Path $PSScriptRoot 'TickChecks.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'ZagentClient.psm1') -Force

# ─── ENVIRONMENT ─────────────────────────────────────
#
# Where the board is and who we are. These read `$env:` and call `exit`,
# so they stay in the SCRIPT rather than the helper module — the module
# is the part a test can import without a board or a shell.

function Get-BoardUrl {
    $url = $env:ZAGENT_URL
    if (-not $url) {
        Write-StdErr @'
ZAGENT_URL is not set — this client does not know where the board is.

  $env:ZAGENT_URL   = 'https://board.example.com'
  $env:ZAGENT_TOKEN = 'zag_…'   # from `zagent auth mint` ON THE BOARD

Set them permanently with:
  [Environment]::SetEnvironmentVariable('ZAGENT_URL', '…', 'User')
'@
        Exit-Zagent $script:EXIT_ERROR
    }
    return $url.TrimEnd('/')
}

function Get-BoardToken {
    $token = $env:ZAGENT_TOKEN
    if (-not $token) {
        Write-StdErr "ZAGENT_TOKEN is not set. Mint one on the board with ``zagent auth mint --name <this-machine>``."
        Exit-Zagent $script:EXIT_ERROR
    }
    return $token
}

<#
Exit, having RECORDED the code where a pipe cannot corrupt it.

`.zagent/last.exit` is the twin of `.zagent/last.json`, and it exists for
the same reason: a caller must be able to read this without the value
passing through a shell. See Write-LastExit in ZagentClient.psm1 for the
two measured breaks that motivated it.

Every `exit` in this file goes through here. The repo may not be resolved
yet on the earliest paths (ZAGENT_URL missing, `--help`), and
Write-LastExit no-ops on an empty repo rather than guessing a location.
#>
function Exit-Zagent {
    param([int]$Code)
    Write-LastExit -Code $Code -Repo $script:RepoForExit
    exit $Code
}

# ─── TRANSPORT ───────────────────────────────────────

function Invoke-Board {
    param([hashtable]$Body, [int]$TimeoutSec = 120)
    $url = Get-BoardUrl
    $token = Get-BoardToken
    # Depth matters: ConvertTo-Json defaults to 2 and would silently
    # flatten `categories.Zenithmon.docs.order` into a type name.
    $json = $Body | ConvertTo-Json -Depth 100 -Compress
    try {
        return Invoke-RestMethod -Method Post -Uri "$url/api/agent" `
            -Headers @{ Authorization = "Bearer $token" } `
            -ContentType 'application/json; charset=utf-8' `
            -TimeoutSec $TimeoutSec `
            -Body ([System.Text.Encoding]::UTF8.GetBytes($json))
    } catch {
        $status = 0
        if ($_.Exception.PSObject.Properties['Response'] -and $_.Exception.Response) {
            $status = [int]$_.Exception.Response.StatusCode
        }
        # The board explains a 4xx in the body. Swallowing it and printing
        # only "400 Bad Request" is how an unattended loop stalls with
        # nothing to act on — surface the reason it gave.
        $detail = $_.ErrorDetails.Message
        $detailObject = $null
        if ($detail) {
            try { $detailObject = $detail | ConvertFrom-Json; $detail = $detailObject.error } catch { }
        }
        # ★ A PROTOCOL MISMATCH IS NOT AN ORDINARY 400. The board refused
        # the REQUEST rather than the command, so no board verdict exists
        # to act on: nothing was claimed, nothing moved, and the fix is
        # to update whichever side is stale. Naming both versions is the
        # whole message — "400 Bad Request" would send an operator into
        # the command's own arguments looking for a defect that is not
        # there.
        if ($status -eq 400 -and $detailObject -and
            $detailObject.PSObject.Properties['protocolVersion']) {
            $requested = 'an unversioned request'
            if ($Body.PSObject.Properties['protocol']) {
                $requested = "v$($Body.protocol)"
            }
            Write-StdErr "Protocol mismatch: the board speaks v$($detailObject.protocolVersion), this client sent $requested."
            Write-StdErr '        Nothing was executed and nothing changed. Update the stale side, then retry.'
            Exit-Zagent $script:EXIT_ERROR
        }
        if ($status -eq 401) {
            Write-StdErr "The board rejected this token (401). Mint a new one with ``zagent auth mint``, or check ZAGENT_TOKEN."
            Exit-Zagent $script:EXIT_ERROR
        }
        if ($status -ge 400 -and $status -lt 500) {
            Write-StdErr "The board refused the request ($status): $detail"
            Exit-Zagent $script:EXIT_ERROR
        }
        Write-StdErr "Could not reach the board at $url — $($_.Exception.Message)"
        if ($detail) { Write-StdErr $detail }
        Exit-Zagent $script:EXIT_UNREACHABLE
    }
}

# ─── MAIN ────────────────────────────────────────────

$argv = @($Arguments)

# Answered HERE, before Find-ClientRepo and before one byte goes out.
# `zagent next --help` claimed a ticket for real; flags.ts fixes that on
# the board, but a guarantee that travels over the network holds only
# while the network does. See Test-HelpFlag for why the positional
# `help` deliberately still goes out.
if (Test-HelpFlag -Argv $argv) {
    Write-Host (Format-HelpStub -Argv $argv)
    Exit-Zagent 0
}

if ($argv.Count -eq 0) { $argv = @('help') }

$repoPath = Find-ClientRepo
# Exit-Zagent records the exit code under this repo. Set as soon as the
# checkout is known and left $null before that, so the earliest failures
# (no ZAGENT_URL, `--help`) write nothing rather than guessing a location.
$script:RepoForExit = $repoPath
$client = Get-ClientProject -Repo $repoPath

# ★★ `abandon` IS CLIENT-COMPOSED, NOT A BOARD COMMAND -- IT DOES NOT
# EXIST IN contract.json AND MUST NOT BE ADDED THERE.
#
# tick.md step 9's ownership-lost path used to be three negatives an
# agent has to hold under pressure: "use comment, but do NOT change
# status, and do NOT clear the assignee -- this is the one release
# where the assignee must stay put." A rule that only says what NOT to
# do is exactly the shape this repo keeps finding does not survive an
# unattended loop, so it is removed structurally: `abandon` has no
# `--status`, `--assignee` or `--label` flag at all, so there is no way
# to spell the mistake, and it dispatches the board's own `comment`
# rather than inventing a new endpoint.
if ($argv[0] -eq 'abandon') {
    if ($argv.Count -lt 2) {
        Write-StdErr 'usage: zagent abandon <KEY> --comment <file> [--json]'
        Exit-Zagent $script:EXIT_ERROR
    }
    $forbiddenFlags = Get-ForbiddenAbandonFlags -Argv $argv
    if ($forbiddenFlags.Count -gt 0) {
        foreach ($forbidden in $forbiddenFlags) {
            Write-StdErr "zagent abandon: --$forbidden is not accepted here."
        }
        Write-StdErr '        Ownership was lost, not resolved: the assignee stays put and the status stays'
        Write-StdErr '        untouched (tick.md step 9). Post the work log with `zagent abandon <KEY> --comment <file>`;'
        Write-StdErr '        use `zagent update` yourself, deliberately, if you actually mean to change any of them.'
        Exit-Zagent $script:EXIT_ERROR
    }
    # Rewrite in place and fall through the rest of this script exactly as
    # though `comment` had been typed -- `--comment <file>` is already a
    # universal path-valued flag (Get-FileFlags), so nothing downstream
    # needs to know `abandon` was ever the verb typed.
    $abandonKey = $argv[1]
    $argv = @('comment', $abandonKey) + @($argv | Select-Object -Skip 2)
}

$asJson = Test-Flag -Argv $argv -Name 'json'

$body = @{ argv = $argv }
if ($client) { $body.client = $client }
# Every request names the protocol version this client speaks. The board
# refuses a mismatch before dispatch, and echoes its own version back —
# see the warning below for what happens when it does not.
$body.protocol = $script:PROTOCOL_VERSION

$files = Get-FileContents -Argv $argv
$fileMap = [ordered]@{}
if ($files) {
    foreach ($property in $files.PSObject.Properties) { $fileMap[$property.Name] = $property.Value }
}

# `guard` and `gates` with no --file/--text compute their OWN input from
# the working tree. The tick used to hand-assemble that git command in
# prose, and BOTH spellings it reached for were wrong in ways nothing
# caught: a commit range that is empty at guard time (so every ticket
# Blocked), and `diff --name-only`, which cannot see a file the worker
# CREATED (so a new .claude/** path evaded the check entirely). See
# Get-WorkingTreeChanges. A guard whose input is assembled by hand is a
# guard with an untested step in front of it.
if ((Test-NeedsChangedSet -Argv $argv) -and -not $fileMap.Contains('file')) {
    $changed = Get-WorkingTreeChanges -Repo $repoPath
    # An empty set is a failed attempt for `guard` and a legitimate
    # answer for `gates`, which the tick may run before the worker has
    # written anything.
    if ($argv[0] -eq 'guard' -and $changed.Count -eq 0) {
        Write-StdErr "guard: nothing has changed in $repoPath — the worker wrote no files."
        Exit-Zagent $script:EXIT_ERROR
    }
    $fileMap['file'] = (($changed -join "`n") + "`n")
}

# `zagent guard <KEY>` ships the gate selection `zagent gates <KEY>`
# recorded, so the BOARD can re-derive it and refuse a stale one. A
# MISSING file is sent as nothing on purpose: the board answers with the
# message naming the step that was skipped, which keeps one explanation
# of the rule rather than two that can drift.
$gateKey = Get-GuardTicketKey -Argv $argv
if ($gateKey) {
    $recorded = Get-RecordedGateSelection -Repo $repoPath -Key $gateKey
    if ($recorded) { $fileMap['gates'] = $recorded }
}

if ($fileMap.Count -gt 0) { $body.files = $fileMap }

# ★★ `blocked-scope` IS EXCLUDED FROM `maxConsecutiveBlocked` (tick.md:1457
# onward), SO MISLABELLING A GATE FAILURE AS A SCOPE PROBLEM LETS A
# THRASHING LOOP PAST ITS OWN CIRCUIT BREAKER.
#
# "Only label what you can defend" was a paragraph an agent had to
# remember under pressure -- exactly the shape this repo keeps finding
# does not hold. Test-BlockedScopeAllowed reads the SAME tick_gates.json
# verdict a human would (a RED exit code -- 2, 3 or 4 -- means a gate
# actually failed, which is a failure block whatever else is true of the
# ticket) and this refuses before the label ever reaches the board.
#
# `--label` may carry a comma-separated list (`--label a,b`); only
# `blocked-scope`'s presence in it triggers the check, and only THAT
# label is what tick.md:1457 is about.
if ($argv[0] -eq 'update' -and $argv.Count -ge 2) {
    $labelValue = Get-FlagValue -Argv $argv -Name 'label'
    if ($labelValue) {
        $labels = Get-LabelList -Value $labelValue
        if ($labels -contains 'blocked-scope') {
            $scopeCheck = Test-BlockedScopeAllowed -Key $argv[1] -Repo $repoPath
            if (-not $scopeCheck.Allowed) {
                Write-StdErr "zagent: refusing --label blocked-scope on $($argv[1]) -- $($scopeCheck.Reason)"
                Write-StdErr '        A scope block is for work that was not buildable AS FILED -- a missing file list,'
                Write-StdErr '        an unmade ruling -- not for a run that failed. If a gate actually failed, this is a'
                Write-StdErr '        FAILURE block: leave it unlabelled (or label it accurately) and let the ticket re-run.'
                Exit-Zagent $script:EXIT_ERROR
            }
        }

        # ★★ `needs-gpu` NOW REQUIRES A MACHINE-READABLE REASON, NOT FREE
        # PROSE AN AGENT HAS TO READ AND INFER.
        #
        # `needs-gpu` means "build Vulkan_*_True and run windowed instead
        # of the headless Null_ config". Since ZEN-6 removed the guard
        # that made "it re-authors a committed scene" true, that used to
        # be the COMMONEST reason a ticket carried the label -- and it
        # stopped being a reason at all, leaving stale labels holding
        # tickets back for nothing. A fixed vocabulary replaces the
        # prose: applying `needs-gpu` now requires exactly one
        # `gpu-reason:*` companion in the SAME write. Deliberately NOT
        # retroactive -- $labels is only what THIS write is applying, per
        # Get-LabelList, never the ticket's already-saved label list, so
        # a ticket that already carries `needs-gpu` from before this rule
        # existed is untouched until somebody writes to its labels again.
        $gpuCheck = Test-NeedsGpuCompanion -LabelsApplied $labels
        if (-not $gpuCheck.Ok) {
            Write-StdErr "zagent: refusing --label $labelValue on $($argv[1]) -- $($gpuCheck.Reason)"
            Exit-Zagent $script:EXIT_ERROR
        }
    }
}

# ★★ DONE IS THE ONE STATUS THE DoD GETS A VOTE ON, AND (with the two
# checks below) THE ONE CALL WHERE THE FILE LIST AND THE COMMIT ALSO DO.
#
# `/tick` step 7 branches on GATE COLOUR and nothing else, so a worker that
# returns a correct partial with a verified impossibility -- gates green on
# what landed, half the DoD unmet -- reads as "Green" and gets recorded Done.
# The work log filed alongside it RENDERS the unticked boxes and nothing has
# ever read them. Checking here rather than in the protocol is the point: this
# is the call that writes the status, so the rule is enforced at a check that
# already fails instead of asked for in a paragraph.
#
# Blocked and In Review pass untouched -- an unmet DoD is the normal state for
# both, and a Blocked log saying how far it got is exactly what a human picks up.
if ($argv[0] -eq 'finish' -and $fileMap.Contains('worklog')) {
    $finishKey = $argv[1]
    $claimedStatus = Get-FlagValue -Argv $argv -Name 'status'
    $statusNorm = if ($claimedStatus) { $claimedStatus.Trim().ToLowerInvariant() } else { $null }

    # ★★ AN EXCURSION IS ALLOWED. AN UNEXPLAINED ONE IS NOT.
    #
    # A worker's file list bounds what it may touch, but the work sometimes
    # genuinely needs to reach outside it (tick.md:852-854 -- ZM-69's worker
    # had to READ a file its own list forbade editing, to find what was
    # missing). Nothing has ever checked either half: not that an excursion
    # happened, and not that it was explained. Test-FileListExcursion
    # answers the first; this answers the second, at the call that writes
    # the permanent record.
    $excursion = Test-FileListExcursion -Key $finishKey -Repo $repoPath
    if (-not $excursion.ListFound) {
        # A missing filelist.txt is NOT "no excursions" -- it is
        # "unverified", and folding the two together is the exact fail-open
        # shape this check exists to remove. Warn rather than refuse: older
        # runs and a hand-run tick legitimately have no list at all.
        Write-StdErr "zagent: finish $finishKey -- $($excursion.Reason)."
        Write-StdErr '        Proceeding, but nobody can tell from this run whether it stayed inside a file list.'
    } elseif (@($excursion.Excursions).Count -gt 0) {
        # Accept the path appearing ANYWHERE in the comment or the work
        # log -- an `## Excursions` section is the obvious place to put
        # it, but the goal is a written justification on the permanent
        # record, not a format a worker has to get exactly right.
        $explanationText = $fileMap['worklog']
        if ($fileMap.Contains('comment')) { $explanationText += "`n" + $fileMap['comment'] }
        $unexplained = Get-UnexplainedExcursions -Excursions $excursion.Excursions -ExplanationText $explanationText
        if ($unexplained.Count -gt 0) {
            Write-StdErr "zagent: refusing to finish $finishKey -- it touched $($unexplained.Count) file(s) outside its recorded file list, and none of them is named in the comment or work log:"
            foreach ($path in $unexplained) { Write-StdErr "  $path" }
            Write-StdErr ''
            Write-StdErr 'Excursions are PERMITTED -- the work may genuinely need to reach outside the list --'
            Write-StdErr 'but every one must be explained on the ticket record. Add the explanation to the'
            Write-StdErr 'comment or work log and finish again. Do not revert the work to dodge this.'
            Exit-Zagent $script:EXIT_ERROR
        }
    }

    # ★★ A FINDING THAT OUTLIVES THE TICKET IS A DECISION, NOT A WORK-LOG
    # LINE (tick.md step 8, "A FINDING THAT OUTLIVES THE TICKET IS A
    # DECISION"). Nobody can verify the JUDGEMENT -- "was there really
    # nothing durable here" is not computable -- so, exactly like the
    # work log's own Coverage row, silence is made illegal instead of the
    # answer being graded.
    #
    # This rule used to live ONLY in this client: `.claude/commands/
    # tick.md` documented it as enforced, and it had quietly stopped
    # running in production the moment `backend.json` cut over to the
    # Vault backend, because nothing there grew an equivalent. A
    # protection that exists on one backend and not the other disappears
    # the instant somebody flips that file, and nothing said so. Now both
    # sides carry it -- Get-DurableFindingsAnswer is a faithful port of
    # Tools/Vault/cli/tick-checks.mjs's `durableFindingsAnswer`, matched
    # shape for shape, so a backend flip cannot change what passes.
    #
    # Checked on EVERY terminal status. Done, Blocked and "In Review" can
    # all discover something durable about the codebase; none of them
    # gets a pass on saying so.
    $durable = Get-DurableFindingsAnswer -WorkLog $fileMap['worklog']
    if (-not $durable.Present -or $null -eq $durable.Answer) {
        Write-StdErr "zagent: refusing to finish $finishKey -- $($durable.Reason)"
        Exit-Zagent $script:EXIT_ERROR
    }

    # ★★ `commit -am` SILENTLY DROPS A CREATED FILE (tick.md:1496). The
    # gates that certified this ticket green ran against a working tree
    # where the file existed; the break surfaces on somebody else's next
    # clone, with the tree already clean and the ticket already Done.
    #
    # Checked only at Done. A Blocked finish may legitimately carry no
    # commit that covers changed.txt at all: `direct` mode commits before
    # parking on a RED gate (tick.md:1544 "COMMIT BEFORE YOU PARK"), but a
    # disputed-value ticket is explicitly REVERTED to a clean tree instead
    # and the diff preserved only in run scratch (tick.md:1480-1487) -- so
    # checking Blocked here would refuse a ticket that did exactly what
    # the protocol asked of it. Done is the one status where "committed
    # everything recorded as changed" is unconditionally true if the
    # protocol was followed, so it is the one status this can check
    # without a false refusal.
    if ($statusNorm -eq 'done') {
        $commitCheck = Test-CommitCompleteness -Key $finishKey -Repo $repoPath
        if ($null -eq $commitCheck.MissingFromCommit) {
            # `MissingFromCommit` is $null (never an empty array) when
            # `changed.txt` is missing or empty -- TickChecks.psm1's own
            # contract for "nothing to compare against". `@($null)` is NOT
            # an empty array (it wraps $null into a ONE-ELEMENT array), so
            # this state has to be checked for explicitly before this ever
            # calls `.Count` on it -- conflating it with "zero missing
            # files" both crashes on the next line (StrictMode: `.Count`
            # on $null throws) and would have refused a ticket that has
            # nothing wrong with it. `changed.txt` is written by step 6.4
            # (`guard`), which always runs before a Done finish in the
            # normal protocol, so this should not happen -- warn rather
            # than silently pass or refuse a ticket this check cannot
            # actually verify.
            Write-StdErr "zagent: finish $finishKey as Done -- $($commitCheck.Reason)."
            Write-StdErr '        Proceeding, but commit completeness could not be verified for this run.'
        } elseif (@($commitCheck.MissingFromCommit).Count -gt 0) {
            Write-StdErr "zagent: refusing to finish $finishKey as Done -- HEAD is missing $(@($commitCheck.MissingFromCommit).Count) file(s) that changed.txt recorded as changed:"
            foreach ($path in $commitCheck.MissingFromCommit) { Write-StdErr "  $path" }
            Write-StdErr ''
            Write-StdErr "This is the 'commit -am' failure mode: a file the worker CREATED is untracked, so"
            Write-StdErr "'-a' never stages it and the commit lands without it -- compiling from a clean"
            Write-StdErr 'checkout will fail even though every gate here ran green against a tree that still'
            Write-StdErr 'had the file on disk. Fix: git add -A, then a new commit (or amend the one that'
            Write-StdErr 'missed it), then finish again.'
            Exit-Zagent $script:EXIT_ERROR
        }
    }

    if ($claimedStatus -and $statusNorm -eq 'done') {
        $unmet = Get-UnmetDoneCriteria -WorkLog $fileMap['worklog']
        if (@($unmet).Count -gt 0) {
            Write-StdErr "zagent: refusing to finish $finishKey as Done -- its work log leaves $(@($unmet).Count) Definition-of-Done item(s) unticked:"
            foreach ($item in @($unmet)) { Write-StdErr "  [ ] $item" }
            Write-StdErr ''
            Write-StdErr 'A green gate says the tree compiles and the pinned count matches. It does not'
            Write-StdErr 'say the ticket was finished. Either tick the box because the work IS done, or'
            Write-StdErr 'finish as Blocked / "In Review" -- both accept an unmet DoD, and a Blocked log'
            Write-StdErr 'saying how far it got is the thing a human picks up.'
            Exit-Zagent $script:EXIT_ERROR
        }
    }
}

# Which commands ship the docs tree lives in the MODULE, beside its twin
# in the board's `needsDocsTree` — see `Test-NeedsDocsTree`. Inline here,
# nothing could reach it: this file runs a command the moment it is
# invoked, so a test cannot import it, and the board machine uses the
# Node client and never executes this script at all.
if (Test-NeedsDocsTree -Argv $argv) {
    $tree = Get-DocsTree -Client $client
    if ($tree) { $body.docsTree = $tree }
}

$amend = Get-AmendContents -Client $client -Argv $argv
if ($amend) { $body.amend = $amend }

$result = Invoke-Board -Body $body -TimeoutSec (Get-RequestTimeout -Argv $argv)

# ★★ STEP 6.4's FRESHNESS QUESTION FOLDS INTO THIS CALL, BECAUSE `guard`
# IS ALREADY THE MANDATORY MERGE-BLOCKING STEP (tick.md:1107-1136,
# tick.md:1265-1273 -- "a rule enforced at a check that already fails is
# a rule, and the same rule in this paragraph is a hope").
#
# The board's guard answers "does the recorded gate selection still
# cover this diff" -- it has never seen `tick_gates.json` and cannot
# answer "did the run that produced that selection actually finish,
# against the tree HEAD is on RIGHT NOW". Reading state/headSha/skipped
# correctly "by eye" is exactly the shape this repo keeps finding does
# not survive an unattended loop, and this directory has held a stale
# "all gates green" from an unrelated tree before (tick.md:1128-1129).
#
# Only turns a WOULD-BE PASS into a failure -- never the reverse. A
# board-side guard refusal already carries its own exit code and
# message, and a stale local verdict has nothing useful to add to a
# refusal that already fired, so this is gated on the board itself
# having said 0.
if ($argv[0] -eq 'guard' -and $gateKey) {
    $verdict = Test-GateVerdictFresh -Key $gateKey -Repo $repoPath
    if (Test-GuardNeedsFreshnessOverride -BoardExitCode $result.exitCode -Fresh $verdict.Fresh) {
        Write-StdErr "zagent: guard $gateKey -- the board's guard passed, but its gate verdict is not fresh: $($verdict.Reason)"
        Write-StdErr '        Treating this as a guard failure -- a stale or missing tick_gates.json cannot certify the tree HEAD is on right now.'
        $result | Add-Member -NotePropertyName exitCode -NotePropertyValue $script:EXIT_ERROR -Force
        $result | Add-Member -NotePropertyName error -NotePropertyValue $true -Force
        $result | Add-Member -NotePropertyName text -NotePropertyValue "guard $gateKey refused locally: gate verdict not fresh -- $($verdict.Reason)" -Force
    }
}

# ★ THE HANDSHAKE'S OTHER HALF. The board echoes `protocolVersion` on
# every answered request. A response WITHOUT one means the board that
# answered predates the handshake — it accepted our version blindly and
# may be parsing argv under an older grammar. That is not a refusal and
# this is not a failure: an older board and this client both speak v1
# semantics today, so the run proceeds and says out loud what it could
# not verify. The moment the two grammars actually diverge, this is the
# line that tells the reader where to look.
if (-not ($result.PSObject.Properties['protocolVersion'])) {
    Write-StdErr 'note: the board answered without a protocolVersion — it predates the protocol handshake.'
    Write-StdErr '      Proceeding; upgrade the board before any release that changes the request contract.'
}

# ★★ A REFUSED CLAIM USED TO LEAVE THE TICKET CLAIMED. THE BOARD IS FIXED;
#    THIS ROLLBACK IS NOW DEFENCE-IN-DEPTH FOR AN OLDER BOARD.
#
# The board's claim transaction used to write FIRST and validate SECOND, so
# `zagent claim <needs-human key>` returned exit 4 -- "no machine can produce
# this deliverable" -- having already written:
#
#     status:   To Do -> In Progress
#     assignee: none  -> the agent
#
# Three consequences, the third fatal. The ticket is left ASSIGNED, so the claim
# query can never see it again; it is left OUT of To Do; and it HOLDS THE I5
# one-ticket-per-repo lock, so the very next `zagent next` returns exit 5, repo
# busy. One `/tick` on a ticket the system says can never be claimed stops every
# project this checkout serves -- and `/tick`'s own step 2 ("targeted exit 4:
# report the error and change nothing") guarantees it stays stopped, because that
# instruction was written assuming a refusal had written nothing.
#
# The BOARD now enforces the order this rollback existed for: the contract is
# evaluated BEFORE the claim writes anything, inside the claim's own
# transaction, so a refusal rolls everything back and the refusal payload
# carries NO previousStatus. This client-side rollback keys on exactly that
# field, so it is INERT against a current board and stays here only for a
# board that predates the fix (or a transport hop to one). Keeping it costs
# nothing and covers the gap between a client update and a board update.
if ($result.exitCode -eq 4 -and ($argv[0] -eq 'claim' -or $argv[0] -eq 'next')) {
    $rolled = Restore-RefusedClaim -Payload $result.payload -Client $client `
        -Invoke { param($a) Invoke-Board -Body @{ argv = $a; client = $client } -TimeoutSec 60 }
    foreach ($note in @($rolled)) { Write-StdErr $note }
}

# `doctor` is the one command whose answer is assembled from both sides.
if ($argv[0] -eq 'doctor') {
    $clientChecks = Get-ClientChecks -Client $client
    $boardChecks = @()
    if ($result.payload.PSObject.Properties['checks']) { $boardChecks = @($result.payload.checks) }
    $all = @($boardChecks) + @($clientChecks)
    $ok = -not ($all | Where-Object { -not $_.ok })
    if ($asJson) {
        [PSCustomObject]@{ ok = $ok; checks = $all } | ConvertTo-Json -Depth 100
    } else {
        foreach ($check in $all) {
            $mark = if ($check.ok) { 'ok  ' } else { 'FAIL' }
            Write-Output "$mark  $($check.name): $($check.detail)"
        }
    }
    Exit-Zagent $(if ($ok) { 0 } else { 1 })
}

$repoForWrites = if ($repoPath) { $repoPath } else { (Get-Location).Path }

# ★★ A CLAIM IS THE BOUNDARY OF AN ATTEMPT, SO THE PREVIOUS ATTEMPT'S GATE
# ARTIFACTS MUST NOT SURVIVE IT.
#
# A claim rewrites ticket.json, body.md and drift.txt and used to leave
# everything else in place. Measured on 2026-08-25: claiming ZM-28 found a
# tick_gates.json already sitting there reading
# `state=complete, exitCode=0, verdict="all gates green"` -- from a DIFFERENT
# commit -- beside a gates.json whose recorded changed-set was an unrelated
# one-file diff. The only thing standing between that and a tick which skips
# its gates was a paragraph of prose telling the reader to compare headSha
# against HEAD by hand, which is precisely the shape this repo keeps naming
# as the defect: a rule in prose in front of a check.
#
# Archived rather than deleted, because I6 says a crashed firing must be
# reconstructible from durable state and the previous attempt's logs are
# part of the forensics. Only ONE generation is kept -- the point is that a
# STALE verdict cannot be read as a current one, not that history is free.
if ($repoPath -and $result.payload -and $result.payload.PSObject.Properties['key'] -and
    ($argv[0] -eq 'claim' -or $argv[0] -eq 'next')) {
    try {
        $runDir = Join-Path (Get-ScratchRoot $repoForWrites) $result.payload.key
        if (Test-Path -LiteralPath $runDir) {
            $prev = "$runDir.prev"
            if (Test-Path -LiteralPath $prev) {
                Remove-Item -LiteralPath $prev -Recurse -Force -ErrorAction SilentlyContinue
            }
            Move-Item -LiteralPath $runDir -Destination $prev -Force -ErrorAction Stop
            Write-StdErr "archived the previous attempt's scratch to $($result.payload.key).prev/ -- a stale gate verdict cannot be read as this run's."
        }
    } catch {
        # Never let housekeeping cost a claim. Say so, and carry on: the
        # tick still has headSha to fall back on.
        Write-StdErr "could not archive the previous run scratch ($($_.Exception.Message)) -- check .zagent/run/$($result.payload.key)/tick_gates.json's headSha against HEAD before trusting it."
    }
}

Write-Results -Result $result -Repo $repoForWrites

# A claim carries the paths and symbols its body claims exist; only this
# side can resolve them. Every ticket body examined in one session had
# drifted from the repo, so the warning goes out on stderr AND into the
# run scratch — stderr is what a human sees, and the file is what
# survives a firing that crashes between the claim and the read.
# It writes UNCONDITIONALLY now. It used to write only when something
# drifted, so a missing drift.txt meant either "nothing drifted" or
# "nothing was extracted to check" -- and the file was missing on three
# consecutive claims whose bodies had all drifted badly. A check whose
# silence has two meanings is a check the reader learns to skip.
if ($repoPath -and $result.payload.PSObject.Properties['citations']) {
    # The ticket's own category ranks an ambiguous citation. Without it a
    # `Status.md` cited by a Zenithmon ticket resolved to DevilsPlayground's,
    # deterministically, because git ls-files returns path order and D sorts
    # before Z -- and the report printed that pick as though it were an answer.
    $driftCategory = $null
    if ($result.payload.PSObject.Properties['category']) {
        $driftCategory = $result.payload.category
    }
    $drift = Get-BodyDrift -Repo $repoPath -Citations $result.payload.citations `
        -CategoryPaths (Get-CategoryPathPrefixes -Client $client -Category $driftCategory)
    $text = Format-BodyDrift -Key $result.payload.key -Missing $drift `
        -Citations $result.payload.citations
    if ($text) {
        Write-StdErr $text
        $driftDir = Join-Path (Get-ScratchRoot $repoForWrites) $result.payload.key
        if (-not (Test-Path -LiteralPath $driftDir)) {
            New-Item -ItemType Directory -Path $driftDir -Force | Out-Null
        }
        [System.IO.File]::WriteAllText((Join-Path $driftDir 'drift.txt'), $text,
            [System.Text.UTF8Encoding]::new($false))
    }
}

# A key written into a doc before its ticket existed is a forward
# reference that comes true — wrongly — the moment the sequence reaches
# it. Checked on the key the board ALLOCATED, so there is nothing to
# predict. See Find-KeyCitations for why this is not a board check.
$createdKey = Get-CreatedKey -Result $result -Argv $argv
if ($repoPath -and $createdKey) {
    $cited = Find-KeyCitations -Repo $repoPath -Key $createdKey `
        -Dirs (Get-LivingDocDirs -Client $client)
    $warning = Format-KeyCitations -Key $createdKey -Citations $cited
    if ($warning) { Write-StdErr $warning }
}

$isError = $result.PSObject.Properties['error'] -and $result.error

if ($asJson) {
    # `last.json` is written on EVERY --json call, refusals included: the
    # tick reads the payload from that file rather than the pipe, because
    # a firing can crash between the call and the read and a file
    # survives that. An exit-4 contract error is exactly the payload it
    # most needs to still be there.
    $json = $result.payload | ConvertTo-Json -Depth 100
    Write-Output $json
    Write-LastResult -Result $result -Repo $repoForWrites
} elseif ($isError) {
    # Exit 3 on an empty queue is ordinary control flow, not a crash —
    # but it is still not output anything should pipe onward.
    Write-StdErr $result.text
} else {
    Write-Output $result.text
}

Exit-Zagent $result.exitCode
