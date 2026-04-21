@echo off
REM ==========================================================================
REM RunAnalysis.bat - full-feature complexity analysis for the Zenith engine.
REM
REM Runs analyze_code_complexity.py with every output format and the accurate
REM tree-sitter parser enabled. Outputs land in <repo-root>\complexity_report\:
REM
REM   analysis_report.json    - full machine-readable report
REM   analysis_report.md      - human-readable refactoring queue
REM   analysis_report.html    - self-contained interactive dashboard
REM   analysis_report.sarif   - SARIF 2.1.0 findings for IDE integration
REM   file_metrics.csv / directory_metrics.csv / function_metrics.csv
REM   *.png                   - chart suite (needs matplotlib)
REM
REM Tree-sitter is used when tree-sitter + tree-sitter-cpp are installed;
REM the tool falls back to the regex parser with a warning otherwise.
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

pushd "%SCRIPT_DIR%.."
python "%SCRIPT_DIR%analyze_code_complexity.py" . ^
  --parser tree-sitter ^
  --html ^
  --sarif ^
  --csv ^
  --ignore-macros ZENITH_ASSERT ^
  -o complexity_report ^
  %*
set "EXITCODE=%ERRORLEVEL%"
popd

endlocal & exit /b %EXITCODE%
