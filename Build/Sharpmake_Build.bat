@echo off
REM Zenith Engine - Sharpmake Build Script (thin forwarder).
REM The real logic lives in Build/regen.ps1: descriptor validation, codegen of
REM Sharpmake_GameInstances.generated.cs, glob-based /sources, a single Sharpmake
REM run (engine sln + all per-game slns), AGDE fixup, and stale-sln cleanup.

powershell -ExecutionPolicy Bypass -File "%~dp0regen.ps1" %*

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Sharpmake regeneration failed!
    pause
    exit /b 1
)
