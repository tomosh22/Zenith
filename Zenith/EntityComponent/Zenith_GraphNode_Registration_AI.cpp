#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "EntityComponent/Components/Zenith_NavMeshComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_Query.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Zenith_GraphNodeHelpers.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include <cmath>

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library - AI domain (navigation + perception).
//
// Navigation nodes reach the agent via Zenith_AIAgentComponent::
// GetNavMeshAgent() - null until something wires it; every node null-checks
// component AND agent (FAILURE). EnsureNavAgent is what a GRAPH uses to do
// that wiring: before it existed, SetNavMeshAgent had exactly three callers,
// all in game C++, so navigation was unreachable from a graph alone. The agent is
// ticked by the component's OnUpdate - nodes never call agent Update.
// NavMoveTo transplants the Zenith_BTAction_MoveTo/MoveToEntity semantics:
// node-owned repath timer (no agent-side repath exists), and the
// acceptance-radius check on arrival - HasReachedDestination() means "end
// of the path found", which on a PARTIAL path is short of the request (the
// stuck-priest bug); real arrival = actual distance <= acceptance radius.
//
// Perception nodes wrap the static Zenith_PerceptionSystem. It only
// advances when something ticks Zenith_PerceptionSystem::Update - games
// tick it themselves or opt in via Zenith_AI::SetEngineTickEnabled(true);
// in a game that ticks nothing, EmitSoundStimulus/awareness are inert.
// Queries return safe defaults for unregistered agents.
//
// ★ THE PINS IN THIS TU ARE LIVE (B-6.5), on the pattern B-6.1 established and
// _Entity.cpp / _Physics.cpp / _Animation.cpp follow. Every INPUT descriptor is
// read through Zenith_GraphNode::GetInput and every OUTPUT descriptor is written
// through SetOutput, so a wire into or out of any of these 13 nodes carries a
// pin declares `static constexpr u_int uPIN_<Name>` immediately before its pin
// table (the INDEX is the runtime address; table order is the contract, asserted
// by GraphPinTable.AIPinIndicesMatchTables). SIX of the thirteen declare none:
// EnsureNavAgent, NavMoveTo, SetNavDestination, StopNav, EmitSoundStimulus and
// RegisterPerceptionTarget carry only reference pins, which stay DIRECT.
//
// Three things specific to THIS TU:
//
//  1. SEVEN GUARDED WRITES LOST THEIR `!m_strXVar.empty()` GUARD - ReadNavState's
//     three, QueryPerceivedTargets.Count, QueryLastHeardSound's three - and so
//     did the shared Zenith_PropertyValue scratch above them and the computation
//     INSIDE each guard (GetDistanceToGo(), GetVelocity(), the list size). Those
//     so the blackboard sees exactly what it saw; what is new is that the SLOT is
//     always latched, which is the only reason a wire can come off a Remaining or
//     an Age whose author never named a variable. The three writers that were
//     ALWAYS unconditional - FindRandomReachablePoint.Result,
//     QueryPrimaryPerceivedTarget.Result, QueryAwarenessOf.Result - take B-6.1's
//     variable literally named "".
//
//  2. TWO FAILURE SHAPES, and they are not interchangeable. SHAPE A (the five
//     query/read nodes): the FAILURE sits ABOVE every accessor, so a failed
//     directly-constructed instance never even self-bound - GetOutputForTest
//     reads null, not a stamped zero - and nothing at all was written. SHAPE B
//     (FindRandomReachablePoint alone): its no-reachable-point FAILURE runs AFTER
//     the Radius GetInput, so THAT path HAS built pin state and Result holds its
//     stamped (0,0,0) - the Raycast-miss shape. Its no-mesh and
//     unresolvable-centre FAILUREs precede the read and build nothing. Either
//     way a consumer wire off any of these outputs must be gated on SUCCESS.
//
//  3. ENTITY_ID OUTPUTS (QueryPrimaryPerceivedTarget.Result,
//     QueryLastHeardSound.Source) go through the NON-template SetOutput with a
//     SetPackedEntityID-stamped value - Zenith_PropertyTraits has no u_int64
//     specialisation. Their stamped zero is packed 0 = {index 0, generation 0},
//     a LEGAL entity id and NOT INVALID_ENTITY_ID.
//------------------------------------------------------------------------------

namespace
{
	Zenith_NavMeshAgent* ResolveNavAgent(Zenith_GraphContext& xContext, const std::string& strTargetVar)
	{
		Zenith_Entity xTarget = xContext.ResolveTargetEntity(strTargetVar);
		if (!xTarget.IsValid())
		{
			return nullptr;
		}
		Zenith_AIAgentComponent* pxAgent = xTarget.TryGetComponent<Zenith_AIAgentComponent>();
		return pxAgent ? pxAgent->GetNavMeshAgent() : nullptr;
	}

	//==========================================================================
	// Navigation
	//==========================================================================

	// Binds a Zenith_NavMeshAgent to the target's Zenith_AIAgentComponent,
	// allocating one the component OWNS if it has none. This is the node that
	// makes every other nav node reachable from a graph.
	//
	// ★ WHY IT EXISTS. Zenith_AIAgentComponent::SetNavMeshAgent had three
	// callers before this, every one of them game C++ (RenderTest's tennis
	// component and DevilsPlayground's priest). There was no graph node, no
	// editor-automation step and no engine component that created or bound an
	// agent -- so every nav node returned FAILURE on the null pointer and
	// navigation could not be authored without dropping into C++. That
	// contradicts the engine's own doctrine (systems are components, gameplay is
	// graphs) and is exactly the class of gap ScriptTest exists to surface.
	//
	// RUNNING is what makes it compose under a one-shot anchor:
	//   OnStart -> EnsureNavAgent -> SetNavDestination -> NavMoveTo
	// is re-driven by the ON_UPDATE dispatch until the mesh resolves, the same
	// mechanism a suspended WaitForCondition relies on.
	class Zenith_GraphNode_EnsureNavAgent : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_EnsureNavAgent)
	public:
		// "" = the first Zenith_NavMeshComponent in the active scene; otherwise
		// an EntityID var naming the entity that holds it.
		ZENITH_PROPERTY(std::string, m_strNavMeshVar, "")
		// "" = self. Declared LAST, per the AI-family convention.
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// BOTH are ENTITY references: FindNavMeshComponent (below) resolves
		// m_strNavMeshVar through xContext.ResolveTargetEntity exactly the way the
		// Execute resolves m_strTargetVar, so an EntityID is the only legal value
		// for either. "" is the discovery/self path in both cases. Both being
		// references, this Execute addresses no pin and declares no uPIN_ constant.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_EnsureNavAgent)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(NavMesh, "m_strNavMeshVar")
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_AIAgentComponent* pxAgentComponent = xTarget.TryGetComponent<Zenith_AIAgentComponent>();
			if (pxAgentComponent == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}

			Zenith_NavMeshComponent* pxNavMeshComponent = FindNavMeshComponent(xContext);
			if (pxNavMeshComponent == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}

			// ★ THE LOAD STATE ALONE CANNOT SEPARATE "not yet" FROM "never".
			// NAVMESH_LOAD_STATE_UNLOADED is documented as "no ref, OR explicitly
			// unloaded", so the asset ref has to be consulted too: an unconfigured
			// component is a FAILURE (nothing is coming), while a configured one
			// whose OnStart has not run yet is RUNNING (it is coming next frame).
			switch (pxNavMeshComponent->GetLoadState())
			{
			case NAVMESH_LOAD_STATE_LOADED:
				break;
			case NAVMESH_LOAD_STATE_FAILED:
				// The header calls a ref that will not load a DEFECT, not a wait
				// state -- so this never becomes RUNNING and never resolves.
				return GRAPH_NODE_STATUS_FAILURE;
			default:
				return pxNavMeshComponent->GetAssetRef().empty()
					? GRAPH_NODE_STATUS_FAILURE	// unconfigured: nothing is coming
					: GRAPH_NODE_STATUS_RUNNING;	// OnStart is deferred to the first Update
			}

			Zenith_NavMesh* pxNavMesh = pxNavMeshComponent->GetNavMesh();
			if (pxNavMesh == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}

			Zenith_NavMeshAgent* pxNavAgent = pxAgentComponent->EnsureOwnedNavMeshAgent();
			if (pxNavAgent == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}

			// ★ "ALREADY WIRED" MEANS BOUND TO *THIS* MESH, NOT MERELY NON-NULL.
			// A component owns its mesh for its own lifetime and the header warns
			// never to hold the pointer across a scene change -- so an agent left
			// over from a previous scene reads as wired while pointing at freed
			// memory. Comparing the two pointers is what catches that.
			if (pxNavAgent->GetNavMesh() != pxNavMesh)
			{
				pxNavAgent->SetNavMesh(pxNavMesh);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "EnsureNavAgent"; }

	private:
		Zenith_NavMeshComponent* FindNavMeshComponent(Zenith_GraphContext& xContext) const
		{
			if (!m_strNavMeshVar.empty())
			{
				Zenith_Entity xHolder = xContext.ResolveTargetEntity(m_strNavMeshVar);
				return xHolder.IsValid() ? xHolder.TryGetComponent<Zenith_NavMeshComponent>() : nullptr;
			}
			// Ordinary ECS discovery -- the same query the component's own docs
			// name. First match wins; a scene with two navmeshes should name one
			// through m_strNavMeshVar rather than rely on query order.
			Zenith_NavMeshComponent* pxFound = nullptr;
			Zenith_ActiveScenes().QueryActiveScene<Zenith_NavMeshComponent>().ForEach(
				[&pxFound](Zenith_EntityID, Zenith_NavMeshComponent& xComponent)
				{
					if (pxFound == nullptr)
					{
						pxFound = &xComponent;
					}
				});
			return pxFound;
		}
	};

	// Moves the agent to a position ref (re-resolved every repath, so an
	// EntityID var gives entity-follow). RUNNING until within
	// m_fAcceptanceRadius of the destination - checked EVERY tick (the BT
	// MoveToEntity semantic: a chase succeeds the moment the agent is in
	// range, not at end-of-path) - then Stop() + SUCCESS. End-of-path while
	// still out of range = unreachable/PARTIAL destination = Stop() +
	// FAILURE. OnAbort stops the agent - the Selector-branch workhorse.
	class Zenith_GraphNode_NavMoveTo : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_NavMoveTo)
	public:
		ZENITH_PROPERTY(std::string, m_strDestinationVar, "target")
		ZENITH_PROPERTY_RANGED(float, m_fAcceptanceRadius, 2.0f, 0.01f, 1000.0f)
		ZENITH_PROPERTY_RANGED(float, m_fRepathInterval, 0.5f, 0.05f, 60.0f)
		ZENITH_PROPERTY(bool, m_bXZDistance, true)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// Destination is a POSITION ref, re-resolved every repath - an EntityID
		// var gives entity-follow, a vec3 var a fixed point. The radii, the
		// repath interval and the XZ flag are consts with no var partner. Both
		// pins are references, so this Execute addresses no pin and declares no
		// uPIN_ constant.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_NavMoveTo)
		ZENITH_GRAPH_PIN_TARGET_POSITION(Destination, "m_strDestinationVar")
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		void OnEnter(Zenith_GraphContext&) override
		{
			// Prime for an immediate first path (the BT MoveToEntity pattern).
			m_fTimeSinceRepath = m_fRepathInterval;
		}

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xMover = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xMover.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_AIAgentComponent* pxAgent = xMover.TryGetComponent<Zenith_AIAgentComponent>();
			Zenith_NavMeshAgent* pxNav = pxAgent ? pxAgent->GetNavMeshAgent() : nullptr;
			Zenith_TransformComponent* pxTransform = xMover.TryGetComponent<Zenith_TransformComponent>();
			if (pxNav == nullptr || pxTransform == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// Latch the mover for OnAbort: the target var may be retargeted
			// while this node is suspended - the abort must stop the agent
			// the node actually drove.
			m_ulMoverPacked = xMover.GetEntityID().GetPacked();

			Zenith_Maths::Vector3 xDestination;
			if (!Zenith_GraphNode_ResolvePositionRef(xContext, m_strDestinationVar, xDestination))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}

			// In range right now? Done - checked every tick so a chase
			// succeeds mid-path (entity-follow keeps repathing otherwise).
			Zenith_Maths::Vector3 xPosition;
			pxTransform->GetPosition(xPosition);
			Zenith_Maths::Vector3 xDelta = xDestination - xPosition;
			if (m_bXZDistance)
			{
				xDelta.y = 0.0f;	// capsules settle above the floor
			}
			if (glm::dot(xDelta, xDelta) <= m_fAcceptanceRadius * m_fAcceptanceRadius)
			{
				pxNav->Stop();
				return GRAPH_NODE_STATUS_SUCCESS;
			}

			m_fTimeSinceRepath += xContext.m_fDt;
			if (m_fTimeSinceRepath >= m_fRepathInterval)
			{
				m_fTimeSinceRepath = 0.0f;
				if (!pxNav->SetDestination(xDestination))
				{
					return GRAPH_NODE_STATUS_FAILURE;	// no navmesh wired
				}
			}

			if (pxNav->NeedsPath())
			{
				return GRAPH_NODE_STATUS_RUNNING;	// path computes on the agent's next update
			}
			if (pxNav->HasReachedDestination())
			{
				// End of path but still out of range (the in-range case
				// returned above): PARTIAL path to an unreachable request.
				pxNav->Stop();
				return GRAPH_NODE_STATUS_FAILURE;
			}
			if (!pxNav->HasPath())
			{
				return GRAPH_NODE_STATUS_FAILURE;	// pathfind failed / agent stopped
			}
			return GRAPH_NODE_STATUS_RUNNING;	// en route
		}

		void OnAbort(Zenith_GraphContext&) override
		{
			// Stop the LATCHED mover, not whatever the target var says now.
			if (m_ulMoverPacked != 0)
			{
				Zenith_Entity xMover = Zenith_ActiveScenes().ResolveEntity(Zenith_EntityID::FromPacked(m_ulMoverPacked));
				if (xMover.IsValid())
				{
					if (Zenith_AIAgentComponent* pxAgent = xMover.TryGetComponent<Zenith_AIAgentComponent>())
					{
						if (Zenith_NavMeshAgent* pxNav = pxAgent->GetNavMeshAgent())
						{
							pxNav->Stop();
						}
					}
				}
				m_ulMoverPacked = 0;
			}
			m_fTimeSinceRepath = 0.0f;
		}
		const char* GetTypeName() const override { return "NavMoveTo"; }

	private:
		float m_fTimeSinceRepath = 0.0f;
		u_int64 m_ulMoverPacked = 0;
	};

	// Fire-and-forget destination issue (momentum-preserving; safe to
	// re-issue). Chains continue immediately - pair with ReadNavState.
	class Zenith_GraphNode_SetNavDestination : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetNavDestination)
	public:
		ZENITH_PROPERTY(std::string, m_strDestinationVar, "target")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// Both pins are references (a position ref and the mover), resolved
		// directly, so no uPIN_ constant exists.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetNavDestination)
		ZENITH_GRAPH_PIN_TARGET_POSITION(Destination, "m_strDestinationVar")
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_NavMeshAgent* pxNav = ResolveNavAgent(xContext, m_strTargetVar);
			if (pxNav == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector3 xDestination;
			if (!Zenith_GraphNode_ResolvePositionRef(xContext, m_strDestinationVar, xDestination))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			return pxNav->SetDestination(xDestination)
				? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
		}
		const char* GetTypeName() const override { return "SetNavDestination"; }
	};

	// Clears path + zeroes agent velocity. (A physics-driven agent's body
	// keeps its last velocity and coasts - engine behaviour.)
	class Zenith_GraphNode_StopNav : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_StopNav)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// Target only: no pin is addressed, so no uPIN_ constant exists.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_StopNav)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_NavMeshAgent* pxNav = ResolveNavAgent(xContext, m_strTargetVar);
			if (pxNav == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxNav->Stop();
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "StopNav"; }
	};

	// Nav state -> blackboard. State int: 0 = none (no path, none pending -
	// idle or failed), 1 = path pending, 2 = moving, 3 = arrived (end of
	// path). Optional remaining-distance + velocity outputs; the remaining
	// distance is the agent's DistanceToGo (the leg it is on plus every segment
	// after it), not the waypoint-only sum.
	class Zenith_GraphNode_ReadNavState : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadNavState)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// All three are the node's own COMPUTED reads of the agent, written
		// through SetOutput in the Execute below - State is the 0-3 code this
		// node derives, not a name.
		//
		// ★ THE THREE `!m_strXVar.empty()` GUARDS ARE GONE (B-6.5), and so are the
		// Zenith_PropertyValue scratch they shared and the GetDistanceToGo() /
		// GetVelocity() calls that sat INSIDE them. All three slots are latched
		// same non-empty rule, so which of the three reach the BLACKBOARD is
		// unchanged - navState by default, Remaining and Velocity only when the
		// author names them. This is PARITY, not a divergence: an empty name
		// created no blackboard variable before and creates none now.
		static constexpr u_int uPIN_State = 0u;
		static constexpr u_int uPIN_Remaining = 1u;
		static constexpr u_int uPIN_Velocity = 2u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_ReadNavState)
		ZENITH_GRAPH_PIN_OUTPUT(State, PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PIN_OUTPUT(Remaining, PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT(Velocity, PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_NavMeshAgent* pxNav = ResolveNavAgent(xContext, m_strTargetVar);
			if (pxNav == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			int32_t iState = 0;
			if (pxNav->NeedsPath())
			{
				iState = 1;
			}
			else if (pxNav->HasReachedDestination())
			{
				iState = 3;
			}
			else if (pxNav->HasPath())
			{
				iState = 2;
			}
			SetOutput<int32_t>(xContext, uPIN_State, iState);
			// GetDistanceToGo, NOT GetRemainingDistance: the latter is
			// waypoint-to-waypoint segments only and reads 0 for the whole
			// final leg -- which on a straight-line path across open ground
			// is the entire journey. A graph reading a permanently-zero
			// "remaining distance" is worse than having no node at all.
			SetOutput<float>(xContext, uPIN_Remaining, pxNav->GetDistanceToGo());
			SetOutput<Zenith_Maths::Vector3>(xContext, uPIN_Velocity, pxNav->GetVelocity());
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadNavState"; }
	};

	// Move speed in m/s (const or var). Note: a game component that owns the
	// agent may re-apply its own tuning over this.
	class Zenith_GraphNode_SetNavSpeed : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetNavSpeed)
	public:
		ZENITH_PROPERTY_RANGED(float, m_fSpeed, 5.0f, 0.0f, 1000.0f)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// behind one pin. Target is a reference and stays direct.
		static constexpr u_int uPIN_Speed = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_SetNavSpeed)
		ZENITH_GRAPH_PIN_INPUT_CONST(Speed, "m_fSpeed", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_NavMeshAgent* pxNav = ResolveNavAgent(xContext, m_strTargetVar);
			if (pxNav == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// After the nav-agent guard, exactly where the blackboard read sat: a
			// node that FAILS on a missing agent must not pull its input.
			pxNav->SetMoveSpeed(GetInput<float>(xContext, uPIN_Speed));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetNavSpeed"; }
	};

	// Random reachable point within an XZ radius of a position ref ("" =
	// self) -> vec3 var. Reachable = polygon-connected from the nearest
	// polygon (islands excluded). The wander/patrol primitive.
	class Zenith_GraphNode_FindRandomReachablePoint : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_FindRandomReachablePoint)
	public:
		ZENITH_PROPERTY(std::string, m_strCenterVar, "")
		ZENITH_PROPERTY_RANGED(float, m_fRadius, 15.0f, 0.1f, 10000.0f)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// Center is a POSITION ref ("" = self) and stays direct; Radius carries
		// point this node COMPUTES and writes.
		//
		// ★ Result's write was ALWAYS unconditional, so this is one of the TU's
		// three `""` divergence sites: an empty m_strResultVar no longer creates a
		// blackboard variable literally named "". The slot still carries the point.
		static constexpr u_int uPIN_Radius = 1u;
		static constexpr u_int uPIN_Result = 2u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_FindRandomReachablePoint)
		ZENITH_GRAPH_PIN_TARGET_POSITION(Center, "m_strCenterVar")
		ZENITH_GRAPH_PIN_INPUT_CONST(Radius, "m_fRadius", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_NavMeshAgent* pxNav = ResolveNavAgent(xContext, m_strTargetVar);
			const Zenith_NavMesh* pxNavMesh = pxNav ? pxNav->GetNavMesh() : nullptr;
			if (pxNavMesh == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector3 xCenter;
			if (!Zenith_GraphNode_ResolvePositionRef(xContext, m_strCenterVar, xCenter))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			// ★ EXACTLY WHERE THE BLACKBOARD READ WAS, which for this node is
			// after the navmesh and centre-resolve guards but BEFORE the
			// no-reachable-point FAILURE below - the Raycast.Direction shape from
			// B-6.3. Moving it down would change which executions read the pin: a
			// wander that found nothing has ALREADY read Radius (and, in a graph,
			// already pulled its producer), while a bound-agent or centre failure
			// has not.
			const float fRadius = GetInput<float>(xContext, uPIN_Radius);
			Zenith_Maths::Vector3 xPoint;
			if (!pxNavMesh->GetRandomReachablePointInRadius(xCenter, fRadius, xPoint))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			SetOutput<Zenith_Maths::Vector3>(xContext, uPIN_Result, xPoint);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "FindRandomReachablePoint"; }
	};

	//==========================================================================
	// Perception
	//==========================================================================

	// Perceived targets -> blackboard LIST of packed EntityIDs (+ count).
	// Unregistered agent = empty list, SUCCESS (degrade gracefully).
	class Zenith_GraphNode_QueryPerceivedTargets : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_QueryPerceivedTargets)
	public:
		ZENITH_PROPERTY(bool, m_bHostileOnly, false)
		ZENITH_PROPERTY(bool, m_bVisibleOnly, false)
		ZENITH_PROPERTY(std::string, m_strListVar, "perceived")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// List names the blackboard's parallel LIST store (GetOrCreateList in the
		// Execute below), which holds no Zenith_PropertyValue and is therefore
		// never typed. Count is an ordinary computed OUTPUT beside it. The two
		// filter flags are consts with no var partner.
		//
		// ★ The Count write below is UNCONDITIONAL now. It used to sit inside
		// non-empty rule, so the blackboard is unchanged and the slot now always
		// latches. The LIST name stays a direct GetOrCreateList - a list is not a
		// Zenith_PropertyValue and can never be a wire.
		static constexpr u_int uPIN_Count = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_QueryPerceivedTargets)
		ZENITH_GRAPH_PIN_LIST(List, "m_strListVar")
		ZENITH_GRAPH_PIN_OUTPUT(Count, PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xAgent = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xAgent.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Vector<Zenith_PropertyValue>& axOut = xContext.m_pxBlackboard->GetOrCreateList(m_strListVar);
			axOut.Clear();
			// Copy immediately - the returned pointer aliases live system
			// storage that mutates on register/unregister.
			if (const Zenith_Vector<Zenith_PerceivedTarget>* paxTargets
				= Zenith_PerceptionSystem::GetPerceivedTargets(xAgent.GetEntityID()))
			{
				for (u_int u = 0; u < paxTargets->GetSize(); ++u)
				{
					const Zenith_PerceivedTarget& xTarget = paxTargets->Get(u);
					if ((m_bHostileOnly && !xTarget.m_bHostile) || (m_bVisibleOnly && !xTarget.m_bCurrentlyVisible))
					{
						continue;
					}
					Zenith_PropertyValue xValue;
					xValue.SetPackedEntityID(xTarget.m_xEntityID.GetPacked());
					axOut.PushBack(xValue);
				}
			}
			SetOutput<int32_t>(xContext, uPIN_Count, static_cast<int32_t>(axOut.GetSize()));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "QueryPerceivedTargets"; }
	};

	// Highest-awareness HOSTILE target -> EntityID var; no target = FAILURE
	// (the has-target gate). Non-hostile registered targets never surface
	// here - query them with QueryAwarenessOf.
	class Zenith_GraphNode_QueryPrimaryPerceivedTarget : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_QueryPrimaryPerceivedTarget)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// Result carries a PACKED EntityID (SetPackedEntityID below), so it is an
		// ENTITY_ID output - the same type a TARGET_ENTITY pin downstream accepts.
		// It goes through the NON-template SetOutput: Zenith_PropertyTraits has no
		// u_int64 specialisation.
		//
		// ★ The write was ALWAYS unconditional, so this is a `""` divergence site.
		// ★ m_strResultVar defaults to "target", which NavMoveTo /
		// SetNavDestination / QueryAwarenessOf all READ by default - a
		// QueryPrimaryPerceivedTarget feeding any of them communicates through
		// that shared default alone today, and must carry a WIRE (or an explicitly
		static constexpr u_int uPIN_Result = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_QueryPrimaryPerceivedTarget)
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xAgent = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xAgent.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_EntityID xPrimary = Zenith_PerceptionSystem::GetPrimaryTarget(xAgent.GetEntityID());
			if (xPrimary == INVALID_ENTITY_ID)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xValue;
			xValue.SetPackedEntityID(xPrimary.GetPacked());
			SetOutput(xContext, uPIN_Result, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "QueryPrimaryPerceivedTarget"; }
	};

	// Freshest heard contact -> position/source/age vars; nothing heard =
	// FAILURE (the investigate gate). "Heard" is sticky per contact: a later
	// sighting of the same source updates the position.
	class Zenith_GraphNode_QueryLastHeardSound : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_QueryLastHeardSound)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// ★ Position here is an OUTPUT, not a position REF: this node WRITES the
		// heard position (SetOutput below). Contrast EmitSoundStimulus, whose
		// identically-named property is a Zenith_GraphNode_ResolvePositionRef
		// input.
		//
		// ★ ALL THREE `!m_strXVar.empty()` GUARDS ARE GONE (B-6.5), along with the
		// Zenith_PropertyValue scratch they shared. Every slot is latched
		// the same non-empty rule, so the blackboard is unchanged - heardPos by
		// default, Source and Age only when named. PARITY, not a divergence.
		// Source is an ENTITY_ID and uses the NON-template SetOutput.
		static constexpr u_int uPIN_Position = 0u;
		static constexpr u_int uPIN_Source = 1u;
		static constexpr u_int uPIN_Age = 2u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_QueryLastHeardSound)
		ZENITH_GRAPH_PIN_OUTPUT(Position, PROPERTY_TYPE_VECTOR3)
		ZENITH_GRAPH_PIN_OUTPUT(Source, PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PIN_OUTPUT(Age, PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xAgent = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xAgent.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_PerceptionSystem::Zenith_LastHeardSound xHeard
				= Zenith_PerceptionSystem::GetLastHeardSoundFor(xAgent.GetEntityID());
			if (!xHeard.m_bValid)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			SetOutput<Zenith_Maths::Vector3>(xContext, uPIN_Position, xHeard.m_xPosition);
			Zenith_PropertyValue xSource;
			xSource.SetPackedEntityID(xHeard.m_xSourceEntity.GetPacked());
			SetOutput(xContext, uPIN_Source, xSource);
			SetOutput<float>(xContext, uPIN_Age, xHeard.m_fAge);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "QueryLastHeardSound"; }
	};

	// Awareness (0-1) of the entity in m_strOfVar -> float var. 0 = unknown
	// or fully decayed (the system forgets at 0). FAILURE when EITHER reference
	// fails to resolve - the agent or the entity it is asked about - and in that
	// case nothing is read and nothing is written. It carries no On Failure exec
	// pin, so a consumer wire off Result must be gated on SUCCESS some other way.
	class Zenith_GraphNode_QueryAwarenessOf : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_QueryAwarenessOf)
	public:
		ZENITH_PROPERTY(std::string, m_strOfVar, "target")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// TWO entity references in one node: Target is the agent DOING the
		// perceiving, Of is the entity it is asked about - both go through
		// xContext.ResolveTargetEntity, so both accept an EntityID only - both stay
		// DIRECT, and only Result is addressed as a pin.
		//
		// ★ Result's write was ALWAYS unconditional, so this is the third and last
		// `""` divergence site in this TU.
		// ★ m_strOfVar defaults to "target", the same name
		// on that node.
		static constexpr u_int uPIN_Result = 1u;

		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_QueryAwarenessOf)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Of, "m_strOfVar")
		ZENITH_GRAPH_PIN_OUTPUT(Result, PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xAgent = xContext.ResolveTargetEntity(m_strTargetVar);
			Zenith_Entity xOf = xContext.ResolveTargetEntity(m_strOfVar);
			if (!xAgent.IsValid() || !xOf.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			SetOutput<float>(xContext, uPIN_Result,
				Zenith_PerceptionSystem::GetAwarenessOf(xAgent.GetEntityID(), xOf.GetEntityID()));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "QueryAwarenessOf"; }
	};

	// Emits a one-shot sound stimulus at a position ref ("" = self) with
	// self as the source (agents never hear their own sounds). Loudness
	// convention: footstep ~0.3/10m, gunshot ~1.0/50m.
	class Zenith_GraphNode_EmitSoundStimulus : public Zenith_GraphNode
	{
	public:
	ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_EmitSoundStimulus)
public:
		ZENITH_PROPERTY(std::string, m_strPositionVar, "")
	ZENITH_PROPERTY_RANGED(float, m_fLoudness, 0.5f, 0.0f, 10.0f)
		ZENITH_PROPERTY_RANGED(float, m_fRadius, 10.0f, 0.1f, 10000.0f)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// Position is a POSITION ref here (Zenith_GraphNode_ResolvePositionRef
		// below; "" = self), unlike QueryLastHeardSound's output of the same name.
		// Loudness and radius are consts with NO var partner, so they are not pins
		// at all - both of this node's pins are references and its Execute
		// addresses none, hence no uPIN_ constant.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_EmitSoundStimulus)
		ZENITH_GRAPH_PIN_TARGET_POSITION(Position, "m_strPositionVar")
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xSource = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xSource.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;	// hearing skips invalid sources entirely
			}
			Zenith_Maths::Vector3 xPosition;
			if (!Zenith_GraphNode_ResolvePositionRef(xContext, m_strPositionVar, xPosition))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PerceptionSystem::EmitSoundStimulus(xPosition, m_fLoudness, m_fRadius, xSource.GetEntityID());
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "EmitSoundStimulus"; }
	};

	// Registers/unregisters the target ("" = self) as a perceivable target.
	// Nothing auto-unregisters on entity destruction - author the symmetric
	// unregister (m_bUnregister) in teardown chains.
	class Zenith_GraphNode_RegisterPerceptionTarget : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_RegisterPerceptionTarget)
	public:
		ZENITH_PROPERTY(bool, m_bHostile, true)
		ZENITH_PROPERTY(bool, m_bUnregister, false)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		// The two flags are consts with no var partner; Target is a reference.
		// Nothing here is addressed as a pin, so no uPIN_ constant exists.
		ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_RegisterPerceptionTarget)
		ZENITH_GRAPH_PIN_TARGET_ENTITY(Target, "m_strTargetVar")
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			if (m_bUnregister)
			{
				Zenith_PerceptionSystem::UnregisterTarget(xTarget.GetEntityID());
			}
			else
			{
				Zenith_PerceptionSystem::RegisterTarget(xTarget.GetEntityID(), m_bHostile);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "RegisterPerceptionTarget"; }
	};
}

void Zenith_RegisterEngineGraphNodes_AI()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	// Navigation
	//
	// ★ LINE NUMBERS DELIBERATELY ABSENT. Every citation here named a line this
	// file has since moved, and three of them were already wrong before B-6.5
	// touched anything - the guard's CONDITION is what a reader needs and it does
	// not rot.
	//
	// On Failure = THE MESH WILL NEVER COME (the load state is FAILED, or it is
	// UNLOADED with an EMPTY asset ref - nothing is coming) or THE AGENT COULD NOT
	// BE ALLOCATED + misconfiguration guards (no target, no AIAgentComponent, no
	// NavMeshComponent, no mesh behind a LOADED component). RUNNING while a
	// CONFIGURED mesh is still loading is not FAILURE and is unaffected by this pin.
	xRegistry.RegisterNodeType<Zenith_GraphNode_EnsureNavAgent>("EnsureNavAgent", GRAPH_EVENT_NONE, 1, false, "AI", true);
	xRegistry.RegisterNodeType<Zenith_GraphNode_NavMoveTo>("NavMoveTo", GRAPH_EVENT_NONE, 1, false, "AI");
	// On Failure = NO BOUND AGENT or NO PATH to the destination (SetDestination
	// refused) + a misconfiguration guard (unresolvable destination ref).
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetNavDestination>("SetNavDestination", GRAPH_EVENT_NONE, 1, false, "AI", true);
	xRegistry.RegisterNodeType<Zenith_GraphNode_StopNav>("StopNav", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadNavState>("ReadNavState", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetNavSpeed>("SetNavSpeed", GRAPH_EVENT_NONE, 1, false, "AI");
	// misconfiguration guards (no bound agent / no mesh, unresolvable centre ref).
	// ★ Only the FIRST of those runs BELOW the Radius GetInput, which is why a
	// two have not.
	xRegistry.RegisterNodeType<Zenith_GraphNode_FindRandomReachablePoint>("FindRandomReachablePoint", GRAPH_EVENT_NONE, 1, false, "AI", true);

	// Perception
	xRegistry.RegisterNodeType<Zenith_GraphNode_QueryPerceivedTargets>("QueryPerceivedTargets", GRAPH_EVENT_NONE, 1, false, "AI");
	// On Failure = NOTHING PERCEIVED (GetPrimaryTarget answered INVALID_ENTITY_ID,
	// incl. an unregistered agent) + a misconfiguration guard (invalid target).
	xRegistry.RegisterNodeType<Zenith_GraphNode_QueryPrimaryPerceivedTarget>("QueryPrimaryPerceivedTarget", GRAPH_EVENT_NONE, 1, false, "AI", true);
	// On Failure = NOTHING HEARD (the Zenith_LastHeardSound is not valid, incl. an
	// unregistered agent) + a misconfiguration guard (invalid target).
	xRegistry.RegisterNodeType<Zenith_GraphNode_QueryLastHeardSound>("QueryLastHeardSound", GRAPH_EVENT_NONE, 1, false, "AI", true);
	xRegistry.RegisterNodeType<Zenith_GraphNode_QueryAwarenessOf>("QueryAwarenessOf", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_EmitSoundStimulus>("EmitSoundStimulus", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_RegisterPerceptionTarget>("RegisterPerceptionTarget", GRAPH_EVENT_NONE, 1, false, "AI");
}

#include "EntityComponent/Zenith_GraphNode_Registration_AI.Tests.inl"
