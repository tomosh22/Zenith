#pragma once
/**
 * Combat_HitDetection.h - Physics-based hit detection
 *
 * Demonstrates:
 * - Distance-based hit detection during attack frames
 * - Hit registration with cooldown to prevent multi-hits
 * - Attack hitbox management
 * - Integration with damage system via events
 *
 * Since we're using capsule colliders without skeletal hitboxes,
 * hit detection is based on distance and attack state timing.
 */

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Maths/Zenith_Maths.h"
#include "Combat_QueryHelper.h"
#include "Combat_DamageSystem.h"
#include <unordered_set>

// ============================================================================
// Hit Registration Data
// ============================================================================

struct Combat_HitInfo
{
	Zenith_EntityID m_uTargetEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID m_uAttackerEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 m_xHitPoint = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xHitDirection = Zenith_Maths::Vector3(0.0f);
	float m_fDamage = 0.0f;
	bool m_bIsComboHit = false;
	uint32_t m_uComboCount = 0;
};

// ============================================================================
// Attack Hitbox
// ============================================================================

struct Combat_AttackHitbox
{
	Zenith_Maths::Vector3 m_xOffset = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);  // Forward offset
	float m_fRadius = 1.0f;
	float m_fDamage = 10.0f;
	bool m_bIsActive = false;
};

// ============================================================================
// Hit Detection Manager
// ============================================================================

/**
 * Combat_HitDetection - Manages hit detection for an attacker
 *
 * Usage:
 *   Combat_HitDetection xHitDetection;
 *   xHitDetection.SetOwner(uPlayerID);
 *
 *   // When attack starts
 *   xHitDetection.ActivateHitbox(fDamage, fRange, uComboCount);
 *
 *   // Each frame during attack
 *   xHitDetection.Update(xTransform);
 *
 *   // When attack ends
 *   xHitDetection.DeactivateHitbox();
 */
class Combat_HitDetection
{
public:
	// ========================================================================
	// Configuration
	// ========================================================================

	void SetOwner(Zenith_EntityID uOwnerID) { m_uOwnerEntityID = uOwnerID; }
	Zenith_EntityID GetOwner() const { return m_uOwnerEntityID; }

	// ========================================================================
	// Hitbox Control
	// ========================================================================

	/**
	 * ActivateHitbox - Enable hit detection with parameters
	 */
	void ActivateHitbox(float fDamage, float fRange, uint32_t uComboCount = 0, bool bIsCombo = false)
	{
		m_xHitbox.m_bIsActive = true;
		m_xHitbox.m_fDamage = fDamage;
		m_xHitbox.m_fRadius = fRange;
		m_uCurrentComboCount = uComboCount;
		m_bIsComboHit = bIsCombo;

		// Clear hit entities for new attack
		m_xHitEntities.clear();
	}

	/**
	 * DeactivateHitbox - Disable hit detection
	 */
	void DeactivateHitbox()
	{
		m_xHitbox.m_bIsActive = false;
		m_xHitEntities.clear();
	}

	bool IsHitboxActive() const { return m_xHitbox.m_bIsActive; }

	// ========================================================================
	// Update
	// ========================================================================

	/**
	 * Update - Check for hits this frame
	 *
	 * @param xTransform Attacker's transform
	 * @return Number of new hits registered this frame
	 */
	uint32_t Update(Zenith_TransformComponent& xTransform)
	{
		if (!m_xHitbox.m_bIsActive)
			return 0;

		Zenith_Maths::Vector3 xPosition;
		Zenith_Maths::Quat xRotation;
		xTransform.GetPosition(xPosition);
		xTransform.GetRotation(xRotation);

		// Calculate hitbox center (offset by facing direction)
		Zenith_Maths::Vector3 xForward = xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		Zenith_Maths::Vector3 xHitboxCenter = xPosition + xForward * m_xHitbox.m_xOffset.z;
		xHitboxCenter.y += 1.0f;  // Center height

		// Find potential targets
		std::vector<Combat_EntityDistance> axTargets;
		if (Combat_QueryHelper::IsPlayer(m_uOwnerEntityID))
		{
			// Player attacks enemies
			axTargets = Combat_QueryHelper::FindEnemiesInRange(xHitboxCenter, m_xHitbox.m_fRadius);
		}
		else
		{
			// Enemy attacks player
			Zenith_EntityID uPlayerID = Combat_QueryHelper::FindPlayer();
			if (uPlayerID != INVALID_ENTITY_ID)
			{
				Zenith_Maths::Vector3 xPlayerPos;
				if (Combat_QueryHelper::GetEntityPosition(uPlayerID, xPlayerPos))
				{
					float fDist = glm::distance(xHitboxCenter, xPlayerPos);
					if (fDist <= m_xHitbox.m_fRadius)
					{
						Combat_EntityDistance xTarget;
						xTarget.m_uEntityID = uPlayerID;
						xTarget.m_fDistance = fDist;
						xTarget.m_xPosition = xPlayerPos;
						axTargets.push_back(xTarget);
					}
				}
			}
		}

		// Register hits
		uint32_t uHitCount = 0;
		for (const Combat_EntityDistance& xTarget : axTargets)
		{
			// Skip already hit entities (prevents multi-hit)
			if (m_xHitEntities.find(xTarget.m_uEntityID) != m_xHitEntities.end())
				continue;

			// Register hit
			RegisterHit(xTarget, xPosition, xForward);
			m_xHitEntities.insert(xTarget.m_uEntityID);
			uHitCount++;
		}

		return uHitCount;
	}

	// ========================================================================
	// Hit Queries
	// ========================================================================

	const std::unordered_set<Zenith_EntityID>& GetHitEntities() const { return m_xHitEntities; }
	uint32_t GetHitCount() const { return static_cast<uint32_t>(m_xHitEntities.size()); }

private:
	// ========================================================================
	// Hit Registration
	// ========================================================================

	void RegisterHit(const Combat_EntityDistance& xTarget, const Zenith_Maths::Vector3& xAttackerPos, const Zenith_Maths::Vector3& xAttackDir)
	{
		// Calculate hit point (between attacker and target)
		Zenith_Maths::Vector3 xHitPoint = (xAttackerPos + xTarget.m_xPosition) * 0.5f;
		xHitPoint.y += 1.0f;

		// Calculate hit direction (from attacker to target)
		Zenith_Maths::Vector3 xHitDir = xTarget.m_xPosition - xAttackerPos;
		xHitDir.y = 0.0f;
		if (glm::length(xHitDir) > 0.01f)
			xHitDir = glm::normalize(xHitDir);
		else
			xHitDir = xAttackDir;

		// Dispatch damage event
		Combat_DamageEvent xDamageEvent;
		xDamageEvent.m_uTargetEntityID = xTarget.m_uEntityID;
		xDamageEvent.m_uAttackerEntityID = m_uOwnerEntityID;
		xDamageEvent.m_fDamage = m_xHitbox.m_fDamage;
		xDamageEvent.m_xHitPoint = xHitPoint;
		xDamageEvent.m_xHitDirection = xHitDir;
		xDamageEvent.m_bIsComboHit = m_bIsComboHit;
		xDamageEvent.m_uComboCount = m_uCurrentComboCount;

		Zenith_EventDispatcher::Get().Dispatch(xDamageEvent);
	}

	// ========================================================================
	// Data
	// ========================================================================

	Zenith_EntityID m_uOwnerEntityID = INVALID_ENTITY_ID;
	Combat_AttackHitbox m_xHitbox;

	// Entities hit during current attack (prevents multi-hit)
	std::unordered_set<Zenith_EntityID> m_xHitEntities;

	uint32_t m_uCurrentComboCount = 0;
	bool m_bIsComboHit = false;
};

// ============================================================================
// Collision Callback Handler
// ============================================================================

/**
 * Combat_CollisionHandler - Handles OnCollisionEnter for combat entities
 *
 * This would be integrated with Zenith_ScriptBehaviour::OnCollisionEnter
 * for physics-based collision detection as an alternative to distance checks.
 */
class Combat_CollisionHandler
{
public:
	/**
	 * HandleCollision - Process a physics collision
	 *
	 * @param uThisEntityID The entity receiving the collision
	 * @param uOtherEntityID The colliding entity
	 */
	static void HandleCollision(Zenith_EntityID uThisEntityID, Zenith_EntityID uOtherEntityID)
	{
		// Determine if this is an attack collision
		// In a full implementation, you would check if the other entity
		// has an active attack hitbox

		// For now, collisions are handled via the distance-based
		// Combat_HitDetection system during attack frames
	}
};
