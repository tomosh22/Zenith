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

static float SlotFloat(const Zenith_PropertyValue* pxSlot, const char* szWhat);
static void SeedFloat(Zenith_GraphBlackboard& xBB, const char* szName, float fValue);

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
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	const Zenith_PropertyValue* pxResult = xNode.GetOutputForTest(Zenith_GraphNode_MathBlackboardFloat::uPIN_Result);
	ZENITH_ASSERT_NOT_NULL(pxResult);
	ZENITH_ASSERT_TRUE(pxResult != nullptr && pxResult->GetType() == PROPERTY_TYPE_FLOAT);
	return pxResult != nullptr && pxResult->GetType() == PROPERTY_TYPE_FLOAT ? pxResult->GetFloat() : 0.0f;
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

	// Failure retains a prior output slot and leaves unrelated blackboard data
	// untouched. This is the direct-construction Shape-B failure contract.
	Zenith_GraphBlackboard xBB;
	SeedFloat(xBB, "v", 6.0f);
	SeedFloat(xBB, "sentinel", 11.0f);
	Zenith_GraphNode_MathBlackboardFloat xNode;
	xNode.m_strVar = "v";
	xNode.m_iOp = GRAPH_MATH_FLOAT_OP_MULTIPLY;
	xNode.m_fOperand = 2.0f;
	Zenith_GraphContext xCtx; xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_MathBlackboardFloat::uPIN_Result), "prior result"), 12.0f, 0.0001f);
	xNode.m_iOp = GRAPH_MATH_FLOAT_OP_DIVIDE;
	xNode.m_fOperand = 0.0f;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_MathBlackboardFloat::uPIN_Result), "retained result"), 12.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBB.GetFloat("sentinel"), 11.0f, 0.0001f);
}

// Runs CompareBlackboardEntity over two pre-seeded packed IDs; asserts SUCCESS
// (the node is a ternary that never fails) and returns its bool result.
static bool RunCompareEntity(u_int64 ulA, u_int64 ulB, int32_t iOp)
{
	Zenith_GraphBlackboard xBB;
	Zenith_PropertyValue xA; xA.SetPackedEntityID(ulA); xBB.SetValue("a", xA);
	Zenith_PropertyValue xB; xB.SetPackedEntityID(ulB); xBB.SetValue("b", xB);
	Zenith_GraphNode_CompareBlackboardEntity xNode;
	xNode.m_iOp = iOp;
	xNode.SetInputForTest(Zenith_GraphNode_CompareBlackboardEntity::uPIN_A, xA);
	xNode.SetInputForTest(Zenith_GraphNode_CompareBlackboardEntity::uPIN_B, xB);
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	const Zenith_PropertyValue* pxResult = xNode.GetOutputForTest(Zenith_GraphNode_CompareBlackboardEntity::uPIN_Result);
	ZENITH_ASSERT_NOT_NULL(pxResult);
	ZENITH_ASSERT_TRUE(pxResult != nullptr && pxResult->GetType() == PROPERTY_TYPE_BOOL);
	return pxResult != nullptr && pxResult->GetType() == PROPERTY_TYPE_BOOL ? pxResult->GetBool() : false;
}

ZENITH_TEST(GraphNodeOps, CompareBlackboardEntityEqualityAndDefault)
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
	// defaulting failure) - this is the deliberately-preserved typed default.
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
	bool bMissingIsTrue = false,
	bool* pbResult = nullptr)
{
	Zenith_GraphNode_LogicBlackboardBool xNode;
	xNode.m_strVars = szVars;
	xNode.m_iOp = iOp;
	xNode.m_bInvert = bInvert;
	xNode.m_bMissingIsTrue = bMissingIsTrue;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	const int iStatus = static_cast<int>(xNode.Execute(xCtx));
	if (pbResult != nullptr && iStatus == static_cast<int>(GRAPH_NODE_STATUS_SUCCESS))
	{
		const Zenith_PropertyValue* pxResult = xNode.GetOutputForTest(Zenith_GraphNode_LogicBlackboardBool::uPIN_Result);
		ZENITH_ASSERT_NOT_NULL(pxResult);
		ZENITH_ASSERT_TRUE(pxResult != nullptr && pxResult->GetType() == PROPERTY_TYPE_BOOL);
		*pbResult = pxResult != nullptr && pxResult->GetType() == PROPERTY_TYPE_BOOL ? pxResult->GetBool() : false;
	}
	return iStatus;
}

// The SUCCESS-path twin: asserts the node reported SUCCESS and returns "r".
static bool RunLogicBool(
	Zenith_GraphBlackboard& xBB,
	const char* szVars,
	int32_t iOp,
	bool bInvert = false,
	bool bMissingIsTrue = false)
{
	bool bResult = false;
	ZENITH_ASSERT_EQ(
		RunLogicBoolStatus(xBB, szVars, iOp, bInvert, bMissingIsTrue, &bResult),
		static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	return bResult;
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
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	const Zenith_PropertyValue* pxFirst = xNode.GetOutputForTest(Zenith_GraphNode_LogicBlackboardBool::uPIN_Result);
	ZENITH_ASSERT_NOT_NULL(pxFirst);
	ZENITH_ASSERT_TRUE(pxFirst != nullptr && pxFirst->GetType() == PROPERTY_TYPE_BOOL && pxFirst->GetBool());

	xNode.m_strVars = "b";	// the edit
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	const Zenith_PropertyValue* pxSecond = xNode.GetOutputForTest(Zenith_GraphNode_LogicBlackboardBool::uPIN_Result);
	ZENITH_ASSERT_NOT_NULL(pxSecond);
	ZENITH_ASSERT_FALSE(pxSecond != nullptr && pxSecond->GetType() == PROPERTY_TYPE_BOOL && pxSecond->GetBool(),
		"the node kept its stale operand list after m_strVars changed");
}

//==============================================================================
// ListAdd / ListRemoveAt / ListClear
//==============================================================================

// Appends an int32 through the real node and returns its status.
static int RunListAddInt(Zenith_GraphBlackboard& xBB, const char* szListVar)
{
	Zenith_GraphNode_ListAdd xNode;
	xNode.m_strListVar = szListVar;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	return static_cast<int>(xNode.Execute(xCtx));
}

static int RunListRemoveAt(Zenith_GraphBlackboard& xBB, const char* szListVar, int32_t iIndex)
{
	Zenith_GraphNode_ListRemoveAt xNode;
	xNode.m_strListVar = szListVar;
	xNode.m_iIndex = iIndex;
	Zenith_PropertyValue xIndex;
	xIndex.SetInt32(iIndex);
	xNode.SetInputForTest(Zenith_GraphNode_ListRemoveAt::uPIN_Index, xIndex);
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

// Appends through a connected input; the absence cases below deliberately leave
// that ANY input UNSET, so a failure cannot create or alter the list.
static void AppendInt(Zenith_GraphBlackboard& xBB, int32_t iValue)
{
	Zenith_GraphNode_ListAdd xNode;
	xNode.m_strListVar = "bag";
	Zenith_PropertyValue xValue; xValue.SetInt32(iValue);
	xNode.SetInputForTest(Zenith_GraphNode_ListAdd::uPIN_Value, xValue);
	Zenith_GraphContext xCtx; xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
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
	ZENITH_ASSERT_EQ(RunListAddInt(xBB, "bag"), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
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

	// A wire selects index 0 even though the node's const is irrelevant.
	ZENITH_ASSERT_EQ(RunListRemoveAt(xBB, "bag", 0), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
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
	// Result is a computed output, so declaring Value READWRITE would fabricate
	// a writer even though this selector is never written.
	Zenith_CheckGraphPin("MathBlackboardFloat", "Value", GRAPH_PIN_ROLE_SELECTOR_READ, PROPERTY_TYPE_FLOAT, "m_strVar");
	Zenith_CheckGraphPin("MathBlackboardFloat", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_FLOAT, "");
	const Zenith_GraphPinDesc* pxMathResult = Zenith_FindGraphPin("MathBlackboardFloat", "Result");
	ZENITH_ASSERT_NOT_NULL(pxMathResult);
	if (pxMathResult != nullptr)
	{
	}

	// INPUT_CONST: the current constant supplies an unconnected input. Losing the
	// constant half would make an unset min var look like an unsatisfied read.
	Zenith_CheckGraphPin("ClampBlackboardFloat", "Min", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_FLOAT, "");
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
	Zenith_CheckGraphPin("ListAdd", "Value", GRAPH_PIN_ROLE_INPUT, eGRAPH_PIN_TYPE_ANY, "");
	Zenith_CheckGraphPin("GetListElement", "Result", GRAPH_PIN_ROLE_OUTPUT, eGRAPH_PIN_TYPE_ANY, "");

	// OUTPUT: a computed result, not a configured destination.
	Zenith_CheckGraphPin("GetListCount", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_INT32, "");
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

//==============================================================================
// PIN RUNTIME for this TU (B-6.1) - the pins are LIVE.
//
// Every node above now reads its INPUT descriptors through
// Zenith_GraphNode::GetInput / GetInputPackedEntityID / TryGetInput and writes its
// OUTPUT descriptors through SetOutput. The unconnected tests construct nodes
// directly, set permanent selectors and constants, and read their output slots.
// The tests below prove that a resolved wire carries the input value.
//
// ★ EVERY Wired_* ROW sets a const (where the pin supports one) and an explicit
// wire. Only the live wire can produce the wired value.
//
// ★ ORDERING RULE: pin state is built ONCE, on the first accessor call, from the
// properties as they read THEN. Assign every property before the first Execute,
// and use a FRESH node when a test changes one that the binding depends on (an op
// code that decides an instance-resolved slot TYPE).
//
// ★ These fixtures never reach a counted census log: the per-game census parses
// `zenith test <G> --headless` runs, which pass --skip-unit-tests.
//==============================================================================

// One row of the index contract: the constant addresses the pin it is named for,
// with the role the migration assumed.
static void CheckMathPin(const Zenith_GraphPinTable& xPins, u_int uIndex, const char* szName,
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

static void SeedFloat(Zenith_GraphBlackboard& xBB, const char* szName, float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	xBB.SetValue(szName, xValue);
}

static void SeedInt(Zenith_GraphBlackboard& xBB, const char* szName, int32_t iValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(iValue);
	xBB.SetValue(szName, xValue);
}

static void SeedVec3(Zenith_GraphBlackboard& xBB, const char* szName, const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	xBB.SetValue(szName, xValue);
}

static Zenith_PropertyValue WireFloat(float fValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetFloat(fValue);
	return xValue;
}

static Zenith_PropertyValue WireInt(int32_t iValue)
{
	Zenith_PropertyValue xValue;
	xValue.SetInt32(iValue);
	return xValue;
}

static Zenith_PropertyValue WireVec3(const Zenith_Maths::Vector3& xVec)
{
	Zenith_PropertyValue xValue;
	xValue.SetVector3(xVec);
	return xValue;
}

static Zenith_PropertyValue WireEntity(u_int64 ulPacked)
{
	Zenith_PropertyValue xValue;
	xValue.SetPackedEntityID(ulPacked);
	return xValue;
}

// Reads a FLOAT output slot, tag-checked (the tagged getters ASSERT, and an
// assertion failure must read as a test failure rather than a DebugBreak).
static float SlotFloat(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0.0f;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_FLOAT),
		"%s: the slot holds type %u, not FLOAT", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_FLOAT ? pxSlot->GetFloat() : 0.0f;
}

static int32_t SlotInt(const Zenith_PropertyValue* pxSlot, const char* szWhat)
{
	ZENITH_ASSERT_NOT_NULL(pxSlot, "%s: the output slot is UNSET", szWhat);
	if (pxSlot == nullptr)
	{
		return 0;
	}
	ZENITH_ASSERT_EQ(static_cast<int>(pxSlot->GetType()), static_cast<int>(PROPERTY_TYPE_INT32),
		"%s: the slot holds type %u, not INT32", szWhat, static_cast<u_int>(pxSlot->GetType()));
	return pxSlot->GetType() == PROPERTY_TYPE_INT32 ? pxSlot->GetInt32() : 0;
}

// A real pure producer is required to prove a conditional pull. A resolved slot
// counter is intentionally insufficient because it latches after its first use.
class Zenith_GraphNode_MathTestCountingFloatProducer : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_MathTestCountingFloatProducer)
public:
	static constexpr u_int uPIN_Value = 0u;
	ZENITH_GRAPH_PINS_BEGIN(Zenith_GraphNode_MathTestCountingFloatProducer)
	ZENITH_GRAPH_PIN_OUTPUT(Value, PROPERTY_TYPE_FLOAT)
	ZENITH_GRAPH_PINS_END

public:

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		++s_uPullCount;
		SetOutput<float>(xContext, uPIN_Value, 0.25f);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "Test_MathCountingFloatProducer"; }

	inline static u_int s_uPullCount = 0u;
};

static void EnsureMathCountingProducerRegistered()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.EnsureInitialized();
	if (xRegistry.Find("Test_MathCountingFloatProducer") == nullptr)
	{
		xRegistry.RegisterNodeType<Zenith_GraphNode_MathTestCountingFloatProducer>(
			"Test_MathCountingFloatProducer", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
	}
}

// ★ TABLE ORDER IS THE CONTRACT. A pin INDEX is what every accessor addresses, so
// a reorder - or an inserted pin - silently re-points every uPIN_ constant in this
// TU at a different descriptor. A static_assert is impossible: the tables are
// filled at static init, so this is the earliest a check can run.
ZENITH_TEST(GraphPinTable, MathPinIndicesMatchTables)
{
	// --- the list family ----------------------------------------------------
	const Zenith_GraphPinTable& xCount = Zenith_GraphNode_GetListCount::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xCount.GetPinCount(), 2u, "GetListCount gained or lost a pin");
	CheckMathPin(xCount, Zenith_GraphNode_GetListCount::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT, "GetListCount");

	const Zenith_GraphPinTable& xElement = Zenith_GraphNode_GetListElement::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xElement.GetPinCount(), 3u, "GetListElement gained or lost a pin");
	CheckMathPin(xElement, Zenith_GraphNode_GetListElement::uPIN_Index, "Index", GRAPH_PIN_ROLE_INPUT, "GetListElement");
	CheckMathPin(xElement, Zenith_GraphNode_GetListElement::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT, "GetListElement");

	const Zenith_GraphPinTable& xAdd = Zenith_GraphNode_ListAdd::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xAdd.GetPinCount(), 2u, "ListAdd gained or lost a pin");
	CheckMathPin(xAdd, Zenith_GraphNode_ListAdd::uPIN_Value, "Value", GRAPH_PIN_ROLE_INPUT, "ListAdd");

	const Zenith_GraphPinTable& xRemove = Zenith_GraphNode_ListRemoveAt::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xRemove.GetPinCount(), 2u, "ListRemoveAt gained or lost a pin");
	CheckMathPin(xRemove, Zenith_GraphNode_ListRemoveAt::uPIN_Index, "Index", GRAPH_PIN_ROLE_INPUT, "ListRemoveAt");

	// ListClear declares its LIST name and nothing else - no pin of its own is
	// live, which is exactly what its count asserts.
	ZENITH_ASSERT_EQ(Zenith_GraphNode_ListClear::GetPinTableStatic().GetPinCount(), 1u,
		"ListClear gained a pin; nothing in its Execute addresses one");

	// --- logic --------------------------------------------------------------
	const Zenith_GraphPinTable& xLogic = Zenith_GraphNode_LogicBlackboardBool::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xLogic.GetPinCount(), 1u, "LogicBlackboardBool gained or lost a pin");
	CheckMathPin(xLogic, Zenith_GraphNode_LogicBlackboardBool::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"LogicBlackboardBool");

	// --- arithmetic ---------------------------------------------------------
	const Zenith_GraphPinTable& xAddI = Zenith_GraphNode_AddBlackboardInt::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xAddI.GetPinCount(), 2u, "AddBlackboardInt gained or lost a pin");
	CheckMathPin(xAddI, Zenith_GraphNode_AddBlackboardInt::uPIN_Delta, "Delta", GRAPH_PIN_ROLE_INPUT, "AddBlackboardInt");

	const Zenith_GraphPinTable& xAddV = Zenith_GraphNode_AddBlackboardVector3::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xAddV.GetPinCount(), 2u, "AddBlackboardVector3 gained or lost a pin");
	CheckMathPin(xAddV, Zenith_GraphNode_AddBlackboardVector3::uPIN_Delta, "Delta", GRAPH_PIN_ROLE_INPUT,
		"AddBlackboardVector3");

	const Zenith_GraphPinTable& xCmpE = Zenith_GraphNode_CompareBlackboardEntity::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xCmpE.GetPinCount(), 3u, "CompareBlackboardEntity gained or lost a pin");
	CheckMathPin(xCmpE, Zenith_GraphNode_CompareBlackboardEntity::uPIN_A, "A", GRAPH_PIN_ROLE_INPUT,
		"CompareBlackboardEntity");
	CheckMathPin(xCmpE, Zenith_GraphNode_CompareBlackboardEntity::uPIN_B, "B", GRAPH_PIN_ROLE_INPUT,
		"CompareBlackboardEntity");
	CheckMathPin(xCmpE, Zenith_GraphNode_CompareBlackboardEntity::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"CompareBlackboardEntity");

	const Zenith_GraphPinTable& xMathF = Zenith_GraphNode_MathBlackboardFloat::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xMathF.GetPinCount(), 3u, "MathBlackboardFloat gained or lost a pin");
	CheckMathPin(xMathF, Zenith_GraphNode_MathBlackboardFloat::uPIN_Operand, "Operand", GRAPH_PIN_ROLE_INPUT,
		"MathBlackboardFloat");
	CheckMathPin(xMathF, Zenith_GraphNode_MathBlackboardFloat::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"MathBlackboardFloat");

	const Zenith_GraphPinTable& xMathV = Zenith_GraphNode_MathBlackboardVector3::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xMathV.GetPinCount(), 4u, "MathBlackboardVector3 gained or lost a pin");
	CheckMathPin(xMathV, Zenith_GraphNode_MathBlackboardVector3::uPIN_Operand, "Operand", GRAPH_PIN_ROLE_INPUT,
		"MathBlackboardVector3");
	CheckMathPin(xMathV, Zenith_GraphNode_MathBlackboardVector3::uPIN_Scalar, "Scalar", GRAPH_PIN_ROLE_INPUT,
		"MathBlackboardVector3");
	CheckMathPin(xMathV, Zenith_GraphNode_MathBlackboardVector3::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT,
		"MathBlackboardVector3");

	const Zenith_GraphPinTable& xLerpF = Zenith_GraphNode_LerpBlackboardFloat::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xLerpF.GetPinCount(), 3u, "LerpBlackboardFloat gained or lost a pin");
	CheckMathPin(xLerpF, Zenith_GraphNode_LerpBlackboardFloat::uPIN_Target, "Target", GRAPH_PIN_ROLE_INPUT,
		"LerpBlackboardFloat");
	CheckMathPin(xLerpF, Zenith_GraphNode_LerpBlackboardFloat::uPIN_T, "T", GRAPH_PIN_ROLE_INPUT,
		"LerpBlackboardFloat");

	const Zenith_GraphPinTable& xLerpV = Zenith_GraphNode_LerpBlackboardVector3::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xLerpV.GetPinCount(), 3u, "LerpBlackboardVector3 gained or lost a pin");
	CheckMathPin(xLerpV, Zenith_GraphNode_LerpBlackboardVector3::uPIN_Target, "Target", GRAPH_PIN_ROLE_INPUT,
		"LerpBlackboardVector3");
	CheckMathPin(xLerpV, Zenith_GraphNode_LerpBlackboardVector3::uPIN_T, "T", GRAPH_PIN_ROLE_INPUT,
		"LerpBlackboardVector3");

	const Zenith_GraphPinTable& xClamp = Zenith_GraphNode_ClampBlackboardFloat::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xClamp.GetPinCount(), 3u, "ClampBlackboardFloat gained or lost a pin");
	CheckMathPin(xClamp, Zenith_GraphNode_ClampBlackboardFloat::uPIN_Min, "Min", GRAPH_PIN_ROLE_INPUT,
		"ClampBlackboardFloat");
	CheckMathPin(xClamp, Zenith_GraphNode_ClampBlackboardFloat::uPIN_Max, "Max", GRAPH_PIN_ROLE_INPUT,
		"ClampBlackboardFloat");

	// --- random -------------------------------------------------------------
	const Zenith_GraphPinTable& xRandF = Zenith_GraphNode_RandomFloat::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xRandF.GetPinCount(), 1u, "RandomFloat gained or lost a pin");
	CheckMathPin(xRandF, Zenith_GraphNode_RandomFloat::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT, "RandomFloat");

	const Zenith_GraphPinTable& xRandI = Zenith_GraphNode_RandomInt::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xRandI.GetPinCount(), 1u, "RandomInt gained or lost a pin");
	CheckMathPin(xRandI, Zenith_GraphNode_RandomInt::uPIN_Result, "Result", GRAPH_PIN_ROLE_OUTPUT, "RandomInt");
}

ZENITH_TEST(MathPinRuntime, Wired_MathFloatOperandAndResult)
{
	Zenith_GraphBlackboard xBB;
	SeedFloat(xBB, "v", 3.0f);

	Zenith_GraphNode_MathBlackboardFloat xNode;
	xNode.m_strVar = "v";
	xNode.m_iOp = GRAPH_MATH_FLOAT_OP_MULTIPLY;
	xNode.m_fOperand = 9.0f;			// leg 1: the const
	xNode.SetInputForTest(Zenith_GraphNode_MathBlackboardFloat::uPIN_Operand, WireFloat(5.0f));	// leg 3: the wire

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// 3 x 5: the WIRE, not the const 9.
	ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_MathBlackboardFloat::uPIN_Result),
		"MathBlackboardFloat.Result"), 15.0f, 0.0001f);
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

ZENITH_TEST(MathPinRuntime, Wired_MathVector3ResultTypeFollowsOp)
{
	// ★ A FRESH NODE PER OP. The Result slot's type is stamped from GetPinType at
	// BIND time - once - so an op code assigned after the first accessor call does
	// not re-type the slot. The third row below pins exactly that.
	const u_int uResult = Zenith_GraphNode_MathBlackboardVector3::uPIN_Result;
	const u_int uScalar = Zenith_GraphNode_MathBlackboardVector3::uPIN_Scalar;

	// --- op 4 (length): a FLOAT slot ---------------------------------------
	{
		Zenith_GraphBlackboard xBB;
		SeedVec3(xBB, "v", Zenith_Maths::Vector3(3.0f, 0.0f, 4.0f));

		Zenith_GraphNode_MathBlackboardVector3 xNode;
		xNode.m_strVar = "v";
		xNode.m_iOp = 4;

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.GetOutputPinType(uResult)), static_cast<int>(PROPERTY_TYPE_FLOAT),
			"op 4 collapses the vector to a scalar, so the slot must be stamped FLOAT");
		ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(uResult), "MathBlackboardVector3.Result op4"),
			5.0f, 0.0001f);
		ZENITH_ASSERT_EQ(xNode.GetOutputMismatchWarningCountForTest(uResult), 0u);
	}

	// --- op 2 (scale): a VECTOR3 slot, and the Scalar pin comes off the wire --
	{
		Zenith_GraphBlackboard xBB;
		SeedVec3(xBB, "v", Zenith_Maths::Vector3(3.0f, 0.0f, 4.0f));

		Zenith_GraphNode_MathBlackboardVector3 xNode;
		xNode.m_strVar = "v";
		xNode.m_iOp = 2;
		xNode.m_fScalar = 9.0f;			// leg 1
		xNode.SetInputForTest(uScalar, WireFloat(5.0f));	// leg 3

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.GetOutputPinType(uResult)), static_cast<int>(PROPERTY_TYPE_VECTOR3));
		const Zenith_PropertyValue* pxSlot = xNode.GetOutputForTest(uResult);
		ZENITH_ASSERT_NOT_NULL(pxSlot);
		ZENITH_ASSERT_NOT_NULL(pxSlot, "the slot must be latched");
		ZENITH_ASSERT_TRUE(pxSlot != nullptr && pxSlot->GetType() == PROPERTY_TYPE_VECTOR3, "the slot must carry the declared tag - a wrong tag must FAIL, not skip");
		if (pxSlot != nullptr && pxSlot->GetType() == PROPERTY_TYPE_VECTOR3)
		{
			ZENITH_ASSERT_NEAR_VEC3(pxSlot->GetVector3(), Zenith_Maths::Vector3(15.0f, 0.0f, 20.0f), 0.0001f);
		}
	}

	// --- the CONSTRAINT, stated out loud: a REUSED instance keeps its stamp ---
	{
		Zenith_GraphBlackboard xBB;
		SeedVec3(xBB, "v", Zenith_Maths::Vector3(3.0f, 0.0f, 4.0f));

		Zenith_GraphNode_MathBlackboardVector3 xNode;
		xNode.m_strVar = "v";
		xNode.m_iOp = 4;				// binds a FLOAT slot

		Zenith_GraphContext xCtx;
		xCtx.m_pxBlackboard = &xBB;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(uResult), "first write"), 5.0f, 0.0001f);

		// The op changes on a LIVE instance: the slot is NOT re-stamped, so the
		// VECTOR3 write is REFUSED with one line and the slot keeps its FLOAT. That
		// is the ordering rule made observable - not a defect to work around.
		xNode.m_iOp = 0;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(xNode.GetOutputMismatchWarningCountForTest(uResult), 1u,
			"a VECTOR3 write into a FLOAT-stamped slot must be refused, once, with a line");
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.GetOutputPinType(uResult)), static_cast<int>(PROPERTY_TYPE_FLOAT));
		ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(uResult), "reused instance"), 5.0f, 0.0001f);
	}
}

ZENITH_TEST(MathPinRuntime, Wired_AddIntDelta)
{
	Zenith_GraphBlackboard xBB;
	SeedInt(xBB, "i", 10);

	Zenith_GraphNode_AddBlackboardInt xNode;
	xNode.m_strVariable = "i";
	xNode.m_iDelta = 9;
	xNode.SetInputForTest(Zenith_GraphNode_AddBlackboardInt::uPIN_Delta, WireInt(5));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// The READWRITE variable is still read AND written DIRECTLY (a selector is
	// never a wire); only the Delta came off the pin.
	ZENITH_ASSERT_EQ(xBB.GetInt32("i"), 15);
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);
}

ZENITH_TEST(MathPinRuntime, Wired_AddVector3Delta)
{
	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "av", Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f));

	Zenith_GraphNode_AddBlackboardVector3 xNode;
	xNode.m_strVariable = "av";
	xNode.m_xDelta = Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f);
	xNode.m_bScaleByDt = false;
	xNode.SetInputForTest(Zenith_GraphNode_AddBlackboardVector3::uPIN_Delta,
		WireVec3(Zenith_Maths::Vector3(5.0f, 5.0f, 5.0f)));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("av"), Zenith_Maths::Vector3(6.0f, 6.0f, 6.0f), 0.0001f);

	// The dt scale still applies to whatever the pin produced.
	Zenith_GraphNode_AddBlackboardVector3 xScaled;
	xScaled.m_strVariable = "av2";
	xScaled.m_bScaleByDt = true;
	xScaled.SetInputForTest(Zenith_GraphNode_AddBlackboardVector3::uPIN_Delta,
		WireVec3(Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f)));
	SeedVec3(xBB, "av2", Zenith_Maths::Vector3(0.0f));
	xCtx.m_fDt = 0.5f;
	ZENITH_ASSERT_EQ(static_cast<int>(xScaled.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("av2"), Zenith_Maths::Vector3(0.0f, 5.0f, 0.0f), 0.0001f);
}

ZENITH_TEST(MathPinRuntime, Wired_CompareEntityAB)
{
	// ★ NO CONST LEG EXISTS on these two pins (an EntityID has no inline-constant
	// twin), so both operands are explicit wires.
	const u_int uPinA = Zenith_GraphNode_CompareBlackboardEntity::uPIN_A;
	const u_int uPinB = Zenith_GraphNode_CompareBlackboardEntity::uPIN_B;

	Zenith_GraphBlackboard xBB;

	Zenith_GraphNode_CompareBlackboardEntity xNode;
	xNode.m_iOp = GRAPH_ENTITY_COMPARE_OP_EQUAL;
	// The wires agree.
	xNode.SetInputForTest(uPinA, WireEntity(0x00000000000000BBull));
	xNode.SetInputForTest(uPinB, WireEntity(0x00000000000000BBull));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const Zenith_PropertyValue* pxSlot = xNode.GetOutputForTest(
		Zenith_GraphNode_CompareBlackboardEntity::uPIN_Result);
	ZENITH_ASSERT_NOT_NULL(pxSlot);
	ZENITH_ASSERT_NOT_NULL(pxSlot, "the slot must be latched");
	ZENITH_ASSERT_TRUE(pxSlot != nullptr && pxSlot->GetType() == PROPERTY_TYPE_BOOL, "the slot must carry the declared tag - a wrong tag must FAIL, not skip");
	if (pxSlot != nullptr && pxSlot->GetType() == PROPERTY_TYPE_BOOL)
	{
		ZENITH_ASSERT_TRUE(pxSlot->GetBool());
	}
}

ZENITH_TEST(MathPinRuntime, Wired_LerpFloatTargetT)
{
	Zenith_GraphBlackboard xBB;
	SeedFloat(xBB, "l", 0.0f);

	Zenith_GraphNode_LerpBlackboardFloat xNode;
	xNode.m_strVar = "l";
	xNode.m_fTarget = 9.0f;
	xNode.m_iMode = 0;
	xNode.m_fT = 0.9f;
	xNode.m_iEasing = 0;
	xNode.SetInputForTest(Zenith_GraphNode_LerpBlackboardFloat::uPIN_Target, WireFloat(10.0f));
	xNode.SetInputForTest(Zenith_GraphNode_LerpBlackboardFloat::uPIN_T, WireFloat(0.5f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// 0 + (10 - 0) * 0.5, both operands off the wire; the write-back stays direct.
	ZENITH_ASSERT_EQ_FLOAT(xBB.GetFloat("l", -1.0f), 5.0f, 0.0001f);
}

ZENITH_TEST(MathPinRuntime, Wired_LerpVector3)
{
	Zenith_GraphBlackboard xBB;
	SeedVec3(xBB, "lv", Zenith_Maths::Vector3(0.0f));

	Zenith_GraphNode_LerpBlackboardVector3 xNode;
	xNode.m_strVar = "lv";
	xNode.m_xTarget = Zenith_Maths::Vector3(9.0f, 9.0f, 9.0f);
	xNode.m_iMode = 0;
	xNode.m_fT = 0.9f;
	xNode.m_iEasing = 0;
	xNode.SetInputForTest(Zenith_GraphNode_LerpBlackboardVector3::uPIN_Target,
		WireVec3(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f)));
	xNode.SetInputForTest(Zenith_GraphNode_LerpBlackboardVector3::uPIN_T, WireFloat(0.5f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	ZENITH_ASSERT_NEAR_VEC3(xBB.GetVector3("lv"), Zenith_Maths::Vector3(5.0f, 0.0f, 0.0f), 0.0001f);
}

ZENITH_TEST(MathPinRuntime, Wired_ClampMinMax)
{
	Zenith_GraphBlackboard xBB;
	SeedFloat(xBB, "c", 20.0f);

	Zenith_GraphNode_ClampBlackboardFloat xNode;
	xNode.m_strVar = "c";
	xNode.m_fMin = 9.0f;
	xNode.m_fMax = 9.0f;
	xNode.SetInputForTest(Zenith_GraphNode_ClampBlackboardFloat::uPIN_Min, WireFloat(0.0f));
	xNode.SetInputForTest(Zenith_GraphNode_ClampBlackboardFloat::uPIN_Max, WireFloat(5.0f));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// Clamped to the WIRED max, not to 7 and not to 9.
	ZENITH_ASSERT_EQ_FLOAT(xBB.GetFloat("c", -1.0f), 5.0f, 0.0001f);
}

ZENITH_TEST(MathPinRuntime, Wired_GetListElementIndexAndAnyResult)
{
	const u_int uIndex = Zenith_GraphNode_GetListElement::uPIN_Index;
	const u_int uResult = Zenith_GraphNode_GetListElement::uPIN_Result;

	Zenith_GraphBlackboard xBB;
	AppendInt(xBB, 10);
	AppendInt(xBB, 20);
	AppendInt(xBB, 30);

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// (a) an ANY OUTPUT slot starts UNSET, and a FAILED execute leaves it UNSET -
	//     which is what makes "the producer has not written yet" observable.
	{
		Zenith_GraphNode_GetListElement xNode;
		xNode.m_strListVar = "bag";
		xNode.m_iIndex = 9;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xNode.GetOutputForTest(uResult), "an ANY slot must start UNSET");
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.GetOutputPinType(uResult)), static_cast<int>(eGRAPH_PIN_TYPE_ANY));
	}

	// (b) the wired index wins over both the var (7) and the const (9).
	{
		Zenith_GraphNode_GetListElement xNode;
		xNode.m_strListVar = "bag";
		xNode.m_iIndex = 9;
		xNode.SetInputForTest(uIndex, WireInt(1));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(SlotInt(xNode.GetOutputForTest(uResult), "GetListElement.Result"), 20);
	}
}

ZENITH_TEST(MathPinRuntime, Wired_ListAddFromWireAndUnsetIsFailure)
{
	const u_int uValue = Zenith_GraphNode_ListAdd::uPIN_Value;

	Zenith_GraphBlackboard xBB;
	AppendInt(xBB, 10);		// "bag" = [10]
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// (a) NOTHING there - no wire, no variable - is FAILURE with the list
	//     UNTOUCHED. That is ListAdd's exact contract, now expressed through the
	//     PRESENCE-aware accessor.
	{
		Zenith_GraphNode_ListAdd xNode;
		xNode.m_strListVar = "bag";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 1u, "a failed ListAdd grew the list");
	}

	// (b) a wired value appends, and WINS over the blackboard variable.
	{
		Zenith_GraphNode_ListAdd xNode;
		xNode.m_strListVar = "bag";
		xNode.SetInputForTest(uValue, WireInt(5));
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 2u);
		ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 1), 5, "expected the WIRED 5; a 7 means the variable won");
	}

	// (c) a node with NO var name and no wire at all: still FAILURE, still no list
	//     created (the ANY pin has no const property, so there is nothing to
	//     default to - which is the whole reason the contract is FAILURE).
	{
		Zenith_GraphNode_ListAdd xNode;
		xNode.m_strListVar = "ghost";
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(xBB.TryGetList("ghost"), "a failed ListAdd created the list it could not touch");
	}
}

ZENITH_TEST(MathPinRuntime, Wired_ListRemoveAtIndex)
{
	Zenith_GraphBlackboard xBB;
	AppendInt(xBB, 10);
	AppendInt(xBB, 20);
	AppendInt(xBB, 30);
	AppendInt(xBB, 40);

	Zenith_GraphNode_ListRemoveAt xNode;
	xNode.m_strListVar = "bag";
	xNode.m_iIndex = 9;
	xNode.SetInputForTest(Zenith_GraphNode_ListRemoveAt::uPIN_Index, WireInt(1));

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	// Order-preserving, at the WIRED index.
	ZENITH_ASSERT_EQ(xBB.TryGetList("bag")->GetSize(), 3u);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 0), 10);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 1), 30);
	ZENITH_ASSERT_EQ(ListElement(xBB, "bag", 2), 40);
}

ZENITH_TEST(MathPinRuntime, Wired_GetListCountResult)
{
	Zenith_GraphBlackboard xBB;
	AppendInt(xBB, 10);
	AppendInt(xBB, 20);

	Zenith_GraphNode_GetListCount xNode;
	xNode.m_strListVar = "bag";

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const u_int uResult = Zenith_GraphNode_GetListCount::uPIN_Result;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.GetOutputPinType(uResult)), static_cast<int>(PROPERTY_TYPE_INT32));
	ZENITH_ASSERT_EQ(SlotInt(xNode.GetOutputForTest(uResult), "GetListCount.Result"), 2);

	// An ABSENT list is still 0, and still latched.
	Zenith_GraphNode_GetListCount xAbsent;
	xAbsent.m_strListVar = "neverExisted";
	ZENITH_ASSERT_EQ(static_cast<int>(xAbsent.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ(SlotInt(xAbsent.GetOutputForTest(uResult), "GetListCount.Result absent"), 0);
}

ZENITH_TEST(MathPinRuntime, Wired_LogicBoolResult)
{
	Zenith_GraphBlackboard xBB;
	SeedBools(xBB, true, true, false);

	Zenith_GraphNode_LogicBlackboardBool xNode;
	xNode.m_strVars = "a,b";		// the ONE exemption: still a DIRECT read, per operand
	xNode.m_iOp = GRAPH_LOGIC_BOOL_OP_AND;

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const u_int uResult = Zenith_GraphNode_LogicBlackboardBool::uPIN_Result;
	ZENITH_ASSERT_EQ(static_cast<int>(xNode.GetOutputPinType(uResult)), static_cast<int>(PROPERTY_TYPE_BOOL));
	const Zenith_PropertyValue* pxSlot = xNode.GetOutputForTest(uResult);
	ZENITH_ASSERT_NOT_NULL(pxSlot);
	ZENITH_ASSERT_NOT_NULL(pxSlot, "the slot must be latched");
	ZENITH_ASSERT_TRUE(pxSlot != nullptr && pxSlot->GetType() == PROPERTY_TYPE_BOOL, "the slot must carry the declared tag - a wrong tag must FAIL, not skip");
	if (pxSlot != nullptr && pxSlot->GetType() == PROPERTY_TYPE_BOOL)
	{
		ZENITH_ASSERT_TRUE(pxSlot->GetBool());
	}

	// A FAILING node latches NOTHING - the empty-operand-list contract reaches the
	// slot as well as the blackboard.
	Zenith_GraphNode_LogicBlackboardBool xEmpty;
	xEmpty.m_strVars = "";
	xEmpty.m_iOp = GRAPH_LOGIC_BOOL_OP_AND;
	const u_int uCountBeforeEmpty = xBB.GetCount();
	ZENITH_ASSERT_EQ(static_cast<int>(xEmpty.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_NULL(xEmpty.GetOutputForTest(uResult), "the empty operand guard must not latch Result");
	ZENITH_ASSERT_EQ(xBB.GetCount(), uCountBeforeEmpty);
	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
}

ZENITH_TEST(MathPinRuntime, Wired_RandomResultsLatch)
{
	// Both Random nodes stay EXEC (impure): a latched draw must not be re-rolled on
	// demand by a consumer's pull, so the only thing the migration changed is where
	// the draw is written.
	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	Zenith_GraphNode_RandomFloat xFloat;
	xFloat.m_fMin = 5.0f;
	xFloat.m_fMax = 6.0f;
	xFloat.m_iSeed = 77;
	ZENITH_ASSERT_EQ(static_cast<int>(xFloat.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const u_int uFloatResult = Zenith_GraphNode_RandomFloat::uPIN_Result;
	const float fDrawn = SlotFloat(xFloat.GetOutputForTest(uFloatResult), "RandomFloat.Result");
	ZENITH_ASSERT_TRUE(fDrawn >= 5.0f && fDrawn <= 6.0f, "RandomFloat latched %f, outside [5,6]", fDrawn);

	Zenith_GraphNode_RandomInt xInt;
	xInt.m_iMin = 2;
	xInt.m_iMax = 4;
	xInt.m_iSeed = 77;
	ZENITH_ASSERT_EQ(static_cast<int>(xInt.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));

	const u_int uIntResult = Zenith_GraphNode_RandomInt::uPIN_Result;
	const int32_t iDrawn = SlotInt(xInt.GetOutputForTest(uIntResult), "RandomInt.Result");
	ZENITH_ASSERT_TRUE(iDrawn >= 2 && iDrawn <= 4, "RandomInt latched %d, outside [2,4]", iDrawn);

	// min > max still FAILS and latches nothing.
	Zenith_GraphNode_RandomInt xBad;
	xBad.m_iMin = 5;
	xBad.m_iMax = 1;
	const u_int uCountBeforeBad = xBB.GetCount();
	ZENITH_ASSERT_EQ(static_cast<int>(xBad.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_NULL(xBad.GetOutputForTest(Zenith_GraphNode_RandomInt::uPIN_Result),
		"an invalid range must not latch RandomInt.Result");
	ZENITH_ASSERT_EQ(xBB.GetCount(), uCountBeforeBad);
	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
}

// An unbound var-or-const input takes its const. The two legs retain the
// missing/wrong-tag controls at the pin seam.
ZENITH_TEST(MathPinRuntime, ConstOperandIgnoresAbsentAndWrongTypeDecoys)
{
	Zenith_GraphBlackboard xBB;
	SeedFloat(xBB, "v", 5.0f);

	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;
	const u_int uOperand = Zenith_GraphNode_MathBlackboardFloat::uPIN_Operand;

	// (a) no wire takes the const 3, so 5 x 3.
	{
		Zenith_GraphNode_MathBlackboardFloat xNode;
		xNode.m_strVar = "v";
		xNode.m_iOp = GRAPH_MATH_FLOAT_OP_MULTIPLY;
		xNode.m_fOperand = 3.0f;
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_MathBlackboardFloat::uPIN_Result),
			"absent decoy"), 15.0f, 0.0001f);
		ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(uOperand), 0u);
	}

	// (b) an actual wrong-tag wire defaults to the const and warns once.
	{
		Zenith_GraphNode_MathBlackboardFloat xNode;
		xNode.m_strVar = "v";
		xNode.m_iOp = GRAPH_MATH_FLOAT_OP_MULTIPLY;
		xNode.m_fOperand = 3.0f;
		Zenith_PropertyValue xWrong;
		xWrong.SetInt32(42);
		xNode.SetInputForTest(uOperand, xWrong);
		ZENITH_ASSERT_EQ(static_cast<int>(xNode.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
		ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xNode.GetOutputForTest(Zenith_GraphNode_MathBlackboardFloat::uPIN_Result),
			"wrong-tag wire"), 15.0f, 0.0001f);
		ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(uOperand), 1u);
	}
}

// ★ EVERY GetInput SITS IN EXACTLY THE BRANCH ITS BLACKBOARD READ OCCUPIED. T is
// read only inside the mode-0 else. This uses a REAL resolved data edge from a
// counting pure producer: a latched slot cannot prove a conditional pull.
ZENITH_TEST(MathPinRuntime, LerpRateModeDoesNotTouchT)
{
	EnsureMathCountingProducerRegistered();

	const auto RunMode = [](int32_t iMode, const char* szValue, float fRate)
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uLerp = xDef.AddNode("LerpBlackboardFloat");
		const u_int uProducer = xDef.AddNode("Test_MathCountingFloatProducer");
		ZENITH_ASSERT_NE(uLerp, 0u);
		ZENITH_ASSERT_NE(uProducer, 0u);
		if (uLerp == 0u || uProducer == 0u)
		{
			return -1.0f;
		}

		Zenith_GraphNode_LerpBlackboardFloat xTemp;
		xTemp.m_strVar = szValue;
		xTemp.m_fTarget = 10.0f;
		xTemp.m_iMode = iMode;
		xTemp.m_fRate = fRate;
		xDef.SetNodeParamsFromInstance(uLerp, &xTemp);
		xDef.AddEdge(uSource, 0u, uLerp);
		ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uLerp, "T"));

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
		xGraph.GetBlackboard().SetValue(szValue, WireFloat(0.0f));
		Zenith_GraphContext xCtx;
		xCtx.m_fDt = 0.5f;
		xCtx.m_pxGraph = &xGraph; xCtx.m_pxBlackboard = &xGraph.GetBlackboard(); xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xCtx);
		Zenith_GraphNode* pxLerp = xGraph.FindNode(uLerp);
		ZENITH_ASSERT_NOT_NULL(pxLerp);
		return pxLerp == nullptr ? -1.0f : xGraph.GetBlackboard().GetFloat(szValue);
	};

	Zenith_GraphNode_MathTestCountingFloatProducer::s_uPullCount = 0u;
	ZENITH_ASSERT_EQ_FLOAT(RunMode(1, "rateValue", 4.0f), 2.0f, 0.0001f);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_MathTestCountingFloatProducer::s_uPullCount, 0u,
		"rate mode pulled T even though the branch never asks for it");

	ZENITH_ASSERT_EQ_FLOAT(RunMode(0, "tValue", 0.0f), 2.5f, 0.0001f);
	ZENITH_ASSERT_EQ(Zenith_GraphNode_MathTestCountingFloatProducer::s_uPullCount, 1u,
		"t mode did not pull its resolved producer exactly once");
}

// An OUTPUT slot carries an impure producer's value without any blackboard name.
ZENITH_TEST(MathPinRuntime, OutputSlotsCarryUnnamedResults)
{
	Zenith_GraphBlackboard xBB;
	Zenith_GraphContext xCtx;
	xCtx.m_pxBlackboard = &xBB;

	// A plain OUTPUT with no name at all.
	Zenith_GraphNode_RandomFloat xRandom;
	xRandom.m_fMin = 4.0f;
	xRandom.m_fMax = 4.0f;
	ZENITH_ASSERT_EQ(static_cast<int>(xRandom.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	// The slot is latched without any blackboard output write.
	ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xRandom.GetOutputForTest(Zenith_GraphNode_RandomFloat::uPIN_Result), "RandomFloat.Result"), 4.0f, 0.0001f);

	// An empty selector remains valid and the output still latches its exact
	// result without creating a blackboard entry.
	Zenith_GraphNode_MathBlackboardFloat xMath;
	xMath.m_strVar = "";
	xMath.m_iOp = GRAPH_MATH_FLOAT_OP_SUBTRACT;
	xMath.m_fOperand = 1.0f;
	ZENITH_ASSERT_EQ(static_cast<int>(xMath.Execute(xCtx)), static_cast<int>(GRAPH_NODE_STATUS_SUCCESS));
	ZENITH_ASSERT_EQ_FLOAT(SlotFloat(xMath.GetOutputForTest(Zenith_GraphNode_MathBlackboardFloat::uPIN_Result), "empty Math.Result"), -1.0f, 0.0001f);
	ZENITH_ASSERT_EQ(xBB.GetCount(), 0u);
	ZENITH_ASSERT_NULL(xBB.TryGetValue(""));
}

#endif // ZENITH_TESTING
