#include "Zenith.h"

// Returns the project name - used by Tools code to construct asset paths
// The build system provides ZENITH_ROOT, and paths are constructed as:
// ZENITH_ROOT + "Games/" + Project_GetName() + "/Assets/"
const char* Project_GetName()
{
	return "Test";
}

// Returns the game assets directory - called by Zenith engine code
// GAME_ASSETS_DIR is defined by the build system for each game
const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

#include "Test/Components/SphereMovement_Behaviour.h"
#include "Test/Components/PlayerController_Behaviour.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "UI/Zenith_UI.h"
#include "Prefab/Zenith_Prefab.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include <filesystem>

void Project_RegisterScriptBehaviours()
{
	PlayerController_Behaviour::RegisterBehaviour();
	HookesLaw_Behaviour::RegisterBehaviour();
	RotationBehaviour_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Test game has no resources that need explicit cleanup
}

void Project_CreateScenes()
{
	// ---- MainMenu scene (build index 0) ----
	{
		const std::string strMenuPath = GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT;

		Zenith_Scene xMenuScene = Zenith_SceneManager::CreateEmptyScene("MainMenu");
		Zenith_SceneData* pxMenuData = Zenith_SceneManager::GetSceneData(xMenuScene);

		Zenith_Entity xMenuManager(pxMenuData, "MenuManager");
		xMenuManager.SetTransient(false);

		// Camera - default perspective at origin
		Zenith_CameraComponent& xCamera = xMenuManager.AddComponent<Zenith_CameraComponent>();
		xCamera.InitialisePerspective(
			Zenith_Maths::Vector3(0.f, 0.f, 0.f),
			0.f,
			0.f,
			glm::radians(45.f),
			0.1f,
			1000.f,
			16.f / 9.f
		);

		// Menu UI
		Zenith_UIComponent& xUI = xMenuManager.AddComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIText* pxMenuTitle = xUI.CreateText("MenuTitle", "TEST");
		pxMenuTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxMenuTitle->SetPosition(0.f, -120.f);
		pxMenuTitle->SetFontSize(72.f);
		pxMenuTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

		Zenith_UI::Zenith_UIButton* pxPlayButton = xUI.CreateButton("MenuPlay", "Play");
		pxPlayButton->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxPlayButton->SetPosition(0.f, 0.f);
		pxPlayButton->SetSize(200.f, 50.f);

		pxMenuData->SaveToFile(strMenuPath);
		Zenith_SceneManager::RegisterSceneBuildIndex(0, strMenuPath);
		Zenith_SceneManager::UnloadScene(xMenuScene);
	}

	// ---- Test gameplay scene (build index 1) ----
	{
		const std::string strGamePath = GAME_ASSETS_DIR "Scenes/Test" ZENITH_SCENE_EXT;

		Zenith_Scene xGameScene = Zenith_SceneManager::CreateEmptyScene("Test");
		Zenith_SceneData* pxGameData = Zenith_SceneManager::GetSceneData(xGameScene);

		Zenith_Entity xGameManager(pxGameData, "GameManager");
		xGameManager.SetTransient(false);

		// Camera - default perspective at origin
		Zenith_CameraComponent& xCamera = xGameManager.AddComponent<Zenith_CameraComponent>();
		xCamera.InitialisePerspective(
			Zenith_Maths::Vector3(0.f, 0.f, 0.f),
			0.f,
			0.f,
			glm::radians(45.f),
			0.1f,
			1000.f,
			16.f / 9.f
		);

		pxGameData->SaveToFile(strGamePath);
		Zenith_SceneManager::RegisterSceneBuildIndex(1, strGamePath);
		Zenith_SceneManager::UnloadScene(xGameScene);
	}
}

void Project_LoadInitialScene()
{
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}