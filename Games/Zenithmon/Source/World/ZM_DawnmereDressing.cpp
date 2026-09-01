#include "Zenith.h"

#include "Zenithmon/Source/World/ZM_DawnmereDressing.h"

#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#include <cmath>

#ifdef ZENITH_TOOLS
#include "Core/Zenith_Engine.h"
#include "Collections/Zenith_Vector.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"
#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include <cstring>
#include <string>
#endif

namespace
{
	// ---- The blind drive legs ----------------------------------------------
	//
	// ★ THESE ARE THE ONLY KEEP-OUT PRIMITIVE THIS FILE SPELLS, and it has to,
	// because a drive leg exists ONLY in test code: it is the straight line
	// DriveTowardXZ walks between two authored anchors, and no shipped data
	// describes it. Every other primitive is READ from the recipe or from
	// ZM_DawnmerePlacement.h.
	//
	// Both endpoints of every leg below are compiled constants from that header,
	// never literals -- so moving an anchor moves its protection with it, which
	// is exactly the drift a mirrored copy would allow.
	//
	// ★ THE THIRD AND FOURTH LEGS LOOK REDUNDANT AND ARE NOT. staging -> target
	// is a SECOND DriveTowardXZ call with a different destination; a prop sitting
	// between the staging waypoint and the doorway sensor would let the first leg
	// finish and wedge the second, which reads as "the warp never fired".
	//
	// ★★ AND EVERY ROW HERE MUST NAME A LEG SOME TEST ACTUALLY DRIVES. See the
	// seam note below for what happens when one is inferred from the map instead:
	// it refuses ground for no reason, and anything that later measures against
	// this table inherits the fiction.
	const ZM_DawnmereDriveLeg s_axDawnmereDriveLegs[] =
	{
		{ "townCentre->homeStaging",
			fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z,
			fZM_DAWNMERE_HOME_X, fZM_DAWNMERE_HOME_DOOR_STAGING_Z },
		{ "homeStaging->homeTarget",
			fZM_DAWNMERE_HOME_X, fZM_DAWNMERE_HOME_DOOR_STAGING_Z,
			fZM_DAWNMERE_HOME_X, fZM_DAWNMERE_HOME_DOOR_TARGET_Z },
		{ "townCentre->labStaging",
			fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z,
			fZM_DAWNMERE_LAB_X, fZM_DAWNMERE_LAB_DOOR_STAGING_Z },
		{ "labStaging->labTarget",
			fZM_DAWNMERE_LAB_X, fZM_DAWNMERE_LAB_DOOR_STAGING_Z,
			fZM_DAWNMERE_LAB_X, fZM_DAWNMERE_LAB_DOOR_TARGET_Z },
		// The NPC walk-up (ZM_NpcTalk_Test): straight +Z off the spawn.
		{ "townCentre->villager",
			fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z,
			fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z + 10.0f },
		// ★★ THE NORTH SEAM WALK IS TWELVE METRES, NOT A HUNDRED AND FORTY, AND
		// THIS TABLE CLAIMED OTHERWISE UNTIL v8 (ZM-D-218). It carried a
		// "townCentre->routeArrival" leg -- the whole length of the route lane --
		// on the assumption that ZM_SeamRoundTrip_Test walks north out of town.
		// It does not: it STAGES each leg with RequestWarp onto the departure
		// scene's own spawn tag and then drives only from that marker into the
		// gate, which is exactly what its own clause "a seam gate is not a short
		// walk from its own marker" is checking. The invented leg sterilised an
		// 84 m strip of lane for scenery it had no reason to refuse, and -- once
		// ZM_Tests_DawnmerePlacement.cpp started measuring NPCs against this
		// table -- failed the VILLAGER, who has stood on the lane's centreline
		// since S6 and has never been on anything a test drives.
		// ★ THE LESSON IS THE TABLE'S OWN PREMISE: a leg belongs here because some
		// test DRIVES it, so read the test rather than the map. The route lane is
		// still protected -- by the "Route" path corridor, which is what actually
		// describes the road.
		{ "routeArrival->northGate",
			fZM_DAWNMERE_FROM_ROUTE1_X, fZM_DAWNMERE_FROM_ROUTE1_Z,
			fZM_DAWNMERE_NORTH_GATE_X, fZM_DAWNMERE_NORTH_GATE_Z },
	};

	// Handed back for an out-of-range index. DEGENERATE and INERT: a zero-length
	// leg at the town centre, under a name no real leg uses, so a caller that
	// skipped the bound check protects a point that is already protected instead
	// of reading off the end of the table.
	const ZM_DawnmereDriveLeg s_xInvalidDriveLeg =
	{
		"UNKNOWN",
		fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z,
		fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z
	};

	// ---- The woodland ------------------------------------------------------
	//
	// Ten discs, every one of them entirely outside the keep-out at
	// fZM_DAWNMERE_TREE_KEEPOUT_MARGIN and entirely inside the terrain at
	// fZM_DAWNMERE_DRESSING_EDGE_MARGIN. Both claims are a UNIT
	// (Tests/ZM_Tests_DawnmereDressing.cpp), not a comment, because the tree
	// brush scatters anywhere in the disc and knows nothing about the town.
	//
	// WHAT THEY ARE FOR, in order: the two flank ridges and the two north knolls
	// are the skyline the landforms build, wooded so the horizon is not bare
	// grass; the two south lobes close the view behind the player's back at
	// spawn; the two route verges line the walk out of town without narrowing the
	// lane; the east meadow fills the quarter the buildings do not use; the south
	// lawn gap is the one small clump the spawn camera actually looks at.
	//
	// ★ THIRTEEN BECAME TEN AT v8 (ZM-D-218), AND THE RADII CAME DOWN WITH THE
	// MAP. On a 256 m sheet a 44 m clump is a sixth of the world; the count fell
	// because there are fewer distinct places left for woodland to BE, not to thin
	// it out -- the trees-per-clump and the spacing are unchanged, so the density
	// inside a clump is exactly what it was.
	const ZM_DawnmereTreeClump s_axDawnmereTreeClumps[] =
	{
		{ "WestRidge",      40.0f, 132.0f, 26.0f },
		{ "NorthWestKnoll", 42.0f, 206.0f, 30.0f },
		{ "NorthEastKnoll", 210.0f, 206.0f, 30.0f },
		{ "EastRidge",     216.0f, 140.0f, 28.0f },
		// ★ MOVED FOR THE ARMED RIVAL'S SIGHT DISC (ZM-D-218). At (62, 38) r 26 this
		// clump reached to within 2.4 m of the rival at (80, 60), and A TREE IS NOT
		// SUBJECT TO THE PROP KEEP-OUT: the tree brush is a TERRAIN tool that knows
		// nothing about this file, so ZM_ScatterDawnmerePropsStep's refusals do not
		// reach it. A trunk on his line makes ZM_ProbeTrainerSightLine report blocked
		// and he never reacts to the player at all.
		// TreeClumps_AreEntirelyOutsideTheTownKeepOut is what reds if it creeps back:
		// it measures against the HARD clearance, which carries his sight radius.
		{ "SouthWestLobe",  44.0f,  34.0f, 24.0f },
		{ "SouthEastLobe", 180.0f,  38.0f, 26.0f },
		{ "RouteVergeWest", 62.0f, 168.0f, 24.0f },
		{ "RouteVergeEast", 176.0f, 172.0f, 24.0f },
		{ "EastMeadow",    212.0f,  80.0f, 24.0f },
		{ "SouthLawnGap",  122.0f,  20.0f, 12.0f },
	};

	// Zero RADIUS is the inert answer for a clump: a dab of radius 0 places
	// nothing at all, so a caller that skipped the bound check paints no trees
	// rather than a pile of them on the town centre.
	const ZM_DawnmereTreeClump s_xInvalidTreeClump =
	{
		"UNKNOWN", fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z, 0.0f
	};

	// ---- The prop scatter --------------------------------------------------
	//
	// Plain literals throughout, deliberately: the four collider floats, the
	// scales and the animation duration all serialize into Dawnmere.zscen through
	// Zenith_InstancedMeshComponent's v5 stream, so they must never be computed
	// through glm/libm at authoring time (see the determinism pin further down).
	//
	// ★ m_fPlazaMin IS THE ART CONTROL AND THE KEEP-OUT IS THE SAFETY ONE. They
	// are not the same thing and neither substitutes for the other: the keep-out
	// is a hard geometric refusal derived from routes and pads, while this is a
	// simple "how close to the middle of town does this KIND of thing belong".
	// Bushes and debris come right up to the paving; standing stones, boulders and
	// fallen trunks stay out where they read as landscape.
	//
	// ★★ THE COLLIDER-FREE ROWS CARRY A MUCH SMALLER m_fPlazaMin THAN THE OTHERS,
	// AND A FIRST DRAFT HAD THEM ALL AT ~45 FOR NO REASON. At that figure the art
	// control was dead: the HARD keep-out already refused everything inside the
	// Plaza pad's 44 m flatten radius, so the two numbers coincided and the town
	// square sat in a bare ring. Splitting the keep-out (see the header) is what
	// let foliage reach the 33 m paving edge, and these numbers are what actually
	// place it there. Verified by looking at the thing: the first capture of the
	// v7 square was people standing in a mown field.
	//
	// The counts are sized for a 256 x 256 m map with roughly a sixth of it
	// spoken for by the town -- about 500 placements plus the woodland's ~280
	// trunk/leaf pairs. Scenery DENSITY is deliberately held roughly constant
	// across the v7 -> v8 shrink rather than the count: a smaller map with the
	// same number of props reads as clutter, and with a scaled count it reads the
	// same, which is the point.
	const ZM_DawnmereScatterGroup s_axDawnmereScatterGroups[] =
	{
		// ---- Stone ---------------------------------------------------------
		// Boulders: the readable ones. Big enough to collide with, so they do.
		// The capsule radius is a FLOOR on the ~1.2 m visual half-width -- proud
		// nowhere the player can reach, and never an invisible wall.
		{ "DawnmereRocks_Boulder",  "Meshes/Rocks/", "Rock_Boulder",  "Rock_Granite.zmtrl", "", 0.0f,
		  0.75f, 2.10f,  36u, 42.0f, 0.85f,  9.0f, 0.85f, 1.85f, 11.0f, 0.0f, 0.0f, 0.16f,
		  0x5B0D1E3u, true,  0.90f, 0.12f, 0.78f },

		// Slabs: low fractured flagstones on the flats. No collider -- a 0.7 m
		// slab is a step, and a capsule is the wrong shape for a plate anyway.
		{ "DawnmereRocks_Slab",     "Meshes/Rocks/", "Rock_Slab",     "Rock_Sandstone.zmtrl", "", 0.0f,
		  0.35f, 2.40f,  29u, 26.0f, 0.30f, 11.0f, 0.70f, 1.35f, 16.0f, 0.0f, 0.0f, 0.22f,
		  0x2C71A9Fu, false, 0.0f,  0.0f,  0.0f },

		// Shards: upright standing stones, kept out on the flanks where they read
		// against the sky. Tight tilt so they stay standing.
		{ "DawnmereRocks_Shard",    "Meshes/Rocks/", "Rock_Shard",    "Rock_Granite.zmtrl", "", 0.0f,
		  1.30f, 2.60f,  16u, 62.0f, 0.70f, 15.0f, 0.85f, 1.75f,  7.0f, 0.0f, 0.0f, 0.10f,
		  0x71E4C05u, true,  0.42f, 1.05f, 1.45f },

		// Pebble clusters: the density layer. Six stones per instance, so 116
		// instances read as ~700 stones for 116 transforms.
		{ "DawnmereRocks_Pebbles",  "Meshes/Rocks/", "Rock_Pebbles",  "Rock_Granite.zmtrl", "", 0.0f,
		  0.20f, 1.10f,  91u, 24.0f, 1.05f,  4.5f, 0.65f, 1.55f, 14.0f, 0.0f, 0.0f, 0.20f,
		  0x1A3F77Bu, false, 0.0f,  0.0f,  0.0f },

		// ---- Deadwood ------------------------------------------------------
		// ★ The three tipped rows below are why m_fLayDownDeg exists. Every piece
		// is MODELLED STANDING (along +Y, origin on the butt), and the instance
		// rotation lays it down -- which is also what gives it a correctly aligned
		// horizontal capsule, since Zenith_InstanceColliderConfig can only
		// describe a Y-aligned one and CreateInstanceBody rotates it by the
		// instance transform. See Tools/Zenith_Tools_FallenTreeAssetExport.cpp.
		//
		// Their m_fSinkFraction is NEGATIVE: once a log is on its side its axis
		// sits one radius above the ground, so the instance has to be LIFTED by
		// roughly that much or the trunk is buried to its midline.
		//
		// The collider spans the trunk: halfHeight = length/2 - radius, offset =
		// length/2, both in the mesh's own standing frame.
		{ "DawnmereDeadwood_Log",   "Meshes/FallenTrees/", "FallenTree_Log",      "FallenTree_Bark.zmtrl", "", 0.0f,
		  3.25f, 3.70f,  13u, 50.0f, 0.42f, 18.0f, 0.80f, 1.20f,  5.0f, 90.0f, 6.50f, -0.26f,
		  0x3D91C77u, true,  0.28f, 2.97f, 3.25f },

		{ "DawnmereDeadwood_LogMossy", "Meshes/FallenTrees/", "FallenTree_LogMossy", "FallenTree_MossyBark.zmtrl", "", 0.0f,
		  2.30f, 2.90f,   9u, 58.0f, 0.38f, 20.0f, 0.85f, 1.30f,  6.0f, 90.0f, 4.60f, -0.33f,
		  0x64B2E19u, true,  0.34f, 1.96f, 2.30f },

		// The stump is NOT tipped: it is what the fallen tree left behind, and it
		// keeps the authored standing attitude, so it also takes the upright
		// rotation path below.
		// ★ The capsule is sized from the stump's REAL height, not from
		// m_fLengthMetres. A stump's root flare and its 0.40 splinter depth add
		// ~28% on top of the axis length, and sizing the capsule from the axis
		// left its top 0.36 m with no collider.
		{ "DawnmereDeadwood_Stump", "Meshes/FallenTrees/", "FallenTree_Stump",    "FallenTree_Bark.zmtrl", "", 0.0f,
		  0.62f, 1.30f,  18u, 44.0f, 0.62f, 12.0f, 0.75f, 1.35f,  6.0f, 0.0f, 0.0f, 0.08f,
		  0x9C05D41u, true,  0.36f, 0.44f, 0.80f },

		// Loose branches: debris, walked through, no collider.
		{ "DawnmereDeadwood_Branches", "Meshes/FallenTrees/", "FallenTree_Branches", "FallenTree_Bark.zmtrl", "", 0.0f,
		  0.30f, 1.60f,  40u, 28.0f, 0.85f,  7.0f, 0.75f, 1.40f, 12.0f, 90.0f, 1.70f, -0.07f,
		  0x2F8A653u, false, 0.0f,  0.0f,  0.0f },

		// ---- Bushes --------------------------------------------------------
		// The three wind-animated rows. Each names the sway VAT baked beside its
		// mesh and the clip duration (a plain literal -- it serializes through the
		// component). All three are foliage you brush past: no collider, same
		// reasoning as the tree leaves and the pebble clusters. Upright, small
		// positive sink so the stems root into a slope. The bounds spheres carry
		// ~0.3 m of slack over the authored envelope so a card at sway peak cannot
		// cross its own cull sphere.
		//
		// ★ THESE ARE THE ROWS THAT COME CLOSEST TO TOWN, and that is the point:
		// they are the verge planting that makes a lane read as a lane. They have
		// no colliders, so the keep-out margin alone carries them.
		{ "DawnmereBushes_Broad",   "Meshes/Bushes/", "Bush_Broad",   "Bush_Foliage.zmtrl", "Bush_Broad_Sway.zanmt", 4.0f,
		  0.60f, 2.00f,  86u, 24.0f, 0.55f,  6.0f, 0.80f, 1.30f,  7.0f, 0.0f, 0.0f, 0.06f,
		  0x6B31F4Du, false, 0.0f,  0.0f,  0.0f },

		// The mound is the density layer of the three: hardy, climbs further up
		// the flanks, packs tightest.
		{ "DawnmereBushes_Mound",   "Meshes/Bushes/", "Bush_Mound",   "Bush_Foliage.zmtrl", "Bush_Mound_Sway.zanmt", 4.0f,
		  0.42f, 1.50f, 109u, 24.0f, 0.80f,  4.5f, 0.70f, 1.25f,  9.0f, 0.0f, 0.0f, 0.06f,
		  0x2D85C1Bu, false, 0.0f,  0.0f,  0.0f },

		// Spindly: sparse, further out, a distinct silhouette against the flanks
		// the shards stand on.
		{ "DawnmereBushes_Spindly", "Meshes/Bushes/", "Bush_Spindly", "Bush_Foliage.zmtrl", "Bush_Spindly_Sway.zanmt", 4.0f,
		  0.85f, 1.90f,  53u, 30.0f, 0.60f,  8.0f, 0.85f, 1.35f,  6.0f, 0.0f, 0.0f, 0.04f,
		  0x7A19E63u, false, 0.0f,  0.0f,  0.0f },
	};

	// Zero COUNT and empty (never null) strings: a caller that skipped the bound
	// check places nothing and cannot dereference a null while building an asset
	// path out of the row.
	const ZM_DawnmereScatterGroup s_xInvalidScatterGroup =
	{
		"UNKNOWN", "", "", "", "", 0.0f,
		0.0f, 0.0f, 0u, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		1u, false, 0.0f, 0.0f, 0.0f
	};

	float ZM_PlanarDistance(float fAX, float fAZ, float fBX, float fBZ)
	{
		const float fDX = fBX - fAX;
		const float fDZ = fBZ - fAZ;
		return std::sqrt(fDX * fDX + fDZ * fDZ);
	}

	// Distance from a point to a SEGMENT, clamped at both ends. A polyline
	// corridor is the union of these, which is what a walked route actually is.
	float ZM_SegmentDistance(float fPX, float fPZ,
		float fAX, float fAZ, float fBX, float fBZ)
	{
		const float fDX = fBX - fAX;
		const float fDZ = fBZ - fAZ;
		const float fLenSq = fDX * fDX + fDZ * fDZ;
		if (fLenSq <= 0.0f)
		{
			return ZM_PlanarDistance(fPX, fPZ, fAX, fAZ);
		}
		float fT = ((fPX - fAX) * fDX + (fPZ - fAZ) * fDZ) / fLenSq;
		fT = fT < 0.0f ? 0.0f : (fT > 1.0f ? 1.0f : fT);
		return ZM_PlanarDistance(fPX, fPZ, fAX + fDX * fT, fAZ + fDZ * fT);
	}
}

u_int ZM_GetDawnmereDriveLegCount()
{
	return (u_int)(sizeof(s_axDawnmereDriveLegs) / sizeof(s_axDawnmereDriveLegs[0]));
}

const ZM_DawnmereDriveLeg& ZM_GetDawnmereDriveLeg(u_int uLeg)
{
	if (uLeg >= ZM_GetDawnmereDriveLegCount())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"ZM_GetDawnmereDriveLeg: index %u is out of range (%u legs)",
			uLeg, ZM_GetDawnmereDriveLegCount());
		return s_xInvalidDriveLeg;
	}
	return s_axDawnmereDriveLegs[uLeg];
}

u_int ZM_GetDawnmereTreeClumpCount()
{
	return (u_int)(sizeof(s_axDawnmereTreeClumps) / sizeof(s_axDawnmereTreeClumps[0]));
}

const ZM_DawnmereTreeClump& ZM_GetDawnmereTreeClump(u_int uClump)
{
	if (uClump >= ZM_GetDawnmereTreeClumpCount())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"ZM_GetDawnmereTreeClump: index %u is out of range (%u clumps)",
			uClump, ZM_GetDawnmereTreeClumpCount());
		return s_xInvalidTreeClump;
	}
	return s_axDawnmereTreeClumps[uClump];
}

u_int ZM_GetDawnmereScatterGroupCount()
{
	return (u_int)(sizeof(s_axDawnmereScatterGroups) / sizeof(s_axDawnmereScatterGroups[0]));
}

const ZM_DawnmereScatterGroup& ZM_GetDawnmereScatterGroup(u_int uGroup)
{
	if (uGroup >= ZM_GetDawnmereScatterGroupCount())
	{
		Zenith_Error(LOG_CATEGORY_SCENE,
			"ZM_GetDawnmereScatterGroup: index %u is out of range (%u groups)",
			uGroup, ZM_GetDawnmereScatterGroupCount());
		return s_xInvalidScatterGroup;
	}
	return s_axDawnmereScatterGroups[uGroup];
}

u_int ZM_GetDawnmereScatterRequestedTotal()
{
	u_int uTotal = 0u;
	for (u_int u = 0u; u < ZM_GetDawnmereScatterGroupCount(); ++u)
	{
		uTotal += s_axDawnmereScatterGroups[u].m_uCount;
	}
	return uTotal;
}

// ============================================================================
// THE KEEP-OUT
//
// ★ EVERY PRIMITIVE EXCEPT THE DRIVE LEGS IS READ, NOT MIRRORED. The pads and
// path corridors come out of ZM_GetDawnmereTerrainRecipe() -- the same const
// table the bake walks -- and the anchors and markers out of
// ZM_DawnmerePlacement.h. Widening a pad or moving an NPC therefore widens or
// moves its protection in the SAME edit, which a second copy of those numbers
// could not do.
//
// ★ THE PATHS ARE PROTECTED AT THEIR **FLATTEN** RADIUS, NOT THEIR DIRT ONE.
// The dirt radius is what the lane LOOKS like; the flatten radius is what the
// terrain was graded to, and it is the wider of the two on all three paths. A
// boulder on graded-but-unpainted ground beside a lane is exactly the thing a
// player walks into while following the visible path.
// ============================================================================
namespace
{
	// ★ ZM_DawnmereBodyAnchorClearance USED TO LIVE HERE, and it is now a public
	// entry point at the bottom of this file -- the authored outdoor prop table
	// needs this half of the keep-out WITHOUT the graded-ground half, because a
	// hand-placed prop has already exercised the judgement that half stands in
	// for. The header carries the argument.

	float ZM_DawnmereBodyAnchorClearanceImpl(float fX, float fZ, bool bIncludeTrainerSight)
	{
		float fBest = 1.0e9f;

		for (u_int u = 0u; u < ZM_GetDawnmereDriveLegCount(); ++u)
		{
			const ZM_DawnmereDriveLeg& xLeg = ZM_GetDawnmereDriveLeg(u);
			const float fD = ZM_SegmentDistance(fX, fZ,
				xLeg.m_fAX, xLeg.m_fAZ, xLeg.m_fBX, xLeg.m_fBZ)
				- fZM_DAWNMERE_DRIVE_LEG_RADIUS;
			fBest = fD < fBest ? fD : fBest;
		}

		for (u_int u = 0u; u < ZM_DAWNMERE_NPC_COUNT; ++u)
		{
			const ZM_DawnmereNpcAnchor& xAnchor = ZM_GetDawnmereNpcAnchor(u);
			const bool bTrainer =
				bIncludeTrainerSight && (u == (u_int)ZM_DAWNMERE_NPC_RIVAL_VESPER);
			const float fRadius = bTrainer
				? fZM_DAWNMERE_KEEPOUT_TRAINER_RADIUS
				: fZM_DAWNMERE_KEEPOUT_ANCHOR_RADIUS;
			const float fD =
				ZM_PlanarDistance(fX, fZ, xAnchor.m_fX, xAnchor.m_fZ) - fRadius;
			fBest = fD < fBest ? fD : fBest;
		}

		for (u_int u = 0u; u < ZM_GetDawnmereWanderWaypointCount(); ++u)
		{
			const ZM_DawnmereNpcAnchor& xWaypoint = ZM_GetDawnmereWanderWaypoint(u);
			const float fD = ZM_PlanarDistance(fX, fZ, xWaypoint.m_fX, xWaypoint.m_fZ)
				- fZM_DAWNMERE_KEEPOUT_ANCHOR_RADIUS;
			fBest = fD < fBest ? fD : fBest;
		}

		// The four points a body is WARPED onto. A prop overlapping one of these
		// puts an arriving capsule inside it, which for a collider is a different
		// and worse failure than a blocked walk -- and for a bush is a player who
		// materialises inside a shrub.
		const float afMarkerX[] =
		{
			fZM_DAWNMERE_TOWN_CENTER_X,
			fZM_DAWNMERE_HOME_X,
			fZM_DAWNMERE_LAB_X,
			fZM_DAWNMERE_FROM_ROUTE1_X,
		};
		const float afMarkerZ[] =
		{
			fZM_DAWNMERE_TOWN_CENTER_Z,
			fZM_DAWNMERE_FROM_HOME_SPAWN_Z,
			fZM_DAWNMERE_FROM_LAB_SPAWN_Z,
			fZM_DAWNMERE_FROM_ROUTE1_Z,
		};
		static_assert(sizeof(afMarkerX) == sizeof(afMarkerZ),
			"the Dawnmere keep-out marker tables must be the same length");
		for (u_int u = 0u; u < (u_int)(sizeof(afMarkerX) / sizeof(afMarkerX[0])); ++u)
		{
			const float fD = ZM_PlanarDistance(fX, fZ, afMarkerX[u], afMarkerZ[u])
				- fZM_DAWNMERE_KEEPOUT_MARKER_RADIUS;
			fBest = fD < fBest ? fD : fBest;
		}

		// The north seam gate, as a BOX rather than a disc: it is 48 m wide and 6 m
		// deep, and a disc big enough to contain it would sterilise 24 m of lane
		// either side of the gate for no reason.
		const ZM_DawnmereBlockout xGate = ZM_GetDawnmereNorthGate();
		{
			const float fDX = std::fabs(fX - xGate.m_xCenter.x) - xGate.m_xScale.x * 0.5f;
			const float fDZ = std::fabs(fZ - xGate.m_xCenter.z) - xGate.m_xScale.z * 0.5f;
			const float fOutX = fDX > 0.0f ? fDX : 0.0f;
			const float fOutZ = fDZ > 0.0f ? fDZ : 0.0f;
			const float fD = std::sqrt(fOutX * fOutX + fOutZ * fOutZ);
			fBest = fD < fBest ? fD : fBest;
		}

		return fBest;
	}

	// The pad/path half, at whichever radius the caller's variant uses.
	float ZM_DawnmereGradedGroundClearance(float fX, float fZ, bool bSoft)
	{
		float fBest = 1.0e9f;
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();

		for (u_int u = 0u; u < xRecipe.m_uPadCount; ++u)
		{
			const ZM_TerrainPadSpec& xPad = xRecipe.m_pxPads[u];
			const float fFloor =
				xPad.m_fFlattenRadius * fZM_DAWNMERE_SOFT_RADIUS_FLATTEN_FRACTION;
			const float fSoft = xPad.m_fDirtRadius > fFloor ? xPad.m_fDirtRadius : fFloor;
			const float fRadius = bSoft ? fSoft : xPad.m_fFlattenRadius;
			const float fD =
				ZM_PlanarDistance(fX, fZ, xPad.m_xCentre.m_fX, xPad.m_xCentre.m_fZ)
				- fRadius;
			fBest = fD < fBest ? fD : fBest;
		}

		for (u_int u = 0u; u < xRecipe.m_uPathCount; ++u)
		{
			const ZM_TerrainPathSpec& xPath = xRecipe.m_pxPaths[u];
			const float fFloor =
				xPath.m_fFlattenRadius * fZM_DAWNMERE_SOFT_RADIUS_FLATTEN_FRACTION;
			const float fSoft = xPath.m_fDirtRadius > fFloor ? xPath.m_fDirtRadius : fFloor;
			const float fRadius = bSoft ? fSoft : xPath.m_fFlattenRadius;
			for (u_int uSeg = 0u; uSeg + 1u < xPath.m_uPointCount; ++uSeg)
			{
				const ZM_TerrainPoint2& xA = xPath.m_pxPoints[uSeg];
				const ZM_TerrainPoint2& xB = xPath.m_pxPoints[uSeg + 1u];
				const float fD = ZM_SegmentDistance(fX, fZ,
					xA.m_fX, xA.m_fZ, xB.m_fX, xB.m_fZ) - fRadius;
				fBest = fD < fBest ? fD : fBest;
			}
		}

		return fBest;
	}
}

float ZM_DawnmereSoftKeepOutClearance(float fX, float fZ)
{
	if (!std::isfinite(fX) || !std::isfinite(fZ))
	{
		return -1.0e9f;
	}
	const float fGraded = ZM_DawnmereGradedGroundClearance(fX, fZ, /*bSoft*/ true);
	const float fAnchors =
		ZM_DawnmereBodyAnchorClearanceImpl(fX, fZ, /*bIncludeTrainerSight*/ false);
	return fGraded < fAnchors ? fGraded : fAnchors;
}

bool ZM_IsInsideDawnmereSoftKeepOut(float fX, float fZ, float fExtraMargin)
{
	const float fMargin = std::isfinite(fExtraMargin) && fExtraMargin > 0.0f
		? fExtraMargin : 0.0f;
	return ZM_DawnmereSoftKeepOutClearance(fX, fZ) <= fMargin;
}

float ZM_DawnmereKeepOutClearance(float fX, float fZ)
{
	// A non-finite sample can never be a legal placement, and answering a large
	// NEGATIVE distance is what makes every caller's `clearance > margin` test
	// reject it without a second isfinite check at each site.
	if (!std::isfinite(fX) || !std::isfinite(fZ))
	{
		return -1.0e9f;
	}

	float fBest = ZM_DawnmereGradedGroundClearance(fX, fZ, /*bSoft*/ false);

	const float fAnchors =
		ZM_DawnmereBodyAnchorClearanceImpl(fX, fZ, /*bIncludeTrainerSight*/ true);
	return fBest < fAnchors ? fBest : fAnchors;
}

// The safety half on its own. See the header: an AUTHORED placement takes this
// and not the graded-ground term, which would refuse both building pads.
float ZM_DawnmereBodyAnchorClearance(float fX, float fZ, bool bIncludeTrainerSight)
{
	// Same total contract as the two composite entry points: a non-finite sample
	// can never be a legal placement, and a large NEGATIVE distance is what makes
	// every caller's `clearance > margin` test reject it with no second check.
	if (!std::isfinite(fX) || !std::isfinite(fZ))
	{
		return -1.0e9f;
	}
	return ZM_DawnmereBodyAnchorClearanceImpl(fX, fZ, bIncludeTrainerSight);
}

bool ZM_IsInsideDawnmereKeepOut(float fX, float fZ, float fExtraMargin)
{
	const float fMargin = std::isfinite(fExtraMargin) && fExtraMargin > 0.0f
		? fExtraMargin : 0.0f;
	return ZM_DawnmereKeepOutClearance(fX, fZ) <= fMargin;
}

#ifdef ZENITH_TOOLS

// Deterministic-FP, for exactly the reason RenderTest's scatter and the engine's
// tree brush both carry the same pin: every value below -- scatter position,
// per-instance rotation and scale -- is SERIALIZED into Dawnmere.zscen by
// Zenith_InstancedMeshComponent, so under the project's /fp:fast a Debug tools
// boot and a Release one would otherwise author different bytes into a TRACKED
// asset. The rejection tests live inside the pin too: a 1-ULP shift in a slope
// or keep-out test would accept a prop one build rejects, desyncing the RNG
// stream and moving the whole scatter.
//
// ★ EVERY RNG DRAW IS HOISTED INTO ITS OWN NAMED CONST, in the order it is meant
// to happen. C++ does not order the evaluation of function ARGUMENTS, so
// `AuthoringQuatMul(RotX(Next()), RotZ(Next()))` leaves it to the compiler which
// draw feeds which axis -- stable for one toolchain, and a silent re-scatter of
// every instance on the day that changes.
ZENITH_AUTHORING_DETERMINISM_BEGIN

void ZM_ScatterDawnmerePropsStep()
{
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	if (pxSceneData == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "[ZM Dressing] prop scatter: no active scene");
		return;
	}

	// Same "open one if the automation batch has not already" contract the
	// terrain-editor automation actions use -- this step runs after the tree
	// paint on a normal boot, but it must not depend on that ordering to read
	// heights.
	Zenith_TerrainEditor& xTerrainEditor = g_xEngine.TerrainEditor();
	if (!xTerrainEditor.IsActive())
	{
		xTerrainEditor.OpenStandalone();
	}

	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
	const float fWorldMaxX = xRecipe.WorldMaxX();
	const float fWorldMaxZ = xRecipe.WorldMaxZ();
	const float fEdge = fZM_DAWNMERE_DRESSING_EDGE_MARGIN;
	const float fSpanX = fWorldMaxX - 2.0f * fEdge;
	const float fSpanZ = fWorldMaxZ - 2.0f * fEdge;

	// The plaza pad's centre is the town's origin for the m_fPlazaMin art
	// control. Read from the recipe by NAME rather than from a mirrored literal.
	float fPlazaX = fZM_DAWNMERE_TOWN_CENTER_X;
	float fPlazaZ = fZM_DAWNMERE_TOWN_CENTER_Z;
	for (u_int u = 0u; u < xRecipe.m_uPadCount; ++u)
	{
		if (std::strcmp(xRecipe.m_pxPads[u].m_szName, "Plaza") == 0)
		{
			fPlazaX = xRecipe.m_pxPads[u].m_xCentre.m_fX;
			fPlazaZ = xRecipe.m_pxPads[u].m_xCentre.m_fZ;
			break;
		}
	}

	u_int uTotalPlaced = 0u;
	for (u_int uGroup = 0u; uGroup < ZM_GetDawnmereScatterGroupCount(); ++uGroup)
	{
		const ZM_DawnmereScatterGroup& xGroup = s_axDawnmereScatterGroups[uGroup];

		Zenith_Entity xEntity = pxSceneData->FindEntityByName(xGroup.m_szEntity);
		if (!xEntity.IsValid())
		{
			xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, xGroup.m_szEntity);
			xEntity.SetTransient(false);
		}
		Zenith_InstancedMeshComponent* pxComp =
			xEntity.TryGetComponent<Zenith_InstancedMeshComponent>();
		if (pxComp == nullptr)
		{
			pxComp = &xEntity.AddComponent<Zenith_InstancedMeshComponent>();
		}
		pxComp->ClearInstances();
		pxComp->LoadMesh(std::string(ENGINE_ASSETS_DIR) + xGroup.m_szAssetDir
			+ xGroup.m_szMeshBase + ZENITH_MESH_ASSET_EXT);
		pxComp->LoadMaterial(std::string("engine:") + xGroup.m_szAssetDir
			+ xGroup.m_szMaterialFile);
		const bool bAnimated = (xGroup.m_szVATFile[0] != '\0');
		if (bAnimated)
		{
			pxComp->LoadAnimationTexture(std::string(ENGINE_ASSETS_DIR)
				+ xGroup.m_szAssetDir + xGroup.m_szVATFile);
			pxComp->SetAnimationDuration(xGroup.m_fAnimDuration);
		}
		pxComp->SetBounds(Zenith_Maths::Vector3(0.0f, xGroup.m_fBoundsCentreY, 0.0f),
			xGroup.m_fBoundsRadius);
		if (xGroup.m_bCollider)
		{
			pxComp->SetInstanceColliderCapsule(xGroup.m_fColliderRadius,
				xGroup.m_fColliderHalfHeight, xGroup.m_fColliderYOffset);
		}
		pxComp->Reserve(xGroup.m_uCount);

		// xorshift32, inline for the same reason the tree brush keeps its own: the
		// stream position IS part of the authored result, so nothing may consume a
		// draw that a different build would not.
		u_int uRngState = xGroup.m_uSeed;
		auto NextFloat01 = [&uRngState]() -> float
		{
			u_int uX = uRngState;
			uX ^= uX << 13; uX ^= uX >> 17; uX ^= uX << 5;
			uRngState = uX;
			return static_cast<float>(uX & 0xFFFFFFu) / 16777215.0f;
		};

		Zenith_Vector<Zenith_Maths::Vector2> xPlaced;
		Zenith_Maths::Vector3 xFirstPlacement(0.0f, 0.0f, 0.0f);
		const float fSpacingSq = xGroup.m_fSpacing * xGroup.m_fSpacing;
		const float fPlazaMinSq = xGroup.m_fPlazaMin * xGroup.m_fPlazaMin;
		const float fLayDownRadians = Zenith_Maths::AuthoringRadians(xGroup.m_fLayDownDeg);
		const bool bLaidDown = (xGroup.m_fLayDownDeg != 0.0f);

		u_int uPlaced = 0u;
		u_int uRejectedByKeepOut = 0u;
		for (u_int uAttempt = 0u;
			uAttempt < xGroup.m_uCount * 40u && uPlaced < xGroup.m_uCount;
			uAttempt++)
		{
			// UNIFORM OVER THE WHOLE SHEET, not over an annulus about the town.
			// RenderTest scatters into a ring because its campus is a plateau in
			// the middle of an otherwise empty bowl; Dawnmere's town is an
			// irregular footprint of pads and lanes, so "everywhere except the
			// town" is the shape that actually describes where scenery goes, and
			// the keep-out below is what expresses it.
			const float fXDraw = NextFloat01();
			const float fZDraw = NextFloat01();
			const float fPX = fEdge + fXDraw * fSpanX;
			const float fPZ = fEdge + fZDraw * fSpanZ;

			// The art control: how close to the middle of town this KIND of thing
			// belongs. Not a safety property -- that is the keep-out immediately
			// below, and neither substitutes for the other.
			const float fPlazaDX = fPX - fPlazaX;
			const float fPlazaDZ = fPZ - fPlazaZ;
			if (fPlazaDX * fPlazaDX + fPlazaDZ * fPlazaDZ < fPlazaMinSq)
			{
				continue;
			}

			// ★★ THE SAFETY REFUSAL, and WHICH keep-out this group gets is decided
			// by its collider and nothing else. A colliding prop takes the HARD one
			// (pads and paths at their FLATTEN radius) because it can wedge a blind
			// drive; a collider-free one takes the SOFT one (DIRT radius) because a
			// player walks straight through it and the only remaining rule is "stay
			// off the paving". The header carries the full argument, including why
			// the difference is worth a second function: the Plaza pad is graded to
			// 44 m and paved to 33, so the hard variant sterilises the 11 m ring of
			// lawn that verge planting belongs in.
			const bool bBlocked = xGroup.m_bCollider
				? ZM_IsInsideDawnmereKeepOut(fPX, fPZ, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN)
				: ZM_IsInsideDawnmereSoftKeepOut(fPX, fPZ, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN);
			if (bBlocked)
			{
				uRejectedByKeepOut++;
				continue;
			}

			// Slope rejection, central differences over 1 m -- the same estimator
			// the tree brush uses, so the two scatters agree about what "steep" is.
			const float fHL = xTerrainEditor.SampleHeightWorld(fPX - 1.0f, fPZ);
			const float fHR = xTerrainEditor.SampleHeightWorld(fPX + 1.0f, fPZ);
			const float fHD = xTerrainEditor.SampleHeightWorld(fPX, fPZ - 1.0f);
			const float fHU = xTerrainEditor.SampleHeightWorld(fPX, fPZ + 1.0f);
			const float fSlopeTan = 0.5f * std::sqrt(
				(fHR - fHL) * (fHR - fHL) + (fHU - fHD) * (fHU - fHD));
			if (fSlopeTan > xGroup.m_fMaxSlopeTan)
			{
				continue;
			}

			// A tipped piece also has to REJECT ground its length cannot bridge:
			// pitching to the average slope still leaves a log crossing a dip with
			// its middle in the air and both ends buried. The draw order is
			// unaffected -- this consumes nothing.
			if (bLaidDown)
			{
				const float fRunProbe = xGroup.m_fLengthMetres * xGroup.m_fScaleMax;
				const float fHereH = xTerrainEditor.SampleHeightWorld(fPX, fPZ);
				const float fMidH = xTerrainEditor.SampleHeightWorld(
					fPX + fRunProbe * 0.5f, fPZ);
				const float fMidH2 = xTerrainEditor.SampleHeightWorld(
					fPX, fPZ + fRunProbe * 0.5f);
				if (std::fabs(fMidH - fHereH) > fRunProbe * 0.22f ||
					std::fabs(fMidH2 - fHereH) > fRunProbe * 0.22f)
				{
					continue;
				}
			}

			bool bTooClose = false;
			for (u_int u = 0u; u < xPlaced.GetSize(); u++)
			{
				const float fDX = xPlaced.Get(u).x - fPX;
				const float fDZ = xPlaced.Get(u).y - fPZ;
				if (fDX * fDX + fDZ * fDZ < fSpacingSq)
				{
					bTooClose = true;
					break;
				}
			}
			if (bTooClose)
			{
				continue;
			}

			// --- Every draw, hoisted and ordered. See the note above. ----------
			const float fScaleDraw = NextFloat01();
			const float fYawDraw = NextFloat01();
			const float fTiltADraw = NextFloat01();
			const float fTiltBDraw = NextFloat01();
			const float fScaleXDraw = NextFloat01();
			const float fScaleYDraw = NextFloat01();
			const float fScaleZDraw = NextFloat01();

			const float fBaseScale = xGroup.m_fScaleMin +
				(xGroup.m_fScaleMax - xGroup.m_fScaleMin) * fScaleDraw;
			const float fTiltLimit = Zenith_Maths::AuthoringRadians(xGroup.m_fTiltDeg);
			const Zenith_Maths::Quat xYaw =
				Zenith_Maths::AuthoringRotationY(fYawDraw * 6.2831853f);

			// An upright prop leans a little off vertical; a laid-down one pitches
			// its far end a little and ROLLS about its own long axis, which is what
			// varies where its branch stubs point. Rolling an upright rock would
			// only re-spin it about the same axis the yaw already covers.
			Zenith_Maths::Quat xAttitude;
			if (bLaidDown)
			{
				// ★ A TIPPED PIECE FOLLOWS THE GROUND ALONG ITS OWN LENGTH, and the
				// one-metre slope test above cannot do that for it. A 6.5 m trunk on
				// the 23-degree ground that test still admits drops 2.7 m end to
				// end, so a log laid dead level buries half of itself in the
				// hillside. Read the height under the FAR end and pitch to match.
				//
				// Local +Y goes to world (sin yaw, 0, cos yaw) once RotX(90) has
				// tipped it, so that is where the far end lands.
				const float fYawAngle = fYawDraw * 6.2831853f;
				const float fRun = xGroup.m_fLengthMetres * fBaseScale;
				const float fFarX = fPX + std::sin(fYawAngle) * fRun;
				const float fFarZ = fPZ + std::cos(fYawAngle) * fRun;
				const float fRise = xTerrainEditor.SampleHeightWorld(fFarX, fFarZ)
					- xTerrainEditor.SampleHeightWorld(fPX, fPZ);

				// RotX(90 + phi) sends the far end's vertical component to -sin(phi),
				// so matching a rise of fRise/fRun means phi = -asin(fRise/fRun).
				const float fRatio = fRise / (fRun > 0.01f ? fRun : 0.01f);
				const float fSlopeSin = fRatio < -1.0f ? -1.0f : (fRatio > 1.0f ? 1.0f : fRatio);
				const Zenith_Maths::Quat xPitch = Zenith_Maths::AuthoringRotationX(
					fLayDownRadians - std::asin(fSlopeSin)
						+ (fTiltADraw * 2.0f - 1.0f) * fTiltLimit);
				const Zenith_Maths::Quat xRoll = Zenith_Maths::AuthoringRotationY(
					fTiltBDraw * 6.2831853f);
				xAttitude = Zenith_Maths::AuthoringQuatMul(xPitch, xRoll);
			}
			else
			{
				const Zenith_Maths::Quat xLeanX = Zenith_Maths::AuthoringRotationX(
					(fTiltADraw * 2.0f - 1.0f) * fTiltLimit);
				const Zenith_Maths::Quat xLeanZ = Zenith_Maths::AuthoringRotationZ(
					(fTiltBDraw * 2.0f - 1.0f) * fTiltLimit);
				xAttitude = Zenith_Maths::AuthoringQuatMul(xLeanX, xLeanZ);
			}
			const Zenith_Maths::Quat xRotation =
				Zenith_Maths::AuthoringQuatMul(xYaw, xAttitude);

			// The mesh's origin sits on its own base, so the sample height IS the
			// resting height; the sink buries the seam on a slope (and a negative
			// sink lifts a tipped log onto its own radius).
			const float fY = xTerrainEditor.SampleHeightWorld(fPX, fPZ)
				- xGroup.m_fSinkFraction * fBaseScale;
			const Zenith_Maths::Vector3 xPosition(fPX, fY, fPZ);
			const Zenith_Maths::Vector3 xScale(
				fBaseScale * (0.92f + 0.16f * fScaleXDraw),
				fBaseScale * (0.86f + 0.28f * fScaleYDraw),
				fBaseScale * (0.92f + 0.16f * fScaleZDraw));

			const uint32_t uInstanceID = pxComp->SpawnInstance(xPosition, xRotation, xScale);
			if (bAnimated)
			{
				// The SAME golden-ratio derivation ReadFromDataStream applies on
				// load (phase is transient, never serialized), so the authoring
				// session's sway matches what every reload of the scene shows.
				// Not an RNG draw -- the placement stream is untouched.
				pxComp->SetInstanceAnimationByIndex(uInstanceID, 0,
					std::fmod(static_cast<float>(uInstanceID) * 0.618034f, 1.0f));
			}
			xPlaced.PushBack(Zenith_Maths::Vector2(fPX, fPZ));
			if (uPlaced == 0u)
			{
				xFirstPlacement = xPosition;
			}
			uPlaced++;
		}

		uTotalPlaced += uPlaced;
		// The first placement and the keep-out rejection count are logged because
		// they are the only cheap way to answer "where did the scatter land" and
		// "is the town eating the whole map" from outside the process.
		Zenith_Log(LOG_CATEGORY_MESH,
			"[ZM Dressing] %s x%u (of %u requested, %u rejected by keep-out), "
			"first at (%.1f, %.1f, %.1f)",
			xGroup.m_szEntity, uPlaced, xGroup.m_uCount, uRejectedByKeepOut,
			xFirstPlacement.x, xFirstPlacement.y, xFirstPlacement.z);
	}

	Zenith_Log(LOG_CATEGORY_MESH,
		"[ZM Dressing] Dawnmere prop scatter complete: %u instances in %u groups "
		"(%u requested)",
		uTotalPlaced, ZM_GetDawnmereScatterGroupCount(),
		ZM_GetDawnmereScatterRequestedTotal());
}

ZENITH_AUTHORING_DETERMINISM_END

#endif // ZENITH_TOOLS
