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
// Test_P1Sprint_WinsTieOverWalkQuiet (MVP-1.7 follow-up coverage gap)
//
// MVP-1.7 has tests for sprint-on (Test_P1Sprint_DrainsLifeFaster),
// sprint-off-when-still (Test_P1Sprint_NoDrainWhenNotMoving), and
// walk-quiet loudness (Test_P1WalkQuiet_*), but NOTHING in the suite
// asserts the documented Shift+Ctrl tie-breaking semantic from the
// villager header comment:
//
//   "Sprint wins ties; walk-quiet takes the slow speed. Holding
//    Shift+Ctrl resolves to sprint -- the louder, faster mode
//    shouldn't be silenced by a held Ctrl."
//
// The relevant code in DPVillager_Behaviour::OnUpdate:
//   m_bIsSprintingNow = DP_Input::ReadSprintHeld() && bMoving;
//   m_bIsWalkQuietNow = !m_bIsSprintingNow
//       && DP_Input::ReadWalkQuietHeld() && bMoving;
//
// A regression where the `!m_bIsSprintingNow` guard got dropped, or
// the two reads got reordered (walk-quiet wins), would silently leak.
// Gameplay-wise this would mean a sprinting player who happens to be
// holding Ctrl (common keyboard ergonomics -- pinky on Ctrl, ring on
// Shift) would suddenly emit quieter footsteps AND move slower while
// still paying the sprint life cost.
//
// Procedure:
//   1. Load GameLevel + pick any villager.
//   2. SetPossessedVillager + wait one bump frame.
//   3. **Sprint+Ctrl window**: hold Shift + Ctrl + W simultaneously.
//      Tick ~30 frames. Sample IsSprintingNow + IsWalkQuietNow on a
//      mid-window frame.
//   4. **Ctrl-only sanity window**: release Shift, keep Ctrl + W.
//      Tick another ~30 frames. Sample IsSprintingNow + IsWalkQuietNow.
//      This proves walk-quiet ACTUALLY works without sprint -- so
//      the "walk-quiet=false in window 1" assertion has teeth.
//   5. Release all inputs.
//   6. Assert:
//        Window 1: sprintingNow=true, walkQuietNow=false
//        Window 2: sprintingNow=false, walkQuietNow=true
//
// What this catches:
//   * A regression where walk-quiet's input read got reordered to win
//     ties (Ctrl+Shift => walk-quiet=true, sprint=false).
//   * A regression where the `!m_bIsSprintingNow` guard inside
//     `m_bIsWalkQuietNow`'s assignment got dropped (Ctrl+Shift =>
//     BOTH true at once, which would also break the speed switch
//     in TickMovement because the if/else-if would pick sprint but
//     subsequent code paths might still see walkQuietNow=true).
// ============================================================================

namespace
{
	enum Phase : int { kTW_Start, kTW_WaitScene, kTW_Possess, kTW_AfterBump,
	                   kTW_ArmSprintWindow, kTW_TickSprintWindow,
	                   kTW_ArmCtrlOnlyWindow, kTW_TickCtrlOnlyWindow,
	                   kTW_ReleaseInputs, kTW_Verify, kTW_Done };

	int                     g_iPhase = kTW_Start;
	Zenith_EntityID         g_xVillager;
	int                     g_iTickCount = 0;
	bool                    g_bSprintingObservedShiftCtrlWindow = false;
	bool                    g_bWalkQuietObservedShiftCtrlWindow = true; // sentinel: must end up false
	bool                    g_bSprintingObservedCtrlOnlyWindow = true;  // sentinel: must end up false
	bool                    g_bWalkQuietObservedCtrlOnlyWindow = false;

	constexpr int kTICK_FRAMES = 30;

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

static void Setup_P1SprintWinsTie()
{
	g_iPhase = kTW_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_iTickCount = 0;
	g_bSprintingObservedShiftCtrlWindow = false;
	g_bWalkQuietObservedShiftCtrlWindow = true;
	g_bSprintingObservedCtrlOnlyWindow = true;
	g_bWalkQuietObservedCtrlOnlyWindow = false;
}

static bool Step_P1SprintWinsTie(int iFrame)
{
	switch (g_iPhase)
	{
	case kTW_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kTW_WaitScene;
		return true;

	case kTW_WaitScene:
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
			g_iPhase = kTW_Possess;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kTW_Done;
		}
		return true;
	}

	case kTW_Possess:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kTW_AfterBump;
		return true;

	case kTW_AfterBump:
		// One frame later OnUpdate has flipped m_bIsPossessed.
		g_iPhase = kTW_ArmSprintWindow;
		return true;

	case kTW_ArmSprintWindow:
		// Hold Shift + Ctrl + W simultaneously. Sprint MUST win the tie.
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_CONTROL, true);
		// Belt-and-braces: defensively clear other movement keys so a
		// leaked-state from a prior batched test doesn't flip the
		// movement direction (shouldn't matter for the sprint flag,
		// but cheap insurance).
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
		g_iTickCount = 0;
		g_iPhase = kTW_TickSprintWindow;
		return true;

	case kTW_TickSprintWindow:
	{
		++g_iTickCount;
		// Sample state at the mid-window frame so we skip any
		// first-frame transient (the kAfterBump frame already gave
		// OnUpdate one tick to observe possession + compute state).
		if (g_iTickCount == kTICK_FRAMES / 2)
		{
			DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
			if (pxV != nullptr)
			{
				g_bSprintingObservedShiftCtrlWindow = pxV->IsSprintingNow();
				g_bWalkQuietObservedShiftCtrlWindow = pxV->IsWalkQuietNow();
			}
		}
		if (g_iTickCount >= kTICK_FRAMES)
		{
			g_iPhase = kTW_ArmCtrlOnlyWindow;
		}
		return true;
	}

	case kTW_ArmCtrlOnlyWindow:
		// Release Shift, keep Ctrl + W. Walk-quiet should engage.
		// This window's job is to prove that walk-quiet CAN engage in
		// the absence of Shift -- so "walk-quiet=false in Shift+Ctrl"
		// can't be explained by "walk-quiet never engages anyway".
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
		// W + Ctrl still held from previous window.
		g_iTickCount = 0;
		g_iPhase = kTW_TickCtrlOnlyWindow;
		return true;

	case kTW_TickCtrlOnlyWindow:
	{
		++g_iTickCount;
		if (g_iTickCount == kTICK_FRAMES / 2)
		{
			DPVillager_Behaviour* pxV = GetVillagerBehaviour(g_xVillager);
			if (pxV != nullptr)
			{
				g_bSprintingObservedCtrlOnlyWindow = pxV->IsSprintingNow();
				g_bWalkQuietObservedCtrlOnlyWindow = pxV->IsWalkQuietNow();
			}
		}
		if (g_iTickCount >= kTICK_FRAMES)
		{
			g_iPhase = kTW_ReleaseInputs;
		}
		return true;
	}

	case kTW_ReleaseInputs:
		// Don't leak input state into the next batched test.
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_CONTROL, false);
		g_iPhase = kTW_Verify;
		return true;

	case kTW_Verify:
		std::printf("[P1SprintWinsTie] Shift+Ctrl window: sprint=%d walkQuiet=%d (expect 1/0). Ctrl-only window: sprint=%d walkQuiet=%d (expect 0/1)\n",
			(int)g_bSprintingObservedShiftCtrlWindow,
			(int)g_bWalkQuietObservedShiftCtrlWindow,
			(int)g_bSprintingObservedCtrlOnlyWindow,
			(int)g_bWalkQuietObservedCtrlOnlyWindow);
		std::fflush(stdout);
		g_iPhase = kTW_Done;
		return false;

	case kTW_Done:
	default:
		return false;
	}
}

static bool Verify_P1SprintWinsTie()
{
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1SprintWinsTie: villager not found");
		return false;
	}
	// Shift+Ctrl: sprint must win. walkQuiet must be false.
	if (!g_bSprintingObservedShiftCtrlWindow)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1SprintWinsTie: IsSprintingNow was false with Shift+Ctrl+W held -- sprint didn't engage at all (precondition failure or sprint guard misfired)");
		return false;
	}
	if (g_bWalkQuietObservedShiftCtrlWindow)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1SprintWinsTie: IsWalkQuietNow was true with Shift+Ctrl+W held -- walk-quiet won the tie. The '!m_bIsSprintingNow' guard inside m_bIsWalkQuietNow's assignment is missing or reordered. Header semantic 'sprint wins ties' is broken.");
		return false;
	}
	// Sanity window: Ctrl alone must engage walk-quiet.
	if (g_bSprintingObservedCtrlOnlyWindow)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1SprintWinsTie: Ctrl-only sanity window had IsSprintingNow=true -- Shift release didn't disengage sprint, or sprint is reading the wrong key");
		return false;
	}
	if (!g_bWalkQuietObservedCtrlOnlyWindow)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1SprintWinsTie: Ctrl-only sanity window had IsWalkQuietNow=false -- walk-quiet doesn't engage even without sprint, so the Shift+Ctrl assertion has no teeth. ReadWalkQuietHeld bug?");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1SprintWinsTieTest = {
	"Test_P1Sprint_WinsTieOverWalkQuiet",
	&Setup_P1SprintWinsTie,
	&Step_P1SprintWinsTie,
	&Verify_P1SprintWinsTie,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1SprintWinsTieTest);

#endif // ZENITH_INPUT_SIMULATOR
