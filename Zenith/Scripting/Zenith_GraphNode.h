#pragma once

#include "Core/Zenith_PropertySystem.h"
#include "Scripting/Zenith_GraphPinTable.h"
#include "Collections/Zenith_Vector.h"
#include "ZenithECS/Zenith_Entity.h"
#include <string>

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
class Zenith_GraphDefinition;

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

	// True while a RESUME drive is in progress: the drive reached its chain
	// through a cursor (a suspended node is being re-executed) AND the anchor is
	// not one of the two periodic ones. A fresh fire nested inside a resume drive
	// (a node firing a custom event on the same graph) inherits it - harmless
	// for the only consumer, since a freshly-entered node gets OnEnter.
	// A suspended flow node is re-executed without OnEnter, so a fan-out node
	// (Sequence) cannot otherwise tell "resume the branch that is still running"
	// from "fire every branch again". Set - and restored - in exactly two places
	// (Zenith_BehaviourGraph.cpp): RunSourceNode's cursor branch and FireEvent's
	// one-shot re-drive loop. false is not a compatibility default: false IS the
	// OnUpdate/OnFixedUpdate semantics (every tick re-fires every branch).
	bool m_bResumeDrive = false;

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

	// Instance-resolved PIN TYPE hook (Zenith_GraphPinTable). A node whose
	// declared pin carries m_bInstanceResolved answers here - e.g. a maths node
	// whose op code decides whether its Result is a FLOAT or a VECTOR3. uPinIndex
	// is the index into the class's pin table.
	//
	// Deliberately takes an INDEX rather than a descriptor reference so this
	// header never has to know Zenith_GraphPinTable.h exists. Returning false
	// (the default) means "I cannot say": the validator then treats the pin as
	// ANY and reports one warning naming the type. It never fabricates a type.
	virtual bool GetPinType(u_int /*uPinIndex*/, Zenith_PropertyType& /*eOut*/) const { return false; }

	// Canonical registered type name (matches Zenith_GraphNodeRegistry).
	virtual const char* GetTypeName() const = 0;

	// Custom-event sources (GRAPH_EVENT_CUSTOM) override this to compare the
	// fired name against their configured event-name parameter.
	virtual bool MatchesCustomEvent(const char* /*szName*/) const { return false; }

	u_int GetNodeID() const { return m_uNodeID; }

	// Variadic data INPUTS: a node whose pin table declares a
	// ZENITH_GRAPH_PIN_INPUT_VARIADIC family reports how many members it has
	// AFTER params are applied; -1 = no family (the default). Wires name a member
	// "<family><ordinal>"; Execute reads one with the ordinal overload of
	// GetInput. This is the DATA-pin sibling of GetDynamicExecOutputCount.
	virtual int32_t GetDynamicDataInputCount() const { return -1; }

	// The class's PIN and PROPERTY tables, reachable from a base-class pointer.
	// ZENITH_GRAPH_PINS_BEGIN overrides both; a node with no pin block (an OPAQUE
	// node) keeps these null defaults and therefore never self-binds. They exist
	// for ONE caller - EnsurePinState() below - and are the only way a node that
	// no graph ever resolved can find its own descriptors.
	virtual const Zenith_GraphPinTable* GetPinTableVirtual() const { return nullptr; }
	virtual const Zenith_PropertyTable* GetPropertyTableVirtual() const { return nullptr; }

	//--------------------------------------------------------------------------
	// PIN RUNTIME (B-2). A node reads its declared INPUT pins and latches its
	// declared OUTPUT pins through these, never through the blackboard directly.
	//
	// ★ NOTHING HERE CAN REACH Zenith_Assert. Every accessor bounds-checks
	// itself: an out-of-range pin, a role that is not INPUT (resp. OUTPUT), an
	// OPAQUE node (no pin table, so the arrays are empty), a temp instance that
	// was never resolved, or a context with a null graph/blackboard all yield the
	// pin DEFAULT (GetInput) / false (TryGetInput) / a no-op (SetOutput) plus at
	// most ONE [GraphPin] log line per instance. RenderTest's tennis contract
	// already builds a context with a null graph and calls Execute directly.
	//
	// The templates are thin wrappers over NON-template out-of-line members: this
	// header only forward-declares Zenith_BehaviourGraph and must never name one
	// of its members.
	//
	// ★ LAZY SELF-BINDING (B-6.1), and it is PERMANENT RUNTIME BEHAVIOUR - not a
	// third transitional path. GetInput*/TryGetInput/SetOutput call
	// EnsurePinState() first: a node whose pin state no graph ever built, but whose
	// class DOES declare a pin table, builds it from its own tables once. A
	// directly-constructed node behaves like an unwired graph node: an INPUT
	// reads its current const-property default or typed zero and an OUTPUT
	// latches only its slot. This keeps standalone Execute tests valid.
	//
	// ORDERING RULE: assign every property BEFORE the first Execute on a
	// directly-constructed node. Pin state binds the const-property POINTER and
	// instance-resolved slot TYPE ONCE. The property value is read when the input
	// is accessed, so a test that changes a const default before executing uses it;
	// a test that changes an op
	// code between fires needs a FRESH node.
	//--------------------------------------------------------------------------

	// The pin's value as T. Connected -> the producer's slot (pure producers
	// evaluate on demand); a tag that is not T's yields the default plus ONE
	// warning per (instance, pin). Unconnected -> the pin default.
	// The pin DEFAULT is the const property's current value when the descriptor
	// declares one, else the type's zero.
	template<typename T>
	T GetInput(Zenith_GraphContext& xContext, u_int uPinIndex)
	{
		return GetInput<T>(xContext, uPinIndex, uGRAPH_PIN_NO_ORDINAL);
	}

	// One member of a variadic input family.
	template<typename T>
	T GetInput(Zenith_GraphContext& xContext, u_int uPinIndex, u_int uOrdinal)
	{
		constexpr Zenith_PropertyType eEXPECTED = Zenith_PropertyTraits<T>::eTYPE;
		T xResult = T();
		const Zenith_PropertyValue* pxValue = ResolveInput(xContext, uPinIndex, uOrdinal, eEXPECTED);
		if (pxValue != nullptr)
		{
			Zenith_PropertyTraits<T>::Load(*pxValue, xResult);
			return xResult;
		}
		const Zenith_PropertyValue xDefault = MakePinDefault(uPinIndex, uOrdinal, eEXPECTED);
		Zenith_PropertyTraits<T>::Load(xDefault, xResult);
		return xResult;
	}

	// The packed-EntityID accessor. Zenith_PropertyTraits has no u_int64
	// specialisation (Core stays ECS-agnostic), so this is the non-template form
	// of exactly the same contract.
	u_int64 GetInputPackedEntityID(Zenith_GraphContext& xContext, u_int uPinIndex);

	// The PRESENCE-aware form, for a wildcard (ANY) consumer that owns its own
	// tag check. See the truth table in Scripting/CLAUDE.md: false means "there is
	// no value here", never "the value was the wrong type" - a connected slot
	// whose tag disagrees comes back TRUE with the raw value.
	bool TryGetInput(Zenith_GraphContext& xContext, u_int uPinIndex, const Zenith_PropertyValue*& pxOut);
	bool TryGetInput(Zenith_GraphContext& xContext, u_int uPinIndex, u_int uOrdinal, const Zenith_PropertyValue*& pxOut);

	// Latches the OUTPUT slot. Outputs never write the blackboard implicitly.
	void SetOutput(Zenith_GraphContext& xContext, u_int uPinIndex, const Zenith_PropertyValue& xValue);

	template<typename T>
	void SetOutput(Zenith_GraphContext& xContext, u_int uPinIndex, const T& xValue)
	{
		Zenith_PropertyValue xStamped;
		Zenith_PropertyTraits<T>::Store(xStamped, xValue);
		SetOutput(xContext, uPinIndex, xStamped);
	}

	// The OUTPUT slot's RESOLVED type - static, instance-resolved, or taken from
	// a DECLARED VARIABLE (ZENITH_GRAPH_PIN_OUTPUT_FROM_VARIABLE). Returns
	// eGRAPH_PIN_TYPE_ANY for a pin that is out of range, is not an OUTPUT, or
	// whose slot genuinely is ANY - bounds-checked, never reaching Zenith_Assert,
	// exactly like every other accessor here.
	//
	// A node whose Execute must compare a value's tag to its own pin BEFORE
	// writing reads it (GetVariable: SetOutput would REFUSE a mismatching tag,
	// leaving the stamped zero SET, and the node would then report SUCCESS while
	// the consumer silently read that zero). The validator's resolver-agreement
	// unit reads it too, to prove the runtime slot and
	// Zenith_GraphDefinitionValidator::ResolvePinType answer the same thing.
	Zenith_PropertyType GetOutputPinType(u_int uPinIndex) const;

	//--------------------------------------------------------------------------
	// TEST SEAM. Always compiled (they are tiny) but engine code never calls
	// them: they exist so a unit can drive one node without authoring a producer,
	// and so the once-per-instance warnings have an observable that needs no log
	// scraping. They are NOT a transitional path - nothing here is deleted by C-1.
	//--------------------------------------------------------------------------
	void SetInputForTest(u_int uPinIndex, const Zenith_PropertyValue& xValue);
	void SetInputForTest(u_int uPinIndex, u_int uOrdinal, const Zenith_PropertyValue& xValue);
	const Zenith_PropertyValue* GetOutputForTest(u_int uPinIndex) const;	// null = UNSET
	u_int GetMismatchWarningCountForTest(u_int uPinIndex) const;
	u_int GetOutputMismatchWarningCountForTest(u_int uPinIndex) const;
	u_int GetCycleWarningCountForTest() const { return m_uCycleWarningCount; }
	u_int GetPureStatusWarningCountForTest() const { return m_uPureStatusWarningCount; }
	u_int GetBadAccessWarningCountForTest() const { return m_uBadAccessWarningCount; }

	// "This call addresses the pin itself, not a member of a variadic family."
	static constexpr u_int uGRAPH_PIN_NO_ORDINAL = 0xFFFFFFFFu;

	// Nothing copies or moves a node - instances are always m_pfnCreate() + a raw
	// pointer the graph owns. Deleted so the per-instance pin state below cannot
	// be silently duplicated into a second instance that the graph does not know
	// about.
	Zenith_GraphNode(const Zenith_GraphNode&) = delete;
	Zenith_GraphNode& operator=(const Zenith_GraphNode&) = delete;
	Zenith_GraphNode(Zenith_GraphNode&&) = delete;
	Zenith_GraphNode& operator=(Zenith_GraphNode&&) = delete;
	Zenith_GraphNode() = default;

private:
	friend class Zenith_BehaviourGraph;
	// ApplyNodeParams clears m_bPinStateBuilt on the instance it configures: a
	// property write changes what the pin state WOULD be, and the graph's own build
	// always follows it.
	friend class Zenith_GraphDefinition;

	// One per pin in the class's table, indexed by the pin's TABLE INDEX (chosen
	// over a pinIndex -> bindingIndex map: the accessors already have to bounds-
	// check, and one array sized to the table makes "is this pin an INPUT" a
	// field read rather than a second lookup). Entries for non-INPUT pins are
	// inert. Members of a variadic family live in m_axVariadicInputs instead.
	struct InputBinding
	{
		Zenith_PropertyValue m_xConstScratch;					// refreshed by TryGetInput's const path
		const Zenith_ReflectedProperty* m_pxConstProperty = nullptr;
		u_int m_uSrcNodeID = 0;
		u_int m_uSrcSlot = 0;
		u_int m_uMismatchWarningCount = 0;
		bool m_bConnected = false;
		bool m_bIsInput = false;
	};

	struct VariadicInput
	{
		InputBinding m_xBinding;
		u_int m_uPinIndex = 0;
		u_int m_uOrdinal = 0;
	};

	struct OutputSlot
	{
		Zenith_PropertyValue m_xValue;
		// The slot's RESOLVED type (static, or the instance's GetPinType answer).
		// eGRAPH_PIN_TYPE_ANY = the slot accepts any tag and starts UNSET.
		Zenith_PropertyType m_eDeclaredType = eGRAPH_PIN_TYPE_ANY;
		u_int m_uMismatchWarningCount = 0;
		bool m_bSet = false;									// false = UNSET
		bool m_bIsOutput = false;
	};

	struct TestOverride
	{
		Zenith_PropertyValue m_xValue;
		u_int m_uPinIndex = 0;
		u_int m_uOrdinal = 0;
	};

	// THE ONE BUILDER of per-instance pin state, for BOTH callers:
	// Zenith_BehaviourGraph::BuildPinState (with the definition, the registry's
	// property table and the type's variadic-collision flag) and EnsurePinState
	// (with the virtuals, no definition and no collision). A null definition means
	// "no declarations are visible", so a from-variable OUTPUT slot stays ANY.
	//
	// ★ IT CLEARS the three arrays on entry. It used to only Reserve + PushBack, so
	// a SECOND build would APPEND and leave pins 0..N-1 addressing stale state.
	void BuildPinStateFromTables(const Zenith_GraphPinTable& xPins, const Zenith_PropertyTable* pxProperties,
		const Zenith_GraphDefinition* pxDefinition, bool bVariadicNameCollision);

	// Builds the pin state once if nothing has. Called ONLY from the accessors an
	// Execute uses (GetInput*/TryGetInput/SetOutput) - never from the test seam or
	// the const accessors, because the latch reads property-derived state and a
	// SetInputForTest before an op-code assignment would stamp the wrong slot type.
	void EnsurePinState();

	// Out-of-line, non-template, defined in Zenith_BehaviourGraph.cpp (the only
	// TU that may name Zenith_BehaviourGraph's members).
	// null = "there is no extracted value; use MakePinDefault".
	const Zenith_PropertyValue* ResolveInput(Zenith_GraphContext& xContext, u_int uPinIndex, u_int uOrdinal,
		Zenith_PropertyType eExpected);
	Zenith_PropertyValue MakePinDefault(u_int uPinIndex, u_int uOrdinal, Zenith_PropertyType eExpected) const;
	InputBinding* FindInputBinding(u_int uPinIndex, u_int uOrdinal);
	const InputBinding* FindInputBinding(u_int uPinIndex, u_int uOrdinal) const;
	const Zenith_PropertyValue* FindTestOverride(u_int uPinIndex, u_int uOrdinal) const;
	const Zenith_PropertyValue* CheckedExtract(InputBinding& xBinding, const Zenith_PropertyValue& xValue,
		Zenith_PropertyType eExpected, u_int uPinIndex);
	void WarnBadAccess(u_int uPinIndex);

	u_int m_uNodeID = 0;	// assigned by the owning graph at instantiation

	// ★ Every one of these is constructed at capacity ZERO on purpose: the
	// default Zenith_Vector constructor HEAP-ALLOCATES eight elements, and temp
	// instances are built constantly (the registry's dynamic-pin probe,
	// GetExecOutputCount, the validator, the builder, AddNode, the editor's param
	// panel). An unresolved node must cost nothing.
	Zenith_Vector<InputBinding> m_axInputs{ 0u };			// sized to the pin table at resolution
	Zenith_Vector<OutputSlot> m_axOutputs{ 0u };			// sized to the pin table at resolution
	Zenith_Vector<VariadicInput> m_axVariadicInputs{ 0u };	// empty unless a family is declared
	Zenith_Vector<TestOverride> m_axTestOverrides{ 0u };		// empty unless SetInputForTest is called

	// Pure nodes: the gather token this slot was computed for (0 = never).
	// 64-BIT DELIBERATELY: the memo hits on "stamp >= current", so a 32-bit
	// counter wrapping after 2^32 Executes would leave a pure node pinned to a
	// stale memo FOREVER rather than merely re-evaluating once.
	u_int64 m_ulMemoGather = 0;
	u_int m_uCycleWarningCount = 0;
	u_int m_uPureStatusWarningCount = 0;
	u_int m_uBadAccessWarningCount = 0;
	bool m_bEvaluating = false;			// pure nodes: re-entry flag (a runtime data cycle)
	bool m_bMemoValid = false;
	// "Some build ran." One bool test per accessor call on a graph-resolved node -
	// the graph path sets it too, so self-binding costs a resolved node nothing.
	bool m_bPinStateBuilt = false;
};
