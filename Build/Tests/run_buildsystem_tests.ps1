# run_buildsystem_tests.ps1
# =============================================================================
# Dependency-free assert-runner for Build/zenith_buildsystem.psm1 (no Pester).
# Covers: name-syntax vectors (shared file), the descriptor validation matrix,
# the codegen golden file + SHA wiring, the worktree guard, and a sanity pass
# over the 12 real Games/*/*.zproj descriptors.
#
# Usage:  pwsh ./Build/Tests/run_buildsystem_tests.ps1
# Exit:   0 = all green, 1 = one or more failures.
#
# ASCII-only body; runs identically under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Split-Path -Parent $here
$repoRoot = Split-Path -Parent $buildDir
$modulePath = Join-Path $buildDir 'zenith_buildsystem.psm1'
$fixtureRoot = Join-Path $here 'Fixtures/GoodGames'
$goldenPath = Join-Path $here 'Fixtures/GoodGames.generated.cs.golden'

Import-Module $modulePath -Force

# --- Tiny assert framework ---------------------------------------------------

$script:Passed = 0
$script:Failed = 0
$script:Failures = New-Object System.Collections.Generic.List[string]

function Invoke-Test {
    param([string]$Name, [scriptblock]$Body)
    try {
        & $Body
        $script:Passed++
        Write-Host "  PASS  $Name" -ForegroundColor Green
    }
    catch {
        $script:Failed++
        $msg = "$Name -- $($_.Exception.Message)"
        $script:Failures.Add($msg)
        Write-Host "  FAIL  $msg" -ForegroundColor Red
    }
}

function Assert-True { param([bool]$Cond, [string]$Msg) if (-not $Cond) { throw "expected true: $Msg" } }
function Assert-False { param([bool]$Cond, [string]$Msg) if ($Cond) { throw "expected false: $Msg" } }
function Assert-Equal { param($Expected, $Actual, [string]$Msg) if ($Expected -ne $Actual) { throw "expected [$Expected] got [$Actual]: $Msg" } }

function Assert-AnyLike {
    param([object]$Items, [string]$Pattern, [string]$Msg)
    $arr = @($Items)
    $hit = $arr | Where-Object { $_ -like $Pattern }
    if (-not $hit) { throw "no item matched [$Pattern]: $Msg. Items: $($arr -join ' | ')" }
}

function Assert-NoneLike {
    param([object]$Items, [string]$Pattern, [string]$Msg)
    $arr = @($Items)
    $hit = $arr | Where-Object { $_ -like $Pattern }
    if ($hit) { throw "unexpected item matched [$Pattern]: $Msg. Items: $($arr -join ' | ')" }
}

# --- Temp fixture helpers ----------------------------------------------------

$script:TempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("zenith_bs_tests_" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $script:TempRoot | Out-Null
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function New-FixtureDescriptor {
    # Create <Root>/<Folder>/<FileBase>.zproj with raw $Json. Optionally add an
    # Android/ dir. Returns the descriptor path.
    param(
        [string]$Root, [string]$Folder, [string]$FileBase, [string]$Json,
        [switch]$WithAndroid
    )
    $dir = Join-Path $Root $Folder
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    if ($WithAndroid) { New-Item -ItemType Directory -Force -Path (Join-Path $dir 'Android') | Out-Null }
    $path = Join-Path $dir "$FileBase.zproj"
    [System.IO.File]::WriteAllText($path, $Json, $utf8NoBom)
    return $path
}

function New-IsolatedGamesRoot {
    # A fresh Games-root dir under the temp root, for scan-level tests.
    param([string]$Tag)
    $r = Join-Path $script:TempRoot ("root_" + $Tag + "_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Force -Path $r | Out-Null
    return $r
}

function Good-Json {
    param([string]$Name, [bool]$Android = $false)
    $a = if ($Android) { 'true' } else { 'false' }
    return "{ ""schemaVersion"": 1, ""name"": ""$Name"", ""android"": $a }"
}

try {
    # ========================================================================
    Write-Host "`n[1] Name-syntax vectors (shared file)" -ForegroundColor Cyan
    Invoke-Test "shared name_validation_cases.txt agrees with Test-ZenithGameNameSyntax" {
        $cases = @(Get-ZenithNameValidationCases)
        Assert-True ($cases.Count -ge 20) "expected >= 20 shared cases, got $($cases.Count)"
        $mismatches = @()
        foreach ($c in $cases) {
            $errs = @(Test-ZenithGameNameSyntax -Name $c.Name)
            $isValid = ($errs.Count -eq 0)
            $wantValid = ($c.Expected -eq 'valid')
            if ($isValid -ne $wantValid) {
                $mismatches += "name='$($c.Name)' expected=$($c.Expected) gotValid=$isValid"
            }
        }
        if ($mismatches.Count -gt 0) { throw ("vector mismatches: " + ($mismatches -join '; ')) }
    }

    Write-Host "`n[2] Name-syntax boundaries (PS-only)" -ForegroundColor Cyan
    Invoke-Test "64-char name is valid, 65-char is invalid" {
        $n64 = 'A' + ('a' * 63)
        $n65 = 'A' + ('a' * 64)
        Assert-Equal 0 (@(Test-ZenithGameNameSyntax -Name $n64).Count) "64-char should be valid"
        Assert-True ((@(Test-ZenithGameNameSyntax -Name $n65).Count) -gt 0) "65-char should be invalid"
    }
    Invoke-Test "non-ASCII letter is rejected" {
        $accented = 'Caf' + [char]0xE9   # 'Cafe' with an e-acute
        Assert-True ((@(Test-ZenithGameNameSyntax -Name $accented).Count) -gt 0) "non-ASCII should be invalid"
    }
    Invoke-Test "reserved prefixes and device names are rejected" {
        Assert-True ((@(Test-ZenithGameNameSyntax -Name 'ZenithGame').Count) -gt 0) "Zenith* reserved"
        Assert-True ((@(Test-ZenithGameNameSyntax -Name 'SentinelGame').Count) -gt 0) "Sentinel* reserved"
        Assert-True ((@(Test-ZenithGameNameSyntax -Name 'NUL').Count) -gt 0) "NUL device"
    }

    # ========================================================================
    Write-Host "`n[3] Descriptor validation matrix" -ForegroundColor Cyan

    Invoke-Test "valid descriptor -> Ok, no errors" {
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'ok') -Folder 'Widget' -FileBase 'Widget' -Json (Good-Json -Name 'Widget')
        $d = Read-ZenithDescriptor -Path $p
        Assert-True $d.Ok "should be Ok. errors: $($d.Errors -join '; ')"
        Assert-Equal 'Widget' $d.Name "name"
    }

    Invoke-Test "unknown key -> hard error" {
        $json = "{ ""schemaVersion"": 1, ""name"": ""Widget"", ""andriod"": true }"
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'unk') -Folder 'Widget' -FileBase 'Widget' -Json $json
        $d = Read-ZenithDescriptor -Path $p
        Assert-False $d.Ok "should be invalid"
        Assert-AnyLike $d.Errors "*unknown key 'andriod'*" "unknown-key error"
    }

    Invoke-Test "name != folder -> error" {
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'nf') -Folder 'Widget' -FileBase 'Widget' -Json (Good-Json -Name 'Gadget')
        $d = Read-ZenithDescriptor -Path $p
        Assert-AnyLike $d.Errors "*must equal its folder name 'Widget'*" "folder-mismatch error"
    }

    Invoke-Test "name != basename -> error" {
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'nb') -Folder 'Widget' -FileBase 'Other' -Json (Good-Json -Name 'Widget')
        $d = Read-ZenithDescriptor -Path $p
        Assert-AnyLike $d.Errors "*must equal its file basename 'Other'*" "basename-mismatch error"
    }

    Invoke-Test "schemaVersion > max -> error" {
        $json = "{ ""schemaVersion"": 2, ""name"": ""Widget"" }"
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'sv') -Folder 'Widget' -FileBase 'Widget' -Json $json
        $d = Read-ZenithDescriptor -Path $p
        Assert-AnyLike $d.Errors "*unsupported schemaVersion 2*" "schemaVersion error"
    }

    Invoke-Test "missing schemaVersion -> error" {
        $json = "{ ""name"": ""Widget"" }"
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'msv') -Folder 'Widget' -FileBase 'Widget' -Json $json
        $d = Read-ZenithDescriptor -Path $p
        Assert-AnyLike $d.Errors "*missing required key 'schemaVersion'*" "missing schemaVersion"
    }

    Invoke-Test "missing name -> error" {
        $json = "{ ""schemaVersion"": 1 }"
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'mn') -Folder 'Widget' -FileBase 'Widget' -Json $json
        $d = Read-ZenithDescriptor -Path $p
        Assert-AnyLike $d.Errors "*missing required key 'name'*" "missing name"
    }

    Invoke-Test "android:true without Android/ dir -> error" {
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'and') -Folder 'Widget' -FileBase 'Widget' -Json (Good-Json -Name 'Widget' -Android $true)
        $d = Read-ZenithDescriptor -Path $p
        Assert-AnyLike $d.Errors "*android:true but*does not exist*" "android-dir error"
    }

    Invoke-Test "android:true WITH Android/ dir -> ok" {
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'and2') -Folder 'Widget' -FileBase 'Widget' -Json (Good-Json -Name 'Widget' -Android $true) -WithAndroid
        $d = Read-ZenithDescriptor -Path $p
        Assert-True $d.Ok "should be Ok. errors: $($d.Errors -join '; ')"
        Assert-True $d.Android "android flag"
    }

    Invoke-Test "android not a bool -> error" {
        $json = "{ ""schemaVersion"": 1, ""name"": ""Widget"", ""android"": ""yes"" }"
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'ab') -Folder 'Widget' -FileBase 'Widget' -Json $json
        $d = Read-ZenithDescriptor -Path $p
        Assert-AnyLike $d.Errors "*android must be a boolean*" "android-type error"
    }

    Invoke-Test "extraDefines not array-of-strings -> error" {
        $json = "{ ""schemaVersion"": 1, ""name"": ""Widget"", ""extraDefines"": [1, 2] }"
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'ed') -Folder 'Widget' -FileBase 'Widget' -Json $json
        $d = Read-ZenithDescriptor -Path $p
        Assert-AnyLike $d.Errors "*extraDefines must be an array of strings*" "extraDefines-type error"
    }

    Invoke-Test "invalid JSON -> error" {
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'bad') -Folder 'Widget' -FileBase 'Widget' -Json "{ not json "
        $d = Read-ZenithDescriptor -Path $p
        Assert-AnyLike $d.Errors "*is not valid JSON*" "json-parse error"
    }

    Invoke-Test "non-object JSON -> error" {
        $p = New-FixtureDescriptor -Root (New-IsolatedGamesRoot 'nonobj') -Folder 'Widget' -FileBase 'Widget' -Json "5"
        $d = Read-ZenithDescriptor -Path $p
        Assert-AnyLike $d.Errors "*must be a JSON object*" "non-object error"
    }

    # ========================================================================
    Write-Host "`n[4] Scan-level validation" -ForegroundColor Cyan

    Invoke-Test "two descriptors in one folder -> error" {
        $root = New-IsolatedGamesRoot 'two'
        New-FixtureDescriptor -Root $root -Folder 'Widget' -FileBase 'Widget' -Json (Good-Json -Name 'Widget') | Out-Null
        New-FixtureDescriptor -Root $root -Folder 'Widget' -FileBase 'Extra' -Json (Good-Json -Name 'Widget') | Out-Null
        $scan = Get-ZenithGameDescriptors -GamesRoot $root
        Assert-AnyLike $scan.Errors "*has 2 .zproj descriptors*" "two-descriptor error"
    }

    Invoke-Test "duplicate name across folders -> error" {
        $root = New-IsolatedGamesRoot 'dup'
        New-FixtureDescriptor -Root $root -Folder 'Dup' -FileBase 'Dup' -Json (Good-Json -Name 'Dup') | Out-Null
        # Second folder lies about its name (also triggers folder-mismatch, but we
        # only assert the duplicate-name signal here).
        New-FixtureDescriptor -Root $root -Folder 'Dupe' -FileBase 'Dupe' -Json (Good-Json -Name 'Dup') | Out-Null
        $scan = Get-ZenithGameDescriptors -GamesRoot $root
        Assert-AnyLike $scan.Errors "*duplicate game name 'Dup'*" "duplicate-name error"
    }

    Invoke-Test "folder with no descriptor -> error" {
        $root = New-IsolatedGamesRoot 'empty'
        New-Item -ItemType Directory -Force -Path (Join-Path $root 'Empty') | Out-Null
        $scan = Get-ZenithGameDescriptors -GamesRoot $root
        Assert-AnyLike $scan.Errors "*has no .zproj descriptor*" "no-descriptor error"
    }

    # ========================================================================
    Write-Host "`n[5] Codegen golden + SHA wiring" -ForegroundColor Cyan

    Invoke-Test "codegen output (hash-redacted) matches committed golden" {
        Assert-True (Test-Path -LiteralPath $goldenPath) "golden file must exist: $goldenPath"
        $raw = Invoke-ZenithCodegen -GamesRoot $fixtureRoot -OutputPath ''
        $redacted = [System.Text.RegularExpressions.Regex]::Replace($raw, '[A-F0-9]{64}', '<HASH>')
        $golden = Get-Content -LiteralPath $goldenPath -Raw
        $normRedacted = $redacted -replace "`r`n", "`n" -replace "`r", "`n"
        $normGolden = $golden -replace "`r`n", "`n" -replace "`r", "`n"
        if ($normRedacted -ne $normGolden) {
            # Find first differing line for a useful message.
            $a = $normRedacted -split "`n"
            $b = $normGolden -split "`n"
            $max = [Math]::Max($a.Count, $b.Count)
            for ($i = 0; $i -lt $max; $i++) {
                $la = if ($i -lt $a.Count) { $a[$i] } else { '<eof>' }
                $lb = if ($i -lt $b.Count) { $b[$i] } else { '<eof>' }
                if ($la -ne $lb) { throw "golden mismatch at line $($i+1): got [$la] want [$lb]" }
            }
            throw "golden mismatch (length differs)"
        }
    }

    Invoke-Test "codegen embeds the real SHA256 of each fixture descriptor" {
        $raw = Invoke-ZenithCodegen -GamesRoot $fixtureRoot -OutputPath ''
        foreach ($f in @('Alpha', 'Beta', 'Gamma')) {
            $p = Join-Path $fixtureRoot "$f/$f.zproj"
            $h = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToUpperInvariant()
            Assert-True ($raw -match [regex]::Escape($h)) "codegen should embed SHA256 of $f ($h)"
        }
    }

    Invoke-Test "codegen throws on an invalid descriptor set" {
        $root = New-IsolatedGamesRoot 'cgbad'
        New-FixtureDescriptor -Root $root -Folder 'Widget' -FileBase 'Widget' -Json (Good-Json -Name 'Gadget') | Out-Null
        $threw = $false
        try { Invoke-ZenithCodegen -GamesRoot $root -OutputPath '' | Out-Null } catch { $threw = $true }
        Assert-True $threw "codegen must throw when validation fails"
    }

    # ========================================================================
    Write-Host "`n[6] Worktree guard" -ForegroundColor Cyan
    Invoke-Test "main tree is not flagged as a linked worktree" {
        Assert-False (Test-ZenithInWorktree -RepoRoot $repoRoot) "repo root should not be a linked worktree"
    }

    # ========================================================================
    Write-Host "`n[7] Real Games/*/*.zproj sanity" -ForegroundColor Cyan
    Invoke-Test "all 12 real descriptors validate clean" {
        $scan = Get-ZenithGameDescriptors -GamesRoot (Join-Path $repoRoot 'Games')
        if ($scan.Errors.Count -gt 0) { throw ("real descriptor errors: " + ($scan.Errors -join '; ')) }
        Assert-Equal 12 $scan.Descriptors.Count "expected 12 real descriptors"
    }
    Invoke-Test "android flags match reality (CityBuilder + Test are false)" {
        $scan = Get-ZenithGameDescriptors -GamesRoot (Join-Path $repoRoot 'Games')
        $byName = @{}
        foreach ($d in $scan.Descriptors) { $byName[$d.Name] = $d }
        Assert-False $byName['CityBuilder'].Android "CityBuilder android:false"
        Assert-False $byName['Test'].Android "Test android:false"
        Assert-True $byName['Sokoban'].Android "Sokoban android:true"
        Assert-True $byName['DevilsPlayground'].Android "DP android:true"
        $trueCount = @($scan.Descriptors | Where-Object { $_.Android }).Count
        Assert-Equal 10 $trueCount "expected exactly 10 android:true games"
    }
    Invoke-Test "TilePuzzle carries its two offline-tool extra projects" {
        $scan = Get-ZenithGameDescriptors -GamesRoot (Join-Path $repoRoot 'Games')
        $tp = $scan.Descriptors | Where-Object { $_.Name -eq 'TilePuzzle' }
        Assert-AnyLike $tp.ExtraSharpmakeProjects "TilePuzzleLevelGenProject" "levelgen extra"
        Assert-AnyLike $tp.ExtraSharpmakeProjects "TilePuzzleRegistryViewerProject" "registryviewer extra"
    }
}
finally {
    Remove-Item -Recurse -Force -Path $script:TempRoot -ErrorAction SilentlyContinue
}

# --- Summary -----------------------------------------------------------------
Write-Host ""
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "buildsystem tests: $script:Passed passed, $script:Failed failed" -ForegroundColor Cyan
if ($script:Failed -gt 0) {
    Write-Host "Failures:" -ForegroundColor Red
    $script:Failures | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}
exit 0
