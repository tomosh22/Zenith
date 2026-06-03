#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Navigation/Zenith_Pathfinding.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1NavMesh_PathRespectsWalls (MVP-1.2.0 / 1.2.1)
//
// Builds a tiny scene with a floor + a single wall box collider, calls
// Zenith_NavMeshGenerator::GenerateFromScene, and asserts that
// Zenith_Pathfinding::FindPath routes a path AROUND the wall (not through).
//
// Layout (top-down, y up, +x right, +z forward):
//
//   z =  +4 .... Goal at (0, 0.1, +3)
//        .
//   z =   0 ............[--Wall--]............ Wall along x in [-2.5, +2.5],
//        .                                     z in [-0.3, +0.3], top y=1.0.
//   z =  -4 .... Start at (0, 0.1, -3)
//
// The fix that makes this test pass landed in the same commit:
//   1. NavMeshGenerator now emits all 6 faces of each box collider, not
//      just the top face. Non-walkable faces (sides + bottom) get marked
//      as OBSTRUCTION spans -- which trip the clearance check above any
//      walkable span beneath them, carving the floor under wall footprints.
//   2. Pathfinding's SmoothPath now checks `SegmentExitsNavMesh` in
//      addition to the existing geometric Raycast + IsBlocked probes.
//      The Raycast misses the carved-out region (it's a ray-vs-polygon-
//      plane test that finds nothing in the gap); the new sample-based
//      probe catches the hole.
// ============================================================================

namespace
{
	enum Phase : int { kNW_Start, kNW_BuildScene, kNW_Generate, kNW_FindPath,
	                   kNW_Verify, kNW_Done };

	int                   g_iPhase = kNW_Start;
	Zenith_Scene          g_xScene;
	Zenith_NavMesh*       g_pxNavMesh = nullptr;
	Zenith_PathResult     g_xPath;
	float                 g_fPathLength = 0.0f;
	float                 g_fMaxLateralAbsX = 0.0f;
	bool                  g_bGenerateSucceeded = false;
	bool                  g_bPathSucceeded = false;
	bool                  g_bPassed = false;

	constexpr float kWALL_HALF_X      = 2.5f;
	constexpr float kSTRAIGHT_LINE_M  = 6.0f;  // (0,0.1,-3) -> (0,0.1,+3)

	void CreateStaticBox(Zenith_SceneData* pxScene, const char* szName,
	                     const Zenith_Maths::Vector3& xPos,
	                     const Zenith_Maths::Vector3& xScale)
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().CreateEntity(pxScene, szName);
		Zenith_TransformComponent& xT = xEnt.GetComponent<Zenith_TransformComponent>();
		xT.SetPosition(xPos);
		xT.SetScale(xScale);
		Zenith_ColliderComponent& xCol = xEnt.AddComponent<Zenith_ColliderComponent>();
		xCol.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
	}
}

static void Setup_P1NavMeshPathRespectsWalls()
{
	g_iPhase = kNW_Start;
	g_xScene = Zenith_Scene();
	g_pxNavMesh = nullptr;
	g_xPath = Zenith_PathResult();
	g_fPathLength = 0.0f;
	g_fMaxLateralAbsX = 0.0f;
	g_bGenerateSucceeded = false;
	g_bPathSucceeded = false;
	g_bPassed = false;
}

static bool Step_P1NavMeshPathRespectsWalls(int iFrame)
{
	switch (g_iPhase)
	{
	case kNW_Start:
		g_xScene = g_xEngine.Scenes().LoadScene("NavMeshWallSpike", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(g_xScene);
		g_iPhase = kNW_BuildScene;
		return true;

	case kNW_BuildScene:
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(g_xScene);
		if (pxScene == nullptr) { g_iPhase = kNW_Done; return false; }

		// Scale = full box size (half-extents = scale * 0.5).
		// Floor: 12m x 12m, top surface y=0.1.
		CreateStaticBox(pxScene, "Floor",
		                Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		                Zenith_Maths::Vector3(12.0f, 0.2f, 12.0f));
		// Wall: 5m wide along x, 1m tall, 0.6m thick in z. Top y=1.0 sits
		// inside the agent's 1.8m clearance zone above the floor at y=0.1
		// -- the wall's interior obstruction spans block the clearance
		// check and the floor cells under the wall become non-walkable.
		CreateStaticBox(pxScene, "Wall",
		                Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f),
		                Zenith_Maths::Vector3(2.0f * kWALL_HALF_X, 1.0f, 0.6f));
		g_iPhase = kNW_Generate;
		return true;
	}

	case kNW_Generate:
	{
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(g_xScene);
		if (pxScene == nullptr) { g_iPhase = kNW_Done; return false; }
		NavMeshGenerationConfig xConfig{}; // engine defaults
		g_pxNavMesh = Zenith_NavMeshGenerator::GenerateFromScene(*pxScene, xConfig);
		g_bGenerateSucceeded = (g_pxNavMesh != nullptr);
		std::printf("[P1NavMeshWalls] GenerateFromScene returned %s (frame %d)\n",
			g_bGenerateSucceeded ? "non-null" : "NULL", iFrame);
		std::fflush(stdout);
		g_iPhase = g_bGenerateSucceeded ? kNW_FindPath : kNW_Verify;
		return true;
	}

	case kNW_FindPath:
	{
		const Zenith_Maths::Vector3 xStart(0.0f, 0.0f, -3.0f);
		const Zenith_Maths::Vector3 xEnd  (0.0f, 0.0f, +3.0f);
		g_xPath = Zenith_Pathfinding::FindPath(*g_pxNavMesh, xStart, xEnd);
		g_bPathSucceeded = (g_xPath.m_eStatus == Zenith_PathResult::Status::SUCCESS);

		g_fPathLength = 0.0f;
		g_fMaxLateralAbsX = 0.0f;
		const uint32_t uWp = g_xPath.m_axWaypoints.GetSize();
		for (uint32_t u = 0; u < uWp; ++u)
		{
			const Zenith_Maths::Vector3& xWp = g_xPath.m_axWaypoints.Get(u);
			if (std::fabs(xWp.x) > g_fMaxLateralAbsX) g_fMaxLateralAbsX = std::fabs(xWp.x);
			if (u > 0)
			{
				const Zenith_Maths::Vector3& xPrev = g_xPath.m_axWaypoints.Get(u - 1);
				const float fDx = xWp.x - xPrev.x;
				const float fDy = xWp.y - xPrev.y;
				const float fDz = xWp.z - xPrev.z;
				g_fPathLength += std::sqrt(fDx * fDx + fDy * fDy + fDz * fDz);
			}
		}
		std::printf("[P1NavMeshWalls] FindPath: status=%d waypoints=%u length=%.3f maxAbsX=%.3f\n",
			(int)g_xPath.m_eStatus, uWp, g_fPathLength, g_fMaxLateralAbsX);
		for (uint32_t u = 0; u < uWp; ++u)
		{
			const Zenith_Maths::Vector3& xWp = g_xPath.m_axWaypoints.Get(u);
			std::printf("[P1NavMeshWalls]   waypoint[%u] = (%.3f, %.3f, %.3f)\n",
				u, xWp.x, xWp.y, xWp.z);
		}
		std::fflush(stdout);
		g_iPhase = kNW_Verify;
		return true;
	}

	case kNW_Verify:
	{
		// Required lateral detour: at least wall_half_width - agent_radius - slack
		// = 2.5 - 0.4 - 0.5 = 1.6m.
		const float kREQUIRED_LATERAL = kWALL_HALF_X - 0.4f - 0.5f;
		const bool bDetours       = g_fMaxLateralAbsX >= kREQUIRED_LATERAL;
		// "Not straight" = path length exceeded the direct line.
		const bool bNotStraight   = g_fPathLength > kSTRAIGHT_LINE_M + 0.5f;

		g_bPassed = g_bGenerateSucceeded && g_bPathSucceeded && bDetours && bNotStraight;
		std::printf("[P1NavMeshWalls] verify: gen=%d path=%d detours=%d notStraight=%d "
		            "(maxAbsX=%.3f needed>=%.3f, length=%.3f needed>=%.3f) passed=%d\n",
			(int)g_bGenerateSucceeded, (int)g_bPathSucceeded,
			(int)bDetours, (int)bNotStraight,
			g_fMaxLateralAbsX, kREQUIRED_LATERAL,
			g_fPathLength, kSTRAIGHT_LINE_M + 0.5f, (int)g_bPassed);
		std::fflush(stdout);

		if (g_pxNavMesh != nullptr) { delete g_pxNavMesh; g_pxNavMesh = nullptr; }
		g_iPhase = kNW_Done;
		return false;
	}

	case kNW_Done:
	default:
		return false;
	}
}

static bool Verify_P1NavMeshPathRespectsWalls()
{
	return g_bPassed;
}

static const Zenith_AutomatedTest g_xP1NavMeshWallsTest = {
	"Test_P1NavMesh_PathRespectsWalls",
	&Setup_P1NavMeshPathRespectsWalls,
	&Step_P1NavMeshPathRespectsWalls,
	&Verify_P1NavMeshPathRespectsWalls,
	60,
	false // m_bRequiresGraphics: pure collider-geometry + navmesh
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1NavMeshWallsTest);

#endif // ZENITH_INPUT_SIMULATOR
