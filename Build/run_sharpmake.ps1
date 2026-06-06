# Helper to run Sharpmake without hitting the `pause` in Sharpmake_Build.bat.
# The .bat hangs CI / autonomous agents because of the pause directive.
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

# Use single-quoted string so PowerShell does not expand the embedded
# double-quotes; pass straight through to Sharpmake unchanged.
$cmd = '..\Sharpmake\Sharpmake.Application.exe /sources("Sharpmake_Common.cs", "Sharpmake_FreeType.cs", "Sharpmake_Msdfgen.cs", "Sharpmake_MsdfAtlasGen.cs", "Sharpmake_Zenith.cs", "Sharpmake_ZenithECS.cs", "Sharpmake_SentinelECS.cs", "Sharpmake_FluxCompiler.cs", "Sharpmake_Games.cs", "Sharpmake_TilePuzzleLevelGen.cs", "Sharpmake_TilePuzzleRegistryViewer.cs")'
Invoke-Expression $cmd
exit $LASTEXITCODE
