#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "Exploration/Components/Exploration_Behaviour.h"
#include "Exploration/Components/Exploration_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "UI/Zenith_UIButton.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#pragma warning(push, 0)
#include <opencv2/opencv.hpp>
#pragma warning(pop)
#include "Memory/Zenith_MemoryManagement_Enabled.h"
extern void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strMaterialPath, const std::string& strOutputDir);
#endif

#include <filesystem>
#include <cmath>

// ============================================================================
// Exploration Resources
// ============================================================================
namespace Exploration
{
	// Terrain materials
	MaterialHandle g_xTerrainMaterial0;
	MaterialHandle g_xTerrainMaterial1;

	// Terrain textures (procedural)
	Zenith_TextureAsset* g_pxGrassTexture = nullptr;
	Zenith_TextureAsset* g_pxRockTexture = nullptr;
}

static bool s_bResourcesInitialized = false;

/**
 * Create a procedural gradient texture for terrain
 */
static Zenith_TextureAsset* CreateGradientTexture(
	uint8_t uR1, uint8_t uG1, uint8_t uB1,
	uint8_t uR2, uint8_t uG2, uint8_t uB2,
	uint32_t uWidth, uint32_t uHeight)
{
	Flux_SurfaceInfo xTexInfo;
	xTexInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xTexInfo.m_uWidth = uWidth;
	xTexInfo.m_uHeight = uHeight;
	xTexInfo.m_uDepth = 1;
	xTexInfo.m_uNumMips = 1;
	xTexInfo.m_uNumLayers = 1;
	xTexInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	std::vector<uint8_t> xPixelData(uWidth * uHeight * 4);

	for (uint32_t y = 0; y < uHeight; ++y)
	{
		float fT = static_cast<float>(y) / static_cast<float>(uHeight - 1);
		uint8_t uR = static_cast<uint8_t>(uR1 + (uR2 - uR1) * fT);
		uint8_t uG = static_cast<uint8_t>(uG1 + (uG2 - uG1) * fT);
		uint8_t uB = static_cast<uint8_t>(uB1 + (uB2 - uB1) * fT);

		for (uint32_t x = 0; x < uWidth; ++x)
		{
			uint32_t uIdx = (y * uWidth + x) * 4;
			xPixelData[uIdx + 0] = uR;
			xPixelData[uIdx + 1] = uG;
			xPixelData[uIdx + 2] = uB;
			xPixelData[uIdx + 3] = 255;
		}
	}

	// Create texture via new asset system
	Zenith_TextureAsset* pxTexture = Zenith_AssetRegistry::Get().Create<Zenith_TextureAsset>();
	if (pxTexture)
	{
		pxTexture->CreateFromData(xPixelData.data(), xTexInfo, false);
	}
	return pxTexture;
}

#ifdef ZENITH_TOOLS
/**
 * Generate procedural heightmap using multi-octave sine waves
 * This creates rolling hills terrain similar to Exploration_TerrainExplorer::GetTerrainHeightAt()
 *
 * @param uSize Image size (must be 4096 for terrain system)
 * @param fTerrainWorldSize World size the heightmap covers
 * @return 32-bit float cv::Mat heightmap (values 0.0-1.0)
 */
static cv::Mat GenerateProceduralHeightmap(uint32_t uSize, float fTerrainWorldSize)
{
	// Terrain export expects CV_32FC1 (32-bit float, single channel)
	cv::Mat xHeightmap(uSize, uSize, CV_32FC1);

	// Our procedural function returns heights in approximate range 0-100
	// MAX_TERRAIN_HEIGHT in export is 4096, and heights are multiplied by that
	// So we normalize our heights to 0-1 range
	const float fMaxProceduralHeight = 100.0f;

	for (uint32_t y = 0; y < uSize; ++y)
	{
		for (uint32_t x = 0; x < uSize; ++x)
		{
			// Convert pixel coordinate to world coordinate
			// Terrain is centered at origin, so offset by half
			float fWorldX = (static_cast<float>(x) / static_cast<float>(uSize - 1)) * fTerrainWorldSize - fTerrainWorldSize * 0.5f;
			float fWorldZ = (static_cast<float>(y) / static_cast<float>(uSize - 1)) * fTerrainWorldSize - fTerrainWorldSize * 0.5f;

			// Calculate procedural height (same algorithm as Exploration_TerrainExplorer::GetTerrainHeightAt)
			float fHeight = 0.0f;

			// Large hills
			float fFreq1 = 0.001f;
			fHeight += std::sin(fWorldX * fFreq1) * std::cos(fWorldZ * fFreq1) * 50.0f;

			// Medium features
			float fFreq2 = 0.005f;
			fHeight += std::sin(fWorldX * fFreq2 + 1.3f) * std::cos(fWorldZ * fFreq2 + 0.7f) * 20.0f;

			// Small details
			float fFreq3 = 0.02f;
			fHeight += std::sin(fWorldX * fFreq3 + 2.1f) * std::cos(fWorldZ * fFreq3 + 1.4f) * 5.0f;

			// Add base height to keep most terrain above water level
			fHeight += 30.0f;

			// Clamp to reasonable terrain bounds
			fHeight = std::max(0.0f, fHeight);

			// Normalize to 0-1 range
			float fNormalized = fHeight / fMaxProceduralHeight;
			fNormalized = std::clamp(fNormalized, 0.0f, 1.0f);

			xHeightmap.at<float>(y, x) = fNormalized;
		}
	}

	// Flip vertically to match terrain export expectations
	// OpenCV stores images top-to-bottom, but terrain export expects bottom-to-top
	cv::flip(xHeightmap, xHeightmap, 0);  // 0 = flip around x-axis (vertical flip)

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Generated procedural heightmap: %ux%u", uSize, uSize);
	return xHeightmap;
}

/**
 * Generate material interpolation map based on height
 * Lower areas get material 0 (grass), higher areas get material 1 (rock)
 *
 * @param xHeightmap The heightmap to base materials on (CV_32FC1 format)
 * @return 32-bit float cv::Mat material map (values 0.0-1.0)
 */
static cv::Mat GenerateMaterialMap(const cv::Mat& xHeightmap)
{
	// Terrain export expects CV_32FC1 (32-bit float, single channel)
	cv::Mat xMaterialMap(xHeightmap.rows, xHeightmap.cols, CV_32FC1);

	// Find min/max heights for normalization
	double dMin, dMax;
	cv::minMaxLoc(xHeightmap, &dMin, &dMax);

	for (int y = 0; y < xHeightmap.rows; ++y)
	{
		for (int x = 0; x < xHeightmap.cols; ++x)
		{
			float fHeight = xHeightmap.at<float>(y, x);

			// Normalize height to 0-1 (already normalized, but account for actual range)
			float fNormHeight = static_cast<float>((fHeight - dMin) / (dMax - dMin));

			// Material blend: grass below 0.4 height, rock above 0.6, blend in between
			float fMaterialLerp = 0.0f;
			if (fNormHeight < 0.4f)
			{
				fMaterialLerp = 0.0f;  // Full grass
			}
			else if (fNormHeight > 0.6f)
			{
				fMaterialLerp = 1.0f;  // Full rock
			}
			else
			{
				// Smooth blend between 0.4 and 0.6
				fMaterialLerp = (fNormHeight - 0.4f) / 0.2f;
			}

			xMaterialMap.at<float>(y, x) = fMaterialLerp;
		}
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Generated material map: %dx%d", xMaterialMap.cols, xMaterialMap.rows);
	return xMaterialMap;
}

/**
 * Generate terrain data and export mesh files
 * This creates the heightmap, material map, and exports all terrain chunks
 */
static bool GenerateAndExportTerrain()
{
	using namespace Flux_TerrainConfig;

	std::string strTerrainDir = std::string(GAME_ASSETS_DIR) + "Terrain/";
	std::string strHeightmapPath = strTerrainDir + "ExplorationHeightmap.tif";
	std::string strMaterialPath = strTerrainDir + "ExplorationMaterial.tif";

	// Create terrain directory
	std::filesystem::create_directories(strTerrainDir);

	// Check if terrain already exists
	std::string strFirstChunk = strTerrainDir + "Render_LOD3_0_0" ZENITH_MESH_EXT;
	if (std::filesystem::exists(strFirstChunk))
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Terrain mesh files already exist, skipping generation");
		return true;
	}

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Generating procedural terrain...");

	// Generate 4096x4096 heightmap (required size for terrain system)
	cv::Mat xHeightmap = GenerateProceduralHeightmap(4096, TERRAIN_SIZE);

	// Generate material map based on height
	cv::Mat xMaterialMap = GenerateMaterialMap(xHeightmap);

	// Save heightmaps as .tif files
	if (!cv::imwrite(strHeightmapPath, xHeightmap))
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] ERROR: Failed to save heightmap to %s", strHeightmapPath.c_str());
		return false;
	}
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Saved heightmap: %s", strHeightmapPath.c_str());

	if (!cv::imwrite(strMaterialPath, xMaterialMap))
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] ERROR: Failed to save material map to %s", strMaterialPath.c_str());
		return false;
	}
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Saved material map: %s", strMaterialPath.c_str());

	// Export terrain meshes (this generates LOD0-LOD3 and physics meshes for all chunks)
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Exporting terrain meshes (this may take a while)...");
	ExportHeightmapFromPaths(strHeightmapPath, strMaterialPath, strTerrainDir);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Terrain mesh export complete!");

	return true;
}
#endif // ZENITH_TOOLS

/**
 * Initialize exploration resources (textures, materials)
 */
static void InitializeExplorationResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace Exploration;

	// Create procedural terrain textures
	// Grass: green with slight variation
	g_pxGrassTexture = CreateGradientTexture(
		60, 120, 40,    // Dark grass
		80, 160, 60,    // Light grass
		4, 4);

	// Rock: gray with variation
	g_pxRockTexture = CreateGradientTexture(
		100, 95, 90,    // Dark rock
		140, 135, 130,  // Light rock
		4, 4);

	// Create terrain materials
	auto& xRegistry = Zenith_AssetRegistry::Get();
	g_xTerrainMaterial0.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xTerrainMaterial0.Get()->SetName("ExplorationTerrainGrass");
	g_xTerrainMaterial0.Get()->SetDiffuseTextureDirectly(g_pxGrassTexture);

	g_xTerrainMaterial1.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xTerrainMaterial1.Get()->SetName("ExplorationTerrainRock");
	g_xTerrainMaterial1.Get()->SetDiffuseTextureDirectly(g_pxRockTexture);

	s_bResourcesInitialized = true;
}

// ============================================================================
// Instanced Trees System
// ============================================================================
namespace Exploration
{
	// Resources for instanced trees
	MaterialHandle g_xTreeMaterial;
	Zenith_InstancedMeshComponent* g_pxTreeComponent = nullptr;
}


/**
 * Simple pseudo-random number generator using position as seed
 */
static float RandomFromPosition(float fX, float fZ, float fOffset)
{
	float fSeed = fX * 12.9898f + fZ * 78.233f + fOffset;
	return std::fmod(std::sin(fSeed) * 43758.5453f, 1.0f);
}

/**
 * Spawn instanced trees across the terrain
 */
static void SpawnInstancedTrees(Zenith_InstancedMeshComponent& xTreeComponent, uint32_t uTargetCount)
{
	using namespace Flux_TerrainConfig;

	// Reserve capacity upfront for efficiency
	xTreeComponent.Reserve(uTargetCount);

	// Calculate grid spacing for even distribution
	float fArea = TERRAIN_SIZE * TERRAIN_SIZE;
	float fTreesPerUnit = static_cast<float>(uTargetCount) / fArea;
	float fSpacing = 1.0f / std::sqrt(fTreesPerUnit);

	// Calculate grid dimensions
	uint32_t uGridDim = static_cast<uint32_t>(TERRAIN_SIZE / fSpacing);
	float fHalfTerrain = TERRAIN_SIZE * 0.5f;

	uint32_t uSpawnedCount = 0;
	for (uint32_t gz = 0; gz < uGridDim && uSpawnedCount < uTargetCount; ++gz)
	{
		for (uint32_t gx = 0; gx < uGridDim && uSpawnedCount < uTargetCount; ++gx)
		{
			// Base position in grid
			float fBaseX = (gx + 0.5f) * fSpacing - fHalfTerrain;
			float fBaseZ = (gz + 0.5f) * fSpacing - fHalfTerrain;

			// Add random offset within cell for natural appearance
			float fOffsetX = (RandomFromPosition(fBaseX, fBaseZ, 0.0f) - 0.5f) * fSpacing * 0.8f;
			float fOffsetZ = (RandomFromPosition(fBaseX, fBaseZ, 1.0f) - 0.5f) * fSpacing * 0.8f;

			float fX = fBaseX + fOffsetX;
			float fZ = fBaseZ + fOffsetZ;

			// Convert tree position from centered coords to terrain mesh coords
			// Terrain mesh X/Z goes from 0 to TERRAIN_SIZE, not -TERRAIN_SIZE/2 to +TERRAIN_SIZE/2
			float fMeshX = fX + fHalfTerrain;
			float fMeshZ = fZ + fHalfTerrain;

			// Get terrain height at mesh position using TerrainExplorer
			// This function handles the coordinate transformations and returns mesh-scale height
			float fMeshY = Exploration_TerrainExplorer::GetTerrainHeightAt(fMeshX, fMeshZ);

			// Skip trees in very low areas (water level)
			// Mesh height -1000 corresponds to normalized 0, so -500 is roughly 12% height
			if (fMeshY < -500.0f)
				continue;

			// Skip trees on steep slopes (high areas = rocky)
			// Mesh height 3096 is max, so 1500 is roughly 60% height
			if (fMeshY > 1500.0f)
			{
				// Random chance to skip in rocky areas
				if (RandomFromPosition(fX, fZ, 2.0f) > 0.3f)
					continue;
			}

			// Random scale variation (0.8 to 1.2), multiplied by 5 for visibility
			float fScale = (0.8f + RandomFromPosition(fX, fZ, 3.0f) * 0.4f) * 5.0f;

			// Random rotation around Y axis
			float fRotation = RandomFromPosition(fX, fZ, 4.0f) * 6.28318f;
			Zenith_Maths::Quat xRotation = glm::angleAxis(fRotation, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

			// Spawn the tree
			uint32_t uInstanceID = xTreeComponent.SpawnInstance(
				Zenith_Maths::Vector3(fMeshX, fMeshY, fMeshZ),
				xRotation,
				Zenith_Maths::Vector3(fScale, fScale, fScale)
			);

			// Random animation phase offset so trees don't all sway in sync
			float fPhase = RandomFromPosition(fX, fZ, 5.0f);
			xTreeComponent.SetInstanceAnimationTime(uInstanceID, fPhase);

			// Slight color variation (green tint)
			float fColorVar = 0.8f + RandomFromPosition(fX, fZ, 6.0f) * 0.4f;
			xTreeComponent.SetInstanceColor(uInstanceID, Zenith_Maths::Vector4(
				0.3f * fColorVar,
				0.5f * fColorVar,
				0.2f * fColorVar,
				1.0f
			));

			++uSpawnedCount;
		}
	}

	Zenith_Log(LOG_CATEGORY_MESH, "[Exploration] Spawned %u instanced trees", uSpawnedCount);
}

/**
 * Create instanced trees entity and spawn trees
 */
static void CreateInstancedTrees(Zenith_SceneData* pxSceneData)
{
	using namespace Exploration;

	std::string strTreeDir = std::string(ENGINE_ASSETS_DIR) + "Meshes/ProceduralTree/";
	std::string strMeshAssetPath = strTreeDir + "Tree.zasset";
	std::string strVATPath = strTreeDir + "Tree_Sway.zanmt";

	// Check if tree assets exist
	if (!std::filesystem::exists(strMeshAssetPath))
	{
		Zenith_Log(LOG_CATEGORY_MESH, "[Exploration] Tree mesh not found: %s", strMeshAssetPath.c_str());
		Zenith_Log(LOG_CATEGORY_MESH, "[Exploration] Run unit tests first to generate tree assets");
		return;
	}

	Zenith_Log(LOG_CATEGORY_MESH, "[Exploration] Creating instanced trees entity...");

	// Create tree material (green with some variation) - guard for replay
	if (g_xTreeMaterial.Get() == nullptr)
	{
		g_xTreeMaterial.Set(Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>());
		g_xTreeMaterial.Get()->SetName("TreeMaterial");
		g_xTreeMaterial.Get()->SetBaseColor(Zenith_Maths::Vector4(0.3f, 0.5f, 0.2f, 1.0f));
	}

	// Create entity with instanced mesh component
	Zenith_Entity xTreesEntity(pxSceneData, "InstancedTrees");
	xTreesEntity.SetTransient(false);

	Zenith_InstancedMeshComponent& xTrees = xTreesEntity.AddComponent<Zenith_InstancedMeshComponent>();
	g_pxTreeComponent = &xTrees;

	// Load mesh
	xTrees.LoadMesh(strMeshAssetPath);

	// Load VAT if available
	if (std::filesystem::exists(strVATPath))
	{
		xTrees.LoadAnimationTexture(strVATPath);
		xTrees.SetAnimationDuration(2.0f);  // 2 second sway cycle
		xTrees.SetAnimationSpeed(1.0f);
		Zenith_Log(LOG_CATEGORY_MESH, "[Exploration] Loaded tree animation texture");
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_MESH, "[Exploration] No VAT found, trees will be static");
	}

	// Set material
	xTrees.SetMaterial(g_xTreeMaterial.Get());

	// Spawn trees (start with 10k for testing, can increase to 100k)
	// Reduced count for initial testing to ensure performance is acceptable
	SpawnInstancedTrees(xTrees, 10000);

	Zenith_Log(LOG_CATEGORY_MESH, "[Exploration] Instanced trees created: %u instances", xTrees.GetInstanceCount());
}

// ============================================================================
// World Content Creation (callable from behaviour)
// ============================================================================

void Exploration_CreateWorldContent(Zenith_SceneData* pxSceneData)
{
	using namespace Exploration;

	// Create terrain entity
#ifdef ZENITH_TOOLS
	if (GenerateAndExportTerrain())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Creating terrain entity...");

		Zenith_Entity xTerrainEntity(pxSceneData, "Terrain");
		xTerrainEntity.SetTransient(false);

		xTerrainEntity.AddComponent<Zenith_TerrainComponent>(
			*g_xTerrainMaterial0.Get(),
			*g_xTerrainMaterial1.Get());

		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Terrain entity created successfully!");
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] WARNING: Failed to generate terrain, skipping terrain entity creation");
	}
#else
	std::string strTerrainDir = std::string(GAME_ASSETS_DIR) + "Terrain/";
	std::string strFirstChunk = strTerrainDir + "Render_LOD3_0_0" ZENITH_MESH_EXT;
	if (std::filesystem::exists(strFirstChunk))
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Found pre-generated terrain, creating terrain entity...");

		Zenith_Entity xTerrainEntity(pxSceneData, "Terrain");
		xTerrainEntity.SetTransient(false);

		xTerrainEntity.AddComponent<Zenith_TerrainComponent>(
			*g_xTerrainMaterial0.Get(),
			*g_xTerrainMaterial1.Get());

		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Terrain entity created successfully!");
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] No terrain meshes found. Run in tools build first to generate terrain.");
	}
#endif

	// Create instanced trees
	CreateInstancedTrees(pxSceneData);
}

void Exploration_CleanupWorldContent()
{
	Exploration::g_pxTreeComponent = nullptr;
}

// ============================================================================
// Project Entry Points
// ============================================================================

const char* Project_GetName()
{
	return "Exploration";
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
	InitializeExplorationResources();

	// Register the main game behavior
	Exploration_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Exploration has no resources that need explicit cleanup
}

void Project_CreateScenes()
{
	using namespace Flux_TerrainConfig;

	// ---- MainMenu scene (build index 0) ----
	{
		const std::string strMenuPath = GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT;

		Zenith_Scene xMenuScene = Zenith_SceneManager::CreateEmptyScene("MainMenu");
		Zenith_SceneData* pxMenuData = Zenith_SceneManager::GetSceneData(xMenuScene);

		Zenith_Entity xMenuManager(pxMenuData, "MenuManager");
		xMenuManager.SetTransient(false);

		// Camera - first-person view at terrain center
		Zenith_CameraComponent& xCamera = xMenuManager.AddComponent<Zenith_CameraComponent>();
		float fStartX = TERRAIN_SIZE * 0.5f;
		float fStartZ = TERRAIN_SIZE * 0.5f;
		float fStartY = 1200.0f;

		xCamera.InitialisePerspective({
			.m_xPosition = Zenith_Maths::Vector3(fStartX, fStartY, fStartZ),
			.m_fPitch = -0.2f,
			.m_fFOV = glm::radians(70.0f),
			.m_fFar = 10000.0f,
		});
		pxMenuData->SetMainCameraEntity(xMenuManager.GetEntityID());

		Zenith_UIComponent& xUI = xMenuManager.AddComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIText* pxMenuTitle = xUI.CreateText("MenuTitle", "EXPLORATION");
		pxMenuTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxMenuTitle->SetPosition(0.f, -120.f);
		pxMenuTitle->SetFontSize(48.f);
		pxMenuTitle->SetColor(Zenith_Maths::Vector4(0.3f, 0.7f, 0.3f, 1.f));

		Zenith_UI::Zenith_UIButton* pxPlayButton = xUI.CreateButton("MenuPlay", "Play");
		pxPlayButton->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxPlayButton->SetPosition(0.f, 0.f);
		pxPlayButton->SetSize(200.f, 50.f);

		Zenith_ScriptComponent& xScript = xMenuManager.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviourForSerialization<Exploration_Behaviour>();

		pxMenuData->SaveToFile(strMenuPath);
		Zenith_SceneManager::RegisterSceneBuildIndex(0, strMenuPath);
		Zenith_SceneManager::UnloadScene(xMenuScene);
	}

	// ---- Exploration gameplay scene (build index 1) ----
	{
		const std::string strGamePath = GAME_ASSETS_DIR "Scenes/Exploration" ZENITH_SCENE_EXT;

		Zenith_Scene xGameScene = Zenith_SceneManager::CreateEmptyScene("Exploration");
		Zenith_SceneData* pxGameData = Zenith_SceneManager::GetSceneData(xGameScene);

		Zenith_Entity xGameManager(pxGameData, "GameManager");
		xGameManager.SetTransient(false);

		// Camera - first-person view at terrain center
		Zenith_CameraComponent& xCamera = xGameManager.AddComponent<Zenith_CameraComponent>();
		float fStartX = TERRAIN_SIZE * 0.5f;
		float fStartZ = TERRAIN_SIZE * 0.5f;
		float fStartY = 1200.0f;

		xCamera.InitialisePerspective({
			.m_xPosition = Zenith_Maths::Vector3(fStartX, fStartY, fStartZ),
			.m_fPitch = -0.2f,
			.m_fFOV = glm::radians(70.0f),
			.m_fFar = 10000.0f,
		});
		pxGameData->SetMainCameraEntity(xGameManager.GetEntityID());

		// HUD UI is created by Exploration_UIManager in OnStart

		Zenith_ScriptComponent& xScript = xGameManager.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviourForSerialization<Exploration_Behaviour>();

		pxGameData->SaveToFile(strGamePath);
		Zenith_SceneManager::RegisterSceneBuildIndex(1, strGamePath);
		Zenith_SceneManager::UnloadScene(xGameScene);
	}
}

void Project_LoadInitialScene()
{
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
