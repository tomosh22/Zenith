#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// DP_GraphPinTotality_Test -- every DP node class that carries a BLACKBOARD
// VARIABLE NAME is annotated with a pin descriptor, and every property a
// descriptor names really exists.
//
// The walk itself is the shared engine harness
// (Zenith/EntityComponent/Zenith_GraphPinTotality.TestHarness.inl) run against
// DP_RegisterGraphNodes -- the same registrar Project_RegisterGameComponents
// installs DP's node types with, so the set it covers is "whatever DP registers
// today" and a node added tomorrow is covered with no list to maintain. The
// harness swaps the process node registry to that one registrar and restores it
// from an RAII snapshot, replaying every row (engine AND game) in its original
// order, so a later test in the same batch sees the registry it expects.
//
// WHAT IT CATCHES: an un-annotated node is OPAQUE to
// Zenith_GraphDefinitionValidator -- it neither reads nor writes anything as far
// as validation can tell. So a whole node can drop out of the report while every
// graph looks clean, which is precisely the failure mode Test_GraphsValidateClean
// (the sibling test) would then be unable to see. This test is that test's
// premise.
//
// ★ WHY THIS IS AN AUTOMATED TEST AND NOT A ZENITH_TEST. Two reasons, and the
// second is the load-bearing one:
//
//   1. DevilsPlayground's gate does not RUN unit tests. `zenith test
//      DevilsPlayground` passes --skip-unit-tests and dp-tests.yml has no unit
//      leg, so a ZENITH_TEST here would be dead coverage -- green forever
//      because nothing ever executed it. The DP suite is the automated-test
//      registry, which is what actually runs.
//
//   2. ZENITH_ASSERT_* OUTSIDE A ZENITH_TEST BODY ASSERTS ON NOTHING
//      (Zenith_TestFramework.h:275 -- the runner has no live test case to record
//      a failure against). Calling the harness's asserting front door from a
//      Step() would therefore pass unconditionally, which is worse than no test.
//      That is why the harness grew a COUNTING front door,
//      Zenith_CountPinTableTotalityFailures: same one walk, each failure logged
//      with Zenith_Error, the COUNT returned for a real check. The counted-check
//      accumulator below is the ScriptTest_Contracts.cpp:86-135 shape, and
//      ReportChecks refuses a run that asserted nothing.
//
// Hermetic: loads no scene, needs no graphics device, finishes in one frame.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Components/DP_GraphNodes.h"
#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"

namespace
{
	int g_iChecks = 0;
	int g_iFailures = 0;
	bool g_bTotalityRan = false;

	void CheckEqInt(int iActual, int iExpected, const char* szWhat)
	{
		++g_iChecks;
		if (iActual != iExpected)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphPins] FAILED: %s (expected %d, got %d)",
				szWhat, iExpected, iActual);
		}
	}

	bool ReportChecks(const char* szTest)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphPins] %s: %d checks, %d failed",
			szTest, g_iChecks, g_iFailures);
		// A test that asserted nothing is a test that cannot fail.
		return g_iFailures == 0 && g_iChecks > 0;
	}

	void Setup_GraphPinTotality()
	{
		g_iChecks = 0;
		g_iFailures = 0;
		g_bTotalityRan = false;
	}

	bool Step_GraphPinTotality(int /*iFrame*/)
	{
		// No exempt properties: DP's six *Key properties and DPReadTuningFloat's
		// m_strKey are DP_Tuning keys rather than blackboard names, and none of
		// them matches the harness's m_str*Var* matcher in the first place -- so
		// there is nothing here that a pin cannot express.
		const u_int uFailures = Zenith_CountPinTableTotalityFailures(
			&DP_RegisterGraphNodes, "DP_GraphNodes.h", nullptr, 0u);

		CheckEqInt(static_cast<int>(uFailures), 0,
			"every DP node type's blackboard-variable-name property is covered by a pin descriptor, "
			"and every property a descriptor names exists (see the Zenith_Error lines for each hit)");

		g_bTotalityRan = true;
		return false;	// entirely synchronous - one frame is all this needs
	}

	bool Verify_GraphPinTotality()
	{
		CheckEqInt(g_bTotalityRan ? 1 : 0, 1, "the totality walk ran");
		return ReportChecks("DP_GraphPinTotality_Test");
	}
}

static const Zenith_AutomatedTest g_xDPGraphPinTotalityTest = {
	"DP_GraphPinTotality_Test",
	&Setup_GraphPinTotality,
	&Step_GraphPinTotality,
	&Verify_GraphPinTotality,
	10 // one frame of work; the cap is a safety net only
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDPGraphPinTotalityTest);

#endif // ZENITH_INPUT_SIMULATOR
