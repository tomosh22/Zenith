#include "Zenith.h"

// ============================================================================
// ZM_Tests_DawnmereDressing (ZM-D-217) -- the boot units that make Dawnmere's
// scenery layer safe to author.
//
// ★★ WHAT THIS SUITE EXISTS TO PREVENT, and it is not a rendering defect.
// Boulders, shards, stumps, logs and tree TRUNKS all carry per-instance capsule
// colliders. Every automated traversal in this game drives the player with
// DriveTowardXZ, which has NO obstacle avoidance: it holds a direction until the
// target is reached or the frame cap expires. A 1.8 m static body anywhere on
// such a leg stops the capsule dead -- the step assist is 0.40 m -- and the
// suite dies at its frame cap with a timeout naming a DISTANCE. Nothing in that
// failure mentions a rock, a scatter or this file, and the offending prop is at
// a position produced by an RNG stream nobody can read off the source.
//
// So the scatter's safety is not something an author can eyeball, and the
// authoring block in Zenithmon.cpp has carried the hazard as PROSE since S6
// ("THE OTHER TWO MUST STAY OFF THE HOME DRIVE CORRIDOR"). A rule in prose in
// front of a check IS the defect. These units are the check.
//
// PURE: no scene, no entity, no physics, no assets, no graphics. They read the
// COMPILED Dawnmere terrain recipe (a static table, never a bake), the compiled
// placement constants and the compiled dressing tables, so they run in the
// headless CI gate -- which is exactly where they are needed, because the
// automated tests that would notice a wedged corridor need baked terrain and can
// RequestSkip.
//
// ★ NO ENTITY IS CREATED HERE, which is a hard constraint rather than a style
// note: the boot unit suite runs before the initial scene load and allocates
// entity indices that scene authoring would then bake in.
// ============================================================================

#include <cmath>
#include <cstring>   // strcmp -- the entity-name and asset-path walks
#include <limits>    // quiet_NaN / infinity -- the totality sweeps

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"   // fZM_SIGHT_MAX_DISTANCE
#include "Zenithmon/Source/World/ZM_DawnmereDressing.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

namespace
{
	// How finely a corridor or a disc is walked. 0.5 m is well under every radius
	// in play (the smallest keep-out primitive is a 5 m anchor disc), so a sampled
	// walk cannot step over a gap it should have found.
	constexpr float fDRESSING_WALK_STEP = 0.5f;

	float DressingPlanarDistance(float fAX, float fAZ, float fBX, float fBZ)
	{
		const float fDX = fBX - fAX;
		const float fDZ = fBZ - fAZ;
		return std::sqrt(fDX * fDX + fDZ * fDZ);
	}

	const ZM_TerrainAuthoringRecipe& DressingRecipe()
	{
		return ZM_GetDawnmereTerrainRecipe();
	}
}

// ============================================================================
// THE WOODLAND
// ============================================================================

// ★★ THE CLAUSE THE WHOLE SUITE IS FOR. The engine's tree brush scatters
// anywhere inside the dab disc and knows NOTHING about the town, so it is the
// WHOLE DISC that has to clear the keep-out, not its centre. Checking the centre
// would pass a 44 m clump whose near edge sits on the plaza.
ZENITH_TEST(ZM_Dressing, TreeClumps_AreEntirelyOutsideTheTownKeepOut)
{
	const u_int uCount = ZM_GetDawnmereTreeClumpCount();
	ZENITH_ASSERT_GT(uCount, 0u,
		"Dawnmere has no tree clumps at all, so this suite proves nothing about "
		"the woodland it was written for");

	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_DawnmereTreeClump& xClump = ZM_GetDawnmereTreeClump(u);
		const float fClearance =
			ZM_DawnmereKeepOutClearance(xClump.m_fX, xClump.m_fZ);
		const float fRequired =
			xClump.m_fRadius + fZM_DAWNMERE_TREE_KEEPOUT_MARGIN;
		ZENITH_ASSERT_GE(fClearance, fRequired,
			"tree clump '%s' at (%.1f, %.1f) has radius %.1f but only %.2f m of "
			"keep-out clearance, so the brush can scatter a COLLIDING trunk %.2f m "
			"inside the town. Every automated traversal drives blind: this ships as "
			"a timeout naming a distance, in a suite that never mentions trees",
			xClump.m_szName, xClump.m_fX, xClump.m_fZ, xClump.m_fRadius,
			fClearance, fRequired - fClearance);
	}
}

// ...and entirely INSIDE the terrain. A dab that reaches past the sheet places
// trees on clamped heights at the boundary, which reads as a row of trunks
// standing in a wall.
ZENITH_TEST(ZM_Dressing, TreeClumps_AreEntirelyInsideTheTerrain)
{
	const ZM_TerrainAuthoringRecipe& xRecipe = DressingRecipe();
	const float fEdge = fZM_DAWNMERE_DRESSING_EDGE_MARGIN;

	for (u_int u = 0u; u < ZM_GetDawnmereTreeClumpCount(); ++u)
	{
		const ZM_DawnmereTreeClump& xClump = ZM_GetDawnmereTreeClump(u);
		ZENITH_ASSERT_GE(xClump.m_fX - xClump.m_fRadius, xRecipe.WorldMinX() + fEdge,
			"tree clump '%s' reaches past the terrain's -X edge", xClump.m_szName);
		ZENITH_ASSERT_LE(xClump.m_fX + xClump.m_fRadius, xRecipe.WorldMaxX() - fEdge,
			"tree clump '%s' reaches past the terrain's +X edge", xClump.m_szName);
		ZENITH_ASSERT_GE(xClump.m_fZ - xClump.m_fRadius, xRecipe.WorldMinZ() + fEdge,
			"tree clump '%s' reaches past the terrain's -Z edge", xClump.m_szName);
		ZENITH_ASSERT_LE(xClump.m_fZ + xClump.m_fRadius, xRecipe.WorldMaxZ() - fEdge,
			"tree clump '%s' reaches past the terrain's +Z edge", xClump.m_szName);
	}
}

// Every clump has a real radius and a name of its own. A zero-radius row would
// place nothing while still reading as woodland in the table, and two rows
// sharing a name make a failure message above ambiguous.
ZENITH_TEST(ZM_Dressing, TreeClumps_AreWellFormedAndDistinct)
{
	const u_int uCount = ZM_GetDawnmereTreeClumpCount();
	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_DawnmereTreeClump& xA = ZM_GetDawnmereTreeClump(u);
		ZENITH_ASSERT_GT(xA.m_fRadius, 0.0f,
			"tree clump '%s' has a zero radius and paints nothing", xA.m_szName);
		ZENITH_ASSERT_NOT_NULL(xA.m_szName, "a tree clump has a null name");
		for (u_int v = u + 1u; v < uCount; ++v)
		{
			const ZM_DawnmereTreeClump& xB = ZM_GetDawnmereTreeClump(v);
			ZENITH_ASSERT_NE(std::strcmp(xA.m_szName, xB.m_szName), 0,
				"two Dawnmere tree clumps are both named '%s'", xA.m_szName);
		}
	}

	// The brush settings themselves have to be a legal range, or the dab silently
	// scatters at a single scale (or, with a zero seed, on the engine's fallback
	// stream rather than Dawnmere's).
	ZENITH_ASSERT_GT(iZM_DAWNMERE_TREES_PER_CLUMP, 0,
		"the Dawnmere tree brush places no trees per dab");
	ZENITH_ASSERT_LE(fZM_DAWNMERE_TREE_SCALE_MIN, fZM_DAWNMERE_TREE_SCALE_MAX,
		"the Dawnmere tree scale range is inverted");
	ZENITH_ASSERT_GT(fZM_DAWNMERE_TREE_SPACING, 0.0f,
		"the Dawnmere tree spacing is not positive, so the brush cannot reject a "
		"pile of trunks in one spot");
	ZENITH_ASSERT_NE(iZM_DAWNMERE_TREE_SEED, 0,
		"a zero tree seed makes Zenith_TerrainEditor fall back to its own default "
		"stream, so Dawnmere's woodland would move if that default ever changed");
}

// ============================================================================
// THE KEEP-OUT
// ============================================================================

// ★ THE ANTI-VACUITY CLAUSE, AND IT IS NOT OPTIONAL. Everything above is
// satisfied trivially by a keep-out that covers the entire map -- and so is the
// prop scatter, which would simply place nothing and log a clean run. This
// walks a coarse grid over the sheet and requires that a real majority of it is
// still available, so "the dressing is safe" cannot be achieved by making the
// dressing empty.
ZENITH_TEST(ZM_Dressing, KeepOut_LeavesMostOfTheMapAvailable)
{
	const ZM_TerrainAuthoringRecipe& xRecipe = DressingRecipe();
	const float fEdge = fZM_DAWNMERE_DRESSING_EDGE_MARGIN;

	u_int uSampled = 0u;
	u_int uAvailable = 0u;
	for (float fX = fEdge; fX <= xRecipe.WorldMaxX() - fEdge; fX += 4.0f)
	{
		for (float fZ = fEdge; fZ <= xRecipe.WorldMaxZ() - fEdge; fZ += 4.0f)
		{
			uSampled++;
			if (!ZM_IsInsideDawnmereKeepOut(fX, fZ, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN))
			{
				uAvailable++;
			}
		}
	}

	ZENITH_ASSERT_GT(uSampled, 0u, "the keep-out sweep sampled nothing");
	const float fFraction = (float)uAvailable / (float)uSampled;
	ZENITH_ASSERT_GT(fFraction, 0.55f,
		"the town keep-out covers %.1f%% of Dawnmere, leaving only %.1f%% for "
		"scenery. Every clause in this suite would still pass with an empty "
		"scatter, so this is the clause that says the dressing can exist at all",
		100.0f * (1.0f - fFraction), 100.0f * fFraction);

	// ...and it must not be vacuous in the other direction either: the town has
	// to be inside its own keep-out.
	ZENITH_ASSERT_LT(fFraction, 0.98f,
		"the town keep-out covers almost nothing, so it is not describing a town");
}

// Every blind drive leg is protected along its WHOLE length, sampled rather than
// checked at its endpoints. The endpoints are inside pads and anchors already;
// the middle of a leg is the part that crosses open ground, and it is exactly
// where a boulder would land.
ZENITH_TEST(ZM_Dressing, KeepOut_CoversEveryBlindDriveLegEndToEnd)
{
	const u_int uLegs = ZM_GetDawnmereDriveLegCount();
	ZENITH_ASSERT_GT(uLegs, 0u, "no blind drive legs are declared at all");

	for (u_int u = 0u; u < uLegs; ++u)
	{
		const ZM_DawnmereDriveLeg& xLeg = ZM_GetDawnmereDriveLeg(u);
		const float fLength = DressingPlanarDistance(
			xLeg.m_fAX, xLeg.m_fAZ, xLeg.m_fBX, xLeg.m_fBZ);
		u_int uSteps = (u_int)std::ceil(fLength / fDRESSING_WALK_STEP);
		if (uSteps == 0u)
		{
			uSteps = 1u;
		}
		for (u_int uStep = 0u; uStep <= uSteps; ++uStep)
		{
			const float fT = (float)uStep / (float)uSteps;
			const float fX = xLeg.m_fAX + (xLeg.m_fBX - xLeg.m_fAX) * fT;
			const float fZ = xLeg.m_fAZ + (xLeg.m_fBZ - xLeg.m_fAZ) * fT;
			ZENITH_ASSERT_TRUE(
				ZM_IsInsideDawnmereKeepOut(fX, fZ, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
				"the blind drive leg '%s' passes through (%.2f, %.2f), which the "
				"prop scatter considers available ground. A colliding prop there "
				"wedges a suite that never mentions the scatter",
				xLeg.m_szName, fX, fZ);
		}
	}
}

// Every place a body STANDS or is WARPED to is inside the keep-out. A prop
// overlapping a spawn marker puts an arriving capsule inside a static body,
// which is a different and worse failure than a blocked walk.
ZENITH_TEST(ZM_Dressing, KeepOut_CoversEveryAuthoredAnchorAndMarker)
{
	struct DressingPoint { const char* m_szName; float m_fX; float m_fZ; };
	const DressingPoint axPoints[] =
	{
		{ "TownCenterSpawn", fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z },
		{ "FromHomeSpawn",   fZM_DAWNMERE_HOME_X,        fZM_DAWNMERE_FROM_HOME_SPAWN_Z },
		{ "HomeDoorTrigger", fZM_DAWNMERE_HOME_X,        fZM_DAWNMERE_HOME_TRIGGER_Z },
		{ "HomeShell",       fZM_DAWNMERE_HOME_X,        fZM_DAWNMERE_HOME_SHELL_Z },
		{ "HomeDoorLeft",    fZM_DAWNMERE_HOME_DOOR_LEFT_X,  fZM_DAWNMERE_HOME_ENTRANCE_Z },
		{ "HomeDoorRight",   fZM_DAWNMERE_HOME_DOOR_RIGHT_X, fZM_DAWNMERE_HOME_ENTRANCE_Z },
		{ "FromLabSpawn",    fZM_DAWNMERE_LAB_X,         fZM_DAWNMERE_FROM_LAB_SPAWN_Z },
		{ "LabDoorTrigger",  fZM_DAWNMERE_LAB_X,         fZM_DAWNMERE_LAB_TRIGGER_Z },
		{ "LabShell",        fZM_DAWNMERE_LAB_X,         fZM_DAWNMERE_LAB_SHELL_Z },
		{ "LabDoorLeft",     fZM_DAWNMERE_LAB_DOOR_LEFT_X,   fZM_DAWNMERE_LAB_ENTRANCE_Z },
		{ "LabDoorRight",    fZM_DAWNMERE_LAB_DOOR_RIGHT_X,  fZM_DAWNMERE_LAB_ENTRANCE_Z },
		{ "FromRoute1Spawn", fZM_DAWNMERE_FROM_ROUTE1_X, fZM_DAWNMERE_FROM_ROUTE1_Z },
		{ "NorthGate",       fZM_DAWNMERE_NORTH_GATE_X,  fZM_DAWNMERE_NORTH_GATE_Z },
	};

	for (const DressingPoint& xPoint : axPoints)
	{
		ZENITH_ASSERT_TRUE(
			ZM_IsInsideDawnmereKeepOut(xPoint.m_fX, xPoint.m_fZ,
				fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
			"the authored point '%s' at (%.1f, %.1f) is on ground the prop scatter "
			"considers available", xPoint.m_szName, xPoint.m_fX, xPoint.m_fZ);
	}

	for (u_int u = 0u; u < ZM_DAWNMERE_NPC_COUNT; ++u)
	{
		const ZM_DawnmereNpcAnchor& xAnchor = ZM_GetDawnmereNpcAnchor(u);
		ZENITH_ASSERT_TRUE(
			ZM_IsInsideDawnmereKeepOut(xAnchor.m_fX, xAnchor.m_fZ,
				fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
			"the authored NPC '%s' at (%.1f, %.1f) stands on ground the prop "
			"scatter considers available", xAnchor.m_szEntityName,
			xAnchor.m_fX, xAnchor.m_fZ);
	}

	// ★★ THE ARMED TRAINER'S WHOLE SIGHT RANGE IS CLEAR OF COLLIDING PROPS, not
	// just his body. ZM_Interactable::TickTrainerSight runs an OCCLUSION RAYCAST
	// (ZM_ProbeTrainerSightLine) once the pure cone passes, so a boulder, shard,
	// stump, log or tree trunk anywhere on the line from him to the player makes
	// him permanently blind -- and the failure reads as "the walk-up STALLED" with
	// every placement clause green, which is what it did.
	//
	// The radius is asserted against fZM_SIGHT_MAX_DISTANCE rather than a literal
	// so that raising the sight range and forgetting the keep-out reds HERE.
	ZENITH_ASSERT_GT(fZM_DAWNMERE_KEEPOUT_TRAINER_RADIUS, fZM_SIGHT_MAX_DISTANCE,
		"the rival's keep-out radius (%.1f m) no longer exceeds his sight range "
		"(%.1f m) -- a colliding prop can stand inside the line he needs clear",
		fZM_DAWNMERE_KEEPOUT_TRAINER_RADIUS, fZM_SIGHT_MAX_DISTANCE);
	{
		const ZM_DawnmereNpcAnchor& xRival =
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_RIVAL_VESPER);
		// Walk his whole sight disc, not just the cone: he turns to WATCHING from
		// any bearing, and the scatter has no idea which way he faces.
		for (u_int u = 0u; u < 72u; ++u)
		{
			const float fAngle = 6.2831853f * (float)u / 72.0f;
			for (float fR = 1.0f; fR <= fZM_SIGHT_MAX_DISTANCE; fR += 1.0f)
			{
				const float fX = xRival.m_fX + std::cos(fAngle) * fR;
				const float fZ = xRival.m_fZ + std::sin(fAngle) * fR;
				ZENITH_ASSERT_TRUE(
					ZM_IsInsideDawnmereKeepOut(fX, fZ, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
					"(%.1f, %.1f) is %.1f m from the armed rival -- inside the %.1f m "
					"sight line his occlusion probe needs clear -- and the HARD "
					"keep-out admits a colliding prop there",
					fX, fZ, fR, fZM_SIGHT_MAX_DISTANCE);
			}
		}
	}

	for (u_int u = 0u; u < ZM_GetDawnmereWanderWaypointCount(); ++u)
	{
		const ZM_DawnmereNpcAnchor& xWaypoint = ZM_GetDawnmereWanderWaypoint(u);
		ZENITH_ASSERT_TRUE(
			ZM_IsInsideDawnmereKeepOut(xWaypoint.m_fX, xWaypoint.m_fZ,
				fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
			"the patrol waypoint '%s' at (%.1f, %.1f) is on ground the prop "
			"scatter considers available", xWaypoint.m_szEntityName,
			xWaypoint.m_fX, xWaypoint.m_fZ);
	}
}

// ★ THE KEEP-OUT IS DERIVED FROM THE RECIPE, NOT MIRRORED, AND THIS IS WHAT SAYS
// SO. Every pad centre must be inside it. If someone re-spelled the pad list as
// literals here and then widened a pad in ZM_TerrainAuthoring.cpp, the two would
// part silently; reading the recipe by NAME is what makes that impossible, and
// this clause is what notices if the reading is ever replaced by a copy.
ZENITH_TEST(ZM_Dressing, KeepOut_TracksTheTerrainRecipesPadsAndPaths)
{
	const ZM_TerrainAuthoringRecipe& xRecipe = DressingRecipe();
	ZENITH_ASSERT_GT(xRecipe.m_uPadCount, 0u, "the Dawnmere recipe has no pads");
	ZENITH_ASSERT_GT(xRecipe.m_uPathCount, 0u, "the Dawnmere recipe has no paths");

	for (u_int u = 0u; u < xRecipe.m_uPadCount; ++u)
	{
		const ZM_TerrainPadSpec& xPad = xRecipe.m_pxPads[u];
		ZENITH_ASSERT_TRUE(
			ZM_IsInsideDawnmereKeepOut(xPad.m_xCentre.m_fX, xPad.m_xCentre.m_fZ, 0.0f),
			"the terrain recipe's '%s' pad centre is not inside the dressing "
			"keep-out, so the keep-out is no longer reading the recipe",
			xPad.m_szName);

		// ...and the pad's own rim, which is where a mirrored-but-narrower copy
		// would first disagree with the recipe.
		const float fRimX = xPad.m_xCentre.m_fX + xPad.m_fFlattenRadius - 0.5f;
		ZENITH_ASSERT_TRUE(
			ZM_IsInsideDawnmereKeepOut(fRimX, xPad.m_xCentre.m_fZ, 0.0f),
			"the terrain recipe's '%s' pad is graded out to %.1f m but the "
			"dressing keep-out stops short of its rim",
			xPad.m_szName, xPad.m_fFlattenRadius);
	}

	for (u_int u = 0u; u < xRecipe.m_uPathCount; ++u)
	{
		const ZM_TerrainPathSpec& xPath = xRecipe.m_pxPaths[u];
		for (u_int uPoint = 0u; uPoint < xPath.m_uPointCount; ++uPoint)
		{
			const ZM_TerrainPoint2& xP = xPath.m_pxPoints[uPoint];
			ZENITH_ASSERT_TRUE(
				ZM_IsInsideDawnmereKeepOut(xP.m_fX, xP.m_fZ, 0.0f),
				"the terrain recipe's '%s' path passes through (%.1f, %.1f), which "
				"the dressing keep-out treats as open ground",
				xPath.m_szName, xP.m_fX, xP.m_fZ);
		}
	}
}

// ============================================================================
// THE SOFT KEEP-OUT -- the one collider-free props are bound by
// ============================================================================

// ★★ THE CLAUSE THAT MAKES THE SPLIT SAFE. The soft variant may be narrower than
// the hard one only where "narrower" costs nothing: on the GRADED-BUT-UNPAVED
// ring of a pad or a path. Everywhere a BODY stands or is warped to -- the seven
// blind drive legs, the six NPC anchors, the two patrol waypoints, the four
// arrival markers and the north gate -- the two must agree exactly, because a
// bush growing out of a spawn marker is a player who materialises inside a shrub
// and a bush on a drive leg is at best a lie about where the lane is.
ZENITH_TEST(ZM_Dressing, SoftKeepOut_RelaxesOnlyGradedGroundNeverABodyAnchor)
{
	// (a) Every blind drive leg is refused by BOTH variants, sampled end to end.
	for (u_int u = 0u; u < ZM_GetDawnmereDriveLegCount(); ++u)
	{
		const ZM_DawnmereDriveLeg& xLeg = ZM_GetDawnmereDriveLeg(u);
		const float fLength = DressingPlanarDistance(
			xLeg.m_fAX, xLeg.m_fAZ, xLeg.m_fBX, xLeg.m_fBZ);
		u_int uSteps = (u_int)std::ceil(fLength / fDRESSING_WALK_STEP);
		if (uSteps == 0u)
		{
			uSteps = 1u;
		}
		for (u_int uStep = 0u; uStep <= uSteps; ++uStep)
		{
			const float fT = (float)uStep / (float)uSteps;
			const float fX = xLeg.m_fAX + (xLeg.m_fBX - xLeg.m_fAX) * fT;
			const float fZ = xLeg.m_fAZ + (xLeg.m_fBZ - xLeg.m_fAZ) * fT;
			ZENITH_ASSERT_TRUE(
				ZM_IsInsideDawnmereSoftKeepOut(fX, fZ, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
				"the SOFT keep-out admits (%.2f, %.2f) on the blind drive leg '%s'",
				fX, fZ, xLeg.m_szName);
		}
	}

	// (b) ...and so is every anchor and every arrival marker.
	for (u_int u = 0u; u < ZM_DAWNMERE_NPC_COUNT; ++u)
	{
		const ZM_DawnmereNpcAnchor& xAnchor = ZM_GetDawnmereNpcAnchor(u);
		ZENITH_ASSERT_TRUE(
			ZM_IsInsideDawnmereSoftKeepOut(xAnchor.m_fX, xAnchor.m_fZ,
				fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
			"the SOFT keep-out admits foliage on the authored NPC '%s'",
			xAnchor.m_szEntityName);
	}
	ZENITH_ASSERT_TRUE(
		ZM_IsInsideDawnmereSoftKeepOut(fZM_DAWNMERE_HOME_X,
			fZM_DAWNMERE_FROM_HOME_SPAWN_Z, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
		"the SOFT keep-out admits foliage on the FromHome arrival marker");
	ZENITH_ASSERT_TRUE(
		ZM_IsInsideDawnmereSoftKeepOut(fZM_DAWNMERE_LAB_X,
			fZM_DAWNMERE_FROM_LAB_SPAWN_Z, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
		"the SOFT keep-out admits foliage on the FromLab arrival marker");
	ZENITH_ASSERT_TRUE(
		ZM_IsInsideDawnmereSoftKeepOut(fZM_DAWNMERE_FROM_ROUTE1_X,
			fZM_DAWNMERE_FROM_ROUTE1_Z, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
		"the SOFT keep-out admits foliage on the FromRoute1 arrival marker");
	ZENITH_ASSERT_TRUE(
		ZM_IsInsideDawnmereSoftKeepOut(fZM_DAWNMERE_NORTH_GATE_X,
			fZM_DAWNMERE_NORTH_GATE_Z, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN),
		"the SOFT keep-out admits foliage inside the north seam gate");

	// (c) It is never WIDER than the hard one. If it were, a collider-free prop
	// would be refused ground a boulder is allowed, which is backwards and would
	// mean the two functions had diverged in structure rather than in radius.
	const ZM_TerrainAuthoringRecipe& xRecipe = DressingRecipe();
	for (float fX = 8.0f; fX <= xRecipe.WorldMaxX() - 8.0f; fX += 8.0f)
	{
		for (float fZ = 8.0f; fZ <= xRecipe.WorldMaxZ() - 8.0f; fZ += 8.0f)
		{
			ZENITH_ASSERT_GE(ZM_DawnmereSoftKeepOutClearance(fX, fZ),
				ZM_DawnmereKeepOutClearance(fX, fZ) - 0.001f,
				"the SOFT keep-out is WIDER than the hard one at (%.1f, %.1f), so a "
				"bush is refused ground a boulder is allowed", fX, fZ);
		}
	}

	// (d) TOTALITY, matching the hard variant's contract.
	ZENITH_ASSERT_LT(
		ZM_DawnmereSoftKeepOutClearance(std::numeric_limits<float>::quiet_NaN(), 0.0f),
		0.0f, "the SOFT keep-out admits a non-finite sample");
}

// ★ AND THE CLAUSE THAT SAYS THE SPLIT ACHIEVED ANYTHING. Everything above is
// satisfied by a soft keep-out identical to the hard one -- which is exactly what
// the first version of this dressing had, and why the town square came back from
// its first capture as a bare ring with people standing in it. This walks the
// Plaza pad's graded-but-unpaved annulus and requires that foliage may stand in
// it, so "the two variants are the same function twice" reds here.
ZENITH_TEST(ZM_Dressing, SoftKeepOut_OpensTheGradedVergeTheHardOneRefuses)
{
	const ZM_TerrainAuthoringRecipe& xRecipe = DressingRecipe();
	const ZM_TerrainPadSpec* pxPlaza = nullptr;
	for (u_int u = 0u; u < xRecipe.m_uPadCount; ++u)
	{
		if (std::strcmp(xRecipe.m_pxPads[u].m_szName, "Plaza") == 0)
		{
			pxPlaza = &xRecipe.m_pxPads[u];
			break;
		}
	}
	ZENITH_ASSERT_NOT_NULL(pxPlaza,
		"the Dawnmere recipe no longer carries a pad named 'Plaza'");
	if (pxPlaza == nullptr)
	{
		return;
	}

	// The pad has to HAVE a verge for this to mean anything -- a pad paved to its
	// own flatten radius has no unpaved ring and the clause below would be
	// vacuous.
	ZENITH_ASSERT_LT(pxPlaza->m_fDirtRadius, pxPlaza->m_fFlattenRadius - 4.0f,
		"the Plaza pad is paved (%.1f m) almost to its graded edge (%.1f m), so it "
		"has no verge for the soft keep-out to open and this clause proves nothing",
		pxPlaza->m_fDirtRadius, pxPlaza->m_fFlattenRadius);

	// Walk a ring midway between the paved edge and the graded edge, counting the
	// bearings each variant admits.
	//
	// ★ THE FLOOR IS AN EIGHTH OF THE RING, AND IT HAS BEEN LOWERED ONCE, WHICH IS
	// WORTH BEING EXPLICIT ABOUT. The first draft asked for half and went red at
	// 25/64 on the v7 town; the floor became a quarter. v8's town is SMALLER with
	// the SAME number of things radiating from it -- three authored lanes, seven
	// blind drive legs and six NPC anchors, all crossing a 29 m ring inside a 34 m
	// pad -- so the same split opens 13/64, and the floor is an eighth.
	//
	// ★★ A FLOOR THAT MOVES EVERY TIME IT REDS IS A RATCHET, so this clause no
	// longer rests on the ring alone: the AREA clause below measures the same
	// property over the whole map, where the answer does not depend on how many
	// NPCs happen to stand on one circle. The ring stays because it is specific --
	// it names the Plaza verge, which is the place the split was built for.
	//
	// What both are really asserting is that the split opened ANY ground at all:
	// an unsplit keep-out scores exactly 0 on both, because clause (c) above
	// already proves the soft variant is never wider than the hard one and clause
	// (a) of the previous test proves the hard one closes the whole ring.
	const float fRing =
		(pxPlaza->m_fDirtRadius + pxPlaza->m_fFlattenRadius) * 0.5f;
	u_int uOpenToFoliage = 0u;
	u_int uClosedToColliders = 0u;
	for (u_int u = 0u; u < 64u; ++u)
	{
		const float fAngle = 6.2831853f * (float)u / 64.0f;
		const float fX = pxPlaza->m_xCentre.m_fX + std::cos(fAngle) * fRing;
		const float fZ = pxPlaza->m_xCentre.m_fZ + std::sin(fAngle) * fRing;
		const bool bHard =
			ZM_IsInsideDawnmereKeepOut(fX, fZ, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN);
		const bool bSoft =
			ZM_IsInsideDawnmereSoftKeepOut(fX, fZ, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN);
		if (bHard)
		{
			++uClosedToColliders;
		}
		if (!bSoft)
		{
			++uOpenToFoliage;
		}
	}

	ZENITH_ASSERT_EQ(uClosedToColliders, 64u,
		"only %u of 64 bearings on the Plaza pad's graded verge are closed to "
		"COLLIDING props -- the hard keep-out no longer covers the pad it reads "
		"from the recipe", uClosedToColliders);
	ZENITH_ASSERT_GT(uOpenToFoliage, 8u,
		"only %u of 64 bearings on the Plaza pad's graded verge are open to "
		"collider-free foliage. The soft keep-out is not actually softer there, so "
		"the town square is a bare ring again -- which is the state this split was "
		"written to end. (25 observed at v7, 13 at v8; the floor is an eighth of the "
		"ring, and an unsplit keep-out scores 0.)",
		uOpenToFoliage);

	// ★★ THE SAME PROPERTY, MEASURED OVER THE WHOLE MAP, so the claim does not
	// rest on how many anchors happen to cross one circle. Every sampled point
	// that the HARD keep-out refuses and the SOFT one admits is ground that
	// carries foliage and could not carry a boulder -- which is exactly what the
	// split buys. Observed at 4.2% of the sheet when v8 landed.
	u_int uSampled = 0u;
	u_int uFoliageOnly = 0u;
	for (float fX = 8.0f; fX <= xRecipe.WorldMaxX() - 8.0f; fX += 4.0f)
	{
		for (float fZ = 8.0f; fZ <= xRecipe.WorldMaxZ() - 8.0f; fZ += 4.0f)
		{
			++uSampled;
			const bool bHard =
				ZM_IsInsideDawnmereKeepOut(fX, fZ, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN);
			const bool bSoft =
				ZM_IsInsideDawnmereSoftKeepOut(fX, fZ, fZM_DAWNMERE_PROP_KEEPOUT_MARGIN);
			if (bHard && !bSoft)
			{
				++uFoliageOnly;
			}
		}
	}
	ZENITH_ASSERT_GT(uSampled, 0u, "the foliage-only sweep sampled nothing");
	ZENITH_ASSERT_GT(uFoliageOnly * 100u, uSampled,
		"only %u of %u sampled points (%.2f%%) are ground the hard keep-out refuses "
		"and the soft one admits. Under 1%% of the map, the split is buying nothing "
		"and the two functions may as well be one",
		uFoliageOnly, uSampled, 100.0 * (double)uFoliageOnly / (double)uSampled);
}

// TOTALITY: a non-finite sample must answer a large NEGATIVE clearance, never a
// NaN that a `> margin` test would silently pass. The scatter reads terrain
// heights right after this check, so a NaN admitted here becomes a NaN in a
// serialized transform.
ZENITH_TEST(ZM_Dressing, KeepOut_IsTotalForNonFiniteInput)
{
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();

	const float afProbes[][2] =
	{
		{ fNaN, 0.0f }, { 0.0f, fNaN }, { fNaN, fNaN },
		{ fInf, 0.0f }, { 0.0f, -fInf }, { fInf, fInf },
	};

	for (const auto& afProbe : afProbes)
	{
		const float fClearance = ZM_DawnmereKeepOutClearance(afProbe[0], afProbe[1]);
		ZENITH_ASSERT_TRUE(std::isfinite(fClearance),
			"ZM_DawnmereKeepOutClearance returned a non-finite clearance for a "
			"non-finite sample");
		ZENITH_ASSERT_LT(fClearance, 0.0f,
			"a non-finite sample is reported as OUTSIDE the keep-out, so a NaN "
			"coordinate would be accepted as a legal prop placement");
		ZENITH_ASSERT_TRUE(
			ZM_IsInsideDawnmereKeepOut(afProbe[0], afProbe[1], 0.0f),
			"ZM_IsInsideDawnmereKeepOut admits a non-finite sample");
	}

	// A negative or non-finite MARGIN must not weaken the test either.
	ZENITH_ASSERT_TRUE(
		ZM_IsInsideDawnmereKeepOut(
			fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z, -100.0f),
		"a negative margin is applied literally, so a caller could shrink the "
		"keep-out below the town itself");
	ZENITH_ASSERT_TRUE(
		ZM_IsInsideDawnmereKeepOut(
			fZM_DAWNMERE_TOWN_CENTER_X, fZM_DAWNMERE_TOWN_CENTER_Z, fNaN),
		"a NaN margin is applied literally");
}

// ============================================================================
// THE PROP TABLE
// ============================================================================

ZENITH_TEST(ZM_Dressing, ScatterGroups_AreWellFormedAndDistinct)
{
	const u_int uCount = ZM_GetDawnmereScatterGroupCount();
	ZENITH_ASSERT_GT(uCount, 0u, "Dawnmere declares no scatter groups");

	for (u_int u = 0u; u < uCount; ++u)
	{
		const ZM_DawnmereScatterGroup& xA = ZM_GetDawnmereScatterGroup(u);

		ZENITH_ASSERT_GT((u_int)std::strlen(xA.m_szEntity), 0u,
			"scatter group %u has an empty entity name", u);
		ZENITH_ASSERT_GT((u_int)std::strlen(xA.m_szAssetDir), 0u,
			"scatter group '%s' has an empty asset directory", xA.m_szEntity);
		ZENITH_ASSERT_GT((u_int)std::strlen(xA.m_szMeshBase), 0u,
			"scatter group '%s' names no mesh", xA.m_szEntity);
		ZENITH_ASSERT_GT((u_int)std::strlen(xA.m_szMaterialFile), 0u,
			"scatter group '%s' names no material", xA.m_szEntity);
		ZENITH_ASSERT_GT(xA.m_uCount, 0u,
			"scatter group '%s' requests zero instances", xA.m_szEntity);
		ZENITH_ASSERT_NE(xA.m_uSeed, 0u,
			"scatter group '%s' has a zero seed, which is a fixed point of "
			"xorshift32: every draw would return the same value and every instance "
			"would land on one spot", xA.m_szEntity);
		ZENITH_ASSERT_GT(xA.m_fScaleMin, 0.0f,
			"scatter group '%s' has a non-positive minimum scale", xA.m_szEntity);
		ZENITH_ASSERT_LE(xA.m_fScaleMin, xA.m_fScaleMax,
			"scatter group '%s' has an inverted scale range", xA.m_szEntity);
		ZENITH_ASSERT_GT(xA.m_fSpacing, 0.0f,
			"scatter group '%s' has no minimum spacing, so its rejection sampler "
			"cannot stop a pile of instances on one spot", xA.m_szEntity);
		ZENITH_ASSERT_GT(xA.m_fBoundsRadius, 0.0f,
			"scatter group '%s' has a zero cull-sphere radius, so every instance "
			"culls at every angle", xA.m_szEntity);
		ZENITH_ASSERT_GE(xA.m_fPlazaMin, 0.0f,
			"scatter group '%s' has a negative plaza exclusion", xA.m_szEntity);

		// A tipped row has to declare its LENGTH, or the ground-following pitch
		// reads a zero run and the log is laid dead level into the hillside.
		if (xA.m_fLayDownDeg != 0.0f)
		{
			ZENITH_ASSERT_GT(xA.m_fLengthMetres, 0.0f,
				"scatter group '%s' is tipped over but declares no length, so it "
				"cannot follow the ground along its own axis", xA.m_szEntity);
		}

		// An animated row needs a real clip duration; a static one must not carry
		// a stray one, because the duration SERIALIZES and a static group with a
		// duration is a component configured for an animation it does not have.
		if (xA.m_szVATFile[0] != '\0')
		{
			ZENITH_ASSERT_GT(xA.m_fAnimDuration, 0.0f,
				"scatter group '%s' names a sway VAT but has no clip duration",
				xA.m_szEntity);
		}
		else
		{
			ZENITH_ASSERT_EQ_FLOAT(xA.m_fAnimDuration, 0.0f, 0.0f,
				"scatter group '%s' is static but carries an animation duration",
				xA.m_szEntity);
		}

		// A collider row needs a real capsule; a non-collider row must not carry
		// stray dimensions, for the same serialization reason.
		if (xA.m_bCollider)
		{
			ZENITH_ASSERT_GT(xA.m_fColliderRadius, 0.0f,
				"scatter group '%s' has a collider with no radius", xA.m_szEntity);
		}
		else
		{
			ZENITH_ASSERT_EQ_FLOAT(xA.m_fColliderRadius, 0.0f, 0.0f,
				"scatter group '%s' has no collider but carries a capsule radius",
				xA.m_szEntity);
		}

		for (u_int v = u + 1u; v < uCount; ++v)
		{
			const ZM_DawnmereScatterGroup& xB = ZM_GetDawnmereScatterGroup(v);
			ZENITH_ASSERT_NE(std::strcmp(xA.m_szEntity, xB.m_szEntity), 0,
				"two Dawnmere scatter groups are both named '%s' -- the second "
				"would adopt the first's entity and clear its instances",
				xA.m_szEntity);
			// ★ DISTINCT SEEDS, not merely non-zero. Two groups sharing a seed
			// draw the SAME position stream, so every instance of one lands
			// within a spacing radius of an instance of the other and the two
			// read as one doubled prop rather than as two scattered ones.
			ZENITH_ASSERT_NE(xA.m_uSeed, xB.m_uSeed,
				"Dawnmere scatter groups '%s' and '%s' share the seed 0x%X, so "
				"they draw identical position streams", xA.m_szEntity,
				xB.m_szEntity, xA.m_uSeed);
		}
	}
}

// The dressing has to be big enough to be dressing. A table that requests a
// handful of instances would pass every safety clause above and leave the map as
// empty as the complaint that started this.
ZENITH_TEST(ZM_Dressing, ScatterGroups_RequestAMeaningfulAmountOfScenery)
{
	const u_int uTotal = ZM_GetDawnmereScatterRequestedTotal();
	ZENITH_ASSERT_GE(uTotal, 400u,
		"the Dawnmere scatter table requests only %u instances across %u groups. "
		"Every safety clause in this suite passes on an empty scatter; this is "
		"the one that says the map is actually dressed",
		uTotal, ZM_GetDawnmereScatterGroupCount());

	// ...and every family is represented. Losing one row of the table is a look
	// change nothing else here would notice.
	bool bRock = false;
	bool bDeadwood = false;
	bool bBush = false;
	for (u_int u = 0u; u < ZM_GetDawnmereScatterGroupCount(); ++u)
	{
		const ZM_DawnmereScatterGroup& xGroup = ZM_GetDawnmereScatterGroup(u);
		bRock     = bRock     || std::strcmp(xGroup.m_szAssetDir, "Meshes/Rocks/") == 0;
		bDeadwood = bDeadwood || std::strcmp(xGroup.m_szAssetDir, "Meshes/FallenTrees/") == 0;
		bBush     = bBush     || std::strcmp(xGroup.m_szAssetDir, "Meshes/Bushes/") == 0;
	}
	ZENITH_ASSERT_TRUE(bRock, "the Dawnmere dressing has no rocks");
	ZENITH_ASSERT_TRUE(bDeadwood, "the Dawnmere dressing has no deadwood");
	ZENITH_ASSERT_TRUE(bBush, "the Dawnmere dressing has no bushes");
}

// ============================================================================
// TOTALITY OF THE ACCESSORS -- this file's house style, and the reason every
// table above is reached through a function rather than exported directly.
// ============================================================================
ZENITH_TEST(ZM_Dressing, Accessors_AreTotalForOutOfRangeIndices)
{
	const u_int auBadIndices[] = { 0xFFFFFFFFu, 1000u };

	for (u_int uBad : auBadIndices)
	{
		const ZM_DawnmereTreeClump& xClump = ZM_GetDawnmereTreeClump(uBad);
		ZENITH_ASSERT_EQ_FLOAT(xClump.m_fRadius, 0.0f, 0.0f,
			"an out-of-range tree clump has a non-zero radius, so a caller that "
			"skipped the bound check would paint a copse on the town centre");
		ZENITH_ASSERT_EQ(std::strcmp(xClump.m_szName, "UNKNOWN"), 0,
			"the out-of-range tree clump sentinel is not named UNKNOWN");

		const ZM_DawnmereScatterGroup& xGroup = ZM_GetDawnmereScatterGroup(uBad);
		ZENITH_ASSERT_EQ(xGroup.m_uCount, 0u,
			"an out-of-range scatter group requests instances");
		ZENITH_ASSERT_NOT_NULL(xGroup.m_szAssetDir,
			"the out-of-range scatter sentinel carries a null asset directory, so "
			"a caller that skipped the bound check would build a path out of it");
		ZENITH_ASSERT_NOT_NULL(xGroup.m_szMeshBase,
			"the out-of-range scatter sentinel carries a null mesh name");

		const ZM_DawnmereDriveLeg& xLeg = ZM_GetDawnmereDriveLeg(uBad);
		ZENITH_ASSERT_EQ_FLOAT(xLeg.m_fAX, xLeg.m_fBX, 0.0f,
			"the out-of-range drive-leg sentinel is not degenerate");
		ZENITH_ASSERT_EQ_FLOAT(xLeg.m_fAZ, xLeg.m_fBZ, 0.0f,
			"the out-of-range drive-leg sentinel is not degenerate");
	}
}
