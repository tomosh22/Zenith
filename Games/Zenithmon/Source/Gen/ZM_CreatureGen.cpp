#include "Zenith.h"

// ============================================================================
// ZM_CreatureGen -- the S4 creature generator driver. See the header for the
// architecture + determinism contract. This TU owns: species -> recipe
// resolution, the per-domain seed derivation, the archetype builder dispatch,
// the mesh finalise order, the albedo / shiny / icon builders, the full-bundle
// driver, the byte-identity + hash + validation machinery, the asset-path
// scheme, and (tools only) the disk bake.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"
#include "Zenithmon/Source/Gen/ZM_CreatureArchetypeCommon.h"   // ZM_SizeClassScale

#include <cstdio>    // snprintf
#include <cstring>   // memcmp
#include <cmath>     // fabsf

namespace
{
	// FNV-1a constants (byte-identical to ZM_GenHashName / the terrain seed hash).
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

	// (Recipe pin) Primary elemental type -> pattern kind. PINNED (part of the
	// golden albedo output). All 18 ZM_TYPE mapped; BELLY/NONE both leave the
	// synth pattern switch a no-op (the belly layer is always applied by
	// ZM_SynthCreatureAlbedo regardless), so they read as "no ink overlay".
	ZM_PATTERN_KIND ZM_CreaturePatternForType(ZM_TYPE eType)
	{
		switch (eType)
		{
		case ZM_TYPE_FIRE:
		case ZM_TYPE_ELECTRIC:
		case ZM_TYPE_BRAWL:
		case ZM_TYPE_DRAKE:     return ZM_PATTERN_STRIPES;

		case ZM_TYPE_GRASS:
		case ZM_TYPE_VENOM:
		case ZM_TYPE_SWARM:
		case ZM_TYPE_FEY:       return ZM_PATTERN_SPOTS;

		case ZM_TYPE_WATER:
		case ZM_TYPE_ICE:
		case ZM_TYPE_SKY:
		case ZM_TYPE_MIND:      return ZM_PATTERN_GRADIENT;

		case ZM_TYPE_NORMAL:
		case ZM_TYPE_EARTH:
		case ZM_TYPE_STONE:
		case ZM_TYPE_IRON:      return ZM_PATTERN_BELLY;

		case ZM_TYPE_PHANTOM:
		case ZM_TYPE_UMBRAL:
		default:                return ZM_PATTERN_NONE;
		}
	}

	// Per-kind file basename pattern (embeds the species name). Shared by the
	// asset-ref scheme and the tools filesystem path.
	const char* ZM_CreatureBasenameFmt(ZM_CREATURE_ASSET_KIND eKind)
	{
		switch (eKind)
		{
		case ZM_CREATURE_ASSET_MESH:           return "%s.zmesh";
		case ZM_CREATURE_ASSET_SKELETON:       return "%s.zskel";
		case ZM_CREATURE_ASSET_ALBEDO:         return "%s_albedo.ztxtr";
		case ZM_CREATURE_ASSET_SHINY:          return "%s_shiny.ztxtr";
		case ZM_CREATURE_ASSET_ICON:           return "%s_icon.ztxtr";
		case ZM_CREATURE_ASSET_MATERIAL:       return "%s.zmtrl";
		case ZM_CREATURE_ASSET_MATERIAL_SHINY: return "%s_shiny.zmtrl";
		case ZM_CREATURE_ASSET_MODEL:          return "%s.zmodel";
		case ZM_CREATURE_ASSET_MODEL_SHINY:    return "%s_shiny.zmodel";
		default:
			Zenith_Assert(false, "ZM_CreatureBasenameFmt: bad kind %u", (u_int)eKind);
			return "%s.bin";
		}
	}
}

// ============================================================================
// Recipe resolution.
// ============================================================================
ZM_CreatureRecipe ZM_ResolveCreatureRecipe(ZM_SPECIES_ID eId)
{
	const ZM_SpeciesData& xData = ZM_GetSpeciesData(eId);

	ZM_CreatureRecipe xRecipe;
	xRecipe.m_eSpecies    = eId;
	xRecipe.m_eArchetype  = xData.m_eArchetype;
	xRecipe.m_uEvoStage   = xData.m_uEvoStage;
	xRecipe.m_uFamilySeed = ZM_GetSpeciesFamilySeed(eId);
	xRecipe.m_eSizeClass  = ZM_GetSpeciesSizeClass(eId);
	xRecipe.m_fSizeScale  = ZM_SizeClassScale(xRecipe.m_eSizeClass);
	xRecipe.m_uElaboration = (xData.m_uEvoStage >= 1u) ? (xData.m_uEvoStage - 1u) : 0u;

	// Per-domain PCG seeds -- the SOLE randomness source for every builder.
	for (u_int d = 0; d < static_cast<u_int>(ZM_GEN_DOMAIN_COUNT); ++d)
	{
		xRecipe.m_aulDomainSeed[d] = ZM_GenDeriveSeed(xRecipe.m_uFamilySeed,
			static_cast<u_int>(eId), xData.m_uEvoStage, static_cast<ZM_GEN_DOMAIN>(d));
	}

	// Texture recipe: types drive the palette (resolved inside ZM_SynthCreatureAlbedo).
	xRecipe.m_xTex.m_ePrimaryType   = xData.m_aeTypes[0];
	xRecipe.m_xTex.m_eSecondaryType = xData.m_aeTypes[1];
	xRecipe.m_xTex.m_uWidth         = uZM_CREATURE_ALBEDO_RESOLUTION;
	xRecipe.m_xTex.m_uHeight        = uZM_CREATURE_ALBEDO_RESOLUTION;

	// Pattern derived from the primary type + elaboration tier. (P5) m_fJitter
	// stays dormant (0). (P6) the pattern KIND/params are pure; the ALBEDO-domain
	// rng (not the reserved PATTERN domain) drives ZM_SynthCreatureAlbedo, matching
	// the foundation's single-rng entry point.
	ZM_PatternParams xPat;
	xPat.m_eKind      = ZM_CreaturePatternForType(xData.m_aeTypes[0]);
	xPat.m_fFrequency = 5.0f + static_cast<float>(xRecipe.m_uElaboration);          // 5,6,7
	xPat.m_fContrast  = 0.6f + 0.15f * static_cast<float>(xRecipe.m_uElaboration);  // 0.60,0.75,0.90
	xPat.m_fJitter    = 0.0f;                                                       // (P5) dormant
	xPat.m_uCount     = 8u + 4u * xRecipe.m_uElaboration;                           // 8,12,16
	xRecipe.m_xTex.m_xPattern = xPat;

	// Eye: fixed placement, radius drawn from the EYE domain (deterministic
	// per-species variation; keeps the ALBEDO stream reserved for the body).
	ZM_GenRNG xEyeRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_EYE);
	xRecipe.m_xTex.m_fEyeU      = 0.5f;
	xRecipe.m_xTex.m_fEyeV      = 0.35f;
	xRecipe.m_xTex.m_fEyeRadius = xEyeRng.NextFloatRange(0.05f, 0.075f);

	return xRecipe;
}

float ZM_CreatureShinyAngle(const ZM_CreatureRecipe& xRecipe)
{
	ZM_GenRNG xRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_SHINY);
	return xRng.NextFloatRange(fZM_CREATURE_SHINY_MIN_DEG, fZM_CREATURE_SHINY_MAX_DEG);
}

// ============================================================================
// Archetype builder dispatch.
// ============================================================================
ZM_ArchetypeBuilderFn ZM_GetArchetypeBuilder(ZM_ARCHETYPE eArchetype)
{
	switch (eArchetype)
	{
	case ZM_ARCHETYPE_QUADRUPED:        return &ZM_BuildArchetype_Quadruped;   // SC1
	case ZM_ARCHETYPE_BIPED:            return &ZM_BuildArchetype_Biped;       // SC2
	case ZM_ARCHETYPE_AVIAN:            return &ZM_BuildArchetype_Avian;       // SC2
	case ZM_ARCHETYPE_SERPENT:          return &ZM_BuildArchetype_Serpent;     // SC3
	case ZM_ARCHETYPE_AQUATIC:          return &ZM_BuildArchetype_Aquatic;     // SC3
	// Remaining archetypes land in later SCs (SC4 INSECTOID+BLOB, SC5 FLOATER_PLANTOID).
	case ZM_ARCHETYPE_INSECTOID:
	case ZM_ARCHETYPE_BLOB:
	case ZM_ARCHETYPE_FLOATER_PLANTOID:
	default:                            return nullptr;
	}
}

// ============================================================================
// Per-output builders.
// ============================================================================
void ZM_BuildCreatureMesh(const ZM_CreatureRecipe& xRecipe, ZM_GenMesh& xMesh)
{
	xMesh.Reset();

	const ZM_ArchetypeBuilderFn pfnBuilder = ZM_GetArchetypeBuilder(xRecipe.m_eArchetype);
	Zenith_Assert(pfnBuilder != nullptr,
		"ZM_BuildCreatureMesh: no archetype builder wired for archetype %u yet",
		static_cast<u_int>(xRecipe.m_eArchetype));
	if (pfnBuilder == nullptr)
	{
		return;   // leave xMesh empty; ZM_ValidateCreature will flag it
	}

	pfnBuilder(xMesh, xRecipe);

	// THE finalise order (the archetype builders emit analytic loft normals, so
	// normals are NOT regenerated here): tangents from UVs, then renormalise the
	// <=2-bone ring skin.
	ZM_GenGenerateTangents(xMesh);
	ZM_GenNormalizeSkinWeights(xMesh);

#ifdef ZENITH_DEBUG
	const ZM_GenMeshValidation xVal = ZM_ValidateGenMesh(xMesh, uZM_GEN_CREATURE_BONE_CAP);
	Zenith_Assert(xVal.m_bWindingOutward,   "creature mesh winding not outward (tri %u)", xVal.m_uFirstBadTriangle);
	Zenith_Assert(xVal.m_bBoundsNonDegen,   "creature mesh bounds degenerate");
	Zenith_Assert(xVal.m_bWeightsSumToOne,  "creature mesh weights do not sum to 1 (vert %u)", xVal.m_uFirstBadVertex);
	Zenith_Assert(xVal.m_bWeightsAtMostTwo, "creature mesh vertex over-influenced (vert %u)", xVal.m_uFirstBadVertex);
	Zenith_Assert(xVal.m_bBonesWithinCap,   "creature skeleton exceeds the creature bone cap (30)");
#endif
}

ZM_GenImage ZM_BuildCreatureAlbedo(const ZM_CreatureRecipe& xRecipe)
{
	// (P6) the ALBEDO domain drives the (single) synth rng.
	ZM_GenRNG xRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_ALBEDO);
	return ZM_SynthCreatureAlbedo(xRecipe.m_xTex, xRng);
}

ZM_GenImage ZM_BuildCreatureShiny(const ZM_CreatureRecipe& xRecipe, const ZM_GenImage& xAlbedo)
{
	return ZM_SynthHueRotate(xAlbedo, ZM_CreatureShinyAngle(xRecipe));
}

// (P4) Flat dex icon: box-downsample the base albedo to 128^2 and blend it over
// a flat primary-type-tinted background, keyed on the DEX_ICON domain.
ZM_GenImage ZM_BuildCreatureIcon(const ZM_CreatureRecipe& xRecipe, const ZM_GenImage& xAlbedo)
{
	if (xAlbedo.IsEmpty())
	{
		return ZM_GenImage();   // no source -> empty icon (validator flags it)
	}

	const u_int uN = uZM_CREATURE_ICON_RESOLUTION;
	ZM_GenImage xIcon(uN, uN);

	// Flat primary-type-tinted background.
	const ZM_TypePalette xPalette = ZM_SynthTypePalette(xRecipe.m_xTex.m_ePrimaryType);
	const Zenith_Maths::Vector3 xTint = xPalette.m_xBase;

	// DEX_ICON-domain nudge on the background influence (deterministic per species).
	ZM_GenRNG xRng = ZM_MakeGenRNG(xRecipe, ZM_GEN_DOMAIN_DEX_ICON);
	const float fTintMix = xRng.NextFloatRange(0.25f, 0.35f);

	const u_int uSrcW = xAlbedo.GetWidth();
	const u_int uSrcH = xAlbedo.GetHeight();
	const u_int uBlockX = (uSrcW >= uN) ? (uSrcW / uN) : 1u;
	const u_int uBlockY = (uSrcH >= uN) ? (uSrcH / uN) : 1u;

	for (u_int uY = 0; uY < uN; ++uY)
	{
		for (u_int uX = 0; uX < uN; ++uX)
		{
			// Box average over the source block.
			Zenith_Maths::Vector4 xAcc(0.0f);
			u_int uSamples = 0u;
			for (u_int uBy = 0; uBy < uBlockY; ++uBy)
			{
				u_int uSy = uY * uBlockY + uBy;
				if (uSy >= uSrcH) { uSy = uSrcH - 1u; }
				for (u_int uBx = 0; uBx < uBlockX; ++uBx)
				{
					u_int uSx = uX * uBlockX + uBx;
					if (uSx >= uSrcW) { uSx = uSrcW - 1u; }
					xAcc += xAlbedo.Get(uSy, uSx);
					++uSamples;
				}
			}
			const float fInv = (uSamples > 0u) ? (1.0f / static_cast<float>(uSamples)) : 1.0f;
			const Zenith_Maths::Vector4 xAvg = xAcc * fInv;

			// Blend the sampled albedo over the flat type tint.
			const Zenith_Maths::Vector3 xRGB(
				xAvg.x * (1.0f - fTintMix) + xTint.x * fTintMix,
				xAvg.y * (1.0f - fTintMix) + xTint.y * fTintMix,
				xAvg.z * (1.0f - fTintMix) + xTint.z * fTintMix);
			xIcon.Set(uY, uX, Zenith_Maths::Vector4(xRGB.x, xRGB.y, xRGB.z, 1.0f));
		}
	}
	return xIcon;
}

void ZM_BuildCreature(ZM_SPECIES_ID eId, ZM_Creature& xOut)
{
	const ZM_CreatureRecipe xRecipe = ZM_ResolveCreatureRecipe(eId);

	xOut.m_eSpecies = eId;
	ZM_BuildCreatureMesh(xRecipe, xOut.m_xMesh);
	xOut.m_xAlbedo = ZM_BuildCreatureAlbedo(xRecipe);
	xOut.m_xShiny  = ZM_BuildCreatureShiny(xRecipe, xOut.m_xAlbedo);
	xOut.m_xIcon   = ZM_BuildCreatureIcon(xRecipe, xOut.m_xAlbedo);
}

// ============================================================================
// Determinism helpers.
// ============================================================================
bool ZM_CreatureMeshEqual(const ZM_GenMesh& xA, const ZM_GenMesh& xB)
{
	return ZM_BufferBytesEqual(xA.m_xPositions,   xB.m_xPositions)
		&& ZM_BufferBytesEqual(xA.m_xNormals,     xB.m_xNormals)
		&& ZM_BufferBytesEqual(xA.m_xUVs,         xB.m_xUVs)
		&& ZM_BufferBytesEqual(xA.m_xTangents,    xB.m_xTangents)
		&& ZM_BufferBytesEqual(xA.m_xColors,      xB.m_xColors)
		&& ZM_BufferBytesEqual(xA.m_xIndices,     xB.m_xIndices)
		&& ZM_BufferBytesEqual(xA.m_xBoneIndices, xB.m_xBoneIndices)
		&& ZM_BufferBytesEqual(xA.m_xBoneWeights, xB.m_xBoneWeights)
		&& ZM_BufferBytesEqual(xA.m_xBones,       xB.m_xBones);
}

bool ZM_CreatureBuildEqual(const ZM_Creature& xA, const ZM_Creature& xB)
{
	return ZM_CreatureMeshEqual(xA.m_xMesh, xB.m_xMesh)
		&& xA.m_xAlbedo.Equals(xB.m_xAlbedo)
		&& xA.m_xShiny.Equals(xB.m_xShiny)
		&& xA.m_xIcon.Equals(xB.m_xIcon);
}

u_int ZM_CreatureContentHash(const ZM_Creature& xCreature)
{
	const ZM_GenMesh& xMesh = xCreature.m_xMesh;
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

	// Fold the three image content hashes (each already FNV over packed texels).
	uHash = ZM_GenHashCombine(uHash, xCreature.m_xAlbedo.ContentHash());
	uHash = ZM_GenHashCombine(uHash, xCreature.m_xShiny.ContentHash());
	uHash = ZM_GenHashCombine(uHash, xCreature.m_xIcon.ContentHash());
	return uHash;
}

// ============================================================================
// Validation.
// ============================================================================
namespace
{
	// The icon carries >= 2 distinct texels iff any texel differs from texel(0,0).
	bool ZM_IconHasDistinctTexels(const ZM_GenImage& xIcon)
	{
		if (xIcon.IsEmpty()) { return false; }
		const Zenith_Maths::Vector4 xRef = xIcon.Get(0u, 0u);
		for (u_int uY = 0; uY < xIcon.GetHeight(); ++uY)
		{
			for (u_int uX = 0; uX < xIcon.GetWidth(); ++uX)
			{
				const Zenith_Maths::Vector4 xT = xIcon.Get(uY, uX);
				if (fabsf(xT.x - xRef.x) > 1.0e-6f || fabsf(xT.y - xRef.y) > 1.0e-6f
					|| fabsf(xT.z - xRef.z) > 1.0e-6f || fabsf(xT.w - xRef.w) > 1.0e-6f)
				{
					return true;
				}
			}
		}
		return false;
	}
}

ZM_CreatureValidation ZM_ValidateCreature(const ZM_Creature& xCreature)
{
	ZM_CreatureValidation xV;

	// --- Mesh structure ---
	const ZM_GenMeshValidation xMesh = ZM_ValidateGenMesh(xCreature.m_xMesh, uZM_GEN_CREATURE_BONE_CAP);
	xV.m_bWindingOutward   = xMesh.m_bWindingOutward;
	xV.m_bBoundsNonDegen   = xMesh.m_bBoundsNonDegen;
	xV.m_bWeightsSumToOne  = xMesh.m_bWeightsSumToOne;
	xV.m_bWeightsAtMostTwo = xMesh.m_bWeightsAtMostTwo;
	xV.m_bBonesWithinCap   = xMesh.m_bBonesWithinCap;
	xV.m_uFirstBadVertex   = xMesh.m_uFirstBadVertex;
	xV.m_uFirstBadTriangle = xMesh.m_uFirstBadTriangle;
	xV.m_bMeshValid = xV.m_bWindingOutward && xV.m_bBoundsNonDegen && xV.m_bWeightsSumToOne
		&& xV.m_bWeightsAtMostTwo && xV.m_bBonesWithinCap;

	// --- Skeleton topology (single root + parent-before-child) ---
	const u_int uNumBones = xCreature.m_xMesh.GetNumBones();
	u_int uRootCount = 0u;
	bool  bOrder = (uNumBones > 0u);
	for (u_int u = 0; u < uNumBones; ++u)
	{
		const int iParent = xCreature.m_xMesh.m_xBones.Get(u).m_iParent;
		if (iParent == -1)
		{
			++uRootCount;
		}
		else if (!(iParent < static_cast<int>(u)))
		{
			bOrder = false;
		}
	}
	xV.m_bHasSingleRoot         = (uRootCount == 1u);
	xV.m_bParentsBeforeChildren = bOrder;

	// --- Textures ---
	xV.m_bAlbedoNonEmpty = !xCreature.m_xAlbedo.IsEmpty();
	xV.m_bShinyDimsMatch = !xCreature.m_xShiny.IsEmpty()
		&& xCreature.m_xShiny.GetWidth()  == xCreature.m_xAlbedo.GetWidth()
		&& xCreature.m_xShiny.GetHeight() == xCreature.m_xAlbedo.GetHeight();
	xV.m_bShinyDiffers = xV.m_bShinyDimsMatch && !xCreature.m_xShiny.Equals(xCreature.m_xAlbedo);
	xV.m_bIconNonEmpty = !xCreature.m_xIcon.IsEmpty();
	xV.m_bIconDistinctTexels = ZM_IconHasDistinctTexels(xCreature.m_xIcon);

	// --- Rollup ---
	xV.m_bAllValid = xV.m_bMeshValid
		&& xV.m_bHasSingleRoot && xV.m_bParentsBeforeChildren
		&& xV.m_bAlbedoNonEmpty && xV.m_bShinyDimsMatch && xV.m_bShinyDiffers
		&& xV.m_bIconNonEmpty && xV.m_bIconDistinctTexels;
	return xV;
}

// ============================================================================
// Asset-path scheme.
// ============================================================================
bool ZM_CreatureAssetPath(ZM_SPECIES_ID eId, ZM_CREATURE_ASSET_KIND eKind,
	char* szOut, u_int uCap)
{
	Zenith_Assert(szOut != nullptr, "ZM_CreatureAssetPath: null output buffer");
	if (szOut == nullptr || uCap == 0u)
	{
		return false;
	}
	szOut[0] = '\0';

	const char* szName = ZM_GetSpeciesName(eId);
	char acBase[128];
	snprintf(acBase, sizeof(acBase), ZM_CreatureBasenameFmt(eKind), szName);

	const int iN = snprintf(szOut, uCap, "game:Creatures/%s/%s", szName, acBase);
	return iN >= 0 && static_cast<u_int>(iN) < uCap;   // false on truncation/overflow
}

// ============================================================================
// Disk bake (TOOLS ONLY).
// ============================================================================
#ifdef ZENITH_TOOLS
namespace
{
	// Build the GAME_ASSETS_DIR filesystem path mirroring the asset-ref scheme:
	//     <GAME_ASSETS_DIR>/Creatures/<Name>/<Name><suffix>.<ext>
	bool ZM_CreatureFsPath(ZM_SPECIES_ID eId, ZM_CREATURE_ASSET_KIND eKind,
		char* szOut, u_int uCap)
	{
		if (szOut == nullptr || uCap == 0u) { return false; }
		szOut[0] = '\0';
		const char* szName = ZM_GetSpeciesName(eId);
		char acBase[128];
		snprintf(acBase, sizeof(acBase), ZM_CreatureBasenameFmt(eKind), szName);
		const int iN = snprintf(szOut, uCap, "%s/Creatures/%s/%s", GAME_ASSETS_DIR, szName, acBase);
		return iN >= 0 && static_cast<u_int>(iN) < uCap;
	}
}

bool ZM_BakeCreature(ZM_SPECIES_ID eId)
{
	ZM_Creature xCreature;
	ZM_BuildCreature(eId, xCreature);

	char acMeshFs[1024];
	char acSkelFs[1024];
	char acAlbedoFs[1024];
	char acShinyFs[1024];
	char acIconFs[1024];
	char acSkelRef[512];

	bool bOk = true;
	bOk &= ZM_CreatureFsPath(eId, ZM_CREATURE_ASSET_MESH,     acMeshFs,   sizeof(acMeshFs));
	bOk &= ZM_CreatureFsPath(eId, ZM_CREATURE_ASSET_SKELETON, acSkelFs,   sizeof(acSkelFs));
	bOk &= ZM_CreatureFsPath(eId, ZM_CREATURE_ASSET_ALBEDO,   acAlbedoFs, sizeof(acAlbedoFs));
	bOk &= ZM_CreatureFsPath(eId, ZM_CREATURE_ASSET_SHINY,    acShinyFs,  sizeof(acShinyFs));
	bOk &= ZM_CreatureFsPath(eId, ZM_CREATURE_ASSET_ICON,     acIconFs,   sizeof(acIconFs));
	bOk &= ZM_CreatureAssetPath(eId, ZM_CREATURE_ASSET_SKELETON, acSkelRef, sizeof(acSkelRef));
	if (!bOk)
	{
		return false;   // a path overflowed; do not bake a partial set
	}

	// Mesh + skeleton (.zmesh + .zskel) with the "game:" skeleton ref wired.
	bOk &= ZM_GenBakeMesh(xCreature.m_xMesh, acMeshFs, acSkelFs, acSkelRef);
	// Albedo + shiny (BC1 512^2) + flat dex icon (BC1 128^2).
	bOk &= ZM_SynthBakeAlbedoBC1(xCreature.m_xAlbedo, acAlbedoFs);
	bOk &= ZM_SynthBakeAlbedoBC1(xCreature.m_xShiny,  acShinyFs);
	bOk &= ZM_SynthBakeIconBC1(xCreature.m_xIcon,     acIconFs);

	// SC5: write the .zmtrl (normal + shiny child material) and .zmodel (normal +
	// shiny) bundles here, keyed on ZM_CREATURE_ASSET_MATERIAL[_SHINY] /
	// ZM_CREATURE_ASSET_MODEL[_SHINY], once the material/model bake bridges land.

	return bOk;
}

bool ZM_BakeAllCreatures()
{
	bool bOk = true;
	const u_int uCount = ZM_GetSpeciesCount();
	for (u_int u = 0; u < uCount; ++u)
	{
		const ZM_SPECIES_ID eId = static_cast<ZM_SPECIES_ID>(u);
		// SC1 wires only the QUADRUPED builder; skip species whose archetype has no
		// builder yet (ZM_BuildCreatureMesh would assert on a null builder).
		if (ZM_GetArchetypeBuilder(ZM_GetSpeciesData(eId).m_eArchetype) == nullptr)
		{
			continue;
		}
		bOk &= ZM_BakeCreature(eId);
	}
	return bOk;
}
#endif   // ZENITH_TOOLS
