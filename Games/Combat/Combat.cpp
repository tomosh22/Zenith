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
#include "ZenithECS/Zenith_EventSystem.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
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

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#endif

// ============================================================================
// Combat Resources - Global access for behaviours
// ============================================================================
namespace Combat
{
	static CombatResources g_xResources;
	CombatResources& Resources() { return g_xResources; }

	const char* g_aszEnemyVariantNames[uENEMY_VARIANT_COUNT] = { "EnemyWeak", "EnemyNormal", "EnemyStrong" };
	const float g_afEnemyVariantScales[uENEMY_VARIANT_COUNT] = { 0.7f, 0.9f, 1.1f };
}

// Lazy initialization of stick figure model asset.
// Called from InitializeCombatResources and again from CreateArena if assets
// were not available at init time (unit tests create them after init).
void Combat::TryInitializeStickFigureModel()
{
	// Already initialized
	if (!Resources().m_strStickFigureModelPath.empty())
		return;

	std::string strStickFigureMeshGeomPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MESH_EXT;
	std::string strStickFigureMeshAssetPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MESH_ASSET_EXT;
	std::string strStickFigureSkeletonPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_SKELETON_EXT;

	if (std::filesystem::exists(strStickFigureMeshAssetPath) && std::filesystem::exists(strStickFigureSkeletonPath))
	{
		// Load the mesh geometry through registry
		if (std::filesystem::exists(strStickFigureMeshGeomPath))
		{
			if (Zenith_MeshGeometryAsset* pxGeom = Zenith_AssetRegistry::Get<Zenith_MeshGeometryAsset>(strStickFigureMeshGeomPath))
			{
				Resources().m_xStickFigureGeometryAsset.Set(pxGeom);
				Resources().m_pxStickFigureGeometry = pxGeom->GetGeometry();
				Zenith_Log(LOG_CATEGORY_MESH, "[Combat] Loaded stick figure mesh from %s", strStickFigureMeshGeomPath.c_str());
			}
		}

		// Create model asset via registry
		Zenith_ModelAsset* pxModel = Zenith_AssetRegistry::Create<Zenith_ModelAsset>();
		pxModel->SetName("StickFigure");
		pxModel->SetSkeletonPath(strStickFigureSkeletonPath);

		Zenith_Vector<std::string> xEmptyMaterials;
		pxModel->AddMeshByPath(strStickFigureMeshAssetPath, xEmptyMaterials);

		// Export model asset
		Resources().m_strStickFigureModelPath = std::string(ENGINE_ASSETS_DIR) + "Meshes/StickFigure/StickFigure" ZENITH_MODEL_EXT;
		pxModel->Export(Resources().m_strStickFigureModelPath.c_str());
		Resources().m_xStickFigureModelAsset.Set(pxModel);
		Zenith_Log(LOG_CATEGORY_MESH, "[Combat] Created model asset at %s", Resources().m_strStickFigureModelPath.c_str());
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_MESH, "[Combat] Stick figure assets not found (.zasset=%s, .zskel=%s), using capsule fallback",
			std::filesystem::exists(strStickFigureMeshAssetPath) ? "exists" : "MISSING",
			std::filesystem::exists(strStickFigureSkeletonPath) ? "exists" : "MISSING");

		// Use capsule as fallback geometry only if not already set
		if (!Resources().m_pxStickFigureGeometry)
		{
			Resources().m_xStickFigureGeometryAsset = Resources().m_xCapsuleAsset;
			Resources().m_pxStickFigureGeometry = Resources().m_pxCapsuleGeometry;
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
	delete Resources().m_pxHitSparkConfig;
	Resources().m_pxHitSparkConfig = nullptr;
	Resources().m_uHitSparkEmitterID = INVALID_ENTITY_ID;

	delete Resources().m_pxFlameConfig;
	Resources().m_pxFlameConfig = nullptr;

	// Drop prefab handle refs (registry now owns these and deletes them on its own teardown)
	Resources().m_xPlayerPrefab.Clear();
	Resources().m_xEnemyPrefab.Clear();
	for (u_int u = 0; u < uENEMY_VARIANT_COUNT; ++u)
	{
		Resources().m_axEnemyVariants[u].Clear();
	}
	Resources().m_xArenaPrefab.Clear();
	Resources().m_xArenaWallPrefab.Clear();

	// Drop model + mesh-geometry handle refs
	Resources().m_xStickFigureModelAsset.Clear();
	Resources().m_xCapsuleAsset.Clear();
	Resources().m_xCubeAsset.Clear();
	Resources().m_xConeAsset.Clear();
	Resources().m_xStickFigureGeometryAsset.Clear();

	// Clear material handles
	Resources().m_xPlayerMaterial.Clear();
	Resources().m_xEnemyMaterial.Clear();
	Resources().m_xArenaMaterial.Clear();
	Resources().m_xWallMaterial.Clear();
	Resources().m_xCandleMaterial.Clear();

	// Clear convenience geometry pointers (handles already cleared above own the lifetime)
	Resources().m_pxStickFigureGeometry = nullptr;
	Resources().m_pxCapsuleGeometry = nullptr;
	Resources().m_pxCubeGeometry = nullptr;
	Resources().m_pxConeGeometry = nullptr;

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

	// Write to .ztxtr file format (same as Zenith_Tools_TextureExport::ExportFromData)
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
	g_xEngine.VulkanMemory().InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	g_xEngine.VulkanMemory().InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
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
	g_xEngine.VulkanMemory().InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	g_xEngine.VulkanMemory().InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
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
	{
		Zenith_MeshGeometryAsset* pxCapsuleAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Flux_MeshGeometry* pxCapsule = new Flux_MeshGeometry();
		GenerateCapsule(*pxCapsule, 0.5f, 1.0f, 16, 16);
		pxCapsuleAsset->SetGeometry(pxCapsule);
		Resources().m_xCapsuleAsset.Set(pxCapsuleAsset);
		Resources().m_pxCapsuleGeometry = pxCapsuleAsset->GetGeometry();
	}
#ifdef ZENITH_TOOLS
	std::string strCapsulePath = strMeshDir + "/Capsule" ZENITH_MESH_EXT;
	Resources().m_pxCapsuleGeometry->Export(strCapsulePath.c_str());
	Resources().m_pxCapsuleGeometry->m_strSourcePath = strCapsulePath;
#endif

	// Create cube geometry (for arena) - use registry's cached unit cube
	Resources().m_xCubeAsset.Set(Zenith_MeshGeometryAsset::CreateUnitCube());
	Resources().m_pxCubeGeometry = Resources().m_xCubeAsset.GetDirect()->GetGeometry();
#ifdef ZENITH_TOOLS
	std::string strCubePath = strMeshDir + "/Cube" ZENITH_MESH_EXT;
	Resources().m_pxCubeGeometry->Export(strCubePath.c_str());
	Resources().m_pxCubeGeometry->m_strSourcePath = strCubePath;
#endif

	// Create cone geometry (for candles on walls) - custom size, tracked through registry
	{
		Zenith_MeshGeometryAsset* pxConeAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Flux_MeshGeometry* pxCone = new Flux_MeshGeometry();
		GenerateCone(*pxCone, 0.08f, 0.25f, 12);
		pxConeAsset->SetGeometry(pxCone);
		Resources().m_xConeAsset.Set(pxConeAsset);
		Resources().m_pxConeGeometry = pxConeAsset->GetGeometry();
	}
#ifdef ZENITH_TOOLS
	std::string strConePath = strMeshDir + "/Cone" ZENITH_MESH_EXT;
	Resources().m_pxConeGeometry->Export(strConePath.c_str());
	Resources().m_pxConeGeometry->m_strSourcePath = strConePath;
#endif

	// Try to load stick figure assets (may not exist yet on first run - unit tests create them)
	Combat::TryInitializeStickFigureModel();

	// Create textures directory
	std::string strTexturesDir = std::string(GAME_ASSETS_DIR) + "/Textures";
	std::filesystem::create_directories(strTexturesDir);

	// Export procedural textures to disk and get TextureHandles
	// SSR VERIFICATION: Using bright distinctive colors for walls and player
	TextureHandle xPlayerTextureHandle = ExportColoredTexture(strTexturesDir + "/Player" ZENITH_TEXTURE_EXT, 0, 255, 255);      // CYAN player for SSR detection
	TextureHandle xEnemyTextureHandle = ExportColoredTexture(strTexturesDir + "/Enemy" ZENITH_TEXTURE_EXT, 204, 51, 51);        // Red enemies
	TextureHandle xArenaTextureHandle = ExportColoredTexture(strTexturesDir + "/Arena" ZENITH_TEXTURE_EXT, 77, 77, 89);         // Gray arena floor
	TextureHandle xWallTextureHandle = ExportColoredTexture(strTexturesDir + "/Wall" ZENITH_TEXTURE_EXT, 255, 0, 255);          // MAGENTA walls for SSR detection
	TextureHandle xCandleTextureHandle = ExportColoredTexture(strTexturesDir + "/Candle" ZENITH_TEXTURE_EXT, 240, 220, 180);    // Cream candle color

	// Create materials with texture paths (properly serializable)

	Resources().m_xPlayerMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xPlayerMaterial.GetDirect()->SetName("CombatPlayer");
	Resources().m_xPlayerMaterial.GetDirect()->SetDiffuseTexture(xPlayerTextureHandle);
	Resources().m_xPlayerMaterial.GetDirect()->SetRoughness(0.9f);  // HIGH roughness - player should be REFLECTED, not reflecting

	Resources().m_xEnemyMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xEnemyMaterial.GetDirect()->SetName("CombatEnemy");
	Resources().m_xEnemyMaterial.GetDirect()->SetDiffuseTexture(xEnemyTextureHandle);
	Resources().m_xEnemyMaterial.GetDirect()->SetRoughness(0.9f);  // HIGH roughness - enemies should be REFLECTED, not reflecting

	Resources().m_xArenaMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xArenaMaterial.GetDirect()->SetName("CombatArena");
	Resources().m_xArenaMaterial.GetDirect()->SetDiffuseTexture(xArenaTextureHandle);
	Resources().m_xArenaMaterial.GetDirect()->SetRoughness(0.15f);  // LOW roughness - floor IS the reflective surface

	Resources().m_xWallMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xWallMaterial.GetDirect()->SetName("CombatWall");
	Resources().m_xWallMaterial.GetDirect()->SetDiffuseTexture(xWallTextureHandle);
	Resources().m_xWallMaterial.GetDirect()->SetRoughness(0.9f);  // HIGH roughness - walls should be REFLECTED, not reflecting

	Resources().m_xCandleMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xCandleMaterial.GetDirect()->SetName("CombatCandle");
	Resources().m_xCandleMaterial.GetDirect()->SetDiffuseTexture(xCandleTextureHandle);
	Resources().m_xCandleMaterial.GetDirect()->SetRoughness(0.9f);  // HIGH roughness - candles should be REFLECTED, not reflecting

	// Create flame particle config for wall candles
	Resources().m_pxFlameConfig = new Flux_ParticleEmitterConfig();
	Resources().m_pxFlameConfig->m_fSpawnRate = 30.0f;                    // Dense flame
	Resources().m_pxFlameConfig->m_uBurstCount = 0;
	Resources().m_pxFlameConfig->m_uMaxParticles = 64;                    // More particles per candle
	Resources().m_pxFlameConfig->m_fSpawnRadius = 0.05f;                  // Slight position variation
	Resources().m_pxFlameConfig->m_fLifetimeMin = 0.4f;
	Resources().m_pxFlameConfig->m_fLifetimeMax = 0.9f;
	Resources().m_pxFlameConfig->m_fSpeedMin = 0.3f;
	Resources().m_pxFlameConfig->m_fSpeedMax = 1.0f;
	Resources().m_pxFlameConfig->m_fSpreadAngleDegrees = 25.0f;           // Wider spread
	Resources().m_pxFlameConfig->m_xEmitDirection = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	Resources().m_pxFlameConfig->m_xGravity = Zenith_Maths::Vector3(0.0f, 0.8f, 0.0f);  // Strong updraft
	Resources().m_pxFlameConfig->m_fDrag = 0.8f;
	Resources().m_pxFlameConfig->m_xColorStart = Zenith_Maths::Vector4(1.0f, 0.9f, 0.3f, 0.9f);  // Bright yellow
	Resources().m_pxFlameConfig->m_xColorEnd = Zenith_Maths::Vector4(1.0f, 0.2f, 0.0f, 0.0f);    // Red->transparent
	Resources().m_pxFlameConfig->m_fSizeStart = 0.15f;
	Resources().m_pxFlameConfig->m_fSizeEnd = 0.04f;
	Resources().m_pxFlameConfig->m_bAdditiveBlending = true;              // Glow effect
	Resources().m_pxFlameConfig->m_fTurbulence = 1.5f;                    // Flickering motion
	Resources().m_pxFlameConfig->m_bUseGPUCompute = false;
	Flux_ParticleEmitterConfig::Register("Combat_Flame", Resources().m_pxFlameConfig);

	// Create prefabs for runtime instantiation
	// Note: Prefabs are lightweight templates - components added after transform is set.
	// Use the persistent scene here rather than GetActiveScene(): InitializeCombatResources
	// runs before the initial scene is loaded, and (post-A6) GetActiveScene returns INVALID
	// until that happens. The persistent scene is always available and these template
	// entities are destroyed before any gameplay begins.
	Zenith_Scene xPersistentScene = g_xEngine.Scenes().GetPersistentScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xPersistentScene);

	// Player prefab
	{
		Zenith_Entity xPlayerTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "PlayerTemplate");
		Zenith_Prefab* pxPlayer = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxPlayer->CreateFromEntity(xPlayerTemplate, "Player");
		Resources().m_xPlayerPrefab.Set(pxPlayer);
		xPlayerTemplate.Destroy();
	}

	// Enemy prefab + three Scale variants demonstrating the variant override system.
	// The base prefab is saved to disk and re-loaded through the asset registry so
	// the variants get a proper path-based PrefabHandle that resolves on
	// Instantiate. Each variant overrides Transform.Scale to a different value
	// (0.7 / 0.9 / 1.1) so the three enemy tiers visibly differ in size.
	{
		Zenith_Entity xEnemyTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "EnemyTemplate");
		Zenith_Prefab* pxEnemy = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxEnemy->CreateFromEntity(xEnemyTemplate, "Enemy");
		Resources().m_xEnemyPrefab.Set(pxEnemy);
		xEnemyTemplate.Destroy();

		// Persist the base to disk so PrefabHandle("EnemyBase.zpfb") resolves
		// through the registry. Cheap relative-path write; the file is owned by
		// the launch and effectively transient.
		const std::string strBasePath = "EnemyBase.zpfb";
		pxEnemy->SaveToFile(strBasePath);
		Zenith_AssetRegistry::Get<Zenith_Prefab>(strBasePath);

		// Build the three Scale variants in memory.
		PrefabHandle xBaseHandle(strBasePath);
		for (u_int u = 0; u < uENEMY_VARIANT_COUNT; ++u)
		{
			Zenith_Prefab* pxVariant = Zenith_AssetRegistry::Create<Zenith_Prefab>();
			pxVariant->CreateAsVariant(xBaseHandle, g_aszEnemyVariantNames[u]);

			Zenith_PropertyOverride xOv;
			xOv.m_strComponentName = "Transform";
			xOv.m_strPropertyPath  = "Scale";
			const float f = g_afEnemyVariantScales[u];
			xOv.m_xValue << Zenith_Maths::Vector3(f, f, f);
			pxVariant->AddOverride(std::move(xOv));

			Resources().m_axEnemyVariants[u].Set(pxVariant);
		}
	}

	// Arena prefab (for floor)
	{
		Zenith_Entity xArenaTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "ArenaTemplate");
		Zenith_Prefab* pxArena = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxArena->CreateFromEntity(xArenaTemplate, "Arena");
		Resources().m_xArenaPrefab.Set(pxArena);
		xArenaTemplate.Destroy();
	}

	// ArenaWall prefab with collider and particle emitter
	// NOTE: ModelComponent is NOT included because mesh/material pointers don't serialize.
	// ModelComponent is added after instantiation in CreateArena().
	{
		Zenith_Entity xWallTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "ArenaWallTemplate");

		// Add ColliderComponent for wall collision
		xWallTemplate.AddComponent<Zenith_ColliderComponent>()
			.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

		// Add ParticleEmitterComponent for candle flame
		Zenith_ParticleEmitterComponent& xEmitter = xWallTemplate.AddComponent<Zenith_ParticleEmitterComponent>();
		xEmitter.SetConfig(Resources().m_pxFlameConfig);
		xEmitter.SetEmitting(true);

		Zenith_Prefab* pxWall = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxWall->CreateFromEntity(xWallTemplate, "ArenaWall");
		Resources().m_xArenaWallPrefab.Set(pxWall);
		xWallTemplate.Destroy();
	}

	// Create hit spark particle config
	Resources().m_pxHitSparkConfig = new Flux_ParticleEmitterConfig();
	Resources().m_pxHitSparkConfig->m_uBurstCount = 20;
	Resources().m_pxHitSparkConfig->m_fSpawnRate = 0.0f;
	Resources().m_pxHitSparkConfig->m_uMaxParticles = 256;
	Resources().m_pxHitSparkConfig->m_fLifetimeMin = 0.2f;
	Resources().m_pxHitSparkConfig->m_fLifetimeMax = 0.4f;
	Resources().m_pxHitSparkConfig->m_fSpeedMin = 8.0f;
	Resources().m_pxHitSparkConfig->m_fSpeedMax = 15.0f;
	Resources().m_pxHitSparkConfig->m_fSpreadAngleDegrees = 60.0f;
	Resources().m_pxHitSparkConfig->m_xGravity = Zenith_Maths::Vector3(0.0f, -5.0f, 0.0f);
	Resources().m_pxHitSparkConfig->m_fDrag = 2.0f;
	Resources().m_pxHitSparkConfig->m_xColorStart = Zenith_Maths::Vector4(1.0f, 0.6f, 0.1f, 1.0f);
	Resources().m_pxHitSparkConfig->m_xColorEnd = Zenith_Maths::Vector4(1.0f, 1.0f, 0.2f, 0.0f);
	Resources().m_pxHitSparkConfig->m_fSizeStart = 0.3f;
	Resources().m_pxHitSparkConfig->m_fSizeEnd = 0.1f;
	Resources().m_pxHitSparkConfig->m_bUseGPUCompute = false;
	Flux_ParticleEmitterConfig::Register("Combat_HitSpark", Resources().m_pxHitSparkConfig);

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

const char* Project_GetGameAssetsDir() { return GAME_ASSETS_DIR; }

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterScriptBehaviours()
{
	// Behaviour registration is now automatic via the ZENITH_BEHAVIOUR_TYPE_NAME macro's
	// static initializer (runs at program startup before main()). This function remains as
	// the per-game lifecycle hook for early CPU-only resource initialization that must
	// happen before any scene load (TOOLS or non-TOOLS builds).
	InitializeCombatResources();
}

void Project_Shutdown()
{
	CleanupCombatResources();
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All combat resources initialized in Project_RegisterScriptBehaviours via InitializeCombatResources
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- MainMenu scene (build index 0) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("MainMenu");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.0f, 12.0f, -15.0f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.7f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(50.0f));
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuTitle", "COMBAT ARENA");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuTitle", 0.0f, -120.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuTitle", 72.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuTitle", 1.0f, 0.2f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuPlay", "Play");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuPlay", 0.0f, 0.0f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("MenuPlay", 200.0f, 50.0f);
	g_xEngine.EditorAutomation().AddStep_AttachScript("Combat_Behaviour");
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Arena gameplay scene (build index 1) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("Arena");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.0f, 12.0f, -15.0f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.7f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(50.0f));
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// PlayerHealth: TopLeft, x=30, y=30+24*3=102, size=15*3=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("PlayerHealth", "Health: 100 / 100");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PlayerHealth", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("PlayerHealth", 30.0f, 102.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PlayerHealth", 45.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("PlayerHealth", 0.2f, 1.0f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("PlayerHealth", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("PlayerHealth", false);

	// PlayerHealthBar: TopLeft, x=30, y=30+24*4=126, size=15*2.5=37.5
	g_xEngine.EditorAutomation().AddStep_CreateUIText("PlayerHealthBar", "[||||||||||||||||||||]");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PlayerHealthBar", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("PlayerHealthBar", 30.0f, 126.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PlayerHealthBar", 37.5f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("PlayerHealthBar", 0.2f, 1.0f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("PlayerHealthBar", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("PlayerHealthBar", false);

	// EnemyCount: TopLeft, x=30, y=30+24*6=174, size=15*3=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("EnemyCount", "Enemies: 3 / 3");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("EnemyCount", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("EnemyCount", 30.0f, 174.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("EnemyCount", 45.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("EnemyCount", 0.8f, 0.8f, 0.8f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("EnemyCount", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("EnemyCount", false);

	// ComboCount: Center, x=0, y=-100, size=15*8=120
	g_xEngine.EditorAutomation().AddStep_CreateUIText("ComboCount", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ComboCount", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ComboCount", 0.0f, -100.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ComboCount", 120.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("ComboCount", 1.0f, 0.8f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ComboCount", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("ComboCount", false);

	// ComboText: Center, x=0, y=-60, size=15*4=60
	g_xEngine.EditorAutomation().AddStep_CreateUIText("ComboText", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ComboText", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ComboText", 0.0f, -60.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ComboText", 60.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("ComboText", 1.0f, 0.8f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ComboText", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("ComboText", false);

	// Controls: BottomLeft, x=30, y=30, size=15*2.5=37.5
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Controls", "WASD: Move | LMB: Attack | RMB: Heavy | Space: Dodge | R: Reset | Esc: Menu");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Controls", static_cast<int>(Zenith_UI::AnchorPreset::BottomLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Controls", 30.0f, 30.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Controls", 37.5f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Controls", 0.7f, 0.7f, 0.7f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Controls", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Controls", false);

	// Status: Center, x=0, y=0, size=15*8=120
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Status", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Status", 0.0f, 0.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Status", 120.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Status", 0.2f, 1.0f, 0.2f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Status", false);

	g_xEngine.EditorAutomation().AddStep_AttachScript("Combat_Behaviour");
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Arena" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Final scene loading ----
	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Arena" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
