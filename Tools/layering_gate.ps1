# =============================================================================
# layering_gate.ps1 -- Wave-8 enforced-layering ratchet (wave8.7-layering-gate).
#
# Zenith's EntityComponent (ECS) and Flux (renderer) layers SHOULD be
# decoupled, but a known set of cross-layer #include edges exists today
# (components that own Flux instances; renderers that read ECS components).
# This gate is a RATCHET: it captures that known set in a checked-in
# allowlist (Tools/layering_allowlist.txt) and FAILS (exit 1) if any NEW
# cross-layer include edge appears that is not on the list.
#
# Later decoupling waves can only SHRINK the coupling: removing a leak is
# always allowed (the now-stale allowlist line is reported as INFO, not a
# failure), but ADDING a new Flux<->EntityComponent edge breaks the build
# until it is either undone or -- deliberately -- added to the allowlist.
#
# What counts as a cross-layer edge:
#   * a source file under Zenith/EntityComponent that #include "Flux/..."
#   * a source file under Zenith/Flux         that #include "EntityComponent/..."
# Source files = *.h, *.cpp, *.inl. Only real (uncommented) #include lines
# count: the include directive must be the first non-whitespace token on the
# line, so "// #include \"Flux/...\"" in prose/comments is ignored.
#
# Edge wire format (one per line, also the allowlist format):
#   <repo-relative-source> => <included-path>
# e.g.  Zenith/EntityComponent/Components/Zenith_ModelComponent.h => Flux/MeshGeometry/Flux_MeshGeometry.h
#
# Usage (robust to cwd -- repo root is derived from the script location):
#   powershell -NoProfile -File Tools/layering_gate.ps1
#   pwsh       -NoProfile -File Tools/layering_gate.ps1
#   powershell -NoProfile -File Tools/layering_gate.ps1 -RepoRoot C:/dev/Zenith
#   powershell -NoProfile -File Tools/layering_gate.ps1 -Update    # rewrite the allowlist from the current tree
#
# Exit codes:
#   0 -- every cross-layer edge in the tree is on the allowlist (ratchet OK).
#   1 -- one or more NEW (non-allow-listed) edges found, OR a setup error
#        (missing scan dirs / missing allowlist).
#
# Built-in cmdlets only (Get-ChildItem, Select-String, Get-Content). ASCII-only
# body so Windows PowerShell 5.1 (default CP1252 codepage) parses it without
# mojibake, matching the existing Tools/ convention (doc_lint.ps1,
# check_acceptance_drift.ps1).
# =============================================================================

[CmdletBinding()]
param(
	# Repo root. When left empty it is derived from the script's own location
	# (parent of Tools/), which makes the gate robust to being launched from
	# any working directory, including the repo root itself.
	[string]$RepoRoot = "",

	# Allowlist file. When left empty it defaults to layering_allowlist.txt
	# next to this script in Tools/.
	[string]$AllowlistPath = "",

	# Regenerate the allowlist from the current tree instead of checking it.
	# Use only when DELIBERATELY accepting the present cross-layer edge set.
	[switch]$Update
)

$ErrorActionPreference = "Stop"

# Resolve $PSScriptRoot-dependent defaults in the BODY, not in the param block.
# Windows PowerShell 5.1 leaves $PSScriptRoot empty inside param default-value
# expressions when ANY parameter is bound explicitly (e.g. -RepoRoot), which
# made "Join-Path $PSScriptRoot ..." throw. Resolving here -- where
# $PSScriptRoot is reliably populated under both 5.1 and 7 -- avoids that.
if ([string]::IsNullOrEmpty($RepoRoot))
{
	$RepoRoot = Split-Path -Parent $PSScriptRoot
}
if ([string]::IsNullOrEmpty($AllowlistPath))
{
	$AllowlistPath = Join-Path $PSScriptRoot 'layering_allowlist.txt'
}

# -----------------------------------------------------------------------------
# Resolve + validate the two layer directories.
# -----------------------------------------------------------------------------
$ecDir   = Join-Path $RepoRoot 'Zenith/EntityComponent'
$fluxDir = Join-Path $RepoRoot 'Zenith/Flux'

if (-not (Test-Path $ecDir) -or -not (Test-Path $fluxDir)) {
	Write-Host "layering_gate: scan dirs not found under RepoRoot '$RepoRoot'." -ForegroundColor Red
	Write-Host "  expected: $ecDir" -ForegroundColor Red
	Write-Host "  expected: $fluxDir" -ForegroundColor Red
	Write-Host "  (pass -RepoRoot <path-to-repo> if running from an unusual location)" -ForegroundColor Red
	exit 1
}

# Absolute, normalized repo root for turning FullName into repo-relative paths.
$repoFull = (Resolve-Path $RepoRoot).Path
# Strip a trailing slash/backslash so the prefix-strip below leaves no leading
# separator on the relative path.
$repoFull = $repoFull.TrimEnd('\', '/')
$repoPrefix = $repoFull + '\'

# -----------------------------------------------------------------------------
# Turn an absolute file path into a forward-slash repo-relative path.
# -----------------------------------------------------------------------------
function Get-RepoRelative
{
	param([string]$FullPath)

	$normalized = $FullPath -replace '/', '\'
	if ($normalized.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase))
	{
		$normalized = $normalized.Substring($repoPrefix.Length)
	}
	# Canonical wire form uses forward slashes regardless of OS.
	return ($normalized -replace '\\', '/')
}

# -----------------------------------------------------------------------------
# Scan one directory for cross-layer includes of a given prefix.
#   $ScanDir       absolute dir to walk (*.h/*.cpp/*.inl)
#   $IncludePrefix the leading path segment that marks the OTHER layer,
#                  e.g. 'Flux/' or 'EntityComponent/'
# Returns a string[] of "<repo-rel-source> => <included-path>" edges.
# -----------------------------------------------------------------------------
function Get-CrossLayerEdges
{
	param(
		[string]$ScanDir,
		[string]$IncludePrefix
	)

	$edges = New-Object System.Collections.Generic.List[string]

	# Match a real include directive only: optional leading whitespace, then
	# #include, then the quoted path beginning with the other layer's prefix.
	# Anchoring at ^ excludes commented-out lines like "// #include \"Flux/..\"".
	$pattern = '^\s*#include\s*"(' + [regex]::Escape($IncludePrefix) + '[^"]+)"'

	$files = Get-ChildItem -Path $ScanDir -Recurse -File -Include *.h, *.cpp, *.inl -ErrorAction SilentlyContinue
	foreach ($file in $files)
	{
		$hits = Select-String -Path $file.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue
		foreach ($hit in $hits)
		{
			$included = $hit.Matches[0].Groups[1].Value
			$rel = Get-RepoRelative -FullPath $file.FullName
			$edges.Add($rel + ' => ' + $included)
		}
	}

	return $edges.ToArray()
}

# -----------------------------------------------------------------------------
# Gather + sort all edges from both directions.
# -----------------------------------------------------------------------------
$ecToFlux = Get-CrossLayerEdges -ScanDir $ecDir   -IncludePrefix 'Flux/'
$fluxToEc = Get-CrossLayerEdges -ScanDir $fluxDir -IncludePrefix 'EntityComponent/'

# Concatenate, de-dup (a file could in theory list the same include twice),
# and sort for deterministic output / a stable allowlist.
$allEdges = @()
$allEdges += $ecToFlux
$allEdges += $fluxToEc
$allEdges = $allEdges | Sort-Object -Unique

# -----------------------------------------------------------------------------
# -Update: rewrite the allowlist from the current tree, then exit 0.
# -----------------------------------------------------------------------------
if ($Update)
{
	$header = @(
		'# layering_allowlist.txt -- Wave-8 enforced-layering ratchet allowlist.',
		'#',
		'# These are the KNOWN cross-layer #include edges between Zenith/Flux and',
		'# Zenith/EntityComponent at the time this ratchet was introduced. They are',
		'# pre-existing leaks, NOT an endorsement of the coupling.',
		'#',
		'# RULES:',
		'#   * Do NOT add new edges to this file to silence the gate. A new',
		'#     Flux<->EntityComponent include should instead be removed/redesigned.',
		'#   * Later decoupling waves SHRINK this list: when a leak is removed the',
		'#     gate still passes and the now-stale line here should be deleted.',
		'#   * Format (one edge per line): <repo-relative-source> => <included-path>',
		'#',
		'# Regenerate (only when deliberately accepting the current set):',
		'#   powershell -NoProfile -File Tools/layering_gate.ps1 -Update',
		'#'
	)
	Set-Content -Path $AllowlistPath -Value ($header + $allEdges) -Encoding ASCII
	Write-Host "layering_gate: wrote $($allEdges.Count) edge(s) to $AllowlistPath" -ForegroundColor Cyan
	exit 0
}

# -----------------------------------------------------------------------------
# Load the allowlist into a case-sensitive set (paths are case-sensitive on
# the engine's target platforms; keep them exact).
# -----------------------------------------------------------------------------
if (-not (Test-Path $AllowlistPath))
{
	Write-Host "layering_gate: allowlist not found: $AllowlistPath" -ForegroundColor Red
	Write-Host "  run with -Update to generate it from the current tree." -ForegroundColor Red
	exit 1
}

$allowSet = @{}
$allowOrder = New-Object System.Collections.Generic.List[string]
foreach ($line in (Get-Content -Path $AllowlistPath))
{
	$trimmed = $line.Trim()
	if ($trimmed.Length -eq 0) { continue }
	if ($trimmed.StartsWith('#')) { continue }
	if (-not $allowSet.ContainsKey($trimmed))
	{
		$allowSet[$trimmed] = $true
		$allowOrder.Add($trimmed) | Out-Null
	}
}

# -----------------------------------------------------------------------------
# Compare: any present edge not on the allowlist is a NEW leak -> fail.
# -----------------------------------------------------------------------------
$newEdges = New-Object System.Collections.Generic.List[string]
foreach ($edge in $allEdges)
{
	if (-not $allowSet.ContainsKey($edge))
	{
		$newEdges.Add($edge) | Out-Null
	}
}

# Stale allowlist entries: listed but no longer present. Informational only --
# the ratchet must not block a wave that successfully removed a leak.
$presentSet = @{}
foreach ($edge in $allEdges) { $presentSet[$edge] = $true }
$staleEntries = New-Object System.Collections.Generic.List[string]
foreach ($entry in $allowOrder)
{
	if (-not $presentSet.ContainsKey($entry))
	{
		$staleEntries.Add($entry) | Out-Null
	}
}

# -----------------------------------------------------------------------------
# Report.
# -----------------------------------------------------------------------------
Write-Host "layering_gate: scanned Zenith/EntityComponent + Zenith/Flux" -ForegroundColor Cyan
Write-Host "  cross-layer edges found : $($allEdges.Count)" -ForegroundColor DarkGray
Write-Host "  allow-listed edges      : $($allowOrder.Count)" -ForegroundColor DarkGray

if ($staleEntries.Count -gt 0)
{
	Write-Host ""
	Write-Host "layering_gate: INFO -- $($staleEntries.Count) allowlist entr(y/ies) no longer present" -ForegroundColor Yellow
	Write-Host "  (a leak was removed -- good. Delete these stale lines from the allowlist.)" -ForegroundColor Yellow
	foreach ($entry in $staleEntries)
	{
		Write-Host "    STALE  $entry" -ForegroundColor Yellow
	}
}

if ($newEdges.Count -gt 0)
{
	Write-Host ""
	Write-Host "layering_gate: FAIL -- $($newEdges.Count) NEW cross-layer include edge(s) not on the allowlist:" -ForegroundColor Red
	foreach ($edge in $newEdges)
	{
		Write-Host "    NEW-LEAK  $edge" -ForegroundColor Red
	}
	Write-Host ""
	Write-Host "  Flux and EntityComponent must not gain new include coupling." -ForegroundColor Red
	Write-Host "  Remove the include, or -- if intentional -- add the edge to" -ForegroundColor Red
	Write-Host "  $AllowlistPath (or re-run with -Update)." -ForegroundColor Red
	exit 1
}

Write-Host ""
Write-Host "layering_gate: OK -- all $($allEdges.Count) cross-layer edge(s) are allow-listed; no new coupling." -ForegroundColor Green
exit 0
