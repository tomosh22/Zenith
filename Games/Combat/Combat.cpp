#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "Combat/Components/Combat_Behaviour.h"
#include "Combat/Components/Combat_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Physics/Zenith_Physics.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/Flux.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UIButton.h"

#include <cmath>
#include <filesystem>

// ============================================================================
// Combat Resources - Global access for behaviours
// ============================================================================
namespace Combat
{
	// Mesh geometry assets (registry-managed)
	Zenith_MeshGeometryAsset* g_pxCapsuleAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxCubeAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxConeAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxStickFigureGeometryAsset = nullptr;

	// Convenience pointers to underlying geometry (do not delete - managed by assets)
	Flux_MeshGeometry* g_pxCapsuleGeometry = nullptr;
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxConeGeometry = nullptr;
	Flux_MeshGeometry* g_pxStickFigureGeometry = nullptr;

	Zenith_ModelAsset* g_pxStickFigureModelAsset = nullptr;  // Model asset with skeleton for animated rendering
	std::string g_strStickFigureModelPath;  // Path to model asset file
	MaterialHandle g_xPlayerMaterial;
	MaterialHandle g_xEnemyMaterial;
	MaterialHandle g_xArenaMaterial;
	MaterialHandle g_xWallMaterial;
	MaterialHandle g_xCandleMaterial;  // Cream color for candles

	// Prefabs for runtime instantiation
	Zenith_Prefab* g_pxPlayerPrefab = nullptr;
	Zenith_Prefab* g_pxEnemyPrefab = nullptr;
	Zenith_Prefab* g_pxArenaPrefab = nullptr;
	Zenith_Prefab* g_pxArenaWallPrefab = nullptr;  // Wall segment with candle and flame

	// Particle effects
	Flux_ParticleEmitterConfig* g_pxHitSparkConfig = nullptr;
	Zenith_EntityID g_uHitSparkEmitterID = INVALID_ENTITY_ID;
	Flux_ParticleEmitterConfig* g_pxFlameConfig = nullptr;  // Candle flame particles
}

// Lazy initialization of stick figure model asset.
// Called from InitializeCombatResources and again from CreateArena if assets
// were not available at init time (unit tests create them after init).
void Combat::TryInitializeStickFigureModel()
{
	// Already initialized
	if (!g_strStickFigureModelPath.empty())
		return;

	std::string strStickFigureMeshGeomPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure.zmesh";
	std::string strStickFigureMeshAssetPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure.zasset";
	std::string strStickFigureSkeletonPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure.zskel";

	if (std::filesystem::exists(strStickFigureMeshAssetPath) && std::filesystem::exists(strStickFigureSkeletonPath))
	{
		// Load the mesh geometry through registry
		if (std::filesystem::exists(strStickFigureMeshGeomPath))
		{
			g_pxStickFigureGeometryAsset = Zenith_AssetRegistry::Get().Get<Zenith_MeshGeometryAsset>(strStickFigureMeshGeomPath);
			if (g_pxStickFigureGeometryAsset)
			{
				g_pxStickFigureGeometry = g_pxStickFigureGeometryAsset->GetGeometry();
				Zenith_Log(LOG_CATEGORY_MESH, "[Combat] Loaded stick figure mesh from %s", strStickFigureMeshGeomPath.c_str());
			}
		}

		// Create model asset via registry
		g_pxStickFigureModelAsset = Zenith_AssetRegistry::Get().Create<Zenith_ModelAsset>();
		g_pxStickFigureModelAsset->SetName("StickFigure");
		g_pxStickFigureModelAsset->SetSkeletonPath(strStickFigureSkeletonPath);

		Zenith_Vector<std::string> xEmptyMaterials;
		g_pxStickFigureModelAsset->AddMeshByPath(strStickFigureMeshAssetPath, xEmptyMaterials);

		// Export model asset
		g_strStickFigureModelPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure.zmodel";
		g_pxStickFigureModelAsset->Export(g_strStickFigureModelPath.c_str());
		Zenith_Log(LOG_CATEGORY_MESH, "[Combat] Created model asset at %s", g_strStickFigureModelPath.c_str());
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_MESH, "[Combat] Stick figure assets not found (.zasset=%s, .zskel=%s), using capsule fallback",
			std::filesystem::exists(strStickFigureMeshAssetPath) ? "exists" : "MISSING",
			std::filesystem::exists(strStickFigureSkeletonPath) ? "exists" : "MISSING");

		// Use capsule as fallback geometry only if not already set
		if (!g_pxStickFigureGeometry)
		{
			g_pxStickFigureGeometryAsset = g_pxCapsuleAsset;
			g_pxStickFigureGeometry = g_pxCapsuleGeometry;
		}
	}
}

static bool s_bResourcesInitialized = false;

// ============================================================================
// Resource Cleanup (called at shutdown)
// ============================================================================
static void CleanupCombatResources()
{
	using namespace Combat;

	if (!s_bResourcesInitialized)
		return;

	// Delete particle configs
	delete g_pxHitSparkConfig;
	g_pxHitSparkConfig = nullptr;
	g_uHitSparkEmitterID = INVALID_ENTITY_ID;

	delete g_pxFlameConfig;
	g_pxFlameConfig = nullptr;

	// Delete prefabs
	delete g_pxPlayerPrefab;
	g_pxPlayerPrefab = nullptr;
	delete g_pxEnemyPrefab;
	g_pxEnemyPrefab = nullptr;
	delete g_pxArenaPrefab;
	g_pxArenaPrefab = nullptr;
	delete g_pxArenaWallPrefab;
	g_pxArenaWallPrefab = nullptr;

	// Clear model asset pointer - registry manages lifetime
	g_pxStickFigureModelAsset = nullptr;

	// Clear mesh geometry pointers - registry manages asset lifetime
	g_pxStickFigureGeometry = nullptr;
	g_pxCapsuleGeometry = nullptr;
	g_pxCubeGeometry = nullptr;
	g_pxConeGeometry = nullptr;

	g_pxStickFigureGeometryAsset = nullptr;
	g_pxCapsuleAsset = nullptr;
	g_pxCubeAsset = nullptr;
	g_pxConeAsset = nullptr;

	// Note: Textures and materials are managed by Zenith_AssetRegistry

	s_bResourcesInitialized = false;
	Zenith_Log(LOG_CATEGORY_ASSET, "[Combat] Resources cleaned up");
}

// ============================================================================
// Procedural Texture Generation
// ============================================================================

// Export a 1x1 colored texture to disk and return a TextureHandle with its path
static TextureHandle ExportColoredTexture(const std::string& strPath, uint8_t uR, uint8_t uG, uint8_t uB)
{
	// Create texture data
	uint8_t aucPixelData[] = { uR, uG, uB, 255 };

	// Write to .ztex file format (same as Zenith_Tools_TextureExport::ExportFromData)
	Zenith_DataStream xStream;
	xStream << (int32_t)1;  // width
	xStream << (int32_t)1;  // height
	xStream << (int32_t)1;  // depth
	xStream << (TextureFormat)TEXTURE_FORMAT_RGBA8_UNORM;
	xStream << (size_t)4;   // data size (1x1x4 bytes)
	xStream.WriteData(aucPixelData, 4);
	xStream.WriteToFile(strPath.c_str());

	// Convert absolute path to prefixed relative path for portability
	std::string strRelativePath = Zenith_AssetRegistry::MakeRelativePath(strPath);
	if (strRelativePath.empty())
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "[Combat] Failed to make relative path for texture: %s", strPath.c_str());
		return TextureHandle();
	}

	// Create TextureHandle with the prefixed path
	return TextureHandle(strRelativePath);
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
// Procedural Cone Geometry Generation (for candles)
// ============================================================================
static void GenerateCone(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices)
{
	// A cone has vertices around the base, plus the apex and base center
	uint32_t uNumVerts = uSlices + 2;  // Base ring + apex + base center
	uint32_t uNumIndices = uSlices * 6;  // Side triangles + base triangles

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;
	xGeometryOut.m_pxPositions = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxNormals = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxUVs = new Zenith_Maths::Vector2[uNumVerts];
	xGeometryOut.m_pxTangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxBitangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxColors = new Zenith_Maths::Vector4[uNumVerts];
	xGeometryOut.m_puIndices = new Flux_MeshGeometry::IndexType[uNumIndices];

	// Generate base ring vertices (indices 0 to uSlices-1)
	for (uint32_t i = 0; i < uSlices; i++)
	{
		float fTheta = static_cast<float>(i) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
		float fX = cos(fTheta) * fRadius;
		float fZ = sin(fTheta) * fRadius;

		xGeometryOut.m_pxPositions[i] = Zenith_Maths::Vector3(fX, 0.0f, fZ);

		// Normal points outward and slightly up
		float fNormalY = fRadius / fHeight;
		Zenith_Maths::Vector3 xNormal = glm::normalize(Zenith_Maths::Vector3(cos(fTheta), fNormalY, sin(fTheta)));
		xGeometryOut.m_pxNormals[i] = xNormal;

		xGeometryOut.m_pxUVs[i] = Zenith_Maths::Vector2(
			static_cast<float>(i) / static_cast<float>(uSlices),
			0.0f
		);

		xGeometryOut.m_pxTangents[i] = Zenith_Maths::Vector3(-sin(fTheta), 0.0f, cos(fTheta));
		xGeometryOut.m_pxBitangents[i] = glm::cross(xNormal, xGeometryOut.m_pxTangents[i]);
		xGeometryOut.m_pxColors[i] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// Apex vertex (index uSlices)
	uint32_t uApexIdx = uSlices;
	xGeometryOut.m_pxPositions[uApexIdx] = Zenith_Maths::Vector3(0.0f, fHeight, 0.0f);
	xGeometryOut.m_pxNormals[uApexIdx] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	xGeometryOut.m_pxUVs[uApexIdx] = Zenith_Maths::Vector2(0.5f, 1.0f);
	xGeometryOut.m_pxTangents[uApexIdx] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	xGeometryOut.m_pxBitangents[uApexIdx] = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	xGeometryOut.m_pxColors[uApexIdx] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	// Base center vertex (index uSlices+1)
	uint32_t uBaseCenterIdx = uSlices + 1;
	xGeometryOut.m_pxPositions[uBaseCenterIdx] = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	xGeometryOut.m_pxNormals[uBaseCenterIdx] = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
	xGeometryOut.m_pxUVs[uBaseCenterIdx] = Zenith_Maths::Vector2(0.5f, 0.5f);
	xGeometryOut.m_pxTangents[uBaseCenterIdx] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	xGeometryOut.m_pxBitangents[uBaseCenterIdx] = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	xGeometryOut.m_pxColors[uBaseCenterIdx] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	// Generate indices
	uint32_t uIdxIdx = 0;

	// Side triangles (connect base ring to apex)
	for (uint32_t i = 0; i < uSlices; i++)
	{
		uint32_t uNext = (i + 1) % uSlices;

		// Side triangle (counter-clockwise for Vulkan when viewed from outside)
		xGeometryOut.m_puIndices[uIdxIdx++] = i;
		xGeometryOut.m_puIndices[uIdxIdx++] = uApexIdx;
		xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
	}

	// Base triangles (connect base ring to center)
	for (uint32_t i = 0; i < uSlices; i++)
	{
		uint32_t uNext = (i + 1) % uSlices;

		// Base triangle (counter-clockwise when viewed from below)
		xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
		xGeometryOut.m_puIndices[uIdxIdx++] = uBaseCenterIdx;
		xGeometryOut.m_puIndices[uIdxIdx++] = i;
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

	// Create directory for procedural meshes
	std::string strMeshDir = std::string(GAME_ASSETS_DIR) + "/Meshes";
	std::filesystem::create_directories(strMeshDir);

	// Create capsule geometry (for characters) - custom size, tracked through registry
	g_pxCapsuleAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>();
	Flux_MeshGeometry* pxCapsule = new Flux_MeshGeometry();
	GenerateCapsule(*pxCapsule, 0.5f, 1.0f, 16, 16);
	g_pxCapsuleAsset->SetGeometry(pxCapsule);
	g_pxCapsuleGeometry = g_pxCapsuleAsset->GetGeometry();
#ifdef ZENITH_TOOLS
	std::string strCapsulePath = strMeshDir + "/Capsule.zmesh";
	g_pxCapsuleGeometry->Export(strCapsulePath.c_str());
	g_pxCapsuleGeometry->m_strSourcePath = strCapsulePath;
#endif

	// Create cube geometry (for arena) - use registry's cached unit cube
	g_pxCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_pxCubeGeometry = g_pxCubeAsset->GetGeometry();
#ifdef ZENITH_TOOLS
	std::string strCubePath = strMeshDir + "/Cube.zmesh";
	g_pxCubeGeometry->Export(strCubePath.c_str());
	g_pxCubeGeometry->m_strSourcePath = strCubePath;
#endif

	// Create cone geometry (for candles on walls) - custom size, tracked through registry
	g_pxConeAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>();
	Flux_MeshGeometry* pxCone = new Flux_MeshGeometry();
	GenerateCone(*pxCone, 0.08f, 0.25f, 12);
	g_pxConeAsset->SetGeometry(pxCone);
	g_pxConeGeometry = g_pxConeAsset->GetGeometry();
#ifdef ZENITH_TOOLS
	std::string strConePath = strMeshDir + "/Cone.zmesh";
	g_pxConeGeometry->Export(strConePath.c_str());
	g_pxConeGeometry->m_strSourcePath = strConePath;
#endif

	// Try to load stick figure assets (may not exist yet on first run - unit tests create them)
	Combat::TryInitializeStickFigureModel();

	// Create textures directory
	std::string strTexturesDir = std::string(GAME_ASSETS_DIR) + "/Textures";
	std::filesystem::create_directories(strTexturesDir);

	// Export procedural textures to disk and get TextureHandles
	// SSR VERIFICATION: Using bright distinctive colors for walls and player
	TextureHandle xPlayerTextureHandle = ExportColoredTexture(strTexturesDir + "/Player.ztex", 0, 255, 255);      // CYAN player for SSR detection
	TextureHandle xEnemyTextureHandle = ExportColoredTexture(strTexturesDir + "/Enemy.ztex", 204, 51, 51);        // Red enemies
	TextureHandle xArenaTextureHandle = ExportColoredTexture(strTexturesDir + "/Arena.ztex", 77, 77, 89);         // Gray arena floor
	TextureHandle xWallTextureHandle = ExportColoredTexture(strTexturesDir + "/Wall.ztex", 255, 0, 255);          // MAGENTA walls for SSR detection
	TextureHandle xCandleTextureHandle = ExportColoredTexture(strTexturesDir + "/Candle.ztex", 240, 220, 180);    // Cream candle color

	// Create materials with texture paths (properly serializable)
	auto& xRegistry = Zenith_AssetRegistry::Get();

	g_xPlayerMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xPlayerMaterial.Get()->SetName("CombatPlayer");
	g_xPlayerMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Player.ztex");
	g_xPlayerMaterial.Get()->SetRoughness(0.9f);  // HIGH roughness - player should be REFLECTED, not reflecting

	g_xEnemyMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xEnemyMaterial.Get()->SetName("CombatEnemy");
	g_xEnemyMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Enemy.ztex");
	g_xEnemyMaterial.Get()->SetRoughness(0.9f);  // HIGH roughness - enemies should be REFLECTED, not reflecting

	g_xArenaMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xArenaMaterial.Get()->SetName("CombatArena");
	g_xArenaMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Arena.ztex");
	g_xArenaMaterial.Get()->SetRoughness(0.15f);  // LOW roughness - floor IS the reflective surface

	g_xWallMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xWallMaterial.Get()->SetName("CombatWall");
	g_xWallMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Wall.ztex");
	g_xWallMaterial.Get()->SetRoughness(0.9f);  // HIGH roughness - walls should be REFLECTED, not reflecting

	g_xCandleMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xCandleMaterial.Get()->SetName("CombatCandle");
	g_xCandleMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Candle.ztex");
	g_xCandleMaterial.Get()->SetRoughness(0.9f);  // HIGH roughness - candles should be REFLECTED, not reflecting

	// Create flame particle config for wall candles
	g_pxFlameConfig = new Flux_ParticleEmitterConfig();
	g_pxFlameConfig->m_fSpawnRate = 30.0f;                    // Dense flame
	g_pxFlameConfig->m_uBurstCount = 0;
	g_pxFlameConfig->m_uMaxParticles = 64;                    // More particles per candle
	g_pxFlameConfig->m_fSpawnRadius = 0.05f;                  // Slight position variation
	g_pxFlameConfig->m_fLifetimeMin = 0.4f;
	g_pxFlameConfig->m_fLifetimeMax = 0.9f;
	g_pxFlameConfig->m_fSpeedMin = 0.3f;
	g_pxFlameConfig->m_fSpeedMax = 1.0f;
	g_pxFlameConfig->m_fSpreadAngleDegrees = 25.0f;           // Wider spread
	g_pxFlameConfig->m_xEmitDirection = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	g_pxFlameConfig->m_xGravity = Zenith_Maths::Vector3(0.0f, 0.8f, 0.0f);  // Strong updraft
	g_pxFlameConfig->m_fDrag = 0.8f;
	g_pxFlameConfig->m_xColorStart = Zenith_Maths::Vector4(1.0f, 0.9f, 0.3f, 0.9f);  // Bright yellow
	g_pxFlameConfig->m_xColorEnd = Zenith_Maths::Vector4(1.0f, 0.2f, 0.0f, 0.0f);    // Red->transparent
	g_pxFlameConfig->m_fSizeStart = 0.15f;
	g_pxFlameConfig->m_fSizeEnd = 0.04f;
	g_pxFlameConfig->m_bAdditiveBlending = true;              // Glow effect
	g_pxFlameConfig->m_fTurbulence = 1.5f;                    // Flickering motion
	g_pxFlameConfig->m_bUseGPUCompute = false;
	Flux_ParticleEmitterConfig::Register("Combat_Flame", g_pxFlameConfig);

	// Create prefabs for runtime instantiation
	// Note: Prefabs are lightweight templates - components added after transform is set
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Player prefab
	{
		Zenith_Entity xPlayerTemplate(pxSceneData, "PlayerTemplate");
		g_pxPlayerPrefab = new Zenith_Prefab();
		g_pxPlayerPrefab->CreateFromEntity(xPlayerTemplate, "Player");
		Zenith_SceneManager::Destroy(xPlayerTemplate);
	}

	// Enemy prefab
	{
		Zenith_Entity xEnemyTemplate(pxSceneData, "EnemyTemplate");
		g_pxEnemyPrefab = new Zenith_Prefab();
		g_pxEnemyPrefab->CreateFromEntity(xEnemyTemplate, "Enemy");
		Zenith_SceneManager::Destroy(xEnemyTemplate);
	}

	// Arena prefab (for floor)
	{
		Zenith_Entity xArenaTemplate(pxSceneData, "ArenaTemplate");
		g_pxArenaPrefab = new Zenith_Prefab();
		g_pxArenaPrefab->CreateFromEntity(xArenaTemplate, "Arena");
		Zenith_SceneManager::Destroy(xArenaTemplate);
	}

	// ArenaWall prefab with collider and particle emitter
	// NOTE: ModelComponent is NOT included because mesh/material pointers don't serialize.
	// ModelComponent is added after instantiation in CreateArena().
	{
		Zenith_Entity xWallTemplate(pxSceneData, "ArenaWallTemplate");

		// Add ColliderComponent for wall collision
		xWallTemplate.AddComponent<Zenith_ColliderComponent>()
			.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

		// Add ParticleEmitterComponent for candle flame
		Zenith_ParticleEmitterComponent& xEmitter = xWallTemplate.AddComponent<Zenith_ParticleEmitterComponent>();
		xEmitter.SetConfig(g_pxFlameConfig);
		xEmitter.SetEmitting(true);

		g_pxArenaWallPrefab = new Zenith_Prefab();
		g_pxArenaWallPrefab->CreateFromEntity(xWallTemplate, "ArenaWall");
		Zenith_SceneManager::Destroy(xWallTemplate);
	}

	// Create hit spark particle config
	g_pxHitSparkConfig = new Flux_ParticleEmitterConfig();
	g_pxHitSparkConfig->m_uBurstCount = 20;
	g_pxHitSparkConfig->m_fSpawnRate = 0.0f;
	g_pxHitSparkConfig->m_uMaxParticles = 256;
	g_pxHitSparkConfig->m_fLifetimeMin = 0.2f;
	g_pxHitSparkConfig->m_fLifetimeMax = 0.4f;
	g_pxHitSparkConfig->m_fSpeedMin = 8.0f;
	g_pxHitSparkConfig->m_fSpeedMax = 15.0f;
	g_pxHitSparkConfig->m_fSpreadAngleDegrees = 60.0f;
	g_pxHitSparkConfig->m_xGravity = Zenith_Maths::Vector3(0.0f, -5.0f, 0.0f);
	g_pxHitSparkConfig->m_fDrag = 2.0f;
	g_pxHitSparkConfig->m_xColorStart = Zenith_Maths::Vector4(1.0f, 0.6f, 0.1f, 1.0f);
	g_pxHitSparkConfig->m_xColorEnd = Zenith_Maths::Vector4(1.0f, 1.0f, 0.2f, 0.0f);
	g_pxHitSparkConfig->m_fSizeStart = 0.3f;
	g_pxHitSparkConfig->m_fSizeEnd = 0.1f;
	g_pxHitSparkConfig->m_bUseGPUCompute = false;
	Flux_ParticleEmitterConfig::Register("Combat_HitSpark", g_pxHitSparkConfig);

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

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeCombatResources();

	// Register behaviors
	Combat_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	CleanupCombatResources();
}

void Project_CreateScenes()
{
	// ---- MainMenu scene (build index 0) ----
	{
		const std::string strMenuPath = GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT;

		Zenith_Scene xMenuScene = Zenith_SceneManager::CreateEmptyScene("MainMenu");
		Zenith_SceneData* pxMenuData = Zenith_SceneManager::GetSceneData(xMenuScene);

		Zenith_Entity xMenuManager(pxMenuData, "GameManager");
		xMenuManager.SetTransient(false);

		Zenith_CameraComponent& xCamera = xMenuManager.AddComponent<Zenith_CameraComponent>();
		xCamera.InitialisePerspective({
			.m_xPosition = Zenith_Maths::Vector3(0.0f, 12.0f, -15.0f),
			.m_fPitch = -0.7f,
			.m_fFOV = glm::radians(50.0f),
		});
		pxMenuData->SetMainCameraEntity(xMenuManager.GetEntityID());

		Zenith_UIComponent& xUI = xMenuManager.AddComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIText* pxMenuTitle = xUI.CreateText("MenuTitle", "COMBAT ARENA");
		pxMenuTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxMenuTitle->SetPosition(0.0f, -120.0f);
		pxMenuTitle->SetFontSize(15.0f * 4.8f);
		pxMenuTitle->SetColor(Zenith_Maths::Vector4(1.0f, 0.2f, 0.2f, 1.0f));

		Zenith_UI::Zenith_UIButton* pxPlayButton = xUI.CreateButton("MenuPlay", "Play");
		pxPlayButton->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxPlayButton->SetPosition(0.0f, 0.0f);
		pxPlayButton->SetSize(200.0f, 50.0f);

		Zenith_ScriptComponent& xScript = xMenuManager.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviourForSerialization<Combat_Behaviour>();

		pxMenuData->SaveToFile(strMenuPath);
		Zenith_SceneManager::RegisterSceneBuildIndex(0, strMenuPath);
		Zenith_SceneManager::UnloadScene(xMenuScene);
	}

	// ---- Arena gameplay scene (build index 1) ----
	{
		const std::string strArenaPath = GAME_ASSETS_DIR "Scenes/Arena" ZENITH_SCENE_EXT;

		Zenith_Scene xArenaScene = Zenith_SceneManager::CreateEmptyScene("Arena");
		Zenith_SceneData* pxArenaData = Zenith_SceneManager::GetSceneData(xArenaScene);

		Zenith_Entity xGameManager(pxArenaData, "GameManager");
		xGameManager.SetTransient(false);

		Zenith_CameraComponent& xCamera = xGameManager.AddComponent<Zenith_CameraComponent>();
		xCamera.InitialisePerspective({
			.m_xPosition = Zenith_Maths::Vector3(0.0f, 12.0f, -15.0f),
			.m_fPitch = -0.7f,
			.m_fFOV = glm::radians(50.0f),
		});
		pxArenaData->SetMainCameraEntity(xGameManager.GetEntityID());

		static constexpr float s_fMarginLeft = 30.0f;
		static constexpr float s_fMarginTop = 30.0f;
		static constexpr float s_fBaseTextSize = 15.0f;
		static constexpr float s_fLineHeight = 24.0f;

		Zenith_UIComponent& xUI = xGameManager.AddComponent<Zenith_UIComponent>();

		auto CreateHUDText = [&](const char* szName, const char* szText,
			Zenith_UI::AnchorPreset eAnchor, float fX, float fY, float fSize,
			Zenith_Maths::Vector4 xColor,
			Zenith_UI::TextAlignment eAlign = Zenith_UI::TextAlignment::Left)
		{
			Zenith_UI::Zenith_UIText* pxText = xUI.CreateText(szName, szText);
			pxText->SetAnchorAndPivot(eAnchor);
			pxText->SetPosition(fX, fY);
			pxText->SetFontSize(fSize);
			pxText->SetColor(xColor);
			pxText->SetAlignment(eAlign);
			pxText->SetVisible(false);
		};

		CreateHUDText("PlayerHealth", "Health: 100 / 100",
			Zenith_UI::AnchorPreset::TopLeft, s_fMarginLeft, s_fMarginTop + s_fLineHeight * 3,
			s_fBaseTextSize * 3.0f, {0.2f, 1.0f, 0.2f, 1.0f});

		CreateHUDText("PlayerHealthBar", "[||||||||||||||||||||]",
			Zenith_UI::AnchorPreset::TopLeft, s_fMarginLeft, s_fMarginTop + s_fLineHeight * 4,
			s_fBaseTextSize * 2.5f, {0.2f, 1.0f, 0.2f, 1.0f});

		CreateHUDText("EnemyCount", "Enemies: 3 / 3",
			Zenith_UI::AnchorPreset::TopLeft, s_fMarginLeft, s_fMarginTop + s_fLineHeight * 6,
			s_fBaseTextSize * 3.0f, {0.8f, 0.8f, 0.8f, 1.0f});

		CreateHUDText("ComboCount", "",
			Zenith_UI::AnchorPreset::Center, 0.0f, -100.0f,
			s_fBaseTextSize * 8.0f, {1.0f, 0.8f, 0.2f, 1.0f}, Zenith_UI::TextAlignment::Center);

		CreateHUDText("ComboText", "",
			Zenith_UI::AnchorPreset::Center, 0.0f, -60.0f,
			s_fBaseTextSize * 4.0f, {1.0f, 0.8f, 0.2f, 1.0f}, Zenith_UI::TextAlignment::Center);

		CreateHUDText("Controls", "WASD: Move | LMB: Attack | RMB: Heavy | Space: Dodge | R: Reset | Esc: Menu",
			Zenith_UI::AnchorPreset::BottomLeft, s_fMarginLeft, s_fMarginTop,
			s_fBaseTextSize * 2.5f, {0.7f, 0.7f, 0.7f, 1.0f});

		CreateHUDText("Status", "",
			Zenith_UI::AnchorPreset::Center, 0.0f, 0.0f,
			s_fBaseTextSize * 8.0f, {0.2f, 1.0f, 0.2f, 1.0f}, Zenith_UI::TextAlignment::Center);

		Zenith_ScriptComponent& xScript = xGameManager.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviourForSerialization<Combat_Behaviour>();

		pxArenaData->SaveToFile(strArenaPath);
		Zenith_SceneManager::RegisterSceneBuildIndex(1, strArenaPath);
		Zenith_SceneManager::UnloadScene(xArenaScene);
	}
}

void Project_LoadInitialScene()
{
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
