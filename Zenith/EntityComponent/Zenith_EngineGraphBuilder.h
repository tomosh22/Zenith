#pragma once

#include "Scripting/Zenith_GraphBuilder.h"
#include "Scripting/Zenith_GraphChain.h"
#include "EntityComponent/Zenith_GraphOps.h"

//------------------------------------------------------------------------------
// Zenith_EngineGraphBuilder - a curated fluent DSL over Zenith_GraphBuilder for
// the ENGINE Behaviour Graph node library. It knows engine node-type name
// strings and the op enums, so it lives EntityComponent-side (keeping the
// Scripting builder itself leaf-safe). Wrap the plain builder a BuildGraph_*
// function receives and author through it:
//
//   Zenith_EngineGraphBuilder xB(xBuilder);
//   const u_int uPre = xB.Node("CombatPlayerPreTick");        // raw (game node)
//   const u_int uSM  = xB.StateMachine({ uState, "Value" }, 9, "...");
//   xB.OnCustomEvent("PlayerTick").Then(uPre).Then(uSM);      // fluent spine
//
// Each factory creates a node + its DEFINING params and returns a
// Zenith_GraphChain (implicitly the node id; supports .Then()/.ThenPin()).
//
// EXACT-DEFAULT RULE: an omitted optional argument leaves the node's own
// ZENITH_PROPERTY default untouched - the factory sets a param ONLY when the
// caller passes a value. This is what makes factory authoring byte-identical to
// the hand-written Node()+Param* it replaces (e.g. OnCustomEvent's
// m_strStorePayloadVar defaults to "payload"; passing nullptr must NOT emit "").
//
// Raw primitives (Node/Param*/Chain/Edge/Variable/Build) are forwarded verbatim
// for the nodes the curated set does not cover (game nodes, var-vs-var compares,
// uncommon engine nodes).
//------------------------------------------------------------------------------
struct Zenith_GraphWireSource
{
	u_int uNode;
	const char* szPin;
};

class Zenith_EngineGraphBuilder
{
public:
	explicit Zenith_EngineGraphBuilder(Zenith_GraphBuilder& xBuilder)
		: m_xBuilder(xBuilder)
	{
	}

	Zenith_GraphBuilder& Raw() { return m_xBuilder; }

	// --- raw primitive pass-throughs -----------------------------------------
	Zenith_GraphBuilder& Variable(const char* szName, const Zenith_PropertyValue& xDefault) { return m_xBuilder.Variable(szName, xDefault); }
	u_int Node(const char* szTypeName) { return m_xBuilder.Node(szTypeName); }
	Zenith_GraphBuilder& Param(u_int uNodeID, const char* szProperty, const Zenith_PropertyValue& xValue) { return m_xBuilder.Param(uNodeID, szProperty, xValue); }
	Zenith_GraphBuilder& ParamFloat(u_int uNodeID, const char* szProperty, float fValue) { return m_xBuilder.ParamFloat(uNodeID, szProperty, fValue); }
	Zenith_GraphBuilder& ParamInt(u_int uNodeID, const char* szProperty, int32_t iValue) { return m_xBuilder.ParamInt(uNodeID, szProperty, iValue); }
	Zenith_GraphBuilder& ParamBool(u_int uNodeID, const char* szProperty, bool bValue) { return m_xBuilder.ParamBool(uNodeID, szProperty, bValue); }
	Zenith_GraphBuilder& ParamString(u_int uNodeID, const char* szProperty, const char* szValue) { return m_xBuilder.ParamString(uNodeID, szProperty, szValue); }
	Zenith_GraphBuilder& ParamVec3(u_int uNodeID, const char* szProperty, const Zenith_Maths::Vector3& xValue) { return m_xBuilder.ParamVec3(uNodeID, szProperty, xValue); }
	template<typename TEnum>
	Zenith_GraphBuilder& ParamEnum(u_int uNodeID, const char* szProperty, TEnum eValue) { return m_xBuilder.ParamEnum(uNodeID, szProperty, eValue); }
	Zenith_GraphBuilder& Edge(u_int uSrcNodeID, u_int uSrcPin, u_int uDstNodeID) { return m_xBuilder.Edge(uSrcNodeID, uSrcPin, uDstNodeID); }
	Zenith_GraphBuilder& DataEdge(u_int uSrcNodeID, const char* szOutPin, u_int uDstNodeID, const char* szInPin) { return m_xBuilder.DataEdge(uSrcNodeID, szOutPin, uDstNodeID, szInPin); }
	Zenith_GraphBuilder& Chain(u_int uFrom, u_int uTo) { return m_xBuilder.Chain(uFrom, uTo); }
	bool Build() { return m_xBuilder.Build(); }
	bool HasErrors() const { return m_xBuilder.HasErrors(); }

	// --- event source factories ----------------------------------------------
	Zenith_GraphChain OnUpdate() { return Anchor(m_xBuilder.Node("OnUpdate")); }
	Zenith_GraphChain OnStart() { return Anchor(m_xBuilder.Node("OnStart")); }

	// szStorePayloadVar omitted -> keep the node default ("payload").
	Zenith_GraphChain OnCustomEvent(const char* szEventName, const char* szStorePayloadVar = nullptr)
	{
		const u_int uNode = m_xBuilder.Node("OnCustomEvent");
		m_xBuilder.ParamString(uNode, "m_strEventName", szEventName);
		if (szStorePayloadVar) { m_xBuilder.ParamString(uNode, "m_strStorePayloadVar", szStorePayloadVar); }
		return Anchor(uNode);
	}

	Zenith_GraphChain OnKeyPressed(int32_t iKeyCode)
	{
		const u_int uNode = m_xBuilder.Node("OnKeyPressed");
		m_xBuilder.ParamInt(uNode, "m_iKeyCode", iKeyCode);
		return Anchor(uNode);
	}

	// --- action layer (B10) ---------------------------------------------------
	// szAction is an ACTION NAME as the game registered it (its Bindings header
	// owns those strings), NOT a device code. This is the form a boot-authored
	// graph should reach for: the same chain then answers to a key, a pad
	// button or an on-screen control with no edit.
	Zenith_GraphChain OnActionPressed(const char* szAction)  { return ActionSource("OnActionPressed", szAction); }
	Zenith_GraphChain OnActionReleased(const char* szAction) { return ActionSource("OnActionReleased", szAction); }
	Zenith_GraphChain OnActionHeld(const char* szAction)     { return ActionSource("OnActionHeld", szAction); }

	Zenith_GraphChain ReadActionAxis1D(const char* szAction) { return ActionRead("ReadActionAxis1D", szAction); }
	Zenith_GraphChain ReadActionAxis2D(const char* szAction) { return ActionRead("ReadActionAxis2D", szAction); }

	// --- flow ----------------------------------------------------------------
	Zenith_GraphChain Branch(Zenith_GraphWireSource xCondition)
	{
		const u_int uNode = m_xBuilder.Node("Branch");
		m_xBuilder.DataEdge(xCondition.uNode, xCondition.szPin, uNode, "Condition");
		return Anchor(uNode);
	}

	Zenith_GraphChain Gate(Zenith_GraphWireSource xOpen)
	{
		const u_int uNode = m_xBuilder.Node("Gate");
		m_xBuilder.DataEdge(xOpen.uNode, xOpen.szPin, uNode, "Open");
		return Anchor(uNode);
	}

	Zenith_GraphChain SwitchOnInt(Zenith_GraphWireSource xValue, int32_t iCaseCount)
	{
		const u_int uNode = m_xBuilder.Node("SwitchOnInt");
		m_xBuilder.ParamInt(uNode, "m_iCaseCount", iCaseCount);
		m_xBuilder.DataEdge(xValue.uNode, xValue.szPin, uNode, "Value");
		return Anchor(uNode);
	}

	Zenith_GraphChain StateMachine(Zenith_GraphWireSource xState, int32_t iStateCount, const char* szStateNames)
	{
		const u_int uNode = m_xBuilder.Node("StateMachine");
		m_xBuilder.ParamInt(uNode, "m_iStateCount", iStateCount);
		m_xBuilder.ParamString(uNode, "m_strStateNames", szStateNames);
		m_xBuilder.DataEdge(xState.uNode, xState.szPin, uNode, "State");
		return Anchor(uNode);
	}

	// --- blackboard compares (constant and wire forms) --------------------------
	Zenith_GraphChain CompareFloat(Zenith_GraphWireSource xValue, Zenith_GraphCompareFloatOp eOp, float fCompareTo)
	{
		const u_int uNode = m_xBuilder.Node("CompareBlackboardFloat");
		m_xBuilder.ParamFloat(uNode, "m_fCompareTo", fCompareTo);
		m_xBuilder.ParamEnum(uNode, "m_iOp", eOp);
		m_xBuilder.DataEdge(xValue.uNode, xValue.szPin, uNode, "Value");
		return Anchor(uNode);
	}
	Zenith_GraphChain CompareFloat(Zenith_GraphWireSource xValue, Zenith_GraphCompareFloatOp eOp, Zenith_GraphWireSource xCompareTo)
	{
		const u_int uNode = m_xBuilder.Node("CompareBlackboardFloat");
		m_xBuilder.ParamEnum(uNode, "m_iOp", eOp);
		m_xBuilder.DataEdge(xValue.uNode, xValue.szPin, uNode, "Value");
		m_xBuilder.DataEdge(xCompareTo.uNode, xCompareTo.szPin, uNode, "CompareTo");
		return Anchor(uNode);
	}

	Zenith_GraphChain CompareInt(Zenith_GraphWireSource xValue, Zenith_GraphCompareIntOp eOp, int32_t iCompareTo)
	{
		const u_int uNode = m_xBuilder.Node("CompareBlackboardInt");
		m_xBuilder.ParamInt(uNode, "m_iCompareTo", iCompareTo);
		m_xBuilder.ParamEnum(uNode, "m_iOp", eOp);
		m_xBuilder.DataEdge(xValue.uNode, xValue.szPin, uNode, "Value");
		return Anchor(uNode);
	}
	Zenith_GraphChain CompareInt(Zenith_GraphWireSource xValue, Zenith_GraphCompareIntOp eOp, Zenith_GraphWireSource xCompareTo)
	{
		const u_int uNode = m_xBuilder.Node("CompareBlackboardInt");
		m_xBuilder.ParamEnum(uNode, "m_iOp", eOp);
		m_xBuilder.DataEdge(xValue.uNode, xValue.szPin, uNode, "Value");
		m_xBuilder.DataEdge(xCompareTo.uNode, xCompareTo.szPin, uNode, "CompareTo");
		return Anchor(uNode);
	}

	// --- blackboard set -------------------------------------------------------
	Zenith_GraphChain SetBlackboardInt(const char* szVariable, int32_t iValue)
	{
		const u_int uNode = m_xBuilder.Node("SetBlackboardInt");
		m_xBuilder.ParamString(uNode, "m_strVariable", szVariable);
		m_xBuilder.ParamInt(uNode, "m_iValue", iValue);
		return Anchor(uNode);
	}

	Zenith_GraphChain SetBlackboardFloat(const char* szVariable, float fValue)
	{
		const u_int uNode = m_xBuilder.Node("SetBlackboardFloat");
		m_xBuilder.ParamString(uNode, "m_strVariable", szVariable);
		m_xBuilder.ParamFloat(uNode, "m_fValue", fValue);
		return Anchor(uNode);
	}

	Zenith_GraphChain SetBlackboardBool(const char* szVariable, bool bValue)
	{
		const u_int uNode = m_xBuilder.Node("SetBlackboardBool");
		m_xBuilder.ParamString(uNode, "m_strVariable", szVariable);
		m_xBuilder.ParamBool(uNode, "m_bValue", bValue);
		return Anchor(uNode);
	}
	Zenith_GraphChain SetBlackboardEntityID(const char* szVariable, Zenith_GraphWireSource xValue)
	{
		const u_int uNode = m_xBuilder.Node("SetBlackboardEntityID");
		m_xBuilder.ParamString(uNode, "m_strVariable", szVariable);
		m_xBuilder.DataEdge(xValue.uNode, xValue.szPin, uNode, "Value");
		return Anchor(uNode);
	}

	// --- blackboard logic -----------------------------------------------------
	// szVars is a COMMA-SEPARATED operand list, verbatim (no trimming - see
	// Zenith_GraphNode_ParseCommaList). LogicBool deliberately always serializes
	// m_bInvert and m_bMissingIsTrue, including their false defaults. This is the
	// recorded exception to exact-default omission: matching raw definitions must
	// include both bool parameters.
	// --- blackboard lists -----------------------------------------------------
	// Read (GetListCount / GetListElement / ForEach) and write (ListAdd /
	// ListRemoveAt / ListClear). None of these had a DSL helper before, so a
	// list-walking graph had to drop to raw Node()+Param* for every one.
	Zenith_GraphChain GetListCount(const char* szListVar)
	{
		const u_int uNode = m_xBuilder.Node("GetListCount");
		m_xBuilder.ParamString(uNode, "m_strListVar", szListVar);
		return Anchor(uNode);
	}
	// Result is consumed from the OUTPUT pin in wire-authored graphs.
	Zenith_GraphChain LogicBool(
		const char* szVars,
		Zenith_GraphLogicBoolOp eOp,
		bool bInvert = false,
		bool bMissingIsTrue = false)
	{
		const u_int uNode = m_xBuilder.Node("LogicBlackboardBool");
		m_xBuilder.ParamString(uNode, "m_strVars", szVars);
		m_xBuilder.ParamEnum(uNode, "m_iOp", eOp);
		m_xBuilder.ParamBool(uNode, "m_bInvert", bInvert);
		m_xBuilder.ParamBool(uNode, "m_bMissingIsTrue", bMissingIsTrue);
		return Anchor(uNode);
	}

	// The list and default index are permanent parameters; Result is wire-only.
	Zenith_GraphChain GetListElement(const char* szListVar, int32_t iIndex)
	{
		const u_int uNode = m_xBuilder.Node("GetListElement");
		m_xBuilder.ParamString(uNode, "m_strListVar", szListVar);
		m_xBuilder.ParamInt(uNode, "m_iIndex", iIndex);
		return Anchor(uNode);
	}
	Zenith_GraphChain GetListElement(const char* szListVar, int32_t iIndex, Zenith_GraphWireSource xIndex)
	{
		const u_int uNode = m_xBuilder.Node("GetListElement");
		m_xBuilder.ParamString(uNode, "m_strListVar", szListVar);
		m_xBuilder.ParamInt(uNode, "m_iIndex", iIndex);
		m_xBuilder.DataEdge(xIndex.uNode, xIndex.szPin, uNode, "Index");
		return Anchor(uNode);
	}

	// szIndexVar omitted -> keep the node default ("" = no index written).
	Zenith_GraphChain ForEach(const char* szListVar, const char* szElementVar, const char* szIndexVar = nullptr)
	{
		const u_int uNode = m_xBuilder.Node("ForEach");
		m_xBuilder.ParamString(uNode, "m_strListVar", szListVar);
		m_xBuilder.ParamString(uNode, "m_strElementVar", szElementVar);
		if (szIndexVar) { m_xBuilder.ParamString(uNode, "m_strIndexVar", szIndexVar); }
		return Anchor(uNode);
	}

	Zenith_GraphChain ListAdd(const char* szListVar, Zenith_GraphWireSource xValue)
	{
		const u_int uNode = m_xBuilder.Node("ListAdd");
		m_xBuilder.ParamString(uNode, "m_strListVar", szListVar);
		m_xBuilder.DataEdge(xValue.uNode, xValue.szPin, uNode, "Value");
		return Anchor(uNode);
	}

	Zenith_GraphChain ListRemoveAt(const char* szListVar, int32_t iIndex)
	{
		const u_int uNode = m_xBuilder.Node("ListRemoveAt");
		m_xBuilder.ParamString(uNode, "m_strListVar", szListVar);
		m_xBuilder.ParamInt(uNode, "m_iIndex", iIndex);
		return Anchor(uNode);
	}
	Zenith_GraphChain ListRemoveAt(const char* szListVar, int32_t iIndex, Zenith_GraphWireSource xIndex)
	{
		const u_int uNode = m_xBuilder.Node("ListRemoveAt");
		m_xBuilder.ParamString(uNode, "m_strListVar", szListVar);
		m_xBuilder.ParamInt(uNode, "m_iIndex", iIndex);
		m_xBuilder.DataEdge(xIndex.uNode, xIndex.szPin, uNode, "Index");
		return Anchor(uNode);
	}

	Zenith_GraphChain ListClear(const char* szListVar)
	{
		const u_int uNode = m_xBuilder.Node("ListClear");
		m_xBuilder.ParamString(uNode, "m_strListVar", szListVar);
		return Anchor(uNode);
	}

	// --- events out -----------------------------------------------------------
	// szTargetVar omitted -> node default ("" = self).
	Zenith_GraphChain FireCustomEvent(const char* szEventName, const char* szTargetVar = nullptr)
	{
		const u_int uNode = m_xBuilder.Node("FireCustomEvent");
		m_xBuilder.ParamString(uNode, "m_strEventName", szEventName);
		if (szTargetVar) { m_xBuilder.ParamString(uNode, "m_strTargetVar", szTargetVar); }
		return Anchor(uNode);
	}
	Zenith_GraphChain FireCustomEvent(const char* szEventName, const char* szTargetVar, Zenith_GraphWireSource xPayload)
	{
		const u_int uNode = m_xBuilder.Node("FireCustomEvent");
		m_xBuilder.ParamString(uNode, "m_strEventName", szEventName);
		if (szTargetVar) { m_xBuilder.ParamString(uNode, "m_strTargetVar", szTargetVar); }
		m_xBuilder.DataEdge(xPayload.uNode, xPayload.szPin, uNode, "Payload");
		return Anchor(uNode);
	}

private:
	Zenith_GraphChain Anchor(u_int uNodeID) { return Zenith_GraphChain(m_xBuilder, uNodeID); }

	Zenith_GraphChain ActionSource(const char* szTypeName, const char* szAction)
	{
		const u_int uNode = m_xBuilder.Node(szTypeName);
		m_xBuilder.ParamString(uNode, "m_strAction", szAction);
		return Anchor(uNode);
	}

	Zenith_GraphChain ActionRead(const char* szTypeName, const char* szAction)
	{
		const u_int uNode = m_xBuilder.Node(szTypeName);
		m_xBuilder.ParamString(uNode, "m_strAction", szAction);
		return Anchor(uNode);
	}

	Zenith_GraphBuilder& m_xBuilder;
};
