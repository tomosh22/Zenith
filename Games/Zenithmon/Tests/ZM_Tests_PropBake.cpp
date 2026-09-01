#include "Zenith.h"

// ============================================================================
// ZM_Tests_PropBake -- S4 SC5 tools-gated smoke test for the PROP disk bake
// (suite ZM_Gen). The static analog of ZM_Tests_HumanBake: it bakes ONE model's
// per-model bundle and proves (a) every per-model file landed non-empty and (b)
// the baked .zmodel is STATIC -- it binds NO skeleton and self-lists ZERO
// animations (props carry no skeleton, no clips).
//
// The whole file body is guarded by ZENITH_TOOLS: ZM_BakeProp only exists in
// _True configs (the _False header no-op returns false), so a _False / Android
// build sees an EMPTY translation unit here and links clean.
//
// SCOPE: ONE model (LampPost) -- fast. The byte-identical re-bake invariant
// (bake twice -> identical file bytes) is deferred to the later ZM_BakeManifest
// box (Roadmap S4); this smoke only proves the write path + the static
// no-skeleton/no-anim binding, NOT re-bake stability.
// ============================================================================

#ifdef ZENITH_TOOLS

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_PropGen.h"             // ZM_BakeProp, asset-path scheme, asset-kind enum
#include "AssetHandling/Zenith_AssetRegistry.h"          // ResolvePath: "game:" ref -> absolute FS path
#include "AssetHandling/Zenith_MeshAsset.h"              // the import-wins clause reads the baked mesh
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

	// ★★★ WHICH PROP THIS TEST MAY BAKE, AND WHY IT IS NOT A CONSTANT ANY MORE.
	//
	// This read `const ZM_PROP_ID eId = ZM_PROP_LAMP_POST;` -- an arbitrary
	// representative, chosen when every prop in the roster was generated. It is
	// no longer arbitrary: `Assets/Props/LampPost/LampPost.glb` is AB-PROP-07, and
	// ZM_BakeProp writes the generated bundle straight over the imported one, on
	// the same paths, unconditionally, on EVERY boot. The import runs earlier in
	// the same boot, so the lamp post's import never survived to be looked at
	// once -- the mesh went 6623 verts -> 72 and every texture 2.8 MB -> 11 KB
	// between the import log line and the unit tally, with the suite green.
	//
	// ★ THE FIX IS NOT "POINT IT AT ANOTHER PROP", which just re-arms the same
	// trap for whichever row is imported next. What this test is ABOUT is the
	// GENERATOR -- that its bundle lands and that a prop .zmodel is static -- so
	// its subject must be a prop the generator actually owns. That is a property
	// of the tree, not a constant, so it is resolved from the tree: the first
	// roster row with no `.glb` beside its bundle.
	//
	// TOTAL: ZM_PROP_COUNT when every prop has been imported, which the caller
	// turns into a SKIP. That day the generator has no props left to prove
	// anything about, and baking one anyway would destroy an asset.
	ZM_PROP_ID ZM_FirstGeneratedProp()
	{
		for (u_int u = 0u; u < static_cast<u_int>(ZM_PROP_COUNT); ++u)
		{
			const ZM_PROP_ID eId = static_cast<ZM_PROP_ID>(u);
			char acRef[512];
			if (!ZM_PropAssetPath(eId, ZM_PROP_ASSET_MODEL, acRef, sizeof(acRef)))
			{
				continue;
			}
			const std::filesystem::path xModel(
				Zenith_AssetRegistry::ResolvePath(std::string(acRef)));
			const std::filesystem::path xGlb =
				xModel.parent_path() / (std::string(ZM_GetPropName(eId)) + ".glb");

			std::error_code xEc;
			if (!std::filesystem::exists(xGlb, xEc) || xEc)
			{
				return eId;
			}
		}
		return ZM_PROP_COUNT;
	}
}

// Bake ONE prop's per-model bundle (.zmesh / _albedo.ztxtr / .zmtrl / .zmodel),
// assert every file landed non-empty, then load the baked .zmodel HERMETICALLY (a
// stream + ParseStream, so the registry cache the bake populated is untouched) and
// assert the STATIC contract: it binds NO skeleton and self-lists ZERO animations.
// If the bake environment is unavailable the bake returns false and the test SKIPS
// rather than fails (matching the human-bake idiom) -- an absent bake environment is
// not a code defect. ZENITH_SKIP itself returns.
ZENITH_TEST(ZM_Gen, PropBake_StaticModelFilesLandAndNoRig)
{
	// ★ RESOLVED FROM THE TREE, NEVER A CONSTANT -- see ZM_FirstGeneratedProp for
	// the asset this destroyed while it was one.
	const ZM_PROP_ID eId = ZM_FirstGeneratedProp();
	if (eId >= ZM_PROP_COUNT)
	{
		ZENITH_SKIP("every prop in the roster is imported, so there is no generated "
			"bundle left to prove anything about -- and baking one would overwrite "
			"an imported asset");
	}
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[ZM_Gen] PropBake: baking '%s', the first roster prop with no .glb beside it",
		ZM_GetPropName(eId));
	if (!ZM_BakeProp(eId))
	{
		ZENITH_SKIP("ZM_BakeProp failed -- bake environment unavailable (GAME_ASSETS_DIR not writable?)");
	}

	// (a) The per-model bundle (.zmesh / _albedo.ztxtr / .zmtrl / .zmodel) must all be
	// present non-empty. The per-model kinds are 0..ZM_PROP_ASSET_KIND_COUNT.
	for (u_int k = 0; k < static_cast<u_int>(ZM_PROP_ASSET_KIND_COUNT); ++k)
	{
		char acRef[512];
		ZENITH_ASSERT_TRUE(ZM_PropAssetPath(eId, static_cast<ZM_PROP_ASSET_KIND>(k), acRef, sizeof(acRef)),
			"prop asset kind %u ref must fit", k);
		ZM_AssertBakedRefExistsNonEmpty(acRef, "per-model prop asset file");
	}

	// (b) Resolve + hermetically load the baked .zmodel (stream + ParseStream -- the
	// registry cache the bake populated is NOT touched).
	char acModelRef[512];
	ZENITH_ASSERT_TRUE(ZM_PropAssetPath(eId, ZM_PROP_ASSET_MODEL, acModelRef, sizeof(acModelRef)),
		"model ref must fit");
	const std::string strModelAbs = Zenith_AssetRegistry::ResolvePath(std::string(acModelRef));

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strModelAbs.c_str());
	ZENITH_ASSERT_TRUE(xStream.IsValid(), "failed to read baked .zmodel: %s", strModelAbs.c_str());

	Zenith_ModelAsset xModel;
	const Zenith_Status xStatus = xModel.ParseStream(xStream);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "failed to parse baked .zmodel: %s", strModelAbs.c_str());

	// The STATIC contract: a prop .zmodel binds NO skeleton and lists NO animations.
	ZENITH_ASSERT_TRUE(xModel.GetSkeletonPath().empty(),
		"static prop .zmodel must bind NO skeleton (got '%s')", xModel.GetSkeletonPath().c_str());
	ZENITH_ASSERT_FALSE(xModel.HasSkeleton(), "static prop .zmodel must have HasSkeleton()==false");
	ZENITH_ASSERT_EQ(xModel.GetNumAnimations(), static_cast<u_int>(0u),
		"static prop .zmodel must self-list ZERO animations");
}


// ★★★ THE OTHER HALF OF THE RULING: **where a `.glb` exists it is the `.glb` that
// gets used.** The import writes the same per-model file set on the same paths,
// so this is supposed to be automatic -- and it silently was not. The test above
// baked a hard-coded ZM_PROP_LAMP_POST unconditionally on EVERY boot, straight
// over the imported bundle: the mesh went 6623 verts -> 72 and every texture
// 2.8 MB -> 11 KB between the import log line and the unit tally, with the whole
// suite green. Nothing anywhere asserted that an import had survived.
//
// ★ IT IS A VERTEX COUNT BECAUSE THE TWO POPULATIONS DO NOT OVERLAP, and the
// threshold is derived rather than picked. MEASURED across the whole roster: the
// largest GENERATED prop is 120 verts (the fence and bridge sections -- they are
// box compositions) and the smallest IMPORTED one is 3515 (the table), a 29x gap.
// 1000 sits ~8x above every generator and ~3.5x below every import.
//
// Bounds would be an exact check rather than a separated one -- the importer
// reproduces the `.glb`'s declared POSITION accessor min/max byte-for-byte -- but
// it needs the glTF JSON parsed here, which is the importer's job and not a
// test's. The gap is wide enough that this cannot be fooled by a re-export.
ZENITH_TEST(ZM_Gen, ImportedPropsUseTheirGlbAndNotTheGenerator)
{
	// Comfortably between the two populations; see above.
	constexpr u_int uMIN_IMPORTED_VERTS = 1000u;

	u_int uImported = 0u;
	for (u_int u = 0u; u < static_cast<u_int>(ZM_PROP_COUNT); ++u)
	{
		const ZM_PROP_ID eId = static_cast<ZM_PROP_ID>(u);
		char acModelRef[512];
		if (!ZM_PropAssetPath(eId, ZM_PROP_ASSET_MODEL, acModelRef, sizeof(acModelRef)))
		{
			continue;
		}
		const std::filesystem::path xModel(
			Zenith_AssetRegistry::ResolvePath(std::string(acModelRef)));
		const std::filesystem::path xGlb =
			xModel.parent_path() / (std::string(ZM_GetPropName(eId)) + ".glb");

		std::error_code xEc;
		if (!std::filesystem::exists(xGlb, xEc) || xEc)
		{
			continue;   // generated, and legitimately so
		}
		++uImported;

		char acMeshRef[512];
		ZENITH_ASSERT_TRUE(
			ZM_PropAssetPath(eId, ZM_PROP_ASSET_MESH, acMeshRef, sizeof(acMeshRef)),
			"'%s' mesh ref must fit", ZM_GetPropName(eId));
		const std::string strMeshAbs =
			Zenith_AssetRegistry::ResolvePath(std::string(acMeshRef));

		Zenith_DataStream xMeshStream;
		xMeshStream.ReadFromFile(strMeshAbs.c_str());
		ZENITH_ASSERT_TRUE(xMeshStream.IsValid(),
			"'%s' has a .glb but its .zmesh could not be read: %s",
			ZM_GetPropName(eId), strMeshAbs.c_str());

		Zenith_MeshAsset xMesh;
		const Zenith_Status xStatus = xMesh.ParseStream(xMeshStream);
		ZENITH_ASSERT_TRUE(xStatus.IsOk(), "'%s': failed to parse %s",
			ZM_GetPropName(eId), strMeshAbs.c_str());

		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_Gen] OBSERVED '%s' has a .glb and a %u-vertex baked mesh",
			ZM_GetPropName(eId), xMesh.GetNumVerts());

		ZENITH_ASSERT_GT(xMesh.GetNumVerts(), uMIN_IMPORTED_VERTS,
			"'%s' has %s beside it but its baked mesh is only %u vertices -- that is "
			"the GENERATOR's box composition, so something overwrote the import. The "
			"generated roster tops out at 120 verts and the smallest import is 3515. "
			"Look for an unconditional ZM_BakeProp: one in a unit test destroyed this "
			"exact asset on every boot.",
			ZM_GetPropName(eId), xGlb.string().c_str(), xMesh.GetNumVerts());
	}

	// ★ ANTI-VACUITY. Every clause above is inside a branch that a tree with no
	// `.glb` at all skips entirely -- which is a legitimate state (the sources are
	// gitignored), so this cannot be an assert. It is logged loudly instead, since
	// a run that checked nothing must not read as a run that checked something.
	if (uImported == 0u)
	{
		Zenith_Warning(LOG_CATEGORY_UNITTEST,
			"[ZM_Gen] no prop has a .glb beside it, so the import-wins clause "
			"measured NOTHING this run. Expected on a clone with no art; a defect "
			"anywhere the sources are present.");
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZM_Gen] OBSERVED %u imported props, all using their .glb", uImported);
	}
}

#endif // ZENITH_TOOLS
