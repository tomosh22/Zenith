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
 * - Navigation: a non-owning NavMeshAgent borrow, ticked every frame in
 *   OnUpdate while the agent is enabled
 * - An enable flag that gates the per-frame nav tick (game logic / behaviour
 *   graphs park an agent by disabling it)
 *
 * Decision-making lives in behaviour graphs (Zenith_GraphComponent) now; the
 * former in-component behaviour tree + blackboard were removed together with the
 * Zenith/AI/BehaviorTree module.
 *
 * Usage:
 *   auto& xAI = xEntity.AddComponent<Zenith_AIAgentComponent>();
 *   xAI.SetNavMeshAgent(&xMyNavAgent);
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

	// The NavMeshAgent pointer is non-owning. Lifetime is managed externally
	// (typically by the navigation system or a parent component that creates
	// agents alongside the navmesh). Setting/clearing this pointer never
	// allocates or frees the underlying agent — callers are responsible for
	// nulling it before the agent it points to is destroyed.
	void SetNavMeshAgent(Zenith_NavMeshAgent* pxAgent) { m_pxNavMeshAgent = pxAgent; }
	Zenith_NavMeshAgent* GetNavMeshAgent() const { return m_pxNavMeshAgent; }

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
	Zenith_Entity m_xParentEntity;
	// Non-owning. See SetNavMeshAgent() for ownership rules.
	Zenith_NavMeshAgent* m_pxNavMeshAgent = nullptr;

	float m_fUpdateInterval = 0.1f;

	bool m_bEnabled = true;
};
