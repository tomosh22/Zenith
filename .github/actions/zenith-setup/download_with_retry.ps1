# download_with_retry.ps1 -- retried Invoke-WebRequest for CI provisioning.
# The three provisioning downloads (LunarG installer / runtime zip, Slang
# release zip) are the flakiest part of every workflow run; a transient CDN
# hiccup should not fail a 60-minute gate. 3 attempts, 10/30/90 s backoff.
# ASCII-only body; CI invokes with pwsh.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Uri,
    [Parameter(Mandatory)][string]$OutFile,
    [int]$MaxAttempts = 3
)

$ErrorActionPreference = 'Stop'
$delays = @(10, 30, 90)
for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
    try {
        Write-Host "download attempt $attempt/$MaxAttempts : $Uri"
        Invoke-WebRequest -Uri $Uri -OutFile $OutFile -UseBasicParsing
        Write-Host "downloaded -> $OutFile"
        return
    }
    catch {
        if ($attempt -eq $MaxAttempts) { throw }
        $s = $delays[$attempt - 1]
        Write-Host "attempt $attempt failed ($($_.Exception.Message)); retrying in $s s"
        Start-Sleep -Seconds $s
    }
}
