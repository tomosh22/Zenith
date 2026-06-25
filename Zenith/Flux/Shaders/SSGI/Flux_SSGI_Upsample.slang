// =====================================================================
// Flux_SSGI_Upsample — Slang module.
//
// Subsystem: SSGI. Bilateral 2x2 upsample from half-res temporally
// accumulated SSGI to full-res. Depth-relative sigma keeps edges sharp
// across the full depth range without over-blurring at silhouettes.
//
// Standalone — no shared module imports beyond the fullscreen vsMain.
// =====================================================================

import Common.Fullscreen;
// Spine #include even though this shader uses no FrameConstants — it reserves
// sets 0/1/2 so PassParams lands at set 3.
#include "Common/Bindings.slang"

// Bilateral upsample tuning — see Flux_SSAO_Upsample for the same
// rationale. SSGI's RGB output uses depth-only weighting (no normal
// term) since it's already been temporally accumulated.
static const float UPSAMPLE_DEPTH_SIGMA_FRACTION = 0.02;
static const float UPSAMPLE_MIN_DEPTH_SIGMA      = 0.001;
static const float UPSAMPLE_MIN_WEIGHT           = 0.0001;

// PASS frequency (set 3) — the shader's own SRVs. Member names preserved exactly.
struct PassParams
{
	Sampler2D g_xSSGITex;   // half-res temporally accumulated
	Sampler2D g_xDepthTex;  // full-res depth
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float fFullDepth = g_xPassSet.g_xDepthTex.Sample(input.uv).r;
	if (fFullDepth >= 1.0) return float4(0.0, 0.0, 0.0, 0.0); // sky → no GI

	uint uW, uH;
	g_xPassSet.g_xSSGITex.GetDimensions(uW, uH);
	float2 xHalfResSize   = float2((float)uW, (float)uH);
	float2 xHalfTexelSize = 1.0 / xHalfResSize;

	float2 xHalfResPos   = input.uv * xHalfResSize - 0.5;
	float2 xHalfResFloor = floor(xHalfResPos);
	float2 xFrac         = xHalfResPos - xHalfResFloor;

	float aafBilinear[2][2];
	aafBilinear[0][0] = (1.0 - xFrac.x) * (1.0 - xFrac.y);
	aafBilinear[1][0] = xFrac.x * (1.0 - xFrac.y);
	aafBilinear[0][1] = (1.0 - xFrac.x) * xFrac.y;
	aafBilinear[1][1] = xFrac.x * xFrac.y;

	float4 xResult     = float4(0.0);
	float  fTotalWeight = 0.0;

	for (int y = 0; y <= 1; y++)
	{
		for (int x = 0; x <= 1; x++)
		{
			float2 xSampleUV = saturate((xHalfResFloor + float2(x, y) + 0.5) * xHalfTexelSize);
			float4 xSample      = g_xPassSet.g_xSSGITex.Sample(xSampleUV);
			float  fSampleDepth = g_xPassSet.g_xDepthTex.Sample(xSampleUV).r;

			if (fSampleDepth >= 1.0) continue;

			float fDepthDiff   = abs(fFullDepth - fSampleDepth);
			float fDepthSigma  = max(fFullDepth * UPSAMPLE_DEPTH_SIGMA_FRACTION, UPSAMPLE_MIN_DEPTH_SIGMA);
			float fDepthWeight = exp(-fDepthDiff * fDepthDiff / (2.0 * fDepthSigma * fDepthSigma));

			float fWeight = max(aafBilinear[x][y] * fDepthWeight, UPSAMPLE_MIN_WEIGHT);
			xResult      += xSample * fWeight;
			fTotalWeight += fWeight;
		}
	}

	if (fTotalWeight > UPSAMPLE_MIN_WEIGHT) return xResult / fTotalWeight;
	return g_xPassSet.g_xSSGITex.Sample(input.uv); // bilateral fail-safe
}
