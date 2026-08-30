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
	case ZM_BUILDING_SURFACE_WALL:  return 256u;
	case ZM_BUILDING_SURFACE_ROOF:  return 256u;
	case ZM_BUILDING_SURFACE_TRIM:  return 128u;
	// Glass carries no pattern worth resolving: it is a near-uniform pane whose
	// look comes from roughness and the reflection, not from texels.
	case ZM_BUILDING_SURFACE_GLASS: return 64u;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingSurfaceResolution: %u is not a surface -- answering wall's",
			(u_int)eSurface);
		return 256u;
	}
}

float ZM_BuildingSurfaceTileMetres(ZM_BUILDING_SURFACE eSurface)
{
	switch (eSurface)
	{
	// 2 m of masonry per repeat: at 256^2 that is 128 px/m, and the courses stay
	// large enough that the repeat does not read as noise from across a street.
	case ZM_BUILDING_SURFACE_WALL:  return 2.00f;
	case ZM_BUILDING_SURFACE_ROOF:  return 1.50f;
	// Trim members are 0.1-0.2 m thick, so a short tile keeps the grain running
	// along a fascia board rather than smearing one texel down its length.
	case ZM_BUILDING_SURFACE_TRIM:  return 0.75f;
	case ZM_BUILDING_SURFACE_GLASS: return 1.20f;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingSurfaceTileMetres: %u is not a surface -- answering wall's",
			(u_int)eSurface);
		return 2.00f;
	}
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
		break;
	case ZM_BUILDING_SURFACE_ROOF:
		// Slate/tile: slightly tighter than plaster so a grazing sun picks out the
		// courses, which is most of what reads as "roof" at a distance.
		xR.m_fRoughness = 0.72f; xR.m_fMetallic = 0.0f;
		xR.m_fNormalStrength = 1.30f; xR.m_fOcclusion = 1.0f;
		break;
	case ZM_BUILDING_SURFACE_TRIM:
		// Painted timber: the smoothest opaque surface on the building, which is
		// what separates a frame from the wall it is set into.
		xR.m_fRoughness = 0.45f; xR.m_fMetallic = 0.0f;
		xR.m_fNormalStrength = 0.80f; xR.m_fOcclusion = 1.0f;
		break;
	case ZM_BUILDING_SURFACE_GLASS:
		// ★ NOT METALLIC. Glass is a dielectric; its mirror-like behaviour is a
		// low roughness plus Fresnel, and metallic=1 would tint the reflection by
		// the base colour and kill the diffuse entirely. The engine's SSR and IBL
		// supply the reflection -- this only has to stop being matte.
		xR.m_fRoughness = 0.08f; xR.m_fMetallic = 0.0f;
		xR.m_fNormalStrength = 0.25f; xR.m_fOcclusion = 1.0f;
		break;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_BuildingGen] ZM_BuildingSurfaceMaterialResponse: %u is not a surface -- answering wall's",
			(u_int)eSurface);
		xR.m_fRoughness = 0.88f; xR.m_fMetallic = 0.0f;
		xR.m_fNormalStrength = 1.15f; xR.m_fOcclusion = 1.0f;
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
	// the call sites below reading as they did.
	inline void ZM_AppendWorldBox(ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xMin,
		const Zenith_Maths::Vector3& xMax, float fTileMetres)
	{
		(void)ZM_StaticMesh::AppendWorldBox(xMesh, xMin, xMax, fTileMetres);
	}

	// One window's opening rectangle on a facade, in facade-local (X, Y).
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

	// ---- WALL --------------------------------------------------------------
	// Plinth, storey body, string courses between storeys, and corner quoins.
	//
	// ★ THE BODY IS STILL ONE SOLID BOX and the windows are NOT holes in it. A
	// boolean-subtracted opening would need a CSG path this generator does not
	// have, and the interiors are separate scenes -- nothing is ever seen through
	// one of these windows. The glass pane is inset in FRONT of the wall instead,
	// which reads identically from outside and costs four verts.
	void ZM_BuildWallMesh(const ZM_BuildingRecipe& xR, const ZM_BuildingShellMetrics& xM,
		ZM_GenMesh& xMesh)
	{
		const float fTile = ZM_BuildingSurfaceTileMetres(ZM_BUILDING_SURFACE_WALL);
		const float fPlinthTop = fZM_BUILDING_PLINTH_HEIGHT;
		const float fPP = fZM_BUILDING_PLINTH_PROUD;

		// Plinth: a proud course at the base. Grounded at y=0 (feet-on-floor,
		// matching the human bind convention and every other generated asset).
		ZM_AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-xM.m_fHalfW - fPP, 0.0f, -xM.m_fHalfD - fPP),
			Zenith_Maths::Vector3( xM.m_fHalfW + fPP, fPlinthTop, xM.m_fHalfD + fPP),
			fTile);

		// Storey body, from the plinth top to the eave.
		ZM_AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-xM.m_fHalfW, fPlinthTop, -xM.m_fHalfD),
			Zenith_Maths::Vector3( xM.m_fHalfW, xM.m_fWallTop, xM.m_fHalfD),
			fTile);

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

		(void)xR;
	}

	// ---- ROOF --------------------------------------------------------------
	void ZM_BuildRoofMesh(const ZM_BuildingRecipe& xR, const ZM_BuildingShellMetrics& xM,
		ZM_GenMesh& xMesh)
	{
		const float fTile = ZM_BuildingSurfaceTileMetres(ZM_BUILDING_SURFACE_ROOF);
		const Zenith_Maths::Vector3 xEaveMin(-xM.m_fExW, xM.m_fWallTop, -xM.m_fExD);
		const Zenith_Maths::Vector3 xEaveMax( xM.m_fExW, xM.m_fWallTop,  xM.m_fExD);

		u_int uFirst = 0u;
		switch (xR.m_eRoof)
		{
		case ZM_ROOF_GABLE:
			uFirst = ZM_StaticMesh::AppendGableRoof(xMesh, xEaveMin, xEaveMax, xM.m_fRise, s_xUnitIsland);
			break;
		case ZM_ROOF_HIP:
			uFirst = ZM_StaticMesh::AppendHipRoof(xMesh, xEaveMin, xEaveMax, xM.m_fRise, s_xUnitIsland);
			break;
		case ZM_ROOF_FLAT:
		default:
			uFirst = ZM_StaticMesh::AppendFlatRoof(xMesh, xEaveMin, xEaveMax,
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
	// Eave fascia, window frames + sills, door surround + leaf, chimney stack.
	void ZM_BuildTrimMesh(const ZM_BuildingRecipe& xR, const ZM_BuildingShellMetrics& xM,
		ZM_GenMesh& xMesh)
	{
		const float fTile = ZM_BuildingSurfaceTileMetres(ZM_BUILDING_SURFACE_TRIM);
		const float fFH = fZM_BUILDING_FASCIA_HEIGHT;
		const float fFP = fZM_BUILDING_FASCIA_PROUD;

		// Eave fascia: a board running the full eave rectangle, just under the roof.
		// One box rather than four so no mitre gap can open at a corner.
		ZM_AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-xM.m_fExW - fFP, xM.m_fWallTop - fFH, -xM.m_fExD - fFP),
			Zenith_Maths::Vector3( xM.m_fExW + fFP, xM.m_fWallTop,        xM.m_fExD + fFP),
			fTile);

		// Window frames + sills, on the -Z and +Z facades.
		if (ZM_FacadeTakesWindows(xM))
		{
			const float fFT = fZM_BUILDING_FRAME_THICK;
			const float fFR = fZM_BUILDING_FRAME_PROUD;
			for (u_int uFace = 0u; uFace < 2u; ++uFace)
			{
				const bool  bMinusZ = (uFace == 0u);
				const float fZOuter = bMinusZ ? -xM.m_fHalfD : xM.m_fHalfD;
				const float fZ0 = bMinusZ ? fZOuter - fFR : fZOuter;
				const float fZ1 = bMinusZ ? fZOuter : fZOuter + fFR;

				for (u_int r = 0u; r < xM.m_uWindowRows; ++r)
				{
					for (u_int c = 0u; c < xM.m_uWindowCols; ++c)
					{
						const ZM_WindowRect xW = ZM_WindowRectAt(xM, c, r);
						if (bMinusZ && !ZM_WindowClearsTheDoor(xM, xW))
						{
							continue;   // this column stands in the doorway
						}

						// Four frame members (left, right, head, sill-under-frame),
						// emitted in a FIXED order.
						ZM_AppendWorldBox(xMesh,
							Zenith_Maths::Vector3(xW.m_fX0 - fFT, xW.m_fY0 - fFT, fZ0),
							Zenith_Maths::Vector3(xW.m_fX0,       xW.m_fY1 + fFT, fZ1), fTile);
						ZM_AppendWorldBox(xMesh,
							Zenith_Maths::Vector3(xW.m_fX1,       xW.m_fY0 - fFT, fZ0),
							Zenith_Maths::Vector3(xW.m_fX1 + fFT, xW.m_fY1 + fFT, fZ1), fTile);
						ZM_AppendWorldBox(xMesh,
							Zenith_Maths::Vector3(xW.m_fX0, xW.m_fY1,       fZ0),
							Zenith_Maths::Vector3(xW.m_fX1, xW.m_fY1 + fFT, fZ1), fTile);
						ZM_AppendWorldBox(xMesh,
							Zenith_Maths::Vector3(xW.m_fX0, xW.m_fY0 - fFT, fZ0),
							Zenith_Maths::Vector3(xW.m_fX1, xW.m_fY0,       fZ1), fTile);

						// Centre mullion: a single vertical bar. It costs 24 verts
						// and is the difference between "a window" and "a hole".
						const float fMx = (xW.m_fX0 + xW.m_fX1) * 0.5f;
						ZM_AppendWorldBox(xMesh,
							Zenith_Maths::Vector3(fMx - fFT * 0.35f, xW.m_fY0, fZ0),
							Zenith_Maths::Vector3(fMx + fFT * 0.35f, xW.m_fY1, fZ1), fTile);

						// Projecting stone sill under the whole opening.
						const float fSP = fZM_BUILDING_SILL_PROUD;
						const float fSz0 = bMinusZ ? fZOuter - fSP : fZOuter;
						const float fSz1 = bMinusZ ? fZOuter : fZOuter + fSP;
						ZM_AppendWorldBox(xMesh,
							Zenith_Maths::Vector3(xW.m_fX0 - fFT * 1.6f,
								xW.m_fY0 - fFT - fZM_BUILDING_SILL_HEIGHT, fSz0),
							Zenith_Maths::Vector3(xW.m_fX1 + fFT * 1.6f, xW.m_fY0 - fFT, fSz1),
							fTile);
					}
				}
			}
		}

		// Door surround + leaf, on the -Z facade only.
		if (ZM_DoorFits(xM))
		{
			const float fDW = xM.m_fDoorWidth * 0.5f;
			const float fDH = xM.m_fDoorHeight;
			const float fDS = fZM_BUILDING_DOOR_SURROUND;
			const float fDP = fZM_BUILDING_DOOR_PROUD;
			const float fZo = -xM.m_fHalfD;

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
			// The leaf itself: PROUD of the wall by less than the surround is, and
			// buried into it at the back so no face of it is coplanar with the
			// masonry. The step between the surround and the leaf is the reveal --
			// a real ledge casting a real shadow. See the constants' header comment
			// for why "recessed" is not available: the wall has no opening to
			// recess into, and trying put the leaf's outward face exactly on the
			// wall plane, which z-fought on both buildings.
			ZM_AppendWorldBox(xMesh,
				Zenith_Maths::Vector3(-fDW, 0.0f, fZo - fZM_BUILDING_DOOR_LEAF_PROUD),
				Zenith_Maths::Vector3( fDW, fDH,  fZo + fZM_BUILDING_DOOR_LEAF_EMBED), fTile);
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

		(void)xR;
	}

	// ---- GLASS -------------------------------------------------------------
	void ZM_BuildGlassMesh(const ZM_BuildingRecipe& xR, const ZM_BuildingShellMetrics& xM,
		ZM_GenMesh& xMesh)
	{
		const float fTile = ZM_BuildingSurfaceTileMetres(ZM_BUILDING_SURFACE_GLASS);
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

		const float fGI = fZM_BUILDING_GLASS_INSET;
		for (u_int uFace = 0u; uFace < 2u; ++uFace)
		{
			const bool  bMinusZ = (uFace == 0u);
			const float fZOuter = bMinusZ ? -xM.m_fHalfD : xM.m_fHalfD;
			// The pane sits just outside the wall plane and just inside the frame's
			// proud face, so it is visibly recessed without z-fighting the wall.
			const float fZ0 = bMinusZ ? fZOuter - fGI : fZOuter;
			const float fZ1 = bMinusZ ? fZOuter : fZOuter + fGI;

			for (u_int r = 0u; r < xM.m_uWindowRows; ++r)
			{
				for (u_int c = 0u; c < xM.m_uWindowCols; ++c)
				{
					const ZM_WindowRect xW = ZM_WindowRectAt(xM, c, r);
					// The SAME refusal the trim applies, or a pane would be left
					// hanging in the doorway with no frame around it.
					if (bMinusZ && !ZM_WindowClearsTheDoor(xM, xW))
					{
						continue;
					}
					ZM_AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(xW.m_fX0, xW.m_fY0, fZ0),
						Zenith_Maths::Vector3(xW.m_fX1, xW.m_fY1, fZ1), fTile);
				}
			}
		}

		(void)xR;
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
	// The per-surface course layout. Wall = brick courses, roof = slate rows,
	// trim = plain (grain only), glass = none.
	struct ZM_SurfaceCourseSpec
	{
		u_int m_uRows = 0u, m_uCols = 0u;
		float m_fJoint = 0.0f;
		bool  m_bStagger = false;
		float m_fRelief = 0.0f;   // how deep the joint cuts, in height units
	};

	ZM_SurfaceCourseSpec ZM_SurfaceCourses(ZM_BUILDING_SURFACE eSurface)
	{
		switch (eSurface)
		{
		// 2 m of wall over 8 courses is a 250 mm course -- render over block.
		case ZM_BUILDING_SURFACE_WALL:  return { 8u, 4u, 0.035f, true,  0.55f };
		// 1.5 m of roof over 6 rows is a 250 mm slate lap.
		case ZM_BUILDING_SURFACE_ROOF:  return { 6u, 5u, 0.045f, true,  0.75f };
		case ZM_BUILDING_SURFACE_TRIM:  return { 1u, 1u, 0.0f,   false, 0.0f  };
		case ZM_BUILDING_SURFACE_GLASS: return { 1u, 1u, 0.0f,   false, 0.0f  };
		default:                        return { 8u, 4u, 0.035f, true,  0.55f };
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
}

ZM_GenImage ZM_BuildBuildingSurfaceHeight(const ZM_BuildingRecipe& xR,
	ZM_BUILDING_SURFACE eSurface)
{
	const u_int uRes = ZM_BuildingSurfaceResolution(eSurface);
	const ZM_SurfaceCourseSpec xC = ZM_SurfaceCourses(eSurface);
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

			// Base grain: fine surface roughness, present on every surface.
			float fH = 0.5f + (ZM_SynthFbm(fU, fV, 16u, uSalt) - 0.5f) * 0.22f;

			if (xC.m_uRows > 1u || xC.m_uCols > 1u)
			{
				const ZM_SynthCourseSample xS = ZM_SynthSampleCourses(fU, fV, xC.m_uRows, xC.m_uCols,
					xC.m_fJoint, xC.m_bStagger, uSalt);
				// Joints CUT IN; unit faces vary slightly so a wall is not a grid of
				// identical bricks.
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
	// a texel -> the domain-isolation unit). ALL draws up-front, FIXED count and
	// order, BEFORE any surface branch.
	ZM_GenRNG xRng = ZM_MakeGenRNG(xR, ZM_GEN_DOMAIN_ALBEDO);
	const float fJitR      = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fJitG      = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fJitB      = xRng.NextFloatRange(-0.04f, 0.04f);
	const float fWeather   = xRng.NextFloatRange(0.10f, 0.30f);   // grime strength
	const float fRoughJit  = xRng.NextFloatRange(-0.05f, 0.05f);

	const u_int uRes = ZM_BuildingSurfaceResolution(eSurface);
	const ZM_SurfaceCourseSpec xC = ZM_SurfaceCourses(eSurface);
	const ZM_BuildingSurfaceResponse xResp = ZM_BuildingSurfaceMaterialResponse(eSurface);
	const u_int uSalt = xR.m_uSyntheticSeed;

	const Zenith_Maths::Vector3 xBase = ZM_ClampV3(Zenith_Maths::Vector3(
		ZM_SurfaceBaseColour(xR, eSurface).x + fJitR,
		ZM_SurfaceBaseColour(xR, eSurface).y + fJitG,
		ZM_SurfaceBaseColour(xR, eSurface).z + fJitB));

	ZM_BuildingTextureSet xOut;
	xOut.m_xAlbedo            = ZM_GenImage(uRes, uRes);

	const ZM_GenImage xHeight = ZM_BuildBuildingSurfaceHeight(xR, eSurface);
	const float fInv = 1.0f / static_cast<float>(uRes);

	for (u_int uY = 0u; uY < uRes; ++uY)
	{
		const float fV = (static_cast<float>(uY) + 0.5f) * fInv;
		for (u_int uX = 0u; uX < uRes; ++uX)
		{
			const float fU = (static_cast<float>(uX) + 0.5f) * fInv;
			const float fH = xHeight.Get(uY, uX).x;

			// ---- ALBEDO ----------------------------------------------------
			Zenith_Maths::Vector3 xCol = xBase;
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

			// Weathering: grime collects low and in the cavities. fV is the tile's
			// own V, so this is a per-tile gradient rather than a building-height
			// one -- deliberate, since the tile repeats up the wall.
			const float fCavity = 1.0f - fH;
			const float fGrime = ZM_Clamp01f(fCavity * fWeather
				+ ZM_SynthFbm(fU, fV, 6u, uSalt + 313u) * fWeather * 0.5f);
			xCol = ZM_LerpV3(xCol, Zenith_Maths::Vector3(0.20f, 0.19f, 0.17f), fGrime * 0.35f);

			xOut.m_xAlbedo.Set(uY, uX, Zenith_Maths::Vector4(
				ZM_Clamp01f(xCol.x), ZM_Clamp01f(xCol.y), ZM_Clamp01f(xCol.z), 1.0f));

		}
	}

	// ★ THE OTHER THREE MAPS COME FROM ONE SHARED BUILDER. Normal, roughness-
	// metallic and occlusion are all pure derivatives of the SAME height field, and
	// the arithmetic is identical for a wall, a room, a prop, a person and a
	// creature -- so it lives in ZM_SynthBuildPbrSet rather than in five copies
	// that could each drift. Only the height field and the response are ours.
	ZM_SynthPbrResponse xPbr;
	xPbr.m_fRoughness       = xResp.m_fRoughness;
	xPbr.m_fMetallic        = xResp.m_fMetallic;
	xPbr.m_fNormalStrength  = xResp.m_fNormalStrength;
	xPbr.m_fRoughnessJitter = fRoughJit;
	xPbr.m_fCavityRoughness = 0.18f;
	xPbr.m_fCavityOcclusion = 0.55f;
	const ZM_SynthPbrSet xSet = ZM_SynthBuildPbrSet(xHeight, xPbr);
	xOut.m_xNormal            = xSet.m_xNormal;
	xOut.m_xRoughnessMetallic = xSet.m_xRoughnessMetallic;
	xOut.m_xOcclusion         = xSet.m_xOcclusion;
	return xOut;
}

bool ZM_BuildingTextureSet::Equals(const ZM_BuildingTextureSet& xOther) const
{
	return m_xAlbedo.Equals(xOther.m_xAlbedo)
		&& m_xNormal.Equals(xOther.m_xNormal)
		&& m_xRoughnessMetallic.Equals(xOther.m_xRoughnessMetallic)
		&& m_xOcclusion.Equals(xOther.m_xOcclusion);
}

bool ZM_BuildingTextureSet::NonEmpty() const
{
	return !m_xAlbedo.IsEmpty() && !m_xNormal.IsEmpty()
		&& !m_xRoughnessMetallic.IsEmpty() && !m_xOcclusion.IsEmpty();
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
		const u_int auMapHash[4] = {
			xT.m_xAlbedo.ContentHash(), xT.m_xNormal.ContentHash(),
			xT.m_xRoughnessMetallic.ContentHash(), xT.m_xOcclusion.ContentHash() };
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
			fBudget = fMaxAbs / ZM_BuildingSurfaceTileMetres(eSurface) + 1.0e-3f;
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

bool ZM_BakeBuilding(ZM_BUILDING_ID eId)
{
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
		std::string strMatRef,  strMatFs;
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_MESH,        strMeshRef, strMeshFs);
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_ALBEDO,      strAlbRef,  strAlbFs);
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_NORMAL,      strNrmRef,  strNrmFs);
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_ROUGH_METAL, strRmRef,   strRmFs);
		bOk &= ZM_ResolveSurfaceAsset(eId, eSurface, ZM_BUILDING_SLOT_OCCLUSION,   strAoRef,   strAoFs);
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
	if (ZM_BuildingBundlePresent(eId))
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
		return true;   // warm: stamp current + all files present -> skip the family
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
