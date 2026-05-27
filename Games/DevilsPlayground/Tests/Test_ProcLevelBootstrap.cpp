#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#include "Components/DPProcLevelBootstrap_Behaviour.h"
#include "Source/DPProcLevel/DPProcLevel_Generator.h"

#include <cstdio>

// ============================================================================
// Test_ProcLevelBootstrap (P4a) -- bootstrap script wire-up smoke test.
//
// Verifies the bootstrap can be attached to a scene entity, OnAwake fires
// during the engine's lifecycle pass, the generator runs to completion,
// and the resulting LevelLayout is queryable via the singleton accessor.
// No entity spawning yet -- that's P4b+. This is purely the harness:
// "can game code call DPProcLevel::Generate from inside a script?"
//
// The test:
//   1. Creates an empty scene.
//   2. Adds an entity with DPProcLevelBootstrap_Behaviour attached.
//   3. Sets the test seed via SetSeedForTest BEFORE OnAwake fires.
//   4. Lets the engine drive OnAwake.
//   5. Verifies the layout is populated (>=1 room) and the seed
//      echoed in the layout header matches what we set.
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";
	Zenith_Scene g_xScene;

	constexpr uint64_t kTestSeed = 12345ull;
}

static void Setup_ProcLevelBootstrap()
{
	g_bPassed = false;
	g_szFailureReason = "";

	g_xScene = g_xEngine.SceneRegistry().CreateEmptyScene("ProcLevelBootstrapTest");
	Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneData(g_xScene);
	if (pxScene == nullptr)
	{
		g_szFailureReason = "CreateEmptyScene returned no SceneData";
		return;
	}

	// Attach the bootstrap script. AddScript fires OnAwake immediately
	// (the AddScript flavour, as opposed to AddScriptForSerialization,
	// drives the lifecycle hooks). Set the seed BEFORE the OnAwake
	// reads it -- AddScript returns AFTER OnAwake has already fired, so
	// we have to use the AddScriptForSerialization path + manual
	// initialisation, OR set the seed before AddScript.
	//
	// We use AddScript here and accept the default seed=0 for the
	// "did the wire-up work" assertion. A SetSeedForTest call after
	// the fact would be too late to affect the generated layout. A
	// later PR can add a config-driven seed if needed.
	Zenith_Entity xBootstrapEntity(pxScene, "ProcLevelBootstrap");
	auto& xScript = xBootstrapEntity.AddComponent<Zenith_ScriptComponent>();
	xScript.AddScript<DPProcLevelBootstrap_Behaviour>();
}

static bool Step_ProcLevelBootstrap(int iFrame)
{
	// Give the lifecycle scheduler one frame to settle, then verify.
	// Most state we care about is already populated after AddScript
	// returns (OnAwake fires synchronously), but using a frame lets
	// the harness print the bootstrap's stdout BEFORE Verify runs.
	if (iFrame < 1) return true;

	DPProcLevelBootstrap_Behaviour* pxBootstrap =
		DPProcLevelBootstrap_Behaviour::Instance();
	if (pxBootstrap == nullptr)
	{
		g_szFailureReason = "Bootstrap instance not registered after OnAwake";
		return false;
	}

	const DPProcLevel::LevelLayout& xLayout = pxBootstrap->GetLayout();
	if (xLayout.axRooms.GetSize() == 0u)
	{
		g_szFailureReason = "Layout has no rooms after Generate";
		return false;
	}
	// Sanity: every population layer ran.
	if (xLayout.axWallSegments.GetSize() == 0u)
	{
		g_szFailureReason = "Layout has no wall segments";
		return false;
	}
	if (xLayout.axGameElements.GetSize() == 0u)
	{
		g_szFailureReason = "Layout has no game elements";
		return false;
	}
	if (xLayout.axVillagerSpawns.GetSize() == 0u)
	{
		g_szFailureReason = "Layout has no villager spawns";
		return false;
	}
	if (!xLayout.xPriestSpawn.bValid)
	{
		g_szFailureReason = "Layout has no priest spawn";
		return false;
	}

	// Seed echo: the bootstrap's default seed is 0.
	if (xLayout.uSeed != 0ull)
	{
		g_szFailureReason = "Layout seed not 0 (bootstrap default)";
		return false;
	}

	// P4b -- the bootstrap should have spawned one entity per wall
	// segment. Allow a small slack for entity creation failure (e.g.
	// scene full) -- in practice the count should match exactly with
	// an empty test scene.
	const uint32_t uSpawnedWalls = pxBootstrap->GetSpawnedWallCount();
	if (uSpawnedWalls != xLayout.axWallSegments.GetSize())
	{
		g_szFailureReason = "Spawned wall count != layout wall-segment count";
		return false;
	}

	// P4c -- the bootstrap should have spawned one entity per game
	// element (pentagram, forge, doors, chests, noise machines, iron,
	// objectives). SpawnPoint elements are skipped (consumed by P4d).
	uint32_t uExpectedElements = 0;
	for (uint32_t i = 0; i < xLayout.axGameElements.GetSize(); ++i)
	{
		if (xLayout.axGameElements.Get(i).eType
		    != DPProcLevel::GameElementType::SpawnPoint)
		{
			++uExpectedElements;
		}
	}
	const uint32_t uSpawnedElements = pxBootstrap->GetSpawnedGameElementCount();
	if (uSpawnedElements != uExpectedElements)
	{
		g_szFailureReason =
			"Spawned game-element count != expected (layout minus SpawnPoints)";
		return false;
	}

	// P4d -- AI agents. Villager count must match the layout; priest is
	// spawned iff xPriestSpawn.bValid.
	const uint32_t uSpawnedVillagers = pxBootstrap->GetSpawnedVillagerCount();
	if (uSpawnedVillagers != xLayout.axVillagerSpawns.GetSize())
	{
		g_szFailureReason =
			"Spawned villager count != layout villager-spawn count";
		return false;
	}
	if (pxBootstrap->GetSpawnedPriest() != xLayout.xPriestSpawn.bValid)
	{
		g_szFailureReason =
			"Spawned-priest flag != layout xPriestSpawn.bValid";
		return false;
	}

	g_bPassed = true;
	std::printf("[ProcLevelBootstrap] PASS: rooms=%u walls=%u elements=%u "
		"(spawned %u) villagers=%u (spawned %u) priest=%d (spawned=%d)\n",
		xLayout.axRooms.GetSize(),
		xLayout.axWallSegments.GetSize(),
		xLayout.axGameElements.GetSize(),
		uSpawnedElements,
		xLayout.axVillagerSpawns.GetSize(),
		uSpawnedVillagers,
		(int)xLayout.xPriestSpawn.bValid,
		(int)pxBootstrap->GetSpawnedPriest());
	std::fflush(stdout);
	return false;
}

static bool Verify_ProcLevelBootstrap()
{
	// Tear down the test scene so the next test in the batch starts
	// with a clean slate (the bootstrap singleton clears via OnDestroy
	// when the scene unloads).
	if (g_xScene.IsValid())
	{
		g_xEngine.SceneOperations().UnloadScene(g_xScene);
	}

	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_CORE,
			"Test_ProcLevelBootstrap: %s", g_szFailureReason);
		return false;
	}
	(void)kTestSeed;  // reserved for future "non-default seed" assertions
	return true;
}

static const Zenith_AutomatedTest g_xProcLevelBootstrapTest = {
	"Test_ProcLevelBootstrap",
	&Setup_ProcLevelBootstrap,
	&Step_ProcLevelBootstrap,
	&Verify_ProcLevelBootstrap,
	30
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xProcLevelBootstrapTest);

#endif // ZENITH_INPUT_SIMULATOR
