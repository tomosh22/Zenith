#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPHUDController_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"

#include <cstdio>

// ============================================================================
// Test_P2HUD_AelfricStateInRealScene (MVP-2.5.1 + 2.5.3 INTEGRATION)
//
// Test_P2HUD_AelfricStateReadouts pins the formatters (state name ->
// text). This test pins the CLASSIFIER: ComputeAelfricState reads
// the priest's blackboard each frame and returns the right enum.
// Real scene with a real priest agent registered with the perception
// system; we drive possession to flip the priest's BB and watch
// the classifier change.
//
// Two scenarios:
//
//   PHASE A: idle scene (priest hasn't seen anything).
//     Setup: load GameLevel, NO villager possessed yet.
//     Expectation: ComputeAelfricState() == Calm. The priest's
//     BB has neither TargetWithDevil nor HasInvestigatePos set.
//
//   PHASE B: possessed villager (priest's BB.TargetWithDevil set).
//     Setup: SetTestOmniscientFallback(true) so the priest's
//     BridgePerceptionToBlackboard fills TargetWithDevil from
//     DP_Player::GetPossessedVillager without needing physical
//     line-of-sight. Possess a villager. Tick a few frames so the
//     priest's OnUpdate runs.
//     Expectation: ComputeAelfricState() == Pursuing.
//
// What this catches (vs the formatter unit test):
//   * ComputeAelfricState reads the wrong BB key.
//   * ComputeAelfricState fails the priest-script iteration (e.g.,
//     wrong template type, DP_Query forgot to scan that entity).
//   * The priest's BridgePerceptionToBlackboard regressed and no
//     longer writes TargetWithDevil even with the omniscient
//     fallback on.
// ============================================================================

namespace
{
	enum Phase : int {
		kAS_Start, kAS_WaitScene, kAS_SnapshotCalm,
		kAS_EnableOmni, kAS_PossessVillager, kAS_TickForBridge,
		kAS_SnapshotPursuing, kAS_Verify, kAS_Done
	};

	int                     g_iPhase = kAS_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xPriest;
	int                     g_iTickCounter = 0;
	DPHUDController_Behaviour::AelfricState g_eCalmSnapshot =
		DPHUDController_Behaviour::AelfricState::Pursuing;   // sentinel
	DPHUDController_Behaviour::AelfricState g_ePursuingSnapshot =
		DPHUDController_Behaviour::AelfricState::Calm;       // sentinel

	// 10 frames is enough for the priest's OnUpdate (which fires
	// each frame regardless of BT throttle) to call
	// BridgePerceptionToBlackboard at least once.
	constexpr int kBRIDGE_TICKS = 10;

	const char* StateName(DPHUDController_Behaviour::AelfricState e)
	{
		switch (e)
		{
		case DPHUDController_Behaviour::AelfricState::Calm:       return "Calm";
		case DPHUDController_Behaviour::AelfricState::Suspicious: return "Suspicious";
		case DPHUDController_Behaviour::AelfricState::Pursuing:   return "Pursuing";
		}
		return "?";
	}
}

static void Setup_P2HUDAelfricRealScene()
{
	g_iPhase = kAS_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_xPriest = INVALID_ENTITY_ID;
	g_iTickCounter = 0;
	g_eCalmSnapshot = DPHUDController_Behaviour::AelfricState::Pursuing;
	g_ePursuingSnapshot = DPHUDController_Behaviour::AelfricState::Calm;
}

static bool Step_P2HUDAelfricRealScene(int iFrame)
{
	switch (g_iPhase)
	{
	case kAS_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kAS_WaitScene;
		return true;

	case kAS_WaitScene:
	{
		Zenith_EntityID xFoundV, xFoundP;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFoundV](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFoundV.IsValid()) xFoundV = xId;
			});
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFoundP](Zenith_EntityID xId, Priest_Behaviour&)
			{ xFoundP = xId; });
		if (xFoundV.IsValid() && xFoundP.IsValid())
		{
			g_xVillager = xFoundV;
			g_xPriest = xFoundP;
			g_iPhase = kAS_SnapshotCalm;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kAS_Done;
		}
		return true;
	}

	case kAS_SnapshotCalm:
		// Disable omniscient fallback so the priest's BB doesn't
		// spuriously fill TargetWithDevil from a stale state.
		DP_Player::SetTestOmniscientFallback(false);
		// No possession yet -- priest's BB should be empty.
		g_eCalmSnapshot = DPHUDController_Behaviour::ComputeAelfricState();
		g_iPhase = kAS_EnableOmni;
		return true;

	case kAS_EnableOmni:
		DP_Player::SetTestOmniscientFallback(true);
		g_iPhase = kAS_PossessVillager;
		return true;

	case kAS_PossessVillager:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iTickCounter = 0;
		g_iPhase = kAS_TickForBridge;
		return true;

	case kAS_TickForBridge:
		++g_iTickCounter;
		if (g_iTickCounter >= kBRIDGE_TICKS) g_iPhase = kAS_SnapshotPursuing;
		return true;

	case kAS_SnapshotPursuing:
		g_ePursuingSnapshot = DPHUDController_Behaviour::ComputeAelfricState();
		g_iPhase = kAS_Verify;
		return true;

	case kAS_Verify:
		std::printf("[P2HUDAelfricReal] calmSnapshot=%s pursuingSnapshot=%s (expected: Calm, Pursuing)\n",
			StateName(g_eCalmSnapshot), StateName(g_ePursuingSnapshot));
		std::fflush(stdout);
		g_iPhase = kAS_Done;
		return false;

	case kAS_Done:
	default:
		return false;
	}
}

static bool Verify_P2HUDAelfricRealScene()
{
	if (!g_xVillager.IsValid() || !g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P2HUDAelfricReal: setup entities missing");
		return false;
	}
	if (g_eCalmSnapshot != DPHUDController_Behaviour::AelfricState::Calm)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2HUDAelfricReal: state pre-possession is %s, expected Calm. The priest's BB has stale TargetWithDevil/HasInvestigatePos before the test possessed anything",
			StateName(g_eCalmSnapshot));
		return false;
	}
	if (g_ePursuingSnapshot != DPHUDController_Behaviour::AelfricState::Pursuing)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2HUDAelfricReal: state after possessing a villager is %s, expected Pursuing. Either the priest's BridgePerceptionToBlackboard isn't writing TargetWithDevil (omniscient fallback off?) or ComputeAelfricState isn't reading the right BB key",
			StateName(g_ePursuingSnapshot));
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2HUDAelfricRealSceneTest = {
	"Test_P2HUD_AelfricStateInRealScene",
	&Setup_P2HUDAelfricRealScene,
	&Step_P2HUDAelfricRealScene,
	&Verify_P2HUDAelfricRealScene,
	180
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2HUDAelfricRealSceneTest);

#endif // ZENITH_INPUT_SIMULATOR
