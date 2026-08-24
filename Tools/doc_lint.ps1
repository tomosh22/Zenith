# =============================================================================
# doc_lint.ps1 -- MVP-0.3.2 documentation cross-check linter.
#
# Runs 7 cross-document consistency checks. Checks 1-6 come from the round-5
# peer-review consensus (see Games/DevilsPlayground/Docs/MvpRoadmap.md MVP-0.3.2
# entry); check 7 was added 2026-08-24 after a live /tick audit found the LIVE
# PIN line -- the one line Status.md tells every cold session to read first --
# was the least checked number in the repository:
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
#   7. Status.md's LIVE PIN block agrees with Tools/unit_baselines.json (the
#      file the gate actually reads) and with the registry the tests declare.
#      EQUALITY, both directions -- understating is the direction drift always
#      travels, so an overshoot-only check is blind to the common case.
#
# Usage:
#   pwsh -NoProfile -File Tools/doc_lint.ps1
#   pwsh -NoProfile -File Tools/doc_lint.ps1 -Game Zenithmon
#   powershell -NoProfile -File Tools/doc_lint.ps1
#
# Exit codes:
#   0 -- all 7 checks pass.
#   1 -- one or more violations detected (each violation is printed in
#        a single grep-able line: "VIOLATION [check-id] path:line description").
#
# Written in PowerShell to match the existing Tools/ convention
# (verify_build_env.ps1, ZenithTestHarness.psm1, agent_session_close.ps1)
# rather than introduce a new Python dependency. ASCII-only body so
# PS 5.1 (default CP1252 codepage) can read it without mojibake. See
# Q-2026-05-12-005 for the parser-error history.
# =============================================================================

[CmdletBinding()]
param(
    [string]$RepoRoot = (Split-Path -Parent $PSScriptRoot),

    # Which game's Docs/ to lint. It used to be hardcoded to
    # DevilsPlayground, which meant none of these six checks had ever run
    # over Zenithmon -- and C6 (cross-references resolve) is exactly the
    # one that would have caught its dangling ZM-D-177 citation.
    [string]$Game = 'DevilsPlayground',

    # When -Verbose-equivalent: print PASS lines for each check too,
    # not just the summary. Helpful for CI logs.
    [switch]$ShowPassed
)

$ErrorActionPreference = "Stop"

$gameDir = Join-Path $RepoRoot "Games/$Game"
$docsDir = Join-Path $gameDir 'Docs'
$configDir = Join-Path $gameDir 'Config'

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
    $testsDir = Join-Path $gameDir 'Tests'
    if (-not (Test-Path $testsDir)) {
        Report-Pass 'C1' "Tests dir missing -- skipping count check"
        return
    }

    # Count ZENITH_AUTOMATED_TEST_REGISTER call sites. Each maps to one
    # registered test. Some .cpp files declare multiple tests; counting
    # registration calls is the accurate signal.
    #
    # ★ EVERY .cpp, NOT 'Test_*.cpp'. The glob used to be 'Test_*.cpp', which is
    # DevilsPlayground's convention and nobody else's: Zenithmon names its
    # suites ZM_AutoTests_*.cpp, so this check counted a registry of ZERO,
    # compared every doc claim against it, and reported PASS. It had therefore
    # never once run against the game whose docs narrate a pinned baseline --
    # which is the whole reason -Game was added.
    $registerCount = 0
    Get-ChildItem -Path $testsDir -Filter '*.cpp' -Recurse | ForEach-Object {
        $content = Get-Content $_.FullName -Raw
        $matches = [regex]::Matches($content, 'ZENITH_AUTOMATED_TEST_REGISTER\s*\(')
        $registerCount += $matches.Count
    }

    # ★ A REGISTRY OF ZERO IS A BROKEN CHECK, NOT A CLEAN ONE. A Tests/ dir that
    # exists and yields no registrations means this function is looking in the
    # wrong place or for the wrong macro -- and reporting that as a pass is the
    # exact shape of failure the check exists to catch, one level up. Every doc
    # claim would sail past a ceiling of zero.
    if ($registerCount -eq 0) {
        Report-Violation 'C1' "${Game}: Tests/ exists but no ZENITH_AUTOMATED_TEST_REGISTER call sites were found under $testsDir -- the count check cannot run, and would pass every doc claim against a ceiling of 0"
        return
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
    # DevilsPlayground-shaped: MVPScope.md and Config/Archetypes.json are
    # its files. Skipping is correct for another game; FAILING would make
    # the linter unusable anywhere else, which is how it came to be
    # pointed at one game in the first place.
    if (-not (Test-Path (Join-Path $docsDir 'MVPScope.md'))) {
        Report-Pass 'C2' "no MVPScope.md in $Game -- check does not apply"
        return
    }
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
    # `MVP-N.N.N` task ids are a DevilsPlayground convention. Zenithmon
    # identifies roadmap items by stage heading plus board key instead.
    if (-not (Test-Path (Join-Path $docsDir 'MvpRoadmap.md'))) {
        Report-Pass 'C3' "no MvpRoadmap.md in $Game -- check does not apply"
        return
    }
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
                # A line/anchor suffix is a pointer into the file, not
                # part of its path: `Foo.cpp:692` names line 692 of Foo.cpp.
                $bare = $target -replace ':\d+$', ''
                # Two conventions coexist in these docs and BOTH are
                # correct: a sibling doc is named relatively
                # (`DecisionLog.md`), while source is named from the REPO
                # ROOT (`Zenith/Flux/Terrain/Flux_TerrainConfig.h`).
                # Resolving only against the doc's directory reported 17
                # perfectly good source links as broken the first time
                # this linter was pointed at Zenithmon.
                try {
                    $candidates = @(
                        [System.IO.Path]::GetFullPath((Join-Path $dir $bare)),
                        [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $bare))
                    )
                    $found = $false
                    foreach ($candidate in $candidates) {
                        if (Test-Path -LiteralPath $candidate) { $found = $true; break }
                    }
                    if (-not $found) {
                        Report-Violation 'C6' "${name}:$($i+1): broken markdown link: '$target' (tried $($candidates -join ' and '))"
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
# Check 7: the LIVE PIN block agrees with Tools/unit_baselines.json and with the
#          registry the tests actually declare.
#
# ★★ WHY THIS EXISTS. `Status.md` carries DATA as well as narration, in four
# independent places -- the LIVE PIN line, the `+N` history chain, the
# committed-asset SHA256 table, and the STATE block -- and NOTHING compared any
# of them against the file the gate actually reads. Measured drift, all live at
# once on 2026-08-23:
#
#   * LIVE PIN read `3387` while `Tools/unit_baselines.json` held `3388`. Traced
#     to 28046d81, which bumped the manifest 3384 -> 3388 (a +4) while its prose
#     said "+3" and landed on 3387: the manifest took the MEASURED number and
#     the sentence took ARITHMETIC, which the manifest's own `$never` note
#     forbids in those words.
#   * The STATE block read `pin 3345, registry 64` -- 44 units stale -- and
#     claimed master was "PUSHED", a state this repo never reaches.
#
# C1 cannot catch either. It only fires when a doc OVERSTATES a "N tests"
# claim, and `registry **N**` matches none of its regexes. So the LIVE PIN line
# -- the one line this file tells every cold session to read first -- was the
# least checked number in the repository.
#
# ★ SCOPED TO THE LIVE PIN BLOCK ON PURPOSE. Historical narration ("registry
# 65 -> **67**", "3384 -> 3388") is legitimately full of superseded numbers; an
# equality check over the whole file would red on its own history. One home per
# value means exactly one line is authoritative, and this checks that one.
# =============================================================================
function Check-LivePin {
    $statusPath = Join-Path $docsDir 'Status.md'
    if (-not (Test-Path $statusPath)) {
        Report-Pass 'C7' "no Status.md -- skipping live-pin check"
        return
    }
    $manifestPath = Join-Path $repoRoot 'Tools/unit_baselines.json'
    if (-not (Test-Path $manifestPath)) {
        Report-Violation 'C7' "Tools/unit_baselines.json not found at $manifestPath -- the pin has no authority to compare against"
        return
    }

    $lines = Get-Content $statusPath
    $pinIdx = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i] -match 'LIVE PIN') { $pinIdx = $i; break }
    }
    if ($pinIdx -lt 0) {
        Report-Pass 'C7' "no LIVE PIN block in Status.md -- nothing to reconcile"
        return
    }

    # The block is the marker line plus the next few; the numbers usually sit on
    # the line after the marker.
    $block = ($lines[$pinIdx..([Math]::Min($pinIdx + 3, $lines.Length - 1))]) -join "`n"

    $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
    $expected = $null
    if ($manifest.baselines.PSObject.Properties[$Game]) {
        $expected = [int]$manifest.baselines.$Game
    }

    $ok = $true

    # --- the boot pin -------------------------------------------------------
    if ($null -ne $expected) {
        if ($block -match '(?im)boot\s+`?(\d{3,6})`?') {
            $claimed = [int]$Matches[1]
            if ($claimed -ne $expected) {
                Report-Violation 'C7' "Status.md:$($pinIdx+2): LIVE PIN says ${Game} boot $claimed but Tools/unit_baselines.json -- the file the gate reads -- says $expected. One home per value; the manifest is it."
                $ok = $false
            }
        } else {
            Report-Violation 'C7' "Status.md:$($pinIdx+1): a LIVE PIN block exists but no boot number could be parsed from it, so the pin narration is unchecked"
            $ok = $false
        }
    }

    # --- the registry -------------------------------------------------------
    # EQUALITY, both directions. Understating is the direction drift always
    # travels -- every new test raises the true count while the prose stays put
    # -- so a check that only catches overstatement is blind to the common case.
    $testsDir = Join-Path $gameDir 'Tests'
    if (Test-Path $testsDir) {
        $registry = 0
        Get-ChildItem -Path $testsDir -Filter '*.cpp' -Recurse | ForEach-Object {
            $registry += ([regex]::Matches((Get-Content $_.FullName -Raw), 'ZENITH_AUTOMATED_TEST_REGISTER\s*\(')).Count
        }
        if ($registry -gt 0 -and $block -match '(?im)registry\s*\*{0,2}\s*(\d{1,4})\s*\*{0,2}') {
            $claimedReg = [int]$Matches[1]
            if ($claimedReg -ne $registry) {
                Report-Violation 'C7' "Status.md:$($pinIdx+2): LIVE PIN says registry $claimedReg but $registry ZENITH_AUTOMATED_TEST_REGISTER call sites exist under $testsDir"
                $ok = $false
            }
        }
    }

    if ($ok) { Report-Pass 'C7' "LIVE PIN agrees with unit_baselines.json and the registry" }
}

# =============================================================================
# Run all checks.
# =============================================================================
Write-Host "doc_lint.ps1: running 7 checks against $docsDir" -ForegroundColor Cyan
Check-TestCount
Check-MvpArchetypeNames
Check-RoadmapUniqueIds
Check-SupersededMarkers
Check-StaleClaims
Check-MarkdownLinks
Check-LivePin

Write-Host ""
if ($script:violations -eq 0) {
    Write-Host "doc_lint.ps1: ALL CHECKS PASS" -ForegroundColor Green
    exit 0
} else {
    Write-Host "doc_lint.ps1: $($script:violations) violation(s) found" -ForegroundColor Red
    exit 1
}
