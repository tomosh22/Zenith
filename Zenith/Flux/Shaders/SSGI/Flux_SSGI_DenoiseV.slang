// =====================================================================
// Flux_SSGI_DenoiseV — Slang module.
//
// Subsystem: SSGI. Vertical half of the separable joint-bilateral
// denoise. Pairs with Flux_SSGI_DenoiseH — DenoiseV reads the
// horizontal-blurred intermediate and writes the final denoised
// output that deferred shading consumes.
//
// Identical to DenoiseH except the inner loop samples along Y instead
// of X. See DenoiseH for full pass documentation.
//
// Slot layout matches DenoiseH and the previous 2D denoise:
//   binding 0: g_xSSGITex (input — H intermediate when running as V)
//   binding 1: PushConstants (shared layout — see DenoisePushConstantsLayout)
//   binding 2: g_xDepthTex
//   binding 3: g_xNormalsTex
//   binding 4: g_xAlbedoTex
// =====================================================================

import Common.Fullscreen;

static const float DENOISE_MIN_WEIGHT      = 0.0001;
static const float DENOISE_SKY_THRESHOLD   = 0.9999;
static const float DENOISE_MIN_DEPTH_SIGMA = 0.0001;

[[vk::binding(0, 0)]] Sampler2D g_xSSGITex;     // H-blurred intermediate

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
[[vk::binding(1, 0)]] ConstantBuffer<DenoisePushConstantsLayout> PushConstants;

[[vk::binding(2, 0)]] Sampler2D g_xDepthTex;
[[vk::binding(3, 0)]] Sampler2D g_xNormalsTex;
[[vk::binding(4, 0)]] Sampler2D g_xAlbedoTex;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

float GaussianWeight(float fDist, float fSigma)
{
	float fNorm = fDist / fSigma;
	return exp(-0.5 * fNorm * fNorm);
}

float3 DecodeNormal(float3 xRaw) { return normalize(xRaw); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	if (PushConstants.u_bEnabled == 0u) return g_xSSGITex.Sample(input.uv);

	uint uW, uH;
	g_xSSGITex.GetDimensions(uW, uH);
	float2 xTexelSize = 1.0 / float2((float)uW, (float)uH);

	float4 xCenterSSGI    = g_xSSGITex.Sample(input.uv);
	float  fCenterDepth   = g_xDepthTex.Sample(input.uv).r;
	float3 xCenterNormal  = DecodeNormal(g_xNormalsTex.Sample(input.uv).rgb);
	float3 xCenterAlbedo  = g_xAlbedoTex.Sample(input.uv).rgb;

	if (fCenterDepth >= DENOISE_SKY_THRESHOLD) return xCenterSSGI;

	float fDepthSigmaAbs = max(fCenterDepth * PushConstants.u_fDepthSigma, DENOISE_MIN_DEPTH_SIGMA);

	float3 xAccumColour      = float3(0.0);
	float  fAccumConfidence  = 0.0;
	float  fAccumWeight      = 0.0;
	int    iRadius           = (int)PushConstants.u_uKernelRadius;

	// Vertical pass — sample only along Y.
	for (int i = -iRadius; i <= iRadius; i++)
	{
		float2 xOffset   = float2(0.0, (float)i);
		float2 xSampleUV = clamp(input.uv + xOffset * xTexelSize, float2(0.001), float2(0.999));

		float4 xSampleSSGI    = g_xSSGITex.Sample(xSampleUV);
		float  fSampleDepth   = g_xDepthTex.Sample(xSampleUV).r;
		float3 xSampleNormal  = DecodeNormal(g_xNormalsTex.Sample(xSampleUV).rgb);
		float3 xSampleAlbedo  = g_xAlbedoTex.Sample(xSampleUV).rgb;

		if (fSampleDepth >= DENOISE_SKY_THRESHOLD) continue;

		float fSpatialDist   = abs((float)i);
		float fSpatialWeight = GaussianWeight(fSpatialDist, PushConstants.u_fSpatialSigma);

		float fDepthDiff   = abs(fSampleDepth - fCenterDepth);
		float fDepthWeight = GaussianWeight(fDepthDiff, fDepthSigmaAbs);

		float fNormalDot   = max(dot(xCenterNormal, xSampleNormal), 0.0);
		float fNormalDiff  = 1.0 - fNormalDot;
		float fNormalWeight = GaussianWeight(fNormalDiff, PushConstants.u_fNormalSigma);

		float fAlbedoDiff   = length(xSampleAlbedo - xCenterAlbedo);
		float fAlbedoWeight = GaussianWeight(fAlbedoDiff, PushConstants.u_fAlbedoSigma);

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
