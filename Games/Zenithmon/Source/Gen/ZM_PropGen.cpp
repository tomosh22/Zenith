#include "Zenith.h"

// ============================================================================
// ZM_PropGen -- the S4 prop generator driver. See the header for the architecture
// + determinism contract. This TU owns: prop -> recipe resolution, the per-domain
// seed derivation, the static composition mesh builder (per-kind part set on a
// role-keyed UV atlas + MESH-domain jitter), the albedo / height / emissive
// painters (ALBEDO domain only), the full-bundle driver, the byte-identity + hash
// + validation machinery, the asset-path scheme, the engine-rock substitution,
// and (tools only, SC5) the disk bake.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_PropGen.h"

#include <cmath>     // sqrtf
#include <cstdio>    // snprintf
#include <cstring>   // memcmp

namespace
{
	// FNV-1a constants (byte-identical to ZM_GenHashName / the building content hash).
	constexpr u_int uZM_FNV_OFFSET = 2166136261u;
	constexpr u_int uZM_FNV_PRIME  = 16777619u;

	// Fold a raw byte range into a running FNV-1a hash.
	u_int ZM_FnvAccum(u_int uHash, const void* pData, size_t uBytes)
	{
		const u_int8* pByte = static_cast<const u_int8*>(pData);
		for (size_t i = 0; i < uBytes; ++i)
		{
			uHash ^= pByte[i];
			uHash *= uZM_FNV_PRIME;
		}
		return uHash;
	}

	// Fold a whole SoA buffer's bytes into a running FNV hash.
	template <typename T>
	u_int ZM_FnvAccumBuffer(u_int uHash, const Zenith_Vector<T>& xVec)
	{
		if (xVec.GetSize() == 0u) { return uHash; }
		return ZM_FnvAccum(uHash, xVec.GetDataPointer(),
			static_cast<size_t>(xVec.GetSize()) * sizeof(T));
	}

	// Byte-exact compare of one SoA buffer pair (sizes first, then memcmp).
	template <typename T>
	bool ZM_BufferBytesEqual(const Zenith_Vector<T>& xA, const Zenith_Vector<T>& xB)
	{
		if (xA.GetSize() != xB.GetSize()) { return false; }
		if (xA.GetSize() == 0u)           { return true;  }
		return memcmp(xA.GetDataPointer(), xB.GetDataPointer(),
			static_cast<size_t>(xA.GetSize()) * sizeof(T)) == 0;
	}

	// Small colour helpers -- ZM_TextureSynth.cpp keeps its Clamp01/Lerp3 file-local
	// (static, not visible here), so the painters get their own copies.
	inline float ZM_Clamp01f(float f) { return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); }
	inline Zenith_Maths::Vector3 ZM_ClampV3(const Zenith_Maths::Vector3& v)
	{ return Zenith_Maths::Vector3(ZM_Clamp01f(v.x), ZM_Clamp01f(v.y), ZM_Clamp01f(v.z)); }
	inline Zenith_Maths::Vector3 ZM_LerpV3(const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b, float t)
	{ return Zenith_Maths::Vector3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }
	inline Zenith_Maths::Vector3 ZM_ScaleV3(const Zenith_Maths::Vector3& a, float f)
	{ return Zenith_Maths::Vector3(a.x * f, a.y * f, a.z * f); }

	// Per-biome tint (the dressing sets lerp their palette base toward this; the
	// generic town props stay ZM_PROP_BIOME_NONE and never tint).
	Zenith_Maths::Vector3 ZM_PropBiomeColour(ZM_PROP_BIOME e)
	{
		switch (e)
		{
		case ZM_PROP_BIOME_MEADOW:   return Zenith_Maths::Vector3(0.45f, 0.62f, 0.32f);
		case ZM_PROP_BIOME_VOLCANIC: return Zenith_Maths::Vector3(0.42f, 0.22f, 0.18f);
		case ZM_PROP_BIOME_COAST:    return Zenith_Maths::Vector3(0.42f, 0.58f, 0.66f);
		case ZM_PROP_BIOME_WETLAND:  return Zenith_Maths::Vector3(0.34f, 0.40f, 0.28f);
		case ZM_PROP_BIOME_SNOW:     return Zenith_Maths::Vector3(0.82f, 0.86f, 0.90f);
		case ZM_PROP_BIOME_CANYON:   return Zenith_Maths::Vector3(0.62f, 0.40f, 0.28f);
		default:                     return Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f);
		}
	}

	// ---- The role atlas --------------------------------------------------------
	//
	// Four quadrants, one per ZM_PROP_UV_ROLE. The mesh islands sit inside their
	// quadrant by this gutter so a bilinear tap at an island edge never reads
	// the neighbouring role. 2% of 512 is ~10 texels -- past the 8 px minimum
	// ZM_GenUVIsland's own comment asks for.
	constexpr float fZM_PROP_UV_GUTTER = 0.02f;

	void ZM_PropRoleQuadrant(ZM_PROP_UV_ROLE eRole, float& fU0, float& fV0, float& fU1, float& fV1)
	{
		switch (eRole)
		{
		case ZM_PROP_UV_SECONDARY: fU0 = 0.5f; fV0 = 0.0f; fU1 = 1.0f; fV1 = 0.5f; break;
		case ZM_PROP_UV_ACCENT:    fU0 = 0.0f; fV0 = 0.5f; fU1 = 0.5f; fV1 = 1.0f; break;
		case ZM_PROP_UV_GLOW:      fU0 = 0.5f; fV0 = 0.5f; fU1 = 1.0f; fV1 = 1.0f; break;
		case ZM_PROP_UV_PRIMARY:
		default:                   fU0 = 0.0f; fV0 = 0.0f; fU1 = 0.5f; fV1 = 0.5f; break;
		}
	}

	// A sub-rectangle of an island, in the island's own normalized space. Used to
	// give each side of a prism its own strip so the grain does not repeat eight
	// times around a post.
	ZM_GenUVIsland ZM_SubIsland(const ZM_GenUVIsland& xIsland, float fU0, float fV0, float fU1, float fV1)
	{
		ZM_GenUVIsland xOut;
		xOut.m_fU0 = xIsland.U(fU0); xOut.m_fV0 = xIsland.V(fV0);
		xOut.m_fU1 = xIsland.U(fU1); xOut.m_fV1 = xIsland.V(fV1);
		return xOut;
	}

	// ---- Static primitives beyond the axis-aligned box ------------------------
	//
	// ZM_StaticMesh owns the box. A CHAMFERED, optionally TAPERED prism is what a
	// fence post, a rail, a sign post or a lamp stem actually is -- a square
	// section with its arrises taken off -- and it is emitted here, file-local,
	// with the same contract as the box: per-face hard normals, outward winding
	// under cross(C-A,B-A), positions/normals/uvs/colours and NO bone buffers.

	void ZM_PGPushVertex(ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xP,
		const Zenith_Maths::Vector3& xN, const Zenith_Maths::Vector2& xUV)
	{
		xMesh.m_xPositions.PushBack(xP);
		xMesh.m_xNormals.PushBack(xN);
		xMesh.m_xUVs.PushBack(xUV);
		xMesh.m_xColors.PushBack(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
	}

	void ZM_PGPushTri(ZM_GenMesh& xMesh, u_int uA, u_int uB, u_int uC)
	{
		xMesh.m_xIndices.PushBack(uA);
		xMesh.m_xIndices.PushBack(uB);
		xMesh.m_xIndices.PushBack(uC);
	}

	Zenith_Maths::Vector3 ZM_PGNormalize(const Zenith_Maths::Vector3& xV, const Zenith_Maths::Vector3& xFallback)
	{
		const float fLenSq = xV.x * xV.x + xV.y * xV.y + xV.z * xV.z;
		if (fLenSq <= 1.0e-12f) { return xFallback; }
		const float fInv = 1.0f / sqrtf(fLenSq);
		return Zenith_Maths::Vector3(xV.x * fInv, xV.y * fInv, xV.z * fInv);
	}

	// One quad, corners in the {BL, BR, TL, TR} layout ZM_StaticMesh uses, wound
	// so its normal points AWAY from xInside. The UVs stay in their slots whichever
	// winding is chosen -- only the index order and the stored normal flip.
	void ZM_PGPushQuadOutward(ZM_GenMesh& xMesh,
		const Zenith_Maths::Vector3& xBL, const Zenith_Maths::Vector3& xBR,
		const Zenith_Maths::Vector3& xTL, const Zenith_Maths::Vector3& xTR,
		const Zenith_Maths::Vector3& xInside, const ZM_GenUVIsland& xIsland)
	{
		const u_int uBase = xMesh.GetNumVerts();
		Zenith_Maths::Vector3 xN = ZM_PGNormalize(glm::cross(xBR - xBL, xTL - xBL),
			Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		const Zenith_Maths::Vector3 xCentroid = (xBL + xBR + xTL + xTR) * 0.25f;
		const bool bOutward = glm::dot(xN, xCentroid - xInside) >= 0.0f;
		if (!bOutward) { xN = -xN; }

		ZM_PGPushVertex(xMesh, xBL, xN, Zenith_Maths::Vector2(xIsland.U(0.0f), xIsland.V(0.0f)));
		ZM_PGPushVertex(xMesh, xBR, xN, Zenith_Maths::Vector2(xIsland.U(1.0f), xIsland.V(0.0f)));
		ZM_PGPushVertex(xMesh, xTL, xN, Zenith_Maths::Vector2(xIsland.U(0.0f), xIsland.V(1.0f)));
		ZM_PGPushVertex(xMesh, xTR, xN, Zenith_Maths::Vector2(xIsland.U(1.0f), xIsland.V(1.0f)));

		if (bOutward)
		{
			ZM_PGPushTri(xMesh, uBase + 0u, uBase + 2u, uBase + 1u);
			ZM_PGPushTri(xMesh, uBase + 1u, uBase + 2u, uBase + 3u);
		}
		else
		{
			ZM_PGPushTri(xMesh, uBase + 0u, uBase + 1u, uBase + 2u);
			ZM_PGPushTri(xMesh, uBase + 2u, uBase + 1u, uBase + 3u);
		}
	}

	// One triangle wound outward from xInside, with explicit UVs.
	void ZM_PGPushTriOutward(ZM_GenMesh& xMesh,
		const Zenith_Maths::Vector3& xV0, const Zenith_Maths::Vector3& xV1, const Zenith_Maths::Vector3& xV2,
		const Zenith_Maths::Vector3& xInside,
		const Zenith_Maths::Vector2& xUV0, const Zenith_Maths::Vector2& xUV1, const Zenith_Maths::Vector2& xUV2)
	{
		const u_int uBase = xMesh.GetNumVerts();
		Zenith_Maths::Vector3 xN = ZM_PGNormalize(glm::cross(xV2 - xV0, xV1 - xV0),
			Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		const Zenith_Maths::Vector3 xCentroid = (xV0 + xV1 + xV2) * (1.0f / 3.0f);
		const bool bOutward = glm::dot(xN, xCentroid - xInside) >= 0.0f;
		if (!bOutward) { xN = -xN; }

		ZM_PGPushVertex(xMesh, xV0, xN, xUV0);
		ZM_PGPushVertex(xMesh, xV1, xN, xUV1);
		ZM_PGPushVertex(xMesh, xV2, xN, xUV2);
		if (bOutward) { ZM_PGPushTri(xMesh, uBase + 0u, uBase + 1u, uBase + 2u); }
		else          { ZM_PGPushTri(xMesh, uBase + 0u, uBase + 2u, uBase + 1u); }
	}

	enum ZM_PG_AXIS : u_int { ZM_PG_AXIS_X, ZM_PG_AXIS_Y, ZM_PG_AXIS_Z };

	// The eight corners of a chamfered rectangle with half-sizes (a, b) and
	// chamfer c, in the cross-section's own (A, B) frame. c is clamped so the
	// chamfer can never consume a side.
	void ZM_PGChamferRing(float fHalfA, float fHalfB, float fChamfer, float afA[8], float afB[8])
	{
		float fC = fChamfer;
		const float fMin = fHalfA < fHalfB ? fHalfA : fHalfB;
		if (fC > 0.45f * fMin) { fC = 0.45f * fMin; }
		if (fC < 0.0f)         { fC = 0.0f; }
		const float a = fHalfA, b = fHalfB;
		afA[0] =  a;      afB[0] = -(b - fC);
		afA[1] =  a;      afB[1] =  (b - fC);
		afA[2] =  a - fC; afB[2] =  b;
		afA[3] = -(a - fC); afB[3] = b;
		afA[4] = -a;      afB[4] =  (b - fC);
		afA[5] = -a;      afB[5] = -(b - fC);
		afA[6] = -(a - fC); afB[6] = -b;
		afA[7] =  a - fC; afB[7] = -b;
	}

	// A chamfered prism along eAxis, starting at xStart (the centre of its first
	// cross-section) and running fLength along +axis. (fHalfA0, fHalfB0) is the
	// start section, (fHalfA1, fHalfB1) the end section -- unequal for a taper.
	// The cross axes are: Y -> (X, Z); X -> (Z, Y); Z -> (X, Y). The island's U
	// runs ALONG the prism so a grain stretched along U runs along the timber; V
	// is split into eight strips, one per side, so the pattern does not repeat
	// around the section.
	void ZM_PGAppendChamferedPrism(ZM_GenMesh& xMesh, ZM_PG_AXIS eAxis,
		const Zenith_Maths::Vector3& xStart, float fLength,
		float fHalfA0, float fHalfB0, float fHalfA1, float fHalfB1, float fChamfer,
		const ZM_GenUVIsland& xIsland, bool bCapStart, bool bCapEnd)
	{
		Zenith_Maths::Vector3 xAxis, xA, xB;
		switch (eAxis)
		{
		case ZM_PG_AXIS_X: xAxis = Zenith_Maths::Vector3(1, 0, 0); xA = Zenith_Maths::Vector3(0, 0, 1); xB = Zenith_Maths::Vector3(0, 1, 0); break;
		case ZM_PG_AXIS_Z: xAxis = Zenith_Maths::Vector3(0, 0, 1); xA = Zenith_Maths::Vector3(1, 0, 0); xB = Zenith_Maths::Vector3(0, 1, 0); break;
		case ZM_PG_AXIS_Y:
		default:           xAxis = Zenith_Maths::Vector3(0, 1, 0); xA = Zenith_Maths::Vector3(1, 0, 0); xB = Zenith_Maths::Vector3(0, 0, 1); break;
		}

		float afA0[8], afB0[8], afA1[8], afB1[8];
		ZM_PGChamferRing(fHalfA0, fHalfB0, fChamfer, afA0, afB0);
		ZM_PGChamferRing(fHalfA1, fHalfB1, fChamfer, afA1, afB1);

		const Zenith_Maths::Vector3 xEnd    = xStart + xAxis * fLength;
		const Zenith_Maths::Vector3 xInside = xStart + xAxis * (fLength * 0.5f);

		Zenith_Maths::Vector3 axR0[8], axR1[8];
		for (u_int i = 0u; i < 8u; ++i)
		{
			axR0[i] = xStart + xA * afA0[i] + xB * afB0[i];
			axR1[i] = xEnd   + xA * afA1[i] + xB * afB1[i];
		}

		for (u_int i = 0u; i < 8u; ++i)
		{
			const u_int j = (i + 1u) % 8u;
			const ZM_GenUVIsland xStrip = ZM_SubIsland(xIsland, 0.0f,
				static_cast<float>(i) / 8.0f, 1.0f, static_cast<float>(i + 1u) / 8.0f);
			// BL/BR along the axis at side-edge i, TL/TR at side-edge j: U runs
			// along the prism, V across the strip.
			ZM_PGPushQuadOutward(xMesh, axR0[i], axR1[i], axR0[j], axR1[j], xInside, xStrip);
		}

		auto CapUV = [&](float fA, float fB, float fHalfA, float fHalfB) -> Zenith_Maths::Vector2
		{
			const float fU = fHalfA > 1.0e-6f ? 0.5f + 0.5f * (fA / fHalfA) : 0.5f;
			const float fV = fHalfB > 1.0e-6f ? 0.5f + 0.5f * (fB / fHalfB) : 0.5f;
			return Zenith_Maths::Vector2(xIsland.U(fU), xIsland.V(fV));
		};
		if (bCapStart)
		{
			for (u_int i = 0u; i < 8u; ++i)
			{
				const u_int j = (i + 1u) % 8u;
				ZM_PGPushTriOutward(xMesh, xStart, axR0[i], axR0[j], xInside,
					CapUV(0.0f, 0.0f, fHalfA0, fHalfB0),
					CapUV(afA0[i], afB0[i], fHalfA0, fHalfB0),
					CapUV(afA0[j], afB0[j], fHalfA0, fHalfB0));
			}
		}
		if (bCapEnd)
		{
			for (u_int i = 0u; i < 8u; ++i)
			{
				const u_int j = (i + 1u) % 8u;
				ZM_PGPushTriOutward(xMesh, xEnd, axR1[i], axR1[j], xInside,
					CapUV(0.0f, 0.0f, fHalfA1, fHalfB1),
					CapUV(afA1[i], afB1[i], fHalfA1, fHalfB1),
					CapUV(afA1[j], afB1[j], fHalfA1, fHalfB1));
			}
		}
	}

	// (cx,cz) horizontal centre; y0 base; (sx,sy,sz) full sizes; the role picks the
	// island. Static, bone-free: wraps ZM_StaticMesh::AppendBox.
	void ZM_AppendPropBox(ZM_GenMesh& m, float cx, float cz, float y0,
		float sx, float sy, float sz, ZM_PROP_UV_ROLE eRole)
	{
		ZM_StaticMesh::AppendBox(m,
			Zenith_Maths::Vector3(cx - 0.5f * sx, y0,      cz - 0.5f * sz),
			Zenith_Maths::Vector3(cx + 0.5f * sx, y0 + sy, cz + 0.5f * sz), ZM_PropUVIsland(eRole));
	}

	// A vertical post from y0 up fH at (cx, cz): square section fHalf at the foot
	// tapering to fHalf * fTaper at the head, chamfered. The foot is buried in the
	// ground it stands on, so only the head is capped.
	void ZM_AppendPropPost(ZM_GenMesh& m, float cx, float cz, float y0, float fH,
		float fHalf, float fTaper, float fChamfer, ZM_PROP_UV_ROLE eRole)
	{
		ZM_PGAppendChamferedPrism(m, ZM_PG_AXIS_Y, Zenith_Maths::Vector3(cx, y0, cz), fH,
			fHalf, fHalf, fHalf * fTaper, fHalf * fTaper, fChamfer, ZM_PropUVIsland(eRole),
			false, true);
	}

	// A horizontal rail along X, centred at (cx, cy, cz), of length fL with a
	// (fHalfZ across, fHalfY tall) section, chamfered, capped both ends.
	void ZM_AppendPropRailX(ZM_GenMesh& m, float cx, float cy, float cz, float fL,
		float fHalfZ, float fHalfY, float fChamfer, ZM_PROP_UV_ROLE eRole)
	{
		ZM_PGAppendChamferedPrism(m, ZM_PG_AXIS_X, Zenith_Maths::Vector3(cx - 0.5f * fL, cy, cz), fL,
			fHalfZ, fHalfY, fHalfZ, fHalfY, fChamfer, ZM_PropUVIsland(eRole), true, true);
	}

	// The same along Z.
	void ZM_AppendPropRailZ(ZM_GenMesh& m, float cx, float cy, float cz, float fL,
		float fHalfX, float fHalfY, float fChamfer, ZM_PROP_UV_ROLE eRole)
	{
		ZM_PGAppendChamferedPrism(m, ZM_PG_AXIS_Z, Zenith_Maths::Vector3(cx, cy, cz - 0.5f * fL), fL,
			fHalfX, fHalfY, fHalfX, fHalfY, fChamfer, ZM_PropUVIsland(eRole), true, true);
	}

	// A lamp SHADE: an octagonal drum from y0 up fH, wider at the foot (fHalfBottom)
	// than the head (fHalfTop), closed at both ends so the glow reads from below
	// as well as from the side. Always the GLOW role -- that is what a shade is.
	void ZM_AppendPropShade(ZM_GenMesh& m, float cx, float cz, float y0, float fH,
		float fHalfBottom, float fHalfTop)
	{
		ZM_PGAppendChamferedPrism(m, ZM_PG_AXIS_Y, Zenith_Maths::Vector3(cx, y0, cz), fH,
			fHalfBottom, fHalfBottom, fHalfTop, fHalfTop, 0.30f * fHalfBottom,
			ZM_PropUVIsland(ZM_PROP_UV_GLOW), true, true);
	}

	// Per-kind per-model file basename pattern (embeds the prop name).
	const char* ZM_PropBasenameFmt(ZM_PROP_ASSET_KIND eKind)
	{
		switch (eKind)
		{
		case ZM_PROP_ASSET_MESH:        return "%s.zmesh";
		case ZM_PROP_ASSET_ALBEDO:      return "%s_albedo.ztxtr";
		case ZM_PROP_ASSET_NORMAL:      return "%s_normal.ztxtr";
		case ZM_PROP_ASSET_ROUGH_METAL: return "%s_rm.ztxtr";
		case ZM_PROP_ASSET_OCCLUSION:   return "%s_ao.ztxtr";
		case ZM_PROP_ASSET_EMISSIVE:    return "%s_emissive.ztxtr";
		case ZM_PROP_ASSET_MATERIAL:    return "%s.zmtrl";
		case ZM_PROP_ASSET_MODEL:       return "%s.zmodel";
		default:
			Zenith_Assert(false, "ZM_PropBasenameFmt: bad kind %u", (u_int)eKind);
			return "%s.bin";
		}
	}

	bool ZM_PropIsFixture(const ZM_PropRecipe& xR)
	{
		return xR.m_eKind == ZM_PROP_KIND_LIGHT_FIXTURE;
	}
}

// ============================================================================
// The role atlas -- public face.
// ============================================================================
ZM_GenUVIsland ZM_PropUVIsland(ZM_PROP_UV_ROLE eRole)
{
	ZM_GenUVIsland xI;
	float fU0, fV0, fU1, fV1;
	ZM_PropRoleQuadrant(eRole, fU0, fV0, fU1, fV1);
	xI.m_fU0 = fU0 + fZM_PROP_UV_GUTTER; xI.m_fV0 = fV0 + fZM_PROP_UV_GUTTER;
	xI.m_fU1 = fU1 - fZM_PROP_UV_GUTTER; xI.m_fV1 = fV1 - fZM_PROP_UV_GUTTER;
	return xI;
}

ZM_GenUVIsland ZM_PropUVPaintRect(ZM_PROP_UV_ROLE eRole)
{
	ZM_GenUVIsland xI;
	ZM_PropRoleQuadrant(eRole, xI.m_fU0, xI.m_fV0, xI.m_fU1, xI.m_fV1);
	return xI;
}

ZM_PROP_UV_ROLE ZM_PropUVRoleAt(float fU, float fV)
{
	if (fU < 0.5f)
	{
		return fV < 0.5f ? ZM_PROP_UV_PRIMARY : ZM_PROP_UV_ACCENT;
	}
	return fV < 0.5f ? ZM_PROP_UV_SECONDARY : ZM_PROP_UV_GLOW;
}

// Per-palette base colour (the albedo base before the biome tint + jitter;
// distinct per palette so different-palette props never collide). PUBLIC since
// ZM-67 -- see the header for why the cold-start presenter must read these five
// constants rather than keep a copy.
Zenith_Maths::Vector3 ZM_PropPaletteColour(ZM_PROP_PALETTE e)
{
	switch (e)
	{
	case ZM_PROP_PALETTE_WOOD:    return Zenith_Maths::Vector3(0.55f, 0.36f, 0.20f);
	case ZM_PROP_PALETTE_STONE:   return Zenith_Maths::Vector3(0.55f, 0.54f, 0.50f);
	case ZM_PROP_PALETTE_METAL:   return Zenith_Maths::Vector3(0.45f, 0.47f, 0.52f);
	case ZM_PROP_PALETTE_PAINTED: return Zenith_Maths::Vector3(0.75f, 0.30f, 0.28f);
	case ZM_PROP_PALETTE_FOLIAGE: return Zenith_Maths::Vector3(0.28f, 0.45f, 0.24f);
	default:
		Zenith_Assert(false, "ZM_PropPaletteColour: bad palette %u", (u_int)e);
		return Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f);
	}
}

// ============================================================================
// Emissive response + the engine-rock substitution.
// ============================================================================
ZM_PropEmissive ZM_GetPropEmissive(ZM_PROP_ID eId)
{
	// ★ THE COLOURS ARE THE LIGHTS THESE HOUSE (ZM_InteriorDressing.h): warm
	// tungsten for the three domestic fixtures, cool fluorescent for the batten.
	// ZM_Tests_PropGen cross-checks each against the light it stands under, so a
	// re-tune of one side reds rather than drifting the shade off its own lamp.
	ZM_PropEmissive xE;
	switch (eId)
	{
	case ZM_PROP_PENDANT_LAMP: xE.m_xColour = Zenith_Maths::Vector3(1.00f, 0.78f, 0.52f); xE.m_fIntensity = 5.0f; break;
	case ZM_PROP_BEDSIDE_LAMP: xE.m_xColour = Zenith_Maths::Vector3(1.00f, 0.72f, 0.44f); xE.m_fIntensity = 4.0f; break;
	case ZM_PROP_FLOOR_LAMP:   xE.m_xColour = Zenith_Maths::Vector3(1.00f, 0.74f, 0.46f); xE.m_fIntensity = 4.0f; break;
	case ZM_PROP_LAB_BATTEN:   xE.m_xColour = Zenith_Maths::Vector3(0.72f, 0.84f, 1.00f); xE.m_fIntensity = 6.0f; break;
	default: break;
	}
	return xE;
}

const char* ZM_PropEngineRockStem(ZM_PROP_ID eId)
{
	// Three roster rows, two engine stones: the rounded Boulder at two scales and
	// the leaning Shard for the mid-size row, so the three read as three rocks
	// rather than one rock three sizes. (Slab is a flat flagstone and Pebbles a
	// cluster -- neither is what a 1.4 m "large rock" row describes.)
	switch (eId)
	{
	case ZM_PROP_ROCK_SMALL: return "Boulder";
	case ZM_PROP_ROCK_LARGE: return "Shard";
	case ZM_PROP_BOULDER:    return "Boulder";
	default:                 return nullptr;
	}
}

namespace
{
	bool ZM_PropEngineRockRef(const char* szStem, const char* szExt, char* szOut, u_int uCap)
	{
		if (szOut == nullptr || uCap == 0u) { return false; }
		szOut[0] = '\0';
		const int iN = snprintf(szOut, uCap, "engine:Meshes/Rocks/Rock_%s%s", szStem, szExt);
		return iN >= 0 && static_cast<u_int>(iN) < uCap;
	}
}

bool ZM_PropModelRef(ZM_PROP_ID eId, char* szOut, u_int uCap)
{
	if (const char* szStem = ZM_PropEngineRockStem(eId))
	{
		// The .zmodel Tools/Zenith_Tools_RockAssetExport.cpp writes beside each
		// stone -- "what AddStep_LoadModel / a ModelComponent reference".
		return ZM_PropEngineRockRef(szStem, ".zmodel", szOut, uCap);
	}
	return ZM_PropAssetPath(eId, ZM_PROP_ASSET_MODEL, szOut, uCap);
}

bool ZM_PropMeshRef(ZM_PROP_ID eId, char* szOut, u_int uCap)
{
	if (const char* szStem = ZM_PropEngineRockStem(eId))
	{
		// The Zenith_MeshAsset export (.zasset) -- the same class ZM's own .zmesh
		// is, so ZM_ResolvePropFit measures it through the same ParseStream.
		return ZM_PropEngineRockRef(szStem, ".zasset", szOut, uCap);
	}
	return ZM_PropAssetPath(eId, ZM_PROP_ASSET_MESH, szOut, uCap);
}

// ============================================================================
// Recipe resolution.
// ============================================================================
ZM_PropRecipe ZM_ResolvePropRecipe(ZM_PROP_ID eId)
{
	const ZM_PropData& xData = ZM_GetPropData(eId);

	ZM_PropRecipe xR;
	xR.m_eId            = eId;
	// Family seed = name hash; distinct stems -> distinct synthetic seeds.
	xR.m_uSyntheticSeed = ZM_GenHashName(xData.m_szName);

	// Per-domain PCG seeds -- the SOLE randomness source for every builder. Props
	// have no evolution, so a fixed synthetic evo-stage feeds ZM_GenDeriveSeed.
	for (u_int d = 0; d < static_cast<u_int>(ZM_GEN_DOMAIN_COUNT); ++d)
	{
		xR.m_aulDomainSeed[d] = ZM_GenDeriveSeed(xR.m_uSyntheticSeed,
			static_cast<u_int>(eId), uZM_PROP_SYNTHETIC_EVO_STAGE, static_cast<ZM_GEN_DOMAIN>(d));
	}

	// Shape axes copied from the roster row (drive the composition + albedo).
	xR.m_eKind    = xData.m_eKind;
	xR.m_eBiome   = xData.m_eBiome;
	xR.m_ePalette = xData.m_ePalette;
	xR.m_fWidth   = xData.m_fWidth;
	xR.m_fDepth   = xData.m_fDepth;
	xR.m_fHeight  = xData.m_fHeight;

	return xR;
}

ZM_GenRNG ZM_MakeGenRNG(const ZM_PropRecipe& xR, ZM_GEN_DOMAIN eDomain)
{
	return ZM_GenRNG(xR.m_aulDomainSeed[eDomain]);
}

// ============================================================================
// Per-output builders.
// ============================================================================
namespace
{
	// The four light fixtures, by id. Each is built AROUND the light it houses:
	// the housed bulb's height above the fixture base is fixed by the placement
	// (ZM_InteriorDressing.h keeps every light where it was) and the shade is
	// placed to bracket it. Sizes are exact -- see the roster rows for why.
	void ZM_BuildFixtureMesh(const ZM_PropRecipe& xR, ZM_GenMesh& m)
	{
		const float fW = xR.m_fWidth, fD = xR.m_fDepth, fH = xR.m_fHeight;
		switch (xR.m_eId)
		{
		case ZM_PROP_PENDANT_LAMP:
			// Ceiling rose flush at the top, a short flex, and a stepped drum shade
			// whose bottom drum is where the bulb sits (0.04 m above the base).
			ZM_AppendPropBox(m, 0.0f, 0.0f, fH - 0.04f, 0.14f, 0.04f, 0.14f, ZM_PROP_UV_ACCENT);
			ZM_PGAppendChamferedPrism(m, ZM_PG_AXIS_Y, Zenith_Maths::Vector3(0.0f, 0.22f, 0.0f),
				fH - 0.04f - 0.22f, 0.006f, 0.006f, 0.006f, 0.006f, 0.002f,
				ZM_PropUVIsland(ZM_PROP_UV_ACCENT), false, false);
			ZM_AppendPropBox(m, 0.0f, 0.0f, 0.00f, fW,         0.08f, fD,         ZM_PROP_UV_GLOW);
			ZM_AppendPropBox(m, 0.0f, 0.0f, 0.08f, fW * 0.88f, 0.08f, fD * 0.88f, ZM_PROP_UV_GLOW);
			ZM_AppendPropBox(m, 0.0f, 0.0f, 0.16f, fW * 0.70f, 0.06f, fD * 0.70f, ZM_PROP_UV_GLOW);
			break;

		case ZM_PROP_BEDSIDE_LAMP:
			// A nightstand (carcass + top + a drawer front), then the lamp on it:
			// base disc, stem, and a tapered shade bracketing the 1.05 m bulb.
			ZM_AppendPropBox(m, 0.0f, 0.0f, 0.00f, fW,          0.58f, fD,          ZM_PROP_UV_PRIMARY);
			ZM_AppendPropBox(m, 0.0f, 0.0f, 0.58f, fW + 0.02f,  0.04f, fD + 0.02f,  ZM_PROP_UV_SECONDARY);
			ZM_AppendPropBox(m, 0.0f, -0.5f * fD - 0.006f, 0.34f, fW * 0.80f, 0.16f, 0.012f, ZM_PROP_UV_SECONDARY);
			ZM_AppendPropBox(m, 0.0f, 0.0f, 0.62f, 0.16f, 0.03f, 0.16f, ZM_PROP_UV_ACCENT);
			ZM_PGAppendChamferedPrism(m, ZM_PG_AXIS_Y, Zenith_Maths::Vector3(0.0f, 0.65f, 0.0f), 0.27f,
				0.012f, 0.012f, 0.010f, 0.010f, 0.004f, ZM_PropUVIsland(ZM_PROP_UV_ACCENT), false, false);
			ZM_AppendPropShade(m, 0.0f, 0.0f, 0.90f, fH - 0.90f, 0.13f, 0.10f);
			break;

		case ZM_PROP_FLOOR_LAMP:
			// A standard lamp: weighted base, tall stem, tapered shade bracketing
			// the 1.35 m bulb, and a finial.
			ZM_PGAppendChamferedPrism(m, ZM_PG_AXIS_Y, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), 0.03f,
				0.16f, 0.16f, 0.15f, 0.15f, 0.05f, ZM_PropUVIsland(ZM_PROP_UV_ACCENT), false, true);
			ZM_PGAppendChamferedPrism(m, ZM_PG_AXIS_Y, Zenith_Maths::Vector3(0.0f, 0.03f, 0.0f), 1.19f,
				0.015f, 0.015f, 0.013f, 0.013f, 0.005f, ZM_PropUVIsland(ZM_PROP_UV_ACCENT), false, false);
			ZM_AppendPropShade(m, 0.0f, 0.0f, 1.20f, 0.30f, 0.5f * fW - 0.01f, 0.15f);
			ZM_AppendPropBox(m, 0.0f, 0.0f, 1.50f, 0.04f, fH - 1.50f, 0.04f, ZM_PROP_UV_ACCENT);
			break;

		case ZM_PROP_LAB_BATTEN:
		default:
		{
			// Suspended: the diffuser at the very bottom (so the light sits IN it),
			// a steel housing over it, a dark end cap at each end, and two drop rods
			// carrying the whole thing up to the ceiling. The housing is 0.12 tall
			// and the rods take the rest of the roster height, so hanging the base
			// at (ceiling - height) lands the rods exactly on the soffit.
			constexpr float fHousingTop = 0.12f;
			ZM_AppendPropBox(m, 0.0f, 0.0f, 0.00f, fW - 0.06f, 0.04f, fD - 0.02f, ZM_PROP_UV_GLOW);
			ZM_AppendPropBox(m, 0.0f, 0.0f, 0.04f, fW, fHousingTop - 0.04f, fD, ZM_PROP_UV_PRIMARY);
			ZM_AppendPropBox(m, -0.5f * fW + 0.015f, 0.0f, 0.0f, 0.03f, fHousingTop, fD + 0.004f, ZM_PROP_UV_ACCENT);
			ZM_AppendPropBox(m,  0.5f * fW - 0.015f, 0.0f, 0.0f, 0.03f, fHousingTop, fD + 0.004f, ZM_PROP_UV_ACCENT);
			if (fH > fHousingTop)
			{
				const float fRod = fH - fHousingTop;
				ZM_AppendPropPost(m, -0.32f * fW, 0.0f, fHousingTop, fRod, 0.010f, 1.0f, 0.003f, ZM_PROP_UV_ACCENT);
				ZM_AppendPropPost(m,  0.32f * fW, 0.0f, fHousingTop, fRod, 0.010f, 1.0f, 0.003f, ZM_PROP_UV_ACCENT);
			}
			break;
		}
		}
	}
}

void ZM_BuildPropMesh(const ZM_PropRecipe& xR, ZM_GenMesh& xMesh)
{
	xMesh.Reset();

	// MESH-domain jitter: ALL draws up-front, in a FIXED order + count, BEFORE the
	// kind branch (so kind never changes the draw count) and drawing ONLY the MESH
	// domain RNG (mutating any other domain seed can never perturb the mesh bytes).
	ZM_GenRNG xRng = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_MESH);
	const float fWJit = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fHJit = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fDJit = xRng.NextFloatRange(-0.04f, 0.04f);
	float afAux[4];
	for (u_int i = 0; i < 4u; ++i) { afAux[i] = 0.70f + 0.30f * xRng.NextFloat01(); }

	// ★ THE FIXTURES ARE UNJITTERED. The draws above still happen (the draw count
	// is the determinism contract), but a fixture is built at exactly its roster
	// size so ZM_ComputePropFit answers the identity and its placement row's Y is
	// the model's base to the millimetre -- see the roster rows.
	const bool  bExact = ZM_PropIsFixture(xR);
	const float fW = xR.m_fWidth  * (bExact ? 1.0f : (1.0f + fWJit));
	const float fD = xR.m_fDepth  * (bExact ? 1.0f : (1.0f + fDJit));
	const float fH = xR.m_fHeight * (bExact ? 1.0f : (1.0f + fHJit));
	const float hW = 0.5f * fW, hD = 0.5f * fD;

	switch (xR.m_eKind)
	{
	case ZM_PROP_KIND_FENCE:
		// Three tapered, chamfered posts and two chamfered rails with a real
		// rectangular section (taller than wide), let into the posts.
		ZM_AppendPropPost(xMesh, -hW,  0.0f, 0.0f, fH, 0.060f, 0.85f, 0.012f, ZM_PROP_UV_PRIMARY);
		ZM_AppendPropPost(xMesh, 0.0f, 0.0f, 0.0f, fH, 0.060f, 0.85f, 0.012f, ZM_PROP_UV_PRIMARY);
		ZM_AppendPropPost(xMesh, +hW,  0.0f, 0.0f, fH, 0.060f, 0.85f, 0.012f, ZM_PROP_UV_PRIMARY);
		ZM_AppendPropRailX(xMesh, 0.0f, 0.30f * fH + 0.045f, 0.0f, fW, 0.030f, 0.045f, 0.008f, ZM_PROP_UV_SECONDARY);
		ZM_AppendPropRailX(xMesh, 0.0f, 0.70f * fH + 0.045f, 0.0f, fW, 0.030f, 0.045f, 0.008f, ZM_PROP_UV_SECONDARY);
		break;
	case ZM_PROP_KIND_SIGN:
		// A tapered post, the painted board, and a capping rail over the board.
		ZM_AppendPropPost(xMesh, 0.0f, 0.0f, 0.0f, fH, 0.070f, 0.80f, 0.015f, ZM_PROP_UV_PRIMARY);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.62f * fH,          fW,          0.30f * fH, 0.08f, ZM_PROP_UV_ACCENT);
		ZM_AppendPropRailX(xMesh, 0.0f, 0.92f * fH + 0.02f, 0.0f, fW + 0.06f, 0.055f, 0.020f, 0.006f, ZM_PROP_UV_SECONDARY);
		break;
	case ZM_PROP_KIND_LAMP:
		// A plinth, a tapered chamfered column, and the head.
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.0f, 0.30f, 0.15f, 0.30f, ZM_PROP_UV_SECONDARY);
		ZM_AppendPropPost(xMesh, 0.0f, 0.0f, 0.15f, fH - 0.35f - 0.15f, 0.060f, 0.75f, 0.015f, ZM_PROP_UV_PRIMARY);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, fH - 0.25f, 0.30f, 0.25f, 0.30f, ZM_PROP_UV_ACCENT);
		break;
	case ZM_PROP_KIND_BRIDGE:
		ZM_AppendPropBox(xMesh, 0.0f, +(hD - 0.10f), 0.0f, 0.20f * fW, 0.50f * fH, 0.15f, ZM_PROP_UV_PRIMARY);
		ZM_AppendPropBox(xMesh, 0.0f, -(hD - 0.10f), 0.0f, 0.20f * fW, 0.50f * fH, 0.15f, ZM_PROP_UV_PRIMARY);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.50f * fH, fW, 0.12f, fD, ZM_PROP_UV_SECONDARY);
		// Handrails: chamfered bars along the span.
		ZM_AppendPropRailZ(xMesh, +(hW - 0.06f), 0.50f * fH + 0.36f, 0.0f, fD, 0.04f, 0.04f, 0.012f, ZM_PROP_UV_ACCENT);
		ZM_AppendPropRailZ(xMesh, -(hW - 0.06f), 0.50f * fH + 0.36f, 0.0f, fD, 0.04f, 0.04f, 0.012f, ZM_PROP_UV_ACCENT);
		ZM_AppendPropPost(xMesh, +(hW - 0.06f), +(hD - 0.06f), 0.50f * fH + 0.12f, 0.24f, 0.04f, 1.0f, 0.010f, ZM_PROP_UV_ACCENT);
		ZM_AppendPropPost(xMesh, -(hW - 0.06f), +(hD - 0.06f), 0.50f * fH + 0.12f, 0.24f, 0.04f, 1.0f, 0.010f, ZM_PROP_UV_ACCENT);
		ZM_AppendPropPost(xMesh, +(hW - 0.06f), -(hD - 0.06f), 0.50f * fH + 0.12f, 0.24f, 0.04f, 1.0f, 0.010f, ZM_PROP_UV_ACCENT);
		ZM_AppendPropPost(xMesh, -(hW - 0.06f), -(hD - 0.06f), 0.50f * fH + 0.12f, 0.24f, 0.04f, 1.0f, 0.010f, ZM_PROP_UV_ACCENT);
		break;
	case ZM_PROP_KIND_LEDGE:
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f,          0.0f,       fW, 0.60f * fH, fD,    ZM_PROP_UV_PRIMARY);
		ZM_AppendPropBox(xMesh, 0.0f, +(hD - 0.15f), 0.60f * fH, fW, 0.40f * fH, 0.30f, ZM_PROP_UV_SECONDARY);
		break;
	case ZM_PROP_KIND_ROCK:
		// ★ NOTHING PRESENTS THIS. The rock rows resolve to the shared engine
		// stones (ZM_PropModelRef); the box keeps the baked bundle total so the
		// family manifest stays honest, and nothing else.
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.0f, fW, fH, fD, ZM_PROP_UV_PRIMARY);
		break;
	case ZM_PROP_KIND_FURNITURE:
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.0f,       fW,         0.60f * fH, fD,         ZM_PROP_UV_PRIMARY);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.60f * fH, 0.90f * fW, 0.40f * fH, 0.90f * fD, ZM_PROP_UV_SECONDARY);
		break;

	// ---- The two CENTRE-ANCHORED kinds -------------------------------------
	//
	// ★ THESE TWO START AT fZM_PROP_ITEM_BASE_Y, NOT AT ZERO, and the header says
	// why in full: their entity's authored origin is the CENTRE of a unit volume,
	// so -0.5 is the ground and 0.0 would hover. The stack below sums to exactly
	// 1.0 * fH, so the top lands at -0.5 + fH and a row's height is still literally
	// how tall the thing is.
	case ZM_PROP_KIND_ITEM_PICKUP:
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, fZM_PROP_ITEM_BASE_Y,              fW,         0.14f * fH, fD,         ZM_PROP_UV_PRIMARY);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, fZM_PROP_ITEM_BASE_Y + 0.14f * fH, 0.62f * fW, 0.62f * fH, 0.62f * fD, ZM_PROP_UV_ACCENT);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, fZM_PROP_ITEM_BASE_Y + 0.76f * fH, 0.30f * fW, 0.24f * fH, 0.30f * fD, ZM_PROP_UV_SECONDARY);
		break;
	case ZM_PROP_KIND_ITEM_SPENT:
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, fZM_PROP_ITEM_BASE_Y, fW, 0.45f * fH, fD, ZM_PROP_UV_PRIMARY);
		ZM_AppendPropBox(xMesh, 0.0f, +(hD - 0.06f * fD), fZM_PROP_ITEM_BASE_Y, fW,         fH, 0.12f * fD, ZM_PROP_UV_SECONDARY);
		ZM_AppendPropBox(xMesh, 0.0f, -(hD - 0.06f * fD), fZM_PROP_ITEM_BASE_Y, fW,         fH, 0.12f * fD, ZM_PROP_UV_SECONDARY);
		ZM_AppendPropBox(xMesh, +(hW - 0.06f * fW), 0.0f, fZM_PROP_ITEM_BASE_Y, 0.12f * fW, fH, fD,         ZM_PROP_UV_SECONDARY);
		ZM_AppendPropBox(xMesh, -(hW - 0.06f * fW), 0.0f, fZM_PROP_ITEM_BASE_Y, 0.12f * fW, fH, fD,         ZM_PROP_UV_SECONDARY);
		break;

	case ZM_PROP_KIND_LIGHT_FIXTURE:
		ZM_BuildFixtureMesh(xR, xMesh);
		break;

	case ZM_PROP_KIND_DRESSING:
	default:
	{
		const float axCX[4] = { -0.25f * fW, +0.25f * fW, -0.25f * fW, +0.25f * fW };
		const float axCZ[4] = { -0.25f * fD, -0.25f * fD, +0.25f * fD, +0.25f * fD };
		for (u_int i = 0; i < 4u; ++i)
		{
			ZM_AppendPropBox(xMesh, axCX[i], axCZ[i], 0.0f, 0.40f * fW, afAux[i] * fH, 0.40f * fD,
				(i & 1u) ? ZM_PROP_UV_SECONDARY : ZM_PROP_UV_PRIMARY);
		}
		break;
	}
	}

	// Finalise tangents (needs normals + UVs; static, so NO skin normaliser, NO bones).
	// The emitters already wrote hard per-face normals -- do NOT re-run
	// ZM_GenGenerateNormals (it would weld + smooth the hard corners).
	ZM_GenGenerateTangents(xMesh);
}

namespace
{
	// The per-palette surface relief at one texel, in [-0.5, 0.5]. Shared by the
	// height field and (scaled down) by the albedo's grain modulation so the two
	// agree about where the grain is. Pure function of (u, v, salt).
	float ZM_PropPaletteRelief(ZM_PROP_PALETTE ePalette, float fU, float fV, u_int uSalt)
	{
		switch (ePalette)
		{
		case ZM_PROP_PALETTE_WOOD:
			// Grain: stretched hard along U so it runs the length of a member,
			// plus a coarse ring lattice for the growth rings.
			return (ZM_SynthValueNoise(fU * 0.20f, fV * 10.0f, 16u, uSalt) - 0.5f) * 0.34f
				 + (ZM_SynthValueNoise(fU * 0.08f, fV * 3.0f,  8u, uSalt + 31u) - 0.5f) * 0.16f;
		case ZM_PROP_PALETTE_STONE:
			// Isotropic pitting: two octaves, no direction.
			return (ZM_SynthFbm(fU, fV, 12u, uSalt) - 0.5f) * 0.44f;
		case ZM_PROP_PALETTE_METAL:
			// Brushed: very fine, very directional, and shallow. Metal relief is
			// almost all in the roughness rather than in the normal.
			return (ZM_SynthValueNoise(fU * 24.0f, fV * 0.15f, 24u, uSalt) - 0.5f) * 0.18f;
		case ZM_PROP_PALETTE_PAINTED:
			// A skim of orange peel, and little else -- paint hides the substrate.
			return (ZM_SynthFbm(fU, fV, 20u, uSalt) - 0.5f) * 0.12f;
		case ZM_PROP_PALETTE_FOLIAGE:
		default:
			// Leafy break-up: mid-frequency clumps.
			return (ZM_SynthFbm(fU, fV, 7u, uSalt) - 0.5f) * 0.40f;
		}
	}

	// The texel's coordinate INSIDE its quadrant, in [0,1) -- so each role's
	// pattern is continuous across its own island and independent of the others.
	void ZM_PropQuadrantLocal(float fU, float fV, float& fLU, float& fLV)
	{
		fLU = fU < 0.5f ? fU * 2.0f : (fU - 0.5f) * 2.0f;
		fLV = fV < 0.5f ? fV * 2.0f : (fV - 0.5f) * 2.0f;
	}
}

ZM_GenImage ZM_BuildPropTexture(const ZM_PropRecipe& xR)
{
	ZM_GenImage xImg(uZM_PROP_ALBEDO_RESOLUTION, uZM_PROP_ALBEDO_RESOLUTION);

	// ALBEDO is the SOLE randomness source (a MESH-seed mutation can never perturb the
	// texture). ALL draws up-front, FIXED count + order, BEFORE any palette/biome branch.
	ZM_GenRNG xRng = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_ALBEDO);
	const float fJitR   = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fJitG   = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fJitB   = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fAccJit = xRng.NextFloatRange(-0.04f, 0.04f);

	Zenith_Maths::Vector3 xBase = ZM_PropPaletteColour(xR.m_ePalette);
	if (xR.m_eBiome != ZM_PROP_BIOME_NONE)
	{
		xBase = ZM_LerpV3(xBase, ZM_PropBiomeColour(xR.m_eBiome), 0.45f);
	}
	xBase = ZM_ClampV3(Zenith_Maths::Vector3(xBase.x + fJitR, xBase.y + fJitG, xBase.z + fJitB));

	// The four role colours. SECONDARY is the same material a shade lighter (a
	// rail's sawn face, a top's end grain); ACCENT is the old accent tone for
	// scenery and BRASS / dark trim for a fixture; GLOW is a lit shade's fabric,
	// warm for the domestic fixtures and cool for the batten -- and just the base
	// again on anything that is not a fixture, since nothing meshes into it.
	const bool bFixture = ZM_PropIsFixture(xR);
	const bool bCool    = (xR.m_eId == ZM_PROP_LAB_BATTEN);
	const Zenith_Maths::Vector3 xSecondary = ZM_ClampV3(ZM_ScaleV3(xBase, 1.10f));
	const Zenith_Maths::Vector3 xAccent = bFixture
		? (bCool ? Zenith_Maths::Vector3(0.18f, 0.19f, 0.21f) : Zenith_Maths::Vector3(0.62f, 0.46f, 0.22f))
		: ZM_ClampV3(Zenith_Maths::Vector3(xBase.x * 0.70f + fAccJit, xBase.y * 0.70f + fAccJit, xBase.z * 0.70f + fAccJit));
	const Zenith_Maths::Vector3 xGlow = bFixture
		? (bCool ? Zenith_Maths::Vector3(0.90f, 0.94f, 1.00f) : Zenith_Maths::Vector3(0.95f, 0.88f, 0.74f))
		: xBase;

	const u_int uRes  = xImg.GetWidth();
	const u_int uSalt = xR.m_uSyntheticSeed;
	const float fInv  = 1.0f / static_cast<float>(uRes);
	for (u_int uY = 0u; uY < uRes; ++uY)
	{
		const float fV = (static_cast<float>(uY) + 0.5f) * fInv;
		for (u_int uX = 0u; uX < uRes; ++uX)
		{
			const float fU = (static_cast<float>(uX) + 0.5f) * fInv;
			const ZM_PROP_UV_ROLE eRole = ZM_PropUVRoleAt(fU, fV);
			float fLU, fLV;
			ZM_PropQuadrantLocal(fU, fV, fLU, fLV);

			Zenith_Maths::Vector3 xCol;
			switch (eRole)
			{
			case ZM_PROP_UV_SECONDARY:
				// The same grain, a different salt, so a rail is not a copy of the post.
				xCol = ZM_ScaleV3(xSecondary, 1.0f + ZM_PropPaletteRelief(xR.m_ePalette, fLU, fLV, uSalt + 97u) * 0.45f);
				break;
			case ZM_PROP_UV_ACCENT:
				// Paint over the relief: much less colour modulation.
				xCol = ZM_ScaleV3(xAccent, 1.0f + ZM_PropPaletteRelief(ZM_PROP_PALETTE_PAINTED, fLU, fLV, uSalt + 193u) * 0.6f);
				break;
			case ZM_PROP_UV_GLOW:
				// Fabric: a faint vertical pleat, nothing else -- the glow does the work.
				xCol = ZM_ScaleV3(xGlow, 1.0f + (ZM_SynthValueNoise(fLU * 40.0f, fLV * 0.2f, 40u, uSalt + 271u) - 0.5f) * 0.10f);
				break;
			case ZM_PROP_UV_PRIMARY:
			default:
				xCol = ZM_ScaleV3(xBase, 1.0f + ZM_PropPaletteRelief(xR.m_ePalette, fLU, fLV, uSalt) * 0.45f);
				break;
			}
			xCol = ZM_ClampV3(xCol);
			xImg.Set(uY, uX, Zenith_Maths::Vector4(xCol.x, xCol.y, xCol.z, 1.0f));
		}
	}
	return xImg;
}

// ============================================================================
// The per-palette height field and material response.
//
// ★ THE HEIGHT FIELD IS THE ONE INPUT, and all three derived maps come from it
// (ZM_SynthBuildPbrSet). Deriving them separately is how a normal map and an AO
// map end up disagreeing about where the surface is, which reads as dirt that
// does not line up with the relief.
// ============================================================================
ZM_GenImage ZM_BuildPropHeight(const ZM_PropRecipe& xR)
{
	const u_int uRes = uZM_PROP_ALBEDO_RESOLUTION;
	const u_int uSalt = xR.m_uSyntheticSeed;

	ZM_GenImage xImg(uRes, uRes);
	const float fInv = 1.0f / static_cast<float>(uRes);
	for (u_int uY = 0u; uY < uRes; ++uY)
	{
		const float fV = (static_cast<float>(uY) + 0.5f) * fInv;
		for (u_int uX = 0u; uX < uRes; ++uX)
		{
			const float fU = (static_cast<float>(uX) + 0.5f) * fInv;
			float fLU, fLV;
			ZM_PropQuadrantLocal(fU, fV, fLU, fLV);

			float fH = 0.5f;
			switch (ZM_PropUVRoleAt(fU, fV))
			{
			case ZM_PROP_UV_SECONDARY:
				fH += ZM_PropPaletteRelief(xR.m_ePalette, fLU, fLV, uSalt + 97u);
				break;
			case ZM_PROP_UV_ACCENT:
				fH += ZM_PropPaletteRelief(ZM_PROP_PALETTE_PAINTED, fLU, fLV, uSalt + 193u);
				break;
			case ZM_PROP_UV_GLOW:
				fH += (ZM_SynthValueNoise(fLU * 40.0f, fLV * 0.2f, 40u, uSalt + 271u) - 0.5f) * 0.06f;
				break;
			case ZM_PROP_UV_PRIMARY:
			default:
				fH += ZM_PropPaletteRelief(xR.m_ePalette, fLU, fLV, uSalt);
				break;
			}

			const float fC = fH < 0.0f ? 0.0f : (fH > 1.0f ? 1.0f : fH);
			xImg.Set(uY, uX, Zenith_Maths::Vector4(fC, fC, fC, 1.0f));
		}
	}
	return xImg;
}

ZM_GenImage ZM_BuildPropEmissive(const ZM_PropRecipe& xR)
{
	// Zero-filled RGB, alpha 1 -- already the inert mask for every non-fixture.
	ZM_GenImage xImg(uZM_PROP_EMISSIVE_RESOLUTION, uZM_PROP_EMISSIVE_RESOLUTION);
	if (!ZM_PropIsFixture(xR))
	{
		return xImg;
	}

	// White inside the GLOW quadrant, inset by ONE gutter so a bilinear tap at
	// the island edge (the mesh island is inset by the same gutter) still reads
	// full white and a tap just outside the quadrant reads black.
	const ZM_GenUVIsland xGlow = ZM_PropUVPaintRect(ZM_PROP_UV_GLOW);
	const float fU0 = xGlow.m_fU0 + fZM_PROP_UV_GUTTER * 0.5f;
	const float fV0 = xGlow.m_fV0 + fZM_PROP_UV_GUTTER * 0.5f;
	const u_int uRes = xImg.GetWidth();
	const float fInv = 1.0f / static_cast<float>(uRes);
	for (u_int uY = 0u; uY < uRes; ++uY)
	{
		const float fV = (static_cast<float>(uY) + 0.5f) * fInv;
		for (u_int uX = 0u; uX < uRes; ++uX)
		{
			const float fU = (static_cast<float>(uX) + 0.5f) * fInv;
			const bool bLit = (fU >= fU0) && (fV >= fV0);
			const float fL = bLit ? 1.0f : 0.0f;
			xImg.Set(uY, uX, Zenith_Maths::Vector4(fL, fL, fL, 1.0f));
		}
	}
	return xImg;
}

ZM_SynthPbrResponse ZM_PropPbrResponse(ZM_PROP_PALETTE ePalette)
{
	ZM_SynthPbrResponse xR;
	switch (ePalette)
	{
	case ZM_PROP_PALETTE_WOOD:
		xR.m_fRoughness = 0.72f; xR.m_fNormalStrength = 1.05f;
		break;
	case ZM_PROP_PALETTE_STONE:
		xR.m_fRoughness = 0.90f; xR.m_fNormalStrength = 1.25f;
		break;
	case ZM_PROP_PALETTE_METAL:
		// ★ THE ONLY ARM THAT IS ACTUALLY METAL, and the only place metallic=1 is
		// right. A lamp post is a conductor: its diffuse should vanish and its
		// reflection should take the base colour's tint. Everywhere else -- wood,
		// stone, paint, leaves, glass, plaster -- metallic=1 would kill the diffuse
		// and tint the reflection by a colour that is not a conductor's.
		xR.m_fRoughness = 0.34f; xR.m_fMetallic = 1.0f; xR.m_fNormalStrength = 0.55f;
		break;
	case ZM_PROP_PALETTE_PAINTED:
		xR.m_fRoughness = 0.44f; xR.m_fNormalStrength = 0.60f;
		break;
	case ZM_PROP_PALETTE_FOLIAGE:
	default:
		xR.m_fRoughness = 0.80f; xR.m_fNormalStrength = 0.90f;
		break;
	}
	return xR;
}

void ZM_BuildProp(ZM_PROP_ID eId, ZM_Prop& xOut)
{
	const ZM_PropRecipe xR = ZM_ResolvePropRecipe(eId);

	xOut.m_eId = eId;
	ZM_BuildPropMesh(xR, xOut.m_xMesh);
	xOut.m_xTexture = ZM_BuildPropTexture(xR);

	// FIXED ORDER: albedo, then the height field it does NOT depend on, then the
	// three maps derived from that height, then the emissive mask (which draws
	// nothing). Part of the determinism contract.
	ZM_SynthPbrResponse xResponse = ZM_PropPbrResponse(xR.m_ePalette);
	// One roughness jitter per prop, from the ALBEDO domain -- the same domain the
	// texture draws from, because it is a surface-finish property, not a shape one.
	ZM_GenRNG xRng = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_ALBEDO);
	xResponse.m_fRoughnessJitter = xRng.NextFloatRange(-0.05f, 0.05f);
	xOut.m_xPbr = ZM_SynthBuildPbrSet(ZM_BuildPropHeight(xR), xResponse);
	xOut.m_xEmissive = ZM_BuildPropEmissive(xR);
}

// ============================================================================
// Determinism helpers.
// ============================================================================
bool ZM_PropMeshEqual(const ZM_GenMesh& xA, const ZM_GenMesh& xB)
{
	return ZM_BufferBytesEqual(xA.m_xPositions,   xB.m_xPositions)
		&& ZM_BufferBytesEqual(xA.m_xNormals,     xB.m_xNormals)
		&& ZM_BufferBytesEqual(xA.m_xUVs,         xB.m_xUVs)
		&& ZM_BufferBytesEqual(xA.m_xTangents,    xB.m_xTangents)
		&& ZM_BufferBytesEqual(xA.m_xColors,      xB.m_xColors)
		&& ZM_BufferBytesEqual(xA.m_xIndices,     xB.m_xIndices)
		&& ZM_BufferBytesEqual(xA.m_xBoneIndices, xB.m_xBoneIndices)   // both empty (static)
		&& ZM_BufferBytesEqual(xA.m_xBoneWeights, xB.m_xBoneWeights)   // both empty (static)
		&& ZM_BufferBytesEqual(xA.m_xBones,       xB.m_xBones);        // both empty (static)
}

bool ZM_PropBuildEqual(const ZM_Prop& xA, const ZM_Prop& xB)
{
	return ZM_PropMeshEqual(xA.m_xMesh, xB.m_xMesh)
		&& xA.m_xTexture.Equals(xB.m_xTexture)
		&& xA.m_xEmissive.Equals(xB.m_xEmissive);
}

u_int ZM_PropContentHash(const ZM_Prop& xProp)
{
	const ZM_GenMesh& xMesh = xProp.m_xMesh;
	u_int uHash = uZM_FNV_OFFSET;
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xPositions);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xNormals);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xUVs);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xTangents);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xColors);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xIndices);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xBoneIndices);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xBoneWeights);
	uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xBones);

	// Fold the texture content hashes (already FNV over packed texels).
	uHash = ZM_GenHashCombine(uHash, xProp.m_xTexture.ContentHash());
	uHash = ZM_GenHashCombine(uHash, xProp.m_xEmissive.ContentHash());
	return uHash;
}

// ============================================================================
// Validation.
// ============================================================================
ZM_PropValidation ZM_ValidateProp(const ZM_Prop& xProp)
{
	ZM_PropValidation xV;
	xV.m_xMesh              = ZM_ValidateGenMeshStatic(xProp.m_xMesh);
	xV.m_bTextureNonEmpty   = !xProp.m_xTexture.IsEmpty();
	xV.m_bEmissiveNonEmpty  = !xProp.m_xEmissive.IsEmpty();
	xV.m_bAllValid          = xV.m_xMesh.m_bAllValid && xV.m_bTextureNonEmpty
		&& xV.m_bEmissiveNonEmpty;
	return xV;
}

// ============================================================================
// Asset-path scheme.
// ============================================================================
bool ZM_PropAssetPath(ZM_PROP_ID eId, ZM_PROP_ASSET_KIND eKind, char* szOut, u_int uCap)
{
	Zenith_Assert(szOut != nullptr, "ZM_PropAssetPath: null output buffer");
	if (szOut == nullptr || uCap == 0u)
	{
		return false;
	}
	szOut[0] = '\0';

	const char* szName = ZM_GetPropName(eId);
	char acBase[128];
	const int iBase = snprintf(acBase, sizeof(acBase), ZM_PropBasenameFmt(eKind), szName);
	if (iBase < 0 || static_cast<u_int>(iBase) >= sizeof(acBase))
	{
		return false;   // internal basename overflowed acBase -- overflow contract is TOTAL
	}

	const int iN = snprintf(szOut, uCap, "game:Props/%s/%s", szName, acBase);
	return iN >= 0 && static_cast<u_int>(iN) < uCap;   // false on truncation/overflow
}

// ============================================================================
// Disk bake (TOOLS ONLY) -- SC5. Writes one static prop bundle: the .zmesh (via
// the skeleton-less ZM_GenBakeStaticMesh bridge), the five .ztxtr maps, the
// .zmtrl, and the .zmodel -- which binds NO skeleton and lists NO animations
// (props are static). The mesh bake creates the Props/<Name>/ folder FIRST
// (SaveToFile + model Export create no directories), so the material + model
// writes that follow land in it.
// ============================================================================
#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Collections/Zenith_Vector.h"
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"    // per-family bake guard (check) + stamp (write)
#include <filesystem>
#include <string>

bool ZM_BakeProp(ZM_PROP_ID eId)
{
	ZM_Prop xProp;
	ZM_BuildProp(eId, xProp);

	char acMeshRef[512], acAlbedoRef[512], acMatRef[512], acModelRef[512];
	char acNormalRef[512], acRmRef[512], acAoRef[512], acEmRef[512];
	bool bOk = true;
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_MESH,        acMeshRef,   sizeof(acMeshRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_ALBEDO,      acAlbedoRef, sizeof(acAlbedoRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_NORMAL,      acNormalRef, sizeof(acNormalRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_ROUGH_METAL, acRmRef,     sizeof(acRmRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_OCCLUSION,   acAoRef,     sizeof(acAoRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_EMISSIVE,    acEmRef,     sizeof(acEmRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_MATERIAL,    acMatRef,    sizeof(acMatRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_MODEL,       acModelRef,  sizeof(acModelRef));
	if (!bOk)
	{
		return false;   // a path overflowed; do not bake a partial bundle
	}

	const std::string strMeshFs   = Zenith_AssetRegistry::ResolvePath(std::string(acMeshRef));
	const std::string strAlbedoFs = Zenith_AssetRegistry::ResolvePath(std::string(acAlbedoRef));
	const std::string strNormalFs = Zenith_AssetRegistry::ResolvePath(std::string(acNormalRef));
	const std::string strRmFs     = Zenith_AssetRegistry::ResolvePath(std::string(acRmRef));
	const std::string strAoFs     = Zenith_AssetRegistry::ResolvePath(std::string(acAoRef));
	const std::string strEmFs     = Zenith_AssetRegistry::ResolvePath(std::string(acEmRef));
	const std::string strMatFs    = Zenith_AssetRegistry::ResolvePath(std::string(acMatRef));
	const std::string strModelFs  = Zenith_AssetRegistry::ResolvePath(std::string(acModelRef));

	// Static mesh (.zmesh) -- NO skeleton, NO skin. Albedo (.ztxtr, BC1).
	bOk &= ZM_GenBakeStaticMesh(xProp.m_xMesh, strMeshFs.c_str());
	bOk &= ZM_SynthBakeAlbedoBC1(xProp.m_xTexture, strAlbedoFs.c_str());
	// ALBEDO is colour (sRGB baked into BC1); the other four are DATA and must
	// stay LINEAR or the shader reads a gamma-curved roughness -- or, for the
	// emissive mask, a gamma-curved gutter.
	bOk &= ZM_SynthBakeNormalBC5(xProp.m_xPbr.m_xNormal,            strNormalFs.c_str());
	bOk &= ZM_SynthBakeLinearBC1(xProp.m_xPbr.m_xRoughnessMetallic, strRmFs.c_str());
	bOk &= ZM_SynthBakeLinearBC1(xProp.m_xPbr.m_xOcclusion,         strAoFs.c_str());
	bOk &= ZM_SynthBakeLinearBC1(xProp.m_xEmissive,                 strEmFs.c_str());

	const std::string strName = ZM_GetPropName(eId);

	// Material (.zmtrl v5): baked albedo in the BASE_COLOR slot, the data maps in
	// theirs, and -- on a fixture -- the emissive mask times the housed light's
	// colour at an HDR intensity. Create<>()+GetDirect() keeps the asset alive
	// across SaveToFile (never a stack object). Every texture is passed as a
	// "game:" ref (stored as a path, NOT loaded now).
	{
		Zenith_AssetHandle<Zenith_MaterialAsset> xMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxMat = xMat.GetDirect();
		pxMat->SetName(strName);
		const ZM_SynthPbrResponse xResp = ZM_PropPbrResponse(ZM_GetPropData(eId).m_ePalette);
		pxMat->SetDiffuseTexture(TextureHandle(std::string(acAlbedoRef)));
		pxMat->SetNormalTexture(TextureHandle(std::string(acNormalRef)));
		pxMat->SetRoughnessMetallicTexture(TextureHandle(std::string(acRmRef)));
		pxMat->SetOcclusionTexture(TextureHandle(std::string(acAoRef)));
		// The scalars are what the maps MODULATE, and what a cold boot with an
		// absent bake falls back to.
		pxMat->SetRoughness(xResp.m_fRoughness);
		pxMat->SetMetallic(xResp.m_fMetallic);
		pxMat->SetNormalStrength(xResp.m_fNormalStrength);

		const ZM_PropEmissive xEm = ZM_GetPropEmissive(eId);
		if (xEm.m_fIntensity > 0.0f)
		{
			// The mask is what keeps the glow on the shade: the engine's default
			// emissive texture is WHITE, so a colour + intensity with no mask would
			// light the whole fixture, body and all.
			pxMat->SetEmissiveTexture(TextureHandle(std::string(acEmRef)));
			pxMat->SetEmissiveColor(xEm.m_xColour);
			pxMat->SetEmissiveIntensity(xEm.m_fIntensity);
		}
		pxMat->SaveToFile(strMatFs);
	}

	// Model (.zmodel v2): the single-submesh static mesh + exactly ONE material.
	// STATIC: NO SetSkeletonPath, NO AddAnimationPath -> HasSkeleton()==false and
	// GetNumAnimations()==0 on the baked model.
	{
		Zenith_AssetHandle<Zenith_ModelAsset> xModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		Zenith_ModelAsset* pxModel = xModel.GetDirect();
		pxModel->SetName(strName);
		Zenith_Vector<std::string> xMats;
		xMats.PushBack(std::string(acMatRef));
		pxModel->AddMeshByPath(std::string(acMeshRef), xMats);
		pxModel->Export(strModelFs.c_str());   // STATIC: NO SetSkeletonPath, NO AddAnimationPath
	}

	// SaveToFile always returns true and Export is void, so exists() is the real IO
	// signal (mirrors ZM_BakeHuman): AND both new artifacts into bOk.
	std::error_code xEc;
	bOk &= std::filesystem::exists(std::filesystem::path(strMatFs),   xEc);
	bOk &= std::filesystem::exists(std::filesystem::path(strModelFs), xEc);
	return bOk;
}

bool ZM_BakeAllProps()
{
	// Per-family bake guard: skip when the stamp is current + every file present.
	const std::filesystem::path xRoot(GAME_ASSETS_DIR);
	if (ZM_BakeManifestCheck(ZM_ASSET_FAMILY_PROPS, xRoot))
	{
		return true;   // warm: stamp current + all files present -> skip the family
	}
	bool bOk = true;
	const u_int uCount = static_cast<u_int>(ZM_PROP_COUNT);
	for (u_int u = 0; u < uCount; ++u)
	{
		bOk &= ZM_BakeProp(static_cast<ZM_PROP_ID>(u));
	}
	if (bOk)
	{
		bOk &= ZM_WriteBakeManifest(ZM_ASSET_FAMILY_PROPS, xRoot);   // stamp only after a fully-successful bake
	}
	return bOk;
}

namespace
{
	// Are all of ONE prop's per-model files on disk and non-empty? The same
	// existence-AND-size pair ZM_BakeManifestCheck uses per file: a zero-byte file is
	// what a bake interrupted mid-write leaves, and it loads as a missing model while
	// existing as a present one.
	bool ZM_PropBundlePresent(ZM_PROP_ID eId)
	{
		for (u_int k = 0; k < static_cast<u_int>(ZM_PROP_ASSET_KIND_COUNT); ++k)
		{
			char acRef[512];
			if (!ZM_PropAssetPath(eId, static_cast<ZM_PROP_ASSET_KIND>(k), acRef, sizeof(acRef)))
			{
				return false;
			}
			const std::filesystem::path xPath(
				Zenith_AssetRegistry::ResolvePath(std::string(acRef)));

			std::error_code xEc;
			if (!std::filesystem::is_regular_file(xPath, xEc) || xEc)
			{
				return false;
			}
			if (std::filesystem::file_size(xPath, xEc) == 0u || xEc)
			{
				return false;
			}
		}
		return true;
	}
}

bool ZM_EnsurePropBaked(ZM_PROP_ID eId)
{
	// TOTAL on a garbage id, like every other entry point here: ZM_GetPropName would
	// answer "NONE" and we would stat game:Props/NONE/... forever.
	if (static_cast<u_int>(eId) >= static_cast<u_int>(ZM_PROP_COUNT))
	{
		return false;
	}
	if (ZM_PropBundlePresent(eId))
	{
		return true;   // warm -- no build, no writes
	}
	if (!ZM_BakeProp(eId))
	{
		return false;
	}
	// RE-ASKED rather than trusting the bake's own bool. ZM_BakeProp already ANDs
	// exists() for the two artifacts whose writers report nothing, but the mesh and
	// texture writers report their own status and this is the one check that covers
	// every file the same way.
	return ZM_PropBundlePresent(eId);
}
#endif   // ZENITH_TOOLS
