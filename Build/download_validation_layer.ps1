# Download Vulkan validation layer for Android arm64-v8a
# Places it in the debug jniLibs directory for the TilePuzzle game

$ErrorActionPreference = "Stop"

# Target directory
$targetDir = "$PSScriptRoot\..\Games\TilePuzzle\Android\app\src\debug\jniLibs\arm64-v8a"

# Check if already downloaded
if (Test-Path "$targetDir\libVkLayer_khronos_validation.so") {
    Write-Host "Validation layer already exists at $targetDir"
    exit 0
}

# Get recent releases
Write-Host "Fetching Vulkan Validation Layer releases..."
$releases = Invoke-RestMethod -Uri "https://api.github.com/repos/KhronosGroup/Vulkan-ValidationLayers/releases?per_page=10" -Headers @{"User-Agent"="Mozilla/5.0"}

foreach ($release in $releases) {
    Write-Host ("  {0}" -f $release.tag_name)
}

# Find a release with android assets - try SDK 1.3.296.0 or similar
$targetRelease = $null
foreach ($release in $releases) {
    $androidAsset = $release.assets | Where-Object { $_.name -match "android" }
    if ($androidAsset) {
        $targetRelease = $release
        Write-Host ("`nFound release with Android assets: {0}" -f $release.tag_name)
        break
    }
}

if (-not $targetRelease) {
    Write-Host "`nNo release with Android assets found in recent releases."
    Write-Host "Listing all assets from first release for inspection:"
    foreach ($asset in $releases[0].assets) {
        Write-Host ("  {0} ({1:N2} MB)" -f $asset.name, ($asset.size / 1MB))
    }
    exit 1
}

# Find the Android asset - list all matching and pick the right one
$androidAssets = @($targetRelease.assets | Where-Object { $_.name -match "android" })
Write-Host "`nAndroid assets found:"
foreach ($a in $androidAssets) {
    Write-Host ("  {0} ({1:N2} MB)" -f $a.name, ($a.size / 1MB))
}

# Prefer the one that seems to be the validation layer binary (not source, not headers)
$androidAsset = $androidAssets | Where-Object { $_.name -match "\.zip$" } | Select-Object -First 1
if (-not $androidAsset) {
    $androidAsset = $androidAssets[0]
}
Write-Host ("`nSelected: {0} ({1:N2} MB)" -f $androidAsset.name, ($androidAsset.size / 1MB))

# Download
$downloadPath = "$env:TEMP\validation_layer_android.zip"
Write-Host "Downloading to $downloadPath..."
Invoke-WebRequest -Uri $androidAsset.browser_download_url -OutFile $downloadPath -Headers @{"User-Agent"="Mozilla/5.0"}

# Extract
$extractPath = "$env:TEMP\validation_layer_extract"
if (Test-Path $extractPath) { Remove-Item -Recurse -Force $extractPath }
Write-Host "Extracting..."
Expand-Archive -Path $downloadPath -DestinationPath $extractPath

# Find the arm64-v8a .so
Write-Host "Looking for arm64-v8a validation layer..."
$soFile = Get-ChildItem -Path $extractPath -Recurse -Filter "libVkLayer_khronos_validation.so" | Where-Object { $_.FullName -match "arm64" -or $_.FullName -match "aarch64" }

if (-not $soFile) {
    Write-Host "Could not find arm64 validation layer .so. Files found:"
    Get-ChildItem -Path $extractPath -Recurse -Filter "*.so" | ForEach-Object { Write-Host ("  {0}" -f $_.FullName) }
    exit 1
}

Write-Host ("Found: {0}" -f $soFile.FullName)

# Create target directory and copy
New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
Copy-Item $soFile.FullName "$targetDir\libVkLayer_khronos_validation.so"

Write-Host ("`nValidation layer installed to: {0}" -f $targetDir)
Write-Host ("File size: {0:N2} MB" -f ((Get-Item "$targetDir\libVkLayer_khronos_validation.so").Length / 1MB))

# Cleanup
Remove-Item -Force $downloadPath -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $extractPath -ErrorAction SilentlyContinue

Write-Host "Done!"
