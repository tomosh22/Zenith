#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1Sprint_NoDrainWhenNotMoving (MVP-1.7.3)
//
// Negative-case sibling to Test_P1Sprint_DrainsLifeFaster: holding
// Shift while NOT moving must NOT incur the sprint life cost. The
// guard lives in DPVillager_Behaviour::OnUpdate, where
// m_bIsSprintingNow is computed as `ReadSprintHeld() && moving`. If
// the guard's "moving" check got dropped (e.g., during a refactor),
// standing-still players would burn life for nothing, breaking
// MVPScope's promise that Shift is a no-op without movement input.
//
// Procedure:
//   1. Load GameLevel, possess any villager.
//   2. Wait one bump frame.
//   3. SetRemainingLifeForTest(30.0).
//   4. SimulateKeyHeld(LEFT_SHIFT) -- shift held but NO movement key.
//   5. Tick 60 frames (~1.0 s).
//   6. Snapshot life delta.
//   7. Assert: drop is approximately 1.0 s (baseline only -- no
//      sprint cost added). Tolerance generous because exact-baseline
//      assertions are covered by LifeTimer_Test; here we only need
//      to prove the sprint extra cost did NOT fire.
//   8. Also assert IsSprintingNow() returned false during the window.
// ============================================================================

namespace
{
	enum Phase : int { kSN_Start, kSN_WaitScene, kSN_Possess, kSN_AfterBump,
	                   kSN_ArmInputs, kSN_Tick, kSN_Verify, kSN_Done };

	int                     g_iPhase = kSN_Start;
	Zenith_EntityID         g_xVillager;
	float                   g_fBaseline = 0.0f;
	float                   g_fAfter = 0.0f;
	bool                    g_bSprintingObservedMidWindow = true;
	int                     g_iTickCount = 0;

	constexpr int kTICK_FRAMES = 60;

	DPVillager_Behaviour* GetVillagerBehaviour(Zenith_EntityID xId)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return nullptr;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return nullptr;
		if (!xEnt.HasComponent<Zenith_ScriptComponent>()) return nullptr;
		return xEnt.GetComponent<Zenith_ScriptComponent>().GetScript<DPVillager_Behaviour>();
	}
}

static void Setup_P1SprintNoDrain()
{
	g_iPhase = kSN_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_fBaseline = 0.0f;
	g_fAfter = 0.0f;
	g_bSprintingObservedMidWindow = true; // sentinel: must end up false
	g_iTickCount = 0;
}

static bool Step_P1SprintNoDrain(int iFrame)
{
	switch (g_iPhase)
	{
	case kSN_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kSN_WaitScene;
		return true;

	case kSN_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		if (xFound.IsValid())
		{
			g_xVillager = xFound;
			g_iPhase = kSN_Possess;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kSN_Done;
		}
		return true;
	}

	case kSN_Possess:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kSN_AfterBump;
		return true;

	case kSN_AfterBump:
		g_iPhase = kSN_ArmInputs;
		return true;

	case kSN_ArmInputs:
	{
		DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
		if (pxV == nullptr) { g_iPhase = kSN_Done; return false; }
		pxV->SetRemainingLifeForTest(30.0f);
		g_fBaseline = pxV->GetRemainingLife();
		// Shift held but NO movement key. The guard in OnUpdate
		// (m_bIsSprintingNow = Shift && moving) should leave it false.
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
		// Make absolutely sure no movement keys are held from a prior
		// batched test (the InputSimulator's between-tests reset
		// should cover this, but belt-and-braces).
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
		g_iTickCount = 0;
		g_iPhase = kSN_Tick;
		return true;
	}

	case kSN_Tick:
	{
		++g_iTickCount;
		if (g_iTickCount == kTICK_FRAMES / 2)
		{
			DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
			if (pxV != nullptr) g_bSprintingObservedMidWindow = pxV->IsSprintingNow();
		}
		if (g_iTickCount >= kTICK_FRAMES)
		{
			DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
			if (pxV != nullptr) g_fAfter = pxV->GetRemainingLife();
			Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
			g_iPhase = kSN_Verify;
		}
		return true;
	}

	case kSN_Verify:
	{
		const float fDrop = g_fBaseline - g_fAfter;
		std::printf("[P1SprintNoDrain] drop=%.3fs (expect ~1.0s, def NOT ~4.0s) sprintObs=%d (expect 0)\n",
			fDrop, (int)g_bSprintingObservedMidWindow);
		std::fflush(stdout);
		g_iPhase = kSN_Done;
		return false;
	}

	case kSN_Done:
	default:
		return false;
	}
}

static bool Verify_P1SprintNoDrain()
{
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1SprintNoDrain: villager not found");
		return false;
	}
	if (g_bSprintingObservedMidWindow)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1SprintNoDrain: IsSprintingNow() returned true with no movement input -- guard misfired");
		return false;
	}
	const float fDrop = g_fBaseline - g_fAfter;
	// Expected ~1.0 s baseline drop over 1 s. Allow 0.5 s tolerance
	// on the high side -- but anything >= 2.0 s implies the sprint
	// cost (3 s/s) leaked in for at least part of the window.
	const float fMaxAllowedDrop = 2.0f;
	if (fDrop >= fMaxAllowedDrop)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1SprintNoDrain: life drop %.3fs is too high (limit %.3fs) -- sprint cost leaked through the no-movement guard",
			fDrop, fMaxAllowedDrop);
		return false;
	}
	if (fDrop <= 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1SprintNoDrain: no life drop at all -- TickLife didn't fire");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1SprintNoDrainTest = {
	"Test_P1Sprint_NoDrainWhenNotMoving",
	&Setup_P1SprintNoDrain,
	&Step_P1SprintNoDrain,
	&Verify_P1SprintNoDrain,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1SprintNoDrainTest);

#endif // ZENITH_INPUT_SIMULATOR
