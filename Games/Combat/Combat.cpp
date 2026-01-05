#include "Zenith.h"

#include "Combat/Components/Combat_Behaviour.h"
#include "Combat/Components/Combat_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Flux/Flux.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "AssetHandling/Zenith_DataAssetManager.h"
#include "Prefab/Zenith_Prefab.h"

#include <cmath>

// ============================================================================
// Combat Resources - Global access for behaviours
// ============================================================================
namespace Combat
{
	Flux_MeshGeometry* g_pxCapsuleGeometry = nullptr;
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MaterialAsset* g_pxPlayerMaterial = nullptr;
	Flux_MaterialAsset* g_pxEnemyMaterial = nullptr;
	Flux_MaterialAsset* g_pxArenaMaterial = nullptr;
	Flux_MaterialAsset* g_pxWallMaterial = nullptr;

	// Prefabs for runtime instantiation
	Zenith_Prefab* g_pxPlayerPrefab = nullptr;
	Zenith_Prefab* g_pxEnemyPrefab = nullptr;
	Zenith_Prefab* g_pxArenaPrefab = nullptr;
}

static Flux_Texture* s_pxPlayerTexture = nullptr;
static Flux_Texture* s_pxEnemyTexture = nullptr;
static Flux_Texture* s_pxArenaTexture = nullptr;
static Flux_Texture* s_pxWallTexture = nullptr;
static bool s_bResourcesInitialized = false;

// ============================================================================
// Procedural Texture Generation
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
// Procedural Capsule Geometry Generation
// ============================================================================
static void GenerateCapsule(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices, uint32_t uStacks)
{
	// A capsule is a cylinder with hemispheres on each end
	// Total height = fHeight (cylinder) + 2*fRadius (hemispheres)

	float fCylinderHalfHeight = fHeight * 0.5f;

	// Hemisphere stacks (top and bottom)
	uint32_t uHemiStacks = uStacks / 2;

	// Calculate vertex count
	uint32_t uNumVerts = (uHemiStacks + 1) * (uSlices + 1) * 2 + (uSlices + 1) * 2;  // Two hemispheres + cylinder
	uint32_t uNumIndices = uHemiStacks * uSlices * 6 * 2 + uSlices * 6;  // Triangles

	// Simpler approach: generate a sphere stretched into a capsule shape
	uNumVerts = (uStacks + 1) * (uSlices + 1);
	uNumIndices = uStacks * uSlices * 6;

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

	// Generate capsule vertices (sphere stretched along Y)
	for (uint32_t uStack = 0; uStack <= uStacks; uStack++)
	{
		float fPhi = static_cast<float>(uStack) / static_cast<float>(uStacks) * 3.14159265f;
		float fSphereY = cos(fPhi);  // -1 to 1
		float fStackRadius = sin(fPhi) * fRadius;

		// Stretch Y coordinate based on position
		float fY;
		if (fSphereY > 0.0f)
		{
			// Top hemisphere
			fY = fSphereY * fRadius + fCylinderHalfHeight;
		}
		else
		{
			// Bottom hemisphere
			fY = fSphereY * fRadius - fCylinderHalfHeight;
		}

		for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
		{
			float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
			float fX = cos(fTheta) * fStackRadius;
			float fZ = sin(fTheta) * fStackRadius;

			Zenith_Maths::Vector3 xPos(fX, fY, fZ);

			// Normal calculation (for capsule, it's the normalized position without Y stretching)
			Zenith_Maths::Vector3 xNormal;
			if (fSphereY > 0.0f)
			{
				xNormal = Zenith_Maths::Vector3(fX, cos(fPhi) * fRadius, fZ);
			}
			else
			{
				xNormal = Zenith_Maths::Vector3(fX, cos(fPhi) * fRadius, fZ);
			}

			if (glm::length(xNormal) > 0.001f)
			{
				xNormal = glm::normalize(xNormal);
			}
			else
			{
				xNormal = Zenith_Maths::Vector3(0.0f, fSphereY > 0.0f ? 1.0f : -1.0f, 0.0f);
			}

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(
				static_cast<float>(uSlice) / static_cast<float>(uSlices),
				static_cast<float>(uStack) / static_cast<float>(uStacks)
			);

			// Tangent/bitangent
			Zenith_Maths::Vector3 xTangent(-sin(fTheta), 0.0f, cos(fTheta));
			xGeometryOut.m_pxTangents[uVertIdx] = xTangent;
			xGeometryOut.m_pxBitangents[uVertIdx] = glm::cross(xNormal, xTangent);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

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
static void InitializeCombatResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace Combat;

	// Create capsule geometry (for characters)
	g_pxCapsuleGeometry = new Flux_MeshGeometry();
	GenerateCapsule(*g_pxCapsuleGeometry, 0.5f, 1.0f, 16, 16);

	// Create cube geometry (for arena)
	g_pxCubeGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*g_pxCubeGeometry);

	// Create textures (procedural single-color pixels)
	s_pxPlayerTexture = CreateColoredTexture(51, 102, 230);    // Blue player
	s_pxEnemyTexture = CreateColoredTexture(204, 51, 51);      // Red enemies
	s_pxArenaTexture = CreateColoredTexture(77, 77, 89);       // Gray arena floor
	s_pxWallTexture = CreateColoredTexture(102, 64, 38);       // Brown walls

	// Create materials
	g_pxPlayerMaterial = Flux_MaterialAsset::Create("CombatPlayer");
	g_pxPlayerMaterial->SetDiffuseTexture(s_pxPlayerTexture);

	g_pxEnemyMaterial = Flux_MaterialAsset::Create("CombatEnemy");
	g_pxEnemyMaterial->SetDiffuseTexture(s_pxEnemyTexture);

	g_pxArenaMaterial = Flux_MaterialAsset::Create("CombatArena");
	g_pxArenaMaterial->SetDiffuseTexture(s_pxArenaTexture);

	g_pxWallMaterial = Flux_MaterialAsset::Create("CombatWall");
	g_pxWallMaterial->SetDiffuseTexture(s_pxWallTexture);

	// Create prefabs for runtime instantiation
	// Note: Prefabs are lightweight templates - components added after transform is set
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Player prefab
	{
		Zenith_Entity xPlayerTemplate(&xScene, "PlayerTemplate");
		g_pxPlayerPrefab = new Zenith_Prefab();
		g_pxPlayerPrefab->CreateFromEntity(xPlayerTemplate, "Player");
		Zenith_Scene::Destroy(xPlayerTemplate);
	}

	// Enemy prefab
	{
		Zenith_Entity xEnemyTemplate(&xScene, "EnemyTemplate");
		g_pxEnemyPrefab = new Zenith_Prefab();
		g_pxEnemyPrefab->CreateFromEntity(xEnemyTemplate, "Enemy");
		Zenith_Scene::Destroy(xEnemyTemplate);
	}

	// Arena prefab
	{
		Zenith_Entity xArenaTemplate(&xScene, "ArenaTemplate");
		g_pxArenaPrefab = new Zenith_Prefab();
		g_pxArenaPrefab->CreateFromEntity(xArenaTemplate, "Arena");
		Zenith_Scene::Destroy(xArenaTemplate);
	}

	s_bResourcesInitialized = true;
}

// ============================================================================
// Project Entry Points
// ============================================================================
const char* Project_GetName()
{
	return "Combat";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeCombatResources();

	// Register DataAsset types
	RegisterCombatDataAssets();

	// Register behaviors
	Combat_Behaviour::RegisterBehaviour();
}

void Project_LoadInitialScene()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	xScene.Reset();

	// Create camera entity
	Zenith_Entity xCameraEntity(&xScene, "MainCamera");
	xScene.GetEntityRef(xCameraEntity.GetEntityID()).SetTransient(false);
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.0f, 12.0f, -15.0f),  // Position: above and behind
		-0.7f,  // Pitch: looking down at arena
		0.0f,   // Yaw: facing forward
		glm::radians(50.0f),  // FOV
		0.1f,
		1000.0f,
		16.0f / 9.0f
	);
	xScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Create main game entity
	Zenith_Entity xCombatEntity(&xScene, "CombatGame");
	xScene.GetEntityRef(xCombatEntity.GetEntityID()).SetTransient(false);

	// UI Setup
	static constexpr float s_fMarginLeft = 30.0f;
	static constexpr float s_fMarginTop = 30.0f;
	static constexpr float s_fBaseTextSize = 15.0f;
	static constexpr float s_fLineHeight = 24.0f;

	Zenith_UIComponent& xUI = xCombatEntity.AddComponent<Zenith_UIComponent>();

	auto SetupTopLeftText = [](Zenith_UI::Zenith_UIText* pxText, float fYOffset)
	{
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxText->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Left);
	};

	// Title
	Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "COMBAT ARENA");
	SetupTopLeftText(pxTitle, 0.0f);
	pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
	pxTitle->SetColor(Zenith_Maths::Vector4(1.0f, 0.2f, 0.2f, 1.0f));

	// Player Health
	Zenith_UI::Zenith_UIText* pxHealth = xUI.CreateText("PlayerHealth", "Health: 100 / 100");
	SetupTopLeftText(pxHealth, s_fLineHeight * 3);
	pxHealth->SetFontSize(s_fBaseTextSize * 3.0f);
	pxHealth->SetColor(Zenith_Maths::Vector4(0.2f, 1.0f, 0.2f, 1.0f));

	// Health Bar
	Zenith_UI::Zenith_UIText* pxHealthBar = xUI.CreateText("PlayerHealthBar", "[||||||||||||||||||||]");
	SetupTopLeftText(pxHealthBar, s_fLineHeight * 4);
	pxHealthBar->SetFontSize(s_fBaseTextSize * 2.5f);
	pxHealthBar->SetColor(Zenith_Maths::Vector4(0.2f, 1.0f, 0.2f, 1.0f));

	// Enemy Count
	Zenith_UI::Zenith_UIText* pxEnemyCount = xUI.CreateText("EnemyCount", "Enemies: 3 / 3");
	SetupTopLeftText(pxEnemyCount, s_fLineHeight * 6);
	pxEnemyCount->SetFontSize(s_fBaseTextSize * 3.0f);
	pxEnemyCount->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.0f));

	// Combo Counter (center of screen)
	Zenith_UI::Zenith_UIText* pxComboCount = xUI.CreateText("ComboCount", "");
	pxComboCount->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxComboCount->SetPosition(0.0f, -100.0f);
	pxComboCount->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxComboCount->SetFontSize(s_fBaseTextSize * 8.0f);
	pxComboCount->SetColor(Zenith_Maths::Vector4(1.0f, 0.8f, 0.2f, 1.0f));

	Zenith_UI::Zenith_UIText* pxComboText = xUI.CreateText("ComboText", "");
	pxComboText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxComboText->SetPosition(0.0f, -60.0f);
	pxComboText->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxComboText->SetFontSize(s_fBaseTextSize * 4.0f);
	pxComboText->SetColor(Zenith_Maths::Vector4(1.0f, 0.8f, 0.2f, 1.0f));

	// Controls (bottom left)
	Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("Controls", "WASD: Move | LMB: Light Attack | RMB: Heavy Attack | Space: Dodge | R: Reset");
	pxControls->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomLeft);
	pxControls->SetPosition(s_fMarginLeft, s_fMarginTop);
	pxControls->SetAlignment(Zenith_UI::TextAlignment::Left);
	pxControls->SetFontSize(s_fBaseTextSize * 2.5f);
	pxControls->SetColor(Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 1.0f));

	// Status (center - for game over/victory/paused)
	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "");
	pxStatus->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxStatus->SetPosition(0.0f, 0.0f);
	pxStatus->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxStatus->SetFontSize(s_fBaseTextSize * 8.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(0.2f, 1.0f, 0.2f, 1.0f));

	// Add script component with Combat behaviour
	Zenith_ScriptComponent& xScript = xCombatEntity.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<Combat_Behaviour>();

	// Save the scene file
	std::string strScenePath = std::string(GAME_ASSETS_DIR) + "/Scenes/Combat.zscn";
	std::filesystem::create_directories(std::string(GAME_ASSETS_DIR) + "/Scenes");
	xScene.SaveToFile(strScenePath);
}
