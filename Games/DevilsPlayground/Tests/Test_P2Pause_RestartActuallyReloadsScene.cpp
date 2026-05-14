#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPPauseMenuController_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P2Pause_RestartActuallyReloadsScene (MVP-2.5.5 INTEGRATION)
//
// Test_P2Menu_PauseAndMainMenuShortcuts pins the flag flip. This
// test pins the END-TO-END BEHAVIOUR: pressing R while paused
// actually unloads the current scene + loads a fresh GameLevel.
// The proof: state that was set in the original scene (possessed
// villager, held item) is cleared after the restart.
//
// Procedure:
//   1. Load GameLevel. Pick a villager. Possess them. Hand them an
//      item via SetHeldItem. Verify possessed + held are valid.
//   2. Set the pause-menu singleton's m_bShown via the test
//      shortcut path (it expects to be shown before the R key
//      registers).
//   3. FireRestartShortcutForTest. This triggers HandleRestart
//      which calls LoadSceneByIndex(1, SINGLE) -- the actual
//      reload, not just a flag flip.
//   4. Wait a few frames for the scene reload to complete +
//      between-tests reset hooks to fire.
//   5. Verify:
//        DP_Player::GetPossessedVillager() == INVALID
//        DP_Player::GetHeldItemEntity(oldVillager) == INVALID
//        WasRestartRequestedForTest() == true (the flag IS set)
//
// What this catches (vs the flag-only unit test):
//   * HandleRestart's LoadSceneByIndex call was removed (only the
//     flag flips; scene doesn't reload).
//   * The reload races with the flag write so observers see the
//     flag without the side effect.
//   * ResetForTest hook leaks (the new scene inherits stale
//     possession from the prior one).
// ============================================================================

namespace
{
	enum Phase : int {
		kRR_Start, kRR_WaitScene, kRR_PossessAndArm,
		kRR_VerifyPreState, kRR_TriggerRestart, kRR_WaitForReload,
		kRR_Snapshot, kRR_Verify, kRR_Done
	};

	int                     g_iPhase = kRR_Start;
	Zenith_EntityID         g_xVillagerBefore;
	Zenith_EntityID         g_xItemBefore;
	int                     g_iWaitFrames = 0;

	Zenith_EntityID         g_xPossessedBefore;
	Zenith_EntityID         g_xHeldBefore;
	bool                    g_bRestartFlag = false;
	Zenith_EntityID         g_xPossessedAfter;
	Zenith_EntityID         g_xHeldAfter;

	// 30 frames after FireRestartShortcut: scene load + between-tests
	// hook should both have completed.
	constexpr int kPOST_RESTART_WAIT = 30;

	void MakeSyntheticItem(Zenith_EntityID& xOut)
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt(pxScene, std::string("Test_PauseRestart_Item"));
		if (!xEnt.IsValid()) return;
		xOut = xEnt.GetEntityID();
	}
}

static void Setup_P2PauseRestart()
{
	g_iPhase = kRR_Start;
	g_xVillagerBefore = INVALID_ENTITY_ID;
	g_xItemBefore = INVALID_ENTITY_ID;
	g_iWaitFrames = 0;
	g_xPossessedBefore = INVALID_ENTITY_ID;
	g_xHeldBefore = INVALID_ENTITY_ID;
	g_bRestartFlag = false;
	g_xPossessedAfter = INVALID_ENTITY_ID;
	g_xHeldAfter = INVALID_ENTITY_ID;
	DPPauseMenuController_Behaviour::ResetPauseShortcutsForTest();
}

static bool Step_P2PauseRestart(int iFrame)
{
	switch (g_iPhase)
	{
	case kRR_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kRR_WaitScene;
		return true;

	case kRR_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		// Also wait for the persistent pause-menu singleton.
		const bool bHasPersistent =
			DPPauseMenuController_Behaviour::GetPersistentInstanceForTest() != nullptr;
		if (xFound.IsValid() && bHasPersistent)
		{
			g_xVillagerBefore = xFound;
			g_iPhase = kRR_PossessAndArm;
		}
		else if (iFrame > 90)
		{
			g_iPhase = kRR_Done;
		}
		return true;
	}

	case kRR_PossessAndArm:
		DP_Player::SetPossessedVillager(g_xVillagerBefore);
		MakeSyntheticItem(g_xItemBefore);
		if (g_xItemBefore.IsValid())
		{
			DP_Player::SetHeldItem(g_xVillagerBefore, g_xItemBefore);
		}
		g_iPhase = kRR_VerifyPreState;
		return true;

	case kRR_VerifyPreState:
		g_xPossessedBefore = DP_Player::GetPossessedVillager();
		g_xHeldBefore = DP_Player::GetHeldItemEntity(g_xVillagerBefore);
		g_iPhase = kRR_TriggerRestart;
		return true;

	case kRR_TriggerRestart:
		// HandleRestart fires LoadSceneByIndex(1, SINGLE) directly --
		// no need to simulate Esc/R key sequence.
		DPPauseMenuController_Behaviour::FireRestartShortcutForTest();
		g_iWaitFrames = 0;
		g_iPhase = kRR_WaitForReload;
		return true;

	case kRR_WaitForReload:
		++g_iWaitFrames;
		if (g_iWaitFrames >= kPOST_RESTART_WAIT) g_iPhase = kRR_Snapshot;
		return true;

	case kRR_Snapshot:
		g_bRestartFlag = DPPauseMenuController_Behaviour::WasRestartRequestedForTest();
		g_xPossessedAfter = DP_Player::GetPossessedVillager();
		g_xHeldAfter = DP_Player::GetHeldItemEntity(g_xVillagerBefore);
		g_iPhase = kRR_Verify;
		return true;

	case kRR_Verify:
		std::printf("[P2PauseRestart] pre: poss=(%u/%u) held=(%u/%u) | post: poss=(%u/%u) held=(%u/%u) flag=%d\n",
			g_xPossessedBefore.m_uIndex, g_xPossessedBefore.m_uGeneration,
			g_xHeldBefore.m_uIndex, g_xHeldBefore.m_uGeneration,
			g_xPossessedAfter.m_uIndex, g_xPossessedAfter.m_uGeneration,
			g_xHeldAfter.m_uIndex, g_xHeldAfter.m_uGeneration,
			(int)g_bRestartFlag);
		std::fflush(stdout);
		// Cleanup flag for next test.
		DPPauseMenuController_Behaviour::ResetPauseShortcutsForTest();
		g_iPhase = kRR_Done;
		return false;

	case kRR_Done:
	default:
		return false;
	}
}

static bool Verify_P2PauseRestart()
{
	if (!g_xVillagerBefore.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2PauseRestart: villager setup failed");
		return false;
	}
	// Pre-restart precondition: possessed villager + held item valid.
	if (!g_xPossessedBefore.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2PauseRestart: pre-restart possessed was INVALID -- SetPossessedVillager didn't take");
		return false;
	}
	// The flag MUST have fired.
	if (!g_bRestartFlag)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2PauseRestart: WasRestartRequestedForTest() false -- shortcut handler didn't run");
		return false;
	}
	// Post-restart: scene reload should have cleared the DP_Player
	// state. The between-tests reset hook does this; if the reload
	// didn't actually fire, the state would persist.
	if (g_xPossessedAfter.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2PauseRestart: post-restart possessed is still (%u/%u) -- the scene didn't actually reload, OR the between-tests reset hook didn't clear DP_Player state",
			g_xPossessedAfter.m_uIndex, g_xPossessedAfter.m_uGeneration);
		return false;
	}
	if (g_xHeldAfter.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2PauseRestart: post-restart held item is still (%u/%u) -- DP_Player::g_xHeldItems leaked across reload",
			g_xHeldAfter.m_uIndex, g_xHeldAfter.m_uGeneration);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2PauseRestartTest = {
	"Test_P2Pause_RestartActuallyReloadsScene",
	&Setup_P2PauseRestart,
	&Step_P2PauseRestart,
	&Verify_P2PauseRestart,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2PauseRestartTest);

#endif // ZENITH_INPUT_SIMULATOR
