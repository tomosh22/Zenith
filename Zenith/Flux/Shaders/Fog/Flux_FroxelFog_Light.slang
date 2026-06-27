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

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
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

// PASS frequency — declared AFTER the spine #include so it lands at set 3.
struct PassParams
{
	ConstantBuffer<LightConstantsLayout> LightConstants;
	Sampler3D u_xDensityGrid;
	RWTexture3D<float4> u_xLightingGrid;
	RWTexture3D<float4> u_xScatteringGrid;

	// (g_xShadowMatrices — all 4 cascade sun view×proj — is in the persistent VIEW set, Phase 5.4)
};
ParameterBlock<PassParams> g_xPassSet;

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
		g_xViewSet.g_xShadowMatrices[0],
		g_xViewSet.g_xShadowMatrices[1],
		g_xViewSet.g_xShadowMatrices[2],
		g_xViewSet.g_xShadowMatrices[3],
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

		float fBiasedDepth = fCurrentDepth - g_xPassSet.LightConstants.u_fVolShadowBias;
		float fShadow      = 0.0;

		for (int i = 0; i < VOL_SHADOW_SAMPLES; i++)
		{
			float2 xSampleUV = xSamplePos + axOffsets[i] * g_xPassSet.LightConstants.u_fVolShadowConeRadius;
			// Compute shaders have no implicit derivatives — using Sample()
			// would force Slang to declare ComputeDerivativeGroupQuadsKHR.
			// Explicit LOD 0 is what we want here anyway (CSMs are
			// non-mipped depth textures). Cascade selects the array layer.
			float fShadowDepth = g_xViewSet.g_xCSM.SampleLevel(float3(xSampleUV, float(iCascade)), 0).r;
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
	if (froxelCoord.x >= (int)g_xPassSet.LightConstants.u_xGridDimensions.x ||
		froxelCoord.y >= (int)g_xPassSet.LightConstants.u_xGridDimensions.y ||
		froxelCoord.z >= (int)g_xPassSet.LightConstants.u_xGridDimensions.z) return;

	float4 densityData     = g_xPassSet.u_xDensityGrid.Load(int4(froxelCoord, 0));
	float  density         = densityData.r;
	float  scatteringCoeff = densityData.g;
	float  absorptionCoeff = densityData.b;

	if (density < FOG_DENSITY_SKIP_THRESHOLD)
	{
		g_xPassSet.u_xLightingGrid[froxelCoord]   = float4(0.0, 0.0, 0.0, 0.0);
		g_xPassSet.u_xScatteringGrid[froxelCoord] = float4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	int3 gridSize = (int3)g_xPassSet.LightConstants.u_xGridDimensions.xyz;
	float3 worldPos = FroxelToWorld(froxelCoord, gridSize, g_xViewSet.g_xView.g_xInvViewProjMat,
									 g_xViewSet.g_xView.g_xCameraNearFar.x,
									 g_xViewSet.g_xView.g_xCameraNearFar.y);
	// FroxelToWorld returns a camera-relative position; lift it to world.
	worldPos += g_xViewSet.g_xView.g_xCamPos_Pad.xyz;

	// View direction towards camera (for the phase function dot).
	float2 uv     = (float2(froxelCoord.xy) + 0.5) / g_xPassSet.LightConstants.u_xGridDimensions.xy;
	float2 ndc    = uv * 2.0 - 1.0;
	float4 clipPos = float4(ndc, 0.5, 1.0);
	// Inverse projection from clip → view, then negate to get camera-facing.
	float4x4 invProj = g_xViewSet.g_xView.g_xInvProjMat;
	float4 viewPos = mul(invProj, clipPos);
	float3 viewDir = -normalize(viewPos.xyz);

	float3 lightDir = normalize(-g_xGlobalSet.g_xGlobal.g_xSunDir_Pad.xyz);

	float cosTheta = dot(viewDir, lightDir);
	float phase    = HenyeyGreenstein(cosTheta, g_xPassSet.LightConstants.u_fPhaseG);

	float3 lightColour = g_xGlobalSet.g_xGlobal.g_xSunColour_Pad.rgb * g_xPassSet.LightConstants.u_xLightColour.a;
	float  shadowTerm  = SampleVolumetricShadow(worldPos);

	// Single-scattering albedo normalisation — scattered light cannot
	// exceed extincted light.
	float sigmaT = scatteringCoeff + absorptionCoeff;
	float albedo = (sigmaT > 0.0001) ? (scatteringCoeff / sigmaT) : 0.0;

	float3 inScatter = lightColour * albedo * scatteringCoeff * phase * shadowTerm;
	float3 ambient   = g_xPassSet.LightConstants.u_xFogColour.rgb * albedo * scatteringCoeff *
					   g_xPassSet.LightConstants.u_fAmbientIrradianceRatio;
	float3 totalScatter = inScatter + ambient;
	float  extinction   = scatteringCoeff + absorptionCoeff;

	if (g_xPassSet.LightConstants.u_uDebugMode == DEBUG_FROXEL_SCATTERING)
	{
		float  scatterMag = length(totalScatter);
		float3 heatColour = lerp(float3(0.0, 0.0, 1.0), float3(1.0, 0.0, 0.0), saturate(scatterMag * 2.0));
		g_xPassSet.u_xLightingGrid[froxelCoord]   = float4(heatColour, 1.0);
		g_xPassSet.u_xScatteringGrid[froxelCoord] = float4(totalScatter, extinction);
		return;
	}
	if (g_xPassSet.LightConstants.u_uDebugMode == DEBUG_FROXEL_EXTINCTION)
	{
		float transmittance = exp(-extinction);
		g_xPassSet.u_xLightingGrid[froxelCoord]   = float4(transmittance, transmittance, transmittance, 1.0);
		g_xPassSet.u_xScatteringGrid[froxelCoord] = float4(totalScatter, extinction);
		return;
	}
	if (g_xPassSet.LightConstants.u_uDebugMode == DEBUG_FROXEL_SHADOW_SAMPLES)
	{
		g_xPassSet.u_xLightingGrid[froxelCoord]   = float4(shadowTerm, shadowTerm, shadowTerm, 1.0);
		g_xPassSet.u_xScatteringGrid[froxelCoord] = float4(totalScatter, extinction);
		return;
	}

	g_xPassSet.u_xLightingGrid[froxelCoord]   = float4(totalScatter, 1.0);
	g_xPassSet.u_xScatteringGrid[froxelCoord] = float4(totalScatter, extinction);
}
