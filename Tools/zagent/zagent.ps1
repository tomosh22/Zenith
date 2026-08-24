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
        exit $script:EXIT_ERROR
    }
    return $url.TrimEnd('/')
}

function Get-BoardToken {
    $token = $env:ZAGENT_TOKEN
    if (-not $token) {
        Write-StdErr "ZAGENT_TOKEN is not set. Mint one on the board with ``zagent auth mint --name <this-machine>``."
        exit $script:EXIT_ERROR
    }
    return $token
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
        if ($detail) {
            try { $detail = ($detail | ConvertFrom-Json).error } catch { }
        }
        if ($status -eq 401) {
            Write-StdErr "The board rejected this token (401). Mint a new one with ``zagent auth mint``, or check ZAGENT_TOKEN."
            exit $script:EXIT_ERROR
        }
        if ($status -ge 400 -and $status -lt 500) {
            Write-StdErr "The board refused the request ($status): $detail"
            exit $script:EXIT_ERROR
        }
        Write-StdErr "Could not reach the board at $url — $($_.Exception.Message)"
        if ($detail) { Write-StdErr $detail }
        exit $script:EXIT_UNREACHABLE
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
    exit 0
}

if ($argv.Count -eq 0) { $argv = @('help') }

$repoPath = Find-ClientRepo
$client = Get-ClientProject -Repo $repoPath
$asJson = Test-Flag -Argv $argv -Name 'json'

$body = @{ argv = $argv }
if ($client) { $body.client = $client }

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
        exit $script:EXIT_ERROR
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

# ★★ DONE IS THE ONE STATUS THE DoD GETS A VOTE ON.
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
    $claimedStatus = Get-FlagValue -Argv $argv -Name 'status'
    if ($claimedStatus -and $claimedStatus.Trim().ToLowerInvariant() -eq 'done') {
        $unmet = Get-UnmetDoneCriteria -WorkLog $fileMap['worklog']
        if (@($unmet).Count -gt 0) {
            Write-StdErr "zagent: refusing to finish $($argv[1]) as Done -- its work log leaves $(@($unmet).Count) Definition-of-Done item(s) unticked:"
            foreach ($item in @($unmet)) { Write-StdErr "  [ ] $item" }
            Write-StdErr ''
            Write-StdErr 'A green gate says the tree compiles and the pinned count matches. It does not'
            Write-StdErr 'say the ticket was finished. Either tick the box because the work IS done, or'
            Write-StdErr 'finish as Blocked / "In Review" -- both accept an unmet DoD, and a Blocked log'
            Write-StdErr 'saying how far it got is the thing a human picks up.'
            exit $script:EXIT_ERROR
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

# ★★ A REFUSED CLAIM USED TO LEAVE THE TICKET CLAIMED, AND THAT HALTED THE REPO.
#
# The board's claim transaction writes FIRST and validates SECOND, so
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
# `needs-human`'s printed contract is "never claimed, by the queue OR BY NAME".
# This is what makes the second half true. Rolling back HERE rather than in the
# protocol is deliberate: a rule in prose in front of a check is the defect this
# repo keeps naming, and every caller of the CLI gets this one, not just a tick.
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
    exit $(if ($ok) { 0 } else { 1 })
}

$repoForWrites = if ($repoPath) { $repoPath } else { (Get-Location).Path }
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

exit $result.exitCode
