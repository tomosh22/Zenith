#Requires -Version 7.0
<#
  Test-ZagentClient.ps1 — unit tests for the `zagent` client helpers.

  A plain test script rather than Pester, matching
  Tools/Test_T0Harness_SessionCloseFlagsExist.ps1: the only PowerShell
  this repo requires is the pwsh every gate already needs, and adding a
  module dependency to run four hundred lines of tests is a worse trade
  than a hundred lines of asserts.

  Nothing here touches the network. The transport and the board are
  covered by the TypeScript suites and by a live round trip; what these
  guard is the part that lives on this side of the wire, including two
  bugs that were found the hard way:

    * `Remove-Annotations` returning `,@(…)` — a PowerShell function
      UNROLLS a returned collection, so a one-element array becomes a
      scalar and `cleanup: ["…"]` goes on the wire as a bare string.
    * `ConvertTo-PosixPath` — a separator mismatch makes an uploaded docs
      tree match nothing, which mirrors zero pages and reports success.

  Usage:
    pwsh -NoProfile -File Tools/zagent/Test-ZagentClient.ps1

  Exit codes:
    0 -- every assertion passed.
    1 -- one or more failed (each is printed with its expectation).
#>

[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:failures = 0
$script:count = 0

function Assert-That {
    param([string]$Name, [scriptblock]$Body)
    $script:count++
    try {
        $ok = & $Body
        if ($ok) {
            Write-Host ("  ok   {0}" -f $Name) -ForegroundColor Green
        } else {
            Write-Host ("  FAIL {0}" -f $Name) -ForegroundColor Red
            $script:failures++
        }
    } catch {
        Write-Host ("  FAIL {0} -- threw: {1}" -f $Name, $_.Exception.Message) -ForegroundColor Red
        $script:failures++
    }
}

function New-TempDir {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("zagent-test-" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    return $dir
}

# ── The module under test, plus a parse-check of the script that uses it.
$here = $PSScriptRoot
Import-Module (Join-Path $here 'ZagentClient.psm1') -Force

Write-Host "`n=== both files parse ===" -ForegroundColor Cyan
foreach ($file in @('zagent.ps1', 'ZagentClient.psm1')) {
    Assert-That "$file parses cleanly" {
        $errors = $null
        [void][System.Management.Automation.Language.Parser]::ParseFile(
            (Join-Path $here $file), [ref]$null, [ref]$errors)
        # A parse error here means every gate stalls on a broken client,
        # and the failure would first appear as a permission prompt.
        $null -eq $errors -or $errors.Count -eq 0
    }
}

Write-Host "`n=== ConvertTo-PosixPath ===" -ForegroundColor Cyan
Assert-That 'converts backslashes' { (ConvertTo-PosixPath 'C:\dev\Zenith\Docs') -eq 'C:/dev/Zenith/Docs' }
Assert-That 'leaves forward slashes alone' { (ConvertTo-PosixPath 'C:/dev/Zenith') -eq 'C:/dev/Zenith' }
Assert-That 'trims a trailing slash' { (ConvertTo-PosixPath 'C:/dev/Zenith/') -eq 'C:/dev/Zenith' }
Assert-That 'trims repeated trailing slashes' { (ConvertTo-PosixPath 'C:/dev/Zenith///') -eq 'C:/dev/Zenith' }
Assert-That 'is idempotent' {
    # The board keys its uploaded docs tree on these strings; a value that
    # normalises differently on a second pass would match nothing.
    $once = ConvertTo-PosixPath 'C:\dev\Zenith\'
    (ConvertTo-PosixPath $once) -eq $once
}

Write-Host "`n=== Remove-Annotations ===" -ForegroundColor Cyan
Assert-That 'strips a top-level $comment' {
    $o = Remove-Annotations ([pscustomobject]@{ '$comment' = 'note'; project = 'ZEN' })
    ($null -eq $o.PSObject.Properties['$comment']) -and $o.project -eq 'ZEN'
}
Assert-That 'strips $comment at depth' {
    $input = [pscustomobject]@{ categories = [pscustomobject]@{
        Engine = [pscustomobject]@{ '$comment' = 'x'; gates = @('a') } } }
    $o = Remove-Annotations $input
    $null -eq $o.categories.Engine.PSObject.Properties['$comment']
}
Assert-That 'KEEPS a ONE-element array as an array' {
    # The bug: a PowerShell function unrolls a returned collection, so
    # `,@(…)` is load-bearing. Without it this serialises as a bare
    # string and the board rejects the whole file with
    # "Expected array, received string".
    $o = Remove-Annotations ([pscustomobject]@{ cleanup = @('one command') })
    $o.cleanup -is [System.Collections.IList]
}
Assert-That 'a one-element array survives ConvertTo-Json as an array' {
    # The property that actually matters, asserted on the wire format.
    $o = Remove-Annotations ([pscustomobject]@{ cleanup = @('one command') })
    ($o | ConvertTo-Json -Depth 10 -Compress) -match '"cleanup":\['
}
Assert-That 'keeps a multi-element array an array' {
    $o = Remove-Annotations ([pscustomobject]@{ gates = @('a', 'b') })
    $o.gates.Count -eq 2
}
Assert-That 'keeps an EMPTY array an array' {
    $o = Remove-Annotations ([pscustomobject]@{ gates = @() })
    $o.gates -is [System.Collections.IList]
}
Assert-That 'does not touch a $ inside a VALUE' {
    # `$env:X` appears in real gate lines; only KEYS are comments.
    $o = Remove-Annotations ([pscustomobject]@{ gates = @('pwsh -c "$env:X=1"') })
    $o.gates[0] -eq 'pwsh -c "$env:X=1"'
}
Assert-That 'passes scalars through' { (Remove-Annotations 'plain') -eq 'plain' }
Assert-That 'passes $null through' { $null -eq (Remove-Annotations $null) }

Write-Host "`n=== Find-ClientRepo ===" -ForegroundColor Cyan
$repo = New-TempDir
New-Item -ItemType Directory -Path (Join-Path $repo 'Games\Zenithmon') -Force | Out-Null
'{ "project": "ZEN", "baseBranch": "master" }' | Set-Content (Join-Path $repo 'zagent.project.json') -Encoding utf8

Assert-That 'finds the file in the repo root' { (Find-ClientRepo $repo) -eq $repo }
Assert-That 'walks UP from a subdirectory' {
    # `zagent owns` from Games/Zenithmon must read the same gates as one
    # run from the root.
    (Find-ClientRepo (Join-Path $repo 'Games\Zenithmon')) -eq $repo
}
Assert-That 'returns null outside any client repo' {
    # Not an error: `zagent auth mint` on a fresh machine has no project
    # file yet, and refusing would make bootstrapping impossible.
    $null -eq (Find-ClientRepo (New-TempDir))
}

Write-Host "`n=== Get-ClientProject ===" -ForegroundColor Cyan
Assert-That 'reports the repo in POSIX form' {
    # The board keys the uploaded docs tree off this.
    (Get-ClientProject -Repo $repo).repo -eq (ConvertTo-PosixPath $repo)
}
Assert-That 'strips annotations before the board ever sees them' {
    $annotated = New-TempDir
    '{ "$comment": "note", "project": "ZEN", "baseBranch": "master" }' |
        Set-Content (Join-Path $annotated 'zagent.project.json') -Encoding utf8
    $null -eq (Get-ClientProject -Repo $annotated).file.PSObject.Properties['$comment']
}
Assert-That 'returns null when there is no repo' { $null -eq (Get-ClientProject -Repo $null) }

Write-Host "`n=== argument inspection ===" -ForegroundColor Cyan
Assert-That 'reads --flag value' { (Get-FlagValue -Argv @('show', '--project', 'ZEN') -Name 'project') -eq 'ZEN' }
Assert-That 'reads --flag=value' { (Get-FlagValue -Argv @('show', '--project=ZEN') -Name 'project') -eq 'ZEN' }
Assert-That 'is null for an absent flag' { $null -eq (Get-FlagValue -Argv @('show') -Name 'project') }
Assert-That 'is null for a flag with no value' { $null -eq (Get-FlagValue -Argv @('show', '--project') -Name 'project') }
Assert-That 'detects a boolean flag' { Test-Flag -Argv @('next', '--json') -Name 'json' }
Assert-That 'does not confuse a value for a flag' { -not (Test-Flag -Argv @('comment', '--text', '--json') -Name 'jsonx') }

Write-Host "`n=== Get-ScratchRoot ===" -ForegroundColor Cyan
Assert-That 'is .zagent/run under the client repo' {
    # It moved here from the board repo when the two stopped having to be
    # the same machine. `.zagent/` is gitignored so the tick's dirty-tree
    # precondition still sees a clean tree.
    (Get-ScratchRoot 'C:\x') -eq (Join-Path (Join-Path 'C:\x' '.zagent') 'run')
}

Write-Host "`n=== Write-Results ===" -ForegroundColor Cyan
$target = New-TempDir
$result = [pscustomobject]@{ writes = @(
    [pscustomobject]@{ base = 'scratch'; path = 'ZEN-1/body.md'; content = "# body`n" }
    [pscustomobject]@{ base = 'repo'; path = 'Docs/DecisionLog.md'; content = "# log`n" }
) }
Write-Results -Result $result -Repo $target

Assert-That 'writes a scratch entry under .zagent/run' {
    Test-Path (Join-Path (Get-ScratchRoot $target) 'ZEN-1\body.md')
}
Assert-That 'writes a repo entry relative to the repo root' {
    Test-Path (Join-Path $target 'Docs\DecisionLog.md')
}
Assert-That 'creates intermediate directories' {
    Test-Path (Join-Path $target 'Docs')
}
Assert-That 'writes the content verbatim' {
    (Get-Content (Join-Path (Get-ScratchRoot $target) 'ZEN-1\body.md') -Raw) -eq "# body`n"
}
Assert-That 'writes NO byte-order mark' {
    # These files are read by git, by the tick, and by a worker's Read
    # tool. A BOM shows up as a diff in all three.
    $bytes = [System.IO.File]::ReadAllBytes((Join-Path $target 'Docs\DecisionLog.md'))
    -not ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
}
Assert-That 'tolerates a result with no writes' {
    Write-Results -Result ([pscustomobject]@{ exitCode = 0 }) -Repo $target
    $true
}

Write-Host "`n=== Get-DocsTree ===" -ForegroundColor Cyan
$docsRepo = New-TempDir
New-Item -ItemType Directory -Path (Join-Path $docsRepo 'Docs\Sub') -Force | Out-Null
'# Status' | Set-Content (Join-Path $docsRepo 'Docs\Status.md') -Encoding utf8
'# Nested' | Set-Content (Join-Path $docsRepo 'Docs\Sub\Nested.md') -Encoding utf8
'not markdown' | Set-Content (Join-Path $docsRepo 'Docs\notes.txt') -Encoding utf8
$docsClient = [pscustomobject]@{
    repo = ConvertTo-PosixPath $docsRepo
    file = [pscustomobject]@{ categories = [pscustomobject]@{
        Fixture = [pscustomobject]@{ docs = [pscustomobject]@{ dir = 'Docs' } } } }
}
$tree = Get-DocsTree -Client $docsClient

Assert-That 'collects markdown recursively' { $tree.PSObject.Properties.Name.Count -eq 2 }
Assert-That 'excludes non-markdown' {
    -not ($tree.PSObject.Properties.Name | Where-Object { $_ -like '*notes.txt' })
}
Assert-That 'keys on POSIX absolute paths' {
    # The board derives its section root the same way. A backslash here
    # matches nothing, and a sync that matches nothing mirrors zero pages
    # and reports success.
    $tree.PSObject.Properties.Name | ForEach-Object { if ($_ -match '\\') { return $false } }
    ($tree.PSObject.Properties.Name | Where-Object { $_ -notmatch '\\' }).Count -eq 2
}
Assert-That 'keys are rooted at the reported repo path' {
    ($tree.PSObject.Properties.Name | Where-Object { $_.StartsWith($docsClient.repo) }).Count -eq 2
}
Assert-That 'is null when no category configures docs' {
    $null -eq (Get-DocsTree -Client ([pscustomobject]@{
        repo = 'C:/x'; file = [pscustomobject]@{ gates = @('a') } }))
}
Assert-That 'is null with no client at all' { $null -eq (Get-DocsTree -Client $null) }

Write-Host "`n=== Get-AllGateLines ===" -ForegroundColor Cyan
$gateClient = [pscustomobject]@{
    repo = 'C:/x'
    file = [pscustomobject]@{
        gates = @('pwsh -a')
        cleanup = @('pwsh -c cleanup')
        categories = [pscustomobject]@{ A = [pscustomobject]@{ gates = @('pwsh -b') } }
    }
}
Assert-That 'gathers project, cleanup and category lines' { (Get-AllGateLines $gateClient).Count -eq 3 }
Assert-That 'tolerates a file with no gates at all' {
    (Get-AllGateLines ([pscustomobject]@{ file = [pscustomobject]@{ project = 'ZEN' } })).Count -eq 0
}

Write-Host "`n=== Get-ClientChecks ===" -ForegroundColor Cyan
Assert-That 'returns an ARRAY even for a single check' {
    # `.Count` on a bare hashtable answers with its key count, so a
    # caller counting checks would silently read 3.
    (Get-ClientChecks -Client $null) -is [System.Collections.IList]
}
Assert-That 'reports the missing project file when there is no repo' {
    $checks = Get-ClientChecks -Client $null
    $checks.Count -eq 1 -and -not $checks[0].ok -and $checks[0].detail -match 'zagent\.project\.json'
}
Assert-That 'resolves ONE row per distinct gate executable' {
    # A dozen identical "pwsh resolves" rows buries the one that failed.
    # `@(...)` around the filter: a Where-Object that matches ONE
    # hashtable hands back the hashtable itself, and `.Count` on one of
    # those answers with its KEY count. The same unrolling trap the code
    # under test has to defend against.
    $checks = Get-ClientChecks -Client $gateClient
    @($checks | Where-Object { $_.name -like 'gate executable*' }).Count -eq 1
}
Assert-That 'flags an executable that is not on PATH' {
    $missing = [pscustomobject]@{ repo = 'C:/x'; file = [pscustomobject]@{
        baseBranch = 'master'; gates = @('definitely-not-a-real-binary x') } }
    # ASSIGN, do not pipe: the function returns one pipeline item on
    # purpose (see its comment), so `| Where-Object` would filter the
    # array itself rather than its rows.
    $checks = Get-ClientChecks -Client $missing
    $row = @($checks | Where-Object { $_.name -like '*definitely-not*' })[0]
    -not $row.ok -and $row.detail -match 'would stall'
}

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
