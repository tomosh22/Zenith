#pragma once
/**
 * Combat_EnemyComponent.h - Per-entity game component attached to each enemy.
 *
 * Unity-style: each enemy has its own component instance, rather than a central
 * manager iterating an enemy list. The Combat_GameComponent GameManager keeps a
 * registry of all enemy entity IDs so it can compute alive count for win
 * conditions and route damage events to the right enemy.
 *
 * Lifecycle (hooks concept-detected by the component-meta registry):
 *   - OnAwake:  register with Combat_GameComponent::RegisterEnemy. NOTE: the
 *               spawner invokes OnAwake directly after AddComponent because the
 *               prefab-instantiated entity is already awoken by then (see
 *               SpawnEnemies).
 *   - OnStart:  initialize the underlying Combat_EnemyAI with the entity's
 *               animator (the AnimatorComponent's skeleton is auto-discovered
 *               during OnStart)
 *   - OnUpdate: tick the AI, gated on the GameManager being in the PLAYING state
 *   - OnDestroy: unregister (also covers timed corpse destruction, Destroy(3.0f))
 *
 * The configuration (move speed, attack range, etc.) defaults to the values that
 * the old central spawner used. Callers can override via SetConfig() before the
 * first update if needed.
 *
 * Implementation lives in Combat_EnemyComponent.cpp so the bodies can reference
 * Combat_GameComponent's static state directly.
 */

#include "ZenithECS/Zenith_Scene.h"
#include "DataStream/Zenith_DataStream.h"

#include "Combat_EnemyAI.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

class Combat_EnemyComponent
{
public:
	Combat_EnemyComponent() = delete;
	Combat_EnemyComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
		, m_bConfigured(false)
		, m_bInitialized(false)
	{
		// Defaults match the old centralised spawner.
		m_xConfig.m_fMoveSpeed = 3.0f;
		m_xConfig.m_fAttackDamage = 15.0f;
		m_xConfig.m_fAttackRange = 1.5f;
		m_xConfig.m_fDetectionRange = 8.0f;
	}

	// No user-declared destructor / copy / move: component pools relocate
	// instances on resize, so the class relies on the implicitly-generated moves.

	// Spawner can override before the first update (otherwise defaults are used).
	void SetConfig(const Combat_EnemyConfig& xConfig)
	{
		m_xConfig = xConfig;
		m_bConfigured = true;
	}

	void OnAwake();
	void OnStart();
	void OnDestroy();
	void OnUpdate(float fDt);

	// Component contract. All state is runtime-only (rebuilt on spawn); nothing
	// needs to persist beyond the version tag.
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		uint32_t uVersion = 0;
		xStream >> uVersion;
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Combat Enemy");
		ImGui::Text("State: %d", static_cast<int>(m_xAI.GetState()));
		ImGui::Text("Alive: %s", m_xAI.IsAlive() ? "yes" : "no");
	}
#endif

	// Called when this enemy is the damage target.
	void TriggerHitStun();

	bool IsAlive() const { return m_xAI.IsAlive(); }
	const Combat_EnemyAI& GetAI() const { return m_xAI; }

	// Graph-facing tick shims: the Combat_EnemyBrain graph's StateMachine drives
	// these (the old C++ decision switch that dispatched them is gone). PreTick
	// returns false when the entity/transform are gone so the graph aborts the
	// tick; the per-state handlers run the matching old switch case verbatim.
	bool Graph_PreTick(float fDt)    { EnsureInitialized(); return m_xAI.GraphPreTick(fDt); }
	void Graph_IdleTick()            { m_xAI.GraphIdleTick(); }
	void Graph_ChaseTick(float fDt)  { m_xAI.GraphChaseTick(fDt); }
	void Graph_AttackTick(float fDt) { m_xAI.GraphAttackTick(fDt); }
	void Graph_HitStunTick(float fDt){ m_xAI.GraphHitStunTick(fDt); }
	void Graph_PostTick(float fDt)   { m_xAI.GraphPostTick(fDt); }
	int  Graph_GetStateInt() const   { return static_cast<int>(m_xAI.GetState()); }

private:
	void EnsureInitialized();
	void FireEnemyBrainTick(float fDt);

	Zenith_Entity      m_xParentEntity;
	Combat_EnemyAI     m_xAI;
	Combat_EnemyConfig m_xConfig;
	bool               m_bConfigured;
	bool               m_bInitialized;
};
