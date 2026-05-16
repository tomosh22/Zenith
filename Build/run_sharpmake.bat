@echo off
REM Non-interactive Sharpmake runner. Same effect as Sharpmake_Build.bat
REM (regen vcxprojs + apply AGDE fix-ups) but without the `pause` directive
REM on failure -- caller checks %ERRORLEVEL%.
pushd "%~dp0"
..\Sharpmake\Sharpmake.Application.exe /sources('Sharpmake_Common.cs', 'Sharpmake_Zenith.cs', 'Sharpmake_FluxCompiler.cs', 'Sharpmake_Games.cs', 'Sharpmake_TilePuzzleLevelGen.cs', 'Sharpmake_TilePuzzleRegistryViewer.cs')
set RC=%ERRORLEVEL%
if %RC% NEQ 0 goto end
REM Apply AGDE fix-ups (cpp20->cpp2a, UBSan injection). Mirrors line 15 of
REM Sharpmake_Build.bat. Skipping this leaves AGDE projects half-configured
REM and causes spurious diffs against master.
powershell -ExecutionPolicy Bypass -NoProfile -File "%~dp0fix_agde_vcxproj.ps1"
set RC=%ERRORLEVEL%
:end
popd
exit /b %RC%
