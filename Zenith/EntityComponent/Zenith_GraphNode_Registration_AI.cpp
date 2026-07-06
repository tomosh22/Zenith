#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_GraphNodeHelpers.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"

#include <cmath>

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library - AI domain (navigation + perception).
//
// Navigation nodes reach the agent via Zenith_AIAgentComponent::
// GetNavMeshAgent() - a NON-OWNING pointer wired by game code, frequently
// null; every node null-checks component AND agent (FAILURE). The agent is
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
				Zenith_Entity xMover = g_xEngine.Scenes().ResolveEntity(Zenith_EntityID::FromPacked(m_ulMoverPacked));
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
	// path). Optional remaining-distance + velocity outputs.
	class Zenith_GraphNode_ReadNavState : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadNavState)
	public:
		ZENITH_PROPERTY(std::string, m_strStateVar, "navState")
		ZENITH_PROPERTY(std::string, m_strRemainingVar, "")
		ZENITH_PROPERTY(std::string, m_strVelocityVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

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
			Zenith_PropertyValue xValue;
			if (!m_strStateVar.empty())
			{
				xValue.SetInt32(iState);
				xContext.m_pxBlackboard->SetValue(m_strStateVar, xValue);
			}
			if (!m_strRemainingVar.empty())
			{
				xValue.SetFloat(pxNav->GetRemainingDistance());
				xContext.m_pxBlackboard->SetValue(m_strRemainingVar, xValue);
			}
			if (!m_strVelocityVar.empty())
			{
				xValue.SetVector3(pxNav->GetVelocity());
				xContext.m_pxBlackboard->SetValue(m_strVelocityVar, xValue);
			}
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
		ZENITH_PROPERTY(std::string, m_strSpeedVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_NavMeshAgent* pxNav = ResolveNavAgent(xContext, m_strTargetVar);
			if (pxNav == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			pxNav->SetMoveSpeed(m_strSpeedVar.empty()
				? m_fSpeed : xContext.m_pxBlackboard->GetFloat(m_strSpeedVar, m_fSpeed));
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
		ZENITH_PROPERTY(std::string, m_strRadiusVar, "")
		ZENITH_PROPERTY(std::string, m_strResultVar, "wanderPoint")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

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
			const float fRadius = m_strRadiusVar.empty()
				? m_fRadius : xContext.m_pxBlackboard->GetFloat(m_strRadiusVar, m_fRadius);
			Zenith_Maths::Vector3 xPoint;
			if (!pxNavMesh->GetRandomReachablePointInRadius(xCenter, fRadius, xPoint))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xValue;
			xValue.SetVector3(xPoint);
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
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
		ZENITH_PROPERTY(std::string, m_strCountVar, "perceivedCount")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

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
			if (!m_strCountVar.empty())
			{
				Zenith_PropertyValue xCount;
				xCount.SetInt32(static_cast<int32_t>(axOut.GetSize()));
				xContext.m_pxBlackboard->SetValue(m_strCountVar, xCount);
			}
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
		ZENITH_PROPERTY(std::string, m_strResultVar, "target")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

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
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
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
		ZENITH_PROPERTY(std::string, m_strPositionVar, "heardPos")
		ZENITH_PROPERTY(std::string, m_strSourceVar, "")
		ZENITH_PROPERTY(std::string, m_strAgeVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

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
			Zenith_PropertyValue xValue;
			if (!m_strPositionVar.empty())
			{
				xValue.SetVector3(xHeard.m_xPosition);
				xContext.m_pxBlackboard->SetValue(m_strPositionVar, xValue);
			}
			if (!m_strSourceVar.empty())
			{
				xValue.SetPackedEntityID(xHeard.m_xSourceEntity.GetPacked());
				xContext.m_pxBlackboard->SetValue(m_strSourceVar, xValue);
			}
			if (!m_strAgeVar.empty())
			{
				xValue.SetFloat(xHeard.m_fAge);
				xContext.m_pxBlackboard->SetValue(m_strAgeVar, xValue);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "QueryLastHeardSound"; }
	};

	// Awareness (0-1) of the entity in m_strOfVar -> float var. 0 = unknown
	// or fully decayed (the system forgets at 0). Always SUCCESS.
	class Zenith_GraphNode_QueryAwarenessOf : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_QueryAwarenessOf)
	public:
		ZENITH_PROPERTY(std::string, m_strOfVar, "target")
		ZENITH_PROPERTY(std::string, m_strResultVar, "awareness")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xAgent = xContext.ResolveTargetEntity(m_strTargetVar);
			Zenith_Entity xOf = xContext.ResolveTargetEntity(m_strOfVar);
			if (!xAgent.IsValid() || !xOf.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xValue;
			xValue.SetFloat(Zenith_PerceptionSystem::GetAwarenessOf(xAgent.GetEntityID(), xOf.GetEntityID()));
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
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
	xRegistry.RegisterNodeType<Zenith_GraphNode_NavMoveTo>("NavMoveTo", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetNavDestination>("SetNavDestination", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_StopNav>("StopNav", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadNavState>("ReadNavState", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetNavSpeed>("SetNavSpeed", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_FindRandomReachablePoint>("FindRandomReachablePoint", GRAPH_EVENT_NONE, 1, false, "AI");

	// Perception
	xRegistry.RegisterNodeType<Zenith_GraphNode_QueryPerceivedTargets>("QueryPerceivedTargets", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_QueryPrimaryPerceivedTarget>("QueryPrimaryPerceivedTarget", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_QueryLastHeardSound>("QueryLastHeardSound", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_QueryAwarenessOf>("QueryAwarenessOf", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_EmitSoundStimulus>("EmitSoundStimulus", GRAPH_EVENT_NONE, 1, false, "AI");
	xRegistry.RegisterNodeType<Zenith_GraphNode_RegisterPerceptionTarget>("RegisterPerceptionTarget", GRAPH_EVENT_NONE, 1, false, "AI");
}
