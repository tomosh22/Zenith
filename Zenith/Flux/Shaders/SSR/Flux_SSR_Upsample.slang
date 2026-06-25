// =====================================================================
// Flux_SSR_Upsample — Slang module.
//
// Subsystem: SSR. Phase 5: 4×4 KNN bilateral upsample. Gathers 16
// half-res candidates around the full-res target pixel, scores each by
// composite (depth + normal + roughness) similarity, and accumulates the
// top-4 weighted by inverse texel distance.
//
// Why 4×4 + KNN over the previous 2×2 bilateral: at depth or normal
// discontinuities, the four 2×2 neighbours may all be on the wrong
// surface side of the silhouette. Widening the gather to 4×4 lets the
// upsample reach farther for matching half-res pixels; KNN selection
// drops the obvious mismatches before they corrupt the accumulator.
//
// Outputs (dual MRT, mirrors raymarch shape):
//   RT0 RGBA: rgb = reflection colour, a = hit confidence
//   RT1 RGBA: rg = hit UV, b = travel distance, a = ray count
//
// Same selection mask drives both RTs so colour + metadata stay
// spatially aligned.
//
// Texture dimensions come from g_xPassSet.SSRConstants — never via GetDimensions
// inside the shader (project convention).
// =====================================================================

#include "Common/Bindings.slang"
import Common.Fullscreen;

// Composite-similarity scoring weights. Scores are in [0, 1]: 1 means
// perfectly matching surface, 0 means total mismatch.
//   * Depth term uses depth-relative sigma so the comparison stays sane
//     across the entire depth range.
//   * Normal term punishes mismatch via 1 - max(NdotN, 0).
//   * Roughness term is a Gaussian on |dr| with the constant below.
static const float UPSAMPLE_DEPTH_SIGMA_FRACTION = 0.02;
static const float UPSAMPLE_MIN_DEPTH_SIGMA      = 0.001;
static const float UPSAMPLE_NORMAL_SIGMA         = 0.25;
static const float UPSAMPLE_ROUGHNESS_SIGMA      = 0.1;
// Final score = depth × normal × roughness, all in [0, 1]. We optionally
// raise the score to a power to bias more aggressively toward similar
// samples — KNN top-4 already does the hard cut, so this is mild.
static const float UPSAMPLE_SCORE_POWER          = 1.0;

// Inverse-distance weight floor — keeps the four winners from collapsing
// to one when several land on the same texel.
static const float UPSAMPLE_INV_DIST_FLOOR       = 0.25;
static const float UPSAMPLE_MIN_TOTAL_WEIGHT     = 0.0001;
static const float UPSAMPLE_SKY_THRESHOLD        = 1.0;

// 4×4 footprint = 16 candidates. Top-K = 4 winners.
static const int   UPSAMPLE_GATHER_DIM           = 4;
static const int   UPSAMPLE_GATHER_TOTAL         = 16;
static const int   UPSAMPLE_TOP_K                = 4;

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
	float u_fHalfResWidth;
	float u_fHalfResHeight;
	float u_fRcpHalfResWidth;
	float u_fRcpHalfResHeight;
	uint  u_uRayCount;
	uint  u_uStepCountMin;
	uint  u_uPad0;
	uint  u_uPad1;
	uint  u_uPad2;
	float4 u_axHiZMipSizes[12];
};
struct PassParams
{
	ConstantBuffer<SSRConstantsLayout> SSRConstants;
	Sampler2D g_xSSRTex;        // half-res ray-march RT0 (colour)
	Sampler2D g_xDepthTex;      // full-res depth
	Sampler2D g_xSSRAuxTex;     // half-res ray-march RT1 (metadata)
	Sampler2D g_xNormalsTex;    // full-res G-buffer normals (for similarity)
	Sampler2D g_xMaterialTex;   // full-res G-buffer material .r = roughness
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

struct UpsampleOut
{
	float4 xColour : SV_Target0;
	float4 xAux    : SV_Target1;
};

UpsampleOut MakeUpsampleOut(float4 xColour, float4 xAux)
{
	UpsampleOut o;
	o.xColour = xColour;
	o.xAux    = xAux;
	return o;
}

[shader("fragment")]
UpsampleOut fsMain(FullscreenVsOut input)
{
	const float4 xZero4 = float4(0.0, 0.0, 0.0, 0.0);

	float fFullDepth = g_xPassSet.g_xDepthTex.Sample(input.uv).r;
	if (fFullDepth >= UPSAMPLE_SKY_THRESHOLD) return MakeUpsampleOut(xZero4, xZero4);

	float3 xCenterNormal    = g_xPassSet.g_xNormalsTex.Sample(input.uv).rgb;
	float  fCenterRoughness = g_xPassSet.g_xMaterialTex.Sample(input.uv).r;

	float2 xHalfResSize   = float2(g_xPassSet.SSRConstants.u_fHalfResWidth,    g_xPassSet.SSRConstants.u_fHalfResHeight);
	float2 xHalfTexelSize = float2(g_xPassSet.SSRConstants.u_fRcpHalfResWidth, g_xPassSet.SSRConstants.u_fRcpHalfResHeight);

	float2 xHalfResPos   = input.uv * xHalfResSize - 0.5;
	float2 xHalfResFloor = floor(xHalfResPos);
	float2 xFrac         = xHalfResPos - xHalfResFloor;

	float fDepthSigma = max(fFullDepth * UPSAMPLE_DEPTH_SIGMA_FRACTION, UPSAMPLE_MIN_DEPTH_SIGMA);

	// Pass 1 — gather 16, score by composite similarity. Store score + the
	// half-res-relative offset index so we can re-fetch the winners later
	// without keeping all 16 colour/aux samples live in registers.
	float aafScores[UPSAMPLE_GATHER_TOTAL];
	int   aaiOffIdx[UPSAMPLE_GATHER_TOTAL];

	int idx = 0;
	for (int yo = -1; yo <= 2; yo++)
	{
		for (int xo = -1; xo <= 2; xo++)
		{
			float2 xSampleUV = saturate((xHalfResFloor + float2((float)xo, (float)yo) + 0.5) * xHalfTexelSize);
			float  fSampleDepth = g_xPassSet.g_xDepthTex.Sample(xSampleUV).r;

			float fScore = 0.0;
			if (fSampleDepth < UPSAMPLE_SKY_THRESHOLD)
			{
				float3 xSampleNormal    = g_xPassSet.g_xNormalsTex.Sample(xSampleUV).rgb;
				float  fSampleRoughness = g_xPassSet.g_xMaterialTex.Sample(xSampleUV).r;

				float fDepthDiff   = abs(fFullDepth - fSampleDepth);
				float fDepthScore  = exp(-fDepthDiff * fDepthDiff / (2.0 * fDepthSigma * fDepthSigma));

				float fNormalDiff  = 1.0 - max(dot(xCenterNormal, xSampleNormal), 0.0);
				float fNormalScore = exp(-fNormalDiff * fNormalDiff / (2.0 * UPSAMPLE_NORMAL_SIGMA * UPSAMPLE_NORMAL_SIGMA));

				float fRoughDiff   = abs(fSampleRoughness - fCenterRoughness);
				float fRoughScore  = exp(-fRoughDiff * fRoughDiff / (2.0 * UPSAMPLE_ROUGHNESS_SIGMA * UPSAMPLE_ROUGHNESS_SIGMA));

				fScore = pow(fDepthScore * fNormalScore * fRoughScore, UPSAMPLE_SCORE_POWER);
			}

			aafScores[idx] = fScore;
			aaiOffIdx[idx] = idx;
			idx++;
		}
	}

	// Pass 2 — selection sort, top-4 by score. Sentinel -1 marks consumed
	// or "no valid candidate at this rank". fBest starts at 0.0 (the sky-
	// score sentinel from Pass 1) so all-sky neighbourhoods leave iBest = -1
	// — without this, sky pixels with score exactly 0.0 win over the
	// `fBest = -1.0` baseline and Pass 3 re-fetches them as if they were
	// real surfaces.
	int aiBestIdx[UPSAMPLE_TOP_K];
	[unroll] for (int s = 0; s < UPSAMPLE_TOP_K; s++) aiBestIdx[s] = -1;

	[unroll] for (int sel = 0; sel < UPSAMPLE_TOP_K; sel++)
	{
		float fBest = 0.0;
		int   iBest = -1;
		[unroll] for (int i = 0; i < UPSAMPLE_GATHER_TOTAL; i++)
		{
			if (aafScores[i] > fBest)
			{
				fBest = aafScores[i];
				iBest = i;
			}
		}
		aiBestIdx[sel] = iBest;
		// Mark this slot consumed. -1 stays excluded from future iterations
		// since -1 < 0.0 (the running fBest baseline).
		if (iBest >= 0) aafScores[iBest] = -1.0;
	}

	// Pass 3 — accumulate the four winners. Weight = score × inverse-distance
	// to the bilinear sample point (xFrac), so we still get sub-texel
	// reconstruction within the chosen four.
	float4 xColourResult = float4(0.0);
	float4 xAuxResult    = float4(0.0);
	float  fTotalWeight  = 0.0;

	[unroll] for (int s = 0; s < UPSAMPLE_TOP_K; s++)
	{
		int iSel = aiBestIdx[s];
		if (iSel < 0) continue;

		// Decode 4×4 offset (xo, yo) ∈ {-1, 0, 1, 2}.
		int iSx = (iSel % UPSAMPLE_GATHER_DIM) - 1;
		int iSy = (iSel / UPSAMPLE_GATHER_DIM) - 1;

		float2 xSampleUV    = saturate((xHalfResFloor + float2((float)iSx, (float)iSy) + 0.5) * xHalfTexelSize);
		float4 xColourSample = g_xPassSet.g_xSSRTex.Sample(xSampleUV);
		float4 xAuxSample    = g_xPassSet.g_xSSRAuxTex.Sample(xSampleUV);

		// Distance to the bilinear sample point; nearer = higher weight.
		float fDx = (float)iSx + 0.5 - xFrac.x;
		float fDy = (float)iSy + 0.5 - xFrac.y;
		float fDistSq = fDx * fDx + fDy * fDy;
		float fInvDist = 1.0 / max(sqrt(fDistSq), UPSAMPLE_INV_DIST_FLOOR);

		// Re-fetch the original score for this slot. Pass 2 set aafScores[iSel]
		// to -1 when consumed, so recompute from the depth/normal/roughness
		// we already gathered — actually simpler: derive a fresh score here.
		float fSampleDepth     = g_xPassSet.g_xDepthTex.Sample(xSampleUV).r;
		float3 xSampleNormal   = g_xPassSet.g_xNormalsTex.Sample(xSampleUV).rgb;
		float  fSampleRoughness = g_xPassSet.g_xMaterialTex.Sample(xSampleUV).r;

		float fDepthDiff   = abs(fFullDepth - fSampleDepth);
		float fDepthScore  = exp(-fDepthDiff * fDepthDiff / (2.0 * fDepthSigma * fDepthSigma));
		float fNormalDiff  = 1.0 - max(dot(xCenterNormal, xSampleNormal), 0.0);
		float fNormalScore = exp(-fNormalDiff * fNormalDiff / (2.0 * UPSAMPLE_NORMAL_SIGMA * UPSAMPLE_NORMAL_SIGMA));
		float fRoughDiff   = abs(fSampleRoughness - fCenterRoughness);
		float fRoughScore  = exp(-fRoughDiff * fRoughDiff / (2.0 * UPSAMPLE_ROUGHNESS_SIGMA * UPSAMPLE_ROUGHNESS_SIGMA));
		float fScore       = pow(fDepthScore * fNormalScore * fRoughScore, UPSAMPLE_SCORE_POWER);

		float fWeight = fScore * fInvDist;

		xColourResult += xColourSample * fWeight;
		xAuxResult    += xAuxSample    * fWeight;
		fTotalWeight  += fWeight;
	}

	if (fTotalWeight > UPSAMPLE_MIN_TOTAL_WEIGHT)
	{
		return MakeUpsampleOut(xColourResult / fTotalWeight, xAuxResult / fTotalWeight);
	}
	// Fallback: passthrough centre half-res sample.
	return MakeUpsampleOut(g_xPassSet.g_xSSRTex.Sample(input.uv), g_xPassSet.g_xSSRAuxTex.Sample(input.uv));
}
