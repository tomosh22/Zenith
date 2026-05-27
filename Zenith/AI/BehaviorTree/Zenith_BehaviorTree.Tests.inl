#include "UnitTests/Zenith_UnitTests.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/BehaviorTree/Zenith_BTNode.h"
#include "AI/BehaviorTree/Zenith_BTComposites.h"
#include "AI/BehaviorTree/Zenith_BTDecorators.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"

// ============================================================================
// Helper: Mock BT Node for testing
// ============================================================================
class MockBTNode : public Zenith_BTNode
{
public:
	MockBTNode(BTNodeStatus eReturnStatus) : m_eReturnStatus(eReturnStatus), m_uExecuteCount(0) {}

	BTNodeStatus Execute(Zenith_Entity& /*xAgent*/, Zenith_Blackboard& /*xBlackboard*/, float /*fDt*/) override
	{
		m_uExecuteCount++;
		return m_eReturnStatus;
	}

	const char* GetTypeName() const override { return "MockBTNode"; }

	BTNodeStatus m_eReturnStatus;
	uint32_t m_uExecuteCount;
};

// ============================================================================
// Behavior Tree Tests
// ============================================================================
ZENITH_TEST(AI, BTSequenceAllSuccess) { Zenith_UnitTests::TestBTSequenceAllSuccess(); }
void Zenith_UnitTests::TestBTSequenceAllSuccess(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTSequence xSequence;
	xSequence.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xSequence.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xSequence.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));

	BTNodeStatus eResult = xSequence.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::SUCCESS, "Sequence with all SUCCESS should return SUCCESS");

}

ZENITH_TEST(AI, BTSequenceFirstFails) { Zenith_UnitTests::TestBTSequenceFirstFails(); }

void Zenith_UnitTests::TestBTSequenceFirstFails(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	MockBTNode* pxSecond = new MockBTNode(BTNodeStatus::SUCCESS);

	Zenith_BTSequence xSequence;
	xSequence.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xSequence.AddChild(pxSecond);

	BTNodeStatus eResult = xSequence.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::FAILURE, "Sequence should fail on first failure");
	ZENITH_ASSERT_EQ(pxSecond->m_uExecuteCount, 0, "Second node should not execute");

}

ZENITH_TEST(AI, BTSequenceRunning) { Zenith_UnitTests::TestBTSequenceRunning(); }

void Zenith_UnitTests::TestBTSequenceRunning(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTSequence xSequence;
	xSequence.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xSequence.AddChild(new MockBTNode(BTNodeStatus::RUNNING));
	xSequence.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));

	BTNodeStatus eResult = xSequence.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::RUNNING, "Sequence should return RUNNING");

}

ZENITH_TEST(AI, BTSelectorFirstSucceeds) { Zenith_UnitTests::TestBTSelectorFirstSucceeds(); }

void Zenith_UnitTests::TestBTSelectorFirstSucceeds(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	MockBTNode* pxSecond = new MockBTNode(BTNodeStatus::SUCCESS);

	Zenith_BTSelector xSelector;
	xSelector.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xSelector.AddChild(pxSecond);

	BTNodeStatus eResult = xSelector.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::SUCCESS, "Selector should succeed on first success");
	ZENITH_ASSERT_EQ(pxSecond->m_uExecuteCount, 0, "Second node should not execute");

}

ZENITH_TEST(AI, BTSelectorAllFail) { Zenith_UnitTests::TestBTSelectorAllFail(); }

void Zenith_UnitTests::TestBTSelectorAllFail(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTSelector xSelector;
	xSelector.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xSelector.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xSelector.AddChild(new MockBTNode(BTNodeStatus::FAILURE));

	BTNodeStatus eResult = xSelector.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::FAILURE, "Selector with all FAILURE should return FAILURE");

}

ZENITH_TEST(AI, BTSelectorRunning) { Zenith_UnitTests::TestBTSelectorRunning(); }

void Zenith_UnitTests::TestBTSelectorRunning(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTSelector xSelector;
	xSelector.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xSelector.AddChild(new MockBTNode(BTNodeStatus::RUNNING));

	BTNodeStatus eResult = xSelector.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::RUNNING, "Selector should return RUNNING");

}

ZENITH_TEST(AI, BTParallelRequireOne) { Zenith_UnitTests::TestBTParallelRequireOne(); }

void Zenith_UnitTests::TestBTParallelRequireOne(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTParallel xParallel(Zenith_BTParallel::Policy::REQUIRE_ONE, Zenith_BTParallel::Policy::REQUIRE_ALL); // Require 1 success, fail on all failures
	xParallel.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::FAILURE));

	BTNodeStatus eResult = xParallel.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::SUCCESS, "Parallel requiring 1 should succeed");

}

ZENITH_TEST(AI, BTParallelRequireAll) { Zenith_UnitTests::TestBTParallelRequireAll(); }

void Zenith_UnitTests::TestBTParallelRequireAll(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTParallel xParallel(Zenith_BTParallel::Policy::REQUIRE_ALL, Zenith_BTParallel::Policy::REQUIRE_ONE); // Require all, fail on 1
	xParallel.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::FAILURE));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));

	BTNodeStatus eResult = xParallel.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::FAILURE, "Parallel requiring all should fail on one failure");

}

ZENITH_TEST(AI, BTParallelAllRunning) { Zenith_UnitTests::TestBTParallelAllRunning(); }

void Zenith_UnitTests::TestBTParallelAllRunning(){
	// Every child returns RUNNING → neither policy met → Parallel returns RUNNING
	// (rather than the default-to-FAILURE fallback for "all complete").
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTParallel xParallel(Zenith_BTParallel::Policy::REQUIRE_ONE, Zenith_BTParallel::Policy::REQUIRE_ONE);
	xParallel.AddChild(new MockBTNode(BTNodeStatus::RUNNING));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::RUNNING));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::RUNNING));

	BTNodeStatus eResult = xParallel.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::RUNNING, "All-running Parallel should return RUNNING");

}

ZENITH_TEST(AI, BTParallelNeitherPolicyMet) { Zenith_UnitTests::TestBTParallelNeitherPolicyMet(); }

void Zenith_UnitTests::TestBTParallelNeitherPolicyMet(){
	// REQUIRE_ALL / REQUIRE_ALL with a mix of SUCCESS and FAILURE: neither
	// policy is satisfied and no children are still running, so Parallel must
	// fall through to the default-FAILURE case (was an edge case before the
	// refactor; the extracted SuccessPolicyMet/FailurePolicyMet predicates
	// preserve it).
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTParallel xParallel(Zenith_BTParallel::Policy::REQUIRE_ALL, Zenith_BTParallel::Policy::REQUIRE_ALL);
	xParallel.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::FAILURE));

	BTNodeStatus eResult = xParallel.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::FAILURE, "Parallel with neither REQUIRE_ALL policy met should default to FAILURE");

}

ZENITH_TEST(AI, BTParallelRequireOneAbortsRunning) { Zenith_UnitTests::TestBTParallelRequireOneAbortsRunning(); }

void Zenith_UnitTests::TestBTParallelRequireOneAbortsRunning(){
	// REQUIRE_ONE success should abort any remaining RUNNING children so the
	// scheduler doesn't keep ticking them on the next frame.
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTParallel xParallel(Zenith_BTParallel::Policy::REQUIRE_ONE, Zenith_BTParallel::Policy::REQUIRE_ALL);
	MockBTNode* pxRunning = new MockBTNode(BTNodeStatus::RUNNING);
	xParallel.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xParallel.AddChild(pxRunning);

	const uint32_t uRunsBefore = pxRunning->m_uExecuteCount;
	BTNodeStatus eResult = xParallel.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::SUCCESS, "Parallel REQUIRE_ONE should succeed once one child succeeds");

	// AbortRunningChildren calls OnAbort, NOT Execute, so the RUNNING child's
	// m_uExecuteCount only reflects the one tick it got before the policy fired.
	ZENITH_ASSERT_EQ(pxRunning->m_uExecuteCount, uRunsBefore + 1, "Aborted running child should have ticked exactly once this frame");

}

ZENITH_TEST(AI, BTParallelRequireOneSuccessWinsOverSimultaneousFailure) { Zenith_UnitTests::TestBTParallelRequireOneSuccessWinsOverSimultaneousFailure(); }

void Zenith_UnitTests::TestBTParallelRequireOneSuccessWinsOverSimultaneousFailure(){
	// Regression guard for the BTComposites helper-extraction refactor.
	// When both success and failure policies are REQUIRE_ONE and a single tick
	// produces both a SUCCESS and a FAILURE child, the node must return SUCCESS
	// (success check runs first in Execute). The refactor preserved this order
	// by splitting TickChildrenAndTally → SuccessPolicyMet → FailurePolicyMet,
	// but no existing test pinned the simultaneous-mixed-outcome case.
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTParallel xParallel(Zenith_BTParallel::Policy::REQUIRE_ONE, Zenith_BTParallel::Policy::REQUIRE_ONE);
	xParallel.AddChild(new MockBTNode(BTNodeStatus::SUCCESS));
	xParallel.AddChild(new MockBTNode(BTNodeStatus::FAILURE));

	BTNodeStatus eResult = xParallel.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::SUCCESS, "REQUIRE_ONE+REQUIRE_ONE with simultaneous SUCCESS and FAILURE must resolve to SUCCESS "
		"(success policy is checked first).");

}

ZENITH_TEST(AI, BTInverter) { Zenith_UnitTests::TestBTInverter(); }

void Zenith_UnitTests::TestBTInverter(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	// Test inverting SUCCESS
	Zenith_BTInverter xInverterSuccess;
	xInverterSuccess.SetChild(new MockBTNode(BTNodeStatus::SUCCESS));
	BTNodeStatus eResult1 = xInverterSuccess.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult1, BTNodeStatus::FAILURE, "Inverter should convert SUCCESS to FAILURE");

	// Test inverting FAILURE
	Zenith_BTInverter xInverterFail;
	xInverterFail.SetChild(new MockBTNode(BTNodeStatus::FAILURE));
	BTNodeStatus eResult2 = xInverterFail.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult2, BTNodeStatus::SUCCESS, "Inverter should convert FAILURE to SUCCESS");

	// Test RUNNING passthrough
	Zenith_BTInverter xInverterRunning;
	xInverterRunning.SetChild(new MockBTNode(BTNodeStatus::RUNNING));
	BTNodeStatus eResult3 = xInverterRunning.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult3, BTNodeStatus::RUNNING, "Inverter should pass through RUNNING");

}

ZENITH_TEST(AI, BTRepeaterCount) { Zenith_UnitTests::TestBTRepeaterCount(); }

void Zenith_UnitTests::TestBTRepeaterCount(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	MockBTNode* pxChild = new MockBTNode(BTNodeStatus::SUCCESS);

	Zenith_BTRepeater xRepeater(3); // Repeat 3 times
	xRepeater.SetChild(pxChild);

	// Execute once - should return RUNNING (not done yet)
	BTNodeStatus eResult = xRepeater.Execute(xAgent, xBlackboard, 0.016f);
	// After 3 calls, it should succeed
	eResult = xRepeater.Execute(xAgent, xBlackboard, 0.016f);
	eResult = xRepeater.Execute(xAgent, xBlackboard, 0.016f);

	ZENITH_ASSERT_EQ(pxChild->m_uExecuteCount, 3, "Child should execute 3 times");

}

ZENITH_TEST(AI, BTCooldown) { Zenith_UnitTests::TestBTCooldown(); }

void Zenith_UnitTests::TestBTCooldown(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	MockBTNode* pxChild = new MockBTNode(BTNodeStatus::SUCCESS);

	Zenith_BTCooldown xCooldown(1.0f); // 1 second cooldown
	xCooldown.SetChild(pxChild);

	// First execution should succeed
	BTNodeStatus eResult1 = xCooldown.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult1, BTNodeStatus::SUCCESS, "First execution should succeed");

	// Immediate second execution should fail (on cooldown)
	BTNodeStatus eResult2 = xCooldown.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult2, BTNodeStatus::FAILURE, "Should fail during cooldown");

}

ZENITH_TEST(AI, BTSucceeder) { Zenith_UnitTests::TestBTSucceeder(); }

void Zenith_UnitTests::TestBTSucceeder(){
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);
	Zenith_Entity xAgent(pxSceneData, "TestAgent");
	Zenith_Blackboard xBlackboard;

	Zenith_BTSucceeder xSucceeder;
	xSucceeder.SetChild(new MockBTNode(BTNodeStatus::FAILURE));

	BTNodeStatus eResult = xSucceeder.Execute(xAgent, xBlackboard, 0.016f);
	ZENITH_ASSERT_EQ(eResult, BTNodeStatus::SUCCESS, "Succeeder should always return SUCCESS");

}

// ============================================================================
// Additional Bug Fix Verification Tests
// ============================================================================

ZENITH_TEST(AI, BTNodeOwnership) { Zenith_UnitTests::TestBTNodeOwnership(); }

void Zenith_UnitTests::TestBTNodeOwnership(){
	// Test that BT nodes properly track parent ownership
	// This verifies the fix for potential double-delete issues

	Zenith_BTSequence xSequence;
	MockBTNode* pxChild = new MockBTNode(BTNodeStatus::SUCCESS);

	// Node should not have parent initially
	ZENITH_ASSERT_FALSE(pxChild->HasParent(), "Node should not have parent initially");

	// Add to sequence
	xSequence.AddChild(pxChild);

	// Node should now have parent
	ZENITH_ASSERT_TRUE(pxChild->HasParent(), "Node should have parent after AddChild");

	// Create a decorator and test SetChild
	Zenith_BTInverter xInverter;
	MockBTNode* pxChild2 = new MockBTNode(BTNodeStatus::SUCCESS);

	ZENITH_ASSERT_FALSE(pxChild2->HasParent(), "Second node should not have parent initially");

	xInverter.SetChild(pxChild2);

	ZENITH_ASSERT_TRUE(pxChild2->HasParent(), "Second node should have parent after SetChild");

}

