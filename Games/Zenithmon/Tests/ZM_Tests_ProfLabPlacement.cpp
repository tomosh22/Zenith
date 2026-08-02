#include "Zenith.h"

// ============================================================================
// ZM_Tests_ProfLabPlacement (SC1) -- the boot units that turn ProfLab's authored
// PLACEMENT from a comment into a checked property, and that tie the placement
// header's mirrored spawn tag to the compiled world table so the two can never
// be spelled twice.
//
// PURE: no scene, no entity, no physics, no assets, no graphics, no g_xEngine.
// Everything here reads COMPILED CONSTANTS -- ZM_ProfLabPlacement.h, ZM_WorldSpec
// and the pure statics of ZM_FollowCamera / ZM_PlayerController / ZM_SpawnPoint
// (nothing is constructed). That is the whole point: the automated leg
// (ZM_ProfLabWarp_Test) needs the committed ProfLab.zscen and a live warp, so it
// cannot be the only place the placement argument lives.
//
// ★ WHAT THESE EIGHT UNITS CANNOT DO, STATED UP FRONT. They run BEFORE the
// initial scene loads (Zenith_Engine.cpp runs RunAllTests() ahead of
// Project_LoadInitialScene), so they can see NEITHER the scene registry NOR one
// byte of Assets/Scenes/ProfLab.zscen. They cannot detect a missing
// RegisterSceneBuildIndex, and they cannot prove the committed bytes match the
// header. Those two claims belong to ZM_ProfLabWarp_Test's clauses A and I. Do
// not read this file's greenness as "ProfLab is reachable" or "ProfLab.zscen is
// current".
//
// ★ AND THESE UNITS CREATE NO ENTITY. That is a hard constraint, not a style
// note: scene authoring bakes in the entity indices assigned during the boot it
// runs in, and the boot-time unit suite allocates entities FIRST -- so one
// entity-creating boot unit re-authors different .zscen bytes and invalidates
// the identical-SHA256 two-boot proof this sub-commit owes.
// ============================================================================

#include <cmath>     // sqrt / fabs / isfinite
#include <cstring>   // strcmp -- the byte-identity claims

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"        // the camera's PURE statics (nothing is constructed)
#include "Zenithmon/Components/ZM_SpawnPoint.h"          // IsTagValid / uTAG_CAPACITY -- the tag contract
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"         // THE body contract -- by name, not by literal
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"

namespace
{
	// "These two floats are the same authored number." Generous by float
	// standards and far below any placement quantity ProfLab uses.
	constexpr float fPROFLAB_EXACT_EPSILON = 1.0e-4f;

	// "These two floats are the same authored PLANE." Looser than the equality
	// epsilon because a face coordinate is a sum of a centre and a half-extent,
	// so it carries one more rounding than a directly authored value.
	constexpr float fPROFLAB_PLANE_EPSILON = 1.0e-3f;

	// The clear gap the doorway must leave BESIDE the player's shoulders, per
	// side. Small on purpose: this is a "the aperture has not been walled up"
	// floor, not a comfort target. The authored opening is several times this.
	constexpr float fPROFLAB_APERTURE_SIDE_MARGIN = 0.25f;

	// ...and the clear gap above the player's head under the lintel.
	constexpr float fPROFLAB_APERTURE_HEAD_MARGIN = 0.20f;

	// How far inside the floor slab the trailing camera must land. The camera is
	// a point, so this is the "it is genuinely in the room, not kissing the wall"
	// margin rather than a body radius.
	constexpr float fPROFLAB_CAMERA_FOOTPRINT_MARGIN = 0.25f;

	// A closed room needs a floor plus four bounding sides at minimum. Asserted
	// so the [0, ZM_PROFLAB_BLOCK_COUNT) walks below cannot pass vacuously on an
	// empty or half-deleted table.
	constexpr u_int uPROFLAB_MIN_BLOCK_COUNT = 5u;

	// Half a turn, for the anti-vacuity heading in U4. Named rather than typed
	// inline so it reads as "the opposite direction" and not as a tuning value.
	constexpr float fPROFLAB_HALF_TURN_RADIANS = 3.14159265f;

	// The authored player capsule half-extent, read from the SHIPPED body contract
	// rather than restated, so a re-tune of the body lands here.
	float ProfLabPlayerHalfExtent()
	{
		return fZM_HUMAN_BODY_HALF_HEIGHT;
	}

	// ★ THE PLAYER RADIUS AND THE SPAWN FEET ARE NOT RE-IMPLEMENTED HERE. Local
	// copies of fZM_PROFLAB_PLAYER_RADIUS / ZM_GetProfLabSpawnFeet() would read
	// the same constants and so would never disagree on a NUMBER -- but the
	// authoring calls the HEADER accessor while these units would be reading
	// their own arithmetic, so changing the accessor's body would leave every
	// unit below green and silent. Call the shipped accessor, always.
	Zenith_Maths::Vector3 ProfLabSpawnCentre()
	{
		Zenith_Maths::Vector3 xCentre = ZM_GetProfLabSpawnFeet();
		xCentre.y += ProfLabPlayerHalfExtent();
		return xCentre;
	}

	bool ProfLabBlockoutIsFinite(const ZM_ProfLabBlockout& xBlock)
	{
		return std::isfinite(xBlock.m_xCenter.x)
			&& std::isfinite(xBlock.m_xCenter.y)
			&& std::isfinite(xBlock.m_xCenter.z)
			&& std::isfinite(xBlock.m_xScale.x)
			&& std::isfinite(xBlock.m_xScale.y)
			&& std::isfinite(xBlock.m_xScale.z);
	}

	// Every printable, space-free character is a legal scene entity name byte.
	// The names are the lookup keys ZM_ProfLabWarp_Test's shell walk feeds to
	// Zenith_SceneData::FindEntityByName, so a stray space or control byte is a
	// silent miss rather than a diagnosable failure.
	bool ProfLabNameIsALookupKey(const char* szName)
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

	// One axis of the slab test. Narrows [fEnterInOut, fExitInOut] and returns
	// false the moment the interval empties. Written per-axis with named scalars
	// rather than indexing the vector, because glm's operator[] takes a SIGNED
	// length_type and this build treats the conversion warning as an error.
	bool NarrowSlab(
		float fOrigin, float fDirection, float fSlabMin, float fSlabMax,
		float& fEnterInOut, float& fExitInOut)
	{
		if (std::fabs(fDirection) < 1.0e-6f)
		{
			// Parallel to this slab: either permanently inside it or never in it.
			return fOrigin >= fSlabMin && fOrigin <= fSlabMax;
		}
		float fNear = (fSlabMin - fOrigin) / fDirection;
		float fFar = (fSlabMax - fOrigin) / fDirection;
		if (fNear > fFar)
		{
			const float fSwap = fNear;
			fNear = fFar;
			fFar = fSwap;
		}
		if (fNear > fEnterInOut) { fEnterInOut = fNear; }
		if (fFar < fExitInOut) { fExitInOut = fFar; }
		return fEnterInOut <= fExitInOut;
	}

	// Slab test. Returns the distance along xDirection at which the ray first
	// enters the box, or -1 when it never enters within fMaxDistance. An origin
	// that starts INSIDE the box returns 0, which is the answer the clamp wants.
	float RayAabbEntryDistance(
		const Zenith_Maths::Vector3& xOrigin,
		const Zenith_Maths::Vector3& xDirection,
		float fMaxDistance,
		const ZM_ProfLabBlockout& xBox)
	{
		const Zenith_Maths::Vector3 xMin = xBox.Min();
		const Zenith_Maths::Vector3 xMax = xBox.Max();
		float fEnter = 0.0f;
		float fExit = fMaxDistance;
		if (!NarrowSlab(xOrigin.x, xDirection.x, xMin.x, xMax.x, fEnter, fExit)
			|| !NarrowSlab(xOrigin.y, xDirection.y, xMin.y, xMax.y, fEnter, fExit)
			|| !NarrowSlab(xOrigin.z, xDirection.z, xMin.z, xMax.z, fEnter, fExit))
		{
			return -1.0f;
		}
		return fEnter;
	}

	// The two front-wall stubs that flank the doorway, and the clear gap between
	// them. DERIVED from the blockout table rather than carried as its own
	// constant, because the aperture IS the absence of geometry between those two
	// boxes -- a separate "aperture width" constant could drift away from the
	// geometry it claims to describe and nothing would notice.
	float ProfLabApertureClearWidth()
	{
		return ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FRONT_RIGHT).Min().x
			- ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FRONT_LEFT).Max().x;
	}

	// Likewise the headroom: floor surface to the underside of the lintel.
	float ProfLabApertureClearHeight()
	{
		return ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_LINTEL).Min().y
			- ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FLOOR).Max().y;
	}
}

// ============================================================================
// U1 -- the compiled world-table row ProfLab's authoring and its warp both read.
// ============================================================================

// The row is an INTERIOR with no terrain set, offering exactly the tag its one
// inbound connection names. Note what is deliberately absent: no build-index
// LITERAL. 41 is spelled once, in ZM_WorldSpec.cpp; everything else -- the
// registration, the warp, this unit -- resolves it through m_uBuildIndex, so a
// renumbered scene moves every consumer together instead of stranding one.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_WorldSpecRowIsTheInteriorWithNoTerrain)
{
	const ZM_WorldSpec& xSpec = ZM_GetWorldSpec(ZM_SCENE_PROFLAB);

	ZENITH_ASSERT_EQ((u_int)xSpec.m_eId, (u_int)ZM_SCENE_PROFLAB,
		"the ProfLab row is not at its own ZM_SCENE_ID index");
	ZENITH_ASSERT_EQ((u_int)xSpec.m_eKind, (u_int)ZM_SCENE_KIND_INTERIOR,
		"ProfLab is authored as a terrain-independent interior on every tools "
		"boot including Null/CI -- kind %s says otherwise",
		ZM_SceneKindToString(xSpec.m_eKind));

	// The empty terrain set is what puts ProfLab in the ALWAYS-RUN authoring
	// section rather than behind the AUTHOR_DAWNMERE gate. A non-empty string
	// here would mean the scene owes a warm terrain bake before it can be
	// authored at all, which is a different (and much more expensive) sub-commit.
	ZENITH_ASSERT_NOT_NULL(xSpec.m_szTerrainSet,
		"the ProfLab row's terrain set is null rather than empty");
	ZENITH_ASSERT_TRUE(
		xSpec.m_szTerrainSet != nullptr && xSpec.m_szTerrainSet[0] == '\0',
		"ProfLab declares terrain set '%s' -- an interior owns no terrain, and a "
		"non-empty set moves its authoring behind the AUTHOR_DAWNMERE gate",
		xSpec.m_szTerrainSet != nullptr ? xSpec.m_szTerrainSet : "(null)");

	// Exactly one offered tag (the arrival marker ProfLab authors) and exactly
	// one outbound connection (the way back to Dawnmere).
	ZENITH_ASSERT_EQ(xSpec.m_uSpawnTagCount, 1u,
		"ProfLab offers %u spawn tags; the interior authors exactly one arrival "
		"marker", xSpec.m_uSpawnTagCount);
	ZENITH_ASSERT_NOT_NULL(xSpec.m_pszSpawnTags,
		"ProfLab declares spawn tags but the array pointer is null");
	ZENITH_ASSERT_EQ(xSpec.m_uConnectionCount, 1u,
		"ProfLab declares %u connections; the interior has one exit",
		xSpec.m_uConnectionCount);
	ZENITH_ASSERT_NOT_NULL(xSpec.m_pxConnections,
		"ProfLab declares connections but the array pointer is null");

	if (xSpec.m_uConnectionCount == 1u && xSpec.m_pxConnections != nullptr)
	{
		const ZM_SceneConnection& xConn = xSpec.m_pxConnections[0];
		ZENITH_ASSERT_EQ((u_int)xConn.m_eTarget, (u_int)ZM_SCENE_DAWNMERE,
			"ProfLab's exit targets %s, not the village it opens onto",
			ZM_GetSceneName(xConn.m_eTarget));

		// Referential closure, asserted without re-spelling the tag: whatever
		// string the connection names, the target must offer it. Re-typing
		// "FromLab" here would compare a literal against itself.
		ZENITH_ASSERT_NOT_NULL(xConn.m_szSpawnTag,
			"ProfLab's exit connection carries a null spawn tag");
		bool bTargetOffersIt = false;
		const ZM_WorldSpec& xTarget = ZM_GetWorldSpec(xConn.m_eTarget);
		for (u_int uTag = 0u; uTag < xTarget.m_uSpawnTagCount; ++uTag)
		{
			if (xConn.m_szSpawnTag != nullptr
				&& std::strcmp(xTarget.m_pszSpawnTags[uTag], xConn.m_szSpawnTag) == 0)
			{
				bTargetOffersIt = true;
			}
		}
		ZENITH_ASSERT_TRUE(bTargetOffersIt,
			"ProfLab's exit targets spawn tag '%s' in %s, which that scene does "
			"not offer -- the warp would pass IsWarpDestinationValid and then park "
			"in WAITING_FOR_SPAWN, which has no timeout",
			xConn.m_szSpawnTag != nullptr ? xConn.m_szSpawnTag : "(null)",
			ZM_GetSceneName(xConn.m_eTarget));
	}

	// The build index round-trips. This is the compiled half of the claim the
	// automated test proves at runtime: an index that does not resolve back to
	// ProfLab means the warp machine cannot name the destination it is holding.
	ZENITH_ASSERT_EQ(
		(u_int)ZM_FindSceneByBuildIndex(xSpec.m_uBuildIndex),
		(u_int)ZM_SCENE_PROFLAB,
		"build index %u does not resolve back to ProfLab",
		xSpec.m_uBuildIndex);
	ZENITH_ASSERT_NE(xSpec.m_uBuildIndex,
		ZM_GetWorldSpec(ZM_SCENE_PLAYERHOME).m_uBuildIndex,
		"ProfLab and PlayerHome share build index %u -- the two interiors would "
		"resolve to one another's scene file", xSpec.m_uBuildIndex);
}

// ============================================================================
// U2 -- the blockout table is non-degenerate.
// ============================================================================

// Every authored shell piece is a real box: finite centre, strictly positive
// extents. A zero or negative scale produces an inverted AABB whose Min() is
// past its Max(), which every clearance and containment claim below would then
// evaluate against nonsense while still returning a bool.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_BlockoutExtentsArePositiveAndFinite)
{
	// ANTI-VACUITY, and it is the clause that makes the walks meaningful. Delete
	// the table and every [0, COUNT) loop below passes by never iterating.
	ZENITH_ASSERT_GE((u_int)ZM_PROFLAB_BLOCK_COUNT, uPROFLAB_MIN_BLOCK_COUNT,
		"the ProfLab blockout table has %u entries; a closed room needs at least "
		"a floor and four bounding sides, and every walk in this file passes "
		"vacuously below that", (u_int)ZM_PROFLAB_BLOCK_COUNT);

	for (u_int u = 0u; u < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++u)
	{
		const ZM_PROFLAB_BLOCK eBlock = (ZM_PROFLAB_BLOCK)u;
		const ZM_ProfLabBlockout xBlock = ZM_GetProfLabBlock(eBlock);
		const char* szName = ZM_GetProfLabBlockName(eBlock);

		ZENITH_ASSERT_TRUE(ProfLabBlockoutIsFinite(xBlock),
			"block '%s' carries a non-finite centre or scale",
			szName != nullptr ? szName : "(null)");
		ZENITH_ASSERT_GT(xBlock.m_xScale.x, 0.0f,
			"block '%s' has X extent %.5f -- a non-positive extent inverts its "
			"AABB and every containment claim about it becomes noise",
			szName != nullptr ? szName : "(null)", (double)xBlock.m_xScale.x);
		ZENITH_ASSERT_GT(xBlock.m_xScale.y, 0.0f,
			"block '%s' has Y extent %.5f -- a non-positive extent inverts its AABB",
			szName != nullptr ? szName : "(null)", (double)xBlock.m_xScale.y);
		ZENITH_ASSERT_GT(xBlock.m_xScale.z, 0.0f,
			"block '%s' has Z extent %.5f -- a non-positive extent inverts its AABB",
			szName != nullptr ? szName : "(null)", (double)xBlock.m_xScale.z);

		// ...and the derived faces really do bracket the centre, so Min()/Max()
		// can be trusted by the units that follow.
		ZENITH_ASSERT_LT(xBlock.Min().x, xBlock.Max().x,
			"block '%s': Min().x is not below Max().x",
			szName != nullptr ? szName : "(null)");
		ZENITH_ASSERT_LT(xBlock.Min().y, xBlock.Max().y,
			"block '%s': Min().y is not below Max().y",
			szName != nullptr ? szName : "(null)");
		ZENITH_ASSERT_LT(xBlock.Min().z, xBlock.Max().z,
			"block '%s': Min().z is not below Max().z",
			szName != nullptr ? szName : "(null)");
	}
}

// ============================================================================
// U3 -- the arrival marker.
// ============================================================================

// The Door spawn stands ON the floor, INSIDE the room, with room behind it for
// the camera. Clause 3 is the feet-vs-centre convention that has bitten this
// project before: ZM_SpawnPoint markers are FEET and the warp adds the capsule
// half-extent at arrival, so a marker authored at a body centre drops the player
// half a body into the ceiling. Clause 0 is what makes "the capsule half-extent"
// one number rather than two: the header MIRRORS it, the warp COMPUTES it.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_DoorSpawnStandsOnTheFloorWithCameraClearance)
{
	const ZM_ProfLabBlockout xFloor = ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FLOOR);
	const Zenith_Maths::Vector3 xFeet = ZM_GetProfLabSpawnFeet();
	const float fRadius = fZM_PROFLAB_PLAYER_RADIUS;

	// (0) ★ THE AUTHORED HALF-EXTENT AND THE ARRIVAL HALF-EXTENT ARE ONE NUMBER.
	//     ZM_GetProfLabPlayerCenter -- which the authoring writes into the scene --
	//     reads fZM_PROFLAB_PLAYER_CAPSULE_HALF_EXTENT, while the automated arrival
	//     clause compares the arrived body against
	//     ZM_GameStateManager::CalculateSpawnCenter. Both now resolve to the body
	//     contract, and this clause is what keeps that true: reintroduce a local
	//     literal on either side and the authored body would split from the point
	//     the warp computes, with every other unit still green.
	ZENITH_ASSERT_EQ_FLOAT(fZM_PROFLAB_PLAYER_CAPSULE_HALF_EXTENT,
		ProfLabPlayerHalfExtent(), fPROFLAB_EXACT_EPSILON,
		"the placement header states a %.5f m capsule half-extent but the body "
		"contract says %.5f -- the authored Player body (ZM_GetProfLabPlayerCenter) "
		"would no longer land on the point the warp computes at arrival "
		"(CalculateSpawnCenter)",
		(double)fZM_PROFLAB_PLAYER_CAPSULE_HALF_EXTENT,
		(double)ProfLabPlayerHalfExtent());

	// (1) The spawn's X column is inside the floor slab by at least the body
	//     radius -- a marker on the boundary puts half the capsule off the slab.
	ZENITH_ASSERT_GT(xFeet.x - xFloor.Min().x, fRadius,
		"the Door spawn at x=%.4f is within the %.4f m body radius of the floor's "
		"-X edge (x=%.4f) -- the capsule would hang off the slab",
		(double)xFeet.x, (double)fRadius, (double)xFloor.Min().x);
	ZENITH_ASSERT_GT(xFloor.Max().x - xFeet.x, fRadius,
		"the Door spawn at x=%.4f is within the %.4f m body radius of the floor's "
		"+X edge (x=%.4f)",
		(double)xFeet.x, (double)fRadius, (double)xFloor.Max().x);

	// (2) ...and its Z column likewise.
	ZENITH_ASSERT_GT(xFeet.z - xFloor.Min().z, fRadius,
		"the Door spawn at z=%.4f is within the %.4f m body radius of the floor's "
		"-Z edge (z=%.4f)",
		(double)xFeet.z, (double)fRadius, (double)xFloor.Min().z);
	ZENITH_ASSERT_GT(xFloor.Max().z - xFeet.z, fRadius,
		"the Door spawn at z=%.4f is within the %.4f m body radius of the floor's "
		"+Z edge (z=%.4f)",
		(double)xFeet.z, (double)fRadius, (double)xFloor.Max().z);

	// (3) THE FEET CONVENTION. The marker's Y is the floor's TOP SURFACE, not its
	//     centre and not a body centre. Exact, because both sides are authored
	//     numbers rather than measurements.
	ZENITH_ASSERT_EQ_FLOAT(xFeet.y, xFloor.Max().y, fPROFLAB_PLANE_EPSILON,
		"the Door spawn's feet Y is %.5f but the floor's top surface is %.5f -- "
		"spawn markers are FEET and the warp adds a %.4f m capsule half-extent at "
		"arrival, so this drops the player through the slab or into the ceiling",
		(double)xFeet.y, (double)xFloor.Max().y,
		(double)ProfLabPlayerHalfExtent());

	// (4) CAMERA CLEARANCE. The fixed-yaw follow camera trails toward -Z, so the
	//     arrival marker has to leave a whole arm's length of room BEHIND the
	//     player inside the shell. Run the SHIPPED ComputeDesiredPosition rather
	//     than re-deriving the offset, then require the resulting point to land
	//     strictly inside the floor's footprint. Slide the spawn toward the back
	//     wall and this is the clause that reds.
	const Zenith_Maths::Vector3 xCamera =
		ZM_FollowCamera::ComputeDesiredPosition(
			ProfLabSpawnCentre(), fZM_PROFLAB_CAMERA_YAW);
	ZENITH_ASSERT_GT(xCamera.z - xFloor.Min().z, fPROFLAB_CAMERA_FOOTPRINT_MARGIN,
		"the trailing camera lands at z=%.4f, which is not %.2f m clear of the "
		"floor's -Z edge (z=%.4f) -- the follow camera would be authored outside "
		"the room it is meant to look into",
		(double)xCamera.z, (double)fPROFLAB_CAMERA_FOOTPRINT_MARGIN,
		(double)xFloor.Min().z);
	ZENITH_ASSERT_GT(xFloor.Max().z - xCamera.z, fPROFLAB_CAMERA_FOOTPRINT_MARGIN,
		"the trailing camera lands at z=%.4f, past the floor's +Z edge (z=%.4f)",
		(double)xCamera.z, (double)xFloor.Max().z);
	ZENITH_ASSERT_GT(xCamera.x - xFloor.Min().x, fPROFLAB_CAMERA_FOOTPRINT_MARGIN,
		"the trailing camera lands at x=%.4f, outside the floor's -X edge (x=%.4f)",
		(double)xCamera.x, (double)xFloor.Min().x);
	ZENITH_ASSERT_GT(xFloor.Max().x - xCamera.x, fPROFLAB_CAMERA_FOOTPRINT_MARGIN,
		"the trailing camera lands at x=%.4f, outside the floor's +X edge (x=%.4f)",
		(double)xCamera.x, (double)xFloor.Max().x);
}

// ============================================================================
// U4 -- the camera, and the direction the entrance faces.
// ============================================================================

// ZM-D-173, restated for an interior: the follow camera keeps ONE authored yaw
// for the whole scene and trails toward -Z at that heading, so the doorway has to
// be the +Z face and the room has to open BEHIND the arriving player. Every
// clearance figure in ZM_ProfLabPlacement.h is derived at this yaw and this arm,
// which is why clauses (1a)..(1c) assert the header's THREE mirrored camera
// constants against ZM_FollowCamera's own getters instead of trusting a comment,
// why clause (5) pins the authored camera entity to the point the shipped spring
// actually settles on, and why clause (6) RUNS
// ZM_GetProfLabCameraBackClearance() rather than leaving it as a claim.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_FollowCameraTrailsIntoTheRoomAtTheAuthoredYaw)
{
	// (1a) The header's mirrored arm IS the shipped arm. Re-tune ZM_FollowCamera
	//      without re-deriving the placement and this reds immediately, rather
	//      than the room quietly becoming too short two commits later.
	ZENITH_ASSERT_EQ_FLOAT(fZM_PROFLAB_CAMERA_ARM,
		ZM_FollowCamera::GetArmLength(), fPROFLAB_EXACT_EPSILON,
		"the placement header mirrors a %.4f m camera arm but ZM_FollowCamera "
		"ships %.4f m -- every clearance figure in ZM_ProfLabPlacement.h was "
		"derived at the header's value",
		(double)fZM_PROFLAB_CAMERA_ARM, (double)ZM_FollowCamera::GetArmLength());

	// (1b) ...and so is the mirrored height. The authoring writes it straight
	//      into the scene as the camera entity's Y (ZM_GetProfLabCameraPosition),
	//      so a re-tune here moves the authored camera and nothing else would say
	//      so. The header's comment claims all THREE camera mirrors are asserted;
	//      this clause and (1c) are the two that used to be missing.
	ZENITH_ASSERT_EQ_FLOAT(fZM_PROFLAB_CAMERA_HEIGHT,
		ZM_FollowCamera::GetCameraHeight(), fPROFLAB_EXACT_EPSILON,
		"the placement header mirrors a %.4f m camera height but ZM_FollowCamera "
		"ships %.4f m -- the authored ProfLab camera entity is written at the "
		"header's value",
		(double)fZM_PROFLAB_CAMERA_HEIGHT,
		(double)ZM_FollowCamera::GetCameraHeight());

	// (1c) ...and the mirrored FOV. ZM_FollowCamera OVERWRITES the camera
	//      component's FOV with GetFOVDegrees() on its first late update, so a
	//      divergence here would make the authored .zscen bytes a lie that the
	//      running game silently corrects -- exactly the drift no scene-content
	//      clause can see.
	ZENITH_ASSERT_EQ_FLOAT(fZM_PROFLAB_CAMERA_FOV_DEGREES,
		ZM_FollowCamera::GetFOVDegrees(), fPROFLAB_EXACT_EPSILON,
		"the placement header mirrors a %.4f degree FOV but ZM_FollowCamera "
		"ships %.4f -- the authored ProfLab camera would be overwritten by the "
		"component on its first frame",
		(double)fZM_PROFLAB_CAMERA_FOV_DEGREES,
		(double)ZM_FollowCamera::GetFOVDegrees());

	// (2) The authored yaw is the heading those figures were derived at.
	const Zenith_Maths::Vector3 xCentre = ProfLabSpawnCentre();
	const Zenith_Maths::Vector3 xCamera =
		ZM_FollowCamera::ComputeDesiredPosition(xCentre, fZM_PROFLAB_CAMERA_YAW);
	const float fTrailX = xCamera.x - xCentre.x;
	const float fTrailZ = xCamera.z - xCentre.z;
	ZENITH_ASSERT_EQ_FLOAT(fTrailX, 0.0f, fPROFLAB_EXACT_EPSILON,
		"at the authored yaw the camera trails %.5f m sideways; the interior's "
		"clearances assume a purely axial trail", (double)fTrailX);

	// (3) ★ THE ENTRANCE FACES -Z. Stated as the property that actually matters:
	//     at the authored yaw the camera goes BEHIND the arriving player in the
	//     -Z direction, by the full arm. That is what forces the doorway onto the
	//     +Z face and the room onto the -Z side of the spawn.
	ZENITH_ASSERT_LT(fTrailZ, 0.0f,
		"at the authored yaw the camera trails toward +Z (%.5f m), i.e. out "
		"through the doorway -- the entrance no longer faces -Z and the arriving "
		"player would be filmed from outside the room", (double)fTrailZ);
	ZENITH_ASSERT_EQ_FLOAT(-fTrailZ, fZM_PROFLAB_CAMERA_ARM,
		fPROFLAB_EXACT_EPSILON,
		"the camera trails %.5f m toward -Z but the mirrored arm is %.5f m -- the "
		"authored yaw is not the axial heading the placement assumes",
		(double)(-fTrailZ), (double)fZM_PROFLAB_CAMERA_ARM);

	// ANTI-VACUITY for clause 3, and the reason it is not merely a restatement of
	// "yaw is zero". Feed the OPPOSITE heading to the same shipped function: the
	// camera must then trail the other way. Without this, a ComputeDesiredPosition
	// that had lost its yaw term entirely would satisfy every clause above.
	const Zenith_Maths::Vector3 xReversed =
		ZM_FollowCamera::ComputeDesiredPosition(
			xCentre, fZM_PROFLAB_CAMERA_YAW + fPROFLAB_HALF_TURN_RADIANS);
	ZENITH_ASSERT_GT(xReversed.z - xCentre.z, 0.0f,
		"reversing the heading does not reverse the trail (%.5f m), so clause 3 "
		"proves nothing about the authored yaw", (double)(xReversed.z - xCentre.z));

	// (4) ...and nothing solid stands between the player's pivot and that camera.
	//     Runs the SHIPPED clamp against the SHIPPED blockout, so a shortened
	//     room lands here rather than in a comment. The floor is excluded on
	//     purpose: the arm rises from a pivot above the slab and the slab is the
	//     surface the player stands on, so a "hit" on it would be the ray leaving
	//     the room downward, not an obstruction.
	const Zenith_Maths::Vector3 xPivot = xCentre
		+ Zenith_Maths::Vector3(0.0f, ZM_FollowCamera::GetPivotHeight(), 0.0f);
	const Zenith_Maths::Vector3 xArm = xCamera - xPivot;
	const float fDesiredArm = std::sqrt(
		xArm.x * xArm.x + xArm.y * xArm.y + xArm.z * xArm.z);
	ZENITH_ASSERT_GT(fDesiredArm, fPROFLAB_EXACT_EPSILON,
		"the authored camera arm is degenerate (%.6f m)", (double)fDesiredArm);
	const Zenith_Maths::Vector3 xDirection = xArm / fDesiredArm;

	bool bHit = false;
	float fHitDistance = fDesiredArm;
	const char* szBlocker = "none";
	for (u_int u = 0u; u < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++u)
	{
		const ZM_PROFLAB_BLOCK eBlock = (ZM_PROFLAB_BLOCK)u;
		if (eBlock == ZM_PROFLAB_BLOCK_FLOOR)
		{
			continue;
		}
		const float fEntry = RayAabbEntryDistance(
			xPivot, xDirection, fDesiredArm, ZM_GetProfLabBlock(eBlock));
		if (fEntry >= 0.0f && fEntry < fHitDistance)
		{
			bHit = true;
			fHitDistance = fEntry;
			szBlocker = ZM_GetProfLabBlockName(eBlock);
		}
	}

	const float fClamped =
		ZM_FollowCamera::ClampArmDistance(fDesiredArm, bHit, fHitDistance);
	ZENITH_ASSERT_GE(fClamped, fDesiredArm - fPROFLAB_PLANE_EPSILON,
		"the authored camera arm clamps from %.4f m to %.4f m -- '%s' stands "
		"between the arriving player and the camera at %.4f m, so the room is "
		"too short behind the Door spawn for the fixed-yaw follow camera",
		(double)fDesiredArm, (double)fClamped,
		szBlocker != nullptr ? szBlocker : "(null)", (double)fHitDistance);

	// (5) ★ THE AUTHORED CAMERA ENTITY vs THE POINT THE SPRING SETTLES ON. The
	//     authoring writes ZM_GetProfLabCameraPosition() into ProfLab.zscen; the
	//     running game writes ComputeDesiredPosition(bodyCentre, yaw) over it on
	//     the first late update after the scene loads (ZM_FollowCamera::OnStart
	//     clears the spring, so that first update SNAPS rather than eases). The
	//     relationship between those two points is a PROPERTY, not a comment:
	//     identical in X and Z, and offset in Y by exactly the body centre's
	//     height, because the header measures its camera height from the FLOOR
	//     while ComputeDesiredPosition measures it from the player's CENTRE.
	const Zenith_Maths::Vector3 xAuthoredCamera = ZM_GetProfLabCameraPosition();
	ZENITH_ASSERT_EQ_FLOAT(xAuthoredCamera.x, xCamera.x, fPROFLAB_EXACT_EPSILON,
		"the authored camera entity sits at x=%.5f but the spring settles at "
		"x=%.5f -- the authored pose is off the settled ray in plan view",
		(double)xAuthoredCamera.x, (double)xCamera.x);
	ZENITH_ASSERT_EQ_FLOAT(xAuthoredCamera.z, xCamera.z, fPROFLAB_EXACT_EPSILON,
		"the authored camera entity sits at z=%.5f but the spring settles at "
		"z=%.5f -- the authored arm is not the arm the component computes",
		(double)xAuthoredCamera.z, (double)xCamera.z);
	ZENITH_ASSERT_EQ_FLOAT(xCamera.y - xAuthoredCamera.y, xCentre.y,
		fPROFLAB_PLANE_EPSILON,
		"the settled camera sits %.5f m above the authored one; the difference "
		"must be exactly the body centre's height (%.5f m), because the header "
		"authors its camera height above the FLOOR and "
		"ZM_FollowCamera::ComputeDesiredPosition adds its own height to the "
		"player's CENTRE. Any other gap means one of the two conventions moved",
		(double)(xCamera.y - xAuthoredCamera.y), (double)xCentre.y);

	// (6) ...and the arithmetic ZM_GetProfLabCameraBackClearance computes is
	//     actually RUN here rather than left as a claim the header makes. The
	//     floor it has to clear is the player radius plus the follow camera's own
	//     collision padding: below that, the arm clamp pulls the camera off the
	//     authored pose the moment the player arrives.
	const float fBackClearance = ZM_GetProfLabCameraBackClearance();
	const float fRequiredClearance =
		fZM_PROFLAB_PLAYER_RADIUS + ZM_FollowCamera::GetCollisionPadding();
	ZENITH_ASSERT_GT(fBackClearance, fRequiredClearance,
		"the authored camera column leaves %.4f m between itself and the back "
		"wall's inner face (z=%.4f), but it needs more than %.4f m (a %.4f m "
		"body radius plus ZM_FollowCamera's %.4f m collision padding) -- below "
		"that the arm clamp constrains the camera on arrival and the authored "
		"pose is a lie",
		(double)fBackClearance, (double)fZM_PROFLAB_INNER_MIN_Z,
		(double)fRequiredClearance, (double)fZM_PROFLAB_PLAYER_RADIUS,
		(double)ZM_FollowCamera::GetCollisionPadding());
}

// ============================================================================
// U5 -- the tag, spelled ONCE.
// ============================================================================

// ★ THE UNIT THAT MAKES THE SHARED CONSTANT LOAD-BEARING. The authoring code
// installs szZM_PROFLAB_SPAWN_TAG on the Door marker and the warp validator
// compares the incoming tag against ZM_WorldSpec's offered list. If those two
// strings were typed twice they could diverge by one byte, IsWarpDestinationValid
// would reject the arrival, and the only symptom would be a warp that never
// completes. Asserted THROUGH the header constant so the divergence is impossible
// to introduce without redding here.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_HeaderSpawnTagMatchesTheWorldSpecRow)
{
	const ZM_WorldSpec& xSpec = ZM_GetWorldSpec(ZM_SCENE_PROFLAB);

	ZENITH_ASSERT_NOT_NULL(szZM_PROFLAB_SPAWN_TAG,
		"the placement header's mirrored spawn tag is null");
	ZENITH_ASSERT_TRUE(ZM_SpawnPoint::IsTagValid(szZM_PROFLAB_SPAWN_TAG),
		"the mirrored spawn tag '%s' is not a tag ZM_SpawnPoint::SetTag would "
		"accept, so the authoring step would assert during the boot that writes "
		"ProfLab.zscen", szZM_PROFLAB_SPAWN_TAG);

	ZENITH_ASSERT_EQ(xSpec.m_uSpawnTagCount, 1u,
		"ProfLab offers %u spawn tags; this unit's byte-identity claim assumes "
		"the single arrival marker", xSpec.m_uSpawnTagCount);
	if (xSpec.m_uSpawnTagCount >= 1u && xSpec.m_pszSpawnTags != nullptr)
	{
		// BYTE-IDENTICAL, not merely "both valid".
		ZENITH_ASSERT_STREQ(szZM_PROFLAB_SPAWN_TAG, xSpec.m_pszSpawnTags[0],
			"the placement header mirrors spawn tag '%s' but the world table "
			"offers '%s' -- RequestWarp would be rejected by "
			"IsWarpDestinationValid, or accepted and then parked forever in "
			"WAITING_FOR_SPAWN because no marker carries the tag it wants",
			szZM_PROFLAB_SPAWN_TAG, xSpec.m_pszSpawnTags[0]);
	}

	// ...and the INBOUND edge names the same string. This is the half that keeps
	// Dawnmere's door honest: the connection Dawnmere declares to ProfLab is what
	// the exit trigger will one day be configured against.
	bool bFound = false;
	const ZM_WorldSpec& xDawnmere = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE);
	for (u_int uConn = 0u; uConn < xDawnmere.m_uConnectionCount; ++uConn)
	{
		const ZM_SceneConnection& xConn = xDawnmere.m_pxConnections[uConn];
		if (xConn.m_eTarget != ZM_SCENE_PROFLAB)
		{
			continue;
		}
		bFound = true;
		ZENITH_ASSERT_STREQ(xConn.m_szSpawnTag, szZM_PROFLAB_SPAWN_TAG,
			"Dawnmere's edge into ProfLab targets spawn tag '%s' but the "
			"placement header authors '%s'",
			xConn.m_szSpawnTag != nullptr ? xConn.m_szSpawnTag : "(null)",
			szZM_PROFLAB_SPAWN_TAG);
	}
	ZENITH_ASSERT_TRUE(bFound,
		"no Dawnmere connection targets ProfLab, so nothing in the compiled world "
		"can ever warp into the interior this sub-commit authors");

	// The tag has to survive the fixed-capacity copy ZM_SpawnPoint::SetTag makes.
	u_int uLength = 0u;
	while (uLength < ZM_SpawnPoint::uTAG_CAPACITY
		&& szZM_PROFLAB_SPAWN_TAG[uLength] != '\0')
	{
		++uLength;
	}
	ZENITH_ASSERT_LT(uLength, ZM_SpawnPoint::uTAG_CAPACITY,
		"the mirrored spawn tag is %u bytes and would be truncated into "
		"ZM_SpawnPoint's %u-byte buffer", uLength, ZM_SpawnPoint::uTAG_CAPACITY);
}

// ============================================================================
// U6 -- the doorway admits the player who arrives through it.
// ============================================================================

// The aperture is an ABSENCE of geometry -- the gap between the two front-wall
// stubs, capped by the lintel -- so its clear size is measured from the boxes
// that bound it rather than carried as its own constant. Widen either stub until
// they meet and this is what says the doorway has been walled up.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_DoorApertureAdmitsTheAuthoredPlayer)
{
	const ZM_ProfLabBlockout xLeft =
		ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FRONT_LEFT);
	const ZM_ProfLabBlockout xRight =
		ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FRONT_RIGHT);
	const ZM_ProfLabBlockout xFloor = ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FLOOR);

	// The two stubs must stand on ONE plane, or "the gap between them" is a
	// diagonal slot rather than a doorway.
	ZENITH_ASSERT_EQ_FLOAT(xLeft.m_xCenter.z, xRight.m_xCenter.z,
		fPROFLAB_EXACT_EPSILON,
		"the doorway's two front stubs sit on different Z planes (%.4f vs %.4f)",
		(double)xLeft.m_xCenter.z, (double)xRight.m_xCenter.z);

	// ...and the left stub really is the -X one, so the width below is a gap and
	// not a signed accident.
	ZENITH_ASSERT_LT(xLeft.m_xCenter.x, xRight.m_xCenter.x,
		"the front-left stub (x=%.4f) is not on the -X side of the front-right "
		"stub (x=%.4f) -- the aperture width below would be measured backwards",
		(double)xLeft.m_xCenter.x, (double)xRight.m_xCenter.x);

	const float fClearWidth = ProfLabApertureClearWidth();
	const float fRequiredWidth = fZM_HUMAN_BODY_FOOTPRINT
		+ 2.0f * fPROFLAB_APERTURE_SIDE_MARGIN;
	ZENITH_ASSERT_GT(fClearWidth, fRequiredWidth,
		"the doorway's clear width is %.4f m; the authored player is %.4f m wide "
		"and needs %.2f m beside each shoulder (%.4f m total) -- widen a front "
		"stub any further and the interior has no usable exit",
		(double)fClearWidth, (double)fZM_HUMAN_BODY_FOOTPRINT,
		(double)fPROFLAB_APERTURE_SIDE_MARGIN, (double)fRequiredWidth);

	// The gap has to be where the player actually is, not merely somewhere along
	// the wall. Both shoulders must clear both stubs at the spawn's X column.
	const float fRadius = fZM_PROFLAB_PLAYER_RADIUS;
	ZENITH_ASSERT_GT(fZM_PROFLAB_SPAWN_X - xLeft.Max().x, fRadius,
		"the Door spawn's X column (%.4f) is within the %.4f m body radius of the "
		"front-left stub's inner face (%.4f) -- the player arrives clipping the "
		"jamb", (double)fZM_PROFLAB_SPAWN_X, (double)fRadius,
		(double)xLeft.Max().x);
	ZENITH_ASSERT_GT(xRight.Min().x - fZM_PROFLAB_SPAWN_X, fRadius,
		"the Door spawn's X column (%.4f) is within the %.4f m body radius of the "
		"front-right stub's inner face (%.4f)",
		(double)fZM_PROFLAB_SPAWN_X, (double)fRadius, (double)xRight.Min().x);

	// Headroom. The lintel bridges the opening; its underside must clear a
	// standing player rather than the doorway's own width.
	const float fClearHeight = ProfLabApertureClearHeight();
	const float fRequiredHeight =
		fZM_HUMAN_BODY_HEIGHT + fPROFLAB_APERTURE_HEAD_MARGIN;
	ZENITH_ASSERT_GT(fClearHeight, fRequiredHeight,
		"the doorway's clear height is %.4f m (floor surface %.4f to lintel "
		"underside %.4f); the authored player stands %.4f m and needs %.2f m of "
		"headroom", (double)fClearHeight, (double)xFloor.Max().y,
		(double)ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_LINTEL).Min().y,
		(double)fZM_HUMAN_BODY_HEIGHT,
		(double)fPROFLAB_APERTURE_HEAD_MARGIN);

	// The lintel must actually SPAN the gap it caps, or the "clear height" above
	// is measured against a beam standing beside the doorway.
	const ZM_ProfLabBlockout xLintel =
		ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_LINTEL);
	ZENITH_ASSERT_LE(xLintel.Min().x, xLeft.Max().x + fPROFLAB_PLANE_EPSILON,
		"the lintel starts at x=%.4f, inside the opening whose -X jamb ends at "
		"%.4f -- it does not bridge the doorway",
		(double)xLintel.Min().x, (double)xLeft.Max().x);
	ZENITH_ASSERT_GE(xLintel.Max().x, xRight.Min().x - fPROFLAB_PLANE_EPSILON,
		"the lintel ends at x=%.4f, short of the +X jamb at %.4f",
		(double)xLintel.Max().x, (double)xRight.Min().x);
}

// ============================================================================
// U7 -- the names the shell walk looks up.
// ============================================================================

// ZM_ProfLabWarp_Test's clause I walks [0, ZM_PROFLAB_BLOCK_COUNT) and resolves
// each block through Zenith_SceneData::FindEntityByName. That walk can only be
// as trustworthy as its keys: a duplicated name makes two rows resolve to one
// entity (so a genuinely misplaced block is compared against its twin and
// passes), and a name carrying a space or a control byte is a silent miss the
// automated test would report as "entity not found" with no cause.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_BlockNamesAreUniquePrintableLookupKeys)
{
	for (u_int u = 0u; u < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++u)
	{
		const ZM_PROFLAB_BLOCK eBlock = (ZM_PROFLAB_BLOCK)u;
		const char* szName = ZM_GetProfLabBlockName(eBlock);
		ZENITH_ASSERT_TRUE(ProfLabNameIsALookupKey(szName),
			"block %u's name is null, empty, or carries a non-printable byte -- "
			"FindEntityByName would miss it and the shell walk would report a "
			"missing entity rather than the real cause", u);
	}

	for (u_int uA = 0u; uA < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++uA)
	{
		for (u_int uB = uA + 1u; uB < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++uB)
		{
			const char* szA = ZM_GetProfLabBlockName((ZM_PROFLAB_BLOCK)uA);
			const char* szB = ZM_GetProfLabBlockName((ZM_PROFLAB_BLOCK)uB);
			if (szA == nullptr || szB == nullptr)
			{
				continue;   // already reported above
			}
			ZENITH_ASSERT_NE(std::strcmp(szA, szB), 0,
				"blocks %u and %u share the entity name '%s' -- the shell walk "
				"would resolve both rows to one entity, so a misplaced block "
				"would be compared against its twin and pass", uA, uB, szA);
		}
	}
}

// ============================================================================
// U8 -- the shell is closed except at the doorway.
// ============================================================================

// The four bounding sides must actually reach the edges of the slab they stand
// on, and stand tall enough that a player cannot see or step over them. A wall
// authored one scale component short leaves a corner gap that no unit measuring
// only the doorway would ever notice.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_ShellEnclosesTheFloorOnEverySide)
{
	const ZM_ProfLabBlockout xFloor = ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FLOOR);
	const ZM_ProfLabBlockout xBack = ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_BACK_WALL);
	const ZM_ProfLabBlockout xLeftWall =
		ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_LEFT_WALL);
	const ZM_ProfLabBlockout xRightWall =
		ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_RIGHT_WALL);
	const ZM_ProfLabBlockout xFrontLeft =
		ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FRONT_LEFT);
	const ZM_ProfLabBlockout xFrontRight =
		ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FRONT_RIGHT);

	// The back wall is the -Z one and the front pair the +Z one. Derived rather
	// than assumed, because every "faces -Z" claim in this file depends on it.
	ZENITH_ASSERT_LT(xBack.m_xCenter.z, xFloor.m_xCenter.z,
		"the back wall (z=%.4f) is not on the -Z side of the floor (z=%.4f) -- "
		"the room would open away from the trailing camera",
		(double)xBack.m_xCenter.z, (double)xFloor.m_xCenter.z);
	ZENITH_ASSERT_GT(xFrontLeft.m_xCenter.z, xFloor.m_xCenter.z,
		"the doorway wall (z=%.4f) is not on the +Z side of the floor (z=%.4f)",
		(double)xFrontLeft.m_xCenter.z, (double)xFloor.m_xCenter.z);

	// ...and the doorway plane is the floor's +Z face, so "the entrance is the
	// +Z wall" is geometry rather than a coincidence that survives a resize.
	ZENITH_ASSERT_EQ_FLOAT(xFrontLeft.m_xCenter.z, xFloor.Max().z,
		fPROFLAB_PLANE_EPSILON,
		"the doorway plane (z=%.4f) does not coincide with the floor's +Z face "
		"(z=%.4f)", (double)xFrontLeft.m_xCenter.z, (double)xFloor.Max().z);

	// The side walls span the slab's full Z extent, and the back wall plus the
	// two doorway stubs span its full X extent, so no corner gap opens.
	ZENITH_ASSERT_LE(xBack.Min().x, xFloor.Min().x + fPROFLAB_PLANE_EPSILON,
		"the back wall starts at x=%.4f, inside the floor's -X edge (%.4f)",
		(double)xBack.Min().x, (double)xFloor.Min().x);
	ZENITH_ASSERT_GE(xBack.Max().x, xFloor.Max().x - fPROFLAB_PLANE_EPSILON,
		"the back wall ends at x=%.4f, short of the floor's +X edge (%.4f)",
		(double)xBack.Max().x, (double)xFloor.Max().x);
	ZENITH_ASSERT_LE(xLeftWall.Min().z, xFloor.Min().z + fPROFLAB_PLANE_EPSILON,
		"the -X wall starts at z=%.4f, inside the floor's -Z edge (%.4f)",
		(double)xLeftWall.Min().z, (double)xFloor.Min().z);
	ZENITH_ASSERT_GE(xLeftWall.Max().z, xFloor.Max().z - fPROFLAB_PLANE_EPSILON,
		"the -X wall ends at z=%.4f, short of the floor's +Z edge (%.4f)",
		(double)xLeftWall.Max().z, (double)xFloor.Max().z);
	ZENITH_ASSERT_LE(xRightWall.Min().z, xFloor.Min().z + fPROFLAB_PLANE_EPSILON,
		"the +X wall starts at z=%.4f, inside the floor's -Z edge (%.4f)",
		(double)xRightWall.Min().z, (double)xFloor.Min().z);
	ZENITH_ASSERT_GE(xRightWall.Max().z, xFloor.Max().z - fPROFLAB_PLANE_EPSILON,
		"the +X wall ends at z=%.4f, short of the floor's +Z edge (%.4f)",
		(double)xRightWall.Max().z, (double)xFloor.Max().z);
	ZENITH_ASSERT_LE(xFrontLeft.Min().x, xFloor.Min().x + fPROFLAB_PLANE_EPSILON,
		"the doorway's -X stub starts at x=%.4f, inside the floor's -X edge "
		"(%.4f) -- a gap opens beside the jamb",
		(double)xFrontLeft.Min().x, (double)xFloor.Min().x);
	ZENITH_ASSERT_GE(xFrontRight.Max().x, xFloor.Max().x - fPROFLAB_PLANE_EPSILON,
		"the doorway's +X stub ends at x=%.4f, short of the floor's +X edge "
		"(%.4f) -- a gap opens beside the jamb",
		(double)xFrontRight.Max().x, (double)xFloor.Max().x);

	// Every bounding side rises at least a standing player above the floor.
	const ZM_PROFLAB_BLOCK aeSides[] = {
		ZM_PROFLAB_BLOCK_BACK_WALL,
		ZM_PROFLAB_BLOCK_LEFT_WALL,
		ZM_PROFLAB_BLOCK_RIGHT_WALL,
		ZM_PROFLAB_BLOCK_FRONT_LEFT,
		ZM_PROFLAB_BLOCK_FRONT_RIGHT,
	};
	const u_int uSideCount = (u_int)(sizeof(aeSides) / sizeof(aeSides[0]));
	for (u_int uSide = 0u; uSide < uSideCount; ++uSide)
	{
		const ZM_ProfLabBlockout xSide = ZM_GetProfLabBlock(aeSides[uSide]);
		const float fRise = xSide.Max().y - xFloor.Max().y;
		ZENITH_ASSERT_GE(fRise, fZM_HUMAN_BODY_HEIGHT,
			"'%s' rises only %.4f m above the floor; the authored player is "
			"%.4f m tall and would see (and step) over it",
			ZM_GetProfLabBlockName(aeSides[uSide]), (double)fRise,
			(double)fZM_HUMAN_BODY_HEIGHT);
	}
}
