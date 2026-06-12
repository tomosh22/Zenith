#pragma once

#include "Scripting/Zenith_GraphNode.h"
#include "Collections/Zenith_Vector.h"
#include <concepts>
#include <string>

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

// Concept: does the node class expose a Phase 0 property table?
template<typename T>
concept HasGraphNodeProperties = requires { { T::GetPropertyTableStatic() } -> std::same_as<Zenith_PropertyTable&>; };

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
	Zenith_GraphNodeCreateFn m_pfnCreate = nullptr;
	Zenith_GraphNodeTableFn m_pfnGetPropertyTable = nullptr;	// null = parameterless node
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
	// non-tools builds).
	template<typename T>
	void RegisterNodeType(const char* szTypeName, GraphEventType eEventType, u_int uExecOutputCount,
		bool bFlowNode, const char* szCategory)
	{
		Zenith_GraphNodeTypeInfo xInfo;
		xInfo.m_strTypeName = szTypeName;
		xInfo.m_eEventType = eEventType;
		xInfo.m_uExecOutputCount = uExecOutputCount;
		xInfo.m_bFlowNode = bFlowNode;
		xInfo.m_pfnCreate = +[]() -> Zenith_GraphNode* { return new T(); };
		if constexpr (HasGraphNodeProperties<T>)
		{
			xInfo.m_pfnGetPropertyTable = +[]() -> const Zenith_PropertyTable* { return &T::GetPropertyTableStatic(); };
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
