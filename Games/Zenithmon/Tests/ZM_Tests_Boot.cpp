#include "Zenith.h"

// ============================================================================
// ZM_Tests_Boot -- S0 hello unit tests. Run at engine boot (ZENITH_TESTING is
// unconditional) before the initial scene loads. This TU compiles directly
// into the game exe, so its static test registrars always run (the dead-strip
// hazard applies only to static-library members).
//
// Real suites land per stage: ZM_Tests_Data (S1), ZM_Tests_Battle (S2), ...
// -- see Docs/TestPlan.md.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Core/Zenith_ProjectHooks.h"
#include "Zenithmon/Components/ZM_GameComponent.h"

ZENITH_TEST(ZM_Boot, ProjectNameIsZenithmon)
{
	ZENITH_ASSERT_STREQ(Project_GetName(), "Zenithmon");
}

ZENITH_TEST(ZM_Boot, GameAssetsDirectoryIsNonEmpty)
{
	const char* szDir = Project_GetGameAssetsDirectory();
	ZENITH_ASSERT_NOT_NULL(szDir);
	if (szDir == nullptr)
	{
		return;
	}
#ifdef ZENITH_ANDROID
	// Inverted on Android BY DESIGN: GAME_ASSETS_DIR is compiled in as ""
	// (Sharpmake_Games.cs) because assets are bundled into the APK and
	// AAssetManager addresses them by a path RELATIVE to the assets root, so
	// any prefix would break every load. The test name reflects the desktop
	// contract asserted below.
	ZENITH_ASSERT_TRUE(szDir[0] == '\0', "GAME_ASSETS_DIR must be empty on Android (APK-relative asset paths)");
#else
	ZENITH_ASSERT_TRUE(szDir[0] != '\0', "GAME_ASSETS_DIR must not be empty");
#endif
}
