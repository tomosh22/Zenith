// =====================================================================
// Flux_TAA_Resolve — temporal anti-aliasing resolve (compute).
//
// Subsystem: TAA. MAIN view only, on the fully-composited lit HDR scene (after
// DeferredShading / Grass / Translucency / Fog / Particles) and before the HDR
// bloom + tonemap, which read this pass's output via GetSceneColourForPostFX.
//
// Per output pixel (a line-by-line mirror of the pure CPU cores in
// Flux/TAA/Flux_TAA_ResolveCPU.h — the CPU cores are the golden-test parity
// contract, so the helper functions below MUST match them exactly):
//   1. 3x3 neighbourhood -> YCoCg mean/sigma (variance-clip box) + closest-depth
//      velocity dilation (foreground velocity wins across edges).
//   2. reproject history at uv - dilatedVelocity (bilinear).
//   3. YCoCg variance clip: clip history toward the neighbourhood mean.
//   4. disocclusion reject: history's stored linear depth vs the current linear
//      depth (relative threshold) -> fall back to the current frame.
//   5. velocity-ramped, Karis-luminance-weighted current/history blend.
// Output: rgb = resolved colour; a = current linear view depth (+Z forward),
// which TAA_CopyToHistory carries into the persistent history for next frame's
// disocclusion test.
//
// Motion vectors are jitter-free at the source (Common/Velocity.slang), so the
// resolve does no jitter subtraction. Moving/skinned objects get camera+static
// velocity only until Stage 4.3 adds prev-transform / prev-pose motion.
// =====================================================================

#include "Common/Bindings.slang"   // spine (sets 0/1/2) + DepthToViewZ (reads the VIEW set)

struct TAAResolveConstantsLayout
{
	uint  g_uOutputWidth;
	uint  g_uOutputHeight;
	uint  g_uHistoryValid;                 // 0 => ignore history (first frame after a graph rebuild)
	uint  g_uPad0;
	float g_fBlendMinAlpha;                // slow-pixel history weight floor (sharpest AA)
	float g_fBlendMaxAlpha;                // fast-pixel current-frame weight (least ghosting)
	float g_fVelocityRejectionThreshold;   // pixels: velocity magnitude that ramps the blend to max
	float g_fHistoryClampStrength;         // gamma in the mean +/- gamma*sigma variance clip
	float g_fDisocclusionDepthThreshold;   // relative depth mismatch that rejects history
	uint  g_uRenderWidth;                  // TEMPORAL UPSCALING: render (source) width  — == g_uOutputWidth  when off
	uint  g_uRenderHeight;                 //                     render (source) height — == g_uOutputHeight when off
	float g_fUpscaleSigma;                 // Gaussian gather sigma in render pixels (~0.47); unused at render==output
};

struct PassParams
{
	Sampler2D                                 g_xHDRTex;       // current jittered lit HDR scene
	Sampler2D                                 g_xVelocityTex;  // RG16F motion vectors (uvCur - uvPrev)
	Sampler2D                                 g_xDepthTex;     // scene depth (D32)
	Sampler2D                                 g_xHistoryTex;   // persistent history: rgb=prev resolved, a=prev linear depth
	RWTexture2D<float4>                       g_xOutputTex;    // resolved output (rgb) + current linear depth (a)
	ConstantBuffer<TAAResolveConstantsLayout> g_xTAAConstants;
};
ParameterBlock<PassParams> g_xPassSet;

// ---- pure CPU-mirror helpers (Flux_TAA_ResolveCPU.h) ----
float3 TAA_RGBToYCoCg(float3 c)
{
	return float3(
		 0.25 * c.x + 0.5 * c.y + 0.25 * c.z,   // Y
		 0.5  * c.x             - 0.5  * c.z,    // Co
		-0.25 * c.x + 0.5 * c.y - 0.25 * c.z);  // Cg
}
float3 TAA_YCoCgToRGB(float3 y)
{
	float fY = y.x, fCo = y.y, fCg = y.z;
	return float3(fY + fCo - fCg, fY + fCg, fY - fCo - fCg);
}
float3 TAA_VarianceClip(float3 xMean, float3 xSigma, float fGamma, float3 xHistory)
{
	float3 xExtent = xSigma * fGamma + 1e-6;
	float3 xV      = xHistory - xMean;
	float3 xUnit   = abs(xV) / xExtent;
	float  fMax    = max(xUnit.x, max(xUnit.y, xUnit.z));
	return (fMax > 1.0) ? (xMean + xV / fMax) : xHistory;
}
bool TAA_DisocclusionReject(float fHistoryPrevDepth, float fExpectedPrevDepth, float fRelThreshold)
{
	float fDenom = max(max(fHistoryPrevDepth, fExpectedPrevDepth), 1e-4);
	return (abs(fHistoryPrevDepth - fExpectedPrevDepth) / fDenom) > fRelThreshold;
}
float TAA_BlendAlpha(float fVelPixels, float fVelThreshold, float fMinAlpha, float fMaxAlpha)
{
	float fT = (fVelThreshold > 0.0) ? saturate(fVelPixels / fVelThreshold) : 1.0;
	return fMinAlpha + (fMaxAlpha - fMinAlpha) * fT;
}
float TAA_KarisWeight(float fLuma)
{
	return 1.0 / (1.0 + max(0.0, fLuma));
}
float3 TAA_ResolveBlend(float3 xCurrent, float3 xHistory, float fAlpha)
{
	float fLumaCur  = TAA_RGBToYCoCg(xCurrent).x;
	float fLumaHist = TAA_RGBToYCoCg(xHistory).x;
	float fWCur     = fAlpha         * TAA_KarisWeight(fLumaCur);
	float fWHist    = (1.0 - fAlpha) * TAA_KarisWeight(fLumaHist);
	float fDenom    = max(fWCur + fWHist, 1e-6);
	return (xCurrent * fWCur + xHistory * fWHist) / fDenom;
}
// Gaussian upscale weight (mirror of Flux_TAAUpscaleWeight): weight(0) == 1.
float TAA_UpscaleWeight(float fDistPixels, float fSigma)
{
	float fX = fDistPixels / fSigma;
	return exp(-0.5 * fX * fX);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void csMain(uint3 dispatchId : SV_DispatchThreadID)
{
	uint2 xPixel = dispatchId.xy;
	const uint uW = g_xPassSet.g_xTAAConstants.g_uOutputWidth;
	const uint uH = g_xPassSet.g_xTAAConstants.g_uOutputHeight;
	if (xPixel.x >= uW || xPixel.y >= uH)
	{
		return;
	}

	const float2 xTexel = 1.0 / float2(uW, uH);              // OUTPUT texel — dispatch / history / velocity-reject use output dims
	const float2 xUV    = (float2(xPixel) + 0.5) * xTexel;   // output pixel centre -> [0,1] UV (resolution-independent)

	// Temporal upscaling: the HDR / velocity / depth sources are at RENDER res, so neighbourhood
	// taps step by the RENDER texel (the sampler maps the [0,1] UV into the render texture for free).
	const uint2  xRenderDim   = uint2(g_xPassSet.g_xTAAConstants.g_uRenderWidth, g_xPassSet.g_xTAAConstants.g_uRenderHeight);
	const float2 xRenderTexel = 1.0 / float2(xRenderDim);

	// Current-frame colour. At render == output (upscaling off) this is exactly today's single
	// centre tap (byte-identical). When upscaling, reconstruct via a Gaussian scatter-as-gather over
	// the surrounding render texels — a line-for-line mirror of Flux_TAAUpscaleReconstruct (CPU).
	float3 xCurrent;
	if (all(xRenderDim == uint2(uW, uH)))
	{
		xCurrent = g_xPassSet.g_xHDRTex.SampleLevel(xUV, 0).rgb;
	}
	else
	{
		const float2 xRenderCoord = xUV * float2(xRenderDim);
		const float2 xBase        = floor(xRenderCoord);
		float3 xAccum = float3(0.0);
		float  fWSum  = 0.0;
		[unroll]
		for (int iGY = -1; iGY <= 1; ++iGY)
		{
			[unroll]
			for (int iGX = -1; iGX <= 1; ++iGX)
			{
				float2 xCentre = float2(xBase.x + float(iGX) + 0.5, xBase.y + float(iGY) + 0.5);
				float2 xTapUV  = xCentre / float2(xRenderDim);
				float2 xD      = xCentre - xRenderCoord;
				float  fW      = TAA_UpscaleWeight(length(xD), g_xPassSet.g_xTAAConstants.g_fUpscaleSigma);
				xAccum += g_xPassSet.g_xHDRTex.SampleLevel(xTapUV, 0).rgb * fW;
				fWSum  += fW;
			}
		}
		xCurrent = xAccum / max(fWSum, 1e-6);
	}

	// Current linear view depth — stored in the output alpha for next frame's
	// disocclusion test, and used as the expected-previous-depth here.
	float fCurRawDepth = g_xPassSet.g_xDepthTex.SampleLevel(xUV, 0).x;
	float fCurViewZ    = DepthToViewZ(xUV, fCurRawDepth);

	// First frame after a graph rebuild / invalidation: no valid history — emit the
	// current frame verbatim (matches Flux_TAAResolveBlend's bHistoryValid == false).
	if (g_xPassSet.g_xTAAConstants.g_uHistoryValid == 0u)
	{
		g_xPassSet.g_xOutputTex[xPixel] = float4(xCurrent, fCurViewZ);
		return;
	}

	// 3x3 neighbourhood: YCoCg moments (variance-clip box) + closest-depth velocity
	// dilation (the nearest neighbour's velocity wins, so foreground motion bleeds
	// over silhouettes instead of the background's).
	float3 xSum   = float3(0.0);
	float3 xSumSq = float3(0.0);
	float  fBestViewZ = 3.402823e38;   // FLT_MAX
	float2 xDilatedVel = g_xPassSet.g_xVelocityTex.SampleLevel(xUV, 0).xy;
	[unroll]
	for (int iY = -1; iY <= 1; ++iY)
	{
		[unroll]
		for (int iX = -1; iX <= 1; ++iX)
		{
			float2 xNUV     = xUV + float2(iX, iY) * xRenderTexel;   // step one RENDER texel (source res)
			float3 xNColour = g_xPassSet.g_xHDRTex.SampleLevel(xNUV, 0).rgb;
			float3 xNY      = TAA_RGBToYCoCg(xNColour);
			xSum   += xNY;
			xSumSq += xNY * xNY;

			float fNViewZ = DepthToViewZ(xNUV, g_xPassSet.g_xDepthTex.SampleLevel(xNUV, 0).x);
			if (fNViewZ < fBestViewZ)
			{
				fBestViewZ  = fNViewZ;
				xDilatedVel = g_xPassSet.g_xVelocityTex.SampleLevel(xNUV, 0).xy;
			}
		}
	}
	const float  fInv   = 1.0 / 9.0;
	const float3 xMean  = xSum * fInv;
	const float3 xVar   = max(float3(0.0), xSumSq * fInv - xMean * xMean);
	const float3 xSigma = sqrt(xVar);

	// Reproject history (bilinear) at uv - velocity.
	float2 xHistUV    = xUV - xDilatedVel;
	float3 xOutColour = xCurrent;
	if (all(xHistUV >= float2(0.0)) && all(xHistUV <= float2(1.0)))
	{
		float4 xHistSample = g_xPassSet.g_xHistoryTex.SampleLevel(xHistUV, 0);
		float3 xHistRGB    = xHistSample.rgb;
		float  fHistViewZ  = xHistSample.a;   // prev linear view depth (from history alpha)

		// Disocclusion: reject history whose stored depth disagrees with the current
		// depth (relative threshold) — a newly-revealed / edge pixel.
		if (!TAA_DisocclusionReject(fHistViewZ, fCurViewZ, g_xPassSet.g_xTAAConstants.g_fDisocclusionDepthThreshold))
		{
			// YCoCg variance clip toward the neighbourhood mean.
			float3 xClippedRGB = TAA_YCoCgToRGB(
				TAA_VarianceClip(xMean, xSigma, g_xPassSet.g_xTAAConstants.g_fHistoryClampStrength, TAA_RGBToYCoCg(xHistRGB)));

			// Velocity-ramped, Karis-weighted blend.
			float fVelPixels = length(xDilatedVel * float2(uW, uH));
			float fAlpha     = TAA_BlendAlpha(fVelPixels,
				g_xPassSet.g_xTAAConstants.g_fVelocityRejectionThreshold,
				g_xPassSet.g_xTAAConstants.g_fBlendMinAlpha,
				g_xPassSet.g_xTAAConstants.g_fBlendMaxAlpha);
			xOutColour = TAA_ResolveBlend(xCurrent, xClippedRGB, fAlpha);
		}
	}

	g_xPassSet.g_xOutputTex[xPixel] = float4(xOutColour, fCurViewZ);
}
