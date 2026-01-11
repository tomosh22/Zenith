#pragma once
/**
 * Combat_DamageSystem.h - Event-based damage and death system
 *
 * Demonstrates:
 * - Custom event structs for damage/death
 * - Zenith_EventDispatcher subscription and dispatch
 * - Health tracking per entity
 * - Knockback application via physics
 *
 * Events:
 * - Combat_DamageEvent: Dispatched when an attack lands
 * - Combat_DeathEvent: Dispatched when an entity's health reaches 0
 * - Combat_HitEvent: Visual feedback for hit registration
 */

#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Maths/Zenith_Maths.h"
#include <unordered_map>

// ============================================================================
// Custom Combat Events
// ============================================================================

/**
 * Combat_DamageEvent - Dispatched when damage is dealt
 */
struct Combat_DamageEvent
{
	Zenith_EntityID m_uTargetEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID m_uAttackerEntityID = INVALID_ENTITY_ID;
	float m_fDamage = 0.0f;
	Zenith_Maths::Vector3 m_xHitPoint = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xHitDirection = Zenith_Maths::Vector3(0.0f);
	bool m_bIsComboHit = false;
	uint32_t m_uComboCount = 0;
};

/**
 * Combat_DeathEvent - Dispatched when an entity dies
 */
struct Combat_DeathEvent
{
	Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID m_uKillerEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 m_xDeathPosition = Zenith_Maths::Vector3(0.0f);
};

/**
 * Combat_HitEvent - Dispatched for visual/audio feedback
 */
struct Combat_HitEvent
{
	Zenith_Maths::Vector3 m_xHitPosition = Zenith_Maths::Vector3(0.0f);
	float m_fHitStrength = 1.0f;  // For particle/sound intensity
	bool m_bIsCritical = false;
};

// ============================================================================
// Health Component Data
// ============================================================================

struct Combat_HealthData
{
	float m_fMaxHealth = 100.0f;
	float m_fCurrentHealth = 100.0f;
	float m_fKnockbackResistance = 0.0f;  // 0-1, reduces knockback
	bool m_bIsInvulnerable = false;
	float m_fInvulnerabilityTimer = 0.0f;
	float m_fInvulnerabilityDuration = 0.2f;  // Brief immunity after hit
	bool m_bIsDead = false;
};

// ============================================================================
// Combat Damage System
// ============================================================================

/**
 * Combat_DamageSystem - Manages health and damage for all combat entities
 *
 * Usage:
 *   // Initialize system
 *   Combat_DamageSystem::Initialize();
 *
 *   // Register entity health
 *   Combat_DamageSystem::RegisterEntity(uEntityID, 100.0f);
 *
 *   // Deal damage (via event)
 *   Zenith_EventDispatcher::Get().Dispatch(Combat_DamageEvent{...});
 *
 *   // Check health
 *   float fHealth = Combat_DamageSystem::GetHealth(uEntityID);
 */
class Combat_DamageSystem
{
public:
	// ========================================================================
	// Initialization
	// ========================================================================

	/**
	 * Initialize - Set up event listeners
	 * Call once at game startup.
	 * Safe to call multiple times - resets health data for new play sessions.
	 */
	static void Initialize()
	{
		if (s_bInitialized)
		{
			// Already initialized, but reset health data for new play session.
			// This clears stale entity IDs from previous sessions while keeping
			// the event subscription active.
			Reset();
			return;
		}

		// Subscribe to damage events
		s_uDamageEventHandle = Zenith_EventDispatcher::Get().SubscribeLambda<Combat_DamageEvent>(
			[](const Combat_DamageEvent& xEvent)
			{
				HandleDamageEvent(xEvent);
			});

		s_bInitialized = true;
	}

	/**
	 * Shutdown - Clean up event listeners
	 */
	static void Shutdown()
	{
		if (!s_bInitialized)
			return;

		Zenith_EventDispatcher::Get().Unsubscribe(s_uDamageEventHandle);
		s_xHealthData.clear();
		s_bInitialized = false;
	}

	/**
	 * Reset - Clear all health data (for new round)
	 */
	static void Reset()
	{
		s_xHealthData.clear();
	}

	// ========================================================================
	// Entity Registration
	// ========================================================================

	/**
	 * RegisterEntity - Add an entity to the damage system
	 */
	static void RegisterEntity(Zenith_EntityID uEntityID, float fMaxHealth, float fKnockbackResistance = 0.0f)
	{
		Combat_HealthData xData;
		xData.m_fMaxHealth = fMaxHealth;
		xData.m_fCurrentHealth = fMaxHealth;
		xData.m_fKnockbackResistance = fKnockbackResistance;
		xData.m_bIsInvulnerable = false;
		xData.m_bIsDead = false;
		s_xHealthData[uEntityID] = xData;
	}

	/**
	 * UnregisterEntity - Remove an entity from the damage system
	 */
	static void UnregisterEntity(Zenith_EntityID uEntityID)
	{
		s_xHealthData.erase(uEntityID);
	}

	// ========================================================================
	// Health Queries
	// ========================================================================

	static float GetHealth(Zenith_EntityID uEntityID)
	{
		auto xIt = s_xHealthData.find(uEntityID);
		if (xIt == s_xHealthData.end())
			return 0.0f;
		return xIt->second.m_fCurrentHealth;
	}

	static float GetMaxHealth(Zenith_EntityID uEntityID)
	{
		auto xIt = s_xHealthData.find(uEntityID);
		if (xIt == s_xHealthData.end())
			return 0.0f;
		return xIt->second.m_fMaxHealth;
	}

	static float GetHealthPercent(Zenith_EntityID uEntityID)
	{
		auto xIt = s_xHealthData.find(uEntityID);
		if (xIt == s_xHealthData.end())
			return 0.0f;
		if (xIt->second.m_fMaxHealth <= 0.0f)
			return 0.0f;
		return xIt->second.m_fCurrentHealth / xIt->second.m_fMaxHealth;
	}

	static bool IsDead(Zenith_EntityID uEntityID)
	{
		auto xIt = s_xHealthData.find(uEntityID);
		if (xIt == s_xHealthData.end())
			return true;  // Unknown entity treated as dead
		return xIt->second.m_bIsDead;
	}

	static bool IsAlive(Zenith_EntityID uEntityID)
	{
		return !IsDead(uEntityID);
	}

	static bool HasEntity(Zenith_EntityID uEntityID)
	{
		return s_xHealthData.find(uEntityID) != s_xHealthData.end();
	}

	// ========================================================================
	// Health Modification
	// ========================================================================

	/**
	 * Heal - Restore health to an entity
	 */
	static void Heal(Zenith_EntityID uEntityID, float fAmount)
	{
		auto xIt = s_xHealthData.find(uEntityID);
		if (xIt == s_xHealthData.end() || xIt->second.m_bIsDead)
			return;

		xIt->second.m_fCurrentHealth = std::min(
			xIt->second.m_fCurrentHealth + fAmount,
			xIt->second.m_fMaxHealth);
	}

	/**
	 * SetInvulnerable - Toggle invulnerability
	 */
	static void SetInvulnerable(Zenith_EntityID uEntityID, bool bInvulnerable)
	{
		auto xIt = s_xHealthData.find(uEntityID);
		if (xIt != s_xHealthData.end())
		{
			xIt->second.m_bIsInvulnerable = bInvulnerable;
		}
	}

	// ========================================================================
	// Update (for invulnerability timers)
	// ========================================================================

	static void Update(float fDt)
	{
		for (auto& [uEntityID, xData] : s_xHealthData)
		{
			if (xData.m_fInvulnerabilityTimer > 0.0f)
			{
				xData.m_fInvulnerabilityTimer -= fDt;
				if (xData.m_fInvulnerabilityTimer <= 0.0f)
				{
					xData.m_bIsInvulnerable = false;
				}
			}
		}
	}

	// ========================================================================
	// Direct Damage (bypasses events, for internal use)
	// ========================================================================

	/**
	 * ApplyDamage - Directly apply damage to an entity
	 * Returns actual damage dealt (may be reduced or blocked)
	 */
	static float ApplyDamage(Zenith_EntityID uTargetID, Zenith_EntityID uAttackerID,
		float fDamage, const Zenith_Maths::Vector3& xHitDirection, float fKnockbackForce = 5.0f)
	{
		auto xIt = s_xHealthData.find(uTargetID);
		if (xIt == s_xHealthData.end())
			return 0.0f;

		Combat_HealthData& xData = xIt->second;

		// Check invulnerability
		if (xData.m_bIsInvulnerable || xData.m_bIsDead)
			return 0.0f;

		// Apply damage
		float fActualDamage = fDamage;
		xData.m_fCurrentHealth -= fActualDamage;

		// Apply knockback
		ApplyKnockback(uTargetID, xHitDirection, fKnockbackForce, xData.m_fKnockbackResistance);

		// Grant brief invulnerability
		xData.m_bIsInvulnerable = true;
		xData.m_fInvulnerabilityTimer = xData.m_fInvulnerabilityDuration;

		// Check for death
		if (xData.m_fCurrentHealth <= 0.0f)
		{
			xData.m_fCurrentHealth = 0.0f;
			xData.m_bIsDead = true;

			// Get death position
			Zenith_Maths::Vector3 xDeathPos(0.0f);
			Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
			if (xScene.EntityExists(uTargetID))
			{
				Zenith_Entity xEntity = xScene.GetEntity(uTargetID);
				if (xEntity.HasComponent<Zenith_TransformComponent>())
				{
					xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xDeathPos);
				}
			}

			// Dispatch death event
			Combat_DeathEvent xDeathEvent;
			xDeathEvent.m_uEntityID = uTargetID;
			xDeathEvent.m_uKillerEntityID = uAttackerID;
			xDeathEvent.m_xDeathPosition = xDeathPos;
			Zenith_EventDispatcher::Get().Dispatch(xDeathEvent);
		}

		return fActualDamage;
	}

private:
	// ========================================================================
	// Event Handlers
	// ========================================================================

	static void HandleDamageEvent(const Combat_DamageEvent& xEvent)
	{
		// Apply damage and calculate final values
		float fFinalDamage = xEvent.m_fDamage;

		// Apply combo multiplier
		if (xEvent.m_bIsComboHit && xEvent.m_uComboCount > 1)
		{
			fFinalDamage *= (1.0f + (xEvent.m_uComboCount - 1) * 0.2f);  // 20% per combo hit
		}

		// Calculate knockback force based on damage
		float fKnockbackForce = fFinalDamage * 0.3f;

		// Apply damage
		float fActualDamage = ApplyDamage(
			xEvent.m_uTargetEntityID,
			xEvent.m_uAttackerEntityID,
			fFinalDamage,
			xEvent.m_xHitDirection,
			fKnockbackForce);

		// Dispatch hit event for visual feedback
		if (fActualDamage > 0.0f)
		{
			Combat_HitEvent xHitEvent;
			xHitEvent.m_xHitPosition = xEvent.m_xHitPoint;
			xHitEvent.m_fHitStrength = fActualDamage / 50.0f;  // Normalize for effects
			xHitEvent.m_bIsCritical = xEvent.m_bIsComboHit && xEvent.m_uComboCount >= 3;
			Zenith_EventDispatcher::Get().Dispatch(xHitEvent);
		}
	}

	/**
	 * ApplyKnockback - Push entity via physics impulse
	 */
	static void ApplyKnockback(Zenith_EntityID uEntityID, const Zenith_Maths::Vector3& xDirection,
		float fForce, float fResistance)
	{
		Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
		if (!xScene.EntityExists(uEntityID))
			return;

		Zenith_Entity xEntity = xScene.GetEntity(uEntityID);
		if (!xEntity.HasComponent<Zenith_ColliderComponent>())
			return;

		Zenith_ColliderComponent& xCollider = xEntity.GetComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody())
			return;

		// Apply knockback with resistance
		float fFinalForce = fForce * (1.0f - fResistance);
		float fLen = glm::length(xDirection);
		if (fFinalForce > 0.0f && fLen > 0.001f)
		{
			Zenith_Maths::Vector3 xImpulse = (xDirection / fLen) * fFinalForce;
			xImpulse.y = fFinalForce * 0.3f;  // Add slight upward component
			Zenith_Physics::AddImpulse(xCollider.GetBodyID(), xImpulse);
		}
	}

	// ========================================================================
	// Static Data
	// ========================================================================

	inline static std::unordered_map<Zenith_EntityID, Combat_HealthData> s_xHealthData;
	inline static Zenith_EventHandle s_uDamageEventHandle = INVALID_EVENT_HANDLE;
	inline static bool s_bInitialized = false;
};
