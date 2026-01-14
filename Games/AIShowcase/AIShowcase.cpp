#include "Zenith.h"

#include "AIShowcase/Components/AIShowcase_Behaviour.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandler.h"
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
	// Geometry
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;
	Flux_MeshGeometry* g_pxCylinderGeometry = nullptr;

	// Materials for arena
	Flux_MaterialAsset* g_pxFloorMaterial = nullptr;
	Flux_MaterialAsset* g_pxWallMaterial = nullptr;
	Flux_MaterialAsset* g_pxObstacleMaterial = nullptr;

	// Materials for agents
	Flux_MaterialAsset* g_pxPlayerMaterial = nullptr;
	Flux_MaterialAsset* g_pxEnemyMaterial = nullptr;
	Flux_MaterialAsset* g_pxLeaderMaterial = nullptr;
	Flux_MaterialAsset* g_pxFlankerMaterial = nullptr;

	// Debug visualization materials
	Flux_MaterialAsset* g_pxCoverPointMaterial = nullptr;
	Flux_MaterialAsset* g_pxPatrolPointMaterial = nullptr;

	// NavMesh
	Zenith_NavMesh* g_pxArenaNavMesh = nullptr;
}

static Flux_Texture* s_pxFloorTexture = nullptr;
static Flux_Texture* s_pxWallTexture = nullptr;
static Flux_Texture* s_pxObstacleTexture = nullptr;
static Flux_Texture* s_pxPlayerTexture = nullptr;
static Flux_Texture* s_pxEnemyTexture = nullptr;
static Flux_Texture* s_pxLeaderTexture = nullptr;
static Flux_Texture* s_pxFlankerTexture = nullptr;
static bool s_bResourcesInitialized = false;

static Flux_Texture* CreateColoredTexture(uint8_t uR, uint8_t uG, uint8_t uB)
{
	Flux_SurfaceInfo xTexInfo;
	xTexInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xTexInfo.m_uWidth = 1;
	xTexInfo.m_uHeight = 1;
	xTexInfo.m_uDepth = 1;
	xTexInfo.m_uNumMips = 1;
	xTexInfo.m_uNumLayers = 1;
	xTexInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	uint8_t aucPixelData[] = { uR, uG, uB, 255 };

	Zenith_AssetHandler::TextureData xTexData;
	xTexData.pData = aucPixelData;
	xTexData.xSurfaceInfo = xTexInfo;
	xTexData.bCreateMips = false;
	xTexData.bIsCubemap = false;

	return Zenith_AssetHandler::AddTexture(xTexData);
}

static void InitializeAIShowcaseResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace AIShowcase;

	// Create geometries
	// Note: Only GenerateUnitCube is available - using it for all shapes
	g_pxCubeGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*g_pxCubeGeometry);

	// Use cube geometry for sphere/cylinder (until proper generation functions added)
	g_pxSphereGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*g_pxSphereGeometry);

	g_pxCylinderGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*g_pxCylinderGeometry);

	// Create textures
	s_pxFloorTexture = CreateColoredTexture(64, 64, 64);       // Dark gray floor
	s_pxWallTexture = CreateColoredTexture(128, 96, 64);       // Brown walls
	s_pxObstacleTexture = CreateColoredTexture(96, 96, 96);    // Gray obstacles
	s_pxPlayerTexture = CreateColoredTexture(51, 153, 255);    // Blue player
	s_pxEnemyTexture = CreateColoredTexture(230, 77, 77);      // Red enemies
	s_pxLeaderTexture = CreateColoredTexture(255, 204, 51);    // Gold leaders
	s_pxFlankerTexture = CreateColoredTexture(255, 128, 0);    // Orange flankers

	// Create materials
	g_pxFloorMaterial = Flux_MaterialAsset::Create("AIShowcase_Floor");
	g_pxFloorMaterial->SetDiffuseTexture(s_pxFloorTexture);

	g_pxWallMaterial = Flux_MaterialAsset::Create("AIShowcase_Wall");
	g_pxWallMaterial->SetDiffuseTexture(s_pxWallTexture);

	g_pxObstacleMaterial = Flux_MaterialAsset::Create("AIShowcase_Obstacle");
	g_pxObstacleMaterial->SetDiffuseTexture(s_pxObstacleTexture);

	g_pxPlayerMaterial = Flux_MaterialAsset::Create("AIShowcase_Player");
	g_pxPlayerMaterial->SetDiffuseTexture(s_pxPlayerTexture);

	g_pxEnemyMaterial = Flux_MaterialAsset::Create("AIShowcase_Enemy");
	g_pxEnemyMaterial->SetDiffuseTexture(s_pxEnemyTexture);

	g_pxLeaderMaterial = Flux_MaterialAsset::Create("AIShowcase_Leader");
	g_pxLeaderMaterial->SetDiffuseTexture(s_pxLeaderTexture);

	g_pxFlankerMaterial = Flux_MaterialAsset::Create("AIShowcase_Flanker");
	g_pxFlankerMaterial->SetDiffuseTexture(s_pxFlankerTexture);

	// Cover/patrol point materials (using existing textures)
	g_pxCoverPointMaterial = Flux_MaterialAsset::Create("AIShowcase_CoverPoint");
	g_pxCoverPointMaterial->SetDiffuseTexture(CreateColoredTexture(51, 204, 51));  // Green

	g_pxPatrolPointMaterial = Flux_MaterialAsset::Create("AIShowcase_PatrolPoint");
	g_pxPatrolPointMaterial->SetDiffuseTexture(CreateColoredTexture(153, 153, 255));  // Light blue

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
