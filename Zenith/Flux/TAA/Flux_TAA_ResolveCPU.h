#pragma once

#include "Maths/Zenith_Maths.h"
#include <cmath>   // fabsf, expf, sqrtf

// ============================================================================
// TAA resolve — PURE CPU mirrors of the neighbourhood-clamped temporal resolve
// (Flux_TAA_Resolve.slang, arriving in Part 2). Every function here is a line-by-line
// transliteration of the shader math, so a headless golden test pins the GPU path
// exactly like Flux_SkinVertexCPU does for skinning.
//
// Pipeline these mirror (per output pixel): velocity-dilate -> reproject history ->
// variance-clip in YCoCg -> disocclusion-reject -> velocity-ramped luma-weighted blend;
// plus the RCAS sharpen tap and the upscale reconstruction kernel.
//
// No GPU, no renderer. Tests in Flux_TAA_ResolveCPU.Tests.inl.
// ============================================================================

// --- YCoCg colour space (variance clipping happens here — perceptual axes) ---
// The clip box aligns with luma/chroma rather than RGB, so it does not clip a
// bright desaturated highlight against a dark saturated neighbour (ghosting) the
// way an RGB AABB would.
inline Zenith_Maths::Vector3 Flux_RGBToYCoCg(const Zenith_Maths::Vector3& xRGB)
{
	return Zenith_Maths::Vector3(
		 0.25f * xRGB.x + 0.5f * xRGB.y + 0.25f * xRGB.z,   // Y
		 0.5f  * xRGB.x                 - 0.5f  * xRGB.z,   // Co
		-0.25f * xRGB.x + 0.5f * xRGB.y - 0.25f * xRGB.z);  // Cg
}

inline Zenith_Maths::Vector3 Flux_YCoCgToRGB(const Zenith_Maths::Vector3& xYCoCg)
{
	const float fY = xYCoCg.x, fCo = xYCoCg.y, fCg = xYCoCg.z;
	return Zenith_Maths::Vector3(fY + fCo - fCg, fY + fCg, fY - fCo - fCg);
}

// --- variance clip (Salvi): clip history toward the neighbourhood mean --------
// Box = mean +/- gamma*sigma (per channel). If the history sample is inside, keep it;
// otherwise return the point where the segment history->centre first enters the box.
// Chosen over min-max AABB: outliers inflate min-max boxes (ghosting) and thin features
// collapse them (flicker); a mean/variance box is robust to both.
inline Zenith_Maths::Vector3 Flux_TAAVarianceClip(
	const Zenith_Maths::Vector3& xMean, const Zenith_Maths::Vector3& xSigma,
	float fGamma, const Zenith_Maths::Vector3& xHistory)
{
	const Zenith_Maths::Vector3 xExtent = xSigma * fGamma + Zenith_Maths::Vector3(1e-6f);
	const Zenith_Maths::Vector3 xV      = xHistory - xMean;   // centre is the mean
	const Zenith_Maths::Vector3 xUnit(
		fabsf(xV.x) / xExtent.x, fabsf(xV.y) / xExtent.y, fabsf(xV.z) / xExtent.z);
	const float fMaxUnit = glm::max(xUnit.x, glm::max(xUnit.y, xUnit.z));
	if (fMaxUnit > 1.0f)
	{
		return xMean + xV / fMaxUnit;   // pull back onto the box surface
	}
	return xHistory;
}

// Mean + population standard deviation of a 3x3 (9-sample) neighbourhood in the given
// colour space. Mirrors the shader's moment accumulation exactly (variance = E[x^2]-E[x]^2,
// clamped >= 0 before the sqrt to absorb FP negatives on a zero-variance neighbourhood).
inline void Flux_TAANeighbourhoodMoments(
	const Zenith_Maths::Vector3* pxSamples, u_int uCount,
	Zenith_Maths::Vector3& xMeanOut, Zenith_Maths::Vector3& xSigmaOut)
{
	Zenith_Maths::Vector3 xSum(0.0f), xSumSq(0.0f);
	for (u_int u = 0; u < uCount; ++u)
	{
		xSum   += pxSamples[u];
		xSumSq += pxSamples[u] * pxSamples[u];
	}
	const float fInv = (uCount > 0u) ? (1.0f / static_cast<float>(uCount)) : 0.0f;
	xMeanOut = xSum * fInv;
	const Zenith_Maths::Vector3 xVar = xSumSq * fInv - xMeanOut * xMeanOut;
	xSigmaOut = Zenith_Maths::Vector3(
		sqrtf(glm::max(0.0f, xVar.x)), sqrtf(glm::max(0.0f, xVar.y)), sqrtf(glm::max(0.0f, xVar.z)));
}

// --- disocclusion rejection --------------------------------------------------
// History carries the linear view depth it was written at (in its alpha). Reject it
// when that disagrees with the depth the previous frame is EXPECTED to have had at the
// reprojected position (reconstructed from current depth + prev view-proj). Relative
// threshold so it scales with distance. Returns true = REJECT (a disocclusion / edge).
inline bool Flux_TAADisocclusionReject(float fHistoryPrevDepth, float fExpectedPrevDepth, float fRelThreshold)
{
	const float fDenom = glm::max(glm::max(fHistoryPrevDepth, fExpectedPrevDepth), 1e-4f);
	return (fabsf(fHistoryPrevDepth - fExpectedPrevDepth) / fDenom) > fRelThreshold;
}

// --- velocity-ramped blend factor --------------------------------------------
// alpha = lerp(minAlpha, maxAlpha, saturate(velPixels / velThreshold)). Slow pixels
// accumulate a long history (min alpha => sharpest AA); fast pixels lean on the current
// frame (max alpha => least ghosting). Monotonic non-decreasing in speed.
inline float Flux_TAABlendAlpha(float fVelPixels, float fVelThreshold, float fMinAlpha, float fMaxAlpha)
{
	const float fT = (fVelThreshold > 0.0f)
		? Zenith_Maths::Clamp(fVelPixels / fVelThreshold, 0.0f, 1.0f)
		: 1.0f;
	return fMinAlpha + (fMaxAlpha - fMinAlpha) * fT;
}

// Karis anti-flicker weight: down-weight bright samples so a lone firefly cannot dominate
// the blend. weight = 1 / (1 + luma). Luma is the YCoCg Y channel (already luminance).
inline float Flux_TAAKarisWeight(float fLuma)
{
	return 1.0f / (1.0f + glm::max(0.0f, fLuma));
}

// The luma-weighted current/history combine. bHistoryValid == false (first frame after a
// graph rebuild / invalidation) => output the current frame verbatim. Inputs are RGB; the
// Karis weight uses each colour's luminance.
inline Zenith_Maths::Vector3 Flux_TAAResolveBlend(
	const Zenith_Maths::Vector3& xCurrent, const Zenith_Maths::Vector3& xHistory,
	float fAlpha, bool bHistoryValid)
{
	if (!bHistoryValid)
	{
		return xCurrent;
	}
	const float fLumaCur  = Flux_RGBToYCoCg(xCurrent).x;
	const float fLumaHist = Flux_RGBToYCoCg(xHistory).x;
	const float fWCur  = fAlpha        * Flux_TAAKarisWeight(fLumaCur);
	const float fWHist = (1.0f - fAlpha) * Flux_TAAKarisWeight(fLumaHist);
	const float fDenom = glm::max(fWCur + fWHist, 1e-6f);
	return (xCurrent * fWCur + xHistory * fWHist) / fDenom;
}

// --- RCAS-style sharpen (5-tap cross, min/max limited) -----------------------
// sharpened = centre + amount*(centre - avg(neighbours)), clamped to the [min,max] of the
// 5 taps so it can never overshoot (ring). amount 0 => identity; a constant neighbourhood
// is returned unchanged (the effective kernel sums to 1 for any amount).
inline Zenith_Maths::Vector3 Flux_TAASharpen(
	const Zenith_Maths::Vector3& xCentre,
	const Zenith_Maths::Vector3& xUp, const Zenith_Maths::Vector3& xDown,
	const Zenith_Maths::Vector3& xLeft, const Zenith_Maths::Vector3& xRight,
	float fAmount)
{
	const Zenith_Maths::Vector3 xAvg   = 0.25f * (xUp + xDown + xLeft + xRight);
	const Zenith_Maths::Vector3 xSharp = xCentre + fAmount * (xCentre - xAvg);
	const Zenith_Maths::Vector3 xMin = glm::min(xCentre, glm::min(glm::min(xUp, xDown), glm::min(xLeft, xRight)));
	const Zenith_Maths::Vector3 xMax = glm::max(xCentre, glm::max(glm::max(xUp, xDown), glm::max(xLeft, xRight)));
	return glm::clamp(xSharp, xMin, xMax);
}

// --- upscale reconstruction kernel -------------------------------------------
// Gaussian weight for a render-sample at fDistPixels from an output-pixel centre.
// sigma ~= 0.47 gives a ~3x3 support ("FSR2-lite" scatter-as-gather). weight(0) == 1.
inline float Flux_TAAUpscaleWeight(float fDistPixels, float fSigma)
{
	const float fX = fDistPixels / fSigma;
	return expf(-0.5f * fX * fX);
}

// Reconstruct the current-frame colour at an OUTPUT pixel from the RENDER-res source
// (temporal upscaling). Scatter-as-gather: express the output pixel's centre in render-pixel
// coordinates, gather the surrounding 3x3 render texels, weight each by a Gaussian of its centre's
// distance to the output position, and normalise. A uniform source reconstructs exactly (Σw
// cancels). This is the GOLDEN mirror the resolve shader's gather transliterates line-for-line;
// the shader only takes this branch when renderDim != outputDim (at render==output it keeps the
// single centre tap, so the scale-1 path is byte-identical). fnSampleRenderUV(uv) samples the
// render-res colour at a [0,1] UV (bilinear, like the shader's Sampler2D).
template <typename SampleFn>
inline Zenith_Maths::Vector3 Flux_TAAUpscaleReconstruct(
	const Zenith_Maths::Vector2& xOutputUV, u_int uRenderW, u_int uRenderH, float fSigma, SampleFn&& fnSampleRenderUV)
{
	const Zenith_Maths::Vector2 xRenderDim(static_cast<float>(uRenderW), static_cast<float>(uRenderH));
	const Zenith_Maths::Vector2 xRenderCoord(xOutputUV.x * xRenderDim.x, xOutputUV.y * xRenderDim.y);
	const Zenith_Maths::Vector2 xBase(floorf(xRenderCoord.x), floorf(xRenderCoord.y));
	Zenith_Maths::Vector3 xAccum(0.0f);
	float fWSum = 0.0f;
	for (int iY = -1; iY <= 1; ++iY)
	{
		for (int iX = -1; iX <= 1; ++iX)
		{
			const Zenith_Maths::Vector2 xCentre(xBase.x + static_cast<float>(iX) + 0.5f,
			                                    xBase.y + static_cast<float>(iY) + 0.5f);
			const Zenith_Maths::Vector2 xTapUV(xCentre.x / xRenderDim.x, xCentre.y / xRenderDim.y);
			const Zenith_Maths::Vector2 xD(xCentre.x - xRenderCoord.x, xCentre.y - xRenderCoord.y);
			const float fDist = sqrtf(xD.x * xD.x + xD.y * xD.y);
			const float fW    = Flux_TAAUpscaleWeight(fDist, fSigma);
			xAccum += fnSampleRenderUV(xTapUV) * fW;
			fWSum  += fW;
		}
	}
	return xAccum / glm::max(fWSum, 1e-6f);
}

// Even-quantised render dimensions for a given output size and render scale.
// scale >= 1 => identity (exact output dims, no rounding). Otherwise round to the nearest
// EVEN value (chroma/2x2 friendly), never below 2, never above output.
inline Zenith_Maths::UVector2 Flux_TAAComputeRenderDims(u_int uOutW, u_int uOutH, float fScale)
{
	const float fClamped = Zenith_Maths::Clamp(fScale, 0.5f, 1.0f);
	if (fClamped >= 1.0f)
	{
		return Zenith_Maths::UVector2(uOutW, uOutH);
	}
	auto RoundEven = [](u_int uOut, float fS) -> u_int
	{
		u_int uR = static_cast<u_int>(static_cast<float>(uOut) * fS + 0.5f);
		uR &= ~1u;                       // to even
		if (uR < 2u)    { uR = 2u; }     // never zero
		if (uR > uOut)  { uR = uOut; }   // never exceed output
		return uR;
	};
	return Zenith_Maths::UVector2(RoundEven(uOutW, fClamped), RoundEven(uOutH, fClamped));
}
