#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPPentagram_Behaviour.h"
#include "Components/DPDoor_Behaviour.h"
#include "Components/DPChest_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DummyNoiseMachine_Behaviour.h"
#include "Components/DPHUDController_Behaviour.h"

// ============================================================================
// GameLevelScene_Test
//
// Loads scene index 1 (L_GameLevel) from inside Setup, gives the scene-load
// pipeline a few frames to settle, then enumerates the active scene to prove
// every behaviour authored in Project_RegisterEditorAutomationSteps is
// actually instantiated. This is the closest thing to an end-to-end M2
// verification that doesn't need real assets.
//
// Phases (driven by iFrame in Step):
//    0       — request LoadSceneByIndex(1, SINGLE).
//    1..30   — wait for IsLoaded + Awake settled. Bail at 30 if not.
//    >=31    — run script-presence assertions, then return false.
// ============================================================================

namespace
{
	bool g_bTriggered          = false;
	bool g_bSceneSwapped       = false;
	bool g_bAssertionsRan      = false;
	bool g_bAllPassed          = true;

	int  g_iVillagerCount      = 0;
	int  g_iPriestCount        = 0;
	int  g_iPentagramCount     = 0;
	int  g_iDoorCount          = 0;
	int  g_iChestCount         = 0;
	int  g_iItemCount          = 0;
	int  g_iNoiseMachineCount  = 0;
	int  g_iHudControllerCount = 0;
}

static void Setup_GameLevelScene()
{
	g_bTriggered          = false;
	g_bSceneSwapped       = false;
	g_bAssertionsRan      = false;
	g_bAllPassed          = true;
	g_iVillagerCount      = 0;
	g_iPriestCount        = 0;
	g_iPentagramCount     = 0;
	g_iDoorCount          = 0;
	g_iChestCount         = 0;
	g_iItemCount          = 0;
	g_iNoiseMachineCount  = 0;
	g_iHudControllerCount = 0;
}

static bool Step_GameLevelScene(int iFrame)
{
	if (iFrame == 0)
	{
		// Kick off scene load. Build index 1 = GameLevel (registered in
		// Project_LoadInitialScene).
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_bTriggered = true;
		return true;
	}

	// Wait until the active scene reports a valid handle that ISN'T the
	// FrontEnd scene anymore (build index 0 → 1).
	if (!g_bSceneSwapped)
	{
		Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActive);
		if (pxSceneData != nullptr)
		{
			// Rough heuristic: GameLevel has GameManager + a handful of
			// gameplay entities. Wait until a script behaviour we expect in
			// GameLevel actually shows up.
			int iFound = 0;
			DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
				[&iFound](Zenith_EntityID, DPVillager_Behaviour&) { ++iFound; });
			if (iFound > 0) g_bSceneSwapped = true;
		}
		if (iFrame > 30 && !g_bSceneSwapped)
		{
			g_bAllPassed = false;
			return false; // give up
		}
		if (!g_bSceneSwapped) return true;
	}

	if (!g_bAssertionsRan)
	{
		// Tally script behaviours present in the active scene.
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[](Zenith_EntityID, DPVillager_Behaviour&) { ++g_iVillagerCount; });
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[](Zenith_EntityID, Priest_Behaviour&) { ++g_iPriestCount; });
		DP_Query::ForEachScriptInActiveScene<DPPentagram_Behaviour>(
			[](Zenith_EntityID, DPPentagram_Behaviour&) { ++g_iPentagramCount; });
		DP_Query::ForEachScriptInActiveScene<DPDoor_Behaviour>(
			[](Zenith_EntityID, DPDoor_Behaviour&) { ++g_iDoorCount; });
		DP_Query::ForEachScriptInActiveScene<DPChest_Behaviour>(
			[](Zenith_EntityID, DPChest_Behaviour&) { ++g_iChestCount; });
		DP_Query::ForEachScriptInActiveScene<DPItemBase_Behaviour>(
			[](Zenith_EntityID, DPItemBase_Behaviour&) { ++g_iItemCount; });
		DP_Query::ForEachScriptInActiveScene<DummyNoiseMachine_Behaviour>(
			[](Zenith_EntityID, DummyNoiseMachine_Behaviour&) { ++g_iNoiseMachineCount; });
		DP_Query::ForEachScriptInActiveScene<DPHUDController_Behaviour>(
			[](Zenith_EntityID, DPHUDController_Behaviour&) { ++g_iHudControllerCount; });

		g_bAssertionsRan = true;
		return false; // we're done
	}

	return false;
}

static bool Verify_GameLevelScene()
{
	// Counts mirror DP_LevelData (auto-generated from L_GameLevel.json).
	// If you re-run Tools/dp_export/sweep.py + Tools/dp_import/generate_level_data.py,
	// these constants must be updated to match the regenerated header.
	if (!g_bTriggered)            return false;
	if (!g_bSceneSwapped)         return false;
	if (!g_bAssertionsRan)        return false;
	if (!g_bAllPassed)            return false;

	if (g_iVillagerCount      != 17) return false;   // DP_LevelData::kVillagerCount
	if (g_iPriestCount        != 1)  return false;
	if (g_iPentagramCount     != 1)  return false;   // authored explicitly
	// 15 doors come from DP_LevelData::kDoorCount (UE source, all stacked at
	// origin); + 1 TestDoor authored alongside the Forge for HumanPlaythrough.
	if (g_iDoorCount          != 16) return false;
	if (g_iChestCount         != 6)  return false;   // DP_LevelData::kChestCount
	// 15 items spawned at runtime by DPItemManager_Behaviour::OnStart, one per
	// DPItemSpawn_Behaviour. Was previously 1 hand-authored objective; manager
	// now drives spawning.
	if (g_iItemCount          != 15) return false;   // DP_LevelData::kItemSpawnCount
	if (g_iNoiseMachineCount  != 1)  return false;
	if (g_iHudControllerCount != 1)  return false;
	return true;
}

static const Zenith_AutomatedTest g_xGameLevelSceneTest = {
	"GameLevelScene_Test",
	&Setup_GameLevelScene,
	&Step_GameLevelScene,
	&Verify_GameLevelScene,
	240 // 4 seconds at 60 Hz — generous so cold-load doesn't timeout
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xGameLevelSceneTest);

#endif // ZENITH_INPUT_SIMULATOR
