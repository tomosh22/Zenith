#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"
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

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#endif

// ============================================================================
// Survival Resources - Global access for behaviours
// ============================================================================
namespace Survival
{
	static SurvivalResources g_xResources;
	SurvivalResources& Resources() { return g_xResources; }
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
	g_xEngine.VulkanMemory().InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	g_xEngine.VulkanMemory().InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
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
	g_xEngine.VulkanMemory().InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	g_xEngine.VulkanMemory().InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
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
	Resources().m_xCubeAsset.Set(Zenith_MeshGeometryAsset::CreateUnitCube());
	Resources().m_pxCubeGeometry = Resources().m_xCubeAsset.GetDirect()->GetGeometry();
#ifdef ZENITH_TOOLS
	std::string strCubePath = strMeshDir + "/Cube" ZENITH_MESH_EXT;
	Resources().m_pxCubeGeometry->Export(strCubePath.c_str());
	Resources().m_pxCubeGeometry->m_strSourcePath = strCubePath;
#endif

	// Custom sphere - tracked through registry
	{
		Zenith_MeshGeometryAsset* pxSphereAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Flux_MeshGeometry* pxSphere = new Flux_MeshGeometry();
		GenerateUVSphere(*pxSphere, 0.5f, 16, 12);
		pxSphereAsset->SetGeometry(pxSphere);
		Resources().m_xSphereAsset.Set(pxSphereAsset);
		Resources().m_pxSphereGeometry = pxSphereAsset->GetGeometry();
	}
#ifdef ZENITH_TOOLS
	std::string strSpherePath = strMeshDir + "/Sphere" ZENITH_MESH_EXT;
	Resources().m_pxSphereGeometry->Export(strSpherePath.c_str());
	Resources().m_pxSphereGeometry->m_strSourcePath = strSpherePath;
#endif

	// Custom capsule - tracked through registry
	{
		Zenith_MeshGeometryAsset* pxCapsuleAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Flux_MeshGeometry* pxCapsule = new Flux_MeshGeometry();
		GenerateCapsule(*pxCapsule, 0.3f, 1.6f, 12, 6);
		pxCapsuleAsset->SetGeometry(pxCapsule);
		Resources().m_xCapsuleAsset.Set(pxCapsuleAsset);
		Resources().m_pxCapsuleGeometry = pxCapsuleAsset->GetGeometry();
	}
#ifdef ZENITH_TOOLS
	std::string strCapsulePath = strMeshDir + "/Capsule" ZENITH_MESH_EXT;
	Resources().m_pxCapsuleGeometry->Export(strCapsulePath.c_str());
	Resources().m_pxCapsuleGeometry->m_strSourcePath = strCapsulePath;
#endif

	// Create textures directory
	std::string strTexturesDir = std::string(GAME_ASSETS_DIR) + "/Textures";
	std::filesystem::create_directories(strTexturesDir);

	// Export procedural textures to disk and get TextureHandles
	TextureHandle xPlayerTextureHandle = ExportColoredTexture(strTexturesDir + "/Player" ZENITH_TEXTURE_EXT, 51, 102, 230);      // Blue player
	TextureHandle xGroundTextureHandle = ExportColoredTexture(strTexturesDir + "/Ground" ZENITH_TEXTURE_EXT, 90, 70, 50);        // Brown ground
	TextureHandle xTreeTextureHandle = ExportColoredTexture(strTexturesDir + "/Tree" ZENITH_TEXTURE_EXT, 40, 120, 40);           // Green tree
	TextureHandle xRockTextureHandle = ExportColoredTexture(strTexturesDir + "/Rock" ZENITH_TEXTURE_EXT, 120, 120, 130);         // Gray rock
	TextureHandle xBerryTextureHandle = ExportColoredTexture(strTexturesDir + "/Berry" ZENITH_TEXTURE_EXT, 200, 50, 80);         // Red berries
	TextureHandle xWoodTextureHandle = ExportColoredTexture(strTexturesDir + "/Wood" ZENITH_TEXTURE_EXT, 139, 90, 43);           // Brown wood
	TextureHandle xStoneTextureHandle = ExportColoredTexture(strTexturesDir + "/Stone" ZENITH_TEXTURE_EXT, 100, 100, 110);       // Gray stone item

	// Create materials with texture paths (properly serializable)

	Resources().m_xPlayerMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xPlayerMaterial.GetDirect()->SetName("SurvivalPlayer");
	Resources().m_xPlayerMaterial.GetDirect()->SetDiffuseTexture(xPlayerTextureHandle);

	Resources().m_xGroundMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xGroundMaterial.GetDirect()->SetName("SurvivalGround");
	Resources().m_xGroundMaterial.GetDirect()->SetDiffuseTexture(xGroundTextureHandle);

	Resources().m_xTreeMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xTreeMaterial.GetDirect()->SetName("SurvivalTree");
	Resources().m_xTreeMaterial.GetDirect()->SetDiffuseTexture(xTreeTextureHandle);

	Resources().m_xRockMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xRockMaterial.GetDirect()->SetName("SurvivalRock");
	Resources().m_xRockMaterial.GetDirect()->SetDiffuseTexture(xRockTextureHandle);

	Resources().m_xBerryMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xBerryMaterial.GetDirect()->SetName("SurvivalBerry");
	Resources().m_xBerryMaterial.GetDirect()->SetDiffuseTexture(xBerryTextureHandle);

	Resources().m_xWoodMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xWoodMaterial.GetDirect()->SetName("SurvivalWood");
	Resources().m_xWoodMaterial.GetDirect()->SetDiffuseTexture(xWoodTextureHandle);

	Resources().m_xStoneMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xStoneMaterial.GetDirect()->SetName("SurvivalStone");
	Resources().m_xStoneMaterial.GetDirect()->SetDiffuseTexture(xStoneTextureHandle);

	// Create prefabs for runtime instantiation.
	// Use the persistent scene here: InitializeResources runs before the initial scene
	// loads, and (post-A6) GetActiveScene returns INVALID until that happens.
	Zenith_Scene xActiveScene = g_xEngine.SceneRegistry().GetPersistentScene();
	Zenith_SceneData* pxSceneData = g_xEngine.SceneRegistry().GetSceneData(xActiveScene);

	// Player prefab
	{
		Zenith_Entity xPlayerTemplate(pxSceneData, "PlayerTemplate");
		Zenith_Prefab* pxPlayer = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxPlayer->CreateFromEntity(xPlayerTemplate, "Player");
		Resources().m_xPlayerPrefab.Set(pxPlayer);
		Zenith_SceneEntityOwnership::Destroy(xPlayerTemplate);
	}

	// Tree prefab (resource node)
	{
		Zenith_Entity xTreeTemplate(pxSceneData, "TreeTemplate");
		Zenith_Prefab* pxTree = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxTree->CreateFromEntity(xTreeTemplate, "Tree");
		Resources().m_xTreePrefab.Set(pxTree);
		Zenith_SceneEntityOwnership::Destroy(xTreeTemplate);
	}

	// Rock prefab (resource node)
	{
		Zenith_Entity xRockTemplate(pxSceneData, "RockTemplate");
		Zenith_Prefab* pxRock = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxRock->CreateFromEntity(xRockTemplate, "Rock");
		Resources().m_xRockPrefab.Set(pxRock);
		Zenith_SceneEntityOwnership::Destroy(xRockTemplate);
	}

	// Berry bush prefab (resource node)
	{
		Zenith_Entity xBerryTemplate(pxSceneData, "BerryBushTemplate");
		Zenith_Prefab* pxBerry = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxBerry->CreateFromEntity(xBerryTemplate, "BerryBush");
		Resources().m_xBerryBushPrefab.Set(pxBerry);
		Zenith_SceneEntityOwnership::Destroy(xBerryTemplate);
	}

	// Dropped item prefab
	{
		Zenith_Entity xDroppedItemTemplate(pxSceneData, "DroppedItemTemplate");
		Zenith_Prefab* pxDropped = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxDropped->CreateFromEntity(xDroppedItemTemplate, "DroppedItem");
		Resources().m_xDroppedItemPrefab.Set(pxDropped);
		Zenith_SceneEntityOwnership::Destroy(xDroppedItemTemplate);
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
	xGroundModel.AddMeshEntry(*Survival::Resources().m_pxCubeGeometry, *Survival::Resources().m_xGroundMaterial.GetDirect());

	// ========================================================================
	// Create Player
	// ========================================================================
	static constexpr float s_fPlayerHeightLocal = 1.6f;

	Zenith_Entity xPlayer = Survival::Resources().m_xPlayerPrefab.GetDirect()->Instantiate(pxSceneData, "Player");
	xPlayer.SetTransient(false);

	Zenith_TransformComponent& xPlayerTransform = xPlayer.GetComponent<Zenith_TransformComponent>();
	xPlayerTransform.SetPosition(Zenith_Maths::Vector3(0.f, s_fPlayerHeightLocal * 0.5f, 0.f));
	xPlayerTransform.SetScale(Zenith_Maths::Vector3(1.f));

	Zenith_ModelComponent& xPlayerModel = xPlayer.AddComponent<Zenith_ModelComponent>();
	xPlayerModel.AddMeshEntry(*Survival::Resources().m_pxCapsuleGeometry, *Survival::Resources().m_xPlayerMaterial.GetDirect());

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
		Zenith_Entity xTree = Survival::Resources().m_xTreePrefab.GetDirect()->Instantiate(pxSceneData, szName);
		xTree.SetTransient(false);

		Zenith_TransformComponent& xTreeTransform = xTree.GetComponent<Zenith_TransformComponent>();
		xTreeTransform.SetPosition(xPos + Zenith_Maths::Vector3(0.f, xScale.y * 0.5f, 0.f));
		xTreeTransform.SetScale(xScale);

		Zenith_ModelComponent& xTreeModel = xTree.AddComponent<Zenith_ModelComponent>();
		xTreeModel.AddMeshEntry(*Survival::Resources().m_pxCubeGeometry, *Survival::Resources().m_xTreeMaterial.GetDirect());
	}

	// Create rocks
	for (uint32_t i = 0; i < s_uRockCount; i++)
	{
		Zenith_Maths::Vector3 xPos = GeneratePosition();
		Zenith_Maths::Vector3 xScale(2.f, 1.5f, 2.f);

		char szName[32];
		snprintf(szName, sizeof(szName), "Rock_%u", i);
		Zenith_Entity xRock = Survival::Resources().m_xRockPrefab.GetDirect()->Instantiate(pxSceneData, szName);
		xRock.SetTransient(false);

		Zenith_TransformComponent& xRockTransform = xRock.GetComponent<Zenith_TransformComponent>();
		xRockTransform.SetPosition(xPos + Zenith_Maths::Vector3(0.f, xScale.y * 0.5f, 0.f));
		xRockTransform.SetScale(xScale);

		Zenith_ModelComponent& xRockModel = xRock.AddComponent<Zenith_ModelComponent>();
		xRockModel.AddMeshEntry(*Survival::Resources().m_pxSphereGeometry, *Survival::Resources().m_xRockMaterial.GetDirect());
	}

	// Create berry bushes
	for (uint32_t i = 0; i < s_uBerryCount; i++)
	{
		Zenith_Maths::Vector3 xPos = GeneratePosition();
		Zenith_Maths::Vector3 xScale(1.2f, 1.f, 1.2f);

		char szName[32];
		snprintf(szName, sizeof(szName), "BerryBush_%u", i);
		Zenith_Entity xBush = Survival::Resources().m_xBerryBushPrefab.GetDirect()->Instantiate(pxSceneData, szName);
		xBush.SetTransient(false);

		Zenith_TransformComponent& xBushTransform = xBush.GetComponent<Zenith_TransformComponent>();
		xBushTransform.SetPosition(xPos + Zenith_Maths::Vector3(0.f, xScale.y * 0.5f, 0.f));
		xBushTransform.SetScale(xScale);

		Zenith_ModelComponent& xBushModel = xBush.AddComponent<Zenith_ModelComponent>();
		xBushModel.AddMeshEntry(*Survival::Resources().m_pxSphereGeometry, *Survival::Resources().m_xBerryMaterial.GetDirect());
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

const char* Project_GetGameAssetsDir() { return GAME_ASSETS_DIR; }

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeSurvivalResources();

	// Survival_Behaviour auto-registers via ZENITH_BEHAVIOUR_TYPE_NAME
}

void Project_Shutdown()
{
	// Drop asset handle refs before Zenith_AssetRegistry::Shutdown teardown.
	Survival::Resources().m_xCubeAsset.Clear();
	Survival::Resources().m_xSphereAsset.Clear();
	Survival::Resources().m_xCapsuleAsset.Clear();
	Survival::Resources().m_xPlayerMaterial.Clear();
	Survival::Resources().m_xGroundMaterial.Clear();
	Survival::Resources().m_xTreeMaterial.Clear();
	Survival::Resources().m_xRockMaterial.Clear();
	Survival::Resources().m_xBerryMaterial.Clear();
	Survival::Resources().m_xWoodMaterial.Clear();
	Survival::Resources().m_xStoneMaterial.Clear();
	Survival::Resources().m_xPlayerPrefab.Clear();
	Survival::Resources().m_xTreePrefab.Clear();
	Survival::Resources().m_xRockPrefab.Clear();
	Survival::Resources().m_xBerryBushPrefab.Clear();
	Survival::Resources().m_xDroppedItemPrefab.Clear();
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All resources initialized in Project_RegisterScriptBehaviours
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- MainMenu scene (build index 0) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("MainMenu");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("MenuManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 10.f, -15.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.5f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(50.f));
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuTitle", "SURVIVAL");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuTitle", 48.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuTitle", 0.2f, 1.f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuPlay", "Play");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	g_xEngine.EditorAutomation().AddStep_AttachScript("Survival_Behaviour");
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Survival gameplay scene (build index 1) ----
	g_xEngine.EditorAutomation().AddStep_CreateScene("Survival");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 10.f, -15.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.5f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(50.f));
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// Top-left HUD (constants: marginLeft=30, marginTop=30, baseTextSize=15, lineHeight=24)
	// Title: y=30+0=30, fontSize=15*4.8=72
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Title", "SURVIVAL");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Title", 30.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Title", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Title", 72.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Title", false);

	// ControlsHeader: y=30+48=78, fontSize=15*3.0=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("ControlsHeader", "Controls:");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("ControlsHeader", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("ControlsHeader", 30.f, 78.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("ControlsHeader", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("ControlsHeader", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("ControlsHeader", 0.9f, 0.9f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("ControlsHeader", false);

	// MoveInstr: y=30+72=102, fontSize=15*2.5=37.5
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MoveInstr", "WASD: Move | E: Interact | Tab: Inventory");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MoveInstr", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MoveInstr", 30.f, 102.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("MoveInstr", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MoveInstr", 37.5f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MoveInstr", 0.7f, 0.7f, 0.7f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("MoveInstr", false);

	// CraftInstr: y=30+96=126, fontSize=15*2.5=37.5
	g_xEngine.EditorAutomation().AddStep_CreateUIText("CraftInstr", "C: Crafting | R: Reset | Esc: Menu");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CraftInstr", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CraftInstr", 30.f, 126.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("CraftInstr", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CraftInstr", 37.5f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CraftInstr", 0.7f, 0.7f, 0.7f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CraftInstr", false);

	// Top-right HUD (inventory)
	// InventoryHeader: y=30+0=30, fontSize=15*3.6=54
	g_xEngine.EditorAutomation().AddStep_CreateUIText("InventoryHeader", "Inventory:");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("InventoryHeader", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("InventoryHeader", -30.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("InventoryHeader", static_cast<int>(Zenith_UI::TextAlignment::Right));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("InventoryHeader", 54.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("InventoryHeader", 0.9f, 0.9f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("InventoryHeader", false);

	// WoodCount: y=30+24=54, fontSize=15*3.0=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("WoodCount", "Wood: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("WoodCount", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("WoodCount", -30.f, 54.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("WoodCount", static_cast<int>(Zenith_UI::TextAlignment::Right));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("WoodCount", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("WoodCount", 0.8f, 0.6f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("WoodCount", false);

	// StoneCount: y=30+48=78, fontSize=15*3.0=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("StoneCount", "Stone: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("StoneCount", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("StoneCount", -30.f, 78.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("StoneCount", static_cast<int>(Zenith_UI::TextAlignment::Right));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("StoneCount", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("StoneCount", 0.6f, 0.6f, 0.7f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("StoneCount", false);

	// BerriesCount: y=30+72=102, fontSize=15*3.0=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("BerriesCount", "Berries: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("BerriesCount", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("BerriesCount", -30.f, 102.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("BerriesCount", static_cast<int>(Zenith_UI::TextAlignment::Right));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("BerriesCount", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("BerriesCount", 0.8f, 0.3f, 0.4f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("BerriesCount", false);

	// CraftedHeader: y=30+120=150, fontSize=15*3.0=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("CraftedHeader", "Crafted:");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CraftedHeader", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CraftedHeader", -30.f, 150.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("CraftedHeader", static_cast<int>(Zenith_UI::TextAlignment::Right));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CraftedHeader", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CraftedHeader", 0.9f, 0.9f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CraftedHeader", false);

	// AxeCount: y=30+144=174, fontSize=15*3.0=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("AxeCount", "Axe: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("AxeCount", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("AxeCount", -30.f, 174.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("AxeCount", static_cast<int>(Zenith_UI::TextAlignment::Right));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("AxeCount", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("AxeCount", 0.6f, 0.8f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("AxeCount", false);

	// PickaxeCount: y=30+168=198, fontSize=15*3.0=45
	g_xEngine.EditorAutomation().AddStep_CreateUIText("PickaxeCount", "Pickaxe: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("PickaxeCount", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("PickaxeCount", -30.f, 198.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("PickaxeCount", static_cast<int>(Zenith_UI::TextAlignment::Right));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("PickaxeCount", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("PickaxeCount", 0.6f, 0.8f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("PickaxeCount", false);

	// Center/bottom HUD
	// InteractPrompt: BottomCenter, (0,-100), Center align, fontSize=15*4.0=60
	g_xEngine.EditorAutomation().AddStep_CreateUIText("InteractPrompt", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("InteractPrompt", static_cast<int>(Zenith_UI::AnchorPreset::BottomCenter));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("InteractPrompt", 0.f, -100.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("InteractPrompt", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("InteractPrompt", 60.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("InteractPrompt", 1.f, 1.f, 0.6f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("InteractPrompt", false);

	// CraftProgress: Center, (0,100), Center align, fontSize=15*3.5=52.5
	g_xEngine.EditorAutomation().AddStep_CreateUIText("CraftProgress", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("CraftProgress", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("CraftProgress", 0.f, 100.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("CraftProgress", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("CraftProgress", 52.5f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("CraftProgress", 0.6f, 1.f, 0.6f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("CraftProgress", false);

	// Status: Center, (0,0), Center align, fontSize=15*5.0=75
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Status", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Status", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Status", 75.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Status", 0.2f, 1.f, 0.2f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Status", false);

	g_xEngine.EditorAutomation().AddStep_AttachScript("Survival_Behaviour");

	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Survival" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Final scene loading ----
	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.SceneRegistry().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.SceneRegistry().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Survival" ZENITH_SCENE_EXT);
	g_xEngine.SceneOperations().LoadSceneByIndexBlockingForBootstrap(0, SCENE_LOAD_SINGLE);
}
