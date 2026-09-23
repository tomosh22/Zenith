# build_lock.ps1 -- one machine-wide build slot (capacity 1).
# =============================================================================
# Every harness shares one interactive session, and a build (msbuild, a unit
# gate that boots an exe, `zenith package`) competes for the same locked
# output files, the same shared %TEMP% dirs, and the same compiler processes
# as every other one running at once. This is the mutex that serializes them.
#
# WHY A NAMED MUTEX, NOT A LOCKFILE: a Mutex is reclaimed by the OS the moment
# its owning process dies -- a `WaitOne` on an abandoned mutex still succeeds
# (as `[System.Threading.AbandonedMutexException]`), so a killed holder can
# never wedge every future acquirer the way a stale lockfile would. A
# semaphore would not give this for free; a Mutex does because it is
# thread/process-owned by construction.
#
# ★ .NET MUTEXES ARE THREAD-AFFINE. Acquire and release on the CALLER'S OWN
# pipeline thread only -- never inside `Start-Job`, `ForEach-Object -Parallel`
# or an event handler. A mutex acquired on one thread cannot be released from
# another; PowerShell's parallel constructs run on different runspaces/threads
# than the one that called Enter-ZenithBuildSlot, so doing this from one would
# silently fail to release, or throw on release from the wrong thread.
#
# Dot-sourceable: nothing below runs merely by dot-sourcing this file. The
# only code that executes unconditionally at the bottom checks whether this
# invocation IS a dot-source (`$MyInvocation.InvocationName -eq '.'`) and
# returns immediately if so -- the `-Run` entry point only fires when this
# file is executed directly (`pwsh -File Tools/build_lock.ps1 -Run "..."`).
#
# Usage (dot-sourced, e.g. from Tools/tick_gates.ps1 or ZenithCli.psm1):
#   . Tools/build_lock.ps1
#   $handle = Enter-ZenithBuildSlot -WaitMinutes 5 -Reason 'tick_gates ZEN-6 verify'
#   try { ... } finally { Exit-ZenithBuildSlot $handle }
#
# Usage (as a command, for the landing call and ad-hoc use):
#   pwsh -NoProfile -File Tools/build_lock.ps1 -Run "<command>" [-WaitMinutes n]
#   exit 11 if the slot could not be acquired in time ("slot busy");
#   otherwise exits with the wrapped command's own exit code.
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param(
    # Only meaningful when this file is invoked directly (`-File`), never
    # when dot-sourced. The command to run once the slot is held.
    [string]$Run,

    [Nullable[double]]$WaitMinutes,

    [int]$PollMilliseconds = 5000
)

$ErrorActionPreference = 'Stop'

# The one name every harness/checkout shares. `Local\` (rather than
# `Global\`) needs no `SeCreateGlobalPrivilege` and still serializes every
# session on this machine, which is the scope this repo's builds compete in.
function Get-ZenithBuildSlotName {
    <#
      The effective mutex name for THIS acquire attempt -- resolved at CALL
      TIME, never cached at dot-source time.

      TEST-ISOLATION SEAM (ZEN-113 fix-forward, attempt 2): a $script:
      constant computed ONCE when this file is dot-sourced broke under
      Tools/Test_TickGates.ps1, which dot-sources this file (via
      tick_gates.ps1) near ITS OWN top -- long before its own build-slot
      section ever sets ZENITH_BUILD_SLOT_NAME. A value snapshotted that
      early was still the REAL slot name by the time Enter-ZenithBuildSlot
      actually ran in-process, so the test's private-mutex holders
      (ZenithTestMutexHolder, the abandoned-mutex child) and this
      in-process caller silently contended on two DIFFERENT mutexes
      instead of the same private one -- under tick_gates the in-process
      side waited on the real, parent-held slot forever (until its own
      bounded timeout); standalone it acquired the real slot immediately
      and never touched the test's private one at all. Calling this
      function fresh from inside Enter-ZenithBuildSlot on every acquire
      attempt picks up an override set at ANY point before THAT call, not
      only one set before dot-sourcing -- a child `pwsh -File
      Tools/build_lock.ps1 -Run ...` process already worked correctly
      because it always re-reads the environment at its own startup; this
      makes the in-process, dot-sourced path behave the same way.

      UNSET -- every real invocation, always -- returns the exact literal
      this name held before this seam existed: behaviour is byte-identical.
    #>
    if ($env:ZENITH_BUILD_SLOT_NAME) {
        # SHOULD-FIX (review): a leaked test override in a real build must be
        # visible, not a silent swap of the slot that serializes it.
        Write-Host "[build_lock] TEST slot override active: $($env:ZENITH_BUILD_SLOT_NAME)" -ForegroundColor DarkYellow
        return $env:ZENITH_BUILD_SLOT_NAME
    }
    return 'Local\ZenithBuildSlot'
}

# --- Config -------------------------------------------------------------

function Get-ZenithBuildSlotWaitMinutesDefault {
    <#
      `lanes.buildSlotWaitMinutes` from <repoRoot>/zagent.project.json, or the
      hard default of 90 when the file, the `lanes` block or the key itself
      is absent -- the key does not exist yet (WP7 adds it), so this must
      degrade quietly rather than throw. PSObject.Properties[...] throughout:
      Set-StrictMode turns a missing property into a throw for a caller (such
      as Tools/Test_TickGates.ps1) that runs under -Version Latest, and a
      config file legally omitting a not-yet-added key must not crash the
      slot wait.
    #>
    param([Parameter(Mandatory)][string]$RepoRoot)
    $hardDefault = 90.0
    $path = Join-Path $RepoRoot 'zagent.project.json'
    if (-not (Test-Path -LiteralPath $path)) { return $hardDefault }
    try {
        $json = Get-Content -LiteralPath $path -Raw -Encoding utf8 | ConvertFrom-Json
        $lanesProp = $json.PSObject.Properties['lanes']
        if (-not $lanesProp -or $null -eq $lanesProp.Value) { return $hardDefault }
        $waitProp = $lanesProp.Value.PSObject.Properties['buildSlotWaitMinutes']
        if (-not $waitProp -or $null -eq $waitProp.Value) { return $hardDefault }
        return [double]$waitProp.Value
    }
    catch {
        return $hardDefault
    }
}

# --- Diagnostics ----------------------------------------------------------
#
# Best-effort only: every read and write below swallows its own I/O errors.
# This file exists purely so a caller stuck in the wait loop can print WHO
# holds the slot; it is never authoritative and nothing downstream depends
# on it existing.

function Get-ZenithGitCommonDir {
    <#
      `git rev-parse --git-common-dir`, resolved to an absolute path. The
      output is RELATIVE (`.git`) from a main checkout and ABSOLUTE
      (`C:/.../.git/worktrees/<name>`) from a linked worktree -- brief A/§4
      says so explicitly, and IsPathRooted is the one correct way to tell
      the two apart rather than guessing from the string's shape.
    #>
    param([Parameter(Mandatory)][string]$RepoRoot)
    try {
        $out = & git -C $RepoRoot rev-parse --git-common-dir 2>$null
        if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($out)) { return $null }
        $out = ([string]$out).Trim()
        if ([System.IO.Path]::IsPathRooted($out)) { return $out }
        return Join-Path $RepoRoot $out
    }
    catch {
        return $null
    }
}

function Write-ZenithBuildSlotDiagnostics {
    param(
        [Parameter(Mandatory)][string]$DiagPath,
        [Parameter(Mandatory)][string]$RepoRoot,
        [string]$Reason
    )
    try {
        $sessionId = $null
        try { $sessionId = (Get-Process -Id $PID).SessionId } catch { }
        $payload = [ordered]@{
            pid       = $PID
            sessionId = $sessionId
            repoRoot  = $RepoRoot
            since     = (Get-Date).ToUniversalTime().ToString('o')
            reason    = $Reason
        }
        $dir = Split-Path -Parent $DiagPath
        if ($dir -and -not (Test-Path -LiteralPath $dir)) {
            New-Item -ItemType Directory -Path $dir -Force -ErrorAction SilentlyContinue | Out-Null
        }
        Set-Content -LiteralPath $DiagPath -Value ($payload | ConvertTo-Json -Depth 4) -Encoding utf8 -ErrorAction Stop
    }
    catch {
        # Best-effort. A diagnostics write must never be why the slot itself
        # fails to acquire.
    }
}

function Get-ZenithBuildSlotHolderInfo {
    <#
      A short human-readable line describing who holds the slot, read from
      the diagnostics file if one is present. Never throws; an unreadable or
      absent file just means "no extra info", not a wait-loop failure.
    #>
    param([string]$DiagPath)
    if ([string]::IsNullOrEmpty($DiagPath) -or -not (Test-Path -LiteralPath $DiagPath)) { return '' }
    try {
        $json = Get-Content -LiteralPath $DiagPath -Raw -Encoding utf8 | ConvertFrom-Json
        $bits = New-Object System.Collections.Generic.List[string]
        if ($json.PSObject.Properties['pid']) { $bits.Add("pid $($json.pid)") }
        if ($json.PSObject.Properties['since']) { $bits.Add("since $($json.since)") }
        if ($json.PSObject.Properties['reason'] -and $json.reason) { $bits.Add("reason: $($json.reason)") }
        if ($bits.Count -eq 0) { return '' }
        return ' (holder: ' + ($bits -join ', ') + ')'
    }
    catch {
        return ''
    }
}

# --- The slot ---------------------------------------------------------------

function Enter-ZenithBuildSlot {
    <#
      Acquires the machine-wide build slot. Returns a handle on success,
      $null on a bounded timeout. ALWAYS pair with Exit-ZenithBuildSlot in a
      `finally`, passing whatever this returned (including $null -- see that
      function's own no-op contract).

      Handle shape:
        [pscustomobject]@{
            Mutex             = <System.Threading.Mutex or $null>
            Inherited         = <bool>
            PriorSlotHeldEnv  = <string or $null>
            PriorNodeReuseEnv = <string or $null>
            DiagPath          = <string or $null>
        }
    #>
    param(
        [Nullable[double]]$WaitMinutes,
        [string]$Reason = '',
        [int]$PollMilliseconds = 5000
    )

    # This function is defined IN this file, so $PSScriptRoot here always
    # resolves to Tools/ (the directory this file lives in) regardless of
    # where it was dot-sourced FROM -- confirmed empirically: a function's
    # $PSScriptRoot binds to its defining file, not the caller's. repoRoot is
    # therefore the parent of Tools/, exactly as the brief specifies.
    $repoRoot = Split-Path -Parent $PSScriptRoot

    # A child `zenith build`/`zenith test` of a slot-holding `tick_gates` (or
    # any nested Enter-ZenithBuildSlot call within the same held slot) must
    # NEVER re-acquire the mutex -- that would deadlock a process against
    # itself. `Start-Process` children inherit the environment block at
    # spawn, so this check alone is enough to propagate the handshake down
    # an arbitrarily deep process tree.
    if ($env:ZENITH_BUILD_SLOT_HELD -eq '1') {
        return [pscustomobject]@{
            Mutex             = $null
            Inherited         = $true
            PriorSlotHeldEnv  = $null
            PriorNodeReuseEnv = $null
            DiagPath          = $null
        }
    }

    $effectiveWaitMinutes = if ($null -ne $WaitMinutes) { $WaitMinutes } else { Get-ZenithBuildSlotWaitMinutesDefault -RepoRoot $repoRoot }
    if ($effectiveWaitMinutes -lt 0) { $effectiveWaitMinutes = 0 }

    $slotName = Get-ZenithBuildSlotName
    $mutex = New-Object System.Threading.Mutex($false, $slotName)
    $diagPath = $null
    $gitCommonDir = Get-ZenithGitCommonDir -RepoRoot $repoRoot
    # Namespaced the same way as the mutex above, from the SAME already-resolved
    # $slotName (not a second raw env read -- one Get-ZenithBuildSlotName call
    # per acquire attempt, so its Write-Host fires at most once here too): an
    # overridden slot name must not read or clobber the REAL slot's
    # diagnostics file, since a private-mutex test run still shares this
    # repo's own .git directory with whatever real build_lock.ps1 invocation
    # (if any) is running alongside it. UNSET -- byte-identical filename,
    # same as always, since $slotName is then the same literal it always was.
    $diagFileName = if ($env:ZENITH_BUILD_SLOT_NAME) {
        'zenith-build-slot-' + ($slotName -replace '[\\/:*?"<>|]', '_') + '.json'
    }
    else {
        'zenith-build-slot.json'
    }
    if ($gitCommonDir) { $diagPath = Join-Path $gitCommonDir $diagFileName }

    $remainingMs = [long]([Math]::Round($effectiveWaitMinutes * 60000.0))
    if ($remainingMs -lt 0) { $remainingMs = 0 }
    $acquired = $false

    while ($true) {
        # `[Math]::Min` per iteration, so a tiny remaining budget still
        # returns promptly rather than blocking for a full poll interval it
        # cannot afford.
        $sliceMs = [Math]::Min($PollMilliseconds, $remainingMs)
        if ($sliceMs -lt 0) { $sliceMs = 0 }
        try {
            $acquired = $mutex.WaitOne($sliceMs)
        }
        catch [System.Threading.AbandonedMutexException] {
            # A killed holder left the mutex abandoned -- .NET still hands us
            # ownership. Counts as ACQUIRED, by design: see the header
            # comment on why this is exactly what makes a Mutex the right
            # primitive here.
            $acquired = $true
        }
        if ($acquired) { break }

        $remainingMs -= $sliceMs
        if ($remainingMs -le 0) { break }

        $holderInfo = Get-ZenithBuildSlotHolderInfo -DiagPath $diagPath
        Write-Host "[build_lock] waiting for build slot...$holderInfo" -ForegroundColor Yellow
    }

    if (-not $acquired) {
        try { $mutex.Dispose() } catch { }
        return $null
    }

    $priorSlotHeldEnv = $env:ZENITH_BUILD_SLOT_HELD
    $priorNodeReuseEnv = $env:MSBUILDDISABLENODEREUSE
    $env:ZENITH_BUILD_SLOT_HELD = '1'
    $env:MSBUILDDISABLENODEREUSE = '1'

    if ($diagPath) {
        Write-ZenithBuildSlotDiagnostics -DiagPath $diagPath -RepoRoot $repoRoot -Reason $Reason
    }

    return [pscustomobject]@{
        Mutex             = $mutex
        Inherited         = $false
        PriorSlotHeldEnv  = $priorSlotHeldEnv
        PriorNodeReuseEnv = $priorNodeReuseEnv
        DiagPath          = $diagPath
    }
}

function Exit-ZenithBuildSlot {
    <#
      Releases a handle from Enter-ZenithBuildSlot. MUST be a safe no-op for
      $null (a timed-out acquire) and for an inherited handle (a nested call
      inside an already-held slot) -- both are called unconditionally from a
      `finally`, so neither may throw or touch state this invocation never
      set up.
    #>
    param($Handle)

    if ($null -eq $Handle) { return }
    if ($Handle.Inherited) { return }

    if ($Handle.DiagPath) {
        try { Remove-Item -LiteralPath $Handle.DiagPath -Force -ErrorAction SilentlyContinue } catch { }
    }

    # Restore whatever was there before -- $null means "was unset", not
    # "restore to the literal string $null".
    if ($null -eq $Handle.PriorSlotHeldEnv) {
        Remove-Item -Path Env:\ZENITH_BUILD_SLOT_HELD -ErrorAction SilentlyContinue
    }
    else {
        $env:ZENITH_BUILD_SLOT_HELD = $Handle.PriorSlotHeldEnv
    }
    if ($null -eq $Handle.PriorNodeReuseEnv) {
        Remove-Item -Path Env:\MSBUILDDISABLENODEREUSE -ErrorAction SilentlyContinue
    }
    else {
        $env:MSBUILDDISABLENODEREUSE = $Handle.PriorNodeReuseEnv
    }

    if ($null -ne $Handle.Mutex) {
        try { $Handle.Mutex.ReleaseMutex() } catch { }
        try { $Handle.Mutex.Dispose() } catch { }
    }
}

# =============================================================================
# `-File` entry point. Detects a dot-source via InvocationName -- dot-sourcing
# this file (`. Tools/build_lock.ps1`) must define the two functions above and
# do NOTHING else, so a test harness (or ZenithCli.psm1/tick_gates.ps1, later)
# can call them directly without accidentally running `-Run`.
# =============================================================================
if ($MyInvocation.InvocationName -eq '.') {
    return
}

if (-not $PSBoundParameters.ContainsKey('Run') -or [string]::IsNullOrEmpty($Run)) {
    # Invoked directly with no -Run: nothing to do. Not an error -- this lets
    # `pwsh -File Tools/build_lock.ps1` be probed harmlessly.
    exit 0
}

$handle = Enter-ZenithBuildSlot -WaitMinutes $WaitMinutes -Reason "build_lock -Run: $Run" -PollMilliseconds $PollMilliseconds
if ($null -eq $handle) {
    Write-Host "[build_lock] slot busy -- timed out waiting" -ForegroundColor Red
    exit 11
}

$exitCode = 1
try {
    # Same technique as tick_gates.ps1's Invoke-GateLine: the command is
    # written to a wrapper .ps1 and run through pwsh rather than split into
    # FilePath + ArgumentList, so it is parsed by the same shell that would
    # have parsed it typed -- splitting would mean re-implementing
    # PowerShell quoting and mangling any quoted argument. Run through
    # `pwsh -File` (a CHILD process) rather than Invoke-Expression in this
    # process, so the handshake env vars set by Enter-ZenithBuildSlot above
    # are inherited by the child exactly the way a nested `zenith build`
    # would see them for real.
    $wrapperDir = [System.IO.Path]::GetTempPath()
    $wrapper = Join-Path $wrapperDir ('build_lock_run_' + [guid]::NewGuid().ToString('N') + '.ps1')
    $body = @(
        '$ErrorActionPreference = ''Continue''',
        $Run,
        'exit $LASTEXITCODE'
    ) -join [Environment]::NewLine
    Set-Content -LiteralPath $wrapper -Value $body -Encoding utf8
    try {
        & pwsh -NoProfile -ExecutionPolicy Bypass -File $wrapper
        $exitCode = $LASTEXITCODE
    }
    finally {
        Remove-Item -LiteralPath $wrapper -Force -ErrorAction SilentlyContinue
    }
}
finally {
    Exit-ZenithBuildSlot $handle
}

exit $exitCode
