#include "Zenith.h"

// ============================================================================
// ZM_Tests_InteriorGen -- the boot units for the ROOMS: the window openings, the
// baked vertex occlusion and wear, the floorboard geometry, and the light
// fixtures that make the two interiors read as photographed rooms rather than
// lit boxes.
//
// PURE / HEADLESS. Every clause reads compiled tables or an in-memory
// ZM_BuildInterior bundle: no scene, no entity, no physics, no assets, no
// graphics, no g_xEngine, no ZENITH_TOOLS reach. They run at boot before the
// scene loads, on the Null backend like every other ZM unit.
//
// ★ WHAT THESE CANNOT DO, STATED UP FRONT so their greenness is not oversold.
// They prove the GEOMETRY is a hole, the SHADING is written, the fixture
// BRACKETS its lamp and the sun vector is exactly unit-length. They cannot prove
// a window looks like a window: that a pane reads as glass and a sky card reads
// as daylight is a judgement about pixels, and the only tests that reach a pixel
// (ZM_InteriorTintPixels_Test) are graphics-required and therefore SKIP -- i.e.
// pass -- in the headless gate. Do not read this file's greenness as "the rooms
// look right".
//
// ★★ AND THE RAY CLAUSES ARE THE POINT OF THE FILE. "The wall has a hole in it"
// is exactly the kind of claim that a mesh builder can silently stop honouring
// -- an off-by-one in the cutter leaves a pane of wall across the opening, the
// room still loads, every bounds clause still passes, and the window is simply
// blind. A ray cast through the opening is the only thing that can see it, so
// each one is paired with an ANTI-VACUITY ray through solid wall that MUST hit.
// ============================================================================

#include <cmath>
#include <cstring>

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_InteriorGen.h"
#include "Zenithmon/Source/Gen/ZM_PropGen.h"
#include "Zenithmon/Source/World/ZM_InteriorDressing.h"
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"   // THE body contract -- never re-spelled
#include "Zenithmon/Components/ZM_FollowCamera.h"

#include <bit>

namespace
{
	// "The same authored number." Generous by float standards, far below any
	// quantity these rooms are built from.
	constexpr float fIG_EPSILON = 1.0e-4f;

	// A point just inside the room from a wall's inner face, on the wall's own
	// normal. The ray casts start here so a hit at t ~ 0 on the wall's own inner
	// skin cannot be mistaken for a miss.
	Zenith_Maths::Vector3 IGRayStart(const ZM_InteriorRoomSpec& xSpec,
		const ZM_InteriorWindow& xWindow, float fY, float fZ)
	{
		const float fX = xSpec.InnerHalfWidth() - 0.05f;
		return Zenith_Maths::Vector3(xWindow.WallSign() * fX, fY, fZ);
	}

	Zenith_Maths::Vector3 IGOutward(const ZM_InteriorWindow& xWindow)
	{
		return Zenith_Maths::Vector3(xWindow.WallSign(), 0.0f, 0.0f);
	}

	// The mean of a mesh's vertex colour over the vertices nearest a point --
	// the shading query the occlusion clauses are written in terms of. Takes
	// EVERY vertex in the ball, which is what a wall wants: a wall is one layer.
	float IGMeanBrightnessNear(const ZM_GenMesh& xMesh, const Zenith_Maths::Vector3& xAt,
		float fRadius, u_int& uCountOut)
	{
		float fSum = 0.0f;
		uCountOut = 0u;
		for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
		{
			const Zenith_Maths::Vector3 xD = xMesh.m_xPositions.Get(v) - xAt;
			if (glm::dot(xD, xD) > fRadius * fRadius) { continue; }
			const Zenith_Maths::Vector4& xC = xMesh.m_xColors.Get(v);
			fSum += (xC.x + xC.y + xC.z) * (1.0f / 3.0f);
			++uCountOut;
		}
		return uCountOut > 0u ? fSum / static_cast<float>(uCountOut) : 0.0f;
	}

	// ★★★ THE WALKING SURFACE ONLY -- WHICH IS NOT THE SAME AS "UP-FACING".
	// PlayerHome's floor is TWO up-facing layers: the boards at the walking plane
	// and the dark underlay 12 mm beneath them, whose top face points up just as
	// squarely (n.y = +1). A probe filtered on ORIENTATION therefore still
	// averages both, and it averages them in whatever proportion the underlay's
	// 3 m grid corners happen to fall inside the ball -- measured at 30% underlay
	// on the door axis against 40% beside it, which is enough to invert the
	// comparison on its own. The board component was reading ~0.94 against 1.00
	// the whole time: the wear was always reaching the boards, and the probe was
	// reporting a mixture.
	//
	// The topmost layer is DISCOVERED from the mesh's own bounds rather than
	// spelled as a constant, so this follows the boards if their lip depth ever
	// changes and re-spells nothing the generator owns.
	float IGMeanBrightnessOnWalkingSurface(const ZM_GenMesh& xMesh,
		const Zenith_Maths::Vector3& xAt, float fRadius, u_int& uCountOut)
	{
		// Comfortably deeper than a board lip (millimetres) and far shallower than
		// the drop to the underlay, so it selects the top layer and only that.
		constexpr float fTOP_LAYER_BAND = 0.005f;
		const float fTopY = ZM_GenMeshBoundsMax(xMesh).y;

		float fSum = 0.0f;
		uCountOut = 0u;
		for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
		{
			if (xMesh.m_xNormals.Get(v).y <= 0.5f) { continue; }                  // up-facing
			if (xMesh.m_xPositions.Get(v).y < fTopY - fTOP_LAYER_BAND) { continue; }  // ...and the TOP layer
			const Zenith_Maths::Vector3 xD = xMesh.m_xPositions.Get(v) - xAt;
			if (glm::dot(xD, xD) > fRadius * fRadius) { continue; }
			const Zenith_Maths::Vector4& xC = xMesh.m_xColors.Get(v);
			fSum += (xC.x + xC.y + xC.z) * (1.0f / 3.0f);
			++uCountOut;
		}
		return uCountOut > 0u ? fSum / static_cast<float>(uCountOut) : 0.0f;
	}

	// The distinct heights of a mesh's UP-FACING vertices, to 0.1 mm. This is how
	// "boards standing on an underlay" is told from "one slab cut into cells":
	// the first has two separated up-facing layers, the second has one. Discovered
	// from the mesh, so no generator constant is re-spelled here.
	void IGCollectUpFacingHeights(const ZM_GenMesh& xMesh, float* pfOut, u_int uCap,
		u_int& uCountOut)
	{
		uCountOut = 0u;
		for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
		{
			if (xMesh.m_xNormals.Get(v).y <= 0.5f) { continue; }
			const float fY = xMesh.m_xPositions.Get(v).y;
			bool bSeen = false;
			for (u_int t = 0u; t < uCountOut; ++t)
			{
				if (fabsf(pfOut[t] - fY) < 1.0e-4f) { bSeen = true; break; }
			}
			if (!bSeen && uCountOut < uCap) { pfOut[uCountOut++] = fY; }
		}
	}
}

// ############################################################################
// 1. The windows are REAL HOLES
// ############################################################################

// For every window in both rooms: a ray from inside the room, through the centre
// of the opening, along the wall's outward normal, hits NO wall triangle -- and
// the SAME ray displaced to solid wall beside the opening DOES. The pair is what
// makes this a test rather than a coincidence: a mesh that emitted no wall at
// all would pass the first clause and fail the second.
ZENITH_TEST(ZM_Gen, InteriorWindowsAreRealHolesInTheWall)
{
	u_int uWindowsChecked = 0u;
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const ZM_InteriorRoomSpec xSpec = ZM_GetInteriorRoomSpec(eRoom);

		ZM_Interior xInterior;
		ZM_BuildInterior(eRoom, xInterior);
		const ZM_GenMesh& xWall = xInterior.m_axMesh[ZM_INTERIOR_SURFACE_WALL];

		const u_int uWindows = ZM_GetInteriorWindowCount(eRoom);
		ZENITH_ASSERT_GT(uWindows, 0u,
			"%s has no windows at all -- every daylight clause below passes "
			"vacuously, and the room is lit only by its own lamps",
			ZM_InteriorRoomName(eRoom));

		for (u_int w = 0u; w < uWindows; ++w)
		{
			const ZM_InteriorWindow& xWindow = ZM_GetInteriorWindow(eRoom, w);
			++uWindowsChecked;

			const float fMidY = 0.5f * (xWindow.m_fSillY + xWindow.m_fHeadY);
			float fT = 0.0f;

			// (a) THROUGH THE OPENING: no wall.
			const bool bThroughHits = ZM_InteriorMeshRayHits(xWall,
				IGRayStart(xSpec, xWindow, fMidY, xWindow.m_fCentreZ), IGOutward(xWindow), fT);
			ZENITH_ASSERT_FALSE(bThroughHits,
				"%s window %u is BLIND: a ray through the middle of its opening "
				"(z=%.2f, y=%.2f) hits wall geometry at t=%.4f. The opening is drawn "
				"and dressed but the wall was never cut, so the frame, the glass and "
				"the sky card are all behind a solid panel",
				ZM_InteriorRoomName(eRoom), w, (double)xWindow.m_fCentreZ, (double)fMidY,
				(double)fT);

			// (b) ANTI-VACUITY -- beside the opening there MUST be wall. Half a
			//     metre outboard of the jamb, at the same height.
			const float fSolidZ = xWindow.Z1() + 0.50f;
			const bool bSolidHits = ZM_InteriorMeshRayHits(xWall,
				IGRayStart(xSpec, xWindow, fMidY, fSolidZ), IGOutward(xWindow), fT);
			ZENITH_ASSERT_TRUE(bSolidHits,
				"%s window %u: the wall 0.5 m beside the opening (z=%.2f) is ALSO "
				"missing, so the 'is a hole' clause above proved nothing -- the "
				"cutter has removed more than the window",
				ZM_InteriorRoomName(eRoom), w, (double)fSolidZ);

			// (c) ...and BELOW the sill, which is the edge a cutter most easily
			//     takes too much of.
			const bool bUnderSillHits = ZM_InteriorMeshRayHits(xWall,
				IGRayStart(xSpec, xWindow, xWindow.m_fSillY - 0.25f, xWindow.m_fCentreZ),
				IGOutward(xWindow), fT);
			ZENITH_ASSERT_TRUE(bUnderSillHits,
				"%s window %u has no wall under its sill (y=%.2f) -- the opening "
				"runs to the floor", ZM_InteriorRoomName(eRoom), w,
				(double)(xWindow.m_fSillY - 0.25f));
		}

		// The DOOR is a hole in the same cutter, and it was one before this change
		// -- so it is the regression guard on the shared machinery.
		float fT = 0.0f;
		const bool bDoorHits = ZM_InteriorMeshRayHits(xWall,
			Zenith_Maths::Vector3(0.0f, 0.5f * xSpec.m_fApertureHeight, xSpec.InnerHalfDepth() - 0.05f),
			Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), fT);
		ZENITH_ASSERT_FALSE(bDoorHits,
			"%s's DOOR aperture is blocked by wall geometry at t=%.4f -- the warp "
			"aperture is the way in and out of this scene",
			ZM_InteriorRoomName(eRoom), (double)fT);
	}

	ZENITH_ASSERT_GE(uWindowsChecked, 4u,
		"only %u windows were checked across both rooms", uWindowsChecked);
}

// Every piece of window dressing lives INSIDE the wall it is set in: nothing
// reaches past the wall CENTRELINE (which is where the blockout the player stops
// against sits), and nothing but the sill and the curtains stands into the room.
ZENITH_TEST(ZM_Gen, InteriorWindowDressingStaysInsideTheWall)
{
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const ZM_InteriorRoomSpec xSpec = ZM_GetInteriorRoomSpec(eRoom);

		ZM_Interior xInterior;
		ZM_BuildInterior(eRoom, xInterior);

		const ZM_INTERIOR_SURFACE aeSurfaces[] = {
			ZM_INTERIOR_SURFACE_WINDOW, ZM_INTERIOR_SURFACE_GLASS,
			ZM_INTERIOR_SURFACE_SKY,    ZM_INTERIOR_SURFACE_CURTAIN };
		for (u_int s = 0u; s < (u_int)(sizeof(aeSurfaces) / sizeof(aeSurfaces[0])); ++s)
		{
			const ZM_GenMesh& xMesh = xInterior.m_axMesh[aeSurfaces[s]];
			const char* szName = ZM_InteriorSurfaceName(aeSurfaces[s]);
			ZENITH_ASSERT_GT(xMesh.GetNumTris(), 0u,
				"%s/%s is empty -- a degenerate .zmesh fails the whole model load",
				ZM_InteriorRoomName(eRoom), szName);

			const Zenith_Maths::Vector3 xMin = ZM_GenMeshBoundsMin(xMesh);
			const Zenith_Maths::Vector3 xMax = ZM_GenMeshBoundsMax(xMesh);
			ZENITH_ASSERT_LE(xMax.x, xSpec.m_fHalfWidth + fIG_EPSILON,
				"%s/%s reaches x=%.4f, past the +X wall centreline at %.4f -- it "
				"would poke out of the building",
				ZM_InteriorRoomName(eRoom), szName, (double)xMax.x, (double)xSpec.m_fHalfWidth);
			ZENITH_ASSERT_GE(xMin.x, -xSpec.m_fHalfWidth - fIG_EPSILON,
				"%s/%s reaches x=%.4f, past the -X wall centreline at -%.4f",
				ZM_InteriorRoomName(eRoom), szName, (double)xMin.x, (double)xSpec.m_fHalfWidth);
			// ...and above the floor, below the ceiling.
			ZENITH_ASSERT_GE(xMin.y, -fIG_EPSILON,
				"%s/%s dips to y=%.4f, below the floor plane",
				ZM_InteriorRoomName(eRoom), szName, (double)xMin.y);
			ZENITH_ASSERT_LE(xMax.y, xSpec.m_fWallHeight + fIG_EPSILON,
				"%s/%s reaches y=%.4f, above the %.2f m ceiling",
				ZM_InteriorRoomName(eRoom), szName, (double)xMax.y, (double)xSpec.m_fWallHeight);
		}

		// The SKY CARD is behind the GLASS, which is behind the frame's room-side
		// face. Get this backwards and the card is in front of the pane -- the
		// window still glows, and the glass is behind the daylight it should be
		// filtering.
		ZENITH_ASSERT_GT(fZM_INTERIOR_WINDOW_REVEAL_DEPTH, fZM_INTERIOR_WINDOW_GLASS_DEPTH,
			"the sky card (%.3f into the wall) is not behind the glass (%.3f)",
			(double)fZM_INTERIOR_WINDOW_REVEAL_DEPTH, (double)fZM_INTERIOR_WINDOW_GLASS_DEPTH);
		ZENITH_ASSERT_GT(fZM_INTERIOR_WINDOW_GLASS_DEPTH, fZM_INTERIOR_WINDOW_FRAME_DEPTH - 0.05f,
			"the glass (%.3f) is not inside the frame (%.3f)",
			(double)fZM_INTERIOR_WINDOW_GLASS_DEPTH, (double)fZM_INTERIOR_WINDOW_FRAME_DEPTH);
	}
}

// The windows clear the FURNITURE and the entrance corridor. A window behind a
// bookshelf is a hole in a wall nobody can see, and one over the doorway would
// put a sky card where the player walks.
ZENITH_TEST(ZM_Interaction, InteriorWindowsClearTheFurnitureAndTheCorridor)
{
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const u_int uWindows = ZM_GetInteriorWindowCount(eRoom);
		const u_int uProps   = ZM_GetInteriorPropCount(eRoom);

		for (u_int w = 0u; w < uWindows; ++w)
		{
			const ZM_InteriorWindow& xWindow = ZM_GetInteriorWindow(eRoom, w);

			// (a) Clear of the corridor: a window's opening must not straddle the
			//     band the player walks in and out through. Both walls are well
			//     outside it in X, so this is about the OPENING's Z span not
			//     reaching the door end of the room.
			ZENITH_ASSERT_LT(xWindow.Z1(),
				ZM_GetInteriorRoomSpec(eRoom).InnerHalfDepth() - 0.30f,
				"%s window %u reaches z=%.2f, into the doorway end of the room",
				ZM_InteriorRoomName(eRoom), w, (double)xWindow.Z1());

			// (b) Clear of the furniture standing against that same wall. A prop
			//     within half its footprint of the opening's Z span, on the window's
			//     own side of the room, is standing in front of it.
			for (u_int p = 0u; p < uProps; ++p)
			{
				const ZM_InteriorProp& xProp = ZM_GetInteriorProp(eRoom, p);
				const bool bSameWall = (xProp.m_fX < 0.0f)
					== (xWindow.m_eWall == ZM_INTERIOR_WALL_NEG_X);
				if (!bSameWall) { continue; }
				// Only props actually against the wall can block it.
				const float fWallX = ZM_GetInteriorRoomSpec(eRoom).InnerHalfWidth();
				if (fabsf(xProp.m_fX) < fWallX - 2.20f) { continue; }

				const bool bOverlapsZ = xProp.m_fZ > xWindow.Z0() - fZM_INTERIOR_PROP_RADIUS
					&& xProp.m_fZ < xWindow.Z1() + fZM_INTERIOR_PROP_RADIUS;
				// ★ THE BED IS THE ONE DELIBERATE EXCEPTION AND IS NOT ONE: a bed is
				//   0.7 m tall and every sill in this game is at 1.00 m or above, so
				//   a bed under a window is a bed under a window. The clause is
				//   therefore about TALL props only.
				const ZM_PropData& xData = ZM_GetPropData(xProp.m_eProp);
				const bool bTall = xData.m_fHeight > 1.00f;
				if (bOverlapsZ && bTall)
				{
					ZENITH_ASSERT_TRUE(false,
						"'%s' (%.2f m tall) stands at z=%.2f against the same wall as "
						"%s window %u (z %.2f..%.2f) -- it blocks the opening, so the "
						"daylight and the sky card are behind a piece of furniture",
						xProp.m_szEntityName, (double)xData.m_fHeight, (double)xProp.m_fZ,
						ZM_InteriorRoomName(eRoom), w, (double)xWindow.Z0(), (double)xWindow.Z1());
				}
			}
		}

		// The RUG and the CURTAINS keep out of the corridor too -- the rug because
		// the player walks over the band and a rug there would be walked through
		// rather than on, and it is the one soft furnishing with a footprint.
		const ZM_InteriorRug xRug = ZM_GetInteriorRug(eRoom);
		const float fRugNear = fabsf(xRug.m_fCentreX) - 0.5f * xRug.m_fWidth;
		ZENITH_ASSERT_GE(fRugNear, fZM_INTERIOR_CORRIDOR_HALF_WIDTH - 0.5f,
			"%s's rug reaches to |x|=%.2f, inside the +/-%.2f entrance corridor",
			ZM_InteriorRoomName(eRoom), (double)fRugNear,
			(double)fZM_INTERIOR_CORRIDOR_HALF_WIDTH);
		ZENITH_ASSERT_GT(xRug.m_fWidth, 0.0f, "%s's rug has no width", ZM_InteriorRoomName(eRoom));
		ZENITH_ASSERT_GT(xRug.m_fThickness, 0.0f, "%s's rug has no thickness",
			ZM_InteriorRoomName(eRoom));
	}
}

// ############################################################################
// 2. The baked occlusion and wear
// ############################################################################

// The corner darkening is REAL and lands where a corner is: a vertex in an
// inside corner is measurably darker than one in the middle of the same wall,
// every colour stays in [0,1] with alpha 1 (the mesh shader multiplies vertex
// colour into albedo only while alpha > 0, so alpha 0 would silently disable the
// whole pass), and the two surfaces that must NOT be shaded are untouched.
ZENITH_TEST(ZM_Gen, InteriorBakedOcclusionDarkensTheCorners)
{
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const ZM_InteriorRoomSpec xSpec = ZM_GetInteriorRoomSpec(eRoom);

		ZM_Interior xInterior;
		ZM_BuildInterior(eRoom, xInterior);

		// (a) THE FUNCTION ITSELF, at three named points. Asserted before the mesh
		//     so a failure says which half is wrong.
		const float fCorner = ZM_InteriorVertexOcclusion(xSpec, Zenith_Maths::Vector3(
			-xSpec.InnerHalfWidth() + 0.05f, 0.05f, -xSpec.InnerHalfDepth() + 0.05f));
		const float fWallMid = ZM_InteriorVertexOcclusion(xSpec, Zenith_Maths::Vector3(
			-xSpec.InnerHalfWidth() + 0.05f, 0.5f * xSpec.m_fWallHeight, 0.0f));
		const float fOpen = ZM_InteriorVertexOcclusion(xSpec, Zenith_Maths::Vector3(
			0.0f, 0.5f * xSpec.m_fWallHeight, 0.0f));

		ZENITH_ASSERT_LT(fCorner, fWallMid,
			"%s: the floor/wall/wall corner (%.4f) is not darker than the middle of "
			"the same wall (%.4f) -- the corner term is not being applied",
			ZM_InteriorRoomName(eRoom), (double)fCorner, (double)fWallMid);
		ZENITH_ASSERT_LT(fWallMid, fOpen,
			"%s: a point on the wall (%.4f) is not darker than the middle of the "
			"room (%.4f)", ZM_InteriorRoomName(eRoom), (double)fWallMid, (double)fOpen);
		ZENITH_ASSERT_EQ_FLOAT(fOpen, 1.0f, 1.0e-3f,
			"%s: the middle of the room is shaded (%.4f) -- the occlusion has no "
			"open-space value of 1 and the whole room is darkened uniformly, which "
			"is an exposure change rather than contact shading",
			ZM_InteriorRoomName(eRoom), (double)fOpen);

		// (b) IT REACHED THE MESH. The wall's own vertices near a corner are
		//     darker on average than its vertices in the middle.
		const ZM_GenMesh& xWall = xInterior.m_axMesh[ZM_INTERIOR_SURFACE_WALL];
		u_int uNearCorner = 0u, uNearMiddle = 0u;
		const float fMeshCorner = IGMeanBrightnessNear(xWall, Zenith_Maths::Vector3(
			-xSpec.InnerHalfWidth(), 0.0f, -xSpec.InnerHalfDepth()), 0.60f, uNearCorner);
		const float fMeshMiddle = IGMeanBrightnessNear(xWall, Zenith_Maths::Vector3(
			-xSpec.InnerHalfWidth(), 0.5f * xSpec.m_fWallHeight, 0.0f), 0.60f, uNearMiddle);
		ZENITH_ASSERT_GT(uNearCorner, 0u,
			"%s: no wall vertex within 0.6 m of the back-left floor corner -- the "
			"wall is not being cut into cells, so there is nowhere for the shading "
			"to interpolate and this clause is measuring nothing",
			ZM_InteriorRoomName(eRoom));
		ZENITH_ASSERT_GT(uNearMiddle, 0u,
			"%s: no wall vertex near the middle of the -X wall", ZM_InteriorRoomName(eRoom));
		ZENITH_ASSERT_LT(fMeshCorner, fMeshMiddle,
			"%s: the wall's vertex colours near the corner (%.4f over %u verts) are "
			"not darker than near the middle (%.4f over %u verts) -- the shading "
			"pass did not reach the mesh",
			ZM_InteriorRoomName(eRoom), (double)fMeshCorner, uNearCorner,
			(double)fMeshMiddle, uNearMiddle);

		// (c) EVERY colour is a legal multiplier, on every shaded surface.
		for (u_int s = 0u; s < (u_int)ZM_INTERIOR_SURFACE_COUNT; ++s)
		{
			const ZM_GenMesh& xMesh = xInterior.m_axMesh[s];
			ZENITH_ASSERT_EQ(xMesh.m_xColors.GetSize(), xMesh.GetNumVerts(),
				"%s/%s has %u colours for %u vertices -- the buffers disagree and the "
				"bake would write whichever is shorter",
				ZM_InteriorRoomName(eRoom), ZM_InteriorSurfaceName((ZM_INTERIOR_SURFACE)s),
				xMesh.m_xColors.GetSize(), xMesh.GetNumVerts());
			for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
			{
				const Zenith_Maths::Vector4& xC = xMesh.m_xColors.Get(v);
				const bool bOk = std::isfinite(xC.x) && std::isfinite(xC.y) && std::isfinite(xC.z)
					&& xC.x >= 0.0f && xC.x <= 1.0f && xC.y >= 0.0f && xC.y <= 1.0f
					&& xC.z >= 0.0f && xC.z <= 1.0f;
				ZENITH_ASSERT_TRUE(bOk,
					"%s/%s vertex %u carries colour (%.3f, %.3f, %.3f), outside [0,1] "
					"-- it would clip or wrap in the unorm8 the vertex format stores",
					ZM_InteriorRoomName(eRoom), ZM_InteriorSurfaceName((ZM_INTERIOR_SURFACE)s),
					v, (double)xC.x, (double)xC.y, (double)xC.z);
				ZENITH_ASSERT_EQ_FLOAT(xC.w, 1.0f, fIG_EPSILON,
					"%s/%s vertex %u has colour alpha %.3f -- the mesh shader "
					"multiplies vertex colour in only while alpha > 0, so this vertex "
					"silently drops its baked shading",
					ZM_InteriorRoomName(eRoom), ZM_InteriorSurfaceName((ZM_INTERIOR_SURFACE)s),
					v, (double)xC.w);
			}
		}

		// (d) THE TWO SURFACES THAT MUST STAY UNSHADED. A darkened corner on the
		//     glass or the sky card is a smudge on the daylight.
		const ZM_INTERIOR_SURFACE aeUnshaded[] = { ZM_INTERIOR_SURFACE_GLASS, ZM_INTERIOR_SURFACE_SKY };
		for (u_int u = 0u; u < 2u; ++u)
		{
			const ZM_GenMesh& xMesh = xInterior.m_axMesh[aeUnshaded[u]];
			for (u_int v = 0u; v < xMesh.GetNumVerts(); ++v)
			{
				const Zenith_Maths::Vector4& xC = xMesh.m_xColors.Get(v);
				ZENITH_ASSERT_EQ_FLOAT(xC.x, 1.0f, 1.0e-3f,
					"%s/%s vertex %u is shaded (%.4f) -- glass and the sky card must "
					"stay unmodulated or the window carries a dirty corner",
					ZM_InteriorRoomName(eRoom), ZM_InteriorSurfaceName(aeUnshaded[u]),
					v, (double)xC.x);
			}
		}
	}
}

// The traffic band runs from the door to the room centre and fades past it, and
// it reaches the floor mesh.
ZENITH_TEST(ZM_Gen, InteriorTrafficWearRunsFromTheDoor)
{
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const ZM_InteriorRoomSpec xSpec = ZM_GetInteriorRoomSpec(eRoom);
		const float fZ = xSpec.InnerHalfDepth();

		const float fOnAxis  = ZM_InteriorTrafficWear(xSpec, Zenith_Maths::Vector3(0.0f, 0.0f, fZ * 0.5f));
		const float fOffAxis = ZM_InteriorTrafficWear(xSpec, Zenith_Maths::Vector3(
			fZM_INTERIOR_WEAR_HALF_WIDTH + 0.5f, 0.0f, fZ * 0.5f));
		const float fBehind  = ZM_InteriorTrafficWear(xSpec, Zenith_Maths::Vector3(0.0f, 0.0f, -fZ * 0.9f));

		ZENITH_ASSERT_LT(fOnAxis, fOffAxis,
			"%s: the door axis (%.4f) is not worn more than the floor beside it "
			"(%.4f) -- the traffic band has no lateral falloff",
			ZM_InteriorRoomName(eRoom), (double)fOnAxis, (double)fOffAxis);
		ZENITH_ASSERT_EQ_FLOAT(fOffAxis, 1.0f, 1.0e-3f,
			"%s: the floor outside the band is worn (%.4f) -- the band covers the "
			"whole room, which is a uniform darkening rather than a path",
			ZM_InteriorRoomName(eRoom), (double)fOffAxis);
		ZENITH_ASSERT_EQ_FLOAT(fBehind, 1.0f, 1.0e-3f,
			"%s: the far end of the room behind the centre is worn (%.4f) -- the "
			"band does not fade, so it reads as a stripe painted across the floor",
			ZM_InteriorRoomName(eRoom), (double)fBehind);

		// It reached the floor mesh: vertices on the door axis are darker than
		// vertices the same distance out to the side.
		ZM_Interior xInterior;
		ZM_BuildInterior(eRoom, xInterior);
		const ZM_GenMesh& xFloor = xInterior.m_axMesh[ZM_INTERIOR_SURFACE_FLOOR];
		u_int uA = 0u, uB = 0u;
		// THE WALKING SURFACE ONLY -- not the board sides buried in the gaps, and
		// not the dark underlay beneath them, whose top face is up-facing too.
		//
		// ★ 0.90 m RATHER THAN 0.50, because the strict filter leaves FAR fewer
		// vertices to average. A board top is a quad, so its vertices sit at the
		// board SEGMENT ENDS -- roughly every 1.2 to 2.4 m along a row -- not
		// continuously under the probe. At 0.5 m a ball can legitimately catch only
		// a handful, and on an unlucky stagger none at all. 0.90 m still sits
		// entirely inside the +/-1.20 m wear band on the axis (so every vertex it
		// averages IS worn) and entirely outside it at the side sample, which is
		// what the comparison needs.
		constexpr float fFLOOR_PROBE_RADIUS = 0.90f;
		const float fMeshAxis = IGMeanBrightnessOnWalkingSurface(xFloor,
			Zenith_Maths::Vector3(0.0f, 0.0f, fZ * 0.5f), fFLOOR_PROBE_RADIUS, uA);
		const float fMeshSide = IGMeanBrightnessOnWalkingSurface(xFloor,
			Zenith_Maths::Vector3(fZM_INTERIOR_WEAR_HALF_WIDTH + 1.0f, 0.0f, fZ * 0.5f),
			fFLOOR_PROBE_RADIUS, uB);
		ZENITH_ASSERT_GT(uA, 0u, "%s: no floor vertex on the door axis to measure",
			ZM_InteriorRoomName(eRoom));
		ZENITH_ASSERT_GT(uB, 0u, "%s: no floor vertex beside the door axis to measure",
			ZM_InteriorRoomName(eRoom));
		ZENITH_ASSERT_LT(fMeshAxis, fMeshSide,
			"%s: the floor's vertex colours on the walked axis (%.4f) are not darker "
			"than beside it (%.4f) -- the wear did not reach the mesh",
			ZM_InteriorRoomName(eRoom), (double)fMeshAxis, (double)fMeshSide);
	}
}

// ############################################################################
// 3. The floorboards are geometry
// ############################################################################

// PlayerHome's floor is individual boards with real gaps and millimetre lips;
// ProfLab's is one poured slab. The walking plane is still exactly y = 0 in both
// -- every spawn marker and every authored body in these scenes stands on it.
ZENITH_TEST(ZM_Gen, InteriorFloorboardsAreIndividualBoards)
{
	ZM_Interior xHome, xLab;
	ZM_BuildInterior(ZM_INTERIOR_ROOM_PLAYER_HOME, xHome);
	ZM_BuildInterior(ZM_INTERIOR_ROOM_PROF_LAB,    xLab);

	const ZM_GenMesh& xHomeFloor = xHome.m_axMesh[ZM_INTERIOR_SURFACE_FLOOR];
	const ZM_GenMesh& xLabFloor  = xLab.m_axMesh[ZM_INTERIOR_SURFACE_FLOOR];

	// ★★ THE ORACLE IS THE SECOND LAYER, NOT A TRIANGLE RATIO. This clause first
	// asserted "the cottage floor has more than 4x the lab floor's triangles",
	// which measured the SHADING CELL SIZE and not the boards: both floors are cut
	// into fZM_INTERIOR_SHADING_CELL grids so the baked vertex colours have
	// somewhere to interpolate, so the lab's "one poured slab" is 24,024 triangles
	// and the ratio says nothing. What actually distinguishes boards from a
	// subdivided slab is that boards are a layer STANDING ON another layer: the
	// cottage floor has up-facing geometry at the walking plane AND up-facing
	// geometry several millimetres below it (the underlay the gaps show), while
	// the lab's floor has exactly one up-facing height. Nothing here re-spells a
	// generator constant -- the heights are discovered from the mesh.
	u_int uHomeUpHeights = 0u, uLabUpHeights = 0u;
	float afHomeUp[16] = {}, afLabUp[16] = {};
	IGCollectUpFacingHeights(xHomeFloor, afHomeUp, 16u, uHomeUpHeights);
	IGCollectUpFacingHeights(xLabFloor,  afLabUp,  16u, uLabUpHeights);

	ZENITH_ASSERT_EQ(uLabUpHeights, 1u,
		"ProfLab's floor has %u distinct up-facing heights; it is one poured slab "
		"and must have exactly one, or this clause cannot tell a second LAYER from "
		"a subdivided one", uLabUpHeights);
	ZENITH_ASSERT_GE(uHomeUpHeights, 2u,
		"PlayerHome's floor has only %u distinct up-facing height(s) -- there is no "
		"second layer under the boards, so the gaps between them show nothing and "
		"the floor is a slab with a board texture on it, which is exactly what the "
		"board TEXTURE already gave us", uHomeUpHeights);

	// ...and the two layers are separated by a real gap, not by a lip.
	float fHighest = afHomeUp[0], fLowest = afHomeUp[0];
	for (u_int t = 0u; t < uHomeUpHeights; ++t)
	{
		if (afHomeUp[t] > fHighest) { fHighest = afHomeUp[t]; }
		if (afHomeUp[t] < fLowest)  { fLowest  = afHomeUp[t]; }
	}
	ZENITH_ASSERT_GE(fHighest - fLowest, 0.005f,
		"PlayerHome's floor layers are only %.4f m apart (%.4f down to %.4f) -- at "
		"that separation the 'underlay' is a lip on the same board run rather than "
		"a surface beneath it", (double)(fHighest - fLowest), (double)fHighest,
		(double)fLowest);
	ZENITH_ASSERT_EQ_FLOAT(fHighest, 0.0f, fIG_EPSILON,
		"PlayerHome's highest up-facing surface is at y=%.5f rather than the y=0 "
		"walking plane", (double)fHighest);

	// The walking plane: the HIGHEST top face is exactly y = 0 in both rooms.
	ZENITH_ASSERT_EQ_FLOAT(ZM_GenMeshBoundsMax(xHomeFloor).y, 0.0f, fIG_EPSILON,
		"PlayerHome's floor tops out at y=%.5f, not the y=0 plane every spawn in "
		"this scene stands on", (double)ZM_GenMeshBoundsMax(xHomeFloor).y);
	ZENITH_ASSERT_EQ_FLOAT(ZM_GenMeshBoundsMax(xLabFloor).y, 0.0f, fIG_EPSILON,
		"ProfLab's floor tops out at y=%.5f, not y=0",
		(double)ZM_GenMeshBoundsMax(xLabFloor).y);

	// THE LIPS ARE REAL: the boards' top faces sit at more than one height, and
	// every one of them is within 3 mm of the walking plane (a bigger step is a
	// trip hazard the player would see through the camera).
	u_int uDistinctTops = 0u;
	float afTops[8] = {};
	for (u_int v = 0u; v < xHomeFloor.GetNumVerts(); ++v)
	{
		const float fY = xHomeFloor.m_xPositions.Get(v).y;
		if (fY < -0.004f) { continue; }   // the underlay and the board sides
		bool bSeen = false;
		for (u_int t = 0u; t < uDistinctTops; ++t)
		{
			if (fabsf(afTops[t] - fY) < 1.0e-4f) { bSeen = true; break; }
		}
		if (!bSeen && uDistinctTops < 8u) { afTops[uDistinctTops++] = fY; }
	}
	ZENITH_ASSERT_GT(uDistinctTops, 1u,
		"every board top in PlayerHome sits at the same height, so there are no "
		"lips between boards -- the gaps are drawn but the floor is still flat, "
		"which is exactly what a board TEXTURE already gave us");
	for (u_int t = 0u; t < uDistinctTops; ++t)
	{
		ZENITH_ASSERT_LE(fabsf(afTops[t]), 0.003f + fIG_EPSILON,
			"a board top sits %.4f m off the walking plane -- past a few "
			"millimetres that is a step, not a lip", (double)fabsf(afTops[t]));
	}
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[ZM_Gen] OBSERVED PlayerHome floor: %u tris, %u distinct board-top heights; "
		"ProfLab floor: %u tris", xHomeFloor.GetNumTris(), uDistinctTops,
		xLabFloor.GetNumTris());
}

// ############################################################################
// 4. The light fixtures
// ############################################################################

// EVERY light has a fixture, and every fixture's GLOWING part contains the lamp
// it houses. A shade that does not bracket its bulb is a glowing object beside a
// glow, and it renders perfectly happily.
ZENITH_TEST(ZM_Gen, InteriorFixturesBracketTheirOwnLights)
{
	u_int uFixturesChecked = 0u;
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const u_int uLights   = ZM_GetInteriorLightCount(eRoom);
		const u_int uFixtures = ZM_GetInteriorFixtureCount(eRoom);

		ZENITH_ASSERT_EQ(uFixtures, uLights,
			"%s has %u lights and %u fixtures -- every light in these rooms was a "
			"bare point in space before this, and a light with no fixture is exactly "
			"that failure back again", ZM_InteriorRoomName(eRoom), uLights, uFixtures);

		for (u_int f = 0u; f < uFixtures; ++f)
		{
			const ZM_InteriorFixture& xFixture = ZM_GetInteriorFixture(eRoom, f);
			++uFixturesChecked;

			// The named light exists in THIS room.
			const ZM_InteriorLight* pxLight = nullptr;
			for (u_int l = 0u; l < uLights; ++l)
			{
				const ZM_InteriorLight& xLight = ZM_GetInteriorLight(eRoom, l);
				if (strcmp(xLight.m_szEntityName, xFixture.m_szLightEntityName) == 0)
				{
					pxLight = &xLight;
					break;
				}
			}
			ZENITH_ASSERT_NOT_NULL(pxLight,
				"fixture '%s' houses '%s', which is not a light in %s -- the fixture "
				"would stand glowing over nothing", xFixture.m_szEntityName,
				xFixture.m_szLightEntityName, ZM_InteriorRoomName(eRoom));
			if (pxLight == nullptr) { continue; }

			// Same column, to the millimetre.
			ZENITH_ASSERT_EQ_FLOAT(xFixture.m_fX, pxLight->m_fX, 1.0e-3f,
				"'%s' stands at x=%.3f but its lamp '%s' is at x=%.3f",
				xFixture.m_szEntityName, (double)xFixture.m_fX,
				pxLight->m_szEntityName, (double)pxLight->m_fX);
			ZENITH_ASSERT_EQ_FLOAT(xFixture.m_fZ, pxLight->m_fZ, 1.0e-3f,
				"'%s' stands at z=%.3f but its lamp '%s' is at z=%.3f",
				xFixture.m_szEntityName, (double)xFixture.m_fZ,
				pxLight->m_szEntityName, (double)pxLight->m_fZ);

			// ★ THE CLAUSE THIS TEST EXISTS FOR: the lamp is inside the glowing part.
			float fLow = 0.0f, fHigh = 0.0f;
			const bool bHasGlow = ZM_GetInteriorFixtureGlowBand(xFixture.m_eProp, fLow, fHigh);
			ZENITH_ASSERT_TRUE(bHasGlow,
				"'%s' wears %s, which declares no glowing band -- either it is not a "
				"light fixture or ZM_GetInteriorFixtureGlowBand has not been told "
				"about it", xFixture.m_szEntityName, ZM_GetPropName(xFixture.m_eProp));
			const float fGlowLowWorld  = xFixture.m_fY + fLow;
			const float fGlowHighWorld = xFixture.m_fY + fHigh;
			ZENITH_ASSERT_GE(pxLight->m_fY, fGlowLowWorld - 1.0e-3f,
				"'%s' sits at y=%.3f, BELOW its fixture's glowing part (%.3f..%.3f) "
				"-- the shade would glow above the light it is supposed to house",
				pxLight->m_szEntityName, (double)pxLight->m_fY,
				(double)fGlowLowWorld, (double)fGlowHighWorld);
			ZENITH_ASSERT_LE(pxLight->m_fY, fGlowHighWorld + 1.0e-3f,
				"'%s' sits at y=%.3f, ABOVE its fixture's glowing part (%.3f..%.3f)",
				pxLight->m_szEntityName, (double)pxLight->m_fY,
				(double)fGlowLowWorld, (double)fGlowHighWorld);

			// The fixture's emissive colour agrees with the light it houses. They
			// are two independent tables, and a shade glowing a different colour
			// from its own lamp is the most visible way for them to drift.
			const ZM_PropEmissive xEmissive = ZM_GetPropEmissive(xFixture.m_eProp);
			ZENITH_ASSERT_GT(xEmissive.m_fIntensity, 0.0f,
				"'%s' wears %s but that prop emits nothing -- the fixture is a dark "
				"object hanging under a floating glow", xFixture.m_szEntityName,
				ZM_GetPropName(xFixture.m_eProp));
			const bool bLightWarm    = pxLight->m_fR > pxLight->m_fB;
			const bool bEmissiveWarm = xEmissive.m_xColour.x > xEmissive.m_xColour.z;
			ZENITH_ASSERT_EQ(bLightWarm ? 1u : 0u, bEmissiveWarm ? 1u : 0u,
				"'%s' throws a %s light (%.2f, %.2f, %.2f) but its shade glows %s "
				"(%.2f, %.2f, %.2f) -- the fixture and its own lamp disagree about "
				"what colour it is", xFixture.m_szEntityName,
				bLightWarm ? "warm" : "cool", (double)pxLight->m_fR, (double)pxLight->m_fG,
				(double)pxLight->m_fB, bEmissiveWarm ? "warm" : "cool",
				(double)xEmissive.m_xColour.x, (double)xEmissive.m_xColour.y,
				(double)xEmissive.m_xColour.z);

			// The name resolves back to the prop, which is how the runtime component
			// picks the model. A typo is a fixture that shows nothing, in a scene
			// that still loads.
			ZENITH_ASSERT_EQ((u_int)ZM_PropForInteriorFixtureEntity(xFixture.m_szEntityName),
				(u_int)xFixture.m_eProp,
				"'%s' does not map back to its own prop id", xFixture.m_szEntityName);

			// A frozen UNIT quaternion, like every other authored rotation
			// (ZM-D-183).
			const float fLenSq = xFixture.m_fQuatW * xFixture.m_fQuatW
				+ xFixture.m_fQuatY * xFixture.m_fQuatY;
			ZENITH_ASSERT_EQ_FLOAT(fLenSq, 1.0f, 1.0e-6f,
				"'%s' carries a non-unit frozen quaternion (|q|^2=%.8f)",
				xFixture.m_szEntityName, (double)fLenSq);
		}
	}
	ZENITH_ASSERT_GE(uFixturesChecked, 7u,
		"only %u fixtures were checked across both rooms -- the tables are empty "
		"or unreachable and this clause is asserting nothing", uFixturesChecked);
}

// A fixture is VISUAL-ONLY and therefore may hang inside the entrance corridor,
// which is the one place the furniture rule does not apply -- but only if it is
// genuinely out of the way: above a standing body AND above the indoor camera's
// own ceiling cap.
ZENITH_TEST(ZM_Interaction, InteriorFixturesInTheCorridorHangClearOfBodyAndCamera)
{
	// The tallest thing that walks through these rooms, READ from the body
	// contract rather than re-spelled (ZM_HumanBody.h is the one authority).
	constexpr float fBODY_HEIGHT = fZM_HUMAN_BODY_HEIGHT;

	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const ZM_InteriorRoomSpec xSpec = ZM_GetInteriorRoomSpec(eRoom);
		const u_int uFixtures = ZM_GetInteriorFixtureCount(eRoom);
		for (u_int f = 0u; f < uFixtures; ++f)
		{
			const ZM_InteriorFixture& xFixture = ZM_GetInteriorFixture(eRoom, f);
			if (fabsf(xFixture.m_fX) >= fZM_INTERIOR_CORRIDOR_HALF_WIDTH)
			{
				continue;   // outside the corridor: the furniture rule already covers it
			}

			// Its LOWEST geometry is its base (these are grounded models).
			ZENITH_ASSERT_GT(xFixture.m_fY, fBODY_HEIGHT,
				"'%s' hangs at y=%.3f inside the +/-%.2f entrance corridor of %s, "
				"below a %.1f m body -- the player would walk through it, and the "
				"walk driver has no obstacle avoidance to route around it",
				xFixture.m_szEntityName, (double)xFixture.m_fY,
				(double)fZM_INTERIOR_CORRIDOR_HALF_WIDTH, ZM_InteriorRoomName(eRoom),
				(double)fBODY_HEIGHT);

			// ...and above the lens. ZM_FollowCamera caps the boom at
			// ceiling - clearance indoors; a pendant below that cap is a shade the
			// camera flies through on every step down the room.
			const float fCameraCap = xSpec.m_fWallHeight - ZM_FollowCamera::GetCeilingClearance();
			ZENITH_ASSERT_GE(xFixture.m_fY, fCameraCap,
				"'%s' hangs at y=%.3f, BELOW the indoor camera cap of %.3f "
				"(ceiling %.2f - clearance %.2f) while standing in the corridor -- "
				"the lens would pass through the shade",
				xFixture.m_szEntityName, (double)xFixture.m_fY, (double)fCameraCap,
				(double)xSpec.m_fWallHeight, (double)ZM_FollowCamera::GetCeilingClearance());

			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_Interaction] OBSERVED '%s' in the corridor at y=%.3f: %.3f m over "
				"a body, %.3f m over the camera cap", xFixture.m_szEntityName,
				(double)xFixture.m_fY, (double)(xFixture.m_fY - fBODY_HEIGHT),
				(double)(xFixture.m_fY - fCameraCap));
		}
	}
}

// The fixture entity names are NEW names -- they collide with nothing already
// authored. The authoring appends them after every pre-existing dressing row
// (ZM-D-148: appending is free, reordering rewrites the committed bytes), and a
// name collision is the one way an append could still overwrite an existing
// entity rather than add one.
ZENITH_TEST(ZM_Gen, InteriorFixtureNamesAreNewAndDistinct)
{
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const u_int uFixtures = ZM_GetInteriorFixtureCount(eRoom);
		for (u_int f = 0u; f < uFixtures; ++f)
		{
			const char* szName = ZM_GetInteriorFixture(eRoom, f).m_szEntityName;
			ZENITH_ASSERT_NOT_NULL(szName, "fixture %u in %s has no name", f,
				ZM_InteriorRoomName(eRoom));

			// Not a furniture entity, in EITHER room...
			ZENITH_ASSERT_EQ((u_int)ZM_PropForInteriorPropEntity(szName),
				(u_int)ZM_PROP_NONE,
				"fixture '%s' collides with a furniture entity name -- the authoring "
				"appends both, so one would overwrite the other", szName);
			// ...not a light, not a shell, not a blockout.
			for (u_int r2 = 0u; r2 < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r2)
			{
				const u_int uLights = ZM_GetInteriorLightCount((ZM_INTERIOR_ROOM)r2);
				for (u_int l = 0u; l < uLights; ++l)
				{
					ZENITH_ASSERT_NE(std::strcmp(szName,
						ZM_GetInteriorLight((ZM_INTERIOR_ROOM)r2, l).m_szEntityName), 0,
						"fixture '%s' collides with a LIGHT entity of the same name -- "
						"the fixture would land on the light's entity and inherit its "
						"transform", szName);
				}
			}
			ZENITH_ASSERT_FALSE(ZM_IsInteriorShellEntity(szName),
				"fixture '%s' collides with a room shell entity", szName);
			ZENITH_ASSERT_FALSE(ZM_IsPlayerHomeBlockName(szName),
				"fixture '%s' collides with a PlayerHome blockout", szName);

			// Distinct within the table.
			for (u_int f2 = f + 1u; f2 < uFixtures; ++f2)
			{
				ZENITH_ASSERT_NE(std::strcmp(szName,
					ZM_GetInteriorFixture(eRoom, f2).m_szEntityName), 0,
					"two fixtures in %s share the entity name '%s'",
					ZM_InteriorRoomName(eRoom), szName);
			}
		}
	}
}

// ############################################################################
// 5. The restored photometric intensities
// ############################################################################

// ★ THE VALUES ARE PHOTOMETRIC AGAIN, AND THE BANDS BELOW ARE PHYSICS RATHER
// THAN A RESTATEMENT OF THE TABLE. Each is the range a real lamp of that
// description occupies, so a return to the ~14% bloom-era figures (130 lm
// pendant, 160 lm tubes) reds here -- which is the whole point: that cut was
// booked as temporary and nothing but a test can hold it to that.
ZENITH_TEST(ZM_Gen, InteriorLightsCarryPhotometricIntensities)
{
	// A domestic ceiling pendant is a 60 W-equivalent lamp: 700-1000 lm.
	// A bedside or standard lamp is a 25-40 W-equivalent: 200-500 lm.
	// A 4 ft fluorescent batten is 1300-2600 lm.
	constexpr float fPENDANT_MIN = 700.0f,  fPENDANT_MAX = 1100.0f;
	constexpr float fLAMP_MIN    = 180.0f,  fLAMP_MAX    = 500.0f;
	constexpr float fBATTEN_MIN  = 1300.0f, fBATTEN_MAX  = 2600.0f;

	float fHomeTotal = 0.0f, fLabTotal = 0.0f;

	const u_int uHome = ZM_GetInteriorLightCount(ZM_INTERIOR_ROOM_PLAYER_HOME);
	for (u_int l = 0u; l < uHome; ++l)
	{
		const ZM_InteriorLight& xLight = ZM_GetInteriorLight(ZM_INTERIOR_ROOM_PLAYER_HOME, l);
		fHomeTotal += xLight.m_fLumens;
		const bool bCeiling = strcmp(xLight.m_szEntityName, "HomeLampCeiling") == 0;
		const float fMin = bCeiling ? fPENDANT_MIN : fLAMP_MIN;
		const float fMax = bCeiling ? fPENDANT_MAX : fLAMP_MAX;
		ZENITH_ASSERT_GE(xLight.m_fLumens, fMin,
			"'%s' emits %.0f lm, below the %.0f lm a real %s puts out. The 2026-09-01 "
			"cut to ~14%% was booked as temporary against the bloom threshold being "
			"re-derived; do not re-apply it here",
			xLight.m_szEntityName, (double)xLight.m_fLumens, (double)fMin,
			bCeiling ? "ceiling pendant" : "bedside/standard lamp");
		ZENITH_ASSERT_LE(xLight.m_fLumens, fMax,
			"'%s' emits %.0f lm, above the %.0f lm ceiling for a %s",
			xLight.m_szEntityName, (double)xLight.m_fLumens, (double)fMax,
			bCeiling ? "ceiling pendant" : "bedside/standard lamp");
	}

	const u_int uLab = ZM_GetInteriorLightCount(ZM_INTERIOR_ROOM_PROF_LAB);
	for (u_int l = 0u; l < uLab; ++l)
	{
		const ZM_InteriorLight& xLight = ZM_GetInteriorLight(ZM_INTERIOR_ROOM_PROF_LAB, l);
		fLabTotal += xLight.m_fLumens;
		ZENITH_ASSERT_GE(xLight.m_fLumens, fBATTEN_MIN,
			"'%s' emits %.0f lm, below the %.0f lm of a 4 ft fluorescent tube",
			xLight.m_szEntityName, (double)xLight.m_fLumens, (double)fBATTEN_MIN);
		ZENITH_ASSERT_LE(xLight.m_fLumens, fBATTEN_MAX,
			"'%s' emits %.0f lm, above the %.0f lm ceiling for one tube",
			xLight.m_szEntityName, (double)xLight.m_fLumens, (double)fBATTEN_MAX);
	}

	// ★ THE DESIGNED HOUSE-TO-LAB RELATIONSHIP (ZM-D-176: "a lab is lit evenly
	// and brightly") SURVIVES THE RESTORE. Measured per unit floor area, because
	// that is what "brightly lit" means in a room twice the size.
	const float fHomeArea = 4.0f * fZM_PLAYERHOME_HALF_WIDTH * fZM_PLAYERHOME_HALF_DEPTH;
	const float fLabArea  = 4.0f * fZM_PROFLAB_HALF_WIDTH * fZM_PROFLAB_HALF_DEPTH;
	const float fHomeDensity = fHomeTotal / fHomeArea;
	const float fLabDensity  = fLabTotal / fLabArea;
	const float fRatio = fLabDensity / fHomeDensity;
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[ZM_Gen] OBSERVED lighting density: PlayerHome %.1f lm over %.0f m2 = %.2f "
		"lm/m2; ProfLab %.1f lm over %.0f m2 = %.2f lm/m2; ratio %.2fx",
		(double)fHomeTotal, (double)fHomeArea, (double)fHomeDensity,
		(double)fLabTotal, (double)fLabArea, (double)fLabDensity, (double)fRatio);
	// ★★ THE BAND IS THE ORDERING AND ITS ROUGH SIZE, NOT A PINNED 1.7x. The
	// first cut of this clause asserted (1.2, 2.6) "because the two rooms have
	// carried ~1.7x through every re-tune". They had -- but 1.707x was never a
	// DESIGNED figure: it is the arithmetic of the UNIFORM cut applied on
	// 2026-09-01, and a uniform cut preserves whatever ratio it starts from
	// whether or not that ratio means anything. The honest per-lamp values are
	// not a uniform multiple of what that cut left (6.9x on the pendant against
	// 10.0x on the battens), so restoring them moves the ratio to 2.74x by
	// arithmetic alone.
	//
	// ★ AND 2.74x IS THE MORE FAITHFUL NUMBER, which is why the VALUES stayed and
	// this band widened instead. ZM-D-176's ruling is qualitative -- "a lab is lit
	// evenly and brightly" -- and real practice puts a bedroom at 50-100 lux
	// against a laboratory's 300-500, a ratio of 3x to 10x. The ceiling below is
	// the point past which the BEDROOM stops reading as a lit room beside the lab;
	// it is not a restatement of the measurement.
	constexpr float fMIN_LAB_ADVANTAGE = 1.2f;
	constexpr float fMAX_LAB_ADVANTAGE = 3.5f;
	ZENITH_ASSERT_GT(fRatio, fMIN_LAB_ADVANTAGE,
		"the lab is only %.2fx as brightly lit per square metre as the bedroom. "
		"ZM-D-176 made 'a lab is lit evenly and brightly' a design decision, and "
		"below this the two rooms are lit alike", (double)fRatio);
	ZENITH_ASSERT_LT(fRatio, fMAX_LAB_ADVANTAGE,
		"the lab is %.2fx the bedroom's lighting density -- past this the bedroom "
		"reads as unlit by comparison rather than as cosier. Real practice tops out "
		"around 10x, but these two rooms share one auto-exposure and the player "
		"walks between them in a few seconds", (double)fRatio);
}

// ############################################################################
// 6. The interior sun
// ############################################################################

// ★★ THE VECTOR IS ALREADY EXACTLY UNIT LENGTH IN FLOAT, AND THAT IS A
// SERIALISATION PROPERTY RATHER THAN TIDINESS. Zenith_SunComponent::SetDirection
// NORMALISES what it is handed -- a sqrt and three divides -- and the normalised
// result is what lands in the committed scene bytes. MSVC Debug and Release
// codegen disagree on transcendentals by 1-2 ULP, which is precisely how
// ZM-D-183's committed scene started ping-ponging in git. A vector whose length
// is already exactly 1.0f divides by exactly 1.0f, so the authored literals ARE
// the serialised bytes in every configuration.
ZENITH_TEST(ZM_Gen, InteriorSunIsAFrozenUnitVectorThatEntersTheWindows)
{
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		const Zenith_Maths::Vector3 xSun = ZM_GetInteriorSunDirection(eRoom);

		// (1) EXACTLY unit length, computed the way the component computes it.
		const float fSumSq = xSun.x * xSun.x + xSun.y * xSun.y + xSun.z * xSun.z;
		ZENITH_ASSERT_EQ(std::bit_cast<u_int>(fSumSq), std::bit_cast<u_int>(1.0f),
			"%s's sun direction (%.7f, %.7f, %.7f) has |d|^2 = %.9f, which is not "
			"BIT-EXACTLY 1.0f. SetDirection normalises before serialising, so a "
			"length that is merely close puts a libm sqrt between the authored "
			"constant and the committed bytes -- the ZM-D-183 failure exactly",
			ZM_InteriorRoomName(eRoom), (double)xSun.x, (double)xSun.y, (double)xSun.z,
			(double)fSumSq);

		// (2) It comes DOWN and it comes in through the -X wall, which is the wall
		//     both rooms window.
		ZENITH_ASSERT_LT(xSun.y, 0.0f,
			"%s's sun travels upward (y=%.4f) -- it would light the ceiling from "
			"outside and nothing would come through a window",
			ZM_InteriorRoomName(eRoom), (double)xSun.y);
		ZENITH_ASSERT_GT(xSun.x, 0.0f,
			"%s's sun travels toward -X (x=%.4f), so it enters through the +X wall. "
			"Both rooms put their sunlit windows on -X",
			ZM_InteriorRoomName(eRoom), (double)xSun.x);

		// (3) ~30 degrees of elevation: high enough to reach the floor well inside
		//     the room, low enough to throw a long frame shadow across it.
		const float fHorizontal = sqrtf(xSun.x * xSun.x + xSun.z * xSun.z);
		const float fElevationDeg = atanf(-xSun.y / fHorizontal) * (180.0f / 3.14159265f);
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_Gen] OBSERVED %s sun (%.7f, %.7f, %.7f): elevation %.2f degrees",
			ZM_InteriorRoomName(eRoom), (double)xSun.x, (double)xSun.y, (double)xSun.z,
			(double)fElevationDeg);
		ZENITH_ASSERT_GT(fElevationDeg, 20.0f,
			"%s's sun sits %.2f degrees above the horizon -- below this it enters "
			"the window almost horizontally and never lands on the floor",
			ZM_InteriorRoomName(eRoom), (double)fElevationDeg);
		ZENITH_ASSERT_LT(fElevationDeg, 45.0f,
			"%s's sun sits %.2f degrees up -- above this the patch lands under the "
			"sill and the frame casts no readable shadow",
			ZM_InteriorRoomName(eRoom), (double)fElevationDeg);

		// (4) THE LIGHT ACTUALLY REACHES THE FLOOR INSIDE THE ROOM. Traced from the
		//     middle of a -X window along the sun direction to y = 0.
		const u_int uWindows = ZM_GetInteriorWindowCount(eRoom);
		const ZM_InteriorRoomSpec xSpec = ZM_GetInteriorRoomSpec(eRoom);
		bool bAnyNegX = false;
		for (u_int w = 0u; w < uWindows; ++w)
		{
			const ZM_InteriorWindow& xWindow = ZM_GetInteriorWindow(eRoom, w);
			if (xWindow.m_eWall != ZM_INTERIOR_WALL_NEG_X) { continue; }
			bAnyNegX = true;
			const float fMidY = 0.5f * (xWindow.m_fSillY + xWindow.m_fHeadY);
			const float fT = fMidY / -xSun.y;   // y reaches 0 after this much travel
			const float fLandX = -xSpec.InnerHalfWidth() + xSun.x * fT;
			const float fLandZ = xWindow.m_fCentreZ + xSun.z * fT;
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[ZM_Gen] OBSERVED %s window %u: sunlight lands at (%.2f, 0, %.2f)",
				ZM_InteriorRoomName(eRoom), w, (double)fLandX, (double)fLandZ);
			ZENITH_ASSERT_LT(fLandX, xSpec.InnerHalfWidth(),
				"%s window %u's sunlight lands at x=%.2f, past the far wall at %.2f "
				"-- the patch never touches this room's floor",
				ZM_InteriorRoomName(eRoom), w, (double)fLandX, (double)xSpec.InnerHalfWidth());
			ZENITH_ASSERT_GT(fLandX, -xSpec.InnerHalfWidth(),
				"%s window %u's sunlight lands at x=%.2f, behind the wall it came "
				"through", ZM_InteriorRoomName(eRoom), w, (double)fLandX);
		}
		ZENITH_ASSERT_TRUE(bAnyNegX,
			"%s has no window on the -X wall, which is the wall the shared sun "
			"direction is aimed through -- its sunlight enters nothing",
			ZM_InteriorRoomName(eRoom));
	}
}

// ############################################################################
// 7. Determinism, with the new buffers in it
// ############################################################################

// Same room id -> byte-identical bundle, INCLUDING the vertex colours the baked
// shading rides in. ZM_InteriorBuildEqual compares them now; a shading pass that
// drew from an RNG (or iterated an unordered container) would red here.
ZENITH_TEST(ZM_Gen, InteriorRebuildIsByteIdenticalIncludingShading)
{
	for (u_int r = 0u; r < (u_int)ZM_INTERIOR_ROOM_COUNT; ++r)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)r;
		ZM_Interior xA, xB;
		ZM_BuildInterior(eRoom, xA);
		ZM_BuildInterior(eRoom, xB);

		ZENITH_ASSERT_TRUE(ZM_InteriorBuildEqual(xA, xB),
			"%s did not rebuild byte-identically", ZM_InteriorRoomName(eRoom));
		ZENITH_ASSERT_EQ(ZM_InteriorContentHash(xA), ZM_InteriorContentHash(xB),
			"%s's content hash is not reproducible", ZM_InteriorRoomName(eRoom));

		// ...and the colours specifically, since they are the newest buffer and
		// the one a re-run could most easily perturb.
		for (u_int s = 0u; s < (u_int)ZM_INTERIOR_SURFACE_COUNT; ++s)
		{
			const ZM_GenMesh& xMa = xA.m_axMesh[s];
			const ZM_GenMesh& xMb = xB.m_axMesh[s];
			ZENITH_ASSERT_EQ(xMa.m_xColors.GetSize(), xMb.m_xColors.GetSize(),
				"%s/%s rebuilt with a different colour count",
				ZM_InteriorRoomName(eRoom), ZM_InteriorSurfaceName((ZM_INTERIOR_SURFACE)s));
			for (u_int v = 0u; v < xMa.m_xColors.GetSize(); ++v)
			{
				ZENITH_ASSERT_EQ_FLOAT(xMa.m_xColors.Get(v).x, xMb.m_xColors.Get(v).x, 0.0f,
					"%s/%s vertex %u rebuilt with a different colour",
					ZM_InteriorRoomName(eRoom),
					ZM_InteriorSurfaceName((ZM_INTERIOR_SURFACE)s), v);
			}
		}

		// The whole bundle still validates, with the colour clause in it.
		const ZM_InteriorValidation xV = ZM_ValidateInterior(xA);
		for (u_int s = 0u; s < (u_int)ZM_INTERIOR_SURFACE_COUNT; ++s)
		{
			ZENITH_ASSERT_TRUE(xV.m_axSurface[s].m_bColoursUnitRange,
				"%s/%s has a vertex colour outside [0,1] or an alpha that is not 1",
				ZM_InteriorRoomName(eRoom), ZM_InteriorSurfaceName((ZM_INTERIOR_SURFACE)s));
		}
		ZENITH_ASSERT_TRUE(xV.m_bAllValid,
			"%s failed the interior validation contract", ZM_InteriorRoomName(eRoom));
	}
}
