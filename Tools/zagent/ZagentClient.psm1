<#
.SYNOPSIS
  Pure helpers for the `zagent` client, split out so they can be TESTED.

.DESCRIPTION
  `zagent.ps1` runs a command the moment it is invoked, which makes every
  function inside it unreachable from a test. Everything here is
  side-effect-free with respect to the network and the environment: path
  resolution, JSON shaping, payload assembly, applying the writes the
  board returns, and the client half of `doctor`.

  Two of these encode bugs that were found the hard way and would recur
  silently if the code were re-derived — see `Remove-Annotations` and
  `ConvertTo-PosixPath`. `Test-ZagentClient.ps1` locks both down.
#>

Set-StrictMode -Version Latest

$script:PROJECT_FILE = 'zagent.project.json'

# `$script:` inside a MODULE is the module's own scope, NOT the scope of the
# script that imported it. So a constant defined in `zagent.ps1` is invisible
# here, and every `exit $script:EXIT_ERROR` below threw
# "the variable cannot be retrieved because it has not been set" under
# Set-StrictMode instead of exiting — turning a clean "file does not exist"
# message into a crash. This is the same module-boundary class as the
# unexported `Get-BoardUrl` the README records: both files parse, every unit
# passes, and the real binary fails.
#
# These MUST match the values in `zagent.ps1`; Test-ZagentClient.ps1 asserts it.
$script:EXIT_ERROR = 1
$script:EXIT_UNREACHABLE = 7

# ─── LOCATING THINGS ─────────────────────────────────

<#
Walk up from the cwd looking for zagent.project.json.

This is the ONE thing here that reads the cwd, and it is the right
question to ask it: the project file is a fact about the checkout you are
standing in. `zagent owns ZEN-11` from Games/Zenithmon must find the same
file as one run from the repo root.
#>
function Find-ClientRepo {
    param([string]$StartDir = (Get-Location).Path)
    $dir = [System.IO.Path]::GetFullPath($StartDir)
    while ($true) {
        if (Test-Path -LiteralPath (Join-Path $dir $script:PROJECT_FILE)) { return $dir }
        $parent = [System.IO.Path]::GetDirectoryName($dir)
        if ([string]::IsNullOrEmpty($parent) -or $parent -eq $dir) { return $null }
        $dir = $parent
    }
}

function ConvertTo-PosixPath {
    param([string]$Path)
    return ($Path -replace '\\', '/') -replace '/+$', ''
}

<#
Strip $-prefixed keys at every depth.

JSON has no comments and this file pins unit-gate BASELINES — numbers
whose whole problem is that they look like magic constants. The board
strips them the same way; doing it here too means a malformed comment
fails locally with a readable error instead of as a 400.
#>
function Remove-Annotations {
    param($Value)
    if ($null -eq $Value) { return $null }
    if ($Value -is [System.Collections.IList]) {
        # `,@(…)` — NOT `@(…)`. A PowerShell function UNROLLS a returned
        # collection, so a one-element array comes back as a scalar and
        # `cleanup: ["…"]` is serialised as a bare string. The board's
        # strict schema then rejects the whole file with "Expected array,
        # received string", which is a confusing way to discover that a
        # language feature ate your JSON.
        return , @($Value | ForEach-Object { Remove-Annotations $_ })
    }
    if ($Value -is [System.Management.Automation.PSCustomObject]) {
        $out = [ordered]@{}
        foreach ($prop in $Value.PSObject.Properties) {
            if ($prop.Name.StartsWith('$')) { continue }
            $out[$prop.Name] = Remove-Annotations $prop.Value
        }
        return [PSCustomObject]$out
    }
    return $Value
}

function Get-ClientProject {
    param([string]$Repo)
    if (-not $Repo) { return $null }
    $file = Join-Path $Repo $script:PROJECT_FILE
    try {
        $raw = Get-Content -LiteralPath $file -Raw -Encoding utf8 | ConvertFrom-Json
    } catch {
        Write-StdErr "$file is not valid JSON — $($_.Exception.Message)"
        exit $script:EXIT_ERROR
    }
    return [PSCustomObject]@{
        repo = ConvertTo-PosixPath $Repo
        file = Remove-Annotations $raw
    }
}

function Write-StdErr {
    param([string]$Message)
    [Console]::Error.WriteLine($Message)
}

# ─── ARGUMENT INSPECTION ─────────────────────────────
#
# Only enough parsing to know which LOCAL files to read. The board does
# the real parsing, so there is exactly one implementation of what
# `--category` means and this script can never drift from it.

function Get-FlagValue {
    param([string[]]$Argv, [string]$Name)
    for ($i = 0; $i -lt $Argv.Count; $i++) {
        if ($Argv[$i] -eq "--$Name") {
            if ($i + 1 -lt $Argv.Count) { return $Argv[$i + 1] }
            return $null
        }
        if ($Argv[$i] -like "--$Name=*") {
            return $Argv[$i].Substring($Name.Length + 3)
        }
    }
    return $null
}

function Test-Flag {
    param([string[]]$Argv, [string]$Name)
    return ($Argv -contains "--$Name")
}

# ─── PAYLOAD ASSEMBLY ────────────────────────────────

<#
Every path the working tree would stage, one per line — the input the
protected-path guard actually wants.

This lives in code rather than in `/tick`'s prose because BOTH ways of
spelling it by hand were wrong, and both shipped:

  * `git diff --name-only <base>...HEAD` is EMPTY at guard time in both
    branching modes. The tick commits at step 7, and step 4 in branch mode
    only runs `switch -c`, so the branch sits at baseBranch with zero
    commits. Empty file -> guard exits 1 -> every ticket Blocks.
  * `git diff --name-only` reports only TRACKED modifications, so a file
    the worker CREATED never reaches the check at all. A new
    `.claude/agents/anything.md` sails straight past it. That one fails
    OPEN, and is invisible on any ticket that only edits existing files.

`status --porcelain -uall` is the only form that sees modified, deleted
AND untracked-but-not-ignored, and `-uall` expands a wholly-new directory
into its files instead of naming the directory.

A rename arrives as `R  old -> new`; BOTH sides are returned, because
moving a file OUT of a protected directory still touches it.
#>
function Get-WorkingTreeChanges {
    param([string]$Repo)
    if (-not $Repo) { return ,@() }
    $lines = & git -C $Repo status --porcelain -uall 2>$null
    if (-not $lines) { return ,@() }
    $paths = [System.Collections.Generic.List[string]]::new()
    foreach ($line in @($lines)) {
        if ($null -eq $line -or $line.Length -le 3) { continue }
        foreach ($part in ($line.Substring(3) -split ' -> ')) {
            $p = $part.Trim()
            # git quotes paths containing spaces or non-ASCII bytes.
            if ($p.Length -ge 2 -and $p.StartsWith('"') -and $p.EndsWith('"')) {
                $p = $p.Substring(1, $p.Length - 2)
            }
            if ($p) { [void]$paths.Add($p) }
        }
    }
    # `,@(…)` or a one-element result unrolls to a bare string on return.
    return ,@($paths | Select-Object -Unique)
}

<# Path-valued flags, resolved to content. The board never opens a path. #>
function Get-FileContents {
    param([string[]]$Argv)
    $files = [ordered]@{}
    foreach ($name in @('file', 'comment', 'worklog')) {
        $value = Get-FlagValue -Argv $Argv -Name $name
        if ($value) {
            if (-not (Test-Path -LiteralPath $value)) {
                Write-StdErr "--$name $value does not exist."
                exit $script:EXIT_ERROR
            }
            $files[$name] = Get-Content -LiteralPath $value -Raw -Encoding utf8
        }
    }
    if ($files.Count -eq 0) { return $null }
    return [PSCustomObject]$files
}

function Test-NeedsDocsTree {
    <#
      Which commands ship the client's living-doc tree with them.

      This rule is spelled TWICE — here, and as `needsDocsTree` in the
      board's `packages/agent/src/dispatch.ts`. That duplication is not
      avoidable: the two run on different machines, in different
      languages, and the client has to decide what to upload BEFORE the
      board sees the request. What is avoidable is the DRIFT, and the
      failure mode when it happens is nasty — a command that reads the
      docs but is missing from this list arrives with no tree, and the
      board answers "no Roadmap.md in the uploaded tree", which reads
      like a client mistake rather than a rule that disagrees with
      itself.

      So the assertions in Test-ZagentClient.ps1 are a deliberate COPY of
      that file's, case for case. A divergence fails on one side.

      It lived inline in zagent.ps1's main body until it grew a second
      clause for `board status`, where nothing could reach it: the main
      body runs only when the script is invoked, so no test could see it
      and the board machine — which uses the Node client — cannot run it
      at all.
    #>
    param([string[]]$Argv)

    if (-not $Argv -or $Argv.Count -lt 2) { return $false }
    if ($Argv[0] -eq 'docs' -and $Argv[1] -in @('sync', 'status')) { return $true }
    # `board status` reads Roadmap.md out of the SAME upload rather than
    # inventing a second way for the board to see a client's disk.
    return ($Argv[0] -eq 'board' -and $Argv[1] -eq 'status')
}

<#
The living-doc Markdown a `docs sync` mirrors.

The board plans the page tree from these, so the keys must be the exact
absolute POSIX paths it derives from `repo` + `docs.dir` — a mismatch
looks like an empty tree and mirrors nothing without erroring.
#>
function Get-DocsTree {
    param($Client)
    if (-not $Client) { return $null }
    $tree = [ordered]@{}
    $categories = $Client.file.PSObject.Properties['categories']

    if ($categories) {
        foreach ($category in $categories.Value.PSObject.Properties) {
            $docs = $category.Value.PSObject.Properties['docs']
            if (-not $docs) { continue }
            $dir = $docs.Value.dir
            $root = Join-Path $Client.repo $dir
            if (-not (Test-Path -LiteralPath $root)) { continue }
            foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Filter *.md) {
                $key = ConvertTo-PosixPath $file.FullName
                $tree[$key] = Get-Content -LiteralPath $file.FullName -Raw -Encoding utf8
            }
        }
    }

    foreach ($entry in (Get-ConventionsTree -Client $Client).GetEnumerator()) {
        $tree[$entry.Key] = $entry.Value
    }

    if ($tree.Count -eq 0) { return $null }
    return [PSCustomObject]$tree
}

<#
Directory basenames a `conventionDocs` walk never descends into — the
built-in floor, before a project's own `exclude` list adds to it.

Mirrors `DEFAULT_CONVENTIONS_EXCLUDE` in `packages/agent/src/docs.ts` in
the other repo. The two lists cannot literally share code — one runs
here, in PowerShell, before anything is shipped over the wire; the other
runs in TypeScript, for the case where the board and the repo share a
machine and `docs sync` reads the disk directly. Keep them in sync by
hand if you touch either.
#>
$script:CONVENTIONS_EXCLUDE_DEFAULT = @(
    '.git', '.zagent', '.claude', '.vs', '.worktrees',
    'node_modules', 'Build', 'build', 'output', 'obj', 'dist',
    '.next', '.turbo', 'coverage', 'playwright-report', 'test-results',
    'ThirdParty', 'Middleware', 'vendor', 'Assets', 'Intermediate', 'Saved'
)

<#
Every `CLAUDE.md` in the repo — root and every leaf module — keyed by
absolute POSIX path so it lines up with what the board plans against
(`repoJoin(repo, '')` plus the relative path, in `docs.ts`).

`Get-ChildItem -Recurse -Filter CLAUDE.md` alone would still WALK every
`node_modules`, every `Build\output`, every asset tree on the way past —
recursion does not get to skip a directory just because nothing inside
it will match. This prunes BEFORE descending: a blocked directory is
never opened at all, matching `conventionsSource`'s contract that a
blocked subtree is unreachable, not merely filtered from the result.

Returns an empty (never null) hashtable when `conventionDocs` is absent
or nothing matched, so `Get-DocsTree` can always `.GetEnumerator()` it.
#>
function Get-ConventionsTree {
    param($Client)
    $tree = [ordered]@{}
    if (-not $Client) { return $tree }
    $conventionDocs = $Client.file.PSObject.Properties['conventionDocs']
    if (-not $conventionDocs) { return $tree }

    $exclude = [System.Collections.Generic.HashSet[string]]::new([string[]]$script:CONVENTIONS_EXCLUDE_DEFAULT)
    $excludeProp = $conventionDocs.Value.PSObject.Properties['exclude']
    if ($excludeProp) {
        foreach ($name in @($excludeProp.Value)) { [void]$exclude.Add($name) }
    }

    $stack = [System.Collections.Generic.Stack[string]]::new()
    $stack.Push($Client.repo)
    while ($stack.Count -gt 0) {
        $dir = $stack.Pop()
        $children = Get-ChildItem -LiteralPath $dir -Force -ErrorAction SilentlyContinue
        foreach ($child in $children) {
            if ($child.PSIsContainer) {
                if ($exclude.Contains($child.Name)) { continue }
                $stack.Push($child.FullName)
            } elseif ($child.Name -eq 'CLAUDE.md') {
                $key = ConvertTo-PosixPath $child.FullName
                $tree[$key] = Get-Content -LiteralPath $child.FullName -Raw -Encoding utf8
            }
        }
    }
    return $tree
}

<#
Files the board must AMEND rather than create.

`decide` scans the next free id out of the existing DecisionLog — the log
is append-only and the number moves with every entry anyone adds, so it
can never be computed without the current contents.
#>
function Get-AmendContents {
    param($Client, [string[]]$Argv)
    if (-not $Client -or $Argv[0] -ne 'decide') { return $null }
    $amend = [ordered]@{}
    $categories = $Client.file.PSObject.Properties['categories']
    if (-not $categories) { return $null }
    foreach ($category in $categories.Value.PSObject.Properties) {
        $log = $category.Value.PSObject.Properties['decisionLog']
        if (-not $log) { continue }
        $abs = Join-Path $Client.repo $log.Value
        if (Test-Path -LiteralPath $abs) {
            $amend[$log.Value] = Get-Content -LiteralPath $abs -Raw -Encoding utf8
        }
    }
    if ($amend.Count -eq 0) { return $null }
    return [PSCustomObject]$amend
}

# ─── RESULT APPLICATION ──────────────────────────────

function Get-ScratchRoot {
    param([string]$Repo)
    return (Join-Path (Join-Path $Repo '.zagent') 'run')
}

<#
The board returns what should be written; this applies it.

That indirection is the whole shape of the remote design — the board
cannot touch a filesystem on another machine, so a claim returns its
scratch and a `decide` returns the composed DecisionLog rather than
writing either.
#>
function Write-Results {
    param($Result, [string]$Repo)
    if (-not $Result.PSObject.Properties['writes']) { return }
    foreach ($write in $Result.writes) {
        $target = if ($write.base -eq 'scratch') {
            Join-Path (Get-ScratchRoot $Repo) $write.path
        } else {
            Join-Path $Repo $write.path
        }
        $dir = [System.IO.Path]::GetDirectoryName($target)
        if (-not (Test-Path -LiteralPath $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }
        # No BOM: these files are read by git, by the tick, and by a
        # worker's Read tool, and a BOM shows up as a diff in all three.
        [System.IO.File]::WriteAllText($target, $write.content, [System.Text.UTF8Encoding]::new($false))
    }
}

function Write-LastResult {
    param($Result, [string]$Repo)
    try {
        $dir = Join-Path $Repo '.zagent'
        if (-not (Test-Path -LiteralPath $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }
        $json = $Result.payload | ConvertTo-Json -Depth 100
        [System.IO.File]::WriteAllText((Join-Path $dir 'last.json'), $json,
            [System.Text.UTF8Encoding]::new($false))
    } catch {
        # Best effort — stdout already carried the payload.
    }
}

# ─── CLIENT-SIDE DOCTOR ──────────────────────────────

<#
The half of `doctor` the board cannot answer.

Repo cleanliness, the current branch and whether a gate executable
resolves are facts about THIS machine. A board asked for them would be
answering about its own disk and calling it the agent's — which is worse
than silence, because it would read as a pass.
#>
function Get-ClientChecks {
    param($Client)
    $checks = @()
    if (-not $Client) {
        return , @(@{ name = 'client repo'; ok = $false
                      detail = "no $script:PROJECT_FILE found above $((Get-Location).Path)" })
    }

    $repo = $Client.repo
    $checks += @{ name = 'client repo'; ok = $true; detail = $repo }

    # `baseBranch` is OPTIONAL in the schema, and StrictMode turns a
    # missing property into a thrown exception rather than $null — so a
    # perfectly valid project file that omits it would make `doctor`
    # crash instead of report, which is the one thing doctor must never
    # do.
    $branchProp = $Client.file.PSObject.Properties['baseBranch']
    if ($branchProp) {
        $expected = $branchProp.Value
        $branch = (git -C $repo rev-parse --abbrev-ref HEAD 2>$null)
        $checks += @{
            name   = 'repo branch'; ok = ($branch -eq $expected)
            detail = if ($branch -eq $expected) { "on $branch" }
                     else { "on $branch, expected $expected" }
        }
    }

    $dirty = (git -C $repo status --porcelain 2>$null)
    $checks += @{
        name = 'repo clean'; ok = [string]::IsNullOrWhiteSpace($dirty)
        detail = if ([string]::IsNullOrWhiteSpace($dirty)) { 'clean' }
                 else { "$(($dirty -split "`n").Count) uncommitted path(s)" }
    }

    # One row per distinct executable, not per gate line: a dozen
    # identical "pwsh resolves" rows buries the one that failed.
    $executables = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($line in Get-AllGateLines $Client) {
        $first = ($line -split '\s+')[0]
        if ($first) { [void]$executables.Add($first) }
    }
    foreach ($exe in $executables) {
        $resolves = [bool](Get-Command $exe -ErrorAction SilentlyContinue)
        $checks += @{
            name = "gate executable $exe"; ok = $resolves
            detail = if ($resolves) { "$exe resolves" } else { "$exe is not on PATH — this gate would stall" }
        }
    }

    $categories = $Client.file.PSObject.Properties['categories']
    if ($categories) {
        foreach ($category in $categories.Value.PSObject.Properties) {
            $docs = $category.Value.PSObject.Properties['docs']
            if (-not $docs) { continue }
            $root = Join-Path $repo $docs.Value.dir
            $present = Test-Path -LiteralPath $root
            $checks += @{
                name = "docs $($category.Name)"; ok = $present
                detail = if ($present) { $root } else { "$root does not exist" }
            }
        }
    }

    $conventionDocs = $Client.file.PSObject.Properties['conventionDocs']
    if ($conventionDocs) {
        $count = (Get-ConventionsTree -Client $Client).Count
        $checks += @{
            name = 'docs Conventions'; ok = ($count -gt 0)
            detail = if ($count -gt 0) { "$count CLAUDE.md file(s) found" }
                     else { 'no CLAUDE.md files found under the repo (after exclude)' }
        }
    }
    # `,@(…)` — a function returning a SINGLE check would otherwise hand
    # back a bare hashtable, and `.Count` on one of those answers "3"
    # (its key count). Same unrolling trap as Remove-Annotations.
    #
    # The cost: the result is one pipeline item, so `… | ForEach-Object`
    # sees the whole array as `$_`. ASSIGN it and iterate — which is what
    # `zagent.ps1` does (`$all = @($boardChecks) + @($clientChecks)`).
    return , @($checks)
}

function Get-AllGateLines {
    param($Client)
    $lines = @()
    foreach ($name in @('gates', 'cleanup')) {
        $prop = $Client.file.PSObject.Properties[$name]
        if ($prop) { $lines += $prop.Value }
    }
    $categories = $Client.file.PSObject.Properties['categories']
    if ($categories) {
        foreach ($category in $categories.Value.PSObject.Properties) {
            $gates = $category.Value.PSObject.Properties['gates']
            if ($gates) { $lines += $gates.Value }
        }
    }
    return , @($lines)
}


Export-ModuleMember -Function Find-ClientRepo, ConvertTo-PosixPath, Remove-Annotations,
    Get-ClientProject, Get-FlagValue, Test-Flag, Get-FileContents, Get-WorkingTreeChanges,
    Test-NeedsDocsTree, Get-DocsTree,
    Get-ConventionsTree, Get-AmendContents, Get-ScratchRoot, Write-Results, Write-LastResult,
    Get-ClientChecks, Get-AllGateLines, Write-StdErr
