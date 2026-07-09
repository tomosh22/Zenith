# test_scaffold.ps1 -- end-to-end scaffold smoke.
# =============================================================================
# Proves `zenith new` produces a game that builds (_True) and boots headless with
# the engine units baseline, and that scaffolding + teardown touch NOTHING outside
# Games/<Name>/ (git status is identical before and after).
#
# Usage:  pwsh ./Tools/test_scaffold.ps1 [-Name ScaffoldSmoke]
# Exit:   0 all green, 1 a check failed.
# =============================================================================

[CmdletBinding()]
param([string]$Name = 'ScaffoldSmoke')

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $repoRoot 'Tools/ZenithCli/ZenithCli.psm1') -Force
$gameDir = Join-Path $repoRoot "Games/$Name"
$zenith = Join-Path $repoRoot 'Tools/zenith.ps1'
$scratch = [System.IO.Path]::GetTempPath()

$script:Pass = 0
$script:Fail = 0
$script:Fails = New-Object System.Collections.Generic.List[string]
function Check([bool]$Cond, [string]$Msg) {
    if ($Cond) { $script:Pass++; Write-Host "  PASS  $Msg" -ForegroundColor Green }
    else { $script:Fail++; $script:Fails.Add($Msg); Write-Host "  FAIL  $Msg" -ForegroundColor Red }
}

$gitBefore = ((git -C $repoRoot status --porcelain) | Out-String)

try {
    if (Test-Path $gameDir) { throw "Games/$Name already exists; pick a fresh -Name or remove it first." }

    Write-Host "[scaffold] zenith new $Name --no-open" -ForegroundColor Cyan
    & $zenith new $Name --no-open
    Check ($LASTEXITCODE -eq 0) "zenith new exits 0"

    $sln = Join-Path $gameDir "$($Name.ToLowerInvariant())_win64.sln"
    Check (Test-Path $sln) "per-game solution generated"
    Check (Test-Path (Join-Path $gameDir "$Name.zproj")) "descriptor created"
    Check (Test-Path (Join-Path $gameDir "$Name.cpp")) ".cpp created (.in stripped)"
    Check (Test-Path (Join-Path $gameDir "Components/$($Name)_GameComponent.h")) "component header created (__GAME_NAME__ filename token)"
    Check ((Get-ChildItem $gameDir -Recurse -Filter '*.in' -File -ErrorAction SilentlyContinue).Count -eq 0) "no .in template files leaked into the game"

    $msbuild = Get-ZenithMsbuild
    Check ($null -ne $msbuild) "msbuild resolved"
    if ($msbuild -and (Test-Path $sln)) {
        Write-Host "[scaffold] building $Name (Vulkan_vs2022_Debug_Win64_True)..." -ForegroundColor Cyan
        & $msbuild $sln /t:$Name /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64 /m /nologo /v:minimal | Out-Host
        Check ($LASTEXITCODE -eq 0) "game builds _True"

        $exe = Join-Path $gameDir "Build/output/win64/vulkan_vs2022_debug_win64_true/$($Name.ToLowerInvariant()).exe"
        Check (Test-Path $exe) "exe produced"
        if (Test-Path $exe) {
            $dir = Split-Path $exe
            Get-ChildItem (Join-Path $repoRoot 'Middleware/slang/bin/*.dll') -ErrorAction SilentlyContinue | ForEach-Object {
                $d = Join-Path $dir $_.Name; if (-not (Test-Path $d)) { Copy-Item $_.FullName $d -Force }
            }
            # Boot with unit tests ENABLED (the smoke's assertion) via the canonical
            # unit gate, so the engine baseline lives in ONE place
            # (Tools/run_unit_gate.ps1 -Baseline default -- the same script
            # engine-gate.yml runs). run_unit_gate deliberately keeps tool exports
            # ON so the asset-export unit tests find their generated assets
            # (see its header; the old inline boot here used --skip-tool-exports,
            # which wedges on a from-scratch checkout with no generated assets).
            & pwsh -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repoRoot 'Tools/run_unit_gate.ps1') `
                -Exe $exe -LogPath (Join-Path $scratch "scaffold_boot_$Name.log")
            Check ($LASTEXITCODE -eq 0) "units-at-boot baseline met (run_unit_gate.ps1)"
        }
    }
}
catch {
    Check $false "unexpected error: $($_.Exception.Message)"
}
finally {
    Write-Host "[scaffold] teardown: remove Games/$Name + regen" -ForegroundColor Cyan
    if (Test-Path $gameDir) { Remove-Item -Recurse -Force $gameDir }
    & (Join-Path $repoRoot 'Build/regen.ps1') | Out-Null
    $gitAfter = ((git -C $repoRoot status --porcelain) | Out-String)
    Check ($gitBefore -eq $gitAfter) "git status identical after teardown (scaffold touched only Games/$Name)"
    if ($gitBefore -ne $gitAfter) {
        Write-Host "    --- git status diff (unexpected residue) ---" -ForegroundColor Yellow
        Compare-Object ($gitBefore -split "`n") ($gitAfter -split "`n") | ForEach-Object { Write-Host "    $($_.SideIndicator) $($_.InputObject)" -ForegroundColor Yellow }
    }
}

Write-Host ""
Write-Host "scaffold smoke: $script:Pass passed, $script:Fail failed" -ForegroundColor Cyan
if ($script:Fail -gt 0) { exit 1 }
exit 0
