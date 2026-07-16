#include "Zenith.h"

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"

#include <cmath>
#include <cstring>

#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include <filesystem>
#include <string>
#endif

// ============================================================================
// Local constants + tiny helpers. Deterministic by construction: no clocks,
// pointers, or container-iteration order feeds any byte a generator emits.
// ============================================================================
namespace
{
	constexpr float fZM_GEN_PI     = 3.14159265358979f;
	constexpr float fZM_GEN_TWO_PI = 6.28318530717959f;

	// FNV-1a constants -- pinned by Hash_Fnv1aMatchesTerrainAnchor. Must equal
	// ZM_TerrainAuthoring.cpp's ZM_Fnv1a32 constants exactly.
	constexpr u_int uZM_FNV_OFFSET_BASIS = 2166136261u;
	constexpr u_int uZM_FNV_PRIME        = 16777619u;

	// Local restatement of Zenith_SkeletonAsset::MAX_BONES (100). Kept as a local
	// constant so the pure path does not include the engine asset header; the
	// bake bridge (which does include it) is the belt-and-braces cross-check.
	constexpr u_int uZM_GEN_ENGINE_MAX_BONES = 100u;

	inline float ZM_SignF(float f) { return (f >= 0.0f) ? 1.0f : -1.0f; }
	inline float ZM_ClampF(float f, float fMin, float fMax)
	{
		return (f < fMin) ? fMin : ((f > fMax) ? fMax : f);
	}

	// Push one vertex worth of the parallel per-vertex buffers (position/normal/
	// uv/colour/skin). Tangents are intentionally NOT pushed here -- they are
	// filled by ZM_GenGenerateTangents so the tested buffer IS the baked buffer.
	inline void ZM_PushLoftVertex(ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xPos,
		const Zenith_Maths::Vector3& xNormal, const Zenith_Maths::Vector2& xUV,
		const glm::uvec4& xBoneIdx, const glm::vec4& xBoneWeight)
	{
		xMesh.m_xPositions.PushBack(xPos);
		xMesh.m_xNormals.PushBack(xNormal);
		xMesh.m_xUVs.PushBack(xUV);
		xMesh.m_xColors.PushBack(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		xMesh.m_xBoneIndices.PushBack(xBoneIdx);
		xMesh.m_xBoneWeights.PushBack(xBoneWeight);
	}

	inline void ZM_PushTri(ZM_GenMesh& xMesh, u_int uA, u_int uB, u_int uC)
	{
		xMesh.m_xIndices.PushBack(uA);
		xMesh.m_xIndices.PushBack(uB);
		xMesh.m_xIndices.PushBack(uC);
	}
}

// ============================================================================
// Seed derivation.
// ============================================================================

u_int ZM_GenHashName(const char* szText)
{
	// Byte-identical to ZM_TerrainAuthoring.cpp:ZM_Fnv1a32 (the anchor test pins
	// the two). NUL string returns the offset basis.
	u_int uHash = uZM_FNV_OFFSET_BASIS;
	if (szText == nullptr)
	{
		return uHash;
	}
	for (const u_int8* pByte = reinterpret_cast<const u_int8*>(szText);
		*pByte != 0u; ++pByte)
	{
		uHash ^= *pByte;
		uHash *= uZM_FNV_PRIME;
	}
	return uHash;
}

u_int ZM_GenHashCombine(u_int uSeedA, u_int uSeedB)
{
	// Order-sensitive FNV-style mix. Frozen by Seed_DeriveIsStableGolden.
	u_int uHash = uSeedA ^ uZM_FNV_OFFSET_BASIS;
	uHash = (uHash * uZM_FNV_PRIME) ^ uSeedB;
	uHash *= uZM_FNV_PRIME;
	return uHash;
}

u_int64 ZM_GenDeriveSeed(u_int uFamilySeed, u_int uSpeciesId, u_int uEvoStage,
	ZM_GEN_DOMAIN eDomain)
{
	// Fold (familySeed, speciesId, evoStage, domain) in that FIXED order into a
	// 32-bit accumulator, then splat to 64 bits via a decorrelated golden-ratio
	// mix. GOLDEN-PINNED: a change here forces a version bump + cold re-bake.
	u_int uAcc = uFamilySeed;
	uAcc = ZM_GenHashCombine(uAcc, uSpeciesId);
	uAcc = ZM_GenHashCombine(uAcc, uEvoStage);
	uAcc = ZM_GenHashCombine(uAcc, static_cast<u_int>(eDomain));

	const u_int uLo = uAcc;
	const u_int uHi = ZM_GenHashCombine(uLo, 0x9E3779B9u);
	return (static_cast<u_int64>(uHi) << 32) | static_cast<u_int64>(uLo);
}

// ============================================================================
// ZM_GenNoise -- verbatim integer arithmetic port of Zenith_TerrainNoise.
// ============================================================================
namespace
{
	// Finalizer-style 32-bit integer hash (xxhash/murmur-like avalanche). Copied
	// exactly from Zenith_TerrainNoise::HashUInt.
	inline u_int ZM_NoiseHashUInt(u_int uX)
	{
		uX ^= uX >> 16;
		uX *= 0x7feb352du;
		uX ^= uX >> 15;
		uX *= 0x846ca68bu;
		uX ^= uX >> 16;
		return uX;
	}

	inline float ZM_NoiseValueAt(int iX, int iY, u_int uSeed)
	{
		return ZM_GenNoise::HashToFloat01(ZM_GenNoise::HashCoords(iX, iY, uSeed));
	}
}

namespace ZM_GenNoise
{
	u_int HashCoords(int iX, int iY, u_int uSeed)
	{
		return ZM_NoiseHashUInt(static_cast<u_int>(iX) * 0x9E3779B1u
			^ static_cast<u_int>(iY) * 0x85EBCA77u
			^ uSeed * 0xC2B2AE3Du);
	}

	float HashToFloat01(u_int uHash)
	{
		return static_cast<float>(uHash & 0x00FFFFFFu) * (1.0f / 16777216.0f);
	}

	float ValueNoise2D(float fX, float fY, u_int uSeed)
	{
		const int iX0 = static_cast<int>(floorf(fX));
		const int iY0 = static_cast<int>(floorf(fY));
		const float fTX = fX - static_cast<float>(iX0);
		const float fTY = fY - static_cast<float>(iY0);
		const float fSX = fTX * fTX * (3.0f - 2.0f * fTX);
		const float fSY = fTY * fTY * (3.0f - 2.0f * fTY);

		const float fV00 = ZM_NoiseValueAt(iX0, iY0, uSeed);
		const float fV10 = ZM_NoiseValueAt(iX0 + 1, iY0, uSeed);
		const float fV01 = ZM_NoiseValueAt(iX0, iY0 + 1, uSeed);
		const float fV11 = ZM_NoiseValueAt(iX0 + 1, iY0 + 1, uSeed);

		const float fTop = fV00 + (fV10 - fV00) * fSX;
		const float fBottom = fV01 + (fV11 - fV01) * fSX;
		return fTop + (fBottom - fTop) * fSY;
	}

	float FBM2D(float fX, float fY, u_int uSeed, u_int uOctaves, float fLacunarity, float fGain)
	{
		uOctaves = uOctaves < 1u ? 1u : (uOctaves > 12u ? 12u : uOctaves);
		float fAmplitude = 1.0f;
		float fFrequency = 1.0f;
		float fSum = 0.0f;
		float fTotal = 0.0f;
		for (u_int u = 0; u < uOctaves; u++)
		{
			fSum += ValueNoise2D(fX * fFrequency, fY * fFrequency, uSeed + u * 1013u) * fAmplitude;
			fTotal += fAmplitude;
			fAmplitude *= fGain;
			fFrequency *= fLacunarity;
		}
		return fTotal > 0.0f ? fSum / fTotal : 0.0f;
	}

	float RidgedFBM2D(float fX, float fY, u_int uSeed, u_int uOctaves, float fLacunarity, float fGain)
	{
		uOctaves = uOctaves < 1u ? 1u : (uOctaves > 12u ? 12u : uOctaves);
		float fAmplitude = 1.0f;
		float fFrequency = 1.0f;
		float fSum = 0.0f;
		float fTotal = 0.0f;
		for (u_int u = 0; u < uOctaves; u++)
		{
			const float fN = ValueNoise2D(fX * fFrequency, fY * fFrequency, uSeed + u * 2027u);
			const float fRidge = 1.0f - fabsf(fN * 2.0f - 1.0f);   // tent: peaks at n=0.5
			fSum += fRidge * fRidge * fAmplitude;
			fTotal += fAmplitude;
			fAmplitude *= fGain;
			fFrequency *= fLacunarity;
		}
		return fTotal > 0.0f ? fSum / fTotal : 0.0f;
	}
}

// ============================================================================
// ZM_GenMesh housekeeping.
// ============================================================================

void ZM_GenMesh::Reset()
{
	m_xPositions.Clear();
	m_xNormals.Clear();
	m_xUVs.Clear();
	m_xTangents.Clear();
	m_xColors.Clear();
	m_xIndices.Clear();
	m_xBoneIndices.Clear();
	m_xBoneWeights.Clear();
	m_xBones.Clear();
}

int ZM_GenMeshFindBone(const ZM_GenMesh& xMesh, const char* szName)
{
	if (szName == nullptr)
	{
		return -1;
	}
	for (u_int u = 0; u < xMesh.m_xBones.GetSize(); u++)
	{
		if (strcmp(xMesh.m_xBones.Get(u).m_szName, szName) == 0)
		{
			return static_cast<int>(u);
		}
	}
	return -1;
}

Zenith_Maths::Vector3 ZM_GenMeshBoundsMin(const ZM_GenMesh& xMesh)
{
	if (xMesh.m_xPositions.GetSize() == 0)
	{
		return Zenith_Maths::Vector3(0);
	}
	Zenith_Maths::Vector3 xMin = xMesh.m_xPositions.Get(0);
	for (u_int u = 1; u < xMesh.m_xPositions.GetSize(); u++)
	{
		xMin = glm::min(xMin, xMesh.m_xPositions.Get(u));
	}
	return xMin;
}

Zenith_Maths::Vector3 ZM_GenMeshBoundsMax(const ZM_GenMesh& xMesh)
{
	if (xMesh.m_xPositions.GetSize() == 0)
	{
		return Zenith_Maths::Vector3(0);
	}
	Zenith_Maths::Vector3 xMax = xMesh.m_xPositions.Get(0);
	for (u_int u = 1; u < xMesh.m_xPositions.GetSize(); u++)
	{
		xMax = glm::max(xMax, xMesh.m_xPositions.Get(u));
	}
	return xMax;
}

// ============================================================================
// Loft primitives.
// ============================================================================
namespace ZM_MeshLoft
{
	u_int EmitRing(ZM_GenMesh& xMesh, const ZM_LoftRing& xRing, u_int uSegs,
		const ZM_GenUVIsland& xIsland, float fVNorm)
	{
		// A ring divides by uSegs for both the angle and the U coordinate below; 0
		// would feed inf/NaN into the position/UV byte buffers. Match the header's
		// documented (>=3) precondition (Part::m_uSegs).
		Zenith_Assert(uSegs >= 3u, "loft ring needs at least 3 segments (uSegs=%u)", uSegs);
		const u_int uFirst = xMesh.GetNumVerts();
		const float fBlendB = ZM_ClampF(xRing.m_fBlendB, 0.0f, 1.0f);
		const glm::uvec4 xBoneIdx(xRing.m_uBoneA, xRing.m_uBoneB, 0u, 0u);
		const glm::vec4 xBoneWeight(1.0f - fBlendB, fBlendB, 0.0f, 0.0f);

		for (u_int uSeg = 0; uSeg <= uSegs; uSeg++)
		{
			const float fAng = fZM_GEN_TWO_PI * static_cast<float>(uSeg) / static_cast<float>(uSegs);
			float fSin = sinf(fAng);
			float fCos = cosf(fAng);
			if (xRing.m_fSuperEllipse != 1.0f)
			{
				// Exponent < 1 pushes the profile toward a rounded box (matches the
				// StickFigure shoe superellipse). Sign-preserving power.
				fSin = ZM_SignF(fSin) * powf(fabsf(fSin), xRing.m_fSuperEllipse);
				fCos = ZM_SignF(fCos) * powf(fabsf(fCos), xRing.m_fSuperEllipse);
			}

			// ang=0 -> -Z (back seam), ang=pi -> +Z (front). Copied from the port.
			const Zenith_Maths::Vector3 xPos(
				xRing.m_fCx + xRing.m_fRx * fSin,
				xRing.m_fY,
				xRing.m_fCz - xRing.m_fRz * fCos);

			// Analytic outward radial normal. Guard zero-length with a fixed
			// deterministic fallback so no NaN reaches the byte buffer.
			Zenith_Maths::Vector3 xNormal(fSin, 0.0f, -fCos);
			const float fLen = glm::length(xNormal);
			xNormal = (fLen > 1.0e-8f) ? (xNormal / fLen) : Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);

			const Zenith_Maths::Vector2 xUV(
				xIsland.U(static_cast<float>(uSeg) / static_cast<float>(uSegs)),
				xIsland.V(fVNorm));

			ZM_PushLoftVertex(xMesh, xPos, xNormal, xUV, xBoneIdx, xBoneWeight);
		}
		return uFirst;
	}

	void StitchRings(ZM_GenMesh& xMesh, u_int uRingAFirstVert, u_int uRingBFirstVert,
		u_int uSegs, bool bFlip)
	{
		// ringA above ringB. Non-flip order makes each tri's cross(C-A,B-A) point
		// OUTWARD (the load-bearing winding; Loft_WindingOutward locks it).
		for (u_int uSeg = 0; uSeg < uSegs; uSeg++)
		{
			const u_int uA = uRingAFirstVert + uSeg;
			const u_int uA1 = uA + 1u;
			const u_int uB = uRingBFirstVert + uSeg;
			const u_int uB1 = uB + 1u;
			if (!bFlip)
			{
				ZM_PushTri(xMesh, uA, uB, uA1);
				ZM_PushTri(xMesh, uA1, uB, uB1);
			}
			else
			{
				ZM_PushTri(xMesh, uA, uA1, uB);
				ZM_PushTri(xMesh, uA1, uB1, uB);
			}
		}
	}

	u_int CapRing(ZM_GenMesh& xMesh, u_int uRingFirstVert, u_int uSegs,
		const Zenith_Maths::Vector3& xCentre, const ZM_GenUVIsland& xIsland,
		float fVNorm, bool bUpward)
	{
		const u_int uCentre = xMesh.GetNumVerts();
		const Zenith_Maths::Vector3 xNormal = bUpward
			? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
			: Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
		const Zenith_Maths::Vector2 xUV(xIsland.U(0.5f), xIsland.V(fVNorm));

		// Inherit the fan centre's skin from the ring's first vertex (fixed,
		// deterministic; keeps the cap bound to the ring's bone(s)).
		const glm::uvec4 xBoneIdx = xMesh.m_xBoneIndices.Get(uRingFirstVert);
		const glm::vec4 xBoneWeight = xMesh.m_xBoneWeights.Get(uRingFirstVert);
		ZM_PushLoftVertex(xMesh, xCentre, xNormal, xUV, xBoneIdx, xBoneWeight);

		for (u_int uSeg = 0; uSeg < uSegs; uSeg++)
		{
			if (bUpward)
			{
				ZM_PushTri(xMesh, uRingFirstVert + uSeg, uRingFirstVert + uSeg + 1u, uCentre);
			}
			else
			{
				ZM_PushTri(xMesh, uRingFirstVert + uSeg, uCentre, uRingFirstVert + uSeg + 1u);
			}
		}
		return uCentre;
	}
}

// ---- AppendPart, split into static sub-builders (BuildGraph split precedent) --
namespace
{
	// Uniform Catmull-Rom through p1..p2 (neighbours p0, p3). fT==0 returns p1
	// EXACTLY, so authored rings survive subdivision byte-for-byte.
	inline float ZM_CatmullRom(float fP0, float fP1, float fP2, float fP3, float fT)
	{
		const float fT2 = fT * fT;
		const float fT3 = fT2 * fT;
		return 0.5f * ((2.0f * fP1)
			+ (-fP0 + fP2) * fT
			+ (2.0f * fP0 - 5.0f * fP1 + 4.0f * fP2 - fP3) * fT2
			+ (-fP0 + 3.0f * fP1 - 3.0f * fP2 + fP3) * fT3);
	}

	// Lerp two authored rings' skin at parameter fT: accumulate per-bone weight,
	// keep the 2 heaviest, renormalise. At fT==0 reproduces ring A exactly.
	void ZM_LerpRingSkin(const ZM_LoftRing& xA, const ZM_LoftRing& xB, float fT,
		u_int& uOutBoneA, u_int& uOutBoneB, float& fOutBlendB)
	{
		u_int auBone[4];
		float afWeight[4];
		int iCount = 0;

		auto Accumulate = [&](u_int uBone, float fW)
		{
			if (fW <= 0.0f) { return; }
			for (int i = 0; i < iCount; i++)
			{
				if (auBone[i] == uBone) { afWeight[i] += fW; return; }
			}
			auBone[iCount] = uBone;
			afWeight[iCount] = fW;
			iCount++;
		};

		Accumulate(xA.m_uBoneA, (1.0f - xA.m_fBlendB) * (1.0f - fT));
		Accumulate(xA.m_uBoneB, xA.m_fBlendB * (1.0f - fT));
		Accumulate(xB.m_uBoneA, (1.0f - xB.m_fBlendB) * fT);
		Accumulate(xB.m_uBoneB, xB.m_fBlendB * fT);

		if (iCount == 0)
		{
			uOutBoneA = uOutBoneB = xA.m_uBoneA;
			fOutBlendB = 0.0f;
			return;
		}

		int iTop0 = 0;
		for (int i = 1; i < iCount; i++) { if (afWeight[i] > afWeight[iTop0]) { iTop0 = i; } }
		int iTop1 = -1;
		for (int i = 0; i < iCount; i++)
		{
			if (i == iTop0) { continue; }
			if (iTop1 < 0 || afWeight[i] > afWeight[iTop1]) { iTop1 = i; }
		}

		const float fW0 = afWeight[iTop0];
		const float fW1 = (iTop1 >= 0) ? afWeight[iTop1] : 0.0f;
		const float fSum = fW0 + fW1;
		uOutBoneA = auBone[iTop0];
		uOutBoneB = (iTop1 >= 0) ? auBone[iTop1] : auBone[iTop0];
		fOutBlendB = (fSum > 1.0e-6f) ? (fW1 / fSum) : 0.0f;
	}

	// Densify authored rings: for each gap emit the authored ring plus (uSub-1)
	// Catmull-Rom interpolated rings, then the final authored ring. uSub<=1 (or
	// <2 rings) copies verbatim. Result count = (N-1)*uSub + 1.
	void ZM_SubdivideRings(const ZM_LoftRing* pxRings, u_int uNumRings, u_int uSub,
		Zenith_Vector<ZM_LoftRing>& xOut)
	{
		xOut.Clear();
		if (uNumRings == 0u)
		{
			return;
		}
		if (uSub <= 1u || uNumRings < 2u)
		{
			for (u_int u = 0; u < uNumRings; u++) { xOut.PushBack(pxRings[u]); }
			return;
		}

		auto At = [&](int i) -> const ZM_LoftRing&
		{
			const int iMax = static_cast<int>(uNumRings) - 1;
			const int iClamped = (i < 0) ? 0 : ((i > iMax) ? iMax : i);
			return pxRings[iClamped];
		};

		for (u_int u = 0; u + 1u < uNumRings; u++)
		{
			const ZM_LoftRing& xP0 = At(static_cast<int>(u) - 1);
			const ZM_LoftRing& xP1 = pxRings[u];
			const ZM_LoftRing& xP2 = pxRings[u + 1u];
			const ZM_LoftRing& xP3 = At(static_cast<int>(u) + 2);
			for (u_int s = 0; s < uSub; s++)
			{
				const float fT = static_cast<float>(s) / static_cast<float>(uSub);
				ZM_LoftRing xR;
				xR.m_fY  = ZM_CatmullRom(xP0.m_fY,  xP1.m_fY,  xP2.m_fY,  xP3.m_fY,  fT);
				xR.m_fCx = ZM_CatmullRom(xP0.m_fCx, xP1.m_fCx, xP2.m_fCx, xP3.m_fCx, fT);
				xR.m_fCz = ZM_CatmullRom(xP0.m_fCz, xP1.m_fCz, xP2.m_fCz, xP3.m_fCz, fT);
				xR.m_fRx = ZM_CatmullRom(xP0.m_fRx, xP1.m_fRx, xP2.m_fRx, xP3.m_fRx, fT);
				xR.m_fRz = ZM_CatmullRom(xP0.m_fRz, xP1.m_fRz, xP2.m_fRz, xP3.m_fRz, fT);
				if (xR.m_fRx < 0.0f) { xR.m_fRx = 0.0f; }
				if (xR.m_fRz < 0.0f) { xR.m_fRz = 0.0f; }
				xR.m_fSuperEllipse = ZM_CatmullRom(xP0.m_fSuperEllipse, xP1.m_fSuperEllipse,
					xP2.m_fSuperEllipse, xP3.m_fSuperEllipse, fT);
				ZM_LerpRingSkin(xP1, xP2, fT, xR.m_uBoneA, xR.m_uBoneB, xR.m_fBlendB);
				xOut.PushBack(xR);
			}
		}
		xOut.PushBack(pxRings[uNumRings - 1u]);
	}

	// Emit + stitch the dense ring list, spreading V by cumulative sweep (|Y|)
	// distance, then optionally cap each end. Returns the first vertex index.
	u_int ZM_EmitAndStitch(ZM_GenMesh& xMesh, const Zenith_Vector<ZM_LoftRing>& xDense,
		u_int uSegs, const ZM_GenUVIsland& xIsland, bool bCapStart, bool bCapEnd)
	{
		const u_int uDense = xDense.GetSize();
		if (uDense == 0u)
		{
			return xMesh.GetNumVerts();
		}

		float fTotal = 0.0f;
		for (u_int u = 1; u < uDense; u++)
		{
			fTotal += fabsf(xDense.Get(u).m_fY - xDense.Get(u - 1u).m_fY);
		}
		if (fTotal < 0.0001f) { fTotal = 0.0001f; }

		u_int uFirst = 0u;
		u_int uPrev = 0u;
		float fAccum = 0.0f;
		for (u_int u = 0; u < uDense; u++)
		{
			if (u > 0u)
			{
				fAccum += fabsf(xDense.Get(u).m_fY - xDense.Get(u - 1u).m_fY);
			}
			const u_int uRing = ZM_MeshLoft::EmitRing(xMesh, xDense.Get(u), uSegs, xIsland, fAccum / fTotal);
			if (u == 0u)
			{
				uFirst = uRing;
			}
			else
			{
				// Flip the wall winding when the sweep ASCENDS in Y (current ring
				// above previous). StitchRings' non-flip order is outward only when
				// ringA is above ringB, so an ascending ring chain must flip to keep
				// each tri's cross(C-A,B-A) OUTWARD -- matching the Y-based cap
				// orientation below (Loft_WindingOutward locks this).
				//
				// PRECONDITION: this Y-oriented rule assumes consecutive ring Y
				// values are DISTINCT. Two rings at exactly equal Y (dY == 0) form a
				// degenerate flat washer whose winding is ill-defined for this
				// vertical-sweep loft (a horizontal cross-section is out of the
				// intended envelope). Left as a documented precondition, NOT a hard
				// assert, so a future non-monotonic creature builder is not constrained.
				const bool bFlip = xDense.Get(u).m_fY > xDense.Get(u - 1u).m_fY;
				ZM_MeshLoft::StitchRings(xMesh, uPrev, uRing, uSegs, bFlip);
			}
			uPrev = uRing;
		}

		if (bCapStart)
		{
			const ZM_LoftRing& xR0 = xDense.Get(0);
			const bool bUpward = xR0.m_fY >= xDense.Get(uDense - 1u).m_fY;
			const float fExt = 0.5f * fmaxf(xR0.m_fRx, xR0.m_fRz);
			const Zenith_Maths::Vector3 xC(xR0.m_fCx, xR0.m_fY + (bUpward ? fExt : -fExt), xR0.m_fCz);
			ZM_MeshLoft::CapRing(xMesh, uFirst, uSegs, xC, xIsland, 0.0f, bUpward);
		}
		if (bCapEnd)
		{
			const ZM_LoftRing& xRL = xDense.Get(uDense - 1u);
			const bool bUpward = xRL.m_fY > xDense.Get(0).m_fY;
			const float fExt = 0.5f * fmaxf(xRL.m_fRx, xRL.m_fRz);
			const Zenith_Maths::Vector3 xC(xRL.m_fCx, xRL.m_fY + (bUpward ? fExt : -fExt), xRL.m_fCz);
			ZM_MeshLoft::CapRing(xMesh, uPrev, uSegs, xC, xIsland, 1.0f, bUpward);
		}
		return uFirst;
	}
}

namespace ZM_MeshLoft
{
	u_int AppendPart(ZM_GenMesh& xMesh, const Part& xPart)
	{
		Zenith_Vector<ZM_LoftRing> xDense;
		ZM_SubdivideRings(xPart.m_pxRings, xPart.m_uNumRings, xPart.m_uSubdiv, xDense);
		if (xDense.GetSize() == 0u)
		{
			return xMesh.GetNumVerts();
		}
		return ZM_EmitAndStitch(xMesh, xDense, xPart.m_uSegs, xPart.m_xIsland,
			xPart.m_bCapStart, xPart.m_bCapEnd);
	}
}

// ============================================================================
// Static (bone-free) primitive kit -- buildings/props.
// ============================================================================
namespace
{
	// Append one quad face: 4 verts (hard, per-face outward normal), 2 tris. The
	// winding (0,2,1)+(1,2,3) over corners laid out {BL, BR, TL, TR} makes each
	// triangle's cross(C-A,B-A) point along xNormal (outward) -- the repo front-face
	// rule, matching Zenith_MeshAsset::GenerateUnitCube. Pushes position/normal/uv/
	// colour ONLY -- NO bone buffers (the static contract; contrast ZM_PushLoftVertex).
	void ZM_PushStaticFace(ZM_GenMesh& xMesh,
		const Zenith_Maths::Vector3& xP0, const Zenith_Maths::Vector3& xP1,
		const Zenith_Maths::Vector3& xP2, const Zenith_Maths::Vector3& xP3,
		const Zenith_Maths::Vector3& xNormal, const ZM_GenUVIsland& xIsland)
	{
		const u_int uBase = xMesh.GetNumVerts();

		xMesh.m_xPositions.PushBack(xP0);
		xMesh.m_xPositions.PushBack(xP1);
		xMesh.m_xPositions.PushBack(xP2);
		xMesh.m_xPositions.PushBack(xP3);

		for (u_int u = 0; u < 4u; u++) { xMesh.m_xNormals.PushBack(xNormal); }
		for (u_int u = 0; u < 4u; u++)
		{
			xMesh.m_xColors.PushBack(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		}

		// Corner UVs into the island sub-rect: p0=BL, p1=BR, p2=TL, p3=TR.
		xMesh.m_xUVs.PushBack(Zenith_Maths::Vector2(xIsland.U(0.0f), xIsland.V(0.0f)));
		xMesh.m_xUVs.PushBack(Zenith_Maths::Vector2(xIsland.U(1.0f), xIsland.V(0.0f)));
		xMesh.m_xUVs.PushBack(Zenith_Maths::Vector2(xIsland.U(0.0f), xIsland.V(1.0f)));
		xMesh.m_xUVs.PushBack(Zenith_Maths::Vector2(xIsland.U(1.0f), xIsland.V(1.0f)));

		ZM_PushTri(xMesh, uBase + 0u, uBase + 2u, uBase + 1u);
		ZM_PushTri(xMesh, uBase + 1u, uBase + 2u, uBase + 3u);
	}

	// Append one triangular face: 3 verts (hard per-face normal), 1 tri. Normal derived
	// from winding so cross(C-A,B-A).normal>0 by construction; emitted with the winding
	// that makes the stored normal face AWAY from xInside (a point inside the solid) ->
	// always OUTWARD. UVs: v0->BL, v1->BR, v2->top-centre of the island. NO bone buffers
	// (the static contract; contrast ZM_PushLoftVertex).
	void ZM_PushStaticTri(ZM_GenMesh& xMesh,
		const Zenith_Maths::Vector3& xV0, const Zenith_Maths::Vector3& xV1, const Zenith_Maths::Vector3& xV2,
		const Zenith_Maths::Vector3& xInside, const ZM_GenUVIsland& xIsland)
	{
		const u_int uBase = xMesh.GetNumVerts();

		Zenith_Maths::Vector3 xN = glm::cross(xV2 - xV0, xV1 - xV0);   // A=v0,B=v1,C=v2 -> cross(C-A,B-A)
		const float fLen = glm::length(xN);
		xN = (fLen > 1.0e-8f) ? (xN / fLen) : Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);

		const Zenith_Maths::Vector3 xCentroid = (xV0 + xV1 + xV2) * (1.0f / 3.0f);
		const bool bOutward = glm::dot(xN, xCentroid - xInside) >= 0.0f;
		const Zenith_Maths::Vector3 xStoreN = bOutward ? xN : -xN;

		xMesh.m_xPositions.PushBack(xV0);
		xMesh.m_xPositions.PushBack(xV1);
		xMesh.m_xPositions.PushBack(xV2);
		for (u_int u = 0; u < 3u; u++) { xMesh.m_xNormals.PushBack(xStoreN); }
		for (u_int u = 0; u < 3u; u++) { xMesh.m_xColors.PushBack(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f)); }
		xMesh.m_xUVs.PushBack(Zenith_Maths::Vector2(xIsland.U(0.0f), xIsland.V(0.0f)));
		xMesh.m_xUVs.PushBack(Zenith_Maths::Vector2(xIsland.U(1.0f), xIsland.V(0.0f)));
		xMesh.m_xUVs.PushBack(Zenith_Maths::Vector2(xIsland.U(0.5f), xIsland.V(1.0f)));

		if (bOutward) { ZM_PushTri(xMesh, uBase + 0u, uBase + 1u, uBase + 2u); }
		else          { ZM_PushTri(xMesh, uBase + 0u, uBase + 2u, uBase + 1u); }
	}
}

namespace ZM_StaticMesh
{
	u_int AppendBox(ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xMin,
		const Zenith_Maths::Vector3& xMax, const ZM_GenUVIsland& xIsland)
	{
		const u_int uFirst = xMesh.GetNumVerts();
		const float x0 = xMin.x, y0 = xMin.y, z0 = xMin.z;
		const float x1 = xMax.x, y1 = xMax.y, z1 = xMax.z;

		// Six faces, corner layout + winding copied from Zenith_MeshAsset::Generate-
		// UnitCube (its (0,2,1)+(1,2,3) order is outward under cross(C-A,B-A)).
		// +Z (front)
		ZM_PushStaticFace(xMesh,
			Zenith_Maths::Vector3(x0, y0, z1), Zenith_Maths::Vector3(x1, y0, z1),
			Zenith_Maths::Vector3(x0, y1, z1), Zenith_Maths::Vector3(x1, y1, z1),
			Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), xIsland);
		// -Z (back)
		ZM_PushStaticFace(xMesh,
			Zenith_Maths::Vector3(x1, y0, z0), Zenith_Maths::Vector3(x0, y0, z0),
			Zenith_Maths::Vector3(x1, y1, z0), Zenith_Maths::Vector3(x0, y1, z0),
			Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f), xIsland);
		// +Y (top)
		ZM_PushStaticFace(xMesh,
			Zenith_Maths::Vector3(x0, y1, z1), Zenith_Maths::Vector3(x1, y1, z1),
			Zenith_Maths::Vector3(x0, y1, z0), Zenith_Maths::Vector3(x1, y1, z0),
			Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xIsland);
		// -Y (bottom)
		ZM_PushStaticFace(xMesh,
			Zenith_Maths::Vector3(x0, y0, z0), Zenith_Maths::Vector3(x1, y0, z0),
			Zenith_Maths::Vector3(x0, y0, z1), Zenith_Maths::Vector3(x1, y0, z1),
			Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), xIsland);
		// +X (right)
		ZM_PushStaticFace(xMesh,
			Zenith_Maths::Vector3(x1, y0, z1), Zenith_Maths::Vector3(x1, y0, z0),
			Zenith_Maths::Vector3(x1, y1, z1), Zenith_Maths::Vector3(x1, y1, z0),
			Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), xIsland);
		// -X (left)
		ZM_PushStaticFace(xMesh,
			Zenith_Maths::Vector3(x0, y0, z0), Zenith_Maths::Vector3(x0, y0, z1),
			Zenith_Maths::Vector3(x0, y1, z0), Zenith_Maths::Vector3(x0, y1, z1),
			Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f), xIsland);

		return uFirst;
	}

	u_int AppendGableRoof(ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xEaveMin,
		const Zenith_Maths::Vector3& xEaveMax, float fRise, const ZM_GenUVIsland& xIsland)
	{
		const u_int uFirst = xMesh.GetNumVerts();
		const float x0 = xEaveMin.x, x1 = xEaveMax.x, z0 = xEaveMin.z, z1 = xEaveMax.z;
		const float yTop = xEaveMin.y, yApex = yTop + fRise, zMid = 0.5f * (z0 + z1);
		const Zenith_Maths::Vector3 xInside(0.0f, yTop, 0.0f);

		// +Z pitch: eave edge z=z1 up to ridge z=zMid. Corners BL,BR,TL,TR; normal from winding.
		{
			const Zenith_Maths::Vector3 BL(x0, yTop, z1), BR(x1, yTop, z1), TL(x0, yApex, zMid), TR(x1, yApex, zMid);
			Zenith_Maths::Vector3 n = glm::cross(BR - BL, TL - BL);
			const float fL = glm::length(n); n = (fL > 1.0e-8f) ? (n / fL) : Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
			ZM_PushStaticFace(xMesh, BL, BR, TL, TR, n, xIsland);
		}
		// -Z pitch: eave edge z=z0 (X reversed to stay outward).
		{
			const Zenith_Maths::Vector3 BL(x1, yTop, z0), BR(x0, yTop, z0), TL(x1, yApex, zMid), TR(x0, yApex, zMid);
			Zenith_Maths::Vector3 n = glm::cross(BR - BL, TL - BL);
			const float fL = glm::length(n); n = (fL > 1.0e-8f) ? (n / fL) : Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
			ZM_PushStaticFace(xMesh, BL, BR, TL, TR, n, xIsland);
		}
		// +X gable end, -X gable end (auto-oriented outward by ZM_PushStaticTri).
		ZM_PushStaticTri(xMesh, Zenith_Maths::Vector3(x1, yTop, z1), Zenith_Maths::Vector3(x1, yApex, zMid), Zenith_Maths::Vector3(x1, yTop, z0), xInside, xIsland);
		ZM_PushStaticTri(xMesh, Zenith_Maths::Vector3(x0, yTop, z0), Zenith_Maths::Vector3(x0, yApex, zMid), Zenith_Maths::Vector3(x0, yTop, z1), xInside, xIsland);
		return uFirst;
	}

	u_int AppendHipRoof(ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xEaveMin,
		const Zenith_Maths::Vector3& xEaveMax, float fRise, const ZM_GenUVIsland& xIsland)
	{
		const u_int uFirst = xMesh.GetNumVerts();
		const float x0 = xEaveMin.x, x1 = xEaveMax.x, z0 = xEaveMin.z, z1 = xEaveMax.z, yTop = xEaveMin.y;
		const Zenith_Maths::Vector3 apex(0.5f * (x0 + x1), yTop + fRise, 0.5f * (z0 + z1));
		const Zenith_Maths::Vector3 inside(0.5f * (x0 + x1), yTop, 0.5f * (z0 + z1));
		const Zenith_Maths::Vector3 c0(x0, yTop, z0), c1(x1, yTop, z0), c2(x1, yTop, z1), c3(x0, yTop, z1);
		ZM_PushStaticTri(xMesh, c0, c1, apex, inside, xIsland);
		ZM_PushStaticTri(xMesh, c1, c2, apex, inside, xIsland);
		ZM_PushStaticTri(xMesh, c2, c3, apex, inside, xIsland);
		ZM_PushStaticTri(xMesh, c3, c0, apex, inside, xIsland);
		return uFirst;
	}

	u_int AppendFlatRoof(ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xEaveMin,
		const Zenith_Maths::Vector3& xEaveMax, float fParapet, const ZM_GenUVIsland& xIsland)
	{
		return AppendBox(xMesh,
			Zenith_Maths::Vector3(xEaveMin.x, xEaveMin.y,            xEaveMin.z),
			Zenith_Maths::Vector3(xEaveMax.x, xEaveMin.y + fParapet, xEaveMax.z), xIsland);
	}
}

// ============================================================================
// Skeleton + finalisation.
// ============================================================================

u_int ZM_GenAddBone(ZM_GenMesh& xMesh, const char* szName, int iParent,
	const Zenith_Maths::Vector3& xLocalPos, const Zenith_Maths::Quat& xLocalRot,
	const Zenith_Maths::Vector3& xLocalScale)
{
	Zenith_Assert(xMesh.GetNumBones() < uZM_GEN_ENGINE_MAX_BONES,
		"ZM_GenAddBone: exceeds Zenith_SkeletonAsset::MAX_BONES (100)");
	Zenith_Assert(iParent >= -1, "bone parent index %d is invalid (-1 == root)", iParent);
	Zenith_Assert(iParent < static_cast<int>(xMesh.GetNumBones()),
		"ZM_GenAddBone: parent must be added before child (parent-before-child)");

	ZM_GenBone xBone;
	// Zero the whole fixed name buffer first so the struct stays deterministically
	// memcmp-able, then copy (truncating) and force a NUL terminator.
	memset(xBone.m_szName, 0, uZM_GEN_BONE_NAME_MAX);
	if (szName != nullptr)
	{
		size_t uLen = strlen(szName);
		Zenith_Assert(uLen < static_cast<size_t>(uZM_GEN_BONE_NAME_MAX),
			"ZM_GenAddBone: bone name too long for the fixed buffer");
		// Truncate defensively (release builds skip the assert); the buffer is
		// already zero-filled so the NUL terminator is guaranteed.
		if (uLen > static_cast<size_t>(uZM_GEN_BONE_NAME_MAX - 1u))
		{
			uLen = static_cast<size_t>(uZM_GEN_BONE_NAME_MAX - 1u);
		}
		memcpy(xBone.m_szName, szName, uLen);
	}
	xBone.m_iParent = iParent;
	xBone.m_xLocalPos = xLocalPos;
	xBone.m_xLocalRot = xLocalRot;
	xBone.m_xLocalScale = xLocalScale;

	const u_int uIndex = xMesh.GetNumBones();
	xMesh.m_xBones.PushBack(xBone);
	return uIndex;
}

void ZM_GenGenerateNormals(ZM_GenMesh& xMesh)
{
	const u_int uNumVerts = xMesh.GetNumVerts();

	// Reset to zero, one per vertex.
	xMesh.m_xNormals.Clear();
	xMesh.m_xNormals.Reserve(uNumVerts);
	for (u_int u = 0; u < uNumVerts; u++)
	{
		xMesh.m_xNormals.PushBack(Zenith_Maths::Vector3(0.0f));
	}

	// Accumulate cross(C-A, B-A) -- the OUTWARD face normal for the repo's
	// left-handed front-face rule (matches the StickFigure port's custom normal
	// pass; NOT the terrain-inverted variant, NOT Zenith_MeshAsset's cross(B-A,C-A)).
	const u_int uNumTris = xMesh.GetNumTris();
	for (u_int u = 0; u < uNumTris; u++)
	{
		const u_int uA = xMesh.m_xIndices.Get(u * 3u + 0u);
		const u_int uB = xMesh.m_xIndices.Get(u * 3u + 1u);
		const u_int uC = xMesh.m_xIndices.Get(u * 3u + 2u);
		const Zenith_Maths::Vector3& xA = xMesh.m_xPositions.Get(uA);
		const Zenith_Maths::Vector3& xB = xMesh.m_xPositions.Get(uB);
		const Zenith_Maths::Vector3& xC = xMesh.m_xPositions.Get(uC);
		const Zenith_Maths::Vector3 xFace = glm::cross(xC - xA, xB - xA);
		xMesh.m_xNormals.Get(uA) += xFace;
		xMesh.m_xNormals.Get(uB) += xFace;
		xMesh.m_xNormals.Get(uC) += xFace;
	}

	// Normalise; guard zero-length with a fixed deterministic fallback (no NaN).
	for (u_int u = 0; u < uNumVerts; u++)
	{
		Zenith_Maths::Vector3& xN = xMesh.m_xNormals.Get(u);
		const float fLen = glm::length(xN);
		xN = (fLen > 1.0e-8f) ? (xN / fLen) : Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	}
}

void ZM_GenGenerateTangents(ZM_GenMesh& xMesh)
{
	const u_int uNumVerts = xMesh.GetNumVerts();

	xMesh.m_xTangents.Clear();
	xMesh.m_xTangents.Reserve(uNumVerts);
	for (u_int u = 0; u < uNumVerts; u++)
	{
		xMesh.m_xTangents.PushBack(Zenith_Maths::Vector3(0.0f));
	}

	// Per-face UV-gradient accumulation (ported from Zenith_MeshAsset::Generate-
	// Tangents), skipping UV-degenerate triangles.
	const u_int uNumTris = xMesh.GetNumTris();
	for (u_int u = 0; u < uNumTris; u++)
	{
		const u_int uA = xMesh.m_xIndices.Get(u * 3u + 0u);
		const u_int uB = xMesh.m_xIndices.Get(u * 3u + 1u);
		const u_int uC = xMesh.m_xIndices.Get(u * 3u + 2u);

		const Zenith_Maths::Vector3& xPosA = xMesh.m_xPositions.Get(uA);
		const Zenith_Maths::Vector3& xPosB = xMesh.m_xPositions.Get(uB);
		const Zenith_Maths::Vector3& xPosC = xMesh.m_xPositions.Get(uC);

		const Zenith_Maths::Vector2& xUVA = xMesh.m_xUVs.Get(uA);
		const Zenith_Maths::Vector2& xUVB = xMesh.m_xUVs.Get(uB);
		const Zenith_Maths::Vector2& xUVC = xMesh.m_xUVs.Get(uC);

		const Zenith_Maths::Vector3 xEdge1 = xPosB - xPosA;
		const Zenith_Maths::Vector3 xEdge2 = xPosC - xPosA;
		const Zenith_Maths::Vector2 xDeltaUV1 = xUVB - xUVA;
		const Zenith_Maths::Vector2 xDeltaUV2 = xUVC - xUVA;

		const float fDet = xDeltaUV1.x * xDeltaUV2.y - xDeltaUV2.x * xDeltaUV1.y;
		if (fabsf(fDet) < 0.0001f)
		{
			continue;
		}
		const float fInvDet = 1.0f / fDet;
		const Zenith_Maths::Vector3 xTangent = fInvDet * (xDeltaUV2.y * xEdge1 - xDeltaUV1.y * xEdge2);

		xMesh.m_xTangents.Get(uA) += xTangent;
		xMesh.m_xTangents.Get(uB) += xTangent;
		xMesh.m_xTangents.Get(uC) += xTangent;
	}

	// Gram-Schmidt orthonormalise against the vertex normal; guard zero-length
	// with a deterministic frame built from the normal (no NaN reaches the buffer).
	const bool bHasNormals = xMesh.m_xNormals.GetSize() == uNumVerts;
	for (u_int u = 0; u < uNumVerts; u++)
	{
		const Zenith_Maths::Vector3 xNormal = bHasNormals
			? xMesh.m_xNormals.Get(u)
			: Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		Zenith_Maths::Vector3& xTangent = xMesh.m_xTangents.Get(u);

		Zenith_Maths::Vector3 xOrtho = xTangent - xNormal * glm::dot(xNormal, xTangent);
		const float fLen = glm::length(xOrtho);
		if (fLen > 1.0e-8f)
		{
			xTangent = xOrtho / fLen;
		}
		else
		{
			// Deterministic fallback frame: cross the normal with whichever world
			// axis it is least aligned to.
			const Zenith_Maths::Vector3 xAxis = (fabsf(xNormal.x) > 0.9f)
				? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
				: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
			Zenith_Maths::Vector3 xFallback = glm::cross(xAxis, xNormal);
			const float fFbLen = glm::length(xFallback);
			xTangent = (fFbLen > 1.0e-8f)
				? (xFallback / fFbLen)
				: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		}
	}
}

void ZM_GenNormalizeSkinWeights(ZM_GenMesh& xMesh)
{
	const u_int uNumVerts = xMesh.GetNumVerts();
	if (xMesh.m_xBoneWeights.GetSize() != uNumVerts || xMesh.m_xBoneIndices.GetSize() != uNumVerts)
	{
		return;
	}

	for (u_int u = 0; u < uNumVerts; u++)
	{
		glm::vec4& xW = xMesh.m_xBoneWeights.Get(u);
		float afW[4] = { xW.x, xW.y, xW.z, xW.w };

		// Keep the two heaviest, in fixed component order for the tie-break.
		int iTop0 = 0;
		for (int i = 1; i < 4; i++) { if (afW[i] > afW[iTop0]) { iTop0 = i; } }
		int iTop1 = -1;
		for (int i = 0; i < 4; i++)
		{
			if (i == iTop0) { continue; }
			if (iTop1 < 0 || afW[i] > afW[iTop1]) { iTop1 = i; }
		}

		const float fW0 = (afW[iTop0] > 0.0f) ? afW[iTop0] : 0.0f;
		const float fW1 = (iTop1 >= 0 && afW[iTop1] > 0.0f) ? afW[iTop1] : 0.0f;
		const float fSum = fW0 + fW1;

		float afOut[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		if (fSum > 1.0e-8f)
		{
			afOut[iTop0] = fW0 / fSum;
			if (iTop1 >= 0) { afOut[iTop1] = fW1 / fSum; }
		}
		else
		{
			// Fully degenerate weights: bind rigidly to the first influence.
			afOut[0] = 1.0f;
		}
		xW = glm::vec4(afOut[0], afOut[1], afOut[2], afOut[3]);
	}
}

// ============================================================================
// Validation.
// ============================================================================

ZM_GenMeshValidation ZM_ValidateGenMesh(const ZM_GenMesh& xMesh, u_int uBoneCap, float fWeightTol)
{
	ZM_GenMeshValidation xResult;

	const u_int uNumVerts = xMesh.GetNumVerts();
	const u_int uNumTris = xMesh.GetNumTris();
	const bool bHasNormals = xMesh.m_xNormals.GetSize() == uNumVerts;

	// --- Winding: cross(C-A,B-A) . avg(vertex normals) > 0 for every triangle ---
	bool bWinding = (uNumTris > 0u) && bHasNormals;
	for (u_int u = 0; u < uNumTris; u++)
	{
		const u_int uA = xMesh.m_xIndices.Get(u * 3u + 0u);
		const u_int uB = xMesh.m_xIndices.Get(u * 3u + 1u);
		const u_int uC = xMesh.m_xIndices.Get(u * 3u + 2u);
		const Zenith_Maths::Vector3& xA = xMesh.m_xPositions.Get(uA);
		const Zenith_Maths::Vector3& xB = xMesh.m_xPositions.Get(uB);
		const Zenith_Maths::Vector3& xC = xMesh.m_xPositions.Get(uC);
		const Zenith_Maths::Vector3 xFace = glm::cross(xC - xA, xB - xA);
		Zenith_Maths::Vector3 xAvgN(0.0f);
		if (bHasNormals)
		{
			xAvgN = xMesh.m_xNormals.Get(uA) + xMesh.m_xNormals.Get(uB) + xMesh.m_xNormals.Get(uC);
		}
		if (glm::dot(xFace, xAvgN) <= 0.0f)
		{
			bWinding = false;
			if (xResult.m_uFirstBadTriangle == 0xFFFFFFFFu)
			{
				xResult.m_uFirstBadTriangle = u;
			}
		}
	}
	xResult.m_bWindingOutward = bWinding;

	// --- Bounds non-degenerate on all three axes ---
	if (uNumVerts > 0u)
	{
		const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
		const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);
		constexpr float fEps = 1.0e-5f;
		xResult.m_bBoundsNonDegen =
			(xMax.x - xMin.x) > fEps &&
			(xMax.y - xMin.y) > fEps &&
			(xMax.z - xMin.z) > fEps;
	}

	// --- Skin weights: sum to 1 (within tol) and at most 2 non-zero influences ---
	const bool bHasWeights = xMesh.m_xBoneWeights.GetSize() == uNumVerts;
	bool bSumOk = bHasWeights && (uNumVerts > 0u);
	bool bAtMostTwo = bHasWeights && (uNumVerts > 0u);
	if (bHasWeights)
	{
		for (u_int u = 0; u < uNumVerts; u++)
		{
			const glm::vec4& xW = xMesh.m_xBoneWeights.Get(u);
			const float fSum = xW.x + xW.y + xW.z + xW.w;
			u_int uNonZero = 0u;
			if (xW.x != 0.0f) { uNonZero++; }
			if (xW.y != 0.0f) { uNonZero++; }
			if (xW.z != 0.0f) { uNonZero++; }
			if (xW.w != 0.0f) { uNonZero++; }

			const bool bVertexBad = (fabsf(fSum - 1.0f) > fWeightTol) || (uNonZero > 2u);
			if (fabsf(fSum - 1.0f) > fWeightTol) { bSumOk = false; }
			if (uNonZero > 2u) { bAtMostTwo = false; }
			if (bVertexBad && xResult.m_uFirstBadVertex == 0xFFFFFFFFu)
			{
				xResult.m_uFirstBadVertex = u;
			}
		}
	}
	xResult.m_bWeightsSumToOne = bSumOk;
	xResult.m_bWeightsAtMostTwo = bAtMostTwo;

	// --- Bone count within cap ---
	xResult.m_bBonesWithinCap = xMesh.GetNumBones() <= uBoneCap;

	return xResult;
}

ZM_GenStaticMeshValidation ZM_ValidateGenMeshStatic(const ZM_GenMesh& xMesh)
{
	ZM_GenStaticMeshValidation xResult;

	const u_int uNumVerts = xMesh.GetNumVerts();
	const u_int uNumTris = xMesh.GetNumTris();
	const u_int uNumIndices = xMesh.m_xIndices.GetSize();
	const bool bHasNormals = xMesh.m_xNormals.GetSize() == uNumVerts;

	// --- Indices in range + triangle-aligned count ---
	bool bIndices = (uNumVerts > 0u) && (uNumIndices > 0u) && ((uNumIndices % 3u) == 0u);
	for (u_int i = 0; i < uNumIndices && bIndices; i++)
	{
		if (xMesh.m_xIndices.Get(i) >= uNumVerts)
		{
			bIndices = false;
		}
	}
	xResult.m_bIndicesInRange = bIndices;

	// --- Winding: cross(C-A,B-A) . avg(vertex normals) > 0 for every triangle ---
	// (same outward rule as the skinned ZM_ValidateGenMesh). Only run once indices
	// are known in range + normals present so no dereference goes out of bounds.
	bool bWinding = (uNumTris > 0u) && bHasNormals && bIndices;
	for (u_int u = 0; u < uNumTris && bIndices && bHasNormals; u++)
	{
		const u_int uA = xMesh.m_xIndices.Get(u * 3u + 0u);
		const u_int uB = xMesh.m_xIndices.Get(u * 3u + 1u);
		const u_int uC = xMesh.m_xIndices.Get(u * 3u + 2u);
		const Zenith_Maths::Vector3& xA = xMesh.m_xPositions.Get(uA);
		const Zenith_Maths::Vector3& xB = xMesh.m_xPositions.Get(uB);
		const Zenith_Maths::Vector3& xC = xMesh.m_xPositions.Get(uC);
		const Zenith_Maths::Vector3 xFace = glm::cross(xC - xA, xB - xA);
		const Zenith_Maths::Vector3 xAvgN =
			xMesh.m_xNormals.Get(uA) + xMesh.m_xNormals.Get(uB) + xMesh.m_xNormals.Get(uC);
		if (glm::dot(xFace, xAvgN) <= 0.0f)
		{
			bWinding = false;
			if (xResult.m_uFirstBadTriangle == 0xFFFFFFFFu)
			{
				xResult.m_uFirstBadTriangle = u;
			}
		}
	}
	xResult.m_bWindingOutward = bWinding;

	// --- Bounds non-degenerate on all three axes ---
	if (uNumVerts > 0u)
	{
		const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
		const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);
		constexpr float fEps = 1.0e-5f;
		xResult.m_bBoundsNonDegen =
			(xMax.x - xMin.x) > fEps &&
			(xMax.y - xMin.y) > fEps &&
			(xMax.z - xMin.z) > fEps;
	}

	// --- UVs finite and within [0,1] (one per vertex) ---
	bool bUVs = (xMesh.m_xUVs.GetSize() == uNumVerts) && (uNumVerts > 0u);
	constexpr float fUVeps = 1.0e-4f;
	for (u_int u = 0; u < xMesh.m_xUVs.GetSize() && bUVs; u++)
	{
		const Zenith_Maths::Vector2& xUV = xMesh.m_xUVs.Get(u);
		if (!std::isfinite(xUV.x) || !std::isfinite(xUV.y)
			|| xUV.x < -fUVeps || xUV.x > 1.0f + fUVeps
			|| xUV.y < -fUVeps || xUV.y > 1.0f + fUVeps)
		{
			bUVs = false;
		}
	}
	xResult.m_bUVsFinite = bUVs;

	// --- The static contract: zero bones, byte-empty skin buffers ---
	xResult.m_bNoSkeleton = (xMesh.GetNumBones() == 0u);
	xResult.m_bNoSkinBuffers =
		(xMesh.m_xBoneIndices.GetSize() == 0u) && (xMesh.m_xBoneWeights.GetSize() == 0u);

	// --- Rollup ---
	xResult.m_bAllValid = xResult.m_bWindingOutward && xResult.m_bBoundsNonDegen
		&& xResult.m_bIndicesInRange && xResult.m_bUVsFinite
		&& xResult.m_bNoSkeleton && xResult.m_bNoSkinBuffers;

	return xResult;
}

// ============================================================================
// Disk-bake bridge (TOOLS ONLY).
// ============================================================================
#ifdef ZENITH_TOOLS
bool ZM_GenBakeMesh(const ZM_GenMesh& xMesh, const char* szMeshPath,
	const char* szSkeletonPath, const char* szSkeletonRef)
{
	if (szMeshPath == nullptr || szSkeletonPath == nullptr || szSkeletonRef == nullptr)
	{
		return false;
	}

	const u_int uNumVerts = xMesh.GetNumVerts();
	const u_int uNumIndices = xMesh.m_xIndices.GetSize();

	// --- Element-wise copy into a stack Zenith_MeshAsset (no re-derive) ---
	Zenith_MeshAsset xAsset;
	xAsset.Reserve(uNumVerts, uNumIndices);

	const bool bHasTangents = xMesh.m_xTangents.GetSize() == uNumVerts;
	const bool bHasColors = xMesh.m_xColors.GetSize() == uNumVerts;
	for (u_int u = 0; u < uNumVerts; u++)
	{
		const Zenith_Maths::Vector3 xTangent = bHasTangents
			? xMesh.m_xTangents.Get(u)
			: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		const Zenith_Maths::Vector4 xColor = bHasColors
			? xMesh.m_xColors.Get(u)
			: Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		xAsset.AddVertex(xMesh.m_xPositions.Get(u), xMesh.m_xNormals.Get(u),
			xMesh.m_xUVs.Get(u), xTangent, xColor);
	}

	// Bitangents = cross(N, T) per vertex (AddVertex does not populate them);
	// matches Zenith_MeshAsset::GenerateTangents' output so the asset is complete.
	for (u_int u = 0; u < uNumVerts; u++)
	{
		const Zenith_Maths::Vector3 xTangent = bHasTangents
			? xMesh.m_xTangents.Get(u)
			: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		xAsset.m_xBitangents.PushBack(glm::cross(xMesh.m_xNormals.Get(u), xTangent));
	}

	const u_int uNumTris = xMesh.GetNumTris();
	for (u_int u = 0; u < uNumTris; u++)
	{
		xAsset.AddTriangle(xMesh.m_xIndices.Get(u * 3u + 0u),
			xMesh.m_xIndices.Get(u * 3u + 1u), xMesh.m_xIndices.Get(u * 3u + 2u));
	}

	const bool bHasSkin = xMesh.m_xBoneIndices.GetSize() == uNumVerts
		&& xMesh.m_xBoneWeights.GetSize() == uNumVerts;
	if (bHasSkin)
	{
		for (u_int u = 0; u < uNumVerts; u++)
		{
			xAsset.SetVertexSkinning(u, xMesh.m_xBoneIndices.Get(u), xMesh.m_xBoneWeights.Get(u));
		}
	}

	if (uNumIndices > 0u)
	{
		xAsset.AddSubmesh(0u, uNumIndices, 0u);
	}

	// --- Skeleton from m_xBones (parent-before-child guaranteed by construction) ---
	Zenith_SkeletonAsset xSkeleton;
	for (u_int b = 0; b < xMesh.GetNumBones(); b++)
	{
		const ZM_GenBone& xBone = xMesh.m_xBones.Get(b);
		xSkeleton.AddBone(std::string(xBone.m_szName), xBone.m_iParent,
			xBone.m_xLocalPos, xBone.m_xLocalRot, xBone.m_xLocalScale);
	}
	xSkeleton.ComputeBindPoseMatrices();

	xAsset.SetSkeletonPath(std::string(szSkeletonRef));
	xAsset.ComputeBounds();

	// --- create_directories (tolerating an absent Assets/ tree), then Export ---
	std::error_code xEc;
	const std::filesystem::path xMeshFsPath(szMeshPath);
	const std::filesystem::path xSkelFsPath(szSkeletonPath);
	if (xMeshFsPath.has_parent_path())
	{
		std::filesystem::create_directories(xMeshFsPath.parent_path(), xEc);
	}
	if (xSkelFsPath.has_parent_path())
	{
		std::filesystem::create_directories(xSkelFsPath.parent_path(), xEc);
	}

	xSkeleton.Export(szSkeletonPath);
	xAsset.Export(szMeshPath);

	// Export is void; verify both artifacts landed as the IO-success signal.
	const bool bMeshOk = std::filesystem::exists(xMeshFsPath, xEc);
	const bool bSkelOk = std::filesystem::exists(xSkelFsPath, xEc);
	return bMeshOk && bSkelOk;
}

// ---- Skeleton-only bake (shared rig) --------------------------------------
// Builds a Zenith_SkeletonAsset from m_xBones (the SAME AddBone + Compute-
// BindPoseMatrices block ZM_GenBakeMesh uses) and Exports ONLY the .zskel. For a
// SHARED rig that many models bind by ref (humans), baked ONCE.
bool ZM_GenBakeSkeleton(const ZM_GenMesh& xMesh, const char* szSkeletonPath)
{
	if (szSkeletonPath == nullptr) { return false; }

	Zenith_SkeletonAsset xSkeleton;
	for (u_int b = 0; b < xMesh.GetNumBones(); b++)
	{
		const ZM_GenBone& xBone = xMesh.m_xBones.Get(b);
		xSkeleton.AddBone(std::string(xBone.m_szName), xBone.m_iParent,
			xBone.m_xLocalPos, xBone.m_xLocalRot, xBone.m_xLocalScale);
	}
	xSkeleton.ComputeBindPoseMatrices();

	std::error_code xEc;
	const std::filesystem::path xSkelFsPath(szSkeletonPath);
	if (xSkelFsPath.has_parent_path())
	{
		std::filesystem::create_directories(xSkelFsPath.parent_path(), xEc);
	}
	xSkeleton.Export(szSkeletonPath);
	return std::filesystem::exists(xSkelFsPath, xEc);
}

// ---- Mesh-only bake binding an EXISTING shared skeleton -------------------
// The vertex/triangle/skin copy block below is VERBATIM the ZM_GenBakeMesh block
// above, duplicated so ZM_GenBakeMesh stays byte-for-byte untouched (which is what
// guarantees creatures re-bake identically). The ONLY differences vs ZM_GenBakeMesh
// are: no szSkeletonPath param, no Zenith_SkeletonAsset build/Export, no skel-path
// create_directories.
bool ZM_GenBakeMeshWithSharedSkeleton(const ZM_GenMesh& xMesh, const char* szMeshPath,
	const char* szSkeletonRef)
{
	if (szMeshPath == nullptr || szSkeletonRef == nullptr)
	{
		return false;
	}

	const u_int uNumVerts = xMesh.GetNumVerts();
	const u_int uNumIndices = xMesh.m_xIndices.GetSize();

	// --- Element-wise copy into a stack Zenith_MeshAsset (no re-derive) ---
	Zenith_MeshAsset xAsset;
	xAsset.Reserve(uNumVerts, uNumIndices);

	const bool bHasTangents = xMesh.m_xTangents.GetSize() == uNumVerts;
	const bool bHasColors = xMesh.m_xColors.GetSize() == uNumVerts;
	for (u_int u = 0; u < uNumVerts; u++)
	{
		const Zenith_Maths::Vector3 xTangent = bHasTangents
			? xMesh.m_xTangents.Get(u)
			: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		const Zenith_Maths::Vector4 xColor = bHasColors
			? xMesh.m_xColors.Get(u)
			: Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		xAsset.AddVertex(xMesh.m_xPositions.Get(u), xMesh.m_xNormals.Get(u),
			xMesh.m_xUVs.Get(u), xTangent, xColor);
	}

	// Bitangents = cross(N, T) per vertex (AddVertex does not populate them);
	// matches Zenith_MeshAsset::GenerateTangents' output so the asset is complete.
	for (u_int u = 0; u < uNumVerts; u++)
	{
		const Zenith_Maths::Vector3 xTangent = bHasTangents
			? xMesh.m_xTangents.Get(u)
			: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
		xAsset.m_xBitangents.PushBack(glm::cross(xMesh.m_xNormals.Get(u), xTangent));
	}

	const u_int uNumTris = xMesh.GetNumTris();
	for (u_int u = 0; u < uNumTris; u++)
	{
		xAsset.AddTriangle(xMesh.m_xIndices.Get(u * 3u + 0u),
			xMesh.m_xIndices.Get(u * 3u + 1u), xMesh.m_xIndices.Get(u * 3u + 2u));
	}

	const bool bHasSkin = xMesh.m_xBoneIndices.GetSize() == uNumVerts
		&& xMesh.m_xBoneWeights.GetSize() == uNumVerts;
	if (bHasSkin)
	{
		for (u_int u = 0; u < uNumVerts; u++)
		{
			xAsset.SetVertexSkinning(u, xMesh.m_xBoneIndices.Get(u), xMesh.m_xBoneWeights.Get(u));
		}
	}

	if (uNumIndices > 0u)
	{
		xAsset.AddSubmesh(0u, uNumIndices, 0u);
	}

	xAsset.SetSkeletonPath(std::string(szSkeletonRef));   // SHARED ref; NO Zenith_SkeletonAsset built, NO .zskel written
	xAsset.ComputeBounds();

	// --- create_directories (tolerating an absent Assets/ tree), then Export ---
	std::error_code xEc;
	const std::filesystem::path xMeshFsPath(szMeshPath);
	if (xMeshFsPath.has_parent_path())
	{
		std::filesystem::create_directories(xMeshFsPath.parent_path(), xEc);
	}

	xAsset.Export(szMeshPath);

	// Export is void; verify the artifact landed as the IO-success signal.
	return std::filesystem::exists(xMeshFsPath, xEc);
}
#endif
