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
#include "Scripting/Zenith_BehaviourGraph.h"
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

static_assert(GRAPH_LOGIC_BOOL_OP_AND == 0, "");
static_assert(GRAPH_LOGIC_BOOL_OP_OR == 1, "");
static_assert(GRAPH_LOGIC_BOOL_OP_XOR == 2, "");

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

// ★ EVERY HALF DECLARES WHAT IT READS, IN THE SAME ORDER. A-8 latched the
// validator, so a fixture whose node reads an undeclared variable fails its
// Build(); and variables serialize FIRST, so the two halves must declare the
// same names, with the same types, in the same order or the byte comparison
// below fails for a reason that has nothing to do with the factory.
static void DeclareFloatVar(Zenith_GraphBuilder& xBuilder, const char* szName)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(0.0f);
	xBuilder.Variable(szName, xValue);
}

static void DeclareIntVar(Zenith_GraphBuilder& xBuilder, const char* szName)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(0);
	xBuilder.Variable(szName, xValue);
}

static void DeclareBoolVar(Zenith_GraphBuilder& xBuilder, const char* szName)
{
	Zenith_PropertyValue xValue;
	xValue.SetBool(false);
	xBuilder.Variable(szName, xValue);
}

static void DeclareEntityVar(Zenith_GraphBuilder& xBuilder, const char* szName)
{
	Zenith_PropertyValue xValue;
	xValue.SetPackedEntityID(0);
	xBuilder.Variable(szName, xValue);
}

// A factory must emit the SAME node + params as the hand-written Node()+Param*.
ZENITH_TEST(EngineGraphBuilder, CompareFloatFactoryMatchesRaw)
{
	Zenith_GraphDefinition xFac;
	{
		Zenith_GraphBuilder xBuilder(xFac);
		DeclareFloatVar(xBuilder, "v");		// CompareBlackboardFloat's Value pin READS it
		Zenith_EngineGraphBuilder xB(xBuilder);
		xB.CompareFloat("v", GRAPH_COMPARE_FLOAT_OP_GREATER, 0.08f, "due");
		ZENITH_ASSERT_TRUE(xBuilder.Build());
	}
	Zenith_GraphDefinition xRaw;
	{
		Zenith_GraphBuilder xBuilder(xRaw);
		DeclareFloatVar(xBuilder, "v");
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

// The B10 action-source / action-read factories. Same equivalence contract as
// every other factory: authoring through the DSL must serialize byte-identically
// to the hand-written Node() + ParamString it replaces.
ZENITH_TEST(EngineGraphBuilder, ActionFactoriesMatchRaw)
{
	const char* aszSourceTypes[] = { "OnActionPressed", "OnActionReleased", "OnActionHeld" };
	for (u_int uCase = 0; uCase < 3u; ++uCase)
	{
		Zenith_GraphDefinition xFac;
		{
			Zenith_GraphBuilder xBd(xFac);
			Zenith_EngineGraphBuilder xB(xBd);
			switch (uCase)
			{
			case 0: xB.OnActionPressed("Dodge"); break;
			case 1: xB.OnActionReleased("Dodge"); break;
			default: xB.OnActionHeld("Dodge"); break;
			}
			ZENITH_ASSERT_TRUE(xBd.Build());
		}
		Zenith_GraphDefinition xRaw;
		{
			Zenith_GraphBuilder xBd(xRaw);
			const u_int u = xBd.Node(aszSourceTypes[uCase]);
			xBd.ParamString(u, "m_strAction", "Dodge");
			ZENITH_ASSERT_TRUE(xBd.Build());
		}
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw), "%s factory diverged", aszSourceTypes[uCase]);
	}

	{	// ReadActionAxis1D WITH an explicit result var
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.ReadActionAxis1D("Lean", "lean"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("ReadActionAxis1D"); xBd.ParamString(u, "m_strAction", "Lean"); xBd.ParamString(u, "m_strResultVar", "lean"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// ReadActionAxis2D WITH an explicit result var
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.ReadActionAxis2D("Move", "move"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("ReadActionAxis2D"); xBd.ParamString(u, "m_strAction", "Move"); xBd.ParamString(u, "m_strResultVar", "move"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
}

// The exact-default rule for the axis factories: an omitted result var must
// leave the node's own default ("axis" / "axis2D") untouched, NOT overwrite it
// with "" -- a graph whose read var silently became the empty string writes
// into a variable nothing can read.
ZENITH_TEST(EngineGraphBuilder, ReadActionAxisOmittedResultVarKeepsNodeDefault)
{
	Zenith_GraphDefinition xFac;
	{
		Zenith_GraphBuilder xBd(xFac);
		Zenith_EngineGraphBuilder xB(xBd);
		xB.ReadActionAxis2D("Move");	// no result-var arg
		ZENITH_ASSERT_TRUE(xBd.Build());
	}
	Zenith_GraphDefinition xRawDefault;
	{
		Zenith_GraphBuilder xBd(xRawDefault);
		const u_int u = xBd.Node("ReadActionAxis2D");
		xBd.ParamString(u, "m_strAction", "Move");
		ZENITH_ASSERT_TRUE(xBd.Build());
	}
	ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRawDefault));

	Zenith_GraphDefinition xRawEmpty;
	{
		Zenith_GraphBuilder xBd(xRawEmpty);
		const u_int u = xBd.Node("ReadActionAxis2D");
		xBd.ParamString(u, "m_strAction", "Move");
		xBd.ParamString(u, "m_strResultVar", "");
		ZENITH_ASSERT_TRUE(xBd.Build());
	}
	ZENITH_ASSERT_FALSE(GraphDefsSerializeIdentically(xFac, xRawEmpty));
}

ZENITH_TEST(EngineGraphBuilder, FlowFactoriesMatchRaw)
{
	{	// Branch - Condition is an INPUT BOOL read
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); DeclareBoolVar(xBd, "c"); Zenith_EngineGraphBuilder xB(xBd); xB.Branch("c"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); DeclareBoolVar(xBd, "c"); const u_int u = xBd.Node("Branch"); xBd.ParamString(u, "m_strConditionVar", "c"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// Gate - Open is an INPUT BOOL read
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); DeclareBoolVar(xBd, "o"); Zenith_EngineGraphBuilder xB(xBd); xB.Gate("o"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); DeclareBoolVar(xBd, "o"); const u_int u = xBd.Node("Gate"); xBd.ParamString(u, "m_strOpenVar", "o"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// SwitchOnInt - Value is an INPUT INT32 read
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); DeclareIntVar(xBd, "v"); Zenith_EngineGraphBuilder xB(xBd); xB.SwitchOnInt("v", 3); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); DeclareIntVar(xBd, "v"); const u_int u = xBd.Node("SwitchOnInt"); xBd.ParamString(u, "m_strVar", "v"); xBd.ParamInt(u, "m_iCaseCount", 3); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// StateMachine - State is an INPUT INT32 read
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); DeclareIntVar(xBd, "s"); Zenith_EngineGraphBuilder xB(xBd); xB.StateMachine("s", 4, "A,B,C,D"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); DeclareIntVar(xBd, "s"); const u_int u = xBd.Node("StateMachine"); xBd.ParamString(u, "m_strStateVar", "s"); xBd.ParamInt(u, "m_iStateCount", 4); xBd.ParamString(u, "m_strStateNames", "A,B,C,D"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
}

ZENITH_TEST(EngineGraphBuilder, BlackboardFactoriesMatchRaw)
{
	{	// CompareInt (with the int-only NOT_EQUAL enum to exercise the mapping)
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); DeclareIntVar(xBd, "v"); Zenith_EngineGraphBuilder xB(xBd); xB.CompareInt("v", GRAPH_COMPARE_INT_OP_NOT_EQUAL, 7, "r"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); DeclareIntVar(xBd, "v"); const u_int u = xBd.Node("CompareBlackboardInt"); xBd.ParamString(u, "m_strVar", "v"); xBd.ParamInt(u, "m_iCompareTo", 7); xBd.ParamInt(u, "m_iOp", 5); xBd.ParamString(u, "m_strResultVar", "r"); ZENITH_ASSERT_TRUE(xBd.Build()); }
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
	{	// with target var - Target is a TARGET_ENTITY ref, so ENTITY_ID is the
		// only type its mask accepts
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); DeclareEntityVar(xBd, "tgt"); Zenith_EngineGraphBuilder xB(xBd); xB.FireCustomEvent("E", "tgt"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); DeclareEntityVar(xBd, "tgt"); const u_int u = xBd.Node("FireCustomEvent"); xBd.ParamString(u, "m_strEventName", "E"); xBd.ParamString(u, "m_strTargetVar", "tgt"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// with payload var only (target left default) - Payload is an INPUT ANY
		// read, so any declared type unifies; FLOAT is arbitrary
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); DeclareFloatVar(xBd, "pl"); Zenith_EngineGraphBuilder xB(xBd); xB.FireCustomEvent("E", nullptr, "pl"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); DeclareFloatVar(xBd, "pl"); const u_int u = xBd.Node("FireCustomEvent"); xBd.ParamString(u, "m_strEventName", "E"); xBd.ParamString(u, "m_strPayloadVar", "pl"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
}

// The blackboard-logic and list factories. Same equivalence contract as every
// other factory, plus the exact-default guard on the two optional bools.
ZENITH_TEST(EngineGraphBuilder, LogicAndListFactoriesMatchRaw)
{
	{	// LogicBool with the non-default XOR op and a NON-default invert
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.LogicBool("armed,jammed", GRAPH_LOGIC_BOOL_OP_XOR, "odd", true, true); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw;
		{
			Zenith_GraphBuilder xBd(xRaw);
			const u_int u = xBd.Node("LogicBlackboardBool");
			ZENITH_ASSERT_NE(u, 0u);	// engine node resolved (test not vacuous)
			xBd.ParamString(u, "m_strVars", "armed,jammed");
			xBd.ParamInt(u, "m_iOp", 2);
			xBd.ParamBool(u, "m_bInvert", true);
			xBd.ParamBool(u, "m_bMissingIsTrue", true);
			xBd.ParamString(u, "m_strResultVar", "odd");
			ZENITH_ASSERT_TRUE(xBd.Build());
		}
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// GetListCount
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.GetListCount("bag", "n"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("GetListCount"); ZENITH_ASSERT_NE(u, 0u); xBd.ParamString(u, "m_strListVar", "bag"); xBd.ParamString(u, "m_strResultVar", "n"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// GetListElement WITH an index var - Index is an INPUT INT32 read
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); DeclareIntVar(xBd, "cursor"); Zenith_EngineGraphBuilder xB(xBd); xB.GetListElement("bag", 3, "elem", "cursor"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); DeclareIntVar(xBd, "cursor"); const u_int u = xBd.Node("GetListElement"); ZENITH_ASSERT_NE(u, 0u); xBd.ParamString(u, "m_strListVar", "bag"); xBd.ParamInt(u, "m_iIndex", 3); xBd.ParamString(u, "m_strResultVar", "elem"); xBd.ParamString(u, "m_strIndexVar", "cursor"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// ForEach WITH an index var
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.ForEach("bag", "elem", "idx"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("ForEach"); ZENITH_ASSERT_NE(u, 0u); xBd.ParamString(u, "m_strListVar", "bag"); xBd.ParamString(u, "m_strElementVar", "elem"); xBd.ParamString(u, "m_strIndexVar", "idx"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// ListAdd - Value is an INPUT ANY read (the value pushed onto the list);
		// nothing in this fixture writes it, so ENTITY_ID is a plausible pick and
		// ANY unifies with it either way
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); DeclareEntityVar(xBd, "spawned"); Zenith_EngineGraphBuilder xB(xBd); xB.ListAdd("bag", "spawned"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); DeclareEntityVar(xBd, "spawned"); const u_int u = xBd.Node("ListAdd"); ZENITH_ASSERT_NE(u, 0u); xBd.ParamString(u, "m_strListVar", "bag"); xBd.ParamString(u, "m_strValueVar", "spawned"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// ListRemoveAt WITH an index var - Index is an INPUT INT32 read
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); DeclareIntVar(xBd, "cursor"); Zenith_EngineGraphBuilder xB(xBd); xB.ListRemoveAt("bag", 2, "cursor"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); DeclareIntVar(xBd, "cursor"); const u_int u = xBd.Node("ListRemoveAt"); ZENITH_ASSERT_NE(u, 0u); xBd.ParamString(u, "m_strListVar", "bag"); xBd.ParamInt(u, "m_iIndex", 2); xBd.ParamString(u, "m_strIndexVar", "cursor"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
	{	// ListClear
		Zenith_GraphDefinition xFac; { Zenith_GraphBuilder xBd(xFac); Zenith_EngineGraphBuilder xB(xBd); xB.ListClear("bag"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		Zenith_GraphDefinition xRaw; { Zenith_GraphBuilder xBd(xRaw); const u_int u = xBd.Node("ListClear"); ZENITH_ASSERT_NE(u, 0u); xBd.ParamString(u, "m_strListVar", "bag"); ZENITH_ASSERT_TRUE(xBd.Build()); }
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRaw));
	}
}

// The exact-default rule for the new optional args, asserted in BOTH directions
// the way OnCustomEventOmittedPayloadKeepsNodeDefault does: an omitted argument
// must MATCH raw-with-the-node-default, and DIFFER from raw forced to the other
// value. One direction alone would pass on a factory that silently ignored the
// argument entirely.
ZENITH_TEST(EngineGraphBuilder, LogicAndListOmittedArgsKeepNodeDefaults)
{
	{	// LogicBool: omitted bInvert / bMissingIsTrue keep the node's false.
		Zenith_GraphDefinition xFac;
		{
			Zenith_GraphBuilder xBd(xFac);
			Zenith_EngineGraphBuilder xB(xBd);
			xB.LogicBool("a,b", GRAPH_LOGIC_BOOL_OP_AND, "r");	// no flags
			ZENITH_ASSERT_TRUE(xBd.Build());
		}
		Zenith_GraphDefinition xRawDefault;
		{
			Zenith_GraphBuilder xBd(xRawDefault);
			const u_int u = xBd.Node("LogicBlackboardBool");
			xBd.ParamString(u, "m_strVars", "a,b");
			xBd.ParamInt(u, "m_iOp", 0);
			xBd.ParamString(u, "m_strResultVar", "r");
			ZENITH_ASSERT_TRUE(xBd.Build());
		}
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRawDefault));

		Zenith_GraphDefinition xRawInverted;
		{
			Zenith_GraphBuilder xBd(xRawInverted);
			const u_int u = xBd.Node("LogicBlackboardBool");
			xBd.ParamString(u, "m_strVars", "a,b");
			xBd.ParamInt(u, "m_iOp", 0);
			xBd.ParamBool(u, "m_bInvert", true);
			xBd.ParamString(u, "m_strResultVar", "r");
			ZENITH_ASSERT_TRUE(xBd.Build());
		}
		ZENITH_ASSERT_FALSE(GraphDefsSerializeIdentically(xFac, xRawInverted));
	}
	{	// ForEach: an omitted index var must keep "", NOT become something else.
		Zenith_GraphDefinition xFac;
		{
			Zenith_GraphBuilder xBd(xFac);
			Zenith_EngineGraphBuilder xB(xBd);
			xB.ForEach("bag", "elem");	// no index var
			ZENITH_ASSERT_TRUE(xBd.Build());
		}
		Zenith_GraphDefinition xRawDefault;
		{
			Zenith_GraphBuilder xBd(xRawDefault);
			const u_int u = xBd.Node("ForEach");
			xBd.ParamString(u, "m_strListVar", "bag");
			xBd.ParamString(u, "m_strElementVar", "elem");
			ZENITH_ASSERT_TRUE(xBd.Build());
		}
		ZENITH_ASSERT_TRUE(GraphDefsSerializeIdentically(xFac, xRawDefault));

		Zenith_GraphDefinition xRawIndexed;
		{
			Zenith_GraphBuilder xBd(xRawIndexed);
			const u_int u = xBd.Node("ForEach");
			xBd.ParamString(u, "m_strListVar", "bag");
			xBd.ParamString(u, "m_strElementVar", "elem");
			xBd.ParamString(u, "m_strIndexVar", "idx");
			ZENITH_ASSERT_TRUE(xBd.Build());
		}
		ZENITH_ASSERT_FALSE(GraphDefsSerializeIdentically(xFac, xRawIndexed));
	}
}

// --- 3b. GetVariable: the blackboard as a WIRE (B-3) --------------------------

namespace
{
	// ★ REGISTERED LAZILY, inside a test body, behind a latch - NEVER at static
	// init. ScriptTest's provenance contract walks the live registry and asserts
	// every row is engine-derived, and a static-init registration would put this
	// scratch type there before that contract ever runs.
	//
	// One INT32 INPUT, its var name defaulting to "" so a fixture that merely
	// places the node reads nothing, and a member recording what the pin
	// delivered - a wire that silently delivered nothing must not be
	// indistinguishable from one that delivered the right value.
	class Test_RegConsumerNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Test_RegConsumerNode)
	public:
		ZENITH_PROPERTY(std::string, m_strValueVar, "")
		ZENITH_PROPERTY(int32_t, m_iDefault, -1)

		ZENITH_GRAPH_PINS_BEGIN(Test_RegConsumerNode)
		ZENITH_GRAPH_PIN_INPUT_VAR_OR_CONST(Value, "m_strValueVar", "m_iDefault", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			m_iLastValue = GetInput<int32_t>(xContext, 0u);
			++m_uReadCount;
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "Test_RegConsumer"; }

		int32_t m_iLastValue = 0;
		u_int m_uReadCount = 0;
	};

	void EnsureRegistrationTestNodesRegistered()
	{
		// Keyed on the REGISTRY, not a static latch: this TU also owns the
		// totality test, whose registrar swap ResetForTests()s every row and
		// restores only the ENGINE set - a bool latch would then stay true with
		// the scratch type gone, and test ORDER would decide the outcome.
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		if (xRegistry.Find("Test_RegConsumer") != nullptr)
		{
			return;
		}
		xRegistry.RegisterNodeType<Test_RegConsumerNode>("Test_RegConsumer", GRAPH_EVENT_NONE, 1, false, "Test");
	}

	// OnUpdate -> Test_RegConsumer, with a GetVariable wired into the consumer's
	// INT32 input. Returns the consumer's node id; 0 means a node did not
	// resolve (an exe without the engine node library).
	u_int BuildGetVariableFixture(Zenith_GraphDefinition& xDefinition, const char* szVariable,
		const Zenith_PropertyValue* pxDeclared, u_int& uOutGetVariableNode)
	{
		uOutGetVariableNode = 0;
		if (pxDeclared != nullptr)
		{
			xDefinition.DeclareVariable(szVariable, *pxDeclared);
		}
		const u_int uSource = xDefinition.AddNode("OnUpdate");
		const u_int uConsumer = xDefinition.AddNode("Test_RegConsumer");
		const u_int uGet = xDefinition.AddNode("GetVariable");
		if (uSource == 0u || uConsumer == 0u || uGet == 0u)
		{
			return 0u;
		}

		// The variable NAME is a node param, so it goes through the blob.
		const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find("GetVariable");
		if (pxInfo == nullptr || pxInfo->m_pfnCreate == nullptr)
		{
			return 0u;
		}
		Zenith_GraphNode* pxTemp = pxInfo->m_pfnCreate();
		xDefinition.ApplyNodeParams(uGet, pxTemp, *pxInfo);
		static_cast<Zenith_GraphNode_GetVariable*>(pxTemp)->m_strVariable = szVariable;
		xDefinition.SetNodeParamsFromInstance(uGet, pxTemp);
		delete pxTemp;

		xDefinition.AddEdge(uSource, 0u, uConsumer);
		xDefinition.AddDataEdge(uGet, "Value", uConsumer, "Value");
		uOutGetVariableNode = uGet;
		return uConsumer;
	}

	void FireOneUpdate(Zenith_BehaviourGraph& xGraph)
	{
		Zenith_GraphContext xContext;
		xContext.m_fDt = 0.016f;
		xContext.m_pxGraph = &xGraph;
		xContext.m_pxBlackboard = &xGraph.GetBlackboard();
		xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	}
}

// ★ THE OUTPUT TYPE IS THE DECLARATION'S. A graph declaring "hp" INT32 wires
// GetVariable straight into an INT32 consumer, and the value arrives.
ZENITH_TEST(GraphNodeOps, GetVariable_OutputTypeFollowsDeclaration)
{
	EnsureRegistrationTestNodesRegistered();

	Zenith_PropertyValue xDeclared;
	xDeclared.SetInt32(7);
	Zenith_GraphDefinition xDef;
	u_int uGet = 0;
	const u_int uConsumer = BuildGetVariableFixture(xDef, "hp", &xDeclared, uGet);
	ZENITH_ASSERT_NE(uConsumer, 0u);
	if (uConsumer == 0u)
	{
		return;
	}

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	// The slot is typed off the DECLARATION, which is what lets SetOutput's tag
	// check pass at all.
	Zenith_GraphNode* pxGet = xGraph.FindNode(uGet);
	ZENITH_ASSERT_NOT_NULL(pxGet);
	if (pxGet != nullptr)
	{
		ZENITH_ASSERT_TRUE(pxGet->GetOutputPinType(1u) == PROPERTY_TYPE_INT32);
	}

	FireOneUpdate(xGraph);

	Test_RegConsumerNode* pxConsumer = static_cast<Test_RegConsumerNode*>(xGraph.FindNode(uConsumer));
	ZENITH_ASSERT_NOT_NULL(pxConsumer);
	if (pxConsumer != nullptr)
	{
		ZENITH_ASSERT_EQ(pxConsumer->m_uReadCount, 1u);
		ZENITH_ASSERT_EQ(pxConsumer->m_iLastValue, 7);
	}
}

// A missing variable is FAILURE, and a non-SUCCESS pure source yields the
// CONSUMER'S OWN pin default - never a fabricated zero.
ZENITH_TEST(GraphNodeOps, GetVariable_MissingVariableFailsAndConsumerReadsDefault)
{
	EnsureRegistrationTestNodesRegistered();

	Zenith_GraphDefinition xDef;
	u_int uGet = 0;
	const u_int uConsumer = BuildGetVariableFixture(xDef, "nothing_declares_me", nullptr, uGet);
	ZENITH_ASSERT_NE(uConsumer, 0u);
	if (uConsumer == 0u)
	{
		return;
	}

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	FireOneUpdate(xGraph);

	Test_RegConsumerNode* pxConsumer = static_cast<Test_RegConsumerNode*>(xGraph.FindNode(uConsumer));
	Zenith_GraphNode* pxGet = xGraph.FindNode(uGet);
	ZENITH_ASSERT_NOT_NULL(pxConsumer);
	ZENITH_ASSERT_NOT_NULL(pxGet);
	if (pxConsumer != nullptr)
	{
		ZENITH_ASSERT_EQ(pxConsumer->m_uReadCount, 1u);
		ZENITH_ASSERT_EQ(pxConsumer->m_iLastValue, -1);	// the const half of its own pin
	}
	if (pxGet != nullptr)
	{
		ZENITH_ASSERT_EQ(pxGet->GetPureStatusWarningCountForTest(), 1u);
	}
}

// ★ THE TAG IS COMPARED BEFORE THE WRITE. A live value whose type disagrees with
// the declaration FAILS - it does not reach SetOutput, whose refusal would leave
// the stamped zero SET and report SUCCESS while the consumer read 0.
ZENITH_TEST(GraphNodeOps, GetVariable_TypeDisagreementWithLiveValueFails)
{
	EnsureRegistrationTestNodesRegistered();

	Zenith_PropertyValue xDeclared;
	xDeclared.SetInt32(7);
	Zenith_GraphDefinition xDef;
	u_int uGet = 0;
	const u_int uConsumer = BuildGetVariableFixture(xDef, "hp", &xDeclared, uGet);
	ZENITH_ASSERT_NE(uConsumer, 0u);
	if (uConsumer == 0u)
	{
		return;
	}

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));

	// A FLOAT lands in a variable the graph declared INT32 - exactly what a
	// SetBlackboardFloat aimed at "hp" would do.
	Zenith_PropertyValue xFloat;
	xFloat.SetFloat(3.5f);
	xGraph.GetBlackboard().SetValue("hp", xFloat);

	FireOneUpdate(xGraph);

	Test_RegConsumerNode* pxConsumer = static_cast<Test_RegConsumerNode*>(xGraph.FindNode(uConsumer));
	Zenith_GraphNode* pxGet = xGraph.FindNode(uGet);
	ZENITH_ASSERT_NOT_NULL(pxConsumer);
	ZENITH_ASSERT_NOT_NULL(pxGet);
	if (pxConsumer != nullptr)
	{
		ZENITH_ASSERT_EQ(pxConsumer->m_iLastValue, -1);	// its own pin default
	}
	if (pxGet != nullptr)
	{
		// The write never happened, so the slot's own mismatch counter is ZERO -
		// the node refused, rather than being refused.
		ZENITH_ASSERT_EQ(pxGet->GetOutputMismatchWarningCountForTest(1u), 0u);
		ZENITH_ASSERT_EQ(pxGet->GetPureStatusWarningCountForTest(), 1u);
	}
}

// PURE means: no exec pins, and evaluated when a consumer GATHERS rather than
// when the exec walk reaches it.
ZENITH_TEST(GraphNodeOps, GetVariable_IsPureAndRunsOnDemand)
{
	EnsureRegistrationTestNodesRegistered();
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();

	const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find("GetVariable");
	ZENITH_ASSERT_NOT_NULL(pxInfo);
	if (pxInfo == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_TRUE(pxInfo->m_bPureNode);
	ZENITH_ASSERT_EQ(pxInfo->m_uExecOutputCount, 0u);

	Zenith_PropertyValue xDeclared;
	xDeclared.SetInt32(7);
	Zenith_GraphDefinition xDef;
	u_int uGet = 0;
	const u_int uConsumer = BuildGetVariableFixture(xDef, "hp", &xDeclared, uGet);
	ZENITH_ASSERT_NE(uConsumer, 0u);
	if (uConsumer == 0u)
	{
		return;
	}

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));

	// Changed AFTER instantiation and BEFORE the dispatch: a node that read its
	// variable at init would still deliver the declared 7.
	Zenith_PropertyValue xLive;
	xLive.SetInt32(9);
	xGraph.GetBlackboard().SetValue("hp", xLive);

	FireOneUpdate(xGraph);

	Test_RegConsumerNode* pxConsumer = static_cast<Test_RegConsumerNode*>(xGraph.FindNode(uConsumer));
	ZENITH_ASSERT_NOT_NULL(pxConsumer);
	if (pxConsumer != nullptr)
	{
		ZENITH_ASSERT_EQ(pxConsumer->m_iLastValue, 9);
	}
}

// --- 4. Pin-table totality + role spot-check (A-6) ----------------------------

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

// ★ The CORE registrar is not core's set. Zenith_RegisterEngineGraphNodes calls
// every sibling sub-registrar at its bottom, so running it alone would sweep
// nine other TUs' node types into this test. Core's own set is the DIFFERENCE:
// each sub-registrar is run first and its names subtracted. That keeps the test
// total by construction - a node added to the core TU tomorrow is covered with
// no list to maintain - and it is the only way to say "this TU's types" without
// filtering on m_strCategory, which is ZENITH_TOOLS-only (so a _False build
// would not compile) and aliases across TUs anyway ("Blackboard" is core's AND
// _Math's, "Flow" core's AND _Flow's, "Scene" core's AND _Scene's).
ZENITH_TEST(GraphPinTable, RegistrationTotality)
{
	Zenith_GraphNodeRegistrarFn const apfnSubRegistrars[] =
	{
		&Zenith_RegisterEngineGraphNodes_Input,
		&Zenith_RegisterEngineGraphNodes_Physics,
		&Zenith_RegisterEngineGraphNodes_Animation,
		&Zenith_RegisterEngineGraphNodes_UI,
		&Zenith_RegisterEngineGraphNodes_Scene,
		&Zenith_RegisterEngineGraphNodes_Entity,
		&Zenith_RegisterEngineGraphNodes_Math,
		&Zenith_RegisterEngineGraphNodes_Flow,
		&Zenith_RegisterEngineGraphNodes_AI,
	};
	Zenith_CheckPinTableTotalityEx(&Zenith_RegisterEngineGraphNodes,
		apfnSubRegistrars, static_cast<u_int>(sizeof(apfnSubRegistrars) / sizeof(apfnSubRegistrars[0])),
		"Registration.cpp", nullptr, 0u);
}

// One representative of each ROLE this TU declares. Roles are what the validator
// consumes (a WRITE role registers a writer that satisfies other readers; a READ
// role registers a read that must be satisfied), so a role typo is invisible to
// the totality walk - it would still count as "covered".
ZENITH_TEST(GraphPinTable, RegistrationRoleSpotCheck)
{
	// READWRITE: read AND written in one Execute. Its own write never satisfies
	// its own read (the validator's SELF_READWRITE rule).
	Zenith_CheckGraphPin("AddBlackboardFloat", "Variable", GRAPH_PIN_ROLE_SELECTOR_READWRITE, PROPERTY_TYPE_FLOAT, "m_strVariable");
	// ★ The trap: the SAME property name on Branch is read-only.
	Zenith_CheckGraphPin("Branch", "Condition", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_BOOL, "m_strConditionVar");

	// INPUT_VAR_OR_CONST: the ternary shape, both halves declared.
	Zenith_CheckGraphPin("AddBlackboardFloat", "Delta", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_FLOAT, "m_strDeltaVar");
	const Zenith_GraphPinDesc* pxDelta = Zenith_FindGraphPin("AddBlackboardFloat", "Delta");
	ZENITH_ASSERT_NOT_NULL(pxDelta);
	if (pxDelta != nullptr)
	{
		ZENITH_ASSERT_STREQ(pxDelta->m_szConstProperty, "m_fDelta",
			"the Delta pin lost its inline-constant half, so an unset delta var would look like an unsatisfied read");
	}

	// TARGET_REF, with the mask that makes a STRING entity name illegal.
	Zenith_CheckGraphPin("DestroyEntity", "Target", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strTargetVar");
	const Zenith_GraphPinDesc* pxTarget = Zenith_FindGraphPin("DestroyEntity", "Target");
	ZENITH_ASSERT_NOT_NULL(pxTarget);
	if (pxTarget != nullptr)
	{
		ZENITH_ASSERT_EQ(pxTarget->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_ENTITY,
			"ResolveTargetEntity accepts a packed ENTITY_ID and nothing else");
	}

	// OUTPUT: a computed result, not a configured destination.
	Zenith_CheckGraphPin("StoreSelfEntityID", "Variable", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_ENTITY_ID, "m_strVariable");
	// SELECTOR_WRITE, INHERITED: the collision family declares no table of its
	// own and must resolve to Zenith_GraphNode_CollisionSourceBase's.
	Zenith_CheckGraphPin("OnCollisionEnter", "StoreEntity", GRAPH_PIN_ROLE_SELECTOR_WRITE, PROPERTY_TYPE_ENTITY_ID, "m_strStoreEntityVar");
	Zenith_CheckGraphPin("OnCollisionExit", "StoreEntity", GRAPH_PIN_ROLE_SELECTOR_WRITE, PROPERTY_TYPE_ENTITY_ID, "m_strStoreEntityVar");
}

// GetVariable's two pins, spelled out: the totality walk above proves
// m_strVariable is COVERED by some descriptor, and this proves it is covered by
// the RIGHT ones - a SELECTOR_READ (the blackboard read, which declare-or-error
// reports) plus a from-variable OUTPUT that binds NOTHING.
ZENITH_TEST(GraphPinTable, GetVariable_TotalityRowPresent)
{
	// The read half: a SELECTOR_READ on m_strVariable, ANY (it reads whatever
	// type the variable is declared as).
	Zenith_CheckGraphPin("GetVariable", "Variable", GRAPH_PIN_ROLE_SELECTOR_READ, eGRAPH_PIN_TYPE_ANY, "m_strVariable");

	// The wire half: an OUTPUT whose TYPE comes from the graph, and whose
	// binding/const/fallback slots are all empty ON PURPOSE - a type source is
	// never a writer, or every other reader's declare-or-error would be silently
	// satisfied by the node that merely READ the variable.
	const Zenith_GraphPinDesc* pxValue = Zenith_FindGraphPin("GetVariable", "Value");
	ZENITH_ASSERT_NOT_NULL(pxValue);
	if (pxValue == nullptr)
	{
		return;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxValue->m_eRole), static_cast<int>(GRAPH_PIN_ROLE_OUTPUT));
	ZENITH_ASSERT_EQ(static_cast<int>(pxValue->m_eType), static_cast<int>(eGRAPH_PIN_TYPE_ANY));
	ZENITH_ASSERT_STREQ(pxValue->m_szTypeFromVarNameProperty, "m_strVariable");
	ZENITH_ASSERT_STREQ(pxValue->m_szVarNameProperty, "");
	ZENITH_ASSERT_STREQ(pxValue->m_szConstProperty, "");
	ZENITH_ASSERT_STREQ(pxValue->m_szFallbackVarNameProperty, "");
	ZENITH_ASSERT_FALSE(pxValue->m_bInstanceResolved);
}

//==============================================================================
// 5. PIN RUNTIME for the core TU (B-6.9) - the pins are LIVE.
//
// Every INPUT / INPUT_CONST descriptor above is read through
// Zenith_GraphNode::GetInput or TryGetInput and every OUTPUT descriptor written
// through SetOutput. The 21 tests ABOVE are unchanged in every assertion and are
// half the proof that an UNCONNECTED node is byte-for-byte what it was; the
// named GraphComponent.* cross-checks (RetrofittedNodeParamsExecution,
// BlackboardNodeFamilyExecution, CrossEntityEventsTargetingBroadcastAndArgs,
// EventPingPongTerminatesAtDepthCap, ThousandEntityUpdateBenchmark,
// RegistryWideNodeRoundTrip) are the other half. The rows below are the proof
// that a WIRE now carries a value.
//
// ★ EVERY ROW ASSERTS THE EXECUTE STATUS FIRST. A guard that FAILS above a read
// makes every later assertion vacuous, and three nodes here (TranslateEntity,
// StoreSelfEntityID, both Payload firers) fail exactly that way.
//
// ★ FRESH NODE PER LEG. Pin state - var names, the const property pointer, the
// slot's resolved type - is latched ONCE on the first accessor call and never
// refreshed from a later property write, and there is no ClearInputForTest, so a
// leg that re-points a var name or drops a wire needs a new instance. A CONST is
// read live through its reflected pointer, so flipping a const between fires on
// ONE instance is legitimate.
//
// ★ THE FLOW NODES NEED A GRAPH. Branch and Loop call
// xContext.m_pxGraph->RunChainFromPin, so a bare context would crash them: their
// rows run on a real Zenith_BehaviourGraph built from a Zenith_GraphDefinition,
// with a SetBlackboardBool / AddBlackboardFloat witness on each exec pin. These
// fixtures are authored through Zenith_GraphDefinition DIRECTLY and never
// through Zenith_GraphBlackboard's Zenith_GraphBuilder: every INPUT default in
// this TU is NON-EMPTY, so Build()'s declare-or-error would latch
// UNDECLARED_READ on a graph that is perfectly legal at runtime.
//
// ★ These fixtures never reach a counted census log: the per-game census parses
// `zenith test <G> --headless` runs, which pass --skip-unit-tests.
//==============================================================================

#include "UnitTests/Zenith_TempScene.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "Core/Zenith_PropertySystem.h"
#include <filesystem>

namespace
{
	// --- value makers, used as BOTH wire values and blackboard seeds -----------
	inline Zenith_PropertyValue CorePin_Float(float fValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetFloat(fValue);
		return xValue;
	}

	inline Zenith_PropertyValue CorePin_Int(int32_t iValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(iValue);
		return xValue;
	}

	inline Zenith_PropertyValue CorePin_Bool(bool bValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetBool(bValue);
		return xValue;
	}

	inline Zenith_PropertyValue CorePin_Vec3(const Zenith_Maths::Vector3& xVec)
	{
		Zenith_PropertyValue xValue;
		xValue.SetVector3(xVec);
		return xValue;
	}

	inline Zenith_PropertyValue CorePin_Str(const char* szValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetString(szValue);
		return xValue;
	}

	// The slot readers are TAG-CHECKED: Zenith_PropertyValue's typed getters
	// Zenith_Assert on a mismatch, and a wrong slot type must read as a test
	// FAILURE rather than a DebugBreak.
	inline bool CorePin_SlotBool(const Zenith_PropertyValue* pxSlot, const char* szWhat)
	{
		ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
		if (pxSlot == nullptr)
		{
			return false;
		}
		ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_BOOL),
			"%s: the slot holds type %u, not BOOL", szWhat, static_cast<u_int>(pxSlot->GetType()));
		return pxSlot->GetType() == PROPERTY_TYPE_BOOL ? pxSlot->GetBool() : false;
	}

	inline u_int64 CorePin_SlotPackedEntity(const Zenith_PropertyValue* pxSlot, const char* szWhat)
	{
		ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
		if (pxSlot == nullptr)
		{
			return 0ull;
		}
		ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_ENTITY_ID),
			"%s: the slot holds type %u, not ENTITY_ID", szWhat, static_cast<u_int>(pxSlot->GetType()));
		return pxSlot->GetType() == PROPERTY_TYPE_ENTITY_ID ? pxSlot->GetPackedEntityID() : 0ull;
	}

	// One row of the index contract: the constant (or the literal, for a pin no
	// Execute addresses) names the pin it is documented as, with the role the
	// migration assumed.
	inline void CorePin_CheckPin(const Zenith_GraphPinTable& xPins, u_int uIndex, const char* szName,
		Zenith_GraphPinRole eRole, const char* szClass)
	{
		ZENITH_ASSERT_LT(uIndex, xPins.GetPinCount(), "%s: pin index %u is past the end of the table", szClass, uIndex);
		if (uIndex >= xPins.GetPinCount())
		{
			return;
		}
		const Zenith_GraphPinDesc& xDesc = xPins.GetPinAt(uIndex);
		ZENITH_ASSERT_STREQ(xDesc.m_szName, szName,
			"%s: pin %u is '%s', not '%s' - the table was REORDERED, so every uPIN_ constant now addresses the wrong pin",
			szClass, uIndex, xDesc.m_szName != nullptr ? xDesc.m_szName : "(null)", szName);
		ZENITH_ASSERT_EQ(static_cast<int>(xDesc.m_eRole), static_cast<int>(eRole),
			"%s: pin '%s' changed ROLE - its runtime accessor would bad-access", szClass, szName);
	}

	// A class with no ZENITH_GRAPH_PINS block answers null from the virtual, which
	// is what keeps it out of the pin runtime entirely (no self-binding, no slots).
	// Asked of a STACK INSTANCE, never of the live registry: the core registrar
	// calls all nine sibling sub-registrars, so the registry is not this TU's set.
	inline void CorePin_CheckNoTable(const Zenith_GraphNode& xNode, const char* szClass)
	{
		ZENITH_ASSERT_NULL(xNode.GetPinTableVirtual(),
			"%s gained a pin table but nothing in its Execute addresses a pin", szClass);
	}

	// Multi-param node configuration through the param-blob path (the route the
	// editor uses). The graph fixtures below MUST configure through here rather
	// than assigning the member on the live node after FindNode:
	// BuildPinStateFromTables latches the binding's var NAME at build time, so a
	// late assignment would leave the binding pointing at the class default.
	struct CorePin_NodeParams
	{
		CorePin_NodeParams(Zenith_GraphDefinition& xDefinition, u_int uNodeID, const char* szTypeName)
			: m_xDefinition(xDefinition)
			, m_uNodeID(uNodeID)
			, m_pxInfo(Zenith_GraphNodeRegistry::Get().Find(szTypeName))
			, m_pxTemp(m_pxInfo != nullptr && m_pxInfo->m_pfnCreate != nullptr ? m_pxInfo->m_pfnCreate() : nullptr)
		{
			// Start from the params already stored for this node, so a second
			// configuration pass does not silently discard the first.
			if (m_pxTemp != nullptr)
			{
				m_xDefinition.ApplyNodeParams(m_uNodeID, m_pxTemp, *m_pxInfo);
			}
		}
		~CorePin_NodeParams()
		{
			if (m_pxTemp != nullptr)
			{
				m_xDefinition.SetNodeParamsFromInstance(m_uNodeID, m_pxTemp);
				delete m_pxTemp;
			}
		}
		CorePin_NodeParams(const CorePin_NodeParams&) = delete;
		CorePin_NodeParams& operator=(const CorePin_NodeParams&) = delete;

		void Set(const char* szProperty, const Zenith_PropertyValue& xValue)
		{
			if (m_pxTemp == nullptr || m_pxInfo->m_pfnGetPropertyTable == nullptr)
			{
				return;
			}
			const Zenith_ReflectedProperty* pxProperty = m_pxInfo->m_pfnGetPropertyTable()->FindProperty(szProperty);
			ZENITH_ASSERT_NOT_NULL(pxProperty, "no property '%s' on this node type", szProperty);
			if (pxProperty == nullptr)
			{
				return;
			}
			Zenith_PropertySystem::SetPropertyValue(m_pxTemp, *pxProperty, xValue);
		}
		void SetString(const char* szProperty, const char* szValue) { Set(szProperty, CorePin_Str(szValue)); }
		void SetInt(const char* szProperty, int32_t iValue) { Set(szProperty, CorePin_Int(iValue)); }

		Zenith_GraphDefinition& m_xDefinition;
		u_int m_uNodeID;
		const Zenith_GraphNodeTypeInfo* m_pxInfo;
		Zenith_GraphNode* m_pxTemp;
	};

	// OnUpdate -> Branch(szConditionVar), with a SetBlackboardBool witness on EACH
	// exec pin - which pin ran is then a blackboard fact, not an inference.
	// Returns the Branch's node id (0 = a node type did not resolve).
	inline u_int CorePin_BuildBranchFixture(Zenith_GraphDefinition& xDefinition, const char* szConditionVar,
		const char* szTrueVar, const char* szFalseVar)
	{
		Zenith_GraphNodeRegistry::Get().EnsureInitialized();
		const u_int uSource = xDefinition.AddNode("OnUpdate");
		const u_int uBranch = xDefinition.AddNode("Branch");
		const u_int uTrue = xDefinition.AddNode("SetBlackboardBool");
		const u_int uFalse = xDefinition.AddNode("SetBlackboardBool");
		if (uSource == 0u || uBranch == 0u || uTrue == 0u || uFalse == 0u)
		{
			return 0u;
		}
		{
			CorePin_NodeParams xParams(xDefinition, uBranch, "Branch");
			xParams.SetString("m_strConditionVar", szConditionVar);
		}
		{
			CorePin_NodeParams xParams(xDefinition, uTrue, "SetBlackboardBool");
			xParams.SetString("m_strVariable", szTrueVar);
		}
		{
			CorePin_NodeParams xParams(xDefinition, uFalse, "SetBlackboardBool");
			xParams.SetString("m_strVariable", szFalseVar);
		}
		xDefinition.AddEdge(uSource, 0u, uBranch);
		xDefinition.AddEdge(uBranch, 0u, uTrue);
		xDefinition.AddEdge(uBranch, 1u, uFalse);
		return uBranch;
	}

	// OnUpdate -> Loop, body pin driving an AddBlackboardFloat COUNTER (const
	// delta 1, its own variable, a name that collides with no node default) and
	// done pin a SetBlackboardBool. Returns the Loop's node id.
	inline u_int CorePin_BuildLoopFixture(Zenith_GraphDefinition& xDefinition, const char* szCountVar,
		int32_t iConstCount, const char* szCounterVar, const char* szDoneVar)
	{
		Zenith_GraphNodeRegistry::Get().EnsureInitialized();
		const u_int uSource = xDefinition.AddNode("OnUpdate");
		const u_int uLoop = xDefinition.AddNode("Loop");
		const u_int uBody = xDefinition.AddNode("AddBlackboardFloat");
		const u_int uDone = xDefinition.AddNode("SetBlackboardBool");
		if (uSource == 0u || uLoop == 0u || uBody == 0u || uDone == 0u)
		{
			return 0u;
		}
		{
			CorePin_NodeParams xParams(xDefinition, uLoop, "Loop");
			xParams.SetString("m_strCountVar", szCountVar);
			xParams.SetInt("m_iCount", iConstCount);
		}
		{
			CorePin_NodeParams xParams(xDefinition, uBody, "AddBlackboardFloat");
			xParams.SetString("m_strVariable", szCounterVar);
		}
		{
			CorePin_NodeParams xParams(xDefinition, uDone, "SetBlackboardBool");
			xParams.SetString("m_strVariable", szDoneVar);
		}
		xDefinition.AddEdge(uSource, 0u, uLoop);
		xDefinition.AddEdge(uLoop, 0u, uBody);
		xDefinition.AddEdge(uLoop, 1u, uDone);
		return uLoop;
	}

	// A handler graph asset whose OnCustomEvent stashes the payload and drives a
	// SetBlackboardBool witness. The payload's absence alone does not prove that
	// dispatch reached the handler. The recipe is
	// GraphComponent.CrossEntityEventsTargetingBroadcastAndArgs': the target's
	// only attachment seam is AddGraphByAssetPath, so the graph has to be SAVED.
	inline std::string CorePin_SavePayloadHandlerAsset(const char* szLeafName, const char* szEventName)
	{
		const std::string strAssetPath = std::string("game:Graphs/") + szLeafName;
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);

		Zenith_GraphNodeRegistry::Get().EnsureInitialized();
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDefinition = xAsset.GetDefinition();
		const u_int uSource = xDefinition.AddNode("OnCustomEvent");
		const u_int uWitness = xDefinition.AddNode("SetBlackboardBool");
		ZENITH_ASSERT_NE(uSource, 0u, "OnCustomEvent must resolve in the registry");
		ZENITH_ASSERT_NE(uWitness, 0u, "SetBlackboardBool must resolve in the registry");
		if (uSource == 0u || uWitness == 0u)
		{
			return strAssetPath;
		}
		{
			CorePin_NodeParams xParams(xDefinition, uSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", szEventName);
			xParams.SetString("m_strStorePayloadVar", "payload");
		}
		{
			CorePin_NodeParams xParams(xDefinition, uWitness, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "handlerRan");
		}
		ZENITH_ASSERT_TRUE(xDefinition.AddEdge(uSource, 0u, uWitness),
			"OnCustomEvent must connect to the handlerRan witness");
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
		return strAssetPath;
	}
}

// ★ TABLE ORDER IS THE CONTRACT, and this is the TU with the worst index shifts:
// SetBlackboard*.Value and AddBlackboardFloat.Delta are index 1 (a SELECTOR sits
// at 0), FireCustomEvent.Payload is index 1 while BroadcastCustomEvent.Payload is
// index 0, and Compare*.Result is index 2. A GetInput aimed at a SELECTOR or
// TARGET index is a silent type-zero plus one BADACCESS line, and a wrong-role
// SetOutput is a silent no-op - so a copied `uPIN_Value = 0u` would be invisible
// without this row. A static_assert is impossible: the tables fill at static init.
//
// The classes are addressed DIRECTLY through T::GetPinTableStatic(), never
// through the live registry: the core registrar calls all nine sibling
// sub-registrars at its bottom, so "what the registry holds" is not this TU's
// set (the same reason RegistrationTotality has to diff registrars).
// 23 registered classes carry a table (the three collision sources by
// INHERITANCE from the unregistered base) and 11 carry none; 23 + 11 = the 34
// RegisterNodeType calls above.
ZENITH_TEST(GraphPinTable, RegistrationPinIndicesMatchTables)
{
	// --- the inherited collision table, shared by three registered classes -----
	const Zenith_GraphPinTable& xCollision = Zenith_GraphNode_CollisionSourceBase::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xCollision.GetPinCount(), 1u, "CollisionSourceBase gained or lost a pin");
	CorePin_CheckPin(xCollision, 0u, "StoreEntity", GRAPH_PIN_ROLE_SELECTOR_WRITE, "CollisionSourceBase");
	ZENITH_ASSERT_TRUE(&Zenith_GraphNode_OnCollisionEnter::GetPinTableStatic() == &xCollision,
		"OnCollisionEnter stopped sharing the base's table - a PINS_BEGIN in a derived class SHADOWS it");
	ZENITH_ASSERT_TRUE(&Zenith_GraphNode_OnCollisionStay::GetPinTableStatic() == &xCollision);
	ZENITH_ASSERT_TRUE(&Zenith_GraphNode_OnCollisionExit::GetPinTableStatic() == &xCollision);

	const Zenith_GraphPinTable& xOnCustom = Zenith_GraphNode_OnCustomEvent::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xOnCustom.GetPinCount(), 1u, "OnCustomEvent gained or lost a pin");
	CorePin_CheckPin(xOnCustom, 0u, "StorePayload", GRAPH_PIN_ROLE_SELECTOR_WRITE, "OnCustomEvent");

	// --- the two comparators: Value 0, CompareTo 1, Result 2 ------------------
	const Zenith_GraphPinTable& xCmpFloat = Zenith_GraphNode_CompareBlackboardFloat::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xCmpFloat.GetPinCount(), 3u, "CompareBlackboardFloat gained or lost a pin");
	CorePin_CheckPin(xCmpFloat, Zenith_GraphNode_CompareBlackboardFloat::uPIN_Value, "Value",
		GRAPH_PIN_ROLE_INPUT, "CompareBlackboardFloat");
	CorePin_CheckPin(xCmpFloat, Zenith_GraphNode_CompareBlackboardFloat::uPIN_CompareTo, "CompareTo",
		GRAPH_PIN_ROLE_INPUT, "CompareBlackboardFloat");
	CorePin_CheckPin(xCmpFloat, Zenith_GraphNode_CompareBlackboardFloat::uPIN_Result, "Result",
		GRAPH_PIN_ROLE_OUTPUT, "CompareBlackboardFloat");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_CompareBlackboardFloat::uPIN_Value, 0u);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_CompareBlackboardFloat::uPIN_CompareTo, 1u);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_CompareBlackboardFloat::uPIN_Result, 2u);

	const Zenith_GraphPinTable& xCmpInt = Zenith_GraphNode_CompareBlackboardInt::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xCmpInt.GetPinCount(), 3u, "CompareBlackboardInt gained or lost a pin");
	CorePin_CheckPin(xCmpInt, Zenith_GraphNode_CompareBlackboardInt::uPIN_Value, "Value",
		GRAPH_PIN_ROLE_INPUT, "CompareBlackboardInt");
	CorePin_CheckPin(xCmpInt, Zenith_GraphNode_CompareBlackboardInt::uPIN_CompareTo, "CompareTo",
		GRAPH_PIN_ROLE_INPUT, "CompareBlackboardInt");
	CorePin_CheckPin(xCmpInt, Zenith_GraphNode_CompareBlackboardInt::uPIN_Result, "Result",
		GRAPH_PIN_ROLE_OUTPUT, "CompareBlackboardInt");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_CompareBlackboardInt::uPIN_Value, 0u);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_CompareBlackboardInt::uPIN_CompareTo, 1u);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_CompareBlackboardInt::uPIN_Result, 2u);

	// --- transform / entity actions -------------------------------------------
	const Zenith_GraphPinTable& xRotate = Zenith_GraphNode_RotateEntity::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xRotate.GetPinCount(), 1u, "RotateEntity gained a pin; its Execute addresses none");
	CorePin_CheckPin(xRotate, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "RotateEntity");

	// ★ THE ONE THAT WOULD HAVE BEEN WRONG IF COPIED: UnitsPerSecond is 0 and
	// Target is 1 - the reverse of ReadVelocity's shape in _Physics.
	const Zenith_GraphPinTable& xTranslate = Zenith_GraphNode_TranslateEntity::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xTranslate.GetPinCount(), 2u, "TranslateEntity gained or lost a pin");
	CorePin_CheckPin(xTranslate, Zenith_GraphNode_TranslateEntity::uPIN_UnitsPerSecond, "UnitsPerSecond",
		GRAPH_PIN_ROLE_INPUT, "TranslateEntity");
	CorePin_CheckPin(xTranslate, 1u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "TranslateEntity");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_TranslateEntity::uPIN_UnitsPerSecond, 0u);

	const Zenith_GraphPinTable& xDestroy = Zenith_GraphNode_DestroyEntity::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xDestroy.GetPinCount(), 1u, "DestroyEntity gained a pin; its Execute addresses none");
	CorePin_CheckPin(xDestroy, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "DestroyEntity");

	// --- the five SetBlackboard*: Variable 0 (SELECTOR_WRITE), Value 1 ---------
	const Zenith_GraphPinTable& xSetBool = Zenith_GraphNode_SetBlackboardBool::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSetBool.GetPinCount(), 2u, "SetBlackboardBool gained or lost a pin");
	CorePin_CheckPin(xSetBool, 0u, "Variable", GRAPH_PIN_ROLE_SELECTOR_WRITE, "SetBlackboardBool");
	CorePin_CheckPin(xSetBool, Zenith_GraphNode_SetBlackboardBool::uPIN_Value, "Value",
		GRAPH_PIN_ROLE_INPUT, "SetBlackboardBool");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_SetBlackboardBool::uPIN_Value, 1u,
		"Value is pin 1 - a 0 here would address the SELECTOR_WRITE destination and read a type zero");

	const Zenith_GraphPinTable& xSetFloat = Zenith_GraphNode_SetBlackboardFloat::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSetFloat.GetPinCount(), 2u, "SetBlackboardFloat gained or lost a pin");
	CorePin_CheckPin(xSetFloat, 0u, "Variable", GRAPH_PIN_ROLE_SELECTOR_WRITE, "SetBlackboardFloat");
	CorePin_CheckPin(xSetFloat, Zenith_GraphNode_SetBlackboardFloat::uPIN_Value, "Value",
		GRAPH_PIN_ROLE_INPUT, "SetBlackboardFloat");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_SetBlackboardFloat::uPIN_Value, 1u);

	const Zenith_GraphPinTable& xSetInt = Zenith_GraphNode_SetBlackboardInt::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSetInt.GetPinCount(), 2u, "SetBlackboardInt gained or lost a pin");
	CorePin_CheckPin(xSetInt, 0u, "Variable", GRAPH_PIN_ROLE_SELECTOR_WRITE, "SetBlackboardInt");
	CorePin_CheckPin(xSetInt, Zenith_GraphNode_SetBlackboardInt::uPIN_Value, "Value",
		GRAPH_PIN_ROLE_INPUT, "SetBlackboardInt");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_SetBlackboardInt::uPIN_Value, 1u);

	const Zenith_GraphPinTable& xSetVec = Zenith_GraphNode_SetBlackboardVector3::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSetVec.GetPinCount(), 2u, "SetBlackboardVector3 gained or lost a pin");
	CorePin_CheckPin(xSetVec, 0u, "Variable", GRAPH_PIN_ROLE_SELECTOR_WRITE, "SetBlackboardVector3");
	CorePin_CheckPin(xSetVec, Zenith_GraphNode_SetBlackboardVector3::uPIN_Value, "Value",
		GRAPH_PIN_ROLE_INPUT, "SetBlackboardVector3");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_SetBlackboardVector3::uPIN_Value, 1u);

	const Zenith_GraphPinTable& xSetStr = Zenith_GraphNode_SetBlackboardString::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSetStr.GetPinCount(), 2u, "SetBlackboardString gained or lost a pin");
	CorePin_CheckPin(xSetStr, 0u, "Variable", GRAPH_PIN_ROLE_SELECTOR_WRITE, "SetBlackboardString");
	CorePin_CheckPin(xSetStr, Zenith_GraphNode_SetBlackboardString::uPIN_Value, "Value",
		GRAPH_PIN_ROLE_INPUT, "SetBlackboardString");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_SetBlackboardString::uPIN_Value, 1u);

	// --- READWRITE selector at 0, Delta at 1 ----------------------------------
	const Zenith_GraphPinTable& xAdd = Zenith_GraphNode_AddBlackboardFloat::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xAdd.GetPinCount(), 2u, "AddBlackboardFloat gained or lost a pin");
	CorePin_CheckPin(xAdd, 0u, "Variable", GRAPH_PIN_ROLE_SELECTOR_READWRITE, "AddBlackboardFloat");
	CorePin_CheckPin(xAdd, Zenith_GraphNode_AddBlackboardFloat::uPIN_Delta, "Delta",
		GRAPH_PIN_ROLE_INPUT, "AddBlackboardFloat");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_AddBlackboardFloat::uPIN_Delta, 1u);

	// --- GetVariable: covered through its EXISTING uVALUE_PIN -----------------
	// ★ A DELIBERATE, RECORDED NAMING/PLACEMENT EXCEPTION (B-3): the constant is
	// uVALUE_PIN, declared inside the class body rather than before PINS_BEGIN.
	// It is NOT renamed or moved here - B-6.9 does not touch this node.
	const Zenith_GraphPinTable& xGetVar = Zenith_GraphNode_GetVariable::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xGetVar.GetPinCount(), 2u, "GetVariable gained or lost a pin");
	CorePin_CheckPin(xGetVar, 0u, "Variable", GRAPH_PIN_ROLE_SELECTOR_READ, "GetVariable");
	CorePin_CheckPin(xGetVar, Zenith_GraphNode_GetVariable::uVALUE_PIN, "Value",
		GRAPH_PIN_ROLE_OUTPUT, "GetVariable");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_GetVariable::uVALUE_PIN, 1u);

	// --- the OUTPUT-only node -------------------------------------------------
	const Zenith_GraphPinTable& xStoreSelf = Zenith_GraphNode_StoreSelfEntityID::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xStoreSelf.GetPinCount(), 1u, "StoreSelfEntityID gained or lost a pin");
	CorePin_CheckPin(xStoreSelf, Zenith_GraphNode_StoreSelfEntityID::uPIN_Variable, "Variable",
		GRAPH_PIN_ROLE_OUTPUT, "StoreSelfEntityID");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_StoreSelfEntityID::uPIN_Variable, 0u);

	// --- the two firers: Payload is 1 on one and 0 on the other ---------------
	const Zenith_GraphPinTable& xFire = Zenith_GraphNode_FireCustomEvent::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xFire.GetPinCount(), 2u, "FireCustomEvent gained or lost a pin");
	CorePin_CheckPin(xFire, 0u, "Target", GRAPH_PIN_ROLE_TARGET_REF, "FireCustomEvent");
	CorePin_CheckPin(xFire, Zenith_GraphNode_FireCustomEvent::uPIN_Payload, "Payload",
		GRAPH_PIN_ROLE_INPUT, "FireCustomEvent");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_FireCustomEvent::uPIN_Payload, 1u);

	const Zenith_GraphPinTable& xBroadcast = Zenith_GraphNode_BroadcastCustomEvent::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xBroadcast.GetPinCount(), 1u, "BroadcastCustomEvent gained or lost a pin");
	CorePin_CheckPin(xBroadcast, Zenith_GraphNode_BroadcastCustomEvent::uPIN_Payload, "Payload",
		GRAPH_PIN_ROLE_INPUT, "BroadcastCustomEvent");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_BroadcastCustomEvent::uPIN_Payload, 0u,
		"BroadcastCustomEvent has no Target, so its Payload is pin 0 - not FireCustomEvent's 1");

	// --- flow: one pin each ---------------------------------------------------
	const Zenith_GraphPinTable& xWait = Zenith_GraphNode_Wait::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xWait.GetPinCount(), 1u, "Wait gained or lost a pin");
	CorePin_CheckPin(xWait, Zenith_GraphNode_Wait::uPIN_Seconds, "Seconds", GRAPH_PIN_ROLE_INPUT, "Wait");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_Wait::uPIN_Seconds, 0u);

	const Zenith_GraphPinTable& xBranch = Zenith_GraphNode_Branch::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xBranch.GetPinCount(), 1u, "Branch gained or lost a pin");
	CorePin_CheckPin(xBranch, Zenith_GraphNode_Branch::uPIN_Condition, "Condition", GRAPH_PIN_ROLE_INPUT, "Branch");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_Branch::uPIN_Condition, 0u);

	const Zenith_GraphPinTable& xGate = Zenith_GraphNode_Gate::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xGate.GetPinCount(), 1u, "Gate gained or lost a pin");
	CorePin_CheckPin(xGate, Zenith_GraphNode_Gate::uPIN_Open, "Open", GRAPH_PIN_ROLE_INPUT, "Gate");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_Gate::uPIN_Open, 0u);

	const Zenith_GraphPinTable& xLoop = Zenith_GraphNode_Loop::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xLoop.GetPinCount(), 1u, "Loop gained or lost a pin");
	CorePin_CheckPin(xLoop, Zenith_GraphNode_Loop::uPIN_Count, "Count", GRAPH_PIN_ROLE_INPUT, "Loop");
	ZENITH_ASSERT_EQ(Zenith_GraphNode_Loop::uPIN_Count, 0u);

	// --- the ELEVEN registered classes with NO table at all -------------------
	// Proved through a STACK instance's virtual, the only thing that decides
	// whether a node can self-bind. Seven simple sources + Timer + DebugLog +
	// LoadSceneByIndex + Once.
	{
		Zenith_GraphNode_OnStart xOnStart;              CorePin_CheckNoTable(xOnStart, "OnStart");
		Zenith_GraphNode_OnUpdate xOnUpdate;            CorePin_CheckNoTable(xOnUpdate, "OnUpdate");
		Zenith_GraphNode_OnFixedUpdate xOnFixed;        CorePin_CheckNoTable(xOnFixed, "OnFixedUpdate");
		Zenith_GraphNode_OnEnable xOnEnable;            CorePin_CheckNoTable(xOnEnable, "OnEnable");
		Zenith_GraphNode_OnDisable xOnDisable;          CorePin_CheckNoTable(xOnDisable, "OnDisable");
		Zenith_GraphNode_OnDestroyEvent xOnDestroy;     CorePin_CheckNoTable(xOnDestroy, "OnDestroy");
		Zenith_GraphNode_OnGraphCall xOnGraphCall;      CorePin_CheckNoTable(xOnGraphCall, "OnGraphCall");
		Zenith_GraphNode_Timer xTimer;                  CorePin_CheckNoTable(xTimer, "Timer");
		Zenith_GraphNode_DebugLog xDebugLog;            CorePin_CheckNoTable(xDebugLog, "DebugLog");
		Zenith_GraphNode_LoadSceneByIndex xLoadScene;   CorePin_CheckNoTable(xLoadScene, "LoadSceneByIndex");
		Zenith_GraphNode_Once xOnce;                    CorePin_CheckNoTable(xOnce, "Once");
	}
}

// ★ NEVER THE SAME TRIPLE ON BOTH PINS, and never an op whose answer is the
// stamped FALSE: each leg wires exactly ONE pin and picks an op under which the
// wire's answer is TRUE and every other leg's is FALSE. A row that let the wire
// and the variable agree, or that asserted a false Result, would pass just as
// happily on a node that read nothing at all.
ZENITH_TEST(CorePinRuntime, Wired_CompareFloatValueAndCompareTo)
{
	const u_int uValue = Zenith_GraphNode_CompareBlackboardFloat::uPIN_Value;
	const u_int uCompareTo = Zenith_GraphNode_CompareBlackboardFloat::uPIN_CompareTo;
	const u_int uResult = Zenith_GraphNode_CompareBlackboardFloat::uPIN_Result;

	// LEG A - only Value is wired. var 7 vs wire 5 against a const CompareTo of
	// 6 under LESS: the wire says TRUE, the variable says FALSE.
	{
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("v", CorePin_Float(7.0f));

		Zenith_GraphNode_CompareBlackboardFloat xNode;
		xNode.m_strVar = "v";
		xNode.m_fCompareTo = 6.0f;
		xNode.m_strCompareVar = "";
		xNode.m_iOp = GRAPH_COMPARE_FLOAT_OP_LESS;
		xNode.SetInputForTest(uValue, CorePin_Float(5.0f));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(CorePin_SlotBool(xNode.GetOutputForTest(uResult), "CompareFloat.Result"),
			"5 < 6 is TRUE - the wired Value never reached the comparison");
		// The dual-write lands exactly where today's SetValue did.
		ZENITH_ASSERT_TRUE(xBB.GetBool("result"));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uValue), 0u, "a WIRED pin must not log a census line");
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uCompareTo), 0u, "an EMPTY var name cannot log");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// LEG B - only CompareTo is wired, and all three of its legs differ: const 9,
	// var 8, wire 6, against a Value of 7 under GREATER. Only the wire makes it
	// TRUE.
	{
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("v", CorePin_Float(7.0f));
		xBB.SetValue("cmp", CorePin_Float(8.0f));

		Zenith_GraphNode_CompareBlackboardFloat xNode;
		xNode.m_strVar = "v";
		xNode.m_fCompareTo = 9.0f;
		xNode.m_strCompareVar = "cmp";
		xNode.m_iOp = GRAPH_COMPARE_FLOAT_OP_GREATER;
		xNode.SetInputForTest(uCompareTo, CorePin_Float(6.0f));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(CorePin_SlotBool(xNode.GetOutputForTest(uResult), "CompareFloat.Result"),
			"7 > 6 is TRUE; 7 > 8 and 7 > 9 are both FALSE, so a FALSE here means the wire lost");
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uCompareTo), 0u);
		// Value is unwired and var-BOUND, so it legitimately logs exactly one line.
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uValue), 1u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// LEG C - the `""` DIVERGENCE, on a TRUE-yielding configuration so the slot's
	// value (true) is distinguishable from a BOOL slot's stamped false.
	{
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("v", CorePin_Float(7.0f));
		const u_int uBefore = xBB.GetCount();

		Zenith_GraphNode_CompareBlackboardFloat xNode;
		xNode.m_strVar = "v";
		xNode.m_fCompareTo = 6.0f;
		xNode.m_iOp = GRAPH_COMPARE_FLOAT_OP_LESS;
		xNode.m_strResultVar = "";
		xNode.SetInputForTest(uValue, CorePin_Float(5.0f));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(CorePin_SlotBool(xNode.GetOutputForTest(uResult), "CompareFloat.Result"));
		ZENITH_ASSERT_NULL(xBB.TryGetValue(""),
			"an unnamed OUTPUT created a blackboard variable literally named \"\"");
		ZENITH_ASSERT_EQ(xBB.GetCount(), uBefore, "an unnamed OUTPUT added a blackboard variable");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// The int twin, leg for leg.
ZENITH_TEST(CorePinRuntime, Wired_CompareIntValueAndCompareTo)
{
	const u_int uValue = Zenith_GraphNode_CompareBlackboardInt::uPIN_Value;
	const u_int uCompareTo = Zenith_GraphNode_CompareBlackboardInt::uPIN_CompareTo;
	const u_int uResult = Zenith_GraphNode_CompareBlackboardInt::uPIN_Result;

	{	// LEG A: var 7 vs wire 5, const CompareTo 6, LESS -> only the wire is TRUE.
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("v", CorePin_Int(7));

		Zenith_GraphNode_CompareBlackboardInt xNode;
		xNode.m_strVar = "v";
		xNode.m_iCompareTo = 6;
		xNode.m_strCompareVar = "";
		xNode.m_iOp = GRAPH_COMPARE_INT_OP_LESS;
		xNode.SetInputForTest(uValue, CorePin_Int(5));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(CorePin_SlotBool(xNode.GetOutputForTest(uResult), "CompareInt.Result"));
		ZENITH_ASSERT_TRUE(xBB.GetBool("result"));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uValue), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{	// LEG B: const 9 / var 8 / wire 6 against Value 7, GREATER.
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("v", CorePin_Int(7));
		xBB.SetValue("cmp", CorePin_Int(8));

		Zenith_GraphNode_CompareBlackboardInt xNode;
		xNode.m_strVar = "v";
		xNode.m_iCompareTo = 9;
		xNode.m_strCompareVar = "cmp";
		xNode.m_iOp = GRAPH_COMPARE_INT_OP_GREATER;
		xNode.SetInputForTest(uCompareTo, CorePin_Int(6));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(CorePin_SlotBool(xNode.GetOutputForTest(uResult), "CompareInt.Result"));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uCompareTo), 0u);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uValue), 1u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{	// LEG C: the `""` divergence on a TRUE-yielding configuration.
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("v", CorePin_Int(7));
		const u_int uBefore = xBB.GetCount();

		Zenith_GraphNode_CompareBlackboardInt xNode;
		xNode.m_strVar = "v";
		xNode.m_iCompareTo = 6;
		xNode.m_iOp = GRAPH_COMPARE_INT_OP_LESS;
		xNode.m_strResultVar = "";
		xNode.SetInputForTest(uValue, CorePin_Int(5));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(CorePin_SlotBool(xNode.GetOutputForTest(uResult), "CompareInt.Result"));
		ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
		ZENITH_ASSERT_EQ(xBB.GetCount(), uBefore);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// THREE DISTINCT DIRECTIONS (not three magnitudes), dt exactly 1.0 so the
// transform delta IS the vector: +X is the const, +Y the variable, +Z the wire.
ZENITH_TEST(CorePinRuntime, Wired_TranslateEntityUnitsPerSecond)
{
	Zenith_TempScene xScene("TestCorePinTranslateScene");
	Zenith_Entity xEntity = xScene.CreateEntity("CorePinTranslate");
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f));

	Zenith_GraphBlackboard xBB;
	xBB.SetValue("units", CorePin_Vec3(Zenith_Maths::Vector3(0.0f, 7.0f, 0.0f)));

	Zenith_GraphNode_TranslateEntity xNode;
	xNode.m_xUnitsPerSecond = Zenith_Maths::Vector3(9.0f, 0.0f, 0.0f);
	xNode.m_strUnitsVar = "units";
	xNode.m_strTargetVar = "";
	xNode.SetInputForTest(Zenith_GraphNode_TranslateEntity::uPIN_UnitsPerSecond,
		CorePin_Vec3(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f)));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_xSelf = xEntity;
	xCtx.m_fDt = 1.0f;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	Zenith_Maths::Vector3 xPosition;
	xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPosition);
	ZENITH_ASSERT_NEAR_VEC3(xPosition, Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f), 0.001f);
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_TranslateEntity::uPIN_UnitsPerSecond), 0u);
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

// The READWRITE half stays DIRECT while Delta comes off the wire: the variable is
// read (10) and written back (10 + 5) by plain blackboard calls, and the dt scale
// is applied AFTER the pin read, exactly as the ternary's result was.
ZENITH_TEST(CorePinRuntime, Wired_AddBlackboardFloatDelta)
{
	const u_int uDelta = Zenith_GraphNode_AddBlackboardFloat::uPIN_Delta;

	{	// (a) no dt scale: 10 + wire 5 = 15, not 10 + const 9 and not 10 + var 7.
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("acc", CorePin_Float(10.0f));
		xBB.SetValue("d", CorePin_Float(7.0f));

		Zenith_GraphNode_AddBlackboardFloat xNode;
		xNode.m_strVariable = "acc";
		xNode.m_fDelta = 9.0f;
		xNode.m_strDeltaVar = "d";
		xNode.SetInputForTest(uDelta, CorePin_Float(5.0f));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_fDt = 0.5f;		// ignored: m_bScaleByDt is false
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xBB.GetFloat("acc"), 15.0f, 0.0001f);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uDelta), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{	// (b) THE SCALE IS APPLIED TO THE WIRE: 10 + 5 * 0.5 = 12.5.
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("acc", CorePin_Float(10.0f));

		Zenith_GraphNode_AddBlackboardFloat xNode;
		xNode.m_strVariable = "acc";
		xNode.m_fDelta = 9.0f;
		xNode.m_bScaleByDt = true;
		xNode.SetInputForTest(uDelta, CorePin_Float(5.0f));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_fDt = 0.5f;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xBB.GetFloat("acc"), 12.5f, 0.0001f);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// ★ THE FIRST WIRE-ABLE CONSTANTS. Each type gets a WIRED leg (the wire beats the
// const) and an UNCONNECTED leg (the const still wins, byte-for-byte the old
// direct member read), on a FRESH node each, with the destination variable
// PRE-SEEDED to the value the losing leg would have produced - so "the node never
// ran" cannot pass. An INPUT_CONST pin has no var-name property at all, so its
// fallback counter can never move.
ZENITH_TEST(CorePinRuntime, Wired_SetBlackboardValueFromWire)
{
	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// --- BOOL: two states only, so the wire must differ from the const --------
	{	// const false, wire true, destination pre-seeded false.
		xBB.SetValue("b", CorePin_Bool(false));
		Zenith_GraphNode_SetBlackboardBool xNode;
		xNode.m_strVariable = "b";
		xNode.m_bValue = false;
		xNode.SetInputForTest(Zenith_GraphNode_SetBlackboardBool::uPIN_Value, CorePin_Bool(true));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(xBB.GetBool("b"), "the wired true lost to the const false");
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetBlackboardBool::uPIN_Value), 0u,
			"an INPUT_CONST pin carries no var name and can never log FALLBACK");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
	{	// the mirror: const true, wire false, destination pre-seeded true.
		xBB.SetValue("b", CorePin_Bool(true));
		Zenith_GraphNode_SetBlackboardBool xNode;
		xNode.m_strVariable = "b";
		xNode.m_bValue = true;
		xNode.SetInputForTest(Zenith_GraphNode_SetBlackboardBool::uPIN_Value, CorePin_Bool(false));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_FALSE(xBB.GetBool("b", true), "the wired false lost to the const true");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
	{	// UNCONNECTED, on the NON-default const (m_bValue defaults to true).
		xBB.SetValue("b", CorePin_Bool(true));
		Zenith_GraphNode_SetBlackboardBool xNode;
		xNode.m_strVariable = "b";
		xNode.m_bValue = false;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_FALSE(xBB.GetBool("b", true), "an unconnected INPUT_CONST must still write its const");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// --- FLOAT 9 / 5 ----------------------------------------------------------
	{
		Zenith_GraphNode_SetBlackboardFloat xNode;
		xNode.m_strVariable = "f";
		xNode.m_fValue = 9.0f;
		xNode.SetInputForTest(Zenith_GraphNode_SetBlackboardFloat::uPIN_Value, CorePin_Float(5.0f));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xBB.GetFloat("f"), 5.0f, 0.0001f);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetBlackboardFloat::uPIN_Value), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
	{
		Zenith_GraphNode_SetBlackboardFloat xNode;
		xNode.m_strVariable = "f";
		xNode.m_fValue = 9.0f;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(xBB.GetFloat("f"), 9.0f, 0.0001f);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// --- INT32 9 / 5 ----------------------------------------------------------
	{
		Zenith_GraphNode_SetBlackboardInt xNode;
		xNode.m_strVariable = "i";
		xNode.m_iValue = 9;
		xNode.SetInputForTest(Zenith_GraphNode_SetBlackboardInt::uPIN_Value, CorePin_Int(5));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(xBB.GetInt32("i"), 5);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetBlackboardInt::uPIN_Value), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
	{
		Zenith_GraphNode_SetBlackboardInt xNode;
		xNode.m_strVariable = "i";
		xNode.m_iValue = 9;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(xBB.GetInt32("i"), 9);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// --- VECTOR3: every component distinct, and neither leg is (0,0,0) --------
	{
		Zenith_GraphNode_SetBlackboardVector3 xNode;
		xNode.m_strVariable = "vec";
		xNode.m_xValue = Zenith_Maths::Vector3(9.0f, 1.0f, 2.0f);
		xNode.SetInputForTest(Zenith_GraphNode_SetBlackboardVector3::uPIN_Value,
			CorePin_Vec3(Zenith_Maths::Vector3(3.0f, 5.0f, 7.0f)));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("vec"), Zenith_Maths::Vector3(3.0f, 5.0f, 7.0f), 0.0001f);
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetBlackboardVector3::uPIN_Value), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
	{
		Zenith_GraphNode_SetBlackboardVector3 xNode;
		xNode.m_strVariable = "vec";
		xNode.m_xValue = Zenith_Maths::Vector3(9.0f, 1.0f, 2.0f);
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("vec"), Zenith_Maths::Vector3(9.0f, 1.0f, 2.0f), 0.0001f);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	// --- STRING "nine" / "five" ----------------------------------------------
	{
		Zenith_GraphNode_SetBlackboardString xNode;
		xNode.m_strVariable = "s";
		xNode.m_strValue = "nine";
		xNode.SetInputForTest(Zenith_GraphNode_SetBlackboardString::uPIN_Value, CorePin_Str("five"));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xBB.GetString("s").c_str(), "five");
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(Zenith_GraphNode_SetBlackboardString::uPIN_Value), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
	{
		Zenith_GraphNode_SetBlackboardString xNode;
		xNode.m_strVariable = "s";
		xNode.m_strValue = "nine";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_STREQ(xBB.GetString("s").c_str(), "nine");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// Wait reads Seconds on EVERY tick, RUNNING ones included. The wired 0.5 against
// a const 9 and a variable 7 is what makes the second tick SUCCEED at all - with
// either other leg the node would still be RUNNING - and the fallback leg is the
// proof that "every tick" does NOT mean "a census line every tick".
ZENITH_TEST(CorePinRuntime, Wired_WaitSecondsFromWire)
{
	const u_int uSeconds = Zenith_GraphNode_Wait::uPIN_Seconds;

	{	// const 9 / var 7 / wire 0.5, dt 0.3: RUNNING then SUCCESS.
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("w", CorePin_Float(7.0f));

		Zenith_GraphNode_Wait xNode;
		xNode.m_fSeconds = 9.0f;
		xNode.m_strSecondsVar = "w";
		xNode.SetInputForTest(uSeconds, CorePin_Float(0.5f));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_fDt = 0.3f;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_RUNNING));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS),
			"0.3 + 0.3 >= the WIRED 0.5 - a second RUNNING means the const or the var won");
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uSeconds), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{	// THE COUNTER LATCHES: a var-bound Seconds read on four RUNNING ticks logs
		// exactly ONE census line, which is what keeps a hot Wait from spamming.
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("w", CorePin_Float(7.0f));		// ASSIGNED: m_strSecondsVar defaults EMPTY

		Zenith_GraphNode_Wait xNode;
		xNode.m_fSeconds = 9.0f;
		xNode.m_strSecondsVar = "w";

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_fDt = 0.3f;
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uSeconds), 0u);
		for (u_int u = 0; u < 4u; ++u)
		{
			ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_RUNNING),
				"1.2 s of dt is well short of the variable's 7 s");
		}
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uSeconds), 1u,
			"four reads must log ONE census line");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// ★ THE LOAD-BEARING LEG IS WIRE-TRUE AGAINST VAR-FALSE. A wire-false leg would
// pass for a node that read nothing at all (false is the BOOL pin default), so
// that direction is only ever the mirror.
ZENITH_TEST(CorePinRuntime, Wired_BranchConditionFromWire)
{
	{	// var false, wire TRUE -> exec pin 0.
		Zenith_GraphDefinition xDef;
		const u_int uBranch = CorePin_BuildBranchFixture(xDef, "cond", "tookTrue", "tookFalse");
		ZENITH_ASSERT_NE(uBranch, 0u);
		if (uBranch == 0u)
		{
			return;
		}
		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		xGraph.GetBlackboard().SetValue("cond", CorePin_Bool(false));

		Zenith_GraphNode* pxBranch = xGraph.FindNode(uBranch);
		ZENITH_ASSERT_NOT_NULL(pxBranch);
		if (pxBranch == nullptr)
		{
			return;
		}
		pxBranch->SetInputForTest(Zenith_GraphNode_Branch::uPIN_Condition, CorePin_Bool(true));

		FireOneUpdate(xGraph);
		ZENITH_ASSERT_TRUE(xGraph.GetBlackboard().GetBool("tookTrue"),
			"the WIRED true did not reach Branch - it took the variable's false pin");
		ZENITH_ASSERT_NULL(xGraph.GetBlackboard().TryGetValue("tookFalse"));
		ZENITH_ASSERT_EQ(pxBranch->GetFallbackUseCountForTest(Zenith_GraphNode_Branch::uPIN_Condition), 0u);
		ZENITH_ASSERT_EQ(pxBranch->GetBadAccessWarningCountForTest(), 0u);
	}

	{	// THE MIRROR: var true, wire false -> exec pin 1.
		Zenith_GraphDefinition xDef;
		const u_int uBranch = CorePin_BuildBranchFixture(xDef, "cond", "tookTrue", "tookFalse");
		ZENITH_ASSERT_NE(uBranch, 0u);
		if (uBranch == 0u)
		{
			return;
		}
		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		xGraph.GetBlackboard().SetValue("cond", CorePin_Bool(true));

		Zenith_GraphNode* pxBranch = xGraph.FindNode(uBranch);
		ZENITH_ASSERT_NOT_NULL(pxBranch);
		if (pxBranch == nullptr)
		{
			return;
		}
		pxBranch->SetInputForTest(Zenith_GraphNode_Branch::uPIN_Condition, CorePin_Bool(false));

		FireOneUpdate(xGraph);
		ZENITH_ASSERT_TRUE(xGraph.GetBlackboard().GetBool("tookFalse"));
		ZENITH_ASSERT_NULL(xGraph.GetBlackboard().TryGetValue("tookTrue"));
		ZENITH_ASSERT_EQ(pxBranch->GetBadAccessWarningCountForTest(), 0u);
	}
}

// Gate needs no graph: its Execute is the read and the status, nothing else.
ZENITH_TEST(CorePinRuntime, Wired_GateOpenFromWire)
{
	const u_int uOpen = Zenith_GraphNode_Gate::uPIN_Open;

	{	// var false, wire TRUE -> SUCCESS (the load-bearing direction).
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("open", CorePin_Bool(false));

		Zenith_GraphNode_Gate xNode;
		xNode.m_strOpenVar = "open";
		xNode.SetInputForTest(uOpen, CorePin_Bool(true));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS),
			"the WIRED true did not open the gate");
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uOpen), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{	// the mirror: var true, wire false -> FAILURE.
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("open", CorePin_Bool(true));

		Zenith_GraphNode_Gate xNode;
		xNode.m_strOpenVar = "open";
		xNode.SetInputForTest(uOpen, CorePin_Bool(false));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// const 9 / var 7 / wire 2: the body counter is the witness, so "which leg won"
// is a number, not a guess. The done flag proves the loop finished rather than
// suspending.
ZENITH_TEST(CorePinRuntime, Wired_LoopCountFromWire)
{
	Zenith_GraphDefinition xDef;
	const u_int uLoop = CorePin_BuildLoopFixture(xDef, "n", 9, "loopCounter", "loopDone");
	ZENITH_ASSERT_NE(uLoop, 0u);
	if (uLoop == 0u)
	{
		return;
	}
	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	xGraph.GetBlackboard().SetValue("n", CorePin_Int(7));

	Zenith_GraphNode* pxLoop = xGraph.FindNode(uLoop);
	ZENITH_ASSERT_NOT_NULL(pxLoop);
	if (pxLoop == nullptr)
	{
		return;
	}
	pxLoop->SetInputForTest(Zenith_GraphNode_Loop::uPIN_Count, CorePin_Int(2));

	FireOneUpdate(xGraph);
	ZENITH_ASSERT_EQ_FLOAT(xGraph.GetBlackboard().GetFloat("loopCounter"), 2.0f, 0.0001f,
		"the body ran neither the wire's 2 times, nor the const 9, nor the var 7");
	ZENITH_ASSERT_TRUE(xGraph.GetBlackboard().GetBool("loopDone"), "the done chain never ran");
	ZENITH_ASSERT_EQ(pxLoop->GetFallbackUseCountForTest(Zenith_GraphNode_Loop::uPIN_Count), 0u);
	ZENITH_ASSERT_EQ(pxLoop->GetBadAccessWarningCountForTest(), 0u);
}

// ★ THE ANY PAYLOAD PIN, and the whole point of TryGetInput collapsing the old
// `m_strPayloadVar.empty() ? nullptr : TryGetValue(...)`: a WIRE supplies a
// payload even with an EMPTY var name, which is exactly how a wired author leaves
// it. The target's only attachment seam is AddGraphByAssetPath, so the handler is
// a SAVED .bgraph whose OnCustomEvent stashes the payload - the observable is the
// TARGET graph's blackboard, not this node's.
ZENITH_TEST(CorePinRuntime, Wired_FireCustomEventPayloadFromWire)
{
	const u_int uPayload = Zenith_GraphNode_FireCustomEvent::uPIN_Payload;
	const std::string strHandler = CorePin_SavePayloadHandlerAsset("UnitTest_CorePinPayload.bgraph", "CorePinPayload");

	Zenith_TempScene xScene("TestCorePinPayloadScene");

	{	// (a) WIRE with an EMPTY var name -> the payload arrives.
		Zenith_Entity xTarget = xScene.CreateEntity("CorePinPayloadWire");
		Zenith_BehaviourGraph* pxHandler =
			xTarget.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strHandler.c_str());
		ZENITH_ASSERT_NOT_NULL(pxHandler);
		if (pxHandler == nullptr)
		{
			return;
		}

		Zenith_GraphBlackboard xBB;
		Zenith_GraphNode_FireCustomEvent xNode;
		xNode.m_strEventName = "CorePinPayload";
		xNode.m_strTargetVar = "";		// self
		xNode.m_strPayloadVar = "";		// EMPTY - today's read would have sent nothing
		xNode.SetInputForTest(uPayload, CorePin_Float(5.0f));

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_xSelf = xTarget;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(pxHandler->GetBlackboard().GetFloat("payload"), 5.0f, 0.0001f,
			"the wired payload never reached the handler");
		ZENITH_ASSERT_TRUE(pxHandler->GetBlackboard().GetBool("handlerRan"),
			"the handler action did not run for the wired payload");
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uPayload), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{	// (b) BOUND BUT ABSENT -> one census line, and NO payload (as today).
		Zenith_Entity xTarget = xScene.CreateEntity("CorePinPayloadAbsent");
		Zenith_BehaviourGraph* pxHandler =
			xTarget.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strHandler.c_str());
		ZENITH_ASSERT_NOT_NULL(pxHandler);
		if (pxHandler == nullptr)
		{
			return;
		}

		Zenith_GraphBlackboard xBB;
		Zenith_GraphNode_FireCustomEvent xNode;
		xNode.m_strEventName = "CorePinPayload";
		xNode.m_strPayloadVar = "noSuchPayload";

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_xSelf = xTarget;
		ZENITH_ASSERT_NULL(pxHandler->GetBlackboard().TryGetValue("handlerRan"));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_NULL(pxHandler->GetBlackboard().TryGetValue("payload"),
			"an ABSENT payload variable must deliver no payload at all");
		ZENITH_ASSERT_TRUE(pxHandler->GetBlackboard().GetBool("handlerRan"),
			"an absent payload must still dispatch to the existing handler action");
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uPayload), 1u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{	// (c) BOUND AND PRESENT -> the variable's value, unchanged.
		Zenith_Entity xTarget = xScene.CreateEntity("CorePinPayloadPresent");
		Zenith_BehaviourGraph* pxHandler =
			xTarget.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strHandler.c_str());
		ZENITH_ASSERT_NOT_NULL(pxHandler);
		if (pxHandler == nullptr)
		{
			return;
		}

		Zenith_GraphBlackboard xBB;
		xBB.SetValue("pay", CorePin_Float(7.0f));
		Zenith_GraphNode_FireCustomEvent xNode;
		xNode.m_strEventName = "CorePinPayload";
		xNode.m_strPayloadVar = "pay";

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_xSelf = xTarget;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(pxHandler->GetBlackboard().GetFloat("payload"), 7.0f, 0.0001f);
		ZENITH_ASSERT_TRUE(pxHandler->GetBlackboard().GetBool("handlerRan"));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uPayload), 1u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// The Broadcast twin. Its Payload is pin 0 (there is no Target), and the node
// reaches EVERY GraphComponent in every loaded scene - so the event name is
// unique to this row and the assertion is on its own target only.
ZENITH_TEST(CorePinRuntime, Wired_BroadcastCustomEventPayloadFromWire)
{
	const u_int uPayload = Zenith_GraphNode_BroadcastCustomEvent::uPIN_Payload;
	const std::string strHandler =
		CorePin_SavePayloadHandlerAsset("UnitTest_CorePinBroadcast.bgraph", "CorePinBroadcastPayload");

	// Broadcast visits every live GraphComponent. Each leg therefore owns a fresh
	// scene, target, handler graph, blackboard and node, so no earlier receiver
	// can satisfy a later parity assertion.
	{ // (a) WIRE with EMPTY var: only the wire can supply 5.
		Zenith_TempScene xScene("TestCorePinBroadcastWireScene");
		Zenith_Entity xTarget = xScene.CreateEntity("CorePinBroadcastWireTarget");
		Zenith_BehaviourGraph* pxHandler =
			xTarget.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strHandler.c_str());
		ZENITH_ASSERT_NOT_NULL(pxHandler);
		if (pxHandler == nullptr)
		{
			return;
		}

		Zenith_GraphBlackboard xBB;
		Zenith_GraphNode_BroadcastCustomEvent xNode;
		xNode.m_strEventName = "CorePinBroadcastPayload";
		xNode.m_strPayloadVar = "";
		xNode.SetInputForTest(uPayload, CorePin_Float(5.0f));
		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(pxHandler->GetBlackboard().GetFloat("payload"), 5.0f, 0.0001f);
		ZENITH_ASSERT_TRUE(pxHandler->GetBlackboard().GetBool("handlerRan"));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uPayload), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{ // (b) BOUND BUT ABSENT: null payload, but the handler action still runs.
		Zenith_TempScene xScene("TestCorePinBroadcastAbsentScene");
		Zenith_Entity xTarget = xScene.CreateEntity("CorePinBroadcastAbsentTarget");
		Zenith_BehaviourGraph* pxHandler =
			xTarget.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strHandler.c_str());
		ZENITH_ASSERT_NOT_NULL(pxHandler);
		if (pxHandler == nullptr)
		{
			return;
		}

		Zenith_GraphBlackboard xBB;
		Zenith_GraphNode_BroadcastCustomEvent xNode;
		xNode.m_strEventName = "CorePinBroadcastPayload";
		xNode.m_strPayloadVar = "missingPayload";
		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_NULL(pxHandler->GetBlackboard().TryGetValue("handlerRan"));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_NULL(pxHandler->GetBlackboard().TryGetValue("payload"));
		ZENITH_ASSERT_TRUE(pxHandler->GetBlackboard().GetBool("handlerRan"));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uPayload), 1u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{ // (c) BOUND AND PRESENT: the distinct blackboard value is delivered.
		Zenith_TempScene xScene("TestCorePinBroadcastPresentScene");
		Zenith_Entity xTarget = xScene.CreateEntity("CorePinBroadcastPresentTarget");
		Zenith_BehaviourGraph* pxHandler =
			xTarget.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strHandler.c_str());
		ZENITH_ASSERT_NOT_NULL(pxHandler);
		if (pxHandler == nullptr)
		{
			return;
		}

		Zenith_GraphBlackboard xBB;
		xBB.SetValue("broadcastPayload", CorePin_Float(11.0f));
		Zenith_GraphNode_BroadcastCustomEvent xNode;
		xNode.m_strEventName = "CorePinBroadcastPayload";
		xNode.m_strPayloadVar = "broadcastPayload";
		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(pxHandler->GetBlackboard().GetFloat("payload"), 11.0f, 0.0001f);
		ZENITH_ASSERT_TRUE(pxHandler->GetBlackboard().GetBool("handlerRan"));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uPayload), 1u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// The OUTPUT by VALUE: self's EXACT packed id. ★ Asserted NON-ZERO first, because
// packed 0 = {index 0, generation 0} is both a LEGAL entity id and the ENTITY_ID
// slot's own stamped zero - a fixture that happened to pack to 0 would make the
// comparison unfalsifiable, so the row takes a second entity in that case.
ZENITH_TEST(CorePinRuntime, Output_StoreSelfEntityIDByValue)
{
	Zenith_TempScene xScene("TestCorePinStoreSelfScene");
	Zenith_Entity xEntity = xScene.CreateEntity("CorePinSelf");
	if (xEntity.GetEntityID().GetPacked() == 0ull)
	{
		xEntity = xScene.CreateEntity("CorePinSelfSecond");
	}
	const u_int64 ulSelf = xEntity.GetEntityID().GetPacked();
	ZENITH_ASSERT_NE(ulSelf, 0ull,
		"a packed id of 0 is also the ENTITY_ID slot's stamped zero - this row could prove nothing");

	{	// (a) the named default: slot AND blackboard carry the exact id.
		Zenith_GraphBlackboard xBB;
		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_xSelf = xEntity;

		Zenith_GraphNode_StoreSelfEntityID xNode;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(CorePin_SlotPackedEntity(
			xNode.GetOutputForTest(Zenith_GraphNode_StoreSelfEntityID::uPIN_Variable), "StoreSelfEntityID.Variable"),
			ulSelf);
		ZENITH_ASSERT_EQ(xBB.GetPackedEntityID("self"), ulSelf);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{	// (b) THE `""` DIVERGENCE: the slot still carries the id; no blackboard
		//     variable named "" is created, where today's SetValue created one.
		Zenith_GraphBlackboard xBB;
		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		xCtx.m_xSelf = xEntity;

		Zenith_GraphNode_StoreSelfEntityID xNode;
		xNode.m_strVariable = "";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(CorePin_SlotPackedEntity(
			xNode.GetOutputForTest(Zenith_GraphNode_StoreSelfEntityID::uPIN_Variable), "StoreSelfEntityID.Variable"),
			ulSelf);
		ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
		ZENITH_ASSERT_EQ(xBB.GetCount(), 0u, "an unnamed OUTPUT created a blackboard variable");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{	// (c) SHAPE A: an invalid self FAILS above every accessor, so a FRESH
		//     instance has built no pin state at all - the slot is ABSENT, not zero.
		Zenith_GraphBlackboard xBB;
		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;		// m_xSelf left default-constructed = invalid

		Zenith_GraphNode_StoreSelfEntityID xNode;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xNode.GetOutputForTest(Zenith_GraphNode_StoreSelfEntityID::uPIN_Variable),
			"the FAILURE is above every accessor, so no pin state was ever built");
		ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// The CENSUS observable. Only a MIGRATED node reaches the transitional var-name
// fallback, and it logs ONE line per (instance, pin) however hot the chain is -
// which is what makes "zero FALLBACK lines in a SUITE boot log" C-1's precondition
// rather than a guess. CompareBlackboardFloat.Value carries it because its var
// name defaults to "value", NON-empty: this node logs whether or not an author
// named anything, and that is why this TU's census rises everywhere.
ZENITH_TEST(CorePinRuntime, Fallback_CountsOncePerPin)
{
	const u_int uValue = Zenith_GraphNode_CompareBlackboardFloat::uPIN_Value;

	Zenith_GraphBlackboard xBB;
	xBB.SetValue("value", CorePin_Float(1.0f));		// the node's OWN default var name

	Zenith_GraphNode_CompareBlackboardFloat xNode;
	xNode.m_fCompareTo = 5.0f;
	xNode.m_iOp = GRAPH_COMPARE_FLOAT_OP_LESS;

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uValue), 0u);
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);

	for (u_int u = 0; u < 2u; ++u)
	{
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	}
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uValue), 1u, "two reads must log ONE census line");
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	ZENITH_ASSERT_TRUE(xBB.GetBool("result"));
}

// The row no existing test in this file covers, and the one ResolveInput
// implements differently from a naive blackboard read: a var-or-const pin whose
// variable is MISSING - or present with the WRONG TAG - falls back to the CONST,
// not to a type zero. FRESH node per leg: the binding latches its var name once.
ZENITH_TEST(CorePinRuntime, Fallback_VarBoundButAbsentTakesTheConst)
{
	const u_int uCompareTo = Zenith_GraphNode_CompareBlackboardFloat::uPIN_CompareTo;

	{	// (a) MISSING: CompareTo = the const 7, so 5 < 7 is TRUE while a
		//     type-zero fallback would make 5 < 0 false. This distinguishes the
		//     const path from both a missing read and the stamped false result.
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("v", CorePin_Float(5.0f));

		Zenith_GraphNode_CompareBlackboardFloat xNode;
		xNode.m_strVar = "v";
		xNode.m_fCompareTo = 7.0f;
		xNode.m_strCompareVar = "missing";
		xNode.m_iOp = GRAPH_COMPARE_FLOAT_OP_LESS;

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_TRUE(CorePin_SlotBool(
			xNode.GetOutputForTest(Zenith_GraphNode_CompareBlackboardFloat::uPIN_Result), "CompareFloat.Result"));
		ZENITH_ASSERT_TRUE(xBB.GetBool("result"));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uCompareTo), 1u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
		ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(uCompareTo), 0u);
	}

	{	// (b) WRONG TAG: an INT32 in a FLOAT pin's variable. The const 7 wins, so
		//     5 > 7 is FALSE; had the INT32 42 been read it would be 5 > 42, also
		//     false - so the discriminator is a const BELOW the value: const 7,
		//     variable 2 (as INT32), value 5. Reading the variable would give TRUE.
		Zenith_GraphBlackboard xBB;
		xBB.SetValue("v", CorePin_Float(5.0f));
		xBB.SetValue("wrongType", CorePin_Int(2));

		Zenith_GraphNode_CompareBlackboardFloat xNode;
		xNode.m_strVar = "v";
		xNode.m_fCompareTo = 7.0f;
		xNode.m_strCompareVar = "wrongType";
		xNode.m_iOp = GRAPH_COMPARE_FLOAT_OP_GREATER;

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_FALSE(xBB.GetBool("result", true),
			"a WRONG-TAGGED variable must fall back to the const 7, not be read as 2");
		// A var-name fallback is not a wire: a mismatching tag there is silent,
		// exactly as the old typed blackboard getter's default was.
		ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(uCompareTo), 0u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}
}

// ★ EVERY INPUT READ IN THIS TU THAT SITS BELOW A GUARD MUST STAY THERE. A
// FAILURE-before-read execution logs NO census line for a var-BOUND pin and, in a
// graph, pulls no producer; the positive control is the identical configuration
// on a target that passes the guard. TranslateEntity carries it because its
// transform guard is reachable headless: Zenith_SceneSystem::CreateEntityBare
// skips the default-components hook, so the entity has no Zenith_TransformComponent.
ZENITH_TEST(CorePinRuntime, Fallback_GuardedFailureDoesNotReadInputs)
{
	const u_int uUnits = Zenith_GraphNode_TranslateEntity::uPIN_UnitsPerSecond;

	Zenith_TempScene xScene("TestCorePinGuardScene");
	Zenith_Entity xBare = g_xEngine.Scenes().CreateEntityBare(xScene.Scene(), "CorePinNoTransform");
	Zenith_Entity xWithTransform = xScene.CreateEntity("CorePinWithTransform");
	xWithTransform.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f));
	ZENITH_ASSERT_NULL(xBare.TryGetComponent<Zenith_TransformComponent>(),
		"CreateEntityBare gave the entity a transform - the guarded-FAILURE leg would be vacuous");

	Zenith_GraphBlackboard xBB;
	xBB.SetValue("units", CorePin_Vec3(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f)));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	xCtx.m_fDt = 1.0f;

	{	// LEG A: no transform -> FAILURE, and the var-bound pin was never read.
		//     ★ m_strUnitsVar defaults EMPTY, so the row has to ASSIGN it - an
		//     unbound pin could not log whatever the code did.
		Zenith_GraphNode_TranslateEntity xNode;
		xNode.m_strUnitsVar = "units";
		xCtx.m_xSelf = xBare;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uUnits), 0u,
			"the UnitsPerSecond read moved ABOVE the transform guard - a failed node pulled its input");
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	}

	{	// LEG B, THE POSITIVE CONTROL: same configuration, a target with a
		//     transform -> SUCCESS and exactly one census line.
		Zenith_GraphNode_TranslateEntity xNode;
		xNode.m_strUnitsVar = "units";
		xCtx.m_xSelf = xWithTransform;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uUnits), 1u);
		ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
		Zenith_Maths::Vector3 xPosition;
		xWithTransform.GetComponent<Zenith_TransformComponent>().GetPosition(xPosition);
		ZENITH_ASSERT_NEAR_VEC3(xPosition, Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f), 0.001f);
	}
}

// SHAPE B, the other direction: Compare*'s two reads sit ABOVE the op switch, so
// an out-of-range op code FAILS having already read both pins. The existing
// GraphNodeOps.CompareBlackboardFloatOutOfRangeFails row pins the STATUS; this one
// pins where the reads happened relative to it.
ZENITH_TEST(CorePinRuntime, Fallback_BadOpCodeFailsAfterBothReads)
{
	const u_int uValue = Zenith_GraphNode_CompareBlackboardFloat::uPIN_Value;
	const u_int uCompareTo = Zenith_GraphNode_CompareBlackboardFloat::uPIN_CompareTo;

	Zenith_GraphBlackboard xBB;
	xBB.SetValue("v", CorePin_Float(5.0f));
	xBB.SetValue("cmp", CorePin_Float(1.0f));
	const u_int uBefore = xBB.GetCount();

	Zenith_GraphNode_CompareBlackboardFloat xNode;
	xNode.m_strVar = "v";
	xNode.m_strCompareVar = "cmp";
	xNode.m_iOp = 99;

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uValue), 1u,
		"the Value read moved BELOW the op switch");
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	ZENITH_ASSERT_EQ(xNode.GetFallbackUseCountForTest(uCompareTo), 1u,
		"the CompareTo read moved BELOW the op switch");
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
	// The WRITE is below the switch, so nothing was latched or dual-written. ★ The
	// Result slot is not ABSENT here - pin state WAS built by the two reads. On
	// this FRESH invalid-op instance it holds the typed slot's stamped false; on
	// a previously successful instance the prior output is retained instead.
	ZENITH_ASSERT_FALSE(CorePin_SlotBool(
		xNode.GetOutputForTest(Zenith_GraphNode_CompareBlackboardFloat::uPIN_Result), "CompareFloat.Result"),
		"a bad op code latched a Result");
	ZENITH_ASSERT_EQ(xBB.GetCount(), uBefore, "a bad op code wrote the result variable");
	ZENITH_ASSERT_NULL(xBB.TryGetValue("result"));
}

#endif // ZENITH_TESTING
