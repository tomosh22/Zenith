#include "Zenith.h"

// ============================================================================
// ZM_InteriorGen -- the room-shell generator driver. See the header for the
// architecture + determinism contract. This TU owns: room -> spec + recipe
// resolution, the per-room/per-surface look table, the four inward-facing
// surface mesh builders, the PBR map builders, the bundle driver, the
// byte-identity + hash + validation machinery, the asset-path scheme, and (tools
// only) the disk bake.
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
			// long timber BOARDS -- many rows, one column, staggered ends. The lab is
			// a square resin TILE grid. Same primitive, opposite reading.
			return bHome ? ZM_IGCourseSpec{ 10u, 1u, 0.012f, true,  0.45f }
			             : ZM_IGCourseSpec{  4u, 4u, 0.020f, false, 0.30f };
		case ZM_INTERIOR_SURFACE_WALL:
			// Home: plaster, no courses at all -- just a fine skim texture. Lab:
			// panel joints on a wide grid.
			return bHome ? ZM_IGCourseSpec{  1u, 1u, 0.0f,   false, 0.0f  }
			             : ZM_IGCourseSpec{  2u, 2u, 0.014f, false, 0.35f };
		case ZM_INTERIOR_SURFACE_CEILING:
			return bHome ? ZM_IGCourseSpec{  1u, 1u, 0.0f,   false, 0.0f  }
			             : ZM_IGCourseSpec{  3u, 3u, 0.016f, false, 0.25f };
		case ZM_INTERIOR_SURFACE_TRIM:
		default:
			return ZM_IGCourseSpec{ 1u, 1u, 0.0f, false, 0.0f };
		}
	}
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
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_InteriorGen] ZM_InteriorSurfaceName: %u is not a surface -- answering floor",
			(u_int)eSurface);
		return "floor";
	}
}

u_int ZM_InteriorSurfaceResolution(ZM_INTERIOR_SURFACE eSurface)
{
	// Small on purpose: these tile, so density is set by the tile size in metres.
	// 256 on a 1.2 m board run is 213 px/m, which is more than a player standing on
	// it can resolve.
	switch (eSurface)
	{
	case ZM_INTERIOR_SURFACE_FLOOR:   return 256u;
	case ZM_INTERIOR_SURFACE_WALL:    return 256u;
	case ZM_INTERIOR_SURFACE_CEILING: return 128u;
	case ZM_INTERIOR_SURFACE_TRIM:    return 128u;
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
// The look table.
//
// ★★ THIS TABLE IS THE ANSWER TO ZM-D-176, and it is written to be read as a
// pair. Every row for PLAYER_HOME is warm (R > B) and every row for PROF_LAB is
// cool (B > R), because the property the ruling asks for -- "the bedroom must
// stop reading as the lab" -- is measured by ZM_InteriorTintPixels_Test as a
// red/blue ratio gap. A hue nudge on shared grey blocks reached 0.121 against a
// 0.15 floor; two genuinely different sets of materials clear it with room to
// spare, because the floor, the walls, the ceiling AND the trim all move
// together instead of one tint fighting a shared albedo.
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
			xL.m_fRoughness = 0.28f; xL.m_fNormalStrength = 0.70f;
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
	default:
		if (bHome)
		{
			// Stained skirting, a shade darker than the boards.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.30f, 0.18f, 0.09f);
			xL.m_fRoughness = 0.38f; xL.m_fNormalStrength = 0.75f;
		}
		else
		{
			// ★ THE ONE METALLIC SURFACE IN EITHER ROOM. Brushed stainless skirting
			// and bench rail: metallic=1 is CORRECT here in a way it never is on a
			// building facade, because this really is a conductor -- its diffuse
			// should vanish and its reflection should take the base colour's tint.
			xL.m_xBaseColour = Zenith_Maths::Vector3(0.58f, 0.62f, 0.68f);
			xL.m_fRoughness = 0.30f; xL.m_fMetallic = 1.0f;
			xL.m_fNormalStrength = 0.50f;
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
	void ZM_IGBuildFloor(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_FLOOR);
		const float fX = xS.InnerHalfWidth();
		const float fZ = xS.InnerHalfDepth();
		// The slab's TOP face is at exactly y=0 -- the floor plane every spawn
		// marker and every authored body in these scenes is placed against.
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fX, -fZM_IG_SLAB, -fZ),
			Zenith_Maths::Vector3( fX, 0.0f,          fZ), fTile);
	}

	void ZM_IGBuildCeiling(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_CEILING);
		const float fX = xS.InnerHalfWidth();
		const float fZ = xS.InnerHalfDepth();
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fX, xS.m_fWallHeight,              -fZ),
			Zenith_Maths::Vector3( fX, xS.m_fWallHeight + fZM_IG_SLAB, fZ), fTile);
	}

	void ZM_IGBuildWalls(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_WALL);
		const float fX = xS.InnerHalfWidth();
		const float fZ = xS.InnerHalfDepth();
		const float fH = xS.m_fWallHeight;
		const float fA = xS.m_fApertureHalfW;

		// -Z back wall.
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fX, 0.0f, -fZ - fZM_IG_SLAB),
			Zenith_Maths::Vector3( fX, fH,   -fZ), fTile);
		// -X and +X side walls.
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fX - fZM_IG_SLAB, 0.0f, -fZ),
			Zenith_Maths::Vector3(-fX,               fH,    fZ), fTile);
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(fX,               0.0f, -fZ),
			Zenith_Maths::Vector3(fX + fZM_IG_SLAB, fH,    fZ), fTile);

		// +Z entrance wall: two panels flanking the aperture, plus the lintel over
		// it. The aperture is a REAL GAP -- the scene's blockouts leave it open and
		// the player walks through it -- so nothing is emitted across it below the
		// lintel.
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fX, 0.0f, fZ),
			Zenith_Maths::Vector3(-fA, fH,   fZ + fZM_IG_SLAB), fTile);
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(fA, 0.0f, fZ),
			Zenith_Maths::Vector3(fX, fH,   fZ + fZM_IG_SLAB), fTile);
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fA, xS.m_fApertureHeight, fZ),
			Zenith_Maths::Vector3( fA, fH,                   fZ + fZM_IG_SLAB), fTile);
	}

	void ZM_IGBuildTrim(const ZM_InteriorRecipe& xR, ZM_GenMesh& xMesh)
	{
		const ZM_InteriorRoomSpec& xS = xR.m_xSpec;
		const float fTile = ZM_InteriorTileMetres(xR.m_eRoom, ZM_INTERIOR_SURFACE_TRIM);
		const float fX = xS.InnerHalfWidth();
		const float fZ = xS.InnerHalfDepth();
		const float fA = xS.m_fApertureHalfW;

		// Skirting: a proud board along the base of every wall. It is the single
		// cheapest thing that stops a room reading as a box -- it draws the
		// wall/floor junction as a line instead of a seam.
		constexpr float fSkirtH = 0.14f;
		constexpr float fSkirtP = 0.035f;   // proud of the wall face, into the room
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fX, 0.0f, -fZ),
			Zenith_Maths::Vector3( fX, fSkirtH, -fZ + fSkirtP), fTile);
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fX,           0.0f,    -fZ),
			Zenith_Maths::Vector3(-fX + fSkirtP, fSkirtH,  fZ), fTile);
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(fX - fSkirtP, 0.0f,    -fZ),
			Zenith_Maths::Vector3(fX,           fSkirtH,  fZ), fTile);
		// The entrance wall's two skirting runs stop at the aperture.
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fX, 0.0f,    fZ - fSkirtP),
			Zenith_Maths::Vector3(-fA, fSkirtH, fZ), fTile);
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(fA, 0.0f,    fZ - fSkirtP),
			Zenith_Maths::Vector3(fX, fSkirtH, fZ), fTile);

		// The door reveal: a lining around the aperture, standing into the room so
		// the opening has an edge rather than a paper-thin cut.
		constexpr float fRevealW = 0.10f;
		constexpr float fRevealP = 0.05f;
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fA - fRevealW, 0.0f, fZ - fRevealP),
			Zenith_Maths::Vector3(-fA,            xS.m_fApertureHeight + fRevealW, fZ), fTile);
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(fA,            0.0f, fZ - fRevealP),
			Zenith_Maths::Vector3(fA + fRevealW, xS.m_fApertureHeight + fRevealW, fZ), fTile);
		ZM_StaticMesh::AppendWorldBox(xMesh,
			Zenith_Maths::Vector3(-fA - fRevealW, xS.m_fApertureHeight, fZ - fRevealP),
			Zenith_Maths::Vector3( fA + fRevealW, xS.m_fApertureHeight + fRevealW, fZ), fTile);
	}
}

void ZM_BuildInteriorSurfaceMesh(const ZM_InteriorRecipe& xR,
	ZM_INTERIOR_SURFACE eSurface, ZM_GenMesh& xMesh)
{
	xMesh.Reset();
	switch (eSurface)
	{
	case ZM_INTERIOR_SURFACE_FLOOR:   ZM_IGBuildFloor  (xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_WALL:    ZM_IGBuildWalls  (xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_CEILING: ZM_IGBuildCeiling(xR, xMesh); break;
	case ZM_INTERIOR_SURFACE_TRIM:    ZM_IGBuildTrim   (xR, xMesh); break;
	default:
		Zenith_Error(LOG_CATEGORY_MESH,
			"[ZM_InteriorGen] ZM_BuildInteriorSurfaceMesh: %u is not a surface -- "
			"emitting the floor", (u_int)eSurface);
		ZM_IGBuildFloor(xR, xMesh);
		break;
	}
	// Hard per-face normals are already written by AppendBox; do NOT re-run
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

			float fH = 0.5f + (ZM_SynthFbm(fU, fV, 16u, uSalt) - 0.5f) * 0.18f;

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
				// the trim, stretched along one axis.
				fH += (ZM_SynthValueNoise(fU * 0.3f, fV * 8.0f, 12u, uSalt + 7u) - 0.5f)
					* (eSurface == ZM_INTERIOR_SURFACE_TRIM ? 0.10f : 0.06f);
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

	const Zenith_Maths::Vector3 xBase(
		ZM_IGClamp01(xL.m_xBaseColour.x + fJitR),
		ZM_IGClamp01(xL.m_xBaseColour.y + fJitG),
		ZM_IGClamp01(xL.m_xBaseColour.z + fJitB));

	ZM_InteriorTextureSet xOut;
	xOut.m_xAlbedo            = ZM_GenImage(uRes, uRes);

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

			// Wear collects in the cavities. Kept light: an interior that reads
			// grimy fights the "somebody lives here" brief.
			const float fWorn = ZM_IGClamp01(fCavity * fWear
				+ ZM_SynthFbm(fU, fV, 5u, uSalt + 211u) * fWear * 0.4f);
			xCol = ZM_IGLerp3(xCol, Zenith_Maths::Vector3(0.16f, 0.14f, 0.12f), fWorn * 0.25f);

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
	xPbr.m_fRoughnessJitter = fRoughJit;
	xPbr.m_fCavityRoughness = 0.16f;
	xPbr.m_fCavityOcclusion = 0.50f;
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
// Determinism + validation.
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
		uHash = ZM_IGFnvAccumBuffer(uHash, xM.m_xIndices);

		const ZM_InteriorTextureSet& xT = xInterior.m_axTextures[s];
		const u_int auMaps[4] = { xT.m_xAlbedo.ContentHash(), xT.m_xNormal.ContentHash(),
			xT.m_xRoughnessMetallic.ContentHash(), xT.m_xOcclusion.ContentHash() };
		uHash = ZM_IGFnvAccum(uHash, auMaps, sizeof(auMaps));
	}
	return uHash;
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

	xV.m_bNoSkeleton    = (xMesh.GetNumBones() == 0u);
	xV.m_bNoSkinBuffers = (xMesh.m_xBoneIndices.GetSize() == 0u)
		&& (xMesh.m_xBoneWeights.GetSize() == 0u);

	xV.m_bAllValid = xV.m_bWindingOutward && xV.m_bBoundsNonDegen
		&& xV.m_bIndicesInRange && xV.m_bUVsFiniteAndBounded
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
