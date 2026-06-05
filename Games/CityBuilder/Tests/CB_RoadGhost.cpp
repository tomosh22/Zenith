#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "CityBuilder/Source/CB_RoadController.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include <cmath>

// ============================================================================
// CB_RoadGhost — windowed gate for the SimCity/C:S-style road tool: lays one
// committed segment (so there is a node to snap to), anchors a NEW road's start,
// then sweeps the ghost-preview cursor around it (green ribbon following the
// "mouse") and finally parks it over the existing node (cyan = snap-to-junction).
// Proves the placement ghost renders BEFORE the confirm click. Screenshot target.
// ============================================================================

namespace { bool s_bSetup = false; }

static void Setup_RoadGhost() { s_bSetup = false; }

static bool Step_RoadGhost(int iFrame)
{
	CB_RoadController*     pxCtrl  = CB_CityManager_Behaviour::GetActiveRoadController();
	CB_TerrainHeightfield* pxField = CB_CityManager_Behaviour::GetActiveHeightfield();
	CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive();
	if (pxCtrl == nullptr || pxField == nullptr || pxMgr == nullptr)
	{
		return iFrame < 900;
	}

	if (!s_bSetup)
	{
		pxMgr->GetTools().SetTool(CB_TOOL_ROAD);
		pxCtrl->SetRoadClass(CB_ROADCLASS_MEDIUM);
		// One committed segment -> leaves a node at (2120,2120) for the snap demo.
		pxCtrl->HandleClick(2120.0f, 1980.0f);
		pxCtrl->HandleClick(2120.0f, 2120.0f);
		pxCtrl->EndRoad();
		pxCtrl->RebuildMesh(*pxField);
		// Anchor a NEW road's start at A — the ghost previews from here to the cursor.
		pxCtrl->HandleClick(1960.0f, 2050.0f);
		s_bSetup = true;
	}

	// Drive the ghost cursor: first half sweeps an arc around A (green preview that
	// tracks the "mouse"); second half parks on the existing node (cyan snap).
	const float fT = static_cast<float>(iFrame) / 900.0f;
	float fBX, fBZ;
	if (fT < 0.5f)
	{
		const float fA = fT * 12.0f;
		fBX = 1960.0f + 130.0f * std::cos(fA);
		fBZ = 2050.0f + 90.0f  * std::sin(fA);
	}
	else
	{
		fBX = 2120.0f; fBZ = 2120.0f;   // snap to the existing junction node
	}
	pxCtrl->SetHoverPointForAutomation(fBX, fBZ);

	return iFrame < 900;
}

static bool Verify_RoadGhost()
{
	CB_RoadController* pxCtrl = CB_CityManager_Behaviour::GetActiveRoadController();
	if (pxCtrl == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_RoadGhost: no road controller");
		return false;
	}
	// The ghost ribbon must have been generated (preview from the anchored start).
	if (pxCtrl->GetPreviewTriVertCount() == 0)
	{
		Zenith_Log(LOG_CATEGORY_GAMEPLAY, "CB_RoadGhost: no ghost-preview ribbon");
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xRoadGhostTest = { "CB_RoadGhost", &Setup_RoadGhost, &Step_RoadGhost, &Verify_RoadGhost, 1000, true };
ZENITH_AUTOMATED_TEST_REGISTER(g_xRoadGhostTest);

#endif // ZENITH_INPUT_SIMULATOR
