#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_ExplorationCharacterization - characterization tests for the wave-2 graph
 * conversion of Exploration's decision logic (menu/play flow, Escape->menu, Tab
 * debug-toggle, day/night time-advance, weather FSM, and the sun/fog read-after-
 * write ordering the ApplyAtmosphere split must preserve).
 *
 * Written against the C++ versions FIRST (Exploration_GameComponent::OnUpdate's
 * MAIN_MENU/PLAYING switch, OnPlayClicked/ReturnToMenu, the Tab check ->
 * Exploration_UIManager::ToggleDebugHUD, Exploration_PlayerController::Update, and
 * Exploration_AtmosphereController::Update = time-advance[A] + UpdateWeather[B] +
 * sun/fog math + ApplyToEngine[C-I]); the behaviour-graph versions must keep every
 * one of these green UNCHANGED. All probes go through surfaces that survive the
 * conversion: the live component's read-only observers (game state, atmosphere
 * statics, debug bool, camera position) and the real input path (Zenith_InputSimulator).
 *
 * SYSTEMS stay C++ (FPS movement simulation, terrain sampling, sun/fog math,
 * ApplyToEngine/Flux, HUD text, FPS counter); only the DECISIONS move to graphs.
 *
 *   Exploration_MenuPlay_Test        - from MAIN_MENU, clicking Play loads the
 *                                      gameplay scene and starts the world (PLAYING).
 *   Exploration_ReturnToMenu_Test    - Escape during play returns to MAIN_MENU.
 *   Exploration_DebugToggle_Test     - Tab flips the debug-HUD visibility bool, and
 *                                      flips it back (same-frame toggle -> UI read).
 *   Exploration_PlayerMovement_Test  - holding W advances the camera along +Z with
 *                                      terrain-follow snapping Y (the whole PLAYING
 *                                      movement loop still runs after conversion).
 *   Exploration_DayNightAndSun_Test  - time-of-day advances (cycle on), freezes
 *                                      (cycle off), wraps at 1.0, AND the sun
 *                                      direction/intensity/fog match the pure
 *                                      Calculate* of the SET time-of-day (locks the
 *                                      A->B->ApplyAtmosphere read-after-write order).
 *   Exploration_Weather_Test         - a forced weather transition advances the
 *                                      transition fraction and interpolates fog
 *                                      density toward the new weather (step B->H).
 *   Exploration_CrossSceneAtmos_Test - play->Esc->play: Configure resets time-of-day
 *                                      to the start value but the weather timer
 *                                      PERSISTS (the cross-scene global-state seam).
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Components/Exploration_GameComponent.h"
#include "Components/Exploration_AtmosphereController.h"
#include "Components/Exploration_TerrainExplorer.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "Maths/Zenith_Maths.h"

#include <cmath>

namespace
{
	using EG = Exploration_GameComponent;

	bool IsPlaying()
	{
		return EG::Test_GetLiveInstance() != nullptr
			&& EG::Test_GetLiveGameState() == ExplorationGameState::PLAYING;
	}

	bool IsMenu()
	{
		return EG::Test_GetLiveGameState() == ExplorationGameState::MAIN_MENU;
	}

	Zenith_UI::Zenith_UIButton* FindMenuPlayButton()
	{
		Zenith_UI::Zenith_UIButton* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Zenith_UIComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Zenith_UIComponent& xUI)
			{
				if (pxFound == nullptr)
					pxFound = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			});
		return pxFound;
	}

	// Every test enters PLAYING at least once; skip the 10k-tree spawn to keep the
	// repeated StartGame cost down (behaviour-neutral for every converted decision).
	void EnterReducedWorld()
	{
		Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
		EG::Test_SetReducedWorld(true);
	}

	void LeaveReducedWorld()
	{
		Zenith_InputSimulator::ClearFixedDt();
		EG::Test_SetReducedWorld(false);
	}
}

// ============================================================================
// Exploration_MenuPlay_Test
// ============================================================================

namespace
{
	enum class MPlayPhase { Boot, WaitMenu, ClickPlay, AwaitPlaying, Done };

	MPlayPhase g_eMPlay = MPlayPhase::Boot;
	int        g_iMPlayFrame = 0;
	bool       g_bMPlayStarted = false;
	bool       g_bMPlayDone = false;
}

static void Setup_MenuPlay()
{
	EnterReducedWorld();
	g_eMPlay = MPlayPhase::Boot;
	g_iMPlayFrame = 0;
	g_bMPlayStarted = false;
	g_bMPlayDone = false;
}

static bool Step_MenuPlay(int iFrame)
{
	switch (g_eMPlay)
	{
	case MPlayPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eMPlay = MPlayPhase::WaitMenu;
		return true;

	case MPlayPhase::WaitMenu:
		if (IsMenu() && FindMenuPlayButton() != nullptr)
		{
			g_eMPlay = MPlayPhase::ClickPlay;
			return true;
		}
		return iFrame < 600;

	case MPlayPhase::ClickPlay:
	{
		Zenith_UI::Zenith_UIButton* pxPlay = FindMenuPlayButton();
		if (pxPlay == nullptr) return false;
		pxPlay->Activate();
		g_iMPlayFrame = 0;
		g_eMPlay = MPlayPhase::AwaitPlaying;
		return true;
	}

	case MPlayPhase::AwaitPlaying:
		if (IsPlaying())
		{
			g_bMPlayStarted = true;
			g_bMPlayDone = true;
			g_eMPlay = MPlayPhase::Done;
			return false;
		}
		if (++g_iMPlayFrame > 900) { g_bMPlayDone = true; g_eMPlay = MPlayPhase::Done; return false; }
		return true;

	case MPlayPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_MenuPlay()
{
	LeaveReducedWorld();
	if (!g_bMPlayDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MenuPlay] never completed (phase %d)", static_cast<int>(g_eMPlay));
		return false;
	}
	if (!g_bMPlayStarted)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MenuPlay] clicking Play did not start the world");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xMenuPlayTest = {
	"Exploration_MenuPlay_Test",
	&Setup_MenuPlay,
	&Step_MenuPlay,
	&Verify_MenuPlay,
	/*maxFrames*/ 1600,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMenuPlayTest);

// ============================================================================
// Exploration_ReturnToMenu_Test
// ============================================================================

namespace
{
	enum class RetPhase { Boot, WaitPlaying, PressEsc, AwaitMenu, Done };

	RetPhase g_eRet = RetPhase::Boot;
	int      g_iRetFrame = 0;
	bool     g_bRetSawMenu = false;
	bool     g_bRetDone = false;
}

static void Setup_ReturnToMenu()
{
	EnterReducedWorld();
	g_eRet = RetPhase::Boot;
	g_iRetFrame = 0;
	g_bRetSawMenu = false;
	g_bRetDone = false;
}

static bool Step_ReturnToMenu(int iFrame)
{
	switch (g_eRet)
	{
	case RetPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRet = RetPhase::WaitPlaying;
		return true;

	case RetPhase::WaitPlaying:
		if (IsPlaying())
		{
			g_eRet = RetPhase::PressEsc;
			return true;
		}
		return iFrame < 900;

	case RetPhase::PressEsc:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iRetFrame = 0;
		g_eRet = RetPhase::AwaitMenu;
		return true;

	case RetPhase::AwaitMenu:
		// Escape unloads the world scene and single-loads the menu scene, whose
		// MenuManager wakes in MAIN_MENU (the new live instance).
		if (IsMenu() && FindMenuPlayButton() != nullptr)
		{
			g_bRetSawMenu = true;
			g_bRetDone = true;
			g_eRet = RetPhase::Done;
			return false;
		}
		if (++g_iRetFrame > 600) { g_bRetDone = true; g_eRet = RetPhase::Done; return false; }
		return true;

	case RetPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_ReturnToMenu()
{
	LeaveReducedWorld();
	if (!g_bRetDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ReturnToMenu] never completed (phase %d)", static_cast<int>(g_eRet));
		return false;
	}
	if (!g_bRetSawMenu)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ReturnToMenu] Escape did not return to MAIN_MENU (state %d)",
			static_cast<int>(EG::Test_GetLiveGameState()));
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xReturnToMenuTest = {
	"Exploration_ReturnToMenu_Test",
	&Setup_ReturnToMenu,
	&Step_ReturnToMenu,
	&Verify_ReturnToMenu,
	/*maxFrames*/ 1600,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xReturnToMenuTest);

// ============================================================================
// Exploration_DebugToggle_Test
// ============================================================================

namespace
{
	enum class DbgPhase { Boot, WaitPlaying, Record, PressTab1, AwaitOn, PressTab2, AwaitOff, Done };

	DbgPhase g_eDbg = DbgPhase::Boot;
	int      g_iDbgFrame = 0;
	bool     g_bDbgBefore = false;
	bool     g_bDbgAfterOn = false;
	bool     g_bDbgAfterOff = false;
	bool     g_bDbgDone = false;
}

static void Setup_DebugToggle()
{
	EnterReducedWorld();
	g_eDbg = DbgPhase::Boot;
	g_iDbgFrame = 0;
	g_bDbgBefore = false;
	g_bDbgAfterOn = false;
	g_bDbgAfterOff = false;
	g_bDbgDone = false;
}

static bool Step_DebugToggle(int iFrame)
{
	switch (g_eDbg)
	{
	case DbgPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eDbg = DbgPhase::WaitPlaying;
		return true;

	case DbgPhase::WaitPlaying:
		if (IsPlaying())
		{
			g_eDbg = DbgPhase::Record;
			return true;
		}
		return iFrame < 900;

	case DbgPhase::Record:
		g_bDbgBefore = EG::Test_IsDebugHUDVisible();
		g_eDbg = DbgPhase::PressTab1;
		return true;

	case DbgPhase::PressTab1:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_TAB);
		g_iDbgFrame = 0;
		g_eDbg = DbgPhase::AwaitOn;
		return true;

	case DbgPhase::AwaitOn:
		if (++g_iDbgFrame < 3) return true;
		g_bDbgAfterOn = EG::Test_IsDebugHUDVisible();
		g_eDbg = DbgPhase::PressTab2;
		return true;

	case DbgPhase::PressTab2:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_TAB);
		g_iDbgFrame = 0;
		g_eDbg = DbgPhase::AwaitOff;
		return true;

	case DbgPhase::AwaitOff:
		if (++g_iDbgFrame < 3) return true;
		g_bDbgAfterOff = EG::Test_IsDebugHUDVisible();
		g_bDbgDone = true;
		g_eDbg = DbgPhase::Done;
		return false;

	case DbgPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_DebugToggle()
{
	LeaveReducedWorld();
	if (!g_bDbgDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DebugToggle] never completed (phase %d)", static_cast<int>(g_eDbg));
		return false;
	}
	// Tab flips the bool, and a second Tab flips it back.
	if (g_bDbgAfterOn == g_bDbgBefore)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DebugToggle] first Tab did not flip debug HUD (before=%d after=%d)",
			g_bDbgBefore ? 1 : 0, g_bDbgAfterOn ? 1 : 0);
		return false;
	}
	if (g_bDbgAfterOff != g_bDbgBefore)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DebugToggle] second Tab did not restore debug HUD (before=%d afterOff=%d)",
			g_bDbgBefore ? 1 : 0, g_bDbgAfterOff ? 1 : 0);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xDebugToggleTest = {
	"Exploration_DebugToggle_Test",
	&Setup_DebugToggle,
	&Step_DebugToggle,
	&Verify_DebugToggle,
	/*maxFrames*/ 1200,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDebugToggleTest);

// ============================================================================
// Exploration_PlayerMovement_Test
// ============================================================================

namespace
{
	enum class MovePhase { Boot, WaitPlaying, Settle, Record, HoldW, Measure, Done };

	MovePhase g_eMove = MovePhase::Boot;
	int       g_iMoveFrame = 0;
	Zenith_Maths::Vector3 g_xMoveBefore(0.f);
	Zenith_Maths::Vector3 g_xMoveAfter(0.f);
	bool      g_bMoveGotBefore = false;
	bool      g_bMoveGotAfter = false;
	bool      g_bMoveDone = false;
}

static void Setup_PlayerMovement()
{
	EnterReducedWorld();
	g_eMove = MovePhase::Boot;
	g_iMoveFrame = 0;
	g_xMoveBefore = Zenith_Maths::Vector3(0.f);
	g_xMoveAfter = Zenith_Maths::Vector3(0.f);
	g_bMoveGotBefore = false;
	g_bMoveGotAfter = false;
	g_bMoveDone = false;
}

static bool Step_PlayerMovement(int iFrame)
{
	switch (g_eMove)
	{
	case MovePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eMove = MovePhase::WaitPlaying;
		return true;

	case MovePhase::WaitPlaying:
		if (IsPlaying())
		{
			g_iMoveFrame = 0;
			g_eMove = MovePhase::Settle;
			return true;
		}
		return iFrame < 900;

	case MovePhase::Settle:
		// Let the grounded-snap settle the camera onto the terrain (no input).
		if (++g_iMoveFrame >= 5)
			g_eMove = MovePhase::Record;
		return true;

	case MovePhase::Record:
		g_bMoveGotBefore = EG::Test_GetCameraPosition(g_xMoveBefore);
		g_iMoveFrame = 0;
		g_eMove = MovePhase::HoldW;
		return true;

	case MovePhase::HoldW:
		// Hold W for 60 frames (WASD movement runs unconditionally; no mouse capture
		// needed). Default camera yaw = 0 -> forward = +Z, so W advances camera Z.
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
		if (++g_iMoveFrame >= 60)
		{
			Zenith_InputSimulator::SimulateKeyUp(ZENITH_KEY_W);
			g_eMove = MovePhase::Measure;
		}
		return true;

	case MovePhase::Measure:
		g_bMoveGotAfter = EG::Test_GetCameraPosition(g_xMoveAfter);
		g_bMoveDone = true;
		g_eMove = MovePhase::Done;
		return false;

	case MovePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PlayerMovement()
{
	LeaveReducedWorld();
	if (!g_bMoveDone || !g_bMoveGotBefore || !g_bMoveGotAfter)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerMovement] never completed / no camera (phase %d)", static_cast<int>(g_eMove));
		return false;
	}
	// W advanced the camera along +Z (moveSpeed 10 * ~1s -> ~+10; require a clear delta).
	const float fDeltaZ = g_xMoveAfter.z - g_xMoveBefore.z;
	if (fDeltaZ < 3.0f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerMovement] W did not advance camera +Z (dz=%.3f)", fDeltaZ);
		return false;
	}
	// Terrain-follow: camera Y stays a small eye-height above the sampled terrain.
	const float fTerrain = Exploration_TerrainExplorer::GetTerrainHeightAt(g_xMoveAfter.x, g_xMoveAfter.z);
	const float fAboveTerrain = g_xMoveAfter.y - fTerrain;
	if (fAboveTerrain < 0.0f || fAboveTerrain > 5.0f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[PlayerMovement] camera not terrain-following (y=%.2f terrain=%.2f above=%.2f)",
			g_xMoveAfter.y, fTerrain, fAboveTerrain);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPlayerMovementTest = {
	"Exploration_PlayerMovement_Test",
	&Setup_PlayerMovement,
	&Step_PlayerMovement,
	&Verify_PlayerMovement,
	/*maxFrames*/ 1200,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPlayerMovementTest);

// ============================================================================
// Exploration_DayNightAndSun_Test
//   advance (cycle on) -> freeze (cycle off) -> wrap at 1.0 -> sun/fog math from
//   the SET time-of-day (the ApplyAtmosphere read-after-write ordering lock).
// ============================================================================

namespace
{
	enum class DNPhase { Boot, WaitPlaying, AdvA, AdvB, FreezeA, FreezeB, WrapSet, WrapWait,
	                     SunSet, SunWait, Done };

	DNPhase g_eDN = DNPhase::Boot;
	int     g_iDNFrame = 0;
	float   g_fDNAdvT0 = 0.f;
	float   g_fDNAdvT1 = 0.f;
	float   g_fDNFreezeT0 = 0.f;
	float   g_fDNFreezeT1 = 0.f;
	float   g_fDNWrapT = 1.f;
	// sun/fog math results at time-of-day 0.25
	float                 g_fDNSunIntensity = -1.f;
	Zenith_Maths::Vector3 g_xDNSunDir(0.f);
	Zenith_Maths::Vector3 g_xDNFogColor(0.f);
	bool    g_bDNDone = false;

	constexpr float kSunTestTime = 0.25f;
}

static void Setup_DayNightAndSun()
{
	EnterReducedWorld();
	g_eDN = DNPhase::Boot;
	g_iDNFrame = 0;
	g_fDNAdvT0 = g_fDNAdvT1 = 0.f;
	g_fDNFreezeT0 = g_fDNFreezeT1 = 0.f;
	g_fDNWrapT = 1.f;
	g_fDNSunIntensity = -1.f;
	g_xDNSunDir = Zenith_Maths::Vector3(0.f);
	g_xDNFogColor = Zenith_Maths::Vector3(0.f);
	g_bDNDone = false;
}

static bool Step_DayNightAndSun(int iFrame)
{
	switch (g_eDN)
	{
	case DNPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eDN = DNPhase::WaitPlaying;
		return true;

	case DNPhase::WaitPlaying:
		if (IsPlaying())
		{
			// Phase 1: cycle ON, known start.
			EG::Test_SetDayCycleEnabled(true);
			EG::Test_SetTimeOfDay(0.30f);
			g_fDNAdvT0 = EG::Test_GetTimeOfDay();
			g_iDNFrame = 0;
			g_eDN = DNPhase::AdvA;
			return true;
		}
		return iFrame < 900;

	case DNPhase::AdvA:
		// let a couple frames pass so the set value is the observed baseline
		if (++g_iDNFrame < 2) return true;
		g_fDNAdvT0 = EG::Test_GetTimeOfDay();
		g_iDNFrame = 0;
		g_eDN = DNPhase::AdvB;
		return true;

	case DNPhase::AdvB:
		if (++g_iDNFrame < 120) return true;
		g_fDNAdvT1 = EG::Test_GetTimeOfDay();
		// Phase 2: freeze.
		EG::Test_SetDayCycleEnabled(false);
		g_iDNFrame = 0;
		g_eDN = DNPhase::FreezeA;
		return true;

	case DNPhase::FreezeA:
		if (++g_iDNFrame < 2) return true;
		g_fDNFreezeT0 = EG::Test_GetTimeOfDay();
		g_iDNFrame = 0;
		g_eDN = DNPhase::FreezeB;
		return true;

	case DNPhase::FreezeB:
		if (++g_iDNFrame < 120) return true;
		g_fDNFreezeT1 = EG::Test_GetTimeOfDay();
		// Phase 3: wrap (cycle on, set just below 1.0).
		EG::Test_SetDayCycleEnabled(true);
		EG::Test_SetTimeOfDay(0.9999f);
		g_iDNFrame = 0;
		g_eDN = DNPhase::WrapSet;
		return true;

	case DNPhase::WrapSet:
		g_iDNFrame = 0;
		g_eDN = DNPhase::WrapWait;
		return true;

	case DNPhase::WrapWait:
		if (++g_iDNFrame < 60) return true;
		g_fDNWrapT = EG::Test_GetTimeOfDay();
		// Phase 4: sun/fog math from a frozen known time-of-day.
		EG::Test_SetDayCycleEnabled(false);
		EG::Test_SetTimeOfDay(kSunTestTime);
		g_iDNFrame = 0;
		g_eDN = DNPhase::SunSet;
		return true;

	case DNPhase::SunSet:
		g_iDNFrame = 0;
		g_eDN = DNPhase::SunWait;
		return true;

	case DNPhase::SunWait:
		// Give ApplyAtmosphere a few frames to recompute sun/fog from the set time.
		if (++g_iDNFrame < 4) return true;
		g_fDNSunIntensity = EG::Test_GetSunIntensity();
		EG::Test_GetSunDirection(g_xDNSunDir);
		EG::Test_GetFogColor(g_xDNFogColor);
		g_bDNDone = true;
		g_eDN = DNPhase::Done;
		return false;

	case DNPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_DayNightAndSun()
{
	LeaveReducedWorld();
	if (!g_bDNDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DayNightAndSun] never completed (phase %d)", static_cast<int>(g_eDN));
		return false;
	}
	// (1) advance: time-of-day increased while the cycle was enabled.
	if (!(g_fDNAdvT1 > g_fDNAdvT0 + 1e-5f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DayNightAndSun] time did not advance (t0=%.6f t1=%.6f)", g_fDNAdvT0, g_fDNAdvT1);
		return false;
	}
	// (2) freeze: time-of-day held steady while the cycle was disabled.
	if (std::fabs(g_fDNFreezeT1 - g_fDNFreezeT0) > 1e-5f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DayNightAndSun] time advanced while frozen (t0=%.6f t1=%.6f)", g_fDNFreezeT0, g_fDNFreezeT1);
		return false;
	}
	// (3) wrap: 0.9999 + advance wrapped back near 0, never exceeding 1.0.
	if (!(g_fDNWrapT >= 0.0f && g_fDNWrapT < 0.5f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DayNightAndSun] time did not wrap (t=%.6f)", g_fDNWrapT);
		return false;
	}
	// (4) sun/fog math read-after-write: the state must equal the pure Calculate* of
	// the SET time-of-day (proves ApplyAtmosphere read the just-advanced time, not a
	// stale value). This is the assertion that locks the A->B->ApplyAtmosphere split.
	const Zenith_Maths::Vector3 xExpectedDir = Exploration_AtmosphereController::CalculateSunDirection(kSunTestTime);
	const float fExpectedIntensity = Exploration_AtmosphereController::CalculateSunIntensity(xExpectedDir);
	const Zenith_Maths::Vector3 xExpectedFog = Exploration_AtmosphereController::CalculateFogColor(kSunTestTime);

	if (std::fabs(g_fDNSunIntensity - fExpectedIntensity) > 1e-3f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DayNightAndSun] sun intensity mismatch (got=%.5f expected=%.5f)", g_fDNSunIntensity, fExpectedIntensity);
		return false;
	}
	if (glm::length(g_xDNSunDir - xExpectedDir) > 1e-3f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DayNightAndSun] sun direction mismatch (got %.4f,%.4f,%.4f expected %.4f,%.4f,%.4f)",
			g_xDNSunDir.x, g_xDNSunDir.y, g_xDNSunDir.z, xExpectedDir.x, xExpectedDir.y, xExpectedDir.z);
		return false;
	}
	if (glm::length(g_xDNFogColor - xExpectedFog) > 1e-3f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DayNightAndSun] fog color mismatch (got %.4f,%.4f,%.4f expected %.4f,%.4f,%.4f)",
			g_xDNFogColor.x, g_xDNFogColor.y, g_xDNFogColor.z, xExpectedFog.x, xExpectedFog.y, xExpectedFog.z);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xDayNightAndSunTest = {
	"Exploration_DayNightAndSun_Test",
	&Setup_DayNightAndSun,
	&Step_DayNightAndSun,
	&Verify_DayNightAndSun,
	/*maxFrames*/ 1800,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDayNightAndSunTest);

// ============================================================================
// Exploration_Weather_Test
//   A forced weather transition advances the transition fraction (step B) and the
//   fog density interpolates toward the new weather density (step B writes the
//   target, step H mixes toward it).
// ============================================================================

namespace
{
	enum class WxPhase { Boot, WaitPlaying, Arm, Record, Advance, Measure, Done };

	WxPhase g_eWx = WxPhase::Boot;
	int     g_iWxFrame = 0;
	float   g_fWxTrans0 = 0.f;
	float   g_fWxTrans1 = 0.f;
	float   g_fWxFog0 = 0.f;
	float   g_fWxFog1 = 0.f;
	bool    g_bWxDone = false;
}

static void Setup_Weather()
{
	EnterReducedWorld();
	g_eWx = WxPhase::Boot;
	g_iWxFrame = 0;
	g_fWxTrans0 = g_fWxTrans1 = 0.f;
	g_fWxFog0 = g_fWxFog1 = 0.f;
	g_bWxDone = false;
}

static bool Step_Weather(int iFrame)
{
	switch (g_eWx)
	{
	case WxPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eWx = WxPhase::WaitPlaying;
		return true;

	case WxPhase::WaitPlaying:
		if (IsPlaying())
		{
			// Known start, then force a CLEAR->FOGGY transition (transition = 0).
			EG::Test_ResetAtmosphere();
			EG::Test_SetWeather(static_cast<int32_t>(Exploration_AtmosphereController::WEATHER_FOGGY));
			g_iWxFrame = 0;
			g_eWx = WxPhase::Arm;
			return true;
		}
		return iFrame < 900;

	case WxPhase::Arm:
		if (++g_iWxFrame < 2) return true;
		g_eWx = WxPhase::Record;
		return true;

	case WxPhase::Record:
		g_fWxTrans0 = EG::Test_GetWeatherTransition();
		g_fWxFog0 = EG::Test_GetFogDensity();
		g_iWxFrame = 0;
		g_eWx = WxPhase::Advance;
		return true;

	case WxPhase::Advance:
		if (++g_iWxFrame < 120) return true;
		g_eWx = WxPhase::Measure;
		return true;

	case WxPhase::Measure:
		g_fWxTrans1 = EG::Test_GetWeatherTransition();
		g_fWxFog1 = EG::Test_GetFogDensity();
		g_bWxDone = true;
		g_eWx = WxPhase::Done;
		return false;

	case WxPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_Weather()
{
	LeaveReducedWorld();
	if (!g_bWxDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Weather] never completed (phase %d)", static_cast<int>(g_eWx));
		return false;
	}
	// Transition fraction advanced from ~0 toward 1 over the 120 frames.
	if (!(g_fWxTrans1 > g_fWxTrans0 + 1e-4f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Weather] transition did not advance (t0=%.5f t1=%.5f)", g_fWxTrans0, g_fWxTrans1);
		return false;
	}
	// Fog density rose toward the foggier target (FOGGY density > CLEAR density).
	if (!(g_fWxFog1 > g_fWxFog0))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[Weather] fog density did not rise toward foggy (f0=%.6f f1=%.6f)", g_fWxFog0, g_fWxFog1);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xWeatherTest = {
	"Exploration_Weather_Test",
	&Setup_Weather,
	&Step_Weather,
	&Verify_Weather,
	/*maxFrames*/ 1400,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xWeatherTest);

// ============================================================================
// Exploration_CrossSceneAtmos_Test
//   play -> Esc -> play: Configure (every OnAwake) resets time-of-day to the start
//   value, but the weather timer is a namespace static NOT reset by Configure, so it
//   PERSISTS across the scene swap. The ApplyAtmosphere split must preserve both.
// ============================================================================

namespace
{
	enum class XsPhase { Boot, WaitPlaying1, Dirty, Accum, RecordBefore, PressEsc, AwaitMenu,
	                     ClickPlay, WaitPlaying2, Settle2, Measure, Done };

	XsPhase g_eXs = XsPhase::Boot;
	int     g_iXsFrame = 0;
	float   g_fXsTimeBefore = 0.f;
	float   g_fXsTimerBefore = 0.f;
	float   g_fXsTimeAfter = 0.f;
	float   g_fXsTimerAfter = 0.f;
	bool    g_bXsDone = false;
}

static void Setup_CrossSceneAtmos()
{
	EnterReducedWorld();
	g_eXs = XsPhase::Boot;
	g_iXsFrame = 0;
	g_fXsTimeBefore = g_fXsTimerBefore = 0.f;
	g_fXsTimeAfter = g_fXsTimerAfter = 0.f;
	g_bXsDone = false;
}

static bool Step_CrossSceneAtmos(int iFrame)
{
	switch (g_eXs)
	{
	case XsPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eXs = XsPhase::WaitPlaying1;
		return true;

	case XsPhase::WaitPlaying1:
		if (IsPlaying())
		{
			g_eXs = XsPhase::Dirty;
			return true;
		}
		return iFrame < 900;

	case XsPhase::Dirty:
		// Force time-of-day far from the 0.25 start value; reset the weather timer to
		// a known 0 so its accumulation is observable.
		EG::Test_ResetAtmosphere();
		EG::Test_SetDayCycleEnabled(true);
		EG::Test_SetTimeOfDay(0.70f);
		g_iXsFrame = 0;
		g_eXs = XsPhase::Accum;
		return true;

	case XsPhase::Accum:
		// Let the weather timer accumulate for ~120 frames of PLAYING.
		if (++g_iXsFrame < 120) return true;
		g_eXs = XsPhase::RecordBefore;
		return true;

	case XsPhase::RecordBefore:
		g_fXsTimeBefore = EG::Test_GetTimeOfDay();
		g_fXsTimerBefore = EG::Test_GetWeatherTimer();
		g_eXs = XsPhase::PressEsc;
		return true;

	case XsPhase::PressEsc:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iXsFrame = 0;
		g_eXs = XsPhase::AwaitMenu;
		return true;

	case XsPhase::AwaitMenu:
		if (IsMenu() && FindMenuPlayButton() != nullptr)
		{
			g_eXs = XsPhase::ClickPlay;
			return true;
		}
		return ++g_iXsFrame < 600;

	case XsPhase::ClickPlay:
	{
		Zenith_UI::Zenith_UIButton* pxPlay = FindMenuPlayButton();
		if (pxPlay == nullptr) return false;
		pxPlay->Activate();
		g_iXsFrame = 0;
		g_eXs = XsPhase::WaitPlaying2;
		return true;
	}

	case XsPhase::WaitPlaying2:
		if (IsPlaying())
		{
			g_iXsFrame = 0;
			g_eXs = XsPhase::Settle2;
			return true;
		}
		return ++g_iXsFrame < 900;

	case XsPhase::Settle2:
		if (++g_iXsFrame < 4) return true;
		g_eXs = XsPhase::Measure;
		return true;

	case XsPhase::Measure:
		g_fXsTimeAfter = EG::Test_GetTimeOfDay();
		g_fXsTimerAfter = EG::Test_GetWeatherTimer();
		g_bXsDone = true;
		g_eXs = XsPhase::Done;
		return false;

	case XsPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_CrossSceneAtmos()
{
	LeaveReducedWorld();
	if (!g_bXsDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[CrossSceneAtmos] never completed (phase %d)", static_cast<int>(g_eXs));
		return false;
	}
	// Time-of-day was forced to 0.70 before the swap; Configure on the new GameManager
	// resets it back near the 0.25 start value.
	if (!(g_fXsTimeBefore > 0.6f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[CrossSceneAtmos] pre-swap time-of-day not dirtied (%.4f)", g_fXsTimeBefore);
		return false;
	}
	if (!(g_fXsTimeAfter < 0.35f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[CrossSceneAtmos] time-of-day did NOT reset on re-entry (before=%.4f after=%.4f)",
			g_fXsTimeBefore, g_fXsTimeAfter);
		return false;
	}
	// The weather timer is NOT reset by Configure: it persisted (and kept counting) across the swap.
	if (!(g_fXsTimerBefore > 0.5f))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[CrossSceneAtmos] weather timer never accumulated before swap (%.4f)", g_fXsTimerBefore);
		return false;
	}
	if (g_fXsTimerAfter < g_fXsTimerBefore - 0.05f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[CrossSceneAtmos] weather timer was reset on re-entry (before=%.4f after=%.4f)",
			g_fXsTimerBefore, g_fXsTimerAfter);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xCrossSceneAtmosTest = {
	"Exploration_CrossSceneAtmos_Test",
	&Setup_CrossSceneAtmos,
	&Step_CrossSceneAtmos,
	&Verify_CrossSceneAtmos,
	/*maxFrames*/ 2400,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xCrossSceneAtmosTest);

// ============================================================================
// Exploration_MouseCaptureAcrossEsc_Test
//   Locks the Escape-return-frame skip (the fix for the review's cross-scene
//   divergence). Original C++: the Esc check does `ReturnToMenu(); return;`, which
//   SKIPS PlayerController::Update on the Esc frame - so its `if (captured &&
//   WasEscapePressed) SetMouseCapture(false)` branch is DEAD (mouse stays captured).
//   Post-conversion, ReturnToMenu (order 60) sets m_eGameState=MAIN_MENU so the @100
//   PLAYING block is skipped that frame too - keeping the Esc-release dead and
//   s_bMouseCaptured unchanged. Without the fix the @100 block would run
//   PlayerController::Update, fire the Esc-release, and flip the (persistent,
//   namespace-static) capture flag - an observable cross-scene divergence.
// ============================================================================

namespace
{
	enum class McPhase { Boot, WaitPlaying, Capture, Confirm, PressEsc, AwaitMenu, Done };

	McPhase g_eMc = McPhase::Boot;
	int     g_iMcFrame = 0;
	bool    g_bMcCapturedBefore = false;
	bool    g_bMcCapturedAfter = false;
	bool    g_bMcSawMenu = false;
	bool    g_bMcDone = false;
}

static void Setup_MouseCapture()
{
	EnterReducedWorld();
	g_eMc = McPhase::Boot;
	g_iMcFrame = 0;
	g_bMcCapturedBefore = false;
	g_bMcCapturedAfter = false;
	g_bMcSawMenu = false;
	g_bMcDone = false;
}

static bool Step_MouseCapture(int iFrame)
{
	switch (g_eMc)
	{
	case McPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eMc = McPhase::WaitPlaying;
		return true;

	case McPhase::WaitPlaying:
		if (IsPlaying())
		{
			g_iMcFrame = 0;
			g_eMc = McPhase::Capture;
			return true;
		}
		return iFrame < 900;

	case McPhase::Capture:
		// Click to capture the mouse (PlayerController::Update processes the click on
		// a normal PLAYING frame). Poll until captured (the flag is a persistent
		// namespace static, so a prior test may have left it either way).
		if (!EG::Test_IsMouseCaptured())
		{
			Zenith_InputSimulator::SimulateKeyPress(ZENITH_MOUSE_BUTTON_LEFT);
			return ++g_iMcFrame < 120;
		}
		g_eMc = McPhase::Confirm;
		return true;

	case McPhase::Confirm:
		g_bMcCapturedBefore = EG::Test_IsMouseCaptured();
		g_eMc = McPhase::PressEsc;
		return true;

	case McPhase::PressEsc:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_iMcFrame = 0;
		g_eMc = McPhase::AwaitMenu;
		return true;

	case McPhase::AwaitMenu:
		if (IsMenu() && FindMenuPlayButton() != nullptr)
		{
			g_bMcSawMenu = true;
			// The Esc-release must NOT have fired: the @100 PLAYING block was skipped
			// on the Esc frame (state left PLAYING would have run it), so the capture
			// flag is unchanged.
			g_bMcCapturedAfter = EG::Test_IsMouseCaptured();
			g_bMcDone = true;
			g_eMc = McPhase::Done;
			return false;
		}
		return ++g_iMcFrame < 600;

	case McPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_MouseCapture()
{
	LeaveReducedWorld();
	if (!g_bMcDone || !g_bMcSawMenu)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MouseCapture] never completed / never reached menu (phase %d)", static_cast<int>(g_eMc));
		return false;
	}
	if (!g_bMcCapturedBefore)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MouseCapture] mouse never captured before Escape");
		return false;
	}
	if (!g_bMcCapturedAfter)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[MouseCapture] Escape flipped the (persistent) capture flag - the @100 PLAYING block ran on the Esc frame (the divergence)");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xMouseCaptureTest = {
	"Exploration_MouseCaptureAcrossEsc_Test",
	&Setup_MouseCapture,
	&Step_MouseCapture,
	&Verify_MouseCapture,
	/*maxFrames*/ 1800,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMouseCaptureTest);

#endif // ZENITH_INPUT_SIMULATOR
