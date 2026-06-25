// =====================================================================
// Flux_SSR_DenoiseH — Slang module.
//
// Subsystem: SSR. Horizontal half of the separable bilateral denoise.
// Pairs with Flux_SSR_DenoiseV.
//
// Phase 3b: BRDF-weighted ratio estimator with spatial sample reuse.
//
// Per-neighbour weight = spatial × normal × depth × roughness ×
//                        confidence_n × D_GGX(centre_NdotH_n, centre_roughness)
//
// where the half-vector for the BRDF term comes from the geometry of the
// neighbour's hit (via the Phase 0 hit-UV metadata RT). This is true
// spatial sample reuse: each neighbour's information is reweighted by how
// well it matches the centre pixel's GGX lobe.
//
// H output is the unnormalized accumulator pair (so V can do the final
// ratio without the squared-confidence bias of ratio-of-ratios):
//   RT0 RGBA: rgb = Σ(w·radiance), a = Σ(w)
//   RT1 RGBA: r   = Σ(w·confidence_n), gba unused
//
// Roughness gating runs at the TOP of the shader (must repeat in V too —
// roughness < 0.15 → mirror passthrough, roughness > 0.7 → IBL passthrough).
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
static const float SSR_DENOISE_BRDF_FLOOR          = 1e-4;

struct PassParams
{
	Sampler2D g_xSSRUpsampledTex;     // colour input (RT0 from upsample)
	ConstantBuffer<SSRDenoisePushConstantsLayout> PushConstants;
	Sampler2D g_xDepthTex;
	Sampler2D g_xNormalsTex;
	Sampler2D g_xMaterialTex;
	Sampler2D g_xSSRUpsampledAuxTex;  // Phase 0 metadata (RT1 from upsample)
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

float GaussianWeight(float fDist, float fSigma)
{
	float fNorm = fDist / fSigma;
	return exp(-0.5 * fNorm * fNorm);
}

struct DenoiseHOut
{
	float4 xColourAccum : SV_Target0; // rgb = Σ(w·color), a = Σ(w)
	float4 xConfAccum   : SV_Target1; // r = Σ(w·confidence_n), gba unused
};

DenoiseHOut MakeDenoiseHOut(float4 xColourAccum, float4 xConfAccum)
{
	DenoiseHOut o;
	o.xColourAccum = xColourAccum;
	o.xConfAccum   = xConfAccum;
	return o;
}

// Passthrough output for mirror-smooth / very-rough / disabled paths. Sets up
// the dual-MRT shape so V can divide unconditionally — the centre sample's
// own weight is 1.
DenoiseHOut PassthroughOut(float4 xCentre)
{
	return MakeDenoiseHOut(float4(xCentre.rgb, 1.0), float4(xCentre.a, 0.0, 0.0, 0.0));
}

[shader("fragment")]
DenoiseHOut fsMain(FullscreenVsOut input)
{
	float4 xCenter = g_xPassSet.g_xSSRUpsampledTex.Sample(input.uv);

	if (g_xPassSet.PushConstants.u_bEnabled == 0u) return PassthroughOut(xCenter);

	float fCenterRoughness = g_xPassSet.g_xMaterialTex.Sample(input.uv).r;
	if (fCenterRoughness < SSR_DENOISE_SMOOTH_THRESHOLD) return PassthroughOut(xCenter);
	if (fCenterRoughness > SSR_DENOISE_ROUGH_THRESHOLD)  return PassthroughOut(xCenter);
	if (xCenter.a        < SSR_DENOISE_MIN_CONFIDENCE)   return PassthroughOut(xCenter);

	float  fCenterDepth   = g_xPassSet.g_xDepthTex.Sample(input.uv).r;
	float3 xCenterWNormal = g_xPassSet.g_xNormalsTex.Sample(input.uv).rgb;

	// View-space basis for the BRDF half-vector calculation. Camera is at the
	// origin in view space, so V (surface→camera) = -normalize(viewPos).
	float3 xCenterViewPos    = GetViewPosFromDepth(input.uv, fCenterDepth);
	float3 xCenterViewNormal = normalize(mul((float3x3)g_xViewSet.g_xView.g_xViewMat, xCenterWNormal));
	float3 xCenterV          = -normalize(xCenterViewPos);

	float fDepthSigmaAbs    = max(fCenterDepth * g_xPassSet.PushConstants.u_fDepthSigma, SSR_DENOISE_MIN_DEPTH_SIGMA);
	float fRoughnessSigma   = max(g_xPassSet.PushConstants.u_fRoughnessSigma, SSR_DENOISE_MIN_ROUGHNESS_SIGMA);
	float2 xRcpScreen       = g_xViewSet.g_xView.g_xRcpScreenDims;

	float3 xColourNum = float3(0.0);
	float  fWeightDen = 0.0;
	float  fConfNum   = 0.0;

	// Phase 4 — variance-adaptive kernel.
	float fVarScale = SSR_ComputeVarianceScale(g_xPassSet.g_xSSRUpsampledTex, g_xPassSet.g_xSSRUpsampledAuxTex, input.uv, xRcpScreen);
	int   iRadius   = (int)max(1.0, round((float)g_xPassSet.PushConstants.u_uKernelRadius * fVarScale));

	// Horizontal pass — sample only along X.
	for (int i = -iRadius; i <= iRadius; i++)
	{
		float2 xOffset   = float2((float)i, 0.0) * xRcpScreen;
		float2 xSampleUV = clamp(input.uv + xOffset, float2(0.001), float2(0.999));

		float4 xSampleSSR       = g_xPassSet.g_xSSRUpsampledTex.Sample(xSampleUV);
		float4 xSampleAux       = g_xPassSet.g_xSSRUpsampledAuxTex.Sample(xSampleUV);
		float  fSampleDepth     = g_xPassSet.g_xDepthTex.Sample(xSampleUV).r;
		float3 xSampleWNormal   = g_xPassSet.g_xNormalsTex.Sample(xSampleUV).rgb;
		float  fSampleRoughness = g_xPassSet.g_xMaterialTex.Sample(xSampleUV).r;

		if (fSampleDepth >= 1.0) continue;       // sky
		if (xSampleAux.b   <  0.0001) continue;  // neighbour had no hit (Phase 0 RT1.b carries travel distance)

		// --- Bilateral edge gates (centre vs neighbour content) ---
		float fSpatialWeight = GaussianWeight(abs((float)i), g_xPassSet.PushConstants.u_fSpatialSigma);

		float fDepthDiff   = abs(fSampleDepth - fCenterDepth);
		float fDepthWeight = GaussianWeight(fDepthDiff, fDepthSigmaAbs);

		float fNormalDiff   = 1.0 - max(dot(xCenterWNormal, xSampleWNormal), 0.0);
		float fNormalWeight = GaussianWeight(fNormalDiff, g_xPassSet.PushConstants.u_fNormalSigma);

		float fRoughnessDiff   = abs(fSampleRoughness - fCenterRoughness);
		float fRoughnessWeight = GaussianWeight(fRoughnessDiff, fRoughnessSigma);

		// --- BRDF reuse weight (centre's lobe vs neighbour's hit direction) ---
		// Use neighbour's hit UV (Phase 0 metadata) to derive a hit world point,
		// then evaluate D_GGX with the centre's normal and the half-vector
		// pointing from centre toward that hit. Larger D = closer to centre's
		// mirror lobe = more weight.
		float2 xN_hitUV     = xSampleAux.rg;
		float  fHitDepth    = g_xPassSet.g_xDepthTex.Sample(xN_hitUV).r;
		float  fBRDFWeight  = SSR_DENOISE_BRDF_FLOOR;
		if (fHitDepth < 1.0)
		{
			float3 xHitViewPos = GetViewPosFromDepth(xN_hitUV, fHitDepth);
			float3 xL          = normalize(xHitViewPos - xCenterViewPos);
			float3 xH          = normalize(xCenterV + xL);
			float  fNdotH      = max(dot(xCenterViewNormal, xH), 0.0);
			fBRDFWeight        = max(DistributionGGX(fNdotH, fCenterRoughness), SSR_DENOISE_BRDF_FLOOR);
		}

		float fSampleConf = xSampleSSR.a;
		float fWeight     = fSpatialWeight * fDepthWeight * fNormalWeight *
							fRoughnessWeight * fSampleConf * fBRDFWeight;

		xColourNum += xSampleSSR.rgb * fWeight;
		fWeightDen += fWeight;
		fConfNum   += fSampleConf    * fWeight;
	}

	// Output unnormalized accumulators — V applies the final ratio.
	return MakeDenoiseHOut(float4(xColourNum, fWeightDen),
						   float4(fConfNum,  0.0, 0.0, 0.0));
}
