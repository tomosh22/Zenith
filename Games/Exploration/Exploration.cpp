#include "Zenith.h"

#include "Exploration/Components/Exploration_Behaviour.h"
#include "Exploration/Components/Exploration_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "AssetHandling/Zenith_DataAssetManager.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <opencv2/opencv.hpp>
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
	Flux_MaterialAsset* g_pxTerrainMaterial0 = nullptr;
	Flux_MaterialAsset* g_pxTerrainMaterial1 = nullptr;

	// Terrain textures (procedural)
	Flux_Texture* g_pxGrassTexture = nullptr;
	Flux_Texture* g_pxRockTexture = nullptr;
}

static bool s_bResourcesInitialized = false;

/**
 * Create a procedural colored texture (single pixel)
 */
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

/**
 * Create a procedural gradient texture for terrain
 */
static Flux_Texture* CreateGradientTexture(
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

	Zenith_AssetHandler::TextureData xTexData;
	xTexData.pData = xPixelData.data();
	xTexData.xSurfaceInfo = xTexInfo;
	xTexData.bCreateMips = false;
	xTexData.bIsCubemap = false;

	return Zenith_AssetHandler::AddTexture(xTexData);
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
	g_pxTerrainMaterial0 = Flux_MaterialAsset::Create("ExplorationTerrainGrass");
	g_pxTerrainMaterial0->SetDiffuseTexture(g_pxGrassTexture);

	g_pxTerrainMaterial1 = Flux_MaterialAsset::Create("ExplorationTerrainRock");
	g_pxTerrainMaterial1->SetDiffuseTexture(g_pxRockTexture);

	s_bResourcesInitialized = true;
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

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeExplorationResources();

	// Register DataAsset types
	RegisterExplorationDataAssets();

	// Register the main game behavior
	Exploration_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Exploration has no resources that need explicit cleanup
}

void Project_LoadInitialScene()
{
	using namespace Exploration;
	using namespace Flux_TerrainConfig;

	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	xScene.Reset();

	// ========================================================================
	// Create Camera Entity
	// ========================================================================
	Zenith_Entity xCameraEntity(&xScene, "MainCamera");
	xCameraEntity.SetTransient(false);

	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();

	// Position camera in the middle of the terrain, at a reasonable height
	// Terrain is centered at origin, so 0,0 is center of terrain
	float fStartX = 0.0f;
	float fStartZ = 0.0f;
	float fStartY = 50.0f;  // Above terrain - will be adjusted by player controller

	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(fStartX, fStartY, fStartZ),
		-0.2f,    // Pitch: slightly looking down
		0.0f,     // Yaw: facing +Z direction
		glm::radians(70.0f),   // FOV: 70 degrees (nice for exploration)
		0.1f,     // Near plane
		5000.0f,  // Far plane (large for terrain viewing)
		16.0f / 9.0f  // Aspect ratio
	);
	xScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	// ========================================================================
	// Create Main Game Entity (with UI and behavior)
	// ========================================================================
	Zenith_Entity xGameEntity(&xScene, "ExplorationGame");
	xGameEntity.SetTransient(false);

	// Add UI component for HUD
	xGameEntity.AddComponent<Zenith_UIComponent>();

	// Add script component with Exploration behaviour
	Zenith_ScriptComponent& xScript = xGameEntity.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<Exploration_Behaviour>();

	// ========================================================================
	// Create Terrain Entity
	// ========================================================================
#ifdef ZENITH_TOOLS
	// Generate terrain meshes if they don't exist (first run)
	if (GenerateAndExportTerrain())
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Creating terrain entity...");

		Zenith_Entity xTerrainEntity(&xScene, "Terrain");
		xTerrainEntity.SetTransient(false);

		// Add terrain component with materials
		// Note: AddComponent automatically passes entity as last arg
		xTerrainEntity.AddComponent<Zenith_TerrainComponent>(
			*g_pxTerrainMaterial0,
			*g_pxTerrainMaterial1);

		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Terrain entity created successfully!");
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] WARNING: Failed to generate terrain, skipping terrain entity creation");
	}
#else
	// In non-tools builds, check if terrain meshes exist and load them
	std::string strTerrainDir = std::string(GAME_ASSETS_DIR) + "Terrain/";
	std::string strFirstChunk = strTerrainDir + "Render_LOD3_0_0" ZENITH_MESH_EXT;
	if (std::filesystem::exists(strFirstChunk))
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Found pre-generated terrain, creating terrain entity...");

		Zenith_Entity xTerrainEntity(&xScene, "Terrain");
		xTerrainEntity.SetTransient(false);

		xTerrainEntity.AddComponent<Zenith_TerrainComponent>(
			*g_pxTerrainMaterial0,
			*g_pxTerrainMaterial1);

		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Terrain entity created successfully!");
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] No terrain meshes found. Run in tools build first to generate terrain.");
	}
#endif

	// ========================================================================
	// Save the scene file
	// ========================================================================
	std::string strScenePath = std::string(GAME_ASSETS_DIR) + "/Scenes/Exploration.zscn";
	std::filesystem::create_directories(std::string(GAME_ASSETS_DIR) + "/Scenes");
	xScene.SaveToFile(strScenePath);
}
