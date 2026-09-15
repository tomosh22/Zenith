$ErrorActionPreference = 'Stop'
$root = 'C:/dev/Zenith'
$out = Join-Path $root 'artifacts/signpost-multimaterial'
foreach ($game in @('RenderTest', 'Zenithmon')) {
    $exe = Join-Path $root "Games/$game/Build/output/win64/null_vs2022_debug_win64_true/$($game.ToLower()).exe"
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName = $exe
    $work = Join-Path $out "$game-work"
    New-Item -ItemType Directory -Force -Path $work | Out-Null
    $psi.WorkingDirectory = $work
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.EnvironmentVariables['APPDATA'] = Join-Path $out "$game-appdata"
    $exportFlag = if ($game -eq 'RenderTest') { '' } else { '--skip-tool-exports' }
    $psi.Arguments = "--exit-after-unit-tests $exportFlag --unit-test-timings=$out/$game-unit-timings.json"
    Write-Output "Running $game Null units with workspace save data..."
    $p = [System.Diagnostics.Process]::Start($psi)
    $stdout = $p.StandardOutput.ReadToEndAsync()
    $stderr = $p.StandardError.ReadToEndAsync()
    if (!$p.WaitForExit(600000)) { $p.Kill(); $p.WaitForExit() }
    $log = $stdout.Result + "`n" + $stderr.Result
    [System.IO.File]::WriteAllText((Join-Path $out "$game-units.log"), $log)
    $line = ($log -split "`n" | Where-Object { $_ -match 'Unit tests complete' } | Select-Object -Last 1)
    Write-Output "Exit $($p.ExitCode): $line"
    if ($p.ExitCode -ne 0 -or $line -notmatch '(\d+) ran, (\d+) passed, 0 failed') { throw "$game unit run failed; see saved log" }
}
