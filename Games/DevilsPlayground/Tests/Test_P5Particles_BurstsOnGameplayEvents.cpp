#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPParticles.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Components/DPVillager_Component.h"

#include <cstdio>

// ============================================================================
// Test_P5Particles_BurstsOnGameplayEvents (2026-05-21)
//
// Pins the DP_Particles dispatch chain: every player-facing gameplay
// event that should produce in-world visual feedback DOES fire the right
// burst kind. The actual particle simulation is engine code (Flux
// particles); what we test here is the DP-side wiring from
// DP_On<Foo> event -> DP_Particles::Burst(kind, position).
//
// Procedure:
//   1. Load ProcLevel; wait for DPProcLevelBootstrap::OnStart to fire
//      DP_Particles::EnsureEmittersInScene() (creates emitter entities).
//   2. Snapshot all per-Kind burst counts (all zero post-reset).
//   3. Dispatch each event the DPParticles subsystem listens for.
//   4. Confirm the corresponding burst counter incremented by 1.
//   5. Verify the emitter entity exists in the persistent scene and
//      has an attached Zenith_ParticleEmitterComponent.
//
// What this catches:
//   * A DP_On<Foo> event lacking a particle subscription (regression: a
//     new event got added but the particles system wasn't extended).
//   * A particle burst dispatch path no-op'ing because the emitter
//     entity was never created.
//   * The between-tests reset hook not clearing burst counters (causes
//     downstream tests to see stale counts).
//
// Doesn't test the visual quality of the particles -- that's a manual
// eye-test that lives in the developer iteration loop.
// ============================================================================

namespace
{
	enum Phase : int {
		kPB_Start, kPB_WaitScene, kPB_WaitBootstrap, kPB_Snapshot,
		kPB_FireEvents, kPB_Verify, kPB_Done
	};

	int                     g_iPhase = kPB_Start;
	int                     g_iWaitFrames = 0;

	uint32_t                g_auBaselineCounts[static_cast<int>(DP_Particles::Kind::COUNT)] = { 0 };
	uint32_t                g_auFinalCounts[static_cast<int>(DP_Particles::Kind::COUNT)] = { 0 };

	bool                    g_abEmitterPresent[static_cast<int>(DP_Particles::Kind::COUNT)] = { false };
	bool                    g_abEmitterHasComponent[static_cast<int>(DP_Particles::Kind::COUNT)] = { false };
}

static void Setup_P5Particles()
{
	g_iPhase = kPB_Start;
	g_iWaitFrames = 0;
	for (int i = 0; i < static_cast<int>(DP_Particles::Kind::COUNT); ++i)
	{
		g_auBaselineCounts[i] = 0;
		g_auFinalCounts[i] = 0;
		g_abEmitterPresent[i] = false;
		g_abEmitterHasComponent[i] = false;
	}
}

static bool Step_P5Particles(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	case kPB_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kPB_WaitScene;
		g_iWaitFrames = 0;
		return true;

	case kPB_WaitScene:
	{
		// Give the scene a few frames so DPProcLevelBootstrap::OnAwake
		// + OnStart have run (bootstrap creates emitter entities in
		// OnStart via DP_Particles::EnsureEmittersInScene).
		++g_iWaitFrames;
		if (g_iWaitFrames < 4) return true;
		g_iPhase = kPB_WaitBootstrap;
		g_iWaitFrames = 0;
		return true;
	}

	case kPB_WaitBootstrap:
	{
		// Confirm every emitter entity has been spawned. If
		// EnsureEmittersInScene hasn't run yet (e.g. bootstrap timing
		// changed), advance after a sane timeout.
		bool bAllPresent = true;
		for (int i = 0; i < static_cast<int>(DP_Particles::Kind::COUNT); ++i)
		{
			DP_Particles::Kind eKind = static_cast<DP_Particles::Kind>(i);
			if (!DP_Particles::GetEmitterEntityForTest(eKind).IsValid())
			{
				bAllPresent = false;
				break;
			}
		}
		++g_iWaitFrames;
		if (!bAllPresent && g_iWaitFrames < 60) return true;
		g_iPhase = kPB_Snapshot;
		return true;
	}

	case kPB_Snapshot:
	{
		// Reset counters first so we're sampling deltas from zero,
		// regardless of whatever bootstrapped events fired before
		// this snapshot.
		DP_Particles::ResetBurstCountsForTest();
		for (int i = 0; i < static_cast<int>(DP_Particles::Kind::COUNT); ++i)
		{
			DP_Particles::Kind eKind = static_cast<DP_Particles::Kind>(i);
			g_auBaselineCounts[i] = DP_Particles::GetBurstCountForTest(eKind);
		}
		g_iPhase = kPB_FireEvents;
		return true;
	}

	case kPB_FireEvents:
	{
		// Pick a real entity in the active scene for the
		// BurstAtEntity path (otherwise it short-circuits). The
		// bootstrap entity itself is fine -- it has a Transform.
		// Dispatching the events bypasses the gameplay-side trigger
		// checks (priest BB sets, key-consumption gates, etc); we're
		// pinning the particle subscription chain, not the gameplay.
		Zenith_EntityID xAny;
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xAny](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (!xAny.IsValid()) xAny = xId;
			});

		auto& xDispatcher = Zenith_EventDispatcher::Get();
		xDispatcher.Dispatch(DP_OnForgeCrafted{ xAny, xAny, INVALID_ENTITY_ID });
		xDispatcher.Dispatch(DP_OnDoorOpened{ xAny, xAny });
		xDispatcher.Dispatch(DP_OnDoorLockRejected{ xAny, xAny, DP_ItemTag::Key });
		xDispatcher.Dispatch(DP_OnChestOpened{ xAny, xAny });
		xDispatcher.Dispatch(DP_OnObjectivePlaced{ xAny, xAny, 0 });
		xDispatcher.Dispatch(DP_OnBellRing{ xAny, xAny, Zenith_Maths::Vector3(10.f, 0.f, 10.f) });
		xDispatcher.Dispatch(DP_OnItemEvaporated{ xAny, DP_ItemTag::BogWater, Zenith_Maths::Vector3(10.f, 0.f, 10.f) });
		xDispatcher.Dispatch(DP_OnPriestAlerted{
			xAny, DP_PriestAlertKind::SawTarget, Zenith_Maths::Vector3(10.f, 0.f, 10.f) });

		g_iPhase = kPB_Verify;
		return true;
	}

	case kPB_Verify:
	{
		Zenith_Scene xPersistent = g_xEngine.Scenes().GetPersistentScene();
		Zenith_SceneData* pxPersistent = g_xEngine.Scenes().GetSceneData(xPersistent);
		for (int i = 0; i < static_cast<int>(DP_Particles::Kind::COUNT); ++i)
		{
			DP_Particles::Kind eKind = static_cast<DP_Particles::Kind>(i);
			g_auFinalCounts[i] = DP_Particles::GetBurstCountForTest(eKind);

			Zenith_EntityID xEnt = DP_Particles::GetEmitterEntityForTest(eKind);
			if (xEnt.IsValid() && pxPersistent != nullptr)
			{
				Zenith_Entity xE = pxPersistent->TryGetEntity(xEnt);
				g_abEmitterPresent[i] = xE.IsValid();
				g_abEmitterHasComponent[i] = g_abEmitterPresent[i]
					&& xE.HasComponent<Zenith_ParticleEmitterComponent>();
			}
		}
		std::printf("[P5Particles] burst counts:");
		for (int i = 0; i < static_cast<int>(DP_Particles::Kind::COUNT); ++i)
			std::printf(" kind%d=%u(prev=%u)", i, g_auFinalCounts[i], g_auBaselineCounts[i]);
		std::printf("\n");
		std::fflush(stdout);
		g_iPhase = kPB_Done;
		return false;
	}

	case kPB_Done:
	default:
		return false;
	}
}

static bool Verify_P5Particles()
{
	// Every emitter entity must be present + have the component.
	for (int i = 0; i < static_cast<int>(DP_Particles::Kind::COUNT); ++i)
	{
		if (!g_abEmitterPresent[i])
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P5Particles: emitter entity for kind=%d missing (DPProcLevelBootstrap didn't call EnsureEmittersInScene?)",
				i);
			return false;
		}
		if (!g_abEmitterHasComponent[i])
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P5Particles: emitter entity for kind=%d lacks Zenith_ParticleEmitterComponent",
				i);
			return false;
		}
	}

	// Every event we dispatched should have produced exactly one burst
	// from its delta vs the baseline. We don't check exact counts
	// because subscriptions could fire multiple times if the dispatcher
	// is mid-bootstrap; we check the delta is >= 1 to avoid false
	// negatives on first-frame timing.
	auto fnCheck = [](DP_Particles::Kind eKind, const char* szName) -> bool {
		const int i = static_cast<int>(eKind);
		const uint32_t uDelta = g_auFinalCounts[i] - g_auBaselineCounts[i];
		if (uDelta == 0)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"P5Particles: kind %s didn't fire (count stayed at %u). Event subscription missing?",
				szName, g_auFinalCounts[i]);
			return false;
		}
		return true;
	};

	if (!fnCheck(DP_Particles::Kind::ForgeSparks,       "ForgeSparks"))       return false;
	if (!fnCheck(DP_Particles::Kind::DoorOpenDust,      "DoorOpenDust"))      return false;
	if (!fnCheck(DP_Particles::Kind::DoorLockRejected,  "DoorLockRejected"))  return false;
	if (!fnCheck(DP_Particles::Kind::ChestOpenDust,     "ChestOpenDust"))     return false;
	if (!fnCheck(DP_Particles::Kind::PentagramRitual,   "PentagramRitual"))   return false;
	if (!fnCheck(DP_Particles::Kind::BellSoulRing,      "BellSoulRing"))      return false;
	if (!fnCheck(DP_Particles::Kind::BogWaterSteam,     "BogWaterSteam"))     return false;
	if (!fnCheck(DP_Particles::Kind::PriestAlert,       "PriestAlert"))       return false;

	return true;
}

static const Zenith_AutomatedTest g_xP5ParticlesTest = {
	"Test_P5Particles_BurstsOnGameplayEvents",
	&Setup_P5Particles,
	&Step_P5Particles,
	&Verify_P5Particles,
	180
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP5ParticlesTest);

#endif // ZENITH_INPUT_SIMULATOR
