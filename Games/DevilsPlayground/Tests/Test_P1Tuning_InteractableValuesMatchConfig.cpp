#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPDoor_Behaviour.h"
#include "Components/DPDoubleDoor_Behaviour.h"
#include "Components/DPChest_Behaviour.h"
#include "Components/DummyNoiseMachine_Behaviour.h"

#include <cmath>
#include <string>

// ============================================================================
// Test_P1Tuning_InteractableValuesMatchConfig (MVP-0.1.4)
//
// Regression guard against the interactable-tuning drift fixed in MVP-0.1.4.
// Each DPInteractable subclass now reads its timing/proximity constants
// from DP_Tuning in OnAwake. This test loads the procgen ProcLevel scene
// (build index 1), locates one instance of DPDoor / DPChest /
// DummyNoiseMachine spawned by the procgen bootstrap, and asserts the
// migrated fields match Tuning.json.
//
// DPDoubleDoor is the one variant the procgen bootstrap does NOT spawn
// (procgen generates only single-leaf doors today). To preserve coverage,
// the test constructs a synthetic DPDoubleDoor entity at runtime so its
// OnAwake fires inside the active scene.
//
// Coverage classes (mirrors MVP-0.1.3's priest tuning test):
//   1. Exact equality vs DP_Tuning::Get<float>() (catches "tuner forgot
//      to delete the hardcoded fallback").
//   2. Exact equality vs the ratified Tuning.json constants (catches
//      "tuner edits both config and fallback to the same wrong value").
// ============================================================================

namespace
{
	enum Phase : int { kI_Start, kI_WaitInteractables, kI_SpawnDoubleDoor, kI_CaptureAll, kI_Done };

	int  g_iPhase            = kI_Start;
	bool g_bFoundDoor        = false;
	bool g_bFoundDoubleDoor  = false;
	bool g_bFoundChest       = false;
	bool g_bFoundNoise       = false;

	float g_fDoorOpenYaw       = -1.0f;
	float g_fDoorOpenDuration  = -1.0f;
	float g_fDDOpenYaw         = -1.0f;
	float g_fDDOpenDuration    = -1.0f;
	float g_fChestOpenDuration = -1.0f;
	float g_fNoiseLoudness     = -1.0f;
	float g_fNoiseRadius       = -1.0f;
	float g_fInteractRadius    = -1.0f;
}

static void Setup_P1Tuning_InteractableValuesMatchConfig()
{
	g_iPhase = kI_Start;
	g_bFoundDoor = g_bFoundDoubleDoor = g_bFoundChest = g_bFoundNoise = false;
}

static bool Step_P1Tuning_InteractableValuesMatchConfig(int iFrame)
{
	switch (g_iPhase)
	{
	case kI_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kI_WaitInteractables;
		return true;

	case kI_WaitInteractables:
	{
		// ProcLevel's bootstrap runs DPProcLevel::Generate inside OnAwake;
		// the procgen-spawned DPDoor / DPChest / DummyNoiseMachine entities
		// have their script OnAwake fired during scene boot. Give the
		// bootstrap a handful of frames to populate the scene, then probe.
		if (iFrame < 30) return true;

		DP_Query::ForEachScriptInActiveScene<DPDoor_Behaviour>(
			[](Zenith_EntityID, DPDoor_Behaviour& xD) {
				if (!g_bFoundDoor) {
					g_bFoundDoor         = true;
					g_fDoorOpenYaw       = xD.GetOpenYaw();
					g_fDoorOpenDuration  = xD.GetOpenDuration();
					g_fInteractRadius    = xD.GetInteractRadius();
				}
			});
		DP_Query::ForEachScriptInActiveScene<DPChest_Behaviour>(
			[](Zenith_EntityID, DPChest_Behaviour& xC) {
				if (!g_bFoundChest) {
					g_bFoundChest        = true;
					g_fChestOpenDuration = xC.GetOpenDuration();
				}
			});
		DP_Query::ForEachScriptInActiveScene<DummyNoiseMachine_Behaviour>(
			[](Zenith_EntityID, DummyNoiseMachine_Behaviour& xN) {
				if (!g_bFoundNoise) {
					g_bFoundNoise     = true;
					g_fNoiseLoudness  = xN.GetLoudness();
					g_fNoiseRadius    = xN.GetRadius();
				}
			});

		if (g_bFoundDoor && g_bFoundChest && g_bFoundNoise)
		{
			g_iPhase = kI_SpawnDoubleDoor;
			return true;
		}

		// Bail out if the procgen scene didn't materialise these behaviours
		// within ~5 s of load -- procgen regressions should fail this test
		// rather than hang.
		if (iFrame > 300)
		{
			g_iPhase = kI_Done;
			return false;
		}
		return true;
	}

	case kI_SpawnDoubleDoor:
	{
		// Procgen doesn't currently spawn DPDoubleDoor (only single-leaf
		// doors). Build one at runtime in the active scene so its OnAwake
		// fires + reads the DP_Tuning constants we want to verify.
		Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xActive);
		if (pxScene == nullptr) { g_iPhase = kI_Done; return false; }

		Zenith_Entity xDoor(pxScene, std::string("TuningTestDoubleDoor"));
		xDoor.AddComponent<Zenith_ScriptComponent>()
		     .AddScript<DPDoubleDoor_Behaviour>();

		g_iPhase = kI_CaptureAll;
		return true;
	}

	case kI_CaptureAll:
	{
		// The synthetic DPDoubleDoor's OnAwake has fired by now (script
		// attach pumps OnAwake before the next frame begins). Pick up its
		// tuning values.
		DP_Query::ForEachScriptInActiveScene<DPDoubleDoor_Behaviour>(
			[](Zenith_EntityID, DPDoubleDoor_Behaviour& xD) {
				if (!g_bFoundDoubleDoor) {
					g_bFoundDoubleDoor = true;
					g_fDDOpenYaw       = xD.GetOpenYaw();
					g_fDDOpenDuration  = xD.GetOpenDuration();
				}
			});

		g_iPhase = kI_Done;
		return false;
	}

	case kI_Done:
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
			"Test_P1Tuning_InteractableValuesMatchConfig: %s actual=%f expected=%f",
			szLabel, fActual, fExpected);
		++g_iFailures;
	}
}

static bool Verify_P1Tuning_InteractableValuesMatchConfig()
{
	g_iFailures = 0;

	if (!g_bFoundDoor)        { Zenith_Log(LOG_CATEGORY_UNITTEST, "no DPDoor_Behaviour found");        ++g_iFailures; }
	if (!g_bFoundDoubleDoor)  { Zenith_Log(LOG_CATEGORY_UNITTEST, "no DPDoubleDoor_Behaviour found");  ++g_iFailures; }
	if (!g_bFoundChest)       { Zenith_Log(LOG_CATEGORY_UNITTEST, "no DPChest_Behaviour found");       ++g_iFailures; }
	if (!g_bFoundNoise)       { Zenith_Log(LOG_CATEGORY_UNITTEST, "no DummyNoiseMachine_Behaviour found"); ++g_iFailures; }
	if (g_iFailures > 0) return false;

	// 1) Match DP_Tuning's returned values.
	CheckMatch("base proximity radius vs tuning",     g_fInteractRadius,    DP_Tuning::Get<float>("interactables.default_proximity_radius_m"));
	CheckMatch("door yaw vs tuning",                  g_fDoorOpenYaw,       DP_Tuning::Get<float>("interactables.door_open_yaw_deg"));
	CheckMatch("door duration vs tuning",             g_fDoorOpenDuration,  DP_Tuning::Get<float>("interactables.door_open_duration_s"));
	CheckMatch("double door yaw vs tuning",           g_fDDOpenYaw,         DP_Tuning::Get<float>("interactables.double_door_open_yaw_deg"));
	CheckMatch("double door duration vs tuning",      g_fDDOpenDuration,    DP_Tuning::Get<float>("interactables.double_door_open_duration_s"));
	CheckMatch("chest duration vs tuning",            g_fChestOpenDuration, DP_Tuning::Get<float>("interactables.chest_open_duration_s"));
	CheckMatch("noise machine loudness vs tuning",    g_fNoiseLoudness,     DP_Tuning::Get<float>("interactables.noise_machine_loudness"));
	CheckMatch("noise machine radius vs tuning",      g_fNoiseRadius,       DP_Tuning::Get<float>("interactables.noise_machine_radius_m"));

	// 2) Defence-in-depth: ratified Tuning.json constants.
	CheckMatch("base proximity == 2.0",        g_fInteractRadius,    2.0f);
	CheckMatch("door yaw == 90.0",             g_fDoorOpenYaw,       90.0f);
	CheckMatch("door duration == 0.4",         g_fDoorOpenDuration,  0.4f);
	CheckMatch("double door yaw == 80.0",      g_fDDOpenYaw,         80.0f);
	CheckMatch("double door duration == 0.5",  g_fDDOpenDuration,    0.5f);
	CheckMatch("chest duration == 0.8",        g_fChestOpenDuration, 0.8f);
	CheckMatch("noise loudness == 1.0",        g_fNoiseLoudness,     1.0f);
	// 2026-05-23: 20.0 -> 19.0 (broke Heretic 100% wins on the canonical
	// 10-seed matrix while keeping seed 250000 winnable; see DecisionLog.md).
	CheckMatch("noise radius == 19.0",         g_fNoiseRadius,       19.0f);

	if (g_iFailures > 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_P1Tuning_InteractableValuesMatchConfig: %d field(s) failed", g_iFailures);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xInteractableTuningTest = {
	"Test_P1Tuning_InteractableValuesMatchConfig",
	&Setup_P1Tuning_InteractableValuesMatchConfig,
	&Step_P1Tuning_InteractableValuesMatchConfig,
	&Verify_P1Tuning_InteractableValuesMatchConfig,
	300,
	// m_bRequiresGraphics: this test depends on procgen-spawned interactable
	// entities in ProcLevel having model load + script attach succeed. CI
	// checkouts without .zmodel assets skip via headless skip-list; windowed
	// runs exercise the assertions.
	true
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xInteractableTuningTest);

#endif // ZENITH_INPUT_SIMULATOR
