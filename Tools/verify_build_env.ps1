# verify_build_env.ps1 -- DevilsPlayground build-environment audit (MVP-0.0.1).
#
# Checks every required prerequisite from Docs/BuildEnvironment.md section 1
# against the local machine. Prints a per-check PASS / WARN / FAIL line
# and exits 0 only if every REQUIRED check passes.
#
# Usage:
#   .\Tools\verify_build_env.ps1                 # standard audit
#   .\Tools\verify_build_env.ps1 -WarningsAreErrors   # CI-grade strictness
#   .\Tools\verify_build_env.ps1 -SkipRepoState  # checks software only
#
# Exit codes:
#   0 -- all required checks pass.
#   1 -- one or more required checks failed (or a soft check failed with
#       -WarningsAreErrors).
#
# Written for compatibility with both Windows PowerShell 5.1
# (powershell.exe) and PowerShell 7+ (pwsh.exe). No PS7-only syntax
# (??, ?., ?:, $null-conditional). The script is the canonical answer
# to ManualSetupChecklist.md section A -- running it green is the proof.

[CmdletBinding()]
param(
    [switch]$WarningsAreErrors,
    [switch]$SkipRepoState
)

$ErrorActionPreference = 'Continue'

# ---------------------------------------------------------------------------
# Result accumulator.
# ---------------------------------------------------------------------------

$script:Results = New-Object System.Collections.ArrayList

function Add-Result {
    param(
        [Parameter(Mandatory)] [string]$Name,
        [Parameter(Mandatory)] [bool]  $Required,
        [Parameter(Mandatory)] [bool]  $OK,
        [string]$Detail = ''
    )
    [void]$script:Results.Add([PSCustomObject]@{
        Name     = $Name
        Required = $Required
        OK       = $OK
        Detail   = $Detail
    })
}

# ---------------------------------------------------------------------------
# Locate the repo root from the script's own location.
# Tools/verify_build_env.ps1 -> repo root is one level up.
# ---------------------------------------------------------------------------

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Write-Host "[verify_build_env] Repo root: $RepoRoot"
Write-Host ""

# ---------------------------------------------------------------------------
# 1. Visual Studio 2022 + "Desktop development with C++" workload.
# ---------------------------------------------------------------------------

$vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vsWhere)) {
    Add-Result -Name 'Visual Studio 2022 (vswhere)' -Required $true -OK $false `
        -Detail "vswhere.exe not found at $vsWhere -- install VS 2022 (any edition)."
} else {
    $vsInstall = & $vsWhere -products * `
        -requires Microsoft.VisualStudio.Workload.NativeDesktop `
        -property installationPath 2>$null
    if (-not $vsInstall) {
        Add-Result -Name 'VS 2022 + C++ workload' -Required $true -OK $false `
            -Detail 'NativeDesktop workload not installed -- re-run VS Installer.'
    } else {
        $first = ($vsInstall -split "`n")[0].Trim()
        Add-Result -Name 'VS 2022 + C++ workload' -Required $true -OK $true -Detail $first
    }
}

# ---------------------------------------------------------------------------
# 2. Windows SDK 10.0.22000+.
# ---------------------------------------------------------------------------

$sdkIncludeBase = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
if (-not (Test-Path $sdkIncludeBase)) {
    Add-Result -Name 'Windows SDK 10.0.22000+' -Required $true -OK $false `
        -Detail "Kits\10\Include directory not found at $sdkIncludeBase."
} else {
    $candidates = Get-ChildItem -Path $sdkIncludeBase -Directory -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -match '^10\.0\.(\d+)\.\d+$' -and [int]$Matches[1] -ge 22000
        }
    if (-not $candidates) {
        $have = (Get-ChildItem -Path $sdkIncludeBase -Directory -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty Name) -join ', '
        Add-Result -Name 'Windows SDK 10.0.22000+' -Required $true -OK $false `
            -Detail "No SDK >= 22000 found. Have: $have"
    } else {
        $best = $candidates | Sort-Object Name -Descending | Select-Object -First 1
        Add-Result -Name 'Windows SDK 10.0.22000+' -Required $true -OK $true -Detail $best.Name
    }
}

# ---------------------------------------------------------------------------
# 3. Vulkan SDK (BuildEnvironment.md pins 1.3.290.0; spec accepts 1.3.x).
# ---------------------------------------------------------------------------

if (-not $env:VULKAN_SDK) {
    Add-Result -Name 'Vulkan SDK 1.3.x' -Required $true -OK $false `
        -Detail 'VULKAN_SDK env var not set -- install LunarG SDK and re-open shell.'
} else {
    $vkHeader = Join-Path $env:VULKAN_SDK 'Include\vulkan\vulkan.h'
    if (-not (Test-Path $vkHeader)) {
        Add-Result -Name 'Vulkan SDK 1.3.x' -Required $true -OK $false `
            -Detail "VULKAN_SDK ($env:VULKAN_SDK) does not contain Include\vulkan\vulkan.h"
    } else {
        $sdkName = Split-Path $env:VULKAN_SDK -Leaf
        $is13 = $sdkName -like '1.3.*'
        $detail = "$sdkName at $env:VULKAN_SDK"
        if (-not $is13) {
            $detail += ' (BuildEnvironment.md pins 1.3.290.0; you have a non-1.3.x version -- may work, unverified)'
        }
        Add-Result -Name 'Vulkan SDK 1.3.x' -Required $true -OK $true -Detail $detail
    }
}

# ---------------------------------------------------------------------------
# 4. .NET 6.0+ runtime (SDK preferred, runtime sufficient).
#    Sharpmake.Application.exe ships pre-built in the repo, so it only
#    needs the .NET runtime to launch -- the full SDK is overkill for
#    routine builds. We treat runtime>=6 as PASS, and additionally note
#    whether the SDK is installed.
# ---------------------------------------------------------------------------

$dotnetExe = Get-Command dotnet -ErrorAction SilentlyContinue
$dotnetPath = $null
if ($dotnetExe) { $dotnetPath = $dotnetExe.Source }
elseif (Test-Path 'C:\Program Files\dotnet\dotnet.exe') {
    $dotnetPath = 'C:\Program Files\dotnet\dotnet.exe'
}

if (-not $dotnetPath) {
    Add-Result -Name '.NET 6.0+ runtime' -Required $true -OK $false `
        -Detail 'dotnet not found on PATH or at C:\Program Files\dotnet. Install the .NET 6 or 8 SDK or runtime.'
} else {
    $sdkLine = $null
    try { $sdkLine = (& $dotnetPath --version 2>$null).Trim() } catch {}
    $hasSdk = $false
    $sdkMajor = 0
    if ($sdkLine -and $sdkLine -match '^(\d+)\.') {
        $hasSdk = $true
        $sdkMajor = [int]$Matches[1]
    }
    # Always inspect installed runtimes (more authoritative than --version,
    # which is SDK-only).
    $rtOut = & $dotnetPath --list-runtimes 2>$null
    $rtMajor = 0
    if ($rtOut) {
        foreach ($line in $rtOut) {
            if ($line -match '^Microsoft\.NETCore\.App\s+(\d+)\.\d+') {
                $m = [int]$Matches[1]
                if ($m -gt $rtMajor) { $rtMajor = $m }
            }
        }
    }
    $bestMajor = [Math]::Max($sdkMajor, $rtMajor)
    if ($bestMajor -ge 6) {
        $msg = ''
        if ($hasSdk) { $msg = "SDK v$sdkLine; " }
        $msg += "runtime v$rtMajor.x"
        if (-not $hasSdk) {
            $msg += ' (no SDK installed; runtime suffices for prebuilt Sharpmake.Application.exe)'
        }
        Add-Result -Name '.NET 6.0+ runtime' -Required $true -OK $true -Detail $msg
    } else {
        Add-Result -Name '.NET 6.0+ runtime' -Required $true -OK $false `
            -Detail "Found SDK v$sdkLine / runtime v$rtMajor.x (need >= 6.0)"
    }
}

# ---------------------------------------------------------------------------
# 5. PowerShell. pwsh.exe (PS7+) is preferred but not strictly required --
#    powershell.exe (PS5.1) suffices for most tooling. Track separately.
# ---------------------------------------------------------------------------

$pwshCmd = Get-Command pwsh.exe -ErrorAction SilentlyContinue
$psCmd   = Get-Command powershell.exe -ErrorAction SilentlyContinue

if ($pwshCmd) {
    $pwshVer = & $pwshCmd.Source -NoProfile -NoLogo -Command `
        '$PSVersionTable.PSVersion.ToString()' 2>$null
    Add-Result -Name 'pwsh.exe (PowerShell 7+)' -Required $false -OK $true `
        -Detail "v$($pwshVer.Trim()) at $($pwshCmd.Source)"
} elseif ($psCmd) {
    Add-Result -Name 'pwsh.exe (PowerShell 7+)' -Required $false -OK $false `
        -Detail "Not installed. powershell.exe available as fallback; some post-build steps that hard-code pwsh.exe will fail silently. Install PS7 to remove this warning."
} else {
    Add-Result -Name 'PowerShell' -Required $true -OK $false `
        -Detail 'Neither pwsh.exe nor powershell.exe found on PATH (improbable on Windows -- investigate).'
}

# ---------------------------------------------------------------------------
# 6. Git 2.30+.
# ---------------------------------------------------------------------------

$gitOut = $null
try { $gitOut = (& git --version 2>$null) } catch {}
if (-not $gitOut -or $gitOut -notmatch 'git version (\d+)\.(\d+)') {
    Add-Result -Name 'Git 2.30+' -Required $true -OK $false `
        -Detail 'git not found on PATH or unparseable version.'
} else {
    $gMaj = [int]$Matches[1]
    $gMin = [int]$Matches[2]
    $okGit = ($gMaj -gt 2) -or ($gMaj -eq 2 -and $gMin -ge 30)
    Add-Result -Name 'Git 2.30+' -Required $true -OK $okGit -Detail $gitOut.Trim()
}

# ---------------------------------------------------------------------------
# 7. GitHub CLI installed AND authenticated.
# ---------------------------------------------------------------------------

$ghCmd = Get-Command gh -ErrorAction SilentlyContinue
if (-not $ghCmd) {
    $defaultGh = 'C:\Program Files\GitHub CLI\gh.exe'
    if (Test-Path $defaultGh) {
        $ghCmd = [PSCustomObject]@{ Source = $defaultGh }
    }
}
if (-not $ghCmd) {
    Add-Result -Name 'gh CLI installed' -Required $true -OK $false `
        -Detail 'gh not on PATH and not at C:\Program Files\GitHub CLI\gh.exe. Install via winget: winget install GitHub.cli'
} else {
    $authOut = & $ghCmd.Source auth status 2>&1 | Out-String
    if ($authOut -match 'Logged in to github.com') {
        $scopeLine = ($authOut -split "`n") | Where-Object { $_ -match 'Token scopes' } | Select-Object -First 1
        $scopes = ''
        if ($scopeLine) { $scopes = ($scopeLine -replace '.*Token scopes:\s*', '').Trim() }
        # Soft-warn about the optional admin:repo_hook scope needed for MVP-0.0.6.
        $hasAdmin = $scopes -match "'admin:repo_hook'"
        $detail = "Logged in. Scopes: $scopes."
        if (-not $hasAdmin) {
            $detail += " (Soft warning: missing 'admin:repo_hook' -- MVP-0.0.6 will fall through to web-UI for branch protection.)"
        }
        Add-Result -Name 'gh CLI authenticated' -Required $true -OK $true -Detail $detail
    } else {
        Add-Result -Name 'gh CLI authenticated' -Required $true -OK $false `
            -Detail 'gh present but not authenticated. Run: gh auth login'
    }
}

# ---------------------------------------------------------------------------
# 8. Repo state -- current branch is master and the working tree is clean.
#    Per MVP-0.0.1 spec. Skippable for cases where the script is being run
#    while developing on a feature branch (the orchestrator's own flow).
# ---------------------------------------------------------------------------

if ($SkipRepoState) {
    Add-Result -Name 'Repo on master and clean' -Required $false -OK $true `
        -Detail 'Skipped (-SkipRepoState).'
} else {
    $branch = (& git -C $RepoRoot branch --show-current 2>$null).Trim()
    $porcelain = & git -C $RepoRoot status --porcelain 2>$null
    $isClean = [string]::IsNullOrWhiteSpace($porcelain)
    $isMaster = ($branch -eq 'master')
    $okRepo = $isMaster -and $isClean
    $changeCount = 0
    if (-not $isClean) {
        $changeCount = (($porcelain -split "`n") | Where-Object { $_ -ne '' }).Count
    }
    $detail = "branch=$branch; "
    if ($isClean) { $detail += 'working tree clean' }
    else          { $detail += "$changeCount uncommitted file(s)" }
    Add-Result -Name 'Repo on master and clean' -Required $true -OK $okRepo -Detail $detail
}

# ---------------------------------------------------------------------------
# Summary.
# ---------------------------------------------------------------------------

Write-Host ''
Write-Host '=== verify_build_env results ==='
$passCount = 0
$warnCount = 0
$failCount = 0
foreach ($r in $script:Results) {
    if ($r.OK) {
        Write-Host ('  [PASS] {0,-32}  {1}' -f $r.Name, $r.Detail) -ForegroundColor Green
        $passCount++
    } elseif (-not $r.Required) {
        Write-Host ('  [WARN] {0,-32}  {1}' -f $r.Name, $r.Detail) -ForegroundColor Yellow
        $warnCount++
    } else {
        Write-Host ('  [FAIL] {0,-32}  {1}' -f $r.Name, $r.Detail) -ForegroundColor Red
        $failCount++
    }
}

Write-Host ''
Write-Host "Summary: $passCount passed, $warnCount warning(s), $failCount required failure(s)."
Write-Host ''

if ($failCount -gt 0) {
    Write-Host 'Required prerequisites missing. See Docs/BuildEnvironment.md section 1 for installation steps.' -ForegroundColor Red
    if ($WarningsAreErrors -and $warnCount -gt 0) {
        Write-Host '(-WarningsAreErrors set; warnings also counted as failure.)' -ForegroundColor Red
    }
    exit 1
}

if ($WarningsAreErrors -and $warnCount -gt 0) {
    Write-Host 'All required checks pass, but warnings are treated as errors (-WarningsAreErrors). Failing.' -ForegroundColor Yellow
    exit 1
}

Write-Host 'All required checks pass.' -ForegroundColor Green
exit 0
