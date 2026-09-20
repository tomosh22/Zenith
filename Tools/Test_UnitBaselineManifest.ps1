# Test_UnitBaselineManifest.ps1 -- asserts the units-at-boot pins have exactly
# ONE home and that every consumer resolves through it.
#
# WHY THIS EXISTS. The pin used to be duplicated across
# .github/workflows/zm-tests.yml, zagent.project.json, Tools/run_unit_gate.ps1's
# -Baseline default and Games/*/Docs/Status.md. Two of those are protectedPaths,
# so bumping a pin was work the agent loop could never do: adding one test reds a
# REQUIRED check with ZERO failing tests, and the fix needed files `zagent guard`
# refuses. Every test-adding ticket was therefore unreachable, in a repo whose
# conventions ask for a test with all new code.
#
# The failure mode this guards against is silent in the worst way: a duplicate
# creeping back in does not break anything until the two copies disagree, and by
# then the symptom is a red required check on a tree where every test passes.
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:count = 0
$script:failures = 0
function Assert-That {
    param([string]$Name, [scriptblock]$Body)
    $script:count++
    try {
        if (& $Body) { Write-Host ("  ok   {0}" -f $Name) -ForegroundColor Green }
        else { Write-Host ("  FAIL {0}" -f $Name) -ForegroundColor Red; $script:failures++ }
    } catch {
        Write-Host ("  FAIL {0} -- threw: {1}" -f $Name, $_.Exception.Message) -ForegroundColor Red
        $script:failures++
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $PSScriptRoot 'unit_baselines.json'
$gatePath = Join-Path $PSScriptRoot 'run_unit_gate.ps1'

Write-Host "`n=== the manifest itself ===" -ForegroundColor Cyan

Assert-That 'unit_baselines.json exists and is valid JSON' {
    (Test-Path -LiteralPath $manifestPath) -and
    ($null -ne (Get-Content -LiteralPath $manifestPath -Raw -Encoding utf8 | ConvertFrom-Json))
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding utf8 | ConvertFrom-Json
$games = @($manifest.baselines.PSObject.Properties |
    Where-Object { -not $_.Name.StartsWith('$') })

Assert-That 'every baseline is a positive whole number' {
    # NOT `-isnot [int]`: ConvertFrom-Json yields System.Int64, so an Int32
    # test rejects every well-formed row. Check the VALUE, not the .NET type.
    ($games.Count -gt 0) -and
    -not (@($games | Where-Object {
        $v = $_.Value
        ($v -isnot [ValueType]) -or ($v -ne [math]::Floor([double]$v)) -or ($v -le 0)
    }).Count)
}

Assert-That 'the games the gates actually run are all present' {
    $names = @($games | ForEach-Object { $_.Name })
    ('Zenithmon' -in $names) -and ('Combat' -in $names)
}

Write-Host "`n=== nothing hardcodes a number any more ===" -ForegroundColor Cyan

# The point of the refactor. A literal -Baseline <N> anywhere in an executable
# consumer means that consumer has its own copy again, and the two only diverge
# once -- silently, at the next bump.
$consumers = @(
    (Join-Path $repoRoot '.github/workflows/zm-tests.yml'),
    (Join-Path $repoRoot '.github/workflows/engine-gate.yml'),
    (Join-Path $repoRoot 'zagent.project.json'),
    (Join-Path $PSScriptRoot 'test_scaffold.ps1')
) | Where-Object { Test-Path -LiteralPath $_ }

foreach ($file in $consumers) {
    $name = Split-Path -Leaf $file
    Assert-That "$name passes no literal -Baseline <N>" {
        $text = Get-Content -LiteralPath $file -Raw -Encoding utf8
        -not ([regex]::IsMatch($text, '-Baseline\s+\d+'))
    }
}

Assert-That 'run_unit_gate.ps1 no longer carries a hardcoded default baseline' {
    $text = Get-Content -LiteralPath $gatePath -Raw -Encoding utf8
    # 0 means "not supplied, resolve from the manifest". Any other literal is a
    # default that would silently apply to a game with no manifest row.
    [regex]::IsMatch($text, '\[int\]\$Baseline\s*=\s*0\b')
}

Write-Host "`n=== every consumer names a game the manifest knows ===" -ForegroundColor Cyan

# A -Game only belongs to this manifest when it is passed to run_unit_gate.ps1.
# Scanning a consumer as raw TEXT does not establish that, and the difference is
# not academic: zagent.project.json's own prose carries the words
# "named by some workflow's -Game or declared advisory", so a bare
# '-Game\s+(\w+)' scan read "or" as a game name and this assertion was RED on
# master with nothing wrong anywhere in the repo. The doc_lint.ps1 exception it
# used to carry was the same defect patched once by hand -- doc_lint takes a
# -Game and is not a baseline consumer either. Anchoring on the gate script
# covers both, and needs no exception list for the next -Game-taking tool to be
# forgotten from. The ungated-row assertion below already anchors this way.
function Get-GateGameNames {
    param([string]$Text)
    # Join PowerShell backtick continuations FIRST: test_scaffold.ps1 and
    # engine-gate.yml both put -Exe/-Game on the line AFTER the script name, so a
    # strictly per-physical-line scan would find no consumer at all and this
    # assertion would pass vacuously -- the exact failure mode this file exists
    # to prevent.
    $logical = [regex]::Replace($Text, '`[ \t]*\r?\n\s*', ' ')
    $names = @()
    foreach ($line in [regex]::Split($logical, '\r?\n')) {
        if ($line -notmatch 'run_unit_gate\.ps1') { continue }
        foreach ($m in [regex]::Matches($line, '-Game\s+([A-Za-z0-9_]+)')) {
            $names += $m.Groups[1].Value
        }
    }
    return ,$names
}

$script:seenGateGames = @()
foreach ($file in $consumers) {
    $script:seenGateGames += Get-GateGameNames -Text (Get-Content -LiteralPath $file -Raw -Encoding utf8)
}

# The sensitivity check for the assertion below, and it is not optional. "No
# unknown game was found" is also exactly what a scanner matching NOTHING
# reports, so a regex that silently stopped matching would read as green. The
# two have to be asserted together.
Assert-That 'the consumer scan actually finds the gate invocations' {
    $seen = @($script:seenGateGames)
    if ($seen.Count) { Write-Host ("       gate -Game: {0}" -f (($seen | Sort-Object -Unique) -join ', ')) -ForegroundColor DarkGray }
    ('Zenithmon' -in $seen) -and ('Combat' -in $seen)
}

Assert-That 'every -Game passed by a consumer has a manifest row' {
    $known = @($games | ForEach-Object { $_.Name })
    $missing = @()
    foreach ($file in $consumers) {
        $leaf = Split-Path -Leaf $file
        foreach ($g in (Get-GateGameNames -Text (Get-Content -LiteralPath $file -Raw -Encoding utf8))) {
            if ($g -notin $known) { $missing += "${leaf}:$g" }
        }
    }
    if ($missing.Count) { Write-Host ("       unknown: {0}" -f ($missing -join ', ')) -ForegroundColor Yellow }
    $missing.Count -eq 0
}

Write-Host "`n=== no row is ungated by accident ===" -ForegroundColor Cyan

# The manifest exists so a pin cannot drift. A row NO workflow reads defeats
# that in the one place nobody looks: it cannot red, so it rots until somebody
# runs it by hand and gets a failure that has nothing to do with their change.
# RenderTest was added in exactly that state. The rule is not "every row must be
# gated" -- gating one can cost a whole extra CI build -- it is that an ungated
# row must be DECLARED, so the debt is visible and deliberate.
Assert-That 'every baseline row is gated by a workflow, or declared advisory with a reason' {
    $workflowDir = Join-Path $repoRoot '.github/workflows'
    $gated = @()
    if (Test-Path -LiteralPath $workflowDir) {
        foreach ($wf in Get-ChildItem $workflowDir -Filter '*.yml' -ErrorAction SilentlyContinue) {
            $text = Get-Content -LiteralPath $wf.FullName -Raw -Encoding utf8
            foreach ($m in [regex]::Matches($text, 'run_unit_gate\.ps1[^\r\n]*?-Game\s+([A-Za-z0-9_]+)')) {
                $gated += $m.Groups[1].Value
            }
            # A call with -Exe but no -Game derives the game from the path.
            foreach ($m in [regex]::Matches($text, 'run_unit_gate\.ps1[\s\S]{0,400}?-Exe\s+''?[^\r\n'']*?Games[/\\]([A-Za-z0-9_]+)[/\\]')) {
                $gated += $m.Groups[1].Value
            }
        }
    }
    $advisory = @()
    if ($manifest.PSObject.Properties['advisory']) {
        $advisory = @($manifest.advisory.PSObject.Properties |
            Where-Object { -not $_.Name.StartsWith('$') -and $_.Value -is [string] -and $_.Value.Trim().Length -gt 40 } |
            ForEach-Object { $_.Name })
    }
    $orphans = @($games | ForEach-Object { $_.Name } |
        Where-Object { $_ -notin $gated -and $_ -notin $advisory })
    if ($orphans.Count) {
        Write-Host ("       ungated and undeclared: {0}" -f ($orphans -join ', ')) -ForegroundColor Yellow
        # Single quotes: a backtick is PowerShell's escape character, so "`advisory`"
        # in a double-quoted string emits a BEL and prints "dvisory".
        Write-Host  '       -> gate it in a workflow, or add an "advisory" entry saying why not' -ForegroundColor Yellow
    }
    $orphans.Count -eq 0
}

Assert-That 'no advisory entry names a row that does not exist' {
    # A stale advisory entry is the same hazard one layer up: it excuses a row
    # nobody has, and hides the next real orphan behind a name that looks handled.
    if (-not $manifest.PSObject.Properties['advisory']) { return $true }
    $names = @($games | ForEach-Object { $_.Name })
    $declared = @($manifest.advisory.PSObject.Properties |
        Where-Object { -not $_.Name.StartsWith('$') } | ForEach-Object { $_.Name })
    -not (@($declared | Where-Object { $_ -notin $names }).Count)
}

Assert-That 'test_scaffold.ps1 pins to Combat, not to the scaffolded game name' {
    # A freshly scaffolded game has no game-specific tests, so it boots exactly
    # the engine suite. Deriving from its own name would find no row and the
    # gate would hard-error on every scaffold smoke run.
    $text = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'test_scaffold.ps1') -Raw -Encoding utf8
    $text -match 'run_unit_gate\.ps1'  -and $text -match '-Game\s+Combat'
}

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
