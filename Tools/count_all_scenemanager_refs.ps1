param([string]$RepoRoot = "C:\dev\Zenith\.claude\worktrees\heuristic-dubinsky-b1c5f3")
$excludeNames = @('Zenith_SceneManager.cpp','Zenith_SceneManager.h','Zenith_SceneManagerInternal.h','Zenith_SceneManagerGuards.h')
$excludeDirPattern = '\\(\.git|complexity_report|complexity_reports|FreeType|Vendor|Sharpmake|sharpmake|cs_build|3rdparty|temp|Temp|TEMP|node_modules)\\'
$buildOutputPattern = '\\(build|Build)\\output\\'
$files = Get-ChildItem -Path $RepoRoot -Recurse -File | Where-Object {
    ($_.Extension -in @('.cpp', '.h', '.inl')) `
    -and ($_.FullName -notmatch $excludeDirPattern) `
    -and ($_.FullName -notmatch $buildOutputPattern) `
    -and ($_.Name -notin $excludeNames)
}
$total = 0
$totalSkip = 0
foreach ($f in $files) {
    $hits = Select-String -Path $f.FullName -Pattern 'Zenith_SceneManager::' -AllMatches
    foreach ($h in $hits) {
        if ($h.Line -match 'CODEMOD_SKIP') { $totalSkip++ } else { $total++ }
    }
}
Write-Host "Total real remaining (excluding CODEMOD_SKIP lines): $total"
Write-Host "Total CODEMOD_SKIP lines (intentional delegators): $totalSkip"
