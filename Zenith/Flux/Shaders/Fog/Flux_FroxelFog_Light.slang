// =====================================================================
// Flux_FroxelFog_Light — Slang module.
//
// Subsystem: Fog. Computes per-froxel directional-light scattering for
// the froxel-based volumetric fog technique. Reads the density volume
// produced by FroxelFog_Inject, runs Henyey-Greenstein on the sun
// direction, samples cascaded shadow maps for volumetric occlusion,
// adds a sky-ambient term, and writes both lighting + scattering
// volumes for the Apply pass to consume.
//
// Imports Common.Frame for FrameConstants (matrices, sun, near/far)
// and Common.Volumetric (HenyeyGreenstein, FroxelToWorld,
// FOG_DENSITY_SKIP_THRESHOLD).
// =====================================================================

import Common.Frame;
import Common.Volumetric;

struct LightConstantsLayout
{
	float4 u_xFogColour;
	float4 u_xLightDirection;
	float4 u_xLightColour;       // RGB = colour, A = intensity
	float4 u_xGridDimensions;
	float  u_fScatteringCoeff;
	float  u_fAbsorptionCoeff;
	float  u_fPhaseG;
	uint   u_uDebugMode;
	// Volumetric shadow runtime params (legacy hardcoded: bias=0.001, cone=0.002).
	float  u_fVolShadowBias;
	float  u_fVolShadowConeRadius;
	// Sky-vs-direct ratio: 0.15-0.25 clear, 0.4-0.6 overcast.
	float  u_fAmbientIrradianceRatio;
	float  _pad0;
};
[[vk::binding(1, 0)]] ConstantBuffer<LightConstantsLayout> LightConstants;
[[vk::binding(2, 0)]] Sampler3D u_xDensityGrid;
[[vk::binding(3, 0)]] RWTexture3D<float4> u_xLightingGrid;
[[vk::binding(4, 0)]] RWTexture3D<float4> u_xScatteringGrid;

// Cascaded shadow maps (matches deferred shading binding pattern).
[[vk::binding(5, 0)]] Sampler2D u_xCSM0;
[[vk::binding(6, 0)]] Sampler2D u_xCSM1;
[[vk::binding(7, 0)]] Sampler2D u_xCSM2;
[[vk::binding(8, 0)]] Sampler2D u_xCSM3;

struct ShadowMatrixLayout { float4x4 u_xShadowMat; };
[[vk::binding( 9, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix0;
[[vk::binding(10, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix1;
[[vk::binding(11, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix2;
[[vk::binding(12, 0)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix3;

static const uint DEBUG_FROXEL_SCATTERING     = 6;
static const uint DEBUG_FROXEL_EXTINCTION     = 7;
static const uint DEBUG_FROXEL_SHADOW_SAMPLES = 8;

// Volumetric shadow constants. Sample count stays a constant for loop
// unrolling; bias / cone radius are runtime via LightConstants.
static const int VOL_SHADOW_SAMPLES = 4;

// Soft-cone shadow map sample. Walks cascades 0..3 picking the first
// whose UV+depth both lie inside [0,1]; returns 1 (lit) when no cascade
// covers the world position (typically far from camera).
float SampleVolumetricShadow(float3 worldPos)
{
	float4x4 axShadowMats[4] = {
		ShadowMatrix0.u_xShadowMat,
		ShadowMatrix1.u_xShadowMat,
		ShadowMatrix2.u_xShadowMat,
		ShadowMatrix3.u_xShadowMat,
	};

	// Fixed 4-tap rotated cone offsets — deterministic, no temporal noise.
	const float2 axOffsets[4] = {
		float2(-0.7071, -0.7071),
		float2( 0.7071, -0.7071),
		float2(-0.7071,  0.7071),
		float2( 0.7071,  0.7071),
	};

	for (int iCascade = 0; iCascade < 4; iCascade++)
	{
		float4 xShadowSpace = mul(axShadowMats[iCascade], float4(worldPos, 1.0));
		float2 xSamplePos   = xShadowSpace.xy / xShadowSpace.w * 0.5 + 0.5;
		float  fCurrentDepth = xShadowSpace.z / xShadowSpace.w;

		if (xSamplePos.x < 0.0 || xSamplePos.x > 1.0 ||
			xSamplePos.y < 0.0 || xSamplePos.y > 1.0 ||
			fCurrentDepth < 0.0 || fCurrentDepth > 1.0) continue;

		float fBiasedDepth = fCurrentDepth - LightConstants.u_fVolShadowBias;
		float fShadow      = 0.0;

		for (int i = 0; i < VOL_SHADOW_SAMPLES; i++)
		{
			float2 xSampleUV = xSamplePos + axOffsets[i] * LightConstants.u_fVolShadowConeRadius;
			float  fShadowDepth;
			// Compute shaders have no implicit derivatives — using Sample()
			// would force Slang to declare ComputeDerivativeGroupQuadsKHR.
			// Explicit LOD 0 is what we want here anyway (CSMs are
			// non-mipped depth textures).
			if      (iCascade == 0) fShadowDepth = u_xCSM0.SampleLevel(xSampleUV, 0).r;
			else if (iCascade == 1) fShadowDepth = u_xCSM1.SampleLevel(xSampleUV, 0).r;
			else if (iCascade == 2) fShadowDepth = u_xCSM2.SampleLevel(xSampleUV, 0).r;
			else                    fShadowDepth = u_xCSM3.SampleLevel(xSampleUV, 0).r;
			fShadow += (fBiasedDepth <= fShadowDepth) ? 1.0 : 0.0;
		}
		return fShadow / (float)VOL_SHADOW_SAMPLES;
	}
	return 1.0; // outside all cascades → assume lit
}

[shader("compute")]
[numthreads(8, 8, 8)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	int3 froxelCoord = (int3)dispatchId;
	if (froxelCoord.x >= (int)LightConstants.u_xGridDimensions.x ||
		froxelCoord.y >= (int)LightConstants.u_xGridDimensions.y ||
		froxelCoord.z >= (int)LightConstants.u_xGridDimensions.z) return;

	float4 densityData     = u_xDensityGrid.Load(int4(froxelCoord, 0));
	float  density         = densityData.r;
	float  scatteringCoeff = densityData.g;
	float  absorptionCoeff = densityData.b;

	if (density < FOG_DENSITY_SKIP_THRESHOLD)
	{
		u_xLightingGrid[froxelCoord]   = float4(0.0, 0.0, 0.0, 0.0);
		u_xScatteringGrid[froxelCoord] = float4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	int3 gridSize = (int3)LightConstants.u_xGridDimensions.xyz;
	float3 worldPos = FroxelToWorld(froxelCoord, gridSize, FrameConstants.g_xInvViewProjMat,
									 FrameConstants.g_xCameraNearFar.x,
									 FrameConstants.g_xCameraNearFar.y);
	// FroxelToWorld returns a camera-relative position; lift it to world.
	worldPos += FrameConstants.g_xCamPos_Pad.xyz;

	// View direction towards camera (for the phase function dot).
	float2 uv     = (float2(froxelCoord.xy) + 0.5) / LightConstants.u_xGridDimensions.xy;
	float2 ndc    = uv * 2.0 - 1.0;
	float4 clipPos = float4(ndc, 0.5, 1.0);
	// Inverse projection from clip → view, then negate to get camera-facing.
	float4x4 invProj = FrameConstants.g_xInvProjMat;
	float4 viewPos = mul(invProj, clipPos);
	float3 viewDir = -normalize(viewPos.xyz);

	float3 lightDir = normalize(-FrameConstants.g_xSunDir_Pad.xyz);

	float cosTheta = dot(viewDir, lightDir);
	float phase    = HenyeyGreenstein(cosTheta, LightConstants.u_fPhaseG);

	float3 lightColour = FrameConstants.g_xSunColour.rgb * LightConstants.u_xLightColour.a;
	float  shadowTerm  = SampleVolumetricShadow(worldPos);

	// Single-scattering albedo normalisation — scattered light cannot
	// exceed extincted light.
	float sigmaT = scatteringCoeff + absorptionCoeff;
	float albedo = (sigmaT > 0.0001) ? (scatteringCoeff / sigmaT) : 0.0;

	float3 inScatter = lightColour * albedo * scatteringCoeff * phase * shadowTerm;
	float3 ambient   = LightConstants.u_xFogColour.rgb * albedo * scatteringCoeff *
					   LightConstants.u_fAmbientIrradianceRatio;
	float3 totalScatter = inScatter + ambient;
	float  extinction   = scatteringCoeff + absorptionCoeff;

	if (LightConstants.u_uDebugMode == DEBUG_FROXEL_SCATTERING)
	{
		float  scatterMag = length(totalScatter);
		float3 heatColour = lerp(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0), saturate(scatterMag * 2.0));
		u_xLightingGrid[froxelCoord]   = float4(heatColour, 1.0);
		u_xScatteringGrid[froxelCoord] = float4(totalScatter, extinction);
		return;
	}
	if (LightConstants.u_uDebugMode == DEBUG_FROXEL_EXTINCTION)
	{
		float transmittance = exp(-extinction);
		u_xLightingGrid[froxelCoord]   = float4(transmittance, transmittance, transmittance, 1.0);
		u_xScatteringGrid[froxelCoord] = float4(totalScatter, extinction);
		return;
	}
	if (LightConstants.u_uDebugMode == DEBUG_FROXEL_SHADOW_SAMPLES)
	{
		u_xLightingGrid[froxelCoord]   = float4(shadowTerm, shadowTerm, shadowTerm, 1.0);
		u_xScatteringGrid[froxelCoord] = float4(totalScatter, extinction);
		return;
	}

	u_xLightingGrid[froxelCoord]   = float4(totalScatter, 1.0);
	u_xScatteringGrid[froxelCoord] = float4(totalScatter, extinction);
}
