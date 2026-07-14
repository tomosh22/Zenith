#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureAnimBake -- S4 SC6 tools-gated smoke test for the creature
// ANIMATION disk bake (suite ZM_Gen). The twin of ZM_Tests_CreatureBake: it bakes
// ONE creature's full bundle (which now includes the 6 procedural .zanim clips)
// and proves (a) every clip file landed non-empty and (b) both baked .zmodels
// (base + shiny) self-list all 6 clip refs in IDLE..FAINT order.
//
// The whole file body is guarded by ZENITH_TOOLS: ZM_BakeCreature + the .zanim
// bake only exist in _True configs (the _False header no-ops return false), so a
// _False / Android build sees an EMPTY translation unit here and links clean.
//
// SCOPE: ONE species only (Fernfawn) -- fast. The byte-identical re-bake invariant
// (bake twice -> identical file bytes) is deferred to the later ZM_BakeManifest box
// (Roadmap S4); this smoke only proves the clip-write path + the model's animation
// self-listing, NOT re-bake stability.
// ============================================================================

#ifdef ZENITH_TOOLS

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"       // ZM_BakeCreature, ZM_CreatureAssetPath, ANIM asset kinds
#include "Zenithmon/Source/Gen/ZM_CreatureAnimGen.h"    // ZM_ANIM_CLIP_COUNT
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"       // ZM_SPECIES_FERNFAWN
#include "AssetHandling/Zenith_AssetRegistry.h"         // ResolvePath: "game:" ref -> absolute FS path
#include "AssetHandling/Zenith_ModelAsset.h"            // Zenith_ModelAsset::ParseStream / GetNumAnimations / GetAnimationPath
#include "DataStream/Zenith_DataStream.h"               // hermetic stream load of the baked .zmodel

#include <filesystem>
#include <string>

// Load one baked .zmodel hermetically (a stream + ParseStream, so the registry cache
// the bake populated is untouched) and assert it self-lists exactly the 6 procedural
// clip refs, in IDLE..FAINT order, each matching the canonical ZM_CreatureAssetPath
// ref. Base and shiny models share the SAME 6 .zanim clips, so both are checked against
// the same canonical refs; szWhich names the model in the diagnostics. Not a ZENITH_TEST
// -- a plain file-local helper, so the unit count is unaffected.
static void ZM_AssertModelSelfListsAllClips(ZM_SPECIES_ID eId,
	ZM_CREATURE_ASSET_KIND eModelKind, const char* szWhich)
{
	char acModelRef[512];
	ZENITH_ASSERT_TRUE(ZM_CreatureAssetPath(eId, eModelKind, acModelRef, sizeof(acModelRef)),
		"%s model ref must fit the buffer", szWhich);
	const std::string strModelAbs = Zenith_AssetRegistry::ResolvePath(std::string(acModelRef));

	Zenith_DataStream xStream;
	xStream.ReadFromFile(strModelAbs.c_str());
	ZENITH_ASSERT_TRUE(xStream.IsValid(), "failed to read baked %s .zmodel: %s", szWhich, strModelAbs.c_str());

	Zenith_ModelAsset xModel;
	const Zenith_Status xStatus = xModel.ParseStream(xStream);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "failed to parse baked %s .zmodel: %s", szWhich, strModelAbs.c_str());

	ZENITH_ASSERT_EQ(xModel.GetNumAnimations(), static_cast<u_int>(ZM_ANIM_CLIP_COUNT),
		"baked %s .zmodel must self-list all 6 procedural clips", szWhich);

	// Per-clip ref match, in IDLE..FAINT order (the bake appends clips in enum order and
	// the model round-trips that order). Guard the index against a wrong count so a
	// prior mismatch can't OOB-read GetAnimationPath.
	if (xModel.GetNumAnimations() == static_cast<u_int>(ZM_ANIM_CLIP_COUNT))
	{
		for (u_int c = 0; c < static_cast<u_int>(ZM_ANIM_CLIP_COUNT); ++c)
		{
			char acExpectRef[512];
			ZENITH_ASSERT_TRUE(ZM_CreatureAssetPath(eId,
				static_cast<ZM_CREATURE_ASSET_KIND>(ZM_CREATURE_ASSET_ANIM_IDLE + c),
				acExpectRef, sizeof(acExpectRef)),
				"clip %u expected ref must fit the buffer", c);

			ZENITH_ASSERT_STREQ(xModel.GetAnimationPath(c).c_str(), acExpectRef,
				"baked %s .zmodel animation %u ref mismatch (order must be IDLE..FAINT)", szWhich, c);
		}
	}
}

// Bake Fernfawn's bundle, then assert the 6 procedural .zanim clips landed
// non-empty AND both baked .zmodels (base + shiny) self-list all 6 clip refs (IDLE..FAINT).
// If the bake environment is unavailable, ZM_BakeCreature returns false and the
// test SKIPS rather than fails (matching ZM_Tests_CreatureBake's idiom) -- an
// absent bake environment is not a code defect. ZENITH_SKIP itself returns, so no
// trailing return is needed (mirrors ZM_Tests_CreatureBake::CreatureBake_BundleFilesLand).
ZENITH_TEST(ZM_Gen, CreatureAnimBake_ClipsLandAndModelReferences)
{
	// Fernfawn (F01 stage 1) is the canonical QUADRUPED reference species and is
	// always buildable; the ZM_Gen dispatch gate proves its archetype is wired.
	const ZM_SPECIES_ID eId = ZM_SPECIES_FERNFAWN;

	if (!ZM_BakeCreature(eId))
	{
		ZENITH_SKIP("ZM_BakeCreature failed -- bake environment unavailable (GAME_ASSETS_DIR not writable?)");
	}

	// (a) Each of the 6 clip files (IDLE..FAINT) must have landed non-empty. The clip
	// asset kinds are contiguous from ZM_CREATURE_ASSET_ANIM_IDLE in ZM_ANIM_CLIP order.
	for (u_int c = 0; c < static_cast<u_int>(ZM_ANIM_CLIP_COUNT); ++c)
	{
		const ZM_CREATURE_ASSET_KIND eKind =
			static_cast<ZM_CREATURE_ASSET_KIND>(ZM_CREATURE_ASSET_ANIM_IDLE + c);

		// Canonical "game:" ref, resolved to the absolute FS path ZM_BakeCreatureClips wrote to.
		char acRef[512];
		ZENITH_ASSERT_TRUE(ZM_CreatureAssetPath(eId, eKind, acRef, sizeof(acRef)),
			"clip %u asset ref must fit the buffer", c);

		const std::string strAbs = Zenith_AssetRegistry::ResolvePath(std::string(acRef));
		const std::filesystem::path xPath(strAbs);

		std::error_code xEc;
		const bool bExists = std::filesystem::exists(xPath, xEc);
		ZENITH_ASSERT_TRUE(bExists, "clip %u .zanim is missing: %s", c, strAbs.c_str());

		if (bExists)
		{
			const std::uintmax_t uSize = std::filesystem::file_size(xPath, xEc);
			ZENITH_ASSERT_FALSE(static_cast<bool>(xEc), "could not stat clip %u .zanim: %s", c, strAbs.c_str());
			ZENITH_ASSERT_GT(static_cast<u_int64>(uSize), static_cast<u_int64>(0u),
				"clip %u .zanim is empty: %s", c, strAbs.c_str());
		}
	}

	// (b) BOTH baked .zmodels (base + shiny) must self-list exactly 6 animation refs,
	// in IDLE..FAINT order, each matching the canonical ZM_CreatureAssetPath ref. The
	// base and shiny models differ only in material and share the same 6 .zanim clips,
	// so both list identical refs. The helper loads each hermetically (stream +
	// ParseStream) so the registry cache the bake populated is not touched.
	ZM_AssertModelSelfListsAllClips(eId, ZM_CREATURE_ASSET_MODEL,       "base");
	ZM_AssertModelSelfListsAllClips(eId, ZM_CREATURE_ASSET_MODEL_SHINY, "shiny");
}

#endif // ZENITH_TOOLS
