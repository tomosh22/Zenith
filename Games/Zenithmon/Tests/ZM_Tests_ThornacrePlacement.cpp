#include "Zenith.h"

// ============================================================================
// ZM_Tests_ThornacrePlacement (S8 item 2, R1-1) -- the boot units for the
// Thornacre TRAVERSAL STUB: the arrival anchor reconciled against the terrain
// recipe, the return edge resolved out of the compiled world table, the ruling-2
// tripwire that says this town has NO GYM DOOR yet, and the settled camera pose
// the arriving player is filmed from.
//
// PURE, and it mirrors Tests/ZM_Tests_ProfLabPlacement.cpp deliberately: no
// scene, no entity, no physics, no assets, no graphics, no g_xEngine, no file
// I/O. Everything here reads COMPILED TABLES -- the placement header, the
// terrain recipe registry, the world table, and the scene-registration table.
//
// ★ AND IT NEVER TOUCHES THE LIVE SCENE REGISTRY. Three units in
// Tests/ZM_Tests_WorldTraversal.cpp call
// g_xEngine.Scenes().RegisterSceneBuildIndex(2, "<fake path>") at boot, and
// RegisterSceneBuildIndex ASSERTS on a re-registration with a different path.
// The registry claims below go through ZM_IsSceneRegisteredForLoad, which reads
// the COMPILED table in Source/World/ZM_SceneRegistry.h and nothing else.
//
// ★ WHAT THESE UNITS CANNOT DO, STATED UP FRONT. They read constants, so they
// can say the placement header, the terrain recipe and the world table agree
// with one another. They cannot say one byte about a Thornacre.zscen -- there
// is no such file yet, which is the whole point of R1-1: the registration and
// the anchors land BEFORE the scene, so the slice that authors it cannot ship
// the trigger without the registration. The expensive halves (a raycast ground
// oracle, a committed-bytes needle proving the absence of a gym door) belong to
// R1-3. Do not read this file's greenness as "Thornacre works".
//
// ★ NOTHING HERE RE-SPELLS A CONSTANT FROM THE HEADER IT TESTS. Every clause
// either compares TWO INDEPENDENTLY AUTHORED TABLES (the placement anchors
// against the terrain recipe's landmarks and pads; the return resolver against
// the world table's connection list), or feeds a known-bad value through the
// IDENTICAL predicate and requires it to fail. The only bare literals are the
// five-name COUNT, the landmark/pad NAMES, and the two component-type-table
// COUNTS -- all of them deliberate tripwires for silent growth, spelled here so
// the edit that grows them has to red something.
// ============================================================================

#include <cmath>     // sqrt / fabs -- the XZ distances; cos / sin -- U15's camera pose
#include <cstring>   // strcmp / strstr -- the name-identity and needle-swallow claims

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"   // IsWarpDestinationValid -- the SHIPPED predicate, run not re-derived
#include "Zenithmon/Components/ZM_SpawnPoint.h"         // IsTagValid / uTAG_CAPACITY -- the tag contract
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"        // THE body contract -- by name, never by literal
#include "Zenithmon/Source/World/ZM_SceneRegistry.h"    // the COMPILED registration table (never the live registry)
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h" // the recipe -- units may include it; the placement header may not
#include "Zenithmon/Source/World/ZM_ThornacrePlacement.h"

namespace
{
	// "These two floats are the same authored number."
	constexpr float fTH_EXACT_EPSILON = 1.0e-4f;

	// "These two anchors are the same authored POINT." Looser than the equality
	// epsilon because an anchor is compared against a table that states it to a
	// different precision, and one of the two is a sum of a centre and a half
	// extent (one more rounding than a directly authored value).
	constexpr float fTH_PLANE_EPSILON = 1.0e-3f;

	// The ZM-D-184 clearance gap is a single half-extent added to a single
	// half-extent, so it is exact to well under this.
	constexpr float fTH_CLEARANCE_EPSILON = 1.0e-5f;

	// How far into a flattened pad an anchor must sit before "the bake really
	// does drive the height here" is a claim rather than a hope. Three quarters
	// of the flatten radius leaves a quarter of margin for the dab falloff at
	// the disc's rim, and it is the same fraction the Route 1 anchors are held
	// to. This is a MARGIN on the pad CENTRE, not a box containment: the flatten
	// dab is driven from the centre outward.
	constexpr float fTH_PAD_CONTAINMENT_FRACTION = 0.75f;

	// ---- The names the recipe and the world table are looked up by ----------
	//
	// Spelled here rather than in the placement header, deliberately, and in
	// opposite directions. The LANDMARK name is also a spawn TAG, and the header
	// refuses to spell a tag (it resolves them); the test is the right side of
	// that seam to state the expectation on. The PAD name is recipe vocabulary
	// the header has no business knowing at all.
	constexpr const char* szTH_ARRIVAL_LANDMARK_NAME = "FromRoute1";
	constexpr const char* szTH_ARRIVAL_PAD_NAME      = "RouteGate";

	// The anti-vacuity probe for the pad-containment clause: a REAL landmark in
	// the SAME recipe that must FAIL the identical predicate. Thornacre's town
	// centre is 444 m up the main lane from the route gate, so a containment
	// check that had degenerated into "always true" reds here.
	constexpr const char* szTH_OUTSIDE_PAD_LANDMARK_NAME = "TownCenter";

	// ---- Component TYPE names, spelled locally -----------------------------
	//
	// ★ WHY THEY ARE HERE AT ALL. A committed .zscen carries entity names,
	// component TYPE names, spawn tags and asset paths in one byte range, and
	// ZM_Tests_CommittedSceneBytes.cpp's needles search BARE NAME BYTES with no
	// NUL. An entity name that is a substring of a serialized type name is
	// SWALLOWED -- the needle counts the type name too and the count is a lie.
	//
	// Game set: Zenithmon.cpp's ZENITH_REGISTER_COMPONENT block (15).
	// Engine set: Zenith_ComponentMeta_Registration.cpp (16) PLUS "AIAgent",
	// which is registered at order 90 through Zenith_AI_RegisterComponents and
	// is therefore easy to miss by reading that one function (17).
	const char* const s_aszGameComponentTypeNames[] =
	{
		"ZM_Game", "ZM_TerrainGrass", "ZM_PlayerController", "ZM_FollowCamera",
		"ZM_GameStateManager", "ZM_SpawnPoint", "ZM_WarpTrigger",
		"ZM_GreyboxVisual", "ZM_BattleArena", "ZM_TallGrassSystem",
		"ZM_BattleTransition", "ZM_BattleDirector", "ZM_UI_MenuStack",
		"ZM_Interactable", "ZM_TouchLayoutController",
	};

	const char* const s_aszEngineComponentTypeNames[] =
	{
		"Transform", "Model", "Tween", "Animator", "Camera", "Light", "Sun",
		"Atmosphere", "Terrain", "Collider", "Graph", "UI", "InstancedMesh",
		"ParticleEmitter", "Attachment", "NavMesh", "AIAgent",
	};

	// ---- Small shared predicates -------------------------------------------

	float THDistanceXZ(float fAX, float fAZ, float fBX, float fBZ)
	{
		const float fDX = fAX - fBX;
		const float fDZ = fAZ - fBZ;
		return std::sqrt(fDX * fDX + fDZ * fDZ);
	}

	// Every printable, space-free character is a legal scene entity name byte.
	// These names are the KEYS FindEntityByName matches on, so a stray space or
	// control byte is a silent miss rather than a diagnosable failure.
	bool THNameIsALookupKey(const char* szName)
	{
		if (szName == nullptr || szName[0] == '\0')
		{
			return false;
		}
		for (u_int uIndex = 0u; szName[uIndex] != '\0'; ++uIndex)
		{
			const unsigned char uCharacter =
				static_cast<unsigned char>(szName[uIndex]);
			if (uCharacter <= 0x20u || uCharacter > 0x7eu)
			{
				return false;
			}
		}
		return true;
	}

	const ZM_TerrainLandmarkSpec* THFindLandmark(
		const ZM_TerrainAuthoringRecipe& xRecipe, const char* szName)
	{
		if (xRecipe.m_pxLandmarks == nullptr || szName == nullptr)
		{
			return nullptr;
		}
		for (u_int u = 0u; u < xRecipe.m_uLandmarkCount; ++u)
		{
			if (xRecipe.m_pxLandmarks[u].m_szName != nullptr
				&& std::strcmp(xRecipe.m_pxLandmarks[u].m_szName, szName) == 0)
			{
				return &xRecipe.m_pxLandmarks[u];
			}
		}
		return nullptr;
	}

	const ZM_TerrainPadSpec* THFindPad(
		const ZM_TerrainAuthoringRecipe& xRecipe, const char* szName)
	{
		if (xRecipe.m_pxPads == nullptr || szName == nullptr)
		{
			return nullptr;
		}
		for (u_int u = 0u; u < xRecipe.m_uPadCount; ++u)
		{
			if (xRecipe.m_pxPads[u].m_szName != nullptr
				&& std::strcmp(xRecipe.m_pxPads[u].m_szName, szName) == 0)
			{
				return &xRecipe.m_pxPads[u];
			}
		}
		return nullptr;
	}

	// THE containment predicate, spelled ONCE so the real anchors and the
	// anti-vacuity probes run through identical arithmetic. A second spelling
	// for the probe would prove only that two copies of a formula agree.
	float THDistanceToPadCentre(const ZM_TerrainPadSpec& xPad, float fX, float fZ)
	{
		return THDistanceXZ(fX, fZ, xPad.m_xCentre.m_fX, xPad.m_xCentre.m_fZ);
	}

	// THE arrival-clearance predicate, likewise spelled once. Stated on the
	// gate's NEAR FACE rather than on its centre -- Q-2026-08-15-001: an arriving
	// player walks off the marker immediately and the eight-way MOVE drive makes
	// the natural walk a 45-degree diagonal, which spends depth as fast as width,
	// so a centre-to-centre figure flatters the geometry.
	float THGateNearFaceClearance(const ZM_ThornacreVolume& xGate, float fMarkerZ)
	{
		return fMarkerZ - xGate.Max().z;
	}

	bool THRowOffersTag(const ZM_WorldSpec& xRow, const char* szTag)
	{
		if (xRow.m_pszSpawnTags == nullptr || szTag == nullptr)
		{
			return false;
		}
		for (u_int u = 0u; u < xRow.m_uSpawnTagCount; ++u)
		{
			if (xRow.m_pszSpawnTags[u] != nullptr
				&& std::strcmp(xRow.m_pszSpawnTags[u], szTag) == 0)
			{
				return true;
			}
		}
		return false;
	}

	// ---- The camera ---------------------------------------------------------

	// The settled camera pose recomputed for an ARBITRARY pitch, so U15 can drive a
	// SIGN-FLIPPED pitch through the identical arithmetic. It is asserted to agree
	// with the SHIPPED accessor at the shipped pitch BEFORE the flipped value is
	// trusted -- otherwise this would be a second implementation checking only
	// itself, which proves nothing about ZM_GetThornacreSettledCameraPosition.
	//
	// It mirrors Route1CameraPositionForPitch in the sibling suite line for line;
	// the two arrivals are the same shape and the two probes must be too.
	Zenith_Maths::Vector3 THCameraPositionForPitch(float fPitch)
	{
		Zenith_Maths::Vector3 xPivot = ZM_GetThornacreSouthArrivalBodyCentre();
		xPivot.y += fZM_THORNACRE_CAMERA_PIVOT_HEIGHT;

		const float fCosPitch = std::cos(fPitch);
		const Zenith_Maths::Vector3 xForward(
			std::sin(fZM_THORNACRE_CAMERA_YAW) * fCosPitch,
			std::sin(fPitch),
			std::cos(fZM_THORNACRE_CAMERA_YAW) * fCosPitch);

		return xPivot - xForward * fZM_THORNACRE_CAMERA_ARM;
	}

	// ★ THE GYM EDGE HAS NO ACCESSOR IN THE PLACEMENT HEADER, BY RULING, SO THE
	// WALK LIVES HERE. A ZM_GetThornacreGymConnection() in a header whose whole
	// point is that the gym does not exist yet would be a gym-named surface in
	// exactly the file the ruling forbids one in. The unit that documents the
	// deliberate asymmetry is the only reader, so this is its right home.
	const ZM_SceneConnection* THFindThornacreGymConnection()
	{
		const ZM_WorldSpec& xRow = ZM_GetWorldSpec(ZM_SCENE_THORNACRE);
		if (xRow.m_pxConnections == nullptr)
		{
			return nullptr;
		}
		for (u_int u = 0u; u < xRow.m_uConnectionCount; ++u)
		{
			if (xRow.m_pxConnections[u].m_eTarget == ZM_SCENE_GYM1)
			{
				return &xRow.m_pxConnections[u];
			}
		}
		return nullptr;
	}
}

// ============================================================================
// U12 -- the arrival anchor sits on the recipe landmark, inside the pad the
// terrain actually flattened.
//
// ★ WHY THIS IS THE FIRST UNIT IN THE FILE. fZM_THORNACRE_RECIPE_TARGET_GROUND_Y
// is NOT A MEASUREMENT and must never be given one: it MIRRORS the recipe's
// m_fTargetHeight, the value every FLATTEN dab drives its pads TO. The town's
// MEASURED surface is the separate table axZM_THORNACRE_GROUND_SAMPLES, whose two
// rows are still SEEDED from this same mirror because no ground-truth oracle has
// run against a Thornacre.zscen yet -- which is exactly why no compiled-constant
// unit in this game can tell that table apart from a frozen one, and why this unit
// reconciles the MIRROR against the recipe rather than reaching for the table.
// The mirror is only honest as a GROUND height while the anchor lies inside a
// flattened disc -- outside one, the bake leaves procedural terrain at some other
// height and the arriving player either floats above the ground or spawns inside
// it, with every compiled constant looking perfectly correct.
//
// Two INDEPENDENTLY AUTHORED tables are compared here: the placement header's
// anchors and the terrain recipe's landmarks and pads. Neither reads the other,
// which is what makes this a check rather than a re-spelling.
// ============================================================================
ZENITH_TEST(ZM_WorldTraversal, Thornacre_ArrivalSitsOnTheRecipeLandmarkTheTerrainFlattened)
{
	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetThornacreTerrainRecipe();

	// (0) ANTI-VACUITY. Every lookup below returns nullptr on an empty table, and
	//     a file that reported "landmark not found" for all of them would at
	//     least be diagnosable -- but the pad and landmark counts are the honest
	//     place to say "the recipe still has content".
	ZENITH_ASSERT_GT(xRecipe.m_uLandmarkCount, 0u,
		"the Thornacre recipe carries no landmarks at all, so every anchor "
		"reconciliation below is a lookup against an empty table");
	ZENITH_ASSERT_GT(xRecipe.m_uPadCount, 0u,
		"the Thornacre recipe carries no pads at all -- nothing is flattened, and "
		"fZM_THORNACRE_RECIPE_TARGET_GROUND_Y (%.4f) is then a height no square "
		"metre of this town actually has",
		(double)fZM_THORNACRE_RECIPE_TARGET_GROUND_Y);

	// (1) THE RECIPE MIRROR IS STILL THE RECIPE'S TARGET HEIGHT. This is the whole
	//     licence for that constant: re-level the town and the mirror is instantly
	//     a lie, so the two have to be reconciled rather than both merely present.
	//
	//     ★ THE SUBJECT IS THE MIRROR, NOT axZM_THORNACRE_GROUND_SAMPLES. This is a
	//     recipe-vs-recipe reconciliation, exactly as the header's own comment on
	//     the mirror says; pointing it at the measured table would make it a
	//     recipe-vs-raycast comparison that reds for a reason this message does not
	//     name.
	ZENITH_ASSERT_EQ_FLOAT(fZM_THORNACRE_RECIPE_TARGET_GROUND_Y,
		xRecipe.m_fTargetHeight, fTH_EXACT_EPSILON,
		"the placement header's recipe-target ground MIRROR is y=%.4f but the "
		"terrain recipe flattens to %.4f -- the mirror is what seeds both rows of "
		"axZM_THORNACRE_GROUND_SAMPLES, so every marker, gate and camera clearance "
		"in ZM_ThornacrePlacement.h is measured against the wrong surface and the "
		"arriving player floats or spawns inside the terrain. Fix the mirror to "
		"match the recipe -- never give it a measured value",
		(double)fZM_THORNACRE_RECIPE_TARGET_GROUND_Y,
		(double)xRecipe.m_fTargetHeight);

	// (2) THE ANCHOR IS THE LANDMARK. The recipe names its route-arrival landmark
	//     after the inbound spawn tag; the placement header refuses to spell that
	//     tag (it is the world table's property), so the two meet here.
	const ZM_TerrainLandmarkSpec* pxLandmark =
		THFindLandmark(xRecipe, szTH_ARRIVAL_LANDMARK_NAME);
	ZENITH_ASSERT_NOT_NULL(pxLandmark,
		"the Thornacre recipe has no landmark named '%s' (%u landmarks). The "
		"arrival anchor in ZM_ThornacrePlacement.h is then a pair of numbers "
		"nothing in the terrain agrees with",
		szTH_ARRIVAL_LANDMARK_NAME, xRecipe.m_uLandmarkCount);

	const ZM_TerrainPadSpec* pxPad = THFindPad(xRecipe, szTH_ARRIVAL_PAD_NAME);
	ZENITH_ASSERT_NOT_NULL(pxPad,
		"the Thornacre recipe has no pad named '%s' (%u pads), so nothing "
		"flattens the ground the route arrival stands on",
		szTH_ARRIVAL_PAD_NAME, xRecipe.m_uPadCount);

	if (pxLandmark == nullptr || pxPad == nullptr)
	{
		return;   // already FAILED; every clause below would be about a null row
	}

	const Zenith_Maths::Vector3 xFeet = ZM_GetThornacreSouthArrivalFeet();

	ZENITH_ASSERT_EQ_FLOAT(xFeet.x, pxLandmark->m_xPosition.m_fX, fTH_PLANE_EPSILON,
		"the Thornacre arrival marker stands at x=%.4f but the recipe's '%s' "
		"landmark is at x=%.4f -- the marker and the terrain feature it was "
		"placed for have drifted apart",
		(double)xFeet.x, szTH_ARRIVAL_LANDMARK_NAME,
		(double)pxLandmark->m_xPosition.m_fX);
	ZENITH_ASSERT_EQ_FLOAT(xFeet.z, pxLandmark->m_xPosition.m_fZ, fTH_PLANE_EPSILON,
		"the Thornacre arrival marker stands at z=%.4f but the recipe's '%s' "
		"landmark is at z=%.4f",
		(double)xFeet.z, szTH_ARRIVAL_LANDMARK_NAME,
		(double)pxLandmark->m_xPosition.m_fZ);

	// (3) ...AND THE LANDMARK ITSELF SITS ON THE FLATTENED HEIGHT. The recipe
	//     states a Y for each landmark; if it disagreed with the target height,
	//     clause (1) would be reconciling the header against a number the recipe
	//     does not use at this spot.
	ZENITH_ASSERT_EQ_FLOAT(pxLandmark->m_xPosition.m_fY, xRecipe.m_fTargetHeight,
		fTH_EXACT_EPSILON,
		"the recipe's '%s' landmark states y=%.4f while the recipe flattens to "
		"%.4f -- the two halves of the recipe disagree about the ground under the "
		"arrival", szTH_ARRIVAL_LANDMARK_NAME,
		(double)pxLandmark->m_xPosition.m_fY, (double)xRecipe.m_fTargetHeight);

	// (4) ★ THE CLAUSE THAT LICENCES THE FLATTEN TARGET AS A GROUND HEIGHT AT ALL.
	//     Inside the pad's flattened disc, with a quarter of the radius to spare.
	//     Its subject is the FLATTEN TARGET the dab drives to -- i.e. the mirror --
	//     because "is this anchor somewhere a dab reached?" is a question about the
	//     recipe, not about what a raycast later measured there.
	const float fPadRadiusLimit =
		fTH_PAD_CONTAINMENT_FRACTION * pxPad->m_fFlattenRadius;
	const float fAnchorToPad = THDistanceToPadCentre(*pxPad, xFeet.x, xFeet.z);
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[ZM_ThornacrePlacement] arrival (%.2f, %.2f) sits %.3f m from the '%s' "
		"pad centre (%.2f, %.2f), limit %.3f m of a %.2f m flatten radius; ground "
		"plane %.3f",
		(double)xFeet.x, (double)xFeet.z, (double)fAnchorToPad,
		szTH_ARRIVAL_PAD_NAME, (double)pxPad->m_xCentre.m_fX,
		(double)pxPad->m_xCentre.m_fZ, (double)fPadRadiusLimit,
		(double)pxPad->m_fFlattenRadius,
		(double)fZM_THORNACRE_RECIPE_TARGET_GROUND_Y);
	ZENITH_ASSERT_LE(fAnchorToPad, fPadRadiusLimit,
		"the Thornacre arrival marker sits %.3f m from the '%s' pad centre, "
		"outside the %.3f m the flatten dab can be trusted for. The ground there "
		"is whatever the procedural pass left, so the flatten target "
		"fZM_THORNACRE_RECIPE_TARGET_GROUND_Y (%.4f) -- which is what seeds this "
		"anchor's row of axZM_THORNACRE_GROUND_SAMPLES -- is a guess, and the "
		"arriving player floats or spawns inside terrain",
		(double)fAnchorToPad, szTH_ARRIVAL_PAD_NAME, (double)fPadRadiusLimit,
		(double)fZM_THORNACRE_RECIPE_TARGET_GROUND_Y);

	// ★ ANTI-VACUITY, THROUGH THE IDENTICAL PREDICATE. A real landmark in the
	//   same recipe, 400-odd metres up the main lane, must FAIL the clause above.
	//   Without this arm a containment check that had degenerated (a pad radius
	//   read as a diameter, a distance that lost its Z term) would keep passing.
	const ZM_TerrainLandmarkSpec* pxOutside =
		THFindLandmark(xRecipe, szTH_OUTSIDE_PAD_LANDMARK_NAME);
	ZENITH_ASSERT_NOT_NULL(pxOutside,
		"the Thornacre recipe has no '%s' landmark, so the anti-vacuity arm of "
		"the pad-containment clause has nothing to probe with -- pick another "
		"landmark well away from the '%s' pad rather than deleting the arm",
		szTH_OUTSIDE_PAD_LANDMARK_NAME, szTH_ARRIVAL_PAD_NAME);
	if (pxOutside != nullptr)
	{
		const float fProbeToPad = THDistanceToPadCentre(*pxPad,
			pxOutside->m_xPosition.m_fX, pxOutside->m_xPosition.m_fZ);
		ZENITH_ASSERT_GT(fProbeToPad, fPadRadiusLimit,
			"the '%s' landmark is %.3f m from the '%s' pad centre, INSIDE the "
			"%.3f m containment limit -- the clause above cannot distinguish an "
			"anchor on the flattened gate from one in the middle of town, so it "
			"is passing vacuously", szTH_OUTSIDE_PAD_LANDMARK_NAME,
			(double)fProbeToPad, szTH_ARRIVAL_PAD_NAME, (double)fPadRadiusLimit);
	}

	// (5) THE ANCHOR IS INSIDE THE EXPORTED WORLD. An anchor past the recipe's
	//     bounds has no baked chunk under it at all -- not merely an unflattened
	//     one -- and therefore no physics body either.
	ZENITH_ASSERT_GE(xFeet.x, xRecipe.m_fWorldMinX,
		"the Thornacre arrival is at x=%.3f, west of the recipe's exported world "
		"(%.3f)", (double)xFeet.x, (double)xRecipe.m_fWorldMinX);
	ZENITH_ASSERT_LE(xFeet.x, xRecipe.m_fWorldMaxX,
		"the Thornacre arrival is at x=%.3f, east of the recipe's exported world "
		"(%.3f)", (double)xFeet.x, (double)xRecipe.m_fWorldMaxX);
	ZENITH_ASSERT_GE(xFeet.z, xRecipe.m_fWorldMinZ,
		"the Thornacre arrival is at z=%.3f, south of the recipe's exported world "
		"(%.3f)", (double)xFeet.z, (double)xRecipe.m_fWorldMinZ);
	ZENITH_ASSERT_LE(xFeet.z, xRecipe.m_fWorldMaxZ,
		"the Thornacre arrival is at z=%.3f, north of the recipe's exported world "
		"(%.3f)", (double)xFeet.z, (double)xRecipe.m_fWorldMaxZ);

	// (6) THE LANDMARK'S NAME IS A TAG THIS TOWN ACTUALLY OFFERS. The recipe names
	//     its route arrival after the inbound spawn tag; the world table decides
	//     what that tag is. Rename either and the marker the warp machine looks
	//     for stops matching the landmark the terrain flattened -- which stalls the
	//     warp in ZM_WARP_TRANSITION_WAITING_FOR_SPAWN for that barrier's whole
	//     frame budget before it errors out (ZM-D-200).
	ZENITH_ASSERT_TRUE(
		THRowOffersTag(ZM_GetWorldSpec(ZM_SCENE_THORNACRE), szTH_ARRIVAL_LANDMARK_NAME),
		"the Thornacre recipe's arrival landmark is named '%s' but the compiled "
		"ZM_SCENE_THORNACRE row does not offer a spawn tag by that name (%u tags) "
		"-- the terrain and the world table disagree about which door this is",
		szTH_ARRIVAL_LANDMARK_NAME,
		ZM_GetWorldSpec(ZM_SCENE_THORNACRE).m_uSpawnTagCount);

	// (7) ★ ZM-D-184: THE AUTHORED PLAYER STARTS EXACTLY ONE HALF-EXTENT CLEAR.
	//     Not zero -- a dynamic body authored at exact contact bursts physics
	//     substeps on its first frame and falls THROUGH the terrain (that is how
	//     Vesper vanished). Not two -- a body dropped from a body-height up is a
	//     visible fall on arrival. The gap is asserted as a DIFFERENCE of the two
	//     accessors, so it follows any change to the ground plane.
	const float fClearanceGap = ZM_GetThornacreAuthoredPlayerCentre().y
		- ZM_GetThornacreSouthArrivalBodyCentre().y;
	ZENITH_ASSERT_EQ_FLOAT(fClearanceGap, fZM_HUMAN_BODY_HALF_HEIGHT,
		fTH_CLEARANCE_EPSILON,
		"the authored Thornacre player starts %.5f m above its resting body "
		"centre; the ZM-D-184 rule is exactly one body half-extent (%.5f). Zero "
		"is the substep-burst fall-through that lost Vesper; more is a visible "
		"drop the moment the town loads",
		(double)fClearanceGap, (double)fZM_HUMAN_BODY_HALF_HEIGHT);
}

// ============================================================================
// U13 -- the return gate is the TABLE's edge, and it does not fire on arrival.
//
// ★ THE MUTATION THIS EXISTS FOR. Thornacre's stub authors ONE way out. If its
// resolver quietly returned uZM_THORNACRE_RETURN_TARGET_UNRESOLVED, R1-3 would
// author a ZM_WarpTrigger pointing at build index 0xFFFFFFFF; if it resolved the
// GYM edge instead (m_pxConnections[0] is a magic index away), the return door
// would lead to a scene with no file. Both are accepted by
// ZM_GameStateManager::IsWarpDestinationValid, which consults ONLY the compiled
// table -- never the registry, never the destination scene -- so the failure is
// not a rejection but ZM_WARP_TRANSITION_WAITING_FOR_SCENE: a black screen behind
// an opaque fade with the player frozen for that barrier's whole frame budget,
// then a Zenith_Error naming the build index it waited on (ZM-D-200).
//
// ★ AND THE SECOND HALF IS GEOMETRIC. A gate whose near face overlaps the
// arriving body fires on the first contact tick and warps the player straight
// back, whose own gate returns them here -- an infinite ping-pong between two
// scenes with no input accepted in between, and every table clause above green.
// ============================================================================
ZENITH_TEST(ZM_WorldTraversal, Thornacre_ReturnGateTargetsRoute1ByTheCompiledConnectionWithArrivalClearance)
{
	// (1) The edge exists at all.
	const ZM_SceneConnection* pxEdge = ZM_GetThornacreReturnConnection();
	ZENITH_ASSERT_NOT_NULL(pxEdge,
		"the compiled ZM_SCENE_THORNACRE row carries no connection targeting "
		"ZM_SCENE_ROUTE1, so ZM_GetThornacreReturnConnection() resolved to "
		"nullptr. The authored return trigger would take the "
		"uZM_THORNACRE_RETURN_TARGET_UNRESOLVED sentinel and this town would be a "
		"dead end -- fix Source/Data/ZM_WorldSpec.cpp, not this test");
	if (pxEdge == nullptr)
	{
		return;   // already FAILED; every clause below would be about a null edge
	}

	// (2) ...and it is the ROUTE edge, not the gym one. Thornacre carries TWO
	//     connections, so a resolver that had degenerated into m_pxConnections[0]
	//     would read perfectly today and hand back the gym the day the table is
	//     reordered.
	ZENITH_ASSERT_EQ((u_int)pxEdge->m_eTarget, (u_int)ZM_SCENE_ROUTE1,
		"ZM_GetThornacreReturnConnection() resolved the edge targeting scene id "
		"%u; the return door must target ZM_SCENE_ROUTE1 (%u). The gym edge is "
		"deliberately unbacked in this milestone and must never be what the "
		"return trigger authors",
		(u_int)pxEdge->m_eTarget, (u_int)ZM_SCENE_ROUTE1);
	ZENITH_ASSERT_EQ((u_int)ZM_GetThornacreReturnTargetScene(),
		(u_int)ZM_SCENE_ROUTE1,
		"ZM_GetThornacreReturnTargetScene() answers scene id %u rather than "
		"ZM_SCENE_ROUTE1 (%u) -- the two resolvers have parted company",
		(u_int)ZM_GetThornacreReturnTargetScene(), (u_int)ZM_SCENE_ROUTE1);

	// (3) The build index is the TABLE's Route 1 index and never the sentinel.
	//     Spelled as ZM_GetWorldSpec(...).m_uBuildIndex on the expected side too:
	//     a literal 20 here would be exactly the second inventory the placement
	//     header's opening star forbids.
	const u_int uResolved = ZM_GetThornacreReturnTargetBuildIndex();
	ZENITH_ASSERT_NE(uResolved, uZM_THORNACRE_RETURN_TARGET_UNRESOLVED,
		"ZM_GetThornacreReturnTargetBuildIndex() returned its unresolved sentinel "
		"even though the Thornacre->Route1 edge exists -- the resolver and the "
		"edge walk have parted company, and R1-3 would author a warp to a build "
		"index no scene can hold");
	ZENITH_ASSERT_EQ(uResolved, ZM_GetWorldSpec(ZM_SCENE_ROUTE1).m_uBuildIndex,
		"the Thornacre return resolves to build index %u but the compiled world "
		"table puts Route 1 at %u -- the trigger would fade the player out and "
		"load whatever is registered at %u, or nothing at all",
		uResolved, ZM_GetWorldSpec(ZM_SCENE_ROUTE1).m_uBuildIndex, uResolved);

	// (4) The tag is one ZM_SpawnPoint will actually accept. SetTag fails CLOSED,
	//     so a tag the component rejects authors an UNTAGGED marker on the far
	//     side that resolves to nothing.
	const char* szTag = ZM_GetThornacreReturnSpawnTag();
	ZENITH_ASSERT_TRUE(szTag != nullptr && szTag[0] != '\0',
		"the Thornacre->Route1 connection carries an empty spawn tag, so the "
		"return trigger has nothing to ask Route 1 for");
	ZENITH_ASSERT_TRUE(ZM_SpawnPoint::IsTagValid(szTag),
		"the Thornacre->Route1 spawn tag '%s' is not a tag ZM_SpawnPoint::SetTag "
		"will accept (capacity %u). SetTag fails CLOSED, so Route 1's arrival "
		"marker would author itself untagged and the return would stall the warp "
		"machine in WAITING_FOR_SPAWN for its whole frame budget before erroring "
		"out (ZM-D-200)",
		szTag, ZM_SpawnPoint::uTAG_CAPACITY);

	// (5) ★ THE RECIPROCAL-INVENTORY CLAUSE. Route 1 must OFFER the tag the
	//     return ASKS for -- and this is the exact predicate
	//     IsWarpDestinationValid evaluates, run here against the same table it
	//     reads. Failing here means the warp would be REJECTED at runtime (a
	//     visible no-op) rather than accepted into a barrier wait that burns its
	//     whole frame budget before it says anything; both are defects, but that
	//     one at least fails immediately.
	const ZM_WorldSpec& xRoute1 = ZM_GetWorldSpec(ZM_SCENE_ROUTE1);
	ZENITH_ASSERT_TRUE(THRowOffersTag(xRoute1, szTag),
		"the Thornacre return asks Route 1 for spawn tag '%s', which is NOT in "
		"Route 1's offered tag list (%u tags)", szTag, xRoute1.m_uSpawnTagCount);
	ZENITH_ASSERT_TRUE(
		ZM_GameStateManager::IsWarpDestinationValid(uResolved, szTag),
		"ZM_GameStateManager::IsWarpDestinationValid(%u, \"%s\") rejects the "
		"Thornacre return door. The shipped predicate is being run here, not "
		"re-derived, so the door would be a silent no-op in game",
		uResolved, szTag);

	// (6) ★ AND THE HALF IsWarpDestinationValid STRUCTURALLY CANNOT SEE. It never
	//     consults the scene registry, so an ACCEPTED warp at an unregistered
	//     index burns the whole WAITING_FOR_SCENE budget. R1-1 exists to make that
	//     unreachable: Route 1's registration row lands before any trigger points
	//     at it. This clause reads the COMPILED table, never the live registry.
	ZENITH_ASSERT_TRUE(ZM_IsSceneRegisteredForLoad(ZM_SCENE_ROUTE1),
		"Route 1 has no row in the compiled scene-registration table, so the "
		"Thornacre return would be ACCEPTED by IsWarpDestinationValid and then "
		"stall in ZM_WARP_TRANSITION_WAITING_FOR_SCENE for that barrier's whole "
		"frame budget -- a black screen behind an opaque fade that ends in a "
		"Zenith_Error, not a crash and not a red test. Restore the row "
		"in Source/World/ZM_SceneRegistry.h");

	// ---- The geometry -------------------------------------------------------

	const ZM_ThornacreVolume xGate = ZM_GetThornacreSouthGate();

	// (7) The gate is BEYOND the marker, on the way out of town. A gate on the
	//     wrong side of its own marker is the ping-pong described in the banner.
	ZENITH_ASSERT_LT(xGate.m_xCenter.z, fZM_THORNACRE_SOUTH_ARRIVAL_Z,
		"the Thornacre return gate is centred at z=%.3f, NORTH of the arrival "
		"marker at z=%.3f -- it stands between the player and the town instead of "
		"between the player and the route",
		(double)xGate.m_xCenter.z, (double)fZM_THORNACRE_SOUTH_ARRIVAL_Z);

	// (8) ★ THE CLAUSE THAT KEEPS THE DOOR FROM BEING AN INFINITE LOOP. Measured
	//     on the NEAR FACE against the ARRIVING BODY CENTRE, so it survives a
	//     change to either the box depth or the marker.
	const float fClearance = THGateNearFaceClearance(xGate,
		ZM_GetThornacreSouthArrivalBodyCentre().z);
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[ZM_ThornacrePlacement] return gate centre z=%.3f, near face z=%.3f, "
		"arrival z=%.3f: %.3f m of clearance against a %.3f m floor",
		(double)xGate.m_xCenter.z, (double)xGate.Max().z,
		(double)ZM_GetThornacreSouthArrivalBodyCentre().z, (double)fClearance,
		(double)fZM_THORNACRE_GATE_ARRIVAL_CLEARANCE_MIN);
	ZENITH_ASSERT_GE(fClearance, fZM_THORNACRE_GATE_ARRIVAL_CLEARANCE_MIN,
		"the Thornacre return gate's near face clears the arriving body by only "
		"%.3f m, under the %.3f m floor. A player warping in would touch the "
		"sensor on an early tick and be sent straight back to Route 1, whose own "
		"gate would return them here: an infinite ping-pong between two scenes "
		"with no input accepted in between",
		(double)fClearance, (double)fZM_THORNACRE_GATE_ARRIVAL_CLEARANCE_MIN);

	// ★ ANTI-VACUITY, THROUGH THE IDENTICAL PREDICATE. A hypothetical gate
	//   centred ON the marker must FAIL the clause above. Without this arm a
	//   clearance that had lost its half-extent term (Max() collapsing to the
	//   centre) would keep passing on geometry that overlapped the player.
	const ZM_ThornacreVolume xProbeOnMarker =
	{
		Zenith_Maths::Vector3(fZM_THORNACRE_SOUTH_GATE_X,
			fZM_THORNACRE_GATE_CENTRE_Y, fZM_THORNACRE_SOUTH_ARRIVAL_Z),
		Zenith_Maths::Vector3(fZM_THORNACRE_GATE_SCALE_X,
			fZM_THORNACRE_GATE_SCALE_Y, fZM_THORNACRE_GATE_SCALE_Z)
	};
	ZENITH_ASSERT_LT(
		THGateNearFaceClearance(xProbeOnMarker,
			ZM_GetThornacreSouthArrivalBodyCentre().z),
		fZM_THORNACRE_GATE_ARRIVAL_CLEARANCE_MIN,
		"a gate centred exactly ON the arrival marker still satisfies the "
		"clearance floor -- the predicate cannot tell an overlapping sensor from "
		"a cleared one, so clause (8) is passing vacuously");

	// (9) THE BOX RESTS ON THE GROUND UNDER **IT**, AND CANNOT BE STEPPED OVER. A
	//     trigger authored at world Y 0 is a tunnel 28 m below the player: it never
	//     fires, the return is dead, and every compiled scale constant still reads
	//     right.
	//
	//     ★ THE GATE'S OWN MEASURED COLUMN, NOT THE ARRIVAL'S AND NOT THE RECIPE
	//     MIRROR. The two columns are 12 m apart; they agree exactly while
	//     axZM_THORNACRE_GROUND_SAMPLES is unfrozen and they will not agree
	//     afterwards, and a sensor resting on its neighbour's ground is part-buried
	//     or hovering with every scale constant still correct. This is the same
	//     ground fZM_THORNACRE_GATE_CENTRE_Y derives from, so the clause checks the
	//     'centre = ground + half the height' arithmetic rather than a shared plane.
	const float fGateGroundY =
		ZM_ThornacreGroundFeetY(ZM_THORNACRE_GROUND_SAMPLE_SOUTH_GATE);
	ZENITH_ASSERT_EQ_FLOAT(xGate.Min().y, fGateGroundY, fTH_EXACT_EPSILON,
		"the Thornacre return gate's underside is at y=%.4f rather than on the "
		"MEASURED ground of its own column (%.4f, "
		"ZM_THORNACRE_GROUND_SAMPLE_SOUTH_GATE) -- the 'centre = ground + half the "
		"height' rule has been broken and the sensor either floats or is buried",
		(double)xGate.Min().y, (double)fGateGroundY);
	ZENITH_ASSERT_GT(fZM_THORNACRE_GATE_SCALE_Y, fZM_HUMAN_BODY_HEIGHT,
		"the Thornacre return gate is %.3f m tall against a %.3f m person -- a "
		"sensor shorter than the body it is meant to catch can be stepped over "
		"by a capsule the physics tick happens to sample above it",
		(double)fZM_THORNACRE_GATE_SCALE_Y, (double)fZM_HUMAN_BODY_HEIGHT);
	ZENITH_ASSERT_GT(fZM_THORNACRE_GATE_SCALE_Z, 0.0f,
		"the Thornacre return gate has depth %.3f -- a non-positive extent "
		"inverts its AABB and every containment claim about it is noise",
		(double)fZM_THORNACRE_GATE_SCALE_Z);

	// (10) ...and it stands on the ground the recipe actually flattened, by the
	//      same containment argument U12 makes for the marker. The gate's own
	//      "still inside the flattened RouteGate pad" comment is a claim, so it
	//      is checked rather than believed.
	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetThornacreTerrainRecipe();
	const ZM_TerrainPadSpec* pxPad = THFindPad(xRecipe, szTH_ARRIVAL_PAD_NAME);
	ZENITH_ASSERT_NOT_NULL(pxPad,
		"the Thornacre recipe has no pad named '%s', so nothing flattens the "
		"ground the return gate rests on", szTH_ARRIVAL_PAD_NAME);
	if (pxPad != nullptr)
	{
		const float fPadRadiusLimit =
			fTH_PAD_CONTAINMENT_FRACTION * pxPad->m_fFlattenRadius;
		const float fGateToPad = THDistanceToPadCentre(*pxPad,
			xGate.m_xCenter.x, xGate.m_xCenter.z);
		ZENITH_ASSERT_LE(fGateToPad, fPadRadiusLimit,
			"the Thornacre return gate is centred %.3f m from the '%s' pad "
			"centre, outside the %.3f m the flatten dab can be trusted for. Its "
			"Min().y was just asserted against the flattened height, so out here "
			"that assertion is comparing the box against a surface that does not "
			"exist", (double)fGateToPad, szTH_ARRIVAL_PAD_NAME,
			(double)fPadRadiusLimit);
	}
}

// ============================================================================
// U14 -- the stub authors NO GYM DOOR, and its five names are unique lookup keys
// no future byte needle can be fooled by.
//
// ★ THE RULING (ZM-D-196). Thornacre is a TRAVERSAL STUB in this milestone: it
// gets terrain, one arrival marker, a player, a camera and a return trigger, and
// NO GYM DOOR. The compiled world table already carries the Thornacre -> Gym1
// ("Door") edge; that edge is deliberately UNBACKED. A later slice lands a byte
// needle asserting the committed Thornacre.zscen contains no gym door at all.
//
// ★ WHY THE COUNT LITERAL BELONGS HERE, AND IS NOT A RE-SPELLING. Nothing else
// in the tree would notice a sixth authored entity. Spelling 5u on this side
// makes "finish the town" an edit that has to red a test and be argued for,
// rather than one that ships quietly. It is a claim about the milestone's SCOPE,
// which is precisely the sort of claim a literal is the right shape for.
//
// ★ AND THE NAME BATTERY HAS TWO TRAPS, BOTH LOAD-BEARING.
//   1. The SCENE name is excluded. "Thornacre" is a strict substring of three of
//      the five entity names, so including it would red the pairwise clause on
//      the first pair it reached -- and a scene name is not an entity anyway.
//   2. ★ THE TYPE-NAME CLAUSE IS ONE-DIRECTIONAL, ON PURPOSE. It asserts
//      strstr(typeName, entityName) == nullptr and NEVER the converse: "Camera"
//      IS a substring of "ThornacreCamera" and "Terrain" of "ThornacreTerrain",
//      so symmetrising it reds immediately. The direction that matters is the
//      one that swallows a NEEDLE -- an entity name hidden inside a serialized
//      type name. DO NOT "fix" this into a symmetric form.
//      The player is EXCLUDED from that battery entirely: it is named "Player"
//      (mandatory -- ZM_FollowCamera.cpp:390 resolves its subject by that exact
//      name, and a scene-unique rename is a black screen), and "Player"
//      IS a substring of the type name "ZM_PlayerController". A needle on it must
//      therefore use the STRICTLY-MORE clause ZM_Tests_CommittedSceneBytes.cpp
//      already ships. The anti-vacuity arm below asserts exactly that overlap, so
//      the exclusion is documented by a passing assertion rather than a comment.
// ============================================================================
ZENITH_TEST(ZM_WorldTraversal, Thornacre_StubAuthorsNoGymDoorAndItsNamesAreUniqueLookupKeys)
{
	// The five names, in DECLARED order. The accessor is asserted to reproduce
	// this order below, which is what turns "walk the accessor" into a check on
	// the switch rather than a tour of whatever it happens to return.
	const char* const aszDeclaredNames[] =
	{
		szZM_THORNACRE_TERRAIN_ENTITY_NAME,
		szZM_THORNACRE_PLAYER_ENTITY_NAME,
		szZM_THORNACRE_CAMERA_ENTITY_NAME,
		szZM_THORNACRE_SOUTH_ARRIVAL_ENTITY_NAME,
		szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME,
	};
	const u_int uDeclaredCount =
		(u_int)(sizeof(aszDeclaredNames) / sizeof(aszDeclaredNames[0]));

	// (0) ★ THE RULING-2 TRIPWIRE. Five entities, no more, and the header's
	//     count has to agree with the inventory this test walks.
	ZENITH_ASSERT_EQ(uZM_THORNACRE_PLACEMENT_ENTITY_COUNT, 5u,
		"the Thornacre stub now declares %u placement entities. It is a TRAVERSAL "
		"STUB by ruling ZM-D-196 -- terrain, one arrival, a player, a camera and a "
		"return trigger, and NO GYM DOOR. If a sixth entity is genuinely wanted, "
		"say so in the DecisionLog and move both this literal and the header's, "
		"rather than making the tripwire follow the change it exists to catch",
		uZM_THORNACRE_PLACEMENT_ENTITY_COUNT);
	ZENITH_ASSERT_EQ(uDeclaredCount, uZM_THORNACRE_PLACEMENT_ENTITY_COUNT,
		"this test walks %u declared name constants but the header counts %u "
		"placement entities -- one of them has gained or lost a name",
		uDeclaredCount, uZM_THORNACRE_PLACEMENT_ENTITY_COUNT);

	// (1) ★ THE IDENTITY CLAUSE. Without it, an accessor that returned the SAME
	//     name for two indices satisfies every other clause in this unit: the
	//     pairwise checks below run over the DECLARED constants, and the count is
	//     a literal. This ties the switch to the declarations, index by index,
	//     and pins the ORDER (which is contract -- the authoring loop walks it,
	//     and reordering rewrites scene bytes under ZM-D-148).
	for (u_int u = 0u; u < uDeclaredCount; ++u)
	{
		ZENITH_ASSERT_STREQ(ZM_GetThornacrePlacementEntityName(u),
			aszDeclaredNames[u],
			"ZM_GetThornacrePlacementEntityName(%u) does not return the %u-th "
			"declared name constant. The authoring loop walks the accessor while "
			"every other check walks the constants, so the two must agree or the "
			"scene gets an entity no test ever looked at", u, u);
	}

	// (2) TOTAL, and DEGENERATE past the end. A caller that walked one index too
	//     far must fail its own lookup rather than silently match a real entity.
	const char* szPastEnd =
		ZM_GetThornacrePlacementEntityName(uZM_THORNACRE_PLACEMENT_ENTITY_COUNT);
	ZENITH_ASSERT_NOT_NULL(szPastEnd,
		"ZM_GetThornacrePlacementEntityName is not total: reading past the end "
		"returns null rather than the documented empty string, and the whole "
		"boot-unit run dies on the first caller that indexes it");
	ZENITH_ASSERT_TRUE(szPastEnd != nullptr && szPastEnd[0] == '\0',
		"ZM_GetThornacrePlacementEntityName(%u) returns '%s' rather than the "
		"documented empty string. Either the accessor grew a sixth case without "
		"the count moving -- a gym door smuggled past the ruling-2 tripwire -- or "
		"it reads off the end of its own switch",
		uZM_THORNACRE_PLACEMENT_ENTITY_COUNT,
		szPastEnd != nullptr ? szPastEnd : "(null)");

	// (3) EVERY NAME IS A LOOKUP KEY, AND NONE OF THEM IS GYM-SHAPED.
	for (u_int u = 0u; u < uDeclaredCount; ++u)
	{
		const char* szName = aszDeclaredNames[u];
		ZENITH_ASSERT_TRUE(THNameIsALookupKey(szName),
			"Thornacre placement name %u is null, empty, or carries a "
			"non-printable byte -- FindEntityByName would miss it and every "
			"clause that resolved it would report a missing entity rather than "
			"the cause", u);
		if (szName == nullptr)
		{
			continue;   // already reported
		}

		// ★ THE RULING, AS A PREDICATE. A gym door authored into this stub would
		//   almost certainly be named for the gym; this catches it even if the
		//   count literal above was "helpfully" updated in the same edit.
		ZENITH_ASSERT_NULL(std::strstr(szName, "Gym"),
			"the Thornacre stub declares an entity named '%s'. This town is a "
			"TRAVERSAL STUB in this milestone by ruling ZM-D-196: the compiled "
			"world table's Thornacre->Gym1 edge is DELIBERATELY unbacked, and a "
			"later slice asserts the committed bytes carry no gym door at all. "
			"Do not finish the town here", szName);
	}

	// (4) PAIRWISE DISTINCT, AND NEITHER OF ANY PAIR CONTAINS THE OTHER.
	//     Distinctness alone is not enough: the FromLab / FromLabSpawn trap is a
	//     needle on the SHORTER name silently counting the LONGER one too, and it
	//     is why the arrival marker is not called "ThornacreSouthGateArrival".
	for (u_int uA = 0u; uA < uDeclaredCount; ++uA)
	{
		for (u_int uB = uA + 1u; uB < uDeclaredCount; ++uB)
		{
			const char* szA = aszDeclaredNames[uA];
			const char* szB = aszDeclaredNames[uB];
			if (szA == nullptr || szB == nullptr)
			{
				continue;   // already reported
			}
			ZENITH_ASSERT_NE(std::strcmp(szA, szB), 0,
				"Thornacre placement entities %u and %u share the name '%s' -- "
				"FindEntityByName would resolve both to whichever the scene "
				"stored first", uA, uB, szA);
			ZENITH_ASSERT_NULL(std::strstr(szA, szB),
				"the Thornacre entity name '%s' CONTAINS '%s'. A committed-bytes "
				"needle on the shorter name would count the longer one too, and "
				"the count would be a lie nobody could read off the failure",
				szA, szB);
			ZENITH_ASSERT_NULL(std::strstr(szB, szA),
				"the Thornacre entity name '%s' CONTAINS '%s'. Same trap, other "
				"direction", szB, szA);
		}
	}

	// (5) ★ NO NAME IS SWALLOWED BY A SERIALIZED COMPONENT TYPE NAME.
	//     ONE DIRECTION ONLY -- see the banner. The player is excluded, and the
	//     anti-vacuity arm below proves why.
	const u_int uGameTypeCount = (u_int)(sizeof(s_aszGameComponentTypeNames)
		/ sizeof(s_aszGameComponentTypeNames[0]));
	const u_int uEngineTypeCount = (u_int)(sizeof(s_aszEngineComponentTypeNames)
		/ sizeof(s_aszEngineComponentTypeNames[0]));
	ZENITH_ASSERT_GT(uGameTypeCount + uEngineTypeCount, 0u,
		"the component type-name inventory in this file is empty, so the "
		"needle-swallow battery below iterates zero times and passes vacuously");

	// ★ AND THE TWO TABLES ARE THE SIZE THE OTHER COPY OF THEM CLAIMS.
	// Tests/ZM_Tests_Route1Placement.cpp transcribes these same 15 game and 17
	// engine rows BYTE-IDENTICALLY (aszROUTE1_GAME_TYPE_NAMES /
	// aszROUTE1_ENGINE_TYPE_NAMES) and NOTHING cross-checks the copies: a component
	// registered later updates one of them and silently weakens the other file's
	// needle-swallow battery, with both files green and no diff to read. Pinning the
	// COUNT as a literal on BOTH sides is the cheap reconciliation -- a row added to
	// one copy now reds the other.
	//
	// ★ THE DURABLE FIX IS A SHARED HEADER, and it is deliberately not landed here.
	// One Games/Zenithmon/Tests/ZM_Tests_ComponentTypeNames.h that both suites
	// include leaves one inventory instead of two; it belongs to whichever slice
	// adds a THIRD battery, where the duplication stops being a pair and starts
	// being a pattern. Until then: EDIT BOTH TABLES AND BOTH LITERALS IN ONE CHANGE.
	ZENITH_ASSERT_EQ(uGameTypeCount, 15u,
		"this file's GAME component type table holds %u rows, not the 15 that "
		"Games/Zenithmon/Zenithmon.cpp's ZENITH_REGISTER_COMPONENT block registers. "
		"Tests/ZM_Tests_Route1Placement.cpp carries a byte-identical copy pinned at "
		"the same literal, so this reds when one copy is grown and the other is not "
		"-- update BOTH tables and BOTH literals", uGameTypeCount);
	ZENITH_ASSERT_EQ(uEngineTypeCount, 17u,
		"this file's ENGINE component type table holds %u rows, not the 17 the "
		"engine serializes (the 16 in Zenith_ComponentMeta_Registration.cpp plus "
		"\"AIAgent\", registered at order 90 through Zenith_AI_RegisterComponents). "
		"Tests/ZM_Tests_Route1Placement.cpp carries a byte-identical copy pinned at "
		"the same literal", uEngineTypeCount);

	for (u_int uName = 0u; uName < uDeclaredCount; ++uName)
	{
		const char* szName = aszDeclaredNames[uName];
		if (szName == nullptr
			|| std::strcmp(szName, szZM_THORNACRE_PLAYER_ENTITY_NAME) == 0)
		{
			// ★ THE PLAYER IS EXCLUDED, DELIBERATELY AND PERMANENTLY. It must be
			//   named "Player" (ZM_FollowCamera resolves its subject by that
			//   literal), and "Player" is a substring of "ZM_PlayerController".
			//   That is a booked cost, not a defect: needles on it use the
			//   strictly-more clause. The arm after this loop asserts the overlap
			//   so the exclusion cannot rot into an unexamined skip.
			continue;
		}

		for (u_int uType = 0u; uType < uGameTypeCount; ++uType)
		{
			ZENITH_ASSERT_NULL(
				std::strstr(s_aszGameComponentTypeNames[uType], szName),
				"the Thornacre entity name '%s' is a substring of the game "
				"component type name '%s'. Component type names are serialized "
				"into the same .zscen byte range entity names are, so a needle on "
				"'%s' would count this type name too and the count would be a lie",
				szName, s_aszGameComponentTypeNames[uType], szName);
		}
		for (u_int uType = 0u; uType < uEngineTypeCount; ++uType)
		{
			ZENITH_ASSERT_NULL(
				std::strstr(s_aszEngineComponentTypeNames[uType], szName),
				"the Thornacre entity name '%s' is a substring of the engine "
				"component type name '%s' -- same needle-swallow trap",
				szName, s_aszEngineComponentTypeNames[uType]);
		}
	}

	// ★ ANTI-VACUITY, THROUGH THE IDENTICAL PREDICATE, AND THE DOCUMENTATION OF
	//   THE ONE EXCLUSION. If this fails, "Player" has stopped overlapping
	//   "ZM_PlayerController" and the exclusion above is dead weight -- OR the
	//   player was renamed, which is the black-screen defect itself.
	ZENITH_ASSERT_NOT_NULL(
		std::strstr("ZM_PlayerController", szZM_THORNACRE_PLAYER_ENTITY_NAME),
		"'%s' is no longer a substring of the component type name "
		"'ZM_PlayerController'. Either the battery above can no longer fire at "
		"all (and its exclusion of the player is meaningless), or the player "
		"entity has been renamed. Since Q-2026-08-15-002 a rename is NOT a black "
		"screen -- ZM_FollowCamera acquires the unique ZM_PlayerController in its "
		"own scene, not an entity called \"Player\" -- but it does invalidate "
		"every committed-bytes needle that takes the STRICTLY-MORE form purely "
		"because the player name nests inside the type name",
		szZM_THORNACRE_PLAYER_ENTITY_NAME);

	// (6) NO NAME CONTAINS A SPAWN TAG THIS SEAM USES. A marker named after its
	//     own tag inflates every future tag needle by one and forces the
	//     strictly-more clause on a claim that ought to be a plain equality --
	//     which is exactly why the arrival is "ThornacreSouthArrival" and not
	//     "ThornacreFromRoute1Spawn".
	const ZM_WorldSpec& xThornacre = ZM_GetWorldSpec(ZM_SCENE_THORNACRE);
	ZENITH_ASSERT_GT(xThornacre.m_uSpawnTagCount, 0u,
		"the compiled ZM_SCENE_THORNACRE row offers no spawn tags at all, so the "
		"tag-containment battery below iterates zero times -- and no warp could "
		"ever land in this town either");
	for (u_int uName = 0u; uName < uDeclaredCount; ++uName)
	{
		const char* szName = aszDeclaredNames[uName];
		if (szName == nullptr)
		{
			continue;
		}
		for (u_int uTag = 0u; uTag < xThornacre.m_uSpawnTagCount; ++uTag)
		{
			const char* szTag = xThornacre.m_pszSpawnTags != nullptr
				? xThornacre.m_pszSpawnTags[uTag] : nullptr;
			if (szTag == nullptr || szTag[0] == '\0')
			{
				continue;
			}
			ZENITH_ASSERT_NULL(std::strstr(szName, szTag),
				"the Thornacre entity name '%s' contains the offered spawn tag "
				"'%s'. Tags land in the same .zscen byte range names do, so a "
				"later tag needle would count this entity name too",
				szName, szTag);
		}
		// ...and the tags this town's own edges ASK for, which land in its bytes
		// on the trigger side rather than on the marker side.
		for (u_int uEdge = 0u; uEdge < xThornacre.m_uConnectionCount; ++uEdge)
		{
			const char* szAsked = xThornacre.m_pxConnections != nullptr
				? xThornacre.m_pxConnections[uEdge].m_szSpawnTag : nullptr;
			if (szAsked == nullptr || szAsked[0] == '\0')
			{
				continue;
			}
			ZENITH_ASSERT_NULL(std::strstr(szName, szAsked),
				"the Thornacre entity name '%s' contains the spawn tag '%s' that "
				"one of this town's own warp edges asks for -- same needle "
				"inflation, on the trigger side", szName, szAsked);
		}
	}

	// (7) ★ THE ASYMMETRY IS DELIBERATE, AND THIS IS WHERE IT IS WRITTEN DOWN.
	//     The world table PROMISES a gym door. The registration table has no gym
	//     row. The stub authors no gym entity. Three independent facts, all
	//     asserted, so "Thornacre has no gym" reads as a decision rather than as
	//     an oversight somebody will one day "fix" in the wrong direction.
	const ZM_SceneConnection* pxGymEdge = THFindThornacreGymConnection();
	ZENITH_ASSERT_NOT_NULL(pxGymEdge,
		"the compiled ZM_SCENE_THORNACRE row no longer carries an edge targeting "
		"ZM_SCENE_GYM1. The stub's whole shape is 'the table promises a gym door "
		"and this milestone deliberately does not back it' -- if the edge is "
		"genuinely being removed, that is a world-table decision to record, and "
		"this unit is the place it has to be argued");
	ZENITH_ASSERT_TRUE(pxGymEdge != nullptr
		&& pxGymEdge->m_szSpawnTag != nullptr
		&& pxGymEdge->m_szSpawnTag[0] != '\0',
		"the Thornacre->Gym1 edge carries no spawn tag, so the unbacked edge is "
		"not merely unbacked but malformed");
	ZENITH_ASSERT_FALSE(ZM_IsSceneRegisteredForLoad(ZM_SCENE_GYM1),
		"Gym 1 has gained a row in the compiled scene-registration table. That is "
		"out of S8 item 2's scope: R1-1 registers exactly the scenes this "
		"milestone authors, and a gym registration is the first half of finishing "
		"a town the ruling says stays a traversal stub");
}

// ============================================================================
// U15 -- the settled camera stands above the ground, behind the arrival, and
// sees the town.
//
// ★ WHY THIS UNIT EXISTS, STATED BLUNTLY: until it landed, THE WHOLE CAMERA HALF
// OF ZM_ThornacrePlacement.h HAD NO READER. ZM_GetThornacreSettledCameraPosition,
// fZM_THORNACRE_CAMERA_FOV_DEGREES, _NEAR, _FAR, _ASPECT and
// _MIN_GROUND_CLEARANCE were compiled constants nothing in the game and nothing
// in the suite ever evaluated -- while the header's own comments said "the camera
// unit's anti-vacuity arm runs through this same arithmetic to prove" and "the
// floor the camera unit enforces". Those sentences described a check that did not
// exist; this unit is what makes them true.
//
// Two silent shipping defects live in that gap, and BOTH leave every other clause
// in this file perfectly green:
//   * a SIGN ERROR ON THE PITCH swings the eye BELOW its pivot instead of above
//     it and buries the camera in the terrain, with every constant in the header
//     still reading exactly as intended;
//   * a DEFAULT FAR PLANE (the 100 m the two interiors ship with) clips a
//     1024 m-deep town away a few strides ahead of the player.
//
// ★ IT MIRRORS ROUTE 1'S TWIN CLAUSE FOR CLAUSE, deliberately --
// Tests/ZM_Tests_Route1Placement.cpp's
// Route1_SettledCameraStandsAboveGroundBehindTheArrival. The two arrivals are the
// same shape, so a reader who has understood one has understood the other, and a
// clause added to one is a clause obviously missing from the other.
//
// ★ AND THE FAR-PLANE CLAUSE IS MEASURED AGAINST THE RECIPE'S OWN Z EXTENT, not
// against a number in the header, so re-cutting the town's bounds moves the
// requirement with them rather than leaving a comment behind.
// ============================================================================
ZENITH_TEST(ZM_WorldTraversal, Thornacre_SettledCameraStandsAboveGroundBehindTheArrival)
{
	// (1) The projection parameters describe a usable frustum at all. A far plane
	//     at or behind the near one, a non-positive near plane, an out-of-domain
	//     FOV or a non-positive aspect each build a projection matrix that renders
	//     NOTHING -- which reads to a player as a black screen and to a programmer
	//     as anything but a camera constant.
	ZENITH_ASSERT_LT(fZM_THORNACRE_CAMERA_NEAR, fZM_THORNACRE_CAMERA_FAR,
		"the Thornacre camera's near plane %.4f is at or beyond its far plane %.4f",
		(double)fZM_THORNACRE_CAMERA_NEAR, (double)fZM_THORNACRE_CAMERA_FAR);
	ZENITH_ASSERT_GT(fZM_THORNACRE_CAMERA_NEAR, 0.0f,
		"the Thornacre camera's near plane is %.4f; a non-positive near plane is "
		"not a perspective projection", (double)fZM_THORNACRE_CAMERA_NEAR);
	ZENITH_ASSERT_GT(fZM_THORNACRE_CAMERA_FOV_DEGREES, 0.0f,
		"the Thornacre camera's field of view is %.4f degrees",
		(double)fZM_THORNACRE_CAMERA_FOV_DEGREES);
	ZENITH_ASSERT_LT(fZM_THORNACRE_CAMERA_FOV_DEGREES, 180.0f,
		"the Thornacre camera's field of view is %.4f degrees, at or past the "
		"degenerate half turn", (double)fZM_THORNACRE_CAMERA_FOV_DEGREES);
	ZENITH_ASSERT_GT(fZM_THORNACRE_CAMERA_ASPECT, 0.0f,
		"the Thornacre camera's aspect ratio is %.4f",
		(double)fZM_THORNACRE_CAMERA_ASPECT);
	ZENITH_ASSERT_GT(fZM_THORNACRE_CAMERA_ARM, 0.0f,
		"the Thornacre camera's arm is %.4f -- a non-positive arm puts the eye at "
		"or in front of its own pivot, which is not a follow camera",
		(double)fZM_THORNACRE_CAMERA_ARM);

	// (2) The far plane covers the town the camera is looking up. Measured against
	//     the RECIPE's authored Z bounds, never against the header's comment, so
	//     re-cutting the region moves the requirement with it.
	//
	//     THE MUTATION: drop the far plane back to the interiors' 100 m and the
	//     town is clipped away in front of the arriving player.
	const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetThornacreTerrainRecipe();
	const float fTownDepth = xRecipe.m_fWorldMaxZ - xRecipe.m_fWorldMinZ;
	ZENITH_ASSERT_GT(fTownDepth, 0.0f,
		"the Thornacre recipe declares a non-positive Z extent (%.4f to %.4f), so "
		"the far-plane clause below has nothing to measure against",
		(double)xRecipe.m_fWorldMinZ, (double)xRecipe.m_fWorldMaxZ);
	ZENITH_ASSERT_GE(fZM_THORNACRE_CAMERA_FAR, fTownDepth,
		"the Thornacre camera's far plane is %.4f against a %.4f m town. The "
		"default 100 m plane the two interiors ship with would clip the world away "
		"a few strides ahead of the player",
		(double)fZM_THORNACRE_CAMERA_FAR, (double)fTownDepth);

	// (3) The settled eye stands clear of the ground.
	//
	//     THE MUTATION: flip the sign of fZM_THORNACRE_CAMERA_PITCH and the arm
	//     swings the eye BELOW the pivot, into the terrain. Clause (5b) proves this
	//     clause can actually see that.
	//
	//     ★ THE FLOOR STANDS ON THE ARRIVAL'S MEASURED COLUMN, because the settled
	//     pose is DERIVED from the arrival: the pivot is
	//     ZM_GetThornacreSouthArrivalBodyCentre lifted by the follow camera's pivot
	//     height, so the surface the eye has to clear is the one that body stands
	//     on -- not the recipe mirror, which is a different quantity wearing the
	//     same number until axZM_THORNACRE_GROUND_SAMPLES is frozen.
	const Zenith_Maths::Vector3 xCamera = ZM_GetThornacreSettledCameraPosition();
	const float fArrivalGroundY =
		ZM_ThornacreGroundFeetY(ZM_THORNACRE_GROUND_SAMPLE_SOUTH_ARRIVAL);
	const float fClearanceFloor =
		fArrivalGroundY + fZM_THORNACRE_CAMERA_MIN_GROUND_CLEARANCE;
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[ZM_ThornacrePlacement] settled camera (%.3f, %.3f, %.3f) against a %.3f m "
		"floor (arrival's measured ground %.3f + %.3f clearance); arrival body "
		"centre z=%.3f, arm %.2f",
		(double)xCamera.x, (double)xCamera.y, (double)xCamera.z,
		(double)fClearanceFloor, (double)fArrivalGroundY,
		(double)fZM_THORNACRE_CAMERA_MIN_GROUND_CLEARANCE,
		(double)ZM_GetThornacreSouthArrivalBodyCentre().z,
		(double)fZM_THORNACRE_CAMERA_ARM);
	ZENITH_ASSERT_GE(xCamera.y, fClearanceFloor,
		"the settled Thornacre camera sits at Y %.4f, below the %.4f m floor (the "
		"arrival's MEASURED ground %.4f + %.4f clearance). A pitch sign error swings "
		"the eye BELOW its pivot and buries it in the terrain, with every camera "
		"constant in the header still reading exactly as intended", (double)xCamera.y,
		(double)fClearanceFloor, (double)fArrivalGroundY,
		(double)fZM_THORNACRE_CAMERA_MIN_GROUND_CLEARANCE);

	// (4) It stands BEHIND the arriving body -- far enough back not to be inside
	//     the capsule, and never further than its own arm.
	//
	//     THE MUTATIONS: a pose accessor that lost its arm term leaves the eye ON
	//     the player (the near half reds); one that added the forward vector
	//     instead of subtracting it, or that swung a longer arm than the constant
	//     declares, puts the eye somewhere the follow camera's spring will never
	//     settle (the far half reds).
	const Zenith_Maths::Vector3 xArrival = ZM_GetThornacreSouthArrivalBodyCentre();
	const float fToArrivalX = xCamera.x - xArrival.x;
	const float fToArrivalZ = xCamera.z - xArrival.z;
	const float fPlanarDistance =
		std::sqrt(fToArrivalX * fToArrivalX + fToArrivalZ * fToArrivalZ);
	ZENITH_ASSERT_GE(fPlanarDistance, fZM_HUMAN_BODY_CAPSULE_RADIUS,
		"the settled Thornacre camera stands only %.4f m from the arriving body in "
		"plan, inside its own %.4f m capsule radius -- the eye is inside the player",
		(double)fPlanarDistance, (double)fZM_HUMAN_BODY_CAPSULE_RADIUS);
	ZENITH_ASSERT_LE(fPlanarDistance, fZM_THORNACRE_CAMERA_ARM,
		"the settled Thornacre camera stands %.4f m from the arriving body in plan, "
		"further than its own %.4f m arm -- the pose accessor is no longer swinging "
		"the arm back from the pivot", (double)fPlanarDistance,
		(double)fZM_THORNACRE_CAMERA_ARM);

	// (5) ★ ANTI-VACUITY, in two steps because the arm needs a second
	//     implementation and a second implementation checking only itself would
	//     prove nothing.
	//     (5a) the local pose arithmetic AGREES with the shipped accessor at the
	//          shipped pitch -- which is what makes it a stand-in for it;
	//     (5b) the SIGN-FLIPPED pitch, through that same arithmetic, must FAIL
	//          clause (3)'s floor. This is the arm the header's pitch comment
	//          promises, and without it clause (3) is not known to be able to red
	//          on the one defect it was written for.
	const Zenith_Maths::Vector3 xLocalAtShippedPitch =
		THCameraPositionForPitch(fZM_THORNACRE_CAMERA_PITCH);
	ZENITH_ASSERT_NEAR_VEC3(xLocalAtShippedPitch, xCamera, fTH_EXACT_EPSILON,
		"this unit's pose arithmetic disagrees with "
		"ZM_GetThornacreSettledCameraPosition at the shipped pitch, so the "
		"sign-flipped probe below is not exercising the accessor's geometry");

	const Zenith_Maths::Vector3 xFlippedPitchCamera =
		THCameraPositionForPitch(-fZM_THORNACRE_CAMERA_PITCH);
	ZENITH_ASSERT_LT(xFlippedPitchCamera.y, fClearanceFloor,
		"the ANTI-VACUITY arm failed: a sign-flipped pitch puts the settled eye at "
		"Y %.4f, which still CLEARS the %.4f m floor clause (3) uses. Clause (3) "
		"therefore cannot red on the pitch sign error it was written for",
		(double)xFlippedPitchCamera.y, (double)fClearanceFloor);
}
