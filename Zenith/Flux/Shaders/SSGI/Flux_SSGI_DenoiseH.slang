// =====================================================================
// Flux_SSGI_DenoiseH — Slang module.
//
// Subsystem: SSGI. Horizontal half of the separable joint-bilateral
// denoise. Pairs with Flux_SSGI_DenoiseV — DenoiseH reads the upsampled
// SSGI and writes a horizontal-blurred intermediate; DenoiseV reads
// that intermediate and writes the final denoised output.
//
// Edges are preserved by combining four weights:
//   * spatial (Gaussian over pixel distance along the axis)
//   * depth   (Gaussian over depth-relative difference)
//   * normal  (Gaussian over 1 - dot(n_p, n_q))
//   * albedo  (Gaussian over RGB distance — material boundary preservation)
//
// Strict bilateral non-separability is mathematically present but
// visually negligible at this kernel size; FidelityFX CACAO and most
// other engines do exactly this.
//
// Slot layout matches the previous 2D denoise:
//   binding 0: g_xPassSet.g_xSSGITex (input)
//   binding 1: g_xPassSet.PushConstants (shared layout — see DenoisePushConstantsLayout)
//   binding 2: g_xPassSet.g_xDepthTex
//   binding 3: g_xPassSet.g_xNormalsTex
//   binding 4: g_xPassSet.g_xAlbedoTex
// =====================================================================

import Common.Fullscreen;
import Common.GBuffer;   // DecodeNormal (shared with DenoiseV — no bindings, does not shift spine spaces)
// Spine #include even though this shader uses no FrameConstants — it reserves
// sets 0/1/2 so PassParams lands at set 3.
#include "Common/Bindings.slang"

static const float DENOISE_MIN_WEIGHT      = 0.0001;
static const float DENOISE_SKY_THRESHOLD   = 0.9999;
static const float DENOISE_MIN_DEPTH_SIGMA = 0.0001;

struct DenoisePushConstantsLayout
{
	float u_fSpatialSigma;
	float u_fDepthSigma;
	float u_fNormalSigma;
	float u_fAlbedoSigma;
	uint  u_uKernelRadius;
	uint  u_bEnabled;
	float _pad0;
	float _pad1;
};

// PASS frequency (set 3) — the shader's own CB + SRVs. Member names preserved exactly.
struct PassParams
{
	Sampler2D g_xSSGITex;     // upsampled SSGI input
	ConstantBuffer<DenoisePushConstantsLayout> PushConstants;
	Sampler2D g_xDepthTex;
	Sampler2D g_xNormalsTex;
	Sampler2D g_xAlbedoTex;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

float GaussianWeight(float fDist, float fSigma)
{
	float fNorm = fDist / fSigma;
	return exp(-0.5 * fNorm * fNorm);
}

// DecodeNormal moved to Common.GBuffer (shared with DenoiseV).

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	if (g_xPassSet.PushConstants.u_bEnabled == 0u) return g_xPassSet.g_xSSGITex.Sample(input.uv);

	uint uW, uH;
	g_xPassSet.g_xSSGITex.GetDimensions(uW, uH);
	float2 xTexelSize = 1.0 / float2((float)uW, (float)uH);

	float4 xCenterSSGI    = g_xPassSet.g_xSSGITex.Sample(input.uv);
	float  fCenterDepth   = g_xPassSet.g_xDepthTex.Sample(input.uv).r;
	float3 xCenterNormal  = DecodeNormal(g_xPassSet.g_xNormalsTex.Sample(input.uv).rgb);
	float3 xCenterAlbedo  = g_xPassSet.g_xAlbedoTex.Sample(input.uv).rgb;

	if (fCenterDepth >= DENOISE_SKY_THRESHOLD) return xCenterSSGI;

	float fDepthSigmaAbs = max(fCenterDepth * g_xPassSet.PushConstants.u_fDepthSigma, DENOISE_MIN_DEPTH_SIGMA);

	float3 xAccumColour      = float3(0.0);
	float  fAccumConfidence  = 0.0;
	float  fAccumWeight      = 0.0;
	int    iRadius           = (int)g_xPassSet.PushConstants.u_uKernelRadius;

	// Horizontal pass — sample only along X.
	for (int i = -iRadius; i <= iRadius; i++)
	{
		float2 xOffset   = float2((float)i, 0.0);
		float2 xSampleUV = clamp(input.uv + xOffset * xTexelSize, float2(0.001), float2(0.999));

		float4 xSampleSSGI    = g_xPassSet.g_xSSGITex.Sample(xSampleUV);
		float  fSampleDepth   = g_xPassSet.g_xDepthTex.Sample(xSampleUV).r;
		float3 xSampleNormal  = DecodeNormal(g_xPassSet.g_xNormalsTex.Sample(xSampleUV).rgb);
		float3 xSampleAlbedo  = g_xPassSet.g_xAlbedoTex.Sample(xSampleUV).rgb;

		if (fSampleDepth >= DENOISE_SKY_THRESHOLD) continue;

		float fSpatialDist   = abs((float)i);
		float fSpatialWeight = GaussianWeight(fSpatialDist, g_xPassSet.PushConstants.u_fSpatialSigma);

		float fDepthDiff   = abs(fSampleDepth - fCenterDepth);
		float fDepthWeight = GaussianWeight(fDepthDiff, fDepthSigmaAbs);

		float fNormalDot   = max(dot(xCenterNormal, xSampleNormal), 0.0);
		float fNormalDiff  = 1.0 - fNormalDot;
		float fNormalWeight = GaussianWeight(fNormalDiff, g_xPassSet.PushConstants.u_fNormalSigma);

		float fAlbedoDiff   = length(xSampleAlbedo - xCenterAlbedo);
		float fAlbedoWeight = GaussianWeight(fAlbedoDiff, g_xPassSet.PushConstants.u_fAlbedoSigma);

		// Multiplicative combination — any single mismatch suppresses the
		// sample's contribution.
		float fWeight = fSpatialWeight * fDepthWeight * fNormalWeight * fAlbedoWeight;

		xAccumColour     += xSampleSSGI.rgb * fWeight;
		fAccumConfidence += xSampleSSGI.a   * fWeight;
		fAccumWeight     += fWeight;
	}

	if (fAccumWeight > DENOISE_MIN_WEIGHT)
	{
		xAccumColour     /= fAccumWeight;
		fAccumConfidence /= fAccumWeight;
		return float4(xAccumColour, fAccumConfidence);
	}
	return xCenterSSGI; // fail-safe — shouldn't trigger in practice
}
