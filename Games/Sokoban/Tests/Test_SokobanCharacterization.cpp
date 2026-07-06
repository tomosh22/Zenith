#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_SokobanCharacterization - characterization tests for the W2 graph
 * conversion of Sokoban's move / level / menu flow.
 *
 * Written against the C++ version FIRST (the key->move dispatch, blocked-move
 * refusal, move counting, push mechanics, win-on-step-complete decision, the
 * R/Esc level flow, and the exact HUD text formatting); the graph version must
 * keep these green unchanged. Probes go through read-only accessors (and the
 * fixture-level seam) that survive the conversion.
 *
 *   Sokoban_MenuPlay_Test   - Play holds focus; activating it starts a level.
 *   Sokoban_MoveFlow_Test   - on the fixture corridor: a blocked move does
 *                             nothing (and does not count), a neutral move
 *                             counts and animates, the push onto the target
 *                             wins on step completion, the HUD text matches
 *                             exactly, and input is dead after the win.
 *   Sokoban_ResetFlow_Test  - R regenerates a fresh solvable level (moves 0,
 *                             not won, solver-validated minMoves >= 5).
 *   Sokoban_EscapeMenu_Test - Esc during play returns to the main menu.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIText.h"
#include "Components/Sokoban_GameComponent.h"

namespace
{
	Sokoban_GameComponent* FindSokobanGame()
	{
		Sokoban_GameComponent* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Sokoban_GameComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Sokoban_GameComponent& xGame)
			{
				if (pxFound == nullptr) pxFound = &xGame;
			});
		return pxFound;
	}

	Zenith_UI::Zenith_UIButton* FindSokobanPlayButton()
	{
		Zenith_UI::Zenith_UIButton* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Zenith_UIComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Zenith_UIComponent& xUI)
			{
				if (pxFound == nullptr)
				{
					pxFound = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
				}
			});
		return pxFound;
	}

	// HUD text probe (elements are canvas-owned - re-found every call).
	std::string GetHUDText(const char* szElement)
	{
		std::string strText;
		g_xEngine.Scenes().QueryAllScenes<Zenith_UIComponent>().ForEach(
			[&strText, szElement](Zenith_EntityID, Zenith_UIComponent& xUI)
			{
				if (strText.empty())
				{
					if (Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(szElement))
					{
						strText = pxText->GetText();
					}
				}
			});
		return strText;
	}
}

// ============================================================================
// Sokoban_MenuPlay_Test
// ============================================================================

namespace
{
	enum class SMenuPhase { Boot, WaitMenu, CheckFocus, ActivatePlay, AwaitPlaying, Done };

	SMenuPhase g_eSMenuPhase = SMenuPhase::Boot;
	int        g_iSMenuFrame = 0;
	bool       g_bSPlayFocused = false;
	bool       g_bSPlayStarted = false;
}

static void Setup_SokobanMenuPlay()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eSMenuPhase = SMenuPhase::Boot;
	g_iSMenuFrame = 0;
	g_bSPlayFocused = false;
	g_bSPlayStarted = false;
}

static bool Step_SokobanMenuPlay(int iFrame)
{
	Sokoban_GameComponent* pxGame = FindSokobanGame();
	Zenith_UI::Zenith_UIButton* pxPlay = FindSokobanPlayButton();

	switch (g_eSMenuPhase)
	{
	case SMenuPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eSMenuPhase = SMenuPhase::WaitMenu;
		return true;

	case SMenuPhase::WaitMenu:
		if (pxGame != nullptr && pxGame->GetGameState() == SokobanGameState::MAIN_MENU && pxPlay != nullptr)
		{
			g_eSMenuPhase = SMenuPhase::CheckFocus;
			g_iSMenuFrame = 0;
		}
		return iFrame < 600;

	case SMenuPhase::CheckFocus:
		if (++g_iSMenuFrame < 10)
		{
			return true;
		}
		if (pxPlay == nullptr) return false;
		g_bSPlayFocused = pxPlay->IsFocused();
		if (!g_bSPlayFocused)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMenu] Play button not focused");
			return false;
		}
		g_eSMenuPhase = SMenuPhase::ActivatePlay;
		return true;

	case SMenuPhase::ActivatePlay:
		if (pxPlay == nullptr) return false;
		pxPlay->Activate();
		g_eSMenuPhase = SMenuPhase::AwaitPlaying;
		g_iSMenuFrame = 0;
		return true;

	case SMenuPhase::AwaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == SokobanGameState::PLAYING)
		{
			g_bSPlayStarted = true;
			g_eSMenuPhase = SMenuPhase::Done;
			return false;
		}
		return ++g_iSMenuFrame < 600;

	case SMenuPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_SokobanMenuPlay()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSPlayFocused) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMenu] focus wrong"); }
	if (!g_bSPlayStarted) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMenu] Play activation never reached PLAYING"); }
	return g_bSPlayFocused && g_bSPlayStarted;
}

static const Zenith_AutomatedTest g_xSokobanMenuPlayTest = {
	"Sokoban_MenuPlay_Test",
	&Setup_SokobanMenuPlay,
	&Step_SokobanMenuPlay,
	&Verify_SokobanMenuPlay,
	/*maxFrames*/ 2000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xSokobanMenuPlayTest);

// ============================================================================
// Sokoban_MoveFlow_Test
// ============================================================================
// Fixture corridor (6x3, walls all around):
//   [W][W][W][W][W][W]
//   [W][P][ ][B][T][W]
//   [W][W][W][W][W][W]
// A at (1,1) is blocked by the wall; D is a neutral move to (2,1); D again
// pushes the box (3,1)->(4,1) onto the target = win, player lands on (3,1).

namespace
{
	enum class SMovePhase
	{
		Boot, WaitPlaying, LoadFixture, CheckFixture,
		PressBlocked, HoldBlocked,
		PressNeutral, AwaitNeutral,
		PressPush, AwaitWin,
		PressAfterWin, HoldAfterWin,
		Done
	};

	SMovePhase g_eSMovePhase = SMovePhase::Boot;
	int        g_iSMoveFrame = 0;
	bool       g_bSBlockedOk = false;
	bool       g_bSNeutralOk = false;
	bool       g_bSWinOk = false;
	bool       g_bSHUDOk = false;
	bool       g_bSWonGateOk = false;
}

static void Setup_SokobanMoveFlow()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eSMovePhase = SMovePhase::Boot;
	g_iSMoveFrame = 0;
	g_bSBlockedOk = false;
	g_bSNeutralOk = false;
	g_bSWinOk = false;
	g_bSHUDOk = false;
	g_bSWonGateOk = false;
}

static bool Step_SokobanMoveFlow(int iFrame)
{
	Sokoban_GameComponent* pxGame = FindSokobanGame();
	switch (g_eSMovePhase)
	{
	case SMovePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eSMovePhase = SMovePhase::WaitPlaying;
		return true;

	case SMovePhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == SokobanGameState::PLAYING)
		{
			g_eSMovePhase = SMovePhase::LoadFixture;
			g_iSMoveFrame = 0;
		}
		return iFrame < 600;

	case SMovePhase::LoadFixture:
		if (++g_iSMoveFrame < 10)
		{
			return true;	// let the generated level settle first
		}
		if (pxGame == nullptr) return false;
		pxGame->Test_LoadFixtureLevel();
		g_eSMovePhase = SMovePhase::CheckFixture;
		g_iSMoveFrame = 0;
		return true;

	case SMovePhase::CheckFixture:
		if (++g_iSMoveFrame < 5)
		{
			return true;
		}
		if (pxGame == nullptr) return false;
		if (pxGame->GetPlayerX() != 1 || pxGame->GetPlayerY() != 1
			|| pxGame->GetMoveCount() != 0 || pxGame->IsWon() || pxGame->GetBoxesOnTargets() != 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMove] fixture state wrong: player (%u,%u) moves %u",
				pxGame->GetPlayerX(), pxGame->GetPlayerY(), pxGame->GetMoveCount());
			return false;
		}
		g_eSMovePhase = SMovePhase::PressBlocked;
		return true;

	case SMovePhase::PressBlocked:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_A);
		g_eSMovePhase = SMovePhase::HoldBlocked;
		g_iSMoveFrame = 0;
		return true;

	case SMovePhase::HoldBlocked:
		if (++g_iSMoveFrame < 20)
		{
			return true;
		}
		if (pxGame == nullptr) return false;
		g_bSBlockedOk = pxGame->GetPlayerX() == 1 && pxGame->GetPlayerY() == 1
			&& pxGame->GetMoveCount() == 0 && !pxGame->IsAnimating();
		if (!g_bSBlockedOk)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMove] blocked move leaked: player (%u,%u) moves %u",
				pxGame->GetPlayerX(), pxGame->GetPlayerY(), pxGame->GetMoveCount());
			return false;
		}
		g_eSMovePhase = SMovePhase::PressNeutral;
		return true;

	case SMovePhase::PressNeutral:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_D);
		g_eSMovePhase = SMovePhase::AwaitNeutral;
		g_iSMoveFrame = 0;
		return true;

	case SMovePhase::AwaitNeutral:
		if (pxGame == nullptr) return false;
		if (pxGame->GetMoveCount() == 1 && pxGame->GetPlayerX() == 2 && !pxGame->IsAnimating())
		{
			g_bSNeutralOk = !pxGame->IsWon();
			g_eSMovePhase = SMovePhase::PressPush;
			return true;
		}
		return ++g_iSMoveFrame < 30;

	case SMovePhase::PressPush:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_D);
		g_eSMovePhase = SMovePhase::AwaitWin;
		g_iSMoveFrame = 0;
		return true;

	case SMovePhase::AwaitWin:
		if (pxGame == nullptr) return false;
		if (pxGame->IsWon())
		{
			g_bSWinOk = pxGame->GetMoveCount() == 2
				&& pxGame->GetPlayerX() == 3
				&& pxGame->GetBoxesOnTargets() == 1;
			if (!g_bSWinOk)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMove] win state wrong: player (%u,%u) moves %u boxes %u",
					pxGame->GetPlayerX(), pxGame->GetPlayerY(), pxGame->GetMoveCount(), pxGame->GetBoxesOnTargets());
				return false;
			}
			// Pin the exact HUD strings (the UI formatting under conversion).
			{
				const std::string strStatus = GetHUDText("Status");
				const std::string strProgress = GetHUDText("Progress");
				const std::string strMinMoves = GetHUDText("MinMoves");
				const std::string strWin = GetHUDText("WinText");
				g_bSHUDOk = strStatus == "Moves: 2"
					&& strProgress == "Boxes: 1 / 1"
					&& strMinMoves == "Min Moves: 2"
					&& strWin == "LEVEL COMPLETE!";
				if (!g_bSHUDOk)
				{
					Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMove] HUD text wrong: '%s' | '%s' | '%s' | '%s'",
						strStatus.c_str(), strProgress.c_str(), strMinMoves.c_str(), strWin.c_str());
					return false;
				}
			}
			g_eSMovePhase = SMovePhase::PressAfterWin;
			return true;
		}
		return ++g_iSMoveFrame < 30;

	case SMovePhase::PressAfterWin:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_A);
		g_eSMovePhase = SMovePhase::HoldAfterWin;
		g_iSMoveFrame = 0;
		return true;

	case SMovePhase::HoldAfterWin:
		if (++g_iSMoveFrame < 20)
		{
			return true;
		}
		if (pxGame == nullptr) return false;
		g_bSWonGateOk = pxGame->GetPlayerX() == 3 && pxGame->GetMoveCount() == 2;
		if (!g_bSWonGateOk)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMove] input leaked after win: player (%u,%u) moves %u",
				pxGame->GetPlayerX(), pxGame->GetPlayerY(), pxGame->GetMoveCount());
		}
		g_eSMovePhase = SMovePhase::Done;
		return false;

	case SMovePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_SokobanMoveFlow()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSBlockedOk) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMove] blocked-move behaviour wrong"); }
	if (!g_bSNeutralOk) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMove] neutral move wrong"); }
	if (!g_bSWinOk)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMove] push/win wrong"); }
	if (!g_bSHUDOk)     { Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMove] HUD text wrong"); }
	if (!g_bSWonGateOk) { Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanMove] won-gate wrong"); }
	return g_bSBlockedOk && g_bSNeutralOk && g_bSWinOk && g_bSHUDOk && g_bSWonGateOk;
}

static const Zenith_AutomatedTest g_xSokobanMoveFlowTest = {
	"Sokoban_MoveFlow_Test",
	&Setup_SokobanMoveFlow,
	&Step_SokobanMoveFlow,
	&Verify_SokobanMoveFlow,
	/*maxFrames*/ 2000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xSokobanMoveFlowTest);

// ============================================================================
// Sokoban_ResetFlow_Test
// ============================================================================

namespace
{
	enum class SResetPhase { Boot, WaitPlaying, LoadFixture, MakeMove, AwaitMove, PressReset, AwaitReset, Done };

	SResetPhase g_eSResetPhase = SResetPhase::Boot;
	int         g_iSResetFrame = 0;
	bool        g_bSResetOk = false;
}

static void Setup_SokobanResetFlow()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eSResetPhase = SResetPhase::Boot;
	g_iSResetFrame = 0;
	g_bSResetOk = false;
}

static bool Step_SokobanResetFlow(int iFrame)
{
	Sokoban_GameComponent* pxGame = FindSokobanGame();
	switch (g_eSResetPhase)
	{
	case SResetPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eSResetPhase = SResetPhase::WaitPlaying;
		return true;

	case SResetPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == SokobanGameState::PLAYING)
		{
			g_eSResetPhase = SResetPhase::LoadFixture;
			g_iSResetFrame = 0;
		}
		return iFrame < 600;

	case SResetPhase::LoadFixture:
		if (++g_iSResetFrame < 10)
		{
			return true;
		}
		if (pxGame == nullptr) return false;
		pxGame->Test_LoadFixtureLevel();
		g_eSResetPhase = SResetPhase::MakeMove;
		g_iSResetFrame = 0;
		return true;

	case SResetPhase::MakeMove:
		if (++g_iSResetFrame < 5)
		{
			return true;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_D);
		g_eSResetPhase = SResetPhase::AwaitMove;
		g_iSResetFrame = 0;
		return true;

	case SResetPhase::AwaitMove:
		if (pxGame == nullptr) return false;
		if (pxGame->GetMoveCount() == 1 && !pxGame->IsAnimating())
		{
			g_eSResetPhase = SResetPhase::PressReset;
			return true;
		}
		return ++g_iSResetFrame < 30;

	case SResetPhase::PressReset:
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_R);
		g_eSResetPhase = SResetPhase::AwaitReset;
		g_iSResetFrame = 0;
		return true;

	case SResetPhase::AwaitReset:
		if (pxGame == nullptr) return false;
		if (pxGame->GetMoveCount() == 0)
		{
			// Fresh generated level: not won, solver-validated (>= 5 moves -
			// the generator's acceptance bar), still PLAYING.
			g_bSResetOk = !pxGame->IsWon()
				&& pxGame->GetMinMoves() >= 5
				&& pxGame->GetGameState() == SokobanGameState::PLAYING;
			if (!g_bSResetOk)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanReset] post-reset wrong: won %d minMoves %u state %d",
					pxGame->IsWon() ? 1 : 0, pxGame->GetMinMoves(), static_cast<int>(pxGame->GetGameState()));
			}
			g_eSResetPhase = SResetPhase::Done;
			return false;
		}
		return ++g_iSResetFrame < 120;

	case SResetPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_SokobanResetFlow()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSResetOk)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanReset] R did not regenerate a fresh level");
	}
	return g_bSResetOk;
}

static const Zenith_AutomatedTest g_xSokobanResetFlowTest = {
	"Sokoban_ResetFlow_Test",
	&Setup_SokobanResetFlow,
	&Step_SokobanResetFlow,
	&Verify_SokobanResetFlow,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xSokobanResetFlowTest);

// ============================================================================
// Sokoban_EscapeMenu_Test
// ============================================================================

namespace
{
	enum class SEscPhase { Boot, WaitPlaying, PressEsc, AwaitMenu, Done };

	SEscPhase g_eSEscPhase = SEscPhase::Boot;
	int       g_iSEscFrame = 0;
	bool      g_bSEscReachedMenu = false;
}

static void Setup_SokobanEscapeMenu()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eSEscPhase = SEscPhase::Boot;
	g_iSEscFrame = 0;
	g_bSEscReachedMenu = false;
}

static bool Step_SokobanEscapeMenu(int iFrame)
{
	Sokoban_GameComponent* pxGame = FindSokobanGame();
	switch (g_eSEscPhase)
	{
	case SEscPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eSEscPhase = SEscPhase::WaitPlaying;
		return true;

	case SEscPhase::WaitPlaying:
		if (pxGame != nullptr && pxGame->GetGameState() == SokobanGameState::PLAYING)
		{
			g_eSEscPhase = SEscPhase::PressEsc;
			g_iSEscFrame = 0;
		}
		return iFrame < 600;

	case SEscPhase::PressEsc:
		if (++g_iSEscFrame < 30)
		{
			return true;
		}
		Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_ESCAPE);
		g_eSEscPhase = SEscPhase::AwaitMenu;
		g_iSEscFrame = 0;
		return true;

	case SEscPhase::AwaitMenu:
		if (pxGame != nullptr && pxGame->GetGameState() == SokobanGameState::MAIN_MENU)
		{
			g_bSEscReachedMenu = true;
			g_eSEscPhase = SEscPhase::Done;
			return false;
		}
		return ++g_iSEscFrame < 600;

	case SEscPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_SokobanEscapeMenu()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSEscReachedMenu)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[SokobanEsc] Esc did not return to the main menu");
	}
	return g_bSEscReachedMenu;
}

static const Zenith_AutomatedTest g_xSokobanEscapeMenuTest = {
	"Sokoban_EscapeMenu_Test",
	&Setup_SokobanEscapeMenu,
	&Step_SokobanEscapeMenu,
	&Verify_SokobanEscapeMenu,
	/*maxFrames*/ 1500,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xSokobanEscapeMenuTest);

#endif // ZENITH_INPUT_SIMULATOR
