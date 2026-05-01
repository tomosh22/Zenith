// =====================================================================
// Flux_SSR_Upsample — Slang module.
//
// Subsystem: SSR. Bilateral 2x2 upsample from half-res ray-march output
// to full-res. Depth-weighted weights keep edges sharp at silhouettes.
// Output RGB = reflection colour, A = hit confidence (preserved from
// the ray-march pass).
//
// Texture dimensions come from SSRConstants — never via GetDimensions
// inside the shader (project convention).
// =====================================================================

import Common.Fullscreen;

// Bilateral upsample tuning — depth-relative sigma keeps edges sharp
// across the full depth range without over-blurring at silhouettes.
static const float UPSAMPLE_DEPTH_SIGMA_FRACTION = 0.02;
static const float UPSAMPLE_MIN_DEPTH_SIGMA      = 0.001;
static const float UPSAMPLE_MIN_WEIGHT           = 0.0001;

// Layout matches the CPU SSRConstants struct end-to-end so the shared CBV
// is interpretable from any SSR pass. Upsample only needs the half-res
// dimensions, but the prefix fields are kept to maintain layout.
struct SSRConstantsLayout
{
	float u_fIntensity;
	float u_fMaxDistance;
	float u_fMaxRoughness;
	float u_fThickness;
	uint  u_uStepCount;
	uint  u_uDebugMode;
	uint  u_uHiZMipCount;
	uint  u_uStartMip;
	uint  u_uFrameIndex;
	uint  u_uBinarySearchIterations;
	float u_fContactHardeningDist;
	float u_fHalfResWidth;
	float u_fHalfResHeight;
	float u_fRcpHalfResWidth;
	float u_fRcpHalfResHeight;
	float u_fPad0;
	float4 u_axHiZMipSizes[12];
};
[[vk::binding(0, 0)]] ConstantBuffer<SSRConstantsLayout> SSRConstants;
[[vk::binding(1, 0)]] Sampler2D g_xSSRTex;     // half-res ray-march output
[[vk::binding(2, 0)]] Sampler2D g_xDepthTex;   // full-res depth

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float fFullDepth = g_xDepthTex.Sample(input.uv).r;
	if (fFullDepth >= 1.0) return float4(0.0, 0.0, 0.0, 0.0); // sky

	float2 xHalfResSize   = float2(SSRConstants.u_fHalfResWidth,    SSRConstants.u_fHalfResHeight);
	float2 xHalfTexelSize = float2(SSRConstants.u_fRcpHalfResWidth, SSRConstants.u_fRcpHalfResHeight);

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
			float4 xSample      = g_xSSRTex.Sample(xSampleUV);
			float  fSampleDepth = g_xDepthTex.Sample(xSampleUV).r;

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
	return g_xSSRTex.Sample(input.uv); // bilateral fail-safe
}
