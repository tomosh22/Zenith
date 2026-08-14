param(
	[int]$BaselineFrames = 120,
	[int]$DebugFrames = 120,
	[int]$RegenerateFrames = 90,
	[int]$ForcedPaddedFrames = 240,
	[int]$TimeoutSeconds = 900,
	[switch]$NoBuild
)

$ErrorActionPreference = "Stop"
$SmokeScript = Join-Path $PSScriptRoot "RunRenderTestSmoke.ps1"

Write-Host "[RenderTestSmokeMatrix] 1/5 auto/native baseline terrain smoke"
if ($NoBuild) {
	& $SmokeScript -NoBuild -Frames $BaselineFrames -TimeoutSeconds $TimeoutSeconds
}
else {
	& $SmokeScript -Frames $BaselineFrames -TimeoutSeconds $TimeoutSeconds
}

Write-Host "[RenderTestSmokeMatrix] 2/5 forced-padded terrain smoke"
& $SmokeScript -NoBuild -Frames $ForcedPaddedFrames -TimeoutSeconds $TimeoutSeconds -ForcedIndirectCountPadded

Write-Host "[RenderTestSmokeMatrix] 3/5 forced-single terrain smoke"
& $SmokeScript -NoBuild -Frames $ForcedPaddedFrames -TimeoutSeconds $TimeoutSeconds -ForcedIndirectCountSingle

# Phase 8 of the terrain indirect-count compatibility plan: a forced-padded
# terrain smoke case. The padded tier is the shipping fallback for no-count
# Android Vulkan ICDs; the smoke matrix must not require a screenshot here
# (RunRenderTestSmoke.ps1 still drives the resource/scene smoke, the
# dedicated graphics A/B wrapper RunTerrainIndirectCompatibility.ps1 handles
# the screenshot gate). This case fails closed if Vulkan validation /
# synchronization errors fire OR the retired "terrain will not render /
# streaming disabled" warning re-appears. RunRenderTestSmoke also requires its
# exact RENDERTEST_SMOKE_INDIRECT_TIER evidence, so these forced arms cannot
# silently fall back to auto/native if CLI-to-backend propagation regresses.
Write-Host "[RenderTestSmokeMatrix] 4/5 forced-padded LOD-debug + wireframe terrain smoke"
& $SmokeScript -NoBuild -Frames $DebugFrames -TimeoutSeconds $TimeoutSeconds -LodDebug -Wireframe -ForcedIndirectCountPadded

Write-Host "[RenderTestSmokeMatrix] 5/5 forced procedural-regeneration terrain smoke"
& $SmokeScript -NoBuild -Frames $RegenerateFrames -TimeoutSeconds $TimeoutSeconds -ForceRegenerate

Write-Host "[RenderTestSmokeMatrix] PASS"
