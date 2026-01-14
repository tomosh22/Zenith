#pragma once

#include "AI/Navigation/Zenith_Pathfinding.h"
#include "Maths/Zenith_Maths.h"

class Zenith_NavMesh;
class Zenith_TransformComponent;

/**
 * Zenith_NavMeshAgent - Agent movement controller on navigation mesh
 *
 * Handles pathfinding requests, path following, and steering.
 * Should be updated each frame to move the agent.
 */
class Zenith_NavMeshAgent
{
public:
	Zenith_NavMeshAgent() = default;
	~Zenith_NavMeshAgent() = default;

	// Move semantics
	Zenith_NavMeshAgent(Zenith_NavMeshAgent&&) noexcept = default;
	Zenith_NavMeshAgent& operator=(Zenith_NavMeshAgent&&) noexcept = default;

	// ========== Configuration ==========

	void SetNavMesh(const Zenith_NavMesh* pxNavMesh) { m_pxNavMesh = pxNavMesh; }
	const Zenith_NavMesh* GetNavMesh() const { return m_pxNavMesh; }

	void SetMoveSpeed(float f) { m_fMoveSpeed = f; }
	float GetMoveSpeed() const { return m_fMoveSpeed; }

	void SetTurnSpeed(float f) { m_fTurnSpeed = f; }
	float GetTurnSpeed() const { return m_fTurnSpeed; }

	void SetStoppingDistance(float f) { m_fStoppingDistance = f; }
	float GetStoppingDistance() const { return m_fStoppingDistance; }

	void SetAcceleration(float f) { m_fAcceleration = f; }
	float GetAcceleration() const { return m_fAcceleration; }

	// ========== Path Control ==========

	/**
	 * Set a new destination and calculate path
	 * @param xDestination Target position in world space
	 * @return True if a path was found
	 */
	bool SetDestination(const Zenith_Maths::Vector3& xDestination);

	/**
	 * Clear current path and stop moving
	 */
	void Stop();

	/**
	 * Check if agent currently has a path
	 */
	bool HasPath() const { return m_xCurrentPath.m_axWaypoints.GetSize() > 0; }

	/**
	 * Check if agent has reached its destination
	 */
	bool HasReachedDestination() const { return m_bReachedDestination; }

	/**
	 * Get the current path
	 */
	const Zenith_PathResult& GetCurrentPath() const { return m_xCurrentPath; }

	/**
	 * Get the current destination
	 */
	const Zenith_Maths::Vector3& GetDestination() const { return m_xDestination; }

	/**
	 * Get remaining distance to destination
	 */
	float GetRemainingDistance() const;

	// ========== Batch Pathfinding Support ==========

	/**
	 * Check if this agent needs a path calculated (has pending destination, no path yet)
	 * Used for batch pathfinding
	 */
	bool NeedsPath() const { return m_bPathPending && !HasPath(); }

	/**
	 * Set path result from external batch calculation
	 * @param xResult Path result to apply
	 */
	void SetPathResult(const Zenith_PathResult& xResult);

	/**
	 * Set the start position for batch path calculation
	 * Call this before collecting path requests for batch processing
	 */
	void SetStartPosition(const Zenith_Maths::Vector3& xStart) { m_xPathStartPos = xStart; }

	/**
	 * Get the pending path request info (for batch processing)
	 * @param xStartOut Receives start position
	 * @param xEndOut Receives destination
	 * @return True if this agent has a pending path request
	 */
	bool GetPendingPathRequest(Zenith_Maths::Vector3& xStartOut, Zenith_Maths::Vector3& xEndOut) const;

	// ========== Update ==========

	/**
	 * Update agent movement for one frame
	 * @param fDt Delta time
	 * @param xTransform Transform component to modify
	 */
	void Update(float fDt, Zenith_TransformComponent& xTransform);

	/**
	 * Update agent and return desired velocity (without modifying transform)
	 * @param fDt Delta time
	 * @param xCurrentPosition Current agent position
	 * @return Desired velocity vector
	 */
	Zenith_Maths::Vector3 CalculateVelocity(float fDt, const Zenith_Maths::Vector3& xCurrentPosition);

	// ========== Debug ==========

	/**
	 * Get current waypoint being navigated to
	 */
	uint32_t GetCurrentWaypointIndex() const { return m_uCurrentWaypoint; }

	/**
	 * Get current movement velocity
	 */
	const Zenith_Maths::Vector3& GetVelocity() const { return m_xVelocity; }

#ifdef ZENITH_TOOLS
	void DebugDraw(const Zenith_Maths::Vector3& xAgentPosition) const;
#endif

private:
	// NavMesh reference
	const Zenith_NavMesh* m_pxNavMesh = nullptr;

	// Current path
	Zenith_PathResult m_xCurrentPath;
	uint32_t m_uCurrentWaypoint = 0;
	Zenith_Maths::Vector3 m_xDestination;
	Zenith_Maths::Vector3 m_xPathStartPos;  // For batch pathfinding
	bool m_bReachedDestination = false;
	bool m_bPathPending = false;  // True when destination set but path not yet calculated

	// Movement parameters
	float m_fMoveSpeed = 5.0f;           // Units per second
	float m_fTurnSpeed = 360.0f;         // Degrees per second
	float m_fStoppingDistance = 0.2f;    // Distance to stop from waypoint
	float m_fAcceleration = 20.0f;       // Speed change per second

	// Current state
	Zenith_Maths::Vector3 m_xVelocity = Zenith_Maths::Vector3(0.0f);
	float m_fCurrentSpeed = 0.0f;

	// Helpers
	Zenith_Maths::Vector3 GetCurrentTargetWaypoint() const;
	void AdvanceWaypoint();
	Zenith_Maths::Vector3 SteerTowards(const Zenith_Maths::Vector3& xTarget,
		const Zenith_Maths::Vector3& xCurrentPos, float fDt);
};
