# ZenithCli.psm1
# =============================================================================
# The `zenith` CLI: create / open / build / run / list games with zero
# build-script edits. Pure functions -- the dispatcher (Tools/zenith.ps1) calls
# Invoke-ZenithCli. Name validation and descriptor logic are NOT reimplemented
# here; they come from Build/zenith_buildsystem.psm1 (single source of truth).
#
# Exit codes (returned as [int] from Invoke-ZenithCli):
#   0 ok | 1 usage | 2 validation | 3 generation | 4 build | 5 not-found
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:EXIT_OK = 0
$script:EXIT_USAGE = 1
$script:EXIT_VALIDATION = 2
$script:EXIT_GENERATION = 3
$script:EXIT_BUILD = 4
$script:EXIT_NOTFOUND = 5

$script:DefaultConfig = 'Vulkan_vs2022_Debug_Win64_True'

# --- Paths / imports ---------------------------------------------------------

function Get-CliRepoRoot {
    # Tools/ZenithCli/ZenithCli.psm1 -> repo root is two levels up.
    return (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
}

function Import-BuildSystem {
    $mod = Join-Path (Get-CliRepoRoot) 'Build/zenith_buildsystem.psm1'
    Import-Module $mod -Force -Global
}

# --- MSBuild resolver --------------------------------------------------------

function Get-ZenithMsbuild {
    # 1. msbuild on PATH (CI uses microsoft/setup-msbuild).
    $cmd = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    # 2. vswhere (handles Insiders / prerelease installs).
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null |
            Select-Object -First 1
        if ($found -and (Test-Path $found)) { return $found }
    }
    return $null
}

# --- Small helpers -----------------------------------------------------------

function Write-CliError { param([string]$Message) Write-Host "zenith: $Message" -ForegroundColor Red }
function Write-CliInfo { param([string]$Message) Write-Host $Message -ForegroundColor Cyan }

function Get-GameDir { param([string]$Name) return (Join-Path (Get-CliRepoRoot) "Games/$Name") }
function Get-GameWin64Sln { param([string]$Name) return (Join-Path (Get-GameDir $Name) "$($Name.ToLowerInvariant())_win64.sln") }

function Resolve-ExistingGameName {
    # Case-insensitive lookup of an existing Games/<Name> dir; returns the real
    # folder name or $null.
    param([string]$Name)
    $gamesRoot = Join-Path (Get-CliRepoRoot) 'Games'
    if (-not (Test-Path $gamesRoot)) { return $null }
    foreach ($d in (Get-ChildItem -LiteralPath $gamesRoot -Directory)) {
        if ($d.Name -ieq $Name) { return $d.Name }
    }
    return $null
}

function Get-GameOutputExe {
    # Newest built exe for a game across win64 output dirs, or a specific config.
    param([string]$Name, [string]$Config)
    $outRoot = Join-Path (Get-GameDir $Name) 'Build/output/win64'
    if (-not (Test-Path $outRoot)) { return $null }
    $exeName = "$($Name.ToLowerInvariant()).exe"
    if ($Config) {
        $p = Join-Path $outRoot "$($Config.ToLowerInvariant())/$exeName"
        if (Test-Path $p) { return (Get-Item $p) }
        return $null
    }
    $exes = Get-ChildItem -LiteralPath $outRoot -Filter $exeName -File -Recurse -ErrorAction SilentlyContinue
    if (-not $exes) { return $null }
    return ($exes | Sort-Object LastWriteTime -Descending | Select-Object -First 1)
}

function Invoke-Regen {
    # Run Build/regen.ps1; returns its exit code. Out-Host keeps regen's console
    # output (including Sharpmake's native stdout) off this function's pipeline so
    # the caller's `$rc = Invoke-Regen` gets a clean integer, not the build log.
    $regen = Join-Path (Get-CliRepoRoot) 'Build/regen.ps1'
    & $regen 2>&1 | Out-Host
    return $LASTEXITCODE
}

# --- Template expansion ------------------------------------------------------

function Expand-ZenithTemplate {
    param(
        [string]$TemplateDir,
        [string]$DestDir,
        [string]$GameName,
        [switch]$NoAndroid
    )
    $lower = $GameName.ToLowerInvariant()
    $upper = $GameName.ToUpperInvariant()
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)

    $files = Get-ChildItem -LiteralPath $TemplateDir -Recurse -File
    foreach ($f in $files) {
        $rel = $f.FullName.Substring($TemplateDir.Length).TrimStart('\', '/')
        $relFwd = $rel.Replace('\', '/')

        # template.json is hub metadata, not part of the generated game.
        if ($relFwd -ieq 'template.json') { continue }
        # Skip the Android subtree when --no-android.
        if ($NoAndroid -and $relFwd -like 'Android/*') { continue }

        # Filename token substitution + .in suffix stripping.
        $destRel = $rel.Replace('__GAME_NAME__', $GameName)
        if ($destRel.EndsWith('.in')) { $destRel = $destRel.Substring(0, $destRel.Length - 3) }
        $destPath = Join-Path $DestDir $destRel
        New-Item -ItemType Directory -Force -Path (Split-Path $destPath -Parent) | Out-Null

        # Content token substitution + hard-fail on any surviving token.
        $content = Get-Content -LiteralPath $f.FullName -Raw
        if ($null -eq $content) { $content = '' }
        $content = $content.Replace('{{GAME_NAME}}', $GameName).Replace('{{GAME_NAME_LOWER}}', $lower).Replace('{{GAME_NAME_UPPER}}', $upper)
        if ($content -match '\{\{') {
            throw "template expansion left an unresolved {{...}} token in $destRel"
        }
        [System.IO.File]::WriteAllText($destPath, $content, $utf8NoBom)
    }
}

# --- Commands ----------------------------------------------------------------

function Invoke-ZenithNew {
    param([string[]]$CmdArgs)
    Import-BuildSystem

    $name = $null; $template = 'NewGame'; $noAndroid = $false; $noOpen = $false
    for ($i = 0; $i -lt $CmdArgs.Count; $i++) {
        $a = $CmdArgs[$i]
        if ($a -eq '--template') { $template = $CmdArgs[++$i] }
        elseif ($a -eq '--no-android') { $noAndroid = $true }
        elseif ($a -eq '--no-open') { $noOpen = $true }
        elseif ($a -like '--*') { Write-CliError "unknown option '$a' for 'new'"; return $script:EXIT_USAGE }
        else { if ($null -eq $name) { $name = $a } else { Write-CliError "unexpected argument '$a'"; return $script:EXIT_USAGE } }
    }
    if ([string]::IsNullOrEmpty($name)) { Write-CliError "usage: zenith new <Name> [--template <T>] [--no-android] [--no-open]"; return $script:EXIT_USAGE }

    # Validate name (syntax + collision).
    $nameErrors = @(Test-ZenithNewGameName -Name $name)
    if ($nameErrors.Count -gt 0) {
        Write-CliError "invalid game name '$name':"
        $nameErrors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
        return $script:EXIT_VALIDATION
    }

    $templateDir = Join-Path (Get-CliRepoRoot) "Build/Templates/$template"
    if (-not (Test-Path $templateDir)) { Write-CliError "template '$template' not found at $templateDir"; return $script:EXIT_NOTFOUND }

    # If android requested but the template has no Android tree, quietly fall to
    # no-android (the descriptor's android flag is set from the template .zproj).
    $gameDir = Get-GameDir $name
    Write-CliInfo "[new] Scaffolding '$name' from template '$template'..."
    try {
        Expand-ZenithTemplate -TemplateDir $templateDir -DestDir $gameDir -GameName $name -NoAndroid:$noAndroid
    }
    catch {
        Write-CliError "template expansion failed: $($_.Exception.Message)"
        return $script:EXIT_GENERATION
    }

    # If --no-android, force the descriptor's android flag to false so validation
    # (android:true requires Android/) passes.
    if ($noAndroid) {
        $zproj = Join-Path $gameDir "$name.zproj"
        if (Test-Path $zproj) {
            $txt = Get-Content -LiteralPath $zproj -Raw
            $txt = $txt -replace '"android"\s*:\s*true', '"android": false'
            [System.IO.File]::WriteAllText($zproj, $txt, (New-Object System.Text.UTF8Encoding($false)))
        }
    }

    Write-CliInfo "[new] Regenerating solutions..."
    $rc = Invoke-Regen
    if ($rc -ne 0) { Write-CliError "regen failed (exit $rc)"; return $script:EXIT_GENERATION }

    $sln = Get-GameWin64Sln $name
    Write-Host ""
    Write-Host "Created game '$name'." -ForegroundColor Green
    Write-Host "  Solution: $sln"
    Write-Host ""
    Write-Host "  NOTE: first build+run must use a *_True config to bake Assets/Scenes/Main.zscen;" -ForegroundColor Yellow
    Write-Host "        thereafter _False / Android configs load the baked scene." -ForegroundColor Yellow

    if (-not $noOpen) {
        if (Test-Path $sln) { Start-Process $sln | Out-Null } else { Write-CliError "generated solution not found: $sln"; return $script:EXIT_GENERATION }
    }
    return $script:EXIT_OK
}

function Invoke-ZenithOpen {
    param([string[]]$CmdArgs)
    if ($CmdArgs.Count -lt 1) { Write-CliError "usage: zenith open <Name>"; return $script:EXIT_USAGE }
    $name = Resolve-ExistingGameName $CmdArgs[0]
    if (-not $name) { Write-CliError "no such game '$($CmdArgs[0])' (look in Games/)"; return $script:EXIT_NOTFOUND }

    Write-CliInfo "[open] Regenerating solutions (guarantees freshness)..."
    $rc = Invoke-Regen
    if ($rc -ne 0) { Write-CliError "regen failed (exit $rc)"; return $script:EXIT_GENERATION }

    $sln = Get-GameWin64Sln $name
    if (-not (Test-Path $sln)) { Write-CliError "solution not found: $sln"; return $script:EXIT_GENERATION }
    Start-Process $sln | Out-Null
    Write-Host "Opened $sln" -ForegroundColor Green
    return $script:EXIT_OK
}

function Invoke-ZenithList {
    param([string[]]$CmdArgs)
    Import-BuildSystem
    $asJson = ($CmdArgs -contains '--json')

    $scan = Get-ZenithGameDescriptors
    $rows = @()
    foreach ($d in ($scan.Descriptors | Sort-Object { $_.Name })) {
        $outRoot = Join-Path (Get-GameDir $d.Name) 'Build/output/win64'
        $builtConfigs = @()
        $newest = $null
        if (Test-Path $outRoot) {
            $exeName = "$($d.Name.ToLowerInvariant()).exe"
            foreach ($cfgDir in (Get-ChildItem -LiteralPath $outRoot -Directory -ErrorAction SilentlyContinue)) {
                $exe = Join-Path $cfgDir.FullName $exeName
                if (Test-Path $exe) {
                    $builtConfigs += $cfgDir.Name
                    $t = (Get-Item $exe).LastWriteTime
                    if ($null -eq $newest -or $t -gt $newest) { $newest = $t }
                }
            }
        }
        $rows += [PSCustomObject]@{
            Name         = $d.Name
            Android      = $d.Android
            BuiltConfigs = $builtConfigs
            NewestBuild  = if ($newest) { $newest.ToString('yyyy-MM-dd HH:mm') } else { '' }
        }
    }

    if ($asJson) {
        $rows | ConvertTo-Json -Depth 5
    }
    else {
        Write-Host ("{0,-18} {1,-8} {2,-11} {3}" -f 'Game', 'Android', 'Newest', 'Built configs') -ForegroundColor Cyan
        foreach ($r in $rows) {
            Write-Host ("{0,-18} {1,-8} {2,-11} {3}" -f $r.Name, $r.Android, $r.NewestBuild, ($r.BuiltConfigs -join ', '))
        }
        if ($scan.Errors.Count -gt 0) {
            Write-Host "`nDescriptor warnings:" -ForegroundColor Yellow
            $scan.Errors | ForEach-Object { Write-Host "  - $_" -ForegroundColor Yellow }
        }
    }
    return $script:EXIT_OK
}

function Invoke-ZenithRegenCmd {
    param([string[]]$CmdArgs)
    $rc = Invoke-Regen
    if ($rc -ne 0) { return $script:EXIT_GENERATION }
    return $script:EXIT_OK
}

function Invoke-ZenithBuild {
    param([string[]]$CmdArgs)
    Import-BuildSystem
    $target = $null; $config = $script:DefaultConfig
    for ($i = 0; $i -lt $CmdArgs.Count; $i++) {
        $a = $CmdArgs[$i]
        if ($a -eq '--config') { $config = $CmdArgs[++$i] }
        elseif ($a -like '--*') { Write-CliError "unknown option '$a'"; return $script:EXIT_USAGE }
        else { if ($null -eq $target) { $target = $a } }
    }
    if ([string]::IsNullOrEmpty($target)) { Write-CliError "usage: zenith build <Name|engine> [--config <C>]"; return $script:EXIT_USAGE }

    $msbuild = Get-ZenithMsbuild
    if (-not $msbuild) { Write-CliError "MSBuild not found (PATH or vswhere)."; return $script:EXIT_BUILD }

    if ($target -ieq 'engine') {
        # Engine build: Zenith + Sentinels, NEVER the whole sln (aux tools are
        # pre-existing-red in ToolsEnabled=True).
        $sln = Join-Path (Get-CliRepoRoot) 'Build/zenith_engine_win64.sln'
        $targets = @('Zenith', 'SentinelECS', 'SentinelPhysics', 'SentinelAI')
        foreach ($t in $targets) {
            Write-CliInfo "[build] $t ($config)..."
            & $msbuild $sln /t:$t /p:Configuration=$config /p:Platform=x64 /m /nologo /v:minimal | Out-Host
            if ($LASTEXITCODE -ne 0) { Write-CliError "engine target '$t' failed"; return $script:EXIT_BUILD }
        }
        return $script:EXIT_OK
    }

    $name = Resolve-ExistingGameName $target
    if (-not $name) { Write-CliError "no such game '$target'"; return $script:EXIT_NOTFOUND }
    $sln = Get-GameWin64Sln $name
    if (-not (Test-Path $sln)) { Write-CliError "solution not found: $sln (run 'zenith regen')"; return $script:EXIT_GENERATION }
    Write-CliInfo "[build] $name ($config)..."
    & $msbuild $sln /t:$name /p:Configuration=$config /p:Platform=x64 /m /nologo /v:minimal | Out-Host
    if ($LASTEXITCODE -ne 0) { Write-CliError "build failed"; return $script:EXIT_BUILD }
    Write-Host "Built $name ($config)." -ForegroundColor Green
    return $script:EXIT_OK
}

function Invoke-ZenithRun {
    param([string[]]$CmdArgs)
    $name = $null; $config = $null; $doBuild = $false; $passThrough = @()
    for ($i = 0; $i -lt $CmdArgs.Count; $i++) {
        $a = $CmdArgs[$i]
        if ($a -eq '--config') { $config = $CmdArgs[++$i] }
        elseif ($a -eq '--build') { $doBuild = $true }
        elseif ($a -eq '--') { if ($i -lt $CmdArgs.Count - 1) { $passThrough += $CmdArgs[($i + 1)..($CmdArgs.Count - 1)] }; break }
        else { if ($null -eq $name) { $name = $a } else { $passThrough += $a } }
    }
    if ([string]::IsNullOrEmpty($name)) { Write-CliError "usage: zenith run <Name> [--config <C>] [--build] [-- <game args>]"; return $script:EXIT_USAGE }
    $real = Resolve-ExistingGameName $name
    if (-not $real) { Write-CliError "no such game '$name'"; return $script:EXIT_NOTFOUND }
    $name = $real

    if ($doBuild) {
        $buildArgs = @($name)
        if ($config) { $buildArgs += @('--config', $config) }
        $rc = Invoke-ZenithBuild -CmdArgs $buildArgs
        if ($rc -ne 0) { return $rc }
    }

    $exe = Get-GameOutputExe -Name $name -Config $config
    if (-not $exe) { Write-CliError "no built exe for '$name'$(if ($config) { " (config $config)" } else { '' }) -- build first ('zenith build $name' or --build)"; return $script:EXIT_NOTFOUND }

    Write-CliInfo "[run] $($exe.FullName)"
    Push-Location (Split-Path $exe.FullName)
    try { & $exe.FullName @passThrough | Out-Host; $code = $LASTEXITCODE } finally { Pop-Location }
    return $code
}

function Invoke-ZenithHub {
    param([string[]]$CmdArgs)
    $rebuild = ($CmdArgs -contains '--rebuild')
    $hubExe = Join-Path (Get-CliRepoRoot) 'ZenithHub/output/win64/vulkan_vs2022_release_win64_false/zenithhub.exe'
    if ($rebuild -or -not (Test-Path $hubExe)) {
        $msbuild = Get-ZenithMsbuild
        if (-not $msbuild) { Write-CliError "MSBuild not found."; return $script:EXIT_BUILD }
        $sln = Join-Path (Get-CliRepoRoot) 'Build/zenith_engine_win64.sln'
        Write-CliInfo "[hub] Building ZenithHub (Vulkan_vs2022_Release_Win64_False)..."
        & $msbuild $sln /t:ZenithHub /p:Configuration=Vulkan_vs2022_Release_Win64_False /p:Platform=x64 /m /nologo /v:minimal | Out-Host
        if ($LASTEXITCODE -ne 0) { Write-CliError "hub build failed (is ZenithHub in the engine sln? Stage 4)"; return $script:EXIT_BUILD }
    }
    if (-not (Test-Path $hubExe)) { Write-CliError "hub exe not found: $hubExe"; return $script:EXIT_NOTFOUND }
    Start-Process $hubExe | Out-Null
    return $script:EXIT_OK
}

function Invoke-ZenithSelftest {
    param([string[]]$CmdArgs)
    $repoRoot = Get-CliRepoRoot
    $suites = @(
        (Join-Path $repoRoot 'Build/Tests/run_buildsystem_tests.ps1'),
        (Join-Path $repoRoot 'Tools/ZenithCli/Tests/run_cli_tests.ps1')
    )
    $anyFail = $false
    foreach ($s in $suites) {
        if (-not (Test-Path $s)) { Write-Host "  (skip, missing) $s" -ForegroundColor DarkGray; continue }
        Write-CliInfo "`n=== $s ==="
        & $s
        if ($LASTEXITCODE -ne 0) { $anyFail = $true }
    }
    if ($anyFail) { return 1 }
    return $script:EXIT_OK
}

# --- Dispatcher --------------------------------------------------------------

function Show-ZenithUsage {
    Write-Host @"
zenith -- Zenith game project CLI

Usage:
  zenith new <Name> [--template <T>] [--no-android] [--no-open]
  zenith open <Name>
  zenith list [--json]
  zenith regen
  zenith build <Name|engine> [--config <C>]
  zenith run <Name> [--config <C>] [--build] [-- <game args>]
  zenith hub [--rebuild]
  zenith selftest

Exit codes: 0 ok | 1 usage | 2 validation | 3 generation | 4 build | 5 not-found
"@
}

function Invoke-ZenithCli {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$CmdArgs)
    if ($null -eq $CmdArgs) { $CmdArgs = @() }
    if ($CmdArgs.Count -eq 0 -or $CmdArgs[0] -in @('-h', '--help', 'help')) { Show-ZenithUsage; return $script:EXIT_OK }

    $cmd = $CmdArgs[0]
    $rest = @()
    if ($CmdArgs.Count -gt 1) { $rest = $CmdArgs[1..($CmdArgs.Count - 1)] }

    switch -Regex ($cmd) {
        '^new$' { return (Invoke-ZenithNew -CmdArgs $rest) }
        '^open$' { return (Invoke-ZenithOpen -CmdArgs $rest) }
        '^list$' { return (Invoke-ZenithList -CmdArgs $rest) }
        '^regen$' { return (Invoke-ZenithRegenCmd -CmdArgs $rest) }
        '^build$' { return (Invoke-ZenithBuild -CmdArgs $rest) }
        '^run$' { return (Invoke-ZenithRun -CmdArgs $rest) }
        '^hub$' { return (Invoke-ZenithHub -CmdArgs $rest) }
        '^selftest$' { return (Invoke-ZenithSelftest -CmdArgs $rest) }
        default { Write-CliError "unknown command '$cmd'"; Show-ZenithUsage; return $script:EXIT_USAGE }
    }
}

Export-ModuleMember -Function @(
    'Invoke-ZenithCli', 'Get-ZenithMsbuild', 'Expand-ZenithTemplate',
    'Get-CliRepoRoot', 'Resolve-ExistingGameName'
)
