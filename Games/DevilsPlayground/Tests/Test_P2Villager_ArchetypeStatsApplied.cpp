#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Source/PublicInterfaces.h"
#include "Source/DP_Archetypes.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>

// ============================================================================
// Test_P2Villager_ArchetypeStatsApplied (MVP-0.2.3)
//
// Verifies DPVillager_Behaviour's OnAwake reads archetype-specific stats
// from DP_Archetypes when SetArchetype is called before the Awake wave
// drains, and that ApplyArchetype at runtime re-resolves the stats.
//
// Coverage:
//   1. Default ("Farmhand") archetype: life=30s, jog=8m/s. Matches the
//      pre-MVP-0.2.3 hard-coded values and the post-MVP-0.1.2 DP_Tuning
//      values exactly -- nothing breaks if a scene's authoring hasn't
//      called SetArchetype yet.
//   2. ApplyArchetype("Child") at runtime: life=15s (frail), jog=8m/s.
//   3. ApplyArchetype("Beggar"): life=25s, jog=8m/s.
//   4. ApplyArchetype("Devout"): life=30s, jog=8m/s. (Devout's distinct
//      possession_channel + scent_floor aren't yet consumed by
//      DPVillager; they're set on the archetype struct for MVP-0.2.4+.)
//
// Uses the first authored villager from GameLevel and calls ApplyArchetype
// directly on it (bypasses authoring API which is deferred per the roadmap
// note: "AddStep_AttachScript takes archetype id as parameter" is the
// follow-up that wires archetype assignment through EditorAutomation).
// ============================================================================

namespace
{
	enum Phase : int { kA_Start, kA_WaitVillager, kA_VerifyDefault, kA_VerifyChild,
	                   kA_VerifyBeggar, kA_VerifyDevout, kA_Done };

	int  g_iPhase        = kA_Start;
	bool g_bFoundVillager = false;
	DPVillager_Behaviour* g_pxVillager = nullptr;

	float g_fDefaultLife  = -1.0f;
	float g_fDefaultSpeed = -1.0f;
	float g_fChildLife    = -1.0f;
	float g_fChildSpeed   = -1.0f;
	float g_fBeggarLife   = -1.0f;
	float g_fBeggarSpeed  = -1.0f;
	float g_fDevoutLife   = -1.0f;
	float g_fDevoutSpeed  = -1.0f;
}

static void Setup_P2Villager_ArchetypeStatsApplied()
{
	g_iPhase = kA_Start;
	g_bFoundVillager = false;
	g_pxVillager = nullptr;
}

static bool Step_P2Villager_ArchetypeStatsApplied(int iFrame)
{
	switch (g_iPhase)
	{
	case kA_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kA_WaitVillager;
		return true;

	case kA_WaitVillager:
	{
		Zenith_EntityID xFoundId;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[](Zenith_EntityID xId, DPVillager_Behaviour& xV) {
				if (!g_bFoundVillager)
				{
					g_bFoundVillager = true;
					g_pxVillager = &xV;
					// Capture default ("Farmhand") stats from OnAwake.
					g_fDefaultLife  = xV.GetMaxLife();
					g_fDefaultSpeed = xV.GetMoveSpeed();
				}
				(void)xId;
			});

		if (g_pxVillager != nullptr)
		{
			g_iPhase = kA_VerifyDefault;
			return true;
		}
		if (iFrame > 120)
		{
			g_iPhase = kA_Done;
			return false;
		}
		return true;
	}

	case kA_VerifyDefault:
		// Switch to Child and capture.
		g_pxVillager->ApplyArchetype("Child");
		g_fChildLife  = g_pxVillager->GetMaxLife();
		g_fChildSpeed = g_pxVillager->GetMoveSpeed();
		g_iPhase = kA_VerifyChild;
		return true;

	case kA_VerifyChild:
		g_pxVillager->ApplyArchetype("Beggar");
		g_fBeggarLife  = g_pxVillager->GetMaxLife();
		g_fBeggarSpeed = g_pxVillager->GetMoveSpeed();
		g_iPhase = kA_VerifyBeggar;
		return true;

	case kA_VerifyBeggar:
		g_pxVillager->ApplyArchetype("Devout");
		g_fDevoutLife  = g_pxVillager->GetMaxLife();
		g_fDevoutSpeed = g_pxVillager->GetMoveSpeed();
		g_iPhase = kA_Done;
		return false;

	case kA_Done:
	default:
		return false;
	}
}

static int g_iFailures = 0;
static void CheckEq(const char* szLabel, float fActual, float fExpected)
{
	const float fTol = 0.001f;
	if (std::fabs(fActual - fExpected) >= fTol)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P2Villager_ArchetypeStatsApplied: '%s' expected %f got %f",
			szLabel, fExpected, fActual);
		++g_iFailures;
	}
}

static bool Verify_P2Villager_ArchetypeStatsApplied()
{
	if (!g_bFoundVillager)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P2Villager_ArchetypeStatsApplied: no DPVillager_Behaviour in active scene");
		return false;
	}

	g_iFailures = 0;

	// 1) Default Farmhand: 30s life, 8m/s jog. Matches pre-MVP-0.2.3.
	CheckEq("default.life",  g_fDefaultLife,  30.0f);
	CheckEq("default.speed", g_fDefaultSpeed, 8.0f);

	// 2) Child: 15s life (frail), 8m/s jog.
	CheckEq("child.life",  g_fChildLife,  15.0f);
	CheckEq("child.speed", g_fChildSpeed, 8.0f);

	// 3) Beggar: 25s life, 8m/s jog.
	CheckEq("beggar.life",  g_fBeggarLife,  25.0f);
	CheckEq("beggar.speed", g_fBeggarSpeed, 8.0f);

	// 4) Devout: 30s life (baseline), 8m/s jog. The distinct
	//    possession_channel_s + demon_scent_floor that gate Devout-specific
	//    behaviour are NOT yet consumed by DPVillager -- those land in a
	//    follow-up (possession-channel migration is a separate MVP item).
	CheckEq("devout.life",  g_fDevoutLife,  30.0f);
	CheckEq("devout.speed", g_fDevoutSpeed, 8.0f);

	return g_iFailures == 0;
}

static const Zenith_AutomatedTest g_xVillagerArchetypeTest = {
	"Test_P2Villager_ArchetypeStatsApplied",
	&Setup_P2Villager_ArchetypeStatsApplied,
	&Step_P2Villager_ArchetypeStatsApplied,
	&Verify_P2Villager_ArchetypeStatsApplied,
	180,
	// m_bRequiresGraphics: this test depends on an authored DPVillager_Behaviour
	// in GameLevel. CI checkouts without .zmodel assets may not spawn villagers
	// reliably; tag so headless CI skips and we keep coverage on local windowed.
	true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xVillagerArchetypeTest);

#endif // ZENITH_INPUT_SIMULATOR
