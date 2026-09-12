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
 * ★ RENDERTEST'S RTTennis* NODES ARE STILL OPAQUE, and that is why this unit
 * can be clean without a single new declaration. They read the blackboard
 * through the compile-time constants RenderTest_TennisBB::k_sz*
 * (RenderTest_TennisAgentComponent.h:47-59) rather than through a name
 * PROPERTY, so no pin descriptor can bind them and the validator sees neither
 * their reads nor their writes (Epic A records this; Epic B turns them into
 * pins). In particular k_szOppEntity and k_szBallEntity stay DELIBERATELY
 * undeclared - RenderTest.cpp:1726-1727 leaves both to the brain shim's OnStart
 * seed, and declaring one would move RenderTest.zscen for no validator gain.
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

		std::snprintf(acWhat, sizeof(acWhat), "%s authored at least one node", xRow.m_szAssetPath);
		CheckTrue(xDefinition.GetNodeCount() > 0, acWhat);

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
