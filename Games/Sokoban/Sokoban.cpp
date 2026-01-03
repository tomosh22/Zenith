#include "Zenith.h"

#include "Sokoban/Components/Sokoban_Behaviour.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"

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

	// UI Setup - must be done before setting behaviour so UpdateUIPositions() works
	static constexpr uint32_t s_uGridOffsetX = 100;
	static constexpr uint32_t s_uGridOffsetY = 100;
	static constexpr uint32_t s_uGridSize = 8;
	static constexpr uint32_t s_uTileSize = 64;
	static constexpr uint32_t s_uTextStartX = s_uGridOffsetX + s_uGridSize * s_uTileSize + 50;
	static constexpr uint32_t s_uTextStartY = s_uGridOffsetY;
	static constexpr float s_fBaseTextSize = 15.f;
	static constexpr float s_fLineHeight = 24.f;

	Zenith_UIComponent& xUI = xSokobanEntity.AddComponent<Zenith_UIComponent>();

	Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "SOKOBAN");
	pxTitle->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY));
	pxTitle->SetFontSize(s_fBaseTextSize * 4.8f);
	pxTitle->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxControls = xUI.CreateText("ControlsHeader", "How to Play:");
	pxControls->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 2);
	pxControls->SetFontSize(s_fBaseTextSize * 3.6f);
	pxControls->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	Zenith_UI::Zenith_UIText* pxMove = xUI.CreateText("MoveInstr", "WASD / Arrows: Move");
	pxMove->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 3);
	pxMove->SetFontSize(s_fBaseTextSize * 3.0f);
	pxMove->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxMouse = xUI.CreateText("MouseInstr", "Mouse Click: Move");
	pxMouse->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 4);
	pxMouse->SetFontSize(s_fBaseTextSize * 3.0f);
	pxMouse->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxReset = xUI.CreateText("ResetInstr", "R: Reset Level");
	pxReset->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 5);
	pxReset->SetFontSize(s_fBaseTextSize * 3.0f);
	pxReset->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxGoal = xUI.CreateText("GoalHeader", "Goal:");
	pxGoal->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 7);
	pxGoal->SetFontSize(s_fBaseTextSize * 3.6f);
	pxGoal->SetColor(Zenith_Maths::Vector4(0.9f, 0.9f, 0.2f, 1.f));

	Zenith_UI::Zenith_UIText* pxGoalDesc = xUI.CreateText("GoalDesc", "Push boxes onto targets");
	pxGoalDesc->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 8);
	pxGoalDesc->SetFontSize(s_fBaseTextSize * 3.0f);
	pxGoalDesc->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.f));

	Zenith_UI::Zenith_UIText* pxStatus = xUI.CreateText("Status", "Moves: 0");
	pxStatus->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 10);
	pxStatus->SetFontSize(s_fBaseTextSize * 3.0f);
	pxStatus->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxProgress = xUI.CreateText("Progress", "Boxes: 0 / 3");
	pxProgress->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 11);
	pxProgress->SetFontSize(s_fBaseTextSize * 3.0f);
	pxProgress->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxMinMoves = xUI.CreateText("MinMoves", "Min Moves: 0");
	pxMinMoves->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 12);
	pxMinMoves->SetFontSize(s_fBaseTextSize * 3.0f);
	pxMinMoves->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));

	Zenith_UI::Zenith_UIText* pxWin = xUI.CreateText("WinText", "");
	pxWin->SetPosition(static_cast<float>(s_uTextStartX), static_cast<float>(s_uTextStartY) + s_fLineHeight * 14);
	pxWin->SetFontSize(s_fBaseTextSize * 4.2f);
	pxWin->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));

	// Add script component last so OnCreate() can access the UI elements
	xSokobanEntity.AddComponent<Zenith_ScriptComponent>().SetBehaviour<Sokoban_Behaviour>();
}
