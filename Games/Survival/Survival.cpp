#include "Zenith.h"

#include "Survival/Components/Survival_Behaviour.h"
#include "Survival/Components/Survival_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Flux/Flux.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "AssetHandling/Zenith_AssetDatabase.h"
#include "AssetHandling/Zenith_DataAssetManager.h"
#include "Prefab/Zenith_Prefab.h"

#include <cmath>
#include <random>
#include <vector>
#include <filesystem>

// ============================================================================
// Survival Resources - Global access for behaviours
// ============================================================================
namespace Survival
{
	// Geometry
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;
	Flux_MeshGeometry* g_pxCapsuleGeometry = nullptr;

	// Materials
	Flux_MaterialAsset* g_pxPlayerMaterial = nullptr;
	Flux_MaterialAsset* g_pxGroundMaterial = nullptr;
	Flux_MaterialAsset* g_pxTreeMaterial = nullptr;
	Flux_MaterialAsset* g_pxRockMaterial = nullptr;
	Flux_MaterialAsset* g_pxBerryMaterial = nullptr;
	Flux_MaterialAsset* g_pxWoodMaterial = nullptr;
	Flux_MaterialAsset* g_pxStoneMaterial = nullptr;

	// Prefabs for runtime instantiation
	Zenith_Prefab* g_pxPlayerPrefab = nullptr;
	Zenith_Prefab* g_pxTreePrefab = nullptr;
	Zenith_Prefab* g_pxRockPrefab = nullptr;
	Zenith_Prefab* g_pxBerryBushPrefab = nullptr;
	Zenith_Prefab* g_pxDroppedItemPrefab = nullptr;
}

static bool s_bResourcesInitialized = false;

// ============================================================================
// Procedural Texture Generation
// ============================================================================

// Export a 1x1 colored texture to disk and return a TextureRef with its GUID
static TextureRef ExportColoredTexture(const std::string& strPath, uint8_t uR, uint8_t uG, uint8_t uB)
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

	// Import into asset database to get GUID
	Zenith_AssetGUID xGUID = Zenith_AssetDatabase::ImportAsset(strPath);
	if (!xGUID.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "[Survival] Failed to import texture: %s", strPath.c_str());
		return TextureRef();
	}

	// Create TextureRef with the GUID
	TextureRef xRef;
	xRef.SetGUID(xGUID);
	return xRef;
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

			Zenith_Maths::Vector3 xTangent(-sin(fTheta), 0.f, cos(fTheta));
			xGeometryOut.m_pxTangents[uVertIdx] = xTangent;
			xGeometryOut.m_pxBitangents[uVertIdx] = glm::cross(xNormal, xTangent);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);

			uVertIdx++;
		}
	}

	uint32_t uIdxIdx = 0;
	for (uint32_t uStack = 0; uStack < uStacks; uStack++)
	{
		for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
		{
			uint32_t uCurrent = uStack * (uSlices + 1) + uSlice;
			uint32_t uNext = uCurrent + uSlices + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
		}
	}

	xGeometryOut.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

// ============================================================================
// Procedural Capsule Generation (for player)
// ============================================================================
static void GenerateCapsule(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices, uint32_t uHalfStacks)
{
	// Capsule = hemisphere top + cylinder middle + hemisphere bottom
	uint32_t uHemisphereVerts = (uHalfStacks + 1) * (uSlices + 1);
	uint32_t uCylinderVerts = 2 * (uSlices + 1);
	uint32_t uNumVerts = uHemisphereVerts * 2 + uCylinderVerts;

	uint32_t uHemisphereIndices = uHalfStacks * uSlices * 6;
	uint32_t uCylinderIndices = uSlices * 6;
	uint32_t uNumIndices = uHemisphereIndices * 2 + uCylinderIndices;

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
	uint32_t uIdxIdx = 0;
	float fCylinderHalfHeight = fHeight * 0.5f - fRadius;

	// Top hemisphere
	uint32_t uTopHemisphereStart = uVertIdx;
	for (uint32_t uStack = 0; uStack <= uHalfStacks; uStack++)
	{
		float fPhi = static_cast<float>(uStack) / static_cast<float>(uHalfStacks) * 3.14159265f * 0.5f;
		float fY = cos(fPhi) * fRadius + fCylinderHalfHeight;
		float fStackRadius = sin(fPhi) * fRadius;

		for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
		{
			float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
			float fX = cos(fTheta) * fStackRadius;
			float fZ = sin(fTheta) * fStackRadius;

			Zenith_Maths::Vector3 xPos(fX, fY, fZ);
			Zenith_Maths::Vector3 xNormal = glm::normalize(Zenith_Maths::Vector3(fX, fY - fCylinderHalfHeight, fZ));

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(
				static_cast<float>(uSlice) / static_cast<float>(uSlices),
				static_cast<float>(uStack) / static_cast<float>(uHalfStacks * 2 + 1)
			);
			xGeometryOut.m_pxTangents[uVertIdx] = Zenith_Maths::Vector3(-sin(fTheta), 0.f, cos(fTheta));
			xGeometryOut.m_pxBitangents[uVertIdx] = glm::cross(xNormal, xGeometryOut.m_pxTangents[uVertIdx]);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);
			uVertIdx++;
		}
	}

	for (uint32_t uStack = 0; uStack < uHalfStacks; uStack++)
	{
		for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
		{
			uint32_t uCurrent = uTopHemisphereStart + uStack * (uSlices + 1) + uSlice;
			uint32_t uNext = uCurrent + uSlices + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
		}
	}

	// Cylinder middle
	uint32_t uCylinderTopStart = uVertIdx;
	for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
	{
		float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
		float fX = cos(fTheta) * fRadius;
		float fZ = sin(fTheta) * fRadius;

		// Top ring
		xGeometryOut.m_pxPositions[uVertIdx] = Zenith_Maths::Vector3(fX, fCylinderHalfHeight, fZ);
		xGeometryOut.m_pxNormals[uVertIdx] = glm::normalize(Zenith_Maths::Vector3(fX, 0.f, fZ));
		xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(static_cast<float>(uSlice) / uSlices, 0.5f);
		xGeometryOut.m_pxTangents[uVertIdx] = Zenith_Maths::Vector3(-sin(fTheta), 0.f, cos(fTheta));
		xGeometryOut.m_pxBitangents[uVertIdx] = Zenith_Maths::Vector3(0.f, 1.f, 0.f);
		xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);
		uVertIdx++;
	}

	uint32_t uCylinderBottomStart = uVertIdx;
	for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
	{
		float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
		float fX = cos(fTheta) * fRadius;
		float fZ = sin(fTheta) * fRadius;

		// Bottom ring
		xGeometryOut.m_pxPositions[uVertIdx] = Zenith_Maths::Vector3(fX, -fCylinderHalfHeight, fZ);
		xGeometryOut.m_pxNormals[uVertIdx] = glm::normalize(Zenith_Maths::Vector3(fX, 0.f, fZ));
		xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(static_cast<float>(uSlice) / uSlices, 0.5f);
		xGeometryOut.m_pxTangents[uVertIdx] = Zenith_Maths::Vector3(-sin(fTheta), 0.f, cos(fTheta));
		xGeometryOut.m_pxBitangents[uVertIdx] = Zenith_Maths::Vector3(0.f, 1.f, 0.f);
		xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);
		uVertIdx++;
	}

	for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
	{
		xGeometryOut.m_puIndices[uIdxIdx++] = uCylinderTopStart + uSlice;
		xGeometryOut.m_puIndices[uIdxIdx++] = uCylinderBottomStart + uSlice;
		xGeometryOut.m_puIndices[uIdxIdx++] = uCylinderTopStart + uSlice + 1;

		xGeometryOut.m_puIndices[uIdxIdx++] = uCylinderTopStart + uSlice + 1;
		xGeometryOut.m_puIndices[uIdxIdx++] = uCylinderBottomStart + uSlice;
		xGeometryOut.m_puIndices[uIdxIdx++] = uCylinderBottomStart + uSlice + 1;
	}

	// Bottom hemisphere
	uint32_t uBottomHemisphereStart = uVertIdx;
	for (uint32_t uStack = 0; uStack <= uHalfStacks; uStack++)
	{
		float fPhi = 3.14159265f * 0.5f + static_cast<float>(uStack) / static_cast<float>(uHalfStacks) * 3.14159265f * 0.5f;
		float fY = cos(fPhi) * fRadius - fCylinderHalfHeight;
		float fStackRadius = sin(fPhi) * fRadius;

		for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
		{
			float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
			float fX = cos(fTheta) * fStackRadius;
			float fZ = sin(fTheta) * fStackRadius;

			Zenith_Maths::Vector3 xPos(fX, fY, fZ);
			Zenith_Maths::Vector3 xNormal = glm::normalize(Zenith_Maths::Vector3(fX, fY + fCylinderHalfHeight, fZ));

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(
				static_cast<float>(uSlice) / static_cast<float>(uSlices),
				0.5f + static_cast<float>(uStack + 1) / static_cast<float>(uHalfStacks * 2 + 1)
			);
			xGeometryOut.m_pxTangents[uVertIdx] = Zenith_Maths::Vector3(-sin(fTheta), 0.f, cos(fTheta));
			xGeometryOut.m_pxBitangents[uVertIdx] = glm::cross(xNormal, xGeometryOut.m_pxTangents[uVertIdx]);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);
			uVertIdx++;
		}
	}

	for (uint32_t uStack = 0; uStack < uHalfStacks; uStack++)
	{
		for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
		{
			uint32_t uCurrent = uBottomHemisphereStart + uStack * (uSlices + 1) + uSlice;
			uint32_t uNext = uCurrent + uSlices + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
		}
	}

	xGeometryOut.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

// ============================================================================
// Resource Initialization
// ============================================================================
static void InitializeSurvivalResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace Survival;

	// Create directory for procedural meshes
	std::string strMeshDir = std::string(GAME_ASSETS_DIR) + "/Meshes";
	std::filesystem::create_directories(strMeshDir);

	// Create geometries
	g_pxCubeGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*g_pxCubeGeometry);
#ifdef ZENITH_TOOLS
	std::string strCubePath = strMeshDir + "/Cube.zmesh";
	g_pxCubeGeometry->Export(strCubePath.c_str());
	g_pxCubeGeometry->m_strSourcePath = strCubePath;
#endif

	g_pxSphereGeometry = new Flux_MeshGeometry();
	GenerateUVSphere(*g_pxSphereGeometry, 0.5f, 16, 12);
#ifdef ZENITH_TOOLS
	std::string strSpherePath = strMeshDir + "/Sphere.zmesh";
	g_pxSphereGeometry->Export(strSpherePath.c_str());
	g_pxSphereGeometry->m_strSourcePath = strSpherePath;
#endif

	g_pxCapsuleGeometry = new Flux_MeshGeometry();
	GenerateCapsule(*g_pxCapsuleGeometry, 0.3f, 1.6f, 12, 6);
#ifdef ZENITH_TOOLS
	std::string strCapsulePath = strMeshDir + "/Capsule.zmesh";
	g_pxCapsuleGeometry->Export(strCapsulePath.c_str());
	g_pxCapsuleGeometry->m_strSourcePath = strCapsulePath;
#endif

	// Create textures directory
	std::string strTexturesDir = std::string(GAME_ASSETS_DIR) + "/Textures";
	std::filesystem::create_directories(strTexturesDir);

	// Export procedural textures to disk and get TextureRefs
	TextureRef xPlayerTextureRef = ExportColoredTexture(strTexturesDir + "/Player.ztex", 51, 102, 230);      // Blue player
	TextureRef xGroundTextureRef = ExportColoredTexture(strTexturesDir + "/Ground.ztex", 90, 70, 50);        // Brown ground
	TextureRef xTreeTextureRef = ExportColoredTexture(strTexturesDir + "/Tree.ztex", 40, 120, 40);           // Green tree
	TextureRef xRockTextureRef = ExportColoredTexture(strTexturesDir + "/Rock.ztex", 120, 120, 130);         // Gray rock
	TextureRef xBerryTextureRef = ExportColoredTexture(strTexturesDir + "/Berry.ztex", 200, 50, 80);         // Red berries
	TextureRef xWoodTextureRef = ExportColoredTexture(strTexturesDir + "/Wood.ztex", 139, 90, 43);           // Brown wood
	TextureRef xStoneTextureRef = ExportColoredTexture(strTexturesDir + "/Stone.ztex", 100, 100, 110);       // Gray stone item

	// Create materials with TextureRefs (properly serializable)
	g_pxPlayerMaterial = Flux_MaterialAsset::Create("SurvivalPlayer");
	g_pxPlayerMaterial->SetDiffuseTextureRef(xPlayerTextureRef);

	g_pxGroundMaterial = Flux_MaterialAsset::Create("SurvivalGround");
	g_pxGroundMaterial->SetDiffuseTextureRef(xGroundTextureRef);

	g_pxTreeMaterial = Flux_MaterialAsset::Create("SurvivalTree");
	g_pxTreeMaterial->SetDiffuseTextureRef(xTreeTextureRef);

	g_pxRockMaterial = Flux_MaterialAsset::Create("SurvivalRock");
	g_pxRockMaterial->SetDiffuseTextureRef(xRockTextureRef);

	g_pxBerryMaterial = Flux_MaterialAsset::Create("SurvivalBerry");
	g_pxBerryMaterial->SetDiffuseTextureRef(xBerryTextureRef);

	g_pxWoodMaterial = Flux_MaterialAsset::Create("SurvivalWood");
	g_pxWoodMaterial->SetDiffuseTextureRef(xWoodTextureRef);

	g_pxStoneMaterial = Flux_MaterialAsset::Create("SurvivalStone");
	g_pxStoneMaterial->SetDiffuseTextureRef(xStoneTextureRef);

	// Create prefabs for runtime instantiation
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Player prefab
	{
		Zenith_Entity xPlayerTemplate(&xScene, "PlayerTemplate");
		g_pxPlayerPrefab = new Zenith_Prefab();
		g_pxPlayerPrefab->CreateFromEntity(xPlayerTemplate, "Player");
		Zenith_Scene::Destroy(xPlayerTemplate);
	}

	// Tree prefab (resource node)
	{
		Zenith_Entity xTreeTemplate(&xScene, "TreeTemplate");
		g_pxTreePrefab = new Zenith_Prefab();
		g_pxTreePrefab->CreateFromEntity(xTreeTemplate, "Tree");
		Zenith_Scene::Destroy(xTreeTemplate);
	}

	// Rock prefab (resource node)
	{
		Zenith_Entity xRockTemplate(&xScene, "RockTemplate");
		g_pxRockPrefab = new Zenith_Prefab();
		g_pxRockPrefab->CreateFromEntity(xRockTemplate, "Rock");
		Zenith_Scene::Destroy(xRockTemplate);
	}

	// Berry bush prefab (resource node)
	{
		Zenith_Entity xBerryTemplate(&xScene, "BerryBushTemplate");
		g_pxBerryBushPrefab = new Zenith_Prefab();
		g_pxBerryBushPrefab->CreateFromEntity(xBerryTemplate, "BerryBush");
		Zenith_Scene::Destroy(xBerryTemplate);
	}

	// Dropped item prefab
	{
		Zenith_Entity xDroppedItemTemplate(&xScene, "DroppedItemTemplate");
		g_pxDroppedItemPrefab = new Zenith_Prefab();
		g_pxDroppedItemPrefab->CreateFromEntity(xDroppedItemTemplate, "DroppedItem");
		Zenith_Scene::Destroy(xDroppedItemTemplate);
	}

	s_bResourcesInitialized = true;
}

// ============================================================================
// Project Entry Points
// ============================================================================
const char* Project_GetName()
{
	return "Survival";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeSurvivalResources();

	// Register DataAsset types
	RegisterSurvivalDataAssets();

	Survival_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Survival has no resources that need explicit cleanup
}

void Project_LoadInitialScene()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	xScene.Reset();

	// Create camera entity - third-person perspective behind player
	Zenith_Entity xCameraEntity(&xScene, "MainCamera");
	xCameraEntity.SetTransient(false);
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.f, 10.f, -15.f),  // Position: behind and above
		-0.5f,  // Pitch: looking slightly down
		0.f,    // Yaw: facing forward
		glm::radians(50.f),
		0.1f,
		1000.f,
		16.f / 9.f
	);
	xScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Create main game entity
	Zenith_Entity xSurvivalEntity(&xScene, "SurvivalGame");
	xSurvivalEntity.SetTransient(false);

	// UI Setup
	static constexpr float s_fMarginLeft = 30.f;
	static constexpr float s_fMarginTop = 30.f;
	static constexpr float s_fBaseTextSize = 15.f;
	static constexpr float s_fLineHeight = 24.f;

	Zenith_UIComponent& xUI = xSurvivalEntity.AddComponent<Zenith_UIComponent>();

	auto SetupTopLeftText = [](Zenith_UI::Zenith_UIText* pxText, float fYOffset)
	{
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxText->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Left);
	};

	// Title
	Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "SURVIVAL");
	SetupTopLeftText(pxTitle, 0.f);
	pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
	pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	// Controls header
	Zenith_UI::Zenith_UIText* pxControlsHeader = xUI.CreateText("ControlsHeader", "Controls:");
	SetupTopLeftText(pxControlsHeader, s_fLineHeight * 2);
	pxControlsHeader->SetFontSize(s_fBaseTextSize * 3.0f);
	pxControlsHeader->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	// Controls text
	Zenith_UI::Zenith_UIText* pxMove = xUI.CreateText("MoveInstr", "WASD: Move | E: Interact | Tab: Inventory");
	SetupTopLeftText(pxMove, s_fLineHeight * 3);
	pxMove->SetFontSize(s_fBaseTextSize * 2.5f);
	pxMove->SetColor(Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 1.f));

	Zenith_UI::Zenith_UIText* pxCraft = xUI.CreateText("CraftInstr", "C: Crafting | R: Reset");
	SetupTopLeftText(pxCraft, s_fLineHeight * 4);
	pxCraft->SetFontSize(s_fBaseTextSize * 2.5f);
	pxCraft->SetColor(Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 1.f));

	// Inventory display (right side)
	auto SetupTopRightText = [](Zenith_UI::Zenith_UIText* pxText, float fYOffset)
	{
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopRight);
		pxText->SetPosition(-30.f, 30.f + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Right);
	};

	Zenith_UI::Zenith_UIText* pxInvHeader = xUI.CreateText("InventoryHeader", "Inventory:");
	SetupTopRightText(pxInvHeader, 0.f);
	pxInvHeader->SetFontSize(s_fBaseTextSize * 3.6f);
	pxInvHeader->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	Zenith_UI::Zenith_UIText* pxWood = xUI.CreateText("WoodCount", "Wood: 0");
	SetupTopRightText(pxWood, s_fLineHeight * 1);
	pxWood->SetFontSize(s_fBaseTextSize * 3.0f);
	pxWood->SetColor(Zenith_Maths::Vector4(0.8f, 0.6f, 0.3f, 1.f));

	Zenith_UI::Zenith_UIText* pxStone = xUI.CreateText("StoneCount", "Stone: 0");
	SetupTopRightText(pxStone, s_fLineHeight * 2);
	pxStone->SetFontSize(s_fBaseTextSize * 3.0f);
	pxStone->SetColor(Zenith_Maths::Vector4(0.6f, 0.6f, 0.7f, 1.f));

	Zenith_UI::Zenith_UIText* pxBerries = xUI.CreateText("BerriesCount", "Berries: 0");
	SetupTopRightText(pxBerries, s_fLineHeight * 3);
	pxBerries->SetFontSize(s_fBaseTextSize * 3.0f);
	pxBerries->SetColor(Zenith_Maths::Vector4(0.8f, 0.3f, 0.4f, 1.f));

	// Crafted items
	Zenith_UI::Zenith_UIText* pxCraftedHeader = xUI.CreateText("CraftedHeader", "Crafted:");
	SetupTopRightText(pxCraftedHeader, s_fLineHeight * 5);
	pxCraftedHeader->SetFontSize(s_fBaseTextSize * 3.0f);
	pxCraftedHeader->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	Zenith_UI::Zenith_UIText* pxAxe = xUI.CreateText("AxeCount", "Axe: 0");
	SetupTopRightText(pxAxe, s_fLineHeight * 6);
	pxAxe->SetFontSize(s_fBaseTextSize * 3.0f);
	pxAxe->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxPickaxe = xUI.CreateText("PickaxeCount", "Pickaxe: 0");
	SetupTopRightText(pxPickaxe, s_fLineHeight * 7);
	pxPickaxe->SetFontSize(s_fBaseTextSize * 3.0f);
	pxPickaxe->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	// Interaction prompt (center-bottom)
	Zenith_UI::Zenith_UIText* pxInteract = xUI.CreateText("InteractPrompt", "");
	pxInteract->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomCenter);
	pxInteract->SetPosition(0.f, -100.f);
	pxInteract->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxInteract->SetFontSize(s_fBaseTextSize * 4.0f);
	pxInteract->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 0.6f, 1.f));

	// Crafting progress (center)
	Zenith_UI::Zenith_UIText* pxCraftProgress = xUI.CreateText("CraftProgress", "");
	pxCraftProgress->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxCraftProgress->SetPosition(0.f, 100.f);
	pxCraftProgress->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxCraftProgress->SetFontSize(s_fBaseTextSize * 3.5f);
	pxCraftProgress->SetColor(Zenith_Maths::Vector4(0.6f, 1.f, 0.6f, 1.f));

	// Status message (center)
	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "");
	pxStatus->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxStatus->SetPosition(0.f, 0.f);
	pxStatus->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxStatus->SetFontSize(s_fBaseTextSize * 5.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));

	// Add script component with Survival behaviour
	Zenith_ScriptComponent& xScript = xSurvivalEntity.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<Survival_Behaviour>();

	// ========================================================================
	// Create Ground
	// ========================================================================
	Zenith_Entity xGround(&xScene, "Ground");
	xGround.SetTransient(false);

	Zenith_TransformComponent& xGroundTransform = xGround.GetComponent<Zenith_TransformComponent>();
	xGroundTransform.SetPosition(Zenith_Maths::Vector3(0.f, -0.5f, 0.f));
	xGroundTransform.SetScale(Zenith_Maths::Vector3(100.f, 1.f, 100.f));

	Zenith_ModelComponent& xGroundModel = xGround.AddComponent<Zenith_ModelComponent>();
	xGroundModel.AddMeshEntry(*Survival::g_pxCubeGeometry, *Survival::g_pxGroundMaterial);

	// ========================================================================
	// Create Player
	// ========================================================================
	static constexpr float s_fPlayerHeightLocal = 1.6f;

	Zenith_Entity xPlayer = Zenith_Scene::Instantiate(*Survival::g_pxPlayerPrefab, "Player");
	xPlayer.SetTransient(false);

	Zenith_TransformComponent& xPlayerTransform = xPlayer.GetComponent<Zenith_TransformComponent>();
	xPlayerTransform.SetPosition(Zenith_Maths::Vector3(0.f, s_fPlayerHeightLocal * 0.5f, 0.f));
	xPlayerTransform.SetScale(Zenith_Maths::Vector3(1.f));

	Zenith_ModelComponent& xPlayerModel = xPlayer.AddComponent<Zenith_ModelComponent>();
	xPlayerModel.AddMeshEntry(*Survival::g_pxCapsuleGeometry, *Survival::g_pxPlayerMaterial);

	// ========================================================================
	// Create Resource Nodes (deterministic positions using fixed seed)
	// ========================================================================
	static constexpr uint32_t s_uTreeCount = 15;
	static constexpr uint32_t s_uRockCount = 10;
	static constexpr uint32_t s_uBerryCount = 8;
	static constexpr float s_fWorldRadius = 40.f;
	static constexpr float s_fMinDistance = 5.f;

	std::mt19937 xRng(12345);  // Fixed seed for reproducible layout
	std::uniform_real_distribution<float> xAngleDist(0.f, 6.28318f);
	std::uniform_real_distribution<float> xRadiusDist(8.f, s_fWorldRadius);

	std::vector<Zenith_Maths::Vector3> axPositions;

	auto GeneratePosition = [&]() -> Zenith_Maths::Vector3
	{
		for (int iTry = 0; iTry < 50; iTry++)
		{
			float fAngle = xAngleDist(xRng);
			float fRadius = xRadiusDist(xRng);
			Zenith_Maths::Vector3 xPos(cos(fAngle) * fRadius, 0.f, sin(fAngle) * fRadius);

			bool bValid = true;
			for (const auto& xExisting : axPositions)
			{
				if (glm::distance(xPos, xExisting) < s_fMinDistance)
				{
					bValid = false;
					break;
				}
			}

			if (bValid)
			{
				axPositions.push_back(xPos);
				return xPos;
			}
		}
		float fAngle = xAngleDist(xRng);
		float fRadius = xRadiusDist(xRng);
		return Zenith_Maths::Vector3(cos(fAngle) * fRadius, 0.f, sin(fAngle) * fRadius);
	};

	// Create trees
	for (uint32_t i = 0; i < s_uTreeCount; i++)
	{
		Zenith_Maths::Vector3 xPos = GeneratePosition();
		Zenith_Maths::Vector3 xScale(1.5f, 4.f, 1.5f);

		char szName[32];
		snprintf(szName, sizeof(szName), "Tree_%u", i);
		Zenith_Entity xTree = Zenith_Scene::Instantiate(*Survival::g_pxTreePrefab, szName);
		xTree.SetTransient(false);

		Zenith_TransformComponent& xTreeTransform = xTree.GetComponent<Zenith_TransformComponent>();
		xTreeTransform.SetPosition(xPos + Zenith_Maths::Vector3(0.f, xScale.y * 0.5f, 0.f));
		xTreeTransform.SetScale(xScale);

		Zenith_ModelComponent& xTreeModel = xTree.AddComponent<Zenith_ModelComponent>();
		xTreeModel.AddMeshEntry(*Survival::g_pxCubeGeometry, *Survival::g_pxTreeMaterial);
	}

	// Create rocks
	for (uint32_t i = 0; i < s_uRockCount; i++)
	{
		Zenith_Maths::Vector3 xPos = GeneratePosition();
		Zenith_Maths::Vector3 xScale(2.f, 1.5f, 2.f);

		char szName[32];
		snprintf(szName, sizeof(szName), "Rock_%u", i);
		Zenith_Entity xRock = Zenith_Scene::Instantiate(*Survival::g_pxRockPrefab, szName);
		xRock.SetTransient(false);

		Zenith_TransformComponent& xRockTransform = xRock.GetComponent<Zenith_TransformComponent>();
		xRockTransform.SetPosition(xPos + Zenith_Maths::Vector3(0.f, xScale.y * 0.5f, 0.f));
		xRockTransform.SetScale(xScale);

		Zenith_ModelComponent& xRockModel = xRock.AddComponent<Zenith_ModelComponent>();
		xRockModel.AddMeshEntry(*Survival::g_pxSphereGeometry, *Survival::g_pxRockMaterial);
	}

	// Create berry bushes
	for (uint32_t i = 0; i < s_uBerryCount; i++)
	{
		Zenith_Maths::Vector3 xPos = GeneratePosition();
		Zenith_Maths::Vector3 xScale(1.2f, 1.f, 1.2f);

		char szName[32];
		snprintf(szName, sizeof(szName), "BerryBush_%u", i);
		Zenith_Entity xBush = Zenith_Scene::Instantiate(*Survival::g_pxBerryBushPrefab, szName);
		xBush.SetTransient(false);

		Zenith_TransformComponent& xBushTransform = xBush.GetComponent<Zenith_TransformComponent>();
		xBushTransform.SetPosition(xPos + Zenith_Maths::Vector3(0.f, xScale.y * 0.5f, 0.f));
		xBushTransform.SetScale(xScale);

		Zenith_ModelComponent& xBushModel = xBush.AddComponent<Zenith_ModelComponent>();
		xBushModel.AddMeshEntry(*Survival::g_pxSphereGeometry, *Survival::g_pxBerryMaterial);
	}

	// Save the scene file
	std::string strScenePath = std::string(GAME_ASSETS_DIR) + "/Scenes/Survival.zscn";
	std::filesystem::create_directories(std::string(GAME_ASSETS_DIR) + "/Scenes");
	xScene.SaveToFile(strScenePath);
}
