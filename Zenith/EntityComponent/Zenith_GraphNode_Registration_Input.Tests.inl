//------------------------------------------------------------------------------
// Pin-table coverage for the Input node TU. Included at the bottom of
// Zenith_GraphNode_Registration_Input.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes and this TU's registrar are still in scope.
//
// The shared machinery (the totality walk, the registrar swap and its RAII
// restore) is Zenith_GraphPinTotality.TestHarness.inl; this file carries only
// this TU's registrar and its representative pins.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, InputTotality)
{
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_Input, "_Input.cpp", nullptr, 0u);
}

ZENITH_TEST(GraphPinTable, InputRoleSpotCheck)
{
	// This TU is almost entirely OUTPUTS: a query node's result var is its own
	// computed value. The types are the ones the Execute bodies actually Set*.
	Zenith_CheckGraphPin("ReadKeyState", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_BOOL, "m_strResultVar");
	Zenith_CheckGraphPin("ReadMovementAxis", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_VECTOR3, "m_strResultVar");
	// ★ VECTOR2, not VECTOR3: the mouse/action 2D reads write SetVector2, and a
	// VECTOR3 here would make every correct graph a type error.
	Zenith_CheckGraphPin("ReadMouseDelta", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_VECTOR2, "m_strResultVar");
	Zenith_CheckGraphPin("ReadActionAxis2D", "Result", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_VECTOR2, "m_strResultVar");

	// ReadPointer publishes FOUR independent results, each written only when its
	// own var is named.
	Zenith_CheckGraphPin("ReadPointer", "Down", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_BOOL, "m_strDownVar");
	Zenith_CheckGraphPin("ReadPointer", "Count", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_INT32, "m_strCountVar");

	// The one non-OUTPUT: an event SOURCE stashing its payload into a configured
	// destination (the collision-source pattern).
	Zenith_CheckGraphPin("OnMouseMoved", "StoreDelta", GRAPH_PIN_ROLE_SELECTOR_WRITE, PROPERTY_TYPE_VECTOR2, "m_strStoreDeltaVar");
}

#endif // ZENITH_TESTING
