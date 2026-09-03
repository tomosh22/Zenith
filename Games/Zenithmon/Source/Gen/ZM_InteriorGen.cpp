#include "Zenith.h"

// ============================================================================
// ZM_InteriorGen -- the room-shell generator driver. See the header for the
// architecture + determinism contract. This TU owns: room -> spec + recipe
// resolution, the per-room/per-surface look table, the window / rug tables, the
// nine inward-facing surface mesh builders, the baked vertex occlusion + wear,
// the PBR map builders, the bundle driver, the byte-identity + hash + validation
// + ray-query machinery, the asset-path scheme, and (tools only) the disk bake.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_InteriorGen.h"
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
	constexpr u_int uZM_IG_FNV_OFFSET = 2166136261u;
	constexpr u_int uZM_IG_FNV_PRIME  = 16777619u;

	u_int ZM_IGFnvAccum(u_int uHash, const void* pData, size_t uBytes)
	{
		const u_int8* pByte = static_cast<const u_int8*>(pData);
		for (size_t i = 0; i < uBytes; ++i)
		{
			uHash ^= pByte[i];
			uHash *= uZM_IG_FNV_PRIME;
		}
		return uHash;
	}

	template <typename T>
	u_int ZM_IGFnvAccumBuffer(u_int uHash, const Zenith_Vector<T>& xVec)
	{
		if (xVec.GetSize() == 0u) { return uHash; }
		return ZM_IGFnvAccum(uHash, xVec.GetDataPointer(),
			static_cast<size_t>(xVec.GetSize()) * sizeof(T));
	}

	template <typename T>
	bool ZM_IGBuffersEqual(const Zenith_Vector<T>& xA, const Zenith_Vector<T>& xB)
	{
		if (xA.GetSize() != xB.GetSize()) { return false; }
		if (xA.GetSize() == 0u)           { return true;  }
		return memcmp(xA.GetDataPointer(), xB.GetDataPointer(),
			static_cast<size_t>(xA.GetSize()) * sizeof(T)) == 0;
	}

	inline float ZM_IGClamp01(float f) { return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f); }
	inline float ZM_IGMin(float a, float b) { return a < b ? a : b; }
	inline float ZM_IGMax(float a, float b) { return a > b ? a : b; }
	inline Zenith_Maths::Vector3 ZM_IGLerp3(const Zenith_Maths::Vector3& a,
		const Zenith_Maths::Vector3& b, float t)
	{
		return Zenith_Maths::Vector3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t);
	}

	// How far a visible slab stands off the blockout's inner face. Thin enough to
	// be invisible as a step, thick enough that no face of it is coplanar with the
	// collider box behind it -- the z-fighting lesson from the exterior door.
	constexpr float fZM_IG_SLAB = 0.06f;

	// The cottage's floorboards.
	constexpr float fZM_IG_BOARD_WIDTH   = 0.140f;
	constexpr float fZM_IG_BOARD_GAP     = 0.004f;   // between rows
	constexpr float fZM_IG_BOARD_END_GAP = 0.003f;   // between boards in a row
	constexpr float fZM_IG_BOARD_LIP     = 0.001f;   // one step of top-face offset
	constexpr float fZM_IG_UNDERLAY_TOP  = -0.012f;  // the dark bed the gaps show

	// Per-SLOT file basename pattern (room name, surface name).
	const char* ZM_IGSlotFmt(ZM_INTERIOR_ASSET_SLOT eSlot)
	{
		switch (eSlot)
		{
		case ZM_INTERIOR_SLOT_MESH:        return "%s_%s.zmesh";
		case ZM_INTERIOR_SLOT_ALBEDO:      return "%s_%s_albedo.ztxtr";
		case ZM_INTERIOR_SLOT_NORMAL:      return "%s_%s_normal.ztxtr";
		case ZM_INTERIOR_SLOT_ROUGH_METAL: return "%s_%s_rm.ztxtr";
		case ZM_INTERIOR_SLOT_OCCLUSION:   return "%s_%s_ao.ztxtr";
		case ZM_INTERIOR_SLOT_MATERIAL:    return "%s_%s.zmtrl";
		default:
			Zenith_Assert(false, "ZM_IGSlotFmt: bad slot %u", (u_int)eSlot);
			return "%s_%s.bin";
		}
	}

	// The course lattice each (room, surface) is patterned with.
	struct ZM_IGCourseSpec
	{
		u_int m_uRows = 1u, m_uCols = 1u;
		float m_fJoint = 0.0f;
		bool  m_bStagger = false;
		float m_fRelief = 0.0f;
	};

	ZM_IGCourseSpec ZM_IGCourses(ZM_INTERIOR_ROOM eRoom, ZM_INTERIOR_SURFACE eSurface)
	{
		const bool bHome = (eRoom == ZM_INTERIOR_ROOM_PLAYER_HOME);
		switch (eSurface)
		{
		case ZM_INTERIOR_SURFACE_FLOOR:
			// ★ THE FLOORS ARE THE CLEAREST TELL BETWEEN THE TWO ROOMS. The home is
			// long timber BOARDS -- and since the boards are GEOMETRY now, the map
			// carries only the grain within a board, so the lattice stays but the
			// joint is a hairline. The lab is a square resin TILE grid with a
			// GROUT RECESS: the joint is deeper and wider than it was, which is the
			// "stronger height/normal" answer to a 2 mm recess.
			return bHome ? ZM_IGCourseSpec{ 10u, 1u, 0.006f, true,  0.30f }
			             : ZM_IGCourseSpec{  4u, 4u, 0.024f, false, 0.55f };
		case ZM_INTERIOR_SURFACE_WALL:
			// Home: plaster, no courses at all -- just a fine skim texture. Lab:
			// panel joints on a wide grid.
			return bHome ? ZM_IGCourseSpec{  1u, 1u, 0.0f,   false, 0.0f  }
			             : ZM_IGCourseSpec{  2u, 2u, 0.014f, false, 0.35f };
		case ZM_INTERIOR_SURFACE_CEILING:
			return bHome ? ZM_IGCourseSpec{  1u, 1u, 0.0f,   false, 0.0f  }
			             : ZM_IGCourseSpec{  3u, 3u, 0.016f, false, 0.25f };
		case ZM_INTERIOR_SURFACE_RUG:
			// A woven pile (home) and a studded rubber mat (lab): the rug is
			// object-mapped, so these lattices are per-rug, not per-metre.
			return bHome ? ZM_IGCourseSpec{ 28u, 28u, 0.10f, false, 0.22f }
			             : ZM_IGCourseSpec{ 14u, 26u, 0.12f, false, 0.40f };
		case ZM_INTERIOR_SURFACE_TRIM:
		case ZM_INTERIOR_SURFACE_WINDOW:
		case ZM_INTERIOR_SURFACE_GLASS:
		case ZM_INTERIOR_SURFACE_SKY:
		case ZM_INTERIOR_SURFACE_CURTAIN:
		default:
			return ZM_IGCourseSpec{ 1u, 1u, 0.0f, false, 0.0f };
		}
	}

	// The surfaces the baked occlusion + wear is written into. Glass is seen
	// through and the sky card is emissive; a darkened corner on either would be
	// a smudge on the window.
	bool ZM_IGSurfaceShaded(ZM_INTERIOR_SURFACE eSurface)
	{
		return eSurface != ZM_INTERIOR_SURFACE_GLASS && eSurface != ZM_INTERIOR_SURFACE_SKY;
	}

	// ---- The window and rug tables --------------------------------------------
	//
	// See the header for the placement argument. Every position here is checked
	// against the furniture rows and the corridor by ZM_Tests_InteriorGen.
	const ZM_InteriorWindow s_axHomeWindows[] =
	{
		// Over the bed on the sun wall, and one on the far wall past the chair.
		{ ZM_INTERIOR_WALL_NEG_X, -3.60f, 1.60f, 1.05f, 2.35f },
		{ ZM_INTERIOR_WALL_POS_X,  2.50f, 1.60f, 1.00f, 2.30f },
	};
	const ZM_InteriorWindow s_axLabWindows[] =
	{
		// Two tall lights per side wall, over the benches and clear of the shelf.
		//
		// ★ THE -X PAIR IS NOT MIRRORED ONTO +X, AND THAT IS THE POINT. Each wall
		// is windowed around ITS OWN furniture -- the same reasoning the dressing
		// header uses for the benches ("a bench's back belongs to ITS OWN wall").
		// LabShelf stands 2.0 m tall at z = 2.60 on the -X wall, so the second -X
		// light moved down the room to 6.00 to clear it; the +X wall carries only
		// a 1.0 m barrel there, which passes under a 1.30 m sill, so it keeps the
		// tighter spacing. A window behind a bookcase is a hole in a wall nobody
		// can see, with the sky card and the sunlight behind it.
		{ ZM_INTERIOR_WALL_NEG_X, -3.00f, 2.40f, 1.30f, 3.00f },
		{ ZM_INTERIOR_WALL_NEG_X,  6.00f, 2.40f, 1.30f, 3.00f },
		{ ZM_INTERIOR_WALL_POS_X, -3.00f, 2.40f, 1.30f, 3.00f },
		{ ZM_INTERIOR_WALL_POS_X,  4.40f, 2.40f, 1.30f, 3.00f },
	};
}

// ============================================================================
// Room + surface description tables.
// ============================================================================
const char* ZM_InteriorRoomName(ZM_INTERIOR_ROOM eRoom)
{
	switch (eRoom)
	{
	case ZM_INTERIOR_ROOM_PLAYER_HOME: return "PlayerHome";
	case ZM_INTERIOR_ROOM_PROF_LAB:    return "ProfLab";
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_InteriorGen] ZM_InteriorRoomName: %u is not a room -- answering PlayerHome",
			(u_int)eRoom);
		return "PlayerHome";
	}
}

const char* ZM_InteriorSurfaceName(ZM_INTERIOR_SURFACE eSurface)
{
	switch (eSurface)
	{
	case ZM_INTERIOR_SURFACE_FLOOR:   return "floor";
	case ZM_INTERIOR_SURFACE_WALL:    return "wall";
	case ZM_INTERIOR_SURFACE_CEILING: return "ceiling";
	case ZM_INTERIOR_SURFACE_TRIM:    return "trim";
	case ZM_INTERIOR_SURFACE_WINDOW:  return "window";
	case ZM_INTERIOR_SURFACE_GLASS:   return "glass";
	case ZM_INTERIOR_SURFACE_SKY:     return "sky";
	case ZM_INTERIOR_SURFACE_CURTAIN: return "curtain";
	case ZM_INTERIOR_SURFACE_RUG:     return "rug";
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_InteriorGen] ZM_InteriorSurfaceName: %u is not a surface -- answering floor",
			(u_int)eSurface);
		return "floor";
	}
}

u_int ZM_InteriorSurfaceResolution(ZM_INTERIOR_SURFACE eSurface)
{
	// Tiling surfaces set their density by the tile size in metres, so they stay
	// small; the two object-mapped or close-up surfaces (the boards underfoot and
	// the rug) take the 512 ceiling. The two flat, uniform ones (glass, sky) need
	// almost nothing.
	switch (eSurface)
	{
	case ZM_INTERIOR_SURFACE_FLOOR:   return 512u;
	case ZM_INTERIOR_SURFACE_WALL:    return 256u;
	case ZM_INTERIOR_SURFACE_CEILING: return 128u;
	case ZM_INTERIOR_SURFACE_TRIM:    return 256u;
	case ZM_INTERIOR_SURFACE_WINDOW:  return 256u;
	case ZM_INTERIOR_SURFACE_GLASS:   return 64u;
	case ZM_INTERIOR_SURFACE_SKY:     return 64u;
	case ZM_INTERIOR_SURFACE_CURTAIN: return 256u;
	case ZM_INTERIOR_SURFACE_RUG:     return 512u;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_InteriorGen] ZM_InteriorSurfaceResolution: %u is not a surface",
			(u_int)eSurface);
		return 256u;
	}
}

float ZM_InteriorTileMetres(ZM_INTERIOR_ROOM eRoom, ZM_INTERIOR_SURFACE eSurface)
{
	const bool bHome = (eRoom == ZM_INTERIOR_ROOM_PLAYER_HOME);
	switch (eSurface)
	{
	case ZM_INTERIOR_SURFACE_FLOOR:   return bHome ? 1.20f : 2.40f;
	case ZM_INTERIOR_SURFACE_WALL:    return bHome ? 1.60f : 2.00f;
	case ZM_INTERIOR_SURFACE_CEILING: return bHome ? 2.00f : 2.40f;
	case ZM_INTERIOR_SURFACE_TRIM:    return 0.60f;
	case ZM_INTERIOR_SURFACE_WINDOW:  return 0.60f;
	case ZM_INTERIOR_SURFACE_GLASS:   return 1.00f;
	case ZM_INTERIOR_SURFACE_SKY:     return 1.00f;
	case ZM_INTERIOR_SURFACE_CURTAIN: return 0.80f;
	case ZM_INTERIOR_SURFACE_RUG:     return 1.00f;   // object-mapped; only the UV budget reads this
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_InteriorGen] ZM_InteriorTileMetres: %u is not a surface", (u_int)eSurface);
		return 1.60f;
	}
}

ZM_InteriorRoomSpec ZM_GetInteriorRoomSpec(ZM_INTERIOR_ROOM eRoom)
{
	ZM_InteriorRoomSpec xSpec;
	switch (eRoom)
	{
	case ZM_INTERIOR_ROOM_PROF_LAB:
		xSpec.m_fHalfWidth      = fZM_PROFLAB_HALF_WIDTH;
		xSpec.m_fHalfDepth      = fZM_PROFLAB_HALF_DEPTH;
		xSpec.m_fWallThickness  = fZM_PROFLAB_WALL_THICKNESS;
		xSpec.m_fWallHeight     = fZM_PROFLAB_WALL_HEIGHT;
		xSpec.m_fApertureHalfW  = fZM_PROFLAB_APERTURE_HALF_WIDTH;
		xSpec.m_fApertureHeight = fZM_PROFLAB_APERTURE_HEIGHT;
		break;
	case ZM_INTERIOR_ROOM_PLAYER_HOME:
	default:
		if (eRoom != ZM_INTERIOR_ROOM_PLAYER_HOME)
		{
			Zenith_Error(LOG_CATEGORY_MESH,
				"[ZM_InteriorGen] ZM_GetInteriorRoomSpec: %u is not a room -- answering "
				"PlayerHome's", (u_int)eRoom);
		}
		xSpec.m_fHalfWidth      = fZM_PLAYERHOME_HALF_WIDTH;
		xSpec.m_fHalfDepth      = fZM_PLAYERHOME_HALF_DEPTH;
		xSpec.m_fWallThickness  = fZM_PLAYERHOME_WALL_THICKNESS;
		xSpec.m_fWallHeight     = fZM_PLAYERHOME_WALL_HEIGHT;
		xSpec.m_fApertureHalfW  = fZM_PLAYERHOME_APERTURE_HALF_WIDTH;
		xSpec.m_fApertureHeight = fZM_PLAYERHOME_APERTURE_HEIGHT;
		break;
	}
	return xSpec;
}

ZM_InteriorRecipe ZM_ResolveInteriorRecipe(ZM_INTERIOR_ROOM eRoom)
{
	ZM_InteriorRecipe xR;
	xR.m_eRoom = (eRoom < ZM_INTERIOR_ROOM_COUNT) ? eRoom : ZM_INTERIOR_ROOM_PLAYER_HOME;
	xR.m_uSyntheticSeed = ZM_GenHashName(ZM_InteriorRoomName(xR.m_eRoom));
	for (u_int d = 0u; d < static_cast<u_int>(ZM_GEN_DOMAIN_COUNT); ++d)
	{
		xR.m_aulDomainSeed[d] = ZM_GenDeriveSeed(xR.m_uSyntheticSeed,
			static_cast<u_int>(xR.m_eRoom), uZM_INTERIOR_SYNTHETIC_EVO_STAGE,
			static_cast<ZM_GEN_DOMAIN>(d));
	}
	xR.m_xSpec = ZM_GetInteriorRoomSpec(xR.m_eRoom);
	return xR;
}

ZM_GenRNG ZM_MakeInteriorRNG(const ZM_InteriorRecipe& xR, ZM_GEN_DOMAIN eDomain)
{
	return ZM_GenRNG(xR.m_aulDomainSeed[eDomain]);
}

// ============================================================================
// Windows + rugs.
// ============================================================================
u_int ZM_GetInteriorWindowCount(ZM_INTERIOR_ROOM eRoom)
{
	switch (eRoom)
	{
	case ZM_INTERIOR_ROOM_PLAYER_HOME:
		return static_cast<u_int>(sizeof(s_axHomeWindows) / sizeof(s_axHomeWindows[0]));
	case ZM_INTERIOR_ROOM_PROF_LAB:
		return static_cast<u_int>(sizeof(s_axLabWindows) / sizeof(s_axLabWindows[0]));
	default:
		return 0u;
	}
}

const ZM_InteriorWindow& ZM_GetInteriorWindow(ZM_INTERIOR_ROOM eRoom, u_int uIndex)
{
	const u_int uCount = ZM_GetInteriorWindowCount(eRoom);
	if (uCount == 0u || uIndex >= uCount)
	{
		return s_axHomeWindows[0];
	}
	return (eRoom == ZM_INTERIOR_ROOM_PROF_LAB) ? s_axLabWindows[uIndex] : s_axHomeWindows[uIndex];
}

ZM_InteriorRug ZM_GetInteriorRug(ZM_INTERIOR_ROOM eRoom)
{
	switch (eRoom)
	{
	case ZM_INTERIOR_ROOM_PROF_LAB:
		// A rubber mat along the front of the west bench.
		return ZM_InteriorRug{ -7.00f, -3.00f, 1.40f, 2.60f, 0.008f };
	case ZM_INTERIOR_ROOM_PLAYER_HOME:
	default:
		// A woven rug between the bed and the room's middle, clear of the corridor.
		return ZM_InteriorRug{ -4.60f, -0.90f, 2.00f, 1.40f, 0.012f };
	}
}

// ============================================================================
// The look table.
//
// ★★ THIS TABLE IS THE ANSWER TO ZM-D-176, and it is written to be read as a
// pair. Every row for PLAYER_HOME is warm (R > B) and every row for PROF_LAB is
// cool (B > R), because the property the ruling asks for -- "the bedroom must
// stop reading as the lab" -- is measured by ZM_InteriorTintPixels_Test as a
// red/blue ratio gap. A hue nudge on shared grey blocks reached 0.121 against a
// 0.15 floor; two genuinely different sets of materials clear it with room to
// spare, because the floor, the walls, the ceiling AND the trim all move
// together instead of one tint fighting a shared albedo. The five newer
// surfaces keep the same rule, glass and sky included: a cottage's daylight is
// warmer than a laboratory's, and its glass is old and slightly amber.
// ============================================================================
ZM_InteriorSurfaceLook ZM_GetInteriorSurfaceLook(ZM_INTERIOR_ROOM eRoom,
	ZM_INTERIOR_SURFACE eSurface)
{
	ZM_InteriorSurfaceLook xL;
	const bool bHome = (eRoom == ZM_INTERIOR_ROOM_PLAYER_HOME);

	switch (eSurface)
	{
	case ZM_INTERIOR_SURFACE_FLOOR:
		if (bHome)
		{
			// Waxed oak boards: strongly warm, and the smoothest thing in the room
			// after the window glass, so it catches the lamp.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.42f, 0.26f, 0.13f);
			xL.m_fRoughness = 0.42f; xL.m_fNormalStrength = 1.10f;
		}
		else
		{
			// Poured resin over screed: cool, near-neutral, and glossier still --
			// a lab floor reads clinical because it reflects.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.30f, 0.34f, 0.40f);
			xL.m_fRoughness = 0.28f; xL.m_fNormalStrength = 0.90f;
		}
		break;

	case ZM_INTERIOR_SURFACE_WALL:
		if (bHome)
		{
			// Warm cream plaster. This is the surface that fills most of the frame,
			// so it carries most of the room's colour cast.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.72f, 0.58f, 0.38f);
			xL.m_fRoughness = 0.92f; xL.m_fNormalStrength = 0.85f;
		}
		else
		{
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.44f, 0.52f, 0.62f);
			xL.m_fRoughness = 0.55f; xL.m_fNormalStrength = 0.65f;
		}
		break;

	case ZM_INTERIOR_SURFACE_CEILING:
		if (bHome)
		{
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.78f, 0.70f, 0.56f);
			xL.m_fRoughness = 0.95f; xL.m_fNormalStrength = 0.60f;
		}
		else
		{
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.62f, 0.68f, 0.76f);
			xL.m_fRoughness = 0.88f; xL.m_fNormalStrength = 0.55f;
		}
		break;

	case ZM_INTERIOR_SURFACE_TRIM:
		if (bHome)
		{
			// Stained skirting, cornice, architraves and joists, a shade darker
			// than the boards.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.30f, 0.18f, 0.09f);
			xL.m_fRoughness = 0.38f; xL.m_fNormalStrength = 0.75f;
		}
		else
		{
			// ★ THE ONE METALLIC SURFACE IN EITHER ROOM. Brushed stainless skirting
			// and channel: metallic=1 is CORRECT here in a way it never is on a
			// building facade, because this really is a conductor -- its diffuse
			// should vanish and its reflection should take the base colour's tint.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.58f, 0.62f, 0.68f);
			xL.m_fRoughness = 0.30f; xL.m_fMetallic = 1.0f;
			xL.m_fNormalStrength = 0.50f;
		}
		break;

	case ZM_INTERIOR_SURFACE_WINDOW:
		if (bHome)
		{
			// Painted cream timber: frame, bars, sill, reveal linings and the rail.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.86f, 0.82f, 0.72f);
			xL.m_fRoughness = 0.45f; xL.m_fNormalStrength = 0.70f;
		}
		else
		{
			// Powder-coated steel: a painted dielectric, NOT metallic.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.50f, 0.55f, 0.60f);
			xL.m_fRoughness = 0.35f; xL.m_fNormalStrength = 0.50f;
		}
		break;

	case ZM_INTERIOR_SURFACE_GLASS:
		// ★ TRANSLUCENT. The pane is drawn in the forward translucent pass with
		// the opacity below as its base-colour alpha, so the sky card behind it
		// (and the room reflected in it) show through. Very low roughness: glass
		// is the one thing in either room that should mirror the lamp.
		xL.m_bTranslucent = true;
		xL.m_fNormalStrength = 0.20f;
		if (bHome)
		{
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.88f, 0.86f, 0.78f);   // old amber glass
			xL.m_fRoughness = 0.06f; xL.m_fOpacity = 0.22f;
		}
		else
		{
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.78f, 0.88f, 0.94f);   // float glass, green-blue
			xL.m_fRoughness = 0.04f; xL.m_fOpacity = 0.20f;
		}
		break;

	case ZM_INTERIOR_SURFACE_SKY:
		// ★ EMISSIVE, HDR. The card sits behind the pane inside the wall and IS
		// the daylight the window reads as: a value several times the tonemapper's
		// white, so it blooms the way an overcast sky does through a window and so
		// the auto-exposure treats the window as the brightest thing in the room.
		xL.m_fRoughness = 1.0f; xL.m_fNormalStrength = 0.0f;
		xL.m_fEmissiveIntensity = 5.0f;
		if (bHome)
		{
			xL.m_xBaseColour     = Zenith_Maths::Vector3(1.00f, 0.96f, 0.90f);
			xL.m_xEmissiveColour = Zenith_Maths::Vector3(0.92f, 0.94f, 1.00f);
		}
		else
		{
			xL.m_xBaseColour     = Zenith_Maths::Vector3(0.82f, 0.92f, 1.00f);
			xL.m_xEmissiveColour = Zenith_Maths::Vector3(0.80f, 0.90f, 1.00f);
		}
		break;

	case ZM_INTERIOR_SURFACE_CURTAIN:
		if (bHome)
		{
			// Heavy ochre cotton.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.62f, 0.36f, 0.24f);
			xL.m_fRoughness = 0.95f; xL.m_fNormalStrength = 0.90f;
		}
		else
		{
			// Grey-blue panel blinds.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.40f, 0.46f, 0.54f);
			xL.m_fRoughness = 0.90f; xL.m_fNormalStrength = 0.60f;
		}
		break;

	case ZM_INTERIOR_SURFACE_RUG:
	default:
		if (bHome)
		{
			// A woven red-ochre rug.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.48f, 0.22f, 0.16f);
			xL.m_fRoughness = 0.98f; xL.m_fNormalStrength = 1.00f;
		}
		else
		{
			// A black rubber anti-fatigue mat.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.10f, 0.11f, 0.12f);
			xL.m_fRoughness = 0.85f; xL.m_fNormalStrength = 0.80f;
		}
		break;
	}
	return xL;
}

// ============================================================================
// Mesh builders. Every visible face points INTO the room.
// ============================================================================
namespace
{
	// A world-tiled box cut into cells no larger than fCell on any axis, so the
	// per-vertex shading has somewhere to interpolate. A thin axis (the slab
	// thickness) stays one cell.
	void ZM_IGAppendGridBox(ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xMin,
		const Zenith_Maths::Vector3& xMax, float fTile, float fCell)
	{
		u_int auN[3];
		float afMin[3] = { xMin.x, xMin.y, xMin.z };
		float afMax[3] = { xMax.x, xMax.y, xMax.z };
		for (u_int a = 0u; a < 3u; ++a)
		{
			const float fExtent = afMax[a] - afMin[a];
			auN[a] = (fExtent > fCell) ? static_cast<u_int>(ceilf(fExtent / fCell - 1.0e-4f)) : 1u;
			if (auN[a] == 0u) { auN[a] = 1u; }
		}
		for (u_int ix = 0u; ix < auN[0]; ++ix)
		{
			for (u_int iy = 0u; iy < auN[1]; ++iy)
			{
				for (u_int iz = 0u; iz < auN[2]; ++iz)
				{
					const float fX0 = afMin[0] + (afMax[0] - afMin[0]) * static_cast<float>(ix)      / static_cast<float>(auN[0]);
					const float fX1 = afMin[0] + (afMax[0] - afMin[0]) * static_cast<float>(ix + 1u) / static_cast<float>(auN[0]);
					const float fY0 = afMin[1] + (afMax[1] - afMin[1]) * static_cast<float>(iy)      / static_cast<float>(auN[1]);
					const float fY1 = afMin[1] + (afMax[1] - afMin[1]) * static_cast<float>(iy + 1u) / static_cast<float>(auN[1]);
					const float fZ0 = afMin[2] + (afMax[2] - afMin[2]) * static_cast<float>(iz)      / static_cast<float>(auN[2]);
					const float fZ1 = afMin[2] + (afMax[2] - afMin[2]) * static_cast<float>(iz + 1u) / static_cast<float>(auN[2]);
					ZM_StaticMesh::AppendWorldBox(xMesh,
						Zenith_Maths::Vector3(fX0, fY0, fZ0), Zenith_Maths::Vector3(fX1, fY1, fZ1), fTile);
				}
			}
		}
	}

	// The four wall faces, in the frame each face's builders use: s runs ALONG
	// the wall, y up, and "depth" runs from the inner face INTO the wall.
	enum ZM_IG_WALL_FACE : u_int
	{
		ZM_IG_FACE_BACK,       // -Z: s = x
		ZM_IG_FACE_NEG_X,      // -X: s = z
		ZM_IG_FACE_POS_X,      // +X: s = z
		ZM_IG_FACE_ENTRANCE,   // +Z: s = x
	};

	// The box between depths d0..d1 (into the wall from its inner face; negative
	// is INTO THE ROOM), along-wall s0..s1, height y0..y1.
	void ZM_IGWallBox(ZM_GenMesh& xMesh, const ZM_InteriorRoomSpec& xS, ZM_IG_WALL_FACE eFace,
		float fD0, float fD1, float fS0, float fS1, float fY0, float fY1, float fTile, float fCell)
	{
		const float fX = xS.InnerHalfWidth();
		const float fZ = xS.InnerHalfDepth();
		Zenith_Maths::Vector3 xMin, xMax;
		switch (eFace)
		{
		case ZM_IG_FACE_BACK:
			xMin = Zenith_Maths::Vector3(fS0, fY0, -fZ - fD1);
			xMax = Zenith_Maths::Vector3(fS1, fY1, -fZ - fD0);
			break;
		case ZM_IG_FACE_NEG_X:
			xMin = Zenith_Maths::Vector3(-fX - fD1, fY0, fS0);
			xMax = Zenith_Maths::Vector3(-fX - fD0, fY1, fS1);
			break;
		case ZM_IG_FACE_POS_X:
			xMin = Zenith_Maths::Vector3(fX + fD0, fY0, fS0);
			xMax = Zenith_Maths::Vector3(fX + fD1, fY1, fS1);
			break;
		case ZM_IG_FACE_ENTRANCE:
		default:
			xMin = Zenith_Maths::Vector3(fS0, fY0, fZ + fD0);
			xMax = Zenith_Maths::Vector3(fS1, fY1, fZ + fD1);
			break;
		}
		if (fCell > 0.0f) { ZM_IGAppendGridBox(xMesh, xMin, xMax, fTile, fCell); }
		else              { ZM_StaticMesh::AppendWorldBox(xMesh, xMin, xMax, fTile); }
	}

	// A rectangular hole in a wall face, in (s, y).
	struct ZM_IGHole
	{
		float m_fS0, m_fS1, m_fY0, m_fY1;
	};

	void ZM_IGSortAscending(float* pf, u_int uN)
	{
		for (u_int i = 1u; i < uN; ++i)
		{
			const float f = pf[i];
			u_int j = i;
			while (j > 0u && pf[j - 1u] > f) { pf[j] = pf[j - 1u]; --j; }
			pf[j] = f;
		}
	}

	// The wall slab with its holes cut out: the face is split on every hole
	// edge, each resulting cell is kept unless its centre lies in a hole, and
	// each kept cell is then gridded for the vertex shading.
	void ZM_IGEmitWallFace(ZM_GenMesh& xMesh, const ZM_InteriorRoomSpec& xS, ZM_IG_WALL_FACE eFace,
		float fTile, const ZM_IGHole* pxHoles, u_int uHoles)
	{
		const float fHalf = (eFace == ZM_IG_FACE_NEG_X || eFace == ZM_IG_FACE_POS_X)
			? xS.InnerHalfDepth() : xS.InnerHalfWidth();
		constexpr u_int uMAX_HOLES = 4u;
		Zenith_Assert(uHoles <= uMAX_HOLES, "ZM_IGEmitWallFace: too many holes (%u)", uHoles);
		if (uHoles > uMAX_HOLES) { uHoles = uMAX_HOLES; }

		float afS[2u + 2u * uMAX_HOLES], afY[2u + 2u * uMAX_HOLES];
		u_int uNS = 0u, uNY = 0u;
		afS[uNS++] = -fHalf; afS[uNS++] = fHalf;
		afY[uNY++] = 0.0f;   afY[uNY++] = xS.m_fWallHeight;
		for (u_int h = 0u; h < uHoles; ++h)
		{
			afS[uNS++] = pxHoles[h].m_fS0; afS[uNS++] = pxHoles[h].m_fS1;
			afY[uNY++] = pxHoles[h].m_fY0; afY[uNY++] = pxHoles[h].m_fY1;
		}
		ZM_IGSortAscending(afS, uNS);
		ZM_IGSortAscending(afY, uNY);

		for (u_int i = 0u; i + 1u < uNS; ++i)
		{
			const float fS0 = afS[i], fS1 = afS[i + 1u];
			if (fS1 - fS0 <= 1.0e-4f) { continue; }
			for (u_int j = 0u; j + 1u < uNY; ++j)
			{
				const float fY0 = afY[j], fY1 = afY[j + 1u];
				if (fY1 - fY0 <= 1.0e-4f) { continue; }
				const float fCS = 0.5f * (fS0 + fS1), fCY = 0.5f * (fY0 + fY1);
				bool bInHole = false;
				for (u_int h = 0u; h < uHoles && !bInHole; ++h)
				{
					bInHole = fCS > pxHoles[h].m_fS0 && fCS < pxHoles[h].m_fS1
						&& fCY > pxHoles[h].m_fY0 && fCY < pxHoles[h].m_fY1;
				}
				if (bInHole) { continue; }
				ZM_IGWallBox(xMesh, xS, eFace, 0.0f, fZM_IG_SLAB, fS0, fS1, fY0, fY1, fTile,
					fZM_INTERIOR_SHADING_CELL);
			}
		}
	}

	ZM_IG_WALL_FACE ZM_IGWindowFace(const ZM_InteriorWindow& xW)
	{
		return xW.m_eWall == ZM_INTERIOR_WALL_NEG_X ? ZM_IG_FACE_NEG_X : ZM_IG_FACE_POS_X;
	}

	u_int ZM_IGCollectWindowHoles(ZM_INTERIOR_ROOM eRoom, ZM_INTERIOR_WALL_SIDE eSide,
		ZM_IGHole* pxOut, u_int uCap)
	{
		u_int uN = 0u;
		const u_int uCount = ZM_GetInteriorWindowCount(eRoom);
		for (u_int w = 0u; w < uCount && uN < uCap; ++w)
		{
			const ZM_InteriorWindow& xW = ZM_GetInteriorWindow(eRoom, w);
			if (xW.m_eWall != eSide) { continue; }
			pxOut[uN++] = ZM_IGHole{ xW.Z0(), xW.Z1(), xW.m_fSillY, xW.m_fHeadY };
		}
		return uN;
	}

	// ---- Floor ---------------------------------------------------------------
	void ZM_IGBuildFloor(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_FLOOR);
		const float fX = xS.InnerHalfWidth();
		const float fZ = xS.InnerHalfDepth();

		if (xR.m_eRoom != ZM_INTERIOR_ROOM_PLAYER_HOME)
		{
			// The lab's poured floor: one slab, gridded for the shading. Its TOP
			// face is at exactly y=0 -- the floor plane every spawn marker and
			// every authored body in these scenes is placed against.
			ZM_IGAppendGridBox(xMesh,
				Zenith_Maths::Vector3(-fX, -fZM_IG_SLAB, -fZ),
				Zenith_Maths::Vector3( fX, 0.0f,         fZ), fTile, fZM_INTERIOR_SHADING_CELL);
			return;
		}

		// ★ THE COTTAGE'S BOARDS ARE GEOMETRY. Rows across Z at the board pitch,
		// each row a run of boards along X with hashed lengths (so the ends
		// stagger) and a hashed top-face step of 0, 1 or 2 mm DOWN from y=0 -- so
		// the lips between neighbours are real and catch the lamp, while the
		// walking plane stays exactly y=0 at the highest boards. Between the
		// boards a dark underlay shows through 3-4 mm gaps.
		//
		// ★ NO RNG: the stagger and the lips are integer hashes of (row, board)
		// under the room's seed, so a board's height depends on which board it
		// is and on nothing that came before it.
		const u_int uFirstUnderlay = xMesh.GetNumVerts();
		ZM_IGAppendGridBox(xMesh,
			Zenith_Maths::Vector3(-fX, -fZM_IG_SLAB,        -fZ),
			Zenith_Maths::Vector3( fX, fZM_IG_UNDERLAY_TOP,  fZ), fTile, 3.0f);
		const u_int uEndUnderlay = xMesh.GetNumVerts();
		for (u_int v = uFirstUnderlay; v < uEndUnderlay; ++v)
		{
			// The bed the gaps show is dark before any shading is applied to it.
			xMesh.m_xColors.Get(v) = Zenith_Maths::Vector4(0.22f, 0.20f, 0.18f, 1.0f);
		}

		const float fPitch = fZM_IG_BOARD_WIDTH + fZM_IG_BOARD_GAP;
		const u_int uRows = static_cast<u_int>(ceilf((2.0f * fZ) / fPitch));
		const u_int uSalt = xR.m_uSyntheticSeed;
		for (u_int r = 0u; r < uRows; ++r)
		{
			const float fZ0 = -fZ + static_cast<float>(r) * fPitch;
			const float fZ1 = ZM_IGMin(fZ0 + fZM_IG_BOARD_WIDTH, fZ);
			if (fZ1 - fZ0 < 0.02f) { continue; }

			// The first board in a row is shortened by a hashed fraction so the
			// end joints do not line up across rows.
			float fX0 = -fX;
			u_int uBoard = 0u;
			float fLen = 0.40f + 1.40f * ZM_SynthTexHash01(r, 0u, uSalt);
			while (fX0 < fX - 0.02f)
			{
				const float fX1 = ZM_IGMin(fX0 + fLen, fX);
				// ★★ THE FIRST BOARD IN EVERY ROW SITS EXACTLY ON THE WALKING PLANE,
				// BY CONSTRUCTION RATHER THAN BY LUCK. The others step 0, 1 or 2 mm
				// DOWN from it, which is what makes the lips between boards real.
				// Deriving every step from the hash would leave "the floor's highest
				// surface is exactly y = 0" true only because some board in some row
				// happened to draw a zero -- overwhelmingly likely across hundreds of
				// boards, and still not a guarantee. That plane is where every spawn
				// marker and every authored body in these scenes stands, so it is
				// pinned instead: a silent 1 mm drop would put the player's feet
				// under the floor in a scene whose bytes are committed.
				const u_int uHashStep = static_cast<u_int>(
					ZM_SynthTexHash01(r, uBoard + 1u, uSalt + 17u) * 3.0f);
				const u_int uStep = (uBoard == 0u) ? 0u : (uHashStep > 2u ? 2u : uHashStep);
				// Spelled as a branch rather than a multiply by zero: the multiply
				// yields NEGATIVE zero, whose bit pattern differs from +0.0f, and
				// these vertices are hashed and baked into a committed asset.
				const float fTop = (uStep == 0u)
					? 0.0f : -fZM_IG_BOARD_LIP * static_cast<float>(uStep);
				ZM_StaticMesh::AppendWorldBox(xMesh,
					Zenith_Maths::Vector3(fX0, -fZM_IG_SLAB, fZ0),
					Zenith_Maths::Vector3(fX1, fTop,         fZ1), fTile);
				fX0 = fX1 + fZM_IG_BOARD_END_GAP;
				++uBoard;
				fLen = 1.20f + 1.20f * ZM_SynthTexHash01(r, uBoard + 1u, uSalt);
			}
		}
	}

	// ---- Ceiling -------------------------------------------------------------
	void ZM_IGBuildCeiling(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_CEILING);
		const float fX = xS.InnerHalfWidth();
		const float fZ = xS.InnerHalfDepth();
		ZM_IGAppendGridBox(xMesh,
			Zenith_Maths::Vector3(-fX, xS.m_fWallHeight,              -fZ),
			Zenith_Maths::Vector3( fX, xS.m_fWallHeight + fZM_IG_SLAB, fZ), fTile,
			fZM_INTERIOR_SHADING_CELL);
	}

	// ---- Walls ---------------------------------------------------------------
	void ZM_IGBuildWalls(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_WALL);

		// -Z back wall: solid.
		ZM_IGEmitWallFace(xMesh, xS, ZM_IG_FACE_BACK, fTile, nullptr, 0u);

		// The two side walls, cut around their windows.
		ZM_IGHole axHoles[4];
		u_int uN = ZM_IGCollectWindowHoles(xR.m_eRoom, ZM_INTERIOR_WALL_NEG_X, axHoles, 4u);
		ZM_IGEmitWallFace(xMesh, xS, ZM_IG_FACE_NEG_X, fTile, axHoles, uN);
		uN = ZM_IGCollectWindowHoles(xR.m_eRoom, ZM_INTERIOR_WALL_POS_X, axHoles, 4u);
		ZM_IGEmitWallFace(xMesh, xS, ZM_IG_FACE_POS_X, fTile, axHoles, uN);

		// +Z entrance wall: two panels flanking the aperture, plus the lintel over
		// it -- the aperture is simply a HOLE in the same cutter. It is a REAL GAP:
		// the scene's blockouts leave it open and the player walks through it, so
		// nothing is emitted across it below the lintel.
		const ZM_IGHole xDoor{ -xS.m_fApertureHalfW, xS.m_fApertureHalfW, 0.0f, xS.m_fApertureHeight };
		ZM_IGEmitWallFace(xMesh, xS, ZM_IG_FACE_ENTRANCE, fTile, &xDoor, 1u);
	}

	// ---- Trim ----------------------------------------------------------------
	//
	// Skirting: three stacked members of decreasing projection -- a plinth, a
	// step and a shadow line -- along the base of every wall, stopping at the
	// door. It is the single cheapest thing that stops a room reading as a box:
	// it draws the wall/floor junction as a moulding instead of a seam.
	void ZM_IGSkirtRun(ZM_GenMesh& xMesh, const ZM_InteriorRoomSpec& xS, ZM_IG_WALL_FACE eFace,
		float fS0, float fS1, float fTile)
	{
		ZM_IGWallBox(xMesh, xS, eFace, -0.040f, 0.0f, fS0, fS1, 0.000f, 0.100f, fTile, 0.0f);
		ZM_IGWallBox(xMesh, xS, eFace, -0.030f, 0.0f, fS0, fS1, 0.100f, 0.130f, fTile, 0.0f);
		ZM_IGWallBox(xMesh, xS, eFace, -0.018f, 0.0f, fS0, fS1, 0.130f, 0.145f, fTile, 0.0f);
	}

	// Cornice: two members at the ceiling line, the lower one set back.
	void ZM_IGCorniceRun(ZM_GenMesh& xMesh, const ZM_InteriorRoomSpec& xS, ZM_IG_WALL_FACE eFace,
		float fS0, float fS1, float fTile)
	{
		const float fH = xS.m_fWallHeight;
		ZM_IGWallBox(xMesh, xS, eFace, -0.050f, 0.0f, fS0, fS1, fH - 0.090f, fH,          fTile, 0.0f);
		ZM_IGWallBox(xMesh, xS, eFace, -0.025f, 0.0f, fS0, fS1, fH - 0.120f, fH - 0.090f, fTile, 0.0f);
	}

	// An architrave: a flat surround fWidth wide standing fProud off the wall,
	// around an opening in (s, y). The bottom member is omitted when the opening
	// reaches the floor (a door).
	void ZM_IGArchitrave(ZM_GenMesh& xMesh, const ZM_InteriorRoomSpec& xS, ZM_IG_WALL_FACE eFace,
		float fS0, float fS1, float fY0, float fY1, float fWidth, float fProud, float fTile)
	{
		const bool bToFloor = fY0 <= 1.0e-4f;
		const float fBottom = bToFloor ? 0.0f : fY0 - fWidth;
		ZM_IGWallBox(xMesh, xS, eFace, -fProud, 0.0f, fS0 - fWidth, fS0,          fBottom, fY1 + fWidth, fTile, 0.0f);
		ZM_IGWallBox(xMesh, xS, eFace, -fProud, 0.0f, fS1,          fS1 + fWidth, fBottom, fY1 + fWidth, fTile, 0.0f);
		ZM_IGWallBox(xMesh, xS, eFace, -fProud, 0.0f, fS0,          fS1,          fY1,     fY1 + fWidth, fTile, 0.0f);
		if (!bToFloor)
		{
			ZM_IGWallBox(xMesh, xS, eFace, -fProud, 0.0f, fS0, fS1, fBottom, fY0, fTile, 0.0f);
		}
	}

	void ZM_IGBuildTrim(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_TRIM);
		const float fX = xS.InnerHalfWidth();
		const float fZ = xS.InnerHalfDepth();
		const float fA = xS.m_fApertureHalfW;

		// Skirting on all four walls; the entrance wall's two runs stop at the door.
		ZM_IGSkirtRun(xMesh, xS, ZM_IG_FACE_BACK,     -fX, fX, fTile);
		ZM_IGSkirtRun(xMesh, xS, ZM_IG_FACE_NEG_X,    -fZ, fZ, fTile);
		ZM_IGSkirtRun(xMesh, xS, ZM_IG_FACE_POS_X,    -fZ, fZ, fTile);
		ZM_IGSkirtRun(xMesh, xS, ZM_IG_FACE_ENTRANCE, -fX, -fA - 0.10f, fTile);
		ZM_IGSkirtRun(xMesh, xS, ZM_IG_FACE_ENTRANCE,  fA + 0.10f, fX, fTile);

		// Cornice all round. It runs across the lintel too: the aperture head is
		// well below the ceiling line in both rooms.
		ZM_IGCorniceRun(xMesh, xS, ZM_IG_FACE_BACK,     -fX, fX, fTile);
		ZM_IGCorniceRun(xMesh, xS, ZM_IG_FACE_NEG_X,    -fZ, fZ, fTile);
		ZM_IGCorniceRun(xMesh, xS, ZM_IG_FACE_POS_X,    -fZ, fZ, fTile);
		ZM_IGCorniceRun(xMesh, xS, ZM_IG_FACE_ENTRANCE, -fX, fX, fTile);

		// The door reveal: a lining around the aperture, standing into the room so
		// the opening has an edge rather than a paper-thin cut...
		constexpr float fRevealW = 0.10f;
		constexpr float fRevealP = 0.05f;
		ZM_IGWallBox(xMesh, xS, ZM_IG_FACE_ENTRANCE, -fRevealP, 0.0f, -fA - fRevealW, -fA,
			0.0f, xS.m_fApertureHeight + fRevealW, fTile, 0.0f);
		ZM_IGWallBox(xMesh, xS, ZM_IG_FACE_ENTRANCE, -fRevealP, 0.0f, fA, fA + fRevealW,
			0.0f, xS.m_fApertureHeight + fRevealW, fTile, 0.0f);
		ZM_IGWallBox(xMesh, xS, ZM_IG_FACE_ENTRANCE, -fRevealP, 0.0f, -fA - fRevealW, fA + fRevealW,
			xS.m_fApertureHeight, xS.m_fApertureHeight + fRevealW, fTile, 0.0f);
		// ...and an architrave outside it.
		ZM_IGArchitrave(xMesh, xS, ZM_IG_FACE_ENTRANCE, -fA - fRevealW, fA + fRevealW,
			0.0f, xS.m_fApertureHeight + fRevealW, 0.08f, 0.02f, fTile);

		// Architraves round the windows.
		const u_int uWindows = ZM_GetInteriorWindowCount(xR.m_eRoom);
		for (u_int w = 0u; w < uWindows; ++w)
		{
			const ZM_InteriorWindow& xW = ZM_GetInteriorWindow(xR.m_eRoom, w);
			ZM_IGArchitrave(xMesh, xS, ZM_IGWindowFace(xW),
				xW.Z0() - fZM_INTERIOR_WINDOW_FRAME_WIDTH, xW.Z1() + fZM_INTERIOR_WINDOW_FRAME_WIDTH,
				xW.m_fSillY - fZM_INTERIOR_WINDOW_FRAME_WIDTH, xW.m_fHeadY + fZM_INTERIOR_WINDOW_FRAME_WIDTH,
				0.08f, 0.02f, fTile);
		}

		// The cottage's ceiling joists: dark timbers spanning the width at a
		// 0.9 m pitch, with one on the room's axis so the pendant hangs from a
		// joist rather than beside one. The lab's ceiling is a flat soffit.
		if (xR.m_eRoom == ZM_INTERIOR_ROOM_PLAYER_HOME)
		{
			constexpr float fJoistPitch = 0.90f;
			constexpr float fJoistW     = 0.16f;
			constexpr float fJoistD     = 0.14f;
			const int iCount = static_cast<int>(floorf((fZ - 0.30f) / fJoistPitch));
			for (int k = -iCount; k <= iCount; ++k)
			{
				const float fJZ = static_cast<float>(k) * fJoistPitch;
				ZM_IGAppendGridBox(xMesh,
					Zenith_Maths::Vector3(-fX, xS.m_fWallHeight - fJoistD, fJZ - 0.5f * fJoistW),
					Zenith_Maths::Vector3( fX, xS.m_fWallHeight,           fJZ + 0.5f * fJoistW),
					fTile, 1.5f);
			}
		}
	}

	// ---- Window dressing -----------------------------------------------------
	void ZM_IGBuildWindowFrames(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_WINDOW);
		const float fFW = fZM_INTERIOR_WINDOW_FRAME_WIDTH;
		const float fBW = fZM_INTERIOR_WINDOW_BAR_WIDTH;
		const u_int uWindows = ZM_GetInteriorWindowCount(xR.m_eRoom);
		for (u_int w = 0u; w < uWindows; ++w)
		{
			const ZM_InteriorWindow& xW = ZM_GetInteriorWindow(xR.m_eRoom, w);
			const ZM_IG_WALL_FACE eFace = ZM_IGWindowFace(xW);
			const float fZ0 = xW.Z0(), fZ1 = xW.Z1();
			const float fY0 = xW.m_fSillY, fY1 = xW.m_fHeadY;

			// Reveal linings: jambs, head and sill lining, from just behind the
			// wall slab to the back of the reveal, framing the sky card.
			const float fD0 = fZM_IG_SLAB, fD1 = fZM_INTERIOR_WINDOW_REVEAL_DEPTH;
			ZM_IGWallBox(xMesh, xS, eFace, fD0, fD1, fZ0 - fFW, fZ0,       fY0 - fFW, fY1 + fFW, fTile, 0.0f);
			ZM_IGWallBox(xMesh, xS, eFace, fD0, fD1, fZ1,       fZ1 + fFW, fY0 - fFW, fY1 + fFW, fTile, 0.0f);
			ZM_IGWallBox(xMesh, xS, eFace, fD0, fD1, fZ0,       fZ1,       fY1,       fY1 + fFW, fTile, 0.0f);
			ZM_IGWallBox(xMesh, xS, eFace, fD0, fD1, fZ0,       fZ1,       fY0 - fFW, fY0,       fTile, 0.0f);

			// The frame: four members in the opening, set back into the reveal.
			const float fF0 = fZM_INTERIOR_WINDOW_FRAME_DEPTH - 0.01f;
			const float fF1 = fZM_INTERIOR_WINDOW_FRAME_DEPTH + 0.05f;
			ZM_IGWallBox(xMesh, xS, eFace, fF0, fF1, fZ0,       fZ0 + fFW, fY0,       fY1,       fTile, 0.0f);
			ZM_IGWallBox(xMesh, xS, eFace, fF0, fF1, fZ1 - fFW, fZ1,       fY0,       fY1,       fTile, 0.0f);
			ZM_IGWallBox(xMesh, xS, eFace, fF0, fF1, fZ0,       fZ1,       fY0,       fY0 + fFW, fTile, 0.0f);
			ZM_IGWallBox(xMesh, xS, eFace, fF0, fF1, fZ0,       fZ1,       fY1 - fFW, fY1,       fTile, 0.0f);

			// Glazing bars: a mullion on the centreline and a transom above the
			// middle, so the pane reads as four lights rather than one sheet.
			const float fMid = xW.m_fCentreZ;
			const float fTransom = fY0 + (fY1 - fY0) * 0.60f;
			ZM_IGWallBox(xMesh, xS, eFace, fF0 + 0.01f, fF1, fMid - 0.5f * fBW, fMid + 0.5f * fBW,
				fY0 + fFW, fY1 - fFW, fTile, 0.0f);
			ZM_IGWallBox(xMesh, xS, eFace, fF0 + 0.01f, fF1, fZ0 + fFW, fZ1 - fFW,
				fTransom - 0.5f * fBW, fTransom + 0.5f * fBW, fTile, 0.0f);

			// The sill board, proud into the room and wider than the opening.
			ZM_IGWallBox(xMesh, xS, eFace, -0.08f, 0.0f, fZ0 - 0.10f, fZ1 + 0.10f, fY0 - fFW, fY0, fTile, 0.0f);

			// The curtain rail: a bar above the head, standing off the wall, long
			// enough to carry both panels drawn back.
			const float fP = fZM_INTERIOR_CURTAIN_PANEL_WIDTH;
			ZM_IGWallBox(xMesh, xS, eFace, -0.11f, -0.08f, fZ0 - fP - 0.05f, fZ1 + fP + 0.05f,
				fY1 + 0.08f, fY1 + 0.11f, fTile, 0.0f);
		}
	}

	void ZM_IGBuildGlass(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_GLASS);
		const float fFW = fZM_INTERIOR_WINDOW_FRAME_WIDTH;
		const u_int uWindows = ZM_GetInteriorWindowCount(xR.m_eRoom);
		for (u_int w = 0u; w < uWindows; ++w)
		{
			const ZM_InteriorWindow& xW = ZM_GetInteriorWindow(xR.m_eRoom, w);
			// One pane inside the frame, 6 mm thick, on the glass plane.
			ZM_IGWallBox(xMesh, xS, ZM_IGWindowFace(xW),
				fZM_INTERIOR_WINDOW_GLASS_DEPTH - 0.003f, fZM_INTERIOR_WINDOW_GLASS_DEPTH + 0.003f,
				xW.Z0() + fFW, xW.Z1() - fFW, xW.m_fSillY + fFW, xW.m_fHeadY - fFW, fTile, 0.0f);
		}
	}

	void ZM_IGBuildSky(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_SKY);
		const u_int uWindows = ZM_GetInteriorWindowCount(xR.m_eRoom);
		for (u_int w = 0u; w < uWindows; ++w)
		{
			const ZM_InteriorWindow& xW = ZM_GetInteriorWindow(xR.m_eRoom, w);
			// The card: 1 cm thick at the back of the reveal, 10 cm larger than the
			// opening on every side so no edge of it can ever be seen past a jamb.
			ZM_IGWallBox(xMesh, xS, ZM_IGWindowFace(xW),
				fZM_INTERIOR_WINDOW_REVEAL_DEPTH - 0.01f, fZM_INTERIOR_WINDOW_REVEAL_DEPTH,
				xW.Z0() - 0.10f, xW.Z1() + 0.10f, xW.m_fSillY - 0.10f, xW.m_fHeadY + 0.10f, fTile, 0.0f);
		}
	}

	void ZM_IGBuildCurtains(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_CURTAIN);
		const bool bHome = (xR.m_eRoom == ZM_INTERIOR_ROOM_PLAYER_HOME);
		// The cottage's curtains fold; the lab's are flat panel blinds. A fold is
		// a vertical slat standing a different distance off the wall from its
		// neighbour, so the panel's silhouette zigzags and its face catches light
		// in bands, which is what a hung fabric does.
		const u_int uSlats = bHome ? 6u : 2u;
		const float fP = fZM_INTERIOR_CURTAIN_PANEL_WIDTH;
		const float fSlat = fP / static_cast<float>(uSlats);
		const u_int uWindows = ZM_GetInteriorWindowCount(xR.m_eRoom);
		for (u_int w = 0u; w < uWindows; ++w)
		{
			const ZM_InteriorWindow& xW = ZM_GetInteriorWindow(xR.m_eRoom, w);
			const ZM_IG_WALL_FACE eFace = ZM_IGWindowFace(xW);
			const float fTop = xW.m_fHeadY + 0.08f;
			const float afPanelS0[2] = { xW.Z0() - fP, xW.Z1() };
			for (u_int p = 0u; p < 2u; ++p)
			{
				for (u_int s = 0u; s < uSlats; ++s)
				{
					const float fS0 = afPanelS0[p] + static_cast<float>(s) * fSlat;
					const float fS1 = fS0 + fSlat;
					const bool bOut = ((s + p) & 1u) == 0u;
					const float fDNear = bOut ? -fZM_INTERIOR_CURTAIN_DEPTH : -0.06f;
					const float fDFar  = bOut ? -0.05f : -0.02f;
					ZM_IGWallBox(xMesh, xS, eFace, fDNear, fDFar, fS0, fS1,
						fZM_INTERIOR_CURTAIN_HEM_Y, fTop, fTile, 1.0f);
				}
			}
		}
	}

	void ZM_IGBuildRug(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		// Object-mapped: the whole [0,1] is the rug, so it can carry a border.
		const ZM_InteriorRug xRug = ZM_GetInteriorRug(xR.m_eRoom);
		const ZM_GenUVIsland xUnit;
		ZM_StaticMesh::AppendBox(xMesh,
			Zenith_Maths::Vector3(xRug.m_fCentreX - 0.5f * xRug.m_fWidth, 0.0f,             xRug.m_fCentreZ - 0.5f * xRug.m_fDepth),
			Zenith_Maths::Vector3(xRug.m_fCentreX + 0.5f * xRug.m_fWidth, xRug.m_fThickness, xRug.m_fCentreZ + 0.5f * xRug.m_fDepth),
			xUnit);
	}

	// ---- The baked shading pass ----------------------------------------------
	void ZM_IGApplyVertexShading(const ZM_InteriorRecipe& xR, ZM_INTERIOR_SURFACE eSurface,
		ZM_GenMesh& xMesh)
	{
		if (!ZM_IGSurfaceShaded(eSurface))
		{
			return;
		}
		const bool bWorn = (eSurface == ZM_INTERIOR_SURFACE_FLOOR || eSurface == ZM_INTERIOR_SURFACE_RUG);
		const u_int uVerts = xMesh.GetNumVerts();
		if (xMesh.m_xColors.GetSize() != uVerts)
		{
			// The emitters always write one colour per vertex; a builder that did
			// not would have nothing to multiply into. Fill white so the pass is
			// total and the shading still lands.
			xMesh.m_xColors.Clear();
			for (u_int v = 0u; v < uVerts; ++v)
			{
				xMesh.m_xColors.PushBack(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
			}
		}
		for (u_int v = 0u; v < uVerts; ++v)
		{
			const Zenith_Maths::Vector3& xP = xMesh.m_xPositions.Get(v);
			// ★ THE NORMAL IS WHAT SAYS WHICH SURFACE THIS VERTEX IS, and therefore
			// which plane must NOT darken it. The emitters write hard per-face
			// normals before this pass runs, so it is available and exact.
			const Zenith_Maths::Vector3& xN = xMesh.m_xNormals.Get(v);
			float fMul = ZM_InteriorSurfaceVertexOcclusion(xR.m_xSpec, xP, xN);
			if (bWorn) { fMul *= ZM_InteriorTrafficWear(xR.m_xSpec, xP); }
			Zenith_Maths::Vector4& xC = xMesh.m_xColors.Get(v);
			// MULTIPLIED, not assigned, so a builder's own pre-shading (the dark
			// underlay under the boards) survives.
			xC = Zenith_Maths::Vector4(ZM_IGClamp01(xC.x * fMul), ZM_IGClamp01(xC.y * fMul),
				ZM_IGClamp01(xC.z * fMul), 1.0f);
		}
	}
}

// ★★★ WHICH PLANE A VERTEX BELONGS TO IS DECIDED BY ITS NORMAL, NOT BY ITS
// DISTANCE. This function shipped comparing each plane's distance against a
// small "own plane" epsilon and skipping the plane when the vertex sat on it --
// which cannot tell the plane a vertex LIES IN from the plane it MEETS. A wall
// vertex at the floor line has x on the wall (skipped, correctly) AND y == 0
// (skipped, WRONGLY, as though the floor were its own plane), so the two
// remaining planes were far away and it came out at exactly 1.0. Every wall
// vertex in both rooms did: the corner darkening was computed, written, and
// uniformly equal to "no darkening at all".
//
// The surface a vertex belongs to is what its NORMAL says, so that is what is
// asked. A floor vertex (n = +Y) is not darkened by the floor and IS darkened by
// a wall it approaches; a wall vertex (n = +/-X) is not darkened by its own wall
// and IS darkened by the floor it meets. Which is the whole effect.
namespace
{
	// The four room planes, as (distance from the vertex, is this the vertex's
	// own surface). Shared by both entry points below.
	void ZM_IGRoomPlaneDistances(const ZM_InteriorRoomSpec& xSpec,
		const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3* pxNormal,
		float afDistanceOut[4], bool abIsOwnSurfaceOut[4])
	{
		afDistanceOut[0] = xSpec.InnerHalfWidth() - fabsf(xPos.x);   // the nearer X wall
		afDistanceOut[1] = xSpec.InnerHalfDepth() - fabsf(xPos.z);   // the nearer Z wall
		afDistanceOut[2] = xPos.y;                                   // the floor
		afDistanceOut[3] = xSpec.m_fWallHeight - xPos.y;             // the ceiling

		for (u_int i = 0u; i < 4u; ++i) { abIsOwnSurfaceOut[i] = false; }
		if (pxNormal == nullptr)
		{
			return;   // a point in space belongs to no surface: every plane counts
		}
		// A face is "in" a plane when its normal is (anti)parallel to that plane's
		// normal. 0.5 is a half-angle of 60 degrees, which cleanly separates the
		// axis-aligned faces these rooms are built from.
		constexpr float fAXIS = 0.5f;
		abIsOwnSurfaceOut[0] = fabsf(pxNormal->x) > fAXIS;
		abIsOwnSurfaceOut[1] = fabsf(pxNormal->z) > fAXIS;
		abIsOwnSurfaceOut[2] = fabsf(pxNormal->y) > fAXIS;
		abIsOwnSurfaceOut[3] = abIsOwnSurfaceOut[2];
	}

	float ZM_IGOcclusionFromPlanes(const float afDistance[4], const bool abIsOwnSurface[4])
	{
		// The two strongest terms, so an inside CORNER (two planes) is darker than
		// a single wall, and a third plane cannot drive it to black.
		float fBest = 0.0f, fSecond = 0.0f;
		for (u_int i = 0u; i < 4u; ++i)
		{
			if (abIsOwnSurface[i]) { continue; }
			// Clamped rather than skipped: a vertex sitting ON or just inside the
			// plane it meets is in full contact with it, which is the darkest case
			// and precisely the one the old epsilon threw away.
			const float fDistance = afDistance[i] > 0.0f ? afDistance[i] : 0.0f;
			const float fT = 1.0f - fDistance / fZM_INTERIOR_OCCLUSION_RADIUS;
			if (fT <= 0.0f) { continue; }
			const float fC = fT * fT;
			if (fC > fBest)        { fSecond = fBest; fBest = fC; }
			else if (fC > fSecond) { fSecond = fC; }
		}
		const float fSum = ZM_IGMin(1.0f, fBest + fSecond);
		return ZM_IGMax(0.10f, 1.0f - fZM_INTERIOR_OCCLUSION_DEPTH * fSum);
	}
}

float ZM_InteriorVertexOcclusion(const ZM_InteriorRoomSpec& xSpec, const Zenith_Maths::Vector3& xPos)
{
	float afDistance[4];
	bool  abIsOwnSurface[4];
	ZM_IGRoomPlaneDistances(xSpec, xPos, nullptr, afDistance, abIsOwnSurface);
	return ZM_IGOcclusionFromPlanes(afDistance, abIsOwnSurface);
}

float ZM_InteriorSurfaceVertexOcclusion(const ZM_InteriorRoomSpec& xSpec,
	const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Vector3& xNormal)
{
	float afDistance[4];
	bool  abIsOwnSurface[4];
	ZM_IGRoomPlaneDistances(xSpec, xPos, &xNormal, afDistance, abIsOwnSurface);
	return ZM_IGOcclusionFromPlanes(afDistance, abIsOwnSurface);
}

float ZM_InteriorTrafficWear(const ZM_InteriorRoomSpec& xSpec, const Zenith_Maths::Vector3& xPos)
{
	const float fZ = xSpec.InnerHalfDepth();
	if (fZ <= 1.0e-4f) { return 1.0f; }
	// Lateral falloff either side of the door axis...
	const float fLat = 1.0f - fabsf(xPos.x) / fZM_INTERIOR_WEAR_HALF_WIDTH;
	if (fLat <= 0.0f) { return 1.0f; }
	// ...full strength from the door to the room centre, fading over the half
	// beyond it.
	const float fT = xPos.z / fZ;   // +1 at the door, 0 at the centre, -1 at the back
	const float fAlong = fT >= 0.0f ? 1.0f : ZM_IGMax(0.0f, 1.0f + fT * 2.0f);
	return 1.0f - fZM_INTERIOR_WEAR_DEPTH * fLat * fLat * fAlong;
}

void ZM_BuildInteriorSurfaceMesh(const ZM_InteriorRecipe& xR,
	ZM_INTERIOR_SURFACE eSurface, ZM_GenMesh& xMesh)
{
	xMesh.Reset();
	switch (eSurface)
	{
	case ZM_INTERIOR_SURFACE_FLOOR:   ZM_IGBuildFloor       (xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_WALL:    ZM_IGBuildWalls       (xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_CEILING: ZM_IGBuildCeiling     (xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_TRIM:    ZM_IGBuildTrim        (xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_WINDOW:  ZM_IGBuildWindowFrames(xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_GLASS:   ZM_IGBuildGlass       (xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_SKY:     ZM_IGBuildSky         (xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_CURTAIN: ZM_IGBuildCurtains    (xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_RUG:     ZM_IGBuildRug         (xR, xMesh); break;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_InteriorGen] ZM_BuildInteriorSurfaceMesh: %u is not a surface -- "
			"emitting the floor", (u_int)eSurface);
		ZM_IGBuildFloor(xR, xMesh);
		break;
	}
	ZM_IGApplyVertexShading(xR, eSurface, xMesh);
	// Hard per-face normals are already written by the emitters; do NOT re-run
	// ZM_GenGenerateNormals or every corner in the room welds and smooths.
	ZM_GenGenerateTangents(xMesh);
}

// ============================================================================
// Texture builders.
// ============================================================================
ZM_GenImage ZM_BuildInteriorSurfaceHeight(const ZM_InteriorRecipe& xR,
	ZM_INTERIOR_SURFACE eSurface)
{
	const u_int uRes = ZM_InteriorSurfaceResolution(eSurface);
	const ZM_IGCourseSpec xC = ZM_IGCourses(xR.m_eRoom, eSurface);
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
			switch (eSurface)
			{
			case ZM_INTERIOR_SURFACE_GLASS:
			case ZM_INTERIOR_SURFACE_SKY:
				// Flat. Glass has no relief a normal map should invent, and the
				// sky card is a uniform emitter.
				break;
			case ZM_INTERIOR_SURFACE_CURTAIN:
				// Vertical pleats: slow across, constant down.
				fH += (ZM_SynthValueNoise(fU * 18.0f, fV * 0.15f, 18u, uSalt + 41u) - 0.5f) * 0.40f;
				fH += (ZM_SynthFbm(fU, fV, 24u, uSalt + 43u) - 0.5f) * 0.06f;
				break;
			default:
				fH += (ZM_SynthFbm(fU, fV, 16u, uSalt) - 0.5f) * 0.18f;
				if (xC.m_uRows > 1u || xC.m_uCols > 1u)
				{
					const ZM_SynthCourseSample xSm = ZM_SynthSampleCourses(fU, fV,
						xC.m_uRows, xC.m_uCols, xC.m_fJoint, xC.m_bStagger, uSalt);
					fH -= xSm.m_fJoint * xC.m_fRelief * 0.5f;
					fH += (xSm.m_fUnit - 0.5f) * 0.05f;
				}
				else
				{
					// A courseless surface still needs relief or its normal map is an
					// expensive way to store flat: plaster skim, or a brushed grain on
					// the trim and the painted window timber, stretched along one axis.
					const bool bGrained = (eSurface == ZM_INTERIOR_SURFACE_TRIM
						|| eSurface == ZM_INTERIOR_SURFACE_WINDOW);
					fH += (ZM_SynthValueNoise(fU * 0.3f, fV * 8.0f, 12u, uSalt + 7u) - 0.5f)
						* (bGrained ? 0.10f : 0.06f);
				}
				break;
			}

			const float fC = ZM_IGClamp01(fH);
			xImg.Set(uY, uX, Zenith_Maths::Vector4(fC, fC, fC, 1.0f));
		}
	}
	return xImg;
}

ZM_InteriorTextureSet ZM_BuildInteriorSurfaceTextures(const ZM_InteriorRecipe& xR,
	ZM_INTERIOR_SURFACE eSurface)
{
	// ALL RNG draws up-front, fixed count and order, from the ALBEDO domain only.
	ZM_GenRNG xRng = ZM_MakeInteriorRNG(xR, ZM_GEN_DOMAIN_ALBEDO);
	const float fJitR     = xRng.NextFloatRange(-0.03f, 0.03f);
	const float fJitG     = xRng.NextFloatRange(-0.03f, 0.03f);
	const float fJitB     = xRng.NextFloatRange(-0.03f, 0.03f);
	const float fWear     = xRng.NextFloatRange(0.08f, 0.22f);
	const float fRoughJit = xRng.NextFloatRange(-0.04f, 0.04f);

	const u_int uRes = ZM_InteriorSurfaceResolution(eSurface);
	const ZM_IGCourseSpec xC = ZM_IGCourses(xR.m_eRoom, eSurface);
	const ZM_InteriorSurfaceLook xL = ZM_GetInteriorSurfaceLook(xR.m_eRoom, eSurface);
	const u_int uSalt = xR.m_uSyntheticSeed;
	const bool bFlat = (eSurface == ZM_INTERIOR_SURFACE_GLASS || eSurface == ZM_INTERIOR_SURFACE_SKY);

	const Zenith_Maths::Vector3 xBase(
		ZM_IGClamp01(xL.m_xBaseColour.x + (bFlat ? 0.0f : fJitR)),
		ZM_IGClamp01(xL.m_xBaseColour.y + (bFlat ? 0.0f : fJitG)),
		ZM_IGClamp01(xL.m_xBaseColour.z + (bFlat ? 0.0f : fJitB)));

	ZM_InteriorTextureSet xOut;
	xOut.m_xAlbedo = ZM_GenImage(uRes, uRes);

	const ZM_GenImage xHeight = ZM_BuildInteriorSurfaceHeight(xR, eSurface);
	const float fInv = 1.0f / static_cast<float>(uRes);

	for (u_int uY = 0u; uY < uRes; ++uY)
	{
		const float fV = (static_cast<float>(uY) + 0.5f) * fInv;
		for (u_int uX = 0u; uX < uRes; ++uX)
		{
			const float fU = (static_cast<float>(uX) + 0.5f) * fInv;
			const float fH = xHeight.Get(uY, uX).x;
			const float fCavity = 1.0f - fH;

			Zenith_Maths::Vector3 xCol = xBase;
			if (!bFlat)
			{
				if (xC.m_uRows > 1u || xC.m_uCols > 1u)
				{
					const ZM_SynthCourseSample xSm = ZM_SynthSampleCourses(fU, fV,
						xC.m_uRows, xC.m_uCols, xC.m_fJoint, xC.m_bStagger, uSalt);
					// The joint is DARKER indoors, not lighter: a board gap or a tile
					// grout line in a lit room is a shadow, where an outdoor mortar bed
					// is a lighter mineral.
					xCol = ZM_IGLerp3(xCol, ZM_IGLerp3(xCol,
						Zenith_Maths::Vector3(0.05f, 0.04f, 0.03f), 0.75f), xSm.m_fJoint);
					const float fUnit = (xSm.m_fUnit - 0.5f) * 0.14f * (1.0f - xSm.m_fJoint);
					xCol = Zenith_Maths::Vector3(xCol.x + fUnit, xCol.y + fUnit, xCol.z + fUnit);
				}

				// The rug's border: a darker band round the edge, object-mapped.
				if (eSurface == ZM_INTERIOR_SURFACE_RUG)
				{
					const float fEdge = ZM_IGMin(ZM_IGMin(fU, 1.0f - fU), ZM_IGMin(fV, 1.0f - fV));
					if (fEdge < 0.08f)
					{
						xCol = ZM_IGLerp3(xCol, Zenith_Maths::Vector3(xCol.x * 0.55f, xCol.y * 0.55f, xCol.z * 0.55f),
							fEdge < 0.06f ? 1.0f : (0.08f - fEdge) / 0.02f);
					}
				}

				// Wear collects in the cavities. Kept light: an interior that reads
				// grimy fights the "somebody lives here" brief.
				const float fWorn = ZM_IGClamp01(fCavity * fWear
					+ ZM_SynthFbm(fU, fV, 5u, uSalt + 211u) * fWear * 0.4f);
				xCol = ZM_IGLerp3(xCol, Zenith_Maths::Vector3(0.16f, 0.14f, 0.12f), fWorn * 0.25f);
			}

			xOut.m_xAlbedo.Set(uY, uX, Zenith_Maths::Vector4(
				ZM_IGClamp01(xCol.x), ZM_IGClamp01(xCol.y), ZM_IGClamp01(xCol.z), 1.0f));
		}
	}

	// The other three maps come from the ONE shared builder -- see the same note in
	// ZM_BuildingGen. Only the height field and the response are this family's.
	ZM_SynthPbrResponse xPbr;
	xPbr.m_fRoughness       = xL.m_fRoughness;
	xPbr.m_fMetallic        = xL.m_fMetallic;
	xPbr.m_fNormalStrength  = xL.m_fNormalStrength;
	xPbr.m_fRoughnessJitter = bFlat ? 0.0f : fRoughJit;
	xPbr.m_fCavityRoughness = bFlat ? 0.0f : 0.16f;
	xPbr.m_fCavityOcclusion = bFlat ? 0.0f : 0.50f;
	// ★ WRAPPED, because every one of these but the rug is a TILING surface. A
	// tiling normal map built with the clamped border rule carries a one-texel
	// line of wrong slope along every repeat, and at a 1.2 m board tile across a
	// 12 m room that line is drawn ten times. The rug is object-mapped ([0,1] IS
	// the rug, border and all), so it takes the clamped rule.
	xPbr.m_bWrap = (eSurface != ZM_INTERIOR_SURFACE_RUG);
	// ★ EDGE WEAR WHERE FEET AND HANDS REACH. A board's lip, a skirting arris and
	// a painted sill are the three things in a room that are actually rubbed
	// smooth; plaster, a ceiling and a curtain are not, and a worn ceiling reads
	// as a rendering error rather than as age.
	switch (eSurface)
	{
	case ZM_INTERIOR_SURFACE_FLOOR:  xPbr.m_fEdgeWearStrength = 0.55f; break;
	case ZM_INTERIOR_SURFACE_TRIM:   xPbr.m_fEdgeWearStrength = 0.40f; break;
	case ZM_INTERIOR_SURFACE_WINDOW: xPbr.m_fEdgeWearStrength = 0.35f; break;
	default: break;
	}
	const ZM_SynthPbrSet xSet = ZM_SynthBuildPbrSet(xHeight, xPbr);
	xOut.m_xNormal            = xSet.m_xNormal;
	xOut.m_xRoughnessMetallic = xSet.m_xRoughnessMetallic;
	xOut.m_xOcclusion         = xSet.m_xOcclusion;
	return xOut;
}

bool ZM_InteriorTextureSet::Equals(const ZM_InteriorTextureSet& xOther) const
{
	return m_xAlbedo.Equals(xOther.m_xAlbedo)
		&& m_xNormal.Equals(xOther.m_xNormal)
		&& m_xRoughnessMetallic.Equals(xOther.m_xRoughnessMetallic)
		&& m_xOcclusion.Equals(xOther.m_xOcclusion);
}

bool ZM_InteriorTextureSet::NonEmpty() const
{
	return !m_xAlbedo.IsEmpty() && !m_xNormal.IsEmpty()
		&& !m_xRoughnessMetallic.IsEmpty() && !m_xOcclusion.IsEmpty();
}

void ZM_BuildInterior(ZM_INTERIOR_ROOM eRoom, ZM_Interior& xOut)
{
	const ZM_InteriorRecipe xR = ZM_ResolveInteriorRecipe(eRoom);
	xOut.m_eRoom = xR.m_eRoom;
	for (u_int s = 0u; s < static_cast<u_int>(ZM_INTERIOR_SURFACE_COUNT); ++s)
	{
		const ZM_INTERIOR_SURFACE eS = static_cast<ZM_INTERIOR_SURFACE>(s);
		ZM_BuildInteriorSurfaceMesh(xR, eS, xOut.m_axMesh[s]);
		xOut.m_axTextures[s] = ZM_BuildInteriorSurfaceTextures(xR, eS);
	}
}

// ============================================================================
// Determinism + validation + queries.
// ============================================================================
bool ZM_InteriorBuildEqual(const ZM_Interior& xA, const ZM_Interior& xB)
{
	if (xA.m_eRoom != xB.m_eRoom) { return false; }
	for (u_int s = 0u; s < static_cast<u_int>(ZM_INTERIOR_SURFACE_COUNT); ++s)
	{
		const ZM_GenMesh& xMa = xA.m_axMesh[s];
		const ZM_GenMesh& xMb = xB.m_axMesh[s];
		if (!ZM_IGBuffersEqual(xMa.m_xPositions, xMb.m_xPositions)) { return false; }
		if (!ZM_IGBuffersEqual(xMa.m_xNormals,   xMb.m_xNormals))   { return false; }
		if (!ZM_IGBuffersEqual(xMa.m_xUVs,       xMb.m_xUVs))       { return false; }
		if (!ZM_IGBuffersEqual(xMa.m_xTangents,  xMb.m_xTangents))  { return false; }
		if (!ZM_IGBuffersEqual(xMa.m_xColors,    xMb.m_xColors))    { return false; }
		if (!ZM_IGBuffersEqual(xMa.m_xIndices,   xMb.m_xIndices))   { return false; }
		if (!xA.m_axTextures[s].Equals(xB.m_axTextures[s]))         { return false; }
	}
	return true;
}

u_int ZM_InteriorContentHash(const ZM_Interior& xInterior)
{
	u_int uHash = uZM_IG_FNV_OFFSET;
	for (u_int s = 0u; s < static_cast<u_int>(ZM_INTERIOR_SURFACE_COUNT); ++s)
	{
		const ZM_GenMesh& xM = xInterior.m_axMesh[s];
		uHash = ZM_IGFnvAccumBuffer(uHash, xM.m_xPositions);
		uHash = ZM_IGFnvAccumBuffer(uHash, xM.m_xNormals);
		uHash = ZM_IGFnvAccumBuffer(uHash, xM.m_xUVs);
		uHash = ZM_IGFnvAccumBuffer(uHash, xM.m_xTangents);
		uHash = ZM_IGFnvAccumBuffer(uHash, xM.m_xColors);
		uHash = ZM_IGFnvAccumBuffer(uHash, xM.m_xIndices);

		const ZM_InteriorTextureSet& xT = xInterior.m_axTextures[s];
		const u_int auMaps[4] = { xT.m_xAlbedo.ContentHash(), xT.m_xNormal.ContentHash(),
			xT.m_xRoughnessMetallic.ContentHash(), xT.m_xOcclusion.ContentHash() };
		uHash = ZM_IGFnvAccum(uHash, auMaps, sizeof(auMaps));
	}
	return uHash;
}

bool ZM_InteriorMeshRayHits(const ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xOrigin,
	const Zenith_Maths::Vector3& xDir, float& fTOut)
{
	fTOut = 0.0f;
	bool bHit = false;
	float fBest = 0.0f;
	const u_int uIdx = xMesh.m_xIndices.GetSize();
	const u_int uVerts = xMesh.GetNumVerts();
	for (u_int t = 0u; t + 2u < uIdx; t += 3u)
	{
		const u_int uA = xMesh.m_xIndices.Get(t), uB = xMesh.m_xIndices.Get(t + 1u), uC = xMesh.m_xIndices.Get(t + 2u);
		if (uA >= uVerts || uB >= uVerts || uC >= uVerts) { continue; }
		const Zenith_Maths::Vector3& xA = xMesh.m_xPositions.Get(uA);
		const Zenith_Maths::Vector3 xE1 = xMesh.m_xPositions.Get(uB) - xA;
		const Zenith_Maths::Vector3 xE2 = xMesh.m_xPositions.Get(uC) - xA;
		const Zenith_Maths::Vector3 xP = glm::cross(xDir, xE2);
		const float fDet = glm::dot(xE1, xP);
		if (fabsf(fDet) < 1.0e-9f) { continue; }
		const float fInvDet = 1.0f / fDet;
		const Zenith_Maths::Vector3 xT = xOrigin - xA;
		const float fU = glm::dot(xT, xP) * fInvDet;
		if (fU < 0.0f || fU > 1.0f) { continue; }
		const Zenith_Maths::Vector3 xQ = glm::cross(xT, xE1);
		const float fV = glm::dot(xDir, xQ) * fInvDet;
		if (fV < 0.0f || fU + fV > 1.0f) { continue; }
		const float fT = glm::dot(xE2, xQ) * fInvDet;
		if (fT <= 1.0e-5f) { continue; }
		if (!bHit || fT < fBest) { bHit = true; fBest = fT; }
	}
	fTOut = fBest;
	return bHit;
}

ZM_InteriorSurfaceValidation ZM_ValidateInteriorSurfaceMesh(const ZM_GenMesh& xMesh,
	float fMaxAbsUVAllowed)
{
	ZM_InteriorSurfaceValidation xV;
	const u_int uVerts = xMesh.GetNumVerts();
	const u_int uIdx   = xMesh.m_xIndices.GetSize();

	xV.m_bIndicesInRange = (uIdx > 0u) && (uIdx % 3u == 0u);
	for (u_int i = 0u; i < uIdx && xV.m_bIndicesInRange; ++i)
	{
		if (xMesh.m_xIndices.Get(i) >= uVerts) { xV.m_bIndicesInRange = false; }
	}

	if (uVerts > 0u)
	{
		const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
		const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);
		constexpr float fEps = 1.0e-4f;
		xV.m_bBoundsNonDegen = (xMax.x - xMin.x) > fEps
			&& (xMax.y - xMin.y) > fEps && (xMax.z - xMin.z) > fEps;
	}

	xV.m_bWindingOutward = xV.m_bIndicesInRange;
	if (xV.m_bIndicesInRange)
	{
		for (u_int t = 0u; t * 3u < uIdx; ++t)
		{
			const u_int uA = xMesh.m_xIndices.Get(t * 3u + 0u);
			const u_int uB = xMesh.m_xIndices.Get(t * 3u + 1u);
			const u_int uC = xMesh.m_xIndices.Get(t * 3u + 2u);
			const Zenith_Maths::Vector3 xFace = glm::cross(
				xMesh.m_xPositions.Get(uC) - xMesh.m_xPositions.Get(uA),
				xMesh.m_xPositions.Get(uB) - xMesh.m_xPositions.Get(uA));
			const Zenith_Maths::Vector3 xAvgN = xMesh.m_xNormals.Get(uA)
				+ xMesh.m_xNormals.Get(uB) + xMesh.m_xNormals.Get(uC);
			if (glm::dot(xFace, xAvgN) <= 0.0f)
			{
				xV.m_bWindingOutward = false;
				xV.m_uFirstBadTriangle = t;
				break;
			}
		}
	}

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

	// The baked shading rides in the colours, and the mesh shader multiplies
	// them in only while alpha > 0 -- so a colour outside [0,1] or an alpha of 0
	// is shading that silently does nothing or overflows the unorm8 the
	// vertex format stores.
	xV.m_bColoursUnitRange = (xMesh.m_xColors.GetSize() == uVerts) && uVerts > 0u;
	for (u_int v = 0u; v < uVerts && xV.m_bColoursUnitRange; ++v)
	{
		const Zenith_Maths::Vector4& xC = xMesh.m_xColors.Get(v);
		const bool bOk = std::isfinite(xC.x) && std::isfinite(xC.y) && std::isfinite(xC.z)
			&& xC.x >= 0.0f && xC.x <= 1.0f && xC.y >= 0.0f && xC.y <= 1.0f
			&& xC.z >= 0.0f && xC.z <= 1.0f && xC.w == 1.0f;
		if (!bOk) { xV.m_bColoursUnitRange = false; }
	}

	xV.m_bNoSkeleton    = (xMesh.GetNumBones() == 0u);
	xV.m_bNoSkinBuffers = (xMesh.m_xBoneIndices.GetSize() == 0u)
		&& (xMesh.m_xBoneWeights.GetSize() == 0u);

	xV.m_bAllValid = xV.m_bWindingOutward && xV.m_bBoundsNonDegen
		&& xV.m_bIndicesInRange && xV.m_bUVsFiniteAndBounded && xV.m_bColoursUnitRange
		&& xV.m_bNoSkeleton && xV.m_bNoSkinBuffers;
	return xV;
}

ZM_InteriorValidation ZM_ValidateInterior(const ZM_Interior& xInterior)
{
	ZM_InteriorValidation xV;
	xV.m_bAllValid = true;
	for (u_int s = 0u; s < static_cast<u_int>(ZM_INTERIOR_SURFACE_COUNT); ++s)
	{
		const ZM_INTERIOR_SURFACE eS = static_cast<ZM_INTERIOR_SURFACE>(s);
		const ZM_GenMesh& xMesh = xInterior.m_axMesh[s];

		// The exact bound the world projection can reach: the largest ABSOLUTE
		// coordinate over the mesh, divided by the tile. See ZM_ValidateBuilding for
		// why it is the absolute coordinate and not half the extent.
		float fBudget = 1.0f;
		if (xMesh.GetNumVerts() > 0u)
		{
			const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
			const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);
			const float afAbs[6] = {
				std::fabs(xMin.x), std::fabs(xMax.x),
				std::fabs(xMin.y), std::fabs(xMax.y),
				std::fabs(xMin.z), std::fabs(xMax.z) };
			float fMaxAbs = 0.0f;
			for (u_int a = 0u; a < 6u; ++a)
			{
				if (afAbs[a] > fMaxAbs) { fMaxAbs = afAbs[a]; }
			}
			fBudget = fMaxAbs / ZM_InteriorTileMetres(xInterior.m_eRoom, eS) + 1.0e-3f;
			if (fBudget < 1.0f + 1.0e-3f) { fBudget = 1.0f + 1.0e-3f; }   // the object-mapped rug
		}

		xV.m_axSurface[s] = ZM_ValidateInteriorSurfaceMesh(xMesh, fBudget);
		xV.m_abTexturesNonEmpty[s] = xInterior.m_axTextures[s].NonEmpty();
		xV.m_bAllValid = xV.m_bAllValid && xV.m_axSurface[s].m_bAllValid
			&& xV.m_abTexturesNonEmpty[s];
	}
	return xV;
}

// ============================================================================
// Asset paths.
// ============================================================================
ZM_INTERIOR_ASSET_KIND ZM_InteriorSurfaceAssetKind(ZM_INTERIOR_SURFACE eSurface,
	ZM_INTERIOR_ASSET_SLOT eSlot)
{
	if (eSurface >= ZM_INTERIOR_SURFACE_COUNT || eSlot >= ZM_INTERIOR_SLOT_COUNT)
	{
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_InteriorGen] ZM_InteriorSurfaceAssetKind: (%u, %u) is not a "
			"surface/slot pair -- answering (FLOOR, MESH)", (u_int)eSurface, (u_int)eSlot);
		return static_cast<ZM_INTERIOR_ASSET_KIND>(0u);
	}
	return static_cast<ZM_INTERIOR_ASSET_KIND>(
		static_cast<u_int>(eSurface) * static_cast<u_int>(ZM_INTERIOR_SLOT_COUNT)
		+ static_cast<u_int>(eSlot));
}

ZM_INTERIOR_SURFACE ZM_InteriorAssetSurface(ZM_INTERIOR_ASSET_KIND eKind)
{
	if (static_cast<u_int>(eKind) >= static_cast<u_int>(ZM_INTERIOR_ASSET_MODEL))
	{
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_InteriorGen] ZM_InteriorAssetSurface: kind %u is not per-surface -- "
			"answering FLOOR", (u_int)eKind);
		return ZM_INTERIOR_SURFACE_FLOOR;
	}
	return static_cast<ZM_INTERIOR_SURFACE>(
		static_cast<u_int>(eKind) / static_cast<u_int>(ZM_INTERIOR_SLOT_COUNT));
}

ZM_INTERIOR_ASSET_SLOT ZM_InteriorAssetSlot(ZM_INTERIOR_ASSET_KIND eKind)
{
	if (static_cast<u_int>(eKind) >= static_cast<u_int>(ZM_INTERIOR_ASSET_MODEL))
	{
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_InteriorGen] ZM_InteriorAssetSlot: kind %u is not per-surface -- "
			"answering MESH", (u_int)eKind);
		return ZM_INTERIOR_SLOT_MESH;
	}
	return static_cast<ZM_INTERIOR_ASSET_SLOT>(
		static_cast<u_int>(eKind) % static_cast<u_int>(ZM_INTERIOR_SLOT_COUNT));
}

bool ZM_InteriorAssetPath(ZM_INTERIOR_ROOM eRoom, ZM_INTERIOR_ASSET_KIND eKind,
	char* szOut, u_int uCap)
{
	Zenith_Assert(szOut != nullptr, "ZM_InteriorAssetPath: null output buffer");
	if (szOut == nullptr || uCap == 0u)
	{
		return false;
	}
	szOut[0] = '\0';

	const char* szRoom = ZM_InteriorRoomName(eRoom);
	char acBase[160];
	int iBase = -1;
	if (eKind == ZM_INTERIOR_ASSET_MODEL)
	{
		iBase = snprintf(acBase, sizeof(acBase), "%s.zmodel", szRoom);
	}
	else
	{
		iBase = snprintf(acBase, sizeof(acBase), ZM_IGSlotFmt(ZM_InteriorAssetSlot(eKind)),
			szRoom, ZM_InteriorSurfaceName(ZM_InteriorAssetSurface(eKind)));
	}
	if (iBase < 0 || static_cast<u_int>(iBase) >= sizeof(acBase))
	{
		return false;
	}

	const int iN = snprintf(szOut, uCap, "game:Interiors/%s/%s", szRoom, acBase);
	return iN >= 0 && static_cast<u_int>(iN) < uCap;
}

// ============================================================================
// Disk bake (TOOLS ONLY).
// ============================================================================
#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Collections/Zenith_Vector.h"
#include "Zenithmon/Source/Gen/ZM_BakeManifest.h"
#include <filesystem>
#include <string>

namespace
{
	bool ZM_IGResolve(ZM_INTERIOR_ROOM eRoom, ZM_INTERIOR_SURFACE eSurface,
		ZM_INTERIOR_ASSET_SLOT eSlot, std::string& strRefOut, std::string& strFsOut)
	{
		char acRef[512];
		if (!ZM_InteriorAssetPath(eRoom, ZM_InteriorSurfaceAssetKind(eSurface, eSlot),
			acRef, sizeof(acRef)))
		{
			return false;
		}
		strRefOut = acRef;
		strFsOut  = Zenith_AssetRegistry::ResolvePath(strRefOut);
		return true;
	}

	bool ZM_IGBundlePresent(ZM_INTERIOR_ROOM eRoom)
	{
		for (u_int k = 0u; k < static_cast<u_int>(ZM_INTERIOR_ASSET_KIND_COUNT); ++k)
		{
			char acRef[512];
			if (!ZM_InteriorAssetPath(eRoom, static_cast<ZM_INTERIOR_ASSET_KIND>(k),
				acRef, sizeof(acRef)))
			{
				return false;
			}
			const std::filesystem::path xPath(
				Zenith_AssetRegistry::ResolvePath(std::string(acRef)));
			std::error_code xEc;
			if (!std::filesystem::is_regular_file(xPath, xEc) || xEc)   { return false; }
			if (std::filesystem::file_size(xPath, xEc) == 0u || xEc)    { return false; }
		}
		return true;
	}
}

bool ZM_BakeInterior(ZM_INTERIOR_ROOM eRoom)
{
	ZM_Interior xInterior;
	ZM_BuildInterior(eRoom, xInterior);

	const std::string strRoom = ZM_InteriorRoomName(eRoom);

	char acModelRef[512];
	if (!ZM_InteriorAssetPath(eRoom, ZM_INTERIOR_ASSET_MODEL, acModelRef, sizeof(acModelRef)))
	{
		return false;
	}
	const std::string strModelFs = Zenith_AssetRegistry::ResolvePath(std::string(acModelRef));

	bool bOk = true;
	Zenith_Vector<std::string> xMeshRefs;
	Zenith_Vector<std::string> xMatRefs;

	for (u_int s = 0u; s < static_cast<u_int>(ZM_INTERIOR_SURFACE_COUNT); ++s)
	{
		const ZM_INTERIOR_SURFACE eS = static_cast<ZM_INTERIOR_SURFACE>(s);
		const ZM_InteriorTextureSet& xTex = xInterior.m_axTextures[s];

		std::string strMeshRef, strMeshFs, strAlbRef, strAlbFs, strNrmRef, strNrmFs;
		std::string strRmRef, strRmFs, strAoRef, strAoFs, strMatRef, strMatFs;
		bOk &= ZM_IGResolve(eRoom, eS, ZM_INTERIOR_SLOT_MESH,        strMeshRef, strMeshFs);
		bOk &= ZM_IGResolve(eRoom, eS, ZM_INTERIOR_SLOT_ALBEDO,      strAlbRef,  strAlbFs);
		bOk &= ZM_IGResolve(eRoom, eS, ZM_INTERIOR_SLOT_NORMAL,      strNrmRef,  strNrmFs);
		bOk &= ZM_IGResolve(eRoom, eS, ZM_INTERIOR_SLOT_ROUGH_METAL, strRmRef,   strRmFs);
		bOk &= ZM_IGResolve(eRoom, eS, ZM_INTERIOR_SLOT_OCCLUSION,   strAoRef,   strAoFs);
		bOk &= ZM_IGResolve(eRoom, eS, ZM_INTERIOR_SLOT_MATERIAL,    strMatRef,  strMatFs);
		if (!bOk)
		{
			return false;
		}

		bOk &= ZM_GenBakeStaticMesh(xInterior.m_axMesh[s], strMeshFs.c_str());
		bOk &= ZM_SynthBakeAlbedoBC1(xTex.m_xAlbedo,            strAlbFs.c_str());
		bOk &= ZM_SynthBakeNormalBC5(xTex.m_xNormal,            strNrmFs.c_str());
		bOk &= ZM_SynthBakeLinearBC1(xTex.m_xRoughnessMetallic, strRmFs.c_str());
		bOk &= ZM_SynthBakeLinearBC1(xTex.m_xOcclusion,         strAoFs.c_str());

		{
			const ZM_InteriorSurfaceLook xL = ZM_GetInteriorSurfaceLook(eRoom, eS);
			Zenith_AssetHandle<Zenith_MaterialAsset> xMat =
				Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
			Zenith_MaterialAsset* pxMat = xMat.GetDirect();
			pxMat->SetName(strRoom + "_" + ZM_InteriorSurfaceName(eS));
			pxMat->SetDiffuseTexture(TextureHandle(strAlbRef));
			pxMat->SetNormalTexture(TextureHandle(strNrmRef));
			pxMat->SetRoughnessMetallicTexture(TextureHandle(strRmRef));
			pxMat->SetOcclusionTexture(TextureHandle(strAoRef));
			pxMat->SetRoughness(xL.m_fRoughness);
			pxMat->SetMetallic(xL.m_fMetallic);
			pxMat->SetNormalStrength(xL.m_fNormalStrength);
			pxMat->SetOcclusionStrength(xL.m_fOcclusion);
			if (xL.m_fEmissiveIntensity > 0.0f)
			{
				// No mask: the whole card glows, uniformly, at the HDR value the
				// look table asks for. The default emissive texture is white.
				pxMat->SetEmissiveColor(xL.m_xEmissiveColour);
				pxMat->SetEmissiveIntensity(xL.m_fEmissiveIntensity);
			}
			if (xL.m_bTranslucent)
			{
				// The forward translucent pass reads its alpha from the sampled
				// albedo times the base colour; the baked albedo is opaque, so the
				// base colour's alpha IS the opacity.
				pxMat->SetBlendMode(MATERIAL_BLEND_TRANSLUCENT);
				pxMat->SetBaseColor(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, xL.m_fOpacity));
			}
			pxMat->SaveToFile(strMatFs);
		}

		xMeshRefs.PushBack(strMeshRef);
		xMatRefs.PushBack(strMatRef);

		std::error_code xEcS;
		bOk &= std::filesystem::exists(std::filesystem::path(strMatFs), xEcS);
	}

	{
		Zenith_AssetHandle<Zenith_ModelAsset> xModel =
			Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		Zenith_ModelAsset* pxModel = xModel.GetDirect();
		pxModel->SetName(strRoom);
		for (u_int s = 0u; s < xMeshRefs.GetSize(); ++s)
		{
			Zenith_Vector<std::string> xMats;
			xMats.PushBack(xMatRefs.Get(s));
			pxModel->AddMeshByPath(xMeshRefs.Get(s), xMats);
		}
		pxModel->Export(strModelFs.c_str());
	}

	std::error_code xEc;
	bOk &= std::filesystem::exists(std::filesystem::path(strModelFs), xEc);
	return bOk;
}

bool ZM_BakeAllInteriors()
{
	const std::filesystem::path xRoot(GAME_ASSETS_DIR);
	if (ZM_BakeManifestCheck(ZM_ASSET_FAMILY_INTERIORS, xRoot))
	{
		return true;
	}
	bool bOk = true;
	for (u_int u = 0u; u < static_cast<u_int>(ZM_INTERIOR_ROOM_COUNT); ++u)
	{
		bOk &= ZM_BakeInterior(static_cast<ZM_INTERIOR_ROOM>(u));
	}
	if (bOk)
	{
		bOk &= ZM_WriteBakeManifest(ZM_ASSET_FAMILY_INTERIORS, xRoot);
	}
	return bOk;
}

bool ZM_EnsureInteriorBaked(ZM_INTERIOR_ROOM eRoom)
{
	if (static_cast<u_int>(eRoom) >= static_cast<u_int>(ZM_INTERIOR_ROOM_COUNT))
	{
		return false;
	}
	if (ZM_IGBundlePresent(eRoom))
	{
		return true;
	}
	if (!ZM_BakeInterior(eRoom))
	{
		return false;
	}
	return ZM_IGBundlePresent(eRoom);
}
#endif   // ZENITH_TOOLS
