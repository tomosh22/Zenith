#pragma once

#include "Core/Zenith_PropertySystem.h"
#include "ZenithECS/Zenith_Entity.h"

//------------------------------------------------------------------------------
// Zenith_GraphNode - base class for Behaviour Graph nodes.
//
// The Behaviour Graph runtime (Zenith/Scripting/) is the engine's gameplay
// scripting tier: designer-authored node graphs hosted on entities via
// Zenith_GraphComponent. This module is layer-clean: it depends on ZenithBase +
// ZenithECS only and never names a concrete component - concrete node
// implementations live in the EntityComponent glue layer
// (Zenith_GraphNode_Registration.cpp) and in per-game code, registered through
// Zenith_GraphNodeRegistry (the Zenith_ComponentMeta registrar pattern).
//
// Execution model (Phase 1): event-driven exec chains.
//   - Event-source nodes (OnStart/OnUpdate/OnCollision/Timer/OnCustomEvent...)
//     anchor chains. A (node, pin) has AT MOST ONE outgoing exec edge - chains
//     are linear; branching/looping is done by flow nodes with multiple output
//     pins that run their sub-chains via Zenith_BehaviourGraph::RunChainFromPin.
//   - Node status is a tri-state (SUCCESS / FAILURE / RUNNING): SUCCESS
//     continues the chain, FAILURE aborts it, RUNNING suspends it (the graph
//     resumes at the running node on the next OnUpdate dispatch).
//   - Node instances are per-graph-instance: plain members ARE per-instance
//     state (timers, loop counters). Node parameters are declared with the
//     ZENITH_PROPERTY macros - the registry serializes/inspects them through
//     the class's property table with zero per-node serialization code.
//------------------------------------------------------------------------------

class Zenith_BehaviourGraph;
class Zenith_GraphBlackboard;

enum GraphNodeStatus : u_int8
{
	GRAPH_NODE_STATUS_SUCCESS = 0,
	GRAPH_NODE_STATUS_FAILURE,
	GRAPH_NODE_STATUS_RUNNING,
};

// The engine-driven events a graph can anchor chains on. Custom (string-named)
// events are GRAPH_EVENT_CUSTOM + a name parameter on the source node.
enum GraphEventType : u_int8
{
	GRAPH_EVENT_NONE = 0,		// not an event source (action/flow node)
	GRAPH_EVENT_ON_START,
	GRAPH_EVENT_ON_UPDATE,
	GRAPH_EVENT_ON_FIXED_UPDATE,
	GRAPH_EVENT_ON_ENABLE,
	GRAPH_EVENT_ON_DISABLE,
	GRAPH_EVENT_ON_DESTROY,
	GRAPH_EVENT_ON_COLLISION_ENTER,
	GRAPH_EVENT_ON_COLLISION_STAY,
	GRAPH_EVENT_ON_COLLISION_EXIT,
	GRAPH_EVENT_TIMER,
	GRAPH_EVENT_CUSTOM,
	GRAPH_EVENT_ON_GRAPH_CALL,	// entry anchor of a callable sub-graph (CallGraph node / RunGraphCall)
	GRAPH_EVENT_COUNT
};	// append-only: event types are registry-derived at instantiation, never serialized

// One named value of a multi-field custom-event payload. The firer owns the
// names; OnCustomEvent sources stash every arg to the blackboard verbatim
// under its name (the generalized collision-source stash pattern).
struct Zenith_GraphEventArg
{
	std::string m_strName;
	Zenith_PropertyValue m_xValue;
};

// Everything a node needs while executing. Passed by reference down the chain.
struct Zenith_GraphContext
{
	Zenith_Entity m_xSelf;								// the entity hosting the graph
	float m_fDt = 0.0f;									// dt of the dispatching event (0 for non-tick events)
	float m_fTimeSeconds = 0.0f;						// engine wall-clock at dispatch (Cooldown-style gates under 0-dt events)
	Zenith_BehaviourGraph* m_pxGraph = nullptr;			// owning graph (RunChainFromPin for flow nodes)
	Zenith_GraphBlackboard* m_pxBlackboard = nullptr;	// the graph instance's variables
	const Zenith_PropertyValue* m_pxEventPayload = nullptr;	// event-specific payload (e.g. other entity on collision), null otherwise
	const Zenith_GraphEventArg* m_pxEventArgs = nullptr;	// named multi-field payload (FireCustomEventWithArgs), null otherwise
	u_int m_uEventArgCount = 0;

	// The standard entity-targeting convention: a node that acts on an entity
	// declares ZENITH_PROPERTY(std::string, m_strTargetVar, "") and resolves it
	// here - empty var = self; otherwise the blackboard var must hold a packed
	// EntityID. Returns an INVALID entity on missing var / wrong type / dead
	// entity (callers FAILURE their chain). Body in Zenith_BehaviourGraph.cpp
	// (leaf-safe: resolves through Zenith_SceneSystem::Get(), no engine types).
	Zenith_Entity ResolveTargetEntity(const std::string& strTargetVar) const;
};

class Zenith_GraphNode
{
public:
	virtual ~Zenith_GraphNode() {}

	// The node's work. Flow nodes run their sub-chains from inside Execute via
	// m_pxGraph->RunChainFromPin(GetNodeID(), uPin, xContext) and own whatever
	// per-instance memory that needs (remembered branch pin, loop counter...).
	virtual GraphNodeStatus Execute(Zenith_GraphContext& xContext) = 0;

	// Chain lifecycle, invoked by RunChainFromPin. OnEnter fires when a chain
	// walk reaches the node FRESH (a suspended node resuming does NOT re-enter);
	// OnExit fires when Execute completes with SUCCESS or FAILURE (never on
	// RUNNING). Default no-ops.
	virtual void OnEnter(Zenith_GraphContext& /*xContext*/) {}
	virtual void OnExit(Zenith_GraphContext& /*xContext*/) {}

	// Preemption hook: the node was suspended (RUNNING) and its chain has been
	// aborted (reactive Selector/StateMachine switched away, or the owner reset).
	// Reset per-run instance state here (elapsed timers, remembered pins); flow
	// nodes must forward the abort into their active output pins via
	// Zenith_BehaviourGraph::AbortChain. Default no-op.
	virtual void OnAbort(Zenith_GraphContext& /*xContext*/) {}

	// Variable-pin flow nodes (Switch/StateMachine/Selector) override this to
	// report their configured pin count AFTER params are applied; -1 = use the
	// registry's static m_uExecOutputCount. Capped at 255 by the chain-cursor
	// key layout (pin lives in the key's low byte).
	virtual int32_t GetDynamicExecOutputCount() const { return -1; }

	// Canonical registered type name (matches Zenith_GraphNodeRegistry).
	virtual const char* GetTypeName() const = 0;

	// Custom-event sources (GRAPH_EVENT_CUSTOM) override this to compare the
	// fired name against their configured event-name parameter.
	virtual bool MatchesCustomEvent(const char* /*szName*/) const { return false; }

	u_int GetNodeID() const { return m_uNodeID; }

private:
	friend class Zenith_BehaviourGraph;
	u_int m_uNodeID = 0;	// assigned by the owning graph at instantiation
};
