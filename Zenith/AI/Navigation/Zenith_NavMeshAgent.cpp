#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Profiling/Zenith_Profiling.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Physics/Zenith_Physics.h"

#ifdef ZENITH_TOOLS
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#endif

bool Zenith_NavMeshAgent::SetDestination(const Zenith_Maths::Vector3& xDestination)
{
	if (m_pxNavMesh == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI, "NavMeshAgent: No NavMesh assigned");
		return false;
	}

	m_xDestination = xDestination;
	m_bReachedDestination = false;
	m_uCurrentWaypoint = 0;
	m_bPathPending = true;  // Mark that we need a path

	// We deliberately do NOT reset m_xVelocity or m_fCurrentSpeed here.
	// An agent that's already moving toward a similar target (e.g. the
	// DP priest re-issuing SetDestination every BT tick to track a
	// moving villager during the Apprehend channel) needs to keep its
	// momentum; otherwise it slows back to zero every tick and never
	// reaches m_fMoveSpeed -- the chase becomes uncatchable. The next
	// CalculateVelocity call will re-aim m_xVelocity along the new
	// path, with m_fCurrentSpeed naturally clamped against
	// m_fMoveSpeed via the accelerate/decelerate-to-desired-speed
	// branch.

	// Path will be calculated later (either in Update or via batch processing)
	m_xCurrentPath.m_axWaypoints.Clear();
	m_xCurrentPath.m_eStatus = Zenith_PathResult::Status::FAILED;

	return true;
}

void Zenith_NavMeshAgent::SetPathResult(const Zenith_PathResult& xResult)
{
	m_xCurrentPath = xResult;
	m_bPathPending = false;
	m_uCurrentWaypoint = 0;

	// Skip first waypoint if it's very close to start position
	if (m_xCurrentPath.m_axWaypoints.GetSize() > 1)
	{
		float fDistToFirst = Zenith_Maths::Length(
			m_xCurrentPath.m_axWaypoints.Get(0) - m_xPathStartPos);
		if (fDistToFirst < m_fStoppingDistance)
		{
			m_uCurrentWaypoint = 1;
		}
	}
}

bool Zenith_NavMeshAgent::GetPendingPathRequest(Zenith_Maths::Vector3& xStartOut, Zenith_Maths::Vector3& xEndOut) const
{
	if (!m_bPathPending)
	{
		return false;
	}
	xStartOut = m_xPathStartPos;
	xEndOut = m_xDestination;
	return true;
}

void Zenith_NavMeshAgent::Stop()
{
	m_xCurrentPath.m_axWaypoints.Clear();
	m_xCurrentPath.m_eStatus = Zenith_PathResult::Status::FAILED;
	m_uCurrentWaypoint = 0;
	m_bReachedDestination = false;
	m_bPathPending = false;
	m_xVelocity = Zenith_Maths::Vector3(0.0f);
	m_fCurrentSpeed = 0.0f;
}

float Zenith_NavMeshAgent::GetRemainingDistance() const
{
	if (!HasPath() || m_bReachedDestination)
	{
		return 0.0f;
	}

	// Bounds check - ensure current waypoint is valid
	if (m_uCurrentWaypoint >= m_xCurrentPath.m_axWaypoints.GetSize())
	{
		return 0.0f;
	}

	// Distance from current position to current waypoint + rest of path
	float fDistance = 0.0f;
	for (uint32_t u = m_uCurrentWaypoint; u + 1 < m_xCurrentPath.m_axWaypoints.GetSize(); ++u)
	{
		fDistance += Zenith_Maths::Length(m_xCurrentPath.m_axWaypoints.Get(u + 1) -
			m_xCurrentPath.m_axWaypoints.Get(u));
	}
	return fDistance;
}

void Zenith_NavMeshAgent::Update(float fDt,
                                 Zenith_TransformComponent& xTransform,
                                 Zenith_ColliderComponent* pxCollider)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_AGENT_UPDATE);

	// Decide once whether this agent drives motion through Jolt or via
	// direct transform writes. The physics path is preferred whenever a
	// dynamic body is present; the legacy SetPosition path is only used
	// for transform-only test fixtures + non-physics agents.
	const bool bUsePhysics =
		pxCollider != nullptr
		&& pxCollider->HasValidBody()
		&& pxCollider->GetRigidBodyType() == RIGIDBODY_TYPE_DYNAMIC;

	if (m_bReachedDestination || m_pxNavMesh == nullptr)
	{
		// Apply deceleration when stopped
		m_fCurrentSpeed = std::max(0.0f, m_fCurrentSpeed - m_fAcceleration * fDt);
		// Stop horizontal motion explicitly: without this, residual XZ
		// velocity from the last steering tick would keep sliding the
		// body until friction (if any) or wall collision damped it. We
		// keep the body's existing Y velocity so gravity / impulses /
		// fall-from-cliff motion can play out -- only the navmesh-
		// driven intent is zeroed.
		if (bUsePhysics)
		{
			const JPH::BodyID& xBodyID = pxCollider->GetBodyID();
			const Zenith_Maths::Vector3 xCurVel = g_xEngine.Physics().GetLinearVelocity(xBodyID);
			g_xEngine.Physics().SetLinearVelocity(xBodyID,
				Zenith_Maths::Vector3(0.0f, xCurVel.y, 0.0f));
		}
		return;
	}

	Zenith_Maths::Vector3 xCurrentPos;
	xTransform.GetPosition(xCurrentPos);

	// Store current position for batch pathfinding requests
	m_xPathStartPos = xCurrentPos;

	// Calculate path if we need one and it wasn't provided by batch processing
	// (m_bPathPending means SetDestination was called but path hasn't been computed yet)
	if (!HasPath() && m_bPathPending)
	{
		// Fallback: compute path synchronously if batch processing wasn't used
		m_xCurrentPath = Zenith_Pathfinding::FindPath(*m_pxNavMesh, xCurrentPos, m_xDestination);
		m_bPathPending = false;

		if (m_xCurrentPath.m_eStatus == Zenith_PathResult::Status::FAILED)
		{
			Stop();
			return;
		}

		m_uCurrentWaypoint = 0;

		// Skip first waypoint if it's very close (it's our current position)
		if (m_xCurrentPath.m_axWaypoints.GetSize() > 1)
		{
			float fDistToFirst = Zenith_Maths::Length(
				m_xCurrentPath.m_axWaypoints.Get(0) - xCurrentPos);
			if (fDistToFirst < m_fStoppingDistance)
			{
				m_uCurrentWaypoint = 1;
			}
		}
	}

	// If still no path (e.g., waiting for batch or failed), decelerate
	if (!HasPath())
	{
		m_fCurrentSpeed = std::max(0.0f, m_fCurrentSpeed - m_fAcceleration * fDt);
		return;
	}

	// Get desired velocity
	Zenith_Maths::Vector3 xNewVelocity = CalculateVelocity(fDt, xCurrentPos);

	if (bUsePhysics)
	{
		// Physics-driven motion: hand the full navmesh-derived velocity
		// to Jolt and let it integrate.
		//
		// The Y component is meaningful: on a flat navmesh (post-project
		// steering) it's 0 and the agent rests on the floor (gravity
		// pushes back into floor collision, equilibrium); on a sloped
		// navmesh it carries the climb/descend intent and the agent
		// follows the slope. Jolt's gravity (if enabled) acts each step
		// as an additional acceleration -- not stomped, just composed
		// with the agent's intent over the step.
		//
		// We deliberately don't preserve the body's PREVIOUS Y velocity
		// here: external impulses (jumps, knockback) propagating into
		// navmesh-driven motion would fight the path. Path-following
		// is supposed to be authoritative for the agent's intent;
		// gravity + collision response handle the rest of the physics
		// story.
		g_xEngine.Physics().SetLinearVelocity(pxCollider->GetBodyID(), xNewVelocity);
	}
	else
	{
		// Legacy transform-only path. Used by test fixtures that drive
		// the agent without a physics body; also the only fallback when
		// a runtime agent's body is non-dynamic (kinematic colliders
		// don't accept SetLinearVelocity from gameplay).
		const Zenith_Maths::Vector3 xNewPos = xCurrentPos + xNewVelocity * fDt;
		xTransform.SetPosition(xNewPos);
	}

	// Update facing direction (rotate towards movement direction)
	if (Zenith_Maths::LengthSq(xNewVelocity) > 0.01f)
	{
		Zenith_Maths::Vector3 xMoveDir = Zenith_Maths::Normalize(xNewVelocity);

		// Calculate target rotation
		float fTargetYaw = std::atan2(xMoveDir.x, xMoveDir.z);

		// Get current rotation as Euler angles
		Zenith_Maths::Quaternion xCurrentQuat;
		xTransform.GetRotation(xCurrentQuat);
		Zenith_Maths::Vector3 xCurrentEuler = glm::eulerAngles(xCurrentQuat);
		float fCurrentYaw = xCurrentEuler.y;

		// Smoothly interpolate rotation
		float fMaxRotation = m_fTurnSpeed * (3.14159265f / 180.0f) * fDt;
		float fDiff = fTargetYaw - fCurrentYaw;

		// Normalize angle difference to [-PI, PI]
		while (fDiff > 3.14159265f) fDiff -= 2.0f * 3.14159265f;
		while (fDiff < -3.14159265f) fDiff += 2.0f * 3.14159265f;

		float fRotation = std::max(-fMaxRotation, std::min(fMaxRotation, fDiff));
		xCurrentEuler.y = fCurrentYaw + fRotation;
		xTransform.SetRotation(Zenith_Maths::Quaternion(xCurrentEuler));
	}
}

Zenith_Maths::Vector3 Zenith_NavMeshAgent::CalculateVelocity(float fDt,
	const Zenith_Maths::Vector3& xCurrentPosition)
{
	if (!HasPath() || m_bReachedDestination)
	{
		// Decelerate to stop
		m_fCurrentSpeed = std::max(0.0f, m_fCurrentSpeed - m_fAcceleration * fDt);
		if (m_fCurrentSpeed < 0.001f)
		{
			m_xVelocity = Zenith_Maths::Vector3(0.0f);
		}
		return m_xVelocity;
	}

	// Steer in navmesh-space, not world-space.
	//
	// Why: the agent entity sits at world Y = polygon_Y + half-capsule
	// (dynamic capsule resting on the floor), while waypoints sit at
	// polygon_Y. Steering 3D-from-world-position produces a vector
	// with a constant downward Y component equal to half-capsule-height.
	// 3D normalisation then channels most of m_fMoveSpeed into a
	// vertical pull -- the agent's velocity slams the capsule into the
	// floor, Jolt resolves the collision by popping the capsule back
	// up, and only a small fraction of the speed reaches the horizontal
	// plane. A 1.5 m capsule offset over a 2 m waypoint distance gives
	// (XZ, Y) = (1.3 m, -1.5 m); 3D-normalised that's only 65 % of the
	// move speed reaching XZ. The priest's 7 m/s effectively drops to
	// ~4.5 m/s, sometimes worse, and the apprehend channel can't hold
	// range on a walking villager.
	//
	// Project the agent's current world position onto the navmesh
	// surface FIRST. After projection, both the projected position and
	// the waypoint live on the polygon, so xToTarget.y now reflects
	// REAL navmesh elevation changes (ramps, stairs, multi-level
	// navmesh) rather than the constant capsule offset. Planar
	// navmeshes (DP's procgen) end up with xToTarget.y exactly 0;
	// non-planar navmeshes (future games with ramps, jump-links, or
	// multi-floor levels) get correct slope-aware steering.
	//
	// The capsule's actual Y in world space is then a physics concern:
	// gravity, ramp collision, and per-frame velocity preservation
	// (see SetDestination + Update) keep the body at its resting height
	// or sliding correctly along sloped surfaces.
	Zenith_Maths::Vector3 xSteerOrigin = xCurrentPosition;
	if (m_pxNavMesh != nullptr)
	{
		Zenith_Maths::Vector3 xProjected = xCurrentPosition;
		if (m_pxNavMesh->ProjectPoint(xCurrentPosition, xProjected, /*fMaxDist=*/10.0f))
		{
			xSteerOrigin = xProjected;
		}
	}

	// Get current target waypoint
	Zenith_Maths::Vector3 xTarget   = GetCurrentTargetWaypoint();
	Zenith_Maths::Vector3 xToTarget = xTarget - xSteerOrigin;
	float fDistToTarget = Zenith_Maths::Length(xToTarget);

	// Check if we've reached current waypoint
	if (fDistToTarget < m_fStoppingDistance)
	{
		AdvanceWaypoint();

		// Check if we're done
		if (m_uCurrentWaypoint >= m_xCurrentPath.m_axWaypoints.GetSize())
		{
			m_bReachedDestination = true;
			m_fCurrentSpeed = 0.0f;
			m_xVelocity = Zenith_Maths::Vector3(0.0f);
			return m_xVelocity;
		}

		// Get new target
		xTarget   = GetCurrentTargetWaypoint();
		xToTarget = xTarget - xSteerOrigin;
		fDistToTarget = Zenith_Maths::Length(xToTarget);
	}

	// Calculate desired direction
	Zenith_Maths::Vector3 xDesiredDir = Zenith_Maths::Vector3(0.0f);
	if (fDistToTarget > 0.001f)
	{
		xDesiredDir = xToTarget / fDistToTarget;
	}

	// Calculate desired speed (slow down as we approach destination)
	float fDesiredSpeed = m_fMoveSpeed;

	// Check total remaining distance (distance to current waypoint + rest of path)
	float fRemainingDist = fDistToTarget + GetRemainingDistance();
	if (fRemainingDist < m_fStoppingDistance * 2.0f)
	{
		// Close to final destination - slow down
		float fSlowdownFactor = fRemainingDist / (m_fStoppingDistance * 2.0f);
		fDesiredSpeed *= fSlowdownFactor;
	}

	// Accelerate/decelerate towards desired speed
	if (m_fCurrentSpeed < fDesiredSpeed)
	{
		m_fCurrentSpeed = std::min(fDesiredSpeed, m_fCurrentSpeed + m_fAcceleration * fDt);
	}
	else if (m_fCurrentSpeed > fDesiredSpeed)
	{
		m_fCurrentSpeed = std::max(fDesiredSpeed, m_fCurrentSpeed - m_fAcceleration * fDt);
	}

	m_xVelocity = xDesiredDir * m_fCurrentSpeed;
	return m_xVelocity;
}

Zenith_Maths::Vector3 Zenith_NavMeshAgent::GetCurrentTargetWaypoint() const
{
	if (m_uCurrentWaypoint < m_xCurrentPath.m_axWaypoints.GetSize())
	{
		return m_xCurrentPath.m_axWaypoints.Get(m_uCurrentWaypoint);
	}
	return m_xDestination;
}

void Zenith_NavMeshAgent::AdvanceWaypoint()
{
	++m_uCurrentWaypoint;
}

Zenith_Maths::Vector3 Zenith_NavMeshAgent::SteerTowards(const Zenith_Maths::Vector3& xTarget,
	const Zenith_Maths::Vector3& xCurrentPos)
{
	Zenith_Maths::Vector3 xToTarget = xTarget - xCurrentPos;
	float fDist = Zenith_Maths::Length(xToTarget);

	if (fDist < 0.001f)
	{
		return Zenith_Maths::Vector3(0.0f);
	}

	return xToTarget / fDist;
}

#ifdef ZENITH_TOOLS
void Zenith_NavMeshAgent::DebugDraw(const Zenith_Maths::Vector3& xAgentPosition) const
{
	if (!Zenith_AIDebugVariables::s_bEnableAllAIDebug)
	{
		return;
	}

	if (!HasPath())
	{
		return;
	}

	const Zenith_Maths::Vector3 xPathColor(1.0f, 1.0f, 0.0f);      // Yellow
	const Zenith_Maths::Vector3 xWaypointColor(1.0f, 0.5f, 0.0f);  // Orange
	const Zenith_Maths::Vector3 xTargetColor(0.0f, 1.0f, 0.0f);    // Green

	// Draw path lines (controlled by s_bDrawAgentPaths)
	if (Zenith_AIDebugVariables::s_bDrawAgentPaths)
	{
		// Draw line from agent to current waypoint
		if (m_uCurrentWaypoint < m_xCurrentPath.m_axWaypoints.GetSize())
		{
			g_xEngine.Primitives().AddLine(xAgentPosition,
				m_xCurrentPath.m_axWaypoints.Get(m_uCurrentWaypoint),
				xPathColor, 0.03f);
		}

		// Draw remaining path
		for (uint32_t u = m_uCurrentWaypoint; u + 1 < m_xCurrentPath.m_axWaypoints.GetSize(); ++u)
		{
			g_xEngine.Primitives().AddLine(m_xCurrentPath.m_axWaypoints.Get(u),
				m_xCurrentPath.m_axWaypoints.Get(u + 1),
				xPathColor, 0.02f);
		}
	}

	// Draw waypoints (controlled by s_bDrawPathWaypoints)
	if (Zenith_AIDebugVariables::s_bDrawPathWaypoints)
	{
		for (uint32_t u = m_uCurrentWaypoint; u < m_xCurrentPath.m_axWaypoints.GetSize(); ++u)
		{
			Zenith_Maths::Vector3 xColor = (u == m_xCurrentPath.m_axWaypoints.GetSize() - 1)
				? xTargetColor : xWaypointColor;
			g_xEngine.Primitives().AddSphere(m_xCurrentPath.m_axWaypoints.Get(u), 0.1f, xColor);
		}

		// Draw destination marker
		g_xEngine.Primitives().AddSphere(m_xDestination, 0.15f, xTargetColor);
	}
}
#endif

#ifdef ZENITH_TESTING
#include "AI/Navigation/Zenith_NavMeshAgent.Tests.inl"
#endif
