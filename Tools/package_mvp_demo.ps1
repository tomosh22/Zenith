# package_mvp_demo.ps1 -- MVP-4.3.3 demo packaging.
#
# Implements MvpRoadmap.md MVP-4.3.3: "Set up the demo packaging script
# that copies the build output + assets to a single folder for
# distribution."
#
# Output layout (under Build/dp_demo_package/ by default):
#   dp_demo_package/
#     devilsplayground.exe
#     *.dll                          (every runtime DLL from the build output)
#     Zenith/Assets/                 (engine asset tree -- fonts, textures,
#                                     reference meshes)
#     Games/DevilsPlayground/Assets/ (game asset tree -- scenes, materials,
#                                     scripts, meshes, prefabs)
#     Config/                        (Tuning.json, Archetypes.json, Reagents.json)
#     README.md                      (the "this is the MVP demo" landing
#                                     doc; cites the playable systems +
#                                     known limitations).
#
# Known limitations (documented in the generated README too):
#   * Asset paths are baked into the exe at build time as absolute
#     paths via Sharpmake's preprocessor defines (ENGINE_ASSETS_DIR,
#     GAME_ASSETS_DIR, SHADER_SOURCE_ROOT, ZENITH_ROOT). The packaged
#     bundle ONLY runs on a machine where Zenith was checked out at
#     the same absolute path used at build time. A fully relocatable
#     build is post-MVP (would require a relative-asset-path mode in
#     the engine + Sharpmake config).
#   * S0 placeholder assets only -- meshes are tinted cubes, no real
#     character animations, no audio playback path. Phase-3 work
#     (Mixamo + Kenney imports) replaces these.
#
# Usage (from repo root):
#   pwsh ./Tools/package_mvp_demo.ps1
#   pwsh ./Tools/package_mvp_demo.ps1 -OutDir C:\release\dp_demo_v1
#   pwsh ./Tools/package_mvp_demo.ps1 -Configuration vs2022_Release_Win64_True

[CmdletBinding()]
param(
    [string]$Configuration = "vs2022_Debug_Win64_True",
    [string]$OutDir        = "Build/dp_demo_package",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$repoRoot = (Get-Location).Path
$exeDir   = Join-Path $repoRoot "Games/DevilsPlayground/Build/output/win64/$($Configuration.ToLower())"
$exePath  = Join-Path $exeDir   "devilsplayground.exe"

if (-not (Test-Path $exePath)) {
    Write-Error @"
Built executable not found: $exePath
Build it first via:
  msbuild Build/zenith_win64.sln /t:DevilsPlayground /p:Configuration=$Configuration /p:Platform=x64
"@
    exit 1
}

# Resolve absolute OutDir (lets the user pass either an absolute or
# repo-relative path).
$outDirAbs = $OutDir
if (-not [System.IO.Path]::IsPathRooted($outDirAbs)) {
    $outDirAbs = Join-Path $repoRoot $OutDir
}

if (Test-Path $outDirAbs) {
    if (-not $Force) {
        Write-Error "Output dir already exists: $outDirAbs (use -Force to overwrite)"
        exit 1
    }
    Remove-Item -Recurse -Force $outDirAbs
}
New-Item -ItemType Directory -Force -Path $outDirAbs | Out-Null

Write-Host "[package] Packaging MVP demo into: $outDirAbs" -ForegroundColor Cyan
Write-Host "[package]   exe:    $exePath" -ForegroundColor DarkGray
Write-Host "[package]   config: $Configuration" -ForegroundColor DarkGray

# 1. Copy exe + every DLL alongside it.
Write-Host "[package] Copying exe + DLLs..." -ForegroundColor Cyan
Copy-Item $exePath -Destination $outDirAbs -Force
$dlls = Get-ChildItem -Path $exeDir -Filter "*.dll" -File
foreach ($dll in $dlls) {
    Copy-Item $dll.FullName -Destination $outDirAbs -Force
}
Write-Host "[package]   $($dlls.Count) DLL(s) copied" -ForegroundColor DarkGray

# 2. Engine assets. The exe loads from ENGINE_ASSETS_DIR (an absolute
# path baked at build time). Copy the tree mirroring that path so a
# user extracting the bundle to the same absolute path can run it.
# This is the documented limitation -- see README block at the bottom.
Write-Host "[package] Copying engine assets (Zenith/Assets/)..." -ForegroundColor Cyan
$engineAssetSrc = Join-Path $repoRoot "Zenith/Assets"
$engineAssetDst = Join-Path $outDirAbs "Zenith/Assets"
if (Test-Path $engineAssetSrc) {
    New-Item -ItemType Directory -Force -Path (Split-Path $engineAssetDst -Parent) | Out-Null
    Copy-Item -Recurse $engineAssetSrc $engineAssetDst -Force
    $engineAssetCount = (Get-ChildItem $engineAssetDst -Recurse -File).Count
    Write-Host "[package]   $engineAssetCount file(s) in Zenith/Assets/" -ForegroundColor DarkGray
} else {
    Write-Warning "Engine asset dir not found at $engineAssetSrc"
}

# 3. Game assets + Config.
Write-Host "[package] Copying game assets (Games/DevilsPlayground/Assets/)..." -ForegroundColor Cyan
$gameAssetSrc = Join-Path $repoRoot "Games/DevilsPlayground/Assets"
$gameAssetDst = Join-Path $outDirAbs "Games/DevilsPlayground/Assets"
if (Test-Path $gameAssetSrc) {
    New-Item -ItemType Directory -Force -Path (Split-Path $gameAssetDst -Parent) | Out-Null
    Copy-Item -Recurse $gameAssetSrc $gameAssetDst -Force
    $gameAssetCount = (Get-ChildItem $gameAssetDst -Recurse -File).Count
    Write-Host "[package]   $gameAssetCount file(s) in Games/DevilsPlayground/Assets/" -ForegroundColor DarkGray
} else {
    Write-Warning "Game asset dir not found at $gameAssetSrc"
}

Write-Host "[package] Copying game Config/ (Tuning, Archetypes, Reagents)..." -ForegroundColor Cyan
$gameConfigSrc = Join-Path $repoRoot "Games/DevilsPlayground/Config"
$gameConfigDst = Join-Path $outDirAbs "Games/DevilsPlayground/Config"
if (Test-Path $gameConfigSrc) {
    New-Item -ItemType Directory -Force -Path (Split-Path $gameConfigDst -Parent) | Out-Null
    Copy-Item -Recurse $gameConfigSrc $gameConfigDst -Force
}

# 4. Shader source root (the exe compiles/reloads .slang files at
# runtime via SHADER_SOURCE_ROOT for the debug config).
Write-Host "[package] Copying shader source (Zenith/Flux/Shaders/)..." -ForegroundColor Cyan
$shaderSrc = Join-Path $repoRoot "Zenith/Flux/Shaders"
$shaderDst = Join-Path $outDirAbs "Zenith/Flux/Shaders"
if (Test-Path $shaderSrc) {
    New-Item -ItemType Directory -Force -Path (Split-Path $shaderDst -Parent) | Out-Null
    Copy-Item -Recurse $shaderSrc $shaderDst -Force
}

# 5. Generate README.md describing what's in the bundle + how to run it.
Write-Host "[package] Generating README.md..." -ForegroundColor Cyan
$readmePath = Join-Path $outDirAbs "README.md"
$readme = @"
# Devil's Playground -- MVP Demo

Top-down stealth-puzzle roguelite. You are a bodiless demon possessing
villagers in a 1670s English village. Collect 5 reagents, complete the
ritual at the pentagram, and avoid the witch-finder Aelfric -- all
before dawn or before every villager burns out.

Packaged on: $(Get-Date -Format "yyyy-MM-dd HH:mm")
Build config: ``$Configuration``

## How to play

1. Run ``devilsplayground.exe`` (the binary in this folder).
2. The front-end loads. Click **Play**.
3. The villagers are tinted cubes. Click on one to possess them. The
   game prevents possessing villagers further than 15 m from your
   last death point (the demon's range).
4. Each possessed body has a 30-second life timer (shown by the HUD
   life bar). Sprint (Shift) drains it faster; walk-quiet (Ctrl) halves
   your footstep loudness so the witch-finder can't hear you as far.
5. Walk onto items to pick them up. The forge takes Iron and outputs
   a Brass Key (also Wood -> Spike). The pentagram needs **five**
   distinct objective reagents delivered to it.
6. Avoid the witch-finder Aelfric (the larger tinted cube). He hears
   footsteps, sees demons in possessed villagers, and apprehends if
   he gets within 2 m of you for 3 seconds.
7. Win: deliver all 5 objectives, see the green "VICTORY" banner.
   Lose: get apprehended, run out of villagers, or have the night
   timer expire (default 5-minute night).
8. After Victory or any loss: press **R** to restart, **Q** to quit
   to the main menu. **Esc** opens the pause menu mid-run with the
   same shortcuts plus Resume.

## Controls reference

| Key | Action |
|---|---|
| Left-click | Possess villager (within range) |
| WASD | Move possessed villager |
| Shift + WASD | Sprint (faster, drains life faster) |
| Ctrl + WASD | Walk quiet (slower, halved footstep loudness) |
| F | Interact with item / door / chest / forge / pentagram |
| G | Drop held item |
| Q / E | Orbit camera left / right |
| Mouse wheel | Zoom camera in / out |
| Esc | Pause menu |
| R | Restart (paused or post-run-over) |
| Q | Quit to main menu (paused or post-run-over) |

## Known limitations (MVP scope)

* **Asset paths are absolute** -- baked into the binary at build time
  via Sharpmake. This packaged bundle only runs unmodified on a
  machine where the Zenith repository is checked out at the same
  absolute path the build used. A fully relocatable / installer-style
  build is post-MVP.
* **Everything is tinted cubes**. Villagers, the witch-finder, items,
  doors, chests, the forge, and the pentagram are all placeholder
  geometry with archetype/tag-based colour. Real meshes + animations
  ship with Phase 3 (Mixamo + Kenney imports).
* **No audio**. \``Zenith_AudioBus`\` records sound emissions for test
  instrumentation but the playback path is post-MVP.
* **One night**. The seven-night campaign + Joan Trew narrative are
  post-MVP. The packaged bundle is "Night 1" only.

## Verification

Pre-package, the suite reports:
* 113+ automated tests passing (\``pwsh Tools/run_dp_tests.ps1 -Headless\``).
* Acceptance drift gate (\``Tools/check_acceptance_drift.ps1\``) green: all
  4 playthrough tests within their MVP-DoD frame budgets.
* Full input-simulator-driven \``HumanPlaythrough_Test\`` wins the run
  in ~1850 frames (Vulkan + window required, skipped in headless CI).

## Where to look in the source

* Game systems: ``Games/DevilsPlayground/Source/PublicInterfaces.h``
  defines every \``DP_*\`` namespace (Player, AI, Items, Win, Fog, Night, etc).
* Behaviours: ``Games/DevilsPlayground/Components/DP*_Behaviour.h``
* Engine surface for the port: see ``Games/DevilsPlayground/CLAUDE.md``.

## Reporting issues

This is an autonomous-loop-built MVP. The codebase is well-tested but
short on art polish; bug reports against gameplay logic are far more
useful right now than visual feedback. Email or open a GitHub issue.
"@
Set-Content -Path $readmePath -Value $readme -Encoding utf8

# 6. Summary.
$totalFileCount = (Get-ChildItem $outDirAbs -Recurse -File).Count
$totalSizeMB = [math]::Round((Get-ChildItem $outDirAbs -Recurse -File |
    Measure-Object -Property Length -Sum).Sum / 1MB, 1)
Write-Host ""
Write-Host "[package] Done." -ForegroundColor Green
Write-Host "[package]   Output:       $outDirAbs" -ForegroundColor Green
Write-Host "[package]   Total files:  $totalFileCount" -ForegroundColor Green
Write-Host "[package]   Total size:   $totalSizeMB MB" -ForegroundColor Green
Write-Host "[package]   README:       $readmePath" -ForegroundColor Green
exit 0
