param([string]$RepoRoot = "C:\dev\Zenith\.claude\worktrees\heuristic-dubinsky-b1c5f3")
$excludeNames = @('Zenith_SceneManager.cpp','Zenith_SceneManager.h','Zenith_SceneManagerInternal.h','Zenith_SceneManagerGuards.h')
$excludeDirPattern = '\\(\.git|complexity_report|complexity_reports|FreeType|Vendor|Sharpmake|sharpmake|cs_build|3rdparty|temp|Temp|TEMP|node_modules)\\'
$buildOutputPattern = '\\(build|Build)\\output\\'
$files = Get-ChildItem -Path $RepoRoot -Recurse -File | Where-Object {
    ($_.Extension -in @('.cpp', '.h', '.inl')) `
    -and ($_.FullName -notmatch $excludeDirPattern) `
    -and ($_.FullName -notmatch $buildOutputPattern) `
    -and ($_.FullName -notmatch '\.Tests\.inl$') `
    -and ($_.Name -notin $excludeNames)
}
$total = 0
$filesWith = 0
foreach ($f in $files) {
    $hitCount = (Select-String -Path $f.FullName -Pattern 'Zenith_SceneManager::' -AllMatches).Matches.Count
    if ($hitCount -gt 0) {
        $total += $hitCount
        $filesWith++
    }
}
Write-Host "Non-test files with Zenith_SceneManager::* remaining: $filesWith"
Write-Host "Total remaining occurrences: $total"
