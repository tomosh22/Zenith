#include "Zenith.h"

#include "Zenithmon/Source/Interaction/ZM_TrainerSightProbe.h"

#include "Core/Zenith_Engine.h"                                 // g_xEngine.Physics()
#include "EntityComponent/Zenith_PhysicsQuery.h"                // RaycastIgnoring -- the ONE ignore-filtered cast
#include "Physics/Zenith_Physics.h"                             // Zenith_Physics::RaycastResult / HasActiveSimulation
#include "Zenithmon/Source/Interaction/ZM_InteractionLogic.h"    // fZM_INTERACT_DEGENERATE_LEN_SQ

#include <cmath>   // std::isfinite / std::sqrt

// ============================================================================
// ZM_TrainerSightProbe (S7 item 3 SC6). See the header for the answer table;
// this file is the ORDER. It is the ONLY impure piece of the trainer sight
// stack that is not a component, and it NEVER calls Zenith_Assert -- the boot
// units feed it NaNs and infinities on purpose, and an assert on a
// unit-supplied input does not fail one test, it ends the whole boot unit run.
// ============================================================================

namespace
{
	// File-local, exactly like ZM_TrainerSightLogic.cpp's ZM_IsFiniteVec3 and
	// ZM_Interactable.cpp's own copy: there is no shared finite-vector helper in
	// this game, and the probe must not grow a dependency to acquire three
	// std::isfinite calls.
	bool ZM_IsFiniteVector3(const Zenith_Maths::Vector3& xValue)
	{
		return std::isfinite(xValue.x)
			&& std::isfinite(xValue.y)
			&& std::isfinite(xValue.z);
	}
}

ZM_TrainerSightProbeResult ZM_ProbeTrainerSightLine(
	const Zenith_Maths::Vector3& xTrainerPosition,
	Zenith_EntityID xTrainerEntityID,
	const Zenith_Maths::Vector3& xTargetPosition,
	Zenith_EntityID xTargetEntityID)
{
	ZM_TrainerSightProbeResult xResult;

	if (!ZM_IsFiniteVector3(xTrainerPosition) || !ZM_IsFiniteVector3(xTargetPosition))
	{
		xResult.m_bClear = false;
		return xResult;
	}

	const Zenith_Maths::Vector3 xSeparation = xTargetPosition - xTrainerPosition;
	const float fSeparationSq = xSeparation.x * xSeparation.x
		+ xSeparation.y * xSeparation.y
		+ xSeparation.z * xSeparation.z;
	if (fSeparationSq <= fZM_INTERACT_DEGENERATE_LEN_SQ)
	{
		xResult.m_bClear = true;
		return xResult;
	}

	Zenith_Physics& xPhysics = g_xEngine.Physics();
	xResult.m_bPhysicsAvailable = xPhysics.HasActiveSimulation();
	if (!xResult.m_bPhysicsAvailable)
	{
		xResult.m_bClear = true;   // FAIL OPEN -- see the header's answer table
		return xResult;
	}

	const float fSeparation = std::sqrt(fSeparationSq);
	const Zenith_Physics::RaycastResult xHit = Zenith_PhysicsQuery::RaycastIgnoring(
		xTrainerPosition, xSeparation, fSeparation, xTrainerEntityID);
	if (!xHit.m_bHit)
	{
		xResult.m_bClear = true;
		return xResult;
	}
	if (xTargetEntityID != INVALID_ENTITY_ID && xHit.m_xHitEntity == xTargetEntityID)
	{
		// The ray terminated on the TARGET's own body. Only one body can be
		// filtered per cast, so this comparison is how the far end is excused.
		xResult.m_bClear = true;
		return xResult;
	}

	xResult.m_bClear = false;
	xResult.m_bBlockerHit = true;
	xResult.m_xBlockerEntityID = xHit.m_xHitEntity;
	xResult.m_fBlockerDistance = xHit.m_fDistance;
	return xResult;
}
