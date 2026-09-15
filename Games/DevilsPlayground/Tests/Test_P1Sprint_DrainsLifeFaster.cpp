#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Component.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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
	                   kSP_BurnoutBaseline, kSP_BurnoutRecord, kSP_Verify, kSP_Done };

	int                     g_iPhase = kSP_Start;
	Zenith_EntityID         g_xVillager;
	float                   g_fSprintBaseline = 0.0f;
	float                   g_fSprintAfter = 0.0f;
	float                   g_fWalkBaseline = 0.0f;
	float                   g_fWalkAfter = 0.0f;
	float                   g_fExpectedSprintDrain = 0.0f;
	float                   g_fPublishedSprintDrain1 = 0.0f;
	float                   g_fPublishedSprintDrain2 = 0.0f;
	float                   g_fPublishedPlainDrain = 0.0f;
	float                   g_fLifeAfterSprintDrain1 = 0.0f;
	float                   g_fLifeAfterSprintDrain2 = 0.0f;
	float                   g_fLifeAfterPlainDrain = 0.0f;
	int                     g_iTickCount = 0;
	bool                    g_bSprintingObservedSprintWindow = false;
	bool                    g_bSprintingObservedWalkWindow = true; // pre-set to "fail sentinel"
	bool                    g_bPossessionFactAfterTransition = false;
	bool                    g_bBurnoutKeptPossessionFact = false;
	bool                    g_bBurnoutReachedDead = false;
	bool                    g_bDeadTickClearedPossessionFact = false;

	constexpr int kTICK_FRAMES = 60; // ~1.0 s at 60 Hz fixed-dt
	constexpr float kFIXED_DT = 1.0f / 60.0f;

	DPVillager_Component* GetVillagerBehaviour(Zenith_EntityID xId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		return xEnt.TryGetComponent<DPVillager_Component>();
	}

	Zenith_BehaviourGraph* GetVillagerGraph(Zenith_EntityID xId)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		Zenith_GraphComponent* pxGraphs = xEnt.IsValid()
			? xEnt.TryGetComponent<Zenith_GraphComponent>() : nullptr;
		if (pxGraphs == nullptr) return nullptr;
		for (u_int u = 0; u < pxGraphs->GetGraphCount(); ++u)
		{
			if (std::strcmp(pxGraphs->GetGraphAssetPathAt(u), DPVillager_Component::kszGraphAsset) == 0)
			{
				return pxGraphs->GetGraphAt(u);
			}
		}
		return nullptr;
	}
}

static void Setup_P1Sprint()
{
	Zenith_InputSimulator::SetFixedDt(kFIXED_DT);
	g_iPhase = kSP_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_fSprintBaseline = 0.0f;
	g_fSprintAfter = 0.0f;
	g_fWalkBaseline = 0.0f;
	g_fWalkAfter = 0.0f;
	g_fExpectedSprintDrain = 0.0f;
	g_fPublishedSprintDrain1 = 0.0f;
	g_fPublishedSprintDrain2 = 0.0f;
	g_fPublishedPlainDrain = 0.0f;
	g_fLifeAfterSprintDrain1 = 0.0f;
	g_fLifeAfterSprintDrain2 = 0.0f;
	g_fLifeAfterPlainDrain = 0.0f;
	g_iTickCount = 0;
	g_bSprintingObservedSprintWindow = false;
	g_bSprintingObservedWalkWindow = true;
	g_bPossessionFactAfterTransition = false;
	g_bBurnoutKeptPossessionFact = false;
	g_bBurnoutReachedDead = false;
	g_bDeadTickClearedPossessionFact = false;
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
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xFound](Zenith_EntityID xId, DPVillager_Component&)
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
		if (Zenith_BehaviourGraph* pxGraph = GetVillagerGraph(g_xVillager))
		{
			g_bPossessionFactAfterTransition = pxGraph->GetBlackboard().GetBool("stateIsPossessed", false);
		}
		g_iPhase = kSP_SprintBaseline;
		return true;

	case kSP_SprintBaseline:
	{
		// Snapshot baseline + arm sprint inputs.
		DPVillager_Component* pxV = GetVillagerBehaviour(g_xVillager);
		if (pxV == nullptr) { g_iPhase = kSP_Done; return false; }
		pxV->SetRemainingLifeForTest(30.0f);
		g_fSprintBaseline = pxV->GetRemainingLife();
		Zenith_BehaviourGraph* pxGraph = GetVillagerGraph(g_xVillager);
		if (pxGraph == nullptr) { g_iPhase = kSP_Done; return false; }
		g_fExpectedSprintDrain = kFIXED_DT * (1.0f + pxGraph->GetBlackboard().GetFloat("sprintCostExtra", 0.0f));
		// ★ C1a -- EDGES, not SetKeyHeld. MOVE and SPRINT are C2 actions now
		// (DP_Bindings.h), and their key rows are fed by the ORDERED transition
		// log, not sampled as a level. SetKeyHeld writes only the simulator's
		// held table, which reaches IsKeyDown and nothing else: the action layer
		// would read "not held" for the whole window and the drain would look
		// like a plain walk. SimulateKeyDown/Up queue real transitions.
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_LEFT_SHIFT);
		g_iTickCount = 0;
		g_iPhase = kSP_SprintTick;
		return true;
	}

	case kSP_SprintTick:
	{
		++g_iTickCount;
		if (g_iTickCount <= 2)
		{
			DPVillager_Component* pxV = GetVillagerBehaviour(g_xVillager);
			Zenith_BehaviourGraph* pxGraph = GetVillagerGraph(g_xVillager);
			if (pxV == nullptr || pxGraph == nullptr) { g_iPhase = kSP_Done; return false; }
			const float fPublishedDrain = pxGraph->GetBlackboard().GetFloat("drain", 0.0f);
			if (g_iTickCount == 1)
			{
				g_fPublishedSprintDrain1 = fPublishedDrain;
				g_fLifeAfterSprintDrain1 = pxV->GetRemainingLife();
			}
			else
			{
				g_fPublishedSprintDrain2 = fPublishedDrain;
				g_fLifeAfterSprintDrain2 = pxV->GetRemainingLife();
			}
		}
		// Sample m_bIsSprintingNow at least once during the window so
		// the verify step can fail noisily if the sprint state machine
		// didn't actually flip on. We check on a mid-window tick so we
		// don't grab the first-frame transient.
		if (g_iTickCount == kTICK_FRAMES / 2)
		{
			DPVillager_Component* pxV = GetVillagerBehaviour(g_xVillager);
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
		DPVillager_Component* pxV = GetVillagerBehaviour(g_xVillager);
		if (pxV == nullptr) { g_iPhase = kSP_Done; return false; }
		g_fSprintAfter = pxV->GetRemainingLife();
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_LEFT_SHIFT);
		// Leave W held -- walk window keeps the movement.
		g_iPhase = kSP_WalkBaseline;
		return true;
	}

	case kSP_WalkBaseline:
	{
		DPVillager_Component* pxV = GetVillagerBehaviour(g_xVillager);
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
		if (g_iTickCount == 1)
		{
			DPVillager_Component* pxV = GetVillagerBehaviour(g_xVillager);
			Zenith_BehaviourGraph* pxGraph = GetVillagerGraph(g_xVillager);
			if (pxV == nullptr || pxGraph == nullptr) { g_iPhase = kSP_Done; return false; }
			g_fPublishedPlainDrain = pxGraph->GetBlackboard().GetFloat("drain", 0.0f);
			g_fLifeAfterPlainDrain = pxV->GetRemainingLife();
		}
		if (g_iTickCount == kTICK_FRAMES / 2)
		{
			DPVillager_Component* pxV = GetVillagerBehaviour(g_xVillager);
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
		DPVillager_Component* pxV = GetVillagerBehaviour(g_xVillager);
		if (pxV == nullptr) { g_iPhase = kSP_Done; return false; }
		g_fWalkAfter = pxV->GetRemainingLife();
		// Release W so we don't leak input state into a subsequent
		// batched test.
		Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
		g_iPhase = kSP_BurnoutBaseline;
		return true;
	}

	case kSP_BurnoutBaseline:
	{
		DPVillager_Component* pxV = GetVillagerBehaviour(g_xVillager);
		if (pxV == nullptr) { g_iPhase = kSP_Done; return false; }
		// The next graph tick must publish the possession fact before its
		// drain tail kills this deliberately exhausted villager.
		pxV->SetRemainingLifeForTest(kFIXED_DT * 0.5f);
		g_iPhase = kSP_BurnoutRecord;
		return true;
	}

	case kSP_BurnoutRecord:
	{
		DPVillager_Component* pxV = GetVillagerBehaviour(g_xVillager);
		Zenith_BehaviourGraph* pxGraph = GetVillagerGraph(g_xVillager);
		if (pxV == nullptr || pxGraph == nullptr) { g_iPhase = kSP_Done; return false; }
		g_bBurnoutReachedDead = (pxV->GetState() == DPVillagerState::Dead);
		g_bBurnoutKeptPossessionFact = pxGraph->GetBlackboard().GetBool("stateIsPossessed", false);
		g_iPhase = kSP_Verify;
		return true;
	}

	case kSP_Verify:
	{
		if (Zenith_BehaviourGraph* pxGraph = GetVillagerGraph(g_xVillager))
		{
			g_bDeadTickClearedPossessionFact = !pxGraph->GetBlackboard().GetBool("stateIsPossessed", true);
		}
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
	Zenith_InputSimulator::ClearFixedDt();
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
	if (!g_bPossessionFactAfterTransition)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Sprint: stateIsPossessed was not published after possession transition");
		return false;
	}
	const float fEpsilon = 0.0001f;
	if (std::fabs(g_fPublishedSprintDrain1 - g_fExpectedSprintDrain) > fEpsilon
		|| std::fabs(g_fPublishedSprintDrain2 - g_fExpectedSprintDrain) > fEpsilon)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1Sprint: two sprint drain publications were %.6f and %.6f, expected %.6f",
			g_fPublishedSprintDrain1, g_fPublishedSprintDrain2, g_fExpectedSprintDrain);
		return false;
	}
	if (std::fabs((g_fSprintBaseline - g_fLifeAfterSprintDrain1) - g_fExpectedSprintDrain) > fEpsilon
		|| std::fabs((g_fLifeAfterSprintDrain1 - g_fLifeAfterSprintDrain2) - g_fExpectedSprintDrain) > fEpsilon)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Sprint: sprint life decrement did not match each published drain without accumulation");
		return false;
	}
	if (std::fabs(g_fPublishedPlainDrain - kFIXED_DT) > fEpsilon)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Sprint: plain drain publication %.6f did not reset to dt %.6f",
			g_fPublishedPlainDrain, kFIXED_DT);
		return false;
	}
	if (std::fabs((g_fWalkBaseline - g_fLifeAfterPlainDrain) - kFIXED_DT) > fEpsilon)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Sprint: first plain life decrement did not match fixed dt");
		return false;
	}
	if (!g_bBurnoutReachedDead || !g_bBurnoutKeptPossessionFact || !g_bDeadTickClearedPossessionFact)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1Sprint: burnout fact did not remain true through death then clear on the following dead tick");
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
