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
#include "DataStream/Zenith_DataStream.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "UI/Zenith_UIButton.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#pragma warning(push, 0)
#include <opencv2/opencv.hpp>
#pragma warning(pop)
#include "Memory/Zenith_MemoryManagement_Enabled.h"
extern void ExportHeightmapFromPaths(const std::string& strHeightmapPath, const std::string& strOutputDir);
extern void ExportHeightmapFromMat(const cv::Mat& xHeightmap, const std::string& strOutputDir);
#endif

#include <filesystem>
#include <cmath>

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#endif

// ============================================================================
// Exploration Resources
// ============================================================================
namespace Exploration
{
	// Terrain materials (4-material splatmap palette)
	MaterialHandle g_axTerrainMaterials[4];
	TextureHandle g_xTerrainSplatmap;
}

static bool s_bResourcesInitialized = false;

#ifdef ZENITH_TOOLS
/**
 * Export a solid-color RGBA8 texture to disk as .ztxtr
 */
static void ExportSolidTexture(
	const std::string& strPath,
	uint8_t uR, uint8_t uG, uint8_t uB, uint8_t uA,
	uint32_t uWidth, uint32_t uHeight)
{
	std::vector<uint8_t> xPixelData(uWidth * uHeight * 4);
	for (uint32_t i = 0; i < uWidth * uHeight; ++i)
	{
		xPixelData[i * 4 + 0] = uR;
		xPixelData[i * 4 + 1] = uG;
		xPixelData[i * 4 + 2] = uB;
		xPixelData[i * 4 + 3] = uA;
	}

	Zenith_DataStream xStream;
	xStream << static_cast<int32_t>(uWidth);
	xStream << static_cast<int32_t>(uHeight);
	xStream << static_cast<int32_t>(1);  // depth
	xStream << static_cast<TextureFormat>(TEXTURE_FORMAT_RGBA8_UNORM);
	xStream << static_cast<size_t>(xPixelData.size());
	xStream.WriteData(xPixelData.data(), xPixelData.size());
	xStream.WriteToFile(strPath.c_str());

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Exported texture: %s", strPath.c_str());
}

/**
 * Export all 5 PBR textures for a terrain material
 * Diffuse = solid color, Normal = flat (128,128,255), RM = (roughness, metallic), Occlusion = white, Emissive = black
 */
static void ExportMaterialTextures(
	const std::string& strDir, const std::string& strName,
	uint8_t uDiffR, uint8_t uDiffG, uint8_t uDiffB,
	uint8_t uRoughness, uint8_t uMetallic)
{
	ExportSolidTexture(strDir + strName + "_Diffuse" ZENITH_TEXTURE_EXT, uDiffR, uDiffG, uDiffB, 255, 4, 4);
	ExportSolidTexture(strDir + strName + "_Normal" ZENITH_TEXTURE_EXT, 128, 128, 255, 255, 4, 4);
	ExportSolidTexture(strDir + strName + "_RM" ZENITH_TEXTURE_EXT, 0, uRoughness, uMetallic, 255, 4, 4);
	ExportSolidTexture(strDir + strName + "_Occlusion" ZENITH_TEXTURE_EXT, 255, 255, 255, 255, 4, 4);
	ExportSolidTexture(strDir + strName + "_Emissive" ZENITH_TEXTURE_EXT, 0, 0, 0, 255, 4, 4);
}

struct SplatmapSlopeData
{
	const cv::Mat* pxHeightmap;
	float* pfSlopes;
	float* pfPerInvocationMaxSlope;
	int iSize;
};

static void ComputeSlopesTask(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
{
	SplatmapSlopeData* pxData = static_cast<SplatmapSlopeData*>(pData);
	const cv::Mat& xHeightmap = *pxData->pxHeightmap;
	const int iSize = pxData->iSize;

	u_int uRowsPerInvocation = (iSize + uNumInvocations - 1) / uNumInvocations;
	u_int uStartRow = uInvocationIndex * uRowsPerInvocation;
	u_int uEndRow = std::min(uStartRow + uRowsPerInvocation, static_cast<u_int>(iSize));

	float fLocalMaxSlope = 0.0f;
	for (u_int y = uStartRow; y < uEndRow; ++y)
	{
		const float* pfRow = xHeightmap.ptr<float>(y);
		const float* pfRowUp = (y > 0) ? xHeightmap.ptr<float>(y - 1) : pfRow;
		const float* pfRowDown = (y < static_cast<u_int>(iSize - 1)) ? xHeightmap.ptr<float>(y + 1) : pfRow;

		for (int x = 0; x < iSize; ++x)
		{
			float fHeight = pfRow[x];
			float fLeft = (x > 0) ? pfRow[x - 1] : fHeight;
			float fRight = (x < iSize - 1) ? pfRow[x + 1] : fHeight;
			float fDx = (fRight - fLeft) * 0.5f;
			float fDy = (pfRowDown[x] - pfRowUp[x]) * 0.5f;
			float fSlope = std::sqrt(fDx * fDx + fDy * fDy);
			pxData->pfSlopes[y * iSize + x] = fSlope;
			fLocalMaxSlope = std::max(fLocalMaxSlope, fSlope);
		}
	}
	pxData->pfPerInvocationMaxSlope[uInvocationIndex] = fLocalMaxSlope;
}

struct SplatmapWeightData
{
	const cv::Mat* pxHeightmap;
	const float* pfSlopes;
	uint8_t* puPixelData;
	int iSize;
	double dMin;
	double dRange;
	float fMaxSlope;
};

static void GenerateWeightsTask(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
{
	SplatmapWeightData* pxData = static_cast<SplatmapWeightData*>(pData);
	const cv::Mat& xHeightmap = *pxData->pxHeightmap;
	const int iSize = pxData->iSize;
	const float fMaxSlope = pxData->fMaxSlope;
	const double dMin = pxData->dMin;
	const double dRange = pxData->dRange;

	u_int uRowsPerInvocation = (iSize + uNumInvocations - 1) / uNumInvocations;
	u_int uStartRow = uInvocationIndex * uRowsPerInvocation;
	u_int uEndRow = std::min(uStartRow + uRowsPerInvocation, static_cast<u_int>(iSize));

	for (u_int y = uStartRow; y < uEndRow; ++y)
	{
		const float* pfRow = xHeightmap.ptr<float>(y);
		for (int x = 0; x < iSize; ++x)
		{
			float fElev = static_cast<float>((pfRow[x] - dMin) / dRange);
			float fSlope = pxData->pfSlopes[y * iSize + x] / fMaxSlope;

			float fSand = std::max(0.0f, 1.0f - fElev / 0.2f);
			float fDirtElev = std::max(0.0f, std::min((fElev - 0.05f) / 0.1f, (0.4f - fElev) / 0.15f));
			float fDirt = fDirtElev * std::max(0.0f, 1.0f - fSlope / 0.5f);
			float fGrassElev = std::max(0.0f, std::min((fElev - 0.05f) / 0.15f, (0.7f - fElev) / 0.2f));
			float fGrass = fGrassElev * std::max(0.0f, 1.0f - fSlope / 0.7f);
			float fRock = std::max(
				std::max(0.0f, (fSlope - 0.25f) / 0.75f),
				std::max(0.0f, (fElev - 0.55f) / 0.45f)
			);

			fGrass = std::max(fGrass, 0.05f);

			float fTotal = fGrass + fRock + fDirt + fSand;
			fGrass /= fTotal;
			fRock /= fTotal;
			fDirt /= fTotal;
			fSand /= fTotal;

			uint32_t uIdx = (y * iSize + x) * 4;
			pxData->puPixelData[uIdx + 0] = static_cast<uint8_t>(fGrass * 255.0f + 0.5f);
			pxData->puPixelData[uIdx + 1] = static_cast<uint8_t>(fRock * 255.0f + 0.5f);
			pxData->puPixelData[uIdx + 2] = static_cast<uint8_t>(fDirt * 255.0f + 0.5f);
			pxData->puPixelData[uIdx + 3] = static_cast<uint8_t>(fSand * 255.0f + 0.5f);
		}
	}
}

/**
 * Generate procedural splatmap (RGBA8) from heightmap
 * R=grass, G=rock, B=dirt, A=sand
 */
static void GenerateProceduralSplatmap(const cv::Mat& xHeightmap, const std::string& strOutputPath)
{
	int iSize = xHeightmap.rows;
	std::vector<uint8_t> xPixelData(iSize * iSize * 4);

	double dMin, dMax;
	cv::minMaxLoc(xHeightmap, &dMin, &dMax);
	double dRange = (dMax - dMin > 0.0001) ? (dMax - dMin) : 1.0;

	u_int uNumInvocations = std::min(static_cast<u_int>(64), static_cast<u_int>(iSize));

	// Pass 1: compute slopes in parallel
	std::vector<float> xSlopes(iSize * iSize);
	std::vector<float> xPerInvocationMaxSlope(uNumInvocations, 0.0f);

	SplatmapSlopeData xSlopeData;
	xSlopeData.pxHeightmap = &xHeightmap;
	xSlopeData.pfSlopes = xSlopes.data();
	xSlopeData.pfPerInvocationMaxSlope = xPerInvocationMaxSlope.data();
	xSlopeData.iSize = iSize;

	Zenith_TaskArray xSlopeTask(ZENITH_PROFILE_INDEX__FLUX_TERRAIN, ComputeSlopesTask, &xSlopeData, uNumInvocations, true);
	Zenith_TaskSystem::SubmitTaskArray(&xSlopeTask);
	xSlopeTask.WaitUntilComplete();

	// Reduce per-invocation max slopes
	float fMaxSlope = 0.0f;
	for (u_int i = 0; i < uNumInvocations; ++i)
		fMaxSlope = std::max(fMaxSlope, xPerInvocationMaxSlope[i]);
	if (fMaxSlope < 0.0001f) fMaxSlope = 1.0f;

	// Pass 2: generate weights in parallel
	SplatmapWeightData xWeightData;
	xWeightData.pxHeightmap = &xHeightmap;
	xWeightData.pfSlopes = xSlopes.data();
	xWeightData.puPixelData = xPixelData.data();
	xWeightData.iSize = iSize;
	xWeightData.dMin = dMin;
	xWeightData.dRange = dRange;
	xWeightData.fMaxSlope = fMaxSlope;

	Zenith_TaskArray xWeightTask(ZENITH_PROFILE_INDEX__FLUX_TERRAIN, GenerateWeightsTask, &xWeightData, uNumInvocations, true);
	Zenith_TaskSystem::SubmitTaskArray(&xWeightTask);
	xWeightTask.WaitUntilComplete();

	Zenith_DataStream xStream;
	xStream << static_cast<int32_t>(iSize);
	xStream << static_cast<int32_t>(iSize);
	xStream << static_cast<int32_t>(1);
	xStream << static_cast<TextureFormat>(TEXTURE_FORMAT_RGBA8_UNORM);
	xStream << static_cast<size_t>(xPixelData.size());
	xStream.WriteData(xPixelData.data(), xPixelData.size());
	xStream.WriteToFile(strOutputPath.c_str());

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Exported splatmap: %s (%dx%d)", strOutputPath.c_str(), iSize, iSize);
}
#endif

#ifdef ZENITH_TOOLS
/**
 * Generate procedural heightmap using multi-octave sine waves
 * This creates rolling hills terrain similar to Exploration_TerrainExplorer::GetTerrainHeightAt()
 *
 * @param uSize Image size (must be 4096 for terrain system)
 * @param fTerrainWorldSize World size the heightmap covers
 * @return 32-bit float cv::Mat heightmap (values 0.0-1.0)
 */
struct HeightmapGenData
{
	cv::Mat* pxHeightmap;
	uint32_t uSize;
	float fTerrainWorldSize;
};

static void GenerateHeightmapRowsTask(void* pData, u_int uInvocationIndex, u_int uNumInvocations)
{
	HeightmapGenData* pxData = static_cast<HeightmapGenData*>(pData);
	const uint32_t uSize = pxData->uSize;
	const float fTerrainWorldSize = pxData->fTerrainWorldSize;
	const float fMaxProceduralHeight = 100.0f;
	const float fSizeMinusOne = static_cast<float>(uSize - 1);

	u_int uRowsPerInvocation = (uSize + uNumInvocations - 1) / uNumInvocations;
	u_int uStartRow = uInvocationIndex * uRowsPerInvocation;
	u_int uEndRow = std::min(uStartRow + uRowsPerInvocation, static_cast<u_int>(uSize));

	for (uint32_t y = uStartRow; y < uEndRow; ++y)
	{
		float* pfRow = pxData->pxHeightmap->ptr<float>(y);
		float fWorldZ = (static_cast<float>(y) / fSizeMinusOne) * fTerrainWorldSize - fTerrainWorldSize * 0.5f;

		for (uint32_t x = 0; x < uSize; ++x)
		{
			float fWorldX = (static_cast<float>(x) / fSizeMinusOne) * fTerrainWorldSize - fTerrainWorldSize * 0.5f;

			float fHeight = 0.0f;
			fHeight += std::sin(fWorldX * 0.001f) * std::cos(fWorldZ * 0.001f) * 50.0f;
			fHeight += std::sin(fWorldX * 0.005f + 1.3f) * std::cos(fWorldZ * 0.005f + 0.7f) * 20.0f;
			fHeight += std::sin(fWorldX * 0.02f + 2.1f) * std::cos(fWorldZ * 0.02f + 1.4f) * 5.0f;
			fHeight += 30.0f;
			fHeight = std::max(0.0f, fHeight);

			pfRow[x] = std::clamp(fHeight / fMaxProceduralHeight, 0.0f, 1.0f);
		}
	}
}

static cv::Mat GenerateProceduralHeightmap(uint32_t uSize, float fTerrainWorldSize)
{
	cv::Mat xHeightmap(uSize, uSize, CV_32FC1);

	HeightmapGenData xData;
	xData.pxHeightmap = &xHeightmap;
	xData.uSize = uSize;
	xData.fTerrainWorldSize = fTerrainWorldSize;

	u_int uNumInvocations = std::min(static_cast<u_int>(64), uSize);
	Zenith_TaskArray xTask(ZENITH_PROFILE_INDEX__FLUX_TERRAIN, GenerateHeightmapRowsTask, &xData, uNumInvocations, true);
	Zenith_TaskSystem::SubmitTaskArray(&xTask);
	xTask.WaitUntilComplete();

	cv::flip(xHeightmap, xHeightmap, 0);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Generated procedural heightmap: %ux%u", uSize, uSize);
	return xHeightmap;
}

/**
 * Generate terrain data and export mesh files + all material textures + splatmap
 */
static bool GenerateAndExportTerrain()
{
	using namespace Flux_TerrainConfig;

	std::string strTerrainDir = std::string(GAME_ASSETS_DIR) + "Terrain/";
	std::string strHeightmapPath = strTerrainDir + "ExplorationHeightmap.tif";
	std::string strTexturesDir = strTerrainDir + "Textures/";

	// Create directories
	std::filesystem::create_directories(strTerrainDir);
	std::filesystem::create_directories(strTexturesDir);

	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Generating procedural terrain...");

	// Generate 4096x4096 heightmap (required size for terrain system)
	cv::Mat xHeightmap = GenerateProceduralHeightmap(4096, TERRAIN_SIZE);

	// Save heightmap as .tif file
	if (!cv::imwrite(strHeightmapPath, xHeightmap))
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] ERROR: Failed to save heightmap to %s", strHeightmapPath.c_str());
		return false;
	}
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Saved heightmap: %s", strHeightmapPath.c_str());

	// Export terrain meshes (HIGH, LOW, and physics for all chunks)
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Exporting terrain meshes (this may take a while)...");
	ExportHeightmapFromMat(xHeightmap, strTerrainDir);
	Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Terrain mesh export complete!");

	// Export all 4 material texture sets (5 PBR textures each = 20 files)
	// RM texture: R=unused, G=roughness, B=metallic (matches shader sampling .gb)
	ExportMaterialTextures(strTexturesDir, "Grass", 60, 130, 40, 230, 0);
	ExportMaterialTextures(strTexturesDir, "Rock", 120, 110, 100, 180, 0);
	ExportMaterialTextures(strTexturesDir, "Dirt", 110, 85, 60, 240, 0);
	ExportMaterialTextures(strTexturesDir, "Sand", 190, 180, 140, 240, 0);

	// Generate splatmap from heightmap
	GenerateProceduralSplatmap(xHeightmap, strTerrainDir + "Splatmap" ZENITH_TEXTURE_EXT);

	return true;
}
#endif // ZENITH_TOOLS

/**
 * Set all 5 PBR texture paths for a terrain material
 */
static void SetMaterialTexturePaths(Zenith_MaterialAsset* pxMat, const std::string& strDir, const std::string& strName)
{
	pxMat->SetDiffuseTexturePath(strDir + strName + "_Diffuse" ZENITH_TEXTURE_EXT);
	pxMat->SetNormalTexturePath(strDir + strName + "_Normal" ZENITH_TEXTURE_EXT);
	pxMat->SetRoughnessMetallicTexturePath(strDir + strName + "_RM" ZENITH_TEXTURE_EXT);
	pxMat->SetOcclusionTexturePath(strDir + strName + "_Occlusion" ZENITH_TEXTURE_EXT);
	pxMat->SetEmissiveTexturePath(strDir + strName + "_Emissive" ZENITH_TEXTURE_EXT);
}

/**
 * Initialize exploration resources (textures, materials)
 */
static void InitializeExplorationResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace Exploration;

	std::string strTexturesDir = std::string(GAME_ASSETS_DIR) + "Terrain/Textures/";

	static const char* aszNames[] = { "Grass", "Rock", "Dirt", "Sand" };
	static const char* aszDisplayNames[] = { "ExplorationTerrainGrass", "ExplorationTerrainRock", "ExplorationTerrainDirt", "ExplorationTerrainSand" };

	auto& xRegistry = Zenith_AssetRegistry::Get();
	for (u_int u = 0; u < 4; u++)
	{
		g_axTerrainMaterials[u].Set(xRegistry.Create<Zenith_MaterialAsset>());
		g_axTerrainMaterials[u].Get()->SetName(aszDisplayNames[u]);
		SetMaterialTexturePaths(g_axTerrainMaterials[u].Get(), strTexturesDir, aszNames[u]);
	}

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
	auto CreateTerrainEntity = [&]()
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Creating terrain entity...");

		Zenith_Entity xTerrainEntity(pxSceneData, "Terrain");
		xTerrainEntity.SetTransient(false);

		Zenith_TerrainComponent& xTerrain = xTerrainEntity.AddComponent<Zenith_TerrainComponent>(
			*g_axTerrainMaterials[0].Get(),
			*g_axTerrainMaterials[1].Get());

		// Set materials 2-3 and splatmap
		xTerrain.GetMaterialHandle(2).Set(g_axTerrainMaterials[2].Get());
		xTerrain.GetMaterialHandle(3).Set(g_axTerrainMaterials[3].Get());
		xTerrain.GetSplatmapHandle().SetPath(std::string(GAME_ASSETS_DIR) + "Terrain/Splatmap" ZENITH_TEXTURE_EXT);

		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] Terrain entity created successfully!");
	};

#ifdef ZENITH_TOOLS
	if (GenerateAndExportTerrain())
	{
		CreateTerrainEntity();
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TERRAIN, "[Exploration] WARNING: Failed to generate terrain, skipping terrain entity creation");
	}
#else
	std::string strTerrainDir = std::string(GAME_ASSETS_DIR) + "Terrain/";
	std::string strFirstChunk = strTerrainDir + "Render_0_0" ZENITH_MESH_EXT;
	if (std::filesystem::exists(strFirstChunk))
	{
		CreateTerrainEntity();
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

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
static void Exploration_GenerateTerrainDataWrapper()
{
	// Only generate terrain mesh files + textures + splatmap during automation.
	// Do NOT create terrain/tree entities here - they are created at runtime
	// by StartNewGame() in the World scene. Creating them here would cause
	// duplicate terrain entities (one saved in Exploration scene, one created
	// in World scene) leading to z-fighting.
	GenerateAndExportTerrain();
}

void Project_InitializeResources()
{
	// All resources initialized in Project_RegisterScriptBehaviours
}

void Project_RegisterEditorAutomationSteps()
{
	using namespace Flux_TerrainConfig;

	// Pre-compute camera start position
	static constexpr float fStartX = TERRAIN_SIZE * 0.5f;
	static constexpr float fStartZ = TERRAIN_SIZE * 0.5f;
	static constexpr float fStartY = 1200.0f;

	// ---- MainMenu scene (build index 0) ----
	Zenith_EditorAutomation::AddStep_CreateScene("MainMenu");
	Zenith_EditorAutomation::AddStep_CreateEntity("MenuManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(fStartX, fStartY, fStartZ);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.2f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(70.0f));
	Zenith_EditorAutomation::AddStep_SetCameraFar(10000.0f);
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIText("MenuTitle", "EXPLORATION");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuTitle", 48.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuTitle", 0.3f, 0.7f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_CreateUIButton("MenuPlay", "Play");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("Exploration_Behaviour");
	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- Exploration gameplay scene (build index 1) ----
	Zenith_EditorAutomation::AddStep_CreateScene("Exploration");
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(fStartX, fStartY, fStartZ);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.2f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(70.0f));
	Zenith_EditorAutomation::AddStep_SetCameraFar(10000.0f);
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	// HUD UI is created by Exploration_UIManager in OnStart
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("Exploration_Behaviour");
	// NOTE: Procedural world generation (terrain + vegetation) cannot be decomposed into
	// atomic editor steps. This is an intentional exception to the one-action-per-step rule.
	Zenith_EditorAutomation::AddStep_Custom(&Exploration_GenerateTerrainDataWrapper);
	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Exploration" ZENITH_SCENE_EXT);
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
	Zenith_SceneManager::RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Exploration" ZENITH_SCENE_EXT);
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
