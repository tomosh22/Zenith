#pragma once

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphPinTable.h"
#include "Collections/Zenith_Vector.h"
#include <concepts>
#include <string>

class Zenith_GraphDefinition;

//------------------------------------------------------------------------------
// Zenith_GraphNodeRegistry - the type registry for Behaviour Graph nodes.
//
// Mirrors the Zenith_ComponentMetaRegistry inversion: this runtime names NO
// concrete node type. The engine installs its node set via
// SetNodeRegistrar(&Zenith_RegisterEngineGraphNodes) from
// Zenith_Engine::Initialise (the registrar lives in the EntityComponent glue
// layer, where naming concrete components is legal); games add their own node
// types from their project hooks. EnsureInitialized() drains the registrar on
// first use.
//
// Node parameters are reflected through the Phase 0 property system: a node
// class that declares ZENITH_PROPERTIES_BEGIN/ZENITH_PROPERTY gets its param
// table picked up automatically (serialization + editor panel for free).
//------------------------------------------------------------------------------

typedef Zenith_GraphNode* (*Zenith_GraphNodeCreateFn)();
typedef const Zenith_PropertyTable* (*Zenith_GraphNodeTableFn)();
typedef const Zenith_GraphPinTable* (*Zenith_GraphNodePinTableFn)();

// Concept: does the node class expose a Phase 0 property table?
template<typename T>
concept HasGraphNodeProperties = requires { { T::GetPropertyTableStatic() } -> std::same_as<Zenith_PropertyTable&>; };

// Concept: does the node class expose a PIN DESCRIPTOR table
// (ZENITH_GRAPH_PINS_BEGIN/END - see Zenith_GraphPinTable.h)? Written exactly
// like HasGraphNodeProperties, including matching the return type EXACTLY, so
// inheritance behaves the same way: a derived class with no block of its own
// resolves to - and shares - its base's table.
template<typename T>
concept HasGraphNodePins = requires { { T::GetPinTableStatic() } -> std::same_as<const Zenith_GraphPinTable&>; };

// The paired tag the PINS_BEGIN macro emits. A class that carries the tag but
// whose table is not DETECTABLE (private, or hand-rolled with the wrong return
// type) would register as an OPAQUE node - carefully annotated and silently
// unvalidated. RegisterNodeType turns exactly that into a compile error.
template<typename T>
concept HasGraphNodePinTableTag = requires { { T::bZENITH_HAS_PIN_TABLE } -> std::convertible_to<bool>; };

// Concept: does the node class pin an on-disk schema version?
// (static constexpr u_int uTYPE_VERSION = N;) Default 1 when absent. Bump it
// when the node's param schema changes meaning - the name-matched property
// serialization already tolerates added/removed/reordered params.
template<typename T>
concept HasGraphNodeTypeVersion = requires { { T::uTYPE_VERSION } -> std::convertible_to<u_int>; };

struct Zenith_GraphNodeTypeInfo
{
	std::string m_strTypeName;
	u_int m_uTypeVersion = 1;
	GraphEventType m_eEventType = GRAPH_EVENT_NONE;	// != NONE for event-source nodes
	u_int m_uExecOutputCount = 1;					// number of output exec pins
	bool m_bFlowNode = false;						// true = runs its output sub-chains from inside Execute (Branch/Loop); false = chain auto-continues via pin 0 on SUCCESS
	// Routable FAILURE: when true the type owns ONE extra exec output at index
	// m_uExecOutputCount ("On Failure"). Unwired it changes nothing (the chain
	// still aborts); wired, RunChainFromPin continues down that edge under the
	// same chain key. Registration REFUSES the flag - Zenith_Error + forced
	// false - on a flow node (whose FAILURE is its sub-chain's propagated
	// status), on a dynamic-pin type (the index would move with the branch
	// count), and at m_uExecOutputCount >= 255 (the chain-cursor key packs the
	// pin into its low byte).
	bool m_bHasFailurePin = false;
	Zenith_GraphNodeCreateFn m_pfnCreate = nullptr;
	Zenith_GraphNodeTableFn m_pfnGetPropertyTable = nullptr;	// null = parameterless node
	// null = the type declares NO pin table and is therefore OPAQUE to
	// Zenith_GraphDefinitionValidator: it contributes no writer and performs no
	// read that any check can see. Annotating the node library is a separate
	// unit; an un-annotated node must never produce a false finding.
	Zenith_GraphNodePinTableFn m_pfnGetPinTable = nullptr;
#ifdef ZENITH_TOOLS
	std::string m_strCategory;		// editor palette grouping ("Flow", "Transform", "Debug", ...)
#endif
};

class Zenith_GraphNodeRegistry
{
public:
	static Zenith_GraphNodeRegistry& Get();

	void Register(const Zenith_GraphNodeTypeInfo& xInfo);

	// Type-safe registration helper: derives create fn, property table, and
	// type version from the class. szCategory is editor metadata (ignored in
	// non-tools builds). bHasFailurePin opts the type into the routable
	// "On Failure" exec pin (see Zenith_GraphNodeTypeInfo::m_bHasFailurePin);
	// Register() validates it and refuses it observably.
	template<typename T>
	void RegisterNodeType(const char* szTypeName, GraphEventType eEventType, u_int uExecOutputCount,
		bool bFlowNode, const char* szCategory, bool bHasFailurePin = false)
	{
		Zenith_GraphNodeTypeInfo xInfo;
		xInfo.m_strTypeName = szTypeName;
		xInfo.m_eEventType = eEventType;
		xInfo.m_uExecOutputCount = uExecOutputCount;
		xInfo.m_bFlowNode = bFlowNode;
		xInfo.m_bHasFailurePin = bHasFailurePin;
		xInfo.m_pfnCreate = +[]() -> Zenith_GraphNode* { return new T(); };
		if constexpr (HasGraphNodeProperties<T>)
		{
			xInfo.m_pfnGetPropertyTable = +[]() -> const Zenith_PropertyTable* { return &T::GetPropertyTableStatic(); };
		}
		if constexpr (HasGraphNodePins<T>)
		{
			xInfo.m_pfnGetPinTable = +[]() -> const Zenith_GraphPinTable* { return &T::GetPinTableStatic(); };
		}
		else
		{
			static_assert(!HasGraphNodePinTableTag<T>,
				"Node type carries bZENITH_HAS_PIN_TABLE but its pin table is not detectable - "
				"GetPinTableStatic() must be PUBLIC and return const Zenith_GraphPinTable&. "
				"A private one would register the node as OPAQUE and silently skip every check.");
		}
		if constexpr (HasGraphNodeTypeVersion<T>)
		{
			xInfo.m_uTypeVersion = T::uTYPE_VERSION;
		}
#ifdef ZENITH_TOOLS
		xInfo.m_strCategory = szCategory ? szCategory : "";
#else
		(void)szCategory;
#endif
		Register(xInfo);
	}

	const Zenith_GraphNodeTypeInfo* Find(const char* szTypeName) const;

	u_int GetTypeCount() const;
	const Zenith_GraphNodeTypeInfo& GetTypeAt(u_int uIndex) const;

	// THE effective exec-output count of one node in one definition - the ONE
	// home of that arithmetic, shared by the editor (drawing, hit rects, box
	// height, connect validation) and by the validator's pin-range check, so
	// what is DRAWN and what is ACCEPTED cannot disagree:
	//   - unknown/unresolved type      -> 1 (the input-chaining pin every node has)
	//   - dynamic-pin type             -> a param-applied temp instance's
	//                                     GetDynamicExecOutputCount(), clamped to
	//                                     255 (the chain-cursor key packs the pin
	//                                     into its low byte). NO failure pin: the
	//                                     registry refuses the flag on these.
	//   - static-pin type              -> m_uExecOutputCount + the failure pin
	//                                     when the type carries one.
	// Costs one temp instance per DYNAMIC node per query; editor/authoring scale.
	u_int GetExecOutputCount(const Zenith_GraphDefinition& xDefinition, u_int uNodeID) const;

	// Registrar inversion (the Zenith_ComponentMetaRegistry pattern). The
	// engine installs the glue-layer registrar at boot; EnsureInitialized
	// drains it exactly once before the first registry use.
	void SetNodeRegistrar(void (*pfnRegistrar)());
	void EnsureInitialized();
	bool IsInitialized() const { return m_bInitialized; }

	// Test support: tears down all registered types + the initialized flag so a
	// test can install a scratch set. Engine code never calls this.
	void ResetForTests();

private:
	Zenith_Vector<Zenith_GraphNodeTypeInfo> m_axTypes;
	void (*m_pfnRegistrar)() = nullptr;
	bool m_bInitialized = false;
};
