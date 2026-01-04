Set-Location 'c:\dev\Zenith\Games\Test\Build\output\win64\vs2022_debug_win64_true'
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outputFile = "test_output_$timestamp.txt"
.\test.exe > $outputFile 2>&1
Write-Output "Output written to: $outputFile"
Get-Content $outputFile | Select-String -Pattern 'TestEntityIsTrivialSize|TestEntityNameFromScene|completed successfully|FAILED|Assertion failed' | Select-Object -Last 50
