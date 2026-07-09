#pragma once

#include "Zenith.h"

//------------------------------------------------------------------------------
// Zenith_GraphOps - typed operation codes for the engine Behaviour Graph nodes
// whose `m_iOp` (and siblings) used to be raw magic integers at both the call
// site (Zenith_GraphBuilder::ParamInt(uNode, "m_iOp", 2)) and the node's
// Execute() switch (case 2: ...).
//
// Home = EntityComponent (not Scripting): these vocabularies belong to the
// engine node library that defines their semantics
// (Zenith_GraphNode_Registration*.cpp). The Scripting builder stays leaf-safe -
// it offers only a GENERIC Zenith_GraphBuilder::ParamEnum<TEnum>() and never
// names these enums.
//
// SERIALIZATION CONTRACT: each enumerator's value is pinned to the integer the
// node's Execute() switch already used, and the underlying type is fixed to
// int32_t (the `m_iOp` ZENITH_PROPERTY type). So authoring a node with an enum
// value serializes byte-for-byte identically to the old ParamInt literal, and
// the switch still `default:`s the same way for out-of-range values. Never
// renumber - append only.
//------------------------------------------------------------------------------

// CompareBlackboardFloat::m_iOp. Float compare has NO not-equal (0-4 only).
enum Zenith_GraphCompareFloatOp : int32_t
{
	GRAPH_COMPARE_FLOAT_OP_LESS          = 0,
	GRAPH_COMPARE_FLOAT_OP_LESS_EQUAL    = 1,
	GRAPH_COMPARE_FLOAT_OP_GREATER       = 2,
	GRAPH_COMPARE_FLOAT_OP_GREATER_EQUAL = 3,
	GRAPH_COMPARE_FLOAT_OP_EQUAL         = 4,
};

// CompareBlackboardInt::m_iOp. The int sibling additionally supports NOT_EQUAL
// (5) and defaults to EQUAL (4) - both distinct from the float node above.
enum Zenith_GraphCompareIntOp : int32_t
{
	GRAPH_COMPARE_INT_OP_LESS          = 0,
	GRAPH_COMPARE_INT_OP_LESS_EQUAL    = 1,
	GRAPH_COMPARE_INT_OP_GREATER       = 2,
	GRAPH_COMPARE_INT_OP_GREATER_EQUAL = 3,
	GRAPH_COMPARE_INT_OP_EQUAL         = 4,
	GRAPH_COMPARE_INT_OP_NOT_EQUAL     = 5,
};

// CompareBlackboardEntity::m_iOp. Only equality is meaningful; the node treats
// ANY value other than NOT_EQUAL as EQUAL (a ternary, not a defaulting switch) -
// keep that when using this enum.
enum Zenith_GraphEntityCompareOp : int32_t
{
	GRAPH_ENTITY_COMPARE_OP_EQUAL     = 0,
	GRAPH_ENTITY_COMPARE_OP_NOT_EQUAL = 1,
};

// MathBlackboardFloat::m_iOp. 6-8 are unary (operand ignored); DIVIDE/MODULO by
// zero FAIL the node (loud, not NaN-quiet).
enum Zenith_GraphMathFloatOp : int32_t
{
	GRAPH_MATH_FLOAT_OP_SUBTRACT = 0,
	GRAPH_MATH_FLOAT_OP_MULTIPLY = 1,
	GRAPH_MATH_FLOAT_OP_DIVIDE   = 2,
	GRAPH_MATH_FLOAT_OP_MODULO   = 3,
	GRAPH_MATH_FLOAT_OP_MIN      = 4,
	GRAPH_MATH_FLOAT_OP_MAX      = 5,
	GRAPH_MATH_FLOAT_OP_ABS      = 6,
	GRAPH_MATH_FLOAT_OP_SIN      = 7,
	GRAPH_MATH_FLOAT_OP_COS      = 8,
};
