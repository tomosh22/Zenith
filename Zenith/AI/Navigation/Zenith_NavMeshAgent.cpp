#include "Zenith.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Profiling/Zenith_Profiling.h"

#ifdef ZENITH_TOOLS
#include "Flux/Primitives/Flux_Primitives.h"
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

	// Clear velocity for fresh start
	m_xVelocity = Zenith_Maths::Vector3(0.0f);
	m_fCurrentSpeed = 0.0f;

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

void Zenith_NavMeshAgent::Update(float fDt, Zenith_TransformComponent& xTransform)
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_NAVMESH_AGENT_UPDATE);

	if (m_bReachedDestination || m_pxNavMesh == nullptr)
	{
		// Apply deceleration when stopped
		m_fCurrentSpeed = std::max(0.0f, m_fCurrentSpeed - m_fAcceleration * fDt);
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

	// Apply velocity to position
	Zenith_Maths::Vector3 xNewPos = xCurrentPos + xNewVelocity * fDt;

	// Update transform
	xTransform.SetPosition(xNewPos);

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

	// Get current target waypoint
	Zenith_Maths::Vector3 xTarget = GetCurrentTargetWaypoint();
	Zenith_Maths::Vector3 xToTarget = xTarget - xCurrentPosition;
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
		xTarget = GetCurrentTargetWaypoint();
		xToTarget = xTarget - xCurrentPosition;
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
			Flux_Primitives::AddLine(xAgentPosition,
				m_xCurrentPath.m_axWaypoints.Get(m_uCurrentWaypoint),
				xPathColor, 0.03f);
		}

		// Draw remaining path
		for (uint32_t u = m_uCurrentWaypoint; u + 1 < m_xCurrentPath.m_axWaypoints.GetSize(); ++u)
		{
			Flux_Primitives::AddLine(m_xCurrentPath.m_axWaypoints.Get(u),
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
			Flux_Primitives::AddSphere(m_xCurrentPath.m_axWaypoints.Get(u), 0.1f, xColor);
		}

		// Draw destination marker
		Flux_Primitives::AddSphere(m_xDestination, 0.15f, xTargetColor);
	}
}
#endif
