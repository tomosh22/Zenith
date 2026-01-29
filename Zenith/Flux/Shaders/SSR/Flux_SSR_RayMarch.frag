#version 450 core

#include "../Common.fxh"
#include "../PBRConstants.fxh"

// ========== SSR CONFIGURATION CONSTANTS ==========
// Blue noise sampling
const int SSR_BLUE_NOISE_SIZE = 64;                    // Blue noise texture dimensions
const float SSR_GOLDEN_RATIO = 0.618033988749;         // Golden ratio for temporal offset

// Normal and direction thresholds
const float SSR_NORMAL_UP_THRESHOLD = 0.999;           // Threshold for up vector selection
const float SSR_DIRECTION_EPSILON = 0.0001;            // Minimum direction magnitude

// Ray start offset (prevents self-intersection)
// Minimum offset ensures we step past the origin surface to avoid self-reflection
// Depth-proportional scale provides consistent behavior across view distances
const float SSR_START_OFFSET_MIN = 0.1;                // Minimum ray start offset (world units)
const float SSR_START_OFFSET_SCALE = 0.01;             // Depth-proportional offset scale
const float SSR_MIN_SCREEN_TRACE_DIST = 0.01;          // Minimum screen-space trace (1% of screen)

// Screen-space ray marching
const float SSR_MAX_SCREEN_DIST_BASE = 0.8;            // Base maximum screen distance to trace
// PHYSICAL ACCURACY FIX: Roughness should NOT reduce trace distance.
// Rough surfaces should trace the same distance but sample at coarser mip levels (cone tracing).
// The roughness-based mip selection (SSR_ROUGHNESS_MIP_SCALE) already handles the reflection cone.
// Previous value of 0.6 caused rough surfaces to miss distant reflections.
const float SSR_ROUGHNESS_DIST_SCALE = 0.0;            // Disabled - rough surfaces trace full distance
// DEPRECATED: Grazing angle no longer affects trace distance (Fresnel physics fix)
// const float SSR_GRAZING_FADE_THRESHOLD = 0.4;

// HiZ traversal
const float SSR_SKY_DEPTH_THRESHOLD = 0.9999;          // Depth value considered sky
const float SSR_STEP_SIZE_MULTIPLIER = 2.0;            // Step size scaling factor
// Binary search refinement iterations for sub-pixel hit precision
// Each iteration halves the search range: 6 iterations = 1/64 precision
// Resolution guidelines:
//   6 iterations (1/64)  - suitable for 1080p-1440p
//   8 iterations (1/256) - recommended for 4K (3840x2160)
// Cost: ~1 additional texture sample per iteration
const int SSR_BINARY_SEARCH_ITERATIONS = 6;
const float SSR_BINARY_SEARCH_RELATIVE_TOLERANCE = 0.0001;  // Relative tolerance (0.01% of ray length)
const float SSR_BINARY_SEARCH_MIN_TOLERANCE = 0.0001;       // Absolute minimum tolerance

// Roughness-based cone tracing
// Maps roughness [0,1] to minimum refinement mip level based on GGX cone footprint.
// At roughness=1.0, samples mip 2-3 (~4-8 pixel area) matching the reflection cone.
// Industry standard range is 2-4x; we use 2.5 as a balanced default.
const float SSR_ROUGHNESS_MIP_SCALE = 2.5;
const int SSR_MAX_ROUGHNESS_MIP = 3;                   // Maximum mip level for cone tracing

// Hit validation
// Backface rejection: reject hits where surface normal faces similar direction as ray.
// dot(hitNormal, rayDir) > threshold means surface is facing away from camera at hit point.
// Lower values = stricter rejection, fewer artifacts from near-parallel surfaces.
// Value mapping: 0.1 ≈ 84° (industry standard), 0.05 ≈ 87°, 0.03 ≈ 88° (strict), 0.02 ≈ 89° (very strict)
// Previous 0.03 was too strict - rejected valid grazing-angle reflections
// UE4/Unity use ~0.1 which balances artifact rejection with visual quality
const float SSR_BACKFACE_THRESHOLD = 0.1;              // ~84 degrees - industry standard
const float SSR_DEPTH_VALID_MIN = 0.1;                 // Minimum valid view-space depth

// Confidence calculation
const float SSR_EDGE_MARGIN_BASE = 0.15;               // Base screen edge fade margin
const float SSR_EDGE_MARGIN_ROUGHNESS = 0.10;          // Additional margin for rough surfaces
const float SSR_BACKFACE_SMOOTHSTEP_MIN = 0.1;         // Backface confidence smoothstep start
const float SSR_BACKFACE_SMOOTHSTEP_MAX = -0.3;        // Backface confidence smoothstep end
// PHYSICAL ACCURACY FIX: Replace screen-space vertical penalty with physics-based metric.
// Previous implementation penalized vertical screen travel, unfairly affecting floor reflections.
// New approach: penalize when reflection ray is nearly parallel to surface (actual stretching cause).
// DEPRECATED: Stretch penalty removed - correlated with viewing angle which
// fights against Fresnel physics at grazing angles (Fresnel physics fix)
// const float SSR_STRETCH_DOT_MIN = 0.05;
// const float SSR_STRETCH_DOT_MAX = 0.3;
const float SSR_MIN_CONFIDENCE = 0.1;                  // Minimum confidence value

// Unified distance fade (replaces double contact hardening)
// Single smoothstep provides better control than stacked ray fade + contact hardening
const float SSR_DISTANCE_FADE_START = 0.3;             // Start fading at 30% of max trace distance
const float SSR_DISTANCE_FADE_END = 0.8;               // Full fade by 80% of max trace distance

// ENERGY CONSERVATION FIX: Fresnel distance compensation DISABLED.
// The deferred shader applies FresnelSchlickRoughness() to SSR output, which is the
// correct location per industry standard (FidelityFX SSSR, UE4, Unity HDRP).
// Applying Fresnel here AND in deferred shader caused double-weighting at grazing angles,
// violating energy conservation (reflections > 100% physically possible energy).
// Distance fade is now purely geometric, not Fresnel-compensated.
const float SSR_FRESNEL_DISTANCE_COMPENSATION = 0.0;  // DISABLED for energy conservation

// ========== CONTACT HARDENING (AAA FEATURE) ==========
// Controls sharp-to-blur transition based on world-space reflection travel distance.
// Reflections are crisp at contact points and progressively blur with distance,
// mimicking how real surfaces show sharp reflections of nearby objects but
// blurry reflections of distant objects (even on smooth surfaces).
//
// SCENE SCALE GUIDANCE (assuming 1 unit = 1 meter):
// +-----------------------+------------------+-------------------------------------+
// | Scene Type            | Recommended (m)  | Example                             |
// +-----------------------+------------------+-------------------------------------+
// | Interior (human-scale)| 1.0 - 3.0        | Floor reflects nearby furniture     |
// | Exterior (landscape)  | 3.0 - 10.0       | Water reflects nearby dock          |
// | Miniature/Tabletop    | 0.02 - 0.05      | Model train reflects tiny buildings |
// | Giant/Architectural   | 20.0 - 50.0      | City plaza reflects skyscrapers     |
// +-----------------------+------------------+-------------------------------------+
//
// Configurable at runtime via u_fContactHardeningDist uniform.
// Access in editor: Flux/SSR/ContactHardeningDist
// Default: 2.0m (balanced for typical first-person human-scale environments)
const float SSR_CONTACT_ROUGHNESS_SCALE_MAX = 2.0;     // Max roughness scaling for contact
const float SSR_CONTACT_CONFIDENCE_SCALE = 0.6;        // How much contact distance reduces confidence

// Roughness confidence falloff
const float SSR_ROUGHNESS_CONF_START = 0.15;           // Roughness where confidence starts fading
const float SSR_ROUGHNESS_CONF_END = 0.7;              // Roughness where confidence reaches minimum
// ENERGY CONSERVATION FIX: Metallic boost disabled.
// The deferred shader applies FresnelSchlickRoughness() which already accounts for
// metallic reflectivity via F0 = mix(0.04, albedo, metallic). A boost here would
// exceed physical energy limits. See ComputeIBLAmbient() in Flux_DeferredShading.frag.
const float SSR_METALLIC_BOOST = 1.0;                  // Disabled - Fresnel applied in deferred shader

// NOTE: Grazing angle confidence fade constants REMOVED per industry standard.
// AAA engines (FidelityFX SSSR, UE4) do NOT fade SSR confidence based on viewing angle.
// The deferred shader's FresnelSchlickRoughness() naturally produces stronger reflections
// at grazing angles. Fading here fights against physics.
// See: AMD FidelityFX SSSR, Brian Karis "Real Shading in UE4" (SIGGRAPH 2013)

// Stochastic ray perturbation
const float SSR_ROUGHNESS_PERTURB_THRESHOLD = 0.05;    // Minimum roughness for perturbation

layout(location = 0) out vec4 o_xReflection;  // RGB = color, A = confidence

layout(location = 0) in vec2 a_xUV;

// SSR constants
layout(std140, set = 0, binding = 1) uniform SSRConstants
{
	float u_fIntensity;
	float u_fMaxDistance;
	float u_fMaxRoughness;
	float u_fThickness;
	uint u_uStepCount;       // Max iterations for hierarchical traversal
	uint u_uDebugMode;
	uint u_uHiZMipCount;     // Number of mip levels in HiZ buffer
	uint u_uStartMip;        // Starting mip for hierarchical traversal
	uint u_uFrameIndex;      // For stochastic ray direction noise variation
	// Resolution-based binary search iterations (calculated per-frame from screen width)
	// 1080p: 6, 1440p: 7, 4K: 8 iterations for appropriate sub-pixel precision
	uint u_uBinarySearchIterations;
	float u_fContactHardeningDist;  // World-space distance (meters) for sharp-to-blur transition
};

// AAA-quality: Output hit distance for contact hardening in resolve pass
// Stored in R channel of a secondary output or packed into confidence

// Textures
layout(set = 0, binding = 2) uniform sampler2D g_xDepthTex;
layout(set = 0, binding = 3) uniform sampler2D g_xNormalsTex;
layout(set = 0, binding = 4) uniform sampler2D g_xMaterialTex;
layout(set = 0, binding = 5) uniform sampler2D g_xHiZTex;
layout(set = 0, binding = 6) uniform sampler2D g_xDiffuseTex;
layout(set = 0, binding = 7) uniform sampler2D g_xBlueNoiseTex;

// Debug mode constants
const uint SSR_DEBUG_NONE = 0u;
const uint SSR_DEBUG_RAY_DIRECTIONS = 1u;
const uint SSR_DEBUG_SCREEN_DIRECTIONS = 2u;
const uint SSR_DEBUG_HIT_POSITIONS = 3u;
const uint SSR_DEBUG_REFLECTION_UVS = 4u;
const uint SSR_DEBUG_CONFIDENCE = 5u;
const uint SSR_DEBUG_DEPTH_COMPARISON = 6u;
const uint SSR_DEBUG_EDGE_FADE = 7u;
const uint SSR_DEBUG_MARCH_DISTANCE = 8u;
const uint SSR_DEBUG_FINAL_RESULT = 9u;
const uint SSR_DEBUG_ROUGHNESS = 10u;       // Visualize roughness from GBuffer
const uint SSR_DEBUG_WORLD_NORMAL_Y = 11u;  // Visualize world normal Y component

// Reconstruct view-space position from depth
vec3 GetViewPosFromDepth(vec2 xUV, float fDepth)
{
	vec2 xNDC = xUV * 2.0 - 1.0;
	vec4 xClipSpace = vec4(xNDC, fDepth, 1.0);
	vec4 xViewSpace = g_xInvProjMat * xClipSpace;
	return xViewSpace.xyz / xViewSpace.w;
}

// Convert depth buffer value to view-space Z at a given UV position
// Using proper UV coordinates gives more accurate results, especially at screen edges
float DepthToViewZ(vec2 xUV, float fDepth)
{
	vec2 xNDC = xUV * 2.0 - 1.0;
	vec4 xClipSpace = vec4(xNDC, fDepth, 1.0);
	vec4 xViewSpace = g_xInvProjMat * xClipSpace;
	return xViewSpace.z / xViewSpace.w;
}

// Transform view position to screen space (returns UV + depth)
vec3 ViewToScreen(vec3 xViewPos)
{
	vec4 xClipSpace = g_xProjMat * vec4(xViewPos, 1.0);
	xClipSpace.xyz /= xClipSpace.w;
	return vec3(xClipSpace.x * 0.5 + 0.5, xClipSpace.y * 0.5 + 0.5, xClipSpace.z);
}

// Compute edge fade to prevent artifacts at screen borders
float ComputeEdgeFade(vec2 xUV, float fMargin)
{
	vec2 xFade = smoothstep(0.0, fMargin, xUV) * smoothstep(0.0, fMargin, 1.0 - xUV);
	return xFade.x * xFade.y;
}

// Note: ImportanceSampleGGX is now in PBRConstants.fxh for consistency

// Sample blue noise with frame-based temporal offset
vec2 GetBlueNoise(ivec2 xPixelCoord, uint uFrameIndex)
{
	// Tile the blue noise texture across the screen
	vec2 xNoiseUV = vec2(xPixelCoord % ivec2(SSR_BLUE_NOISE_SIZE)) / float(SSR_BLUE_NOISE_SIZE);
	vec2 xNoise = texture(g_xBlueNoiseTex, xNoiseUV).rg;

	// Add temporal offset using golden ratio for good distribution across frames
	xNoise = fract(xNoise + float(uFrameIndex) * SSR_GOLDEN_RATIO);

	return xNoise;
}

// Hierarchical HiZ ray march function
// Uses coarse-to-fine traversal for O(log N) performance vs O(N) linear marching
// fRoughness is used to limit trace distance for rough surfaces (reduces stretching)
// fNdotV is used to limit trace distance at shallow viewing angles
vec4 RayMarch(vec3 xViewOrigin, vec3 xReflectDir, vec3 xViewNormal, float fRoughness, float fNdotV, out float fMarchDistance, out vec2 xScreenDir)
{
	fMarchDistance = 0.0;
	xScreenDir = vec2(0.0);

	// Add offset along ray direction to avoid self-intersection
	float fStartOffset = max(SSR_START_OFFSET_MIN, abs(xViewOrigin.z) * SSR_START_OFFSET_SCALE);
	vec3 xViewStart = xViewOrigin + xReflectDir * fStartOffset;

	// Project start position to screen
	vec3 xScreenStart = ViewToScreen(xViewStart);

	// Compute screen-space trace direction based on reflection ray
	float fRaySampleDist = min(u_fMaxDistance * 0.5, xViewStart.z * 0.5);
	vec3 xRaySampleView = xViewStart + xReflectDir * fRaySampleDist;

	// Clamp to prevent near-plane issues
	float fNearPlane = g_xCameraNearFar.x;
	if (xRaySampleView.z < fNearPlane)
	{
		// Avoid division by zero when reflection is perpendicular to view direction
		if (abs(xReflectDir.z) > SSR_DIRECTION_EPSILON)
		{
			float fSafeT = (fNearPlane - xViewStart.z) / xReflectDir.z * 0.9;
			if (fSafeT > 0.0)
				xRaySampleView = xViewStart + xReflectDir * fSafeT;
			else
				xRaySampleView = xViewStart + xReflectDir * 0.1;
		}
		else
		{
			xRaySampleView = xViewStart + xReflectDir * 0.1;
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
		xScreenDir = normalize(vec2(xReflectDir.x, xReflectDir.y) + vec2(SSR_DIRECTION_EPSILON));
	}

	// Determine max screen distance to trace
	// PHYSICAL ACCURACY FIX: Grazing angle removed from trace distance calculation.
	// Fresnel physics demands STRONGER reflections at grazing angles, which requires
	// full trace distance to find valid hits. AAA engines (FidelityFX SSSR, UE4)
	// trace full distance regardless of viewing angle.
	float fRoughnessScale = 1.0 - fRoughness * fRoughness * SSR_ROUGHNESS_DIST_SCALE;
	float fMaxScreenDist = SSR_MAX_SCREEN_DIST_BASE * fRoughnessScale;

	// Calculate where the ray would exit the screen
	vec2 xExitDist = vec2(1e10);
	if (abs(xScreenDir.x) > SSR_DIRECTION_EPSILON)
	{
		xExitDist.x = xScreenDir.x > 0.0 ? (1.0 - xScreenStart.x) / xScreenDir.x : -xScreenStart.x / xScreenDir.x;
	}
	if (abs(xScreenDir.y) > SSR_DIRECTION_EPSILON)
	{
		xExitDist.y = xScreenDir.y > 0.0 ? (1.0 - xScreenStart.y) / xScreenDir.y : -xScreenStart.y / xScreenDir.y;
	}
	fMaxScreenDist = min(fMaxScreenDist, min(xExitDist.x, xExitDist.y));
	fMaxScreenDist = max(fMaxScreenDist, SSR_MIN_SCREEN_TRACE_DIST);

	// For depth interpolation, compute 1/z at start and estimated end
	// Clamp to prevent division by zero or negative values
	float fSafeStartZ = max(xViewStart.z, g_xCameraNearFar.x);
	float fInvZStart = 1.0 / fSafeStartZ;
	float fEstimatedRayDist = u_fMaxDistance * fMaxScreenDist;
	float fEstimatedEndZ = fSafeStartZ + xReflectDir.z * fEstimatedRayDist;
	fEstimatedEndZ = max(g_xCameraNearFar.x, fEstimatedEndZ);
	float fInvZEnd = 1.0 / fEstimatedEndZ;

	// Get screen dimensions for texel size calculation
	vec2 xScreenSize = textureSize(g_xHiZTex, 0);
	vec2 xTexelSize = 1.0 / xScreenSize;

	// Hierarchical traversal state
	int iCurrentMip = int(min(u_uStartMip, u_uHiZMipCount - 1u));
	float fT = 0.0;                    // Current parametric position along ray [0, 1]
	float fPrevT = 0.0;
	float fPrevDepthDiff = 0.0;
	bool bFirstSample = true;

	// ========== AAA ROUGHNESS-BASED CONE TRACING ==========
	// For rough surfaces, don't refine all the way to mip 0
	// Instead, clamp minimum mip based on roughness to match reflection cone footprint
	// This improves quality for rough reflections by sampling appropriate spatial area
	// smooth (r=0): refine to mip 0 for sharp reflections
	// rough (r=0.5): minimum mip 1-2, samples 2-4 pixel region
	// very rough (r=1.0): minimum mip 2-3, samples 4-8 pixel region
	int iMinMip = int(floor(fRoughness * SSR_ROUGHNESS_MIP_SCALE));  // 0 at smooth, ~2-3 at rough
	iMinMip = clamp(iMinMip, 0, SSR_MAX_ROUGHNESS_MIP);  // Cap at mip 3

	// Hierarchical ray march
	for (uint uIter = 0u; uIter < u_uStepCount; uIter++)
	{
		// Current sample position on screen
		vec2 xSampleUV = xScreenStart.xy + xScreenDir * fT * fMaxScreenDist;

		// Bounds check
		if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
			xSampleUV.y < 0.0 || xSampleUV.y > 1.0)
		{
			break;
		}

		// Sample HiZ at current mip level (RG format: R = min depth, G = max depth)
		// Conservative mip sampling - clamp to texel center to avoid boundary issues
		ivec2 xMipSize = textureSize(g_xHiZTex, iCurrentMip);
		vec2 xHalfTexel = 0.5 / vec2(xMipSize);
		vec2 xClampedUV = clamp(xSampleUV, xHalfTexel, 1.0 - xHalfTexel);
		vec2 xHiZSample = textureLod(g_xHiZTex, xClampedUV, float(iCurrentMip)).rg;
		float fMinDepth = xHiZSample.r;  // Closest surface in this region
		float fMaxDepth = xHiZSample.g;  // Farthest surface in this region

		// Skip sky pixels (check both min and max)
		if (fMinDepth >= SSR_SKY_DEPTH_THRESHOLD)
		{
			// Step forward at current mip level
			float fMipScale = exp2(float(iCurrentMip));
			float fStepSize = fMipScale * max(xTexelSize.x, xTexelSize.y) * SSR_STEP_SIZE_MULTIPLIER;
			fPrevT = fT;
			fT += fStepSize / fMaxScreenDist;

			if (fT >= 1.0)
				break;
			continue;
		}

		// Convert depth to view-space Z (use min depth for conservative test)
		float fMinViewZ = DepthToViewZ(xSampleUV, fMinDepth);
		float fMaxViewZ = DepthToViewZ(xSampleUV, fMaxDepth);

		// Perspective-correct ray depth
		float fInvZ = mix(fInvZStart, fInvZEnd, fT);
		float fRayViewZ = 1.0 / fInvZ;

		// Depth differences for min-max testing
		// fMinDiff > 0: ray is behind closest surface
		// fMaxDiff > 0: ray is behind farthest surface (definite intersection zone)
		float fMinDiff = fRayViewZ - fMinViewZ;
		float fMaxDiff = fRayViewZ - fMaxViewZ;

		// Adaptive thickness based on current mip level (larger tolerance at coarser mips)
		float fMipThickness = u_fThickness * exp2(float(iCurrentMip));

		// Use min depth for conservative "behind surface" test
		bool bBehindMinSurface = fMinDiff > 0.0;
		// Use max depth for definite hit confirmation
		bool bBehindMaxSurface = fMaxDiff > 0.0;
		bool bWithinThickness = fMinDiff < fMipThickness * SSR_STEP_SIZE_MULTIPLIER;
		bool bValidDepth = fMinViewZ > SSR_DEPTH_VALID_MIN;

		// For the rest of the algorithm, use fMinDiff as the primary depth diff
		float fDepthDiff = fMinDiff;

		if (bBehindMinSurface && bValidDepth)
		{
			// Ray is behind surface - need to refine or report hit
			// Use roughness-based minimum mip for cone tracing
			if (iCurrentMip <= iMinMip)
			{
				// At finest mip and behind surface - potential hit
				if (bWithinThickness || (!bFirstSample && fPrevDepthDiff * fDepthDiff < 0.0))
				{
					// Binary search refinement for accurate hit position
					float fSearchStart = fPrevT;
					float fSearchEnd = fT;

					// Binary search refinement at minimum mip level (respects roughness cone)
					float fSearchMip = float(iMinMip);

					// Calculate relative tolerance based on initial search range
					// This prevents over-refinement for short rays and under-refinement for long rays
					float fInitialRange = fSearchEnd - fSearchStart;
					float fTolerance = max(fInitialRange * SSR_BINARY_SEARCH_RELATIVE_TOLERANCE, SSR_BINARY_SEARCH_MIN_TOLERANCE);

					for (int j = 0; j < int(u_uBinarySearchIterations); j++)  // Resolution-based iterations (6-8)
					{
						// Early termination when search has converged (using relative tolerance)
						if (abs(fSearchEnd - fSearchStart) < fTolerance)
							break;

						float fMidT = (fSearchStart + fSearchEnd) * 0.5;

						vec2 xMidUV = xScreenStart.xy + xScreenDir * fMidT * fMaxScreenDist;
						float fMidSceneDepth = textureLod(g_xHiZTex, xMidUV, fSearchMip).r;  // Use min depth at roughness mip
						float fMidSceneZ = DepthToViewZ(xMidUV, fMidSceneDepth);

						float fMidInvZ = mix(fInvZStart, fInvZEnd, fMidT);
						float fMidRayZ = 1.0 / fMidInvZ;
						float fMidDiff = fMidRayZ - fMidSceneZ;

						if (fMidDiff > 0.0)
							fSearchEnd = fMidT;
						else
							fSearchStart = fMidT;
					}

					// Compute final hit position
					float fFinalT = (fSearchStart + fSearchEnd) * 0.5;
					vec2 xFinalUV = xScreenStart.xy + xScreenDir * fFinalT * fMaxScreenDist;
					float fFinalSceneDepth = textureLod(g_xHiZTex, xFinalUV, fSearchMip).r;
					float fFinalSceneZ = DepthToViewZ(xFinalUV, fFinalSceneDepth);

					// Backface detection: reject hits on surfaces facing away from the ray
					// Sample world-space normal at hit position and transform to view space
					vec3 xHitWorldNormal = texture(g_xNormalsTex, xFinalUV).rgb;
					vec3 xHitViewNormal = normalize(mat3(g_xViewMat) * xHitWorldNormal);

					// Reflection direction in view space (passed into this function)
					// If hit normal faces same direction as ray, it's a backface
					float fBackfaceDot = dot(xHitViewNormal, xReflectDir);
					if (fBackfaceDot > SSR_BACKFACE_THRESHOLD)  // Small threshold to handle grazing angles
					{
						// Backface hit - reject and continue marching
						fPrevT = fT;
						float fStepSize = xTexelSize.x * SSR_STEP_SIZE_MULTIPLIER;
						fT += fStepSize / fMaxScreenDist;
						bFirstSample = false;
						continue;
					}

					// Confidence calculation
					float fEdgeMargin = SSR_EDGE_MARGIN_BASE + fRoughness * SSR_EDGE_MARGIN_ROUGHNESS;  // 15-25% margin (AAA standard)
					float fEdgeFade = ComputeEdgeFade(xFinalUV, fEdgeMargin);

					// Distance fade - purely geometric, fades confidence at far trace distances
					// ENERGY CONSERVATION: Fresnel compensation removed. The deferred shader
					// applies FresnelSchlickRoughness() to SSR output, which correctly handles
					// grazing angle reflections. Applying Fresnel here caused double-weighting.
					float fDistanceFade = 1.0 - smoothstep(SSR_DISTANCE_FADE_START, SSR_DISTANCE_FADE_END, fFinalT);

					float fFinalInvZ = mix(fInvZStart, fInvZEnd, fFinalT);
					float fFinalRayZ = 1.0 / fFinalInvZ;
					float fDepthError = abs(fFinalRayZ - fFinalSceneZ);
					float fDepthConfidence = 1.0 - smoothstep(0.0, u_fThickness * SSR_STEP_SIZE_MULTIPLIER, fDepthError);

					// Add backface confidence penalty for surfaces nearly parallel to ray
					float fBackfaceConfidence = smoothstep(SSR_BACKFACE_SMOOTHSTEP_MIN, SSR_BACKFACE_SMOOTHSTEP_MAX, fBackfaceDot);

					// PHYSICAL ACCURACY FIX: Stretch penalty removed.
					// The previous metric used dot(reflectDir, normal) which correlates with viewing
					// angle for horizontal surfaces (floors). At grazing angles on floors, the reflection
					// direction is nearly horizontal while the normal is vertical, causing low confidence
					// at exactly the angles where Fresnel says reflections should be strongest.
					// Stretching is now handled by HiZ traversal, binary search refinement,
					// backface rejection, and resolve pass blur.
					float fStretchConfidence = 1.0;

					float fConfidence = fEdgeFade * fDistanceFade * fDepthConfidence * fBackfaceConfidence * fStretchConfidence;
					fConfidence = max(fConfidence, SSR_MIN_CONFIDENCE);

					fMarchDistance = fFinalT;
					return vec4(xFinalUV, fFinalSceneDepth, fConfidence);
				}
				else
				{
					// Behind surface but outside thickness - step back and try smaller step
					fT = fPrevT;
					float fStepSize = xTexelSize.x * 0.5;
					fPrevT = fT;
					fT += fStepSize / fMaxScreenDist;
				}
			}
			else
			{
				// Behind surface at coarse mip - refine to finer mip
				// Step back slightly before refining
				fT = max(0.0, fT - exp2(float(iCurrentMip - 1)) * max(xTexelSize.x, xTexelSize.y) / fMaxScreenDist);
				iCurrentMip--;
			}
		}
		else
		{
			// Ray is in front of surface - step forward
			// Use step size appropriate for current mip level
			float fMipScale = exp2(float(iCurrentMip));
			float fStepSize = fMipScale * max(xTexelSize.x, xTexelSize.y) * SSR_STEP_SIZE_MULTIPLIER;

			fPrevDepthDiff = fDepthDiff;
			fPrevT = fT;
			fT += fStepSize / fMaxScreenDist;

			// If we've stepped too far, go to coarser mip for faster traversal
			// But only if we haven't recently refined
			if (fT < SSR_DISTANCE_FADE_START && iCurrentMip < int(u_uStartMip) && uIter > 4u)
			{
				// Consider going to coarser mip if consistently in front
				// This helps skip large empty regions quickly
			}

			if (fT >= 1.0)
				break;
		}

		bFirstSample = false;
	}

	return vec4(0.0);
}

void main()
{
	// Debug mode 99: Output solid magenta for ALL pixels to verify shader is running
	// This should appear BEFORE any texture reads or early exits
	if (u_uDebugMode == 99u)
	{
		o_xReflection = vec4(1.0, 0.0, 1.0, 1.0);  // Solid magenta
		return;
	}

	float fDepth = texture(g_xDepthTex, a_xUV).r;

	// Early exit for sky
	if (fDepth >= 1.0)
	{
		o_xReflection = vec4(0.0);
		return;
	}

	// Read material properties
	vec4 xMaterial = texture(g_xMaterialTex, a_xUV);
	float fRoughness = xMaterial.r;

	// Get normal (stored in world space as float16, already [-1,1] range)
	vec3 xWorldNormal = texture(g_xNormalsTex, a_xUV).rgb;

	// Debug: visualize roughness from GBuffer (black=0, white=1, green=0.5/maxRoughness)
	if (u_uDebugMode == SSR_DEBUG_ROUGHNESS)
	{
		// Red channel = actual roughness, Green = would pass threshold, Blue = roughness/maxRoughness
		float fPassesThreshold = (fRoughness <= u_fMaxRoughness) ? 1.0 : 0.0;
		o_xReflection = vec4(fRoughness, fPassesThreshold, fRoughness / u_fMaxRoughness, 1.0);
		return;
	}

	// Debug: visualize world normal Y component (floor normals should be bright)
	if (u_uDebugMode == SSR_DEBUG_WORLD_NORMAL_Y)
	{
		// R = normal.y (floor=bright), G = normal.y > 0.7, B = normal.y > 0.9
		float fNormalY = xWorldNormal.y * 0.5 + 0.5;  // Map -1..1 to 0..1
		o_xReflection = vec4(fNormalY, (xWorldNormal.y > 0.7) ? 1.0 : 0.0, (xWorldNormal.y > 0.9) ? 1.0 : 0.0, 1.0);
		return;
	}

	// Skip very rough surfaces
	if (fRoughness > u_fMaxRoughness)
	{
		o_xReflection = vec4(0.0);
		return;
	}

	// Reconstruct position in view space
	vec3 xViewPos = GetViewPosFromDepth(a_xUV, fDepth);

	vec3 xViewNormal = normalize(mat3(g_xViewMat) * xWorldNormal);

	// Compute reflection direction in view space
	vec3 xViewDir = normalize(xViewPos);  // Direction from camera to surface
	vec3 xReflectDir = reflect(xViewDir, xViewNormal);

	// Apply stochastic perturbation for rough surfaces using GGX importance sampling
	// This gives physically accurate rough reflections instead of post-blur only
	if (fRoughness > SSR_ROUGHNESS_PERTURB_THRESHOLD)
	{
		ivec2 xPixelCoord = ivec2(gl_FragCoord.xy);
		vec2 xNoise = GetBlueNoise(xPixelCoord, u_uFrameIndex);

		// Sample GGX distribution around the perfect reflection direction
		vec3 xH = ImportanceSampleGGX(xNoise, xViewNormal, fRoughness);
		vec3 xPerturbedDir = reflect(xViewDir, xH);

		// Ensure the perturbed direction is valid (above the surface)
		if (dot(xPerturbedDir, xViewNormal) > 0.0)
		{
			xReflectDir = xPerturbedDir;
		}
	}

	// Debug: ray directions
	if (u_uDebugMode == SSR_DEBUG_RAY_DIRECTIONS)
	{
		o_xReflection = vec4(xReflectDir * 0.5 + 0.5, 1.0);
		return;
	}

	// Debug: edge fade
	if (u_uDebugMode == SSR_DEBUG_EDGE_FADE)
	{
		float fEdgeFade = ComputeEdgeFade(a_xUV, 0.1);
		o_xReflection = vec4(vec3(fEdgeFade), 1.0);
		return;
	}

	// Calculate viewing angle for ray march distance limiting
	float fNdotV = max(dot(xViewNormal, -xViewDir), 0.0);

	// Perform ray march - roughness and viewing angle affect max trace distance
	float fMarchDistance = 0.0;
	vec2 xScreenDir = vec2(0.0);
	vec4 xHitResult = RayMarch(xViewPos, xReflectDir, xViewNormal, fRoughness, fNdotV, fMarchDistance, xScreenDir);

	// Debug: screen-space march direction
	if (u_uDebugMode == SSR_DEBUG_SCREEN_DIRECTIONS)
	{
		o_xReflection = vec4(xScreenDir * 0.5 + 0.5, 0.0, 1.0);
		return;
	}

	// Debug: reflection UVs
	if (u_uDebugMode == SSR_DEBUG_REFLECTION_UVS)
	{
		o_xReflection = vec4(xHitResult.xy, 0.0, 1.0);
		return;
	}

	// Debug: confidence mask
	if (u_uDebugMode == SSR_DEBUG_CONFIDENCE)
	{
		o_xReflection = vec4(vec3(xHitResult.w), 1.0);
		return;
	}

	// Debug: depth comparison - visualize depth differences found during trace
	if (u_uDebugMode == SSR_DEBUG_DEPTH_COMPARISON)
	{
		vec3 xScreenStart = ViewToScreen(xViewPos);
		float fStartViewZ = xViewPos.z;

		float fMaxDepthDiff = 0.0;  // Maximum (startZ - sceneZ) found
		float fMinSceneZ = 1e10;
		bool bFoundCloser = false;

		for (uint i = 1u; i <= 64u; i++)
		{
			float fT = float(i) / 64.0;
			vec2 xSampleUV = xScreenStart.xy + xScreenDir * fT * 0.8;

			if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
				xSampleUV.y < 0.0 || xSampleUV.y > 1.0)
				break;

			float fSceneDepth = texture(g_xHiZTex, xSampleUV).r;
			if (fSceneDepth < 0.9999)
			{
				float fSceneViewZ = DepthToViewZ(xSampleUV, fSceneDepth);
				float fDepthDiff = fStartViewZ - fSceneViewZ;
				fMaxDepthDiff = max(fMaxDepthDiff, fDepthDiff);
				fMinSceneZ = min(fMinSceneZ, fSceneViewZ);
				if (fSceneViewZ < fStartViewZ)
					bFoundCloser = true;
			}
		}

		// Red = max depth difference in world units / 10 (how much closer did we find?)
		// Green = 1 if we found ANY closer geometry, 0 if not
		// Blue = would hit with 0.5 unit threshold? (fMaxDepthDiff > 0.5)
		float fR = clamp(fMaxDepthDiff / 10.0, 0.0, 1.0);  // 0-10 units shown as 0-1
		float fG = bFoundCloser ? 1.0 : 0.0;
		float fB = (fMaxDepthDiff > 0.5) ? 1.0 : 0.0;

		o_xReflection = vec4(fR, fG, fB, 1.0);
		return;
	}

	// Debug: march distance - repurposed to show trace endpoint
	if (u_uDebugMode == SSR_DEBUG_MARCH_DISTANCE)
	{
		// Show where the trace ends up (final sample position)
		vec3 xScreenStart = ViewToScreen(xViewPos);
		vec2 xEndUV = xScreenStart.xy + xScreenDir * 0.8;  // End of trace

		// Sample what's at the end position
		float fEndDepth = texture(g_xHiZTex, clamp(xEndUV, vec2(0.0), vec2(1.0))).r;
		bool bHitSky = fEndDepth >= 0.9999;

		// Red = end UV X, Green = end UV Y, Blue = hit sky?
		o_xReflection = vec4(xEndUV.x, xEndUV.y, bHitSky ? 1.0 : 0.0, 1.0);
		return;
	}

	if (xHitResult.w > 0.0)
	{
		// Sample color at hit position from GBuffer diffuse
		vec3 xReflectionColor = texture(g_xDiffuseTex, xHitResult.xy).rgb;

		// Debug: hit positions
		if (u_uDebugMode == SSR_DEBUG_HIT_POSITIONS)
		{
			vec3 xHitWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, xHitResult.xy);
			o_xReflection = vec4(xHitWorldPos / 100.0, 1.0);
			return;
		}

		// Apply intensity
		xReflectionColor *= u_fIntensity;

		// Note: Fresnel is applied in deferred shader via BRDF LUT (F * scale + bias)
		// Do NOT apply Fresnel here to avoid double application

		// Start with ray march confidence
		float fFinalConfidence = xHitResult.w;

		// ========== AAA CONTACT HARDENING ==========
		// Calculate world-space travel distance for contact hardening
		// Reflections close to contact point should be sharp, far reflections blur
		// Using world-space distance ensures correct behavior regardless of camera distance
		vec3 xOriginWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, a_xUV);
		vec3 xHitWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, xHitResult.xy);
		float fWorldTravelDist = length(xHitWorldPos - xOriginWorldPos);

		// Contact hardening factor: 0 at contact (sharp), 1 at far (blur)
		// This is encoded in confidence: high confidence = sharp reflection
		float fContactHardening = smoothstep(0.0, u_fContactHardeningDist, fWorldTravelDist);

		// For smooth surfaces, maintain sharpness at contact
		// For rough surfaces, blur transitions faster
		float fRoughnessContactScale = mix(1.0, SSR_CONTACT_ROUGHNESS_SCALE_MAX, fRoughness);
		fContactHardening = clamp(fContactHardening * fRoughnessContactScale, 0.0, 1.0);

		// Contact hardening confidence - confidence reduces with reflection distance
		// ENERGY CONSERVATION: Grazing angle compensation removed. The deferred shader
		// applies FresnelSchlickRoughness() to SSR output, which correctly handles
		// grazing angle reflectivity. Contact hardening is now purely distance-based.
		float fContactConfidence = 1.0 - fContactHardening * SSR_CONTACT_CONFIDENCE_SCALE;

		// Reduce confidence for rough surfaces - SSR can't properly represent rough reflections
		// This causes the deferred shader to blend more toward IBL (which handles roughness correctly)
		// Smooth surfaces: keep full SSR, rough surfaces: fade toward IBL
		float fRoughnessConfidence = 1.0 - smoothstep(SSR_ROUGHNESS_CONF_START, SSR_ROUGHNESS_CONF_END, fRoughness);  // Preserve smooth surface reflections
		fFinalConfidence *= fRoughnessConfidence;

		// NOTE: Metallic boost was removed for energy conservation.
		// The deferred shader's FresnelSchlickRoughness() already handles metallic surfaces
		// correctly by using F0 = mix(0.04, albedo, metallic). Adding a boost here would
		// cause metallic surfaces to exceed physical energy limits.
		// If you need stronger SSR for metals, adjust the IBL/SSR blend weights in the
		// deferred shader instead, which preserves energy conservation.
		float fMetallic = texture(g_xMaterialTex, a_xUV).g;
		float fMetallicBoost = mix(1.0, SSR_METALLIC_BOOST, fMetallic);  // Now 1.0 (no boost)
		fFinalConfidence *= fMetallicBoost;

		// NOTE: Grazing angle confidence fade REMOVED per industry standard (FidelityFX SSSR, UE4).
		// The deferred shader applies FresnelSchlickRoughness() which naturally produces
		// stronger reflections at grazing angles. Fading SSR confidence at grazing angles
		// fights against this physics - reflections become weak where Fresnel says strong.
		// The existing stretch penalty (fStretchConfidence) handles quality issues at
		// extreme angles without inverting the Fresnel effect.

		// Apply contact hardening to final confidence
		fFinalConfidence *= fContactConfidence;
		fFinalConfidence = clamp(fFinalConfidence, 0.0, 1.0);

		o_xReflection = vec4(xReflectionColor, fFinalConfidence);
	}
	else
	{
		o_xReflection = vec4(0.0);
	}
}
