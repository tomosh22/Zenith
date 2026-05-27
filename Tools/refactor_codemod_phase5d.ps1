#!/usr/bin/env pwsh
# Phase 5d codemod: rewrite Zenith_SceneManager:: call sites in .Tests.inl
# test files to use the subsystem accessors on g_xEngine. Mirrors the Phase 5c
# routing table; only the file filter differs (we INCLUDE .Tests.inl files
# this time, which Phase 5c excluded).

param(
    [string]$RepoRoot = "C:\dev\Zenith\.claude\worktrees\heuristic-dubinsky-b1c5f3",
    [switch]$DryRun
)

# Routing table (identical to Phase 5c).
$routing = @{
    # SceneRegistry
    'AddToSceneNameCache'                = @('SceneRegistry', 'AddToSceneNameCache')
    'RemoveFromSceneNameCache'           = @('SceneRegistry', 'RemoveFromSceneNameCache')
    'RenameScene'                        = @('SceneRegistry', 'RenameScene')
    'GetPersistentScene'                 = @('SceneRegistry', 'GetPersistentScene')
    'GetSceneData'                       = @('SceneRegistry', 'GetSceneData')
    'GetSceneDataByHandle'               = @('SceneRegistry', 'GetSceneDataByHandle')
    'GetSceneDataForEntity'              = @('SceneRegistry', 'GetSceneDataForEntity')
    'GetSceneFromHandle'                 = @('SceneRegistry', 'GetSceneFromHandle')
    'AllocateSceneHandle'                = @('SceneRegistry', 'AllocateSceneHandle')
    'FreeSceneHandle'                    = @('SceneRegistry', 'FreeSceneHandle')
    'GetLoadedSceneCount'                = @('SceneRegistry', 'GetLoadedSceneCount')
    'GetTotalSceneCount'                 = @('SceneRegistry', 'GetTotalSceneCount')
    'GetBuildSceneCount'                 = @('SceneRegistry', 'GetBuildSceneCount')
    'GetActiveScene'                     = @('SceneRegistry', 'GetActiveScene')
    'GetSceneAt'                         = @('SceneRegistry', 'GetSceneAt')
    'GetSceneByBuildIndex'               = @('SceneRegistry', 'GetSceneByBuildIndex')
    'GetSceneByName'                     = @('SceneRegistry', 'GetSceneByName')
    'GetSceneByPath'                     = @('SceneRegistry', 'GetSceneByPath')
    'RegisterSceneBuildIndex'            = @('SceneRegistry', 'RegisterSceneBuildIndex')
    'ClearBuildIndexRegistry'            = @('SceneRegistry', 'ClearBuildIndexRegistry')
    'GetBuildIndexRegistrySize'          = @('SceneRegistry', 'GetBuildIndexRegistrySize')
    'MakeInvalidScene'                   = @('SceneRegistry', 'MakeInvalidScene')
    'IsSceneVisibleToUser'               = @('SceneRegistry', 'IsSceneVisibleToUser')
    'IsSceneUpdatable'                   = @('SceneRegistry', 'IsSceneUpdatable')
    'CreateScene'                        = @('SceneRegistry', 'CreateScene')
    'CreateEmptyScene'                   = @('SceneRegistry', 'CreateEmptyScene')
    'SetActiveScene'                     = @('SceneRegistry', 'SetActiveScene')
    'SetScenePaused'                     = @('SceneRegistry', 'SetScenePaused')
    'IsScenePaused'                      = @('SceneRegistry', 'IsScenePaused')
    'FindMainCameraAcrossScenes'         = @('SceneRegistry', 'FindMainCameraAcrossScenes')
    'GetSceneSlotCount'                  = @('SceneRegistry', 'GetSceneSlotCount')
    'GetSceneDataAtSlot'                 = @('SceneRegistry', 'GetSceneDataAtSlot')
    'GetLoadedSceneDataAtSlot'           = @('SceneRegistry', 'GetLoadedSceneDataAtSlot')
    'SelectNewActiveScene'               = @('SceneRegistry', 'SelectNewActiveScene')
    'GetRegisteredScenePath'             = @('SceneRegistry', 'GetRegisteredScenePath')
    'GetAllOfComponentTypeFromAllScenes' = @('SceneRegistry', 'GetAllOfComponentTypeFromAllScenes')

    # SceneOperations
    'CountScenesBeingAsyncUnloaded'      = @('SceneOperations', 'CountScenesBeingAsyncUnloaded')
    'NotifyAsyncJobPriorityChanged'      = @('SceneOperations', 'NotifyAsyncJobPriorityChanged')
    'GetOperation'                       = @('SceneOperations', 'GetOperation')
    'IsOperationValid'                   = @('SceneOperations', 'IsOperationValid')
    'SetAsyncUnloadBatchSize'            = @('SceneOperations', 'SetAsyncUnloadBatchSize')
    'GetAsyncUnloadBatchSize'            = @('SceneOperations', 'GetAsyncUnloadBatchSize')
    'SetMaxConcurrentAsyncLoads'         = @('SceneOperations', 'SetMaxConcurrentAsyncLoads')
    'GetMaxConcurrentAsyncLoads'         = @('SceneOperations', 'GetMaxConcurrentAsyncLoads')
    'GetUnloadUnusedAssetsCallCount'     = @('SceneOperations', 'GetUnloadUnusedAssetsCallCount')
    'LoadScene'                          = @('SceneOperations', 'LoadScene')
    'LoadSceneByIndex'                   = @('SceneOperations', 'LoadSceneByIndex')
    'LoadSceneBlockingForBootstrap'      = @('SceneOperations', 'LoadSceneBlockingForBootstrap')
    'LoadSceneByIndexBlockingForBootstrap' = @('SceneOperations', 'LoadSceneByIndexBlockingForBootstrap')
    'LoadSceneBlocking_ToolsOnly'        = @('SceneOperations', 'LoadSceneBlocking_ToolsOnly')
    'LoadSceneByIndexBlocking_ToolsOnly' = @('SceneOperations', 'LoadSceneByIndexBlocking_ToolsOnly')
    'LoadSceneAsync'                     = @('SceneOperations', 'LoadSceneAsync')
    'LoadSceneAsyncByIndex'              = @('SceneOperations', 'LoadSceneAsyncByIndex')
    'UnloadScene'                        = @('SceneOperations', 'UnloadScene')
    'UnloadSceneAsync'                   = @('SceneOperations', 'UnloadSceneAsync')
    'UnloadSceneForced'                  = @('SceneOperations', 'UnloadSceneForced')
    'UnloadUnusedAssets'                 = @('SceneOperations', 'UnloadUnusedAssets')
    'HasPendingDestructions'             = @('SceneOperations', 'HasPendingDestructions')
    'ResetAllRenderSystems'              = @('SceneOperations', 'ResetAllRenderSystems')

    # SceneLifecycle
    'Update'                             = @('SceneLifecycle', 'Update')
    'WaitForUpdateComplete'              = @('SceneLifecycle', 'WaitForUpdateComplete')
    'SetFixedTimestep'                   = @('SceneLifecycle', 'SetFixedTimestep')
    'GetFixedTimestep'                   = @('SceneLifecycle', 'GetFixedTimestep')
    'IsLoadingScene'                     = @('SceneLifecycle', 'IsLoadingScene')
    'IsPrefabInstantiating'              = @('SceneLifecycle', 'IsPrefabInstantiating')
    'IsUpdating'                         = @('SceneLifecycle', 'IsUpdating')
    'GetPendingBuildIndex'               = @('SceneLifecycle', 'GetPendingBuildIndex')
    'IsCircularLoadDependency'           = @('SceneLifecycle', 'IsCircularLoadDependency')
    'SetInitialSceneLoadCallback'        = @('SceneLifecycle', 'SetInitialSceneLoadCallback')
    'GetInitialSceneLoadCallback'        = @('SceneLifecycle', 'GetInitialSceneLoadCallback')
    'DispatchFullLifecycleInit'          = @('SceneLifecycle', 'DispatchFullLifecycleInit')
    'PushLifecycleContext'               = @('SceneLifecycle', 'PushLifecycleContext')
    'PopLifecycleContext'                = @('SceneLifecycle', 'PopLifecycleContext')
    'GetDefaultCreationScene'            = @('SceneLifecycle', 'GetDefaultCreationScene')
    'SetMainLoopRunning'                 = @('SceneLifecycle', 'SetMainLoopRunning')
    'GetLastDeferredLoadOp'              = @('SceneLifecycle', 'GetLastDeferredLoadOp')

    # SceneCallbacks (callback names drop the "Callback" suffix on the subsystem)
    'IsActiveSceneSuppressed'            = @('SceneCallbacks', 'IsActiveSceneSuppressed')
    'RegisterActiveSceneChangedCallback' = @('SceneCallbacks', 'RegisterActiveSceneChanged')
    'UnregisterActiveSceneChangedCallback' = @('SceneCallbacks', 'UnregisterActiveSceneChanged')
    'RegisterSceneLoadedCallback'        = @('SceneCallbacks', 'RegisterSceneLoaded')
    'UnregisterSceneLoadedCallback'      = @('SceneCallbacks', 'UnregisterSceneLoaded')
    'RegisterSceneUnloadingCallback'     = @('SceneCallbacks', 'RegisterSceneUnloading')
    'UnregisterSceneUnloadingCallback'   = @('SceneCallbacks', 'UnregisterSceneUnloading')
    'RegisterSceneUnloadedCallback'      = @('SceneCallbacks', 'RegisterSceneUnloaded')
    'UnregisterSceneUnloadedCallback'    = @('SceneCallbacks', 'UnregisterSceneUnloaded')
    'RegisterSceneLoadStartedCallback'   = @('SceneCallbacks', 'RegisterSceneLoadStarted')
    'UnregisterSceneLoadStartedCallback' = @('SceneCallbacks', 'UnregisterSceneLoadStarted')
    'RegisterEntityPersistentCallback'   = @('SceneCallbacks', 'RegisterEntityPersistent')
    'UnregisterEntityPersistentCallback' = @('SceneCallbacks', 'UnregisterEntityPersistent')
    'FireSceneLoadedCallbacks'           = @('SceneCallbacks', 'FireSceneLoaded')
    'FireSceneUnloadingCallbacks'        = @('SceneCallbacks', 'FireSceneUnloading')
    'FireSceneUnloadedCallbacks'         = @('SceneCallbacks', 'FireSceneUnloaded')
    'FireActiveSceneChangedCallbacks'    = @('SceneCallbacks', 'FireActiveSceneChanged')
    'FireSceneLoadStartedCallbacks'      = @('SceneCallbacks', 'FireSceneLoadStarted')
    'FireEntityPersistentCallbacks'      = @('SceneCallbacks', 'FireEntityPersistent')
}

# Class-static targets (Zenith_SceneEntityOwnership::Foo)
$ownershipMethods = @(
    'Destroy', 'DestroyImmediate', 'MoveEntityToScene', 'MergeScenes', 'MarkEntityPersistent', 'CreateEntity'
)

# === File discovery — Phase 5d targets only .Tests.inl files =================

$excludeDirPattern = '\\(\.git|complexity_report|complexity_reports|FreeType|Vendor|Sharpmake|sharpmake|cs_build|3rdparty|temp|Temp|TEMP|node_modules)\\'
$buildOutputPattern = '\\(build|Build)\\output\\'

$files = Get-ChildItem -Path $RepoRoot -Recurse -File | Where-Object {
    ($_.FullName -match '\.Tests\.inl$') `
    -and ($_.FullName -notmatch $excludeDirPattern) `
    -and ($_.FullName -notmatch $buildOutputPattern)
}

Write-Host "Scanning $($files.Count) Tests.inl files..."

$totalEdits = 0
$filesEdited = 0

foreach ($file in $files) {
    $content = [System.IO.File]::ReadAllText($file.FullName)
    if (-not $content.Contains('Zenith_SceneManager::')) { continue }

    $originalContent = $content

    $lines = $content -split "(?<=\r?\n)"
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]
        if ($line.Contains('CODEMOD_SKIP')) { continue }

        foreach ($methodName in $routing.Keys) {
            $target = $routing[$methodName]
            $accessor = $target[0]
            $newName = $target[1]
            $pattern = "Zenith_SceneManager::$methodName\b"
            $replacement = "g_xEngine.$accessor().$newName"
            $line = [System.Text.RegularExpressions.Regex]::Replace($line, $pattern, $replacement)
        }

        foreach ($methodName in $ownershipMethods) {
            $pattern = "Zenith_SceneManager::$methodName\b"
            $replacement = "Zenith_SceneEntityOwnership::$methodName"
            $line = [System.Text.RegularExpressions.Regex]::Replace($line, $pattern, $replacement)
        }

        $lines[$i] = $line
    }
    $content = -join $lines

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
