# Thin forwarder to Build/regen.ps1 (the canonical Zenith regenerator).
# Kept so existing muscle-memory / docs that call run_sharpmake.ps1 keep working.
# The real logic -- descriptor validation, codegen, glob-based /sources, single
# Sharpmake run, AGDE fixup, stale-sln cleanup -- lives in regen.ps1.
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'regen.ps1') @args
exit $LASTEXITCODE
