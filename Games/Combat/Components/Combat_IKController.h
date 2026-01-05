#pragma once
/**
 * Combat_IKController.h - Inverse Kinematics for combat characters
 *
 * Demonstrates:
 * - Foot placement IK using raycasts
 * - Look-at IK for head tracking
 * - IK blending with animation
 * - Disabling IK during certain states
 *
 * For procedural capsule characters, this simulates IK effects
 * by providing offsets and rotations that would be applied to a
 * skeletal mesh's IK solver.
 */

#include "Flux/MeshAnimation/Flux_InverseKinematics.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"
#include "Combat_QueryHelper.h"

// ============================================================================
// IK Configuration
// ============================================================================

struct Combat_IKConfig
{
	// Foot IK
	float m_fFootIKRaycastHeight = 1.0f;    // Height above foot to start raycast
	float m_fFootIKRaycastDistance = 1.5f;  // Max raycast distance
	float m_fFootIKBlendSpeed = 10.0f;      // How fast IK blends in/out

	// Look-at IK
	float m_fLookAtMaxAngle = 1.2f;         // ~70 degrees max rotation
	float m_fLookAtBlendSpeed = 5.0f;       // How fast head turns

	// Body offsets
	float m_fMaxBodyOffset = 0.2f;          // Max vertical body adjustment
};

// ============================================================================
// IK State
// ============================================================================

/**
 * Combat_IKState - Current IK solution state
 *
 * Since we're using procedural capsules, this stores the computed
 * offsets that would normally be applied via Flux_IKSolver.
 */
struct Combat_IKState
{
	// Foot placement (simulated)
	float m_fLeftFootOffset = 0.0f;
	float m_fRightFootOffset = 0.0f;
	Zenith_Maths::Quat m_xLeftFootRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Quat m_xRightFootRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);

	// Body adjustment
	float m_fBodyVerticalOffset = 0.0f;

	// Look-at
	Zenith_Maths::Quat m_xHeadRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	float m_fLookAtWeight = 0.0f;

	// Blend weights
	float m_fFootIKWeight = 0.0f;
};

// ============================================================================
// IK Controller
// ============================================================================

/**
 * Combat_IKController - Manages IK for a combat character
 *
 * This controller demonstrates the IK concepts without requiring
 * actual skeletal animation. For a real character with bones,
 * you would use Flux_IKSolver directly.
 */
class Combat_IKController
{
public:
	// ========================================================================
	// Configuration
	// ========================================================================

	void SetConfig(const Combat_IKConfig& xConfig) { m_xConfig = xConfig; }
	const Combat_IKConfig& GetConfig() const { return m_xConfig; }

	// ========================================================================
	// Enable/Disable
	// ========================================================================

	void SetFootIKEnabled(bool bEnabled) { m_bFootIKEnabled = bEnabled; }
	void SetLookAtIKEnabled(bool bEnabled) { m_bLookAtIKEnabled = bEnabled; }
	bool IsFootIKEnabled() const { return m_bFootIKEnabled; }
	bool IsLookAtIKEnabled() const { return m_bLookAtIKEnabled; }

	// ========================================================================
	// State Access
	// ========================================================================

	const Combat_IKState& GetState() const { return m_xState; }

	// ========================================================================
	// Update
	// ========================================================================

	/**
	 * Update - Compute IK solution for current frame
	 *
	 * @param xTransform Character's transform component
	 * @param xTargetLookAt Position to look at (e.g., nearest enemy)
	 * @param fGroundHeight Ground height at character position
	 * @param bCanUseIK Whether IK should be active (false during dodge, death)
	 * @param fDt Delta time
	 */
	void Update(
		Zenith_TransformComponent& xTransform,
		const Zenith_Maths::Vector3& xTargetLookAt,
		float fGroundHeight,
		bool bCanUseIK,
		float fDt)
	{
		// Get character position and rotation
		Zenith_Maths::Vector3 xPosition;
		Zenith_Maths::Quat xRotation;
		xTransform.GetPosition(xPosition);
		xTransform.GetRotation(xRotation);

		// Update foot IK
		if (m_bFootIKEnabled && bCanUseIK)
		{
			UpdateFootIK(xPosition, xRotation, fGroundHeight, fDt);
		}
		else
		{
			// Blend out foot IK
			m_xState.m_fFootIKWeight = glm::mix(m_xState.m_fFootIKWeight, 0.0f, fDt * m_xConfig.m_fFootIKBlendSpeed);
		}

		// Update look-at IK
		if (m_bLookAtIKEnabled && bCanUseIK)
		{
			UpdateLookAtIK(xPosition, xRotation, xTargetLookAt, fDt);
		}
		else
		{
			// Blend out look-at IK
			m_xState.m_fLookAtWeight = glm::mix(m_xState.m_fLookAtWeight, 0.0f, fDt * m_xConfig.m_fLookAtBlendSpeed);
		}
	}

	/**
	 * UpdateWithAutoTarget - Automatically find nearest enemy for look-at
	 */
	void UpdateWithAutoTarget(
		Zenith_TransformComponent& xTransform,
		Zenith_EntityID uOwnerEntityID,
		float fGroundHeight,
		bool bCanUseIK,
		float fDt)
	{
		Zenith_Maths::Vector3 xPosition;
		xTransform.GetPosition(xPosition);

		// Find look-at target
		Zenith_Maths::Vector3 xLookTarget = xPosition + Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);

		if (Combat_QueryHelper::IsPlayer(uOwnerEntityID))
		{
			// Player looks at nearest enemy
			Zenith_EntityID uNearestEnemy = Combat_QueryHelper::FindNearestEnemy(xPosition);
			if (uNearestEnemy != INVALID_ENTITY_ID)
			{
				Combat_QueryHelper::GetEntityPosition(uNearestEnemy, xLookTarget);
				xLookTarget.y = xPosition.y + 1.5f;  // Look at head height
			}
		}
		else if (Combat_QueryHelper::IsEnemy(uOwnerEntityID))
		{
			// Enemy looks at player
			xLookTarget = Combat_QueryHelper::GetPlayerPosition();
			xLookTarget.y = xPosition.y + 1.5f;
		}

		Update(xTransform, xLookTarget, fGroundHeight, bCanUseIK, fDt);
	}

	// ========================================================================
	// Reset
	// ========================================================================

	void Reset()
	{
		m_xState = Combat_IKState();
	}

private:
	// ========================================================================
	// Foot IK (Simulated)
	// ========================================================================

	void UpdateFootIK(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Quat& xRotation,
		float fGroundHeight,
		float fDt)
	{
		// Calculate foot positions relative to character
		Zenith_Maths::Vector3 xForward = xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		Zenith_Maths::Vector3 xRight = xRotation * Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);

		// Foot spacing
		static constexpr float s_fFootSpacing = 0.3f;

		Zenith_Maths::Vector3 xLeftFootPos = xPosition + xRight * (-s_fFootSpacing);
		Zenith_Maths::Vector3 xRightFootPos = xPosition + xRight * s_fFootSpacing;

		// Simulate raycast results (in real game, use physics raycasts)
		// For now, assume flat ground at fGroundHeight
		float fLeftGroundHeight = fGroundHeight;
		float fRightGroundHeight = fGroundHeight;

		// Calculate foot offsets
		float fTargetLeftOffset = fLeftGroundHeight - (xPosition.y - 1.0f);  // Assuming 1.0 foot offset
		float fTargetRightOffset = fRightGroundHeight - (xPosition.y - 1.0f);

		// Clamp offsets
		fTargetLeftOffset = glm::clamp(fTargetLeftOffset, -m_xConfig.m_fMaxBodyOffset, m_xConfig.m_fMaxBodyOffset);
		fTargetRightOffset = glm::clamp(fTargetRightOffset, -m_xConfig.m_fMaxBodyOffset, m_xConfig.m_fMaxBodyOffset);

		// Smooth blend
		m_xState.m_fLeftFootOffset = glm::mix(m_xState.m_fLeftFootOffset, fTargetLeftOffset, fDt * m_xConfig.m_fFootIKBlendSpeed);
		m_xState.m_fRightFootOffset = glm::mix(m_xState.m_fRightFootOffset, fTargetRightOffset, fDt * m_xConfig.m_fFootIKBlendSpeed);

		// Calculate body offset (average of foot offsets)
		float fTargetBodyOffset = (m_xState.m_fLeftFootOffset + m_xState.m_fRightFootOffset) * 0.5f;
		m_xState.m_fBodyVerticalOffset = glm::mix(m_xState.m_fBodyVerticalOffset, fTargetBodyOffset, fDt * m_xConfig.m_fFootIKBlendSpeed);

		// Blend in foot IK weight
		m_xState.m_fFootIKWeight = glm::mix(m_xState.m_fFootIKWeight, 1.0f, fDt * m_xConfig.m_fFootIKBlendSpeed);
	}

	// ========================================================================
	// Look-At IK
	// ========================================================================

	void UpdateLookAtIK(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Quat& xRotation,
		const Zenith_Maths::Vector3& xTargetPos,
		float fDt)
	{
		// Get direction to target
		Zenith_Maths::Vector3 xToTarget = xTargetPos - xPosition;

		// Skip if target is too close
		if (glm::length(xToTarget) < 0.5f)
		{
			m_xState.m_fLookAtWeight = glm::mix(m_xState.m_fLookAtWeight, 0.0f, fDt * m_xConfig.m_fLookAtBlendSpeed);
			return;
		}

		xToTarget = glm::normalize(xToTarget);

		// Get character's forward direction
		Zenith_Maths::Vector3 xForward = xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		Zenith_Maths::Vector3 xUp = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);

		// Calculate look-at rotation using the engine's utility function
		// SolveLookAtIK returns a quaternion rotation to apply
		Zenith_Maths::Quat xTargetHeadRot = SolveLookAtIK(
			xPosition + Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f),  // Head position
			xForward,
			xUp,
			xTargetPos,
			m_xConfig.m_fLookAtMaxAngle);

		// Smooth blend rotation
		m_xState.m_xHeadRotation = glm::slerp(m_xState.m_xHeadRotation, xTargetHeadRot, fDt * m_xConfig.m_fLookAtBlendSpeed);

		// Blend in look-at weight
		m_xState.m_fLookAtWeight = glm::mix(m_xState.m_fLookAtWeight, 1.0f, fDt * m_xConfig.m_fLookAtBlendSpeed);
	}

	// ========================================================================
	// Data
	// ========================================================================

	Combat_IKConfig m_xConfig;
	Combat_IKState m_xState;

	bool m_bFootIKEnabled = true;
	bool m_bLookAtIKEnabled = true;
};

// ============================================================================
// Utility: Apply IK to Visual
// ============================================================================

/**
 * ApplyIKToTransform - Apply IK offsets to a procedural character
 *
 * For capsule-based characters, this applies the body vertical offset.
 * For skeletal characters, you would use Flux_IKSolver::Solve() instead.
 */
inline void ApplyIKToTransform(
	Zenith_TransformComponent& xTransform,
	const Combat_IKState& xIKState)
{
	// Apply body vertical offset
	Zenith_Maths::Vector3 xPos;
	xTransform.GetPosition(xPos);
	xPos.y += xIKState.m_fBodyVerticalOffset * xIKState.m_fFootIKWeight;
	xTransform.SetPosition(xPos);
}
