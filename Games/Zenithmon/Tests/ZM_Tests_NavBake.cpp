#include "Zenith.h"

// ============================================================================
// ZM_Tests_NavBake -- S7 item 3 SC1b boot units for the Dawnmere navmesh bake
// constants (Games/Zenithmon/Source/Nav/ZM_NavBake).
//
// These pin the two values that are LOAD-BEARING ACROSS FILES and that nothing
// else would catch:
//   * the asset-ref STRING, which the authored Dawnmere.zscen carries and the
//     runtime resolves. A typo here does not fail to compile -- it fails at
//     runtime, hours later, as a missing navmesh.
//   * the bake CELL SIZE, which decides the committed asset's resolution and
//     therefore the polygon band ZM_NavmeshAsset_Test asserts.
//
// Both are spelled out as LITERALS here, never derived from the constant they
// pin, so a change to either has to be a deliberate edit in two places.
//
// Pure and headless: no scene, no disk read, no GPU.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Zenithmon/Source/Nav/ZM_NavBake.h"

#include <cstring>
#include <string>

ZENITH_TEST(ZM_Nav, DawnmereNavmeshAssetRefSpelling)
{
	// The exact string the scene serializes and the runtime resolves.
	ZENITH_ASSERT_STREQ(szZM_DAWNMERE_NAVMESH_ASSET_REF, "game:Navmesh/Dawnmere.znavmesh",
		"The Dawnmere navmesh asset ref is wire-visible in the committed scene");

	// ...and it must be built from the engine's extension constant, not a
	// hand-typed ".znavmesh", so an engine-side rename cannot silently diverge.
	const std::string strRef(szZM_DAWNMERE_NAVMESH_ASSET_REF);
	const std::string strExt(ZENITH_NAVMESH_EXT);
	ZENITH_ASSERT_TRUE(strRef.size() > strExt.size(), "The ref is longer than its extension");
	ZENITH_ASSERT_EQ(strRef.compare(strRef.size() - strExt.size(), strExt.size(), strExt), 0,
		"The ref ends with ZENITH_NAVMESH_EXT");

	// The `game:` prefix is what routes resolution through the game assets root
	// (and therefore what makes --assets-root packaging work).
	ZENITH_ASSERT_EQ(strRef.compare(0, 5, "game:"), 0, "The ref is game-scoped");
}

ZENITH_TEST(ZM_Nav, DawnmereNavmeshBakeCellSize)
{
	// 16 m is SC1's evaluated resolution: it lands the polygon count inside the
	// 3969..4489 band ZM_NavmeshAsset_Test asserts, and stays far clear of the
	// generator's iMaxDim=1024 voxel-grid clamp.
	ZENITH_ASSERT_EQ_FLOAT(fZM_DAWNMERE_NAVMESH_CELL_SIZE, 16.0f, 0.0001f,
		"The Dawnmere bake cell size decides the committed asset's resolution");
}

ZENITH_TEST(ZM_Nav, DawnmereNavmeshBakePathIsAbsoluteAndTyped)
{
	const char* szPath = ZM_GetDawnmereNavmeshBakePath();
	ZENITH_ASSERT_NOT_NULL(szPath, "The bake destination is always resolvable");

	const std::string strPath(szPath);
	ZENITH_ASSERT_TRUE(strPath.size() > 0, "The bake destination is non-empty");

	// The bake writes into the game's Assets tree, under the navmesh folder, with
	// the engine's extension -- the three properties the .gitignore re-include
	// (`!**/*.znavmesh`) and the committed-asset story depend on.
	const std::string strExt(ZENITH_NAVMESH_EXT);
	ZENITH_ASSERT_EQ(strPath.compare(strPath.size() - strExt.size(), strExt.size(), strExt), 0,
		"The bake destination carries the navmesh extension");
	ZENITH_ASSERT_TRUE(strPath.find("Navmesh") != std::string::npos,
		"The bake destination lives under the Navmesh asset folder");
	ZENITH_ASSERT_TRUE(strPath.find("Dawnmere") != std::string::npos,
		"The bake destination is named for its scene");

#ifdef ZENITH_ANDROID
	// Android inverts the absoluteness half of this contract, by design.
	// GAME_ASSETS_DIR is compiled in as "" there (Sharpmake_Games.cs) because
	// assets are bundled into the APK and AAssetManager addresses them by a
	// path RELATIVE to the assets root -- any prefix would break every load.
	// The typed/foldered/named assertions above are the load-bearing ones and
	// still hold; the test name reflects the desktop case.
	ZENITH_ASSERT_TRUE(!strPath.empty() && strPath.find(':') == std::string::npos && strPath[0] != '/',
		"The bake destination is APK-relative on Android");
#else
	// An absolute path (GAME_ASSETS_DIR is compiled in), so the bake does not
	// depend on the working directory the exe was launched from.
	const bool bAbsolute = !strPath.empty()
		&& (strPath.find(':') != std::string::npos || strPath[0] == '/');
	ZENITH_ASSERT_TRUE(bAbsolute, "The bake destination is absolute");
#endif
}
