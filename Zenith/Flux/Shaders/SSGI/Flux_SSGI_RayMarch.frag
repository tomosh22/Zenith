#version 450 core

#include "../Common.fxh"
#include "../PBRConstants.fxh"

// ========== SSGI CONFIGURATION CONSTANTS ==========
// Blue noise sampling: Uses BLUE_NOISE_SIZE and GOLDEN_RATIO from Common.fxh

// Normal and direction thresholds
const float SSGI_NORMAL_UP_THRESHOLD = 0.999;
// Note: Direction epsilon now uses PBR_DIRECTION_EPSILON from PBRConstants.fxh

// Ray start offset (prevents self-intersection)
const float SSGI_START_OFFSET_MIN = 0.1;
const float SSGI_START_OFFSET_SCALE = 0.01;
const float SSGI_MIN_SCREEN_TRACE_DIST = 0.01;

// Screen-space ray marching
const float SSGI_MAX_SCREEN_DIST = 0.6;

// HiZ traversal
const float SSGI_SKY_DEPTH_THRESHOLD = 0.9999;
const float SSGI_STEP_SIZE_MULTIPLIER = 2.0;
const int SSGI_BINARY_SEARCH_ITERATIONS = 4;
const float SSGI_BINARY_SEARCH_TOLERANCE = 0.001;

// Roughness-based mip clamping (consistency with SSR)
// For rough surfaces, don't refine all the way to mip 0
// Maps roughness [0,1] to minimum refinement mip level
// Diffuse GI is inherently broader than specular, so use lower scale than SSR (2.5)
const float SSGI_ROUGHNESS_MIP_SCALE = 2.0;
const int SSGI_MAX_ROUGHNESS_MIP = 2;  // Cap at mip 2 (diffuse doesn't need mip 3)

// Hit validation
const float SSGI_BACKFACE_THRESHOLD = 0.1;
const float SSGI_DEPTH_VALID_MIN = 0.1;

// Confidence calculation
const float SSGI_EDGE_MARGIN = 0.15;
const float SSGI_BACKFACE_SMOOTHSTEP_MIN = 0.1;
const float SSGI_BACKFACE_SMOOTHSTEP_MAX = -0.3;
// Reduced from 0.05 to minimize invalid hit glow while maintaining temporal stability
const float SSGI_MIN_CONFIDENCE = 0.02;
const float SSGI_DISTANCE_FADE_START = 0.4;
const float SSGI_DISTANCE_FADE_END = 0.9;

// Horizon occlusion: fades GI when ray direction is nearly parallel to surface
// Less critical for SSGI than SSR (cosine-weighted sampling already favors upward rays)
// but still helps prevent artifacts from grazing-angle rays
const float SSGI_HORIZON_FADE_SCALE = 5.0;

layout(location = 0) out vec4 o_xSSGI;  // RGB = indirect color, A = confidence

layout(location = 0) in vec2 a_xUV;

// SSGI constants
layout(std140, set = 0, binding = 1) uniform SSGIConstants
{
	float u_fIntensity;
	float u_fMaxDistance;
	float u_fThickness;
	uint u_uStepCount;
	uint u_uFrameIndex;
	uint u_uHiZMipCount;
	uint u_uDebugMode;
	float u_fRoughnessThreshold;
	uint u_uStartMip;
	uint u_uRaysPerPixel;       // Number of hemisphere samples per pixel (1-8, default 1)
	float _pad0;
	float _pad1;
};

// Textures
layout(set = 0, binding = 2) uniform sampler2D g_xDepthTex;
layout(set = 0, binding = 3) uniform sampler2D g_xNormalsTex;
layout(set = 0, binding = 4) uniform sampler2D g_xMaterialTex;
layout(set = 0, binding = 5) uniform sampler2D g_xHiZTex;
layout(set = 0, binding = 6) uniform sampler2D g_xDiffuseTex;
layout(set = 0, binding = 7) uniform sampler2D g_xBlueNoiseTex;

// Debug mode constants
const uint SSGI_DEBUG_NONE = 0u;
const uint SSGI_DEBUG_RAY_DIRECTIONS = 1u;
const uint SSGI_DEBUG_HIT_POSITIONS = 2u;
const uint SSGI_DEBUG_CONFIDENCE = 3u;
const uint SSGI_DEBUG_TEMPORAL_WEIGHT = 4u;
const uint SSGI_DEBUG_FINAL_RESULT = 5u;

// Note: GetViewPosFromDepth, DepthToViewZ, ViewToScreen, ComputeEdgeFade
// are now centralized in Common.fxh for consistency across SSR/SSGI

// Sample blue noise with frame-based temporal offset
vec2 GetBlueNoise(ivec2 xPixelCoord, uint uFrameIndex)
{
	vec2 xNoiseUV = vec2(xPixelCoord % ivec2(BLUE_NOISE_SIZE)) / float(BLUE_NOISE_SIZE);
	vec2 xNoise = texture(g_xBlueNoiseTex, xNoiseUV).rg;
	xNoise = fract(xNoise + float(uFrameIndex % 64) * GOLDEN_RATIO);
	return xNoise;
}

// Build orthonormal basis using Frisvad's method
// This provides continuous tangent frames without the left/right orientation flip
// that occurs with the cross-product method using a fixed up vector.
// Reference: Frisvad, "Building an Orthonormal Basis from a 3D Unit Vector Without Normalization"
void BuildOrthonormalBasisFrisvad(vec3 n, out vec3 b1, out vec3 b2)
{
	if (n.z < -0.9999999)
	{
		// Handle the singularity at n = (0, 0, -1)
		b1 = vec3(0.0, -1.0, 0.0);
		b2 = vec3(-1.0, 0.0, 0.0);
	}
	else
	{
		float a = 1.0 / (1.0 + n.z);
		float b = -n.x * n.y * a;
		b1 = vec3(1.0 - n.x * n.x * a, b, -n.x);
		b2 = vec3(b, 1.0 - n.y * n.y * a, -n.y);
	}
}

// Cosine-weighted hemisphere sampling for diffuse GI
// This is importance sampling for Lambertian BRDF: PDF = cos(theta) / PI
vec3 SampleHemisphereCosine(vec2 Xi, vec3 N)
{
	// Cosine-weighted hemisphere sampling
	// Maps uniform [0,1]^2 to hemisphere with cosine-weighted distribution
	float fCosTheta = sqrt(1.0 - Xi.x);  // cos(theta) = sqrt(1 - u1)
	float fSinTheta = sqrt(Xi.x);         // sin(theta) = sqrt(u1)
	float fPhi = TWO_PI * Xi.y;

	// Spherical to cartesian (tangent space)
	vec3 H;
	H.x = fSinTheta * cos(fPhi);
	H.y = fSinTheta * sin(fPhi);
	H.z = fCosTheta;

	// Build tangent-space basis using Frisvad's method
	// This avoids the orientation flip between left/right facing surfaces
	// that occurs with cross(up, N) where up is a fixed vector
	vec3 tangent, bitangent;
	BuildOrthonormalBasisFrisvad(N, tangent, bitangent);

	// Transform from tangent space to world/view space
	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// Hierarchical HiZ ray march for SSGI
// fRoughness is used for roughness-based mip clamping (matches SSR's cone tracing approach)
vec4 RayMarch(vec3 xViewOrigin, vec3 xRayDir, vec3 xViewNormal, float fRoughness, out float fMarchDistance, out vec2 xScreenDir)
{
	fMarchDistance = 0.0;
	xScreenDir = vec2(0.0);

	// Add offset along ray direction to avoid self-intersection
	float fStartOffset = max(SSGI_START_OFFSET_MIN, abs(xViewOrigin.z) * SSGI_START_OFFSET_SCALE);
	vec3 xViewStart = xViewOrigin + xRayDir * fStartOffset;

	// Project start position to screen
	vec3 xScreenStart = ViewToScreen(xViewStart);

	// Compute screen-space trace direction
	float fRaySampleDist = min(u_fMaxDistance * 0.5, xViewStart.z * 0.5);
	vec3 xRaySampleView = xViewStart + xRayDir * fRaySampleDist;

	// Clamp to prevent near-plane issues
	float fNearPlane = g_xCameraNearFar.x;
	if (xRaySampleView.z < fNearPlane)
	{
		if (abs(xRayDir.z) > PBR_DIRECTION_EPSILON)
		{
			float fSafeT = (fNearPlane - xViewStart.z) / xRayDir.z * 0.9;
			if (fSafeT > 0.0)
				xRaySampleView = xViewStart + xRayDir * fSafeT;
			else
				xRaySampleView = xViewStart + xRayDir * 0.1;
		}
		else
		{
			xRaySampleView = xViewStart + xRayDir * 0.1;
		}
	}

	vec3 xRaySampleScreen = ViewToScreen(xRaySampleView);
	vec2 xScreenDelta = xRaySampleScreen.xy - xScreenStart.xy;
	float fDeltaLen = length(xScreenDelta);

	if (fDeltaLen > 0.001)
	{
		xScreenDir = xScreenDelta / fDeltaLen;
	}
	else
	{
		// Fallback for degenerate screen direction (ray going nearly straight into/out of screen)
		// Preserve the sign of non-zero ray components to avoid directional bias
		vec2 xDir = vec2(xRayDir.x, xRayDir.y);
		vec2 xEps = vec2(PBR_DIRECTION_EPSILON);
		if (xDir.x < 0.0) xEps.x = -PBR_DIRECTION_EPSILON;
		if (xDir.y < 0.0) xEps.y = -PBR_DIRECTION_EPSILON;
		xScreenDir = normalize(xDir + xEps);
	}

	// Determine max screen distance to trace
	float fMaxScreenDist = SSGI_MAX_SCREEN_DIST;

	// Calculate where the ray would exit the screen
	vec2 xExitDist = vec2(1e10);
	if (abs(xScreenDir.x) > PBR_DIRECTION_EPSILON)
	{
		xExitDist.x = xScreenDir.x > 0.0 ? (1.0 - xScreenStart.x) / xScreenDir.x : -xScreenStart.x / xScreenDir.x;
	}
	if (abs(xScreenDir.y) > PBR_DIRECTION_EPSILON)
	{
		xExitDist.y = xScreenDir.y > 0.0 ? (1.0 - xScreenStart.y) / xScreenDir.y : -xScreenStart.y / xScreenDir.y;
	}
	fMaxScreenDist = min(fMaxScreenDist, min(xExitDist.x, xExitDist.y));
	fMaxScreenDist = max(fMaxScreenDist, SSGI_MIN_SCREEN_TRACE_DIST);

	// Depth interpolation setup
	float fSafeStartZ = max(xViewStart.z, g_xCameraNearFar.x);
	float fInvZStart = 1.0 / fSafeStartZ;
	float fEstimatedRayDist = u_fMaxDistance * fMaxScreenDist;
	float fEstimatedEndZ = fSafeStartZ + xRayDir.z * fEstimatedRayDist;
	fEstimatedEndZ = max(g_xCameraNearFar.x, fEstimatedEndZ);
	float fInvZEnd = 1.0 / fEstimatedEndZ;

	// Screen dimensions for texel size
	vec2 xScreenSize = textureSize(g_xHiZTex, 0);
	vec2 xTexelSize = 1.0 / xScreenSize;

	// Hierarchical traversal state
	int iCurrentMip = int(min(u_uStartMip, u_uHiZMipCount - 1u));
	float fT = 0.0;
	float fPrevT = 0.0;
	float fPrevDepthDiff = 0.0;
	bool bFirstSample = true;

	// ========== ROUGHNESS-BASED CONE TRACING ==========
	// Match SSR approach: rough surfaces sample coarser mips
	// Diffuse GI is inherently broader, so we use a lower scale (2.0 vs 2.5)
	int iMinMip = int(floor(fRoughness * SSGI_ROUGHNESS_MIP_SCALE));
	iMinMip = clamp(iMinMip, 0, SSGI_MAX_ROUGHNESS_MIP);

	// Ray march loop
	for (uint uIter = 0u; uIter < u_uStepCount; uIter++)
	{
		vec2 xSampleUV = xScreenStart.xy + xScreenDir * fT * fMaxScreenDist;

		// Bounds check
		if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
			xSampleUV.y < 0.0 || xSampleUV.y > 1.0)
		{
			break;
		}

		// Sample HiZ
		ivec2 xMipSize = textureSize(g_xHiZTex, iCurrentMip);
		vec2 xHalfTexel = 0.5 / vec2(xMipSize);
		vec2 xClampedUV = clamp(xSampleUV, xHalfTexel, 1.0 - xHalfTexel);
		vec2 xHiZSample = textureLod(g_xHiZTex, xClampedUV, float(iCurrentMip)).rg;
		float fMinDepth = xHiZSample.r;

		// Skip sky pixels
		if (fMinDepth >= SSGI_SKY_DEPTH_THRESHOLD)
		{
			float fMipScale = exp2(float(iCurrentMip));
			float fStepSize = fMipScale * max(xTexelSize.x, xTexelSize.y) * SSGI_STEP_SIZE_MULTIPLIER;
			fPrevT = fT;
			fT += fStepSize / fMaxScreenDist;

			if (fT >= 1.0)
				break;
			continue;
		}

		// Convert depth to view-space Z
		float fMinViewZ = DepthToViewZ(xSampleUV, fMinDepth);

		// Perspective-correct ray depth
		float fInvZ = mix(fInvZStart, fInvZEnd, fT);
		float fRayViewZ = 1.0 / fInvZ;

		float fDepthDiff = fRayViewZ - fMinViewZ;

		// Adaptive thickness
		float fMipThickness = u_fThickness * exp2(float(iCurrentMip));

		bool bBehindSurface = fDepthDiff > 0.0;
		bool bWithinThickness = fDepthDiff < fMipThickness * SSGI_STEP_SIZE_MULTIPLIER;
		bool bValidDepth = fMinViewZ > SSGI_DEPTH_VALID_MIN;

		if (bBehindSurface && bValidDepth)
		{
			if (iCurrentMip <= iMinMip)
			{
				// At minimum mip for this roughness level - potential hit
				if (bWithinThickness || (!bFirstSample && fPrevDepthDiff * fDepthDiff < 0.0))
				{
					// Binary search refinement
					float fSearchStart = fPrevT;
					float fSearchEnd = fT;

					for (int j = 0; j < SSGI_BINARY_SEARCH_ITERATIONS; j++)
					{
						if (abs(fSearchEnd - fSearchStart) < SSGI_BINARY_SEARCH_TOLERANCE)
							break;

						float fMidT = (fSearchStart + fSearchEnd) * 0.5;

						vec2 xMidUV = xScreenStart.xy + xScreenDir * fMidT * fMaxScreenDist;
						float fMidSceneDepth = textureLod(g_xHiZTex, xMidUV, float(iMinMip)).r;
						float fMidSceneZ = DepthToViewZ(xMidUV, fMidSceneDepth);

						float fMidInvZ = mix(fInvZStart, fInvZEnd, fMidT);
						float fMidRayZ = 1.0 / fMidInvZ;
						float fMidDiff = fMidRayZ - fMidSceneZ;

						if (fMidDiff > 0.0)
							fSearchEnd = fMidT;
						else
							fSearchStart = fMidT;
					}

					// Final hit position (sample at minimum mip for roughness-based cone tracing)
					float fFinalT = (fSearchStart + fSearchEnd) * 0.5;
					vec2 xFinalUV = xScreenStart.xy + xScreenDir * fFinalT * fMaxScreenDist;
					float fFinalSceneDepth = textureLod(g_xHiZTex, xFinalUV, float(iMinMip)).r;
					float fFinalSceneZ = DepthToViewZ(xFinalUV, fFinalSceneDepth);

					// Backface detection
					vec3 xHitWorldNormal = texture(g_xNormalsTex, xFinalUV).rgb;
					vec3 xHitViewNormal = normalize(mat3(g_xViewMat) * xHitWorldNormal);

					float fBackfaceDot = dot(xHitViewNormal, xRayDir);
					if (fBackfaceDot > SSGI_BACKFACE_THRESHOLD)
					{
						// Backface hit - continue marching
						fPrevT = fT;
						float fStepSize = max(xTexelSize.x, xTexelSize.y) * SSGI_STEP_SIZE_MULTIPLIER;
						fT += fStepSize / fMaxScreenDist;
						bFirstSample = false;
						continue;
					}

					// Confidence calculation
					float fEdgeFade = ComputeEdgeFade(xFinalUV, SSGI_EDGE_MARGIN);
					float fDistanceFade = 1.0 - smoothstep(SSGI_DISTANCE_FADE_START, SSGI_DISTANCE_FADE_END, fFinalT);

					float fFinalInvZ = mix(fInvZStart, fInvZEnd, fFinalT);
					float fFinalRayZ = 1.0 / fFinalInvZ;
					float fDepthError = abs(fFinalRayZ - fFinalSceneZ);
					float fDepthConfidence = 1.0 - smoothstep(0.0, u_fThickness * SSGI_STEP_SIZE_MULTIPLIER, fDepthError);

					float fBackfaceConfidence = smoothstep(SSGI_BACKFACE_SMOOTHSTEP_MIN, SSGI_BACKFACE_SMOOTHSTEP_MAX, fBackfaceDot);

					// Horizon occlusion: fade rays nearly parallel to surface
					// NdotR = dot(normal, rayDir): positive for valid hemisphere samples
					float fNdotR = max(dot(xViewNormal, xRayDir), 0.0);
					float fHorizonFade = clamp(fNdotR * SSGI_HORIZON_FADE_SCALE, 0.0, 1.0);

					float fConfidence = fEdgeFade * fDistanceFade * fDepthConfidence * fBackfaceConfidence * fHorizonFade;
					fConfidence = max(fConfidence, SSGI_MIN_CONFIDENCE);

					fMarchDistance = fFinalT;
					return vec4(xFinalUV, fFinalSceneDepth, fConfidence);
				}
				else
				{
					// Behind surface but outside thickness
					fT = fPrevT;
					float fStepSize = max(xTexelSize.x, xTexelSize.y) * 0.5;
					fPrevT = fT;
					fT += fStepSize / fMaxScreenDist;
				}
			}
			else
			{
				// Refine to finer mip
				fT = max(0.0, fT - exp2(float(iCurrentMip - 1)) * max(xTexelSize.x, xTexelSize.y) / fMaxScreenDist);
				iCurrentMip--;
			}
		}
		else
		{
			// Step forward
			float fMipScale = exp2(float(iCurrentMip));
			float fStepSize = fMipScale * max(xTexelSize.x, xTexelSize.y) * SSGI_STEP_SIZE_MULTIPLIER;

			fPrevDepthDiff = fDepthDiff;
			fPrevT = fT;
			fT += fStepSize / fMaxScreenDist;

			if (fT >= 1.0)
				break;
		}

		bFirstSample = false;
	}

	return vec4(0.0);
}

void main()
{
	float fDepth = texture(g_xDepthTex, a_xUV).r;

	// Early exit for sky
	if (fDepth >= 1.0)
	{
		o_xSSGI = vec4(0.0);
		return;
	}

	// Read G-Buffer
	vec3 xWorldNormal = texture(g_xNormalsTex, a_xUV).rgb;
	vec4 xMaterial = texture(g_xMaterialTex, a_xUV);
	float fRoughness = xMaterial.r;

	// Optional: skip very smooth surfaces (SSR handles them better)
	if (fRoughness < u_fRoughnessThreshold)
	{
		o_xSSGI = vec4(0.0);
		return;
	}

	// Reconstruct view-space position
	vec3 xViewPos = GetViewPosFromDepth(a_xUV, fDepth);
	vec3 xViewNormal = normalize(mat3(g_xViewMat) * xWorldNormal);

	ivec2 xPixelCoord = ivec2(gl_FragCoord.xy);

	// Debug: ray directions (show first ray only)
	if (u_uDebugMode == SSGI_DEBUG_RAY_DIRECTIONS)
	{
		vec2 xNoise = GetBlueNoise(xPixelCoord, u_uFrameIndex);
		vec3 xSampleDir = SampleHemisphereCosine(xNoise, xViewNormal);
		o_xSSGI = vec4(xSampleDir * 0.5 + 0.5, 1.0);
		return;
	}

	// ========== MULTI-RAY SAMPLING ==========
	// Sample multiple rays per pixel for higher quality (configurable via u_uRaysPerPixel)
	// Each ray uses a different noise offset based on golden ratio for good distribution
	vec3 xAccumulatedColor = vec3(0.0);
	float fAccumulatedConfidence = 0.0;
	uint uValidHits = 0u;

	uint uRayCount = max(u_uRaysPerPixel, 1u);  // Ensure at least 1 ray
	for (uint uRay = 0u; uRay < uRayCount; uRay++)
	{
		// Generate hemisphere sample direction using blue noise
		// Offset noise for each ray using golden ratio for low-discrepancy distribution
		vec2 xNoise = GetBlueNoise(xPixelCoord, u_uFrameIndex);
		xNoise = fract(xNoise + float(uRay) * GOLDEN_RATIO);

		// Sample direction in hemisphere around normal (cosine-weighted for Lambertian)
		vec3 xSampleDir = SampleHemisphereCosine(xNoise, xViewNormal);

		// Perform ray march (now with roughness for mip clamping)
		float fMarchDistance = 0.0;
		vec2 xScreenDir = vec2(0.0);
		vec4 xHitResult = RayMarch(xViewPos, xSampleDir, xViewNormal, fRoughness, fMarchDistance, xScreenDir);

		if (xHitResult.w > 0.0)
		{
			// Sample color at hit position
			vec3 xIndirectColor = texture(g_xDiffuseTex, xHitResult.xy).rgb;

			// Accumulate weighted by confidence
			xAccumulatedColor += xIndirectColor * xHitResult.w;
			fAccumulatedConfidence += xHitResult.w;
			uValidHits++;
		}
	}

	// Debug: confidence (show average confidence of valid hits)
	if (u_uDebugMode == SSGI_DEBUG_CONFIDENCE)
	{
		float fAvgConfidence = (uValidHits > 0u) ? (fAccumulatedConfidence / float(uValidHits)) : 0.0;
		o_xSSGI = vec4(vec3(fAvgConfidence), 1.0);
		return;
	}

	// Debug: hit positions (show first valid hit only - requires re-marching)
	if (u_uDebugMode == SSGI_DEBUG_HIT_POSITIONS && uValidHits > 0u)
	{
		vec2 xNoise = GetBlueNoise(xPixelCoord, u_uFrameIndex);
		vec3 xSampleDir = SampleHemisphereCosine(xNoise, xViewNormal);
		float fMarchDistance = 0.0;
		vec2 xScreenDir = vec2(0.0);
		vec4 xHitResult = RayMarch(xViewPos, xSampleDir, xViewNormal, fRoughness, fMarchDistance, xScreenDir);
		if (xHitResult.w > 0.0)
		{
			vec3 xHitWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, xHitResult.xy);
			o_xSSGI = vec4(xHitWorldPos / 100.0, 1.0);
			return;
		}
	}

	// Compute final averaged result
	if (uValidHits > 0u)
	{
		// Confidence-weighted average of hit colors (stable, consistent result)
		vec3 xFinalColor = xAccumulatedColor / fAccumulatedConfidence;

		// Apply intensity
		xFinalColor *= u_fIntensity;

		// Confidence is average quality of valid hits only (not diluted by ray count)
		// This ensures consistent GI blending regardless of rays per pixel setting
		float fFinalConfidence = fAccumulatedConfidence / float(uValidHits);

		// Note: For diffuse GI, no Fresnel weighting needed
		// The cosine-weighted hemisphere sampling already importance-samples the Lambertian BRDF

		o_xSSGI = vec4(xFinalColor, fFinalConfidence);
	}
	else
	{
		o_xSSGI = vec4(0.0);
	}
}
