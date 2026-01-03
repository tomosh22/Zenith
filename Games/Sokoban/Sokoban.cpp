#include "Zenith.h"

#include "Sokoban/Components/Sokoban_Behaviour.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandler.h"

// ============================================================================
// Sokoban Resources - Global access for behaviours
// ============================================================================
namespace Sokoban
{
	Flux_MeshGeometry* g_pxCubeGeometry = nullptr;
	Flux_MaterialAsset* g_pxFloorMaterial = nullptr;
	Flux_MaterialAsset* g_pxWallMaterial = nullptr;
	Flux_MaterialAsset* g_pxBoxMaterial = nullptr;
	Flux_MaterialAsset* g_pxBoxOnTargetMaterial = nullptr;
	Flux_MaterialAsset* g_pxPlayerMaterial = nullptr;
	Flux_MaterialAsset* g_pxTargetMaterial = nullptr;
}

static Flux_Texture* s_pxFloorTexture = nullptr;
static Flux_Texture* s_pxWallTexture = nullptr;
static Flux_Texture* s_pxBoxTexture = nullptr;
static Flux_Texture* s_pxBoxOnTargetTexture = nullptr;
static Flux_Texture* s_pxPlayerTexture = nullptr;
static Flux_Texture* s_pxTargetTexture = nullptr;
static bool s_bResourcesInitialized = false;

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

static void InitializeSokobanResources()
{
	if (s_bResourcesInitialized)
		return;

	using namespace Sokoban;

	g_pxCubeGeometry = new Flux_MeshGeometry();
	Flux_MeshGeometry::GenerateUnitCube(*g_pxCubeGeometry);

	s_pxFloorTexture = CreateColoredTexture(77, 77, 89);
	s_pxWallTexture = CreateColoredTexture(102, 64, 38);
	s_pxBoxTexture = CreateColoredTexture(204, 128, 51);
	s_pxBoxOnTargetTexture = CreateColoredTexture(51, 204, 51);
	s_pxPlayerTexture = CreateColoredTexture(51, 102, 230);
	s_pxTargetTexture = CreateColoredTexture(51, 153, 51);

	g_pxFloorMaterial = Flux_MaterialAsset::Create("SokobanFloor");
	g_pxFloorMaterial->SetDiffuseTexture(s_pxFloorTexture);

	g_pxWallMaterial = Flux_MaterialAsset::Create("SokobanWall");
	g_pxWallMaterial->SetDiffuseTexture(s_pxWallTexture);

	g_pxBoxMaterial = Flux_MaterialAsset::Create("SokobanBox");
	g_pxBoxMaterial->SetDiffuseTexture(s_pxBoxTexture);

	g_pxBoxOnTargetMaterial = Flux_MaterialAsset::Create("SokobanBoxOnTarget");
	g_pxBoxOnTargetMaterial->SetDiffuseTexture(s_pxBoxOnTargetTexture);

	g_pxPlayerMaterial = Flux_MaterialAsset::Create("SokobanPlayer");
	g_pxPlayerMaterial->SetDiffuseTexture(s_pxPlayerTexture);

	g_pxTargetMaterial = Flux_MaterialAsset::Create("SokobanTarget");
	g_pxTargetMaterial->SetDiffuseTexture(s_pxTargetTexture);

	s_bResourcesInitialized = true;
}
// ============================================================================

const char* Project_GetName()
{
	return "Sokoban";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_RegisterScriptBehaviours()
{
	// Initialize resources at startup (like Unity's [RuntimeInitializeOnLoadMethod])
	InitializeSokobanResources();

	Sokoban_Behaviour::RegisterBehaviour();
}

void Project_LoadInitialScene()
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	xScene.Reset();

	Zenith_Entity xCameraEntity(&xScene, "MainCamera");
	Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
	// Top-down 3D view: camera directly above the grid, looking straight down
	xCamera.InitialisePerspective(
		Zenith_Maths::Vector3(0.f, 12.f, 0.f),  // Position: 12 up, centered
		-1.5f,  // Pitch: -1.5 radians (nearly straight down)
		0.f,    // Yaw: 0 degrees
		glm::radians(45.f),   // FOV: 45 degrees
		0.1f,
		1000.f,
		16.f / 9.f
	);
	xScene.SetMainCameraEntity(xCameraEntity.GetEntityID());

	Zenith_Entity xSokobanEntity(&xScene, "SokobanGame");

	// UI Setup - anchored to top-right corner of screen
	static constexpr float s_fMarginRight = 30.f;  // Offset from right edge
	static constexpr float s_fMarginTop = 30.f;    // Offset from top edge
	static constexpr float s_fBaseTextSize = 15.f;
	static constexpr float s_fLineHeight = 24.f;

	Zenith_UIComponent& xUI = xSokobanEntity.AddComponent<Zenith_UIComponent>();

	// Helper lambda to set up top-right anchored text
	auto SetupTopRightText = [](Zenith_UI::Zenith_UIText* pxText, float fYOffset)
	{
		pxText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopRight);
		pxText->SetPosition(-s_fMarginRight, s_fMarginTop + fYOffset);
		pxText->SetAlignment(Zenith_UI::TextAlignment::Right);
	};

	Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "SOKOBAN");
	SetupTopRightText(pxTitle, 0.f);
	pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
	pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("ControlsHeader", "How to Play:");
	SetupTopRightText(pxControls, s_fLineHeight * 2);
	pxControls->SetFontSize(s_fBaseTextSize * 3.6f);
	pxControls->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	Zenith_UI::Zenith_UIText* pxMove = xUI.CreateText("MoveInstr", "WASD / Arrows: Move");
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

	Zenith_UI::Zenith_UIText* pxGoalDesc = xUI.CreateText("GoalDesc", "Push boxes onto targets");
	SetupTopRightText(pxGoalDesc, s_fLineHeight * 7);
	pxGoalDesc->SetFontSize(s_fBaseTextSize * 3.0f);
	pxGoalDesc->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "Moves: 0");
	SetupTopRightText(pxStatus, s_fLineHeight * 9);
	pxStatus->SetFontSize(s_fBaseTextSize * 3.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxProgress = xUI.CreateText("Progress", "Boxes: 0 / 3");
	SetupTopRightText(pxProgress, s_fLineHeight * 10);
	pxProgress->SetFontSize(s_fBaseTextSize * 3.0f);
	pxProgress->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxMinMoves = xUI.CreateText("MinMoves", "Min Moves: 0");
	SetupTopRightText(pxMinMoves, s_fLineHeight * 11);
	pxMinMoves->SetFontSize(s_fBaseTextSize * 3.0f);
	pxMinMoves->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxWin = xUI.CreateText("WinText", "");
	SetupTopRightText(pxWin, s_fLineHeight * 13);
	pxWin->SetFontSize(s_fBaseTextSize * 4.2f);
	pxWin->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));

	// Add script component with Sokoban behaviour
	// Resources are automatically obtained from Sokoban:: namespace in OnCreate()
	// (like Unity where MonoBehaviours get their references during Awake)
	Zenith_ScriptComponent& xScript = xSokobanEntity.AddComponent<Zenith_ScriptComponent>();
	xScript.SetBehaviour<Sokoban_Behaviour>();
}
