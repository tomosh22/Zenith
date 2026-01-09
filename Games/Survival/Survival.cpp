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
#include "AssetHandling/Zenith_DataAssetManager.h"
#include "Prefab/Zenith_Prefab.h"

#include <cmath>

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

static Flux_Texture* s_pxPlayerTexture = nullptr;
static Flux_Texture* s_pxGroundTexture = nullptr;
static Flux_Texture* s_pxTreeTexture = nullptr;
static Flux_Texture* s_pxRockTexture = nullptr;
static Flux_Texture* s_pxBerryTexture = nullptr;
static Flux_Texture* s_pxWoodTexture = nullptr;
static Flux_Texture* s_pxStoneTexture = nullptr;
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

	// Create geometries
	g_pxCubeGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*g_pxCubeGeometry);

	g_pxSphereGeometry = new Flux_MeshGeometry();
	GenerateUVSphere(*g_pxSphereGeometry, 0.5f, 16, 12);

	g_pxCapsuleGeometry = new Flux_MeshGeometry();
	GenerateCapsule(*g_pxCapsuleGeometry, 0.3f, 1.6f, 12, 6);

	// Create textures (procedural single-color pixels)
	s_pxPlayerTexture = CreateColoredTexture(51, 102, 230);      // Blue player
	s_pxGroundTexture = CreateColoredTexture(90, 70, 50);        // Brown ground
	s_pxTreeTexture = CreateColoredTexture(40, 120, 40);         // Green tree
	s_pxRockTexture = CreateColoredTexture(120, 120, 130);       // Gray rock
	s_pxBerryTexture = CreateColoredTexture(200, 50, 80);        // Red berries
	s_pxWoodTexture = CreateColoredTexture(139, 90, 43);         // Brown wood
	s_pxStoneTexture = CreateColoredTexture(100, 100, 110);      // Gray stone item

	// Create materials
	g_pxPlayerMaterial = Flux_MaterialAsset::Create("SurvivalPlayer");
	g_pxPlayerMaterial->SetDiffuseTexture(s_pxPlayerTexture);

	g_pxGroundMaterial = Flux_MaterialAsset::Create("SurvivalGround");
	g_pxGroundMaterial->SetDiffuseTexture(s_pxGroundTexture);

	g_pxTreeMaterial = Flux_MaterialAsset::Create("SurvivalTree");
	g_pxTreeMaterial->SetDiffuseTexture(s_pxTreeTexture);

	g_pxRockMaterial = Flux_MaterialAsset::Create("SurvivalRock");
	g_pxRockMaterial->SetDiffuseTexture(s_pxRockTexture);

	g_pxBerryMaterial = Flux_MaterialAsset::Create("SurvivalBerry");
	g_pxBerryMaterial->SetDiffuseTexture(s_pxBerryTexture);

	g_pxWoodMaterial = Flux_MaterialAsset::Create("SurvivalWood");
	g_pxWoodMaterial->SetDiffuseTexture(s_pxWoodTexture);

	g_pxStoneMaterial = Flux_MaterialAsset::Create("SurvivalStone");
	g_pxStoneMaterial->SetDiffuseTexture(s_pxStoneTexture);

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

	// Save the scene file
	std::string strScenePath = std::string(GAME_ASSETS_DIR) + "/Scenes/Survival.zscn";
	std::filesystem::create_directories(std::string(GAME_ASSETS_DIR) + "/Scenes");
	xScene.SaveToFile(strScenePath);
}
