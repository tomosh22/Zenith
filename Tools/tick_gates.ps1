# tick_gates.ps1 -- run one ticket's gates, in the order that only costs one cycle.
# =============================================================================
# This is `/tick` step 6.2 as a MECHANISM. It used to be four paragraphs of
# prose in `.claude/commands/tick.md`, each added after the rule it describes
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
#       10 MEASURED: the unit pin needs bumping (not a failure; see below)
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

$runDir = Join-Path $RepoRoot ".zagent/run/$Key"
$logPath = Join-Path $runDir 'gates.log'
$resultPath = Join-Path $runDir 'tick_gates.json'
if (-not $NoRun -and -not (Test-Path -LiteralPath $runDir)) {
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
}

$script:Steps = New-Object System.Collections.Generic.List[object]

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

# The automated-test registry count, derived EXACTLY as doc_lint.ps1 check C7
# derives it -- the paren is load-bearing. A bare `grep -c
# ZENITH_AUTOMATED_TEST_REGISTER` also counts the paren-less mentions in comments
# (three of them live in ZM_AutoTests_TrainerSight.cpp) and over-reports by that
# many. Status.md's LIVE PIN block carries this number beside the boot pin and C7
# asserts BOTH, but only the boot pin comes out of a gate run -- the registry
# count comes from `zenith test`, which `measure` does not run. So a phase whose
# whole job is "observe the numbers before you narrate them" was handing over one
# of the two. This closes that.
function Get-AutomatedRegistryCount([string]$Game) {
    $testsDir = Join-Path $repoRoot "Games/$Game/Tests"
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
function Invoke-ExeSweep {
    param([string]$RepoRoot = $script:RepoRoot)
    $names = @(Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'Games') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Name.ToLowerInvariant() })
    if ($names.Count -eq 0) { return }
    $killed = @()
    foreach ($p in @(Get-Process -ErrorAction SilentlyContinue)) {
        if ($names -notcontains $p.ProcessName.ToLowerInvariant()) { continue }
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
    }
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
function Get-GateList {
    Write-Banner "zagent gates $Key"
    $gatesJson = Join-Path $runDir 'gates.json'
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
    steps = @(); skipped = @()
}
Set-Content -LiteralPath $resultPath -Value ($startPayload | ConvertTo-Json -Depth 6) -Encoding utf8

try {
    Write-Line ''
    Write-Line "[tick_gates] $Key - phase '$Phase' - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    Write-Line "[tick_gates] HEAD $(if ($script:HeadSha) { $script:HeadSha.Substring(0, [Math]::Min(12, $script:HeadSha.Length)) } else { '(unknown)' })"

    $gateLines = Get-GateList
    if ($null -eq $gateLines) {
        Write-Result 5 'could not read the gate list; nothing ran' $null
        exit 5
    }

    # Classified rather than positional. A category may order its gates however
    # it likes, and `measure` needs the build and unit lines specifically.
    $buildLines = @($gateLines | Where-Object { $_ -match 'zenith\.ps1["'']?\s+build\b' })
    $unitLines = @($gateLines | Where-Object { $_ -match 'run_unit_gate\.ps1' })

    # ---- MEASURE -------------------------------------------------------------
    if ($Phase -eq 'measure' -or $Phase -eq 'all') {

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

        # 3. Every created .cpp must appear BY NAME in the build log.
        # ** THE SAME QUESTION, ASKED OF THIS SCRIPT: what does the check do
        # when its INPUT is absent? A created file with no build line in the
        # gate list cannot be proven compiled by anything, and skipping in
        # silence would be the exact failure the check exists to catch --
        # dressed as a pass. Refuse instead.
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
                if ($buildLog -like "*$leaf*") { Write-Line "  compiled: $leaf" 'Green' }
                else { Write-Line "  NOT IN BUILD LOG: $leaf" 'Red'; $uncompiled.Add($c.Path) }
            }
            if ($uncompiled.Count -gt 0) {
                Write-Result 3 "$($uncompiled.Count) created file(s) never reached the compiler -- the build is green and the deliverable is not in it" @{ uncompiled = $uncompiled.ToArray() }
                exit 3
            }
        }

        # 4. The unit gate, ALONE, to observe the count.
        # measure deliberately runs only the build and the unit gate, so those
        # are what it PLANNED; the rest are verify's and are not "skipped" here.
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
            if ($null -ne $reg) {
                Write-Line "[tick_gates] REGISTRY: $g has $reg ZENITH_AUTOMATED_TEST_REGISTER call site(s). Derived with doc_lint C7's own paren-anchored regex -- a bare grep also counts paren-less mentions in comments and over-reports." 'Yellow'
            }
        }

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

        # Re-derived, not reused. Between measure and verify the caller edits
        # the pin and the docs, and those edits can pull in a category whose
        # gates were never selected -- which is exactly what `zagent guard`
        # fails on. Asking again here means the recording matches the diff that
        # is about to be committed.
        if ($Phase -eq 'verify') {
            $gateLines = Get-GateList
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

        Write-Banner "verify -- every gate, in order ($($gateLines.Count) line(s))"
        foreach ($line in $gateLines) {
            $step = Invoke-GateLine -Line $line -Label 'verify'
            if ($step.ExitCode -ne 0) {
                # ★ A FAILED GATE VOIDS EVERY GATE BEHIND IT. Stopping here is
                # correct and cheap, but the lines that never ran are the ones a
                # reader most needs named: on one ticket the unit gate and
                # doc_lint were both behind a failing suite, so a freshly-bumped
                # pin was unconfirmed and the linter had never seen the edited
                # docs -- and nothing in the result said so.
                $notRun = @($gateLines | Where-Object { $_ -ne $line -and @($script:Steps.ToArray() | ForEach-Object { $_.Line }) -notcontains $_ })
                if ($notRun.Count -gt 0) {
                    Write-Line ''
                    Write-Line "[tick_gates] $($notRun.Count) gate line(s) NEVER RAN because this one failed:" 'Yellow'
                    foreach ($n in $notRun) { Write-Line "  not run: $n" 'Yellow' }
                }
                Write-Result 4 "gate failed: $line" $null
                exit 4
            }
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
    Write-Result 1 "internal error: $($err.Exception.Message)" $null
    exit 1
}
finally {
    # Every exit path, including a throw and a Ctrl+C. A gate that died holding
    # a game exe open fails the NEXT build on a locked output file, and that
    # failure names a file nobody touched.
    Invoke-ExeSweep
}
