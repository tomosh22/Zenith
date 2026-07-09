#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_Boot_Test -- proves the game boots the FrontEnd scene and attaches
// ZM_GameComponent. The S0 hello-automated-test; richer scene/flow tests land
// per stage (see Docs/TestPlan.md).
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameComponent.h"

namespace
{
	ZM_GameComponent* FindZMGame()
	{
		ZM_GameComponent* pxFound = nullptr;
		g_xEngine.Scenes().QueryAllScenes<ZM_GameComponent>().ForEach(
			[&pxFound](Zenith_EntityID, ZM_GameComponent& xGame)
			{
				if (pxFound == nullptr) { pxFound = &xGame; }
			});
		return pxFound;
	}

	bool g_bZMBootOk = false;
}

static void Setup_ZMBoot()
{
	g_bZMBootOk = false;
}

static bool Step_ZMBoot(int iFrame)
{
	// The boot-authored FrontEnd scene attaches the game component to
	// GameManager; once it resolves, the game exe has booted its scene
	// successfully.
	if (FindZMGame() != nullptr)
	{
		g_bZMBootOk = true;
		return false;
	}
	return iFrame < 300;
}

static bool Verify_ZMBoot()
{
	if (!g_bZMBootOk)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[ZM_Boot] game component was not found after boot");
	}
	return g_bZMBootOk;
}

static const Zenith_AutomatedTest g_xZMBootTest = {
	"ZM_Boot_Test",
	&Setup_ZMBoot,
	&Step_ZMBoot,
	&Verify_ZMBoot,
	/*maxFrames*/ 600,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMBootTest);

#endif // ZENITH_INPUT_SIMULATOR
