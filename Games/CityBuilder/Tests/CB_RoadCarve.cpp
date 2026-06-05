#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_RoadTerrain.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_TerrainGen.h"
#include <cmath>

// ============================================================================
// CB_RoadCarve — gate for the road→terrain carve (CPU layer, headless). A road
// across the hills levels + recesses its corridor in the heightfield (so the road
// ribbon + frontage buildings sit on a flat bed), while ground well away from any
// road keeps the original hill height. The GPU mesh re-upload is windowed-only.
// ============================================================================

namespace { using V2 = Zenith_Maths::Vector2; bool Near(float a, float b, float e) { return std::fabs(a - b) <= e; } }

static bool Verify_CB_RoadCarve_Heightfield()
{
	CB_RoadGraph xG;
	const uint32_t uA = xG.AddNode(V2(1000.0f, 1000.0f));
	const uint32_t uB = xG.AddNode(V2(1600.0f, 1000.0f));
	xG.AddSegment(uA, uB, CB_Spline::Straight(V2(1000.0f, 1000.0f), V2(1600.0f, 1000.0f)), CB_ROADCLASS_MEDIUM);

	CB_TerrainHeightfield xF;
	xF.Init(257, 257, 16.0f, 0.0f, 0.0f, CB_TerrainGen::HEIGHT_SCALE);
	for (uint32_t uZ = 0; uZ < xF.GetSamplesZ(); ++uZ)
	{
		for (uint32_t uX = 0; uX < xF.GetSamplesX(); ++uX)
		{
			xF.SetNormalized(uX, uZ, CB_TerrainGen::HillNorm(static_cast<float>(uX) * 16.0f, static_cast<float>(uZ) * 16.0f));
		}
	}

	const float fOnRoadBefore = xF.GetHeightAt(1300.0f, 1000.0f);   // on the road centreline
	const float fOffRoadBefore = xF.GetHeightAt(1300.0f, 1320.0f);  // 320m off the road

	CB_RoadTerrain::FlattenHeightfield(xG, xF);

	const float fOnRoadAfter  = xF.GetHeightAt(1300.0f, 1000.0f);
	const float fOffRoadAfter = xF.GetHeightAt(1300.0f, 1320.0f);

	bool bOk = true;
	// On the road: levelled to the centreline height, recessed by the bed depth.
	if (!(fOnRoadAfter < fOnRoadBefore - 0.2f))                                      { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadCarve: road not recessed (%.2f -> %.2f)", fOnRoadBefore, fOnRoadAfter); bOk = false; }
	if (!Near(fOnRoadAfter, fOnRoadBefore - CB_RoadTerrain::BED_DEPTH, 0.4f))        { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadCarve: bed depth wrong (%.2f, want %.2f)", fOnRoadAfter, fOnRoadBefore - CB_RoadTerrain::BED_DEPTH); bOk = false; }
	// Off the road: hill preserved.
	if (!Near(fOffRoadAfter, fOffRoadBefore, 0.3f))                                  { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadCarve: off-road terrain changed (%.2f -> %.2f)", fOffRoadBefore, fOffRoadAfter); bOk = false; }
	// Off-road terrain is genuinely hilly (not flat) — the carve is local.
	if (!(fOffRoadAfter > 5.0f))                                                     { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadCarve: terrain not hilly (%.2f)", fOffRoadAfter); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_RoadCarve: on-road %.1f->%.1f, off-road %.1f->%.1f", fOnRoadBefore, fOnRoadAfter, fOffRoadBefore, fOffRoadAfter);
	return bOk;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xRoadCarveTest = { "CB_RoadCarve_Heightfield", nullptr, &Step_Once, &Verify_CB_RoadCarve_Heightfield, 30, false };
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadCarveTest);

#endif // ZENITH_INPUT_SIMULATOR
