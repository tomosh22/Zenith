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
		auto xhAsset = Zenith_AssetRegistry::Create<T>();
		T* pxAsset = xhAsset.GetDirect();
		ZENITH_ASSERT_NOT_NULL(pxAsset, "Create<T>() must return a procedural asset");
		if (pxAsset != nullptr)
		{
			ZENITH_ASSERT_TRUE(pxAsset->IsProcedural(), "Create<T>() asset must carry a procedural:// path");
			xhAsset.Clear();
			Zenith_AssetRegistry::ForceUnload(pxAsset->GetPath());
		}
	}

	// Get<T>() on a non-existent file must fail to null — member-contract types
	// via new -> LoadFromFile FILE_NOT_FOUND -> delete; static-factory types via
	// the Result<T*> error unwrap.
	template<typename T>
	void CheckMissingFileNull(const char* szPath)
	{
		ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<T>(szPath),
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
	ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<Zenith_AnimationAsset>("procedural://__loadertest__"),
		"Animation must reject a procedural:// load path");
	ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<Zenith_MeshGeometryAsset>("procedural://__loadertest__"),
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
	ZENITH_ASSERT_NULL(Zenith_AssetRegistry::GetView<Zenith_TextureAsset>(""),
		"Get<T>(\"\") must return null (empty is not procedural-create)");
}

ZENITH_TEST(AssetLoader, ForceUnloadOfFreshCreateRemovesAsset)
{
	// Regression: ForceUnload(pxAsset->GetPath()) passes a reference INTO the asset's
	// own m_strPath. ForceUnloadInternal must copy that key BEFORE deleting the asset
	// — otherwise the subsequent cache Remove() hashes/compares freed memory, a heap
	// corruption that surfaces deferred as an access violation in a later, unrelated
	// free. This exercises exactly that reference-into-the-asset call shape.
	auto xhTex = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
	Zenith_TextureAsset* pxTex = xhTex.GetDirect();
	ZENITH_ASSERT_NOT_NULL(pxTex, "Create<T>() must return an asset");
	if (pxTex != nullptr)
	{
		const std::string strPath = pxTex->GetPath();   // stable copy for the post-unload assert
		ZENITH_ASSERT_TRUE(Zenith_AssetRegistry::IsLoaded(strPath), "a freshly created asset must be cached");
		xhTex.Clear();
		Zenith_AssetRegistry::ForceUnload(pxTex->GetPath());   // the reference-into-asset trap
		ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strPath), "ForceUnload must remove the asset from the registry");
	}
}

// ============================================================================
// Ownership-API tests (Acquire / GetView / Create-returns-handle / get-or-create)
//
// Pin the refcount contract of the handle-returning registry API from the
// AssetHandle ownership overhaul: Create<T>()/Acquire<T>() hand back an OWNING
// handle (AddRef'd UNDER the registry lock — no 0-refcount window a concurrent
// UnloadUnused could reclaim), Create<T>(path) is atomic get-or-create (never
// clobbers), and GetView<T> is a raw non-owning view. All use public APIs + fresh
// procedural assets (unique paths -> deterministic absolute refcounts), so no
// fixtures/GPU; each cleans up so the live registry the suite runs in is undisturbed.
// ============================================================================

ZENITH_TEST(AssetOwnership, CreateReturnsAddRefdHandleNoZeroRefWindow)
{
	MaterialHandle xHandle = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	ZENITH_ASSERT_NOT_NULL(xHandle.GetDirect(), "Create<T>() must return a resolved owning handle");
	ZENITH_ASSERT_EQ(xHandle.GetDirect()->GetRefCount(), 1u,
		"Create<T>() must AddRef to exactly 1 (no 0-refcount window)");

	// The held handle keeps the asset alive across UnloadUnused; clearing it lets go.
	const std::string strPath = xHandle.GetDirect()->GetPath();
	Zenith_AssetRegistry::UnloadUnused();
	ZENITH_ASSERT_TRUE(Zenith_AssetRegistry::IsLoaded(strPath),
		"an asset held by a Create<T>() handle must survive UnloadUnused");
	xHandle.Clear();
	Zenith_AssetRegistry::UnloadUnused();
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strPath),
		"after the last handle is cleared, UnloadUnused reclaims the asset");
}

ZENITH_TEST(AssetOwnership, AcquireHoldsRefAndSurvivesUnloadUnused)
{
	const char* szPath = "procedural://__acquire_ownership_test__";
	{
		MaterialHandle xSeed = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>(szPath);
		ZENITH_ASSERT_TRUE(xSeed.HasPath(), "Create<T>(path) handle must retain the path");
	}   // xSeed released -> refcount 0, but still cached
	ZENITH_ASSERT_TRUE(Zenith_AssetRegistry::IsLoaded(szPath), "asset is still cached at refcount 0");

	MaterialHandle xAcquired = Zenith_AssetRegistry::Acquire<Zenith_MaterialAsset>(szPath);
	ZENITH_ASSERT_NOT_NULL(xAcquired.GetDirect(), "Acquire must resolve the cached asset");
	ZENITH_ASSERT_EQ(xAcquired.GetDirect()->GetRefCount(), 1u, "Acquire must AddRef the asset to 1");
	Zenith_AssetRegistry::UnloadUnused();
	ZENITH_ASSERT_TRUE(Zenith_AssetRegistry::IsLoaded(szPath), "an Acquire'd asset survives UnloadUnused");

	const std::string strPath = szPath;
	xAcquired.Clear();
	Zenith_AssetRegistry::UnloadUnused();
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strPath), "reclaimed after the handle is cleared");
}

ZENITH_TEST(AssetOwnership, CreatePathIsAtomicGetOrCreate)
{
	// Create<T>(path) is get-or-create: a second call with the same path returns the
	// SAME instance (AddRef'd) rather than clobbering it (the old overwrite leaked the
	// previous asset and invalidated live references).
	const char* szPath = "procedural://__getorcreate_ownership_test__";
	MaterialHandle xFirst = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>(szPath);
	ZENITH_ASSERT_NOT_NULL(xFirst.GetDirect(), "first Create<T>(path)");
	xFirst.GetDirect()->SetName("GetOrCreateOriginal");

	MaterialHandle xSecond = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>(szPath);
	ZENITH_ASSERT_EQ(xSecond.GetDirect(), xFirst.GetDirect(),
		"Create<T>(path) must return the existing instance, not clobber it");
	ZENITH_ASSERT_EQ(xFirst.GetDirect()->GetRefCount(), 2u,
		"two owning handles to the same path -> refcount 2 (no leak, no clobber)");
	ZENITH_ASSERT_EQ(Zenith_AssetRegistry::GetView<Zenith_MaterialAsset>(szPath), xFirst.GetDirect(),
		"the path is retained and re-findable via GetView");

	const std::string strPath = szPath;
	xFirst.Clear();
	xSecond.Clear();
	Zenith_AssetRegistry::ForceUnload(strPath);
}

ZENITH_TEST(AssetOwnership, GetViewLeavesRefcountUnchanged)
{
	// GetView<T> is a raw, NON-owning view: it must NOT change the refcount.
	MaterialHandle xHandle = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>("procedural://__getview_ownership_test__");
	const uint32_t uBefore = xHandle.GetDirect()->GetRefCount();
	Zenith_MaterialAsset* pxView = Zenith_AssetRegistry::GetView<Zenith_MaterialAsset>("procedural://__getview_ownership_test__");
	ZENITH_ASSERT_EQ(pxView, xHandle.GetDirect(), "GetView returns the cached instance");
	ZENITH_ASSERT_EQ(xHandle.GetDirect()->GetRefCount(), uBefore, "GetView must NOT AddRef");

	const std::string strPath = xHandle.GetDirect()->GetPath();
	xHandle.Clear();
	Zenith_AssetRegistry::ForceUnload(strPath);
}

ZENITH_TEST(AssetOwnership, PrimitiveCreatorsShareOneCachedInstance)
{
	// Zenith_MeshGeometryAsset::CreateUnitCube() returns an owning handle to a
	// per-path cached instance; two calls share the SAME asset (refcounted, so a held
	// primitive survives UnloadUnused instead of lingering at refcount 0 forever).
	MeshGeometryHandle xCubeA = Zenith_MeshGeometryAsset::CreateUnitCube();
	MeshGeometryHandle xCubeB = Zenith_MeshGeometryAsset::CreateUnitCube();
	ZENITH_ASSERT_NOT_NULL(xCubeA.GetDirect(), "primitive creator returns a resolved handle");
	ZENITH_ASSERT_EQ(xCubeA.GetDirect(), xCubeB.GetDirect(),
		"two CreateUnitCube() calls must share one cached instance");

	const std::string strPath = xCubeA.GetDirect()->GetPath();
	Zenith_AssetRegistry::UnloadUnused();
	ZENITH_ASSERT_TRUE(Zenith_AssetRegistry::IsLoaded(strPath),
		"a primitive held by a handle survives UnloadUnused");
}
