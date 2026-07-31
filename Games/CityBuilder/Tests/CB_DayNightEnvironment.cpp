#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_SunComponent.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "CityBuilder/Components/CB_DayNightCycleComponent.h"
#include "CityBuilder/Source/CB_DayNight.h"
#include "ZenithECS/Zenith_SceneSystem.h"

// ============================================================================
// CB_DayNightEnvironment — locks the ZM-D-172 mutable-authority leak fix.
//
// CityBuilder's day/night clock must drive the authoritative Zenith_SunComponent
// TIME-OF-DAY geometry (co-located on the CityManager environment entity), NOT
// the renderer's radiometric anchor. Night brightness therefore results from the
// sun moving below the horizon + atmospheric transmittance, not from animating
// solar intensity. This test proves:
//   - the CityManager (the CBDayNight host) carries a Zenith_SunComponent after
//     OnStart (the co-located environment-entity authoring shape);
//   - driving the clock to noon / midnight moves the Sun's time-of-day angle to
//     90 / 270 degrees (wrapped);
//   - the Flux sun intensity (the one radiometric anchor) stays at
//     AtmosphereConfig::fSUN_INTENSITY at BOTH noon and midnight -- it is NOT
//     animated by the clock (the old SetSunIntensity leak produced 2.8 / 0.2).
//
// Headless (m_bRequiresGraphics=false): it reads the live component + a Skybox
// float, both available on the Null backend. The harness boot loads scene 0
// (City) before Setup, so OnStart has already attached the Sun.
// ============================================================================

namespace
{
	bool g_bFailed      = false;
	bool g_bSawSun      = false;
	bool g_bNoonOk      = false;
	bool g_bMidnightOk  = false;
	bool g_bAnchorNoon  = false;
	bool g_bAnchorNight = false;

	Zenith_SunComponent* FindActiveSun()
	{
		Zenith_SunComponent* pxSun = nullptr;
		g_xEngine.Scenes().QueryActiveScene<Zenith_SunComponent>().ForEach(
			[&](Zenith_EntityID, Zenith_SunComponent& xSun)
			{
				if (pxSun == nullptr) pxSun = &xSun;
			});
		return pxSun;
	}

	void DnEnvFail(const char* szReason)
	{
		if (!g_bFailed) Zenith_Error(LOG_CATEGORY_UNITTEST, "[CBDayNightEnv] %s", szReason);
		g_bFailed = true;
	}
}

static void Setup_CB_DayNightEnvironment()
{
	g_bFailed = false;
	g_bSawSun = false;
	g_bNoonOk = false;
	g_bMidnightOk = false;
	g_bAnchorNoon = false;
	g_bAnchorNight = false;

	CB_DayNight* pxCycle = CB_DayNightCycleComponent::GetActive();
	if (pxCycle == nullptr)
	{
		DnEnvFail("no active CB_DayNight cycle -- CBDayNight OnStart did not fire");
		return;
	}
	// Freeze the clock so a set m_fTimeOfDay is held steady across frames while
	// OnUpdate maps it onto the Sun's time-of-day angle.
	pxCycle->m_fDayLengthSecs = 1e9f;
	pxCycle->m_fTimeOfDay = 0.5f;  // noon
}

static bool Step_CB_DayNightEnvironment(int iFrame)
{
	if (g_bFailed) return false;
	if (iFrame < 2) return true;  // let OnStart's Sun attach + an OnUpdate drive it

	CB_DayNight* pxCycle = CB_DayNightCycleComponent::GetActive();
	if (pxCycle == nullptr)
	{
		DnEnvFail("active CB_DayNight cycle disappeared mid-test");
		return false;
	}

	Zenith_SunComponent* pxSun = FindActiveSun();
	if (pxSun == nullptr)
	{
		DnEnvFail("CityManager has no co-located Zenith_SunComponent after OnStart");
		return false;
	}
	g_bSawSun = true;

	const float fAnchor = AtmosphereConfig::fSUN_INTENSITY;

	// Noon pass: clock 0.5 -> Sun angle 90; anchor must NOT have been animated.
	if (!g_bNoonOk)
	{
		const float fAngle = pxSun->GetTimeOfDayAngleDegrees();
		if (fabsf(fAngle - 90.0f) > 0.5f)
		{
			DnEnvFail("noon clock did not move the Sun to 90 degrees (time-of-day)");
			return false;
		}
		g_bNoonOk = true;
		const float fIntensity = g_xEngine.Skybox().GetSunIntensity();
		if (fabsf(fIntensity - fAnchor) > 0.0001f)
		{
			DnEnvFail("Flux sun intensity was animated away from the radiometric anchor at noon");
			return false;
		}
		g_bAnchorNoon = true;
		// Drive to midnight for the next pass.
		pxCycle->m_fTimeOfDay = 0.0f;
		return true;
	}

	// Midnight pass: clock 0.0 -> Sun angle 270 (wrapped); anchor still unchanged.
	if (!g_bMidnightOk)
	{
		const float fAngle = pxSun->GetTimeOfDayAngleDegrees();
		if (fabsf(fAngle - 270.0f) > 0.5f)
		{
			DnEnvFail("midnight clock did not move the Sun to 270 degrees (time-of-day)");
			return false;
		}
		g_bMidnightOk = true;
		const float fIntensity = g_xEngine.Skybox().GetSunIntensity();
		if (fabsf(fIntensity - fAnchor) > 0.0001f)
		{
			DnEnvFail("Flux sun intensity was animated away from the radiometric anchor at midnight");
			return false;
		}
		g_bAnchorNight = true;
		return false;  // done
	}
	return false;
}

static bool Verify_CB_DayNightEnvironment()
{
	return !g_bFailed
		&& g_bSawSun
		&& g_bNoonOk
		&& g_bMidnightOk
		&& g_bAnchorNoon
		&& g_bAnchorNight;
}

static const Zenith_AutomatedTest g_xCBDayNightEnv = {
	"CB_DayNight_Environment",
	&Setup_CB_DayNightEnvironment,
	&Step_CB_DayNightEnvironment,
	&Verify_CB_DayNightEnvironment,
	30,
	false   // headless: reads a component + a Skybox float (Null backend has both)
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xCBDayNightEnv);

#endif // ZENITH_INPUT_SIMULATOR