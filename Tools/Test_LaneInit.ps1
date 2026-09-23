# Test_LaneInit.ps1 -- BOUNDED assertions for Tools/lane_init.ps1's pure and
# bounded helpers.
#
# Deliberately narrow scope (per the reviewed brief): dot-sources
# lane_init.ps1 -NoRun (defines every function, runs nothing -- see
# Tools/Test_TickGates.ps1's identical -NoRun pattern and its own header on
# why dot-sourcing a script that does not guard against running itself is the
# one thing to avoid) and tests ONLY:
#
#   1. The linked-worktree / git-common-dir validation (Confirm-LaneInitPair
#      and its helpers), against a throwaway repo + a REAL `git worktree add`.
#   2. The copy-set rule (Get-LaneInitCopySet / Group-LaneInitFilesByParentDir
#      / Split-LaneInitFileBatches / Copy-LaneInitIgnoredTree) against a
#      KB-scale synthetic fixture directory with a tracked file, an ignored
#      file and an untracked-but-not-ignored file.
#
# This NEVER invokes regen, the build/unit-gate provisioning pass, or
# robocopy against anything under the real C:\dev\Zenith checkout -- every
# fixture is a from-scratch git repo under $env:TEMP, and lane_init.ps1 is
# read only for its own source (via dot-sourcing).
#
# Usage: pwsh -NoProfile -File Tools/Test_LaneInit.ps1
# Exit:  0 all pass, 1 otherwise.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# -NoRun: dot-sourcing a script RUNS it, and a test that pretends to
# provision a lane (regen, build, unit-gate every game) is the one thing this
# switch exists to prevent. -Lane/-Main are Mandatory, so pass real, harmless
# paths -- -NoRun returns before either is ever read.
. (Join-Path $PSScriptRoot 'lane_init.ps1') -Lane $PSScriptRoot -Main $PSScriptRoot -NoRun

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

function New-LaneInitTestRepo {
    $dir = Join-Path ([IO.Path]::GetTempPath()) ("laneinittest_" + [guid]::NewGuid().ToString('N').Substring(0, 10))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    & git -C $dir init --quiet 2>$null | Out-Null
    & git -C $dir config user.email 'test@example.invalid' 2>$null | Out-Null
    & git -C $dir config user.name 'test' 2>$null | Out-Null
    return $dir
}

function New-LaneInitTestCommit {
    param([string]$Dir, [string]$Message = 'init')
    & git -C $Dir add -A 2>$null | Out-Null
    & git -C $Dir commit -q -m $Message 2>$null | Out-Null
}

Write-Host "`n=== Confirm-LaneInitPair / linked-worktree validation ===" -ForegroundColor Cyan

Assert-That 'a real linked worktree sharing the main checkout''s common-dir validates cleanly' {
    $main = New-LaneInitTestRepo
    $wt = Join-Path ([IO.Path]::GetTempPath()) ("laneinittest_wt_" + [guid]::NewGuid().ToString('N').Substring(0, 10))
    try {
        Set-Content -LiteralPath (Join-Path $main 'readme.txt') -Value 'hi' -Encoding utf8
        New-LaneInitTestCommit -Dir $main
        & git -C $main worktree add -q -b lanebranch $wt 2>$null | Out-Null

        # Should not throw.
        Confirm-LaneInitPair -Lane $wt -Main $main
        $true
    }
    finally {
        Remove-Item -LiteralPath $wt -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $main -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Assert-That 'Test-LaneInitIsLinkedWorktree is true for the worktree and false for the main checkout' {
    $main = New-LaneInitTestRepo
    $wt = Join-Path ([IO.Path]::GetTempPath()) ("laneinittest_wt_" + [guid]::NewGuid().ToString('N').Substring(0, 10))
    try {
        Set-Content -LiteralPath (Join-Path $main 'readme.txt') -Value 'hi' -Encoding utf8
        New-LaneInitTestCommit -Dir $main
        & git -C $main worktree add -q -b lanebranch2 $wt 2>$null | Out-Null

        (Test-LaneInitIsLinkedWorktree -RepoPath $wt) -and (-not (Test-LaneInitIsLinkedWorktree -RepoPath $main))
    }
    finally {
        Remove-Item -LiteralPath $wt -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $main -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Assert-That 'the main checkout used as -Lane is refused -- it is not a linked worktree' {
    $main = New-LaneInitTestRepo
    try {
        Set-Content -LiteralPath (Join-Path $main 'readme.txt') -Value 'hi' -Encoding utf8
        New-LaneInitTestCommit -Dir $main
        $threw = $false
        try { Confirm-LaneInitPair -Lane $main -Main $main } catch { $threw = $true }
        $threw
    }
    finally { Remove-Item -LiteralPath $main -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a linked worktree used as -Main is refused -- -Main must be the main checkout' {
    $main = New-LaneInitTestRepo
    $wt = Join-Path ([IO.Path]::GetTempPath()) ("laneinittest_wt_" + [guid]::NewGuid().ToString('N').Substring(0, 10))
    try {
        Set-Content -LiteralPath (Join-Path $main 'readme.txt') -Value 'hi' -Encoding utf8
        New-LaneInitTestCommit -Dir $main
        & git -C $main worktree add -q -b lanebranch3 $wt 2>$null | Out-Null

        $threw = $false
        try { Confirm-LaneInitPair -Lane $wt -Main $wt } catch { $threw = $true }
        $threw
    }
    finally {
        Remove-Item -LiteralPath $wt -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $main -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Assert-That 'two unrelated repositories (different git-common-dir) are refused' {
    $repoA = New-LaneInitTestRepo
    $repoB = New-LaneInitTestRepo
    try {
        Set-Content -LiteralPath (Join-Path $repoA 'a.txt') -Value 'a' -Encoding utf8
        New-LaneInitTestCommit -Dir $repoA
        Set-Content -LiteralPath (Join-Path $repoB 'b.txt') -Value 'b' -Encoding utf8
        New-LaneInitTestCommit -Dir $repoB

        $threw = $false
        try { Confirm-LaneInitPair -Lane $repoB -Main $repoA } catch { $threw = $true }
        $threw
    }
    finally {
        Remove-Item -LiteralPath $repoA -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $repoB -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "`n=== The copy-set rule ===" -ForegroundColor Cyan

function New-LaneInitCopySetFixture {
    # A KB-scale repo with one directory holding a TRACKED file, an IGNORED
    # file, and an untracked file that is NOT ignored -- only the ignored one
    # must ever be selected.
    $repo = New-LaneInitTestRepo
    $subdir = Join-Path $repo 'Stuff'
    New-Item -ItemType Directory -Path $subdir -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $subdir 'Tracked.dat') -Value 'tracked' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $repo '.gitignore') -Value 'Stuff/Ignored.dat' -Encoding utf8
    New-LaneInitTestCommit -Dir $repo
    Set-Content -LiteralPath (Join-Path $subdir 'Ignored.dat') -Value 'ignored' -Encoding utf8
    Set-Content -LiteralPath (Join-Path $subdir 'Untracked.dat') -Value 'untracked but not ignored' -Encoding utf8
    return $repo
}

Assert-That 'Get-LaneInitCopySet selects only the ignored, untracked file -- never the tracked or the plain-untracked one' {
    $repo = New-LaneInitCopySetFixture
    try {
        $found = Get-LaneInitCopySet -MainRoot $repo -RelativeDir 'Stuff'
        (@($found).Count -eq 1) -and (@($found)[0] -eq 'Stuff/Ignored.dat')
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'Get-LaneInitCopySet returns an empty array (never $null) when nothing under the dir is ignored' {
    $repo = New-LaneInitTestRepo
    try {
        New-Item -ItemType Directory -Path (Join-Path $repo 'Empty') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Empty/Tracked.dat') -Value 'tracked' -Encoding utf8
        New-LaneInitTestCommit -Dir $repo
        $found = Get-LaneInitCopySet -MainRoot $repo -RelativeDir 'Empty'
        ($null -ne $found) -and ($found -is [array]) -and (@($found).Count -eq 0)
    }
    finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'Group-LaneInitFilesByParentDir groups files by their own parent directory' {
    $paths = @('Games/A/Assets/x.bin', 'Games/A/Assets/y.bin', 'Games/B/Assets/z.bin')
    $grouped = Group-LaneInitFilesByParentDir -RelativePaths $paths
    $aKey = 'Games\A\Assets'
    $bKey = 'Games\B\Assets'
    ($grouped.Contains($aKey)) -and ($grouped.Contains($bKey)) -and
    ($grouped[$aKey].Count -eq 2) -and ($grouped[$bKey].Count -eq 1) -and
    ($grouped[$aKey] -contains 'x.bin') -and ($grouped[$aKey] -contains 'y.bin') -and
    ($grouped[$bKey] -contains 'z.bin')
}

Assert-That 'Split-LaneInitFileBatches chunks a long list under a small budget, and every leaf survives exactly once' {
    $leaves = New-Object System.Collections.Generic.List[string]
    for ($i = 0; $i -lt 50; $i++) { $leaves.Add("File_$i.dat") }
    $batches = Split-LaneInitFileBatches -Leaves $leaves -MaxChars 100

    $seen = New-Object System.Collections.Generic.List[string]
    foreach ($b in $batches) { foreach ($f in $b) { $seen.Add($f) } }

    ($batches.Count -gt 1) -and
    ($seen.Count -eq $leaves.Count) -and
    (-not (Compare-Object -ReferenceObject @($leaves) -DifferenceObject @($seen)))
}

Assert-That 'Split-LaneInitFileBatches returns exactly one batch when everything fits' {
    $leaves = New-Object System.Collections.Generic.List[string]
    $leaves.Add('a.dat'); $leaves.Add('b.dat')
    $batches = Split-LaneInitFileBatches -Leaves $leaves -MaxChars 20000
    ($batches.Count -eq 1) -and ($batches[0].Count -eq 2)
}

Assert-That 'Copy-LaneInitIgnoredTree end-to-end: copies only the ignored file into the destination lane directory' {
    $repo = New-LaneInitCopySetFixture
    $lane = Join-Path ([IO.Path]::GetTempPath()) ("laneinittest_dst_" + [guid]::NewGuid().ToString('N').Substring(0, 10))
    try {
        New-Item -ItemType Directory -Path $lane -Force | Out-Null
        $copied = Copy-LaneInitIgnoredTree -MainRoot $repo -LaneRoot $lane -RelativeDir 'Stuff' -Label 'Stuff'

        $ignoredCopied = Test-Path -LiteralPath (Join-Path $lane 'Stuff/Ignored.dat')
        $trackedCopied = Test-Path -LiteralPath (Join-Path $lane 'Stuff/Tracked.dat')
        $untrackedCopied = Test-Path -LiteralPath (Join-Path $lane 'Stuff/Untracked.dat')

        ($copied -eq 1) -and $ignoredCopied -and (-not $trackedCopied) -and (-not $untrackedCopied)
    }
    finally {
        Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $lane -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Assert-That 'Copy-LaneInitIgnoredTree returns 0 and copies nothing when the dir has no ignored files' {
    $repo = New-LaneInitTestRepo
    $lane = Join-Path ([IO.Path]::GetTempPath()) ("laneinittest_dst_" + [guid]::NewGuid().ToString('N').Substring(0, 10))
    try {
        New-Item -ItemType Directory -Path (Join-Path $repo 'Empty') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Empty/Tracked.dat') -Value 'tracked' -Encoding utf8
        New-LaneInitTestCommit -Dir $repo
        New-Item -ItemType Directory -Path $lane -Force | Out-Null

        $copied = Copy-LaneInitIgnoredTree -MainRoot $repo -LaneRoot $lane -RelativeDir 'Empty' -Label 'Empty'
        ($copied -eq 0) -and (-not (Test-Path -LiteralPath (Join-Path $lane 'Empty')))
    }
    finally {
        Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath $lane -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host ''
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) {
    Write-Host 'PASS' -ForegroundColor Green
    exit 0
}
Write-Host "$($script:failures) assertion(s) failed" -ForegroundColor Red
exit 1
