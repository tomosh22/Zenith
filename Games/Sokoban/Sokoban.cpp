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

	// Add script component last so OnCreate() can access the UI elements
	xSokobanEntity.AddComponent<Zenith_ScriptComponent>().SetBehaviour<Sokoban_Behaviour>();
}
