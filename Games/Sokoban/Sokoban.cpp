#include "Zenith.h"

#include "Sokoban/Components/Sokoban_Behaviour.h"
#include "Sokoban/Components/Sokoban_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "AssetHandling/Zenith_DataAssetManager.h"
#include "Prefab/Zenith_Prefab.h"

// ============================================================================
// Sokoban Resources - Global access for behaviours
// ============================================================================
namespace Sokoban
{
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MaterialAsset* g_pxFloorMaterial = nullptr;
	Flux_MaterialAsset* g_pxWallMaterial = nullptr;
	Flux_MaterialAsset* g_pxBoxMaterial = nullptr;
	Flux_MaterialAsset* g_pxBoxOnTargetMaterial = nullptr;
	Flux_MaterialAsset* g_pxPlayerMaterial = nullptr;
	Flux_MaterialAsset* g_pxTargetMaterial = nullptr;

	// Prefabs for runtime instantiation
	Zenith_Prefab* g_pxTilePrefab = nullptr;
	Zenith_Prefab* g_pxBoxPrefab = nullptr;
	Zenith_Prefab* g_pxPlayerPrefab = nullptr;

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

	g_pxCubeGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*g_pxCubeGeometry);

	// Use grid pattern texture with BaseColor for all materials
	Flux_Texture* pxGridTex = &Flux_Graphics::s_xGridPatternTexture2D;

	g_pxFloorMaterial = Flux_MaterialAsset::Create("SokobanFloor");
	g_pxFloorMaterial->SetDiffuseTexture(pxGridTex);
	g_pxFloorMaterial->SetBaseColor({ 77.f/255.f, 77.f/255.f, 89.f/255.f, 1.f });

	g_pxWallMaterial = Flux_MaterialAsset::Create("SokobanWall");
	g_pxWallMaterial->SetDiffuseTexture(pxGridTex);
	g_pxWallMaterial->SetBaseColor({ 102.f/255.f, 64.f/255.f, 38.f/255.f, 1.f });

	g_pxBoxMaterial = Flux_MaterialAsset::Create("SokobanBox");
	g_pxBoxMaterial->SetDiffuseTexture(pxGridTex);
	g_pxBoxMaterial->SetBaseColor({ 204.f/255.f, 128.f/255.f, 51.f/255.f, 1.f });

	g_pxBoxOnTargetMaterial = Flux_MaterialAsset::Create("SokobanBoxOnTarget");
	g_pxBoxOnTargetMaterial->SetDiffuseTexture(pxGridTex);
	g_pxBoxOnTargetMaterial->SetBaseColor({ 51.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	g_pxPlayerMaterial = Flux_MaterialAsset::Create("SokobanPlayer");
	g_pxPlayerMaterial->SetDiffuseTexture(pxGridTex);
	g_pxPlayerMaterial->SetBaseColor({ 51.f/255.f, 102.f/255.f, 230.f/255.f, 1.f });

	g_pxTargetMaterial = Flux_MaterialAsset::Create("SokobanTarget");
	g_pxTargetMaterial->SetDiffuseTexture(pxGridTex);
	g_pxTargetMaterial->SetBaseColor({ 51.f/255.f, 153.f/255.f, 51.f/255.f, 1.f });

	// Create prefabs for runtime instantiation
	// Note: Prefabs don't include ModelComponent because material varies by tile type
	// The behaviour adds ModelComponent with correct material at instantiation
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Tile prefab - basic entity (ModelComponent added at runtime with correct material)
	{
		Zenith_Entity xTileTemplate(&xScene, "TileTemplate");
		// No ModelComponent - added by behaviour with appropriate material

		g_pxTilePrefab = new Zenith_Prefab();
		g_pxTilePrefab->CreateFromEntity(xTileTemplate, "Tile");

		Zenith_Scene::Destroy(xTileTemplate);
	}

	// Box prefab - basic entity
	{
		Zenith_Entity xBoxTemplate(&xScene, "BoxTemplate");
		// No ModelComponent - added by behaviour with appropriate material

		g_pxBoxPrefab = new Zenith_Prefab();
		g_pxBoxPrefab->CreateFromEntity(xBoxTemplate, "Box");

		Zenith_Scene::Destroy(xBoxTemplate);
	}

	// Player prefab - basic entity
	{
		Zenith_Entity xPlayerTemplate(&xScene, "PlayerTemplate");
		// No ModelComponent - added by behaviour with player material

		g_pxPlayerPrefab = new Zenith_Prefab();
		g_pxPlayerPrefab->CreateFromEntity(xPlayerTemplate, "Player");

		Zenith_Scene::Destroy(xPlayerTemplate);
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

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeSokobanResources();

	// Register DataAsset types
	RegisterSokobanDataAssets();

	Sokoban_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Clean up particle config
	delete Sokoban::g_pxDustConfig;
	Sokoban::g_pxDustConfig = nullptr;
	Sokoban::g_uDustEmitterID = INVALID_ENTITY_ID;
}

void Project_LoadInitialScene()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	xScene.Reset();

	Zenith_Entity xCameraEntity(&xScene, "MainCamera");
	xCameraEntity.SetTransient(false);  // Persistent - will be saved to scene
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	// Top-down 3D view: camera directly above the grid, looking straight down
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.f, 12.f, 0.f),  // Position: 12 up, centered
		-1.5f,  // Pitch: -1.5 radians (nearly straight down)
		0.f,    // Yaw: 0 degrees
		glm::radians(45.f),   // FOV: 45 degrees
		0.1f,
		1000.f,
		16.f / 9.f
	);
	xScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	Zenith_Entity xSokobanEntity(&xScene, "SokobanGame");
	xSokobanEntity.SetTransient(false);  // Persistent - will be saved to scene

	// UI Setup - anchored to top-right corner of screen
	static constexpr float s_fMarginRight = 30.f;  // Offset from right edge
	static constexpr float s_fMarginTop = 30.f;    // Offset from top edge
	static constexpr float s_fBaseTextSize = 15.f;
	static constexpr float s_fLineHeight = 24.f;

	Zenith_UIComponent& xUI = xSokobanEntity.AddComponent<Zenith_UIComponent>();

	// Helper lambda to set up top-right anchored text
	auto SetupTopRightText = [](Zenith_UI::Zenith_UIText* pxText, float fYOffset)
	{
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopRight);
		pxText->SetPosition(-s_fMarginRight, s_fMarginTop + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Right);
	};

	Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "SOKOBAN");
	SetupTopRightText(pxTitle, 0.f);
	pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
	pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("ControlsHeader", "How to Play:");
	SetupTopRightText(pxControls, s_fLineHeight * 2);
	pxControls->SetFontSize(s_fBaseTextSize * 3.6f);
	pxControls->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	Zenith_UI::Zenith_UIText* pxMove = xUI.CreateText("MoveInstr", "WASD / Arrows: Move");
	SetupTopRightText(pxMove, s_fLineHeight * 3);
	pxMove->SetFontSize(s_fBaseTextSize * 3.0f);
	pxMove->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxReset = xUI.CreateText("ResetInstr", "R: Reset Level");
	SetupTopRightText(pxReset, s_fLineHeight * 4);
	pxReset->SetFontSize(s_fBaseTextSize * 3.0f);
	pxReset->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxGoal = xUI.CreateText("GoalHeader", "Goal:");
	SetupTopRightText(pxGoal, s_fLineHeight * 6);
	pxGoal->SetFontSize(s_fBaseTextSize * 3.6f);
	pxGoal->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	Zenith_UI::Zenith_UIText* pxGoalDesc = xUI.CreateText("GoalDesc", "Push boxes onto targets");
	SetupTopRightText(pxGoalDesc, s_fLineHeight * 7);
	pxGoalDesc->SetFontSize(s_fBaseTextSize * 3.0f);
	pxGoalDesc->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "Moves: 0");
	SetupTopRightText(pxStatus, s_fLineHeight * 9);
	pxStatus->SetFontSize(s_fBaseTextSize * 3.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxProgress = xUI.CreateText("Progress", "Boxes: 0 / 3");
	SetupTopRightText(pxProgress, s_fLineHeight * 10);
	pxProgress->SetFontSize(s_fBaseTextSize * 3.0f);
	pxProgress->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxMinMoves = xUI.CreateText("MinMoves", "Min Moves: 0");
	SetupTopRightText(pxMinMoves, s_fLineHeight * 11);
	pxMinMoves->SetFontSize(s_fBaseTextSize * 3.0f);
	pxMinMoves->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxWin = xUI.CreateText("WinText", "");
	SetupTopRightText(pxWin, s_fLineHeight * 13);
	pxWin->SetFontSize(s_fBaseTextSize * 4.2f);
	pxWin->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));

	// Create dust trail particle config programmatically
	Sokoban::g_pxDustConfig = new Flux_ParticleEmitterConfig();
	Sokoban::g_pxDustConfig->m_fSpawnRate = 30.0f;              // Continuous while moving
	Sokoban::g_pxDustConfig->m_uBurstCount = 0;                 // Not burst mode
	Sokoban::g_pxDustConfig->m_uMaxParticles = 128;
	Sokoban::g_pxDustConfig->m_fLifetimeMin = 0.3f;
	Sokoban::g_pxDustConfig->m_fLifetimeMax = 0.6f;
	Sokoban::g_pxDustConfig->m_fSpeedMin = 0.5f;
	Sokoban::g_pxDustConfig->m_fSpeedMax = 1.5f;
	Sokoban::g_pxDustConfig->m_fSpreadAngleDegrees = 60.0f;
	Sokoban::g_pxDustConfig->m_xGravity = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);  // Light settling
	Sokoban::g_pxDustConfig->m_fDrag = 2.0f;
	Sokoban::g_pxDustConfig->m_xColorStart = Zenith_Maths::Vector4(0.6f, 0.5f, 0.4f, 0.6f);  // Brown
	Sokoban::g_pxDustConfig->m_xColorEnd = Zenith_Maths::Vector4(0.6f, 0.5f, 0.4f, 0.0f);    // Transparent
	Sokoban::g_pxDustConfig->m_fSizeStart = 0.15f;
	Sokoban::g_pxDustConfig->m_fSizeEnd = 0.25f;              // Expand as dissipate
	Sokoban::g_pxDustConfig->m_bUseGPUCompute = false;        // CPU for simple effect

	// Register config for scene restore after editor Play/Stop
	Flux_ParticleEmitterConfig::Register("Sokoban_DustTrail", Sokoban::g_pxDustConfig);

	// Create particle emitter entity for dust
	Zenith_Entity xDustEmitter(&xScene, "DustEmitter");
	xDustEmitter.SetTransient(false);
	Zenith_ParticleEmitterComponent& xEmitter = xDustEmitter.AddComponent<Zenith_ParticleEmitterComponent>();
	xEmitter.SetConfig(Sokoban::g_pxDustConfig);
	Sokoban::g_uDustEmitterID = xDustEmitter.GetEntityID();

	// Add script component with Sokoban behaviour
	// Resources are automatically obtained from Sokoban:: namespace in OnCreate()
	Zenith_ScriptComponent& xScript = xSokobanEntity.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<Sokoban_Behaviour>();

	// Save the scene file
	std::string strScenePath = std::string(GAME_ASSETS_DIR) + "/Scenes/Sokoban.zscn";
	std::filesystem::create_directories(std::string(GAME_ASSETS_DIR) + "/Scenes");
	xScene.SaveToFile(strScenePath);
}
