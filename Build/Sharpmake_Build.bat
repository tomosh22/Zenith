@echo off
REM Zenith Engine - Sharpmake Build Script
REM Generates Visual Studio solution files for all platforms

..\Sharpmake\Sharpmake.Application.exe /sources('Sharpmake_Common.cs', 'Sharpmake_Zenith.cs', 'Sharpmake_FluxCompiler.cs', 'Sharpmake_Games.cs')

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Sharpmake generation failed!
    pause
    exit /b 1
)

echo.
echo Solution files generated successfully.
echo Generated solutions:
echo   - zenith_win64.sln (Windows)
echo   - zenith_agde.sln (Android)
pause
