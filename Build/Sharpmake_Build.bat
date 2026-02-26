@echo off
REM Zenith Engine - Sharpmake Build Script
REM Generates Visual Studio solution files for all platforms

..\Sharpmake\Sharpmake.Application.exe /sources('Sharpmake_Common.cs', 'Sharpmake_Zenith.cs', 'Sharpmake_FluxCompiler.cs', 'Sharpmake_Games.cs', 'Sharpmake_TilePuzzleLevelGen.cs')

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Sharpmake generation failed!
    pause
    exit /b 1
)

REM Fix AGDE C++ standard: Sharpmake generates "cpp20" but AGDE only accepts "cpp2a"
powershell -Command "Get-ChildItem -Path '%~dp0..' -Recurse -Filter '*_agde.vcxproj' | ForEach-Object { (Get-Content $_.FullName) -replace '<CppLanguageStandard>cpp20</CppLanguageStandard>','<CppLanguageStandard>cpp2a</CppLanguageStandard>' | Set-Content $_.FullName }"

echo.
echo Solution files generated successfully.
echo Generated solutions:
echo   - zenith_win64.sln (Windows)
echo   - zenith_agde.sln (Android)
pause
