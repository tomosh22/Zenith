#include "Zenith.h"

#if defined(ZENITH_INPUT_SIMULATOR) && defined(ZENITH_TOOLS)

/**
 * RT_GraphsValidateClean - the mechanical precondition for latching the graph
 * validator (A-8), for RenderTest's two boot-authored graphs.
 *
 * Each is built HERE, in process, from the SAME BuildGraph_RenderTest*
 * function the tools boot writes the .bgraph from, and asserted to produce ZERO
 * ERROR-severity findings. A-8 latched that property into Build()'s return; if
 * it ever stops holding, this unit names the rule, the variable and the node,
 * instead of a tools boot going red later with no owner.
 *
 * ★ BUILT FROM THE BUILDER, NEVER FROM DISK - the Test_TennisBrainContract.cpp
 * doctrine. `.bgraph` files are gitignored and written only by a tools boot, so
 * a disk-reading unit would find nothing on a fresh checkout and would have to
 * skip, and a skip counts as a pass.
 *
 * ★ SEVEN RTTennis* NODES ARE ANNOTATED HERE: their 15 data INPUTs and three
 * BallEntity TARGET_REF descriptors make the validator see every graph read.
 * BallEntity is consequently declared as an ENTITY_ID with an INVALID packed
 * seed in BuildGraph_RenderTestTennisBrain; the bridge still overwrites it in
 * OnStart. OppEntity remains deliberately undeclared because no graph node
 * reads it. RTTennisTickGate, RTTennisDecideShot, and the four RTPlayer* verbs
 * have no blackboard read and remain OPAQUE by design.
 *
 * ★ #ifdef ZENITH_TOOLS: BuildGraph_RenderTestPlayerActions lives inside
 * RenderTest.cpp's tools block, so a `_False` config has no definition to call.
 * BuildGraph_RenderTestTennisBrain is unconditional, but pairing them in one
 * table is worth more than covering one graph in one more config.
 *
 * ★ A THIRD BUILDER ADDED WITHOUT A ROW HERE GOES UNCHECKED. The builders are
 * free functions; nothing enumerates them at compile time. The floor below
 * catches a row DELETED, which is the half that can be checked mechanically.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Scripting/Zenith_GraphDefinitionValidator.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "RenderTest/RenderTest_Tennis.h"                       // BuildGraph_RenderTestTennisBrain
#include "RenderTest/RenderTest_Graphs.h"                       // BuildGraph_RenderTestPlayerActions
#include "RenderTest/Components/RenderTest_TennisAgentComponent.h"   // kszGraphAsset

#include <cstdio>
#include <cstring>

namespace
{
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
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RTGraphs] FAILED: %s", szWhat);
		}
	}

	void CheckEqInt(int iActual, int iExpected, const char* szWhat)
	{
		++g_iChecks;
		if (iActual != iExpected)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[RTGraphs] FAILED: %s (expected %d, got %d)",
				szWhat, iExpected, iActual);
		}
	}

	bool ReportChecks(const char* szTest)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RTGraphs] %s: %d checks, %d failed",
			szTest, g_iChecks, g_iFailures);
		// A test that asserted nothing is a test that cannot fail.
		return g_iFailures == 0 && g_iChecks > 0;
	}

	struct GraphBuilderRow
	{
		const char* m_szAssetPath;
		void (*m_pfnBuild)(Zenith_GraphBuilder&);
	};

	const GraphBuilderRow g_axGraphBuilders[] =
	{
		{ RenderTest_TennisAgentComponent::kszGraphAsset,   &BuildGraph_RenderTestTennisBrain },
		{ "game:Graphs/RenderTest_PlayerActions.bgraph",    &BuildGraph_RenderTestPlayerActions },
	};

	constexpr u_int uGRAPH_BUILDER_ROWS =
		static_cast<u_int>(sizeof(g_axGraphBuilders) / sizeof(g_axGraphBuilders[0]));

	u_int ExpectedDataEdgeCount(const char* szAssetPath)
	{
		return std::strcmp(szAssetPath, RenderTest_TennisAgentComponent::kszGraphAsset) == 0 ? 24u : 0u;
	}

	u_int ExpectedGetVariableCount(const char* szAssetPath)
	{
		return std::strcmp(szAssetPath, RenderTest_TennisAgentComponent::kszGraphAsset) == 0 ? 21u : 0u;
	}

	bool ExpectedGetVariableType(const char* szVariable, Zenith_PropertyType& eOut)
	{
		using namespace RenderTest_TennisBB;
		if (std::strcmp(szVariable, "tickAccum") == 0) { eOut = PROPERTY_TYPE_FLOAT; return true; }
		if (std::strcmp(szVariable, k_szPhase) == 0 || std::strcmp(szVariable, k_szBallEpoch) == 0
			|| std::strcmp(szVariable, k_szMySide) == 0) { eOut = PROPERTY_TYPE_INT32; return true; }
		if (std::strcmp(szVariable, k_szIsServer) == 0 || std::strcmp(szVariable, k_szIsSecondServe) == 0
			|| std::strcmp(szVariable, k_szIsMyBall) == 0 || std::strcmp(szVariable, k_szServeBallParked) == 0
			|| std::strcmp(szVariable, k_szServeFromDeuce) == 0) { eOut = PROPERTY_TYPE_BOOL; return true; }
		if (std::strcmp(szVariable, k_szBallSpin) == 0) { eOut = PROPERTY_TYPE_VECTOR3; return true; }
		return false;
	}

	u_int CheckGetVariableTypes(const Zenith_GraphDefinition& xDefinition, const char* szAssetPath)
	{
		const Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		u_int uCount = 0;
		for (u_int uNode = 0; uNode < xDefinition.GetNodeCount(); ++uNode)
		{
			const Zenith_GraphNodeDef& xNodeDef = xDefinition.GetNodeAt(uNode);
			if (xNodeDef.m_strTypeName != "GetVariable") continue;
			++uCount;
			const Zenith_GraphNodeTypeInfo* pxInfo = xRegistry.Find("GetVariable");
			Zenith_GraphNode* pxNode = pxInfo ? pxInfo->m_pfnCreate() : nullptr;
			CheckTrue(pxNode != nullptr, "GetVariable is registered for RenderTest type-resolution checks");
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
				&& Zenith_GraphDefinitionValidator::ResolvePinType(xDefinition, xRegistry,
					xNodeDef.m_uNodeID, uValuePin, eActual);
			char acWhat[256];
			std::snprintf(acWhat, sizeof(acWhat), "%s GetVariable(%s) resolves its concrete Value type",
				szAssetPath, pxVariable && xVariable.GetType() == PROPERTY_TYPE_STRING
					? xVariable.GetString().c_str() : "<missing or non-string>");
			CheckTrue(bExpected && bResolved && eActual == eExpected && eActual != eGRAPH_PIN_TYPE_ANY, acWhat);
			delete pxNode;
		}
		return uCount;
	}

	void CheckTennisBallEntityDeclaration(const Zenith_GraphDefinition& xDefinition, const char* szAssetPath)
	{
		if (std::strcmp(szAssetPath, RenderTest_TennisAgentComponent::kszGraphAsset) != 0) return;
		const Zenith_GraphVariableDecl* pxBallEntity = nullptr;
		for (u_int uVariable = 0; uVariable < xDefinition.GetVariableCount(); ++uVariable)
		{
			const Zenith_GraphVariableDecl& xVariable = xDefinition.GetVariableAt(uVariable);
			if (xVariable.m_strName == RenderTest_TennisBB::k_szBallEntity)
			{
				pxBallEntity = &xVariable;
				break;
			}
		}
		CheckTrue(pxBallEntity != nullptr, "tennis graph declares BallEntity before graph initialization");
		if (pxBallEntity == nullptr) return;
		CheckEqInt(static_cast<int>(pxBallEntity->m_xDefault.GetType()), static_cast<int>(PROPERTY_TYPE_ENTITY_ID),
			"tennis BallEntity declaration has ENTITY_ID type");
		if (pxBallEntity->m_xDefault.GetType() != PROPERTY_TYPE_ENTITY_ID) return;
		CheckTrue(pxBallEntity->m_xDefault.GetPackedEntityID() == INVALID_ENTITY_ID.GetPacked(),
			"tennis BallEntity declaration seeds exactly INVALID_ENTITY_ID before attachment");
	}

	bool g_bRan = false;

	void RunOneRow(const GraphBuilderRow& xRow)
	{
		char acWhat[256];

		Zenith_GraphDefinition xDefinition;
		Zenith_GraphBuilder xBuilder(xDefinition);
		xRow.m_pfnBuild(xBuilder);

		// Build() runs the FULL-tier validation pass AFTER the commit loop and
		// keeps the report on the builder, so the builder stays alive below.
		const bool bBuilt = xBuilder.Build();
		std::snprintf(acWhat, sizeof(acWhat), "%s builds with no authoring error", xRow.m_szAssetPath);
		CheckTrue(bBuilt, acWhat);
		if (bBuilt)
		{
			std::snprintf(acWhat, sizeof(acWhat), "%s authored at least one node", xRow.m_szAssetPath);
			CheckTrue(xDefinition.GetNodeCount() > 0, acWhat);
			std::snprintf(acWhat, sizeof(acWhat), "%s authors its exact data-edge count", xRow.m_szAssetPath);
			CheckEqInt(static_cast<int>(xDefinition.GetDataEdgeCount()),
				static_cast<int>(ExpectedDataEdgeCount(xRow.m_szAssetPath)), acWhat);
			const u_int uGetVariables = CheckGetVariableTypes(xDefinition, xRow.m_szAssetPath);
			std::snprintf(acWhat, sizeof(acWhat), "%s authors its exact GetVariable count", xRow.m_szAssetPath);
			CheckEqInt(static_cast<int>(uGetVariables), static_cast<int>(ExpectedGetVariableCount(xRow.m_szAssetPath)), acWhat);
			CheckTennisBallEntityDeclaration(xDefinition, xRow.m_szAssetPath);

			Zenith_BehaviourGraph xGraph;
			const bool bInstanced = xGraph.InitialiseFromDefinition(xDefinition);
			std::snprintf(acWhat, sizeof(acWhat), "%s instantiates its authored graph", xRow.m_szAssetPath);
			CheckTrue(bInstanced, acWhat);
			if (bInstanced)
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
				continue;	// LIST_NAME / DECLARED_UNUSED are warnings and stay
			}
			++iErrors;
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[RTGraphs]   %s node=%u:%s pin=%s var=%s rule=%s | %s",
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
		CheckEqInt(static_cast<int>(uGRAPH_BUILDER_ROWS), 2,
			"the builder table still lists both graphs RenderTest authors");

		for (u_int uRow = 0; uRow < uGRAPH_BUILDER_ROWS; ++uRow)
		{
			RunOneRow(g_axGraphBuilders[uRow]);
		}
		g_bRan = true;
		return false;	// entirely synchronous - one frame is all this needs
	}

	bool Verify_GraphsValidateClean()
	{
		CheckTrue(g_bRan, "the validate-clean sweep ran");
		return ReportChecks("RT_GraphsValidateClean");
	}
}

static const Zenith_AutomatedTest g_xRTGraphsValidateCleanTest = {
	"RT_GraphsValidateClean",
	&Setup_GraphsValidateClean,
	&Step_GraphsValidateClean,
	&Verify_GraphsValidateClean,
	/*maxFrames*/ 8,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRTGraphsValidateCleanTest);

#endif // ZENITH_INPUT_SIMULATOR && ZENITH_TOOLS
