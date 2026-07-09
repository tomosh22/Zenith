//------------------------------------------------------------------------------
// Op-execution tests for the Blackboard/Maths node TU (CompareBlackboardEntity,
// MathBlackboardFloat). Included at the bottom of
// Zenith_GraphNode_Registration_Math.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes are still in scope. The enum-value wall lives
// in the sibling Zenith_GraphNode_Registration.Tests.inl.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"
#include "Scripting/Zenith_GraphBlackboard.h"

#ifdef ZENITH_TESTING

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

#endif // ZENITH_TESTING
