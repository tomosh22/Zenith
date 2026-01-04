Set-Location 'c:\dev\Zenith\Games\Sokoban\Build\output\win64\vs2022_debug_win64_true'
Remove-Item -Path 'sokoban_test_out.txt' -ErrorAction SilentlyContinue
.\sokoban.exe > sokoban_test_out.txt 2>&1
Get-Content sokoban_test_out.txt | Select-String -Pattern 'completed|FAILED|Assert|passed|Running.*Test' | Select-Object -Last 250
