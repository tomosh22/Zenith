// =====================================================================
// Flux_TemporalFog_Resolve — Slang module.
//
// Subsystem: Fog. Blends current-frame froxel fog with the previous
// frame's history using jitter offsets, then neighbourhood-clamps the
// history to suppress ghosting. 8x8x8 workgroup, one thread per froxel.
//
// Standalone — no FrameConstants dependency. The C++ side binds slot 0
// to FrameConstants for shared layout consistency, but this shader
// doesn't reflect or read it.
// =====================================================================

import Common.Frame;  // exposes FrameConstants binding for layout parity

struct ResolveConstantsLayout
{
	float4 u_xGridDimensions;
	float4 u_xJitterOffset; // xy = current jitter, zw = previous jitter
	float  u_fBlendWeight;
	float  u_fNearZ;
	float  u_fFarZ;
	uint   u_uDebugMode;
	uint   u_uFrameIndex;
	float  u_fPad0;
	float  u_fPad1;
	float  u_fPad2;
};
[[vk::binding(1, 0)]] ConstantBuffer<ResolveConstantsLayout> ResolveConstants;
[[vk::binding(2, 0)]] Sampler3D u_xCurrentFog;
[[vk::binding(3, 0)]] Sampler3D u_xHistoryFog;
[[vk::binding(4, 0)]] RWTexture3D<float4> u_xOutputFog;
[[vk::binding(5, 0)]] RWTexture2D<float4> u_xDebugMotionVectors;

static const uint DEBUG_TEMPORAL_MOTION_VECTORS = 17;
static const uint DEBUG_TEMPORAL_HISTORY_WEIGHT = 18;
static const uint DEBUG_TEMPORAL_JITTER_OFFSET  = 19;
static const uint DEBUG_TEMPORAL_DISOCCLUSION   = 20;

// Neighbourhood clamping suppresses ghosting from stale history samples
// that fall outside the current frame's local value range. Six axis
// neighbours (instead of full 27) for cost; centre included.
float4 ClampToNeighborhood(float4 history, float3 uvw)
{
	float4 minVal = float4(1e10, 1e10, 1e10, 1e10);
	float4 maxVal = float4(-1e10, -1e10, -1e10, -1e10);

	float3 texelSize = 1.0 / ResolveConstants.u_xGridDimensions.xyz;

	for (int i = -1; i <= 1; i += 2)
	{
		// Compute shaders have no implicit derivatives — explicit LOD 0
		// avoids forcing Slang to declare ComputeDerivativeGroupQuadsKHR.
		float4 sampleX = u_xCurrentFog.SampleLevel(uvw + float3((float)i * texelSize.x, 0.0, 0.0), 0);
		float4 sampleY = u_xCurrentFog.SampleLevel(uvw + float3(0.0, (float)i * texelSize.y, 0.0), 0);
		float4 sampleZ = u_xCurrentFog.SampleLevel(uvw + float3(0.0, 0.0, (float)i * texelSize.z), 0);
		minVal = min(minVal, min(sampleX, min(sampleY, sampleZ)));
		maxVal = max(maxVal, max(sampleX, max(sampleY, sampleZ)));
	}

	float4 center = u_xCurrentFog.SampleLevel(uvw, 0);
	minVal = min(minVal, center);
	maxVal = max(maxVal, center);

	return clamp(history, minVal, maxVal);
}

[shader("compute")]
[numthreads(8, 8, 8)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	int3 froxelCoord = (int3)dispatchId;

	if (froxelCoord.x >= (int)ResolveConstants.u_xGridDimensions.x ||
		froxelCoord.y >= (int)ResolveConstants.u_xGridDimensions.y ||
		froxelCoord.z >= (int)ResolveConstants.u_xGridDimensions.z) return;

	float3 uvw           = (float3(froxelCoord) + 0.5) / ResolveConstants.u_xGridDimensions.xyz;
	float2 jitteredUV    = uvw.xy + ResolveConstants.u_xJitterOffset.xy / ResolveConstants.u_xGridDimensions.xy;
	float3 jitteredUVW   = float3(jitteredUV, uvw.z);
	float4 currentFog    = u_xCurrentFog.SampleLevel(jitteredUVW, 0);

	// Simplified reprojection: assumes static camera. Proper reprojection
	// would need previous-frame matrices in the constant buffer.
	float3 prevUVW       = uvw;
	float2 prevJitterUV  = prevUVW.xy + ResolveConstants.u_xJitterOffset.zw / ResolveConstants.u_xGridDimensions.xy;
	float3 prevJitterUVW = float3(prevJitterUV, prevUVW.z);
	float4 historyFog    = u_xHistoryFog.SampleLevel(prevJitterUVW, 0);

	historyFog = ClampToNeighborhood(historyFog, uvw);
	float blendWeight = ResolveConstants.u_fBlendWeight;

	if (ResolveConstants.u_uDebugMode == DEBUG_TEMPORAL_MOTION_VECTORS &&
		froxelCoord.z == (int)(ResolveConstants.u_xGridDimensions.z / 2))
	{
		u_xDebugMotionVectors[froxelCoord.xy] = float4(0.5, 0.5, 0.5, 1.0);
	}

	if (ResolveConstants.u_uDebugMode == DEBUG_TEMPORAL_HISTORY_WEIGHT)
	{
		u_xOutputFog[froxelCoord] = float4(blendWeight, blendWeight, blendWeight, 1.0);
		return;
	}
	if (ResolveConstants.u_uDebugMode == DEBUG_TEMPORAL_JITTER_OFFSET)
	{
		float2 jitter = ResolveConstants.u_xJitterOffset.xy;
		u_xOutputFog[froxelCoord] = float4(jitter.x + 0.5, jitter.y + 0.5, 0.0, 1.0);
		return;
	}
	if (ResolveConstants.u_uDebugMode == DEBUG_TEMPORAL_DISOCCLUSION)
	{
		u_xOutputFog[froxelCoord] = float4(0.0, 1.0, 0.0, 1.0);
		return;
	}

	u_xOutputFog[froxelCoord] = lerp(currentFog, historyFog, blendWeight);
}
