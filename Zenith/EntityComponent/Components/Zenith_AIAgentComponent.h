#pragma once

#include "ZenithECS/Zenith_Entity.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#endif

class Zenith_DataStream;
class Zenith_NavMeshAgent;

/**
 * Zenith_AIAgentComponent - AI host component for entities
 *
 * Provides an entity's AI-system integration seams:
 * - Perception registration (via PerceptionSystem, in OnAwake/OnDestroy)
 * - Navigation: a NavMeshAgent, ticked every frame in OnUpdate while enabled.
 *   It may be BORROWED (SetNavMeshAgent, the historical path -- game code owns
 *   the lifetime) or OWNED (EnsureOwnedNavMeshAgent, the opt-in path the
 *   EnsureNavAgent graph node uses). Nothing auto-creates one.
 * - An enable flag that gates the per-frame nav tick (game logic / behaviour
 *   graphs park an agent by disabling it)
 *
 * Decision-making lives in behaviour graphs (Zenith_GraphComponent) now; the
 * former in-component behaviour tree + blackboard were removed together with the
 * Zenith/AI/BehaviorTree module.
 *
 * Usage:
 *   auto& xAI = xEntity.AddComponent<Zenith_AIAgentComponent>();
 *   xAI.SetNavMeshAgent(&xMyNavAgent);        // borrow: caller owns it
 *   Zenith_NavMeshAgent* p = xAI.EnsureOwnedNavMeshAgent();  // or: we own it
 */
class Zenith_AIAgentComponent
{
public:
	Zenith_AIAgentComponent() = delete;
	explicit Zenith_AIAgentComponent(Zenith_Entity& xParentEntity);
	~Zenith_AIAgentComponent();

	// Prevent copying
	Zenith_AIAgentComponent(const Zenith_AIAgentComponent&) = delete;
	Zenith_AIAgentComponent& operator=(const Zenith_AIAgentComponent&) = delete;

	// Allow moving (for component pool swap-and-pop)
	Zenith_AIAgentComponent(Zenith_AIAgentComponent&& xOther) noexcept;
	Zenith_AIAgentComponent& operator=(Zenith_AIAgentComponent&& xOther) noexcept;

	// ========== Lifecycle ==========

	void OnAwake();
	void OnUpdate(float fDt);
	void OnDestroy();

	// ========== Navigation ==========
	//
	// TWO POINTERS, not a pointer plus an ownership bool. The bool form makes
	// the self-assignment and borrow-vs-own cases easy to get subtly wrong, and
	// gives you nothing to assert; this shape has one checkable invariant:
	//
	//   m_pxOwnedNavMeshAgent != nullptr  =>  m_pxOwnedNavMeshAgent == m_pxNavMeshAgent
	//
	// Neither pointer is SERIALIZED. They are runtime-only, so this adds no
	// .zscen format change and no cross-game scene republish.

	// Installs a BORROWED agent: this component never frees it, and the caller
	// must null the borrow before destroying what it points at.
	//
	// ★ p == the currently-installed pointer is a NO-OP, and that is the case
	// that bites: freeing the owned agent first would destroy the very object
	// being installed. SetNavMeshAgent(nullptr) clears both pointers (freeing an
	// owned agent). Any other value frees an owned agent and installs the borrow.
	void SetNavMeshAgent(Zenith_NavMeshAgent* pxAgent);

	// Returns an agent, allocating one this component OWNS if there is none.
	//
	// ★ AN EXISTING BORROW WINS AND IS RETURNED UNTOUCHED. A game's explicit
	// SetNavMeshAgent is a deliberate decision; an auto-wire (the EnsureNavAgent
	// graph node) must never quietly replace it. Idempotent.
	Zenith_NavMeshAgent* EnsureOwnedNavMeshAgent();

	Zenith_NavMeshAgent* GetNavMeshAgent() const { return m_pxNavMeshAgent; }

	// True when the ACTIVE agent is one this component allocated. Exists for the
	// ownership units and the editor panel; game code should not need it.
	bool OwnsNavMeshAgent() const { return m_pxOwnedNavMeshAgent != nullptr; }

	// ========== Entity Access ==========

	Zenith_Entity GetEntity() const { return m_xParentEntity; }

	// ========== Configuration ==========

	void SetEnabled(bool b) { m_bEnabled = b; }
	bool IsEnabled() const { return m_bEnabled; }

	// Tick-interval tuning knob (retained across the BehaviorTree teardown; it no
	// longer gates any in-component tick — the nav agent updates every frame).
	void SetUpdateInterval(float f) { m_fUpdateInterval = f; }
	float GetUpdateInterval() const { return m_fUpdateInterval; }

	// ========== Serialization ==========

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// ========== Editor UI ==========

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	// Frees m_pxOwnedNavMeshAgent and nulls both pointers. The one place the
	// owned agent is deleted, so the destructor, SetNavMeshAgent and both move
	// operations cannot disagree about it.
	void ReleaseOwnedNavMeshAgent();

	Zenith_Entity m_xParentEntity;

	// ACTIVE: borrowed or owned. Every consumer reads this one.
	Zenith_NavMeshAgent* m_pxNavMeshAgent = nullptr;
	// Non-null ONLY when we allocated it, in which case it equals the pointer
	// above. See the invariant in the Navigation section.
	Zenith_NavMeshAgent* m_pxOwnedNavMeshAgent = nullptr;

	float m_fUpdateInterval = 0.1f;

	bool m_bEnabled = true;
};
