#include "Zenith.h"

#include "Runner/Components/Runner_Behaviour.h"
#include "Runner/Components/Runner_Config.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "Flux/Flux_Graphics.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "AssetHandling/Zenith_DataAssetManager.h"
#include "Prefab/Zenith_Prefab.h"

#include <cmath>

// ============================================================================
// Runner Resources - Global access for behaviours
// ============================================================================
namespace Runner
{
	Flux_MeshGeometry* g_pxCapsuleGeometry = nullptr;
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;

	Flux_MaterialAsset* g_pxCharacterMaterial = nullptr;
	Flux_MaterialAsset* g_pxGroundMaterial = nullptr;
	Flux_MaterialAsset* g_pxObstacleMaterial = nullptr;
	Flux_MaterialAsset* g_pxCollectibleMaterial = nullptr;
	Flux_MaterialAsset* g_pxDustMaterial = nullptr;
	Flux_MaterialAsset* g_pxCollectParticleMaterial = nullptr;

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

	// Create capsule geometry for character
	g_pxCapsuleGeometry = new Flux_MeshGeometry();
	GenerateCapsule(*g_pxCapsuleGeometry, 0.4f, 1.8f, 16, 12);

	// Create cube geometry for obstacles and ground
	g_pxCubeGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*g_pxCubeGeometry);

	// Create sphere geometry for collectibles and particles
	g_pxSphereGeometry = new Flux_MeshGeometry();
	GenerateUVSphere(*g_pxSphereGeometry, 0.5f, 16, 12);

	// Use grid pattern texture with BaseColor for all materials
	Flux_Texture* pxGridTex = &Flux_Graphics::s_xGridPatternTexture2D;

	// Create materials with grid texture and BaseColor
	g_pxCharacterMaterial = Flux_MaterialAsset::Create("RunnerCharacter");
	g_pxCharacterMaterial->SetDiffuseTexture(pxGridTex);
	g_pxCharacterMaterial->SetBaseColor({ 51.f/255.f, 153.f/255.f, 255.f/255.f, 1.f });

	g_pxGroundMaterial = Flux_MaterialAsset::Create("RunnerGround");
	g_pxGroundMaterial->SetDiffuseTexture(pxGridTex);
	g_pxGroundMaterial->SetBaseColor({ 102.f/255.f, 77.f/255.f, 51.f/255.f, 1.f });

	g_pxObstacleMaterial = Flux_MaterialAsset::Create("RunnerObstacle");
	g_pxObstacleMaterial->SetDiffuseTexture(pxGridTex);
	g_pxObstacleMaterial->SetBaseColor({ 204.f/255.f, 51.f/255.f, 51.f/255.f, 1.f });

	g_pxCollectibleMaterial = Flux_MaterialAsset::Create("RunnerCollectible");
	g_pxCollectibleMaterial->SetDiffuseTexture(pxGridTex);
	g_pxCollectibleMaterial->SetBaseColor({ 255.f/255.f, 215.f/255.f, 0.f/255.f, 1.f });

	g_pxDustMaterial = Flux_MaterialAsset::Create("RunnerDust");
	g_pxDustMaterial->SetDiffuseTexture(pxGridTex);
	g_pxDustMaterial->SetBaseColor({ 180.f/255.f, 150.f/255.f, 100.f/255.f, 1.f });

	g_pxCollectParticleMaterial = Flux_MaterialAsset::Create("RunnerCollectParticle");
	g_pxCollectParticleMaterial->SetDiffuseTexture(pxGridTex);
	g_pxCollectParticleMaterial->SetBaseColor({ 255.f/255.f, 255.f/255.f, 150.f/255.f, 1.f });

	// Create prefabs for runtime instantiation
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Character prefab
	{
		Zenith_Entity xCharTemplate(&xScene, "CharacterTemplate");
		g_pxCharacterPrefab = new Zenith_Prefab();
		g_pxCharacterPrefab->CreateFromEntity(xCharTemplate, "Runner");
		Zenith_Scene::Destroy(xCharTemplate);
	}

	// Ground prefab
	{
		Zenith_Entity xGroundTemplate(&xScene, "GroundTemplate");
		g_pxGroundPrefab = new Zenith_Prefab();
		g_pxGroundPrefab->CreateFromEntity(xGroundTemplate, "Ground");
		Zenith_Scene::Destroy(xGroundTemplate);
	}

	// Obstacle prefab
	{
		Zenith_Entity xObstacleTemplate(&xScene, "ObstacleTemplate");
		g_pxObstaclePrefab = new Zenith_Prefab();
		g_pxObstaclePrefab->CreateFromEntity(xObstacleTemplate, "Obstacle");
		Zenith_Scene::Destroy(xObstacleTemplate);
	}

	// Collectible prefab
	{
		Zenith_Entity xCollectibleTemplate(&xScene, "CollectibleTemplate");
		g_pxCollectiblePrefab = new Zenith_Prefab();
		g_pxCollectiblePrefab->CreateFromEntity(xCollectibleTemplate, "Collectible");
		Zenith_Scene::Destroy(xCollectibleTemplate);
	}

	// Particle prefab
	{
		Zenith_Entity xParticleTemplate(&xScene, "ParticleTemplate");
		g_pxParticlePrefab = new Zenith_Prefab();
		g_pxParticlePrefab->CreateFromEntity(xParticleTemplate, "Particle");
		Zenith_Scene::Destroy(xParticleTemplate);
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

	// Register DataAsset types
	RegisterRunnerDataAssets();

	Runner_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// Runner has no resources that need explicit cleanup
}

void Project_LoadInitialScene()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	xScene.Reset();

	// Create camera entity
	Zenith_Entity xCameraEntity(&xScene, "MainCamera");
	xCameraEntity.SetTransient(false);
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.f, 4.f, -8.f),   // Position: behind and above
		-0.3f,  // Pitch: looking slightly down
		0.f,    // Yaw: facing forward (+Z)
		glm::radians(60.f),   // FOV
		0.1f,
		1000.f,
		16.f / 9.f
	);
	xScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Create main game entity
	Zenith_Entity xRunnerEntity(&xScene, "RunnerGame");
	xRunnerEntity.SetTransient(false);

	// UI Setup
	static constexpr float s_fMarginLeft = 30.f;
	static constexpr float s_fMarginTop = 30.f;
	static constexpr float s_fBaseTextSize = 15.f;
	static constexpr float s_fLineHeight = 28.f;

	Zenith_UIComponent& xUI = xRunnerEntity.AddComponent<Zenith_UIComponent>();

	auto SetupTopLeftText = [](Zenith_UI::Zenith_UIText* pxText, float fYOffset)
	{
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxText->SetPosition(s_fMarginLeft, s_fMarginTop + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Left);
	};

	// Title
	Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "ENDLESS RUNNER");
	SetupTopLeftText(pxTitle, 0.f);
	pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
	pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	// Distance
	Zenith_UI::Zenith_UIText* pxDistance = xUI.CreateText("Distance", "0m");
	SetupTopLeftText(pxDistance, s_fLineHeight * 2.5f);
	pxDistance->SetFontSize(s_fBaseTextSize * 6.0f);
	pxDistance->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	// Score
	Zenith_UI::Zenith_UIText* pxScore = xUI.CreateText("Score", "Score: 0");
	SetupTopLeftText(pxScore, s_fLineHeight * 5);
	pxScore->SetFontSize(s_fBaseTextSize * 3.0f);
	pxScore->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	// High Score
	Zenith_UI::Zenith_UIText* pxHighScore = xUI.CreateText("HighScore", "Best: 0");
	SetupTopLeftText(pxHighScore, s_fLineHeight * 6);
	pxHighScore->SetFontSize(s_fBaseTextSize * 3.0f);
	pxHighScore->SetColor(Zenith_Maths::Vector4(1.f, 0.84f, 0.f, 1.f));

	// Speed
	Zenith_UI::Zenith_UIText* pxSpeed = xUI.CreateText("Speed", "Speed: 15.0");
	SetupTopLeftText(pxSpeed, s_fLineHeight * 7);
	pxSpeed->SetFontSize(s_fBaseTextSize * 3.0f);
	pxSpeed->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	// Controls hint
	Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("Controls", "A/D: Switch Lanes | Space/W: Jump | S: Slide");
	SetupTopLeftText(pxControls, s_fLineHeight * 9);
	pxControls->SetFontSize(s_fBaseTextSize * 2.5f);
	pxControls->SetColor(Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 1.f));

	// Status (center of screen for game over/pause)
	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "");
	pxStatus->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
	pxStatus->SetPosition(0.f, 0.f);
	pxStatus->SetAlignment(Zenith_UI::TextAlignment::Center);
	pxStatus->SetFontSize(s_fBaseTextSize * 5.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(1.f, 0.3f, 0.3f, 1.f));

	// Add script component with Runner behaviour
	Zenith_ScriptComponent& xScript = xRunnerEntity.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<Runner_Behaviour>();

	// Save the scene file
	std::string strScenePath = std::string(GAME_ASSETS_DIR) + "/Scenes/Runner.zscn";
	std::filesystem::create_directories(std::string(GAME_ASSETS_DIR) + "/Scenes");
	xScene.SaveToFile(strScenePath);
}
