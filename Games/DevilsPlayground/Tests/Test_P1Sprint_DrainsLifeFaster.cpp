#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Sprint_DrainsLifeFaster (MVP-1.7.1 + 1.7.2)
//
// Verifies MVP-1.7's sprint mechanic: while sprinting (Shift held AND
// moving), the possessed villager's life drains at
//   1.0 + movement.sprint_life_cost_extra_per_s   (1.0 + 1.5 = 2.5 s/s)
// versus the 1.0 s/s baseline when only moving (no Shift). The
// 1.5 s/s extra cost was lowered from 3.0 on 2026-05-20 after the
// personality seed-matrix showed sprint personalities completing
// objective loops at 1/3 the rate of walking ones.
//
// Procedure:
//   1. Load GameLevel, possess any villager.
//   2. Wait one bump frame so OnUpdate sets m_bIsPossessed and bumps
//      life to m_fMaxLife.
//   3. **Sprint window**: SetRemainingLifeForTest(30.0) as a baseline,
//      SimulateKeyHeld(W) + SimulateKeyHeld(LEFT_SHIFT), tick 60
//      frames (~1.0 s).  Snapshot life delta.
//   4. **Walk window**: SetRemainingLifeForTest(30.0) again,
//      SimulateKeyHeld(LEFT_SHIFT, false) but keep W held, tick 60
//      frames.  Snapshot life delta.
//   5. Assert: sprint delta > walk delta by AT LEAST 2 s (out of the
//      3 s/s extra-cost-per-second the tuning specifies; 2 s gives
//      slack against frame-time jitter / boundary frames).
// ============================================================================

namespace
{
	enum Phase : int { kSP_Start, kSP_WaitScene, kSP_Possess, kSP_AfterBump,
	                   kSP_SprintBaseline, kSP_SprintTick, kSP_SprintRecord,
	                   kSP_WalkBaseline, kSP_WalkTick, kSP_WalkRecord,
	                   kSP_Verify, kSP_Done };

	int                     g_iPhase = kSP_Start;
	Zenith_EntityID         g_xVillager;
	float                   g_fSprintBaseline = 0.0f;
	float                   g_fSprintAfter = 0.0f;
	float                   g_fWalkBaseline = 0.0f;
	float                   g_fWalkAfter = 0.0f;
	int                     g_iTickCount = 0;
	bool                    g_bSprintingObservedSprintWindow = false;
	bool                    g_bSprintingObservedWalkWindow = true; // pre-set to "fail sentinel"

	constexpr int kTICK_FRAMES = 60; // ~1.0 s at 60 Hz fixed-dt

	DPVillager_Behaviour* GetVillagerBehaviour(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
	}
}

static void Setup_P1Sprint()
{
	g_iPhase = kSP_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_fSprintBaseline = 0.0f;
	g_fSprintAfter = 0.0f;
	g_fWalkBaseline = 0.0f;
	g_fWalkAfter = 0.0f;
	g_iTickCount = 0;
	g_bSprintingObservedSprintWindow = false;
	g_bSprintingObservedWalkWindow = true;
}

static bool Step_P1Sprint(int iFrame)
{
	switch (g_iPhase)
	{
	case kSP_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kSP_WaitScene;
		return true;

	case kSP_WaitScene:
	{
		// Grab any villager -- which one doesn't matter for the sprint
		// life math.
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		if (xFound.IsValid())
		{
			g_xVillager = xFound;
			g_iPhase = kSP_Possess;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kSP_Done;
		}
		return true;
	}

	case kSP_Possess:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kSP_AfterBump;
		return true;

	case kSP_AfterBump:
		// One frame later OnUpdate has flipped m_bIsPossessed and bumped
		// remaining life to m_fMaxLife. From here on we control life via
		// SetRemainingLifeForTest.
		g_iPhase = kSP_SprintBaseline;
		return true;

	case kSP_SprintBaseline:
	{
		// Snapshot baseline + arm sprint inputs.
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
		if (pxV == nullptr) { g_iPhase = kSP_Done; return false; }
		pxV->SetRemainingLifeForTest(30.0f);
		g_fSprintBaseline = pxV->GetRemainingLife();
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
		g_iTickCount = 0;
		g_iPhase = kSP_SprintTick;
		return true;
	}

	case kSP_SprintTick:
	{
		++g_iTickCount;
		// Sample m_bIsSprintingNow at least once during the window so
		// the verify step can fail noisily if the sprint state machine
		// didn't actually flip on. We check on a mid-window tick so we
		// don't grab the first-frame transient.
		if (g_iTickCount == kTICK_FRAMES / 2)
		{
			DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
			if (pxV != nullptr) g_bSprintingObservedSprintWindow = pxV->IsSprintingNow();
		}
		if (g_iTickCount >= kTICK_FRAMES)
		{
			g_iPhase = kSP_SprintRecord;
		}
		return true;
	}

	case kSP_SprintRecord:
	{
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
		if (pxV == nullptr) { g_iPhase = kSP_Done; return false; }
		g_fSprintAfter = pxV->GetRemainingLife();
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
		// Leave W held -- walk window keeps the movement.
		g_iPhase = kSP_WalkBaseline;
		return true;
	}

	case kSP_WalkBaseline:
	{
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
		if (pxV == nullptr) { g_iPhase = kSP_Done; return false; }
		pxV->SetRemainingLifeForTest(30.0f);
		g_fWalkBaseline = pxV->GetRemainingLife();
		g_iTickCount = 0;
		g_iPhase = kSP_WalkTick;
		return true;
	}

	case kSP_WalkTick:
	{
		++g_iTickCount;
		if (g_iTickCount == kTICK_FRAMES / 2)
		{
			DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
			if (pxV != nullptr) g_bSprintingObservedWalkWindow = pxV->IsSprintingNow();
		}
		if (g_iTickCount >= kTICK_FRAMES)
		{
			g_iPhase = kSP_WalkRecord;
		}
		return true;
	}

	case kSP_WalkRecord:
	{
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
		if (pxV == nullptr) { g_iPhase = kSP_Done; return false; }
		g_fWalkAfter = pxV->GetRemainingLife();
		// Release W so we don't leak input state into a subsequent
		// batched test.
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		g_iPhase = kSP_Verify;
		return true;
	}

	case kSP_Verify:
	{
		const float fSprintDrop = g_fSprintBaseline - g_fSprintAfter;
		const float fWalkDrop = g_fWalkBaseline - g_fWalkAfter;
		std::printf("[P1Sprint] sprintDrop=%.3fs walkDrop=%.3fs diff=%.3fs sprintObs=%d walkObs=%d\n",
			fSprintDrop, fWalkDrop, fSprintDrop - fWalkDrop,
			(int)g_bSprintingObservedSprintWindow,
			(int)g_bSprintingObservedWalkWindow);
		std::fflush(stdout);
		g_iPhase = kSP_Done;
		return false;
	}

	case kSP_Done:
	default:
		return false;
	}
}

static bool Verify_P1Sprint()
{
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Sprint: villager not found");
		return false;
	}
	if (!g_bSprintingObservedSprintWindow)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Sprint: IsSprintingNow was false during sprint window -- state machine didn't engage");
		return false;
	}
	if (g_bSprintingObservedWalkWindow)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Sprint: IsSprintingNow was true during walk window -- Shift release didn't disengage sprint");
		return false;
	}
	const float fSprintDrop = g_fSprintBaseline - g_fSprintAfter;
	const float fWalkDrop = g_fWalkBaseline - g_fWalkAfter;
	if (fWalkDrop <= 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Sprint: walk window had no life drop -- TickLife didn't run");
		return false;
	}
	// Expected difference: ~1.0 s/s extra cost over a 1 s window = 1.0 s
	// (was 3.0 s/s -> 1.5 s/s -> 1.0 s/s; tuning was dropped further on
	// 2026-05-21 as part of the game-balance pass that targets >0% and
	// <100% win rate per personality + >=1 win per procgen seed). The
	// test still asserts sprint drains MORE than walk; the magnitude
	// floor is just lower to match the rebalanced cost.
	// Allow ~30% slack (0.7 s minimum) for frame-time / boundary jitter.
	const float fDiff = fSprintDrop - fWalkDrop;
	const float fMinDiff = 0.7f;
	if (fDiff < fMinDiff)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Sprint: sprint drop (%.3fs) - walk drop (%.3fs) = %.3fs, expected >= %.3fs",
			fSprintDrop, fWalkDrop, fDiff, fMinDiff);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1SprintTest = {
	"Test_P1Sprint_DrainsLifeFaster",
	&Setup_P1Sprint,
	&Step_P1Sprint,
	&Verify_P1Sprint,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1SprintTest);

#endif // ZENITH_INPUT_SIMULATOR
