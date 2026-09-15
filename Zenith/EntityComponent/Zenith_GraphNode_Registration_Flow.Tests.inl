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
	Zenith_CheckGraphPin("SwitchOnInt", "Value", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_INT32, "");
	Zenith_CheckGraphPin("StateMachine", "State", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_INT32, "");
	// The string switch's scalar input had no spot check before B-6.10.
	Zenith_CheckGraphPin("SwitchOnString", "Value", GRAPH_PIN_ROLE_INPUT, PROPERTY_TYPE_STRING, "");
}

//==============================================================================
// PIN RUNTIME for this TU (B-6.10) - THREE pins are LIVE, and only three.
//
// SwitchOnInt.Value, SwitchOnString.Value and StateMachine.State are read
// through Zenith_GraphNode::GetInput. Everything else in the TU stays a direct
// blackboard access BY ROLE: WaitForCondition.Condition is SELECTOR_READWRITE,
// ForEach.Element/.Index are SELECTOR_WRITE destinations, ForEach.List names the
// parallel LIST store, and CallGraph's asset path is not a pin at all. There is
// no OUTPUT pin in this TU, so no `""` divergence row exists here.
//
// ★ EVERY ROW RUNS ON A GRAPH INSTANCE, and it has to. All three migrated nodes
// reach xContext.m_pxGraph->RunChainFromPin, so a directly-constructed node with
// a bare context - the shape every sibling TU's wired rows use - would
// dereference null. That is also why this TU contributes ZERO
// direct-construction rows to the B-7.6 ledger: these rows are already
// graph-instance rows and need no rewrite.
//
// ★ EVERY CONFIGURATION PROPERTY GOES THROUGH THE PARAM BLOB, BEFORE
// InitialiseFromDefinition. SwitchOnString is worse still:
// its EnsureCasesParsed() latch is not reset by ApplyNodeParams, so a late
// m_strCases leaves m_axCases EMPTY and every case routes to the default pin.
//
// ORDER: AddNode -> params into the blob -> AddEdge (the case/state count is
// applied, so pin N exists) -> InitialiseFromDefinition -> FindNode ->
// SetInputForTest (the override SURVIVES the build - BuildPinStateFromTables
// clears the three BINDING arrays, not m_axTestOverrides - and is consulted
// before the connected, var and const paths) -> fire.
//
// ★ FRESH GRAPH PER LEG. m_iActivePin and m_iCurrentState are per-INSTANCE
// state.
//
// ★ These fixtures never reach a counted census log: the per-game census parses
// `zenith test <G> --headless` runs, which pass --skip-unit-tests.
//==============================================================================

namespace
{
	// ---- value makers -------------------------------------------------------

	inline Zenith_PropertyValue FlowPin_Str(const char* szValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetString(szValue);
		return xValue;
	}

	inline Zenith_PropertyValue FlowPin_Int(int32_t iValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(iValue);
		return xValue;
	}

	inline Zenith_PropertyValue FlowPin_Float(float fValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetFloat(fValue);
		return xValue;
	}

	// ---- the scratch producer ----------------------------------------------

	// PURE, one INT32 OUTPUT, an execute counter, and a plain member for the
	// value it publishes. The counter is the only way to tell "the consumer did
	// not re-read" from "the consumer re-read and got the same answer": a
	// suspended SwitchOnInt must leave it exactly where it was.
	//
	// The output slot is the only path the value can take to the consumer.
	class Test_FlowPinProducerNode : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Test_FlowPinProducerNode)
	public:
		ZENITH_PROPERTY(int32_t, m_iValue, 0)

		static constexpr u_int uPIN_Value = 0u;

		ZENITH_GRAPH_PINS_BEGIN(Test_FlowPinProducerNode)
		ZENITH_GRAPH_PIN_OUTPUT(Value, PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			++m_uExecuteCount;
			SetOutput<int32_t>(xContext, uPIN_Value, m_iValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "Test_FlowPinProducer"; }

		u_int m_uExecuteCount = 0;
	};

	// ★ KEYED ON THE REGISTRY, never on a static bool. This TU owns
	// GraphPinTable.FlowTotality, whose registrar swap ResetForTests()s every row
	// and restores only the ENGINE set - a latch would stay true with the scratch
	// type gone, and test ORDER would decide whether the rows below resolve.
	inline void FlowPin_EnsureRegistry()
	{
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		if (xRegistry.Find("Test_FlowPinProducer") != nullptr)
		{
			return;
		}
		xRegistry.RegisterNodeType<Test_FlowPinProducerNode>("Test_FlowPinProducer", GRAPH_EVENT_NONE, 0, false,
			"Test", false, true);
	}

	// ---- definition authoring ----------------------------------------------

	struct FlowPin_Param
	{
		const char* m_szName;
		Zenith_PropertyValue m_xValue;
	};

	// The ONE way a property reaches a node in this file. Round-trips through a
	// temp instance: ApplyNodeParams seeds it with whatever the blob already
	// holds (AddNode captures the defaults), the named properties are written,
	// and SetNodeParamsFromInstance serialises the whole set back. Nothing here
	// touches a LIVE node - the graph's build is what must see these values.
	inline void FlowPin_SetParams(Zenith_GraphDefinition& xDefinition, u_int uNodeID, const char* szTypeName,
		const FlowPin_Param* paxParams, u_int uCount)
	{
		const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find(szTypeName);
		ZENITH_ASSERT_NOT_NULL(pxInfo, "'%s' is not registered - the engine node library did not load", szTypeName);
		if (pxInfo == nullptr || pxInfo->m_pfnCreate == nullptr || pxInfo->m_pfnGetPropertyTable == nullptr)
		{
			return;
		}
		const Zenith_PropertyTable* pxTable = pxInfo->m_pfnGetPropertyTable();
		Zenith_GraphNode* pxTemp = pxInfo->m_pfnCreate();
		xDefinition.ApplyNodeParams(uNodeID, pxTemp, *pxInfo);
		for (u_int u = 0; u < uCount; ++u)
		{
			const Zenith_ReflectedProperty* pxProperty = pxTable->FindProperty(paxParams[u].m_szName);
			ZENITH_ASSERT_NOT_NULL(pxProperty, "'%s' has no property '%s'", szTypeName, paxParams[u].m_szName);
			if (pxProperty != nullptr)
			{
				Zenith_PropertySystem::SetPropertyValue(pxTemp, *pxProperty, paxParams[u].m_xValue);
			}
		}
		xDefinition.SetNodeParamsFromInstance(uNodeID, pxTemp);
		delete pxTemp;
	}

	// One witness per exec pin: a SetBlackboardBool writing TRUE to a variable
	// named for the pin it hangs off.
	//
	// ★ THE NAME GOES THROUGH THE BLOB, and it must be distinct from every node
	// default in play: SetBlackboardBool defaults to "flag", SetBlackboardInt to
	// "value" - which is SwitchOnInt's OWN key - so a witness left on its default
	// would overwrite the very variable the dispatcher reads.
	//
	// m_bValue stays at its default TRUE (a non-zero, so "the witness ran" is not
	// the stamped BOOL zero); a pin that did NOT run leaves its variable ABSENT,
	// which is what every negative assertion below checks.
	inline u_int FlowPin_AddWitness(Zenith_GraphDefinition& xDefinition, u_int uSrcNodeID, u_int uSrcPin,
		const char* szVariable)
	{
		const u_int uWitness = xDefinition.AddNode("SetBlackboardBool");
		if (uWitness == 0u)
		{
			return 0u;
		}
		const FlowPin_Param axParams[] = { { "m_strVariable", FlowPin_Str(szVariable) } };
		FlowPin_SetParams(xDefinition, uWitness, "SetBlackboardBool", axParams, 1u);
		ZENITH_ASSERT_TRUE(xDefinition.AddEdge(uSrcNodeID, uSrcPin, uWitness), "the witness edge must be accepted (pin %u)", uSrcPin);
		return uWitness;
	}

	// ---- driving + observing -----------------------------------------------

	inline void FlowPin_FireOneUpdate(Zenith_BehaviourGraph& xGraph)
	{
		Zenith_GraphContext xContext;
		xContext.m_fDt = 0.016f;
		xContext.m_pxGraph = &xGraph;
		xContext.m_pxBlackboard = &xGraph.GetBlackboard();
		xGraph.FireEvent(GRAPH_EVENT_ON_UPDATE, xContext);
	}

	inline bool FlowPin_Ran(Zenith_BehaviourGraph& xGraph, const char* szVariable)
	{
		const Zenith_PropertyValue* pxValue = xGraph.GetBlackboard().TryGetValue(szVariable);
		return pxValue != nullptr && pxValue->GetType() == PROPERTY_TYPE_BOOL && pxValue->GetBool();
	}

	// ★ ABSENT, not false. A witness that never ran wrote NOTHING, and "false"
	// would also be the answer a BOOL slot's stamped zero gives.
	inline void FlowPin_AssertAbsent(Zenith_BehaviourGraph& xGraph, const char* szVariable)
	{
		ZENITH_ASSERT_NULL(xGraph.GetBlackboard().TryGetValue(szVariable),
			"'%s' exists: the pin it witnesses ran when it must not have", szVariable);
	}

	// One row of the index contract.
	inline void FlowPin_CheckPin(const Zenith_GraphPinTable& xPins, u_int uIndex, const char* szName,
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
}

// ★ TABLE ORDER IS THE CONTRACT. A pin INDEX is what every accessor addresses,
// so a reorder - or an inserted pin - silently re-points a uPIN_ constant at a
// different descriptor, and a GetInput aimed at a SELECTOR or LIST pin is a
// silent type-zero plus one BADACCESS line.
//
// TEN classes are registered by this TU. FIVE carry a pin table (SwitchOnInt,
// SwitchOnString, StateMachine, ForEach, WaitForCondition) and five carry none
// (Selector, Sequence, Repeat, CallGraph, Cooldown) - proved through a STACK
// instance's null GetPinTableVirtual(), not through the live registry. Only the
// THREE migrated classes declare a uPIN_ constant; ForEach and WaitForCondition
// address no pin from their Execute and are asserted by literal index.
//
// ★ SwitchOnString.Value had NO coverage at all before this row -
// FlowRoleSpotCheck checks SwitchOnInt and StateMachine and stops.
ZENITH_TEST(GraphPinTable, FlowPinIndicesMatchTables)
{
	const Zenith_GraphPinTable& xSwitchInt = Zenith_GraphNode_SwitchOnInt::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSwitchInt.GetPinCount(), 1u, "SwitchOnInt gained or lost a pin");
	FlowPin_CheckPin(xSwitchInt, Zenith_GraphNode_SwitchOnInt::uPIN_Value, "Value", GRAPH_PIN_ROLE_INPUT,
		"SwitchOnInt");

	const Zenith_GraphPinTable& xSwitchString = Zenith_GraphNode_SwitchOnString::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xSwitchString.GetPinCount(), 1u, "SwitchOnString gained or lost a pin");
	FlowPin_CheckPin(xSwitchString, Zenith_GraphNode_SwitchOnString::uPIN_Value, "Value", GRAPH_PIN_ROLE_INPUT,
		"SwitchOnString");

	const Zenith_GraphPinTable& xMachine = Zenith_GraphNode_StateMachine::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xMachine.GetPinCount(), 1u, "StateMachine gained or lost a pin");
	FlowPin_CheckPin(xMachine, Zenith_GraphNode_StateMachine::uPIN_State, "State", GRAPH_PIN_ROLE_INPUT,
		"StateMachine");

	// ForEach: a LIST name and two SELECTOR_WRITE destinations. Nothing here is a
	// wire, so no constant exists - but an INPUT pin APPEARING in this table is
	// exactly what this row would notice.
	const Zenith_GraphPinTable& xForEach = Zenith_GraphNode_ForEach::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xForEach.GetPinCount(), 3u, "ForEach gained or lost a pin");
	FlowPin_CheckPin(xForEach, 0u, "List", GRAPH_PIN_ROLE_LIST, "ForEach");
	FlowPin_CheckPin(xForEach, 1u, "Element", GRAPH_PIN_ROLE_SELECTOR_WRITE, "ForEach");
	FlowPin_CheckPin(xForEach, 2u, "Index", GRAPH_PIN_ROLE_SELECTOR_WRITE, "ForEach");

	const Zenith_GraphPinTable& xWaitFor = Zenith_GraphNode_WaitForCondition::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xWaitFor.GetPinCount(), 1u, "WaitForCondition gained or lost a pin");
	FlowPin_CheckPin(xWaitFor, 0u, "Condition", GRAPH_PIN_ROLE_SELECTOR_READWRITE, "WaitForCondition");

	// The five with no pin block at all. A stack instance answers null from the
	// base class's default; the macro is what would override it.
	{
		Zenith_GraphNode_Selector xSelector;
		ZENITH_ASSERT_NULL(xSelector.GetPinTableVirtual(), "Selector gained a pin table");
		Zenith_GraphNode_Sequence xSequence;
		ZENITH_ASSERT_NULL(xSequence.GetPinTableVirtual(), "Sequence gained a pin table");
		Zenith_GraphNode_Repeat xRepeat;
		ZENITH_ASSERT_NULL(xRepeat.GetPinTableVirtual(), "Repeat gained a pin table");
		Zenith_GraphNode_CallGraph xCallGraph;
		ZENITH_ASSERT_NULL(xCallGraph.GetPinTableVirtual(), "CallGraph gained a pin table");
		Zenith_GraphNode_Cooldown xCooldown;
		ZENITH_ASSERT_NULL(xCooldown.GetPinTableVirtual(), "Cooldown gained a pin table");
	}
}

// Three cases from base 0 plus a default pin. The WIRE says case 2, so the pin
// that ran names the leg that won - and neither is
// the default pin, which a node that read nothing at all would take only by
// coincidence.
ZENITH_TEST(FlowPinRuntime, Wired_SwitchOnIntValueFromWire)
{
	FlowPin_EnsureRegistry();
	const u_int uValuePin = Zenith_GraphNode_SwitchOnInt::uPIN_Value;

	// (a) the wire selects case 2.
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSwitch = xDef.AddNode("SwitchOnInt");
		ZENITH_ASSERT_NE(uSource, 0u);
		ZENITH_ASSERT_NE(uSwitch, 0u);
		const FlowPin_Param axParams[] =
		{
			{ "m_iCaseBase", FlowPin_Int(0) },
			{ "m_iCaseCount", FlowPin_Int(3) },
		};
		FlowPin_SetParams(xDef, uSwitch, "SwitchOnInt", axParams, 2u);
		xDef.AddEdge(uSource, 0u, uSwitch);
		FlowPin_AddWitness(xDef, uSwitch, 0u, "case0Ran");
		FlowPin_AddWitness(xDef, uSwitch, 1u, "case1Ran");
		FlowPin_AddWitness(xDef, uSwitch, 2u, "case2Ran");
		FlowPin_AddWitness(xDef, uSwitch, 3u, "defaultRan");

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);

		Zenith_GraphNode* pxSwitch = xGraph.FindNode(uSwitch);
		ZENITH_ASSERT_NOT_NULL(pxSwitch);
		if (pxSwitch == nullptr)
		{
			return;
		}
		pxSwitch->SetInputForTest(uValuePin, FlowPin_Int(2));			// the WIRE leg
		FlowPin_FireOneUpdate(xGraph);

		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "case2Ran"), "the wired case did not run");
		FlowPin_AssertAbsent(xGraph, "case0Ran");
		FlowPin_AssertAbsent(xGraph, "case1Ran");
		FlowPin_AssertAbsent(xGraph, "defaultRan");
		ZENITH_ASSERT_EQ(pxSwitch->GetBadAccessWarningCountForTest(), 0u);
	}

	// (b) OUT OF RANGE -> the default pin. FRESH graph: m_iActivePin is
	//     per-instance state.
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSwitch = xDef.AddNode("SwitchOnInt");
		const FlowPin_Param axParams[] =
		{
			{ "m_iCaseBase", FlowPin_Int(0) },
			{ "m_iCaseCount", FlowPin_Int(3) },
		};
		FlowPin_SetParams(xDef, uSwitch, "SwitchOnInt", axParams, 2u);
		xDef.AddEdge(uSource, 0u, uSwitch);
		FlowPin_AddWitness(xDef, uSwitch, 1u, "case1Ran");
		FlowPin_AddWitness(xDef, uSwitch, 3u, "defaultRan");

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		Zenith_GraphNode* pxSwitch = xGraph.FindNode(uSwitch);
		ZENITH_ASSERT_NOT_NULL(pxSwitch);
		if (pxSwitch == nullptr)
		{
			return;
		}
		pxSwitch->SetInputForTest(uValuePin, FlowPin_Int(99));
		FlowPin_FireOneUpdate(xGraph);

		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "defaultRan"), "an out-of-range wire must take the DEFAULT pin");
		FlowPin_AssertAbsent(xGraph, "case1Ran");
		ZENITH_ASSERT_EQ(pxSwitch->GetBadAccessWarningCountForTest(), 0u);
	}
}

// The STRING dispatcher. Cases "a,b,c" and a wire selecting "c". ★ m_strCases
// goes through the blob BEFORE the graph is built: the
// EnsureCasesParsed() latch is not reset by ApplyNodeParams, so a case list
// assigned after FindNode would leave m_axCases EMPTY and route everything to
// the default pin - a green-looking row proving nothing.
ZENITH_TEST(FlowPinRuntime, Wired_SwitchOnStringValueFromWire)
{
	FlowPin_EnsureRegistry();
	const u_int uValuePin = Zenith_GraphNode_SwitchOnString::uPIN_Value;

	// (a) the wire selects case c.
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSwitch = xDef.AddNode("SwitchOnString");
		ZENITH_ASSERT_NE(uSwitch, 0u);
		const FlowPin_Param axParams[] =
		{
			{ "m_strCases", FlowPin_Str("a,b,c") },
		};
		FlowPin_SetParams(xDef, uSwitch, "SwitchOnString", axParams, 1u);
		xDef.AddEdge(uSource, 0u, uSwitch);
		FlowPin_AddWitness(xDef, uSwitch, 0u, "caseARan");
		FlowPin_AddWitness(xDef, uSwitch, 1u, "caseBRan");
		FlowPin_AddWitness(xDef, uSwitch, 2u, "caseCRan");
		FlowPin_AddWitness(xDef, uSwitch, 3u, "defaultRan");

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);
		Zenith_GraphNode* pxSwitch = xGraph.FindNode(uSwitch);
		ZENITH_ASSERT_NOT_NULL(pxSwitch);
		if (pxSwitch == nullptr)
		{
			return;
		}
		pxSwitch->SetInputForTest(uValuePin, FlowPin_Str("c"));
		FlowPin_FireOneUpdate(xGraph);

		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "caseCRan"), "the wired case did not run");
		FlowPin_AssertAbsent(xGraph, "caseARan");
		FlowPin_AssertAbsent(xGraph, "caseBRan");
		FlowPin_AssertAbsent(xGraph, "defaultRan");
		ZENITH_ASSERT_EQ(pxSwitch->GetBadAccessWarningCountForTest(), 0u);
	}

	// (b) an UNKNOWN label -> the default pin (FRESH graph).
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSwitch = xDef.AddNode("SwitchOnString");
		const FlowPin_Param axParams[] =
		{
			{ "m_strCases", FlowPin_Str("a,b,c") },
		};
		FlowPin_SetParams(xDef, uSwitch, "SwitchOnString", axParams, 1u);
		xDef.AddEdge(uSource, 0u, uSwitch);
		FlowPin_AddWitness(xDef, uSwitch, 1u, "caseBRan");
		FlowPin_AddWitness(xDef, uSwitch, 3u, "defaultRan");

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		Zenith_GraphNode* pxSwitch = xGraph.FindNode(uSwitch);
		ZENITH_ASSERT_NOT_NULL(pxSwitch);
		if (pxSwitch == nullptr)
		{
			return;
		}
		pxSwitch->SetInputForTest(uValuePin, FlowPin_Str("zzz"));
		FlowPin_FireOneUpdate(xGraph);

		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "defaultRan"), "an unknown label must take the DEFAULT pin");
		FlowPin_AssertAbsent(xGraph, "caseBRan");
		ZENITH_ASSERT_EQ(pxSwitch->GetBadAccessWarningCountForTest(), 0u);
	}
}

// ★ THIS ROW ASSERTS THE BODY SWITCH, AND NOTHING ELSE. The Exit_/Enter_
// transition events need xContext.m_xSelf to carry a Zenith_GraphComponent,
// which the sanctioned fixture here never has; their coverage stays with
// BehaviourGraph.StateMachineTransitionAbortsOldState and ScriptTest's
// ST_StateGym_Test. What IS asserted: the wire decides which state body runs,
// a CHANGED wire moves the machine on the next fire (read every fire), and the
// clamp holds at both ends.
ZENITH_TEST(FlowPinRuntime, Wired_StateMachineStateFromWire)
{
	FlowPin_EnsureRegistry();
	const u_int uStatePin = Zenith_GraphNode_StateMachine::uPIN_State;

	// (a) wire 2, then the wire moves to 0 on a second fire.
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uMachine = xDef.AddNode("StateMachine");
		ZENITH_ASSERT_NE(uMachine, 0u);
		const FlowPin_Param axParams[] =
		{
			{ "m_iStateCount", FlowPin_Int(3) },
		};
		FlowPin_SetParams(xDef, uMachine, "StateMachine", axParams, 1u);
		xDef.AddEdge(uSource, 0u, uMachine);
		FlowPin_AddWitness(xDef, uMachine, 0u, "state0Ran");
		FlowPin_AddWitness(xDef, uMachine, 1u, "state1Ran");
		FlowPin_AddWitness(xDef, uMachine, 2u, "state2Ran");

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);
		Zenith_GraphNode* pxMachine = xGraph.FindNode(uMachine);
		ZENITH_ASSERT_NOT_NULL(pxMachine);
		if (pxMachine == nullptr)
		{
			return;
		}
		pxMachine->SetInputForTest(uStatePin, FlowPin_Int(2));
		FlowPin_FireOneUpdate(xGraph);

		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "state2Ran"), "the wired state's body did not run");
		FlowPin_AssertAbsent(xGraph, "state0Ran");
		FlowPin_AssertAbsent(xGraph, "state1Ran");

		// THE TRANSITION, observed through the witnesses: the wire moves, and the
		// machine follows on the very next fire because State is read EVERY fire.
		// state2Ran survives from before - the witness records that the pin ran
		// once, not that it is running now.
		pxMachine->SetInputForTest(uStatePin, FlowPin_Int(0));
		FlowPin_FireOneUpdate(xGraph);
		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "state0Ran"), "a changed wire did not move the machine");
		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "state2Ran"));
		FlowPin_AssertAbsent(xGraph, "state1Ran");
		ZENITH_ASSERT_EQ(pxMachine->GetBadAccessWarningCountForTest(), 0u);
	}

	// (b) CLAMP, both ends, FRESH graph each (m_iCurrentState is per-instance).
	{
		const int32_t aiWires[] = { 99, -5 };
		const char* aszExpected[] = { "state2Ran", "state0Ran" };
		const char* aszForbidden[] = { "state0Ran", "state2Ran" };
		for (u_int u = 0; u < 2u; ++u)
		{
			Zenith_GraphDefinition xDef;
			const u_int uSource = xDef.AddNode("OnUpdate");
			const u_int uMachine = xDef.AddNode("StateMachine");
			const FlowPin_Param axParams[] =
			{
				{ "m_iStateCount", FlowPin_Int(3) },
			};
			FlowPin_SetParams(xDef, uMachine, "StateMachine", axParams, 1u);
			xDef.AddEdge(uSource, 0u, uMachine);
			FlowPin_AddWitness(xDef, uMachine, 0u, "state0Ran");
			FlowPin_AddWitness(xDef, uMachine, 1u, "state1Ran");
			FlowPin_AddWitness(xDef, uMachine, 2u, "state2Ran");

			Zenith_BehaviourGraph xGraph;
			ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
			Zenith_GraphNode* pxMachine = xGraph.FindNode(uMachine);
			ZENITH_ASSERT_NOT_NULL(pxMachine);
			if (pxMachine == nullptr)
			{
				return;
			}
			pxMachine->SetInputForTest(uStatePin, FlowPin_Int(aiWires[u]));
			FlowPin_FireOneUpdate(xGraph);

			ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, aszExpected[u]),
				"a wire of %d must clamp to '%s'", aiWires[u], aszExpected[u]);
			FlowPin_AssertAbsent(xGraph, aszForbidden[u]);
			FlowPin_AssertAbsent(xGraph, "state1Ran");
			ZENITH_ASSERT_EQ(pxMachine->GetBadAccessWarningCountForTest(), 0u);
		}
	}
}

// This real-data-edge row proves reactive StateMachine pulls. SetInputForTest
// bypasses the connected path entirely; the separate suspended-switch row also
// uses a real data edge to verify that its key is not pulled again on resume.
//
// A GetVariable producer feeds State through AddDataEdge. The source variable is
// changed between fires and the machine FOLLOWS it - the doc claim "a wired
// State is pulled every fire, reactive by design" is exactly what this witnesses.
// The State property is clear, so the data edge is the only source.
ZENITH_TEST(FlowPinRuntime, Wired_StateMachineStateFollowsAWiredProducer)
{
	FlowPin_EnsureRegistry();
	const u_int uStatePin = Zenith_GraphNode_StateMachine::uPIN_State;

	Zenith_GraphDefinition xDef;
	// DECLARED INT32: GetVariable's output slot is typed off the declaration, and
	// an ANY slot would make the tag agreement at the wire unprovable.
	xDef.DeclareVariable("smWired", FlowPin_Int(0));
	const u_int uSource = xDef.AddNode("OnUpdate");
	const u_int uMachine = xDef.AddNode("StateMachine");
	const u_int uGet = xDef.AddNode("GetVariable");
	ZENITH_ASSERT_NE(uMachine, 0u);
	ZENITH_ASSERT_NE(uGet, 0u);
	if (uMachine == 0u || uGet == 0u)
	{
		return;
	}
	{
		const FlowPin_Param axParams[] =
		{
			{ "m_iStateCount", FlowPin_Int(3) },
		};
		FlowPin_SetParams(xDef, uMachine, "StateMachine", axParams, 1u);
	}
	{
		const FlowPin_Param axParams[] = { { "m_strVariable", FlowPin_Str("smWired") } };
		FlowPin_SetParams(xDef, uGet, "GetVariable", axParams, 1u);
	}
	xDef.AddEdge(uSource, 0u, uMachine);
	ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uGet, "Value", uMachine, "State"),
		"the data edge into StateMachine.State was refused");
	FlowPin_AddWitness(xDef, uMachine, 0u, "state0Ran");
	FlowPin_AddWitness(xDef, uMachine, 1u, "state1Ran");
	FlowPin_AddWitness(xDef, uMachine, 2u, "state2Ran");

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u, "the data edge must resolve at instantiation");
	ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);
	xGraph.GetBlackboard().SetValue("smWired", FlowPin_Int(2));

	Zenith_GraphNode* pxMachine = xGraph.FindNode(uMachine);
	ZENITH_ASSERT_NOT_NULL(pxMachine);
	if (pxMachine == nullptr)
	{
		return;
	}
	FlowPin_FireOneUpdate(xGraph);
	ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "state2Ran"), "the producer's value did not reach State");
	FlowPin_AssertAbsent(xGraph, "state0Ran");
	FlowPin_AssertAbsent(xGraph, "state1Ran");

	// The producer's source moves; the machine follows on the next fire.
	xGraph.GetBlackboard().SetValue("smWired", FlowPin_Int(0));
	FlowPin_FireOneUpdate(xGraph);
	ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "state0Ran"),
		"State was not re-pulled: a wired StateMachine must read its producer EVERY fire");
	FlowPin_AssertAbsent(xGraph, "state1Ran");

	ZENITH_ASSERT_EQ(pxMachine->GetMismatchWarningCountForTest(uStatePin), 0u);
	ZENITH_ASSERT_EQ(pxMachine->GetBadAccessWarningCountForTest(), 0u);
}

// ★ A SUSPENDED CASE DOES NOT RE-READ ITS KEY. The observable is the route and
// the producer count.
//
// Case 1's chain suspends on a Wait (0.02 s against a 0.016 s dt: RUNNING on
// fire 1, SUCCESS on fire 2). The key is flipped to case 2 in between. If the
// switch re-read, case 2's witness would fire and case 1's tail never would.
ZENITH_TEST(FlowPinRuntime, Wired_SwitchOnIntSuspendedCaseDoesNotReread)
{
	FlowPin_EnsureRegistry();

	// (a) a wired pure producer begins at case 1, then changes to case 2 while
	//     the Wait is suspended. Its counter proves the switch does not re-pull.
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSwitch = xDef.AddNode("SwitchOnInt");
		const u_int uProducer = xDef.AddNode("Test_FlowPinProducer");
		ZENITH_ASSERT_NE(uProducer, 0u);
		if (uProducer == 0u)
		{
			return;
		}
		const FlowPin_Param axSwitch[] =
		{
			{ "m_iCaseBase", FlowPin_Int(0) },
			{ "m_iCaseCount", FlowPin_Int(3) },
		};
		FlowPin_SetParams(xDef, uSwitch, "SwitchOnInt", axSwitch, 2u);
		const FlowPin_Param axProducer[] = { { "m_iValue", FlowPin_Int(1) } };
		FlowPin_SetParams(xDef, uProducer, "Test_FlowPinProducer", axProducer, 1u);
		xDef.AddEdge(uSource, 0u, uSwitch);
		ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uSwitch, "Value"));

		const u_int uWait = xDef.AddNode("Wait");
		ZENITH_ASSERT_NE(uWait, 0u);
		const FlowPin_Param axWait[] = { { "m_fSeconds", FlowPin_Float(0.02f) } };
		FlowPin_SetParams(xDef, uWait, "Wait", axWait, 1u);
		xDef.AddEdge(uSwitch, 1u, uWait);
		FlowPin_AddWitness(xDef, uWait, 0u, "case1Tail");
		FlowPin_AddWitness(xDef, uSwitch, 2u, "case2Ran");
		FlowPin_AddWitness(xDef, uSwitch, 3u, "defaultRan");

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
		ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);
		Test_FlowPinProducerNode* pxProducer =
			static_cast<Test_FlowPinProducerNode*>(xGraph.FindNode(uProducer));
		ZENITH_ASSERT_NOT_NULL(pxProducer);
		if (pxProducer == nullptr)
		{
			return;
		}

		FlowPin_FireOneUpdate(xGraph);							// case 1 taken, Wait RUNNING
		ZENITH_ASSERT_EQ(pxProducer->m_uExecuteCount, 1u);
		FlowPin_AssertAbsent(xGraph, "case1Tail");
		FlowPin_AssertAbsent(xGraph, "case2Ran");

		pxProducer->m_iValue = 2;
		FlowPin_FireOneUpdate(xGraph);							// the Wait completes

		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "case1Tail"),
			"the suspended case did not resume - the switch re-read its key mid-suspension");
		FlowPin_AssertAbsent(xGraph, "case2Ran");
		FlowPin_AssertAbsent(xGraph, "defaultRan");
		ZENITH_ASSERT_EQ(pxProducer->m_uExecuteCount, 1u,
			"the suspended switch pulled its producer again");

		Zenith_GraphNode* pxSwitch = xGraph.FindNode(uSwitch);
		ZENITH_ASSERT_NOT_NULL(pxSwitch);
		if (pxSwitch != nullptr)
		{
			ZENITH_ASSERT_EQ(pxSwitch->GetBadAccessWarningCountForTest(), 0u);
		}
	}

	// (b) the WIRED leg, and the falsifiable half: the producer's execute counter
	//     does NOT advance across the suspended fire. A pure producer is pulled
	//     only when its consumer asks, so "the key was not re-read" is a COUNT
	//     here rather than an inference from the route.
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSwitch = xDef.AddNode("SwitchOnInt");
		const u_int uProducer = xDef.AddNode("Test_FlowPinProducer");
		ZENITH_ASSERT_NE(uProducer, 0u);
		if (uProducer == 0u)
		{
			return;
		}
		const FlowPin_Param axSwitch[] =
		{
			{ "m_iCaseBase", FlowPin_Int(0) },
			{ "m_iCaseCount", FlowPin_Int(3) },
		};
		FlowPin_SetParams(xDef, uSwitch, "SwitchOnInt", axSwitch, 2u);
		const FlowPin_Param axProducer[] = { { "m_iValue", FlowPin_Int(1) } };
		FlowPin_SetParams(xDef, uProducer, "Test_FlowPinProducer", axProducer, 1u);
		xDef.AddEdge(uSource, 0u, uSwitch);
		ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uSwitch, "Value"));

		const u_int uWait = xDef.AddNode("Wait");
		const FlowPin_Param axWait[] = { { "m_fSeconds", FlowPin_Float(0.02f) } };
		FlowPin_SetParams(xDef, uWait, "Wait", axWait, 1u);
		xDef.AddEdge(uSwitch, 1u, uWait);
		FlowPin_AddWitness(xDef, uWait, 0u, "wiredCase1Tail");
		FlowPin_AddWitness(xDef, uSwitch, 2u, "wiredCase2Ran");

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u, "the data edge must resolve at instantiation");
		ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);

		Test_FlowPinProducerNode* pxProducer =
			static_cast<Test_FlowPinProducerNode*>(xGraph.FindNode(uProducer));
		Zenith_GraphNode* pxSwitch = xGraph.FindNode(uSwitch);
		ZENITH_ASSERT_NOT_NULL(pxProducer);
		ZENITH_ASSERT_NOT_NULL(pxSwitch);
		if (pxProducer == nullptr || pxSwitch == nullptr)
		{
			return;
		}

		FlowPin_FireOneUpdate(xGraph);
		ZENITH_ASSERT_EQ(pxProducer->m_uExecuteCount, 1u, "the switch never pulled its producer");
		FlowPin_AssertAbsent(xGraph, "wiredCase1Tail");

		// The producer would now answer 2. The switch must not ask.
		pxProducer->m_iValue = 2;
		FlowPin_FireOneUpdate(xGraph);
		ZENITH_ASSERT_EQ(pxProducer->m_uExecuteCount, 1u,
			"the suspended switch PULLED its producer again - the read left its `m_iActivePin < 0` branch");
		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "wiredCase1Tail"));
		FlowPin_AssertAbsent(xGraph, "wiredCase2Ran");
		ZENITH_ASSERT_EQ(pxSwitch->GetBadAccessWarningCountForTest(), 0u);
	}
}

// The OTHER half of the timing contract, and the reason the two dispatchers are
// not interchangeable: StateMachine reads at the TOP of every Execute. A pure
// producer's counter proves both fires pull State; the state witnesses prove the
// changed value reaches the machine.
ZENITH_TEST(FlowPinRuntime, Wired_StateMachineReadsEveryFire)
{
	FlowPin_EnsureRegistry();

	Zenith_GraphDefinition xDef;
	const u_int uSource = xDef.AddNode("OnUpdate");
	const u_int uMachine = xDef.AddNode("StateMachine");
	const u_int uProducer = xDef.AddNode("Test_FlowPinProducer");
	ZENITH_ASSERT_NE(uProducer, 0u);
	if (uProducer == 0u)
	{
		return;
	}
	const FlowPin_Param axParams[] =
	{
		{ "m_iStateCount", FlowPin_Int(3) },
	};
	FlowPin_SetParams(xDef, uMachine, "StateMachine", axParams, 1u);
	const FlowPin_Param axProducer[] = { { "m_iValue", FlowPin_Int(1) } };
	FlowPin_SetParams(xDef, uProducer, "Test_FlowPinProducer", axProducer, 1u);
	xDef.AddEdge(uSource, 0u, uMachine);
	ZENITH_ASSERT_TRUE(xDef.AddDataEdge(uProducer, "Value", uMachine, "State"));
	FlowPin_AddWitness(xDef, uMachine, 0u, "everyFire0Ran");
	FlowPin_AddWitness(xDef, uMachine, 1u, "everyFire1Ran");
	FlowPin_AddWitness(xDef, uMachine, 2u, "everyFire2Ran");

	Zenith_BehaviourGraph xGraph;
	ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
	ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
	ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);
	Test_FlowPinProducerNode* pxProducer =
		static_cast<Test_FlowPinProducerNode*>(xGraph.FindNode(uProducer));
	ZENITH_ASSERT_NOT_NULL(pxProducer);
	if (pxProducer == nullptr)
	{
		return;
	}

	FlowPin_FireOneUpdate(xGraph);
	ZENITH_ASSERT_EQ(pxProducer->m_uExecuteCount, 1u, "the first StateMachine fire did not pull its producer");
	ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "everyFire1Ran"));
	FlowPin_AssertAbsent(xGraph, "everyFire2Ran");

	pxProducer->m_iValue = 2;
	FlowPin_FireOneUpdate(xGraph);
	ZENITH_ASSERT_EQ(pxProducer->m_uExecuteCount, 2u,
		"StateMachine did not pull State on its second fire");
	ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "everyFire2Ran"),
		"the machine did not follow its producer - State must be read on EVERY fire");

	Zenith_GraphNode* pxMachine = xGraph.FindNode(uMachine);
	ZENITH_ASSERT_NOT_NULL(pxMachine);
	if (pxMachine != nullptr)
	{
		ZENITH_ASSERT_EQ(pxMachine->GetBadAccessWarningCountForTest(), 0u);
	}
}

// ★ THE TYPE ZERO, NOT A CONST. Neither dispatcher declares a const half, so an
// unbound pin takes the type's zero. Both legs
// choose a configuration where the zero is DISTINGUISHABLE from "the node did
// something sensible".
ZENITH_TEST(FlowPinRuntime, Unbound_DispatchInputTakesTheTypeZero)
{
	FlowPin_EnsureRegistry();

	// (a) SwitchOnInt with m_iCaseBase = 10: an unbound key gives 0 - 10 = -10,
	//     which is OUT OF RANGE and takes the DEFAULT pin. Case 0 would be the
	//     answer if the base were ignored, so the two are not confusable - the
	//     BehaviourGraph.SwitchOnIntRoutesCasesAndDefault shape.
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSwitch = xDef.AddNode("SwitchOnInt");
		const FlowPin_Param axParams[] =
		{
			{ "m_iCaseBase", FlowPin_Int(10) },
			{ "m_iCaseCount", FlowPin_Int(3) },
		};
		FlowPin_SetParams(xDef, uSwitch, "SwitchOnInt", axParams, 2u);
		xDef.AddEdge(uSource, 0u, uSwitch);
		FlowPin_AddWitness(xDef, uSwitch, 0u, "absentCase0Ran");
		FlowPin_AddWitness(xDef, uSwitch, 3u, "absentDefaultRan");

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		FlowPin_FireOneUpdate(xGraph);

		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "absentDefaultRan"),
			"an unbound key must read the type zero (0 - base = -10 -> the default pin)");
		FlowPin_AssertAbsent(xGraph, "absentCase0Ran");

		Zenith_GraphNode* pxSwitch = xGraph.FindNode(uSwitch);
		ZENITH_ASSERT_NOT_NULL(pxSwitch);
		if (pxSwitch != nullptr)
		{
			ZENITH_ASSERT_EQ(pxSwitch->GetBadAccessWarningCountForTest(), 0u);
		}
	}

	// (b) StateMachine: state 0 IS the type zero, so this preserves the explicit
	//     zero route with an unbound input.
	{
		Zenith_GraphDefinition xDef;
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uMachine = xDef.AddNode("StateMachine");
		const FlowPin_Param axParams[] =
		{
			{ "m_iStateCount", FlowPin_Int(3) },
		};
		FlowPin_SetParams(xDef, uMachine, "StateMachine", axParams, 1u);
		xDef.AddEdge(uSource, 0u, uMachine);
		FlowPin_AddWitness(xDef, uMachine, 0u, "absentState0Ran");
		FlowPin_AddWitness(xDef, uMachine, 2u, "absentState2Ran");

		Zenith_BehaviourGraph xGraph;
		ZENITH_ASSERT_TRUE(xGraph.InitialiseFromDefinition(xDef));
		FlowPin_FireOneUpdate(xGraph);

		ZENITH_ASSERT_TRUE(FlowPin_Ran(xGraph, "absentState0Ran"));
		FlowPin_AssertAbsent(xGraph, "absentState2Ran");

		Zenith_GraphNode* pxMachine = xGraph.FindNode(uMachine);
		ZENITH_ASSERT_NOT_NULL(pxMachine);
		if (pxMachine != nullptr)
		{
			ZENITH_ASSERT_EQ(pxMachine->GetBadAccessWarningCountForTest(), 0u);
		}
	}
}

#endif // ZENITH_TESTING
