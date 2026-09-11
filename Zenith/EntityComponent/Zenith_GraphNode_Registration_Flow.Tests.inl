//------------------------------------------------------------------------------
// Pin-table coverage for the Flow node TU. Included at the bottom of
// Zenith_GraphNode_Registration_Flow.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes and this TU's registrar are still in scope.
//
// What the totality walk proves, why the registry is SWAPPED rather than
// filtered, and why the restore is RAII all live ONCE, in the shared harness:
// Zenith_GraphPinTotality.TestHarness.inl. This file carries only what is
// specific to this TU - its registrar and its representative pins.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, FlowTotality)
{
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_Flow, "_Flow.cpp", nullptr, 0u);
}

ZENITH_TEST(GraphPinTable, FlowRoleSpotCheck)
{
	// ★ READWRITE: WaitForCondition READS the flag every tick and, under
	// m_bResetOnPass, WRITES it back to false on the pass. Branch carries the
	// same property NAME read-only (that half is asserted in the core TU's spot
	// check) - the role follows the Execute body, not the spelling.
	Zenith_CheckGraphPin("WaitForCondition", "Condition", GRAPH_PIN_ROLE_SELECTOR_READWRITE, PROPERTY_TYPE_BOOL, "m_strConditionVar");

	// LIST: the blackboard's parallel list store, which holds no
	// Zenith_PropertyValue and is therefore never declared or typed.
	Zenith_CheckGraphPin("ForEach", "List", GRAPH_PIN_ROLE_LIST, eGRAPH_PIN_TYPE_ANY, "m_strListVar");
	// The two destinations ForEach publishes per element. Element is genuinely
	// ANY (the list's element type is not knowable from the node); Index is not.
	Zenith_CheckGraphPin("ForEach", "Element", GRAPH_PIN_ROLE_SELECTOR_WRITE, eGRAPH_PIN_TYPE_ANY, "m_strElementVar");
	Zenith_CheckGraphPin("ForEach", "Index", GRAPH_PIN_ROLE_SELECTOR_WRITE, PROPERTY_TYPE_INT32, "m_strIndexVar");

	// The two dispatchers read their key and never write it.
	Zenith_CheckGraphPin("SwitchOnInt", "Value", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_INT32, "m_strVar");
	Zenith_CheckGraphPin("StateMachine", "State", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_INT32, "m_strStateVar");
}

#endif // ZENITH_TESTING
