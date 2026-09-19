<#
.SYNOPSIS
  Pure helpers shared by the `zagent` tooling.

.DESCRIPTION
  `zagent.ps1` is now a thin dispatch shim straight into the Vault CLI
  (Tools/Vault/cli/client.mjs) and needs none of these itself. What
  remains here is the handful of functions `build_prompt.ps1` still
  calls standalone, via `pwsh -File`, independent of `zagent.ps1` and
  of which backend (there is only one now) is in play:

    * Find-ClientRepo   — locate this checkout from any subdirectory
    * ConvertTo-PosixPath — the path-normalisation form the docs mirror
                             and the board key off
    * Write-StdErr      — the one place stderr output is written
    * Get-ScratchRoot   — where a ticket's run scratch lives
    * Write-LastExit    — record an exit code where a pipe cannot
                           corrupt it

  Everything that used to live here for the legacy HTTP transport —
  argument parsing, the docs-tree upload, drift/citation checks, the
  wire contract, `doctor`'s client-side checks, and so on — was removed
  with that backend. `Tools/Vault/cli/tick-checks.mjs` and the Vault
  CLI's own code are the live equivalent for anything still needed.

  `Test-ZagentClient.ps1` locks down `ConvertTo-PosixPath`, whose
  bug — a separator mismatch making an uploaded docs tree match
  nothing, which mirrors zero pages and reports success — was found the
  hard way.
#>

Set-StrictMode -Version Latest

$script:PROJECT_FILE = 'zagent.project.json'

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

function Write-StdErr {
    param([string]$Message)
    [Console]::Error.WriteLine($Message)
}

# ─── RUN SCRATCH ──────────────────────────────────────

function Get-ScratchRoot {
    param([string]$Repo)
    return (Join-Path (Join-Path $Repo '.zagent') 'run')
}

<#
★★ THE EXIT CODE, AS A FILE. This is the one value the protocol tells you
to trust and never gave you a way to read safely.

`tick.md` spends forty lines and three measured war stories on "NEVER PIPE
OR SUFFIX A CALL WHOSE EXIT CODE YOU INTEND TO READ", and it is still the
most-broken rule in the protocol. Two more breaks on 2026-08-25 alone:
`zagent check ZM-70 2>&1 | head; echo $?` read 0 where the real code was
5, and the harness's own completion notification reported 0 for a
`tick_gates` run that exited 10 (a pin needing a bump). Branching on
either commits a stale baseline with every gate reported green.

The asymmetry is the whole defect. EVERY other value the protocol says to
read from a file already IS one — `ticket.json`, `body.md`, `drift.txt`,
`gates.json`, `tick_gates.json` — precisely because a pipe corrupts them.
The exit code was reachable only through the channel the protocol says
never to trust. `tick_gates.ps1` solved this for itself with
`tick_gates.json`, which is exactly why its verdict survived a run whose
shell status did not.

So: one line beside `last.json`, written on EVERY exit path including the
early ones, and read the way `last.json` is read. A rule enforced by a
file beats a rule repeated in prose.
#>
function Write-LastExit {
    param([int]$Code, [string]$Repo)
    try {
        if (-not $Repo) { return }
        $dir = Join-Path $Repo '.zagent'
        if (-not (Test-Path -LiteralPath $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }
        [System.IO.File]::WriteAllText((Join-Path $dir 'last.exit'), "$Code`n",
            [System.Text.UTF8Encoding]::new($false))
    } catch {
        # Best effort — never let recording the code change the code.
    }
}

Export-ModuleMember -Function Find-ClientRepo, ConvertTo-PosixPath, Write-StdErr, Get-ScratchRoot, Write-LastExit
