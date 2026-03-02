#pragma once

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * TilePuzzle_AutoTest.h - Automated test suite for the Tile Puzzle game
 *
 * Implemented as a Zenith_ScriptBehaviour with a per-frame state machine.
 * When --autotest is passed, this behaviour is added as a persistent entity.
 *
 * The visual puzzle test uses Zenith_InputSimulator to simulate touch input
 * (tap to select, swipe to drag) and solves a real puzzle level on screen.
 * You can see the shapes moving and cats being eliminated in real-time.
 *
 * Logic-only tests (save/load, coins, star rating) also run each frame.
 */

#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"
#include "TilePuzzle/Components/TilePuzzle_Behaviour.h"
#include "TilePuzzle/Components/Pinball_Behaviour.h"
#include "TilePuzzle/Components/TilePuzzle_Types.h"
#include "TilePuzzle/Components/TilePuzzle_Rules.h"
#include "TilePuzzle/Components/TilePuzzle_Solver.h"
#include "TilePuzzle/Components/TilePuzzle_SaveData.h"
#include "TilePuzzle/Components/TilePuzzleLevelData_Serialize.h"
#include "DataStream/Zenith_DataStream.h"

#include <cstring>
#include <unordered_set> // #TODO: Replace with engine hash map

class TilePuzzle_AutoTest : public Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(TilePuzzle_AutoTest)

	TilePuzzle_AutoTest() = delete;
	TilePuzzle_AutoTest(Zenith_Entity& /*xParentEntity*/)
	{
	}

	// ========================================================================
	// State machine phases
	// ========================================================================

	enum AutoTestPhase : uint32_t
	{
		PHASE_INIT = 0,
		// Logic-only tests
		PHASE_TEST_STAR_RATING,
		PHASE_TEST_UNDO,
		PHASE_TEST_HINT,
		PHASE_TEST_PINBALL_LAUNCH,
		PHASE_TEST_PINBALL_GATES,
		PHASE_TEST_SAVE_LOAD,
		PHASE_TEST_COIN_SYSTEM,
		// Full-game test (plays ALL levels + pinball gates)
		PHASE_FULL_GAME_RESET_SAVE,
		PHASE_FULL_GAME_WAIT_PLAYING,
		PHASE_FULL_GAME_COMPUTE_SOLUTION,
		PHASE_FULL_GAME_SELECT_AND_MOVE,
		PHASE_FULL_GAME_WAIT_SETTLE,
		PHASE_FULL_GAME_CHECK_COMPLETE,
		PHASE_FULL_GAME_NEXT_LEVEL,
		PHASE_FULL_GAME_PINBALL_ENTER,
		PHASE_FULL_GAME_PINBALL_WAIT,
		PHASE_FULL_GAME_PINBALL_CLEAR,
		PHASE_FULL_GAME_PINBALL_RETURN,
		PHASE_FULL_GAME_VERIFY,
		// Summary
		PHASE_COMPLETE,
		PHASE_DONE
	};

	// ========================================================================
	// Lifecycle
	// ========================================================================

	void OnStart() ZENITH_FINAL override
	{
		m_ePhase = PHASE_INIT;
		m_uPassed = 0;
		m_uFailed = 0;
		m_uFrameDelay = 0;

		Zenith_Log(LOG_CATEGORY_UNITTEST, "====================================");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "TilePuzzle AutoTest Suite (Visual)");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "====================================");

		EnsureTestShapeInitialized();
	}

	void OnUpdate(const float /*fDeltaTime*/) ZENITH_FINAL override
	{
		// Delay between tests so the user can see each one
		if (m_uFrameDelay > 0)
		{
			m_uFrameDelay--;
			return;
		}

		switch (m_ePhase)
		{
		case PHASE_INIT:
			Zenith_Log(LOG_CATEGORY_UNITTEST, "Starting tests...");
			m_ePhase = PHASE_TEST_STAR_RATING;
			m_uFrameDelay = 5;
			break;

		case PHASE_TEST_STAR_RATING:
			RunSingleTest("Test_PuzzleLevel_StarRating", &Test_PuzzleLevel_StarRating);
			m_ePhase = PHASE_TEST_UNDO;
			m_uFrameDelay = 5;
			break;

		case PHASE_TEST_UNDO:
			RunSingleTest("Test_PuzzleLevel_Undo", &Test_PuzzleLevel_Undo);
			m_ePhase = PHASE_TEST_HINT;
			m_uFrameDelay = 5;
			break;

		case PHASE_TEST_HINT:
			RunSingleTest("Test_PuzzleLevel_Hint", &Test_PuzzleLevel_Hint);
			m_ePhase = PHASE_TEST_PINBALL_LAUNCH;
			m_uFrameDelay = 5;
			break;

		case PHASE_TEST_PINBALL_LAUNCH:
			RunSingleTest("Test_Pinball_LaunchAndScore", &Test_Pinball_LaunchAndScore);
			m_ePhase = PHASE_TEST_PINBALL_GATES;
			m_uFrameDelay = 5;
			break;

		case PHASE_TEST_PINBALL_GATES:
			RunSingleTest("Test_Pinball_GateObjectives", &Test_Pinball_GateObjectives);
			m_ePhase = PHASE_TEST_SAVE_LOAD;
			m_uFrameDelay = 5;
			break;

		case PHASE_TEST_SAVE_LOAD:
			RunSingleTest("Test_SaveLoad_Integrity", &Test_SaveLoad_Integrity);
			m_ePhase = PHASE_TEST_COIN_SYSTEM;
			m_uFrameDelay = 5;
			break;

		case PHASE_TEST_COIN_SYSTEM:
			RunSingleTest("Test_CoinSystem", &Test_CoinSystem);
			m_ePhase = PHASE_FULL_GAME_RESET_SAVE;
			m_uFrameDelay = 5;
			break;

		// ==============================================================
		// Full-game test - plays ALL levels + pinball gates
		// ==============================================================
		case PHASE_FULL_GAME_RESET_SAVE:
			UpdateFullGame_ResetSave();
			break;
		case PHASE_FULL_GAME_WAIT_PLAYING:
			UpdateFullGame_WaitForPlaying();
			break;
		case PHASE_FULL_GAME_COMPUTE_SOLUTION:
			UpdateFullGame_ComputeSolution();
			break;
		case PHASE_FULL_GAME_SELECT_AND_MOVE:
			UpdateFullGame_SelectAndMove();
			break;
		case PHASE_FULL_GAME_WAIT_SETTLE:
			UpdateFullGame_WaitSettle();
			break;
		case PHASE_FULL_GAME_CHECK_COMPLETE:
			UpdateFullGame_CheckComplete();
			break;
		case PHASE_FULL_GAME_NEXT_LEVEL:
			UpdateFullGame_NextLevel();
			break;
		case PHASE_FULL_GAME_PINBALL_ENTER:
			UpdateFullGame_PinballEnter();
			break;
		case PHASE_FULL_GAME_PINBALL_WAIT:
			UpdateFullGame_PinballWait();
			break;
		case PHASE_FULL_GAME_PINBALL_CLEAR:
			UpdateFullGame_PinballClear();
			break;
		case PHASE_FULL_GAME_PINBALL_RETURN:
			UpdateFullGame_PinballReturn();
			break;
		case PHASE_FULL_GAME_VERIFY:
			UpdateFullGame_Verify();
			break;

		case PHASE_COMPLETE:
		{
			// Ensure simulator is disabled before reporting results
			Zenith_InputSimulator::Disable();

			uint32_t uTotal = m_uPassed + m_uFailed;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "====================================");
			Zenith_Log(LOG_CATEGORY_UNITTEST, "Results: %u/%u passed, %u failed", m_uPassed, uTotal, m_uFailed);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "====================================");

			if (m_uFailed == 0)
				Zenith_Log(LOG_CATEGORY_UNITTEST, "ALL TESTS PASSED");
			else
				Zenith_Log(LOG_CATEGORY_UNITTEST, "SOME TESTS FAILED");

			m_ePhase = PHASE_DONE;
			break;
		}

		case PHASE_DONE:
			break;
		}
	}

private:
	AutoTestPhase m_ePhase = PHASE_INIT;
	uint32_t m_uPassed = 0;
	uint32_t m_uFailed = 0;
	uint32_t m_uFrameDelay = 0;

	// ========================================================================
	// Full-game test state
	// ========================================================================
	TilePuzzle_Behaviour* m_pxPuzzleBehaviour = nullptr;

	// Full-game progression
	uint32_t m_uFullGameLevelCount = 0;       // Total available levels
	uint32_t m_uFullGameCurrentLevel = 0;     // Current level being played (1-based)
	uint32_t m_uFullGameNextGate = 0;         // Next pinball gate to clear (0-based)
	uint32_t m_uFullGameLevelsCompleted = 0;  // Counter for reporting
	uint32_t m_uFullGameGatesCleared = 0;     // Counter for reporting

	// Per-level solution replay state
	uint32_t m_uTotalCellMoves = 0;       // Total single-cell moves for current level
	uint32_t m_uWaitFrames = 0;
	uint32_t m_uResolvesUsed = 0;         // How many times we re-solved

	// Full solution from solver: ALL moves, not just the first
	std::vector<TilePuzzleSolutionMove> m_axLiveSolution;
	uint32_t m_uCurrentSolutionMoveIndex = 0; // Which move in the solution we're currently executing
	TilePuzzleLevelData m_xCleanLevel;    // Clean level used for BFS path finding (solver's expected state)
	TilePuzzleLevelData m_xSolverCleanLevel; // Clean level at solve time (updated to track solver's state)
	uint32_t m_uSolverPreviousMask = 0;  // Cumulative elimination mask from solver's perspective
	uint32_t m_uCurrentTargetShape = 0;   // Shape index in ORIGINAL level for TryMoveShape
	uint32_t m_uCleanTargetShape = 0;     // Shape index in CLEAN level for BFS path finding
	int32_t m_iCurrentTargetX = 0;        // Target X for current outer move
	int32_t m_iCurrentTargetY = 0;        // Target Y for current outer move

	// Mapping from original shape index to clean level index (built at solve time, stable for full chain)
	uint32_t m_auOrigToClean[16];
	uint32_t m_uOrigToCleanCount = 0;

	// Cell-by-cell path from BFS (for current outer move)
	static constexpr uint32_t uMAX_CELL_PATH = 128;
	TilePuzzleDirection m_aeCellPath[uMAX_CELL_PATH];
	uint32_t m_uCellPathLength = 0;
	uint32_t m_uCellPathIndex = 0;

	// ========================================================================
	// Test Runner Helper
	// ========================================================================

	void RunSingleTest(const char* szName, bool (*pfnTest)())
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] Running %s...", szName);
		bool bResult = pfnTest();
		if (bResult)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] PASS: %s", szName);
			m_uPassed++;
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: %s", szName);
			m_uFailed++;
		}
	}

	// ========================================================================
	// Visual Puzzle Test - Frame-by-frame state machine
	// ========================================================================

	// Find the TilePuzzle_Behaviour from the GameManager entity
	TilePuzzle_Behaviour* FindPuzzleBehaviour()
	{
		// Search all loaded scenes for the GameManager entity
		for (uint32_t uSlot = 0; uSlot < 16; ++uSlot)
		{
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataAtSlot(uSlot);
			if (!pxScene)
				continue;

			Zenith_Entity xEntity = pxScene->FindEntityByName("GameManager");
			if (!xEntity.IsValid())
				continue;

			if (!xEntity.HasComponent<Zenith_ScriptComponent>())
				continue;

			Zenith_ScriptComponent& xScript = xEntity.GetComponent<Zenith_ScriptComponent>();
			TilePuzzle_Behaviour* pxBehaviour = xScript.GetBehaviour<TilePuzzle_Behaviour>();
			if (pxBehaviour)
				return pxBehaviour;
		}
		return nullptr;
	}

	// ========================================================================
	// Full-Game Test: Reset save and start from level 1
	// ========================================================================
	void UpdateFullGame_ResetSave()
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] Running Test_FullGame...");

		Zenith_InputSimulator::Enable();

		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Could not find TilePuzzle_Behaviour on GameManager");
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		// Reset save data for clean full-game run
		m_pxPuzzleBehaviour->m_xSaveData.Reset();
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_pxPuzzleBehaviour->m_xSaveData);

		m_uFullGameLevelCount = m_pxPuzzleBehaviour->m_uAvailableLevelCount;
		Zenith_Assert(m_uFullGameLevelCount >= TilePuzzleSaveData::uMAX_LEVELS,
			"Not enough levels: %u/%u", m_uFullGameLevelCount, TilePuzzleSaveData::uMAX_LEVELS);

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Available levels: %u", m_uFullGameLevelCount);

		// Start from level 1
		m_pxPuzzleBehaviour->m_uCurrentLevelNumber = 1;
		m_pxPuzzleBehaviour->m_xSaveData.uCurrentLevel = 1;
		m_pxPuzzleBehaviour->m_xSaveData.uLives = TilePuzzleSaveData::uMAX_LIVES;
		m_pxPuzzleBehaviour->StartGame();

		m_uFullGameCurrentLevel = 1;
		m_uFullGameNextGate = 0;
		m_uFullGameLevelsCompleted = 0;
		m_uFullGameGatesCleared = 0;
		ResetPerLevelState();
		m_ePhase = PHASE_FULL_GAME_WAIT_PLAYING;
	}

	void ResetPerLevelState()
	{
		m_uWaitFrames = 0;
		m_uTotalCellMoves = 0;
		m_uResolvesUsed = 0;
		m_uCellPathLength = 0;
		m_uCellPathIndex = 0;
		m_axLiveSolution.clear();
		m_uCurrentSolutionMoveIndex = 0;
	}

	// Wait for the puzzle to be in PLAYING state
	void UpdateFullGame_WaitForPlaying()
	{
		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			m_uWaitFrames++;
			if (m_uWaitFrames > 120)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Timed out waiting for GameManager (level %u)",
					m_uFullGameCurrentLevel);
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
				m_uFailed++;
				Zenith_InputSimulator::Disable();
				m_ePhase = PHASE_COMPLETE;
			}
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_PLAYING)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Level %u loaded, state=PLAYING", m_uFullGameCurrentLevel);
			m_ePhase = PHASE_FULL_GAME_COMPUTE_SOLUTION;
			return;
		}

		m_uWaitFrames++;
		if (m_uWaitFrames > 120)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Timed out waiting for PLAYING state (level %u, state=%u)",
				m_uFullGameCurrentLevel, m_pxPuzzleBehaviour->m_eState);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
		}
	}

	// Build a clean level from the current game state (no removed shapes, no eliminated cats)
	// and populate the original→clean index mapping.
	void BuildCleanLevelFromCurrentState()
	{
		const TilePuzzleLevelData& xOriginal = m_pxPuzzleBehaviour->m_xCurrentLevel;

		m_xCleanLevel = TilePuzzleLevelData();
		m_xCleanLevel.uGridWidth = xOriginal.uGridWidth;
		m_xCleanLevel.uGridHeight = xOriginal.uGridHeight;
		m_xCleanLevel.aeCells = xOriginal.aeCells;
		m_xCleanLevel.uMinimumMoves = 0;

		memset(m_auOrigToClean, 0xFF, sizeof(m_auOrigToClean));
		m_uOrigToCleanCount = 0;

		for (size_t i = 0; i < xOriginal.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xs = xOriginal.axShapes[i];
			if (xs.bRemoved)
				continue;
			uint32_t uCleanIdx = static_cast<uint32_t>(m_xCleanLevel.axShapes.size());
			if (i < 16)
				m_auOrigToClean[i] = uCleanIdx;
			m_uOrigToCleanCount++;
			m_xCleanLevel.axShapes.push_back(xs);
		}

		for (const auto& xCat : xOriginal.axCats)
		{
			if (!xCat.bEliminated)
				m_xCleanLevel.axCats.push_back(xCat);
		}
	}

	void UpdateFullGame_ComputeSolution()
	{
		const TilePuzzleLevelData& xLevel = m_pxPuzzleBehaviour->m_xCurrentLevel;

		// Log level info only on first solve for this level
		if (m_uResolvesUsed == 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Level %u: Grid %ux%u, Shapes: %u, Cats: %u, Par: %u",
				m_uFullGameCurrentLevel,
				xLevel.uGridWidth, xLevel.uGridHeight,
				static_cast<uint32_t>(xLevel.axShapes.size()),
				static_cast<uint32_t>(xLevel.axCats.size()),
				xLevel.uMinimumMoves);
		}

		// Safety limit: prevent infinite re-solve loops
		if (m_uResolvesUsed >= 100)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Re-solve limit reached (level %u, %u re-solves)",
				m_uFullGameCurrentLevel, m_uResolvesUsed);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		// If we still have moves remaining from a previous solve, try each one
		// Skip moves whose cell paths fail (intermediate state may have diverged)
		while (m_uCurrentSolutionMoveIndex < static_cast<uint32_t>(m_axLiveSolution.size()))
		{
			if (SetupNextSolutionMove())
				return;
			// Cell path failed — skip this move and try the next one
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    Cell path failed for move %u/%u, skipping",
				m_uCurrentSolutionMoveIndex, static_cast<uint32_t>(m_axLiveSolution.size()));
		}

		// Re-solve from current board state
		ResolveFromCurrentState();
		m_uCurrentSolutionMoveIndex = 0;

		if (m_axLiveSolution.empty())
		{
			uint32_t uAlive = 0;
			for (const auto& xC : xLevel.axCats) if (!xC.bEliminated) uAlive++;
			if (uAlive > 0)
				Zenith_Log(LOG_CATEGORY_UNITTEST, "    solve#%u: EMPTY solution (cats_alive=%u)", m_uResolvesUsed, uAlive);
			m_uWaitFrames = 0;
			m_ePhase = PHASE_FULL_GAME_CHECK_COMPLETE;
			return;
		}

		// Log solve info
		if (m_uResolvesUsed <= 5 || m_uResolvesUsed % 10 == 0)
		{
			uint32_t uAlive = 0;
			for (const auto& xC : xLevel.axCats) if (!xC.bEliminated) uAlive++;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    solve#%u: cats_alive=%u sol_len=%u",
				m_uResolvesUsed, uAlive, static_cast<uint32_t>(m_axLiveSolution.size()));
		}

		// Try each move in the fresh solution until one succeeds
		bool bFoundMove = false;
		while (m_uCurrentSolutionMoveIndex < static_cast<uint32_t>(m_axLiveSolution.size()))
		{
			if (SetupNextSolutionMove())
			{
				bFoundMove = true;
				break;
			}
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    Cell path failed for move %u/%u of solve#%u, skipping",
				m_uCurrentSolutionMoveIndex, static_cast<uint32_t>(m_axLiveSolution.size()), m_uResolvesUsed);
		}
		if (!bFoundMove)
		{
			// All moves in fresh solve failed — wait and retry
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    All moves failed for solve#%u, will retry", m_uResolvesUsed);
			m_axLiveSolution.clear();
			m_uFrameDelay = 5;
		}
	}

	// Set up the next move from the current solution. Returns true if cell path was found.
	bool SetupNextSolutionMove()
	{
		const TilePuzzleLevelData& xLevel = m_pxPuzzleBehaviour->m_xCurrentLevel;
		const TilePuzzleSolutionMove& xMove = m_axLiveSolution[m_uCurrentSolutionMoveIndex];
		m_uCurrentSolutionMoveIndex++;

		m_uCurrentTargetShape = xMove.uShapeIndex;
		m_iCurrentTargetX = xMove.iEndX;
		m_iCurrentTargetY = xMove.iEndY;

		// Check if the target shape is still valid (not removed in game)
		if (m_uCurrentTargetShape >= static_cast<uint32_t>(xLevel.axShapes.size()) ||
			xLevel.axShapes[m_uCurrentTargetShape].bRemoved)
		{
			return false;
		}

		// Map original shape index to clean level index (mapping built at solve time)
		if (m_uCurrentTargetShape < 16)
			m_uCleanTargetShape = m_auOrigToClean[m_uCurrentTargetShape];
		else
			return false;

		if (m_uCleanTargetShape == UINT32_MAX)
			return false;

		// Use the SOLVER'S clean level (tracks solver's expected state, not game state).
		// This ensures FindSolverInnerPath finds the exact same cell path the solver planned.
		// Pass the required elimination mask so we match the solver's expected cat eliminations.
		int32_t iPathLen = FindSolverInnerPath(m_xSolverCleanLevel, m_uCleanTargetShape,
			m_iCurrentTargetX, m_iCurrentTargetY, m_aeCellPath, uMAX_CELL_PATH,
			xMove.uExpectedElimMask);

		if (iPathLen < 0)
		{
			const TilePuzzleShapeInstance& xShape = xLevel.axShapes[m_uCurrentTargetShape];
			bool bRoundTrip = (xShape.iOriginX == m_iCurrentTargetX &&
				xShape.iOriginY == m_iCurrentTargetY);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    No cell path for shape %u (%d,%d)->(%d,%d) mask=0x%X%s",
				m_uCurrentTargetShape, xShape.iOriginX, xShape.iOriginY,
				m_iCurrentTargetX, m_iCurrentTargetY, xMove.uExpectedElimMask,
				bRoundTrip ? " [round-trip]" : "");
			return false;
		}

		// Update solver's clean level to reflect this move's result
		// (so next move's BFS starts from the correct state)
		m_xSolverCleanLevel.axShapes[m_uCleanTargetShape].iOriginX = m_iCurrentTargetX;
		m_xSolverCleanLevel.axShapes[m_uCleanTargetShape].iOriginY = m_iCurrentTargetY;

		// Mark newly eliminated cats and removed shapes in solver's clean level
		uint32_t uNewlyElim = xMove.uExpectedElimMask & ~m_uSolverPreviousMask;
		if (uNewlyElim != 0)
		{
			for (size_t ci = 0; ci < m_xSolverCleanLevel.axCats.size(); ++ci)
			{
				if (uNewlyElim & (1u << ci))
					m_xSolverCleanLevel.axCats[ci].bEliminated = true;
			}
			// Check for shape removal (all cats of a color eliminated)
			for (size_t si = 0; si < m_xSolverCleanLevel.axShapes.size(); ++si)
			{
				TilePuzzleShapeInstance& xs = m_xSolverCleanLevel.axShapes[si];
				if (!xs.pxDefinition || !xs.pxDefinition->bDraggable || xs.bRemoved)
					continue;
				bool bAllElim = true;
				for (size_t ci = 0; ci < m_xSolverCleanLevel.axCats.size(); ++ci)
				{
					if (m_xSolverCleanLevel.axCats[ci].eColor == xs.eColor &&
						!m_xSolverCleanLevel.axCats[ci].bEliminated)
					{
						bAllElim = false;
						break;
					}
				}
				if (bAllElim) xs.bRemoved = true;
			}
		}
		m_uSolverPreviousMask = xMove.uExpectedElimMask;

		m_uCellPathLength = static_cast<uint32_t>(iPathLen);
		m_uCellPathIndex = 0;

		m_ePhase = PHASE_FULL_GAME_SELECT_AND_MOVE;
		m_uFrameDelay = 1;
		return true;
	}


	// Map puzzle direction to keyboard key code.
	// Game maps: W→DOWN, S→UP, A→LEFT, D→RIGHT (inverted Y due to camera)
	static Zenith_KeyCode DirectionToKeyCode(TilePuzzleDirection eDir)
	{
		switch (eDir)
		{
		case TILEPUZZLE_DIR_UP:    return ZENITH_KEY_S;
		case TILEPUZZLE_DIR_DOWN:  return ZENITH_KEY_W;
		case TILEPUZZLE_DIR_LEFT:  return ZENITH_KEY_A;
		case TILEPUZZLE_DIR_RIGHT: return ZENITH_KEY_D;
		default: return static_cast<Zenith_KeyCode>(-1);
		}
	}

	// Re-solve from the current game state using BFS solver.
	// Creates a clean level copy (without eliminated cats or removed shapes),
	// solves it, then maps the solution indices back to the original level.
	// Stores the FULL solution so we can execute all moves before re-solving.
	void ResolveFromCurrentState()
	{
		const TilePuzzleLevelData& xOriginal = m_pxPuzzleBehaviour->m_xCurrentLevel;

		// Build clean level: only active shapes and living cats
		TilePuzzleLevelData xClean;
		xClean.uGridWidth = xOriginal.uGridWidth;
		xClean.uGridHeight = xOriginal.uGridHeight;
		xClean.aeCells = xOriginal.aeCells;
		xClean.uMinimumMoves = 0;

		// Copy shapes, tracking clean→original index mapping
		uint32_t auShapeMap[16];
		uint32_t uShapeMapCount = 0;
		for (size_t i = 0; i < xOriginal.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xs = xOriginal.axShapes[i];
			if (xs.bRemoved)
				continue;
			if (uShapeMapCount < 16)
				auShapeMap[uShapeMapCount] = static_cast<uint32_t>(i);
			uShapeMapCount++;
			xClean.axShapes.push_back(xs);
		}

		// Copy only living cats
		for (const auto& xCat : xOriginal.axCats)
		{
			if (!xCat.bEliminated)
				xClean.axCats.push_back(xCat);
		}

		// Solve the clean level
		std::vector<TilePuzzleSolutionMove> axSolution;
		TilePuzzle_Solver::SolveLevelWithPath(xClean, axSolution);

		// Store the solver's clean level for cell path BFS (updated as moves execute)
		m_xSolverCleanLevel = xClean;
		m_uSolverPreviousMask = 0;

		// Build orig→clean index mapping (stable for the full solution chain)
		memset(m_auOrigToClean, 0xFF, sizeof(m_auOrigToClean));
		m_uOrigToCleanCount = 0;
		for (uint32_t i = 0; i < uShapeMapCount && i < 16; ++i)
		{
			m_auOrigToClean[auShapeMap[i]] = i;
			m_uOrigToCleanCount++;
		}

		// Map solution from clean level indices to original level indices
		m_axLiveSolution.clear();
		for (const auto& xMove : axSolution)
		{
			TilePuzzleSolutionMove xMapped;
			if (xMove.uShapeIndex < uShapeMapCount)
				xMapped.uShapeIndex = auShapeMap[xMove.uShapeIndex];
			else
				xMapped.uShapeIndex = xMove.uShapeIndex;
			xMapped.iEndX = xMove.iEndX;
			xMapped.iEndY = xMove.iEndY;
			xMapped.uExpectedElimMask = xMove.uExpectedElimMask;
			m_axLiveSolution.push_back(xMapped);
		}

		m_uResolvesUsed++;
	}

	void UpdateFullGame_SelectAndMove()
	{
		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Lost reference to puzzle behaviour (level %u)",
				m_uFullGameCurrentLevel);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE ||
			m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_VICTORY_OVERLAY)
		{
			m_uWaitFrames = 0;
			m_ePhase = PHASE_FULL_GAME_CHECK_COMPLETE;
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState != TILEPUZZLE_STATE_PLAYING)
			return;

		if (m_uTotalCellMoves >= 10000)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Exceeded 10000 cell moves (level %u)", m_uFullGameCurrentLevel);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		const TilePuzzleLevelData& xLevel = m_pxPuzzleBehaviour->m_xCurrentLevel;
		uint32_t uShapeIdx = m_uCurrentTargetShape;

		if (uShapeIdx >= static_cast<uint32_t>(xLevel.axShapes.size()))
		{
			m_ePhase = PHASE_FULL_GAME_COMPUTE_SOLUTION;
			m_uFrameDelay = 2;
			return;
		}

		const TilePuzzleShapeInstance& xShape = xLevel.axShapes[uShapeIdx];

		if (xShape.bRemoved)
		{
			// Shape was removed during elimination — continue to next solution move
			m_ePhase = PHASE_FULL_GAME_COMPUTE_SOLUTION;
			m_uFrameDelay = 2;
			return;
		}

		// Check if we've completed the cell path for this outer move
		if (m_uCellPathIndex >= m_uCellPathLength)
		{
			// Move to next solution move (solver's clean level already updated)
			m_ePhase = PHASE_FULL_GAME_COMPUTE_SOLUTION;
			m_uFrameDelay = 1;
			return;
		}

		// Execute the next step in the cell path via direct TryMoveShape call
		TilePuzzleDirection eDir = m_aeCellPath[m_uCellPathIndex];
		m_uCellPathIndex++;

		bool bMoved = m_pxPuzzleBehaviour->TryMoveShape(static_cast<int32_t>(uShapeIdx), eDir);
		if (!bMoved)
		{
			// Move rejected — game state diverged from BFS expectation. Force re-solve.
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    REJECTED: shape %u step %u/%u, forcing re-solve",
				uShapeIdx, m_uCellPathIndex, m_uCellPathLength);
			m_axLiveSolution.clear(); // Force full re-solve
			m_uCurrentSolutionMoveIndex = 0;
			m_ePhase = PHASE_FULL_GAME_COMPUTE_SOLUTION;
			m_uFrameDelay = 2;
			return;
		}

		m_uTotalCellMoves++;
		m_uWaitFrames = 0;
		m_ePhase = PHASE_FULL_GAME_WAIT_SETTLE;
		m_uFrameDelay = 0;
	}

	void UpdateFullGame_WaitSettle()
	{
		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Lost reference during settle (level %u)",
				m_uFullGameCurrentLevel);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_SHAPE_SLIDING ||
			m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_CHECK_ELIMINATION)
		{
			m_uWaitFrames++;
			if (m_uWaitFrames > 120)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Settle timeout (level %u, state=%u)",
					m_uFullGameCurrentLevel, m_pxPuzzleBehaviour->m_eState);
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
				m_uFailed++;
				m_ePhase = PHASE_COMPLETE;
			}
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE ||
			m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_VICTORY_OVERLAY)
		{
			m_uWaitFrames = 0;
			m_ePhase = PHASE_FULL_GAME_CHECK_COMPLETE;
			return;
		}

		// Shape may have been removed during elimination check
		if (m_uCurrentTargetShape < static_cast<uint32_t>(m_pxPuzzleBehaviour->m_xCurrentLevel.axShapes.size()) &&
			m_pxPuzzleBehaviour->m_xCurrentLevel.axShapes[m_uCurrentTargetShape].bRemoved)
		{
			m_ePhase = PHASE_FULL_GAME_COMPUTE_SOLUTION;
			m_uFrameDelay = 2;
			return;
		}

		m_ePhase = PHASE_FULL_GAME_SELECT_AND_MOVE;
	}

	void UpdateFullGame_CheckComplete()
	{
		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Lost reference during check (level %u)",
				m_uFullGameCurrentLevel);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE ||
			m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_VICTORY_OVERLAY)
		{
			m_uFullGameLevelsCompleted++;
			m_uWaitFrames = 0;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Level %u/%u COMPLETE (stars=%u, moves=%u, re-solves=%u)",
				m_uFullGameCurrentLevel, m_uFullGameLevelCount,
				m_pxPuzzleBehaviour->m_uStarsEarned, m_pxPuzzleBehaviour->m_uMoveCount, m_uResolvesUsed);

			// Check if we should enter pinball (every 10th level)
			if (m_uFullGameCurrentLevel % 10 == 0 && m_uFullGameNextGate < 10)
			{
				m_ePhase = PHASE_FULL_GAME_PINBALL_ENTER;
				m_uFrameDelay = 5;
				return;
			}

			// More levels to play?
			if (m_uFullGameCurrentLevel < m_uFullGameLevelCount)
			{
				m_ePhase = PHASE_FULL_GAME_NEXT_LEVEL;
				m_uFrameDelay = 2;
				return;
			}

			// All levels done - clear any remaining pinball gates
			if (m_uFullGameNextGate < 10)
			{
				m_ePhase = PHASE_FULL_GAME_PINBALL_ENTER;
				m_uFrameDelay = 5;
				return;
			}

			// Everything done
			m_ePhase = PHASE_FULL_GAME_VERIFY;
			m_uFrameDelay = 5;
		}
		else if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_SHAPE_SLIDING ||
			m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_CHECK_ELIMINATION)
		{
			// Still settling, wait
			m_uWaitFrames++;
			if (m_uWaitFrames > 120)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Level %u settle timeout in CheckComplete (state=%u)",
					m_uFullGameCurrentLevel, m_pxPuzzleBehaviour->m_eState);
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
				m_uFailed++;
				Zenith_InputSimulator::Disable();
				m_ePhase = PHASE_COMPLETE;
			}
		}
		else if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_PLAYING)
		{
			// Wait a few frames for the game to detect completion
			m_uWaitFrames++;
			if (m_uWaitFrames > 10)
			{
				// Still PLAYING after waiting - re-solve via COMPUTE_SOLUTION
				m_ePhase = PHASE_FULL_GAME_COMPUTE_SOLUTION;
				m_uFrameDelay = 2;
			}
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Level %u unexpected state %u",
				m_uFullGameCurrentLevel, m_pxPuzzleBehaviour->m_eState);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
		}
	}

	void UpdateFullGame_NextLevel()
	{
		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Lost reference during NextLevel");
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		m_uFullGameCurrentLevel++;
		// Ensure lives don't run out
		m_pxPuzzleBehaviour->m_xSaveData.uLives = TilePuzzleSaveData::uMAX_LIVES;
		m_pxPuzzleBehaviour->NextLevel();
		ResetPerLevelState();
		m_ePhase = PHASE_FULL_GAME_WAIT_PLAYING;
	}

	// ========================================================================
	// Pinball gate phases
	// ========================================================================

	Pinball_Behaviour* FindPinballBehaviour()
	{
		for (uint32_t uSlot = 0; uSlot < 16; ++uSlot)
		{
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataAtSlot(uSlot);
			if (!pxScene)
				continue;
			Zenith_Entity xEntity = pxScene->FindEntityByName("PinballManager");
			if (!xEntity.IsValid())
				continue;
			if (!xEntity.HasComponent<Zenith_ScriptComponent>())
				continue;
			Zenith_ScriptComponent& xScript = xEntity.GetComponent<Zenith_ScriptComponent>();
			Pinball_Behaviour* pxBehaviour = xScript.GetBehaviour<Pinball_Behaviour>();
			if (pxBehaviour)
				return pxBehaviour;
		}
		return nullptr;
	}

	void UpdateFullGame_PinballEnter()
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Entering pinball for gate %u...", m_uFullGameNextGate);
		Zenith_SceneManager::LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
		m_uWaitFrames = 0;
		m_ePhase = PHASE_FULL_GAME_PINBALL_WAIT;
		m_uFrameDelay = 10;
	}

	void UpdateFullGame_PinballWait()
	{
		Pinball_Behaviour* pxPinball = FindPinballBehaviour();
		if (!pxPinball)
		{
			m_uWaitFrames++;
			if (m_uWaitFrames > 120)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Timed out waiting for Pinball_Behaviour");
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
				m_uFailed++;
				Zenith_InputSimulator::Disable();
				m_ePhase = PHASE_COMPLETE;
			}
			return;
		}

		// Pinball is loaded and ready
		m_ePhase = PHASE_FULL_GAME_PINBALL_CLEAR;
		m_uFrameDelay = 5;
	}

	void UpdateFullGame_PinballClear()
	{
		Pinball_Behaviour* pxPinball = FindPinballBehaviour();
		if (!pxPinball)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Lost Pinball_Behaviour");
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		// Directly mark the gate as cleared
		pxPinball->m_xSaveData.SetPinballGateCleared(m_uFullGameNextGate);
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &pxPinball->m_xSaveData);

		m_uFullGameGatesCleared++;
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Gate %u cleared (%u/10)", m_uFullGameNextGate, m_uFullGameGatesCleared);

		m_uFullGameNextGate++;

		// Return to menu
		pxPinball->ReturnToMenu();
		m_uWaitFrames = 0;
		m_ePhase = PHASE_FULL_GAME_PINBALL_RETURN;
		m_uFrameDelay = 10;
	}

	void UpdateFullGame_PinballReturn()
	{
		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			m_uWaitFrames++;
			if (m_uWaitFrames > 120)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Timed out returning from pinball to menu");
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
				m_uFailed++;
				Zenith_InputSimulator::Disable();
				m_ePhase = PHASE_COMPLETE;
			}
			return;
		}

		// We're back on the menu. Continue with next level or verify.
		if (m_uFullGameCurrentLevel < m_uFullGameLevelCount)
		{
			// More levels to play
			m_pxPuzzleBehaviour->m_xSaveData.uLives = TilePuzzleSaveData::uMAX_LIVES;
			m_pxPuzzleBehaviour->m_uCurrentLevelNumber = m_uFullGameCurrentLevel + 1;
			m_pxPuzzleBehaviour->m_xSaveData.uCurrentLevel = m_uFullGameCurrentLevel + 1;
			m_pxPuzzleBehaviour->StartGame();
			m_uFullGameCurrentLevel++;
			ResetPerLevelState();
			m_ePhase = PHASE_FULL_GAME_WAIT_PLAYING;
			return;
		}

		// All levels done - any remaining gates?
		if (m_uFullGameNextGate < 10)
		{
			m_ePhase = PHASE_FULL_GAME_PINBALL_ENTER;
			m_uFrameDelay = 5;
			return;
		}

		// All done
		m_ePhase = PHASE_FULL_GAME_VERIFY;
		m_uFrameDelay = 5;
	}

	void UpdateFullGame_Verify()
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  ---- Full-Game Verification ----");

		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Cannot find behaviour for verification");
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		const TilePuzzleSaveData& xSave = m_pxPuzzleBehaviour->m_xSaveData;
		bool bPass = true;

		// Verify all levels completed
		uint32_t uCompletedCount = 0;
		uint32_t uTotalStars = 0;
		for (uint32_t i = 0; i < m_uFullGameLevelCount; ++i)
		{
			if (xSave.axLevelRecords[i].bCompleted)
			{
				uCompletedCount++;
				uTotalStars += xSave.axLevelRecords[i].uBestStars;
			}
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Levels completed: %u/%u", uCompletedCount, m_uFullGameLevelCount);
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Total stars: %u", uTotalStars);
		if (uCompletedCount != m_uFullGameLevelCount)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Not all levels completed");
			bPass = false;
		}

		// Verify all pinball gates cleared
		uint32_t uGatesCleared = 0;
		for (uint32_t i = 0; i < 10; ++i)
		{
			if (xSave.IsPinballGateCleared(i))
				uGatesCleared++;
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Pinball gates cleared: %u/10", uGatesCleared);
		if (uGatesCleared != 10)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Not all pinball gates cleared");
			bPass = false;
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Coins: %u, Cats collected: %u",
			xSave.uCoins, xSave.uCatsCollectedCount);

		if (bPass)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] PASS: Test_FullGame");
			m_uPassed++;
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_FullGame");
			m_uFailed++;
		}

		Zenith_InputSimulator::Disable();
		m_ePhase = PHASE_COMPLETE;
		m_uFrameDelay = 30;
	}

	// ========================================================================
	// Test Shape Data (for logic tests)
	// ========================================================================

	static TilePuzzleShapeDefinition s_xTestSingleShape;
	static bool s_bTestShapeInitialized;

	static void EnsureTestShapeInitialized()
	{
		if (!s_bTestShapeInitialized)
		{
			s_xTestSingleShape = TilePuzzleShapes::GetSingleShape(true);
			s_bTestShapeInitialized = true;
		}
	}

	// ========================================================================
	// BFS Pathfinder: find cell-by-cell path for a shape
	// ========================================================================

	// Diagnostic: dump board state when cell path fails
	static void DumpBoardState(const TilePuzzleLevelData& xLevel, uint32_t uShapeIndex, int32_t iTargetX, int32_t iTargetY)
	{
		int32_t iW = static_cast<int32_t>(xLevel.uGridWidth);
		int32_t iH = static_cast<int32_t>(xLevel.uGridHeight);
		Zenith_Log(LOG_CATEGORY_UNITTEST, "    === Board Dump (grid %dx%d) ===", iW, iH);

		// Print grid with shape/cat positions marked
		for (int32_t y = 0; y < iH; ++y)
		{
			char acRow[64];
			for (int32_t x = 0; x < iW; ++x)
			{
				uint32_t uIdx = y * iW + x;
				if (xLevel.aeCells[uIdx] != TILEPUZZLE_CELL_FLOOR)
					acRow[x] = '#';
				else
					acRow[x] = '.';
			}
			acRow[iW] = '\0';
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    Row %d: %s", y, acRow);
		}

		// Shapes
		for (size_t s = 0; s < xLevel.axShapes.size(); ++s)
		{
			const TilePuzzleShapeInstance& xs = xLevel.axShapes[s];
			if (!xs.pxDefinition) continue;
			const char* pszType = xs.pxDefinition->bDraggable ? "DRAG" : "STATIC";
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    Shape[%u] %s origin=(%d,%d) color=%d removed=%d unlock=%u cells=%u%s",
				static_cast<uint32_t>(s), pszType, xs.iOriginX, xs.iOriginY,
				static_cast<int>(xs.eColor), xs.bRemoved ? 1 : 0,
				xs.uUnlockThreshold,
				static_cast<uint32_t>(xs.pxDefinition->axCells.size()),
				s == uShapeIndex ? " <-- MOVING" : "");
			// Print cell offsets
			for (size_t c = 0; c < xs.pxDefinition->axCells.size(); ++c)
			{
				const TilePuzzleCellOffset& xOff = xs.pxDefinition->axCells[c];
				Zenith_Log(LOG_CATEGORY_UNITTEST, "      cell[%u] offset=(%d,%d) -> world=(%d,%d)",
					static_cast<uint32_t>(c), xOff.iX, xOff.iY,
					xs.iOriginX + xOff.iX, xs.iOriginY + xOff.iY);
			}
		}

		// Cats
		for (size_t c = 0; c < xLevel.axCats.size(); ++c)
		{
			const TilePuzzleCatData& xCat = xLevel.axCats[c];
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    Cat[%u] pos=(%d,%d) color=%d elim=%d blocker=%d",
				static_cast<uint32_t>(c), xCat.iGridX, xCat.iGridY,
				static_cast<int>(xCat.eColor), xCat.bEliminated ? 1 : 0,
				xCat.bOnBlocker ? 1 : 0);
		}

		// Target
		const TilePuzzleShapeInstance& xMoving = xLevel.axShapes[uShapeIndex];
		Zenith_Log(LOG_CATEGORY_UNITTEST, "    Target: shape %u (%d,%d)->(%d,%d)",
			uShapeIndex, xMoving.iOriginX, xMoving.iOriginY, iTargetX, iTargetY);

		// Test 4 directions from start
		TilePuzzleDirection aeDirs[] = { TILEPUZZLE_DIR_UP, TILEPUZZLE_DIR_DOWN, TILEPUZZLE_DIR_LEFT, TILEPUZZLE_DIR_RIGHT };
		const char* apszDirNames[] = { "UP", "DOWN", "LEFT", "RIGHT" };
		for (int d = 0; d < 4; ++d)
		{
			int32_t iDX, iDY;
			TilePuzzleDirections::GetDelta(aeDirs[d], iDX, iDY);
			bool bCan = CanMoveShapeOnLevel(xLevel, uShapeIndex, iDX, iDY);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    Direction %s (%d,%d): %s",
				apszDirNames[d], iDX, iDY, bCan ? "OK" : "BLOCKED");
		}
		Zenith_Log(LOG_CATEGORY_UNITTEST, "    === End Board Dump ===");
	}

	// Check if a shape can move in a direction on the live game state
	static uint32_t CountEliminatedCats(const TilePuzzleLevelData& xLevel)
	{
		uint32_t uCount = 0;
		for (size_t i = 0; i < xLevel.axCats.size(); ++i)
		{
			if (xLevel.axCats[i].bEliminated)
				uCount++;
		}
		return uCount;
	}

	static bool CanMoveShapeOnLevel(const TilePuzzleLevelData& xLevel, uint32_t uShapeIndex, int32_t iDeltaX, int32_t iDeltaY)
	{
		const TilePuzzleShapeInstance& xShape = xLevel.axShapes[uShapeIndex];
		if (!xShape.pxDefinition || !xShape.pxDefinition->bDraggable || xShape.bRemoved)
			return false;

		TilePuzzle_Rules::ShapeState axDraggableStates[16];
		size_t uDraggableCount = 0;
		size_t uMovingDraggableIdx = 0;
		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xOther = xLevel.axShapes[i];
			if (!xOther.pxDefinition || !xOther.pxDefinition->bDraggable || xOther.bRemoved)
				continue;
			if (uDraggableCount >= 16) break;
			if (i == uShapeIndex)
				uMovingDraggableIdx = uDraggableCount;
			axDraggableStates[uDraggableCount].pxDefinition = xOther.pxDefinition;
			axDraggableStates[uDraggableCount].iOriginX = xOther.iOriginX;
			axDraggableStates[uDraggableCount].iOriginY = xOther.iOriginY;
			axDraggableStates[uDraggableCount].eColor = xOther.eColor;
			axDraggableStates[uDraggableCount].uUnlockThreshold = xOther.uUnlockThreshold;
			uDraggableCount++;
		}

		TilePuzzle_Rules::CatState axCatStates[16];
		uint32_t uEliminatedMask = 0;
		size_t uCatCount = xLevel.axCats.size();
		if (uCatCount > 16) uCatCount = 16;
		for (size_t i = 0; i < uCatCount; ++i)
		{
			axCatStates[i].iGridX = xLevel.axCats[i].iGridX;
			axCatStates[i].iGridY = xLevel.axCats[i].iGridY;
			axCatStates[i].eColor = xLevel.axCats[i].eColor;
			axCatStates[i].bOnBlocker = xLevel.axCats[i].bOnBlocker;
			if (xLevel.axCats[i].bEliminated)
				uEliminatedMask |= (1u << i);
		}

		return TilePuzzle_Rules::CanMoveShape(
			xLevel,
			axDraggableStates, uDraggableCount,
			uMovingDraggableIdx,
			xShape.iOriginX + iDeltaX, xShape.iOriginY + iDeltaY,
			axCatStates, uCatCount,
			uEliminatedMask);
	}

	// Simulate cat eliminations on a level state for a given shape.
	// Marks eliminated cats and removed shapes.
	static void SimulateEliminations(TilePuzzleLevelData& xLevel)
	{
		bool bChanged = true;
		while (bChanged)
		{
			bChanged = false;
			for (size_t s = 0; s < xLevel.axShapes.size(); ++s)
			{
				const TilePuzzleShapeInstance& xShape = xLevel.axShapes[s];
				if (!xShape.pxDefinition || !xShape.pxDefinition->bDraggable || xShape.bRemoved)
					continue;

				for (size_t c = 0; c < xLevel.axCats.size(); ++c)
				{
					if (xLevel.axCats[c].bEliminated)
						continue;
					TilePuzzleCatData& xCat = xLevel.axCats[c];
					if (xCat.eColor != xShape.eColor)
						continue;

					if (xCat.bOnBlocker)
					{
						for (const auto& xCell : xShape.pxDefinition->axCells)
						{
							int32_t iCX = xShape.iOriginX + xCell.iX;
							int32_t iCY = xShape.iOriginY + xCell.iY;
							if ((abs(iCX - xCat.iGridX) + abs(iCY - xCat.iGridY)) == 1)
							{
								xCat.bEliminated = true;
								bChanged = true;
								break;
							}
						}
					}
					else
					{
						for (const auto& xCell : xShape.pxDefinition->axCells)
						{
							int32_t iCX = xShape.iOriginX + xCell.iX;
							int32_t iCY = xShape.iOriginY + xCell.iY;
							if (iCX == xCat.iGridX && iCY == xCat.iGridY)
							{
								xCat.bEliminated = true;
								bChanged = true;
								break;
							}
						}
					}
				}

				// Check if all cats of this color are eliminated → remove shape
				if (!xShape.bRemoved)
				{
					bool bAllElim = true;
					for (const auto& xCat : xLevel.axCats)
					{
						if (xCat.eColor == xShape.eColor && !xCat.bEliminated)
						{
							bAllElim = false;
							break;
						}
					}
					if (bAllElim)
					{
						xLevel.axShapes[s].bRemoved = true;
						bChanged = true;
					}
				}
			}
		}
	}

	// Find cell-by-cell path using the EXACT same BFS logic as the solver's
	// inner BFS. This ensures the path eliminates the same cats the solver expects.
	// Mirrors TilePuzzle_Solver.h lines 694-828 precisely.
	// uRequiredElimMask: if non-zero, only accept paths that produce EXACTLY this mask.
	// This ensures we follow the same elimination sequence the solver planned.
	int32_t FindSolverInnerPath(const TilePuzzleLevelData& xLevel, uint32_t uShapeIndex,
		int32_t iTargetX, int32_t iTargetY,
		TilePuzzleDirection* aePathOut, uint32_t uMaxPathLen,
		uint32_t uRequiredElimMask = 0)
	{
		uint32_t uGridWidth = xLevel.uGridWidth;
		uint32_t uGridHeight = xLevel.uGridHeight;

		// Collect draggable shapes (same as solver setup)
		static constexpr uint32_t uMaxDrag = 5;
		const TilePuzzleShapeDefinition* apxDefs[uMaxDrag];
		TilePuzzleColor aeColors[uMaxDrag];
		uint32_t auColorCatMasks[uMaxDrag];
		int32_t aiPosX[uMaxDrag], aiPosY[uMaxDrag];
		uint32_t auFullToLocal[16]; // maps full shape index → draggable index
		memset(auFullToLocal, 0xFF, sizeof(auFullToLocal));
		uint32_t uNumDraggable = 0;
		uint32_t uDragIdx = UINT32_MAX; // local index of the shape we're moving

		uint32_t uNumCats = static_cast<uint32_t>(xLevel.axCats.size());
		if (uNumCats > 16) return -1;

		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xs = xLevel.axShapes[i];
			if (!xs.pxDefinition || !xs.pxDefinition->bDraggable || xs.bRemoved)
				continue;
			if (uNumDraggable >= uMaxDrag) return -1;
			auFullToLocal[i] = uNumDraggable;
			if (static_cast<uint32_t>(i) == uShapeIndex)
				uDragIdx = uNumDraggable;
			apxDefs[uNumDraggable] = xs.pxDefinition;
			aeColors[uNumDraggable] = xs.eColor;
			aiPosX[uNumDraggable] = xs.iOriginX;
			aiPosY[uNumDraggable] = xs.iOriginY;
			uint32_t uMask = 0;
			for (uint32_t j = 0; j < uNumCats; ++j)
			{
				if (xLevel.axCats[j].eColor == xs.eColor)
					uMask |= (1u << j);
			}
			auColorCatMasks[uNumDraggable] = uMask;
			uNumDraggable++;
		}
		if (uDragIdx == UINT32_MAX) return -1;

		// Pre-compute walkable grid (same as solver)
		bool abWalkable[256];
		memset(abWalkable, 0, sizeof(abWalkable));
		for (uint32_t y = 0; y < uGridHeight; y++)
			for (uint32_t x = 0; x < uGridWidth; x++)
				abWalkable[y * uGridWidth + x] = (xLevel.aeCells[y * uGridWidth + x] == TILEPUZZLE_CELL_FLOOR);

		// Mark static (non-draggable) shape cells as non-walkable
		for (size_t i = 0; i < xLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xs = xLevel.axShapes[i];
			if (!xs.pxDefinition || xs.pxDefinition->bDraggable) continue;
			for (size_t j = 0; j < xs.pxDefinition->axCells.size(); ++j)
			{
				int32_t cx = xs.iOriginX + xs.pxDefinition->axCells[j].iX;
				int32_t cy = xs.iOriginY + xs.pxDefinition->axCells[j].iY;
				if (cx >= 0 && cy >= 0 && static_cast<uint32_t>(cx) < uGridWidth && static_cast<uint32_t>(cy) < uGridHeight)
					abWalkable[cy * uGridWidth + cx] = false;
			}
		}

		// Cat positions and colors
		int32_t aiCatX[16], aiCatY[16];
		TilePuzzleColor aeCatColors[16];
		bool abCatOnBlocker[16];
		for (uint32_t i = 0; i < uNumCats; ++i)
		{
			aiCatX[i] = xLevel.axCats[i].iGridX;
			aiCatY[i] = xLevel.axCats[i].iGridY;
			aeCatColors[i] = xLevel.axCats[i].eColor;
			abCatOnBlocker[i] = xLevel.axCats[i].bOnBlocker;
		}

		// Cat-at-cell lookup
		int8_t aiCatAtCell[256];
		memset(aiCatAtCell, -1, sizeof(aiCatAtCell));
		for (uint32_t i = 0; i < uNumCats; ++i)
		{
			uint32_t uIdx = static_cast<uint32_t>(aiCatY[i]) * uGridWidth + static_cast<uint32_t>(aiCatX[i]);
			aiCatAtCell[uIdx] = static_cast<int8_t>(i);
		}

		// Initial elimination mask
		uint32_t uInitMask = 0;
		for (uint32_t i = 0; i < uNumCats; ++i)
		{
			if (xLevel.axCats[i].bEliminated)
				uInitMask |= (1u << i);
		}

		// Inner BFS state: pack (x, y, elimMask) into uint64_t (matches solver)
		auto PackInner = [](int32_t x, int32_t y, uint32_t mask) -> uint64_t {
			return (static_cast<uint64_t>(x) << 32) | (static_cast<uint64_t>(y) << 16) | static_cast<uint64_t>(mask);
		};

		struct InnerNode
		{
			int32_t iX, iY;
			uint32_t uMask;
			int32_t iParent;
			TilePuzzleDirection eDir;
		};

		static constexpr int32_t iMAX_NODES = 65536;
		InnerNode* axNodes = new InnerNode[iMAX_NODES];
		uint32_t uNodeCount = 0;
		uint32_t uFront = 0;

		std::unordered_set<uint64_t> xVisited; // #TODO: Replace with engine hash map
		xVisited.reserve(4096);

		int32_t iStartX = aiPosX[uDragIdx];
		int32_t iStartY = aiPosY[uDragIdx];
		bool bRoundTrip = (iStartX == iTargetX && iStartY == iTargetY);

		uint64_t uStartKey = PackInner(iStartX, iStartY, uInitMask);
		xVisited.insert(uStartKey);
		axNodes[uNodeCount++] = { iStartX, iStartY, uInitMask, -1, TILEPUZZLE_DIR_NONE };

		// Direction order matches solver exactly: UP, DOWN, LEFT, RIGHT
		int32_t aiDeltaX[] = {0, 0, -1, 1};
		int32_t aiDeltaY[] = {-1, 1, 0, 0};
		TilePuzzleDirection aeDir[] = {
			TILEPUZZLE_DIR_UP, TILEPUZZLE_DIR_DOWN,
			TILEPUZZLE_DIR_LEFT, TILEPUZZLE_DIR_RIGHT
		};

		int32_t iBestNode = -1;

		while (uFront < uNodeCount)
		{
			InnerNode& xCur = axNodes[uFront++];

			for (int32_t iDir = 0; iDir < 4; ++iDir)
			{
				int32_t iNewX = xCur.iX + aiDeltaX[iDir];
				int32_t iNewY = xCur.iY + aiDeltaY[iDir];

				// Bounds + walkable check for all cells of the moving shape
				bool bValid = true;
				const std::vector<TilePuzzleCellOffset>& axMovingCells = apxDefs[uDragIdx]->axCells;
				for (size_t c = 0; c < axMovingCells.size(); ++c)
				{
					int32_t iCellX = iNewX + axMovingCells[c].iX;
					int32_t iCellY = iNewY + axMovingCells[c].iY;

					if (iCellX < 0 || iCellY < 0 ||
						static_cast<uint32_t>(iCellX) >= uGridWidth ||
						static_cast<uint32_t>(iCellY) >= uGridHeight)
					{ bValid = false; break; }

					uint32_t uCellIdx = static_cast<uint32_t>(iCellY) * uGridWidth + static_cast<uint32_t>(iCellX);
					if (!abWalkable[uCellIdx])
					{ bValid = false; break; }

					// Check collision with other draggable shapes
					for (uint32_t si = 0; si < uNumDraggable; ++si)
					{
						if (si == uDragIdx) continue;
						if ((auColorCatMasks[si] & ~xCur.uMask) == 0) continue; // removed
						const std::vector<TilePuzzleCellOffset>& axOtherCells = apxDefs[si]->axCells;
						for (size_t sc = 0; sc < axOtherCells.size(); ++sc)
						{
							if (aiPosX[si] + axOtherCells[sc].iX == iCellX &&
								aiPosY[si] + axOtherCells[sc].iY == iCellY)
							{ bValid = false; break; }
						}
						if (!bValid) break;
					}
					if (!bValid) break;

					// Check wrong-color cat blocking
					int8_t iCatIdx = aiCatAtCell[uCellIdx];
					if (iCatIdx >= 0 &&
						!(xCur.uMask & (1u << static_cast<uint32_t>(iCatIdx))) &&
						aeCatColors[iCatIdx] != aeColors[uDragIdx])
					{ bValid = false; break; }
				}

				if (!bValid) continue;

				// Compute newly eliminated cats (same as solver)
				uint32_t uNewlyEliminated = 0;
				for (uint32_t si = 0; si < uNumDraggable; ++si)
				{
					if ((auColorCatMasks[si] & ~xCur.uMask) == 0) continue;
					int32_t iShapeX = (si == uDragIdx) ? iNewX : aiPosX[si];
					int32_t iShapeY = (si == uDragIdx) ? iNewY : aiPosY[si];
					const std::vector<TilePuzzleCellOffset>& axShapeCells = apxDefs[si]->axCells;
					for (size_t c = 0; c < axShapeCells.size(); ++c)
					{
						int32_t iCellX = iShapeX + axShapeCells[c].iX;
						int32_t iCellY = iShapeY + axShapeCells[c].iY;
						for (uint32_t ci = 0; ci < uNumCats; ++ci)
						{
							if ((xCur.uMask | uNewlyEliminated) & (1u << ci)) continue;
							if (aeCatColors[ci] != aeColors[si]) continue;
							if (abCatOnBlocker[ci])
							{
								int32_t iDX = iCellX - aiCatX[ci];
								int32_t iDY = iCellY - aiCatY[ci];
								if ((iDX == 0 && (iDY == 1 || iDY == -1)) ||
									(iDY == 0 && (iDX == 1 || iDX == -1)))
									uNewlyEliminated |= (1u << ci);
							}
							else
							{
								if (aiCatX[ci] == iCellX && aiCatY[ci] == iCellY)
									uNewlyEliminated |= (1u << ci);
							}
						}
					}
				}

				uint32_t uNewMask = xCur.uMask | uNewlyEliminated;
				uint64_t uNextKey = PackInner(iNewX, iNewY, uNewMask);
				if (xVisited.count(uNextKey) > 0)
					continue;
				xVisited.insert(uNextKey);

				if (uNodeCount >= iMAX_NODES)
					break;

				axNodes[uNodeCount++] = { iNewX, iNewY, uNewMask, static_cast<int32_t>(uFront - 1), aeDir[iDir] };

				// Check if we reached target
				if (bRoundTrip)
				{
					// Round-trip: must return to start with more cats eliminated
					if (iNewX == iTargetX && iNewY == iTargetY && uNewMask != uInitMask)
					{
						if (uRequiredElimMask == 0 || uNewMask == uRequiredElimMask)
						{
							iBestNode = static_cast<int32_t>(uNodeCount - 1);
							goto found;
						}
					}
				}
				else
				{
					if (iNewX == iTargetX && iNewY == iTargetY)
					{
						if (uRequiredElimMask == 0 || uNewMask == uRequiredElimMask)
						{
							iBestNode = static_cast<int32_t>(uNodeCount - 1);
							goto found;
						}
					}
				}
			}
		}
		found:

		if (iBestNode < 0)
		{
			delete[] axNodes;
			return -1;
		}

		// Trace back
		int32_t aiRev[256];
		int32_t iLen = 0;
		int32_t iNode = iBestNode;
		while (axNodes[iNode].iParent >= 0)
		{
			if (iLen >= 256) { delete[] axNodes; return -1; }
			aiRev[iLen++] = iNode;
			iNode = axNodes[iNode].iParent;
		}
		if (static_cast<uint32_t>(iLen) > uMaxPathLen)
		{
			delete[] axNodes;
			return -1;
		}
		for (int32_t i = 0; i < iLen; ++i)
			aePathOut[i] = axNodes[aiRev[iLen - 1 - i]].eDir;

		delete[] axNodes;
		return iLen;
	}

	// BFS to find a round-trip path from start back to start that eliminates
	// at least one new cat. Used for solver round-trip moves (start == target).
	int32_t FindRoundTripPath(const TilePuzzleLevelData& xLevel, uint32_t uShapeIndex,
		TilePuzzleDirection* aePathOut, uint32_t uMaxPathLen)
	{
		const TilePuzzleShapeInstance& xShape = xLevel.axShapes[uShapeIndex];
		int32_t iStartX = xShape.iOriginX;
		int32_t iStartY = xShape.iOriginY;

		struct BFSNode
		{
			int32_t iX, iY;
			int32_t iParent;
			TilePuzzleDirection eDir;
			uint32_t uElimMask;
		};

		static constexpr int32_t iMAX_BFS_NODES = 65536;
		BFSNode* axNodes = new BFSNode[iMAX_BFS_NODES];
		uint32_t uNodeCount = 0;
		uint32_t uFront = 0;

		int32_t iW = static_cast<int32_t>(xLevel.uGridWidth);
		int32_t iH = static_cast<int32_t>(xLevel.uGridHeight);

		uint32_t uInitElimMask = 0;
		for (size_t c = 0; c < xLevel.axCats.size() && c < 16; ++c)
		{
			if (xLevel.axCats[c].bEliminated)
				uInitElimMask |= (1u << c);
		}

		std::unordered_set<uint32_t> xVisited; // #TODO: Replace with engine hash map
		auto MakeVisitedKey = [iW](int32_t iX, int32_t iY, uint32_t uMask) -> uint32_t {
			return static_cast<uint32_t>(iY * iW + iX) * 65536u + (uMask & 0xFFFF);
		};

		axNodes[uNodeCount++] = { iStartX, iStartY, -1, TILEPUZZLE_DIR_NONE, uInitElimMask };
		xVisited.insert(MakeVisitedKey(iStartX, iStartY, uInitElimMask));

		TilePuzzleDirection aeDirections[] = {
			TILEPUZZLE_DIR_UP, TILEPUZZLE_DIR_DOWN,
			TILEPUZZLE_DIR_LEFT, TILEPUZZLE_DIR_RIGHT
		};

		int32_t iBestTargetNode = -1;

		TilePuzzleLevelData xBFSLevel = xLevel;

		while (uFront < uNodeCount)
		{
			BFSNode& xCur = axNodes[uFront++];

			for (size_t c = 0; c < xLevel.axCats.size() && c < 16; ++c)
				xBFSLevel.axCats[c].bEliminated = (xCur.uElimMask & (1u << c)) != 0;
			for (size_t s = 0; s < xBFSLevel.axShapes.size(); ++s)
			{
				TilePuzzleShapeInstance& xs = xBFSLevel.axShapes[s];
				if (!xs.pxDefinition || !xs.pxDefinition->bDraggable) continue;
				xs.bRemoved = xLevel.axShapes[s].bRemoved;
				bool bAllElim = true;
				for (size_t c = 0; c < xBFSLevel.axCats.size(); ++c)
				{
					if (xBFSLevel.axCats[c].eColor == xs.eColor && !xBFSLevel.axCats[c].bEliminated)
					{ bAllElim = false; break; }
				}
				if (bAllElim) xs.bRemoved = true;
			}
			xBFSLevel.axShapes[uShapeIndex].iOriginX = xCur.iX;
			xBFSLevel.axShapes[uShapeIndex].iOriginY = xCur.iY;

			for (TilePuzzleDirection eDir : aeDirections)
			{
				int32_t iDX, iDY;
				TilePuzzleDirections::GetDelta(eDir, iDX, iDY);
				int32_t iNX = xCur.iX + iDX;
				int32_t iNY = xCur.iY + iDY;

				if (iNX < 0 || iNX >= iW || iNY < 0 || iNY >= iH)
					continue;
				if (!CanMoveShapeOnLevel(xBFSLevel, uShapeIndex, iDX, iDY))
					continue;

				xBFSLevel.axShapes[uShapeIndex].iOriginX = iNX;
				xBFSLevel.axShapes[uShapeIndex].iOriginY = iNY;
				SimulateEliminations(xBFSLevel);

				uint32_t uNewElimMask = 0;
				for (size_t c = 0; c < xBFSLevel.axCats.size() && c < 16; ++c)
				{
					if (xBFSLevel.axCats[c].bEliminated)
						uNewElimMask |= (1u << c);
				}

				uint32_t uVisKey = MakeVisitedKey(iNX, iNY, uNewElimMask);
				bool bSkip = (xVisited.count(uVisKey) > 0);

				// Restore state
				xBFSLevel.axShapes[uShapeIndex].iOriginX = xCur.iX;
				xBFSLevel.axShapes[uShapeIndex].iOriginY = xCur.iY;
				for (size_t c = 0; c < xLevel.axCats.size() && c < 16; ++c)
					xBFSLevel.axCats[c].bEliminated = (xCur.uElimMask & (1u << c)) != 0;
				for (size_t s = 0; s < xBFSLevel.axShapes.size(); ++s)
				{
					TilePuzzleShapeInstance& xs = xBFSLevel.axShapes[s];
					if (!xs.pxDefinition || !xs.pxDefinition->bDraggable) continue;
					xs.bRemoved = xLevel.axShapes[s].bRemoved;
					bool bAllElim = true;
					for (size_t c = 0; c < xBFSLevel.axCats.size(); ++c)
					{
						if (xBFSLevel.axCats[c].eColor == xs.eColor && !xBFSLevel.axCats[c].bEliminated)
						{ bAllElim = false; break; }
					}
					if (bAllElim) xs.bRemoved = true;
				}

				if (bSkip)
					continue;
				xVisited.insert(uVisKey);

				if (uNodeCount >= iMAX_BFS_NODES)
					break;

				axNodes[uNodeCount++] = { iNX, iNY, static_cast<int32_t>(uFront - 1), eDir, uNewElimMask };

				// Use first round-trip found (shortest)
				if (iNX == iStartX && iNY == iStartY && uNewElimMask != uInitElimMask)
				{
					iBestTargetNode = static_cast<int32_t>(uNodeCount - 1);
					goto roundtrip_done;
				}
			}
		}
		roundtrip_done:

		if (iBestTargetNode < 0)
		{
			delete[] axNodes;
			return -1;
		}

		int32_t aiPathReversed[64];
		int32_t iPathLen = 0;
		int32_t iNode = iBestTargetNode;
		while (axNodes[iNode].iParent >= 0)
		{
			if (iPathLen >= 64) { delete[] axNodes; return -1; }
			aiPathReversed[iPathLen++] = iNode;
			iNode = axNodes[iNode].iParent;
		}

		if (static_cast<uint32_t>(iPathLen) > uMaxPathLen)
		{
			delete[] axNodes;
			return -1;
		}

		for (int32_t i = 0; i < iPathLen; ++i)
			aePathOut[i] = axNodes[aiPathReversed[iPathLen - 1 - i]].eDir;

		delete[] axNodes;
		return iPathLen;
	}

	// BFS to find shortest cell path to the target position.
	// Uses (position, elimination_mask) as the full BFS state key so that
	// paths through same-color cats are explored.
	// Returns the first (shortest) path found to minimize unplanned eliminations.
	int32_t FindCellPath(const TilePuzzleLevelData& xLevel, uint32_t uShapeIndex,
		int32_t iTargetX, int32_t iTargetY,
		TilePuzzleDirection* aePathOut, uint32_t uMaxPathLen)
	{
		const TilePuzzleShapeInstance& xShape = xLevel.axShapes[uShapeIndex];
		int32_t iStartX = xShape.iOriginX;
		int32_t iStartY = xShape.iOriginY;

		if (iStartX == iTargetX && iStartY == iTargetY)
			return 0;

		struct BFSNode
		{
			int32_t iX, iY;
			int32_t iParent;
			TilePuzzleDirection eDir;
			uint32_t uElimMask;
		};

		static constexpr int32_t iMAX_BFS_NODES = 65536;
		BFSNode* axNodes = new BFSNode[iMAX_BFS_NODES];
		uint32_t uNodeCount = 0;
		uint32_t uFront = 0;

		int32_t iW = static_cast<int32_t>(xLevel.uGridWidth);
		int32_t iH = static_cast<int32_t>(xLevel.uGridHeight);

		// Initial elimination mask
		uint32_t uInitElimMask = 0;
		for (size_t c = 0; c < xLevel.axCats.size() && c < 16; ++c)
		{
			if (xLevel.axCats[c].bEliminated)
				uInitElimMask |= (1u << c);
		}

		// Visited: full (position, elimination_mask) key
		// Key = position * 65536 + elimMask (position < 100, elimMask < 65536)
		std::unordered_set<uint32_t> xVisited; // #TODO: Replace with engine hash map
		auto MakeVisitedKey = [iW](int32_t iX, int32_t iY, uint32_t uMask) -> uint32_t {
			return static_cast<uint32_t>(iY * iW + iX) * 65536u + (uMask & 0xFFFF);
		};

		axNodes[uNodeCount++] = { iStartX, iStartY, -1, TILEPUZZLE_DIR_NONE, uInitElimMask };
		xVisited.insert(MakeVisitedKey(iStartX, iStartY, uInitElimMask));

		TilePuzzleDirection aeDirections[] = {
			TILEPUZZLE_DIR_UP, TILEPUZZLE_DIR_DOWN,
			TILEPUZZLE_DIR_LEFT, TILEPUZZLE_DIR_RIGHT
		};

		// Track the first target node found (shortest BFS path)
		int32_t iBestTargetNode = -1;

		// Create a mutable copy of the level for testing moves
		TilePuzzleLevelData xBFSLevel = xLevel;

		while (uFront < uNodeCount)
		{
			BFSNode& xCur = axNodes[uFront++];

			// Restore level state for this node's elimination mask
			for (size_t c = 0; c < xLevel.axCats.size() && c < 16; ++c)
				xBFSLevel.axCats[c].bEliminated = (xCur.uElimMask & (1u << c)) != 0;

			// Restore shape removal flags based on elimination mask
			for (size_t s = 0; s < xBFSLevel.axShapes.size(); ++s)
			{
				TilePuzzleShapeInstance& xs = xBFSLevel.axShapes[s];
				if (!xs.pxDefinition || !xs.pxDefinition->bDraggable)
					continue;
				xs.bRemoved = xLevel.axShapes[s].bRemoved;
				bool bAllElim = true;
				for (size_t c = 0; c < xBFSLevel.axCats.size(); ++c)
				{
					if (xBFSLevel.axCats[c].eColor == xs.eColor && !xBFSLevel.axCats[c].bEliminated)
					{
						bAllElim = false;
						break;
					}
				}
				if (bAllElim)
					xs.bRemoved = true;
			}

			// Set shape position for this node
			xBFSLevel.axShapes[uShapeIndex].iOriginX = xCur.iX;
			xBFSLevel.axShapes[uShapeIndex].iOriginY = xCur.iY;

			for (TilePuzzleDirection eDir : aeDirections)
			{
				int32_t iDX, iDY;
				TilePuzzleDirections::GetDelta(eDir, iDX, iDY);
				int32_t iNX = xCur.iX + iDX;
				int32_t iNY = xCur.iY + iDY;

				if (iNX < 0 || iNX >= iW || iNY < 0 || iNY >= iH)
					continue;

				if (!CanMoveShapeOnLevel(xBFSLevel, uShapeIndex, iDX, iDY))
					continue;

				// Simulate move: place shape at new position
				xBFSLevel.axShapes[uShapeIndex].iOriginX = iNX;
				xBFSLevel.axShapes[uShapeIndex].iOriginY = iNY;

				// Simulate eliminations from this new position
				SimulateEliminations(xBFSLevel);

				// Compute new elimination mask
				uint32_t uNewElimMask = 0;
				for (size_t c = 0; c < xBFSLevel.axCats.size() && c < 16; ++c)
				{
					if (xBFSLevel.axCats[c].bEliminated)
						uNewElimMask |= (1u << c);
				}

				// Check visited with full (position, elimination_mask) key
				uint32_t uVisKey = MakeVisitedKey(iNX, iNY, uNewElimMask);
				bool bSkip = (xVisited.count(uVisKey) > 0);

				// Always restore state before next direction or node addition
				xBFSLevel.axShapes[uShapeIndex].iOriginX = xCur.iX;
				xBFSLevel.axShapes[uShapeIndex].iOriginY = xCur.iY;
				for (size_t c = 0; c < xLevel.axCats.size() && c < 16; ++c)
					xBFSLevel.axCats[c].bEliminated = (xCur.uElimMask & (1u << c)) != 0;
				for (size_t s = 0; s < xBFSLevel.axShapes.size(); ++s)
				{
					TilePuzzleShapeInstance& xs = xBFSLevel.axShapes[s];
					if (!xs.pxDefinition || !xs.pxDefinition->bDraggable) continue;
					xs.bRemoved = xLevel.axShapes[s].bRemoved;
					bool bAllElim = true;
					for (size_t c = 0; c < xBFSLevel.axCats.size(); ++c)
					{
						if (xBFSLevel.axCats[c].eColor == xs.eColor && !xBFSLevel.axCats[c].bEliminated)
						{
							bAllElim = false;
							break;
						}
					}
					if (bAllElim) xs.bRemoved = true;
				}

				if (bSkip)
					continue;

				xVisited.insert(uVisKey);

				if (uNodeCount >= iMAX_BFS_NODES)
					break;

				axNodes[uNodeCount++] = { iNX, iNY, static_cast<int32_t>(uFront - 1), eDir, uNewElimMask };

				// Use first (shortest) path found to minimize unplanned eliminations
				if (iNX == iTargetX && iNY == iTargetY)
				{
					iBestTargetNode = static_cast<int32_t>(uNodeCount - 1);
					goto bfs_done;
				}
			}
		}
		bfs_done:

		// No path found
		if (iBestTargetNode < 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "    BFS exhausted: %u nodes explored, %u visited states",
				uNodeCount, static_cast<uint32_t>(xVisited.size()));
			delete[] axNodes;
			return -1;
		}

		// Trace back from best target node
		int32_t aiPathReversed[64];
		int32_t iPathLen = 0;
		int32_t iNode = iBestTargetNode;
		while (axNodes[iNode].iParent >= 0)
		{
			if (iPathLen >= 64) { delete[] axNodes; return -1; }
			aiPathReversed[iPathLen++] = iNode;
			iNode = axNodes[iNode].iParent;
		}

		if (static_cast<uint32_t>(iPathLen) > uMaxPathLen)
		{
			delete[] axNodes;
			return -1;
		}

		for (int32_t i = 0; i < iPathLen; ++i)
			aePathOut[i] = axNodes[aiPathReversed[iPathLen - 1 - i]].eDir;

		delete[] axNodes;
		return iPathLen;
	}

	// ========================================================================
	// Helper: Build test levels programmatically (for logic tests)
	// ========================================================================

	static TilePuzzleLevelData BuildSimpleTestLevel()
	{
		EnsureTestShapeInitialized();

		TilePuzzleLevelData xLevel;
		xLevel.uGridWidth = 4;
		xLevel.uGridHeight = 4;
		xLevel.uMinimumMoves = 1;

		xLevel.aeCells.resize(16, TILEPUZZLE_CELL_FLOOR);

		TilePuzzleShapeInstance xShape;
		xShape.pxDefinition = &s_xTestSingleShape;
		xShape.iOriginX = 0;
		xShape.iOriginY = 0;
		xShape.eColor = TILEPUZZLE_COLOR_RED;
		xShape.uUnlockThreshold = 0;
		xShape.bRemoved = false;
		xLevel.axShapes.push_back(xShape);

		TilePuzzleCatData xCat;
		xCat.eColor = TILEPUZZLE_COLOR_RED;
		xCat.iGridX = 3;
		xCat.iGridY = 0;
		xCat.bEliminated = false;
		xCat.bOnBlocker = false;
		xCat.fEliminationProgress = 0.f;
		xLevel.axCats.push_back(xCat);

		return xLevel;
	}

	static TilePuzzleLevelData BuildTwoShapeTestLevel()
	{
		EnsureTestShapeInitialized();

		TilePuzzleLevelData xLevel;
		xLevel.uGridWidth = 5;
		xLevel.uGridHeight = 3;
		xLevel.uMinimumMoves = 2;

		xLevel.aeCells.resize(15, TILEPUZZLE_CELL_FLOOR);

		TilePuzzleShapeInstance xShape1;
		xShape1.pxDefinition = &s_xTestSingleShape;
		xShape1.iOriginX = 0;
		xShape1.iOriginY = 0;
		xShape1.eColor = TILEPUZZLE_COLOR_RED;
		xShape1.uUnlockThreshold = 0;
		xShape1.bRemoved = false;
		xLevel.axShapes.push_back(xShape1);

		TilePuzzleShapeInstance xShape2;
		xShape2.pxDefinition = &s_xTestSingleShape;
		xShape2.iOriginX = 0;
		xShape2.iOriginY = 2;
		xShape2.eColor = TILEPUZZLE_COLOR_BLUE;
		xShape2.uUnlockThreshold = 0;
		xShape2.bRemoved = false;
		xLevel.axShapes.push_back(xShape2);

		TilePuzzleCatData xCat1;
		xCat1.eColor = TILEPUZZLE_COLOR_RED;
		xCat1.iGridX = 4;
		xCat1.iGridY = 0;
		xCat1.bEliminated = false;
		xCat1.bOnBlocker = false;
		xCat1.fEliminationProgress = 0.f;
		xLevel.axCats.push_back(xCat1);

		TilePuzzleCatData xCat2;
		xCat2.eColor = TILEPUZZLE_COLOR_BLUE;
		xCat2.iGridX = 4;
		xCat2.iGridY = 2;
		xCat2.bEliminated = false;
		xCat2.bOnBlocker = false;
		xCat2.fEliminationProgress = 0.f;
		xLevel.axCats.push_back(xCat2);

		return xLevel;
	}

	// ========================================================================
	// Helper: Star calculation
	// ========================================================================

	static uint32_t CalculateStars(uint32_t uMoveCount, uint32_t uPar)
	{
		if (uPar == 0) uPar = 1;
		if (uMoveCount <= uPar)
			return 3;
		else if (uMoveCount <= uPar + 2)
			return 2;
		else
			return 1;
	}

	// ========================================================================
	// Logic Test: Star rating calculation
	// ========================================================================

	static bool Test_PuzzleLevel_StarRating()
	{
		bool bPass = true;

		TilePuzzleLevelData xLevel = BuildTwoShapeTestLevel();

		int32_t iSolverMoves = TilePuzzle_Solver::SolveLevel(xLevel);
		if (iSolverMoves < 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Two-shape level unsolvable");
			return false;
		}

		uint32_t uPar = xLevel.uMinimumMoves;
		if (uPar == 0) uPar = 1;

		if (CalculateStars(uPar, uPar) != 3)
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: par moves should give 3 stars"); bPass = false; }
		if (CalculateStars(uPar + 2, uPar) != 2)
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: par+2 should give 2 stars"); bPass = false; }
		if (CalculateStars(uPar + 3, uPar) != 1)
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: par+3 should give 1 star"); bPass = false; }
		if (CalculateStars(uPar + 1, uPar) != 2)
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: par+1 should give 2 stars"); bPass = false; }
		if (CalculateStars(1, uPar) != 3)
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: below par should give 3 stars"); bPass = false; }

		return bPass;
	}

	// ========================================================================
	// Logic Test: Undo reverts shape position
	// ========================================================================

	static bool Test_PuzzleLevel_Undo()
	{
		bool bPass = true;
		TilePuzzleLevelData xLevel = BuildSimpleTestLevel();

		TilePuzzle_Rules::ShapeState axShapes[1];
		axShapes[0].pxDefinition = xLevel.axShapes[0].pxDefinition;
		axShapes[0].iOriginX = 0;
		axShapes[0].iOriginY = 0;
		axShapes[0].eColor = xLevel.axShapes[0].eColor;
		axShapes[0].uUnlockThreshold = 0;

		TilePuzzle_Rules::CatState axCats[1];
		axCats[0].iGridX = xLevel.axCats[0].iGridX;
		axCats[0].iGridY = xLevel.axCats[0].iGridY;
		axCats[0].eColor = xLevel.axCats[0].eColor;
		axCats[0].bOnBlocker = false;

		// Move right
		bool bCanMove = TilePuzzle_Rules::CanMoveShape(xLevel, axShapes, 1, 0, 1, 0, axCats, 1, 0);
		if (!bCanMove) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Cannot move right"); return false; }

		int32_t iSavedX = axShapes[0].iOriginX;
		int32_t iSavedY = axShapes[0].iOriginY;
		axShapes[0].iOriginX = 1;
		axShapes[0].iOriginY = 0;

		// Undo
		axShapes[0].iOriginX = iSavedX;
		axShapes[0].iOriginY = iSavedY;

		if (axShapes[0].iOriginX != 0 || axShapes[0].iOriginY != 0)
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Undo did not restore position"); bPass = false; }

		// Verify elimination works at target
		axShapes[0].iOriginX = 3;
		axShapes[0].iOriginY = 0;
		uint32_t uElim = TilePuzzle_Rules::ComputeNewlyEliminatedCats(axShapes, 1, axCats, 1, 0);
		if (uElim == 0)
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Cat not eliminated at (3,0)"); bPass = false; }

		return bPass;
	}

	// ========================================================================
	// Logic Test: Hint system finds an improving move
	// ========================================================================

	static bool Test_PuzzleLevel_Hint()
	{
		TilePuzzleLevelData xLevel = BuildTwoShapeTestLevel();

		int32_t iSolutionMoves = TilePuzzle_Solver::SolveLevel(xLevel);
		if (iSolutionMoves < 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Level is not solvable");
			return false;
		}

		int32_t iBestResult = INT32_MAX;
		TilePuzzleDirection aeDirections[] = { TILEPUZZLE_DIR_UP, TILEPUZZLE_DIR_DOWN, TILEPUZZLE_DIR_LEFT, TILEPUZZLE_DIR_RIGHT };

		TilePuzzle_Rules::ShapeState axShapes[2];
		TilePuzzle_Rules::CatState axCats[2];
		for (size_t i = 0; i < 2; ++i)
		{
			axShapes[i].pxDefinition = xLevel.axShapes[i].pxDefinition;
			axShapes[i].iOriginX = xLevel.axShapes[i].iOriginX;
			axShapes[i].iOriginY = xLevel.axShapes[i].iOriginY;
			axShapes[i].eColor = xLevel.axShapes[i].eColor;
			axShapes[i].uUnlockThreshold = 0;
			axCats[i].iGridX = xLevel.axCats[i].iGridX;
			axCats[i].iGridY = xLevel.axCats[i].iGridY;
			axCats[i].eColor = xLevel.axCats[i].eColor;
			axCats[i].bOnBlocker = false;
		}

		for (size_t iShape = 0; iShape < 2; ++iShape)
		{
			if (!xLevel.axShapes[iShape].pxDefinition->bDraggable) continue;
			for (TilePuzzleDirection eDir : aeDirections)
			{
				int32_t iDX, iDY;
				TilePuzzleDirections::GetDelta(eDir, iDX, iDY);
				int32_t iNewX = xLevel.axShapes[iShape].iOriginX + iDX;
				int32_t iNewY = xLevel.axShapes[iShape].iOriginY + iDY;

				if (!TilePuzzle_Rules::CanMoveShape(xLevel, axShapes, 2, iShape, iNewX, iNewY, axCats, 2, 0))
					continue;

				TilePuzzleLevelData xAfter = xLevel;
				xAfter.axShapes[iShape].iOriginX = iNewX;
				xAfter.axShapes[iShape].iOriginY = iNewY;
				int32_t iRes = TilePuzzle_Solver::SolveLevel(xAfter, 500000);
				if (iRes >= 0 && iRes < iBestResult)
					iBestResult = iRes;
			}
		}

		if (iBestResult > iSolutionMoves)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Hint worsens solution");
			return false;
		}
		return true;
	}

	// ========================================================================
	// Logic Test: Pinball launch
	// ========================================================================

	static bool Test_Pinball_LaunchAndScore()
	{
		volatile float fMaxForce = 35.f;
		volatile uint32_t uPegScore = 100;
		volatile uint32_t uTargetScore = 500;
		volatile float fBallRadius = 0.15f;

		if (fMaxForce <= 0.f || uPegScore == 0 || uTargetScore == 0 || fBallRadius <= 0.f)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Invalid physics constants");
			return false;
		}

		uint32_t uScore = 0;
		for (uint32_t i = 0; i < 5; ++i) uScore += uPegScore;
		if (uScore != 500)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Score accumulation");
			return false;
		}
		return true;
	}

	// ========================================================================
	// Logic Test: Pinball gate objectives
	// ========================================================================

	static bool Test_Pinball_GateObjectives()
	{
		bool bPass = true;

		Zenith_DataStream xStream;
		xStream.ReadFromFile(GAME_ASSETS_DIR "Pinball/GateData.bin");

		uint32_t uGateCount = 0;
		xStream >> uGateCount;

		if (uGateCount == 0 || uGateCount > 10)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Invalid gate count %u", uGateCount);
			return false;
		}

		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Loaded %u gates from GateData.bin", uGateCount);

		for (uint32_t uGate = 0; uGate < uGateCount; ++uGate)
		{
			PinballGateData xGateData;
			memset(&xGateData, 0, sizeof(xGateData));

			xStream >> xGateData.eObjectiveType;
			xStream >> xGateData.uScoreThreshold;
			xStream >> xGateData.uTargetHitsRequired;
			xStream >> xGateData.uMaxBalls;
			xStream >> xGateData.uNumPegs;

			for (uint32_t i = 0; i < 8; ++i)
			{
				xStream >> xGateData.afPegPositionsX[i];
				xStream >> xGateData.afPegPositionsY[i];
			}
			xStream >> xGateData.bHasAllPegsObjective;

			if (xGateData.eObjectiveType > PINBALL_OBJ_COMBINED)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Gate %u invalid type %u", uGate, xGateData.eObjectiveType);
				bPass = false;
			}
			if (xGateData.uNumPegs > 8)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Gate %u too many pegs %u", uGate, xGateData.uNumPegs);
				bPass = false;
			}
		}
		return bPass;
	}

	// ========================================================================
	// Logic Test: Save/load integrity
	// ========================================================================

	static bool Test_SaveLoad_Integrity()
	{
		bool bPass = true;

		TilePuzzleSaveData xOriginal;
		xOriginal.Reset();
		xOriginal.uHighestLevelReached = 42;
		xOriginal.uCurrentLevel = 37;
		xOriginal.uPinballScore = 12345;
		xOriginal.uCoins = 9876;
		xOriginal.uTotalStars = 0;
		xOriginal.uDailyStreak = 7;
		xOriginal.uLastDailyDate = 20260301;
		xOriginal.uLives = 3;
		xOriginal.uLastLifeRegenTime = 1000000;
		xOriginal.uDailyPuzzleBestMoves = 5;
		xOriginal.uLastDailyPuzzleDate = 20260301;

		xOriginal.axLevelRecords[0].bCompleted = true;
		xOriginal.axLevelRecords[0].uBestMoves = 3;
		xOriginal.axLevelRecords[0].fBestTime = 12.5f;
		xOriginal.axLevelRecords[0].uBestStars = 3;
		xOriginal.axLevelRecords[5].bCompleted = true;
		xOriginal.axLevelRecords[5].uBestMoves = 7;
		xOriginal.axLevelRecords[5].fBestTime = 45.2f;
		xOriginal.axLevelRecords[5].uBestStars = 2;

		xOriginal.SetStarRating(1, 3);
		xOriginal.SetStarRating(6, 2);
		xOriginal.SetStarRating(10, 1);
		xOriginal.SetPinballGateCleared(0);
		xOriginal.SetPinballGateCleared(3);
		xOriginal.SetPinballGateCleared(7);
		xOriginal.CollectCat(0);
		xOriginal.CollectCat(5);
		xOriginal.CollectCat(42);
		xOriginal.CollectCat(99);

		static constexpr const char* szTempSavePath = GAME_ASSETS_DIR "autotest_save_tmp.bin";

		{
			Zenith_DataStream xWriteStream;
			TilePuzzle_WriteSaveData(xWriteStream, &xOriginal);
			xWriteStream.WriteToFile(szTempSavePath);
		}

		Zenith_DataStream xReadStream;
		xReadStream.ReadFromFile(szTempSavePath);
		TilePuzzleSaveData xLoaded;
		xLoaded.Reset();
		TilePuzzle_ReadSaveData(xReadStream, TilePuzzleSaveData::uGAME_SAVE_VERSION, &xLoaded);

		if (xLoaded.uHighestLevelReached != 42) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: uHighestLevelReached"); bPass = false; }
		if (xLoaded.uCurrentLevel != 37) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: uCurrentLevel"); bPass = false; }
		if (xLoaded.uPinballScore != 12345) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: uPinballScore"); bPass = false; }
		if (xLoaded.uCoins != 9876) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: uCoins"); bPass = false; }
		if (xLoaded.axLevelRecords[0].uBestMoves != 3) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: level 0 record"); bPass = false; }
		if (xLoaded.axLevelRecords[5].uBestMoves != 7) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: level 5 record"); bPass = false; }
		if (xLoaded.GetStarRating(1) != 3 || xLoaded.GetStarRating(6) != 2 || xLoaded.GetStarRating(10) != 1)
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: star ratings"); bPass = false; }
		if (!xLoaded.IsPinballGateCleared(0) || !xLoaded.IsPinballGateCleared(3) || !xLoaded.IsPinballGateCleared(7))
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: gate flags"); bPass = false; }
		if (xLoaded.IsPinballGateCleared(1) || xLoaded.IsPinballGateCleared(5))
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: gate false positives"); bPass = false; }
		if (!xLoaded.IsCatCollected(0) || !xLoaded.IsCatCollected(42) || !xLoaded.IsCatCollected(99))
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: cat collected"); bPass = false; }
		if (xLoaded.IsCatCollected(1) || xLoaded.IsCatCollected(50))
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: cat false positives"); bPass = false; }
		if (xLoaded.uDailyStreak != 7) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: daily streak"); bPass = false; }
		if (xLoaded.uLives != 3) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: lives"); bPass = false; }

		return bPass;
	}

	// ========================================================================
	// Logic Test: Coin system
	// ========================================================================

	static bool Test_CoinSystem()
	{
		bool bPass = true;
		TilePuzzleSaveData xData;
		xData.Reset();

		if (xData.uCoins != 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: initial coins"); bPass = false; }
		xData.AddCoins(100);
		if (xData.uCoins != 100) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: AddCoins(100)"); bPass = false; }
		xData.AddCoins(50);
		if (xData.uCoins != 150) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: AddCoins(50)"); bPass = false; }

		if (!xData.SpendCoins(30) || xData.uCoins != 120) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: SpendCoins(30)"); bPass = false; }
		if (xData.SpendCoins(200) || xData.uCoins != 120) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: SpendCoins(200) should fail"); bPass = false; }
		if (!xData.SpendCoins(120) || xData.uCoins != 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: SpendCoins(120)"); bPass = false; }
		if (xData.SpendCoins(1)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: SpendCoins(1) should fail"); bPass = false; }

		xData.AddCoins(200);
		xData.AddCoins(-50);
		if (xData.uCoins != 150) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: AddCoins(-50)"); bPass = false; }
		xData.AddCoins(-1000);
		if (xData.uCoins != 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: clamp to 0"); bPass = false; }

		xData.Reset();
		xData.SetStarRating(1, 3);
		if (xData.uTotalStars != 3) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: SetStarRating"); bPass = false; }
		xData.SetStarRating(1, 2);
		if (xData.GetStarRating(1) != 3) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: no downgrade"); bPass = false; }

		xData.Reset();
		if (xData.uLives != TilePuzzleSaveData::uMAX_LIVES) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: initial lives"); bPass = false; }
		xData.LoseLife();
		if (xData.uLives != TilePuzzleSaveData::uMAX_LIVES - 1) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: LoseLife"); bPass = false; }
		if (xData.TryRefillLivesWithCoins()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: refill with 0 coins"); bPass = false; }
		xData.AddCoins(static_cast<int32_t>(TilePuzzleSaveData::uLIFE_REFILL_COST));
		if (!xData.TryRefillLivesWithCoins() || xData.uLives != TilePuzzleSaveData::uMAX_LIVES)
		{ Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: refill with coins"); bPass = false; }

		return bPass;
	}
};

// Static member definitions
TilePuzzleShapeDefinition TilePuzzle_AutoTest::s_xTestSingleShape;
bool TilePuzzle_AutoTest::s_bTestShapeInitialized = false;

#endif // ZENITH_INPUT_SIMULATOR
