// =====================================================================
// Flux_SSAO_Upsample — Slang module.
//
// Subsystem: SSAO. Bilateral 2x2 upsample from half-res blurred SSAO
// to full-res, weighted by depth so silhouettes don't bleed across
// edges. Output goes to the HDR scene target with multiplicative blend
// (driven by the alpha channel).
//
// Standalone — no shared module imports beyond the fullscreen vsMain.
// =====================================================================

import Common.Fullscreen;

// Bilateral upsample tuning:
//   * sigma is a fraction of local linear depth so weights scale with scene scale
//   * floors prevent zero-sigma blow-ups when depth is very small
//   * MIN_WEIGHT prevents 0/0 fallout when every neighbour is sky-rejected
static const float UPSAMPLE_DEPTH_SIGMA_FRACTION = 0.02;
static const float UPSAMPLE_MIN_DEPTH_SIGMA      = 0.001;
static const float UPSAMPLE_MIN_WEIGHT           = 0.0001;

[[vk::binding(0, 0)]] Sampler2D g_xOcclusionTex; // half-res blurred SSAO
[[vk::binding(1, 0)]] Sampler2D g_xDepthTex;     // full-res depth

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float fFullDepth = g_xDepthTex.Sample(input.uv).r;

	// Sky → fully unoccluded. Output 1.0 across all channels so the
	// downstream multiplicative blend leaves the HDR target untouched.
	if (fFullDepth >= 1.0) return float4(1.0, 1.0, 1.0, 1.0);

	uint uW, uH;
	g_xOcclusionTex.GetDimensions(uW, uH);
	float2 xHalfResSize   = float2((float)uW, (float)uH);
	float2 xHalfTexelSize = 1.0 / xHalfResSize;

	// Position the full-res UV inside the half-res grid so we can pick
	// the four surrounding texels and combine them with bilinear weights.
	float2 xHalfResPos   = input.uv * xHalfResSize - 0.5;
	float2 xHalfResFloor = floor(xHalfResPos);
	float2 xFrac         = xHalfResPos - xHalfResFloor;

	float aafBilinear[2][2];
	aafBilinear[0][0] = (1.0 - xFrac.x) * (1.0 - xFrac.y);
	aafBilinear[1][0] = xFrac.x * (1.0 - xFrac.y);
	aafBilinear[0][1] = (1.0 - xFrac.x) * xFrac.y;
	aafBilinear[1][1] = xFrac.x * xFrac.y;

	float fResult      = 0.0;
	float fTotalWeight = 0.0;

	for (int y = 0; y <= 1; y++)
	{
		for (int x = 0; x <= 1; x++)
		{
			float2 xSampleUV = saturate((xHalfResFloor + float2(x, y) + 0.5) * xHalfTexelSize);

			float fSampleOcclusion = g_xOcclusionTex.Sample(xSampleUV).r;
			float fSampleDepth     = g_xDepthTex.Sample(xSampleUV).r;

			if (fSampleDepth >= 1.0) continue;

			// Depth-difference Gaussian weight. Sigma scales with scene
			// depth so the filter widens for distant geometry without
			// over-blurring near surfaces.
			float fDepthDiff   = abs(fFullDepth - fSampleDepth);
			float fDepthSigma  = max(fFullDepth * UPSAMPLE_DEPTH_SIGMA_FRACTION, UPSAMPLE_MIN_DEPTH_SIGMA);
			float fDepthWeight = exp(-fDepthDiff * fDepthDiff / (2.0 * fDepthSigma * fDepthSigma));

			float fWeight = max(aafBilinear[x][y] * fDepthWeight, UPSAMPLE_MIN_WEIGHT);
			fResult      += fSampleOcclusion * fWeight;
			fTotalWeight += fWeight;
		}
	}

	float fOcclusion;
	if (fTotalWeight > UPSAMPLE_MIN_WEIGHT)
		fOcclusion = fResult / fTotalWeight;
	else
		fOcclusion = g_xOcclusionTex.Sample(input.uv).r; // bilateral fail-safe

	// All channels carry the occlusion factor — the SSAO blend pulls the
	// alpha channel as the multiplier against the HDR scene.
	return float4(fOcclusion, fOcclusion, fOcclusion, fOcclusion);
}
