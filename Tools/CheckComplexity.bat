@echo off
REM ==========================================================================
REM CheckComplexity.bat - CI gate for code complexity regressions.
REM
REM Runs analyze_code_complexity.py under the `engine-ci` profile so any function
REM that exceeds the profile's --fail-on thresholds causes a non-zero exit.
REM Intended for CI pipelines:
REM
REM   - Exit 0 -> no function crosses any threshold. Safe to merge.
REM   - Exit 1 -> at least one function is too hot. Build fails.
REM
REM All scope and threshold configuration lives in `complexity_profiles.json`
REM (next to this script) under the `engine-ci` key. To tune the gate, edit that
REM file rather than this batch script.
REM
REM Profile thresholds are checked-in CEILINGS, not aspirational targets. The
REM gate keeps things from getting worse, not the definition of clean code.
REM
REM Usage:
REM   CheckComplexity.bat
REM   CheckComplexity.bat --parser tree-sitter   (force tree-sitter parser)
REM   CheckComplexity.bat --no-md --no-json      (gate-only, skip reports)
REM ==========================================================================

setlocal

set "SCRIPT_DIR=%~dp0"

REM Resolve a Python interpreter. Prefer `py -3` (Windows launcher), then plain
REM `python`. Bail with a one-line message if neither is on PATH.
set "PYTHON_CMD="
where py >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set "PYTHON_CMD=py -3"
) else (
    where python >nul 2>nul
    if %ERRORLEVEL% EQU 0 (
        set "PYTHON_CMD=python"
    )
)
if "%PYTHON_CMD%"=="" (
    echo CheckComplexity: Python not found on PATH. Install Python 3.9+ or run via the Windows launcher.
    exit /b 2
)

pushd "%SCRIPT_DIR%.."
%PYTHON_CMD% "%SCRIPT_DIR%analyze_code_complexity.py" . ^
  --profile engine-ci ^
  --no-viz ^
  -o complexity_report_ci ^
  %*
set "EXITCODE=%ERRORLEVEL%"
popd

if %EXITCODE% NEQ 0 (
    echo.
    echo ============================================================
    echo COMPLEXITY GATE FAILED
    echo One or more engine functions exceeds the complexity budget.
    echo See complexity_report_ci\analysis_report.md for the refactor
    echo queue sorted by priority score.
    echo ============================================================
)

endlocal & exit /b %EXITCODE%
