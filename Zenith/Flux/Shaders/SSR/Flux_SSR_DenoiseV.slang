// =====================================================================
// Flux_SSR_DenoiseV — Slang module.
//
// Subsystem: SSR. Vertical half of the separable bilateral denoise.
// Pairs with Flux_SSR_DenoiseH.
//
// Phase 3b: reads H's unnormalized accumulator pair and continues the
// accumulation along the V axis with its own bilateral × BRDF weights,
// then applies the final ratio at the end. This avoids the squared-
// confidence bias of running ratio-of-ratios across H and V.
//
// Final output is single-MRT RGBA — rgb = colour ratio,
// a = filtered confidence — same shape the deferred shader has always
// consumed. Roughness gating repeats here independently (mirror-smooth
// and very-rough pixels passthrough whichever H output shape they have).
//
// View matrices / screen dims and the GetViewPosFromDepth helper come from the
// Bindings spine (replaces the former `import Common.Frame`).
// =====================================================================

#include "Common/Bindings.slang"
import Common.Fullscreen;
import Common.PBR;
import SSR.Flux_SSR_DenoiseCommon;

static const float SSR_DENOISE_SMOOTH_THRESHOLD    = 0.15;
static const float SSR_DENOISE_ROUGH_THRESHOLD     = 0.7;
static const float SSR_DENOISE_MIN_CONFIDENCE      = 0.001;
static const float SSR_DENOISE_MIN_DEPTH_SIGMA     = 0.0001;
static const float SSR_DENOISE_MIN_ROUGHNESS_SIGMA = 0.001;
static const float SSR_DENOISE_MIN_WEIGHT          = 1e-6;
static const float SSR_DENOISE_BRDF_FLOOR          = 1e-4;

struct PassParams
{
	Sampler2D g_xSSRDenoiseHColTex; // H pass RT0: rgb=Σ(w·color), a=Σ(w)
	ConstantBuffer<SSRDenoisePushConstantsLayout> PushConstants;
	Sampler2D g_xDepthTex;
	Sampler2D g_xNormalsTex;
	Sampler2D g_xMaterialTex;
	Sampler2D g_xSSRUpsampledAuxTex; // Phase 0 metadata for BRDF reuse
	Sampler2D g_xSSRDenoiseHConfTex; // H pass RT1: r=Σ(w·conf)
	Sampler2D g_xSSRUpsampledTex;    // upsampled colour, for passthrough fallback
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

float GaussianWeight(float fDist, float fSigma)
{
	float fNorm = fDist / fSigma;
	return exp(-0.5 * fNorm * fNorm);
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float4 xCenterUpsampled = g_xPassSet.g_xSSRUpsampledTex.Sample(input.uv);

	if (g_xPassSet.PushConstants.u_bEnabled == 0u) return xCenterUpsampled;

	float fCenterRoughness = g_xPassSet.g_xMaterialTex.Sample(input.uv).r;
	if (fCenterRoughness < SSR_DENOISE_SMOOTH_THRESHOLD) return xCenterUpsampled;
	if (fCenterRoughness > SSR_DENOISE_ROUGH_THRESHOLD)  return xCenterUpsampled;
	if (xCenterUpsampled.a < SSR_DENOISE_MIN_CONFIDENCE) return xCenterUpsampled;

	float  fCenterDepth   = g_xPassSet.g_xDepthTex.Sample(input.uv).r;
	float3 xCenterWNormal = g_xPassSet.g_xNormalsTex.Sample(input.uv).rgb;

	float3 xCenterViewPos    = GetViewPosFromDepth(input.uv, fCenterDepth);
	float3 xCenterViewNormal = normalize(mul((float3x3)g_xViewSet.g_xView.g_xViewMat, xCenterWNormal));
	float3 xCenterV          = -normalize(xCenterViewPos);

	float fDepthSigmaAbs    = max(fCenterDepth * g_xPassSet.PushConstants.u_fDepthSigma, SSR_DENOISE_MIN_DEPTH_SIGMA);
	float fRoughnessSigma   = max(g_xPassSet.PushConstants.u_fRoughnessSigma, SSR_DENOISE_MIN_ROUGHNESS_SIGMA);
	float2 xRcpScreen       = g_xViewSet.g_xView.g_xRcpScreenDims;

	float3 xTotalColourNum = float3(0.0);
	float  fTotalWeightDen = 0.0;
	float  fTotalConfNum   = 0.0;

	// Phase 4 — variance-adaptive kernel. Uses the upsampled (pre-denoise)
	// colour + distance — same input H sees, so the two passes' radii agree.
	float fVarScale = SSR_ComputeVarianceScale(g_xPassSet.g_xSSRUpsampledTex, g_xPassSet.g_xSSRUpsampledAuxTex, input.uv, xRcpScreen);
	int   iRadius   = (int)max(1.0, round((float)g_xPassSet.PushConstants.u_uKernelRadius * fVarScale));

	// Vertical pass — sample only along Y.
	for (int j = -iRadius; j <= iRadius; j++)
	{
		float2 xOffset   = float2(0.0, (float)j) * xRcpScreen;
		float2 xSampleUV = clamp(input.uv + xOffset, float2(0.001), float2(0.999));

		float4 xH_RT0 = g_xPassSet.g_xSSRDenoiseHColTex.Sample(xSampleUV);   // (Σ_H w·C, Σ_H w)
		float4 xH_RT1 = g_xPassSet.g_xSSRDenoiseHConfTex.Sample(xSampleUV);  // (Σ_H w·conf, _, _, _)

		// If H produced no weight at this pixel (its accumulator was empty),
		// skip — there's nothing meaningful to combine.
		if (xH_RT0.a < SSR_DENOISE_MIN_WEIGHT) continue;

		float4 xSampleAux       = g_xPassSet.g_xSSRUpsampledAuxTex.Sample(xSampleUV);
		float  fSampleDepth     = g_xPassSet.g_xDepthTex.Sample(xSampleUV).r;
		float3 xSampleWNormal   = g_xPassSet.g_xNormalsTex.Sample(xSampleUV).rgb;
		float  fSampleRoughness = g_xPassSet.g_xMaterialTex.Sample(xSampleUV).r;

		if (fSampleDepth >= 1.0) continue;
		if (xSampleAux.b   <  0.0001) continue;

		// Same V-direction bilateral gates as H.
		float fSpatialWeight = GaussianWeight(abs((float)j), g_xPassSet.PushConstants.u_fSpatialSigma);

		float fDepthDiff   = abs(fSampleDepth - fCenterDepth);
		float fDepthWeight = GaussianWeight(fDepthDiff, fDepthSigmaAbs);

		float fNormalDiff   = 1.0 - max(dot(xCenterWNormal, xSampleWNormal), 0.0);
		float fNormalWeight = GaussianWeight(fNormalDiff, g_xPassSet.PushConstants.u_fNormalSigma);

		float fRoughnessDiff   = abs(fSampleRoughness - fCenterRoughness);
		float fRoughnessWeight = GaussianWeight(fRoughnessDiff, fRoughnessSigma);

		// BRDF reuse — same logic as H, computed against THIS V-neighbour's hit.
		float2 xN_hitUV    = xSampleAux.rg;
		float  fHitDepth   = g_xPassSet.g_xDepthTex.Sample(xN_hitUV).r;
		float  fBRDFWeight = SSR_DENOISE_BRDF_FLOOR;
		if (fHitDepth < 1.0)
		{
			float3 xHitViewPos = GetViewPosFromDepth(xN_hitUV, fHitDepth);
			float3 xL          = normalize(xHitViewPos - xCenterViewPos);
			float3 xH          = normalize(xCenterV + xL);
			float  fNdotH      = max(dot(xCenterViewNormal, xH), 0.0);
			fBRDFWeight        = max(DistributionGGX(fNdotH, fCenterRoughness), SSR_DENOISE_BRDF_FLOOR);
		}

		float fW_V = fSpatialWeight * fDepthWeight * fNormalWeight * fRoughnessWeight * fBRDFWeight;

		// Compose: V's weight scales H's accumulators. Numerator/denominator
		// are tracked separately so the final ratio cleanly normalizes —
		// this is the proper joint estimator across both passes, not a
		// ratio-of-ratios.
		xTotalColourNum += fW_V * xH_RT0.rgb;
		fTotalWeightDen += fW_V * xH_RT0.a;
		fTotalConfNum   += fW_V * xH_RT1.r;
	}

	if (fTotalWeightDen > SSR_DENOISE_MIN_WEIGHT)
	{
		float fInv = 1.0 / fTotalWeightDen;
		return float4(xTotalColourNum * fInv, fTotalConfNum * fInv);
	}
	return xCenterUpsampled; // fail-safe
}
