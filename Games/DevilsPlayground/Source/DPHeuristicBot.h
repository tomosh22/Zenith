#pragma once

/**
 * DPHeuristicBot - rule-based AI player for the DP verification harness.
 *
 * Phase 3a of the telemetry / verification system (2026-05-16).
 *
 * The bot drives Zenith_InputSimulator each frame based on a goal-stack
 * derived from the current game state (possession, held item, priest
 * proximity, pentagram readiness). It is NOT a full navmesh-driven
 * planner -- straight-line movement plus a stuck-detector + random
 * strafe is good enough to exercise every game mechanic the verification
 * analyzer cares about (possession, movement, sprint, walk-quiet,
 * F-interact, G-drop, objective handoff, victory). Phase 3b will
 * upgrade pathing to use DP_AI::GetOrBuildLevelNavMesh +
 * Zenith_Pathfinding::FindPath; the bot's public surface (Reset / Tick)
 * stays stable across the upgrade.
 *
 * Threading: Tick MUST be called from the main thread between
 * Zenith_InputSimulator::BeginTestFrame() and the next StepFrame.
 * The bot reads engine state via DP_Player + DP_Win + DP_Query queries,
 * which all assume single-threaded main-thread access.
 *
 * Build flag: ZENITH_INPUT_SIMULATOR. Tests + tools-only.
 */

#ifdef ZENITH_INPUT_SIMULATOR

#include "EntityComponent/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"
#include "Source/DevilsPlayground_Tags.h"

#include <cstdint>

namespace DPHeuristicBot
{
	// Current goal -- exposed for the test's verification step so it can
	// assert the bot tried each goal type during the playthrough.
	enum class Goal : uint8_t
	{
		Idle           = 0,
		PossessClosest = 1, // not possessed -> click nearest villager
		WalkToObjective= 2, // possessed, no item, objective somewhere -> walk to it
		PickupItem     = 3, // possessed, no item, within interact radius -> stop
		WalkToPentagram= 4, // holding objective -> walk to pentagram
		WalkToForge    = 5, // holding iron + no key yet -> walk to forge
		FleeFromPriest = 6, // priest within kFleeDistance -> walk AWAY
		BodySwap       = 7, // life < kSwapThreshold -> click a fresh villager
	};

	// Reset bot state. Call once per test setup BEFORE the first Tick;
	// clears the stuck detector, last-position cache, current goal, and
	// the seeded random walk state.
	void Reset();

	// Step the bot one frame. fDt is the fixed timestep used by the
	// test harness (1/60). iFrame is the absolute frame index since the
	// start of the test (used for periodic re-pick of goal, deterministic
	// strafe direction).
	void Tick(int iFrame, float fDt);

	// Read the bot's current decision (set by the most recent Tick).
	// Tests use this to verify the goal-stack made at least one decision
	// of each type across the playthrough.
	Goal GetCurrentGoal();

	// Cumulative frame counters -- the test asserts non-zero values
	// against these to verify the bot exercised the mechanic. Reset is
	// part of Reset().
	uint32_t GetSprintFrameCount();
	uint32_t GetWalkQuietFrameCount();
	uint32_t GetInteractPressCount();
	uint32_t GetDropPressCount();
	uint32_t GetPossessClickCount();

	// =========================================================
	// TestSurface -- testability hooks. Exposes the bot's pure
	// helpers (grid math, A*, goal dispatch) so unit tests can pin
	// their behaviour against a hand-built walkability grid +
	// observation tuple without needing a scene or real entities.
	//
	// Phase 3b added ~200 lines of A* / replan code; this namespace
	// is the contract that lets Test_DPHeuristicBot_* exercise it
	// without coupling tests to internal storage.
	//
	// All functions are no-ops in shipping builds (the whole header
	// is #ifdef'd to ZENITH_INPUT_SIMULATOR).
	// =========================================================
	namespace TestSurface
	{
		// Pathing-grid configuration constants. Tests use these to
		// build hand-crafted walkability grids of the right size.
		int   GetGridDim();
		float GetCellSize();
		float GetOriginX();
		float GetOriginZ();

		// Inject a walkability grid for tests. abGrid must have
		// length GetGridDim() * GetGridDim(); the function copies in
		// and marks the grid as built (BuildPathGrid will not rebuild
		// from raycasts). Pair with ResetForTest() to release.
		void SetWalkabilityGridForTest(const bool* abGrid);
		void ResetForTest();

		// Grid math.
		void WorldToCell(const Zenith_Maths::Vector3& xWorld, int& iOutX, int& iOutZ);
		Zenith_Maths::Vector3 CellToWorld(int iX, int iZ);
		bool IsCellWalkable(int iX, int iZ);

		// Spiral-outward fallback. Returns -1 if no walkable cell
		// within iMaxRing rings of (iX, iZ).
		int SnapToWalkable(int iX, int iZ, int iMaxRing = 8);

		// A* over the injected grid. Returns true + populates xOutPath
		// with world-space waypoints from start to end. Returns false
		// when no path exists.
		bool ComputePathAStar(const Zenith_Maths::Vector3& xStart,
		                     const Zenith_Maths::Vector3& xEnd,
		                     Zenith_Vector<Zenith_Maths::Vector3>& xOutPath);

		// Pure goal-dispatch table. Production PickGoal builds an
		// Observation from DP_Player / scene queries; this entry
		// point takes the same logical state as primitives so tests
		// can pin every branch.
		//
		// fPriestDist < 0 means "no priest in scene / unknown".
		// fLife = 0 is acceptable when bPossessed == false.
		Goal PickGoalForState(bool bPossessed,
		                      float fPriestDist,
		                      float fLife,
		                      DP_ItemTag eHeldTag,
		                      bool bPentagramPresent,
		                      bool bForgePresent,
		                      bool bObjectiveItemAvailable);
	}
}

#endif // ZENITH_INPUT_SIMULATOR
