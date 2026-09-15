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
	void CheckEqInt(int iActual, int iExpected, const char* szWhat);

	void CheckPinInventory()
	{
		// This is deliberately a second, concrete contract beside the reflective
		// totality walk below.  It catches a table that remains internally valid
		// while a DP pin is inserted, removed, or re-ordered without updating the
		// public uPIN_ address used by Execute().
		const Zenith_GraphPinTable* apxTables[] = {
			&DPNode_ReadHeldObjective::GetPinTableStatic(), &DPNode_WinCheckAlreadyCollected::GetPinTableStatic(),
			&DPNode_WinNotifyCollected::GetPinTableStatic(), &DPNode_ConsumeHeldItem::GetPinTableStatic(),
			&DPNode_DispatchObjectivePlaced::GetPinTableStatic(), &DPNode_ConsumeKeyForUnlock::GetPinTableStatic(),
			&DPNode_DispatchDoorOpened::GetPinTableStatic(), &DPNode_DispatchDoorClosed::GetPinTableStatic(),
			&DPNode_AnimateDoorLeaves::GetPinTableStatic(), &DPNode_DispatchChestOpened::GetPinTableStatic(),
			&DPNode_DoorCheckKey::GetPinTableStatic(), &DPNode_DoorPentagramDeferral::GetPinTableStatic(),
			&DPNode_DoorAdvanceAnim::GetPinTableStatic(), &DPNode_VillagerEmitFootstep::GetPinTableStatic(),
			&DPNode_PickVillagerUnderCursor::GetPinTableStatic(), &DPNode_TryPossess::GetPinTableStatic(),
			&DPNode_ForgeCraft::GetPinTableStatic(), &DPNode_ReadTuningFloat::GetPinTableStatic(),
			&DPNode_ItemChildRefusal::GetPinTableStatic(), &DPNode_ItemArmChannel::GetPinTableStatic(),
			&DPNode_ItemCommitPickup::GetPinTableStatic(), &DPNode_ItemFinishPickup::GetPinTableStatic(), &DPNode_ItemRingBell::GetPinTableStatic(),
			&DPNode_ItemEvaporate::GetPinTableStatic(), &DPNode_PriestPickPatrolTarget::GetPinTableStatic(),
			&DPNode_PriestApprehendChannel::GetPinTableStatic()
		};

		int iDescriptors = 0, iInputs = 0, iOutputs = 0, iReadWrites = 0;
		for (const Zenith_GraphPinTable* pxTable : apxTables)
		{
			for (u_int u = 0; pxTable && u < pxTable->GetPinCount(); ++u)
			{
				const Zenith_GraphPinDesc& xPin = pxTable->GetPinAt(u);
				++iDescriptors;
				if (xPin.m_eRole == GRAPH_PIN_ROLE_INPUT) ++iInputs;
				if (xPin.m_eRole == GRAPH_PIN_ROLE_OUTPUT) ++iOutputs;
				if (xPin.m_eRole == GRAPH_PIN_ROLE_SELECTOR_READWRITE) ++iReadWrites;
			}
		}
		CheckEqInt(static_cast<int>(sizeof(apxTables) / sizeof(apxTables[0])), 26, "DP has 26 annotated graph-node tables");
		CheckEqInt(iDescriptors, 50, "DP's 26 tables contain 50 descriptors");
		CheckEqInt(iInputs, 36, "DP descriptor inventory has 36 INPUT pins");
		CheckEqInt(iOutputs, 10, "DP descriptor inventory has 10 OUTPUT pins");
		CheckEqInt(iReadWrites, 4, "DP descriptor inventory has 4 READWRITE pins");

#define DP_CHECK_PIN_INDEX(NodeType, PinName) \
		CheckEqInt(static_cast<int>(NodeType::GetPinTableStatic().FindPinIndex(#PinName)), static_cast<int>(NodeType::uPIN_##PinName), #NodeType "." #PinName " uPIN matches its table index")
		DP_CHECK_PIN_INDEX(DPNode_ReadHeldObjective, Villager); DP_CHECK_PIN_INDEX(DPNode_ReadHeldObjective, Tag);
		DP_CHECK_PIN_INDEX(DPNode_WinCheckAlreadyCollected, Tag); DP_CHECK_PIN_INDEX(DPNode_WinNotifyCollected, Villager); DP_CHECK_PIN_INDEX(DPNode_WinNotifyCollected, Tag);
		DP_CHECK_PIN_INDEX(DPNode_ConsumeHeldItem, Villager); DP_CHECK_PIN_INDEX(DPNode_DispatchObjectivePlaced, Villager); DP_CHECK_PIN_INDEX(DPNode_DispatchObjectivePlaced, Tag);
		DP_CHECK_PIN_INDEX(DPNode_ConsumeKeyForUnlock, Villager); DP_CHECK_PIN_INDEX(DPNode_DispatchDoorOpened, Villager); DP_CHECK_PIN_INDEX(DPNode_DispatchDoorClosed, Villager);
		DP_CHECK_PIN_INDEX(DPNode_AnimateDoorLeaves, IsOpen); DP_CHECK_PIN_INDEX(DPNode_DispatchChestOpened, Villager);
		DP_CHECK_PIN_INDEX(DPNode_DoorCheckKey, Villager); DP_CHECK_PIN_INDEX(DPNode_DoorPentagramDeferral, Villager); DP_CHECK_PIN_INDEX(DPNode_DoorAdvanceAnim, Anim); DP_CHECK_PIN_INDEX(DPNode_DoorAdvanceAnim, SettledAnim);
		DP_CHECK_PIN_INDEX(DPNode_VillagerEmitFootstep, WalkQuiet); DP_CHECK_PIN_INDEX(DPNode_VillagerEmitFootstep, Loudness); DP_CHECK_PIN_INDEX(DPNode_VillagerEmitFootstep, Radius); DP_CHECK_PIN_INDEX(DPNode_VillagerEmitFootstep, QuietMult);
		DP_CHECK_PIN_INDEX(DPNode_PickVillagerUnderCursor, Result); DP_CHECK_PIN_INDEX(DPNode_TryPossess, Villager);
		DP_CHECK_PIN_INDEX(DPNode_ForgeCraft, Villager); DP_CHECK_PIN_INDEX(DPNode_ForgeCraft, RecipeInput); DP_CHECK_PIN_INDEX(DPNode_ForgeCraft, RecipeOutput);
		DP_CHECK_PIN_INDEX(DPNode_ReadTuningFloat, Result); DP_CHECK_PIN_INDEX(DPNode_ItemChildRefusal, Villager); DP_CHECK_PIN_INDEX(DPNode_ItemChildRefusal, Tag);
		DP_CHECK_PIN_INDEX(DPNode_ItemArmChannel, Villager); DP_CHECK_PIN_INDEX(DPNode_ItemArmChannel, ChannelVillager); DP_CHECK_PIN_INDEX(DPNode_ItemArmChannel, ChannelDuration); DP_CHECK_PIN_INDEX(DPNode_ItemArmChannel, ChannelRemaining);
		DP_CHECK_PIN_INDEX(DPNode_ItemCommitPickup, Villager); DP_CHECK_PIN_INDEX(DPNode_ItemCommitPickup, ChannelVillager); DP_CHECK_PIN_INDEX(DPNode_ItemCommitPickup, ChannelRemaining); DP_CHECK_PIN_INDEX(DPNode_ItemCommitPickup, CommittedVillager);
		DP_CHECK_PIN_INDEX(DPNode_ItemFinishPickup, Villager); DP_CHECK_PIN_INDEX(DPNode_ItemFinishPickup, Tag);
		DP_CHECK_PIN_INDEX(DPNode_ItemRingBell, Villager); DP_CHECK_PIN_INDEX(DPNode_ItemRingBell, SpecialBehaviour); DP_CHECK_PIN_INDEX(DPNode_ItemEvaporate, Tag);
		DP_CHECK_PIN_INDEX(DPNode_PriestPickPatrolTarget, SuspicionRadius); DP_CHECK_PIN_INDEX(DPNode_PriestPickPatrolTarget, HighScentTarget); DP_CHECK_PIN_INDEX(DPNode_PriestPickPatrolTarget, PatrolTarget);
		DP_CHECK_PIN_INDEX(DPNode_PriestApprehendChannel, TargetWithDevil);
#undef DP_CHECK_PIN_INDEX
	}

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
		{
			Zenith_GraphPinTotalityRegistryGuard xGuard;
			Zenith_GraphPinTotality_SwapToRegistrar(&DP_RegisterGraphNodes);
			CheckEqInt(static_cast<int>(Zenith_GraphNodeRegistry::Get().GetTypeCount()), 38,
				"DP isolated registrar produces exactly 38 node types");
		}
		CheckPinInventory();

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
