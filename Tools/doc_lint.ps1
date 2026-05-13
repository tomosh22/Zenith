# =============================================================================
# doc_lint.ps1 -- MVP-0.3.2 documentation cross-check linter.
#
# Runs 6 cross-document consistency checks per the round-5 peer-review
# consensus (see Games/DevilsPlayground/Docs/MvpRoadmap.md MVP-0.3.2 entry):
#
#   1. Test count consistency across Status.md / TestPlan.md /
#      BuildEnvironment.md / AgentBriefing.md / Shortfalls.md.
#   2. MVP archetype names agree across MVPScope.md / TestPlan.md /
#      MvpRoadmap.md section 0.2.1 / Archetypes.json "mvp": true entries.
#   3. Roadmap task IDs are unique within MvpRoadmap.md.
#   4. No [SUPERSEDED] markers in active doc text (only in DecisionLog).
#   5. No false "X does not exist" claims for files that DO exist
#      (recurring stale-claim failure mode).
#   6. Cross-references via markdown links resolve (no dead (./Path.md)
#      pointers).
#
# Usage:
#   pwsh -NoProfile -File Tools/doc_lint.ps1
#   powershell -NoProfile -File Tools/doc_lint.ps1
#
# Exit codes:
#   0 -- all 6 checks pass.
#   1 -- one or more violations detected (each violation is printed in
#        a single grep-able line: "VIOLATION [check-id] path:line description").
#
# Written in PowerShell to match the existing Tools/ convention
# (verify_build_env.ps1, run_dp_tests.ps1, agent_session_close.ps1)
# rather than introduce a new Python dependency. ASCII-only body so
# PS 5.1 (default CP1252 codepage) can read it without mojibake. See
# Q-2026-05-12-005 for the parser-error history.
# =============================================================================

[CmdletBinding()]
param(
    [string]$RepoRoot = (Split-Path -Parent $PSScriptRoot),

    # When -Verbose-equivalent: print PASS lines for each check too,
    # not just the summary. Helpful for CI logs.
    [switch]$ShowPassed
)

$ErrorActionPreference = "Stop"

$docsDir = Join-Path $RepoRoot 'Games/DevilsPlayground/Docs'
$configDir = Join-Path $RepoRoot 'Games/DevilsPlayground/Config'

if (-not (Test-Path $docsDir)) {
    Write-Error "Docs dir not found: $docsDir"
    exit 1
}

# Violation reporter -- prepends VIOLATION tag + check id so failures are
# easy to grep out of CI logs.
$script:violations = 0
function Report-Violation {
    param([string]$Check, [string]$Msg)
    Write-Host "VIOLATION [$Check] $Msg" -ForegroundColor Red
    $script:violations++
}

function Report-Pass {
    param([string]$Check, [string]$Msg)
    if ($ShowPassed) {
        Write-Host "PASS [$Check] $Msg" -ForegroundColor Green
    }
}

# =============================================================================
# Check 1: test count consistency.
#
# Each doc may mention "N tests" or "N/M passing" in its text. The expected
# number is the count of Test_*.cpp files under Games/DevilsPlayground/Tests/
# that contain a ZENITH_AUTOMATED_TEST_REGISTER call. Numbers in docs are
# allowed to undershoot (e.g. "34 tests" while reality is 36) but must not
# overshoot. Drift is detected as "doc claims N, registry has M, N > M".
# =============================================================================
function Check-TestCount {
    $testsDir = Join-Path $RepoRoot 'Games/DevilsPlayground/Tests'
    if (-not (Test-Path $testsDir)) {
        Report-Pass 'C1' "Tests dir missing -- skipping count check"
        return
    }

    # Count ZENITH_AUTOMATED_TEST_REGISTER call sites. Each maps to one
    # registered test. Some .cpp files declare multiple tests; counting
    # registration calls is the accurate signal.
    $registerCount = 0
    Get-ChildItem -Path $testsDir -Filter 'Test_*.cpp' -Recurse | ForEach-Object {
        $content = Get-Content $_.FullName -Raw
        $matches = [regex]::Matches($content, 'ZENITH_AUTOMATED_TEST_REGISTER\s*\(')
        $registerCount += $matches.Count
    }

    # Doc claim sites. Each docs file may have multiple "N tests" mentions;
    # report any that overshoot.
    $docFiles = @(
        'Status.md',
        'TestPlan.md',
        'BuildEnvironment.md',
        'AgentBriefing.md',
        'Shortfalls.md'
    )

    foreach ($docFile in $docFiles) {
        $path = Join-Path $docsDir $docFile
        if (-not (Test-Path $path)) { continue }
        $lines = Get-Content $path
        for ($i = 0; $i -lt $lines.Length; $i++) {
            $line = $lines[$i]
            # Skip approximate / future-projection mentions (~N tests).
            if ($line -match '~\s*\d') { continue }
            # Match concrete claims only: "N/M passing|tests", "N tests passing|registered|in the suite".
            # Bounded to 1-3 digit counts to avoid hits on years like 2026.
            $claimed = $null
            if ($line -match '\b(\d{1,3})\s*\/\s*(\d{1,3})\s+(?:tests?|passing)') {
                # "N/M passing" or "N/M tests" -- claim is N or M, whichever is larger.
                $a = [int]$matches[1]
                $b = [int]$matches[2]
                $claimed = [Math]::Max($a, $b)
            }
            elseif ($line -match '\b(\d{1,3})\s+tests?\s+(?:passing|registered|in\s+the\s+suite|in\s+batch)') {
                $claimed = [int]$matches[1]
            }
            if ($null -ne $claimed -and $claimed -gt $registerCount -and $claimed -gt 5) {
                Report-Violation 'C1' "${docFile}:$($i+1): claims $claimed tests but registry has $registerCount"
            }
        }
    }
    Report-Pass 'C1' "test-count consistency (registry=$registerCount)"
}

# =============================================================================
# Check 2: MVP archetype names agree across docs + JSON.
# =============================================================================
function Check-MvpArchetypeNames {
    $archetypesPath = Join-Path $configDir 'Archetypes.json'
    if (-not (Test-Path $archetypesPath)) {
        Report-Pass 'C2' "Archetypes.json missing -- skipping archetype check"
        return
    }

    # Parse the MVP archetype list from the JSON. Hand-rolled regex over the
    # interleaved "id" + "mvp" fields -- the existing DP_Archetypes parser
    # is C++ and not invokable from here.
    $jsonContent = Get-Content $archetypesPath -Raw
    $jsonMvp = New-Object System.Collections.ArrayList
    # Match each archetype object: capture "id": "X" followed (within ~20
    # lines) by "mvp": true. PS regex with the dotall (?s) modifier handles
    # multi-line matches.
    $entryMatches = [regex]::Matches($jsonContent, '"id"\s*:\s*"([^"]+)"[^{]*?"mvp"\s*:\s*(true|false)', [System.Text.RegularExpressions.RegexOptions]::Singleline)
    foreach ($m in $entryMatches) {
        if ($m.Groups[2].Value -eq 'true') {
            $null = $jsonMvp.Add($m.Groups[1].Value)
        }
    }

    if ($jsonMvp.Count -eq 0) {
        Report-Violation 'C2' "Archetypes.json has no mvp:true entries"
        return
    }

    $sortedJson = ($jsonMvp | Sort-Object) -join ','

    # Doc files claim MVP archetype names too. Look for explicit list
    # expressions like "Farmhand, Beggar, Devout, Child" (the ratified order
    # documented in DecisionLog 2026-05-12).
    $docPaths = @(
        (Join-Path $docsDir 'MVPScope.md'),
        (Join-Path $docsDir 'TestPlan.md'),
        (Join-Path $docsDir 'MvpRoadmap.md')
    )
    foreach ($path in $docPaths) {
        if (-not (Test-Path $path)) { continue }
        $name = Split-Path -Leaf $path
        $content = Get-Content $path -Raw
        # Look for any of the JSON-MVP names mentioned in a list context.
        # If a doc mentions ALL 4 JSON-MVP names, it agrees. If it mentions
        # OTHER names (e.g. "Sexton" still listed) in an MVP context, that
        # is a violation -- but "Sexton" can appear in DecisionLog context.
        # Pragmatic rule: warn only if the JSON-MVP set doesn't appear in
        # full anywhere in the file.
        $missing = @()
        foreach ($id in $jsonMvp) {
            if ($content -notmatch [regex]::Escape($id)) {
                $missing += $id
            }
        }
        if ($missing.Count -gt 0) {
            Report-Violation 'C2' "${name}: doc never mentions MVP archetype(s): $($missing -join ', ') (expected per Archetypes.json mvp:true: $sortedJson)"
        }
    }
    Report-Pass 'C2' "MVP archetype name agreement (JSON mvp:true = $sortedJson)"
}

# =============================================================================
# Check 3: roadmap task IDs are unique.
# =============================================================================
function Check-RoadmapUniqueIds {
    $path = Join-Path $docsDir 'MvpRoadmap.md'
    if (-not (Test-Path $path)) { return }

    $seen = @{}
    $lines = Get-Content $path
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i] -match '\*\*(MVP-\d+\.\d+\.\d+)\*\*') {
            $id = $matches[1]
            if ($seen.ContainsKey($id)) {
                Report-Violation 'C3' "MvpRoadmap.md:$($i+1): duplicate task id '$id' (first seen line $($seen[$id]))"
            } else {
                $seen[$id] = $i + 1
            }
        }
    }
    Report-Pass 'C3' "roadmap task ID uniqueness ($($seen.Count) ids)"
}

# =============================================================================
# Check 4: no [SUPERSEDED] markers in active text (only in DecisionLog).
# =============================================================================
function Check-SupersededMarkers {
    Get-ChildItem -Path $docsDir -Filter '*.md' | ForEach-Object {
        if ($_.Name -eq 'DecisionLog.md') { return }
        $lines = Get-Content $_.FullName
        for ($i = 0; $i -lt $lines.Length; $i++) {
            $line = $lines[$i]
            # Skip backtick-quoted occurrences (the literal `[SUPERSEDED]`
            # text being described, e.g. inside this linter's own spec entry
            # in MvpRoadmap.md). Active text means unquoted.
            if ($line -match '`\[SUPERSEDED\]`') { continue }
            if ($line -match '\[SUPERSEDED\]') {
                Report-Violation 'C4' "$($_.Name):$($i+1): contains [SUPERSEDED] marker (only DecisionLog.md may carry these)"
            }
        }
    }
    Report-Pass 'C4' "no [SUPERSEDED] markers in active text"
}

# =============================================================================
# Check 5: no false "X does not exist" claims for files that DO exist.
# =============================================================================
function Check-StaleClaims {
    Get-ChildItem -Path $docsDir -Filter '*.md' -Recurse | ForEach-Object {
        $name = $_.Name
        $lines = Get-Content $_.FullName
        for ($i = 0; $i -lt $lines.Length; $i++) {
            $line = $lines[$i]
            # Match patterns like "FOO.bar does not exist" or "FOO.bar is missing".
            # The matched filename pattern allows backtick or plain prose mention.
            $m = [regex]::Match($line, '[`\s](\S+\.(slang|cpp|h|json|md|ps1))[`\s][^.]*?(does not exist|is missing|not present|not yet created)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
            if ($m.Success) {
                $claimedFile = $m.Groups[1].Value
                # Skip URL-like fragments and quoted absolutes.
                if ($claimedFile -match '^https?:' -or $claimedFile -match '^/') { continue }
                # Try common locations -- repo root + common Tools/ Source/ etc.
                $candidates = @(
                    (Join-Path $RepoRoot $claimedFile),
                    (Get-ChildItem -Path $RepoRoot -Filter (Split-Path -Leaf $claimedFile) -Recurse -ErrorAction SilentlyContinue -File | Select-Object -First 1 -ExpandProperty FullName)
                )
                foreach ($c in $candidates) {
                    if ($c -and (Test-Path $c)) {
                        Report-Violation 'C5' "${name}:$($i+1): claims '$claimedFile' does not exist, but it exists at $c"
                        break
                    }
                }
            }
        }
    }
    Report-Pass 'C5' "no false 'X does not exist' claims"
}

# =============================================================================
# Check 6: cross-references via markdown links resolve.
# =============================================================================
function Check-MarkdownLinks {
    Get-ChildItem -Path $docsDir -Filter '*.md' -Recurse | ForEach-Object {
        $name = $_.Name
        $dir = $_.DirectoryName
        $lines = Get-Content $_.FullName
        for ($i = 0; $i -lt $lines.Length; $i++) {
            $line = $lines[$i]
            # Match markdown link targets like (./File.md) or (../Path/File.md).
            # Skip absolute URLs (http: https: mailto:) and anchors (#section).
            $linkMatches = [regex]::Matches($line, '\]\(([^)]+)\)')
            foreach ($m in $linkMatches) {
                $target = $m.Groups[1].Value
                # Strip trailing #anchor.
                $target = ($target -split '#')[0]
                if ($target -eq '') { continue }
                # Skip external URLs.
                if ($target -match '^https?://' -or $target -match '^mailto:') { continue }
                # Relative path -- resolve against the doc's own directory.
                try {
                    $resolved = Join-Path $dir $target
                    $normalized = [System.IO.Path]::GetFullPath($resolved)
                    if (-not (Test-Path -LiteralPath $normalized)) {
                        Report-Violation 'C6' "${name}:$($i+1): broken markdown link: '$target' (resolved to $normalized)"
                    }
                } catch {
                    # Malformed path -- count as broken.
                    Report-Violation 'C6' "${name}:$($i+1): malformed markdown link: '$target'"
                }
            }
        }
    }
    Report-Pass 'C6' "markdown cross-references resolve"
}

# =============================================================================
# Run all checks.
# =============================================================================
Write-Host "doc_lint.ps1: running 6 checks against $docsDir" -ForegroundColor Cyan
Check-TestCount
Check-MvpArchetypeNames
Check-RoadmapUniqueIds
Check-SupersededMarkers
Check-StaleClaims
Check-MarkdownLinks

Write-Host ""
if ($script:violations -eq 0) {
    Write-Host "doc_lint.ps1: ALL CHECKS PASS" -ForegroundColor Green
    exit 0
} else {
    Write-Host "doc_lint.ps1: $($script:violations) violation(s) found" -ForegroundColor Red
    exit 1
}
