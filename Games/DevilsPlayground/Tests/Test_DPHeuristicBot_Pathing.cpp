#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Source/DPHeuristicBot.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include <cstdio>
#include <vector>
#include <cmath>

// ============================================================================
// Test_DPHeuristicBot_Pathing
//
// Pins the Phase 3b A* / grid math added on 2026-05-16. Each test cluster
// hand-builds a walkability grid via TestSurface::SetWalkabilityGridForTest
// and asserts the corresponding helper's behaviour without ever loading a
// scene or running the engine main loop.
//
// Clusters:
//   1. WorldToCell <-> CellToWorld round-trip.
//   2. IsCellWalkable bounds + flag semantics.
//   3. SnapToWalkable: same cell, adjacent ring, max-ring fallback.
//   4. ComputePathAStar: straight corridor / L-bend / walled-off island.
//   5. ComputePathAStar: start == end produces a one-waypoint path.
//   6. ComputePathAStar: end waypoint replaced with exact target coords.
//
// Each cluster ResetForTest()s the grid before populating it so test
// state leakage across clusters can't hide bugs.
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";
	bool Fail(const char* sz) { g_szFailureReason = sz; return false; }

	using DPHeuristicBot::TestSurface::GetGridDim;
	using DPHeuristicBot::TestSurface::GetCellSize;
	using DPHeuristicBot::TestSurface::GetOriginX;
	using DPHeuristicBot::TestSurface::GetOriginZ;

	// Build a fully walkable grid (open arena). Using unsigned char as
	// the storage type instead of bool because std::vector<bool> is a
	// bit-packed specialisation that doesn't expose .data().
	// sizeof(unsigned char) == sizeof(bool) on every platform we care
	// about, so reinterpret_cast at the TestSurface boundary is benign.
	std::vector<unsigned char> BuildAllWalkable()
	{
		const int N = GetGridDim();
		return std::vector<unsigned char>(static_cast<size_t>(N * N), 1u);
	}

	std::vector<unsigned char> BuildOpenArena()
	{
		return BuildAllWalkable();
	}

	void SetCell(std::vector<unsigned char>& abGrid, int iX, int iZ, bool bWalkable)
	{
		const int N = GetGridDim();
		if (iX < 0 || iX >= N || iZ < 0 || iZ >= N) return;
		abGrid[static_cast<size_t>(iZ * N + iX)] = bWalkable ? 1u : 0u;
	}

	void LoadGrid(const std::vector<unsigned char>& abGrid)
	{
		DPHeuristicBot::TestSurface::ResetForTest();
		DPHeuristicBot::TestSurface::SetWalkabilityGridForTest(
			reinterpret_cast<const bool*>(abGrid.data()));
	}
}

// 1. WorldToCell <-> CellToWorld round-trip.
static bool TestWorldCellRoundTrip()
{
	// CellToWorld returns the cell CENTRE; WorldToCell of that centre
	// should give us back the same indices. Walk every 5th cell so the
	// test stays quick.
	const int N = GetGridDim();
	for (int z = 0; z < N; z += 5)
	{
		for (int x = 0; x < N; x += 5)
		{
			const Zenith_Maths::Vector3 xW = DPHeuristicBot::TestSurface::CellToWorld(x, z);
			int rx = -1, rz = -1;
			DPHeuristicBot::TestSurface::WorldToCell(xW, rx, rz);
			if (rx != x || rz != z)
			{
				static char sBuf[128];
				std::snprintf(sBuf, sizeof(sBuf),
					"round-trip mismatch at (%d,%d) -> world (%.2f,%.2f) -> (%d,%d)",
					x, z, xW.x, xW.z, rx, rz);
				return Fail(sBuf);
			}
		}
	}
	return true;
}

// 2. IsCellWalkable: bounds + flag semantics.
static bool TestIsCellWalkable()
{
	std::vector<unsigned char> abGrid = BuildAllWalkable();
	SetCell(abGrid, 10, 10, false);
	SetCell(abGrid, 20, 30, false);
	LoadGrid(abGrid);

	// Out-of-range -> false.
	if (DPHeuristicBot::TestSurface::IsCellWalkable(-1, 5))    return Fail("oob: x=-1 should be unwalkable");
	if (DPHeuristicBot::TestSurface::IsCellWalkable(5, -1))    return Fail("oob: z=-1 should be unwalkable");
	if (DPHeuristicBot::TestSurface::IsCellWalkable(GetGridDim(), 5))     return Fail("oob: x=N should be unwalkable");
	if (DPHeuristicBot::TestSurface::IsCellWalkable(5, GetGridDim()))     return Fail("oob: z=N should be unwalkable");
	if (DPHeuristicBot::TestSurface::IsCellWalkable(GetGridDim()+50, 0))  return Fail("oob: x=N+50 should be unwalkable");

	// In-range walkable.
	if (!DPHeuristicBot::TestSurface::IsCellWalkable(0, 0))   return Fail("inrange: (0,0) should be walkable");
	if (!DPHeuristicBot::TestSurface::IsCellWalkable(5, 5))   return Fail("inrange: (5,5) should be walkable");

	// Explicitly-marked unwalkable.
	if (DPHeuristicBot::TestSurface::IsCellWalkable(10, 10))  return Fail("flag: (10,10) should be unwalkable");
	if (DPHeuristicBot::TestSurface::IsCellWalkable(20, 30))  return Fail("flag: (20,30) should be unwalkable");

	return true;
}

// 3. SnapToWalkable: spiral-outward search.
static bool TestSnapToWalkable()
{
	std::vector<unsigned char> abGrid(GetGridDim() * GetGridDim(), false);
	// Single walkable cell at (15, 15). Everything else blocked.
	SetCell(abGrid, 15, 15, true);
	LoadGrid(abGrid);

	const int N = GetGridDim();

	// Same cell already walkable -> returns its own linear index.
	{
		const int iIdx = DPHeuristicBot::TestSurface::SnapToWalkable(15, 15, 8);
		if (iIdx != 15 * N + 15) return Fail("snap-same: should return own index");
	}

	// Adjacent ring -- (14, 15) is blocked, but (15, 15) is one step away.
	// With iMaxRing >= 1 the spiral finds it.
	{
		const int iIdx = DPHeuristicBot::TestSurface::SnapToWalkable(14, 15, 8);
		if (iIdx != 15 * N + 15) return Fail("snap-1ring: should find (15,15)");
	}

	// Distant cell with iMaxRing too small -> returns -1.
	{
		const int iIdx = DPHeuristicBot::TestSurface::SnapToWalkable(20, 20, 2);
		if (iIdx != -1) return Fail("snap-toofar-smallmax: should fail (return -1)");
	}

	// Distant cell with iMaxRing large enough -> finds (15, 15).
	{
		const int iIdx = DPHeuristicBot::TestSurface::SnapToWalkable(20, 20, 8);
		if (iIdx != 15 * N + 15) return Fail("snap-toofar-largemax: should find (15,15)");
	}

	// All unwalkable -> returns -1 even with huge ring.
	{
		std::vector<unsigned char> abEmpty(GetGridDim() * GetGridDim(), false);
		LoadGrid(abEmpty);
		const int iIdx = DPHeuristicBot::TestSurface::SnapToWalkable(30, 30, 16);
		if (iIdx != -1) return Fail("snap-empty: empty grid should return -1");
	}

	return true;
}

// 4a. ComputePathAStar: straight corridor (no obstacles).
static bool TestPathOpenArena()
{
	std::vector<unsigned char> abGrid = BuildOpenArena();
	LoadGrid(abGrid);

	// Start at (5,5), end at (25,5). Path should be ~20 cells long (1m
	// hops at GetCellSize() spacing, mostly horizontal).
	const Zenith_Maths::Vector3 xStart = DPHeuristicBot::TestSurface::CellToWorld(5, 5);
	const Zenith_Maths::Vector3 xEnd   = DPHeuristicBot::TestSurface::CellToWorld(25, 5);

	Zenith_Vector<Zenith_Maths::Vector3> xPath;
	if (!DPHeuristicBot::TestSurface::ComputePathAStar(xStart, xEnd, xPath))
		return Fail("open-arena: A* failed on open grid");
	if (xPath.GetSize() < 2u) return Fail("open-arena: path should have >=2 waypoints");
	// Final waypoint should equal xEnd exactly (the bot replaces the
	// last cell-centre with the precise target).
	const Zenith_Maths::Vector3& xLast = xPath.Get(xPath.GetSize() - 1u);
	if (std::fabs(xLast.x - xEnd.x) > 0.01f ||
	    std::fabs(xLast.z - xEnd.z) > 0.01f)
	{
		return Fail("open-arena: last waypoint not snapped to target");
	}
	return true;
}

// 4b. ComputePathAStar: L-bend obstacle (a wall column the path must route around).
static bool TestPathLBend()
{
	std::vector<unsigned char> abGrid = BuildOpenArena();
	// Vertical wall: column x=15, z=0..20. Path from (5, 10) to (25, 10)
	// must route around (either north over z=20+ or south... but only
	// north is open since the wall is z=0..20; "south" would mean z<0).
	for (int z = 0; z <= 20; ++z) SetCell(abGrid, 15, z, false);
	LoadGrid(abGrid);

	const Zenith_Maths::Vector3 xStart = DPHeuristicBot::TestSurface::CellToWorld(5, 10);
	const Zenith_Maths::Vector3 xEnd   = DPHeuristicBot::TestSurface::CellToWorld(25, 10);

	Zenith_Vector<Zenith_Maths::Vector3> xPath;
	if (!DPHeuristicBot::TestSurface::ComputePathAStar(xStart, xEnd, xPath))
		return Fail("l-bend: A* failed when L-route exists");
	if (xPath.GetSize() < 10u)
		return Fail("l-bend: path too short to be valid detour");

	// Every waypoint should be in a walkable cell (sanity).
	for (uint32_t i = 0; i < xPath.GetSize(); ++i)
	{
		int x, z;
		DPHeuristicBot::TestSurface::WorldToCell(xPath.Get(i), x, z);
		// The last waypoint may be the exact target which sits at the
		// same cell as the planner picked, so skip the bounds check on it.
		if (i + 1 < xPath.GetSize() &&
		    !DPHeuristicBot::TestSurface::IsCellWalkable(x, z))
		{
			return Fail("l-bend: waypoint landed on unwalkable cell");
		}
	}
	return true;
}

// 4c. ComputePathAStar: walled-off island has no path.
static bool TestPathWalledOff()
{
	std::vector<unsigned char> abGrid(GetGridDim() * GetGridDim(), false);
	// Two islands of walkable cells, no connection.
	for (int z = 5; z <= 8; ++z)
		for (int x = 5; x <= 8; ++x)
			SetCell(abGrid, x, z, true);
	for (int z = 20; z <= 23; ++z)
		for (int x = 20; x <= 23; ++x)
			SetCell(abGrid, x, z, true);
	LoadGrid(abGrid);

	const Zenith_Maths::Vector3 xStart = DPHeuristicBot::TestSurface::CellToWorld(6, 6);
	const Zenith_Maths::Vector3 xEnd   = DPHeuristicBot::TestSurface::CellToWorld(21, 21);

	Zenith_Vector<Zenith_Maths::Vector3> xPath;
	if (DPHeuristicBot::TestSurface::ComputePathAStar(xStart, xEnd, xPath))
	{
		return Fail("walled-off: A* should report no path between islands");
	}
	return true;
}

// 5. Start == End -> single-waypoint path (no-op route).
static bool TestPathStartEqualsEnd()
{
	std::vector<unsigned char> abGrid = BuildOpenArena();
	LoadGrid(abGrid);

	const Zenith_Maths::Vector3 xPt = DPHeuristicBot::TestSurface::CellToWorld(10, 10);
	Zenith_Vector<Zenith_Maths::Vector3> xPath;
	if (!DPHeuristicBot::TestSurface::ComputePathAStar(xPt, xPt, xPath))
		return Fail("start==end: A* should succeed trivially");
	if (xPath.GetSize() == 0u)
		return Fail("start==end: path should not be empty");
	return true;
}

// 6a. Phase-5-audit scene-aware grid cache. Verify that
// SetWalkabilityGridForTest stamps the kTestInjectedHandle sentinel
// (INT_MIN) so BuildPathGrid short-circuits and doesn't raycast over
// the test grid, and that ResetForTest clears the handle back to -1
// ("no grid cached").
static bool TestSceneHandleCache()
{
	std::vector<unsigned char> abGrid = BuildAllWalkable();
	LoadGrid(abGrid);
	const int iSentinel = DPHeuristicBot::TestSurface::GetPathGridSceneHandleForTest();
	// Magic sentinel value -- documented invariant: must be far from any
	// real Zenith_Scene handle so BuildPathGrid recognises it. We don't
	// reach into the cpp's kTestInjectedHandle; we assert "negative + far
	// below -1" which is sufficient to prove the contract.
	if (iSentinel >= 0 || iSentinel > -1000)
		return Fail("scene-cache: sentinel not stamped (handle should be a large negative value)");
	DPHeuristicBot::TestSurface::ResetForTest();
	if (DPHeuristicBot::TestSurface::GetPathGridSceneHandleForTest() != -1)
		return Fail("scene-cache: ResetForTest didn't clear handle to -1");
	return true;
}

// 6. Final waypoint = exact target (the planner replaces the last cell
// centre with xEnd so the bot lands on the target, not the cell centre).
static bool TestPathLastWaypointExact()
{
	std::vector<unsigned char> abGrid = BuildOpenArena();
	LoadGrid(abGrid);

	// Aim at a point OFF the cell centre.
	const Zenith_Maths::Vector3 xStart = DPHeuristicBot::TestSurface::CellToWorld(5, 5);
	const float fOffCx = GetOriginX() + (10 + 0.5f) * GetCellSize() + 0.7f;
	const float fOffCz = GetOriginZ() + (10 + 0.5f) * GetCellSize() - 0.3f;
	const Zenith_Maths::Vector3 xEnd(fOffCx, 1.0f, fOffCz);

	Zenith_Vector<Zenith_Maths::Vector3> xPath;
	if (!DPHeuristicBot::TestSurface::ComputePathAStar(xStart, xEnd, xPath))
		return Fail("last-exact: A* failed");
	const Zenith_Maths::Vector3& xLast = xPath.Get(xPath.GetSize() - 1u);
	if (std::fabs(xLast.x - fOffCx) > 0.01f ||
	    std::fabs(xLast.z - fOffCz) > 0.01f)
	{
		return Fail("last-exact: final waypoint should equal xEnd exactly");
	}
	return true;
}

static void Setup_Pathing()
{
	g_bPassed = false;
	g_szFailureReason = "";

	if (!TestWorldCellRoundTrip())   return;
	if (!TestIsCellWalkable())       return;
	if (!TestSnapToWalkable())       return;
	if (!TestPathOpenArena())        return;
	if (!TestPathLBend())            return;
	if (!TestPathWalledOff())        return;
	if (!TestPathStartEqualsEnd())   return;
	if (!TestPathLastWaypointExact())return;
	if (!TestSceneHandleCache())     return;

	// Reset so subsequent tests start with a clean recorder + clean grid.
	DPHeuristicBot::TestSurface::ResetForTest();

	g_bPassed = true;
	std::printf("[DPHeuristicBot_Pathing] all 9 cluster tests passed\n");
	std::fflush(stdout);
}

static bool Step_Pathing(int /*iFrame*/) { return false; }

static bool Verify_Pathing()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "DPHeuristicBot_Pathing: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPathingTest = {
	"Test_DPHeuristicBot_Pathing",
	&Setup_Pathing,
	&Step_Pathing,
	&Verify_Pathing,
	30
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPathingTest);

#endif // ZENITH_INPUT_SIMULATOR
