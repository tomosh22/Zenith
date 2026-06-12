#include "Zenith.h"
#include "Core/Zenith_Engine.h"

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

#include "Test/Components/Test_PlayerControllerComponent.h"
#include "Test/Components/Test_GraphNodes.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
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
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#endif

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterGameComponents()
{
	// Register the game components with the component-meta registry
	// (orders 100+ are unique per game, after the engine built-ins).
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
	xRegistry.RegisterComponent<Test_PlayerControllerComponent>("TestPlayerController", 100);

	// Behaviour Graph node library (used by the boot-authored physics-toy graphs).
	Test_RegisterGraphNodes();

#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry& xEditorRegistry = Zenith_ComponentEditorRegistry::Get();
	xEditorRegistry.RegisterComponent<Test_PlayerControllerComponent>("TestPlayerController");
#endif
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
	g_xEngine.EditorAutomation().AddStep_CreateScene("MainMenu");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("MenuManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(45.f));
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuTitle", "TEST");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuTitle", 72.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuTitle", 1.f, 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuPlay", "Play");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Behaviour graphs (regenerated every boot through the graph editor's
	// ---- atomic actions; the physics-toy entities below run them) ----
	g_xEngine.EditorAutomation().AddStep_GraphOpenFresh("game:Graphs/Test_Spinner.bgraph");
	g_xEngine.EditorAutomation().AddStep_GraphAddNode("OnUpdate");
	g_xEngine.EditorAutomation().AddStep_GraphAddNode("TestSpinPlatform");
	g_xEngine.EditorAutomation().AddStep_GraphSelectNode("TestSpinPlatform", 0);
	g_xEngine.EditorAutomation().AddStep_GraphSetNodeParamVec3("m_xAngularVel", 0.f, 2.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_GraphConnect("OnUpdate", 0, 0, "TestSpinPlatform", 0);
	g_xEngine.EditorAutomation().AddStep_GraphSave();
	g_xEngine.EditorAutomation().AddStep_GraphClose();

	g_xEngine.EditorAutomation().AddStep_GraphOpenFresh("game:Graphs/Test_Spring.bgraph");
	g_xEngine.EditorAutomation().AddStep_GraphAddNode("OnUpdate");
	g_xEngine.EditorAutomation().AddStep_GraphAddNode("TestHookesForce");
	g_xEngine.EditorAutomation().AddStep_GraphSelectNode("TestHookesForce", 0);
	g_xEngine.EditorAutomation().AddStep_GraphSetNodeParamVec3("m_xDesiredPosition", 0.f, 5.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_GraphConnect("OnUpdate", 0, 0, "TestHookesForce", 0);
	g_xEngine.EditorAutomation().AddStep_GraphSave();
	g_xEngine.EditorAutomation().AddStep_GraphClose();

	// ---- Test gameplay scene (build index 1) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("Test");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(45.f));

	// Spinning platform driven by the Test_Spinner graph (constant angular
	// velocity through the physics body - the retired TestRotation logic).
	g_xEngine.EditorAutomation().AddStep_CreateEntity("Spinner");
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(0.f, 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_AddCollider();
	g_xEngine.EditorAutomation().AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_DYNAMIC);
	g_xEngine.EditorAutomation().AddStep_AddComponent("Graph");
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Test_Spinner.bgraph");

	// Spring-tethered body driven by the Test_Spring graph (Hooke's-law force
	// toward a target - the retired TestHookesLaw logic).
	g_xEngine.EditorAutomation().AddStep_CreateEntity("Spring");
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(3.f, 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_AddCollider();
	g_xEngine.EditorAutomation().AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_DYNAMIC);
	g_xEngine.EditorAutomation().AddStep_AddComponent("Graph");
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Test_Spring.bgraph");

	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Test" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Final scene loading ----
	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Test" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
