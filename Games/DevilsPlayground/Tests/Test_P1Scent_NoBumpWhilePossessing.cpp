#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Scent_NoBumpWhilePossessing (MVP-1.6 follow-up coverage gap)
//
// Scent is supposed to accumulate on the EVENT of voluntary switching
// onto a villager -- NOT continuously while you possess that villager.
// The GDD frames it as "the demon's stink lingers where the demon HAS
// BEEN, not where it currently IS". A regression where:
//   * `TryVoluntaryPossessSwitch` got rewritten to bump scent each
//     frame the target is possessed instead of once on the switch
//     edge, or
//   * `TickDemonScent` accidentally accumulated alongside decay (sign
//     flip on the per-frame delta) only for the currently-possessed
//     villager, or
//   * Some new "scent gain over time" feature snuck in unannounced
// would cause scent on the currently-possessed villager to GROW (or
// at least stop decaying) over multi-second possession windows.
//
// Gameplay symptom: the future hound subsystem would track wherever
// the player is currently sitting, not wherever the player has been.
// Defeats the point.
//
// `Test_P1Scent_DecaysOverTime` only covers UNPOSSESSED decay (it
// bumps B, then switches off / sits idle). It does NOT cover the
// scenario where the player just sits possessing the bumped villager
// for a long time, which is exactly the regression this test catches.
//
// Procedure:
//   1. Load GameLevel + pick two villagers in mutual range.
//   2. SetPossessedVillager(A) -- system path, no scent.
//   3. TryVoluntaryPossessSwitch(B) -- scent[B] = 0.3, B is now
//      possessed. Snapshot scent[B] one frame later.
//   4. Tick 120 frames (~2 s) WITHOUT switching anywhere else.
//   5. Snapshot scent[B] again.
//   6. Assert:
//        scent[B] strictly decreased (didn't grow)
//        scent[B] dropped by approximately decay_per_s * 2s (matches
//        the expectation of `Test_P1Scent_DecaysOverTime`).
//
// The "approximately decay rate" assertion is what specifically
// catches a "bump-every-frame while possessed" regression -- in that
// world, scent[B] would either pin at 1.0 (saturation) or grow at
// `per_possession * 60 frames/s` minus decay = wildly positive.
// Decay alone would give us ~0.10 drop; bump-per-frame would give
// us ~+18 (or saturate). The 0.5 * decay_per_s * 2s lower bound
// cleanly separates the two regimes.
// ============================================================================

namespace
{
	enum Phase : int { kNB_Start, kNB_WaitScene, kNB_PossessA, kNB_HopToB,
	                   kNB_SnapshotInitial, kNB_Tick, kNB_SnapshotFinal,
	                   kNB_Verify, kNB_Done };

	int                     g_iPhase = kNB_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	float                   g_fScentInitial = 0.0f;
	float                   g_fScentFinal = 0.0f;
	int                     g_iTickCount = 0;

	// 2 s window: long enough that bump-per-frame would saturate or
	// rocket past, short enough that a normal decay-only world only
	// drops by ~0.10. The two regimes are unambiguous.
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

static void Setup_P1ScentNoBumpWhilePossessing()
{
	g_iPhase = kNB_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_fScentInitial = 0.0f;
	g_fScentFinal = 0.0f;
	g_iTickCount = 0;
}

static bool Step_P1ScentNoBumpWhilePossessing(int iFrame)
{
	switch (g_iPhase)
	{
	case kNB_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kNB_WaitScene;
		return true;

	case kNB_WaitScene:
		PickClosestPair(g_xA, g_xB);
		if (g_xA.IsValid() && g_xB.IsValid())
		{
			g_iPhase = kNB_PossessA;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kNB_Done;
		}
		return true;

	case kNB_PossessA:
		// System path -- no scent on A.
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kNB_HopToB;
		return true;

	case kNB_HopToB:
		// Voluntary switch onto B -- scent[B] += 0.3 ONCE on the edge.
		// B is now the possessed villager.
		DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iPhase = kNB_SnapshotInitial;
		return true;

	case kNB_SnapshotInitial:
		// One frame later so the bump landed in the table.
		g_fScentInitial = DP_Player::GetDemonScent(g_xB);
		g_iTickCount = 0;
		g_iPhase = kNB_Tick;
		return true;

	case kNB_Tick:
		// Sit on B. Do NOT issue any further voluntary switches. If
		// the impl is correct, only TickDemonScent's decay branch runs
		// on this entry across the next 120 frames. If a regression
		// has slipped in that bumps the possessed villager per frame,
		// scent[B] will skyrocket or pin at 1.0.
		++g_iTickCount;
		if (g_iTickCount >= kTICK_FRAMES)
		{
			g_iPhase = kNB_SnapshotFinal;
		}
		return true;

	case kNB_SnapshotFinal:
		g_fScentFinal = DP_Player::GetDemonScent(g_xB);
		g_iPhase = kNB_Verify;
		return true;

	case kNB_Verify:
	{
		const float fDecayPerSec =
			DP_Tuning::Get<float>("possession.demon_scent_decay_per_s");
		const float fElapsed = kTICK_FRAMES * 0.01666f;
		const float fExpectedDrop = fDecayPerSec * fElapsed;
		const float fObservedDelta = g_fScentInitial - g_fScentFinal;
		std::printf("[P1ScentNoBumpWhilePossessing] initial=%.3f final=%.3f delta=%.3f (expected decay ~%.3f over %.2fs; positive delta = decay-only, negative = bump-while-possessed regression)\n",
			g_fScentInitial, g_fScentFinal, fObservedDelta,
			fExpectedDrop, fElapsed);
		std::fflush(stdout);
		g_iPhase = kNB_Done;
		return false;
	}

	case kNB_Done:
	default:
		return false;
	}
}

static bool Verify_P1ScentNoBumpWhilePossessing()
{
	if (!g_xA.IsValid() || !g_xB.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1ScentNoBumpWhilePossessing: villager pick failed");
		return false;
	}
	if (g_fScentInitial <= 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentNoBumpWhilePossessing: initial scent on B is %.3f -- TryVoluntaryPossessSwitch didn't bump scent (precondition failure)",
			g_fScentInitial);
		return false;
	}
	// Primary assertion: scent must strictly decrease (decay only).
	// If it grew, somebody bumped it while possessed.
	if (g_fScentFinal >= g_fScentInitial)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentNoBumpWhilePossessing: final scent (%.3f) >= initial (%.3f). Scent grew or stayed flat while continuously possessing B for 2s -- a per-frame bump regression in TickDemonScent or TryVoluntaryPossessSwitch is leaking scent every tick.",
			g_fScentFinal, g_fScentInitial);
		return false;
	}
	// Secondary assertion: the drop matches the decay expectation. If
	// scent dropped but by FAR LESS than the decay rate would predict
	// (or NEGATIVE delta below the floor), that suggests a partial
	// bump is fighting decay -- e.g., bumping by 0.005/frame against
	// 0.05/s decay would net out at ~+0.25 over 2s but scent caps at
	// 1.0 so we'd see ~+0.2 here.
	const float fDecayPerSec =
		DP_Tuning::Get<float>("possession.demon_scent_decay_per_s");
	const float fElapsed = kTICK_FRAMES * 0.01666f;
	const float fExpectedDrop = fDecayPerSec * fElapsed;
	const float fObservedDrop = g_fScentInitial - g_fScentFinal;
	// 50% tolerance: same lower bound as Test_P1Scent_DecaysOverTime
	// so the two tests fail together if decay rate or tick path break.
	const float fMinDrop = fExpectedDrop * 0.5f;
	if (fObservedDrop < fMinDrop)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1ScentNoBumpWhilePossessing: scent dropped only %.3f over %.2fs while possessed -- expected at least %.3f given decay_per_s=%.3f. Suggests partial bump leaking each frame and partially cancelling decay.",
			fObservedDrop, fElapsed, fMinDrop, fDecayPerSec);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1ScentNoBumpWhilePossessingTest = {
	"Test_P1Scent_NoBumpWhilePossessing",
	&Setup_P1ScentNoBumpWhilePossessing,
	&Step_P1ScentNoBumpWhilePossessing,
	&Verify_P1ScentNoBumpWhilePossessing,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1ScentNoBumpWhilePossessingTest);

#endif // ZENITH_INPUT_SIMULATOR
