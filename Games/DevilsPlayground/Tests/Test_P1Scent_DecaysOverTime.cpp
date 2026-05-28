#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Scent_DecaysOverTime (MVP-1.6 follow-up coverage gap)
//
// `Test_P1Scent_AccumulatesOnPossession` verifies scent accumulates on
// voluntary switches and that the BB-write picks the highest, but
// NOTHING in the suite asserts that `TickDemonScent`'s decay path
// actually runs. A regression where:
//   * `decay_per_s` got set to 0 in tuning, or
//   * the controller stopped calling `DP_Player::TickDemonScent`, or
//   * the per-frame `fDt` arithmetic got broken
// would silently leak through. Scent would just sit at its accumulated
// peak forever -- gameplay-wise, the priest's future hound subsystem
// would react to "every villager you ever possessed" instead of
// "recently possessed".
//
// Procedure:
//   1. Load GameLevel.
//   2. Pick a villager.
//   3. SetPossessedVillager(villager) -- no scent bump (system path).
//   4. TryVoluntaryPossessSwitch on a different villager -- bumps scent
//      to ~0.3 on THAT villager (we choose the FIRST villager iterated
//      and switch to it from a different anchor; cleaner than poking
//      the impl directly).
//      Actually simpler: bump the chosen villager directly.
//      Picking 2 villagers in mutual range so the range gate doesn't
//      refuse the bump-switch.
//   5. Snapshot scent on the bumped villager immediately.
//   6. Tick N frames (~2 s = 120 frames at fixed 60 Hz dt).
//   7. Snapshot scent again.
//   8. Assert: scent[2] < scent[1] by at least `expected_decay * 0.5`
//      (50% tolerance absorbs frame-time jitter; we just need to
//      prove the decay path RAN, not measure the rate precisely).
//
// Expected math:
//   per_possession = 0.3
//   decay_per_s    = 0.05
//   2 s wait        -> ~0.10 decay
//   Lower bound on observed decay: 0.05
// ============================================================================

namespace
{
	enum Phase : int { kSD_Start, kSD_WaitScene, kSD_PossessA, kSD_BumpB,
	                   kSD_SnapshotInitial, kSD_Tick, kSD_SnapshotFinal,
	                   kSD_Verify, kSD_Done };

	int                     g_iPhase = kSD_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	float                   g_fScentInitial = 0.0f;
	float                   g_fScentFinal = 0.0f;
	int                     g_iTickCount = 0;

	// 2 s decay window -- long enough for measurable drop, short
	// enough to fit in a single test budget.
	constexpr int kTICK_FRAMES = 120;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	float HorizontalDistance(const Zenith_Maths::Vector3& xA,
	                         const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	void PickClosestPair(Zenith_EntityID& xA, Zenith_EntityID& xB)
	{
		struct VPos { Zenith_EntityID xId; Zenith_Maths::Vector3 xPos; };
		Zenith_Vector<VPos> axVs;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axVs](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				VPos xV; xV.xId = xId;
				if (TryGetEntityPos(xId, xV.xPos)) axVs.PushBack(xV);
			});
		if (axVs.GetSize() < 2) return;
		float fMin = 1e30f;
		for (uint32_t i = 0; i < axVs.GetSize(); ++i)
		for (uint32_t j = i + 1; j < axVs.GetSize(); ++j)
		{
			const float fD = HorizontalDistance(axVs.Get(i).xPos, axVs.Get(j).xPos);
			if (fD < fMin) { fMin = fD; xA = axVs.Get(i).xId; xB = axVs.Get(j).xId; }
		}
	}
}

static void Setup_P1ScentDecays()
{
	g_iPhase = kSD_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_fScentInitial = 0.0f;
	g_fScentFinal = 0.0f;
	g_iTickCount = 0;
}

static bool Step_P1ScentDecays(int iFrame)
{
	switch (g_iPhase)
	{
	case kSD_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kSD_WaitScene;
		return true;

	case kSD_WaitScene:
		PickClosestPair(g_xA, g_xB);
		if (g_xA.IsValid() && g_xB.IsValid())
		{
			g_iPhase = kSD_PossessA;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kSD_Done;
		}
		return true;

	case kSD_PossessA:
		// System path -- no scent. Anchor on A.
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kSD_BumpB;
		return true;

	case kSD_BumpB:
		// Voluntary switch onto B -- scent[B] += 0.3.
		DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iPhase = kSD_SnapshotInitial;
		return true;

	case kSD_SnapshotInitial:
		g_fScentInitial = DP_Player::GetDemonScent(g_xB);
		g_iTickCount = 0;
		g_iPhase = kSD_Tick;
		return true;

	case kSD_Tick:
		++g_iTickCount;
		if (g_iTickCount >= kTICK_FRAMES)
		{
			g_iPhase = kSD_SnapshotFinal;
		}
		return true;

	case kSD_SnapshotFinal:
		g_fScentFinal = DP_Player::GetDemonScent(g_xB);
		g_iPhase = kSD_Verify;
		return true;

	case kSD_Verify:
	{
		const float fDecayPerSec =
			DP_Tuning::Get<float>("possession.demon_scent_decay_per_s");
		const float fElapsed = kTICK_FRAMES * 0.01666f;
		const float fExpectedDrop = fDecayPerSec * fElapsed;
		const float fObservedDrop = g_fScentInitial - g_fScentFinal;
		std::printf("[P1ScentDecays] initial=%.3f final=%.3f drop=%.3f (expected ~%.3f over %.2fs)\n",
			g_fScentInitial, g_fScentFinal, fObservedDrop,
			fExpectedDrop, fElapsed);
		std::fflush(stdout);
		g_iPhase = kSD_Done;
		return false;
	}

	case kSD_Done:
	default:
		return false;
	}
}

static bool Verify_P1ScentDecays()
{
	if (!g_xA.IsValid() || !g_xB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ScentDecays: villager pick failed");
		return false;
	}
	if (g_fScentInitial <= 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentDecays: initial scent on B is %.3f -- TryVoluntaryPossessSwitch didn't bump scent",
			g_fScentInitial);
		return false;
	}
	const float fObservedDrop = g_fScentInitial - g_fScentFinal;
	const float fDecayPerSec =
		DP_Tuning::Get<float>("possession.demon_scent_decay_per_s");
	const float fElapsed = kTICK_FRAMES * 0.01666f;
	const float fExpectedDrop = fDecayPerSec * fElapsed;
	// 50% tolerance: catches "decay path didn't run at all" cleanly
	// without coupling tightly to the exact tuning value or timing.
	const float fMinDrop = fExpectedDrop * 0.5f;
	if (fObservedDrop < fMinDrop)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentDecays: scent dropped only %.3f over %.2fs (expected at least %.3f given decay_per_s=%.3f) -- TickDemonScent isn't running or decay is 0",
			fObservedDrop, fElapsed, fMinDrop, fDecayPerSec);
		return false;
	}
	if (g_fScentFinal >= g_fScentInitial)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentDecays: final scent (%.3f) is not less than initial (%.3f) -- decay didn't fire",
			g_fScentFinal, g_fScentInitial);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1ScentDecaysTest = {
	"Test_P1Scent_DecaysOverTime",
	&Setup_P1ScentDecays,
	&Step_P1ScentDecays,
	&Verify_P1ScentDecays,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1ScentDecaysTest);

#endif // ZENITH_INPUT_SIMULATOR
