#Requires -Version 7.0
<#
.SYNOPSIS
  Assemble `.zagent/run/<KEY>/prompt.md` for a tick worker, deterministically.

.DESCRIPTION
  `/tick` step 5 ("Dispatch the worker") used to be six remembered rules —
  slice the Goal out of body.md, slice the Definition of Done out of body.md
  (NEVER the `definitionOfDone` array), quote `conventions` verbatim, open
  with five fixed clauses, write a file list, inline any governing rulings —
  and forgetting one has already cost a real incident:

    ZM-23's body carried "* ITEMS 1 AND 2 ARE ONE INDIVISIBLE COMMIT" as
    prose sitting BETWEEN two checkbox lines. The `definitionOfDone` array
    on the claim payload holds only the checkbox TEXT — never the prose
    around it — so an orchestrator who (reasonably) inlined the array
    instead of slicing body.md would hand the worker a spec silently
    missing its one indivisible-commit instruction, and nothing downstream
    could tell, because the worker has no shell and no board to notice the
    cut against.

  `Tools/zagent/protocol/TICK.md#6-verify` states the reason this exists as a script
  rather than as another paragraph of prose: "a rule enforced at a check
  that already fails is a rule, and the same rule in this paragraph is a
  hope." Six remembered rules become: two judgement calls (-Files,
  -Boundary) plus a deterministic slice of two files already on disk.

.PARAMETER Key
  The ticket key, e.g. ZM-70. Selects `.zagent/run/<Key>/{ticket.json,body.md}`.

.PARAMETER Repo
  Repo root. Defaults to walking up from THIS script's own directory for
  `zagent.project.json` -- the same search `Find-ClientRepo` in
  ZagentClient.psm1 already performs for `zagent.ps1`. Starting from the
  script rather than the caller's cwd means `build_prompt.ps1` finds the
  same repo whether the orchestrator invokes it from the repo root or from
  a game subdirectory, without reimplementing the walk.

.PARAMETER Files
  The exhaustive list of files the worker may edit. A JUDGEMENT CALL --
  tick.md spends most of step 5 on how to derive this (follow the DoD's
  nouns into the repo, but grep the SYMBOL never the prose; leave out a
  file that would let the worker decide something the ticket does not
  contain) and none of that is mechanical, so this script does not attempt
  to derive it. It only formats what it is given and writes filelist.txt.

.PARAMETER Boundary
  The one-sentence stop condition. Also a judgement call -- "the known
  edge of this ticket" is a reading of the ticket, not a fact a script can
  compute.

.PARAMETER Rulings
  Paths to files whose contents get inlined verbatim under a
  "## Rulings that govern this work" heading. Optional: most tickets carry
  none.

.NOTES
  Exit codes: 0 ok, 1 on any failure (message on stderr). The code is also
  written to a file, per the repo-wide rule at Write-LastExit in
  ZagentClient.psm1 -- "tick.md spends forty lines and three measured war
  stories on NEVER PIPE OR SUFFIX A CALL WHOSE EXIT CODE YOU INTEND TO
  READ, and it is still the most-broken rule in the protocol." This script
  is invoked directly with `pwsh -File`, not through `zagent.ps1`, so
  overwriting the shared `.zagent/last.exit` channel here would stomp the
  exit code of whatever real `zagent` command the orchestrator ran most
  recently -- the exact channel `/tick` reads to learn how THAT command
  went. So this reuses `Write-LastExit` (it is exported, and the shared
  channel is still the right place for "did the last thing I ran
  succeed") but ALSO writes a per-ticket
  `.zagent/run/<Key>/build_prompt.exit`, which is the file a step that
  specifically wants to know "did the LAST BUILD_PROMPT RUN for THIS
  ticket succeed" should read instead of racing a later zagent call for
  the shared file.
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Key,

    [string]$Repo,

    [Parameter(Mandatory = $true)]
    [string[]]$Files,

    [Parameter(Mandatory = $true)]
    [string]$Boundary,

    [string[]]$Rulings = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'ZagentClient.psm1') -Force

# These MUST match ZagentClient.psm1's own constants -- see that file's
# comment on why they cannot simply be imported (module scope, not the
# calling script's scope).
$script:EXIT_OK = 0
$script:EXIT_ERROR = 1

# The repo may not be known yet (a failed Find-ClientRepo means nothing to
# write $Repo/.zagent/... into), so track exit intent by hand rather than
# relying on Write-LastExit's own best-effort no-throw contract, and always
# call it on the way out -- including the early-failure paths, which is the
# entire point of recording it as a file rather than trusting `$?`.
function Write-BuildPromptExit {
    param([int]$Code, [string]$RepoPath, [string]$TicketKey)
    Write-LastExit -Code $Code -Repo $RepoPath
    if ($RepoPath -and $TicketKey) {
        try {
            $dir = Join-Path (Get-ScratchRoot $RepoPath) $TicketKey
            # Only ever write INTO an existing claim scratch -- never create
            # one. A run directory that does not exist means `zagent claim`
            # never ran (or its scratch was cleared), and this script only
            # READS that scratch; conjuring the directory just to drop an
            # exit code in it would leave a half-real `.zagent/run/<Key>/`
            # for a ticket nothing actually claimed.
            if (Test-Path -LiteralPath $dir) {
                [System.IO.File]::WriteAllText((Join-Path $dir 'build_prompt.exit'), "$Code`n",
                    [System.Text.UTF8Encoding]::new($false))
            }
        } catch {
            # Best effort, same contract as Write-LastExit -- never let
            # recording the code replace the code.
        }
    }
}

function Fail {
    param([string]$Message, [string]$RepoPath = $Repo, [string]$TicketKey = $Key)
    Write-StdErr "build_prompt: $Message"
    Write-BuildPromptExit -Code $script:EXIT_ERROR -RepoPath $RepoPath -TicketKey $TicketKey
    exit $script:EXIT_ERROR
}

function Get-OptionalValue {
    <#
      Like Get-OptionalString, but returns the property's own CLR value (or
      $null) instead of coercing everything through [string]. predispatch.json
      needs a genuine JSON `null` for a field that is truly absent -- `harness`
      most commonly, on a claim that predates `--harness` or never passed one
      -- and Get-OptionalString's [string]$Default parameter cannot carry that:
      passing $null to a [string]-typed parameter binds it as '', which would
      make "absent" and "explicitly empty" the same JSON value.
    #>
    param($Object, [string]$Name)
    if ($null -eq $Object) { return $null }
    $prop = $Object.PSObject.Properties[$Name]
    if (-not $prop) { return $null }
    return $prop.Value
}

function Get-OptionalString {
    <#
      Read a possibly-absent string property without throwing.

      Set-StrictMode turns a missing property into a throw, and
      `conventions` -- like `baseBranch` in Get-ClientChecks -- is legal to
      omit from a payload. A crash here would be strictly worse than an
      empty conventions clause: it would take down the whole dispatch over
      a field nothing downstream requires to exist.
    #>
    param($Object, [string]$Name, [string]$Default = '')
    if ($null -eq $Object) { return $Default }
    $prop = $Object.PSObject.Properties[$Name]
    if (-not $prop -or $null -eq $prop.Value) { return $Default }
    return [string]$prop.Value
}

function Get-MarkdownSection {
    <#
      The line range [StartIndex, EndIndex) of a heading section: the
      heading line itself through every line up to (not including) the
      next heading of the SAME OR HIGHER level.

      "Same or higher" is what makes this correct for a body that nests
      subsections under its Goal or its DoD (neither does today, but
      nothing stops one from starting to) -- a level-3 subheading under a
      level-2 "## Definition of Done" must stay INSIDE the slice, and only
      the next level-1-or-2 heading ends it.

      Returns $null when the heading is not found, so the two call sites
      below can tell "absent" from "empty" -- Get-DoD-Section-missing is a
      hard failure; nothing else here uses that distinction.
    #>
    param([string[]]$Lines, [string]$HeadingText)

    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match '^(#{1,6})\s+(.+?)\s*$') {
            $level = $Matches[1].Length
            $text = $Matches[2].Trim()
            if ($text -ieq $HeadingText) {
                $end = $Lines.Count
                for ($j = $i + 1; $j -lt $Lines.Count; $j++) {
                    if ($Lines[$j] -match '^(#{1,6})\s') {
                        if ($Matches[1].Length -le $level) { $end = $j; break }
                    }
                }
                return [PSCustomObject]@{ StartIndex = $i; EndIndex = $end }
            }
        }
    }
    return $null
}

function Get-RangeText {
    <#
      Lines[Start..End) joined back into text, WITHOUT the PowerShell
      reversed-range trap: `$Lines[5..4]` does not error, it returns
      lines 5 and 4 in that order, which would silently reverse an
      empty section's neighbours into the output.
    #>
    param([string[]]$Lines, [int]$Start, [int]$End)
    if ($End -le $Start) { return '' }
    return ($Lines[$Start..($End - 1)] -join "`n")
}

# ─── Resolve the repo ─────────────────────────────────

if (-not $Repo) {
    $Repo = Find-ClientRepo -StartDir $PSScriptRoot
    if (-not $Repo) {
        Write-StdErr "build_prompt: no zagent.project.json found walking up from $PSScriptRoot -- pass -Repo explicitly."
        Write-BuildPromptExit -Code $script:EXIT_ERROR -RepoPath $null -TicketKey $Key
        exit $script:EXIT_ERROR
    }
}
if (-not (Test-Path -LiteralPath $Repo)) {
    Write-StdErr "build_prompt: -Repo $Repo does not exist."
    Write-BuildPromptExit -Code $script:EXIT_ERROR -RepoPath $null -TicketKey $Key
    exit $script:EXIT_ERROR
}
$Repo = (Resolve-Path -LiteralPath $Repo).Path

# ─── Judgement-call inputs: validate them before touching disk ───────
#
# `-Files <string[]>` (and `-Rulings <string[]>` below) cannot mean
# "space-separated CLI values" the way it would for an in-session function
# call, because this script is invoked as `pwsh -File ...` -- a REAL
# process boundary. Measured while building this script: PowerShell's own
# native-argument marshalling for a nested
# `pwsh -File child.ps1 -Files a.cpp,b.cpp` collapses the comma-array
# literal into ONE argv token with the commas still embedded
# ("a.cpp,b.cpp"), rather than exploding it into separate elements the way
# it would for an in-process `&` call. A naive `[string[]]$Files` binds
# that as a ONE-ELEMENT array holding the literal text "a.cpp,b.cpp", and
# every file after the first silently vanishes into a single path that
# does not exist. Splitting every bound value on commas here is what makes
# the documented interface actually work when invoked exactly as
# specified -- and is a no-op for a value that never had a comma in it
# (repo paths in this tree do not contain one -- same accepted assumption
# for both parameters).
function Split-CommaJoinedValues {
    param([string[]]$Values)
    return @(
        foreach ($raw in $Values) {
            foreach ($piece in ($raw -split ',')) {
                $trimmed = $piece.Trim()
                if ($trimmed) { $trimmed }
            }
        }
    )
}

$Files = @(Split-CommaJoinedValues -Values $Files)
$Rulings = @(Split-CommaJoinedValues -Values $Rulings)

if ($Files.Count -eq 0) {
    Fail "-Files was empty. The file list is load-bearing in both directions (tick.md step 5) -- an empty list is never a valid dispatch."
}
if ([string]::IsNullOrWhiteSpace($Boundary)) {
    Fail "-Boundary was empty. A dispatch with no stop condition is exactly the 'improvise past it' failure mode step 5 exists to prevent."
}
foreach ($rulingPath in $Rulings) {
    if (-not (Test-Path -LiteralPath $rulingPath)) {
        Fail "-Rulings path '$rulingPath' does not exist."
    }
}

# ─── Read the claim's scratch -- ticket.json and body.md ─────────────

$runDir = Join-Path (Get-ScratchRoot $Repo) $Key
$ticketPath = Join-Path $runDir 'ticket.json'
$bodyPath = Join-Path $runDir 'body.md'

if (-not (Test-Path -LiteralPath $ticketPath)) {
    Fail "$ticketPath does not exist. Run 'zagent claim $Key' (or 'next') first -- this script only READS the claim's scratch, it never creates it."
}
if (-not (Test-Path -LiteralPath $bodyPath)) {
    Fail "$bodyPath does not exist. Run 'zagent claim $Key' (or 'next') first -- this script only READS the claim's scratch, it never creates it."
}

try {
    $ticketRaw = Get-Content -LiteralPath $ticketPath -Raw -Encoding utf8
    $ticket = $ticketRaw | ConvertFrom-Json
} catch {
    Fail "$ticketPath is not valid JSON -- $($_.Exception.Message)"
}

$bodyRaw = Get-Content -LiteralPath $bodyPath -Raw -Encoding utf8
$bodyLines = $bodyRaw -split "`r`n|`n"

$conventions = Get-OptionalString $ticket 'conventions' ''

# ─── ★★ THE ONE RULE THIS SCRIPT EXISTS TO ENFORCE ────────────────────
#
# The Definition of Done comes from body.md's own "## Definition of Done"
# heading, and ONLY from there. The `definitionOfDone` array on ticket.json
# is checkbox TEXT ONLY -- the board strips it out of the body for counting
# boxes, and every character of prose between or around those boxes (ZM-23's
# "ITEMS 1 AND 2 ARE ONE INDIVISIBLE COMMIT") is invisible to it. This
# script never reads that array at all, which is a stronger guarantee than
# "prefers body.md" -- there is no code path here that could silently fall
# back to the mutilated version.

$dodSection = Get-MarkdownSection -Lines $bodyLines -HeadingText 'Definition of Done'
if (-not $dodSection) {
    Fail "$bodyPath has no '## Definition of Done' heading (any level, e.g. '##' or '###'). Refusing to fall back to the ticket's definitionOfDone array -- that array holds only checkbox text and drops any prose between or around the boxes (see ZM-23). Fix the body, or if this ticket genuinely has no DoD section, that is itself something to report rather than paper over."
}
$dodContent = (Get-RangeText -Lines $bodyLines -Start ($dodSection.StartIndex + 1) -End $dodSection.EndIndex).Trim()

$goalSection = Get-MarkdownSection -Lines $bodyLines -HeadingText 'Goal'
$goalContent = if ($goalSection) {
    (Get-RangeText -Lines $bodyLines -Start ($goalSection.StartIndex + 1) -End $goalSection.EndIndex).Trim()
} else {
    ''
}

# "The rest of the body" -- everything that is not the Goal or the DoD
# section, in original order, so a ticket's other prose (rulings context,
# scope notes, "almost all of the machinery already exists" -- whatever it
# carries) still reaches the worker. I3 says paste the body; Goal and DoD
# just get their own headings pulled to the front of the required shape.
$excluded = [System.Collections.Generic.HashSet[int]]::new()
if ($goalSection) {
    for ($i = $goalSection.StartIndex; $i -lt $goalSection.EndIndex; $i++) { [void]$excluded.Add($i) }
}
for ($i = $dodSection.StartIndex; $i -lt $dodSection.EndIndex; $i++) { [void]$excluded.Add($i) }

$restLines = [System.Collections.Generic.List[string]]::new()
for ($i = 0; $i -lt $bodyLines.Count; $i++) {
    if (-not $excluded.Contains($i)) { [void]$restLines.Add($bodyLines[$i]) }
}
$restContent = ($restLines -join "`n") -replace '(\n){3,}', "`n`n"
$restContent = $restContent.Trim()

# ─── Rulings, verbatim ────────────────────────────────

$rulingsBlocks = [System.Collections.Generic.List[string]]::new()
foreach ($rulingPath in $Rulings) {
    $content = (Get-Content -LiteralPath $rulingPath -Raw -Encoding utf8).TrimEnd()
    $name = Split-Path -Path $rulingPath -Leaf
    [void]$rulingsBlocks.Add("### $name`n`n$content")
}

# ─── The exhaustive file list -- an orchestrator judgement, formatted here ──

$normalisedFiles = @($Files | ForEach-Object { ConvertTo-PosixPath $_ })
$filesInline = $normalisedFiles -join ', '

# `$Repo` is the checkout whose own copy of this script ran -- the lane,
# when invoked as TICK.md's relative `Tools\zagent\build_prompt.ps1` from a
# lane's worktree (Find-ClientRepo -StartDir $PSScriptRoot, above). Every
# path this prompt hands the worker is repo-relative and never says
# relative to WHAT, which is silently correct when the harness session is
# rooted in the same checkout the ticket belongs to and silently wrong
# when an orchestrator drives a registered lane from a session rooted
# elsewhere (or on a harness whose subagents do not inherit cwd) -- the
# worker then greps and edits the MAIN checkout instead of the lane
# (observed 2026-09-23). Stated once, in the step-zero clause, below.
$repoPosix = ConvertTo-PosixPath $Repo

# ─── Assemble the prompt ──────────────────────────────
#
# This is a transcription of the five clauses at tick.md step 5, kept
# word-for-word: a paraphrase here is exactly the kind of drift the whole
# script exists to remove.

$lines = [System.Collections.Generic.List[string]]::new()

# ─── The blast-radius survey (ZEN-89) ─────────────────
#
# tick.md step 5 mandates this as the worker's FIRST instruction, and this
# script did not emit it -- so every tick that trusted the script silently
# lost the one control that catches an under-scoped file list. It is emitted
# here, verbatim from the protocol, rather than left to each orchestrator to
# remember: a clause that has to be pasted by hand is a clause that is
# sometimes not pasted.
#
# Why it is the WORKER's job and not the orchestrator's: deriving the list
# happens before anyone has read the code the ticket is about, while the
# worker has the task context and a fresh budget. Making the list a hypothesis
# the worker must falsify is what caught ZM-69's missing scene-registry row
# and ZEN-40's entirely wrong subsystem -- in the latter case the worker made
# ZERO edits and returned a corrected list pointing two directories away.
[void]$lines.Add('**Step zero, before you edit anything.** Every path in this prompt — the file list below and everything you grep or read — is relative to the repository root `' + $repoPosix + '`: read, grep and edit only under it. For every identifier and every behaviour you intend to change, grep the whole repository and confirm each hit that must CHANGE is inside the file list below. Read the UNITS that reference them, not just the file names — a test can pin your change without naming it in a way that greps. A file list bounds WRITES, not READS: read as widely as you need. **If ANY hit that must change lands outside the list, STOP and report the complete corrected list before making a single edit.** Say what the survey found even if it found nothing.')
[void]$lines.Add('')
[void]$lines.Add('**DO NOT** attempt to run **CI gate command lines**, build, test, commit, push, or touch the board — you have no shell; the orchestrator does all of that.')
[void]$lines.Add("**Files you may edit:** $filesInline.")
[void]$lines.Add("**Conventions:** $conventions")
[void]$lines.Add('**Report back:** files changed, a one-paragraph summary, proposed work-log text, and any decisions, questions, shortfalls or workflow suggestions you want recorded.')
[void]$lines.Add("**Boundary:** $Boundary On reaching it, STOP and report — do not improvise past it.")
[void]$lines.Add('')
[void]$lines.Add('## Goal')
[void]$lines.Add('')
[void]$lines.Add($goalContent)
[void]$lines.Add('')
[void]$lines.Add('## Definition of Done')
[void]$lines.Add('')
[void]$lines.Add($dodContent)
if ($restContent) {
    [void]$lines.Add('')
    [void]$lines.Add($restContent)
}
if ($rulingsBlocks.Count -gt 0) {
    [void]$lines.Add('')
    [void]$lines.Add('## Rulings that govern this work')
    [void]$lines.Add('')
    [void]$lines.Add(($rulingsBlocks -join "`n`n"))
}

$promptText = ($lines -join "`n") + "`n"

# ─── Write ─────────────────────────────────────────────

if (-not (Test-Path -LiteralPath $runDir)) {
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
}

$promptPath = Join-Path $runDir 'prompt.md'
if (Test-Path -LiteralPath $promptPath) {
    Write-StdErr "build_prompt: $promptPath already exists -- overwriting (a re-dispatch is normal)."
}
# No BOM: prompt.md is read by a worker's Read tool and by a human diffing
# a re-dispatch, and a BOM shows up as a spurious diff in both -- same rule
# Write-Results follows in ZagentClient.psm1.
[System.IO.File]::WriteAllText($promptPath, $promptText, [System.Text.UTF8Encoding]::new($false))

# filelist.txt is a CONTRACT with the (parallel) out-of-scope-edit check:
# plain, one path per line, no header, no blank lines, LF endings. Do not
# decorate this file -- anything added here is a field the other side's
# parser was not written to skip.
$filelistPath = Join-Path $runDir 'filelist.txt'
$filelistText = ($normalisedFiles -join "`n") + "`n"
[System.IO.File]::WriteAllText($filelistPath, $filelistText, [System.Text.UTF8Encoding]::new($false))

# ─── predispatch.json -- the WORKER-MOVED-NOTHING baseline ────────────
#
# `Tools/Vault/cli/tick-checks.mjs`'s `workerMovedNothing` is I1's
# compensating control: did the WORKER commit, branch, stage, touch the
# board, or litter scratch, between this write and `guard` (TICK.md step
# 6.4)? This is DELIBERATELY THE LAST WRITE in this script, after `claim`
# (a board write -- every board write is a commit, so a baseline taken at
# claim time always sees a HEAD that has since moved) and after step 4's
# `git switch -c` for a `branching: "branch"` category (which runs BETWEEN
# claim and here) -- taking the baseline any earlier reports a phantom
# headMoved/branchMoved for something the ORCHESTRATOR did, not the worker.
function Get-GitLine {
    param([string[]]$Arguments)
    $output = & git -C $Repo @Arguments 2>$null
    if ($LASTEXITCODE -ne 0) {
        Fail "git $($Arguments -join ' ') failed (exit $LASTEXITCODE) -- cannot write predispatch.json without it."
    }
    return [string]$output
}

$predispatchHead = Get-GitLine -Arguments @('rev-parse', 'HEAD')
$predispatchBranch = Get-GitLine -Arguments @('rev-parse', '--abbrev-ref', 'HEAD')
$statusLines = @(& git -C $Repo status --porcelain 2>$null)
if ($LASTEXITCODE -ne 0) {
    Fail "git status --porcelain failed (exit $LASTEXITCODE) -- cannot write predispatch.json without it."
}
# Sorted, deduplicated -- a SET, not a log: `workerMovedNothing` compares this
# against the porcelain lines it reads later to tell a line that was already
# staged before dispatch apart from one the worker staged itself.
$predispatchPorcelain = @($statusLines | Where-Object { $_ -and $_.Trim() -ne '' } | Sort-Object -Unique)

$predispatch = [ordered]@{
    head      = $predispatchHead
    branch    = $predispatchBranch
    porcelain = $predispatchPorcelain
    boardRow  = [ordered]@{
        status    = Get-OptionalValue $ticket 'status'
        assignee  = Get-OptionalValue $ticket 'assignee'
        updatedAt = Get-OptionalValue $ticket 'updatedAt'
    }
    harness   = Get-OptionalValue $ticket 'harness'
    at        = [DateTime]::UtcNow.ToString('o')
}
$predispatchPath = Join-Path $runDir 'predispatch.json'
$predispatchText = (($predispatch | ConvertTo-Json -Depth 5) + "`n")
[System.IO.File]::WriteAllText($predispatchPath, $predispatchText, [System.Text.UTF8Encoding]::new($false))

Write-BuildPromptExit -Code $script:EXIT_OK -RepoPath $Repo -TicketKey $Key
exit $script:EXIT_OK
