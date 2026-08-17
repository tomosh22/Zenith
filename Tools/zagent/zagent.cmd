@echo off
REM zagent — shim so a bare `zagent …` works from cmd, PowerShell and Git
REM Bash once Tools\zagent is on PATH.
REM
REM %* is forwarded UNQUOTED-BUT-INTACT via -File, which does not re-split
REM on whitespace the way -Command would. A gate line like
REM   --text "two words"
REM survives; through -Command it would arrive as two arguments.
REM
REM `exit /b` propagates the script's own code, because 3/4/5/6 are
REM ordinary control flow for this CLI and the tick branches on them.
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0zagent.ps1" %*
exit /b %ERRORLEVEL%
