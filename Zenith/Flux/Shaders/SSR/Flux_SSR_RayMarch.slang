// =====================================================================
// Flux_SSR_RayMarch — Slang module.
//
// Subsystem: SSR. Half-resolution screen-space reflections via HiZ-
// accelerated ray marching. Reflection ray is GGX-perturbed for rough
// surfaces, then traverses the depth pyramid (binary-search refined),
// and the final hit colour comes from the G-buffer diffuse target.
//
// Outputs (dual MRT):
//   RT0 RGBA: rgb = reflection colour,        a = confidence
//   RT1 RGBA: rg  = hit UV, b = travel dist,  a = ray count
//
// RT1 carries hit metadata for downstream passes (denoise BRDF reuse,
// variance estimation) — see Flux_SSR.cpp for the transient layout.
// On miss / debug / sky / roughness-rejected paths both RTs go to zero.
//
// Camera matrices / depth helpers / blue-noise constants come from the
// Bindings spine (replaces the former `import Common.Frame`); Common.PBR
// supplies ImportanceSampleGGX / PBR_DIRECTION_EPSILON.
// =====================================================================

#include "Common/Bindings.slang"
import Common.Fullscreen;
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

// Phase 6 — adaptive per-pixel step budget. Mirror-smooth surfaces and pixels
// near a screen edge get more steps; high-roughness surfaces in the centre of
// the frame get fewer (their wide GGX lobe + IBL fallback hide the impact).
// Edge proximity is measured against the nearest screen edge (ramp ends at
// 20% of screen so the extra-budget zone covers the visible edge band).
static const float SSR_ADAPTIVE_ROUGHNESS_BREAK = 0.5;
static const float SSR_ADAPTIVE_EDGE_BAND       = 0.2;

// Roughness-driven cone tracing. Roughness ∈ [0, 1] maps to mip 0..3.
static const float SSR_ROUGHNESS_MIP_SCALE       = 2.5;
static const int   SSR_MAX_ROUGHNESS_MIP         = 3;

// Hit normal must face *toward* the ray (NdotR < 0). 0.0 is the clean physical
// threshold; the previous 0.1 admitted slightly back-facing hits which produced
// the "noisy halo around silhouettes" look.
static const float SSR_BACKFACE_THRESHOLD        = 0.0;
static const float SSR_DEPTH_VALID_MIN           = 0.1;

// Edge-margin base trimmed from 0.15 → 0.08 so the screen-edge fade doesn't
// eat ~30% of usable reflection area at high roughness. Combined with
// SSR_EDGE_MARGIN_ROUGHNESS = 0.10 this caps the fade band at 18% of screen
// even on near-fully-rough surfaces, vs the previous 25%.
static const float SSR_EDGE_MARGIN_BASE          = 0.08;
static const float SSR_EDGE_MARGIN_ROUGHNESS     = 0.10;
static const float SSR_BACKFACE_SMOOTHSTEP_MIN   = 0.1;
static const float SSR_BACKFACE_SMOOTHSTEP_MAX   = -0.3;
// Artificial floor retired (was 0.1). The min was masking weak hits with a
// fixed brightness, producing the "speckled noise floor" on near-mirror
// surfaces. Phase 3a denoise gates handle low-confidence pixels properly.
static const float SSR_MIN_CONFIDENCE            = 0.0;

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

// Phase 2 — multi-ray per pixel for rough surfaces.
//   Roughness gate: surfaces below this stay at 1 ray (the GGX lobe is tight
//   enough that one importance-sampled ray is already a good estimate). Above
//   this, take u_uRayCount stratified GGX samples per pixel and average.
//   Hammersley provides a low-discrepancy 2D sequence that, combined with the
//   per-pixel blue-noise offset, gives clean stratification across both pixels
//   and rays.
static const float SSR_MULTIRAY_ROUGHNESS_THRESHOLD = 0.3;
//   Hard cap on rays per pixel — keeps the shader's stack usage bounded even
//   if the CBV value is mis-set.
static const uint  SSR_MAX_RAYS_PER_PIXEL           = 4u;
//   Average-confidence early-exit threshold. Matches SSGI's
//   SSGI_EARLY_EXIT_CONFIDENCE so behaviour is consistent across screen-space
//   stochastic effects.
static const float SSR_EARLY_EXIT_CONFIDENCE        = 0.95;

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
static const uint SSR_DEBUG_RAY_COUNT        = 12u;

struct SSRConstantsLayout
{
	float u_fIntensity;
	float u_fMaxDistance;
	float u_fMaxRoughness;
	float u_fThickness;
	// Phase 6: u_uStepCount is now the *maximum* per-pixel step budget.
	// u_uStepCountMin is the floor for high-roughness / centre-of-screen
	// pixels — those don't need precision so we save iterations there.
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
	// Phase 2 — rays-per-pixel for rough surfaces. Effective count is clamped
	// against SSR_MAX_RAYS_PER_PIXEL inside the shader.
	uint  u_uRayCount;
	// Phase 6 — minimum step budget (mirror-smooth surfaces use up to
	// u_uStepCount; high-roughness surfaces drop to u_uStepCountMin).
	uint  u_uStepCountMin;
	// Padding to land the float4 array on a 16-byte boundary. Slang's CBV
	// layout rules promote float4 arrays to 16-byte alignment regardless;
	// these explicit pads keep the C++ side honest.
	uint  u_uPad0;
	uint  u_uPad1;
	uint  u_uPad2;
	// Cached HiZ per-mip dimensions: xy = (w, h), zw = (1/w, 1/h).
	// Replaces inner-loop GetDimensions calls.
	float4 u_axHiZMipSizes[12];
};
struct PassParams
{
	ConstantBuffer<SSRConstantsLayout> SSRConstants;
	Sampler2D g_xDepthTex;
	Sampler2D g_xNormalsTex;
	Sampler2D g_xMaterialTex;
	Sampler2D g_xHiZTex;
	Sampler2D g_xDiffuseTex;
	Sampler2D g_xBlueNoiseTex;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

// Dual-MRT output. RT0 keeps the legacy (rgb=colour, a=confidence) shape that
// upsample/deferred consume. RT1 carries metadata used by Phase 3b denoise
// (BRDF reuse needs neighbour hit UV) and Phase 4 variance (radial component).
struct RayMarchOut
{
	float4 xColour : SV_Target0;
	float4 xAux    : SV_Target1;
};

RayMarchOut MakeRayMarchOut(float4 xColour, float4 xAux)
{
	RayMarchOut o;
	o.xColour = xColour;
	o.xAux    = xAux;
	return o;
}

float2 GetBlueNoise(int2 xPixelCoord, uint uFrameIndex)
{
	float2 xNoiseUV = float2(xPixelCoord % int2(BLUE_NOISE_SIZE, BLUE_NOISE_SIZE)) / (float)BLUE_NOISE_SIZE;
	float2 xNoise   = g_xPassSet.g_xBlueNoiseTex.Sample(xNoiseUV).rg;
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
				uint   uStepBudget,
				out float fMarchDistance,
				out float2 xScreenDir)
{
	fMarchDistance = 0.0;
	xScreenDir     = float2(0.0, 0.0);

	float fStartOffset = max(SSR_START_OFFSET_MIN, abs(xViewOrigin.z) * SSR_START_OFFSET_SCALE);
	float3 xViewStart  = xViewOrigin + xReflectDir * fStartOffset;

	float3 xScreenStart = ViewToScreen(xViewStart);

	float fRaySampleDist = min(g_xPassSet.SSRConstants.u_fMaxDistance * 0.5, xViewStart.z * 0.5);
	float3 xRaySampleView = xViewStart + xReflectDir * fRaySampleDist;

	float fNearPlane = GetCameraNearFar().x;
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
	float fSafeStartZ      = max(xViewStart.z, GetCameraNearFar().x);
	float fInvZStart       = 1.0 / fSafeStartZ;
	float fEstimatedRayDist = g_xPassSet.SSRConstants.u_fMaxDistance * fMaxScreenDist;
	float fEstimatedEndZ   = max(GetCameraNearFar().x,
								  fSafeStartZ + xReflectDir.z * fEstimatedRayDist);
	float fInvZEnd         = 1.0 / fEstimatedEndZ;

	// Mip 0 dimensions from the cached CBV array — never query the
	// texture itself (project convention: dims flow through the CBV).
	float2 xScreenSize = g_xPassSet.SSRConstants.u_axHiZMipSizes[0].xy;
	float2 xTexelSize  = g_xPassSet.SSRConstants.u_axHiZMipSizes[0].zw;

	int   iCurrentMip   = (int)min(g_xPassSet.SSRConstants.u_uStartMip, g_xPassSet.SSRConstants.u_uHiZMipCount - 1u);
	float fT            = 0.0;
	float fPrevT        = 0.0;
	float fPrevDepthDiff = 0.0;
	bool  bFirstSample  = true;

	int iMinMip = clamp((int)floor(fRoughness * SSR_ROUGHNESS_MIP_SCALE), 0, SSR_MAX_ROUGHNESS_MIP);

	for (uint uIter = 0u; uIter < uStepBudget; uIter++)
	{
		float2 xSampleUV = xScreenStart.xy + xScreenDir * fT * fMaxScreenDist;

		if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
			xSampleUV.y < 0.0 || xSampleUV.y > 1.0) break;

		// Inner loop hot path — read mip dims from the cached CBV array.
		float2 xMipSize   = g_xPassSet.SSRConstants.u_axHiZMipSizes[iCurrentMip].xy;
		float2 xHalfTexel = g_xPassSet.SSRConstants.u_axHiZMipSizes[iCurrentMip].zw * 0.5;
		float2 xClampedUV  = clamp(xSampleUV, xHalfTexel, 1.0 - xHalfTexel);
		float2 xHiZSample  = g_xPassSet.g_xHiZTex.SampleLevel(xClampedUV, (float)iCurrentMip).rg;
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
		float fMipThickness = g_xPassSet.SSRConstants.u_fThickness * exp2((float)iCurrentMip);

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

					for (int j = 0; j < (int)g_xPassSet.SSRConstants.u_uBinarySearchIterations; j++)
					{
						if (abs(fSearchEnd - fSearchStart) < fTolerance) break;

						float fMidT          = (fSearchStart + fSearchEnd) * 0.5;
						float2 xMidUV        = xScreenStart.xy + xScreenDir * fMidT * fMaxScreenDist;
						float fMidSceneDepth = g_xPassSet.g_xHiZTex.SampleLevel(xMidUV, fSearchMip).r;
						float fMidSceneZ     = DepthToViewZ(xMidUV, fMidSceneDepth);

						float fMidInvZ = lerp(fInvZStart, fInvZEnd, fMidT);
						float fMidRayZ = 1.0 / fMidInvZ;
						float fMidDiff = fMidRayZ - fMidSceneZ;

						if (fMidDiff > 0.0) fSearchEnd   = fMidT;
						else                fSearchStart = fMidT;
					}

					float fFinalT          = (fSearchStart + fSearchEnd) * 0.5;
					float2 xFinalUV        = xScreenStart.xy + xScreenDir * fFinalT * fMaxScreenDist;
					float fFinalSceneDepth = g_xPassSet.g_xHiZTex.SampleLevel(xFinalUV, fSearchMip).r;
					float fFinalSceneZ     = DepthToViewZ(xFinalUV, fFinalSceneDepth);

					float3 xHitWorldNormal = g_xPassSet.g_xNormalsTex.Sample(xFinalUV).rgb;
					float3 xHitViewNormal  = normalize(mul((float3x3)GetViewMat(), xHitWorldNormal));
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
					float fDepthConfidence = 1.0 - smoothstep(0.0, g_xPassSet.SSRConstants.u_fThickness * SSR_STEP_SIZE_MULTIPLIER, fDepthError);

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
RayMarchOut fsMain(FullscreenVsOut input, float4 fragCoord : SV_Position)
{
	const float4 xZero4 = float4(0.0, 0.0, 0.0, 0.0);

	// Debug 99: solid magenta — proves the pipeline ran at all.
	if (g_xPassSet.SSRConstants.u_uDebugMode == 99u) return MakeRayMarchOut(float4(1.0, 0.0, 1.0, 1.0), xZero4);

	float fDepth = g_xPassSet.g_xDepthTex.Sample(input.uv).r;
	if (fDepth >= 1.0) return MakeRayMarchOut(xZero4, xZero4); // sky

	float4 xMaterial    = g_xPassSet.g_xMaterialTex.Sample(input.uv);
	float  fRoughness   = xMaterial.r;
	float3 xWorldNormal = g_xPassSet.g_xNormalsTex.Sample(input.uv).rgb;

	if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_ROUGHNESS)
	{
		float fPassesThreshold = (fRoughness <= g_xPassSet.SSRConstants.u_fMaxRoughness) ? 1.0 : 0.0;
		return MakeRayMarchOut(float4(fRoughness, fPassesThreshold, fRoughness / g_xPassSet.SSRConstants.u_fMaxRoughness, 1.0), xZero4);
	}
	if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_WORLD_NORMAL_Y)
	{
		float fNormalY = xWorldNormal.y * 0.5 + 0.5;
		return MakeRayMarchOut(float4(fNormalY,
									  (xWorldNormal.y > 0.7) ? 1.0 : 0.0,
									  (xWorldNormal.y > 0.9) ? 1.0 : 0.0,
									  1.0), xZero4);
	}

	if (fRoughness > g_xPassSet.SSRConstants.u_fMaxRoughness) return MakeRayMarchOut(xZero4, xZero4);

	float3 xViewPos          = GetViewPosFromDepth(input.uv, fDepth);
	float3 xViewNormal       = normalize(mul((float3x3)GetViewMat(), xWorldNormal));
	float3 xViewDir          = normalize(xViewPos);
	float3 xMirrorReflectDir = reflect(xViewDir, xViewNormal);
	float  fNdotV            = max(dot(xViewNormal, -xViewDir), 0.0);

	// Phase 2: take multiple GGX importance samples on rough surfaces (where
	// one sample is too noisy a Monte-Carlo estimate of the lobe). Mirror-
	// smooth surfaces (R ≤ 0.3) stay at 1 ray since their lobe is tight.
	uint uRequestedRays = 1u;
	if (fRoughness > SSR_MULTIRAY_ROUGHNESS_THRESHOLD)
	{
		uRequestedRays = clamp(g_xPassSet.SSRConstants.u_uRayCount, 1u, SSR_MAX_RAYS_PER_PIXEL);
	}

	// Phase 6 — per-pixel step budget. Scale between u_uStepCountMin and
	// u_uStepCount based on (a) surface smoothness — sharp reflections need
	// more steps to land precisely; rough reflections will be averaged out
	// by the lobe regardless — and (b) edge proximity — pixels near the
	// screen edge get a longer trace so the ray has room to find a hit
	// before exiting.
	float fSmoothness    = 1.0 - smoothstep(0.0, SSR_ADAPTIVE_ROUGHNESS_BREAK, fRoughness);
	float fEdgeProximity = min(min(input.uv.x, 1.0 - input.uv.x),
								min(input.uv.y, 1.0 - input.uv.y));
	float fEdgeNearness  = 1.0 - smoothstep(0.0, SSR_ADAPTIVE_EDGE_BAND, fEdgeProximity);
	float fHighStepFactor = max(fSmoothness, fEdgeNearness);

	uint uStepMin    = max(g_xPassSet.SSRConstants.u_uStepCountMin, 1u);
	uint uStepMax    = max(g_xPassSet.SSRConstants.u_uStepCount, uStepMin);
	uint uStepBudget = uStepMin + (uint)((float)(uStepMax - uStepMin) * fHighStepFactor);

	// Multi-ray accumulators. We average colour / confidence / hit-UV /
	// travel-distance across rays that produced a valid hit; misses don't
	// contribute. Importance sampling already biases toward the GGX D-term,
	// so the average is unweighted (re-weighting by D would double-bias
	// toward the mirror direction).
	float3 xAccumColour    = float3(0.0);
	float  fAccumConf      = 0.0;
	float2 xAccumHitUV     = float2(0.0);
	float  fAccumDist      = 0.0;
	uint   uEffectiveRays  = 0u;  // rays that actually hit
	uint   uRaysAttempted  = 0u;  // rays we actually traced (≤ uRequestedRays — early-exit can stop short)

	// First-ray captured state used by per-ray-direction debug visualisations
	// — these fall through with k=0 data even when the loop runs multiple
	// iterations.
	float3 xFirstReflectDir = xMirrorReflectDir;
	float2 xFirstScreenDir  = float2(0.0, 0.0);
	float4 xFirstHitResult  = float4(0.0, 0.0, 0.0, 0.0);

	int2 xPixelCoord = (int2)fragCoord.xy;
	for (uint k = 0u; k < uRequestedRays; k++)
	{
		uRaysAttempted++;
		// Stratified Hammersley + per-pixel blue-noise offset. Hammersley
		// gives a low-discrepancy 2D sequence across rays; the blue-noise
		// offset rotates the sequence per pixel so neighbours don't all see
		// the same N samples.
		float2 xBlueNoise  = GetBlueNoise(xPixelCoord, g_xPassSet.SSRConstants.u_uFrameIndex);
		float2 xHammersley = Hammersley(k, max(uRequestedRays, 1u));
		float2 xXi         = frac(xBlueNoise + xHammersley);

		float3 xKReflectDir = xMirrorReflectDir;
		if (fRoughness > SSR_ROUGHNESS_PERTURB_THRESHOLD)
		{
			float3 xH            = ImportanceSampleGGX(xXi, xViewNormal, fRoughness);
			float3 xPerturbedDir = reflect(xViewDir, xH);
			// Reject perturbations that fall below the surface — reflect()
			// can produce sub-horizon directions for low NdotH samples.
			if (dot(xPerturbedDir, xViewNormal) > 0.0) xKReflectDir = xPerturbedDir;
		}

		float  fKMarchDist  = 0.0;
		float2 xKScreenDir  = float2(0.0, 0.0);
		float4 xKHitResult  = RayMarch(xViewPos, xKReflectDir, xViewNormal, fRoughness, fNdotV, uStepBudget, fKMarchDist, xKScreenDir);

		if (k == 0u)
		{
			xFirstReflectDir = xKReflectDir;
			xFirstScreenDir  = xKScreenDir;
			xFirstHitResult  = xKHitResult;
		}

		if (xKHitResult.w > 0.0)
		{
			xAccumColour  += g_xPassSet.g_xDiffuseTex.Sample(xKHitResult.xy).rgb;
			fAccumConf    += xKHitResult.w;
			xAccumHitUV   += xKHitResult.xy;
			fAccumDist    += fKMarchDist;
			uEffectiveRays++;

			// Average-confidence early exit (matches SSGI). Stop spending
			// extra rays once the running mean confidence already clears the
			// "this is clearly a good hit" bar.
			if ((fAccumConf / float(uEffectiveRays)) > SSR_EARLY_EXIT_CONFIDENCE) break;
		}
	}

	// Per-ray-direction debug visualisations — all use the first ray's data
	// so the visualisation is deterministic regardless of multi-ray count.
	if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_RAY_DIRECTIONS)
		return MakeRayMarchOut(float4(xFirstReflectDir * 0.5 + 0.5, 1.0), xZero4);

	if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_EDGE_FADE)
	{
		float fEdgeFade = ComputeEdgeFade(input.uv, 0.1);
		return MakeRayMarchOut(float4(fEdgeFade, fEdgeFade, fEdgeFade, 1.0), xZero4);
	}

	if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_SCREEN_DIRECTIONS)
		return MakeRayMarchOut(float4(xFirstScreenDir * 0.5 + 0.5, 0.0, 1.0), xZero4);
	if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_REFLECTION_UVS)
		return MakeRayMarchOut(float4(xFirstHitResult.xy, 0.0, 1.0), xZero4);
	if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_CONFIDENCE)
		return MakeRayMarchOut(float4(xFirstHitResult.w, xFirstHitResult.w, xFirstHitResult.w, 1.0), xZero4);

	if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_DEPTH_COMPARISON)
	{
		float3 xScreenStart = ViewToScreen(xViewPos);
		float  fStartViewZ  = xViewPos.z;
		float  fMaxDepthDiff = 0.0;
		bool   bFoundCloser = false;
		for (uint i = 1u; i <= 64u; i++)
		{
			float  fT_dbg     = (float)i / 64.0;
			float2 xSampleUV  = xScreenStart.xy + xFirstScreenDir * fT_dbg * 0.8;
			if (xSampleUV.x < 0.0 || xSampleUV.x > 1.0 ||
				xSampleUV.y < 0.0 || xSampleUV.y > 1.0) break;
			float fSceneDepth = g_xPassSet.g_xHiZTex.Sample(xSampleUV).r;
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
		return MakeRayMarchOut(float4(fR, fG, fB, 1.0), xZero4);
	}

	if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_MARCH_DISTANCE)
	{
		float3 xScreenStart = ViewToScreen(xViewPos);
		float2 xEndUV       = xScreenStart.xy + xFirstScreenDir * 0.8;
		float  fEndDepth    = g_xPassSet.g_xHiZTex.Sample(saturate(xEndUV)).r;
		bool   bHitSky      = fEndDepth >= 0.9999;
		return MakeRayMarchOut(float4(xEndUV.x, xEndUV.y, bHitSky ? 1.0 : 0.0, 1.0), xZero4);
	}

	if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_RAY_COUNT)
	{
		float fNorm = (float)uEffectiveRays / (float)max(SSR_MAX_RAYS_PER_PIXEL, 1u);
		return MakeRayMarchOut(float4(fNorm, fNorm, fNorm, 1.0), xZero4);
	}

	if (uEffectiveRays > 0u)
	{
		// Two denominators:
		//   * Colour / hitUV / distance average over EFFECTIVE rays — the
		//     undimmed "what the lobe sampled where it found something" value.
		//   * Confidence averages over ATTEMPTED rays so misses contribute 0
		//     to the numerator. Without this, 1 of 4 rays hitting at conf=1
		//     reads as fully reliable; with it, the result correctly fades
		//     to ~25% confidence so the deferred shader blends in IBL for
		//     the 75% of the lobe that found no screen-space hit. Using
		//     attempted (not requested) rays keeps early-exit honest — those
		//     untraced rays don't penalize the confidence as faux misses.
		float  fInvHits   = 1.0 / (float)uEffectiveRays;
		float  fInvAll    = 1.0 / (float)max(uRaysAttempted, 1u);
		float3 xAvgColour = xAccumColour * fInvHits;
		float  fAvgConf   = fAccumConf   * fInvAll;
		float2 xAvgHitUV  = xAccumHitUV  * fInvHits;
		float  fAvgDist   = fAccumDist   * fInvHits;

		if (g_xPassSet.SSRConstants.u_uDebugMode == SSR_DEBUG_HIT_POSITIONS)
		{
			float3 xHitWorldPos = GetWorldPosFromDepthTex(g_xPassSet.g_xDepthTex, xAvgHitUV);
			return MakeRayMarchOut(float4(xHitWorldPos / 100.0, 1.0), xZero4);
		}

		xAvgColour *= g_xPassSet.SSRConstants.u_fIntensity;

		float fFinalConfidence = fAvgConf;

		// Contact hardening — distance-weighted confidence falloff using
		// the world-space travel from origin to averaged hit.
		float3 xOriginWorldPos = GetWorldPosFromDepthTex(g_xPassSet.g_xDepthTex, input.uv);
		float3 xHitWorldPos    = GetWorldPosFromDepthTex(g_xPassSet.g_xDepthTex, xAvgHitUV);
		float  fWorldTravelDist = length(xHitWorldPos - xOriginWorldPos);

		float fContactHardening = smoothstep(0.0, g_xPassSet.SSRConstants.u_fContactHardeningDist, fWorldTravelDist);
		float fRoughnessContactScale = lerp(1.0, SSR_CONTACT_ROUGHNESS_SCALE_MAX, fRoughness);
		fContactHardening = saturate(fContactHardening * fRoughnessContactScale);
		float fContactConfidence = 1.0 - fContactHardening * SSR_CONTACT_CONFIDENCE_SCALE;

		// Fade SSR for rough surfaces so the deferred shader blends in IBL.
		float fRoughnessConfidence = 1.0 - smoothstep(SSR_ROUGHNESS_CONF_START, SSR_ROUGHNESS_CONF_END, fRoughness);
		fFinalConfidence *= fRoughnessConfidence;

		float fMetallic      = g_xPassSet.g_xMaterialTex.Sample(input.uv).g;
		float fMetallicBoost = lerp(1.0, SSR_METALLIC_BOOST, fMetallic); // currently 1.0 (energy conservation)
		fFinalConfidence    *= fMetallicBoost;

		// Horizon occlusion — fades reflections nearly parallel to the
		// surface. Use the first ray's direction as a representative — for
		// multi-ray, all rays sit in the same GGX lobe so any one is fine.
		float fNdotR       = max(dot(xViewNormal, xFirstReflectDir), 0.0);
		float fHorizonFade = saturate(fNdotR * SSR_HORIZON_FADE_SCALE);
		fFinalConfidence  *= fHorizonFade;

		fFinalConfidence  *= fContactConfidence;
		fFinalConfidence   = saturate(fFinalConfidence);

		// RT1 metadata for Phase 3b/4 consumers:
		//   rg = average hit UV
		//   b  = average travel distance
		//   a  = effective ray count (post early-exit)
		float4 xAux = float4(xAvgHitUV, fAvgDist, (float)uEffectiveRays);
		return MakeRayMarchOut(float4(xAvgColour, fFinalConfidence), xAux);
	}
	return MakeRayMarchOut(xZero4, xZero4);
}
