# =============================================================================
# layering_gate.ps1 -- Wave-8 enforced-layering ratchet (wave8.7-layering-gate).
#
# This script runs TWO INDEPENDENT ratchets in a single pass:
#
#   (A) EC<->Flux include ratchet  (the original gate)
#   (B) ECS-leaf ratchet           (added later: keeps the ECS core leaf-clean)
#
# Both ratchets share the same wire format and the same "shrink-only" rule:
#   a NEW violation not on the matching allowlist => report + exit 1; an
#   allowlist entry that is no longer present => INFO only (never a failure).
# The overall exit code is 0 only if BOTH ratchets are green.
#
# -----------------------------------------------------------------------------
# (A) EC<->Flux include ratchet
# -----------------------------------------------------------------------------
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
# -----------------------------------------------------------------------------
# (B) ECS-leaf ratchet
# -----------------------------------------------------------------------------
# The ECS CORE -- the ZenithECS leaf lib, physically located under
# Zenith/ZenithECS -- should remain a low-level "leaf": it must not reach
# UP into engine-side subsystems, nor reach SIDEWAYS into the concrete
# components it stores. Components/ is engine-side and legitimately coupled,
# so it is deliberately NOT scanned by this ratchet.
#
# Two violation kinds are tracked, both in the same shrink-only allowlist
# (Tools/ecs_leaf_allowlist.txt):
#
#   kind 1 -- forbidden #include: a real (uncommented) #include whose quoted
#             path starts with any of these prefixes
#               Flux/  Physics/  UI/  Editor/  AssetHandling/  AI/
#               Components/  EntityComponent/Components/
#             or equals exactly  Core/Zenith_Engine.h
#             Wire form:  <repo-rel-source> => <included-path>
#
#   kind 2 -- forbidden symbol: a line of CODE that mentions the engine
#             singleton token  g_xEngine . Pure-comment lines (first
#             non-whitespace is //) are skipped, and a g_xEngine that occurs
#             only in the trailing // comment of a code line does NOT count
#             (Phase 2 drives this code count to zero, so comment-only
#             mentions must never be counted). At most one violation per
#             (file,line).
#             Wire form:  <repo-rel-source> => g_xEngine
#
# Source files for (B) = *.h, *.cpp, *.inl under Zenith/ZenithECS (the leaf
# lib; engine-side Components/ + glue stay in EntityComponent/, covered by (A)).
#
# -----------------------------------------------------------------------------
# Usage (robust to cwd -- repo root is derived from the script location):
#   powershell -NoProfile -File Tools/layering_gate.ps1
#   pwsh       -NoProfile -File Tools/layering_gate.ps1
#   powershell -NoProfile -File Tools/layering_gate.ps1 -RepoRoot C:/dev/Zenith
#   powershell -NoProfile -File Tools/layering_gate.ps1 -Update    # rewrite BOTH allowlists from the current tree
#
# -AllowlistPath overrides the EC<->Flux allowlist only (the original
# parameter, unchanged). The ECS-leaf allowlist always lives next to this
# script as Tools/ecs_leaf_allowlist.txt.
#
# Exit codes:
#   0 -- BOTH ratchets green: every present violation is on its allowlist.
#   1 -- one or more NEW (non-allow-listed) violations in EITHER ratchet, OR
#        a setup error (missing scan dirs / missing allowlist).
#
# Built-in cmdlets only (Get-ChildItem, Select-String, Get-Content,
# Set-Content). ASCII-only body so Windows PowerShell 5.1 (default CP1252
# codepage) parses it without mojibake, matching the existing Tools/
# convention (doc_lint.ps1, check_acceptance_drift.ps1).
# =============================================================================

[CmdletBinding()]
param(
	# Repo root. When left empty it is derived from the script's own location
	# (parent of Tools/), which makes the gate robust to being launched from
	# any working directory, including the repo root itself.
	[string]$RepoRoot = "",

	# EC<->Flux allowlist file. When left empty it defaults to
	# layering_allowlist.txt next to this script in Tools/. (This parameter
	# governs ratchet (A) only; the ECS-leaf allowlist path is fixed.)
	[string]$AllowlistPath = "",

	# Regenerate BOTH allowlists from the current tree instead of checking
	# them. Use only when DELIBERATELY accepting the present violation sets.
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

# The ECS-leaf allowlist always lives next to this script (not user-overridable;
# the spec fixes its name so the two ratchets never share a file).
$EcsLeafAllowlistPath = Join-Path $PSScriptRoot 'ecs_leaf_allowlist.txt'

# -----------------------------------------------------------------------------
# Resolve + validate the two layer directories.
# -----------------------------------------------------------------------------
$ecDir   = Join-Path $RepoRoot 'Zenith/EntityComponent'
$fluxDir = Join-Path $RepoRoot 'Zenith/Flux'
# The ECS-leaf ratchet (B) scans the relocated leaf lib, Zenith/ZenithECS (the
# ECS core was physically moved out of EntityComponent/). EntityComponent/ now
# holds only engine-side glue + the Components/ subtree, which ratchet (A) covers.
$ecsLeafDir = Join-Path $RepoRoot 'Zenith/ZenithECS'

if (-not (Test-Path $ecDir) -or -not (Test-Path $fluxDir) -or -not (Test-Path $ecsLeafDir)) {
	Write-Host "layering_gate: scan dirs not found under RepoRoot '$RepoRoot'." -ForegroundColor Red
	Write-Host "  expected: $ecDir" -ForegroundColor Red
	Write-Host "  expected: $fluxDir" -ForegroundColor Red
	Write-Host "  expected: $ecsLeafDir" -ForegroundColor Red
	Write-Host "  (pass -RepoRoot <path-to-repo> if running from an unusual location)" -ForegroundColor Red
	exit 1
}

# The ECS-leaf scan excludes the Components/ subtree.
$ecComponentsDir = Join-Path $ecDir 'Components'

# Absolute, normalized repo root for turning FullName into repo-relative paths.
$repoFull = (Resolve-Path $RepoRoot).Path
# Strip a trailing slash/backslash so the prefix-strip below leaves no leading
# separator on the relative path.
$repoFull = $repoFull.TrimEnd('\', '/')
$repoPrefix = $repoFull + '\'

# Normalized absolute prefix used to detect (and exclude) the Components/
# subtree. Trailing separator so it only matches the directory, not a sibling
# file that merely shares the "Components" stem.
$ecComponentsFull = (Resolve-Path $ecComponentsDir -ErrorAction SilentlyContinue)
if ($null -ne $ecComponentsFull)
{
	$ecComponentsPrefix = ($ecComponentsFull.Path.TrimEnd('\', '/')) + '\'
}
else
{
	# Components/ missing entirely -- nothing to exclude.
	$ecComponentsPrefix = $null
}

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
# Deterministic ORDINAL sort (optionally de-duplicating).
#
# Sort-Object uses CULTURE-aware string comparison, which (a) varies by host
# locale and PowerShell edition and (b) disagrees with the ORDINAL hashtable
# keys used when loading the allowlist -- e.g. it orders "InstancedMeshesImpl"
# before "InstanceGroup" whereas a byte/ordinal sort does the reverse. The
# checked-in allowlist is ordinal-ordered, so we sort ordinally here to keep
# -Update byte-stable and host-independent (and consistent with the case-
# sensitive membership test). Built on [Array]::Sort + StringComparer.Ordinal.
# -----------------------------------------------------------------------------
function Sort-Ordinal
{
	param(
		[string[]]$Items,
		[switch]  $Unique
	)

	# Normalize to a concrete array AND clone it: [Array]::Sort sorts in place,
	# and casting an already-[string[]] value with [string[]]$Items returns the
	# SAME reference -- sorting it would silently mutate the caller's array. The
	# explicit .Clone() guarantees Sort-Ordinal is side-effect-free (this bit us
	# when Resolve-AllowlistBody compared, then returned, the existing array).
	$arr = [string[]]@()
	if ($null -ne $Items)
	{
		$src = [string[]]$Items
		$arr = [string[]]$src.Clone()
	}

	if ($arr.Length -gt 1)
	{
		[Array]::Sort($arr, [System.StringComparer]::Ordinal)
	}

	if (-not $Unique) { return $arr }

	# De-dup an already ordinally-sorted array, preserving order.
	$out = New-Object System.Collections.Generic.List[string]
	$seen = $null
	foreach ($item in $arr)
	{
		if ($null -eq $seen -or -not [System.String]::Equals($item, $seen, [System.StringComparison]::Ordinal))
		{
			$out.Add($item) | Out-Null
			$seen = $item
		}
	}
	return $out.ToArray()
}

# -----------------------------------------------------------------------------
# Read the DATA entries (non-blank, non-comment) of an allowlist file in their
# on-disk order. Returns string[] (empty if the file is missing).
# -----------------------------------------------------------------------------
function Get-AllowlistEntries
{
	param([string]$Path)

	$entries = New-Object System.Collections.Generic.List[string]
	if (-not (Test-Path $Path)) { return $entries.ToArray() }
	foreach ($line in (Get-Content -Path $Path))
	{
		$trimmed = $line.Trim()
		if ($trimmed.Length -eq 0) { continue }
		if ($trimmed.StartsWith('#')) { continue }
		$entries.Add($trimmed) | Out-Null
	}
	return $entries.ToArray()
}

# -----------------------------------------------------------------------------
# Choose the body lines to (re)write for an allowlist on -Update.
#
# If the freshly-scanned violation set is IDENTICAL (as an ordinal multiset)
# to what the existing file already records, the existing on-disk ORDER is
# preserved verbatim -- so -Update is a byte-stable no-op against any
# pre-existing ordering (the checked-in layering_allowlist.txt was generated
# under a different locale, so a blind re-sort would churn it pointlessly).
# Only when the set actually changed do we emit the newly-sorted body.
#
#   $Scanned   ordinally-sorted (and, for EC<->Flux, de-duped) current set.
#   $Path      the allowlist file to compare against.
# Returns string[] body lines (no header).
# -----------------------------------------------------------------------------
function Resolve-AllowlistBody
{
	param(
		[string[]]$Scanned,
		[string]  $Path
	)

	$existing = Get-AllowlistEntries -Path $Path

	# Compare as ordinal multisets: same length AND same sorted contents.
	$scannedArr  = @(); if ($null -ne $Scanned)  { $scannedArr  = [string[]]$Scanned }
	$existingArr = @(); if ($null -ne $existing) { $existingArr = [string[]]$existing }

	$same = $false
	if ($scannedArr.Length -eq $existingArr.Length)
	{
		$existingSorted = Sort-Ordinal -Items $existingArr
		$scannedSorted  = Sort-Ordinal -Items $scannedArr
		$same = $true
		for ($i = 0; $i -lt $scannedSorted.Length; $i++)
		{
			if (-not [System.String]::Equals($scannedSorted[$i], $existingSorted[$i], [System.StringComparison]::Ordinal))
			{
				$same = $false
				break
			}
		}
	}

	if ($same)
	{
		# Identical set -> keep the file's current line order untouched.
		return $existingArr
	}
	# Set changed -> emit the freshly-sorted body.
	return $scannedArr
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
# Scan the ECS core (EntityComponent minus Components/) for ECS-leaf
# violations. Returns a string[] of "<repo-rel-source> => <target>" lines
# where <target> is either the forbidden include path (kind 1) or the literal
# 'g_xEngine' (kind 2).
# -----------------------------------------------------------------------------
function Get-EcsLeafViolations
{
	param(
		[string]$ScanDir,
		[string]$ExcludePrefix,  # absolute Components/ prefix to skip; may be $null
		[string[]]$ExcludeNames = @()   # engine-side EntityComponent/ files compiled into the ENGINE lib (NOT ZenithECS); not leaf TUs
	)

	$violations = New-Object System.Collections.Generic.List[string]

	# kind 1: a real #include whose quoted path begins with a forbidden
	# prefix, OR equals exactly Core/Zenith_Engine.h. Same first-token rule as
	# the EC<->Flux scan (anchored ^\s*#include) so commented-out includes do
	# not count. The alternation lists each forbidden prefix; the final branch
	# pins the exact engine-header path with a closing quote.
	$includePattern =
		'^\s*#include\s*"(' +
		'Flux/[^"]+'                     + '|' +
		'Physics/[^"]+'                  + '|' +
		'UI/[^"]+'                       + '|' +
		'Editor/[^"]+'                   + '|' +
		'AssetHandling/[^"]+'            + '|' +
		'AI/[^"]+'                       + '|' +
		'Components/[^"]+'               + '|' +
		'EntityComponent/Components/[^"]+' + '|' +
		'Core/Zenith_Engine\.h'          +
		')"'

	$files = Get-ChildItem -Path $ScanDir -Recurse -File -Include *.h, *.cpp, *.inl -ErrorAction SilentlyContinue
	foreach ($file in $files)
	{
		# Skip the Components/ subtree entirely. Guard on IsNullOrEmpty (not just
		# -ne $null): an omitted [string] parameter binds to '' under some hosts,
		# and ''.StartsWith('') is ALWAYS true -- which would silently skip EVERY
		# file (the ZenithECS scan passes no ExcludePrefix, so this matters).
		if (-not [string]::IsNullOrEmpty($ExcludePrefix))
		{
			$full = ($file.FullName -replace '/', '\')
			if ($full.StartsWith($ExcludePrefix, [System.StringComparison]::OrdinalIgnoreCase))
			{
				continue
			}
		}

		# Skip engine-side files that physically live under EntityComponent/ but
		# compile into the ENGINE lib, not ZenithECS (build-excluded from the leaf
		# by the Sharpmake partition). Their includes of concrete components are
		# legitimate engine glue, not leaf violations.
		if ($ExcludeNames -contains $file.Name)
		{
			continue
		}

		$rel = Get-RepoRelative -FullPath $file.FullName

		# --- kind 1: forbidden includes ---
		$incHits = Select-String -Path $file.FullName -Pattern $includePattern -AllMatches -ErrorAction SilentlyContinue
		foreach ($hit in $incHits)
		{
			$included = $hit.Matches[0].Groups[1].Value
			$violations.Add($rel + ' => ' + $included)
		}

		# --- kind 2: forbidden symbol g_xEngine in CODE ---
		# Read the file line-by-line so we can strip the trailing // comment and
		# skip pure-comment lines. We count at most one violation per (file,line)
		# by only emitting once even though we already filtered by line.
		$lines = Get-Content -Path $file.FullName -ErrorAction SilentlyContinue
		foreach ($line in $lines)
		{
			# Fast reject: no token at all on this raw line.
			if ($line -notmatch 'g_xEngine') { continue }

			$trimmedStart = $line.TrimStart()
			# Pure-comment line: first non-whitespace token is //.
			if ($trimmedStart.StartsWith('//')) { continue }

			# Strip a trailing // line-comment so a g_xEngine mentioned only in
			# the comment portion does not count. This is a deliberately simple
			# textual strip (no string/char-literal awareness): the ECS source
			# does not embed "//" inside string literals on g_xEngine lines, and
			# erring toward NOT counting comment text is the safe direction for a
			# ratchet whose Phase-2 target is zero.
			$codePart = $line
			$slashIdx = $codePart.IndexOf('//')
			if ($slashIdx -ge 0)
			{
				$codePart = $codePart.Substring(0, $slashIdx)
			}

			if ($codePart -match 'g_xEngine')
			{
				$violations.Add($rel + ' => g_xEngine')
			}
		}
	}

	return $violations.ToArray()
}

# -----------------------------------------------------------------------------
# Generic shrink-only ratchet check.
#   $Label        human label for the section header, e.g. 'EC<->Flux'.
#   $Present       string[] of currently-present violation lines (sorted/unique).
#   $AllowFile    path to this ratchet's allowlist.
#   $NewNoun      noun used in the FAIL message, e.g. 'cross-layer include edge'.
#   $FixHint      extra remediation line(s) printed on failure.
# Prints a clearly-labelled section and returns $true if GREEN (no new
# violations), $false otherwise. Missing allowlist => prints error, returns
# $false (treated as a setup failure by the caller).
# -----------------------------------------------------------------------------
function Test-Ratchet
{
	param(
		[string]   $Label,
		[string[]] $Present,
		[string]   $AllowFile,
		[string]   $NewNoun,
		[string[]] $FixHint
	)

	Write-Host ""
	Write-Host "=== Ratchet: $Label ===" -ForegroundColor Cyan

	if (-not (Test-Path $AllowFile))
	{
		Write-Host "layering_gate [$Label]: allowlist not found: $AllowFile" -ForegroundColor Red
		Write-Host "  run with -Update to generate it from the current tree." -ForegroundColor Red
		return $false
	}

	# Load the allowlist into a case-sensitive set (paths/symbols are
	# case-sensitive on the engine's target platforms; keep them exact).
	$allowSet = @{}
	$allowOrder = New-Object System.Collections.Generic.List[string]
	foreach ($line in (Get-Content -Path $AllowFile))
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

	# NEW violations: present but not allow-listed -> fail. Track distinct keys
	# so a file with many identical occurrences (e.g. N g_xEngine lines) is
	# reported once, while $newCount still reflects the true occurrence total.
	$newCount = 0
	$newKeys = @{}
	$newOrder = New-Object System.Collections.Generic.List[string]
	# Distinct present keys, so the summary line is comparable to the (unique)
	# allowlist key count even when $Present carries duplicate occurrences.
	$presentSet = @{}
	foreach ($item in $Present)
	{
		if (-not $presentSet.ContainsKey($item)) { $presentSet[$item] = $true }
		if (-not $allowSet.ContainsKey($item))
		{
			$newCount++
			if (-not $newKeys.ContainsKey($item))
			{
				$newKeys[$item] = $true
				$newOrder.Add($item) | Out-Null
			}
		}
	}

	# Stale allowlist entries: listed but no longer present. Informational only
	# -- the ratchet must not block a wave that successfully removed a leak.
	$staleEntries = New-Object System.Collections.Generic.List[string]
	foreach ($entry in $allowOrder)
	{
		if (-not $presentSet.ContainsKey($entry))
		{
			$staleEntries.Add($entry) | Out-Null
		}
	}

	# "occurrences" = raw violation lines (with duplicates); "distinct" = unique
	# wire keys, directly comparable to the allow-listed key count.
	Write-Host "  violations found  : $($Present.Count) occurrence(s), $($presentSet.Count) distinct" -ForegroundColor DarkGray
	Write-Host "  allow-listed      : $($allowOrder.Count) (distinct keys)" -ForegroundColor DarkGray

	if ($staleEntries.Count -gt 0)
	{
		Write-Host ""
		Write-Host "layering_gate [$Label]: INFO -- $($staleEntries.Count) allowlist entr(y/ies) no longer present" -ForegroundColor Yellow
		Write-Host "  (a leak was removed -- good. Delete these stale lines from the allowlist.)" -ForegroundColor Yellow
		foreach ($entry in $staleEntries)
		{
			Write-Host "    STALE  $entry" -ForegroundColor Yellow
		}
	}

	if ($newCount -gt 0)
	{
		Write-Host ""
		Write-Host "layering_gate [$Label]: FAIL -- $newCount NEW $NewNoun occurrence(s) ($($newOrder.Count) distinct) not on the allowlist:" -ForegroundColor Red
		foreach ($item in $newOrder)
		{
			Write-Host "    NEW-LEAK  $item" -ForegroundColor Red
		}
		Write-Host ""
		foreach ($hintLine in $FixHint)
		{
			Write-Host "  $hintLine" -ForegroundColor Red
		}
		return $false
	}

	Write-Host ""
	Write-Host "layering_gate [$Label]: OK -- all $($Present.Count) violation(s) are allow-listed; no new coupling." -ForegroundColor Green
	return $true
}

# -----------------------------------------------------------------------------
# Gather (A) EC<->Flux edges from both directions.
# -----------------------------------------------------------------------------
$ecToFlux = Get-CrossLayerEdges -ScanDir $ecDir   -IncludePrefix 'Flux/'
$fluxToEc = Get-CrossLayerEdges -ScanDir $fluxDir -IncludePrefix 'EntityComponent/'

# Concatenate, de-dup (a file could in theory list the same include twice),
# and sort ORDINALLY for deterministic, host-independent output / a stable
# allowlist that round-trips byte-for-byte through -Update.
$allEdges = @()
$allEdges += $ecToFlux
$allEdges += $fluxToEc
$allEdges = Sort-Ordinal -Items $allEdges -Unique

# -----------------------------------------------------------------------------
# Gather (B) ECS-leaf violations.
#
# NOTE: unlike the EC<->Flux edge set, the ECS-leaf set is sorted but NOT
# de-duplicated. The g_xEngine wire form ('<file> => g_xEngine') carries no
# line number, so every g_xEngine code line in a file would collapse to one
# entry under Sort-Object -Unique -- crushing the real per-line violation
# count (~275) down to ~1-per-file. Keeping duplicates makes the allowlist
# reflect the true number of violations (one line per occurrence) and keeps
# the reported counts honest. The ratchet COMPARE below is set-membership
# keyed on the wire string, so duplicate lines are harmless: a present
# occurrence is allowed iff its key is on the list, and a file's key only goes
# stale once its LAST occurrence is removed.
# -----------------------------------------------------------------------------
$ecsLeafRaw = Get-EcsLeafViolations -ScanDir $ecsLeafDir
$ecsLeaf = @()
$ecsLeaf += $ecsLeafRaw
$ecsLeaf = Sort-Ordinal -Items $ecsLeaf

# -----------------------------------------------------------------------------
# -Update: rewrite BOTH allowlists from the current tree, then exit 0.
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
	# Preserve the existing file's line order when the edge SET is unchanged, so
	# -Update is a byte-stable no-op against the checked-in (foreign-locale)
	# ordering instead of churning it.
	$ecFluxBody = Resolve-AllowlistBody -Scanned $allEdges -Path $AllowlistPath
	Set-Content -Path $AllowlistPath -Value ($header + $ecFluxBody) -Encoding ASCII
	Write-Host "layering_gate: wrote $($ecFluxBody.Count) edge(s) to $AllowlistPath" -ForegroundColor Cyan

	$ecsHeader = @(
		'# ecs_leaf_allowlist.txt -- ECS-leaf ratchet allowlist (independent of',
		'# the EC<->Flux ratchet in layering_allowlist.txt).',
		'#',
		'# The ECS CORE (Zenith/EntityComponent minus the Components/ subtree) must',
		'# stay a low-level leaf. These are the KNOWN leaf violations at the time',
		'# the ratchet was introduced -- pre-existing coupling, NOT an endorsement.',
		'#',
		'# Two violation kinds are tracked here:',
		'#   * forbidden #include : path starts with Flux/ Physics/ UI/ Editor/',
		'#     AssetHandling/ AI/ Components/ EntityComponent/Components/, or equals',
		'#     exactly Core/Zenith_Engine.h.',
		'#   * forbidden symbol   : a line of CODE mentioning g_xEngine.',
		'#',
		'# RULES:',
		'#   * Do NOT add new entries here to silence the gate. Fix the source.',
		'#   * Later waves SHRINK this list: when a violation is removed the gate',
		'#     still passes and the now-stale line here should be deleted.',
		'#   * Phase 2 drives the g_xEngine code count to ZERO.',
		'#   * Format (one entry per line): <repo-relative-source> => <target>',
		'#     where <target> is the included path, or the literal g_xEngine.',
		'#',
		'# Regenerate (only when deliberately accepting the current set):',
		'#   powershell -NoProfile -File Tools/layering_gate.ps1 -Update',
		'#'
	)
	# Same order-preserving treatment for the ECS-leaf list (no-op on first
	# seed since the file does not yet exist).
	$ecsLeafBody = Resolve-AllowlistBody -Scanned $ecsLeaf -Path $EcsLeafAllowlistPath
	Set-Content -Path $EcsLeafAllowlistPath -Value ($ecsHeader + $ecsLeafBody) -Encoding ASCII
	Write-Host "layering_gate: wrote $($ecsLeafBody.Count) ECS-leaf violation(s) to $EcsLeafAllowlistPath" -ForegroundColor Cyan

	exit 0
}

# -----------------------------------------------------------------------------
# Check BOTH ratchets. The overall exit code is 0 only if BOTH are green.
# -----------------------------------------------------------------------------
Write-Host "layering_gate: scanning Zenith/EntityComponent + Zenith/Flux (EC<->Flux) and Zenith/ZenithECS (ECS-leaf)" -ForegroundColor Cyan

$ecFluxFixHint = @(
	'Flux and EntityComponent must not gain new include coupling.',
	'Remove the include, or -- if intentional -- add the edge to',
	"$AllowlistPath (or re-run with -Update)."
)
$ecFluxOk = Test-Ratchet -Label 'EC<->Flux' -Present $allEdges -AllowFile $AllowlistPath -NewNoun 'cross-layer include edge' -FixHint $ecFluxFixHint

$ecsLeafFixHint = @(
	'The ECS core (EntityComponent minus Components/) must stay leaf-clean:',
	'no Flux/Physics/UI/Editor/AssetHandling/AI/Components includes, no',
	'Core/Zenith_Engine.h, no g_xEngine in code. Remove it, or -- if',
	"intentional -- add the entry to $EcsLeafAllowlistPath (or re-run with -Update)."
)
$ecsLeafOk = Test-Ratchet -Label 'ECS-leaf' -Present $ecsLeaf -AllowFile $EcsLeafAllowlistPath -NewNoun 'ECS-leaf violation' -FixHint $ecsLeafFixHint

Write-Host ""
if ($ecFluxOk -and $ecsLeafOk)
{
	Write-Host "layering_gate: OK -- both ratchets green." -ForegroundColor Green
	exit 0
}

Write-Host "layering_gate: FAIL -- one or more ratchets reported a new violation (or a setup error)." -ForegroundColor Red
exit 1
