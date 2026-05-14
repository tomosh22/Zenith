#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P2Fog_LightAddsHole (MVP-2.4.4)
//
// Pins the "lights contribute fog holes" half of the GDD §4.6 fog
// contract. After DPFogPass_Behaviour::OnUpdate runs, the registered
// fog hole count equals (villager count) + (light count) -- no more,
// no less. Counts are taken from the active scene.
//
// Procedure:
//   1. Load GameLevel.
//   2. Tick a few frames so DPFogPass_Behaviour::OnUpdate runs at
//      least once (rebuilds the table from scratch each frame).
//   3. Count villagers via DP_Query::ForEachScriptInActiveScene.
//   4. Count lights via the scene's Query<Zenith_LightComponent>.
//   5. Get DP_Fog::GetFogHoleCount().
//   6. Assert holeCount == villagerCount + lightCount.
//
// Why exact equality (not >=): excess holes mean something is being
// added that shouldn't be. The priest entity would show up here if
// `Test_P2Fog_AelfricNotRevealed`'s assertion failed -- but that
// test catches the priest case specifically. This test catches:
//   * A bug where the same entity gets registered twice in one frame
//     (loop without `if not already there` guard, OR the produced-
//     hole-table not getting cleared each frame).
//   * A future entity type (e.g., flames, traps) being added to the
//     rebuild loop without intent. Surface those decisions in PRs
//     rather than letting them silently inflate the count.
//
// What this does NOT pin (deferred to MVP-2.4.5 Memory Fog work):
//   * Hole RADIUS per producer. The current code uses 3 m for every
//     villager (possessed or not) and `light.Range + slop` for lights.
//     The GDD spec is 8m possessed / 1.5m un-possessed -- that's
//     MVP-2.4.3 follow-up work, not a regression guard.
// ============================================================================

namespace
{
	enum Phase : int { kFL_Start, kFL_WaitScene, kFL_Tick, kFL_Count,
	                   kFL_Verify, kFL_Done };

	int                     g_iPhase = kFL_Start;
	int                     g_iVillagerCount = 0;
	int                     g_iLightCount = 0;
	uint32_t                g_uHoleCount = 0;
	int                     g_iTickFrames = 0;

	constexpr int kTICK_FRAMES = 10;
}

static void Setup_P2FogLightAddsHole()
{
	g_iPhase = kFL_Start;
	g_iVillagerCount = 0;
	g_iLightCount = 0;
	g_uHoleCount = 0;
	g_iTickFrames = 0;
}

static bool Step_P2FogLightAddsHole(int iFrame)
{
	switch (g_iPhase)
	{
	case kFL_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kFL_WaitScene;
		return true;

	case kFL_WaitScene:
	{
		// Wait until at least one villager exists (proxy for "scene
		// fully OnAwoken").
		int iCount = 0;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&iCount](Zenith_EntityID, DPVillager_Behaviour&) { ++iCount; });
		if (iCount > 0)
		{
			g_iPhase = kFL_Tick;
			g_iTickFrames = 0;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kFL_Done;
		}
		return true;
	}

	case kFL_Tick:
		++g_iTickFrames;
		if (g_iTickFrames >= kTICK_FRAMES) g_iPhase = kFL_Count;
		return true;

	case kFL_Count:
	{
		// Count villagers and lights using the same loops DPFogPass
		// uses internally, so a mismatch means producers <-> consumer
		// disagree on what's in scope.
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[](Zenith_EntityID, DPVillager_Behaviour&) { ++g_iVillagerCount; });
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene != nullptr)
		{
			pxScene->Query<Zenith_LightComponent>().ForEach(
				[](Zenith_EntityID, Zenith_LightComponent&) { ++g_iLightCount; });
		}
		g_uHoleCount = DP_Fog::GetFogHoleCount();
		std::printf("[P2FogLightAddsHole] villagers=%d lights=%d holes=%u (expected %d)\n",
			g_iVillagerCount, g_iLightCount, g_uHoleCount,
			g_iVillagerCount + g_iLightCount);
		std::fflush(stdout);
		g_iPhase = kFL_Verify;
		return true;
	}

	case kFL_Verify:
		g_iPhase = kFL_Done;
		return false;

	case kFL_Done:
	default:
		return false;
	}
}

static bool Verify_P2FogLightAddsHole()
{
	if (g_iVillagerCount == 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2FogLightAddsHole: no villagers iterated -- scene precondition failed");
		return false;
	}
	if (g_iLightCount == 0)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2FogLightAddsHole: no lights iterated in GameLevel -- the test premise (lights contribute holes) can't be verified without at least one light. Check scene authoring");
		return false;
	}
	const uint32_t uExpected =
		static_cast<uint32_t>(g_iVillagerCount + g_iLightCount);
	if (g_uHoleCount != uExpected)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2FogLightAddsHole: GetFogHoleCount=%u, expected %u (villagers=%d + lights=%d). Excess holes implies an unintended producer (e.g., priest leaked in); fewer holes implies a producer dropped entries (e.g., light loop bailed early)",
			g_uHoleCount, uExpected, g_iVillagerCount, g_iLightCount);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2FogLightAddsHoleTest = {
	"Test_P2Fog_LightAddsHole",
	&Setup_P2FogLightAddsHole,
	&Step_P2FogLightAddsHole,
	&Verify_P2FogLightAddsHole,
	120
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2FogLightAddsHoleTest);

#endif // ZENITH_INPUT_SIMULATOR
