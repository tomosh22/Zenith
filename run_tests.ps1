Set-Location 'c:\dev\Zenith\Games\Test\Build\output\win64\vs2022_debug_win64_true'
.\test.exe > testoutput.txt 2>&1
Get-Content testoutput.txt | Where-Object { $_ -match '\[.*Test' -or $_ -match 'passed|PASSED|failed|FAILED|Assert' } | Select-Object -Last 150
