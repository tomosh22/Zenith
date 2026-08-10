// Shared source for the fixed-axis SSAO bilateral passes. Resource layout and
// axis-specific accumulation stay local; numerical weights and the fallback
// threshold come from Common.Bilateral.

#ifndef FLUX_SSAO_BLURCOMMON_SLANG
#define FLUX_SSAO_BLURCOMMON_SLANG

import Common.Fullscreen;
import Common.Bilateral;
#include "Common/Bindings.slang"

static const float BLUR_SKY_THRESHOLD    = 0.9999;
static const float BLUR_MIN_DEPTH_SIGMA  = 0.0001;

struct SSAOBlurConstantsLayout
{
	float u_fSpatialSigma;
	float u_fDepthSigma;
	float u_fNormalSigma;
	uint  u_uKernelRadius;
	float2 u_xRcpTexelSize;
	float2 u_xPadding;
};
struct PassParams
{
	ConstantBuffer<SSAOBlurConstantsLayout> SSAOBlurConstants;
	Sampler2D g_xOcclusionTex;
	Sampler2D g_xDepthTex;
	Sampler2D g_xNormalTex;
};
ParameterBlock<PassParams> g_xPassSet;

float4 SSAOBlur1D(float2 xUV, float2 xAxis)
{
	float fCenterOcclusion = g_xPassSet.g_xOcclusionTex.SampleLevel(xUV, 0).r;
	float fCenterDepth     = g_xPassSet.g_xDepthTex.SampleLevel(xUV, 0).r;

	if (fCenterDepth >= BLUR_SKY_THRESHOLD) return float4(1.0, 1.0, 1.0, 1.0);

	float3 xCenterNormal = normalize(g_xPassSet.g_xNormalTex.SampleLevel(xUV, 0).rgb);
	float fDepthSigmaAbs = max(fCenterDepth * g_xPassSet.SSAOBlurConstants.u_fDepthSigma, BLUR_MIN_DEPTH_SIGMA);
	float2 xTexelSize = g_xPassSet.SSAOBlurConstants.u_xRcpTexelSize;

	float fAccum     = 0.0;
	float fWeightSum = 0.0;
	int   iRadius    = (int)g_xPassSet.SSAOBlurConstants.u_uKernelRadius;

	for (int iTap = -iRadius; iTap <= iRadius; iTap++)
	{
		// saturate rather than relying on the caller's sampler address mode: the
		// kernel steps off-screen near the borders, and the shader cannot see
		// which sampler was bound.
		float2 xSampleUV = saturate(xUV + xAxis * (float)iTap * xTexelSize);
		float fSampleDepth = g_xPassSet.g_xDepthTex.SampleLevel(xSampleUV, 0).r;
		// Sky G-buffer normals are zero. Reject the tap before normalizing so
		// 0/0 cannot poison the bilateral accumulator with NaNs.
		if (fSampleDepth >= BLUR_SKY_THRESHOLD) continue;

		float fSampleOcclusion = g_xPassSet.g_xOcclusionTex.SampleLevel(xSampleUV, 0).r;
		float3 xSampleNormal   = normalize(g_xPassSet.g_xNormalTex.SampleLevel(xSampleUV, 0).rgb);

		// A fixed-axis tap has scalar distance; abs replaces the old 2-D length
		// only here, after the blur has become separable.
		float fSpatialWeight = BilateralGaussianWeight(
			abs((float)iTap), g_xPassSet.SSAOBlurConstants.u_fSpatialSigma);
		float fDepthWeight = BilateralDepthWeight(fCenterDepth, fSampleDepth, fDepthSigmaAbs);
		float fNormalWeight = BilateralNormalWeight(
			xCenterNormal, xSampleNormal, g_xPassSet.SSAOBlurConstants.u_fNormalSigma);

		float fWeight = fSpatialWeight * fDepthWeight * fNormalWeight;
		fAccum     += fSampleOcclusion * fWeight;
		fWeightSum += fWeight;
	}

	// The centre tap has zero bilateral deltas and therefore weight 1. Keep the
	// fallback nevertheless as a finite-result invariant for malformed tuning.
	float fResult = (fWeightSum > BILATERAL_MIN_WEIGHT) ? fAccum / fWeightSum : fCenterOcclusion;
	return float4(fResult, fResult, fResult, fResult);
}

#endif // FLUX_SSAO_BLURCOMMON_SLANG
