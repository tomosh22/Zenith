# Fix AGDE vcxproj files after Sharpmake generation
# Fix C++ standard from cpp20 to cpp2a
#
# Sharpmake emits *_agde.vcxproj into exactly two locations: Build/ (the engine +
# static-lib projects) and each Games/<Game>/Build/ dir (the game projects). We
# target those directly with non-recursive path globs (the vcxproj sit directly
# in those dirs, never under obj/ or output/) instead of a blind
# `Get-ChildItem -Recurse` from the repo root.
#
# WHY: the old whole-repo recurse descended into .claude/worktrees/, which holds
# dozens of full-repo COPIES left by Claude Code workflow/agent runs -- each a
# complete tree with its own *_agde.vcxproj -- so it walked dozens of nested
# repos and made `Sharpmake_Build.bat` hang for many minutes to hours. The
# targeted globs below are bounded to the real output dirs, so this stays fast no
# matter what accumulates elsewhere in the tree.

$files = @()
$files += Get-ChildItem -Path "$PSScriptRoot\*_agde.vcxproj" -ErrorAction SilentlyContinue
$files += Get-ChildItem -Path "$PSScriptRoot\..\Games\*\Build\*_agde.vcxproj" -ErrorAction SilentlyContinue

foreach ($f in $files) {
    Write-Host "Processing: $($f.FullName)"
    $content = Get-Content $f.FullName -Raw
    $content = $content -replace '<CppLanguageStandard>cpp20</CppLanguageStandard>', '<CppLanguageStandard>cpp2a</CppLanguageStandard>'
    Set-Content $f.FullName $content -Encoding UTF8 -NoNewline
    Write-Host "  Done"
}

Write-Host "`nAll AGDE vcxproj files updated."
