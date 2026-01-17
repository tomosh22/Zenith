#include "Zenith.h"

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
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux_Graphics.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_TacticalPoint.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

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
	Zenith_MaterialAsset* g_pxFloorMaterial = nullptr;
	Zenith_MaterialAsset* g_pxWallMaterial = nullptr;
	Zenith_MaterialAsset* g_pxObstacleMaterial = nullptr;

	// Materials for agents
	Zenith_MaterialAsset* g_pxPlayerMaterial = nullptr;
	Zenith_MaterialAsset* g_pxEnemyMaterial = nullptr;
	Zenith_MaterialAsset* g_pxLeaderMaterial = nullptr;
	Zenith_MaterialAsset* g_pxFlankerMaterial = nullptr;

	// Debug visualization materials
	Zenith_MaterialAsset* g_pxCoverPointMaterial = nullptr;
	Zenith_MaterialAsset* g_pxPatrolPointMaterial = nullptr;

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
	g_pxFloorMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxFloorMaterial->SetName("AIShowcase_Floor");
	g_pxFloorMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxFloorMaterial->SetBaseColor({ 64.f/255.f, 64.f/255.f, 64.f/255.f, 1.f });

	g_pxWallMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxWallMaterial->SetName("AIShowcase_Wall");
	g_pxWallMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxWallMaterial->SetBaseColor({ 128.f/255.f, 96.f/255.f, 64.f/255.f, 1.f });

	g_pxObstacleMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxObstacleMaterial->SetName("AIShowcase_Obstacle");
	g_pxObstacleMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxObstacleMaterial->SetBaseColor({ 96.f/255.f, 96.f/255.f, 96.f/255.f, 1.f });

	g_pxPlayerMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxPlayerMaterial->SetName("AIShowcase_Player");
	g_pxPlayerMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxPlayerMaterial->SetBaseColor({ 51.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

	g_pxEnemyMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxEnemyMaterial->SetName("AIShowcase_Enemy");
	g_pxEnemyMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxEnemyMaterial->SetBaseColor({ 230.f/255.f, 77.f/255.f, 77.f/255.f, 1.f });

	g_pxLeaderMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxLeaderMaterial->SetName("AIShowcase_Leader");
	g_pxLeaderMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxLeaderMaterial->SetBaseColor({ 255.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	g_pxFlankerMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxFlankerMaterial->SetName("AIShowcase_Flanker");
	g_pxFlankerMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxFlankerMaterial->SetBaseColor({ 255.f/255.f, 128.f/255.f, 0.f/255.f, 1.f });

	// Cover/patrol point materials
	g_pxCoverPointMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxCoverPointMaterial->SetName("AIShowcase_CoverPoint");
	g_pxCoverPointMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxCoverPointMaterial->SetBaseColor({ 51.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	g_pxPatrolPointMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxPatrolPointMaterial->SetName("AIShowcase_PatrolPoint");
	g_pxPatrolPointMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxPatrolPointMaterial->SetBaseColor({ 153.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

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

void Project_LoadInitialScene()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	xScene.Reset();

	// Create camera
	Zenith_Entity xCameraEntity(&xScene, "MainCamera");
	xCameraEntity.SetTransient(false);
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	// Top-down isometric view for tactical overview
	// Camera positioned behind and above the arena (similar to Combat game setup)
	// Arena is 40x30 centered at origin, so camera at Z=-35 to see the full arena
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.f, 30.f, -35.f),  // Position: behind arena, elevated
		-0.7f,   // Pitch: looking down at arena (matches Combat)
		0.f,     // Yaw: facing forward (+Z direction)
		glm::radians(50.f),  // FOV to see full arena
		0.1f,
		500.f,
		16.f / 9.f
	);
	xScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Create main game entity with behaviour
	Zenith_Entity xGameEntity(&xScene, "AIShowcase");
	xGameEntity.SetTransient(false);

	// UI Setup
	static constexpr float s_fMargin = 20.f;
	static constexpr float s_fTextSize = 14.f;
	static constexpr float s_fLineHeight = 22.f;

	Zenith_UIComponent& xUI = xGameEntity.AddComponent<Zenith_UIComponent>();

	// Title
	Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "AI SHOWCASE");
	pxTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
	pxTitle->SetPosition(s_fMargin, s_fMargin);
	pxTitle->SetFontSize(s_fTextSize * 3.0f);
	pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	// Controls header
	Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("ControlsHeader", "Controls:");
	pxControls->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
	pxControls->SetPosition(s_fMargin, s_fMargin + s_fLineHeight * 2);
	pxControls->SetFontSize(s_fTextSize * 2.4f);
	pxControls->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	// Control instructions
	const char* astrControls[] = {
		"WASD: Move player",
		"Space: Attack/Make sound",
		"1-5: Change formation",
		"R: Reset demo"
	};
	for (uint32_t u = 0; u < 4; ++u)
	{
		char szName[32];
		sprintf(szName, "Control%u", u);
		Zenith_UI::Zenith_UIText* pxText = xUI.CreateText(szName, astrControls[u]);
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxText->SetPosition(s_fMargin, s_fMargin + s_fLineHeight * (3 + u));
		pxText->SetFontSize(s_fTextSize * 2.0f);
		pxText->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));
	}

	// Status text (bottom-left)
	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "Enemies: 0 | Squads: 0");
	pxStatus->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomLeft);
	pxStatus->SetPosition(s_fMargin, -s_fMargin);
	pxStatus->SetFontSize(s_fTextSize * 2.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	// Add script component with AIShowcase behaviour
	Zenith_ScriptComponent& xScript = xGameEntity.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<AIShowcase_Behaviour>();

	// Save scene file
	std::string strScenePath = std::string(GAME_ASSETS_DIR) + "/Scenes/AIShowcase.zscn";
	std::filesystem::create_directories(std::string(GAME_ASSETS_DIR) + "/Scenes");
	xScene.SaveToFile(strScenePath);
}
