@echo off
REM zenith CLI shim -> Tools/zenith.ps1 (see Tools/ZenithCli/ZenithCli.psm1).
REM Usage: zenith <new|open|list|regen|build|run|hub|selftest> [args]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\zenith.ps1" %*
exit /b %ERRORLEVEL%
