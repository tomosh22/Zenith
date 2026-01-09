#pragma once
/**
 * Combat_EnemyAI.h - Simple enemy behavior
 *
 * Demonstrates:
 * - Finding player via Zenith_Query
 * - Chase behavior with arrival distance
 * - Attack decision based on range and cooldown
 * - Hit reaction and knockback response
 *
 * Enemies will chase the player and attack when in range.
 */

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Combat_QueryHelper.h"
#include "Combat_DamageSystem.h"
#include "Combat_HitDetection.h"
#include "Combat_AnimationController.h"
#include "Combat_IKController.h"

// ============================================================================
// Enemy State
// ============================================================================

enum class Combat_EnemyState : uint8_t
{
	IDLE,
	CHASING,
	ATTACKING,
	HIT_STUN,
	DEAD
};

// ============================================================================
// Enemy AI Configuration
// ============================================================================

struct Combat_EnemyConfig
{
	float m_fMoveSpeed = 3.0f;
	float m_fRotationSpeed = 8.0f;
	float m_fDetectionRange = 15.0f;
	float m_fAttackRange = 1.2f;  // Melee attack range
	float m_fChaseStopDistance = 0.8f;  // Stop just inside attack range
	float m_fAttackDamage = 15.0f;
	float m_fAttackDuration = 0.4f;
	float m_fAttackCooldown = 1.5f;
	float m_fHitStunDuration = 0.3f;
};

// ============================================================================
// Enemy AI Controller
// ============================================================================

/**
 * Combat_EnemyAI - Controls a single enemy's behavior
 */
class Combat_EnemyAI
{
public:
	// ========================================================================
	// Initialization
	// ========================================================================

	void Initialize(Zenith_EntityID uEntityID, const Combat_EnemyConfig& xConfig, Flux_SkeletonInstance* pxSkeleton = nullptr)
	{
		m_uEntityID = uEntityID;
		m_xConfig = xConfig;
		m_eState = Combat_EnemyState::IDLE;

		// Initialize subsystems
		m_xHitDetection.SetOwner(uEntityID);
		if (pxSkeleton)
		{
			m_xAnimController.Initialize(pxSkeleton);
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[Enemy %u] Animation controller initialized with skeleton", uEntityID.m_uIndex);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[Enemy %u] WARNING: No skeleton provided, animation will not work!", uEntityID.m_uIndex);
		}
		m_xIKController.SetFootIKEnabled(true);
		m_xIKController.SetLookAtIKEnabled(true);
	}

	void Reset()
	{
		m_eState = Combat_EnemyState::IDLE;
		m_fStateTimer = 0.0f;
		m_fAttackCooldownTimer = 0.0f;
		m_xHitDetection.DeactivateHitbox();
		m_xAnimController.Reset();
		m_xIKController.Reset();
	}

	// ========================================================================
	// State Accessors
	// ========================================================================

	Combat_EnemyState GetState() const { return m_eState; }
	bool IsAlive() const { return m_eState != Combat_EnemyState::DEAD; }
	bool IsAttacking() const { return m_eState == Combat_EnemyState::ATTACKING; }
	Zenith_EntityID GetEntityID() const { return m_uEntityID; }

	const Combat_AnimationController& GetAnimController() const { return m_xAnimController; }
	const Combat_IKController& GetIKController() const { return m_xIKController; }

	// ========================================================================
	// Update
	// ========================================================================

	/**
	 * Update - Main AI update loop
	 */
	void Update(float fDt)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (!xScene.EntityExists(m_uEntityID))
			return;

		Zenith_Entity xEntity = xScene.GetEntity(m_uEntityID);
		if (!xEntity.HasComponent<Zenith_TransformComponent>())
			return;

		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

		// Enforce upright orientation every frame (collision impulses can still tip characters)
		if (xEntity.HasComponent<Zenith_ColliderComponent>())
		{
			Zenith_ColliderComponent& xCollider = xEntity.GetComponent<Zenith_ColliderComponent>();
			if (xCollider.HasValidBody())
			{
				Zenith_Physics::EnforceUpright(xCollider.GetBodyID());
			}
		}

		// Check if dead via damage system
		if (Combat_DamageSystem::IsDead(m_uEntityID))
		{
			if (m_eState != Combat_EnemyState::DEAD)
			{
				m_eState = Combat_EnemyState::DEAD;
				m_xHitDetection.DeactivateHitbox();
			}
		}

		// Update cooldowns
		if (m_fAttackCooldownTimer > 0.0f)
			m_fAttackCooldownTimer -= fDt;

		// State machine
		switch (m_eState)
		{
		case Combat_EnemyState::IDLE:
			UpdateIdleState(xTransform, fDt);
			break;

		case Combat_EnemyState::CHASING:
			UpdateChaseState(xEntity, xTransform, fDt);
			break;

		case Combat_EnemyState::ATTACKING:
			UpdateAttackState(xTransform, fDt);
			break;

		case Combat_EnemyState::HIT_STUN:
			UpdateHitStunState(fDt);
			break;

		case Combat_EnemyState::DEAD:
			// No updates when dead
			break;
		}

		// Update animation
		bool bIsAttacking = (m_eState == Combat_EnemyState::ATTACKING);
		bool bIsHit = (m_eState == Combat_EnemyState::HIT_STUN);
		bool bIsDead = (m_eState == Combat_EnemyState::DEAD);
		m_xAnimController.UpdateForEnemy(m_fCurrentSpeed, bIsAttacking, bIsHit, bIsDead, fDt);

		// Update IK
		bool bCanUseIK = (m_eState != Combat_EnemyState::DEAD && m_eState != Combat_EnemyState::HIT_STUN);
		m_xIKController.UpdateWithAutoTarget(xTransform, m_uEntityID, 0.0f, bCanUseIK, fDt);
	}

	/**
	 * TriggerHitStun - Called when enemy takes damage
	 */
	void TriggerHitStun()
	{
		if (m_eState == Combat_EnemyState::DEAD)
			return;

		m_eState = Combat_EnemyState::HIT_STUN;
		m_fStateTimer = m_xConfig.m_fHitStunDuration;
		m_xHitDetection.DeactivateHitbox();
	}

private:
	// ========================================================================
	// State Handlers
	// ========================================================================

	void UpdateIdleState(Zenith_TransformComponent& xTransform, float fDt)
	{
		m_fCurrentSpeed = 0.0f;

		// Check for player in detection range
		Zenith_EntityID uPlayerID = Combat_QueryHelper::FindPlayer();
		if (uPlayerID == INVALID_ENTITY_ID)
			return;

		Zenith_Maths::Vector3 xMyPos, xPlayerPos;
		xTransform.GetPosition(xMyPos);
		if (!Combat_QueryHelper::GetEntityPosition(uPlayerID, xPlayerPos))
			return;

		float fDist = glm::distance(xMyPos, xPlayerPos);
		if (fDist <= m_xConfig.m_fDetectionRange)
		{
			m_uTargetEntityID = uPlayerID;
			m_eState = Combat_EnemyState::CHASING;
		}
	}

	void UpdateChaseState(Zenith_Entity& xEntity, Zenith_TransformComponent& xTransform, float fDt)
	{
		// Check if player still exists
		Zenith_Maths::Vector3 xTargetPos;
		if (!Combat_QueryHelper::GetEntityPosition(m_uTargetEntityID, xTargetPos))
		{
			m_eState = Combat_EnemyState::IDLE;
			m_fCurrentSpeed = 0.0f;
			return;
		}

		// Check if player is dead
		if (Combat_DamageSystem::IsDead(m_uTargetEntityID))
		{
			m_eState = Combat_EnemyState::IDLE;
			m_fCurrentSpeed = 0.0f;
			return;
		}

		Zenith_Maths::Vector3 xMyPos;
		xTransform.GetPosition(xMyPos);

		float fDist = glm::distance(xMyPos, xTargetPos);

		// Check if in attack range
		static float s_fLogTimer = 0.0f;
		s_fLogTimer += fDt;
		if (s_fLogTimer > 1.0f)
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[Enemy %u] Chase: dist=%.2f, attackRange=%.2f, cooldown=%.2f",
				m_uEntityID.m_uIndex, fDist, m_xConfig.m_fAttackRange, m_fAttackCooldownTimer);
			s_fLogTimer = 0.0f;
		}

		if (fDist <= m_xConfig.m_fAttackRange && m_fAttackCooldownTimer <= 0.0f)
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[Enemy %u] Starting attack! dist=%.2f", m_uEntityID.m_uIndex, fDist);
			StartAttack();
			return;
		}

		// Move towards player
		if (fDist > m_xConfig.m_fChaseStopDistance)
		{
			Zenith_Maths::Vector3 xDirection = xTargetPos - xMyPos;
			xDirection.y = 0.0f;
			float fLen = glm::length(xDirection);
			if (fLen > 0.001f)
			{
				xDirection = xDirection / fLen;

				// Apply movement via physics
				if (xEntity.HasComponent<Zenith_ColliderComponent>())
				{
					Zenith_ColliderComponent& xCollider = xEntity.GetComponent<Zenith_ColliderComponent>();
					if (xCollider.HasValidBody())
					{
						Zenith_Maths::Vector3 xVelocity = xDirection * m_xConfig.m_fMoveSpeed;
						xVelocity.y = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID()).y;
						Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
					}
				}

				// Rotate towards player
				RotateTowards(xTransform, xDirection, fDt);

				m_fCurrentSpeed = m_xConfig.m_fMoveSpeed;
			}
		}
		else
		{
			// Stop movement but keep facing player
			if (xEntity.HasComponent<Zenith_ColliderComponent>())
			{
				Zenith_ColliderComponent& xCollider = xEntity.GetComponent<Zenith_ColliderComponent>();
				if (xCollider.HasValidBody())
				{
					Zenith_Maths::Vector3 xVelocity = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
					xVelocity.x = 0.0f;
					xVelocity.z = 0.0f;
					Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
				}
			}

			m_fCurrentSpeed = 0.0f;
		}
	}

	void UpdateAttackState(Zenith_TransformComponent& xTransform, float fDt)
	{
		m_fStateTimer -= fDt;
		m_fCurrentSpeed = 0.0f;

		// Check for hits during attack
		m_xHitDetection.Update(xTransform);

		// Attack finished
		if (m_fStateTimer <= 0.0f)
		{
			m_xHitDetection.DeactivateHitbox();
			m_fAttackCooldownTimer = m_xConfig.m_fAttackCooldown;
			m_eState = Combat_EnemyState::CHASING;
		}
	}

	void UpdateHitStunState(float fDt)
	{
		m_fStateTimer -= fDt;
		m_fCurrentSpeed = 0.0f;

		if (m_fStateTimer <= 0.0f)
		{
			m_eState = Combat_EnemyState::CHASING;
		}
	}

	// ========================================================================
	// Attack Logic
	// ========================================================================

	void StartAttack()
	{
		m_eState = Combat_EnemyState::ATTACKING;
		m_fStateTimer = m_xConfig.m_fAttackDuration;

		// Activate hitbox
		m_xHitDetection.ActivateHitbox(
			m_xConfig.m_fAttackDamage,
			m_xConfig.m_fAttackRange,
			0,
			false);
	}

	// ========================================================================
	// Helpers
	// ========================================================================

	void RotateTowards(Zenith_TransformComponent& xTransform, const Zenith_Maths::Vector3& xTargetDir, float fDt)
	{
		if (glm::length(xTargetDir) < 0.01f)
			return;

		Zenith_Maths::Quat xCurrentRot;
		xTransform.GetRotation(xCurrentRot);

		float fTargetYaw = atan2(xTargetDir.x, xTargetDir.z);
		Zenith_Maths::Quat xTargetRot = glm::angleAxis(fTargetYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

		Zenith_Maths::Quat xNewRot = glm::slerp(xCurrentRot, xTargetRot, fDt * m_xConfig.m_fRotationSpeed);
		xTransform.SetRotation(xNewRot);
	}

	// ========================================================================
	// Data
	// ========================================================================

	Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID m_uTargetEntityID = INVALID_ENTITY_ID;
	Combat_EnemyConfig m_xConfig;

	Combat_EnemyState m_eState = Combat_EnemyState::IDLE;
	float m_fStateTimer = 0.0f;
	float m_fAttackCooldownTimer = 0.0f;
	float m_fCurrentSpeed = 0.0f;

	Combat_HitDetection m_xHitDetection;
	Combat_AnimationController m_xAnimController;
	Combat_IKController m_xIKController;
};

// ============================================================================
// Enemy Manager
// ============================================================================

/**
 * Combat_EnemyManager - Manages all enemies in the arena
 */
class Combat_EnemyManager
{
public:
	/**
	 * RegisterEnemy - Add an enemy to the manager
	 */
	void RegisterEnemy(Zenith_EntityID uEntityID, const Combat_EnemyConfig& xConfig, Flux_SkeletonInstance* pxSkeleton = nullptr)
	{
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[EnemyManager] RegisterEnemy %u, skeleton=%p, vector size before=%zu",
			uEntityID.m_uIndex, pxSkeleton, m_axEnemies.size());
		Combat_EnemyAI xAI;
		xAI.Initialize(uEntityID, xConfig, pxSkeleton);
		m_axEnemies.push_back(std::move(xAI));
		Zenith_Log(LOG_CATEGORY_ANIMATION, "[EnemyManager] After push_back, vector size=%zu", m_axEnemies.size());
	}

	/**
	 * Update - Update all enemies
	 */
	void Update(float fDt)
	{
		static float s_fLogTimer = 0.0f;
		s_fLogTimer += fDt;
		bool bLogThisFrame = s_fLogTimer > 2.0f;
		if (bLogThisFrame)
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[EnemyManager] Updating %zu enemies", m_axEnemies.size());
			s_fLogTimer = 0.0f;
		}

		for (Combat_EnemyAI& xEnemy : m_axEnemies)
		{
			if (bLogThisFrame)
			{
				Zenith_Log(LOG_CATEGORY_ANIMATION, "[EnemyManager] Enemy %u state=%d", xEnemy.GetEntityID().m_uIndex, (int)xEnemy.GetState());
			}
			xEnemy.Update(fDt);
		}

		// Process deferred hit stuns AFTER update loop completes
		// This avoids nested iteration over m_axEnemies which can cause crashes
		ProcessDeferredHitStuns();
	}

	/**
	 * TriggerHitStunForEntity - Queue enemy for hit stun (deferred processing)
	 * Note: This is called during damage events which may occur during Update iteration.
	 * We defer processing to avoid nested iteration over m_axEnemies.
	 */
	void TriggerHitStunForEntity(Zenith_EntityID uEntityID)
	{
		m_axDeferredHitStuns.push_back(uEntityID);
	}

	/**
	 * GetAliveCount - Count living enemies
	 */
	uint32_t GetAliveCount() const
	{
		uint32_t uCount = 0;
		for (const Combat_EnemyAI& xEnemy : m_axEnemies)
		{
			if (xEnemy.IsAlive())
				uCount++;
		}
		return uCount;
	}

	/**
	 * Reset - Clear all enemies
	 */
	void Reset()
	{
		m_axEnemies.clear();
		m_axDeferredHitStuns.clear();
	}

	/**
	 * GetEnemies - Get reference to enemy list
	 */
	std::vector<Combat_EnemyAI>& GetEnemies() { return m_axEnemies; }
	const std::vector<Combat_EnemyAI>& GetEnemies() const { return m_axEnemies; }

private:
	/**
	 * ProcessDeferredHitStuns - Apply queued hit stuns after Update completes
	 */
	void ProcessDeferredHitStuns()
	{
		for (Zenith_EntityID uEntityID : m_axDeferredHitStuns)
		{
			for (Combat_EnemyAI& xEnemy : m_axEnemies)
			{
				if (xEnemy.GetEntityID() == uEntityID)
				{
					xEnemy.TriggerHitStun();
					break;
				}
			}
		}
		m_axDeferredHitStuns.clear();
	}

	std::vector<Combat_EnemyAI> m_axEnemies;
	std::vector<Zenith_EntityID> m_axDeferredHitStuns;
};
