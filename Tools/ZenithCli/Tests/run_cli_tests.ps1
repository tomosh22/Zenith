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
    Invoke-Test "help exits 0" { Assert ((@(Invoke-ZenithCli @('help')))[-1] -eq 0) "help -> 0" }
    Invoke-Test "no args exits 0 (usage banner)" { Assert ((@(Invoke-ZenithCli @()))[-1] -eq 0) "no args -> 0" }
    Invoke-Test "unknown command exits 1 (usage)" { Assert ((@(Invoke-ZenithCli @('bogus')))[-1] -eq 1) "bogus -> 1" }
    Invoke-Test "new without a name exits 1 (usage)" { Assert ((@(Invoke-ZenithCli @('new')))[-1] -eq 1) "new -> 1" }
    Invoke-Test "open without a name exits 1 (usage)" { Assert ((@(Invoke-ZenithCli @('open')))[-1] -eq 1) "open -> 1" }
    Invoke-Test "build without a target exits 1 (usage)" { Assert ((@(Invoke-ZenithCli @('build')))[-1] -eq 1) "build -> 1" }
    Invoke-Test "new with an invalid name exits 2 (validation)" { Assert ((@(Invoke-ZenithCli @('new', 'sokoban')))[-1] -eq 2) "new lowercase -> 2" }
    Invoke-Test "open a non-existent game exits 5 (not-found)" { Assert ((@(Invoke-ZenithCli @('open', 'NoSuchGameXYZ')))[-1] -eq 5) "open missing -> 5" }
    Invoke-Test "known flags parse (not rejected as unknown)" {
        # A valid name + a missing template => not-found (5). If --no-open/--template
        # were mis-parsed as unknown options, this would be usage (1) instead.
        Assert ((@(Invoke-ZenithCli @('new', 'TmpFlagCheckXyz', '--template', 'NoSuchTemplate', '--no-open')))[-1] -eq 5) "flags parse -> 5 not 1"
    }

    Write-Host "`n[3] MSBuild resolver" -ForegroundColor Cyan
    Invoke-Test "Get-ZenithMsbuild finds an msbuild" {
        $mb = Get-ZenithMsbuild
        Assert ($null -ne $mb -and (Test-Path $mb)) "msbuild resolved to a real path (got '$mb')"
    }
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "CLI tests: $script:Pass passed, $script:Fail failed" -ForegroundColor Cyan
if ($script:Fail -gt 0) { $script:Fails | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }; exit 1 }
exit 0
