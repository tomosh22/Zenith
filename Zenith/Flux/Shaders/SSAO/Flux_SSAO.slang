// =====================================================================
// Flux_SSAO — Slang module.
//
// Subsystem: SSAO. Half-resolution screen-space ambient occlusion.
// Reconstructs view-space position from the depth buffer, samples
// along NUM_DIRECTIONS x N step rays in screen space, and accumulates
// occlusion from samples whose direction vector has a positive
// component along the surface normal (hemisphere test).
//
// Includes Common/Bindings for the VIEW set (view + projection matrices)
// and the GetViewPosFromDepth helper.
// =====================================================================

import Common.Fullscreen;
#include "Common/Bindings.slang"

struct SSAOConstantsLayout
{
	float RADIUS;
	float BIAS;
	float INTENSITY;
	uint  USE_IGN_ROTATION;
};
struct PassParams
{
	ConstantBuffer<SSAOConstantsLayout> SSAOConstants;
	Sampler2D g_xDepthTex;
	Sampler2D g_xNormalTex;
};
ParameterBlock<PassParams> g_xPassSet;

// Two pipeline variants specialise this value to 32 (current/default) or 16
// (Phase-3 opt-in). Keeping it out of the per-draw CB lets the driver fold the
// step count and unroll the fixed loop after Vulkan specialization.
[SpecializationConstant] const uint FLUX_SC_SSAO_KERNEL_SIZE = 32u;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

static const int   NUM_DIRECTIONS = 8;
static const float TWO_PI         = 6.28318530718;
static const float INV_SQRT_TWO   = 0.70710678118;

// Eight evenly spaced unit directions. The per-pixel random rotation below is
// applied as one 2-D matrix multiply per direction, so each pixel evaluates one
// sin/cos pair instead of one pair for every direction.
static const float2 DIRECTION_VECTORS[NUM_DIRECTIONS] = {
	float2( 1.0,          0.0),
	float2( INV_SQRT_TWO, INV_SQRT_TWO),
	float2( 0.0,          1.0),
	float2(-INV_SQRT_TWO, INV_SQRT_TWO),
	float2(-1.0,          0.0),
	float2(-INV_SQRT_TWO,-INV_SQRT_TWO),
	float2( 0.0,         -1.0),
	float2( INV_SQRT_TWO,-INV_SQRT_TWO),
};

float LegacyRotationHash(float2 xP)
{
	return frac(sin(dot(xP, float2(12.9898, 78.233))) * 43758.5453);
}

// Jimenez interleaved-gradient noise: deterministic in pixel space and much
// less structured than the sine hash, with blue-noise-like local distribution.
float InterleavedGradientNoise(float2 xPixel)
{
	return frac(52.9829189 * frac(dot(xPixel, float2(0.06711056, 0.00583715))));
}

float RotationHash(float2 xPixel)
{
	return (g_xPassSet.SSAOConstants.USE_IGN_ROTATION != 0u)
		? InterleavedGradientNoise(xPixel)
		: LegacyRotationHash(xPixel);
}

// Closed-form inverse projection for the standard engine projection shape.
// Solving depth = clip.z / clip.w first avoids a full inverse-matrix multiply
// for every SSAO tap. P[3][2]/P[3][3] retain perspective vs orthographic
// behaviour, while P[0][2]/P[1][2] retain TAA jitter (and off-centre frusta).
// The engine projection matrices have no XY skew or XY contribution to clip.w.
float3 ViewPosFromDepthClosedForm(
	float2 xUV,
	float fDepth,
	float2 xRcpProjScale,
	float2 xProjZToXY,
	float2 xProjTranslationXY,
	float4 xDepthCoefficients)
{
	// xDepthCoefficients = (P22, P23, P32, P33).
	float fViewZ = (xDepthCoefficients.y - fDepth * xDepthCoefficients.w) /
		(fDepth * xDepthCoefficients.z - xDepthCoefficients.x);
	float fClipW = xDepthCoefficients.z * fViewZ + xDepthCoefficients.w;
	float2 xNDC = xUV * 2.0 - 1.0;
	float2 xViewXY = (xNDC * fClipW - xProjZToXY * fViewZ - xProjTranslationXY) * xRcpProjScale;
	return float3(xViewXY, fViewZ);
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input, float4 fragCoord : SV_Position) : SV_Target
{
	float fDepth = g_xPassSet.g_xDepthTex.Sample(input.uv).x;

	if (fDepth >= 1.0) return float4(1.0, 1.0, 1.0, 1.0); // sky → unoccluded

	// Hoist the uniform projection coefficients once per pixel. The reconstruction
	// helper supports both perspective and orthographic matrices and keeps the
	// jitter terms injected into P02/P12 by TAA.
	float4x4 xProj = GetProjMat();
	float2 xProjScale = float2(xProj[0][0], xProj[1][1]);
	float2 xRcpProjScale = 1.0 / xProjScale;
	float2 xProjZToXY = float2(xProj[0][2], xProj[1][2]);
	float2 xProjTranslationXY = float2(xProj[0][3], xProj[1][3]);
	float4 xDepthCoefficients = float4(xProj[2][2], xProj[2][3], xProj[3][2], xProj[3][3]);

	float3 xFragPos     = ViewPosFromDepthClosedForm(input.uv, fDepth,
		xRcpProjScale, xProjZToXY, xProjTranslationXY, xDepthCoefficients);
	float3 xWorldNormal = normalize(g_xPassSet.g_xNormalTex.Sample(input.uv).rgb);
	float3 xNormal      = normalize(mul((float3x3)GetViewMat(), xWorldNormal));

	// Project the world-space radius through clip.w. For perspective clip.w is
	// +/-viewZ (the historical 1/depth behaviour); for orthographic it is the
	// constant P33, so the screen footprint correctly remains depth-independent.
	float fClipW = xDepthCoefficients.z * xFragPos.z + xDepthCoefficients.w;
	float fInvClipW = 0.5 / max(abs(fClipW), 0.000001);
	float2 xScreenScale = float2(
		g_xPassSet.SSAOConstants.RADIUS * abs(xProjScale.x) * fInvClipW,
		g_xPassSet.SSAOConstants.RADIUS * abs(xProjScale.y) * fInvClipW);

	// Per-pixel random rotation breaks the banding pattern an unrotated
	// 8-direction sample would otherwise produce.
	float fRotation = RotationHash(fragCoord.xy) * TWO_PI;
	float fSinRotation = sin(fRotation);
	float fCosRotation = cos(fRotation);

	int iSteps = max(1, (int)FLUX_SC_SSAO_KERNEL_SIZE / NUM_DIRECTIONS);

	float fOcclusion    = 0.0;
	int   iValidSamples = 0;

	[unroll]
	for (int iDir = 0; iDir < NUM_DIRECTIONS; iDir++)
	{
		float2 xBaseDirection = DIRECTION_VECTORS[iDir];
		float2 xDirection = float2(
			xBaseDirection.x * fCosRotation - xBaseDirection.y * fSinRotation,
			xBaseDirection.x * fSinRotation + xBaseDirection.y * fCosRotation);
		float2 xStepUV = xDirection * xScreenScale / (float)iSteps;

		for (int iStep = 1; iStep <= iSteps; iStep++)
		{
			float2 xSampleUV = input.uv + xStepUV * (float)iStep;
			if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
				xSampleUV.y < 0.0 || xSampleUV.y > 1.0) continue;

			float fSampleDepth = g_xPassSet.g_xDepthTex.Sample(xSampleUV).x;
			if (fSampleDepth >= 1.0) continue; // skip sky

			float3 xSamplePos = ViewPosFromDepthClosedForm(xSampleUV, fSampleDepth,
				xRcpProjScale, xProjZToXY, xProjTranslationXY, xDepthCoefficients);
			float3 xDiff      = xSamplePos - xFragPos;
			float  fDist      = length(xDiff);
			if (fDist < 0.0001) continue;

			// Hemisphere test: positive component along the surface normal
			// means the sample is "above" the tangent plane and blocks
			// incoming ambient light.
			float fNDot = dot(xDiff / fDist, xNormal);
			if (fNDot > g_xPassSet.SSAOConstants.BIAS)
			{
				float fFalloff = max(0.0, 1.0 - fDist / g_xPassSet.SSAOConstants.RADIUS);
				fFalloff *= fFalloff;
				fOcclusion += fNDot * fFalloff;
			}

			iValidSamples++;
		}
	}

	float fResult = (iValidSamples > 0)
		? saturate(1.0 - fOcclusion / (float)iValidSamples)
		: 1.0;

	fResult = pow(fResult, g_xPassSet.SSAOConstants.INTENSITY);
	return float4(fResult, fResult, fResult, fResult);
}
