#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

layout(std140, set = 0, binding = 1) uniform SSAOConstants{
	float RADIUS;
	float BIAS;
	float INTENSITY;
	float KERNEL_SIZE;
};

layout(set = 0, binding = 2) uniform sampler2D g_xDepthTex;
layout(set = 0, binding = 3) uniform sampler2D g_xNormalTex;

const int NUM_DIRECTIONS = 8;
const float TWO_PI = 6.28318530718;

float Hash(vec2 xP)
{
	return fract(sin(dot(xP, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
	float fDepth = texture(g_xDepthTex, a_xUV).x;

	// Early exit for skybox
	if (fDepth >= 1.0)
	{
		o_xColour = vec4(1.0);
		return;
	}

	// View-space position
	vec3 xFragPos = GetViewPosFromDepth(a_xUV, fDepth);

	// G-buffer stores world normals directly in [-1,1] SFLOAT — no decode needed
	vec3 xWorldNormal = normalize(texture(g_xNormalTex, a_xUV).rgb);
	vec3 xNormal = normalize(mat3(g_xViewMat) * xWorldNormal);

	// Project view-space RADIUS to screen-space per axis
	// A world-space circle projects to a UV-space ellipse due to aspect ratio
	float fInvZ = 0.5 / abs(xFragPos.z);
	vec2 xScreenScale = vec2(
		RADIUS * abs(g_xProjMat[0][0]) * fInvZ,
		RADIUS * abs(g_xProjMat[1][1]) * fInvZ
	);

	// Per-pixel random rotation to break banding
	float fRotation = Hash(gl_FragCoord.xy) * TWO_PI;

	int iSteps = max(1, int(KERNEL_SIZE) / NUM_DIRECTIONS);

	float fOcclusion = 0.0;
	int iValidSamples = 0;

	for (int iDir = 0; iDir < NUM_DIRECTIONS; iDir++)
	{
		float fAngle = (float(iDir) / float(NUM_DIRECTIONS)) * TWO_PI + fRotation;
		vec2 xStepUV = vec2(cos(fAngle), sin(fAngle)) * xScreenScale / float(iSteps);

		for (int iStep = 1; iStep <= iSteps; iStep++)
		{
			vec2 xSampleUV = a_xUV + xStepUV * float(iStep);

			// Skip samples outside screen bounds
			if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
				xSampleUV.y < 0.0 || xSampleUV.y > 1.0)
			{
				continue;
			}

			float fSampleDepth = texture(g_xDepthTex, xSampleUV).x;
			if (fSampleDepth >= 1.0) continue; // Skip sky samples

			// Reconstruct view-space position at the sample's screen location
			vec3 xSamplePos = GetViewPosFromDepth(xSampleUV, fSampleDepth);

			// Vector from fragment to scene surface at sample screen position
			vec3 xDiff = xSamplePos - xFragPos;
			float fDist = length(xDiff);

			if (fDist < 0.0001) continue;

			// Hemisphere check: positive dot means sample is above tangent plane
			// (in the normal hemisphere) — it blocks incoming ambient light
			float fNDot = dot(xDiff / fDist, xNormal);

			if (fNDot > BIAS)
			{
				// Quadratic distance falloff within RADIUS
				float fFalloff = max(0.0, 1.0 - fDist / RADIUS);
				fFalloff *= fFalloff;

				fOcclusion += fNDot * fFalloff;
			}

			iValidSamples++;
		}
	}

	float fResult;
	if (iValidSamples > 0)
	{
		fResult = clamp(1.0 - fOcclusion / float(iValidSamples), 0.0, 1.0);
	}
	else
	{
		fResult = 1.0;
	}

	// Apply intensity (power curve)
	fResult = pow(fResult, INTENSITY);

	o_xColour = vec4(fResult);
}
