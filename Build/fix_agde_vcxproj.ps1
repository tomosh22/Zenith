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
# repos and hung regeneration for many minutes to hours. The
# targeted globs below are bounded to the real output dirs, so this stays fast no
# matter what accumulates elsewhere in the tree.

$files = @()
$files += Get-ChildItem -Path "$PSScriptRoot\*_agde.vcxproj" -ErrorAction SilentlyContinue
$files += Get-ChildItem -Path "$PSScriptRoot\..\Games\*\Build\*_agde.vcxproj" -ErrorAction SilentlyContinue

# Fail-fast guard against an invalid Configuration/Platform pairing (e.g. the
# "arm64_v8a" config selected with the "Android-x86_64" platform, or vice
# versa). Every valid *_agde.sln only ever offers the matching diagonal pair,
# so this can only be reached by invoking an *_agde.vcxproj directly with a
# hand-picked /p:Configuration /p:Platform (or a stale IDE "recent project"
# entry). Without this guard, MSBuild silently falls back to an EMPTY
# AdditionalIncludeDirectories for the un-declared pair (there's no matching
# ItemDefinitionGroup), producing a wall of confusing "'Zenith.h' file not
# found" / "'imgui.h' file not found" errors instead of one clear diagnostic.
$validateTargetMarker = 'ZenithValidateAgdeConfigPlatform'
$validateTarget = @"
  <Target Name="$validateTargetMarker" BeforeTargets="Build;ClCompile;Link;Lib">
    <Error Condition="`$(Configuration.Contains('arm64_v8a')) And '`$(Platform)'!='Android-arm64-v8a'" Text="Zenith AGDE: Configuration '`$(Configuration)' requires Platform 'Android-arm64-v8a', but Platform is '`$(Platform)'. Build via the per-game *_agde.sln (never this .vcxproj directly) so Visual Studio/MSBuild only ever offers valid Configuration/Platform pairs." />
    <Error Condition="`$(Configuration.Contains('x86_64')) And '`$(Platform)'!='Android-x86_64'" Text="Zenith AGDE: Configuration '`$(Configuration)' requires Platform 'Android-x86_64', but Platform is '`$(Platform)'. Build via the per-game *_agde.sln (never this .vcxproj directly) so Visual Studio/MSBuild only ever offers valid Configuration/Platform pairs." />
  </Target>
"@

foreach ($f in $files) {
    Write-Host "Processing: $($f.FullName)"
    $content = Get-Content $f.FullName -Raw
    $content = $content -replace '<CppLanguageStandard>cpp20</CppLanguageStandard>', '<CppLanguageStandard>cpp2a</CppLanguageStandard>'

    if ($content -notmatch [regex]::Escape($validateTargetMarker)) {
        $content = $content -replace '(\s*)</Project>\s*$', "`r`n$validateTarget`r`n</Project>"
    }

    Set-Content $f.FullName $content -Encoding UTF8 -NoNewline
    Write-Host "  Done"
}

Write-Host "`nAll AGDE vcxproj files updated."
