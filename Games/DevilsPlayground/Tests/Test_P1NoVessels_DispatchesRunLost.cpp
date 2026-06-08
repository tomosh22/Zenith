#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Collections/Zenith_Vector.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P1NoVessels_DispatchesRunLost (MVP-1.3.5 -- NoVessels half)
//
// MVP-1.3.5 in the roadmap covers two run-loss causes: Dawn (Night
// timer expires) and NoVessels (every villager dead). Dawn requires
// the Phase 4 Night-timer system to exist; NoVessels can land
// standalone since it just observes the villager death stream.
//
// This test pins the NoVessels half:
//   "Every villager in the level has died and no fresh body is
//    available for possession" -> dispatch DP_OnRunLost{NoVessels}.
//
// Procedure:
//   1. Subscribe to DP_OnVillagerDied + DP_OnRunLost in Setup so the
//      per-death and run-end counts can be tallied without polling.
//   2. Load GameLevel.
//   3. Wait for the 17 DPVillager_Behaviour scripts to OnStart.
//   4. Iterate, collect every villager EntityID, then call Kill() on
//      each in turn. Kill() is the new MVP-1.3.5 method that runs
//      the same death-dispatch + NoVessels-scan that natural-cause
//      (TickLife) death uses.
//   5. Verify:
//        DP_OnVillagerDied fired exactly N times (N = villager count)
//        DP_OnRunLost{NoVessels} fired exactly ONCE
//        Specifically: NoVessels fired ON THE Nth death, NOT earlier
//        (a regression where the scan reports "0 alive" prematurely
//        would dispatch on death #1 or #2).
//
// What this catches:
//   * Kill() forgot to dispatch DP_OnRunLost (the scan code missing
//     entirely).
//   * The scan counts the just-died villager incorrectly (e.g., off-
//     by-one: dispatching when 1 villager remains alive).
//   * A dead-villager check that uses != 1.0 instead of > 0.0 (or
//     similar boundary slip).
//   * A subscriber-side regression that double-counts via a stale
//     event-handle in a between-tests reset (the unsubscribe in
//     Setup catches a between-test handle leak).
// ============================================================================

namespace
{
	enum Phase : int { kNV_Start, kNV_WaitScene, kNV_KillAll,
	                   kNV_Verify, kNV_Done };

	int                     g_iPhase = kNV_Start;
	int                     g_iVillagerCountAtStart = 0;
	int                     g_iDiedCount = 0;
	int                     g_iNoVesselsDispatchCount = 0;
	// Sentinel: tracks how many deaths had occurred BEFORE the NoVessels
	// event fired. Should equal g_iVillagerCountAtStart (i.e., NoVessels
	// fires AFTER the very last death, not before).
	int                     g_iDiedCountWhenNoVesselsFired = -1;

	Zenith_EventHandle      g_uDiedHandle = 0;
	Zenith_EventHandle      g_uRunLostHandle = 0;

	void OnVillagerDied(const DP_OnVillagerDied&)
	{
		++g_iDiedCount;
	}

	void OnRunLost(const DP_OnRunLost& xEvt)
	{
		if (xEvt.m_eCause == DP_RunLostCause::NoVessels)
		{
			++g_iNoVesselsDispatchCount;
			if (g_iDiedCountWhenNoVesselsFired < 0)
			{
				g_iDiedCountWhenNoVesselsFired = g_iDiedCount;
			}
		}
	}
}

static void Setup_P1NoVessels()
{
	g_iPhase = kNV_Start;
	g_iVillagerCountAtStart = 0;
	g_iDiedCount = 0;
	g_iNoVesselsDispatchCount = 0;
	g_iDiedCountWhenNoVesselsFired = -1;
	g_uDiedHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnVillagerDied>(
		&OnVillagerDied);
	g_uRunLostHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnRunLost>(
		&OnRunLost);
}

static bool Step_P1NoVessels(int iFrame)
{
	switch (g_iPhase)
	{
	case kNV_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kNV_WaitScene;
		return true;

	case kNV_WaitScene:
	{
		int iCount = 0;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&iCount](Zenith_EntityID, DPVillager_Behaviour&) { ++iCount; });
		// GameLevel has 17 authored villagers (some scenes may pad);
		// any count >= 2 is enough for the test to be meaningful (the
		// NoVessels-fires-on-LAST-death vs ON-ANY-DEATH discrimination
		// requires >= 2 villagers).
		if (iCount >= 2)
		{
			g_iVillagerCountAtStart = iCount;
			g_iPhase = kNV_KillAll;
		}
		else if (iFrame > 90)
		{
			g_iPhase = kNV_Done;
		}
		return true;
	}

	case kNV_KillAll:
	{
		// Collect all villager handles FIRST (avoid nesting the
		// outer iteration with the inner NoVessels-scan iteration
		// inside Kill()), then call Kill() on each in turn. Order
		// doesn't matter for the assertions; the test only cares
		// that NoVessels fires on the LAST death.
		Zenith_Vector<Zenith_EntityID> axVillagers;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axVillagers](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				axVillagers.PushBack(xId);
			});
		for (uint32_t u = 0; u < axVillagers.GetSize(); ++u)
		{
			Zenith_EntityID xId = axVillagers.Get(u);
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
			if (pxScene == nullptr) continue;
			Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
			if (!xEnt.IsValid()) continue;
			if (!xEnt.HasComponent<Zenith_ScriptComponent>()) continue;
			DPVillager_Behaviour* pxV =
				xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
			if (pxV != nullptr) pxV->Kill();
		}
		g_iPhase = kNV_Verify;
		return true;
	}

	case kNV_Verify:
		std::printf("[P1NoVessels] villagerCount=%d diedCount=%d noVesselsDispatchCount=%d diedAtNoVessels=%d\n",
			g_iVillagerCountAtStart, g_iDiedCount,
			g_iNoVesselsDispatchCount, g_iDiedCountWhenNoVesselsFired);
		std::fflush(stdout);
		g_iPhase = kNV_Done;
		return false;

	case kNV_Done:
	default:
		// Unsubscribe so the static event tally doesn't bleed into
		// the next batched test's run. (Between-tests hook does a
		// full reset of DP state but our static int counters live
		// here; clean up the dispatcher subscription explicitly so
		// the next Setup_P1NoVessels gets a fresh subscriber.)
		if (g_uDiedHandle != 0)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_uDiedHandle);
			g_uDiedHandle = 0;
		}
		if (g_uRunLostHandle != 0)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(g_uRunLostHandle);
			g_uRunLostHandle = 0;
		}
		return false;
	}
}

static bool Verify_P1NoVessels()
{
	if (g_iVillagerCountAtStart < 2)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1NoVessels: only %d villagers found in GameLevel (need >= 2 for the test to be meaningful)",
			g_iVillagerCountAtStart);
		return false;
	}
	if (g_iDiedCount != g_iVillagerCountAtStart)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1NoVessels: DP_OnVillagerDied fired %d times but expected %d (one per villager) -- Kill() may not dispatch, or some villagers' Kill() became no-ops",
			g_iDiedCount, g_iVillagerCountAtStart);
		return false;
	}
	if (g_iNoVesselsDispatchCount != 1)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1NoVessels: DP_OnRunLost{NoVessels} fired %d times but expected exactly 1. >1 means the alive-count scan fired the event multiple times (broken idempotency); 0 means it never fired (regression in Kill()'s scan logic)",
			g_iNoVesselsDispatchCount);
		return false;
	}
	if (g_iDiedCountWhenNoVesselsFired != g_iVillagerCountAtStart)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1NoVessels: NoVessels fired when %d villagers had died, but expected to fire when ALL %d had died. Early dispatch implies the alive-count scan is off-by-one or counts the just-died villager incorrectly",
			g_iDiedCountWhenNoVesselsFired, g_iVillagerCountAtStart);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1NoVesselsTest = {
	"Test_P1NoVessels_DispatchesRunLost",
	&Setup_P1NoVessels,
	&Step_P1NoVessels,
	&Verify_P1NoVessels,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1NoVesselsTest);

#endif // ZENITH_INPUT_SIMULATOR
