#include "Zenith.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Runner/Components/Runner_Behaviour.h"
#include "Runner/Components/Runner_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
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
	static RunnerResources g_xResources;
	RunnerResources& Resources() { return g_xResources; }
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
	g_xEngine.VulkanMemory().InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	g_xEngine.VulkanMemory().InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
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
		Resources().m_xCapsuleAsset.Set(pxCapsuleAsset);
		Resources().m_pxCapsuleGeometry = pxCapsuleAsset->GetGeometry();
	}

	// Create cube geometry for obstacles and ground - use registry's cached unit cube
	Resources().m_xCubeAsset.Set(Zenith_MeshGeometryAsset::CreateUnitCube());
	Resources().m_pxCubeGeometry = Resources().m_xCubeAsset.GetDirect()->GetGeometry();

	// Create sphere geometry for collectibles and particles - custom size, tracked through registry
	{
		Zenith_MeshGeometryAsset* pxSphereAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Flux_MeshGeometry* pxSphere = new Flux_MeshGeometry();
		GenerateUVSphere(*pxSphere, 0.5f, 16, 12);
		pxSphereAsset->SetGeometry(pxSphere);
		Resources().m_xSphereAsset.Set(pxSphereAsset);
		Resources().m_pxSphereGeometry = pxSphereAsset->GetGeometry();
	}

	// Use grid pattern texture with BaseColor for all materials.
	const TextureHandle& xGridTex = g_xEngine.FluxGraphics().m_xGridTexture;

	// Create materials with grid texture and BaseColor
	Resources().m_xCharacterMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xCharacterMaterial.GetDirect()->SetName("RunnerCharacter");
	Resources().m_xCharacterMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xCharacterMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

	Resources().m_xGroundMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xGroundMaterial.GetDirect()->SetName("RunnerGround");
	Resources().m_xGroundMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xGroundMaterial.GetDirect()->SetBaseColor({ 102.f/255.f, 77.f/255.f, 51.f/255.f, 1.f });

	Resources().m_xObstacleMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xObstacleMaterial.GetDirect()->SetName("RunnerObstacle");
	Resources().m_xObstacleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xObstacleMaterial.GetDirect()->SetBaseColor({ 204.f/255.f, 51.f/255.f, 51.f/255.f, 1.f });

	Resources().m_xCollectibleMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xCollectibleMaterial.GetDirect()->SetName("RunnerCollectible");
	Resources().m_xCollectibleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xCollectibleMaterial.GetDirect()->SetBaseColor({ 255.f/255.f, 215.f/255.f, 0.f/255.f, 1.f });

	Resources().m_xDustMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xDustMaterial.GetDirect()->SetName("RunnerDust");
	Resources().m_xDustMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xDustMaterial.GetDirect()->SetBaseColor({ 180.f/255.f, 150.f/255.f, 100.f/255.f, 1.f });

	Resources().m_xCollectParticleMaterial.Set(Zenith_AssetRegistry::Create<Zenith_MaterialAsset>());
	Resources().m_xCollectParticleMaterial.GetDirect()->SetName("RunnerCollectParticle");
	Resources().m_xCollectParticleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xCollectParticleMaterial.GetDirect()->SetBaseColor({ 255.f/255.f, 255.f/255.f, 150.f/255.f, 1.f });

	// Create prefabs for runtime instantiation.
	// Use the persistent scene here: InitializeResources runs before the initial scene
	// loads, and (post-A6) GetActiveScene returns INVALID until that happens.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetPersistentScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Character prefab
	{
		Zenith_Entity xCharTemplate(pxSceneData, "CharacterTemplate");
		Zenith_Prefab* pxChar = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxChar->CreateFromEntity(xCharTemplate, "Runner");
		Resources().m_xCharacterPrefab.Set(pxChar);
		g_xEngine.Scenes().Destroy(xCharTemplate);
	}

	// Ground prefab
	{
		Zenith_Entity xGroundTemplate(pxSceneData, "GroundTemplate");
		Zenith_Prefab* pxGround = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxGround->CreateFromEntity(xGroundTemplate, "Ground");
		Resources().m_xGroundPrefab.Set(pxGround);
		g_xEngine.Scenes().Destroy(xGroundTemplate);
	}

	// Obstacle prefab
	{
		Zenith_Entity xObstacleTemplate(pxSceneData, "ObstacleTemplate");
		Zenith_Prefab* pxObstacle = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxObstacle->CreateFromEntity(xObstacleTemplate, "Obstacle");
		Resources().m_xObstaclePrefab.Set(pxObstacle);
		g_xEngine.Scenes().Destroy(xObstacleTemplate);
	}

	// Collectible prefab
	{
		Zenith_Entity xCollectibleTemplate(pxSceneData, "CollectibleTemplate");
		Zenith_Prefab* pxCollectible = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxCollectible->CreateFromEntity(xCollectibleTemplate, "Collectible");
		Resources().m_xCollectiblePrefab.Set(pxCollectible);
		g_xEngine.Scenes().Destroy(xCollectibleTemplate);
	}

	// Particle prefab
	{
		Zenith_Entity xParticleTemplate(pxSceneData, "ParticleTemplate");
		Zenith_Prefab* pxParticle = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxParticle->CreateFromEntity(xParticleTemplate, "Particle");
		Resources().m_xParticlePrefab.Set(pxParticle);
		g_xEngine.Scenes().Destroy(xParticleTemplate);
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
	Runner::Resources().m_xCapsuleAsset.Clear();
	Runner::Resources().m_xCubeAsset.Clear();
	Runner::Resources().m_xSphereAsset.Clear();
	Runner::Resources().m_xCharacterMaterial.Clear();
	Runner::Resources().m_xGroundMaterial.Clear();
	Runner::Resources().m_xObstacleMaterial.Clear();
	Runner::Resources().m_xCollectibleMaterial.Clear();
	Runner::Resources().m_xDustMaterial.Clear();
	Runner::Resources().m_xCollectParticleMaterial.Clear();
	Runner::Resources().m_xCharacterPrefab.Clear();
	Runner::Resources().m_xGroundPrefab.Clear();
	Runner::Resources().m_xObstaclePrefab.Clear();
	Runner::Resources().m_xCollectiblePrefab.Clear();
	Runner::Resources().m_xParticlePrefab.Clear();
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
	g_xEngine.EditorAutomation().AddStep_CreateScene("MainMenu");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 4.f, -8.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.3f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(60.f));
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();
	// MenuTitle: Center, fontSize=90, alignment=Center
	g_xEngine.EditorAutomation().AddStep_CreateUIText("MenuTitle", "ENDLESS RUNNER");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuTitle", 0.f, -120.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("MenuTitle", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("MenuTitle", 90.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("MenuTitle", 0.3f, 0.6f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("MenuPlay", "Play");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("MenuPlay", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("MenuPlay", 200.f, 50.f);
	g_xEngine.EditorAutomation().AddStep_AttachScript("Runner_Behaviour");
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// --- Runner scene (build index 1) ---
	g_xEngine.EditorAutomation().AddStep_CreateScene("Runner");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("GameManager");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(0.f, 4.f, -8.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(-0.3f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(glm::radians(60.f));
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().AddStep_AddUI();

	// HUD UI: marginLeft=30, marginTop=30, baseTextSize=15, lineHeight=28
	// Title: TopLeft, (30, 30), fontSize=72, white, alignment=Left, hidden
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Title", "ENDLESS RUNNER");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Title", 30.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Title", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Title", 72.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Title", false);

	// Distance: TopLeft, (30, 100), fontSize=90, white, hidden
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Distance", "0m");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Distance", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Distance", 30.f, 100.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Distance", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Distance", 90.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Distance", 1.f, 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Distance", false);

	// Score: TopLeft, (30, 170), fontSize=45, blue-ish, hidden
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Score", "Score: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Score", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Score", 30.f, 170.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Score", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Score", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Score", 0.6f, 0.8f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Score", false);

	// HighScore: TopLeft, (30, 198), fontSize=45, gold, hidden
	g_xEngine.EditorAutomation().AddStep_CreateUIText("HighScore", "Best: 0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("HighScore", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("HighScore", 30.f, 198.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("HighScore", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("HighScore", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("HighScore", 1.f, 0.84f, 0.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("HighScore", false);

	// Speed: TopLeft, (30, 226), fontSize=45, blue-ish, hidden
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Speed", "Speed: 15.0");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Speed", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Speed", 30.f, 226.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Speed", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Speed", 45.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Speed", 0.6f, 0.8f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Speed", false);

	// Controls: TopLeft, (30, 282), fontSize=37.5, gray, hidden
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Controls", "A/D: Lanes | Space/W: Jump | S: Slide | R: Reset | Esc: Menu");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Controls", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Controls", 30.f, 282.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Controls", static_cast<int>(Zenith_UI::TextAlignment::Left));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Controls", 37.5f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Controls", 0.7f, 0.7f, 0.7f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Controls", false);

	// Status: Center, (0, 0), alignment=Center, fontSize=75, red, hidden
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Status", "");
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Status", 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Status", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Status", 75.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Status", 1.f, 0.3f, 0.3f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Status", false);

	// Script
	g_xEngine.EditorAutomation().AddStep_AttachScript("Runner_Behaviour");
	g_xEngine.EditorAutomation().AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Runner" ZENITH_SCENE_EXT);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();

	// ---- Final scene loading ----
	g_xEngine.EditorAutomation().AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Runner" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
