#include "UnitTests/Zenith_UnitTests.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_AnimationAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "AssetHandling/Zenith_FontAsset.h"
#include "Prefab/Zenith_Prefab.h"

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

// ============================================================================
// Unified asset-loader tests (Workstream A)
//
// The ten per-type registry loaders were collapsed onto two templates
// (LoadAssetGeneric / LoadAssetViaStaticFactory) driven by a
// Zenith_AssetLoadTraits<T> trait. These pin the branch logic that moved into
// the templates — exercised through the PUBLIC Get<T>()/Create<T>() facade, so
// no on-disk fixtures or GPU are needed (an empty/missing path never reaches the
// GPU-upload step). Created procedural assets are ForceUnload'd by their
// generated path so the live registry the suite runs inside is left undisturbed.
//
// These bodies use only public APIs, so they live directly in the test function
// (no Zenith_UnitTests member needed) and a templated helper carries the asserts
// (ZENITH_ASSERT_* resolve to a Zenith_TestRunner singleton call, not `this`).
// ============================================================================

namespace
{
	// Create<T>() routes through the loader's empty-path branch (CreateInternal
	// calls the loader with ""), so this exercises that branch for every type and
	// confirms the result is a procedural asset. It is then force-unloaded by its
	// generated path so the live registry the suite runs inside is left undisturbed.
	template<typename T>
	void CheckCreateProcedural()
	{
		T* pxAsset = Zenith_AssetRegistry::Create<T>();
		ZENITH_ASSERT_NOT_NULL(pxAsset, "Create<T>() must return a procedural asset");
		if (pxAsset != nullptr)
		{
			ZENITH_ASSERT_TRUE(pxAsset->IsProcedural(), "Create<T>() asset must carry a procedural:// path");
			Zenith_AssetRegistry::ForceUnload(pxAsset->GetPath());
		}
	}

	// Get<T>() on a non-existent file must fail to null — member-contract types
	// via new -> LoadFromFile FILE_NOT_FOUND -> delete; static-factory types via
	// the Result<T*> error unwrap.
	template<typename T>
	void CheckMissingFileNull(const char* szPath)
	{
		ZENITH_ASSERT_NULL(Zenith_AssetRegistry::Get<T>(szPath),
			"Get<T>() on a missing file must return null");
	}
}

ZENITH_TEST(AssetLoader, CreateProceduralAllTypes)
{
	CheckCreateProcedural<Zenith_TextureAsset>();
	CheckCreateProcedural<Zenith_MaterialAsset>();
	CheckCreateProcedural<Zenith_MeshAsset>();
	CheckCreateProcedural<Zenith_SkeletonAsset>();
	CheckCreateProcedural<Zenith_ModelAsset>();
	CheckCreateProcedural<Zenith_AnimationAsset>();
	CheckCreateProcedural<Zenith_MeshGeometryAsset>();
	CheckCreateProcedural<Zenith_FontAsset>();
	CheckCreateProcedural<Zenith_Prefab>();
}

ZENITH_TEST(AssetLoader, GetProceduralPathRejectedForGuardedTypes)
{
	// Animation + MeshGeometry set kGuardProcedural=true: a procedural:// load path
	// returns INVALID_ARGUMENT from the loader, so Get<T> resolves to null. This is
	// the single behavioural knob the trait encodes — lock it down.
	ZENITH_ASSERT_NULL(Zenith_AssetRegistry::Get<Zenith_AnimationAsset>("procedural://__loadertest__"),
		"Animation must reject a procedural:// load path");
	ZENITH_ASSERT_NULL(Zenith_AssetRegistry::Get<Zenith_MeshGeometryAsset>("procedural://__loadertest__"),
		"MeshGeometry must reject a procedural:// load path");
}

ZENITH_TEST(AssetLoader, GetMissingFileReturnsNull)
{
	// Both contracts must fail cleanly to null on a missing file: member-contract
	// (Texture/Material/Font) and static-factory (Mesh/Skeleton/Model).
	CheckMissingFileNull<Zenith_TextureAsset>("game:__loader_missing__.ztxtr");
	CheckMissingFileNull<Zenith_MaterialAsset>("game:__loader_missing__.zmtrl");
	CheckMissingFileNull<Zenith_FontAsset>("game:__loader_missing__.zfont");
	CheckMissingFileNull<Zenith_MeshAsset>("game:__loader_missing__.zmesh");
	CheckMissingFileNull<Zenith_SkeletonAsset>("game:__loader_missing__.zskel");
	CheckMissingFileNull<Zenith_ModelAsset>("game:__loader_missing__.zmodel");
}

ZENITH_TEST(AssetLoader, GetEmptyPathReturnsNull)
{
	// Empty path on Get<T> is rejected upstream of the loader (GetInternal),
	// distinct from Create<T>'s empty path (which makes a procedural instance).
	ZENITH_ASSERT_NULL(Zenith_AssetRegistry::Get<Zenith_TextureAsset>(""),
		"Get<T>(\"\") must return null (empty is not procedural-create)");
}

ZENITH_TEST(AssetLoader, ForceUnloadOfFreshCreateRemovesAsset)
{
	// Regression: ForceUnload(pxAsset->GetPath()) passes a reference INTO the asset's
	// own m_strPath. ForceUnloadInternal must copy that key BEFORE deleting the asset
	// — otherwise the subsequent cache Remove() hashes/compares freed memory, a heap
	// corruption that surfaces deferred as an access violation in a later, unrelated
	// free. This exercises exactly that reference-into-the-asset call shape.
	Zenith_TextureAsset* pxTex = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
	ZENITH_ASSERT_NOT_NULL(pxTex, "Create<T>() must return an asset");
	if (pxTex != nullptr)
	{
		const std::string strPath = pxTex->GetPath();   // stable copy for the post-unload assert
		ZENITH_ASSERT_TRUE(Zenith_AssetRegistry::IsLoaded(strPath), "a freshly created asset must be cached");
		Zenith_AssetRegistry::ForceUnload(pxTex->GetPath());   // the reference-into-asset trap
		ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strPath), "ForceUnload must remove the asset from the registry");
	}
}
