#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_DayNight.h"
#include "CityBuilder/Source/CB_SimulationTick.h"
#include "CityBuilder/Source/CB_SaveLoad.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"
#include "DataStream/Zenith_DataStream.h"
#include <cmath>

// ============================================================================
// CB_SaveLoad / day-night / speed — Phase-8 gate. Save/load round-trips the
// authoritative city state; the day/night clock + sun math and the sim speed
// table are verified directly. Logic-only (headless).
// ============================================================================

static bool Verify_CB_DayNight_Cycle()
{
	CB_DayNight xDN;
	bool bOk = true;

	// Wrap.
	xDN.m_fTimeOfDay = 0.9f;
	xDN.m_fDayLengthSecs = 1.0f;
	xDN.Advance(0.5f);  // 0.9 + 0.5 = 1.4 -> wraps to 0.4
	if (!(xDN.m_fTimeOfDay >= 0.0f && xDN.m_fTimeOfDay < 1.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "DayNight: no wrap %f", xDN.m_fTimeOfDay); bOk = false; }

	// Day/night classification.
	xDN.m_fTimeOfDay = 0.5f;
	if (!xDN.IsDay())  { Zenith_Log(LOG_CATEGORY_UNITTEST, "DayNight: noon not day"); bOk = false; }
	xDN.m_fTimeOfDay = 0.0f;
	if (xDN.IsDay())   { Zenith_Log(LOG_CATEGORY_UNITTEST, "DayNight: midnight is day"); bOk = false; }

	// Sun overhead at noon -> light points down (y strongly negative).
	xDN.m_fTimeOfDay = 0.5f;
	const Zenith_Maths::Vector3 xSun = xDN.GetSunDirection();
	if (!(xSun.y < -0.9f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "DayNight: noon sun y %f", xSun.y); bOk = false; }
	if (xDN.GetSunElevation() < 0.99f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "DayNight: noon elevation %f", xDN.GetSunElevation()); bOk = false; }
	return bOk;
}

static bool Verify_CB_Sim_Speed()
{
	bool bOk = true;
	if (CB_SimulationTick::SpeedMultiplier(CB_SIM_PAUSED) != 0.0f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: paused"); bOk = false; }
	if (CB_SimulationTick::SpeedMultiplier(CB_SIM_SLOW)   != 0.5f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: slow"); bOk = false; }
	if (CB_SimulationTick::SpeedMultiplier(CB_SIM_NORMAL) != 1.0f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: normal"); bOk = false; }
	if (CB_SimulationTick::SpeedMultiplier(CB_SIM_FAST)   != 2.0f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: fast"); bOk = false; }
	if (CB_SimulationTick::SpeedMultiplier(CB_SIM_ULTRA)  != 4.0f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: ultra"); bOk = false; }

	CB_SimulationTick xSim;
	xSim.SetSpeed(CB_SIM_FAST);
	if (xSim.GetSpeed() != CB_SIM_FAST) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: set/get"); bOk = false; }
	// Paused + uninitialised sim: Update fires no ticks.
	xSim.SetSpeed(CB_SIM_PAUSED);
	xSim.Update(100.0f);
	if (xSim.GetTickCount() != 0) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: paused ticked"); bOk = false; }
	return bOk;
}

static bool Verify_CB_SaveLoad_RoundTrip()
{
	CB_CityManager_Behaviour* pxMgr = CB_CityManager_Behaviour::GetActive();
	if (pxMgr == nullptr) { Zenith_Log(LOG_CATEGORY_UNITTEST, "SaveLoad: no CityManager"); return false; }

	// Build a small city: road strip + residential, grow it.
	for (uint32_t x = 300; x <= 320; ++x) { pxMgr->GetRoads().PlaceRoad(x, 300, CB_ROAD_SMALL); }
	for (uint32_t x = 300; x <= 320; ++x)
	{
		pxMgr->GetGrid().GetCell(x, 299).SetZone(CB_ZONE_RESIDENTIAL);
		pxMgr->GetGrid().GetCell(x, 301).SetZone(CB_ZONE_RESIDENTIAL);
	}
	pxMgr->GetSim().RunTicks(10);

	// Deform terrain near the city.
	CB_TerrainBrush xRaise;
	xRaise.m_eMode = CB_TERRAIN_BRUSH_RAISE;
	xRaise.m_fCentreX = 1280.0f; xRaise.m_fCentreZ = 1200.0f; xRaise.m_fRadius = 200.0f; xRaise.m_fStrength = 1.0f;
	pxMgr->GetHeightfield().ApplyBrush(xRaise);
	pxMgr->GetHeightfield().ApplyBrush(xRaise);

	// Snapshot.
	const uint32_t uBuildings = pxMgr->GetBuildings().GetActiveCount();
	const float    fTreasury  = pxMgr->GetStats().m_fTreasury;
	const float    fHeight    = pxMgr->GetHeightfield().GetHeightAt(1280.0f, 1200.0f);
	const bool     bRoad      = pxMgr->GetGrid().GetCell(310, 300).HasRoad();
	if (!(uBuildings > 0)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "SaveLoad: no buildings to save"); return false; }
	if (!(fHeight > 1.0f)) { Zenith_Log(LOG_CATEGORY_UNITTEST, "SaveLoad: terrain not raised %f", fHeight); return false; }

	// Save.
	Zenith_DataStream xStream;
	CB_SaveLoad::Save(*pxMgr, xStream);

	// Wipe / change everything.
	pxMgr->GetGrid().Initialize(1024, 1024, 4.0f, 0.0f, 0.0f);
	pxMgr->GetBuildings().Initialize(&pxMgr->GetGrid());
	pxMgr->GetHeightfield().Init(257, 257, 16.0f, 0.0f, 0.0f, 200.0f);
	pxMgr->GetSim().SetStartingTreasury(-12345.0f);

	// Load back.
	xStream.SetCursor(0);
	const bool bLoaded = CB_SaveLoad::Load(*pxMgr, xStream);

	bool bOk = true;
	if (!bLoaded)                                                   { Zenith_Log(LOG_CATEGORY_UNITTEST, "SaveLoad: load failed"); return false; }
	if (pxMgr->GetBuildings().GetActiveCount() != uBuildings)       { Zenith_Log(LOG_CATEGORY_UNITTEST, "SaveLoad: buildings %u != %u", pxMgr->GetBuildings().GetActiveCount(), uBuildings); bOk = false; }
	if (pxMgr->GetStats().m_fTreasury != fTreasury)                { Zenith_Log(LOG_CATEGORY_UNITTEST, "SaveLoad: treasury %f != %f", pxMgr->GetStats().m_fTreasury, fTreasury); bOk = false; }
	if (pxMgr->GetGrid().GetCell(310, 300).HasRoad() != bRoad)      { Zenith_Log(LOG_CATEGORY_UNITTEST, "SaveLoad: road not restored"); bOk = false; }
	if (std::fabs(pxMgr->GetHeightfield().GetHeightAt(1280.0f, 1200.0f) - fHeight) > 0.5f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "SaveLoad: height %f != %f", pxMgr->GetHeightfield().GetHeightAt(1280.0f, 1200.0f), fHeight); bOk = false; }
	return bOk;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xDayNight  = { "CB_DayNight_Cycle",       nullptr, &Step_Once, &Verify_CB_DayNight_Cycle,       30, false };
static const Zenith_AutomatedTest g_xSpeed     = { "CB_Sim_Speed",            nullptr, &Step_Once, &Verify_CB_Sim_Speed,            30, false };
static const Zenith_AutomatedTest g_xSaveLoad  = { "CB_SaveLoad_RoundTrip",   nullptr, &Step_Once, &Verify_CB_SaveLoad_RoundTrip,   60, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xDayNight);
ZENITH_AUTOMATED_TEST_REGISTER(g_xSpeed);
ZENITH_AUTOMATED_TEST_REGISTER(g_xSaveLoad);

#endif // ZENITH_INPUT_SIMULATOR
