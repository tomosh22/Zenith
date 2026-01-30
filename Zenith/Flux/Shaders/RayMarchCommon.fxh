#ifndef RAY_MARCH_COMMON_FXH
#define RAY_MARCH_COMMON_FXH

// ============================================================================
// SHARED HiZ RAY MARCH UTILITIES
// Zenith Engine - Flux Renderer
//
// Common hierarchical ray marching infrastructure used by SSR and SSGI.
// Provides O(log N) performance via coarse-to-fine HiZ traversal.
//
// References:
// - GPU Pro 5: "Hi-Z Screen-Space Cone-Traced Reflections"
// - FidelityFX SSSR: AMD's reference SSR implementation
// ============================================================================

#include "Common.fxh"
#include "PBRConstants.fxh"

// ============================================================================
// RAY MARCH CONFIGURATION STRUCTURE
// Allows SSR/SSGI to customize behavior while sharing core logic
// ============================================================================
struct RayMarchConfig
{
	// Ray start offset (prevents self-intersection)
	float fStartOffsetMin;        // Minimum ray start offset (world units)
	float fStartOffsetScale;      // Depth-proportional offset scale

	// Screen-space limits
	float fMinScreenTraceDist;    // Minimum screen-space trace (fraction of screen)
	float fMaxScreenDist;         // Maximum screen distance to trace

	// HiZ traversal
	float fSkyDepthThreshold;     // Depth value considered sky
	float fStepSizeMultiplier;    // Step size scaling factor
	float fThickness;             // Surface thickness for hit detection

	// Hit validation
	float fBackfaceThreshold;     // Backface rejection threshold (dot product)
	float fDepthValidMin;         // Minimum valid view-space depth

	// Confidence calculation
	float fEdgeMargin;            // Screen edge fade margin (fraction)
	float fDistanceFadeStart;     // Distance fade start (parametric)
	float fDistanceFadeEnd;       // Distance fade end (parametric)
	float fMinConfidence;         // Minimum confidence value
	float fBackfaceSmoothstepMin; // Backface confidence smoothstep start
	float fBackfaceSmoothstepMax; // Backface confidence smoothstep end

	// Roughness-based cone tracing
	float fRoughnessMipScale;     // Maps roughness to mip level
	int   iMaxRoughnessMip;       // Maximum mip level for cone tracing

	// Binary search refinement
	int   iBinarySearchIterations;  // Number of binary search iterations
	float fBinarySearchTolerance;   // Binary search tolerance
};

// ============================================================================
// RAY MARCH RESULT STRUCTURE
// ============================================================================
struct RayMarchResult
{
	vec2  xHitUV;         // UV of hit position
	float fHitDepth;      // Depth at hit position
	float fConfidence;    // Hit confidence [0,1]
	bool  bHit;           // Whether a valid hit occurred
};

// ============================================================================
// DEFAULT CONFIGURATIONS
// ============================================================================

// SSR default configuration
RayMarchConfig GetSSRConfig(float fThickness)
{
	RayMarchConfig config;

	config.fStartOffsetMin = 0.1;
	config.fStartOffsetScale = 0.01;
	config.fMinScreenTraceDist = 0.01;
	config.fMaxScreenDist = 0.8;
	config.fSkyDepthThreshold = 0.9999;
	config.fStepSizeMultiplier = 2.0;
	config.fThickness = fThickness;
	config.fBackfaceThreshold = 0.1;
	config.fDepthValidMin = 0.1;
	config.fEdgeMargin = 0.15;
	config.fDistanceFadeStart = 0.3;
	config.fDistanceFadeEnd = 0.8;
	config.fMinConfidence = 0.1;
	config.fBackfaceSmoothstepMin = 0.1;
	config.fBackfaceSmoothstepMax = -0.3;
	config.fRoughnessMipScale = 2.5;
	config.iMaxRoughnessMip = 3;
	config.iBinarySearchIterations = 6;
	config.fBinarySearchTolerance = 0.0001;

	return config;
}

// SSGI default configuration
RayMarchConfig GetSSGIConfig(float fThickness)
{
	RayMarchConfig config;

	config.fStartOffsetMin = 0.1;
	config.fStartOffsetScale = 0.01;
	config.fMinScreenTraceDist = 0.01;
	config.fMaxScreenDist = 0.6;
	config.fSkyDepthThreshold = 0.9999;
	config.fStepSizeMultiplier = 2.0;
	config.fThickness = fThickness;
	config.fBackfaceThreshold = 0.1;
	config.fDepthValidMin = 0.1;
	config.fEdgeMargin = 0.15;
	config.fDistanceFadeStart = 0.4;
	config.fDistanceFadeEnd = 0.9;
	config.fMinConfidence = 0.02;
	config.fBackfaceSmoothstepMin = 0.1;
	config.fBackfaceSmoothstepMax = -0.3;
	config.fRoughnessMipScale = 2.0;
	config.iMaxRoughnessMip = 2;
	config.iBinarySearchIterations = 4;
	config.fBinarySearchTolerance = 0.001;

	return config;
}

// ============================================================================
// SHARED UTILITY FUNCTIONS
// ============================================================================

// Compute ray start position with self-intersection offset
vec3 ComputeRayStart(vec3 xViewOrigin, vec3 xRayDir, float fOffsetMin, float fOffsetScale)
{
	float fStartOffset = max(fOffsetMin, abs(xViewOrigin.z) * fOffsetScale);
	return xViewOrigin + xRayDir * fStartOffset;
}

// Compute screen-space direction from view-space ray
// Returns screen direction and updates ray sample position if clamped to near plane
vec2 ComputeScreenDirection(vec3 xViewStart, vec3 xRayDir, float fMaxDistance, float fNearPlane)
{
	// Compute screen-space trace direction based on reflection ray
	float fRaySampleDist = min(fMaxDistance * 0.5, xViewStart.z * 0.5);
	vec3 xRaySampleView = xViewStart + xRayDir * fRaySampleDist;

	// Clamp to prevent near-plane issues
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

	vec3 xScreenStart = ViewToScreen(xViewStart);
	vec3 xRaySampleScreen = ViewToScreen(xRaySampleView);
	vec2 xScreenDelta = xRaySampleScreen.xy - xScreenStart.xy;
	float fDeltaLen = length(xScreenDelta);

	if (fDeltaLen > 0.001)
	{
		return xScreenDelta / fDeltaLen;
	}
	else
	{
		// Fallback for degenerate screen direction (ray going nearly straight into/out of screen)
		// Preserve the sign of non-zero ray components to avoid directional bias
		vec2 xDir = vec2(xRayDir.x, xRayDir.y);
		vec2 xEps = vec2(PBR_DIRECTION_EPSILON);
		if (xDir.x < 0.0) xEps.x = -PBR_DIRECTION_EPSILON;
		if (xDir.y < 0.0) xEps.y = -PBR_DIRECTION_EPSILON;
		return normalize(xDir + xEps);
	}
}

// Compute screen exit distance for ray termination
float ComputeScreenExitDistance(vec3 xScreenStart, vec2 xScreenDir, float fMaxScreenDist, float fMinScreenTraceDist)
{
	vec2 xExitDist = vec2(1e10);
	if (abs(xScreenDir.x) > PBR_DIRECTION_EPSILON)
	{
		xExitDist.x = xScreenDir.x > 0.0 ? (1.0 - xScreenStart.x) / xScreenDir.x : -xScreenStart.x / xScreenDir.x;
	}
	if (abs(xScreenDir.y) > PBR_DIRECTION_EPSILON)
	{
		xExitDist.y = xScreenDir.y > 0.0 ? (1.0 - xScreenStart.y) / xScreenDir.y : -xScreenStart.y / xScreenDir.y;
	}
	float fExitDist = min(fMaxScreenDist, min(xExitDist.x, xExitDist.y));
	return max(fExitDist, fMinScreenTraceDist);
}

// Setup perspective-correct depth interpolation
// Returns inverse Z at start and end for linear interpolation
void SetupDepthInterpolation(vec3 xViewStart, vec3 xRayDir, float fMaxDistance, float fMaxScreenDist,
                              float fNearPlane, out float fInvZStart, out float fInvZEnd)
{
	float fSafeStartZ = max(xViewStart.z, fNearPlane);
	fInvZStart = 1.0 / fSafeStartZ;
	float fEstimatedRayDist = fMaxDistance * fMaxScreenDist;
	float fEstimatedEndZ = fSafeStartZ + xRayDir.z * fEstimatedRayDist;
	fEstimatedEndZ = max(fNearPlane, fEstimatedEndZ);
	fInvZEnd = 1.0 / fEstimatedEndZ;
}

// Sample HiZ buffer with proper clamping to texel center
vec2 SampleHiZ(sampler2D xHiZTex, vec2 xUV, int iMip)
{
	ivec2 xMipSize = textureSize(xHiZTex, iMip);
	vec2 xHalfTexel = 0.5 / vec2(xMipSize);
	vec2 xClampedUV = clamp(xUV, xHalfTexel, 1.0 - xHalfTexel);
	return textureLod(xHiZTex, xClampedUV, float(iMip)).rg;
}

// Detect backface hits
// Returns true if the hit is a backface (should be rejected)
bool IsBackfaceHit(vec2 xHitUV, vec3 xRayDir, float fThreshold,
                   sampler2D xNormalsTex, mat4 xViewMat)
{
	vec3 xHitWorldNormal = texture(xNormalsTex, xHitUV).rgb;
	vec3 xHitViewNormal = normalize(mat3(xViewMat) * xHitWorldNormal);
	float fBackfaceDot = dot(xHitViewNormal, xRayDir);
	return fBackfaceDot > fThreshold;
}

// Compute hit confidence based on geometric factors
float ComputeHitConfidence(vec2 xFinalUV, float fFinalT, float fDepthError,
                           float fBackfaceDot, RayMarchConfig config)
{
	// Edge fade
	float fEdgeFade = ComputeEdgeFade(xFinalUV, config.fEdgeMargin);

	// Distance fade - purely geometric
	float fDistanceFade = 1.0 - smoothstep(config.fDistanceFadeStart, config.fDistanceFadeEnd, fFinalT);

	// Depth confidence
	float fDepthConfidence = 1.0 - smoothstep(0.0, config.fThickness * config.fStepSizeMultiplier, fDepthError);

	// Backface confidence
	float fBackfaceConfidence = smoothstep(config.fBackfaceSmoothstepMin, config.fBackfaceSmoothstepMax, fBackfaceDot);

	float fConfidence = fEdgeFade * fDistanceFade * fDepthConfidence * fBackfaceConfidence;
	return max(fConfidence, config.fMinConfidence);
}

// ============================================================================
// MAIN HiZ RAY MARCH FUNCTION
// Core hierarchical traversal shared by SSR and SSGI
// ============================================================================

// Performs hierarchical HiZ ray march
// xViewOrigin: Ray origin in view space
// xRayDir: Ray direction in view space (normalized)
// xViewNormal: Surface normal in view space
// fRoughness: Surface roughness [0,1] for cone tracing
// config: Ray march configuration
// uStepCount: Maximum iterations for hierarchical traversal
// uStartMip: Starting mip for coarse traversal
// uHiZMipCount: Total number of mip levels in HiZ buffer
// fMaxDistance: Maximum world-space trace distance
// xHiZTex: HiZ depth buffer texture
// xNormalsTex: World-space normals texture
// xViewMat: View matrix
// fMarchDistance: Output - parametric distance traveled
// xScreenDir: Output - screen-space direction
RayMarchResult HiZRayMarch(
	vec3 xViewOrigin,
	vec3 xRayDir,
	vec3 xViewNormal,
	float fRoughness,
	RayMarchConfig config,
	uint uStepCount,
	uint uStartMip,
	uint uHiZMipCount,
	float fMaxDistance,
	sampler2D xHiZTex,
	sampler2D xNormalsTex,
	mat4 xViewMat,
	vec2 xCameraNearFar,
	out float fMarchDistance,
	out vec2 xScreenDir)
{
	RayMarchResult result;
	result.xHitUV = vec2(0.0);
	result.fHitDepth = 0.0;
	result.fConfidence = 0.0;
	result.bHit = false;

	fMarchDistance = 0.0;
	xScreenDir = vec2(0.0);

	// Compute ray start with offset to avoid self-intersection
	vec3 xViewStart = ComputeRayStart(xViewOrigin, xRayDir, config.fStartOffsetMin, config.fStartOffsetScale);

	// Project start position to screen
	vec3 xScreenStart = ViewToScreen(xViewStart);

	// Compute screen-space direction
	float fNearPlane = xCameraNearFar.x;
	xScreenDir = ComputeScreenDirection(xViewStart, xRayDir, fMaxDistance, fNearPlane);

	// Compute max screen distance to trace
	float fMaxScreenDist = ComputeScreenExitDistance(xScreenStart, xScreenDir, config.fMaxScreenDist, config.fMinScreenTraceDist);

	// Setup depth interpolation
	float fInvZStart, fInvZEnd;
	SetupDepthInterpolation(xViewStart, xRayDir, fMaxDistance, fMaxScreenDist, fNearPlane, fInvZStart, fInvZEnd);

	// Get screen dimensions for texel size calculation
	vec2 xScreenSize = textureSize(xHiZTex, 0);
	vec2 xTexelSize = 1.0 / xScreenSize;

	// Hierarchical traversal state
	int iCurrentMip = int(min(uStartMip, uHiZMipCount - 1u));
	float fT = 0.0;
	float fPrevT = 0.0;
	float fPrevDepthDiff = 0.0;
	bool bFirstSample = true;

	// Roughness-based cone tracing: rough surfaces sample coarser mips
	int iMinMip = int(floor(fRoughness * config.fRoughnessMipScale));
	iMinMip = clamp(iMinMip, 0, config.iMaxRoughnessMip);

	// Hierarchical ray march loop
	for (uint uIter = 0u; uIter < uStepCount; uIter++)
	{
		// Current sample position on screen
		vec2 xSampleUV = xScreenStart.xy + xScreenDir * fT * fMaxScreenDist;

		// Bounds check
		if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
		    xSampleUV.y < 0.0 || xSampleUV.y > 1.0)
		{
			break;
		}

		// Sample HiZ at current mip level
		vec2 xHiZSample = SampleHiZ(xHiZTex, xSampleUV, iCurrentMip);
		float fMinDepth = xHiZSample.r;

		// Skip sky pixels
		if (fMinDepth >= config.fSkyDepthThreshold)
		{
			float fMipScale = exp2(float(iCurrentMip));
			float fStepSize = fMipScale * max(xTexelSize.x, xTexelSize.y) * config.fStepSizeMultiplier;
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

		// Adaptive thickness based on current mip level
		float fMipThickness = config.fThickness * exp2(float(iCurrentMip));

		bool bBehindSurface = fDepthDiff > 0.0;
		bool bWithinThickness = fDepthDiff < fMipThickness * config.fStepSizeMultiplier;
		bool bValidDepth = fMinViewZ > config.fDepthValidMin;

		if (bBehindSurface && bValidDepth)
		{
			// Ray is behind surface - need to refine or report hit
			if (iCurrentMip <= iMinMip)
			{
				// At finest mip and behind surface - potential hit
				if (bWithinThickness || (!bFirstSample && fPrevDepthDiff * fDepthDiff < 0.0))
				{
					// Binary search refinement for accurate hit position
					float fSearchStart = fPrevT;
					float fSearchEnd = fT;
					float fSearchMip = float(iMinMip);

					for (int j = 0; j < config.iBinarySearchIterations; j++)
					{
						if (abs(fSearchEnd - fSearchStart) < config.fBinarySearchTolerance)
							break;

						float fMidT = (fSearchStart + fSearchEnd) * 0.5;
						vec2 xMidUV = xScreenStart.xy + xScreenDir * fMidT * fMaxScreenDist;
						float fMidSceneDepth = textureLod(xHiZTex, xMidUV, fSearchMip).r;
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
					float fFinalSceneDepth = textureLod(xHiZTex, xFinalUV, fSearchMip).r;
					float fFinalSceneZ = DepthToViewZ(xFinalUV, fFinalSceneDepth);

					// Backface detection
					if (IsBackfaceHit(xFinalUV, xRayDir, config.fBackfaceThreshold, xNormalsTex, xViewMat))
					{
						// Backface hit - reject and continue marching
						fPrevT = fT;
						float fStepSize = max(xTexelSize.x, xTexelSize.y) * config.fStepSizeMultiplier;
						fT += fStepSize / fMaxScreenDist;
						bFirstSample = false;
						continue;
					}

					// Compute confidence
					float fFinalInvZ = mix(fInvZStart, fInvZEnd, fFinalT);
					float fFinalRayZ = 1.0 / fFinalInvZ;
					float fDepthError = abs(fFinalRayZ - fFinalSceneZ);

					vec3 xHitWorldNormal = texture(xNormalsTex, xFinalUV).rgb;
					vec3 xHitViewNormal = normalize(mat3(xViewMat) * xHitWorldNormal);
					float fBackfaceDot = dot(xHitViewNormal, xRayDir);

					float fConfidence = ComputeHitConfidence(xFinalUV, fFinalT, fDepthError, fBackfaceDot, config);

					// Return successful hit
					result.xHitUV = xFinalUV;
					result.fHitDepth = fFinalSceneDepth;
					result.fConfidence = fConfidence;
					result.bHit = true;
					fMarchDistance = fFinalT;
					return result;
				}
				else
				{
					// Behind surface but outside thickness - step back and try smaller step
					fT = fPrevT;
					float fStepSize = max(xTexelSize.x, xTexelSize.y) * 0.5;
					fPrevT = fT;
					fT += fStepSize / fMaxScreenDist;
				}
			}
			else
			{
				// Behind surface at coarse mip - refine to finer mip
				fT = max(0.0, fT - exp2(float(iCurrentMip - 1)) * max(xTexelSize.x, xTexelSize.y) / fMaxScreenDist);
				iCurrentMip--;
			}
		}
		else
		{
			// Ray is in front of surface - step forward
			float fMipScale = exp2(float(iCurrentMip));
			float fStepSize = fMipScale * max(xTexelSize.x, xTexelSize.y) * config.fStepSizeMultiplier;

			fPrevDepthDiff = fDepthDiff;
			fPrevT = fT;
			fT += fStepSize / fMaxScreenDist;

			if (fT >= 1.0)
				break;
		}

		bFirstSample = false;
	}

	return result;
}

#endif // RAY_MARCH_COMMON_FXH
