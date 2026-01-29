#version 450 core

#include "../Common.fxh"

// ========== SSR RESOLVE CONFIGURATION CONSTANTS ==========
// Roughness thresholds
const float SSR_RESOLVE_SMOOTH_THRESHOLD = 0.15;      // Below this = mirror-like, pass through
const float SSR_RESOLVE_ROUGH_THRESHOLD = 0.7;        // Above this = very rough, pass through to IBL
const float SSR_RESOLVE_ROUGHNESS_RANGE = 0.55;       // Range for remapping (0.7 - 0.15)

// Confidence thresholds
const float SSR_RESOLVE_MIN_CONFIDENCE = 0.001;       // Minimum confidence for blur processing
const float SSR_RESOLVE_MIN_WEIGHT = 0.001;           // Minimum accumulated weight

// Contact-hardening blur
// BLUR RADIUS CALCULATION:
// blurRadius = roughness² * BLUR_SCALE + OFFSET
// At max roughness (remapped=1.0): blurRadius = 48 + 2 = 50 pixels
// At half roughness (remapped=0.5): blurRadius = 12 + 2 = 14 pixels
//
// The kernel samples at wider spacing to cover the full blur radius:
// offsetScale = blurRadius / kernelSize
// So a kernel of 8 with blur of 50 samples at 6.25 pixel spacing
//
// PHYSICAL BASIS: 48.0 approximates GGX reflection cone footprint at 1080p.
// At roughness=1.0, GGX lobe spans ~45° half-angle. For a reflection ray
// traveling 10m, the cone footprint is ~10m * tan(45°) ≈ 10m, which at
// typical FOV and distance projects to approximately 50 pixels.
// Empirically tuned for visual quality; industry range is 32-64 at 1080p.
const float SSR_RESOLVE_BLUR_SCALE = 48.0;            // Base blur radius in pixels at max roughness
const float SSR_RESOLVE_BLUR_OFFSET = 2.0;            // Minimum blur radius (even smooth surfaces get some)
const float SSR_RESOLVE_CONTACT_SCALE_MIN = 0.2;      // Minimum contact scale (near = sharp)
const float SSR_RESOLVE_MIN_BLUR_RADIUS = 1.0;        // Minimum blur for stability

// Kernel parameters
// Max kernel of 4 = 9x9 samples, with depth scale can reach 8 = 17x17 samples
// At max blur (50px) with kernel 8: samples spaced ~6 pixels apart
const float SSR_RESOLVE_MAX_KERNEL_SIZE = 4.0;        // Maximum kernel half-size (9x9 base)
const float SSR_RESOLVE_KERNEL_DIVISOR = 8.0;         // Blur radius to kernel size divisor
const float SSR_RESOLVE_SIGMA_SCALE = 0.5;            // Gaussian sigma to blur ratio

// Depth-adaptive kernel
const float SSR_RESOLVE_DEPTH_SCALE_FAR = 0.1;        // Depth value for max kernel scale (far)
const float SSR_RESOLVE_DEPTH_SCALE_MIN = 0.5;        // Minimum kernel scale at near
const float SSR_RESOLVE_DEPTH_SCALE_MAX = 2.0;        // Maximum kernel scale at far

// Edge detection
const float SSR_RESOLVE_EDGE_GRADIENT_SCALE = 10.0;   // Gradient magnitude to edge factor multiplier

// Bilateral weights
// NUMERICAL STABILITY: Reduced normal powers to preserve blur on curved surfaces (spheres, cylinders)
// Previous powers of 32/24 were too strict - rejected valid samples on naturally curved geometry
const float SSR_RESOLVE_NORMAL_POWER_SMOOTH = 16.0;   // Normal strictness for smooth surfaces (was 32, too strict)
const float SSR_RESOLVE_NORMAL_POWER_ROUGH = 8.0;     // Normal strictness for rough surfaces (was 24, too strict)
// Depth sigma as fraction of center depth - provides consistent behavior across camera far-plane ranges
// Using depth-relative sigma ensures bilateral filter works correctly whether far plane is 100m or 1000m
const float SSR_RESOLVE_DEPTH_SIGMA_FRACTION = 0.02;  // 2% of local depth for bilateral weighting

// Resolution scaling: blur radius is tuned for 1080p, scale proportionally at other resolutions
// This ensures consistent visual blur appearance across 720p, 1080p, 1440p, and 4K
const float SSR_RESOLVE_REFERENCE_HEIGHT = 1080.0;

layout(location = 0) out vec4 o_xReflection;

layout(location = 0) in vec2 a_xUV;

// SSR constants
layout(std140, set = 0, binding = 1) uniform SSRConstants
{
	float u_fIntensity;
	float u_fMaxDistance;
	float u_fMaxRoughness;
	float u_fThickness;
	uint u_uStepCount;
	uint u_uDebugMode;
	uint u_uHiZMipCount;
	uint u_uStartMip;
};

// Textures
layout(set = 0, binding = 2) uniform sampler2D g_xRayMarchTex;
layout(set = 0, binding = 3) uniform sampler2D g_xNormalsTex;
layout(set = 0, binding = 4) uniform sampler2D g_xMaterialTex;
layout(set = 0, binding = 5) uniform sampler2D g_xDepthTex;

void main()
{
	// Read material properties
	vec4 xMaterial = texture(g_xMaterialTex, a_xUV);
	float fRoughness = xMaterial.r;

	// Read center normal for bilateral weighting (stored as float16, already [-1,1] range)
	vec3 xCenterNormal = texture(g_xNormalsTex, a_xUV).rgb;

	// Read center depth for depth-aware bilateral filtering
	float fCenterDepth = texture(g_xDepthTex, a_xUV).r;

	// Read center SSR result - confidence encodes contact hardening
	vec4 xCenterSSR = texture(g_xRayMarchTex, a_xUV);
	float fCenterConfidence = xCenterSSR.a;

	// For smooth surfaces (roughness < threshold), pass through directly - mirror-like
	if (fRoughness < SSR_RESOLVE_SMOOTH_THRESHOLD)
	{
		o_xReflection = xCenterSSR;
		return;
	}

	// For very rough surfaces (roughness > threshold), also pass through directly
	// The ray march shader reduces confidence for rough surfaces, causing
	// the deferred shader to blend toward IBL which handles roughness properly
	if (fRoughness > SSR_RESOLVE_ROUGH_THRESHOLD)
	{
		o_xReflection = xCenterSSR;
		return;
	}

	// No valid hit - pass through
	if (fCenterConfidence < SSR_RESOLVE_MIN_CONFIDENCE)
	{
		o_xReflection = xCenterSSR;
		return;
	}

	// For medium roughness (smooth to rough threshold), apply contact-aware blur
	// Remap roughness from [threshold, threshold] to [0, 1] for blur calculation
	float fRemappedRoughness = (fRoughness - SSR_RESOLVE_SMOOTH_THRESHOLD) / SSR_RESOLVE_ROUGHNESS_RANGE;

	// ========== AAA CONTACT HARDENING ==========
	// Confidence encodes contact distance (high = near contact, low = far)
	// Scale blur radius based on confidence: near contact = sharp, far = blurry
	// This matches Unreal/Unity's contact-hardening SSR
	float fContactFactor = 1.0 - clamp(fCenterConfidence, 0.0, 1.0);  // 0 = near, 1 = far

	// Resolution scaling: scale blur proportionally to maintain consistent visual appearance
	// At 4K (2160p), blur radius is 2x the base; at 720p, blur radius is 0.667x
	float fResolutionScale = g_xScreenDims.y / SSR_RESOLVE_REFERENCE_HEIGHT;
	float fScaledBlurScale = SSR_RESOLVE_BLUR_SCALE * fResolutionScale;

	// Base blur radius from roughness (resolution-scaled)
	float fRoughnessBlur = fRemappedRoughness * fRemappedRoughness * fScaledBlurScale + SSR_RESOLVE_BLUR_OFFSET;

	// Contact hardening multiplier: near contact gets minimal blur
	// Far reflections get full roughness-based blur
	float fContactScale = mix(SSR_RESOLVE_CONTACT_SCALE_MIN, 1.0, fContactFactor);

	// Final blur radius combines roughness and contact distance
	float fBlurRadiusPixels = fRoughnessBlur * fContactScale;

	// Minimum blur for stability (smooth surfaces still get some filtering)
	fBlurRadiusPixels = max(fBlurRadiusPixels, SSR_RESOLVE_MIN_BLUR_RADIUS);

	// Dynamic kernel size based on blur radius (up to 9x9)
	int iKernelSize = int(min(SSR_RESOLVE_MAX_KERNEL_SIZE, ceil(fBlurRadiusPixels / SSR_RESOLVE_KERNEL_DIVISOR)));

	// ========== DEPTH-ADAPTIVE KERNEL SCALING ==========
	// Distant surfaces get larger kernels to compensate for perspective
	// Near surfaces get smaller kernels for sharper detail
	float fDepthScale = mix(SSR_RESOLVE_DEPTH_SCALE_MIN, SSR_RESOLVE_DEPTH_SCALE_MAX,
		smoothstep(0.0, SSR_RESOLVE_DEPTH_SCALE_FAR, fCenterDepth));
	iKernelSize = int(clamp(float(iKernelSize) * fDepthScale, 1.0, SSR_RESOLVE_MAX_KERNEL_SIZE * SSR_RESOLVE_DEPTH_SCALE_MAX));

	// ========== GRADIENT-BASED EDGE DETECTION ==========
	// Detect edges using normal gradient - increase strictness at edges
	vec3 xGradientX = dFdx(xCenterNormal);
	vec3 xGradientY = dFdy(xCenterNormal);
	float fGradientMag = length(xGradientX) + length(xGradientY);
	float fEdgeFactor = 1.0 + fGradientMag * SSR_RESOLVE_EDGE_GRADIENT_SCALE;

	// Bilateral blur
	vec4 xAccum = vec4(0.0);
	float fTotalWeight = 0.0;

	// Gaussian sigma scales with blur radius
	float fSigma = max(fBlurRadiusPixels * SSR_RESOLVE_SIGMA_SCALE, SSR_RESOLVE_MIN_BLUR_RADIUS);
	float fSigmaSq2 = 2.0 * fSigma * fSigma;

	for (int y = -iKernelSize; y <= iKernelSize; y++)
	{
		for (int x = -iKernelSize; x <= iKernelSize; x++)
		{
			// Scale offset so kernel covers the blur radius
			float fOffsetScale = fBlurRadiusPixels / float(iKernelSize);
			vec2 xOffset = vec2(float(x), float(y)) * g_xRcpScreenDims * fOffsetScale;
			vec2 xSampleUV = a_xUV + xOffset;

			// Clamp to screen bounds
			xSampleUV = clamp(xSampleUV, vec2(0.0), vec2(1.0));

			vec4 xSample = texture(g_xRayMarchTex, xSampleUV);
			vec3 xSampleNormal = texture(g_xNormalsTex, xSampleUV).rgb;
			float fSampleDepth = texture(g_xDepthTex, xSampleUV).r;

			// Spatial weight (Gaussian falloff)
			float fDistSq = float(x * x + y * y);
			float fSpatialWeight = exp(-fDistSq / (fSigmaSq2 + 0.001));

			// Normal weight (bilateral - preserve edges)
			// Relax normal strictness slightly for rougher surfaces
			// Apply edge factor to increase strictness at detected edges
			float fNormalSimilarity = max(dot(xCenterNormal, xSampleNormal), 0.0);
			float fNormalPower = mix(SSR_RESOLVE_NORMAL_POWER_SMOOTH, SSR_RESOLVE_NORMAL_POWER_ROUGH, fRemappedRoughness);
			fNormalPower *= fEdgeFactor;  // Increase strictness at edges
			float fNormalWeight = pow(fNormalSimilarity, fNormalPower);

			// Depth weight (bilateral - preserve depth discontinuities)
			// Reject samples at significantly different depths
			// NUMERICAL STABILITY: Using depth-relative sigma for consistent behavior across camera ranges
			float fDepthDiff = abs(fSampleDepth - fCenterDepth);
			float fDepthSigma = max(fCenterDepth * SSR_RESOLVE_DEPTH_SIGMA_FRACTION, 0.001);  // Depth-relative, min 0.001
			float fDepthWeight = exp(-fDepthDiff * fDepthDiff / (fDepthSigma * fDepthSigma));

			// Confidence weight (weight by hit confidence)
			float fConfidenceWeight = xSample.a;

			float fWeight = fSpatialWeight * fNormalWeight * fDepthWeight * fConfidenceWeight;

			xAccum += xSample * fWeight;
			fTotalWeight += fWeight;
		}
	}

	if (fTotalWeight > SSR_RESOLVE_MIN_WEIGHT)
	{
		o_xReflection = xAccum / fTotalWeight;
	}
	else
	{
		o_xReflection = texture(g_xRayMarchTex, a_xUV);
	}
}
