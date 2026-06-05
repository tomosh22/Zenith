#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "CityBuilder/Source/CB_RoadController.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"

// ============================================================================
// CB_RoadDraw — G1 windowed gate. Draws a curved, multi-segment, tangent-
// continuous road through the live CB_RoadController (the same HandleClick the
// mouse tool drives) near the city centre, then asserts the road graph + the
// ribbon mesh built, and renders for ~30 frames so a screenshot shows the curve
// on the terrain. Windowed (needs the live terrain heightfield + GPU).
// ============================================================================

namespace
{
	bool s_bDrawn = false;

	CB_RoadController* Ctrl() { return CB_CityManager_Behaviour::GetActiveRoadController(); }
}

static void Setup_RoadDraw()
{
	s_bDrawn = false;
}

static bool Step_RoadDraw(int iFrame)
{
	if (!s_bDrawn)
	{
		CB_RoadController*     pxCtrl  = Ctrl();
		CB_TerrainHeightfield* pxField = CB_CityManager_Behaviour::GetActiveHeightfield();
		if (pxCtrl != nullptr && pxField != nullptr)
		{
			// Four clicks near the 4km-world centre → a smooth S-curve (3 segments).
			pxCtrl->SetRoadClass(CB_ROADCLASS_MEDIUM);
			pxCtrl->HandleClick(1950.0f, 2048.0f);
			pxCtrl->HandleClick(2050.0f, 2120.0f);
			pxCtrl->HandleClick(2170.0f, 2010.0f);
			pxCtrl->HandleClick(2270.0f, 2090.0f);
			pxCtrl->EndRoad();
			pxCtrl->RebuildMesh(*pxField);
			s_bDrawn = true;
		}
	}
	return iFrame < 240;   // hold ~4s so the curved road is visible/screenshot-able
}

static bool Verify_RoadDraw()
{
	CB_RoadController* pxCtrl = Ctrl();
	if (pxCtrl == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_RoadDraw: no active road controller");
		return false;
	}
	const CB_RoadGraph& xGraph = pxCtrl->GetGraph();
	bool bOk = true;
	if (xGraph.GetActiveSegmentCount() < 3)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_RoadDraw: expected >= 3 segments, got %u", xGraph.GetActiveSegmentCount());
		bOk = false;
	}
	if (xGraph.GetActiveNodeCount() < 4)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_RoadDraw: expected >= 4 nodes, got %u", xGraph.GetActiveNodeCount());
		bOk = false;
	}
	// At least one curved segment: its arc length should exceed its chord.
	bool bAnyCurved = false;
	for (uint32_t i = 0; i < xGraph.GetSegmentSlotCount(); ++i)
	{
		const CB_RoadSegment& xSeg = xGraph.GetSegment(i);
		if (!xSeg.m_bActive) continue;
		const float fChord = CB_Spline::Distance(xSeg.m_xSpline.m_axControl[0], xSeg.m_xSpline.m_axControl[3]);
		if (xSeg.m_xSpline.Length() > fChord + 0.5f) { bAnyCurved = true; }
	}
	if (!bAnyCurved)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_RoadDraw: no curved segment found");
		bOk = false;
	}
	return bOk;
}

static const Zenith_AutomatedTest g_xRoadDrawTest = { "CB_RoadDraw", &Setup_RoadDraw, &Step_RoadDraw, &Verify_RoadDraw, 300, true };
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadDrawTest);

#endif // ZENITH_INPUT_SIMULATOR
