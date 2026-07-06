# regen.ps1 -- canonical Zenith solution regeneration.
# =============================================================================
# One command to regenerate every solution from a consistent snapshot:
#   1. Refuse to run from a linked git worktree (Sharpmake-in-worktree hazard).
#   2. Validate all Games/<Name>/<Name>.zproj descriptors (fail with ALL errors).
#   3. Codegen Build/Sharpmake_GameInstances.generated.cs from the descriptors.
#   4. Build the /sources list from the Build/Sharpmake_*.cs glob (no stale
#      hand-maintained lists).
#   5. Run Sharpmake ONCE -> engine sln + all per-game slns in one snapshot.
#   6. Fix up AGDE vcxproj (c++2a + UBSan) -- previously skipped by run_sharpmake.ps1.
#   7. Delete the obsolete monolithic Build/zenith_win64.sln / zenith_agde.sln.
#   8. Print the generated .sln inventory (consumed by the CLI + hub).
#
# Usage:
#   pwsh ./Build/regen.ps1              # uses ..\Sharpmake\Sharpmake.Application.exe
#   pwsh ./Build/regen.ps1 -UseDotnet   # uses `dotnet exec ...Sharpmake.Application.dll` (CI)
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param(
    [switch]$UseDotnet
)

$ErrorActionPreference = 'Stop'
$buildDir = $PSScriptRoot
$repoRoot = Split-Path -Parent $buildDir
Import-Module (Join-Path $buildDir 'zenith_buildsystem.psm1') -Force

function Fail([string]$Message, [int]$Code) {
    Write-Host ""
    Write-Host "regen.ps1: $Message" -ForegroundColor Red
    exit $Code
}

# 1. Worktree refusal ---------------------------------------------------------
if (Test-ZenithInWorktree -RepoRoot $repoRoot) {
    Fail "refusing to run from a linked git worktree ('$repoRoot'). Sharpmake generates absolute paths that resolve against the wrong tree here -- run regen from the MAIN checkout." 2
}

# 2. Validate descriptors -----------------------------------------------------
Write-Host "[regen] Validating game descriptors..." -ForegroundColor Cyan
$scan = Get-ZenithGameDescriptors
if ($scan.Errors.Count -gt 0) {
    Write-Host "[regen] Descriptor validation FAILED:" -ForegroundColor Red
    $scan.Errors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    Fail "$($scan.Errors.Count) descriptor error(s); nothing regenerated." 3
}
Write-Host "[regen] $($scan.Descriptors.Count) descriptors OK: $((@($scan.Descriptors | ForEach-Object { $_.Name })) -join ', ')" -ForegroundColor Green

# 3. Codegen ------------------------------------------------------------------
$generatedPath = Join-Path $buildDir 'Sharpmake_GameInstances.generated.cs'
Write-Host "[regen] Generating $($generatedPath | Split-Path -Leaf)..." -ForegroundColor Cyan
Invoke-ZenithCodegen -Descriptors $scan.Descriptors -OutputPath $generatedPath | Out-Null

# 4. Build the /sources list from the glob ------------------------------------
$sourceFiles = Get-ChildItem -LiteralPath $buildDir -Filter 'Sharpmake_*.cs' -File | Sort-Object Name
if ($sourceFiles.Count -eq 0) { Fail "no Build/Sharpmake_*.cs files found." 3 }
$srcList = ($sourceFiles | ForEach-Object { "'$($_.Name)'" }) -join ', '
$sourcesArg = "/sources($srcList)"
Write-Host "[regen] Sharpmake sources ($($sourceFiles.Count)): $((@($sourceFiles | ForEach-Object { $_.Name })) -join ', ')" -ForegroundColor DarkGray

# 5. Run Sharpmake ------------------------------------------------------------
Push-Location $buildDir
try {
    if ($UseDotnet) {
        $dll = Join-Path $repoRoot 'Sharpmake\Sharpmake.Application.dll'
        if (-not (Test-Path $dll)) { Fail "Sharpmake.Application.dll not found at $dll (needed for -UseDotnet)." 3 }
        # cmd /c preserves the parenthesised /sources token verbatim (PowerShell
        # native-arg quoting mangles it).
        cmd /c "dotnet exec `"$dll`" $sourcesArg"
    }
    else {
        $exe = Join-Path $repoRoot 'Sharpmake\Sharpmake.Application.exe'
        if (-not (Test-Path $exe)) { Fail "Sharpmake.Application.exe not found at $exe." 3 }
        cmd /c "`"$exe`" $sourcesArg"
    }
    $sharpmakeExit = $LASTEXITCODE
}
finally {
    Pop-Location
}
if ($sharpmakeExit -ne 0) { Fail "Sharpmake exited $sharpmakeExit." 3 }

# 6. AGDE vcxproj fixup -------------------------------------------------------
$fixAgde = Join-Path $buildDir 'fix_agde_vcxproj.ps1'
if (Test-Path $fixAgde) {
    Write-Host "[regen] Fixing up AGDE vcxproj (c++2a + UBSan)..." -ForegroundColor Cyan
    & powershell -ExecutionPolicy Bypass -File $fixAgde
    if ($LASTEXITCODE -ne 0) { Fail "fix_agde_vcxproj.ps1 exited $LASTEXITCODE." 3 }
}

# 7. Delete the obsolete monolithic solutions ---------------------------------
foreach ($stale in @('zenith_win64.sln', 'zenith_agde.sln')) {
    $p = Join-Path $buildDir $stale
    if (Test-Path $p) {
        Remove-Item -Force $p
        Write-Host "[regen] Removed obsolete $stale (superseded by per-game + engine slns)." -ForegroundColor Yellow
    }
}

# 7.5. Prune orphaned generated artifacts (deleted games / dropped android) ----
Remove-ZenithOrphanGameArtifacts -RepoRoot $repoRoot -Descriptors $scan.Descriptors | Out-Null

# 8. Print the generated .sln inventory ---------------------------------------
Write-Host ""
Write-Host "[regen] Generated solutions:" -ForegroundColor Green
$engineSln = Get-ChildItem -LiteralPath $buildDir -Filter '*.sln' -File -ErrorAction SilentlyContinue
foreach ($s in $engineSln) { Write-Host "  $($s.FullName)" }
$gameSlns = Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Games') -Filter '*.sln' -File -Recurse -Depth 1 -ErrorAction SilentlyContinue
foreach ($s in ($gameSlns | Sort-Object FullName)) { Write-Host "  $($s.FullName)" }
Write-Host ""
Write-Host "[regen] Done." -ForegroundColor Green
exit 0
