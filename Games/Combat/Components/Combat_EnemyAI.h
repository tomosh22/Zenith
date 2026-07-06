#pragma once
#include "Core/Zenith_Engine.h"
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

#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Maths/Zenith_Maths.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
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
	float m_fAttackRange = 1.5f;  // Accounts for physics collision settling distance
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

	void Initialize(Zenith_EntityID uEntityID, const Combat_EnemyConfig& xConfig, Zenith_AnimatorComponent* pxAnimator = nullptr)
	{
		m_uEntityID = uEntityID;
		m_xConfig = xConfig;
		m_eState = Combat_EnemyState::IDLE;

		// Initialize subsystems
		m_xHitDetection.SetOwner(uEntityID);
		if (pxAnimator)
		{
			m_xAnimController.Initialize(*pxAnimator);
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[Enemy %u] Animation controller initialized with AnimatorComponent", uEntityID.m_uIndex);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_ANIMATION, "[Enemy %u] WARNING: No AnimatorComponent provided, animation will not work!", uEntityID.m_uIndex);
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

	// ========================================================================
	// Graph tick (the old Update decision-switch now lives in the
	// Combat_EnemyBrain.bgraph StateMachine; these are its leaf shims). The
	// per-frame order is preserved: PreTick -> per-state handler -> PostTick,
	// all run synchronously in one EnemyBrainTick dispatch.
	// ========================================================================

	// Pre-switch work: resolve + guards + EnforceUpright + death-check +
	// cooldown decrement. Caches the entity/transform for the state handlers +
	// PostTick this tick. Returns false if the entity/transform are gone (the
	// old early-returns), so the graph aborts the rest of the tick.
	bool GraphPreTick(float fDt)
	{
		// Clear the cached transform up-front so PostTick's null-guard reproduces
		// the old Update's full early-return on the entity-invalid path (the old
		// body ran neither the state handler nor anim/IK when the entity was gone;
		// PostTick fires from a separate EnemyBrainTick source, so without this it
		// would dereference a stale/freed transform from a prior tick).
		m_pxTickTransform = nullptr;

		m_xTickEntity = g_xEngine.Scenes().ResolveEntity(m_uEntityID);
		if (!m_xTickEntity.IsValid())
			return false;

		m_pxTickTransform = m_xTickEntity.TryGetComponent<Zenith_TransformComponent>();
		if (m_pxTickTransform == nullptr)
			return false;

		// Enforce upright orientation every frame (collision impulses can still tip characters)
		if (Zenith_ColliderComponent* pxCollider = m_xTickEntity.TryGetComponent<Zenith_ColliderComponent>())
		{
			Zenith_ColliderComponent& xCollider = *pxCollider;
			if (xCollider.HasValidBody())
			{
				g_xEngine.Physics().EnforceUpright(xCollider.GetBodyID());
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

		return true;
	}

	// Per-state handlers (dispatched by the graph StateMachine on enemyState).
	void GraphIdleTick()           { UpdateIdleState(*m_pxTickTransform); }
	void GraphChaseTick(float fDt)  { UpdateChaseState(m_xTickEntity, *m_pxTickTransform, fDt); }
	void GraphAttackTick(float fDt) { UpdateAttackState(*m_pxTickTransform, fDt); }
	void GraphHitStunTick(float fDt){ UpdateHitStunState(fDt); }

	// Post-switch work: animation + IK from the post-dispatch state.
	void GraphPostTick(float fDt)
	{
		if (m_pxTickTransform == nullptr)
			return;

		// Update animation
		bool bIsAttacking = (m_eState == Combat_EnemyState::ATTACKING);
		bool bIsHit = (m_eState == Combat_EnemyState::HIT_STUN);
		bool bIsDead = (m_eState == Combat_EnemyState::DEAD);
		m_xAnimController.UpdateForEnemy(m_fCurrentSpeed, bIsAttacking, bIsHit, bIsDead);

		// Update IK
		bool bCanUseIK = (m_eState != Combat_EnemyState::DEAD && m_eState != Combat_EnemyState::HIT_STUN);
		m_xIKController.UpdateWithAutoTarget(*m_pxTickTransform, m_uEntityID, 0.0f, bCanUseIK, fDt);
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

	void UpdateIdleState(Zenith_TransformComponent& xTransform)
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
		bool bInRange = fDist <= m_xConfig.m_fAttackRange;
		bool bCooldownReady = m_fAttackCooldownTimer <= 0.0f;

		if (bInRange && bCooldownReady)
		{
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
				if (Zenith_ColliderComponent* pxCollider = xEntity.TryGetComponent<Zenith_ColliderComponent>())
				{
					Zenith_ColliderComponent& xCollider = *pxCollider;
					if (xCollider.HasValidBody())
					{
						Zenith_Maths::Vector3 xVelocity = xDirection * m_xConfig.m_fMoveSpeed;
						xVelocity.y = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID()).y;
						g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
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
			if (Zenith_ColliderComponent* pxCollider = xEntity.TryGetComponent<Zenith_ColliderComponent>())
			{
				Zenith_ColliderComponent& xCollider = *pxCollider;
				if (xCollider.HasValidBody())
				{
					Zenith_Maths::Vector3 xVelocity = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID());
					xVelocity.x = 0.0f;
					xVelocity.z = 0.0f;
					g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
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

		// Ensure current rotation is normalized (can drift due to physics)
		xCurrentRot = glm::normalize(xCurrentRot);

		float fTargetYaw = atan2(xTargetDir.x, xTargetDir.z);
		Zenith_Maths::Quat xTargetRot = glm::angleAxis(fTargetYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

		Zenith_Maths::Quat xNewRot = glm::slerp(xCurrentRot, xTargetRot, fDt * m_xConfig.m_fRotationSpeed);

		// Normalize to prevent drift accumulation and Jolt assertions
		xNewRot = glm::normalize(xNewRot);
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

	// Resolved once per tick by GraphPreTick; consumed by the state handlers +
	// GraphPostTick this tick (valid only within one EnemyBrainTick dispatch).
	Zenith_Entity m_xTickEntity;
	Zenith_TransformComponent* m_pxTickTransform = nullptr;

	Combat_HitDetection m_xHitDetection;
	Combat_AnimationController m_xAnimController;
	Combat_IKController m_xIKController;
};

// (Combat_EnemyManager removed - it was dead code, a legacy central manager
//  that iterated a std::vector<Combat_EnemyAI> calling Update(). The live
//  architecture is per-entity Combat_EnemyComponent, and Combat_EnemyAI's
//  decision switch now lives in the Combat_EnemyBrain graph, so the manager's
//  only remaining call site - xEnemy.Update(fDt) - no longer exists.)
