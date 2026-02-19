#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "Marble/Components/Marble_Behaviour.h"
#include "Marble/Components/Marble_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Prefab/Zenith_Prefab.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "UI/Zenith_UIButton.h"

#include <cmath>

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#endif

// ============================================================================
// Marble Resources - Global access for behaviours
// ============================================================================
namespace Marble
{
	// Geometry assets (registry-managed)
	Zenith_MeshGeometryAsset* g_pxSphereAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxCubeAsset = nullptr;

	// Convenience pointers to underlying geometry
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	MaterialHandle g_xBallMaterial;
	MaterialHandle g_xPlatformMaterial;
	MaterialHandle g_xGoalMaterial;
	MaterialHandle g_xCollectibleMaterial;
	MaterialHandle g_xFloorMaterial;

	// Prefabs for runtime instantiation
	Zenith_Prefab* g_pxBallPrefab = nullptr;
	Zenith_Prefab* g_pxPlatformPrefab = nullptr;
	Zenith_Prefab* g_pxGoalPrefab = nullptr;
	Zenith_Prefab* g_pxCollectiblePrefab = nullptr;
}

static bool s_bResourcesInitialized = false;

// ============================================================================
// Procedural UV Sphere Generation
// ============================================================================
static void GenerateUVSphere(Flux_MeshGeometry& xGeometryOut, float fRadius, uint32_t uSlices, uint32_t uStacks)
{
	uint32_t uNumVerts = (uStacks + 1) * (uSlices + 1);
	uint32_t uNumIndices = uStacks * uSlices * 6;

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;
	xGeometryOut.m_pxPositions = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxNormals = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxUVs = new Zenith_Maths::Vector2[uNumVerts];
	xGeometryOut.m_pxTangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxBitangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxColors = new Zenith_Maths::Vector4[uNumVerts];
	xGeometryOut.m_puIndices = new Flux_MeshGeometry::IndexType[uNumIndices];

	uint32_t uVertIdx = 0;

	// Generate vertices
	for (uint32_t uStack = 0; uStack <= uStacks; uStack++)
	{
		float fPhi = static_cast<float>(uStack) / static_cast<float>(uStacks) * 3.14159265f;
		float fY = cos(fPhi) * fRadius;
		float fStackRadius = sin(fPhi) * fRadius;

		for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
		{
			float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
			float fX = cos(fTheta) * fStackRadius;
			float fZ = sin(fTheta) * fStackRadius;

			Zenith_Maths::Vector3 xPos(fX, fY, fZ);
			Zenith_Maths::Vector3 xNormal = glm::length(xPos) > 0.001f ? glm::normalize(xPos) : Zenith_Maths::Vector3(0.f, 1.f, 0.f);

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(
				static_cast<float>(uSlice) / static_cast<float>(uSlices),
				static_cast<float>(uStack) / static_cast<float>(uStacks)
			);

			// Simple tangent/bitangent calculation for sphere
			Zenith_Maths::Vector3 xTangent(-sin(fTheta), 0.f, cos(fTheta));
			xGeometryOut.m_pxTangents[uVertIdx] = xTangent;
			xGeometryOut.m_pxBitangents[uVertIdx] = glm::cross(xNormal, xTangent);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);

			uVertIdx++;
		}
	}

	// Generate indices
	uint32_t uIdxIdx = 0;
	for (uint32_t uStack = 0; uStack < uStacks; uStack++)
	{
		for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
		{
			uint32_t uCurrent = uStack * (uSlices + 1) + uSlice;
			uint32_t uNext = uCurrent + uSlices + 1;

			// Counter-clockwise winding for Vulkan
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
		}
	}

	// Generate buffer layout and vertex data
	xGeometryOut.GenerateLayoutAndVertexData();

	// Upload to GPU
	Flux_MemoryManager::InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

// ============================================================================
// Resource Initialization
// ============================================================================
static void InitializeMarbleResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace Marble;

	// Create sphere geometry (custom radius - tracked through registry)
	g_pxSphereAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>();
	Flux_MeshGeometry* pxSphereGeom = new Flux_MeshGeometry();
	GenerateUVSphere(*pxSphereGeom, 0.5f, 16, 12);
	g_pxSphereAsset->SetGeometry(pxSphereGeom);
	g_pxSphereGeometry = g_pxSphereAsset->GetGeometry();

	// Create cube geometry (uses cached unit cube)
	g_pxCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_pxCubeGeometry = g_pxCubeAsset->GetGeometry();

	// Use grid pattern texture with BaseColor for all materials
	Zenith_TextureAsset* pxGridTex = Flux_Graphics::s_pxGridTexture;

	// Create materials with grid texture and BaseColor
	auto& xRegistry = Zenith_AssetRegistry::Get();
	g_xBallMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xBallMaterial.Get()->SetName("MarbleBall");
	g_xBallMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xBallMaterial.Get()->SetBaseColor({ 51.f/255.f, 102.f/255.f, 230.f/255.f, 1.f });

	g_xPlatformMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xPlatformMaterial.Get()->SetName("MarblePlatform");
	g_xPlatformMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xPlatformMaterial.Get()->SetBaseColor({ 102.f/255.f, 102.f/255.f, 102.f/255.f, 1.f });

	g_xGoalMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xGoalMaterial.Get()->SetName("MarbleGoal");
	g_xGoalMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xGoalMaterial.Get()->SetBaseColor({ 51.f/255.f, 204.f/255.f, 51.f/255.f, 1.f });

	g_xCollectibleMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xCollectibleMaterial.Get()->SetName("MarbleCollectible");
	g_xCollectibleMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xCollectibleMaterial.Get()->SetBaseColor({ 255.f/255.f, 215.f/255.f, 0.f/255.f, 1.f });

	g_xFloorMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xFloorMaterial.Get()->SetName("MarbleFloor");
	g_xFloorMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xFloorMaterial.Get()->SetBaseColor({ 77.f/255.f, 77.f/255.f, 89.f/255.f, 1.f });

	// Create prefabs for runtime instantiation
	// Note: Prefabs are lightweight templates with only TransformComponent
	// ModelComponent and ColliderComponent are added AFTER setting position/scale
	// (ColliderComponent creates physics bodies - must be added after transform is set)
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Ball prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xBallTemplate(pxSceneData, "BallTemplate");

		g_pxBallPrefab = new Zenith_Prefab();
		g_pxBallPrefab->CreateFromEntity(xBallTemplate, "Ball");

		Zenith_SceneManager::Destroy(xBallTemplate);
	}

	// Platform prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xPlatformTemplate(pxSceneData, "PlatformTemplate");

		g_pxPlatformPrefab = new Zenith_Prefab();
		g_pxPlatformPrefab->CreateFromEntity(xPlatformTemplate, "Platform");

		Zenith_SceneManager::Destroy(xPlatformTemplate);
	}

	// Goal prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xGoalTemplate(pxSceneData, "GoalTemplate");

		g_pxGoalPrefab = new Zenith_Prefab();
		g_pxGoalPrefab->CreateFromEntity(xGoalTemplate, "Goal");

		Zenith_SceneManager::Destroy(xGoalTemplate);
	}

	// Collectible prefab - basic entity (ModelComponent added at runtime, no collider - uses distance check)
	{
		Zenith_Entity xCollectibleTemplate(pxSceneData, "CollectibleTemplate");

		g_pxCollectiblePrefab = new Zenith_Prefab();
		g_pxCollectiblePrefab->CreateFromEntity(xCollectibleTemplate, "Collectible");

		Zenith_SceneManager::Destroy(xCollectibleTemplate);
	}

	s_bResourcesInitialized = true;
}

// ============================================================================
// Project Entry Points
// ============================================================================
const char* Project_GetName()
{
	return "Marble";
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
	InitializeMarbleResources();

	Marble_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Marble has no resources that need explicit cleanup
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All Marble resources initialized in Project_RegisterScriptBehaviours
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- MainMenu scene (build index 0) ----
	Zenith_EditorAutomation::AddStep_CreateScene("MainMenu");
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 8.f, -12.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.4f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(50.f));
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIText("MenuTitle", "MARBLE ROLL");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("MenuTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuTitle", 90.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuTitle", 0.4f, 0.6f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_CreateUIButton("MenuPlay", "Play");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	Zenith_EditorAutomation::AddStep_CreateUIButton("MenuQuit", "Quit");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuQuit", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuQuit", 0.f, 70.f);
	Zenith_EditorAutomation::AddStep_SetUISize("MenuQuit", 200.f, 50.f);
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("Marble_Behaviour");
	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- Marble gameplay scene (build index 1) ----
	Zenith_EditorAutomation::AddStep_CreateScene("Marble");
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 8.f, -12.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.4f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(50.f));
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AddUI();

	// UI layout constants (matching original): marginLeft=30, marginTop=30, baseTextSize=15, lineHeight=24
	// Title: y=30+0=30, fontSize=15*4.8=72
	Zenith_EditorAutomation::AddStep_CreateUIText("Title", "MARBLE ROLL");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Title", 30.f, 30.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Title", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Title", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Title", 72.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);

	// Score: y=30+72=102, fontSize=15*3.0=45
	Zenith_EditorAutomation::AddStep_CreateUIText("Score", "Score: 0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Score", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Score", 30.f, 102.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Score", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Score", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Score", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Score", 0.6f, 0.8f, 1.f, 1.f);

	// Time: y=30+96=126, fontSize=15*3.0=45
	Zenith_EditorAutomation::AddStep_CreateUIText("Time", "Time: 60.0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Time", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Time", 30.f, 126.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Time", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Time", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Time", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Time", 0.6f, 0.8f, 1.f, 1.f);

	// Collected: y=30+120=150, fontSize=15*3.0=45
	Zenith_EditorAutomation::AddStep_CreateUIText("Collected", "Collected: 0 / 5");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Collected", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Collected", 30.f, 150.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Collected", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Collected", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Collected", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Collected", 0.6f, 0.8f, 1.f, 1.f);

	// Controls: y=30+168=198, fontSize=15*2.5=37.5
	Zenith_EditorAutomation::AddStep_CreateUIText("Controls", "WASD: Move | Space: Jump | R: Reset | Esc: Menu");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Controls", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Controls", 30.f, 198.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Controls", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Controls", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Controls", 37.5f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Controls", 0.7f, 0.7f, 0.7f, 1.f);

	// Status: Center anchor, (0,0), Center alignment, fontSize=15*6=90
	Zenith_EditorAutomation::AddStep_CreateUIText("Status", "");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Status", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Status", false);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Status", 90.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Status", 0.2f, 1.f, 0.2f, 1.f);

	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("Marble_Behaviour");

	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Marble" ZENITH_SCENE_EXT);
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
	Zenith_SceneManager::RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Marble" ZENITH_SCENE_EXT);
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
