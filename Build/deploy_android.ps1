# deploy_android.ps1 -- stage native .so libraries from AGDE build output into
# each game's Gradle jniLibs dir so Gradle can package them into the APK.
# =============================================================================
# Descriptor-driven: the game set is every Games/<Name>/<Name>.zproj with
# android:true, so coverage stays in sync automatically (today: 10 games -- the
# old hand-maintained list covered 8, this gains RenderTest + DevilsPlayground).
#
# Usage (from Build/ or anywhere):
#   pwsh ./Build/deploy_android.ps1                 # all android games, debug
#   pwsh ./Build/deploy_android.ps1 release         # all android games, release
#   pwsh ./Build/deploy_android.ps1 debug Sokoban   # one game
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param(
    [ValidateSet('debug', 'release')][string]$BuildType = 'debug',
    [string]$Game = ''
)

$ErrorActionPreference = 'Stop'
$buildDir = $PSScriptRoot
$repoRoot = Split-Path -Parent $buildDir
Import-Module (Join-Path $buildDir 'zenith_buildsystem.psm1') -Force

$agdeConfig = "arm64_v8a_vs2022_${BuildType}_agde_false"
$abi = 'arm64-v8a'

# libc++_shared.so from the NDK (AGDE uses the c++_shared STL).
$cppShared = $null
foreach ($ndkRoot in @($env:ANDROID_NDK_ROOT, $env:ANDROID_NDK)) {
    if ($ndkRoot -and -not $cppShared) {
        $candidate = Join-Path $ndkRoot 'toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\aarch64-linux-android\libc++_shared.so'
        if (Test-Path $candidate) { $cppShared = $candidate }
    }
}

# Discover android:true games from descriptors.
$scan = Get-ZenithGameDescriptors
if ($scan.Errors.Count -gt 0) {
    Write-Host "deploy_android: descriptor validation failed:" -ForegroundColor Red
    $scan.Errors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    exit 3
}
$androidGames = @($scan.Descriptors | Where-Object { $_.Android })
if ($Game -ne '') {
    $androidGames = @($androidGames | Where-Object { $_.Name -ieq $Game })
    if ($androidGames.Count -eq 0) {
        Write-Host "deploy_android: '$Game' is not an android:true game." -ForegroundColor Red
        exit 5
    }
}

Write-Host "deploy_android: $($androidGames.Count) android game(s), $BuildType: $((@($androidGames | ForEach-Object { $_.Name })) -join ', ')" -ForegroundColor Cyan

$staged = 0
$failed = 0
foreach ($d in $androidGames) {
    $name = $d.Name
    $lib = "lib$($name.ToLowerInvariant()).so"
    $so = Join-Path $repoRoot "Games\$name\Build\output\agde\$agdeConfig\$lib"
    $jniDir = Join-Path $repoRoot "Games\$name\Android\app\jniLibs\$abi"

    Write-Host "`n=== $name ==="
    if (-not (Test-Path $so)) {
        Write-Host "  WARNING: $so not found" -ForegroundColor Yellow
        Write-Host "  Build the game's AGDE solution first:" -ForegroundColor Yellow
        Write-Host "    msbuild Games\$name\$($name.ToLowerInvariant())_agde.sln /p:Configuration=$agdeConfig /p:Platform=Android-arm64-v8a"
        $failed++
        continue
    }
    New-Item -ItemType Directory -Force -Path $jniDir | Out-Null
    Copy-Item $so (Join-Path $jniDir $lib) -Force
    Write-Host "  Copied $lib"
    if ($cppShared) {
        Copy-Item $cppShared (Join-Path $jniDir 'libc++_shared.so') -Force
        Write-Host "  Copied libc++_shared.so"
    }
    Write-Host "  Staged to $jniDir" -ForegroundColor Green
    $staged++
}

Write-Host ""
Write-Host "============================================================================"
Write-Host "Done. Staged: $staged  Failed/Missing: $failed"
if ($staged -gt 0) {
    Write-Host ""
    Write-Host "Next steps:"
    Write-Host "  cd Games\<GameName>\Android"
    Write-Host "  .\gradlew assembleDebug"
    Write-Host "  adb install -r app\build\outputs\apk\debug\app-debug.apk"
}
Write-Host "============================================================================"

if ($failed -gt 0) { exit 1 }
exit 0
