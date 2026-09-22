#Requires -Version 7.0
<#
.SYNOPSIS
  Generate every per-harness `/tick` artifact from the canonical, harness-
  neutral sources: Tools/zagent/protocol/TICK.md, the two role charters in
  Tools/zagent/protocol/roles/, Tools/zagent/protocol/harnesses.json and
  Tools/zagent/protocol/entrypoint.md.tmpl.

.DESCRIPTION
  Stage 3 of the harness-neutrality work. Before this script, the protocol
  lived twice by hand: Tools/zagent/protocol/TICK.md (the harness-neutral
  body) and .claude/commands/tick.md (frontmatter + a hand-kept copy of the
  same body), plus two hand-written agent definitions
  (.claude/agents/zagent-{worker,reviewer}.md) whose tool lists were the
  ONLY enforcement of invariant I1 ("the worker never builds, tests, runs,
  commits, or touches the board"). Every one of those becomes a generated
  file here, so a model swap, a tier being retired, or a protected-path
  list changing is one edit to harnesses.json / vault.config.json followed
  by a regeneration, never a hand-edit to the generated copies.

  Emits, for `-Harness claude` (the default):

    .claude/commands/tick.md                    -- frontmatter + TICK.md's body
    .claude/agents/zagent-worker-sonnet.md       -- WORKER, sonnet tier
    .claude/agents/zagent-worker-opus.md         -- WORKER, opus tier
    .claude/agents/zagent-reviewer-low.md        -- REVIEWER, LOW risk
    .claude/agents/zagent-reviewer-medium.md     -- REVIEWER, MEDIUM risk
    .claude/agents/zagent-reviewer-high.md       -- REVIEWER, HIGH risk
    Tools/zagent/protocol/roles/generated/worker.resolved.md   -- resolved worker charter
    Tools/zagent/protocol/roles/generated/reviewer.resolved.md -- resolved reviewer charter

  and `git rm`s the two now-superseded hand-written agent files
  (.claude/agents/zagent-worker.md, .claude/agents/zagent-reviewer.md).

  `-Harness opencode` (Stage 5) emits .opencode/opencode.json (five agent
  entries under its "agent" map) and .opencode/commands/tick.md (a "stub"
  entry point pointing at TICK.md, per its bodyDelivery: "reference").

  `-Harness codex` (Stage 6) emits five .codex/agents/*.toml role files
  (2 worker tiers + 3 reviewer risks, no haiku worker), the
  .codex/skills/tick/SKILL.md entry point (deliveryMode: "inline" -- the
  full TICK.md body embedded, Codex having no {file:...}-equivalent
  reference mechanism), and .codex/config.toml (the pre-authorisation
  layer). See Get-CodexArtifacts's own comments for the TOML key set and
  the measurements it rests on (app.asar, stage0-verdicts.md).

  All three harnesses' profiles live as DATA in
  Tools/zagent/protocol/harnesses.json regardless of which one a given
  call emits -- `-Harness <id>` only restricts which artifacts this run
  touches (see that parameter's own doc below).

  ★ ONLY THE WORKER CHARTER CARRIES A SUBSTITUTION. `{{PROTECTED_PATHS}}`
  in Tools/zagent/protocol/roles/worker.md is filled from vault.config.json's
  `policy.resolved.guardrails.protectedPaths` -- the RESOLVED list, which is
  what `zagent guard` actually enforces (see that file's own comment on why
  `zagent.project.json`'s own `protectedPaths` is documentation only: it is
  one link in a `??` FALLBACK chain that `boardGuardrails` -- i.e. this
  same resolved list -- always wins over). Reading the enforced list here,
  rather than re-typing it, is what makes the charter unable to drift from
  the mechanism that actually blocks a ticket.

  ★ deliveryMode -- "inline" for claude, chosen DELIBERATELY over "stub".
  A "stub" entry point would tell the worker to go Read
  Tools/zagent/protocol/TICK.md itself rather than carrying a full copy
  inline into .claude/commands/tick.md. That would remove the duplication
  entirely, but nobody has measured whether a worker actually follows a
  stub as reliably as an inline copy -- the plan wanted two or three ticks
  run each way before trusting it. Absent that measurement, this script
  keeps the form ALREADY PROVEN IN PRODUCTION (today's tick.md IS a full
  inline copy) rather than betting on the unmeasured alternative. See
  harnesses.json's own `$comment_deliveryMode` for the per-harness
  reasoning (OpenCode gets "stub" because it has a REAL {file:...}
  resolution mechanism Stage 0 measured; Codex gets "inline" because it has
  no equivalent).

  ★★ AND THE STUB FORM HAS AN EXPIRY DATE, RECORDED HERE SO IT IS NOT
  MISTAKEN FOR A PROPERTY. If "stub" is ever selected for a harness whose
  worker resolves it by reading TICK.md with a single Read call, that only
  works while the protocol fits inside that tool's default read window.
  Claude's own Read tool defaults to a 2000-line window, and
  Tools/zagent/protocol/TICK.md is currently 1952 lines -- **48 lines of
  headroom: 117 when first measured, 98 after the protocol was
  neutralised, 48 now**. The TREND is the finding -- the protocol grows,
  because nearly every addition to it is a recorded failure. Note also
  that 2000 is CLAUDE CODE'S Read default; no other harness's window has
  been measured, so a "stub" harness is bounded by a number borrowed from
  a tool it does not use. That margin
  shrinks every time TICK.md grows and nobody is re-measuring it by hand;
  Tools/zagent/Test-HarnessParity.ps1 carries the assertion that fails
  once it thins below 50 lines, specifically so this stops being something
  a person has to remember to check.

.PARAMETER Check
  Regenerate every artifact to memory (never touching disk) and byte-
  compare each one against what is actually on disk. Prints every stale or
  missing path and exits 1 if any are found, 0 if the tree already matches
  what the generator would produce -- the same shape as `zenith regen
  --check`. Also flags the two superseded hand-written agent files as
  stale if either still exists on disk.

.PARAMETER Harness
  Restrict generation (or, under -Check, comparison) to one harness.
  Defaults to "claude" -- so that changing only Claude's model roster in
  harnesses.json can never produce a diff in OpenCode's or Codex's
  artifacts (each emitter reads only its own `harnesses.<id>` branch).
  Pass "opencode" or "codex" explicitly to generate or check those.

.NOTES
  Idempotent: running this twice with no source change produces byte-
  identical output both times, so `git status` does not change between
  the two runs. All generated text is normalised to CRLF line endings
  (ConvertTo-Crlf below) to match this repo's own convention for these
  files (Tools/zagent/protocol/TICK.md and today's .claude/commands/tick.md
  are both CRLF on disk) regardless of which line ending the canonical
  charter sources happen to be authored with.
#>

[CmdletBinding()]
param(
    [switch]$Check,

    [ValidateSet('claude', 'opencode', 'codex')]
    [string]$Harness = 'claude'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$here = $PSScriptRoot
Import-Module (Join-Path $here 'ZagentClient.psm1') -Force

$repoRoot = Find-ClientRepo -StartDir $here
if (-not $repoRoot) {
    throw "Build-HarnessArtifacts: no zagent.project.json found walking up from $here"
}

$protocolDir = Join-Path $repoRoot 'Tools/zagent/protocol'
$rolesDir = Join-Path $protocolDir 'roles'
$generatedRolesDir = Join-Path $rolesDir 'generated'
$claudeAgentsDir = Join-Path $repoRoot '.claude/agents'
$claudeCommandsDir = Join-Path $repoRoot '.claude/commands'

# ─── Small helpers ─────────────────────────────────────

function ConvertTo-Crlf {
    <#
      Normalise to CRLF regardless of the input's own line endings, so a
      canonical charter authored with plain LF and TICK.md's own native
      CRLF combine into one uniformly-CRLF generated file rather than a
      mixture. Normalise-then-expand (never a blind LF->CRLF replace) so a
      file that is ALREADY CRLF is left alone rather than doubled into
      CRCRLF.
    #>
    param([string]$Text)
    $lfOnly = $Text -replace "`r`n", "`n"
    return $lfOnly -replace "`n", "`r`n"
}

function Read-RawText {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Build-HarnessArtifacts: required source file is missing: $Path"
    }
    return [System.IO.File]::ReadAllText($Path)
}

function Write-Utf8NoBom {
    <#
      No BOM: every one of these files is read by a harness's own Read
      tool or diffed by a human, and a BOM shows up as a spurious diff in
      both -- the same rule build_prompt.ps1's Write-Results follows.
    #>
    param([string]$Path, [string]$Content)
    $dir = Split-Path -Path $Path -Parent
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Get-GeneratedHeaderComment {
    param([string]$SourceDescription)
    return "<!--`nGENERATED — DO NOT EDIT.`nSource: $SourceDescription`nRegenerate: pwsh -NoProfile -File Tools/zagent/Build-HarnessArtifacts.ps1`n-->"
}

# ─── Load the two canonical charters, and guard the TOML trap NOW ─────
#
# Item 6 of the Stage 3 brief: a later stage embeds each charter inside a
# TOML multi-line LITERAL string ('''...'''), specifically to survive the
# backslashes, backticks and double quotes both charters already contain
# (Tools\zenith.ps1, `git status`, "Files changed"). A literal ''' has
# exactly one failure mode -- a literal ''' occurring IN the body -- and a
# basic string (""\"...\"\"") would mangle everything else, so the fix is
# "never emit a form that needs escaping", not "escape harder". Assert the
# precondition here, on every run of this script, long before any TOML
# emitter exists to need it: a charter edit that introduces a literal '''
# fails loudly today rather than producing malformed TOML in Stage 5/6.
$workerCharterPath = Join-Path $rolesDir 'worker.md'
$reviewerCharterPath = Join-Path $rolesDir 'reviewer.md'
$workerCharter = Read-RawText $workerCharterPath
$reviewerCharter = Read-RawText $reviewerCharterPath

foreach ($pair in @(
        @{ Name = 'roles/worker.md'; Text = $workerCharter },
        @{ Name = 'roles/reviewer.md'; Text = $reviewerCharter }
    )) {
    if ($pair.Text.Contains("'''")) {
        throw "Build-HarnessArtifacts: $($pair.Name) contains a literal ''' -- a later stage embeds this charter inside a TOML multi-line LITERAL string ('''...'''), whose one failure mode is exactly this sequence appearing in the body. Hard-failing here rather than escaping: rewrite the charter to avoid a literal triple-single-quote run."
    }
}

# ─── Load harnesses.json + vault.config.json's ENFORCED protected paths ──

$harnessesConfig = Get-Content -Raw (Join-Path $protocolDir 'harnesses.json') | ConvertFrom-Json
$vaultConfigPath = Join-Path $repoRoot 'vault.config.json'
$vaultConfig = Get-Content -Raw $vaultConfigPath | ConvertFrom-Json

$protectedPaths = @($vaultConfig.policy.resolved.guardrails.protectedPaths)
if ($protectedPaths.Count -eq 0) {
    throw "Build-HarnessArtifacts: vault.config.json policy.resolved.guardrails.protectedPaths is empty -- refusing to generate a worker charter that would claim no path is protected."
}

function Format-ProtectedPathsList {
    <#
      "`.a/**`, `.b`, or `.c`" -- backticked, Oxford-commaed. This is the
      text {{PROTECTED_PATHS}} expands to; it is built from the RESOLVED
      list (see the file header) precisely so the charter cannot name a
      path list the enforcement mechanism does not actually use.
    #>
    param([string[]]$Paths)
    $backticked = @($Paths | ForEach-Object { "``$_``" })
    if ($backticked.Count -eq 1) { return $backticked[0] }
    if ($backticked.Count -eq 2) { return "$($backticked[0]) or $($backticked[1])" }
    $allButLast = $backticked[0..($backticked.Count - 2)] -join ', '
    return "$allButLast, or $($backticked[-1])"
}
$protectedPathsText = Format-ProtectedPathsList -Paths $protectedPaths

# ─── Resolve the requested harness's profile ──────────────────────────

if (-not $harnessesConfig.harnesses.PSObject.Properties[$Harness]) {
    throw "Build-HarnessArtifacts: harnesses.json declares no '$Harness' entry."
}
$harnessProfile = $harnessesConfig.harnesses.$Harness

# ═══════════════════════════════════════════════════════════════════════
#  CLAUDE emitter
# ═══════════════════════════════════════════════════════════════════════

function Get-SubstitutedWorkerBody {
    param($HarnessProfile)
    $body = $workerCharter.Replace('{{PROTECTED_PATHS}}', $protectedPathsText)
    if ($HarnessProfile.workerToolDenial -eq 'declared') {
        # Item 5 of the Stage 3 brief -- one conditional here, not a second
        # hand-maintained charter. Verbatim text from the brief.
        $declaredParagraph = "**You have a shell here and you must not use it for anything.** No build, no test, no ``git``, no ``zagent``. The orchestrator records the repository's exact state before you start and compares it after; any commit, any staged file, any board mutation is detected and Blocks the ticket regardless of how good the work is."
        $body = $body.TrimEnd() + "`n`n" + $declaredParagraph + "`n"
    }
    return $body
}

function New-AgentFileContent {
    param(
        [string]$Name,
        [string]$Description,
        [string]$Tools,
        [string]$Model,
        [string]$SourceDescription,
        [string]$Body
    )
    $fmLines = @(
        '---'
        "name: $Name"
        "description: $Description"
        "tools: $Tools"
        "model: $Model"
        '---'
    )
    $fm = $fmLines -join "`n"
    $header = Get-GeneratedHeaderComment -SourceDescription $SourceDescription
    $raw = "$fm`n`n$header`n`n$Body"
    return ConvertTo-Crlf $raw
}

function Get-ClaudeArtifacts {
    <#
      Returns an ordered map of repo-relative path -> generated content,
      for every file `-Harness claude` owns, plus the list of now-
      superseded paths it wants REMOVED. Pure -- touches no disk -- so
      -Check can call it exactly the same way emission does.
    #>
    param($HarnessProfile)

    $artifacts = [ordered]@{}

    $workerBody = Get-SubstitutedWorkerBody -HarnessProfile $HarnessProfile
    $reviewerBody = $reviewerCharter

    $workerToolsList = 'Read, Edit, Write, Grep, Glob'
    $reviewerToolsList = 'Read, Grep, Glob'

    $denialClause = if ($HarnessProfile.workerToolDenial -eq 'declared') {
        'cannot build, test, run gates, commit, or touch the board by role convention alone -- this harness does not structurally deny it (see WORKER-MOVED-NOTHING at TICK.md step 6.4)'
    } else {
        'cannot build, test, run gates, commit, or touch the board by construction'
    }

    $workerSource = 'Tools/zagent/protocol/roles/worker.md + Tools/zagent/protocol/harnesses.json (claude.roles.worker) + vault.config.json (policy.resolved.guardrails.protectedPaths)'
    $reviewerSource = 'Tools/zagent/protocol/roles/reviewer.md + Tools/zagent/protocol/harnesses.json (claude.roles.reviewerByRisk)'

    # ── Worker role files, one per routed tier (sonnet, opus). haiku is
    #    absent from roles.worker by design (unusedTiers) and generates
    #    NOTHING -- this loop is the enforcement of that, not a comment
    #    about it: it can only ever iterate what roles.worker actually has.
    foreach ($tierProp in $HarnessProfile.roles.worker.PSObject.Properties) {
        $tier = $tierProp.Name
        $model = $tierProp.Value.model
        $description = "The AUTHORING half of a /tick, routed at the $tier tier. Implements one ticket's file edits from a spec inlined into its prompt. It $denialClause."
        $content = New-AgentFileContent -Name "zagent-worker-$tier" -Description $description `
            -Tools $workerToolsList -Model $model -SourceDescription $workerSource -Body $workerBody
        $artifacts[".claude/agents/zagent-worker-$tier.md"] = $content
    }

    # ── Reviewer role files, one per risk (LOW, MEDIUM, HIGH) -- ALL
    #    THREE, per the Stage 3 brief: review.required is the OR of
    #    reviewOn and the DoD-assertion scan, so even a risk absent from
    #    reviewOn (LOW) can still need a reviewer model.
    foreach ($riskProp in $HarnessProfile.roles.reviewerByRisk.PSObject.Properties) {
        $risk = $riskProp.Name
        $model = $riskProp.Value.model
        $riskLower = $risk.ToLowerInvariant()
        $description = "The REVIEW half of a /tick, spawned when a ticket's review is required — the OR of its risk being in reviewOn and a scan of its Definition of Done for claims a gate cannot settle — routed at the $risk-risk tier. Reads an inlined diff and reports findings. Read-only by construction, so it cannot edit, build, or run anything."
        $content = New-AgentFileContent -Name "zagent-reviewer-$riskLower" -Description $description `
            -Tools $reviewerToolsList -Model $model -SourceDescription $reviewerSource -Body $reviewerBody
        $artifacts[".claude/agents/zagent-reviewer-$riskLower.md"] = $content
    }

    # ── Resolved charters -- substituted bodies, no frontmatter. Today's
    #    reader is nobody (Stage 3 generates them but nothing consumes
    #    them yet); OpenCode's {file:...} bodyDelivery is the consumer a
    #    later stage wires up (harnesses.json: opencode.bodyDelivery).
    $resolvedWorkerHeader = Get-GeneratedHeaderComment -SourceDescription 'Tools/zagent/protocol/roles/worker.md + vault.config.json (policy.resolved.guardrails.protectedPaths)'
    $artifacts['Tools/zagent/protocol/roles/generated/worker.resolved.md'] = ConvertTo-Crlf "$resolvedWorkerHeader`n`n$workerBody"

    $resolvedReviewerHeader = Get-GeneratedHeaderComment -SourceDescription 'Tools/zagent/protocol/roles/reviewer.md'
    $artifacts['Tools/zagent/protocol/roles/generated/reviewer.resolved.md'] = ConvertTo-Crlf "$resolvedReviewerHeader`n`n$reviewerBody"

    # ── The entry point itself: frontmatter (unchanged from the
    #    hand-authored original) + generated header + TICK.md's body,
    #    inline (deliveryMode: "inline" -- see the file header).
    $tickBody = Read-RawText (Join-Path $protocolDir 'TICK.md')
    $entryFm = @(
        '---'
        'description: Run one agent ticket — the queue head, or a ticket you name'
        'allowed-tools: Bash(zagent:*), Bash(timeout *), Bash(pwsh -NoProfile*), Bash(git:*), Agent, Read, Write, Edit, ScheduleWakeup, TaskStop'
        'argument-hint: [PROJECT_KEY | TICKET_KEY]'
        '---'
    ) -join "`n"
    $entryHeader = Get-GeneratedHeaderComment -SourceDescription 'Tools/zagent/protocol/TICK.md (body) + Tools/zagent/protocol/harnesses.json (claude entry point profile, deliveryMode: inline)'
    $entryRaw = "$entryFm`n`n$entryHeader`n`n$tickBody"
    $artifacts[$HarnessProfile.entryPoint] = ConvertTo-Crlf $entryRaw

    $toRemove = @(
        '.claude/agents/zagent-worker.md',
        '.claude/agents/zagent-reviewer.md'
    )

    return [PSCustomObject]@{
        Artifacts = $artifacts
        ToRemove  = $toRemove
    }
}

# ═══════════════════════════════════════════════════════════════════════
#  OPENCODE emitter
# ═══════════════════════════════════════════════════════════════════════
#
# Two files, both consumed by the OpenCode desktop app's own config
# loader (Stage 0 probes 3/4/9/10, all MEASURED against the running app):
#
#   .opencode/opencode.json        -- agent DEFINITIONS ONLY (five: the two
#                                      WORKER tiers + three REVIEWER risks),
#                                      under the top-level "agent" key.
#   .opencode/commands/tick.md     -- the entry point, built from
#                                      entrypoint.md.tmpl (deliveryMode
#                                      "stub": a pointer at TICK.md, not a
#                                      full inline copy -- see that key's
#                                      own $comment in harnesses.json).
#
# ★ PERMISSION VOCABULARY, verified during Stage 5 rather than assumed:
# the bundled @opencode-ai/sdk (.opencode/node_modules/@opencode-ai/sdk)
# ships TWO generated API surfaces. The default export
# (dist/gen/types.gen.d.ts) types AgentConfig.permission as only
# {edit, bash, webfetch, doom_loop, external_directory} -- NO "task" key
# at all. The v2 export (dist/v2/gen/types.gen.d.ts) types it as
# PermissionConfig: {read, edit, glob, grep, list, bash, task,
# external_directory, todowrite, question, webfetch, websearch,
# codesearch, lsp, doom_loop, skill}, each an "allow"|"ask"|"deny" (or,
# for the Rule-shaped ones, a per-pattern map). Stage 0 probe 3 measured
# permission:{bash:deny,task:deny} actually enforced at tool-execution
# time -- that only typechecks against the v2 shape, so v2 is what this
# running app actually honours, and the user's own (now-deleted)
# ~/.config/opencode/opencode.jsonc independently confirms the same
# vocabulary (edit/bash/task, no "write" key, "edit" alone gates every
# file mutation). harnesses.json's opencode.permissionByRole carries the
# per-ROLE correction this implies -- see its own $comment.

function Get-OpenCodeAgentEntry {
    <#
      One entry under .opencode/opencode.json's "agent" map. $Permission
      is a PSCustomObject (harnesses.json's permissionByRole.worker or
      .reviewer, read as-is so the emitted keys can never drift from what
      that file declares) rather than a fixed parameter list, so a future
      permission key added there needs no change here.
    #>
    param(
        [string]$Name,
        [string]$Description,
        [string]$Model,
        [PSCustomObject]$Permission,
        [string]$PromptRelativePath
    )
    $permLines = [System.Collections.Generic.List[string]]::new()
    foreach ($prop in $Permission.PSObject.Properties) {
        if ($prop.Name.StartsWith('$')) { continue }
        $permLines.Add("        `"$($prop.Name)`": `"$($prop.Value)`"")
    }
    $permBlock = $permLines -join ",`n"
    $descriptionEscaped = $Description.Replace('\', '\\').Replace('"', '\"')
    return @"
    "$Name": {
      "description": "$descriptionEscaped",
      "mode": "subagent",
      "model": "$Model",
      "permission": {
$permBlock
      },
      "prompt": "{file:$PromptRelativePath}"
    }
"@
}

function Get-OpenCodeArtifacts {
    <#
      Returns the same {Artifacts, ToRemove} shape as Get-ClaudeArtifacts,
      so -Check and the emit loop below need no per-harness branching.
      Pure -- touches no disk beyond the two Test-Path reads below, which
      only guard a clear error message; they do not feed the generated
      content.
    #>
    param($HarnessProfile)

    $artifacts = [ordered]@{}

    # bodyDelivery "reference" (harnesses.json) -- point {file:...} at the
    # ALREADY-GENERATED resolved charters rather than re-deriving them
    # here. They are Get-ClaudeArtifacts's output (Tools/zagent/protocol/
    # roles/generated/{worker,reviewer}.resolved.md), generated whenever
    # `-Harness claude` (the default) runs -- which every workflow in
    # this repo's own docs runs before asking for another harness. Guard
    # with a clear error rather than silently emitting a {file:...} that
    # would 404 inside OpenCode's own config loader.
    $resolvedWorkerPath = Join-Path $generatedRolesDir 'worker.resolved.md'
    $resolvedReviewerPath = Join-Path $generatedRolesDir 'reviewer.resolved.md'
    foreach ($pair in @(
            @{ Path = $resolvedWorkerPath; Name = 'worker.resolved.md' },
            @{ Path = $resolvedReviewerPath; Name = 'reviewer.resolved.md' }
        )) {
        if (-not (Test-Path -LiteralPath $pair.Path)) {
            throw "Build-HarnessArtifacts -Harness opencode: Tools/zagent/protocol/roles/generated/$($pair.Name) does not exist. OpenCode's bodyDelivery is 'reference' -- it points {file:...} at this already-generated file rather than embedding a copy. Run 'pwsh -NoProfile -File Tools/zagent/Build-HarnessArtifacts.ps1' (the default -Harness claude) first; that is what generates it."
        }
    }

    $permissionByRole = $harnessesConfig.harnesses.opencode.permissionByRole
    $workerPermission = $permissionByRole.worker
    $reviewerPermission = $permissionByRole.reviewer

    # ── the "agent" map: five entries, none named haiku (roles.worker /
    #    roles.reviewerByRisk are the only source iterated -- exactly the
    #    same enforcement-by-construction Get-ClaudeArtifacts relies on).
    $agentEntries = [ordered]@{}

    foreach ($tierProp in $HarnessProfile.roles.worker.PSObject.Properties) {
        $tier = $tierProp.Name
        $model = $tierProp.Value.model
        $description = "The AUTHORING half of a /tick, routed at the $tier tier. Implements one ticket's file edits from a spec inlined into its prompt. It cannot build, test, run gates, commit, or touch the board by construction (permission.bash/task = deny)."
        $name = "zagent-worker-$tier"
        $agentEntries[$name] = Get-OpenCodeAgentEntry -Name $name -Description $description -Model $model `
            -Permission $workerPermission -PromptRelativePath '../Tools/zagent/protocol/roles/generated/worker.resolved.md'
    }

    foreach ($riskProp in $HarnessProfile.roles.reviewerByRisk.PSObject.Properties) {
        $risk = $riskProp.Name
        $model = $riskProp.Value.model
        $riskLower = $risk.ToLowerInvariant()
        $description = "The REVIEW half of a /tick, spawned when a ticket's review is required — the OR of its risk being in reviewOn and a scan of its Definition of Done for claims a gate cannot settle — routed at the $risk-risk tier. Reads an inlined diff and reports findings. Read-only by construction (permission.edit/bash/task = deny), so it cannot edit, build, or run anything."
        $name = "zagent-reviewer-$riskLower"
        $agentEntries[$name] = Get-OpenCodeAgentEntry -Name $name -Description $description -Model $model `
            -Permission $reviewerPermission -PromptRelativePath '../Tools/zagent/protocol/roles/generated/reviewer.resolved.md'
    }

    $agentBlock = ($agentEntries.Values -join ",`n")

    $subagentDepth = $harnessesConfig.harnesses.opencode.subagentDepth
    $header = Get-GeneratedHeaderComment -SourceDescription 'Tools/zagent/protocol/harnesses.json (opencode.roles, permissionByRole, subagentDepth) + Tools/zagent/protocol/roles/generated/{worker,reviewer}.resolved.md'
    # JSONC: the "GENERATED" header and the subagent_depth rationale are
    # both legal `//` comments here -- Stage 0 probe 9 (measured): every
    # candidate config path, INCLUDING the ".opencode/opencode.json"
    # spelling despite its ".json" extension, goes through the same
    # JSONC-tolerant parser.
    $headerAsLineComments = ($header -split "`n" | Where-Object { $_ -notin @('<!--', '-->') } | ForEach-Object { "// $_" }) -join "`n"

    $opencodeJson = @"
$headerAsLineComments
{
  "`$schema": "https://opencode.ai/config.json",

  // subagent_depth: 1 is already the smallest value that lets a depth-0
  // orchestrator (this session, driving /tick) dispatch a depth-1 WORKER
  // or REVIEWER subagent -- Stage 0 probe 10 measured the default IS
  // enforced (walks parentID) and that a spawned child gets an
  // auto-added task:deny permission, so raising it would only widen what
  // a worker or reviewer could itself go on to spawn. Ship the default;
  // do not raise it (harnesses.json opencode.`$comment_subagentDepth).
  "subagent_depth": $subagentDepth,

  "agent": {
$agentBlock
  }
}
"@
    $artifacts['.opencode/opencode.json'] = ConvertTo-Crlf $opencodeJson

    # ── the entry point, from entrypoint.md.tmpl. The body-vs-pointer
    #    choice READS $HarnessProfile.deliveryMode rather than being hard-
    #    coded, so flipping that one key in harnesses.json actually changes
    #    what is emitted. It was hard-coded to the "stub" shape, which made
    #    the key a comment rather than a control: harnesses.json could say
    #    "inline" while this emitter kept writing a pointer, and nothing
    #    would have reported the disagreement.
    $tmplPath = Join-Path $protocolDir 'entrypoint.md.tmpl'
    $tmpl = Read-RawText $tmplPath

    $controlCell = {
        param($Value)
        if ($null -eq $Value) { return '*absent — no mechanism on this harness*' }
        return $Value
    }

    # deliveryMode decides whether the protocol body is copied in whole or
    # merely pointed at. "stub" makes the worker's own read tool responsible
    # for pulling ~1950 lines in one go -- and the 2000-line figure that
    # bounds that is CLAUDE CODE'S Read default, never measured for OpenCode.
    # "inline" removes that unmeasured dependency at the cost of a bigger
    # generated file. Test-HarnessParity.ps1's headroom assertion binds only
    # while some harness is "stub", so flipping this back re-arms it.
    $tickBodyPointer = if ($HarnessProfile.deliveryMode -eq 'inline') {
        Read-RawText (Join-Path $protocolDir 'TICK.md')
    } elseif ($HarnessProfile.deliveryMode -eq 'stub') {
        "Before doing anything else, read [``Tools/zagent/protocol/TICK.md``](../../Tools/zagent/protocol/TICK.md) **in full** — that file *is* the protocol body; nothing here substitutes for it, and OpenCode's own `{file:...}` inclusion is not used for this pointer (deliveryMode: ``stub``, harnesses.json). Come back to this file only for the harness-specific resolution below."
    } else {
        throw "Build-HarnessArtifacts: harnesses.json opencode.deliveryMode is '$($HarnessProfile.deliveryMode)' -- expected 'inline' or 'stub'."
    }

    $humanPrereqText = if ($HarnessProfile.humanPrerequisites.Count -eq 0) {
        '*none recorded*'
    } else {
        (($HarnessProfile.humanPrerequisites | ForEach-Object { "- $_" }) -join "`n")
    }

    $entryBody = $tmpl
    $entryBody = $entryBody.Replace('{{HARNESS_NAME}}', 'OpenCode')
    $entryBody = $entryBody.Replace('{{TICK_BODY_OR_POINTER}}', $tickBodyPointer)
    $entryBody = $entryBody.Replace('{{HARNESS_ID}}', 'opencode')
    $entryBody = $entryBody.Replace('{{CONTROL_DISPATCH}}', (& $controlCell $HarnessProfile.controls.DISPATCH))
    $entryBody = $entryBody.Replace('{{CONTROL_AWAIT}}', (& $controlCell $HarnessProfile.controls.AWAIT))
    $entryBody = $entryBody.Replace('{{CONTROL_ABORT}}', (& $controlCell $HarnessProfile.controls.ABORT))
    $entryBody = $entryBody.Replace('{{CONTROL_SCHEDULE}}', (& $controlCell $HarnessProfile.controls.SCHEDULE))
    $entryBody = $entryBody.Replace('{{CONTROL_STOP}}', (& $controlCell $HarnessProfile.controls.STOP))
    $entryBody = $entryBody.Replace('{{LOOP_MODE}}', $HarnessProfile.loopMode)
    $entryBody = $entryBody.Replace('{{ROLE_WORKER_SONNET}}', '`.opencode/opencode.json` -> `agent."zagent-worker-sonnet"`')
    $entryBody = $entryBody.Replace('{{ROLE_WORKER_OPUS}}', '`.opencode/opencode.json` -> `agent."zagent-worker-opus"`')
    $entryBody = $entryBody.Replace('{{ROLE_REVIEWER_LOW}}', '`.opencode/opencode.json` -> `agent."zagent-reviewer-low"`')
    $entryBody = $entryBody.Replace('{{ROLE_REVIEWER_MEDIUM}}', '`.opencode/opencode.json` -> `agent."zagent-reviewer-medium"`')
    $entryBody = $entryBody.Replace('{{ROLE_REVIEWER_HIGH}}', '`.opencode/opencode.json` -> `agent."zagent-reviewer-high"`')
    $entryBody = $entryBody.Replace('{{WORKER_TOOL_DENIAL}}', $HarnessProfile.workerToolDenial)
    $entryBody = $entryBody.Replace('{{REVIEW_MODE}}', $HarnessProfile.review.mode)
    $entryBody = $entryBody.Replace('{{PREAUTH_KIND}}', $HarnessProfile.preauth.kind)
    $entryBody = $entryBody.Replace('{{PREAUTH_SOURCES}}', ($HarnessProfile.preauth.sources -join ', '))
    $entryBody = $entryBody.Replace('{{HUMAN_PREREQUISITES}}', $humanPrereqText)

    $entryFm = @(
        '---'
        'description: Run one agent ticket — the queue head, or a ticket you name'
        '---'
    ) -join "`n"
    $entryHeader = Get-GeneratedHeaderComment -SourceDescription "Tools/zagent/protocol/TICK.md (body) + Tools/zagent/protocol/entrypoint.md.tmpl + Tools/zagent/protocol/harnesses.json (opencode entry point profile, deliveryMode: $($HarnessProfile.deliveryMode))"
    $entryRaw = "$entryFm`n`n$entryHeader`n`n$entryBody"
    $artifacts['.opencode/commands/tick.md'] = ConvertTo-Crlf $entryRaw

    return [PSCustomObject]@{
        Artifacts = $artifacts
        ToRemove  = @()
    }
}


# ═══════════════════════════════════════════════════════════════════════
#  CODEX emitter
# ═══════════════════════════════════════════════════════════════════════
#
# Stage 6. Five agent TOML files (2 worker tiers + 3 reviewer risks, no
# haiku worker -- iterating roles.worker/roles.reviewerByRisk is the same
# enforcement-by-construction the other two emitters rely on), the
# .codex/skills/tick/SKILL.md entry point, and .codex/config.toml (the
# pre-authorisation layer).
#
# ★ roleDir is PLURAL (.codex/agents), per Stage 0 probe 5 (MEASURED):
# unlike OpenCode, Codex accepts only the plural spelling.
#
# ★ AgentRoleToml key set, MEASURED against the running app's own bundle
# (app.asar), not guessed: a file dropped directly under .codex/agents/
# (not referenced from config.toml) is parsed with a `.loose()` Zod schema
# validating exactly {name, description, nickname_candidates} and passing
# every OTHER key through unvalidated by that listing pass -- which is
# where model / sandbox_mode / developer_instructions live, the same
# "normal Config field a config_file layer inherits" shape Stage 0 probe 2
# already found for model_reasoning_effort. ★★ `name` IS REQUIRED IN
# PRACTICE for a directory-discovered file: the scanner passes
# roleNameHint: null for anything found by walking .codex/agents/ (only a
# config.toml `[agents.X]` entry gets a hint, from its own table key), and
# the parser drops the whole agent silently if `name` is absent and no
# hint exists (`i.data.name?.trim()||t?.trim()||""` -> empty -> null).
# Every emitted file below sets `name` explicitly for exactly this reason.
#
# ★ deliveryMode "inline" (harnesses.json): Codex has no {file:...}-
# equivalent reference mechanism, so SKILL.md carries TICK.md's full body,
# the same shape as Claude's .claude/commands/tick.md -- not OpenCode's
# short pointer. entrypoint.md.tmpl's {{TICK_BODY_OR_POINTER}} slot is
# filled with the whole file rather than a sentence.

function ConvertTo-TomlBasicString {
    <#
      TOML basic (double-quoted) string escaping: backslash first, then
      the quote itself -- order matters, escaping the quote first would
      double-escape the backslash just inserted.
    #>
    param([string]$Text)
    $escaped = $Text.Replace('\', '\\').Replace('"', '\"')
    return '"' + $escaped + '"'
}

function New-CodexAgentToml {
    <#
      One .codex/agents/*.toml file. developer_instructions is a TOML
      multi-line LITERAL string ('''...''') -- see item 4 of the Stage 6
      brief and this script's own top-of-file ''' guard, which already
      validates roles/worker.md and roles/reviewer.md on every run
      regardless of -Harness. That guard checks the RAW charter files;
      neither {{PROTECTED_PATHS}} substitution nor the "declared" paragraph
      Get-SubstitutedWorkerBody appends can introduce a literal ''', so no
      second check is needed here.
    #>
    param(
        [string]$Name,
        [string]$Description,
        [string]$Model,
        [string]$SandboxMode,
        [string]$SourceDescription,
        [string]$Body
    )
    $header = @(
        "# GENERATED — DO NOT EDIT."
        "# Source: $SourceDescription"
        "# Regenerate: pwsh -NoProfile -File Tools/zagent/Build-HarnessArtifacts.ps1 -Harness codex"
    ) -join "`n"
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add($header)
    $lines.Add('')
    $lines.Add('name = ' + (ConvertTo-TomlBasicString $Name))
    $lines.Add('description = ' + (ConvertTo-TomlBasicString $Description))
    $lines.Add('model = ' + (ConvertTo-TomlBasicString $Model))
    $lines.Add('sandbox_mode = ' + (ConvertTo-TomlBasicString $SandboxMode))
    $lines.Add('')
    $lines.Add("developer_instructions = '''")
    $lines.Add($Body.TrimEnd())
    $lines.Add("'''")
    $raw = ($lines -join "`n") + "`n"
    return ConvertTo-Crlf $raw
}

function Get-CodexArtifacts {
    <#
      Returns the same {Artifacts, ToRemove} shape as the other two
      emitters, so -Check and the emit loop need no per-harness branching.
    #>
    param($HarnessProfile)

    $artifacts = [ordered]@{}

    # Reuses Get-SubstitutedWorkerBody exactly as the claude emitter does --
    # it is already generic over $HarnessProfile.workerToolDenial, and
    # codex's is "declared", so the same "you have a shell and must not use
    # it" paragraph this function appends for claude-shaped "declared"
    # profiles applies here unchanged.
    $workerBody = Get-SubstitutedWorkerBody -HarnessProfile $HarnessProfile
    $reviewerBody = $reviewerCharter

    $workerSource = 'Tools/zagent/protocol/roles/worker.md + Tools/zagent/protocol/harnesses.json (codex.roles.worker) + vault.config.json (policy.resolved.guardrails.protectedPaths)'
    $reviewerSource = 'Tools/zagent/protocol/roles/reviewer.md + Tools/zagent/protocol/harnesses.json (codex.roles.reviewerByRisk)'

    # ★ Hardcoded, not read from harnesses.json -- see that file's own
    # $comment_reviewerSandbox. sandbox_mode has exactly two legal values
    # and read-only would break the worker's entire job, so there is
    # nothing to choose between and nothing worth parking as data.
    $workerSandboxMode = 'workspace-write'
    $reviewerSandboxMode = $HarnessProfile.reviewerSandbox

    # ── Worker role files, one per routed tier. haiku is absent from
    #    roles.worker (unusedTiers) and generates nothing -- this loop can
    #    only ever iterate what roles.worker actually has.
    foreach ($tierProp in $HarnessProfile.roles.worker.PSObject.Properties) {
        $tier = $tierProp.Name
        $model = $tierProp.Value.model
        $name = "zagent-worker-$tier"
        $description = "The AUTHORING half of a /tick, routed at the $tier tier. Implements one ticket's file edits from a spec inlined into its prompt. It cannot build, test, run gates, commit, or touch the board by role convention alone -- this harness does not structurally deny it (workerToolDenial: declared; see WORKER-MOVED-NOTHING at TICK.md step 6.4)."
        $content = New-CodexAgentToml -Name $name -Description $description -Model $model -SandboxMode $workerSandboxMode `
            -SourceDescription $workerSource -Body $workerBody
        $artifacts[".codex/agents/$name.toml"] = $content
    }

    # ── Reviewer role files, one per risk -- ALL THREE, same reasoning as
    #    the other two emitters (review.required is the OR of reviewOn and
    #    the DoD-assertion scan, so even LOW needs a reviewer model).
    foreach ($riskProp in $HarnessProfile.roles.reviewerByRisk.PSObject.Properties) {
        $risk = $riskProp.Name
        $model = $riskProp.Value.model
        $riskLower = $risk.ToLowerInvariant()
        $name = "zagent-reviewer-$riskLower"
        $description = "The REVIEW half of a /tick, spawned when a ticket's review is required — the OR of its risk being in reviewOn and a scan of its Definition of Done for claims a gate cannot settle — routed at the $risk-risk tier. Reads an inlined diff and reports findings. Read-only by construction (sandbox_mode = read-only), so it cannot edit, build, or run anything."
        $content = New-CodexAgentToml -Name $name -Description $description -Model $model -SandboxMode $reviewerSandboxMode `
            -SourceDescription $reviewerSource -Body $reviewerBody
        $artifacts[".codex/agents/$name.toml"] = $content
    }

    # ── The entry point: entrypoint.md.tmpl with deliveryMode "inline" --
    #    TICK.md's full body substituted in, wrapped in the SKILL.md
    #    frontmatter shape measured from the two bundled curated skills
    #    under Codex's own app.asar (name + description, nothing else).
    $tmplPath = Join-Path $protocolDir 'entrypoint.md.tmpl'
    $tmpl = Read-RawText $tmplPath
    $tickBody = Read-RawText (Join-Path $protocolDir 'TICK.md')

    $controlCell = {
        param($Value)
        if ($null -eq $Value) { return '*absent — no mechanism on this harness*' }
        return $Value
    }

    $humanPrereqText = if ($HarnessProfile.humanPrerequisites.Count -eq 0) {
        '*none recorded*'
    } else {
        (($HarnessProfile.humanPrerequisites | ForEach-Object { "- $_" }) -join "`n")
    }

    $entryBody = $tmpl
    $entryBody = $entryBody.Replace('{{HARNESS_NAME}}', 'Codex')
    $entryBody = $entryBody.Replace('{{TICK_BODY_OR_POINTER}}', $tickBody)
    $entryBody = $entryBody.Replace('{{HARNESS_ID}}', 'codex')
    $entryBody = $entryBody.Replace('{{CONTROL_DISPATCH}}', (& $controlCell $HarnessProfile.controls.DISPATCH))
    $entryBody = $entryBody.Replace('{{CONTROL_AWAIT}}', (& $controlCell $HarnessProfile.controls.AWAIT))
    $entryBody = $entryBody.Replace('{{CONTROL_ABORT}}', (& $controlCell $HarnessProfile.controls.ABORT))
    $entryBody = $entryBody.Replace('{{CONTROL_SCHEDULE}}', (& $controlCell $HarnessProfile.controls.SCHEDULE))
    $entryBody = $entryBody.Replace('{{CONTROL_STOP}}', (& $controlCell $HarnessProfile.controls.STOP))
    $entryBody = $entryBody.Replace('{{LOOP_MODE}}', $HarnessProfile.loopMode)
    $entryBody = $entryBody.Replace('{{ROLE_WORKER_SONNET}}', '`.codex/agents/zagent-worker-sonnet.toml`')
    $entryBody = $entryBody.Replace('{{ROLE_WORKER_OPUS}}', '`.codex/agents/zagent-worker-opus.toml`')
    $entryBody = $entryBody.Replace('{{ROLE_REVIEWER_LOW}}', '`.codex/agents/zagent-reviewer-low.toml`')
    $entryBody = $entryBody.Replace('{{ROLE_REVIEWER_MEDIUM}}', '`.codex/agents/zagent-reviewer-medium.toml`')
    $entryBody = $entryBody.Replace('{{ROLE_REVIEWER_HIGH}}', '`.codex/agents/zagent-reviewer-high.toml`')
    $entryBody = $entryBody.Replace('{{WORKER_TOOL_DENIAL}}', $HarnessProfile.workerToolDenial)
    $entryBody = $entryBody.Replace('{{REVIEW_MODE}}', $HarnessProfile.review.mode)
    $entryBody = $entryBody.Replace('{{PREAUTH_KIND}}', $HarnessProfile.preauth.kind)
    $entryBody = $entryBody.Replace('{{PREAUTH_SOURCES}}', ($HarnessProfile.preauth.sources -join ', '))
    $entryBody = $entryBody.Replace('{{HUMAN_PREREQUISITES}}', $humanPrereqText)

    $entryFm = @(
        '---'
        'name: tick'
        'description: Run one agent ticket — the queue head, or a ticket you name'
        '---'
    ) -join "`n"
    $entryHeader = Get-GeneratedHeaderComment -SourceDescription 'Tools/zagent/protocol/TICK.md (body) + Tools/zagent/protocol/entrypoint.md.tmpl + Tools/zagent/protocol/harnesses.json (codex entry point profile, deliveryMode: inline)'
    $entryRaw = "$entryFm`n`n$entryHeader`n`n$entryBody"
    $artifacts['.codex/skills/tick/SKILL.md'] = ConvertTo-Crlf $entryRaw

    # ── The pre-authorisation layer. Comments inside the file itself carry
    #    the same honesty the brief asks for in harnesses.json's own
    #    $comment_preauth: strongly evidenced as a real config layer, not
    #    behaviourally confirmed; trust_level is machine-local and absent
    #    on purpose; the Windows elevated sandbox enforces nothing today.
    $configToml = @'
# GENERATED — DO NOT EDIT.
# Source: Tools/zagent/protocol/harnesses.json (codex.preauth)
# Regenerate: pwsh -NoProfile -File Tools/zagent/Build-HarnessArtifacts.ps1 -Harness codex
#
# The pre-authorisation layer for an unattended /tick on Codex. What is and
# is not confirmed about it (stage0-verdicts.md, probes 1 and 6):
#
#   - A repo-level .codex/config.toml as a real project config layer is
#     STRONGLY EVIDENCED (the app's own UI offers to edit "the project's
#     .codex/config.toml", and a TOML parse + layered config resolver
#     follow immediately) but was NOT behaviourally confirmed end to end --
#     the confirming run was blocked by a usage cap. A parity test can
#     assert this file exists and parses; it cannot assert Codex actually
#     loads and applies it without a live, unmetered run.
#   - trust_level -- the setting that actually relaxes approvals for a
#     project -- lives ONLY in the machine-local ~/.codex/config.toml under
#     [projects.'<path>'] and is NOT repo-committable. A human sets it on
#     every machine that runs this harness (codex.humanPrerequisites).
#   - [windows] sandbox = "elevated" parses to a real config enum, but its
#     backing feature is disabled in this build, so it enforces NOTHING
#     today -- recorded here rather than left as a silent no-op a reader
#     could mistake for real protection.

approval_policy = "never"

[windows]
sandbox = "elevated"
'@
    $artifacts['.codex/config.toml'] = ConvertTo-Crlf $configToml

    return [PSCustomObject]@{
        Artifacts = $artifacts
        ToRemove  = @()
    }
}


$plan = switch ($Harness) {
    'claude' { Get-ClaudeArtifacts -HarnessProfile $harnessProfile }
    'opencode' { Get-OpenCodeArtifacts -HarnessProfile $harnessProfile }
    'codex' { Get-CodexArtifacts -HarnessProfile $harnessProfile }
    default { throw "Build-HarnessArtifacts: unreachable -- harness '$Harness' should have been rejected above." }
}

# ═══════════════════════════════════════════════════════════════════════
#  -Check: compare in memory, touch nothing
# ═══════════════════════════════════════════════════════════════════════

if ($Check) {
    $stale = [System.Collections.Generic.List[string]]::new()

    foreach ($relPath in $plan.Artifacts.Keys) {
        $fullPath = Join-Path $repoRoot $relPath
        $expected = $plan.Artifacts[$relPath]
        if (-not (Test-Path -LiteralPath $fullPath)) {
            Write-Host "STALE (missing) $relPath"
            $stale.Add($relPath)
            continue
        }
        $actual = [System.IO.File]::ReadAllText($fullPath)
        if ($actual -ne $expected) {
            Write-Host "STALE (content differs) $relPath"
            $stale.Add($relPath)
        }
    }

    foreach ($relPath in $plan.ToRemove) {
        $fullPath = Join-Path $repoRoot $relPath
        if (Test-Path -LiteralPath $fullPath) {
            Write-Host "STALE (should be removed) $relPath"
            $stale.Add($relPath)
        }
    }

    if ($stale.Count -eq 0) {
        Write-Host "Build-HarnessArtifacts -Check: up to date ($($plan.Artifacts.Count) artifact(s) for harness '$Harness')."
        exit 0
    } else {
        Write-Host ""
        Write-Host "Build-HarnessArtifacts -Check: $($stale.Count) stale path(s):"
        foreach ($p in $stale) { Write-Host "  $p" }
        exit 1
    }
}

# ═══════════════════════════════════════════════════════════════════════
#  Emit
# ═══════════════════════════════════════════════════════════════════════

foreach ($relPath in $plan.Artifacts.Keys) {
    $fullPath = Join-Path $repoRoot $relPath
    Write-Utf8NoBom -Path $fullPath -Content $plan.Artifacts[$relPath]
    Write-Host "wrote $relPath"
}

foreach ($relPath in $plan.ToRemove) {
    $fullPath = Join-Path $repoRoot $relPath
    if (Test-Path -LiteralPath $fullPath) {
        Push-Location $repoRoot
        try {
            & git rm --quiet -- $relPath 2>&1 | Out-Null
            if ($LASTEXITCODE -ne 0) {
                # Fall back to a plain delete -- e.g. the file exists but was
                # never tracked. Never leave a superseded file behind just
                # because git did not know about it.
                Remove-Item -LiteralPath $fullPath -Force
            }
        } finally {
            Pop-Location
        }
        Write-Host "removed $relPath"
    }
}

Write-Host ""
Write-Host "Build-HarnessArtifacts: generated $($plan.Artifacts.Count) artifact(s) for harness '$Harness'."
exit 0
