#include "Zenith.h"

#include "TilePuzzle/Components/TilePuzzle_Types.h"
#include "TilePuzzle/Components/TilePuzzle_Behaviour.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux_Graphics.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Prefab/Zenith_Prefab.h"

// ============================================================================
// TilePuzzle Resources - Global access for behaviours
// ============================================================================
namespace TilePuzzle
{
	// Shared geometry assets (registry-managed)
	Zenith_MeshGeometryAsset* g_pxCubeAsset = nullptr;
	Zenith_MeshGeometryAsset* g_pxSphereAsset = nullptr;

	// Convenience pointers to underlying geometry (do not delete - managed by assets)
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* g_pxSphereGeometry = nullptr;

	// Floor material
	Zenith_MaterialAsset* g_pxFloorMaterial = nullptr;

	// Blocker material (static shapes)
	Zenith_MaterialAsset* g_pxBlockerMaterial = nullptr;

	// Colored shape materials (draggable)
	Zenith_MaterialAsset* g_apxShapeMaterials[TILEPUZZLE_COLOR_COUNT] = {};

	// Colored cat materials
	Zenith_MaterialAsset* g_apxCatMaterials[TILEPUZZLE_COLOR_COUNT] = {};

	// Prefabs for runtime instantiation
	Zenith_Prefab* g_pxCellPrefab = nullptr;
	Zenith_Prefab* g_pxShapeCubePrefab = nullptr;
	Zenith_Prefab* g_pxCatPrefab = nullptr;
}

static bool s_bResourcesInitialized = false;

/**
 * Generate a unit sphere mesh for Flux_MeshGeometry
 * Uses UV sphere algorithm with configurable segments
 */
static void GenerateUnitSphere(Flux_MeshGeometry& xGeometryOut, uint32_t uLatitudeSegments = 16, uint32_t uLongitudeSegments = 32)
{
	const float PI = 3.14159265359f;

	uint32_t uNumVerts = (uLatitudeSegments + 1) * (uLongitudeSegments + 1);
	uint32_t uNumIndices = uLatitudeSegments * uLongitudeSegments * 6;

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;

	xGeometryOut.m_pxPositions = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxUVs = new Zenith_Maths::Vector2[uNumVerts];
	xGeometryOut.m_pxNormals = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxTangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxBitangents = new Zenith_Maths::Vector3[uNumVerts];
	xGeometryOut.m_pxColors = new Zenith_Maths::Vector4[uNumVerts];
	xGeometryOut.m_puIndices = new Flux_MeshGeometry::IndexType[uNumIndices];

	// Generate vertices
	uint32_t uVertIdx = 0;
	for (uint32_t lat = 0; lat <= uLatitudeSegments; ++lat)
	{
		float fTheta = lat * PI / uLatitudeSegments;  // 0 to PI (top to bottom)
		float fSinTheta = sinf(fTheta);
		float fCosTheta = cosf(fTheta);

		for (uint32_t lon = 0; lon <= uLongitudeSegments; ++lon)
		{
			float fPhi = lon * 2.0f * PI / uLongitudeSegments;  // 0 to 2PI
			float fSinPhi = sinf(fPhi);
			float fCosPhi = cosf(fPhi);

			// Position on unit sphere (radius 0.5 for unit diameter)
			Zenith_Maths::Vector3 xPos;
			xPos.x = fSinTheta * fCosPhi * 0.5f;
			xPos.y = fCosTheta * 0.5f;
			xPos.z = fSinTheta * fSinPhi * 0.5f;

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;

			// Normal = normalized position (pointing outward)
			Zenith_Maths::Vector3 xNormal = { fSinTheta * fCosPhi, fCosTheta, fSinTheta * fSinPhi };
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;

			// UV coordinates
			float fU = static_cast<float>(lon) / uLongitudeSegments;
			float fV = static_cast<float>(lat) / uLatitudeSegments;
			xGeometryOut.m_pxUVs[uVertIdx] = { fU, fV };

			// Tangent (pointing east along longitude)
			xGeometryOut.m_pxTangents[uVertIdx] = { -fSinPhi, 0.0f, fCosPhi };

			// Bitangent (pointing south along latitude)
			xGeometryOut.m_pxBitangents[uVertIdx] = { fCosTheta * fCosPhi, -fSinTheta, fCosTheta * fSinPhi };

			// Default white color
			xGeometryOut.m_pxColors[uVertIdx] = { 1.0f, 1.0f, 1.0f, 1.0f };

			uVertIdx++;
		}
	}

	// Generate indices (CCW winding for Vulkan)
	uint32_t uIdxIdx = 0;
	for (uint32_t lat = 0; lat < uLatitudeSegments; ++lat)
	{
		for (uint32_t lon = 0; lon < uLongitudeSegments; ++lon)
		{
			uint32_t uCurrent = lat * (uLongitudeSegments + 1) + lon;
			uint32_t uNext = uCurrent + uLongitudeSegments + 1;

			// Triangle 1 (CCW)
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

			// Triangle 2 (CCW)
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
		}
	}

	xGeometryOut.GenerateLayoutAndVertexData();

	// Upload to GPU
	Flux_MemoryManager::InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

static void InitializeTilePuzzleResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace TilePuzzle;

	// Create geometry using registry's cached primitives
	g_pxCubeAsset = Zenith_MeshGeometryAsset::CreateUnitCube();
	g_pxCubeGeometry = g_pxCubeAsset->GetGeometry();

	g_pxSphereAsset = Zenith_MeshGeometryAsset::CreateUnitSphere(16);
	g_pxSphereGeometry = g_pxSphereAsset->GetGeometry();

	// Use grid pattern texture with BaseColor for all materials
	Zenith_TextureAsset* pxGridTex = Flux_Graphics::s_pxGridTexture;

	// Create materials with grid texture and BaseColor
	auto& xRegistry = Zenith_AssetRegistry::Get();
	g_pxFloorMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxFloorMaterial->SetName("TilePuzzleFloor");
	g_pxFloorMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxFloorMaterial->SetBaseColor({ 77.f/255.f, 77.f/255.f, 89.f/255.f, 1.f });

	g_pxBlockerMaterial = xRegistry.Create<Zenith_MaterialAsset>();
	g_pxBlockerMaterial->SetName("TilePuzzleBlocker");
	g_pxBlockerMaterial->SetDiffuseTextureDirectly(pxGridTex);
	g_pxBlockerMaterial->SetBaseColor({ 80.f/255.f, 50.f/255.f, 30.f/255.f, 1.f });

	// Shape materials with distinct colors
	const char* aszShapeColorNames[] = { "Red", "Green", "Blue", "Yellow" };
	const Zenith_Maths::Vector4 axShapeColors[] = {
		{ 230.f/255.f, 60.f/255.f, 60.f/255.f, 1.f },    // Red
		{ 60.f/255.f, 200.f/255.f, 60.f/255.f, 1.f },    // Green
		{ 60.f/255.f, 100.f/255.f, 230.f/255.f, 1.f },   // Blue
		{ 230.f/255.f, 230.f/255.f, 60.f/255.f, 1.f }    // Yellow
	};
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleShape%s", aszShapeColorNames[i]);
		g_apxShapeMaterials[i] = xRegistry.Create<Zenith_MaterialAsset>();
		g_apxShapeMaterials[i]->SetName(szName);
		g_apxShapeMaterials[i]->SetDiffuseTextureDirectly(pxGridTex);
		g_apxShapeMaterials[i]->SetBaseColor(axShapeColors[i]);
	}

	// Cat materials (same colors as shapes)
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleCat%s", aszShapeColorNames[i]);
		g_apxCatMaterials[i] = xRegistry.Create<Zenith_MaterialAsset>();
		g_apxCatMaterials[i]->SetName(szName);
		g_apxCatMaterials[i]->SetDiffuseTextureDirectly(pxGridTex);
		g_apxCatMaterials[i]->SetBaseColor(axShapeColors[i]);
	}

	// Create prefabs for runtime instantiation
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Cell prefab (floor tiles)
	{
		Zenith_Entity xCellTemplate(&xScene, "CellTemplate");
		g_pxCellPrefab = new Zenith_Prefab();
		g_pxCellPrefab->CreateFromEntity(xCellTemplate, "Cell");
		Zenith_Scene::Destroy(xCellTemplate);
	}

	// Shape cube prefab (for multi-cube shapes)
	{
		Zenith_Entity xShapeCubeTemplate(&xScene, "ShapeCubeTemplate");
		g_pxShapeCubePrefab = new Zenith_Prefab();
		g_pxShapeCubePrefab->CreateFromEntity(xShapeCubeTemplate, "ShapeCube");
		Zenith_Scene::Destroy(xShapeCubeTemplate);
	}

	// Cat prefab (spheres)
	{
		Zenith_Entity xCatTemplate(&xScene, "CatTemplate");
		g_pxCatPrefab = new Zenith_Prefab();
		g_pxCatPrefab->CreateFromEntity(xCatTemplate, "Cat");
		Zenith_Scene::Destroy(xCatTemplate);
	}

	s_bResourcesInitialized = true;
}

// ============================================================================
// Required Entry Point Functions
// ============================================================================

const char* Project_GetName()
{
	return "TilePuzzle";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_RegisterScriptBehaviours()
{
	InitializeTilePuzzleResources();
	TilePuzzle_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// TilePuzzle has no resources that need explicit cleanup
}

void Project_LoadInitialScene()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	xScene.Reset();

	// Create main camera entity - top-down 3D view
	Zenith_Entity xCameraEntity(&xScene, "MainCamera");
	xCameraEntity.SetTransient(false);
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.f, 12.f, 0.f),  // Position: 12 up, centered
		-1.5f,  // Pitch: nearly straight down
		0.f,    // Yaw
		glm::radians(45.f),
		0.1f,
		1000.f,
		16.f / 9.f
	);
	xScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	// Create main game entity
	Zenith_Entity xGameEntity(&xScene, "TilePuzzleGame");
	xGameEntity.SetTransient(false);

	// UI Setup
	static constexpr float s_fMarginRight = 30.f;
	static constexpr float s_fMarginTop = 30.f;
	static constexpr float s_fBaseTextSize = 15.f;
	static constexpr float s_fLineHeight = 24.f;

	Zenith_UIComponent& xUI = xGameEntity.AddComponent<Zenith_UIComponent>();

	auto SetupTopRightText = [](Zenith_UI::Zenith_UIText* pxText, float fYOffset)
	{
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopRight);
		pxText->SetPosition(-s_fMarginRight, s_fMarginTop + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Right);
	};

	Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "TILE PUZZLE");
	SetupTopRightText(pxTitle, 0.f);
	pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
	pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("ControlsHeader", "How to Play:");
	SetupTopRightText(pxControls, s_fLineHeight * 2);
	pxControls->SetFontSize(s_fBaseTextSize * 3.6f);
	pxControls->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	Zenith_UI::Zenith_UIText* pxMove = xUI.CreateText("MoveInstr", "Click+Drag or Arrows: Move");
	SetupTopRightText(pxMove, s_fLineHeight * 3);
	pxMove->SetFontSize(s_fBaseTextSize * 3.0f);
	pxMove->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxReset = xUI.CreateText("ResetInstr", "R: Reset Level");
	SetupTopRightText(pxReset, s_fLineHeight * 4);
	pxReset->SetFontSize(s_fBaseTextSize * 3.0f);
	pxReset->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxGoal = xUI.CreateText("GoalHeader", "Goal:");
	SetupTopRightText(pxGoal, s_fLineHeight * 6);
	pxGoal->SetFontSize(s_fBaseTextSize * 3.6f);
	pxGoal->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	Zenith_UI::Zenith_UIText* pxGoalDesc = xUI.CreateText("GoalDesc", "Match shapes to cats");
	SetupTopRightText(pxGoalDesc, s_fLineHeight * 7);
	pxGoalDesc->SetFontSize(s_fBaseTextSize * 3.0f);
	pxGoalDesc->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "Level: 1  Moves: 0");
	SetupTopRightText(pxStatus, s_fLineHeight * 9);
	pxStatus->SetFontSize(s_fBaseTextSize * 3.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxProgress = xUI.CreateText("Progress", "Cats: 0 / 3");
	SetupTopRightText(pxProgress, s_fLineHeight * 10);
	pxProgress->SetFontSize(s_fBaseTextSize * 3.0f);
	pxProgress->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxWin = xUI.CreateText("WinText", "");
	SetupTopRightText(pxWin, s_fLineHeight * 12);
	pxWin->SetFontSize(s_fBaseTextSize * 4.2f);
	pxWin->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));

	// Add script component with TilePuzzle behaviour
	// Use SetBehaviourForSerialization - OnAwake will be dispatched when Play mode is entered
	Zenith_ScriptComponent& xScript = xGameEntity.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviourForSerialization<TilePuzzle_Behaviour>();

	// Save the scene file
	std::string strScenePath = std::string(GAME_ASSETS_DIR) + "/Scenes/TilePuzzle.zscn";
	std::filesystem::create_directories(std::string(GAME_ASSETS_DIR) + "/Scenes");
	xScene.SaveToFile(strScenePath);

	// Load from disk to ensure unified lifecycle code path (LoadFromFile handles OnAwake/OnEnable)
	xScene.LoadFromFile(strScenePath);
}
