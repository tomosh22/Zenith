#include "Zenith.h"

#if defined(ZENITH_INPUT_SIMULATOR) && defined(ZENITH_TOOLS)

// ============================================================================
// Combat_GraphsValidateClean -- the mechanical precondition for latching the
// graph validator (A-8).
//
// Every one of Combat's five boot-authored graphs is built HERE, in process,
// from the SAME BuildGraph_Combat* function the tools boot writes the .bgraph
// from, and asserted to produce ZERO findings with m_bWouldBeError. That is the
// property A-8 flips into a hard error: the day it stops holding, this unit
// names the rule, the variable and the node instead of a tools boot going red
// weeks later with no owner.
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
#include "Combat/Combat_Graphs.h"

#include <cstdio>

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
	};

	// Keyed by ASSET PATH, spelled exactly as Project_RegisterEditorAutomationSteps
	// spells it in its AddStep_GraphBuild calls (Combat.cpp:790-803).
	const GraphBuilderRow g_axGraphBuilders[] =
	{
		{ "game:Graphs/Combat_PlayerAttack.bgraph", &BuildGraph_CombatPlayerAttack },
		{ "game:Graphs/Combat_RoundFlow.bgraph",    &BuildGraph_CombatRoundFlow },
		{ "game:Graphs/Combat_PlayerState.bgraph",  &BuildGraph_CombatPlayerState },
		{ "game:Graphs/Combat_EnemyBrain.bgraph",   &BuildGraph_CombatEnemyBrain },
		{ "game:Graphs/Combat_GameFlow.bgraph",     &BuildGraph_CombatGameFlow },
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

		// Build() runs the FULL-tier validation pass AFTER the commit loop (before
		// it the blobs still hold AddNode defaults rather than Param* values), and
		// keeps the report on the builder -- so the builder must stay alive while
		// the findings are read.
		const bool bBuilt = xBuilder.Build();
		std::snprintf(acWhat, sizeof(acWhat), "%s builds with no authoring error", xRow.m_szAssetPath);
		CheckTrue(bBuilt, acWhat);

		std::snprintf(acWhat, sizeof(acWhat), "%s authored at least one node", xRow.m_szAssetPath);
		CheckTrue(xDefinition.GetNodeCount() > 0, acWhat);

		int iWouldBeErrors = 0;
		for (u_int uFinding = 0; uFinding < xBuilder.GetValidationFindingCount(); ++uFinding)
		{
			const Zenith_GraphValidationFinding& xFinding = xBuilder.GetValidationFindingAt(uFinding);
			if (!xFinding.m_bWouldBeError)
			{
				continue;	// LIST_NAME / DECLARED_UNUSED and friends stay warnings
			}
			++iWouldBeErrors;
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
			"%s reports ZERO would-be-error findings (A-8 can latch)", xRow.m_szAssetPath);
		CheckEqInt(iWouldBeErrors, 0, acWhat);
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
