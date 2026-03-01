#version 450 core

// Bilateral upsample from half-res SSAO to full-res
// Adapted from Flux_SSGI_Upsample.frag
// Uses depth-weighted 2x2 bilateral filter to preserve edges during upsampling

const float UPSAMPLE_DEPTH_SIGMA_FRACTION = 0.02; // 2% of local depth
const float UPSAMPLE_MIN_DEPTH_SIGMA = 0.001;
const float UPSAMPLE_MIN_WEIGHT = 0.0001;

layout(location = 0) out vec4 o_xColour;
layout(location = 0) in vec2 a_xUV;

layout(set = 0, binding = 0) uniform sampler2D g_xOcclusionTex; // Half-res blurred SSAO
layout(set = 0, binding = 1) uniform sampler2D g_xDepthTex;     // Full-res depth

void main()
{
	float fFullDepth = texture(g_xDepthTex, a_xUV).r;

	// Sky: no occlusion
	if (fFullDepth >= 1.0)
	{
		o_xColour = vec4(1.0);
		return;
	}

	vec2 xHalfResSize = vec2(textureSize(g_xOcclusionTex, 0));
	vec2 xHalfTexelSize = 1.0 / xHalfResSize;

	// Bilateral 2x2 upsample
	float fResult = 0.0;
	float fTotalWeight = 0.0;

	// Position in half-res space
	vec2 xHalfResPos = a_xUV * xHalfResSize - 0.5;
	vec2 xHalfResFloor = floor(xHalfResPos);
	vec2 xFrac = xHalfResPos - xHalfResFloor;

	// Bilinear weights
	float aafBilinear[2][2];
	aafBilinear[0][0] = (1.0 - xFrac.x) * (1.0 - xFrac.y);
	aafBilinear[1][0] = xFrac.x * (1.0 - xFrac.y);
	aafBilinear[0][1] = (1.0 - xFrac.x) * xFrac.y;
	aafBilinear[1][1] = xFrac.x * xFrac.y;

	for (int y = 0; y <= 1; y++)
	{
		for (int x = 0; x <= 1; x++)
		{
			vec2 xSampleUV = clamp((xHalfResFloor + vec2(x, y) + 0.5) * xHalfTexelSize, vec2(0.0), vec2(1.0));

			float fSampleOcclusion = texture(g_xOcclusionTex, xSampleUV).r;
			float fSampleDepth = texture(g_xDepthTex, xSampleUV).r;

			if (fSampleDepth >= 1.0)
			{
				continue;
			}

			// Bilateral depth weight
			float fDepthDiff = abs(fFullDepth - fSampleDepth);
			float fDepthSigma = max(fFullDepth * UPSAMPLE_DEPTH_SIGMA_FRACTION, UPSAMPLE_MIN_DEPTH_SIGMA);
			float fDepthWeight = exp(-fDepthDiff * fDepthDiff / (2.0 * fDepthSigma * fDepthSigma));

			float fWeight = max(aafBilinear[x][y] * fDepthWeight, UPSAMPLE_MIN_WEIGHT);

			fResult += fSampleOcclusion * fWeight;
			fTotalWeight += fWeight;
		}
	}

	float fOcclusion;
	if (fTotalWeight > UPSAMPLE_MIN_WEIGHT)
	{
		fOcclusion = fResult / fTotalWeight;
	}
	else
	{
		fOcclusion = texture(g_xOcclusionTex, a_xUV).r;
	}

	// All channels set to occlusion - alpha drives the multiplicative blend
	o_xColour = vec4(fOcclusion);
}
