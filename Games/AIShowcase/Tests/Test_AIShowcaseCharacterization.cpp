#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_AIShowcaseCharacterization - characterization tests for the W3 graph
 * conversion of AIShowcase (menu/state/player-verb flow + the per-enemy brain).
 *
 * Written against the C++ version FIRST (menu->play, P pause/resume, Esc->menu,
 * WASD teleport-clamp movement, formation switch, and the enemy chase/patrol
 * decision); the graph version must keep these green unchanged. Probes go
 * through read-only accessors + a scenario-setup teleport that survive the
 * conversion.
 *
 *   AIShowcase_MenuPlay_Test    - Play holds focus; activating it starts the arena.
 *   AIShowcase_PauseResume_Test - P pauses (arena frozen), P resumes.
 *   AIShowcase_EscapeMenu_Test  - Esc during play returns to the main menu.
 *   AIShowcase_PlayerMove_Test  - holding W drives the player +Z, clamped to arena.
 *   AIShowcase_Formation_Test   - pressing 3 selects formation index 2.
 *   AIShowcase_EnemyChase_Test  - an enemy that perceives the player targets it.
 *   AIShowcase_EnemyPatrol_Test - an enemy with no target patrols to a waypoint.
 *
 * requiresGraphics = true: the arena builds Flux-backed model/material entities,
 * so these run WINDOWED (headless skips them), like RenderTest/Combat.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "UI/Zenith_UIButton.h"
#include "Maths/Zenith_Maths.h"
#include "Components/AIShowcase_GameComponent.h"

namespace
{
	AIShowcase_GameComponent* FindAIShowcaseGame()
	{
		AIShowcase_GameComponent* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<AIShowcase_GameComponent>().ForEach(
			[&pxFound](Zenith_EntityID, AIShowcase_GameComponent& xGame)
			{
				if (pxFound == nullptr) pxFound = &xGame;
			});
		return pxFound;
	}

	Zenith_UI::Zenith_UIButton* FindAIShowcasePlayButton()
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

	// The four hard-coded patrol waypoints an idle enemy cycles through
	// (AIShowcase_GameComponent::UpdateEnemyAI s_axPatrolPoints).
	const Zenith_Maths::Vector3 s_axPatrolWaypoints[4] = {
		Zenith_Maths::Vector3(-15.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(15.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, -12.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f),
	};

	float NearestWaypointXZDist(const Zenith_Maths::Vector3& xPos)
	{
		float fBest = 1.0e9f;
		for (uint32_t u = 0; u < 4; ++u)
		{
			const float fDx = xPos.x - s_axPatrolWaypoints[u].x;
			const float fDz = xPos.z - s_axPatrolWaypoints[u].z;
			const float fD = sqrtf(fDx * fDx + fDz * fDz);
			if (fD < fBest) fBest = fD;
		}
		return fBest;
	}

	float XZDist(const Zenith_Maths::Vector3& a, const Zenith_Maths::Vector3& b)
	{
		const float fDx = a.x - b.x;
		const float fDz = a.z - b.z;
		return sqrtf(fDx * fDx + fDz * fDz);
	}
}

// ============================================================================
// AIShowcase_MenuPlay_Test
// ============================================================================

namespace
{
	enum class AMenuPhase { Boot, WaitMenu, CheckFocus, ActivatePlay, AwaitPlaying, Done };
	AMenuPhase g_eAMenuPhase = AMenuPhase::Boot;
	int        g_iAMenuFrame = 0;
	bool       g_bAPlayFocused = false;
	bool       g_bAPlayStarted = false;
}

static void Setup_AIShowcaseMenuPlay()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eAMenuPhase = AMenuPhase::Boot;
	g_iAMenuFrame = 0;
	g_bAPlayFocused = false;
	g_bAPlayStarted = false;
}

static bool Step_AIShowcaseMenuPlay(int iFrame)
{
	AIShowcase_GameComponent* pxGame = FindAIShowcaseGame();
	Zenith_UI::Zenith_UIButton* pxPlay = FindAIShowcasePlayButton();

	switch (g_eAMenuPhase)
	{
	case AMenuPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eAMenuPhase = AMenuPhase::WaitMenu;
		return true;

	case AMenuPhase::WaitMenu:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::MAIN_MENU && pxPlay != nullptr)
		{
			g_eAMenuPhase = AMenuPhase::CheckFocus;
			g_iAMenuFrame = 0;
		}
		return iFrame < 600;

	case AMenuPhase::CheckFocus:
		if (++g_iAMenuFrame < 10) return true;
		if (pxPlay == nullptr) return false;
		g_bAPlayFocused = pxPlay->IsFocused();
		if (!g_bAPlayFocused)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISMenu] Play button not focused");
			return false;
		}
		g_eAMenuPhase = AMenuPhase::ActivatePlay;
		return true;

	case AMenuPhase::ActivatePlay:
		if (pxPlay == nullptr) return false;
		pxPlay->Activate();
		g_eAMenuPhase = AMenuPhase::AwaitPlaying;
		g_iAMenuFrame = 0;
		return true;

	case AMenuPhase::AwaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::PLAYING
			&& pxGame->GetEnemyCount() > 0)
		{
			g_bAPlayStarted = true;
			g_eAMenuPhase = AMenuPhase::Done;
			return false;
		}
		return ++g_iAMenuFrame < 600;

	case AMenuPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_AIShowcaseMenuPlay()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bAPlayFocused) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISMenu] focus wrong"); }
	if (!g_bAPlayStarted) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISMenu] Play never reached PLAYING with enemies"); }
	return g_bAPlayFocused && g_bAPlayStarted;
}

static const Zenith_AutomatedTest g_xAIShowcaseMenuPlayTest = {
	"AIShowcase_MenuPlay_Test",
	&Setup_AIShowcaseMenuPlay,
	&Step_AIShowcaseMenuPlay,
	&Verify_AIShowcaseMenuPlay,
	/*maxFrames*/ 2000,
	/*bRequiresGraphics*/ true,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAIShowcaseMenuPlayTest);

// ============================================================================
// AIShowcase_PauseResume_Test
// ============================================================================

namespace
{
	enum class APausePhase { Boot, WaitPlaying, PressPause, AwaitPaused, PressResume, AwaitResumed, Done };
	APausePhase g_eAPausePhase = APausePhase::Boot;
	int         g_iAPauseFrame = 0;
	bool        g_bAPausedOk = false;
	bool        g_bAResumedOk = false;
}

static void Setup_AIShowcasePauseResume()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eAPausePhase = APausePhase::Boot;
	g_iAPauseFrame = 0;
	g_bAPausedOk = false;
	g_bAResumedOk = false;
}

static bool Step_AIShowcasePauseResume(int iFrame)
{
	AIShowcase_GameComponent* pxGame = FindAIShowcaseGame();
	switch (g_eAPausePhase)
	{
	case APausePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eAPausePhase = APausePhase::WaitPlaying;
		return true;

	case APausePhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::PLAYING)
		{
			g_eAPausePhase = APausePhase::PressPause;
			g_iAPauseFrame = 0;
		}
		return iFrame < 600;

	case APausePhase::PressPause:
		if (++g_iAPauseFrame < 10) return true;
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P);
		g_eAPausePhase = APausePhase::AwaitPaused;
		g_iAPauseFrame = 0;
		return true;

	case APausePhase::AwaitPaused:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::PAUSED)
		{
			const Zenith_Scene xArena = pxGame->GetArenaScene();
			const bool bArenaPaused = xArena.IsValid() && g_xEngine.Scenes().IsScenePaused(xArena);
			g_bAPausedOk = bArenaPaused;
			if (!g_bAPausedOk)
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISPause] PAUSED but arena not paused");
			g_eAPausePhase = APausePhase::PressResume;
			g_iAPauseFrame = 0;
			return true;
		}
		return ++g_iAPauseFrame < 120;

	case APausePhase::PressResume:
		if (++g_iAPauseFrame < 10) return true;
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_P);
		g_eAPausePhase = APausePhase::AwaitResumed;
		g_iAPauseFrame = 0;
		return true;

	case APausePhase::AwaitResumed:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::PLAYING)
		{
			const Zenith_Scene xArena = pxGame->GetArenaScene();
			g_bAResumedOk = xArena.IsValid() && !g_xEngine.Scenes().IsScenePaused(xArena);
			if (!g_bAResumedOk)
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISPause] resumed but arena still paused");
			g_eAPausePhase = APausePhase::Done;
			return false;
		}
		return ++g_iAPauseFrame < 120;

	case APausePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_AIShowcasePauseResume()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bAPausedOk)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISPause] P did not pause the arena"); }
	if (!g_bAResumedOk) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISPause] P did not resume"); }
	return g_bAPausedOk && g_bAResumedOk;
}

static const Zenith_AutomatedTest g_xAIShowcasePauseResumeTest = {
	"AIShowcase_PauseResume_Test",
	&Setup_AIShowcasePauseResume,
	&Step_AIShowcasePauseResume,
	&Verify_AIShowcasePauseResume,
	/*maxFrames*/ 2000,
	/*bRequiresGraphics*/ true,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAIShowcasePauseResumeTest);

// ============================================================================
// AIShowcase_EscapeMenu_Test
// ============================================================================

namespace
{
	enum class AEscPhase { Boot, WaitPlaying, PressEsc, AwaitMenu, Done };
	AEscPhase g_eAEscPhase = AEscPhase::Boot;
	int       g_iAEscFrame = 0;
	bool      g_bAEscReachedMenu = false;
}

static void Setup_AIShowcaseEscapeMenu()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eAEscPhase = AEscPhase::Boot;
	g_iAEscFrame = 0;
	g_bAEscReachedMenu = false;
}

static bool Step_AIShowcaseEscapeMenu(int iFrame)
{
	AIShowcase_GameComponent* pxGame = FindAIShowcaseGame();
	switch (g_eAEscPhase)
	{
	case AEscPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eAEscPhase = AEscPhase::WaitPlaying;
		return true;

	case AEscPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::PLAYING)
		{
			g_eAEscPhase = AEscPhase::PressEsc;
			g_iAEscFrame = 0;
		}
		return iFrame < 600;

	case AEscPhase::PressEsc:
		if (++g_iAEscFrame < 30) return true;
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_eAEscPhase = AEscPhase::AwaitMenu;
		g_iAEscFrame = 0;
		return true;

	case AEscPhase::AwaitMenu:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::MAIN_MENU)
		{
			g_bAEscReachedMenu = true;
			g_eAEscPhase = AEscPhase::Done;
			return false;
		}
		return ++g_iAEscFrame < 600;

	case AEscPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_AIShowcaseEscapeMenu()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bAEscReachedMenu) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISEsc] Esc did not return to menu"); }
	return g_bAEscReachedMenu;
}

static const Zenith_AutomatedTest g_xAIShowcaseEscapeMenuTest = {
	"AIShowcase_EscapeMenu_Test",
	&Setup_AIShowcaseEscapeMenu,
	&Step_AIShowcaseEscapeMenu,
	&Verify_AIShowcaseEscapeMenu,
	/*maxFrames*/ 1500,
	/*bRequiresGraphics*/ true,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAIShowcaseEscapeMenuTest);

// ============================================================================
// AIShowcase_PlayerMove_Test
// ============================================================================
// W drives the player +Z (forward, away from camera), clamped to arena bound
// fMaxZ = s_fArenaHeight*0.5 - 1 = 14.

namespace
{
	enum class AMovePhase { Boot, WaitPlaying, Record, HoldForward, Check, Done };
	AMovePhase g_eAMovePhase = AMovePhase::Boot;
	int        g_iAMoveFrame = 0;
	float      g_fAStartZ = 0.0f;
	bool       g_bAMovedForward = false;
	bool       g_bAClampedOk = false;
}

static void Setup_AIShowcasePlayerMove()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eAMovePhase = AMovePhase::Boot;
	g_iAMoveFrame = 0;
	g_fAStartZ = 0.0f;
	g_bAMovedForward = false;
	g_bAClampedOk = false;
}

static bool Step_AIShowcasePlayerMove(int iFrame)
{
	AIShowcase_GameComponent* pxGame = FindAIShowcaseGame();
	switch (g_eAMovePhase)
	{
	case AMovePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eAMovePhase = AMovePhase::WaitPlaying;
		return true;

	case AMovePhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::PLAYING)
		{
			g_eAMovePhase = AMovePhase::Record;
			g_iAMoveFrame = 0;
		}
		return iFrame < 600;

	case AMovePhase::Record:
	{
		if (++g_iAMoveFrame < 5) return true;
		if (pxGame == nullptr) return false;
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(pxGame->GetPlayerEntityID());
		if (!xPlayer.IsValid()) return false;
		Zenith_Maths::Vector3 xPos;
		xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		g_fAStartZ = xPos.z;
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
		g_eAMovePhase = AMovePhase::HoldForward;
		g_iAMoveFrame = 0;
		return true;
	}

	case AMovePhase::HoldForward:
		// Hold long enough to move measurably and reach the clamp.
		if (++g_iAMoveFrame < 240) return true;
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		g_eAMovePhase = AMovePhase::Check;
		g_iAMoveFrame = 0;
		return true;

	case AMovePhase::Check:
	{
		if (++g_iAMoveFrame < 5) return true;
		if (pxGame == nullptr) return false;
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(pxGame->GetPlayerEntityID());
		if (!xPlayer.IsValid()) return false;
		Zenith_Maths::Vector3 xPos;
		xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		g_bAMovedForward = xPos.z > g_fAStartZ + 1.0f;
		g_bAClampedOk = xPos.z <= 14.05f;	// s_fArenaHeight*0.5 - 1
		if (!g_bAMovedForward || !g_bAClampedOk)
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISMove] startZ %.2f endZ %.2f", g_fAStartZ, xPos.z);
		g_eAMovePhase = AMovePhase::Done;
		return false;
	}

	case AMovePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_AIShowcasePlayerMove()
{
	Zenith_InputSimulator::ClearFixedDt();
	Zenith_InputSimulator::ClearHeldKeys();
	if (!g_bAMovedForward) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISMove] W did not move player forward"); }
	if (!g_bAClampedOk)    { Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISMove] player exceeded arena clamp"); }
	return g_bAMovedForward && g_bAClampedOk;
}

static const Zenith_AutomatedTest g_xAIShowcasePlayerMoveTest = {
	"AIShowcase_PlayerMove_Test",
	&Setup_AIShowcasePlayerMove,
	&Step_AIShowcasePlayerMove,
	&Verify_AIShowcasePlayerMove,
	/*maxFrames*/ 1500,
	/*bRequiresGraphics*/ true,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAIShowcasePlayerMoveTest);

// ============================================================================
// AIShowcase_Formation_Test
// ============================================================================

namespace
{
	enum class AFormPhase { Boot, WaitPlaying, PressThree, AwaitFormation, Done };
	AFormPhase g_eAFormPhase = AFormPhase::Boot;
	int        g_iAFormFrame = 0;
	bool       g_bAFormationOk = false;
}

static void Setup_AIShowcaseFormation()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eAFormPhase = AFormPhase::Boot;
	g_iAFormFrame = 0;
	g_bAFormationOk = false;
}

static bool Step_AIShowcaseFormation(int iFrame)
{
	AIShowcase_GameComponent* pxGame = FindAIShowcaseGame();
	switch (g_eAFormPhase)
	{
	case AFormPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eAFormPhase = AFormPhase::WaitPlaying;
		return true;

	case AFormPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::PLAYING)
		{
			g_eAFormPhase = AFormPhase::PressThree;
			g_iAFormFrame = 0;
		}
		return iFrame < 600;

	case AFormPhase::PressThree:
		if (++g_iAFormFrame < 10) return true;
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_3);
		g_eAFormPhase = AFormPhase::AwaitFormation;
		g_iAFormFrame = 0;
		return true;

	case AFormPhase::AwaitFormation:
		if (pxGame != nullptr && pxGame->GetCurrentFormation() == 2)
		{
			g_bAFormationOk = true;
			g_eAFormPhase = AFormPhase::Done;
			return false;
		}
		return ++g_iAFormFrame < 60;

	case AFormPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_AIShowcaseFormation()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bAFormationOk) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISForm] key 3 did not select formation 2"); }
	return g_bAFormationOk;
}

static const Zenith_AutomatedTest g_xAIShowcaseFormationTest = {
	"AIShowcase_Formation_Test",
	&Setup_AIShowcaseFormation,
	&Step_AIShowcaseFormation,
	&Verify_AIShowcaseFormation,
	/*maxFrames*/ 1500,
	/*bRequiresGraphics*/ true,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAIShowcaseFormationTest);

// ============================================================================
// AIShowcase_EnemyChase_Test (STAR)
// ============================================================================
// Re-place the player each frame in enemy 0's live forward (FOV centre, LOS
// clear at short range) until enemy 0 perceives it (the RenderTest apprehend
// lesson), then assert enemy 0's nav destination tracks the player (chase),
// not a patrol waypoint.

namespace
{
	enum class AChasePhase { Boot, WaitPlaying, DriveIntoView, Verify, Done };
	AChasePhase g_eAChasePhase = AChasePhase::Boot;
	int         g_iAChaseFrame = 0;
	bool        g_bAChaseOk = false;
	Zenith_Maths::Vector3 g_xAChasePlayerPos = Zenith_Maths::Vector3(0.0f);
}

static void Setup_AIShowcaseEnemyChase()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eAChasePhase = AChasePhase::Boot;
	g_iAChaseFrame = 0;
	g_bAChaseOk = false;
	g_xAChasePlayerPos = Zenith_Maths::Vector3(0.0f);
}

static bool Step_AIShowcaseEnemyChase(int iFrame)
{
	AIShowcase_GameComponent* pxGame = FindAIShowcaseGame();
	switch (g_eAChasePhase)
	{
	case AChasePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eAChasePhase = AChasePhase::WaitPlaying;
		return true;

	case AChasePhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::PLAYING
			&& pxGame->GetEnemyCount() > 0)
		{
			g_eAChasePhase = AChasePhase::DriveIntoView;
			g_iAChaseFrame = 0;
		}
		return iFrame < 600;

	case AChasePhase::DriveIntoView:
	{
		if (pxGame == nullptr) return false;
		// Leave the player where it is (it is a dynamic body - teleporting the
		// transform would not move the physics-backed position perception reads)
		// and bring the enemy to it instead: pin enemy 0 three metres in front
		// of the player, on -Z facing +Z toward it, in the open player-start
		// area. Re-applied each frame so the patrol wander can't drag it off.
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(pxGame->GetPlayerEntityID());
		if (!xPlayer.IsValid()) return false;
		xPlayer.GetComponent<Zenith_TransformComponent>().GetPosition(g_xAChasePlayerPos);
		const Zenith_Maths::Vector3 xEnemyPos(g_xAChasePlayerPos.x, g_xAChasePlayerPos.y, g_xAChasePlayerPos.z - 3.0f);
		pxGame->Test_SetEnemyPose(0, xEnemyPos);

		if (pxGame->Test_EnemyPerceivesPlayer(0))
		{
			// Stop pinning; let the decision consume the fresh perception.
			g_eAChasePhase = AChasePhase::Verify;
			g_iAChaseFrame = 0;
			return true;
		}
		return ++g_iAChaseFrame < 180;
	}

	case AChasePhase::Verify:
	{
		if (++g_iAChaseFrame < 4) return true;
		if (pxGame == nullptr) return false;
		Zenith_Maths::Vector3 xDest;
		if (!pxGame->Test_GetEnemyDestination(0, xDest)) return false;
		// Chase: destination tracks the player, and is clearly away from any
		// patrol waypoint.
		const float fToPlayer = XZDist(xDest, g_xAChasePlayerPos);
		const float fToWaypoint = NearestWaypointXZDist(xDest);
		g_bAChaseOk = pxGame->Test_EnemyPerceivesPlayer(0) && fToPlayer < 2.5f;
		if (!g_bAChaseOk)
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISChase] dest->player %.2f dest->wp %.2f", fToPlayer, fToWaypoint);
		g_eAChasePhase = AChasePhase::Done;
		return false;
	}

	case AChasePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_AIShowcaseEnemyChase()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bAChaseOk) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISChase] perceiving enemy did not chase the player"); }
	return g_bAChaseOk;
}

static const Zenith_AutomatedTest g_xAIShowcaseEnemyChaseTest = {
	"AIShowcase_EnemyChase_Test",
	&Setup_AIShowcaseEnemyChase,
	&Step_AIShowcaseEnemyChase,
	&Verify_AIShowcaseEnemyChase,
	/*maxFrames*/ 2000,
	/*bRequiresGraphics*/ true,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAIShowcaseEnemyChaseTest);

// ============================================================================
// AIShowcase_EnemyPatrol_Test (STAR)
// ============================================================================
// An enemy that perceives nothing patrols to one of the four waypoints.

namespace
{
	enum class APatrolPhase { Boot, WaitPlaying, Settle, Verify, Done };
	APatrolPhase g_eAPatrolPhase = APatrolPhase::Boot;
	int          g_iAPatrolFrame = 0;
	bool         g_bAPatrolOk = false;
}

static void Setup_AIShowcaseEnemyPatrol()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eAPatrolPhase = APatrolPhase::Boot;
	g_iAPatrolFrame = 0;
	g_bAPatrolOk = false;
}

static bool Step_AIShowcaseEnemyPatrol(int iFrame)
{
	AIShowcase_GameComponent* pxGame = FindAIShowcaseGame();
	switch (g_eAPatrolPhase)
	{
	case APatrolPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eAPatrolPhase = APatrolPhase::WaitPlaying;
		return true;

	case APatrolPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == AIShowcaseGameState::PLAYING
			&& pxGame->GetEnemyCount() > 0)
		{
			g_eAPatrolPhase = APatrolPhase::Settle;
			g_iAPatrolFrame = 0;
		}
		return iFrame < 600;

	case APatrolPhase::Settle:
		// Let the enemy driver run so idle agents pick a patrol waypoint.
		if (++g_iAPatrolFrame < 20) return true;
		g_eAPatrolPhase = APatrolPhase::Verify;
		return true;

	case APatrolPhase::Verify:
	{
		if (pxGame == nullptr) return false;
		// Find an enemy that does not perceive the player (all should, initially).
		for (uint32_t u = 0; u < pxGame->GetEnemyCount(); ++u)
		{
			if (pxGame->Test_EnemyPerceivesPlayer(u)) continue;
			Zenith_Maths::Vector3 xDest;
			if (!pxGame->Test_GetEnemyDestination(u, xDest)) continue;
			if (NearestWaypointXZDist(xDest) < 1.0f)
			{
				g_bAPatrolOk = true;
				break;
			}
		}
		if (!g_bAPatrolOk)
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISPatrol] no idle enemy heading to a patrol waypoint");
		g_eAPatrolPhase = APatrolPhase::Done;
		return false;
	}

	case APatrolPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_AIShowcaseEnemyPatrol()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bAPatrolOk) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[AISPatrol] idle enemy did not patrol to a waypoint"); }
	return g_bAPatrolOk;
}

static const Zenith_AutomatedTest g_xAIShowcaseEnemyPatrolTest = {
	"AIShowcase_EnemyPatrol_Test",
	&Setup_AIShowcaseEnemyPatrol,
	&Step_AIShowcaseEnemyPatrol,
	&Verify_AIShowcaseEnemyPatrol,
	/*maxFrames*/ 1500,
	/*bRequiresGraphics*/ true,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAIShowcaseEnemyPatrolTest);

#endif // ZENITH_INPUT_SIMULATOR
