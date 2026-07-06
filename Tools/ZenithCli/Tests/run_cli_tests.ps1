# run_cli_tests.ps1 -- dependency-free assert-runner for the Zenith CLI module.
# Covers template expansion (token substitution, .in stripping, __GAME_NAME__
# filename tokens, --no-android skip, hard-fail on stray tokens) and dispatcher
# exit-code behavior. Name/descriptor validation is covered by the buildsystem
# suite (single source of truth).
#
# Usage:  pwsh ./Tools/ZenithCli/Tests/run_cli_tests.ps1
# Exit:   0 all green, 1 a failure.

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
# $here = <root>/Tools/ZenithCli/Tests -> repo root is three levels up.
$repoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $here))
Import-Module (Join-Path $repoRoot 'Tools/ZenithCli/ZenithCli.psm1') -Force

$script:Pass = 0
$script:Fail = 0
$script:Fails = New-Object System.Collections.Generic.List[string]
function Invoke-Test([string]$Name, [scriptblock]$Body) {
    try { & $Body; $script:Pass++; Write-Host "  PASS  $Name" -ForegroundColor Green }
    catch { $script:Fail++; $script:Fails.Add("$Name -- $($_.Exception.Message)"); Write-Host "  FAIL  $Name -- $($_.Exception.Message)" -ForegroundColor Red }
}
function Assert([bool]$Cond, [string]$Msg) { if (-not $Cond) { throw $Msg } }

# Host-agnostic CLI invoker. PS 5.1 does NOT flatten an array passed
# positionally into a ValueFromRemainingArguments param (pwsh 7 does), so
# `Invoke-ZenithCli @('open','X')` arrives as ONE nested element under 5.1.
# Splatting behaves identically on both hosts (zenith.ps1 splats @args too).
function Invoke-CliCode([string[]]$CliArgs) {
    if ($null -eq $CliArgs) { $CliArgs = @() }
    return (@(Invoke-ZenithCli @CliArgs))[-1]
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("zenith_cli_tests_" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

function New-Template([string]$Tag) {
    $t = Join-Path $tmp "tpl_$Tag"
    New-Item -ItemType Directory -Force -Path $t | Out-Null
    return $t
}
function Put([string]$Root, [string]$RelPath, [string]$Content) {
    $p = Join-Path $Root $RelPath
    New-Item -ItemType Directory -Force -Path (Split-Path $p -Parent) | Out-Null
    [System.IO.File]::WriteAllText($p, $Content, $utf8)
}

try {
    Write-Host "`n[1] Template expansion" -ForegroundColor Cyan

    Invoke-Test "content + filename tokens substitute, .in stripped" {
        $t = New-Template 'ok'
        Put $t '__GAME_NAME__.cpp.in' 'name={{GAME_NAME}} lower={{GAME_NAME_LOWER}} upper={{GAME_NAME_UPPER}}'
        Put $t 'Components/__GAME_NAME___Foo.h.in' '// {{GAME_NAME}} header'
        Put $t 'template.json' '{ "name": "X" }'
        $dest = Join-Path $tmp 'dest_ok'
        Expand-ZenithTemplate -TemplateDir $t -DestDir $dest -GameName 'MyGame'
        Assert (Test-Path (Join-Path $dest 'MyGame.cpp')) "MyGame.cpp should exist"
        Assert (-not (Test-Path (Join-Path $dest 'MyGame.cpp.in'))) ".in should be stripped"
        Assert (Test-Path (Join-Path $dest 'Components/MyGame_Foo.h')) "token filename should expand"
        Assert (-not (Test-Path (Join-Path $dest 'template.json'))) "template.json should NOT be copied"
        $content = Get-Content (Join-Path $dest 'MyGame.cpp') -Raw
        Assert ($content -eq 'name=MyGame lower=mygame upper=MYGAME') "content tokens: got [$content]"
    }

    Invoke-Test "--no-android skips the Android subtree" {
        $t = New-Template 'android'
        Put $t '__GAME_NAME__.cpp.in' '{{GAME_NAME}}'
        Put $t 'Android/app/build.gradle.in' 'applicationId "com.zenith.{{GAME_NAME_LOWER}}"'
        $dest = Join-Path $tmp 'dest_noandroid'
        Expand-ZenithTemplate -TemplateDir $t -DestDir $dest -GameName 'MyGame' -NoAndroid
        Assert (Test-Path (Join-Path $dest 'MyGame.cpp')) "cpp present"
        Assert (-not (Test-Path (Join-Path $dest 'Android'))) "Android subtree should be skipped"
    }

    Invoke-Test "--no-android=false keeps the Android subtree" {
        $t = New-Template 'android2'
        Put $t 'Android/app/build.gradle.in' 'id "{{GAME_NAME_LOWER}}"'
        $dest = Join-Path $tmp 'dest_android'
        Expand-ZenithTemplate -TemplateDir $t -DestDir $dest -GameName 'MyGame'
        Assert (Test-Path (Join-Path $dest 'Android/app/build.gradle')) "Android tree should be present"
        $c = Get-Content (Join-Path $dest 'Android/app/build.gradle') -Raw
        Assert ($c -eq 'id "mygame"') "android content token: got [$c]"
    }

    Invoke-Test "unresolved {{token}} hard-fails expansion" {
        $t = New-Template 'bad'
        Put $t '__GAME_NAME__.cpp.in' 'value={{UNKNOWN_TOKEN}}'
        $dest = Join-Path $tmp 'dest_bad'
        $threw = $false
        try { Expand-ZenithTemplate -TemplateDir $t -DestDir $dest -GameName 'MyGame' } catch { $threw = $true }
        Assert $threw "expansion must throw on an unresolved token"
    }

    Write-Host "`n[2] Dispatcher exit codes" -ForegroundColor Cyan
    Invoke-Test "help exits 0" { Assert ((Invoke-CliCode @('help')) -eq 0) "help -> 0" }
    Invoke-Test "no args exits 0 (usage banner)" { Assert ((Invoke-CliCode @()) -eq 0) "no args -> 0" }
    Invoke-Test "unknown command exits 1 (usage)" { Assert ((Invoke-CliCode @('bogus')) -eq 1) "bogus -> 1" }
    Invoke-Test "new without a name exits 1 (usage)" { Assert ((Invoke-CliCode @('new')) -eq 1) "new -> 1" }
    Invoke-Test "open without a name exits 1 (usage)" { Assert ((Invoke-CliCode @('open')) -eq 1) "open -> 1" }
    Invoke-Test "build without a target exits 1 (usage)" { Assert ((Invoke-CliCode @('build')) -eq 1) "build -> 1" }
    Invoke-Test "new with an invalid name exits 2 (validation)" { Assert ((Invoke-CliCode @('new', 'sokoban')) -eq 2) "new lowercase -> 2" }
    Invoke-Test "open a non-existent game exits 5 (not-found)" { Assert ((Invoke-CliCode @('open', 'NoSuchGameXYZ')) -eq 5) "open missing -> 5" }
    Invoke-Test "known flags parse (not rejected as unknown)" {
        # A valid name + a missing template => not-found (5). If --no-open/--template
        # were mis-parsed as unknown options, this would be usage (1) instead.
        Assert ((Invoke-CliCode @('new', 'TmpFlagCheckXyz', '--template', 'NoSuchTemplate', '--no-open')) -eq 5) "flags parse -> 5 not 1"
    }

    Write-Host "`n[3] MSBuild resolver" -ForegroundColor Cyan
    Invoke-Test "Get-ZenithMsbuild finds an msbuild" {
        $mb = Get-ZenithMsbuild
        Assert ($null -ne $mb -and (Test-Path $mb)) "msbuild resolved to a real path (got '$mb')"
    }

    Write-Host "`n[4] clean / regen --check dispatcher exit codes" -ForegroundColor Cyan
    Invoke-Test "clean with unknown game exits 5 (not-found)" { Assert ((Invoke-CliCode @('clean', 'NoSuchGameXYZ')) -eq 5) "clean missing -> 5" }
    Invoke-Test "clean with unknown flag exits 1 (usage)" { Assert ((Invoke-CliCode @('clean', '--bogus')) -eq 1) "clean --bogus -> 1" }
    Invoke-Test "clean --processes-only --dry-run exits 0" { Assert ((Invoke-CliCode @('clean', '--processes-only', '--dry-run')) -eq 0) "clean dry-run -> 0" }
    Invoke-Test "clean <Game> --dry-run exits 0 and deletes nothing" {
        $out = Join-Path $repoRoot 'Games/Sokoban/Build/output'
        $existedBefore = Test-Path $out
        Assert ((Invoke-CliCode @('clean', 'Sokoban', '--dry-run')) -eq 0) "clean dry-run -> 0"
        Assert ($existedBefore -eq (Test-Path $out)) "dry-run must not delete output dirs"
    }
    Invoke-Test "regen with unknown flag exits 1 (usage)" { Assert ((Invoke-CliCode @('regen', '--bogus')) -eq 1) "regen --bogus -> 1" }
    Invoke-Test "regen --check on the real repo exits 0 or 3 (never usage/crash)" {
        # 0 when the tree is freshly regenerated; 3 when stale -- both are valid
        # states for a dev tree. Anything else is a bug.
        $rc = (Invoke-CliCode @('regen', '--check'))
        Assert ($rc -eq 0 -or $rc -eq 3) "regen --check -> 0|3 (got $rc)"
    }
    Invoke-Test "build --timeout parses (missing target still usage=1)" {
        Assert ((Invoke-CliCode @('build', '--timeout', '5')) -eq 1) "build --timeout no target -> 1"
    }
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "CLI tests: $script:Pass passed, $script:Fail failed" -ForegroundColor Cyan
if ($script:Fail -gt 0) { $script:Fails | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }; exit 1 }
exit 0
