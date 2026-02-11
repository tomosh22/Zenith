#include "Zenith.h"

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

// ============================================================================
// Sokoban Resources - Global access for behaviours
// ============================================================================
namespace Sokoban
{
	Zenith_MeshGeometryAsset* g_pxCubeAsset = nullptr;
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	MaterialHandle g_xFloorMaterial;
	MaterialHandle g_xWallMaterial;
	MaterialHandle g_xBoxMaterial;
	MaterialHandle g_xBoxOnTargetMaterial;
	MaterialHandle g_xPlayerMaterial;
	MaterialHandle g_xTargetMaterial;

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

	g_pxCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_pxCubeGeometry = g_pxCubeAsset->GetGeometry();

	// Use grid pattern texture with BaseColor for all materials
	Zenith_TextureAsset* pxGridTex = Flux_Graphics::s_pxGridTexture;

	auto& xRegistry = Zenith_AssetRegistry::Get();
	g_xFloorMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xFloorMaterial.Get()->SetName("SokobanFloor");
	g_xFloorMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xFloorMaterial.Get()->SetBaseColor({ 77.f/255.f, 77.f/255.f, 89.f/255.f, 1.f });

	g_xWallMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xWallMaterial.Get()->SetName("SokobanWall");
	g_xWallMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xWallMaterial.Get()->SetBaseColor({ 102.f/255.f, 64.f/255.f, 38.f/255.f, 1.f });

	g_xBoxMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xBoxMaterial.Get()->SetName("SokobanBox");
	g_xBoxMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xBoxMaterial.Get()->SetBaseColor({ 204.f/255.f, 128.f/255.f, 51.f/255.f, 1.f });

	g_xBoxOnTargetMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xBoxOnTargetMaterial.Get()->SetName("SokobanBoxOnTarget");
	g_xBoxOnTargetMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xBoxOnTargetMaterial.Get()->SetBaseColor({ 51.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	g_xPlayerMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xPlayerMaterial.Get()->SetName("SokobanPlayer");
	g_xPlayerMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xPlayerMaterial.Get()->SetBaseColor({ 51.f/255.f, 102.f/255.f, 230.f/255.f, 1.f });

	g_xTargetMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xTargetMaterial.Get()->SetName("SokobanTarget");
	g_xTargetMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xTargetMaterial.Get()->SetBaseColor({ 51.f/255.f, 153.f/255.f, 51.f/255.f, 1.f });

	// Create prefabs for runtime instantiation
	// Note: Prefabs don't include ModelComponent because material varies by tile type
	// The behaviour adds ModelComponent with correct material at instantiation
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Tile prefab - basic entity (ModelComponent added at runtime with correct material)
	{
		Zenith_Entity xTileTemplate(pxSceneData, "TileTemplate");
		// No ModelComponent - added by behaviour with appropriate material

		g_pxTilePrefab = new Zenith_Prefab();
		g_pxTilePrefab->CreateFromEntity(xTileTemplate, "Tile");

		Zenith_SceneManager::Destroy(xTileTemplate);
	}

	// Box prefab - basic entity
	{
		Zenith_Entity xBoxTemplate(pxSceneData, "BoxTemplate");
		// No ModelComponent - added by behaviour with appropriate material

		g_pxBoxPrefab = new Zenith_Prefab();
		g_pxBoxPrefab->CreateFromEntity(xBoxTemplate, "Box");

		Zenith_SceneManager::Destroy(xBoxTemplate);
	}

	// Player prefab - basic entity
	{
		Zenith_Entity xPlayerTemplate(pxSceneData, "PlayerTemplate");
		// No ModelComponent - added by behaviour with player material

		g_pxPlayerPrefab = new Zenith_Prefab();
		g_pxPlayerPrefab->CreateFromEntity(xPlayerTemplate, "Player");

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

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeSokobanResources();

	Sokoban_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Clean up particle config
	delete Sokoban::g_pxDustConfig;
	Sokoban::g_pxDustConfig = nullptr;
	Sokoban::g_uDustEmitterID = INVALID_ENTITY_ID;
}

void Project_CreateScenes()
{
	// Create dust trail particle config (global resource)
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

	// ---- MainMenu scene (build index 0) ----
	{
		const std::string strMenuPath = GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT;

		Zenith_Scene xMenuScene = Zenith_SceneManager::CreateEmptyScene("MainMenu");
		Zenith_SceneData* pxMenuData = Zenith_SceneManager::GetSceneData(xMenuScene);

		Zenith_Entity xMenuManager(pxMenuData, "MenuManager");
		xMenuManager.SetTransient(false);

		Zenith_CameraComponent& xCamera = xMenuManager.AddComponent<Zenith_CameraComponent>();
		xCamera.InitialisePerspective(
			Zenith_Maths::Vector3(0.f, 12.f, 0.f),
			-1.5f, 0.f,
			glm::radians(45.f), 0.1f, 1000.f, 16.f / 9.f
		);
		pxMenuData->SetMainCameraEntity(xMenuManager.GetEntityID());

		Zenith_UIComponent& xUI = xMenuManager.AddComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIText* pxMenuTitle = xUI.CreateText("MenuTitle", "SOKOBAN");
		pxMenuTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxMenuTitle->SetPosition(0.f, -120.f);
		pxMenuTitle->SetFontSize(72.f);
		pxMenuTitle->SetColor({1.f, 1.f, 1.f, 1.f});

		Zenith_UI::Zenith_UIButton* pxPlayBtn = xUI.CreateButton("MenuPlay", "Play");
		pxPlayBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxPlayBtn->SetPosition(0.f, 0.f);
		pxPlayBtn->SetSize(200.f, 50.f);

		Zenith_ScriptComponent& xScript = xMenuManager.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviourForSerialization<Sokoban_Behaviour>();

		pxMenuData->SaveToFile(strMenuPath);
		Zenith_SceneManager::RegisterSceneBuildIndex(0, strMenuPath);
		Zenith_SceneManager::UnloadScene(xMenuScene);
	}

	// ---- Sokoban gameplay scene (build index 1) ----
	{
		const std::string strGamePath = GAME_ASSETS_DIR "Scenes/Sokoban" ZENITH_SCENE_EXT;

		Zenith_Scene xGameScene = Zenith_SceneManager::CreateEmptyScene("Sokoban");
		Zenith_SceneData* pxGameData = Zenith_SceneManager::GetSceneData(xGameScene);

		Zenith_Entity xGameManager(pxGameData, "GameManager");
		xGameManager.SetTransient(false);

		Zenith_CameraComponent& xCamera = xGameManager.AddComponent<Zenith_CameraComponent>();
		xCamera.InitialisePerspective(
			Zenith_Maths::Vector3(0.f, 12.f, 0.f),
			-1.5f, 0.f,
			glm::radians(45.f), 0.1f, 1000.f, 16.f / 9.f
		);
		pxGameData->SetMainCameraEntity(xGameManager.GetEntityID());

		Zenith_UIComponent& xUI = xGameManager.AddComponent<Zenith_UIComponent>();

		static constexpr float s_fMarginRight = 30.f;
		static constexpr float s_fMarginTop = 30.f;
		static constexpr float s_fBaseTextSize = 15.f;
		static constexpr float s_fLineHeight = 24.f;

		auto SetupTopRightText = [](Zenith_UI::Zenith_UIText* pxText, float fYOffset, bool bVisible)
		{
			pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopRight);
			pxText->SetPosition(-s_fMarginRight, s_fMarginTop + fYOffset);
			pxText->SetAlignment(Zenith_UI::TextAlignment::Right);
			pxText->SetVisible(bVisible);
		};

		Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "SOKOBAN");
		SetupTopRightText(pxTitle, 0.f, false);
		pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
		pxTitle->SetColor({1.f, 1.f, 1.f, 1.f});

		Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("ControlsHeader", "How to Play:");
		SetupTopRightText(pxControls, s_fLineHeight * 2, false);
		pxControls->SetFontSize(s_fBaseTextSize * 3.6f);
		pxControls->SetColor({0.9f, 0.9f, 0.2f, 1.f});

		Zenith_UI::Zenith_UIText* pxMove = xUI.CreateText("MoveInstr", "WASD / Arrows: Move");
		SetupTopRightText(pxMove, s_fLineHeight * 3, false);
		pxMove->SetFontSize(s_fBaseTextSize * 3.0f);
		pxMove->SetColor({0.8f, 0.8f, 0.8f, 1.f});

		Zenith_UI::Zenith_UIText* pxReset = xUI.CreateText("ResetInstr", "R: New Level  Esc: Menu");
		SetupTopRightText(pxReset, s_fLineHeight * 4, false);
		pxReset->SetFontSize(s_fBaseTextSize * 3.0f);
		pxReset->SetColor({0.8f, 0.8f, 0.8f, 1.f});

		Zenith_UI::Zenith_UIText* pxGoal = xUI.CreateText("GoalHeader", "Goal:");
		SetupTopRightText(pxGoal, s_fLineHeight * 6, false);
		pxGoal->SetFontSize(s_fBaseTextSize * 3.6f);
		pxGoal->SetColor({0.9f, 0.9f, 0.2f, 1.f});

		Zenith_UI::Zenith_UIText* pxGoalDesc = xUI.CreateText("GoalDesc", "Push boxes onto targets");
		SetupTopRightText(pxGoalDesc, s_fLineHeight * 7, false);
		pxGoalDesc->SetFontSize(s_fBaseTextSize * 3.0f);
		pxGoalDesc->SetColor({0.8f, 0.8f, 0.8f, 1.f});

		Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "Moves: 0");
		SetupTopRightText(pxStatus, s_fLineHeight * 9, false);
		pxStatus->SetFontSize(s_fBaseTextSize * 3.0f);
		pxStatus->SetColor({0.6f, 0.8f, 1.f, 1.f});

		Zenith_UI::Zenith_UIText* pxProgress = xUI.CreateText("Progress", "Boxes: 0 / 3");
		SetupTopRightText(pxProgress, s_fLineHeight * 10, false);
		pxProgress->SetFontSize(s_fBaseTextSize * 3.0f);
		pxProgress->SetColor({0.6f, 0.8f, 1.f, 1.f});

		Zenith_UI::Zenith_UIText* pxMinMoves = xUI.CreateText("MinMoves", "Min Moves: 0");
		SetupTopRightText(pxMinMoves, s_fLineHeight * 11, false);
		pxMinMoves->SetFontSize(s_fBaseTextSize * 3.0f);
		pxMinMoves->SetColor({0.6f, 0.8f, 1.f, 1.f});

		Zenith_UI::Zenith_UIText* pxWin = xUI.CreateText("WinText", "");
		SetupTopRightText(pxWin, s_fLineHeight * 13, false);
		pxWin->SetFontSize(s_fBaseTextSize * 4.2f);
		pxWin->SetColor({0.2f, 1.f, 0.2f, 1.f});

		Zenith_UI::Zenith_UIText* pxLoadingText = xUI.CreateText("LoadingText", "Generating puzzle...");
		pxLoadingText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxLoadingText->SetPosition(0.f, 0.f);
		pxLoadingText->SetFontSize(36.f);
		pxLoadingText->SetColor({1.f, 1.f, 1.f, 1.f});
		pxLoadingText->SetVisible(false);

		Zenith_Entity xDustEmitter(pxGameData, "DustEmitter");
		xDustEmitter.SetTransient(false);
		Zenith_ParticleEmitterComponent& xEmitter = xDustEmitter.AddComponent<Zenith_ParticleEmitterComponent>();
		xEmitter.SetConfig(Sokoban::g_pxDustConfig);

		Zenith_ScriptComponent& xScript = xGameManager.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviourForSerialization<Sokoban_Behaviour>();

		pxGameData->SaveToFile(strGamePath);
		Zenith_SceneManager::RegisterSceneBuildIndex(1, strGamePath);
		Zenith_SceneManager::UnloadScene(xGameScene);
	}
}

void Project_LoadInitialScene()
{
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
