// =====================================================================
// Flux_SSAO_Blur — Slang module.
//
// Subsystem: SSAO. Joint bilateral blur on the half-res SSAO target.
// Combines spatial / depth / normal weights to preserve geometry edges
// while smoothing the noisy raw occlusion. Operates at half-res; the
// upsample pass fans the result out to full-res afterwards.
//
// Does not read VIEW/GLOBAL data — operates purely on its own occlusion,
// depth and normal samplers. It still #includes Common/Bindings so the
// spine reserves sets 0/1/2 and this shader's own PassParams lands at set 3.
// =====================================================================

import Common.Fullscreen;
#include "Common/Bindings.slang"

static const float BLUR_MIN_WEIGHT       = 0.0001;
static const float BLUR_SKY_THRESHOLD    = 0.9999;
static const float BLUR_MIN_DEPTH_SIGMA  = 0.0001;

struct SSAOBlurConstantsLayout
{
	float u_fSpatialSigma;
	float u_fDepthSigma;
	float u_fNormalSigma;
	uint  u_uKernelRadius;
};
struct PassParams
{
	ConstantBuffer<SSAOBlurConstantsLayout> SSAOBlurConstants;
	Sampler2D g_xOcclusionTex;
	Sampler2D g_xDepthTex;
	Sampler2D g_xNormalTex;
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
	float fCenterOcclusion = g_xPassSet.g_xOcclusionTex.Sample(input.uv).r;
	float fCenterDepth     = g_xPassSet.g_xDepthTex.Sample(input.uv).r;

	if (fCenterDepth >= BLUR_SKY_THRESHOLD) return float4(1.0, 1.0, 1.0, 1.0);

	float3 xCenterNormal = normalize(g_xPassSet.g_xNormalTex.Sample(input.uv).rgb);

	float fDepthSigmaAbs = max(fCenterDepth * g_xPassSet.SSAOBlurConstants.u_fDepthSigma, BLUR_MIN_DEPTH_SIGMA);

	uint uW, uH;
	g_xPassSet.g_xOcclusionTex.GetDimensions(uW, uH);
	float2 xTexelSize = 1.0 / float2((float)uW, (float)uH);

	float fAccum     = 0.0;
	float fWeightSum = 0.0;
	int   iRadius    = (int)g_xPassSet.SSAOBlurConstants.u_uKernelRadius;

	for (int iY = -iRadius; iY <= iRadius; iY++)
	{
		for (int iX = -iRadius; iX <= iRadius; iX++)
		{
			float2 xOffset   = float2((float)iX, (float)iY);
			float2 xSampleUV = clamp(input.uv + xOffset * xTexelSize, float2(0.001), float2(0.999));

			float fSampleOcclusion = g_xPassSet.g_xOcclusionTex.Sample(xSampleUV).r;
			float fSampleDepth     = g_xPassSet.g_xDepthTex.Sample(xSampleUV).r;
			float3 xSampleNormal   = normalize(g_xPassSet.g_xNormalTex.Sample(xSampleUV).rgb);

			float fSpatialDist   = length(xOffset);
			float fSpatialWeight = GaussianWeight(fSpatialDist, g_xPassSet.SSAOBlurConstants.u_fSpatialSigma);

			float fDepthDiff   = abs(fSampleDepth - fCenterDepth);
			float fDepthWeight = GaussianWeight(fDepthDiff, fDepthSigmaAbs);

			// Normal weight: 1 - cos(angle) makes parallel surfaces stay
			// fully weighted while sharp normal changes (geometry edges)
			// fall off via the gaussian.
			float fNormalDiff   = 1.0 - max(dot(xCenterNormal, xSampleNormal), 0.0);
			float fNormalWeight = GaussianWeight(fNormalDiff, g_xPassSet.SSAOBlurConstants.u_fNormalSigma);

			float fWeight = fSpatialWeight * fDepthWeight * fNormalWeight;
			fAccum     += fSampleOcclusion * fWeight;
			fWeightSum += fWeight;
		}
	}

	float fResult = (fWeightSum > BLUR_MIN_WEIGHT) ? fAccum / fWeightSum : fCenterOcclusion;
	return float4(fResult, fResult, fResult, fResult);
}
