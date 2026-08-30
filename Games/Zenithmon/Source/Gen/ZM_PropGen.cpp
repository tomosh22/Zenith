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
		case ZM_PROP_ASSET_NORMAL:   return "%s_normal.ztxtr";
		case ZM_PROP_ASSET_ROUGH_METAL: return "%s_rm.ztxtr";
		case ZM_PROP_ASSET_OCCLUSION:   return "%s_ao.ztxtr";
		case ZM_PROP_ASSET_MATERIAL: return "%s.zmtrl";
		case ZM_PROP_ASSET_MODEL:    return "%s.zmodel";
		default:
			Zenith_Assert(false, "ZM_PropBasenameFmt: bad kind %u", (u_int)eKind);
			return "%s.bin";
		}
	}
}

// Per-palette base colour (the SC4 albedo base before the biome tint + jitter;
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

	// ---- The two CENTRE-ANCHORED kinds -------------------------------------
	//
	// ★ THESE TWO START AT fZM_PROP_ITEM_BASE_Y, NOT AT ZERO, and the header says
	// why in full: their entity's authored origin is the CENTRE of a unit volume,
	// so -0.5 is the ground and 0.0 would hover. The stack below sums to exactly
	// 1.0 * fH, so the top lands at -0.5 + fH and a row's height is still literally
	// how tall the thing is.
	case ZM_PROP_KIND_ITEM_PICKUP:
		// A plinth, the item's BODY, and a cap/stopper. The body and cap are the
		// silhouette; the plinth is shared with SPENT below on purpose, so a prop that
		// has been taken reads as "the place the item was" rather than as a new object.
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, fZM_PROP_ITEM_BASE_Y,               fW,         0.14f * fH, fD,         xUV);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, fZM_PROP_ITEM_BASE_Y + 0.14f * fH,  0.62f * fW, 0.62f * fH, 0.62f * fD, xUV);
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, fZM_PROP_ITEM_BASE_Y + 0.76f * fH,  0.30f * fW, 0.24f * fH, 0.30f * fD, xUV);
		break;
	case ZM_PROP_KIND_ITEM_SPENT:
		// The plinth with the body REMOVED and a rim added: an open, empty tray. The
		// emptiness is the whole point -- a spent prop must not read as a pickup, and
		// something small still standing on the plinth would.
		ZM_AppendPropBox(xMesh, 0.0f, 0.0f, fZM_PROP_ITEM_BASE_Y,  fW,         0.45f * fH, fD,         xUV);
		ZM_AppendPropBox(xMesh, 0.0f, +(hD - 0.06f * fD), fZM_PROP_ITEM_BASE_Y,  fW,         fH, 0.12f * fD, xUV);
		ZM_AppendPropBox(xMesh, 0.0f, -(hD - 0.06f * fD), fZM_PROP_ITEM_BASE_Y,  fW,         fH, 0.12f * fD, xUV);
		ZM_AppendPropBox(xMesh, +(hW - 0.06f * fW), 0.0f, fZM_PROP_ITEM_BASE_Y,  0.12f * fW, fH, fD,         xUV);
		ZM_AppendPropBox(xMesh, -(hW - 0.06f * fW), 0.0f, fZM_PROP_ITEM_BASE_Y,  0.12f * fW, fH, fD,         xUV);
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
			float fH = 0.5f;

			switch (xR.m_ePalette)
			{
			case ZM_PROP_PALETTE_WOOD:
				// Grain: stretched hard along U so it runs the length of a board,
				// plus a coarse ring lattice for the growth rings.
				fH += (ZM_SynthValueNoise(fU * 0.20f, fV * 10.0f, 16u, uSalt) - 0.5f) * 0.34f;
				fH += (ZM_SynthValueNoise(fU * 0.08f, fV * 3.0f,  8u, uSalt + 31u) - 0.5f) * 0.16f;
				break;
			case ZM_PROP_PALETTE_STONE:
				// Isotropic pitting: two octaves, no direction.
				fH += (ZM_SynthFbm(fU, fV, 12u, uSalt) - 0.5f) * 0.44f;
				break;
			case ZM_PROP_PALETTE_METAL:
				// Brushed: very fine, very directional, and shallow. Metal relief is
				// almost all in the roughness rather than in the normal.
				fH += (ZM_SynthValueNoise(fU * 24.0f, fV * 0.15f, 24u, uSalt) - 0.5f) * 0.18f;
				break;
			case ZM_PROP_PALETTE_PAINTED:
				// A skim of orange peel, and little else -- paint hides the substrate.
				fH += (ZM_SynthFbm(fU, fV, 20u, uSalt) - 0.5f) * 0.12f;
				break;
			case ZM_PROP_PALETTE_FOLIAGE:
			default:
				// Leafy break-up: mid-frequency clumps.
				fH += (ZM_SynthFbm(fU, fV, 7u, uSalt) - 0.5f) * 0.40f;
				break;
			}

			const float fC = fH < 0.0f ? 0.0f : (fH > 1.0f ? 1.0f : fH);
			xImg.Set(uY, uX, Zenith_Maths::Vector4(fC, fC, fC, 1.0f));
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
	// three maps derived from that height. Part of the determinism contract.
	ZM_SynthPbrResponse xResponse = ZM_PropPbrResponse(xR.m_ePalette);
	// One roughness jitter per prop, from the ALBEDO domain -- the same domain the
	// texture draws from, because it is a surface-finish property, not a shape one.
	ZM_GenRNG xRng = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_ALBEDO);
	xResponse.m_fRoughnessJitter = xRng.NextFloatRange(-0.05f, 0.05f);
	xOut.m_xPbr = ZM_SynthBuildPbrSet(ZM_BuildPropHeight(xR), xResponse);
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
// Disk bake (TOOLS ONLY) -- SC5. Writes one static prop bundle: the .zmesh (via
// the skeleton-less ZM_GenBakeStaticMesh bridge), the albedo .ztxtr (BC1), the
// .zmtrl (baked albedo in BASE_COLOR, matte dielectric), and the .zmodel -- which
// binds NO skeleton and lists NO animations (props are static). The mesh bake
// creates the Props/<Name>/ folder FIRST (SaveToFile + model Export create no
// directories), so the material + model writes that follow land in it.
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
	char acNormalRef[512], acRmRef[512], acAoRef[512];
	bool bOk = true;
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_MESH,        acMeshRef,   sizeof(acMeshRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_ALBEDO,      acAlbedoRef, sizeof(acAlbedoRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_NORMAL,      acNormalRef, sizeof(acNormalRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_ROUGH_METAL, acRmRef,     sizeof(acRmRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_OCCLUSION,   acAoRef,     sizeof(acAoRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_MATERIAL, acMatRef,    sizeof(acMatRef));
	bOk &= ZM_PropAssetPath(eId, ZM_PROP_ASSET_MODEL,    acModelRef,  sizeof(acModelRef));
	if (!bOk)
	{
		return false;   // a path overflowed; do not bake a partial bundle
	}

	const std::string strMeshFs   = Zenith_AssetRegistry::ResolvePath(std::string(acMeshRef));
	const std::string strAlbedoFs = Zenith_AssetRegistry::ResolvePath(std::string(acAlbedoRef));
	const std::string strNormalFs = Zenith_AssetRegistry::ResolvePath(std::string(acNormalRef));
	const std::string strRmFs     = Zenith_AssetRegistry::ResolvePath(std::string(acRmRef));
	const std::string strAoFs     = Zenith_AssetRegistry::ResolvePath(std::string(acAoRef));
	const std::string strMatFs    = Zenith_AssetRegistry::ResolvePath(std::string(acMatRef));
	const std::string strModelFs  = Zenith_AssetRegistry::ResolvePath(std::string(acModelRef));

	// Static mesh (.zmesh) -- NO skeleton, NO skin. Albedo (.ztxtr, BC1).
	bOk &= ZM_GenBakeStaticMesh(xProp.m_xMesh, strMeshFs.c_str());
	bOk &= ZM_SynthBakeAlbedoBC1(xProp.m_xTexture, strAlbedoFs.c_str());
	// ALBEDO is colour (sRGB baked into BC1); the other three are DATA and must
	// stay LINEAR or the shader reads a gamma-curved roughness.
	bOk &= ZM_SynthBakeNormalBC5(xProp.m_xPbr.m_xNormal,            strNormalFs.c_str());
	bOk &= ZM_SynthBakeLinearBC1(xProp.m_xPbr.m_xRoughnessMetallic, strRmFs.c_str());
	bOk &= ZM_SynthBakeLinearBC1(xProp.m_xPbr.m_xOcclusion,         strAoFs.c_str());

	const std::string strName = ZM_GetPropName(eId);

	// Material (.zmtrl v5): baked albedo in the BASE_COLOR slot, matte dielectric.
	// Create<>()+GetDirect() keeps the asset alive across SaveToFile (never a stack
	// object). The albedo is passed as a "game:" ref (stored as a path, NOT loaded now).
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
	// Are all four of ONE prop's per-model files on disk and non-empty? The same
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
	// albedo writers report their own status and this is the one check that covers
	// all four the same way.
	return ZM_PropBundlePresent(eId);
}
#endif   // ZENITH_TOOLS
