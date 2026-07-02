#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "../Components/DPFogPass_Component.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P2Fog_MemoryRasterizeGrid
//
// Pins the CPU->GPU rasterize path the fog memory texture is built from:
// DP_Fog::ComputeMemoryCellBounds + RasterizeMemoryVisibility +
// GetMemoryStateCounts, against a hand-authored 3-cell memory table.
//
// Procedure (same scene scaffolding as Test_P2Fog_MemoryDimsAfter10s —
// WITHOUT_LOADING scene + hand-awoken DPFogPass_Component):
//   1. Reveal cells (10,20) and (12,20); tick 12 s (both -> VisitedDim).
//   2. Reveal cell (11,22) (age 0 -> VisitedVisible).
//   3. Bounds must be exactly min(10,20) max(12,22).
//   4. State counts must be visible=1 dim=2 hidden=0.
//   5. Rasterize an 8x8 grid at origin (9,19): exactly 3 texels non-zero,
//      at the expected (u,v) slots; the visible cell reads 255; the two dim
//      cells read the same MemoryVisibilityForAge value the production
//      thresholds produce.
//   6. Rasterize a deliberately tiny 2x2 window at origin (11,19): only the
//      one cell inside it is written (window clipping).
//
// What this catches:
//   * X/Z (u/v) transposition in the rasterizer (fog would render rotated).
//   * Origin off-by-one (fog trail offset one metre from where you walked).
//   * Missing memset (stale texels from the previous frame's window).
//   * Bounds not covering all cells (memory silently cropped).
// ============================================================================

namespace
{
	bool g_bRasterPassed = false;
	const char* g_szRasterFailure = "";
	int g_iRasterFailureStep = 0;
}

static void Setup_P2MemoryRasterize()
{
	g_bRasterPassed = false;
	g_szRasterFailure = "";
	g_iRasterFailureStep = 0;

	auto FailAt = [](int iStep, const char* sz)
	{
		g_iRasterFailureStep = iStep;
		g_szRasterFailure = sz;
	};

	Zenith_Scene xScene = g_xEngine.Scenes().LoadScene("MemoryRasterizeTest", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xScene);
	Zenith_Entity xFogEntity = g_xEngine.Scenes().CreateEntity(pxScene, "FogPassEntity");
	xFogEntity.AddComponent<DPFogPass_Component>().OnAwake();

	DP_Fog::ClearAllMemoryReveals();

	// Step 1-2: author the 3-cell table. Positions chosen inside the cells'
	// interiors so grid snapping is unambiguous.
	DP_Fog::RecordMemoryReveal(Zenith_Maths::Vector3(10.2f, 0.0f, 20.7f)); // cell (10,20)
	DP_Fog::RecordMemoryReveal(Zenith_Maths::Vector3(12.9f, 0.0f, 20.1f)); // cell (12,20)
	DP_Fog::TickMemoryFog(12.0f);                                          // both now dim (10 < 12 <= 30)
	DP_Fog::RecordMemoryReveal(Zenith_Maths::Vector3(11.5f, 0.0f, 22.3f)); // cell (11,22), age 0

	// Step 3: bounds.
	int32_t iMinX = 0, iMinZ = 0, iMaxX = 0, iMaxZ = 0;
	if (!DP_Fog::ComputeMemoryCellBounds(iMinX, iMinZ, iMaxX, iMaxZ))
	{
		FailAt(3, "ComputeMemoryCellBounds returned false with 3 cells revealed");
		g_xEngine.Scenes().UnloadScene(xScene);
		return;
	}
	if (iMinX != 10 || iMinZ != 20 || iMaxX != 12 || iMaxZ != 22)
	{
		FailAt(3, "bounds mismatch (expected min(10,20) max(12,22))");
		g_xEngine.Scenes().UnloadScene(xScene);
		return;
	}

	// Step 4: state counts.
	uint32_t uVisible = 0, uDim = 0, uHidden = 0;
	DP_Fog::GetMemoryStateCounts(uVisible, uDim, uHidden);
	if (uVisible != 1 || uDim != 2 || uHidden != 0)
	{
		FailAt(4, "state counts mismatch (expected visible=1 dim=2 hidden=0)");
		g_xEngine.Scenes().UnloadScene(xScene);
		return;
	}

	// Step 5: rasterize 8x8 at origin (9,19). Poison the buffer first so a
	// missing memset in the rasterizer is caught.
	uint8_t auGrid[8 * 8];
	std::memset(auGrid, 0xCD, sizeof(auGrid));
	DP_Fog::MemoryGrid xGrid;
	xGrid.m_iOriginCellX = 9;
	xGrid.m_iOriginCellZ = 19;
	xGrid.m_uSize = 8;
	const uint32_t uWritten = DP_Fog::RasterizeMemoryVisibility(auGrid, xGrid);
	if (uWritten != 3)
	{
		FailAt(5, "expected exactly 3 cells written to the 8x8 window");
		g_xEngine.Scenes().UnloadScene(xScene);
		return;
	}

	// Expected texels: (u,v) = (cellX-9, cellZ-19).
	const uint8_t uDimExpected = DP_Fog::MemoryVisibilityForAge(12.0f,
		DP_Tuning::Get<float>("fog_of_war.memory_visible_s"),
		DP_Tuning::Get<float>("fog_of_war.memory_dim_s"),
		DP_Tuning::Get<float>("fog_of_war.memory_dim_visibility"),
		DP_Tuning::Get<float>("fog_of_war.memory_hidden_fade_s"));
	if (uDimExpected == 0 || uDimExpected == 255)
	{
		FailAt(5, "dim reference value degenerate (tuning thresholds unusable)");
		g_xEngine.Scenes().UnloadScene(xScene);
		return;
	}
	uint32_t uNonZero = 0;
	for (uint32_t v = 0; v < 8; ++v)
	{
		for (uint32_t u = 0; u < 8; ++u)
		{
			if (auGrid[v * 8 + u] != 0) ++uNonZero;
		}
	}
	if (uNonZero != 3)
	{
		FailAt(5, "expected exactly 3 non-zero texels (memset or splat bug)");
		g_xEngine.Scenes().UnloadScene(xScene);
		return;
	}
	if (auGrid[1 * 8 + 1] != uDimExpected ||   // cell (10,20)
	    auGrid[1 * 8 + 3] != uDimExpected ||   // cell (12,20)
	    auGrid[3 * 8 + 2] != 255)              // cell (11,22)
	{
		FailAt(5, "texel placement/value mismatch (u/v transposition or origin bug)");
		g_xEngine.Scenes().UnloadScene(xScene);
		return;
	}

	// Step 6: window clipping — 2x2 at origin (11,19) contains only cell
	// (12,20) of the three.
	uint8_t auSmall[2 * 2];
	DP_Fog::MemoryGrid xSmall;
	xSmall.m_iOriginCellX = 11;
	xSmall.m_iOriginCellZ = 19;
	xSmall.m_uSize = 2;
	const uint32_t uSmallWritten = DP_Fog::RasterizeMemoryVisibility(auSmall, xSmall);
	if (uSmallWritten != 1 || auSmall[1 * 2 + 1] != uDimExpected)
	{
		FailAt(6, "window clipping mismatch (expected only cell (12,20) at (1,1))");
		g_xEngine.Scenes().UnloadScene(xScene);
		return;
	}

	g_bRasterPassed = true;
	std::printf("[P2MemoryRasterize] all 6 steps passed (dim value=%u)\n", uDimExpected);
	std::fflush(stdout);

	g_xEngine.Scenes().UnloadScene(xScene);
}

static bool Step_P2MemoryRasterize(int /*iFrame*/)
{
	return false; // all work happens in Setup
}

static bool Verify_P2MemoryRasterize()
{
	if (!g_bRasterPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P2MemoryRasterize: step %d failed -- %s",
			g_iRasterFailureStep, g_szRasterFailure);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP2MemoryRasterizeTest = {
	"Test_P2Fog_MemoryRasterizeGrid",
	&Setup_P2MemoryRasterize,
	&Step_P2MemoryRasterize,
	&Verify_P2MemoryRasterize,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP2MemoryRasterizeTest);

#endif // ZENITH_INPUT_SIMULATOR
