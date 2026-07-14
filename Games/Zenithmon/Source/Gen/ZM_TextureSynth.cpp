#include "Zenith.h"

#include "Zenithmon/Source/Gen/ZM_TextureSynth.h"

#ifdef ZENITH_TOOLS
// The offline texture exporter lives under the engine /Tools tree, which is not
// on a game's include search path, so reach it with a relative path (mirrors
// RenderTest_Tennis.cpp / Zenith_EditorPanel_ContentBrowser.cpp).
#include "../../Tools/Zenith_Tools_TextureExport.h"
#include <filesystem>
#include <string>
#endif

#include <cmath>
#include <cstring>

using Zenith_Maths::Vector2;
using Zenith_Maths::Vector3;
using Zenith_Maths::Vector4;

// ===========================================================================
// File-local helpers
// ===========================================================================
namespace
{
	inline float Clamp01(float f)
	{
		return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
	}

	// Standard sRGB OETF (linear -> sRGB), per channel.
	inline float LinearToSRGB(float f)
	{
		f = Clamp01(f);
		return f <= 0.0031308f ? f * 12.92f : 1.055f * powf(f, 1.0f / 2.4f) - 0.055f;
	}

	// Deterministic quantise: (u_int8)(clamp01(c) * 255 + 0.5).
	inline u_int8 QuantiseU8(float f)
	{
		return (u_int8)(Clamp01(f) * 255.0f + 0.5f);
	}

	inline float Lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	inline Vector3 Lerp3(const Vector3& a, const Vector3& b, float t)
	{
		return Vector3(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t));
	}

	// UV of a texel centre using the (w-1)/(h-1) normalisation the decals + fills
	// share, so bound tests are exact. A 1x1 axis degenerates to 0.
	inline float NormU(u_int uX, u_int uWidth)
	{
		return uWidth <= 1u ? 0.0f : (float)uX / (float)(uWidth - 1u);
	}
	inline float NormV(u_int uY, u_int uHeight)
	{
		return uHeight <= 1u ? 0.0f : (float)uY / (float)(uHeight - 1u);
	}

	// RGB -> HSV. Hue in [0,360), sat/val in [0,1].
	Vector3 RGBToHSV(const Vector3& xRGB)
	{
		const float fR = xRGB.x, fG = xRGB.y, fB = xRGB.z;
		const float fMax = fmaxf(fR, fmaxf(fG, fB));
		const float fMin = fminf(fR, fminf(fG, fB));
		const float fDelta = fMax - fMin;

		float fH = 0.0f;
		if (fDelta > 1.0e-6f)
		{
			if (fMax == fR)
			{
				fH = fmodf((fG - fB) / fDelta, 6.0f);
			}
			else if (fMax == fG)
			{
				fH = (fB - fR) / fDelta + 2.0f;
			}
			else
			{
				fH = (fR - fG) / fDelta + 4.0f;
			}
			fH *= 60.0f;
			if (fH < 0.0f)
			{
				fH += 360.0f;
			}
		}
		const float fS = fMax <= 1.0e-6f ? 0.0f : fDelta / fMax;
		return Vector3(fH, fS, fMax);
	}

	// HSV -> RGB. Hue in [0,360).
	Vector3 HSVToRGB(const Vector3& xHSV)
	{
		const float fH = xHSV.x;
		const float fS = xHSV.y;
		const float fV = xHSV.z;

		const float fC = fV * fS;
		const float fX = fC * (1.0f - fabsf(fmodf(fH / 60.0f, 2.0f) - 1.0f));
		const float fM = fV - fC;

		float fR = 0.0f, fG = 0.0f, fB = 0.0f;
		if (fH < 60.0f)       { fR = fC; fG = fX; fB = 0.0f; }
		else if (fH < 120.0f) { fR = fX; fG = fC; fB = 0.0f; }
		else if (fH < 180.0f) { fR = 0.0f; fG = fC; fB = fX; }
		else if (fH < 240.0f) { fR = 0.0f; fG = fX; fB = fC; }
		else if (fH < 300.0f) { fR = fX; fG = 0.0f; fB = fC; }
		else                  { fR = fC; fG = 0.0f; fB = fX; }

		return Vector3(fR + fM, fG + fM, fB + fM);
	}

	// The 18 elemental palettes, indexed by ZM_TYPE (0..ZM_TYPE_COUNT-1). All
	// components are in-gamut [0,1]; the primaries are deliberately distinct so
	// the "not all equal" palette-coverage test holds.
	const ZM_TypePalette s_axTypePalettes[ZM_TYPE_COUNT] =
	{
		/* NORMAL   */ { Vector3(0.72f, 0.66f, 0.55f), Vector3(0.45f, 0.38f, 0.30f), Vector3(0.90f, 0.86f, 0.78f) },
		/* FIRE     */ { Vector3(0.85f, 0.25f, 0.12f), Vector3(0.98f, 0.72f, 0.20f), Vector3(0.96f, 0.55f, 0.30f) },
		/* WATER    */ { Vector3(0.15f, 0.40f, 0.80f), Vector3(0.40f, 0.75f, 0.95f), Vector3(0.75f, 0.88f, 0.96f) },
		/* GRASS    */ { Vector3(0.25f, 0.65f, 0.28f), Vector3(0.55f, 0.80f, 0.30f), Vector3(0.80f, 0.90f, 0.60f) },
		/* ELECTRIC */ { Vector3(0.95f, 0.85f, 0.15f), Vector3(0.98f, 0.90f, 0.45f), Vector3(0.98f, 0.95f, 0.75f) },
		/* ICE      */ { Vector3(0.65f, 0.88f, 0.92f), Vector3(0.85f, 0.95f, 0.98f), Vector3(0.92f, 0.97f, 0.99f) },
		/* BRAWL    */ { Vector3(0.60f, 0.25f, 0.20f), Vector3(0.80f, 0.45f, 0.30f), Vector3(0.85f, 0.65f, 0.55f) },
		/* VENOM    */ { Vector3(0.55f, 0.20f, 0.65f), Vector3(0.80f, 0.35f, 0.85f), Vector3(0.75f, 0.55f, 0.80f) },
		/* EARTH    */ { Vector3(0.55f, 0.40f, 0.22f), Vector3(0.75f, 0.60f, 0.35f), Vector3(0.85f, 0.75f, 0.55f) },
		/* SKY      */ { Vector3(0.55f, 0.75f, 0.95f), Vector3(0.90f, 0.95f, 0.98f), Vector3(0.85f, 0.90f, 0.96f) },
		/* MIND     */ { Vector3(0.90f, 0.45f, 0.70f), Vector3(0.65f, 0.35f, 0.80f), Vector3(0.95f, 0.75f, 0.85f) },
		/* SWARM    */ { Vector3(0.55f, 0.60f, 0.20f), Vector3(0.75f, 0.78f, 0.30f), Vector3(0.80f, 0.82f, 0.55f) },
		/* STONE    */ { Vector3(0.55f, 0.52f, 0.48f), Vector3(0.38f, 0.36f, 0.33f), Vector3(0.72f, 0.70f, 0.66f) },
		/* PHANTOM  */ { Vector3(0.30f, 0.25f, 0.45f), Vector3(0.50f, 0.40f, 0.65f), Vector3(0.60f, 0.55f, 0.72f) },
		/* DRAKE    */ { Vector3(0.30f, 0.35f, 0.60f), Vector3(0.35f, 0.65f, 0.60f), Vector3(0.70f, 0.75f, 0.80f) },
		/* UMBRAL   */ { Vector3(0.15f, 0.15f, 0.20f), Vector3(0.35f, 0.25f, 0.40f), Vector3(0.45f, 0.42f, 0.50f) },
		/* IRON     */ { Vector3(0.60f, 0.63f, 0.68f), Vector3(0.78f, 0.80f, 0.85f), Vector3(0.85f, 0.87f, 0.90f) },
		/* FEY      */ { Vector3(0.95f, 0.70f, 0.85f), Vector3(0.98f, 0.85f, 0.92f), Vector3(0.99f, 0.90f, 0.95f) },
	};
}

// ===========================================================================
// ZM_GenImage
// ===========================================================================
ZM_GenImage::ZM_GenImage(u_int uWidth, u_int uHeight)
	: m_uWidth(uWidth)
	, m_uHeight(uHeight)
{
	const u_int uTexels = uWidth * uHeight;
	m_xRGBA.Reserve(uTexels * 4u);
	for (u_int u = 0u; u < uTexels; u++)
	{
		m_xRGBA.PushBack(0.0f);   // R
		m_xRGBA.PushBack(0.0f);   // G
		m_xRGBA.PushBack(0.0f);   // B
		m_xRGBA.PushBack(1.0f);   // A
	}
}

Vector4 ZM_GenImage::Get(u_int uY, u_int uX) const
{
	Zenith_Assert(uX < m_uWidth && uY < m_uHeight, "ZM_GenImage::Get out of range");
	const u_int uI = (uY * m_uWidth + uX) * 4u;
	return Vector4(m_xRGBA.Get(uI), m_xRGBA.Get(uI + 1u), m_xRGBA.Get(uI + 2u), m_xRGBA.Get(uI + 3u));
}

void ZM_GenImage::Set(u_int uY, u_int uX, const Vector4& xRGBA)
{
	Zenith_Assert(uX < m_uWidth && uY < m_uHeight, "ZM_GenImage::Set out of range");
	const u_int uI = (uY * m_uWidth + uX) * 4u;
	m_xRGBA.Get(uI)      = xRGBA.x;
	m_xRGBA.Get(uI + 1u) = xRGBA.y;
	m_xRGBA.Get(uI + 2u) = xRGBA.z;
	m_xRGBA.Get(uI + 3u) = xRGBA.w;
}

void ZM_GenImage::PackRGBA8(Zenith_Vector<u_int8>& xOut, bool bSRGBEncode) const
{
	const u_int uTexels = m_uWidth * m_uHeight;
	xOut.Clear();
	xOut.Reserve(uTexels * 4u);
	for (u_int u = 0u; u < uTexels; u++)
	{
		const u_int uI = u * 4u;
		float fR = m_xRGBA.Get(uI);
		float fG = m_xRGBA.Get(uI + 1u);
		float fB = m_xRGBA.Get(uI + 2u);
		const float fA = m_xRGBA.Get(uI + 3u);   // alpha stays linear
		if (bSRGBEncode)
		{
			fR = LinearToSRGB(fR);
			fG = LinearToSRGB(fG);
			fB = LinearToSRGB(fB);
		}
		xOut.PushBack(QuantiseU8(fR));
		xOut.PushBack(QuantiseU8(fG));
		xOut.PushBack(QuantiseU8(fB));
		xOut.PushBack(QuantiseU8(fA));
	}
}

bool ZM_GenImage::Equals(const ZM_GenImage& xOther) const
{
	if (m_uWidth != xOther.m_uWidth || m_uHeight != xOther.m_uHeight)
	{
		return false;
	}
	Zenith_Vector<u_int8> xA, xB;
	PackRGBA8(xA, false);
	xOther.PackRGBA8(xB, false);
	if (xA.GetSize() != xB.GetSize())
	{
		return false;
	}
	if (xA.GetSize() == 0u)
	{
		return true;
	}
	return memcmp(xA.GetDataPointer(), xB.GetDataPointer(), xA.GetSize()) == 0;
}

u_int ZM_GenImage::ContentHash() const
{
	Zenith_Vector<u_int8> xBytes;
	PackRGBA8(xBytes, false);
	u_int uHash = 2166136261u;
	for (u_int u = 0u; u < xBytes.GetSize(); u++)
	{
		uHash ^= (u_int)xBytes.Get(u);
		uHash *= 16777619u;
	}
	return uHash;
}

// ===========================================================================
// Palette
// ===========================================================================
ZM_TypePalette ZM_SynthTypePalette(ZM_TYPE eType)
{
	Zenith_Assert((u_int)eType < (u_int)ZM_TYPE_COUNT, "ZM_SynthTypePalette: type out of range");
	if ((u_int)eType >= (u_int)ZM_TYPE_COUNT)
	{
		return s_axTypePalettes[ZM_TYPE_NORMAL];   // deterministic fallback
	}
	return s_axTypePalettes[(u_int)eType];
}

ZM_TypePalette ZM_SynthBlendPalette(ZM_TYPE ePrimary, ZM_TYPE eSecondary)
{
	const ZM_TypePalette xPrim = ZM_SynthTypePalette(ePrimary);
	if (eSecondary == ZM_TYPE_NONE)
	{
		return xPrim;   // mono-type: unchanged
	}
	const ZM_TypePalette xSec = ZM_SynthTypePalette(eSecondary);

	ZM_TypePalette xOut;
	// 60/40 primary-weighted body; order-sensitive since the 0.6 weight tracks
	// the primary slot.
	xOut.m_xBase   = xPrim.m_xBase * 0.6f + xSec.m_xBase * 0.4f;
	xOut.m_xAccent = xSec.m_xBase;      // secondary's identity supplies the detail ink
	xOut.m_xBelly  = xPrim.m_xBelly;    // underside stays the primary's
	return xOut;
}

// ===========================================================================
// Fills + patterns
// ===========================================================================
void ZM_SynthFillSolid(ZM_GenImage& xImg, const Vector3& xColour)
{
	const Vector4 xRGBA(xColour.x, xColour.y, xColour.z, 1.0f);
	for (u_int uY = 0u; uY < xImg.GetHeight(); uY++)
	{
		for (u_int uX = 0u; uX < xImg.GetWidth(); uX++)
		{
			xImg.Set(uY, uX, xRGBA);
		}
	}
}

void ZM_SynthFillGradient(ZM_GenImage& xImg, const Vector3& xTop, const Vector3& xBottom)
{
	for (u_int uY = 0u; uY < xImg.GetHeight(); uY++)
	{
		const float fT = NormV(uY, xImg.GetHeight());   // 0 at top, 1 at bottom
		const Vector3 xC = Lerp3(xTop, xBottom, fT);
		const Vector4 xRGBA(xC.x, xC.y, xC.z, 1.0f);
		for (u_int uX = 0u; uX < xImg.GetWidth(); uX++)
		{
			xImg.Set(uY, uX, xRGBA);
		}
	}
}

// Horizontal ink bands along V. Pure (no RNG). A band covers the first half of
// each period; m_fContrast scales the ink blend so it never fully clobbers the
// base unless requested.
void ZM_SynthApplyStripes(ZM_GenImage& xImg, const ZM_PatternParams& xParams, const Vector3& xInk)
{
	const float fFreq = xParams.m_fFrequency > 0.0f ? xParams.m_fFrequency : 1.0f;
	const float fContrast = Clamp01(xParams.m_fContrast);
	for (u_int uY = 0u; uY < xImg.GetHeight(); uY++)
	{
		const float fV = NormV(uY, xImg.GetHeight());
		const float fBand = fV * fFreq;
		const float fFrac = fBand - floorf(fBand);
		if (fFrac < 0.5f)
		{
			for (u_int uX = 0u; uX < xImg.GetWidth(); uX++)
			{
				const Vector4 xSrc = xImg.Get(uY, uX);
				const Vector3 xBase(xSrc.x, xSrc.y, xSrc.z);
				const Vector3 xC = Lerp3(xBase, xInk, fContrast);
				xImg.Set(uY, uX, Vector4(xC.x, xC.y, xC.z, xSrc.w));
			}
		}
	}
}

// The ONLY RNG-driven fill. Draw order is FIXED and unit-tested: uCount is fixed
// from the params up front (never conditioned on a float threshold), and for
// each spot i = 0..uCount-1 the stream is drawn EXACTLY in the order
// centreU, centreV, radius, softness -- then the soft disc is rasterised (no
// further draws), so the PCG stream position is a pure function of uCount.
void ZM_SynthApplySpots(ZM_GenImage& xImg, const ZM_PatternParams& xParams, const Vector3& xInk, ZM_GenRNG& xRng)
{
	const u_int uCount = xParams.m_uCount;
	const float fFreq = xParams.m_fFrequency > 0.0f ? xParams.m_fFrequency : 1.0f;
	const float fContrast = Clamp01(xParams.m_fContrast);
	const float fBaseR = 0.5f / fFreq;
	const float fMinR = fBaseR * 0.5f;
	const float fMaxR = fBaseR * 1.0f;

	for (u_int uSpot = 0u; uSpot < uCount; uSpot++)
	{
		// FIXED draw order -- do not reorder.
		const float fCentreU  = xRng.NextFloat01();
		const float fCentreV  = xRng.NextFloat01();
		const float fRadius   = xRng.NextFloatRange(fMinR, fMaxR);
		const float fSoftness = xRng.NextFloat01();

		const float fEdge = fRadius * (0.2f + 0.8f * fSoftness);
		for (u_int uY = 0u; uY < xImg.GetHeight(); uY++)
		{
			const float fV = NormV(uY, xImg.GetHeight());
			for (u_int uX = 0u; uX < xImg.GetWidth(); uX++)
			{
				const float fU = NormU(uX, xImg.GetWidth());
				const float fDU = fU - fCentreU;
				const float fDV = fV - fCentreV;
				const float fD = sqrtf(fDU * fDU + fDV * fDV);
				if (fD < fRadius)
				{
					const float fFall = fEdge > 1.0e-6f ? Clamp01((fRadius - fD) / fEdge) : 1.0f;
					const float fBlend = fFall * fContrast;
					const Vector4 xSrc = xImg.Get(uY, uX);
					const Vector3 xBase(xSrc.x, xSrc.y, xSrc.z);
					const Vector3 xC = Lerp3(xBase, xInk, fBlend);
					xImg.Set(uY, uX, Vector4(xC.x, xC.y, xC.z, xSrc.w));
				}
			}
		}
	}
}

// Soft underside blend: below fSplitV (higher V == lower body), lerp toward the
// belly colour over an fSoftness feather. Pure (no RNG).
void ZM_SynthApplyBelly(ZM_GenImage& xImg, const Vector3& xBelly, float fSplitV, float fSoftness)
{
	const float fFeather = fSoftness > 1.0e-6f ? fSoftness : 1.0e-6f;
	for (u_int uY = 0u; uY < xImg.GetHeight(); uY++)
	{
		const float fV = NormV(uY, xImg.GetHeight());
		const float fT = Clamp01((fV - fSplitV) / fFeather);
		if (fT > 0.0f)
		{
			for (u_int uX = 0u; uX < xImg.GetWidth(); uX++)
			{
				const Vector4 xSrc = xImg.Get(uY, uX);
				const Vector3 xBase(xSrc.x, xSrc.y, xSrc.z);
				const Vector3 xC = Lerp3(xBase, xBelly, fT);
				xImg.Set(uY, uX, Vector4(xC.x, xC.y, xC.z, xSrc.w));
			}
		}
	}
}

void ZM_SynthStampEyeDecal(ZM_GenImage& xImg, float fCentreU, float fCentreV, float fRadius,
	const Vector3& xIris, const Vector3& xPupil)
{
	if (fRadius <= 0.0f)
	{
		return;
	}
	const float fPupilR = fRadius * 0.45f;
	for (u_int uY = 0u; uY < xImg.GetHeight(); uY++)
	{
		const float fV = NormV(uY, xImg.GetHeight());
		for (u_int uX = 0u; uX < xImg.GetWidth(); uX++)
		{
			const float fU = NormU(uX, xImg.GetWidth());
			const float fDU = fU - fCentreU;
			const float fDV = fV - fCentreV;
			const float fD = sqrtf(fDU * fDU + fDV * fDV);
			if (fD <= fRadius)
			{
				const Vector4 xSrc = xImg.Get(uY, uX);
				const Vector3& xC = (fD <= fPupilR) ? xPupil : xIris;
				xImg.Set(uY, uX, Vector4(xC.x, xC.y, xC.z, xSrc.w));
			}
		}
	}
}

// Axis-aligned rect fill. Writes ONLY inside [u0,u1] x [v0,v1] (inclusive); every
// texel outside is left untouched -- the building-gen window/door reuse contract.
void ZM_SynthStampRectDecal(ZM_GenImage& xImg, float fU0, float fV0, float fU1, float fV1,
	const Vector3& xColour)
{
	const float fLoU = fminf(fU0, fU1);
	const float fHiU = fmaxf(fU0, fU1);
	const float fLoV = fminf(fV0, fV1);
	const float fHiV = fmaxf(fV0, fV1);
	for (u_int uY = 0u; uY < xImg.GetHeight(); uY++)
	{
		const float fV = NormV(uY, xImg.GetHeight());
		if (fV < fLoV || fV > fHiV)
		{
			continue;
		}
		for (u_int uX = 0u; uX < xImg.GetWidth(); uX++)
		{
			const float fU = NormU(uX, xImg.GetWidth());
			if (fU < fLoU || fU > fHiU)
			{
				continue;
			}
			const Vector4 xSrc = xImg.Get(uY, uX);
			xImg.Set(uY, uX, Vector4(xColour.x, xColour.y, xColour.z, xSrc.w));
		}
	}
}

// ===========================================================================
// Shiny hue-rotate
// ===========================================================================
ZM_GenImage ZM_SynthHueRotate(const ZM_GenImage& xSrc, float fDegrees)
{
	ZM_GenImage xOut(xSrc.GetWidth(), xSrc.GetHeight());
	if (xSrc.IsEmpty())
	{
		return xOut;
	}

	// Pass 1: measure chroma. An achromatic (grey/white/black) source has no hue
	// to rotate, so we take the nudge path instead to guarantee a differing hash.
	// The threshold sits just ABOVE the 8-bit quantization step (1/255 ~= 3.9e-3):
	// a near-grey source with saturation below this can round back byte-identical
	// after a hue rotation, which would trip the ContentHash assert below in
	// tools/debug. Anything that low takes the deterministic nudge path instead.
	float fMaxSat = 0.0f;
	for (u_int uY = 0u; uY < xSrc.GetHeight(); uY++)
	{
		for (u_int uX = 0u; uX < xSrc.GetWidth(); uX++)
		{
			const Vector4 xTex = xSrc.Get(uY, uX);
			const Vector3 xHSV = RGBToHSV(Vector3(xTex.x, xTex.y, xTex.z));
			fMaxSat = fmaxf(fMaxSat, xHSV.y);
		}
	}
	const bool bAchromatic = fMaxSat <= 2.0f / 255.0f;

	// Normalise the rotation into [0,360).
	float fRot = fmodf(fDegrees, 360.0f);
	if (fRot < 0.0f)
	{
		fRot += 360.0f;
	}

	for (u_int uY = 0u; uY < xSrc.GetHeight(); uY++)
	{
		for (u_int uX = 0u; uX < xSrc.GetWidth(); uX++)
		{
			const Vector4 xTex = xSrc.Get(uY, uX);
			Vector3 xRGB(xTex.x, xTex.y, xTex.z);
			if (bAchromatic)
			{
				// Fixed deterministic tone shift; leaves no texel value bit-identical
				// (0 -> 0.03, 1 -> 0.93), so ContentHash always changes.
				xRGB = Vector3(Clamp01(xRGB.x * 0.9f + 0.03f),
					Clamp01(xRGB.y * 0.9f + 0.03f),
					Clamp01(xRGB.z * 0.9f + 0.03f));
			}
			else
			{
				Vector3 xHSV = RGBToHSV(xRGB);
				xHSV.x = fmodf(xHSV.x + fRot, 360.0f);
				xRGB = HSVToRGB(xHSV);
			}
			xOut.Set(uY, uX, Vector4(xRGB.x, xRGB.y, xRGB.z, xTex.w));
		}
	}

	Zenith_Assert(xOut.ContentHash() != xSrc.ContentHash(),
		"ZM_SynthHueRotate produced a byte-identical shiny variant");
	return xOut;
}

// ===========================================================================
// Normal from height
// ===========================================================================
ZM_GenImage ZM_SynthNormalFromHeight(const ZM_GenImage& xHeightSrc, float fStrength)
{
	const u_int uW = xHeightSrc.GetWidth();
	const u_int uH = xHeightSrc.GetHeight();
	ZM_GenImage xOut(uW, uH);
	if (xHeightSrc.IsEmpty())
	{
		return xOut;
	}

	const float fScale = 2.2f * ((float)uW / 1024.0f) * fStrength;
	for (u_int uY = 0u; uY < uH; uY++)
	{
		const u_int uYP = (uY + 1u < uH) ? uY + 1u : uH - 1u;   // clamp edges
		const u_int uYM = (uY > 0u) ? uY - 1u : 0u;
		for (u_int uX = 0u; uX < uW; uX++)
		{
			const u_int uXP = (uX + 1u < uW) ? uX + 1u : uW - 1u;
			const u_int uXM = (uX > 0u) ? uX - 1u : 0u;

			const float fHXP = xHeightSrc.Get(uY, uXP).x;
			const float fHXM = xHeightSrc.Get(uY, uXM).x;
			const float fHYP = xHeightSrc.Get(uYP, uX).x;
			const float fHYM = xHeightSrc.Get(uYM, uX).x;

			const float fDX = (fHXP - fHXM) * fScale;
			const float fDY = (fHYP - fHYM) * fScale;

			Vector3 xN(-fDX, -fDY, 1.0f);
			const float fLenSq = xN.x * xN.x + xN.y * xN.y + xN.z * xN.z;
			if (fLenSq > 1.0e-12f)
			{
				xN = xN * (1.0f / sqrtf(fLenSq));
			}
			else
			{
				xN = Vector3(0.0f, 0.0f, 1.0f);   // flat fallback
			}

			xOut.Set(uY, uX, Vector4(xN.x * 0.5f + 0.5f, xN.y * 0.5f + 0.5f, xN.z * 0.5f + 0.5f, 1.0f));
		}
	}
	return xOut;
}

// ===========================================================================
// Top-level creature albedo
// ===========================================================================
// FIXED DRAW ORDER (documented + unit-tested for determinism):
//   1. Resolve palette (mono TypePalette, or BlendPalette for dual-type).
//   2. FillGradient base -> slightly-darker base (body shading).
//   3. Pattern layer, switched on m_xPattern.m_eKind:
//        STRIPES  -> ApplyStripes(accent)          [pure]
//        SPOTS    -> ApplySpots(accent, xRng)       [ONLY RNG consumer]
//        GRADIENT -> (nothing; base already graded)
//        BELLY    -> (nothing here; step 4 owns it)
//        NONE     -> (nothing)
//   4. ApplyBelly(palette.belly, split 0.6, softness 0.15).
//   5. StampEyeDecal(eye, iris = accent, pupil = near-black).
// Only step 3/SPOTS consumes xRng, so a non-SPOTS recipe never touches the
// stream and any recipe is a pure function of (recipe, seed).
ZM_GenImage ZM_SynthCreatureAlbedo(const ZM_CreatureTexRecipe& xRecipe, ZM_GenRNG& xRng)
{
	const ZM_TypePalette xPalette = (xRecipe.m_eSecondaryType == ZM_TYPE_NONE)
		? ZM_SynthTypePalette(xRecipe.m_ePrimaryType)
		: ZM_SynthBlendPalette(xRecipe.m_ePrimaryType, xRecipe.m_eSecondaryType);

	ZM_GenImage xImg(xRecipe.m_uWidth, xRecipe.m_uHeight);

	// 2. Base gradient (slightly darker toward the bottom).
	ZM_SynthFillGradient(xImg, xPalette.m_xBase, xPalette.m_xBase * 0.85f);

	// 3. Pattern layer.
	switch (xRecipe.m_xPattern.m_eKind)
	{
	case ZM_PATTERN_STRIPES:
		ZM_SynthApplyStripes(xImg, xRecipe.m_xPattern, xPalette.m_xAccent);
		break;
	case ZM_PATTERN_SPOTS:
		ZM_SynthApplySpots(xImg, xRecipe.m_xPattern, xPalette.m_xAccent, xRng);
		break;
	case ZM_PATTERN_GRADIENT:
	case ZM_PATTERN_BELLY:
	case ZM_PATTERN_NONE:
	default:
		break;
	}

	// 4. Underside.
	ZM_SynthApplyBelly(xImg, xPalette.m_xBelly, 0.6f, 0.15f);

	// 5. Eye.
	const Vector3 xPupil(0.05f, 0.05f, 0.06f);
	ZM_SynthStampEyeDecal(xImg, xRecipe.m_fEyeU, xRecipe.m_fEyeV, xRecipe.m_fEyeRadius,
		xPalette.m_xAccent, xPupil);

	return xImg;
}

// ===========================================================================
// .ztxtr bake bridges (TOOLS ONLY)
// ===========================================================================
#ifdef ZENITH_TOOLS
namespace
{
	void EnsureParentDir(const char* szPath)
	{
		std::error_code xEc;
		const std::filesystem::path xParent = std::filesystem::path(szPath).parent_path();
		if (!xParent.empty())
		{
			std::filesystem::create_directories(xParent, xEc);   // non-throwing; tolerates existing/absent
		}
	}
}

bool ZM_SynthBakeAlbedoBC1(const ZM_GenImage& xImg, const char* szPath)
{
	if (xImg.IsEmpty() || szPath == nullptr)
	{
		return false;
	}
	// BC1 has no sRGB variant, so bake the sRGB OETF into the bytes and sample as
	// UNORM (matches the engine's compressed-colour convention).
	Zenith_Vector<u_int8> xBytes;
	xImg.PackRGBA8(xBytes, /*bSRGBEncode*/ true);
	EnsureParentDir(szPath);
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xBytes.GetDataPointer(), std::string(szPath),
		(int32_t)xImg.GetWidth(), (int32_t)xImg.GetHeight(), TextureCompressionMode::BC1);
	return true;
}

bool ZM_SynthBakeAlbedoSRGB(const ZM_GenImage& xImg, const char* szPath)
{
	if (xImg.IsEmpty() || szPath == nullptr)
	{
		return false;
	}
	// The RGBA8_SRGB format flag carries sRGB, so pack LINEAR bytes (matches the
	// StickFigure_Albedo path).
	Zenith_Vector<u_int8> xBytes;
	xImg.PackRGBA8(xBytes, /*bSRGBEncode*/ false);
	EnsureParentDir(szPath);
	Zenith_Tools_TextureExport::ExportFromDataV2Uncompressed(
		xBytes.GetDataPointer(), std::string(szPath),
		(int32_t)xImg.GetWidth(), (int32_t)xImg.GetHeight(), TEXTURE_FORMAT_RGBA8_SRGB);
	return true;
}

bool ZM_SynthBakeNormalBC5(const ZM_GenImage& xNormalImg, const char* szPath)
{
	if (xNormalImg.IsEmpty() || szPath == nullptr)
	{
		return false;
	}
	// Normals are data, not colour: pack non-sRGB, BC5 keeps R,G and the shader
	// rebuilds Z.
	Zenith_Vector<u_int8> xBytes;
	xNormalImg.PackRGBA8(xBytes, /*bSRGBEncode*/ false);
	EnsureParentDir(szPath);
	Zenith_Tools_TextureExport::ExportFromDataCompressed(
		xBytes.GetDataPointer(), std::string(szPath),
		(int32_t)xNormalImg.GetWidth(), (int32_t)xNormalImg.GetHeight(), TextureCompressionMode::BC5);
	return true;
}

bool ZM_SynthBakeIconBC1(const ZM_GenImage& xImg, const char* szPath)
{
	// Dex icons are displayed colour on a BC1 (no sRGB format) target -- same
	// bake-in-sRGB convention as the creature albedo.
	return ZM_SynthBakeAlbedoBC1(xImg, szPath);
}
#endif
