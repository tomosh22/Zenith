@echo off
REM Zenith Engine - Sharpmake Build Script
REM Generates Visual Studio solution files for all platforms

..\Sharpmake\Sharpmake.Application.exe /sources('Sharpmake_Common.cs', 'Sharpmake_FreeType.cs', 'Sharpmake_Msdfgen.cs', 'Sharpmake_MsdfAtlasGen.cs', 'Sharpmake_Zenith.cs', 'Sharpmake_FluxCompiler.cs', 'Sharpmake_Games.cs', 'Sharpmake_TilePuzzleLevelGen.cs', 'Sharpmake_TilePuzzleRegistryViewer.cs')

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Sharpmake generation failed!
    pause
    exit /b 1
)

REM Fix AGDE vcxproj: C++ standard (cpp20->cpp2a) and inject UBSan flags for debug builds
powershell -ExecutionPolicy Bypass -File "%~dp0fix_agde_vcxproj.ps1"

echo.
echo Solution files generated successfully.
echo Generated solutions:
echo   - zenith_win64.sln (Windows)
echo   - zenith_agde.sln (Android)
echo.
echo AGDE debug builds have UBSan enabled (-fsanitize=undefined)
