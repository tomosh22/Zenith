#include "Zenith.h"

// ============================================================================
// ZM_BuildingGen -- the S4 building generator driver. See the header for the
// architecture + determinism contract. This TU owns: building -> recipe
// resolution, the per-domain seed derivation, the SC2 parametric shell mesh
// builder (body box + roof-by-kind + MESH-domain jitter), the flat-facade builder,
// the full-bundle driver, the byte-identity + hash + validation machinery, the
// asset-path scheme, and (tools only, SC5) the disk bake STUBS. The SC3 facade
// decals land later.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_BuildingGen.h"

#include <cstdio>    // snprintf
#include <cstring>   // memcmp

namespace
{
	// FNV-1a constants (byte-identical to ZM_GenHashName / the human content hash).
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

	// Per-palette solid facade base colour (SC1 flat fill; distinct per palette so
	// different-palette facades never collide). SC3 replaces this with real decals.
	Zenith_Maths::Vector3 ZM_BuildingPaletteColour(ZM_BUILDING_PALETTE ePalette)
	{
		switch (ePalette)
		{
		case ZM_BUILDING_PALETTE_WARM:  return Zenith_Maths::Vector3(0.82f, 0.56f, 0.42f);
		case ZM_BUILDING_PALETTE_COOL:  return Zenith_Maths::Vector3(0.52f, 0.60f, 0.72f);
		case ZM_BUILDING_PALETTE_EARTH: return Zenith_Maths::Vector3(0.50f, 0.46f, 0.34f);
		default:
			Zenith_Assert(false, "ZM_BuildingPaletteColour: bad palette %u", (u_int)ePalette);
			return Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f);
		}
	}

	// Per-kind per-model file basename pattern (embeds the building name).
	const char* ZM_BuildingBasenameFmt(ZM_BUILDING_ASSET_KIND eKind)
	{
		switch (eKind)
		{
		case ZM_BUILDING_ASSET_MESH:     return "%s.zmesh";
		case ZM_BUILDING_ASSET_FACADE:   return "%s_facade.ztxtr";
		case ZM_BUILDING_ASSET_MATERIAL: return "%s.zmtrl";
		case ZM_BUILDING_ASSET_MODEL:    return "%s.zmodel";
		default:
			Zenith_Assert(false, "ZM_BuildingBasenameFmt: bad kind %u", (u_int)eKind);
			return "%s.bin";
		}
	}
}

// ============================================================================
// Recipe resolution.
// ============================================================================
ZM_BuildingRecipe ZM_ResolveBuildingRecipe(ZM_BUILDING_ID eId)
{
	const ZM_BuildingData& xData = ZM_GetBuildingData(eId);

	ZM_BuildingRecipe xR;
	xR.m_eId            = eId;
	// Family seed = name hash; distinct stems -> distinct synthetic seeds.
	xR.m_uSyntheticSeed = ZM_GenHashName(xData.m_szName);

	// Per-domain PCG seeds -- the SOLE randomness source for every builder. Buildings
	// have no evolution, so a fixed synthetic evo-stage feeds ZM_GenDeriveSeed.
	for (u_int d = 0; d < static_cast<u_int>(ZM_GEN_DOMAIN_COUNT); ++d)
	{
		xR.m_aulDomainSeed[d] = ZM_GenDeriveSeed(xR.m_uSyntheticSeed,
			static_cast<u_int>(eId), uZM_BUILDING_SYNTHETIC_EVO_STAGE, static_cast<ZM_GEN_DOMAIN>(d));
	}

	// Shape axes copied from the roster row (drive the shell + facade). m_fStoreyHeight
	// keeps the recipe default (the roster carries no per-row storey height).
	xR.m_eStyle       = xData.m_eStyle;
	xR.m_ePalette     = xData.m_ePalette;
	xR.m_eRoof        = xData.m_eRoof;
	xR.m_fWidth       = xData.m_fWidth;
	xR.m_fDepth       = xData.m_fDepth;
	xR.m_uStoreys     = xData.m_uStoreys;
	xR.m_uWindowCols  = xData.m_uWindowCols;
	xR.m_uWindowRows  = xData.m_uWindowRows;
	xR.m_eThemeType   = xData.m_eThemeType;

	return xR;
}

ZM_GenRNG ZM_MakeGenRNG(const ZM_BuildingRecipe& xR, ZM_GEN_DOMAIN eDomain)
{
	return ZM_GenRNG(xR.m_aulDomainSeed[eDomain]);
}

// ============================================================================
// Per-output builders.
// ============================================================================
void ZM_BuildBuildingMesh(const ZM_BuildingRecipe& xR, ZM_GenMesh& xMesh)
{
	xMesh.Reset();

	// MESH-domain jitter: ALL draws up-front, in a FIXED order, BEFORE the roof-kind
	// branch (so kind never changes the draw count) and drawing ONLY the MESH domain
	// RNG (mutating any other domain seed can never perturb the mesh bytes).
	ZM_GenRNG xRng = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_MESH);
	const float fWJit     = xRng.NextFloatRange(-0.03f, 0.03f);
	const float fDJit     = xRng.NextFloatRange(-0.03f, 0.03f);
	const float fHJit     = xRng.NextFloatRange(-0.03f, 0.03f);
	const float fPitchJit = xRng.NextFloatRange(-0.10f, 0.10f);   // always drawn (even FLAT)

	const float fWidth   = xR.m_fWidth        * (1.0f + fWJit);
	const float fDepth   = xR.m_fDepth        * (1.0f + fDJit);
	const float fStoreyH = xR.m_fStoreyHeight * (1.0f + fHJit);
	const float fHalfW   = fWidth * 0.5f;
	const float fHalfD   = fDepth * 0.5f;
	const float fWallTop = fStoreyH * static_cast<float>(xR.m_uStoreys);

	// Two disjoint UV sub-rects of the facade atlas: walls in the lower band, roof in
	// the upper band. Both stay within [0,1] (the static validator rejects out-of-range UVs).
	ZM_GenUVIsland xWall;  xWall.m_fU0 = 0.0f; xWall.m_fV0 = 0.0f;  xWall.m_fU1 = 1.0f; xWall.m_fV1 = 0.72f;
	ZM_GenUVIsland xRoof;  xRoof.m_fU0 = 0.0f; xRoof.m_fV0 = 0.78f; xRoof.m_fU1 = 1.0f; xRoof.m_fV1 = 1.0f;

	// Body box: footprint centred on the origin, grounded at y=0 (feet-on-floor,
	// matching the human bind convention).
	ZM_StaticMesh::AppendBox(xMesh, Zenith_Maths::Vector3(-fHalfW, 0.0f, -fHalfD),
		Zenith_Maths::Vector3(fHalfW, fWallTop, fHalfD), xWall);

	// Roof sits on an overhang-expanded eave rectangle at the wall top.
	const float fOverhang = 0.3f;
	const float fHalfMin  = (fHalfW < fHalfD) ? fHalfW : fHalfD;
	const float fRise     = 0.7f * fHalfMin * (1.0f + fPitchJit);
	const float fExW      = fHalfW + fOverhang;
	const float fExD      = fHalfD + fOverhang;
	const Zenith_Maths::Vector3 xEaveMin(-fExW, fWallTop, -fExD);
	const Zenith_Maths::Vector3 xEaveMax( fExW, fWallTop,  fExD);

	switch (xR.m_eRoof)
	{
	case ZM_ROOF_GABLE: ZM_StaticMesh::AppendGableRoof(xMesh, xEaveMin, xEaveMax, fRise, xRoof); break;
	case ZM_ROOF_HIP:   ZM_StaticMesh::AppendHipRoof(xMesh, xEaveMin, xEaveMax, fRise, xRoof);   break;
	case ZM_ROOF_FLAT:
	default:            ZM_StaticMesh::AppendFlatRoof(xMesh, xEaveMin, xEaveMax, 0.4f, xRoof);   break;
	}

	// Finalise tangents (needs normals + UVs; static, so NO skin normaliser, NO bones).
	// AppendBox/the roof emitters already wrote hard per-face normals -- do NOT re-run
	// ZM_GenGenerateNormals (it would weld + smooth the hard corners).
	ZM_GenGenerateTangents(xMesh);
}

ZM_GenImage ZM_BuildBuildingFacade(const ZM_BuildingRecipe& xR)
{
	ZM_GenImage xImg(uZM_BUILDING_FACADE_RESOLUTION, uZM_BUILDING_FACADE_RESOLUTION);
	ZM_SynthFillSolid(xImg, ZM_BuildingPaletteColour(xR.m_ePalette));
	return xImg;
}

void ZM_BuildBuilding(ZM_BUILDING_ID eId, ZM_Building& xOut)
{
	const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);

	xOut.m_eId = eId;
	ZM_BuildBuildingMesh(xR, xOut.m_xMesh);
	xOut.m_xFacade = ZM_BuildBuildingFacade(xR);
}

// ============================================================================
// Determinism helpers.
// ============================================================================
bool ZM_BuildingMeshEqual(const ZM_GenMesh& xA, const ZM_GenMesh& xB)
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

bool ZM_BuildingBuildEqual(const ZM_Building& xA, const ZM_Building& xB)
{
	return ZM_BuildingMeshEqual(xA.m_xMesh, xB.m_xMesh)
		&& xA.m_xFacade.Equals(xB.m_xFacade);
}

u_int ZM_BuildingContentHash(const ZM_Building& xBuilding)
{
	const ZM_GenMesh& xMesh = xBuilding.m_xMesh;
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

	// Fold the facade content hash (already FNV over packed texels).
	uHash = ZM_GenHashCombine(uHash, xBuilding.m_xFacade.ContentHash());
	return uHash;
}

// ============================================================================
// Validation.
// ============================================================================
ZM_BuildingValidation ZM_ValidateBuilding(const ZM_Building& xBuilding)
{
	ZM_BuildingValidation xV;
	xV.m_xMesh          = ZM_ValidateGenMeshStatic(xBuilding.m_xMesh);
	xV.m_bFacadeNonEmpty = !xBuilding.m_xFacade.IsEmpty();
	xV.m_bAllValid       = xV.m_xMesh.m_bAllValid && xV.m_bFacadeNonEmpty;
	return xV;
}

// ============================================================================
// Asset-path scheme.
// ============================================================================
bool ZM_BuildingAssetPath(ZM_BUILDING_ID eId, ZM_BUILDING_ASSET_KIND eKind, char* szOut, u_int uCap)
{
	Zenith_Assert(szOut != nullptr, "ZM_BuildingAssetPath: null output buffer");
	if (szOut == nullptr || uCap == 0u)
	{
		return false;
	}
	szOut[0] = '\0';

	const char* szName = ZM_GetBuildingName(eId);
	char acBase[128];
	const int iBase = snprintf(acBase, sizeof(acBase), ZM_BuildingBasenameFmt(eKind), szName);
	if (iBase < 0 || static_cast<u_int>(iBase) >= sizeof(acBase))
	{
		return false;   // internal basename overflowed acBase -- overflow contract is TOTAL
	}

	const int iN = snprintf(szOut, uCap, "game:Buildings/%s/%s", szName, acBase);
	return iN >= 0 && static_cast<u_int>(iN) < uCap;   // false on truncation/overflow
}

// ============================================================================
// Disk bake (TOOLS ONLY) -- STUBS in SC1. The real bake (mesh/facade/.zmtrl/
// .zmodel bundle via the ZM_GenBakeStaticMesh bridge) lands in SC5. Declared here
// so the seam is frozen; non-tools no-ops live in the header.
// ============================================================================
#ifdef ZENITH_TOOLS
bool ZM_BakeBuilding(ZM_BUILDING_ID /*eId*/)
{
	return false;   // SC5
}

bool ZM_BakeAllBuildings()
{
	return false;   // SC5
}
#endif   // ZENITH_TOOLS
