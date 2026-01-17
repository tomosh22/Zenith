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

void Project_LoadInitialScene()
{
	Zenith_Scene::GetCurrentScene().Reset();
}