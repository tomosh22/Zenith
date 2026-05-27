#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPProcLevelBootstrap_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DPPentagram_Behaviour.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/DPDoor_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_ProcLevelScene (P5) -- end-to-end scene load + bootstrap-spawn check.
//
// Loads the authored ProcLevel scene (build index 1 -- the only gameplay
// scene since 2026-05-19) and verifies that the bootstrap's OnAwake spawned
// a fully-populated level: walls + game elements + AI agents. Sits alongside
// Test_ProcLevelBootstrap (which is a synthetic "create empty scene + add
// bootstrap manually" smoke test) -- this test exercises the real load path
// used by the runtime + the personality playthrough tests.
//
// Pass criteria (seed 0, default GenConfig):
//   - Bootstrap singleton registered.
//   - Layout has >0 rooms / walls / elements / villager spawns / patrol /
//     valid priest spawn.
//   - At least 1 DPVillager_Behaviour script in the active scene.
//   - Exactly 1 Priest_Behaviour script.
//   - At least 1 DPPentagram_Behaviour + 1 DPForge_Behaviour.
//   - At least 1 DPItemBase script (iron + objectives).
// ============================================================================

namespace
{
	enum Phase : int { kLoading, kVerify, kDone };

	int g_iPhase = kLoading;
	int g_iWaitFrames = 0;

	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	template<typename T>
	int CountScripts()
	{
		int iCount = 0;
		DP_Query::ForEachScriptInActiveScene<T>(
			[&iCount](Zenith_EntityID, T&) { ++iCount; });
		return iCount;
	}
}

static void Setup_ProcLevelScene()
{
	g_iPhase = kLoading;
	g_iWaitFrames = 0;
	g_bPassed = false;
	g_szFailureReason = "";

	// Build index 1 is the authored ProcLevel scene (see
	// DevilsPlayground.cpp::Project_LoadInitialScene).
	g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
}

static bool Step_ProcLevelScene(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	case kLoading:
		// Give the scene swap + bootstrap OnAwake time to settle.
		// The bootstrap spawns ~75 entities and the navmesh generator
		// runs at the end -- 30 frames is generous but conservative.
		if (++g_iWaitFrames < 30) return true;
		g_iPhase = kVerify;
		return true;

	case kVerify:
	{
		DPProcLevelBootstrap_Behaviour* pxBootstrap =
			DPProcLevelBootstrap_Behaviour::Instance();
		if (pxBootstrap == nullptr)
		{
			g_szFailureReason = "Bootstrap singleton not set after scene load";
			g_iPhase = kDone;
			return false;
		}

		const DPProcLevel::LevelLayout& xLayout = pxBootstrap->GetLayout();
		if (xLayout.axRooms.GetSize() == 0u
		 || xLayout.axWallSegments.GetSize() == 0u
		 || xLayout.axGameElements.GetSize() == 0u
		 || xLayout.axVillagerSpawns.GetSize() == 0u
		 || !xLayout.xPriestSpawn.bValid)
		{
			g_szFailureReason = "Layout missing expected procgen output";
			g_iPhase = kDone;
			return false;
		}

		// Wall + game-element + villager + priest counts (already
		// covered by Test_ProcLevelBootstrap but verifying here too
		// in case the scene-load path diverges).
		if (pxBootstrap->GetSpawnedWallCount() != xLayout.axWallSegments.GetSize())
		{
			g_szFailureReason = "Wall entity count != layout WallSegment count";
			g_iPhase = kDone;
			return false;
		}
		if (pxBootstrap->GetSpawnedVillagerCount() != xLayout.axVillagerSpawns.GetSize())
		{
			g_szFailureReason = "Villager entity count != layout VillagerSpawn count";
			g_iPhase = kDone;
			return false;
		}
		if (!pxBootstrap->GetSpawnedPriest())
		{
			g_szFailureReason = "Priest entity not spawned";
			g_iPhase = kDone;
			return false;
		}

		// Cross-check the script-side query: the spawned entities have
		// their behaviours actually attached + registered with the
		// script component manager. Counting via DP_Query confirms the
		// AddScript<T>(...) calls in the bootstrap actually produced
		// queryable scripts (not just "entity exists with type name").
		const int iVillagers = CountScripts<DPVillager_Behaviour>();
		const int iPriests   = CountScripts<Priest_Behaviour>();
		const int iItems     = CountScripts<DPItemBase_Behaviour>();
		const int iPents     = CountScripts<DPPentagram_Behaviour>();
		const int iForges    = CountScripts<DPForge_Behaviour>();
		const int iDoors     = CountScripts<DPDoor_Behaviour>();

		if (iVillagers < 1) { g_szFailureReason = "No villager scripts"; g_iPhase = kDone; return false; }
		if (iPriests != 1)  { g_szFailureReason = "Priest script count != 1"; g_iPhase = kDone; return false; }
		if (iItems < 1)     { g_szFailureReason = "No DPItemBase scripts"; g_iPhase = kDone; return false; }
		if (iPents < 1)     { g_szFailureReason = "No pentagram script"; g_iPhase = kDone; return false; }
		if (iForges < 1)    { g_szFailureReason = "No forge script"; g_iPhase = kDone; return false; }
		// Doors are optional -- a level layout where the pentagram has
		// only one incident corridor still places at least 1 door
		// (door always on a gated corridor); current default seed has
		// >= 1 door but we don't hard-require it here for forward-compat.
		(void)iDoors;

		std::printf("[ProcLevelScene] PASS: rooms=%u walls=%u elements=%u "
			"villagers(spawned=%u, scripts=%d) priest(spawned=%d, scripts=%d) "
			"items=%d pentagrams=%d forges=%d doors=%d\n",
			xLayout.axRooms.GetSize(),
			xLayout.axWallSegments.GetSize(),
			xLayout.axGameElements.GetSize(),
			pxBootstrap->GetSpawnedVillagerCount(), iVillagers,
			(int)pxBootstrap->GetSpawnedPriest(), iPriests,
			iItems, iPents, iForges, iDoors);
		std::fflush(stdout);

		g_bPassed = true;
		g_iPhase = kDone;
		return false;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_ProcLevelScene()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_CORE,
			"Test_ProcLevelScene: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xProcLevelSceneTest = {
	"Test_ProcLevelScene",
	&Setup_ProcLevelScene,
	&Step_ProcLevelScene,
	&Verify_ProcLevelScene,
	120  // up to 120 frames -- scene load + bootstrap + verify
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xProcLevelSceneTest);

#endif // ZENITH_INPUT_SIMULATOR
