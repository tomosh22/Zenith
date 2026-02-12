#include "Zenith.h"

#include "TilePuzzle/Components/TilePuzzle_Types.h"
#include "TilePuzzle/Components/TilePuzzle_Behaviour.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux_Graphics.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UIButton.h"

// ============================================================================
// TilePuzzle Resources - Global access for behaviours
// ============================================================================
namespace TilePuzzle
{
	// Shared geometry assets (registry-managed)
	Zenith_MeshGeometryAsset* g_pxCubeAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxSphereAsset = nullptr;

	// Convenience pointers to underlying geometry (do not delete - managed by assets)
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;

	// Floor material
	MaterialHandle g_xFloorMaterial;

	// Blocker material (static shapes)
	MaterialHandle g_xBlockerMaterial;

	// Colored shape materials (draggable)
	MaterialHandle g_axShapeMaterials[TILEPUZZLE_COLOR_COUNT];

	// Colored cat materials
	MaterialHandle g_axCatMaterials[TILEPUZZLE_COLOR_COUNT];

	// Prefabs for runtime instantiation
	Zenith_Prefab* g_pxCellPrefab = nullptr;
	Zenith_Prefab* g_pxShapeCubePrefab = nullptr;
	Zenith_Prefab* g_pxCatPrefab = nullptr;
}

static bool s_bResourcesInitialized = false;

static void InitializeTilePuzzleResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace TilePuzzle;

	// Create geometry using registry's cached primitives
	g_pxCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_pxCubeGeometry = g_pxCubeAsset->GetGeometry();

	g_pxSphereAsset = Zenith_MeshGeometryAsset::CreateUnitSphere(16);
	g_pxSphereGeometry = g_pxSphereAsset->GetGeometry();

	// Use grid pattern texture with BaseColor for all materials
	Zenith_TextureAsset* pxGridTex = Flux_Graphics::s_pxGridTexture;

	// Create materials with grid texture and BaseColor
	auto& xRegistry = Zenith_AssetRegistry::Get();
	g_xFloorMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xFloorMaterial.Get()->SetName("TilePuzzleFloor");
	g_xFloorMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xFloorMaterial.Get()->SetBaseColor({ 77.f/255.f, 77.f/255.f, 89.f/255.f, 1.f });

	g_xBlockerMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xBlockerMaterial.Get()->SetName("TilePuzzleBlocker");
	g_xBlockerMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xBlockerMaterial.Get()->SetBaseColor({ 80.f/255.f, 50.f/255.f, 30.f/255.f, 1.f });

	// Shape materials with distinct colors
	const char* aszShapeColorNames[] = { "Red", "Green", "Blue", "Yellow" };
	const Zenith_Maths::Vector4 axShapeColors[] = {
		{ 230.f/255.f, 60.f/255.f, 60.f/255.f, 1.f },    // Red
		{ 60.f/255.f, 200.f/255.f, 60.f/255.f, 1.f },    // Green
		{ 60.f/255.f, 100.f/255.f, 230.f/255.f, 1.f },   // Blue
		{ 230.f/255.f, 230.f/255.f, 60.f/255.f, 1.f }    // Yellow
	};
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleShape%s", aszShapeColorNames[i]);
		g_axShapeMaterials[i].Set(xRegistry.Create<Zenith_MaterialAsset>());
		g_axShapeMaterials[i].Get()->SetName(szName);
		g_axShapeMaterials[i].Get()->SetDiffuseTextureDirectly(pxGridTex);
		g_axShapeMaterials[i].Get()->SetBaseColor(axShapeColors[i]);
	}

	// Cat materials (same colors as shapes)
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleCat%s", aszShapeColorNames[i]);
		g_axCatMaterials[i].Set(xRegistry.Create<Zenith_MaterialAsset>());
		g_axCatMaterials[i].Get()->SetName(szName);
		g_axCatMaterials[i].Get()->SetDiffuseTextureDirectly(pxGridTex);
		g_axCatMaterials[i].Get()->SetBaseColor(axShapeColors[i]);
	}

	// Create prefabs for runtime instantiation
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Cell prefab (floor tiles)
	{
		Zenith_Entity xCellTemplate(pxSceneData, "CellTemplate");
		g_pxCellPrefab = new Zenith_Prefab();
		g_pxCellPrefab->CreateFromEntity(xCellTemplate, "Cell");
		Zenith_SceneManager::Destroy(xCellTemplate);
	}

	// Shape cube prefab (for multi-cube shapes)
	{
		Zenith_Entity xShapeCubeTemplate(pxSceneData, "ShapeCubeTemplate");
		g_pxShapeCubePrefab = new Zenith_Prefab();
		g_pxShapeCubePrefab->CreateFromEntity(xShapeCubeTemplate, "ShapeCube");
		Zenith_SceneManager::Destroy(xShapeCubeTemplate);
	}

	// Cat prefab (spheres)
	{
		Zenith_Entity xCatTemplate(pxSceneData, "CatTemplate");
		g_pxCatPrefab = new Zenith_Prefab();
		g_pxCatPrefab->CreateFromEntity(xCatTemplate, "Cat");
		Zenith_SceneManager::Destroy(xCatTemplate);
	}

	s_bResourcesInitialized = true;
}

// ============================================================================
// Required Entry Point Functions
// ============================================================================

const char* Project_GetName()
{
	return "TilePuzzle";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_RegisterScriptBehaviours()
{
	InitializeTilePuzzleResources();
	TilePuzzle_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// TilePuzzle has no resources that need explicit cleanup
}

void Project_CreateScenes()
{
	// ---- MainMenu scene (build index 0) ----
	{
		const std::string strMenuPath = GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT;

		Zenith_Scene xMenuScene = Zenith_SceneManager::CreateEmptyScene("MainMenu");
		Zenith_SceneData* pxMenuData = Zenith_SceneManager::GetSceneData(xMenuScene);

		Zenith_Entity xMenuManager(pxMenuData, "GameManager");
		xMenuManager.SetTransient(false);

		Zenith_CameraComponent& xCamera = xMenuManager.AddComponent<Zenith_CameraComponent>();
		xCamera.InitialisePerspective({
			.m_xPosition = Zenith_Maths::Vector3(0.f, 12.f, 0.f),
			.m_fPitch = -1.5f,
			.m_fFOV = glm::radians(45.f),
		});
		pxMenuData->SetMainCameraEntity(xMenuManager.GetEntityID());

		Zenith_UIComponent& xUI = xMenuManager.AddComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIText* pxMenuTitle = xUI.CreateText("MenuTitle", "TILE PUZZLE");
		pxMenuTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxMenuTitle->SetPosition(0.f, -120.f);
		pxMenuTitle->SetFontSize(72.f);
		pxMenuTitle->SetColor({1.f, 1.f, 1.f, 1.f});

		Zenith_UI::Zenith_UIButton* pxPlayBtn = xUI.CreateButton("MenuPlay", "Play");
		pxPlayBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxPlayBtn->SetPosition(0.f, 0.f);
		pxPlayBtn->SetSize(200.f, 50.f);

		Zenith_ScriptComponent& xScript = xMenuManager.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviourForSerialization<TilePuzzle_Behaviour>();

		pxMenuData->SaveToFile(strMenuPath);
		Zenith_SceneManager::RegisterSceneBuildIndex(0, strMenuPath);
		Zenith_SceneManager::UnloadScene(xMenuScene);
	}

	// ---- TilePuzzle gameplay scene (build index 1) ----
	{
		const std::string strGamePath = GAME_ASSETS_DIR "Scenes/TilePuzzle" ZENITH_SCENE_EXT;

		Zenith_Scene xGameScene = Zenith_SceneManager::CreateEmptyScene("TilePuzzle");
		Zenith_SceneData* pxGameData = Zenith_SceneManager::GetSceneData(xGameScene);

		Zenith_Entity xGameManager(pxGameData, "GameManager");
		xGameManager.SetTransient(false);

		Zenith_CameraComponent& xCamera = xGameManager.AddComponent<Zenith_CameraComponent>();
		xCamera.InitialisePerspective({
			.m_xPosition = Zenith_Maths::Vector3(0.f, 12.f, 0.f),
			.m_fPitch = -1.5f,
			.m_fFOV = glm::radians(45.f),
		});
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

		Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "TILE PUZZLE");
		SetupTopRightText(pxTitle, 0.f, false);
		pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
		pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

		Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("ControlsHeader", "How to Play:");
		SetupTopRightText(pxControls, s_fLineHeight * 2, false);
		pxControls->SetFontSize(s_fBaseTextSize * 3.6f);
		pxControls->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

		Zenith_UI::Zenith_UIText* pxMove = xUI.CreateText("MoveInstr", "Click+Drag or Arrows: Move");
		SetupTopRightText(pxMove, s_fLineHeight * 3, false);
		pxMove->SetFontSize(s_fBaseTextSize * 3.0f);
		pxMove->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

		Zenith_UI::Zenith_UIText* pxReset = xUI.CreateText("ResetInstr", "R: Reset  Esc: Menu");
		SetupTopRightText(pxReset, s_fLineHeight * 4, false);
		pxReset->SetFontSize(s_fBaseTextSize * 3.0f);
		pxReset->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

		Zenith_UI::Zenith_UIText* pxGoal = xUI.CreateText("GoalHeader", "Goal:");
		SetupTopRightText(pxGoal, s_fLineHeight * 6, false);
		pxGoal->SetFontSize(s_fBaseTextSize * 3.6f);
		pxGoal->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

		Zenith_UI::Zenith_UIText* pxGoalDesc = xUI.CreateText("GoalDesc", "Match shapes to cats");
		SetupTopRightText(pxGoalDesc, s_fLineHeight * 7, false);
		pxGoalDesc->SetFontSize(s_fBaseTextSize * 3.0f);
		pxGoalDesc->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

		Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "Level: 1  Moves: 0");
		SetupTopRightText(pxStatus, s_fLineHeight * 9, false);
		pxStatus->SetFontSize(s_fBaseTextSize * 3.0f);
		pxStatus->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

		Zenith_UI::Zenith_UIText* pxProgress = xUI.CreateText("Progress", "Cats: 0 / 3");
		SetupTopRightText(pxProgress, s_fLineHeight * 10, false);
		pxProgress->SetFontSize(s_fBaseTextSize * 3.0f);
		pxProgress->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

		Zenith_UI::Zenith_UIText* pxWin = xUI.CreateText("WinText", "");
		SetupTopRightText(pxWin, s_fLineHeight * 12, false);
		pxWin->SetFontSize(s_fBaseTextSize * 4.2f);
		pxWin->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));

		Zenith_ScriptComponent& xScript = xGameManager.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviourForSerialization<TilePuzzle_Behaviour>();

		pxGameData->SaveToFile(strGamePath);
		Zenith_SceneManager::RegisterSceneBuildIndex(1, strGamePath);
		Zenith_SceneManager::UnloadScene(xGameScene);
	}
}

void Project_LoadInitialScene()
{
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
