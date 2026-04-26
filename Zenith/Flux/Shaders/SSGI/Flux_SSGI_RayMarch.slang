// =====================================================================
// Flux_SSGI_RayMarch — Slang module.
//
// Subsystem: SSGI. Half-resolution screen-space global illumination via
// HiZ-accelerated ray marching. For each pixel, shoots N cosine-weighted
// hemisphere rays, traverses the depth pyramid (logarithmic step count),
// refines hits with binary search, then samples the G-buffer diffuse at
// the hit position. Output RGBA: RGB = indirect colour, A = confidence.
//
// Imports Common.Frame (FrameConstants, GetViewPosFromDepth,
// DepthToViewZ, ViewToScreen, ComputeEdgeFade, BLUE_NOISE_SIZE,
// GOLDEN_RATIO) and Common.PBR (TWO_PI, PBR_DIRECTION_EPSILON).
//
// Multi-ray sampling: u_uRaysPerPixel rays per pixel, each offset by
// golden ratio in noise space for low-discrepancy distribution.
// Confidence-weighted averaging keeps blending stable across ray counts.
// =====================================================================

import Common.Fullscreen;
import Common.Frame;
import Common.PBR;

// ----- SSGI tuning constants -----

static const float SSGI_NORMAL_UP_THRESHOLD       = 0.999;
static const float SSGI_START_OFFSET_MIN          = 0.1;
static const float SSGI_START_OFFSET_SCALE        = 0.01;
static const float SSGI_MIN_SCREEN_TRACE_DIST     = 0.01;
static const float SSGI_MAX_SCREEN_DIST           = 0.6;
static const float SSGI_SKY_DEPTH_THRESHOLD       = 0.9999;
static const float SSGI_STEP_SIZE_MULTIPLIER      = 2.0;
static const int   SSGI_BINARY_SEARCH_ITERATIONS  = 4;
static const float SSGI_BINARY_SEARCH_TOLERANCE   = 0.001;
// Roughness-driven mip clamping — match SSR's cone-tracing approach.
// Diffuse GI is broader than specular so we use a smaller scale.
static const float SSGI_ROUGHNESS_MIP_SCALE       = 2.0;
static const int   SSGI_MAX_ROUGHNESS_MIP         = 2;
static const float SSGI_BACKFACE_THRESHOLD        = 0.1;
static const float SSGI_DEPTH_VALID_MIN           = 0.1;
static const float SSGI_EDGE_MARGIN               = 0.15;
static const float SSGI_BACKFACE_SMOOTHSTEP_MIN   = 0.1;
static const float SSGI_BACKFACE_SMOOTHSTEP_MAX   = -0.3;
// Floor at 0.02 — minimises invalid-hit glow without losing temporal stability.
static const float SSGI_MIN_CONFIDENCE            = 0.02;
static const float SSGI_DISTANCE_FADE_START       = 0.4;
static const float SSGI_DISTANCE_FADE_END         = 0.9;
static const float SSGI_HORIZON_FADE_SCALE        = 5.0;

// Debug-mode constants
static const uint SSGI_DEBUG_NONE                 = 0u;
static const uint SSGI_DEBUG_RAY_DIRECTIONS       = 1u;
static const uint SSGI_DEBUG_HIT_POSITIONS        = 2u;
static const uint SSGI_DEBUG_CONFIDENCE           = 3u;
static const uint SSGI_DEBUG_TEMPORAL_WEIGHT      = 4u;
static const uint SSGI_DEBUG_FINAL_RESULT         = 5u;

struct SSGIConstantsLayout
{
	float u_fIntensity;
	float u_fMaxDistance;
	float u_fThickness;
	uint  u_uStepCount;
	uint  u_uFrameIndex;
	uint  u_uHiZMipCount;
	uint  u_uDebugMode;
	float u_fRoughnessThreshold;
	uint  u_uStartMip;
	uint  u_uRaysPerPixel;
	float _pad0;
	float _pad1;
};
[[vk::binding(1, 0)]] ConstantBuffer<SSGIConstantsLayout> SSGIConstants;
[[vk::binding(2, 0)]] Sampler2D g_xDepthTex;
[[vk::binding(3, 0)]] Sampler2D g_xNormalsTex;
[[vk::binding(4, 0)]] Sampler2D g_xMaterialTex;
[[vk::binding(5, 0)]] Sampler2D g_xHiZTex;
[[vk::binding(6, 0)]] Sampler2D g_xDiffuseTex;
[[vk::binding(7, 0)]] Sampler2D g_xBlueNoiseTex;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

float2 GetBlueNoise(int2 xPixelCoord, uint uFrameIndex)
{
	float2 xNoiseUV = float2(xPixelCoord % int2(BLUE_NOISE_SIZE, BLUE_NOISE_SIZE)) / (float)BLUE_NOISE_SIZE;
	float2 xNoise   = g_xBlueNoiseTex.Sample(xNoiseUV).rg;
	xNoise          = frac(xNoise + (float)(uFrameIndex % 64u) * GOLDEN_RATIO);
	return xNoise;
}

// Frisvad orthonormal basis from a unit normal — produces continuous
// tangent frames without the orientation flip that cross(up, N) hits
// near +Z / −Z.
void BuildOrthonormalBasisFrisvad(float3 n, out float3 b1, out float3 b2)
{
	if (n.z < -0.9999999)
	{
		// Singularity at n = (0, 0, −1) — use the documented fallback.
		b1 = float3(0.0, -1.0, 0.0);
		b2 = float3(-1.0, 0.0, 0.0);
	}
	else
	{
		float a = 1.0 / (1.0 + n.z);
		float b = -n.x * n.y * a;
		b1 = float3(1.0 - n.x * n.x * a, b, -n.x);
		b2 = float3(b, 1.0 - n.y * n.y * a, -n.y);
	}
}

// Cosine-weighted hemisphere sample — importance samples the Lambertian
// BRDF (PDF = cos(theta) / PI). Maps Xi ∈ [0,1)² to hemisphere around N.
float3 SampleHemisphereCosine(float2 Xi, float3 N)
{
	float fCosTheta = sqrt(1.0 - Xi.x);
	float fSinTheta = sqrt(Xi.x);
	float fPhi      = TWO_PI * Xi.y;

	float3 H = float3(fSinTheta * cos(fPhi), fSinTheta * sin(fPhi), fCosTheta);

	float3 tangent, bitangent;
	BuildOrthonormalBasisFrisvad(N, tangent, bitangent);

	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

// Hierarchical HiZ ray march. fRoughness drives the minimum mip the
// traversal will descend to (rougher surfaces sample coarser mips —
// matches SSR cone-tracing). Returns:
//   xy = hit UV, z = hit depth, w = confidence (0 if no hit)
float4 RayMarch(float3 xViewOrigin,
				float3 xRayDir,
				float3 xViewNormal,
				float  fRoughness,
				out float fMarchDistance,
				out float2 xScreenDir)
{
	fMarchDistance = 0.0;
	xScreenDir     = float2(0.0, 0.0);

	float fStartOffset = max(SSGI_START_OFFSET_MIN, abs(xViewOrigin.z) * SSGI_START_OFFSET_SCALE);
	float3 xViewStart  = xViewOrigin + xRayDir * fStartOffset;

	float3 xScreenStart = ViewToScreen(xViewStart);

	// Sample a probe point along the ray to derive screen-space direction.
	float fRaySampleDist = min(SSGIConstants.u_fMaxDistance * 0.5, xViewStart.z * 0.5);
	float3 xRaySampleView = xViewStart + xRayDir * fRaySampleDist;

	// Clamp probe to stay in front of the near plane — unbounded reciprocal-z
	// interpolation past the near plane breaks the perspective math.
	float fNearPlane = FrameConstants.g_xCameraNearFar.x;
	if (xRaySampleView.z < fNearPlane)
	{
		if (abs(xRayDir.z) > PBR_DIRECTION_EPSILON)
		{
			float fSafeT = (fNearPlane - xViewStart.z) / xRayDir.z * 0.9;
			if (fSafeT > 0.0) xRaySampleView = xViewStart + xRayDir * fSafeT;
			else              xRaySampleView = xViewStart + xRayDir * 0.1;
		}
		else
		{
			xRaySampleView = xViewStart + xRayDir * 0.1;
		}
	}

	float3 xRaySampleScreen = ViewToScreen(xRaySampleView);
	float2 xScreenDelta     = xRaySampleScreen.xy - xScreenStart.xy;
	float  fDeltaLen        = length(xScreenDelta);

	if (fDeltaLen > 0.001)
	{
		xScreenDir = xScreenDelta / fDeltaLen;
	}
	else
	{
		// Ray going straight into / out of the screen — preserve the sign of
		// the non-zero ray components so we don't bias toward (+x, +y).
		float2 xDir = float2(xRayDir.x, xRayDir.y);
		float2 xEps = float2(PBR_DIRECTION_EPSILON, PBR_DIRECTION_EPSILON);
		if (xDir.x < 0.0) xEps.x = -PBR_DIRECTION_EPSILON;
		if (xDir.y < 0.0) xEps.y = -PBR_DIRECTION_EPSILON;
		xScreenDir = normalize(xDir + xEps);
	}

	float fMaxScreenDist = SSGI_MAX_SCREEN_DIST;

	// Distance along screenDir before the ray exits [0,1]² in UV space.
	float2 xExitDist = float2(1e10, 1e10);
	if (abs(xScreenDir.x) > PBR_DIRECTION_EPSILON)
	{
		xExitDist.x = xScreenDir.x > 0.0 ? (1.0 - xScreenStart.x) / xScreenDir.x
										 : -xScreenStart.x / xScreenDir.x;
	}
	if (abs(xScreenDir.y) > PBR_DIRECTION_EPSILON)
	{
		xExitDist.y = xScreenDir.y > 0.0 ? (1.0 - xScreenStart.y) / xScreenDir.y
										 : -xScreenStart.y / xScreenDir.y;
	}
	fMaxScreenDist = min(fMaxScreenDist, min(xExitDist.x, xExitDist.y));
	fMaxScreenDist = max(fMaxScreenDist, SSGI_MIN_SCREEN_TRACE_DIST);

	// Reciprocal-Z interpolation endpoints — gives perspective-correct depth
	// at every fT along the screen-space line.
	float fSafeStartZ      = max(xViewStart.z, FrameConstants.g_xCameraNearFar.x);
	float fInvZStart       = 1.0 / fSafeStartZ;
	float fEstimatedRayDist = SSGIConstants.u_fMaxDistance * fMaxScreenDist;
	float fEstimatedEndZ   = max(FrameConstants.g_xCameraNearFar.x,
								  fSafeStartZ + xRayDir.z * fEstimatedRayDist);
	float fInvZEnd         = 1.0 / fEstimatedEndZ;

	// Texel size for HiZ stepping.
	uint uW0, uH0, uMips0;
	g_xHiZTex.GetDimensions(0, uW0, uH0, uMips0);
	float2 xScreenSize = float2((float)uW0, (float)uH0);
	float2 xTexelSize  = 1.0 / xScreenSize;

	int   iCurrentMip   = (int)min(SSGIConstants.u_uStartMip, SSGIConstants.u_uHiZMipCount - 1u);
	float fT            = 0.0;
	float fPrevT        = 0.0;
	float fPrevDepthDiff = 0.0;
	bool  bFirstSample  = true;

	int iMinMip = clamp((int)floor(fRoughness * SSGI_ROUGHNESS_MIP_SCALE), 0, SSGI_MAX_ROUGHNESS_MIP);

	for (uint uIter = 0u; uIter < SSGIConstants.u_uStepCount; uIter++)
	{
		float2 xSampleUV = xScreenStart.xy + xScreenDir * fT * fMaxScreenDist;

		if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
			xSampleUV.y < 0.0 || xSampleUV.y > 1.0) break;

		uint uMW, uMH, uMipsM;
		g_xHiZTex.GetDimensions(iCurrentMip, uMW, uMH, uMipsM);
		float2 xMipSize    = float2((float)uMW, (float)uMH);
		float2 xHalfTexel  = 0.5 / xMipSize;
		float2 xClampedUV  = clamp(xSampleUV, xHalfTexel, 1.0 - xHalfTexel);
		float2 xHiZSample  = g_xHiZTex.SampleLevel(xClampedUV, (float)iCurrentMip).rg;
		float  fMinDepth   = xHiZSample.r;

		// Sky pixels: fast-forward by mip-scaled step.
		if (fMinDepth >= SSGI_SKY_DEPTH_THRESHOLD)
		{
			float fMipScale = exp2((float)iCurrentMip);
			float fStepSize = fMipScale * max(xTexelSize.x, xTexelSize.y) * SSGI_STEP_SIZE_MULTIPLIER;
			fPrevT = fT;
			fT    += fStepSize / fMaxScreenDist;
			if (fT >= 1.0) break;
			continue;
		}

		float fMinViewZ = DepthToViewZ(xSampleUV, fMinDepth);

		// Perspective-correct ray depth at this fT (reciprocal-Z LERP).
		float fInvZ      = lerp(fInvZStart, fInvZEnd, fT);
		float fRayViewZ  = 1.0 / fInvZ;
		float fDepthDiff = fRayViewZ - fMinViewZ;

		float fMipThickness = SSGIConstants.u_fThickness * exp2((float)iCurrentMip);

		bool bBehindSurface  = fDepthDiff > 0.0;
		bool bWithinThickness = fDepthDiff < fMipThickness * SSGI_STEP_SIZE_MULTIPLIER;
		bool bValidDepth     = fMinViewZ > SSGI_DEPTH_VALID_MIN;

		if (bBehindSurface && bValidDepth)
		{
			if (iCurrentMip <= iMinMip)
			{
				if (bWithinThickness || (!bFirstSample && fPrevDepthDiff * fDepthDiff < 0.0))
				{
					// Binary-search refinement on the [prevT, T] interval.
					float fSearchStart = fPrevT;
					float fSearchEnd   = fT;

					for (int j = 0; j < SSGI_BINARY_SEARCH_ITERATIONS; j++)
					{
						if (abs(fSearchEnd - fSearchStart) < SSGI_BINARY_SEARCH_TOLERANCE) break;

						float fMidT          = (fSearchStart + fSearchEnd) * 0.5;
						float2 xMidUV        = xScreenStart.xy + xScreenDir * fMidT * fMaxScreenDist;
						float fMidSceneDepth = g_xHiZTex.SampleLevel(xMidUV, (float)iMinMip).r;
						float fMidSceneZ     = DepthToViewZ(xMidUV, fMidSceneDepth);

						float fMidInvZ = lerp(fInvZStart, fInvZEnd, fMidT);
						float fMidRayZ = 1.0 / fMidInvZ;
						float fMidDiff = fMidRayZ - fMidSceneZ;

						if (fMidDiff > 0.0) fSearchEnd   = fMidT;
						else                fSearchStart = fMidT;
					}

					float fFinalT          = (fSearchStart + fSearchEnd) * 0.5;
					float2 xFinalUV        = xScreenStart.xy + xScreenDir * fFinalT * fMaxScreenDist;
					float fFinalSceneDepth = g_xHiZTex.SampleLevel(xFinalUV, (float)iMinMip).r;
					float fFinalSceneZ     = DepthToViewZ(xFinalUV, fFinalSceneDepth);

					// Backface hits aren't visible from the camera — skip.
					float3 xHitWorldNormal = g_xNormalsTex.Sample(xFinalUV).rgb;
					float3 xHitViewNormal  = normalize(mul((float3x3)FrameConstants.g_xViewMat, xHitWorldNormal));
					float  fBackfaceDot    = dot(xHitViewNormal, xRayDir);
					if (fBackfaceDot > SSGI_BACKFACE_THRESHOLD)
					{
						fPrevT = fT;
						float fStepSize = max(xTexelSize.x, xTexelSize.y) * SSGI_STEP_SIZE_MULTIPLIER;
						fT += fStepSize / fMaxScreenDist;
						bFirstSample = false;
						continue;
					}

					// Confidence accumulation. Multiplicative — any single
					// failing factor (off-screen, far ray, deep miss, grazing,
					// horizon) suppresses the hit.
					float fEdgeFade        = ComputeEdgeFade(xFinalUV, SSGI_EDGE_MARGIN);
					float fDistanceFade    = 1.0 - smoothstep(SSGI_DISTANCE_FADE_START, SSGI_DISTANCE_FADE_END, fFinalT);

					float fFinalInvZ       = lerp(fInvZStart, fInvZEnd, fFinalT);
					float fFinalRayZ       = 1.0 / fFinalInvZ;
					float fDepthError      = abs(fFinalRayZ - fFinalSceneZ);
					float fDepthConfidence = 1.0 - smoothstep(0.0, SSGIConstants.u_fThickness * SSGI_STEP_SIZE_MULTIPLIER, fDepthError);

					float fBackfaceConfidence = smoothstep(SSGI_BACKFACE_SMOOTHSTEP_MIN, SSGI_BACKFACE_SMOOTHSTEP_MAX, fBackfaceDot);

					float fNdotR        = max(dot(xViewNormal, xRayDir), 0.0);
					float fHorizonFade  = saturate(fNdotR * SSGI_HORIZON_FADE_SCALE);

					float fConfidence = fEdgeFade * fDistanceFade * fDepthConfidence * fBackfaceConfidence * fHorizonFade;
					fConfidence       = max(fConfidence, SSGI_MIN_CONFIDENCE);

					fMarchDistance = fFinalT;
					return float4(xFinalUV, fFinalSceneDepth, fConfidence);
				}
				else
				{
					// Behind surface but outside thickness band — back up half a step
					// and keep marching.
					fT = fPrevT;
					float fStepSize = max(xTexelSize.x, xTexelSize.y) * 0.5;
					fPrevT = fT;
					fT += fStepSize / fMaxScreenDist;
				}
			}
			else
			{
				// Refine — back off a little so the next iteration can hit
				// the surface at a finer mip.
				fT = max(0.0, fT - exp2((float)(iCurrentMip - 1)) * max(xTexelSize.x, xTexelSize.y) / fMaxScreenDist);
				iCurrentMip--;
			}
		}
		else
		{
			float fMipScale = exp2((float)iCurrentMip);
			float fStepSize = fMipScale * max(xTexelSize.x, xTexelSize.y) * SSGI_STEP_SIZE_MULTIPLIER;

			fPrevDepthDiff = fDepthDiff;
			fPrevT = fT;
			fT += fStepSize / fMaxScreenDist;

			if (fT >= 1.0) break;
		}

		bFirstSample = false;
	}

	return float4(0.0, 0.0, 0.0, 0.0);
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input, float4 fragCoord : SV_Position) : SV_Target
{
	float fDepth = g_xDepthTex.Sample(input.uv).r;
	if (fDepth >= 1.0) return float4(0.0, 0.0, 0.0, 0.0); // sky → no GI

	float3 xWorldNormal = g_xNormalsTex.Sample(input.uv).rgb;
	float4 xMaterial    = g_xMaterialTex.Sample(input.uv);
	float  fRoughness   = xMaterial.r;

	// Skip very-smooth surfaces — SSR handles them better.
	if (fRoughness < SSGIConstants.u_fRoughnessThreshold) return float4(0.0, 0.0, 0.0, 0.0);

	float3 xViewPos    = GetViewPosFromDepth(input.uv, fDepth);
	float3 xViewNormal = normalize(mul((float3x3)FrameConstants.g_xViewMat, xWorldNormal));

	int2 xPixelCoord = (int2)fragCoord.xy;

	if (SSGIConstants.u_uDebugMode == SSGI_DEBUG_RAY_DIRECTIONS)
	{
		float2 xNoise     = GetBlueNoise(xPixelCoord, SSGIConstants.u_uFrameIndex);
		float3 xSampleDir = SampleHemisphereCosine(xNoise, xViewNormal);
		return float4(xSampleDir * 0.5 + 0.5, 1.0);
	}

	// Multi-ray hemisphere sampling. Each ray gets a golden-ratio-shifted
	// noise sample so the set has low-discrepancy coverage of [0,1)².
	float3 xAccumulatedColour    = float3(0.0, 0.0, 0.0);
	float  fAccumulatedConfidence = 0.0;
	uint   uValidHits             = 0u;
	uint   uRayCount              = max(SSGIConstants.u_uRaysPerPixel, 1u);

	for (uint uRay = 0u; uRay < uRayCount; uRay++)
	{
		float2 xNoise     = GetBlueNoise(xPixelCoord, SSGIConstants.u_uFrameIndex);
		xNoise            = frac(xNoise + (float)uRay * GOLDEN_RATIO);
		float3 xSampleDir = SampleHemisphereCosine(xNoise, xViewNormal);

		float fMarchDistance = 0.0;
		float2 xScreenDir    = float2(0.0, 0.0);
		float4 xHitResult    = RayMarch(xViewPos, xSampleDir, xViewNormal, fRoughness, fMarchDistance, xScreenDir);

		if (xHitResult.w > 0.0)
		{
			float3 xIndirectColour = g_xDiffuseTex.Sample(xHitResult.xy).rgb;
			xAccumulatedColour     += xIndirectColour * xHitResult.w;
			fAccumulatedConfidence += xHitResult.w;
			uValidHits++;
		}
	}

	if (SSGIConstants.u_uDebugMode == SSGI_DEBUG_CONFIDENCE)
	{
		float fAvgConfidence = (uValidHits > 0u) ? (fAccumulatedConfidence / (float)uValidHits) : 0.0;
		return float4(fAvgConfidence, fAvgConfidence, fAvgConfidence, 1.0);
	}

	if (SSGIConstants.u_uDebugMode == SSGI_DEBUG_HIT_POSITIONS && uValidHits > 0u)
	{
		float2 xNoise     = GetBlueNoise(xPixelCoord, SSGIConstants.u_uFrameIndex);
		float3 xSampleDir = SampleHemisphereCosine(xNoise, xViewNormal);
		float fMarchDistance = 0.0;
		float2 xScreenDir    = float2(0.0, 0.0);
		float4 xHitResult    = RayMarch(xViewPos, xSampleDir, xViewNormal, fRoughness, fMarchDistance, xScreenDir);
		if (xHitResult.w > 0.0)
		{
			float3 xHitWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, xHitResult.xy);
			return float4(xHitWorldPos / 100.0, 1.0);
		}
	}

	if (uValidHits > 0u)
	{
		float3 xFinalColour     = (xAccumulatedColour / fAccumulatedConfidence) * SSGIConstants.u_fIntensity;
		// Confidence is the avg of valid hits — not diluted by ray count —
		// so blending downstream is consistent across u_uRaysPerPixel.
		float fFinalConfidence = fAccumulatedConfidence / (float)uValidHits;
		return float4(xFinalColour, fFinalConfidence);
	}
	return float4(0.0, 0.0, 0.0, 0.0);
}
