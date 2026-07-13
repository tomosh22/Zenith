#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "SaveData/Zenith_SaveData.h"
#include "Zenithmon/Components/ZM_GameComponent.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#endif

#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Core/Zenith_CommandLine.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#include <filesystem>
#include <string>
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
ZENITH_REGISTER_COMPONENT(ZM_PlayerController, "ZM_PlayerController", 102u)
ZENITH_REGISTER_COMPONENT(ZM_FollowCamera, "ZM_FollowCamera", 103u)
ZENITH_REGISTER_COMPONENT(ZM_GameStateManager, "ZM_GameStateManager", 104u)
ZENITH_REGISTER_COMPONENT(ZM_SpawnPoint, "ZM_SpawnPoint", 105u)
ZENITH_REGISTER_COMPONENT(ZM_WarpTrigger, "ZM_WarpTrigger", 106u)

#ifdef ZENITH_TOOLS
namespace
{
	MaterialHandle g_axDawnmereTerrainMaterials[4];

	void ZM_ConfigureTownCenterSpawnPoint()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		ZM_SpawnPoint* pxSpawnPoint = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<ZM_SpawnPoint>()
			: nullptr;
		Zenith_Assert(pxSpawnPoint != nullptr,
			"TownCenter authoring requires the selected ZM_SpawnPoint");
		if (pxSpawnPoint == nullptr)
		{
			return;
		}

		const bool bTagSet = pxSpawnPoint->SetTag("TownCenter");
		Zenith_Assert(bTagSet, "TownCenter is not a valid spawn tag");
	}

	const char* ZM_TerrainBakeQueueResultToString(
		ZM_TERRAIN_BAKE_QUEUE_RESULT eResult)
	{
		switch (eResult)
		{
		case ZM_TERRAIN_BAKE_HEADLESS: return "HEADLESS";
		case ZM_TERRAIN_BAKE_WARM: return "WARM";
		case ZM_TERRAIN_BAKE_QUEUED: return "QUEUED";
		case ZM_TERRAIN_BAKE_PREPARE_FAILED: return "PREPARE_FAILED";
		default: return "INVALID";
		}
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
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_PlayerController>("ZM_PlayerController");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_FollowCamera>("ZM_FollowCamera");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_GameStateManager>("ZM_GameStateManager");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_SpawnPoint>("ZM_SpawnPoint");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_WarpTrigger>("ZM_WarpTrigger");
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
		ZM_GameStateManager::ResetRuntimeStateForTests();
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
	ZM_TerrainBakeSelection xTerrainSelection;
	if (!ZM_ParseTerrainBakeSelection(__argc, __argv, xTerrainSelection))
	{
		const bool bHasErrorArgument = xTerrainSelection.m_iErrorArgument >= 0 &&
			xTerrainSelection.m_iErrorArgument < __argc && __argv != nullptr &&
			__argv[xTerrainSelection.m_iErrorArgument] != nullptr;
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Selector rejected: result=%s, argvIndex=%d, argument='%s'; no automation queued",
			ZM_TerrainBakeSelectionParseResultToString(
				xTerrainSelection.m_eParseResult),
			xTerrainSelection.m_iErrorArgument,
			bHasErrorArgument ? __argv[xTerrainSelection.m_iErrorArgument] : "<null>");
		Zenith_Assert(false,
			"Invalid Zenithmon terrain-bake selector at argv index %d",
			xTerrainSelection.m_iErrorArgument);
		return;
	}

	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();

	xAuto.AddStep_CreateScene("FrontEnd");
	xAuto.AddStep_CreateEntity("ZM_GameStateRoot");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_AddComponent("ZM_GameStateManager");

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
	ZM_TerrainBakeBatchPlan xTerrainBatch;
	if (bHeadless)
	{
		xTerrainBatch = ZM_BuildTerrainBakeBatchPlan(
			xTerrainSelection, true, 0u);
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Batch result: mode=%s, result=HEADLESS, probes=0, warmMask=0x0, queueMask=0x0, queued=0, sceneAuthoring=DEFERRED",
			ZM_TerrainBakeSelectionModeToString(xTerrainSelection.m_eMode));
	}
	else
	{
		u_int uWarmRecipeMask = 0u;
		const u_int uRecipeCount = ZM_GetTerrainAuthoringRecipeCount();
		for (u_int i = 0; i < uRecipeCount; ++i)
		{
			const ZM_TerrainAuthoringRecipe& xRecipe =
				ZM_GetTerrainAuthoringRecipe(i);
			const bool bWarm = ZM_IsTerrainBakeWarm(
				xRecipe, std::filesystem::path(GAME_ASSETS_DIR));
			if (bWarm)
			{
				uWarmRecipeMask |= 1u << i;
			}
			Zenith_Log(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch probe: index=%u, set='%s', warm=%s",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet,
				bWarm ? "true" : "false");
		}

		xTerrainBatch = ZM_BuildTerrainBakeBatchPlan(
			xTerrainSelection, false, uWarmRecipeMask);
		u_int uQueuedRecipeCount = 0u;
		for (u_int i = 0; i < uRecipeCount; ++i)
		{
			const ZM_TerrainAuthoringRecipe& xRecipe =
				ZM_GetTerrainAuthoringRecipe(i);
			const u_int uRecipeBit = 1u << i;
			const bool bQueue =
				(xTerrainBatch.m_uQueueRecipeMask & uRecipeBit) != 0u;
			const bool bWarm = (uWarmRecipeMask & uRecipeBit) != 0u;
			const char* szDecision = bQueue ? "QUEUE" :
				(bWarm ? "SKIP_WARM" : "SKIP_FILTERED");
			Zenith_Log(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch decision: index=%u, set='%s', action=%s",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet, szDecision);
			if (!bQueue)
			{
				continue;
			}

			const bool bForce = xTerrainSelection.m_eMode !=
				ZM_TERRAIN_BAKE_SELECTION_AUTO_MISSING;
			const ZM_TERRAIN_BAKE_QUEUE_RESULT eQueueResult =
				ZM_QueueTerrainBake(xAuto, xRecipe, false, bForce);
			Zenith_Log(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch queue: index=%u, set='%s', result=%s",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet,
				ZM_TerrainBakeQueueResultToString(eQueueResult));
			if (eQueueResult == ZM_TERRAIN_BAKE_QUEUED)
			{
				++uQueuedRecipeCount;
				continue;
			}
			if (eQueueResult == ZM_TERRAIN_BAKE_WARM && !bForce)
			{
				// A cold-to-warm race is harmless, but the immutable pre-scan
				// plan still defers scene authoring until the next boot.
				continue;
			}

			// Preparation may fail after earlier recipes appended actions.
			// Reset makes this boot all-or-nothing: no partial batch executes.
			xAuto.Reset();
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch aborted: index=%u, set='%s', result=%s; automation reset",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet,
				ZM_TerrainBakeQueueResultToString(eQueueResult));
			Zenith_Assert(false,
				"Terrain bake batch preparation failed for %s",
				xRecipe.m_pxWorldSpec->m_szTerrainSet);
			return;
		}

		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Batch result: mode=%s, result=%s, probes=%u, warmMask=0x%X, queueMask=0x%X, queued=%u, sceneAuthoring=%s",
			ZM_TerrainBakeSelectionModeToString(xTerrainSelection.m_eMode),
			xTerrainBatch.m_uQueueRecipeMask == 0u ? "NO_QUEUE" : "QUEUED",
			uRecipeCount, xTerrainBatch.m_uWarmRecipeMask,
			xTerrainBatch.m_uQueueRecipeMask, uQueuedRecipeCount,
			xTerrainBatch.m_bAuthorDawnmereScene ?
				"AUTHOR_DAWNMERE" : "DEFERRED");
	}

	// A queued cold/forced batch completes this boot. Author Dawnmere only when
	// the windowed pre-scan found every registered terrain warm and queued none.
	// Thornacre and Route1 remain measurement-only recipes in this milestone.
	if (xTerrainBatch.m_bAuthorDawnmereScene)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		const ZM_TerrainPreviewCameraSpec& xCamera = xRecipe.m_xPreviewCamera;
		const std::string strSplatmapPath = std::string("game:Terrain/") +
			xRecipe.m_pxWorldSpec->m_szTerrainSet + "/Splatmap_RGBA" ZENITH_TEXTURE_EXT;
		const Zenith_Maths::Vector3 xTownCenterFeet(512.0f, 25.98577f, 480.0f);
		const Zenith_Maths::Vector3 xPlayerScale(0.8f, 1.8f, 0.8f);
		const float fPlayerCapsuleHalfExtent =
			ZM_PlayerController::CalculateCapsuleHalfExtent(xPlayerScale);
		const Zenith_Maths::Vector3 xPlayerCenter =
			xTownCenterFeet + Zenith_Maths::Vector3(
				0.0f, fPlayerCapsuleHalfExtent, 0.0f);

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

		// Spawn markers are feet/surface anchors. Runtime warps and this authored
		// preview placement share the controller's scale-derived capsule extent.
		xAuto.AddStep_CreateEntity("TownCenterSpawn");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xTownCenterFeet.x, xTownCenterFeet.y, xTownCenterFeet.z);
		xAuto.AddStep_AddComponent("ZM_SpawnPoint");
		xAuto.AddStep_Custom(&ZM_ConfigureTownCenterSpawnPoint);

		// The player and camera are Dawnmere-owned. SINGLE scene loads therefore
		// replace both entities instead of carrying movement/camera state between
		// scenes. TownCenter is the exact sampled terrain surface; adding the
		// scale-derived 0.9 m capsule half-extent produces the authored centre.
		xAuto.AddStep_CreateEntity("Player");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xPlayerCenter.x, xPlayerCenter.y, xPlayerCenter.z);
		xAuto.AddStep_SetTransformScale(
			xPlayerScale.x, xPlayerScale.y, xPlayerScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AddComponent("ZM_PlayerController");

		xAuto.AddStep_CreateEntity("DawnmerePreviewCamera");
		xAuto.AddStep_AddCamera();
		xAuto.AddStep_SetCameraPosition(xCamera.m_xPosition.m_fX, xCamera.m_xPosition.m_fY, xCamera.m_xPosition.m_fZ);
		xAuto.AddStep_SetCameraYaw(0.0f);
		xAuto.AddStep_SetCameraPitch(xCamera.m_fPitch);
		xAuto.AddStep_SetCameraFOV(glm::radians(xCamera.m_fFovDegrees));
		xAuto.AddStep_SetCameraNear(xCamera.m_fNearPlane);
		xAuto.AddStep_SetCameraFar(xCamera.m_fFarPlane);
		xAuto.AddStep_AddComponent("ZM_FollowCamera");
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
