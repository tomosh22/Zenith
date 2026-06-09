#include "Zenith.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_BehaviorTree.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#ifdef ZENITH_TOOLS
// Editor "Add Component" menu registry. Registering AIAgent here keeps the AI
// module the single owner of its component's registration (meta + editor menu)
// without the ECS reflection core ever naming AIAgent.
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

// Registrar for the AI module's components, invoked engine-side from
// Zenith_RegisterEngineComponents() (EntityComponent/Zenith_ComponentMeta_Registration.cpp)
// via a forward declaration. Defining it here (a TU that sees the full
// Zenith_AIAgentComponent header) keeps the EntityComponent module free of any AI
// dependency. Order 90 is passed explicitly now that the ECS core no longer holds
// a name->order map; it matches the value AIAgent had in the former
// GetSerializationOrder() map (serialized after the built-ins).
void Zenith_AI_RegisterComponents()
{
	Zenith_ComponentMetaRegistry::Get().RegisterComponent<Zenith_AIAgentComponent>("AIAgent", 90);

#ifdef ZENITH_TOOLS
	// Mirror AIAgent into the editor menu registry. This is the side-effect that
	// used to fire implicitly inside Zenith_ComponentMetaRegistry::RegisterComponent<T>
	// before the ECS core was made leaf-clean; the AI module now owns it for its
	// own component. Inserted after the built-ins (this forwarder runs last),
	// preserving the historical menu ordering where AIAgent appeared last.
	Zenith_ComponentRegistry::Get().RegisterComponent<Zenith_AIAgentComponent>("AIAgent");
#endif
}

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
	// Behavior trees are built in code; there is no asset loader.
	if (!m_strBehaviorTreeAsset.empty() && m_pxBehaviorTree == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_AI, "Behavior tree asset '%s' requested but trees are built in code. Disabling AI agent.", m_strBehaviorTreeAsset.c_str());
		m_bEnabled = false;
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

	// Update navigation (runs every frame for smooth movement). The
	// NavMeshAgent prefers the physics path -- SetLinearVelocity on a
	// dynamic Jolt body -- when the entity has one. Pass through the
	// collider so the agent can dispatch to that path; absent a
	// collider it falls back to direct transform writes for transform-
	// only test fixtures.
	if (m_pxNavMeshAgent != nullptr && m_xParentEntity.IsValid())
	{
		if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			Zenith_TransformComponent& xTransform = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
			Zenith_ColliderComponent* pxCollider = nullptr;
			if (m_xParentEntity.HasComponent<Zenith_ColliderComponent>())
			{
				pxCollider = &m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
			}
			m_pxNavMeshAgent->Update(fDt, xTransform, pxCollider);
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
	xStream << m_bEnabled;
	xStream << m_fBehaviorUpdateInterval;
	xStream << m_strBehaviorTreeAsset;
	m_xBlackboard.WriteToDataStream(xStream);
}

void Zenith_AIAgentComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_bEnabled;
	xStream >> m_fBehaviorUpdateInterval;
	xStream >> m_strBehaviorTreeAsset;
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
		m_xBlackboard.IterateEntries(
			[](void*, const char* szKey, const char* szType, const char* szValue)
			{
				ImGui::BulletText("%s = %s (%s)", szKey, szValue, szType);
			},
			nullptr);
	}
}
#endif
