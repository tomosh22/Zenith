#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Source/DP_LevelData.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

// ============================================================================
// BuildingWallClosure_Test
//
// Regression for a UE→Zenith coordinate-conversion bug. The modular wall kit
// (BuildingAssetKit) uses UE_X as the wall's *length* axis: a wall mesh is
// 2 m natural along UE_X, and source actors stretch their length via
// scale[0] (the UE_X scale factor). The mesh-import path lays the wall's
// long axis on Zenith_X (matches glTF axis convention).
//
// The original Tools/dp_import/generate_level_data.py mapped UE.X → Zenith.Z
// for *positions* but kept UE.X → ZenithX for the mesh, so source scale[0]
// (UE_X length) ended up on Zenith.sz (the wall's depth axis). Walls came
// out 1 m long instead of 6 m, and corners never met — the village
// looked like scattered fence posts instead of buildings.
//
// We pick a known corner-wall-corner triplet from L_GameLevel
// (the 3-piece wall at UE.Y = 6000):
//
//   CornerWallSection  UE(7200, 6000) yaw=-90°  scale=(1,1,1) → Zenith(72,1,60)
//   WallSection2       UE(7600, 6000) yaw=180°  scale=(3,1,1) → Zenith(76,1,60) sx=3
//   CornerWallSection2 UE(8000, 6000) yaw=-180° scale=(1,1,1) → Zenith(80,1,60)
//
// The test verifies (a) the entities are at the expected Zenith positions
// (UE.X → Zenith.X straight mapping), and (b) the wall's scale is on the
// X axis (mesh-local length), not the Z axis (mesh-local depth). If the
// python regresses to the broken UE.X → Zenith.Z swap, the wall's scale
// falls on sz and the wall's mesh-local X stops aligning with the
// building run direction — observable here.
// ============================================================================

namespace
{
	enum Phase : int { kStart, kWaitForLoad, kProbe, kDone };
	int  g_iPhase = kStart;
	int  g_iWait  = 0;
	bool g_bFoundAll       = false;
	bool g_bPositionsMatch = false;
	bool g_bScaleAxisOK    = false;

	// Index of each piece inside DP_LevelData::kStaticDeco. The first 5
	// entries are the floor + 4 outer-perimeter walls; the modular kit
	// starts at index 6 with the south-wall corner of the first building.
	constexpr uint32_t kCorner1Idx = 6;
	constexpr uint32_t kWallIdx    = 7;
	constexpr uint32_t kCorner2Idx = 8;

	struct Expected
	{
		float fX, fY, fZ;
		float fSx, fSy, fSz;
		const char* szLabel;
	};

	// What the conversion should produce. If these drift, either the
	// python position swap or the scale swap has regressed (or both).
	//
	// Y=1.0 matches the UE source pos.y for these wall placements. With
	// the mesh-aware OBB shape in Zenith_ColliderComponent::CreateBoxShape
	// the visual + collider bounds are derived from the mesh's own bounds
	// (y_min=0, y_max=4) plus a scale-applied offset, so the original
	// bShiftYByHalfScale=true lift is no longer needed — entity Y stays
	// at the source value and the wall still sits flush above the floor.
	constexpr Expected g_axExpected[3] = {
		{ 72.0f, 1.0f, 60.0f, 1.0f, 1.0f, 1.0f, "CornerWallSection"  },
		{ 76.0f, 1.0f, 60.0f, 3.0f, 1.0f, 1.0f, "WallSection2"       },
		{ 80.0f, 1.0f, 60.0f, 1.0f, 1.0f, 1.0f, "CornerWallSection2" },
	};

	bool FloatNear(float fA, float fB, float fEps = 0.05f)
	{
		return std::fabs(fA - fB) < fEps;
	}
}

static void Setup_BuildingWallClosure()
{
	g_iPhase = kStart;
	g_iWait  = 0;
	g_bFoundAll       = false;
	g_bPositionsMatch = false;
	g_bScaleAxisOK    = false;
}

static bool Step_BuildingWallClosure(int /*iFrame*/)
{
	switch (g_iPhase)
	{
	case kStart:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWaitForLoad;
		return true;

	case kWaitForLoad:
	{
		++g_iWait;
		Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xActive);
		if (pxScene != nullptr)
		{
			// The first StaticDeco entity (the floor) tells us the batch
			// has been authored. AuthorPlacementBatch enqueues steps in
			// order, so by the time we see StaticDeco_0 the corner-wall-
			// corner triplet at indices 6/7/8 should already be live.
			Zenith_Entity xEnt = pxScene->FindEntityByName("StaticDeco_0");
			if (xEnt.IsValid())
			{
				g_iPhase = kProbe;
				return true;
			}
		}
		if (g_iWait > 60) { g_iPhase = kDone; return false; }
		return true;
	}

	case kProbe:
	{
		Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xActive);
		if (pxScene == nullptr) { g_iPhase = kDone; return false; }

		const uint32_t auIdx[3] = { kCorner1Idx, kWallIdx, kCorner2Idx };

		bool bFoundAll       = true;
		bool bPositionsMatch = true;
		bool bScaleAxisOK    = true;

		for (int i = 0; i < 3; ++i)
		{
			char szName[32];
			std::snprintf(szName, sizeof(szName), "StaticDeco_%u", auIdx[i]);
			Zenith_Entity xEnt = pxScene->FindEntityByName(szName);
			if (!xEnt.IsValid()) { bFoundAll = false; continue; }
			if (!xEnt.HasComponent<Zenith_TransformComponent>()) { bFoundAll = false; continue; }

			Zenith_TransformComponent& xT = xEnt.GetComponent<Zenith_TransformComponent>();
			Zenith_Maths::Vector3 xPos;  xT.GetPosition(xPos);
			Zenith_Maths::Vector3 xScale; xT.GetScale(xScale);

			const Expected& xE = g_axExpected[i];
			if (!FloatNear(xPos.x, xE.fX) ||
				!FloatNear(xPos.y, xE.fY) ||
				!FloatNear(xPos.z, xE.fZ))
			{
				std::printf("[BuildingWallClosure] %s pos=(%.2f, %.2f, %.2f) expected=(%.2f, %.2f, %.2f)\n",
					xE.szLabel, xPos.x, xPos.y, xPos.z, xE.fX, xE.fY, xE.fZ);
				bPositionsMatch = false;
			}
			if (!FloatNear(xScale.x, xE.fSx) ||
				!FloatNear(xScale.y, xE.fSy) ||
				!FloatNear(xScale.z, xE.fSz))
			{
				std::printf("[BuildingWallClosure] %s scale=(%.2f, %.2f, %.2f) expected=(%.2f, %.2f, %.2f)\n",
					xE.szLabel, xScale.x, xScale.y, xScale.z, xE.fSx, xE.fSy, xE.fSz);
				bScaleAxisOK = false;
			}
		}

		g_bFoundAll       = bFoundAll;
		g_bPositionsMatch = bPositionsMatch;
		g_bScaleAxisOK    = bScaleAxisOK;
		std::fflush(stdout);

		g_iPhase = kDone;
		return false;
	}

	case kDone:
	default:
		return false;
	}
}

static bool Verify_BuildingWallClosure()
{
	return g_bFoundAll && g_bPositionsMatch && g_bScaleAxisOK;
}

static const Zenith_AutomatedTest g_xBuildingWallClosureTest = {
	"BuildingWallClosure_Test",
	&Setup_BuildingWallClosure,
	&Step_BuildingWallClosure,
	&Verify_BuildingWallClosure,
	120
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xBuildingWallClosureTest);

#endif // ZENITH_INPUT_SIMULATOR
