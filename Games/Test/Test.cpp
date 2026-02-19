#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"

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

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#endif

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

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

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// Test game has no resources that need initialization
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- MainMenu scene (build index 0) ----
	Zenith_EditorAutomation::AddStep_CreateScene("MainMenu");
	Zenith_EditorAutomation::AddStep_CreateEntity("MenuManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(45.f));
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIText("MenuTitle", "TEST");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuTitle", 72.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuTitle", 1.f, 1.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_CreateUIButton("MenuPlay", "Play");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- Test gameplay scene (build index 1) ----
	Zenith_EditorAutomation::AddStep_CreateScene("Test");
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(45.f));
	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Test" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- Final scene loading ----
	Zenith_EditorAutomation::AddStep_SetInitialSceneLoadCallback(&Project_LoadInitialScene);
	Zenith_EditorAutomation::AddStep_SetLoadingScene(true);
	Zenith_EditorAutomation::AddStep_Custom(&Project_LoadInitialScene);
	Zenith_EditorAutomation::AddStep_SetLoadingScene(false);
}
#endif

void Project_LoadInitialScene()
{
	Zenith_SceneManager::RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Test" ZENITH_SCENE_EXT);
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
