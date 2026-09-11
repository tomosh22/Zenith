//------------------------------------------------------------------------------
// Routable-FAILURE coverage for the AI node TU. Included at the bottom of
// Zenith_GraphNode_Registration_AI.cpp (ZENITH_TESTING), where the
// anonymous-namespace node classes are still in scope.
//
// ONE table-driven test per owning TU. What each row proves, why the wired
// fail-probe is the positive control, and why the anchor/probes are engine
// nodes addressed by name all live ONCE, in the shared harness:
// Zenith_GraphNodeFailurePin.TestHarness.inl. This file carries only what is
// specific to this TU - the scene fixture, the rows, and the non-opted node.
//
// The harness's positive control earns its keep hardest HERE: EnsureNavAgent's
// other non-SUCCESS status is RUNNING, which looks identical to a routed
// FAILURE from pin 0's side (neither successor runs). Only the fail-probe
// separates them.
//
// CROSS-REFERENCES, deliberately not duplicated here:
//   * the runtime SEMANTICS of the pin are the FailurePin_* tests in
//     Zenith/Scripting/Zenith_Scripting.Tests.inl;
//   * EnsureNavAgent's full status table without a mesh - including the
//     RUNNING row this file cannot reach - is
//     Zenith_GraphComponent.Tests.inl:3929-3977, and the perception fixtures
//     with a REGISTERED agent are :3101-3136.
//
// NavMoveTo is deliberately NOT in this table: it is a RUNNING/suspending node
// whose failure handling interacts with the chain cursor, so it stays out of
// the opt-in set until that interaction is designed.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Zenith_GraphNodeFailurePin.TestHarness.inl"
#include "UnitTests/Zenith_TempScene.h"

ZENITH_TEST(GraphNodeFailurePin, AIOptIns)
{
	// Self is a REAL agent entity, so every row below fails on its own named
	// branch rather than on the invalid-target guard each node opens with.
	Zenith_TempScene xTempScene("FailurePinAIScene");
	Zenith_Entity xSelf = xTempScene.CreateEntity("FailurePinAgent");
	xSelf.AddComponent<Zenith_AIAgentComponent>();

	// ★ AN UNCONFIGURED NavMeshComponent, NEVER A CONFIGURED-BUT-UNLOADED ONE.
	// EnsureNavAgent reads the ASSET REF to tell "nothing is coming" (FAILURE,
	// :119) from "it is coming next frame" (RUNNING, :120). An empty ref is the
	// first; a real ref whose deferred OnStart has not run is the second, and
	// wiring a failure handler for THAT would fire on a mesh that then loads.
	Zenith_Entity xNavHolder = xTempScene.CreateEntity("FailurePinNavHolder");
	Zenith_NavMeshComponent& xNavMesh = xNavHolder.AddComponent<Zenith_NavMeshComponent>();
	ZENITH_ASSERT_TRUE(xNavMesh.GetAssetRef().empty(), "the fixture needs an UNCONFIGURED navmesh ref");

	const Zenith_FailurePinCase axCases[] =
	{
		// EnsureNavAgent: FAILURE at Registration_AI.cpp:119 (the UNCONFIGURED
		// ref - nothing is coming). The target has a Zenith_AIAgentComponent and
		// the active scene has a Zenith_NavMeshComponent, so the :90 / :95 / :101
		// misconfiguration guards are all cleared first.
		{ "EnsureNavAgent", nullptr },
		// SetNavDestination: FAILURE at Registration_AI.cpp:306 (NO BOUND
		// AGENT). Nothing in this fixture ever binds one - EnsureNavAgent
		// returns above its allocation - so the node reaches its own gate.
		{ "SetNavDestination", nullptr },
		// FindRandomReachablePoint: FAILURE at Registration_AI.cpp:447 (no bound
		// agent, hence no mesh). This is the DEEPEST branch reachable headless:
		// :459 ("no reachable point in the radius") needs a LOADED navmesh,
		// which needs a bake - Zenith_GraphComponent.Tests.inl owns that path.
		{ "FindRandomReachablePoint", nullptr },
		// QueryPrimaryPerceivedTarget: FAILURE at Registration_AI.cpp:544
		// (nothing perceived). Self is a valid entity that was never registered
		// as a perception agent, so GetPrimaryTarget answers INVALID_ENTITY_ID
		// without the system being initialised at all.
		{ "QueryPrimaryPerceivedTarget", nullptr },
		// QueryLastHeardSound: FAILURE at Registration_AI.cpp:578 (nothing
		// heard) - the same unregistered-agent route, via a default-constructed
		// Zenith_LastHeardSound whose m_bValid is false.
		{ "QueryLastHeardSound", nullptr },
	};

	Zenith_CheckFailurePinTable(axCases, static_cast<u_int>(sizeof(axCases) / sizeof(axCases[0])), xSelf);

	// Nothing in the fixture may have allocated an agent behind our back - a
	// bound agent would make three of the rows above pass for the wrong reason.
	ZENITH_ASSERT_NULL(xSelf.GetComponent<Zenith_AIAgentComponent>().GetNavMeshAgent(),
		"a failing EnsureNavAgent must not have allocated an agent");

	// StopNav's only FAILURE is "no agent to stop", which a teardown chain has
	// no handling for.
	Zenith_CheckNodeIsNotOptedIn("StopNav");
}

//------------------------------------------------------------------------------
// Pin-table coverage for this TU. What the totality walk proves, why the
// registry is SWAPPED rather than filtered, and why the restore is RAII all live
// ONCE, in Zenith_GraphPinTotality.TestHarness.inl; only this TU's registrar and
// its representative pins are here.
//------------------------------------------------------------------------------

#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

ZENITH_TEST(GraphPinTable, AITotality)
{
	// No exemptions: every m_str*Var* property in this TU is expressible as a
	// pin - m_strListVar names ONE list, not a comma-separated set.
	Zenith_CheckPinTableTotality(&Zenith_RegisterEngineGraphNodes_AI, "_AI.cpp", nullptr, 0u);
}

ZENITH_TEST(GraphPinTable, AIRoleSpotCheck)
{
	// LIST: the blackboard's parallel list store, which holds no
	// Zenith_PropertyValue and is therefore never typed. Count beside it is an
	// ordinary computed OUTPUT.
	Zenith_CheckGraphPin("QueryPerceivedTargets", "List", GRAPH_PIN_ROLE_LIST, eGRAPH_PIN_TYPE_ANY, "m_strListVar");
	Zenith_CheckGraphPin("QueryPerceivedTargets", "Count", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_INT32, "m_strCountVar");

	// INPUT_VAR_OR_CONST with both halves bound.
	Zenith_CheckGraphPin("SetNavSpeed", "Speed", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_FLOAT, "m_strSpeedVar");
	const Zenith_GraphPinDesc* pxSpeed = Zenith_FindGraphPin("SetNavSpeed", "Speed");
	ZENITH_ASSERT_NOT_NULL(pxSpeed, "SetNavSpeed must declare a Speed pin");
	if (pxSpeed != nullptr)
	{
		ZENITH_ASSERT_STREQ(pxSpeed->m_szConstProperty, "m_fSpeed",
			"SetNavSpeed.Speed lost its inline-constant half");
	}

	// ★ The two TARGET flavours differ ONLY in the accepted-type mask: a
	// destination may be an EntityID (follow) or a VECTOR3 (a fixed point),
	// while the mover itself must be an EntityID.
	const Zenith_GraphPinDesc* pxDestination = Zenith_FindGraphPin("NavMoveTo", "Destination");
	const Zenith_GraphPinDesc* pxMover = Zenith_FindGraphPin("NavMoveTo", "Target");
	ZENITH_ASSERT_NOT_NULL(pxDestination, "NavMoveTo must declare a Destination pin");
	ZENITH_ASSERT_NOT_NULL(pxMover, "NavMoveTo must declare a Target pin");
	if (pxDestination != nullptr && pxMover != nullptr)
	{
		ZENITH_ASSERT_EQ(static_cast<int>(pxDestination->m_eRole), static_cast<int>(GRAPH_PIN_ROLE_TARGET_REF),
			"NavMoveTo.Destination is a runtime-resolved reference, not a value input");
		ZENITH_ASSERT_EQ(pxDestination->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_POSITION,
			"NavMoveTo.Destination must accept ENTITY_ID or VECTOR3");
		ZENITH_ASSERT_EQ(pxMover->m_uAcceptedTypeMask, uGRAPH_PIN_ACCEPT_TARGET_ENTITY,
			"NavMoveTo.Target must accept a packed ENTITY_ID and nothing else");
	}

	// Nav state reads are the node's own computed OUTPUTs - m_strStateVar holds
	// the 0-3 code this node derives, not a state NAME.
	Zenith_CheckGraphPin("ReadNavState", "State", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_INT32, "m_strStateVar");

	// ★ SAME PROPERTY NAME, OPPOSITE ROLES: QueryLastHeardSound WRITES the heard
	// position; EmitSoundStimulus RESOLVES one to emit at. The role follows the
	// Execute body, never the spelling.
	Zenith_CheckGraphPin("QueryLastHeardSound", "Position", GRAPH_PIN_ROLE_OUTPUT, PROPERTY_TYPE_VECTOR3, "m_strPositionVar");
	Zenith_CheckGraphPin("EmitSoundStimulus", "Position", GRAPH_PIN_ROLE_TARGET_REF, eGRAPH_PIN_TYPE_ANY, "m_strPositionVar");
}

#endif // ZENITH_TESTING
