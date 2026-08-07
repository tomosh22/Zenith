#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Source/PublicInterfaces.h"
#include "Source/DPMaterials.h"

// ============================================================================
// Materials_Test
//
// Loads ProcLevel (build index 1), then verifies:
//   - DPMaterials::Initialize ran during Project_RegisterGameComponents
//     (the canonical __DPDefault fallback material is registered), and
//   - the gameplay materials the level actually uses resolve out of the asset
//     registry rather than silently falling back to the default.
//
// This is intentionally lenient on individual material CONTENT - the accepted
// constraint is "engine default lit + correct material params, accept loss for
// parameters Zenith doesn't model".
//
// ---------------------------------------------------------------------------
// 2026-08-07: RETARGETED off the deleted UE-bridge pipeline.
//
// This test used to assert `GetRegisteredMaterialCount() >= 30` and that
// "game:Materials/LevelPrototyping_Materials_PrototypeGrid_M_PrototypeGrid_Red.zmtrl"
// resolved. Both encoded a world that no longer exists: the `Tools/dp_export/`
// UE bridge and its 37 `.json` material dumps were deleted on 2026-05-19 along
// with the hand-authored GameLevel (see DevilsPlayground/CLAUDE.md), and
// DP's materials were re-authored as a hand-built PBR set on 2026-06-13
// (d9ea6f5b). `DPMaterials::AuthorAllMaterials` walks `Assets/Materials/*.json`,
// of which there are now ZERO, so Initialize registers exactly ONE material
// (the default) and the PrototypeGrid greybox placeholder is simply gone.
// The two assertions were therefore unsatisfiable, not failing.
//
// It went unnoticed because the test is `m_bRequiresGraphics = true` and the
// dp-tests gate runs `--headless` on a Null build, where such tests are
// SKIPPED before Setup ever runs.
//
// The named-material list below is deliberately the SHIPPING set, so this test
// now fails if a gameplay material stops resolving - which is what it was
// always trying to protect.
// ============================================================================

namespace
{
	bool g_bSceneTriggered     = false;
	bool g_bSceneLoaded        = false;
	bool g_bAssertionsRan      = false;

	uint32_t g_uRegisteredCount = 0;
	bool g_bDefaultPresent     = false;
	bool g_bExpectedPresent    = false;
	bool g_bUEMappingWorks     = false;

	// The materials DP actually authors for gameplay entities (2026-06-13
	// "appropriate PBR materials for every entity"). Every one of these must
	// resolve out of the registry by the time ProcLevel is up; a miss means an
	// entity is rendering with the grey fallback.
	const char* const kaszEXPECTED_MATERIALS[] = {
		"game:Materials/DP_Ground.zmtrl",
		"game:Materials/DP_StoneWall.zmtrl",
		"game:Materials/DP_DoorWood.zmtrl",
		"game:Materials/DP_Chest.zmtrl",
		"game:Materials/DP_Forge.zmtrl",
		"game:Materials/DP_Pentagram.zmtrl",
		"game:Materials/DP_NoiseMachine.zmtrl",
		"game:Materials/DP_PriestRobe.zmtrl",
		"game:Materials/DP_Robe_Farmhand.zmtrl",
		"game:Materials/DP_Robe_Beggar.zmtrl",
		"game:Materials/DP_Robe_Devout.zmtrl",
		"game:Materials/DP_Robe_Child.zmtrl",
	};
	constexpr uint32_t uEXPECTED_MATERIAL_COUNT =
		static_cast<uint32_t>(sizeof(kaszEXPECTED_MATERIALS) / sizeof(kaszEXPECTED_MATERIALS[0]));

	const char* g_szFirstMissingMaterial = nullptr;
}

static void Setup_Materials()
{
	g_bSceneTriggered  = false;
	g_bSceneLoaded     = false;
	g_bAssertionsRan   = false;
	g_uRegisteredCount = 0;
	g_bDefaultPresent  = false;
	g_bExpectedPresent = false;
	g_bUEMappingWorks  = false;
	g_szFirstMissingMaterial = nullptr;
}

static bool Step_Materials(int iFrame)
{
	if (iFrame == 0)
	{
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_bSceneTriggered = true;
		return true;
	}

	// Wait a few frames for the scene load to settle.
	if (!g_bSceneLoaded)
	{
		Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActive);
		if (pxSceneData != nullptr)
		{
			g_bSceneLoaded = true;
		}
		if (iFrame > 30 && !g_bSceneLoaded) return false;
		if (!g_bSceneLoaded) return true;
	}

	if (!g_bAssertionsRan)
	{
		// 1. How many materials DPMaterials::Initialize + on-demand authoring
		//    have registered. Reported for diagnosis; the meaningful assertion
		//    is the named list below, not a magic total.
		g_uRegisteredCount = DPMaterials::GetRegisteredMaterialCount();

		// 2. Default material should always be present — proves Initialize ran.
		Zenith_MaterialAsset* pxDefault = Zenith_AssetRegistry::GetView<Zenith_MaterialAsset>(
			"game:Materials/__DPDefault.zmtrl");
		g_bDefaultPresent = (pxDefault != nullptr);

		// 3. Every shipping gameplay material must resolve. A miss here is an
		//    entity silently rendering with the grey fallback.
		g_bExpectedPresent = true;
		for (uint32_t u = 0; u < uEXPECTED_MATERIAL_COUNT; ++u)
		{
			if (Zenith_AssetRegistry::GetView<Zenith_MaterialAsset>(kaszEXPECTED_MATERIALS[u]) == nullptr)
			{
				g_bExpectedPresent = false;
				if (g_szFirstMissingMaterial == nullptr)
				{
					g_szFirstMissingMaterial = kaszEXPECTED_MATERIALS[u];
				}
				Zenith_Error(LOG_CATEGORY_ASSET,
					"Materials_Test: expected material did not resolve: %s", kaszEXPECTED_MATERIALS[u]);
			}
		}

		// 4. UE-path -> registry-path mapping stays pinned on its original input.
		//    Unlike (3) this is a PURE string transform that never touches the
		//    registry, so it does not care that the asset it names was deleted
		//    with the UE bridge - and it was passing throughout. Kept as-is:
		//    UEPathToRegistryPath is still live (DPMaterials::AuthorMaterialFromJson
		//    derives stems through the same convention).
		std::string strMapped = DPMaterials::UEPathToRegistryPath(
			"/Game/LevelPrototyping/Materials/PrototypeGrid/M_PrototypeGrid_Red.M_PrototypeGrid_Red");
		g_bUEMappingWorks = (strMapped ==
			"game:Materials/LevelPrototyping_Materials_PrototypeGrid_M_PrototypeGrid_Red.zmtrl");

		// Diagnostic logging — keeps failures discoverable from the harness log.
		Zenith_Log(LOG_CATEGORY_ASSET,
			"Materials_Test: count=%u default=%d expectedAll=%d (%u checked, firstMissing=%s) mapping=%d mapped='%s'",
			g_uRegisteredCount,
			(int)g_bDefaultPresent,
			(int)g_bExpectedPresent,
			uEXPECTED_MATERIAL_COUNT,
			g_szFirstMissingMaterial ? g_szFirstMissingMaterial : "(none)",
			(int)g_bUEMappingWorks,
			strMapped.c_str());

		g_bAssertionsRan = true;
		return false;
	}

	return false;
}

static bool Verify_Materials()
{
	if (!g_bSceneTriggered) return false;
	if (!g_bSceneLoaded)    return false;
	if (!g_bAssertionsRan)  return false;

	// No magic total any more. The old `>= 30` encoded the 37-file UE `.json`
	// dump that was deleted with the bridge; DPMaterials now authors its set in
	// code, so the honest lower bound is "the default plus every shipping
	// gameplay material", which (3) checks by NAME.
	if (g_uRegisteredCount < uEXPECTED_MATERIAL_COUNT) return false;
	if (!g_bDefaultPresent)      return false;
	if (!g_bExpectedPresent)     return false;
	if (!g_bUEMappingWorks)      return false;
	return true;
}

static const Zenith_AutomatedTest g_xMaterialsTest = {
	"Materials_Test",
	&Setup_Materials,
	&Step_Materials,
	&Verify_Materials,
	240,
	true   // m_bRequiresGraphics: asserts on Zenith_MaterialAsset count + DPMaterials::Initialize side-table
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMaterialsTest);

#endif // ZENITH_INPUT_SIMULATOR
