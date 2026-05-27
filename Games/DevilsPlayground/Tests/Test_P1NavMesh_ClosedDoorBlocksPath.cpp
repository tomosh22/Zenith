#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Navigation/Zenith_Pathfinding.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1NavMesh_ClosedDoorBlocksPath (MVP-1.2.3)
//
// Two-room scene separated by a wall with a single door-width gap. Asserts
// that toggling the doorway polygon's BLOCKED flag via Zenith_NavMesh::
// SetBlockedAtPoint (the runtime-blocking API DPDoor_Behaviour drives via
// SyncNavMeshBlock) actually changes Zenith_Pathfinding::FindPath's verdict:
//
//   * Door OPEN (no blocked polygons)    -> FindPath SUCCESS
//   * Door CLOSED (gap polygon blocked)  -> FindPath FAILED / PARTIAL
//   * Door reopened (gap unblocked)      -> FindPath SUCCESS again
//
// This is the engine-side contract DPDoor_Behaviour::SyncNavMeshBlock relies
// on; it landed in earlier engine work (PR #28 et al.) and is exercised
// indirectly by every GameLevel path-through-door scenario. The unit test
// pins the contract so future navmesh refactors can't silently break it
// without surfacing here.
//
// Layout (top-down, +x right, +z forward; agent radius 0.4m default):
//
//   z = +4 .... Goal at (0, 0.0, +3)
//        .
//   z =  0 .[WW][WW]  GAP  [WW][WW]   Two wall segments left + right of x=0
//        .                            with a 2m wide gap centred on x=0.
//   z = -4 .... Start at (0, 0.0, -3)
//
// Why two scenarios run in sequence (open -> closed -> open):
//   * "open then closed" alone could be passed by an implementation that
//     poisons the navmesh on first SetBlockedAtPoint(true).
//   * "closed then open" alone could be passed by an implementation that
//     resets blocked state on every FindPath call.
// Cycling proves the flag is honoured per-query and that unblocking
// genuinely reverses the effect.
// ============================================================================

namespace
{
	enum Phase : int { kCD_Start, kCD_BuildScene, kCD_Generate,
	                   kCD_PathOpenInitial, kCD_BlockAndPathClosed,
	                   kCD_UnblockAndPathOpen, kCD_Verify, kCD_Done };

	int                   g_iPhase = kCD_Start;
	Zenith_Scene          g_xScene;
	Zenith_NavMesh*       g_pxNavMesh = nullptr;
	bool                  g_bGenerateOK = false;
	bool                  g_bOpenInitialPathOK = false;
	bool                  g_bClosedPathBlocked = false;
	bool                  g_bReopenedPathOK = false;
	bool                  g_bPassed = false;

	// Door position (centre of gap) — used for SetBlockedAtPoint and as the
	// hint for distinguishing "blocked" from "no polygon found at all".
	constexpr float kDOOR_X = 0.0f;
	constexpr float kDOOR_Z = 0.0f;
	constexpr float kDOOR_Y = 0.0f;

	void CreateStaticBox(Zenith_SceneData* pxScene, const char* szName,
	                     const Zenith_Maths::Vector3& xPos,
	                     const Zenith_Maths::Vector3& xScale)
	{
		Zenith_Entity xEnt(pxScene, szName);
		Zenith_TransformComponent& xT = xEnt.GetComponent<Zenith_TransformComponent>();
		xT.SetPosition(xPos);
		xT.SetScale(xScale);
		Zenith_ColliderComponent& xCol = xEnt.AddComponent<Zenith_ColliderComponent>();
		xCol.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
	}
}

static void Setup_P1NavMeshClosedDoorBlocksPath()
{
	g_iPhase = kCD_Start;
	g_xScene = Zenith_Scene();
	delete g_pxNavMesh;
	g_pxNavMesh = nullptr;
	g_bGenerateOK = false;
	g_bOpenInitialPathOK = false;
	g_bClosedPathBlocked = false;
	g_bReopenedPathOK = false;
	g_bPassed = false;
}

static bool Step_P1NavMeshClosedDoorBlocksPath(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	case kCD_Start:
		g_xScene = g_xEngine.SceneRegistry().CreateEmptyScene("NavMeshClosedDoorTest");
		g_xEngine.SceneRegistry().SetActiveScene(g_xScene);
		g_iPhase = kCD_BuildScene;
		return true;

	case kCD_BuildScene:
	{
		Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneData(g_xScene);
		if (pxScene == nullptr) { g_iPhase = kCD_Done; return false; }

		// Floor: 11m wide × 12m deep, top surface y=0.1. Width is matched
		// to the combined wall span so A* CANNOT route around the walls'
		// end caps — the floor terminates exactly where the walls do, and
		// the agent's 0.4 m radius keeps it off the navmesh edge. The
		// doorway is then the ONLY connection between the south and
		// north halves; "closed door -> path fails" becomes a real
		// assertion rather than a "let A* find a long detour" test.
		CreateStaticBox(pxScene, "Floor",
		                Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		                Zenith_Maths::Vector3(11.0f, 0.2f, 12.0f));

		// Left wall segment: x = [-5.5, -0.5], z = [-0.3, +0.3], top y = 1.0.
		// Centre x = -3.0, length x = 5.0m, length z = 0.6m, height y = 1.0m.
		// Spans the full -X half of the floor -- no detour space at the end.
		CreateStaticBox(pxScene, "WallLeft",
		                Zenith_Maths::Vector3(-3.0f, 0.5f, 0.0f),
		                Zenith_Maths::Vector3(5.0f, 1.0f, 0.6f));
		// Right wall segment: x = [+0.5, +5.5], mirrored.
		CreateStaticBox(pxScene, "WallRight",
		                Zenith_Maths::Vector3(+3.0f, 0.5f, 0.0f),
		                Zenith_Maths::Vector3(5.0f, 1.0f, 0.6f));
		// Gap: x in [-0.5, +0.5] (1 m wide) at z=0 is open in the geometry.
		// With agent radius 0.4 m the walkable corridor is ~0.2 m wide.
		g_iPhase = kCD_Generate;
		return true;
	}

	case kCD_Generate:
	{
		Zenith_SceneData* pxScene = g_xEngine.SceneRegistry().GetSceneData(g_xScene);
		if (pxScene == nullptr) { g_iPhase = kCD_Done; return false; }
		NavMeshGenerationConfig xCfg{}; // engine defaults match production
		g_pxNavMesh = Zenith_NavMeshGenerator::GenerateFromScene(*pxScene, xCfg);
		g_bGenerateOK = (g_pxNavMesh != nullptr) && (g_pxNavMesh->GetPolygonCount() > 0);
		std::printf("[P1NavMeshDoor] GenerateFromScene: %s (polys=%u)\n",
			g_bGenerateOK ? "OK" : "FAIL",
			g_pxNavMesh ? g_pxNavMesh->GetPolygonCount() : 0u);
		std::fflush(stdout);
		g_iPhase = g_bGenerateOK ? kCD_PathOpenInitial : kCD_Verify;
		return true;
	}

	case kCD_PathOpenInitial:
	{
		// Door OPEN by default (no polygon blocked yet). Path should route
		// through the gap.
		const Zenith_Maths::Vector3 xStart(0.0f, 0.0f, -3.0f);
		const Zenith_Maths::Vector3 xEnd  (0.0f, 0.0f, +3.0f);
		const Zenith_PathResult xR =
			Zenith_Pathfinding::FindPath(*g_pxNavMesh, xStart, xEnd);
		g_bOpenInitialPathOK = (xR.m_eStatus == Zenith_PathResult::Status::SUCCESS)
		                   && (xR.m_axWaypoints.GetSize() >= 2);
		std::printf("[P1NavMeshDoor] open(initial) FindPath: status=%d waypoints=%u dist=%.2f\n",
			(int)xR.m_eStatus, xR.m_axWaypoints.GetSize(), xR.m_fTotalDistance);
		std::fflush(stdout);
		g_iPhase = kCD_BlockAndPathClosed;
		return true;
	}

	case kCD_BlockAndPathClosed:
	{
		// Slam the door shut: block every polygon across the doorway's full
		// footprint. SetBlockedAtPoint only marks polygons whose 2D
		// footprint contains the probe point, and at the default 0.3 m
		// cell size a 1 m × 0.6 m doorway contains multiple polygons. We
		// sweep a 5×5 grid (x in [-0.4, +0.4], z in [-0.4, +0.4]) to
		// guarantee every walkable polygon in the doorway and its
		// immediate fringe is marked. DPDoor_Behaviour's production call
		// is a single point at the door's transform pos -- which works
		// only when the doorway is exactly 1 polygon wide; the test
		// geometry uses a wider gap so we have headroom to assert the
		// open case (a 1-polygon doorway is hard to pass an OPEN-case
		// FindPath through reliably with how Recast aligns voxel grids).
		uint32_t uBlocked = 0;
		for (int ix = -2; ix <= 2; ++ix)
		{
			const float fX = kDOOR_X + 0.2f * static_cast<float>(ix);
			for (int iz = -2; iz <= 2; ++iz)
			{
				const float fZ = kDOOR_Z + 0.2f * static_cast<float>(iz);
				uBlocked += g_pxNavMesh->SetBlockedAtPoint(
					Zenith_Maths::Vector3(fX, kDOOR_Y, fZ),
					/*bBlocked=*/true, /*fMaxVerticalDist=*/3.0f);
			}
		}
		std::printf("[P1NavMeshDoor] door CLOSED: SetBlockedAtPoint marked %u polys (5x5 grid)\n",
			uBlocked);

		const Zenith_Maths::Vector3 xStart(0.0f, 0.0f, -3.0f);
		const Zenith_Maths::Vector3 xEnd  (0.0f, 0.0f, +3.0f);
		const Zenith_PathResult xR =
			Zenith_Pathfinding::FindPath(*g_pxNavMesh, xStart, xEnd);
		// "Blocked" semantics: SUCCESS is a hard fail. FAILED or PARTIAL are
		// both acceptable -- FAILED means A* couldn't reach the goal at all,
		// PARTIAL means it stopped at the closest reachable polygon (which
		// in this layout means a polygon on the start side of the wall, not
		// on the goal side). Distinguishing the two requires reading the
		// final waypoint, which is below.
		g_bClosedPathBlocked = (xR.m_eStatus != Zenith_PathResult::Status::SUCCESS);
		std::printf("[P1NavMeshDoor] closed FindPath: status=%d waypoints=%u dist=%.2f -> blocked=%d\n",
			(int)xR.m_eStatus, xR.m_axWaypoints.GetSize(), xR.m_fTotalDistance,
			(int)g_bClosedPathBlocked);
		// Even a PARTIAL path is suspect if its final waypoint is past the
		// wall -- A* should not have been able to step over a blocked
		// polygon. Belt-and-braces check:
		if (xR.m_eStatus == Zenith_PathResult::Status::PARTIAL
			&& xR.m_axWaypoints.GetSize() > 0)
		{
			const Zenith_Maths::Vector3& xLast =
				xR.m_axWaypoints.Get(xR.m_axWaypoints.GetSize() - 1);
			if (xLast.z > 0.5f)
			{
				std::printf("[P1NavMeshDoor]   ... but PARTIAL ended on goal side (z=%.2f) -- treating as fail\n",
					xLast.z);
				g_bClosedPathBlocked = false;
			}
		}
		std::fflush(stdout);
		g_iPhase = kCD_UnblockAndPathOpen;
		return true;
	}

	case kCD_UnblockAndPathOpen:
	{
		// Open the door again: unblock every polygon under the same 5x5
		// grid of probes used to close it.
		uint32_t uUnblocked = 0;
		for (int ix = -2; ix <= 2; ++ix)
		{
			const float fX = kDOOR_X + 0.2f * static_cast<float>(ix);
			for (int iz = -2; iz <= 2; ++iz)
			{
				const float fZ = kDOOR_Z + 0.2f * static_cast<float>(iz);
				uUnblocked += g_pxNavMesh->SetBlockedAtPoint(
					Zenith_Maths::Vector3(fX, kDOOR_Y, fZ),
					/*bBlocked=*/false, /*fMaxVerticalDist=*/3.0f);
			}
		}
		std::printf("[P1NavMeshDoor] door REOPENED: SetBlockedAtPoint unmarked %u polys (5x5 grid)\n",
			uUnblocked);

		const Zenith_Maths::Vector3 xStart(0.0f, 0.0f, -3.0f);
		const Zenith_Maths::Vector3 xEnd  (0.0f, 0.0f, +3.0f);
		const Zenith_PathResult xR =
			Zenith_Pathfinding::FindPath(*g_pxNavMesh, xStart, xEnd);
		g_bReopenedPathOK = (xR.m_eStatus == Zenith_PathResult::Status::SUCCESS)
		                && (xR.m_axWaypoints.GetSize() >= 2);
		std::printf("[P1NavMeshDoor] reopened FindPath: status=%d waypoints=%u dist=%.2f\n",
			(int)xR.m_eStatus, xR.m_axWaypoints.GetSize(), xR.m_fTotalDistance);
		std::fflush(stdout);
		g_iPhase = kCD_Verify;
		return true;
	}

	case kCD_Verify:
		g_bPassed = g_bGenerateOK
		         && g_bOpenInitialPathOK
		         && g_bClosedPathBlocked
		         && g_bReopenedPathOK;
		std::printf("[P1NavMeshDoor] verify: gen=%d openInit=%d closedBlocked=%d reopened=%d passed=%d\n",
			(int)g_bGenerateOK, (int)g_bOpenInitialPathOK,
			(int)g_bClosedPathBlocked, (int)g_bReopenedPathOK,
			(int)g_bPassed);
		std::fflush(stdout);

		delete g_pxNavMesh;
		g_pxNavMesh = nullptr;
		g_iPhase = kCD_Done;
		return false;

	case kCD_Done:
	default:
		return false;
	}
}

static bool Verify_P1NavMeshClosedDoorBlocksPath()
{
	return g_bPassed;
}

static const Zenith_AutomatedTest g_xP1NavMeshClosedDoorTest = {
	"Test_P1NavMesh_ClosedDoorBlocksPath",
	&Setup_P1NavMeshClosedDoorBlocksPath,
	&Step_P1NavMeshClosedDoorBlocksPath,
	&Verify_P1NavMeshClosedDoorBlocksPath,
	60,
	false // m_bRequiresGraphics: pure collider-geometry + navmesh + pathfinding
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1NavMeshClosedDoorTest);

#endif // ZENITH_INPUT_SIMULATOR
