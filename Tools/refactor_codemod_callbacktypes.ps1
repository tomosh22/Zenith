#!/usr/bin/env pwsh
# Phase 5e codemod: rewrite Zenith_SceneManager::{callback typedef name} usages
# to the top-level Zenith_*Callback / Zenith_SceneCallbackHandle type names.

param(
    [string]$RepoRoot = "C:\dev\Zenith\.claude\worktrees\heuristic-dubinsky-b1c5f3",
    [switch]$DryRun
)

# Ordered list — process longer names first to avoid partial substitutions.
$typeMap = [ordered]@{
    'INVALID_CALLBACK_HANDLE'      = 'Zenith_INVALID_SCENE_CALLBACK_HANDLE'
    'SceneLoadStartedCallback'     = 'Zenith_SceneLoadStartedCallback'
    'EntityPersistentCallback'     = 'Zenith_EntityPersistentCallback'
    'SceneUnloadingCallback'       = 'Zenith_SceneUnloadingCallback'
    'SceneUnloadedCallback'        = 'Zenith_SceneUnloadedCallback'
    'SceneChangedCallback'         = 'Zenith_SceneChangedCallback'
    'SceneLoadedCallback'          = 'Zenith_SceneLoadedCallback'
    'CallbackHandle'               = 'Zenith_SceneCallbackHandle'
}

$excludeNames = @(
    'Zenith_SceneManager.cpp',
    'Zenith_SceneManager.h',
    'Zenith_SceneManagerInternal.h',
    'Zenith_SceneManagerGuards.h',
    'Zenith_SceneSystemGuards.h',
    'Zenith_SceneSystemGuards.cpp',
    'Zenith_SceneCallbackTypes.h'
)

$excludeDirPattern = '\\(\.git|complexity_report|complexity_reports|FreeType|Vendor|Sharpmake|sharpmake|cs_build|3rdparty|temp|Temp|TEMP|node_modules)\\'
$buildOutputPattern = '\\(build|Build)\\output\\'

$files = Get-ChildItem -Path $RepoRoot -Recurse -File | Where-Object {
    ($_.Extension -in @('.cpp', '.h', '.inl')) `
    -and ($_.FullName -notmatch $excludeDirPattern) `
    -and ($_.FullName -notmatch $buildOutputPattern) `
    -and ($_.Name -notin $excludeNames)
}

Write-Host "Scanning $($files.Count) files for callback-typedef usages..."

$totalEdits = 0
$filesEdited = 0

foreach ($file in $files) {
    $content = [System.IO.File]::ReadAllText($file.FullName)
    if (-not $content.Contains('Zenith_SceneManager::')) { continue }

    $originalContent = $content

    foreach ($oldName in $typeMap.Keys) {
        $newName = $typeMap[$oldName]
        $pattern = "Zenith_SceneManager::$oldName\b"
        $content = [System.Text.RegularExpressions.Regex]::Replace($content, $pattern, $newName)
    }

    if ($content -ne $originalContent) {
        $filesEdited++
        $editCount = ([regex]::Matches($originalContent, 'Zenith_SceneManager::')).Count - ([regex]::Matches($content, 'Zenith_SceneManager::')).Count
        $totalEdits += $editCount
        if ($DryRun) {
            Write-Host "[DRY] $($file.FullName): $editCount edit(s)"
        } else {
            [System.IO.File]::WriteAllText($file.FullName, $content)
            Write-Host "[EDIT] $($file.FullName): $editCount edit(s)"
        }
    }
}

Write-Host "Done. Edited $filesEdited file(s); $totalEdits substitution(s)."
