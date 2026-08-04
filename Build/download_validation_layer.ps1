# download_validation_layer.ps1 -- fetch the Khronos Vulkan validation layer for
# an Android ABI and stage it into a game's debug jniLibs.
# =============================================================================
# WHY THIS IS USEFUL ON A DEBUG BUILD: the engine requests VK_EXT_debug_utils
# whenever ZENITH_FLUX_PROFILING is defined. On Android that extension is often
# supplied by the validation layer, not the platform loader. The engine treats
# it as optional so an APK without the layer can still boot, but each ABI that
# should provide validation diagnostics needs its own layer .so staged here.
#
# Usage (from Build/ or anywhere):
#   pwsh ./Build/download_validation_layer.ps1                        # TilePuzzle, both ABIs
#   pwsh ./Build/download_validation_layer.ps1 -Abi x86_64            # one ABI
#   pwsh ./Build/download_validation_layer.ps1 -Game Zenithmon        # another game
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param(
    [string]$Game = 'TilePuzzle',
    # Deliberately NOT a [ValidateSet]: the ABI axis comes from
    # zenith_config.psd1 at runtime, and a literal set here would silently
    # reject a newly-added ABI. Validated against the config below.
    [string]$Abi = 'all'
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'zenith_buildsystem.psm1') -Force

# ABI -> the path fragment the layer archive uses for that ABI's .so. Only the
# archive-naming aliases are local; the ABI axis itself comes from the config.
$abiMatch = @{
    'arm64-v8a' = 'arm64-v8a|arm64|aarch64'
    'x86_64'    = 'x86_64|x86-64'
}
$allAbiNames = @((Get-ZenithAndroidAbis) | ForEach-Object { $_.DirName })
if ($Abi -eq 'all') {
    $abis = $allAbiNames
} else {
    $abis = @($allAbiNames | Where-Object { $_ -eq $Abi })
    if ($abis.Count -eq 0) {
        Write-Host "download_validation_layer: unknown ABI '$Abi'. Known ABIs: $($allAbiNames -join ', '), all" -ForegroundColor Red
        exit 4
    }
}

# An ABI added to the axis but not to $abiMatch would silently stage nothing.
$unmatched = @($abis | Where-Object { -not $abiMatch.ContainsKey($_) })
if ($unmatched.Count -gt 0) {
    Write-Host "download_validation_layer: no archive-name pattern for ABI(s): $($unmatched -join ', ')." -ForegroundColor Red
    Write-Host "  Add an entry to `$abiMatch in this script." -ForegroundColor Red
    exit 4
}

$gameAndroidDir = "$PSScriptRoot\..\Games\$Game\Android"
if (-not (Test-Path $gameAndroidDir)) {
    Write-Host "download_validation_layer: '$Game' has no Android/ tree at $gameAndroidDir" -ForegroundColor Red
    exit 2
}

# Skip work entirely if every requested ABI is already staged.
$missing = @($abis | Where-Object {
    -not (Test-Path "$gameAndroidDir\app\src\debug\jniLibs\$_\libVkLayer_khronos_validation.so")
})
if ($missing.Count -eq 0) {
    Write-Host "Validation layer already staged for $Game : $($abis -join ', ')"
    exit 0
}
Write-Host "download_validation_layer: $Game needs $($missing -join ', ')" -ForegroundColor Cyan

Write-Host "Fetching Vulkan Validation Layer releases..."
$releases = Invoke-RestMethod -Uri "https://api.github.com/repos/KhronosGroup/Vulkan-ValidationLayers/releases?per_page=10" -Headers @{"User-Agent" = "Mozilla/5.0" }

$targetRelease = $null
foreach ($release in $releases) {
    if ($release.assets | Where-Object { $_.name -match "android" }) {
        $targetRelease = $release
        Write-Host ("Found release with Android assets: {0}" -f $release.tag_name)
        break
    }
}
if (-not $targetRelease) {
    Write-Host "No release with Android assets found in the 10 most recent releases." -ForegroundColor Red
    exit 1
}

$androidAssets = @($targetRelease.assets | Where-Object { $_.name -match "android" })
$androidAsset = $androidAssets | Where-Object { $_.name -match "\.zip$" } | Select-Object -First 1
if (-not $androidAsset) { $androidAsset = $androidAssets[0] }
Write-Host ("Selected: {0} ({1:N2} MB)" -f $androidAsset.name, ($androidAsset.size / 1MB))

$downloadPath = "$env:TEMP\validation_layer_android.zip"
$extractPath = "$env:TEMP\validation_layer_extract"
Write-Host "Downloading..."
Invoke-WebRequest -Uri $androidAsset.browser_download_url -OutFile $downloadPath -Headers @{"User-Agent" = "Mozilla/5.0" }
if (Test-Path $extractPath) { Remove-Item -Recurse -Force $extractPath }
Write-Host "Extracting..."
Expand-Archive -Path $downloadPath -DestinationPath $extractPath

$staged = 0
$failed = 0
foreach ($a in $missing) {
    $pattern = $abiMatch[$a]
    $soFile = Get-ChildItem -Path $extractPath -Recurse -Filter "libVkLayer_khronos_validation.so" |
        Where-Object { $_.FullName -match $pattern } | Select-Object -First 1
    if (-not $soFile) {
        Write-Host "  $a : NOT FOUND in the archive" -ForegroundColor Yellow
        $failed++
        continue
    }
    $targetDir = "$gameAndroidDir\app\src\debug\jniLibs\$a"
    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
    Copy-Item $soFile.FullName "$targetDir\libVkLayer_khronos_validation.so" -Force
    Write-Host ("  {0} : staged ({1:N2} MB)" -f $a, ($soFile.Length / 1MB)) -ForegroundColor Green
    $staged++
}

Remove-Item -Force $downloadPath -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $extractPath -ErrorAction SilentlyContinue

Write-Host "Done. Staged: $staged  Missing: $failed"
if ($failed -gt 0) { exit 1 }
exit 0
