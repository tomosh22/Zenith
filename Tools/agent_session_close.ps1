# =============================================================================
# agent_session_close.ps1 -- MVP-0.3.1 session-end helper.
#
# Agents call this at the end of an orchestrator session to:
#   1. Update Games/DevilsPlayground/Docs/Status.md's "Last completed"
#      bullet with the just-finished task id + a one-line summary.
#   2. Append a DecisionLog.md entry with the same summary.
#   3. Print the next un-checked roadmap task so the next session has
#      a clear handoff.
#
# Usage:
#   pwsh -NoProfile -File Tools/agent_session_close.ps1 `
#       -TaskId MVP-0.2.4 `
#       -Summary "Test_P3Inventory_ArchetypeCount asserts 24-roster + MVP-4 instantiation"
#
# Idempotency: the script appends rather than replaces. Running twice with
# the same -TaskId produces two DecisionLog entries -- by design (each
# session's append captures the wall-clock time the work landed).
#
# ASCII-only body so it parses under both PowerShell 5.1 (powershell.exe)
# and 7.x (pwsh.exe). See Q-2026-05-12-005.
# =============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$TaskId,

    [Parameter(Mandatory = $true)]
    [string]$Summary,

    # Repo root (the directory containing Games/DevilsPlayground/). Defaults
    # to the script's grandparent (Tools/.. = repo root).
    [string]$RepoRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = "Stop"

$statusPath     = Join-Path $RepoRoot 'Games/DevilsPlayground/Docs/Status.md'
$decisionPath   = Join-Path $RepoRoot 'Games/DevilsPlayground/Docs/DecisionLog.md'
$roadmapPath    = Join-Path $RepoRoot 'Games/DevilsPlayground/Docs/MvpRoadmap.md'

if (-not (Test-Path $statusPath))   { Write-Error "Status.md not found at $statusPath";   exit 1 }
if (-not (Test-Path $decisionPath)) { Write-Error "DecisionLog.md not found at $decisionPath"; exit 1 }
if (-not (Test-Path $roadmapPath))  { Write-Error "MvpRoadmap.md not found at $roadmapPath";  exit 1 }

$today = (Get-Date).ToString("yyyy-MM-dd")

# ----- 1) Update Status.md ---------------------------------------------------
# Insert a new "Last completed" bullet immediately under the "## Last completed"
# heading. Preserves the rest of the file untouched. If no such heading exists,
# fall back to appending at end of file (and warn).
$statusContent = Get-Content $statusPath -Raw
$newBullet = "- **$TaskId** ($today) -- $Summary"
$headingPattern = '(?m)^## Last completed\s*$'

if ($statusContent -match $headingPattern) {
    # Insert the new bullet right after the heading line.
    $updated = $statusContent -replace $headingPattern, "## Last completed`n`n$newBullet"
    Set-Content -Path $statusPath -Value $updated -NoNewline
    Write-Host "Status.md: prepended '$TaskId' to Last completed" -ForegroundColor Cyan
} else {
    Write-Warning "Status.md has no '## Last completed' heading; appending bullet at end."
    Add-Content -Path $statusPath -Value "`n$newBullet"
}

# ----- 2) Append to DecisionLog.md -------------------------------------------
# Insert a new dated entry at the top (just under the "---" separator that
# follows the file header). Newest entries go first per the file's own
# format note.
$decisionContent = Get-Content $decisionPath -Raw
$entry = @"
## $today -- $TaskId

$Summary

---

"@

# Find the first "---" separator after the file header and insert the new
# entry right after it. If no separator found, prepend to the body.
$lines = $decisionContent -split "`n"
$insertIdx = -1
for ($i = 0; $i -lt $lines.Length; $i++) {
    if ($lines[$i].Trim() -eq '---') {
        $insertIdx = $i + 1
        break
    }
}

if ($insertIdx -ge 0) {
    $before = $lines[0..$insertIdx] -join "`n"
    $after  = if ($insertIdx + 1 -lt $lines.Length) { $lines[($insertIdx + 1)..($lines.Length - 1)] -join "`n" } else { "" }
    $newContent = $before + "`n" + $entry + $after
    Set-Content -Path $decisionPath -Value $newContent -NoNewline
    Write-Host "DecisionLog.md: appended entry for '$TaskId'" -ForegroundColor Cyan
} else {
    Write-Warning "DecisionLog.md has no '---' separator after header; appending at end."
    Add-Content -Path $decisionPath -Value $entry
}

# ----- 3) Print next un-checked roadmap task ---------------------------------
$roadmap = Get-Content $roadmapPath
$nextLine = $null
foreach ($line in $roadmap) {
    # Match unchecked bullets like "- [ ] **MVP-X.Y.Z** -- description"
    if ($line -match '^\- \[ \] \*\*(MVP-\d+\.\d+\.\d+)\*\*\s*--\s*(.+?)\s*$' -or
        $line -match '^\- \[ \] \*\*(MVP-\d+\.\d+\.\d+)\*\*\s*[—-]+\s*(.+?)\s*$') {
        $nextLine = "$($matches[1]) -- $($matches[2])"
        break
    }
}

Write-Host ""
if ($null -ne $nextLine) {
    Write-Host "Next un-checked roadmap task: $nextLine" -ForegroundColor Green
} else {
    Write-Host "No un-checked tasks found in MvpRoadmap.md (Phase 0 complete?)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "agent_session_close.ps1: done." -ForegroundColor Cyan
exit 0
