#include "Zenith.h"

// ============================================================================
// ZM_PropGen -- the S4 prop generator driver. See the header for the architecture
// + determinism contract. This TU owns: prop -> recipe resolution, the per-domain
// seed derivation, the SC4 static box-composition mesh builder (per-kind box set +
// MESH-domain jitter), the SC4 placeholder-albedo builder (palette/biome base +
// accent, ALBEDO domain only), the full-bundle driver, the byte-identity + hash +
// validation machinery, the asset-path scheme, and (tools only, SC5) the disk
// bake STUBS.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_PropGen.h"

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
	// (static, not visible here), so the albedo builder gets its own copies.
	inline float ZM_Clamp01f(float f) { return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); }
	inline Zenith_Maths::Vector3 ZM_ClampV3(const Zenith_Maths::Vector3& v)
	{ return Zenith_Maths::Vector3(ZM_Clamp01f(v.x), ZM_Clamp01f(v.y), ZM_Clamp01f(v.z)); }
	inline Zenith_Maths::Vector3 ZM_LerpV3(const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b, float t)
	{ return Zenith_Maths::Vector3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }

	// Per-palette base colour (the SC4 albedo base before the biome tint + jitter;
	// distinct per palette so different-palette props never collide).
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

	// (cx,cz) horizontal centre; y0 base; (sx,sy,sz) full sizes. Static, bone-free:
	// wraps ZM_StaticMesh::AppendBox with the default full-[0,1] UV island.
	void ZM_AppendPropBox(ZM_GenMesh& m, float cx, float cz, float y0,
		float sx, float sy, float sz, const ZM_GenUVIsland& xUV)
	{
		ZM_StaticMesh::AppendBox(m,
			Zenith_Maths::Vector3(cx - 0.5f * sx, y0,      cz - 0.5f * sz),
			Zenith_Maths::Vector3(cx + 0.5f * sx, y0 + sy, cz + 0.5f * sz), xUV);
	}

	// Per-kind per-model file basename pattern (embeds the prop name).
	const char* ZM_PropBasenameFmt(ZM_PROP_ASSET_KIND eKind)
	{
		switch (eKind)
		{
		case ZM_PROP_ASSET_MESH:     return "%s.zmesh";
		case ZM_PROP_ASSET_ALBEDO:   return "%s_albedo.ztxtr";
		case ZM_PROP_ASSET_MATERIAL: return "%s.zmtrl";
		case ZM_PROP_ASSET_MODEL:    return "%s.zmodel";
		default:
			Zenith_Assert(false, "ZM_PropBasenameFmt: bad kind %u", (u_int)eKind);
			return "%s.bin";
		}
	}
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

	// Shape axes copied from the roster row (drive the box composition + albedo).
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

	const float fW = xR.m_fWidth  * (1.0f + fWJit);
	const float fD = xR.m_fDepth  * (1.0f + fDJit);
	const float fH = xR.m_fHeight * (1.0f + fHJit);
	const float hW = 0.5f * fW, hD = 0.5f * fD;
	const ZM_GenUVIsland xUV;

	switch (xR.m_eKind)
	{
	case ZM_PROP_KIND_FENCE:
		ZM_AppendPropBox(xMesh, -hW,  0.0f, 0.0f,        0.12f, fH,   0.12f, xUV);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.0f,        0.12f, fH,   0.12f, xUV);
		ZM_AppendPropBox(xMesh, +hW,  0.0f, 0.0f,        0.12f, fH,   0.12f, xUV);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.30f * fH,  fW,    0.08f, 0.06f, xUV);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.70f * fH,  fW,    0.08f, 0.06f, xUV);
		break;
	case ZM_PROP_KIND_SIGN:
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.0f,        0.14f, fH,          0.14f, xUV);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.62f * fH,  fW,    0.30f * fH,  0.08f, xUV);
		break;
	case ZM_PROP_KIND_LAMP:
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.0f,        0.30f, 0.15f,       0.30f, xUV);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.15f,       0.12f, fH - 0.35f,  0.12f, xUV);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, fH - 0.25f,  0.30f, 0.25f,       0.30f, xUV);
		break;
	case ZM_PROP_KIND_BRIDGE:
		ZM_AppendPropBox(xMesh, 0.0f,          +(hD - 0.10f), 0.0f,        0.20f * fW, 0.50f * fH, 0.15f, xUV);
		ZM_AppendPropBox(xMesh, 0.0f,          -(hD - 0.10f), 0.0f,        0.20f * fW, 0.50f * fH, 0.15f, xUV);
		ZM_AppendPropBox(xMesh, 0.0f,          0.0f,          0.50f * fH,  fW,         0.12f,      fD,    xUV);
		ZM_AppendPropBox(xMesh, +(hW - 0.06f), 0.0f,          0.50f * fH,  0.08f,      0.40f,      fD,    xUV);
		ZM_AppendPropBox(xMesh, -(hW - 0.06f), 0.0f,          0.50f * fH,  0.08f,      0.40f,      fD,    xUV);
		break;
	case ZM_PROP_KIND_LEDGE:
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f,          0.0f,        fW, 0.60f * fH, fD,    xUV);
		ZM_AppendPropBox(xMesh, 0.0f, +(hD - 0.15f), 0.60f * fH,  fW, 0.40f * fH, 0.30f, xUV);
		break;
	case ZM_PROP_KIND_ROCK:
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.0f, fW, fH, fD, xUV);
		break;
	case ZM_PROP_KIND_FURNITURE:
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.0f,        fW,         0.60f * fH, fD,         xUV);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, 0.60f * fH,  0.90f * fW, 0.40f * fH, 0.90f * fD, xUV);
		break;
	case ZM_PROP_KIND_DRESSING:
	default:
	{
		const float axCX[4] = { -0.25f * fW, +0.25f * fW, -0.25f * fW, +0.25f * fW };
		const float axCZ[4] = { -0.25f * fD, -0.25f * fD, +0.25f * fD, +0.25f * fD };
		for (u_int i = 0; i < 4u; ++i) { ZM_AppendPropBox(xMesh, axCX[i], axCZ[i], 0.0f, 0.40f * fW, afAux[i] * fH, 0.40f * fD, xUV); }
		break;
	}
	}

	// Finalise tangents (needs normals + UVs; static, so NO skin normaliser, NO bones).
	// AppendBox already wrote hard per-face normals -- do NOT re-run ZM_GenGenerateNormals
	// (it would weld + smooth the hard corners).
	ZM_GenGenerateTangents(xMesh);
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

	const Zenith_Maths::Vector3 xAccent = ZM_ClampV3(Zenith_Maths::Vector3(
		xBase.x * 0.70f + fAccJit, xBase.y * 0.70f + fAccJit, xBase.z * 0.70f + fAccJit));

	// FIXED paint order: base fill -> accent band across the top of the image.
	ZM_SynthFillSolid(xImg, xBase);
	ZM_SynthStampRectDecal(xImg, 0.0f, 0.80f, 1.0f, 1.0f, xAccent);
	return xImg;
}

void ZM_BuildProp(ZM_PROP_ID eId, ZM_Prop& xOut)
{
	const ZM_PropRecipe xR = ZM_ResolvePropRecipe(eId);

	xOut.m_eId = eId;
	ZM_BuildPropMesh(xR, xOut.m_xMesh);
	xOut.m_xTexture = ZM_BuildPropTexture(xR);
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
		&& xA.m_xTexture.Equals(xB.m_xTexture);
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

	// Fold the texture content hash (already FNV over packed texels).
	uHash = ZM_GenHashCombine(uHash, xProp.m_xTexture.ContentHash());
	return uHash;
}

// ============================================================================
// Validation.
// ============================================================================
ZM_PropValidation ZM_ValidateProp(const ZM_Prop& xProp)
{
	ZM_PropValidation xV;
	xV.m_xMesh           = ZM_ValidateGenMeshStatic(xProp.m_xMesh);
	xV.m_bTextureNonEmpty = !xProp.m_xTexture.IsEmpty();
	xV.m_bAllValid        = xV.m_xMesh.m_bAllValid && xV.m_bTextureNonEmpty;
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
// Disk bake (TOOLS ONLY) -- STUBS in SC4. The real bake (mesh/albedo/.zmtrl/
// .zmodel bundle via the ZM_GenBakeStaticMesh bridge) lands in SC5. Declared here
// so the seam is frozen; non-tools no-ops live in the header.
// ============================================================================
#ifdef ZENITH_TOOLS
bool ZM_BakeProp(ZM_PROP_ID /*eId*/)
{
	return false;   // SC5
}

bool ZM_BakeAllProps()
{
	return false;   // SC5
}
#endif   // ZENITH_TOOLS
