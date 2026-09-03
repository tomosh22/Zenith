#include "Zenith.h"

// ============================================================================
// ZM_BuildingGen -- the S4 building generator driver. See the header for the
// architecture + determinism contract. This TU owns: building -> recipe
// resolution, the per-domain seed derivation, the shell-metric resolver every
// surface builder reads, the four per-surface mesh builders (wall / roof / trim /
// glass) with their world-space UV projection, the per-surface PBR map builders
// (albedo + height -> normal, roughness/metallic, ambient occlusion; ALBEDO
// domain only), the full-bundle driver, the byte-identity + hash + validation
// machinery, the asset-path scheme, and (tools only) the disk bake.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_BuildingGen.h"

#include <cmath>     // isfinite
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

	// Per-palette wall base colour (the starting colour before the gym theme tint,
	// the ALBEDO jitter and the per-texel course/weathering work; distinct per
	// palette so different-palette facades never collide).
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
	// (static, not visible here), so this TU gets its own copies.
	inline float ZM_Clamp01f(float f) { return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); }
	inline Zenith_Maths::Vector3 ZM_ClampV3(const Zenith_Maths::Vector3& v)
	{ return Zenith_Maths::Vector3(ZM_Clamp01f(v.x), ZM_Clamp01f(v.y), ZM_Clamp01f(v.z)); }
	inline Zenith_Maths::Vector3 ZM_LerpV3(const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b, float t)
	{ return Zenith_Maths::Vector3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }

	// Per-palette roof colour (distinct from the wall base so roof and wall read as
	// separate materials even before the maps do their work).
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

	// Per-SLOT file basename pattern. Takes the building name and the surface name
	// (in that order), so every per-surface artifact is <Name>_<surface><suffix>.
	const char* ZM_BuildingSlotFmt(ZM_BUILDING_ASSET_SLOT eSlot)
	{
		switch (eSlot)
		{
		case ZM_BUILDING_SLOT_MESH:        return "%s_%s.zmesh";
		case ZM_BUILDING_SLOT_ALBEDO:      return "%s_%s_albedo.ztxtr";
		case ZM_BUILDING_SLOT_NORMAL:      return "%s_%s_normal.ztxtr";
		case ZM_BUILDING_SLOT_ROUGH_METAL: return "%s_%s_rm.ztxtr";
		case ZM_BUILDING_SLOT_OCCLUSION:   return "%s_%s_ao.ztxtr";
	case ZM_BUILDING_SLOT_HEIGHT:      return "%s_%s_height.ztxtr";
		case ZM_BUILDING_SLOT_MATERIAL:    return "%s_%s.zmtrl";
		default:
			Zenith_Assert(false, "ZM_BuildingSlotFmt: bad slot %u", (u_int)eSlot);
			return "%s_%s.bin";
		}
	}
}
// ============================================================================
// SURFACE CLASS DESCRIPTION TABLES.
// ============================================================================
const char* ZM_BuildingSurfaceName(ZM_BUILDING_SURFACE eSurface)
{
	switch (eSurface)
	{
	case ZM_BUILDING_SURFACE_WALL:  return "wall";
	case ZM_BUILDING_SURFACE_ROOF:  return "roof";
	case ZM_BUILDING_SURFACE_TRIM:  return "trim";
	case ZM_BUILDING_SURFACE_GLASS: return "glass";
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingSurfaceName: %u is not a surface -- answering 'wall'",
			(u_int)eSurface);
		return "wall";
	}
}

u_int ZM_BuildingSurfaceResolution(ZM_BUILDING_SURFACE eSurface)
{
	switch (eSurface)
	{
	// 512^2 on a ~3 m tile is 170 px/m: a player standing at the wall gets a
	// texel every 6 mm, and the shared micro-detail pair carries the grain below
	// that. BC1 + mips is ~170 KB a map, which is what the surface split bought
	// the room for.
	case ZM_BUILDING_SURFACE_WALL:  return 512u;
	case ZM_BUILDING_SURFACE_ROOF:  return 512u;
	// Trim is 0.1-0.2 m members on a 0.75 m tile: 256^2 is 341 px/m there, finer
	// than the wall, which is right for the surface a player's eye lands on.
	case ZM_BUILDING_SURFACE_TRIM:  return 256u;
	// Glass carries no pattern worth resolving: it is a near-uniform pane whose
	// look comes from roughness and the reflection, not from texels.
	case ZM_BUILDING_SURFACE_GLASS: return 64u;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingSurfaceResolution: %u is not a surface -- answering wall's",
			(u_int)eSurface);
		return 512u;
	}
}

float ZM_BuildingSurfaceTileMetres(ZM_BUILDING_SURFACE eSurface)
{
	switch (eSurface)
	{
	// NOMINAL only -- the real wall tile is the building's STOREY HEIGHT (see
	// ZM_BuildingSurfaceTileMetresFor). 3 m is the roster's common storey, so a
	// recipe-less caller sees the value the lattices are designed around.
	case ZM_BUILDING_SURFACE_WALL:  return 3.00f;
	case ZM_BUILDING_SURFACE_ROOF:  return 1.50f;
	// Trim members are 0.1-0.2 m thick, so a short tile keeps the grain running
	// along a fascia board rather than smearing one texel down its length.
	case ZM_BUILDING_SURFACE_TRIM:  return 0.75f;
	case ZM_BUILDING_SURFACE_GLASS: return 1.20f;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingSurfaceTileMetres: %u is not a surface -- answering wall's",
			(u_int)eSurface);
		return 3.00f;
	}
}

float ZM_BuildingSurfaceTileMetresFor(const ZM_BuildingRecipe& xR, ZM_BUILDING_SURFACE eSurface)
{
	if (eSurface != ZM_BUILDING_SURFACE_WALL)
	{
		return ZM_BuildingSurfaceTileMetres(eSurface);
	}
	// ★ THE WALL TILE IS THE STOREY. v = worldY / tile then IS height-within-the
	// -storey, which is what lets the splash-back sit above the real plinth, the
	// drips hang under the real sill row and the runoff gather under the real
	// eave -- instead of a gradient that repeats one and a half times per storey
	// and anchors to nothing. Read from the SHELL METRICS, not the roster row, so
	// a jittered storey moves the texture with the geometry.
	const ZM_BuildingShellMetrics xM = ZM_ResolveBuildingShellMetrics(xR);
	return xM.m_fStoreyHeight > 0.1f ? xM.m_fStoreyHeight : ZM_BuildingSurfaceTileMetres(eSurface);
}

ZM_BuildingSurfaceResponse ZM_BuildingSurfaceMaterialResponse(ZM_BUILDING_SURFACE eSurface)
{
	ZM_BuildingSurfaceResponse xR;
	switch (eSurface)
	{
	case ZM_BUILDING_SURFACE_WALL:
		// Render/plaster over masonry: dry, wholly dielectric, and the one surface
		// whose relief is worth pushing because it covers the most screen.
		xR.m_fRoughness = 0.88f; xR.m_fMetallic = 0.0f;
		xR.m_fNormalStrength = 1.15f; xR.m_fOcclusion = 1.0f;
		// POM at 1.6% of the tile: mortar beds and the dish of each unit face
		// actually recede, and the wall's silhouette against a window reveal
		// breaks up. Above ~2% the march starts to swim at grazing angles.
		xR.m_fHeightScale = 0.016f;
		xR.m_fDetailTiling = 10.0f;
		xR.m_fEdgeWear = 0.85f;
		break;
	case ZM_BUILDING_SURFACE_ROOF:
		// Slate/tile: slightly tighter than plaster so a grazing sun picks out the
		// courses, which is most of what reads as "roof" at a distance.
		xR.m_fRoughness = 0.72f; xR.m_fMetallic = 0.0f;
		xR.m_fNormalStrength = 1.30f; xR.m_fOcclusion = 1.0f;
		xR.m_fHeightScale = 0.018f;   // slate laps are deeper than mortar beds
		xR.m_fDetailTiling = 8.0f;
		xR.m_fEdgeWear = 0.70f;
		break;
	case ZM_BUILDING_SURFACE_TRIM:
		// Painted timber: the smoothest opaque surface on the building, which is
		// what separates a frame from the wall it is set into.
		xR.m_fRoughness = 0.45f; xR.m_fMetallic = 0.0f;
		xR.m_fNormalStrength = 0.80f; xR.m_fOcclusion = 1.0f;
		// A painted board's own grain is already at texel scale on a 0.75 m tile,
		// so no detail overlay and only a shallow POM. Its edge wear is the
		// strongest of the four: a handrail, a sill and a door frame are what
		// people actually touch.
		xR.m_fHeightScale = 0.010f;
		xR.m_fDetailTiling = 0.0f;
		xR.m_fEdgeWear = 1.0f;
		break;
	case ZM_BUILDING_SURFACE_GLASS:
		// ★ NOT METALLIC. Glass is a dielectric; its mirror-like behaviour is a
		// low roughness plus Fresnel, and metallic=1 would tint the reflection by
		// the base colour and kill the diffuse entirely. The engine's SSR and IBL
		// supply the reflection -- this only has to stop being matte.
		xR.m_fRoughness = 0.08f; xR.m_fMetallic = 0.0f;
		xR.m_fNormalStrength = 0.25f; xR.m_fOcclusion = 1.0f;
		// NO POM and NO detail overlay on glass: a pane has no relief to march
		// and a grain overlay would frost it.
		xR.m_fHeightScale = 0.0f;
		xR.m_fDetailTiling = 0.0f;
		xR.m_fEdgeWear = 0.0f;
		break;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingSurfaceMaterialResponse: %u is not a surface -- answering wall's",
			(u_int)eSurface);
		xR.m_fRoughness = 0.88f; xR.m_fMetallic = 0.0f;
		xR.m_fNormalStrength = 1.15f; xR.m_fOcclusion = 1.0f;
		xR.m_fHeightScale = 0.016f; xR.m_fDetailTiling = 10.0f; xR.m_fEdgeWear = 0.85f;
		break;
	}
	return xR;
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

	// Shape axes copied from the roster row (drive the shell + every map).
	xR.m_eStyle        = xData.m_eStyle;
	xR.m_ePalette      = xData.m_ePalette;
	xR.m_eRoof         = xData.m_eRoof;
	xR.m_fWidth        = xData.m_fWidth;
	xR.m_fDepth        = xData.m_fDepth;
	xR.m_fStoreyHeight = xData.m_fStoreyHeight;
	xR.m_fRoofPitch    = xData.m_fRoofPitch;
	xR.m_uStoreys      = xData.m_uStoreys;
	xR.m_uWindowCols   = xData.m_uWindowCols;
	xR.m_uWindowRows   = xData.m_uWindowRows;
	xR.m_eThemeType    = xData.m_eThemeType;
	xR.m_bSiteFixed    = xData.m_bSiteFixed;
	xR.m_fDoorWidth    = xData.m_fDoorWidth;
	xR.m_fDoorHeight   = xData.m_fDoorHeight;

	return xR;
}

ZM_GenRNG ZM_MakeGenRNG(const ZM_BuildingRecipe& xR, ZM_GEN_DOMAIN eDomain)
{
	return ZM_GenRNG(xR.m_aulDomainSeed[eDomain]);
}

// ============================================================================
// Shell metrics -- resolved ONCE, read by all four surface builders.
// ============================================================================
float ZM_BuildingShellMetrics::WindowCentreX(u_int uCol) const
{
	const u_int uCols = m_uWindowCols > 0u ? m_uWindowCols : 1u;
	// Columns are spread across the facade's inner span (the quoins own the ends),
	// centre-of-cell so an odd count puts one window exactly on the axis.
	const float fSpan = (m_fHalfW - fZM_BUILDING_QUOIN_WIDTH) * 2.0f;
	const float fCell = fSpan / static_cast<float>(uCols);
	const u_int uClamped = uCol < uCols ? uCol : uCols - 1u;
	return -fSpan * 0.5f + fCell * (static_cast<float>(uClamped) + 0.5f);
}

float ZM_BuildingShellMetrics::WindowSillY(u_int uRow) const
{
	const u_int uRows = m_uWindowRows > 0u ? m_uWindowRows : 1u;
	const u_int uClamped = uRow < uRows ? uRow : uRows - 1u;
	// One row per storey where the counts agree; otherwise rows are spread evenly
	// up the wall. Either way a sill never lands above the eave.
	const float fPerRow = m_fWallTop / static_cast<float>(uRows);
	return fPerRow * static_cast<float>(uClamped) + fZM_BUILDING_WINDOW_SILL_Y;
}

ZM_BuildingShellMetrics ZM_ResolveBuildingShellMetrics(const ZM_BuildingRecipe& xR)
{
	// MESH-domain jitter: ALL draws up-front, in a FIXED order, BEFORE any branch
	// (so neither the roof kind nor the site-fixed flag can change the draw count)
	// and drawing ONLY the MESH domain RNG.
	//
	// ★ THE DRAWS HAPPEN FOR A SITE-FIXED BUILDING TOO. The RNG stream position is
	// part of the authored result; skipping four draws here would re-roll nothing
	// today (MESH is not read again) but would silently move every future
	// MESH-domain output for exactly two rows, which is the kind of divergence that
	// is only ever found by a determinism test failing years later.
	ZM_GenRNG xRng = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_MESH);
	const float fWJit     = xRng.NextFloatRange(-0.03f, 0.03f);
	const float fDJit     = xRng.NextFloatRange(-0.03f, 0.03f);
	const float fHJit     = xRng.NextFloatRange(-0.03f, 0.03f);
	const float fPitchJit = xRng.NextFloatRange(-0.10f, 0.10f);   // always drawn (even FLAT)

	const float fApply = xR.m_bSiteFixed ? 0.0f : 1.0f;

	ZM_BuildingShellMetrics xM;
	xM.m_fWidth        = xR.m_fWidth        * (1.0f + fWJit * fApply);
	xM.m_fDepth        = xR.m_fDepth        * (1.0f + fDJit * fApply);
	xM.m_fStoreyHeight = xR.m_fStoreyHeight * (1.0f + fHJit * fApply);
	xM.m_fHalfW        = xM.m_fWidth * 0.5f;
	xM.m_fHalfD        = xM.m_fDepth * 0.5f;
	xM.m_uStoreys      = xR.m_uStoreys    > 0u ? xR.m_uStoreys    : 1u;
	xM.m_uWindowCols   = xR.m_uWindowCols > 0u ? xR.m_uWindowCols : 1u;
	xM.m_uWindowRows   = xR.m_uWindowRows > 0u ? xR.m_uWindowRows : 1u;
	xM.m_fWallTop      = xM.m_fStoreyHeight * static_cast<float>(xM.m_uStoreys);
	// The door is NEVER jittered, site-fixed or not: on a site-fixed row it is the
	// interior aperture, and on a free row a jittered doorway buys nothing a
	// jittered facade has not already bought.
	xM.m_fDoorWidth    = xR.m_fDoorWidth;
	xM.m_fDoorHeight   = xR.m_fDoorHeight;

	const float fHalfMin = (xM.m_fHalfW < xM.m_fHalfD) ? xM.m_fHalfW : xM.m_fHalfD;
	xM.m_fRise = (xR.m_eRoof == ZM_ROOF_FLAT)
		? 0.0f
		: xR.m_fRoofPitch * fHalfMin * (1.0f + fPitchJit * fApply);
	xM.m_fRidgeY = (xR.m_eRoof == ZM_ROOF_FLAT)
		? xM.m_fWallTop + fZM_BUILDING_PARAPET_HEIGHT
		: xM.m_fWallTop + xM.m_fRise;

	xM.m_fExW = xM.m_fHalfW + fZM_BUILDING_EAVE_OVERHANG;
	xM.m_fExD = xM.m_fHalfD + fZM_BUILDING_EAVE_OVERHANG;

	// A chimney belongs to a pitched roof over a heated room. A flat civic roof
	// gets none, and neither does a building too small to carry one sensibly.
	xM.m_bHasChimney = (xR.m_eRoof != ZM_ROOF_FLAT)
		&& (fHalfMin > fZM_BUILDING_CHIMNEY_SIDE * 1.5f);

	return xM;
}

// ============================================================================
// Mesh builders.
// ============================================================================
namespace
{
	// The identity island. The roof emitters take one; ZM_StaticMesh::ApplyWorldUVs
	// then overwrites every UV they wrote, so it is a placeholder that never
	// survives.
	const ZM_GenUVIsland s_xUnitIsland = { 0.0f, 0.0f, 1.0f, 1.0f };

	// ★ THE WORLD-UV BOX EMITTER LIVES IN ZM_StaticMesh NOW (ZM_GenCommon). It
	// moved there when the INTERIOR generator needed exactly the same primitive:
	// uniform texel density from a world-space projection is a property of
	// architectural surfaces generally, not of buildings, and a second copy would
	// be a second place for the projection convention to drift. This wrapper keeps
	// the call sites below reading as they did. Every box is CHAMFERED unless the
	// caller says otherwise: a panel that abuts another panel of the same surface
	// passes 0, because a bevel along a seam that does not exist in the material
	// would draw a groove the plaster does not have.
	inline void ZM_AppendWorldBox(ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xMin,
		const Zenith_Maths::Vector3& xMax, float fTileMetres,
		float fChamfer = fZM_STATIC_BOX_CHAMFER)
	{
		(void)ZM_StaticMesh::AppendWorldBox(xMesh, xMin, xMax, fTileMetres, fChamfer);
	}

	// Tint every vertex from uFirst onward. Vertex colour multiplies the albedo in
	// the uber-shader (SampleAlbedoWithCutout), which is how one wall material can
	// carry a per-FACE grime cast and how the room-behind card is dark without a
	// fifth material.
	void ZM_TintFrom(ZM_GenMesh& xMesh, u_int uFirst, const Zenith_Maths::Vector4& xTint)
	{
		const u_int uEnd = xMesh.GetNumVerts();
		for (u_int v = uFirst; v < uEnd; ++v)
		{
			xMesh.m_xColors.Get(v) = xTint;
		}
	}

	// One opening's rectangle on a facade, in facade-local (X, Y).
	struct ZM_WindowRect
	{
		float m_fX0, m_fY0, m_fX1, m_fY1;
	};

	ZM_WindowRect ZM_WindowRectAt(const ZM_BuildingShellMetrics& xM, u_int uCol, u_int uRow)
	{
		const float fCx = xM.WindowCentreX(uCol);
		const float fY0 = xM.WindowSillY(uRow);
		return { fCx - fZM_BUILDING_WINDOW_WIDTH * 0.5f, fY0,
		         fCx + fZM_BUILDING_WINDOW_WIDTH * 0.5f, fY0 + fZM_BUILDING_WINDOW_HEIGHT };
	}

	// Is there room on this facade for the window grid at all? A tiny shed asked
	// for 4 columns would otherwise emit overlapping frames wider than its wall.
	bool ZM_FacadeTakesWindows(const ZM_BuildingShellMetrics& xM)
	{
		const float fSpan = (xM.m_fHalfW - fZM_BUILDING_QUOIN_WIDTH) * 2.0f;
		const float fCell = fSpan / static_cast<float>(xM.m_uWindowCols);
		const float fTopRow = xM.WindowSillY(xM.m_uWindowRows - 1u) + fZM_BUILDING_WINDOW_HEIGHT;
		return fCell > fZM_BUILDING_WINDOW_WIDTH + fZM_BUILDING_FRAME_THICK * 4.0f
			&& fTopRow < xM.m_fWallTop - fZM_BUILDING_FASCIA_HEIGHT;
	}

	// The door sits on the -Z facade, on the building's axis. Returns false when
	// the wall is too narrow to carry one clear of the quoins.
	bool ZM_DoorFits(const ZM_BuildingShellMetrics& xM)
	{
		return xM.m_fHalfW > xM.m_fDoorWidth * 0.5f
			+ fZM_BUILDING_DOOR_SURROUND + fZM_BUILDING_QUOIN_WIDTH
			&& xM.m_fWallTop > xM.m_fDoorHeight + fZM_BUILDING_DOOR_SURROUND;
	}

	// ★ WOULD THIS WINDOW BE STANDING IN THE DOORWAY?
	//
	// The window grid is laid out across the facade's whole inner span and knows
	// nothing about the door, which is centred on the axis. For a narrow domestic
	// door the two never meet. For the Home they do: its door is the interior's
	// 4.0 m aperture, and with four columns the inner pair land at x = +/-1.96 --
	// squarely inside the doorway. The first render put a window frame, a sill and
	// a pane THROUGH the front door, with every clause green because nothing
	// compared the two grids.
	//
	// Only ever asked for the -Z facade; the +Z one has no door.
	bool ZM_WindowClearsTheDoor(const ZM_BuildingShellMetrics& xM, const ZM_WindowRect& xW)
	{
		if (!ZM_DoorFits(xM))
		{
			return true;
		}
		const float fDoorHalf = xM.m_fDoorWidth * 0.5f + fZM_BUILDING_DOOR_SURROUND;
		// Below the door head is the only band that can conflict; a fanlight above
		// a 2.5 m door on a 3 m wall is legitimate and rare.
		if (xW.m_fY0 >= xM.m_fDoorHeight + fZM_BUILDING_DOOR_SURROUND)
		{
			return true;
		}
		// The sill overhangs the opening, so clear the WIDER of the two.
		const float fWinLeft  = xW.m_fX0 - fZM_BUILDING_FRAME_THICK * 1.6f;
		const float fWinRight = xW.m_fX1 + fZM_BUILDING_FRAME_THICK * 1.6f;
		return fWinRight <= -fDoorHalf || fWinLeft >= fDoorHalf;
	}

	// Does this (facade, col, row) window exist? One answer for the wall panels,
	// the reveals, the frames, the glass and the room cards, so the five cannot
	// disagree about where the holes are.
	bool ZM_WindowExists(const ZM_BuildingShellMetrics& xM, bool bMinusZ, u_int uCol, u_int uRow,
		ZM_WindowRect& xOut)
	{
		if (!ZM_FacadeTakesWindows(xM))
		{
			return false;
		}
		xOut = ZM_WindowRectAt(xM, uCol, uRow);
		return !bMinusZ || ZM_WindowClearsTheDoor(xM, xOut);
	}

	// The door's opening rectangle (the pocket cut through the facade slab AND the
	// plinth), facade-local. Only on -Z.
	ZM_WindowRect ZM_DoorRect(const ZM_BuildingShellMetrics& xM)
	{
		return { -xM.m_fDoorWidth * 0.5f, 0.0f, xM.m_fDoorWidth * 0.5f, xM.m_fDoorHeight };
	}

	// The pocket a facade's openings sit in: the facade slab's Z band. bMinusZ
	// selects the front (-Z) or back (+Z) facade; "outer" is the wall plane,
	// "inner" is the pocket back, a reveal in.
	struct ZM_FacadeBand
	{
		float m_fOuter, m_fInner;   // z of the wall plane / the pocket back
		float m_fSign;              // outward direction along Z (-1 front, +1 back)
		float Min() const { return m_fOuter < m_fInner ? m_fOuter : m_fInner; }
		float Max() const { return m_fOuter < m_fInner ? m_fInner : m_fOuter; }
		// z a distance d OUTWARD of the pocket back (d > 0 toward the street).
		float FromBack(float fD) const { return m_fInner + m_fSign * fD; }
		// z a distance d outward of the wall plane.
		float FromWall(float fD) const { return m_fOuter + m_fSign * fD; }
	};

	ZM_FacadeBand ZM_FacadeBandOf(const ZM_BuildingShellMetrics& xM, bool bMinusZ)
	{
		ZM_FacadeBand xB;
		xB.m_fSign  = bMinusZ ? -1.0f : 1.0f;
		xB.m_fOuter = bMinusZ ? -xM.m_fHalfD : xM.m_fHalfD;
		xB.m_fInner = xB.m_fOuter - xB.m_fSign * fZM_BUILDING_REVEAL_DEPTH;
		return xB;
	}

	// Collect every opening on a facade, in a FIXED order (door first, then rows
	// then columns). The order is part of the determinism contract only insofar
	// as vertex order is: no RNG is involved.
	u_int ZM_CollectOpenings(const ZM_BuildingShellMetrics& xM, bool bMinusZ,
		ZM_WindowRect* axOut, u_int uCap)
	{
		u_int uN = 0u;
		if (bMinusZ && ZM_DoorFits(xM) && uN < uCap)
		{
			axOut[uN++] = ZM_DoorRect(xM);
		}
		for (u_int r = 0u; r < xM.m_uWindowRows; ++r)
		{
			for (u_int c = 0u; c < xM.m_uWindowCols; ++c)
			{
				ZM_WindowRect xW;
				if (ZM_WindowExists(xM, bMinusZ, c, r, xW) && uN < uCap)
				{
					axOut[uN++] = xW;
				}
			}
		}
		return uN;
	}

	// The most openings one facade can carry: the roster tops out at 5 x 3 windows
	// plus a door. Sized with room to spare; ZM_CollectOpenings clamps to it.
	constexpr u_int uZM_MAX_FACADE_OPENINGS = 64u;

	// ★ PANELISE A SLAB AROUND ITS OPENINGS. A rectangle [x0,x1] x [y0,y1] minus a
	// set of disjoint axis-aligned holes is emitted as a set of boxes: the
	// distinct hole Y edges (plus the slab's own) cut it into horizontal bands,
	// and within each band the holes crossing it cut it into X runs. Every box
	// is world-UV'd, so the texture is continuous across the seams, and every box
	// is UN-chamfered, because those seams are not edges of the material.
	//
	// Emits nothing for a band or run of zero width, so a hole flush with an
	// edge (the door, which starts at y=0 below the slab's own bottom) produces
	// no degenerate box.
	void ZM_EmitPanelisedSlab(ZM_GenMesh& xMesh, float fX0, float fX1, float fY0, float fY1,
		float fZ0, float fZ1, const ZM_WindowRect* axHoles, u_int uHoles, float fTile)
	{
		// Band edges: the slab's bottom/top and every hole edge inside it, sorted,
		// unique. Bounded by the opening cap, so a fixed array suffices.
		float afY[uZM_MAX_FACADE_OPENINGS * 2u + 2u];
		u_int uY = 0u;
		afY[uY++] = fY0;
		afY[uY++] = fY1;
		for (u_int h = 0u; h < uHoles; ++h)
		{
			const float afE[2] = { axHoles[h].m_fY0, axHoles[h].m_fY1 };
			for (u_int e = 0u; e < 2u; ++e)
			{
				if (afE[e] > fY0 && afE[e] < fY1)
				{
					afY[uY++] = afE[e];
				}
			}
		}
		// Insertion sort (tiny N, and it keeps the emission order a pure function
		// of the inputs rather than of a library's sort stability).
		for (u_int i = 1u; i < uY; ++i)
		{
			const float fK = afY[i];
			u_int j = i;
			while (j > 0u && afY[j - 1u] > fK) { afY[j] = afY[j - 1u]; --j; }
			afY[j] = fK;
		}

		constexpr float fEps = 1.0e-4f;
		for (u_int b = 0u; b + 1u < uY; ++b)
		{
			const float fBy0 = afY[b], fBy1 = afY[b + 1u];
			if (fBy1 - fBy0 <= fEps) { continue; }
			const float fMidY = (fBy0 + fBy1) * 0.5f;

			// Holes crossing this band, as X intervals, sorted by x0 (insertion).
			float afHx0[uZM_MAX_FACADE_OPENINGS], afHx1[uZM_MAX_FACADE_OPENINGS];
			u_int uHx = 0u;
			for (u_int h = 0u; h < uHoles; ++h)
			{
				if (axHoles[h].m_fY0 <= fMidY && axHoles[h].m_fY1 >= fMidY)
				{
					float fHx0 = axHoles[h].m_fX0, fHx1 = axHoles[h].m_fX1;
					if (fHx0 < fX0) { fHx0 = fX0; }
					if (fHx1 > fX1) { fHx1 = fX1; }
					if (fHx1 - fHx0 <= fEps) { continue; }
					u_int j = uHx;
					while (j > 0u && afHx0[j - 1u] > fHx0)
					{
						afHx0[j] = afHx0[j - 1u]; afHx1[j] = afHx1[j - 1u]; --j;
					}
					afHx0[j] = fHx0; afHx1[j] = fHx1; ++uHx;
				}
			}

			float fRunX = fX0;
			for (u_int h = 0u; h < uHx; ++h)
			{
				if (afHx0[h] - fRunX > fEps)
				{
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(fRunX, fBy0, fZ0),
						Zenith_Maths::Vector3(afHx0[h], fBy1, fZ1), fTile, 0.0f);
				}
				if (afHx1[h] > fRunX) { fRunX = afHx1[h]; }
			}
			if (fX1 - fRunX > fEps)
			{
				ZM_AppendWorldBox(xMesh,
					Zenith_Maths::Vector3(fRunX, fBy0, fZ0),
					Zenith_Maths::Vector3(fX1, fBy1, fZ1), fTile, 0.0f);
			}
		}
	}

	// ---- WALL --------------------------------------------------------------
	// Plinth, storey body (a core slab plus two panelised facade slabs with REAL
	// openings), string courses between storeys, corner quoins, and the dark
	// room-behind card in every window pocket.
	//
	// ★ THE OPENINGS ARE HOLES. Version 3 kept the body one solid box and glued
	// the pane to its face, which read as a decal from any angle but head-on. The
	// facade slab is fZM_BUILDING_REVEAL_DEPTH deep and is cut into panels around
	// every window and the door; the trim lines the cut with a reveal, the glass
	// sits at the back of it, and the card behind the glass is this mesh, tinted
	// near-black by vertex colour so the pocket reads as a dark room rather than
	// as plaster.
	void ZM_BuildWallMesh(const ZM_BuildingRecipe& xR, const ZM_BuildingShellMetrics& xM,
		ZM_GenMesh& xMesh)
	{
		const float fTile = ZM_BuildingSurfaceTileMetresFor(xR, ZM_BUILDING_SURFACE_WALL);
		const float fPlinthTop = fZM_BUILDING_PLINTH_HEIGHT;
		const float fPP = fZM_BUILDING_PLINTH_PROUD;
		const float fR  = fZM_BUILDING_REVEAL_DEPTH;

		// Plinth: a proud course at the base, grounded at y=0 (feet-on-floor,
		// matching the human bind convention and every other generated asset).
		// Cut around the door pocket, which runs down to the ground.
		if (ZM_DoorFits(xM))
		{
			const float fDW = xM.m_fDoorWidth * 0.5f;
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-xM.m_fHalfW - fPP, 0.0f, -xM.m_fHalfD - fPP),
				Zenith_Maths::Vector3(-fDW, fPlinthTop, xM.m_fHalfD + fPP), fTile);
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(fDW, 0.0f, -xM.m_fHalfD - fPP),
				Zenith_Maths::Vector3(xM.m_fHalfW + fPP, fPlinthTop, xM.m_fHalfD + fPP), fTile);
			// The piece behind the pocket: chamfer-free, its front is the pocket's
			// own back wall and abuts the two side pieces.
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-fDW, 0.0f, -xM.m_fHalfD + fR),
				Zenith_Maths::Vector3(fDW, fPlinthTop, xM.m_fHalfD + fPP), fTile, 0.0f);
		}
		else
		{
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-xM.m_fHalfW - fPP, 0.0f, -xM.m_fHalfD - fPP),
				Zenith_Maths::Vector3( xM.m_fHalfW + fPP, fPlinthTop, xM.m_fHalfD + fPP),
				fTile);
		}

		// Storey body. The CORE slab spans the depth between the two pockets; the
		// two FACADE slabs are panelised around their openings.
		ZM_AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-xM.m_fHalfW, fPlinthTop, -xM.m_fHalfD + fR),
			Zenith_Maths::Vector3( xM.m_fHalfW, xM.m_fWallTop, xM.m_fHalfD - fR),
			fTile, 0.0f);
		for (u_int uFace = 0u; uFace < 2u; ++uFace)
		{
			const bool bMinusZ = (uFace == 0u);
			const ZM_FacadeBand xB = ZM_FacadeBandOf(xM, bMinusZ);
			ZM_WindowRect axHoles[uZM_MAX_FACADE_OPENINGS];
			const u_int uHoles = ZM_CollectOpenings(xM, bMinusZ, axHoles, uZM_MAX_FACADE_OPENINGS);
			ZM_EmitPanelisedSlab(xMesh, -xM.m_fHalfW, xM.m_fHalfW, fPlinthTop, xM.m_fWallTop,
				xB.Min(), xB.Max(), axHoles, uHoles, fTile);
		}

		// String course at each storey division (never at the eave -- the fascia
		// owns that line).
		for (u_int s = 1u; s < xM.m_uStoreys; ++s)
		{
			const float fY = xM.m_fStoreyHeight * static_cast<float>(s);
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-xM.m_fHalfW - fPP * 0.6f, fY - 0.08f, -xM.m_fHalfD - fPP * 0.6f),
				Zenith_Maths::Vector3( xM.m_fHalfW + fPP * 0.6f, fY + 0.08f,  xM.m_fHalfD + fPP * 0.6f),
				fTile);
		}

		// Corner quoins: four proud pilasters running plinth-to-eave. They are what
		// stops a big flat wall reading as a single card at a grazing angle.
		const float fQW = fZM_BUILDING_QUOIN_WIDTH;
		const float fQP = fZM_BUILDING_QUOIN_PROUD;
		const float afQX[4] = { -xM.m_fHalfW, xM.m_fHalfW - fQW, -xM.m_fHalfW, xM.m_fHalfW - fQW };
		const float afQZ[4] = { -xM.m_fHalfD, -xM.m_fHalfD, xM.m_fHalfD - fQW, xM.m_fHalfD - fQW };
		for (u_int q = 0u; q < 4u; ++q)
		{
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(afQX[q] - fQP, fPlinthTop, afQZ[q] - fQP),
				Zenith_Maths::Vector3(afQX[q] + fQW + fQP, xM.m_fWallTop, afQZ[q] + fQW + fQP),
				fTile);
		}

		// ★ THE SECOND GRIME COLOUR IS A PER-FACE CAST. One wall material serves
		// every face, so "cool green-black on the sun-averted faces, warm dust on
		// the lit ones" cannot be a texel choice -- it is a vertex tint. The +Z
		// (back) and -X faces are the sun-averted ones by the map's convention (the
		// sun tracks the -Z/+X quarter in every Zenithmon outdoor scene); they get
		// a faint cool cast that the texture's own biological grime reads through,
		// the others stay neutral. Horizontal faces (ledges) get a touch of dust.
		for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector3& xN = xMesh.m_xNormals.Get(v);
			if (xN.z > 0.5f || xN.x < -0.5f)
			{
				xMesh.m_xColors.Get(v) = Zenith_Maths::Vector4(0.90f, 0.94f, 0.91f, 1.0f);
			}
			else if (xN.y > 0.5f)
			{
				xMesh.m_xColors.Get(v) = Zenith_Maths::Vector4(0.97f, 0.96f, 0.93f, 1.0f);
			}
		}

		// The room-behind cards: one dark card just off the back of every window
		// pocket, behind the glass. Tinted AFTER the face pass so the cast above
		// does not touch them.
		for (u_int uFace = 0u; uFace < 2u; ++uFace)
		{
			const bool bMinusZ = (uFace == 0u);
			const ZM_FacadeBand xB = ZM_FacadeBandOf(xM, bMinusZ);
			const float fZa = xB.FromBack(fZM_BUILDING_ROOM_CARD_GAP);
			const float fZb = xB.FromBack(fZM_BUILDING_ROOM_CARD_GAP + fZM_BUILDING_ROOM_CARD_THICK);
			const float fL = fZM_BUILDING_REVEAL_LINING;
			for (u_int r = 0u; r < xM.m_uWindowRows; ++r)
			{
				for (u_int c = 0u; c < xM.m_uWindowCols; ++c)
				{
					ZM_WindowRect xW;
					if (!ZM_WindowExists(xM, bMinusZ, c, r, xW)) { continue; }
					const u_int uFirst = xMesh.GetNumVerts();
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0 + fL, xW.m_fY0 + fL, fZa < fZb ? fZa : fZb),
						Zenith_Maths::Vector3(xW.m_fX1 - fL, xW.m_fY1 - fL, fZa < fZb ? fZb : fZa),
						fTile, 0.0f);
					ZM_TintFrom(xMesh, uFirst, Zenith_Maths::Vector4(0.05f, 0.05f, 0.06f, 1.0f));
				}
			}
		}
	}

	// ---- ROOF --------------------------------------------------------------

	// Per-course jitter in [-1,1], a pure hash of (building, facet, course) so
	// the course count -- which varies per building -- never touches the RNG
	// stream (a variable draw count is the one thing the hoisted-draw rule
	// forbids).
	float ZM_CourseJitter(u_int uSalt, u_int uFacet, u_int uCourse)
	{
		return ZM_SynthTexHash01(uCourse, uFacet, uSalt + 7919u) * 2.0f - 1.0f;
	}

	// Lay slate courses up ONE pitch facet. The facet is the quad (or triangle,
	// when the ridge edge is a point) between the eave edge A->B and the ridge
	// edge A'->B'; P(s, t) interpolates across (t) and up (s). Each course is a
	// strip [s0, s1] pushed out along the facet normal, its lower edge by the lip
	// plus its jitter and its upper edge by the jitter alone (so it tilts down
	// onto the course below), plus a lip face closing the step. Outward is away
	// from xInside.
	void ZM_AppendCoursedPitch(ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xEaveA,
		const Zenith_Maths::Vector3& xEaveB, const Zenith_Maths::Vector3& xRidgeA,
		const Zenith_Maths::Vector3& xRidgeB, const Zenith_Maths::Vector3& xInside,
		u_int uSalt, u_int uFacet)
	{
		Zenith_Maths::Vector3 xN = glm::cross(xEaveB - xEaveA, xRidgeA - xEaveA);
		const float fNL = glm::length(xN);
		if (fNL < 1.0e-8f)
		{
			// A degenerate facet (zero-width eave); the ridge-only case.
			xN = glm::cross(xEaveB - xEaveA, xRidgeB - xEaveA);
		}
		const float fNL2 = glm::length(xN);
		xN = (fNL2 > 1.0e-8f) ? (xN / fNL2) : Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		const Zenith_Maths::Vector3 xMid = (xEaveA + xEaveB + xRidgeA + xRidgeB) * 0.25f;
		if (glm::dot(xN, xMid - xInside) < 0.0f) { xN = -xN; }

		const float fSlope = glm::length((xRidgeA + xRidgeB) * 0.5f - (xEaveA + xEaveB) * 0.5f);
		u_int uCourses = static_cast<u_int>(fSlope / fZM_BUILDING_ROOF_GAUGE + 0.999f);
		if (uCourses < 1u) { uCourses = 1u; }
		const float fLapFrac = (fSlope > 1.0e-6f) ? fZM_BUILDING_ROOF_LAP / fSlope : 0.0f;

		auto xP = [&](float fS, float fT) -> Zenith_Maths::Vector3
		{
			const Zenith_Maths::Vector3 xE = xEaveA  + (xEaveB  - xEaveA)  * fT;
			const Zenith_Maths::Vector3 xRg = xRidgeA + (xRidgeB - xRidgeA) * fT;
			return xE + (xRg - xE) * fS;
		};

		for (u_int c = 0u; c < uCourses; ++c)
		{
			const float fS0 = static_cast<float>(c) / static_cast<float>(uCourses);
			float fS1 = static_cast<float>(c + 1u) / static_cast<float>(uCourses) + fLapFrac;
			if (fS1 > 1.0f) { fS1 = 1.0f; }
			const float fJ   = ZM_CourseJitter(uSalt, uFacet, c) * fZM_BUILDING_ROOF_JITTER;
			const float fLow = fZM_BUILDING_ROOF_LIP + fJ;
			const float fHigh = fJ;

			const Zenith_Maths::Vector3 xBL = xP(fS0, 0.0f) + xN * fLow;
			const Zenith_Maths::Vector3 xBR = xP(fS0, 1.0f) + xN * fLow;
			const Zenith_Maths::Vector3 xTL = xP(fS1, 0.0f) + xN * fHigh;
			const Zenith_Maths::Vector3 xTR = xP(fS1, 1.0f) + xN * fHigh;

			// The course surface. A near-zero upper edge (the apex course of a hip
			// facet) is a triangle, not a quad with a collapsed side -- a zero-area
			// triangle has no winding and fails the outward check.
			if (glm::length(xTR - xTL) < 1.0e-4f)
			{
				ZM_StaticMesh::AppendTri(xMesh, xBL, xBR, xTL, xInside, s_xUnitIsland);
			}
			else
			{
				Zenith_Maths::Vector3 xCN = glm::cross(xBR - xBL, xTL - xBL);
				const float fL = glm::length(xCN);
				xCN = (fL > 1.0e-8f) ? (xCN / fL) : xN;
				if (glm::dot(xCN, xN) < 0.0f) { xCN = -xCN; }
				ZM_StaticMesh::AppendFace(xMesh, xBL, xBR, xTL, xTR, xCN, s_xUnitIsland);
			}

			// The lip: the step from this course's proud lower edge down to the
			// plane of the course below. Faces down-slope and outward -- what a
			// grazing sun rakes across and what draws the course lines from the
			// street.
			const Zenith_Maths::Vector3 xLipL = xP(fS0, 0.0f) - xN * (fZM_BUILDING_ROOF_LIP * 0.25f);
			const Zenith_Maths::Vector3 xLipR = xP(fS0, 1.0f) - xN * (fZM_BUILDING_ROOF_LIP * 0.25f);
			if (glm::length(xBR - xBL) > 1.0e-4f)
			{
				Zenith_Maths::Vector3 xLN = glm::cross(xLipR - xLipL, xBL - xLipL);
				const float fL = glm::length(xLN);
				const Zenith_Maths::Vector3 xDown = xP(0.0f, 0.5f) - xP(1.0f, 0.5f);
				xLN = (fL > 1.0e-8f) ? (xLN / fL) : xDown;
				if (glm::dot(xLN, xDown + xN) < 0.0f) { xLN = -xLN; }
				ZM_StaticMesh::AppendFace(xMesh, xLipL, xLipR, xBL, xBR, xLN, s_xUnitIsland);
			}
		}
	}

	void ZM_BuildRoofMesh(const ZM_BuildingRecipe& xR, const ZM_BuildingShellMetrics& xM,
		ZM_GenMesh& xMesh)
	{
		const float fTile = ZM_BuildingSurfaceTileMetresFor(xR, ZM_BUILDING_SURFACE_ROOF);
		const float x0 = -xM.m_fExW, x1 = xM.m_fExW;
		const float z0 = -xM.m_fExD, z1 = xM.m_fExD;
		const float yT = xM.m_fWallTop;
		const u_int uSalt = xR.m_uSyntheticSeed;
		const Zenith_Maths::Vector3 xInside(0.0f, yT, 0.0f);

		const u_int uFirst = xMesh.GetNumVerts();
		switch (xR.m_eRoof)
		{
		case ZM_ROOF_GABLE:
		{
			const float yA = xM.m_fRidgeY;
			// +Z pitch and -Z pitch, coursed from their eaves to the shared ridge.
			ZM_AppendCoursedPitch(xMesh,
				Zenith_Maths::Vector3(x0, yT, z1), Zenith_Maths::Vector3(x1, yT, z1),
				Zenith_Maths::Vector3(x0, yA, 0.0f), Zenith_Maths::Vector3(x1, yA, 0.0f),
				xInside, uSalt, 0u);
			ZM_AppendCoursedPitch(xMesh,
				Zenith_Maths::Vector3(x1, yT, z0), Zenith_Maths::Vector3(x0, yT, z0),
				Zenith_Maths::Vector3(x1, yA, 0.0f), Zenith_Maths::Vector3(x0, yA, 0.0f),
				xInside, uSalt, 1u);
			// Gable ends close the prism (masonry in a real gable; roof material
			// here so the surface count stays four -- the trim's barge boards
			// edge them).
			ZM_StaticMesh::AppendTri(xMesh, Zenith_Maths::Vector3(x1, yT, z1),
				Zenith_Maths::Vector3(x1, yA, 0.0f), Zenith_Maths::Vector3(x1, yT, z0),
				xInside, s_xUnitIsland);
			ZM_StaticMesh::AppendTri(xMesh, Zenith_Maths::Vector3(x0, yT, z0),
				Zenith_Maths::Vector3(x0, yA, 0.0f), Zenith_Maths::Vector3(x0, yT, z1),
				xInside, s_xUnitIsland);
			// Ridge run: a capped course along the apex.
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(x0, yA - fZM_BUILDING_RIDGE_RISE * 0.6f, -fZM_BUILDING_RIDGE_HALF),
				Zenith_Maths::Vector3(x1, yA + fZM_BUILDING_RIDGE_RISE,         fZM_BUILDING_RIDGE_HALF),
				fTile);
			break;
		}
		case ZM_ROOF_HIP:
		{
			const Zenith_Maths::Vector3 xApex(0.0f, xM.m_fRidgeY, 0.0f);
			const Zenith_Maths::Vector3 c0(x0, yT, z0), c1(x1, yT, z0), c2(x1, yT, z1), c3(x0, yT, z1);
			ZM_AppendCoursedPitch(xMesh, c0, c1, xApex, xApex, xInside, uSalt, 0u);
			ZM_AppendCoursedPitch(xMesh, c1, c2, xApex, xApex, xInside, uSalt, 1u);
			ZM_AppendCoursedPitch(xMesh, c2, c3, xApex, xApex, xInside, uSalt, 2u);
			ZM_AppendCoursedPitch(xMesh, c3, c0, xApex, xApex, xInside, uSalt, 3u);
			// Apex cap.
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-fZM_BUILDING_RIDGE_HALF, xM.m_fRidgeY - fZM_BUILDING_RIDGE_RISE * 0.6f, -fZM_BUILDING_RIDGE_HALF),
				Zenith_Maths::Vector3( fZM_BUILDING_RIDGE_HALF, xM.m_fRidgeY + fZM_BUILDING_RIDGE_RISE,         fZM_BUILDING_RIDGE_HALF),
				fTile);
			break;
		}
		case ZM_ROOF_FLAT:
		default:
			(void)ZM_StaticMesh::AppendFlatRoof(xMesh,
				Zenith_Maths::Vector3(x0, yT, z0), Zenith_Maths::Vector3(x1, yT, z1),
				fZM_BUILDING_PARAPET_HEIGHT, s_xUnitIsland);
			break;
		}
		ZM_StaticMesh::ApplyWorldUVs(xMesh, uFirst, fTile);

		// The chimney's CAP is roof material; its stack is trim. Splitting them is
		// not fussiness -- a brick stack with a stone cap is what a chimney looks
		// like, and the two want different roughness.
		if (xM.m_bHasChimney)
		{
			const float fHalfSide = fZM_BUILDING_CHIMNEY_SIDE * 0.5f;
			// Offset from the ridge so it does not sit on the apex of a hip roof.
			const float fCx = xM.m_fHalfW * 0.45f;
			const float fCz = 0.0f;
			const float fTop = xM.m_fRidgeY + fZM_BUILDING_CHIMNEY_RISE;
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(fCx - fHalfSide - 0.06f, fTop, fCz - fHalfSide - 0.06f),
				Zenith_Maths::Vector3(fCx + fHalfSide + 0.06f, fTop + 0.14f, fCz + fHalfSide + 0.06f),
				fTile);
		}
	}

	// ---- TRIM --------------------------------------------------------------
	// Eave fascia, barge boards, window frames + reveals + glazing bars + sills,
	// door surround + reveal + recessed leaf + threshold + handle, chimney stack.
	void ZM_BuildTrimMesh(const ZM_BuildingRecipe& xR, const ZM_BuildingShellMetrics& xM,
		ZM_GenMesh& xMesh)
	{
		const float fTile = ZM_BuildingSurfaceTileMetresFor(xR, ZM_BUILDING_SURFACE_TRIM);
		const float fFH = fZM_BUILDING_FASCIA_HEIGHT;
		const float fFP = fZM_BUILDING_FASCIA_PROUD;
		const float fL  = fZM_BUILDING_REVEAL_LINING;

		// Eave fascia: a board running the full eave rectangle, just under the roof.
		// One box rather than four so no mitre gap can open at a corner -- and it is
		// a full slab, so its underside IS the soffit that closes the overhang from
		// below and draws the eave shadow line on the wall.
		ZM_AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-xM.m_fExW - fFP, xM.m_fWallTop - fFH, -xM.m_fExD - fFP),
			Zenith_Maths::Vector3( xM.m_fExW + fFP, xM.m_fWallTop,        xM.m_fExD + fFP),
			fTile);

		// Barge boards on a gable: a sloped board along each of the four gable
		// edges, from the eave corner to the ridge, its outer face just outside
		// the roof's end so the course ends are covered.
		if (xR.m_eRoof == ZM_ROOF_GABLE)
		{
			const u_int uBargeFirst = xMesh.GetNumVerts();
			for (u_int e = 0u; e < 2u; ++e)
			{
				const float fSx = (e == 0u) ? 1.0f : -1.0f;
				const float fX  = fSx * xM.m_fExW;
				for (u_int p = 0u; p < 2u; ++p)
				{
					const float fZ = (p == 0u) ? -xM.m_fExD : xM.m_fExD;
					const Zenith_Maths::Vector3 xEave(fX, xM.m_fWallTop + fZM_BUILDING_ROOF_LIP, fZ);
					const Zenith_Maths::Vector3 xRidge(fX, xM.m_fRidgeY + fZM_BUILDING_ROOF_LIP, 0.0f);
					ZM_StaticMesh::AppendParallelepiped(xMesh,
						xEave - Zenith_Maths::Vector3(0.0f, fZM_BUILDING_BARGE_DROP, 0.0f)
							- Zenith_Maths::Vector3(fSx * 0.01f, 0.0f, 0.0f),
						xRidge - xEave,
						Zenith_Maths::Vector3(fSx * (fZM_BUILDING_BARGE_THICK + 0.01f), 0.0f, 0.0f),
						Zenith_Maths::Vector3(0.0f, fZM_BUILDING_BARGE_DROP, 0.0f),
						s_xUnitIsland);
				}
			}
			ZM_StaticMesh::ApplyWorldUVs(xMesh, uBargeFirst, fTile);
		}

		// Windows: frame, reveal lining, glazing bars and sill, on both facades.
		const float fFT = fZM_BUILDING_FRAME_THICK;
		const float fFR = fZM_BUILDING_FRAME_PROUD;
		for (u_int uFace = 0u; uFace < 2u; ++uFace)
		{
			const bool  bMinusZ = (uFace == 0u);
			const ZM_FacadeBand xB = ZM_FacadeBandOf(xM, bMinusZ);
			// Frame: from the wall plane out by the frame's proud.
			const float fFz0 = xB.m_fOuter < xB.FromWall(fFR) ? xB.m_fOuter : xB.FromWall(fFR);
			const float fFz1 = xB.m_fOuter < xB.FromWall(fFR) ? xB.FromWall(fFR) : xB.m_fOuter;
			// Reveal lining: from the frame's proud face to the pocket back.
			const float fRz0 = xB.m_fInner < xB.FromWall(fFR) ? xB.m_fInner : xB.FromWall(fFR);
			const float fRz1 = xB.m_fInner < xB.FromWall(fFR) ? xB.FromWall(fFR) : xB.m_fInner;
			// Glazing bars: just in front of the glass.
			const float fGlassFront = xB.FromBack(fZM_BUILDING_GLASS_SETBACK);
			const float fBarOut = xB.FromBack(fZM_BUILDING_GLASS_SETBACK + fZM_BUILDING_GLAZING_BAR_OUT);
			const float fBz0 = fGlassFront < fBarOut ? fGlassFront : fBarOut;
			const float fBz1 = fGlassFront < fBarOut ? fBarOut : fGlassFront;

			for (u_int r = 0u; r < xM.m_uWindowRows; ++r)
			{
				for (u_int c = 0u; c < xM.m_uWindowCols; ++c)
				{
					ZM_WindowRect xW;
					if (!ZM_WindowExists(xM, bMinusZ, c, r, xW)) { continue; }

					// Four frame members (left, right, head, sill-under-frame),
					// emitted in a FIXED order, proud of the wall around the hole.
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0 - fFT, xW.m_fY0 - fFT, fFz0),
						Zenith_Maths::Vector3(xW.m_fX0,       xW.m_fY1 + fFT, fFz1), fTile);
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX1,       xW.m_fY0 - fFT, fFz0),
						Zenith_Maths::Vector3(xW.m_fX1 + fFT, xW.m_fY1 + fFT, fFz1), fTile);
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0, xW.m_fY1,       fFz0),
						Zenith_Maths::Vector3(xW.m_fX1, xW.m_fY1 + fFT, fFz1), fTile);
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0, xW.m_fY0 - fFT, fFz0),
						Zenith_Maths::Vector3(xW.m_fX1, xW.m_fY0,       fFz1), fTile);

					// Reveal lining INSIDE the hole: jambs, head, cill. Their outer
					// faces are flush with the frame's proud face (adjacent, never
					// overlapping) and they run to the pocket back.
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0,      xW.m_fY0, fRz0),
						Zenith_Maths::Vector3(xW.m_fX0 + fL, xW.m_fY1, fRz1), fTile);
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX1 - fL, xW.m_fY0, fRz0),
						Zenith_Maths::Vector3(xW.m_fX1,      xW.m_fY1, fRz1), fTile);
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0 + fL, xW.m_fY1 - fL, fRz0),
						Zenith_Maths::Vector3(xW.m_fX1 - fL, xW.m_fY1,      fRz1), fTile);
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0 + fL, xW.m_fY0,      fRz0),
						Zenith_Maths::Vector3(xW.m_fX1 - fL, xW.m_fY0 + fL, fRz1), fTile);

					// Glazing bars: one vertical, one horizontal, crossing the pane
					// just in front of the glass -- a four-light casement. They are
					// the difference between "a window" and "a hole".
					const float fMx = (xW.m_fX0 + xW.m_fX1) * 0.5f;
					const float fMy = (xW.m_fY0 + xW.m_fY1) * 0.5f;
					const float fHb = fZM_BUILDING_GLAZING_BAR * 0.5f;
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(fMx - fHb, xW.m_fY0 + fL, fBz0),
						Zenith_Maths::Vector3(fMx + fHb, xW.m_fY1 - fL, fBz1), fTile);
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0 + fL, fMy - fHb, fBz0),
						Zenith_Maths::Vector3(xW.m_fX1 - fL, fMy + fHb, fBz1), fTile);

					// Projecting stone sill under the whole opening.
					const float fSP = fZM_BUILDING_SILL_PROUD;
					const float fSz0 = xB.m_fOuter < xB.FromWall(fSP) ? xB.m_fOuter : xB.FromWall(fSP);
					const float fSz1 = xB.m_fOuter < xB.FromWall(fSP) ? xB.FromWall(fSP) : xB.m_fOuter;
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0 - fFT * 1.6f,
							xW.m_fY0 - fFT - fZM_BUILDING_SILL_HEIGHT, fSz0),
						Zenith_Maths::Vector3(xW.m_fX1 + fFT * 1.6f, xW.m_fY0 - fFT, fSz1),
						fTile);
				}
			}
		}

		// Door: surround, reveal lining, recessed leaf (or leaves), threshold and
		// handle(s), on the -Z facade only.
		if (ZM_DoorFits(xM))
		{
			const ZM_FacadeBand xB = ZM_FacadeBandOf(xM, true);
			const float fDW = xM.m_fDoorWidth * 0.5f;
			const float fDH = xM.m_fDoorHeight;
			const float fDS = fZM_BUILDING_DOOR_SURROUND;
			const float fDP = fZM_BUILDING_DOOR_PROUD;
			const float fZo = xB.m_fOuter;

			// Surround: two jambs and a head, proud of the wall.
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-fDW - fDS, 0.0f, fZo - fDP),
				Zenith_Maths::Vector3(-fDW,       fDH + fDS, fZo), fTile);
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(fDW,       0.0f, fZo - fDP),
				Zenith_Maths::Vector3(fDW + fDS, fDH + fDS, fZo), fTile);
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-fDW - fDS, fDH, fZo - fDP),
				Zenith_Maths::Vector3( fDW + fDS, fDH + fDS, fZo), fTile);

			// Reveal lining inside the pocket: jambs from the threshold up, and the
			// head. From the surround's proud face to the pocket back.
			const float fRz0 = xB.m_fInner;          // pocket back (less negative)
			const float fRz1 = fZo - fDP;            // surround front (more negative)
			const float fTh = fZM_BUILDING_THRESHOLD_H;
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-fDW,      fTh, fRz1),
				Zenith_Maths::Vector3(-fDW + fL, fDH, fRz0), fTile);
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(fDW - fL, fTh, fRz1),
				Zenith_Maths::Vector3(fDW,      fDH, fRz0), fTile);
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-fDW + fL, fDH - fL, fRz1),
				Zenith_Maths::Vector3( fDW - fL, fDH,      fRz0), fTile);

			// Threshold: the stone step the leaf stands on, filling the pocket floor
			// from the plinth's proud face to the pocket back. It abuts the plinth
			// pieces either side (same front plane, no overlap).
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-fDW, 0.0f, fZo - fZM_BUILDING_PLINTH_PROUD),
				Zenith_Maths::Vector3( fDW, fTh,  xB.m_fInner), fTile);

			// The leaf (or a pair, for an entrance too wide for one), at the BACK of
			// the pocket: its face a full reveal behind the wall plane, its back
			// buried in the core slab.
			const float fLeafFront = xB.FromBack(fZM_BUILDING_DOOR_LEAF_THICK);
			const float fLeafBack  = xB.m_fInner - xB.m_fSign * fZM_BUILDING_DOOR_LEAF_EMBED;
			const float fLz0 = fLeafFront < fLeafBack ? fLeafFront : fLeafBack;
			const float fLz1 = fLeafFront < fLeafBack ? fLeafBack : fLeafFront;
			const float fOpenW = (fDW - fL) * 2.0f;
			const bool bDouble = fOpenW > fZM_BUILDING_DOUBLE_DOOR_MIN;
			const float fGap = bDouble ? fZM_BUILDING_DOOR_LEAF_GAP * 0.5f : 0.0f;
			const u_int uLeaves = bDouble ? 2u : 1u;
			for (u_int l = 0u; l < uLeaves; ++l)
			{
				const float fLx0 = bDouble ? (l == 0u ? -fDW + fL : fGap) : -fDW + fL;
				const float fLx1 = bDouble ? (l == 0u ? -fGap : fDW - fL) : fDW - fL;
				ZM_AppendWorldBox(xMesh,
					Zenith_Maths::Vector3(fLx0, fTh,      fLz0),
					Zenith_Maths::Vector3(fLx1, fDH - fL, fLz1), fTile);

				// Handle: a bar on the leaf's meeting edge (the right edge of a
				// single leaf; the inner edge of each of a pair).
				const float fHx = bDouble
					? (l == 0u ? -fGap - fZM_BUILDING_HANDLE_INSET : fGap + fZM_BUILDING_HANDLE_INSET)
					: fDW - fL - fZM_BUILDING_HANDLE_INSET;
				const float fHz1 = fLeafFront;
				const float fHz0 = fLeafFront + xB.m_fSign * fZM_BUILDING_HANDLE_PROUD;
				ZM_AppendWorldBox(xMesh,
					Zenith_Maths::Vector3(fHx - fZM_BUILDING_HANDLE_SIDE * 0.5f,
						fZM_BUILDING_HANDLE_Y - fZM_BUILDING_HANDLE_LEN * 0.5f, fHz0 < fHz1 ? fHz0 : fHz1),
					Zenith_Maths::Vector3(fHx + fZM_BUILDING_HANDLE_SIDE * 0.5f,
						fZM_BUILDING_HANDLE_Y + fZM_BUILDING_HANDLE_LEN * 0.5f, fHz0 < fHz1 ? fHz1 : fHz0),
					fTile);
			}
		}

		// Chimney stack (the cap is roof material -- see ZM_BuildRoofMesh).
		if (xM.m_bHasChimney)
		{
			const float fHalfSide = fZM_BUILDING_CHIMNEY_SIDE * 0.5f;
			const float fCx = xM.m_fHalfW * 0.45f;
			const float fCz = 0.0f;
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(fCx - fHalfSide, xM.m_fWallTop - 0.2f, fCz - fHalfSide),
				Zenith_Maths::Vector3(fCx + fHalfSide, xM.m_fRidgeY + fZM_BUILDING_CHIMNEY_RISE,
					fCz + fHalfSide),
				fTile);
		}
	}

	// ---- GLASS -------------------------------------------------------------
	// One pane per opening, at the BACK of its reveal: set back from the wall
	// plane by the pocket depth less fZM_BUILDING_GLASS_SETBACK, inside the
	// lining, in front of the dark room card the wall mesh carries.
	void ZM_BuildGlassMesh(const ZM_BuildingRecipe& xR, const ZM_BuildingShellMetrics& xM,
		ZM_GenMesh& xMesh)
	{
		const float fTile = ZM_BuildingSurfaceTileMetresFor(xR, ZM_BUILDING_SURFACE_GLASS);
		if (!ZM_FacadeTakesWindows(xM))
		{
			// ★ NEVER EMPTY. A surface with no geometry would bake a degenerate
			// .zmesh and fail the model load, so a windowless building still gets a
			// single pane -- a fanlight over the door if there is one, otherwise a
			// small pane on the -Z wall. The alternative (a variable submesh count)
			// would make the .zmodel's mesh order depend on the roster row, and
			// every consumer indexes it.
			const float fW = xM.m_fHalfW * 0.20f;
			const float fY = xM.m_fWallTop * 0.55f;
			const float fZo = -xM.m_fHalfD - fZM_BUILDING_GLASS_INSET;
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-fW, fY - 0.25f, fZo - 0.03f),
				Zenith_Maths::Vector3( fW, fY + 0.25f, fZo), fTile);
			return;
		}

		const float fL = fZM_BUILDING_REVEAL_LINING;
		for (u_int uFace = 0u; uFace < 2u; ++uFace)
		{
			const bool  bMinusZ = (uFace == 0u);
			const ZM_FacadeBand xB = ZM_FacadeBandOf(xM, bMinusZ);
			const float fZa = xB.FromBack(fZM_BUILDING_GLASS_SETBACK);
			const float fZb = xB.FromBack(fZM_BUILDING_GLASS_SETBACK - fZM_BUILDING_GLASS_THICK);
			for (u_int r = 0u; r < xM.m_uWindowRows; ++r)
			{
				for (u_int c = 0u; c < xM.m_uWindowCols; ++c)
				{
					ZM_WindowRect xW;
					if (!ZM_WindowExists(xM, bMinusZ, c, r, xW)) { continue; }
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0 + fL, xW.m_fY0 + fL, fZa < fZb ? fZa : fZb),
						Zenith_Maths::Vector3(xW.m_fX1 - fL, xW.m_fY1 - fL, fZa < fZb ? fZb : fZa),
						fTile);
				}
			}
		}
	}
}

void ZM_BuildBuildingSurfaceMesh(const ZM_BuildingRecipe& xR,
	ZM_BUILDING_SURFACE eSurface, ZM_GenMesh& xMesh)
{
	xMesh.Reset();
	const ZM_BuildingShellMetrics xM = ZM_ResolveBuildingShellMetrics(xR);

	switch (eSurface)
	{
	case ZM_BUILDING_SURFACE_WALL:  ZM_BuildWallMesh (xR, xM, xMesh); break;
	case ZM_BUILDING_SURFACE_ROOF:  ZM_BuildRoofMesh (xR, xM, xMesh); break;
	case ZM_BUILDING_SURFACE_TRIM:  ZM_BuildTrimMesh (xR, xM, xMesh); break;
	case ZM_BUILDING_SURFACE_GLASS: ZM_BuildGlassMesh(xR, xM, xMesh); break;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildBuildingSurfaceMesh: %u is not a surface -- emitting the wall",
			(u_int)eSurface);
		ZM_BuildWallMesh(xR, xM, xMesh);
		break;
	}

	// Finalise tangents (needs normals + UVs; static, so NO skin normaliser, NO
	// bones). AppendBox and the roof emitters already wrote hard per-face normals
	// -- do NOT re-run ZM_GenGenerateNormals, which would weld and smooth every
	// hard corner in the building.
	ZM_GenGenerateTangents(xMesh);
}

// ============================================================================
// Texture builders.
//
// ★ EVERY TEXEL IS A PURE FUNCTION OF (u, v, one seed drawn per map). The RNG is
// drawn from ONLY at the top of ZM_BuildBuildingSurfaceTextures, a fixed number
// of times in a fixed order; the per-texel variation comes from an integer hash
// of the texel coordinate. That is not a stylistic choice -- a per-texel RNG draw
// makes the image depend on iteration order, which makes it depend on the
// resolution, which is exactly how a "deterministic" generator stops being one.
//
// ★ AND EVERY MAP TILES. u and v wrap at the image edge by construction (the
// patterns are periodic in texel space), so a repeated wall shows no seam.
// ============================================================================
namespace
{
	// The per-surface course layout. Wall = masonry courses (sized from the tile,
	// which is the storey height), roof = slate rows, trim = plain (grain only),
	// glass = none.
	struct ZM_SurfaceCourseSpec
	{
		u_int m_uRows = 0u, m_uCols = 0u;
		float m_fJoint = 0.0f;
		bool  m_bStagger = false;
		float m_fRelief = 0.0f;   // how deep the joint cuts, in height units
	};

	ZM_SurfaceCourseSpec ZM_SurfaceCourses(ZM_BUILDING_SURFACE eSurface, float fTileMetres)
	{
		switch (eSurface)
		{
		// 250 mm courses of render over block, however tall the storey is. The
		// joint width is in course-fractions, so it is the same 9 mm at 12 or 14
		// courses; at 512^2 on 3 m that is ~1.5 texels, which BC1 keeps.
		case ZM_BUILDING_SURFACE_WALL:
			return { ZM_BuildingWallCourseRows(fTileMetres), ZM_BuildingWallCourseCols(fTileMetres),
			         0.035f, true, 0.55f };
		// 1.5 m of roof over 6 rows is a 250 mm slate lap.
		case ZM_BUILDING_SURFACE_ROOF:  return { 6u, 5u, 0.045f, true,  0.75f };
		case ZM_BUILDING_SURFACE_TRIM:  return { 1u, 1u, 0.0f,   false, 0.0f  };
		case ZM_BUILDING_SURFACE_GLASS: return { 1u, 1u, 0.0f,   false, 0.0f  };
		default:
			return { ZM_BuildingWallCourseRows(fTileMetres), ZM_BuildingWallCourseCols(fTileMetres),
			         0.035f, true, 0.55f };
		}
	}

	// The surface's base colour, before per-texel variation.
	Zenith_Maths::Vector3 ZM_SurfaceBaseColour(const ZM_BuildingRecipe& xR,
		ZM_BUILDING_SURFACE eSurface)
	{
		switch (eSurface)
		{
		case ZM_BUILDING_SURFACE_WALL:
		{
			Zenith_Maths::Vector3 xWall = ZM_BuildingPaletteColour(xR.m_ePalette);
			if (xR.m_eThemeType != ZM_TYPE_NONE)
			{
				const Zenith_Maths::Vector3 xType = ZM_SynthTypePalette(xR.m_eThemeType).m_xBase;
				xWall = ZM_LerpV3(xWall, xType, 0.35f);
			}
			return xWall;
		}
		case ZM_BUILDING_SURFACE_ROOF:
			return ZM_BuildingRoofColour(xR.m_ePalette);
		case ZM_BUILDING_SURFACE_TRIM:
			// Painted timber: a light off-white that reads against every palette,
			// pulled a little toward the wall so it looks chosen rather than applied.
			return ZM_LerpV3(Zenith_Maths::Vector3(0.86f, 0.84f, 0.79f),
				ZM_BuildingPaletteColour(xR.m_ePalette), 0.18f);
		case ZM_BUILDING_SURFACE_GLASS:
			// Dark, slightly cyan: an unlit pane seen from outside is mostly the
			// reflection of the sky plus the darkness of the room behind it.
			return Zenith_Maths::Vector3(0.055f, 0.075f, 0.090f);
		default:
			return Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f);
		}
	}

	// A low-frequency HUE + VALUE drift across the tile: period 2-3, +/-8% value,
	// +/-3% hue (as a rotation of the R/B balance). It is what stops two walls of
	// the same building -- and the eight repeats up and along one wall -- reading
	// as the same stamped square. Pure function of (u, v, salt).
	Zenith_Maths::Vector3 ZM_MacroTint(const Zenith_Maths::Vector3& xCol, float fU, float fV, u_int uSalt)
	{
		// ★ THE VALUE TERM IS PERIOD 3, NOT 2, AND THAT IS NOT ARBITRARY. A period-2
		// wrapping lattice is MIRROR-SYMMETRIC about the tile's midpoint -- the
		// second half is the first half reversed -- so however strong it is, the
		// two halves of the tile have exactly equal means and the repeat is as
		// visible as it was. Period 3 has no such symmetry.
		const float fValue = (ZM_SynthValueNoise(fU, fV, 3u, uSalt + 5u) - 0.5f) * 2.0f;   // [-1,1]
		const float fHue   = (ZM_SynthValueNoise(fU, fV, 2u, uSalt + 11u) - 0.5f) * 2.0f;
		const float fScale = 1.0f + fValue * 0.08f;
		// A +/-3% hue nudge as a red/blue see-saw: warmer where positive, cooler
		// where negative, luminance-neutral to first order.
		const float fWarm = fHue * 0.03f;
		return Zenith_Maths::Vector3(
			xCol.x * fScale * (1.0f + fWarm),
			xCol.y * fScale,
			xCol.z * fScale * (1.0f - fWarm));
	}
}

u_int ZM_BuildingWallCourseRows(float fTileMetres)
{
	const u_int uRows = static_cast<u_int>(fTileMetres / 0.25f + 0.5f);
	return uRows < 2u ? 2u : uRows;
}

u_int ZM_BuildingWallCourseCols(float fTileMetres)
{
	const u_int uCols = static_cast<u_int>(fTileMetres / 0.50f + 0.5f);
	return uCols < 2u ? 2u : uCols;
}

ZM_BuildingWeatherTerms ZM_BuildingWallWeatherAt(float fU, float fV, float fTileMetres,
	u_int uSalt, float fDripDensity)
{
	ZM_BuildingWeatherTerms xT;
	// World height within the storey. v = 0 is the storey floor line: the plinth
	// top on the ground storey, the string course above.
	const float fY = fV * fTileMetres;
	const float fPlinthTop = fZM_BUILDING_PLINTH_HEIGHT;

	// (c) Splash-back: 1 at the plinth top, gone fZM_BUILDING_SPLASH_HEIGHT up.
	// Also 1 on the plinth itself, which is what gets rained on hardest.
	if (fY <= fPlinthTop)
	{
		xT.m_fSplash = 1.0f;
	}
	else
	{
		const float fT = (fY - fPlinthTop) / fZM_BUILDING_SPLASH_HEIGHT;
		xT.m_fSplash = ZM_Clamp01f(1.0f - fT);
		xT.m_fSplash *= xT.m_fSplash;   // eases out rather than ending on a line
	}

	// (c) Runoff under the eave / string course: 0 until RUNOFF_HEIGHT below the
	// tile top, 1 at it.
	{
		const float fFromTop = fTileMetres - fY;
		const float fT = 1.0f - fFromTop / fZM_BUILDING_RUNOFF_HEIGHT;
		xT.m_fRunoff = ZM_Clamp01f(fT);
		xT.m_fRunoff *= xT.m_fRunoff;
	}

	// (b) Sill drips: streaks starting under the sill row (the projecting sill's
	// underside, WINDOW_SILL_Y less the frame and the sill stone) and running
	// down DRIP_LENGTH. Streak columns are a hash across U -- fDripDensity of
	// them per tile, each 2-4 cm wide, each with its own length -- because the
	// window pitch is not a multiple of the tile and cannot be anchored in U.
	{
		const float fSillUnder = fZM_BUILDING_WINDOW_SILL_Y - fZM_BUILDING_FRAME_THICK
			- fZM_BUILDING_SILL_HEIGHT;
		if (fY < fSillUnder && fY > fSillUnder - fZM_BUILDING_DRIP_LENGTH)
		{
			const u_int uCount = static_cast<u_int>(fDripDensity + 0.5f);
			float fBest = 0.0f;
			for (u_int d = 0u; d < uCount; ++d)
			{
				const float fCentre = ZM_SynthTexHash01(d, 3u, uSalt + 601u);
				const float fHalfW  = (0.010f + 0.010f * ZM_SynthTexHash01(d, 5u, uSalt + 601u)) / fTileMetres;
				const float fLen    = fZM_BUILDING_DRIP_LENGTH * (0.45f + 0.55f * ZM_SynthTexHash01(d, 7u, uSalt + 601u));
				// Wrapped distance in U.
				float fD = fU - fCentre;
				if (fD > 0.5f)  { fD -= 1.0f; }
				if (fD < -0.5f) { fD += 1.0f; }
				const float fAcross = ZM_Clamp01f(1.0f - (fD < 0.0f ? -fD : fD) / fHalfW);
				const float fDown   = ZM_Clamp01f(1.0f - (fSillUnder - fY) / fLen);
				const float fS = fAcross * fDown * fDown;
				if (fS > fBest) { fBest = fS; }
			}
			// Break the streak's edge with the stretched noise so it is a stain, not
			// a painted bar: stretched 1:8 vertically.
			const float fBreak = 0.6f + 0.4f * ZM_SynthValueNoise(fU * 8.0f, fV * 0.5f, 24u, uSalt + 613u);
			xT.m_fDrip = ZM_Clamp01f(fBest * fBreak);
		}
	}
	return xT;
}

Zenith_Maths::Vector3 ZM_BuildingGrimeColour(bool bBiological)
{
	// ★ MEASURED, NOT EYEBALLED. These two are mixed into a base colour at
	// (grime x 0.55), so whatever separates them is divided by roughly three
	// before it reaches a texel. The shipped pair differs by 0.140 in (G-R) and
	// 0.156 in luma; the previous pair -- (0.30,0.27,0.22) / (0.13,0.17,0.13) --
	// differed by 0.070 and 0.114, which came out as a 0.006 hue difference in the
	// grimiest decile of a warm wall. That is not a second colour, it is rounding.
	//
	// Biological is GREENER AND DARKER: algae in a north-facing corner is nearly
	// black with a green cast. Dust is warmer and lighter: mineral, ochre-biased.
	return bBiological
		? Zenith_Maths::Vector3(0.09f, 0.16f, 0.11f)
		: Zenith_Maths::Vector3(0.36f, 0.29f, 0.19f);
}

float ZM_BuildingWallGrimeBioMixAt(float fU, float fV, u_int uSalt, float fBioShare)
{
	// Period 4 so a patch of growth is roughly a quarter of a tile across -- the
	// scale damp actually collects at -- and +/-0.8 so both ends SATURATE (see
	// the header: at +/-0.4 the mix measured 0.40-0.92 and neither colour was
	// ever seen pure).
	return ZM_Clamp01f(fBioShare
		+ (ZM_SynthValueNoise(fU, fV, 4u, uSalt + 347u) - 0.5f) * 1.6f);
}

ZM_BuildingAlbedoDraws ZM_BuildingAlbedoDrawsFor(const ZM_BuildingRecipe& xR)
{
	// ALL draws up-front, FIXED count and order, BEFORE any surface branch -- so
	// a MESH-seed mutation can never perturb a texel and a new draw can only be
	// APPENDED here. This is the ONLY site that advances the ALBEDO stream for a
	// building.
	ZM_GenRNG xRng = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_ALBEDO);
	ZM_BuildingAlbedoDraws xD;
	xD.m_fJitterR     = xRng.NextFloatRange(-0.04f, 0.04f);
	xD.m_fJitterG     = xRng.NextFloatRange(-0.04f, 0.04f);
	xD.m_fJitterB     = xRng.NextFloatRange(-0.04f, 0.04f);
	xD.m_fWeather     = xRng.NextFloatRange(0.10f, 0.30f);
	xD.m_fRoughJitter = xRng.NextFloatRange(-0.05f, 0.05f);
	xD.m_fBioShare    = xRng.NextFloatRange(0.30f, 0.70f);
	xD.m_fDripDensity = xRng.NextFloatRange(2.0f, 5.0f);
	return xD;
}

ZM_GenImage ZM_BuildBuildingSurfaceHeight(const ZM_BuildingRecipe& xR,
	ZM_BUILDING_SURFACE eSurface)
{
	const u_int uRes = ZM_BuildingSurfaceResolution(eSurface);
	const float fTile = ZM_BuildingSurfaceTileMetresFor(xR, eSurface);
	const ZM_SurfaceCourseSpec xC = ZM_SurfaceCourses(eSurface, fTile);
	// Salt the pattern by the building's own seed so two palettes of one style do
	// not share a brick for brick identical wall.
	const u_int uSalt = xR.m_uSyntheticSeed;

	ZM_GenImage xImg(uRes, uRes);
	const float fInv = 1.0f / static_cast<float>(uRes);
	for (u_int uY = 0u; uY < uRes; ++uY)
	{
		const float fV = (static_cast<float>(uY) + 0.5f) * fInv;
		for (u_int uX = 0u; uX < uRes; ++uX)
		{
			const float fU = (static_cast<float>(uX) + 0.5f) * fInv;

			// Base grain: fine surface roughness, present on every surface. Two
			// scales, because a trowel and a sand grain are not the same size.
			float fH = 0.5f + (ZM_SynthFbm(fU, fV, 16u, uSalt) - 0.5f) * 0.18f
				+ (ZM_SynthValueNoise(fU, fV, 96u, uSalt + 23u) - 0.5f) * 0.06f;

			if (xC.m_uRows > 1u || xC.m_uCols > 1u)
			{
				const ZM_SynthCourseSample xS = ZM_SynthSampleCourses(fU, fV, xC.m_uRows, xC.m_uCols,
					xC.m_fJoint, xC.m_bStagger, uSalt);
				// Joints CUT IN; unit faces vary slightly so a wall is not a grid of
				// identical bricks, and each unit's face has its own gentle dish.
				fH -= xS.m_fJoint * xC.m_fRelief * 0.5f;
				fH += (xS.m_fUnit - 0.5f) * 0.06f;
			}
			else if (eSurface == ZM_BUILDING_SURFACE_TRIM)
			{
				// Timber grain runs along U (the long axis of a board at this tile
				// size), so the noise is stretched hard in that direction.
				fH += (ZM_SynthValueNoise(fU * 0.25f, fV * 6.0f, 12u, uSalt + 7u) - 0.5f) * 0.10f;
			}

			const float fC = ZM_Clamp01f(fH);
			xImg.Set(uY, uX, Zenith_Maths::Vector4(fC, fC, fC, 1.0f));
		}
	}
	return xImg;
}

ZM_BuildingTextureSet ZM_BuildBuildingSurfaceTextures(const ZM_BuildingRecipe& xR,
	ZM_BUILDING_SURFACE eSurface)
{
	// ALBEDO is the SOLE randomness source (a MESH-seed mutation can never perturb
	// a texel -> the domain-isolation unit). Every draw happens ONCE, in
	// ZM_BuildingAlbedoDrawsFor, in a fixed order -- see its comment.
	const ZM_BuildingAlbedoDraws xDraws = ZM_BuildingAlbedoDrawsFor(xR);
	const float fWeather  = xDraws.m_fWeather;
	const float fRoughJit = xDraws.m_fRoughJitter;
	const float fDripDens = xDraws.m_fDripDensity;

	const u_int uRes = ZM_BuildingSurfaceResolution(eSurface);
	const float fTile = ZM_BuildingSurfaceTileMetresFor(xR, eSurface);
	const ZM_SurfaceCourseSpec xC = ZM_SurfaceCourses(eSurface, fTile);
	const ZM_BuildingSurfaceResponse xResp = ZM_BuildingSurfaceMaterialResponse(eSurface);
	const u_int uSalt = xR.m_uSyntheticSeed;

	const Zenith_Maths::Vector3 xBase = ZM_ClampV3(Zenith_Maths::Vector3(
		ZM_SurfaceBaseColour(xR, eSurface).x + xDraws.m_fJitterR,
		ZM_SurfaceBaseColour(xR, eSurface).y + xDraws.m_fJitterG,
		ZM_SurfaceBaseColour(xR, eSurface).z + xDraws.m_fJitterB));

	ZM_BuildingTextureSet xOut;
	xOut.m_xAlbedo = ZM_GenImage(uRes, uRes);

	// ★ THE OTHER THREE MAPS -- AND THE EDGE-WEAR MASK THE ALBEDO READS -- COME
	// FROM ONE SHARED BUILDER. Normal, roughness-metallic, occlusion and wear are
	// all pure derivatives of the SAME height field, and the arithmetic is
	// identical for a wall, a room, a prop, a person and a creature -- so it lives
	// in ZM_SynthBuildPbrSet rather than in five copies that could each drift.
	// Only the height field and the response are ours. Built FIRST so the albedo
	// can lighten and expose exactly the arrises the roughness map polishes.
	const ZM_GenImage xHeight = ZM_BuildBuildingSurfaceHeight(xR, eSurface);
	ZM_SynthPbrResponse xPbr;
	xPbr.m_fRoughness        = xResp.m_fRoughness;
	xPbr.m_fMetallic         = xResp.m_fMetallic;
	xPbr.m_fNormalStrength   = xResp.m_fNormalStrength;
	xPbr.m_fRoughnessJitter  = fRoughJit;
	xPbr.m_fCavityRoughness  = 0.18f;
	xPbr.m_fCavityOcclusion  = 0.55f;
	xPbr.m_bWrap             = true;   // every building map tiles
	xPbr.m_fEdgeWearStrength = xResp.m_fEdgeWear;
	xPbr.m_fEdgeWearRoughness = 0.22f;
	const ZM_SynthPbrSet xSet = ZM_SynthBuildPbrSet(xHeight, xPbr);

	// The two grime colours: warm dust, and the cool green-black of algae and
	// soot that lives where the sun does not reach. Their mix is a slow noise so
	// the same wall carries both -- the per-face vertex cast (ZM_BuildWallMesh)
	// decides which of them a face reads through.
	const Zenith_Maths::Vector3 xDust = ZM_BuildingGrimeColour(false);
	const Zenith_Maths::Vector3 xBio  = ZM_BuildingGrimeColour(true);

	const float fInv = 1.0f / static_cast<float>(uRes);
	for (u_int uY = 0u; uY < uRes; ++uY)
	{
		const float fV = (static_cast<float>(uY) + 0.5f) * fInv;
		for (u_int uX = 0u; uX < uRes; ++uX)
		{
			const float fU = (static_cast<float>(uX) + 0.5f) * fInv;
			const float fH = xHeight.Get(uY, uX).x;
			const float fWear = xSet.m_xEdgeWear.Get(uY, uX).x * xResp.m_fEdgeWear;

			// ---- ALBEDO ----------------------------------------------------
			// (e) The macro tint first, so everything painted over it inherits it.
			Zenith_Maths::Vector3 xCol = ZM_MacroTint(xBase, fU, fV, uSalt);
			if (xC.m_uRows > 1u || xC.m_uCols > 1u)
			{
				const ZM_SynthCourseSample xS = ZM_SynthSampleCourses(fU, fV, xC.m_uRows, xC.m_uCols,
					xC.m_fJoint, xC.m_bStagger, uSalt);
				// Mortar is lighter and greyer than the unit it beds.
				const Zenith_Maths::Vector3 xMortar = ZM_LerpV3(xCol,
					Zenith_Maths::Vector3(0.72f, 0.71f, 0.68f), 0.65f);
				xCol = ZM_LerpV3(xCol, xMortar, xS.m_fJoint);
				// Per-unit tonal variation: the single cheapest thing that stops a
				// tiling masonry texture reading as wallpaper.
				const float fUnitTone = (xS.m_fUnit - 0.5f) * 0.16f * (1.0f - xS.m_fJoint);
				xCol = Zenith_Maths::Vector3(xCol.x + fUnitTone, xCol.y + fUnitTone, xCol.z + fUnitTone);
			}

			// (a) Edge wear on the albedo. Plaster and stone LIGHTEN where they are
			// rubbed (the dirt and patina go first); paint on timber DARKENS,
			// because what shows through is the wood.
			if (eSurface == ZM_BUILDING_SURFACE_TRIM)
			{
				xCol = ZM_LerpV3(xCol, Zenith_Maths::Vector3(0.36f, 0.27f, 0.17f), fWear * 0.55f);
			}
			else
			{
				const Zenith_Maths::Vector3 xLight(xCol.x + 0.18f, xCol.y + 0.17f, xCol.z + 0.15f);
				xCol = ZM_LerpV3(xCol, ZM_LerpV3(xLight, Zenith_Maths::Vector3(0.80f, 0.79f, 0.76f), 0.4f),
					fWear * 0.6f);
			}

			// Grime: cavities everywhere, plus -- on the WALL, where V is world
			// height -- the rows gravity paints: splash-back above the plinth,
			// runoff under the eave, drips under the sills.
			const float fCavity = 1.0f - fH;
			float fGrime = fCavity * fWeather
				+ ZM_SynthFbm(fU, fV, 6u, uSalt + 313u) * fWeather * 0.5f;
			if (eSurface == ZM_BUILDING_SURFACE_WALL)
			{
				const ZM_BuildingWeatherTerms xW = ZM_BuildingWallWeatherAt(fU, fV, fTile, uSalt, fDripDens);
				fGrime += xW.m_fSplash * (0.55f + 0.30f * fWeather)
					+ xW.m_fRunoff * (0.35f + 0.25f * fWeather)
					+ xW.m_fDrip   * 0.45f;
			}
			else if (eSurface == ZM_BUILDING_SURFACE_ROOF)
			{
				// Slate: moss in the laps (the cavity term, doubled) and a slow
				// patchiness across the pitch.
				fGrime += fCavity * fWeather * 0.6f
					+ ZM_SynthValueNoise(fU, fV, 3u, uSalt + 331u) * 0.15f;
			}
			// Worn arrises shed their grime.
			fGrime = ZM_Clamp01f(fGrime * (1.0f - fWear * 0.8f));
			// (d) The grime COLOUR: a slow blend of dust and biology, biased by the
			// building's own draw and saturating at both ends (see the header).
			const float fBioMix = ZM_BuildingWallGrimeBioMixAt(fU, fV, uSalt, xDraws.m_fBioShare);
			const Zenith_Maths::Vector3 xGrime = ZM_LerpV3(xDust, xBio, fBioMix);
			xCol = ZM_LerpV3(xCol, xGrime, fGrime * 0.55f);

			xOut.m_xAlbedo.Set(uY, uX, Zenith_Maths::Vector4(
				ZM_Clamp01f(xCol.x), ZM_Clamp01f(xCol.y), ZM_Clamp01f(xCol.z), 1.0f));
		}
	}

	xOut.m_xNormal            = xSet.m_xNormal;
	xOut.m_xRoughnessMetallic = xSet.m_xRoughnessMetallic;
	xOut.m_xOcclusion         = xSet.m_xOcclusion;
	xOut.m_xHeight            = xHeight;
	return xOut;
}

bool ZM_BuildingTextureSet::Equals(const ZM_BuildingTextureSet& xOther) const
{
	return m_xAlbedo.Equals(xOther.m_xAlbedo)
		&& m_xNormal.Equals(xOther.m_xNormal)
		&& m_xRoughnessMetallic.Equals(xOther.m_xRoughnessMetallic)
		&& m_xOcclusion.Equals(xOther.m_xOcclusion)
		&& m_xHeight.Equals(xOther.m_xHeight);
}

bool ZM_BuildingTextureSet::NonEmpty() const
{
	return !m_xAlbedo.IsEmpty() && !m_xNormal.IsEmpty()
		&& !m_xRoughnessMetallic.IsEmpty() && !m_xOcclusion.IsEmpty()
		&& !m_xHeight.IsEmpty();
}

ZM_SynthDetailPair ZM_BuildBuildingMicroDetail()
{
	// ONE pair for every building: salted by a fixed name, never by a building,
	// so the file every material references is the same bytes whichever building
	// happened to bake it.
	return ZM_SynthBuildMicroDetail(uZM_SYNTH_MICRODETAIL_RESOLUTION,
		ZM_GenHashName("BuildingMicroDetail"));
}

bool ZM_BuildingSharedDetailPath(ZM_BUILDING_DETAIL_MAP eMap, char* szOut, u_int uCap)
{
	Zenith_Assert(szOut != nullptr, "ZM_BuildingSharedDetailPath: null output buffer");
	if (szOut == nullptr || uCap == 0u)
	{
		return false;
	}
	szOut[0] = '\0';
	const char* szFile = "MicroDetail_albedo.ztxtr";
	switch (eMap)
	{
	case ZM_BUILDING_DETAIL_ALBEDO: szFile = "MicroDetail_albedo.ztxtr"; break;
	case ZM_BUILDING_DETAIL_NORMAL: szFile = "MicroDetail_normal.ztxtr"; break;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingSharedDetailPath: %u is not a detail map -- answering the albedo",
			(u_int)eMap);
		break;
	}
	const int iN = snprintf(szOut, uCap, "game:Buildings/Shared/%s", szFile);
	return iN >= 0 && static_cast<u_int>(iN) < uCap;
}

ZM_BuildingGlassGlow ZM_BuildingGlassGlowFor(const ZM_BuildingRecipe& xR)
{
	ZM_BuildingGlassGlow xG;
	// The player's own home is lived in: a faint warm glow in the panes so it
	// reads inhabited at dusk. 0.6 x (1.0, 0.85, 0.6) is a lamp seen through a
	// curtain, not a shop window.
	if (xR.m_eId == ZM_BUILDING_PLAYER_HOME)
	{
		xG.m_fIntensity = 0.6f;
	}
	return xG;
}

void ZM_BuildBuilding(ZM_BUILDING_ID eId, ZM_Building& xOut)
{
	const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(eId);

	xOut.m_eId = eId;
	// FIXED ORDER: surface-major, mesh then textures. Part of the determinism
	// contract -- the meshes and the maps draw from different domains, but a
	// reader comparing two bundles walks them in this order.
	for (u_int s = 0u; s < static_cast<u_int>(ZM_BUILDING_SURFACE_COUNT); ++s)
	{
		const ZM_BUILDING_SURFACE eSurface = static_cast<ZM_BUILDING_SURFACE>(s);
		ZM_BuildBuildingSurfaceMesh(xR, eSurface, xOut.m_axMesh[s]);
		xOut.m_axTextures[s] = ZM_BuildBuildingSurfaceTextures(xR, eSurface);
	}
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
	if (xA.m_eId != xB.m_eId) { return false; }
	for (u_int s = 0u; s < static_cast<u_int>(ZM_BUILDING_SURFACE_COUNT); ++s)
	{
		if (!ZM_BuildingMeshEqual(xA.m_axMesh[s], xB.m_axMesh[s]))       { return false; }
		if (!xA.m_axTextures[s].Equals(xB.m_axTextures[s]))              { return false; }
	}
	return true;
}

u_int ZM_BuildingContentHash(const ZM_Building& xBuilding)
{
	u_int uHash = uZM_FNV_OFFSET;
	for (u_int s = 0u; s < static_cast<u_int>(ZM_BUILDING_SURFACE_COUNT); ++s)
	{
		const ZM_GenMesh& xMesh = xBuilding.m_axMesh[s];
		uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xPositions);
		uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xNormals);
		uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xUVs);
		uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xTangents);
		uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xColors);
		uHash = ZM_FnvAccumBuffer(uHash, xMesh.m_xIndices);

		const ZM_BuildingTextureSet& xT = xBuilding.m_axTextures[s];
		const u_int auMapHash[5] = {
			xT.m_xAlbedo.ContentHash(), xT.m_xNormal.ContentHash(),
			xT.m_xRoughnessMetallic.ContentHash(), xT.m_xOcclusion.ContentHash(),
			xT.m_xHeight.ContentHash() };
		uHash = ZM_FnvAccum(uHash, auMapHash, sizeof(auMapHash));
	}
	return uHash;
}

// ============================================================================
// Validation.
// ============================================================================
ZM_BuildingSurfaceValidation ZM_ValidateBuildingSurfaceMesh(const ZM_GenMesh& xMesh,
	float fMaxAbsUVAllowed)
{
	ZM_BuildingSurfaceValidation xV;
	const u_int uVerts = xMesh.GetNumVerts();
	const u_int uIdx   = xMesh.m_xIndices.GetSize();

	// ---- indices -----------------------------------------------------------
	xV.m_bIndicesInRange = (uIdx > 0u) && (uIdx % 3u == 0u);
	for (u_int i = 0u; i < uIdx && xV.m_bIndicesInRange; ++i)
	{
		if (xMesh.m_xIndices.Get(i) >= uVerts) { xV.m_bIndicesInRange = false; }
	}

	// ---- bounds ------------------------------------------------------------
	if (uVerts > 0u)
	{
		const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
		const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);
		constexpr float fEps = 1.0e-4f;
		xV.m_bBoundsNonDegen = (xMax.x - xMin.x) > fEps
			&& (xMax.y - xMin.y) > fEps && (xMax.z - xMin.z) > fEps;
	}

	// ---- winding: cross(C-A, B-A) . faceNormal > 0, the repo-wide rule -----
	xV.m_bWindingOutward = xV.m_bIndicesInRange;
	if (xV.m_bIndicesInRange)
	{
		for (u_int t = 0u; t * 3u < uIdx; ++t)
		{
			const u_int uA = xMesh.m_xIndices.Get(t * 3u + 0u);
			const u_int uB = xMesh.m_xIndices.Get(t * 3u + 1u);
			const u_int uC = xMesh.m_xIndices.Get(t * 3u + 2u);
			const Zenith_Maths::Vector3& xA = xMesh.m_xPositions.Get(uA);
			const Zenith_Maths::Vector3& xB = xMesh.m_xPositions.Get(uB);
			const Zenith_Maths::Vector3& xC = xMesh.m_xPositions.Get(uC);
			const Zenith_Maths::Vector3 xFace = glm::cross(xC - xA, xB - xA);
			const Zenith_Maths::Vector3 xAvgN =
				(xMesh.m_xNormals.Get(uA) + xMesh.m_xNormals.Get(uB) + xMesh.m_xNormals.Get(uC));
			if (glm::dot(xFace, xAvgN) <= 0.0f)
			{
				xV.m_bWindingOutward = false;
				xV.m_uFirstBadTriangle = t;
				break;
			}
		}
	}

	// ---- UVs: finite, and inside the TILE budget (not inside [0,1]) --------
	//
	// ★ THIS IS THE ONE CLAUSE THAT DIFFERS FROM ZM_ValidateGenMeshStatic, and the
	// reason this function exists. A tiling surface legitimately leaves [0,1]; what
	// it must not do is leave the range its own footprint can justify, because that
	// is what a projection bug looks like -- a UV of 1e7 still renders, just as a
	// uniform smear nobody can see in a headless test.
	xV.m_bUVsFiniteAndBounded = true;
	for (u_int v = 0u; v < uVerts; ++v)
	{
		const Zenith_Maths::Vector2& xUV = xMesh.m_xUVs.Get(v);
		if (!std::isfinite(xUV.x) || !std::isfinite(xUV.y))
		{
			xV.m_bUVsFiniteAndBounded = false;
			break;
		}
		const float fA = xUV.x < 0.0f ? -xUV.x : xUV.x;
		const float fB = xUV.y < 0.0f ? -xUV.y : xUV.y;
		if (fA > xV.m_fMaxAbsUV) { xV.m_fMaxAbsUV = fA; }
		if (fB > xV.m_fMaxAbsUV) { xV.m_fMaxAbsUV = fB; }
	}
	if (xV.m_bUVsFiniteAndBounded && xV.m_fMaxAbsUV > fMaxAbsUVAllowed)
	{
		xV.m_bUVsFiniteAndBounded = false;
	}

	// ---- the static contract ----------------------------------------------
	xV.m_bNoSkeleton    = (xMesh.GetNumBones() == 0u);
	xV.m_bNoSkinBuffers = (xMesh.m_xBoneIndices.GetSize() == 0u)
		&& (xMesh.m_xBoneWeights.GetSize() == 0u);

	xV.m_bAllValid = xV.m_bWindingOutward && xV.m_bBoundsNonDegen
		&& xV.m_bIndicesInRange && xV.m_bUVsFiniteAndBounded
		&& xV.m_bNoSkeleton && xV.m_bNoSkinBuffers;
	return xV;
}

ZM_BuildingValidation ZM_ValidateBuilding(const ZM_Building& xBuilding)
{
	ZM_BuildingValidation xV;
	xV.m_bAllValid = true;
	// The UV budget is per TILE, and the wall's tile is the building's own storey
	// height -- so the budget has to be computed from the recipe, not from the
	// nominal table. A building with a 2.91 m jittered storey legitimately reaches
	// |v| = wallTop / 2.91, which the nominal 3.0 would red.
	const bool bKnownId = static_cast<u_int>(xBuilding.m_eId) < static_cast<u_int>(ZM_BUILDING_COUNT);
	const ZM_BuildingRecipe xR = ZM_ResolveBuildingRecipe(
		bKnownId ? xBuilding.m_eId : ZM_BUILDING_HOUSE_COTTAGE_WARM);
	for (u_int s = 0u; s < static_cast<u_int>(ZM_BUILDING_SURFACE_COUNT); ++s)
	{
		const ZM_BUILDING_SURFACE eSurface = static_cast<ZM_BUILDING_SURFACE>(s);
		const ZM_GenMesh& xMesh = xBuilding.m_axMesh[s];

		// The UV budget a surface can justify, and it is an EXACT bound rather than
		// a generous one.
		//
		// ZM_StaticMesh::ApplyWorldUVs writes uv = (world coordinate) / tile on the two axes it
		// projects, so the largest |uv| any vertex can carry is exactly the largest
		// ABSOLUTE coordinate over the mesh divided by the tile size. Anything above
		// that is not a big building, it is a projection bug.
		//
		// ★ IT IS THE ABSOLUTE COORDINATE, NOT HALF THE EXTENT. The first draft
		// used extent/2 on the reasoning that a model is centred on the origin --
		// true in X and Z, and FALSE in Y, where every building is ground-anchored
		// at y=0 and a 3 m wall reaches |v| = 3/tile rather than 1.5/tile. It
		// passed for the wall and roof and red for the trim of every cottage.
		float fBudget = 1.0f;
		if (xMesh.GetNumVerts() > 0u)
		{
			const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
			const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);
			const float afAbs[6] = {
				xMin.x < 0.0f ? -xMin.x : xMin.x, xMax.x < 0.0f ? -xMax.x : xMax.x,
				xMin.y < 0.0f ? -xMin.y : xMin.y, xMax.y < 0.0f ? -xMax.y : xMax.y,
				xMin.z < 0.0f ? -xMin.z : xMin.z, xMax.z < 0.0f ? -xMax.z : xMax.z };
			float fMaxAbs = 0.0f;
			for (u_int a = 0u; a < 6u; ++a)
			{
				if (afAbs[a] > fMaxAbs) { fMaxAbs = afAbs[a]; }
			}
			// The epsilon absorbs float rounding in the divide, nothing more.
			fBudget = fMaxAbs / ZM_BuildingSurfaceTileMetresFor(xR, eSurface) + 1.0e-3f;
		}

		xV.m_axSurface[s] = ZM_ValidateBuildingSurfaceMesh(xMesh, fBudget);
		xV.m_abTexturesNonEmpty[s] = xBuilding.m_axTextures[s].NonEmpty();
		xV.m_bAllValid = xV.m_bAllValid && xV.m_axSurface[s].m_bAllValid
			&& xV.m_abTexturesNonEmpty[s];
	}
	return xV;
}

// ============================================================================
// Asset-path scheme.
// ============================================================================
ZM_BUILDING_ASSET_KIND ZM_BuildingSurfaceAssetKind(ZM_BUILDING_SURFACE eSurface,
	ZM_BUILDING_ASSET_SLOT eSlot)
{
	if (eSurface >= ZM_BUILDING_SURFACE_COUNT || eSlot >= ZM_BUILDING_SLOT_COUNT)
	{
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingSurfaceAssetKind: (%u, %u) is not a "
			"surface/slot pair -- answering (WALL, MESH)", (u_int)eSurface, (u_int)eSlot);
		return static_cast<ZM_BUILDING_ASSET_KIND>(0u);
	}
	return static_cast<ZM_BUILDING_ASSET_KIND>(
		static_cast<u_int>(eSurface) * static_cast<u_int>(ZM_BUILDING_SLOT_COUNT)
		+ static_cast<u_int>(eSlot));
}

ZM_BUILDING_SURFACE ZM_BuildingAssetSurface(ZM_BUILDING_ASSET_KIND eKind)
{
	if (static_cast<u_int>(eKind) >= static_cast<u_int>(ZM_BUILDING_ASSET_MODEL))
	{
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingAssetSurface: kind %u is not per-surface -- "
			"answering WALL", (u_int)eKind);
		return ZM_BUILDING_SURFACE_WALL;
	}
	return static_cast<ZM_BUILDING_SURFACE>(
		static_cast<u_int>(eKind) / static_cast<u_int>(ZM_BUILDING_SLOT_COUNT));
}

ZM_BUILDING_ASSET_SLOT ZM_BuildingAssetSlot(ZM_BUILDING_ASSET_KIND eKind)
{
	if (static_cast<u_int>(eKind) >= static_cast<u_int>(ZM_BUILDING_ASSET_MODEL))
	{
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingAssetSlot: kind %u is not per-surface -- "
			"answering MESH", (u_int)eKind);
		return ZM_BUILDING_SLOT_MESH;
	}
	return static_cast<ZM_BUILDING_ASSET_SLOT>(
		static_cast<u_int>(eKind) % static_cast<u_int>(ZM_BUILDING_SLOT_COUNT));
}

bool ZM_BuildingAssetPath(ZM_BUILDING_ID eId, ZM_BUILDING_ASSET_KIND eKind, char* szOut, u_int uCap)
{
	Zenith_Assert(szOut != nullptr, "ZM_BuildingAssetPath: null output buffer");
	if (szOut == nullptr || uCap == 0u)
	{
		return false;
	}
	szOut[0] = '\0';

	const char* szName = ZM_GetBuildingName(eId);
	char acBase[160];
	int iBase = -1;
	if (eKind == ZM_BUILDING_ASSET_MODEL)
	{
		iBase = snprintf(acBase, sizeof(acBase), "%s.zmodel", szName);
	}
	else
	{
		const char* szSurface = ZM_BuildingSurfaceName(ZM_BuildingAssetSurface(eKind));
		iBase = snprintf(acBase, sizeof(acBase),
			ZM_BuildingSlotFmt(ZM_BuildingAssetSlot(eKind)), szName, szSurface);
	}
	if (iBase < 0 || static_cast<u_int>(iBase) >= sizeof(acBase))
	{
		return false;   // internal basename overflowed acBase -- overflow contract is TOTAL
	}

	const int iN = snprintf(szOut, uCap, "game:Buildings/%s/%s", szName, acBase);
	return iN >= 0 && static_cast<u_int>(iN) < uCap;   // false on truncation/overflow
}

// ============================================================================
// Disk bake (TOOLS ONLY). Writes one static building bundle: per surface a
// .zmesh (via the skeleton-less ZM_GenBakeStaticMesh bridge), four .ztxtr maps
// and a .zmtrl; then ONE .zmodel binding all four (mesh, material) pairs, with
// NO skeleton and NO animations. The first mesh bake creates the
// Buildings/<Name>/ folder (SaveToFile + model Export create no directories), so
// every write after it lands in the right place.
// ============================================================================
#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Collections/Zenith_Vector.h"
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"    // per-family bake guard (check) + stamp (write)
#include <filesystem>
#include <string>

namespace
{
	// Resolve one (surface, slot) ref + its filesystem path in one step. Returns
	// false on a path overflow so the caller can refuse to bake a PARTIAL bundle --
	// a half-written building is worse than an absent one, because the manifest
	// would stamp it.
	bool ZM_ResolveSurfaceAsset(ZM_BUILDING_ID eId, ZM_BUILDING_SURFACE eSurface,
		ZM_BUILDING_ASSET_SLOT eSlot, std::string& strRefOut, std::string& strFsOut)
	{
		char acRef[512];
		if (!ZM_BuildingAssetPath(eId, ZM_BuildingSurfaceAssetKind(eSurface, eSlot),
			acRef, sizeof(acRef)))
		{
			return false;
		}
		strRefOut = acRef;
		strFsOut  = Zenith_AssetRegistry::ResolvePath(strRefOut);
		return true;
	}
}

bool ZM_EnsureBuildingMicroDetailBaked()
{
	char acAlbRef[512], acNrmRef[512];
	if (!ZM_BuildingSharedDetailPath(ZM_BUILDING_DETAIL_ALBEDO, acAlbRef, sizeof(acAlbRef))
		|| !ZM_BuildingSharedDetailPath(ZM_BUILDING_DETAIL_NORMAL, acNrmRef, sizeof(acNrmRef)))
	{
		return false;
	}
	const std::string strAlbFs = Zenith_AssetRegistry::ResolvePath(std::string(acAlbRef));
	const std::string strNrmFs = Zenith_AssetRegistry::ResolvePath(std::string(acNrmRef));

	// Warm: both present non-empty -> a stat and no writes (mirrors
	// ZM_EnsureBuildingBaked; a zero-byte file is what an interrupted write
	// leaves, and it loads as a missing texture while existing as a present one).
	std::error_code xEc;
	const std::filesystem::path xAlb(strAlbFs), xNrm(strNrmFs);
	const bool bAlbOk = std::filesystem::is_regular_file(xAlb, xEc) && !xEc
		&& std::filesystem::file_size(xAlb, xEc) > 0u && !xEc;
	const bool bNrmOk = std::filesystem::is_regular_file(xNrm, xEc) && !xEc
		&& std::filesystem::file_size(xNrm, xEc) > 0u && !xEc;
	if (bAlbOk && bNrmOk)
	{
		return true;
	}

	const ZM_SynthDetailPair xPair = ZM_BuildBuildingMicroDetail();
	if (!xPair.NonEmpty())
	{
		return false;
	}
	// ★ THE DETAIL ALBEDO IS LINEAR DATA, NOT COLOUR. It is an x2 multiplier
	// around mid-grey, so an sRGB-encoded one would overlay the wrong curve and
	// darken every wall it touches -- the same gamma trap the RM/AO split exists
	// for. LinearBC1, never the albedo path.
	bool bOk = ZM_SynthBakeLinearBC1(xPair.m_xAlbedo, strAlbFs.c_str());
	bOk &= ZM_SynthBakeNormalBC5(xPair.m_xNormal, strNrmFs.c_str());
	bOk &= std::filesystem::exists(std::filesystem::path(strAlbFs), xEc);
	bOk &= std::filesystem::exists(std::filesystem::path(strNrmFs), xEc);
	return bOk;
}

bool ZM_BakeBuilding(ZM_BUILDING_ID eId)
{
	// Every wall and roof material this writes REFERENCES the shared pair, so it
	// has to exist first -- a material pointing at an absent texture loads with
	// the slot empty, which silently turns the detail flag off.
	if (!ZM_EnsureBuildingMicroDetailBaked())
	{
		return false;
	}

	ZM_Building xBuilding;
	ZM_BuildBuilding(eId, xBuilding);

	const std::string strName = ZM_GetBuildingName(eId);

	char acModelRef[512];
	if (!ZM_BuildingAssetPath(eId, ZM_BUILDING_ASSET_MODEL, acModelRef, sizeof(acModelRef)))
	{
		return false;
	}
	const std::string strModelFs = Zenith_AssetRegistry::ResolvePath(std::string(acModelRef));

	bool bOk = true;
	// Collected as we go so the .zmodel is written from the SAME refs the meshes
	// and materials were actually saved under, rather than from a second
	// derivation that could disagree.
	Zenith_Vector<std::string> xMeshRefs;
	Zenith_Vector<std::string> xMatRefs;

	for (u_int s = 0u; s < static_cast<u_int>(ZM_BUILDING_SURFACE_COUNT); ++s)
	{
		const ZM_BUILDING_SURFACE eSurface = static_cast<ZM_BUILDING_SURFACE>(s);
		const ZM_BuildingTextureSet& xTex = xBuilding.m_axTextures[s];

		std::string strMeshRef, strMeshFs;
		std::string strAlbRef,  strAlbFs;
		std::string strNrmRef,  strNrmFs;
		std::string strRmRef,   strRmFs;
		std::string strAoRef,   strAoFs;
		std::string strHtRef,   strHtFs;
		std::string strMatRef,  strMatFs;
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_MESH,        strMeshRef, strMeshFs);
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_ALBEDO,      strAlbRef,  strAlbFs);
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_NORMAL,      strNrmRef,  strNrmFs);
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_ROUGH_METAL, strRmRef,   strRmFs);
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_OCCLUSION,   strAoRef,   strAoFs);
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_HEIGHT,     strHtRef,   strHtFs);
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_MATERIAL,    strMatRef,  strMatFs);
		if (!bOk)
		{
			return false;   // a path overflowed; do not bake a partial bundle
		}

		bOk &= ZM_GenBakeStaticMesh(xBuilding.m_axMesh[s], strMeshFs.c_str());
		// ALBEDO is colour (sRGB-encoded into BC1); the other three are DATA and
		// must stay linear or the shader reads a gamma-curved roughness.
		bOk &= ZM_SynthBakeAlbedoBC1(xTex.m_xAlbedo,            strAlbFs.c_str());
		bOk &= ZM_SynthBakeNormalBC5(xTex.m_xNormal,            strNrmFs.c_str());
		bOk &= ZM_SynthBakeLinearBC1(xTex.m_xRoughnessMetallic, strRmFs.c_str());
		bOk &= ZM_SynthBakeLinearBC1(xTex.m_xOcclusion,         strAoFs.c_str());
		// ★ THE HEIGHT FIELD IS DATA TOO, and it is the one map whose gamma would
		// be invisible: an sRGB-curved height still marches, just to the wrong
		// depth everywhere, which reads as "the POM is a bit weak" rather than as
		// a bug.
		bOk &= ZM_SynthBakeLinearBC1(xTex.m_xHeight,            strHtFs.c_str());

		// Material (.zmtrl v5). Create<>()+GetDirect() keeps the asset alive across
		// SaveToFile (never a stack object). Every map is passed as a "game:" ref
		// (stored as a path, NOT loaded now).
		{
			const ZM_BuildingSurfaceResponse xResp = ZM_BuildingSurfaceMaterialResponse(eSurface);
			Zenith_AssetHandle<Zenith_MaterialAsset> xMat =
				Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxMat = xMat.GetDirect();
			pxMat->SetName(strName + "_" + ZM_BuildingSurfaceName(eSurface));
			pxMat->SetDiffuseTexture(TextureHandle(strAlbRef));
			pxMat->SetNormalTexture(TextureHandle(strNrmRef));
			pxMat->SetRoughnessMetallicTexture(TextureHandle(strRmRef));
			pxMat->SetOcclusionTexture(TextureHandle(strAoRef));
			// ★ HEIGHT + A NON-ZERO SCALE IS WHAT TURNS POM ON. Flux's
			// BuildMaterialDrawFlags sets MATERIAL_DRAW_FLAG_POM iff a height
			// texture is bound AND m_fHeightScale > 0, so binding one without the
			// other is a silently-inert map (and a scale without a map is a silently
			// inert number).
			pxMat->SetTexture(MATERIAL_TEXTURE_HEIGHT, TextureHandle(strHtRef));
			if (xResp.m_fHeightScale > 0.0f)
			{
				pxMat->SetHeightScale(xResp.m_fHeightScale);
			}
			else
			{
				pxMat->SetHeightScale(0.0f);   // glass: a bound map the shader must not march
			}
			// The SHARED micro-detail pair -- one 512^2 grain overlaid 8-12 times
			// per tile, which is what carries the surface below the 6 mm texel the
			// base maps reach. Detail maps auto-enable on texture presence, so a
			// surface that wants none simply binds none.
			if (xResp.m_fDetailTiling > 0.0f)
			{
				char acDetAlb[512], acDetNrm[512];
				if (ZM_BuildingSharedDetailPath(ZM_BUILDING_DETAIL_ALBEDO, acDetAlb, sizeof(acDetAlb))
					&& ZM_BuildingSharedDetailPath(ZM_BUILDING_DETAIL_NORMAL, acDetNrm, sizeof(acDetNrm)))
				{
					pxMat->SetTexture(MATERIAL_TEXTURE_DETAIL_ALBEDO, TextureHandle(std::string(acDetAlb)));
					pxMat->SetTexture(MATERIAL_TEXTURE_DETAIL_NORMAL, TextureHandle(std::string(acDetNrm)));
					pxMat->SetDetailTiling(Zenith_Maths::Vector2(xResp.m_fDetailTiling, xResp.m_fDetailTiling));
				}
			}
			// A lit window is what makes a house read as somebody's house.
			if (eSurface == ZM_BUILDING_SURFACE_GLASS)
			{
				const ZM_BuildingGlassGlow xGlow = ZM_BuildingGlassGlowFor(ZM_ResolveBuildingRecipe(eId));
				if (xGlow.m_fIntensity > 0.0f)
				{
					pxMat->SetEmissiveColor(xGlow.m_xColour);
					pxMat->SetEmissiveIntensity(xGlow.m_fIntensity);
				}
			}
			// The scalars still matter: they are what the map MODULATES, and they
			// are what a cold boot with an absent bake falls back to.
			pxMat->SetRoughness(xResp.m_fRoughness);
			pxMat->SetMetallic(xResp.m_fMetallic);
			pxMat->SetNormalStrength(xResp.m_fNormalStrength);
			pxMat->SetOcclusionStrength(xResp.m_fOcclusion);
			pxMat->SaveToFile(strMatFs);
		}

		xMeshRefs.PushBack(strMeshRef);
		xMatRefs.PushBack(strMatRef);

		std::error_code xEcSurface;
		bOk &= std::filesystem::exists(std::filesystem::path(strMatFs), xEcSurface);
	}

	// Model (.zmodel v2): FOUR submeshes, each with exactly one material, in
	// SURFACE ORDER. STATIC: NO SetSkeletonPath, NO AddAnimationPath -> the baked
	// model reports HasSkeleton()==false and GetNumAnimations()==0.
	{
		Zenith_AssetHandle<Zenith_ModelAsset> xModel =
			Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		Zenith_ModelAsset* pxModel = xModel.GetDirect();
		pxModel->SetName(strName);
		for (u_int s = 0u; s < xMeshRefs.GetSize(); ++s)
		{
			Zenith_Vector<std::string> xMats;
			xMats.PushBack(xMatRefs.Get(s));
			pxModel->AddMeshByPath(xMeshRefs.Get(s), xMats);
		}
		pxModel->Export(strModelFs.c_str());
	}

	// SaveToFile always returns true and Export is void, so exists() is the real IO
	// signal (mirrors ZM_BakeHuman).
	std::error_code xEc;
	bOk &= std::filesystem::exists(std::filesystem::path(strModelFs), xEc);
	return bOk;
}

namespace
{
	// Are ALL of ONE building's per-model files on disk and non-empty? The same
	// existence-AND-size pair ZM_BakeManifestCheck uses per file: a zero-byte file
	// is what a bake interrupted mid-write leaves, and it loads as a missing model
	// while existing as a present one.
	//
	// This walks [0, KIND_COUNT), so the surface split extended it for free -- 25
	// files per building rather than 4, with nothing here to keep in step.
	bool ZM_BuildingBundlePresent(ZM_BUILDING_ID eId)
	{
		for (u_int k = 0; k < static_cast<u_int>(ZM_BUILDING_ASSET_KIND_COUNT); ++k)
		{
			char acRef[512];
			if (!ZM_BuildingAssetPath(eId, static_cast<ZM_BUILDING_ASSET_KIND>(k),
				acRef, sizeof(acRef)))
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

bool ZM_EnsureBuildingBaked(ZM_BUILDING_ID eId)
{
	// ★ WHY THIS EXISTS. ZM_BakeAllAssets has no shipped caller -- the whole
	// generated library is baked as a side effect of whichever automated test runs
	// first. That was harmless while nothing in a SHIPPED scene referenced a
	// building: the gallery baked what the gallery needed. It stopped being
	// harmless the moment Dawnmere's authoring started writing
	// "game:Buildings/PlayerHome/PlayerHome.zmodel" into a COMMITTED scene file,
	// because an authoring boot that had not happened to run the gallery first
	// would publish a reference to an asset that does not exist and report success.
	//
	// TOTAL on a garbage id, like every other entry point here: ZM_GetBuildingName
	// would answer with a sentinel and we would stat game:Buildings/NONE/... forever.
	if (static_cast<u_int>(eId) >= static_cast<u_int>(ZM_BUILDING_COUNT))
	{
		return false;
	}
	// Warm means the bundle AND the shared micro-detail pair every wall/roof
	// material references. A present bundle beside an absent pair loads with the
	// detail slots empty, which turns the detail flag off silently.
	if (ZM_BuildingBundlePresent(eId) && ZM_EnsureBuildingMicroDetailBaked())
	{
		return true;   // warm -- no build, no writes
	}
	if (!ZM_BakeBuilding(eId))
	{
		return false;
	}
	// RE-ASKED rather than trusting the bake's own bool, mirroring
	// ZM_EnsurePropBaked: several writers report their own status and this is the
	// one check that covers every artifact the same way.
	return ZM_BuildingBundlePresent(eId);
}

bool ZM_BakeAllBuildings()
{
	// Per-family bake guard: skip when the stamp is current + every file present.
	const std::filesystem::path xRoot(GAME_ASSETS_DIR);
	if (ZM_BakeManifestCheck(ZM_ASSET_FAMILY_BUILDINGS, xRoot))
	{
		// The shared pair is NOT one of the manifest's enumerated per-building
		// files, so the family stamp cannot speak for it; ask directly.
		return ZM_EnsureBuildingMicroDetailBaked();
	}
	bool bOk = true;
	const u_int uCount = static_cast<u_int>(ZM_BUILDING_COUNT);
	for (u_int u = 0; u < uCount; ++u)
	{
		bOk &= ZM_BakeBuilding(static_cast<ZM_BUILDING_ID>(u));
	}
	if (bOk)
	{
		bOk &= ZM_WriteBakeManifest(ZM_ASSET_FAMILY_BUILDINGS, xRoot);   // stamp only after a fully-successful bake
	}
	return bOk;
}
#endif   // ZENITH_TOOLS
