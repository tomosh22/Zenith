#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "AIShowcase/Components/AIShowcase_Behaviour.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux_Graphics.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_TacticalPoint.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "UI/Zenith_UIButton.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#endif

// ============================================================================
// AIShowcase Resources - Global access for behaviours
// ============================================================================
namespace AIShowcase
{
	// Geometry assets (registry-managed)
	Zenith_MeshGeometryAsset* g_pxCubeAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxSphereAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxCylinderAsset = nullptr;

	// Convenience pointers to underlying geometry
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;
	Flux_MeshGeometry* g_pxCylinderGeometry = nullptr;

	// Materials for arena
	MaterialHandle g_xFloorMaterial;
	MaterialHandle g_xWallMaterial;
	MaterialHandle g_xObstacleMaterial;

	// Materials for agents
	MaterialHandle g_xPlayerMaterial;
	MaterialHandle g_xEnemyMaterial;
	MaterialHandle g_xLeaderMaterial;
	MaterialHandle g_xFlankerMaterial;

	// Debug visualization materials
	MaterialHandle g_xCoverPointMaterial;
	MaterialHandle g_xPatrolPointMaterial;

	// NavMesh
	Zenith_NavMesh* g_pxArenaNavMesh = nullptr;
}

static bool s_bResourcesInitialized = false;

static void InitializeAIShowcaseResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace AIShowcase;

	// Create geometries using registry's cached primitives
	g_pxCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_pxCubeGeometry = g_pxCubeAsset->GetGeometry();

	g_pxSphereAsset = Zenith_MeshGeometryAsset::CreateUnitSphere(16);
	g_pxSphereGeometry = g_pxSphereAsset->GetGeometry();

	g_pxCylinderAsset = Zenith_MeshGeometryAsset::CreateUnitCylinder(16);
	g_pxCylinderGeometry = g_pxCylinderAsset->GetGeometry();

	// Use grid pattern texture with BaseColor for all materials
	Zenith_TextureAsset* pxGridTex = Flux_Graphics::s_pxGridTexture;

	// Create materials with grid texture and BaseColor
	auto& xRegistry = Zenith_AssetRegistry::Get();
	g_xFloorMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xFloorMaterial.Get()->SetName("AIShowcase_Floor");
	g_xFloorMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xFloorMaterial.Get()->SetBaseColor({ 64.f/255.f, 64.f/255.f, 64.f/255.f, 1.f });

	g_xWallMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xWallMaterial.Get()->SetName("AIShowcase_Wall");
	g_xWallMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xWallMaterial.Get()->SetBaseColor({ 128.f/255.f, 96.f/255.f, 64.f/255.f, 1.f });

	g_xObstacleMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xObstacleMaterial.Get()->SetName("AIShowcase_Obstacle");
	g_xObstacleMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xObstacleMaterial.Get()->SetBaseColor({ 96.f/255.f, 96.f/255.f, 96.f/255.f, 1.f });

	g_xPlayerMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xPlayerMaterial.Get()->SetName("AIShowcase_Player");
	g_xPlayerMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xPlayerMaterial.Get()->SetBaseColor({ 51.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

	g_xEnemyMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xEnemyMaterial.Get()->SetName("AIShowcase_Enemy");
	g_xEnemyMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xEnemyMaterial.Get()->SetBaseColor({ 230.f/255.f, 77.f/255.f, 77.f/255.f, 1.f });

	g_xLeaderMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xLeaderMaterial.Get()->SetName("AIShowcase_Leader");
	g_xLeaderMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xLeaderMaterial.Get()->SetBaseColor({ 255.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	g_xFlankerMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xFlankerMaterial.Get()->SetName("AIShowcase_Flanker");
	g_xFlankerMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xFlankerMaterial.Get()->SetBaseColor({ 255.f/255.f, 128.f/255.f, 0.f/255.f, 1.f });

	// Cover/patrol point materials
	g_xCoverPointMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xCoverPointMaterial.Get()->SetName("AIShowcase_CoverPoint");
	g_xCoverPointMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xCoverPointMaterial.Get()->SetBaseColor({ 51.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	g_xPatrolPointMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xPatrolPointMaterial.Get()->SetName("AIShowcase_PatrolPoint");
	g_xPatrolPointMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xPatrolPointMaterial.Get()->SetBaseColor({ 153.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

	s_bResourcesInitialized = true;
}
// ============================================================================

const char* Project_GetName()
{
	return "AIShowcase";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeAIShowcaseResources();

	// Initialize AI systems
	Zenith_PerceptionSystem::Initialise();
	Zenith_SquadManager::Initialise();
	Zenith_TacticalPointSystem::Initialise();

#ifdef ZENITH_TOOLS
	// Register AI debug variables
	Zenith_AIDebugVariables::Initialise();
#endif

	// Register behaviours
	AIShowcase_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Shutdown AI systems
	Zenith_TacticalPointSystem::Shutdown();
	Zenith_SquadManager::Shutdown();
	Zenith_PerceptionSystem::Shutdown();

	// Cleanup NavMesh
	delete AIShowcase::g_pxArenaNavMesh;
	AIShowcase::g_pxArenaNavMesh = nullptr;
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All resources initialized in Project_RegisterScriptBehaviours
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- MainMenu scene (build index 0) ----
	Zenith_EditorAutomation::AddStep_CreateScene("MainMenu");
	Zenith_EditorAutomation::AddStep_CreateEntity("MenuManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 30.f, -35.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.7f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(50.f));
	Zenith_EditorAutomation::AddStep_SetCameraFar(500.f);
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIText("MenuTitle", "AI SHOWCASE");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuTitle", 48.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuTitle", 0.2f, 0.6f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_CreateUIButton("MenuPlay", "Play");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("AIShowcase_Behaviour");
	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- AIShowcase gameplay scene (build index 1) ----
	Zenith_EditorAutomation::AddStep_CreateScene("AIShowcase");
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 30.f, -35.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.7f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(50.f));
	Zenith_EditorAutomation::AddStep_SetCameraFar(500.f);
	Zenith_EditorAutomation::AddStep_AddUI();

	// HUD UI: margin=20, textSize=14, lineHeight=22
	// Title: TopLeft, (20, 20), fontSize=42, white, hidden
	Zenith_EditorAutomation::AddStep_CreateUIText("Title", "AI SHOWCASE");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Title", 20.f, 20.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Title", 42.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("Title", false);

	// ControlsHeader: TopLeft, (20, 64), fontSize=33.6, yellow, hidden
	Zenith_EditorAutomation::AddStep_CreateUIText("ControlsHeader", "Controls:");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("ControlsHeader", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("ControlsHeader", 20.f, 64.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("ControlsHeader", 33.6f);
	Zenith_EditorAutomation::AddStep_SetUIColor("ControlsHeader", 0.9f, 0.9f, 0.2f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("ControlsHeader", false);

	// Control lines: TopLeft, (20, 86+22*i), fontSize=28, gray, hidden
	{
		static const char* s_aszControlNames[] = { "Control0", "Control1", "Control2", "Control3", "Control4" };
		static const char* s_aszControlTexts[] = {
			"WASD: Move player", "Space: Attack/Make sound",
			"1-5: Change formation", "R: Reset demo", "Esc: Menu"
		};
		for (uint32_t u = 0; u < 5; ++u)
		{
			float fY = 86.f + 22.f * static_cast<float>(u);
			Zenith_EditorAutomation::AddStep_CreateUIText(s_aszControlNames[u], s_aszControlTexts[u]);
			Zenith_EditorAutomation::AddStep_SetUIAnchor(s_aszControlNames[u], static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
			Zenith_EditorAutomation::AddStep_SetUIPosition(s_aszControlNames[u], 20.f, fY);
			Zenith_EditorAutomation::AddStep_SetUIFontSize(s_aszControlNames[u], 28.f);
			Zenith_EditorAutomation::AddStep_SetUIColor(s_aszControlNames[u], 0.8f, 0.8f, 0.8f, 1.f);
			Zenith_EditorAutomation::AddStep_SetUIVisible(s_aszControlNames[u], false);
		}
	}

	// Status: BottomLeft, (20, -20), fontSize=28, blue-ish, hidden
	Zenith_EditorAutomation::AddStep_CreateUIText("Status", "Enemies: 0 | Squads: 0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Status", 20.f, -20.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Status", 28.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Status", 0.6f, 0.8f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("Status", false);

	// Script
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("AIShowcase_Behaviour");
	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/AIShowcase" ZENITH_SCENE_EXT);
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
	Zenith_SceneManager::RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/AIShowcase" ZENITH_SCENE_EXT);
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
