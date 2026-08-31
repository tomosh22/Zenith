#include "Zenith.h"

// ============================================================================
// ZM_Tests_FollowCameraCeiling -- the boot units for the indoor camera clamp.
//
// ★★ THE DEFECT THIS PINS. ZM_FollowCamera::ComputeDesiredPosition puts the lens
// fCAMERA_HEIGHT (3.0 m) above the player's TRANSFORM, unconditionally. That is
// right outdoors and wrong in every room this game has: PlayerHome's ceiling slab
// starts at 3.0 m and a player standing on its floor has a transform at ~0.9 m,
// so the desired lens sat at ~3.9 m -- most of a metre ABOVE the ceiling, looking
// down through it. The player could not see inside their own house.
//
// ★ AND NO RAYCAST COULD HAVE FOUND IT. The arm is already swept for
// obstructions and it swept straight through this one: the interior shell is a
// VISUAL-ONLY entity (ZM_InteriorDressing.h -- the seven blockout blocks own all
// the collision and none of them is a ceiling), so the slab has no collider to
// hit. That is why the fix is a clamp against a resolved NUMBER rather than
// another query.
//
// PURE. ClampBoomBelowCeiling is a static over three vectors, so these units run
// in EVERY configuration with no scene, no physics and no graphics. That matters:
// the only other thing that exercises the clamp is ZM_ImportedPropShowcase_Test, which is
// graphics-required and therefore SKIPS -- i.e. passes -- in the headless gate.
//
// ★ WHAT THESE UNITS CANNOT DO. They prove the arithmetic and the total cases.
// They cannot prove that PlayerHome's camera RESOLVES a ceiling at all (that is
// ResolveCeiling walking the live scene for a shell entity, checked by
// ZM_ImportedPropShowcase_Test's live-lens clause) and they cannot prove the resulting
// shot looks right. Do not read this file's greenness as "the room is visible".
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Maths/Zenith_Maths.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Source/Gen/ZM_InteriorGen.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"

#include <cmath>

namespace
{
	constexpr float fFC_EPSILON = 1.0e-4f;

	Zenith_Maths::Vector3 FCVec(float x, float y, float z)
	{
		return Zenith_Maths::Vector3(x, y, z);
	}
}

// Outdoors -- the sentinel -- must be the identity. Every overworld scene takes
// this path on every frame, so a regression here would move the whole game's
// camera rather than just the interiors'.
ZENITH_TEST(ZM_FollowCameraCeiling, NoCeilingLeavesTheBoomUntouched)
{
	const Zenith_Maths::Vector3 xPivot = FCVec(0.0f, 1.5f, 3.5f);
	const Zenith_Maths::Vector3 xDesired = FCVec(0.0f, 3.9f, -2.0f);

	const Zenith_Maths::Vector3 xOut = ZM_FollowCamera::ClampBoomBelowCeiling(
		xDesired, xPivot, ZM_FollowCamera::GetNoCeiling());

	ZENITH_ASSERT_LT(glm::length(xOut - xDesired), fFC_EPSILON,
		"an outdoor scene must get its boom back unchanged");
}

// A lens that already clears the slab is left alone -- the clamp is a ceiling,
// not a rail that pins every indoor shot to one height.
ZENITH_TEST(ZM_FollowCameraCeiling, ALensAlreadyUnderTheSlabIsUntouched)
{
	const Zenith_Maths::Vector3 xPivot = FCVec(0.0f, 1.0f, 0.0f);
	const Zenith_Maths::Vector3 xDesired = FCVec(0.0f, 1.8f, -3.0f);

	const Zenith_Maths::Vector3 xOut =
		ZM_FollowCamera::ClampBoomBelowCeiling(xDesired, xPivot, 3.0f);

	ZENITH_ASSERT_LT(glm::length(xOut - xDesired), fFC_EPSILON,
		"a lens already below the cap must not be moved");
}

// ★ THE SHIPPED CASE, WITH PLAYERHOME'S REAL NUMBERS -- all of them READ, none
// re-spelled. This is the exact geometry that produced a picture of the ceiling.
ZENITH_TEST(ZM_FollowCameraCeiling, PlayerHomeBoomEndsUpUnderTheSlab)
{
	const float fCeiling =
		ZM_GetInteriorRoomSpec(ZM_INTERIOR_ROOM_PLAYER_HOME).m_fWallHeight;
	const float fCap = fCeiling - ZM_FollowCamera::GetCeilingClearance();

	// A player standing on the floor: the transform is the capsule centre.
	const Zenith_Maths::Vector3 xPlayer = FCVec(0.0f, fZM_HUMAN_BODY_HALF_HEIGHT, 3.5f);
	const Zenith_Maths::Vector3 xPivot =
		xPlayer + FCVec(0.0f, ZM_FollowCamera::GetPivotHeight(), 0.0f);
	const Zenith_Maths::Vector3 xDesired =
		ZM_FollowCamera::ComputeDesiredPosition(xPlayer, 0.0f);

	// The premise: the UNCLAMPED boom really is above the ceiling. If this ever
	// stops being true the rest of the test is vacuous, so it is asserted.
	ZENITH_ASSERT_GT(xDesired.y, fCeiling,
		"the unclamped boom must sit ABOVE PlayerHome's %.2f m ceiling for this "
		"test to mean anything (got %.3f)", (double)fCeiling, (double)xDesired.y);

	const Zenith_Maths::Vector3 xOut =
		ZM_FollowCamera::ClampBoomBelowCeiling(xDesired, xPivot, fCeiling);

	ZENITH_ASSERT_LT(xOut.y, fCap + fFC_EPSILON,
		"the clamped lens must sit at or under the %.3f m cap (got %.3f)",
		(double)fCap, (double)xOut.y);
	ZENITH_ASSERT_LT(std::fabs(xOut.y - fCap), fFC_EPSILON,
		"a boom that overshoots must land exactly ON the cap, not somewhere near it");
}

// ★★ THE BOOM SLIDES, IT DOES NOT FLATTEN. Capping only the height also clears
// the ceiling -- and costs the shot its pitch, filling the top third of the frame
// with ceiling and pushing the room out of the bottom. Sliding along the boom
// keeps the angle the camera was designed at. This clause is what tells those two
// implementations apart; without it the cheaper, worse one passes.
ZENITH_TEST(ZM_FollowCameraCeiling, ClampPreservesThePitchAndShortensTheArm)
{
	const Zenith_Maths::Vector3 xPivot = FCVec(0.0f, 1.5f, 3.5f);
	const Zenith_Maths::Vector3 xDesired = FCVec(0.0f, 3.9f, -2.0f);
	const float fCeiling = 3.0f;

	const Zenith_Maths::Vector3 xOut =
		ZM_FollowCamera::ClampBoomBelowCeiling(xDesired, xPivot, fCeiling);

	const Zenith_Maths::Vector3 xArmBefore = xDesired - xPivot;
	const Zenith_Maths::Vector3 xArmAfter = xOut - xPivot;

	// Same direction...
	const Zenith_Maths::Vector3 xDirBefore = glm::normalize(xArmBefore);
	const Zenith_Maths::Vector3 xDirAfter = glm::normalize(xArmAfter);
	ZENITH_ASSERT_GT(glm::dot(xDirBefore, xDirAfter), 1.0f - 1.0e-4f,
		"the clamped boom must point the same way as the authored one "
		"(dot %.6f) -- a height-only cap would flatten it",
		(double)glm::dot(xDirBefore, xDirAfter));

	// ...shorter, and still in front of the player rather than through them.
	ZENITH_ASSERT_LT(glm::length(xArmAfter), glm::length(xArmBefore),
		"clamping must SHORTEN the arm");
	ZENITH_ASSERT_GT(glm::length(xArmAfter), 0.0f, "the arm must not collapse to zero");
}

// ★ TOTAL WHERE THE ROOM'S OWN ASSUMPTION FAILS. A player standing on a prop, on
// a mezzanine, or simply not in the room the shell describes can have a PIVOT
// above the cap. Dropping the lens to the cap there would bury it in the floor
// and point the camera up at the player's feet, so the boom is left alone and
// the ordinary arm raycast -- which CAN see the walls -- stays the only
// constraint.
ZENITH_TEST(ZM_FollowCameraCeiling, APivotAboveTheCapIsLeftToTheRaycast)
{
	const Zenith_Maths::Vector3 xPivot = FCVec(0.0f, 2.9f, 0.0f);
	const Zenith_Maths::Vector3 xDesired = FCVec(0.0f, 5.3f, -5.5f);

	const Zenith_Maths::Vector3 xOut =
		ZM_FollowCamera::ClampBoomBelowCeiling(xDesired, xPivot, 3.0f);

	ZENITH_ASSERT_LT(glm::length(xOut - xDesired), fFC_EPSILON,
		"a pivot at or above the cap must leave the boom untouched");
}

// Non-finite input must pass straight through rather than propagating a NaN into
// the camera, which would reach Zenith_CameraComponent::SetPosition.
ZENITH_TEST(ZM_FollowCameraCeiling, NonFiniteInputIsPassedThrough)
{
	const float fNaN = std::nanf("");
	const Zenith_Maths::Vector3 xPivot = FCVec(0.0f, 1.5f, 0.0f);
	const Zenith_Maths::Vector3 xDesired = FCVec(0.0f, 3.9f, -5.5f);

	const Zenith_Maths::Vector3 xNanCeiling =
		ZM_FollowCamera::ClampBoomBelowCeiling(xDesired, xPivot, fNaN);
	ZENITH_ASSERT_LT(glm::length(xNanCeiling - xDesired), fFC_EPSILON,
		"a non-finite ceiling must leave the boom untouched");

	const Zenith_Maths::Vector3 xNanDesired =
		ZM_FollowCamera::ClampBoomBelowCeiling(FCVec(0.0f, fNaN, 0.0f), xPivot, 3.0f);
	ZENITH_ASSERT_TRUE(std::isnan(xNanDesired.y),
		"a non-finite desired position is returned as-is, not silently repaired");
}

// ★ THE CLEARANCE MUST ACTUALLY CLEAR THE NEAR PLANE. Both interiors author a
// 0.1 m near plane, so a clearance at or below that would put the ceiling slab
// inside the frustum's front face and back where it started.
ZENITH_TEST(ZM_FollowCameraCeiling, ClearanceExceedsTheAuthoredNearPlane)
{
	constexpr float fAUTHORED_INTERIOR_NEAR_PLANE = 0.1f;
	ZENITH_ASSERT_GT(ZM_FollowCamera::GetCeilingClearance(),
		fAUTHORED_INTERIOR_NEAR_PLANE,
		"the ceiling clearance (%.3f) must exceed the authored near plane (%.3f), "
		"or the slab re-enters the frustum",
		(double)ZM_FollowCamera::GetCeilingClearance(),
		(double)fAUTHORED_INTERIOR_NEAR_PLANE);

	// And both rooms must be tall enough for the clamped lens to stay above the
	// pivot at all -- a room shorter than pivot + clearance would invert the shot.
	for (u_int u = 0u; u < (u_int)ZM_INTERIOR_ROOM_COUNT; ++u)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)u;
		const float fCeiling = ZM_GetInteriorRoomSpec(eRoom).m_fWallHeight;
		const float fPivotY =
			fZM_HUMAN_BODY_HALF_HEIGHT + ZM_FollowCamera::GetPivotHeight();
		ZENITH_ASSERT_GT(fCeiling - ZM_FollowCamera::GetCeilingClearance(), fPivotY,
			"room %u: the %.2f m ceiling leaves no headroom above the %.2f m pivot",
			u, (double)fCeiling, (double)fPivotY);
	}
}
