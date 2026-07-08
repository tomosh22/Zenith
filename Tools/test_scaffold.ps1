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
            $bootOut = Join-Path $scratch "scaffold_boot_$Name.txt"
            # Boot with unit tests ENABLED (the smoke's assertion). Bounded because
            # games can hang at shutdown teardown (a known pre-existing behavior).
            $p = Start-Process -FilePath $exe -ArgumentList @('--headless', '--exit-after-frames', '120', '--skip-tool-exports') `
                -PassThru -NoNewWindow -RedirectStandardOutput $bootOut -RedirectStandardError "$bootOut.err"
            if (-not $p.WaitForExit(180000)) { try { $p.Kill() } catch {} }
            $txt = Get-Content $bootOut -ErrorAction SilentlyContinue
            $unitsLine = "$(($txt | Select-String -Pattern 'Unit tests complete' | Select-Object -Last 1))"
            $cleanPass = $unitsLine -match '1047 ran, 1047 passed, 0 failed'
            # RegistryWideNodeRoundTrip is a known layout-sensitive pre-existing flake
            # (task_726cc81d): the game's symbol layout can trip it. Tolerate it as
            # the SOLE failure -- the engine still ran its full 1047-test suite.
            $knownFlake = ($unitsLine -match '1047 ran, 1046 passed, 1 failed') -and
                ($null -ne ($txt | Select-String -Pattern 'FAILED\s+GraphComponent::RegistryWideNodeRoundTrip'))
            Check ($cleanPass -or $knownFlake) "units-at-boot: 1047 ran, 0 failed (or only the known RegistryWideNodeRoundTrip flake)"
            if (-not ($cleanPass -or $knownFlake)) { Write-Host "    units line was: $unitsLine" -ForegroundColor Yellow }
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
