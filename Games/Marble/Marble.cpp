#include "Zenith.h"

#include "Marble/Components/Marble_Behaviour.h"
#include "Marble/Components/Marble_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Flux/Flux.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "AssetHandling/Zenith_DataAssetManager.h"
#include "Prefab/Zenith_Prefab.h"

#include <cmath>

// ============================================================================
// Marble Resources - Global access for behaviours
// ============================================================================
namespace Marble
{
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MaterialAsset* g_pxBallMaterial = nullptr;
	Flux_MaterialAsset* g_pxPlatformMaterial = nullptr;
	Flux_MaterialAsset* g_pxGoalMaterial = nullptr;
	Flux_MaterialAsset* g_pxCollectibleMaterial = nullptr;
	Flux_MaterialAsset* g_pxFloorMaterial = nullptr;

	// Prefabs for runtime instantiation
	Zenith_Prefab* g_pxBallPrefab = nullptr;
	Zenith_Prefab* g_pxPlatformPrefab = nullptr;
	Zenith_Prefab* g_pxGoalPrefab = nullptr;
	Zenith_Prefab* g_pxCollectiblePrefab = nullptr;
}

static Flux_Texture* s_pxBallTexture = nullptr;
static Flux_Texture* s_pxPlatformTexture = nullptr;
static Flux_Texture* s_pxGoalTexture = nullptr;
static Flux_Texture* s_pxCollectibleTexture = nullptr;
static Flux_Texture* s_pxFloorTexture = nullptr;
static bool s_bResourcesInitialized = false;

// ============================================================================
// Procedural Texture Generation (same pattern as Sokoban)
// ============================================================================
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

	// Create sphere geometry
	g_pxSphereGeometry = new Flux_MeshGeometry();
	GenerateUVSphere(*g_pxSphereGeometry, 0.5f, 16, 12);

	// Create cube geometry
	g_pxCubeGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*g_pxCubeGeometry);

	// Create textures (procedural single-color pixels)
	s_pxBallTexture = CreateColoredTexture(51, 102, 230);       // Blue ball
	s_pxPlatformTexture = CreateColoredTexture(102, 102, 102);  // Gray platforms
	s_pxGoalTexture = CreateColoredTexture(51, 204, 51);        // Green goal
	s_pxCollectibleTexture = CreateColoredTexture(255, 215, 0); // Gold collectibles
	s_pxFloorTexture = CreateColoredTexture(77, 77, 89);        // Dark gray floor

	// Create materials
	g_pxBallMaterial = Flux_MaterialAsset::Create("MarbleBall");
	g_pxBallMaterial->SetDiffuseTexture(s_pxBallTexture);

	g_pxPlatformMaterial = Flux_MaterialAsset::Create("MarblePlatform");
	g_pxPlatformMaterial->SetDiffuseTexture(s_pxPlatformTexture);

	g_pxGoalMaterial = Flux_MaterialAsset::Create("MarbleGoal");
	g_pxGoalMaterial->SetDiffuseTexture(s_pxGoalTexture);

	g_pxCollectibleMaterial = Flux_MaterialAsset::Create("MarbleCollectible");
	g_pxCollectibleMaterial->SetDiffuseTexture(s_pxCollectibleTexture);

	g_pxFloorMaterial = Flux_MaterialAsset::Create("MarbleFloor");
	g_pxFloorMaterial->SetDiffuseTexture(s_pxFloorTexture);

	// Create prefabs for runtime instantiation
	// Note: Prefabs are lightweight templates with only TransformComponent
	// ModelComponent and ColliderComponent are added AFTER setting position/scale
	// (ColliderComponent creates physics bodies - must be added after transform is set)
	Zenith_Scene::BeginPrefabCreation();
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Ball prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xBallTemplate(&xScene, "BallTemplate");

		g_pxBallPrefab = new Zenith_Prefab();
		g_pxBallPrefab->CreateFromEntity(xBallTemplate, "Ball");

		Zenith_Scene::Destroy(xBallTemplate);
	}

	// Platform prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xPlatformTemplate(&xScene, "PlatformTemplate");

		g_pxPlatformPrefab = new Zenith_Prefab();
		g_pxPlatformPrefab->CreateFromEntity(xPlatformTemplate, "Platform");

		Zenith_Scene::Destroy(xPlatformTemplate);
	}

	// Goal prefab - basic entity (ModelComponent and ColliderComponent added at runtime)
	{
		Zenith_Entity xGoalTemplate(&xScene, "GoalTemplate");

		g_pxGoalPrefab = new Zenith_Prefab();
		g_pxGoalPrefab->CreateFromEntity(xGoalTemplate, "Goal");

		Zenith_Scene::Destroy(xGoalTemplate);
	}

	// Collectible prefab - basic entity (ModelComponent added at runtime, no collider - uses distance check)
	{
		Zenith_Entity xCollectibleTemplate(&xScene, "CollectibleTemplate");

		g_pxCollectiblePrefab = new Zenith_Prefab();
		g_pxCollectiblePrefab->CreateFromEntity(xCollectibleTemplate, "Collectible");

		Zenith_Scene::Destroy(xCollectibleTemplate);
	}

	Zenith_Scene::EndPrefabCreation();

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

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeMarbleResources();

	// Register DataAsset types
	RegisterMarbleDataAssets();

	Marble_Behaviour::RegisterBehaviour();
}

void Project_LoadInitialScene()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	xScene.Reset();

	// Enter prefab creation mode for initial scene setup
	Zenith_Scene::BeginPrefabCreation();

	// Create camera entity
	Zenith_Entity xCameraEntity(&xScene, "MainCamera");
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.f, 8.f, -12.f),  // Position: behind and above
		-0.4f,  // Pitch: looking slightly down (negative pitch = look down)
		0.f,    // Yaw: facing forward
		glm::radians(50.f),   // FOV
		0.1f,
		1000.f,
		16.f / 9.f
	);
	xScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Create main game entity
	Zenith_Entity xMarbleEntity(&xScene, "MarbleGame");

	// UI Setup - anchored to top-left corner
	static constexpr float s_fMarginLeft = 30.f;
	static constexpr float s_fMarginTop = 30.f;
	static constexpr float s_fBaseTextSize = 15.f;
	static constexpr float s_fLineHeight = 24.f;

	Zenith_UIComponent& xUI = xMarbleEntity.AddComponent<Zenith_UIComponent>();

	auto SetupTopLeftText = [](Zenith_UI::Zenith_UIText* pxText, float fYOffset)
	{
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxText->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Left);
	};

	Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "MARBLE ROLL");
	SetupTopLeftText(pxTitle, 0.f);
	pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
	pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxScore = xUI.CreateText("Score", "Score: 0");
	SetupTopLeftText(pxScore, s_fLineHeight * 3);
	pxScore->SetFontSize(s_fBaseTextSize * 3.0f);
	pxScore->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxTime = xUI.CreateText("Time", "Time: 60.0");
	SetupTopLeftText(pxTime, s_fLineHeight * 4);
	pxTime->SetFontSize(s_fBaseTextSize * 3.0f);
	pxTime->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxCollected = xUI.CreateText("Collected", "Collected: 0 / 5");
	SetupTopLeftText(pxCollected, s_fLineHeight * 5);
	pxCollected->SetFontSize(s_fBaseTextSize * 3.0f);
	pxCollected->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("Controls", "WASD: Move | Space: Jump | R: Reset");
	SetupTopLeftText(pxControls, s_fLineHeight * 7);
	pxControls->SetFontSize(s_fBaseTextSize * 2.5f);
	pxControls->SetColor(Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 1.f));

	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "");
	pxStatus->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxStatus->SetPosition(0.f, 0.f);
	pxStatus->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxStatus->SetFontSize(s_fBaseTextSize * 6.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));

	// Add script component with Marble behaviour
	Zenith_ScriptComponent& xScript = xMarbleEntity.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<Marble_Behaviour>();

	// Exit prefab creation mode
	Zenith_Scene::EndPrefabCreation();
}
