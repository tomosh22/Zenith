#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "Profiling/Zenith_Profiling.h"

#include <chrono>
#include <cstdio>
#include <cmath>

// ============================================================================
// Test_T1NavMesh_GeneratorPerfOnGameLevel (diagnostic)
//
// Loads GameLevel, scans every static collider for its world-space AABB,
// then calls Zenith_NavMeshGenerator::GenerateFromScene under the
// profiling system and dumps a per-stage report.
//
// Surfaces:
//   * The outlier collider(s) stretching the navmesh bounding box.
//   * Which pipeline stage (voxelize / filter / regions / contours /
//     poly mesh / final assembly) dominates the wall-clock budget.
//
// Pass criterion: GenerateFromScene returns a non-null navmesh. The
// timing data is for triage, not yet for enforcing a perf budget.
// ============================================================================

namespace
{
	enum Phase : int { kGP_Start, kGP_WaitScene, kGP_Dump, kGP_Generate, kGP_Done };

	int   g_iPhase = kGP_Start;
	bool  g_bPassed = false;
}

static void Setup_T1NavMeshGeneratorPerf()
{
	g_iPhase = kGP_Start;
	g_bPassed = false;
	Zenith_Profiling::ClearEvents();
}

static bool Step_T1NavMeshGeneratorPerf(int iFrame)
{
	switch (g_iPhase)
	{
	case kGP_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kGP_WaitScene;
		return true;

	case kGP_WaitScene:
		if (iFrame < 10) return true;
		g_iPhase = kGP_Dump;
		return true;

	case kGP_Dump:
	{
		// Scan every entity with a static collider, log its AABB. The goal
		// is to find outliers that stretch the navmesh bounding box.
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kGP_Done; return false; }

		const Zenith_Vector<Zenith_EntityID>& axIds = pxScene->GetActiveEntities();
		uint32_t uColliderCount = 0;
		float fMinX = 1e30f, fMinY = 1e30f, fMinZ = 1e30f;
		float fMaxX = -1e30f, fMaxY = -1e30f, fMaxZ = -1e30f;

		std::printf("[T1NavMeshPerf] Scanning %u entities for static colliders...\n",
			axIds.GetSize());

		for (uint32_t u = 0; u < axIds.GetSize(); ++u)
		{
			Zenith_Entity xEnt = pxScene->TryGetEntity(axIds.Get(u));
			if (!xEnt.IsValid()) continue;
			if (!xEnt.HasComponent<Zenith_ColliderComponent>()) continue;
			Zenith_ColliderComponent& xCol = xEnt.GetComponent<Zenith_ColliderComponent>();
			if (xCol.GetRigidBodyType() != RIGIDBODY_TYPE_STATIC) continue;
			if (!xEnt.HasComponent<Zenith_TransformComponent>()) continue;

			++uColliderCount;
			Zenith_TransformComponent& xT = xEnt.GetComponent<Zenith_TransformComponent>();
			Zenith_Maths::Vector3 xPos, xScl;
			xT.GetPosition(xPos);
			xT.GetScale(xScl);

			const float fHalfX = std::fabs(xScl.x) * 0.5f;
			const float fHalfY = std::fabs(xScl.y) * 0.5f;
			const float fHalfZ = std::fabs(xScl.z) * 0.5f;
			const float fEntMinX = xPos.x - fHalfX, fEntMaxX = xPos.x + fHalfX;
			const float fEntMinY = xPos.y - fHalfY, fEntMaxY = xPos.y + fHalfY;
			const float fEntMinZ = xPos.z - fHalfZ, fEntMaxZ = xPos.z + fHalfZ;

			fMinX = std::min(fMinX, fEntMinX); fMaxX = std::max(fMaxX, fEntMaxX);
			fMinY = std::min(fMinY, fEntMinY); fMaxY = std::max(fMaxY, fEntMaxY);
			fMinZ = std::min(fMinZ, fEntMinZ); fMaxZ = std::max(fMaxZ, fEntMaxZ);

			// Print colliders with extreme bounds (likely outliers).
			const bool bExtremeXorZ = std::fabs(xPos.x) > 200.0f || std::fabs(xPos.z) > 200.0f;
			const bool bExtremeY    = std::fabs(xPos.y) > 50.0f;
			const bool bExtremeSize = fHalfX > 50.0f || fHalfY > 50.0f || fHalfZ > 50.0f;
			if (bExtremeXorZ || bExtremeY || bExtremeSize)
			{
				const std::string& strName = xEnt.GetName();
				std::printf("[T1NavMeshPerf] OUTLIER name='%s' pos=(%.1f, %.1f, %.1f) "
				            "halfExt=(%.1f, %.1f, %.1f)\n",
					strName.c_str(), xPos.x, xPos.y, xPos.z, fHalfX, fHalfY, fHalfZ);
			}
		}

		std::printf("[T1NavMeshPerf] %u static colliders. Scene AABB: "
		            "x=[%.1f, %.1f] y=[%.1f, %.1f] z=[%.1f, %.1f] -> %.1fm x %.1fm x %.1fm\n",
			uColliderCount,
			fMinX, fMaxX, fMinY, fMaxY, fMinZ, fMaxZ,
			fMaxX - fMinX, fMaxY - fMinY, fMaxZ - fMinZ);
		std::fflush(stdout);
		g_iPhase = kGP_Generate;
		return true;
	}

	case kGP_Generate:
	{
		Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xScene);
		if (pxScene == nullptr) { g_iPhase = kGP_Done; return false; }

		NavMeshGenerationConfig xCfg{};
		const auto xStart = std::chrono::high_resolution_clock::now();
		Zenith_NavMesh* pxNavMesh = Zenith_NavMeshGenerator::GenerateFromScene(*pxScene, xCfg);
		const auto xEnd = std::chrono::high_resolution_clock::now();

		const std::chrono::duration<float, std::milli> xMs = xEnd - xStart;
		const uint32_t uPolyCount = (pxNavMesh != nullptr) ? pxNavMesh->GetPolygonCount() : 0;

		std::printf("[T1NavMeshPerf] GenerateFromScene on GameLevel: %.2f ms, %u polygons\n",
			xMs.count(), uPolyCount);
		std::printf("[T1NavMeshPerf] ---- profiling report ----\n");
		std::fflush(stdout);
		Zenith_Profiling::WriteTextReport(stdout);
		std::printf("[T1NavMeshPerf] ---- end profiling ----\n");
		std::fflush(stdout);

		g_bPassed = (pxNavMesh != nullptr);
		delete pxNavMesh;
		g_iPhase = kGP_Done;
		return false;
	}

	case kGP_Done:
	default:
		return false;
	}
}

static bool Verify_T1NavMeshGeneratorPerf()
{
	return g_bPassed;
}

static const Zenith_AutomatedTest g_xT1NavMeshGeneratorPerfTest = {
	"Test_T1NavMesh_GeneratorPerfOnGameLevel",
	&Setup_T1NavMeshGeneratorPerf,
	&Step_T1NavMeshGeneratorPerf,
	&Verify_T1NavMeshGeneratorPerf,
	600,
	false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xT1NavMeshGeneratorPerfTest);

#endif // ZENITH_INPUT_SIMULATOR
