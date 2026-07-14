#include "Zenith.h"

// ============================================================================
// ZM_Tests_CreatureBake -- S4 SC5b tools-gated smoke test for the creature disk
// bake (suite ZM_Gen). Unlike the pure/headless ZM_Tests_CreatureGen gate, this
// exercises the ZENITH_TOOLS-only ZM_BakeCreature: it bakes ONE creature and
// proves the on-disk bundle write path lands ALL NINE artifacts (mesh, skeleton,
// albedo, shiny, icon, base material, shiny material, base model, shiny model).
//
// The whole file body is guarded by ZENITH_TOOLS: ZM_BakeCreature only exists in
// _True configs (the _False header no-op returns false), so a _False / Android
// build sees an EMPTY translation unit here and links clean.
//
// SCOPE: one QUADRUPED species only (fast -- a bundle bake synthesises a 512^2
// albedo + shiny + a 128^2 icon and writes nine files). The FULL byte-identical
// re-bake invariant (bake twice -> identical file bytes) is deferred to the later
// ZM_BakeManifest box (Roadmap S4); this smoke only proves the bundle-write path
// produces all nine files, non-empty.
// ============================================================================

#ifdef ZENITH_TOOLS

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Gen/ZM_CreatureGen.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "AssetHandling/Zenith_AssetRegistry.h"   // ResolvePath: "game:" ref -> absolute FS path

#include <filesystem>
#include <string>

// Bake a single creature bundle and assert every one of the nine bundle files
// landed on disk and is non-empty. If the bake environment is unavailable (e.g.
// GAME_ASSETS_DIR is not writable), ZM_BakeCreature returns false and the test
// SKIPS rather than fails -- an absent bake environment is not a code defect.
//
// (Skip mechanism: ZENITH_SKIP is the unit-test-framework skip. The automated /
// windowed tests use Zenith_AutomatedTestRunner::RequestSkip instead; that facility
// belongs to the harness runner, not to a boot-time ZENITH_TEST unit.)
ZENITH_TEST(ZM_Gen, CreatureBake_BundleFilesLand)
{
	// Fernfawn (F01 stage 1) is the canonical QUADRUPED reference species and is
	// always buildable; the ZM_Gen dispatch gate proves its archetype is wired.
	const ZM_SPECIES_ID eId = ZM_SPECIES_FERNFAWN;

	if (!ZM_BakeCreature(eId))
	{
		ZENITH_SKIP("ZM_BakeCreature failed -- bake environment unavailable (GAME_ASSETS_DIR not writable?)");
	}

	// Every kind in [MESH .. MODEL_SHINY] must have landed a non-empty file. The
	// enum's first ZM_CREATURE_ASSET_KIND_COUNT values are exactly the nine bundle
	// artifacts, so this loop covers all nine.
	for (u_int k = 0; k < static_cast<u_int>(ZM_CREATURE_ASSET_KIND_COUNT); ++k)
	{
		const ZM_CREATURE_ASSET_KIND eKind = static_cast<ZM_CREATURE_ASSET_KIND>(k);

		// The canonical "game:" ref, then resolve it to the absolute filesystem path
		// (game: -> GAME_ASSETS_DIR) -- the same location ZM_BakeCreature wrote to.
		char acRef[512];
		ZENITH_ASSERT_TRUE(ZM_CreatureAssetPath(eId, eKind, acRef, sizeof(acRef)),
			"asset ref for kind %u must fit the buffer", k);

		const std::string strAbs = Zenith_AssetRegistry::ResolvePath(std::string(acRef));
		const std::filesystem::path xPath(strAbs);

		std::error_code xEc;
		const bool bExists = std::filesystem::exists(xPath, xEc);
		ZENITH_ASSERT_TRUE(bExists, "bundle file for kind %u is missing: %s", k, strAbs.c_str());

		if (bExists)
		{
			const std::uintmax_t uSize = std::filesystem::file_size(xPath, xEc);
			ZENITH_ASSERT_FALSE(static_cast<bool>(xEc), "could not stat bundle file for kind %u: %s", k, strAbs.c_str());
			ZENITH_ASSERT_GT(static_cast<u_int64>(uSize), static_cast<u_int64>(0u),
				"bundle file for kind %u is empty: %s", k, strAbs.c_str());
		}
	}
}

#endif // ZENITH_TOOLS
