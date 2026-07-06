#pragma once

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Collections/Zenith_Vector.h"
#include "Collections/Zenith_HashMap.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"
#include <string>

//------------------------------------------------------------------------------
// Zenith_GraphDefinition - the serializable description of a behaviour graph
// (what a .bgraph asset stores): variable declarations, node descriptions
// (type name + schema version + length-framed param blob), exec edges, and
// editor layout (always serialized so non-tools round-trips preserve it).
//
// Unresolved-node preservation falls out of the storage model: the definition
// keeps every node's type name, version, and param bytes verbatim whether or
// not the type is registered in this build - a load + resave round-trips
// unknown nodes unchanged. Resolution happens at INSTANTIATION
// (Zenith_BehaviourGraph::InitialiseFromDefinition); chains through an
// unresolved node fail gracefully with a once-per-node report.
//
// Exec-graph rule (enforced by AddEdge): a (node, pin) has at most ONE
// outgoing edge - chains are linear; branching is flow nodes with multiple
// output pins.
//------------------------------------------------------------------------------

struct Zenith_GraphVariableDecl
{
	std::string m_strName;
	Zenith_PropertyValue m_xDefault;
};

struct Zenith_GraphNodeDef
{
	u_int m_uNodeID = 0;	// stable, unique within the graph; 0 = invalid
	std::string m_strTypeName;
	u_int m_uTypeVersion = 1;
	Zenith_DataStream m_xParamBlob;	// property blob (bytes = GetCursor()); preserved verbatim for unresolved types

	Zenith_GraphNodeDef() = default;
	Zenith_GraphNodeDef(Zenith_GraphNodeDef&&) = default;
	Zenith_GraphNodeDef& operator=(Zenith_GraphNodeDef&&) = default;
	Zenith_GraphNodeDef(const Zenith_GraphNodeDef&) = delete;
	Zenith_GraphNodeDef& operator=(const Zenith_GraphNodeDef&) = delete;
};

struct Zenith_GraphEdge
{
	u_int m_uSrcNodeID = 0;
	u_int m_uSrcPin = 0;
	u_int m_uDstNodeID = 0;
	u_int m_uDstPin = 0;
};

class Zenith_GraphDefinition
{
public:
	//--------------------------------------------------------------------------
	// Authoring (editor + code-built graphs/tests)
	//--------------------------------------------------------------------------

	void DeclareVariable(const char* szName, const Zenith_PropertyValue& xDefault);

	// Adds a node of the registered type and returns its ID (0 on failure).
	// Default param values are captured into the blob immediately (via a
	// temporary instance) so a freshly-authored node round-trips stably.
	u_int AddNode(const char* szTypeName);

	// Serializes pxConfigured's current property values into the node's param
	// blob. The instance must be of the node's registered type.
	bool SetNodeParamsFromInstance(u_int uNodeID, const Zenith_GraphNode* pxConfigured);

	bool RemoveNode(u_int uNodeID);	// also removes touching edges

	// Enforces the one-outgoing-edge-per-(src,pin) exec rule and rejects
	// self-loops/unknown endpoints. Returns false (logged) when rejected.
	bool AddEdge(u_int uSrcNodeID, u_int uSrcPin, u_int uDstNodeID, u_int uDstPin);
	bool RemoveEdge(u_int uSrcNodeID, u_int uSrcPin);

	void Clear();

	//--------------------------------------------------------------------------
	// Access
	//--------------------------------------------------------------------------

	u_int GetVariableCount() const { return m_axVariables.GetSize(); }
	const Zenith_GraphVariableDecl& GetVariableAt(u_int uIndex) const { return m_axVariables.Get(uIndex); }

	u_int GetNodeCount() const { return m_axNodes.GetSize(); }
	const Zenith_GraphNodeDef& GetNodeAt(u_int uIndex) const { return m_axNodes.Get(uIndex); }
	const Zenith_GraphNodeDef* FindNodeDef(u_int uNodeID) const;

	u_int GetEdgeCount() const { return m_axEdges.GetSize(); }
	const Zenith_GraphEdge& GetEdgeAt(u_int uIndex) const { return m_axEdges.Get(uIndex); }

	// Mutable variable access (graph editor): null when absent.
	Zenith_GraphVariableDecl* FindVariableMutable(const char* szName);
	bool RemoveVariable(const char* szName);

	// Editor canvas positions - always present (and serialized) so non-tools
	// builds round-trip layout; only the editing UI is tools-gated.
	void SetNodeEditorPos(u_int uNodeID, const Zenith_Maths::Vector2& xPos) { m_xEditorPositions[uNodeID] = xPos; }
	bool GetNodeEditorPos(u_int uNodeID, Zenith_Maths::Vector2& xOut) const;

	//--------------------------------------------------------------------------
	// Serialization (the .bgraph payload)
	//--------------------------------------------------------------------------

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	// Returns false (and leaves the definition cleared) on bad magic /
	// unsupported version / malformed payload.
	bool ReadFromDataStream(Zenith_DataStream& xStream);

	static constexpr u_int uGRAPH_MAGIC = 0x52474258u;	// "XBGR" ('Z' would collide with .zdata families; X = exec-graph)
	static constexpr u_int uGRAPH_VERSION = 1;

private:
	Zenith_Vector<Zenith_GraphVariableDecl> m_axVariables;
	Zenith_Vector<Zenith_GraphNodeDef> m_axNodes;
	Zenith_Vector<Zenith_GraphEdge> m_axEdges;
	Zenith_HashMap<u_int, Zenith_Maths::Vector2> m_xEditorPositions;
	u_int m_uNextNodeID = 1;
};

//------------------------------------------------------------------------------
// Zenith_BehaviourGraph - a live, per-entity-slot instance of a definition.
//
// Owns its node instances (per-instance members ARE per-instance state) and a
// blackboard initialised from the declarations (then overridden per entity by
// Zenith_GraphComponent). Dispatch is strictly main-thread, driven by the
// hosting component's lifecycle hooks.
//
// Chain state: a cursor per (anchor node, pin) records the node a suspended
// (RUNNING) chain resumes at. Chains anchored on one-shot events (OnStart,
// collisions, custom events) that suspend are re-driven by the next OnUpdate
// dispatch until they complete.
//------------------------------------------------------------------------------
class Zenith_BehaviourGraph
{
public:
	Zenith_BehaviourGraph() = default;
	~Zenith_BehaviourGraph();

	Zenith_BehaviourGraph(const Zenith_BehaviourGraph&) = delete;
	Zenith_BehaviourGraph& operator=(const Zenith_BehaviourGraph&) = delete;

	// Builds node instances through Zenith_GraphNodeRegistry, applies param
	// blobs, seeds the blackboard from declarations, and indexes event sources.
	// Returns true even with unresolved nodes (they report + fail their chains);
	// false only on internal inconsistency.
	bool InitialiseFromDefinition(const Zenith_GraphDefinition& xDefinition);
	void Shutdown();

	Zenith_GraphBlackboard& GetBlackboard() { return m_xBlackboard; }
	const Zenith_GraphBlackboard& GetBlackboard() const { return m_xBlackboard; }

	bool HasEventSource(GraphEventType eEvent) const;

	// True when an ON_UPDATE dispatch would actually do work: OnUpdate/Timer
	// sources, suspended one-shot chains, or live chain cursors. The hosting
	// component skips idle graphs entirely - graphs anchored only on
	// collisions/custom events cost ~nothing per frame.
	bool NeedsUpdateDispatch() const;

	// Runs every source of eEvent (sources gate themselves via Execute - e.g.
	// Timer returns SUCCESS only when its interval elapses). ON_UPDATE dispatch
	// additionally resumes suspended one-shot chains.
	void FireEvent(GraphEventType eEvent, Zenith_GraphContext& xContext);
	void FireCustomEvent(const char* szName, Zenith_GraphContext& xContext);

	// Runs (or resumes) the chain hanging off (uNodeID, uPin). Public for flow
	// nodes (Branch/Loop) to drive their output sub-chains from inside Execute.
	GraphNodeStatus RunChainFromPin(u_int uNodeID, u_int uPin, Zenith_GraphContext& xContext);

	// Aborts the suspended chain hanging off (uNodeID, uPin), if any: calls
	// OnAbort on the cursor node (flow nodes forward the abort into their own
	// active pins from there) and clears the cursor + any matching suspended
	// one-shot anchor. No-op when nothing is suspended. The preemption
	// primitive behind reactive Selector / StateMachine transitions.
	void AbortChain(u_int uNodeID, u_int uPin, Zenith_GraphContext& xContext);

	// Aborts EVERY suspended chain (OnAbort per cursor node) - CallGraph
	// teardown / preemption of a whole child graph.
	void AbortAllChains(Zenith_GraphContext& xContext);

	// Runs every ON_GRAPH_CALL entry anchor (the callable-sub-graph protocol
	// behind the CallGraph node): RUNNING if any chain suspended (the next call
	// resumes it), FAILURE when anchors exist but every chain failed, SUCCESS
	// otherwise (including a graph with no anchors - a no-op call).
	GraphNodeStatus RunGraphCall(Zenith_GraphContext& xContext);

	Zenith_GraphNode* FindNode(u_int uNodeID);
	u_int GetNodeCount() const { return m_axNodes.GetSize(); }
	u_int GetUnresolvedCount() const { return m_uUnresolvedCount; }

	// Drops all chain cursors + suspended anchors (play-mode restarts).
	void ResetChainState();

	// The node currently inside Execute (0 outside dispatch) - editor live
	// execution highlighting.
	u_int GetExecutingNodeID() const { return m_uExecutingNodeID; }

	// Node IDs executed since the start of the last ON_UPDATE dispatch (capped)
	// - drives the editor's live execution highlighting.
	const Zenith_Vector<u_int>& GetRecentlyExecuted() const { return m_auRecentlyExecuted; }

private:
	struct NodeInstance
	{
		u_int m_uNodeID = 0;
		Zenith_GraphNode* m_pxNode = nullptr;	// null = unresolved in this build
		const Zenith_GraphNodeTypeInfo* m_pxTypeInfo = nullptr;
		bool m_bWarnedUnresolved = false;
	};

	NodeInstance* FindInstance(u_int uNodeID);
	u_int FindSuccessor(u_int uNodeID, u_int uPin) const;	// 0 = none
	static u_int64 MakeChainKey(u_int uNodeID, u_int uPin) { return (static_cast<u_int64>(uNodeID) << 8) | static_cast<u_int64>(uPin & 0xFFu); }
	void RunSourceNode(NodeInstance& xSource, Zenith_GraphContext& xContext);

	Zenith_Vector<NodeInstance> m_axNodes;
	Zenith_Vector<Zenith_GraphEdge> m_axEdges;
	Zenith_HashMap<u_int64, u_int> m_xChainCursors;			// (anchor,pin) -> resume node
	Zenith_Vector<u_int> m_auEventSources[GRAPH_EVENT_COUNT];
	Zenith_Vector<u_int> m_auSuspendedOneShotAnchors;		// resumed on ON_UPDATE dispatch
	Zenith_GraphBlackboard m_xBlackboard;
	Zenith_Vector<u_int> m_auRecentlyExecuted;
	u_int m_uUnresolvedCount = 0;
	u_int m_uExecutingNodeID = 0;
};
