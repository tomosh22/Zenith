#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_TerrainModifier.h"
#include "CityBuilder/Source/CB_TerrainGen.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include <cmath>

// ============================================================================
// CB_Terrain — Phase-2 gate. Exercises the CPU-authoritative terrain
// heightfield (GetHeightAt + runtime deform brushes) and the CityManager
// wiring. All logic-only (headless); the pure-field tests build a local
// CB_TerrainHeightfield so they don't depend on the loaded scene.
// ============================================================================

namespace
{
	bool NearlyEqual(float a, float b, float eps = 0.01f) { return std::fabs(a - b) <= eps; }
}

// ---- CB_Terrain_Flat: a freshly-initialised field reads flat (height 0) ----
static bool Verify_CB_Terrain_Flat()
{
	CB_TerrainHeightfield xField;
	xField.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);

	bool bOk = true;
	if (!xField.IsInitialized()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Flat: not initialised"); bOk = false; }
	if (!NearlyEqual(xField.GetHeightAt(2048.0f, 2048.0f), 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Flat: centre not flat"); bOk = false; }
	if (!NearlyEqual(xField.GetHeightAt(0.0f, 0.0f), 0.0f))       { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Flat: origin not flat"); bOk = false; }
	// Out-of-bounds queries clamp, never crash.
	if (!NearlyEqual(xField.GetHeightAt(-500.0f, 999999.0f), 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Flat: oob not flat"); bOk = false; }
	return bOk;
}

// ---- CB_Terrain_Raise: RAISE brush lifts the centre, leaves far cells flat ----
static bool Verify_CB_Terrain_Raise()
{
	CB_TerrainHeightfield xField;
	xField.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);

	CB_TerrainBrush xBrush;
	xBrush.m_eMode = CB_TERRAIN_BRUSH_RAISE;
	xBrush.m_fCentreX = 2048.0f;
	xBrush.m_fCentreZ = 2048.0f;
	xBrush.m_fRadius = 200.0f;
	xBrush.m_fStrength = 1.0f;
	xField.ApplyBrush(xBrush);

	bool bOk = true;
	const float fCentre = xField.GetHeightAt(2048.0f, 2048.0f);
	if (!(fCentre > 1.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Raise: centre not raised (%f)", fCentre); bOk = false; }
	if (xField.GetDirtyCount() == 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Raise: no dirty samples"); bOk = false; }
	// A point well outside the brush radius stays flat.
	if (!NearlyEqual(xField.GetHeightAt(100.0f, 100.0f), 0.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Raise: far cell not flat"); bOk = false; }
	return bOk;
}

// ---- CB_Terrain_Flatten: FLATTEN returns a raised area toward a target ----
static bool Verify_CB_Terrain_Flatten()
{
	CB_TerrainHeightfield xField;
	xField.Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);

	CB_TerrainBrush xRaise;
	xRaise.m_eMode = CB_TERRAIN_BRUSH_RAISE;
	xRaise.m_fCentreX = 1024.0f;
	xRaise.m_fCentreZ = 1024.0f;
	xRaise.m_fRadius = 300.0f;
	xRaise.m_fStrength = 1.0f;
	xField.ApplyBrush(xRaise);
	xField.ApplyBrush(xRaise);  // raise twice so it's clearly non-zero

	const float fRaised = xField.GetHeightAt(1024.0f, 1024.0f);
	if (!(fRaised > 1.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Flatten: precondition raise failed (%f)", fRaised); return false; }

	// Flatten back toward 0. Several passes since FLATTEN blends fractionally.
	CB_TerrainBrush xFlat;
	xFlat.m_eMode = CB_TERRAIN_BRUSH_FLATTEN;
	xFlat.m_fCentreX = 1024.0f;
	xFlat.m_fCentreZ = 1024.0f;
	xFlat.m_fRadius = 300.0f;
	xFlat.m_fStrength = 1.0f;
	xFlat.m_fTargetWorldY = 0.0f;
	for (int i = 0; i < 8; ++i) { xField.ApplyBrush(xFlat); }

	const float fAfter = xField.GetHeightAt(1024.0f, 1024.0f);
	if (!(fAfter < fRaised * 0.5f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Flatten: not flattened (%f -> %f)", fRaised, fAfter); return false; }
	return true;
}

// ---- CB_Terraform_RaiseLower: the terraform tool's brush (raise then lower) reshapes
//      a hill-shaped field — raise builds a hill, lower digs it back below the ground ----
static bool Verify_CB_Terraform_RaiseLower()
{
	CB_TerrainHeightfield xField;
	xField.Init(257, 257, 16.0f, 0.0f, 0.0f, CB_TerrainGen::HEIGHT_SCALE);
	// Shape to the baseline rolling hills, exactly like the live CityManager field.
	for (uint32_t uZ = 0; uZ < xField.GetSamplesZ(); ++uZ)
	{
		for (uint32_t uX = 0; uX < xField.GetSamplesX(); ++uX)
		{
			xField.SetNormalized(uX, uZ, CB_TerrainGen::HillNorm(static_cast<float>(uX) * 16.0f, static_cast<float>(uZ) * 16.0f));
		}
	}

	const float fX = 2048.0f, fZ = 2048.0f;
	const float fBase = xField.GetHeightAt(fX, fZ);

	// Hold LMB ~2s (the tool's radius + per-frame strength) → a real hill.
	CB_TerrainBrush xRaise;
	xRaise.m_eMode = CB_TERRAIN_BRUSH_RAISE;
	xRaise.m_fCentreX = fX; xRaise.m_fCentreZ = fZ;
	xRaise.m_fRadius = 45.0f; xRaise.m_fStrength = 0.02f;
	for (int i = 0; i < 120; ++i) { xField.ApplyBrush(xRaise); }
	const float fRaised = xField.GetHeightAt(fX, fZ);

	// Then hold RMB → dig it back down past the original ground.
	CB_TerrainBrush xLower = xRaise;
	xLower.m_eMode = CB_TERRAIN_BRUSH_LOWER;
	for (int i = 0; i < 240; ++i) { xField.ApplyBrush(xLower); }
	const float fLowered = xField.GetHeightAt(fX, fZ);

	bool bOk = true;
	if (!(fRaised  > fBase + 5.0f))    { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terraform_RaiseLower: raise didn't build a hill (%.1f -> %.1f)", fBase, fRaised); bOk = false; }
	if (!(fLowered < fRaised - 5.0f))  { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terraform_RaiseLower: lower didn't dig it down (%.1f -> %.1f)", fRaised, fLowered); bOk = false; }
	if (!(fLowered < fBase + 0.01f))   { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terraform_RaiseLower: didn't drop below the original ground (%.1f vs %.1f)", fLowered, fBase); bOk = false; }
	// A point outside the brush keeps its baseline hill height (local edit only).
	const float fFar = xField.GetHeightAt(1000.0f, 1000.0f);
	const float fFarBase = CB_TerrainGen::HillNorm(1000.0f, 1000.0f) * CB_TerrainGen::HEIGHT_SCALE;
	if (!NearlyEqual(fFar, fFarBase, 1.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terraform_RaiseLower: far point disturbed (%.1f vs %.1f)", fFar, fFarBase); bOk = false; }
	Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terraform_RaiseLower: base %.1f -> raised %.1f -> lowered %.1f", fBase, fRaised, fLowered);
	return bOk;
}

// ---- CB_Terrain_Active: CityManager publishes a live, hill-shaped heightfield ----
static bool Verify_CB_Terrain_Active()
{
	CB_TerrainHeightfield* pxField = CB_CityManager_Behaviour::GetActiveHeightfield();
	if (pxField == nullptr) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Active: no active heightfield"); return false; }
	if (!pxField->IsInitialized()) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Active: active field not initialised"); return false; }
	// The field is shaped to the shared CB_TerrainGen rolling hills (the SAME
	// function the GPU bake uses), so the bridge query is non-flat + matches it.
	const float fCentre = CB_TerrainModifier::GetHeightAt(2048.0f, 2048.0f);
	const float fExpect = CB_TerrainGen::HillNorm(2048.0f, 2048.0f) * CB_TerrainGen::HEIGHT_SCALE;
	if (!NearlyEqual(fCentre, fExpect, 1.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Active: centre %.1f != hill %.1f", fCentre, fExpect); return false; }
	if (!(fCentre > 10.0f))                   { Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Terrain_Active: terrain not hilly (%.1f)", fCentre); return false; }
	return true;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xTerrainFlatTest    = { "CB_Terrain_Flat",    nullptr, &Step_Once, &Verify_CB_Terrain_Flat,    30, false };
static const Zenith_AutomatedTest g_xTerrainRaiseTest   = { "CB_Terrain_Raise",   nullptr, &Step_Once, &Verify_CB_Terrain_Raise,   30, false };
static const Zenith_AutomatedTest g_xTerrainFlattenTest = { "CB_Terrain_Flatten", nullptr, &Step_Once, &Verify_CB_Terrain_Flatten, 30, false };
static const Zenith_AutomatedTest g_xTerraformTest      = { "CB_Terraform_RaiseLower", nullptr, &Step_Once, &Verify_CB_Terraform_RaiseLower, 30, false };
static const Zenith_AutomatedTest g_xTerrainActiveTest  = { "CB_Terrain_Active",  nullptr, &Step_Once, &Verify_CB_Terrain_Active,  30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xTerrainFlatTest);
ZENITH_AUTOMATED_TEST_REGISTER(g_xTerrainRaiseTest);
ZENITH_AUTOMATED_TEST_REGISTER(g_xTerrainFlattenTest);
ZENITH_AUTOMATED_TEST_REGISTER(g_xTerraformTest);
ZENITH_AUTOMATED_TEST_REGISTER(g_xTerrainActiveTest);

#endif // ZENITH_INPUT_SIMULATOR
