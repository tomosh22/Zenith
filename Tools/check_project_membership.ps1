<#
.SYNOPSIS
    Phase 0 instrument for the engine lib-split refactor: verifies that every
    compiled .cpp/.c source file belongs to exactly one project per link target.

.DESCRIPTION
    Parses every generated .vcxproj on disk (they are gitignored; regenerate
    with Build\regen.ps1 if they are missing/stale) and:

      1. DUPLICATE-OWNED check: for every project that is an Application or
         DynamicLibrary in a given configuration (a "link target"), computes
         the transitive closure of its ProjectReference graph and unions the
         compiled-source sets of that closure for the SAME configuration. Any
         source compiled by more than one project in that union is reported.

      1b. ORPHAN PROJECT check: a project that is never itself a link target
          and is never reached (via ProjectReference) from one cannot be
          caught by check 1 at all, because it never appears in any closure.
          Reported separately, together with any source it shares with a
          project that DOES build (e.g. Zenith / ZenithTools both compiling
          Tools/Zenith_Tools_*.cpp and Zenith/Flux/MeshGeometry/**).

      2. UNOWNED check: every Zenith/**/*.cpp|*.c not compiled by ANY parsed
         project, for a representative set of win64 configurations.

      3. Grep gate: real conf.SourceFilesBuildExcludeRegex.Add(...) CALL SITES
         across Build/Sharpmake_*.cs (excluding the vendored middleware
         projects, which never adopted the mechanism this refactor is
         eliminating). A line that merely MENTIONS the identifier in a
         comment (of which this codebase's history carries several,
         documenting the mechanism's removal) does not count — only an
         actual invocation is a finding. Any survivor not named in
         $script:BuildExcludeAllowlist below is fatal.

    FATAL BY DEFAULT (Phase 4h.4): exits 1 if any DUPLICATE, ORPHAN PROJECT,
    ORPHAN-DUPLICATE, genuine UNOWNED, or un-allowlisted build-exclude-gate
    finding exists. Pass -ReportOnly to fall back to the old always-exit-0
    behaviour (findings are still printed either way).

.PARAMETER RepoRoot
    Repo root. Defaults to two levels up from this script (Tools/ -> repo root).

.PARAMETER ReportOnly
    Print findings but always exit 0, regardless of what was found. The
    escape hatch this script used to default to before Phase 4h.4 flipped
    the polarity — use it for an exploratory run, never for a gate.

.PARAMETER Json
    Emit machine-readable findings (a single JSON object) instead of the
    human-readable report.
#>
[CmdletBinding()]
param(
	[string]$RepoRoot = $(Split-Path -Parent (Split-Path -Parent $PSCommandPath)),
	[switch]$ReportOnly,
	[switch]$Json
)

# ---------------------------------------------------------------------------
# Phase 4h.3 drove the SourceFilesBuildExcludeRegex.Add(...) call-site count
# to ZERO across the whole codebase (Sharpmake_Common.cs's
# ConfigurePlatformExcludes -- the mechanism's last caller -- was deleted
# outright, along with the method itself). This allowlist is therefore
# EMPTY, and stays that way until a genuinely unavoidable per-configuration
# exclude resurfaces (Phase 4h.3's own report names none). Add an entry only
# with a comment explaining precisely why membership-exclusion could not
# replace it -- an honest, documented survivor beats a contrived
# micro-library, but the default is zero and this gate is fatal on any
# undocumented one.
$script:BuildExcludeAllowlist = @()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:MsbuildNs = 'http://schemas.microsoft.com/developer/msbuild/2003'

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Get-NormalizedPath
{
	param([string]$Path)
	return ([System.IO.Path]::GetFullPath($Path)).ToLowerInvariant()
}

function Get-ConfigKeyFromCondition
{
	param([string]$Condition)
	if ([string]::IsNullOrEmpty($Condition)) { return $null }
	if ($Condition -match "=='([^']+)'") { return $Matches[1] }
	return $null
}

# ---------------------------------------------------------------------------
# Deliverable 1 — vcxproj discovery
# ---------------------------------------------------------------------------

function Get-CandidateVcxprojFiles
{
	param([string]$Root)

	# Noise dirs that are not part of the tracked tree at all (gitignored
	# worktree scratch, VCS internals, permission-denied scratch dirs).
	$excludeTopDirs = @('.git', '.claude', '.codex_tmp', 'node_modules')

	$collected = New-Object System.Collections.Generic.List[System.IO.FileInfo]

	$topEntries = Get-ChildItem -LiteralPath $Root -Force -ErrorAction SilentlyContinue
	foreach ($entry in $topEntries)
	{
		if (-not $entry.PSIsContainer) { continue }
		if ($excludeTopDirs -contains $entry.Name) { continue }

		$found = Get-ChildItem -LiteralPath $entry.FullName -Recurse -File -Filter '*.vcxproj' -ErrorAction SilentlyContinue
		foreach ($f in $found) { $collected.Add($f) }
	}

	# Generated-artifact / vendored-project-file exclusions:
	#   obj, output, .vs, dist          — build intermediates (gitignored)
	#   Build\artifacts                 — test/telemetry/log output root
	#   Middleware (anywhere in path)   — vendored third-party trees ship
	#                                      their OWN .vcxproj/.sln (imgui-docking
	#                                      examples, VulkanSDK Demos, freetype's
	#                                      bundled vc2010 project under
	#                                      Tools\Middleware\freetype\...). These
	#                                      are not Sharpmake output, use
	#                                      unrelated Configuration|Platform
	#                                      names, and are never referenced by
	#                                      any Zenith solution — including them
	#                                      would corrupt the graph, not extend it.
	$excludeSegmentRegex = '(?i)[\\/](obj|output|\.vs|dist|Middleware)[\\/]'
	$excludeArtifactsRegex = '(?i)[\\/]Build[\\/]artifacts[\\/]'

	$result = New-Object System.Collections.Generic.List[System.IO.FileInfo]
	foreach ($f in $collected)
	{
		if ($f.FullName -match $excludeSegmentRegex) { continue }
		if ($f.FullName -match $excludeArtifactsRegex) { continue }
		$result.Add($f)
	}
	return ,@($result.ToArray())
}

function ConvertFrom-VcxprojFile
{
	param([System.IO.FileInfo]$File)

	[xml]$xml = Get-Content -LiteralPath $File.FullName -Raw
	$ns = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
	$ns.AddNamespace('msb', $script:MsbuildNs)

	$projDir = $File.DirectoryName

	# --- Declared configurations ("Config|Platform" strings) ---
	$configs = New-Object System.Collections.Generic.List[string]
	foreach ($node in $xml.SelectNodes("//msb:ItemGroup[@Label='ProjectConfigurations']/msb:ProjectConfiguration", $ns))
	{
		$inc = $node.GetAttribute('Include')
		if ($inc) { [void]$configs.Add($inc) }
	}

	# --- ConfigurationType per config ---
	$configTypes = @{}
	foreach ($pg in $xml.SelectNodes("//msb:PropertyGroup[@Label='Configuration']", $ns))
	{
		$key = Get-ConfigKeyFromCondition $pg.GetAttribute('Condition')
		if (-not $key) { continue }
		$ctNode = $pg.SelectSingleNode('msb:ConfigurationType', $ns)
		if ($ctNode) { $configTypes[$key] = $ctNode.InnerText.Trim() }
	}

	# --- Compiled sources: sourceKey(lower, absolute) -> { cfg -> excluded(bool) } ---
	# Only .cpp/.c are in scope (the invariant is about compiled C/C++ TUs).
	$sources = @{}
	foreach ($cl in $xml.SelectNodes('//msb:ItemGroup/msb:ClCompile', $ns))
	{
		$inc = $cl.GetAttribute('Include')
		if ([string]::IsNullOrWhiteSpace($inc)) { continue }

		$ext = [System.IO.Path]::GetExtension($inc).ToLowerInvariant()
		if ($ext -ne '.cpp' -and $ext -ne '.c') { continue }

		$absSrc = Get-NormalizedPath (Join-Path $projDir $inc)

		if (-not $sources.ContainsKey($absSrc)) { $sources[$absSrc] = @{} }
		$excludeMap = $sources[$absSrc]

		# Defensive: an item-level Condition (rare in this codebase's output,
		# but legal MSBuild) restricts the item to ONE config; every other
		# declared config is implicitly excluded.
		$itemCfgKey = Get-ConfigKeyFromCondition $cl.GetAttribute('Condition')
		if ($itemCfgKey)
		{
			foreach ($c in $configs) { if ($c -ne $itemCfgKey) { $excludeMap[$c] = $true } }
		}

		foreach ($ex in $cl.SelectNodes('msb:ExcludedFromBuild', $ns))
		{
			$cfgKey = Get-ConfigKeyFromCondition $ex.GetAttribute('Condition')
			if (-not $cfgKey) { continue }
			$val = $ex.InnerText.Trim().ToLowerInvariant()
			$excludeMap[$cfgKey] = ($val -eq 'true')
		}
	}

	# --- ProjectReference edges (dependency graph) ---
	$refs = New-Object System.Collections.Generic.List[string]
	foreach ($pr in $xml.SelectNodes('//msb:ItemGroup/msb:ProjectReference', $ns))
	{
		$inc = $pr.GetAttribute('Include')
		if ([string]::IsNullOrWhiteSpace($inc)) { continue }
		[void]$refs.Add((Get-NormalizedPath (Join-Path $projDir $inc)))
	}

	# NOTE: these are plain hashtable-literal assignments (`=`), not `return`
	# statements — PowerShell does not unroll a collection through a pipeline
	# here, so wrapping with the `,@(...)` "protect an array" idiom would
	# instead NEST the array one level deep. Assign the arrays directly.
	return [pscustomobject]@{
		Path        = Get-NormalizedPath $File.FullName
		DisplayPath = $File.FullName
		Name        = [System.IO.Path]::GetFileNameWithoutExtension($File.Name)
		Configs     = $configs.ToArray()
		ConfigTypes = $configTypes
		Sources     = $sources
		References  = $refs.ToArray()
	}
}

# ---------------------------------------------------------------------------
# Load every project
# ---------------------------------------------------------------------------

# NOTE: with -Json, stdout must be clean JSON for a caller to pipe into a
# parser. Progress/diagnostic chatter below is gated off $Json (or, for the
# parse-error warning, sent to stderr, which is always safe) so it never
# lands in the redirected stdout stream alongside the JSON object.
if (-not $Json)
{
	Write-Host "Discovering .vcxproj files under $RepoRoot ..."
}
$vcxprojFiles = Get-CandidateVcxprojFiles $RepoRoot
if (-not $Json)
{
	Write-Host "  found $($vcxprojFiles.Count) candidate project files"
}

$projects = @{}
$parseErrors = New-Object System.Collections.Generic.List[object]
foreach ($f in $vcxprojFiles)
{
	try
	{
		$p = ConvertFrom-VcxprojFile $f
		$projects[$p.Path] = $p
	}
	catch
	{
		$parseErrors.Add([pscustomobject]@{ File = $f.FullName; Error = $_.Exception.Message })
	}
}

if ($parseErrors.Count -gt 0)
{
	[Console]::Error.WriteLine("WARNING: $($parseErrors.Count) file(s) failed to parse:")
	foreach ($pe in $parseErrors) { [Console]::Error.WriteLine("  $($pe.File): $($pe.Error)") }
}

# ---------------------------------------------------------------------------
# Compiled-source-per-config cache + reference-closure cache
# ---------------------------------------------------------------------------

$compiledCache = @{}
function Get-CompiledSources
{
	param([string]$ProjKey, [string]$Cfg)

	$cacheKey = "$ProjKey|$Cfg"
	if ($compiledCache.ContainsKey($cacheKey)) { return $compiledCache[$cacheKey] }

	$proj = $projects[$ProjKey]
	$set = New-Object System.Collections.Generic.HashSet[string]
	if ($proj.Configs -contains $Cfg)
	{
		foreach ($srcKey in $proj.Sources.Keys)
		{
			$excludeMap = $proj.Sources[$srcKey]
			$excluded = $false
			if ($excludeMap.ContainsKey($Cfg)) { $excluded = [bool]$excludeMap[$Cfg] }
			if (-not $excluded) { [void]$set.Add($srcKey) }
		}
	}
	$compiledCache[$cacheKey] = $set
	return ,$set
}

$closureCache = @{}
function Get-TransitiveClosure
{
	# ProjectReference carries no per-config Condition anywhere in this
	# codebase's generated output (verified), so the reference graph is
	# config-agnostic and a closure can be cached per project alone.
	param([string]$StartKey)

	if ($closureCache.ContainsKey($StartKey)) { return $closureCache[$StartKey] }

	$visited = New-Object System.Collections.Generic.HashSet[string]
	$stack = New-Object System.Collections.Generic.Stack[string]
	$stack.Push($StartKey)
	while ($stack.Count -gt 0)
	{
		$cur = $stack.Pop()
		if (-not $visited.Add($cur)) { continue }
		if (-not $projects.ContainsKey($cur)) { continue }
		foreach ($r in $projects[$cur].References)
		{
			if (-not $visited.Contains($r)) { $stack.Push($r) }
		}
	}
	$closureCache[$StartKey] = $visited
	return ,$visited
}

# ---------------------------------------------------------------------------
# Link targets (Application / DynamicLibrary, per declared config)
# ---------------------------------------------------------------------------

$linkTargets = New-Object System.Collections.Generic.List[object]
foreach ($projKey in $projects.Keys)
{
	$proj = $projects[$projKey]
	foreach ($cfg in $proj.Configs)
	{
		$ct = $null
		if ($proj.ConfigTypes.ContainsKey($cfg)) { $ct = $proj.ConfigTypes[$cfg] }
		if ($ct -eq 'Application' -or $ct -eq 'DynamicLibrary')
		{
			$linkTargets.Add([pscustomobject]@{ ProjKey = $projKey; Config = $cfg })
		}
	}
}

# ---------------------------------------------------------------------------
# Check 1: DUPLICATE-OWNED sources within a link target's closure
# ---------------------------------------------------------------------------

$duplicateFindings = New-Object System.Collections.Generic.List[object]
foreach ($lt in $linkTargets)
{
	$closure = Get-TransitiveClosure $lt.ProjKey
	$owners = @{}   # sourceKey -> List[projKey]  (closure is a set, so each project contributes at most once)

	foreach ($member in $closure)
	{
		if (-not $projects.ContainsKey($member)) { continue }
		$set = Get-CompiledSources $member $lt.Config
		foreach ($src in $set)
		{
			if (-not $owners.ContainsKey($src)) { $owners[$src] = New-Object System.Collections.Generic.List[string] }
			[void]$owners[$src].Add($member)
		}
	}

	foreach ($src in $owners.Keys)
	{
		$ownerKeys = $owners[$src]
		if ($ownerKeys.Count -le 1) { continue }
		$ownerNames = @($ownerKeys | ForEach-Object { $projects[$_].Name } | Sort-Object -Unique)
		if ($ownerNames.Count -le 1) { continue }

		$duplicateFindings.Add([pscustomobject]@{
			Config     = $lt.Config
			ExeProject = $projects[$lt.ProjKey].Name
			Source     = $src
			Owners     = $ownerNames
		})
	}
}

# ---------------------------------------------------------------------------
# Check 1b: ORPHAN PROJECTS (never a link target, never reachable from one)
# ---------------------------------------------------------------------------

$linkTargetProjKeys = New-Object System.Collections.Generic.HashSet[string]
foreach ($lt in $linkTargets) { [void]$linkTargetProjKeys.Add($lt.ProjKey) }

$reachable = New-Object System.Collections.Generic.HashSet[string]
foreach ($k in $linkTargetProjKeys)
{
	foreach ($m in (Get-TransitiveClosure $k)) { [void]$reachable.Add($m) }
}

$orphanKeys = @($projects.Keys | Where-Object { -not $reachable.Contains($_) })

function Get-AnyConfigCompiledUnion
{
	param([string]$ProjKey)
	$proj = $projects[$ProjKey]
	$set = New-Object System.Collections.Generic.HashSet[string]
	# Iterate only REAL configs (ones with an actual ConfigurationType
	# PropertyGroup), not every declared <ProjectConfiguration> entry. AGDE
	# projects declare the full Configuration-name x Platform-name CROSS
	# PRODUCT (e.g. "Vulkan_arm64_v8a_..._Agde_False|Android-x86_64") but only
	# the diagonal pairs are ever actually configured/built; the mismatched
	# ghost pairs carry no ConfigurationType and no per-config
	# ExcludedFromBuild entries either, so treating them as real would make
	# every source look "compiled" there by the default-compiled rule.
	foreach ($cfg in $proj.ConfigTypes.Keys)
	{
		foreach ($s in (Get-CompiledSources $ProjKey $cfg)) { [void]$set.Add($s) }
	}
	return ,$set
}

$orphanDuplicateFindings = New-Object System.Collections.Generic.List[object]
foreach ($ok in $orphanKeys)
{
	$orphanSet = Get-AnyConfigCompiledUnion $ok
	if ($orphanSet.Count -eq 0) { continue }

	foreach ($pk in $projects.Keys)
	{
		if ($pk -eq $ok) { continue }
		$otherSet = Get-AnyConfigCompiledUnion $pk
		if ($otherSet.Count -eq 0) { continue }

		foreach ($src in $orphanSet)
		{
			if ($otherSet.Contains($src))
			{
				$orphanDuplicateFindings.Add([pscustomobject]@{
					OrphanProject  = $projects[$ok].Name
					Source         = $src
					AlsoCompiledBy = $projects[$pk].Name
				})
			}
		}
	}
}

# ---------------------------------------------------------------------------
# Check 2: UNOWNED sources for representative win64 configurations
# ---------------------------------------------------------------------------

$representativeConfigs = @(
	'Vulkan_vs2022_Debug_Win64_True|x64',
	'Null_vs2022_Debug_Win64_True|x64',
	'D3D12_vs2022_Debug_Win64_False|x64'
)

# Baked-in ignore list. Each entry documents WHY the tree is legitimately
# unowned in the configs it applies to. "Configs = $null" means "applies to
# every representative config".
# Patterns are unanchored — they match an \Android\ / \Vulkan\ / \D3D12\ /
# \Null\ segment ANYWHERE in the path, not just directly under Zenith\ — e.g.
# Zenith\RenderBackends\Vulkan\Platform\Android\Zenith_Android_Vulkan.cpp is
# caught by the Android pattern even though it is nested several levels
# deeper than Zenith\Android\.
#
# ★ PHASE 4E CHANGED THE *MECHANISM* THE BACKEND PATTERNS DOCUMENT, NOT THE
# PATTERNS THEMSELVES. Before this phase, Zenith/RenderBackends/{Vulkan,D3D12,
# Null} were all globbed by the single monolithic aggregate ZenithProject
# (which declares every representative config), so a backend tree was
# unowned-in-a-config because Build/Sharpmake_Common.cs's
# ConfigurePlatformExcludes had per-CONFIGURATION build-EXCLUDED it
# (conf.SourceFilesBuildExcludeRegex — "owned, but not built here"). That
# block is now DELETED. Each backend is instead its own TARGET-RESTRICTED
# project (Sharpmake_ZenithRenderBackendVulkan.cs / …D3D12.cs / …Null.cs) that
# declares ONLY the RenderBackend fragment(s) it supports — so for e.g. the
# Null representative config, the Vulkan backend project's own `Configs` array
# never contains that config key AT ALL, and `Get-CompiledSources` returns an
# empty set for it (see that function's `if ($proj.Configs -contains $Cfg)`
# guard) — "not a project member of this config's solution", not "excluded
# from an otherwise-member project's build". The net effect on THIS check is
# identical (the same sources are absent from the same configs' compiled
# unions), so the patterns and their per-config scoping are UNCHANGED — only
# this comment and each entry's Reason text changed, to describe project
# membership instead of a build exclude that no longer exists.
$ignoreUnownedPatterns = @(
	[pscustomobject]@{
		Pattern = '\\android\\'
		Configs = $null
		Reason  = 'ConfigurePlatformExcludes still excludes any \Android\ path segment wholesale on Platform.win64 (all backends, all Tools states) — Android is agde-only; this arm was NOT touched by Phase 4e. Matches Zenith\Android\** AND nested platform dirs like Zenith\RenderBackends\Vulkan\Platform\Android\ (now one level deeper than before Phase 1 grouped the backends under RenderBackends\).'
	}
	[pscustomobject]@{
		Pattern = '\\vulkan\\'
		Configs = @('Null_vs2022_Debug_Win64_True|x64', 'D3D12_vs2022_Debug_Win64_False|x64')
		Reason  = 'PROJECT MEMBERSHIP (Phase 4e), not a build exclude: ZenithRenderBackendVulkan declares only RenderBackend.Vulkan targets, so it is not a project member of a Null_/D3D12_ config at all — any \Vulkan\ path segment is therefore absent from every project compiled in those configs.'
	}
	[pscustomobject]@{
		Pattern = '\\d3d12\\'
		Configs = @('Vulkan_vs2022_Debug_Win64_True|x64', 'Null_vs2022_Debug_Win64_True|x64')
		Reason  = 'PROJECT MEMBERSHIP (Phase 4e): ZenithRenderBackendD3D12 declares only RenderBackend.D3D12 (win64-only) targets, so any \D3D12\ path segment is absent from every project compiled in a Vulkan_/Null_ config (D3D12 is the reserved link-neutrality-proof backend).'
	}
	[pscustomobject]@{
		Pattern = '\\null\\'
		Configs = @('Vulkan_vs2022_Debug_Win64_True|x64', 'D3D12_vs2022_Debug_Win64_False|x64')
		Reason  = 'PROJECT MEMBERSHIP (Phase 4e): ZenithRenderBackendNull declares only RenderBackend.Null (win64-only) targets, so any \Null\ path segment is absent from every project compiled in a Vulkan_/D3D12_ config.'
	}
	[pscustomobject]@{
		Pattern = '\\editor\\'
		Configs = @('D3D12_vs2022_Debug_Win64_False|x64')
		Reason  = 'PROJECT MEMBERSHIP (Phase 4g), UPDATED from "excluded from build in a _False config": Zenith/Editor/ used to be owned by the aggregate ZenithProject and reach zero output in a ToolsEnabled.False config only because every one of its .cpp files is individually wrapped in #ifdef ZENITH_TOOLS. It is now owned WHOLE by ZenithEditorLibProject, which declares ONLY a win64 x ToolsEnabled.True target set (Sharpmake_ZenithEditor.cs) — so a ToolsEnabled.False config (the representative D3D12_..._False config here) simply has no Editor project in its solution at all, the same "absent project, not an empty one" shape the three render-backend entries above already document for Phase 4e. The reasoning changed; the absence did not — still ignorable, not a real unowned-file defect.'
	}
)

$zenithDir = Join-Path $RepoRoot 'Zenith'
$allZenithCpp = @(Get-ChildItem -LiteralPath $zenithDir -Recurse -File -Include *.cpp, *.c -ErrorAction SilentlyContinue |
	ForEach-Object { Get-NormalizedPath $_.FullName })
$allZenithCppSet = [System.Collections.Generic.HashSet[string]]::new([string[]]$allZenithCpp)

$unownedByConfig = @{}
foreach ($cfg in $representativeConfigs)
{
	$compiledUnion = New-Object System.Collections.Generic.HashSet[string]
	foreach ($projKey in $projects.Keys)
	{
		foreach ($s in (Get-CompiledSources $projKey $cfg)) { [void]$compiledUnion.Add($s) }
	}

	$genuine = New-Object System.Collections.Generic.List[string]
	$ignoredCount = 0
	foreach ($src in $allZenithCppSet)
	{
		if ($compiledUnion.Contains($src)) { continue }

		$ignored = $false
		foreach ($p in $ignoreUnownedPatterns)
		{
			$appliesHere = (-not $p.Configs) -or ($p.Configs -contains $cfg)
			if ($appliesHere -and ($src -match $p.Pattern)) { $ignored = $true; break }
		}

		if ($ignored) { $ignoredCount++ } else { [void]$genuine.Add($src) }
	}

	$unownedByConfig[$cfg] = [pscustomobject]@{
		Genuine      = @($genuine.ToArray() | Sort-Object)
		IgnoredCount = $ignoredCount
	}
}

$totalGenuineUnowned = 0
foreach ($cfg in $representativeConfigs) { $totalGenuineUnowned += $unownedByConfig[$cfg].Genuine.Count }

# ---------------------------------------------------------------------------
# Deliverable 2: SourceFilesBuildExcludeRegex.Add(...) call-site gate
# (Phase 4h.4: FATAL by default, minus $script:BuildExcludeAllowlist)
# ---------------------------------------------------------------------------

function Get-BuildExcludeRegexFindings
{
	param([string]$Root)

	$vendorFiles = @('Sharpmake_FreeType.cs', 'Sharpmake_Msdfgen.cs', 'Sharpmake_MsdfAtlasGen.cs')
	$buildDir = Join-Path $Root 'Build'
	$files = @(Get-ChildItem -LiteralPath $buildDir -Filter 'Sharpmake_*.cs' -File -ErrorAction SilentlyContinue |
		Where-Object { $vendorFiles -notcontains $_.Name } | Sort-Object Name)

	# Phase 4h.4 NARROWED the match from "mentions the identifier anywhere"
	# to "is an actual .Add(...) call site" -- a comment documenting the
	# mechanism's own removal (this codebase's history now carries several,
	# deliberately, as a record of what got measured and why) is not a
	# finding, and gating on it would make this check permanently red for
	# prose that is doing exactly its job.
	$findings = New-Object System.Collections.Generic.List[object]
	foreach ($f in $files)
	{
		$lines = Get-Content -LiteralPath $f.FullName
		for ($i = 0; $i -lt $lines.Count; $i++)
		{
			# A REAL call site, not a comment quoting the syntax (this
			# codebase's history literally shows the old call inside prose,
			# e.g. Sharpmake_Zenith.cs's own "...is DELETED" line) --
			# require the trimmed line not to start with "//".
			if ($lines[$i].TrimStart() -notmatch '^//' -and $lines[$i] -match 'SourceFilesBuildExcludeRegex\s*\.\s*Add\s*\(')
			{
				$isAllowlisted = $false
				foreach ($a in $script:BuildExcludeAllowlist)
				{
					if ($a.File -eq $f.Name -and $a.Line -eq ($i + 1)) { $isAllowlisted = $true; break }
				}
				$findings.Add([pscustomobject]@{
					File        = $f.Name
					Line        = ($i + 1)
					Text        = $lines[$i].Trim()
					Allowlisted = $isAllowlisted
				})
			}
		}
	}
	return ,@($findings.ToArray())
}

$buildExcludeFindings = Get-BuildExcludeRegexFindings $RepoRoot

# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------

$buildExcludeFatalFindings = @($buildExcludeFindings | Where-Object { -not $_.Allowlisted })

# Phase 4h.4: the build-exclude gate is now part of $hasFatalFindings (it
# used to never affect the exit code at all). An allowlisted survivor is
# still PRINTED (visibility never regresses) but does not fail the gate.
$hasFatalFindings = ($duplicateFindings.Count -gt 0) -or ($orphanKeys.Count -gt 0) -or ($totalGenuineUnowned -gt 0) -or ($buildExcludeFatalFindings.Count -gt 0)
$fatalMode = -not [bool]$ReportOnly

if ($Json)
{
	$out = [pscustomobject]@{
		ProjectsParsed              = $projects.Count
		ParseErrors                 = $parseErrors
		LinkTargetCount             = $linkTargets.Count
		Duplicates                  = $duplicateFindings
		OrphanProjects              = @($orphanKeys | ForEach-Object { $projects[$_].DisplayPath })
		OrphanDuplicates            = $orphanDuplicateFindings
		UnownedByConfig             = $unownedByConfig
		TotalGenuineUnowned         = $totalGenuineUnowned
		BuildExcludeRegexCount      = $buildExcludeFindings.Count
		BuildExcludeRegexLines      = $buildExcludeFindings
		BuildExcludeRegexFatalCount = $buildExcludeFatalFindings.Count
		ReportOnly                  = [bool]$ReportOnly
		FatalMode                   = $fatalMode
		HasFatalFindings            = $hasFatalFindings
	}
	$out | ConvertTo-Json -Depth 8
}
else
{
	Write-Host ''
	Write-Host '============================================================'
	Write-Host ' Zenith project-membership check'
	Write-Host '============================================================'
	Write-Host "Projects parsed   : $($projects.Count)"
	Write-Host "Link targets      : $($linkTargets.Count)  (Application/DynamicLibrary x declared config)"
	Write-Host ''

	Write-Host '--- Check 1: DUPLICATE-OWNED sources (per link target closure) ---'
	Write-Host "Findings: $($duplicateFindings.Count)"
	$maxPrint = 300
	$printed = 0
	foreach ($d in ($duplicateFindings | Sort-Object ExeProject, Source, Config))
	{
		if ($printed -ge $maxPrint)
		{
			Write-Host "  ... $($duplicateFindings.Count - $maxPrint) more (use -Json for the full list)"
			break
		}
		Write-Host "DUPLICATE $($d.Config) $($d.ExeProject) $($d.Source) owned by: $($d.Owners -join ', ')"
		$printed++
	}
	Write-Host ''

	Write-Host '--- Check 1b: ORPHAN PROJECTS (not reachable from any Application/DynamicLibrary) ---'
	Write-Host "Orphan projects: $($orphanKeys.Count)"
	foreach ($ok in $orphanKeys) { Write-Host "ORPHAN-PROJECT $($projects[$ok].DisplayPath)" }
	Write-Host "Orphan-duplicate findings: $($orphanDuplicateFindings.Count)"
	foreach ($od in ($orphanDuplicateFindings | Sort-Object OrphanProject, Source, AlsoCompiledBy))
	{
		Write-Host "ORPHAN-DUPLICATE $($od.OrphanProject) $($od.Source) also compiled by: $($od.AlsoCompiledBy)"
	}
	Write-Host ''

	Write-Host '--- Check 2: UNOWNED sources (Zenith/**/*.cpp|*.c, representative configs) ---'
	foreach ($cfg in $representativeConfigs)
	{
		$r = $unownedByConfig[$cfg]
		Write-Host "[$cfg] unowned: $($r.Genuine.Count)  (ignored: $($r.IgnoredCount))"
		foreach ($u in $r.Genuine) { Write-Host "UNOWNED $cfg $u" }
	}
	Write-Host ''

	Write-Host '--- Deliverable 2: SourceFilesBuildExcludeRegex.Add(...) call-site gate (FATAL by default) ---'
	Write-Host "Call sites (Build/Sharpmake_*.cs, excluding vendored middleware projects): $($buildExcludeFindings.Count)"
	if ($script:BuildExcludeAllowlist.Count -eq 0)
	{
		Write-Host "Allowlist: EMPTY (Phase 4h.3 drove every call site in the codebase to zero -- see Tools/check_project_membership.ps1's `$script:BuildExcludeAllowlist comment)"
	}
	foreach ($bf in $buildExcludeFindings)
	{
		$tag = if ($bf.Allowlisted) { 'ALLOWLISTED' } else { 'FINDING' }
		Write-Host "$tag $($bf.File):$($bf.Line): $($bf.Text)"
	}
	Write-Host ''

	Write-Host '============================================================'
	Write-Host ' SUMMARY'
	Write-Host '============================================================'
	Write-Host "Projects parsed                        : $($projects.Count)"
	Write-Host "Link targets                           : $($linkTargets.Count)"
	Write-Host "Duplicate-owned findings                : $($duplicateFindings.Count)"
	Write-Host "Orphan projects                         : $($orphanKeys.Count)"
	Write-Host "Orphan-duplicate findings               : $($orphanDuplicateFindings.Count)"
	Write-Host "Unowned sources (genuine)               : $totalGenuineUnowned"
	Write-Host "SourceFilesBuildExcludeRegex.Add() sites : $($buildExcludeFindings.Count)  (fatal: $($buildExcludeFatalFindings.Count), allowlisted: $($buildExcludeFindings.Count - $buildExcludeFatalFindings.Count))"
	Write-Host "Mode                                    : $(if ($fatalMode) { 'FATAL' } else { 'report-only (-ReportOnly)' })"
	Write-Host '============================================================'
}

if ($fatalMode -and $hasFatalFindings) { exit 1 }
exit 0
