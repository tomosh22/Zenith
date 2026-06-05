#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "CityBuilder/Components/CB_CityManager_Behaviour.h"

// ============================================================================
// CB_Boot — Phase-1 autonomous gate.
//
// Verifies the City scene boots end-to-end: the CityManager behaviour reached
// OnStart, OnUpdate is ticking, and a main camera resolves. Logic-only (no GPU
// dependency), so it runs in the headless CI pass. The harness boot ordering
// loads scene 0 (City) before Setup runs, so the test only has to observe.
// ============================================================================

static void Setup_CB_Boot()
{
	// Scene 0 (City) is already loaded + in Playing mode by the harness.
}

static bool Step_CB_Boot(int iFrame)
{
	// Tick a few frames so OnStart and several OnUpdate calls fire.
	return iFrame < 5;
}

static bool Verify_CB_Boot()
{
	bool bOk = true;

	if (!CB_CityManager_Behaviour::WasStarted())
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Boot: CityManager OnStart did not fire");
		bOk = false;
	}
	if (CB_CityManager_Behaviour::GetUpdateCount() == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Boot: CityManager OnUpdate never ticked");
		bOk = false;
	}
	if (Zenith_GetMainCameraAcrossScenes() == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "CB_Boot: no main camera resolved");
		bOk = false;
	}
	return bOk;
}

static const Zenith_AutomatedTest g_xCBBootTest = {
	"CB_Boot",
	&Setup_CB_Boot,
	&Step_CB_Boot,
	&Verify_CB_Boot,
	120,
	false   // logic-only — runs headless
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xCBBootTest);

#endif // ZENITH_INPUT_SIMULATOR
