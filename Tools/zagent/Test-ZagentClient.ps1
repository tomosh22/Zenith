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


Write-Host "`n=== Test-NeedsChangedSet ===" -ForegroundColor Cyan
# `guard` and `gates` ask the same question of the same working tree —
# which files did this ticket touch. Answering it twice, differently, is
# how the gate list and the guard come to describe different diffs.
Assert-That 'covers both commands that compute their own changed set' {
    (Test-NeedsChangedSet -Argv @('guard')) -and
    (Test-NeedsChangedSet -Argv @('gates', 'ZM-50'))
}
Assert-That 'leaves every other command alone' {
    (-not (Test-NeedsChangedSet -Argv @('claim', 'ZM-50'))) -and
    (-not (Test-NeedsChangedSet -Argv @('docs', 'sync'))) -and
    (-not (Test-NeedsChangedSet -Argv @()))
}
Assert-That 'survives a null argv under Set-StrictMode' {
    (-not (Test-NeedsChangedSet -Argv $null))
}
Assert-That 'returns a boolean, not a collection' {
    ((Test-NeedsChangedSet -Argv @('guard')) -is [bool]) -and
    ((Test-NeedsChangedSet -Argv @('queue')) -is [bool])
}

# An EXPLICIT changed set must win over the working tree. `--text` is read
# board-side, so without this the client shipped the working tree instead
# and the caller got a verdict about a different set of files with no hint
# their input had been discarded.
Assert-That 'lets an explicit --text override the working-tree changed set' {
    (-not (Test-NeedsChangedSet -Argv @('guard', '--text', 'a/b.cpp'))) -and
    (-not (Test-NeedsChangedSet -Argv @('gates', 'ZM-1', '--text', 'a/b.cpp')))
}
Assert-That 'still computes the set when no explicit input is given' {
    (Test-NeedsChangedSet -Argv @('guard')) -and (Test-NeedsChangedSet -Argv @('gates', 'ZM-1'))
}

Write-Host "`n=== Get-GuardTicketKey ===" -ForegroundColor Cyan
# Matched on the `-<digits>` SUFFIX, the same rule /tick step 1 uses.
# Digits are legal inside a project key, so "letters versus digits" is
# the wrong test and would misread a project named ZEN2.
Assert-That 'reads the key off a keyed guard' {
    (Get-GuardTicketKey -Argv @('guard', 'ZM-50')) -eq 'ZM-50'
}
Assert-That 'returns null for a bare guard' {
    $null -eq (Get-GuardTicketKey -Argv @('guard'))
}
Assert-That 'returns null for a different command' {
    ($null -eq (Get-GuardTicketKey -Argv @('gates', 'ZM-50'))) -and
    ($null -eq (Get-GuardTicketKey -Argv @('claim', 'ZM-50')))
}
# A project key is not a ticket key. `zagent guard ZM` would otherwise
# be sent as a key the board cannot resolve.
Assert-That 'refuses a project key' {
    $null -eq (Get-GuardTicketKey -Argv @('guard', 'ZM'))
}
Assert-That 'accepts digits INSIDE the project key' {
    (Get-GuardTicketKey -Argv @('guard', 'ZEN2-7')) -eq 'ZEN2-7'
}
Assert-That 'survives a null argv under Set-StrictMode' {
    $null -eq (Get-GuardTicketKey -Argv $null)
}

Write-Host "`n=== Get-RecordedGateSelection ===" -ForegroundColor Cyan
# The gate selection is recorded by `zagent gates <KEY>` and re-derived
# by the board at guard time. A MISSING file must come back as $null and
# NOT as an empty string: the board reads "absent" as "the selection step
# was skipped" and blocks, where an empty string would be a parse error
# with a message about JSON rather than about the step nobody ran.
Assert-That 'reads a recorded selection out of the run scratch' {
    $repo = New-TempDir
    try {
        $dir = Join-Path (Join-Path (Join-Path $repo '.zagent') 'run') 'ZM-50'
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $dir 'gates.json') -Value '{"gates":["a"]}' -Encoding utf8
        $raw = Get-RecordedGateSelection -Repo $repo -Key 'ZM-50'
        ($raw -is [string]) -and ($raw.Contains('"gates"'))
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}
Assert-That 'returns null when the gate-selection step never ran' {
    $repo = New-TempDir
    try {
        $null -eq (Get-RecordedGateSelection -Repo $repo -Key 'ZM-50')
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}
Assert-That 'returns null rather than throwing on a missing repo or key' {
    ($null -eq (Get-RecordedGateSelection -Repo '' -Key 'ZM-50')) -and
    ($null -eq (Get-RecordedGateSelection -Repo 'C:\nope' -Key ''))
}
# It must read the SAME location `zagent gates` writes to, which is
# Get-ScratchRoot + <KEY> + gates.json. A second spelling of that path
# would look exactly like "the step was never run".
Assert-That 'reads from the scratch root the board writes to' {
    $repo = New-TempDir
    try {
        $expected = Join-Path (Join-Path (Get-ScratchRoot $repo) 'ZM-50') 'gates.json'
        New-Item -ItemType Directory -Path (Split-Path $expected) -Force | Out-Null
        Set-Content -LiteralPath $expected -Value '{}' -Encoding utf8
        $null -ne (Get-RecordedGateSelection -Repo $repo -Key 'ZM-50')
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

# Every category that declares `paths` must also declare `gates`, or the
# union it exists for silently adds nothing. The board treats a gateless
# category as inert by design (a category may legitimately own no build),
# but in THIS repo a paths-without-gates entry is always a mistake, and
# it is invisible: the gates that do run all pass.
Assert-That 'every zagent.project.json category with paths also has gates' {
    $repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Get-Content (Join-Path $repo 'zagent.project.json') -Raw | ConvertFrom-Json
    $bad = @()
    foreach ($category in $file.categories.PSObject.Properties) {
        $paths = $category.Value.PSObject.Properties['paths']
        $gates = $category.Value.PSObject.Properties['gates']
        if ($paths -and @($paths.Value).Count -gt 0) {
            if (-not $gates -or @($gates.Value).Count -eq 0) { $bad += $category.Name }
        }
    }
    if ($bad) { Write-Host ("       categories with paths and no gates: " + ($bad -join ', ')) }
    $bad.Count -eq 0
}


Write-Host "`n=== Get-BodyDrift / Format-BodyDrift ===" -ForegroundColor Cyan
# The board extracts what a body claims exists and CANNOT check it -- it
# may be on another machine and has never seen this checkout. Resolving
# is the one half only the client can do, and every ticket body examined
# in one session had drifted: seven for seven.

function New-DriftRepo {
    $repo = New-TempDir
    & git -C $repo init --quiet 2>$null | Out-Null
    & git -C $repo config user.email 'test@example.invalid' 2>$null | Out-Null
    & git -C $repo config user.name 'test' 2>$null | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $repo 'Tools') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $repo 'Tools\present.ps1') -Value 'function Zenith_Present {}' -Encoding utf8
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -m init --quiet 2>$null | Out-Null
    return $repo
}

Assert-That 'reports a cited path this checkout does not have' {
    $repo = New-DriftRepo
    try {
        $c = [PSCustomObject]@{ paths = @('Tools/present.ps1', 'Tools/gone.ps1'); symbols = @() }
        $missing = Get-BodyDrift -Repo $repo -Citations $c
        (@($missing).Count -eq 1) -and (@($missing)[0].value -eq 'Tools/gone.ps1')
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'reports a cited symbol no tracked file contains' {
    $repo = New-DriftRepo
    try {
        $c = [PSCustomObject]@{ paths = @(); symbols = @('Zenith_Present', 'Zenith_Vanished') }
        $missing = Get-BodyDrift -Repo $repo -Citations $c
        (@($missing).Count -eq 1) -and (@($missing)[0].kind -eq 'symbol')
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'is an EMPTY ARRAY when everything resolves, not null and not a string' {
    $repo = New-DriftRepo
    try {
        $c = [PSCustomObject]@{ paths = @('Tools/present.ps1'); symbols = @('Zenith_Present') }
        $missing = Get-BodyDrift -Repo $repo -Citations $c
        ($null -ne $missing) -and ($missing -is [array]) -and ($missing.Count -eq 0)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

# `,@(...)` again: a PowerShell function UNROLLS a returned collection,
# so a single drifted citation would come back as a bare object and
# `.Count` would answer with its property count.
Assert-That 'returns an ARRAY for a single drifted citation' {
    $repo = New-DriftRepo
    try {
        $c = [PSCustomObject]@{ paths = @('Tools/gone.ps1'); symbols = @() }
        (Get-BodyDrift -Repo $repo -Citations $c) -is [array]
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'tolerates a payload with no citations at all' {
    ((Get-BodyDrift -Repo 'C:\nope' -Citations $null).Count -eq 0) -and
    ((Get-BodyDrift -Repo '' -Citations ([PSCustomObject]@{ paths = @('x/y'); symbols = @() })).Count -eq 0)
}

# A payload that legitimately omits one of the two lists must not throw
# under Set-StrictMode, which turns a missing property into an error.
Assert-That 'reads each list through PSObject.Properties, not by direct access' {
    $repo = New-DriftRepo
    try {
        $missing = Get-BodyDrift -Repo $repo -Citations ([PSCustomObject]@{ paths = @('Tools/gone.ps1') })
        @($missing).Count -eq 1
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

# ★ "NOTHING EXTRACTED" AND "NOTHING DRIFTED" USED TO BE THE SAME OUTPUT,
# WHICH WAS NO OUTPUT AT ALL. Three consecutive claims came back with
# `citations: {paths:[], symbols:[], lines:[]}` and wrote no drift.txt,
# so at the call site an unparsed body read exactly like a clean one --
# while every body examined HAD drifted. ZM-27's Goal claimed there was
# "no way to USE an item at all today" with ZM_Bag, ZM_ItemData,
# ZM_UI_Bag and ZM_ShopLogic all present and the bag already persisting
# as save module 6.
Assert-That 'says NOTHING WAS CHECKED when the board extracted no citations' {
    $c = [PSCustomObject]@{ paths = @(); symbols = @(); lines = @() }
    $text = Format-BodyDrift -Key 'ZM-27' -Missing @() -Citations $c
    ($null -ne $text) -and $text.Contains('NO citations') -and
    $text.Contains('NOTHING was checked') -and $text.Contains('not a clean result')
}

Assert-That 'reports the COUNT when everything resolved, so a pass is evidence of something' {
    $c = [PSCustomObject]@{ paths = @('a.cpp', 'b.cpp'); symbols = @('Zenith_X'); lines = @() }
    $text = Format-BodyDrift -Key 'ZM-50' -Missing @() -Citations $c
    $text.Contains('all 3 checkable citation(s) resolve')
}

Assert-That 'reports N of M when some drifted' {
    $c = [PSCustomObject]@{ paths = @('a.cpp', 'b.cpp'); symbols = @('Zenith_X'); lines = @() }
    $missing = @([PSCustomObject]@{ kind = 'path'; value = 'a.cpp'; movedTo = $null })
    $text = Format-BodyDrift -Key 'ZM-50' -Missing $missing -Citations $c
    $text.Contains('1 of 3 checkable') -and $text.Contains('(2 resolved.)')
}

# `lines` is the half nothing can resolve: a line number is true only
# against a commit nobody recorded. Counting it as checked would be a
# lie; omitting it would hide the one bucket that needs a human.
Assert-That 'counts line citations SEPARATELY and says they were not checked' {
    $c = [PSCustomObject]@{ paths = @('a.cpp'); symbols = @(); lines = @('Zenith.cpp:522') }
    $text = Format-BodyDrift -Key 'ZEN-5' -Missing @() -Citations $c
    $text.Contains('all 1 checkable') -and $text.Contains('NOT CHECKED')
}

Assert-That 'a body with ONLY line citations still reports that nothing checkable came back' {
    $c = [PSCustomObject]@{ paths = @(); symbols = @(); lines = @('Zenith.cpp:522') }
    $text = Format-BodyDrift -Key 'ZEN-5' -Missing @() -Citations $c
    $text.Contains('0 checkable citation(s) resolve') -and $text.Contains('NOT CHECKED')
}

Assert-That 'Get-CitationCount tolerates a null, a missing field and empty strings' {
    # `Set-StrictMode` turns a missing property into a throw, and a
    # payload that legally omits one would otherwise crash the client
    # rather than report.
    $a = Get-CitationCount -Citations $null
    $b = Get-CitationCount -Citations ([PSCustomObject]@{ paths = @('x') })
    $c = Get-CitationCount -Citations ([PSCustomObject]@{ paths = @('', $null, 'x'); symbols = @() })
    ($a.total -eq 0) -and ($b.checkable -eq 1) -and ($b.lines -eq 0) -and ($c.paths -eq 1)
}

# The word ADVISORY has to be in the text. A ticket may legitimately name
# what it is about to CREATE, and the first false alarm on one of those
# teaches the reader to ignore the whole check.
Assert-That 'says the warning is ADVISORY, and names the ticket and the citation' {
    $c = [PSCustomObject]@{ paths = @('Tools/gone.ps1'); symbols = @(); lines = @() }
    $text = Format-BodyDrift -Key 'ZM-50' -Citations $c `
        -Missing @([PSCustomObject]@{ kind = 'path'; value = 'Tools/gone.ps1' })
    $text.Contains('ZM-50') -and $text.Contains('Tools/gone.ps1') -and $text.Contains('ADVISORY')
}

# "It moved" is far commoner than "it never existed", and the difference
# decides what a worker DOES: fix a stale path, or create a second file
# at the cited location. ZM-20 cites Tests/ZM_Tests_CommittedSceneBytes.cpp
# and the real file lives under Games/Zenithmon/Tests/.
Assert-That 'points at a file of the same name living somewhere else' {
    $repo = New-TempDir
    try {
        & git -C $repo init --quiet 2>$null | Out-Null
        & git -C $repo config user.email 'test@example.invalid' 2>$null | Out-Null
        & git -C $repo config user.name 'test' 2>$null | Out-Null
        New-Item -ItemType Directory -Path (Join-Path $repo 'Games\Zenithmon\Tests') -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $repo 'Games\Zenithmon\Tests\ZM_Moved.cpp') -Value 'x' -Encoding utf8
        & git -C $repo add -A 2>$null | Out-Null
        & git -C $repo commit -m init --quiet 2>$null | Out-Null
        $c = [PSCustomObject]@{ paths = @('Tests/ZM_Moved.cpp'); symbols = @() }
        # NOT `@(Get-BodyDrift ...)`. The function returns `,@(...)` so a
        # one-row result survives PowerShell's unrolling, and wrapping it
        # again nests the rows one level down -- where `$rows[0].kind`
        # still interpolates correctly through member enumeration while
        # `$rows[0].PSObject.Properties['movedTo']` quietly answers with
        # the ARRAY's properties. That combination fails half a check and
        # passes the other half, which is how it survived being written.
        $missing = Get-BodyDrift -Repo $repo -Citations $c
        $text = Format-BodyDrift -Key 'ZM-20' -Missing $missing -Citations $c
        ($missing[0].movedTo -eq 'Games/Zenithmon/Tests/ZM_Moved.cpp') -and
        $text.Contains('Games/Zenithmon/Tests/ZM_Moved.cpp')
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'says only that it is gone when no file of that name exists' {
    $c = [PSCustomObject]@{ paths = @('a/b.cpp'); symbols = @(); lines = @() }
    $text = Format-BodyDrift -Key 'ZM-1' -Citations $c `
        -Missing @([PSCustomObject]@{ kind = 'path'; value = 'a/b.cpp'; movedTo = $null })
    -not $text.Contains('a file of that name is at')
}


Write-Host "`n=== Get-RequestTimeout ===" -ForegroundColor Cyan
# There was NO timeout, so a board that accepted the connection and then
# stopped responding hung the client forever -- and an unattended /tick
# has no operator to notice. A timeout lands in the same catch as a
# refused connection, so it exits 7: "I never reached the board" is
# exactly what it means.
Assert-That 'gives every command a bounded wait' {
    (Get-RequestTimeout -Argv @('queue')) -gt 0
}
# `docs sync` ships a ~1 MB Markdown tree and writes a CRDT per page.
# Giving every command that budget would mean a wedged `queue` also took
# fifteen minutes to admit it.
Assert-That 'gives the docs upload a longer budget than everything else' {
    (Get-RequestTimeout -Argv @('docs', 'sync')) -gt (Get-RequestTimeout -Argv @('queue'))
}
Assert-That 'covers docs status, which uploads the same tree' {
    (Get-RequestTimeout -Argv @('docs', 'status')) -eq (Get-RequestTimeout -Argv @('docs', 'sync'))
}
Assert-That 'does not extend the budget for a docs READ' {
    (Get-RequestTimeout -Argv @('docs', 'read', 'Zenithmon/Status')) -eq
    (Get-RequestTimeout -Argv @('queue'))
}
Assert-That 'survives a null or one-element argv under Set-StrictMode' {
    ((Get-RequestTimeout -Argv $null) -gt 0) -and ((Get-RequestTimeout -Argv @('docs')) -gt 0)
}
# The upload predicate and the timeout predicate must agree about which
# commands ship the tree, or one of them is budgeting for a request the
# other never makes.
Assert-That 'is generous for exactly the commands that upload the docs tree' {
    $long = (Get-RequestTimeout -Argv @('docs', 'sync'))
    foreach ($argv in @(@('docs', 'sync'), @('docs', 'status'), @('queue'), @('docs', 'ls'))) {
        $uploads = Test-NeedsDocsTree -Argv $argv
        $generous = (Get-RequestTimeout -Argv $argv) -eq $long
        # `board status` uploads the tree but is a cheap read of one
        # file, so it is allowed to be either -- every OTHER command
        # must have the two answers agree.
        if ($argv[0] -ne 'board' -and $uploads -ne $generous) { return $false }
    }
    return $true
}

# The board resolves a path-valued flag from the CLIENT's disk, so the
# two lists have to name the same flags. A flag missing here arrives at
# the board as a PATH STRING which it then treats as prose -- a ticket
# body silently replaced by the characters `C:\tmp\dod.md`.
Assert-That 'resolves every path-valued flag the board declares in FILE_FLAGS' {
    # Asks the FUNCTION, not the file. This used to regex the psm1 source
    # for its `foreach ($name in @(...))` literal, which meant the check
    # tracked the shape of the code rather than its answer -- moving the
    # list into Get-FileFlags broke it while the behaviour was correct.
    $names = Get-FileFlags -Argv @('create')
    foreach ($required in @('file', 'comment', 'worklog', 'body', 'goal', 'dod')) {
        if ($required -notin $names) {
            Write-Host "       missing path-valued flag: $required"
            return $false
        }
    }
    return $true
}


Write-Host "`n=== Engine gates compile EVERY game ===" -ForegroundColor Cyan
# An engine change can break a game it never mentions. Games/Combat --
# which used to be the only game the Engine gate list built -- references
# Zenith_TerrainComponent in ZERO source files, while Zenithmon,
# CityBuilder and RenderTest reference it in 12, 4 and 4. ZEN-5 made
# members of that header private and every Engine gate went green.
#
# The `paths` union cannot catch that: the diff is inside Engine's OWN
# paths, so there is no foreign category to pull in. A public header's
# blast radius is everything that INCLUDES it, which no directory mapping
# expresses -- so the gate list has to name every game, and THIS is what
# stops a newly-added game from being silently left out of it.
Assert-That 'the Engine gate list builds every game that has a .zproj' {
    $repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Get-Content (Join-Path $repo 'zagent.project.json') -Raw | ConvertFrom-Json
    $gates = @($file.categories.Engine.gates) -join "`n"

    $games = @(Get-ChildItem -Path (Join-Path $repo 'Games') -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName ("{0}.zproj" -f $_.Name)) } |
        ForEach-Object { $_.Name })
    if ($games.Count -eq 0) { Write-Host '       no .zproj games found -- cannot assert'; return $false }

    $missing = @($games | Where-Object { $gates -notmatch [regex]::Escape("build $_ ") })
    if ($missing.Count) {
        Write-Host ("       games missing from the Engine gate list: " + ($missing -join ', '))
    }
    $missing.Count -eq 0
}

# Compile before test: a compile break in ANY game should stop the run
# before a minute goes into one game's suite. If a `test` line drifts
# above a `build` line the list still passes, just slower and less
# usefully, so this is a cheap ordering guard rather than a correctness
# one.
Assert-That 'every build line comes before the first test line' {
    $repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $file = Get-Content (Join-Path $repo 'zagent.project.json') -Raw | ConvertFrom-Json
    $gates = @($file.categories.Engine.gates)
    $lastBuild = -1; $firstTest = $gates.Count
    for ($i = 0; $i -lt $gates.Count; $i++) {
        if ($gates[$i] -match '\bbuild\s') { $lastBuild = $i }
        if ($gates[$i] -match '\btest\s' -and $firstTest -eq $gates.Count) { $firstTest = $i }
    }
    $lastBuild -lt $firstTest
}

Write-Host "`n=== Get-FileFlags ===" -ForegroundColor Cyan
# `--goal` names two different things, and this list is what tells them
# apart. A TICKET's goal is a `## Goal` SECTION -- multi-line Markdown,
# so it arrives as a path. A SPRINT's is one inline sentence. While the
# list was applied to every command the sprint form was unreachable:
# `sprint create <NAME> --goal "some text"` treated the text as a
# filename and died on Get-FileContents' Test-Path check.
#
# Spelled TWICE, like Test-NeedsDocsTree above and for the same reason:
# the slurp happens on THIS side, before any request exists, so the
# board cannot decide it. Mirrors `fileFlagsFor` in dispatch.ts.
Assert-That 'keeps --goal path-valued for a ticket update' {
    (Get-FileFlags -Argv @('update', 'ZM-1')) -contains 'goal'
}
Assert-That 'drops --goal for a sprint, whose goal is typed inline' {
    -not ((Get-FileFlags -Argv @('sprint', 'create', 'S9')) -contains 'goal')
}
Assert-That 'narrows only --goal, never the other five' {
    $flags = Get-FileFlags -Argv @('sprint', 'create', 'S9')
    ($flags -contains 'file') -and ($flags -contains 'comment') -and
    ($flags -contains 'worklog') -and ($flags -contains 'body') -and ($flags -contains 'dod')
}
Assert-That 'leaves every other command on the full list' {
    ((Get-FileFlags -Argv @('create')) -contains 'goal') -and
    ((Get-FileFlags -Argv @('finish', 'ZM-1')) -contains 'goal')
}
# `$Argv[0]` on an empty array is a Set-StrictMode violation, and this
# script sets it -- a bare index would throw rather than fall through.
Assert-That 'survives an empty and a null argv under Set-StrictMode' {
    ((Get-FileFlags -Argv @()) -contains 'goal') -and
    ((Get-FileFlags -Argv $null) -contains 'goal')
}
# The narrowing is a RETURNED collection, and a PowerShell function
# unrolls one -- a five-element result must not arrive as five values
# or a bare string. `,@(...)` is what keeps it an array.
Assert-That 'returns an array, not an unrolled string' {
    $flags = Get-FileFlags -Argv @('sprint', 'create', 'S9')
    ($flags -is [array]) -and ($flags.Count -eq 5)
}

Write-Host ""
Write-Host "=== the forward-reference trap: a key cited before it existed ===" -ForegroundColor Cyan

function New-DocsFixture {
    $root = Join-Path ([System.IO.Path]::GetTempPath()) ("zdocs_" + [guid]::NewGuid().ToString('N').Substring(0, 8))
    New-Item -ItemType Directory -Path (Join-Path $root 'Games/Zenithmon/Docs') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $root 'Games/Zenithmon/Docs/Sub') -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $root 'Games/Zenithmon/Docs/Questions.md') `
        -Value "Q-2026-08-14-001 tracked as [ZM-50 / ZEN-2]`nunrelated line`nsee ZEN-20 for the other one" -Encoding utf8
    Set-Content -LiteralPath (Join-Path $root 'Games/Zenithmon/Docs/Sub/Nested.md') `
        -Value "nested mention of ZEN-2 here" -Encoding utf8
    # Not markdown: a key in source or a lockfile is not a prose citation.
    Set-Content -LiteralPath (Join-Path $root 'Games/Zenithmon/Docs/notes.txt') `
        -Value "ZEN-2 in a text file" -Encoding utf8
    return $root
}

$fakeClient = [PSCustomObject]@{
    repo = 'x'
    file = [PSCustomObject]@{
        categories = [PSCustomObject]@{
            Zenithmon = [PSCustomObject]@{ docs = [PSCustomObject]@{ dir = 'Games/Zenithmon/Docs' } }
            Engine    = [PSCustomObject]@{ gates = @('build') }   # no docs at all
        }
    }
}

Assert-That 'the doc dirs come from categories.*.docs.dir, skipping a category with none' {
    $dirs = Get-LivingDocDirs -Client $fakeClient
    ($dirs.Count -eq 1) -and ($dirs[0] -eq 'Games/Zenithmon/Docs')
}

Assert-That 'a project with no categories yields an EMPTY ARRAY, not a throw' {
    # Set-StrictMode turns a missing property into a throw, and a project
    # file may legally have no categories at all.
    $bare = [PSCustomObject]@{ repo = 'x'; file = [PSCustomObject]@{} }
    $dirs = Get-LivingDocDirs -Client $bare
    ($dirs -is [array]) -and ($dirs.Count -eq 0) -and
    ((Get-LivingDocDirs -Client $null).Count -eq 0)
}

Assert-That 'finds the citation, with file and line' {
    $root = New-DocsFixture
    try {
        $hits = Find-KeyCitations -Repo $root -Key 'ZEN-2' -Dirs @('Games/Zenithmon/Docs')
        $q = $hits | Where-Object { $_.File -like '*Questions.md' }
        ($q.Line -eq 1) -and ($q.File -eq 'Games/Zenithmon/Docs/Questions.md')
    } finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That '** ZEN-2 does NOT match ZEN-20 -- the whole point is a trailing digit' {
    $root = New-DocsFixture
    try {
        # NO @() around the call. `Find-KeyCitations` returns `,@(...)`,
        # which emits the array as ONE pipeline item so a one-element
        # result stays an array -- and `@()` around that collects one
        # item and nests it, turning Count 2 into Count 1. Assign
        # directly, the way every other test of a `,@()` return here does.
        $hits = Find-KeyCitations -Repo $root -Key 'ZEN-2' -Dirs @('Games/Zenithmon/Docs')
        # Questions.md line 1 and Sub/Nested.md line 1. The `see ZEN-20`
        # line must not be among them.
        ($hits.Count -eq 2) -and (-not ($hits.Text -match 'see ZEN-20'))
    } finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'recurses into subdirectories but ignores non-markdown' {
    $root = New-DocsFixture
    try {
        $hits = Find-KeyCitations -Repo $root -Key 'ZEN-2' -Dirs @('Games/Zenithmon/Docs')
        ($hits.File -contains 'Games/Zenithmon/Docs/Sub/Nested.md') -and
        (-not ($hits.File -like '*notes.txt'))
    } finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a missing docs dir is skipped, not fatal' {
    $hits = Find-KeyCitations -Repo 'C:\does\not\exist' -Key 'ZEN-2' -Dirs @('Games/X/Docs')
    ($hits -is [array]) -and ($hits.Count -eq 0)
}

Assert-That 'an uncited key returns an EMPTY ARRAY, and formats to $null' {
    # Silence is RIGHT here, unlike the drift check: "no citations" is the
    # ordinary case for a freshly allocated key, not a check that never ran.
    $root = New-DocsFixture
    try {
        $hits = Find-KeyCitations -Repo $root -Key 'ZEN-99' -Dirs @('Games/Zenithmon/Docs')
        ($hits.Count -eq 0) -and ($null -eq (Format-KeyCitations -Key 'ZEN-99' -Citations $hits))
    } finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'the created key comes from payload.key on a create' {
    $result = [PSCustomObject]@{ payload = [PSCustomObject]@{ key = 'ZEN-9' } }
    (Get-CreatedKey -Result $result -Argv @('create', '--project', 'ZEN')) -eq 'ZEN-9'
}

Assert-That '** a key at the ROOT is NOT read -- the mistake that made this check inert' {
    # `$result.key` is always $null; the key is under `payload`. The
    # first guard read the root, so the check silently never ran, and a
    # check that cannot fire reads exactly like a clean result. It got
    # past a green suite and a live `create` that exited 0.
    $rootOnly = [PSCustomObject]@{ key = 'ZEN-9' }
    $null -eq (Get-CreatedKey -Result $rootOnly -Argv @('create'))
}

Assert-That 'no key for a command that is not create' {
    $result = [PSCustomObject]@{ payload = [PSCustomObject]@{ key = 'ZEN-9' } }
    ($null -eq (Get-CreatedKey -Result $result -Argv @('show', 'ZEN-9'))) -and
    ($null -eq (Get-CreatedKey -Result $result -Argv @()))
}

Assert-That 'a create that returned no payload is not a throw under StrictMode' {
    ($null -eq (Get-CreatedKey -Result ([PSCustomObject]@{}) -Argv @('create'))) -and
    ($null -eq (Get-CreatedKey -Result $null -Argv @('create'))) -and
    ($null -eq (Get-CreatedKey -Result ([PSCustomObject]@{ payload = $null }) -Argv @('create')))
}

Assert-That 'the warning names every file:line and says what to do' {
    $root = New-DocsFixture
    try {
        $hits = Find-KeyCitations -Repo $root -Key 'ZEN-2' -Dirs @('Games/Zenithmon/Docs')
        $text = Format-KeyCitations -Key 'ZEN-2' -Citations $hits
        ($text -match 'ALREADY CITED') -and ($text -match 'Questions\.md:1') -and
        ($text -match 'Nested\.md:1') -and ($text -match 'allocated sequentially')
    } finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
}

Write-Host ""
Write-Host "=== help is answered locally, and only the FLAG forms are ===" -ForegroundColor Cyan

# `zagent next --help` CLAIMED A TICKET. Every token here is one that
# would have, so this block is the regression net for that.
foreach ($token in @('--help', '-h', '-help', '--h', '-?', '/?')) {
    Assert-That "``$token`` on an ACTING command is a help request, not the action" {
        Test-HelpFlag -Argv @('next', $token)
    }
}

Assert-That 'a help token ANYWHERE in argv still counts, not just position 2' {
    Test-HelpFlag -Argv @('claim', '--project', 'ZM', '--help')
}

# These two are the other half of the design: the board owns the command
# table, so the positional route has to keep reaching it. Intercepting
# `help` here would mean the real list could never be printed at all.
Assert-That '** a BARE `help` positional is NOT intercepted -- the board owns the list' {
    -not (Test-HelpFlag -Argv @('help'))
}

Assert-That '** `help <command>` is NOT intercepted either' {
    -not (Test-HelpFlag -Argv @('help', 'next'))
}

Assert-That 'an EMPTY argv is not intercepted -- it already routes to the board' {
    (-not (Test-HelpFlag -Argv @())) -and (-not (Test-HelpFlag -Argv $null))
}

Assert-That 'an ordinary command is untouched' {
    (-not (Test-HelpFlag -Argv @('queue', '--project', 'ZM'))) -and
    (-not (Test-HelpFlag -Argv @('show', 'ZM-21')))
}

Assert-That 'the subject is the command asked about, and $null when there is none' {
    ((Get-HelpSubject -Argv @('next', '--help')) -eq 'next') -and
    ($null -eq (Get-HelpSubject -Argv @('--help'))) -and
    ($null -eq (Get-HelpSubject -Argv @('/?')))
}

# `zagent help next` ignores its argument and prints the same table as
# `zagent help`, so the stub must not offer per-command help. It names
# the command you asked about and points at the one view that exists.
Assert-That 'the stub names the subject but promises only `zagent help`' {
    $stub = Format-HelpStub -Argv @('next', '--help')
    ($stub -match 'You asked about `next`') -and
    ($stub -match 'nothing was claimed') -and
    (-not ($stub -match 'zagent help next'))
}

# The stub must stay a STUB. If someone ever pastes the command table in
# to make it more helpful, the fact has two homes and the copy goes
# stale -- which is the defect this whole client keeps finding. These
# are board-side command names that must not appear locally.
Assert-That '** the stub carries NO command list -- that fact has one home' {
    $stub = Format-HelpStub -Argv @('--help')
    # Counting `zagent <word>` LINES, not vocabulary. The first version
    # of this matched command NAMES and failed on "nothing to claim" --
    # which is an exit-code description, not a command listing. English
    # and the command table share words; what they do not share is shape.
    # A pasted table is many invocation lines, so the count is the tell.
    $invocations = @([regex]::Matches($stub, 'zagent [<\w]'))
    $invocations.Count -le 4
}

Write-Host "`n=== AUDIT FIXES 2026-08-24 ===" -ForegroundColor Cyan
# Four defects found by running /tick three times end to end against the ZM
# board. Each assertion below names the failure it closes, because every one
# of them went GREEN on every gate before it was found.

function New-TwoGameRepo {
    # Two games that BOTH carry a Status.md -- the shape that made the drift
    # hint wrong. DevilsPlayground sorts before Zenithmon, so `git ls-files`
    # returns it first and `Select-Object -First 1` always picked the wrong one.
    $repo = New-TempDir
    & git -C $repo init --quiet 2>$null | Out-Null
    & git -C $repo config user.email 'test@example.invalid' 2>$null | Out-Null
    & git -C $repo config user.name 'test' 2>$null | Out-Null
    foreach ($g in @('DevilsPlayground', 'Zenithmon')) {
        $d = Join-Path $repo "Games\$g\Docs"
        New-Item -ItemType Directory -Path $d -Force | Out-Null
        Set-Content -LiteralPath (Join-Path $d 'Status.md') -Value "# $g" -Encoding utf8
    }
    & git -C $repo add -A 2>$null | Out-Null
    & git -C $repo commit -m init --quiet 2>$null | Out-Null
    return $repo
}

Assert-That '** an ambiguous citation prefers the TICKET CATEGORY over path order' {
    $repo = New-TwoGameRepo
    try {
        $c = [PSCustomObject]@{ paths = @('Status.md'); symbols = @() }
        $m = @(Get-BodyDrift -Repo $repo -Citations $c -CategoryPaths @('Games/Zenithmon/'))
        # Without the category it resolved to DevilsPlayground, deterministically.
        ($m.Count -eq 1) -and ($m[0].movedTo -like '*Zenithmon*')
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That '** with NO category to rank by, it names every candidate and picks none' {
    $repo = New-TwoGameRepo
    try {
        $c = [PSCustomObject]@{ paths = @('Status.md'); symbols = @() }
        $m = @(Get-BodyDrift -Repo $repo -Citations $c)
        # movedTo stays $null on purpose: one path reads as an answer.
        ($m.Count -eq 1) -and ($null -eq $m[0].movedTo) -and (@($m[0].candidates).Count -eq 2)
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That '** an ambiguous drift report ASKS rather than answers' {
    $repo = New-TwoGameRepo
    try {
        $c = [PSCustomObject]@{ paths = @('Status.md'); symbols = @() }
        $m = Get-BodyDrift -Repo $repo -Citations $c
        $text = Format-BodyDrift -Key 'ZM-1' -Missing $m -Citations $c
        ($text -match 'WHICH ONE is a judgement') -and
            ($text -match 'Games/DevilsPlayground/Docs/Status\.md') -and
            ($text -match 'Games/Zenithmon/Docs/Status\.md')
    } finally { Remove-Item -LiteralPath $repo -Recurse -Force -ErrorAction SilentlyContinue }
}

Assert-That 'a category path glob resolves to its literal prefix' {
    $client = [PSCustomObject]@{ repo = 'C:/x'; file = [PSCustomObject]@{
        categories = [PSCustomObject]@{ Zenithmon = [PSCustomObject]@{ paths = @('Games/Zenithmon/**') } } } }
    $p = @(Get-CategoryPathPrefixes -Client $client -Category 'Zenithmon')
    ($p.Count -eq 1) -and ($p[0] -eq 'Games/Zenithmon/')
}

Assert-That 'an unknown category ranks by nothing rather than by nothing-matches' {
    $client = [PSCustomObject]@{ repo = 'C:/x'; file = [PSCustomObject]@{
        categories = [PSCustomObject]@{ Zenithmon = [PSCustomObject]@{ paths = @('Games/Zenithmon/**') } } } }
    # NOT `@(...)`. These return `,@()`, so an extra @() wraps the empty array
    # into a ONE-element array and Count reads 1. Same trap the file already
    # documents at Get-BodyDrift: call it directly and read .Count.
    ((Get-CategoryPathPrefixes -Client $client -Category 'Nope').Count -eq 0) -and
        ((Get-CategoryPathPrefixes -Client $null -Category 'Zenithmon').Count -eq 0)
}

Assert-That '** a REFUSED claim is rolled back -- lane AND assignee' {
    # The board writes before it validates, so exit 4 arrives with the row
    # already In Progress and assigned. Left alone it holds the I5 repo lock
    # and the next `zagent next` returns exit 5: one /tick on a needs-human
    # ticket stops every project this checkout serves.
    $calls = [System.Collections.Generic.List[string]]::new()
    $payload = [PSCustomObject]@{ key = 'ZM-64'; previousStatus = 'To Do' }
    $notes = Restore-RefusedClaim -Payload $payload -Client $null -Invoke {
        param($a) [void]$calls.Add($a -join ' '); return $null }
    ($calls.Count -eq 2) -and
        ($calls[0] -eq 'move ZM-64 To Do') -and
        ($calls[1] -eq 'update ZM-64 --assignee none') -and
        (@($notes).Count -ge 1)
}

Assert-That '** a claim that was NOT written is left alone -- no pointless writes' {
    # A board that validates first sends no previousStatus. The rollback must
    # become inert rather than issuing two writes against an untouched row.
    $calls = [System.Collections.Generic.List[string]]::new()
    $payload = [PSCustomObject]@{ key = 'ZM-64' }
    $notes = Restore-RefusedClaim -Payload $payload -Client $null -Invoke {
        param($a) [void]$calls.Add($a -join ' '); return $null }
    ($calls.Count -eq 0) -and (@($notes).Count -eq 0)
}

Assert-That 'a rollback whose writes THROW reports instead of crashing the refusal' {
    $payload = [PSCustomObject]@{ key = 'ZM-64'; previousStatus = 'To Do' }
    $notes = Restore-RefusedClaim -Payload $payload -Client $null -Invoke {
        param($a) throw 'board said no' }
    ($notes.Count -ge 1) -and (($notes -join ' ') -match 'rollback FAILED')
}

Assert-That '** an unticked Definition-of-Done box is found' {
    $log = @'
## Work Log

**Definition of Done**

- [x] the first thing
- [ ] the second thing, which is not done

**Deviations / follow-ups**

- [ ] this is NOT a DoD box and must not count
'@
    $unmet = @(Get-UnmetDoneCriteria -WorkLog $log)
    ($unmet.Count -eq 1) -and ($unmet[0] -eq 'the second thing, which is not done')
}

Assert-That 'a fully ticked Definition of Done is clean' {
    $log = "**Definition of Done**`n`n- [x] one`n- [x] two`n"
    (Get-UnmetDoneCriteria -WorkLog $log).Count -eq 0
}

Assert-That 'a work log with no Definition of Done section is clean, not a crash' {
    ((Get-UnmetDoneCriteria -WorkLog "## Work Log`n`n- [ ] a bare checklist").Count -eq 0) -and
        ((Get-UnmetDoneCriteria -WorkLog $null).Count -eq 0) -and
        ((Get-UnmetDoneCriteria -WorkLog '').Count -eq 0)
}

Assert-That 'a heading-style Definition of Done is read the same as a bold one' {
    $log = "### Definition of Done`n`n- [ ] still counts`n"
    @(Get-UnmetDoneCriteria -WorkLog $log).Count -eq 1
}

Write-Host ""
Write-Host ("{0}/{1} assertions passed." -f ($script:count - $script:failures), $script:count)
if ($script:failures -eq 0) { Write-Host 'PASS' -ForegroundColor Green; exit 0 }
Write-Host ("FAIL -- {0} assertion(s)" -f $script:failures) -ForegroundColor Red
exit 1
