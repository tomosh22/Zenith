#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_AssetRegistry.h"

// ============================================================================
// AssetRegistry path normalization tests
//
// Cover the SetGameAssetsDir / SetEngineAssetsDir → NormalizeAssetDirPath
// pipeline. Each test saves the live dir strings at entry and restores them
// at exit so the surrounding live-engine state isn't perturbed. Save/restore
// is inlined per test rather than wrapped in a guard struct because friend
// class access doesn't extend to anonymous-namespace types — only to member
// functions of Zenith_UnitTests.
// ============================================================================

ZENITH_TEST(AssetRegistry, NormalizeBackslashesToForwardSlash) { Zenith_UnitTests::TestAssetRegistryNormalizeBackslashes(); }
void Zenith_UnitTests::TestAssetRegistryNormalizeBackslashes()
{
	const std::string strSavedGame = Zenith_AssetRegistry::s_strGameAssetsDir;

	Zenith_AssetRegistry::SetGameAssetsDir("C:\\foo\\bar\\baz");
	ZENITH_ASSERT_EQ(Zenith_AssetRegistry::s_strGameAssetsDir, std::string("C:/foo/bar/baz"),
		"Backslashes must be converted to forward slashes");

	Zenith_AssetRegistry::s_strGameAssetsDir = strSavedGame;
}

ZENITH_TEST(AssetRegistry, NormalizeStripsTrailingForwardSlash) { Zenith_UnitTests::TestAssetRegistryStripsTrailingForwardSlash(); }
void Zenith_UnitTests::TestAssetRegistryStripsTrailingForwardSlash()
{
	const std::string strSavedGame = Zenith_AssetRegistry::s_strGameAssetsDir;

	Zenith_AssetRegistry::SetGameAssetsDir("game/assets/");
	ZENITH_ASSERT_EQ(Zenith_AssetRegistry::s_strGameAssetsDir, std::string("game/assets"),
		"Trailing forward slash must be stripped");

	Zenith_AssetRegistry::s_strGameAssetsDir = strSavedGame;
}

ZENITH_TEST(AssetRegistry, NormalizeStripsTrailingBackslash) { Zenith_UnitTests::TestAssetRegistryStripsTrailingBackslash(); }
void Zenith_UnitTests::TestAssetRegistryStripsTrailingBackslash()
{
	const std::string strSavedGame = Zenith_AssetRegistry::s_strGameAssetsDir;

	// Backslash is converted to forward slash first, then the trailing
	// forward slash is stripped — the helper handles both the convert
	// and strip in one pass over the same string.
	Zenith_AssetRegistry::SetGameAssetsDir("game\\assets\\");
	ZENITH_ASSERT_EQ(Zenith_AssetRegistry::s_strGameAssetsDir, std::string("game/assets"),
		"Trailing backslash must be normalised then stripped");

	Zenith_AssetRegistry::s_strGameAssetsDir = strSavedGame;
}

ZENITH_TEST(AssetRegistry, NormalizeMixedSeparators) { Zenith_UnitTests::TestAssetRegistryMixedSeparators(); }
void Zenith_UnitTests::TestAssetRegistryMixedSeparators()
{
	const std::string strSavedGame = Zenith_AssetRegistry::s_strGameAssetsDir;

	Zenith_AssetRegistry::SetGameAssetsDir("C:/foo\\bar/baz\\qux");
	ZENITH_ASSERT_EQ(Zenith_AssetRegistry::s_strGameAssetsDir, std::string("C:/foo/bar/baz/qux"),
		"Mixed forward and back slashes must all be normalised to forward slash");

	Zenith_AssetRegistry::s_strGameAssetsDir = strSavedGame;
}

ZENITH_TEST(AssetRegistry, NormalizeEmptyStringNoOp) { Zenith_UnitTests::TestAssetRegistryEmptyStringNoOp(); }
void Zenith_UnitTests::TestAssetRegistryEmptyStringNoOp()
{
	const std::string strSavedGame = Zenith_AssetRegistry::s_strGameAssetsDir;

	Zenith_AssetRegistry::SetGameAssetsDir("");
	ZENITH_ASSERT_TRUE(Zenith_AssetRegistry::s_strGameAssetsDir.empty(),
		"Empty input must remain empty after normalisation");

	Zenith_AssetRegistry::s_strGameAssetsDir = strSavedGame;
}

ZENITH_TEST(AssetRegistry, NormalizeNoChangeNeeded) { Zenith_UnitTests::TestAssetRegistryNoChangeNeeded(); }
void Zenith_UnitTests::TestAssetRegistryNoChangeNeeded()
{
	const std::string strSavedGame = Zenith_AssetRegistry::s_strGameAssetsDir;

	Zenith_AssetRegistry::SetGameAssetsDir("C:/already/normalised");
	ZENITH_ASSERT_EQ(Zenith_AssetRegistry::s_strGameAssetsDir, std::string("C:/already/normalised"),
		"Already-normalised input must round-trip unchanged");

	Zenith_AssetRegistry::s_strGameAssetsDir = strSavedGame;
}

ZENITH_TEST(AssetRegistry, NormalizeSingleSlashStripsToEmpty) { Zenith_UnitTests::TestAssetRegistrySingleSlashStripsToEmpty(); }
void Zenith_UnitTests::TestAssetRegistrySingleSlashStripsToEmpty()
{
	const std::string strSavedGame = Zenith_AssetRegistry::s_strGameAssetsDir;

	// A bare "/" is just a trailing slash — strip it, leaving empty.
	Zenith_AssetRegistry::SetGameAssetsDir("/");
	ZENITH_ASSERT_TRUE(Zenith_AssetRegistry::s_strGameAssetsDir.empty(),
		"A bare slash must strip to empty");

	Zenith_AssetRegistry::s_strGameAssetsDir = strSavedGame;
}

ZENITH_TEST(AssetRegistry, GameAndEngineDirsTrackedSeparately) { Zenith_UnitTests::TestAssetRegistryGameAndEngineDirsSeparate(); }
void Zenith_UnitTests::TestAssetRegistryGameAndEngineDirsSeparate()
{
	const std::string strSavedGame = Zenith_AssetRegistry::s_strGameAssetsDir;
	const std::string strSavedEngine = Zenith_AssetRegistry::s_strEngineAssetsDir;

	Zenith_AssetRegistry::SetGameAssetsDir("game\\dir/");
	Zenith_AssetRegistry::SetEngineAssetsDir("engine\\dir/");
	ZENITH_ASSERT_EQ(Zenith_AssetRegistry::s_strGameAssetsDir, std::string("game/dir"),
		"Game dir normalised independently of engine dir");
	ZENITH_ASSERT_EQ(Zenith_AssetRegistry::s_strEngineAssetsDir, std::string("engine/dir"),
		"Engine dir normalised independently of game dir");

	Zenith_AssetRegistry::s_strGameAssetsDir = strSavedGame;
	Zenith_AssetRegistry::s_strEngineAssetsDir = strSavedEngine;
}
