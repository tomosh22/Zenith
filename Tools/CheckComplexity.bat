@echo off
REM ==========================================================================
REM CheckComplexity.bat - CI gate for code complexity regressions.
REM
REM Runs analyze_code_complexity.py with --fail-on thresholds so any function
REM that exceeds them causes a non-zero exit. Intended for CI pipelines:
REM
REM   - Exit 0 -> no function crosses any threshold. Safe to merge.
REM   - Exit 1 -> at least one function is too hot. Build fails.
REM
REM Scope: engine source only. Games/*, Tools/*, TilePuzzleLevelGen,
REM TilePuzzleRegistryViewer, and UnitTests are excluded:
REM   - Games/Tools contain gameplay/editor-tooling logic where a wide
REM     function is often a one-off dispatch, not engine quality.
REM   - UnitTests/* RunAllTests are call-lists by design; their "LOC" is
REM     just a list of test invocations and has no semantic complexity.
REM
REM Thresholds (set to current worst-case engine ceiling + small headroom,
REM NOT aspirational targets):
REM   max-cc=100       - cyclomatic complexity of any function <= 100
REM   max-cognitive=150 - cognitive complexity of any function <= 150
REM   max-nesting=8    - nesting depth of any function <= 8
REM   max-loc=900      - code lines in any function <= 900
REM
REM Current ceiling is set by Zenith_EditorAutomation::ExecuteAction (CC=88,
REM LOC=885) — a large action-dispatch switch that is a known refactor
REM target. Tighten the thresholds once that function is factored. The point
REM of this gate is to keep things from getting *worse*, not to pretend the
REM codebase is already clean.
REM
REM Usage:
REM   CheckComplexity.bat
REM   CheckComplexity.bat --parser tree-sitter     (accurate parser)
REM ==========================================================================

setlocal

set "SCRIPT_DIR=%~dp0"

pushd "%SCRIPT_DIR%.."
python "%SCRIPT_DIR%analyze_code_complexity.py" . ^
  --no-viz ^
  --ignore-macros ZENITH_ASSERT ^
  -e Games Tools TilePuzzleLevelGen TilePuzzleRegistryViewer UnitTests Middleware ThirdParty External vendor build Build .git .claude ^
  -o complexity_report_ci ^
  --fail-on max-cc=100,max-cognitive=150,max-nesting=8,max-loc=900 ^
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
