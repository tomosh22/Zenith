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
// ★ WHAT THESE FIFTEEN UNITS CANNOT DO, STATED UP FRONT. They run BEFORE the
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
#include "Zenithmon/Components/ZM_PlayerController.h"    // fRUN_SPEED -- the sensor-depth clause's per-frame travel
#include "Zenithmon/Components/ZM_SpawnPoint.h"          // IsTagValid / uTAG_CAPACITY -- the tag contract
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"   // the SHIPPED candidate picker -- run, not re-derived
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

	// ========================================================================
	// U9..U11 support: the ARRIVAL FRUSTUM, and Professor Aster's box.
	// ========================================================================

	// How far inside the frustum's edge a point has to land before this file will
	// call it "on screen". Aster's own half-width, deliberately: a figure whose
	// centre is exactly on the frustum edge is half off the side of the picture,
	// which is not what "the player sees him when they arrive" means.
	constexpr float fPROFLAB_ASTER_FRUSTUM_MARGIN = fZM_HUMAN_BODY_FOOTPRINT * 0.5f;

	// ★ THE COUNTEREXAMPLE, AND THE ONLY REASON THESE TWO LITERALS ARE HERE. An
	// earlier draft of this placement proposed roughly this anchor for Aster; it is
	// about 71.6 degrees off the settled camera's plan-view axis against a ~48.5
	// degree horizontal half-angle at 16:9, i.e. the player warps in and sees an
	// empty room. It is fed through the SAME predicate as the shipped anchor by
	// U9's anti-vacuity clause, which is what proves that clause can red at all.
	// These are NOT shipped constants and nothing outside this file may read them.
	constexpr float fPROFLAB_REJECTED_ASTER_X = -4.5f;
	constexpr float fPROFLAB_REJECTED_ASTER_Z = 1.0f;

	// Hand-rolled vector arithmetic, matching this file's existing slab test rather
	// than reaching for glm: these units are the pure half of the argument and
	// every step of the projection should be readable as arithmetic.
	float ProfLabDot(
		const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		return xA.x * xB.x + xA.y * xB.y + xA.z * xB.z;
	}

	Zenith_Maths::Vector3 ProfLabCross(
		const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		return Zenith_Maths::Vector3(
			xA.y * xB.z - xA.z * xB.y,
			xA.z * xB.x - xA.x * xB.z,
			xA.x * xB.y - xA.y * xB.x);
	}

	Zenith_Maths::Vector3 ProfLabNormalise(const Zenith_Maths::Vector3& xVector)
	{
		const float fLength = std::sqrt(ProfLabDot(xVector, xVector));
		if (!(fLength > 1.0e-6f))
		{
			return Zenith_Maths::Vector3(0.0f);   // TOTAL: never a NaN
		}
		return xVector / fLength;
	}

	// The camera basis the arriving player is filmed through. ★ IT IS BUILT FROM
	// THE LOOK-AT GEOMETRY, NOT FROM A PLAN-VIEW BEARING. The settled camera is
	// pitched down onto the player's chest, so "how many degrees off +Z is he?" --
	// the number an eyeball estimate reaches for -- is not the quantity the
	// perspective divide compares. Forward is the unit vector from the settled
	// position to the pivot (the two points ZM_FollowCamera::BuildLookAtPose is
	// handed), right is horizontal because the camera carries no roll, and up
	// closes the frame.
	struct ProfLabArrivalFrustum
	{
		Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xForward = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xRight = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xUp = Zenith_Maths::Vector3(0.0f);
		float m_fTanHalfVerticalFOV = 0.0f;
	};

	ProfLabArrivalFrustum ProfLabBuildArrivalFrustum()
	{
		ProfLabArrivalFrustum xFrustum;
		xFrustum.m_xPosition = ZM_GetProfLabSettledCameraPosition();
		xFrustum.m_xForward = ProfLabNormalise(
			ZM_GetProfLabArrivalPivot() - xFrustum.m_xPosition);
		xFrustum.m_xRight = ProfLabNormalise(ProfLabCross(
			xFrustum.m_xForward, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
		xFrustum.m_xUp = ProfLabCross(xFrustum.m_xRight, xFrustum.m_xForward);
		// Zenith_CameraComponent feeds m_fFOV straight to glm::perspective, whose
		// first argument is the VERTICAL field of view -- so the half-angle below is
		// the vertical one and the horizontal is the aspect-widened version of it.
		xFrustum.m_fTanHalfVerticalFOV = std::tan(
			glm::radians(fZM_PROFLAB_CAMERA_FOV_DEGREES) * 0.5f);
		return xFrustum;
	}

	// One point, measured in that camera's space. Every field is reported rather
	// than reduced to a bool so a failure says HOW far outside the point landed.
	struct ProfLabFrustumSample
	{
		float m_fDepth = 0.0f;            // along forward; the perspective divide's denominator
		float m_fLateral = 0.0f;          // |camera-space X|
		float m_fVertical = 0.0f;         // |camera-space Y|
		float m_fLateralLimit = 0.0f;     // the half-width at the mirrored 16:9 aspect
		float m_fSquareLimit = 0.0f;      // ...and at aspect 1.0, the narrowest any landscape window gives
		float m_fVerticalLimit = 0.0f;
	};

	ProfLabFrustumSample ProfLabProject(
		const ProfLabArrivalFrustum& xFrustum, const Zenith_Maths::Vector3& xPoint)
	{
		const Zenith_Maths::Vector3 xOffset = xPoint - xFrustum.m_xPosition;
		ProfLabFrustumSample xSample;
		xSample.m_fDepth = ProfLabDot(xOffset, xFrustum.m_xForward);
		xSample.m_fLateral = std::fabs(ProfLabDot(xOffset, xFrustum.m_xRight));
		xSample.m_fVertical = std::fabs(ProfLabDot(xOffset, xFrustum.m_xUp));
		const float fHalfHeight = xSample.m_fDepth * xFrustum.m_fTanHalfVerticalFOV;
		xSample.m_fVerticalLimit = fHalfHeight;
		xSample.m_fSquareLimit = fHalfHeight;
		xSample.m_fLateralLimit = fHalfHeight * fZM_PROFLAB_CAMERA_ASPECT;
		return xSample;
	}

	// Aster's authored body, as the box the AABB collider is given at runtime --
	// the COMPILED contract (ZM_HumanBody.h), never the transform scale.
	ZM_ProfLabBlockout ProfLabAsterBody()
	{
		return ZM_ProfLabBlockout{
			ZM_GetProfLabAsterCenter(),
			Zenith_Maths::Vector3(
				fZM_HUMAN_BODY_FOOTPRINT,
				fZM_HUMAN_BODY_HEIGHT,
				fZM_HUMAN_BODY_FOOTPRINT) };
	}

	// The eight corners of that box, so the frustum claim is about the FIGURE and
	// not about an infinitely small point at his navel.
	Zenith_Maths::Vector3 ProfLabAsterBodyCorner(u_int uCorner)
	{
		const ZM_ProfLabBlockout xBody = ProfLabAsterBody();
		const Zenith_Maths::Vector3 xMin = xBody.Min();
		const Zenith_Maths::Vector3 xMax = xBody.Max();
		return Zenith_Maths::Vector3(
			(uCorner & 1u) != 0u ? xMax.x : xMin.x,
			(uCorner & 2u) != 0u ? xMax.y : xMin.y,
			(uCorner & 4u) != 0u ? xMax.z : xMin.z);
	}

	constexpr u_int uPROFLAB_BODY_CORNER_COUNT = 8u;
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
			"not offer -- the warp would pass IsWarpDestinationValid and then stall "
			"in WAITING_FOR_SPAWN for its whole frame budget before erroring out "
			"(ZM-D-200)",
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
			"IsWarpDestinationValid, or accepted and then stalled for the whole "
			"WAITING_FOR_SPAWN budget because no marker carries the tag it wants",
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

// ============================================================================
// U9 -- PROFESSOR ASTER IS ON SCREEN WHEN THE PLAYER ARRIVES.
// ============================================================================

// ★ THE PROPERTY WITH NO OTHER OWNER, AND THE ONE AN EARLIER DRAFT GOT WRONG.
// Every other unit in this file measures the ROOM: the shell encloses, the doorway
// admits, the camera fits. None of them can say whether the thing the player came
// here to meet is in the picture at the moment they arrive, and "the professor is
// in his lab" is satisfied just as well by a professor standing behind the camera.
// So this unit runs the projection: it builds the arrival frustum out of the same
// constants the authoring reads, projects every corner of Aster's authored body
// into it, and requires the whole figure inside by a stated margin.
//
// THREE ARMS, AND THE THIRD IS WHAT MAKES THE FIRST TWO WORTH HAVING:
//   * the mirrored camera constants this frustum is built from ARE the shipped
//     ones (the settled position and the pivot are compared against
//     ZM_FollowCamera's own statics, not assumed);
//   * the eight body corners land inside, at the 16:9 aspect AND -- for the centre
//     -- at aspect 1.0, which is the narrowest horizontal field any landscape
//     window can give and therefore a claim no window shape can invalidate;
//   * the REJECTED anchor, fed through the identical predicate, lands OUTSIDE. A
//     containment test that cannot reject anything proves nothing about the anchor
//     it accepts.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_AsterStandsInsideTheArrivalFrustum)
{
	// (1) THE MIRRORS THIS FRUSTUM IS BUILT FROM. The arm, height and FOV are
	//     already asserted against ZM_FollowCamera by U4 clauses (1a)..(1c); the
	//     PIVOT height is the fourth, and it is the one that decides how far DOWN
	//     the camera is aimed and therefore what the frame contains vertically.
	ZENITH_ASSERT_EQ_FLOAT(fZM_PROFLAB_CAMERA_PIVOT_HEIGHT,
		ZM_FollowCamera::GetPivotHeight(), fPROFLAB_EXACT_EPSILON,
		"the placement header mirrors a %.4f m camera pivot height but "
		"ZM_FollowCamera ships %.4f m -- the arrival frustum below is aimed at the "
		"header's value, so every containment claim in this unit would be measured "
		"through a camera the game does not have",
		(double)fZM_PROFLAB_CAMERA_PIVOT_HEIGHT,
		(double)ZM_FollowCamera::GetPivotHeight());

	// (2) THE SETTLED POSITION IS THE SHIPPED ONE. ZM_GetProfLabSettledCameraPosition
	//     claims to be where ComputeDesiredPosition puts the camera on arrival;
	//     RUN that function rather than trusting the claim.
	const Zenith_Maths::Vector3 xSpawnCentre = ProfLabSpawnCentre();
	const Zenith_Maths::Vector3 xSettled = ZM_GetProfLabSettledCameraPosition();
	const Zenith_Maths::Vector3 xShippedSettled =
		ZM_FollowCamera::ComputeDesiredPosition(
			xSpawnCentre, fZM_PROFLAB_CAMERA_YAW);
	ZENITH_ASSERT_EQ_FLOAT(xSettled.x, xShippedSettled.x, fPROFLAB_EXACT_EPSILON,
		"the header's settled camera sits at x=%.5f but ComputeDesiredPosition "
		"answers x=%.5f", (double)xSettled.x, (double)xShippedSettled.x);
	ZENITH_ASSERT_EQ_FLOAT(xSettled.y, xShippedSettled.y, fPROFLAB_PLANE_EPSILON,
		"the header's settled camera sits at y=%.5f but ComputeDesiredPosition "
		"answers y=%.5f -- the capsule half-extent lift is wrong, so the frustum "
		"below is aimed from the wrong height",
		(double)xSettled.y, (double)xShippedSettled.y);
	ZENITH_ASSERT_EQ_FLOAT(xSettled.z, xShippedSettled.z, fPROFLAB_EXACT_EPSILON,
		"the header's settled camera sits at z=%.5f but ComputeDesiredPosition "
		"answers z=%.5f", (double)xSettled.z, (double)xShippedSettled.z);

	// (3) ...and the PIVOT is the point ZM_FollowCamera aims at: the target's
	//     position lifted by the component's own pivot height.
	const Zenith_Maths::Vector3 xPivot = ZM_GetProfLabArrivalPivot();
	const Zenith_Maths::Vector3 xShippedPivot = xSpawnCentre
		+ Zenith_Maths::Vector3(0.0f, ZM_FollowCamera::GetPivotHeight(), 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xPivot.x, xShippedPivot.x, fPROFLAB_EXACT_EPSILON,
		"the header's arrival pivot sits at x=%.5f, not on the arriving body's "
		"column (%.5f)", (double)xPivot.x, (double)xShippedPivot.x);
	ZENITH_ASSERT_EQ_FLOAT(xPivot.y, xShippedPivot.y, fPROFLAB_PLANE_EPSILON,
		"the header's arrival pivot sits at y=%.5f but ZM_FollowCamera aims at "
		"y=%.5f", (double)xPivot.y, (double)xShippedPivot.y);
	ZENITH_ASSERT_EQ_FLOAT(xPivot.z, xShippedPivot.z, fPROFLAB_EXACT_EPSILON,
		"the header's arrival pivot sits at z=%.5f, not on the arriving body's "
		"column (%.5f)", (double)xPivot.z, (double)xShippedPivot.z);

	const ProfLabArrivalFrustum xFrustum = ProfLabBuildArrivalFrustum();
	ZENITH_ASSERT_GT(xFrustum.m_fTanHalfVerticalFOV, 0.0f,
		"the arrival frustum has a non-positive half-angle (tan = %.6f); every "
		"containment claim below would be vacuous",
		(double)xFrustum.m_fTanHalfVerticalFOV);

	// (4) EVERY CORNER OF THE FIGURE, not just his centre. A margin-free claim
	//     about a point would still pass with his shoulder off the side of frame.
	for (u_int uCorner = 0u; uCorner < uPROFLAB_BODY_CORNER_COUNT; ++uCorner)
	{
		const Zenith_Maths::Vector3 xCorner = ProfLabAsterBodyCorner(uCorner);
		const ProfLabFrustumSample xSample = ProfLabProject(xFrustum, xCorner);

		ZENITH_ASSERT_GT(xSample.m_fDepth, fZM_PROFLAB_CAMERA_NEAR,
			"body corner %u of Professor Aster sits %.4f m along the arrival "
			"camera's forward axis, inside its %.4f m near plane -- he is beside "
			"or BEHIND the camera, which is the failure mode this unit exists for",
			uCorner, (double)xSample.m_fDepth, (double)fZM_PROFLAB_CAMERA_NEAR);
		ZENITH_ASSERT_LT(xSample.m_fDepth, fZM_PROFLAB_CAMERA_FAR,
			"body corner %u sits %.4f m out, past the %.4f m far plane",
			uCorner, (double)xSample.m_fDepth, (double)fZM_PROFLAB_CAMERA_FAR);

		ZENITH_ASSERT_LT(xSample.m_fLateral,
			xSample.m_fLateralLimit - fPROFLAB_ASTER_FRUSTUM_MARGIN,
			"body corner %u of Professor Aster is %.4f m off the arrival camera's "
			"axis where the frame is only %.4f m wide at that depth (%.4f m), "
			"leaving less than the %.2f m margin -- the player warps in and he is "
			"off (or half off) the side of the picture. Pull him DEEPER along the "
			"camera's forward axis rather than sideways: the frame widens linearly "
			"with depth, so depth buys width and a lateral step spends it",
			uCorner, (double)xSample.m_fLateral, (double)xSample.m_fLateralLimit,
			(double)xSample.m_fDepth, (double)fPROFLAB_ASTER_FRUSTUM_MARGIN);
		ZENITH_ASSERT_LT(xSample.m_fVertical,
			xSample.m_fVerticalLimit - fPROFLAB_ASTER_FRUSTUM_MARGIN,
			"body corner %u of Professor Aster is %.4f m off the arrival camera's "
			"axis vertically where the frame is only %.4f m tall at that depth "
			"(%.4f m) -- his head or his feet are out of frame",
			uCorner, (double)xSample.m_fVertical,
			(double)xSample.m_fVerticalLimit, (double)xSample.m_fDepth);
	}

	// (5) ★ THE ASPECT-INDEPENDENT ARM. fZM_PROFLAB_CAMERA_ASPECT is a MIRROR of a
	//     private default no pure unit can read, so clause (4) is only as good as
	//     that mirror. A square viewport is the narrowest horizontal field any
	//     aspect >= 1 can produce, so requiring his centre inside THAT is a claim
	//     the mirror cannot invalidate.
	const ProfLabFrustumSample xCentre =
		ProfLabProject(xFrustum, ZM_GetProfLabAsterCenter());
	ZENITH_ASSERT_LT(xCentre.m_fLateral,
		xCentre.m_fSquareLimit - fPROFLAB_ASTER_FRUSTUM_MARGIN,
		"Professor Aster's centre is %.4f m off the arrival camera's axis, which "
		"clears the %.4f m half-width a 16:9 window gives but NOT the %.4f m a "
		"square one does. Clause (4) above is measured through a MIRRORED aspect "
		"(fZM_PROFLAB_CAMERA_ASPECT); this clause is the one that does not depend "
		"on it, and it is now the binding constraint",
		(double)xCentre.m_fLateral, (double)xCentre.m_fLateralLimit,
		(double)xCentre.m_fSquareLimit);

	// (6) ★ ANTI-VACUITY. The rejected anchor, through the identical predicate.
	//     Without this, a projection that had lost its lateral term entirely -- or
	//     a frustum built with an absurdly wide half-angle -- would satisfy every
	//     clause above and this unit would be decoration.
	const Zenith_Maths::Vector3 xRejected(
		fPROFLAB_REJECTED_ASTER_X,
		ZM_GetProfLabAsterCenter().y,
		fPROFLAB_REJECTED_ASTER_Z);
	const ProfLabFrustumSample xRejectedSample =
		ProfLabProject(xFrustum, xRejected);
	ZENITH_ASSERT_GT(xRejectedSample.m_fLateral, xRejectedSample.m_fLateralLimit,
		"the REJECTED anchor (%.2f, %.2f) projects to %.4f m off axis against a "
		"%.4f m half-width and is therefore reported as ON SCREEN. That anchor is "
		"known to be off screen, so the containment clauses above cannot currently "
		"red and prove nothing about where Aster stands",
		(double)fPROFLAB_REJECTED_ASTER_X, (double)fPROFLAB_REJECTED_ASTER_Z,
		(double)xRejectedSample.m_fLateral,
		(double)xRejectedSample.m_fLateralLimit);
}

// ============================================================================
// U10 -- HE IS SOMEBODY YOU WALK UP TO, AND HE IS INSIDE THE ROOM.
// ============================================================================

// The Dawnmere convention is that talking costs a walk: an NPC standing inside
// reach at the moment the scene hands control back would open his dialogue from
// the doormat, off a stray interact press the player never aimed. That is the
// first half. The second is the ordinary shell hygiene every authored body owes --
// feet on the floor, clear of the walls, not standing on the arrival point, and
// not parked in the camera's arm.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_AsterStandsOutsideArrivalReachInsideTheShell)
{
	const Zenith_Maths::Vector3 xAster = ZM_GetProfLabAsterCenter();
	const Zenith_Maths::Vector3 xSpawnCentre = ProfLabSpawnCentre();

	// (1) ★ RUN THE SHIPPED PICKER, DO NOT RE-DERIVE ITS RULE. A hand-written
	//     "distance > 2.9" here would be a second implementation of
	//     ZM_PickInteractTarget's acceptance test and could agree with the header
	//     while disagreeing with the game. The origin is pointed STRAIGHT AT HIM so
	//     the facing cone can never be the reason -- the answer this asserts is
	//     about RANGE and nothing else.
	ZM_InteractProbe xProbe;
	xProbe.m_xPosition = xAster;
	xProbe.m_fRadius = fZM_PROFLAB_ASTER_REACH_BONUS;
	xProbe.m_bEnabled = true;

	ZM_InteractOrigin xArrivalOrigin;
	xArrivalOrigin.m_xPosition = xSpawnCentre;
	xArrivalOrigin.m_xForward = xAster - xSpawnCentre;

	const ZM_InteractTuning xTuning{};
	u_int uBest = 0u;
	const ZM_INTERACT_REJECT eAtArrival = ZM_PickInteractTarget(
		&xProbe, 1u, xArrivalOrigin, xTuning, uBest);
	ZENITH_ASSERT_EQ((u_int)eAtArrival, (u_int)ZM_INTERACT_REJECT_OUT_OF_RANGE,
		"a player standing on ProfLab's arrival marker and looking straight at "
		"Professor Aster gets '%s' from the shipped picker, not OUT_OF_RANGE. He "
		"stands %.4f m away in XZ against a %.4f m effective reach (%.4f m picker "
		"reach plus his %.4f m authored radius) -- an NPC reachable from the "
		"doormat opens his dialogue before the player has taken a step",
		ZM_InteractRejectName(eAtArrival),
		(double)ZM_GetProfLabAsterArrivalStandoff(),
		(double)fZM_PROFLAB_ASTER_EFFECTIVE_REACH,
		(double)fZM_INTERACT_MAX_DISTANCE,
		(double)fZM_PROFLAB_ASTER_REACH_BONUS);

	// ANTI-VACUITY for (1): the SAME probe and the SAME tuning must accept once
	// the player has walked up. Without this arm, a probe that was silently
	// disabled -- or a tuning with a zero reach -- would satisfy the clause above
	// while proving nothing about the distance.
	ZM_InteractOrigin xWalkedUpOrigin;
	xWalkedUpOrigin.m_xPosition = xAster
		+ ProfLabNormalise(xSpawnCentre - xAster)
			* (fZM_PROFLAB_ASTER_EFFECTIVE_REACH * 0.5f);
	xWalkedUpOrigin.m_xForward = xAster - xWalkedUpOrigin.m_xPosition;
	const ZM_INTERACT_REJECT eWalkedUp = ZM_PickInteractTarget(
		&xProbe, 1u, xWalkedUpOrigin, xTuning, uBest);
	ZENITH_ASSERT_EQ((u_int)eWalkedUp, (u_int)ZM_INTERACT_OK,
		"the same probe and tuning refuse Professor Aster ('%s') even from half "
		"his effective reach, so the OUT_OF_RANGE above says nothing about how far "
		"away he is authored", ZM_InteractRejectName(eWalkedUp));

	// (2) HIS FEET ARE ON THE FLOOR. Authored positions in this game are body
	//     CENTRES; half a body height below one is where his feet are, and that has
	//     to be the floor's top face or he is sunk into the slab / hovering.
	const ZM_ProfLabBlockout xFloor = ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_FLOOR);
	const ZM_ProfLabBlockout xBody = ProfLabAsterBody();
	ZENITH_ASSERT_EQ_FLOAT(xBody.Min().y, xFloor.Max().y, fPROFLAB_PLANE_EPSILON,
		"Professor Aster's feet are at y=%.5f but the floor's top face is %.5f -- "
		"an authored human position is a body CENTRE (ZM_HumanBody.h), so this is "
		"half a body height out somewhere",
		(double)xBody.Min().y, (double)xFloor.Max().y);

	// (3) ...and his whole box is inside the shell's inner faces, not merely
	//     inside the floor's outline.
	ZENITH_ASSERT_GT(xBody.Min().x,
		ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_LEFT_WALL).Max().x,
		"Professor Aster's -X face is at x=%.4f, inside the -X wall's inner face "
		"(%.4f)", (double)xBody.Min().x,
		(double)ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_LEFT_WALL).Max().x);
	ZENITH_ASSERT_LT(xBody.Max().x,
		ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_RIGHT_WALL).Min().x,
		"Professor Aster's +X face is at x=%.4f, inside the +X wall's inner face "
		"(%.4f)", (double)xBody.Max().x,
		(double)ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_RIGHT_WALL).Min().x);
	ZENITH_ASSERT_GT(xBody.Min().z,
		ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_BACK_WALL).Max().z,
		"Professor Aster's -Z face is at z=%.4f, inside the back wall's inner face "
		"(%.4f)", (double)xBody.Min().z,
		(double)ZM_GetProfLabBlock(ZM_PROFLAB_BLOCK_BACK_WALL).Max().z);
	ZENITH_ASSERT_LT(xBody.Max().z, fZM_PROFLAB_INNER_MAX_Z,
		"Professor Aster's +Z face is at z=%.4f, inside the doorway wall's inner "
		"face (%.4f) -- he is standing in the wall he is authored to greet people "
		"in front of", (double)xBody.Max().z, (double)fZM_PROFLAB_INNER_MAX_Z);

	// (4) He is not standing where the arriving PLAYER's capsule materialises. The
	//     reach clause implies this today, but it is a different property (bodies
	//     overlapping, not dialogue firing) and would survive a reach re-tune.
	const float fDeltaX = xAster.x - xSpawnCentre.x;
	const float fDeltaZ = xAster.z - xSpawnCentre.z;
	const float fSeparation = std::sqrt(fDeltaX * fDeltaX + fDeltaZ * fDeltaZ);
	const float fTouching =
		fZM_PROFLAB_PLAYER_RADIUS + fZM_HUMAN_BODY_FOOTPRINT * 0.5f;
	ZENITH_ASSERT_GT(fSeparation, fTouching,
		"Professor Aster stands %.4f m from the arrival point in XZ, inside the "
		"%.4f m at which the arriving player's capsule and his static box are "
		"already in contact -- the warp would drop the player inside him",
		(double)fSeparation, (double)fTouching);

	// (5) ...and he is not parked in the follow camera's arm. RUNS the same slab
	//     test and the same shipped clamp U4 clause (4) uses on the shell, so a
	//     professor who blocked the camera would clamp the arm and pull the view
	//     into the player's back on arrival.
	const Zenith_Maths::Vector3 xPivot = ZM_GetProfLabArrivalPivot();
	const Zenith_Maths::Vector3 xArm = ZM_GetProfLabSettledCameraPosition() - xPivot;
	const float fDesiredArm = std::sqrt(ProfLabDot(xArm, xArm));
	ZENITH_ASSERT_GT(fDesiredArm, fPROFLAB_EXACT_EPSILON,
		"the arrival camera arm is degenerate (%.6f m)", (double)fDesiredArm);
	const float fEntry = RayAabbEntryDistance(
		xPivot, xArm / fDesiredArm, fDesiredArm, xBody);
	const float fClamped = ZM_FollowCamera::ClampArmDistance(
		fDesiredArm, fEntry >= 0.0f, fEntry);
	ZENITH_ASSERT_GE(fClamped, fDesiredArm - fPROFLAB_PLANE_EPSILON,
		"Professor Aster's body stands between the arriving player and the follow "
		"camera: the %.4f m arm clamps to %.4f m against a hit at %.4f m, so the "
		"camera would snap into the player's back the moment the scene loads",
		(double)fDesiredArm, (double)fClamped, (double)fEntry);
}

// ============================================================================
// U11 -- the name Aster is looked up by.
// ============================================================================

// ZM_ProfLabWarp_Test's clause I3 and ZM_CommittedSceneBytes' ProfLab tripwire
// both resolve him BY NAME, and the InteriorTint scan now walks the block names
// specifically so that a HUMAN body can never enter the shell population. All
// three of those depend on his name being a usable key that collides with nothing
// -- a duplicate would make one lookup silently answer with the other entity.
ZENITH_TEST(ZM_WorldTraversal, ProfLab_AsterEntityNameIsAUniqueLookupKey)
{
	ZENITH_ASSERT_TRUE(ProfLabNameIsALookupKey(szZM_PROFLAB_ASTER_ENTITY_NAME),
		"Professor Aster's authored entity name is null, empty, or carries a "
		"non-printable byte -- FindEntityByName would miss it and every clause "
		"that resolves him would report a missing entity rather than the cause");

	for (u_int u = 0u; u < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++u)
	{
		const char* szBlockName = ZM_GetProfLabBlockName((ZM_PROFLAB_BLOCK)u);
		ZENITH_ASSERT_NE(
			std::strcmp(szZM_PROFLAB_ASTER_ENTITY_NAME, szBlockName), 0,
			"Professor Aster is authored under the same entity name as shell block "
			"%u ('%s'). The interior-tint scan walks the block names and would "
			"collect a HUMAN body as though it were a wall -- which on a cold tree "
			"wears the same 'ZM_Greybox' material and would be compared against the "
			"blockout grey", u, szBlockName);
	}

	const char* aszOtherEntities[] = {
		szZM_PROFLAB_SPAWN_ENTITY_NAME,
		szZM_PROFLAB_PLAYER_ENTITY_NAME,
		szZM_PROFLAB_CAMERA_ENTITY_NAME,
	};
	const u_int uOtherCount =
		(u_int)(sizeof(aszOtherEntities) / sizeof(aszOtherEntities[0]));
	for (u_int u = 0u; u < uOtherCount; ++u)
	{
		ZENITH_ASSERT_NE(
			std::strcmp(szZM_PROFLAB_ASTER_ENTITY_NAME, aszOtherEntities[u]), 0,
			"Professor Aster shares the entity name '%s' with another authored "
			"ProfLab entity -- FindEntityByName would resolve both to whichever "
			"the scene stored first", aszOtherEntities[u]);
	}
}

// ============================================================================
// S8 SC-E -- THE FOUR UNITS THE LAB SEAM AND THE PROFESSOR'S FACING ADD.
//
// ★ WHAT THEY ARE, AND WHAT THEY ARE HONESTLY NOT. Every one of them reads
// compiled constants, so they are the CHEAP half of this slice's proof: they can
// say the compiled world table and the compiled geometry agree with one another,
// and they cannot say one byte about whether Dawnmere.zscen or ProfLab.zscen was
// ever re-authored. The expensive halves are named where they live -- clause I4
// and the facing arm of clause I3 in Tests/ZM_AutoTests_ProfLab.cpp (which runs
// with no terrain and never skips, so it is a real CI gate), and
// ZM_CommittedSceneBytes/DawnmereCarriesTheLabSeamMarkerAndTag (a boot unit over
// the committed Dawnmere bytes). Do not read this file's greenness as "the lab
// door works".
// ============================================================================

// ============================================================================
// U12 -- the exit edge is the TABLE's, not this header's.
//
// The mutation this exists for is the one an adversarial reviewer named: a
// configure function that writes SOME OTHER build index (PlayerHome's, say)
// instead of resolving Dawnmere's. That authoring reads
// ZM_GetProfLabExitTargetBuildIndex(), so the way to make that mutation
// unreachable is to prove the resolver answers what the compiled table says --
// and, crucially, that the tag it hands back is one Dawnmere actually OFFERS.
//
// ★ THAT LAST CLAUSE IS THE RECIPROCAL-INVENTORY ONE, and it is the only check in
// this file that could ever have caught the shipped-exit-without-a-marker stall at
// the TABLE level. ZM_GameStateManager::IsWarpDestinationValid asks exactly this
// question of the table and nothing else -- so if it fails here, the warp would
// have been REJECTED at runtime (a visible no-op) rather than accepted into
// WAITING_FOR_SPAWN. Both are defects; this one at least fails immediately,
// rather than after a whole barrier budget (ZM-D-200).
// ============================================================================
ZENITH_TEST(ZM_WorldTraversal, ProfLab_ExitSensorTargetsDawnmereByTheCompiledConnection)
{
	// (1) The edge exists at all. A ProfLab row with no Dawnmere connection is a
	//     room with no way out, and the resolver's sentinel would be authored into
	//     the scene as a warp to a build index no scene holds.
	const ZM_SceneConnection* pxEdge = ZM_GetProfLabExitConnection();
	ZENITH_ASSERT_NOT_NULL(pxEdge,
		"the compiled ZM_SCENE_PROFLAB row carries no connection targeting "
		"ZM_SCENE_DAWNMERE, so ZM_GetProfLabExitConnection() resolved to nullptr. "
		"The authored ProfLabExitTrigger would take the "
		"uZM_PROFLAB_EXIT_TARGET_UNRESOLVED sentinel and the lab would have no exit "
		"-- fix Source/Data/ZM_WorldSpec.cpp, not this test");
	if (pxEdge == nullptr)
	{
		return;   // already FAILED; every clause below would be about a null edge
	}

	// (2) The resolved build index is the TABLE's Dawnmere index, and never the
	//     sentinel. Spelled as ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex on
	//     the expected side too -- a literal 2 here would be a second inventory,
	//     which is the exact defect the header's opening star forbids.
	const u_int uResolved = ZM_GetProfLabExitTargetBuildIndex();
	ZENITH_ASSERT_NE(uResolved, uZM_PROFLAB_EXIT_TARGET_UNRESOLVED,
		"ZM_GetProfLabExitTargetBuildIndex() returned its unresolved sentinel even "
		"though the ProfLab->Dawnmere edge exists -- the resolver and the edge walk "
		"have parted company");
	ZENITH_ASSERT_EQ(uResolved, ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex,
		"the ProfLab exit resolves to build index %u but the compiled world table "
		"puts Dawnmere at %u. The authored exit would fade the player out and load "
		"whatever scene is registered at %u -- or nothing at all",
		uResolved, ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex, uResolved);

	// (3) The tag is a usable ZM_SpawnPoint tag: non-empty, inside the component's
	//     fixed capacity, and accepted by the component's own validator. SetTag
	//     fails CLOSED, so a tag the component rejects authors an UNTAGGED marker
	//     that resolves to nothing.
	const char* szTag = ZM_GetProfLabExitSpawnTag();
	ZENITH_ASSERT_TRUE(szTag != nullptr && szTag[0] != '\0',
		"the ProfLab->Dawnmere connection carries an empty spawn tag");
	ZENITH_ASSERT_TRUE(ZM_SpawnPoint::IsTagValid(szTag),
		"the ProfLab->Dawnmere connection's spawn tag '%s' is not a tag "
		"ZM_SpawnPoint::SetTag will accept (capacity %u). SetTag fails CLOSED, so "
		"the Dawnmere arrival marker would author itself untagged and the exit would "
		"stall the warp machine in WAITING_FOR_SPAWN for its whole frame budget "
		"before erroring out (ZM-D-200)",
		szTag, ZM_SpawnPoint::uTAG_CAPACITY);

	// (4) ★ THE RECIPROCAL CLAUSE. Dawnmere must OFFER the tag the exit ASKS for.
	//     This is the exact predicate IsWarpDestinationValid evaluates, run here
	//     against the same table it reads.
	const ZM_WorldSpec& xDawnmere = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE);
	bool bOffered = false;
	for (u_int uTag = 0u; uTag < xDawnmere.m_uSpawnTagCount; ++uTag)
	{
		if (xDawnmere.m_pszSpawnTags != nullptr
			&& xDawnmere.m_pszSpawnTags[uTag] != nullptr
			&& std::strcmp(xDawnmere.m_pszSpawnTags[uTag], szTag) == 0)
		{
			bOffered = true;
			break;
		}
	}
	ZENITH_ASSERT_TRUE(bOffered,
		"the ProfLab exit asks Dawnmere for spawn tag '%s', which is NOT in "
		"Dawnmere's offered tag list (%u tags). ZM_GameStateManager::"
		"IsWarpDestinationValid evaluates exactly this, so the warp would be "
		"REJECTED at runtime and the lab door would be a silent no-op",
		szTag, xDawnmere.m_uSpawnTagCount);

	// (5) ...and the OTHER direction of the same door: Dawnmere's LabDoorTrigger is
	//     authored against ProfLab's own arrival tag, so ProfLab must offer it.
	const ZM_WorldSpec& xProfLab = ZM_GetWorldSpec(ZM_SCENE_PROFLAB);
	bool bArrivalOffered = false;
	for (u_int uTag = 0u; uTag < xProfLab.m_uSpawnTagCount; ++uTag)
	{
		if (xProfLab.m_pszSpawnTags != nullptr
			&& xProfLab.m_pszSpawnTags[uTag] != nullptr
			&& std::strcmp(xProfLab.m_pszSpawnTags[uTag], szZM_PROFLAB_SPAWN_TAG) == 0)
		{
			bArrivalOffered = true;
			break;
		}
	}
	ZENITH_ASSERT_TRUE(bArrivalOffered,
		"Dawnmere's lab doorway sensor is authored against ProfLab's arrival tag "
		"'%s', which ProfLab does not offer (%u tags) -- the way IN would be "
		"rejected even if the way out worked",
		szZM_PROFLAB_SPAWN_TAG, xProfLab.m_uSpawnTagCount);
}

// ============================================================================
// U13 -- the exit sensor's geometry, and the ONE clause that keeps the door from
// being an infinite loop.
//
// The other four clauses are ordinary "is this box the right box" checks. Clause
// (5) is not: it is the difference between a working door and a game that, on
// arrival, warps the player back out and in and out forever with no input
// accepted between the two.
// ============================================================================
ZENITH_TEST(ZM_WorldTraversal, ProfLab_ExitSensorFillsTheApertureAndClearsTheArrivingBody)
{
	const ZM_ProfLabBlockout xSensor = ZM_GetProfLabExitTrigger();
	ZENITH_ASSERT_TRUE(ProfLabBlockoutIsFinite(xSensor),
		"the exit sensor's blockout carries a non-finite component -- the authored "
		"transform would be NaN and the body would never overlap anything");

	// (1) It fills the aperture in X, exactly. Narrower and a player can slip past
	//     it through the gap and out of the world; wider and it pokes through the
	//     two front panels.
	const float fApertureHalfWidth = fZM_PROFLAB_APERTURE_HALF_WIDTH;
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Min().x, -fApertureHalfWidth,
		fPROFLAB_PLANE_EPSILON,
		"the exit sensor's -X face is at %.5f but the doorway opening starts at "
		"%.5f -- there is a strip of clear aperture beside the sensor, and a player "
		"who walks through it leaves the room without warping",
		(double)xSensor.Min().x, (double)-fApertureHalfWidth);
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Max().x, fApertureHalfWidth,
		fPROFLAB_PLANE_EPSILON,
		"the exit sensor's +X face is at %.5f but the doorway opening ends at %.5f",
		(double)xSensor.Max().x, (double)fApertureHalfWidth);

	// (2) It stands ON the floor and fills the opening's height. A gap under it
	//     would be jumpable-under only in theory, but a gap ABOVE it is the real
	//     hazard on a slope-free interior: nothing here should be reachable over
	//     the top of a 3 m sensor, and a short one is a silent authoring slip.
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Min().y, fZM_PROFLAB_FLOOR_TOP_Y,
		fPROFLAB_PLANE_EPSILON,
		"the exit sensor's underside is at y=%.5f, not on the floor's top face "
		"(%.5f) -- a player could pass beneath it",
		(double)xSensor.Min().y, (double)fZM_PROFLAB_FLOOR_TOP_Y);
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Max().y,
		fZM_PROFLAB_FLOOR_TOP_Y + fZM_PROFLAB_APERTURE_HEIGHT,
		fPROFLAB_PLANE_EPSILON,
		"the exit sensor's top is at y=%.5f but the doorway opening reaches %.5f",
		(double)xSensor.Max().y,
		(double)(fZM_PROFLAB_FLOOR_TOP_Y + fZM_PROFLAB_APERTURE_HEIGHT));

	// (3) Its FAR face is the doorway wall's inner plane, so it is the LAST thing
	//     between the player and the gap -- nothing can reach the aperture without
	//     having crossed the sensor first.
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Max().z, fZM_PROFLAB_INNER_MAX_Z,
		fPROFLAB_PLANE_EPSILON,
		"the exit sensor's far face is at z=%.5f, not on the doorway wall's inner "
		"plane (%.5f). A gap between the two is a strip of room a player can stand "
		"in on the far side of the sensor, still inside the building, having already "
		"passed the only thing that would have warped them",
		(double)xSensor.Max().z, (double)fZM_PROFLAB_INNER_MAX_Z);

	// (4) It is deep enough not to be tunnelled through. At the round trip's 30 Hz
	//     fixed dt a running capsule covers fRUN_SPEED/30 per frame; a sensor
	//     shallower than a couple of those is a plane a fast body can step over
	//     between physics ticks.
	//
	//     ★ THIS CLAUSE BECAME THE BINDING ONE WHEN Q-2026-08-15-001 RETREATED THE
	//     NEAR FACE. Depth is no longer a comfortable 1.5 m: the sensor gave up
	//     0.8125 m of it to clear the diagonal walk-up (see the unit below), so
	//     depth is now the SCARCE quantity in this room and this is the floor that
	//     says how much further it can ever be spent. Do not weaken it to buy more
	//     retreat -- a sensor a fast walker tunnels through is a worse door than one
	//     that fires early.
	constexpr float fMIN_SENSOR_TICKS = 2.0f;
	const float fPerFrameAt30Hz = ZM_PlayerController::fRUN_SPEED / 30.0f;
	ZENITH_ASSERT_GT(fZM_PROFLAB_EXIT_TRIGGER_SCALE_Z,
		fPerFrameAt30Hz * fMIN_SENSOR_TICKS,
		"the exit sensor is only %.4f m deep, against %.4f m of travel per 30 Hz "
		"frame at run speed -- fewer than %.0f physics ticks inside the box, which "
		"is a sensor a running player can step across between ticks",
		(double)fZM_PROFLAB_EXIT_TRIGGER_SCALE_Z, (double)fPerFrameAt30Hz,
		(double)fMIN_SENSOR_TICKS);

	// (4b) ★ ...AND THE QUANTITY THAT ACTUALLY DECIDES TUNNELLING IS NOT THE BOX.
	//      ZM_WarpTrigger fires from OnCollisionEnter -- a BODY-vs-BODY contact --
	//      and the player is a capsule of radius fZM_PROFLAB_PLAYER_RADIUS, so contact
	//      opens a radius BEFORE his centre reaches the near face and persists a
	//      radius past the far one. The window a walker spends in contact is
	//      therefore the depth plus a full capsule DIAMETER, and that is the figure
	//      the "is this still a usable exit?" question wants. Asserted against a
	//      WALK, because walking is what the last metre to a doorway is done at, and
	//      then again at run speed because a player may sprint at it.
	constexpr float fMIN_CONTACT_TICKS_WALKING = 6.0f;
	constexpr float fMIN_CONTACT_TICKS_RUNNING = 4.0f;
	const float fContactWindow = fZM_PROFLAB_EXIT_TRIGGER_SCALE_Z
		+ 2.0f * fZM_PROFLAB_PLAYER_RADIUS;
	const float fWalkPerFrameAt30Hz = ZM_PlayerController::fWALK_SPEED / 30.0f;
	ZENITH_ASSERT_GT(fContactWindow,
		fWalkPerFrameAt30Hz * fMIN_CONTACT_TICKS_WALKING,
		"a walking player is in contact with the exit sensor over only %.4f m "
		"(%.4f m of depth plus a %.4f m capsule diameter), against %.4f m of travel "
		"per 30 Hz frame at walk speed -- fewer than %.0f ticks of overlap. The "
		"sensor has been retreated far enough that the door is no longer reliable",
		(double)fContactWindow, (double)fZM_PROFLAB_EXIT_TRIGGER_SCALE_Z,
		(double)(2.0f * fZM_PROFLAB_PLAYER_RADIUS), (double)fWalkPerFrameAt30Hz,
		(double)fMIN_CONTACT_TICKS_WALKING);
	ZENITH_ASSERT_GT(fContactWindow,
		fPerFrameAt30Hz * fMIN_CONTACT_TICKS_RUNNING,
		"a RUNNING player is in contact with the exit sensor over only %.4f m, "
		"against %.4f m of travel per 30 Hz frame -- fewer than %.0f ticks",
		(double)fContactWindow, (double)fPerFrameAt30Hz,
		(double)fMIN_CONTACT_TICKS_RUNNING);

	// (4c) The NAMED near-face plane IS the box's -Z face. Three readers now spell
	//      that plane as fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z rather than re-subtracting
	//      half a depth from the centre (the arrival clearance, the unit below, and
	//      the intro beat's walk-up guard), so the name has to stay welded to the
	//      geometry it claims to describe.
	ZENITH_ASSERT_EQ_FLOAT(xSensor.Min().z, fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z,
		fPROFLAB_PLANE_EPSILON,
		"the exit sensor's box starts at z=%.5f but "
		"fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z says %.5f -- the named plane and the "
		"authored box have parted company, and every clearance stated against the "
		"name is now about a face the scene does not have",
		(double)xSensor.Min().z, (double)fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z);

	// (5) ★★ THE ANTI-LOOP CLAUSE. The player warps IN to fZM_PROFLAB_SPAWN_Z on
	//     the "Door" tag. If the arriving capsule -- the feet marker plus a body
	//     radius -- touched this sensor, ZM_WarpTrigger would fire on the first
	//     contact tick and send the player straight back to Dawnmere, whose
	//     LabDoorTrigger would send them back here, forever, with no input accepted
	//     in between. Note this is about the CAPSULE, not the marker: a check on
	//     the marker point alone would pass with the body half inside the box.
	const float fClearance = ZM_GetProfLabExitTriggerArrivalClearance();
	ZENITH_ASSERT_GT(fClearance, 0.0f,
		"the arriving player's capsule (feet z=%.4f plus %.4f m of body radius) "
		"reaches into the exit sensor, whose near face is at z=%.4f -- clearance "
		"%.4f m. The warp would fire on the arrival tick and bounce the player "
		"between Dawnmere and ProfLab forever, frozen, behind a fade. Move the "
		"sensor deeper toward the doorway or the arrival marker further into the "
		"room; do NOT weaken this clause",
		(double)fZM_PROFLAB_SPAWN_Z, (double)fZM_PROFLAB_PLAYER_RADIUS,
		(double)fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z,
		(double)fClearance);

	// ...and the same claim with a MARGIN, so the door does not merely work but
	// keeps working after a small placement change. Half a body radius is the
	// smallest gap in which "the player took a step before the door fired" is
	// observably true rather than frame-order dependent.
	const float fMinClearance = fZM_PROFLAB_PLAYER_RADIUS * 0.5f;
	ZENITH_ASSERT_GT(fClearance, fMinClearance,
		"the arriving player stands only %.4f m clear of the exit sensor, inside the "
		"%.4f m margin this room keeps. The door does not fire on arrival TODAY, but "
		"there is no room left for the marker or the sensor to move",
		(double)fClearance, (double)fMinClearance);
}

// ============================================================================
// U14 -- PROFESSOR ASTER FACES THE PLAYER.
//
// ★★ THIS UNIT MUST NOT RE-SPELL THE QUATERNION THE AUTHORING SPELLS. A guard
// that compares ZM_ProfLabAsterFacing() against a locally written (0, 1, 0, 0)
// cannot see the authored value move: both sides are the same constant and they
// would move together, which is the whole finding of the self-referential-guard
// lesson this repo has already paid for. So the property under test is not "these
// 16 bytes" -- it is HE FACES THE PLAYER, expressed as dot products against
// directions derived from the arrival placement.
//
// ★ AND IT RUNS THE SHIPPED FORWARD FUNCTION. ZM_ForwardFromRotation is what the
// interact picker and the trainer cone use to decide what an entity is facing, and
// it rotates the +Z basis (never glm::eulerAngles, which collapses past 90 degrees
// off +Z and has already cost this repo a debugging cycle). Reimplementing the
// rotation here would let this unit and the game disagree about what "facing"
// means while both stayed green.
//
// THE TWO FLOORS, and where they come from:
//   * toward the ARRIVING BODY, the geometry gives dot ~ 0.340. He stands off to
//     one side of the walk-in line by design (his X is one body footprint clear of
//     the doorway opening), so this dot can never be near 1 without moving him into
//     the corridor. The floor is 0.20: comfortably below the authored value, and
//     ABOVE ZERO, which is the whole claim -- turned toward the arriving half of
//     the room rather than away from it.
//   * toward the SETTLED CAMERA, the geometry gives dot ~ 0.875. That is the eye
//     the arrival is actually seen through, and it is the stronger constraint --
//     it is what rejects a +X or -X facing, which the first floor alone would
//     accept. The floor is 0.70.
// Neither floor is a tuning value to relax when it reds. A red here means the
// professor turned away from the door.
//
// ANTI-VACUITY: the SHIPPED-BEFORE-SC-E value -- identity, i.e. the +Z facing that
// put his back to the player -- is fed through the identical predicate and is
// required to FAIL both floors. Without that arm a predicate that always returned
// a large dot would look green.
// ============================================================================

namespace
{
	// The XZ dot between "the way this rotation faces" and "the way from here to
	// there". Both halves go through the shipped helpers: the facing through
	// ZM_ForwardFromRotation, the bearing through ZM_FlattenXZ, so a degenerate
	// input yields a zero vector and a zero dot rather than a NaN.
	float ProfLabFacingDotToward(
		const Zenith_Maths::Quat& xRotation,
		const Zenith_Maths::Vector3& xFrom,
		const Zenith_Maths::Vector3& xTo)
	{
		const Zenith_Maths::Vector3 xForward = ZM_ForwardFromRotation(xRotation);
		const Zenith_Maths::Vector3 xBearing = ZM_FlattenXZ(xTo - xFrom);
		return xForward.x * xBearing.x + xForward.z * xBearing.z;
	}
}

ZENITH_TEST(ZM_WorldTraversal, ProfLab_AsterFacingIsTurnedTowardTheArrival)
{
	// The floors, named so a failure message can state them and so neither is a
	// bare number in an assertion. See the block above for their derivation.
	constexpr float fFACES_PLAYER_MIN_DOT = 0.20f;
	constexpr float fFACES_CAMERA_MIN_DOT = 0.70f;

	const Zenith_Maths::Quat xFacing = ZM_ProfLabAsterFacing();

	// (1) It is a UNIT quaternion. A non-unit one still rotates, but Jolt
	//     normalises the body's and the serialized transform would then disagree
	//     with the constant -- a ZM-D-179-shaped drift with a different cause.
	const float fLengthSquared = xFacing.x * xFacing.x + xFacing.y * xFacing.y
		+ xFacing.z * xFacing.z + xFacing.w * xFacing.w;
	ZENITH_ASSERT_EQ_FLOAT(fLengthSquared, 1.0f, fPROFLAB_EXACT_EPSILON,
		"Professor Aster's frozen facing has squared length %.7f, not 1. A "
		"non-unit authored quaternion is normalised by the physics body and the "
		"saved bytes then differ from the constant every test compares against",
		(double)fLengthSquared);

	// (2) It gives a real XZ heading at all. A facing whose XZ projection has
	//     collapsed (a pure pitch, say) makes every dot below zero and would fail
	//     the floors for a reason that has nothing to do with where he is looking.
	const Zenith_Maths::Vector3 xForward = ZM_ForwardFromRotation(xFacing);
	const float fForwardLengthSquared =
		xForward.x * xForward.x + xForward.z * xForward.z;
	ZENITH_ASSERT_GT(fForwardLengthSquared, 0.5f,
		"Professor Aster's frozen facing has no XZ heading (flattened forward "
		"(%.5f, %.5f), length squared %.7f) -- it is a pitch or a roll, not a turn, "
		"and the interact picker's cone would reject him outright",
		(double)xForward.x, (double)xForward.z, (double)fForwardLengthSquared);

	const Zenith_Maths::Vector3 xAster = ZM_GetProfLabAsterCenter();
	const Zenith_Maths::Vector3 xArrivalPivot = ZM_GetProfLabArrivalPivot();
	const Zenith_Maths::Vector3 xSettledCamera =
		ZM_GetProfLabSettledCameraPosition();

	// (3) He is turned toward the arriving player, not away.
	const float fPlayerDot =
		ProfLabFacingDotToward(xFacing, xAster, xArrivalPivot);
	ZENITH_ASSERT_GT(fPlayerDot, fFACES_PLAYER_MIN_DOT,
		"Professor Aster's authored facing dots %.5f with the direction from his "
		"anchor to the arrival point, against a floor of %.2f. A NEGATIVE value "
		"means he has his back to the player -- which is exactly the state this "
		"placement shipped in before SC-E, with ProfLab_AsterStandsInsideTheArrival"
		"Frustum green throughout because being ON SCREEN is not the same claim as "
		"FACING somebody",
		(double)fPlayerDot, (double)fFACES_PLAYER_MIN_DOT);

	// (4) ...and toward the eye the arrival is filmed through, which is the
	//     stronger of the two and the one that rejects a sideways facing.
	const float fCameraDot =
		ProfLabFacingDotToward(xFacing, xAster, xSettledCamera);
	ZENITH_ASSERT_GT(fCameraDot, fFACES_CAMERA_MIN_DOT,
		"Professor Aster's authored facing dots %.5f with the direction from his "
		"anchor to the pose the follow camera settles at on arrival, against a floor "
		"of %.2f. This is the clause that rejects a facing turned 90 degrees to "
		"either side -- one of those still points vaguely at the player's column and "
		"would clear clause (3), while showing the player the professor's shoulder",
		(double)fCameraDot, (double)fFACES_CAMERA_MIN_DOT);

	// (5) ★ ANTI-VACUITY. The pre-SC-E authored value -- identity, forward +Z --
	//     must FAIL both floors through the identical predicate, or neither of the
	//     clauses above is known to be able to red.
	const Zenith_Maths::Quat xIdentity(1.0f, 0.0f, 0.0f, 0.0f);
	const float fIdentityPlayerDot =
		ProfLabFacingDotToward(xIdentity, xAster, xArrivalPivot);
	const float fIdentityCameraDot =
		ProfLabFacingDotToward(xIdentity, xAster, xSettledCamera);
	ZENITH_ASSERT_LT(fIdentityPlayerDot, fFACES_PLAYER_MIN_DOT,
		"the ANTI-VACUITY arm failed: the pre-SC-E identity rotation dots %.5f "
		"toward the arrival point, which CLEARS the %.2f floor clause (3) uses. "
		"Clause (3) therefore cannot red on the defect it was written for -- the "
		"floor, the anchors or the arrival placement have moved",
		(double)fIdentityPlayerDot, (double)fFACES_PLAYER_MIN_DOT);
	ZENITH_ASSERT_LT(fIdentityCameraDot, fFACES_CAMERA_MIN_DOT,
		"the ANTI-VACUITY arm failed: the pre-SC-E identity rotation dots %.5f "
		"toward the settled camera, clearing the %.2f floor clause (4) uses",
		(double)fIdentityCameraDot, (double)fFACES_CAMERA_MIN_DOT);
}

// ============================================================================
// U15 -- the six lab-seam names are usable, distinct lookup keys.
//
// Four separate readers resolve these by name: the tools authoring, clause I4 of
// ZM_ProfLabWarp_Test, ZM_CommittedSceneBytes' Dawnmere needle, and SC-D's
// ground-truth oracle (which looks "DawnmereLabShell" up specifically so its
// post-SC-E re-measure can IGNORE that body -- a collision or a typo there means
// ten frozen ground heights get re-measured off the building's roof).
// ============================================================================
ZENITH_TEST(ZM_WorldTraversal, ProfLab_LabSeamEntityNamesAreUniqueLookupKeys)
{
	const char* aszSeamNames[] = {
		szZM_PROFLAB_EXIT_TRIGGER_ENTITY_NAME,
		szZM_DAWNMERE_LAB_SHELL_ENTITY_NAME,
		szZM_DAWNMERE_LAB_DOOR_LEFT_ENTITY_NAME,
		szZM_DAWNMERE_LAB_DOOR_RIGHT_ENTITY_NAME,
		szZM_DAWNMERE_LAB_DOOR_LINTEL_ENTITY_NAME,
		szZM_DAWNMERE_FROM_LAB_SPAWN_ENTITY_NAME,
		szZM_DAWNMERE_LAB_DOOR_TRIGGER_ENTITY_NAME,
	};
	const u_int uSeamCount =
		(u_int)(sizeof(aszSeamNames) / sizeof(aszSeamNames[0]));

	for (u_int u = 0u; u < uSeamCount; ++u)
	{
		ZENITH_ASSERT_TRUE(ProfLabNameIsALookupKey(aszSeamNames[u]),
			"lab-seam entity name %u is null, empty, or carries a non-printable "
			"byte -- FindEntityByName would miss it and every clause that resolves "
			"it would report a missing entity rather than the cause", u);
		for (u_int uOther = u + 1u; uOther < uSeamCount; ++uOther)
		{
			ZENITH_ASSERT_NE(
				std::strcmp(aszSeamNames[u], aszSeamNames[uOther]), 0,
				"lab-seam entity names %u and %u are both '%s' -- "
				"FindEntityByName would resolve both to whichever the scene stored "
				"first", u, uOther, aszSeamNames[u]);
		}
	}

	// The exit sensor also has to be distinct from everything else authored INTO
	// ProfLab, which the seam list above does not cover.
	const char* aszProfLabEntities[] = {
		szZM_PROFLAB_SPAWN_ENTITY_NAME,
		szZM_PROFLAB_PLAYER_ENTITY_NAME,
		szZM_PROFLAB_CAMERA_ENTITY_NAME,
		szZM_PROFLAB_ASTER_ENTITY_NAME,
	};
	const u_int uProfLabCount =
		(u_int)(sizeof(aszProfLabEntities) / sizeof(aszProfLabEntities[0]));
	for (u_int u = 0u; u < uProfLabCount; ++u)
	{
		ZENITH_ASSERT_NE(
			std::strcmp(szZM_PROFLAB_EXIT_TRIGGER_ENTITY_NAME,
				aszProfLabEntities[u]), 0,
			"the exit sensor shares the entity name '%s' with another authored "
			"ProfLab entity", aszProfLabEntities[u]);
	}
	for (u_int u = 0u; u < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++u)
	{
		const char* szBlockName = ZM_GetProfLabBlockName((ZM_PROFLAB_BLOCK)u);
		ZENITH_ASSERT_NE(
			std::strcmp(szZM_PROFLAB_EXIT_TRIGGER_ENTITY_NAME, szBlockName), 0,
			"the exit sensor shares the entity name of shell block %u ('%s')",
			u, szBlockName);
	}
}

// ============================================================================
// U16 -- THE WALK-UP TO THE PROFESSOR DOES NOT REACH THE DOORWAY WARP.
// (Q-2026-08-15-001, and the CI-visible half of its fix.)
//
// ★★ THE PROPERTY EVERY OTHER UNIT IN THIS FILE MEASURED AROUND. U13 clause (5)
// asks whether the player is clear of the exit sensor AT THE ARRIVAL POINT, which
// they are for about a third of a second: they land on the marker and immediately
// walk at the professor. ZM_PlayerController is driven by a MOVE action whose
// keyboard scheme is W/A/S/D, so the natural "up and to the left toward him" input
// is W+A HELD TOGETHER -- a 45-degree line, not the bearing -- and a diagonal spends
// DEPTH as fast as it spends width. The shipped 1.5 m sensor put its near face at
// 6.25; that walk closed to reach with its leading edge at 6.334, fired
// ZM_WarpTrigger::OnCollisionEnter, and took the player back out to Dawnmere at the
// exact moment Aster came into range. Every clause in this file stayed green,
// because they all measured the arrival and the defect was in the walk.
//
// ★ WHAT THIS UNIT IS FOR, GIVEN ZM_AutoTests_IntroBeat ALREADY WALKS IT. That test
// drives the real eight-way chooser at the real professor and reds on this defect
// with a measured leading edge -- but it is ONE automated test with a 4,200-frame
// budget and a terrain prerequisite elsewhere in its route. This is the arithmetic,
// at boot, in the same file as the geometry, where it cannot be skipped and cannot
// time out. The two are deliberately different KINDS of proof about one property.
//
// ★ AND IT IS NOT SELF-REFERENTIAL. The near face is derived from the walk-in span;
// the leading edge is derived from Aster's XZ, the interact reach, the capsule
// radius and a square root. They are different functions of overlapping inputs, so
// the inequality between them has real content -- and the anti-vacuity arm feeds the
// RETIRED 1.5 m depth through the identical predicate and requires it to CLIP.
//
// ★ THE STRAIGHT-DIAGONAL SOLVE IS CONSERVATIVE, WHICH IS WHY IT NEEDS NO DEAD ZONE.
// The chooser drops W the moment the target's forward component falls inside its
// dead zone, and a walk that stops gaining depth early arrives SHALLOWER than this
// closed form says. So the figure below is an UPPER bound on the depth at closure,
// and a margin against it is a margin against every path the chooser can produce.
// ============================================================================

namespace
{
	// ★ THE RETIRED SENSOR DEPTH, and the only reason this literal is in this file.
	// It is what fZM_PROFLAB_EXIT_TRIGGER_SCALE_Z was before Q-2026-08-15-001, i.e.
	// the geometry the 45-degree walk-up is KNOWN to clip. U16's anti-vacuity arm
	// feeds it through the identical predicate, which is what proves the clause
	// above it can red at all. NOT a shipped constant; nothing outside this file may
	// read it, and it must never be restored to the header.
	constexpr float fPROFLAB_RETIRED_SENSOR_DEPTH = 1.5f;

	// How far past the closure ring the "the picker really does accept here" arm
	// probes, and how far short of it the "...and not yet here" arm does. The first
	// only has to beat float rounding on an INCLUSIVE boundary; the second is a real
	// step, so that arm is about distance rather than about a tie.
	constexpr float fPROFLAB_CLOSURE_NUDGE = 0.001f;
	constexpr float fPROFLAB_CLOSURE_STEP_BACK = 0.05f;

	// The eight-way chooser's answer for a target that is BOTH to the side and
	// deeper, at the authored yaw: one unit step on each axis, normalised. Derived
	// from the SIGNS of the offset rather than written as (-1, 0, +1), so a
	// placement that moved Aster to +X would still describe the walk he is actually
	// approached along.
	Zenith_Maths::Vector3 ProfLabEightWayDiagonal(
		const Zenith_Maths::Vector3& xFrom, const Zenith_Maths::Vector3& xTo)
	{
		const float fStepX = xTo.x > xFrom.x ? 1.0f : (xTo.x < xFrom.x ? -1.0f : 0.0f);
		const float fStepZ = xTo.z > xFrom.z ? 1.0f : (xTo.z < xFrom.z ? -1.0f : 0.0f);
		return ProfLabNormalise(Zenith_Maths::Vector3(fStepX, 0.0f, fStepZ));
	}
}

ZENITH_TEST(ZM_WorldTraversal, ProfLab_ExitSensorClearsTheDiagonalWalkUpToTheProfessor)
{
	const Zenith_Maths::Vector3 xSpawnCentre = ProfLabSpawnCentre();
	const Zenith_Maths::Vector3 xAster = ZM_GetProfLabAsterCenter();

	// (1) THE INPUT UNDER TEST EXISTS. If the professor stood on the walk-in
	//     centreline, or no deeper than the arrival marker, the eight-way chooser
	//     would press ONE key and there would be no diagonal to clear -- and every
	//     clause below would pass by describing a walk nobody takes.
	const Zenith_Maths::Vector3 xDiagonal =
		ProfLabEightWayDiagonal(xSpawnCentre, xAster);
	ZENITH_ASSERT_GT(std::fabs(xDiagonal.x), fPROFLAB_EXACT_EPSILON,
		"the eight-way walk toward Professor Aster has no sideways component "
		"(direction x=%.5f) -- he is on the arrival marker's own column, so this "
		"unit is about a diagonal that does not exist", (double)xDiagonal.x);
	ZENITH_ASSERT_GT(xDiagonal.z, fPROFLAB_EXACT_EPSILON,
		"the eight-way walk toward Professor Aster gains no DEPTH (direction "
		"z=%.5f). He no longer stands deeper into the room than the arrival marker, "
		"so a diagonal approach cannot carry the player toward the doorway and this "
		"unit proves nothing", (double)xDiagonal.z);

	// (2) SOLVE FOR THE CLOSURE POINT: the first t at which the walker's planar
	//     distance to him equals the picker's effective reach, i.e. where
	//     ZM_AutoTests_IntroBeat's approach phase ends and the player stops. With a
	//     UNIT direction this is the smaller root of
	//         t^2 - 2 (V . D) t + (|V|^2 - R^2) = 0.
	const float fToAsterX = xAster.x - xSpawnCentre.x;
	const float fToAsterZ = xAster.z - xSpawnCentre.z;
	const float fAlong = fToAsterX * xDiagonal.x + fToAsterZ * xDiagonal.z;
	const float fDistanceSquared = fToAsterX * fToAsterX + fToAsterZ * fToAsterZ;
	const float fReach = fZM_PROFLAB_ASTER_EFFECTIVE_REACH;
	const float fDiscriminant =
		fAlong * fAlong - fDistanceSquared + fReach * fReach;

	//     ANTI-VACUITY for the solve itself: a negative discriminant would mean the
	//     45-degree line never enters his reach at all, so the "leading edge at
	//     closure" below would be a square root of a negative number and the walk-up
	//     in the intro beat would time out rather than warp.
	ZENITH_ASSERT_GE(fDiscriminant, 0.0f,
		"the 45-degree walk toward Professor Aster never closes to his %.4f m "
		"effective reach (discriminant %.5f) -- the eight-way chooser would carry "
		"the player past him and the intro beat's approach would stall, which is a "
		"different defect from the one this unit is about",
		(double)fReach, (double)fDiscriminant);
	if (fDiscriminant < 0.0f)
	{
		return;   // already FAILED; everything below would be a NaN
	}

	const float fTravel = fAlong - std::sqrt(fDiscriminant);
	ZENITH_ASSERT_GT(fTravel, 0.0f,
		"the walk toward Professor Aster closes to reach after %.5f m of travel, "
		"i.e. the player already stands inside his reach on arrival. "
		"ProfLab_AsterStandsOutsideArrivalReachInsideTheShell says otherwise, so one "
		"of the two is measuring a different geometry", (double)fTravel);

	// (3) ★ THE HEADLINE. The capsule's LEADING EDGE at that point -- centre plus a
	//     body radius, because ZM_WarpTrigger fires on a body contact and not on a
	//     centre-in-box test -- must clear the sensor's near face by a real margin.
	//     The floor is one full fZM_PROFLAB_PLAYER_RADIUS: a named body quantity
	//     rather than a tuning number, and the smallest gap in which "the player was
	//     nowhere near the door" is true rather than arithmetically survivable.
	const float fDepthAtClosure = fTravel * xDiagonal.z;
	const float fLeadingEdge =
		xSpawnCentre.z + fDepthAtClosure + fZM_PROFLAB_PLAYER_RADIUS;
	const float fMargin = fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z - fLeadingEdge;
	ZENITH_ASSERT_GT(fMargin, fZM_PROFLAB_PLAYER_RADIUS,
		"a player walking W+A at Professor Aster -- the eight-way chooser's answer "
		"for a target that is both sideways and deeper, and what a human holding two "
		"keys produces -- closes to his %.4f m reach after %.4f m of travel, having "
		"gained %.4f m of DEPTH. That puts the capsule's leading edge at z=%.4f "
		"against an exit-sensor near face at z=%.4f: a margin of %.4f m, inside the "
		"%.4f m floor. At or below ZERO the player is warped back out to Dawnmere at "
		"the exact moment the professor comes into range, with the seam still "
		"answering ZM_INTERACT_OK and nothing else in the gate saying a word "
		"(Q-2026-08-15-001). RETREAT THE SENSOR, do not move the professor -- pulling "
		"him toward the camera narrows the arrival frame at his depth and re-opens "
		"the off-screen defect SC-C closed",
		(double)fReach, (double)fTravel, (double)fDepthAtClosure,
		(double)fLeadingEdge, (double)fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z,
		(double)fMargin, (double)fZM_PROFLAB_PLAYER_RADIUS);

	// (4) ★ ANTI-VACUITY. The RETIRED 1.5 m depth, through the identical predicate,
	//     must be CLIPPED by that same leading edge. Without this arm a solve that
	//     had lost its depth term -- or a reach re-tuned to nothing -- would satisfy
	//     clause (3) and this unit would be decoration.
	const float fRetiredNearFace =
		fZM_PROFLAB_INNER_MAX_Z - fPROFLAB_RETIRED_SENSOR_DEPTH;
	ZENITH_ASSERT_GT(fLeadingEdge, fRetiredNearFace,
		"the ANTI-VACUITY arm failed: the same 45-degree walk puts the leading edge "
		"at z=%.4f, which CLEARS the retired %.2f m sensor's near face at z=%.4f. "
		"That geometry is known to have warped the player out mid-walk-up, so clause "
		"(3) above cannot currently red on the defect it was written for -- the "
		"reach, the professor's anchor or the arrival marker has moved",
		(double)fLeadingEdge, (double)fPROFLAB_RETIRED_SENSOR_DEPTH,
		(double)fRetiredNearFace);

	// (5) ★ THE STRUCTURAL BACKSTOP, which does not depend on the reach at all. The
	//     chooser stops pressing W the moment the player draws level with him in
	//     depth, so the DEEPEST leading edge any walk-up-to-Aster can produce is his
	//     own depth plus a body radius -- whatever the picker's range is re-tuned to
	//     tomorrow. Clause (3) can be re-opened by a longer reach; this one cannot.
	const float fDeepestPossibleEdge =
		fZM_PROFLAB_ASTER_Z + fZM_PROFLAB_PLAYER_RADIUS;
	ZENITH_ASSERT_LT(fDeepestPossibleEdge, fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z,
		"the deepest point any walk-up to the professor can put the capsule's "
		"leading edge (his own depth %.4f plus a %.4f m body radius = %.4f) is at or "
		"past the exit sensor's near face (%.4f). The sensor is inside the walking "
		"envelope, so SOME approach to him fires the doorway warp even if the "
		"45-degree one above happens to clear it",
		(double)fZM_PROFLAB_ASTER_Z, (double)fZM_PROFLAB_PLAYER_RADIUS,
		(double)fDeepestPossibleEdge,
		(double)fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z);

	// (6) ★ AND HE HIMSELF IS CLEAR OF IT IN DEPTH, not merely off to one side.
	//     Before the retreat his CENTRE (6.375) stood inside the sensor's depth span
	//     (near face 6.25) and the only thing keeping the professor out of the
	//     doorway warp was his X. A clearance held by one axis in a room whose other
	//     axis is the one everybody walks along is a coincidence, not a property.
	const float fAsterDepthClearance =
		ZM_GetProfLabAsterExitSensorDepthClearance();
	ZENITH_ASSERT_GT(fAsterDepthClearance, 0.0f,
		"Professor Aster's body reaches to z=%.4f, at or past the exit sensor's near "
		"face (%.4f) -- clearance %.4f m. He is inside the sensor's DEPTH span and "
		"stays out of the doorway warp only because the aperture is narrower than he "
		"is far off centre",
		(double)(fZM_PROFLAB_ASTER_Z + fZM_HUMAN_BODY_FOOTPRINT * 0.5f),
		(double)fZM_PROFLAB_EXIT_TRIGGER_NEAR_Z, (double)fAsterDepthClearance);

	// (7) ★ THE CLOSURE POINT IS THE SHIPPED PICKER'S, NOT THIS FILE'S ARITHMETIC.
	//     Clauses (3) and (4) are about a ring computed from
	//     fZM_PROFLAB_ASTER_EFFECTIVE_REACH; run ZM_PickInteractTarget at that point
	//     -- and one real step short of it -- so the ring is known to be where the
	//     walk actually STOPS rather than merely somewhere inside his range.
	ZM_InteractProbe xProbe;
	xProbe.m_xPosition = xAster;
	xProbe.m_fRadius = fZM_PROFLAB_ASTER_REACH_BONUS;
	xProbe.m_bEnabled = true;
	const ZM_InteractTuning xTuning{};
	u_int uBest = 0u;

	ZM_InteractOrigin xAtClosure;
	xAtClosure.m_xPosition =
		xSpawnCentre + xDiagonal * (fTravel + fPROFLAB_CLOSURE_NUDGE);
	xAtClosure.m_xForward = xAster - xAtClosure.m_xPosition;
	const ZM_INTERACT_REJECT eAtClosure = ZM_PickInteractTarget(
		&xProbe, 1u, xAtClosure, xTuning, uBest);
	ZENITH_ASSERT_EQ((u_int)eAtClosure, (u_int)ZM_INTERACT_OK,
		"the shipped picker answers '%s' at the point this unit solves for as the "
		"end of the walk-up (%.4f m along the diagonal). The leading edge in clause "
		"(3) is therefore measured somewhere the walk does not stop, and the margin "
		"it reports is about the wrong frame",
		ZM_InteractRejectName(eAtClosure), (double)fTravel);

	ZM_InteractOrigin xShortOfClosure;
	xShortOfClosure.m_xPosition =
		xSpawnCentre + xDiagonal * (fTravel - fPROFLAB_CLOSURE_STEP_BACK);
	xShortOfClosure.m_xForward = xAster - xShortOfClosure.m_xPosition;
	const ZM_INTERACT_REJECT eShortOfClosure = ZM_PickInteractTarget(
		&xProbe, 1u, xShortOfClosure, xTuning, uBest);
	ZENITH_ASSERT_EQ(
		(u_int)eShortOfClosure, (u_int)ZM_INTERACT_REJECT_OUT_OF_RANGE,
		"the shipped picker ALREADY answers '%s' %.3f m short of the closure point "
		"this unit solves for, so the walk stops shallower than clause (3) measures "
		"and that clause is not the tight bound it claims to be",
		ZM_InteractRejectName(eShortOfClosure),
		(double)fPROFLAB_CLOSURE_STEP_BACK);
}
