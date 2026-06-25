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
	float KERNEL_SIZE;
};
struct PassParams
{
	ConstantBuffer<SSAOConstantsLayout> SSAOConstants;
	Sampler2D g_xDepthTex;
	Sampler2D g_xNormalTex;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

static const int   NUM_DIRECTIONS = 8;
static const float TWO_PI         = 6.28318530718;

float Hash(float2 xP)
{
	return frac(sin(dot(xP, float2(12.9898, 78.233))) * 43758.5453);
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input, float4 fragCoord : SV_Position) : SV_Target
{
	float fDepth = g_xPassSet.g_xDepthTex.Sample(input.uv).x;

	if (fDepth >= 1.0) return float4(1.0, 1.0, 1.0, 1.0); // sky → unoccluded

	float3 xFragPos     = GetViewPosFromDepth(input.uv, fDepth);
	float3 xWorldNormal = normalize(g_xPassSet.g_xNormalTex.Sample(input.uv).rgb);
	float3 xNormal      = normalize(mul((float3x3)g_xViewSet.g_xView.g_xViewMat, xWorldNormal));

	// World-space SSAO radius projected into UV space scales by 1/depth and
	// by the projection's per-axis scale — so the screen footprint of the
	// sphere stays correct under perspective.
	float fInvZ        = 0.5 / abs(xFragPos.z);
	float2 xScreenScale = float2(
		g_xPassSet.SSAOConstants.RADIUS * abs(g_xViewSet.g_xView.g_xProjMat[0][0]) * fInvZ,
		g_xPassSet.SSAOConstants.RADIUS * abs(g_xViewSet.g_xView.g_xProjMat[1][1]) * fInvZ);

	// Per-pixel random rotation breaks the banding pattern an unrotated
	// 8-direction sample would otherwise produce.
	float fRotation = Hash(fragCoord.xy) * TWO_PI;

	int iSteps = max(1, (int)g_xPassSet.SSAOConstants.KERNEL_SIZE / NUM_DIRECTIONS);

	float fOcclusion    = 0.0;
	int   iValidSamples = 0;

	for (int iDir = 0; iDir < NUM_DIRECTIONS; iDir++)
	{
		float  fAngle  = ((float)iDir / (float)NUM_DIRECTIONS) * TWO_PI + fRotation;
		float2 xStepUV = float2(cos(fAngle), sin(fAngle)) * xScreenScale / (float)iSteps;

		for (int iStep = 1; iStep <= iSteps; iStep++)
		{
			float2 xSampleUV = input.uv + xStepUV * (float)iStep;
			if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
				xSampleUV.y < 0.0 || xSampleUV.y > 1.0) continue;

			float fSampleDepth = g_xPassSet.g_xDepthTex.Sample(xSampleUV).x;
			if (fSampleDepth >= 1.0) continue; // skip sky

			float3 xSamplePos = GetViewPosFromDepth(xSampleUV, fSampleDepth);
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
