#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ScriptTest_Boot_Test (C1) -- the Hub actually booted.
//
// WHAT THIS PROVES, AND WHY IT IS NOT "the exe did not crash"
//
// ScriptTest carries zero gameplay C++, so nothing in this game has a
// constructor, an OnStart or a component of its own that a boot failure could
// trip over. Everything that makes the hub a hub is DATA: an authored entity
// called GameManager, a Behaviour Graph slot pointing at ST_HubFlow, and a UI
// canvas whose element NAMES the graph's OnUIButtonClicked sources watch. Every
// one of those is a string with no compile-time link to anything, which is
// exactly the failure mode this game is built to expose -- a graph that loads
// cleanly and does nothing, on a scene that renders and answers no input.
//
// So the boot assertion is: the ACTIVE scene contains the manager entity, its
// graph slots all RESOLVED (an unresolved slot keeps its path verbatim and
// silently runs nothing), and its canvas carries the first hub button. If any
// of the three is missing the game booted to a dead menu, and every behavioural
// test that follows would fail for a reason that has nothing to do with its own
// subject.
//
// Every string below comes from ScriptTest_Graphs.h. A test that restated one
// would prove only that the test agrees with itself.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"

#include "ScriptTest/ScriptTest_Graphs.h"

namespace
{
	//-------------------------------------------------------------------------
	// Named-check accumulator. Every check runs and reports, so ONE run names
	// EVERY clause that moved rather than stopping at the first.
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
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[ScriptTestBoot] FAILED: %s", szWhat);
		}
	}

	void CheckEqInt(int iActual, int iExpected, const char* szWhat)
	{
		++g_iChecks;
		if (iActual != iExpected)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[ScriptTestBoot] FAILED: %s (expected %d, got %d)",
				szWhat, iExpected, iActual);
		}
	}

	bool ReportChecks(const char* szTest)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ScriptTestBoot] %s: %d checks, %d failed",
			szTest, g_iChecks, g_iFailures);
		// A test that asserted nothing is a test that cannot fail - an empty run
		// is a broken rig, not a pass.
		return g_iFailures == 0 && g_iChecks > 0;
	}

	//-------------------------------------------------------------------------
	// What the poll observed. Booleans and counts, never pointers: the scene
	// reload the harness runs between tests can relocate every component, and
	// Verify runs after Step has finished.
	//-------------------------------------------------------------------------
	bool g_bSawActiveScene = false;
	bool g_bFoundManager = false;
	bool g_bHasGraphComponent = false;
	int  g_iGraphSlotCount = 0;
	int  g_iResolvedSlotCount = 0;
	int  g_iUnresolvedNodeCount = 0;
	bool g_bHasUIComponent = false;
	bool g_bFoundHubButton = false;
	bool g_bSettledWithinBudget = false;
	int  g_iFramesPolled = 0;

	// One poll. Writes what it saw and answers "is everything in place yet?" -
	// the loop stops on the first fully-satisfied frame, so a partial state is
	// what Verify reports when it never arrives.
	bool PollHubState()
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData == nullptr)
		{
			return false;
		}
		g_bSawActiveScene = true;

		Zenith_Entity xManager = pxSceneData->FindEntityByName(ScriptTest::Entities::szGAME_MANAGER);
		if (!xManager.IsValid())
		{
			return false;
		}
		g_bFoundManager = true;

		// ---- the graph half -------------------------------------------------
		int iSlots = 0;
		int iResolved = 0;
		int iUnresolved = 0;
		Zenith_GraphComponent* pxGraphComponent = xManager.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphComponent != nullptr)
		{
			g_bHasGraphComponent = true;
			iSlots = static_cast<int>(pxGraphComponent->GetGraphCount());
			for (u_int u = 0; u < pxGraphComponent->GetGraphCount(); ++u)
			{
				Zenith_BehaviourGraph* pxGraph = pxGraphComponent->GetGraphAt(u);
				if (pxGraph == nullptr)
				{
					// An unresolvable .bgraph keeps its path + override bytes
					// verbatim (the unresolved-slot contract) and runs NOTHING.
					continue;
				}
				++iResolved;
				iUnresolved += static_cast<int>(pxGraph->GetUnresolvedCount());
			}
		}
		g_iGraphSlotCount = iSlots;
		g_iResolvedSlotCount = iResolved;
		g_iUnresolvedNodeCount = iUnresolved;

		// ---- the UI half ----------------------------------------------------
		bool bFoundButton = false;
		Zenith_UIComponent* pxUI = xManager.TryGetComponent<Zenith_UIComponent>();
		if (pxUI != nullptr)
		{
			g_bHasUIComponent = true;
			bFoundButton = pxUI->FindElement(ScriptTest::UINames::szBTN_MOTION) != nullptr;
		}
		g_bFoundHubButton = bFoundButton;

		return g_bHasGraphComponent
			&& iSlots >= 1
			&& iResolved == iSlots
			&& iUnresolved == 0
			&& g_bHasUIComponent
			&& bFoundButton;
	}

	void Setup_ScriptTestBoot()
	{
		ResetChecks();
		g_bSawActiveScene = false;
		g_bFoundManager = false;
		g_bHasGraphComponent = false;
		g_iGraphSlotCount = 0;
		g_iResolvedSlotCount = 0;
		g_iUnresolvedNodeCount = 0;
		g_bHasUIComponent = false;
		g_bFoundHubButton = false;
		g_bSettledWithinBudget = false;
		g_iFramesPolled = 0;
	}

	// Polls across frames rather than asserting on frame 0: a scene load is
	// asynchronous from a test's point of view, and blocking here would deadlock
	// the loop that services it.
	bool Step_ScriptTestBoot(int iFrame)
	{
		g_iFramesPolled = iFrame + 1;
		if (PollHubState())
		{
			g_bSettledWithinBudget = true;
			return false;
		}
		return true;
	}

	bool Verify_ScriptTestBoot()
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ScriptTestBoot] polled %d frames; scene=%d manager=%d graphComponent=%d slots=%d resolved=%d unresolvedNodes=%d ui=%d button=%d",
			g_iFramesPolled, g_bSawActiveScene ? 1 : 0, g_bFoundManager ? 1 : 0,
			g_bHasGraphComponent ? 1 : 0, g_iGraphSlotCount, g_iResolvedSlotCount,
			g_iUnresolvedNodeCount, g_bHasUIComponent ? 1 : 0, g_bFoundHubButton ? 1 : 0);

		CheckTrue(g_bSawActiveScene, "the boot left an ACTIVE scene");
		CheckTrue(g_bFoundManager, "the active scene contains the GameManager entity");
		CheckTrue(g_bHasGraphComponent, "GameManager carries a Zenith_GraphComponent");
		CheckTrue(g_iGraphSlotCount >= 1, "GameManager's graph component holds at least one slot");
		CheckEqInt(g_iResolvedSlotCount, g_iGraphSlotCount,
			"every graph slot RESOLVED (an unresolved slot keeps its path and runs nothing)");
		CheckEqInt(g_iUnresolvedNodeCount, 0,
			"no unresolved NODE inside the resolved graphs (a missing node type fails its chain silently)");
		CheckTrue(g_bHasUIComponent, "GameManager carries a Zenith_UIComponent");
		CheckTrue(g_bFoundHubButton, "the hub canvas carries the first gym button by the name ST_HubFlow watches");
		CheckTrue(g_bSettledWithinBudget, "the hub was fully in place before the frame budget ran out");

		return ReportChecks("ScriptTest_Boot_Test");
	}
}

static const Zenith_AutomatedTest g_xScriptTestBootTest = {
	"ScriptTest_Boot_Test",
	&Setup_ScriptTestBoot,
	&Step_ScriptTestBoot,
	&Verify_ScriptTestBoot,
	/*maxFrames*/ 600,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xScriptTestBootTest);

#endif // ZENITH_INPUT_SIMULATOR
