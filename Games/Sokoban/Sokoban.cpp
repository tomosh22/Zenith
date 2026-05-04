#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "Sokoban/Components/Sokoban_Behaviour.h"
#include "Sokoban/Components/Sokoban_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "Prefab/Zenith_Prefab.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#endif

// ============================================================================
// Sokoban Resources - Global access for behaviours
// ============================================================================
namespace Sokoban
{
	MeshGeometryHandle g_xCubeAsset;
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	MaterialHandle g_xFloorMaterial;
	MaterialHandle g_xWallMaterial;
	MaterialHandle g_xBoxMaterial;
	MaterialHandle g_xBoxOnTargetMaterial;
	MaterialHandle g_xPlayerMaterial;
	MaterialHandle g_xTargetMaterial;

	// Prefabs for runtime instantiation (handles so they participate in ref counting)
	PrefabHandle g_xTilePrefab;
	PrefabHandle g_xBoxPrefab;
	PrefabHandle g_xPlayerPrefab;

	// Particle effects
	Flux_ParticleEmitterConfig* g_pxDustConfig = nullptr;
	Zenith_EntityID g_uDustEmitterID = INVALID_ENTITY_ID;
}

static bool s_bResourcesInitialized = false;

static void InitializeSokobanResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace Sokoban;

	g_xCubeAsset.Set(Zenith_MeshGeometryAsset::CreateUnitCube());
	g_pxCubeGeometry = g_xCubeAsset.GetDirect()->GetGeometry();

	// Use grid pattern texture with BaseColor for all materials.
	const TextureHandle& xGridTex = Flux_Graphics::s_xGridTexture;

	g_xFloorMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xFloorMaterial.GetDirect()->SetName("SokobanFloor");
	g_xFloorMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xFloorMaterial.GetDirect()->SetBaseColor({ 77.f/255.f, 77.f/255.f, 89.f/255.f, 1.f });

	g_xWallMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xWallMaterial.GetDirect()->SetName("SokobanWall");
	g_xWallMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xWallMaterial.GetDirect()->SetBaseColor({ 102.f/255.f, 64.f/255.f, 38.f/255.f, 1.f });

	g_xBoxMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xBoxMaterial.GetDirect()->SetName("SokobanBox");
	g_xBoxMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xBoxMaterial.GetDirect()->SetBaseColor({ 204.f/255.f, 128.f/255.f, 51.f/255.f, 1.f });

	g_xBoxOnTargetMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xBoxOnTargetMaterial.GetDirect()->SetName("SokobanBoxOnTarget");
	g_xBoxOnTargetMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xBoxOnTargetMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	g_xPlayerMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xPlayerMaterial.GetDirect()->SetName("SokobanPlayer");
	g_xPlayerMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xPlayerMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 102.f/255.f, 230.f/255.f, 1.f });

	g_xTargetMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xTargetMaterial.GetDirect()->SetName("SokobanTarget");
	g_xTargetMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xTargetMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 153.f/255.f, 51.f/255.f, 1.f });

	// Create prefabs for runtime instantiation
	// Note: Prefabs don't include ModelComponent because material varies by tile type
	// The behaviour adds ModelComponent with correct material at instantiation
	// Use the persistent scene here: InitializeResources runs before the initial scene
	// loads, and (post-A6) GetActiveScene returns INVALID until that happens.
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Tile prefab - basic entity (ModelComponent added at runtime with correct material)
	{
		Zenith_Entity xTileTemplate(pxSceneData, "TileTemplate");
		// No ModelComponent - added by behaviour with appropriate material

		Zenith_Prefab* pxTile = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxTile->CreateFromEntity(xTileTemplate, "Tile");
		g_xTilePrefab.Set(pxTile);

		Zenith_SceneManager::Destroy(xTileTemplate);
	}

	// Box prefab - basic entity
	{
		Zenith_Entity xBoxTemplate(pxSceneData, "BoxTemplate");
		// No ModelComponent - added by behaviour with appropriate material

		Zenith_Prefab* pxBox = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxBox->CreateFromEntity(xBoxTemplate, "Box");
		g_xBoxPrefab.Set(pxBox);

		Zenith_SceneManager::Destroy(xBoxTemplate);
	}

	// Player prefab - basic entity
	{
		Zenith_Entity xPlayerTemplate(pxSceneData, "PlayerTemplate");
		// No ModelComponent - added by behaviour with player material

		Zenith_Prefab* pxPlayer = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxPlayer->CreateFromEntity(xPlayerTemplate, "Player");
		g_xPlayerPrefab.Set(pxPlayer);

		Zenith_SceneManager::Destroy(xPlayerTemplate);
	}

	s_bResourcesInitialized = true;
}
// ============================================================================

const char* Project_GetName()
{
	return "Sokoban";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

const char* Project_GetGameAssetsDir() { return GAME_ASSETS_DIR; }

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeSokobanResources();

	// Create dust trail particle config (global resource used at runtime by behaviour)
	Sokoban::g_pxDustConfig = new Flux_ParticleEmitterConfig();
	Sokoban::g_pxDustConfig->m_fSpawnRate = 30.0f;
	Sokoban::g_pxDustConfig->m_uBurstCount = 0;
	Sokoban::g_pxDustConfig->m_uMaxParticles = 128;
	Sokoban::g_pxDustConfig->m_fLifetimeMin = 0.3f;
	Sokoban::g_pxDustConfig->m_fLifetimeMax = 0.6f;
	Sokoban::g_pxDustConfig->m_fSpeedMin = 0.5f;
	Sokoban::g_pxDustConfig->m_fSpeedMax = 1.5f;
	Sokoban::g_pxDustConfig->m_fSpreadAngleDegrees = 60.0f;
	Sokoban::g_pxDustConfig->m_xGravity = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
	Sokoban::g_pxDustConfig->m_fDrag = 2.0f;
	Sokoban::g_pxDustConfig->m_xColorStart = Zenith_Maths::Vector4(0.6f, 0.5f, 0.4f, 0.6f);
	Sokoban::g_pxDustConfig->m_xColorEnd = Zenith_Maths::Vector4(0.6f, 0.5f, 0.4f, 0.0f);
	Sokoban::g_pxDustConfig->m_fSizeStart = 0.15f;
	Sokoban::g_pxDustConfig->m_fSizeEnd = 0.25f;
	Sokoban::g_pxDustConfig->m_bUseGPUCompute = false;
	Flux_ParticleEmitterConfig::Register("Sokoban_DustTrail", Sokoban::g_pxDustConfig);

	// Sokoban_Behaviour auto-registers via ZENITH_BEHAVIOUR_TYPE_NAME (no explicit call needed)
}

void Project_Shutdown()
{
	// Drop asset handle refs before Zenith_AssetRegistry::Shutdown teardown.
	// Static handle destructors run after the registry is gone (use-after-free
	// territory) — explicit Clear() here is the lifecycle fix.
	Sokoban::g_xCubeAsset.Clear();
	Sokoban::g_xFloorMaterial.Clear();
	Sokoban::g_xWallMaterial.Clear();
	Sokoban::g_xBoxMaterial.Clear();
	Sokoban::g_xBoxOnTargetMaterial.Clear();
	Sokoban::g_xPlayerMaterial.Clear();
	Sokoban::g_xTargetMaterial.Clear();
	Sokoban::g_xTilePrefab.Clear();
	Sokoban::g_xBoxPrefab.Clear();
	Sokoban::g_xPlayerPrefab.Clear();

	// Clean up particle config
	delete Sokoban::g_pxDustConfig;
	Sokoban::g_pxDustConfig = nullptr;
	Sokoban::g_uDustEmitterID = INVALID_ENTITY_ID;
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All Sokoban resources initialized in Project_RegisterScriptBehaviours
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- MainMenu scene (build index 0) ----
	Zenith_EditorAutomation::AddStep_CreateScene("MainMenu");
	Zenith_EditorAutomation::AddStep_CreateEntity("MenuManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 12.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-1.5f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(45.f));
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIText("MenuTitle", "SOKOBAN");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuTitle", 72.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuTitle", 1.f, 1.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_CreateUIButton("MenuPlay", "Play");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	Zenith_EditorAutomation::AddStep_AttachScript("Sokoban_Behaviour");
	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- Sokoban gameplay scene (build index 1) ----
	Zenith_EditorAutomation::AddStep_CreateScene("Sokoban");
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 12.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-1.5f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(45.f));
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AddUI();

	// UI layout constants (matching original): margin=30, marginTop=30, baseText=15, lineH=24
	// Title
	Zenith_EditorAutomation::AddStep_CreateUIText("Title", "SOKOBAN");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Title", -30.f, 30.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Title", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Title", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Title", 72.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);

	// ControlsHeader (lineH*2 = 48)
	Zenith_EditorAutomation::AddStep_CreateUIText("ControlsHeader", "How to Play:");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ControlsHeader", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ControlsHeader", -30.f, 78.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("ControlsHeader", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("ControlsHeader", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("ControlsHeader", 54.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("ControlsHeader", 0.9f, 0.9f, 0.2f, 1.f);

	// MoveInstr (lineH*3 = 72)
	Zenith_EditorAutomation::AddStep_CreateUIText("MoveInstr", "WASD / Arrows: Move");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MoveInstr", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MoveInstr", -30.f, 102.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("MoveInstr", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("MoveInstr", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MoveInstr", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MoveInstr", 0.8f, 0.8f, 0.8f, 1.f);

	// ResetInstr (lineH*4 = 96)
	Zenith_EditorAutomation::AddStep_CreateUIText("ResetInstr", "R: New Level  Esc: Menu");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ResetInstr", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ResetInstr", -30.f, 126.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("ResetInstr", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("ResetInstr", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("ResetInstr", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("ResetInstr", 0.8f, 0.8f, 0.8f, 1.f);

	// GoalHeader (lineH*6 = 144)
	Zenith_EditorAutomation::AddStep_CreateUIText("GoalHeader", "Goal:");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("GoalHeader", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("GoalHeader", -30.f, 174.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("GoalHeader", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("GoalHeader", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("GoalHeader", 54.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("GoalHeader", 0.9f, 0.9f, 0.2f, 1.f);

	// GoalDesc (lineH*7 = 168)
	Zenith_EditorAutomation::AddStep_CreateUIText("GoalDesc", "Push boxes onto targets");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("GoalDesc", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("GoalDesc", -30.f, 198.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("GoalDesc", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("GoalDesc", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("GoalDesc", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("GoalDesc", 0.8f, 0.8f, 0.8f, 1.f);

	// Status (lineH*9 = 216)
	Zenith_EditorAutomation::AddStep_CreateUIText("Status", "Moves: 0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Status", -30.f, 246.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Status", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Status", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Status", 0.6f, 0.8f, 1.f, 1.f);

	// Progress (lineH*10 = 240)
	Zenith_EditorAutomation::AddStep_CreateUIText("Progress", "Boxes: 0 / 3");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Progress", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Progress", -30.f, 270.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Progress", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Progress", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Progress", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Progress", 0.6f, 0.8f, 1.f, 1.f);

	// MinMoves (lineH*11 = 264)
	Zenith_EditorAutomation::AddStep_CreateUIText("MinMoves", "Min Moves: 0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MinMoves", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MinMoves", -30.f, 294.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("MinMoves", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("MinMoves", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MinMoves", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MinMoves", 0.6f, 0.8f, 1.f, 1.f);

	// WinText (lineH*13 = 312)
	Zenith_EditorAutomation::AddStep_CreateUIText("WinText", "");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("WinText", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	Zenith_EditorAutomation::AddStep_SetUIPosition("WinText", -30.f, 342.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("WinText", static_cast<int>(Zenith_UI::TextAlignment::Right));
	Zenith_EditorAutomation::AddStep_SetUIVisible("WinText", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("WinText", 63.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("WinText", 0.2f, 1.f, 0.2f, 1.f);

	// LoadingText (centered)
	Zenith_EditorAutomation::AddStep_CreateUIText("LoadingText", "Generating puzzle...");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("LoadingText", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("LoadingText", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("LoadingText", 36.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("LoadingText", 1.f, 1.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("LoadingText", false);

	// DustEmitter entity
	Zenith_EditorAutomation::AddStep_CreateEntity("DustEmitter");
	Zenith_EditorAutomation::AddStep_AddParticleEmitter();
	Zenith_EditorAutomation::AddStep_SetParticleConfig(Sokoban::g_pxDustConfig);

	// Back to GameManager for script
	Zenith_EditorAutomation::AddStep_SelectEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AttachScript("Sokoban_Behaviour");

	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Sokoban" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- Final scene loading ----
	Zenith_EditorAutomation::AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	Zenith_SceneManager::RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Sokoban" ZENITH_SCENE_EXT);
	Zenith_SceneManager::LoadSceneByIndexBlockingForBootstrap(0, SCENE_LOAD_SINGLE);
}
