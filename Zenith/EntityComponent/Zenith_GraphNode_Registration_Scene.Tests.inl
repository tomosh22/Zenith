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

//==============================================================================
// PIN RUNTIME for this TU (B-6.7) - and the answer is that there is NOTHING to
// migrate, which is worth one test rather than one sentence.
//
// This TU's ONLY pin is OnSceneLoaded.StorePath, a SELECTOR_WRITE: a configured
// DESTINATION NAME the event source stashes its payload under. A SELECTOR is
// never a wire (Scripting/CLAUDE.md, Validation), so it has no runtime accessor
// to move onto, no uPIN_ constant, no dual-write, no "" divergence and nothing
// for C-1 to delete. The migrated sibling TUs carry a <TU>PinIndicesMatchTables
// row because a table REORDER silently re-points their uPIN_ constants; this TU
// carries one because the absence of a value pin is a claim that should fail
// loudly if someone adds one without migrating an Execute.
//==============================================================================

ZENITH_TEST(GraphPinTable, ScenePinIndicesMatchTables)
{
	const Zenith_GraphPinTable& xLoaded = Zenith_GraphNode_OnSceneLoaded::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xLoaded.GetPinCount(), 1u, "OnSceneLoaded gained or lost a pin");
	if (xLoaded.GetPinCount() == 1u)
	{
		const Zenith_GraphPinDesc& xDesc = xLoaded.GetPinAt(0u);
		ZENITH_ASSERT_STREQ(xDesc.m_szName, "StorePath",
			"OnSceneLoaded's only pin is '%s', not 'StorePath'",
			xDesc.m_szName != nullptr ? xDesc.m_szName : "(null)");
		ZENITH_ASSERT_EQ(static_cast<int>(xDesc.m_eRole), static_cast<int>(GRAPH_PIN_ROLE_SELECTOR_WRITE),
			"StorePath became a value pin - its Execute still writes the blackboard DIRECTLY and would have to be migrated");
	}

	// The default destination name, which is what makes the direct SetValue
	// observable to a downstream chain at all.
	Zenith_GraphNode_OnSceneLoaded xLoadedNode;
	ZENITH_ASSERT_STREQ(xLoadedNode.m_strStorePathVar.c_str(), "loadedScene",
		"OnSceneLoaded's stash destination default moved");

	// The other three classes carry no table at all - m_strScenePath is an ASSET
	// path, not a blackboard name. A stack instance's null GetPinTableVirtual() is
	// the class answering for itself; the live registry would answer for whatever
	// a game registered last (the precedent is
	// BehaviourGraph.PinRuntime_OpaqueNodeStillBadAccess).
	Zenith_GraphNode_LoadSceneByAsset xLoad;
	ZENITH_ASSERT_NULL(xLoad.GetPinTableVirtual(), "LoadSceneByAsset gained a pin table");
	Zenith_GraphNode_UnloadScene xUnload;
	ZENITH_ASSERT_NULL(xUnload.GetPinTableVirtual(), "UnloadScene gained a pin table");
	Zenith_GraphNode_SetActiveScene xSetActive;
	ZENITH_ASSERT_NULL(xSetActive.GetPinTableVirtual(), "SetActiveScene gained a pin table");
}

#endif // ZENITH_TESTING
