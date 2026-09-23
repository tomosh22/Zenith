# Test_LanePins.ps1 -- assertions for Tools/lane_pins.ps1.
#
# A plain assert script, matching Tools/Test_TickGates.ps1's style: counts,
# "N/N assertions passed", non-zero exit on failure. Tools/lane_pins.ps1 is
# invoked as a CHILD PROCESS (`pwsh -File`) against throwaway repos under
# $env:TEMP, never dot-sourced -- it calls `exit`, and dot-sourcing it would
# kill this test run the first time it hit a Fail path.
#
# Every fixture uses EXPLICIT branch names (`git checkout -b base/ours/theirs`)
# rather than relying on `init.defaultBranch`, and `core.autocrlf=false` so the
# mixed CRLF/LF bytes a fixture writes survive commit and checkout unchanged.
#
# Usage: pwsh -NoProfile -File Tools/Test_LanePins.ps1
# Exit:  0 all pass, 1 otherwise.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$lanePinsScript = Join-Path $PSScriptRoot 'lane_pins.ps1'
$docLintScript = Join-Path $PSScriptRoot 'doc_lint.ps1'

$script:count = 0
$script:failures = 0
function Assert-That {
    param([string]$Name, [scriptblock]$Body)
    $script:count++
    $ok = $false
    try { $ok = [bool](& $Body) } catch { Write-Host "       threw: $($_.Exception.Message)" -ForegroundColor Yellow }
    if ($ok) { Write-Host "  ok   $Name" -ForegroundColor Green }
    else { Write-Host "  FAIL $Name" -ForegroundColor Red; $script:failures++ }
}

function Test-LanePinsTextEqual {
    # Ordinal, case-sensitive comparison -- PowerShell's `-eq` on strings is
    # case-INSENSITIVE by default, which would mask a byte-fidelity bug here.
    param([string]$A, [string]$B)
    return [string]::Equals($A, $B, [System.StringComparison]::Ordinal)
}

function Write-LanePinsTestFileUtf8NoBom {
    param([string]$Path, [string]$Text)
    $parent = Split-Path -Parent $Path
    if ($parent -and -not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
    $enc = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($Path, $Text, $enc)
}

function Get-LanePinsTestGitShowRaw {
    # Byte-accurate `git show <ref>:<path>` via shell redirection, mirroring
    # how lane_pins.ps1 itself reads blobs -- so the "byte-identical" checks
    # below are comparing against the same ground truth lane_pins.ps1 reads.
    param([string]$RepoDir, [string]$Ref, [string]$Path)
    $tmp = Join-Path ([IO.Path]::GetTempPath()) ("lanepinstest_" + [guid]::NewGuid().ToString('N') + '.txt')
    try {
        cmd /c "git -C `"$RepoDir`" show `"${Ref}:${Path}`" > `"$tmp`"" 2>$null
        return [IO.File]::ReadAllText($tmp)
    }
    finally {
        Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue
    }
}

function New-LanePinsTestRepo {
    $dir = Join-Path ([IO.Path]::GetTempPath()) ("lanepins_" + [guid]::NewGuid().ToString('N').Substring(0, 10))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    & git -C $dir init --quiet 2>$null | Out-Null
    & git -C $dir config core.autocrlf false 2>$null | Out-Null
    & git -C $dir config user.email 'test@example.invalid' 2>$null | Out-Null
    & git -C $dir config user.name 'test' 2>$null | Out-Null
    # Pinned so the suite is deterministic whatever the machine's global git
    # config says -- a personal/global merge.conflictStyle=diff3 (or zdiff3)
    # would otherwise change the shape of every fixture below.
    & git -C $dir config merge.conflictStyle merge 2>$null | Out-Null
    & git -C $dir checkout -q -b base 2>$null | Out-Null
    return $dir
}

function New-LanePinsTestCommit {
    param([string]$Dir, [string]$Message)
    & git -C $Dir add -A 2>$null | Out-Null
    & git -C $Dir commit -q -m $Message 2>$null | Out-Null
}

function New-LanePinsTestConflict {
    # Branches 'ours' and 'theirs' off the current (already-committed) 'base',
    # applies each side's edits (scriptblocks taking the repo dir as their one
    # parameter), commits both, checks out 'ours', and attempts
    # `git merge --no-ff theirs`. Leaves the repo mid-merge; does not assert
    # the merge actually conflicted (callers check MERGE_HEAD / file state).
    param([string]$Dir, [scriptblock]$ApplyOurs, [scriptblock]$ApplyTheirs, [string]$ConflictStyle = $null)
    & git -C $Dir checkout -q -b ours 2>$null | Out-Null
    & $ApplyOurs $Dir
    New-LanePinsTestCommit -Dir $Dir -Message 'ours'
    & git -C $Dir checkout -q base 2>$null | Out-Null
    & git -C $Dir checkout -q -b theirs 2>$null | Out-Null
    & $ApplyTheirs $Dir
    New-LanePinsTestCommit -Dir $Dir -Message 'theirs'
    & git -C $Dir checkout -q ours 2>$null | Out-Null
    if ($ConflictStyle) {
        & git -C $Dir -c "merge.conflictStyle=$ConflictStyle" merge --no-ff theirs -m 'merge theirs into ours' 2>$null | Out-Null
    }
    else {
        & git -C $Dir merge --no-ff theirs -m 'merge theirs into ours' 2>$null | Out-Null
    }
}

function Invoke-LanePinsScript {
    param([string]$Repo)
    & pwsh -NoProfile -File $lanePinsScript -Repo $Repo 2>&1 | Out-Null
    return $LASTEXITCODE
}

function Set-LanePinsTestBaselineValue {
    # Textual substitution of one numeric "key": N row, mirroring the
    # structural approach lane_pins.ps1 itself uses (never a JSON round trip).
    param([string]$Text, [string]$Key, [int]$NewValue)
    $pattern = '("' + [regex]::Escape($Key) + '"\s*:\s*)(\d+)(\s*,?)'
    $rx = [regex]$pattern
    if (-not $rx.IsMatch($Text)) { throw "[Test_LanePins] fixture row '$Key' not found in baseline text" }
    return $rx.Replace($Text, [System.Text.RegularExpressions.MatchEvaluator] { param($m) $m.Groups[1].Value + $NewValue + $m.Groups[3].Value }, 1)
}

# =============================================================================
# Status.md fixture builder -- a short synthetic file with an early '## '
# heading, mixed CRLF/LF lines, a '**Last updated:**' line, one LIVE PIN pair,
# and a history area under a 'History:' line.
# =============================================================================

function New-LanePinsStatusMdText {
    param(
        [string]$GameLabel,
        [string]$LastUpdatedDate,
        [string]$LivePinDate,
        [int[]]$LivePinNumbers,
        [string[]]$HistoryLines = @(),
        [string[]]$NarrationLines = @()
    )
    # Padded with unchanged filler context lines between each conflicting
    # region -- git merges two changed hunks into ONE conflict block when
    # they sit within its diff context window of each other (default 3
    # lines), and this fixture needs THREE SEPARATE blocks (Last Updated,
    # LIVE PIN, History), matching the real Status.md's own shape (line 3,
    # lines 20-21, and the history area much further down).
    $entries = @(
        @{ T = '# Fixture Status'; N = "`n" },
        @{ T = ''; N = "`r`n" },
        @{ T = "**Last updated:** $LastUpdatedDate"; N = "`n" }
    )
    for ($i = 1; $i -le 8; $i++) { $entries += @{ T = "context filler line $i"; N = "`n" } }
    $entries += @{ T = ''; N = "`r`n" }
    $entries += @{ T = "LIVE PIN (UPDATED $LivePinDate):"; N = "`n" }
    $entries += @{ T = "$GameLabel boot ``$($LivePinNumbers[0])``; engine boot (Null Combat) ``$($LivePinNumbers[1])``; Null RenderTest ``$($LivePinNumbers[2])``; registry **$($LivePinNumbers[3])**."; N = "`r`n" }
    # The real file's small gap below the LIVE PIN pair: blank, a 2-line
    # ">  OBSERVED ..." blockquote anchor (never itself edited by a test
    # scenario -- always identical across base/ours/theirs, so it never
    # introduces a conflict of its own), then blank, then optional narration
    # paragraphs a test scenario inserts (distinct from -HistoryLines, which
    # lives in the separate History area further down).
    $entries += @{ T = ''; N = "`n" }
    $entries += @{ T = '> OBSERVED: fixture measurement anchor line one.'; N = "`n" }
    $entries += @{ T = '> fixture measurement anchor line two.'; N = "`n" }
    $entries += @{ T = ''; N = "`n" }
    foreach ($n in $NarrationLines) { $entries += @{ T = $n; N = "`n" } }
    for ($i = 9; $i -le 16; $i++) { $entries += @{ T = "context filler line $i"; N = "`n" } }
    $entries += @{ T = ''; N = "`n" }
    $entries += @{ T = 'History:'; N = "`r`n" }
    foreach ($h in $HistoryLines) { $entries += @{ T = $h; N = "`n" } }
    $entries += @{ T = '(nothing yet)'; N = "`n" }
    $entries += @{ T = ''; N = "`n" }
    $entries += @{ T = '## Detail'; N = "`r`n" }
    $entries += @{ T = 'body text below the heading, never touched'; N = "`n" }

    $sb = New-Object System.Text.StringBuilder
    foreach ($e in $entries) { [void]$sb.Append($e.T); [void]$sb.Append($e.N) }
    return $sb.ToString()
}

$today = Get-Date -Format 'yyyy-MM-dd'

# =============================================================================
Write-Host "`n=== Tools/unit_baselines.json: delta merge + byte fidelity ===" -ForegroundColor Cyan
# =============================================================================

Assert-That 'delta rule theirs+(ours-base); a row only theirs changed stays theirs''; untouched rows/bytes preserved' {
    $repo = New-LanePinsTestRepo
    try {
        $realText = [IO.File]::ReadAllText((Join-Path $repoRoot 'Tools/unit_baselines.json'))
        $realManifest = $realText | ConvertFrom-Json
        $curRenderTest = [int]$realManifest.baselines.RenderTest
        $curZenithmon = [int]$realManifest.baselines.Zenithmon
        $curCombat = [int]$realManifest.baselines.Combat

        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo 'Tools/unit_baselines.json') -Text $realText
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        $expectedRenderTest = $curRenderTest + 7   # theirs(+2) + (ours(+5) - base(+0))
        $expectedZenithmon = $curZenithmon + 6      # ours unchanged from base -> theirs'

        New-LanePinsTestConflict -Dir $repo `
            -ApplyOurs {
            param($Dir)
            $p = Join-Path $Dir 'Tools/unit_baselines.json'
            $t = [IO.File]::ReadAllText($p)
            $t = Set-LanePinsTestBaselineValue -Text $t -Key 'RenderTest' -NewValue ($curRenderTest + 5)
            Write-LanePinsTestFileUtf8NoBom -Path $p -Text $t
        } `
            -ApplyTheirs {
            param($Dir)
            $p = Join-Path $Dir 'Tools/unit_baselines.json'
            $t = [IO.File]::ReadAllText($p)
            $t = Set-LanePinsTestBaselineValue -Text $t -Key 'RenderTest' -NewValue ($curRenderTest + 2)
            $t = Set-LanePinsTestBaselineValue -Text $t -Key 'Zenithmon' -NewValue ($curZenithmon + 6)
            Write-LanePinsTestFileUtf8NoBom -Path $p -Text $t
        }

        $exit = Invoke-LanePinsScript -Repo $repo

        $resultText = [IO.File]::ReadAllText((Join-Path $repo 'Tools/unit_baselines.json'))
        $resultManifest = $resultText | ConvertFrom-Json

        $theirsCommittedText = Get-LanePinsTestGitShowRaw -RepoDir $repo -Ref 'theirs' -Path 'Tools/unit_baselines.json'
        $expectedText = Set-LanePinsTestBaselineValue -Text $theirsCommittedText -Key 'RenderTest' -NewValue $expectedRenderTest

        ($exit -eq 0) -and
        ($resultManifest.baselines.RenderTest -eq $expectedRenderTest) -and
        ($resultManifest.baselines.Zenithmon -eq $expectedZenithmon) -and
        ($resultManifest.baselines.Combat -eq $curCombat) -and
        (Test-LanePinsTextEqual -A $resultText -B $expectedText)
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

# =============================================================================
Write-Host "`n=== Games/<G>/Docs/Status.md: union + single recomputed LIVE PIN ===" -ForegroundColor Cyan
# =============================================================================

Assert-That 'Last updated -> today; exactly one LIVE PIN pair, delta-merged + (UPDATED today); both +N entries, ours first; untouched lines byte-identical' {
    $repo = New-LanePinsTestRepo
    try {
        $relPath = 'Games/Fixture/Docs/Status.md'
        $baseText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(100, 200, 300, 10)
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo $relPath) -Text $baseText
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        New-LanePinsTestConflict -Dir $repo `
            -ApplyOurs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-05' -LivePinDate '2026-09-05' -LivePinNumbers @(105, 200, 300, 10) -HistoryLines @('+1 ours entry')
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        } `
            -ApplyTheirs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-06' -LivePinDate '2026-09-06' -LivePinNumbers @(102, 200, 305, 12) -HistoryLines @('+1 theirs entry')
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        }

        $exit = Invoke-LanePinsScript -Repo $repo

        $resultText = [IO.File]::ReadAllText((Join-Path $repo $relPath))
        $expectedText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate $today -LivePinDate $today -LivePinNumbers @(107, 200, 305, 12) -HistoryLines @('+1 ours entry', '+1 theirs entry')

        ($exit -eq 0) -and (Test-LanePinsTextEqual -A $resultText -B $expectedText)
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

# =============================================================================
Write-Host "`n=== D1: same-day LIVE PIN header (identical both sides), numbers-only conflict ===" -ForegroundColor Cyan
# =============================================================================
# When two lanes bump pins the same day the header line is byte-identical on
# both branches, so git narrows the conflict to the numbers line alone and the
# header sits OUTSIDE the <<<<<<< / ======= / >>>>>>> span entirely.

Assert-That 'identical header date both sides: single numbers line delta-merged (4600/2739/2842/116), header date -> today, real doc_lint C7 passes' {
    $repo = New-LanePinsTestRepo
    try {
        $relPath = 'Games/Fixture/Docs/Status.md'
        $baseText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(4594, 2737, 2840, 116)
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo $relPath) -Text $baseText
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo 'Tools/unit_baselines.json') -Text '{ "baselines": { "Fixture": 4600 } }'
        Copy-Item -LiteralPath $docLintScript -Destination (Join-Path $repo 'Tools/doc_lint.ps1') -Force
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        New-LanePinsTestConflict -Dir $repo `
            -ApplyOurs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(4596, 2739, 2842, 116)
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        } `
            -ApplyTheirs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(4598, 2737, 2840, 116)
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        }

        $exit = Invoke-LanePinsScript -Repo $repo
        if ($exit -ne 0) { return $false }

        $resultText = [IO.File]::ReadAllText((Join-Path $repo $relPath))
        $expectedText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate $today -LivePinNumbers @(4600, 2739, 2842, 116)
        if (-not (Test-LanePinsTextEqual -A $resultText -B $expectedText)) { return $false }

        $docLintOut = & pwsh -NoProfile -File (Join-Path $repo 'Tools/doc_lint.ps1') -RepoRoot $repo -Game Fixture -ShowPassed 2>&1 | Out-String
        ($docLintOut -match 'PASS \[C7\]') -and ($docLintOut -notmatch 'VIOLATION \[C7\]')
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'D1 + D3: same numbers-only conflict, both sides also add a distinct narration paragraph -> two conflict blocks, narration separated by one blank line, ours first' {
    $repo = New-LanePinsTestRepo
    try {
        $relPath = 'Games/Fixture/Docs/Status.md'
        $baseText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(4594, 2737, 2840, 116)
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo $relPath) -Text $baseText
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        New-LanePinsTestConflict -Dir $repo `
            -ApplyOurs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(4596, 2739, 2842, 116) -NarrationLines @('> **+1 ours narration**')
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        } `
            -ApplyTheirs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(4598, 2737, 2840, 116) -NarrationLines @('> **+1 theirs narration**')
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        }

        $mergedText = [IO.File]::ReadAllText((Join-Path $repo $relPath))
        $conflictBlockCount = ([regex]::Matches($mergedText, '(?m)^<<<<<<< ')).Count
        if ($conflictBlockCount -ne 2) { return $false }

        $exit = Invoke-LanePinsScript -Repo $repo
        if ($exit -ne 0) { return $false }

        $resultText = [IO.File]::ReadAllText((Join-Path $repo $relPath))
        $expectedText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate $today -LivePinNumbers @(4600, 2739, 2842, 116) -NarrationLines @('> **+1 ours narration**', '', '> **+1 theirs narration**')

        Test-LanePinsTextEqual -A $resultText -B $expectedText
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'only one side inserts narration (a pure one-sided insertion, auto-merges outside any conflict block) -> no blank-line insertion artefacts' {
    $repo = New-LanePinsTestRepo
    try {
        $relPath = 'Games/Fixture/Docs/Status.md'
        $baseText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(100, 200, 300, 10)
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo $relPath) -Text $baseText
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        New-LanePinsTestConflict -Dir $repo `
            -ApplyOurs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-05' -LivePinDate '2026-09-05' -LivePinNumbers @(105, 200, 300, 10) -HistoryLines @('+1 ours entry') -NarrationLines @('> **+1 ours narration**')
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        } `
            -ApplyTheirs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-06' -LivePinDate '2026-09-06' -LivePinNumbers @(102, 200, 305, 12) -HistoryLines @('+1 theirs entry')
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        }

        $exit = Invoke-LanePinsScript -Repo $repo
        if ($exit -ne 0) { return $false }

        $resultText = [IO.File]::ReadAllText((Join-Path $repo $relPath))
        $expectedText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate $today -LivePinDate $today -LivePinNumbers @(107, 200, 305, 12) -HistoryLines @('+1 ours entry', '+1 theirs entry') -NarrationLines @('> **+1 ours narration**')

        Test-LanePinsTextEqual -A $resultText -B $expectedText
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

# =============================================================================
Write-Host "`n=== Mandatory: the resolved Status.md passes the REAL doc_lint C7 check ===" -ForegroundColor Cyan
# =============================================================================
# A minimal fixture -- Games/<Fixture>/Docs/Status.md + a matching
# Tools/unit_baselines.json row, no Tests/ dir (so C1 and C7's registry half
# both no-op) -- run against the REAL Tools/doc_lint.ps1 as a child process.
# Asserted on the C7 line specifically, never on the overall exit code: other
# checks (e.g. C6, cross-references) can legitimately fire on a sparse
# fixture, and doc_lint's own exit code does not distinguish which check
# failed. This verifies only the boot pin for -Game and would leave the
# engine-boot/RenderTest/registry numbers unchecked by C7 -- see doc_lint.ps1
# Check-LivePin, which parses only the 'boot `N`' field and (with a Tests/
# dir present) the registry count.

Assert-That 'resolved LIVE PIN boot number agrees with unit_baselines.json under the real doc_lint.ps1 C7 check' {
    $repo = New-LanePinsTestRepo
    try {
        $relPath = 'Games/Fixture/Docs/Status.md'
        $baseText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(100, 200, 300, 10)
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo $relPath) -Text $baseText
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo 'Tools/unit_baselines.json') -Text '{ "baselines": { "Fixture": 107 } }'
        Copy-Item -LiteralPath $docLintScript -Destination (Join-Path $repo 'Tools/doc_lint.ps1') -Force
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        New-LanePinsTestConflict -Dir $repo `
            -ApplyOurs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-05' -LivePinDate '2026-09-05' -LivePinNumbers @(105, 200, 300, 10)
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        } `
            -ApplyTheirs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-06' -LivePinDate '2026-09-06' -LivePinNumbers @(102, 200, 300, 10)
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        }

        $exit = Invoke-LanePinsScript -Repo $repo
        if ($exit -ne 0) { return $false }

        # -ShowPassed: doc_lint.ps1 prints an individual "PASS [id]" line for a
        # passing check only under this switch; without it, a passing check
        # is silent and only violations are printed.
        $docLintOut = & pwsh -NoProfile -File (Join-Path $repo 'Tools/doc_lint.ps1') -RepoRoot $repo -Game Fixture -ShowPassed 2>&1 | Out-String

        ($docLintOut -match 'PASS \[C7\]') -and ($docLintOut -notmatch 'VIOLATION \[C7\]')
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

# =============================================================================
Write-Host "`n=== Refusal shapes: exit 2, nothing written ===" -ForegroundColor Cyan
# =============================================================================

Assert-That 'a conflict below the first ## heading -> exit 2, working file byte-unchanged' {
    $repo = New-LanePinsTestRepo
    try {
        $relPath = 'Games/Fixture/Docs/Status.md'
        $baseText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(100, 200, 300, 10)
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo $relPath) -Text $baseText
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        # Only the body line AFTER '## Detail' conflicts -- nothing above the
        # heading is touched by either side.
        New-LanePinsTestConflict -Dir $repo `
            -ApplyOurs {
            param($Dir)
            $p = Join-Path $Dir $relPath
            $t = [IO.File]::ReadAllText($p)
            $t = $t -replace 'body text below the heading, never touched', 'ours changed the body'
            Write-LanePinsTestFileUtf8NoBom -Path $p -Text $t
        } `
            -ApplyTheirs {
            param($Dir)
            $p = Join-Path $Dir $relPath
            $t = [IO.File]::ReadAllText($p)
            $t = $t -replace 'body text below the heading, never touched', 'theirs changed the body'
            Write-LanePinsTestFileUtf8NoBom -Path $p -Text $t
        }

        $onDiskBefore = [IO.File]::ReadAllText((Join-Path $repo $relPath))
        $exit = Invoke-LanePinsScript -Repo $repo
        $onDiskAfter = [IO.File]::ReadAllText((Join-Path $repo $relPath))

        ($exit -eq 2) -and (Test-LanePinsTextEqual -A $onDiskBefore -B $onDiskAfter) -and ($onDiskAfter -match '<<<<<<<')
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a LIVE PIN numbers-line conflict with an adjacent line changed too (header identical, outside) -> exit 2, byte-unchanged' {
    $repo = New-LanePinsTestRepo
    try {
        $relPath = 'Games/Fixture/Docs/Status.md'
        $baseText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(4594, 2737, 2840, 116)
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo $relPath) -Text $baseText
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        # Both sides change the numbers line AND the adjacent OBSERVED anchor
        # line, one line apart -- close enough that git merges them into a
        # single conflict block, so neither side's slice is the required
        # exactly-one-line shape.
        New-LanePinsTestConflict -Dir $repo `
            -ApplyOurs {
            param($Dir)
            $p = Join-Path $Dir $relPath
            $t = [IO.File]::ReadAllText($p)
            $t = $t -replace 'Fixture boot `4594`; engine boot \(Null Combat\) `2737`; Null RenderTest `2840`; registry \*\*116\*\*\.', 'Fixture boot `4596`; engine boot (Null Combat) `2739`; Null RenderTest `2842`; registry **116**.'
            $t = $t -replace '> OBSERVED: fixture measurement anchor line one\.', '> OBSERVED: ours changed this anchor line too.'
            Write-LanePinsTestFileUtf8NoBom -Path $p -Text $t
        } `
            -ApplyTheirs {
            param($Dir)
            $p = Join-Path $Dir $relPath
            $t = [IO.File]::ReadAllText($p)
            $t = $t -replace 'Fixture boot `4594`; engine boot \(Null Combat\) `2737`; Null RenderTest `2840`; registry \*\*116\*\*\.', 'Fixture boot `4598`; engine boot (Null Combat) `2737`; Null RenderTest `2840`; registry **116**.'
            $t = $t -replace '> OBSERVED: fixture measurement anchor line one\.', '> OBSERVED: theirs changed this anchor line too.'
            Write-LanePinsTestFileUtf8NoBom -Path $p -Text $t
        }

        $onDiskBefore = [IO.File]::ReadAllText((Join-Path $repo $relPath))
        $exit = Invoke-LanePinsScript -Repo $repo
        $onDiskAfter = [IO.File]::ReadAllText((Join-Path $repo $relPath))

        ($exit -eq 2) -and (Test-LanePinsTextEqual -A $onDiskBefore -B $onDiskAfter) -and ($onDiskAfter -match '<<<<<<<')
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'diff3-style conflict markers (merge.conflictStyle=diff3) -> exit 2, byte-unchanged' {
    $repo = New-LanePinsTestRepo
    try {
        $relPath = 'Games/Fixture/Docs/Status.md'
        $baseText = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(4594, 2737, 2840, 116)
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo $relPath) -Text $baseText
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        New-LanePinsTestConflict -Dir $repo -ConflictStyle 'diff3' `
            -ApplyOurs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(4596, 2739, 2842, 116)
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        } `
            -ApplyTheirs {
            param($Dir)
            $t = New-LanePinsStatusMdText -GameLabel 'Fixture' -LastUpdatedDate '2026-09-01' -LivePinDate '2026-09-01' -LivePinNumbers @(4598, 2737, 2840, 116)
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir $relPath) -Text $t
        }

        $onDiskBefore = [IO.File]::ReadAllText((Join-Path $repo $relPath))
        if ($onDiskBefore -notmatch '\|\|\|\|\|\|\|') { return $false }
        $exit = Invoke-LanePinsScript -Repo $repo
        $onDiskAfter = [IO.File]::ReadAllText((Join-Path $repo $relPath))

        ($exit -eq 2) -and (Test-LanePinsTextEqual -A $onDiskBefore -B $onDiskAfter)
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'an unrelated conflicted file -> exit 2, nothing written' {
    $repo = New-LanePinsTestRepo
    try {
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo 'README.md') -Text "base line`n"
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        New-LanePinsTestConflict -Dir $repo `
            -ApplyOurs {
            param($Dir)
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir 'README.md') -Text "ours line`n"
        } `
            -ApplyTheirs {
            param($Dir)
            Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $Dir 'README.md') -Text "theirs line`n"
        }

        $onDiskBefore = [IO.File]::ReadAllText((Join-Path $repo 'README.md'))
        $exit = Invoke-LanePinsScript -Repo $repo
        $onDiskAfter = [IO.File]::ReadAllText((Join-Path $repo 'README.md'))

        ($exit -eq 2) -and (Test-LanePinsTextEqual -A $onDiskBefore -B $onDiskAfter)
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a delete/modify conflict on a pin file -> exit 2' {
    $repo = New-LanePinsTestRepo
    try {
        $realText = [IO.File]::ReadAllText((Join-Path $repoRoot 'Tools/unit_baselines.json'))
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo 'Tools/unit_baselines.json') -Text $realText
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        New-LanePinsTestConflict -Dir $repo `
            -ApplyOurs {
            param($Dir)
            & git -C $Dir rm -q -- 'Tools/unit_baselines.json' 2>$null | Out-Null
        } `
            -ApplyTheirs {
            param($Dir)
            $p = Join-Path $Dir 'Tools/unit_baselines.json'
            $t = [IO.File]::ReadAllText($p)
            $t = Set-LanePinsTestBaselineValue -Text $t -Key 'Combat' -NewValue 99999
            Write-LanePinsTestFileUtf8NoBom -Path $p -Text $t
        }

        $exit = Invoke-LanePinsScript -Repo $repo
        $exit -eq 2
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'no MERGE_HEAD -> exit 2' {
    $repo = New-LanePinsTestRepo
    try {
        Write-LanePinsTestFileUtf8NoBom -Path (Join-Path $repo 'README.md') -Text "hello`n"
        New-LanePinsTestCommit -Dir $repo -Message 'base'

        $exit = Invoke-LanePinsScript -Repo $repo
        $exit -eq 2
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host ''
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) {
    Write-Host 'PASS' -ForegroundColor Green
    exit 0
}
Write-Host "$($script:failures) assertion(s) failed" -ForegroundColor Red
exit 1
