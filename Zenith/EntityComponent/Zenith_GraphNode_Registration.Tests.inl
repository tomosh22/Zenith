//------------------------------------------------------------------------------
// Op-enum tests for the engine Behaviour Graph node library.
// Included at the bottom of Zenith_GraphNode_Registration.cpp (ZENITH_TESTING),
// where the anonymous-namespace node classes are still in scope.
//
// Two jobs:
//   1. A compile-time static_assert wall pinning every op enumerator to the raw
//      integer the switch used before Phase 1 (the serialization contract - a
//      drift here would silently repoint baked .bgraph blobs).
//   2. Per-op EXECUTION + out-of-range FALLBACK coverage for the switch/ternary
//      rewrites (a naive rewrite could change the default path).
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "EntityComponent/Zenith_EngineGraphBuilder.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TESTING

// --- 1. Value wall: enumerators MUST equal the historic ints ------------------
static_assert(GRAPH_COMPARE_FLOAT_OP_LESS == 0, "");
static_assert(GRAPH_COMPARE_FLOAT_OP_LESS_EQUAL == 1, "");
static_assert(GRAPH_COMPARE_FLOAT_OP_GREATER == 2, "");
static_assert(GRAPH_COMPARE_FLOAT_OP_GREATER_EQUAL == 3, "");
static_assert(GRAPH_COMPARE_FLOAT_OP_EQUAL == 4, "");

static_assert(GRAPH_COMPARE_INT_OP_LESS == 0, "");
static_assert(GRAPH_COMPARE_INT_OP_LESS_EQUAL == 1, "");
static_assert(GRAPH_COMPARE_INT_OP_GREATER == 2, "");
static_assert(GRAPH_COMPARE_INT_OP_GREATER_EQUAL == 3, "");
static_assert(GRAPH_COMPARE_INT_OP_EQUAL == 4, "");
static_assert(GRAPH_COMPARE_INT_OP_NOT_EQUAL == 5, "");

static_assert(GRAPH_ENTITY_COMPARE_OP_EQUAL == 0, "");
static_assert(GRAPH_ENTITY_COMPARE_OP_NOT_EQUAL == 1, "");

static_assert(GRAPH_MATH_FLOAT_OP_SUBTRACT == 0, "");
static_assert(GRAPH_MATH_FLOAT_OP_MULTIPLY == 1, "");
static_assert(GRAPH_MATH_FLOAT_OP_DIVIDE == 2, "");
static_assert(GRAPH_MATH_FLOAT_OP_MODULO == 3, "");
static_assert(GRAPH_MATH_FLOAT_OP_MIN == 4, "");
static_assert(GRAPH_MATH_FLOAT_OP_MAX == 5, "");
static_assert(GRAPH_MATH_FLOAT_OP_ABS == 6, "");
static_assert(GRAPH_MATH_FLOAT_OP_SIN == 7, "");
static_assert(GRAPH_MATH_FLOAT_OP_COS == 8, "");

// --- 2. Execution + fallback --------------------------------------------------

// Runs a CompareBlackboardFloat over var "v" (pre-seeded) and returns its bool
// result; asserts the node itself reported SUCCESS.
static bool RunCompareFloat(Zenith_GraphBlackboard& xBB, int32_t iOp, float fCompareTo)
{
	Zenith_GraphNode_CompareBlackboardFloat xNode;
	xNode.m_strVar = "v";
	xNode.m_fCompareTo = fCompareTo;
	xNode.m_iOp = iOp;
	xNode.m_strResultVar = "r";
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	return xBB.GetBool("r", false);
}

ZENITH_TEST(GraphNodeOps, CompareBlackboardFloatAllOps)
{
	Zenith_GraphBlackboard xBB;
	Zenith_PropertyValue xV; xV.SetFloat(5.0f);
	xBB.SetValue("v", xV);

	ZENITH_ASSERT_TRUE(RunCompareFloat(xBB, GRAPH_COMPARE_FLOAT_OP_LESS, 6.0f));
	ZENITH_ASSERT_FALSE(RunCompareFloat(xBB, GRAPH_COMPARE_FLOAT_OP_LESS, 5.0f));
	ZENITH_ASSERT_TRUE(RunCompareFloat(xBB, GRAPH_COMPARE_FLOAT_OP_LESS_EQUAL, 5.0f));
	ZENITH_ASSERT_FALSE(RunCompareFloat(xBB, GRAPH_COMPARE_FLOAT_OP_LESS_EQUAL, 4.0f));
	ZENITH_ASSERT_TRUE(RunCompareFloat(xBB, GRAPH_COMPARE_FLOAT_OP_GREATER, 4.0f));
	ZENITH_ASSERT_FALSE(RunCompareFloat(xBB, GRAPH_COMPARE_FLOAT_OP_GREATER, 5.0f));
	ZENITH_ASSERT_TRUE(RunCompareFloat(xBB, GRAPH_COMPARE_FLOAT_OP_GREATER_EQUAL, 5.0f));
	ZENITH_ASSERT_FALSE(RunCompareFloat(xBB, GRAPH_COMPARE_FLOAT_OP_GREATER_EQUAL, 6.0f));
	ZENITH_ASSERT_TRUE(RunCompareFloat(xBB, GRAPH_COMPARE_FLOAT_OP_EQUAL, 5.0f));
	ZENITH_ASSERT_FALSE(RunCompareFloat(xBB, GRAPH_COMPARE_FLOAT_OP_EQUAL, 4.0f));
}

ZENITH_TEST(GraphNodeOps, CompareBlackboardFloatOutOfRangeFails)
{
	Zenith_GraphBlackboard xBB;
	Zenith_PropertyValue xV; xV.SetFloat(1.0f);
	xBB.SetValue("v", xV);
	Zenith_GraphNode_CompareBlackboardFloat xNode;
	xNode.m_strVar = "v"; xNode.m_fCompareTo = 0.0f; xNode.m_iOp = 99; xNode.m_strResultVar = "r";
	Zenith_GraphContext xCtx; xCtx.m_pxBlackboard = &xBB;
	// The default: arm of the switch must still FAIL (not silently pick a branch).
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
}

static bool RunCompareInt(Zenith_GraphBlackboard& xBB, int32_t iOp, int32_t iCompareTo)
{
	Zenith_GraphNode_CompareBlackboardInt xNode;
	xNode.m_strVar = "v";
	xNode.m_iCompareTo = iCompareTo;
	xNode.m_iOp = iOp;
	xNode.m_strResultVar = "r";
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	return xBB.GetBool("r", false);
}

ZENITH_TEST(GraphNodeOps, CompareBlackboardIntAllOps)
{
	Zenith_GraphBlackboard xBB;
	Zenith_PropertyValue xV; xV.SetInt32(5);
	xBB.SetValue("v", xV);

	// LESS: 5<6 true; 5<5 false - the boundary distinguishes < from <=.
	ZENITH_ASSERT_TRUE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_LESS, 6));
	ZENITH_ASSERT_FALSE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_LESS, 5));
	ZENITH_ASSERT_FALSE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_LESS, 4));
	// LESS_EQUAL: 5<=5 true; 5<=4 false.
	ZENITH_ASSERT_TRUE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_LESS_EQUAL, 5));
	ZENITH_ASSERT_FALSE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_LESS_EQUAL, 4));
	// GREATER: 5>4 true; 5>5 false - the boundary distinguishes > from >=.
	ZENITH_ASSERT_TRUE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_GREATER, 4));
	ZENITH_ASSERT_FALSE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_GREATER, 5));
	ZENITH_ASSERT_FALSE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_GREATER, 6));
	// GREATER_EQUAL: 5>=5 true; 5>=6 false.
	ZENITH_ASSERT_TRUE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_GREATER_EQUAL, 5));
	ZENITH_ASSERT_FALSE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_GREATER_EQUAL, 6));
	// EQUAL
	ZENITH_ASSERT_TRUE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_EQUAL, 5));
	ZENITH_ASSERT_FALSE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_EQUAL, 4));
	// NOT_EQUAL (5) is the int-only op the float sibling lacks.
	ZENITH_ASSERT_TRUE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_NOT_EQUAL, 4));
	ZENITH_ASSERT_FALSE(RunCompareInt(xBB, GRAPH_COMPARE_INT_OP_NOT_EQUAL, 5));
}

ZENITH_TEST(GraphNodeOps, CompareBlackboardIntOutOfRangeFails)
{
	Zenith_GraphBlackboard xBB;
	Zenith_PropertyValue xV; xV.SetInt32(1);
	xBB.SetValue("v", xV);
	Zenith_GraphNode_CompareBlackboardInt xNode;
	xNode.m_strVar = "v"; xNode.m_iCompareTo = 0; xNode.m_iOp = 99; xNode.m_strResultVar = "r";
	Zenith_GraphContext xCtx; xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
}

// --- 3. Engine-node factory equivalence (Zenith_EngineGraphBuilder) -----------

static bool GraphDefsSerializeIdentically(Zenith_GraphDefinition& xA, Zenith_GraphDefinition& xB)
{
	Zenith_DataStream xSA;
	Zenith_DataStream xSB;
	xA.WriteToDataStream(xSA);
	xB.WriteToDataStream(xSB);
	if (xSA.GetCursor() != xSB.GetCursor()) { return false; }
	const u_int8* pA = static_cast<const u_int8*>(xSA.GetData());
	const u_int8* pB = static_cast<const u_int8*>(xSB.GetData());
	for (uint64_t u = 0; u < xSA.GetCursor(); ++u)
	{
		if (pA[u] != pB[u]) { return false; }
	}
	return true;
}

// A factory must emit the SAME node + params as the hand-written Node()+Param*.
ZENITH_TEST(EngineGraphBuilder, CompareFloatFactoryMatchesRaw)
{
	Zenith_GraphDefinition xFac;
	{
		Zenith_GraphBuilder xBuilder(xFac);
		Zenith_EngineGraphBuilder xB(xBuilder);
		xB.CompareFloat("v", GRAPH_COMPARE_FLOAT_OP_GREATER, 0.08f, "due");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_GraphDefinition xRaw;
	{
		Zenith_GraphBuilder xBuilder(xRaw);
		const u_int uNode = xBuilder.Node("CompareBlackboardFloat");
		ZENITH_ASSERT_NE(uNode, 0u);	// engine node resolved (test not vacuous)
		xBuilder.ParamString(uNode, "m_strVar", "v");
		xBuilder.ParamFloat(uNode, "m_fCompareTo", 0.08f);
		xBuilder.ParamInt(uNode, "m_iOp", 2);
		xBuilder.ParamString(uNode, "m_strResultVar", "due");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
}

// The exact-default rule: an omitted optional arg must leave the node default
// untouched, NOT overwrite it with "".
ZENITH_TEST(EngineGraphBuilder, OnCustomEventOmittedPayloadKeepsNodeDefault)
{
	Zenith_GraphDefinition xFac;
	{
		Zenith_GraphBuilder xBuilder(xFac);
		Zenith_EngineGraphBuilder xB(xBuilder);
		xB.OnCustomEvent("Tick");	// no payload arg
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	// Raw, leaving m_strStorePayloadVar at its "payload" default -> must MATCH.
	Zenith_GraphDefinition xRawDefault;
	{
		Zenith_GraphBuilder xBuilder(xRawDefault);
		const u_int uNode = xBuilder.Node("OnCustomEvent");
		ZENITH_ASSERT_NE(uNode, 0u);
		xBuilder.ParamString(uNode, "m_strEventName", "Tick");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRawDefault));

	// Raw, forcing m_strStorePayloadVar to "" -> must DIFFER (guards nullptr != "").
	Zenith_GraphDefinition xRawEmpty;
	{
		Zenith_GraphBuilder xBuilder(xRawEmpty);
		const u_int uNode = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uNode, "m_strEventName", "Tick");
		xBuilder.ParamString(uNode, "m_strStorePayloadVar", "");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	ZENITH_ASSERT_FALSE(GraphDefsSerializeIdentically(xFac, xRawEmpty));
}

ZENITH_TEST(EngineGraphBuilder, SourceFactoriesMatchRaw)
{
	{	// OnUpdate (no params)
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.OnUpdate(); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); ZENITH_ASSERT_NE(xBd.Node("OnUpdate"), 0u); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// OnStart (no params)
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.OnStart(); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); ZENITH_ASSERT_NE(xBd.Node("OnStart"), 0u); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// OnKeyPressed
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.OnKeyPressed(42); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("OnKeyPressed"); xBd.ParamInt(u, "m_iKeyCode", 42); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// OnCustomEvent WITH payload
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.OnCustomEvent("E", "dt"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("OnCustomEvent"); xBd.ParamString(u, "m_strEventName", "E"); xBd.ParamString(u, "m_strStorePayloadVar", "dt"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
}

ZENITH_TEST(EngineGraphBuilder, FlowFactoriesMatchRaw)
{
	{	// Branch
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.Branch("c"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("Branch"); xBd.ParamString(u, "m_strConditionVar", "c"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// Gate
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.Gate("o"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("Gate"); xBd.ParamString(u, "m_strOpenVar", "o"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// SwitchOnInt
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.SwitchOnInt("v", 3); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("SwitchOnInt"); xBd.ParamString(u, "m_strVar", "v"); xBd.ParamInt(u, "m_iCaseCount", 3); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// StateMachine
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.StateMachine("s", 4, "A,B,C,D"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("StateMachine"); xBd.ParamString(u, "m_strStateVar", "s"); xBd.ParamInt(u, "m_iStateCount", 4); xBd.ParamString(u, "m_strStateNames", "A,B,C,D"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
}

ZENITH_TEST(EngineGraphBuilder, BlackboardFactoriesMatchRaw)
{
	{	// CompareInt (with the int-only NOT_EQUAL enum to exercise the mapping)
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.CompareInt("v", GRAPH_COMPARE_INT_OP_NOT_EQUAL, 7, "r"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("CompareBlackboardInt"); xBd.ParamString(u, "m_strVar", "v"); xBd.ParamInt(u, "m_iCompareTo", 7); xBd.ParamInt(u, "m_iOp", 5); xBd.ParamString(u, "m_strResultVar", "r"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// SetBlackboardInt
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.SetBlackboardInt("v", 9); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("SetBlackboardInt"); xBd.ParamString(u, "m_strVariable", "v"); xBd.ParamInt(u, "m_iValue", 9); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// SetBlackboardFloat
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.SetBlackboardFloat("v", 1.5f); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("SetBlackboardFloat"); xBd.ParamString(u, "m_strVariable", "v"); xBd.ParamFloat(u, "m_fValue", 1.5f); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// SetBlackboardBool - false, i.e. NON-default (node default m_bValue is true)
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.SetBlackboardBool("v", false); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("SetBlackboardBool"); xBd.ParamString(u, "m_strVariable", "v"); xBd.ParamBool(u, "m_bValue", false); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
}

ZENITH_TEST(EngineGraphBuilder, FireCustomEventFactoryAndDefaults)
{
	{	// bare (only event name) matches raw with only m_strEventName set.
		// (Unlike OnCustomEvent, FireCustomEvent's m_strTargetVar/m_strPayloadVar
		// both DEFAULT to "" - so omitting them is byte-indistinguishable from
		// setting "". The exact-default rule holds trivially; nothing to guard.)
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.FireCustomEvent("E"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("FireCustomEvent"); xBd.ParamString(u, "m_strEventName", "E"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// with target var
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.FireCustomEvent("E", "tgt"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("FireCustomEvent"); xBd.ParamString(u, "m_strEventName", "E"); xBd.ParamString(u, "m_strTargetVar", "tgt"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// with payload var only (target left default)
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.FireCustomEvent("E", nullptr, "pl"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("FireCustomEvent"); xBd.ParamString(u, "m_strEventName", "E"); xBd.ParamString(u, "m_strPayloadVar", "pl"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
}

#endif // ZENITH_TESTING
