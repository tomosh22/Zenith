@echo off
REM ==========================================================================
REM RunAnalysis.bat - full-feature complexity analysis for the Zenith engine.
REM
REM Runs analyze_code_complexity.py under the `full-snapshot` profile with every
REM output format and the accurate tree-sitter parser auto-selected when its
REM deps are installed. Outputs land in <repo-root>\complexity_report\:
REM
REM   analysis_report.json    - full machine-readable report (incl. health summary)
REM   analysis_report.md      - human-readable refactoring queue
REM   analysis_report.html    - self-contained interactive dashboard
REM   analysis_report.sarif   - SARIF 2.1.0 findings for IDE integration
REM   file_metrics.csv / directory_metrics.csv / function_metrics.csv
REM   *.png                   - chart suite (needs matplotlib)
REM
REM Scope, ignored macros, and (no) thresholds come from the `full-snapshot`
REM profile in `complexity_profiles.json`. Edit that file to change scope.
REM
REM Tree-sitter is used when tree-sitter + tree-sitter-cpp are installed;
REM the tool falls back to the regex parser with a warning otherwise. Override
REM with `--parser regex` or `--parser tree-sitter` if needed.
REM
REM To install tree-sitter (requires a C compiler on PATH):
REM   pip install tree-sitter tree-sitter-cpp
REM
REM LLM refactor suggestions are NOT enabled here. Add --llm-suggestions (and
REM set ANTHROPIC_API_KEY) via the extra-args pass-through below if you want
REM AI recommendations.
REM
REM Usage:
REM   RunAnalysis.bat
REM   RunAnalysis.bat --fail-on max-cc=60,max-nesting=8
REM   RunAnalysis.bat --llm-suggestions --llm-max 10
REM ==========================================================================

setlocal

set "SCRIPT_DIR=%~dp0"

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
    echo RunAnalysis: Python not found on PATH. Install Python 3.9+ or run via the Windows launcher.
    exit /b 2
)

pushd "%SCRIPT_DIR%.."
%PYTHON_CMD% "%SCRIPT_DIR%analyze_code_complexity.py" . ^
  --profile full-snapshot ^
  --html ^
  --sarif ^
  --csv ^
  -o complexity_report ^
  %*
set "EXITCODE=%ERRORLEVEL%"
popd

endlocal & exit /b %EXITCODE%
