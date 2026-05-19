#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPHUDController_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"
#include "Components/Priest_Behaviour.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Maths/Zenith_Maths.h"

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
//     Setup: register the villager + teleport it 4 m into the
//     priest's facing direction so real sight-cone perception
//     catches it. Possess. Tick until awareness gain crosses the
//     detection threshold (kBRIDGE_TICKS frames).
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

	// Awareness gain rate ~2.0/s means the priest's awareness of a
	// villager in its sight cone crosses any reasonable detection
	// threshold within ~1 s. 90 frames at 60 Hz = 1.5 s -- comfortable
	// margin so the test isn't flaky on slow CI even though the bridge
	// itself runs every frame.
	constexpr int kBRIDGE_TICKS = 90;

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
		// MVP-1.9 cleanup: no possession yet + no LOS setup -- the
		// priest's BB has nothing in TargetWithDevil so the
		// classifier should report Calm.
		g_eCalmSnapshot = DPHUDController_Behaviour::ComputeAelfricState();
		g_iPhase = kAS_EnableOmni;
		return true;

	case kAS_EnableOmni:
	{
		// MVP-1.9 cleanup (omniscient fallback removed): use real
		// perception instead. Register the villager + teleport it
		// 4 m IN FRONT of the priest (authored priest yaw = 0,
		// facing +Z) so the priest's sight cone catches it.
		Zenith_PerceptionSystem::RegisterTarget(g_xVillager, /*hostile=*/true);
		Zenith_SceneData* pxPriestScene =
			Zenith_SceneManager::GetSceneDataForEntity(g_xPriest);
		Zenith_SceneData* pxVillagerScene =
			Zenith_SceneManager::GetSceneDataForEntity(g_xVillager);
		if (pxPriestScene != nullptr && pxVillagerScene != nullptr)
		{
			Zenith_Entity xPriestEnt = pxPriestScene->TryGetEntity(g_xPriest);
			Zenith_Entity xVillagerEnt = pxVillagerScene->TryGetEntity(g_xVillager);
			if (xPriestEnt.IsValid() && xVillagerEnt.IsValid()
			 && xPriestEnt.HasComponent<Zenith_TransformComponent>()
			 && xVillagerEnt.HasComponent<Zenith_TransformComponent>())
			{
				Zenith_Maths::Vector3 xPriestPos;
				xPriestEnt.GetComponent<Zenith_TransformComponent>()
					.GetPosition(xPriestPos);
				xVillagerEnt.GetComponent<Zenith_TransformComponent>()
					.SetPosition(Zenith_Maths::Vector3(
						xPriestPos.x, xPriestPos.y, xPriestPos.z + 4.0f));
			}
		}
		g_iPhase = kAS_PossessVillager;
		return true;
	}

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
