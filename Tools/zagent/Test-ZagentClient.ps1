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

Write-Host "`n=== Test-NeedsDocsTree ===" -ForegroundColor Cyan
# This predicate is spelled TWICE — here and as `needsDocsTree` in the
# board's packages/agent/src/dispatch.ts. The cases below are a
# deliberate COPY of that file's `describe('needsDocsTree')`, case for
# case, so a rule that gains a command on one side and not the other
# fails on THIS side rather than showing up as the board answering
# "no Roadmap.md in the uploaded tree" — which reads like a client
# mistake rather than two copies of a rule disagreeing.
Assert-That 'covers the docs mirror' {
    (Test-NeedsDocsTree -Argv @('docs', 'sync')) -and
    (Test-NeedsDocsTree -Argv @('docs', 'status'))
}
Assert-That 'covers the doc-to-board drift check, which reads the SAME upload' {
    Test-NeedsDocsTree -Argv @('board', 'status')
}
Assert-That 'leaves every other command alone' {
    (-not (Test-NeedsDocsTree -Argv @('docs', 'read', 'Zenithmon/Status'))) -and
    (-not (Test-NeedsDocsTree -Argv @('queue'))) -and
    (-not (Test-NeedsDocsTree -Argv @('board'))) -and
    (-not (Test-NeedsDocsTree -Argv @()))
}
# `$argv[0]` on an empty array is a Set-StrictMode violation, and this
# script sets it — a bare index would throw rather than return false.
Assert-That 'survives a null argv under Set-StrictMode' {
    (-not (Test-NeedsDocsTree -Argv $null))
}
# A subcommand is required: `board` alone must not ship the tree, and
# neither must a command that merely STARTS with one of these words.
Assert-That 'needs the subcommand, not just the verb' {
    (-not (Test-NeedsDocsTree -Argv @('docs'))) -and
    (-not (Test-NeedsDocsTree -Argv @('boards', 'status'))) -and
    (-not (Test-NeedsDocsTree -Argv @('board', 'statuses')))
}
# Extra arguments are normal — `board status --project ZM` is the form
# the docs actually tell people to type.
Assert-That 'ignores trailing flags' {
    (Test-NeedsDocsTree -Argv @('board', 'status', '--project', 'ZM')) -and
    (Test-NeedsDocsTree -Argv @('docs', 'sync', '--project', 'ZEN'))
}
# Returns a real boolean, not a truthy string or a one-element array —
# the `,@(…)` unrolling hazard two functions up, from the other side.
Assert-That 'returns a boolean, not a collection' {
    ((Test-NeedsDocsTree -Argv @('board', 'status')) -is [bool]) -and
    ((Test-NeedsDocsTree -Argv @('queue')) -is [bool])
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

Write-Host "`n=== Get-ConventionsTree ===" -ForegroundColor Cyan
$convRepo = New-TempDir
New-Item -ItemType Directory -Path (Join-Path $convRepo 'Flux') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $convRepo 'node_modules\pkg') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $convRepo 'Games\Zenithmon\Build\output') -Force | Out-Null
'# root' | Set-Content (Join-Path $convRepo 'CLAUDE.md') -Encoding utf8
'# flux' | Set-Content (Join-Path $convRepo 'Flux\CLAUDE.md') -Encoding utf8
'# onboarding, not a convention file' | Set-Content (Join-Path $convRepo 'Flux\Onboarding.md') -Encoding utf8
'must never be read' | Set-Content (Join-Path $convRepo 'node_modules\pkg\CLAUDE.md') -Encoding utf8
'must never be read' | Set-Content (Join-Path $convRepo 'Games\Zenithmon\Build\output\CLAUDE.md') -Encoding utf8

$convClient = [pscustomobject]@{
    repo = ConvertTo-PosixPath $convRepo
    file = [pscustomobject]@{ conventionDocs = [pscustomobject]@{ title = 'Conventions' } }
}
$convTree = Get-ConventionsTree -Client $convClient

Assert-That 'finds CLAUDE.md at the repo root and nested' { $convTree.Count -eq 2 }
Assert-That 'excludes a same-named non-CLAUDE.md file' {
    -not ($convTree.Keys | Where-Object { $_ -like '*Onboarding.md' })
}
Assert-That 'never descends into the built-in blocklist' {
    -not ($convTree.Keys | Where-Object { $_ -like '*node_modules*' -or $_ -like '*Build*output*' })
}
Assert-That 'keys on POSIX absolute paths rooted at the repo' {
    ($convTree.Keys | Where-Object { $_.StartsWith($convClient.repo) -and $_ -notmatch '\\' }).Count -eq 2
}
Assert-That 'a project exclude prunes on top of the built-in list' {
    New-Item -ItemType Directory -Path (Join-Path $convRepo 'Skip') -Force | Out-Null
    'must never be read' | Set-Content (Join-Path $convRepo 'Skip\CLAUDE.md') -Encoding utf8
    # Unexcluded, `Skip` is picked up like any other directory.
    (Get-ConventionsTree -Client $convClient).Count -eq 3 -and
    (Get-ConventionsTree -Client ([pscustomobject]@{
        repo = $convClient.repo
        file = [pscustomobject]@{ conventionDocs = [pscustomobject]@{ exclude = @('Skip') } }
    })).Count -eq 2
}
Assert-That 'is an empty (not null) tree when conventionDocs is absent' {
    $tree = Get-ConventionsTree -Client ([pscustomobject]@{
        repo = 'C:/x'; file = [pscustomobject]@{ gates = @('a') } })
    $null -ne $tree -and $tree.Count -eq 0
}
Assert-That 'is an empty tree with no client at all' {
    (Get-ConventionsTree -Client $null).Count -eq 0
}
Assert-That 'Get-DocsTree merges category docs and conventions in one tree' {
    $merged = [pscustomobject]@{
        repo = $docsClient.repo
        file = [pscustomobject]@{
            categories = $docsClient.file.categories
            conventionDocs = [pscustomobject]@{}
        }
    }
    New-Item -ItemType Directory -Path $docsRepo -Force | Out-Null
    '# root' | Set-Content (Join-Path $docsRepo 'CLAUDE.md') -Encoding utf8
    $combined = Get-DocsTree -Client $merged
    # 2 markdown docs + 1 root CLAUDE.md
    $combined.PSObject.Properties.Name.Count -eq 3
}

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

Write-Host "`n=== Get-WorkingTreeChanges ===" -ForegroundColor Cyan

# The guard's input used to be hand-assembled in /tick's prose, and both
# spellings shipped broken: a commit range that is empty at guard time
# (every ticket Blocked), and `diff --name-only`, which cannot see a
# CREATED file (a new .claude/** path evaded the protected-path check).
function New-GitRepo {
    $dir = New-TempDir
    & git -C $dir init --quiet 2>$null | Out-Null
    & git -C $dir config user.email 't@t.invalid' 2>$null | Out-Null
    & git -C $dir config user.name 'T' 2>$null | Out-Null
    return $dir
}

Assert-That 'sees a file the worker CREATED (the case diff --name-only misses)' {
    $repo = New-GitRepo
    Set-Content (Join-Path $repo 'tracked.txt') 'x'
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm init 2>$null | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $repo '.claude') -Force | Out-Null
    Set-Content (Join-Path $repo '.claude/evil.md') 'new file under a protected path'
    $changed = Get-WorkingTreeChanges -Repo $repo
    $changed -contains '.claude/evil.md'
}

Assert-That 'sees a modification to a tracked file' {
    $repo = New-GitRepo
    Set-Content (Join-Path $repo 'a.txt') 'one'
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm init 2>$null | Out-Null
    Set-Content (Join-Path $repo 'a.txt') 'two'
    (Get-WorkingTreeChanges -Repo $repo) -contains 'a.txt'
}

Assert-That 'returns BOTH sides of a rename' {
    $repo = New-GitRepo
    Set-Content (Join-Path $repo 'old.txt') 'content that stays identical so git calls it a rename'
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm init 2>$null | Out-Null
    & git -C $repo mv old.txt new.txt 2>$null | Out-Null
    $changed = Get-WorkingTreeChanges -Repo $repo
    # Moving a file OUT of a protected directory still touches it, so a
    # rename that reported only its destination would be a hole.
    ($changed -contains 'old.txt') -and ($changed -contains 'new.txt')
}

Assert-That 'is an EMPTY ARRAY on a clean tree, not null and not a string' {
    $repo = New-GitRepo
    Set-Content (Join-Path $repo 'a.txt') 'x'
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm init 2>$null | Out-Null
    $changed = Get-WorkingTreeChanges -Repo $repo
    ($null -ne $changed) -and ($changed.Count -eq 0)
}

Assert-That 'returns an ARRAY for a single change, not an unrolled string' {
    $repo = New-GitRepo
    Set-Content (Join-Path $repo 'a.txt') 'x'
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm init 2>$null | Out-Null
    Set-Content (Join-Path $repo 'a.txt') 'y'
    $changed = Get-WorkingTreeChanges -Repo $repo
    # `,@(…)` on return, or one element unrolls to a bare string and
    # `.Count` answers with the string's LENGTH.
    ($changed -is [array]) -and ($changed.Count -eq 1)
}

Assert-That 'unquotes a path containing a space' {
    $repo = New-GitRepo
    Set-Content (Join-Path $repo 'a.txt') 'x'
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm init 2>$null | Out-Null
    Set-Content (Join-Path $repo 'has space.txt') 'y'
    $changed = Get-WorkingTreeChanges -Repo $repo
    # git wraps such a path in quotes in --porcelain output; a guard
    # comparing '"has space.txt"' against a glob would never match.
    $changed -contains 'has space.txt'
}

Assert-That 'expands a wholly-new directory into its files (-uall)' {
    $repo = New-GitRepo
    Set-Content (Join-Path $repo 'a.txt') 'x'
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm init 2>$null | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $repo 'brand/new') -Force | Out-Null
    Set-Content (Join-Path $repo 'brand/new/one.md') '1'
    Set-Content (Join-Path $repo 'brand/new/two.md') '2'
    $changed = Get-WorkingTreeChanges -Repo $repo
    # Without -uall git reports 'brand/' and a protected-path glob for
    # 'brand/new/*.md' would not match it.
    ($changed -contains 'brand/new/one.md') -and ($changed -contains 'brand/new/two.md')
}

Assert-That 'ignores gitignored files' {
    $repo = New-GitRepo
    Set-Content (Join-Path $repo '.gitignore') "ignored/`n"
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -qm init 2>$null | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $repo 'ignored') -Force | Out-Null
    Set-Content (Join-Path $repo 'ignored/scratch.txt') 'x'
    # .zagent/ is gitignored and holds run scratch; if it showed up here
    # every guard would report paths the commit will never contain.
    -not ((Get-WorkingTreeChanges -Repo $repo) -contains 'ignored/scratch.txt')
}

Assert-That 'is an empty array when the repo path is absent' {
    $changed = Get-WorkingTreeChanges -Repo $null
    ($null -ne $changed) -and ($changed.Count -eq 0)
}

Write-Host "`n=== exit-code constants survive the module boundary ===" -ForegroundColor Cyan

# `$script:` inside a module is the MODULE's scope, so a constant defined in
# zagent.ps1 is invisible to ZagentClient.psm1. Every `exit $script:EXIT_ERROR`
# in the module threw under Set-StrictMode instead of exiting: `zagent guard
# --file <missing>` reported "cannot be retrieved because it has not been set"
# rather than naming the missing file. Both files parsed and all 62 assertions
# passed, which is the same shape as the unexported `Get-BoardUrl` incident.
Assert-That 'the module defines every EXIT_ constant its own code references' {
    $psm1 = Get-Content (Join-Path $PSScriptRoot 'ZagentClient.psm1') -Raw
    $used = [regex]::Matches($psm1, '\$script:(EXIT_[A-Z_]+)') |
        ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    $defined = [regex]::Matches($psm1, '(?m)^\s*\$script:(EXIT_[A-Z_]+)\s*=') |
        ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    if (-not $used) { return $true }
    -not (@($used | Where-Object { $_ -notin $defined }).Count)
}

Assert-That 'module and script agree on every shared EXIT_ value' {
    $psm1 = Get-Content (Join-Path $PSScriptRoot 'ZagentClient.psm1') -Raw
    $ps1  = Get-Content (Join-Path $PSScriptRoot 'zagent.ps1')        -Raw
    $vals = {
        param($text)
        $h = @{}
        foreach ($m in [regex]::Matches($text, '(?m)^\s*\$script:(EXIT_[A-Z_]+)\s*=\s*(\d+)')) {
            $h[$m.Groups[1].Value] = [int]$m.Groups[2].Value
        }
        return $h
    }
    $a = & $vals $psm1
    $b = & $vals $ps1
    $shared = $a.Keys | Where-Object { $b.ContainsKey($_) }
    if (-not $shared) { return $false }   # they MUST overlap; no overlap means a rename slipped through
    -not (@($shared | Where-Object { $a[$_] -ne $b[$_] }).Count)
}

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
