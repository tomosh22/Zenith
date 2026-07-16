#include "Zenith.h"

// ============================================================================
// ZM_Tests_HumanBake -- S4 SC5 tools-gated smoke test for the HUMAN disk bake
// (suite ZM_Gen). The human analog of ZM_Tests_CreatureBake / ZM_Tests_CreatureAnim-
// Bake: it bakes the ONE shared rig + 9 shared clips ONCE and ONE model's per-model
// bundle, and proves (a) every shared + per-model file landed non-empty and (b) the
// baked .zmodel binds the SHARED skeleton (not a per-model one) and self-lists all 9
// SHARED clip refs in IDLE..FAINT order.
//
// The whole file body is guarded by ZENITH_TOOLS: ZM_BakeHumanShared / ZM_BakeHuman
// only exist in _True configs (the _False header no-ops return false), so a _False /
// Android build sees an EMPTY translation unit here and links clean.
//
// SCOPE: the shared rig + ONE model (PlayerM, the first roster id) -- fast. The
// byte-identical re-bake invariant (bake twice -> identical file bytes) is deferred
// to the later ZM_BakeManifest box (Roadmap S4); this smoke only proves the write
// path + the model's shared-rig/shared-clip binding, NOT re-bake stability.
// ============================================================================

#ifdef ZENITH_TOOLS

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_HumanGen.h"          // ZM_BakeHumanShared, ZM_BakeHuman, asset-path schemes, clip count
#include "AssetHandling/Zenith_AssetRegistry.h"         // ResolvePath: "game:" ref -> absolute FS path
#include "AssetHandling/Zenith_ModelAsset.h"            // Zenith_ModelAsset::ParseStream / GetSkeletonPath / GetNumAnimations / GetAnimationPath
#include "DataStream/Zenith_DataStream.h"               // hermetic stream load of the baked .zmodel

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

// Bake the shared rig + clips ONCE, then PlayerM's per-model bundle, and assert every
// shared file (1 .zskel + 9 .zanim) and every per-model file (.zmesh / _albedo.ztxtr /
// .zmtrl / .zmodel) landed non-empty. If the bake environment is unavailable the bake
// returns false and the test SKIPS rather than fails (matching the creature-bake idiom)
// -- an absent bake environment is not a code defect. ZENITH_SKIP itself returns.
ZENITH_TEST(ZM_Gen, HumanBake_SharedAndModelFilesLand)
{
	if (!ZM_BakeHumanShared())
	{
		ZENITH_SKIP("ZM_BakeHumanShared failed -- bake environment unavailable (GAME_ASSETS_DIR not writable?)");
	}

	// (a) The shared rig + 9 clips (SKELETON + ANIM_IDLE..ANIM_FAINT) must all be
	// present non-empty. The shared kinds are contiguous 0..ZM_HUMAN_SHARED_ASSET_KIND_COUNT.
	for (u_int k = 0; k < static_cast<u_int>(ZM_HUMAN_SHARED_ASSET_KIND_COUNT); ++k)
	{
		const ZM_HUMAN_SHARED_ASSET_KIND eKind = static_cast<ZM_HUMAN_SHARED_ASSET_KIND>(k);
		char acRef[512];
		ZENITH_ASSERT_TRUE(ZM_HumanSharedAssetPath(eKind, acRef, sizeof(acRef)),
			"shared asset kind %u ref must fit the buffer", k);
		ZM_AssertBakedRefExistsNonEmpty(acRef, "shared asset file");
	}

	// (b) PlayerM's per-model bundle (.zmesh / _albedo.ztxtr / .zmtrl / .zmodel) must
	// all be present non-empty. The per-model kinds are 0..ZM_HUMAN_ASSET_KIND_COUNT.
	const ZM_HUMAN_ID eId = ZM_HUMAN_PLAYER_M;
	if (!ZM_BakeHuman(eId))
	{
		ZENITH_SKIP("ZM_BakeHuman failed -- bake environment unavailable (GAME_ASSETS_DIR not writable?)");
	}
	for (u_int k = 0; k < static_cast<u_int>(ZM_HUMAN_ASSET_KIND_COUNT); ++k)
	{
		const ZM_HUMAN_ASSET_KIND eKind = static_cast<ZM_HUMAN_ASSET_KIND>(k);
		char acRef[512];
		ZENITH_ASSERT_TRUE(ZM_HumanAssetPath(eId, eKind, acRef, sizeof(acRef)),
			"per-model asset kind %u ref must fit the buffer", k);
		ZM_AssertBakedRefExistsNonEmpty(acRef, "per-model asset file");
	}
}

// Bake the shared rig + PlayerM's bundle, then load the baked .zmodel HERMETICALLY
// (a stream + ParseStream, so the registry cache the bake populated is untouched) and
// assert it binds the SHARED skeleton ref (proving it does NOT own a per-model rig) and
// self-lists exactly the 9 SHARED clip refs in IDLE..FAINT order.
ZENITH_TEST(ZM_Gen, HumanBake_ModelBindsSharedRigAndClips)
{
	if (!ZM_BakeHumanShared())
	{
		ZENITH_SKIP("ZM_BakeHumanShared failed -- bake environment unavailable (GAME_ASSETS_DIR not writable?)");
	}

	const ZM_HUMAN_ID eId = ZM_HUMAN_PLAYER_M;
	if (!ZM_BakeHuman(eId))
	{
		ZENITH_SKIP("ZM_BakeHuman failed -- bake environment unavailable (GAME_ASSETS_DIR not writable?)");
	}

	// Resolve + hermetically load the baked .zmodel (stream + ParseStream -- the
	// registry cache the bake populated is NOT touched).
	char acModelRef[512];
	ZENITH_ASSERT_TRUE(ZM_HumanAssetPath(eId, ZM_HUMAN_ASSET_MODEL, acModelRef, sizeof(acModelRef)),
		"model ref must fit the buffer");
	const std::string strModelAbs = Zenith_AssetRegistry::ResolvePath(std::string(acModelRef));

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strModelAbs.c_str());
	ZENITH_ASSERT_TRUE(xStream.IsValid(), "failed to read baked .zmodel: %s", strModelAbs.c_str());

	Zenith_ModelAsset xModel;
	const Zenith_Status xStatus = xModel.ParseStream(xStream);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "failed to parse baked .zmodel: %s", strModelAbs.c_str());

	// The model must bind the SHARED skeleton ref (game:Humans/Shared/Human.zskel),
	// proving humans do NOT carry a per-model rig.
	char acSkelRef[512];
	ZENITH_ASSERT_TRUE(ZM_HumanSharedAssetPath(ZM_HUMAN_SHARED_ASSET_SKELETON, acSkelRef, sizeof(acSkelRef)),
		"shared skeleton ref must fit the buffer");
	ZENITH_ASSERT_STREQ(xModel.GetSkeletonPath().c_str(), acSkelRef,
		"baked .zmodel must bind the SHARED skeleton ref");

	// It must self-list exactly the 9 SHARED clip refs, in IDLE..FAINT order.
	ZENITH_ASSERT_EQ(xModel.GetNumAnimations(), static_cast<u_int>(ZM_HUMAN_CLIP_COUNT),
		"baked .zmodel must self-list all 9 shared clips");

	// Guard the index against a wrong count so a prior mismatch can't OOB-read GetAnimationPath.
	if (xModel.GetNumAnimations() == static_cast<u_int>(ZM_HUMAN_CLIP_COUNT))
	{
		for (u_int c = 0; c < static_cast<u_int>(ZM_HUMAN_CLIP_COUNT); ++c)
		{
			char acExpectRef[512];
			ZENITH_ASSERT_TRUE(ZM_HumanSharedAssetPath(
				static_cast<ZM_HUMAN_SHARED_ASSET_KIND>(ZM_HUMAN_SHARED_ASSET_ANIM_IDLE + c),
				acExpectRef, sizeof(acExpectRef)),
				"clip %u expected shared ref must fit the buffer", c);

			ZENITH_ASSERT_STREQ(xModel.GetAnimationPath(c).c_str(), acExpectRef,
				"baked .zmodel animation %u ref mismatch (order must be IDLE..FAINT)", c);
		}
	}
}

#endif // ZENITH_TOOLS
