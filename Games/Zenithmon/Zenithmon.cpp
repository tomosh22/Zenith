#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "SaveData/Zenith_SaveData.h"
#include "Zenithmon/Components/ZM_GameComponent.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#endif

#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Core/Zenith_CommandLine.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#include <cstdlib>
#include <cstring>
#endif

// ============================================================================
// Zenithmon -- Pokemon-style monster-collecting RPG (see Docs/ for the GDD,
// roadmap, and stage plan).
//
// The game component registers with the component-meta registry via the
// static-init macro (NOT a direct call from Project_RegisterGameComponents: the
// registry is sealed before that hook runs). Dead-strip safe: this TU defines
// the Project_* entry points the engine references, so its static initializers
// always run. Serialization orders: ZM components claim 100+.
// ============================================================================
ZENITH_REGISTER_COMPONENT(ZM_GameComponent, "ZM_Game", 100u)
ZENITH_REGISTER_COMPONENT(ZM_TerrainGrass, "ZM_TerrainGrass", 101u)

#ifdef ZENITH_TOOLS
namespace
{
	MaterialHandle g_axDawnmereTerrainMaterials[4];

	bool ZM_HasCommandLineFlag(const char* szFlag)
	{
		for (int i = 1; i < __argc; ++i)
		{
			if (std::strcmp(__argv[i], szFlag) == 0)
			{
				return true;
			}
		}
		return false;
	}

	void ZM_InitializeDawnmereTerrainMaterials()
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		for (u_int uSlot = 0; uSlot < 4u; ++uSlot)
		{
			const ZM_TerrainMaterialSpec& xSpec = xRecipe.m_pxMaterials[uSlot];
			MaterialHandle& xHandle = g_axDawnmereTerrainMaterials[uSlot];
			xHandle = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();

			Zenith_MaterialAsset* pxMaterial = xHandle.GetDirect();
			pxMaterial->SetName(xSpec.m_szName);
			pxMaterial->SetBaseColor({
				xSpec.m_afBaseColour[0],
				xSpec.m_afBaseColour[1],
				xSpec.m_afBaseColour[2],
				xSpec.m_afBaseColour[3] });
			pxMaterial->SetRoughness(xSpec.m_fRoughness);
			pxMaterial->SetMetallic(xSpec.m_fMetallic);
		}
	}
}
#endif

const char* Project_GetName()
{
	return "Zenithmon";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterGameComponents()
{
	// Meta-registry registration is the ZENITH_REGISTER_COMPONENT macro above.
	// Tools builds additionally register with the editor "Add Component" menu
	// (append-anytime registry, not sealed).
#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_GameComponent>("ZM_Game");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_TerrainGrass>("ZM_TerrainGrass");
#endif

	// Save/load persistence root: %APPDATA%/Zenith/Zenithmon/. The versioned
	// per-module save schema lands at S7 (Docs/SaveFormat.md); initialising from
	// S0 keeps the test-hook plumbing live from the first commit.
	Zenith_SaveData::Initialise("Zenithmon");

#ifdef ZENITH_INPUT_SIMULATOR
	// Between-tests reset for batched automated tests. The harness force-loads
	// scene 0 before firing this hook, so entity-owned state is already cleared
	// via OnDestroy; only ownerless game globals need explicit reset here. Keep
	// this hook current as systems land (the DP hook is the reference).
	Zenith_AutomatedTestRunner::RegisterBetweenTestsHook([]()
	{
		Zenith_SaveData::ClearForTest();
	});
#endif
}

void Project_Shutdown()
{
#ifdef ZENITH_TOOLS
	for (MaterialHandle& xMaterial : g_axDawnmereTerrainMaterials)
	{
		xMaterial = MaterialHandle{};
	}
#endif
}

void Project_LoadInitialScene();	// forward decl for the automation step below

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// Automation borrows these handles while serializing Dawnmere. The saved
	// terrain owns its material data; these temporary handles live until shutdown.
	ZM_InitializeDawnmereTerrainMaterials();
}

// Boot-authored scene: a camera, a title, and the game component, saved to
// Assets/Scenes/FrontEnd.zscen (build index 0 -- the plan's scene table lives
// in Docs/GameDesignDocument.md). A _False / Android build LOADS that baked
// scene instead of authoring it (this function is tools-only), so the FIRST
// build+run must be a *_True config to bake FrontEnd.zscen.
void Project_RegisterEditorAutomationSteps()
{
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();

	xAuto.AddStep_CreateScene("FrontEnd");
	xAuto.AddStep_CreateEntity("GameManager");
	xAuto.AddStep_AddCamera();
	xAuto.AddStep_SetCameraPosition(0.f, 3.f, 6.f);
	xAuto.AddStep_SetCameraPitch(-0.4f);
	xAuto.AddStep_SetCameraFOV(glm::radians(60.f));
	xAuto.AddStep_SetAsMainCamera();
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIText("Title", "Zenithmon");
	xAuto.AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	xAuto.AddStep_SetUIPosition("Title", 0.f, -220.f);
	xAuto.AddStep_SetUIFontSize("Title", 54.f);
	xAuto.AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);
	xAuto.AddStep_AddComponent("ZM_Game");
	xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/FrontEnd" ZENITH_SCENE_EXT);
	xAuto.AddStep_UnloadScene();

	// Terrain rendering requires a graphics device. Headless runs retain the
	// FrontEnd logic scene above but neither mutate terrain assets nor author a
	// scene containing Terrain/Flux components.
	const bool bHeadless = Zenith_CommandLine::IsHeadless();
	ZM_TERRAIN_BAKE_QUEUE_RESULT eBakeResult = ZM_TERRAIN_BAKE_HEADLESS;
	if (!bHeadless)
	{
		const bool bForceTerrainBake = ZM_HasCommandLineFlag(szZM_FORCE_TERRAIN_BAKE_FLAG);
		eBakeResult = ZM_QueueDawnmereTerrainBake(xAuto, bHeadless, bForceTerrainBake);
	}
	if (eBakeResult == ZM_TERRAIN_BAKE_PREPARE_FAILED)
	{
		Zenith_Assert(false,
			"Dawnmere terrain bake preparation failed; suppressing scene authoring for this boot");
	}

	// A queued cold/forced bake completes this boot. Author the scene only on a
	// later windowed boot that observes the finalized manifest as warm.
	if (eBakeResult == ZM_TERRAIN_BAKE_WARM)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		const ZM_TerrainPreviewCameraSpec& xCamera = xRecipe.m_xPreviewCamera;
		const std::string strSplatmapPath = std::string("game:Terrain/") +
			xRecipe.m_pxWorldSpec->m_szTerrainSet + "/Splatmap_RGBA" ZENITH_TEXTURE_EXT;

		xAuto.AddStep_CreateScene("Dawnmere");
		xAuto.AddStep_CreateEntity("DawnmereTerrain");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_AddComponent("Terrain");
		xAuto.AddStep_TerrainSetAssetSet(xRecipe.m_pxWorldSpec->m_szTerrainSet);
		for (int iSlot = 0; iSlot < 4; ++iSlot)
		{
			xAuto.AddStep_SetTerrainMaterial(iSlot, g_axDawnmereTerrainMaterials[iSlot].GetDirect());
		}
		xAuto.AddStep_SetTerrainSplatmapPath(strSplatmapPath.c_str());
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_TERRAIN, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_TerrainGrass");

		xAuto.AddStep_CreateEntity("DawnmerePreviewCamera");
		xAuto.AddStep_AddCamera();
		xAuto.AddStep_SetCameraPosition(xCamera.m_xPosition.m_fX, xCamera.m_xPosition.m_fY, xCamera.m_xPosition.m_fZ);
		xAuto.AddStep_SetCameraYaw(xCamera.m_fYaw);
		xAuto.AddStep_SetCameraPitch(xCamera.m_fPitch);
		xAuto.AddStep_SetCameraFOV(glm::radians(xCamera.m_fFovDegrees));
		xAuto.AddStep_SetCameraNear(xCamera.m_fNearPlane);
		xAuto.AddStep_SetCameraFar(xCamera.m_fFarPlane);
		xAuto.AddStep_SetAsMainCamera();
		xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT);
		xAuto.AddStep_UnloadScene();
	}

	xAuto.AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/FrontEnd" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(2, GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
