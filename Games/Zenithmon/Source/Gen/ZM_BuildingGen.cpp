#include "Zenith.h"

// ============================================================================
// ZM_BuildingGen -- the S4 building generator driver. See the header for the
// architecture + determinism contract. This TU owns: building -> recipe
// resolution, the per-domain seed derivation, the SC2 parametric shell mesh
// builder (body box + roof-by-kind + MESH-domain jitter), the SC3 facade decal
// builder (wall band palette/theme/window-grid/door + roof band, ALBEDO domain
// only), the full-bundle driver, the byte-identity + hash + validation machinery,
// the asset-path scheme, and (tools only, SC5) the disk bake STUBS.
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

	// Per-palette wall base colour (the SC3 facade wall band's starting colour before
	// the gym theme tint + ALBEDO jitter; distinct per palette so different-palette
	// facades never collide).
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

	// Small colour helpers -- ZM_TextureSynth.cpp keeps its Clamp01/Lerp3 file-local
	// (static, not visible here), so the facade builder gets its own copies.
	inline float ZM_Clamp01f(float f) { return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); }
	inline Zenith_Maths::Vector3 ZM_ClampV3(const Zenith_Maths::Vector3& v)
	{ return Zenith_Maths::Vector3(ZM_Clamp01f(v.x), ZM_Clamp01f(v.y), ZM_Clamp01f(v.z)); }
	inline Zenith_Maths::Vector3 ZM_LerpV3(const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b, float t)
	{ return Zenith_Maths::Vector3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }

	// Per-palette roof band colour (the SC3 facade roof band; distinct from the wall
	// base so window/door/roof/wall read as separate surfaces).
	Zenith_Maths::Vector3 ZM_BuildingRoofColour(ZM_BUILDING_PALETTE ePalette)
	{
		switch (ePalette)
		{
		case ZM_BUILDING_PALETTE_WARM:  return Zenith_Maths::Vector3(0.45f, 0.22f, 0.18f);
		case ZM_BUILDING_PALETTE_COOL:  return Zenith_Maths::Vector3(0.28f, 0.32f, 0.42f);
		case ZM_BUILDING_PALETTE_EARTH: return Zenith_Maths::Vector3(0.30f, 0.26f, 0.20f);
		default:
			Zenith_Assert(false, "ZM_BuildingRoofColour: bad palette %u", (u_int)ePalette);
			return Zenith_Maths::Vector3(0.25f, 0.25f, 0.25f);
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

	// ALBEDO is the SOLE randomness source (a MESH-seed mutation can never perturb the
	// facade -> FacadeDomainIsolation). ALL draws up-front, FIXED count + order, BEFORE
	// any palette/theme/grid branch (catch/flee draw-order contract). The window loop
	// draws NO RNG (positions are geometry).
	ZM_GenRNG xRng = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_ALBEDO);
	const float fWallJitR = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fWallJitG = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fWallJitB = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fRoofJit  = xRng.NextFloatRange(-0.04f, 0.04f);

	Zenith_Maths::Vector3 xWall = ZM_BuildingPaletteColour(xR.m_ePalette);
	if (xR.m_eThemeType != ZM_TYPE_NONE)
	{
		const Zenith_Maths::Vector3 xType = ZM_SynthTypePalette(xR.m_eThemeType).m_xBase;
		xWall = ZM_LerpV3(xWall, xType, 0.35f);
	}
	xWall = ZM_ClampV3(Zenith_Maths::Vector3(xWall.x + fWallJitR, xWall.y + fWallJitG, xWall.z + fWallJitB));

	Zenith_Maths::Vector3 xRoof = ZM_BuildingRoofColour(xR.m_ePalette);
	xRoof = ZM_ClampV3(Zenith_Maths::Vector3(xRoof.x + fRoofJit, xRoof.y + fRoofJit, xRoof.z + fRoofJit));

	const Zenith_Maths::Vector3 xWindow(0.80f, 0.88f, 0.95f);
	const Zenith_Maths::Vector3 xDoor(0.30f, 0.18f, 0.10f);

	// FIXED paint order: wall base -> roof band -> window grid (r outer, c inner) -> door.
	ZM_SynthFillSolid(xImg, xWall);
	ZM_SynthStampRectDecal(xImg, 0.0f, fZM_FACADE_ROOF_V0, 1.0f, 1.0f, xRoof);

	const u_int uCols = xR.m_uWindowCols > 0u ? xR.m_uWindowCols : 1u;
	const u_int uRows = xR.m_uWindowRows > 0u ? xR.m_uWindowRows : 1u;
	const float fCellW = (fZM_FACADE_GRID_U1 - fZM_FACADE_GRID_U0) / (float)uCols;
	const float fCellH = (fZM_FACADE_GRID_V1 - fZM_FACADE_GRID_V0) / (float)uRows;
	for (u_int r = 0u; r < uRows; ++r)
	{
		for (u_int c = 0u; c < uCols; ++c)
		{
			const float fCellU = fZM_FACADE_GRID_U0 + (float)c * fCellW;
			const float fCellV = fZM_FACADE_GRID_V0 + (float)r * fCellH;
			ZM_SynthStampRectDecal(xImg,
				fCellU + fZM_FACADE_WIN_INSET * fCellW,
				fCellV + fZM_FACADE_WIN_INSET * fCellH,
				fCellU + (1.0f - fZM_FACADE_WIN_INSET) * fCellW,
				fCellV + (1.0f - fZM_FACADE_WIN_INSET) * fCellH,
				xWindow);
		}
	}

	ZM_SynthStampRectDecal(xImg, fZM_FACADE_DOOR_U0, fZM_FACADE_DOOR_V0,
		fZM_FACADE_DOOR_U1, fZM_FACADE_DOOR_V1, xDoor);
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
// Disk bake (TOOLS ONLY) -- SC5. Writes one static building bundle: the .zmesh
// (via the skeleton-less ZM_GenBakeStaticMesh bridge), the facade .ztxtr (BC1),
// the .zmtrl (baked facade in BASE_COLOR, matte dielectric), and the .zmodel --
// which binds NO skeleton and lists NO animations (buildings are static). The
// mesh bake creates the Buildings/<Name>/ folder FIRST (SaveToFile + model Export
// create no directories), so the material + model writes that follow land in it.
// ============================================================================
#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Collections/Zenith_Vector.h"
#include <filesystem>
#include <string>

bool ZM_BakeBuilding(ZM_BUILDING_ID eId)
{
	ZM_Building xBuilding;
	ZM_BuildBuilding(eId, xBuilding);

	char acMeshRef[512], acFacadeRef[512], acMatRef[512], acModelRef[512];
	bool bOk = true;
	bOk &= ZM_BuildingAssetPath(eId, ZM_BUILDING_ASSET_MESH,     acMeshRef,   sizeof(acMeshRef));
	bOk &= ZM_BuildingAssetPath(eId, ZM_BUILDING_ASSET_FACADE,   acFacadeRef, sizeof(acFacadeRef));
	bOk &= ZM_BuildingAssetPath(eId, ZM_BUILDING_ASSET_MATERIAL, acMatRef,    sizeof(acMatRef));
	bOk &= ZM_BuildingAssetPath(eId, ZM_BUILDING_ASSET_MODEL,    acModelRef,  sizeof(acModelRef));
	if (!bOk)
	{
		return false;   // a path overflowed; do not bake a partial bundle
	}

	const std::string strMeshFs   = Zenith_AssetRegistry::ResolvePath(std::string(acMeshRef));
	const std::string strFacadeFs = Zenith_AssetRegistry::ResolvePath(std::string(acFacadeRef));
	const std::string strMatFs    = Zenith_AssetRegistry::ResolvePath(std::string(acMatRef));
	const std::string strModelFs  = Zenith_AssetRegistry::ResolvePath(std::string(acModelRef));

	// Static mesh (.zmesh) -- NO skeleton, NO skin. Facade (.ztxtr, BC1).
	bOk &= ZM_GenBakeStaticMesh(xBuilding.m_xMesh, strMeshFs.c_str());
	bOk &= ZM_SynthBakeAlbedoBC1(xBuilding.m_xFacade, strFacadeFs.c_str());

	const std::string strName = ZM_GetBuildingName(eId);

	// Material (.zmtrl v5): baked facade in the BASE_COLOR slot, matte dielectric.
	// Create<>()+GetDirect() keeps the asset alive across SaveToFile (never a stack
	// object). The facade is passed as a "game:" ref (stored as a path, NOT loaded now).
	{
		Zenith_AssetHandle<Zenith_MaterialAsset> xMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MaterialAsset* pxMat = xMat.GetDirect();
		pxMat->SetName(strName);
		pxMat->SetDiffuseTexture(TextureHandle(std::string(acFacadeRef)));
		pxMat->SetRoughness(0.8f);
		pxMat->SetMetallic(0.0f);
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

bool ZM_BakeAllBuildings()
{
	bool bOk = true;
	const u_int uCount = static_cast<u_int>(ZM_BUILDING_COUNT);
	for (u_int u = 0; u < uCount; ++u)
	{
		bOk &= ZM_BakeBuilding(static_cast<ZM_BUILDING_ID>(u));
	}
	return bOk;
}
#endif   // ZENITH_TOOLS
