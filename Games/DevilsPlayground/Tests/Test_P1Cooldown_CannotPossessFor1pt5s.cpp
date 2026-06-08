#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Collections/Zenith_Vector.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>

// ============================================================================
// Test_P1Cooldown_CannotPossessFor1pt5s (MVP-1.5.1 + 1.5.2)
//
// Verifies the possession-cooldown gate added by MVP-1.5.2. After a
// voluntary switch, `DP_Player::TryVoluntaryPossessSwitch(another_id)`
// must REFUSE any further switches until the cooldown drains to zero.
// Default cooldown is `possession.cooldown_after_voluntary_switch_s = 1.5`.
//
// Procedure:
//   1. Load GameLevel.
//   2. Pick three distinct villagers (A = closest, B = mid-distance,
//      C = farthest -- the actual choice doesn't matter; we just need
//      three different EntityIDs).
//   3. SetPossessedVillager(A) -- direct write, no cooldown. Initial
//      possession.
//   4. TryVoluntaryPossessSwitch(B) -- should SUCCEED. Cooldown now
//      armed at ~1.5s.
//   5. Immediately TryVoluntaryPossessSwitch(C) -- should REFUSE
//      (cooldown active). Player remains possessing B.
//   6. Run ~60 frames (~1.0s). Cooldown should still be active
//      (~0.5s remaining).
//   7. TryVoluntaryPossessSwitch(C) again -- still REFUSED.
//   8. Run ~40 more frames (~0.7s additional, total ~1.7s -- past the
//      1.5s cooldown). Cooldown should now be zero.
//   9. TryVoluntaryPossessSwitch(C) -- should SUCCEED.
//
// What this proves end-to-end:
//   * TryVoluntaryPossessSwitch returns false during the cooldown
//     window.
//   * DPPlayerController_Behaviour's per-frame TickPossessionCooldown
//     drains the timer at wall-clock rate.
//   * The possession state stays on the LAST successfully-set villager
//     while the cooldown is active (the rejected attempts don't
//     accidentally null out the handle).
// ============================================================================

namespace
{
	enum Phase : int { kCD_Start, kCD_WaitScene, kCD_PossessA, kCD_SwitchToB,
	                   kCD_TryEarlySwitchToC, kCD_TickFirstHalf,
	                   kCD_TryMidSwitchToC, kCD_TickPastEnd,
	                   kCD_TryLateSwitchToC, kCD_Done };

	int                     g_iPhase = kCD_Start;
	Zenith_EntityID         g_xA;
	Zenith_EntityID         g_xB;
	Zenith_EntityID         g_xC;

	// Captured verdicts of each TryVoluntaryPossessSwitch attempt.
	bool                    g_bSwitchToBOk = false;
	bool                    g_bEarlySwitchToCOk = false;
	bool                    g_bMidSwitchToCOk = false;
	bool                    g_bLateSwitchToCOk = false;
	Zenith_EntityID         g_xAfterEarlyAttempt;
	Zenith_EntityID         g_xAfterMidAttempt;
	Zenith_EntityID         g_xAfterLateAttempt;
	int                     g_iTickFrames = 0;

	// Frame budgets chosen against the 1.5s default cooldown:
	//   * 60 frames ~= 1.0s. After this we should STILL be in cooldown.
	//   * 40 more frames ~= 0.67s additional, total ~1.67s. Past the
	//     1.5s mark, cooldown should have drained.
	constexpr int kFRAMES_FIRST_TICK_BLOCK  = 60;
	constexpr int kFRAMES_SECOND_TICK_BLOCK = 40;

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

	float HorizontalDistance(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	// #9: procgen now assigns varied archetypes. Force a chosen test villager
	// to Farmhand so possession is IMMEDIATE -- this test isolates the cooldown
	// gate, not the Devout 0.8s possession channel (covered by the DevoutChannel
	// tests). ApplyArchetype re-seeds stats + resets life (villager isn't
	// possessed yet at this point), which is fine for the cooldown sequence.
	void ForceFarmhand(Zenith_EntityID xId)
	{
		if (!xId.IsValid()) return;
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_ScriptComponent>()) return;
		if (DPVillager_Behaviour* pxV =
				xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>())
		{
			pxV->ApplyArchetype("Farmhand");
		}
	}

	// Pick three distinct villagers that are MUTUALLY within
	// `possession.range_from_anchor_m` of each other. Required because
	// MVP-1.8's range gate also fires inside TryVoluntaryPossessSwitch
	// -- if A and B are 80 m apart, the switch is refused for the
	// "out of range" reason, not the cooldown reason this test wants
	// to assert.
	//
	// Strategy: find the closest pair (A, B) in the level, then pick C
	// as the villager closest to A (other than B) that's also within
	// range of A AND of B. With the stock 17-villager spread of
	// GameLevel that gives the Blacksmith / Blacksmith2 / Pleb6
	// cluster (all under 15 m mutually).
	void PickThreeMutualRangeVillagers(Zenith_EntityID& xA,
	                                    Zenith_EntityID& xB,
	                                    Zenith_EntityID& xC)
	{
		const float fMaxRange =
			DP_Tuning::Get<float>("possession.range_from_anchor_m");

		struct Cand { Zenith_EntityID xId; Zenith_Maths::Vector3 xPos; };
		Zenith_Vector<Cand> axCands;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&axCands]
			(Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				Cand xV; xV.xId = xId;
				if (TryGetEntityPos(xId, xV.xPos))
				{
					axCands.PushBack(xV);
				}
			});
		if (axCands.GetSize() < 3) return;

		// Find the closest pair (A, B).
		float fMinPair = 1e30f;
		uint32_t uA = UINT32_MAX, uB = UINT32_MAX;
		for (uint32_t i = 0; i < axCands.GetSize(); ++i)
		{
			for (uint32_t j = i + 1; j < axCands.GetSize(); ++j)
			{
				const float fD = HorizontalDistance(
					axCands.Get(i).xPos, axCands.Get(j).xPos);
				if (fD < fMinPair)
				{
					fMinPair = fD;
					uA = i;
					uB = j;
				}
			}
		}
		if (uA == UINT32_MAX) return;
		if (fMinPair > fMaxRange) return; // No in-range pair anywhere.

		// Find C: a villager (not A, not B) within fMaxRange of BOTH
		// A AND B. We try every remaining candidate and stop on the
		// first hit. If none qualify, give up -- the caller will see
		// xC.IsValid() == false and fail with a test-design message.
		for (uint32_t k = 0; k < axCands.GetSize(); ++k)
		{
			if (k == uA || k == uB) continue;
			const float fDA = HorizontalDistance(
				axCands.Get(k).xPos, axCands.Get(uA).xPos);
			const float fDB = HorizontalDistance(
				axCands.Get(k).xPos, axCands.Get(uB).xPos);
			if (fDA <= fMaxRange && fDB <= fMaxRange)
			{
				xA = axCands.Get(uA).xId;
				xB = axCands.Get(uB).xId;
				xC = axCands.Get(k).xId;
				return;
			}
		}
	}
}

static void Setup_P1Cooldown()
{
	g_iPhase = kCD_Start;
	g_xA = INVALID_ENTITY_ID;
	g_xB = INVALID_ENTITY_ID;
	g_xC = INVALID_ENTITY_ID;
	g_bSwitchToBOk = false;
	g_bEarlySwitchToCOk = false;
	g_bMidSwitchToCOk = false;
	g_bLateSwitchToCOk = false;
	g_xAfterEarlyAttempt = INVALID_ENTITY_ID;
	g_xAfterMidAttempt = INVALID_ENTITY_ID;
	g_xAfterLateAttempt = INVALID_ENTITY_ID;
	g_iTickFrames = 0;
}

static bool Step_P1Cooldown(int iFrame)
{
	switch (g_iPhase)
	{
	case kCD_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kCD_WaitScene;
		return true;

	case kCD_WaitScene:
	{
		PickThreeMutualRangeVillagers(g_xA, g_xB, g_xC);
		if (g_xA.IsValid() && g_xB.IsValid() && g_xC.IsValid()
			&& g_xA.m_uIndex != g_xB.m_uIndex
			&& g_xB.m_uIndex != g_xC.m_uIndex
			&& g_xA.m_uIndex != g_xC.m_uIndex)
		{
			g_iPhase = kCD_PossessA;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kCD_Done;
		}
		return true;
	}

	case kCD_PossessA:
		// #9: neutralise archetype variety -- force the three test villagers to
		// Farmhand so each switch possesses immediately (no Devout channel).
		ForceFarmhand(g_xA);
		ForceFarmhand(g_xB);
		ForceFarmhand(g_xC);
		// Direct write -- baseline state, no cooldown.
		DP_Player::SetPossessedVillager(g_xA);
		g_iPhase = kCD_SwitchToB;
		return true;

	case kCD_SwitchToB:
		// Voluntary switch -- arms the cooldown on success.
		g_bSwitchToBOk = DP_Player::TryVoluntaryPossessSwitch(g_xB);
		g_iPhase = kCD_TryEarlySwitchToC;
		return true;

	case kCD_TryEarlySwitchToC:
		// Immediately attempt another switch -- must be REFUSED.
		g_bEarlySwitchToCOk = DP_Player::TryVoluntaryPossessSwitch(g_xC);
		g_xAfterEarlyAttempt = DP_Player::GetPossessedVillager();
		g_iTickFrames = 0;
		g_iPhase = kCD_TickFirstHalf;
		return true;

	case kCD_TickFirstHalf:
		++g_iTickFrames;
		if (g_iTickFrames >= kFRAMES_FIRST_TICK_BLOCK)
		{
			g_iPhase = kCD_TryMidSwitchToC;
		}
		return true;

	case kCD_TryMidSwitchToC:
		// Still within the 1.5s window (~1.0s elapsed) -- still REFUSED.
		g_bMidSwitchToCOk = DP_Player::TryVoluntaryPossessSwitch(g_xC);
		g_xAfterMidAttempt = DP_Player::GetPossessedVillager();
		g_iTickFrames = 0;
		g_iPhase = kCD_TickPastEnd;
		return true;

	case kCD_TickPastEnd:
		++g_iTickFrames;
		if (g_iTickFrames >= kFRAMES_SECOND_TICK_BLOCK)
		{
			g_iPhase = kCD_TryLateSwitchToC;
		}
		return true;

	case kCD_TryLateSwitchToC:
		// Past the 1.5s window (~1.67s elapsed total) -- now SUCCEEDS.
		g_bLateSwitchToCOk = DP_Player::TryVoluntaryPossessSwitch(g_xC);
		g_xAfterLateAttempt = DP_Player::GetPossessedVillager();
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Cooldown: switchToB=%d earlyToC=%d midToC=%d lateToC=%d (expect 1 0 0 1) postLate=%u/%u (expect %u/%u)",
			(int)g_bSwitchToBOk, (int)g_bEarlySwitchToCOk,
			(int)g_bMidSwitchToCOk, (int)g_bLateSwitchToCOk,
			g_xAfterLateAttempt.m_uIndex, g_xAfterLateAttempt.m_uGeneration,
			g_xC.m_uIndex, g_xC.m_uGeneration);
		g_iPhase = kCD_Done;
		return false;

	case kCD_Done:
	default:
		return false;
	}
}

static bool Verify_P1Cooldown()
{
	if (!g_xA.IsValid() || !g_xB.IsValid() || !g_xC.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Cooldown: failed to pick 3 distinct villagers");
		return false;
	}
	if (!g_bSwitchToBOk)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Cooldown: initial switch A->B should have succeeded (no prior cooldown)");
		return false;
	}
	if (g_bEarlySwitchToCOk)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Cooldown: immediate switch B->C was allowed -- cooldown gate didn't fire");
		return false;
	}
	if (g_xAfterEarlyAttempt.m_uIndex != g_xB.m_uIndex
		|| g_xAfterEarlyAttempt.m_uGeneration != g_xB.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Cooldown: rejected switch should have left possession on B, but got %u/%u (B=%u/%u)",
			g_xAfterEarlyAttempt.m_uIndex, g_xAfterEarlyAttempt.m_uGeneration,
			g_xB.m_uIndex, g_xB.m_uGeneration);
		return false;
	}
	if (g_bMidSwitchToCOk)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Cooldown: mid-window switch B->C was allowed -- cooldown drained too fast");
		return false;
	}
	if (g_xAfterMidAttempt.m_uIndex != g_xB.m_uIndex
		|| g_xAfterMidAttempt.m_uGeneration != g_xB.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Cooldown: rejected mid switch should have left possession on B");
		return false;
	}
	if (!g_bLateSwitchToCOk)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Cooldown: late switch B->C past the 1.5s window was refused -- cooldown didn't drain");
		return false;
	}
	if (g_xAfterLateAttempt.m_uIndex != g_xC.m_uIndex
		|| g_xAfterLateAttempt.m_uGeneration != g_xC.m_uGeneration)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Cooldown: late switch did not change possession to C");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1CooldownTest = {
	"Test_P1Cooldown_CannotPossessFor1pt5s",
	&Setup_P1Cooldown,
	&Step_P1Cooldown,
	&Verify_P1Cooldown,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1CooldownTest);

#endif // ZENITH_INPUT_SIMULATOR
