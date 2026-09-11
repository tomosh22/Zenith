//------------------------------------------------------------------------------
// Pin-table coverage for the Scene node TU. Included at the bottom of
// Zenith_GraphNode_Registration_Scene.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes and this TU's registrar are still in scope.
//
// The shared machinery (the totality walk, the registrar swap and its RAII
// restore) is Zenith_GraphPinTotality.TestHarness.inl.
//
// This TU has exactly ONE blackboard-variable-name property across its four node
// types - OnSceneLoaded.m_strStorePathVar - so the spot check is one assertion.
// The other three nodes carry only m_strScenePath, an ASSET PATH, which is not a
// blackboard name and is deliberately not a pin: they have no pin table at all
// and are OPAQUE to the validator by design, which the totality walk accepts
// because opacity is only a fault for a type that HAS a variable-name property.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, SceneTotality)
{
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_Scene, "_Scene.cpp", nullptr, 0u);
}

ZENITH_TEST(GraphPinTable, SceneRoleSpotCheck)
{
	// The loaded path arrives as the event's STRING payload and is stashed into
	// a configured destination - SELECTOR_WRITE STRING, not an OUTPUT ANY.
	Zenith_CheckGraphPin("OnSceneLoaded", "StorePath", GRAPH_PIN_ROLE_SELECTOR_WRITE, PROPERTY_TYPE_STRING, "m_strStorePathVar");
}

#endif // ZENITH_TESTING
