#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Runner/Components/Runner_GameComponent.h"
#include "Runner/Components/Runner_GraphNodes.h"
#include "Runner/Components/Runner_Config.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
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
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Input/Zenith_KeyCodes.h"
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
	g_xEngine.FluxMemory().InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	g_xEngine.FluxMemory().InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
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

	g_xEngine.FluxMemory().InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	g_xEngine.FluxMemory().InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
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
		auto xhCapsuleAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Zenith_MeshGeometryAsset* pxCapsuleAsset = xhCapsuleAsset.GetDirect();
		Flux_MeshGeometry* pxCapsule = new Flux_MeshGeometry();
		GenerateCapsule(*pxCapsule, 0.4f, 1.8f, 16, 12);
		pxCapsuleAsset->SetGeometry(pxCapsule);
		Resources().m_xCapsuleAsset.Set(pxCapsuleAsset);
		Resources().m_pxCapsuleGeometry = pxCapsuleAsset->GetGeometry();
	}

	// Create cube geometry for obstacles and ground - use registry's cached unit cube
	Resources().m_xCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	Resources().m_pxCubeGeometry = Resources().m_xCubeAsset.GetDirect()->GetGeometry();

	// Create sphere geometry for collectibles and particles - custom size, tracked through registry
	{
		auto xhSphereAsset = Zenith_AssetRegistry::Create<Zenith_MeshGeometryAsset>();
		Zenith_MeshGeometryAsset* pxSphereAsset = xhSphereAsset.GetDirect();
		Flux_MeshGeometry* pxSphere = new Flux_MeshGeometry();
		GenerateUVSphere(*pxSphere, 0.5f, 16, 12);
		pxSphereAsset->SetGeometry(pxSphere);
		Resources().m_xSphereAsset.Set(pxSphereAsset);
		Resources().m_pxSphereGeometry = pxSphereAsset->GetGeometry();
	}

	// Use grid pattern texture with BaseColor for all materials.
	const TextureHandle& xGridTex = g_xEngine.FluxGraphics().m_xGridTexture;

	// Create materials with grid texture and BaseColor
	Resources().m_xCharacterMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xCharacterMaterial.GetDirect()->SetName("RunnerCharacter");
	Resources().m_xCharacterMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xCharacterMaterial.GetDirect()->SetBaseColor({ 51.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

	Resources().m_xGroundMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xGroundMaterial.GetDirect()->SetName("RunnerGround");
	Resources().m_xGroundMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xGroundMaterial.GetDirect()->SetBaseColor({ 102.f/255.f, 77.f/255.f, 51.f/255.f, 1.f });

	Resources().m_xObstacleMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xObstacleMaterial.GetDirect()->SetName("RunnerObstacle");
	Resources().m_xObstacleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xObstacleMaterial.GetDirect()->SetBaseColor({ 204.f/255.f, 51.f/255.f, 51.f/255.f, 1.f });

	Resources().m_xCollectibleMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xCollectibleMaterial.GetDirect()->SetName("RunnerCollectible");
	Resources().m_xCollectibleMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xCollectibleMaterial.GetDirect()->SetBaseColor({ 255.f/255.f, 215.f/255.f, 0.f/255.f, 1.f });

	Resources().m_xDustMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
	Resources().m_xDustMaterial.GetDirect()->SetName("RunnerDust");
	Resources().m_xDustMaterial.GetDirect()->SetDiffuseTexture(xGridTex);
	Resources().m_xDustMaterial.GetDirect()->SetBaseColor({ 180.f/255.f, 150.f/255.f, 100.f/255.f, 1.f });

	Resources().m_xCollectParticleMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
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
		Zenith_Entity xCharTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "CharacterTemplate");
		auto xhChar = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxChar = xhChar.GetDirect();
		pxChar->CreateFromEntity(xCharTemplate, "Runner");
		Resources().m_xCharacterPrefab.Set(pxChar);
		xCharTemplate.Destroy();
	}

	// Ground prefab
	{
		Zenith_Entity xGroundTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "GroundTemplate");
		auto xhGround = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxGround = xhGround.GetDirect();
		pxGround->CreateFromEntity(xGroundTemplate, "Ground");
		Resources().m_xGroundPrefab.Set(pxGround);
		xGroundTemplate.Destroy();
	}

	// Obstacle prefab
	{
		Zenith_Entity xObstacleTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "ObstacleTemplate");
		auto xhObstacle = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxObstacle = xhObstacle.GetDirect();
		pxObstacle->CreateFromEntity(xObstacleTemplate, "Obstacle");
		Resources().m_xObstaclePrefab.Set(pxObstacle);
		xObstacleTemplate.Destroy();
	}

	// Collectible prefab
	{
		Zenith_Entity xCollectibleTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "CollectibleTemplate");
		auto xhCollectible = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxCollectible = xhCollectible.GetDirect();
		pxCollectible->CreateFromEntity(xCollectibleTemplate, "Collectible");
		Resources().m_xCollectiblePrefab.Set(pxCollectible);
		xCollectibleTemplate.Destroy();
	}

	// Particle prefab
	{
		Zenith_Entity xParticleTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "ParticleTemplate");
		auto xhParticle = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		Zenith_Prefab* pxParticle = xhParticle.GetDirect();
		pxParticle->CreateFromEntity(xParticleTemplate, "Particle");
		Resources().m_xParticlePrefab.Set(pxParticle);
		xParticleTemplate.Destroy();
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

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterGameComponents()
{
	// Initialize resources at startup
	InitializeRunnerResources();

	// Register the Runner game component with the component-meta registry
	// (serialization/lifecycle) and, in tools builds, the editor "Add Component"
	// registry (display name used by AddStep_AddComponent / the editor menu).
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
	xRegistry.RegisterComponent<Runner_GameComponent>("RunnerGame", 100);
	// The character shim (static-scope wrapper the Runner_CharacterActions
	// graph resolves through) - added at runtime by CreateCharacter.
	xRegistry.RegisterComponent<Runner_CharacterShim>("RunnerCharacter", 101);
	Runner_RegisterGraphNodes();
#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<Runner_GameComponent>("RunnerGame");
#endif
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
	// All resources initialized in Project_RegisterGameComponents
}

namespace
{
	// Runner_RunFlow: the gameplay GameManager's graph. OWNS the run's game
	// state (gameState int; score/highScore FLOATS so the accumulate/max
	// arithmetic is engine float nodes); C++ reads it back through the
	// Runner_GameComponent accessors.
	//
	// Anchors, in dispatch order:
	//   OnStart              - show the HUD (the old StartGame visibility).
	//   OnCustomEvent chains - "RunTick", fired once per PLAYING frame at the
	//       old decision block's callsite. Node order = the old same-frame
	//       decision order: stage facts -> scoring -> obstacle -> dead.
	//   OnKeyPressed chains  - the old top-of-PLAYING key decisions.
	//   StateMachine (LAST)  - reactive dispatcher on gameState; its
	//       RunnerEnter_Paused / RunnerExit_Paused transition events drive
	//       the run-scene pause.
	void BuildGraph_RunnerRunFlow(Zenith_GraphBuilder& xBuilder)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(static_cast<int32_t>(RunnerGameState::PLAYING));
		xBuilder.Variable("gameState", xValue);
		xValue.SetFloat(0.0f);
		xBuilder.Variable("score", xValue);
		xBuilder.Variable("highScore", xValue);

		// ---- OnStart: show the HUD ----------------------------------------
		const char* aszHUD[] = { "Title", "Distance", "Score", "HighScore", "Speed", "Controls", "Status" };
		u_int uPrevious = xBuilder.Node("OnStart");
		for (const char* szElement : aszHUD)
		{
			const u_int uShow = xBuilder.Node("SetUIVisible");
			xBuilder.ParamString(uShow, "m_strElement", szElement);
			xBuilder.ParamBool(uShow, "m_bVisible", true);
			xBuilder.Chain(uPrevious, uShow);
			uPrevious = uShow;
		}

		// ---- RunTick 1: publish the frame's systems results -----------------
		const u_int uTickStage = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uTickStage, "m_strEventName", "RunTick");
		const u_int uStage = xBuilder.Node("RunnerStageFrameResults");
		xBuilder.Chain(uTickStage, uStage);

		// ---- RunTick 2: scoring (score += the frame's pickup points) --------
		const u_int uTickScore = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uTickScore, "m_strEventName", "RunTick");
		const u_int uAddScore = xBuilder.Node("AddBlackboardFloat");
		xBuilder.ParamString(uAddScore, "m_strVariable", "score");
		xBuilder.ParamString(uAddScore, "m_strDeltaVar", "pointsGained");
		xBuilder.Chain(uTickScore, uAddScore);

		// ---- RunTick 3: obstacle hit -> kill + GAME_OVER + high-score sync --
		const u_int uTickObstacle = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uTickObstacle, "m_strEventName", "RunTick");
		const u_int uBrHit = xBuilder.Node("Branch");
		xBuilder.ParamString(uBrHit, "m_strConditionVar", "obstacleHit");
		const u_int uKill = xBuilder.Node("RunnerKillCharacter");
		const u_int uHitOver = xBuilder.Node("SetBlackboardInt");
		xBuilder.ParamString(uHitOver, "m_strVariable", "gameState");
		xBuilder.ParamInt(uHitOver, "m_iValue", static_cast<int32_t>(RunnerGameState::GAME_OVER));
		const u_int uHitSync = xBuilder.Node("MathBlackboardFloat");
		xBuilder.ParamString(uHitSync, "m_strVar", "score");
		xBuilder.ParamInt(uHitSync, "m_iOp", 5);	// max
		xBuilder.ParamString(uHitSync, "m_strOperandVar", "highScore");
		xBuilder.ParamString(uHitSync, "m_strResultVar", "highScore");
		xBuilder.Chain(uTickObstacle, uBrHit);
		xBuilder.Edge(uBrHit, 0, uKill);
		xBuilder.Chain(uKill, uHitOver).Chain(uHitOver, uHitSync);

		// ---- RunTick 4: character dead -> GAME_OVER ------------------------
		// Deliberately WITHOUT a high-score sync - the pre-graph quirk,
		// preserved structurally (this chain simply has no max node).
		const u_int uTickDead = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uTickDead, "m_strEventName", "RunTick");
		const u_int uBrDead = xBuilder.Node("Branch");
		xBuilder.ParamString(uBrDead, "m_strConditionVar", "characterDead");
		const u_int uDeadOver = xBuilder.Node("SetBlackboardInt");
		xBuilder.ParamString(uDeadOver, "m_strVariable", "gameState");
		xBuilder.ParamInt(uDeadOver, "m_iValue", static_cast<int32_t>(RunnerGameState::GAME_OVER));
		xBuilder.Chain(uTickDead, uBrDead);
		xBuilder.Edge(uBrDead, 0, uDeadOver);

		// ---- P: pause toggle (PLAYING <-> PAUSED) ---------------------------
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", ZENITH_KEY_P);
			const u_int uSwitch = xBuilder.Node("SwitchOnInt");
			xBuilder.ParamString(uSwitch, "m_strVar", "gameState");
			xBuilder.ParamInt(uSwitch, "m_iCaseCount", 4);
			xBuilder.Chain(uKey, uSwitch);
			const u_int uToPaused = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uToPaused, "m_strVariable", "gameState");
			xBuilder.ParamInt(uToPaused, "m_iValue", static_cast<int32_t>(RunnerGameState::PAUSED));
			xBuilder.Edge(uSwitch, static_cast<u_int>(RunnerGameState::PLAYING), uToPaused);
			const u_int uToPlaying = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uToPlaying, "m_strVariable", "gameState");
			xBuilder.ParamInt(uToPlaying, "m_iValue", static_cast<int32_t>(RunnerGameState::PLAYING));
			xBuilder.Edge(uSwitch, static_cast<u_int>(RunnerGameState::PAUSED), uToPlaying);
		}

		// ---- R: reset (PLAYING/GAME_OVER - every run state except PAUSED) ---
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", ZENITH_KEY_R);
			const u_int uCanReset = xBuilder.Node("CompareBlackboardInt");
			xBuilder.ParamString(uCanReset, "m_strVar", "gameState");
			xBuilder.ParamInt(uCanReset, "m_iCompareTo", static_cast<int32_t>(RunnerGameState::PAUSED));
			xBuilder.ParamInt(uCanReset, "m_iOp", 5);	// notEqual
			xBuilder.ParamString(uCanReset, "m_strResultVar", "canReset");
			const u_int uBrReset = xBuilder.Node("Branch");
			xBuilder.ParamString(uBrReset, "m_strConditionVar", "canReset");
			const u_int uSync = xBuilder.Node("MathBlackboardFloat");
			xBuilder.ParamString(uSync, "m_strVar", "score");
			xBuilder.ParamInt(uSync, "m_iOp", 5);	// max
			xBuilder.ParamString(uSync, "m_strOperandVar", "highScore");
			xBuilder.ParamString(uSync, "m_strResultVar", "highScore");
			const u_int uRegen = xBuilder.Node("RunnerRegenerateRun");
			const u_int uResetScore = xBuilder.Node("SetBlackboardFloat");
			xBuilder.ParamString(uResetScore, "m_strVariable", "score");
			xBuilder.ParamFloat(uResetScore, "m_fValue", 0.0f);
			const u_int uResetState = xBuilder.Node("SetBlackboardInt");
			xBuilder.ParamString(uResetState, "m_strVariable", "gameState");
			xBuilder.ParamInt(uResetState, "m_iValue", static_cast<int32_t>(RunnerGameState::PLAYING));
			xBuilder.Chain(uKey, uCanReset).Chain(uCanReset, uBrReset);
			xBuilder.Edge(uBrReset, 0, uSync);
			xBuilder.Chain(uSync, uRegen).Chain(uRegen, uResetScore).Chain(uResetScore, uResetState);
		}

		// ---- Esc: back to the menu from ANY run state (sync high score,
		// ---- tear down, SINGLE load at the end of the chain) -----------------
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", ZENITH_KEY_ESCAPE);
			const u_int uSync = xBuilder.Node("MathBlackboardFloat");
			xBuilder.ParamString(uSync, "m_strVar", "score");
			xBuilder.ParamInt(uSync, "m_iOp", 5);	// max
			xBuilder.ParamString(uSync, "m_strOperandVar", "highScore");
			xBuilder.ParamString(uSync, "m_strResultVar", "highScore");
			const u_int uUnload = xBuilder.Node("RunnerUnloadRun");
			const u_int uMenu = xBuilder.Node("LoadSceneByIndex");
			xBuilder.ParamInt(uMenu, "m_iSceneIndex", 0);
			xBuilder.Chain(uKey, uSync).Chain(uSync, uUnload).Chain(uUnload, uMenu);
		}

		// ---- Pause side effects: StateMachine transition events -------------
		{
			const u_int uEnterPaused = xBuilder.Node("OnCustomEvent");
			xBuilder.ParamString(uEnterPaused, "m_strEventName", "RunnerEnter_Paused");
			const u_int uDoPause = xBuilder.Node("RunnerSetRunPaused");
			xBuilder.ParamBool(uDoPause, "m_bPaused", true);
			xBuilder.Chain(uEnterPaused, uDoPause);

			const u_int uExitPaused = xBuilder.Node("OnCustomEvent");
			xBuilder.ParamString(uExitPaused, "m_strEventName", "RunnerExit_Paused");
			const u_int uDoResume = xBuilder.Node("RunnerSetRunPaused");
			xBuilder.ParamBool(uDoResume, "m_bPaused", false);
			xBuilder.Chain(uExitPaused, uDoResume);
		}

		// ---- Reactive StateMachine, LAST in the ON_UPDATE dispatch ----------
		{
			const u_int uTick = xBuilder.Node("OnUpdate");
			const u_int uMachine = xBuilder.Node("StateMachine");
			xBuilder.ParamString(uMachine, "m_strStateVar", "gameState");
			xBuilder.ParamInt(uMachine, "m_iStateCount", 4);
			xBuilder.ParamString(uMachine, "m_strStateNames", "Menu,Playing,Paused,GameOver");
			xBuilder.ParamString(uMachine, "m_strEventPrefix", "Runner");
			xBuilder.Chain(uTick, uMachine);
		}
	}

	// Runner_GameFlow: the menu GameManager's graph. gameState pinned at
	// MAIN_MENU for the shim accessors; the single Play button holds focus
	// every frame and drives the scene load through the engine's UIButton
	// trampoline source (keyboard Enter activation included).
	void BuildGraph_RunnerGameFlow(Zenith_GraphBuilder& xBuilder)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(static_cast<int32_t>(RunnerGameState::MAIN_MENU));
		xBuilder.Variable("gameState", xValue);

		const u_int uPlayClicked = xBuilder.Node("OnUIButtonClicked");
		xBuilder.ParamString(uPlayClicked, "m_strButton", "MenuPlay");
		const u_int uLoadGame = xBuilder.Node("LoadSceneByIndex");
		xBuilder.ParamInt(uLoadGame, "m_iSceneIndex", 1);
		xBuilder.Chain(uPlayClicked, uLoadGame);

		const u_int uTick = xBuilder.Node("OnUpdate");
		const u_int uFocus = xBuilder.Node("RunnerFocusPlayButton");
		xBuilder.Chain(uTick, uFocus);
	}

	// Runner_CharacterActions: the character entity's graph (runtime-attached
	// by CreateCharacter; nodes resolve Runner_CharacterShim - the
	// static-scope -> shim-wrapper pilot).
	//
	//   OnKeyPressed chains - which input triggers which Try* gate (the old
	//       HandleInput dispatch); the gate CONDITIONS stay in the controller.
	//   "CharTick" chains   - fired by the GameComponent between the
	//       controller and animation systems passes (dt payload):
	//       1. the slide-duration countdown -> EndSlide (DecrementTimer
	//          pilot; armed by RunnerTrySlide from the config duration),
	//       2. the character-state -> animation-state mapping (SwitchOnInt;
	//          the DEAD pin is deliberately unwired - keep-last-state).
	void BuildGraph_RunnerCharacterActions(Zenith_GraphBuilder& xBuilder)
	{
		Zenith_PropertyValue xValue;
		xValue.SetFloat(0.0f);
		xBuilder.Variable("slideTimer", xValue);

		// ---- Input -> Try* gates --------------------------------------------
		const int32_t aiLeftKeys[] = { ZENITH_KEY_A, ZENITH_KEY_LEFT };
		for (const int32_t iKey : aiLeftKeys)
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", iKey);
			const u_int uLane = xBuilder.Node("RunnerTrySwitchLane");
			xBuilder.ParamInt(uLane, "m_iDirection", -1);
			xBuilder.Chain(uKey, uLane);
		}
		const int32_t aiRightKeys[] = { ZENITH_KEY_D, ZENITH_KEY_RIGHT };
		for (const int32_t iKey : aiRightKeys)
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", iKey);
			const u_int uLane = xBuilder.Node("RunnerTrySwitchLane");
			xBuilder.ParamInt(uLane, "m_iDirection", 1);
			xBuilder.Chain(uKey, uLane);
		}
		const int32_t aiJumpKeys[] = { ZENITH_KEY_SPACE, ZENITH_KEY_W, ZENITH_KEY_UP };
		for (const int32_t iKey : aiJumpKeys)
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", iKey);
			const u_int uJump = xBuilder.Node("RunnerTryJump");
			xBuilder.Chain(uKey, uJump);
		}
		const int32_t aiSlideKeys[] = { ZENITH_KEY_S, ZENITH_KEY_DOWN };
		for (const int32_t iKey : aiSlideKeys)
		{
			const u_int uKey = xBuilder.Node("OnKeyPressed");
			xBuilder.ParamInt(uKey, "m_iKeyCode", iKey);
			const u_int uSlide = xBuilder.Node("RunnerTrySlide");
			xBuilder.Chain(uKey, uSlide);
		}

		// ---- CharTick 1: slide-duration countdown -> EndSlide ----------------
		// (custom-event dispatch carries no dt; the payload var is the dt)
		const u_int uTickSlide = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uTickSlide, "m_strEventName", "CharTick");
		const u_int uSubDt = xBuilder.Node("MathBlackboardFloat");
		xBuilder.ParamString(uSubDt, "m_strVar", "slideTimer");
		xBuilder.ParamInt(uSubDt, "m_iOp", 0);	// sub
		xBuilder.ParamString(uSubDt, "m_strOperandVar", "payload");
		const u_int uClamp = xBuilder.Node("ClampBlackboardFloat");
		xBuilder.ParamString(uClamp, "m_strVar", "slideTimer");
		xBuilder.ParamFloat(uClamp, "m_fMin", 0.0f);
		xBuilder.ParamFloat(uClamp, "m_fMax", 3600.0f);
		const u_int uCmpExpired = xBuilder.Node("CompareBlackboardFloat");
		xBuilder.ParamString(uCmpExpired, "m_strVar", "slideTimer");
		xBuilder.ParamFloat(uCmpExpired, "m_fCompareTo", 0.0f);
		xBuilder.ParamInt(uCmpExpired, "m_iOp", 1);	// lessEqual
		xBuilder.ParamString(uCmpExpired, "m_strResultVar", "slideExpired");
		const u_int uBrExpired = xBuilder.Node("Branch");
		xBuilder.ParamString(uBrExpired, "m_strConditionVar", "slideExpired");
		const u_int uEndSlide = xBuilder.Node("RunnerEndSlide");
		xBuilder.Chain(uTickSlide, uSubDt).Chain(uSubDt, uClamp).Chain(uClamp, uCmpExpired).Chain(uCmpExpired, uBrExpired);
		xBuilder.Edge(uBrExpired, 0, uEndSlide);

		// ---- CharTick 2: character state -> animation state -----------------
		const u_int uTickAnim = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uTickAnim, "m_strEventName", "CharTick");
		const u_int uFacts = xBuilder.Node("RunnerStageCharacterFacts");
		const u_int uSwitch = xBuilder.Node("SwitchOnInt");
		xBuilder.ParamString(uSwitch, "m_strVar", "charState");
		xBuilder.ParamInt(uSwitch, "m_iCaseCount", 4);
		xBuilder.Chain(uTickAnim, uFacts).Chain(uFacts, uSwitch);

		// RUNNING: speed > 0.1 ? RUN : IDLE
		const u_int uCmpMoving = xBuilder.Node("CompareBlackboardFloat");
		xBuilder.ParamString(uCmpMoving, "m_strVar", "speed");
		xBuilder.ParamFloat(uCmpMoving, "m_fCompareTo", 0.1f);
		xBuilder.ParamInt(uCmpMoving, "m_iOp", 2);	// greater
		xBuilder.ParamString(uCmpMoving, "m_strResultVar", "isMoving");
		const u_int uBrMoving = xBuilder.Node("Branch");
		xBuilder.ParamString(uBrMoving, "m_strConditionVar", "isMoving");
		const u_int uAnimRun = xBuilder.Node("RunnerSetAnimState");
		xBuilder.ParamInt(uAnimRun, "m_iAnimState", static_cast<int32_t>(RunnerAnimState::RUN));
		const u_int uAnimIdle = xBuilder.Node("RunnerSetAnimState");
		xBuilder.ParamInt(uAnimIdle, "m_iAnimState", static_cast<int32_t>(RunnerAnimState::IDLE));
		xBuilder.Edge(uSwitch, static_cast<u_int>(RunnerCharacterState::RUNNING), uCmpMoving);
		xBuilder.Chain(uCmpMoving, uBrMoving);
		xBuilder.Edge(uBrMoving, 0, uAnimRun);
		xBuilder.Edge(uBrMoving, 1, uAnimIdle);

		// JUMPING / SLIDING -> direct mappings; DEAD pin unwired (keep last).
		const u_int uAnimJump = xBuilder.Node("RunnerSetAnimState");
		xBuilder.ParamInt(uAnimJump, "m_iAnimState", static_cast<int32_t>(RunnerAnimState::JUMP));
		xBuilder.Edge(uSwitch, static_cast<u_int>(RunnerCharacterState::JUMPING), uAnimJump);
		const u_int uAnimSlide = xBuilder.Node("RunnerSetAnimState");
		xBuilder.ParamInt(uAnimSlide, "m_iAnimState", static_cast<int32_t>(RunnerAnimState::SLIDE));
		xBuilder.Edge(uSwitch, static_cast<u_int>(RunnerCharacterState::SLIDING), uAnimSlide);
	}
}

void Project_RegisterEditorAutomationSteps()
{
	// ---- Behaviour graphs (regenerated every boot through the programmatic
	// ---- builder - the W1 conversion's decomposed engine-node graphs) ----
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.AddStep_GraphBuild("game:Graphs/Runner_RunFlow.bgraph", &BuildGraph_RunnerRunFlow);
	xAuto.AddStep_GraphBuild("game:Graphs/Runner_GameFlow.bgraph", &BuildGraph_RunnerGameFlow);
	xAuto.AddStep_GraphBuild("game:Graphs/Runner_CharacterActions.bgraph", &BuildGraph_RunnerCharacterActions);

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
	g_xEngine.EditorAutomation().AddStep_AddComponent("RunnerGame");
	// Menu flow graph (Play click + focus + MAIN_MENU state).
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Runner_GameFlow.bgraph");
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

	// Game component
	g_xEngine.EditorAutomation().AddStep_AddComponent("RunnerGame");
	// Run-flow graph on the gameplay GameManager: RunnerGame fires "RunTick"
	// into it from the PLAYING branch of its OnUpdate.
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/Runner_RunFlow.bgraph");
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
