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
//   const u_int uSM  = xB.StateMachine("playerState", 9, "...");
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

	// --- flow ----------------------------------------------------------------
	Zenith_GraphChain Branch(const char* szConditionVar)
	{
		const u_int uNode = m_xBuilder.Node("Branch");
		m_xBuilder.ParamString(uNode, "m_strConditionVar", szConditionVar);
		return Anchor(uNode);
	}

	Zenith_GraphChain Gate(const char* szOpenVar)
	{
		const u_int uNode = m_xBuilder.Node("Gate");
		m_xBuilder.ParamString(uNode, "m_strOpenVar", szOpenVar);
		return Anchor(uNode);
	}

	Zenith_GraphChain SwitchOnInt(const char* szVar, int32_t iCaseCount)
	{
		const u_int uNode = m_xBuilder.Node("SwitchOnInt");
		m_xBuilder.ParamString(uNode, "m_strVar", szVar);
		m_xBuilder.ParamInt(uNode, "m_iCaseCount", iCaseCount);
		return Anchor(uNode);
	}

	Zenith_GraphChain StateMachine(const char* szStateVar, int32_t iStateCount, const char* szStateNames)
	{
		const u_int uNode = m_xBuilder.Node("StateMachine");
		m_xBuilder.ParamString(uNode, "m_strStateVar", szStateVar);
		m_xBuilder.ParamInt(uNode, "m_iStateCount", iStateCount);
		m_xBuilder.ParamString(uNode, "m_strStateNames", szStateNames);
		return Anchor(uNode);
	}

	// --- blackboard compares (constant form; var-vs-var keeps raw Node) -------
	Zenith_GraphChain CompareFloat(const char* szVar, Zenith_GraphCompareFloatOp eOp, float fCompareTo, const char* szResultVar)
	{
		const u_int uNode = m_xBuilder.Node("CompareBlackboardFloat");
		m_xBuilder.ParamString(uNode, "m_strVar", szVar);
		m_xBuilder.ParamFloat(uNode, "m_fCompareTo", fCompareTo);
		m_xBuilder.ParamEnum(uNode, "m_iOp", eOp);
		m_xBuilder.ParamString(uNode, "m_strResultVar", szResultVar);
		return Anchor(uNode);
	}

	Zenith_GraphChain CompareInt(const char* szVar, Zenith_GraphCompareIntOp eOp, int32_t iCompareTo, const char* szResultVar)
	{
		const u_int uNode = m_xBuilder.Node("CompareBlackboardInt");
		m_xBuilder.ParamString(uNode, "m_strVar", szVar);
		m_xBuilder.ParamInt(uNode, "m_iCompareTo", iCompareTo);
		m_xBuilder.ParamEnum(uNode, "m_iOp", eOp);
		m_xBuilder.ParamString(uNode, "m_strResultVar", szResultVar);
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

	// --- events out -----------------------------------------------------------
	// szTargetVar omitted -> node default ("" = self); szPayloadVar omitted ->
	// node default ("" = no payload). Both obey the exact-default rule.
	Zenith_GraphChain FireCustomEvent(const char* szEventName, const char* szTargetVar = nullptr, const char* szPayloadVar = nullptr)
	{
		const u_int uNode = m_xBuilder.Node("FireCustomEvent");
		m_xBuilder.ParamString(uNode, "m_strEventName", szEventName);
		if (szTargetVar) { m_xBuilder.ParamString(uNode, "m_strTargetVar", szTargetVar); }
		if (szPayloadVar) { m_xBuilder.ParamString(uNode, "m_strPayloadVar", szPayloadVar); }
		return Anchor(uNode);
	}

private:
	Zenith_GraphChain Anchor(u_int uNodeID) { return Zenith_GraphChain(m_xBuilder, uNodeID); }

	Zenith_GraphBuilder& m_xBuilder;
};
