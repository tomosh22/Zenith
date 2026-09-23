# run_buildsystem_tests.ps1
# =============================================================================
# Dependency-free assert-runner for Build/zenith_buildsystem.psm1 (no Pester).
# Covers: name-syntax vectors (shared file), the descriptor validation matrix,
# the codegen golden file + SHA wiring, the worktree guard, and a sanity pass
# over the 5 real Games/*/*.zproj descriptors.
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
        Assert-True ((@(Test-ZenithGameNameSyntax -Name 'ZenithGame').Count) -gt 0) "Zenith+capitalized word reserved"
        Assert-True ((@(Test-ZenithGameNameSyntax -Name 'SentinelGame').Count) -gt 0) "Sentinel+capitalized word reserved"
        Assert-True ((@(Test-ZenithGameNameSyntax -Name 'Zenith').Count) -gt 0) "bare Zenith reserved"
        Assert-True ((@(Test-ZenithGameNameSyntax -Name 'Sentinel').Count) -gt 0) "bare Sentinel reserved"
        Assert-True ((@(Test-ZenithGameNameSyntax -Name 'Zenith2').Count) -gt 0) "Zenith+digit reserved"
        Assert-True ((@(Test-ZenithGameNameSyntax -Name 'NUL').Count) -gt 0) "NUL device"
    }
    Invoke-Test "lowercase continuation of an engine prefix is a distinct word" {
        Assert-Equal 0 (@(Test-ZenithGameNameSyntax -Name 'Zenithmon').Count) "Zenithmon should be valid"
        Assert-Equal 0 (@(Test-ZenithGameNameSyntax -Name 'Sentinelle').Count) "Sentinelle should be valid"
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

    Invoke-Test "docs-only folder -> skipped, not an error" {
        # A game written up before it is written. The whole point is that this must
        # not stop the scan: a descriptor error aborts regen.ps1 REPO-WIDE, and a
        # regen that never runs leaves newly created source files out of the vcxproj
        # while `zenith build` still exits 0.
        $root = New-IsolatedGamesRoot 'docsonly'
        New-Item -ItemType Directory -Force -Path (Join-Path $root 'Concept/Docs') | Out-Null
        Set-Content -LiteralPath (Join-Path $root 'Concept/Docs/GameDesignDocument.md') -Value '# Concept'
        $scan = Get-ZenithGameDescriptors -GamesRoot $root
        Assert-True ($scan.Errors.Count -eq 0) "docs-only folder produced no error (got: $($scan.Errors -join '; '))"
        Assert-True ($scan.Descriptors.Count -eq 0) "docs-only folder contributed no descriptor"
    }

    Invoke-Test "docs and reference mockups -> skipped" {
        $root = New-IsolatedGamesRoot 'docsrefs'
        New-Item -ItemType Directory -Force -Path (Join-Path $root 'Concept/Docs'), (Join-Path $root 'Concept/Mockups') | Out-Null
        Set-Content -LiteralPath (Join-Path $root 'Concept/Mockups/reference.png') -Value 'reference fixture'
        $scan = Get-ZenithGameDescriptors -GamesRoot $root
        Assert-Equal 0 $scan.Errors.Count "reference assets must not block unrelated builds"
        Assert-Equal 0 $scan.Descriptors.Count "reference assets are not a build target"
        Set-Content -LiteralPath (Join-Path $root 'Concept/Main.cpp') -Value '// source requires descriptor'
        $scan = Get-ZenithGameDescriptors -GamesRoot $root
        Assert-AnyLike $scan.Errors "*has no .zproj descriptor*" "source remains an error beside mockups"
    }

    Invoke-Test "folder with SOURCE but no descriptor -> still an error" {
        # The carve-out above must stay narrow. A folder carrying anything besides a
        # single Docs directory is a game somebody forgot to give a descriptor, which
        # is the mistake the check exists for.
        $root = New-IsolatedGamesRoot 'srcnodesc'
        New-Item -ItemType Directory -Force -Path (Join-Path $root 'Halfling/Docs') | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $root 'Halfling/Source') | Out-Null
        Set-Content -LiteralPath (Join-Path $root 'Halfling/Source/Main.cpp') -Value '// game'
        $scan = Get-ZenithGameDescriptors -GamesRoot $root
        Assert-AnyLike $scan.Errors "*has no .zproj descriptor*" "source-bearing folder still errors"
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
    Invoke-Test "worktree detection agrees with checkout metadata" {
        Assert-Equal (Test-Path -LiteralPath (Join-Path $repoRoot '.git') -PathType Leaf) (Test-ZenithInWorktree -RepoRoot $repoRoot) "linked checkout uses a .git file"
    }

    # ========================================================================
    Write-Host "`n[7] Real Games/*/*.zproj sanity" -ForegroundColor Cyan
    # CityBuilder, Combat, DevilsPlayground, RenderTest, TilePuzzle, Zenithmon.
    Invoke-Test "all real descriptors validate clean" {
        $scan = Get-ZenithGameDescriptors -GamesRoot (Join-Path $repoRoot 'Games')
        if ($scan.Errors.Count -gt 0) { throw ("real descriptor errors: " + ($scan.Errors -join '; ')) }
        Assert-Equal @(Get-ChildItem (Join-Path $repoRoot 'Games/*/*.zproj') -File).Count $scan.Descriptors.Count "every real descriptor was scanned"
    }
    Invoke-Test "android flags match reality (CityBuilder is false)" {
        $scan = Get-ZenithGameDescriptors -GamesRoot (Join-Path $repoRoot 'Games')
        $byName = @{}
        foreach ($d in $scan.Descriptors) { $byName[$d.Name] = $d }
        Assert-False $byName['CityBuilder'].Android "CityBuilder android:false"
        Assert-True $byName['Combat'].Android "Combat android:true"
        Assert-True $byName['DevilsPlayground'].Android "DP android:true"
        Assert-True $byName['Zenithmon'].Android "Zenithmon android:true"
        # Combat, DevilsPlayground, RenderTest, TilePuzzle, Zenithmon.
        # CityBuilder is the only android:false game.
        $trueCount = @($scan.Descriptors | Where-Object { $_.Android }).Count
        Assert-Equal 5 $trueCount "expected exactly 5 android:true games"
    }
    Invoke-Test "TilePuzzle carries its two offline-tool extra projects" {
        $scan = Get-ZenithGameDescriptors -GamesRoot (Join-Path $repoRoot 'Games')
        $tp = $scan.Descriptors | Where-Object { $_.Name -eq 'TilePuzzle' }
        Assert-AnyLike $tp.ExtraSharpmakeProjects "TilePuzzleLevelGenProject" "levelgen extra"
        Assert-AnyLike $tp.ExtraSharpmakeProjects "TilePuzzleRegistryViewerProject" "registryviewer extra"
    }

    # ========================================================================
    Write-Host "`n[8] Central config (zenith_config.psd1)" -ForegroundColor Cyan

    Invoke-Test "config data loads with the expected keys" {
        $cfg = Get-ZenithBuildConfigData
        foreach ($k in @('DefaultConfigWin64', 'HubConfigWin64', 'AndroidOutDirTemplate',
                'AndroidMsBuildConfigTemplate', 'AndroidAbis', 'SlangVersion', 'VulkanSdkVersion', 'ArtifactsRoot')) {
            Assert-True ($cfg.ContainsKey($k)) "config key '$k' present"
        }
    }
    Invoke-Test "the Android ABI axis carries both spellings of each ABI" {
        $cfg = Get-ZenithBuildConfigData
        # Key = config-name token, Value = on-disk ABI dir. They differ for
        # arm64 and coincide for x86_64 -- the trap ZenithAndroidAbi documents.
        Assert-Equal 'arm64-v8a' $cfg.AndroidAbis['arm64_v8a'] "arm64 dir name"
        Assert-Equal 'x86_64'    $cfg.AndroidAbis['x86_64']    "x86_64 dir name"
    }
    Invoke-Test "agde MSBuild config name carries the Vulkan_ prefix and the ABI" {
        $cfg = Get-ZenithBuildConfigData
        Assert-Equal 'Vulkan_x86_64_vs2022_Debug_Agde_False' ($cfg.AndroidMsBuildConfigTemplate -f 'x86_64', 'Debug') "emulator config name"
        Assert-Equal 'x86_64_vs2022_debug_agde_false' ($cfg.AndroidOutDirTemplate -f 'x86_64', 'debug') "emulator OutDir leaf"
    }
    Invoke-Test "Get-ZenithAndroidAbis hands back both spellings, per ABI" {
        $abis = Get-ZenithAndroidAbis
        Assert-Equal 2 $abis.Count "ABI count"
        $arm = $abis | Where-Object { $_.Token -eq 'arm64_v8a' }
        Assert-Equal 'arm64-v8a' $arm.DirName "arm64 DirName from accessor"
        # Accessor output must never be indexed positionally by spelling alone --
        # this pins that Token and DirName really are distinct fields.
        Assert-True ($arm.Token -ne $arm.DirName) "arm64 spellings differ"
    }
    Invoke-Test "Android path accessors match the psd1 templates" {
        Assert-Equal 'x86_64_vs2022_debug_agde_false' (Get-ZenithAndroidOutDir -AbiToken 'x86_64' -BuildType 'debug') "OutDir accessor"
        Assert-Equal 'Vulkan_arm64_v8a_vs2022_Release_Agde_False' (Get-ZenithAndroidMsBuildConfig -AbiToken 'arm64_v8a' -BuildType 'release') "MSBuild config accessor"
    }
    Invoke-Test "the ABI axis agrees across Sharpmake, Gradle and zenith_config" {
        # THE drift test. The axis is declared in three places by necessity
        # (C#/Gradle/PowerShell cannot share a literal); nothing but this test
        # stops them diverging, and a divergence is silent -- either an APK
        # missing an ABI, or a packaged ABI with no native lib in it.
        #
        # It compares the full token->dir PAIR from every source, and it READS
        # each source's own spelling rather than re-deriving one. An earlier cut
        # of this test transformed the Sharpmake enum name with a hardcoded
        # `-replace '_v8a$','-v8a'`, which (a) never looked at ZenithAndroidAbi's
        # ConfigToken/DirName switch tables at all, so a wrong spelling THERE --
        # exactly the trap the axis exists to prevent -- passed green, and
        # (b) baked in the very rule under test, so a correct widening to an ABI
        # that does not follow the _v8a pattern would have gone red.
        $cfgPairs = @((Get-ZenithAndroidAbis) | ForEach-Object { "$($_.Token)=>$($_.DirName)" } | Sort-Object)

        $commonCs = Get-Content -Raw (Join-Path $repoRoot 'Build/Sharpmake_Common.cs')

        # Pull each switch table body out by name, then the case -> literal pairs.
        function Get-AbiSwitchMap([string]$Source, [string]$FnName) {
            if ($Source -notmatch ("(?s)string\s+" + [regex]::Escape($FnName) + "\s*\([^)]*\)\s*\{(.*?)\n\t\}")) {
                throw "could not find ZenithAndroidAbi.$FnName in Sharpmake_Common.cs"
            }
            $map = @{}
            foreach ($m in [regex]::Matches($Matches[1], 'case\s+Android\.AndroidBuildTargets\.(\w+)\s*:\s*return\s+"([^"]+)"')) {
                $map[$m.Groups[1].Value] = $m.Groups[2].Value
            }
            if ($map.Count -eq 0) { throw "ZenithAndroidAbi.$FnName has no case arms" }
            return $map
        }
        $tokenMap = Get-AbiSwitchMap $commonCs 'ConfigToken'
        $dirMap = Get-AbiSwitchMap $commonCs 'DirName'

        # ZenithAndroidAbi.All -- the enum members the agde configs are generated for.
        if ($commonCs -notmatch 'AndroidBuildTargets\s+All\s*=>([^;]+);') {
            throw "could not find ZenithAndroidAbi.All in Sharpmake_Common.cs"
        }
        $allMembers = @([regex]::Matches($Matches[1], 'AndroidBuildTargets\.(\w+)') | ForEach-Object { $_.Groups[1].Value })

        # Every ABI in All must be spelled by BOTH switch tables, or ConfigToken/
        # DirName throws at generation time for it.
        foreach ($member in $allMembers) {
            Assert-True $tokenMap.ContainsKey($member) "ConfigToken covers '$member'"
            Assert-True $dirMap.ContainsKey($member)   "DirName covers '$member'"
        }
        $sharpmakePairs = @($allMembers | ForEach-Object { "$($tokenMap[$_])=>$($dirMap[$_])" } | Sort-Object)

        # Gradle: ext.zenithAbis = ['arm64-v8a': 'arm64_v8a', ...] -- DIR : TOKEN,
        # the TRANSPOSE of the psd1 map. Normalise to token=>dir before comparing.
        $gradle = Get-Content -Raw (Join-Path $repoRoot 'Build/zenith_android_abis.gradle')
        if ($gradle -notmatch 'ext\.zenithAbis\s*=\s*\[([^\]]+)\]') {
            throw "could not find ext.zenithAbis in zenith_android_abis.gradle"
        }
        $gradlePairs = @([regex]::Matches($Matches[1], "'([^']+)'\s*:\s*'([^']+)'") |
            ForEach-Object { "$($_.Groups[2].Value)=>$($_.Groups[1].Value)" } | Sort-Object)

        Assert-Equal ($cfgPairs -join ' | ') ($sharpmakePairs -join ' | ') "Sharpmake axis vs zenith_config"
        Assert-Equal ($cfgPairs -join ' | ') ($gradlePairs -join ' | ')    "Gradle axis vs zenith_config"
    }
    Invoke-Test "every android:true game applies the shared ABI axis" {
        # A game that hardcodes abiFilters silently drops every other ABI at
        # package time, which reads as INSTALL_FAILED_NO_MATCHING_ABIS on device.
        $scan = Get-ZenithGameDescriptors
        foreach ($d in @($scan.Descriptors | Where-Object { $_.Android })) {
            $gradlePath = Join-Path $repoRoot "Games/$($d.Name)/Android/app/build.gradle"
            Assert-True (Test-Path $gradlePath) "$($d.Name) has app/build.gradle"
            $text = Get-Content -Raw $gradlePath
            Assert-True ($text -match 'zenith_android_abis\.gradle') "$($d.Name) applies the shared ABI axis"
            Assert-True ($text -match 'abiFilters\.addAll\(\s*zenithAbis\.keySet\(\)\s*\)') "$($d.Name) filters on the shared axis"
            Assert-False ($text -match "abiFilters\s+'") "$($d.Name) does not hardcode an ABI"
        }
    }
    Invoke-Test "default config is the documented Vulkan Debug True" {
        Assert-Equal 'Vulkan_vs2022_Debug_Win64_True' (Get-ZenithDefaultConfig) "default config"
    }
    Invoke-Test "ConvertTo-ZenithOutputDir lowercases the config name" {
        Assert-Equal 'vulkan_vs2022_debug_win64_true' (ConvertTo-ZenithOutputDir -Config 'Vulkan_vs2022_Debug_Win64_True') "lowercase mapping"
    }
    Invoke-Test "Get-ZenithGameExePath composes the exact expected path" {
        $p = Get-ZenithGameExePath -Name 'Combat' -Config 'Vulkan_vs2022_Debug_Win64_True' -RepoRoot 'C:\r'
        $norm = $p.Replace('\', '/')
        Assert-Equal 'C:/r/Games/Combat/Build/output/win64/vulkan_vs2022_debug_win64_true/combat.exe' $norm "exe path"
    }

    # ========================================================================
    Write-Host "`n[9] Regen drift detection (Test-ZenithRegenDrift)" -ForegroundColor Cyan

    # Shared fixture: a valid games root + a generated file + fake slns.
    function New-DriftFixture {
        $root = New-IsolatedGamesRoot 'drift'
        New-FixtureDescriptor -Root $root -Folder 'Widget' -FileBase 'Widget' -Json (Good-Json -Name 'Widget') | Out-Null
        $fakeRepo = Join-Path $script:TempRoot ("driftrepo_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        New-Item -ItemType Directory -Force -Path (Join-Path $fakeRepo 'Build') | Out-Null
        $gen = Join-Path $fakeRepo 'Build/Sharpmake_GameInstances.generated.cs'
        Invoke-ZenithCodegen -GamesRoot $root -OutputPath $gen | Out-Null
        New-Item -ItemType File -Force -Path (Join-Path $fakeRepo 'Build/zenith_engine_win64.sln') | Out-Null
        New-Item -ItemType File -Force -Path (Join-Path $root 'Widget/widget_win64.sln') | Out-Null
        return [PSCustomObject]@{ GamesRoot = $root; RepoRoot = $fakeRepo; Generated = $gen }
    }

    Invoke-Test "in-sync fixture reports InSync" {
        $f = New-DriftFixture
        # GamesRoot is outside the fake repo, so pass all three explicitly.
        $d = Test-ZenithRegenDrift -GamesRoot $f.GamesRoot -GeneratedPath $f.Generated -RepoRoot $f.RepoRoot
        Assert-True $d.InSync "expected in-sync. reasons: $($d.Reasons -join '; ')"
    }
    Invoke-Test "mutated descriptor reports drift" {
        $f = New-DriftFixture
        $zproj = Join-Path $f.GamesRoot 'Widget/Widget.zproj'
        [System.IO.File]::AppendAllText($zproj, ' ')
        $d = Test-ZenithRegenDrift -GamesRoot $f.GamesRoot -GeneratedPath $f.Generated -RepoRoot $f.RepoRoot
        Assert-False $d.InSync "expected drift after descriptor mutation"
        Assert-AnyLike $d.Reasons "*stale relative to the descriptors*" "stale reason"
    }
    Invoke-Test "missing generated file reports drift" {
        $f = New-DriftFixture
        Remove-Item -LiteralPath $f.Generated -Force
        $d = Test-ZenithRegenDrift -GamesRoot $f.GamesRoot -GeneratedPath $f.Generated -RepoRoot $f.RepoRoot
        Assert-False $d.InSync "expected drift when generated file missing"
        Assert-AnyLike $d.Reasons "*generated file missing*" "missing reason"
    }
    Invoke-Test "missing game sln reports drift" {
        $f = New-DriftFixture
        Remove-Item -LiteralPath (Join-Path $f.GamesRoot 'Widget/widget_win64.sln') -Force
        $d = Test-ZenithRegenDrift -GamesRoot $f.GamesRoot -GeneratedPath $f.Generated -RepoRoot $f.RepoRoot
        Assert-False $d.InSync "expected drift when a game sln is missing"
        Assert-AnyLike $d.Reasons "*game solution missing*" "sln reason"
    }
    Invoke-Test "descriptor validation errors report drift with reasons" {
        $f = New-DriftFixture
        New-FixtureDescriptor -Root $f.GamesRoot -Folder 'Bad' -FileBase 'Bad' -Json '{ not json' | Out-Null
        $d = Test-ZenithRegenDrift -GamesRoot $f.GamesRoot -GeneratedPath $f.Generated -RepoRoot $f.RepoRoot
        Assert-False $d.InSync "expected drift on descriptor errors"
        Assert-AnyLike $d.Reasons "descriptor:*" "descriptor-prefixed reason"
    }

    # ========================================================================
    Write-Host "`n[10] Build-process hygiene" -ForegroundColor Cyan

    Invoke-Test "Stop-ZenithBuildProcesses -DryRun runs without throwing" {
        # Callers always wrap with @(...): an empty enumerated return unwraps to
        # $null in plain assignment, which @() normalizes to a 0-count array.
        $r = @(Stop-ZenithBuildProcesses -DryRun)
        Assert-True ($r.Count -ge 0) "dry-run sweep completed"
    }
    Invoke-Test "impossible age filter returns empty (kills nothing)" {
        $r = @(Stop-ZenithBuildProcesses -DryRun -OlderThanMinutes 525600)
        Assert-Equal 0 $r.Count "one-year age filter should match nothing"
    }

    # ========================================================================
    Write-Host "`n[11] Runtime DLL heal (Repair-ZenithRuntimeDlls)" -ForegroundColor Cyan

    Invoke-Test "copies missing slang DLLs, never overwrites existing ones" {
        $base = Join-Path $script:TempRoot ("dll_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        $exeDir = Join-Path $base 'out'
        $slang = Join-Path $base 'slangbin'
        New-Item -ItemType Directory -Force -Path $exeDir, $slang | Out-Null
        [System.IO.File]::WriteAllText((Join-Path $slang 'slang.dll'), 'fresh', $utf8NoBom)
        [System.IO.File]::WriteAllText((Join-Path $slang 'slang-rt.dll'), 'fresh', $utf8NoBom)
        [System.IO.File]::WriteAllText((Join-Path $exeDir 'slang.dll'), 'existing', $utf8NoBom)
        # -AssimpBinDir MUST be overridden too, or the function falls back to the
        # REAL repo Tools/Middleware/assimp/bin and this test's count depends on
        # whether the dev box happens to have the assimp middleware checked out
        # (7 copies here, 1 on a bare machine). All three sources get fixtures.
        $copied = @(Repair-ZenithRuntimeDlls -ExeDir $exeDir -SlangBinDir $slang `
                -AssimpBinDir (Join-Path $base 'nosuchassimp') -SiblingGlob (Join-Path $base 'nosuch/*'))
        Assert-Equal 1 $copied.Count "exactly one DLL copied"
        Assert-Equal 'slang-rt.dll' $copied[0] "the missing one"
        Assert-Equal 'existing' (Get-Content -LiteralPath (Join-Path $exeDir 'slang.dll') -Raw) "existing DLL untouched"
    }
    Invoke-Test "pulls missing DLLs from sibling output dirs" {
        $base = Join-Path $script:TempRoot ("sib_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        $exeDir = Join-Path $base 'games/A/leaf'
        $sib = Join-Path $base 'games/B/leaf'
        New-Item -ItemType Directory -Force -Path $exeDir, $sib | Out-Null
        [System.IO.File]::WriteAllText((Join-Path $sib 'assimp.dll'), 'sib', $utf8NoBom)
        # As above: every source needs a fixture, including assimp, or the real
        # repo middleware leaks into the count on a fully-provisioned machine.
        $copied = @(Repair-ZenithRuntimeDlls -ExeDir $exeDir -SlangBinDir (Join-Path $base 'nosuchslang') `
                -AssimpBinDir (Join-Path $base 'nosuchassimp') -SiblingGlob (Join-Path $base 'games/*/leaf'))
        Assert-Equal 1 $copied.Count "one DLL from sibling"
        Assert-True (Test-Path (Join-Path $exeDir 'assimp.dll')) "assimp.dll copied"
    }

    # ========================================================================
    Write-Host "`n[12] Orphan-artifact prune (Remove-ZenithOrphanGameArtifacts)" -ForegroundColor Cyan

    Invoke-Test "orphan game dir loses slns + vcxprojs, keeps sources; live game untouched" {
        $fakeRepo = Join-Path $script:TempRoot ("prune_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        $games = Join-Path $fakeRepo 'Games'
        # Live game with a descriptor.
        New-FixtureDescriptor -Root $games -Folder 'Live' -FileBase 'Live' -Json (Good-Json -Name 'Live') | Out-Null
        New-Item -ItemType File -Force -Path (Join-Path $games 'Live/live_win64.sln') | Out-Null
        # Orphan dir: no descriptor, generated artifacts + a source file.
        $orphan = Join-Path $games 'Ghost'
        New-Item -ItemType Directory -Force -Path (Join-Path $orphan 'Build') | Out-Null
        New-Item -ItemType File -Force -Path (Join-Path $orphan 'ghost_win64.sln') | Out-Null
        New-Item -ItemType File -Force -Path (Join-Path $orphan 'Build/ghost_win64.vcxproj') | Out-Null
        New-Item -ItemType File -Force -Path (Join-Path $orphan 'Ghost.cpp') | Out-Null

        $scan = Get-ZenithGameDescriptors -GamesRoot $games
        # Ghost has no .zproj -> scan reports an error but still returns Live.
        $removed = @(Remove-ZenithOrphanGameArtifacts -RepoRoot $fakeRepo -Descriptors $scan.Descriptors)
        Assert-Equal 2 $removed.Count "two artifacts pruned. got: $($removed -join '; ')"
        Assert-False (Test-Path (Join-Path $orphan 'ghost_win64.sln')) "orphan sln pruned"
        Assert-False (Test-Path (Join-Path $orphan 'Build/ghost_win64.vcxproj')) "orphan vcxproj pruned"
        Assert-True (Test-Path (Join-Path $orphan 'Ghost.cpp')) "sources NEVER pruned"
        Assert-True (Test-Path (Join-Path $games 'Live/live_win64.sln')) "live game untouched"
    }

    Invoke-Test "android:false game loses stale agde artifacts, keeps win64 ones" {
        $fakeRepo = Join-Path $script:TempRoot ("prune2_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        $games = Join-Path $fakeRepo 'Games'
        New-FixtureDescriptor -Root $games -Folder 'Widget' -FileBase 'Widget' -Json (Good-Json -Name 'Widget') | Out-Null
        $wdir = Join-Path $games 'Widget'
        New-Item -ItemType Directory -Force -Path (Join-Path $wdir 'Build') | Out-Null
        New-Item -ItemType File -Force -Path (Join-Path $wdir 'widget_win64.sln') | Out-Null
        New-Item -ItemType File -Force -Path (Join-Path $wdir 'widget_agde.sln') | Out-Null
        New-Item -ItemType File -Force -Path (Join-Path $wdir 'Build/widget_agde.vcxproj') | Out-Null

        $scan = Get-ZenithGameDescriptors -GamesRoot $games
        $removed = @(Remove-ZenithOrphanGameArtifacts -RepoRoot $fakeRepo -Descriptors $scan.Descriptors)
        Assert-Equal 2 $removed.Count "agde sln + vcxproj pruned. got: $($removed -join '; ')"
        Assert-True (Test-Path (Join-Path $wdir 'widget_win64.sln')) "win64 sln kept"
        Assert-False (Test-Path (Join-Path $wdir 'widget_agde.sln')) "agde sln pruned"
    }

    # ========================================================================
    Write-Host "`n[13] Tracking policy (regenerate-first invariants)" -ForegroundColor Cyan

    Invoke-Test "no generated/transient files are git-tracked" {
        Push-Location $repoRoot
        try {
            # Android: .gradle/ is Gradle's own cache and .agde/ is AGDE-generated
            # (it bakes a machine-local MSBuild path and the last-built ABI). Both
            # were tracked for TilePuzzle until they were untracked; a .gitignore
            # rule alone does NOT untrack, so this pattern is what keeps them out.
            $bad = @(git ls-files -- "Build/*.vcxproj*" "Build/*.sln" "Games/*/build/*.vcxproj*" "Games/*/Build/*.vcxproj*" "Build/*.log" "Build/tmpclaude-*" "Build/dp_telemetry" "Build/citybuilder_test_results" "Build/artifacts" "Games/*/Android/.gradle/*" "Games/*/Android/.agde/*" "Games/*/Android/app/build/*" "Games/*/Android/local.properties" 2>$null)
        }
        finally { Pop-Location }
        if ($bad.Count -gt 0) { throw ("tracked generated/transient files (policy violation): " + ($bad -join '; ')) }
    }

    Invoke-Test "hand-written build files are NOT gitignored" {
        Push-Location $repoRoot
        try {
            $mustNotIgnore = @(
                'Build/Sharpmake_Common.cs', 'Build/regen.ps1', 'Build/zenith_buildsystem.psm1',
                'Build/zenith_config.psd1', 'Build/Templates/NewGame/template.json',
                'Build/Tests/run_buildsystem_tests.ps1',
                # Hand-written and load-bearing: every game's app/build.gradle
                # does `apply from:` this. The Games/*/Android/ ignore rules sit
                # close enough to it that a widened rule could swallow it.
                'Build/zenith_android_abis.gradle',
                # Per-game Gradle scaffolding is hand-written, and lives right
                # next to the .gradle/ + .agde/ dirs that ARE ignored.
                'Games/TilePuzzle/Android/app/build.gradle',
                'Games/TilePuzzle/Android/settings.gradle',
                'Games/Zenithmon/Android/app/build.gradle',
                'Games/Zenithmon/Android/app/src/main/AndroidManifest.xml'
            )
            $ignored = @(git check-ignore @mustNotIgnore 2>$null)
        }
        finally { Pop-Location }
        if ($ignored.Count -gt 0) { throw ("hand-written files are ignored (surgical-ignore violation): " + ($ignored -join '; ')) }
    }

    # ========================================================================
    # WP5: regen staleness (plan Sec.2 "regen_sha in every checkout") +
    # registered lanes. No git-fixture idiom existed for this module before
    # WP5 (Tools/Test_TickGates.ps1's New-GitFixture is the model this
    # follows) -- every test below builds a real throwaway `git init` repo
    # under $script:TempRoot, never touches the real checkout, and never runs
    # msbuild/regen/Sharpmake.
    Write-Host "`n[14] WP5: project-shaping predicate + untracked-file listing" -ForegroundColor Cyan

    function New-Wp5GitFixture {
        # A throwaway repo with a tracked .cpp, a .gitignore excluding
        # Build/artifacts/ (mirrors the real repo's .gitignore:212), and one
        # commit. Every regen-staleness test below builds on this.
        $dir = Join-Path $script:TempRoot ("wp5_git_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        & git -C $dir init --quiet 2>$null | Out-Null
        & git -C $dir config user.email 'test@example.invalid' 2>$null | Out-Null
        & git -C $dir config user.name 'test' 2>$null | Out-Null
        Set-Content -LiteralPath (Join-Path $dir '.gitignore') -Value '/Build/artifacts/'
        New-Item -ItemType Directory -Path (Join-Path $dir 'Games/Probe') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $dir 'Games/Probe/Existing.cpp') -Value '// tracked'
        & git -C $dir add -A 2>$null | Out-Null
        & git -C $dir commit -m init --quiet 2>$null | Out-Null
        return $dir
    }

    function Invoke-Wp5Git {
        param([string]$Dir, [string[]]$GitArgs)
        & git -C $Dir @GitArgs 2>$null | Out-Null
    }

    Invoke-Test "Test-ZenithProjectShapingPath: extensions, Middleware exclusion, Sharpmake/.zproj, backslashes" {
        Assert-True (Test-ZenithProjectShapingPath -RelativePath 'Games/X/Y.cpp') "cpp"
        Assert-True (Test-ZenithProjectShapingPath -RelativePath 'Zenith/Flux/Z.h') "h"
        Assert-True (Test-ZenithProjectShapingPath -RelativePath 'A/B.inl') "inl"
        Assert-True (Test-ZenithProjectShapingPath -RelativePath 'A/B.slang') "slang"
        Assert-False (Test-ZenithProjectShapingPath -RelativePath 'A/B.hpp') "hpp is NOT in SourceFilesExtensions"
        Assert-False (Test-ZenithProjectShapingPath -RelativePath 'A/B.cc') "cc is NOT in SourceFilesExtensions"
        Assert-False (Test-ZenithProjectShapingPath -RelativePath 'Middleware/foo/bar.cpp') "Middleware/ is excluded"
        Assert-True (Test-ZenithProjectShapingPath -RelativePath 'Games\X\Y.cpp') "backslash paths normalise"
        Assert-True (Test-ZenithProjectShapingPath -RelativePath 'Build/Sharpmake_Zenith.cs') "Sharpmake source"
        Assert-True (Test-ZenithProjectShapingPath -RelativePath 'Games/Zenithmon/Zenithmon.zproj') "descriptor"
        Assert-False (Test-ZenithProjectShapingPath -RelativePath 'Docs/BuildSystem.md') "unrelated file"
    }

    Invoke-Test "Get-ZenithUntrackedProjectShapingFiles: only untracked + shaping, sorted" {
        $repo = New-Wp5GitFixture
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Zeta.cpp') -Value '// u'
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Alpha.cpp') -Value '// u'
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/readme.md') -Value '// not shaping'
        $found = @(Get-ZenithUntrackedProjectShapingFiles -RepoRoot $repo)
        Assert-Equal 2 $found.Count "only the two .cpp files, readme.md excluded"
        Assert-Equal 'Games/Probe/Alpha.cpp' $found[0] "sorted first"
        Assert-Equal 'Games/Probe/Zeta.cpp' $found[1] "sorted second"
    }

    Invoke-Test "Get-ZenithUntrackedProjectShapingFiles: clean tree -> empty array, never null" {
        $repo = New-Wp5GitFixture
        $found = @(Get-ZenithUntrackedProjectShapingFiles -RepoRoot $repo)
        Assert-Equal 0 $found.Count "clean tree, nothing untracked"
    }

    # ========================================================================
    Write-Host "`n[15] WP5: Write-ZenithRegenStateFile + Test-ZenithRegenStale" -ForegroundColor Cyan

    Invoke-Test "Write-ZenithRegenStateFile: writes under Build/artifacts/, gitignored, porcelain status unchanged" {
        $repo = New-Wp5GitFixture
        $before = @(& git -C $repo status --porcelain)
        $ok = Write-ZenithRegenStateFile -RepoRoot $repo
        Assert-True $ok "write should succeed against a real git repo with a HEAD"
        $shaFile = Join-Path $repo 'Build/artifacts/regen_sha'
        Assert-True (Test-Path -LiteralPath $shaFile) "regen_sha should exist"
        $after = @(& git -C $repo status --porcelain)
        Assert-Equal ($before -join '|') ($after -join '|') "porcelain status unchanged (Build/artifacts/ is gitignored)"
    }

    Invoke-Test "Write-ZenithRegenStateFile: writes NOTHING when HEAD cannot be resolved (unborn repo)" {
        $dir = Join-Path $script:TempRoot ("wp5_unborn_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        & git -C $dir init --quiet 2>$null | Out-Null
        $ok = Write-ZenithRegenStateFile -RepoRoot $dir
        Assert-False $ok "no commits yet -> HEAD unresolvable"
        Assert-False (Test-Path -LiteralPath (Join-Path $dir 'Build/artifacts/regen_sha')) "nothing written on failure"
    }

    Invoke-Test "clean tree, freshly written state -> NOT stale" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-False $r.Stale "reason: $($r.Reason)"
    }

    Invoke-Test "missing regen_sha -> stale" {
        $repo = New-Wp5GitFixture
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "no state file at all"
        Assert-AnyLike @($r.Reason) "*no Build/artifacts/regen_sha*" "reason names the missing file"
    }

    Invoke-Test "unresolvable regen_sha commit (rebase / deleted branch) -> stale" {
        $repo = New-Wp5GitFixture
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Build/artifacts') | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Build/artifacts/regen_sha') -Value 'deadbeefdeadbeefdeadbeefdeadbeefdeadbeef'
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "unresolvable commit"
        Assert-AnyLike @($r.Reason) "*unresolvable*" "reason names the sha"
    }

    Invoke-Test "a tracked .cpp ADDED since the last regen -> stale (A)" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/New.cpp') -Value '// new'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add', '--quiet')
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "added source"
        Assert-AnyLike @($r.Reason) "*New.cpp*" "reason names the file"
    }

    Invoke-Test "a tracked .slang ADDED since the last regen -> stale" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Shader.slang') -Value '// s'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add', '--quiet')
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "added shader"
    }

    Invoke-Test "a tracked .cpp MODIFIED only -> NOT stale" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Add-Content -LiteralPath (Join-Path $repo 'Games/Probe/Existing.cpp') -Value '// edited'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'edit', '--quiet')
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-False $r.Stale "a plain M on a .cpp is not stale: $($r.Reason)"
    }

    Invoke-Test "Build/Sharpmake_X.cs MODIFIED -> stale (ANY status, not just A/D/R)" {
        $repo = New-Wp5GitFixture
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Build') | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_X.cs') -Value '// x'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add sharpmake', '--quiet')
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Add-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_X.cs') -Value '// tweak'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'tweak', '--quiet')
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "a plain M on a Sharpmake_*.cs must still trigger"
    }

    Invoke-Test "a NEW Build/Sharpmake_Y.cs -> stale" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Build') | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_Y.cs') -Value '// y'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add', '--quiet')
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "a newly ADDED Sharpmake_*.cs needs a regen too"
    }

    Invoke-Test "a .zproj MODIFIED -> stale" {
        $repo = New-Wp5GitFixture
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Probe.zproj') -Value '{ }'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add zproj', '--quiet')
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Add-Content -LiteralPath (Join-Path $repo 'Games/Probe/Probe.zproj') -Value ' '
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'edit zproj', '--quiet')
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "descriptor edit"
    }

    Invoke-Test "Middleware/*.cpp ADDED -> NOT stale" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Middleware') | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Middleware/foo.cpp') -Value '// vendored'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'vendor', '--quiet')
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-False $r.Stale "vendored source excluded: $($r.Reason)"
    }

    Invoke-Test "an untracked NEW .cpp -> stale; after a (simulated) regen records it, the SAME file -> NOT stale" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Untracked.cpp') -Value '// u'
        $r1 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r1.Stale "new untracked source"
        Assert-AnyLike @($r1.Reason) "*Untracked.cpp*" "reason names it"
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        $r2 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-False $r2.Stale "the SAME untracked file, now recorded, stops tripping it: $($r2.Reason)"
    }

    Invoke-Test "a fully clean tree (no changes since the last regen) -> NOT stale" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-False $r.Stale "clean: $($r.Reason)"
    }

    # ------------------------------------------------------------------------
    # WP5 follow-up (ZEN-111): the working-tree dirty fingerprint. A `/tick`
    # gates BEFORE it commits, so these exercise UNCOMMITTED (staged and
    # unstaged) project-shaping edits -- the gap the committed-diff loop above
    # cannot see.
    Write-Host "`n[15b] WP5 follow-up: uncommitted project-shaping edits (ZEN-111)" -ForegroundColor Cyan

    Invoke-Test "uncommitted M on a Build/Sharpmake_X.cs (regen state written WITHOUT it) -> stale, reason names the file + 'uncommitted'" {
        $repo = New-Wp5GitFixture
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Build') | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_X.cs') -Value '// x'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add sharpmake', '--quiet')
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Add-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_X.cs') -Value '// uncommitted tweak'
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "uncommitted M on a Sharpmake file the last regen did not see"
        Assert-AnyLike @($r.Reason) "*Sharpmake_X.cs*" "reason names the file"
        Assert-AnyLike @($r.Reason) "*uncommitted*" "reason says uncommitted"
    }

    Invoke-Test "no-regen-loop cycle for an uncommitted M on a Sharpmake file: record it -> NOT stale; edit again -> stale; revert to HEAD -> stale" {
        $repo = New-Wp5GitFixture
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Build') | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_X.cs') -Value '// x'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add sharpmake', '--quiet')

        Add-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_X.cs') -Value '// uncommitted tweak 1'
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        $r1 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-False $r1.Stale "the dirty edit was recorded at regen time: $($r1.Reason)"

        Add-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_X.cs') -Value '// uncommitted tweak 2'
        $r2 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r2.Stale "content hash changed again since the recorded fingerprint"

        Invoke-Wp5Git -Dir $repo -GitArgs @('checkout', '--', 'Build/Sharpmake_X.cs')
        $r3 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r3.Stale "reverted to HEAD -- the recorded dirty entry is now absent (full set equality, both directions)"
    }

    Invoke-Test "uncommitted M on a Games/G/G.zproj -> stale" {
        $repo = New-Wp5GitFixture
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Games/G') | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Games/G/G.zproj') -Value '{ }'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add zproj', '--quiet')
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Add-Content -LiteralPath (Join-Path $repo 'Games/G/G.zproj') -Value ' '
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "uncommitted descriptor edit"
    }

    Invoke-Test "working-tree deletion (unstaged) of a tracked source -> stale; git mv rename of one -> stale" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Remove-Item -LiteralPath (Join-Path $repo 'Games/Probe/Existing.cpp') -Force
        $r1 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r1.Stale "unstaged deletion of a tracked source"

        $repo2 = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo2 | Out-Null
        Invoke-Wp5Git -Dir $repo2 -GitArgs @('mv', 'Games/Probe/Existing.cpp', 'Games/Probe/Renamed.cpp')
        $r2 = Test-ZenithRegenStale -RepoRoot $repo2
        Assert-True $r2.Stale "staged git mv rename of a tracked source"
    }

    Invoke-Test "uncommitted M on a tracked .cpp only -> NOT stale (same rule as the committed path)" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Add-Content -LiteralPath (Join-Path $repo 'Games/Probe/Existing.cpp') -Value '// uncommitted edit'
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-False $r.Stale "a plain uncommitted M on a .cpp is not stale: $($r.Reason)"
    }

    Invoke-Test "an old-format regen_sha (HEAD line + a bare untracked path, no 'untracked '/'dirty ' prefix) -> stale" {
        $repo = New-Wp5GitFixture
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Build/artifacts') | Out-Null
        $headSha = ([string](& git -C $repo rev-parse HEAD)).Trim()
        Set-Content -LiteralPath (Join-Path $repo 'Build/artifacts/regen_sha') -Value @($headSha, 'Games/Probe/SomeUntracked.cpp')
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "old bare-path format must be treated as stale, no legacy reader path"
        Assert-AnyLike @($r.Reason) "*old format*" "reason names the format problem"
    }

    Invoke-Test "no-regen-loop cycle for a working-tree deletion: record -> NOT stale; delete a second file -> stale; restore the first -> stale" {
        $repo = New-Wp5GitFixture
        Set-Content -LiteralPath (Join-Path $repo 'Games/Probe/Second.cpp') -Value '// second tracked'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add second', '--quiet')

        Remove-Item -LiteralPath (Join-Path $repo 'Games/Probe/Existing.cpp') -Force
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        $r1 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-False $r1.Stale "the deletion was recorded at regen time: $($r1.Reason)"

        Remove-Item -LiteralPath (Join-Path $repo 'Games/Probe/Second.cpp') -Force
        $r2 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r2.Stale "a second deletion, not yet recorded"

        Invoke-Wp5Git -Dir $repo -GitArgs @('checkout', '--', 'Games/Probe/Existing.cpp')
        $r3 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r3.Stale "the first file was restored -- its recorded deletion entry is now absent"
    }

    Invoke-Test "no-regen-loop cycle for a git mv rename: record -> NOT stale; rename again -> stale; rename back to the original path -> stale" {
        $repo = New-Wp5GitFixture
        Invoke-Wp5Git -Dir $repo -GitArgs @('mv', 'Games/Probe/Existing.cpp', 'Games/Probe/Renamed.cpp')
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        $r1 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-False $r1.Stale "the rename was recorded at regen time: $($r1.Reason)"

        Invoke-Wp5Git -Dir $repo -GitArgs @('mv', 'Games/Probe/Renamed.cpp', 'Games/Probe/RenamedAgain.cpp')
        $r2 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r2.Stale "renamed again -- a different new-path entry than what was recorded"

        Invoke-Wp5Git -Dir $repo -GitArgs @('mv', 'Games/Probe/RenamedAgain.cpp', 'Games/Probe/Existing.cpp')
        $r3 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r3.Stale "renamed back to the original path -- fully reverted, the recorded rename entry is now absent"
    }

    Invoke-Test "a STAGED git mv of Build/Sharpmake_Old.cs -> Build/Sharpmake_New.cs -> stale (the porcelain-arrow trap)" {
        $repo = New-Wp5GitFixture
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Build') | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_Old.cs') -Value '// old'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add sharpmake', '--quiet')
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        Invoke-Wp5Git -Dir $repo -GitArgs @('mv', 'Build/Sharpmake_Old.cs', 'Build/Sharpmake_New.cs')
        $r = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r.Stale "a staged Sharpmake rename must trip even though 'git status --porcelain' would print it as one 'old -> new' field"
    }

    Invoke-Test "a Sharpmake/.zproj change STAGED (git add, not committed) -> stale; staged + further unstaged edit -> stale" {
        $repo = New-Wp5GitFixture
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Build') | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_Staged.cs') -Value '// x'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', '-A')
        Invoke-Wp5Git -Dir $repo -GitArgs @('commit', '-m', 'add sharpmake', '--quiet')
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null

        Add-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_Staged.cs') -Value '// staged edit'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', 'Build/Sharpmake_Staged.cs')
        $r1 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r1.Stale "staged (not committed) Sharpmake edit"

        Add-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_Staged.cs') -Value '// further unstaged edit'
        $r2 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r2.Stale "staged AND further unstaged edit, still stale"
    }

    Invoke-Test "a STAGED-NEW Build/Sharpmake_Y.cs -> stale; a STAGED-NEW Src/new.cpp -> stale (A)" {
        $repo = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo | Out-Null
        New-Item -ItemType Directory -Force -Path (Join-Path $repo 'Build') | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Build/Sharpmake_Y.cs') -Value '// y'
        Invoke-Wp5Git -Dir $repo -GitArgs @('add', 'Build/Sharpmake_Y.cs')
        $r1 = Test-ZenithRegenStale -RepoRoot $repo
        Assert-True $r1.Stale "a staged-new Sharpmake file"

        $repo2 = New-Wp5GitFixture
        Write-ZenithRegenStateFile -RepoRoot $repo2 | Out-Null
        Set-Content -LiteralPath (Join-Path $repo2 'Games/Probe/New.cpp') -Value '// new'
        Invoke-Wp5Git -Dir $repo2 -GitArgs @('add', 'Games/Probe/New.cpp')
        $r2 = Test-ZenithRegenStale -RepoRoot $repo2
        Assert-True $r2.Stale "a staged-new (A) tracked source"
    }

    # ========================================================================
    Write-Host "`n[16] WP5: Test-ZenithRegisteredLane" -ForegroundColor Cyan

    function New-Wp5LaneFixture {
        # A main repo + a linked worktree (mirrors `zagent lane add`'s `git
        # worktree add --detach`), for Test-ZenithRegisteredLane.
        $main = Join-Path $script:TempRoot ("wp5_lane_main_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        New-Item -ItemType Directory -Path $main -Force | Out-Null
        & git -C $main init --quiet -b master 2>$null | Out-Null
        & git -C $main config user.email 'test@example.invalid' 2>$null | Out-Null
        & git -C $main config user.name 'test' 2>$null | Out-Null
        Set-Content -LiteralPath (Join-Path $main 'readme.txt') -Value 'x'
        & git -C $main add -A 2>$null | Out-Null
        & git -C $main commit -m init --quiet 2>$null | Out-Null
        $lane = Join-Path $script:TempRoot ("wp5_lane_wt_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        & git -C $main worktree add --detach $lane master 2>$null | Out-Null
        return [PSCustomObject]@{ Main = $main; Lane = $lane }
    }

    function Get-Wp5LaneVaultDir {
        param([string]$LaneRoot)
        $commonDir = (& git -C $LaneRoot rev-parse --git-common-dir 2>$null)
        if (-not [System.IO.Path]::IsPathRooted($commonDir)) { $commonDir = Join-Path $LaneRoot $commonDir }
        return (Join-Path $commonDir 'vault')
    }

    Invoke-Test "main checkout is never a lane" {
        $f = New-Wp5LaneFixture
        Assert-False (Test-ZenithRegisteredLane -RepoRoot $f.Main) "main checkout"
    }

    Invoke-Test "an unregistered linked worktree is not a lane (no lanes.json)" {
        $f = New-Wp5LaneFixture
        Assert-False (Test-ZenithRegisteredLane -RepoRoot $f.Lane) "no lanes.json at all"
    }

    Invoke-Test "a worktree registered in <git-common-dir>/vault/lanes.json IS a lane (case/slash-insensitive)" {
        $f = New-Wp5LaneFixture
        $vaultDir = Get-Wp5LaneVaultDir -LaneRoot $f.Lane
        New-Item -ItemType Directory -Force -Path $vaultDir | Out-Null
        # Deliberately different case AND separator than the real path -- the
        # match must still hold (regen.ps1/ZenithCli.psm1 build-slot messages
        # quote a Windows-style path; a lane could be registered from either
        # style).
        $laneUpperBackslash = $f.Lane.ToUpperInvariant().Replace('/', '\')
        $payload = @{ lanes = @{ ZM = @{ path = $laneUpperBackslash } } } | ConvertTo-Json -Depth 5
        Set-Content -LiteralPath (Join-Path $vaultDir 'lanes.json') -Value $payload
        Assert-True (Test-ZenithRegisteredLane -RepoRoot $f.Lane) "registered, case/slash-insensitively"
    }

    Invoke-Test "an invalid lanes.json is treated as 'not a lane', never throws" {
        $f = New-Wp5LaneFixture
        $vaultDir = Get-Wp5LaneVaultDir -LaneRoot $f.Lane
        New-Item -ItemType Directory -Force -Path $vaultDir | Out-Null
        Set-Content -LiteralPath (Join-Path $vaultDir 'lanes.json') -Value '{ not json'
        $threw = $false
        $result = $true
        try { $result = Test-ZenithRegisteredLane -RepoRoot $f.Lane } catch { $threw = $true }
        Assert-False $threw "must never throw"
        Assert-False $result "malformed json -> false"
    }

    Invoke-Test "a lanes.json with no matching path -> not a lane" {
        $f = New-Wp5LaneFixture
        $vaultDir = Get-Wp5LaneVaultDir -LaneRoot $f.Lane
        New-Item -ItemType Directory -Force -Path $vaultDir | Out-Null
        $payload = @{ lanes = @{ ZM = @{ path = 'C:/somewhere/else/entirely' } } } | ConvertTo-Json -Depth 5
        Set-Content -LiteralPath (Join-Path $vaultDir 'lanes.json') -Value $payload
        Assert-False (Test-ZenithRegisteredLane -RepoRoot $f.Lane) "registered lane names a DIFFERENT path"
    }

    Invoke-Test "regen.ps1's worktree-refusal condition: a registered lane needs no flag, an unregistered one is still refused" {
        # Pins the exact boolean combination regen.ps1 uses at its refusal
        # check (~line 43) and its ZENITH_ROOT post-check (~line 115), without
        # running regen.ps1 itself (which needs a real Sharpmake install).
        $f = New-Wp5LaneFixture
        $allowFlag = $false

        $refuseUnregistered = (Test-ZenithInWorktree -RepoRoot $f.Lane) -and (-not $allowFlag) -and (-not (Test-ZenithRegisteredLane -RepoRoot $f.Lane))
        Assert-True $refuseUnregistered "an unregistered linked worktree must still be refused with no flag"

        $vaultDir = Get-Wp5LaneVaultDir -LaneRoot $f.Lane
        New-Item -ItemType Directory -Force -Path $vaultDir | Out-Null
        $payload = @{ lanes = @{ ZM = @{ path = $f.Lane } } } | ConvertTo-Json -Depth 5
        Set-Content -LiteralPath (Join-Path $vaultDir 'lanes.json') -Value $payload

        $refuseRegistered = (Test-ZenithInWorktree -RepoRoot $f.Lane) -and (-not $allowFlag) -and (-not (Test-ZenithRegisteredLane -RepoRoot $f.Lane))
        Assert-False $refuseRegistered "a REGISTERED lane must pass with NO -AllowLinkedWorktree flag"

        # The ZENITH_ROOT post-check is keyed on Test-ZenithInWorktree alone
        # (not on -AllowLinkedWorktree), so it still applies to a lane even
        # though Invoke-Regen never passes the flag.
        Assert-True (Test-ZenithInWorktree -RepoRoot $f.Lane) "the post-check guard still covers a lane"
    }

    # ========================================================================
    # WP5 follow-up (post-review). Invoke-Regen (ZenithCli.psm1) runs
    # regen.ps1 in-process via `& $regen`, as a CHILD SCOPE of a module that
    # already has `Set-StrictMode -Version Latest` set -- strict mode is
    # inherited by child scopes, so regen.ps1 runs strict there even though it
    # sets nothing itself and is non-strict when run standalone
    # (`pwsh -File Build/regen.ps1`). A REAL `zenith build --headless` hit
    # this: step 7.6's `$prunedEngine = Remove-ZenithOrphanEngineArtifacts
    # ...` captured $null (that function ends `return $removed.ToArray()`,
    # unrolled by `return`, so an EMPTY result is zero pipeline objects and a
    # plain-assignment capture of zero objects is $null, not an empty array --
    # the same trap Get-ZenithUntrackedProjectShapingFiles's own header
    # comment documents), and `$prunedEngine.Count` under Version Latest
    # throws "The property 'Count' cannot be found on this object" rather
    # than evaluating falsy. regen.ps1 now wraps that capture (and the
    # analogous $sourceFiles one) in `@(...)`, which is null-safe regardless
    # of strict mode. This section pins the fix directly, WITHOUT running
    # Sharpmake.
    Write-Host "`n[17] WP5 follow-up: regen.ps1's post-Sharpmake tail is StrictMode-safe (Invoke-Regen hazard)" -ForegroundColor Cyan

    Invoke-Test "characterization: an UNWRAPPED capture of Remove-ZenithOrphanEngineArtifacts throws under Set-StrictMode -Version Latest when there is nothing to prune" {
        # Proves the mechanism this whole section guards against -- and that
        # this test would have caught the original bug. A fixture repo whose
        # Build/ dir has neither a Sharpmake_*.cs nor a stray .vcxproj is
        # exactly the "nothing to prune" case the real failure hit.
        $fakeRepo = Join-Path $script:TempRoot ("wp5_strict_prune_naive_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        New-Item -ItemType Directory -Force -Path (Join-Path $fakeRepo 'Build') | Out-Null

        $script:Wp5NaiveThrew = $false
        $script:Wp5NaiveMessage = ''
        & {
            Set-StrictMode -Version Latest
            try {
                $naive = Remove-ZenithOrphanEngineArtifacts -RepoRoot $fakeRepo
                [void]$naive.Count
            }
            catch {
                $script:Wp5NaiveThrew = $true
                $script:Wp5NaiveMessage = $_.Exception.Message
            }
        }
        Assert-True $script:Wp5NaiveThrew "an unwrapped plain-assignment .Count must throw under strict mode with nothing to prune (got no throw -- the mechanism this section relies on may have changed)"
        Assert-AnyLike @($script:Wp5NaiveMessage) "*Count*" "the strict-mode error names the missing property: $script:Wp5NaiveMessage"
    }

    Invoke-Test "the fix: @(Remove-ZenithOrphanEngineArtifacts ...) survives Set-StrictMode -Version Latest, Count is 0" {
        $fakeRepo = Join-Path $script:TempRoot ("wp5_strict_prune_fixed_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        New-Item -ItemType Directory -Force -Path (Join-Path $fakeRepo 'Build') | Out-Null

        $script:Wp5FixedThrew = $false
        $script:Wp5FixedCount = -1
        & {
            Set-StrictMode -Version Latest
            try {
                # The exact expression regen.ps1 now uses at its step 7.6.
                $prunedEngine = @(Remove-ZenithOrphanEngineArtifacts -RepoRoot $fakeRepo)
                $script:Wp5FixedCount = $prunedEngine.Count
            }
            catch {
                $script:Wp5FixedThrew = $true
            }
        }
        Assert-False $script:Wp5FixedThrew "regen.ps1's own @(...)-wrapped pattern must not throw under strict mode"
        Assert-Equal 0 $script:Wp5FixedCount "nothing to prune -> Count 0, never null"
    }

    Invoke-Test "the fix also covers Get-ChildItem | Sort-Object (regen.ps1's `$sourceFiles): @(...) survives strict mode on an empty match" {
        # Mirrors regen.ps1's `$sourceFiles = @(Get-ChildItem ... -Filter
        # 'Sharpmake_*.cs' | Sort-Object Name)` -- Sort-Object's pipeline also
        # collapses to $null (not an empty array) on zero input, the same
        # trap, different source.
        $emptyDir = Join-Path $script:TempRoot ("wp5_strict_sourcefiles_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
        New-Item -ItemType Directory -Force -Path $emptyDir | Out-Null

        $script:Wp5SourceFilesThrew = $false
        $script:Wp5SourceFilesCount = -1
        & {
            Set-StrictMode -Version Latest
            try {
                $sourceFiles = @(Get-ChildItem -LiteralPath $emptyDir -Filter 'Sharpmake_*.cs' -File | Sort-Object Name)
                $script:Wp5SourceFilesCount = $sourceFiles.Count
            }
            catch {
                $script:Wp5SourceFilesThrew = $true
            }
        }
        Assert-False $script:Wp5SourceFilesThrew "the @(...)-wrapped Sort-Object pipeline must not throw under strict mode"
        Assert-Equal 0 $script:Wp5SourceFilesCount "no Sharpmake_*.cs in an empty dir -> Count 0, never null"
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
