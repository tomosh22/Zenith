// =====================================================================
// Flux_SSR_RayMarch — Slang module.
//
// Subsystem: SSR. Half-resolution screen-space reflections via HiZ-
// accelerated ray marching. Reflection ray is GGX-perturbed for rough
// surfaces, then traverses the depth pyramid (binary-search refined),
// and the final hit colour comes from the G-buffer diffuse target.
//
// Output RGBA: RGB = colour, A = confidence (used by Resolve for
// roughness-driven contact-hardening blur and by deferred for IBL blend).
//
// Imports Common.Frame (matrices, depth helpers, blue noise constants)
// and Common.PBR (ImportanceSampleGGX, PBR_DIRECTION_EPSILON).
// =====================================================================

import Common.Fullscreen;
import Common.Frame;
import Common.PBR;

// ----- SSR tuning constants -----

static const float SSR_NORMAL_UP_THRESHOLD       = 0.999;
static const float SSR_START_OFFSET_MIN          = 0.1;
static const float SSR_START_OFFSET_SCALE        = 0.01;
static const float SSR_MIN_SCREEN_TRACE_DIST     = 0.01;
static const float SSR_MAX_SCREEN_DIST_BASE      = 0.8;
// Trace full distance regardless of roughness — rough surfaces use
// coarser HiZ mips (cone tracing), not shorter rays.
static const float SSR_ROUGHNESS_DIST_SCALE      = 0.0;
static const float SSR_SKY_DEPTH_THRESHOLD       = 0.9999;
static const float SSR_STEP_SIZE_MULTIPLIER      = 2.0;

// Binary-search refinement — 6 iterations gives 1/64 sub-pixel precision
// at 1080p. Resolution-aware count comes through u_uBinarySearchIterations
// (typically 6/7/8 for 1080p/1440p/4K).
static const int   SSR_BINARY_SEARCH_ITERATIONS         = 6;
static const float SSR_BINARY_SEARCH_RELATIVE_TOLERANCE = 0.0001;
static const float SSR_BINARY_SEARCH_MIN_TOLERANCE      = 0.0001;

// Roughness-driven cone tracing. Roughness ∈ [0, 1] maps to mip 0..3.
static const float SSR_ROUGHNESS_MIP_SCALE       = 2.5;
static const int   SSR_MAX_ROUGHNESS_MIP         = 3;

static const float SSR_BACKFACE_THRESHOLD        = 0.1;
static const float SSR_DEPTH_VALID_MIN           = 0.1;

static const float SSR_EDGE_MARGIN_BASE          = 0.15;
static const float SSR_EDGE_MARGIN_ROUGHNESS     = 0.10;
static const float SSR_BACKFACE_SMOOTHSTEP_MIN   = 0.1;
static const float SSR_BACKFACE_SMOOTHSTEP_MAX   = -0.3;
static const float SSR_MIN_CONFIDENCE            = 0.1;

static const float SSR_DISTANCE_FADE_START       = 0.3;
static const float SSR_DISTANCE_FADE_END         = 0.8;

// Contact hardening: world-space reflection travel distance fades the
// confidence smoothly. Default 2 m for human-scale interiors; runtime
// override via u_fContactHardeningDist.
static const float SSR_CONTACT_ROUGHNESS_SCALE_MAX = 2.0;
static const float SSR_CONTACT_CONFIDENCE_SCALE  = 0.6;

// Roughness confidence falloff — let the deferred shader blend toward IBL.
static const float SSR_ROUGHNESS_CONF_START      = 0.15;
static const float SSR_ROUGHNESS_CONF_END        = 0.7;
// Metallic boost intentionally disabled — Fresnel handled in deferred.
static const float SSR_METALLIC_BOOST            = 1.0;

static const float SSR_ROUGHNESS_PERTURB_THRESHOLD = 0.05;
static const float SSR_HORIZON_FADE_SCALE        = 5.0;

// Debug modes.
static const uint SSR_DEBUG_NONE              = 0u;
static const uint SSR_DEBUG_RAY_DIRECTIONS    = 1u;
static const uint SSR_DEBUG_SCREEN_DIRECTIONS = 2u;
static const uint SSR_DEBUG_HIT_POSITIONS     = 3u;
static const uint SSR_DEBUG_REFLECTION_UVS   = 4u;
static const uint SSR_DEBUG_CONFIDENCE       = 5u;
static const uint SSR_DEBUG_DEPTH_COMPARISON = 6u;
static const uint SSR_DEBUG_EDGE_FADE        = 7u;
static const uint SSR_DEBUG_MARCH_DISTANCE   = 8u;
static const uint SSR_DEBUG_FINAL_RESULT     = 9u;
static const uint SSR_DEBUG_ROUGHNESS        = 10u;
static const uint SSR_DEBUG_WORLD_NORMAL_Y   = 11u;

struct SSRConstantsLayout
{
	float u_fIntensity;
	float u_fMaxDistance;
	float u_fMaxRoughness;
	float u_fThickness;
	uint  u_uStepCount;
	uint  u_uDebugMode;
	uint  u_uHiZMipCount;
	uint  u_uStartMip;
	uint  u_uFrameIndex;
	uint  u_uBinarySearchIterations;
	float u_fContactHardeningDist;
	// Half-res ray-march output dimensions — populated CPU-side, never
	// queried with GetDimensions in the shader (project convention).
	float u_fHalfResWidth;
	float u_fHalfResHeight;
	float u_fRcpHalfResWidth;
	float u_fRcpHalfResHeight;
	float u_fPad0;
	// Cached HiZ per-mip dimensions: xy = (w, h), zw = (1/w, 1/h).
	// Replaces inner-loop GetDimensions calls.
	float4 u_axHiZMipSizes[12];
};
[[vk::binding(1, 0)]] ConstantBuffer<SSRConstantsLayout> SSRConstants;
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
	xNoise          = frac(xNoise + (float)uFrameIndex * GOLDEN_RATIO);
	return xNoise;
}

// HiZ ray march. Returns hit UV (xy), hit depth (z), confidence (w);
// confidence = 0 means no hit.
float4 RayMarch(float3 xViewOrigin,
				float3 xReflectDir,
				float3 xViewNormal,
				float  fRoughness,
				float  fNdotV,
				out float fMarchDistance,
				out float2 xScreenDir)
{
	fMarchDistance = 0.0;
	xScreenDir     = float2(0.0, 0.0);

	float fStartOffset = max(SSR_START_OFFSET_MIN, abs(xViewOrigin.z) * SSR_START_OFFSET_SCALE);
	float3 xViewStart  = xViewOrigin + xReflectDir * fStartOffset;

	float3 xScreenStart = ViewToScreen(xViewStart);

	float fRaySampleDist = min(SSRConstants.u_fMaxDistance * 0.5, xViewStart.z * 0.5);
	float3 xRaySampleView = xViewStart + xReflectDir * fRaySampleDist;

	float fNearPlane = FrameConstants.g_xCameraNearFar.x;
	if (xRaySampleView.z < fNearPlane)
	{
		if (abs(xReflectDir.z) > PBR_DIRECTION_EPSILON)
		{
			float fSafeT = (fNearPlane - xViewStart.z) / xReflectDir.z * 0.9;
			if (fSafeT > 0.0) xRaySampleView = xViewStart + xReflectDir * fSafeT;
			else              xRaySampleView = xViewStart + xReflectDir * 0.1;
		}
		else
		{
			xRaySampleView = xViewStart + xReflectDir * 0.1;
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
		float2 xDir = float2(xReflectDir.x, xReflectDir.y);
		float2 xEps = float2(PBR_DIRECTION_EPSILON, PBR_DIRECTION_EPSILON);
		if (xDir.x < 0.0) xEps.x = -PBR_DIRECTION_EPSILON;
		if (xDir.y < 0.0) xEps.y = -PBR_DIRECTION_EPSILON;
		xScreenDir = normalize(xDir + xEps);
	}

	float fRoughnessScale = 1.0 - fRoughness * fRoughness * SSR_ROUGHNESS_DIST_SCALE;
	float fMaxScreenDist  = SSR_MAX_SCREEN_DIST_BASE * fRoughnessScale;

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
	fMaxScreenDist = max(fMaxScreenDist, SSR_MIN_SCREEN_TRACE_DIST);

	// Reciprocal-Z LERP endpoints for perspective-correct depth at every fT.
	float fSafeStartZ      = max(xViewStart.z, FrameConstants.g_xCameraNearFar.x);
	float fInvZStart       = 1.0 / fSafeStartZ;
	float fEstimatedRayDist = SSRConstants.u_fMaxDistance * fMaxScreenDist;
	float fEstimatedEndZ   = max(FrameConstants.g_xCameraNearFar.x,
								  fSafeStartZ + xReflectDir.z * fEstimatedRayDist);
	float fInvZEnd         = 1.0 / fEstimatedEndZ;

	// Mip 0 dimensions from the cached CBV array — never query the
	// texture itself (project convention: dims flow through the CBV).
	float2 xScreenSize = SSRConstants.u_axHiZMipSizes[0].xy;
	float2 xTexelSize  = SSRConstants.u_axHiZMipSizes[0].zw;

	int   iCurrentMip   = (int)min(SSRConstants.u_uStartMip, SSRConstants.u_uHiZMipCount - 1u);
	float fT            = 0.0;
	float fPrevT        = 0.0;
	float fPrevDepthDiff = 0.0;
	bool  bFirstSample  = true;

	int iMinMip = clamp((int)floor(fRoughness * SSR_ROUGHNESS_MIP_SCALE), 0, SSR_MAX_ROUGHNESS_MIP);

	for (uint uIter = 0u; uIter < SSRConstants.u_uStepCount; uIter++)
	{
		float2 xSampleUV = xScreenStart.xy + xScreenDir * fT * fMaxScreenDist;

		if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
			xSampleUV.y < 0.0 || xSampleUV.y > 1.0) break;

		// Inner loop hot path — read mip dims from the cached CBV array.
		float2 xMipSize   = SSRConstants.u_axHiZMipSizes[iCurrentMip].xy;
		float2 xHalfTexel = SSRConstants.u_axHiZMipSizes[iCurrentMip].zw * 0.5;
		float2 xClampedUV  = clamp(xSampleUV, xHalfTexel, 1.0 - xHalfTexel);
		float2 xHiZSample  = g_xHiZTex.SampleLevel(xClampedUV, (float)iCurrentMip).rg;
		float  fMinDepth   = xHiZSample.r;
		float  fMaxDepth   = xHiZSample.g;

		// Sky pixels — fast-forward.
		if (fMinDepth >= SSR_SKY_DEPTH_THRESHOLD)
		{
			float fMipScale = exp2((float)iCurrentMip);
			float fStepSize = fMipScale * max(xTexelSize.x, xTexelSize.y) * SSR_STEP_SIZE_MULTIPLIER;
			fPrevT = fT;
			fT    += fStepSize / fMaxScreenDist;
			if (fT >= 1.0) break;
			continue;
		}

		float fMinViewZ = DepthToViewZ(xSampleUV, fMinDepth);
		// fMaxViewZ is computed but only fed into the conservative thickness
		// test — we still drive the binary search off the min-depth surface.

		float fInvZ      = lerp(fInvZStart, fInvZEnd, fT);
		float fRayViewZ  = 1.0 / fInvZ;
		float fMinDiff   = fRayViewZ - fMinViewZ;
		float fMipThickness = SSRConstants.u_fThickness * exp2((float)iCurrentMip);

		bool bBehindMinSurface  = fMinDiff > 0.0;
		bool bWithinThickness   = fMinDiff < fMipThickness * SSR_STEP_SIZE_MULTIPLIER;
		bool bValidDepth        = fMinViewZ > SSR_DEPTH_VALID_MIN;
		float fDepthDiff        = fMinDiff;

		if (bBehindMinSurface && bValidDepth)
		{
			if (iCurrentMip <= iMinMip)
			{
				if (bWithinThickness || (!bFirstSample && fPrevDepthDiff * fDepthDiff < 0.0))
				{
					float fSearchStart = fPrevT;
					float fSearchEnd   = fT;
					float fSearchMip   = (float)iMinMip;

					float fInitialRange = fSearchEnd - fSearchStart;
					float fTolerance    = max(fInitialRange * SSR_BINARY_SEARCH_RELATIVE_TOLERANCE,
											  SSR_BINARY_SEARCH_MIN_TOLERANCE);

					for (int j = 0; j < (int)SSRConstants.u_uBinarySearchIterations; j++)
					{
						if (abs(fSearchEnd - fSearchStart) < fTolerance) break;

						float fMidT          = (fSearchStart + fSearchEnd) * 0.5;
						float2 xMidUV        = xScreenStart.xy + xScreenDir * fMidT * fMaxScreenDist;
						float fMidSceneDepth = g_xHiZTex.SampleLevel(xMidUV, fSearchMip).r;
						float fMidSceneZ     = DepthToViewZ(xMidUV, fMidSceneDepth);

						float fMidInvZ = lerp(fInvZStart, fInvZEnd, fMidT);
						float fMidRayZ = 1.0 / fMidInvZ;
						float fMidDiff = fMidRayZ - fMidSceneZ;

						if (fMidDiff > 0.0) fSearchEnd   = fMidT;
						else                fSearchStart = fMidT;
					}

					float fFinalT          = (fSearchStart + fSearchEnd) * 0.5;
					float2 xFinalUV        = xScreenStart.xy + xScreenDir * fFinalT * fMaxScreenDist;
					float fFinalSceneDepth = g_xHiZTex.SampleLevel(xFinalUV, fSearchMip).r;
					float fFinalSceneZ     = DepthToViewZ(xFinalUV, fFinalSceneDepth);

					float3 xHitWorldNormal = g_xNormalsTex.Sample(xFinalUV).rgb;
					float3 xHitViewNormal  = normalize(mul((float3x3)FrameConstants.g_xViewMat, xHitWorldNormal));
					float  fBackfaceDot    = dot(xHitViewNormal, xReflectDir);
					if (fBackfaceDot > SSR_BACKFACE_THRESHOLD)
					{
						fPrevT = fT;
						float fStepSize = max(xTexelSize.x, xTexelSize.y) * SSR_STEP_SIZE_MULTIPLIER;
						fT += fStepSize / fMaxScreenDist;
						bFirstSample = false;
						continue;
					}

					float fEdgeMargin   = SSR_EDGE_MARGIN_BASE + fRoughness * SSR_EDGE_MARGIN_ROUGHNESS;
					float fEdgeFade     = ComputeEdgeFade(xFinalUV, fEdgeMargin);
					float fDistanceFade = 1.0 - smoothstep(SSR_DISTANCE_FADE_START, SSR_DISTANCE_FADE_END, fFinalT);

					float fFinalInvZ       = lerp(fInvZStart, fInvZEnd, fFinalT);
					float fFinalRayZ       = 1.0 / fFinalInvZ;
					float fDepthError      = abs(fFinalRayZ - fFinalSceneZ);
					float fDepthConfidence = 1.0 - smoothstep(0.0, SSRConstants.u_fThickness * SSR_STEP_SIZE_MULTIPLIER, fDepthError);

					float fBackfaceConfidence = smoothstep(SSR_BACKFACE_SMOOTHSTEP_MIN, SSR_BACKFACE_SMOOTHSTEP_MAX, fBackfaceDot);
					// Stretch penalty intentionally retired — fought Fresnel at grazing angles.
					float fStretchConfidence  = 1.0;

					float fConfidence = fEdgeFade * fDistanceFade * fDepthConfidence *
										fBackfaceConfidence * fStretchConfidence;
					fConfidence       = max(fConfidence, SSR_MIN_CONFIDENCE);

					fMarchDistance = fFinalT;
					return float4(xFinalUV, fFinalSceneDepth, fConfidence);
				}
				else
				{
					fT = fPrevT;
					float fStepSize = max(xTexelSize.x, xTexelSize.y) * 0.5;
					fPrevT = fT;
					fT += fStepSize / fMaxScreenDist;
				}
			}
			else
			{
				fT = max(0.0, fT - exp2((float)(iCurrentMip - 1)) * max(xTexelSize.x, xTexelSize.y) / fMaxScreenDist);
				iCurrentMip--;
			}
		}
		else
		{
			float fMipScale = exp2((float)iCurrentMip);
			float fStepSize = fMipScale * max(xTexelSize.x, xTexelSize.y) * SSR_STEP_SIZE_MULTIPLIER;

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
	// Debug 99: solid magenta — proves the pipeline ran at all.
	if (SSRConstants.u_uDebugMode == 99u) return float4(1.0, 0.0, 1.0, 1.0);

	float fDepth = g_xDepthTex.Sample(input.uv).r;
	if (fDepth >= 1.0) return float4(0.0, 0.0, 0.0, 0.0); // sky

	float4 xMaterial    = g_xMaterialTex.Sample(input.uv);
	float  fRoughness   = xMaterial.r;
	float3 xWorldNormal = g_xNormalsTex.Sample(input.uv).rgb;

	if (SSRConstants.u_uDebugMode == SSR_DEBUG_ROUGHNESS)
	{
		float fPassesThreshold = (fRoughness <= SSRConstants.u_fMaxRoughness) ? 1.0 : 0.0;
		return float4(fRoughness, fPassesThreshold, fRoughness / SSRConstants.u_fMaxRoughness, 1.0);
	}
	if (SSRConstants.u_uDebugMode == SSR_DEBUG_WORLD_NORMAL_Y)
	{
		float fNormalY = xWorldNormal.y * 0.5 + 0.5;
		return float4(fNormalY,
					  (xWorldNormal.y > 0.7) ? 1.0 : 0.0,
					  (xWorldNormal.y > 0.9) ? 1.0 : 0.0,
					  1.0);
	}

	if (fRoughness > SSRConstants.u_fMaxRoughness) return float4(0.0, 0.0, 0.0, 0.0);

	float3 xViewPos    = GetViewPosFromDepth(input.uv, fDepth);
	float3 xViewNormal = normalize(mul((float3x3)FrameConstants.g_xViewMat, xWorldNormal));
	float3 xViewDir    = normalize(xViewPos);
	float3 xReflectDir = reflect(xViewDir, xViewNormal);

	// Stochastic GGX perturbation for rough surfaces — gives physically-correct
	// rough reflections rather than relying on post-blur alone.
	if (fRoughness > SSR_ROUGHNESS_PERTURB_THRESHOLD)
	{
		int2  xPixelCoord  = (int2)fragCoord.xy;
		float2 xNoise       = GetBlueNoise(xPixelCoord, SSRConstants.u_uFrameIndex);
		float3 xH           = ImportanceSampleGGX(xNoise, xViewNormal, fRoughness);
		float3 xPerturbedDir = reflect(xViewDir, xH);
		// Reject perturbations that fall below the surface — reflect() can
		// produce sub-horizon directions for low NdotH samples.
		if (dot(xPerturbedDir, xViewNormal) > 0.0) xReflectDir = xPerturbedDir;
	}

	if (SSRConstants.u_uDebugMode == SSR_DEBUG_RAY_DIRECTIONS)
		return float4(xReflectDir * 0.5 + 0.5, 1.0);

	if (SSRConstants.u_uDebugMode == SSR_DEBUG_EDGE_FADE)
	{
		float fEdgeFade = ComputeEdgeFade(input.uv, 0.1);
		return float4(fEdgeFade, fEdgeFade, fEdgeFade, 1.0);
	}

	float fNdotV = max(dot(xViewNormal, -xViewDir), 0.0);

	float fMarchDistance = 0.0;
	float2 xScreenDir    = float2(0.0, 0.0);
	float4 xHitResult    = RayMarch(xViewPos, xReflectDir, xViewNormal, fRoughness, fNdotV, fMarchDistance, xScreenDir);

	if (SSRConstants.u_uDebugMode == SSR_DEBUG_SCREEN_DIRECTIONS)
		return float4(xScreenDir * 0.5 + 0.5, 0.0, 1.0);
	if (SSRConstants.u_uDebugMode == SSR_DEBUG_REFLECTION_UVS)
		return float4(xHitResult.xy, 0.0, 1.0);
	if (SSRConstants.u_uDebugMode == SSR_DEBUG_CONFIDENCE)
		return float4(xHitResult.w, xHitResult.w, xHitResult.w, 1.0);

	if (SSRConstants.u_uDebugMode == SSR_DEBUG_DEPTH_COMPARISON)
	{
		float3 xScreenStart = ViewToScreen(xViewPos);
		float  fStartViewZ  = xViewPos.z;
		float  fMaxDepthDiff = 0.0;
		bool   bFoundCloser = false;
		for (uint i = 1u; i <= 64u; i++)
		{
			float  fT_dbg     = (float)i / 64.0;
			float2 xSampleUV  = xScreenStart.xy + xScreenDir * fT_dbg * 0.8;
			if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
				xSampleUV.y < 0.0 || xSampleUV.y > 1.0) break;
			float fSceneDepth = g_xHiZTex.Sample(xSampleUV).r;
			if (fSceneDepth < 0.9999)
			{
				float fSceneViewZ = DepthToViewZ(xSampleUV, fSceneDepth);
				float fDD = fStartViewZ - fSceneViewZ;
				fMaxDepthDiff = max(fMaxDepthDiff, fDD);
				if (fSceneViewZ < fStartViewZ) bFoundCloser = true;
			}
		}
		float fR = saturate(fMaxDepthDiff / 10.0);
		float fG = bFoundCloser ? 1.0 : 0.0;
		float fB = (fMaxDepthDiff > 0.5) ? 1.0 : 0.0;
		return float4(fR, fG, fB, 1.0);
	}

	if (SSRConstants.u_uDebugMode == SSR_DEBUG_MARCH_DISTANCE)
	{
		float3 xScreenStart = ViewToScreen(xViewPos);
		float2 xEndUV       = xScreenStart.xy + xScreenDir * 0.8;
		float  fEndDepth    = g_xHiZTex.Sample(saturate(xEndUV)).r;
		bool   bHitSky      = fEndDepth >= 0.9999;
		return float4(xEndUV.x, xEndUV.y, bHitSky ? 1.0 : 0.0, 1.0);
	}

	if (xHitResult.w > 0.0)
	{
		float3 xReflectionColour = g_xDiffuseTex.Sample(xHitResult.xy).rgb;

		if (SSRConstants.u_uDebugMode == SSR_DEBUG_HIT_POSITIONS)
		{
			float3 xHitWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, xHitResult.xy);
			return float4(xHitWorldPos / 100.0, 1.0);
		}

		xReflectionColour *= SSRConstants.u_fIntensity;

		float fFinalConfidence = xHitResult.w;

		// Contact hardening — distance-weighted confidence falloff using
		// the world-space travel between origin and hit.
		float3 xOriginWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, input.uv);
		float3 xHitWorldPos    = GetWorldPosFromDepthTex(g_xDepthTex, xHitResult.xy);
		float  fWorldTravelDist = length(xHitWorldPos - xOriginWorldPos);

		float fContactHardening = smoothstep(0.0, SSRConstants.u_fContactHardeningDist, fWorldTravelDist);
		float fRoughnessContactScale = lerp(1.0, SSR_CONTACT_ROUGHNESS_SCALE_MAX, fRoughness);
		fContactHardening = saturate(fContactHardening * fRoughnessContactScale);
		float fContactConfidence = 1.0 - fContactHardening * SSR_CONTACT_CONFIDENCE_SCALE;

		// Fade SSR for rough surfaces so the deferred shader blends in IBL.
		float fRoughnessConfidence = 1.0 - smoothstep(SSR_ROUGHNESS_CONF_START, SSR_ROUGHNESS_CONF_END, fRoughness);
		fFinalConfidence *= fRoughnessConfidence;

		float fMetallic      = g_xMaterialTex.Sample(input.uv).g;
		float fMetallicBoost = lerp(1.0, SSR_METALLIC_BOOST, fMetallic); // currently 1.0 (energy conservation)
		fFinalConfidence    *= fMetallicBoost;

		// Horizon occlusion — fades reflections nearly parallel to the
		// surface (small normal jitter → wild direction changes there).
		float fNdotR       = max(dot(xViewNormal, xReflectDir), 0.0);
		float fHorizonFade = saturate(fNdotR * SSR_HORIZON_FADE_SCALE);
		fFinalConfidence  *= fHorizonFade;

		fFinalConfidence  *= fContactConfidence;
		fFinalConfidence   = saturate(fFinalConfidence);

		return float4(xReflectionColour, fFinalConfidence);
	}
	return float4(0.0, 0.0, 0.0, 0.0);
}
