$target = 'C:\dev\Zenith\Games\Combat\build\output\win64\vs2022_debug_win64_true\combat.exe'
$found = $false
Get-Process | ForEach-Object {
    try {
        $modules = $_.Modules
        foreach ($m in $modules) {
            if ($m.FileName -eq $target) {
                Write-Output "PID=$($_.Id) Name=$($_.Name) Path=$($_.MainModule.FileName)"
                $found = $true
                break
            }
        }
    } catch {}
}
if (-not $found) { Write-Output "No process loading combat.exe" }
