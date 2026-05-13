#pragma once

// Zenith_NavMeshTestPathfinder
//
// Test-harness pathfinder wrapper around `Zenith_Pathfinding::FindPath`.
// Lives behind ZENITH_INPUT_SIMULATOR so it's only compiled into test
// builds; production AI uses `Zenith_NavMeshAgent` directly.
//
// Why this exists (MVP-1.2.9 — DP roadmap):
//   The DP test `Test_HumanPlaythrough.cpp` historically ran its bot via
//   `ComputePathAStar`, a grid-based pathfinder maintained inside the
//   test file. The grid is **decoupled from the navmesh** -- it uses its
//   own per-cell walkability flags computed from the level's collider
//   geometry. Once the production navmesh evolved (PR #32 walls block
//   paths, PR #33 perf fix, PR #35 door opt-out), the grid pathfinder
//   started disagreeing with the real navmesh: a bot could path through
//   "walkable" grid cells that the navmesh considered blocked, or vice
//   versa. The test could pass its MVP acceptance by routing through
//   walls in a way the priest's pursuit logic could never.
//
// Contract:
//   * `ComputePath(navmesh, start, end, outWaypoints)` returns true on
//     SUCCESS or PARTIAL, false on FAILED. The smoothed waypoint list is
//     written to outWaypoints.
//   * No-op (returns false) if navmesh has zero polygons.
//
// Future direction: when long-form bot tests stabilise, the test
// pathfinder may grow per-bot path caching / replan triggers / etc.
// For now it's a thin shim so the test bot uses **the same path the
// priest would use** and we don't ship two pathfinders.

#ifdef ZENITH_INPUT_SIMULATOR

#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_Pathfinding.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

namespace Zenith_NavMeshTestPathfinder
{
	// Returns true if a usable path was found (SUCCESS or PARTIAL).
	// The smoothed waypoints (incl. start + end) are written to
	// axWaypointsOut. The vector is cleared before writing.
	inline bool ComputePath(const Zenith_NavMesh& xNavMesh,
		const Zenith_Maths::Vector3& xStart,
		const Zenith_Maths::Vector3& xEnd,
		Zenith_Vector<Zenith_Maths::Vector3>& axWaypointsOut)
	{
		axWaypointsOut.Clear();
		if (xNavMesh.GetPolygonCount() == 0)
		{
			return false;
		}

		Zenith_PathResult xResult = Zenith_Pathfinding::FindPath(xNavMesh, xStart, xEnd);
		if (xResult.m_eStatus != Zenith_PathResult::Status::SUCCESS &&
		    xResult.m_eStatus != Zenith_PathResult::Status::PARTIAL)
		{
			return false;
		}

		const uint32_t uCount = xResult.m_axWaypoints.GetSize();
		for (uint32_t u = 0; u < uCount; ++u)
		{
			axWaypointsOut.PushBack(xResult.m_axWaypoints.Get(u));
		}
		return true;
	}
}

#endif // ZENITH_INPUT_SIMULATOR
