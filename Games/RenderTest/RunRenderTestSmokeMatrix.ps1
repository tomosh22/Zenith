param(
	[int]$BaselineFrames = 120,
	[int]$DebugFrames = 120,
	[int]$RegenerateFrames = 90,
	[int]$TimeoutSeconds = 900,
	[switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$SmokeScript = Join-Path $PSScriptRoot "RunRenderTestSmoke.ps1"

Write-Host "[RenderTestSmokeMatrix] 1/3 baseline terrain smoke"
if ($NoBuild) {
	& $SmokeScript -NoBuild -Frames $BaselineFrames -TimeoutSeconds $TimeoutSeconds
}
else {
	& $SmokeScript -Frames $BaselineFrames -TimeoutSeconds $TimeoutSeconds
}

Write-Host "[RenderTestSmokeMatrix] 2/3 LOD-debug + wireframe terrain smoke"
& $SmokeScript -NoBuild -Frames $DebugFrames -TimeoutSeconds $TimeoutSeconds -LodDebug -Wireframe

Write-Host "[RenderTestSmokeMatrix] 3/3 forced procedural-regeneration terrain smoke"
& $SmokeScript -NoBuild -Frames $RegenerateFrames -TimeoutSeconds $TimeoutSeconds -ForceRegenerate

Write-Host "[RenderTestSmokeMatrix] PASS"
