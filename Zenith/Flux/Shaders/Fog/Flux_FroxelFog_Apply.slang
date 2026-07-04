// =====================================================================
// Flux_FroxelFog_Apply — Slang module.
//
// Subsystem: Fog. Final compositing pass for the froxel-based volumetric
// fog technique. Reads the precomputed lighting + scattering 3D textures
// (built by FroxelFog_Inject + FroxelFog_Light) and accumulates fog
// along each view ray with Beer-Lambert transmittance.
//
// Output: RGB = accumulated in-scatter, A = fog alpha (1 - transmittance).
// Imports Common.Frame for FrameConstants.g_xInvProjMat and
// Common.Volumetric for SAFE_NEAR_Z / SAFE_DEPTH_RATIO / early-exit
// transmittance.
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Fullscreen;
import Common.Volumetric;

struct ApplyConstantsLayout
{
	float4 u_xGridDimensions;
	float  u_fNearZ;
	float  u_fFarZ;
	uint   u_uDebugMode;
	uint   u_uDebugSliceIndex;
};
// PASS frequency — declared AFTER the spine #include so it lands at set 3.
struct PassParams
{
	ConstantBuffer<ApplyConstantsLayout> ApplyConstants;
	Sampler2D u_xDepthTexture;
	Sampler3D u_xLightingGrid;
	Sampler3D u_xScatteringGrid;
};
ParameterBlock<PassParams> g_xPassSet;

static const uint DEBUG_FROXEL_DENSITY_SLICE  = 3;
static const uint DEBUG_FROXEL_DENSITY_MAX    = 4;
static const uint DEBUG_FROXEL_LIGHTING_SLICE = 5;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

float DepthToFroxelZ(float linearDepth, float nearZ, float farZ)
{
	nearZ       = max(nearZ, SAFE_NEAR_Z);
	farZ        = max(farZ, nearZ * SAFE_DEPTH_RATIO);
	linearDepth = max(linearDepth, SAFE_NEAR_Z);
	return saturate(log(linearDepth / nearZ) / log(farZ / nearZ));
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	if (g_xPassSet.ApplyConstants.u_uDebugMode == DEBUG_FROXEL_DENSITY_SLICE)
	{
		float sliceZ = ((float)g_xPassSet.ApplyConstants.u_uDebugSliceIndex + 0.5) / g_xPassSet.ApplyConstants.u_xGridDimensions.z;
		float4 scatterData = g_xPassSet.u_xScatteringGrid.Sample(float3(input.uv, sliceZ));
		float v = saturate(scatterData.a);
		return float4(v, v, v, 1.0);
	}

	if (g_xPassSet.ApplyConstants.u_uDebugMode == DEBUG_FROXEL_DENSITY_MAX)
	{
		float maxExtinction = 0.0;
		int   sliceCount    = (int)g_xPassSet.ApplyConstants.u_xGridDimensions.z;
		for (int z = 0; z < sliceCount; z++)
		{
			float sliceZ = ((float)z + 0.5) / g_xPassSet.ApplyConstants.u_xGridDimensions.z;
			float4 scatterData = g_xPassSet.u_xScatteringGrid.Sample(float3(input.uv, sliceZ));
			maxExtinction = max(maxExtinction, scatterData.a);
		}
		float v = saturate(maxExtinction);
		return float4(v, v, v, 1.0);
	}

	if (g_xPassSet.ApplyConstants.u_uDebugMode == DEBUG_FROXEL_LIGHTING_SLICE)
	{
		float sliceZ = ((float)g_xPassSet.ApplyConstants.u_uDebugSliceIndex + 0.5) / g_xPassSet.ApplyConstants.u_xGridDimensions.z;
		float3 lighting = g_xPassSet.u_xLightingGrid.Sample(float3(input.uv, sliceZ)).rgb;
		return float4(lighting, 1.0);
	}

	float depth = g_xPassSet.u_xDepthTexture.Sample(input.uv).r;

	// Reconstruct linear depth in view space. Engine uses +Z forward, so
	// viewPos.z is positive — no negation needed.
	float4 clipPos = float4(input.uv * 2.0 - 1.0, depth, 1.0);
	float4 viewPos = mul(GetInvProjMat(), clipPos);
	viewPos /= viewPos.w;
	float linearDepth = viewPos.z;

	float nearZ = max(g_xPassSet.ApplyConstants.u_fNearZ, SAFE_NEAR_Z);
	float farZ  = max(g_xPassSet.ApplyConstants.u_fFarZ, nearZ * SAFE_DEPTH_RATIO);
	linearDepth = clamp(linearDepth, nearZ, farZ);

	// 32 samples = ~2× oversampling of the 64-slice froxel grid with
	// trilinear filtering. Tuning: 16 mobile / 32 balanced / 64 high.
	const int NUM_SAMPLES = 32;

	float3 accumulatedLight        = float3(0.0, 0.0, 0.0);
	float  accumulatedTransmittance = 1.0;

	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		float t           = (float)(i + 1) / (float)NUM_SAMPLES;
		float sampleDepth = nearZ * pow(linearDepth / nearZ, t);
		if (sampleDepth > linearDepth) break;

		float froxelZ = DepthToFroxelZ(sampleDepth, nearZ, farZ);

		float3 inScatter   = g_xPassSet.u_xLightingGrid.Sample(float3(input.uv, froxelZ)).rgb;
		float4 scatterData = g_xPassSet.u_xScatteringGrid.Sample(float3(input.uv, froxelZ));
		float  extinction  = scatterData.a;

		float prevDepth = (i == 0)
			? nearZ
			: nearZ * pow(linearDepth / nearZ, (float)i / (float)NUM_SAMPLES);
		float stepSize = sampleDepth - prevDepth;

		float stepTransmittance = exp(-extinction * stepSize);
		accumulatedLight       += inScatter * accumulatedTransmittance * (1.0 - stepTransmittance);
		accumulatedTransmittance *= stepTransmittance;

		if (accumulatedTransmittance < FOG_TRANSMITTANCE_EARLY_EXIT) break;
	}

	float fogAlpha = 1.0 - accumulatedTransmittance;
	return float4(accumulatedLight, fogAlpha);
}
