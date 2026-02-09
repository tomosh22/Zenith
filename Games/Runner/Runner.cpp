#include "Zenith.h"

#include "Runner/Components/Runner_Behaviour.h"
#include "Runner/Components/Runner_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux_Graphics.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UIButton.h"

#include <cmath>

// ============================================================================
// Runner Resources - Global access for behaviours
// ============================================================================
namespace Runner
{
	// Geometry assets (registry-managed)
	Zenith_MeshGeometryAsset* g_pxCapsuleAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxCubeAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxSphereAsset = nullptr;

	// Convenience pointers to underlying geometry
	Flux_MeshGeometry* g_pxCapsuleGeometry = nullptr;
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;

	MaterialHandle g_xCharacterMaterial;
	MaterialHandle g_xGroundMaterial;
	MaterialHandle g_xObstacleMaterial;
	MaterialHandle g_xCollectibleMaterial;
	MaterialHandle g_xDustMaterial;
	MaterialHandle g_xCollectParticleMaterial;

	Zenith_Prefab* g_pxCharacterPrefab = nullptr;
	Zenith_Prefab* g_pxGroundPrefab = nullptr;
	Zenith_Prefab* g_pxObstaclePrefab = nullptr;
	Zenith_Prefab* g_pxCollectiblePrefab = nullptr;
	Zenith_Prefab* g_pxParticlePrefab = nullptr;
}

static bool s_bResourcesInitialized = false;

// ============================================================================
// Procedural Capsule Geometry Generation
// ============================================================================
static void GenerateCapsule(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices, uint32_t uRings)
{
	// Capsule: two hemispheres connected by a cylinder
	// Total height = fHeight (not including hemispheres)
	// Hemisphere radius = fRadius

	float fCylinderHeight = fHeight - 2.0f * fRadius;
	if (fCylinderHeight < 0.0f) fCylinderHeight = 0.0f;

	// Calculate vertex and index counts
	// Two hemispheres: uRings/2 stacks each, uSlices slices
	// Cylinder: 2 rings, uSlices slices
	uint32_t uHemisphereVerts = (uRings / 2 + 1) * (uSlices + 1);
	uint32_t uCylinderVerts = 2 * (uSlices + 1);
	uint32_t uNumVerts = uHemisphereVerts * 2 + uCylinderVerts;

	uint32_t uHemisphereIndices = (uRings / 2) * uSlices * 6;
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

	// Top hemisphere (Y offset = fCylinderHeight / 2)
	float fTopOffset = fCylinderHeight * 0.5f;
	uint32_t uTopHemiStart = uVertIdx;

	for (uint32_t uRing = 0; uRing <= uRings / 2; uRing++)
	{
		float fPhi = static_cast<float>(uRing) / static_cast<float>(uRings) * 3.14159265f;
		float fY = cos(fPhi) * fRadius + fTopOffset;
		float fRingRadius = sin(fPhi) * fRadius;

		for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
		{
			float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
			float fX = cos(fTheta) * fRingRadius;
			float fZ = sin(fTheta) * fRingRadius;

			Zenith_Maths::Vector3 xPos(fX, fY, fZ);
			Zenith_Maths::Vector3 xNormal = glm::normalize(Zenith_Maths::Vector3(fX, (fY - fTopOffset), fZ));

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(
				static_cast<float>(uSlice) / static_cast<float>(uSlices),
				static_cast<float>(uRing) / static_cast<float>(uRings)
			);

			Zenith_Maths::Vector3 xTangent(-sin(fTheta), 0.f, cos(fTheta));
			xGeometryOut.m_pxTangents[uVertIdx] = xTangent;
			xGeometryOut.m_pxBitangents[uVertIdx] = glm::cross(xNormal, xTangent);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);

			uVertIdx++;
		}
	}

	// Top hemisphere indices
	for (uint32_t uRing = 0; uRing < uRings / 2; uRing++)
	{
		for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
		{
			uint32_t uCurrent = uTopHemiStart + uRing * (uSlices + 1) + uSlice;
			uint32_t uNext = uCurrent + uSlices + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
		}
	}

	// Cylinder
	uint32_t uCylinderStart = uVertIdx;
	for (int iRing = 0; iRing < 2; iRing++)
	{
		float fY = (iRing == 0) ? fTopOffset : -fCylinderHeight * 0.5f;
		for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
		{
			float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
			float fX = cos(fTheta) * fRadius;
			float fZ = sin(fTheta) * fRadius;

			xGeometryOut.m_pxPositions[uVertIdx] = Zenith_Maths::Vector3(fX, fY, fZ);
			xGeometryOut.m_pxNormals[uVertIdx] = glm::normalize(Zenith_Maths::Vector3(fX, 0.f, fZ));
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(
				static_cast<float>(uSlice) / static_cast<float>(uSlices),
				0.5f + static_cast<float>(iRing) * 0.1f
			);
			xGeometryOut.m_pxTangents[uVertIdx] = Zenith_Maths::Vector3(-sin(fTheta), 0.f, cos(fTheta));
			xGeometryOut.m_pxBitangents[uVertIdx] = Zenith_Maths::Vector3(0.f, 1.f, 0.f);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);
			uVertIdx++;
		}
	}

	// Cylinder indices
	for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
	{
		uint32_t uCurrent = uCylinderStart + uSlice;
		uint32_t uNext = uCurrent + uSlices + 1;

		xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
		xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
		xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

		xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
		xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
		xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
	}

	// Bottom hemisphere (Y offset = -fCylinderHeight / 2)
	float fBottomOffset = -fCylinderHeight * 0.5f;
	uint32_t uBottomHemiStart = uVertIdx;

	for (uint32_t uRing = uRings / 2; uRing <= uRings; uRing++)
	{
		float fPhi = static_cast<float>(uRing) / static_cast<float>(uRings) * 3.14159265f;
		float fY = cos(fPhi) * fRadius + fBottomOffset;
		float fRingRadius = sin(fPhi) * fRadius;

		for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
		{
			float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
			float fX = cos(fTheta) * fRingRadius;
			float fZ = sin(fTheta) * fRingRadius;

			Zenith_Maths::Vector3 xPos(fX, fY, fZ);
			Zenith_Maths::Vector3 xNormal = glm::normalize(Zenith_Maths::Vector3(fX, (fY - fBottomOffset), fZ));

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(
				static_cast<float>(uSlice) / static_cast<float>(uSlices),
				static_cast<float>(uRing) / static_cast<float>(uRings)
			);

			Zenith_Maths::Vector3 xTangent(-sin(fTheta), 0.f, cos(fTheta));
			xGeometryOut.m_pxTangents[uVertIdx] = xTangent;
			xGeometryOut.m_pxBitangents[uVertIdx] = glm::cross(xNormal, xTangent);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f);

			uVertIdx++;
		}
	}

	// Bottom hemisphere indices
	uint32_t uHalfRings = uRings / 2;
	for (uint32_t uRing = 0; uRing < uHalfRings; uRing++)
	{
		for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
		{
			uint32_t uCurrent = uBottomHemiStart + uRing * (uSlices + 1) + uSlice;
			uint32_t uNext = uCurrent + uSlices + 1;

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
// Resource Initialization
// ============================================================================
static void InitializeRunnerResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace Runner;

	// Create capsule geometry for character - custom size, tracked through registry
	g_pxCapsuleAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>();
	Flux_MeshGeometry* pxCapsule = new Flux_MeshGeometry();
	GenerateCapsule(*pxCapsule, 0.4f, 1.8f, 16, 12);
	g_pxCapsuleAsset->SetGeometry(pxCapsule);
	g_pxCapsuleGeometry = g_pxCapsuleAsset->GetGeometry();

	// Create cube geometry for obstacles and ground - use registry's cached unit cube
	g_pxCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_pxCubeGeometry = g_pxCubeAsset->GetGeometry();

	// Create sphere geometry for collectibles and particles - custom size, tracked through registry
	g_pxSphereAsset = Zenith_AssetRegistry::Get().Create<Zenith_MeshGeometryAsset>();
	Flux_MeshGeometry* pxSphere = new Flux_MeshGeometry();
	GenerateUVSphere(*pxSphere, 0.5f, 16, 12);
	g_pxSphereAsset->SetGeometry(pxSphere);
	g_pxSphereGeometry = g_pxSphereAsset->GetGeometry();

	// Use grid pattern texture with BaseColor for all materials
	Zenith_TextureAsset* pxGridTex = Flux_Graphics::s_pxGridTexture;

	// Create materials with grid texture and BaseColor
	auto& xRegistry = Zenith_AssetRegistry::Get();
	g_xCharacterMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xCharacterMaterial.Get()->SetName("RunnerCharacter");
	g_xCharacterMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xCharacterMaterial.Get()->SetBaseColor({ 51.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

	g_xGroundMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xGroundMaterial.Get()->SetName("RunnerGround");
	g_xGroundMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xGroundMaterial.Get()->SetBaseColor({ 102.f/255.f, 77.f/255.f, 51.f/255.f, 1.f });

	g_xObstacleMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xObstacleMaterial.Get()->SetName("RunnerObstacle");
	g_xObstacleMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xObstacleMaterial.Get()->SetBaseColor({ 204.f/255.f, 51.f/255.f, 51.f/255.f, 1.f });

	g_xCollectibleMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xCollectibleMaterial.Get()->SetName("RunnerCollectible");
	g_xCollectibleMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xCollectibleMaterial.Get()->SetBaseColor({ 255.f/255.f, 215.f/255.f, 0.f/255.f, 1.f });

	g_xDustMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xDustMaterial.Get()->SetName("RunnerDust");
	g_xDustMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xDustMaterial.Get()->SetBaseColor({ 180.f/255.f, 150.f/255.f, 100.f/255.f, 1.f });

	g_xCollectParticleMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xCollectParticleMaterial.Get()->SetName("RunnerCollectParticle");
	g_xCollectParticleMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xCollectParticleMaterial.Get()->SetBaseColor({ 255.f/255.f, 255.f/255.f, 150.f/255.f, 1.f });

	// Create prefabs for runtime instantiation
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Character prefab
	{
		Zenith_Entity xCharTemplate(pxSceneData, "CharacterTemplate");
		g_pxCharacterPrefab = new Zenith_Prefab();
		g_pxCharacterPrefab->CreateFromEntity(xCharTemplate, "Runner");
		Zenith_SceneManager::Destroy(xCharTemplate);
	}

	// Ground prefab
	{
		Zenith_Entity xGroundTemplate(pxSceneData, "GroundTemplate");
		g_pxGroundPrefab = new Zenith_Prefab();
		g_pxGroundPrefab->CreateFromEntity(xGroundTemplate, "Ground");
		Zenith_SceneManager::Destroy(xGroundTemplate);
	}

	// Obstacle prefab
	{
		Zenith_Entity xObstacleTemplate(pxSceneData, "ObstacleTemplate");
		g_pxObstaclePrefab = new Zenith_Prefab();
		g_pxObstaclePrefab->CreateFromEntity(xObstacleTemplate, "Obstacle");
		Zenith_SceneManager::Destroy(xObstacleTemplate);
	}

	// Collectible prefab
	{
		Zenith_Entity xCollectibleTemplate(pxSceneData, "CollectibleTemplate");
		g_pxCollectiblePrefab = new Zenith_Prefab();
		g_pxCollectiblePrefab->CreateFromEntity(xCollectibleTemplate, "Collectible");
		Zenith_SceneManager::Destroy(xCollectibleTemplate);
	}

	// Particle prefab
	{
		Zenith_Entity xParticleTemplate(pxSceneData, "ParticleTemplate");
		g_pxParticlePrefab = new Zenith_Prefab();
		g_pxParticlePrefab->CreateFromEntity(xParticleTemplate, "Particle");
		Zenith_SceneManager::Destroy(xParticleTemplate);
	}

	s_bResourcesInitialized = true;
}

// ============================================================================
// Project Entry Points
// ============================================================================
const char* Project_GetName()
{
	return "Runner";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeRunnerResources();

	Runner_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Runner has no resources that need explicit cleanup
}

void Project_LoadInitialScene()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	pxSceneData->Reset();

	// Create persistent GameManager entity (camera + UI + script)
	Zenith_Entity xGameManager(pxSceneData, "GameManager");
	xGameManager.SetTransient(false);

	// Camera
	Zenith_CameraComponent& xCamera = xGameManager.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.f, 4.f, -8.f),
		-0.3f,
		0.f,
		glm::radians(60.f),
		0.1f,
		1000.f,
		16.f / 9.f
	);
	pxSceneData->SetMainCameraEntity(xGameManager.GetEntityID());

	// UI
	static constexpr float s_fMarginLeft = 30.f;
	static constexpr float s_fMarginTop = 30.f;
	static constexpr float s_fBaseTextSize = 15.f;
	static constexpr float s_fLineHeight = 28.f;

	Zenith_UIComponent& xUI = xGameManager.AddComponent<Zenith_UIComponent>();

	// --- Menu UI (visible initially) ---
	Zenith_UI::Zenith_UIText* pxMenuTitle = xUI.CreateText("MenuTitle", "ENDLESS RUNNER");
	pxMenuTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxMenuTitle->SetPosition(0.f, -120.f);
	pxMenuTitle->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxMenuTitle->SetFontSize(s_fBaseTextSize * 6.0f);
	pxMenuTitle->SetColor(Zenith_Maths::Vector4(0.3f, 0.6f, 1.f, 1.f));

	Zenith_UI::Zenith_UIButton* pxPlayBtn = xUI.CreateButton("MenuPlay", "Play");
	pxPlayBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxPlayBtn->SetPosition(0.f, 0.f);
	pxPlayBtn->SetSize(200.f, 50.f);

	// --- HUD UI (hidden initially) ---
	auto CreateHUDText = [&](const char* szName, const char* szText, float fYOffset) -> Zenith_UI::Zenith_UIText*
	{
		Zenith_UI::Zenith_UIText* pxText = xUI.CreateText(szName, szText);
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxText->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Left);
		pxText->SetVisible(false);
		return pxText;
	};

	Zenith_UI::Zenith_UIText* pxTitle = CreateHUDText("Title", "ENDLESS RUNNER", 0.f);
	pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
	pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxDistance = CreateHUDText("Distance", "0m", s_fLineHeight * 2.5f);
	pxDistance->SetFontSize(s_fBaseTextSize * 6.0f);
	pxDistance->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxScore = CreateHUDText("Score", "Score: 0", s_fLineHeight * 5);
	pxScore->SetFontSize(s_fBaseTextSize * 3.0f);
	pxScore->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxHighScore = CreateHUDText("HighScore", "Best: 0", s_fLineHeight * 6);
	pxHighScore->SetFontSize(s_fBaseTextSize * 3.0f);
	pxHighScore->SetColor(Zenith_Maths::Vector4(1.f, 0.84f, 0.f, 1.f));

	Zenith_UI::Zenith_UIText* pxSpeed = CreateHUDText("Speed", "Speed: 15.0", s_fLineHeight * 7);
	pxSpeed->SetFontSize(s_fBaseTextSize * 3.0f);
	pxSpeed->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxControls = CreateHUDText("Controls", "A/D: Lanes | Space/W: Jump | S: Slide | R: Reset | Esc: Menu", s_fLineHeight * 9);
	pxControls->SetFontSize(s_fBaseTextSize * 2.5f);
	pxControls->SetColor(Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 1.f));

	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "");
	pxStatus->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxStatus->SetPosition(0.f, 0.f);
	pxStatus->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxStatus->SetFontSize(s_fBaseTextSize * 5.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(1.f, 0.3f, 0.3f, 1.f));
	pxStatus->SetVisible(false);

	// Script
	Zenith_ScriptComponent& xScript = xGameManager.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviourForSerialization<Runner_Behaviour>();

	// Mark as persistent
	xGameManager.DontDestroyOnLoad();
}
