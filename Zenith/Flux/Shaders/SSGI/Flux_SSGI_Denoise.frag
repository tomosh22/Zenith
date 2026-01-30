#version 450 core

//=============================================================================
// SSGI Joint Bilateral Denoise Shader
//=============================================================================
// This shader applies a joint bilateral filter to the upsampled SSGI result.
// It uses depth, normal, and albedo edges from the G-buffer to preserve
// geometric and material boundaries while smoothing noise.
//
// Physical Motivation:
// - Same depth -> same surface -> should have similar GI
// - Same normal -> same orientation -> similar lighting response
// - Same albedo -> same material -> similar GI interaction
//
// Weight(p,q) = G_spatial(||p-q||)
//             x G_depth(|depth_p - depth_q| / depth_p)
//             x G_normal(1 - dot(N_p, N_q))
//             x G_albedo(||albedo_p - albedo_q||)
//=============================================================================

//=============================================================================
// CONSTANTS
//=============================================================================

// Minimum weight to prevent division by zero
const float DENOISE_MIN_WEIGHT = 0.0001;

// Sky detection threshold
const float DENOISE_SKY_THRESHOLD = 0.9999;

// Minimum depth sigma to prevent issues at near plane
const float DENOISE_MIN_DEPTH_SIGMA = 0.0001;

//=============================================================================
// UNIFORMS
//=============================================================================

// Textures - binding 0 first, then skip binding 1 for PushConstants
layout(set = 0, binding = 0) uniform sampler2D g_xSSGITex;     // Upsampled SSGI input
layout(set = 0, binding = 2) uniform sampler2D g_xDepthTex;    // Full-res depth
layout(set = 0, binding = 3) uniform sampler2D g_xNormalsTex;  // G-buffer normals
layout(set = 0, binding = 4) uniform sampler2D g_xAlbedoTex;   // G-buffer albedo

// Denoise constants (scratch buffer at binding 1, matches legacy PushConstant API)
layout(std140, set = 0, binding = 1) uniform PushConstants
{
	float u_fSpatialSigma;    // Spatial Gaussian sigma (pixels)
	float u_fDepthSigma;      // Depth threshold (fraction of local depth)
	float u_fNormalSigma;     // Normal threshold (1 - dot product range)
	float u_fAlbedoSigma;     // Albedo threshold (color distance)
	uint u_uKernelRadius;     // Filter radius in pixels
	uint u_bEnabled;          // Enable/disable denoise
	float _pad0;
	float _pad1;
};

//=============================================================================
// INPUT/OUTPUT
//=============================================================================

layout(location = 0) in vec2 a_xUV;
layout(location = 0) out vec4 o_xSSGI;

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

// Gaussian weight function
// Returns exp(-0.5 * (dist/sigma)^2)
float GaussianWeight(float fDist, float fSigma)
{
	float fNorm = fDist / fSigma;
	return exp(-0.5 * fNorm * fNorm);
}

// Get world-space normal from G-buffer
// G-buffer stores normals as raw values in RGB channels (not encoded)
// This matches the convention used in deferred shading and SSGI ray march
vec3 DecodeNormal(vec3 xRaw)
{
	return normalize(xRaw);
}

//=============================================================================
// MAIN
//=============================================================================

void main()
{
	// Bypass if disabled - pass through input unchanged
	if (u_bEnabled == 0u)
	{
		o_xSSGI = texture(g_xSSGITex, a_xUV);
		return;
	}

	// Get texel size for offset calculations
	vec2 xTexelSize = 1.0 / vec2(textureSize(g_xSSGITex, 0));

	// Sample center pixel features
	vec4 xCenterSSGI = texture(g_xSSGITex, a_xUV);
	float fCenterDepth = texture(g_xDepthTex, a_xUV).r;
	vec3 xCenterNormal = DecodeNormal(texture(g_xNormalsTex, a_xUV).rgb);
	vec3 xCenterAlbedo = texture(g_xAlbedoTex, a_xUV).rgb;

	// Early exit for sky pixels - no denoising needed
	if (fCenterDepth >= DENOISE_SKY_THRESHOLD)
	{
		o_xSSGI = xCenterSSGI;
		return;
	}

	// Compute depth-relative sigma
	// This ensures consistent edge detection across depth ranges
	// (nearby objects get tighter threshold, distant objects get looser)
	float fDepthSigmaAbs = max(fCenterDepth * u_fDepthSigma, DENOISE_MIN_DEPTH_SIGMA);

	// Accumulation variables
	vec3 xAccumColor = vec3(0.0);
	float fAccumConfidence = 0.0;
	float fAccumWeight = 0.0;

	// Kernel loop
	int iRadius = int(u_uKernelRadius);

	for (int iY = -iRadius; iY <= iRadius; iY++)
	{
		for (int iX = -iRadius; iX <= iRadius; iX++)
		{
			// Calculate sample UV
			vec2 xOffset = vec2(float(iX), float(iY));
			vec2 xSampleUV = a_xUV + xOffset * xTexelSize;

			// Clamp to valid screen bounds
			xSampleUV = clamp(xSampleUV, vec2(0.001), vec2(0.999));

			// Sample all features at this position
			vec4 xSampleSSGI = texture(g_xSSGITex, xSampleUV);
			float fSampleDepth = texture(g_xDepthTex, xSampleUV).r;
			vec3 xSampleNormal = DecodeNormal(texture(g_xNormalsTex, xSampleUV).rgb);
			vec3 xSampleAlbedo = texture(g_xAlbedoTex, xSampleUV).rgb;

			// Skip sky samples
			if (fSampleDepth >= DENOISE_SKY_THRESHOLD)
			{
				continue;
			}

			//=================================================================
			// SPATIAL WEIGHT
			// Gaussian falloff with distance from center pixel
			//=================================================================
			float fSpatialDist = length(xOffset);
			float fSpatialWeight = GaussianWeight(fSpatialDist, u_fSpatialSigma);

			//=================================================================
			// DEPTH WEIGHT
			// Preserves depth discontinuities (object silhouettes)
			// Uses depth-relative sigma for scale-invariant edge detection
			//=================================================================
			float fDepthDiff = abs(fSampleDepth - fCenterDepth);
			float fDepthWeight = GaussianWeight(fDepthDiff, fDepthSigmaAbs);

			//=================================================================
			// NORMAL WEIGHT
			// Preserves normal discontinuities (surface creases, corners)
			// Uses (1 - dot) as distance metric: 0 = same direction, 2 = opposite
			//=================================================================
			float fNormalDot = max(dot(xCenterNormal, xSampleNormal), 0.0);
			float fNormalDiff = 1.0 - fNormalDot;
			float fNormalWeight = GaussianWeight(fNormalDiff, u_fNormalSigma);

			//=================================================================
			// ALBEDO WEIGHT
			// Preserves material boundaries (different surfaces)
			// Uses Euclidean distance in RGB space
			//=================================================================
			float fAlbedoDiff = length(xSampleAlbedo - xCenterAlbedo);
			float fAlbedoWeight = GaussianWeight(fAlbedoDiff, u_fAlbedoSigma);

			//=================================================================
			// COMBINED WEIGHT
			// Multiplicative combination ensures all edges are respected
			//=================================================================
			float fWeight = fSpatialWeight * fDepthWeight * fNormalWeight * fAlbedoWeight;

			// Accumulate weighted samples
			xAccumColor += xSampleSSGI.rgb * fWeight;
			fAccumConfidence += xSampleSSGI.a * fWeight;
			fAccumWeight += fWeight;
		}
	}

	// Normalize accumulated result
	if (fAccumWeight > DENOISE_MIN_WEIGHT)
	{
		xAccumColor /= fAccumWeight;
		fAccumConfidence /= fAccumWeight;
	}
	else
	{
		// Fallback to center sample if all weights were zero
		// (shouldn't happen in practice, but defensive)
		xAccumColor = xCenterSSGI.rgb;
		fAccumConfidence = xCenterSSGI.a;
	}

	o_xSSGI = vec4(xAccumColor, fAccumConfidence);
}
