#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Source/DP_Tuning.h"

#include <cmath>

// ============================================================================
// Test_P1Tuning_LoadsAndValuesInBand (MVP-0.1.1 — red step)
//
// Verifies that the DP_Tuning subsystem (initialised during
// DevilsPlayground::InitializeResources) has loaded Config/Tuning.json and
// exposes plausible values via templated Get<T>() accessors.
//
// Two classes of assertion:
//   1. Exact-equality spot checks against the ratified Tuning.json (catches
//      accidental edits to load-bearing constants like the 30 s life timer
//      or the demon's anchor spawn).
//   2. Sanity bands — each value sits inside its plausible operating range.
//      The bands hard-code reasonable extremes, not the json values, so a
//      future tuner who shifts a number to a still-sensible value won't
//      tickle this test; only egregiously bad tuning trips it.
//
// 1-phase pattern: Setup is a no-op (DP_Tuning::Initialize already ran during
// boot), Step terminates on iFrame==0 (no ticking needed — we're reading
// cached values), Verify holds all assertions.
// ============================================================================

static int g_iTuningFailures = 0;

static void Setup_P1Tuning()
{
	g_iTuningFailures = 0;
}

static bool Step_P1Tuning(int iFrame)
{
	// Never iterate — values are read once in Verify.
	return iFrame < 0;
}

static bool VerifyFloatEqual(const char* szKey, float fActual, float fExpected)
{
	const float fTolerance = 0.001f;
	if (fabsf(fActual - fExpected) >= fTolerance)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning: '%s' expected %f, got %f (tolerance %f)",
			szKey, fExpected, fActual, fTolerance);
		++g_iTuningFailures;
		return false;
	}
	return true;
}

static bool VerifyIntEqual(const char* szKey, int iActual, int iExpected)
{
	if (iActual != iExpected)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning: '%s' expected %d, got %d",
			szKey, iExpected, iActual);
		++g_iTuningFailures;
		return false;
	}
	return true;
}

static bool VerifyBoolEqual(const char* szKey, bool bActual, bool bExpected)
{
	if (bActual != bExpected)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning: '%s' expected %d, got %d",
			szKey, bExpected ? 1 : 0, bActual ? 1 : 0);
		++g_iTuningFailures;
		return false;
	}
	return true;
}

static bool VerifyFloatInRangeInclusive(const char* szKey, float fActual,
                                        float fMin, float fMax)
{
	if (fActual < fMin || fActual > fMax)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning: '%s'=%f outside inclusive band [%f, %f]",
			szKey, fActual, fMin, fMax);
		++g_iTuningFailures;
		return false;
	}
	return true;
}

static bool VerifyFloatInRangeExclusive(const char* szKey, float fActual,
                                        float fMin, float fMax)
{
	if (fActual <= fMin || fActual >= fMax)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning: '%s'=%f outside exclusive band (%f, %f)",
			szKey, fActual, fMin, fMax);
		++g_iTuningFailures;
		return false;
	}
	return true;
}

static bool VerifyIntInRangeInclusive(const char* szKey, int iActual,
                                      int iMin, int iMax)
{
	if (iActual < iMin || iActual > iMax)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning: '%s'=%d outside inclusive band [%d, %d]",
			szKey, iActual, iMin, iMax);
		++g_iTuningFailures;
		return false;
	}
	return true;
}

static bool Verify_P1Tuning()
{
	// ---- 1) Exact-equality spot checks ------------------------------------

	const float fLifeTimerDefault =
		DP_Tuning::Get<float>("possession.life_timer_default_s");
	VerifyFloatEqual("possession.life_timer_default_s",
		fLifeTimerDefault, 30.0f);

	const float fAnchorX =
		DP_Tuning::Get<float>("possession.anchor_initial_position.x");
	VerifyFloatEqual("possession.anchor_initial_position.x",
		fAnchorX, 50.0f);

	const int iReagentsRequired =
		DP_Tuning::Get<int>("night.reagents_required_for_victory");
	VerifyIntEqual("night.reagents_required_for_victory",
		iReagentsRequired, 5);

	const bool bApprehendInterruptible =
		DP_Tuning::Get<bool>("priest.apprehend_interruptible_by_switch");
	VerifyBoolEqual("priest.apprehend_interruptible_by_switch",
		bApprehendInterruptible, true);

	const bool bFollowModeDefault =
		DP_Tuning::Get<bool>("camera.follow_mode_default");
	VerifyBoolEqual("camera.follow_mode_default",
		bFollowModeDefault, false);

	// ---- 2) Sanity-band asserts -------------------------------------------

	// 6) Life-timer default sits between its min and max bounds.
	const float fLifeTimerMin =
		DP_Tuning::Get<float>("possession.life_timer_min_s");
	const float fLifeTimerMax =
		DP_Tuning::Get<float>("possession.life_timer_max_s");
	VerifyFloatInRangeInclusive("possession.life_timer_default_s vs min/max",
		fLifeTimerDefault, fLifeTimerMin, fLifeTimerMax);

	// 7) Movement speeds strictly increase: walk < jog < sprint.
	const float fWalkSpeed   = DP_Tuning::Get<float>("movement.walk_speed_mps");
	const float fJogSpeed    = DP_Tuning::Get<float>("movement.jog_speed_mps");
	const float fSprintSpeed = DP_Tuning::Get<float>("movement.sprint_speed_mps");
	if (!(fWalkSpeed < fJogSpeed && fJogSpeed < fSprintSpeed))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning: movement speeds out of order "
			"(walk=%f jog=%f sprint=%f); expected walk<jog<sprint",
			fWalkSpeed, fJogSpeed, fSprintSpeed);
		++g_iTuningFailures;
	}

	// 8) Priest FOV strictly between 0 and 360 degrees.
	const float fPriestFov = DP_Tuning::Get<float>("priest.sight_fov_deg");
	VerifyFloatInRangeExclusive("priest.sight_fov_deg",
		fPriestFov, 0.0f, 360.0f);

	// 9) Priest apprehend range strictly between 0 and 10 metres.
	const float fApprehendRange =
		DP_Tuning::Get<float>("priest.apprehend_range_m");
	VerifyFloatInRangeExclusive("priest.apprehend_range_m",
		fApprehendRange, 0.0f, 10.0f);

	// 10) Fog-of-war max holes per frame in [1, 1024].
	const int iMaxHolesPerFrame =
		DP_Tuning::Get<int>("fog_of_war.max_holes_per_frame");
	VerifyIntInRangeInclusive("fog_of_war.max_holes_per_frame",
		iMaxHolesPerFrame, 1, 1024);

	// 11) Dawn timer between 1 minute and 1 hour (exclusive).
	const float fDawnTimer = DP_Tuning::Get<float>("night.dawn_timer_s");
	VerifyFloatInRangeExclusive("night.dawn_timer_s",
		fDawnTimer, 60.0f, 3600.0f);

	// 12) Fixed-dt sits between 0.01 and 0.02 (exclusive) — 60 Hz region.
	const float fFixedDt60Hz =
		DP_Tuning::Get<float>("test_constants.fixed_dt_60hz");
	VerifyFloatInRangeExclusive("test_constants.fixed_dt_60hz",
		fFixedDt60Hz, 0.01f, 0.02f);

	if (g_iTuningFailures > 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning: %d assertion(s) failed", g_iTuningFailures);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xTuningTest = {
	"Test_P1Tuning_LoadsAndValuesInBand",
	&Setup_P1Tuning,
	&Step_P1Tuning,
	&Verify_P1Tuning,
	30 // m_iMaxFrames — values are cached, no ticking needed
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTuningTest);

#endif // ZENITH_INPUT_SIMULATOR
