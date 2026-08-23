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

<#
  Path-valued flags, resolved to content. The board never opens a path.

  `body`, `goal` and `dod` are here for the same reason `comment` and
  `worklog` are: no amount of shell quoting survives a multi-line
  Markdown body, and the symptom is a ticket whose spec got truncated at
  the first newline with a successful exit. The list is the twin of
  FILE_FLAGS in the board's dispatch.ts -- a flag missing from one side
  arrives as a path string the board then treats as prose.
#>
<#
  Which of those flags are path-valued FOR THIS COMMAND.

  `--goal` names two different things depending on where it lands. A
  TICKET's goal replaces its `## Goal` section -- multi-line Markdown,
  hence a path, for the reason above. A SPRINT's goal is a one-sentence
  objective typed inline. They only ever shared a spelling, and while
  this list was applied to every command the sprint one was unreachable:
  `sprint create <NAME> --goal "some text"` read the text as a filename
  and died on the Test-Path check above.

  Twin of `fileFlagsFor` in the board's dispatch.ts, for the same reason
  the list itself is spelled twice -- the slurp happens HERE, before any
  request exists, so the board cannot decide it. A narrowing missing
  from this side sends the board a path string it then stores as prose.
#>
function Get-FileFlags {
    param([string[]]$Argv)
    $all = @('file', 'comment', 'worklog', 'body', 'goal', 'dod')
    if ($null -ne $Argv -and $Argv.Count -gt 0 -and $Argv[0] -eq 'sprint') {
        # `,@(...)` or a one-element result unrolls to a bare string.
        return ,@($all | Where-Object { $_ -ne 'goal' })
    }
    return ,@($all)
}

function Get-FileContents {
    param([string[]]$Argv)
    $files = [ordered]@{}
    foreach ($name in (Get-FileFlags -Argv $Argv)) {
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

function Get-RequestTimeout {
    <#
      How long to wait before deciding the board is not going to answer.

      There was NO timeout at all, so a board that accepted the
      connection and then stopped responding hung the client forever --
      and an unattended /tick has no operator to notice. A timeout falls
      into the same catch as a refused connection and so exits 7, which
      is right: "I never reached the board" is exactly what it means.

      `docs sync` gets its own budget because it ships a ~1 MB Markdown
      tree and writes a CRDT per page, which is legitimately slow.
      Giving every command that allowance would mean a wedged `queue`
      also took fifteen minutes to admit it.

      In the MODULE rather than in zagent.ps1's main body, for the reason
      Test-NeedsDocsTree is: that file runs a command the moment it is
      invoked, so nothing there is reachable from a test.
    #>
    param([string[]]$Argv)

    if ($Argv -and $Argv.Count -ge 2 -and $Argv[0] -eq 'docs' -and $Argv[1] -in @('sync', 'status')) {
        return 900
    }
    return 120
}

function Test-NeedsChangedSet {
    <#
      Which commands compute the working-tree changed set for themselves.

      `guard` has always done it. `gates` joins it because the two ask
      the same question of the same tree — which files did this ticket
      touch — and answering it twice, differently, is how the gate list
      and the guard end up describing different diffs.

      The repo is the only side that can answer: the board may be on
      another machine and has never seen this filesystem.
    #>
    param([string[]]$Argv)

    if (-not $Argv -or $Argv.Count -lt 1) { return $false }
    if ($Argv[0] -notin @('guard', 'gates')) { return $false }
    # An EXPLICIT set wins. `--file` is caught by the caller (it lands in
    # the file map), but `--text` is read board-side and would otherwise
    # be silently overridden by the working tree -- so someone debugging a
    # guard decision with `--text` got a verdict about a completely
    # different set of files and no hint that their input was discarded.
    if ($null -ne (Get-FlagValue -Argv $Argv -Name 'text')) { return $false }
    return $true
}

function Get-GuardTicketKey {
    <#
      The ticket key on a `zagent guard <KEY>`, or $null.

      Matched on the `-<digits>` suffix, the same rule /tick uses to tell
      a ticket key from a project key — digits ARE legal inside a project
      key, so "letters versus digits" is the wrong test. Anything that is
      not a ticket key is left for the board to reject by name rather
      than silently treated as an absent key: a typo that reads as "no
      key" would skip the gate-selection check and merge.
    #>
    param([string[]]$Argv)

    if (-not $Argv -or $Argv.Count -lt 2) { return $null }
    if ($Argv[0] -ne 'guard') { return $null }
    if ($Argv[1] -notmatch '-\d+$') { return $null }
    return $Argv[1]
}

function Get-BodyDrift {
    <#
      Which of a ticket body's citations no longer resolve in THIS repo.

      Every ticket body examined in one session had drifted -- seven for
      seven. One cited a file with zero matches, one quoted a hardcoded
      path that had already been parameterised, one was entirely already
      done. A body is a description of the past, and `master` moved.

      The board extracts the citations and cannot check them: it may be
      on another machine and has never seen this checkout. So the split
      is not a technicality -- resolving them is the one half only the
      client can do.

      ADVISORY. A ticket that says "add `Zenith_GroundQuery`" cites
      something that correctly does not exist yet, so an unresolved
      citation is a prompt to READ the body, never a refusal. What it
      replaces is "spot-check the ticket's central claim against the
      repo" in a protocol paragraph, which is the shape of instruction
      this loop keeps finding does not hold.

      Symbols go through `git grep` rather than a filesystem walk: git is
      already a hard dependency of every step of a tick, it searches only
      tracked files, and it is fast enough to run per symbol.
    #>
    param([string]$Repo, $Citations)

    if (-not $Repo -or -not $Citations) { return , @() }
    $missing = [System.Collections.Generic.List[object]]::new()

    $paths = $Citations.PSObject.Properties['paths']
    if ($paths) {
        foreach ($path in @($paths.Value)) {
            if (-not $path) { continue }
            if (Test-Path -LiteralPath (Join-Path $Repo $path)) { continue }
            # "It moved" is far commoner than "it never existed", and it
            # is the difference between a worker fixing a stale path and
            # a worker CREATING a second file at the cited location.
            # ZM-20 cites Tests/ZM_Tests_CommittedSceneBytes.cpp; the
            # file is real and lives under Games/Zenithmon/Tests/.
            $leaf = Split-Path $path -Leaf
            $elsewhere = @(& git -C $Repo ls-files -- "*/$leaf" $leaf 2>$null) |
                Where-Object { $_ } | Select-Object -First 1
            [void]$missing.Add([PSCustomObject]@{
                kind = 'path'; value = $path; movedTo = $elsewhere
            })
        }
    }

    $symbols = $Citations.PSObject.Properties['symbols']
    if ($symbols) {
        foreach ($symbol in @($symbols.Value)) {
            if (-not $symbol) { continue }
            & git -C $Repo grep --quiet --fixed-strings -- $symbol 2>$null
            if ($LASTEXITCODE -ne 0) {
                [void]$missing.Add([PSCustomObject]@{
                    kind = 'symbol'; value = $symbol; movedTo = $null
                })
            }
        }
    }

    # `,@(...)` or a one-element result unrolls to a bare object on return.
    return , @($missing)
}

function Get-CitationCount {
    <#
      How many citations the board actually EXTRACTED, split by whether
      this side can check them.

      `lines` is deliberately its own bucket. A line number is true only
      relative to a commit nobody recorded, so it goes stale on the next
      edit ABOVE it and no resolver can tell -- ZEN-5's body cited line
      522 for a specifier that was at 540 and the drift check was
      silent. Counting it as "checked" would be a lie; omitting it
      entirely would hide the one thing that needs a human's eyes.
    #>
    param($Citations)

    $count = { param($Field)
        if (-not $Citations) { return 0 }
        $prop = $Citations.PSObject.Properties[$Field]
        if (-not $prop) { return 0 }
        return @($prop.Value | Where-Object { $_ }).Count
    }
    $paths = & $count 'paths'
    $symbols = & $count 'symbols'
    $lines = & $count 'lines'
    return [PSCustomObject]@{
        paths     = $paths
        symbols   = $symbols
        lines     = $lines
        checkable = $paths + $symbols
        total     = $paths + $symbols + $lines
    }
}

function Format-BodyDrift {
    <#
      The drift report -- ALWAYS a string, never $null.

      ★ "NOTHING EXTRACTED" AND "NOTHING DRIFTED" USED TO BE THE SAME
      OUTPUT, WHICH WAS NO OUTPUT. Three consecutive claims came back
      with `citations: {paths:[], symbols:[], lines:[]}` and wrote no
      drift.txt, so at the call site an unparsed body was
      indistinguishable from a clean one. Meanwhile every body examined
      HAD drifted: ZM-27's Goal claimed there was "no way to USE an item
      at all today" while ZM_Bag, ZM_ItemData, ZM_UI_Bag, ZM_ShopLogic
      and item use in battle all existed, and the bag already persisted
      as save module 6. The check reported nothing and the reader took
      that for a pass.

      So the count leads every report. `0 citations extracted` and `4 of
      4 resolve` are different sentences now, and only the second is
      evidence of anything.

      Separate from Get-BodyDrift so a test can assert the WORDS without
      needing a repo: the sentence has to say "advisory" out loud, or the
      first unresolved citation on a legitimately forward-looking ticket
      teaches the reader to ignore the whole check.
    #>
    param([string]$Key, $Missing, $Citations)

    $rows = @($Missing)
    $n = Get-CitationCount -Citations $Citations
    $lines = @()

    if ($n.total -eq 0) {
        # The loud case. Nothing was checked, so nothing about this body
        # has been established either way.
        $lines += "$Key -- NO citations were extracted from this body, so NOTHING was checked."
        $lines += 'That is not a clean result. A body with no quoted path or symbol has not been'
        $lines += 'verified against anything -- read it against `master` yourself before inlining'
        $lines += 'it into a worker prompt. Every body examined in one session had drifted.'
        return ($lines -join [Environment]::NewLine)
    }

    $resolved = $n.checkable - $rows.Count
    if ($rows.Count -eq 0) {
        $lines += "$Key -- all $($n.checkable) checkable citation(s) resolve in this checkout."
    } else {
        $lines += "$Key -- $($rows.Count) of $($n.checkable) checkable citation(s) do not resolve here:"
        foreach ($row in $rows) {
            $line = "  $($row.kind)  $($row.value)"
            $moved = $row.PSObject.Properties['movedTo']
            if ($moved -and $moved.Value) { $line += "   (a file of that name is at $($moved.Value))" }
            $lines += $line
        }
        if ($resolved -gt 0) { $lines += "  ($resolved resolved.)" }
    }

    # Always, including on a clean report: it is the half the count above
    # cannot speak for.
    if ($n.lines -gt 0) {
        $lines += "  $($n.lines) line-number citation(s) -- NOT CHECKED, and uncheckable. A line"
        $lines += '  number is true only against a commit nobody recorded; open the file.'
    }

    $lines += 'A body describes the past. Re-read it against `master` before inlining it into a'
    $lines += 'worker prompt -- this is ADVISORY, since a ticket may legitimately name what it is'
    $lines += 'about to create.'
    return ($lines -join [Environment]::NewLine)
}

function Get-RecordedGateSelection {
    <#
      The `gates.json` a `zagent gates <KEY>` left in the run scratch, or
      $null when the step was never run.

      Returned raw rather than parsed. The board re-derives the selection
      from the ticket and the current changed set and compares — putting
      the comparison there rather than here keeps one implementation of
      "does this gate list still describe this diff", in the language
      that already owns the glob dialect and the category map.
    #>
    param([string]$Repo, [string]$Key)

    if (-not $Repo -or -not $Key) { return $null }
    $path = Join-Path (Join-Path (Get-ScratchRoot $Repo) $Key) 'gates.json'
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    return Get-Content -LiteralPath $path -Raw -Encoding utf8
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
    Get-ClientProject, Get-FlagValue, Test-Flag, Get-FileFlags, Get-FileContents, Get-WorkingTreeChanges,
    Test-NeedsDocsTree, Test-NeedsChangedSet, Get-RequestTimeout, Get-GuardTicketKey,
    Get-RecordedGateSelection,
    Get-BodyDrift, Format-BodyDrift, Get-CitationCount, Get-DocsTree,
    Get-ConventionsTree, Get-AmendContents, Get-ScratchRoot, Write-Results, Write-LastResult,
    Get-ClientChecks, Get-AllGateLines, Write-StdErr
