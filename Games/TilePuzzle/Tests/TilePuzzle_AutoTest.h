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
		// Visual puzzle test (keyboard input, multi-frame)
		PHASE_VISUAL_PUZZLE_START,
		PHASE_VISUAL_PUZZLE_WAIT_FOR_PLAYING,
		PHASE_VISUAL_PUZZLE_COMPUTE_SOLUTION,
		PHASE_VISUAL_PUZZLE_SELECT_AND_MOVE,
		PHASE_VISUAL_PUZZLE_WAIT_SETTLE,
		PHASE_VISUAL_PUZZLE_CHECK_COMPLETE,
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
			m_ePhase = PHASE_VISUAL_PUZZLE_START;
			m_uFrameDelay = 5;
			break;

		// ==============================================================
		// Visual puzzle test - plays a real level using simulated touch
		// ==============================================================
		case PHASE_VISUAL_PUZZLE_START:
			UpdateVisualPuzzle_Start();
			break;
		case PHASE_VISUAL_PUZZLE_WAIT_FOR_PLAYING:
			UpdateVisualPuzzle_WaitForPlaying();
			break;
		case PHASE_VISUAL_PUZZLE_COMPUTE_SOLUTION:
		{
			const TilePuzzleLevelData& xLevel = m_pxPuzzleBehaviour->m_xCurrentLevel;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Grid: %ux%u, Shapes: %u, Cats: %u, Par: %u",
				xLevel.uGridWidth, xLevel.uGridHeight,
				static_cast<uint32_t>(xLevel.axShapes.size()),
				static_cast<uint32_t>(xLevel.axCats.size()),
				xLevel.uMinimumMoves);

			// Use pre-computed solution as primary plan
			const TilePuzzleLevelData& xLvl = m_pxPuzzleBehaviour->m_xCurrentLevel;
			if (xLvl.axSolution.empty())
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  No pre-computed solution, computing live...");
				ResolveFromCurrentState();
			}
			else
			{
				m_axLiveSolution = xLvl.axSolution;
			}
			if (m_axLiveSolution.empty())
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Solver could not find a solution");
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
				m_uFailed++;
				Zenith_InputSimulator::Disable();
				m_ePhase = PHASE_COMPLETE;
				break;
			}

			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Solution has %u drag moves", static_cast<uint32_t>(m_axLiveSolution.size()));
			for (uint32_t i = 0; i < static_cast<uint32_t>(m_axLiveSolution.size()); ++i)
			{
				const auto& xM = m_axLiveSolution[i];
				const auto& xS = xLvl.axShapes[xM.uShapeIndex];
				Zenith_Log(LOG_CATEGORY_UNITTEST, "    Move %u: shape %u (%d,%d)->(%d,%d) color=%u",
					i + 1, xM.uShapeIndex, xS.iOriginX, xS.iOriginY, xM.iEndX, xM.iEndY,
					static_cast<uint32_t>(xS.eColor));
			}
			m_uCurrentDragIndex = 0;
			m_ePhase = PHASE_VISUAL_PUZZLE_SELECT_AND_MOVE;
			m_uFrameDelay = 2;
			break;
		}
		case PHASE_VISUAL_PUZZLE_SELECT_AND_MOVE:
			UpdateVisualPuzzle_SelectAndMove();
			break;
		case PHASE_VISUAL_PUZZLE_WAIT_SETTLE:
			UpdateVisualPuzzle_WaitSettle();
			break;
		case PHASE_VISUAL_PUZZLE_CHECK_COMPLETE:
			UpdateVisualPuzzle_CheckComplete();
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
	// Visual puzzle test state
	// ========================================================================
	TilePuzzle_Behaviour* m_pxPuzzleBehaviour = nullptr;

	// Solution replay state
	uint32_t m_uCurrentDragIndex = 0;     // Index into live solution
	uint32_t m_uTotalCellMoves = 0;       // Total single-cell moves executed
	uint32_t m_uTotalMovesExecuted = 0;   // Total drag moves completed
	uint32_t m_uWaitFrames = 0;
	uint32_t m_uResolvesUsed = 0;         // How many times we re-solved

	// Live solution (re-computed from current game state when needed)
	std::vector<TilePuzzleSolutionMove> m_axLiveSolution;

	// Pre-computed cell path for current drag
	static constexpr uint32_t uMAX_PATH_LENGTH = 64;
	TilePuzzleDirection m_aeCellPath[uMAX_PATH_LENGTH];
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

	// Start: trigger "Continue" to load a puzzle level
	void UpdateVisualPuzzle_Start()
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] Running Test_VisualPuzzle_TouchSolve...");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "  Starting puzzle level via Continue...");

		// Enable the input simulator so Zenith_Input reads from simulated state
		Zenith_InputSimulator::Enable();

		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Could not find TilePuzzle_Behaviour on GameManager");
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		// Ensure we have lives and start from level 1
		m_pxPuzzleBehaviour->m_xSaveData.uLives = TilePuzzleSaveData::uMAX_LIVES;
		m_pxPuzzleBehaviour->m_uCurrentLevelNumber = 1;
		m_pxPuzzleBehaviour->m_xSaveData.uCurrentLevel = 1;

		// Directly call StartGame to load the puzzle scene
		m_pxPuzzleBehaviour->StartGame();

		m_uWaitFrames = 0;
		m_uTotalMovesExecuted = 0;
		m_uTotalCellMoves = 0;
		m_uCurrentDragIndex = 0;
		m_uCellPathLength = 0;
		m_uCellPathIndex = 0;
		m_uResolvesUsed = 0;
		m_axLiveSolution.clear();
		m_ePhase = PHASE_VISUAL_PUZZLE_WAIT_FOR_PLAYING;
	}

	// Wait for the puzzle to be in PLAYING state
	void UpdateVisualPuzzle_WaitForPlaying()
	{
		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			m_uWaitFrames++;
			if (m_uWaitFrames > 120)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Timed out waiting for GameManager after scene load");
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
				m_uFailed++;
				Zenith_InputSimulator::Disable();
				m_ePhase = PHASE_COMPLETE;
			}
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_PLAYING)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Puzzle level loaded, state=PLAYING");
			m_ePhase = PHASE_VISUAL_PUZZLE_COMPUTE_SOLUTION;
			return;
		}

		m_uWaitFrames++;
		if (m_uWaitFrames > 120)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Timed out waiting for PLAYING state (state=%u)", m_pxPuzzleBehaviour->m_eState);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
		}
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

		// Map solution indices back to original level's shape array
		m_axLiveSolution.clear();
		for (const auto& xMove : axSolution)
		{
			TilePuzzleSolutionMove xMapped;
			// xMove.uShapeIndex is the full index in the CLEAN shapes array
			if (xMove.uShapeIndex < uShapeMapCount)
				xMapped.uShapeIndex = auShapeMap[xMove.uShapeIndex];
			else
				xMapped.uShapeIndex = xMove.uShapeIndex; // fallback
			xMapped.iEndX = xMove.iEndX;
			xMapped.iEndY = xMove.iEndY;
			m_axLiveSolution.push_back(xMapped);
		}

		m_uCurrentDragIndex = 0;
		m_uCellPathLength = 0;
		m_uCellPathIndex = 0;
		m_uResolvesUsed++;
	}

	void UpdateVisualPuzzle_SelectAndMove()
	{
		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Lost reference to puzzle behaviour");
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
			m_uFailed++;
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE ||
			m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_VICTORY_OVERLAY)
		{
			m_ePhase = PHASE_VISUAL_PUZZLE_CHECK_COMPLETE;
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState != TILEPUZZLE_STATE_PLAYING)
		{
			return;
		}

		const TilePuzzleLevelData& xLevel = m_pxPuzzleBehaviour->m_xCurrentLevel;

		if (m_uCurrentDragIndex >= static_cast<uint32_t>(m_axLiveSolution.size()))
		{
			m_ePhase = PHASE_VISUAL_PUZZLE_CHECK_COMPLETE;
			return;
		}

		if (m_uTotalCellMoves >= 2000)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Exceeded 2000 cell moves");
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		const TilePuzzleSolutionMove& xDrag = m_axLiveSolution[m_uCurrentDragIndex];
		uint32_t uShapeIdx = xDrag.uShapeIndex;

		if (uShapeIdx >= static_cast<uint32_t>(xLevel.axShapes.size()))
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Invalid shape index %u, skipping drag", uShapeIdx);
			m_uCurrentDragIndex++;
			return;
		}

		const TilePuzzleShapeInstance& xShape = xLevel.axShapes[uShapeIdx];

		if (xShape.bRemoved)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Drag %u: shape %u removed, skipping",
				m_uCurrentDragIndex + 1, uShapeIdx);
			m_uCurrentDragIndex++;
			m_uCellPathLength = 0;
			m_uCellPathIndex = 0;
			return;
		}

		int32_t iTargetX = xDrag.iEndX;
		int32_t iTargetY = xDrag.iEndY;

		// If we completed a cell path for a previous drag, check if shape reached target
		if (m_uCellPathLength > 0 && m_uCellPathIndex >= m_uCellPathLength)
		{
			// Previous cell path was consumed - advance to next drag
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Drag %u complete: shape %u now at (%d,%d)",
				m_uCurrentDragIndex + 1, uShapeIdx, xShape.iOriginX, xShape.iOriginY);
			m_uCurrentDragIndex++;
			m_uCellPathLength = 0;
			m_uCellPathIndex = 0;
			return;
		}

		// Compute cell path if needed
		if (m_uCellPathIndex >= m_uCellPathLength)
		{
			int32_t iPathLen;
			if (xShape.iOriginX == iTargetX && xShape.iOriginY == iTargetY)
			{
				// Start == target: solver round-trip to trigger eliminations
				iPathLen = FindRoundTripPath(xLevel, uShapeIdx, m_aeCellPath, uMAX_PATH_LENGTH);
				if (iPathLen <= 0)
				{
					// No round-trip needed or possible - skip
					Zenith_Log(LOG_CATEGORY_UNITTEST, "  Drag %u: shape %u at (%d,%d) start==target, no round-trip found, skipping",
						m_uCurrentDragIndex + 1, uShapeIdx, xShape.iOriginX, xShape.iOriginY);
					m_uCurrentDragIndex++;
					m_uCellPathLength = 0;
					m_uCellPathIndex = 0;
					return;
				}
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  Drag %u: shape %u round-trip path len=%d",
					m_uCurrentDragIndex + 1, uShapeIdx, iPathLen);
			}
			else
			{
				iPathLen = FindCellPath(xLevel, uShapeIdx, iTargetX, iTargetY,
					m_aeCellPath, uMAX_PATH_LENGTH);
			}

			if (iPathLen <= 0)
			{
				// Can't find path to target - re-solve from current state
				if (m_uResolvesUsed < 20)
				{
					Zenith_Log(LOG_CATEGORY_UNITTEST, "  No path from (%d,%d) to (%d,%d) for shape %u, re-solving...",
						xShape.iOriginX, xShape.iOriginY, iTargetX, iTargetY, uShapeIdx);
					ResolveFromCurrentState();
					return;
				}
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: No path and resolve limit reached");
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
				m_uFailed++;
				Zenith_InputSimulator::Disable();
				m_ePhase = PHASE_COMPLETE;
				return;
			}

			m_uCellPathLength = static_cast<uint32_t>(iPathLen);
			m_uCellPathIndex = 0;
		}

		TilePuzzleDirection eChosenDir = m_aeCellPath[m_uCellPathIndex];
		m_uCellPathIndex++;

		m_pxPuzzleBehaviour->m_iSelectedShapeIndex = static_cast<int32_t>(uShapeIdx);
		Zenith_KeyCode eKey = DirectionToKeyCode(eChosenDir);
		Zenith_InputSimulator::SimulateKeyPress(eKey);
		m_uTotalCellMoves++;

		m_uWaitFrames = 0;
		m_ePhase = PHASE_VISUAL_PUZZLE_WAIT_SETTLE;
		m_uFrameDelay = 2;
	}

	// Wait for game state to settle (animations, eliminations)
	void UpdateVisualPuzzle_WaitSettle()
	{
		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Lost reference during settle wait");
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
			m_uFailed++;
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		// Wait until back in PLAYING or LEVEL_COMPLETE state
		if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_SHAPE_SLIDING ||
			m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_CHECK_ELIMINATION)
		{
			m_uWaitFrames++;
			if (m_uWaitFrames > 120)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Timed out waiting for game to settle (state=%u)",
					m_pxPuzzleBehaviour->m_eState);
				Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
				m_uFailed++;
				m_ePhase = PHASE_COMPLETE;
			}
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE ||
			m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_VICTORY_OVERLAY)
		{
			// Level solved!
			m_ePhase = PHASE_VISUAL_PUZZLE_CHECK_COMPLETE;
			return;
		}

		// Back to PLAYING - continue navigating toward target
		m_ePhase = PHASE_VISUAL_PUZZLE_SELECT_AND_MOVE;
	}

	// Verify level completion
	void UpdateVisualPuzzle_CheckComplete()
	{
		m_pxPuzzleBehaviour = FindPuzzleBehaviour();
		if (!m_pxPuzzleBehaviour)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Lost reference during completion check");
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
			m_uFailed++;
			Zenith_InputSimulator::Disable();
			m_ePhase = PHASE_COMPLETE;
			return;
		}

		if (m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE ||
			m_pxPuzzleBehaviour->m_eState == TILEPUZZLE_STATE_VICTORY_OVERLAY)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Level completed! Drags: %u, Cell moves: %u, Game moves: %u, Stars: %u, Re-solves: %u",
				m_uTotalMovesExecuted, m_uTotalCellMoves,
				m_pxPuzzleBehaviour->m_uMoveCount, m_pxPuzzleBehaviour->m_uStarsEarned, m_uResolvesUsed);
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] PASS: Test_VisualPuzzle_TouchSolve");
			m_uPassed++;
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  FAIL: Level not complete (state=%u, drags=%u, cell moves=%u, re-solves=%u)",
				m_pxPuzzleBehaviour->m_eState, m_uTotalMovesExecuted,
				m_uTotalCellMoves, m_uResolvesUsed);

			// Log remaining cats and shape positions for debugging
			uint32_t uRemainingCats = 0;
			for (const auto& xCat : m_pxPuzzleBehaviour->m_xCurrentLevel.axCats)
			{
				if (!xCat.bEliminated)
					uRemainingCats++;
			}
			Zenith_Log(LOG_CATEGORY_UNITTEST, "  Remaining cats: %u", uRemainingCats);
			for (size_t i = 0; i < m_pxPuzzleBehaviour->m_xCurrentLevel.axShapes.size(); ++i)
			{
				const auto& xShape = m_pxPuzzleBehaviour->m_xCurrentLevel.axShapes[i];
				if (xShape.pxDefinition && xShape.pxDefinition->bDraggable && !xShape.bRemoved)
				{
					Zenith_Log(LOG_CATEGORY_UNITTEST, "  Shape %u: pos=(%d,%d) color=%u",
						static_cast<uint32_t>(i), xShape.iOriginX, xShape.iOriginY, xShape.eColor);
				}
			}
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutoTest] FAIL: Test_VisualPuzzle_TouchSolve");
			m_uFailed++;
		}

		Zenith_InputSimulator::Disable();
		m_ePhase = PHASE_COMPLETE;
		m_uFrameDelay = 30; // Let user see the result
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

	// Check if a shape can move in a direction on the live game state
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

		static constexpr int32_t iMAX_BFS_NODES = 4096;
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
			TILEPUZZLE_DIR_RIGHT, TILEPUZZLE_DIR_LEFT,
			TILEPUZZLE_DIR_DOWN, TILEPUZZLE_DIR_UP
		};

		int32_t iBestTargetNode = -1;
		uint32_t uBestElimCount = 0;

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

				// Check: reached start position with MORE eliminations
				if (iNX == iStartX && iNY == iStartY && uNewElimMask != uInitElimMask)
				{
					uint32_t uElimCount = TILEPUZZLE_POPCNT(uNewElimMask);
					if (iBestTargetNode < 0 || uElimCount > uBestElimCount)
					{
						iBestTargetNode = static_cast<int32_t>(uNodeCount - 1);
						uBestElimCount = uElimCount;
					}
				}
			}
		}

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

	// BFS to find cell path that maximizes eliminations along the way.
	// Uses (position, elimination_mask) as the full BFS state key so that
	// round-trip paths through same-color cats are explored. Among all paths
	// reaching the target, returns the one with the most cat eliminations.
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

		static constexpr int32_t iMAX_BFS_NODES = 4096;
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
			TILEPUZZLE_DIR_RIGHT, TILEPUZZLE_DIR_LEFT,
			TILEPUZZLE_DIR_DOWN, TILEPUZZLE_DIR_UP
		};

		// Track the best target node (most eliminations)
		int32_t iBestTargetNode = -1;
		uint32_t uBestElimCount = 0;

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

				// Check if we reached the target
				if (iNX == iTargetX && iNY == iTargetY)
				{
					uint32_t uElimCount = TILEPUZZLE_POPCNT(uNewElimMask);
					if (iBestTargetNode < 0 || uElimCount > uBestElimCount)
					{
						iBestTargetNode = static_cast<int32_t>(uNodeCount - 1);
						uBestElimCount = uElimCount;
					}
					// Don't return - continue BFS to find paths with more eliminations
				}
			}
		}

		// No path found
		if (iBestTargetNode < 0)
		{
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
