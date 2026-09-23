# tick_gates.ps1 -- run one ticket's gates, in the order that only costs one cycle.
# =============================================================================
# This is `/tick` step 6.2 as a MECHANISM. It used to be four paragraphs of
# prose in `Tools/zagent/protocol/TICK.md`, each added after the rule it describes
# was forgotten once -- which is the defect this repo keeps naming: a rule in
# prose in front of a check IS the defect. Every paragraph below became code.
#
#   1. REGEN WHEN A FILE WAS CREATED, AND TREAT A NON-ZERO REGEN AS FATAL.
#      Sharpmake bakes an explicit <ClCompile> list into the vcxproj at
#      generation time, so a new .cpp enters the build only at a regen. Skip it
#      and the build passes GREEN with the ticket's deliverable never compiled
#      -- measured on ZM-27: three new files, build exit 0, zero occurrences of
#      the new TU in the log. regen.ps1 exits 3 and regenerates NOTHING while
#      the stale projects stay on disk, so its failure is invisible downstream.
#
#   2. ASSERT EVERY CREATED .cpp BY NAME IN THE BUILD LOG. A green build is not
#      evidence a file was compiled; it is evidence that whatever WAS in the
#      project compiled. Nothing else catches this: the unit count still moves,
#      by however many tests landed in files that already existed, which is
#      indistinguishable from an ordinary baseline bump -- and the pin bump then
#      ratchets the broken state in permanently.
#
#   3. RUN THE UNIT GATE ALONE FIRST. The pin narration cannot be WRITTEN until
#      the count has been OBSERVED, and observing it means running a gate -- so
#      the doc edit necessarily lands after a gate run, and the "gates are the
#      last thing before the guard" rule then demands a full re-run. ZM-27 spent
#      ~25 minutes re-certifying three comment edits that only doc_lint reads.
#      Measuring first turns two cycles into one.
#
#   4. NEVER LET A FOREGROUND TIMEOUT CUT A GATE OFF. `zenith test Zenithmon
#      --headless` runs past 20 minutes. A killed gate gives you no exit code --
#      so you cannot tell a timeout from a failure -- and leaves the game exe
#      alive holding build outputs, which fails the NEXT build for an unrelated
#      reason. Every line here is started as a child process this script owns
#      and waits on unbounded, and the exe sweep runs on every exit path.
#
# Usage:
#   pwsh -NoProfile -File Tools/tick_gates.ps1 <TICKET_KEY> -Phase measure
#   pwsh -NoProfile -File Tools/tick_gates.ps1 <TICKET_KEY> -Phase verify
#   pwsh -NoProfile -File Tools/tick_gates.ps1 <TICKET_KEY>            # both
#
# ** RUN IT BACKGROUNDED. It owns processes that outlive any 10-minute tool
# timeout; it streams to .zagent/run/<KEY>/gates.log as it goes, so progress is
# readable without waiting, and writes .zagent/run/<KEY>/tick_gates.json at the
# end for a caller that wants the result rather than the transcript.
#
# Exit: 0 all green - 2 regen failed - 3 a created file was never compiled
#       4 a gate line failed - 5 could not read the gate list
#       8 a phase ran ZERO gate lines -- see Write-Result's own guard below.
#         The Tooling category's 12 gate lines match neither the build-line
#         nor the unit-line regex `measure` classifies its work from, so both
#         arrays came back empty, nothing was ever run, and the code fell
#         through to `Write-Result 0 'all gates green'` having executed
#         nothing. tick.md designates this JSON as the trustworthy channel --
#         explicitly OVER the shell exit code -- so Write-Result refuses to
#         let a $Code of 0 leave here with an empty steps array; it rewrites
#         the exit code to 8 and says so in the verdict instead.
#         NOT 6: this script's own ladder had 6 free, but `zagent`'s ladder
#         (root CLAUDE.md; branched on throughout Tools/zagent/protocol/TICK.md) is
#         0/1/3/4/5/6/7, and 6 there means CIRCUIT BREAKER OPEN -- a tick reads
#         both ladders, often in the same step, and a shared number meaning
#         "nothing ran" from one and "the loop is thrashing, stop" from the
#         other is exactly the kind of disagreement "exit codes are the
#         interface -- branch on them, not on the text" is supposed to
#         prevent. 8 is free in BOTH ladders.
#       10 MEASURED: the unit pin needs bumping (not a failure; see below)
#       11 slot busy -- the machine-wide build slot (Tools/build_lock.ps1,
#          Local\ZenithBuildSlot) could not be acquired within its wait
#          budget. Non-evidential, like 8: nothing about THIS ticket's gates
#          failed, another checkout is simply mid-build. A lane-mode caller
#          treats this as `next.json` = wait, not a failure block.
#       12 tree unstable: non-deterministic tracked output. Verify's working
#          tree changed (treeShaStart != treeShaEnd) on BOTH the first pass
#          and the one automatic retry -- see the treeShaStart/treeShaEnd/
#          treeRetried fields in tick_gates.json. A single mismatch is not
#          this: a gate line legitimately rewriting a tracked generated file
#          (Zenith/Flux/Shaders/Generated/*.h) or re-authoring a .zscen is
#          handled by re-running verify once, silently, with treeRetried set.
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory, Position = 0)][string]$Key,

    # measure -- regen if needed, build, prove the new TUs compiled, run the
    #            unit gate ALONE and report the count. Stops before the slow
    #            suite so the pin and the Status.md narration can be written.
    # verify  -- re-derive the gate list from the CURRENT diff and run every
    #            line, in order. This is the run that certifies the commit.
    # all     -- both, back to back. Correct only when no doc edit is coming.
    [ValidateSet('measure', 'verify', 'all')][string]$Phase = 'all',

    # Skip `zagent gates $Key`'s shell-out and read the ticket's OWN already-
    # recorded $runDir/gates.json instead, at BOTH call sites (measure's
    # initial read and verify's re-derive). This is lane REGATE (plan
    # mechanics 3.4): the selection has to stay the one the ticket was
    # originally claimed against, not "every category whose paths a
    # merge-with-master diff now touches". Missing/empty gates.json falls
    # through to the same exit-5 path as a failed `zagent gates` call.
    [switch]$ReuseGateList,

    [string]$RepoRoot,

    # A per-line watchdog, off by default. The whole point of this script is
    # that gates are not cut off; set it only when a gate is suspected wedged.
    [int]$TimeoutMinutes = 0,

    # Define the functions and return without running anything. This is how
    # Tools/Test_TickGates.ps1 reaches the pure helpers: dot-sourcing a script
    # RUNS it, and a test that claims a gate ran is the one thing this file
    # exists to prevent.
    [switch]$NoRun
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($RepoRoot)) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
$script:RepoRoot = $RepoRoot

# Enter-ZenithBuildSlot / Exit-ZenithBuildSlot. Dot-sourcing this defines
# functions only -- nothing runs merely by reaching this line, -NoRun
# included, which is why it happens unconditionally at the top rather than
# behind the `if ($NoRun) { return }` gate further down. $PSScriptRoot here
# is THIS file's own directory (Tools/), same as build_lock.ps1's own -- both
# scripts live beside each other.
. (Join-Path $PSScriptRoot 'build_lock.ps1')

$runDir = Join-Path $RepoRoot ".zagent/run/$Key"
$logPath = Join-Path $runDir 'gates.log'
$resultPath = Join-Path $runDir 'tick_gates.json'
if (-not $NoRun -and -not (Test-Path -LiteralPath $runDir)) {
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
}

$script:Steps = New-Object System.Collections.Generic.List[object]

# The build-slot handle from Enter-ZenithBuildSlot, set inside the try block
# below. Initialised to $null here so the outermost `finally` can safely call
# Exit-ZenithBuildSlot on it even on the (unreachable in practice, but never
# assumed) chance something throws before the acquire line runs -- see
# Exit-ZenithBuildSlot's own contract: it is a no-op for $null.
$script:BuildSlotHandle = $null

# ★★ PROVENANCE. A verdict with no commit and no state is indistinguishable from
# a stale one left by an earlier attempt -- and I6 points a crash-resumed firing
# straight at this directory and tells it to trust what it finds. Measured on one
# ticket: this file held `all gates green` from a DIFFERENT tree at claim time,
# and a superseded `exit 4` while a later phase was mid-run. Both read exactly
# like the truth.
#
# So: stamp WHICH tree and WHEN, and write a `running` record at START. A reader
# now has three distinguishable states -- `running` (re-run it), a headSha that
# does not match HEAD (ignore it), or a verdict it can trust.
$script:HeadSha = ''
try {
    $sha = & git -C $RepoRoot rev-parse HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($sha)) {
        $script:HeadSha = ([string]$sha).Trim()
    }
} catch { }
$script:StartedUtc = (Get-Date).ToUniversalTime().ToString('o')

# The lines THIS phase intends to run. `skipped` is derived from it, because a
# gate list that stops at the first failure leaves the rest un-run and the steps
# array simply ENDS -- absence was an inference, not data.
$script:PlannedLines = New-Object System.Collections.Generic.List[string]

# Lines this phase DELIBERATELY does not run, as opposed to lines a failure cut
# off. `measure` runs the build and the unit gate only (rule 3 above), so on a
# clean measure `skipped` is legitimately empty while HALF THE GATE LIST has not
# run -- and tick.md tells the reader to "read `skipped` before concluding
# anything about what was verified". Following that literally at measure is how a
# subset run gets mistaken for a full one. `skipped` keeps meaning "a failure
# stopped the list"; this is the other reason a line did not run.
$script:DeferredLines = New-Object System.Collections.Generic.List[string]

# A RED gate line and a FLAKY one look identical from here -- exit code only
# -- and the Tools/Vault Vitest suite has known intermittent failures on
# Windows, so a Tooling ticket has a real chance of a red it did not cause.
# That red still makes the tick a failure block and still counts toward
# `maxConsecutiveBlocked`. This is the PRODUCER half only: every entry below
# is evidence for whatever reads it (a separate consumer decides what a
# `{firstExit: 4, rerunExit: 0}` pair means for the block count), and the
# CONTRACT is the field name and the three keys per entry -- do not rename
# either. The overall verdict never moves because of this array: the line
# that went red is already recorded red in $script:Steps by the time a retry
# is even attempted, and every call site still branches on that FIRST
# result. See Invoke-GateLineFlakeAware below for why a `zenith.ps1 build`
# line is excluded (a build failure is a different class, and re-running one
# to rule out a flake costs minutes it is not worth spending).
$script:Flakes = New-Object System.Collections.Generic.List[object]

# The per-game registry count Get-AutomatedRegistryCount derives, below --
# the field Write-Result emits into tick_gates.json as `registry`. An ordered
# MAP, not a bare scalar, because a phase can measure more than one game's
# unit gate (see the `deferred` field's own reasoning for why this is a map
# keyed the same way). Populated only inside measure's registry-count loop
# near the bottom of this file; left at its initial empty map on every other
# exit path -- a `verify`-only run, and every one of measure's OWN early
# exits (regen failure, build failure, the uncompiled-file checks, a real
# unit failure) -- so the JSON key is present UNCONDITIONALLY rather than
# vanishing on a run that never reached the loop.
$script:RegistryCounts = [ordered]@{}

# Freshness = verified tree (plan "Integration mechanics" 1). `git write-tree`
# of the working tree, taken before the first gate line and after the last,
# via Get-WorkingTreeSha below. Populated by BOTH phases: measure's pair is
# informational only (nothing branches on it); verify's pair is what
# `integrate` later compares against the landed commit's own tree, and a
# mismatch there drives the one-time automatic re-verify (see the VERIFY
# phase body). Left at $null until a phase that runs gate lines computes
# them, same "field present even when unpopulated" rule as `registry` above.
$script:TreeShaStart = $null
$script:TreeShaEnd = $null
# Set only by the verify retry path -- true iff the tree changed between
# verify's first pass and its second. Never set by measure, and never
# cleared back to $false once verify has retried once in this invocation.
$script:TreeRetried = $false

# The automated-test registry count, derived EXACTLY as doc_lint.ps1 check C7
# derives it -- the paren is load-bearing. A bare `grep -c
# ZENITH_AUTOMATED_TEST_REGISTER` also counts the paren-less mentions in comments
# (three of them live in ZM_AutoTests_TrainerSight.cpp) and over-reports by that
# many. Status.md's LIVE PIN block carries this number beside the boot pin and C7
# asserts BOTH, but only the boot pin comes out of a gate run -- the registry
# count comes from `zenith test`, which `measure` does not run. So a phase whose
# whole job is "observe the numbers before you narrate them" was handing over one
# of the two. This closes that.
function Get-AutomatedRegistryCount {
    # -RepoRoot is a parameter, not the script-scoped variable -- same
    # reasoning as Get-CreatedSourceFiles's own comment above: a helper only
    # testable against the live checkout is a helper nobody tests. Defaults
    # to $script:RepoRoot so the one in-script call site
    # (`Get-AutomatedRegistryCount $g`) is unaffected.
    param([string]$Game, [string]$RepoRoot = $script:RepoRoot)
    $testsDir = Join-Path $RepoRoot "Games/$Game/Tests"
    if (-not (Test-Path -LiteralPath $testsDir)) { return $null }
    $n = 0
    Get-ChildItem -LiteralPath $testsDir -Filter '*.cpp' -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        $raw = Get-Content -LiteralPath $_.FullName -Raw -ErrorAction SilentlyContinue
        if ($null -ne $raw) {
            $n += ([regex]::Matches($raw, 'ZENITH_AUTOMATED_TEST_REGISTER\s*\(')).Count
        }
    }
    return $n
}

function Write-Line([string]$Text, [string]$Colour = 'Gray') {
    Write-Host $Text -ForegroundColor $Colour
    Add-Content -LiteralPath $logPath -Value $Text -Encoding utf8
}

function Write-Banner([string]$Text) {
    Write-Line ''
    Write-Line ("=" * 78)
    Write-Line "[tick_gates] $Text"
    Write-Line ("=" * 78) 'Cyan'
}

# --- The exe sweep -----------------------------------------------------------
#
# Derived from Games/*, not from a hard-coded list: a game added later would
# otherwise be swept by nothing, and the symptom is a build failing on a locked
# output file with no connection to the change that caused it. Step 9 of the
# tick has a sweep too, and it is far too late -- it runs after the gates.
#
# NAME ALONE used to be the whole criterion, and lane mode makes that wrong:
# a ZM lane's tick_gates would kill a `zenithmon.exe` a DIFFERENT lane (or a
# human) is running out of `C:\dev\Zenith-lanes\ZM`. Select-SweepTargets adds
# a PATH scope -- name match AND the process's own .Path under -RepoRoot --
# factored out as a pure function so it is testable without ever actually
# killing anything (see Tools/Test_TickGates.ps1's own warning on that).
function Select-SweepTargets {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$Processes,
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]]$Names
    )
    $normRoot = ($RepoRoot -replace '/', '\').TrimEnd('\')
    $prefixedRoot = $normRoot + '\'
    $result = New-Object System.Collections.Generic.List[object]
    foreach ($p in $Processes) {
        $name = $null
        try { $name = $p.Name } catch { $name = $null }
        if ([string]::IsNullOrEmpty($name)) { continue }
        if ($Names -notcontains $name.ToLowerInvariant()) { continue }

        # $null/inaccessible .Path -> SKIP, never kill. A protected or
        # cross-session process can throw merely reading .Path (access
        # denied), and "cannot prove where it lives" must resolve to "leave
        # it alone", not "kill it because the name matched".
        $path = $null
        try { $path = $p.Path } catch { $path = $null }
        if ([string]::IsNullOrEmpty($path)) { continue }

        $normPath = ($path -replace '/', '\')
        # Prefix on a DIRECTORY BOUNDARY, case-insensitive (Windows paths):
        # `C:\dev\Zenith` must not match `C:\dev\Zenith-lanes\ZM` -- a bare
        # StartsWith without the trailing separator would.
        $isRootItself = $normPath.Equals($normRoot, [System.StringComparison]::OrdinalIgnoreCase)
        $isUnderRoot = $normPath.StartsWith($prefixedRoot, [System.StringComparison]::OrdinalIgnoreCase)
        if ($isRootItself -or $isUnderRoot) { $result.Add($p) }
    }
    # `.ToArray()` via the leading comma -- see the note above Write-Result:
    # `@($list)` on a List[object] throws regardless of what it holds.
    return , $result.ToArray()
}

function Invoke-ExeSweep {
    param([string]$RepoRoot = $script:RepoRoot)
    $names = @(Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'Games') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Name.ToLowerInvariant() })
    if ($names.Count -eq 0) { return }
    $targets = Select-SweepTargets -Processes @(Get-Process -ErrorAction SilentlyContinue) -RepoRoot $RepoRoot -Names $names
    $killed = @()
    foreach ($p in $targets) {
        try { $p.Kill(); $killed += "$($p.ProcessName) (pid $($p.Id))" } catch { }
    }
    if ($killed.Count -gt 0) { Write-Line "[sweep] killed stray game exe(s): $($killed -join ', ')" 'Yellow' }
}

# --- Running one gate line ---------------------------------------------------
#
# The line is written to a wrapper script and run through pwsh rather than being
# split into FilePath + ArgumentList. Splitting means re-implementing PowerShell
# quoting, and a gate line carrying a quoted argument (the cleanup line does)
# would be mangled in a way that silently changes what ran. A wrapper is parsed
# by the same shell that would have parsed it typed.
function Invoke-GateLine {
    param([string]$Line, [string]$Label)

    $wrapper = Join-Path $runDir ('gate_' + [Math]::Abs($Line.GetHashCode()) + '.ps1')
    $body = @(
        '$ErrorActionPreference = ''Continue''',
        $Line,
        'exit $LASTEXITCODE'
    ) -join [Environment]::NewLine
    Set-Content -LiteralPath $wrapper -Value $body -Encoding utf8

    $outPath = "$wrapper.out"
    Write-Line ''
    Write-Line "[gate] $Label" 'Cyan'
    Write-Line "       $Line"
    $started = Get-Date

    $proc = Start-Process -FilePath 'pwsh' `
        -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $wrapper) `
        -WorkingDirectory $script:RepoRoot `
        -NoNewWindow -PassThru -RedirectStandardOutput $outPath -RedirectStandardError "$outPath.err"

    # Unbounded by default. A gate that is merely SLOW must not be killed: a
    # killed gate reports no exit code at all, which is strictly worse than a
    # late one because it cannot be told apart from a failure.
    if ($TimeoutMinutes -gt 0) {
        if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
            Write-Line "[gate] WATCHDOG: exceeded $TimeoutMinutes min; killing" 'Red'
            try { & taskkill /T /F /PID $proc.Id 2>&1 | Out-Null } catch { }
            Invoke-ExeSweep
            return [PSCustomObject]@{ Line = $Line; Label = $Label; ExitCode = 124; Output = ''; Seconds = 0 }
        }
    }
    else {
        $proc.WaitForExit()
    }

    $text = ''
    foreach ($f in @($outPath, "$outPath.err")) {
        if (Test-Path -LiteralPath $f) { $text += (Get-Content -LiteralPath $f -Raw -ErrorAction SilentlyContinue) }
    }
    $seconds = [Math]::Round(((Get-Date) - $started).TotalSeconds, 1)
    Add-Content -LiteralPath $logPath -Value $text -Encoding utf8
    $code = $proc.ExitCode
    Write-Line "[gate] $Label -> exit $code (${seconds}s)" $(if ($code -eq 0) { 'Green' } else { 'Red' })

    $step = [PSCustomObject]@{ Line = $Line; Label = $Label; ExitCode = $code; Output = $text; Seconds = $seconds }
    $script:Steps.Add($step)
    return $step
}

# The ONE place the build-line shape is matched. Used both to classify
# $buildLines below and by the flake retry, which must never re-run a build --
# two separate regexes here is how they drift apart.
function Test-IsZenithBuildLine {
    param([string]$Line)
    return $Line -match 'zenith\.ps1["'']?\s+build\b'
}

# --- Flake-tolerant gate line -------------------------------------------------
#
# Wraps Invoke-GateLine: on a RED result for a line that is NOT a
# `zenith.ps1 build` line, re-runs that ONE line once more on the same tree
# and records both exit codes in $script:Flakes (see its header comment for
# the contract). This is evidence only. The VERDICT this phase reports does
# not change: the caller still branches on $step, the FIRST invocation's
# result -- Invoke-GateLine already appended that as the line's one entry in
# $script:Steps before this function's retry check even runs, and the retry's
# own step is simply a second entry alongside it, never a replacement.
function Invoke-GateLineFlakeAware {
    param([string]$Line, [string]$Label)
    $step = Invoke-GateLine -Line $Line -Label $Label
    if ($step.ExitCode -ne 0 -and -not (Test-IsZenithBuildLine $Line)) {
        Write-Banner "RED -- re-running once to rule out a flake ($Label)"
        $rerun = Invoke-GateLine -Line $Line -Label "$Label (flake re-run)"
        $script:Flakes.Add([PSCustomObject]@{ line = $Line; firstExit = $step.ExitCode; rerunExit = $rerun.ExitCode })
        if ($rerun.ExitCode -eq 0) {
            Write-Line "[flake] SECOND run was green -- recorded as a possible flake, verdict for this line stays RED (first run's exit $($step.ExitCode))" 'Yellow'
        }
        else {
            Write-Line "[flake] second run was ALSO red (exit $($rerun.ExitCode)) -- looks deterministic, not a flake" 'Yellow'
        }
    }
    return $step
}

# ** NEVER WRITE `@($list)` FOR A List[object] -- USE `.ToArray()`.
#
#   $L = New-Object System.Collections.Generic.List[object]; $L.Add('x')
#   @($L)            -> "Argument types do not match"     (pwsh 7.6.5)
#   $L.ToArray()     -> OK
#   @($L)  where $L is List[STRING]                       -> OK
#
# The array subexpression resolves the IList indexer against List<object> and
# fails, whatever the list actually holds -- so the habit of defensively
# wrapping a collection in @() is the thing that breaks. It surfaced here as a
# bare one-line error with no location, on the FAILURE path only, which is the
# worst place for a script the loop branches on exit codes from: the gate had
# genuinely failed and the report of it crashed. The catch block below now
# names the file and line for exactly this reason.
function Write-Result([int]$Code, [string]$Verdict, $Extra) {
    # Every planned line that never produced a step. When a gate list stops at
    # the first failure the remainder is silently absent -- on one ticket that
    # meant the unit gate and doc_lint never ran, so a freshly-bumped pin was
    # unconfirmed and the linter had never seen the edited docs, and nothing
    # said so. Now it is a field.
    $ran = @($script:Steps.ToArray() | ForEach-Object { $_.Line })
    $skipped = @($script:PlannedLines.ToArray() | Where-Object { $ran -notcontains $_ })

    # ★★ A GREEN VERDICT WITH ZERO STEPS IS THE JSON LYING BY OMISSION.
    # tick.md designates THIS file as the trustworthy channel -- explicitly
    # OVER the shell exit code -- so a caller trusts `exitCode: 0` / a green
    # verdict string without re-deriving it from `steps`. The Tooling
    # category's gate lines match neither the build-line nor the unit-line
    # regex `measure` classifies its work from (see Test-IsZenithBuildLine and
    # the unitLines regex above), so both arrays came back empty, nothing was
    # ever run, and the code still reached `Write-Result 0 'all gates green'`
    # at the bottom of the script. Refuse it HERE, once, rather than trusting
    # every call site -- present and future -- to check first. Scoped to
    # $Code -eq 0 only: a non-zero code with zero steps (e.g. 5, "could not
    # read the gate list") is already an honest red and needs no rewriting.
    #
    # 8, NOT 6: this file's own exit ladder (0/2/3/4/5/10) had 6 free, but
    # `zagent`'s SEPARATE ladder -- root CLAUDE.md, branched on throughout
    # Tools/zagent/protocol/TICK.md -- already spends 6 on CIRCUIT BREAKER OPEN. A
    # tick reads both ladders, often in the same step, so reusing 6 here would
    # make the same number mean "nothing ran" from this script and "the loop
    # is thrashing, stop" from zagent -- exactly the ambiguity "exit codes are
    # the interface -- branch on them, not on the text" exists to rule out. 8
    # is free in both (zagent's is 0/1/3/4/5/6/7).
    if ($Code -eq 0 -and $script:Steps.Count -eq 0) {
        $Verdict = "measure ran no gate lines -- '$Verdict' would have been reported having executed nothing; refusing to call that green"
        $Code = 8
    }

    $payload = [ordered]@{
        key         = $Key
        phase       = $Phase
        state       = 'complete'
        exitCode    = $Code
        verdict     = $Verdict
        headSha     = $script:HeadSha
        startedUtc  = $script:StartedUtc
        finishedUtc = (Get-Date).ToUniversalTime().ToString('o')
        steps       = @($script:Steps.ToArray() | ForEach-Object {
                [PSCustomObject]@{ label = $_.Label; line = $_.Line; exitCode = $_.ExitCode; seconds = $_.Seconds }
            })
        skipped     = $skipped
        deferred    = @($script:DeferredLines.ToArray())
        # Evidence only -- see $script:Flakes' own header comment for the
        # contract and why the verdict above never depends on this array.
        flakes      = @($script:Flakes.ToArray())
        # Per-game automated-test registry count (Get-AutomatedRegistryCount),
        # keyed by game name. ★ Emitted UNCONDITIONALLY, even as an empty map
        # -- same reasoning as `archivedPriorRun: false` beating an absent
        # field: a reader cannot tell "no games measured" from "an older
        # script that never wrote this" if the key simply vanishes on a run
        # that never reached the registry-count loop.
        registry    = $script:RegistryCounts
        # Freshness = verified tree (see $script:TreeShaStart's own header
        # comment). Present unconditionally, same "field over absence" rule
        # as `registry` -- a run that never reached a tree-sha computation
        # (an early exit-5 with no gate list) still emits the key, as $null.
        treeShaStart = $script:TreeShaStart
        treeShaEnd   = $script:TreeShaEnd
        treeRetried  = $script:TreeRetried
    }
    # Extra can override anything above, INCLUDING state -- the internal-error
    # catch block passes state = 'error' this way, because 'complete' on a
    # thrown-exception path is exactly the lie this whole function exists to
    # stop telling (a crashed run used to report state: complete, green steps,
    # and skipped: [] all at once). Tools/Vault/cli/tick-checks.mjs's
    # gateVerdictFresh already treats any state other than 'complete' as not
    # fresh -- the same bucket a killed 'running' run falls into -- so 'error'
    # does not need a new branch on that side; it trips the existing one
    # correctly for the first time.
    if ($null -ne $Extra) { foreach ($k in $Extra.Keys) { $payload[$k] = $Extra[$k] } }
    Set-Content -LiteralPath $resultPath -Value ($payload | ConvertTo-Json -Depth 6) -Encoding utf8
    Write-Line ''
    Write-Line "[tick_gates] $Verdict (exit $Code)" $(if ($Code -eq 0) { 'Green' } else { 'Red' })
    Write-Line "[tick_gates] transcript: $logPath"
    Write-Line "[tick_gates] result:     $resultPath"
}

# --- The gate list -----------------------------------------------------------
#
# From `zagent gates <KEY>`, never from zagent.project.json by hand. The command
# unions in every category whose declared `paths` the CURRENT diff reached, and
# RECORDS the selection in gates.json -- which `zagent guard <KEY>` later
# re-derives and compares against. Re-deriving it here by hand would produce a
# recording that no longer describes the diff, which guard reports as a failure.
#
# -ReuseGateList (lane REGATE, plan mechanics 3.4): skip the `zagent gates`
# shell-out entirely and read the ticket's OWN already-recorded gates.json.
# The selection this reuses is the ticket's own claim-time gate list, not
# "everything a merge-with-master diff now touches" -- re-deriving after a
# lane-side `merge master` could silently narrow OR widen it depending on
# what the merge changed, and REGATE's whole point is running the SAME gates
# again on the merged tip.
function Get-GateList {
    param([switch]$ReuseGateList)

    $gatesJson = Join-Path $runDir 'gates.json'

    if ($ReuseGateList) {
        Write-Banner "reusing recorded gate list (-ReuseGateList): $gatesJson"
        if (-not (Test-Path -LiteralPath $gatesJson)) {
            Write-Line "[tick_gates] -ReuseGateList set but no gates.json at $gatesJson" 'Red'
            return $null
        }
        $record = Get-Content -LiteralPath $gatesJson -Raw -Encoding utf8 | ConvertFrom-Json
        $lines = @($record.gates)
        if ($lines.Count -eq 0) {
            Write-Line '[tick_gates] the gate list is EMPTY. An empty gate list can never merge.' 'Red'
            return $null
        }
        foreach ($l in $lines) { Write-Line "  $l" }
        return $lines
    }

    Write-Banner "zagent gates $Key"
    & zagent gates $Key 2>&1 | ForEach-Object { Write-Line "  $_" }
    $rc = $LASTEXITCODE
    if ($rc -ne 0) {
        Write-Line "[tick_gates] 'zagent gates $Key' exited $rc" 'Red'
        return $null
    }
    if (-not (Test-Path -LiteralPath $gatesJson)) {
        Write-Line "[tick_gates] no gates.json at $gatesJson" 'Red'
        return $null
    }
    $record = Get-Content -LiteralPath $gatesJson -Raw -Encoding utf8 | ConvertFrom-Json
    $lines = @($record.gates)
    # An empty gate list can never merge -- step 6 of the tick says so, and a
    # list of zero commands is trivially green, which is the worst possible way
    # for a gate to be wrong.
    if ($lines.Count -eq 0) {
        Write-Line '[tick_gates] the gate list is EMPTY. An empty gate list can never merge.' 'Red'
        return $null
    }
    return $lines
}

# --- The working-tree SHA (freshness = verified tree) ------------------------
#
# `git write-tree` of the CURRENT working tree, without ever touching the
# real index: the real index file (`git rev-parse --git-path index` -- a
# RELATIVE `.git/index` in a main checkout, an ABSOLUTE
# `.../worktrees/<name>/index` in a linked worktree, brief §"Integration
# mechanics" 1) is copied to a throwaway temp file, `GIT_INDEX_FILE` is
# pointed at the copy for the duration of `add -A` + `write-tree`, and the
# prior `GIT_INDEX_FILE` is restored in a `finally` regardless of outcome.
# Copying the real index (rather than starting from an empty one) keeps its
# stat cache and CRLF-in-index protection and avoids re-hashing every tracked
# file -- the same reasoning the plan gives for this exact technique.
function Get-WorkingTreeSha {
    param([string]$RepoRoot = $script:RepoRoot)

    $indexPathOut = & git -C $RepoRoot rev-parse --git-path index 2>$null
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($indexPathOut)) { return $null }
    $indexPathOut = ([string]$indexPathOut).Trim()
    $realIndex = if ([System.IO.Path]::IsPathRooted($indexPathOut)) { $indexPathOut } else { Join-Path $RepoRoot $indexPathOut }

    $tempIndex = Join-Path ([System.IO.Path]::GetTempPath()) ('tick_gates_index_' + [guid]::NewGuid().ToString('N'))
    if (Test-Path -LiteralPath $realIndex) {
        try { Copy-Item -LiteralPath $realIndex -Destination $tempIndex -Force } catch { return $null }
    }
    # else: no index has ever been written (a brand-new repo before any `git
    # add`) -- a GIT_INDEX_FILE pointing at a path that does not yet exist is
    # a legal empty index to git, and `add -A` below creates it there.

    $priorIndexEnv = $env:GIT_INDEX_FILE
    try {
        $env:GIT_INDEX_FILE = $tempIndex
        & git -C $RepoRoot add -A 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) { return $null }
        $tree = & git -C $RepoRoot write-tree 2>$null
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($tree)) { return $null }
        return ([string]$tree).Trim()
    }
    catch {
        return $null
    }
    finally {
        if ($null -eq $priorIndexEnv) {
            Remove-Item -Path Env:\GIT_INDEX_FILE -ErrorAction SilentlyContinue
        }
        else {
            $env:GIT_INDEX_FILE = $priorIndexEnv
        }
        Remove-Item -LiteralPath $tempIndex -Force -ErrorAction SilentlyContinue
    }
}

# --- Created source files ----------------------------------------------------
#
# `git status --porcelain` rather than `git diff --name-only`: diff reports only
# tracked MODIFICATIONS, so a file the worker CREATED never reaches it. That
# spelling failed OPEN in `guard` for two green ticks before a file-creating
# ticket exposed it, and it is the same mistake here with a worse consequence.
function Get-CreatedSourceFiles {
    # -RepoRoot is a parameter, not the script-scoped variable, so
    # Tools/Test_TickGates.ps1 can point it at a throwaway git fixture. A helper
    # only testable against the live checkout is a helper nobody tests.
    param([string]$RepoRoot = $script:RepoRoot)
    $out = @(& git -C $RepoRoot status --porcelain 2>$null)
    $created = New-Object System.Collections.Generic.List[object]
    foreach ($row in $out) {
        if ([string]::IsNullOrWhiteSpace($row)) { continue }
        $status = $row.Substring(0, 2)
        $path = $row.Substring(3)
        # A rename is reported as `R  old.cpp -> new.cpp` on ONE line. Taking
        # the whole remainder gives a path that exists nowhere, and it still
        # ends in .cpp, so it passes the extension filter and lands in the
        # report as garbage. The DESTINATION is the file that has to compile.
        if ($path -match '\s->\s') { $path = ($path -split '\s->\s', 2)[1] }
        $path = $path.Trim().Trim('"')
        # '??' untracked, 'A ' staged-new, ' A'/'AM' partially staged.
        # 'D'/'R' too: a DELETED or RENAMED source also leaves the vcxproj
        # describing a file list that no longer exists. That one at least fails
        # LOUDLY (the compiler cannot open it), so it is folded in here for the
        # regen decision only -- the by-name assertion below applies to created
        # files, which is where the silent failure lives.
        if ($status -notmatch '[ADR?]') { continue }
        if ($path -notmatch '\.(cpp|cc|cxx)$') { continue }
        # A rename's destination is a NEW translation unit as far as the
        # vcxproj is concerned -- it has never been in the file list -- so it
        # gets the by-name build assertion like any other created file.
        $created.Add([PSCustomObject]@{ Path = $path; IsNew = ($status -match '[A?R]') })
    }
    # `.ToArray()`, not `@($created)` -- see the note above Write-Result.
    return , $created.ToArray()
}

function Get-CreatedIncludeFiles {
    # ** A CREATED .inl IS NOT A TRANSLATION UNIT, SO THE BY-NAME BUILD-LOG
    # ASSERTION CANNOT SEE IT -- AND THIS REPO'S UNIT SUITES ARE .inl FILES.
    #
    # Every Flux module hosts its tests in `<Module>.Tests.inl`, #included
    # from the bottom of its .cpp (Flux_HiZ.cpp:315, Flux_Decals.cpp:683, and
    # ~20 more). Such a file needs no regen -- it is not its own TU -- so the
    # extension filter above correctly leaves it out of the regen decision.
    # But it was left out of the PROOF as well, and that is the same silent
    # hole the .cpp assertion exists to close: create a .Tests.inl, forget the
    # #include, and the build is green, the unit count does not move, and
    # nothing says a word. Measured on ZEN-40, where tick_gates printed "no
    # created source files; skipping regen" while a new .Tests.inl sat in its
    # own `changed` list.
    #
    # The compiler cannot answer this one, so the check is static: something
    # tracked must #include the file.
    param([string]$RepoRoot = $script:RepoRoot)
    $out = @(& git -C $RepoRoot status --porcelain 2>$null)
    $created = New-Object System.Collections.Generic.List[string]
    foreach ($row in $out) {
        if ([string]::IsNullOrWhiteSpace($row)) { continue }
        $status = $row.Substring(0, 2)
        $path = $row.Substring(3)
        if ($path -match '\s->\s') { $path = ($path -split '\s->\s', 2)[1] }
        $path = $path.Trim().Trim('"')
        if ($status -notmatch '[A?R]') { continue }
        if ($path -notmatch '\.inl$') { continue }
        $created.Add($path)
    }
    return , $created.ToArray()
}

# --- Proving a created TU was actually compiled -------------------------------
#
# Replaces the by-name build-log grep as the AUTHORITY (that grep is kept as a
# secondary, informational signal at the call site). It only ever proved a
# COLD build: MSBuild echoes a TU on the build that recompiles it, and an
# INCREMENTAL build that finds a created file's .obj already up to date names
# nothing, so the grep reported "not compiled" for a file that provably
# reached the compiler and whose .obj sits on disk, already linked into the
# .lib/.exe. `measure` is re-run routinely -- the protocol mandates it after
# any post-measure edit -- so an incremental second run is the COMMON case,
# not an edge one. The .obj's presence under the project's own
# IntermediatePath is idempotent: checking it does not care whether the
# compile happened moments ago or three builds back.

function Resolve-GateConfigLeaf {
    # The MSBuild /p:Configuration a `zenith.ps1 build` gate line resolves to,
    # LOWERCASED -- Sharpmake lowercases the config name to form the
    # output/intermediate directory leaf. Build/zenith_buildsystem.psm1's
    # ConvertTo-ZenithOutputDir is the ONE other place that fact lives; this
    # does not re-derive it, it names the same three configs zenith.ps1's own
    # flags select (zenith_config.psd1: HeadlessConfigWin64 / DefaultConfigWin64)
    # and lowercases the same way.
    param([string]$Line)
    if ($Line -match '--headless\b') { return 'null_vs2022_debug_win64_true' }
    if ($Line -match '--config\s+(\S+)') { return $Matches[1].ToLowerInvariant() }
    return 'vulkan_vs2022_debug_win64_true'
}

function Get-GateBuildGameName {
    # The game a `zenith.ps1 build <X> ...` line targets, or $null for
    # `build engine` (which builds the shared engine libs under Build/, not
    # any game's own project).
    param([string]$Line)
    if ($Line -match '\bbuild\s+([A-Za-z0-9_]+)') {
        $name = $Matches[1]
        if ($name -ieq 'engine') { return $null }
        return $name
    }
    return $null
}

function Find-CompiledObjectPath {
    # Where a created source file's compiled .obj should be, given the configs
    # THIS phase actually built (one {Game; Leaf} record per $buildLines entry
    # that ran -- every one of them exited 0 by the time step 3 calls this, a
    # failing build line exits the phase first). Returns the .obj path found,
    # or $null.
    #
    # A file under Games/<Name>/... compiles into that game's OWN project, so
    # the location is exact and verified against a real build tree (flat, one
    # .obj per TU, no further nesting):
    #   Games/<Name>/Build/obj/win64/<config>/<leaf>.obj
    #
    # A file under the shared engine tree (Zenith/**, or anything else this
    # repo's Sharpmake split routes into one of the ~32 engine-lib static
    # libraries living under Build/) does NOT get the same precision:
    # answering "which lib owns this source path" exactly means replicating
    # Build/Sharpmake_EngineLibs.cs's whole membership table here -- a second
    # copy of a mapping Docs/BuildSystem.md Sec.4 and
    # Tools/check_project_membership.ps1 already go to some trouble to keep as
    # ONE owner. Every engine lib's IntermediatePath shares the same shape
    # (Build/obj/<lowercase-lib-name>/win64/<config>/), so instead this
    # searches every lib's obj tree for the leaf at a config this phase built.
    # That is still strictly stronger than the build-log grep it replaces
    # (idempotent, and it inspects the actual link input rather than transient
    # stdout) but is not pinpoint the way the game-project case is. If this
    # ever proves too broad -- a stale .obj under an unrelated lib with a
    # colliding file name -- narrow it with the real membership table rather
    # than loosening the check.
    #
    # fixB/ZEN-111: the Engine gate list now also builds standalone repo-root
    # tool exe projects whole (FluxCompiler, ZenithHub, and TilePuzzle's
    # extraSharpmakeProjects TilePuzzleLevelGen/TilePuzzleRegistryViewer via
    # `build TilePuzzle --whole`). Each of those has its own FLAT obj tree at
    # <ProjectDir>/obj/win64/<config>/<leaf>.obj -- the same shape as a game's
    # own Build/obj, just rooted one level up -- so a source under one of
    # them gets the same exact lookup as a Games/<Name> file, DERIVED from
    # whether its own top-level directory owns an obj/win64/ subtree rather
    # than a hard-coded project list (verified on disk against the real
    # checkout's FluxCompiler/, ZenithHub/, TilePuzzleLevelGen/,
    # TilePuzzleRegistryViewer/ output). ZenithTools is deliberately NOT one
    # of these: Sharpmake_ZenithTools.cs gives it its OWN IntermediatePath
    # under Build/obj/ZenithTools/win64/<config>/, so its sources (under
    # Tools/) already resolve through the broad Build/obj search below, same
    # as every other engine lib -- and Zenith/** itself has no Zenith/obj
    # tree of its own for the same reason (its objects land directly under
    # Build/obj/win64/<config>/, part of that same broad search).
    param(
        [Parameter(Mandatory)][string]$RepoRoot,
        [Parameter(Mandatory)][string]$SourcePath,
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]]$BuiltConfigs
    )
    $leaf = [System.IO.Path]::GetFileNameWithoutExtension((Split-Path -Leaf $SourcePath))
    $normalizedSource = ($SourcePath -replace '\\', '/')
    $gameMatch = [regex]::Match($normalizedSource, '^Games/([^/]+)/')

    if ($gameMatch.Success) {
        $game = $gameMatch.Groups[1].Value
        # Prefer the leaves THIS game's own build line(s) used. A ticket that
        # also unions in the Engine category's `build engine` line (Game =
        # $null) contributes nothing here -- that line never touches a game's
        # obj tree.
        $leaves = @($BuiltConfigs | Where-Object { $_.Game -and $_.Game -ieq $game } | ForEach-Object { $_.Leaf } | Select-Object -Unique)
        if ($leaves.Count -eq 0) {
            # No build line named this game -- should not happen for a
            # coherent gate list, but fall back to every leaf that WAS built
            # rather than reporting unconditionally uncompiled.
            $leaves = @($BuiltConfigs | ForEach-Object { $_.Leaf } | Select-Object -Unique)
        }
        foreach ($cfgLeaf in $leaves) {
            $candidate = Join-Path $RepoRoot "Games/$game/Build/obj/win64/$cfgLeaf/$leaf.obj"
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
        return $null
    }

    $leaves = @($BuiltConfigs | ForEach-Object { $_.Leaf } | Select-Object -Unique)

    $topMatch = [regex]::Match($normalizedSource, '^([^/]+)/')
    if ($topMatch.Success) {
        $topDir = $topMatch.Groups[1].Value
        $projObjRoot = Join-Path $RepoRoot "$topDir/obj/win64"
        if (Test-Path -LiteralPath $projObjRoot) {
            foreach ($cfgLeaf in $leaves) {
                $candidate = Join-Path $RepoRoot "$topDir/obj/win64/$cfgLeaf/$leaf.obj"
                if (Test-Path -LiteralPath $candidate) { return $candidate }
            }
            # This top-level dir owns a flat obj tree of its own (a tool exe
            # project), so -- exactly like the Games/<Name> case above -- a
            # miss here is a real miss, not a cue to fall through to the
            # broad engine-wide search below.
            return $null
        }
    }

    $engineObjRoot = Join-Path $RepoRoot 'Build/obj'
    if (-not (Test-Path -LiteralPath $engineObjRoot)) { return $null }
    foreach ($cfgLeaf in $leaves) {
        $needle = "\win64\$cfgLeaf\"
        $hit = Get-ChildItem -LiteralPath $engineObjRoot -Recurse -Filter "$leaf.obj" -File -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -like "*$needle*" } |
            Select-Object -First 1
        if ($hit) { return $hit.FullName }
    }
    return $null
}

# =============================================================================
if ($NoRun) { return }

# ---- Start-of-phase housekeeping -------------------------------------------
#
# 1. ROTATE the transcript. `gates.log` was Add-Content-only and reached 112 MB
#    on one ticket; the whole `.zagent/` scratch reached 608 MB, gitignored, so
#    it grew invisibly forever. One phase, one transcript, one archived
#    predecessor -- bounded at two.
# 2. PRUNE the per-gate wrappers and their output. Individual `gate_*.out` files
#    run to 50 MB each and were never removed, so a re-attempted ticket
#    accumulated every previous run's. The verdict lives in tick_gates.json and
#    the transcript in gates.log; these are raw child stdout and are superseded
#    the moment a new phase starts.
if (Test-Path -LiteralPath $logPath) {
    Move-Item -LiteralPath $logPath -Destination (Join-Path $runDir 'gates.prev.log') -Force
}
Get-ChildItem -LiteralPath $runDir -Filter 'gate_*' -File -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue

# 3. The `running` record, written BEFORE anything can fail. A killed run now
#    leaves `state: running` rather than the previous run's verdict.
$startPayload = [ordered]@{
    key = $Key; phase = $Phase; state = 'running'; exitCode = $null
    verdict = 'in progress -- this run has not produced a verdict yet'
    headSha = $script:HeadSha; startedUtc = $script:StartedUtc
    steps = @(); skipped = @(); flakes = @()
}
Set-Content -LiteralPath $resultPath -Value ($startPayload | ConvertTo-Json -Depth 6) -Encoding utf8

try {
    Write-Line ''
    Write-Line "[tick_gates] $Key - phase '$Phase' - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    Write-Line "[tick_gates] HEAD $(if ($script:HeadSha) { $script:HeadSha.Substring(0, [Math]::Min(12, $script:HeadSha.Length)) } else { '(unknown)' })"

    # --- The build slot: held for the WHOLE invocation that runs gate lines,
    # every phase (measure/verify/all) -- released only in the outermost
    # `finally` below, AFTER Invoke-ExeSweep. Acquired here, first thing
    # inside the try, so a long wait leaves tick_gates.json saying `running`
    # (the record written just above, before this try) rather than a stale
    # verdict from a previous invocation -- and so a timeout can still write
    # an honest exit-11 verdict through the SAME Write-Result/finally paths
    # every other exit uses.
    $script:BuildSlotHandle = Enter-ZenithBuildSlot -Reason "tick_gates $Key phase=$Phase"
    if ($null -eq $script:BuildSlotHandle) {
        Write-Result 11 'slot busy -- the machine-wide build slot is held by another checkout; not a gate failure' $null
        exit 11
    }

    $gateLines = Get-GateList -ReuseGateList:$ReuseGateList
    if ($null -eq $gateLines) {
        Write-Result 5 'could not read the gate list; nothing ran' $null
        exit 5
    }

    # Classified rather than positional. A category may order its gates however
    # it likes, and `measure` needs the build and unit lines specifically.
    $buildLines = @($gateLines | Where-Object { Test-IsZenithBuildLine $_ })
    $unitLines = @($gateLines | Where-Object { $_ -match 'run_unit_gate\.ps1' })

    # ---- MEASURE -------------------------------------------------------------
    if ($Phase -eq 'measure' -or $Phase -eq 'all') {

        # `PlannedLines` (and its complement, `DeferredLines`) are populated
        # HERE -- as soon as the gate list is classified and BEFORE regen, the
        # build, or a single gate line has run -- so that `skipped` is correct
        # from ANY exit path below, including a thrown exception. It used to
        # be set at the bottom of this phase, after the build loop and both
        # file-provenance checks: an internal error anywhere in between left
        # `$script:PlannedLines` empty, so `Write-Result`'s `skipped` (derived
        # as PlannedLines minus ran) reported `[]` while the unit gate and
        # every verify line genuinely never ran -- a crash that looked, on the
        # one field a reader is told to check, like nothing had been asked of
        # this run at all.
        $script:PlannedLines.Clear()
        foreach ($l in $buildLines) { $script:PlannedLines.Add($l) }
        foreach ($l in $unitLines) { $script:PlannedLines.Add($l) }
        # Everything else is verify's. Recorded as DEFERRED rather than left to
        # inference: `skipped` will be empty on a clean measure, and a reader told
        # to "read skipped before concluding anything about what was verified"
        # would otherwise read a green subset as a green whole.
        $script:DeferredLines.Clear()
        foreach ($l in $gateLines) {
            if ($script:PlannedLines -notcontains $l) { $script:DeferredLines.Add($l) }
        }
        if ($script:DeferredLines.Count -gt 0) {
            Write-Line "[tick_gates] measure runs $($script:PlannedLines.Count) of $($gateLines.Count) gate line(s); $($script:DeferredLines.Count) DEFERRED to -Phase verify:" 'DarkGray'
            foreach ($l in $script:DeferredLines) { Write-Line "[tick_gates]   deferred: $l" 'DarkGray' }
        }

        # Working-tree SHA, informational for measure (see $script:TreeShaStart's
        # own header comment) -- taken before the first gate line runs.
        $script:TreeShaStart = Get-WorkingTreeSha -RepoRoot $RepoRoot

        # 1. Regen, when and only when a source file was created.
        $created = Get-CreatedSourceFiles
        # `@(...)` over an ARRAY is safe (it is the List[object] overload that
        # throws), and it is load-bearing: a filter matching exactly one file
        # would otherwise unroll to a bare object and `.Count` would be $null.
        $newFiles = @($created | Where-Object { $_.IsNew })
        if ($created.Count -gt 0) {
            Write-Banner "$($created.Count) added/removed source file(s) -- regenerating"
            foreach ($c in $created) { Write-Line "  $(if ($c.IsNew) { 'created' } else { 'removed/renamed' }): $($c.Path)" }
            $regen = Invoke-GateLine -Line "pwsh -NoProfile -File `"$RepoRoot\Build\regen.ps1`"" -Label 'regen'
            if ($regen.ExitCode -ne 0) {
                # FATAL, not noise. regen exits 3 and regenerates NOTHING while
                # the stale projects stay on disk, so continuing produces a
                # green build of the OLD file list.
                Write-Result 2 "regen failed (exit $($regen.ExitCode)) -- stale projects are still on disk and a build now would be green and wrong" $null
                exit 2
            }
        }
        else {
            Write-Line '[tick_gates] no created source files; skipping regen.'
        }

        # 2. Build.
        Write-Banner "build ($($buildLines.Count) line(s))"
        $buildLog = ''
        foreach ($line in $buildLines) {
            $step = Invoke-GateLine -Line $line -Label 'build'
            $buildLog += $step.Output
            if ($step.ExitCode -ne 0) {
                Write-Result 4 "build failed: $line" $null
                exit 4
            }
        }
        # Every build line that reaches here exited 0. Record which config
        # each one built -- and which game, when it names one -- so step 3 can
        # ask "does an .obj for this file exist under a config THIS phase
        # actually built" instead of grepping transient stdout.
        $builtConfigs = New-Object System.Collections.Generic.List[object]
        foreach ($line in $buildLines) {
            $builtConfigs.Add([PSCustomObject]@{ Game = (Get-GateBuildGameName $line); Leaf = (Resolve-GateConfigLeaf $line) })
        }

        # 3. Every created .cpp must have a compiled .obj under its project's
        # IntermediatePath, at a config this phase actually built.
        # ** THE BUILD-LOG GREP THIS AUTHORITY REPLACED IS NOT IDEMPOTENT.
        # MSBuild only echoes a TU on the build that actually RECOMPILES it --
        # on an INCREMENTAL build where a created file's .obj is already up to
        # date (routine: the protocol mandates re-running a phase after any
        # post-measure edit), nothing names it in stdout, and the old check
        # reported NOT IN BUILD LOG / exit 3 for a file whose .obj is on disk
        # and whose .lib already contains it -- a false failure. The .obj's
        # PRESENCE is what a linked build actually depends on, and checking
        # for it is idempotent by construction: it is there whether this
        # build compiled it moments ago or three builds back. The build-log
        # grep is kept below as a SECONDARY, informational signal only; see
        # Find-CompiledObjectPath's own comment for what "a config this phase
        # built" means for a game-project file versus a shared-engine-tree one.
        if ($newFiles.Count -gt 0 -and $buildLines.Count -eq 0) {
            Write-Banner 'created files, and NO build gate to prove them with'
            foreach ($c in $newFiles) { Write-Line "  unverifiable: $($c.Path)" 'Red' }
            Write-Result 3 "$($newFiles.Count) file(s) were created and this ticket's gate list contains no build line, so nothing can show they compile" @{ uncompiled = @($newFiles | ForEach-Object { $_.Path }) }
            exit 3
        }
        if ($newFiles.Count -gt 0) {
            Write-Banner 'proving the created TUs were compiled'
            $uncompiled = New-Object System.Collections.Generic.List[string]
            foreach ($c in $newFiles) {
                $leaf = Split-Path -Leaf $c.Path
                $objHit = Find-CompiledObjectPath -RepoRoot $RepoRoot -SourcePath $c.Path -BuiltConfigs $builtConfigs.ToArray()
                $inLog = $buildLog -like "*$leaf*"
                if ($null -ne $objHit) {
                    $note = if (-not $inLog) { ' (build log did not name it -- an incremental build names only what it recompiled; the .obj is the authority)' } else { '' }
                    Write-Line "  compiled: $leaf -> $objHit$note" 'Green'
                }
                else {
                    $note = if ($inLog) { ' (build log DID name it, but no matching .obj was found -- treating as uncompiled; the .obj is the authority)' } else { '' }
                    Write-Line "  NOT COMPILED: $leaf -- no .obj under any built config's IntermediatePath$note" 'Red'
                    $uncompiled.Add($c.Path)
                }
            }
            if ($uncompiled.Count -gt 0) {
                Write-Result 3 "$($uncompiled.Count) created file(s) have no compiled .obj under any built config's IntermediatePath -- the build is green and the deliverable is not proven in it" @{ uncompiled = $uncompiled.ToArray() }
                exit 3
            }
        }

        # 3b. Every created .inl must be #included by SOMETHING tracked.
        # See Get-CreatedIncludeFiles: the build log names TUs, so a .Tests.inl
        # that nothing includes is invisible to check 3 and to the compiler.
        # PLAIN ASSIGNMENT, NEVER @(...). Get-CreatedIncludeFiles ends
        # `return , $created.ToArray()` -- the leading comma is what carries an
        # EMPTY array through the pipeline intact. Wrapping that call in @(...)
        # RE-WRAPS it: the function emits one object (the array), @() collects
        # the pipeline into an array of ONE element whose [0] is that array. So
        # .Count was always 1, the loop always ran, and with no created .inl the
        # single element was an empty array -- `Split-Path -Leaf @()` throws
        # "Cannot bind argument to parameter 'Path' because it is an empty
        # array" and the whole phase exits 1 AFTER every build has passed.
        # That crashed `-Phase measure` on EVERY run that created no .inl --
        # this check sits OUTSIDE the `if ($newFiles.Count -gt 0)` block that
        # closes above, so whether any source file was created is irrelevant
        # (measured on ZEN-101). Get-CreatedSourceFiles is consumed correctly
        # at line ~436; match it.
        $newIncludes = Get-CreatedIncludeFiles
        if ($newIncludes.Count -gt 0) {
            Write-Banner 'proving the created .inl files are included somewhere'
            $orphans = New-Object System.Collections.Generic.List[string]
            foreach ($inc in $newIncludes) {
                $leaf = Split-Path -Leaf $inc
                # `git grep -l` searches TRACKED files only, which is what we
                # want: an include added in an untracked scratch file proves
                # nothing. The created .inl itself is excluded so a file that
                # merely names itself in a comment does not pass.
                $hits = @(& git -C $RepoRoot grep -l --fixed-strings -- $leaf 2>$null) |
                    Where-Object { $_ -and ($_ -replace '\\', '/') -ne ($inc -replace '\\', '/') }
                if ($hits.Count -gt 0) { Write-Line "  included: $leaf" 'Green' }
                else { Write-Line "  INCLUDED BY NOTHING: $leaf" 'Red'; $orphans.Add($inc) }
            }
            if ($orphans.Count -gt 0) {
                Write-Result 3 "$($orphans.Count) created .inl file(s) are #included by nothing -- the build is green, the unit count has not moved, and the deliverable is not in the binary" @{ uncompiled = $orphans.ToArray() }
                exit 3
            }
        }

        # 4. The unit gate, ALONE, to observe the count.
        # measure deliberately runs only the build and the unit gate, so those
        # are what it PLANNED; the rest are verify's and are not "skipped" here.
        # (`PlannedLines`/`DeferredLines` were populated at the TOP of this
        # phase, before regen -- see the comment there for why.)
        Write-Banner "unit gate alone ($($unitLines.Count) line(s)) -- measuring"
        $measurements = New-Object System.Collections.Generic.List[object]
        $pinNeedsBump = $false
        $measuredGames = New-Object System.Collections.Generic.List[string]
        foreach ($line in $unitLines) {
            $step = Invoke-GateLine -Line $line -Label 'unit'
            $game = if ($line -match '-Game\s+([A-Za-z0-9_]+)') { $Matches[1] } else { '(derived from -Exe)' }
            if (-not $measuredGames.Contains($game)) { $measuredGames.Add($game) }
            $tally = $null
            if ($step.Output -match '(\d+)\s+ran,\s+(\d+)\s+passed,\s+(\d+)\s+failed') {
                $tally = [PSCustomObject]@{ ran = [int]$Matches[1]; passed = [int]$Matches[2]; failed = [int]$Matches[3] }
            }
            $measurements.Add([PSCustomObject]@{ game = $game; exitCode = $step.ExitCode; tally = $tally })

            if ($step.ExitCode -eq 0) { continue }
            # ** A RED GATE WITH ZERO FAILURES IS THE MEASUREMENT, NOT A FAILURE.
            # The gate asserts `ran == Baseline` EXACTLY, so a ticket that adds
            # or removes tests reds it with nothing broken. That is the gate
            # working, and the number it prints is the deliverable.
            if ($null -ne $tally -and $tally.failed -eq 0) {
                $pinNeedsBump = $true
                Write-Line ''
                Write-Line "[tick_gates] MEASURED: $game ran $($tally.ran) with 0 failures." 'Yellow'
                Write-Line "[tick_gates] Bump '$game' to $($tally.ran) in Tools/unit_baselines.json -- the ONE home for a pin." 'Yellow'
                Write-Line '[tick_gates] A backend-neutral ENGINE unit moves EVERY game row; say in the work log which you measured and which you inferred.' 'Yellow'
                continue
            }
            Write-Result 4 "unit gate failed with real failures: $line" @{ measurements = $measurements.ToArray() }
            exit 4
        }

        # The registry count, for EVERY measured game and whether or not the pin
        # moved. It was printed only on a bump, which is exactly backwards: the
        # case that needs it most is a ticket that adds an AUTOMATED test and no
        # boot units -- the registry moves, the pin does not, and nothing else in
        # the run would have said so. (ZM-67 stage 1 was 69 -> 70 with the pin
        # unchanged.) Status.md's LIVE PIN block carries both numbers and
        # doc_lint C7 asserts both, so a phase whose job is "observe before you
        # narrate" has to hand over both.
        foreach ($g in $measuredGames) {
            $reg = Get-AutomatedRegistryCount $g
            $script:RegistryCounts[$g] = $reg
            if ($null -ne $reg) {
                Write-Line "[tick_gates] REGISTRY: $g has $reg ZENITH_AUTOMATED_TEST_REGISTER call site(s). Derived with doc_lint C7's own paren-anchored regex -- a bare grep also counts paren-less mentions in comments and over-reports." 'Yellow'
            }
        }

        # Working-tree SHA, informational -- after the last gate line measure
        # actually ran. (An early exit above -- regen/build/uncompiled/unit
        # failure -- leaves this at its $null initial value; that is fine,
        # this pair is not authoritative for measure the way it is for
        # verify.)
        $script:TreeShaEnd = Get-WorkingTreeSha -RepoRoot $RepoRoot

        if ($pinNeedsBump) {
            # Exit 10, not 0 and not 1. The caller must stop here and write the
            # pin, the Status.md narration and any doc edits BEFORE the verify
            # phase -- which is the whole reason the phases are split.
            Write-Result 10 'MEASURED: a pin needs bumping. Edit Tools/unit_baselines.json (and any Status.md narration) NOW, then run -Phase verify.' @{ measurements = $measurements.ToArray() }
            exit 10
        }
        Write-Line ''
        if ($unitLines.Count -eq 0) {
            # Not the same sentence as "the pins are fine". A category whose
            # gate list has no unit line measured NOTHING, and reporting that
            # as a clean measurement is how a reader concludes a pin was
            # checked when no pin was read.
            Write-Line '[tick_gates] NO unit-gate line in this gate list -- no pin was measured.' 'Yellow'
        }
        else {
            Write-Line '[tick_gates] pins already correct; nothing to write before verify.' 'Green'
        }
    }

    # ---- VERIFY --------------------------------------------------------------
    if ($Phase -eq 'verify' -or $Phase -eq 'all') {

        # Re-derived, not reused (unless -ReuseGateList). Between measure and
        # verify the caller edits the pin and the docs, and those edits can
        # pull in a category whose gates were never selected -- which is
        # exactly what `zagent guard` fails on. Asking again here means the
        # recording matches the diff that is about to be committed.
        # -ReuseGateList (lane REGATE) is the deliberate exception: the point
        # there is running the SAME selection again on a merged tip, not
        # re-deriving from whatever the merge happened to touch.
        if ($Phase -eq 'verify') {
            $gateLines = Get-GateList -ReuseGateList:$ReuseGateList
            if ($null -eq $gateLines) {
                Write-Result 5 'could not read the gate list; nothing ran' $null
                exit 5
            }
        }

        # verify intends to run EVERY line, so anything missing from the steps
        # array when this returns is genuinely un-run -- which is exactly the
        # case worth naming, since the list stops at the first failure.
        $script:PlannedLines.Clear()
        foreach ($l in $gateLines) { $script:PlannedLines.Add($l) }

        # ★ FRESHNESS = VERIFIED TREE (plan "Integration mechanics" 1). Every
        # gate line legitimately rewrites tracked output sometimes (a shader
        # header regenerated by the build gate, a .zscen re-authored by a
        # boot), so a single treeShaStart != treeShaEnd is NOT itself a
        # failure -- it is answered by re-running verify ONCE more, in this
        # same invocation, on the now-current tree. Only a SECOND mismatch
        # (pass 2's own start != its own end) is non-deterministic tracked
        # output, which is a real red: exit 12.
        $maxVerifyPasses = 2
        for ($verifyPass = 1; $verifyPass -le $maxVerifyPasses; $verifyPass++) {
            $passLabel = if ($verifyPass -eq 1) { '' } else { " (pass $verifyPass -- retry after a tree change)" }
            Write-Banner "verify -- every gate, in order ($($gateLines.Count) line(s))$passLabel"

            # Pass 2 recomputes its own start FRESH rather than reusing pass
            # 1's end -- they are expected to be equal, but computing it
            # again is what makes this correct even if they are not.
            $passTreeShaStart = Get-WorkingTreeSha -RepoRoot $RepoRoot
            if ($verifyPass -eq 1) { $script:TreeShaStart = $passTreeShaStart }

            $failedLine = $null
            foreach ($line in $gateLines) {
                # Flake-aware: a RED non-build line is re-run once and the
                # outcome recorded in $script:Flakes, but $step is still the
                # FIRST run's result and the branch below is unchanged by a
                # rerun -- see Invoke-GateLineFlakeAware's own comment.
                $step = Invoke-GateLineFlakeAware -Line $line -Label 'verify'
                if ($step.ExitCode -ne 0) {
                    $failedLine = $line
                    break
                }
            }

            $passTreeShaEnd = Get-WorkingTreeSha -RepoRoot $RepoRoot
            $script:TreeShaEnd = $passTreeShaEnd

            if ($null -ne $failedLine) {
                # ★ A FAILED GATE VOIDS EVERY GATE BEHIND IT. Stopping here is
                # correct and cheap, but the lines that never ran are the ones a
                # reader most needs named: on one ticket the unit gate and
                # doc_lint were both behind a failing suite, so a freshly-bumped
                # pin was unconfirmed and the linter had never seen the edited
                # docs -- and nothing in the result said so.
                $notRun = @($gateLines | Where-Object { $_ -ne $failedLine -and @($script:Steps.ToArray() | ForEach-Object { $_.Line }) -notcontains $_ })
                if ($notRun.Count -gt 0) {
                    Write-Line ''
                    Write-Line "[tick_gates] $($notRun.Count) gate line(s) NEVER RAN because this one failed:" 'Yellow'
                    foreach ($n in $notRun) { Write-Line "  not run: $n" 'Yellow' }
                }
                Write-Result 4 "gate failed: $failedLine" $null
                exit 4
            }

            # A $null on either side means Get-WorkingTreeSha itself could not
            # produce an answer (e.g. `git write-tree` failed for a reason
            # unrelated to any gate) -- treated as stable rather than as a
            # mismatch, so a diagnostic helper's own failure can never block
            # an otherwise-green verify or force a pointless retry.
            $stable = ($null -eq $passTreeShaStart) -or ($null -eq $passTreeShaEnd) -or ($passTreeShaStart -eq $passTreeShaEnd)
            if ($stable) { break }

            if ($verifyPass -ge $maxVerifyPasses) {
                Write-Result 12 "tree unstable: non-deterministic tracked output across two verify passes (pass $verifyPass start=$passTreeShaStart end=$passTreeShaEnd)" $null
                exit 12
            }

            Write-Line "[tick_gates] working tree changed during verify (start $passTreeShaStart != end $passTreeShaEnd) -- a gate line rewrote tracked output; re-running verify once more" 'Yellow'
            $script:TreeRetried = $true
        }
    }

    Write-Result 0 'all gates green' $null
    exit 0
}
catch {
    # A bare PowerShell error reaches the caller as one line with no location,
    # which on an unattended tick is indistinguishable from a gate failing. The
    # script's OWN faults must name themselves.
    $err = $_
    Write-Line ''
    Write-Line "[tick_gates] INTERNAL ERROR: $($err.Exception.Message)" 'Red'
    Write-Line "  at $($err.InvocationInfo.ScriptName):$($err.InvocationInfo.ScriptLineNumber)" 'Red'
    Write-Line "  $($err.InvocationInfo.Line.Trim())" 'Red'
    Write-Line ($err.ScriptStackTrace) 'Red'
    # state = 'error', not the default 'complete' -- a thrown exception used to
    # report state: complete, green-looking steps, and skipped: [] all at
    # once, which is the exact lie Write-Result's own guard (see its header
    # comment) exists to stop telling from the other end. Checked against the
    # one real consumer of this field, Tools/Vault/cli/tick-checks.mjs's
    # gateVerdictFresh: it already treats any state other than 'complete' as
    # not fresh (the same bucket 'running' falls into), so this does not need
    # a new branch there -- it correctly trips the EXISTING one for the first
    # time instead of sliding past it.
    Write-Result 1 "internal error: $($err.Exception.Message)" @{ state = 'error' }
    exit 1
}
finally {
    # Every exit path, including a throw and a Ctrl+C. A gate that died holding
    # a game exe open fails the NEXT build on a locked output file, and that
    # failure names a file nobody touched.
    Invoke-ExeSweep
    # The build slot is released LAST, after the sweep -- a stray game exe
    # this run's own gates left open should be cleared while THIS run still
    # holds the slot, not handed to whichever checkout acquires it next.
    # Exit-ZenithBuildSlot is a safe no-op for $null (never acquired, e.g. an
    # exit-11 timeout) and for an inherited handle (a nested tick_gates or
    # `zenith build` inside an already-held slot never touches the mutex).
    Exit-ZenithBuildSlot $script:BuildSlotHandle
}
