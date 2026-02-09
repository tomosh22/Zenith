#include "Zenith.h"

#include "Survival/Components/Survival_Behaviour.h"
#include "Survival/Components/Survival_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Prefab/Zenith_Prefab.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "UI/Zenith_UIButton.h"

#include <cmath>
#include <random>
#include <vector>
#include <filesystem>

// ============================================================================
// Survival Resources - Global access for behaviours
// ============================================================================
namespace Survival
{
	// Geometry assets (registry-managed)
	Zenith_MeshGeometryAsset* g_pxCubeAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxSphereAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxCapsuleAsset = nullptr;

	// Convenience pointers to underlying geometry
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;
	Flux_MeshGeometry* g_pxCapsuleGeometry = nullptr;

	// Materials
	MaterialHandle g_xPlayerMaterial;
	MaterialHandle g_xGroundMaterial;
	MaterialHandle g_xTreeMaterial;
	MaterialHandle g_xRockMaterial;
	MaterialHandle g_xBerryMaterial;
	MaterialHandle g_xWoodMaterial;
	MaterialHandle g_xStoneMaterial;

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
		Zenith_Error(LOG_CATEGORY_ASSET, "[Survival] Failed to make relative path for texture: %s", strPath.c_str());
		return TextureHandle();
	}

	// Create TextureHandle with the prefixed path
	return TextureHandle(strRelativePath);
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

	// Create geometries using registry
	g_pxCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_pxCubeGeometry = g_pxCubeAsset->GetGeometry();
#ifdef ZENITH_TOOLS
	std::string strCubePath = strMeshDir + "/Cube.zmesh";
	g_pxCubeGeometry->Export(strCubePath.c_str());
	g_pxCubeGeometry->m_strSourcePath = strCubePath;
#endif

	// Custom sphere - tracked through registry
	g_pxSphereAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>();
	Flux_MeshGeometry* pxSphere = new Flux_MeshGeometry();
	GenerateUVSphere(*pxSphere, 0.5f, 16, 12);
	g_pxSphereAsset->SetGeometry(pxSphere);
	g_pxSphereGeometry = g_pxSphereAsset->GetGeometry();
#ifdef ZENITH_TOOLS
	std::string strSpherePath = strMeshDir + "/Sphere.zmesh";
	g_pxSphereGeometry->Export(strSpherePath.c_str());
	g_pxSphereGeometry->m_strSourcePath = strSpherePath;
#endif

	// Custom capsule - tracked through registry
	g_pxCapsuleAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>();
	Flux_MeshGeometry* pxCapsule = new Flux_MeshGeometry();
	GenerateCapsule(*pxCapsule, 0.3f, 1.6f, 12, 6);
	g_pxCapsuleAsset->SetGeometry(pxCapsule);
	g_pxCapsuleGeometry = g_pxCapsuleAsset->GetGeometry();
#ifdef ZENITH_TOOLS
	std::string strCapsulePath = strMeshDir + "/Capsule.zmesh";
	g_pxCapsuleGeometry->Export(strCapsulePath.c_str());
	g_pxCapsuleGeometry->m_strSourcePath = strCapsulePath;
#endif

	// Create textures directory
	std::string strTexturesDir = std::string(GAME_ASSETS_DIR) + "/Textures";
	std::filesystem::create_directories(strTexturesDir);

	// Export procedural textures to disk and get TextureHandles
	TextureHandle xPlayerTextureHandle = ExportColoredTexture(strTexturesDir + "/Player.ztex", 51, 102, 230);      // Blue player
	TextureHandle xGroundTextureHandle = ExportColoredTexture(strTexturesDir + "/Ground.ztex", 90, 70, 50);        // Brown ground
	TextureHandle xTreeTextureHandle = ExportColoredTexture(strTexturesDir + "/Tree.ztex", 40, 120, 40);           // Green tree
	TextureHandle xRockTextureHandle = ExportColoredTexture(strTexturesDir + "/Rock.ztex", 120, 120, 130);         // Gray rock
	TextureHandle xBerryTextureHandle = ExportColoredTexture(strTexturesDir + "/Berry.ztex", 200, 50, 80);         // Red berries
	TextureHandle xWoodTextureHandle = ExportColoredTexture(strTexturesDir + "/Wood.ztex", 139, 90, 43);           // Brown wood
	TextureHandle xStoneTextureHandle = ExportColoredTexture(strTexturesDir + "/Stone.ztex", 100, 100, 110);       // Gray stone item

	// Create materials with texture paths (properly serializable)
	auto& xRegistry = Zenith_AssetRegistry::Get();

	g_xPlayerMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xPlayerMaterial.Get()->SetName("SurvivalPlayer");
	g_xPlayerMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Player.ztex");

	g_xGroundMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xGroundMaterial.Get()->SetName("SurvivalGround");
	g_xGroundMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Ground.ztex");

	g_xTreeMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xTreeMaterial.Get()->SetName("SurvivalTree");
	g_xTreeMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Tree.ztex");

	g_xRockMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xRockMaterial.Get()->SetName("SurvivalRock");
	g_xRockMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Rock.ztex");

	g_xBerryMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xBerryMaterial.Get()->SetName("SurvivalBerry");
	g_xBerryMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Berry.ztex");

	g_xWoodMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xWoodMaterial.Get()->SetName("SurvivalWood");
	g_xWoodMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Wood.ztex");

	g_xStoneMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xStoneMaterial.Get()->SetName("SurvivalStone");
	g_xStoneMaterial.Get()->SetDiffuseTexturePath(strTexturesDir + "/Stone.ztex");

	// Create prefabs for runtime instantiation
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Player prefab
	{
		Zenith_Entity xPlayerTemplate(pxSceneData, "PlayerTemplate");
		g_pxPlayerPrefab = new Zenith_Prefab();
		g_pxPlayerPrefab->CreateFromEntity(xPlayerTemplate, "Player");
		Zenith_SceneManager::Destroy(xPlayerTemplate);
	}

	// Tree prefab (resource node)
	{
		Zenith_Entity xTreeTemplate(pxSceneData, "TreeTemplate");
		g_pxTreePrefab = new Zenith_Prefab();
		g_pxTreePrefab->CreateFromEntity(xTreeTemplate, "Tree");
		Zenith_SceneManager::Destroy(xTreeTemplate);
	}

	// Rock prefab (resource node)
	{
		Zenith_Entity xRockTemplate(pxSceneData, "RockTemplate");
		g_pxRockPrefab = new Zenith_Prefab();
		g_pxRockPrefab->CreateFromEntity(xRockTemplate, "Rock");
		Zenith_SceneManager::Destroy(xRockTemplate);
	}

	// Berry bush prefab (resource node)
	{
		Zenith_Entity xBerryTemplate(pxSceneData, "BerryBushTemplate");
		g_pxBerryBushPrefab = new Zenith_Prefab();
		g_pxBerryBushPrefab->CreateFromEntity(xBerryTemplate, "BerryBush");
		Zenith_SceneManager::Destroy(xBerryTemplate);
	}

	// Dropped item prefab
	{
		Zenith_Entity xDroppedItemTemplate(pxSceneData, "DroppedItemTemplate");
		g_pxDroppedItemPrefab = new Zenith_Prefab();
		g_pxDroppedItemPrefab->CreateFromEntity(xDroppedItemTemplate, "DroppedItem");
		Zenith_SceneManager::Destroy(xDroppedItemTemplate);
	}

	s_bResourcesInitialized = true;
}

// ============================================================================
// World Content Creation (called from Survival_Behaviour::StartGame)
// ============================================================================
void Survival_CreateWorldContent(Zenith_SceneData* pxSceneData)
{
	// ========================================================================
	// Create Ground
	// ========================================================================
	Zenith_Entity xGround(pxSceneData, "Ground");
	xGround.SetTransient(false);

	Zenith_TransformComponent& xGroundTransform = xGround.GetComponent<Zenith_TransformComponent>();
	xGroundTransform.SetPosition(Zenith_Maths::Vector3(0.f, -0.5f, 0.f));
	xGroundTransform.SetScale(Zenith_Maths::Vector3(100.f, 1.f, 100.f));

	Zenith_ModelComponent& xGroundModel = xGround.AddComponent<Zenith_ModelComponent>();
	xGroundModel.AddMeshEntry(*Survival::g_pxCubeGeometry, *Survival::g_xGroundMaterial.Get());

	// ========================================================================
	// Create Player
	// ========================================================================
	static constexpr float s_fPlayerHeightLocal = 1.6f;

	Zenith_Entity xPlayer = Survival::g_pxPlayerPrefab->Instantiate(pxSceneData, "Player");
	xPlayer.SetTransient(false);

	Zenith_TransformComponent& xPlayerTransform = xPlayer.GetComponent<Zenith_TransformComponent>();
	xPlayerTransform.SetPosition(Zenith_Maths::Vector3(0.f, s_fPlayerHeightLocal * 0.5f, 0.f));
	xPlayerTransform.SetScale(Zenith_Maths::Vector3(1.f));

	Zenith_ModelComponent& xPlayerModel = xPlayer.AddComponent<Zenith_ModelComponent>();
	xPlayerModel.AddMeshEntry(*Survival::g_pxCapsuleGeometry, *Survival::g_xPlayerMaterial.Get());

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
		Zenith_Entity xTree = Survival::g_pxTreePrefab->Instantiate(pxSceneData, szName);
		xTree.SetTransient(false);

		Zenith_TransformComponent& xTreeTransform = xTree.GetComponent<Zenith_TransformComponent>();
		xTreeTransform.SetPosition(xPos + Zenith_Maths::Vector3(0.f, xScale.y * 0.5f, 0.f));
		xTreeTransform.SetScale(xScale);

		Zenith_ModelComponent& xTreeModel = xTree.AddComponent<Zenith_ModelComponent>();
		xTreeModel.AddMeshEntry(*Survival::g_pxCubeGeometry, *Survival::g_xTreeMaterial.Get());
	}

	// Create rocks
	for (uint32_t i = 0; i < s_uRockCount; i++)
	{
		Zenith_Maths::Vector3 xPos = GeneratePosition();
		Zenith_Maths::Vector3 xScale(2.f, 1.5f, 2.f);

		char szName[32];
		snprintf(szName, sizeof(szName), "Rock_%u", i);
		Zenith_Entity xRock = Survival::g_pxRockPrefab->Instantiate(pxSceneData, szName);
		xRock.SetTransient(false);

		Zenith_TransformComponent& xRockTransform = xRock.GetComponent<Zenith_TransformComponent>();
		xRockTransform.SetPosition(xPos + Zenith_Maths::Vector3(0.f, xScale.y * 0.5f, 0.f));
		xRockTransform.SetScale(xScale);

		Zenith_ModelComponent& xRockModel = xRock.AddComponent<Zenith_ModelComponent>();
		xRockModel.AddMeshEntry(*Survival::g_pxSphereGeometry, *Survival::g_xRockMaterial.Get());
	}

	// Create berry bushes
	for (uint32_t i = 0; i < s_uBerryCount; i++)
	{
		Zenith_Maths::Vector3 xPos = GeneratePosition();
		Zenith_Maths::Vector3 xScale(1.2f, 1.f, 1.2f);

		char szName[32];
		snprintf(szName, sizeof(szName), "BerryBush_%u", i);
		Zenith_Entity xBush = Survival::g_pxBerryBushPrefab->Instantiate(pxSceneData, szName);
		xBush.SetTransient(false);

		Zenith_TransformComponent& xBushTransform = xBush.GetComponent<Zenith_TransformComponent>();
		xBushTransform.SetPosition(xPos + Zenith_Maths::Vector3(0.f, xScale.y * 0.5f, 0.f));
		xBushTransform.SetScale(xScale);

		Zenith_ModelComponent& xBushModel = xBush.AddComponent<Zenith_ModelComponent>();
		xBushModel.AddMeshEntry(*Survival::g_pxSphereGeometry, *Survival::g_xBerryMaterial.Get());
	}
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

	Survival_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Survival has no resources that need explicit cleanup
}

void Project_LoadInitialScene()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	pxSceneData->Reset();

	// Create persistent GameManager entity (camera + UI + script)
	Zenith_Entity xGameManager(pxSceneData, "GameManager");
	xGameManager.SetTransient(false);

	// Camera - third-person perspective behind player
	Zenith_CameraComponent& xCamera = xGameManager.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.f, 10.f, -15.f),
		-0.5f,
		0.f,
		glm::radians(50.f),
		0.1f,
		1000.f,
		16.f / 9.f
	);

	// UI
	Zenith_UIComponent& xUI = xGameManager.AddComponent<Zenith_UIComponent>();

	// ---- Menu UI (visible initially) ----
	Zenith_UI::Zenith_UIText* pxMenuTitle = xUI.CreateText("MenuTitle", "SURVIVAL");
	pxMenuTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxMenuTitle->SetPosition(0.f, -120.f);
	pxMenuTitle->SetFontSize(48.f);
	pxMenuTitle->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));

	Zenith_UI::Zenith_UIButton* pxPlayButton = xUI.CreateButton("MenuPlay", "Play");
	pxPlayButton->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxPlayButton->SetPosition(0.f, 0.f);
	pxPlayButton->SetSize(200.f, 50.f);

	// ---- HUD UI (hidden initially) ----
	static constexpr float s_fMarginLeft = 30.f;
	static constexpr float s_fMarginTop = 30.f;
	static constexpr float s_fBaseTextSize = 15.f;
	static constexpr float s_fLineHeight = 24.f;

	auto CreateHUDTextTopLeft = [&](const char* szName, const char* szText, float fYOffset,
		float fSizeMultiplier, const Zenith_Maths::Vector4& xColor) -> Zenith_UI::Zenith_UIText*
	{
		Zenith_UI::Zenith_UIText* pxText = xUI.CreateText(szName, szText);
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxText->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Left);
		pxText->SetFontSize(s_fBaseTextSize * fSizeMultiplier);
		pxText->SetColor(xColor);
		pxText->SetVisible(false);
		return pxText;
	};

	auto CreateHUDTextTopRight = [&](const char* szName, const char* szText, float fYOffset,
		float fSizeMultiplier, const Zenith_Maths::Vector4& xColor) -> Zenith_UI::Zenith_UIText*
	{
		Zenith_UI::Zenith_UIText* pxText = xUI.CreateText(szName, szText);
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopRight);
		pxText->SetPosition(-30.f, 30.f + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Right);
		pxText->SetFontSize(s_fBaseTextSize * fSizeMultiplier);
		pxText->SetColor(xColor);
		pxText->SetVisible(false);
		return pxText;
	};

	// Top-left HUD
	CreateHUDTextTopLeft("Title", "SURVIVAL", 0.f, 4.8f,
		Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));
	CreateHUDTextTopLeft("ControlsHeader", "Controls:", s_fLineHeight * 2, 3.0f,
		Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));
	CreateHUDTextTopLeft("MoveInstr", "WASD: Move | E: Interact | Tab: Inventory", s_fLineHeight * 3, 2.5f,
		Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 1.f));
	CreateHUDTextTopLeft("CraftInstr", "C: Crafting | R: Reset | Esc: Menu", s_fLineHeight * 4, 2.5f,
		Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 1.f));

	// Top-right HUD (inventory)
	CreateHUDTextTopRight("InventoryHeader", "Inventory:", 0.f, 3.6f,
		Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));
	CreateHUDTextTopRight("WoodCount", "Wood: 0", s_fLineHeight * 1, 3.0f,
		Zenith_Maths::Vector4(0.8f, 0.6f, 0.3f, 1.f));
	CreateHUDTextTopRight("StoneCount", "Stone: 0", s_fLineHeight * 2, 3.0f,
		Zenith_Maths::Vector4(0.6f, 0.6f, 0.7f, 1.f));
	CreateHUDTextTopRight("BerriesCount", "Berries: 0", s_fLineHeight * 3, 3.0f,
		Zenith_Maths::Vector4(0.8f, 0.3f, 0.4f, 1.f));
	CreateHUDTextTopRight("CraftedHeader", "Crafted:", s_fLineHeight * 5, 3.0f,
		Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));
	CreateHUDTextTopRight("AxeCount", "Axe: 0", s_fLineHeight * 6, 3.0f,
		Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));
	CreateHUDTextTopRight("PickaxeCount", "Pickaxe: 0", s_fLineHeight * 7, 3.0f,
		Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	// Center/bottom HUD
	Zenith_UI::Zenith_UIText* pxInteract = xUI.CreateText("InteractPrompt", "");
	pxInteract->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomCenter);
	pxInteract->SetPosition(0.f, -100.f);
	pxInteract->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxInteract->SetFontSize(s_fBaseTextSize * 4.0f);
	pxInteract->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 0.6f, 1.f));
	pxInteract->SetVisible(false);

	Zenith_UI::Zenith_UIText* pxCraftProgress = xUI.CreateText("CraftProgress", "");
	pxCraftProgress->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxCraftProgress->SetPosition(0.f, 100.f);
	pxCraftProgress->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxCraftProgress->SetFontSize(s_fBaseTextSize * 3.5f);
	pxCraftProgress->SetColor(Zenith_Maths::Vector4(0.6f, 1.f, 0.6f, 1.f));
	pxCraftProgress->SetVisible(false);

	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "");
	pxStatus->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxStatus->SetPosition(0.f, 0.f);
	pxStatus->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxStatus->SetFontSize(s_fBaseTextSize * 5.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));
	pxStatus->SetVisible(false);

	// Script
	Zenith_ScriptComponent& xScript = xGameManager.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviourForSerialization<Survival_Behaviour>();

	// Mark as persistent - survives all scene transitions
	xGameManager.DontDestroyOnLoad();
}
