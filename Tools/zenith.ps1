# zenith.ps1 -- dispatcher for the Zenith game CLI.
# Imports the CLI module and forwards all args to Invoke-ZenithCli, which returns
# the process exit code as its last pipeline value (any leading values -- e.g.
# `list --json` output -- are passed through to stdout).
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'ZenithCli/ZenithCli.psm1') -Force

$rc = Invoke-ZenithCli @args
$code = 0
if ($null -ne $rc) {
    $arr = @($rc)
    $code = $arr[-1]
    if ($arr.Count -gt 1) { $arr[0..($arr.Count - 2)] }
}
exit ([int]$code)
