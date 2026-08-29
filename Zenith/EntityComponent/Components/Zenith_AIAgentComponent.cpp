#include "Zenith.h"
#include "Profiling/Zenith_Profiling.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Zenith_AIWorldHooks.h"
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
	// A BORROWED agent is still not ours to free; an OWNED one is.
	ReleaseOwnedNavMeshAgent();
}

Zenith_AIAgentComponent::Zenith_AIAgentComponent(Zenith_AIAgentComponent&& xOther) noexcept
	: m_xParentEntity(xOther.m_xParentEntity)
	, m_pxNavMeshAgent(xOther.m_pxNavMeshAgent)
	, m_pxOwnedNavMeshAgent(xOther.m_pxOwnedNavMeshAgent)
	, m_fUpdateInterval(xOther.m_fUpdateInterval)
	, m_bEnabled(xOther.m_bEnabled)
{
	// NEUTRALISE THE SOURCE, both pointers. Clearing only the active one would
	// leave the moved-from component still holding the owned agent and free it
	// from its destructor -- which the pool runs immediately after this. Also
	// disable it, so the pool's move-construct-then-destruct-source never ticks
	// a corpse.
	xOther.m_pxNavMeshAgent = nullptr;
	xOther.m_pxOwnedNavMeshAgent = nullptr;
	xOther.m_bEnabled = false;
}

Zenith_AIAgentComponent& Zenith_AIAgentComponent::operator=(Zenith_AIAgentComponent&& xOther) noexcept
{
	if (this != &xOther)
	{
		// Our own agent goes first: overwriting the pointer without this would
		// leak it, and there is no other owner left to free it.
		ReleaseOwnedNavMeshAgent();

		m_xParentEntity = xOther.m_xParentEntity;
		m_pxNavMeshAgent = xOther.m_pxNavMeshAgent;
		m_pxOwnedNavMeshAgent = xOther.m_pxOwnedNavMeshAgent;
		m_fUpdateInterval = xOther.m_fUpdateInterval;
		m_bEnabled = xOther.m_bEnabled;

		xOther.m_pxNavMeshAgent = nullptr;
		xOther.m_pxOwnedNavMeshAgent = nullptr;
		xOther.m_bEnabled = false;
	}
	return *this;
}

void Zenith_AIAgentComponent::ReleaseOwnedNavMeshAgent()
{
	if (m_pxOwnedNavMeshAgent != nullptr)
	{
		// The invariant says the active pointer is the same object, so it dies
		// with it -- never left dangling.
		Zenith_Assert(m_pxOwnedNavMeshAgent == m_pxNavMeshAgent,
			"AIAgent: owned nav agent is not the active one");
		delete m_pxOwnedNavMeshAgent;
		m_pxOwnedNavMeshAgent = nullptr;
		m_pxNavMeshAgent = nullptr;
	}
}

void Zenith_AIAgentComponent::SetNavMeshAgent(Zenith_NavMeshAgent* pxAgent)
{
	// ★ Installing the pointer that is already installed must do NOTHING. The
	// naive "free the old one, then assign" would delete the very agent being
	// installed and hand back a dangling pointer -- and this is reachable:
	// SetNavMeshAgent(GetNavMeshAgent()) is what a re-wire pass looks like.
	if (pxAgent == m_pxNavMeshAgent)
	{
		return;
	}
	ReleaseOwnedNavMeshAgent();
	m_pxNavMeshAgent = pxAgent;
}

Zenith_NavMeshAgent* Zenith_AIAgentComponent::EnsureOwnedNavMeshAgent()
{
	// A BORROW WINS. Game code that wired an agent deliberately keeps it; an
	// auto-wire never replaces an explicit decision.
	if (m_pxNavMeshAgent != nullptr)
	{
		return m_pxNavMeshAgent;
	}
	m_pxOwnedNavMeshAgent = new Zenith_NavMeshAgent();
	m_pxNavMeshAgent = m_pxOwnedNavMeshAgent;
	return m_pxNavMeshAgent;
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

#ifdef ZENITH_TOOLS
		// AI/Pathfinding/{Agent Paths, Path Waypoints}. Drawn from HERE rather than
		// from Zenith_AI::DebugDraw (which covers the world-level visualisations)
		// because the nav agent is owned by this component — there is no registry
		// of live Zenith_NavMeshAgents to walk. DebugDraw re-checks the master
		// toggle and both section flags itself.
		Zenith_Maths::Vector3 xAgentPos;
		if (Zenith_AI_GetEntityPosition(m_xParentEntity.GetEntityID(), xAgentPos))
		{
			m_pxNavMeshAgent->DebugDraw(xAgentPos);
		}
#endif
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
	ImGui::Text("Nav agent: %s", m_pxNavMeshAgent
		? (m_pxOwnedNavMeshAgent ? "owned" : "borrowed") : "(none)");
}
#endif

// AI unit tests are hosted here (engine-side) rather than in their sibling AI-leaf
// .cpp: the tests exercise g_xEngine / concrete components, which the AI leaf must
// not name. This TU is always linked (the component registrar references
// Zenith_AIAgentComponent), so the ZENITH_TEST registrars survive /OPT:REF.
#ifdef ZENITH_TESTING
#include "AI/Zenith_AIDebugVariables.Tests.inl"
// Zenith_NavMesh.Tests.inl FIRST: the baker and stats suites reuse its
// NavMeshPersistTempFile / byte-buffer helpers (one TU, so they share the
// anonymous namespace).
#include "AI/Navigation/Zenith_NavMesh.Tests.inl"
#include "AI/Navigation/Zenith_NavMeshBaker.Tests.inl"
#include "AI/Navigation/Zenith_NavMeshStats.Tests.inl"
#include "AI/Navigation/Zenith_NavMeshAgent.Tests.inl"
#include "AI/Navigation/Zenith_NavMeshGenerator.Tests.inl"
#include "AI/Navigation/Zenith_Pathfinding.Tests.inl"
#include "AI/Perception/Zenith_PerceptionSystem.Tests.inl"
#include "AI/Squad/Zenith_Formation.Tests.inl"
#include "AI/Squad/Zenith_Squad.Tests.inl"
#include "AI/Squad/Zenith_TacticalPoint.Tests.inl"
#endif
