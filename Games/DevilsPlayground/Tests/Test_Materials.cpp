#include "Zenith.h"

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
// Loads GameLevel (build index 1), then verifies:
//   - DPMaterials::Initialize ran during Project_RegisterScriptBehaviours
//   - The asset registry returns non-null for at least one expected material
//     authored from a UE parameter dump
//   - The total registered count is at least 30 (the dump produces 37 .json
//     files plus one default material; allowing some to be skipped silently
//     when the JSON is malformed or the texture .ztxtr is missing).
//   - The default fallback material is registered.
//
// This is intentionally lenient on individual material content - the
// constraint accepted by the user is "engine default lit + correct material
// params, accept loss for parameters Zenith doesn't model".
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
		// 1. DPMaterials should report at least 30 materials registered. The
		//    UE dump produces 37 .json files plus the default fallback.
		g_uRegisteredCount = DPMaterials::GetRegisteredMaterialCount();

		// 2. Default material should always be present.
		Zenith_MaterialAsset* pxDefault = Zenith_AssetRegistry::Get<Zenith_MaterialAsset>(
			"game:Materials/__DPDefault.zmtrl");
		g_bDefaultPresent = (pxDefault != nullptr);

		// 3. At least one expected material from the UE dump - pick a stable
		//    one (the prototype-grid red, which has explicit SurfaceColor).
		Zenith_MaterialAsset* pxRed = Zenith_AssetRegistry::Get<Zenith_MaterialAsset>(
			"game:Materials/LevelPrototyping_Materials_PrototypeGrid_M_PrototypeGrid_Red.zmtrl");
		g_bExpectedPresent = (pxRed != nullptr);

		// 4. UE-path -> registry-path mapping should be deterministic.
		std::string strMapped = DPMaterials::UEPathToRegistryPath(
			"/Game/LevelPrototyping/Materials/PrototypeGrid/M_PrototypeGrid_Red.M_PrototypeGrid_Red");
		g_bUEMappingWorks = (strMapped ==
			"game:Materials/LevelPrototyping_Materials_PrototypeGrid_M_PrototypeGrid_Red.zmtrl");

		// Diagnostic logging — keeps failures discoverable from the harness log.
		Zenith_Log(LOG_CATEGORY_ASSET,
			"Materials_Test: count=%u default=%d expected=%d mapping=%d mapped='%s'",
			g_uRegisteredCount,
			(int)g_bDefaultPresent,
			(int)g_bExpectedPresent,
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

	if (g_uRegisteredCount < 30) return false;
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
