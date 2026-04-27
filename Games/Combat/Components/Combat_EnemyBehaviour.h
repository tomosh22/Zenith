#pragma once
/**
 * Combat_EnemyBehaviour.h - Per-entity script attached to each enemy.
 *
 * Unity-style: each enemy has its own script instance, rather than a central manager
 * iterating an enemy list. The Combat_Behaviour GameManager keeps a registry of all
 * Combat_EnemyBehaviour instances so it can compute alive count for win conditions
 * and route damage events to the right enemy.
 *
 * Lifecycle:
 *   - OnAwake:  register with Combat_Behaviour::RegisterEnemy
 *   - OnStart:  initialize the underlying Combat_EnemyAI with the entity's animator
 *               (the AnimatorComponent's skeleton is auto-discovered during OnStart)
 *   - OnUpdate: tick the AI, gated on the GameManager being in the PLAYING state
 *   - OnDestroy: unregister (also covers SceneManager::Destroy(corpse, 3.0f))
 *
 * The configuration (move speed, attack range, etc.) defaults to the values that the
 * old central spawner used. Callers can override via SetConfig() before the first
 * update if needed.
 *
 * Implementation lives in Combat_EnemyBehaviour.cpp so the bodies can reference
 * Combat_Behaviour's static state directly.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#include "Combat_EnemyAI.h"

class Combat_EnemyBehaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(Combat_EnemyBehaviour)

	Combat_EnemyBehaviour() = delete;
	Combat_EnemyBehaviour(Zenith_Entity&)
		: m_bConfigured(false)
		, m_bInitialized(false)
	{
		// Defaults match the old centralised spawner.
		m_xConfig.m_fMoveSpeed = 3.0f;
		m_xConfig.m_fAttackDamage = 15.0f;
		m_xConfig.m_fAttackRange = 1.5f;
		m_xConfig.m_fDetectionRange = 8.0f;
	}

	~Combat_EnemyBehaviour() = default;

	// Spawner can override before the first update (otherwise defaults are used).
	void SetConfig(const Combat_EnemyConfig& xConfig)
	{
		m_xConfig = xConfig;
		m_bConfigured = true;
	}

	void OnAwake() ZENITH_FINAL override;
	void OnStart() ZENITH_FINAL override;
	void OnDestroy() ZENITH_FINAL override;
	void OnUpdate(float fDt) ZENITH_FINAL override;

	// Called by Combat_Behaviour::OnDamageEvent when this enemy is the damage target.
	void TriggerHitStun();

	bool IsAlive() const { return m_xAI.IsAlive(); }
	const Combat_EnemyAI& GetAI() const { return m_xAI; }

private:
	void EnsureInitialized();

	Combat_EnemyAI    m_xAI;
	Combat_EnemyConfig m_xConfig;
	bool              m_bConfigured;
	bool              m_bInitialized;
};
