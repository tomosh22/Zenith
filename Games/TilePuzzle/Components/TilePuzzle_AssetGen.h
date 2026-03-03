#pragma once

#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Flux_Enums.h"
#include "TilePuzzle/Components/TilePuzzle_Types.h"
#include <cmath>
#include <cstring>
#include <filesystem>

// ============================================================================
// SDF Helper Library
// ============================================================================
// All functions operate on normalized [0,1] coordinates.
// Negative return = inside shape, positive = outside.

namespace TilePuzzle_SDF
{
	static constexpr float fPI = 3.14159265358979f;

	// --- Primitives ---

	static inline float Circle(float fX, float fY, float fCx, float fCy, float fR)
	{
		float fDx = fX - fCx;
		float fDy = fY - fCy;
		return sqrtf(fDx * fDx + fDy * fDy) - fR;
	}

	static inline float Box(float fX, float fY, float fCx, float fCy, float fHW, float fHH)
	{
		float fDx = fabsf(fX - fCx) - fHW;
		float fDy = fabsf(fY - fCy) - fHH;
		float fOutDist = sqrtf(fmaxf(fDx, 0.f) * fmaxf(fDx, 0.f) + fmaxf(fDy, 0.f) * fmaxf(fDy, 0.f));
		float fInDist = fminf(fmaxf(fDx, fDy), 0.f);
		return fOutDist + fInDist;
	}

	static inline float RoundedBox(float fX, float fY, float fCx, float fCy, float fHW, float fHH, float fR)
	{
		return Box(fX, fY, fCx, fCy, fHW - fR, fHH - fR) - fR;
	}

	static inline float Segment(float fX, float fY, float fAx, float fAy, float fBx, float fBy)
	{
		float fDx = fBx - fAx;
		float fDy = fBy - fAy;
		float fPx = fX - fAx;
		float fPy = fY - fAy;
		float fLen2 = fDx * fDx + fDy * fDy;
		float fT = (fLen2 > 0.f) ? fmaxf(0.f, fminf(1.f, (fPx * fDx + fPy * fDy) / fLen2)) : 0.f;
		float fProjX = fAx + fT * fDx - fX;
		float fProjY = fAy + fT * fDy - fY;
		return sqrtf(fProjX * fProjX + fProjY * fProjY);
	}

	static inline float Triangle(float fX, float fY, float fAx, float fAy, float fBx, float fBy, float fCx, float fCy)
	{
		// Signed distance to triangle using edge-based approach
		float fD0 = Segment(fX, fY, fAx, fAy, fBx, fBy);
		float fD1 = Segment(fX, fY, fBx, fBy, fCx, fCy);
		float fD2 = Segment(fX, fY, fCx, fCy, fAx, fAy);
		float fDist = fminf(fminf(fD0, fD1), fD2);

		// Check if point is inside triangle using cross product signs
		float fS0 = (fBx - fAx) * (fY - fAy) - (fBy - fAy) * (fX - fAx);
		float fS1 = (fCx - fBx) * (fY - fBy) - (fCy - fBy) * (fX - fBx);
		float fS2 = (fAx - fCx) * (fY - fCy) - (fAy - fCy) * (fX - fCx);

		bool bInside = (fS0 >= 0.f && fS1 >= 0.f && fS2 >= 0.f) ||
			(fS0 <= 0.f && fS1 <= 0.f && fS2 <= 0.f);

		return bInside ? -fDist : fDist;
	}

	static inline float Arc(float fX, float fY, float fCx, float fCy, float fR, float fStartAngle, float fEndAngle, float fThickness)
	{
		float fDx = fX - fCx;
		float fDy = fY - fCy;
		float fAngle = atan2f(fDy, fDx);
		if (fAngle < 0.f)
			fAngle += 2.f * fPI;

		// Normalize angles
		float fStart = fmodf(fStartAngle, 2.f * fPI);
		float fEnd = fmodf(fEndAngle, 2.f * fPI);
		if (fStart < 0.f) fStart += 2.f * fPI;
		if (fEnd < 0.f) fEnd += 2.f * fPI;

		bool bInArc;
		if (fStart <= fEnd)
		{
			bInArc = (fAngle >= fStart && fAngle <= fEnd);
		}
		else
		{
			bInArc = (fAngle >= fStart || fAngle <= fEnd);
		}

		if (bInArc)
		{
			float fDistToCircle = fabsf(sqrtf(fDx * fDx + fDy * fDy) - fR);
			return fDistToCircle - fThickness * 0.5f;
		}
		else
		{
			// Distance to nearest arc endpoint
			float fEx0 = fCx + fR * cosf(fStart);
			float fEy0 = fCy + fR * sinf(fStart);
			float fEx1 = fCx + fR * cosf(fEnd);
			float fEy1 = fCy + fR * sinf(fEnd);
			float fD0 = sqrtf((fX - fEx0) * (fX - fEx0) + (fY - fEy0) * (fY - fEy0));
			float fD1 = sqrtf((fX - fEx1) * (fX - fEx1) + (fY - fEy1) * (fY - fEy1));
			return fminf(fD0, fD1) - fThickness * 0.5f;
		}
	}

	static inline float Star5(float fX, float fY, float fCx, float fCy, float fOuterR, float fInnerR)
	{
		float fDx = fX - fCx;
		float fDy = fY - fCy;
		float fAngle = atan2f(fDy, fDx) + fPI * 0.5f;
		if (fAngle < 0.f) fAngle += 2.f * fPI;

		// Fold into one sector of 5
		float fSectorAngle = 2.f * fPI / 5.f;
		float fA = fmodf(fAngle, fSectorAngle) - fSectorAngle * 0.5f;

		float fDist = sqrtf(fDx * fDx + fDy * fDy);
		float fCos = cosf(fA);
		float fSin = fabsf(sinf(fA));

		// Line from outer tip to inner notch
		float fNotchAngle = fSectorAngle * 0.5f;

		float fTipX = fOuterR;
		float fTipY = 0.f;
		float fNotchX = fInnerR * cosf(fNotchAngle);
		float fNotchY = fInnerR * sinf(fNotchAngle);

		// Point in folded space
		float fPx = fDist * fCos;
		float fPy = fDist * fSin;

		// Edge from tip to notch
		float fEdgeDx = fNotchX - fTipX;
		float fEdgeDy = fNotchY - fTipY;
		float fEdgeLen2 = fEdgeDx * fEdgeDx + fEdgeDy * fEdgeDy;
		float fT = ((fPx - fTipX) * fEdgeDx + (fPy - fTipY) * fEdgeDy) / fEdgeLen2;
		fT = fmaxf(0.f, fminf(1.f, fT));

		float fClosestX = fTipX + fT * fEdgeDx;
		float fClosestY = fTipY + fT * fEdgeDy;

		float fSegDist = sqrtf((fPx - fClosestX) * (fPx - fClosestX) + (fPy - fClosestY) * (fPy - fClosestY));

		// Sign: negative if inside (left of edge vector)
		float fCross = fEdgeDx * (fPy - fTipY) - fEdgeDy * (fPx - fTipX);
		return (fCross < 0.f) ? -fSegDist : fSegDist;
	}

	// --- Combinators ---

	static inline float Union(float fA, float fB) { return fminf(fA, fB); }
	static inline float Subtract(float fA, float fB) { return fmaxf(fA, -fB); }
	static inline float Intersect(float fA, float fB) { return fmaxf(fA, fB); }

	// --- Rendering ---

	static inline float Fill(float fDist, float fAA)
	{
		return fmaxf(0.f, fminf(1.f, 0.5f - fDist / fAA));
	}

	static inline float Stroke(float fDist, float fWidth, float fAA)
	{
		return Fill(fabsf(fDist) - fWidth * 0.5f, fAA);
	}

	// --- Pixel Helpers ---

	static inline uint32_t PackRGBA(uint8_t uR, uint8_t uG, uint8_t uB, uint8_t uA)
	{
		return static_cast<uint32_t>(uR) |
			(static_cast<uint32_t>(uG) << 8) |
			(static_cast<uint32_t>(uB) << 16) |
			(static_cast<uint32_t>(uA) << 24);
	}

	static inline uint32_t PackRGBAf(float fR, float fG, float fB, float fA)
	{
		auto Clamp = [](float f) { return fmaxf(0.f, fminf(1.f, f)); };
		return PackRGBA(
			static_cast<uint8_t>(Clamp(fR) * 255.f + 0.5f),
			static_cast<uint8_t>(Clamp(fG) * 255.f + 0.5f),
			static_cast<uint8_t>(Clamp(fB) * 255.f + 0.5f),
			static_cast<uint8_t>(Clamp(fA) * 255.f + 0.5f));
	}

	// Alpha-blend foreground over background
	static inline uint32_t AlphaBlend(uint32_t uBg, float fR, float fG, float fB, float fA)
	{
		float fBgR = static_cast<float>(uBg & 0xFF) / 255.f;
		float fBgG = static_cast<float>((uBg >> 8) & 0xFF) / 255.f;
		float fBgB = static_cast<float>((uBg >> 16) & 0xFF) / 255.f;
		float fBgA = static_cast<float>((uBg >> 24) & 0xFF) / 255.f;

		float fOutA = fA + fBgA * (1.f - fA);
		if (fOutA < 0.001f) return 0;

		float fOutR = (fR * fA + fBgR * fBgA * (1.f - fA)) / fOutA;
		float fOutG = (fG * fA + fBgG * fBgA * (1.f - fA)) / fOutA;
		float fOutB = (fB * fA + fBgB * fBgA * (1.f - fA)) / fOutA;

		return PackRGBAf(fOutR, fOutG, fOutB, fOutA);
	}
}

// ============================================================================
// Texture Write Helper — writes in the format Zenith_TextureAsset::LoadFromFile reads
// ============================================================================

#ifdef ZENITH_TOOLS

namespace TilePuzzle_AssetGen
{
	static void WriteTexture(const uint32_t* puPixels, uint32_t uWidth, uint32_t uHeight, const char* szPath)
	{
		Zenith_DataStream xStream;
		xStream << static_cast<int32_t>(uWidth);
		xStream << static_cast<int32_t>(uHeight);
		xStream << static_cast<int32_t>(1);
		xStream << TEXTURE_FORMAT_RGBA8_UNORM;
		size_t ulDataSize = static_cast<size_t>(uWidth) * uHeight * sizeof(uint32_t);
		xStream << ulDataSize;
		xStream.WriteData(puPixels, ulDataSize);
		xStream.WriteToFile(szPath);
	}

	// ========================================================================
	// Icon Texture Generators (white-on-transparent unless noted)
	// ========================================================================

	static void GenerateIcon_StarFilled(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);
		float fOuterR = 0.42f;
		float fInnerR = 0.18f;

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				float fDist = Star5(fX, fY, 0.5f, 0.5f, fOuterR, fInnerR);
				float fAlpha = Fill(fDist, fAA);

				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_StarEmpty(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);
		float fOuterR = 0.42f;
		float fInnerR = 0.18f;
		float fStrokeWidth = 0.04f;

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				float fDist = Star5(fX, fY, 0.5f, 0.5f, fOuterR, fInnerR);
				float fAlpha = Stroke(fDist, fStrokeWidth, fAA);

				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_Coin(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Filled circle
				float fOuter = Circle(fX, fY, 0.5f, 0.5f, 0.40f);
				float fAlphaFill = Fill(fOuter, fAA);

				// Ring border
				float fRing = Stroke(Circle(fX, fY, 0.5f, 0.5f, 0.33f), 0.03f, fAA);

				float fAlpha = fmaxf(fAlphaFill, 0.f);
				// Darken the ring area slightly for embossed effect
				float fBright = 1.f - fRing * 0.3f;

				puPixels[uY * uSize + uX] = PackRGBAf(fBright, fBright, fBright, fAlpha);
			}
		}
	}

	static void GenerateIcon_Heart(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Heart: two circles at top + rotated square below
				float fCircleR = 0.19f;
				float fLeft = Circle(fX, fY, 0.35f, 0.38f, fCircleR);
				float fRight = Circle(fX, fY, 0.65f, 0.38f, fCircleR);

				// Rotated 45-degree square (diamond) below
				float fRx = (fX - 0.5f) * 0.7071f + (fY - 0.55f) * 0.7071f;
				float fRy = -(fX - 0.5f) * 0.7071f + (fY - 0.55f) * 0.7071f;
				float fSquare = fmaxf(fabsf(fRx), fabsf(fRy)) - 0.24f;

				float fDist = Union(Union(fLeft, fRight), fSquare);
				float fAlpha = Fill(fDist, fAA);

				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_Undo(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// 270-degree arc (counterclockwise from right)
				float fArcDist = Arc(fX, fY, 0.5f, 0.5f, 0.28f, 0.5f * fPI, 2.f * fPI, 0.06f);
				float fArcAlpha = Fill(fArcDist, fAA);

				// Arrowhead at the end of the arc (pointing left at bottom)
				float fArrowDist = Triangle(fX, fY,
					0.5f, 0.78f,       // tip (bottom center)
					0.35f, 0.62f,      // left
					0.5f, 0.62f);      // right
				float fArrowAlpha = Fill(fArrowDist, fAA);

				float fAlpha = fmaxf(fArcAlpha, fArrowAlpha);
				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_Skip(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Two right-pointing triangles
				float fT1 = Triangle(fX, fY,
					0.18f, 0.25f,   // top-left
					0.50f, 0.50f,   // right point
					0.18f, 0.75f);  // bottom-left

				float fT2 = Triangle(fX, fY,
					0.48f, 0.25f,
					0.80f, 0.50f,
					0.48f, 0.75f);

				float fDist = Union(fT1, fT2);
				float fAlpha = Fill(fDist, fAA);

				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_Lock(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Body: rounded rectangle
				float fBody = RoundedBox(fX, fY, 0.5f, 0.62f, 0.25f, 0.20f, 0.04f);

				// Shackle: thick arc at top
				float fShackle = Arc(fX, fY, 0.5f, 0.42f, 0.15f, fPI, 2.f * fPI, 0.05f);
				// Only keep upper half of shackle
				float fUpperHalf = fY - 0.42f;
				fShackle = Intersect(fShackle, -fUpperHalf);

				float fDist = Union(fBody, fShackle);
				float fAlpha = Fill(fDist, fAA);

				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_Menu(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Three horizontal bars
				float fBar1 = RoundedBox(fX, fY, 0.5f, 0.30f, 0.30f, 0.04f, 0.02f);
				float fBar2 = RoundedBox(fX, fY, 0.5f, 0.50f, 0.30f, 0.04f, 0.02f);
				float fBar3 = RoundedBox(fX, fY, 0.5f, 0.70f, 0.30f, 0.04f, 0.02f);

				float fDist = Union(Union(fBar1, fBar2), fBar3);
				float fAlpha = Fill(fDist, fAA);

				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_Back(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Left-pointing chevron: two line segments
				float fSeg1 = Segment(fX, fY, 0.60f, 0.25f, 0.30f, 0.50f);
				float fSeg2 = Segment(fX, fY, 0.30f, 0.50f, 0.60f, 0.75f);

				float fDist = fminf(fSeg1, fSeg2) - 0.04f; // thickness
				float fAlpha = Fill(fDist, fAA);

				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_SoundOn(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Speaker body (small rect)
				float fBody = Box(fX, fY, 0.28f, 0.50f, 0.08f, 0.10f);

				// Speaker cone (triangle)
				float fCone = Triangle(fX, fY,
					0.28f, 0.35f,    // top-left
					0.48f, 0.22f,    // top-right
					0.28f, 0.65f);   // bottom-left
				float fCone2 = Triangle(fX, fY,
					0.28f, 0.65f,
					0.48f, 0.22f,
					0.48f, 0.78f);
				float fSpeaker = Union(Union(fBody, fCone), fCone2);

				// Sound wave arcs
				float fWave1 = Arc(fX, fY, 0.42f, 0.50f, 0.16f,
					-0.6f * fPI, 0.6f * fPI, 0.035f);
				float fWave2 = Arc(fX, fY, 0.42f, 0.50f, 0.28f,
					-0.5f * fPI, 0.5f * fPI, 0.035f);

				float fAlpha = fmaxf(fmaxf(Fill(fSpeaker, fAA), Fill(fWave1, fAA)), Fill(fWave2, fAA));

				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_SoundOff(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Speaker body (same as sound on but without waves)
				float fBody = Box(fX, fY, 0.28f, 0.50f, 0.08f, 0.10f);
				float fCone = Triangle(fX, fY,
					0.28f, 0.35f, 0.48f, 0.22f, 0.28f, 0.65f);
				float fCone2 = Triangle(fX, fY,
					0.28f, 0.65f, 0.48f, 0.22f, 0.48f, 0.78f);
				float fSpeaker = Union(Union(fBody, fCone), fCone2);

				// Diagonal slash
				float fSlash = Segment(fX, fY, 0.58f, 0.25f, 0.70f, 0.75f) - 0.03f;

				float fAlpha = fmaxf(Fill(fSpeaker, fAA), Fill(fSlash, fAA));

				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_Reset(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// ~330-degree circular arc
				float fArcDist = Arc(fX, fY, 0.5f, 0.5f, 0.28f,
					0.25f * fPI, 2.0f * fPI, 0.055f);
				float fArcAlpha = Fill(fArcDist, fAA);

				// Arrowhead at arc start (pointing right at top-right area)
				float fArrowEndX = 0.5f + 0.28f * cosf(0.25f * fPI);
				float fArrowEndY = 0.5f + 0.28f * sinf(0.25f * fPI);
				float fArrowDist = Triangle(fX, fY,
					fArrowEndX + 0.12f, fArrowEndY - 0.02f,   // tip
					fArrowEndX - 0.02f, fArrowEndY - 0.10f,   // left
					fArrowEndX - 0.02f, fArrowEndY + 0.06f);  // right
				float fArrowAlpha = Fill(fArrowDist, fAA);

				float fAlpha = fmaxf(fArcAlpha, fArrowAlpha);
				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_Gear(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);
		uint32_t uTeeth = 8;
		float fOuterR = 0.40f;
		float fInnerR = 0.30f;
		float fHoleR = 0.14f;
		float fToothHalfWidth = 0.07f;

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Base circle
				float fDist = Circle(fX, fY, 0.5f, 0.5f, fInnerR);

				// Add teeth using rotated boxes
				for (uint32_t t = 0; t < uTeeth; ++t)
				{
					float fAngle = static_cast<float>(t) * 2.f * fPI / static_cast<float>(uTeeth);
					float fCos = cosf(fAngle);
					float fSin = sinf(fAngle);

					// Center of tooth
					float fMidR = (fInnerR + fOuterR) * 0.5f;
					float fTcx = 0.5f + fMidR * fCos;
					float fTcy = 0.5f + fMidR * fSin;

					// Rotated box for tooth
					float fLx = fX - fTcx;
					float fLy = fY - fTcy;
					// Rotate into tooth local space
					float fRx = fLx * fCos + fLy * fSin;
					float fRy = -fLx * fSin + fLy * fCos;

					float fToothHalfH = (fOuterR - fInnerR) * 0.5f + 0.02f;
					float fTooth = fmaxf(fabsf(fRx) - fToothHalfWidth, fabsf(fRy) - fToothHalfH);

					fDist = Union(fDist, fTooth);
				}

				// Subtract center hole
				float fHole = Circle(fX, fY, 0.5f, 0.5f, fHoleR);
				fDist = Subtract(fDist, fHole);

				float fAlpha = Fill(fDist, fAA);
				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	static void GenerateIcon_CatSilhouette(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Head circle
				float fHead = Circle(fX, fY, 0.5f, 0.55f, 0.28f);

				// Two triangle ears
				float fLeftEar = Triangle(fX, fY,
					0.25f, 0.45f,   // base left
					0.32f, 0.18f,   // tip
					0.42f, 0.38f);  // base right

				float fRightEar = Triangle(fX, fY,
					0.58f, 0.38f,
					0.68f, 0.18f,
					0.75f, 0.45f);

				float fDist = Union(Union(fHead, fLeftEar), fRightEar);
				float fAlpha = Fill(fDist, fAA);

				// Question mark (simple approximation) overlaid darker
				float fQArc = Arc(fX, fY, 0.5f, 0.48f, 0.08f,
					-0.2f * fPI, 1.2f * fPI, 0.035f);
				float fQStem = Segment(fX, fY, 0.5f, 0.56f, 0.5f, 0.63f) - 0.02f;
				float fQDot = Circle(fX, fY, 0.5f, 0.70f, 0.025f);
				float fQAlpha = fmaxf(fmaxf(Fill(fQArc, fAA), Fill(fQStem, fAA)), Fill(fQDot, fAA));

				// Cat body is gray, question mark is darker
				float fR = 0.6f;
				float fG = 0.6f;
				float fB = 0.6f;
				if (fQAlpha > 0.01f && fAlpha > 0.01f)
				{
					fR = fR * (1.f - fQAlpha) + 0.2f * fQAlpha;
					fG = fG * (1.f - fQAlpha) + 0.2f * fQAlpha;
					fB = fB * (1.f - fQAlpha) + 0.2f * fQAlpha;
				}

				puPixels[uY * uSize + uX] = PackRGBAf(fR, fG, fB, fAlpha);
			}
		}
	}

	static void GenerateIcon_Hint(uint32_t* puPixels, uint32_t uSize)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Bulb: circle at top
				float fBulb = Circle(fX, fY, 0.5f, 0.36f, 0.20f);

				// Base: trapezoid/rounded rect at bottom
				float fBase = RoundedBox(fX, fY, 0.5f, 0.66f, 0.10f, 0.06f, 0.02f);

				// Neck connecting bulb to base
				float fNeck = Box(fX, fY, 0.5f, 0.58f, 0.08f, 0.06f);

				// Screw threads: two thin horizontal lines
				float fThread1 = Box(fX, fY, 0.5f, 0.60f, 0.11f, 0.01f);
				float fThread2 = Box(fX, fY, 0.5f, 0.64f, 0.11f, 0.01f);

				// Rays: small lines radiating from bulb
				float fRay1 = Segment(fX, fY, 0.5f, 0.10f, 0.5f, 0.14f) - 0.015f;
				float fRay2 = Segment(fX, fY, 0.25f, 0.20f, 0.30f, 0.24f) - 0.015f;
				float fRay3 = Segment(fX, fY, 0.75f, 0.20f, 0.70f, 0.24f) - 0.015f;

				float fBody = Union(Union(fBulb, fBase), fNeck);
				float fDetail = Union(Union(fThread1, fThread2), Union(Union(fRay1, fRay2), fRay3));
				float fDist = Union(fBody, fDetail);
				float fAlpha = Fill(fDist, fAA);

				puPixels[uY * uSize + uX] = PackRGBAf(1.f, 1.f, 1.f, fAlpha);
			}
		}
	}

	// ========================================================================
	// Cat Face Texture Generator
	// ========================================================================

	static void GenerateCatFace(uint32_t* puPixels, uint32_t uSize,
		float fBaseR, float fBaseG, float fBaseB)
	{
		using namespace TilePuzzle_SDF;
		float fAA = 1.5f / static_cast<float>(uSize);

		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				float fX = (static_cast<float>(uX) + 0.5f) / static_cast<float>(uSize);
				float fY = (static_cast<float>(uY) + 0.5f) / static_cast<float>(uSize);

				// Start with base color, fully opaque
				float fR = fBaseR;
				float fG = fBaseG;
				float fB = fBaseB;
				float fA = 1.f;

				// Eyes: white ovals
				float fEyeL = Circle(fX, fY, 0.35f, 0.38f, 0.10f);
				float fEyeR = Circle(fX, fY, 0.65f, 0.38f, 0.10f);
				float fEyeAlpha = fmaxf(Fill(fEyeL, fAA), Fill(fEyeR, fAA));
				fR = fR * (1.f - fEyeAlpha) + 1.f * fEyeAlpha;
				fG = fG * (1.f - fEyeAlpha) + 1.f * fEyeAlpha;
				fB = fB * (1.f - fEyeAlpha) + 1.f * fEyeAlpha;

				// Pupils: black circles
				float fPupilL = Circle(fX, fY, 0.37f, 0.38f, 0.045f);
				float fPupilR = Circle(fX, fY, 0.67f, 0.38f, 0.045f);
				float fPupilAlpha = fmaxf(Fill(fPupilL, fAA), Fill(fPupilR, fAA));
				fR = fR * (1.f - fPupilAlpha) + 0.05f * fPupilAlpha;
				fG = fG * (1.f - fPupilAlpha) + 0.05f * fPupilAlpha;
				fB = fB * (1.f - fPupilAlpha) + 0.05f * fPupilAlpha;

				// Pupil highlights
				float fHlL = Circle(fX, fY, 0.355f, 0.365f, 0.015f);
				float fHlR = Circle(fX, fY, 0.655f, 0.365f, 0.015f);
				float fHlAlpha = fmaxf(Fill(fHlL, fAA), Fill(fHlR, fAA));
				fR = fR * (1.f - fHlAlpha) + 1.f * fHlAlpha;
				fG = fG * (1.f - fHlAlpha) + 1.f * fHlAlpha;
				fB = fB * (1.f - fHlAlpha) + 1.f * fHlAlpha;

				// Nose: small inverted triangle (pink)
				float fNose = Triangle(fX, fY,
					0.47f, 0.52f,    // top-left
					0.53f, 0.52f,    // top-right
					0.50f, 0.56f);   // bottom center
				float fNoseAlpha = Fill(fNose, fAA);
				fR = fR * (1.f - fNoseAlpha) + 0.85f * fNoseAlpha;
				fG = fG * (1.f - fNoseAlpha) + 0.45f * fNoseAlpha;
				fB = fB * (1.f - fNoseAlpha) + 0.50f * fNoseAlpha;

				// Whiskers: 6 thin lines (dark gray)
				float fWhiskerColor = 0.15f;
				float fW1 = Segment(fX, fY, 0.40f, 0.55f, 0.12f, 0.48f) - 0.006f;
				float fW2 = Segment(fX, fY, 0.40f, 0.56f, 0.12f, 0.56f) - 0.006f;
				float fW3 = Segment(fX, fY, 0.40f, 0.57f, 0.12f, 0.64f) - 0.006f;
				float fW4 = Segment(fX, fY, 0.60f, 0.55f, 0.88f, 0.48f) - 0.006f;
				float fW5 = Segment(fX, fY, 0.60f, 0.56f, 0.88f, 0.56f) - 0.006f;
				float fW6 = Segment(fX, fY, 0.60f, 0.57f, 0.88f, 0.64f) - 0.006f;

				float fWDist = fminf(fminf(fminf(fW1, fW2), fminf(fW3, fW4)), fminf(fW5, fW6));
				float fWAlpha = Fill(fWDist, fAA);
				fR = fR * (1.f - fWAlpha) + fWhiskerColor * fWAlpha;
				fG = fG * (1.f - fWAlpha) + fWhiskerColor * fWAlpha;
				fB = fB * (1.f - fWAlpha) + fWhiskerColor * fWAlpha;

				// Mouth: two small downward arcs
				float fMouthL = Arc(fX, fY, 0.46f, 0.58f, 0.04f,
					0.2f * fPI, 0.8f * fPI, 0.02f);
				float fMouthR = Arc(fX, fY, 0.54f, 0.58f, 0.04f,
					0.2f * fPI, 0.8f * fPI, 0.02f);
				float fMouthAlpha = fmaxf(Fill(fMouthL, fAA), Fill(fMouthR, fAA));
				fR = fR * (1.f - fMouthAlpha) + 0.2f * fMouthAlpha;
				fG = fG * (1.f - fMouthAlpha) + 0.15f * fMouthAlpha;
				fB = fB * (1.f - fMouthAlpha) + 0.15f * fMouthAlpha;

				puPixels[uY * uSize + uX] = PackRGBAf(fR, fG, fB, fA);
			}
		}
	}

	// ========================================================================
	// Gameplay Texture Generators
	// ========================================================================

	static void GenerateFloorTile(uint32_t* puPixels, uint32_t uSize)
	{
		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				// Base: light gray
				float fR = 0.78f, fG = 0.78f, fB = 0.82f;

				// Subtle grid lines every 32 pixels
				uint32_t uGridSpacing = uSize / 4;
				bool bOnGridX = (uX % uGridSpacing) < 2 || (uX % uGridSpacing) >= (uGridSpacing - 1);
				bool bOnGridY = (uY % uGridSpacing) < 2 || (uY % uGridSpacing) >= (uGridSpacing - 1);

				if (bOnGridX || bOnGridY)
				{
					fR -= 0.08f;
					fG -= 0.08f;
					fB -= 0.08f;
				}

				// Simple hash noise for subtle variation
				uint32_t uHash = (uX * 374761393u + uY * 668265263u) ^ 0x85ebca6bu;
				uHash = ((uHash >> 16) ^ uHash) * 0x45d9f3bu;
				float fNoise = (static_cast<float>(uHash & 0xFF) / 255.f - 0.5f) * 0.04f;
				fR += fNoise;
				fG += fNoise;
				fB += fNoise;

				puPixels[uY * uSize + uX] = TilePuzzle_SDF::PackRGBAf(fR, fG, fB, 1.f);
			}
		}
	}

	static void GenerateBlockerTexture(uint32_t* puPixels, uint32_t uSize)
	{
		for (uint32_t uY = 0; uY < uSize; ++uY)
		{
			for (uint32_t uX = 0; uX < uSize; ++uX)
			{
				// Base: dark brown (#503C1E)
				float fR = 80.f / 255.f;
				float fG = 60.f / 255.f;
				float fB = 30.f / 255.f;

				// Diagonal stripes (period ~16 pixels)
				uint32_t uPeriod = uSize / 8;
				uint32_t uDiag = (uX + uY) % uPeriod;
				bool bLightStripe = uDiag < (uPeriod / 2);

				if (bLightStripe)
				{
					fR += 0.06f;
					fG += 0.05f;
					fB += 0.03f;
				}

				puPixels[uY * uSize + uX] = TilePuzzle_SDF::PackRGBAf(fR, fG, fB, 1.f);
			}
		}
	}

	// ========================================================================
	// Orchestration: Generate All Textures
	// ========================================================================

	static void GenerateAllTextures()
	{
		Zenith_Log(LOG_CATEGORY_GENERAL, "Generating procedural textures...");

		// Create output directories
		std::filesystem::create_directories(GAME_ASSETS_DIR "Textures/Icons");
		std::filesystem::create_directories(GAME_ASSETS_DIR "Textures/CatFaces");
		std::filesystem::create_directories(GAME_ASSETS_DIR "Textures/Gameplay");

		// ----- Icons (48x48 or 64x64) -----
		{
			constexpr uint32_t uSmall = 48;
			constexpr uint32_t uLarge = 64;

			uint32_t auSmallBuf[uSmall * uSmall];
			uint32_t auLargeBuf[uLarge * uLarge];

			auto WriteSmall = [&](void (*pfnGen)(uint32_t*, uint32_t), const char* szName) {
				memset(auSmallBuf, 0, sizeof(auSmallBuf));
				pfnGen(auSmallBuf, uSmall);
				char szPath[ZENITH_MAX_PATH_LENGTH];
				snprintf(szPath, sizeof(szPath),
					GAME_ASSETS_DIR "Textures/Icons/%s" ZENITH_TEXTURE_EXT, szName);
				WriteTexture(auSmallBuf, uSmall, uSmall, szPath);
			};

			auto WriteLarge = [&](void (*pfnGen)(uint32_t*, uint32_t), const char* szName) {
				memset(auLargeBuf, 0, sizeof(auLargeBuf));
				pfnGen(auLargeBuf, uLarge);
				char szPath[ZENITH_MAX_PATH_LENGTH];
				snprintf(szPath, sizeof(szPath),
					GAME_ASSETS_DIR "Textures/Icons/%s" ZENITH_TEXTURE_EXT, szName);
				WriteTexture(auLargeBuf, uLarge, uLarge, szPath);
			};

			WriteLarge(GenerateIcon_StarFilled, "star_filled");
			WriteLarge(GenerateIcon_StarEmpty, "star_empty");
			WriteSmall(GenerateIcon_Coin, "coin");
			WriteSmall(GenerateIcon_Heart, "heart");
			WriteSmall(GenerateIcon_Undo, "undo");
			WriteSmall(GenerateIcon_Skip, "skip");
			WriteSmall(GenerateIcon_Lock, "lock");
			WriteSmall(GenerateIcon_Menu, "menu");
			WriteSmall(GenerateIcon_Back, "back");
			WriteSmall(GenerateIcon_SoundOn, "sound_on");
			WriteSmall(GenerateIcon_SoundOff, "sound_off");
			WriteSmall(GenerateIcon_Reset, "reset");
			WriteSmall(GenerateIcon_Gear, "gear");
			WriteLarge(GenerateIcon_CatSilhouette, "cat_silhouette");
			WriteSmall(GenerateIcon_Hint, "hint");

			Zenith_Log(LOG_CATEGORY_GENERAL, "  Wrote 15 icon textures to " GAME_ASSETS_DIR "Textures/Icons/");
		}

		// ----- Cat Face Textures (256x256) -----
		{
			constexpr uint32_t uCatSize = 256;
			uint32_t* puCatBuf = static_cast<uint32_t*>(
				Zenith_MemoryManagement::Allocate(uCatSize * uCatSize * sizeof(uint32_t)));

			// Color palette (same as materials.bin)
			float afColors[TILEPUZZLE_COLOR_COUNT][3] = {
				{ 230.f / 255.f,  60.f / 255.f,  60.f / 255.f },  // Red
				{  60.f / 255.f, 200.f / 255.f,  60.f / 255.f },  // Green
				{  60.f / 255.f, 100.f / 255.f, 230.f / 255.f },  // Blue
				{ 230.f / 255.f, 230.f / 255.f,  60.f / 255.f },  // Yellow
				{ 180.f / 255.f,  60.f / 255.f, 220.f / 255.f },  // Purple
			};

			for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
			{
				memset(puCatBuf, 0, uCatSize * uCatSize * sizeof(uint32_t));
				GenerateCatFace(puCatBuf, uCatSize,
					afColors[i][0], afColors[i][1], afColors[i][2]);

				char szPath[ZENITH_MAX_PATH_LENGTH];
				snprintf(szPath, sizeof(szPath),
					GAME_ASSETS_DIR "Textures/CatFaces/cat_face_%u" ZENITH_TEXTURE_EXT, i);
				WriteTexture(puCatBuf, uCatSize, uCatSize, szPath);
			}

			Zenith_MemoryManagement::Deallocate(puCatBuf);
			Zenith_Log(LOG_CATEGORY_GENERAL, "  Wrote %u cat face textures to " GAME_ASSETS_DIR "Textures/CatFaces/",
				TILEPUZZLE_COLOR_COUNT);
		}

		// ----- Gameplay Textures (128x128) -----
		{
			constexpr uint32_t uGameSize = 128;
			uint32_t auGameBuf[uGameSize * uGameSize];

			memset(auGameBuf, 0, sizeof(auGameBuf));
			GenerateFloorTile(auGameBuf, uGameSize);
			WriteTexture(auGameBuf, uGameSize, uGameSize,
				GAME_ASSETS_DIR "Textures/Gameplay/floor_tile" ZENITH_TEXTURE_EXT);

			memset(auGameBuf, 0, sizeof(auGameBuf));
			GenerateBlockerTexture(auGameBuf, uGameSize);
			WriteTexture(auGameBuf, uGameSize, uGameSize,
				GAME_ASSETS_DIR "Textures/Gameplay/blocker" ZENITH_TEXTURE_EXT);

			Zenith_Log(LOG_CATEGORY_GENERAL, "  Wrote gameplay textures to " GAME_ASSETS_DIR "Textures/Gameplay/");
		}

		Zenith_Log(LOG_CATEGORY_GENERAL, "Procedural texture generation complete.");
	}

	// ========================================================================
	// Material Generation (pinball .zmtrl files)
	// ========================================================================

	static void GeneratePinballMaterials()
	{
		Zenith_Log(LOG_CATEGORY_GENERAL, "Generating pinball materials...");

		auto& xRegistry = Zenith_AssetRegistry::Get();

		// Pinball ball: silver/chrome
		{
			Zenith_MaterialAsset* pxMat = xRegistry.Create<Zenith_MaterialAsset>();
			pxMat->SetName("PinballBall");
			pxMat->SetBaseColor(Zenith_Maths::Vector4(192.f / 255.f, 192.f / 255.f, 192.f / 255.f, 1.f));
			pxMat->SetMetallic(0.9f);
			pxMat->SetRoughness(0.1f);
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath),
				GAME_ASSETS_DIR "Materials/pinball_ball" ZENITH_MATERIAL_EXT);
			pxMat->SaveToFile(szPath);
		}

		// Pinball peg: cyan matte
		{
			Zenith_MaterialAsset* pxMat = xRegistry.Create<Zenith_MaterialAsset>();
			pxMat->SetName("PinballPeg");
			pxMat->SetBaseColor(Zenith_Maths::Vector4(60.f / 255.f, 200.f / 255.f, 230.f / 255.f, 1.f));
			pxMat->SetMetallic(0.0f);
			pxMat->SetRoughness(0.6f);
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath),
				GAME_ASSETS_DIR "Materials/pinball_peg" ZENITH_MATERIAL_EXT);
			pxMat->SaveToFile(szPath);
		}

		// Pinball peg hit: cyan with emissive flash
		{
			Zenith_MaterialAsset* pxMat = xRegistry.Create<Zenith_MaterialAsset>();
			pxMat->SetName("PinballPegHit");
			pxMat->SetBaseColor(Zenith_Maths::Vector4(60.f / 255.f, 200.f / 255.f, 230.f / 255.f, 1.f));
			pxMat->SetMetallic(0.0f);
			pxMat->SetRoughness(0.6f);
			pxMat->SetEmissiveColor(Zenith_Maths::Vector3(60.f / 255.f, 200.f / 255.f, 230.f / 255.f));
			pxMat->SetEmissiveIntensity(2.0f);
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath),
				GAME_ASSETS_DIR "Materials/pinball_peg_hit" ZENITH_MATERIAL_EXT);
			pxMat->SaveToFile(szPath);
		}

		Zenith_Log(LOG_CATEGORY_GENERAL, "  Wrote pinball materials to " GAME_ASSETS_DIR "Materials/");
	}

	// ========================================================================
	// Particle Config Generation (.zptcl files)
	// ========================================================================

	static void GenerateParticleConfigs()
	{
		Zenith_Log(LOG_CATEGORY_GENERAL, "Generating particle configs...");

		std::filesystem::create_directories(GAME_ASSETS_DIR "Particles");

		// Elimination particle effect (cat sparkle burst)
		{
			Flux_ParticleEmitterConfig xConfig;
			xConfig.m_fSpawnRate = 0.f;
			xConfig.m_uBurstCount = 25;
			xConfig.m_uMaxParticles = 30;
			xConfig.m_fLifetimeMin = 0.4f;
			xConfig.m_fLifetimeMax = 0.8f;
			xConfig.m_xEmitDirection = Zenith_Maths::Vector3(0.f, 1.f, 0.f);
			xConfig.m_fSpreadAngleDegrees = 180.f;
			xConfig.m_fSpeedMin = 2.f;
			xConfig.m_fSpeedMax = 5.f;
			xConfig.m_xGravity = Zenith_Maths::Vector3(0.f, -2.f, 0.f);
			xConfig.m_fDrag = 0.5f;
			xConfig.m_xColorStart = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);
			xConfig.m_xColorEnd = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 0.f);
			xConfig.m_fSizeStart = 0.04f;
			xConfig.m_fSizeEnd = 0.01f;
			xConfig.m_bAdditiveBlending = true;

			Zenith_DataStream xStream;
			xConfig.WriteToDataStream(xStream);
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath),
				GAME_ASSETS_DIR "Particles/elimination" ZENITH_PARTICLES_EXT);
			xStream.WriteToFile(szPath);
		}

		// Victory confetti effect
		{
			Flux_ParticleEmitterConfig xConfig;
			xConfig.m_fSpawnRate = 0.f;
			xConfig.m_uBurstCount = 80;
			xConfig.m_uMaxParticles = 100;
			xConfig.m_fLifetimeMin = 2.0f;
			xConfig.m_fLifetimeMax = 3.0f;
			xConfig.m_xEmitDirection = Zenith_Maths::Vector3(0.f, -1.f, 0.f);
			xConfig.m_fSpreadAngleDegrees = 60.f;
			xConfig.m_fSpeedMin = 1.f;
			xConfig.m_fSpeedMax = 3.f;
			xConfig.m_xGravity = Zenith_Maths::Vector3(0.f, 5.f, 0.f);
			xConfig.m_fDrag = 0.5f;
			xConfig.m_xColorStart = Zenith_Maths::Vector4(1.f, 0.8f, 0.f, 1.f);
			xConfig.m_xColorEnd = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 0.f);
			xConfig.m_fSizeStart = 0.06f;
			xConfig.m_fSizeEnd = 0.03f;
			xConfig.m_fRotationMin = 0.f;
			xConfig.m_fRotationMax = 6.28f;
			xConfig.m_fRotationSpeedMin = -3.f;
			xConfig.m_fRotationSpeedMax = 3.f;
			xConfig.m_fSpawnRadius = 3.0f;

			Zenith_DataStream xStream;
			xConfig.WriteToDataStream(xStream);
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath),
				GAME_ASSETS_DIR "Particles/victory_confetti" ZENITH_PARTICLES_EXT);
			xStream.WriteToFile(szPath);
		}

		Zenith_Log(LOG_CATEGORY_GENERAL, "  Wrote particle configs to " GAME_ASSETS_DIR "Particles/");
	}

	// ========================================================================
	// Runtime Loading: Particle Configs
	// ========================================================================

	static void LoadParticleConfigs()
	{
		// Elimination
		{
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath),
				GAME_ASSETS_DIR "Particles/elimination" ZENITH_PARTICLES_EXT);

			Zenith_DataStream xStream;
			xStream.ReadFromFile(szPath);
			if (xStream.IsValid())
			{
				TilePuzzle::g_pxEliminationParticleConfig = new Flux_ParticleEmitterConfig();
				TilePuzzle::g_pxEliminationParticleConfig->ReadFromDataStream(xStream);
				Flux_ParticleEmitterConfig::Register("Elimination",
					TilePuzzle::g_pxEliminationParticleConfig);
			}
		}

		// Victory confetti
		{
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath),
				GAME_ASSETS_DIR "Particles/victory_confetti" ZENITH_PARTICLES_EXT);

			Zenith_DataStream xStream;
			xStream.ReadFromFile(szPath);
			if (xStream.IsValid())
			{
				TilePuzzle::g_pxVictoryConfettiConfig = new Flux_ParticleEmitterConfig();
				TilePuzzle::g_pxVictoryConfettiConfig->ReadFromDataStream(xStream);
				Flux_ParticleEmitterConfig::Register("VictoryConfetti",
					TilePuzzle::g_pxVictoryConfettiConfig);
			}
		}
	}
}

#endif // ZENITH_TOOLS

// ============================================================================
// Runtime-only loading (non-tools builds also need LoadParticleConfigs)
// ============================================================================

#ifndef ZENITH_TOOLS

namespace TilePuzzle_AssetGen
{
	static void LoadParticleConfigs()
	{
		// Elimination
		{
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath),
				GAME_ASSETS_DIR "Particles/elimination" ZENITH_PARTICLES_EXT);

			Zenith_DataStream xStream;
			xStream.ReadFromFile(szPath);
			if (xStream.IsValid())
			{
				TilePuzzle::g_pxEliminationParticleConfig = new Flux_ParticleEmitterConfig();
				TilePuzzle::g_pxEliminationParticleConfig->ReadFromDataStream(xStream);
				Flux_ParticleEmitterConfig::Register("Elimination",
					TilePuzzle::g_pxEliminationParticleConfig);
			}
		}

		// Victory confetti
		{
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath),
				GAME_ASSETS_DIR "Particles/victory_confetti" ZENITH_PARTICLES_EXT);

			Zenith_DataStream xStream;
			xStream.ReadFromFile(szPath);
			if (xStream.IsValid())
			{
				TilePuzzle::g_pxVictoryConfettiConfig = new Flux_ParticleEmitterConfig();
				TilePuzzle::g_pxVictoryConfettiConfig->ReadFromDataStream(xStream);
				Flux_ParticleEmitterConfig::Register("VictoryConfetti",
					TilePuzzle::g_pxVictoryConfettiConfig);
			}
		}
	}
}

#endif // !ZENITH_TOOLS
