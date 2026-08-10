// =====================================================================
// Common.Bilateral — shared joint-bilateral weighting helpers.
//
// This module owns no resources or parameter blocks, so importing it cannot
// change descriptor layouts. Keep the Gaussian formulation and the fallback
// threshold here so SSAO, SSGI and SSR use one numerical definition.
// =====================================================================

// exp(-0.5 * x^2) == exp2((-0.5 / ln(2)) * x^2).
public static const float BILATERAL_GAUSSIAN_EXP2_SCALE = -0.72134752;

// Canonical fallback guard. Active SSAO/SSGI kernels include their centre tap:
// all bilateral deltas are zero there, so that tap has weight 1 and the
// accumulated weight is at least 1. Lowering their former 1e-4 guard to this
// value therefore cannot change the selected output branch.
public static const float BILATERAL_MIN_WEIGHT = 1e-6;

public float BilateralGaussianWeight(float fDistance, float fSigma)
{
	float fNormalisedDistance = fDistance / fSigma;
	return exp2(BILATERAL_GAUSSIAN_EXP2_SCALE * fNormalisedDistance * fNormalisedDistance);
}

public float BilateralDepthWeight(float fCenterDepth, float fSampleDepth, float fSigma)
{
	return BilateralGaussianWeight(abs(fSampleDepth - fCenterDepth), fSigma);
}

public float BilateralNormalWeight(float3 xCenterNormal, float3 xSampleNormal, float fSigma)
{
	float fNormalDot = max(dot(xCenterNormal, xSampleNormal), 0.0);
	return BilateralGaussianWeight(1.0 - fNormalDot, fSigma);
}

public float BilateralAlbedoWeight(float3 xCenterAlbedo, float3 xSampleAlbedo, float fSigma)
{
	return BilateralGaussianWeight(length(xSampleAlbedo - xCenterAlbedo), fSigma);
}
