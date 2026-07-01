@echo off
REM ==========================================================================
REM CheckMemoryBudget.bat - CI gate for memory-budget regressions.
REM
REM Runs check_memory_budget.py against the committed baseline
REM (Tools/memory_budget_baseline.json). Two modes:
REM
REM   CheckMemoryBudget.bat
REM       RATCHET mode (no live run). Validates the baseline itself: every
REM       budgeted entry must have budget_bytes >= baseline_peak_bytes. Zero
REM       external state -> always runnable in CI. Exit 1 on any inconsistency.
REM
REM   CheckMemoryBudget.bat --csv zenith_memory_dump.csv
REM       LIVE mode. Compares actual peaks from a `--memory-dump` CSV against
REM       the baseline (budget cap + regression tolerance). ci_capturable=false
REM       entries (VRAM/RENDERER) are skipped unless --include-gpu.
REM
REM Budgets are checked-in CEILINGS: shrink them, don't grow them. A justified
REM regression must update Tools/memory_budget_baseline.json in the same commit.
REM ==========================================================================

setlocal

set "SCRIPT_DIR=%~dp0"

REM Resolve a Python interpreter. Use && (runtime exit-code) + `if not defined`
REM (a runtime check) rather than %ERRORLEVEL% inside a parenthesized if/else block:
REM cmd.exe expands %ERRORLEVEL% for the whole block at PARSE time, so the nested test
REM would read the stale value from `where py`, not the fresh `where python`.
set "PYTHON_CMD="
where py >nul 2>nul && set "PYTHON_CMD=py -3"
if not defined PYTHON_CMD (
    where python >nul 2>nul && set "PYTHON_CMD=python"
)
if not defined PYTHON_CMD (
    echo CheckMemoryBudget: Python not found on PATH. Install Python 3.9+ or run via the Windows launcher.
    exit /b 2
)

%PYTHON_CMD% "%SCRIPT_DIR%check_memory_budget.py" %*
set "EXITCODE=%ERRORLEVEL%"

if %EXITCODE% NEQ 0 (
    echo.
    echo ============================================================
    echo MEMORY BUDGET GATE FAILED
    echo A memory budget was exceeded, regressed, or is inconsistent
    echo with its recorded peak. See Tools\memory_budget_baseline.json.
    echo Budgets are ceilings: shrink them, don't grow them.
    echo ============================================================
)

endlocal ^& exit /b %EXITCODE%
