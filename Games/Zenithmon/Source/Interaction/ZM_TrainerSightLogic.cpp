#include "Zenith.h"

#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"

#include <cmath>   // std::isfinite / std::fabs

// ============================================================================
// ZM_TrainerSightLogic (S7 item 3 SC3). See the header for the contract; this
// file is the ORDER. Nothing here touches the ECS, the scene, the physics world,
// the UI or the disk, and NOTHING HERE MAY Zenith_Assert ON ITS ARGUMENTS -- the
// boot units feed these functions NaNs, negative ranges and zero facings on
// purpose to pin the fail-closed answers.
// ============================================================================

namespace
{
	bool ZM_IsFiniteVec3(const Zenith_Maths::Vector3& xValue)
	{
		return std::isfinite(xValue.x) && std::isfinite(xValue.y) && std::isfinite(xValue.z);
	}
}

bool ZM_IsTargetInTrainerSight(const Zenith_Maths::Vector3& xTrainerPosition,
	const Zenith_Maths::Vector3& xTrainerForward,
	const Zenith_Maths::Vector3& xTargetPosition,
	const ZM_TrainerSightTuning& xTuning)
{
	// 1. FAIL CLOSED on anything non-finite, BEFORE any comparison. Every test
	//    below is a `>` against a threshold, and every comparison with a NaN is
	//    false -- so without this guard a NaN would SAIL THROUGH range, band and
	//    the coincident carve-out and report "seen".
	if (!ZM_IsFiniteVec3(xTrainerPosition)
		|| !ZM_IsFiniteVec3(xTrainerForward)
		|| !ZM_IsFiniteVec3(xTargetPosition)
		|| !std::isfinite(xTuning.m_fMaxDistance)
		|| !std::isfinite(xTuning.m_fMinFacingDot)
		|| !std::isfinite(xTuning.m_fMaxVertical))
	{
		return false;
	}

	// 2. No facing -> no cone. The SHARED flatten, so "facing" means here exactly
	//    what it means to ZM_ForwardFromRotation and to the interact picker.
	const Zenith_Maths::Vector3 xForward = ZM_FlattenXZ(xTrainerForward);
	if (((xForward.x * xForward.x) + (xForward.z * xForward.z)) <= fZM_INTERACT_DEGENERATE_LEN_SQ)
	{
		return false;
	}

	// 3. A negative range never squares its way back into "everything is in range".
	if (xTuning.m_fMaxDistance < 0.0f)
	{
		return false;
	}

	// 4. XZ distance, Y deliberately dropped -- a trainer standing on a slope must
	//    still spot the player; the height difference is policed by step 5.
	const float fDeltaX = xTargetPosition.x - xTrainerPosition.x;
	const float fDeltaZ = xTargetPosition.z - xTrainerPosition.z;
	const float fDistanceSquared = (fDeltaX * fDeltaX) + (fDeltaZ * fDeltaZ);
	if (fDistanceSquared > (xTuning.m_fMaxDistance * xTuning.m_fMaxDistance))
	{
		return false;
	}

	// 5. Vertical band, inclusive: this is what stops a trainer spotting the player
	//    through a floor.
	const float fDeltaY = xTargetPosition.y - xTrainerPosition.y;
	if (std::fabs(fDeltaY) > xTuning.m_fMaxVertical)
	{
		return false;
	}

	// 6. The ONE angular test in the game.
	return ZM_IsFacingXZ(xTrainerPosition, xForward, xTargetPosition, xTuning.m_fMinFacingDot);
}

bool ZM_IsTargetInTrainerSightFromRotation(const Zenith_Maths::Vector3& xTrainerPosition,
	const Zenith_Maths::Quat& xTrainerRotation,
	const Zenith_Maths::Vector3& xTargetPosition,
	const ZM_TrainerSightTuning& xTuning)
{
	// Never re-derives a facing of its own -- see the header on glm::eulerAngles.
	return ZM_IsTargetInTrainerSight(xTrainerPosition,
		ZM_ForwardFromRotation(xTrainerRotation), xTargetPosition, xTuning);
}
