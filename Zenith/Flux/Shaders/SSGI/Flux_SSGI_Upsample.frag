#version 450 core

// Note: This shader does NOT include Common.fxh because it doesn't need frame constants.
// It only needs the two texture inputs for bilateral upsampling.

// ========== BILATERAL UPSAMPLE CONSTANTS ==========
// Depth-relative sigma for better silhouette handling (matches SSR_Resolve pattern)
// Using fraction of local depth instead of fixed NDC sigma provides consistent
// behavior across different depth ranges and prevents over-blurring at silhouettes
const float UPSAMPLE_DEPTH_SIGMA_FRACTION = 0.02;  // 2% of local depth
const float UPSAMPLE_MIN_DEPTH_SIGMA = 0.001;      // Absolute minimum for near-plane
const float UPSAMPLE_MIN_WEIGHT = 0.0001;          // Minimum sample weight

layout(location = 0) out vec4 o_xSSGI;

layout(location = 0) in vec2 a_xUV;

// Textures
layout(set = 0, binding = 0) uniform sampler2D g_xSSGITex;   // Half-res SSGI (temporal accumulated)
layout(set = 0, binding = 1) uniform sampler2D g_xDepthTex;  // Full-res depth

void main()
{
	// Get full-res depth at this pixel
	float fFullDepth = texture(g_xDepthTex, a_xUV).r;

	// Early exit for sky
	if (fFullDepth >= 1.0)
	{
		o_xSSGI = vec4(0.0);
		return;
	}

	// Get half-res texel size
	vec2 xHalfResSize = vec2(textureSize(g_xSSGITex, 0));
	vec2 xHalfTexelSize = 1.0 / xHalfResSize;

	// Full-res depth for bilateral weighting
	// We need to sample depth at the same positions as the SSGI samples
	vec2 xFullResSize = vec2(textureSize(g_xDepthTex, 0));

	// Convert full depth to linear for better comparison
	// (Actually we can just compare raw depth values since we're looking for similarity)

	// Bilateral 2x2 upsample
	// Sample the 4 nearest half-res texels and weight by depth similarity
	vec4 xResult = vec4(0.0);
	float fTotalWeight = 0.0;

	// Calculate the position in half-res space
	vec2 xHalfResPos = a_xUV * xHalfResSize - 0.5;
	vec2 xHalfResFloor = floor(xHalfResPos);
	vec2 xFrac = xHalfResPos - xHalfResFloor;

	// Bilinear weights
	float aafBilinear[2][2];
	aafBilinear[0][0] = (1.0 - xFrac.x) * (1.0 - xFrac.y);
	aafBilinear[1][0] = xFrac.x * (1.0 - xFrac.y);
	aafBilinear[0][1] = (1.0 - xFrac.x) * xFrac.y;
	aafBilinear[1][1] = xFrac.x * xFrac.y;

	// Sample 2x2 neighborhood
	for (int y = 0; y <= 1; y++)
	{
		for (int x = 0; x <= 1; x++)
		{
			// Half-res UV for this sample
			vec2 xSampleUV = (xHalfResFloor + vec2(x, y) + 0.5) * xHalfTexelSize;

			// Clamp to valid range
			xSampleUV = clamp(xSampleUV, vec2(0.0), vec2(1.0));

			// Sample SSGI
			vec4 xSample = texture(g_xSSGITex, xSampleUV);

			// Get depth at the corresponding full-res position
			// The half-res sample represents a 2x2 block in full-res
			// Sample depth at the center of that block
			vec2 xDepthUV = xSampleUV;
			float fSampleDepth = texture(g_xDepthTex, xDepthUV).r;

			// Skip sky samples
			if (fSampleDepth >= 1.0)
			{
				continue;
			}

			// Bilateral depth weight (depth-relative sigma for better silhouette handling)
			float fDepthDiff = abs(fFullDepth - fSampleDepth);
			float fDepthSigma = max(fFullDepth * UPSAMPLE_DEPTH_SIGMA_FRACTION, UPSAMPLE_MIN_DEPTH_SIGMA);
			float fDepthWeight = exp(-fDepthDiff * fDepthDiff / (2.0 * fDepthSigma * fDepthSigma));

			// Combine bilinear and depth weights
			float fWeight = aafBilinear[x][y] * fDepthWeight;
			fWeight = max(fWeight, UPSAMPLE_MIN_WEIGHT);

			xResult += xSample * fWeight;
			fTotalWeight += fWeight;
		}
	}

	// Normalize
	if (fTotalWeight > UPSAMPLE_MIN_WEIGHT)
	{
		o_xSSGI = xResult / fTotalWeight;
	}
	else
	{
		// Fallback to center sample if all weights are zero
		o_xSSGI = texture(g_xSSGITex, a_xUV);
	}
}
