@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM TilePuzzle Level Generator - Full Pipeline
REM Generates levels for all difficulty tiers and copies them to Assets/Levels
REM ============================================================================

set LEVELGEN="%~dp0output\win64\vs2022_release_win64_false\tilepuzzlelevelgen.exe"
set ASSETS_DIR="%~dp0..\Games\TilePuzzle\Assets\Levels"
set TEMP_BASE=%TEMP%\levelgen

REM Check the executable exists
if not exist %LEVELGEN% (
    echo Error: Release build not found at %LEVELGEN%
    echo Build the TilePuzzleLevelGen project in Release configuration first.
    exit /b 1
)

REM Create output directory
if not exist %ASSETS_DIR% mkdir %ASSETS_DIR%

REM ---- Tier definitions ----
REM Format: tier_name count timeout
set TIER[0]=tutorial 10 120
set TIER[1]=easy 15 120
set TIER[2]=medium 20 300
set TIER[3]=hard 20 300
set TIER[4]=expert 15 300
set TIER[5]=master 20 600
set NUM_TIERS=6

echo ============================================
echo  TilePuzzle Level Generation
echo ============================================
echo.

REM ---- Generate each tier into a temp directory ----
for /L %%t in (0,1,5) do (
    for /f "tokens=1,2,3" %%a in ("!TIER[%%t]!") do (
        set TIER_NAME=%%a
        set TIER_COUNT=%%b
        set TIER_TIMEOUT=%%c
        set TIER_DIR=%TEMP_BASE%_!TIER_NAME!

        echo [%%a] Generating %%b levels ^(timeout: %%cs^)...
        %LEVELGEN% --count %%b --tier %%a --output "!TIER_DIR!" --timeout %%c
        if errorlevel 1 (
            echo Error: Generation failed for tier %%a
            exit /b 1
        )
        echo.
    )
)

REM ---- Clear existing levels ----
echo Clearing %ASSETS_DIR%...
del /q %ASSETS_DIR%\level_*.tlvl 2>nul
del /q %ASSETS_DIR%\level_*.png 2>nul

REM ---- Combine all tiers with sequential numbering ----
echo Combining levels into %ASSETS_DIR%...
set LEVEL_NUM=0

for /L %%t in (0,1,5) do (
    for /f "tokens=1" %%a in ("!TIER[%%t]!") do (
        set TIER_DIR=%TEMP_BASE%_%%a

        REM Copy each .tlvl file from this tier in order
        for /f "tokens=*" %%f in ('dir /b /on "!TIER_DIR!\level_*.tlvl" 2^>nul') do (
            set /a LEVEL_NUM+=1
            set PADDED=000!LEVEL_NUM!
            set PADDED=!PADDED:~-4!

            copy /y "!TIER_DIR!\%%f" %ASSETS_DIR%\level_!PADDED!.tlvl >nul

            REM Copy matching .png if it exists
            set PNG_NAME=%%~nf.png
            if exist "!TIER_DIR!\!PNG_NAME!" (
                copy /y "!TIER_DIR!\!PNG_NAME!" %ASSETS_DIR%\level_!PADDED!.png >nul
            )
        )
        echo   %%a: copied to levels !LEVEL_NUM!
    )
)

echo.
echo ============================================
echo  Done: !LEVEL_NUM! levels in %ASSETS_DIR%
echo ============================================

REM ---- Cleanup temp directories ----
for /L %%t in (0,1,5) do (
    for /f "tokens=1" %%a in ("!TIER[%%t]!") do (
        rmdir /s /q "%TEMP_BASE%_%%a" 2>nul
    )
)

endlocal
