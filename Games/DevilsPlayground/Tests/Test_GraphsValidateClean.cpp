#include "Zenith.h"

#if defined(ZENITH_INPUT_SIMULATOR) && defined(ZENITH_TOOLS)

// ============================================================================
// DP_GraphsValidateClean_Test -- every DevilsPlayground graph builds, and the
// graph validator reports ZERO error-severity findings for it.
//
// Each of the 12 builders is run IN PROCESS through a Zenith_GraphBuilder, so
// there is no .bgraph on disk to go stale and no dependency on a prior tools
// authoring pass having left one behind. Per row:
//
//   - Build() == true               (no authoring error: unknown type, unknown
//                                    property, rejected edge)
//   - zero ERROR-severity validation findings
//
// A-8 latched the validator: an ERROR finding is what makes Build() return
// false, so the first bullet now implies the second. Both are still asserted,
// because only the second NAMES the rule, the variable and the node. Anything
// that stays a WARNING -- DECLARED_UNUSED, a LIST name, an unresolved instance
// type -- is deliberately NOT checked here.
//
// Each hit is logged with its rule, variable, node id and node type, because the
// count alone would not tell anyone which of a dozen chains moved.
//
// ★ THE HONEST FLOOR, carried over from ScriptTest_Contracts.cpp:489-494: the
// row count check catches a builder DELETED from the table, not a THIRTEENTH
// builder added without a row. Nothing enumerates the builders at compile time,
// so the only guard against a new graph slipping past this test is the row going
// in with it. AuthorBehaviourGraphs() in DevilsPlayground.cpp is the list to
// mirror -- one AddStep_GraphBuild there, one row here.
//
// ★ AUTOMATED, NOT A ZENITH_TEST, and TOOLS-GATED. DevilsPlayground's gate runs
// the automated-test registry and passes --skip-unit-tests, so a ZENITH_TEST
// here would never execute (see Test_GraphPinTotality.cpp's header for the full
// reasoning, including why ZENITH_ASSERT_* outside a test body checks nothing).
// The builders themselves live inside DevilsPlayground.cpp's ZENITH_TOOLS block,
// so this whole TU is gated the same way -- referencing them from a _False
// configuration would be a link error the Null_*_True gate could not see.
//
// Hermetic: loads no scene, needs no graphics device, finishes in one frame.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Scripting/Zenith_GraphDefinitionValidator.h"

#include "DP_Graphs.h"
#include "Components/DPVillager_Component.h"
#include "Components/DPItemBase_Component.h"
#include "Components/DPForge_Component.h"
#include "Components/DPPlayerController_Component.h"
#include "Components/DPPauseMenuController_Component.h"
#include "Components/Priest_Component.h"

#include <cstdio>

namespace
{
	int g_iChecks = 0;
	int g_iFailures = 0;
	bool g_bValidateRan = false;

	void CheckTrue(bool bCondition, const char* szWhat)
	{
		++g_iChecks;
		if (!bCondition)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphsClean] FAILED: %s", szWhat);
		}
	}

	void CheckEqInt(int iActual, int iExpected, const char* szWhat)
	{
		++g_iChecks;
		if (iActual != iExpected)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphsClean] FAILED: %s (expected %d, got %d)",
				szWhat, iExpected, iActual);
		}
	}

	bool ReportChecks(const char* szTest)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphsClean] %s: %d checks, %d failed",
			szTest, g_iChecks, g_iFailures);
		// A test that asserted nothing is a test that cannot fail.
		return g_iFailures == 0 && g_iChecks > 0;
	}

	struct GraphBuilderRow
	{
		const char* m_szAssetPath;
		void (*m_pfnBuild)(Zenith_GraphBuilder&);
	};

	// The 12 rows AuthorBehaviourGraphs() hands to AddStep_GraphBuild, in the
	// same order. The six graphs whose asset path is a component constant use
	// THAT constant rather than restating the string: a row that restated it
	// would prove only that this file agrees with itself.
	const GraphBuilderRow g_axGraphBuilders[] = {
		{ DPVillager_Component::kszGraphAsset,           &BuildGraph_DPVillager },
		{ DPItemBase_Component::kszGraphAsset,           &BuildGraph_DPItem },
		{ DPForge_Component::kszGraphAsset,              &BuildGraph_DPForge },
		{ DPPlayerController_Component::kszGraphAsset,   &BuildGraph_DPPlayerControl },
		{ DPPauseMenuController_Component::kszGraphAsset,&BuildGraph_DPPauseMenu },
		{ Priest_Component::kszGraphAsset,               &BuildGraph_DPPriest },
		{ "game:Graphs/DP_MainMenu.bgraph",              &BuildGraph_DPMainMenu },
		{ "game:Graphs/DP_Pentagram.bgraph",             &BuildGraph_DPPentagram },
		{ "game:Graphs/DP_Chest.bgraph",                 &BuildGraph_DPChest },
		{ "game:Graphs/DP_NoiseMachine.bgraph",          &BuildGraph_DPNoiseMachine },
		{ "game:Graphs/DP_DoubleDoor.bgraph",            &BuildGraph_DPDoubleDoor },
		{ "game:Graphs/DP_Door.bgraph",                  &BuildGraph_DPDoor },
	};
	constexpr u_int uGRAPH_BUILDER_ROWS = static_cast<u_int>(COUNT_OF(g_axGraphBuilders));

	void Setup_GraphsValidateClean()
	{
		g_iChecks = 0;
		g_iFailures = 0;
		g_bValidateRan = false;
	}

	bool Step_GraphsValidateClean(int /*iFrame*/)
	{
		CheckEqInt(static_cast<int>(uGRAPH_BUILDER_ROWS), 12,
			"the builder table still lists all twelve graphs DevilsPlayground authors");

		for (u_int uRow = 0; uRow < uGRAPH_BUILDER_ROWS; ++uRow)
		{
			const GraphBuilderRow& xRow = g_axGraphBuilders[uRow];
			char acWhat[256];

			Zenith_GraphDefinition xDefinition;
			int iErrors = 0;
			{
				Zenith_GraphBuilder xBuilder(xDefinition);
				xBuilder.SetGraphName(xRow.m_szAssetPath);
				xRow.m_pfnBuild(xBuilder);

				const bool bBuilt = xBuilder.Build();
				std::snprintf(acWhat, sizeof(acWhat), "%s builds with no authoring error", xRow.m_szAssetPath);
				CheckTrue(bBuilt, acWhat);

				// The findings live on the builder and die with it - read them
				// inside this scope.
				for (u_int uFinding = 0; uFinding < xBuilder.GetValidationFindingCount(); ++uFinding)
				{
					const Zenith_GraphValidationFinding& xFinding = xBuilder.GetValidationFindingAt(uFinding);
					if (xFinding.m_eSeverity != GRAPH_VALIDATION_SEVERITY_ERROR)
					{
						continue;	// warnings stay warnings (DECLARED_UNUSED, LIST_NAME, ...)
					}
					++iErrors;
					Zenith_Log(LOG_CATEGORY_UNITTEST,
						"[DPGraphsClean]   %s node=%u:%s pin=%s var=%s rule=%s | %s",
						xRow.m_szAssetPath,
						xFinding.m_uNodeID,
						xFinding.m_strTypeName.empty() ? "-" : xFinding.m_strTypeName.c_str(),
						xFinding.m_strPin.empty() ? "-" : xFinding.m_strPin.c_str(),
						xFinding.m_strVar.empty() ? "-" : xFinding.m_strVar.c_str(),
						Zenith_GraphDefinitionValidator::GetRuleName(xFinding.m_eRule),
						xFinding.m_strWhat.c_str());
				}
			}

			std::snprintf(acWhat, sizeof(acWhat), "%s reports ZERO error-severity findings", xRow.m_szAssetPath);
			CheckEqInt(iErrors, 0, acWhat);

			std::snprintf(acWhat, sizeof(acWhat), "%s authored at least one node", xRow.m_szAssetPath);
			CheckTrue(xDefinition.GetNodeCount() > 0, acWhat);
		}

		g_bValidateRan = true;
		return false;	// entirely synchronous - one frame is all this needs
	}

	bool Verify_GraphsValidateClean()
	{
		CheckEqInt(g_bValidateRan ? 1 : 0, 1, "the per-builder validation checks ran");
		return ReportChecks("DP_GraphsValidateClean_Test");
	}
}

static const Zenith_AutomatedTest g_xDPGraphsValidateCleanTest = {
	"DP_GraphsValidateClean_Test",
	&Setup_GraphsValidateClean,
	&Step_GraphsValidateClean,
	&Verify_GraphsValidateClean,
	10 // one frame of work; the cap is a safety net only
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDPGraphsValidateCleanTest);

#endif // ZENITH_INPUT_SIMULATOR && ZENITH_TOOLS
