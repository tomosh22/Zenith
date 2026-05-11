#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPItemSpawn_Behaviour.h"
#include "Components/DPItemManager_Behaviour.h"
#include "Components/DPDoor_Behaviour.h"
#include "Components/DPDoubleDoor_Behaviour.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DummyNoiseMachine_Behaviour.h"

// ============================================================================
// GymScenes_Test — exercises the four authored gym scenes (build indices 2-5).
//
// For each gym in turn:
//   1. LoadSceneByIndex(N, SINGLE).
//   2. Wait up to 30 frames for the scene to swap.
//   3. Tally the mechanic-specific behaviour count and assert >= expected
//      minimum (counts mirror the AddStep_AttachScript calls in
//      DevilsPlayground.cpp::AuthorGym*).
//
// Running all four sequentially in one test (instead of four separate tests)
// keeps suite runtime down: each scene swap cleanup is ~1s vs 25s of per-test
// boot. The batch harness's BetweenTests phase still resets state for the
// next batched test run.
// ============================================================================

namespace
{
	enum Phase : int {
		kStart,
		kWaitItems,    kVerifyItems,
		kLoadNoise,    kWaitNoise,    kVerifyNoise,
		kLoadDoors,    kWaitDoors,    kVerifyDoors,
		kLoadForge,    kWaitForge,    kVerifyForge,
		kDone
	};

	int g_iPhase = kStart;
	int g_iWaitFrames = 0;

	int g_iItemsVillagers   = 0;
	int g_iItemsSpawners    = 0;
	int g_iItemsManagers    = 0;

	int g_iNoiseVillagers   = 0;
	int g_iNoisePriests     = 0;
	int g_iNoiseMachines    = 0;

	int g_iDoorsVillagers   = 0;
	int g_iDoorsSingles     = 0;
	int g_iDoorsDoubles     = 0;

	int g_iForgeVillagers   = 0;
	int g_iForgeForges      = 0;
	int g_iForgeSpawners    = 0;

	bool g_bAllScenesLoaded = false;

	template<typename T>
	int CountScripts()
	{
		int iCount = 0;
		DP_Query::ForEachScriptInActiveScene<T>(
			[&iCount](Zenith_EntityID, T&) { ++iCount; });
		return iCount;
	}
}

static void Setup_GymScenes()
{
	g_iPhase = kStart;
	g_iWaitFrames = 0;

	g_iItemsVillagers = g_iItemsSpawners = g_iItemsManagers = 0;
	g_iNoiseVillagers = g_iNoisePriests  = g_iNoiseMachines = 0;
	g_iDoorsVillagers = g_iDoorsSingles  = g_iDoorsDoubles  = 0;
	g_iForgeVillagers = g_iForgeForges   = g_iForgeSpawners = 0;

	g_bAllScenesLoaded = false;
}

// Helper: trigger a scene load + transition to "wait" sub-phase.
static bool BeginLoadAndWait(int iBuildIndex, Phase eWaitPhase)
{
	Zenith_SceneManager::LoadSceneByIndex(iBuildIndex, SCENE_LOAD_SINGLE);
	g_iWaitFrames = 0;
	g_iPhase = eWaitPhase;
	return true;
}

// Helper: poll the active scene; transition to verify sub-phase once a
// villager appears (every gym scene has one). Bail out after 30 frames.
static bool WaitForVillager(Phase eVerifyPhase)
{
	++g_iWaitFrames;
	int iVillagers = CountScripts<DPVillager_Behaviour>();
	if (iVillagers > 0)
	{
		g_iPhase = eVerifyPhase;
		return true;
	}
	if (g_iWaitFrames > 30)
	{
		// Scene never loaded — give up; Verify will see zero counts.
		g_iPhase = kDone;
		return false;
	}
	return true;
}

static bool Step_GymScenes(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	case kStart:
		return BeginLoadAndWait(2, kWaitItems);

	case kWaitItems:
		return WaitForVillager(kVerifyItems);

	case kVerifyItems:
	{
		g_iItemsVillagers = CountScripts<DPVillager_Behaviour>();
		g_iItemsSpawners  = CountScripts<DPItemSpawn_Behaviour>();
		g_iItemsManagers  = CountScripts<DPItemManager_Behaviour>();
		return BeginLoadAndWait(3, kWaitNoise);
	}

	case kWaitNoise:
		return WaitForVillager(kVerifyNoise);

	case kVerifyNoise:
	{
		g_iNoiseVillagers = CountScripts<DPVillager_Behaviour>();
		g_iNoisePriests   = CountScripts<Priest_Behaviour>();
		g_iNoiseMachines  = CountScripts<DummyNoiseMachine_Behaviour>();
		return BeginLoadAndWait(4, kWaitDoors);
	}

	case kWaitDoors:
		return WaitForVillager(kVerifyDoors);

	case kVerifyDoors:
	{
		g_iDoorsVillagers = CountScripts<DPVillager_Behaviour>();
		g_iDoorsSingles   = CountScripts<DPDoor_Behaviour>();
		g_iDoorsDoubles   = CountScripts<DPDoubleDoor_Behaviour>();
		return BeginLoadAndWait(5, kWaitForge);
	}

	case kWaitForge:
		return WaitForVillager(kVerifyForge);

	case kVerifyForge:
	{
		g_iForgeVillagers = CountScripts<DPVillager_Behaviour>();
		g_iForgeForges    = CountScripts<DPForge_Behaviour>();
		g_iForgeSpawners  = CountScripts<DPItemSpawn_Behaviour>();
		g_bAllScenesLoaded = true;
		g_iPhase = kDone;
		return false;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_GymScenes()
{
	if (!g_bAllScenesLoaded)               return false;
	// Items: 1 villager, 6 spawners, 1 manager
	if (g_iItemsVillagers   < 1)           return false;
	if (g_iItemsSpawners    != 6)          return false;
	if (g_iItemsManagers    != 1)          return false;
	// Noise: 1 villager, 1 priest, 3 noise machines
	if (g_iNoiseVillagers   < 1)           return false;
	if (g_iNoisePriests     != 1)          return false;
	if (g_iNoiseMachines    != 3)          return false;
	// Doors: 1 villager, 1 single door, 1 double door
	if (g_iDoorsVillagers   < 1)           return false;
	if (g_iDoorsSingles     != 1)          return false;
	if (g_iDoorsDoubles     != 1)          return false;
	// Forge: 1 villager, 1 forge, 9 spawners (5 objective + 4 iron)
	if (g_iForgeVillagers   < 1)           return false;
	if (g_iForgeForges      != 1)          return false;
	if (g_iForgeSpawners    != 9)          return false;
	return true;
}

static const Zenith_AutomatedTest g_xGymScenesTest = {
	"GymScenes_Test",
	&Setup_GymScenes,
	&Step_GymScenes,
	&Verify_GymScenes,
	600 // generous: 4 scene loads × ~30 frames + verify
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xGymScenesTest);

#endif // ZENITH_INPUT_SIMULATOR
