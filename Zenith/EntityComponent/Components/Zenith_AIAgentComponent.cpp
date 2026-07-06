#include "Zenith.h"
#include "Profiling/Zenith_Profiling.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#ifdef ZENITH_TOOLS
// Editor "Add Component" menu registry. Registering AIAgent here keeps the AI
// module the single owner of its component's registration (meta + editor menu)
// without the ECS reflection core ever naming AIAgent.
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
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
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<Zenith_AIAgentComponent>("AIAgent");
#endif
}

Zenith_AIAgentComponent::Zenith_AIAgentComponent(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

Zenith_AIAgentComponent::~Zenith_AIAgentComponent()
{
	// Note: we don't own the nav agent, it's set externally.
}

Zenith_AIAgentComponent::Zenith_AIAgentComponent(Zenith_AIAgentComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_pxNavMeshAgent(xOther.m_pxNavMeshAgent)
	, m_fUpdateInterval(xOther.m_fUpdateInterval)
	, m_bEnabled(xOther.m_bEnabled)
{
	// Clear moved-from object's pointer to prevent a dangling borrow, and disable
	// it so the pool's move-construct-then-destruct-source never ticks a corpse.
	xOther.m_pxNavMeshAgent = nullptr;
	xOther.m_bEnabled = false;
}

Zenith_AIAgentComponent& Zenith_AIAgentComponent::operator=(Zenith_AIAgentComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		m_xParentEntity = xOther.m_xParentEntity;
		m_pxNavMeshAgent = xOther.m_pxNavMeshAgent;
		m_fUpdateInterval = xOther.m_fUpdateInterval;
		m_bEnabled = xOther.m_bEnabled;

		xOther.m_pxNavMeshAgent = nullptr;
		xOther.m_bEnabled = false;
	}
	return *this;
}

void Zenith_AIAgentComponent::OnAwake()
{
	// Register with perception system
	Zenith_PerceptionSystem::RegisterAgent(m_xParentEntity.GetEntityID());
}

void Zenith_AIAgentComponent::OnUpdate(float fDt)
{
	if (!m_bEnabled)
	{
		return;
	}

	// Update navigation (runs every frame for smooth movement). The agent takes
	// only the entity id and resolves its transform + collider body through the
	// AI world hooks (engine-side): it prefers the physics path (SetLinearVelocity
	// on a dynamic Jolt body) when the entity has one, else falls back to direct
	// transform writes for transform-only test fixtures.
	if (m_pxNavMeshAgent != nullptr && m_xParentEntity.IsValid())
	{
		m_pxNavMeshAgent->Update(fDt, m_xParentEntity.GetEntityID());
	}
}

void Zenith_AIAgentComponent::OnDestroy()
{
	// Unregister from perception system
	Zenith_PerceptionSystem::UnregisterAgent(m_xParentEntity.GetEntityID());
}

void Zenith_AIAgentComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_bEnabled;
	xStream << m_fUpdateInterval;
}

void Zenith_AIAgentComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Back-compat: pre-teardown streams also carried a behaviour-tree asset path +
	// a blackboard here. The component-meta reader is size-prefixed and realigns
	// the cursor to the declared payload boundary after this returns, so those
	// trailing legacy bytes are absorbed without desyncing the stream.
	xStream >> m_bEnabled;
	xStream >> m_fUpdateInterval;
}

#ifdef ZENITH_TOOLS
void Zenith_AIAgentComponent::RenderPropertiesPanel()
{
	ImGui::Checkbox("Enabled", &m_bEnabled);
	ImGui::DragFloat("Update Interval", &m_fUpdateInterval, 0.01f, 0.016f, 1.0f, "%.3f sec");
	ImGui::Text("Nav agent: %s", m_pxNavMeshAgent ? "wired" : "(none)");
}
#endif

// AI unit tests are hosted here (engine-side) rather than in their sibling AI-leaf
// .cpp: the tests exercise g_xEngine / concrete components, which the AI leaf must
// not name. This TU is always linked (the component registrar references
// Zenith_AIAgentComponent), so the ZENITH_TEST registrars survive /OPT:REF.
#ifdef ZENITH_TESTING
#include "AI/Zenith_AIDebugVariables.Tests.inl"
#include "AI/Navigation/Zenith_NavMesh.Tests.inl"
#include "AI/Navigation/Zenith_NavMeshAgent.Tests.inl"
#include "AI/Navigation/Zenith_NavMeshGenerator.Tests.inl"
#include "AI/Navigation/Zenith_Pathfinding.Tests.inl"
#include "AI/Perception/Zenith_PerceptionSystem.Tests.inl"
#include "AI/Squad/Zenith_Formation.Tests.inl"
#include "AI/Squad/Zenith_Squad.Tests.inl"
#include "AI/Squad/Zenith_TacticalPoint.Tests.inl"
#endif
