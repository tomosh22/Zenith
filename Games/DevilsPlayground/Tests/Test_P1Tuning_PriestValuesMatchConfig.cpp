#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/Priest_Behaviour.h"

#include <cmath>

// ============================================================================
// Test_P1Tuning_PriestValuesMatchConfig (MVP-0.1.3)
//
// Regression guard against the priest-tuning drift fixed in MVP-0.1.3.
// Priest_Behaviour previously hardcoded sight=20, hearing=25, FOV=120,
// move=5, peripheral=FOV*1.25 — drift from GDD §4.5 / Tuning.json which
// specify sight=25, hearing=30, FOV=110, pursue=7, peripheral=130. After
// the migration the priest reads from DP_Tuning in OnAwake; this test
// asserts every field matches.
//
// Coverage:
//   1. Exact equality vs DP_Tuning::Get<float>() return values for every
//      migrated key (catches the future "tuner edits config but forgot
//      to delete the hardcoded fallback" bug).
//   2. Exact equality vs the ratified Tuning.json constants (catches the
//      "tuner edits both the config and the priest fallback to the same
//      wrong value" bug).
// ============================================================================

namespace
{
	enum Phase : int { kPr_Start, kPr_WaitPriest, kPr_Done };

	int    g_iPhase     = kPr_Start;
	bool   g_bFoundPriest = false;

	float g_fActSuspicion = -1.0f;
	float g_fActHearRange = -1.0f;
	float g_fActHearLoud  = -1.0f;
	float g_fActSightRng  = -1.0f;
	float g_fActSightFov  = -1.0f;
	float g_fActSightPeri = -1.0f;
	float g_fActSightEye  = -1.0f;
	float g_fActAwareGain = -1.0f;
	float g_fActAwareDec  = -1.0f;
	float g_fActMoveSpd   = -1.0f;
}

static void Setup_P1Tuning_PriestValuesMatchConfig()
{
	g_iPhase = kPr_Start;
	g_bFoundPriest = false;
}

static bool Step_P1Tuning_PriestValuesMatchConfig(int iFrame)
{
	switch (g_iPhase)
	{
	case kPr_Start:
		g_xEngine.SceneOperations().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kPr_WaitPriest;
		return true;

	case kPr_WaitPriest:
	{
		Priest_Behaviour* pxPriest = nullptr;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&pxPriest](Zenith_EntityID /*xId*/, Priest_Behaviour& xP) {
				if (pxPriest == nullptr) { pxPriest = &xP; }
			});

		if (pxPriest != nullptr)
		{
			g_bFoundPriest = true;
			g_fActSuspicion = pxPriest->GetSuspicionRadius();
			g_fActHearRange = pxPriest->GetHearingRange();
			g_fActHearLoud  = pxPriest->GetHearingLoudnessThr();
			g_fActSightRng  = pxPriest->GetSightRange();
			g_fActSightFov  = pxPriest->GetSightFOV();
			g_fActSightPeri = pxPriest->GetSightPeripheral();
			g_fActSightEye  = pxPriest->GetSightEyeHeight();
			g_fActAwareGain = pxPriest->GetSightAwarenessGain();
			g_fActAwareDec  = pxPriest->GetSightAwarenessDecay();
			g_fActMoveSpd   = pxPriest->GetMoveSpeed();
			g_iPhase = kPr_Done;
			return false;
		}

		if (iFrame > 120)
		{
			g_iPhase = kPr_Done;
			return false;
		}
		return true;
	}

	case kPr_Done:
	default:
		return false;
	}
}

static int g_iFailures = 0;

static void CheckMatch(const char* szLabel, float fActual, float fExpected)
{
	const float fTol = 0.001f;
	if (std::fabs(fActual - fExpected) >= fTol)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning_PriestValuesMatchConfig: %s actual=%f expected=%f",
			szLabel, fActual, fExpected);
		++g_iFailures;
	}
}

static bool Verify_P1Tuning_PriestValuesMatchConfig()
{
	if (!g_bFoundPriest)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning_PriestValuesMatchConfig: no Priest_Behaviour in active scene after 120 frames");
		return false;
	}

	g_iFailures = 0;

	// 1) Match DP_Tuning's returned values (the migration's primary contract).
	CheckMatch("suspicion_radius_m vs tuning",        g_fActSuspicion, DP_Tuning::Get<float>("priest.suspicion_radius_m"));
	CheckMatch("hearing_range_m vs tuning",           g_fActHearRange, DP_Tuning::Get<float>("priest.hearing_range_m"));
	CheckMatch("hearing_loudness_threshold vs tuning",g_fActHearLoud,  DP_Tuning::Get<float>("priest.hearing_loudness_threshold"));
	CheckMatch("sight_range_m vs tuning",             g_fActSightRng,  DP_Tuning::Get<float>("priest.sight_range_m"));
	CheckMatch("sight_fov_deg vs tuning",             g_fActSightFov,  DP_Tuning::Get<float>("priest.sight_fov_deg"));
	CheckMatch("sight_peripheral_deg vs tuning",      g_fActSightPeri, DP_Tuning::Get<float>("priest.sight_peripheral_deg"));
	CheckMatch("sight_eye_height_m vs tuning",        g_fActSightEye,  DP_Tuning::Get<float>("priest.sight_eye_height_m"));
	CheckMatch("sight_awareness_gain_rate vs tuning", g_fActAwareGain, DP_Tuning::Get<float>("priest.sight_awareness_gain_rate"));
	CheckMatch("sight_awareness_decay_rate vs tuning",g_fActAwareDec,  DP_Tuning::Get<float>("priest.sight_awareness_decay_rate"));
	CheckMatch("pursue_speed_mps vs tuning",          g_fActMoveSpd,   DP_Tuning::Get<float>("priest.pursue_speed_mps"));

	// 2) Defence-in-depth: ratified Tuning.json constants (catches the
	//    "tuner edits both the JSON and the priest fallback to the same
	//    wrong value" case). These are GDD §4.5 ratified numbers.
	CheckMatch("suspicion_radius == 15.0",  g_fActSuspicion, 15.0f);
	CheckMatch("hearing_range == 35.0",     g_fActHearRange, 35.0f);
	CheckMatch("hearing_loudness == 0.05",  g_fActHearLoud,  0.05f);
	CheckMatch("sight_range == 25.0",       g_fActSightRng,  25.0f);
	CheckMatch("sight_fov == 110.0",        g_fActSightFov,  110.0f);
	CheckMatch("sight_peripheral == 130.0", g_fActSightPeri, 130.0f);
	CheckMatch("sight_eye_height == 1.6",   g_fActSightEye,  1.6f);
	CheckMatch("awareness_gain == 2.0",     g_fActAwareGain, 2.0f);
	CheckMatch("awareness_decay == 0.5",    g_fActAwareDec,  0.5f);
	CheckMatch("pursue_speed == 7.0",       g_fActMoveSpd,   7.0f);

	if (g_iFailures > 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning_PriestValuesMatchConfig: %d field(s) failed", g_iFailures);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xPriestTuningTest = {
	"Test_P1Tuning_PriestValuesMatchConfig",
	&Setup_P1Tuning_PriestValuesMatchConfig,
	&Step_P1Tuning_PriestValuesMatchConfig,
	&Verify_P1Tuning_PriestValuesMatchConfig,
	180,
	// m_bRequiresGraphics: GameLevel's priest entity attaches Priest_Behaviour
	// through the scene's full authoring chain (model load + script attach +
	// AIAgent registration). On a fresh CI checkout where Assets/Meshes/ is
	// gitignored, the priest entity may not fully spawn -- tag so headless CI
	// skips and we keep coverage on local windowed runs.
	true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPriestTuningTest);

#endif // ZENITH_INPUT_SIMULATOR
