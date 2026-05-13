#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_Pathfinding.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// Test_T0NavMesh_QueryCountIncrements (MVP-0.4.4)
//
// Tier-0 smoke for Zenith_NavMesh::GetQueryCountForTest:
//   1. ResetQueryCountForTest() zeroes the counter.
//   2. Each Zenith_Pathfinding::FindPath call increments by exactly 1.
//   3. FindPath calls against an empty navmesh still increment (the counter
//      is per-call, not per-success).
//
// Pure engine-API exercise -- no scene, no entity, no graphics. Builds a
// trivial empty NavMesh and runs FindPath against it (which will return
// FAILED since there are no polygons; the counter still ticks).
// ============================================================================

namespace
{
	int g_iFailures = 0;
}

static void Setup_T0NavMesh_QueryCountIncrements()
{
	g_iFailures = 0;
	Zenith_NavMesh::ResetQueryCountForTest();
}

static bool Step_T0NavMesh_QueryCountIncrements(int iFrame)
{
	(void)iFrame;
	return false;
}

static bool Verify_T0NavMesh_QueryCountIncrements()
{
	g_iFailures = 0;

	// 1) Reset leaves the counter at 0.
	if (Zenith_NavMesh::GetQueryCountForTest() != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0NavMesh: counter not zero after Reset (got %u)",
			Zenith_NavMesh::GetQueryCountForTest());
		++g_iFailures;
	}

	// 2) Build a minimal empty navmesh and issue 3 path queries. The counter
	//    should land at 3 regardless of pathfinding result. Empty navmesh =>
	//    Zenith_Pathfinding::FindPath returns FAILED; we don't care about
	//    the result, only that the counter ticked.
	Zenith_NavMesh xEmptyMesh;
	const Zenith_Maths::Vector3 xStart(0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xEnd(10.0f, 0.0f, 10.0f);

	(void)Zenith_Pathfinding::FindPath(xEmptyMesh, xStart, xEnd);
	(void)Zenith_Pathfinding::FindPath(xEmptyMesh, xStart, xEnd);
	(void)Zenith_Pathfinding::FindPath(xEmptyMesh, xStart, xEnd);

	const u_int uAfterThree = Zenith_NavMesh::GetQueryCountForTest();
	if (uAfterThree != 3)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0NavMesh: expected counter=3 after 3 FindPath calls, got %u",
			uAfterThree);
		++g_iFailures;
	}

	// 3) ResetQueryCountForTest puts it back at zero.
	Zenith_NavMesh::ResetQueryCountForTest();
	if (Zenith_NavMesh::GetQueryCountForTest() != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0NavMesh: Reset didn't clear (counter=%u)",
			Zenith_NavMesh::GetQueryCountForTest());
		++g_iFailures;
	}

	return g_iFailures == 0;
}

static const Zenith_AutomatedTest g_xNavMeshQueryCountTest = {
	"Test_T0NavMesh_QueryCountIncrements",
	&Setup_T0NavMesh_QueryCountIncrements,
	&Step_T0NavMesh_QueryCountIncrements,
	&Verify_T0NavMesh_QueryCountIncrements,
	10,
	false // m_bRequiresGraphics: false -- pure pathfinding-API exercise
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xNavMeshQueryCountTest);

#endif // ZENITH_INPUT_SIMULATOR
