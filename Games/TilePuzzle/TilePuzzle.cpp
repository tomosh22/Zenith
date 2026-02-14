#include "Zenith.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "TilePuzzle/Components/TilePuzzle_Types.h"
#include "TilePuzzle/Components/TilePuzzle_Behaviour.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Flux/Flux_Graphics.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIRect.h"
#include "SaveData/Zenith_SaveData.h"

#include <unordered_set>

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
	MaterialHandle g_xFloorMaterial;

	// Blocker material (static shapes)
	MaterialHandle g_xBlockerMaterial;

	// Colored shape materials (draggable)
	MaterialHandle g_axShapeMaterials[TILEPUZZLE_COLOR_COUNT];

	// Colored cat materials
	MaterialHandle g_axCatMaterials[TILEPUZZLE_COLOR_COUNT];

	// Prefabs for runtime instantiation
	Zenith_Prefab* g_pxCellPrefab = nullptr;
	Zenith_Prefab* g_pxShapeCubePrefab = nullptr;
	Zenith_Prefab* g_pxCatPrefab = nullptr;

	// Pre-generated merged meshes for each shape type
	Flux_MeshGeometry* g_apxShapeMeshes[TILEPUZZLE_SHAPE_COUNT] = {};
}

static bool s_bResourcesInitialized = false;

// ============================================================================
// Merged Polyomino Mesh Generation
// ============================================================================
static void GenerateShapeMesh(const TilePuzzleShapeDefinition& xDef, Flux_MeshGeometry& xGeometryOut)
{
	static constexpr float fBorder = 0.05f;
	static constexpr float fHalf = 0.5f;
	static constexpr float fHalfHeight = 0.5f;

	const std::vector<TilePuzzleCellOffset>& axCells = xDef.axCells;
	uint32_t uNumCells = static_cast<uint32_t>(axCells.size());

	// Build occupancy set for O(1) neighbor lookup
	// Key = (y+128)*256 + (x+128) to handle negative offsets
	std::unordered_set<uint32_t> xOccupied;
	for (uint32_t c = 0; c < uNumCells; ++c)
	{
		uint32_t uKey = (static_cast<uint32_t>(axCells[c].iY + 128)) * 256
			+ static_cast<uint32_t>(axCells[c].iX + 128);
		xOccupied.insert(uKey);
	}

	auto IsOccupied = [&](int32_t iX, int32_t iY) -> bool
	{
		uint32_t uKey = (static_cast<uint32_t>(iY + 128)) * 256
			+ static_cast<uint32_t>(iX + 128);
		return xOccupied.count(uKey) > 0;
	};

	// Count exterior faces: side faces only where no neighbor, top/bottom always
	uint32_t uNumExteriorFaces = 0;
	for (uint32_t c = 0; c < uNumCells; ++c)
	{
		int32_t iCX = axCells[c].iX;
		int32_t iCY = axCells[c].iY;

		if (!IsOccupied(iCX + 1, iCY)) uNumExteriorFaces++;
		if (!IsOccupied(iCX - 1, iCY)) uNumExteriorFaces++;
		if (!IsOccupied(iCX, iCY + 1)) uNumExteriorFaces++;
		if (!IsOccupied(iCX, iCY - 1)) uNumExteriorFaces++;
		uNumExteriorFaces += 2; // top + bottom always
	}

	uint32_t uNumVerts = uNumExteriorFaces * 4;
	uint32_t uNumIndices = uNumExteriorFaces * 6;

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;

	xGeometryOut.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxUVs = static_cast<Zenith_Maths::Vector2*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
	xGeometryOut.m_pxNormals = static_cast<Zenith_Maths::Vector3*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxTangents = static_cast<Zenith_Maths::Vector3*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxColors = static_cast<Zenith_Maths::Vector4*>(
		Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
	xGeometryOut.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(
		Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));

	uint32_t uVert = 0;
	uint32_t uIdx = 0;

	// Add a quad face (4 verts, 6 indices) with CCW winding matching GenerateUnitCube
	auto AddFace = [&](
		const Zenith_Maths::Vector3& p0, const Zenith_Maths::Vector3& p1,
		const Zenith_Maths::Vector3& p2, const Zenith_Maths::Vector3& p3,
		const Zenith_Maths::Vector3& xNormal,
		const Zenith_Maths::Vector3& xTangent,
		const Zenith_Maths::Vector3& xBitangent)
	{
		uint32_t uBase = uVert;

		Zenith_Maths::Vector3 apPositions[4] = { p0, p1, p2, p3 };
		Zenith_Maths::Vector2 axUVs[4] = { {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f}, {1.f, 1.f} };

		for (uint32_t v = 0; v < 4; ++v)
		{
			xGeometryOut.m_pxPositions[uVert] = apPositions[v];
			xGeometryOut.m_pxNormals[uVert] = xNormal;
			xGeometryOut.m_pxTangents[uVert] = xTangent;
			xGeometryOut.m_pxBitangents[uVert] = xBitangent;
			xGeometryOut.m_pxUVs[uVert] = axUVs[v];
			xGeometryOut.m_pxColors[uVert] = { 1.f, 1.f, 1.f, 1.f };
			uVert++;
		}

		xGeometryOut.m_puIndices[uIdx++] = uBase + 0;
		xGeometryOut.m_puIndices[uIdx++] = uBase + 2;
		xGeometryOut.m_puIndices[uIdx++] = uBase + 1;
		xGeometryOut.m_puIndices[uIdx++] = uBase + 1;
		xGeometryOut.m_puIndices[uIdx++] = uBase + 2;
		xGeometryOut.m_puIndices[uIdx++] = uBase + 3;
	};

	for (uint32_t c = 0; c < uNumCells; ++c)
	{
		float fCX = static_cast<float>(axCells[c].iX);
		float fCZ = static_cast<float>(axCells[c].iY);
		int32_t iCX = axCells[c].iX;
		int32_t iCY = axCells[c].iY;

		bool bHasRight = IsOccupied(iCX + 1, iCY);
		bool bHasLeft = IsOccupied(iCX - 1, iCY);
		bool bHasFront = IsOccupied(iCX, iCY + 1); // +Z
		bool bHasBack = IsOccupied(iCX, iCY - 1);  // -Z

		// Border-adjusted extents: internal edges at exact boundary, external edges inset
		float fMinX = fCX - fHalf + (bHasLeft ? 0.f : fBorder);
		float fMaxX = fCX + fHalf - (bHasRight ? 0.f : fBorder);
		float fMinZ = fCZ - fHalf + (bHasBack ? 0.f : fBorder);
		float fMaxZ = fCZ + fHalf - (bHasFront ? 0.f : fBorder);
		float fMinY = -fHalfHeight;
		float fMaxY = fHalfHeight;

		// +X face (right side) - only if no neighbor
		if (!bHasRight)
		{
			AddFace(
				{ fMaxX, fMinY, fMaxZ }, { fMaxX, fMinY, fMinZ },
				{ fMaxX, fMaxY, fMaxZ }, { fMaxX, fMaxY, fMinZ },
				{ 1.f, 0.f, 0.f }, { 0.f, 0.f, -1.f }, { 0.f, 1.f, 0.f }
			);
		}
		// -X face (left side)
		if (!bHasLeft)
		{
			AddFace(
				{ fMinX, fMinY, fMinZ }, { fMinX, fMinY, fMaxZ },
				{ fMinX, fMaxY, fMinZ }, { fMinX, fMaxY, fMaxZ },
				{ -1.f, 0.f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f, 0.f }
			);
		}
		// +Z face (front, grid Y+1)
		if (!bHasFront)
		{
			AddFace(
				{ fMinX, fMinY, fMaxZ }, { fMaxX, fMinY, fMaxZ },
				{ fMinX, fMaxY, fMaxZ }, { fMaxX, fMaxY, fMaxZ },
				{ 0.f, 0.f, 1.f }, { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }
			);
		}
		// -Z face (back, grid Y-1)
		if (!bHasBack)
		{
			AddFace(
				{ fMaxX, fMinY, fMinZ }, { fMinX, fMinY, fMinZ },
				{ fMaxX, fMaxY, fMinZ }, { fMinX, fMaxY, fMinZ },
				{ 0.f, 0.f, -1.f }, { -1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }
			);
		}
		// +Y face (top) - always exterior
		AddFace(
			{ fMinX, fMaxY, fMaxZ }, { fMaxX, fMaxY, fMaxZ },
			{ fMinX, fMaxY, fMinZ }, { fMaxX, fMaxY, fMinZ },
			{ 0.f, 1.f, 0.f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f, -1.f }
		);
		// -Y face (bottom) - always exterior
		AddFace(
			{ fMinX, fMinY, fMinZ }, { fMaxX, fMinY, fMinZ },
			{ fMinX, fMinY, fMaxZ }, { fMaxX, fMinY, fMaxZ },
			{ 0.f, -1.f, 0.f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f, 1.f }
		);
	}

	xGeometryOut.GenerateLayoutAndVertexData();
	Flux_MemoryManager::InitialiseVertexBuffer(
		xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(
		xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
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

	// Generate merged polyomino meshes for each shape type
	for (uint32_t u = 0; u < TILEPUZZLE_SHAPE_COUNT; ++u)
	{
		TilePuzzleShapeDefinition xDef = TilePuzzleShapes::GetShape(
			static_cast<TilePuzzleShapeType>(u), true);
		g_apxShapeMeshes[u] = new Flux_MeshGeometry();
		GenerateShapeMesh(xDef, *g_apxShapeMeshes[u]);
	}

	// Use grid pattern texture with BaseColor for all materials
	Zenith_TextureAsset* pxGridTex = Flux_Graphics::s_pxGridTexture;

	// Create materials with grid texture and BaseColor
	auto& xRegistry = Zenith_AssetRegistry::Get();
	g_xFloorMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xFloorMaterial.Get()->SetName("TilePuzzleFloor");
	g_xFloorMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xFloorMaterial.Get()->SetBaseColor({ 77.f/255.f, 77.f/255.f, 89.f/255.f, 1.f });

	g_xBlockerMaterial.Set(xRegistry.Create<Zenith_MaterialAsset>());
	g_xBlockerMaterial.Get()->SetName("TilePuzzleBlocker");
	g_xBlockerMaterial.Get()->SetDiffuseTextureDirectly(pxGridTex);
	g_xBlockerMaterial.Get()->SetBaseColor({ 80.f/255.f, 50.f/255.f, 30.f/255.f, 1.f });

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
		g_axShapeMaterials[i].Set(xRegistry.Create<Zenith_MaterialAsset>());
		g_axShapeMaterials[i].Get()->SetName(szName);
		g_axShapeMaterials[i].Get()->SetDiffuseTextureDirectly(pxGridTex);
		g_axShapeMaterials[i].Get()->SetBaseColor(axShapeColors[i]);
	}

	// Cat materials (same colors as shapes)
	for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
	{
		char szName[64];
		snprintf(szName, sizeof(szName), "TilePuzzleCat%s", aszShapeColorNames[i]);
		g_axCatMaterials[i].Set(xRegistry.Create<Zenith_MaterialAsset>());
		g_axCatMaterials[i].Get()->SetName(szName);
		g_axCatMaterials[i].Get()->SetDiffuseTextureDirectly(pxGridTex);
		g_axCatMaterials[i].Get()->SetBaseColor(axShapeColors[i]);
	}

	// Create prefabs for runtime instantiation
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);

	// Cell prefab (floor tiles)
	{
		Zenith_Entity xCellTemplate(pxSceneData, "CellTemplate");
		g_pxCellPrefab = new Zenith_Prefab();
		g_pxCellPrefab->CreateFromEntity(xCellTemplate, "Cell");
		Zenith_SceneManager::Destroy(xCellTemplate);
	}

	// Shape cube prefab (for multi-cube shapes)
	{
		Zenith_Entity xShapeCubeTemplate(pxSceneData, "ShapeCubeTemplate");
		g_pxShapeCubePrefab = new Zenith_Prefab();
		g_pxShapeCubePrefab->CreateFromEntity(xShapeCubeTemplate, "ShapeCube");
		Zenith_SceneManager::Destroy(xShapeCubeTemplate);
	}

	// Cat prefab (spheres)
	{
		Zenith_Entity xCatTemplate(pxSceneData, "CatTemplate");
		g_pxCatPrefab = new Zenith_Prefab();
		g_pxCatPrefab->CreateFromEntity(xCatTemplate, "Cat");
		Zenith_SceneManager::Destroy(xCatTemplate);
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

void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOptions)
{
	xOptions.m_uWindowWidth = 720;
	xOptions.m_uWindowHeight = 1280;
	xOptions.m_bFogEnabled = false;
	xOptions.m_bSSREnabled = false;
	xOptions.m_bSkyboxEnabled = false;
	xOptions.m_xSkyboxColour = Zenith_Maths::Vector3(0.1f, 0.1f, 0.15f);
}

void Project_RegisterScriptBehaviours()
{
	Zenith_SaveData::Initialise("TilePuzzle");
	InitializeTilePuzzleResources();
	TilePuzzle_Behaviour::RegisterBehaviour();
}

void Project_Shutdown()
{
	// TilePuzzle has no resources that need explicit cleanup
}

void Project_CreateScenes()
{
	// ---- MainMenu scene (build index 0) ----
	{
		const std::string strMenuPath = GAME_ASSETS_DIR "Scenes/MainMenu" ZENITH_SCENE_EXT;

		Zenith_Scene xMenuScene = Zenith_SceneManager::CreateEmptyScene("MainMenu");
		Zenith_SceneData* pxMenuData = Zenith_SceneManager::GetSceneData(xMenuScene);

		Zenith_Entity xMenuManager(pxMenuData, "GameManager");
		xMenuManager.SetTransient(false);

		Zenith_CameraComponent& xCamera = xMenuManager.AddComponent<Zenith_CameraComponent>();
		xCamera.InitialisePerspective({
			.m_xPosition = Zenith_Maths::Vector3(0.f, 12.f, 0.f),
			.m_fPitch = -1.5f,
			.m_fFOV = glm::radians(45.f),
			.m_fAspectRatio = 9.f / 16.f,
		});
		pxMenuData->SetMainCameraEntity(xMenuManager.GetEntityID());

		Zenith_UIComponent& xUI = xMenuManager.AddComponent<Zenith_UIComponent>();

		// ---- Main Menu UI ----

		Zenith_UI::Zenith_UIRect* pxMenuBg = xUI.CreateRect("MenuBackground");
		pxMenuBg->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxMenuBg->SetPosition(0.f, 0.f);
		pxMenuBg->SetSize(4000.f, 4000.f);
		pxMenuBg->SetColor({0.08f, 0.08f, 0.15f, 1.f});

		Zenith_UI::Zenith_UIText* pxMenuTitle = xUI.CreateText("MenuTitle", "TILE PUZZLE");
		pxMenuTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxMenuTitle->SetPosition(0.f, -180.f);
		pxMenuTitle->SetFontSize(72.f);
		pxMenuTitle->SetColor({1.f, 1.f, 1.f, 1.f});

		Zenith_UI::Zenith_UIButton* pxContinueBtn = xUI.CreateButton("ContinueButton", "Continue");
		pxContinueBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxContinueBtn->SetPosition(0.f, -20.f);
		pxContinueBtn->SetSize(300.f, 80.f);
		pxContinueBtn->SetFontSize(32.f);
		pxContinueBtn->SetNormalColor({0.2f, 0.25f, 0.4f, 1.f});
		pxContinueBtn->SetHoverColor({0.3f, 0.35f, 0.55f, 1.f});
		pxContinueBtn->SetPressedColor({0.12f, 0.15f, 0.25f, 1.f});

		Zenith_UI::Zenith_UIButton* pxLevelSelectBtn = xUI.CreateButton("LevelSelectButton", "Level Select");
		pxLevelSelectBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxLevelSelectBtn->SetPosition(0.f, 80.f);
		pxLevelSelectBtn->SetSize(300.f, 80.f);
		pxLevelSelectBtn->SetFontSize(32.f);
		pxLevelSelectBtn->SetNormalColor({0.2f, 0.25f, 0.4f, 1.f});
		pxLevelSelectBtn->SetHoverColor({0.3f, 0.35f, 0.55f, 1.f});
		pxLevelSelectBtn->SetPressedColor({0.12f, 0.15f, 0.25f, 1.f});

		Zenith_UI::Zenith_UIButton* pxNewGameBtn = xUI.CreateButton("NewGameButton", "New Game");
		pxNewGameBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxNewGameBtn->SetPosition(0.f, 180.f);
		pxNewGameBtn->SetSize(300.f, 80.f);
		pxNewGameBtn->SetFontSize(32.f);
		pxNewGameBtn->SetNormalColor({0.2f, 0.25f, 0.4f, 1.f});
		pxNewGameBtn->SetHoverColor({0.3f, 0.35f, 0.55f, 1.f});
		pxNewGameBtn->SetPressedColor({0.12f, 0.15f, 0.25f, 1.f});

		// ---- Level Select UI (starts hidden, toggled by behaviour) ----

		Zenith_UI::Zenith_UIRect* pxLevelSelectBg = xUI.CreateRect("LevelSelectBg");
		pxLevelSelectBg->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxLevelSelectBg->SetPosition(0.f, 0.f);
		pxLevelSelectBg->SetSize(4000.f, 4000.f);
		pxLevelSelectBg->SetColor({0.08f, 0.08f, 0.15f, 1.f});
		pxLevelSelectBg->SetVisible(false);

		Zenith_UI::Zenith_UIText* pxLevelSelectTitle = xUI.CreateText("LevelSelectTitle", "Select Level");
		pxLevelSelectTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxLevelSelectTitle->SetPosition(0.f, -260.f);
		pxLevelSelectTitle->SetFontSize(48.f);
		pxLevelSelectTitle->SetColor({1.f, 1.f, 1.f, 1.f});
		pxLevelSelectTitle->SetVisible(false);

		Zenith_UI::Zenith_UIText* pxPageText = xUI.CreateText("PageText", "Page 1 / 5");
		pxPageText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxPageText->SetPosition(0.f, -200.f);
		pxPageText->SetFontSize(32.f);
		pxPageText->SetColor({0.7f, 0.7f, 0.8f, 1.f});
		pxPageText->SetVisible(false);

		// Level select buttons (4 rows x 5 columns)
		for (uint32_t uRow = 0; uRow < 4; ++uRow)
		{
			for (uint32_t uCol = 0; uCol < 5; ++uCol)
			{
				uint32_t uIdx = uRow * 5 + uCol;
				char szBtnName[32];
				snprintf(szBtnName, sizeof(szBtnName), "LevelBtn_%u", uIdx);
				char szLabel[8];
				snprintf(szLabel, sizeof(szLabel), "%u", uIdx + 1);

				Zenith_UI::Zenith_UIButton* pxLevelBtn = xUI.CreateButton(szBtnName, szLabel);
				pxLevelBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
				float fX = (static_cast<float>(uCol) - 2.f) * 105.f;
				float fY = -50.f + (static_cast<float>(uRow) - 1.5f) * 65.f;
				pxLevelBtn->SetPosition(fX, fY);
				pxLevelBtn->SetSize(90.f, 55.f);
				pxLevelBtn->SetFontSize(20.f);
				pxLevelBtn->SetNormalColor({0.2f, 0.3f, 0.5f, 1.f});
				pxLevelBtn->SetHoverColor({0.3f, 0.4f, 0.6f, 1.f});
				pxLevelBtn->SetPressedColor({0.1f, 0.15f, 0.3f, 1.f});
				pxLevelBtn->SetVisible(false);
			}
		}

		Zenith_UI::Zenith_UIButton* pxPrevPageBtn = xUI.CreateButton("PrevPageButton", "<");
		pxPrevPageBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxPrevPageBtn->SetPosition(-160.f, 180.f);
		pxPrevPageBtn->SetSize(100.f, 50.f);
		pxPrevPageBtn->SetFontSize(28.f);
		pxPrevPageBtn->SetNormalColor({0.15f, 0.2f, 0.3f, 1.f});
		pxPrevPageBtn->SetHoverColor({0.25f, 0.3f, 0.45f, 1.f});
		pxPrevPageBtn->SetVisible(false);

		Zenith_UI::Zenith_UIButton* pxBackBtn = xUI.CreateButton("BackButton", "Back");
		pxBackBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxBackBtn->SetPosition(0.f, 180.f);
		pxBackBtn->SetSize(120.f, 50.f);
		pxBackBtn->SetFontSize(24.f);
		pxBackBtn->SetNormalColor({0.15f, 0.2f, 0.3f, 1.f});
		pxBackBtn->SetHoverColor({0.25f, 0.3f, 0.45f, 1.f});
		pxBackBtn->SetVisible(false);

		Zenith_UI::Zenith_UIButton* pxNextPageBtn = xUI.CreateButton("NextPageButton", ">");
		pxNextPageBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxNextPageBtn->SetPosition(160.f, 180.f);
		pxNextPageBtn->SetSize(100.f, 50.f);
		pxNextPageBtn->SetFontSize(28.f);
		pxNextPageBtn->SetNormalColor({0.15f, 0.2f, 0.3f, 1.f});
		pxNextPageBtn->SetHoverColor({0.25f, 0.3f, 0.45f, 1.f});
		pxNextPageBtn->SetVisible(false);

		Zenith_ScriptComponent& xScript = xMenuManager.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviourForSerialization<TilePuzzle_Behaviour>();

		pxMenuData->SaveToFile(strMenuPath);
		Zenith_SceneManager::RegisterSceneBuildIndex(0, strMenuPath);
		Zenith_SceneManager::UnloadScene(xMenuScene);
	}

	// ---- TilePuzzle gameplay scene (build index 1) ----
	{
		const std::string strGamePath = GAME_ASSETS_DIR "Scenes/TilePuzzle" ZENITH_SCENE_EXT;

		Zenith_Scene xGameScene = Zenith_SceneManager::CreateEmptyScene("TilePuzzle");
		Zenith_SceneData* pxGameData = Zenith_SceneManager::GetSceneData(xGameScene);

		Zenith_Entity xGameManager(pxGameData, "GameManager");
		xGameManager.SetTransient(false);

		Zenith_CameraComponent& xCamera = xGameManager.AddComponent<Zenith_CameraComponent>();
		xCamera.InitialisePerspective({
			.m_xPosition = Zenith_Maths::Vector3(0.f, 12.f, 0.f),
			.m_fPitch = -1.5f,
			.m_fFOV = glm::radians(45.f),
			.m_fAspectRatio = 9.f / 16.f,
		});
		pxGameData->SetMainCameraEntity(xGameManager.GetEntityID());

		Zenith_UIComponent& xUI = xGameManager.AddComponent<Zenith_UIComponent>();

		static constexpr float s_fMarginRight = 30.f;
		static constexpr float s_fMarginTop = 30.f;
		static constexpr float s_fBaseTextSize = 15.f;
		static constexpr float s_fLineHeight = 24.f;

		auto SetupTopRightText = [](Zenith_UI::Zenith_UIText* pxText, float fYOffset, bool bVisible)
		{
			pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopRight);
			pxText->SetPosition(-s_fMarginRight, s_fMarginTop + fYOffset);
			pxText->SetAlignment(Zenith_UI::TextAlignment::Right);
			pxText->SetVisible(bVisible);
		};

		Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "TILE PUZZLE");
		SetupTopRightText(pxTitle, 0.f, false);
		pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
		pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

		Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("ControlsHeader", "How to Play:");
		SetupTopRightText(pxControls, s_fLineHeight * 2, false);
		pxControls->SetFontSize(s_fBaseTextSize * 3.6f);
		pxControls->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

		Zenith_UI::Zenith_UIText* pxMove = xUI.CreateText("MoveInstr", "Click+Drag or Arrows: Move");
		SetupTopRightText(pxMove, s_fLineHeight * 3, false);
		pxMove->SetFontSize(s_fBaseTextSize * 3.0f);
		pxMove->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

		Zenith_UI::Zenith_UIText* pxReset = xUI.CreateText("ResetInstr", "R: Reset  Esc: Menu");
		SetupTopRightText(pxReset, s_fLineHeight * 4, false);
		pxReset->SetFontSize(s_fBaseTextSize * 3.0f);
		pxReset->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

		Zenith_UI::Zenith_UIText* pxGoal = xUI.CreateText("GoalHeader", "Goal:");
		SetupTopRightText(pxGoal, s_fLineHeight * 6, false);
		pxGoal->SetFontSize(s_fBaseTextSize * 3.6f);
		pxGoal->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

		Zenith_UI::Zenith_UIText* pxGoalDesc = xUI.CreateText("GoalDesc", "Match shapes to cats");
		SetupTopRightText(pxGoalDesc, s_fLineHeight * 7, false);
		pxGoalDesc->SetFontSize(s_fBaseTextSize * 3.0f);
		pxGoalDesc->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

		Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "Level: 1  Moves: 0");
		SetupTopRightText(pxStatus, s_fLineHeight * 9, false);
		pxStatus->SetFontSize(s_fBaseTextSize * 3.0f);
		pxStatus->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

		Zenith_UI::Zenith_UIText* pxProgress = xUI.CreateText("Progress", "Cats: 0 / 3");
		SetupTopRightText(pxProgress, s_fLineHeight * 10, false);
		pxProgress->SetFontSize(s_fBaseTextSize * 3.0f);
		pxProgress->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

		Zenith_UI::Zenith_UIText* pxWin = xUI.CreateText("WinText", "");
		SetupTopRightText(pxWin, s_fLineHeight * 12, false);
		pxWin->SetFontSize(s_fBaseTextSize * 4.2f);
		pxWin->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));

		// Gameplay action buttons
		Zenith_UI::Zenith_UIButton* pxResetBtn = xUI.CreateButton("ResetBtn", "Reset");
		pxResetBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxResetBtn->SetPosition(20.f, 20.f);
		pxResetBtn->SetSize(100.f, 50.f);
		pxResetBtn->SetFontSize(20.f);
		pxResetBtn->SetNormalColor({0.2f, 0.25f, 0.35f, 1.f});
		pxResetBtn->SetHoverColor({0.3f, 0.35f, 0.5f, 1.f});

		Zenith_UI::Zenith_UIButton* pxMenuBtn = xUI.CreateButton("MenuBtn", "Menu");
		pxMenuBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopLeft);
		pxMenuBtn->SetPosition(20.f, 80.f);
		pxMenuBtn->SetSize(100.f, 50.f);
		pxMenuBtn->SetFontSize(20.f);
		pxMenuBtn->SetNormalColor({0.2f, 0.25f, 0.35f, 1.f});
		pxMenuBtn->SetHoverColor({0.3f, 0.35f, 0.5f, 1.f});

		Zenith_UI::Zenith_UIButton* pxNextLevelBtn = xUI.CreateButton("NextLevelBtn", "Next Level");
		pxNextLevelBtn->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
		pxNextLevelBtn->SetPosition(0.f, 80.f);
		pxNextLevelBtn->SetSize(200.f, 60.f);
		pxNextLevelBtn->SetFontSize(28.f);
		pxNextLevelBtn->SetNormalColor({0.15f, 0.4f, 0.2f, 1.f});
		pxNextLevelBtn->SetHoverColor({0.25f, 0.55f, 0.3f, 1.f});
		pxNextLevelBtn->SetVisible(false);

		Zenith_ScriptComponent& xScript = xGameManager.AddComponent<Zenith_ScriptComponent>();
		xScript.SetBehaviourForSerialization<TilePuzzle_Behaviour>();

		pxGameData->SaveToFile(strGamePath);
		Zenith_SceneManager::RegisterSceneBuildIndex(1, strGamePath);
		Zenith_SceneManager::UnloadScene(xGameScene);
	}
}

void Project_LoadInitialScene()
{
	Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
