#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>

// ============================================================================
// Test_P1Villager_TuningMigration (MVP-0.1.2)
//
// Verifies that DPVillager_Behaviour reads m_fMaxLife from
//   possession.life_timer_default_s and m_fMoveSpeed from
//   movement.jog_speed_mps in OnAwake (via DP_Tuning::Get<float>),
// rather than the previous hard-coded 30.0f / 8.0f class-body initializers.
//
// Proof:
//   1. Load GameLevel (which has 17 authored villagers).
//   2. Wait until at least one DPVillager_Behaviour is present + awoken.
//   3. Compare its GetMaxLife() / GetMoveSpeed() against the values
//      DP_Tuning returns for the same keys. Equality (not band) -- the
//      migration's contract is exact propagation of the JSON value.
//
// Defence-in-depth: also compare against the ratified Tuning.json constants
// (30.0 s life timer, 8.0 m/s jog speed) so a future tuner who edits BOTH
// the JSON and the behaviour to the same wrong value still trips the test.
// ============================================================================

namespace
{
	enum Phase : int { kS_Start, kS_WaitVillager, kS_Verify, kS_Done };

	int   g_iPhase                  = kS_Start;
	bool  g_bFoundVillager          = false;
	float g_fActualMaxLife          = -1.0f;
	float g_fActualMoveSpeed        = -1.0f;
	float g_fExpectedFromTuningLife = -1.0f;
	float g_fExpectedFromTuningSpd  = -1.0f;
}

static void Setup_P1Villager_TuningMigration()
{
	g_iPhase                  = kS_Start;
	g_bFoundVillager          = false;
	g_fActualMaxLife          = -1.0f;
	g_fActualMoveSpeed        = -1.0f;
	g_fExpectedFromTuningLife = -1.0f;
	g_fExpectedFromTuningSpd  = -1.0f;
}

static bool Step_P1Villager_TuningMigration(int iFrame)
{
	switch (g_iPhase)
	{
	case kS_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kS_WaitVillager;
		return true;

	case kS_WaitVillager:
	{
		// Wait until a DPVillager_Behaviour shows up. OnAwake fires when the
		// script attaches; by the time DP_Query finds it, the tuning reads
		// have already populated m_fMaxLife and m_fMoveSpeed.
		Zenith_EntityID xFoundId;
		DPVillager_Behaviour* pxFound = nullptr;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFoundId, &pxFound](Zenith_EntityID xId, DPVillager_Behaviour& xVillager) {
				if (!xFoundId.IsValid())
				{
					xFoundId = xId;
					pxFound  = &xVillager;
				}
			});

		if (pxFound != nullptr)
		{
			g_bFoundVillager          = true;
			g_fActualMaxLife          = pxFound->GetMaxLife();
			g_fActualMoveSpeed        = pxFound->GetMoveSpeed();
			g_fExpectedFromTuningLife = DP_Tuning::Get<float>("possession.life_timer_default_s");
			g_fExpectedFromTuningSpd  = DP_Tuning::Get<float>("movement.jog_speed_mps");
			g_iPhase                  = kS_Verify;
			return true;
		}

		// Bail out after a reasonable wait if no villager appears (asset
		// missing on this checkout / scene failed to author).
		if (iFrame > 120)
		{
			g_iPhase = kS_Done;
			return false;
		}
		return true;
	}

	case kS_Verify:
		g_iPhase = kS_Done;
		return false;

	case kS_Done:
	default:
		return false;
	}
}

static bool Verify_P1Villager_TuningMigration()
{
	if (!g_bFoundVillager)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Villager_TuningMigration: no DPVillager_Behaviour found "
			"in active scene after 120 frames");
		return false;
	}

	const float fTol = 0.001f;

	// 1. Behaviour's max-life matches DP_Tuning's returned value.
	if (std::fabs(g_fActualMaxLife - g_fExpectedFromTuningLife) >= fTol)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Villager_TuningMigration: GetMaxLife()=%f != DP_Tuning"
			"::Get<float>(possession.life_timer_default_s)=%f",
			g_fActualMaxLife, g_fExpectedFromTuningLife);
		return false;
	}

	// 2. Behaviour's move-speed matches DP_Tuning's returned value.
	if (std::fabs(g_fActualMoveSpeed - g_fExpectedFromTuningSpd) >= fTol)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Villager_TuningMigration: GetMoveSpeed()=%f != DP_Tuning"
			"::Get<float>(movement.jog_speed_mps)=%f",
			g_fActualMoveSpeed, g_fExpectedFromTuningSpd);
		return false;
	}

	// 3. Defence-in-depth: ratified Tuning.json constants (catches the
	//    "tuner edits both the JSON and the behaviour to the same wrong
	//    value" case).
	if (std::fabs(g_fActualMaxLife - 30.0f) >= fTol)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Villager_TuningMigration: GetMaxLife()=%f != ratified "
			"30.0f (possession.life_timer_default_s in Tuning.json)",
			g_fActualMaxLife);
		return false;
	}
	if (std::fabs(g_fActualMoveSpeed - 8.0f) >= fTol)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Villager_TuningMigration: GetMoveSpeed()=%f != ratified "
			"8.0f (movement.jog_speed_mps in Tuning.json)",
			g_fActualMoveSpeed);
		return false;
	}

	return true;
}

static const Zenith_AutomatedTest g_xVillagerTuningMigrationTest = {
	"Test_P1Villager_TuningMigration",
	&Setup_P1Villager_TuningMigration,
	&Step_P1Villager_TuningMigration,
	&Verify_P1Villager_TuningMigration,
	180, // m_iMaxFrames -- generous; the test settles within the first
	     // few frames after scene-load.
	// m_bRequiresGraphics: this test depends on DPVillager attaching to an
	// authored villager entity in GameLevel. The villager's authoring chain
	// (LoadModel + SET_MODEL_MATERIAL) needs the .zmodel asset tree which is
	// gitignored, so on a fresh CI checkout no villager script will spawn.
	// Tag accordingly so the harness skips the test under --headless until
	// asset provisioning lands (Q-2026-05-12-007 follow-up).
	true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xVillagerTuningMigrationTest);

#endif // ZENITH_INPUT_SIMULATOR
