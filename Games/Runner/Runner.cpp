#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "Runner/Components/Runner_Behaviour.h"
#include "Runner/Components/Runner_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "FileAccess/Zenith_FileAccess.h"
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

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#endif

// ============================================================================
// Runner Resources - Global access for behaviours
// ============================================================================
namespace Runner
{
	// Geometry assets (registry-managed via handles)
	MeshGeometryHandle g_xCapsuleAsset;
	MeshGeometryHandle g_xCubeAsset;
	MeshGeometryHandle g_xSphereAsset;

	// Convenience pointers to underlying geometry (set in init via .GetDirect()->GetGeometry())
	Flux_MeshGeometry* g_pxCapsuleGeometry = nullptr;
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;

	MaterialHandle g_xCharacterMaterial;
	MaterialHandle g_xGroundMaterial;
	MaterialHandle g_xObstacleMaterial;
	MaterialHandle g_xCollectibleMaterial;
	MaterialHandle g_xDustMaterial;
	MaterialHandle g_xCollectParticleMaterial;

	PrefabHandle g_xCharacterPrefab;
	PrefabHandle g_xGroundPrefab;
	PrefabHandle g_xObstaclePrefab;
	PrefabHandle g_xCollectiblePrefab;
	PrefabHandle g_xParticlePrefab;
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
	{
		Zenith_MeshGeometryAsset* pxCapsuleAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Flux_MeshGeometry* pxCapsule = new Flux_MeshGeometry();
		GenerateCapsule(*pxCapsule, 0.4f, 1.8f, 16, 12);
		pxCapsuleAsset->SetGeometry(pxCapsule);
		g_xCapsuleAsset.Set(pxCapsuleAsset);
		g_pxCapsuleGeometry = pxCapsuleAsset->GetGeometry();
	}

	// Create cube geometry for obstacles and ground - use registry's cached unit cube
	g_xCubeAsset.Set(Zenith_MeshGeometryAsset::CreateUnitCube());
	g_pxCubeGeometry = g_xCubeAsset.GetDirect()->GetGeometry();

	// Create sphere geometry for collectibles and particles - custom size, tracked through registry
	{
		Zenith_MeshGeometryAsset* pxSphereAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Flux_MeshGeometry* pxSphere = new Flux_MeshGeometry();
		GenerateUVSphere(*pxSphere, 0.5f, 16, 12);
		pxSphereAsset->SetGeometry(pxSphere);
		g_xSphereAsset.Set(pxSphereAsset);
		g_pxSphereGeometry = pxSphereAsset->GetGeometry();
	}

	// Use grid pattern texture with BaseColor for all materials.
	const TextureHandle& xGridTex = Flux_Graphics::s_xGridTexture;

	// Create materials with grid texture and BaseColor
	g_xCharacterMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xCharacterMaterial.GetDirect()->SetName("RunnerCharacter");
	g_xCharacterMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xCharacterMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

	g_xGroundMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xGroundMaterial.GetDirect()->SetName("RunnerGround");
	g_xGroundMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xGroundMaterial.GetDirect()->SetBaseColor({ 102.f/255.f, 77.f/255.f, 51.f/255.f, 1.f });

	g_xObstacleMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xObstacleMaterial.GetDirect()->SetName("RunnerObstacle");
	g_xObstacleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xObstacleMaterial.GetDirect()->SetBaseColor({ 204.f/255.f, 51.f/255.f, 51.f/255.f, 1.f });

	g_xCollectibleMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xCollectibleMaterial.GetDirect()->SetName("RunnerCollectible");
	g_xCollectibleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xCollectibleMaterial.GetDirect()->SetBaseColor({ 255.f/255.f, 215.f/255.f, 0.f/255.f, 1.f });

	g_xDustMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xDustMaterial.GetDirect()->SetName("RunnerDust");
	g_xDustMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xDustMaterial.GetDirect()->SetBaseColor({ 180.f/255.f, 150.f/255.f, 100.f/255.f, 1.f });

	g_xCollectParticleMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	g_xCollectParticleMaterial.GetDirect()->SetName("RunnerCollectParticle");
	g_xCollectParticleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	g_xCollectParticleMaterial.GetDirect()->SetBaseColor({ 255.f/255.f, 255.f/255.f, 150.f/255.f, 1.f });

	// Create prefabs for runtime instantiation.
	// Use the persistent scene here: InitializeResources runs before the initial scene
	// loads, and (post-A6) GetActiveScene returns INVALID until that happens.
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetPersistentScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Character prefab
	{
		Zenith_Entity xCharTemplate(pxSceneData, "CharacterTemplate");
		Zenith_Prefab* pxChar = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxChar->CreateFromEntity(xCharTemplate, "Runner");
		g_xCharacterPrefab.Set(pxChar);
		Zenith_SceneManager::Destroy(xCharTemplate);
	}

	// Ground prefab
	{
		Zenith_Entity xGroundTemplate(pxSceneData, "GroundTemplate");
		Zenith_Prefab* pxGround = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxGround->CreateFromEntity(xGroundTemplate, "Ground");
		g_xGroundPrefab.Set(pxGround);
		Zenith_SceneManager::Destroy(xGroundTemplate);
	}

	// Obstacle prefab
	{
		Zenith_Entity xObstacleTemplate(pxSceneData, "ObstacleTemplate");
		Zenith_Prefab* pxObstacle = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxObstacle->CreateFromEntity(xObstacleTemplate, "Obstacle");
		g_xObstaclePrefab.Set(pxObstacle);
		Zenith_SceneManager::Destroy(xObstacleTemplate);
	}

	// Collectible prefab
	{
		Zenith_Entity xCollectibleTemplate(pxSceneData, "CollectibleTemplate");
		Zenith_Prefab* pxCollectible = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxCollectible->CreateFromEntity(xCollectibleTemplate, "Collectible");
		g_xCollectiblePrefab.Set(pxCollectible);
		Zenith_SceneManager::Destroy(xCollectibleTemplate);
	}

	// Particle prefab
	{
		Zenith_Entity xParticleTemplate(pxSceneData, "ParticleTemplate");
		Zenith_Prefab* pxParticle = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxParticle->CreateFromEntity(xParticleTemplate, "Particle");
		g_xParticlePrefab.Set(pxParticle);
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

const char* Project_GetGameAssetsDir() { return GAME_ASSETS_DIR; }

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup
	InitializeRunnerResources();

	// Runner_Behaviour auto-registers via ZENITH_BEHAVIOUR_TYPE_NAME
}

void Project_Shutdown()
{
	// Drop asset handle refs before Zenith_AssetRegistry::Shutdown teardown.
	Runner::g_xCapsuleAsset.Clear();
	Runner::g_xCubeAsset.Clear();
	Runner::g_xSphereAsset.Clear();
	Runner::g_xCharacterMaterial.Clear();
	Runner::g_xGroundMaterial.Clear();
	Runner::g_xObstacleMaterial.Clear();
	Runner::g_xCollectibleMaterial.Clear();
	Runner::g_xDustMaterial.Clear();
	Runner::g_xCollectParticleMaterial.Clear();
	Runner::g_xCharacterPrefab.Clear();
	Runner::g_xGroundPrefab.Clear();
	Runner::g_xObstaclePrefab.Clear();
	Runner::g_xCollectiblePrefab.Clear();
	Runner::g_xParticlePrefab.Clear();
}

void Project_LoadInitialScene(); // Forward declaration for automation steps

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All resources initialized in Project_RegisterScriptBehaviours
}

void Project_RegisterEditorAutomationSteps()
{
	// --- MainMenu scene (build index 0) ---
	Zenith_EditorAutomation::AddStep_CreateScene("MainMenu");
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 4.f, -8.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.3f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(60.f));
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AddUI();
	// MenuTitle: Center, fontSize=90, alignment=Center
	Zenith_EditorAutomation::AddStep_CreateUIText("MenuTitle", "ENDLESS RUNNER");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("MenuTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuTitle", 90.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("MenuTitle", 0.3f, 0.6f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_CreateUIButton("MenuPlay", "Play");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	Zenith_EditorAutomation::AddStep_AttachScript("Runner_Behaviour");
	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// --- Runner scene (build index 1) ---
	Zenith_EditorAutomation::AddStep_CreateScene("Runner");
	Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(0.f, 4.f, -8.f);
	Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.3f);
	Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(60.f));
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::AddStep_AddUI();

	// HUD UI: marginLeft=30, marginTop=30, baseTextSize=15, lineHeight=28
	// Title: TopLeft, (30, 30), fontSize=72, white, alignment=Left, hidden
	Zenith_EditorAutomation::AddStep_CreateUIText("Title", "ENDLESS RUNNER");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Title", 30.f, 30.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Title", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Title", 72.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("Title", false);

	// Distance: TopLeft, (30, 100), fontSize=90, white, hidden
	Zenith_EditorAutomation::AddStep_CreateUIText("Distance", "0m");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Distance", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Distance", 30.f, 100.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Distance", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Distance", 90.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Distance", 1.f, 1.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("Distance", false);

	// Score: TopLeft, (30, 170), fontSize=45, blue-ish, hidden
	Zenith_EditorAutomation::AddStep_CreateUIText("Score", "Score: 0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Score", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Score", 30.f, 170.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Score", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Score", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Score", 0.6f, 0.8f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("Score", false);

	// HighScore: TopLeft, (30, 198), fontSize=45, gold, hidden
	Zenith_EditorAutomation::AddStep_CreateUIText("HighScore", "Best: 0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("HighScore", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("HighScore", 30.f, 198.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("HighScore", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIFontSize("HighScore", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("HighScore", 1.f, 0.84f, 0.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("HighScore", false);

	// Speed: TopLeft, (30, 226), fontSize=45, blue-ish, hidden
	Zenith_EditorAutomation::AddStep_CreateUIText("Speed", "Speed: 15.0");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Speed", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Speed", 30.f, 226.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Speed", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Speed", 45.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Speed", 0.6f, 0.8f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("Speed", false);

	// Controls: TopLeft, (30, 282), fontSize=37.5, gray, hidden
	Zenith_EditorAutomation::AddStep_CreateUIText("Controls", "A/D: Lanes | Space/W: Jump | S: Slide | R: Reset | Esc: Menu");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Controls", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Controls", 30.f, 282.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Controls", static_cast<int>(Zenith_UI::TextAlignment::Left));
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Controls", 37.5f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Controls", 0.7f, 0.7f, 0.7f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("Controls", false);

	// Status: Center, (0, 0), alignment=Center, fontSize=75, red, hidden
	Zenith_EditorAutomation::AddStep_CreateUIText("Status", "");
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIPosition("Status", 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Status", 75.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Status", 1.f, 0.3f, 0.3f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("Status", false);

	// Script
	Zenith_EditorAutomation::AddStep_AttachScript("Runner_Behaviour");
	Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Runner" ZENITH_SCENE_EXT);
	Zenith_EditorAutomation::AddStep_UnloadScene();

	// ---- Final scene loading ----
	Zenith_EditorAutomation::AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	Zenith_SceneManager::RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Runner" ZENITH_SCENE_EXT);
	Zenith_SceneManager::LoadSceneByIndexBlockingForBootstrap(0, SCENE_LOAD_SINGLE);
}
