//------------------------------------------------------------------------------
// Op-execution tests for the Blackboard/Maths node TU (CompareBlackboardEntity,
// MathBlackboardFloat, LogicBlackboardBool, the list mutators). Included at the
// bottom of
// Zenith_GraphNode_Registration_Math.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes are still in scope. The enum-value wall lives
// in the sibling Zenith_GraphNode_Registration.Tests.inl.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Scripting/Zenith_GraphBlackboard.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_GraphNodeFailurePin.TestHarness.inl"

// Runs MathBlackboardFloat "r = v <op> operand" and returns r; asserts SUCCESS.
static float RunMathFloat(float fVal, int32_t iOp, float fOperand)
{
	Zenith_GraphBlackboard xBB;
	Zenith_PropertyValue xV; xV.SetFloat(fVal);
	xBB.SetValue("v", xV);
	Zenith_GraphNode_MathBlackboardFloat xNode;
	xNode.m_strVar = "v";
	xNode.m_iOp = iOp;
	xNode.m_fOperand = fOperand;
	xNode.m_strResultVar = "r";
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	return xBB.GetFloat("r");
}

// Same, but returns the node status (for the fail paths that must NOT write).
static int RunMathFloatStatus(float fVal, int32_t iOp, float fOperand)
{
	Zenith_GraphBlackboard xBB;
	Zenith_PropertyValue xV; xV.SetFloat(fVal);
	xBB.SetValue("v", xV);
	Zenith_GraphNode_MathBlackboardFloat xNode;
	xNode.m_strVar = "v";
	xNode.m_iOp = iOp;
	xNode.m_fOperand = fOperand;
	xNode.m_strResultVar = "r";
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	return static_cast<int>(xNode.Execute(xCtx));
}

ZENITH_TEST(GraphNodeOps, MathBlackboardFloatAllOps)
{
	ZENITH_ASSERT_EQ_FLOAT(RunMathFloat(5.0f, GRAPH_MATH_FLOAT_OP_SUBTRACT, 3.0f), 2.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(RunMathFloat(5.0f, GRAPH_MATH_FLOAT_OP_MULTIPLY, 3.0f), 15.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(RunMathFloat(6.0f, GRAPH_MATH_FLOAT_OP_DIVIDE, 2.0f), 3.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(RunMathFloat(7.0f, GRAPH_MATH_FLOAT_OP_MODULO, 3.0f), 1.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(RunMathFloat(5.0f, GRAPH_MATH_FLOAT_OP_MIN, 3.0f), 3.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(RunMathFloat(5.0f, GRAPH_MATH_FLOAT_OP_MAX, 3.0f), 5.0f, 0.0001f);
	// Unary ops ignore the operand.
	ZENITH_ASSERT_EQ_FLOAT(RunMathFloat(-5.0f, GRAPH_MATH_FLOAT_OP_ABS, 999.0f), 5.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(RunMathFloat(0.0f, GRAPH_MATH_FLOAT_OP_SIN, 999.0f), 0.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(RunMathFloat(0.0f, GRAPH_MATH_FLOAT_OP_COS, 999.0f), 1.0f, 0.0001f);
}

ZENITH_TEST(GraphNodeOps, MathBlackboardFloatDivModByZeroAndOutOfRangeFail)
{
	// DIVIDE / MODULO by zero must FAIL loudly (not NaN-quietly).
	ZENITH_ASSERT_EQ(RunMathFloatStatus(6.0f, GRAPH_MATH_FLOAT_OP_DIVIDE, 0.0f), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(RunMathFloatStatus(6.0f, GRAPH_MATH_FLOAT_OP_MODULO, 0.0f), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	// Out-of-range op hits the switch default: -> FAILURE.
	ZENITH_ASSERT_EQ(RunMathFloatStatus(1.0f, 99, 1.0f), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
}

// Runs CompareBlackboardEntity over two pre-seeded packed IDs; asserts SUCCESS
// (the node is a ternary that never fails) and returns its bool result.
static bool RunCompareEntity(u_int64 ulA, u_int64 ulB, int32_t iOp)
{
	Zenith_GraphBlackboard xBB;
	Zenith_PropertyValue xA; xA.SetPackedEntityID(ulA); xBB.SetValue("a", xA);
	Zenith_PropertyValue xB; xB.SetPackedEntityID(ulB); xBB.SetValue("b", xB);
	Zenith_GraphNode_CompareBlackboardEntity xNode;
	xNode.m_strVarA = "a";
	xNode.m_strVarB = "b";
	xNode.m_iOp = iOp;
	xNode.m_strResultVar = "r";
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	return xBB.GetBool("r", false);
}

ZENITH_TEST(GraphNodeOps, CompareBlackboardEntityEqualityAndFallback)
{
	const u_int64 ulX = 0x0000000100000001ull;
	const u_int64 ulY = 0x0000000200000002ull;

	// EQUAL
	ZENITH_ASSERT_TRUE(RunCompareEntity(ulX, ulX, GRAPH_ENTITY_COMPARE_OP_EQUAL));
	ZENITH_ASSERT_FALSE(RunCompareEntity(ulX, ulY, GRAPH_ENTITY_COMPARE_OP_EQUAL));
	// NOT_EQUAL
	ZENITH_ASSERT_FALSE(RunCompareEntity(ulX, ulX, GRAPH_ENTITY_COMPARE_OP_NOT_EQUAL));
	ZENITH_ASSERT_TRUE(RunCompareEntity(ulX, ulY, GRAPH_ENTITY_COMPARE_OP_NOT_EQUAL));
	// Out-of-range op: the ternary treats anything != NOT_EQUAL as EQUAL (NOT a
	// defaulting failure) - this is the deliberately-preserved fallback.
	ZENITH_ASSERT_TRUE(RunCompareEntity(ulX, ulX, 99));
	ZENITH_ASSERT_FALSE(RunCompareEntity(ulX, ulY, 99));
}

//==============================================================================
// LogicBlackboardBool
//
// Every clause of the node's documented contract gets a case here, because the
// node has no shape a caller can inspect: an operand list is a STRING, and a
// mis-parsed one produces a plausible bool instead of an error.
//
// A FRESH node per run, deliberately. The node caches its parsed operand list
// keyed on m_strVars, so reusing one instance across cases would still be
// correct - but a helper that shared one would stop being able to prove that.
//==============================================================================

// Runs LogicBlackboardBool over a blackboard the caller pre-seeded, returning
// the node STATUS; the result bool (when written) lands in "r".
static int RunLogicBoolStatus(
	Zenith_GraphBlackboard& xBB,
	const char* szVars,
	int32_t iOp,
	bool bInvert = false,
	bool bMissingIsTrue = false)
{
	Zenith_GraphNode_LogicBlackboardBool xNode;
	xNode.m_strVars = szVars;
	xNode.m_iOp = iOp;
	xNode.m_bInvert = bInvert;
	xNode.m_bMissingIsTrue = bMissingIsTrue;
	xNode.m_strResultVar = "r";
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	return static_cast<int>(xNode.Execute(xCtx));
}

// The SUCCESS-path twin: asserts the node reported SUCCESS and returns "r".
static bool RunLogicBool(
	Zenith_GraphBlackboard& xBB,
	const char* szVars,
	int32_t iOp,
	bool bInvert = false,
	bool bMissingIsTrue = false)
{
	// Poison the result first: a node that wrote nothing must not read as false
	// by accident.
	Zenith_PropertyValue xPoison; xPoison.SetBool(true);
	xBB.SetValue("r", xPoison);
	ZENITH_ASSERT_EQ(
		RunLogicBoolStatus(xBB, szVars, iOp, bInvert, bMissingIsTrue),
		static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	return xBB.GetBool("r", true);
}

// Seeds up to three bools named a, b, c.
static void SeedBools(Zenith_GraphBlackboard& xBB, bool bA, bool bB, bool bC)
{
	Zenith_PropertyValue xV;
	xV.SetBool(bA); xBB.SetValue("a", xV);
	xV.SetBool(bB); xBB.SetValue("b", xV);
	xV.SetBool(bC); xBB.SetValue("c", xV);
}

ZENITH_TEST(GraphNodeOps, LogicBlackboardBoolTruthTables)
{
	Zenith_GraphBlackboard xBB;

	// --- two operands: the full 2x2 table for all three ops -----------------
	const bool abTable[4][2] = { { false, false }, { false, true }, { true, false }, { true, true } };
	for (u_int u = 0; u < 4u; ++u)
	{
		const bool bA = abTable[u][0];
		const bool bB = abTable[u][1];
		SeedBools(xBB, bA, bB, false);
		ZENITH_ASSERT_EQ(RunLogicBool(xBB, "a,b", GRAPH_LOGIC_BOOL_OP_AND) ? 1 : 0, (bA && bB) ? 1 : 0,
			"AND(%d,%d)", bA ? 1 : 0, bB ? 1 : 0);
		ZENITH_ASSERT_EQ(RunLogicBool(xBB, "a,b", GRAPH_LOGIC_BOOL_OP_OR) ? 1 : 0, (bA || bB) ? 1 : 0,
			"OR(%d,%d)", bA ? 1 : 0, bB ? 1 : 0);
		ZENITH_ASSERT_EQ(RunLogicBool(xBB, "a,b", GRAPH_LOGIC_BOOL_OP_XOR) ? 1 : 0, (bA != bB) ? 1 : 0,
			"XOR(%d,%d)", bA ? 1 : 0, bB ? 1 : 0);
	}

	// --- one operand: AND and OR are both the IDENTITY ----------------------
	// This is what makes m_bInvert a clean NOT, so it is contract, not trivia.
	SeedBools(xBB, true, false, false);
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a", GRAPH_LOGIC_BOOL_OP_AND));
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a", GRAPH_LOGIC_BOOL_OP_OR));
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a", GRAPH_LOGIC_BOOL_OP_XOR));
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "b", GRAPH_LOGIC_BOOL_OP_AND));
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "b", GRAPH_LOGIC_BOOL_OP_OR));
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "b", GRAPH_LOGIC_BOOL_OP_XOR));
}

ZENITH_TEST(GraphNodeOps, LogicBlackboardBoolXorIsParityNotExactlyOne)
{
	// THE case that separates the two readings. At three operands "exactly one"
	// and "odd count" agree everywhere EXCEPT (true,true,true): parity says
	// true, exactly-one says false. A binary-only test could never see this.
	Zenith_GraphBlackboard xBB;

	SeedBools(xBB, true, true, true);
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a,b,c", GRAPH_LOGIC_BOOL_OP_XOR), "XOR(1,1,1) must be parity-true");

	SeedBools(xBB, true, true, false);
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a,b,c", GRAPH_LOGIC_BOOL_OP_XOR));

	SeedBools(xBB, true, false, false);
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a,b,c", GRAPH_LOGIC_BOOL_OP_XOR));

	SeedBools(xBB, false, false, false);
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a,b,c", GRAPH_LOGIC_BOOL_OP_XOR));

	// AND / OR at three operands, for completeness of the N-ary claim.
	SeedBools(xBB, true, true, false);
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a,b,c", GRAPH_LOGIC_BOOL_OP_AND));
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a,b,c", GRAPH_LOGIC_BOOL_OP_OR));
	SeedBools(xBB, true, true, true);
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a,b,c", GRAPH_LOGIC_BOOL_OP_AND));
	SeedBools(xBB, false, false, false);
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a,b,c", GRAPH_LOGIC_BOOL_OP_OR));
}

ZENITH_TEST(GraphNodeOps, LogicBlackboardBoolInvertGivesNotNandNor)
{
	Zenith_GraphBlackboard xBB;

	// NOT: one operand + invert.
	SeedBools(xBB, true, false, false);
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a", GRAPH_LOGIC_BOOL_OP_AND, /*invert*/ true));
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "b", GRAPH_LOGIC_BOOL_OP_AND, /*invert*/ true));

	// NAND / NOR.
	SeedBools(xBB, true, true, false);
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a,b", GRAPH_LOGIC_BOOL_OP_AND, /*invert*/ true));
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a,b", GRAPH_LOGIC_BOOL_OP_OR, /*invert*/ true));
	SeedBools(xBB, false, false, false);
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a,b", GRAPH_LOGIC_BOOL_OP_AND, /*invert*/ true));
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a,b", GRAPH_LOGIC_BOOL_OP_OR, /*invert*/ true));
}

ZENITH_TEST(GraphNodeOps, LogicBlackboardBoolMissingOperandTakesTheDefault)
{
	Zenith_GraphBlackboard xBB;
	SeedBools(xBB, true, true, false);

	// "gone" is absent entirely.
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a,gone", GRAPH_LOGIC_BOOL_OP_AND, false, /*missingIsTrue*/ false));
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a,gone", GRAPH_LOGIC_BOOL_OP_AND, false, /*missingIsTrue*/ true));

	// A PRESENT but wrongly-typed operand takes the same route - GetBool's
	// documented contract returns the default on a type mismatch, and the node
	// deliberately does not distinguish the two cases.
	Zenith_PropertyValue xInt; xInt.SetInt32(7);
	xBB.SetValue("num", xInt);
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a,num", GRAPH_LOGIC_BOOL_OP_AND, false, /*missingIsTrue*/ false));
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a,num", GRAPH_LOGIC_BOOL_OP_AND, false, /*missingIsTrue*/ true));
}

ZENITH_TEST(GraphNodeOps, LogicBlackboardBoolTokenisationIsVerbatim)
{
	Zenith_GraphBlackboard xBB;
	SeedBools(xBB, true, true, false);

	// NO TRIMMING. "a, b" is "a" and " b"; " b" is absent, so with
	// missingIsTrue=false the AND is false even though 'a' and 'b' are both
	// true. Pinned so the choice is recorded rather than rediscovered - see
	// Zenith_GraphNode_ParseCommaList for why trimming is not an option.
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a, b", GRAPH_LOGIC_BOOL_OP_AND));
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a, b", GRAPH_LOGIC_BOOL_OP_AND, false, /*missingIsTrue*/ true));

	// EMPTY TOKENS ARE SKIPPED: "a,,b" is two operands, and a trailing comma
	// adds none. If either produced a third (empty-named, absent) operand the
	// AND below would be false.
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a,,b", GRAPH_LOGIC_BOOL_OP_AND));
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, "a,b,", GRAPH_LOGIC_BOOL_OP_AND));
	ZENITH_ASSERT_TRUE(RunLogicBool(xBB, ",a,b", GRAPH_LOGIC_BOOL_OP_AND));
	// ...and the same list under XOR still sees TWO operands (parity even).
	ZENITH_ASSERT_FALSE(RunLogicBool(xBB, "a,,b", GRAPH_LOGIC_BOOL_OP_XOR));
}

ZENITH_TEST(GraphNodeOps, LogicBlackboardBoolEmptyListAndBadOpFail)
{
	Zenith_GraphBlackboard xBB;
	SeedBools(xBB, true, true, false);

	// An EMPTY operand list is FAILURE - not the vacuous AND (true) and not
	// false. An unauthored node aborts its chain instead of writing something
	// plausible.
	ZENITH_ASSERT_EQ(RunLogicBoolStatus(xBB, "", GRAPH_LOGIC_BOOL_OP_AND), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(RunLogicBoolStatus(xBB, ",,,", GRAPH_LOGIC_BOOL_OP_OR), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));

	// ...and it writes NOTHING on the way out.
	Zenith_PropertyValue xPoison; xPoison.SetBool(true);
	xBB.SetValue("r", xPoison);
	ZENITH_ASSERT_EQ(RunLogicBoolStatus(xBB, "", GRAPH_LOGIC_BOOL_OP_AND), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_TRUE(xBB.GetBool("r", false), "a failed LogicBlackboardBool must not touch its result var");

	// Out-of-range op hits the switch default -> FAILURE, like every sibling.
	ZENITH_ASSERT_EQ(RunLogicBoolStatus(xBB, "a,b", 99), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(RunLogicBoolStatus(xBB, "a,b", -1), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_TRUE(xBB.GetBool("r", false), "an out-of-range op must not touch the result var either");
}

ZENITH_TEST(GraphNodeOps, LogicBlackboardBoolRereadsAnEditedOperandList)
{
	// The operand list is cached, keyed on the property string. The editor
	// property panel writes m_strVars on a LIVE node instance, so a one-shot
	// "parsed" latch would keep evaluating the list the author just replaced.
	Zenith_GraphBlackboard xBB;
	SeedBools(xBB, true, false, false);

	Zenith_GraphNode_LogicBlackboardBool xNode;
	xNode.m_strVars = "a";
	xNode.m_iOp = GRAPH_LOGIC_BOOL_OP_AND;
	xNode.m_strResultVar = "r";
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_TRUE(xBB.GetBool("r", false));

	xNode.m_strVars = "b";	// the edit
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_FALSE(xBB.GetBool("r", true), "the node kept its stale operand list after m_strVars changed");
}

//==============================================================================
// ListAdd / ListRemoveAt / ListClear
//==============================================================================

// Appends an int32 through the real node and returns its status.
static int RunListAddInt(Zenith_GraphBlackboard& xBB, const char* szListVar, const char* szValueVar)
{
	Zenith_GraphNode_ListAdd xNode;
	xNode.m_strListVar = szListVar;
	xNode.m_strValueVar = szValueVar;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	return static_cast<int>(xNode.Execute(xCtx));
}

static int RunListRemoveAt(Zenith_GraphBlackboard& xBB, const char* szListVar, int32_t iIndex, const char* szIndexVar = "")
{
	Zenith_GraphNode_ListRemoveAt xNode;
	xNode.m_strListVar = szListVar;
	xNode.m_iIndex = iIndex;
	xNode.m_strIndexVar = szIndexVar;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	return static_cast<int>(xNode.Execute(xCtx));
}

static int RunListClear(Zenith_GraphBlackboard& xBB, const char* szListVar)
{
	Zenith_GraphNode_ListClear xNode;
	xNode.m_strListVar = szListVar;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	return static_cast<int>(xNode.Execute(xCtx));
}

// Seeds "item" then appends it, so the helper below builds a list of ints
// through exactly the path a graph would.
static void AppendInt(Zenith_GraphBlackboard& xBB, int32_t iValue)
{
	Zenith_PropertyValue xV; xV.SetInt32(iValue);
	xBB.SetValue("item", xV);
	ZENITH_ASSERT_EQ(RunListAddInt(xBB, "bag", "item"), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
}

static int32_t ListElement(const Zenith_GraphBlackboard& xBB, const char* szListVar, u_int uIndex)
{
	const Zenith_Vector<Zenith_PropertyValue>* pxList = xBB.TryGetList(szListVar);
	if (pxList == nullptr || uIndex >= pxList->GetSize())
	{
		return -1;
	}
	return pxList->Get(uIndex).GetInt32();
}

ZENITH_TEST(GraphNodeOps, ListAddAppendsAndFailsOnAMissingSource)
{
	Zenith_GraphBlackboard xBB;

	// The list does not exist yet: the first add creates it.
	ZENITH_ASSERT_NULL(xBB.TryGetList("bag"));
	AppendInt(xBB, 10);
	ZENITH_ASSERT_NOT_NULL(xBB.TryGetList("bag"));
	ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 1u);

	AppendInt(xBB, 20);
	AppendInt(xBB, 30);
	ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 3u);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 0), 10);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 2), 30);

	// An ABSENT source variable FAILS and appends nothing - a silently
	// defaulted element would be an entry nobody authored.
	ZENITH_ASSERT_EQ(RunListAddInt(xBB, "bag", "nope"), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 3u);
}

ZENITH_TEST(GraphNodeOps, ListRemoveAtPreservesOrder)
{
	Zenith_GraphBlackboard xBB;
	AppendInt(xBB, 10);
	AppendInt(xBB, 20);
	AppendInt(xBB, 30);

	// Three elements first, as the ordinary case.
	//
	// ★ THREE IS NOT ENOUGH TO SEPARATE Remove FROM RemoveSwap. Dropping index
	// 1 of [10,20,30] leaves [10,30] EITHER WAY - the swap pulls the last
	// element into the hole, and the last element IS the one after it. Only a
	// list with two elements past the hole can tell them apart, which is what
	// the four-element case below is for.
	ZENITH_ASSERT_EQ(RunListRemoveAt(xBB, "bag", 1), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 2u);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 0), 10);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 1), 30);

	// Four elements, drop index 1: order-preserving gives [10, 30, 40];
	// a swap-remove would give [10, 40, 30].
	ZENITH_ASSERT_EQ(RunListClear(xBB, "bag"), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	AppendInt(xBB, 10);
	AppendInt(xBB, 20);
	AppendInt(xBB, 30);
	AppendInt(xBB, 40);
	ZENITH_ASSERT_EQ(RunListRemoveAt(xBB, "bag", 1), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 3u);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 0), 10);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 1), 30, "ListRemoveAt reordered the list (swap-remove?)");
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 2), 40);
}

ZENITH_TEST(GraphNodeOps, ListRemoveAtBoundsGate)
{
	Zenith_GraphBlackboard xBB;
	AppendInt(xBB, 10);
	AppendInt(xBB, 20);

	// Out of range either way -> FAILURE, list untouched. Same gate as
	// GetListElement.
	ZENITH_ASSERT_EQ(RunListRemoveAt(xBB, "bag", 2), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(RunListRemoveAt(xBB, "bag", -1), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 2u);

	// An ABSENT list FAILS - and must NOT be created as a side effect of the
	// failed removal.
	ZENITH_ASSERT_EQ(RunListRemoveAt(xBB, "ghost", 0), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_NULL(xBB.TryGetList("ghost"), "a failed ListRemoveAt created the list it could not touch");

	// m_strIndexVar overrides m_iIndex when set (the const-or-var idiom).
	Zenith_PropertyValue xIdx; xIdx.SetInt32(0);
	xBB.SetValue("idx", xIdx);
	ZENITH_ASSERT_EQ(RunListRemoveAt(xBB, "bag", 99, "idx"), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 1u);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 0), 20);
}

ZENITH_TEST(GraphNodeOps, ListClearEmptiesAndNeverFails)
{
	Zenith_GraphBlackboard xBB;
	AppendInt(xBB, 10);
	AppendInt(xBB, 20);

	ZENITH_ASSERT_EQ(RunListClear(xBB, "bag"), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 0u);

	// Clearing an ABSENT list is SUCCESS, not FAILURE: "no elements" is exactly
	// what the caller asked for, and a teardown chain must not depend on
	// whether anything was ever added.
	ZENITH_ASSERT_EQ(RunListClear(xBB, "neverExisted"), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(RunListClear(xBB, "bag"), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// ...and the list is reusable afterwards.
	AppendInt(xBB, 99);
	ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 1u);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 0), 99);
}

//==============================================================================
// Routable FAILURE - the "On Failure" exec pin opt-ins of THIS TU.
//
// What each row proves, why the wired fail-probe is the positive control, and
// why the anchor/probes are engine nodes addressed by name all live ONCE, in
// the shared harness: Zenith_GraphNodeFailurePin.TestHarness.inl (included at
// the top of this file). Only the rows and the non-opted node are local.
//
// The runtime SEMANTICS of the pin (same chain key, suspending handlers,
// aborts, the cycle cap, refusal on flow/dynamic-pin/source types) are the
// FailurePin_* tests in Zenith_Scripting.Tests.inl - not duplicated here.
//==============================================================================

ZENITH_TEST(GraphNodeFailurePin, BlackboardMathOptIns)
{
	const Zenith_FailurePinCase axCases[] =
	{
		// GetListElement: FAILURE at Registration_Math.cpp:70 (list absent /
		// index out of range). "__nolist__" is never created by anything, so
		// TryGetList returns null and the one FAILURE branch is taken.
		{
			"GetListElement",
			[](Zenith_GraphBuilder& xBuilder, u_int uNode)
			{
				xBuilder.ParamString(uNode, "m_strListVar", "__nolist__");
			}
		},
	};

	// No scene: this row is pure blackboard, so self is deliberately invalid.
	Zenith_CheckFailurePinTable(axCases, static_cast<u_int>(sizeof(axCases) / sizeof(axCases[0])), Zenith_Entity());

	// GetListCount cannot fail at all, which is exactly why it is not an opt-in.
	Zenith_CheckNodeIsNotOptedIn("GetListCount");
}

//==============================================================================
// Pin-table coverage for this TU (the A-6 annotation sweep).
//
// What the totality walk proves, why the registry is SWAPPED to this TU's
// registrar rather than filtered by category, and why the restore is RAII all
// live ONCE, in the shared harness: Zenith_GraphPinTotality.TestHarness.inl.
// Only this TU's registrar, its exemption and its representative pins are here.
//==============================================================================

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, MathTotality)
{
	// ★ THE SWEEP'S ONE EXEMPTION, and it is passed HERE rather than baked into
	// the harness, so it can never quietly excuse a property in another TU.
	// LogicBlackboardBool.m_strVars matches the m_str*Var* matcher but holds a
	// COMMA-SEPARATED LIST of operand names (Zenith_GraphNode_ParseCommaList):
	// no descriptor can express N names, and one pointing at it would make the
	// validator check the literal "a,b" as a single variable name.
	const char* const aszExempt[] = { "m_strVars" };
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_Math, "_Math.cpp",
		aszExempt, static_cast<u_int>(sizeof(aszExempt) / sizeof(aszExempt[0])));
}

// One representative of each ROLE this TU declares. Roles are what the validator
// consumes (a WRITE role registers a writer that satisfies other readers; a READ
// role registers a read that must be satisfied), so a role typo is invisible to
// the totality walk - the pin would still count as "covered".
ZENITH_TEST(GraphPinTable, MathRoleSpotCheck)
{
	// READWRITE: read AND written under the SAME name in one Execute, with no
	// result-var alternative. Its own write never satisfies its own read.
	Zenith_CheckGraphPin("AddBlackboardInt", "Variable", GRAPH_PIN_ROLE_SELECTOR_READWRITE, PROPERTY_TYPE_INT32, "m_strVariable");
	Zenith_CheckGraphPin("ClampBlackboardFloat", "Value", GRAPH_PIN_ROLE_SELECTOR_READWRITE, PROPERTY_TYPE_FLOAT, "m_strVar");

	// ★ THE TRAP, and the reason the two are asserted side by side: the maths
	// nodes carry the same in-place SHAPE but m_strVar is SELECTOR_READ there.
	// Their write happens only when m_strResultVar is empty, and that write is
	// the Result pin's FALLBACK - a READWRITE would fabricate a writer in the
	// out-of-place case, where m_strVar is never written at all.
	Zenith_CheckGraphPin("MathBlackboardFloat", "Value", GRAPH_PIN_ROLE_SELECTOR_READ, PROPERTY_TYPE_FLOAT, "m_strVar");
	Zenith_CheckGraphPin("MathBlackboardFloat", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_FLOAT, "m_strResultVar");
	const Zenith_GraphPinDesc* pxMathResult = Zenith_FindGraphPin("MathBlackboardFloat", "Result");
	ZENITH_ASSERT_NOT_NULL(pxMathResult);
	if (pxMathResult != nullptr)
	{
		ZENITH_ASSERT_STREQ(pxMathResult->m_szFallbackVarNameProperty, "m_strVar",
			"the fallback is the ONLY thing expressing the in-place write back into m_strVar");
	}

	// INPUT_VAR_OR_CONST: the ternary shape, both halves declared. Losing the
	// constant half would make an unset min var look like an unsatisfied read.
	Zenith_CheckGraphPin("ClampBlackboardFloat", "Min", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_FLOAT, "m_strMinVar");
	const Zenith_GraphPinDesc* pxMin = Zenith_FindGraphPin("ClampBlackboardFloat", "Min");
	ZENITH_ASSERT_NOT_NULL(pxMin);
	if (pxMin != nullptr)
	{
		ZENITH_ASSERT_STREQ(pxMin->m_szConstProperty, "m_fMin",
			"the Min pin lost its inline-constant half");
	}

	// LIST: the blackboard's parallel list store, which holds no
	// Zenith_PropertyValue and is therefore never typed...
	Zenith_CheckGraphPin("ListAdd", "List", GRAPH_PIN_ROLE_LIST, eGRAPH_PIN_TYPE_ANY, "m_strListVar");
	// ...and the element appended to it is genuinely ANY, as is the element
	// GetListElement reads back out.
	Zenith_CheckGraphPin("ListAdd", "Value", GRAPH_PIN_ROLE_INPUT, eGRAPH_PIN_TYPE_ANY, "m_strValueVar");
	Zenith_CheckGraphPin("GetListElement", "Result", GRAPH_PIN_ROLE_OUTPUT, eGRAPH_PIN_TYPE_ANY, "m_strResultVar");

	// OUTPUT: a computed result, not a configured destination.
	Zenith_CheckGraphPin("GetListCount", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_INT32, "m_strResultVar");
}

// The one instance-resolved pin in this TU. MathBlackboardVector3's ops 4
// (length) and 5 (dot) collapse the vector to a FLOAT while every other op
// writes a VECTOR3, so the Result pin's type cannot be static - and a node that
// declined to answer would leave the validator treating every result as ANY.
ZENITH_TEST(GraphPinTable, MathVector3ResultTypeFollowsOp)
{
	// ★ THE INDEX COMES FROM THE TABLE, never from a literal: a pin inserted
	// above Result tomorrow must fail this test rather than silently move it on
	// to a different pin.
	const Zenith_GraphPinTable& xPins = Zenith_GraphNode_MathBlackboardVector3::GetPinTableStatic();
	u_int uResultIndex = xPins.GetPinCount();
	for (u_int u = 0; u < xPins.GetPinCount(); ++u)
	{
		if (xPins.GetPinAt(u).m_szName != nullptr && std::strcmp(xPins.GetPinAt(u).m_szName, "Result") == 0)
		{
			uResultIndex = u;
		}
	}
	ZENITH_ASSERT_LT(uResultIndex, xPins.GetPinCount(), "MathBlackboardVector3 declares no pin named 'Result'");
	if (uResultIndex >= xPins.GetPinCount())
	{
		return;
	}
	ZENITH_ASSERT_TRUE(xPins.GetPinAt(uResultIndex).m_bInstanceResolved,
		"the Result pin stopped being instance-resolved, so GetPinType is never consulted");

	Zenith_GraphNode_MathBlackboardVector3 xNode;
	// Past the last real op (6) as well: an out-of-range op FAILS at execute
	// time and must still not answer FLOAT.
	for (int32_t iOp = 0; iOp <= 8; ++iOp)
	{
		xNode.m_iOp = iOp;
		Zenith_PropertyType eType = eGRAPH_PIN_TYPE_ANY;
		ZENITH_ASSERT_TRUE(xNode.GetPinType(uResultIndex, eType), "op %d: the Result pin must answer", iOp);
		const Zenith_PropertyType eExpected = (iOp == 4 || iOp == 5) ? PROPERTY_TYPE_FLOAT : PROPERTY_TYPE_VECTOR3;
		ZENITH_ASSERT_EQ(static_cast<int>(eType), static_cast<int>(eExpected),
			"op %d resolved the Result pin to the wrong type", iOp);
	}

	// Every OTHER pin DECLINES. Answering for a statically typed pin would let a
	// per-instance guess override the declared type.
	xNode.m_iOp = 4;
	for (u_int u = 0; u < xPins.GetPinCount(); ++u)
	{
		if (u == uResultIndex)
		{
			continue;
		}
		Zenith_PropertyType eType = eGRAPH_PIN_TYPE_ANY;
		ZENITH_ASSERT_FALSE(xNode.GetPinType(u, eType), "pin '%s' must decline to answer GetPinType",
			xPins.GetPinAt(u).m_szName != nullptr ? xPins.GetPinAt(u).m_szName : "(null)");
	}
}

#endif // ZENITH_TESTING
