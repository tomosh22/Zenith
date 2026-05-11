@echo off
REM ============================================================================
REM run_sweep.bat
REM Drives Tools/dp_export/sweep.py inside Unreal Editor 5.6 in commandlet mode.
REM
REM Why a commandlet and not "open editor, run script": running sweep.py
REM interactively requires a human to: launch the editor, open the level,
REM trigger the Python script, copy the output. The commandlet form is
REM unattended — call this from a build script, CI, or directly from the
REM command line, and every actor placement + SCS child mesh transform lands
REM in Games/DevilsPlayground/Assets/Maps/*.json without UI interaction.
REM
REM Assumes the default UE 5.6 install location. If you've installed UE
REM elsewhere, set UE_BIN_DIR before running:
REM     set UE_BIN_DIR=D:\UnrealEngine\UE_5.6\Engine\Binaries\Win64
REM     run_sweep.bat
REM ============================================================================

setlocal

if not defined UE_BIN_DIR (
    set "UE_BIN_DIR=C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64"
)

set "UE_CMD=%UE_BIN_DIR%\UnrealEditor-Cmd.exe"
set "PROJECT=C:\dev\GameJam0\GameJam0.uproject"
set "SCRIPT_DIR=%~dp0"
set "SCRIPT=%SCRIPT_DIR%sweep.py"

if not exist "%UE_CMD%" (
    echo ERROR: UnrealEditor-Cmd.exe not found at: %UE_CMD%
    echo Set UE_BIN_DIR to override the install location.
    exit /b 1
)
if not exist "%PROJECT%" (
    echo ERROR: UE project not found at: %PROJECT%
    exit /b 1
)
if not exist "%SCRIPT%" (
    echo ERROR: sweep.py not found at: %SCRIPT%
    exit /b 1
)

REM -run=pythonscript : run a single Python file then exit, no GUI.
REM -unattended       : skip every prompt (saves, asset migrations, etc.).
REM -nopause          : don't pause on exit even on error.
REM -nosplash         : skip splash screen rendering (faster start).
REM -nullrhi          : no rendering — sweep.py only reads asset data, never
REM                     submits draws, so the null RHI saves several seconds
REM                     of GPU init and avoids needing a display.
REM
REM Note: ExecutePython picks the script up after the world is initialised
REM enough for AssetRegistry queries to succeed; sweep.py itself loads each
REM map via EditorLoadingAndSavingUtils.load_map.
echo Running sweep.py via UE 5.6 commandlet...
echo   Project: %PROJECT%
echo   Script:  %SCRIPT%
echo.
"%UE_CMD%" "%PROJECT%" -run=pythonscript -script="%SCRIPT%" -unattended -nopause -nosplash -nullrhi -log

set "EXIT=%ERRORLEVEL%"
echo.
echo sweep finished, exit code = %EXIT%
echo Output: C:\dev\Zenith\Games\DevilsPlayground\Assets\Maps\*.json
echo Log:    C:\tmp\dp_sweep.log
endlocal
exit /b %EXIT%
