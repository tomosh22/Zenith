Set-Location 'C:\dev\Zenith\Games\TilePuzzle\Build\output\win64\vs2022_debug_win64_true'
$p = Start-Process -FilePath '.\tilepuzzle.exe' -ArgumentList '--autotest' -NoNewWindow -PassThru -RedirectStandardOutput 'C:\dev\Zenith\autotest_log.txt' -RedirectStandardError 'C:\dev\Zenith\autotest_err.txt'
Start-Sleep -Seconds 1800
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
