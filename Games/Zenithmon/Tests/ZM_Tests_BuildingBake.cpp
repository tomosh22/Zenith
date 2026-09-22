#include "Zenith.h"

// ============================================================================
// ZM_Tests_BuildingBake -- S4 SC5 tools-gated smoke test for the BUILDING disk
// bake (suite ZM_Gen). The static analog of ZM_Tests_HumanBake: it bakes ONE
// model's per-model bundle and proves (a) every per-model file landed non-empty
// and (b) the baked .zmodel is STATIC -- it binds NO skeleton and self-lists ZERO
// animations (buildings carry no skeleton, no clips).
//
// The whole file body is guarded by ZENITH_TOOLS: ZM_BakeBuilding only exists in
// _True configs (the _False header no-op returns false), so a _False / Android
// build sees an EMPTY translation unit here and links clean.
//
// SCOPE: ONE model (CareCenter) -- fast. The byte-identical re-bake invariant
// (bake twice -> identical file bytes) is deferred to the later ZM_BakeManifest
// box (Roadmap S4); this smoke only proves the write path + the static
// no-skeleton/no-anim binding, NOT re-bake stability.
// ============================================================================

#ifdef ZENITH_TOOLS

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_BuildingGen.h"        // ZM_BakeBuilding, asset-path scheme, asset-kind enum
#include "AssetHandling/Zenith_AssetRegistry.h"          // ResolvePath: "game:" ref -> absolute FS path
#include "AssetHandling/Zenith_ModelAsset.h"             // Zenith_ModelAsset::ParseStream / GetSkeletonPath / HasSkeleton / GetNumAnimations
#include "DataStream/Zenith_DataStream.h"                // hermetic stream load of the baked .zmodel

#include <filesystem>
#include <string>

namespace
{
	// Assert one baked "game:" asset ref resolved to a non-empty file on disk.
	// szWhich names the file in the diagnostics. A plain file-local helper (not a
	// ZENITH_TEST), so the unit count is unaffected.
	void ZM_AssertBakedRefExistsNonEmpty(const char* szRef, const char* szWhich)
	{
		const std::string strAbs = Zenith_AssetRegistry::ResolvePath(std::string(szRef));
		const std::filesystem::path xPath(strAbs);

		std::error_code xEc;
		const bool bExists = std::filesystem::exists(xPath, xEc);
		ZENITH_ASSERT_TRUE(bExists, "%s is missing: %s", szWhich, strAbs.c_str());
		if (bExists)
		{
			const std::uintmax_t uSize = std::filesystem::file_size(xPath, xEc);
			ZENITH_ASSERT_FALSE(static_cast<bool>(xEc), "could not stat %s: %s", szWhich, strAbs.c_str());
			ZENITH_ASSERT_GT(static_cast<u_int64>(uSize), static_cast<u_int64>(0u),
				"%s is empty: %s", szWhich, strAbs.c_str());
		}
	}
}

// Bake ONE building's per-model bundle (.zmesh / _facade.ztxtr / .zmtrl / .zmodel),
// assert every file landed non-empty, then load the baked .zmodel HERMETICALLY (a
// stream + ParseStream, so the registry cache the bake populated is untouched) and
// assert the STATIC contract: it binds NO skeleton and self-lists ZERO animations.
// If the bake environment is unavailable the bake returns false and the test SKIPS
// rather than fails (matching the human-bake idiom) -- an absent bake environment is
// not a code defect. ZENITH_SKIP itself returns.
ZENITH_TEST(ZM_Gen, BuildingBake_StaticModelFilesLandAndNoRig)
{
	const ZM_BUILDING_ID eId = ZM_BUILDING_CARE_CENTER;
	char acSourceRef[512];
	ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath(eId,ZM_BUILDING_ASSET_MODEL,acSourceRef,sizeof(acSourceRef)),"model path must fit");
	auto xSource=std::filesystem::path(Zenith_AssetRegistry::ResolvePath(acSourceRef));xSource.replace_extension(".glb");
	std::error_code xSourceError;
	const bool bAuthored=std::filesystem::is_regular_file(xSource,xSourceError);
	if (!ZM_BakeBuilding(eId))
	{
		ZENITH_SKIP("ZM_BakeBuilding failed -- bake environment unavailable (GAME_ASSETS_DIR not writable?)");
	}

	// (a) The per-model bundle (.zmesh / _facade.ztxtr / .zmtrl / .zmodel) must all be
	// present non-empty. The per-model kinds are 0..ZM_BUILDING_ASSET_KIND_COUNT.
	for (u_int k = 0; !bAuthored && k < static_cast<u_int>(ZM_BUILDING_ASSET_KIND_COUNT); ++k)
	{
		char acRef[512];
		ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath(eId, static_cast<ZM_BUILDING_ASSET_KIND>(k), acRef, sizeof(acRef)),
			"building asset kind %u ref must fit", k);
		ZM_AssertBakedRefExistsNonEmpty(acRef, "per-model building asset file");
	}

	// (b) Resolve + hermetically load the baked .zmodel (stream + ParseStream -- the
	// registry cache the bake populated is NOT touched).
	char acModelRef[512];
	ZENITH_ASSERT_TRUE(ZM_BuildingAssetPath(eId, ZM_BUILDING_ASSET_MODEL, acModelRef, sizeof(acModelRef)),
		"model ref must fit");
	const std::string strModelAbs = Zenith_AssetRegistry::ResolvePath(std::string(acModelRef));

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strModelAbs.c_str());
	ZENITH_ASSERT_TRUE(xStream.IsValid(), "failed to read baked .zmodel: %s", strModelAbs.c_str());

	Zenith_ModelAsset xModel;
	const Zenith_Status xStatus = xModel.ParseStream(xStream);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "failed to parse baked .zmodel: %s", strModelAbs.c_str());
	if(bAuthored)
	{
		// Authored buildings own their material sections; procedural surface-file
		// names are not dependencies of this model. Baking must preserve that.
		ZENITH_ASSERT_EQ(xModel.GetNumMeshes(),1u,"GLB building must remain one imported mesh");
		if(xModel.GetNumMeshes()==1u)
		{
			const auto& xBinding=xModel.GetMeshBinding(0);
			const auto xActual=std::filesystem::path(Zenith_AssetRegistry::ResolvePath(xBinding.GetMeshPath()));
			auto xExpected=xSource;xExpected.replace_extension(".zmesh");
			ZENITH_ASSERT_TRUE(std::filesystem::equivalent(xActual,xExpected,xSourceError),"bake overwrote the authored building mesh binding");
			ZM_AssertBakedRefExistsNonEmpty(xBinding.GetMeshPath().c_str(),"imported building mesh");
			ZENITH_ASSERT_GT(xBinding.m_xMaterials.GetSize(),0u,"imported building needs materials");
			for(u_int m=0;m<xBinding.m_xMaterials.GetSize();++m)
				ZM_AssertBakedRefExistsNonEmpty(xBinding.GetMaterialPath(m).c_str(),"imported building material");
		}
	}

	// The STATIC contract: a building .zmodel binds NO skeleton and lists NO animations.
	ZENITH_ASSERT_TRUE(xModel.GetSkeletonPath().empty(),
		"static building .zmodel must bind NO skeleton (got '%s')", xModel.GetSkeletonPath().c_str());
	ZENITH_ASSERT_FALSE(xModel.HasSkeleton(), "static building .zmodel must have HasSkeleton()==false");
	ZENITH_ASSERT_EQ(xModel.GetNumAnimations(), static_cast<u_int>(0u),
		"static building .zmodel must self-list ZERO animations");
}

#endif // ZENITH_TOOLS
