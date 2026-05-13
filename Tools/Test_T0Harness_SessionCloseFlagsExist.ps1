# Test_T0Harness_SessionCloseFlagsExist.ps1
#
# MVP-0.3.1 smoke test: proves Tools/agent_session_close.ps1 parses cleanly
# under both Windows PowerShell 5.1 and pwsh 7.x, declares the expected
# -TaskId / -Summary parameters, and exits cleanly on a dry-run invocation.
#
# Why static parse-check + dry-run: the script mutates Status.md /
# DecisionLog.md. A full invocation against the real docs would pollute
# them with test-only entries. Instead we parse-check (catches parser
# errors) AND invoke against temp-dir copies of the doc files so the
# real docs stay untouched.
#
# Usage:
#   powershell -NoProfile -File Tools/Test_T0Harness_SessionCloseFlagsExist.ps1
#   pwsh -NoProfile -File Tools/Test_T0Harness_SessionCloseFlagsExist.ps1
#
# Exit codes:
#   0 -- script parses cleanly, declares expected params, and successfully
#        mutates the temp-dir copies.
#   1 -- any of the above fail.
#
# ASCII-only body so PS 5.1 (CP1252 default) can read it without mojibake.
# See Q-2026-05-12-005.

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$script = Join-Path $PSScriptRoot 'agent_session_close.ps1'
if (-not (Test-Path $script)) {
    Write-Error "Script not found at $script"
    exit 1
}

# 1) Parse-check.
try {
    $cmd = Get-Command $script -ErrorAction Stop
} catch {
    Write-Error "Failed to parse $script -- $($_.Exception.Message)"
    exit 1
}

# 2) Param declarations.
$expected = @('TaskId', 'Summary', 'RepoRoot')
$missing = @()
foreach ($flag in $expected) {
    if (-not $cmd.Parameters.ContainsKey($flag)) {
        $missing += $flag
    }
}
if ($missing.Count -gt 0) {
    Write-Error "Missing expected parameters: $($missing -join ', ')"
    exit 1
}

# 3) Dry-run against temp-dir fixtures.
$tmpRoot = Join-Path ([System.IO.Path]::GetTempPath()) "dp_session_close_test_$([Guid]::NewGuid().ToString('N'))"
try {
    $docsDir = Join-Path $tmpRoot 'Games/DevilsPlayground/Docs'
    New-Item -ItemType Directory -Path $docsDir -Force | Out-Null

    # Minimal fixture content matching the real file shapes the script
    # expects (## Last completed heading, --- separators).
    $statusFixture = @"
# DP Status

**Last updated:** 2026-05-13 (fixture)

## Current task

(none)

## Last completed

- placeholder
"@
    Set-Content -Path (Join-Path $docsDir 'Status.md') -Value $statusFixture -NoNewline

    $decisionFixture = @"
# DP Decision Log

**Purpose:** fixture.

---

## 2026-05-13 -- placeholder

placeholder
"@
    Set-Content -Path (Join-Path $docsDir 'DecisionLog.md') -Value $decisionFixture -NoNewline

    $roadmapFixture = @"
# Devil's Playground -- MVP Roadmap

- [x] **MVP-0.3.1** -- session-end helper (done)
- [ ] **MVP-0.3.2** -- doc linter
- [ ] **MVP-0.3.3** -- git LFS
"@
    Set-Content -Path (Join-Path $docsDir 'MvpRoadmap.md') -Value $roadmapFixture -NoNewline

    # Invoke. Suppress stdout to keep the test output focused.
    & $script -TaskId 'TEST-0.0.0' -Summary 'fixture smoke run' -RepoRoot $tmpRoot | Out-Null

    # Verify side-effects.
    $statusAfter = Get-Content (Join-Path $docsDir 'Status.md') -Raw
    if ($statusAfter -notmatch 'TEST-0.0.0') {
        Write-Error "Status.md was not updated with TEST-0.0.0 bullet"
        exit 1
    }
    $decisionAfter = Get-Content (Join-Path $docsDir 'DecisionLog.md') -Raw
    if ($decisionAfter -notmatch 'TEST-0.0.0') {
        Write-Error "DecisionLog.md was not updated with TEST-0.0.0 entry"
        exit 1
    }
} finally {
    if (Test-Path $tmpRoot) {
        Remove-Item -Recurse -Force $tmpRoot -ErrorAction SilentlyContinue
    }
}

Write-Host "Test_T0Harness_SessionCloseFlagsExist: PASS" -ForegroundColor Green
exit 0
