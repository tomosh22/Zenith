#version 450 core

// Joint bilateral blur for SSAO (operates at half-resolution)
// Uses depth + normal edge-stopping to preserve geometry boundaries

const float BLUR_MIN_WEIGHT = 0.0001;
const float BLUR_SKY_THRESHOLD = 0.9999;
const float BLUR_MIN_DEPTH_SIGMA = 0.0001;

layout(location = 0) out vec4 o_xColour;
layout(location = 0) in vec2 a_xUV;

layout(std140, set = 0, binding = 1) uniform SSAOBlurConstants {
	float u_fSpatialSigma;
	float u_fDepthSigma;
	float u_fNormalSigma;
	uint u_uKernelRadius;
};

layout(set = 0, binding = 2) uniform sampler2D g_xOcclusionTex;
layout(set = 0, binding = 3) uniform sampler2D g_xDepthTex;
layout(set = 0, binding = 4) uniform sampler2D g_xNormalTex;

float GaussianWeight(float fDist, float fSigma)
{
	float fNorm = fDist / fSigma;
	return exp(-0.5 * fNorm * fNorm);
}

void main()
{
	float fCenterOcclusion = texture(g_xOcclusionTex, a_xUV).r;
	float fCenterDepth = texture(g_xDepthTex, a_xUV).r;

	// Sky pixels: no occlusion
	if (fCenterDepth >= BLUR_SKY_THRESHOLD)
	{
		o_xColour = vec4(1.0);
		return;
	}

	vec3 xCenterNormal = normalize(texture(g_xNormalTex, a_xUV).rgb);

	// Depth-relative sigma for scale-invariant edge detection
	float fDepthSigmaAbs = max(fCenterDepth * u_fDepthSigma, BLUR_MIN_DEPTH_SIGMA);

	vec2 xTexelSize = 1.0 / vec2(textureSize(g_xOcclusionTex, 0));

	float fAccum = 0.0;
	float fWeightSum = 0.0;
	int iRadius = int(u_uKernelRadius);

	for (int iY = -iRadius; iY <= iRadius; iY++)
	{
		for (int iX = -iRadius; iX <= iRadius; iX++)
		{
			vec2 xOffset = vec2(float(iX), float(iY));
			vec2 xSampleUV = clamp(a_xUV + xOffset * xTexelSize, vec2(0.001), vec2(0.999));

			float fSampleOcclusion = texture(g_xOcclusionTex, xSampleUV).r;
			float fSampleDepth = texture(g_xDepthTex, xSampleUV).r;
			vec3 xSampleNormal = normalize(texture(g_xNormalTex, xSampleUV).rgb);

			// Spatial weight
			float fSpatialDist = length(xOffset);
			float fSpatialWeight = GaussianWeight(fSpatialDist, u_fSpatialSigma);

			// Depth weight
			float fDepthDiff = abs(fSampleDepth - fCenterDepth);
			float fDepthWeight = GaussianWeight(fDepthDiff, fDepthSigmaAbs);

			// Normal weight
			float fNormalDiff = 1.0 - max(dot(xCenterNormal, xSampleNormal), 0.0);
			float fNormalWeight = GaussianWeight(fNormalDiff, u_fNormalSigma);

			float fWeight = fSpatialWeight * fDepthWeight * fNormalWeight;
			fAccum += fSampleOcclusion * fWeight;
			fWeightSum += fWeight;
		}
	}

	float fResult = (fWeightSum > BLUR_MIN_WEIGHT) ? fAccum / fWeightSum : fCenterOcclusion;
	o_xColour = vec4(fResult);
}
