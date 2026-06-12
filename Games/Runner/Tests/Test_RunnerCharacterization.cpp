#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * Test_RunnerCharacterization - characterization test for the wave-2 graph
 * conversion of Runner's scoring / game-over flow.
 *
 * Written against the C++ version FIRST (the score accumulation on collectible
 * pickup and the obstacle-hit / character-dead -> GAME_OVER decisions inside
 * Runner_GameComponent::OnUpdate's PLAYING branch); the graph version must
 * keep it green unchanged. Probes go through read-only accessors that survive
 * the conversion.
 *
 *   Runner_GameOverFlow_Test - the character auto-runs with no evasive input;
 *                              spawned obstacles eventually end the run, the
 *                              state flips to GAME_OVER, and the high score
 *                              equals the final score (first run, score <=
 *                              high). Score stays monotonic along the way.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Components/Runner_GameComponent.h"

namespace
{
	Runner_GameComponent* FindRunnerGame()
	{
		Runner_GameComponent* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<Runner_GameComponent>().ForEach(
			[&pxFound](Zenith_EntityID, Runner_GameComponent& xGame)
			{
				if (pxFound == nullptr) pxFound = &xGame;
			});
		return pxFound;
	}

	enum class RunPhase { Boot, WaitPlaying, Run, Done };

	RunPhase g_eRunPhase = RunPhase::Boot;
	int      g_iRunFrame = 0;
	bool     g_bSawGameOver = false;
	bool     g_bScoreMonotonic = true;
	uint32_t g_uLastScore = 0;
	uint32_t g_uFinalScore = 0;
	uint32_t g_uFinalHighScore = 0;
}

static void Setup_GameOverFlow()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	g_eRunPhase = RunPhase::Boot;
	g_iRunFrame = 0;
	g_bSawGameOver = false;
	g_bScoreMonotonic = true;
	g_uLastScore = 0;
	g_uFinalScore = 0;
	g_uFinalHighScore = 0;
}

static bool Step_GameOverFlow(int iFrame)
{
	switch (g_eRunPhase)
	{
	case RunPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_eRunPhase = RunPhase::WaitPlaying;
		return true;

	case RunPhase::WaitPlaying:
	{
		Runner_GameComponent* pxGame = FindRunnerGame();
		if (pxGame != nullptr && pxGame->GetGameState() == RunnerGameState::PLAYING)
		{
			g_eRunPhase = RunPhase::Run;
			g_iRunFrame = 0;
			return true;
		}
		return iFrame < 600;
	}

	case RunPhase::Run:
	{
		Runner_GameComponent* pxGame = FindRunnerGame();
		if (pxGame == nullptr) return false;

		const uint32_t uScore = pxGame->GetScore();
		if (uScore < g_uLastScore)
		{
			g_bScoreMonotonic = false;
		}
		g_uLastScore = uScore;

		if (pxGame->GetGameState() == RunnerGameState::GAME_OVER)
		{
			g_bSawGameOver = true;
			g_uFinalScore = pxGame->GetScore();
			g_uFinalHighScore = pxGame->GetHighScore();
			g_eRunPhase = RunPhase::Done;
			return false;
		}
		// No evasive input - the auto-runner hits an obstacle (or dies) on its
		// own. Two minutes of game time is far beyond the expected survival.
		return ++g_iRunFrame < 7200;
	}

	case RunPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_GameOverFlow()
{
	Zenith_InputSimulator::ClearFixedDt();
	if (!g_bSawGameOver)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerGameOver] never reached GAME_OVER");
		return false;
	}
	if (!g_bScoreMonotonic)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerGameOver] score decreased mid-run");
		return false;
	}
	// First run of the process: the high score must have absorbed the final
	// score (the obstacle-hit path syncs it).
	if (g_uFinalHighScore < g_uFinalScore)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[RunnerGameOver] high score %u < final score %u",
			g_uFinalHighScore, g_uFinalScore);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xRunnerGameOverFlowTest = {
	"Runner_GameOverFlow_Test",
	&Setup_GameOverFlow,
	&Step_GameOverFlow,
	&Verify_GameOverFlow,
	/*maxFrames*/ 9000,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRunnerGameOverFlowTest);

#endif // ZENITH_INPUT_SIMULATOR
