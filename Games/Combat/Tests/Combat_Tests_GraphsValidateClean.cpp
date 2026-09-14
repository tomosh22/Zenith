#include "Zenith.h"

#if defined(ZENITH_INPUT_SIMULATOR) && defined(ZENITH_TOOLS)

// ============================================================================
// Combat_GraphsValidateClean -- the mechanical precondition for latching the
// graph validator (A-8).
//
// Every one of Combat's five boot-authored graphs is built HERE, in process,
// from the SAME BuildGraph_Combat* function the tools boot writes the .bgraph
// from, and asserted to produce ZERO ERROR-severity findings. A-8 latched that
// property into Build()'s return, so a regression now shows up twice: Build()
// comes back false AND this unit names the rule, the variable and the node -
// instead of a tools boot going red weeks later with no owner.
//
// ★ WHY IT BUILDS FROM THE BUILDER AND NEVER FROM DISK. `.bgraph` files are
// gitignored and are authored only by a TOOLS boot, so a disk-reading unit
// would find nothing on a fresh checkout and would have to skip -- and a skip
// counts as a pass. Building in-process means there is nothing to be stale and
// nothing to skip on.
//
// ★ WHY IT IS #ifdef ZENITH_TOOLS. The five builders live inside Combat.cpp's
// tools block (Combat.cpp:524) with the rest of the authoring, so a `_False`
// config has no definitions to call. The declarations in Combat_Graphs.h carry
// the same gate; this test simply does not exist there.
//
// ★ A SIXTH BUILDER ADDED WITHOUT A ROW HERE GOES UNCHECKED, exactly as the
// ScriptTest builder table says of its own (ScriptTest_Contracts.cpp:243-246).
// The builders are free functions: nothing enumerates them at compile time, so
// there is no count to compare against and no way to make the omission
// mechanical. The row goes in with the builder, or the graph has no coverage.
// The floor below catches a row DELETED, which is the half that CAN be checked.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Scripting/Zenith_GraphDefinitionValidator.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "UnitTests/Zenith_TempScene.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputActions.h"
#include "Input/Zenith_Pointers.h"
#include "Combat/Combat_Graphs.h"
#include "Combat/Combat_Bindings.h"
#include "Combat/Components/Combat_GraphNodes.h"

#include <cstdio>
#include <cstring>

namespace
{
	//-------------------------------------------------------------------------
	// Named-check accumulator (the ScriptTest_Contracts.cpp:77-135 shape): every
	// clause runs and reports, so ONE run names EVERY graph that moved.
	//-------------------------------------------------------------------------
	int g_iChecks = 0;
	int g_iFailures = 0;

	void ResetChecks()
	{
		g_iChecks = 0;
		g_iFailures = 0;
	}

	void CheckTrue(bool bCondition, const char* szWhat)
	{
		++g_iChecks;
		if (!bCondition)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[CombatGraphs] FAILED: %s", szWhat);
		}
	}

	void CheckEqInt(int iActual, int iExpected, const char* szWhat)
	{
		++g_iChecks;
		if (iActual != iExpected)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[CombatGraphs] FAILED: %s (expected %d, got %d)",
				szWhat, iExpected, iActual);
		}
	}

	bool ReportChecks(const char* szTest)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[CombatGraphs] %s: %d checks, %d failed",
			szTest, g_iChecks, g_iFailures);
		// A test that asserted nothing is a test that cannot fail.
		return g_iFailures == 0 && g_iChecks > 0;
	}

	struct GraphBuilderRow
	{
		const char* m_szAssetPath;
		void (*m_pfnBuild)(Zenith_GraphBuilder&);
		u_int m_uDataEdges;
		u_int m_uGetVariables;
	};

	// Keyed by ASSET PATH, spelled exactly as Project_RegisterEditorAutomationSteps
	// spells it in its AddStep_GraphBuild calls (Combat.cpp:790-803).
	const GraphBuilderRow g_axGraphBuilders[] =
	{
		{ "game:Graphs/Combat_PlayerAttack.bgraph", &BuildGraph_CombatPlayerAttack, 11u, 3u },
		{ "game:Graphs/Combat_RoundFlow.bgraph",    &BuildGraph_CombatRoundFlow,    5u, 1u },
		{ "game:Graphs/Combat_PlayerState.bgraph",  &BuildGraph_CombatPlayerState, 10u, 9u },
		{ "game:Graphs/Combat_EnemyBrain.bgraph",   &BuildGraph_CombatEnemyBrain,   6u, 5u },
		{ "game:Graphs/Combat_GameFlow.bgraph",     &BuildGraph_CombatGameFlow,     3u, 0u },
	};

	constexpr u_int uGRAPH_BUILDER_ROWS =
		static_cast<u_int>(sizeof(g_axGraphBuilders) / sizeof(g_axGraphBuilders[0]));

	bool g_bRan = false;

	bool ExpectedGetVariableType(const char* szVariable, Zenith_PropertyType& eOut)
	{
		if (std::strcmp(szVariable, "hitFrameReady") == 0 || std::strcmp(szVariable, "isAttacking") == 0)
		{
			eOut = PROPERTY_TYPE_BOOL;
			return true;
		}
		if (std::strcmp(szVariable, "comboCount") == 0)
		{
			eOut = PROPERTY_TYPE_INT32;
			return true;
		}
		if (std::strcmp(szVariable, "payload") == 0)
		{
			eOut = PROPERTY_TYPE_FLOAT;
			return true;
		}
		return false;
	}

	u_int CheckGetVariableTypes(const Zenith_GraphDefinition& xDefinition, const char* szAssetPath)
	{
		const Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find("GetVariable");
		u_int uCount = 0u;
		for (u_int uNode = 0u; uNode < xDefinition.GetNodeCount(); ++uNode)
		{
			const Zenith_GraphNodeDef& xNodeDef = xDefinition.GetNodeAt(uNode);
			if (xNodeDef.m_strTypeName != "GetVariable") continue;
			++uCount;
			Zenith_GraphNode* pxNode = pxInfo ? pxInfo->m_pfnCreate() : nullptr;
			CheckTrue(pxNode != nullptr, "GetVariable is registered for Combat type-resolution checks");
			if (pxNode == nullptr) continue;
			xDefinition.ApplyNodeParams(xNodeDef.m_uNodeID, pxNode, *pxInfo);
			const Zenith_PropertyTable* pxProperties = pxInfo->m_pfnGetPropertyTable ? pxInfo->m_pfnGetPropertyTable() : nullptr;
			const Zenith_ReflectedProperty* pxVariable = pxProperties ? pxProperties->FindProperty("m_strVariable") : nullptr;
			Zenith_PropertyValue xVariable;
			if (pxVariable != nullptr) pxVariable->m_pfnGet(pxNode, xVariable);
			Zenith_PropertyType eExpected = eGRAPH_PIN_TYPE_ANY;
			const bool bExpected = pxVariable != nullptr && xVariable.GetType() == PROPERTY_TYPE_STRING
				&& ExpectedGetVariableType(xVariable.GetString().c_str(), eExpected);
			const Zenith_GraphPinTable* pxPins = pxInfo->m_pfnGetPinTable ? pxInfo->m_pfnGetPinTable() : nullptr;
			const u_int uValuePin = pxPins ? pxPins->FindPinIndex("Value") : 0u;
			Zenith_PropertyType eActual = eGRAPH_PIN_TYPE_ANY;
			const bool bResolved = pxPins != nullptr && uValuePin < pxPins->GetPinCount()
				&& Zenith_GraphDefinitionValidator::ResolvePinType(xDefinition, xRegistry, xNodeDef.m_uNodeID, uValuePin, eActual);
			char acWhat[256];
			std::snprintf(acWhat, sizeof(acWhat), "%s GetVariable(%s) resolves a declared concrete Value type",
				szAssetPath, pxVariable && xVariable.GetType() == PROPERTY_TYPE_STRING ? xVariable.GetString().c_str() : "<missing>");
			CheckTrue(bExpected && bResolved && eActual == eExpected && eActual != eGRAPH_PIN_TYPE_ANY, acWhat);
			delete pxNode;
		}
		return uCount;
	}

	Zenith_GraphNode* FindOnlyNodeOfType(Zenith_BehaviourGraph& xGraph,
		const Zenith_GraphDefinition& xDefinition, const char* szType)
	{
		u_int uFound = 0u;
		for (u_int u = 0u; u < xDefinition.GetNodeCount(); ++u)
		{
			const Zenith_GraphNodeDef& xDef = xDefinition.GetNodeAt(u);
			if (xDef.m_strTypeName == szType) { uFound = xDef.m_uNodeID; }
		}
		return uFound != 0u ? xGraph.FindNode(uFound) : nullptr;
	}

	void RunPlayerAttackWriterCrossAnchor()
	{
		Zenith_TempScene xScene("CombatPlayerAttackWriterContract");
		const Zenith_EntityID xOldPlayer = Combat_GameComponent::GetPlayerEntityID();
		Zenith_Entity xPlayer = xScene.CreateEntity("PlayerAttackWriter");
		xPlayer.AddComponent<Zenith_ColliderComponent>();
		Zenith_AnimatorComponent& xAnimator = xPlayer.AddComponent<Zenith_AnimatorComponent>();
		Combat_PlayerComponent& xPlayerComponent = xPlayer.AddComponent<Combat_PlayerComponent>();
		xAnimator.OnStart(); xPlayerComponent.OnAwake(); xPlayerComponent.OnStart();

		Zenith_InputActions& xActions = g_xEngine.Actions();
		const bool bOldOverride = xActions.IsProfileOverridden(); const u_int8 uOldProfile = xActions.GetActiveProfile();
		struct PlayerAttackFixtureRestore
		{
			Combat_PlayerComponent& m_xPlayer;
			Zenith_EntityID m_xOldPlayer;
			Zenith_InputActions& m_xActions;
			bool m_bOldOverride;
			u_int8 m_uOldProfile;
			~PlayerAttackFixtureRestore()
			{
				m_xPlayer.OnDestroy(); Combat_GameComponent::RegisterPlayer(m_xOldPlayer);
				g_xEngine.Input().ResetTransientForTest(); g_xEngine.Pointers().ResetTransientForTest(); m_xActions.ResetTransientForTest();
				if (m_bOldOverride) m_xActions.SetProfileOverride(m_uOldProfile); else m_xActions.ClearOverride();
			}
		} xRestore{ xPlayerComponent, xOldPlayer, xActions, bOldOverride, uOldProfile };
		g_xEngine.Input().ResetTransientForTest(); g_xEngine.Pointers().ResetTransientForTest(); xActions.ResetTransientForTest();
		xActions.SetProfileOverride(Combat_Bindings::uPROFILE_DESKTOP);
		g_xEngine.Input().DrainPendingPlatformEvents(); g_xEngine.Pointers().BeginFrame(1.0f);
		Zenith_InputEvent xPress; xPress.m_eType = INPUT_EVENT_MOUSE_PRESS; xPress.m_iCode = ZENITH_MOUSE_BUTTON_LEFT;
		g_xEngine.Input().AppendInjectedEvent(xPress); xActions.UpdateProfile(); xActions.FinalizeReservedUI(); xActions.FinalizeGameplay();
		CheckTrue(xPlayerComponent.Graph_PreTick(0.0f), "PlayerAttack writer fixture reaches its current dispatch");
		xPlayerComponent.Graph_MovementTick(0.0f);
		auto* pxState = xAnimator.GetStateMachine().GetState(CombatAnimStates::ATTACK1);
		CheckTrue(pxState != nullptr && pxState->GetBlendTree() != nullptr, "PlayerAttack writer fixture has Attack1 hit-frame state");
		if (pxState != nullptr && pxState->GetBlendTree() != nullptr) { xAnimator.GetStateMachine().SetState(CombatAnimStates::ATTACK1); pxState->GetBlendTree()->SetNormalizedTime(0.5f); }

		Zenith_GraphDefinition xDef; Zenith_GraphBuilder xBuilder(xDef); BuildGraph_CombatPlayerAttack(xBuilder);
		const bool bBuilt = xBuilder.Build();
		CheckTrue(bBuilt, "production Combat_PlayerAttack definition builds for cross-anchor writer regression");
		if (!bBuilt) { return; }
		Zenith_BehaviourGraph xGraph; const bool bInitialised = xGraph.InitialiseFromDefinition(xDef);
		CheckTrue(bInitialised, "production Combat_PlayerAttack definition instantiates for cross-anchor writer regression");
		if (!bInitialised) { xGraph.Shutdown(); return; }
		CheckEqInt(static_cast<int>(xGraph.GetResolutionSkipCountForTest()), 0, "production Combat_PlayerAttack resolves zero skipped data edges");
		Zenith_GraphContext xContext; xContext.m_xSelf = xPlayer; xContext.m_pxGraph = &xGraph; xContext.m_pxBlackboard = &xGraph.GetBlackboard();
		Zenith_GraphNode* pxSuccessRegister = FindOnlyNodeOfType(xGraph, xDef, "CombatRegisterHits");
		CheckTrue(pxSuccessRegister != nullptr, "production PlayerAttack resolves its unique RegisterHits node");
		if (pxSuccessRegister != nullptr)
		{
			pxSuccessRegister->SetOutput<int32_t>(xContext, CombatNode_RegisterHits::uPIN_HitCount, 73);
			const Zenith_PropertyValue* pxPreseed = pxSuccessRegister->GetOutputForTest(CombatNode_RegisterHits::uPIN_HitCount);
			CheckTrue(pxPreseed != nullptr && pxPreseed->GetType() == PROPERTY_TYPE_INT32 && pxPreseed->GetInt32() == 73,
				"success PlayerAttack RegisterHits sentinel is seeded to 73");
		}
		xGraph.FireCustomEvent("AttackTick", xContext);
		CheckTrue(xGraph.GetBlackboard().GetBool("isAttacking", false), "QueryAttackState IsAttacking writer reaches later AttackTick anchor");
		CheckEqInt(xGraph.GetBlackboard().GetInt32("comboCount", 0), 1, "QueryAttackState ComboCount writer publishes the nonzero live combo");
		CheckTrue(xGraph.GetBlackboard().GetBool("hitFrameReady", false), "QueryAttackState HitFrame writer publishes the live hit frame");
		const Zenith_PropertyValue* pxSuccessRegisterOutput = pxSuccessRegister ? pxSuccessRegister->GetOutputForTest(CombatNode_RegisterHits::uPIN_HitCount) : nullptr;
		CheckTrue(pxSuccessRegisterOutput != nullptr && pxSuccessRegisterOutput->GetType() == PROPERTY_TYPE_INT32 && pxSuccessRegisterOutput->GetInt32() == 0,
			"successful AttackTick executes RegisterHits and overwrites the 73 sentinel");
		xGraph.Shutdown();

		Zenith_GraphDefinition xFailDef; Zenith_GraphBuilder xFailBuilder(xFailDef); BuildGraph_CombatPlayerAttack(xFailBuilder);
		const bool bFailBuilt = xFailBuilder.Build(); CheckTrue(bFailBuilt, "failed-query PlayerAttack definition builds");
		if (!bFailBuilt) { return; }
		Zenith_BehaviourGraph xFailGraph; const bool bFailInitialised = xFailGraph.InitialiseFromDefinition(xFailDef);
		CheckTrue(bFailInitialised, "failed-query PlayerAttack definition instantiates");
		if (!bFailInitialised) { xFailGraph.Shutdown(); return; }
		CheckEqInt(static_cast<int>(xFailGraph.GetResolutionSkipCountForTest()), 0, "failed-query PlayerAttack resolves zero skipped data edges");
		Zenith_PropertyValue xTrue; xTrue.SetBool(true); Zenith_PropertyValue xThree; xThree.SetInt32(3);
		xFailGraph.GetBlackboard().SetValue("isAttacking", xTrue); xFailGraph.GetBlackboard().SetValue("hitFrameReady", xTrue); xFailGraph.GetBlackboard().SetValue("comboCount", xThree);
		Zenith_GraphNode* pxRegister = FindOnlyNodeOfType(xFailGraph, xFailDef, "CombatRegisterHits");
		CheckTrue(pxRegister != nullptr, "failed-query PlayerAttack resolves its unique RegisterHits node");
		Zenith_GraphContext xFailContext; xFailContext.m_pxGraph = &xFailGraph; xFailContext.m_pxBlackboard = &xFailGraph.GetBlackboard();
		if (pxRegister != nullptr) pxRegister->SetOutput<int32_t>(xFailContext, CombatNode_RegisterHits::uPIN_HitCount, 91);
		xFailGraph.FireCustomEvent("AttackTick", xFailContext);
		CheckTrue(xFailGraph.GetBlackboard().GetBool("isAttacking", false), "failed QueryAttackState preserves seeded IsAttacking");
		CheckTrue(xFailGraph.GetBlackboard().GetBool("hitFrameReady", false), "failed QueryAttackState preserves seeded HitFrame");
		CheckEqInt(xFailGraph.GetBlackboard().GetInt32("comboCount", 0), 3, "failed QueryAttackState preserves seeded ComboCount");
		const Zenith_PropertyValue* pxRegisterOutput = pxRegister ? pxRegister->GetOutputForTest(CombatNode_RegisterHits::uPIN_HitCount) : nullptr;
		CheckTrue(pxRegisterOutput != nullptr && pxRegisterOutput->GetType() == PROPERTY_TYPE_INT32
			&& pxRegisterOutput->GetInt32() == 91, "failed query retains the 91 RegisterHits sentinel");
		xFailGraph.Shutdown();
	}

	void RunOneRow(const GraphBuilderRow& xRow, u_int& uTotalDataEdges, u_int& uTotalGetVariables)
	{
		char acWhat[256];

		Zenith_GraphDefinition xDefinition;
		Zenith_GraphBuilder xBuilder(xDefinition);
		xRow.m_pfnBuild(xBuilder);

		// Build() runs the FULL-tier validation pass AFTER the commit loop (before
		// it the blobs still hold AddNode defaults rather than Param* values), and
		// keeps the report on the builder -- so the builder must stay alive while
		// the findings are read.
		const bool bBuilt = xBuilder.Build();
		std::snprintf(acWhat, sizeof(acWhat), "%s builds with no authoring error", xRow.m_szAssetPath);
		CheckTrue(bBuilt, acWhat);
		if (bBuilt)
		{
			std::snprintf(acWhat, sizeof(acWhat), "%s authored at least one node", xRow.m_szAssetPath);
			CheckTrue(xDefinition.GetNodeCount() > 0, acWhat);

			std::snprintf(acWhat, sizeof(acWhat), "%s authors its exact data-edge count", xRow.m_szAssetPath);
			CheckEqInt(static_cast<int>(xDefinition.GetDataEdgeCount()), static_cast<int>(xRow.m_uDataEdges), acWhat);
			uTotalDataEdges += xDefinition.GetDataEdgeCount();
			const u_int uGetVariables = CheckGetVariableTypes(xDefinition, xRow.m_szAssetPath);
			uTotalGetVariables += uGetVariables;
			std::snprintf(acWhat, sizeof(acWhat), "%s authors its exact GetVariable count", xRow.m_szAssetPath);
			CheckEqInt(static_cast<int>(uGetVariables), static_cast<int>(xRow.m_uGetVariables), acWhat);
			Zenith_BehaviourGraph xGraph;
			const bool bInitialised = xGraph.InitialiseFromDefinition(xDefinition);
			std::snprintf(acWhat, sizeof(acWhat), "%s instantiates", xRow.m_szAssetPath);
			CheckTrue(bInitialised, acWhat);
			if (bInitialised)
			{
				std::snprintf(acWhat, sizeof(acWhat), "%s resolves ZERO skipped data edges", xRow.m_szAssetPath);
				CheckEqInt(static_cast<int>(xGraph.GetResolutionSkipCountForTest()), 0, acWhat);
			}
			xGraph.Shutdown();
		}

		int iErrors = 0;
		for (u_int uFinding = 0; uFinding < xBuilder.GetValidationFindingCount(); ++uFinding)
		{
			const Zenith_GraphValidationFinding& xFinding = xBuilder.GetValidationFindingAt(uFinding);
			if (xFinding.m_eSeverity != GRAPH_VALIDATION_SEVERITY_ERROR)
			{
				continue;	// LIST_NAME / DECLARED_UNUSED and friends stay warnings
			}
			++iErrors;
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[CombatGraphs]   %s node=%u:%s pin=%s var=%s rule=%s | %s",
				xRow.m_szAssetPath, xFinding.m_uNodeID,
				xFinding.m_strTypeName.c_str(),
				xFinding.m_strPin.empty() ? "-" : xFinding.m_strPin.c_str(),
				xFinding.m_strVar.empty() ? "-" : xFinding.m_strVar.c_str(),
				Zenith_GraphDefinitionValidator::GetRuleName(xFinding.m_eRule),
				xFinding.m_strWhat.c_str());
		}
		std::snprintf(acWhat, sizeof(acWhat),
			"%s reports ZERO error-severity findings (Build() latches on one)", xRow.m_szAssetPath);
		CheckEqInt(iErrors, 0, acWhat);
	}

	void Setup_GraphsValidateClean()
	{
		ResetChecks();
		g_bRan = false;
	}

	bool Step_GraphsValidateClean(int /*iFrame*/)
	{
		// A floor, honestly labelled: it catches a row DELETED from the table
		// above, never a sixth builder added without one.
		CheckEqInt(static_cast<int>(uGRAPH_BUILDER_ROWS), 5,
			"the builder table still lists all five graphs Combat authors");

		u_int uTotalDataEdges = 0u;
		u_int uTotalGetVariables = 0u;
		for (u_int uRow = 0; uRow < uGRAPH_BUILDER_ROWS; ++uRow)
		{
			RunOneRow(g_axGraphBuilders[uRow], uTotalDataEdges, uTotalGetVariables);
		}
		RunPlayerAttackWriterCrossAnchor();
		CheckEqInt(static_cast<int>(uTotalDataEdges), 35,
			"all five Combat builders author the required 35 data edges");
		CheckEqInt(static_cast<int>(uTotalGetVariables), 18,
			"all five Combat builders author the required 18 GetVariable nodes");
		g_bRan = true;
		return false;	// entirely synchronous - one frame is all this needs
	}

	bool Verify_GraphsValidateClean()
	{
		CheckTrue(g_bRan, "the validate-clean sweep ran");
		return ReportChecks("Combat_GraphsValidateClean");
	}
}

static const Zenith_AutomatedTest g_xCombatGraphsValidateCleanTest = {
	"Combat_GraphsValidateClean",
	&Setup_GraphsValidateClean,
	&Step_GraphsValidateClean,
	&Verify_GraphsValidateClean,
	/*maxFrames*/ 8,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xCombatGraphsValidateCleanTest);

#endif // ZENITH_INPUT_SIMULATOR && ZENITH_TOOLS
