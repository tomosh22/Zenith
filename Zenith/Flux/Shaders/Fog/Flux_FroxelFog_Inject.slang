// =====================================================================
// Flux_FroxelFog_Inject — Slang module.
//
// Subsystem: Fog. Builds the fog density volume for the froxel-based
// volumetric fog technique. One thread per froxel cell (8x8x8 group).
// Output: RWTexture3D with R=density, G=scattering, B=absorption.
//
// Density combines a uniform base term, a height-based exponential
// falloff, and an animated 3D noise texture for natural variation.
//
// Imports Common.Frame for FrameConstants (g_xInvProjMat, g_xInvViewMat)
// and Common.Volumetric for SAFE_NEAR_Z / SAFE_DEPTH_RATIO,
// HeightFogDensity, and UIToPhysicalDensity.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Volumetric;

struct InjectConstantsLayout
{
	float4 u_xFogParams;        // x = density, y = scattering, z = absorption, w = time
	float4 u_xNoiseParams;      // x = scale, y = speed, z = detail, w = unused
	float4 u_xHeightParams;     // x = base, y = falloff, z = min, w = max
	float4 u_xGridDimensions;   // x = w, y = h, z = depth
	float  u_fNearZ;
	float  u_fFarZ;
	uint   u_uFrameIndex;
	float  u_fPad0;
};
// PASS frequency — declared AFTER the spine #include so it lands at set 3.
struct PassParams
{
	ConstantBuffer<InjectConstantsLayout> InjectConstants;
	Sampler3D u_xNoiseTexture3D;
	RWTexture3D<float4> u_xDensityGrid;
};
ParameterBlock<PassParams> g_xPassSet;

// Reconstruct world-space position for the centre of a froxel cell.
// The view ray is rebuilt from the inverse projection at the froxel's
// XY UV, then scaled to the slice's exponential depth and finally
// transformed to world by the inverse view matrix.
float3 FroxelToWorldPosition(int3 froxelCoord)
{
	float2 uv = (float2(froxelCoord.xy) + 0.5) / g_xPassSet.InjectConstants.u_xGridDimensions.xy;

	float nearZ       = max(g_xPassSet.InjectConstants.u_fNearZ, SAFE_NEAR_Z);
	float farZ        = max(g_xPassSet.InjectConstants.u_fFarZ, nearZ * SAFE_DEPTH_RATIO);
	float sliceNorm   = ((float)froxelCoord.z + 0.5) / g_xPassSet.InjectConstants.u_xGridDimensions.z;
	float linearDepth = nearZ * pow(farZ / nearZ, sliceNorm);

	float2 ndc     = uv * 2.0 - 1.0;
	float4 clipPos = float4(ndc, 0.0, 1.0);
	float4 viewPos = mul(GetInvProjMat(), clipPos);
	viewPos /= viewPos.w;

	// Scale view position so its z reaches linearDepth exactly. Engine uses
	// +Z forward in view space, so abs() guards the rare clipPos.z=0 case.
	float  viewZ          = max(abs(viewPos.z), SAFE_NEAR_Z);
	float3 scaledViewPos  = viewPos.xyz * (linearDepth / viewZ);

	float4 worldPos = mul(GetInvViewMat(), float4(scaledViewPos, 1.0));
	return worldPos.xyz;
}

[shader("compute")]
[numthreads(8, 8, 8)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	int3 froxelCoord = (int3)dispatchId;

	if (froxelCoord.x >= (int)g_xPassSet.InjectConstants.u_xGridDimensions.x ||
		froxelCoord.y >= (int)g_xPassSet.InjectConstants.u_xGridDimensions.y ||
		froxelCoord.z >= (int)g_xPassSet.InjectConstants.u_xGridDimensions.z) return;

	float3 worldPos = FroxelToWorldPosition(froxelCoord);

	float baseDensity   = g_xPassSet.InjectConstants.u_xFogParams.x;
	float heightDensity = HeightFogDensity(worldPos.y, g_xPassSet.InjectConstants.u_xHeightParams.x, g_xPassSet.InjectConstants.u_xHeightParams.y);
	if (worldPos.y < g_xPassSet.InjectConstants.u_xHeightParams.z ||
		worldPos.y > g_xPassSet.InjectConstants.u_xHeightParams.w) heightDensity = 0.0;

	// Sample 3D noise. Y is animated over time so the fog billows
	// vertically; world-space coordinate scale gives consistent feature
	// size regardless of camera position.
	float3 noiseCoord = worldPos * g_xPassSet.InjectConstants.u_xNoiseParams.x;
	noiseCoord.y    += g_xPassSet.InjectConstants.u_xFogParams.w * g_xPassSet.InjectConstants.u_xNoiseParams.y;
	float  noise      = g_xPassSet.u_xNoiseTexture3D.SampleLevel(noiseCoord, 0).r;

	float scaledBaseDensity = UIToPhysicalDensity(baseDensity);
	float finalDensity      = scaledBaseDensity * heightDensity * noise;

	float4 densityOutput = float4(
		finalDensity,
		g_xPassSet.InjectConstants.u_xFogParams.y * finalDensity, // scattering
		g_xPassSet.InjectConstants.u_xFogParams.z * finalDensity, // absorption
		1.0);

	g_xPassSet.u_xDensityGrid[froxelCoord] = densityOutput;
}
