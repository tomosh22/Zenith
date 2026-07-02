#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"

#include "Source/DP_Minimap.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_T0Minimap_PureMaths
//
// Unit-tests the three pure DP_Minimap functions in isolation (no scene):
//
//   1. BuildMapView + WorldToPanel: bounds corners land on the panel edges
//      (respecting padding), the centre lands at the panel centre, and a
//      non-square level letterboxes on the longer axis (aspect preserved).
//   2. RoomPanelRect: axis-aligned rooms map 1:1; a 90°-yawed room swaps
//      its extents; 45° grows the AABB to (hx+hz) on both axes.
//   3. ColourForMemoryState: NeverSeen is fully transparent; visibility 255
//      is fully opaque; the hidden floor keeps once-seen rooms non-zero;
//      alpha is monotonic in visibility.
// ============================================================================

namespace
{
	bool g_bMiniPassed = false;
	const char* g_szMiniWhy = "";

	bool Near(float fA, float fB, float fEps = 0.01f) { return std::fabs(fA - fB) <= fEps; }
}

static void Setup_MinimapPure()
{
	g_bMiniPassed = false;
	g_szMiniWhy = "";

	// ---- 1. Map view + world->panel ----
	// Square bounds 0..100 with 4 m padding on a 220 px panel: covered
	// extent 108 m, so (0,0) world lands 4 m in from the panel corner.
	{
		const DP_Minimap::MapView xView = DP_Minimap::BuildMapView(0.0f, 0.0f, 100.0f, 100.0f, 220.0f, 4.0f);
		const float fScale = 220.0f / 108.0f;
		const Vec2 xOrigin = DP_Minimap::WorldToPanel(xView, 0.0f, 0.0f);
		if (!Near(xOrigin.x, 4.0f * fScale) || !Near(xOrigin.y, 4.0f * fScale))
			{ g_szMiniWhy = "world origin misplaced on panel"; return; }
		const Vec2 xCentre = DP_Minimap::WorldToPanel(xView, 50.0f, 50.0f);
		if (!Near(xCentre.x, 110.0f) || !Near(xCentre.y, 110.0f))
			{ g_szMiniWhy = "bounds centre must map to panel centre"; return; }
		const Vec2 xMax = DP_Minimap::WorldToPanel(xView, 104.0f, 104.0f);
		if (!Near(xMax.x, 220.0f) || !Near(xMax.y, 220.0f))
			{ g_szMiniWhy = "padded max corner must map to panel max"; return; }
	}

	// Non-square bounds: X extent 200, Z extent 100 (+ 2*4 padding). The
	// larger axis (208) sets the scale; Z is centred inside the square
	// coverage, so world Z=50 (the Z mid) still maps to the panel centre Y.
	{
		const DP_Minimap::MapView xView = DP_Minimap::BuildMapView(0.0f, 0.0f, 200.0f, 100.0f, 220.0f, 4.0f);
		const Vec2 xMid = DP_Minimap::WorldToPanel(xView, 100.0f, 50.0f);
		if (!Near(xMid.x, 110.0f) || !Near(xMid.y, 110.0f))
			{ g_szMiniWhy = "non-square bounds must letterbox centred"; return; }
		// A 10 m distance must map to the same pixel run on both axes.
		const Vec2 xA = DP_Minimap::WorldToPanel(xView, 0.0f, 0.0f);
		const Vec2 xB = DP_Minimap::WorldToPanel(xView, 10.0f, 10.0f);
		if (!Near(xB.x - xA.x, xB.y - xA.y))
			{ g_szMiniWhy = "aspect ratio not preserved"; return; }
	}

	// ---- 2. Room AABB footprint ----
	{
		const DP_Minimap::MapView xView = DP_Minimap::BuildMapView(0.0f, 0.0f, 100.0f, 100.0f, 220.0f, 4.0f);
		const float fScale = 220.0f / 108.0f;
		Vec2 xTL, xSize;

		// Axis-aligned 8x4 room.
		DP_Minimap::RoomPanelRect(xView, 50.0f, 50.0f, 4.0f, 2.0f, 0.0f, xTL, xSize);
		if (!Near(xSize.x, 8.0f * fScale) || !Near(xSize.y, 4.0f * fScale))
			{ g_szMiniWhy = "axis-aligned room size wrong"; return; }

		// 90° yaw swaps the extents.
		DP_Minimap::RoomPanelRect(xView, 50.0f, 50.0f, 4.0f, 2.0f, glm::half_pi<float>(), xTL, xSize);
		if (!Near(xSize.x, 4.0f * fScale, 0.05f) || !Near(xSize.y, 8.0f * fScale, 0.05f))
			{ g_szMiniWhy = "90-degree room must swap extents"; return; }

		// 45° yaw: AABB half-extent = (hx + hz) * cos(45).
		DP_Minimap::RoomPanelRect(xView, 50.0f, 50.0f, 4.0f, 2.0f, glm::quarter_pi<float>(), xTL, xSize);
		const float fExpect = 2.0f * (4.0f + 2.0f) * 0.70710678f * fScale;
		if (!Near(xSize.x, fExpect, 0.05f) || !Near(xSize.y, fExpect, 0.05f))
			{ g_szMiniWhy = "45-degree room AABB wrong"; return; }
	}

	// ---- 3. Memory colour mapping ----
	{
		using S = DP_Fog::MemoryTileState;
		const Vec4 xNever = DP_Minimap::ColourForMemoryState(S::NeverSeen, 0u);
		if (xNever.w != 0.0f) { g_szMiniWhy = "NeverSeen must be transparent"; return; }

		const Vec4 xFresh = DP_Minimap::ColourForMemoryState(S::VisitedVisible, 255u);
		if (!Near(xFresh.w, 1.0f)) { g_szMiniWhy = "fully-visible room must be opaque"; return; }

		const Vec4 xHidden = DP_Minimap::ColourForMemoryState(S::VisitedHidden, 0u);
		if (xHidden.w <= 0.0f || xHidden.w > 0.5f)
			{ g_szMiniWhy = "hidden room must keep a small non-zero floor alpha"; return; }

		float fPrev = -1.0f;
		for (uint32_t u = 0; u <= 255; u += 5)
		{
			const Vec4 xC = DP_Minimap::ColourForMemoryState(S::VisitedDim, static_cast<uint8_t>(u));
			if (xC.w < fPrev) { g_szMiniWhy = "alpha must be monotonic in visibility"; return; }
			fPrev = xC.w;
		}
	}

	g_bMiniPassed = true;
	std::printf("[T0MinimapPure] all 3 clusters passed\n");
	std::fflush(stdout);
}

static bool Step_MinimapPure(int /*iFrame*/) { return false; }

static bool Verify_MinimapPure()
{
	if (!g_bMiniPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "T0MinimapPure failed: %s", g_szMiniWhy);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xT0MinimapPureTest = {
	"Test_T0Minimap_PureMaths",
	&Setup_MinimapPure,
	&Step_MinimapPure,
	&Verify_MinimapPure,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xT0MinimapPureTest);

#endif // ZENITH_INPUT_SIMULATOR
