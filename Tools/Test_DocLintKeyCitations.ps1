# Test_DocLintKeyCitations.ps1 -- assertions for doc_lint.ps1's C9 check
# (bare bracketed ticket-key citations resolve to a real ticket).
#
# A plain assert script, matching Tools/Test_TickGates.ps1 and
# Tools/Test_UnitBaselineManifest.ps1: the only PowerShell this repo requires
# is the `pwsh` every gate already needs.
#
# doc_lint.ps1 runs all of its checks unconditionally at the bottom of the
# file and then calls `exit` -- there is no flag to stop it short, and this
# ticket's boundary forbids re-plumbing that invocation. So this harness
# treats doc_lint.ps1 as a BLACK BOX: it builds a small fixture repo (a
# ticket registry plus one Games/<Game>/Docs/Notes.md per scenario) and
# invokes `pwsh -File doc_lint.ps1 -RepoRoot <fixture> -Game <scenario>` in a
# child process, then asserts on its stdout/stderr text and exit code. Every
# fixture doc is built to avoid tripping C1-C8 (no Tests/, MVPScope.md,
# MvpRoadmap.md, [SUPERSEDED] text, "does not exist" claims, Status.md, or
# ArtBrief.md), so a clean run's exit code is a meaningful signal about C9
# alone, not noise from an unrelated check.
#
# Usage: pwsh -NoProfile -File Tools/Test_DocLintKeyCitations.ps1
# Exit:  0 all pass, 1 otherwise.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$docLintPath = Join-Path $PSScriptRoot 'doc_lint.ps1'

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

# One fixture root, reused across scenarios: the ticket registry is the same
# for all of them (a single known ticket, ZM-50), and each scenario gets its
# own Games/<Game>/Docs so the doc content under test never collides with
# another scenario's.
function New-FixtureRoot {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("doclint_c9_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path (Join-Path $dir 'Zenith/Docs/Board/tickets') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $dir 'Zenith/Docs/Board/tickets/ZM-50.md') -Value '# fixture ticket' -Encoding utf8
    return $dir
}

# Runs doc_lint.ps1 against one scenario: a fresh Games/<GameName>/Docs
# containing a single Notes.md with the given content.
function Invoke-DocLintScenario {
    param([string]$Root, [string]$GameName, [string]$NotesContent)
    $docsDir = Join-Path $Root "Games/$GameName/Docs"
    New-Item -ItemType Directory -Path $docsDir -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $docsDir 'Notes.md') -Value $NotesContent -Encoding utf8
    $output = & pwsh -NoProfile -File $docLintPath -RepoRoot $Root -Game $GameName -ShowPassed 2>&1 | Out-String
    return [PSCustomObject]@{ Output = $output; ExitCode = $LASTEXITCODE }
}

$fixtureRoot = New-FixtureRoot
try {

    Write-Host "`n=== dangling key IS reported (this is the assertion that dies without C9) ===" -ForegroundColor Cyan

    # ** THE LOAD-BEARING ASSERTION, and DELIBERATELY the one single-line
    # fixture left in this file. `Get-Content` returns a bare String (not a
    # 1-element array) for a one-line file, so the check's line-indexed loop
    # silently iterates CHARACTERS instead of lines unless the call is
    # @()-wrapped -- this fixture is what pins that fix. Every other
    # scenario below is multi-line so it cannot pass vacuously the same way.
    Assert-That '** a bare [ZM-999] with no matching ticket file is reported as C9 (single-line fixture -- pins the Get-Content @() wrap)' {
        $r = Invoke-DocLintScenario -Root $fixtureRoot -GameName 'Dangling' -NotesContent 'See [ZM-999] for detail.'
        ($r.ExitCode -eq 1) -and ($r.Output -match 'VIOLATION \[C9\][^\r\n]*ZM-999')
    }

    Write-Host "`n=== a resolving key is NOT reported ===" -ForegroundColor Cyan

    Assert-That 'a bare [ZM-50], which DOES resolve, is not reported -- alongside a dangling control key that IS, so this fails if the doc were never actually scanned' {
        # The control (ZM-800, on its own line) is the point: without it,
        # a check that reports NOTHING on every input would pass this
        # assertion just as well as a working one.
        $content = @'
See [ZM-50] for detail, which resolves and must not be reported.

A dangling control on another line: [ZM-800] must be reported.
'@
        $r = Invoke-DocLintScenario -Root $fixtureRoot -GameName 'Resolves' -NotesContent $content
        ($r.ExitCode -eq 1) -and ($r.Output -notmatch "cites 'ZM-50'") -and ($r.Output -match "cites 'ZM-800'")
    }

    Write-Host "`n=== suppressed shapes: fenced block, inline span, decision key, link text ===" -ForegroundColor Cyan

    Assert-That 'a dangling key inside a FENCED code block (```) is not reported, alongside an unfenced control key that is' {
        $content = @'
Example citation shape:

```text
[ZM-999]
```

A dangling control, not fenced: [ZM-800] must be reported.
'@
        $r = Invoke-DocLintScenario -Root $fixtureRoot -GameName 'Fenced' -NotesContent $content
        ($r.ExitCode -eq 1) -and ($r.Output -notmatch "cites 'ZM-999'") -and ($r.Output -match "cites 'ZM-800'")
    }

    Assert-That 'a dangling key inside a ~~~ fenced block is not reported (both fence characters, not just backtick), alongside an unfenced control key that is' {
        $content = @'
Example citation shape:

~~~text
[ZM-999]
~~~

A dangling control, not fenced: [ZM-800] must be reported.
'@
        $r = Invoke-DocLintScenario -Root $fixtureRoot -GameName 'FencedTilde' -NotesContent $content
        ($r.ExitCode -eq 1) -and ($r.Output -notmatch "cites 'ZM-999'") -and ($r.Output -match "cites 'ZM-800'")
    }

    Assert-That 'a dangling key inside an INLINE code span is not reported, alongside a control key outside any span that is' {
        $content = @'
The shape is `[ZM-999]`, written literally as an example.

A dangling control, not in a code span: [ZM-800] must be reported.
'@
        $r = Invoke-DocLintScenario -Root $fixtureRoot -GameName 'InlineSpan' -NotesContent $content
        ($r.ExitCode -eq 1) -and ($r.Output -notmatch "cites 'ZM-999'") -and ($r.Output -match "cites 'ZM-800'")
    }

    Assert-That 'a DECISION key (ZM-D-203) is never reported as a dangling ticket, alongside a control key that is' {
        $content = @'
Related decision [ZM-D-203] explains why this shipped.

A dangling control: [ZM-800] must be reported.
'@
        $r = Invoke-DocLintScenario -Root $fixtureRoot -GameName 'DecisionKey' -NotesContent $content
        ($r.ExitCode -eq 1) -and ($r.Output -notmatch "cites 'ZM-D-203'") -and ($r.Output -match "cites 'ZM-800'")
    }

    Assert-That 'a dangling key inside a markdown LINK''s bracket text is C6''s business, not C9''s -- alongside a control key that C9 reports' {
        # [ZM-999](https://example.invalid/ZM-999) is a real link. C6 itself
        # skips it (it explicitly ignores http(s)/mailto targets), and C9
        # must not report the bracket text either -- reporting it here would
        # be a second, weaker copy of what C6 already owns.
        $content = @'
See [ZM-999](https://example.invalid/ZM-999) for detail.

A dangling control, not a link: [ZM-800] must be reported.
'@
        $r = Invoke-DocLintScenario -Root $fixtureRoot -GameName 'LinkText' -NotesContent $content
        ($r.ExitCode -eq 1) -and ($r.Output -notmatch "cites 'ZM-999'") -and ($r.Output -match "cites 'ZM-800'") -and ($r.Output -notmatch 'VIOLATION \[C6\]')
    }

    Write-Host "`n=== multiple keys in one bracket, both separators ===" -ForegroundColor Cyan

    Assert-That 'a "/"-separated bracket reports only the dangling half, not the resolving one' {
        # NOTE: the violation message quotes the WHOLE bracket, so
        # '[ZM-50 / ZM-999]' legitimately contains the substring "ZM-50"
        # even when only ZM-999 is the thing being reported. Check the
        # "cites '<key>'" phrase specifically, not bare substring presence.
        # Multi-line so this cannot pass vacuously on the scalar Get-Content
        # bug -- the load-bearing single-line fixture above already pins that.
        $content = @'
Filler line for context.

See [ZM-50 / ZM-999] for both the shipped and unfiled halves.
'@
        $r = Invoke-DocLintScenario -Root $fixtureRoot -GameName 'SlashPair' -NotesContent $content
        $violations = @([regex]::Matches($r.Output, 'VIOLATION \[C9\][^\r\n]*'))
        ($violations.Count -eq 1) -and ($violations[0].Value -match "cites 'ZM-999'") -and ($violations[0].Value -notmatch "cites 'ZM-50'")
    }

    Assert-That 'a ","-separated bracket is read the same way as a "/"-separated one' {
        $content = @'
Filler line for context.

See [ZM-50, ZM-999] for both.
'@
        $r = Invoke-DocLintScenario -Root $fixtureRoot -GameName 'CommaPair' -NotesContent $content
        $violations = @([regex]::Matches($r.Output, 'VIOLATION \[C9\][^\r\n]*'))
        ($violations.Count -eq 1) -and ($violations[0].Value -match "cites 'ZM-999'") -and ($violations[0].Value -notmatch "cites 'ZM-50'")
    }

    Write-Host "`n=== the check id and location are grep-able, like every other check here ===" -ForegroundColor Cyan

    Assert-That 'the violation line names the file and 1-based line number, matching the house style' {
        $content = @'
line one is filler

See [ZM-999] here on line three.
'@
        $r = Invoke-DocLintScenario -Root $fixtureRoot -GameName 'LineNumber' -NotesContent $content
        $r.Output -match 'Notes\.md:3: cites .ZM-999.'
    }

} finally {
    Remove-Item -LiteralPath $fixtureRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
