#pragma once

#include "EntityComponent/Zenith_Entity.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

class Zenith_DataStream;
class Zenith_BehaviorTree;
class Zenith_NavMeshAgent;

/**
 * Zenith_AIAgentComponent - Main AI component for entities
 *
 * This component provides AI capabilities to an entity:
 * - Behavior tree execution for decision-making
 * - Blackboard for state sharing between nodes
 * - Navigation integration (via NavMeshAgent, set externally)
 * - Perception integration (via PerceptionSystem)
 *
 * Usage:
 *   auto& xAI = xEntity.AddComponent<Zenith_AIAgentComponent>(xEntity);
 *   xAI.SetBehaviorTree(pxPatrolTree);
 *   xAI.GetBlackboard().SetFloat("PatrolRadius", 10.0f);
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
	void OnStart();
	void OnUpdate(float fDt);
	void OnDestroy();

	// ========== Behavior Tree ==========

	void SetBehaviorTree(Zenith_BehaviorTree* pxTree);
	Zenith_BehaviorTree* GetBehaviorTree() const { return m_pxBehaviorTree; }

	// ========== Blackboard ==========

	Zenith_Blackboard& GetBlackboard() { return m_xBlackboard; }
	const Zenith_Blackboard& GetBlackboard() const { return m_xBlackboard; }

	// ========== Navigation ==========

	void SetNavMeshAgent(Zenith_NavMeshAgent* pxAgent) { m_pxNavMeshAgent = pxAgent; }
	Zenith_NavMeshAgent* GetNavMeshAgent() const { return m_pxNavMeshAgent; }

	// ========== Entity Access ==========

	Zenith_Entity GetEntity() const { return m_xParentEntity; }

	// ========== Configuration ==========

	void SetEnabled(bool b) { m_bEnabled = b; }
	bool IsEnabled() const { return m_bEnabled; }

	void SetUpdateInterval(float f) { m_fBehaviorUpdateInterval = f; }
	float GetUpdateInterval() const { return m_fBehaviorUpdateInterval; }

	// ========== Debug ==========

	const char* GetCurrentNodeName() const { return m_szCurrentNodeName; }

	// ========== Serialization ==========

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// ========== Editor UI ==========

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	void TickBehaviorTree(float fDt);

	Zenith_Entity m_xParentEntity;
	Zenith_Blackboard m_xBlackboard;
	Zenith_BehaviorTree* m_pxBehaviorTree = nullptr;
	Zenith_NavMeshAgent* m_pxNavMeshAgent = nullptr;

	// Behavior tree tick rate control
	float m_fBehaviorUpdateInterval = 0.1f;  // Default 10 Hz
	float m_fTimeSinceLastUpdate = 0.0f;

	bool m_bEnabled = true;

	// Debug info
	const char* m_szCurrentNodeName = "";

	// Asset path for serialization
	std::string m_strBehaviorTreeAsset;
};
