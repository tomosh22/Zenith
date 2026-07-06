#pragma once
/**
 * Combat_PlayerComponent.h - Per-entity game component attached to the player.
 *
 * Unity-style: this component lives on the player entity rather than on the
 * central GameManager. It owns the player's controller, animation, IK, and hit
 * detection state. The central Combat_GameComponent (GameManager) only
 * orchestrates game state and routes damage events; the player ticks itself.
 *
 * Lifecycle (hooks concept-detected by the component-meta registry):
 *   - OnAwake:  register self with Combat_GameComponent::RegisterPlayer; set
 *               hit-detection owner. NOTE: the spawner invokes OnAwake directly
 *               after AddComponent because the prefab-instantiated entity is
 *               already awoken by then (see CreateArena).
 *   - OnStart:  initialize the animation controller from the entity's
 *               AnimatorComponent (deferred to OnStart because the animator's
 *               skeleton is auto-discovered from the ModelComponent at OnStart
 *               time)
 *   - OnUpdate: per-frame movement / animation / IK / attack work, gated on the
 *               GameManager being in the PLAYING state
 *   - OnDestroy: unregister
 *
 * Implementation lives in Combat_PlayerComponent.cpp so the bodies can reference
 * Combat_GameComponent's static state directly without the cyclic include that
 * would otherwise arise (Combat_GameComponent.h already includes this header to
 * attach the component in CreateArena).
 */

#include "ZenithECS/Zenith_Scene.h"
#include "DataStream/Zenith_DataStream.h"

#include "Combat_PlayerController.h"
#include "Combat_AnimationController.h"
#include "Combat_IKController.h"
#include "Combat_HitDetection.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

class Zenith_TransformComponent;

class Combat_PlayerComponent
{
public:
	Combat_PlayerComponent() = delete;
	Combat_PlayerComponent(Zenith_Entity& xEntity)
		: m_xParentEntity(xEntity)
	{
	}

	// No user-declared destructor / copy / move: component pools relocate
	// instances on resize, so the class relies on the implicitly-generated moves.

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
		ImGui::Text("Combat Player");
		ImGui::Text("State: %d", static_cast<int>(m_xController.GetState()));
		ImGui::Text("Combo: %u", m_xController.GetComboCount());
	}
#endif

	// Called when the damage target is the player.
	void TriggerHitStun(float fDuration = 0.3f);

	uint32_t GetComboCount() const { return m_xController.GetComboCount(); }
	const Combat_PlayerController& GetController() const { return m_xController; }
	const Combat_AnimationController& GetAnimController() const { return m_xAnimController; }

	// Wave-2 graph conversion: the attack flow DECISIONS live in the
	// boot-authored Combat_PlayerAttack.bgraph (CombatNode_AttackFlow on the
	// "AttackTick" custom event, fired at the end of OnUpdate - exactly where
	// the old UpdateAttack ran). These expose the systems the node executes
	// through.
	Combat_HitDetection& HitDetection() { return m_xHitDetection; }
	const Combat_AnimationController& AnimController() const { return m_xAnimController; }

	// Combat_PlayerState.bgraph tick shims: the graph's StateMachine drives these
	// (the old Combat_PlayerController::Update decision switch is gone). PreTick
	// resolves the transform/collider and returns false if either is missing, so
	// the graph aborts the tick; m_eState stays the C++ source of truth
	// (GetState()/GetComboCount() are unchanged), mirrored to the blackboard only
	// to drive the StateMachine dispatch.
	bool Graph_PreTick(float fDt); // defined in .cpp (needs Transform/Collider)
	void Graph_MovementTick(float fDt) { m_xController.GraphMovementTick(fDt); }
	void Graph_AttackTick(float fDt)   { m_xController.GraphAttackTick(fDt); }
	void Graph_DodgeTick(float fDt)    { m_xController.GraphDodgeTick(fDt); }
	void Graph_HitStunTick(float fDt)  { m_xController.GraphHitStunTick(fDt); }
	void Graph_PostTick()              { m_xController.GraphPostTick(); }
	int  Graph_GetPlayerStateInt() const { return static_cast<int>(m_xController.GetState()); }

private:
	void FireAttackTick();
	void FirePlayerTick(float fDt);

	Zenith_Entity              m_xParentEntity;
	Combat_PlayerController    m_xController;
	Combat_AnimationController m_xAnimController;
	Combat_IKController        m_xIKController;
	Combat_HitDetection        m_xHitDetection;
};
