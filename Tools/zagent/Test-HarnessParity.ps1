#Requires -Version 7.0
<#
  Test-HarnessParity.ps1 — checks that every harness's entry point
  pre-authorises the same gate lines and mandated commands `/tick` needs,
  and that the three harnesses (Claude Code, OpenCode, Codex) stay in
  parity with each other and with the canonical, harness-neutral
  Tools/zagent/protocol/TICK.md.

  ── Why this check exists at all (moved out of TICK.md's own prose,
     unchanged, because it is measured history the protocol used to carry
     as a standing rule rather than a script) ──

  ★★ AND IT IS NOT ONLY GATE LINES — IT IS EVERY COMMAND AND EVERY TOOL
  THIS FILE MANDATES. Two were missing when this rule was audited, both
  added by a fix that did not think to widen the list it had just moved
  outside:

  - Step 7 once mandated `timeout 120 zagent docs sync --json`. `Bash(zagent:*)`
    is anchored on `zagent` as the FIRST token, so it matched nothing —
    and the stall lands *after* the commit and *before* `finish`, which is
    the one place this file says the queue must never stop. `Bash(timeout *)`
    went on the list then and stays there, though **step 7 no longer wraps
    that command**: the underlying hang was a per-page WebSocket and is fixed
    (upstream `05408ef`). Kept as the worked example, because the rule it
    illustrates is untouched — a mandated command whose first token is not
    prefix-matched stalls the loop on a permission prompt.
  - Step 5 mandates `TaskStop` for the wedged-worker guardrail, and it was
    not granted at all: the only enforcement action for a loop that is
    already stuck needed a tool the skill never listed.

  When you add a mandated command to this file, add its prefix above in the
  SAME edit. The check is mechanical: every backticked command in the steps
  below should be prefix-matched by an entry in `allowed-tools`.

  ── End moved prose ──

  A plain test script rather than Pester, matching
  Tools/zagent/Test-ZagentClient.ps1 and
  Tools/Test_T0Harness_SessionCloseFlagsExist.ps1: the only PowerShell this
  repo requires is the pwsh every gate already needs.

  Usage:
    pwsh -NoProfile -File Tools/zagent/Test-HarnessParity.ps1

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

$here = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $here '..\..')).Path
$protocolDir = Join-Path $repoRoot 'Tools/zagent/protocol'
$rolesDir = Join-Path $protocolDir 'roles'
$generatedRolesDir = Join-Path $rolesDir 'generated'

Write-Host "`n=== self: this script itself parses ===" -ForegroundColor Cyan
Assert-That 'Test-HarnessParity.ps1 parses cleanly' {
    $errors = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        (Join-Path $here 'Test-HarnessParity.ps1'), [ref]$null, [ref]$errors)
    $null -eq $errors -or $errors.Count -eq 0
}

Assert-That 'Tools/zagent/protocol/TICK.md exists' {
    Test-Path (Join-Path $protocolDir 'TICK.md')
}

# ─── Shared helpers ─────────────────────────────────────────────────────

function Test-HasProp {
    <#
      True only when $Object DECLARES $Name as a property, whether or not
      its value is null. Distinguishes "declared explicitly as null" (a
      harness with no ABORT mechanism, say) from "not declared at all" (a
      schema hole a `??` fallback would silently paper over) -- assertion 8's
      whole point.
    #>
    param($Object, [string]$Name)
    if ($null -eq $Object) { return $false }
    return $null -ne $Object.PSObject.Properties[$Name]
}

function Get-PropNames {
    param($Object)
    if ($null -eq $Object) { return @() }
    return @($Object.PSObject.Properties.Name)
}

function ConvertTo-CrlfIndependent {
    <#
      Reimplemented independently of Build-HarnessArtifacts.ps1's own
      ConvertTo-Crlf (same algorithm, separate code) so assertion 1 does not
      depend on the generator's own line-ending logic being correct -- it
      is checking the generator's OUTPUT against an independently-derived
      expectation, not calling back into the generator.
    #>
    param([string]$Text)
    $lfOnly = $Text -replace "`r`n", "`n"
    return $lfOnly -replace "`n", "`r`n"
}

function Format-ProtectedPathsListIndependent {
    param([string[]]$Paths)
    $backticked = @($Paths | ForEach-Object { "``$_``" })
    if ($backticked.Count -eq 1) { return $backticked[0] }
    if ($backticked.Count -eq 2) { return "$($backticked[0]) or $($backticked[1])" }
    $allButLast = $backticked[0..($backticked.Count - 2)] -join ', '
    return "$allButLast, or $($backticked[-1])"
}

function Get-CommandPrefixRoot {
    <#
      Normalises one declared pre-authorisation entry -- `Bash(git:*)`,
      `Bash(pwsh -NoProfile*)`, `PowerShell(git *)`, ... -- down to the bare
      command-prefix text it grants: "git", "pwsh -NoProfile". The three
      spellings this repo's own settings actually use (`cmd:*`, `cmd *`,
      `cmd*`) all reduce to the same root, which is what "coverage by a
      broad prefix counts" means in practice: a claim payload never cares
      which of the three spellings a tool's own permission DSL prefers.
    #>
    param([string]$Entry)
    $inner = $Entry
    if ($inner -match '^(?:Bash|PowerShell)\((.*)\)$') { $inner = $Matches[1] }
    if ($inner.EndsWith('*')) { $inner = $inner.Substring(0, $inner.Length - 1) }
    $inner = $inner.TrimEnd(':', ' ')
    return $inner
}

function Test-CommandCovered {
    param([string]$Command, [string[]]$PrefixRoots)
    $cmd = $Command.TrimStart()
    foreach ($root in $PrefixRoots) {
        if ([string]::IsNullOrEmpty($root)) { continue }
        if ($cmd.Length -eq $root.Length) {
            if ($cmd.Equals($root, [StringComparison]::OrdinalIgnoreCase)) { return $true }
            continue
        }
        if ($cmd.Length -gt $root.Length -and $cmd.Substring(0, $root.Length).Equals($root, [StringComparison]::OrdinalIgnoreCase)) {
            $nextChar = $cmd[$root.Length]
            if ($nextChar -eq ' ' -or $nextChar -eq '"') { return $true }
        }
    }
    return $false
}

function Get-GithubSlug {
    <#
      GitHub's own heading-anchor algorithm: lowercase, strip everything
      that is not a letter/number/space/hyphen/underscore (Unicode-aware,
      so a leading ★★ disappears rather than becoming a stray hyphen),
      then collapse whitespace runs to single hyphens.
    #>
    param([string]$Heading)
    $h = $Heading.ToLowerInvariant()
    $h = $h -replace '[^\p{L}\p{N}\s_-]', ''
    $h = $h.Trim() -replace '\s+', '-'
    return $h
}

# ─── Load canonical sources ─────────────────────────────────────────────

$harnessesConfig = Get-Content -Raw (Join-Path $protocolDir 'harnesses.json') | ConvertFrom-Json
$vaultConfig = Get-Content -Raw (Join-Path $repoRoot 'vault.config.json') | ConvertFrom-Json
$projectConfig = Get-Content -Raw (Join-Path $repoRoot 'zagent.project.json') | ConvertFrom-Json

$tickPath = Join-Path $protocolDir 'TICK.md'
$tickLines = @(Get-Content -LiteralPath $tickPath)
$tickText = Get-Content -Raw -LiteralPath $tickPath

$workerCharterPath = Join-Path $rolesDir 'worker.md'
$reviewerCharterPath = Join-Path $rolesDir 'reviewer.md'
$workerCharterRaw = Get-Content -Raw -LiteralPath $workerCharterPath
$reviewerCharterRaw = Get-Content -Raw -LiteralPath $reviewerCharterPath

$routingPath = Join-Path $repoRoot 'Tools/Vault/cli/legacy-pure/routing.mjs'
$routingText = Get-Content -Raw -LiteralPath $routingPath
$compatPath = Join-Path $repoRoot 'Tools/Vault/cli/compat.mjs'
$compatText = Get-Content -Raw -LiteralPath $compatPath

$buildArtifactsPath = Join-Path $repoRoot 'Tools/zagent/Build-HarnessArtifacts.ps1'
$buildArtifactsText = Get-Content -Raw -LiteralPath $buildArtifactsPath

$harnessNames = @($harnessesConfig.harnesses.PSObject.Properties.Name)

$protectedPaths = @($vaultConfig.policy.resolved.guardrails.protectedPaths)

# ═════════════════════════════════════════════════════════════════════════
#  No duplication
# ═════════════════════════════════════════════════════════════════════════

Write-Host "`n=== 1. embedded role bodies match the canonical charter ===" -ForegroundColor Cyan

# The one paragraph a "declared" workerToolDenial harness appends to its
# WORKER body (item 5 of the Stage 3 brief). Extracted from
# Build-HarnessArtifacts.ps1's own source rather than re-typed here, so this
# assertion cannot silently drift from what the generator actually emits --
# if that literal ever changes, this regex either still finds the (new)
# literal or throws loudly rather than comparing against a stale copy.
if ($buildArtifactsText -notmatch '\$declaredParagraph\s*=\s*("(?:[^"`]|`.)*")') {
    throw "Test-HarnessParity: could not locate `$declaredParagraph in Build-HarnessArtifacts.ps1 -- assertion 1's 'declared' accounting depends on it existing verbatim."
}
$declaredParagraph = Invoke-Expression $Matches[1]

$protectedPathsText = Format-ProtectedPathsListIndependent -Paths $protectedPaths
$expectedWorkerBodyBase = $workerCharterRaw.Replace('{{PROTECTED_PATHS}}', $protectedPathsText)

function Get-ExpectedWorkerBody {
    param([string]$WorkerToolDenial)
    $body = $expectedWorkerBodyBase
    if ($WorkerToolDenial -eq 'declared') {
        $body = $body.TrimEnd() + "`n`n" + $declaredParagraph + "`n"
    }
    return ConvertTo-CrlfIndependent $body
}
$expectedReviewerBody = ConvertTo-CrlfIndependent $reviewerCharterRaw

function Get-EmbeddedBodyAfterHtmlComment {
    <#
      For Claude's .md agent files and both harnesses' entry points built
      the same way: front-matter, then a "<!-- GENERATED ... -->" comment,
      then the body. Returns everything after the FIRST "-->" with the
      leading blank line(s) trimmed.
    #>
    param([string]$Path)
    $raw = Get-Content -Raw -LiteralPath $Path
    $idx = $raw.IndexOf('-->')
    if ($idx -lt 0) { throw "Get-EmbeddedBodyAfterHtmlComment: no GENERATED header comment in $Path" }
    return $raw.Substring($idx + 3).TrimStart("`r", "`n")
}

function Get-EmbeddedTomlBody {
    <#
      Codex's `.toml` role files carry the body as a
      developer_instructions = '''...''' multi-line literal string.
    #>
    param([string]$Path)
    $raw = Get-Content -Raw -LiteralPath $Path
    if ($raw -notmatch "(?s)developer_instructions = '''\r?\n(.*)\r?\n'''") {
        throw "Get-EmbeddedTomlBody: no developer_instructions literal string found in $Path"
    }
    return (ConvertTo-CrlfIndependent $Matches[1]) + "`r`n"
}

$claudeProfile = $harnessesConfig.harnesses.claude
$codexProfile = $harnessesConfig.harnesses.codex
$opencodeProfile = $harnessesConfig.harnesses.opencode

foreach ($tierProp in $claudeProfile.roles.worker.PSObject.Properties) {
    $tier = $tierProp.Name
    $path = Join-Path $repoRoot ".claude/agents/zagent-worker-$tier.md"
    Assert-That "claude worker/$tier body byte-identical to worker.md + substitution" {
        $actual = Get-EmbeddedBodyAfterHtmlComment -Path $path
        $expected = Get-ExpectedWorkerBody -WorkerToolDenial $claudeProfile.workerToolDenial
        $actual -ceq $expected
    }
}
foreach ($riskProp in $claudeProfile.roles.reviewerByRisk.PSObject.Properties) {
    $risk = $riskProp.Name.ToLowerInvariant()
    $path = Join-Path $repoRoot ".claude/agents/zagent-reviewer-$risk.md"
    Assert-That "claude reviewer/$risk body byte-identical to reviewer.md" {
        (Get-EmbeddedBodyAfterHtmlComment -Path $path) -ceq $expectedReviewerBody
    }
}

foreach ($tierProp in $codexProfile.roles.worker.PSObject.Properties) {
    $tier = $tierProp.Name
    $path = Join-Path $repoRoot ".codex/agents/zagent-worker-$tier.toml"
    Assert-That "codex worker/$tier body byte-identical to worker.md + substitution (+ declared paragraph)" {
        # New-CodexAgentToml wraps the body in '''...''' after $Body.TrimEnd() --
        # a deliberate part of Codex's own TOML emission (item 4 of the Stage 6
        # brief), not a byte the charter itself carries, so TrimEnd() both
        # sides here rather than asserting a trailing-newline difference that
        # is not a real divergence from the canonical charter.
        $actual = (Get-EmbeddedTomlBody -Path $path).TrimEnd()
        $expected = (Get-ExpectedWorkerBody -WorkerToolDenial $codexProfile.workerToolDenial).TrimEnd()
        $actual -ceq $expected
    }
}
foreach ($riskProp in $codexProfile.roles.reviewerByRisk.PSObject.Properties) {
    $risk = $riskProp.Name.ToLowerInvariant()
    $path = Join-Path $repoRoot ".codex/agents/zagent-reviewer-$risk.toml"
    Assert-That "codex reviewer/$risk body byte-identical to reviewer.md" {
        (Get-EmbeddedTomlBody -Path $path).TrimEnd() -ceq $expectedReviewerBody.TrimEnd()
    }
}

# OpenCode's bodyDelivery is "reference": its own role config must point
# {file:...} at the GENERATED resolved charter, never at the raw canonical
# charter directly -- roles/worker.md still carries {{PROTECTED_PATHS}}
# unsubstituted, so a build that pointed there would ship a literal
# template placeholder into a live agent's prompt.
$opencodeJsonPath = Join-Path $repoRoot '.opencode/opencode.json'
function ConvertFrom-Jsonc {
    param([string]$Path)
    $lines = Get-Content -LiteralPath $Path
    $stripped = $lines | Where-Object { $_.TrimStart() -notmatch '^//' }
    return ($stripped -join "`n") | ConvertFrom-Json
}
$opencodeJson = ConvertFrom-Jsonc -Path $opencodeJsonPath

foreach ($tierProp in $opencodeProfile.roles.worker.PSObject.Properties) {
    $tier = $tierProp.Name
    $agentName = "zagent-worker-$tier"
    Assert-That "opencode worker/$tier prompt references the GENERATED resolved worker charter" {
        $entry = $opencodeJson.agent.$agentName
        $prompt = $entry.prompt
        ($prompt -match '\{file:.*worker\.resolved\.md\}') -and (-not ($prompt -match 'roles/worker\.md'))
    }
}
foreach ($riskProp in $opencodeProfile.roles.reviewerByRisk.PSObject.Properties) {
    $risk = $riskProp.Name.ToLowerInvariant()
    $agentName = "zagent-reviewer-$risk"
    Assert-That "opencode reviewer/$risk prompt references the GENERATED resolved reviewer charter" {
        $entry = $opencodeJson.agent.$agentName
        $prompt = $entry.prompt
        ($prompt -match '\{file:.*reviewer\.resolved\.md\}') -and (-not ($prompt -match 'roles/reviewer\.md'))
    }
}
Assert-That 'the resolved worker/reviewer charter files opencode references actually exist' {
    (Test-Path (Join-Path $generatedRolesDir 'worker.resolved.md')) -and (Test-Path (Join-Path $generatedRolesDir 'reviewer.resolved.md'))
}

Write-Host "`n=== 2. Build-HarnessArtifacts.ps1 -Check exits 0 for every harness ===" -ForegroundColor Cyan
foreach ($h in @('claude', 'opencode', 'codex')) {
    Assert-That "Build-HarnessArtifacts.ps1 -Check -Harness $h exits 0" {
        $output = & pwsh -NoProfile -File $buildArtifactsPath -Check -Harness $h 2>&1
        $exit = $LASTEXITCODE
        if ($exit -ne 0) {
            Write-Host "       Build-HarnessArtifacts.ps1 -Check -Harness $h stdout:" -ForegroundColor Yellow
            $output | ForEach-Object { Write-Host "         $_" }
        }
        $exit -eq 0
    }
}

Write-Host "`n=== 3. every generated file carries the GENERATED header ===" -ForegroundColor Cyan
$generatedFiles = @(
    '.claude/commands/tick.md',
    '.claude/agents/zagent-worker-sonnet.md',
    '.claude/agents/zagent-worker-opus.md',
    '.claude/agents/zagent-reviewer-low.md',
    '.claude/agents/zagent-reviewer-medium.md',
    '.claude/agents/zagent-reviewer-high.md',
    'Tools/zagent/protocol/roles/generated/worker.resolved.md',
    'Tools/zagent/protocol/roles/generated/reviewer.resolved.md',
    '.opencode/opencode.json',
    '.opencode/commands/tick.md',
    '.codex/agents/zagent-worker-sonnet.toml',
    '.codex/agents/zagent-worker-opus.toml',
    '.codex/agents/zagent-reviewer-low.toml',
    '.codex/agents/zagent-reviewer-medium.toml',
    '.codex/agents/zagent-reviewer-high.toml',
    '.codex/skills/tick/SKILL.md',
    '.codex/config.toml'
)
foreach ($relPath in $generatedFiles) {
    Assert-That "$relPath carries the GENERATED header" {
        $full = Join-Path $repoRoot $relPath
        (Test-Path -LiteralPath $full) -and ((Get-Content -Raw -LiteralPath $full).Contains('GENERATED — DO NOT EDIT.'))
    }
}

# ═════════════════════════════════════════════════════════════════════════
#  Thin entry points -- only where deliveryMode is "stub"
# ═════════════════════════════════════════════════════════════════════════

Write-Host "`n=== 4. a 'stub' entry point stays under 120 lines ===" -ForegroundColor Cyan
foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
    $id = $hProp.Name
    $profile = $hProp.Value
    if ($profile.deliveryMode -ne 'stub') {
        Write-Host "  (skip) $id -- deliveryMode is '$($profile.deliveryMode)', not 'stub'; covered by assertion 1 instead" -ForegroundColor DarkGray
        continue
    }
    $entryFull = Join-Path $repoRoot $profile.entryPoint
    Assert-That "$id ($($profile.entryPoint)) entry point is under 120 lines" {
        $lineCount = @(Get-Content -LiteralPath $entryFull).Count
        if ($lineCount -ge 120) {
            Write-Host "       $($profile.entryPoint) is $lineCount lines -- the Claude frontmatter's broad prefixes (Bash(pwsh -NoProfile*), etc.) pre-authorise 37 gate lines plus cleanup in one entry, so 120 was measured comfortable; re-check the ceiling before raising it."
        }
        $lineCount -lt 120
    }
}

Write-Host "`n=== 5. entry points built from entrypoint.md.tmpl carry the pointer + all four sections ===" -ForegroundColor Cyan
# Claude's entry point does NOT use entrypoint.md.tmpl at all (deliveryMode
# "inline" bypasses the template entirely -- Get-ClaudeArtifacts splices
# frontmatter directly onto TICK.md's own body, see Build-HarnessArtifacts.ps1).
# It therefore has no "## Controls" / "## Roles" / "## Review policy" /
# "## Pre-authorisation" sections and no "pointer to TICK.md" distinct from
# simply BEING TICK.md's body -- that is a real, documented asymmetry
# (Tools/zagent/CLAUDE.md's own "three permitted differences" section), not
# an oversight this assertion should invent a false positive over. Scoped to
# the two harnesses that actually resolve via the shared template.
$templateHarnesses = @('opencode', 'codex')
foreach ($id in $templateHarnesses) {
    $profile = $harnessesConfig.harnesses.$id
    $entryFull = Join-Path $repoRoot $profile.entryPoint
    Assert-That "$id entry point points at TICK.md and carries all four profile sections" {
        $text = Get-Content -Raw -LiteralPath $entryFull
        $hasPointer = $text.Contains('Tools/zagent/protocol/TICK.md')
        $hasControls = $text -match '(?m)^## Controls\s*$'
        $hasRoles = $text -match '(?m)^## Roles\s*$'
        $hasReview = $text -match '(?m)^## Review policy\s*$'
        $hasPreauth = $text -match '(?m)^## Pre-authorisation\s*$'
        if (-not ($hasPointer -and $hasControls -and $hasRoles -and $hasReview -and $hasPreauth)) {
            Write-Host ("       pointer={0} Controls={1} Roles={2} Review-policy={3} Pre-authorisation={4}" -f $hasPointer, $hasControls, $hasRoles, $hasReview, $hasPreauth)
        }
        $hasPointer -and $hasControls -and $hasRoles -and $hasReview -and $hasPreauth
    }
}
Write-Host "  (note) claude's entry point is deliveryMode:inline and bypasses entrypoint.md.tmpl entirely -- exempted from this assertion, same rationale as assertion 4's inline exemption." -ForegroundColor DarkGray

# ═════════════════════════════════════════════════════════════════════════
#  Canonical file stays neutral
# ═════════════════════════════════════════════════════════════════════════

Write-Host "`n=== 6. TICK.md names no harness, tool, or concrete model string ===" -ForegroundColor Cyan
$forbiddenLiterals = @('Agent{', 'subagent_type', 'ScheduleWakeup', 'TaskStop', 'allowed-tools', '.claude/', '.codex/', '.opencode/')

$exemptTierNames = @('haiku', 'sonnet', 'opus')
$modelStrings = [System.Collections.Generic.HashSet[string]]::new()
foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
    $profile = $hProp.Value
    foreach ($groupName in @('worker', 'reviewerByRisk')) {
        $group = $profile.roles.$groupName
        if ($null -eq $group) { continue }
        foreach ($entryProp in $group.PSObject.Properties) {
            $model = $entryProp.Value.model
            if ([string]::IsNullOrEmpty($model)) { continue }
            if ($exemptTierNames -contains $model) { continue }
            [void]$modelStrings.Add($model)
        }
    }
    if ($null -ne $profile.unusedTiers) {
        foreach ($entryProp in $profile.unusedTiers.PSObject.Properties) {
            $val = $entryProp.Value
            if ($null -eq $val) { continue }
            $model = $val.model
            if ([string]::IsNullOrEmpty($model)) { continue }
            if ($exemptTierNames -contains $model) { continue }
            [void]$modelStrings.Add($model)
        }
    }
}

Assert-That 'TICK.md contains none of the harness-specific forbidden literals or concrete model strings (bare tier names and bare "CLAUDE.md" exempt)' {
    $hits = New-Object System.Collections.Generic.List[string]
    foreach ($lit in $forbiddenLiterals) {
        if ($tickText.Contains($lit)) { $hits.Add("literal: $lit") }
    }
    foreach ($model in $modelStrings) {
        if ($tickText.Contains($model)) { $hits.Add("model string: $model") }
    }
    if ($hits.Count -gt 0) {
        Write-Host "       found: $($hits -join '; ')"
    }
    $hits.Count -eq 0
}

Write-Host "`n=== 7. every bare tier-name mention in TICK.md is in tier-mentions.allowlist ===" -ForegroundColor Cyan
$allowlistPath = Join-Path $protocolDir 'tier-mentions.allowlist'
$allowlistLines = Get-Content -LiteralPath $allowlistPath
$allowlistKeys = [System.Collections.Generic.HashSet[string]]::new()
foreach ($line in $allowlistLines) {
    $t = $line.Trim()
    if ($t -eq '' -or $t.StartsWith('#')) { continue }
    if ($t -match '^narrative:(?<path>[^:]+):(?<line>\d+)\s*=>\s*(?<tier>\S+)$') {
        [void]$allowlistKeys.Add("$($Matches.path):$($Matches.line)=>$($Matches.tier.ToLowerInvariant())")
    }
}

Assert-That 'no NEW bare haiku/sonnet/opus mention in TICK.md is missing from tier-mentions.allowlist' {
    $newMentions = New-Object System.Collections.Generic.List[string]
    for ($i = 0; $i -lt $tickLines.Count; $i++) {
        $lineNum = $i + 1
        foreach ($m in [regex]::Matches($tickLines[$i], '\b(haiku|sonnet|opus)\b', 'IgnoreCase')) {
            $tier = $m.Value.ToLowerInvariant()
            $key = "Tools/zagent/protocol/TICK.md:${lineNum}=>$tier"
            if (-not $allowlistKeys.Contains($key)) {
                $newMentions.Add("$key (`"$($tickLines[$i].Trim())`")")
            }
        }
    }
    if ($newMentions.Count -gt 0) {
        Write-Host "       NEW mention(s) not in tier-mentions.allowlist:"
        $newMentions | ForEach-Object { Write-Host "         $_" }
    }
    $newMentions.Count -eq 0
}

# ═════════════════════════════════════════════════════════════════════════
#  Profile completeness -- fail-closed
# ═════════════════════════════════════════════════════════════════════════

Write-Host "`n=== 8. every harness declares every schema key (no ?? defaults) ===" -ForegroundColor Cyan
$coreSchemaKeys = @('entryPoint', 'invocation', 'roleDir', 'roleFormat', 'bodyDelivery', 'deliveryMode',
    'roles', 'unusedTiers', 'workerToolDenial', 'review', 'loopMode', 'controls', 'preauth', 'humanPrerequisites')
$controlVerbs = @('DISPATCH', 'AWAIT', 'ABORT', 'SCHEDULE', 'STOP')

foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
    $id = $hProp.Name
    $profile = $hProp.Value
    Assert-That "$id declares every core schema key" {
        $missing = @($coreSchemaKeys | Where-Object { -not (Test-HasProp $profile $_) })
        if ($missing.Count -gt 0) { Write-Host "       missing: $($missing -join ', ')" }
        $missing.Count -eq 0
    }
    Assert-That "$id.roles declares worker and reviewerByRisk" {
        (Test-HasProp $profile.roles 'worker') -and (Test-HasProp $profile.roles 'reviewerByRisk')
    }
    Assert-That "$id.controls declares all five control verbs (value may be null, key must exist)" {
        $missing = @($controlVerbs | Where-Object { -not (Test-HasProp $profile.controls $_) })
        if ($missing.Count -gt 0) { Write-Host "       missing: $($missing -join ', ')" }
        $missing.Count -eq 0
    }
    Assert-That "$id.review declares mode" { Test-HasProp $profile.review 'mode' }
    Assert-That "$id.preauth declares kind and sources" {
        (Test-HasProp $profile.preauth 'kind') -and (Test-HasProp $profile.preauth 'sources')
    }
}

Write-Host "`n=== 9. every MODEL_LADDER tier is routed OR parked, in every harness, never both/neither ===" -ForegroundColor Cyan
if ($routingText -notmatch 'const MODEL_LADDER = \[(?<list>[^\]]*)\]') {
    throw "Test-HarnessParity: could not find MODEL_LADDER in $routingPath"
}
$modelLadder = @($Matches.list -split ',' | ForEach-Object { $_.Trim().Trim('"') } | Where-Object { $_ -ne '' })
Assert-That 'MODEL_LADDER read from routing.mjs is non-empty' { $modelLadder.Count -gt 0 }

foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
    $id = $hProp.Name
    $profile = $hProp.Value
    foreach ($tier in $modelLadder) {
        $inWorker = Test-HasProp $profile.roles.worker $tier
        $inUnused = Test-HasProp $profile.unusedTiers $tier
        Assert-That "${id}: tier '$tier' is routed (roles.worker) XOR parked (unusedTiers), never both/neither" {
            if ($inWorker -and $inUnused) { Write-Host "       '$tier' is in BOTH roles.worker and unusedTiers on $id" }
            if (-not $inWorker -and -not $inUnused) { Write-Host "       '$tier' is in NEITHER roles.worker nor unusedTiers on $id" }
            $inWorker -xor $inUnused
        }
    }
}

Write-Host "`n=== 10. modelByComplexity values are routed tiers everywhere, and never parked ===" -ForegroundColor Cyan
# ★ Read the block routing ACTUALLY consumes. Tools/Vault/cli/compat.mjs's
# legacyConfigOf() returns withProjectFileOverlay(policy.resolved, index)
# whenever policy.resolved is non-empty (true for this repo's
# vault.config.json), and only falls back to policy.board.routing for a bare
# fixture whose policy.resolved is empty. So policy.resolved.routing is what
# a real checkout's resolveRouting() actually reads -- verified by reading
# compat.mjs directly rather than trusting routing.mjs's own (now-corrected)
# header comment. Assert the two copies are equal too: they are separate
# blocks in one file with nothing else comparing them.
$resolvedRouting = $vaultConfig.policy.resolved.routing
$boardRouting = $vaultConfig.policy.board.routing

Assert-That 'compat.mjs legacyConfigOf() reads policy.resolved (via withProjectFileOverlay) whenever policy.resolved is non-empty' {
    ($compatText -match 'if\s*\(Object\.keys\(policy\.resolved\)\.length\)\s*return\s*withProjectFileOverlay\(policy\.resolved,\s*index\)') -and
    (@($vaultConfig.policy.resolved.PSObject.Properties).Count -gt 0)
}

Assert-That 'policy.resolved.routing and policy.board.routing are equal (two copies, nothing else compares them)' {
    $a = $resolvedRouting | ConvertTo-Json -Depth 20 -Compress
    $b = $boardRouting | ConvertTo-Json -Depth 20 -Compress
    if ($a -ne $b) {
        Write-Host "       resolved: $a"
        Write-Host "       board:    $b"
    }
    $a -eq $b
}

foreach ($complexityProp in $resolvedRouting.modelByComplexity.PSObject.Properties) {
    $complexity = $complexityProp.Name
    $tier = $complexityProp.Value
    foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
        $id = $hProp.Name
        $profile = $hProp.Value
        Assert-That "modelByComplexity.$complexity ('$tier') is a routed worker tier on $id, and not parked" {
            $isRouted = Test-HasProp $profile.roles.worker $tier
            $isParked = Test-HasProp $profile.unusedTiers $tier
            if (-not $isRouted) { Write-Host "       '$tier' is not in $id.roles.worker" }
            if ($isParked) { Write-Host "       '$tier' is ALSO in $id.unusedTiers" }
            $isRouted -and (-not $isParked)
        }
    }
}

Write-Host "`n=== 11. review.mode is legal, and reviewerByRisk carries all three risks ===" -ForegroundColor Cyan
foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
    $id = $hProp.Name
    $profile = $hProp.Value
    Assert-That "$id.review.mode is 'payload' or 'always'" {
        $profile.review.mode -in @('payload', 'always')
    }
    Assert-That "$id.roles.reviewerByRisk carries LOW, MEDIUM, and HIGH" {
        $missing = @(@('LOW', 'MEDIUM', 'HIGH') | Where-Object { -not (Test-HasProp $profile.roles.reviewerByRisk $_) })
        if ($missing.Count -gt 0) { Write-Host "       missing: $($missing -join ', ')" }
        $missing.Count -eq 0
    }
}

Write-Host "`n=== 12. exactly one role per routed tier/risk is generated, none for a parked tier ===" -ForegroundColor Cyan

function Test-RoleArtifactExists {
    param([string]$HarnessId, [string]$RoleName, $OpenCodeJson)
    switch ($HarnessId) {
        'claude' { return Test-Path -LiteralPath (Join-Path $repoRoot ".claude/agents/$RoleName.md") }
        'codex' { return Test-Path -LiteralPath (Join-Path $repoRoot ".codex/agents/$RoleName.toml") }
        'opencode' { return $null -ne $OpenCodeJson.agent.PSObject.Properties[$RoleName] }
        default { throw "Test-RoleArtifactExists: unknown harness '$HarnessId'" }
    }
}

foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
    $id = $hProp.Name
    $profile = $hProp.Value

    foreach ($tierProp in $profile.roles.worker.PSObject.Properties) {
        $tier = $tierProp.Name
        Assert-That "$id generates a worker role for routed tier '$tier'" {
            Test-RoleArtifactExists -HarnessId $id -RoleName "zagent-worker-$tier" -OpenCodeJson $opencodeJson
        }
    }
    foreach ($riskProp in $profile.roles.reviewerByRisk.PSObject.Properties) {
        $risk = $riskProp.Name.ToLowerInvariant()
        Assert-That "$id generates a reviewer role for risk '$risk'" {
            Test-RoleArtifactExists -HarnessId $id -RoleName "zagent-reviewer-$risk" -OpenCodeJson $opencodeJson
        }
    }
    if ($null -ne $profile.unusedTiers) {
        foreach ($tierProp in $profile.unusedTiers.PSObject.Properties) {
            $tier = $tierProp.Name
            Assert-That "$id generates NO worker role for parked tier '$tier'" {
                -not (Test-RoleArtifactExists -HarnessId $id -RoleName "zagent-worker-$tier" -OpenCodeJson $opencodeJson)
            }
        }
    }
}

# The entry point's own "## Roles" table (opencode/codex only, per assertion
# 5's scoping) must name a role reference that actually resolves.
foreach ($id in $templateHarnesses) {
    $profile = $harnessesConfig.harnesses.$id
    $entryText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $profile.entryPoint)
    $expectedRefs = switch ($id) {
        'opencode' {
            @{
                'worker-sonnet'   = '`.opencode/opencode.json` -> `agent."zagent-worker-sonnet"`'
                'worker-opus'     = '`.opencode/opencode.json` -> `agent."zagent-worker-opus"`'
                'reviewer-low'    = '`.opencode/opencode.json` -> `agent."zagent-reviewer-low"`'
                'reviewer-medium' = '`.opencode/opencode.json` -> `agent."zagent-reviewer-medium"`'
                'reviewer-high'   = '`.opencode/opencode.json` -> `agent."zagent-reviewer-high"`'
            }
        }
        'codex' {
            @{
                'worker-sonnet'   = '`.codex/agents/zagent-worker-sonnet.toml`'
                'worker-opus'     = '`.codex/agents/zagent-worker-opus.toml`'
                'reviewer-low'    = '`.codex/agents/zagent-reviewer-low.toml`'
                'reviewer-medium' = '`.codex/agents/zagent-reviewer-medium.toml`'
                'reviewer-high'   = '`.codex/agents/zagent-reviewer-high.toml`'
            }
        }
    }
    foreach ($roleKey in $expectedRefs.Keys) {
        Assert-That "$id entry point's Roles table maps '$roleKey' to a role reference that exists" {
            $ref = $expectedRefs[$roleKey]
            $entryText.Contains($ref)
        }
    }
}

Write-Host "`n=== 13. workerToolDenial 'declared' pairs with review.mode 'always' ===" -ForegroundColor Cyan
foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
    $id = $hProp.Name
    $profile = $hProp.Value
    Assert-That "${id}: workerToolDenial 'declared' implies review.mode 'always' (or workerToolDenial is 'enforced')" {
        if ($profile.workerToolDenial -eq 'declared') {
            if ($profile.review.mode -ne 'always') {
                Write-Host "       $id.workerToolDenial is 'declared' but review.mode is '$($profile.review.mode)', not 'always'"
            }
            $profile.review.mode -eq 'always'
        } else {
            $true
        }
    }
}

# ═════════════════════════════════════════════════════════════════════════
#  Pre-authorisation
# ═════════════════════════════════════════════════════════════════════════

Write-Host "`n=== 14/15. prefix-allowlist / permission-glob harnesses pre-authorise every gate, cleanup, and mandated command ===" -ForegroundColor Cyan

function Get-AllGateAndCleanupLines {
    <#
      Union of every command line this repo's own gate/cleanup config can
      hand to an unattended tick: the project-level gates/cleanup plus every
      CATEGORY's own gates/cleanup (a category's paths get UNIONED into a
      diff's gate list per Tools/Vault/cli/legacy-pure/routing.mjs's
      effectiveGates, so any of them can run on any ticket that touches its
      paths).
    #>
    param($ProjectConfig)
    $lines = [System.Collections.Generic.List[string]]::new()
    if (Test-HasProp $ProjectConfig 'gates') {
        foreach ($g in @($ProjectConfig.gates)) { if ($g) { $lines.Add($g) } }
    }
    if (Test-HasProp $ProjectConfig 'cleanup') {
        foreach ($c in @($ProjectConfig.cleanup)) { if ($c) { $lines.Add($c) } }
    }
    foreach ($catProp in $ProjectConfig.categories.PSObject.Properties) {
        $cat = $catProp.Value
        if (Test-HasProp $cat 'gates') {
            foreach ($g in @($cat.gates)) { if ($g) { $lines.Add($g) } }
        }
        if (Test-HasProp $cat 'cleanup') {
            foreach ($c in @($cat.cleanup)) { if ($c) { $lines.Add($c) } }
        }
    }
    return @($lines | Select-Object -Unique)
}
$allGateAndCleanupLines = Get-AllGateAndCleanupLines -ProjectConfig $projectConfig

function Get-DeclaredPrefixRoots {
    <#
      Reads a harness's own declared preauth.sources and returns the UNION
      of prefix roots across all of them -- Claude Code's own permission
      model is additive across the command frontmatter and settings.json
      (either granting a prefix is enough), not a requirement that each
      source independently cover everything on its own; settings.json's own
      entries are deliberately narrow (named scripts only) and rely on the
      command frontmatter's broader Bash(pwsh -NoProfile*) etc. for the rest.
    #>
    param($HarnessId)
    $roots = [System.Collections.Generic.List[string]]::new()
    switch ($HarnessId) {
        'claude' {
            $entryPath = Join-Path $repoRoot '.claude/commands/tick.md'
            $entryText = Get-Content -Raw -LiteralPath $entryPath
            if ($entryText -match '(?m)^allowed-tools:\s*(.+)$') {
                foreach ($tok in ($Matches[1] -split ',')) {
                    $t = $tok.Trim()
                    if ($t -match '^(?:Bash|PowerShell)\(') { $roots.Add((Get-CommandPrefixRoot $t)) }
                }
            }
            $settingsPath = Join-Path $repoRoot '.claude/settings.json'
            if (Test-Path -LiteralPath $settingsPath) {
                $settings = Get-Content -Raw -LiteralPath $settingsPath | ConvertFrom-Json
                foreach ($entry in @($settings.permissions.allow)) {
                    if ($entry -match '^(?:Bash|PowerShell)\(') { $roots.Add((Get-CommandPrefixRoot $entry)) }
                }
            }
        }
        default { throw "Get-DeclaredPrefixRoots: no reader implemented for harness '$HarnessId'" }
    }
    return @($roots | Select-Object -Unique)
}

# TICK.md quotes `npm test` as MEASURED HISTORY ("Measured on ZEN-79:
# `npm test` in `Tools/Vault` was already red on a ...") describing a fact
# about a past run, not instructing the loop to invoke bare `npm test` --
# the loop only ever runs it nested inside the already-covered gate line
# `pwsh -NoProfile -Command "Set-Location Tools/Vault; npm test"`. One
# reviewed, documented exception, the same shape tier-mentions.allowlist
# already uses for narrative tier mentions; if this ever needs a second
# entry, it should become a real allowlist file instead of growing here.
#
# Matched by COMMAND TEXT plus a stable content marker on the same source
# line, not by line number -- TICK.md's line numbers move every time
# unrelated content is added earlier in the file (this exact exemption's
# own line moved twice while this script was being written), and a
# line-anchored exemption here would be exactly the fragile-citation habit
# assertion 21 exists to catch.
$narrativeCommandExemptions = @(
    @{ Text = 'npm test'; LineContains = 'Measured on ZEN-79' }
)

function Get-MandatedCommandsFromTick {
    param([string]$Text, [string[]]$FirstTokens)
    # Blank out fenced blocks WITHOUT removing their newlines, so every line
    # number reported below still lines up with the real file -- a plain
    # removal shifts everything after the first fence, which is exactly how
    # the narrative exemption below briefly stopped matching while this
    # script was being written (the fenced fixture examples earlier in
    # TICK.md shifted `npm test`'s reported line out from under it).
    $noFences = [regex]::Replace($Text, '```[\s\S]*?```', {
            param($m)
            return ("`n" * ([regex]::Matches($m.Value, "`n")).Count)
        })
    $lines = @($noFences -split "`n")
    $found = New-Object System.Collections.Generic.List[object]
    for ($i = 0; $i -lt $lines.Count; $i++) {
        foreach ($m in [regex]::Matches($lines[$i], '`([^`\n]+)`')) {
            $span = $m.Groups[1].Value.Trim()
            if ($span -eq '') { continue }
            $firstToken = ($span -split '\s+')[0]
            if ($FirstTokens -contains $firstToken) {
                $found.Add([PSCustomObject]@{ Line = $i + 1; Command = $span })
            }
        }
    }
    return $found
}

foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
    $id = $hProp.Name
    $profile = $hProp.Value
    if ($profile.preauth.kind -notin @('prefix-allowlist', 'permission-glob')) {
        Write-Host "  (skip) $id -- preauth.kind is '$($profile.preauth.kind)', not prefix-allowlist/permission-glob" -ForegroundColor DarkGray
        continue
    }

    $roots = Get-DeclaredPrefixRoots -HarnessId $id

    foreach ($line in $allGateAndCleanupLines) {
        Assert-That "$id pre-authorises gate/cleanup line: $line" {
            $covered = Test-CommandCovered -Command $line -PrefixRoots $roots
            if (-not $covered) { Write-Host "       NOT covered on $id by any of: $($roots -join ', ')" }
            $covered
        }
    }

    $mandated = Get-MandatedCommandsFromTick -Text $tickText -FirstTokens @('zagent', 'pwsh', 'git', 'npm', 'timeout')
    foreach ($cmd in $mandated) {
        $sourceLine = if ($cmd.Line -ge 1 -and $cmd.Line -le $tickLines.Count) { $tickLines[$cmd.Line - 1] } else { '' }
        $exemption = $narrativeCommandExemptions | Where-Object { $_.Text -eq $cmd.Command -and $sourceLine.Contains($_.LineContains) }
        if ($exemption) {
            Write-Host "  (skip) TICK.md:$($cmd.Line) ``$($cmd.Command)`` -- documented narrative exemption (measured-history prose, not a mandated top-level invocation)" -ForegroundColor DarkGray
            continue
        }
        Assert-That "$id pre-authorises TICK.md:$($cmd.Line) mandated command: $($cmd.Command)" {
            $covered = Test-CommandCovered -Command $cmd.Command -PrefixRoots $roots
            if (-not $covered) { Write-Host "       NOT covered on $id by any of: $($roots -join ', ')" }
            $covered
        }
    }
}

Write-Host "`n=== 16. Codex pre-authorisation -- asserted only where it can bind ===" -ForegroundColor Cyan
# ★ Honesty over reach, per the brief: approval_policy is a real, settable
# config key (Stage 0 probe 6, measured) so a parity test CAN assert this
# file exists, parses as TOML-shaped, and sets it. trust_level is
# machine-local (~/.codex/config.toml, not repo-committable) and
# [windows] sandbox = "elevated" parses but its backing feature is disabled
# in this build and enforces NOTHING today -- this checkout cannot assert
# either one binds anything, and does not try. If that reads as a weaker
# assertion than 14/15, it is: it checks the file's SHAPE, not its effect,
# because nothing runnable from this repo can confirm the effect without a
# live, unmetered Codex run (stage0-verdicts.md, "Still open" item 2).
$codexConfigPath = Join-Path $repoRoot '.codex/config.toml'
Assert-That '.codex/config.toml exists and declares approval_policy' {
    (Test-Path -LiteralPath $codexConfigPath) -and ((Get-Content -Raw -LiteralPath $codexConfigPath) -match 'approval_policy\s*=\s*"never"')
}
Assert-That '.codex/config.toml does NOT claim to set trust_level or features.multi_agent (both are machine-local / unassertable from this repo)' {
    $text = Get-Content -Raw -LiteralPath $codexConfigPath
    (-not ($text -match '(?m)^\s*trust_level\s*=')) -and (-not ($text -match 'multi_agent'))
}

Write-Host "`n=== 17. humanPrerequisites are named in that harness's own entry-point pre-flight ===" -ForegroundColor Cyan
foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
    $id = $hProp.Name
    $profile = $hProp.Value
    $prereqs = @($profile.humanPrerequisites)
    if ($prereqs.Count -eq 0) {
        Write-Host "  (skip) $id -- no humanPrerequisites declared" -ForegroundColor DarkGray
        continue
    }
    $entryFull = Join-Path $repoRoot $profile.entryPoint
    $entryText = Get-Content -Raw -LiteralPath $entryFull
    foreach ($prereq in $prereqs) {
        Assert-That "$id entry point names human prerequisite: $($prereq.Substring(0, [Math]::Min(60, $prereq.Length)))..." {
            $entryText.Contains($prereq)
        }
    }
}

# ═════════════════════════════════════════════════════════════════════════
#  Protected paths -- against the list `guard` actually reads
# ═════════════════════════════════════════════════════════════════════════

Write-Host "`n=== 18. every path this ticket adds is protected in vault.config.json's RESOLVED guardrails ===" -ForegroundColor Cyan
# ★ vault.config.json's policy.resolved.guardrails.protectedPaths, not
# zagent.project.json's own protectedPaths -- guardrailsOf (compat.mjs)
# resolves protected paths as a `??` FALLBACK CHAIN (project's own ??
# boardGuardrails ?? clientFile's), never a union, and boardGuardrails (this
# list) wins whenever it is non-empty, which it is here. Asserting against
# zagent.project.json's copy would assert nothing about what `zagent guard`
# actually enforces.
$mustBeProtected = @(
    '.claude/**',
    'zagent.project.json',
    'vault.config.json',
    'Tools/zagent/protocol/**',
    'Tools/zagent/Build-HarnessArtifacts.ps1',
    'Tools/zagent/Test-HarnessParity.ps1',
    '.codex/**',
    '.opencode/**'
)
foreach ($p in $mustBeProtected) {
    Assert-That "vault.config.json's resolved protectedPaths includes '$p'" {
        $protectedPaths -contains $p
    }
}

Write-Host "`n=== 19. every harness's entryPoint/roleDir is covered by a resolved protected-path pattern ===" -ForegroundColor Cyan
function Test-GlobCoversPath {
    param([string]$Pattern, [string]$Path)
    $p = $Pattern.Replace('\', '/')
    $target = $Path.Replace('\', '/')
    if ($p -eq $target) { return $true }
    if ($p.EndsWith('/**')) {
        $prefix = $p.Substring(0, $p.Length - 3)
        return ($target -eq $prefix) -or $target.StartsWith("$prefix/")
    }
    return $false
}

foreach ($hProp in $harnessesConfig.harnesses.PSObject.Properties) {
    $id = $hProp.Name
    $profile = $hProp.Value
    foreach ($fieldName in @('entryPoint', 'roleDir')) {
        if (-not (Test-HasProp $profile $fieldName)) { continue }
        $value = $profile.$fieldName
        Assert-That "$id.$fieldName ('$value') is covered by a resolved protected-path pattern" {
            $covered = $false
            foreach ($pattern in $protectedPaths) {
                if (Test-GlobCoversPath -Pattern $pattern -Path $value) { $covered = $true; break }
            }
            if (-not $covered) { Write-Host "       '$value' matches none of: $($protectedPaths -join ', ')" }
            $covered
        }
    }
}
# Codex's pre-authorisation layer is a distinct path from roleDir/entryPoint
# and would be exactly the kind of fourth config surface a new harness could
# add and forget to protect -- covered explicitly rather than assumed.
Assert-That "codex.preauth's config file ('.codex/config.toml') is covered by a resolved protected-path pattern" {
    $value = $codexProfile.preauth.sources | Where-Object { $_ -match '\.codex/config\.toml' } | Select-Object -First 1
    $covered = $false
    foreach ($pattern in $protectedPaths) {
        if (Test-GlobCoversPath -Pattern $pattern -Path '.codex/config.toml') { $covered = $true; break }
    }
    (-not [string]::IsNullOrEmpty($value)) -and $covered
}

# ═════════════════════════════════════════════════════════════════════════
#  Citations
# ═════════════════════════════════════════════════════════════════════════

Write-Host "`n=== 20/21. TICK.md citations under Tools/** resolve, and no live file hardcodes a line number or the old Claude-only path ===" -ForegroundColor Cyan

$knownAnchors = [System.Collections.Generic.HashSet[string]]::new()
foreach ($line in $tickLines) {
    if ($line -match '^#{1,6}\s+(.+?)\s*$') {
        [void]$knownAnchors.Add((Get-GithubSlug $Matches[1]))
    }
}
Assert-That 'TICK.md has at least one heading (the anchor set is not vacuously empty)' { $knownAnchors.Count -gt 0 }

function Get-LiveToolsFiles {
    <#
      Every text source file under Tools/**, excluding the vendored/
      generated trees that are explicitly out of scope for both citation
      checks (Tools/Vault/fixtures/** are canned test inputs, Tools/Vault/
      artifacts/** are build/coverage output -- neither is "prose a person
      maintains"), excluding node_modules, and excluding this script's own
      file (its source necessarily contains the literal strings both checks
      search FOR, as regex/comparison text, not as citations).
    #>
    $exts = @('.md', '.ps1', '.psm1', '.mjs', '.js', '.json', '.toml')
    $excludeDirs = @(
        (Join-Path $repoRoot 'Tools/Vault/fixtures'),
        (Join-Path $repoRoot 'Tools/Vault/artifacts')
    )
    Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Tools') -Recurse -File |
        Where-Object {
            $full = $_.FullName
            ($exts -contains $_.Extension) -and
            ($full -notmatch '[\\/]node_modules[\\/]') -and
            (-not ($excludeDirs | Where-Object { $full.StartsWith($_) })) -and
            ($full -ne (Join-Path $repoRoot 'Tools/zagent/Test-HarnessParity.ps1'))
        }
}
$liveToolsFiles = @(Get-LiveToolsFiles)
Assert-That 'at least one live file was scanned under Tools/** for TICK.md citations' { $liveToolsFiles.Count -gt 0 }

$anchorCitationRegex = 'TICK\.md#([A-Za-z0-9_-]+)'
$badAnchors = New-Object System.Collections.Generic.List[string]
foreach ($f in $liveToolsFiles) {
    $lines = Get-Content -LiteralPath $f.FullName
    for ($i = 0; $i -lt $lines.Count; $i++) {
        foreach ($m in [regex]::Matches($lines[$i], $anchorCitationRegex)) {
            $anchor = $m.Groups[1].Value
            if (-not $knownAnchors.Contains($anchor)) {
                $rel = $f.FullName.Substring($repoRoot.Length + 1).Replace('\', '/')
                $badAnchors.Add("${rel}:$($i + 1): TICK.md#$anchor")
            }
        }
    }
}
Assert-That 'every TICK.md#<anchor> citation under Tools/** resolves to a real TICK.md heading' {
    if ($badAnchors.Count -gt 0) { $badAnchors | ForEach-Object { Write-Host "       $_" } }
    $badAnchors.Count -eq 0
}

# Files whose entire purpose is to document the GENERATED-artifact pipeline
# necessarily name the concrete path `.claude/commands/tick.md` as data or
# as a description of build output, not as "the habit" of pointing a reader
# at the harness-specific copy as though it were the source of truth. This
# ratchet's own file (tier-mentions.allowlist) is likewise exempt from the
# line-number half for the same reason: its entire job is to be a
# maintained, line-anchored index into TICK.md, not a stale citation.
$citationExemptFiles = @(
    'Tools/zagent/protocol/tier-mentions.allowlist',
    'Tools/zagent/Build-HarnessArtifacts.ps1',
    'Tools/zagent/CLAUDE.md',
    'Tools/zagent/README.md',
    'Tools/zagent/protocol/harnesses.json',
    'Tools/zagent/Test-ZagentClient.ps1',
    # A unit test whose whole job is to check a path-allowlist predicate
    # against literal path strings, including this exact one -- it asserts
    # `.claude/commands/tick.md` stays DISALLOWED, i.e. it is testing FOR
    # the property this check wants, not exhibiting the habit.
    'Tools/Vault/test/legacy-backend-guard.test.mjs'
) | ForEach-Object { (Join-Path $repoRoot $_) }

$lineRefRegex = 'TICK\.md:\d+'
$claudePathRegex = [regex]::Escape('.claude/commands/tick.md')
$staleRefs = New-Object System.Collections.Generic.List[string]
foreach ($f in $liveToolsFiles) {
    if ($citationExemptFiles -contains $f.FullName) { continue }
    $lines = Get-Content -LiteralPath $f.FullName
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $rel = $f.FullName.Substring($repoRoot.Length + 1).Replace('\', '/')
        if ($lines[$i] -match $lineRefRegex) {
            $staleRefs.Add("${rel}:$($i + 1): hardcodes a TICK.md line number")
        }
        if ($lines[$i] -match $claudePathRegex) {
            $staleRefs.Add("${rel}:$($i + 1): references .claude/commands/tick.md instead of the canonical TICK.md")
        }
    }
}
Assert-That 'no live file under Tools/** (outside the documented pipeline-description exceptions) hardcodes a TICK.md line number or the old Claude-only path' {
    if ($staleRefs.Count -gt 0) { $staleRefs | ForEach-Object { Write-Host "       $_" } }
    $staleRefs.Count -eq 0
}

Write-Host "`n=== 22. Tools/zagent/CLAUDE.md exists and names every harness ===" -ForegroundColor Cyan
$zagentClaudeMdPath = Join-Path $repoRoot 'Tools/zagent/CLAUDE.md'
Assert-That 'Tools/zagent/CLAUDE.md exists' { Test-Path -LiteralPath $zagentClaudeMdPath }
if (Test-Path -LiteralPath $zagentClaudeMdPath) {
    $claudeMdText = Get-Content -Raw -LiteralPath $zagentClaudeMdPath
    foreach ($id in $harnessNames) {
        Assert-That "Tools/zagent/CLAUDE.md names harness '$id'" {
            $claudeMdText -match [regex]::Escape($id)
        }
    }
}

# ═════════════════════════════════════════════════════════════════════════
#  Stage 3 headroom: the "stub" deliveryMode's read-window margin
# ═════════════════════════════════════════════════════════════════════════
#
# ★ THE 2000-LINE FIGURE IS A MEASUREMENT OF ONE TOOL, NOT A UNIVERSAL
# CONSTANT, AND THE ASSERTION MUST ONLY BIND WHEN THAT TOOL IS ACTUALLY IN
# PLAY. The concern this assertion polices is narrow: a harness whose
# `deliveryMode` is `"stub"` tells its own session to go read
# Tools/zagent/protocol/TICK.md itself, in one call, using whatever
# default-sized read its own tooling gives a file read with no explicit
# range -- Claude Code's own Read tool is the ONE case actually measured at
# a 2000-line default window. A harness that is `"inline"` (claude, codex
# today) never issues that read at all: the full body is already spliced
# into its entry point at generation time, so a growing TICK.md costs it
# nothing. Failing this assertion while NO harness is `"stub"` would be
# exactly the failure mode the brief warned about -- reds over a constraint
# that binds nothing yet, which trains people to ignore the gate.
#
# So: compute and ALWAYS print the margin (never hide the number, whichever
# way this resolves), and FAIL only when BOTH (a) the margin has actually
# dropped under the threshold AND (b) at least one harness in
# harnesses.json is genuinely `deliveryMode: "stub"` today -- i.e. the
# figure is live, not academic. When no harness is stub, this is a
# non-failing informational line naming the current margin, so the next
# person to flip a harness to "stub" gets a hard failure the moment it
# starts mattering, without today's number (which does not bind anything
# right now, or DOES -- see below) being silently swallowed either way.
#
# ★★ THE FIGURE ITSELF IS A MEASUREMENT WITH AN EXPIRY DATE, NOT A
# PROPERTY: 117 lines of headroom when this programme started, 98 after
# the harness-neutrality pass, and whatever this run prints now. It only
# ever shrinks as TICK.md grows, and nobody re-measures it by hand.
Write-Host "`n=== stub-delivery headroom: TICK.md vs. a 2000-line default Read window ===" -ForegroundColor Cyan
$stubHarnessIds = @($harnessesConfig.harnesses.PSObject.Properties | Where-Object { $_.Value.deliveryMode -eq 'stub' } | ForEach-Object { $_.Name })
Assert-That 'TICK.md leaves at least 50 lines of headroom under a 2000-line default Read window, IF a harness actually relies on it (deliveryMode: "stub")' {
    $lineCount = $tickLines.Count
    $headroom = 2000 - $lineCount
    if ($stubHarnessIds.Count -eq 0) {
        Write-Host ("       TICK.md is {0} lines -- {1} line(s) of margin under a 2000-line default Read window. INFORMATIONAL ONLY: no harness in harnesses.json is deliveryMode 'stub' today, so nothing actually relies on a single default-sized Read of the whole file, and this margin does not bind anything yet." -f $lineCount, $headroom)
        return $true
    }
    Write-Host ("       TICK.md is {0} lines -- {1} line(s) of margin under a 2000-line default Read window. BINDING: {2} harness(es) are deliveryMode 'stub' today ({3}) and rely on exactly this." -f $lineCount, $headroom, $stubHarnessIds.Count, ($stubHarnessIds -join ', '))
    $headroom -ge 50
}

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
