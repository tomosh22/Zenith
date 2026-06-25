// =====================================================================
// Flux_PrefilterEnvMap — Slang module.
//
// Subsystem: IBL. Pre-computes the GGX-prefiltered specular environment
// cubemap mip chain. One mip + face per dispatch (roughness fed in via
// PrefilterConstants.g_fRoughness, face via g_uFaceIndex).
//
// Smooth-roughness anti-aliasing: GGX becomes a delta function as
// roughness → 0, so for r < IBL_SMOOTH_BLEND_THRESHOLD we blend the
// importance-sampled result toward a direct mirror reflection.
// =====================================================================

#include "Common/Bindings.slang"
import Common.Fullscreen;
import Common.PBR;
import Common.Atmosphere;

struct PrefilterConstantsLayout
{
	float g_fRoughness;
	uint  g_uUseAtmosphere;
	float g_fSunIntensity;
	uint  g_uFaceIndex;
};
struct PassParams
{
	ConstantBuffer<PrefilterConstantsLayout> PrefilterConstants;
	SamplerCube g_xSkyboxCubemap;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

// 64 samples is enough — the higher mips blur out per-sample noise.
// Down from 1024 in early prototypes (~61M total → ~30M).
static const uint SAMPLE_COUNT = 64u;

float3 CubemapFaceDirection(float2 xUV, uint uFace)
{
	float2 xCoord = xUV * 2.0 - 1.0;
	float3 xDir;
	switch (uFace)
	{
		case 0u: xDir = float3( 1.0, -xCoord.y, -xCoord.x); break;
		case 1u: xDir = float3(-1.0, -xCoord.y,  xCoord.x); break;
		case 2u: xDir = float3( xCoord.x,  1.0,  xCoord.y); break;
		case 3u: xDir = float3( xCoord.x, -1.0, -xCoord.y); break;
		case 4u: xDir = float3( xCoord.x, -xCoord.y,  1.0); break;
		case 5u: xDir = float3(-xCoord.x, -xCoord.y, -1.0); break;
		default: xDir = float3(0.0, 1.0, 0.0); break;
	}
	return normalize(xDir);
}

float3 SampleEnvironment(float3 xDir)
{
	if (g_xPassSet.PrefilterConstants.g_uUseAtmosphere > 0u)
	{
		float3 xSunDir = normalize(-g_xGlobalSet.g_xGlobal.g_xSunDir_Pad.xyz);
		AtmosphereResult xResult = ComputeAtmosphereScattering(
			float3(0.0, EARTH_RADIUS + IBL_REFERENCE_HEIGHT_METERS, 0.0),
			normalize(xDir),
			xSunDir,
			g_xPassSet.PrefilterConstants.g_fSunIntensity);
		return xResult.xColor;
	}
	return g_xPassSet.g_xSkyboxCubemap.Sample(xDir).rgb;
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float3 xN = CubemapFaceDirection(input.uv, g_xPassSet.PrefilterConstants.g_uFaceIndex);
	float3 xR = xN;
	float3 xV = xR;

	// Mirror sample for the smooth-roughness blend below.
	float3 xMirrorColor = SampleEnvironment(xN);

	// GGX importance sampling needs a non-zero roughness — the distribution
	// degenerates to a delta as r → 0 and division blows up.
	float fClampedRoughness = max(g_xPassSet.PrefilterConstants.g_fRoughness, PBR_GGX_MIN_ROUGHNESS);

	float3 xPrefilteredColor = float3(0.0, 0.0, 0.0);
	float  fTotalWeight      = 0.0;

	for (uint i = 0u; i < SAMPLE_COUNT; i++)
	{
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		float3 H  = ImportanceSampleGGX(Xi, xN, fClampedRoughness);
		float3 L  = normalize(2.0 * dot(xV, H) * H - xV);

		float NdotL = max(dot(xN, L), 0.0);
		if (NdotL > 0.0)
		{
			xPrefilteredColor += SampleEnvironment(L) * NdotL;
			fTotalWeight      += NdotL;
		}
	}

	xPrefilteredColor /= max(fTotalWeight, 0.001);

	// Smooth-roughness blend — avoids the visual discontinuity that
	// importance sampling produces near a delta-function GGX lobe.
	if (g_xPassSet.PrefilterConstants.g_fRoughness < IBL_SMOOTH_BLEND_THRESHOLD)
	{
		float fBlendFactor = smoothstep(PBR_GGX_MIN_ROUGHNESS,
										IBL_SMOOTH_BLEND_THRESHOLD,
										g_xPassSet.PrefilterConstants.g_fRoughness);
		xPrefilteredColor = lerp(xMirrorColor, xPrefilteredColor, fBlendFactor);
	}

	return float4(xPrefilteredColor, 1.0);
}
