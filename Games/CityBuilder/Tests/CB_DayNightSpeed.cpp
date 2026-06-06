#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "CityBuilder/Source/CB_DayNight.h"
#include "CityBuilder/Source/CB_SimSpeed.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// CB_DayNight / sim-speed unit tests (headless). The day/night clock + sun-
// direction math and the sim-speed multiplier table are pure logic, verified
// directly without a live scene.
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
	if (CB_SpeedMultiplier(CB_SIM_PAUSED) != 0.0f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: paused"); bOk = false; }
	if (CB_SpeedMultiplier(CB_SIM_SLOW)   != 0.5f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: slow"); bOk = false; }
	if (CB_SpeedMultiplier(CB_SIM_NORMAL) != 1.0f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: normal"); bOk = false; }
	if (CB_SpeedMultiplier(CB_SIM_FAST)   != 2.0f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: fast"); bOk = false; }
	if (CB_SpeedMultiplier(CB_SIM_ULTRA)  != 4.0f) { Zenith_Log(LOG_CATEGORY_UNITTEST, "Speed: ultra"); bOk = false; }
	return bOk;
}

static bool Step_Once(int iFrame) { return iFrame < 1; }

static const Zenith_AutomatedTest g_xDayNight = { "CB_DayNight_Cycle", nullptr, &Step_Once, &Verify_CB_DayNight_Cycle, 30, false };
static const Zenith_AutomatedTest g_xSpeed    = { "CB_Sim_Speed",      nullptr, &Step_Once, &Verify_CB_Sim_Speed,      30, false };

ZENITH_AUTOMATED_TEST_REGISTER(g_xDayNight);
ZENITH_AUTOMATED_TEST_REGISTER(g_xSpeed);

#endif // ZENITH_INPUT_SIMULATOR
