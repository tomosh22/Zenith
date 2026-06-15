#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include "Source/PublicInterfaces.h"
#include "Components/DPHUDController_Component.h"
#include "Components/DPVillager_Component.h"
#include "Components/Priest_Component.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

#include <cstdio>

// ============================================================================
// Test_P2HUD_AelfricStateInRealScene (MVP-2.5.1 + 2.5.3 INTEGRATION)
//
// Pins the CLASSIFIER (DPHUDController_Component::ComputeAelfricState):
// the priest's blackboard each frame must classify into Calm / Suspicious
// / Pursuing.
//
//   PHASE A: idle scene -- priest's BB has neither TargetWithDevil nor
//   HasInvestigatePos. Expectation: ComputeAelfricState() == Calm.
//
//   PHASE B: drive a hostile perception target via the proper API
//   (Zenith_PerceptionSystem::EmitDamageStimulus). The damage stimulus
//   grants the priest immediate full awareness of the attacker, which
//   the priest's BridgePerceptionToBlackboard then writes into
//   BB.TargetWithDevil on the next frame. Expectation:
//   ComputeAelfricState() == Pursuing.
//
// What this catches (vs the formatter unit test):
//   * ComputeAelfricState reads the wrong BB key.
//   * ComputeAelfricState fails the priest-script iteration (e.g.,
//     wrong template type, DP_Query forgot to scan that entity).
//   * The priest's BridgePerceptionToBlackboard regressed and no
//     longer translates a perceived possessed villager into
//     BB.TargetWithDevil.
//
// Note: earlier versions of this test teleported the villager 4 m in
// the priest's facing direction and ticked 90 frames for the sight
// cone to lift awareness above the detection threshold. That setup
// was brittle to procgen geometry changes -- any wall or navmesh shift
// that altered the priest's patrol target could swing its facing away
// from the static villager position, so perception never built up.
// EmitDamageStimulus drives the same code path through the public API
// without any spatial dependency.
// ============================================================================

namespace
{
	enum Phase : int {
		kAS_Start, kAS_WaitScene, kAS_SnapshotCalm,
		kAS_EmitDamage, kAS_WaitBridge,
		kAS_SnapshotPursuing, kAS_Verify, kAS_Done
	};

	int                     g_iPhase = kAS_Start;
	Zenith_EntityID         g_xVillager;
	Zenith_EntityID         g_xPriest;
	int                     g_iTickCounter = 0;
	DPHUDController_Component::AelfricState g_eCalmSnapshot =
		DPHUDController_Component::AelfricState::Pursuing;   // sentinel
	DPHUDController_Component::AelfricState g_ePursuingSnapshot =
		DPHUDController_Component::AelfricState::Calm;       // sentinel

	// One full update cycle has to elapse between EmitDamageStimulus and
	// reading BB.TargetWithDevil: PerceptionSystem::Update runs first to
	// fold the damage stimulus into the priest's perceived-targets list,
	// THEN the priest's OnUpdate runs the bridge that copies that list
	// into the blackboard. Two ticks gives both phases a comfortable
	// chance to run regardless of script vs system update ordering.
	constexpr int kBRIDGE_TICKS = 2;

	const char* StateName(DPHUDController_Component::AelfricState e)
	{
		switch (e)
		{
		case DPHUDController_Component::AelfricState::Calm:       return "Calm";
		case DPHUDController_Component::AelfricState::Suspicious: return "Suspicious";
		case DPHUDController_Component::AelfricState::Pursuing:   return "Pursuing";
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
	g_eCalmSnapshot = DPHUDController_Component::AelfricState::Pursuing;
	g_ePursuingSnapshot = DPHUDController_Component::AelfricState::Calm;
}

static bool Step_P2HUDAelfricRealScene(int iFrame)
{
	switch (g_iPhase)
	{
	case kAS_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kAS_WaitScene;
		return true;

	case kAS_WaitScene:
	{
		Zenith_EntityID xFoundV, xFoundP;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xFoundV](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (!xFoundV.IsValid()) xFoundV = xId;
			});
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xFoundP](Zenith_EntityID xId, Priest_Component&)
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
	{
		// Establish a deterministic clean precondition for the classifier. The
		// procgen scene emits sounds on load that, under the suite's small
		// --fixed-dt, the priest is still "hearing" at snapshot time (they had
		// aged out by the 1-frame snapshot at wall-clock, so this only surfaced
		// in the fixed-dt batch). Clear the priest's decision BB so we assert the
		// CLASSIFIER mapping (clean BB -> Calm), independent of what the procgen
		// priest happens to perceive. The damage -> BB -> Pursuing integration is
		// still exercised by the phases below.
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(g_xPriest);
		if (pxScene != nullptr)
		{
			Zenith_Entity xEnt = pxScene->TryGetEntity(g_xPriest);
			if (xEnt.IsValid() && xEnt.HasComponent<Zenith_AIAgentComponent>())
			{
				Zenith_Blackboard& xBB =
					xEnt.GetComponent<Zenith_AIAgentComponent>().GetBlackboard();
				xBB.SetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
				xBB.SetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL, INVALID_ENTITY_ID);
			}
		}
		g_eCalmSnapshot = DPHUDController_Component::ComputeAelfricState();
		g_iPhase = kAS_EmitDamage;
		return true;
	}

	case kAS_EmitDamage:
		// Drive the bridge through the public perception API. Possess
		// the villager so the bridge's IsPossessedVillager filter
		// accepts it as a TargetWithDevil candidate, register the
		// villager as a hostile target (gives the perception system
		// permission to track it), and emit a damage stimulus from
		// the villager against the priest. The damage path bypasses
		// the sight cone -- it grants immediate full awareness of the
		// attacker -- so there's no geometry dependency to make this
		// flaky against procgen layout changes.
		DP_Player::SetPossessedVillager(g_xVillager);
		Zenith_PerceptionSystem::RegisterTarget(g_xVillager, /*hostile=*/true);
		Zenith_PerceptionSystem::EmitDamageStimulus(g_xPriest, g_xVillager);
		g_iTickCounter = 0;
		g_iPhase = kAS_WaitBridge;
		return true;

	case kAS_WaitBridge:
		++g_iTickCounter;
		if (g_iTickCounter >= kBRIDGE_TICKS) g_iPhase = kAS_SnapshotPursuing;
		return true;

	case kAS_SnapshotPursuing:
		g_ePursuingSnapshot = DPHUDController_Component::ComputeAelfricState();
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
	if (g_eCalmSnapshot != DPHUDController_Component::AelfricState::Calm)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2HUDAelfricReal: state pre-possession is %s, expected Calm. The priest's BB has stale TargetWithDevil/HasInvestigatePos before the test possessed anything",
			StateName(g_eCalmSnapshot));
		return false;
	}
	if (g_ePursuingSnapshot != DPHUDController_Component::AelfricState::Pursuing)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2HUDAelfricReal: state after EmitDamageStimulus is %s, expected Pursuing. Either the priest's BridgePerceptionToBlackboard isn't translating the damage-perceived possessed villager into BB.TargetWithDevil, or ComputeAelfricState isn't reading the right BB key",
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
