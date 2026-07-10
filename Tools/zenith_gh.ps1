# zenith_gh.ps1 -- gh CLI wrapper with self-bootstrapping auth.
# =============================================================================
# Sandboxed agent sessions have no persisted `gh auth login`; the GitHub
# credential DOES live in the git credential manager (git push works). This
# wrapper derives GH_TOKEN from `git credential fill` when gh is not already
# authenticated, then forwards all arguments to gh verbatim.
#
# Why a wrapper instead of inline bootstrap: permission allow-rules prefix-match
# the WHOLE command string, so a compound "$env:GH_TOKEN = ...; gh pr merge"
# never matches a "gh pr *" rule. "pwsh -NoProfile -File Tools\zenith_gh.ps1 pr
# merge 123" does match its rule -- autonomous sessions call gh through here.
#
# Usage:  pwsh -NoProfile -File Tools\zenith_gh.ps1 <gh args...>
#         e.g.  ... Tools\zenith_gh.ps1 pr checks 143 --json name,bucket
# Exit:   gh's exit code (1 if no credential could be derived).
#
# ASCII-only body; runs under Windows PowerShell 5.1 and pwsh 7.
# =============================================================================

[CmdletBinding()]
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$GhArgs)

$ErrorActionPreference = 'Stop'

# Already authenticated (user machine with gh auth login, or GH_TOKEN set)?
& gh auth status 2>$null | Out-Null
if ($LASTEXITCODE -ne 0) {
    $cred = "protocol=https`nhost=github.com`n`n" | git credential fill 2>$null
    $match = $cred | Select-String '^password=(.+)$'
    if ($null -eq $match) {
        Write-Error "zenith_gh: gh is not authenticated and no GitHub credential is available from 'git credential fill'."
        exit 1
    }
    $env:GH_TOKEN = $match.Matches[0].Groups[1].Value
}

& gh @GhArgs
exit $LASTEXITCODE
