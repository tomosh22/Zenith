#include "Zenith.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_BehaviorTree.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

ZENITH_REGISTER_COMPONENT(Zenith_AIAgentComponent, "AIAgent")

Zenith_AIAgentComponent::Zenith_AIAgentComponent(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

Zenith_AIAgentComponent::~Zenith_AIAgentComponent()
{
	// Note: We don't own the behavior tree or nav agent, they're set externally
}

Zenith_AIAgentComponent::Zenith_AIAgentComponent(Zenith_AIAgentComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_xBlackboard(std::move(xOther.m_xBlackboard))
	, m_pxBehaviorTree(xOther.m_pxBehaviorTree)
	, m_pxNavMeshAgent(xOther.m_pxNavMeshAgent)
	, m_fBehaviorUpdateInterval(xOther.m_fBehaviorUpdateInterval)
	, m_fTimeSinceLastUpdate(xOther.m_fTimeSinceLastUpdate)
	, m_bEnabled(xOther.m_bEnabled)
	, m_szCurrentNodeName(xOther.m_szCurrentNodeName)
	, m_strBehaviorTreeAsset(std::move(xOther.m_strBehaviorTreeAsset))
{
	// Clear moved-from object's pointers to prevent dangling references
	xOther.m_pxBehaviorTree = nullptr;
	xOther.m_pxNavMeshAgent = nullptr;
	xOther.m_szCurrentNodeName = "";  // Reset to safe empty string literal
	xOther.m_bEnabled = false;  // Disable moved-from object
}

Zenith_AIAgentComponent& Zenith_AIAgentComponent::operator=(Zenith_AIAgentComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		m_xParentEntity = xOther.m_xParentEntity;
		m_xBlackboard = std::move(xOther.m_xBlackboard);
		m_pxBehaviorTree = xOther.m_pxBehaviorTree;
		m_pxNavMeshAgent = xOther.m_pxNavMeshAgent;
		m_fBehaviorUpdateInterval = xOther.m_fBehaviorUpdateInterval;
		m_fTimeSinceLastUpdate = xOther.m_fTimeSinceLastUpdate;
		m_bEnabled = xOther.m_bEnabled;
		m_szCurrentNodeName = xOther.m_szCurrentNodeName;
		m_strBehaviorTreeAsset = std::move(xOther.m_strBehaviorTreeAsset);

		// Clear moved-from object's pointers to prevent dangling references
		xOther.m_pxBehaviorTree = nullptr;
		xOther.m_pxNavMeshAgent = nullptr;
		xOther.m_szCurrentNodeName = "";  // Reset to safe empty string literal
		xOther.m_bEnabled = false;  // Disable moved-from object
	}
	return *this;
}

void Zenith_AIAgentComponent::OnAwake()
{
	// Register with perception system
	Zenith_PerceptionSystem::RegisterAgent(m_xParentEntity.GetEntityID());
}

void Zenith_AIAgentComponent::OnStart()
{
	// Load behavior tree asset if specified
	if (!m_strBehaviorTreeAsset.empty() && m_pxBehaviorTree == nullptr)
	{
		m_pxBehaviorTree = Zenith_BehaviorTree::LoadFromFile(m_strBehaviorTreeAsset);
		if (m_pxBehaviorTree == nullptr)
		{
			Zenith_Log(LOG_CATEGORY_AI, "Failed to load behavior tree asset: %s. Disabling AI agent.", m_strBehaviorTreeAsset.c_str());
			m_bEnabled = false;
		}
	}
}

void Zenith_AIAgentComponent::OnUpdate(float fDt)
{
	if (!m_bEnabled)
	{
		return;
	}

	// Update behavior tree at configured interval
	m_fTimeSinceLastUpdate += fDt;
	if (m_fTimeSinceLastUpdate >= m_fBehaviorUpdateInterval)
	{
		TickBehaviorTree(m_fTimeSinceLastUpdate);
		m_fTimeSinceLastUpdate = 0.0f;
	}

	// Update navigation (runs every frame for smooth movement)
	if (m_pxNavMeshAgent != nullptr && m_xParentEntity.IsValid())
	{
		if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
			m_pxNavMeshAgent->Update(fDt, xTransform);
		}
	}
}

void Zenith_AIAgentComponent::OnDestroy()
{
	// Unregister from perception system
	Zenith_PerceptionSystem::UnregisterAgent(m_xParentEntity.GetEntityID());

	// Abort behavior tree if running
	if (m_pxBehaviorTree != nullptr)
	{
		m_pxBehaviorTree->Abort(m_xParentEntity, m_xBlackboard);
	}
}

void Zenith_AIAgentComponent::SetBehaviorTree(Zenith_BehaviorTree* pxTree)
{
	// Abort old tree if running
	if (m_pxBehaviorTree != nullptr)
	{
		m_pxBehaviorTree->Abort(m_xParentEntity, m_xBlackboard);
	}

	m_pxBehaviorTree = pxTree;
	m_szCurrentNodeName = "";
}

void Zenith_AIAgentComponent::TickBehaviorTree(float fDt)
{
	if (m_pxBehaviorTree == nullptr)
	{
		return;
	}

	m_pxBehaviorTree->Tick(m_xParentEntity, m_xBlackboard, fDt);
	m_szCurrentNodeName = m_pxBehaviorTree->GetCurrentNodeName();
}

void Zenith_AIAgentComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write configuration
	xStream << m_bEnabled;
	xStream << m_fBehaviorUpdateInterval;

	// Write behavior tree asset path
	uint32_t uPathLen = static_cast<uint32_t>(m_strBehaviorTreeAsset.length());
	xStream << uPathLen;
	if (uPathLen > 0)
	{
		xStream.Write(m_strBehaviorTreeAsset.data(), uPathLen);
	}

	// Write blackboard state (optional, for save games)
	m_xBlackboard.WriteToDataStream(xStream);
}

void Zenith_AIAgentComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read configuration
	xStream >> m_bEnabled;
	xStream >> m_fBehaviorUpdateInterval;

	// Read behavior tree asset path
	uint32_t uPathLen = 0;
	xStream >> uPathLen;
	if (uPathLen > 0)
	{
		m_strBehaviorTreeAsset.resize(uPathLen);
		xStream.Read(m_strBehaviorTreeAsset.data(), uPathLen);
	}
	else
	{
		m_strBehaviorTreeAsset.clear();
	}

	// Read blackboard state
	m_xBlackboard.ReadFromDataStream(xStream);
}

#ifdef ZENITH_TOOLS
void Zenith_AIAgentComponent::RenderPropertiesPanel()
{
	ImGui::Checkbox("Enabled", &m_bEnabled);

	ImGui::DragFloat("Update Interval", &m_fBehaviorUpdateInterval, 0.01f, 0.016f, 1.0f, "%.3f sec");

	ImGui::Text("Behavior Tree: %s", m_pxBehaviorTree ? m_pxBehaviorTree->GetName().c_str() : "(none)");
	ImGui::Text("Current Node: %s", m_szCurrentNodeName);

	if (m_pxBehaviorTree != nullptr)
	{
		const char* szStatus = "Unknown";
		switch (m_pxBehaviorTree->GetLastStatus())
		{
		case BTNodeStatus::SUCCESS: szStatus = "SUCCESS"; break;
		case BTNodeStatus::FAILURE: szStatus = "FAILURE"; break;
		case BTNodeStatus::RUNNING: szStatus = "RUNNING"; break;
		}
		ImGui::Text("Status: %s", szStatus);
	}

	// Blackboard viewer
	if (ImGui::CollapsingHeader("Blackboard"))
	{
		ImGui::Text("Entries: %u", m_xBlackboard.GetSize());
		// TODO: Iterate and display blackboard contents
	}
}
#endif
