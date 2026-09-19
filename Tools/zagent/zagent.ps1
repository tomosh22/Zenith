#Requires -Version 7.0
<#
.SYNOPSIS
  zagent — the agent board's command line.

.DESCRIPTION
  There is exactly one backend: Zenith Vault, a Node CLI at
  Tools/Vault/cli/client.mjs that IS this checkout — no network
  credential, no separate board machine, nothing to select. This script
  is a thin dispatch shim: it resolves this repo's root (two levels up
  from its own location, since Tools/zagent always lives at
  <repo>/Tools/zagent) and hands off every argument, verbatim, to

      node Tools/Vault/cli/client.mjs --vault <repoRoot> <args...>

  then exits with that process's own exit code.

  A remote-HTTP backend used to be selectable here via
  `Tools/zagent/backend.json`. It was removed along with the file: see
  Tools/zagent/README.md and Tools/Vault/README.md for what replaced it.

.NOTES
  Exit codes are the control flow, and /tick branches on them; see
  Tools/Vault/cli/client.mjs and Tools/Vault/README.md for what each one
  means. This shim adds only one exit code of its own: 1, if `node` or
  the Vault client cannot be found at all.
#>

[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# The board speaks UTF-8 and this client's output is READ BY THINGS: a tick
# redirects it to a file, tick_gates.ps1 appends it to gates.log, and a work
# log quotes it. Windows PowerShell hands a redirected stream the OEM code
# page, so `·` (U+00B7) -- which the queue and doctor summaries use as a
# separator -- went out as the single byte 0xFA, which is not valid UTF-8 at
# all. The em dash was already being folded to `-` elsewhere, so the fold
# table looked complete while one character quietly corrupted every capture.
# Setting the encoding fixes the whole class rather than one more character.
try {
    [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
    $OutputEncoding = [System.Text.UTF8Encoding]::new($false)
} catch {
    # A redirected or absent console can refuse this. Losing a separator is
    # cosmetic; refusing to run the command is not.
}

# There is only one backend, so the repo root IS the vault root -- no
# config file, no branch. $PSScriptRoot is always <repo>/Tools/zagent, so
# two levels up is always <repo>, regardless of the caller's cwd.
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$vaultClientPath = Join-Path $PSScriptRoot '../Vault/cli/client.mjs'
if (-not (Test-Path -LiteralPath $vaultClientPath)) {
    [Console]::Error.WriteLine("Vault client is missing: $vaultClientPath")
    exit 1
}

& node $vaultClientPath --vault $repoRoot @Arguments
exit $LASTEXITCODE
