# lane_init.ps1 -- provision a linked worktree as a parallel-lane workspace.
# =============================================================================
# Called by `zagent lane add <PROJECT>` (Tools/Vault) after it runs
# `git worktree add --detach <root>/<PROJECT> master`: this script turns that
# bare worktree into something a lane's own /tick can actually build and test
# in, and the lane is registered only if this script exits 0.
#
# A fresh linked worktree has none of the gitignored runtime inputs (slang and
# Sharpmake binaries, the Vulkan SDK), none of the ~14 GB of gitignored source
# art under every Games/*/Assets and Zenith/Assets, and has never been
# regenerated -- so it has no .sln/.vcxproj at all. This copies the ignored
# inputs from the main checkout (NEVER tracked files -- those already exist in
# the lane, checked out from git, and copying over them from a possibly-dirty
# main checkout would silently corrupt a committed .zscen/.znavmesh/.ztexdecl),
# regenerates, and then proves the lane can actually build and pass every
# pinned game's unit gate before `lane add` will register it.
#
# Usage:
#   pwsh -NoProfile -File Tools/lane_init.ps1 -Lane <path> -Main <path> [-SkipAssets] [-SkipProvision] [-NoRun]
#
# -NoRun defines every function and returns without doing anything -- the
# dot-source entry point Tools/Test_LaneInit.ps1 uses to reach the pure/bounded
# helpers without ever touching a real ~14 GB asset copy, a real regen, or a
# real build. Tools/Test_LaneInit.ps1's own header explains why its coverage
# stays deliberately narrow.
#
# Exit: 0 every step passed and the lane is ready to provision; 1 otherwise.
#       Nothing is registered by this script -- that is `zagent lane add`'s job,
#       gated on this exit code.
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Lane,
    [Parameter(Mandatory)][string]$Main,
    [switch]$SkipAssets,
    [switch]$SkipProvision,

    # A lane's FIRST provisioning boot pays a one-time cost no steady-state
    # gate line's own -TimeoutSec budgets for: the tool export/bake re-runs
    # in full (the copied asset manifests key on absolute paths, so a lane's
    # copy from -Main is never seen as warm in the new tree), stacked on top
    # of the full unit suite. Every unit-gate line used during provisioning
    # -- copied from zagent.project.json or synthesized by convention alike
    # -- has its own -TimeoutSec raised to the larger of itself and this
    # value before it runs; steady-state gates (run by the tick afterwards,
    # never by this script) are untouched.
    [int]$ProvisionTimeoutSec = 3600,

    # Define the functions and return without running anything. This is how
    # Tools/Test_LaneInit.ps1 reaches the bounded helpers: dot-sourcing a
    # script RUNS it, and a test that pretends to provision a lane is the one
    # thing this switch exists to prevent -- see Tools/Test_TickGates.ps1's
    # identical -NoRun pattern.
    [switch]$NoRun
)

<#
.SYNOPSIS
    Provisions a linked git worktree as a parallel-lane workspace.
.DESCRIPTION
    Validates that -Lane is a linked worktree sharing -Main's git-common-dir,
    copies the gitignored runtime inputs (slang, Sharpmake, the Vulkan SDK)
    and, unless -SkipAssets, every gitignored asset under Games/*/Assets and
    Zenith/Assets, from -Main into -Lane. Tracked files are never copied --
    the copy-set rule is exactly `git ls-files --others --ignored
    --exclude-standard`, which returns only untracked-and-ignored files.
    Asserts the lane's tree is then clean, regenerates its solutions
    (Build/regen.ps1 -AllowLinkedWorktree), and, unless -SkipProvision, builds
    and unit-gates every game named in the lane's own Tools/unit_baselines.json
    baselines, reading the build and unit-gate command lines from the lane's
    own zagent.project.json where one exists.
.PARAMETER Lane
    The linked worktree to provision. Must be a linked worktree (its git-dir
    differs from its git-common-dir) sharing -Main's git-common-dir.
.PARAMETER Main
    The main checkout to copy runtime inputs and assets from. Must NOT be a
    linked worktree.
.PARAMETER SkipAssets
    Skip the ~14 GB Games/*/Assets + Zenith/Assets copy. For a lane that will
    only be used for a game whose gates never boot an asset-dependent path,
    or for re-running the runtime-input copy and regen alone.
.PARAMETER SkipProvision
    Skip the build + unit-gate pass for every baseline row. Useful for
    re-running only the copy/regen steps.
.PARAMETER ProvisionTimeoutSec
    The minimum -TimeoutSec every provisioning unit-gate line is raised to
    (never lowered). Default 3600 -- a lane's first boot re-runs the full
    tool export/bake on top of the full unit suite, which does not fit the
    600s every steady-state gate line carries.
.PARAMETER NoRun
    Define the functions and return without doing anything.
#>

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# =============================================================================
# Validation: -Lane and -Main share one git-common-dir, -Main is the main
# checkout, -Lane is a linked worktree.
# =============================================================================

function Get-LaneInitGitCommonDir {
    # `git rev-parse --git-common-dir` is RELATIVE (".git") from a main
    # checkout and ABSOLUTE from a linked worktree -- resolve both forms with
    # IsPathRooted rather than assuming either shape.
    param([Parameter(Mandatory)][string]$RepoPath)
    Push-Location -LiteralPath $RepoPath
    try {
        $out = & git rev-parse --git-common-dir 2>$null
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($out)) { return $null }
        $out = $out.Trim()
        if ([IO.Path]::IsPathRooted($out)) {
            return [IO.Path]::GetFullPath($out)
        }
        return [IO.Path]::GetFullPath((Join-Path $RepoPath $out))
    }
    finally { Pop-Location }
}

function Get-LaneInitGitDir {
    param([Parameter(Mandatory)][string]$RepoPath)
    Push-Location -LiteralPath $RepoPath
    try {
        $out = & git rev-parse --git-dir 2>$null
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($out)) { return $null }
        $out = $out.Trim()
        if ([IO.Path]::IsPathRooted($out)) {
            return [IO.Path]::GetFullPath($out)
        }
        return [IO.Path]::GetFullPath((Join-Path $RepoPath $out))
    }
    finally { Pop-Location }
}

function Test-LaneInitIsLinkedWorktree {
    param([Parameter(Mandatory)][string]$RepoPath)
    $gitDir = Get-LaneInitGitDir -RepoPath $RepoPath
    $commonDir = Get-LaneInitGitCommonDir -RepoPath $RepoPath
    if ([string]::IsNullOrEmpty($gitDir) -or [string]::IsNullOrEmpty($commonDir)) { return $false }
    return ($gitDir -ne $commonDir)
}

function Confirm-LaneInitPair {
    # Throws with a message naming the problem; returns nothing on success.
    param([Parameter(Mandatory)][string]$Lane, [Parameter(Mandatory)][string]$Main)

    if (-not (Test-Path -LiteralPath $Lane -PathType Container)) {
        throw "[lane_init] -Lane '$Lane' does not exist"
    }
    if (-not (Test-Path -LiteralPath $Main -PathType Container)) {
        throw "[lane_init] -Main '$Main' does not exist"
    }

    $mainCommon = Get-LaneInitGitCommonDir -RepoPath $Main
    $laneCommon = Get-LaneInitGitCommonDir -RepoPath $Lane
    if ([string]::IsNullOrEmpty($mainCommon)) {
        throw "[lane_init] -Main '$Main' is not a git repository"
    }
    if ([string]::IsNullOrEmpty($laneCommon)) {
        throw "[lane_init] -Lane '$Lane' is not a git repository"
    }
    if ($mainCommon -ne $laneCommon) {
        throw "[lane_init] -Lane and -Main do not share a git-common-dir ('$laneCommon' vs '$mainCommon') -- they are not worktrees of the same repository"
    }

    if (Test-LaneInitIsLinkedWorktree -RepoPath $Main) {
        throw "[lane_init] -Main '$Main' is itself a linked worktree; it must be the main checkout"
    }
    if (-not (Test-LaneInitIsLinkedWorktree -RepoPath $Lane)) {
        throw "[lane_init] -Lane '$Lane' is not a linked worktree"
    }
}

# =============================================================================
# The copy-set rule: exactly `git ls-files --others --ignored --exclude-standard
# -- <dir>` from -Main, grouped by parent directory and copied in
# CreateProcess-safe robocopy batches. NEVER --directory (it collapses dirs
# and would force /XD, which drops ignored content inside a partially-ignored
# tree). NEVER a tracked file -- this git invocation cannot return one.
# =============================================================================

function Get-LaneInitCopySet {
    # Returns the ignored, untracked, forward-slash relative paths under
    # $RelativeDir (a repo-relative, forward-slash directory spec) that
    # `-- <dir>` scoping selects. An array, never $null, never unrolled by a
    # 1-element return (`,@(...)`).
    param([Parameter(Mandatory)][string]$MainRoot, [Parameter(Mandatory)][string]$RelativeDir)
    Push-Location -LiteralPath $MainRoot
    try {
        $out = & git ls-files --others --ignored --exclude-standard -- $RelativeDir 2>$null
        if ($LASTEXITCODE -ne 0) { return , @() }
        return , @($out | Where-Object { $_ })
    }
    finally { Pop-Location }
}

function Group-LaneInitFilesByParentDir {
    # {parent-dir (backslash, repo-relative) -> List[leaf-name]}, insertion
    # ordered so batching below is deterministic.
    param([Parameter(Mandatory)][string[]]$RelativePaths)
    $map = [ordered]@{}
    foreach ($p in $RelativePaths) {
        $native = $p -replace '/', '\'
        $parent = Split-Path -Parent $native
        if ([string]::IsNullOrEmpty($parent)) { $parent = '.' }
        $leaf = Split-Path -Leaf $native
        if (-not $map.Contains($parent)) {
            $map[$parent] = New-Object System.Collections.Generic.List[string]
        }
        $map[$parent].Add($leaf)
    }
    return $map
}

function Split-LaneInitFileBatches {
    # Chunks a leaf-name list so a robocopy command line stays well under the
    # ~32K CreateProcess limit. Budgeted conservatively (20000 chars of leaf
    # names) to leave headroom for the source/dest dirs and robocopy's own
    # switches.
    param([Parameter(Mandatory)][System.Collections.Generic.List[string]]$Leaves, [int]$MaxChars = 20000)
    $batches = New-Object System.Collections.Generic.List[object]
    $current = New-Object System.Collections.Generic.List[string]
    $currentLen = 0
    foreach ($leaf in $Leaves) {
        $addLen = $leaf.Length + 3
        if ($current.Count -gt 0 -and ($currentLen + $addLen) -gt $MaxChars) {
            $batches.Add(@($current.ToArray()))
            $current = New-Object System.Collections.Generic.List[string]
            $currentLen = 0
        }
        $current.Add($leaf)
        $currentLen += $addLen
    }
    if ($current.Count -gt 0) { $batches.Add(@($current.ToArray())) }
    return , @($batches.ToArray())
}

function Invoke-LaneInitRobocopyBatch {
    # Copies one parent directory's worth of named files with one robocopy
    # invocation. Exit 0-7 is success (per robocopy's own convention); >= 8 is
    # failure and this throws.
    param([Parameter(Mandatory)][string]$SourceDir, [Parameter(Mandatory)][string]$DestDir, [Parameter(Mandatory)][string[]]$Leaves)
    New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
    $roboArgs = @($SourceDir, $DestDir) + $Leaves + @('/R:1', '/W:1', '/NP')
    & robocopy @roboArgs | Out-Null
    $rc = $LASTEXITCODE
    if ($rc -ge 8) {
        throw "[lane_init] robocopy exited $rc copying $($Leaves.Count) file(s) '$SourceDir' -> '$DestDir'"
    }
    return $Leaves.Count
}

function Copy-LaneInitIgnoredTree {
    # Applies the copy-set rule to $RelativeDir and copies every returned file
    # from $MainRoot into $LaneRoot at the same relative path, batched per
    # parent directory. Returns the total file count copied.
    param([Parameter(Mandatory)][string]$MainRoot, [Parameter(Mandatory)][string]$LaneRoot, [Parameter(Mandatory)][string]$RelativeDir, [string]$Label = $RelativeDir)

    $files = Get-LaneInitCopySet -MainRoot $MainRoot -RelativeDir $RelativeDir
    if ($files.Count -eq 0) {
        Write-Host "[lane_init]   $Label -- nothing to copy (no ignored files under this path)" -ForegroundColor DarkGray
        return 0
    }

    $grouped = Group-LaneInitFilesByParentDir -RelativePaths $files
    $total = 0
    foreach ($parent in $grouped.Keys) {
        $srcDir = if ($parent -eq '.') { $MainRoot } else { Join-Path $MainRoot $parent }
        $dstDir = if ($parent -eq '.') { $LaneRoot } else { Join-Path $LaneRoot $parent }
        $batches = Split-LaneInitFileBatches -Leaves $grouped[$parent]
        foreach ($batch in $batches) {
            $total += Invoke-LaneInitRobocopyBatch -SourceDir $srcDir -DestDir $dstDir -Leaves $batch
        }
    }
    Write-Host "[lane_init]   $Label -- copied $total file(s)" -ForegroundColor DarkGray
    return $total
}

# =============================================================================
# Step 5: assert the lane's tree is clean after the copy steps.
# =============================================================================

function Assert-LaneInitClean {
    param([Parameter(Mandatory)][string]$LaneRoot)
    Push-Location -LiteralPath $LaneRoot
    try {
        $status = & git status --porcelain 2>$null
        if ($LASTEXITCODE -ne 0) { throw "[lane_init] 'git status --porcelain' failed in lane '$LaneRoot'" }
        if ($status) {
            $lines = @($status) -join "`n"
            throw "[lane_init] lane '$LaneRoot' is not clean after the copy steps:`n$lines"
        }
    }
    finally { Pop-Location }
}

# =============================================================================
# Step 6: regen.
# =============================================================================

function Invoke-LaneInitRegen {
    param([Parameter(Mandatory)][string]$LaneRoot)
    $regenScript = Join-Path $LaneRoot 'Build/regen.ps1'
    if (-not (Test-Path -LiteralPath $regenScript)) {
        throw "[lane_init] Build/regen.ps1 not found at '$regenScript'"
    }
    Push-Location -LiteralPath $LaneRoot
    try {
        # Same audit as Invoke-LaneInitGateLine: this is also a nested `pwsh
        # -File ...` child process, so its stdout is piped to Out-Host rather
        # than left to leak, unsuppressed, into this function's (and in turn
        # Invoke-LaneInit's) own output. $LASTEXITCODE is read into a local
        # BEFORE anything else runs, so a later statement can never clobber
        # it, and the comparison below is always a plain scalar check.
        & pwsh -NoProfile -File $regenScript -AllowLinkedWorktree | Out-Host
        $code = $LASTEXITCODE
        if ($null -eq $code) { $code = 0 }
        if ($code -ne 0) {
            throw "[lane_init] Build/regen.ps1 -AllowLinkedWorktree exited $code in lane '$LaneRoot'"
        }
    }
    finally { Pop-Location }
}

# =============================================================================
# Step 7: per-baseline-row provisioning. Reads the build and unit-gate command
# lines from the lane's own zagent.project.json where a matching one exists,
# rather than hard-coding copies of them here (they would drift).
# =============================================================================

function Get-LaneInitGateLinesForGame {
    # Scans every category's `gates` array in zagent.project.json for a build
    # line naming $Game and a run_unit_gate.ps1 line naming $Game -- a game's
    # build line and its unit-gate line are not necessarily in the same
    # category (Zenithmon's are in the Zenithmon category; Combat's are in
    # Engine).
    param([Parameter(Mandatory)][string]$LaneRoot, [Parameter(Mandatory)][string]$Game)

    $projPath = Join-Path $LaneRoot 'zagent.project.json'
    if (-not (Test-Path -LiteralPath $projPath)) {
        throw "[lane_init] zagent.project.json not found at '$projPath'"
    }
    $proj = Get-Content -LiteralPath $projPath -Raw -Encoding utf8 | ConvertFrom-Json

    $buildLine = $null
    $unitGateLine = $null
    $escaped = [regex]::Escape($Game)
    foreach ($catName in $proj.categories.PSObject.Properties.Name) {
        $cat = $proj.categories.$catName
        if (-not $cat.gates) { continue }
        foreach ($line in $cat.gates) {
            if (-not $buildLine -and $line -match "zenith\.ps1\s+build\s+$escaped\s+--headless") {
                $buildLine = $line
            }
            if (-not $unitGateLine -and $line -match "run_unit_gate\.ps1" -and $line -match "-Game\s+$escaped(\s|$)") {
                $unitGateLine = $line
            }
        }
    }
    return [PSCustomObject]@{ BuildLine = $buildLine; UnitGateLine = $unitGateLine }
}

function Get-LaneInitSynthesizedUnitGateLine {
    # For a game with no run_unit_gate.ps1 line anywhere in zagent.project.json
    # (RenderTest today: CI-advisory only), synthesize one by the same
    # convention every other game's line already follows.
    param([Parameter(Mandatory)][string]$Game)
    $exeName = $Game.ToLowerInvariant()
    return "pwsh -NoProfile -ExecutionPolicy Bypass -File Tools\run_unit_gate.ps1 -Exe Games\$Game\Build\output\win64\null_vs2022_debug_win64_true\$exeName.exe -Game $Game -TimeoutSec 600"
}

function Get-LaneInitBaselineGames {
    # The numeric rows under `baselines` in the lane's unit_baselines.json --
    # today Zenithmon, Combat, RenderTest. $-prefixed keys are comments, not
    # rows, and are excluded.
    param([Parameter(Mandatory)][string]$LaneRoot)
    $manifestPath = Join-Path $LaneRoot 'Tools/unit_baselines.json'
    if (-not (Test-Path -LiteralPath $manifestPath)) {
        throw "[lane_init] Tools/unit_baselines.json not found at '$manifestPath'"
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding utf8 | ConvertFrom-Json
    $games = @($manifest.baselines.PSObject.Properties |
            Where-Object { -not $_.Name.StartsWith('$') } |
            ForEach-Object { $_.Name })
    if ($games.Count -eq 0) {
        throw "[lane_init] no baseline rows found in '$manifestPath'"
    }
    return $games
}

function Invoke-LaneInitGateLine {
    # Runs one gate command line (as zagent.project.json spells it, a single
    # `pwsh ...` invocation) with the lane as the working directory. Returns
    # ONLY its exit code, as a single [int].
    #
    # ★ OUTPUT-POLLUTION HAZARD, MEASURED LIVE (first `zagent lane add ZEN`):
    # `Invoke-Expression $Line` runs a NESTED `pwsh -File ...` as an external
    # child process. Every line that child prints to stdout -- Write-Host
    # included, since Write-Host output still crosses a process boundary as
    # plain bytes once redirected -- becomes a separate string object on
    # THIS function's own success pipeline, because nothing here captures or
    # suppresses it. `return $LASTEXITCODE` then adds one more object (the
    # int) after every one of those lines. A caller that assigns the result
    # (`$buildExit = Invoke-LaneInitGateLine ...`) collects ALL of it into an
    # array, not just the trailing int -- and PowerShell's `-ne` on an array
    # is a FILTER (it returns the elements that are not equal), not a
    # boolean, so `$buildExit -ne 0` was true for almost any array (build
    # output lines are never literally the integer 0) even on a clean, exit-0
    # build. A successful "0 errors" msbuild run was reported as a failure
    # whose "exit code" -- once string-interpolated into the error message --
    # was the entire captured build log with a stray `0` stuck on the end.
    #
    # Also captures a trimmed TAIL of everything the child printed -- stdout
    # AND stderr, merged via appending `2>&1` to the invoked line -- into
    # $script:LaneInitLastGateOutputTail, for a caller to build a richer
    # failure message from. This matters because run_unit_gate.ps1's own
    # diagnostic lines (a timeout, a missing "Unit tests complete" line, the
    # boot log path) go through `Write-Error`, which crosses a process
    # boundary as plain text but is rendered straight to the console and
    # does NOT reach a bare success-stream capture -- confirmed empirically:
    # without the `2>&1` merge, stderr text from a nested `pwsh` child never
    # appears on this function's own pipeline at all, capture list included.
    # `[string]$_` on a merged stderr line yields a readable ErrorRecord
    # rendering (colour escapes and all); the caller strips those before use.
    #
    # A ForEach-Object scriptblock whose statements emit nothing themselves
    # (`List.Add` returns void; Write-Host bypasses the success stream) is
    # itself a sink -- so, like Out-Host, this function's only pipeline
    # output is still the single explicit `return` below, and the fix this
    # function exists for (a plain, single [int] return) is unchanged.
    param([Parameter(Mandatory)][string]$LaneRoot, [Parameter(Mandatory)][string]$Line, [int]$TailLines = 60)
    Push-Location -LiteralPath $LaneRoot
    try {
        $captured = New-Object System.Collections.Generic.List[string]
        Invoke-Expression "$Line 2>&1" | ForEach-Object {
            $text = [string]$_
            $captured.Add($text)
            Write-Host $text
        }
        $script:LaneInitLastGateOutputTail =
        if ($captured.Count -gt $TailLines) { @($captured.GetRange($captured.Count - $TailLines, $TailLines)) }
        else { @($captured.ToArray()) }
        $code = $LASTEXITCODE
        if ($null -eq $code) { $code = 0 }
        return [int]$code
    }
    finally { Pop-Location }
}

function Get-LaneInitCleanOutputTail {
    # $script:LaneInitLastGateOutputTail, joined and stripped of ANSI colour
    # escapes (a merged stderr ErrorRecord renders with them), for embedding
    # in a failure message.
    $lines = if ($script:LaneInitLastGateOutputTail) { $script:LaneInitLastGateOutputTail } else { @() }
    $joined = ($lines -join "`n")
    $esc = [string][char]27
    $ansiPattern = [regex]::Escape($esc) + '\[[0-9;]*m'
    return ($joined -replace $ansiPattern, '')
}

function Set-LaneInitUnitGateTimeout {
    # Rewrites (or appends) a run_unit_gate.ps1 command line's -TimeoutSec to
    # the larger of its existing value and $MinimumSeconds -- never smaller,
    # and byte-unchanged when the existing value already meets it. A lane's
    # FIRST provisioning boot pays a one-time cost no steady-state gate
    # line's own -TimeoutSec budgets for: the tool export/bake re-runs in
    # full (the copied asset manifests key on absolute paths, so a lane's
    # copy from -Main is never seen as warm in the new tree), stacked on top
    # of the full unit suite. Applies the same way to a line copied verbatim
    # from zagent.project.json and to one this script synthesized by
    # convention -- both already spell `-TimeoutSec <n>`.
    param([Parameter(Mandatory)][string]$Line, [Parameter(Mandatory)][int]$MinimumSeconds)
    if ($Line -match '-TimeoutSec\s+(\d+)') {
        $existing = [int]$Matches[1]
        if ($existing -ge $MinimumSeconds) { return $Line }
        return ($Line -replace '-TimeoutSec\s+\d+', "-TimeoutSec $MinimumSeconds")
    }
    return "$Line -TimeoutSec $MinimumSeconds"
}

function Invoke-LaneInitProvisionCheck {
    # For every game in the lane's own unit_baselines.json, build
    # Null_vs2022_Debug_Win64_True (via that game's --headless build line) and
    # run its unit gate, requiring both to succeed. Throws on the first
    # failure, naming the game and the command, with a trimmed tail of the
    # failing gate's own output attached (build and unit-gate alike) so the
    # cause is visible without having to open a multi-thousand-line boot log
    # by hand.
    param([Parameter(Mandatory)][string]$LaneRoot, [int]$ProvisionTimeoutSec = 3600)

    $games = Get-LaneInitBaselineGames -LaneRoot $LaneRoot
    foreach ($game in $games) {
        $lines = Get-LaneInitGateLinesForGame -LaneRoot $LaneRoot -Game $game
        if (-not $lines.BuildLine) {
            throw "[lane_init] no '--headless' build gate line for '$game' found in zagent.project.json"
        }
        $unitLine = $lines.UnitGateLine
        if (-not $unitLine) {
            $unitLine = Get-LaneInitSynthesizedUnitGateLine -Game $game
            Write-Host "[lane_init]   $game has no run_unit_gate.ps1 line in zagent.project.json; synthesizing by convention" -ForegroundColor Yellow
        }

        # A lane's FIRST provisioning boot re-runs the full tool export/bake
        # on top of the full unit suite (see Set-LaneInitUnitGateTimeout's
        # own comment), which the gate line's copied-or-synthesized
        # `-TimeoutSec 600` does not budget for -- raise it, never lower it.
        $rewrittenUnitLine = Set-LaneInitUnitGateTimeout -Line $unitLine -MinimumSeconds $ProvisionTimeoutSec
        if ($rewrittenUnitLine -ne $unitLine) {
            Write-Host "[lane_init]   $game -- unit gate timeout raised to ${ProvisionTimeoutSec}s for provisioning" -ForegroundColor DarkGray
        }
        else {
            Write-Host "[lane_init]   $game -- unit gate timeout already >= ${ProvisionTimeoutSec}s; unchanged" -ForegroundColor DarkGray
        }
        $unitLine = $rewrittenUnitLine

        Write-Host "[lane_init]   $game -- build" -ForegroundColor Cyan
        $buildExit = Invoke-LaneInitGateLine -LaneRoot $LaneRoot -Line $lines.BuildLine
        if ($buildExit -ne 0) {
            $tail = Get-LaneInitCleanOutputTail
            throw "[lane_init] build failed for '$game' (exit $buildExit): $($lines.BuildLine)`n--- last $(@($script:LaneInitLastGateOutputTail).Count) output line(s) ---`n$tail"
        }

        Write-Host "[lane_init]   $game -- unit gate" -ForegroundColor Cyan
        $unitExit = Invoke-LaneInitGateLine -LaneRoot $LaneRoot -Line $unitLine
        if ($unitExit -ne 0) {
            $tail = Get-LaneInitCleanOutputTail
            $timedOut = $tail -match '(?i)timeout after \d+s'
            $timeoutNote = if ($timedOut) { ' -- TIMED OUT' } else { '' }
            $logNote = ''
            if ($tail -match '(?i)\(log:\s*([^)]+)\)') {
                $logNote = " -- boot log: $($Matches[1].Trim())"
            }
            throw "[lane_init] unit gate failed for '$game'$timeoutNote (exit $unitExit)$logNote`: $unitLine`n--- last $(@($script:LaneInitLastGateOutputTail).Count) output line(s) ---`n$tail"
        }
    }
    return $games
}

# =============================================================================
# Orchestration.
# =============================================================================

function Invoke-LaneInit {
    param(
        [Parameter(Mandatory)][string]$Lane,
        [Parameter(Mandatory)][string]$Main,
        [switch]$SkipAssets,
        [switch]$SkipProvision,
        [int]$ProvisionTimeoutSec = 3600
    )

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $laneRoot = (Resolve-Path -LiteralPath $Lane).Path
    $mainRoot = (Resolve-Path -LiteralPath $Main).Path

    Write-Host "[lane_init] Validating lane/main pair..." -ForegroundColor Cyan
    Confirm-LaneInitPair -Lane $laneRoot -Main $mainRoot

    Write-Host "[lane_init] Copying gitignored runtime inputs..." -ForegroundColor Cyan
    $filesCopied = 0
    $filesCopied += Copy-LaneInitIgnoredTree -MainRoot $mainRoot -LaneRoot $laneRoot -RelativeDir 'Middleware/slang/bin' -Label 'Middleware/slang/bin'
    $filesCopied += Copy-LaneInitIgnoredTree -MainRoot $mainRoot -LaneRoot $laneRoot -RelativeDir 'Sharpmake' -Label 'Sharpmake'
    $filesCopied += Copy-LaneInitIgnoredTree -MainRoot $mainRoot -LaneRoot $laneRoot -RelativeDir 'Middleware/VulkanSDK' -Label 'Middleware/VulkanSDK'

    if ($SkipAssets) {
        Write-Host "[lane_init] -SkipAssets: skipping the Games/*/Assets + Zenith/Assets copy" -ForegroundColor Yellow
    }
    else {
        Write-Host "[lane_init] Copying assets (source art included -- this can take several minutes over ~14 GB)..." -ForegroundColor Cyan
        $assetDirs = New-Object System.Collections.Generic.List[string]
        if (Test-Path -LiteralPath (Join-Path $mainRoot 'Zenith/Assets')) { $assetDirs.Add('Zenith/Assets') }
        $gamesRoot = Join-Path $mainRoot 'Games'
        if (Test-Path -LiteralPath $gamesRoot) {
            foreach ($gameDir in (Get-ChildItem -LiteralPath $gamesRoot -Directory | Sort-Object Name)) {
                $assetsDir = Join-Path $gameDir.FullName 'Assets'
                if (Test-Path -LiteralPath $assetsDir) {
                    $assetDirs.Add("Games/$($gameDir.Name)/Assets")
                }
            }
        }
        foreach ($rel in $assetDirs) {
            $filesCopied += Copy-LaneInitIgnoredTree -MainRoot $mainRoot -LaneRoot $laneRoot -RelativeDir $rel -Label $rel
        }
    }

    Write-Host "[lane_init] Verifying the lane's tree is clean after the copy steps..." -ForegroundColor Cyan
    Assert-LaneInitClean -LaneRoot $laneRoot

    Write-Host "[lane_init] Regenerating solutions (Build/regen.ps1 -AllowLinkedWorktree)..." -ForegroundColor Cyan
    Invoke-LaneInitRegen -LaneRoot $laneRoot

    $gamesProvisioned = @()
    if ($SkipProvision) {
        Write-Host "[lane_init] -SkipProvision: skipping the build + unit-gate pass" -ForegroundColor Yellow
    }
    else {
        Write-Host "[lane_init] Provisioning: build + unit gate for every baseline row..." -ForegroundColor Cyan
        $gamesProvisioned = Invoke-LaneInitProvisionCheck -LaneRoot $laneRoot -ProvisionTimeoutSec $ProvisionTimeoutSec
    }

    $sw.Stop()
    $minutes = [Math]::Round($sw.Elapsed.TotalMinutes, 1)
    Write-Host "[lane_init] Done in $minutes min. Files copied: $filesCopied. Games provisioned: $(@($gamesProvisioned) -join ', ')." -ForegroundColor Green
}

# =============================================================================
if ($NoRun) { return }

try {
    Invoke-LaneInit -Lane $Lane -Main $Main -SkipAssets:$SkipAssets -SkipProvision:$SkipProvision -ProvisionTimeoutSec $ProvisionTimeoutSec
    exit 0
}
catch {
    Write-Host ''
    Write-Host "[lane_init] FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
