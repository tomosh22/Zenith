#include "Zenith.h"

// ============================================================================
// ZM_Tests_GraphPinTotality -- the five graph-VALIDATION units for Zenithmon.
//
//   ZenithmonNodesTotality              -- every blackboard-variable-name
//                                          property in ZM's node library is
//                                          covered by a pin descriptor.
//   ZenithmonTrainerChallengeValidatesClean -- the one graph ZM authors reports no
//                                          error-severity finding.
//
// WHY THESE ARE ZENITH_TESTs. Zenithmon's gate runs its boot units
// (run_unit_gate.ps1 -Game Zenithmon), which is the home where a unit actually
// RUNS -- the same reasoning ZM_Tests_TrainerChallengeGraph.cpp:141 acts on.
// The five total tests add three Zenithmon baseline rows, and ONLY Zenithmon's: a
// game-exe-only unit cannot move Combat's or RenderTest's row.
//
// WHAT THE TOTALITY WALK PROVES, why the registry is SWAPPED to ZM's registrar
// rather than filtered by category, and why the restore is RAII and replays the
// snapshot IN ORDER (ZM's rows sit at 0..N-1, ahead of every engine row) all
// live once in Zenith/EntityComponent/Zenith_GraphPinTotality.TestHarness.inl.
// Only ZM's registrar is named here.
//
// ★ THE FAILURE THIS CATCHES IS A SILENT ONE. A node class with a m_str*Var*
// property and no pin descriptor is OPAQUE to Zenith_GraphDefinitionValidator:
// it contributes no writer and performs no read that any check can see, so a
// node can drop out of validation entirely while every behavioural unit in
// ZM_Tests_TrainerChallengeGraph.cpp stays green -- they drive the beat, not
// the descriptor table.
// ============================================================================

#ifdef ZENITH_TESTING

#include "Core/Zenith_TestFramework.h"
#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Scripting/Zenith_GraphDefinitionValidator.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Zenithmon/Components/ZM_GraphNodes.h"               // ZM_RegisterGraphNodes
#include "Zenithmon/Source/Graph/ZM_GraphAuthoring.h"         // BuildGraph_ZM_TrainerChallenge

namespace
{
	class ZM_TestUnsetAnyProducer : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(ZM_TestUnsetAnyProducer)
	public:
		ZENITH_GRAPH_PINS_BEGIN(ZM_TestUnsetAnyProducer)
		ZENITH_GRAPH_PIN_OUTPUT(Value, eGRAPH_PIN_TYPE_ANY)
		ZENITH_GRAPH_PINS_END

	public:
		GraphNodeStatus Execute(Zenith_GraphContext&) override { return GRAPH_NODE_STATUS_SUCCESS; }
		const char* GetTypeName() const override { return "ZM_TestUnsetAnyProducer"; }
	};

	void EnsureUnsetAnyProducerRegistered()
	{
		static bool bRegistered = false;
		if (!bRegistered)
		{
			Zenith_GraphNodeRegistry::Get().RegisterNodeType<ZM_TestUnsetAnyProducer>(
				"ZM_TestUnsetAnyProducer", GRAPH_EVENT_NONE, 0u, false, "ZenithmonTest", false, true);
			bRegistered = true;
		}
	}
}

ZENITH_TEST(GraphPinTable, ZenithmonNodesTotality)
{
	// No exemptions: ZM has one slot-only INPUT descriptor and no permanent
	// blackboard-name property (the exempt list is for comma-separated LIST data).
	Zenith_CheckPinTableTotality(&ZM_RegisterGraphNodes, "ZM_GraphNodes.h", nullptr, 0u);
	Zenith_GraphPinTotalityRegistryGuard xGuard;
	Zenith_GraphPinTotality_SwapToRegistrar(&ZM_RegisterGraphNodes);
	ZENITH_ASSERT_EQ(Zenith_GraphNodeRegistry::Get().GetTypeCount(), 1u);
}

ZENITH_TEST(GraphPinTable, ZenithmonPinIndicesMatchTables)
{
	const Zenith_GraphPinTable& xPins = ZM_GraphNode_PushTrainerChallenge::GetPinTableStatic();
	ZENITH_ASSERT_EQ(xPins.GetPinCount(), 1u);
	ZENITH_ASSERT_EQ(xPins.FindPinIndex("TrainerId"), ZM_GraphNode_PushTrainerChallenge::uPIN_TrainerId);
	const Zenith_GraphPinDesc& xTrainerId = xPins.GetPinAt(ZM_GraphNode_PushTrainerChallenge::uPIN_TrainerId);
	ZENITH_ASSERT_EQ(static_cast<int>(xTrainerId.m_eRole), static_cast<int>(GRAPH_PIN_ROLE_INPUT));
	ZENITH_ASSERT_EQ(static_cast<int>(xTrainerId.m_eType), static_cast<int>(PROPERTY_TYPE_INT32));
	ZENITH_ASSERT_STREQ(xTrainerId.m_szConstProperty, "m_iTrainerId");
	ZENITH_ASSERT_STREQ(xTrainerId.m_szVarNameProperty, "");
}

ZENITH_TEST(GraphPinTable, ZenithmonTrainerIdOverrideWins)
{
	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();
	// A null blackboard takes the legacy early guard before any accessor runs.
	ZM_GraphNode_PushTrainerChallenge xNull;
	Zenith_GraphContext xNullContext;
	ZENITH_ASSERT_EQ(static_cast<int>(xNull.Execute(xNullContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(xNull.GetBadAccessWarningCountForTest(), 0u);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushAttempts, 1u);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_eLastChallengeTrainer, ZM_TRAINER_NONE);
	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();

	// The permanent NONE default is independent of blackboard lookup. Keep
	// contrary absent/wrong-tag blackboards in the fixture, but clear the legacy
	// name so this remains a C-1-safe default-value proof.
	Zenith_GraphBlackboard xAbsent;
	Zenith_GraphContext xAbsentContext; xAbsentContext.m_pxBlackboard = &xAbsent;
	ZM_GraphNode_PushTrainerChallenge xAbsentNode;
	ZENITH_ASSERT_EQ(static_cast<int>(xAbsentNode.Execute(xAbsentContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(xAbsentNode.GetBadAccessWarningCountForTest(), 0u);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushAttempts, 1u);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_eLastChallengeTrainer, ZM_TRAINER_NONE);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushSucceeded, 0u);
	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();
	Zenith_PropertyValue xWrongTag; xWrongTag.SetFloat(3.0f);
	xAbsent.SetValue("zmTrainerId", xWrongTag);
	ZM_GraphNode_PushTrainerChallenge xWrongNamed;
	ZENITH_ASSERT_EQ(static_cast<int>(xWrongNamed.Execute(xAbsentContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
	ZENITH_ASSERT_EQ(xWrongNamed.GetMismatchWarningCountForTest(ZM_GraphNode_PushTrainerChallenge::uPIN_TrainerId), 0u);
	ZENITH_ASSERT_EQ(xWrongNamed.GetBadAccessWarningCountForTest(), 0u);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushAttempts, 1u);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_eLastChallengeTrainer, ZM_TRAINER_NONE);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushSucceeded, 0u);
	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();

	Zenith_GraphBlackboard xBlackboard;
	Zenith_PropertyValue xContrary;
	xContrary.SetInt32(static_cast<int32_t>(ZM_TRAINER_NONE));
	xBlackboard.SetValue("contrary", xContrary);
	Zenith_GraphContext xContext;
	xContext.m_pxBlackboard = &xBlackboard;
	ZM_GraphNode_PushTrainerChallenge xNode;
	Zenith_PropertyValue xVesper;
	xVesper.SetInt32(static_cast<int32_t>(ZM_TRAINER_RIVAL_VESPER));
	xNode.SetInputForTest(ZM_GraphNode_PushTrainerChallenge::uPIN_TrainerId, xVesper);
	xNode.Execute(xContext);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_eLastChallengeTrainer,
		ZM_TRAINER_RIVAL_VESPER,
		"a checked TrainerId override wins over the contrary blackboard value");
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushSucceeded, 0u,
		"the boot-unit override has no menu root and must not push dialogue");
	ZENITH_ASSERT_EQ(xNode.GetMismatchWarningCountForTest(ZM_GraphNode_PushTrainerChallenge::uPIN_TrainerId), 0u);
	ZENITH_ASSERT_EQ(xNode.GetBadAccessWarningCountForTest(), 0u);

	Zenith_PropertyValue xWrongOverride; xWrongOverride.SetFloat(2.0f);
	ZM_GraphNode_PushTrainerChallenge xWrongOverrideNode;
	xWrongOverrideNode.SetInputForTest(ZM_GraphNode_PushTrainerChallenge::uPIN_TrainerId, xWrongOverride);
	xWrongOverrideNode.Execute(xContext);
	ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_eLastChallengeTrainer, ZM_TRAINER_NONE);
	ZENITH_ASSERT_EQ(xWrongOverrideNode.GetMismatchWarningCountForTest(ZM_GraphNode_PushTrainerChallenge::uPIN_TrainerId), 1u);
	ZENITH_ASSERT_EQ(xWrongOverrideNode.GetBadAccessWarningCountForTest(), 0u);
	ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();
}

ZENITH_TEST(GraphPinTable, ZenithmonTrainerIdUnsetAnyWireUsesNone)
{
	// Register before this graph is initialized; never reset the shared registry,
	// because live graph instances retain registry-owned type information.
	EnsureUnsetAnyProducerRegistered();
	Zenith_GraphDefinition xDefinition;
	const u_int uProducer = xDefinition.AddNode("ZM_TestUnsetAnyProducer");
	const u_int uConsumer = xDefinition.AddNode(szZM_GRAPH_NODE_PUSH_TRAINER_CHALLENGE);
	ZENITH_ASSERT_TRUE(uProducer != 0u && uConsumer != 0u);
	ZENITH_ASSERT_TRUE(xDefinition.AddDataEdge(uProducer, "Value", uConsumer, "TrainerId"));
	Zenith_BehaviourGraph xGraph;
	const bool bInitialised = xGraph.InitialiseFromDefinition(xDefinition);
	ZENITH_ASSERT_TRUE(bInitialised);
	if (!bInitialised) { xGraph.Shutdown(); return; }
	ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u);
	Zenith_GraphNode* pxSource = xGraph.FindNode(uProducer);
	Zenith_GraphNode* pxConsumer = xGraph.FindNode(uConsumer);
	ZENITH_ASSERT_NOT_NULL(pxSource);
	ZENITH_ASSERT_NOT_NULL(pxConsumer);
	if (pxSource != nullptr && pxConsumer != nullptr)
	{
		ZENITH_ASSERT_NULL(pxSource->GetOutputForTest(0u));
		Zenith_PropertyValue xContrary;
		xContrary.SetInt32(static_cast<int32_t>(ZM_TRAINER_RIVAL_VESPER));
		xGraph.GetBlackboard().SetValue("contrary", xContrary);
		Zenith_GraphContext xContext;
		xContext.m_pxGraph = &xGraph;
		xContext.m_pxBlackboard = &xGraph.GetBlackboard();
		ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();
		ZENITH_ASSERT_EQ(static_cast<int>(pxConsumer->Execute(xContext)), static_cast<int>(GRAPH_NODE_STATUS_FAILURE));
		ZENITH_ASSERT_NULL(pxSource->GetOutputForTest(0u));
		ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushAttempts, 1u);
		ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_eLastChallengeTrainer, ZM_TRAINER_NONE);
		ZENITH_ASSERT_EQ(ZM_GraphNodeTestCounters::s_uChallengePushSucceeded, 0u);
		ZENITH_ASSERT_EQ(pxConsumer->GetMismatchWarningCountForTest(ZM_GraphNode_PushTrainerChallenge::uPIN_TrainerId), 0u);
		ZENITH_ASSERT_EQ(pxConsumer->GetBadAccessWarningCountForTest(), 0u);
		ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();
	}
	xGraph.Shutdown();
}

// ---- The graph half: zero error-severity findings, so Build() stays true ------------------
//
// Built IN-PROCESS from BuildGraph_ZM_TrainerChallenge, never from disk:
// `.bgraph` files are gitignored and written only by a TOOLS boot, so a
// disk-reading unit would skip on a fresh checkout and a skip counts as a pass.

ZENITH_TEST(GraphPinTable, ZenithmonTrainerChallengeValidatesClean)
{
	Zenith_GraphDefinition xDefinition;
	Zenith_GraphBuilder xBuilder(xDefinition);
	BuildGraph_ZM_TrainerChallenge(xBuilder);

	// Build() runs the FULL-tier validation pass after the commit loop and keeps
	// the report on the builder, so the builder must stay alive below.
	const bool bBuilt = xBuilder.Build();
	ZENITH_ASSERT_TRUE(bBuilt,
		"BuildGraph_ZM_TrainerChallenge's Build() failed -- the tools boot would "
		"write a broken .bgraph");

	u_int uErrors = 0u;
	for (u_int uFinding = 0; uFinding < xBuilder.GetValidationFindingCount(); ++uFinding)
	{
		const Zenith_GraphValidationFinding& xFinding = xBuilder.GetValidationFindingAt(uFinding);
		if (xFinding.m_eSeverity != GRAPH_VALIDATION_SEVERITY_ERROR)
		{
			continue;	// LIST_NAME / DECLARED_UNUSED are warnings and stay
		}
		++uErrors;
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZMGraphs]   %s node=%u:%s pin=%s var=%s rule=%s | %s",
			szZM_GRAPH_TRAINER_CHALLENGE_ASSET, xFinding.m_uNodeID,
			xFinding.m_strTypeName.c_str(),
			xFinding.m_strPin.empty() ? "-" : xFinding.m_strPin.c_str(),
			xFinding.m_strVar.empty() ? "-" : xFinding.m_strVar.c_str(),
			Zenith_GraphDefinitionValidator::GetRuleName(xFinding.m_eRule),
			xFinding.m_strWhat.c_str());
	}

	ZENITH_ASSERT_EQ(uErrors, 0u,
		"%s reports an error-severity finding -- Build() latches on it and "
		"this game's tools boot reds. The lines above name the rule, the "
		"variable and the node.",
		szZM_GRAPH_TRAINER_CHALLENGE_ASSET);
	ZENITH_ASSERT_EQ(xDefinition.GetDataEdgeCount(), 1u,
		"the authored trainer challenge graph has exactly one GetVariable data edge");
	ZENITH_ASSERT_EQ(xDefinition.GetVariableCount(), 1u,
		"the authored trainer challenge graph declares its payload seed exactly once");
	if (xDefinition.GetVariableCount() == 1u)
	{
		const Zenith_GraphVariableDecl& xSeed = xDefinition.GetVariableAt(0u);
		ZENITH_ASSERT_TRUE(xSeed.m_strName == szZM_GRAPH_VAR_TRAINER_ID);
		ZENITH_ASSERT_TRUE(xSeed.m_xDefault.GetType() == PROPERTY_TYPE_INT32);
		if (xSeed.m_xDefault.GetType() == PROPERTY_TYPE_INT32)
		{
			ZENITH_ASSERT_EQ(xSeed.m_xDefault.GetInt32(), static_cast<int32_t>(ZM_TRAINER_NONE));
		}
	}
	const Zenith_GraphNodeTypeInfo* pxGetVariableInfo = Zenith_GraphNodeRegistry::Get().Find("GetVariable");
	for (u_int uNode = 0u; uNode < xDefinition.GetNodeCount(); ++uNode)
	{
		const Zenith_GraphNodeDef& xNodeDef = xDefinition.GetNodeAt(uNode);
		if (xNodeDef.m_strTypeName != "GetVariable") continue;
		const Zenith_GraphPinTable* pxPins = pxGetVariableInfo && pxGetVariableInfo->m_pfnGetPinTable
			? pxGetVariableInfo->m_pfnGetPinTable() : nullptr;
		Zenith_PropertyType eType = eGRAPH_PIN_TYPE_ANY;
		const bool bResolved = pxPins != nullptr && Zenith_GraphDefinitionValidator::ResolvePinType(
			xDefinition, Zenith_GraphNodeRegistry::Get(), xNodeDef.m_uNodeID, pxPins->FindPinIndex("Value"), eType);
		ZENITH_ASSERT_TRUE(bResolved && eType == PROPERTY_TYPE_INT32);
	}
	Zenith_BehaviourGraph xGraph;
	const bool bInitialised = xGraph.InitialiseFromDefinition(xDefinition);
	ZENITH_ASSERT_TRUE(bInitialised,
		"the authored trainer challenge graph must initialize before resolution is inspected");
	if (bInitialised)
	{
		ZENITH_ASSERT_EQ(xGraph.GetResolutionSkipCountForTest(), 0u,
			"the authored trainer challenge graph resolves its GetVariable wire");
	}
	xGraph.Shutdown();
}

#endif // ZENITH_TESTING
