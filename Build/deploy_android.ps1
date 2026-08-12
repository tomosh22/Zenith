# deploy_android.ps1 -- stage native .so libraries from AGDE build output into
# each game's Gradle jniLibs dir so Gradle can package them into the APK.
# =============================================================================
# Descriptor-driven: the game set is every Games/<Name>/<Name>.zproj with
# android:true, so coverage stays in sync automatically.
#
# Usage (from Build/ or anywhere):
#   pwsh ./Build/deploy_android.ps1                 # all android games, debug
#   pwsh ./Build/deploy_android.ps1 release         # all android games, release
#   pwsh ./Build/deploy_android.ps1 debug Combat    # one game
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param(
    [ValidateSet('debug', 'release')][string]$BuildType = 'debug',
    [string]$Game = '',
    # Deliberately NOT a [ValidateSet]: the ABI axis comes from
    # zenith_config.psd1 at runtime, and a literal set here would silently
    # reject a newly-added ABI. Validated against the config below.
    [string]$Abi = 'all'
)

$ErrorActionPreference = 'Stop'
$buildDir = $PSScriptRoot
$repoRoot = Split-Path -Parent $buildDir
Import-Module (Join-Path $buildDir 'zenith_buildsystem.psm1') -Force

# The ABI axis comes from zenith_config.psd1 (mirroring ZenithAndroidAbi in
# Sharpmake_Common.cs / Build/zenith_android_abis.gradle) -- adding an ABI must
# not require editing this script.
$allAbis = Get-ZenithAndroidAbis

if ($Abi -eq 'all') {
    $abis = $allAbis
} else {
    $abis = @($allAbis | Where-Object { $_.DirName -eq $Abi })
    if ($abis.Count -eq 0) {
        $known = ($allAbis | ForEach-Object { $_.DirName }) -join ', '
        Write-Host "deploy_android: unknown ABI '$Abi'. Known ABIs: $known, all" -ForegroundColor Red
        exit 4
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

# ${BuildType} must be brace-delimited: a bare "$BuildType:" parses the colon as a
# scope/drive qualifier (like $env:PATH) and is a hard PARSE error, which took the
# whole script down before it ran a single line.
Write-Host "deploy_android: $($androidGames.Count) android game(s), ${BuildType}: $((@($androidGames | ForEach-Object { $_.Name })) -join ', ')" -ForegroundColor Cyan

$staged = 0
$missing = 0        # an ABI that simply was not built -- informational, NOT a failure
$emptyGames = 0     # a game for which NOTHING could be staged -- the real failure
foreach ($d in $androidGames) {
    $name = $d.Name
    $lib = "lib$($name.ToLowerInvariant()).so"
    $gameStaged = 0
    Write-Host "`n=== $name ==="

    # NOTE the loop variable is NOT $abi: PowerShell variable names are
    # case-insensitive, so $abi would alias the [string]-typed $Abi PARAMETER and
    # silently coerce each ABI object to its ToString() form (leaving .Token empty).
    foreach ($abiEntry in $abis) {
        $a = $abiEntry.DirName
        # The OutDir Sharpmake pins (no backend prefix) -- see Sharpmake_Games.cs.
        $agdeOutDir = Get-ZenithAndroidOutDir -AbiToken $abiEntry.Token -BuildType $BuildType
        $so = Join-Path $repoRoot "Games\$name\Build\output\agde\$agdeOutDir\$a\$lib"
        $jniDir = Join-Path $repoRoot "Games\$name\Android\app\jniLibs\$a"

        if (-not (Test-Path $so)) {
            # Not an error when only some ABIs were built -- Gradle merges
            # whichever exist. Say so and move on.
            Write-Host "  $a : not built ($so)" -ForegroundColor Yellow
            $msbConfig = Get-ZenithAndroidMsBuildConfig -AbiToken $abiEntry.Token -BuildType $BuildType
            Write-Host "    msbuild Games\$name\$($name.ToLowerInvariant())_agde.sln /t:$name /p:Configuration=$msbConfig /p:Platform=Android-$a"
            $missing++
            continue
        }
        New-Item -ItemType Directory -Force -Path $jniDir | Out-Null
        Copy-Item $so (Join-Path $jniDir $lib) -Force
        # No libc++_shared.so to stage: every game links c++_static (see
        # UseOfStl in Build/Sharpmake_Games.cs) -- one native .so per APK, so
        # there's nothing else in the process that would need to share a
        # runtime copy, and static sidesteps the NDK-vendored shared STL's
        # lack of 16 KB page-size alignment (Google Play's AGDE1112 check).
        Write-Host "  $a : staged to $jniDir" -ForegroundColor Green
        $staged++
        $gameStaged++
    }

    if ($gameStaged -eq 0) {
        Write-Host "  no ABI staged for $name" -ForegroundColor Red
        $emptyGames++
    }
}

Write-Host ""
Write-Host "============================================================================"
Write-Host "Done. Staged: $staged  Not built: $missing  Games with nothing staged: $emptyGames"
if ($staged -gt 0) {
    Write-Host ""
    Write-Host "Next steps:"
    Write-Host "  cd Games\<GameName>\Android"
    Write-Host "  .\gradlew assembleDebug"
    # -t is REQUIRED: AGP marks debug APKs testOnly, and adb refuses them without it.
    Write-Host "  adb install -r -t app\build\outputs\apk\debug\app-debug.apk"
}
Write-Host "============================================================================"

# A partially-built axis is the NORMAL case (you usually build one ABI at a time,
# and -Abi defaults to 'all'), so an unbuilt ABI must not fail the script -- that
# is what the "not an error" comment above the $missing++ has always claimed.
# Only a game for which NOTHING could be staged is a genuine failure.
if ($emptyGames -gt 0) { exit 1 }
exit 0
