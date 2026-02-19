#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "UnitTests/Zenith_AutomationTests.h"
#include "UnitTests/Zenith_EditorTestFixture.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIRect.h"
#include "FileAccess/Zenith_FileAccess.h"
#include <cmath>
#include <filesystem>

static void NoOp() {}

void Zenith_AutomationTests::RunAllTests()
{
	// State Machine tests
	TestInitialState();
	TestBeginSetsRunning();
	TestResetClearsState();

	// Step Execution tests
	TestStepExecutionOrder();
	TestExecuteEmptyQueue();
	TestCompletionAfterAllSteps();

	// Entity Operation tests
	TestCreateEntityStep();
	TestEntitySelectionTracking();

	// Component Operation tests
	TestAddComponentStep();

	// Transform Operation tests
	TestSetTransformPositionStep();
	TestSetTransformScaleStep();

	// Camera Operation tests
	TestSetCameraFOVStep();
	TestSetCameraPitchYawStep();
	TestSetCameraPositionStep();
	TestSetAsMainCameraStep();

	// Negative Path tests
	TestAddInvalidComponentStep();

	// Custom Step tests
	TestCustomStepExecution();

	// Scene Lifecycle tests
	TestCreateSaveUnloadCycle();

	// UI Operation tests
	TestCreateUITextStep();
	TestCreateUIButtonStep();
	TestCreateUIRectStep();
	TestSetUIPropertiesStep();
	TestSetUIButtonStyleStep();

	// Script/Behaviour tests
	TestSetBehaviourStep();
	TestSetBehaviourForSerializationStep();

	// Camera Extended tests
	TestSetCameraNearFarAspectStep();

	// Scene Round-Trip tests
	TestSceneSaveLoadRoundTrip();

	// Edge Case tests
	TestResetDuringExecution();
	TestBeginWithZeroSteps();
	TestDoubleBeginWithoutReset();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] All automation tests passed");
}

//=============================================================================
// State Machine Tests
//=============================================================================

void Zenith_AutomationTests::TestInitialState()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestInitialState...");

	Zenith_EditorAutomation::Reset();

	Zenith_Assert(!Zenith_EditorAutomation::IsRunning(), "Should not be running after reset");
	Zenith_Assert(!Zenith_EditorAutomation::IsComplete(), "Should not be complete after reset");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] TestInitialState passed");
}

void Zenith_AutomationTests::TestBeginSetsRunning()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestBeginSetsRunning...");

	Zenith_EditorAutomation::Reset();

	// Add a dummy step so Begin has something to work with
	Zenith_EditorAutomation::AddStep_Custom(&NoOp);
	Zenith_EditorAutomation::Begin();

	Zenith_Assert(Zenith_EditorAutomation::IsRunning(), "Should be running after Begin");
	Zenith_Assert(!Zenith_EditorAutomation::IsComplete(), "Should not be complete right after Begin");

	Zenith_EditorAutomation::Reset();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] TestBeginSetsRunning passed");
}

void Zenith_AutomationTests::TestResetClearsState()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestResetClearsState...");

	// Add steps and begin
	Zenith_EditorAutomation::AddStep_Custom(&NoOp);
	Zenith_EditorAutomation::AddStep_Custom(&NoOp);
	Zenith_EditorAutomation::Begin();

	Zenith_Assert(Zenith_EditorAutomation::IsRunning(), "Should be running");

	// Reset should clear everything
	Zenith_EditorAutomation::Reset();

	Zenith_Assert(!Zenith_EditorAutomation::IsRunning(), "Should not be running after Reset");
	Zenith_Assert(!Zenith_EditorAutomation::IsComplete(), "Should not be complete after Reset");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] TestResetClearsState passed");
}

//=============================================================================
// Step Execution Tests
//=============================================================================

static uint32_t s_uCustomStepCounter = 0;
static void IncrementCounter() { s_uCustomStepCounter++; }

void Zenith_AutomationTests::TestStepExecutionOrder()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestStepExecutionOrder...");

	Zenith_EditorAutomation::Reset();
	s_uCustomStepCounter = 0;

	// Add 3 custom steps
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);

	Zenith_EditorAutomation::Begin();

	// Execute steps one at a time
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(s_uCustomStepCounter == 1, "Counter should be 1 after first step");
	Zenith_Assert(Zenith_EditorAutomation::IsRunning(), "Should still be running after first step");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(s_uCustomStepCounter == 2, "Counter should be 2 after second step");
	Zenith_Assert(Zenith_EditorAutomation::IsRunning(), "Should still be running after second step");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(s_uCustomStepCounter == 3, "Counter should be 3 after third step");
	Zenith_Assert(!Zenith_EditorAutomation::IsRunning(), "Should not be running after all steps");
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "Should be complete after all steps");

	Zenith_EditorAutomation::Reset();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] TestStepExecutionOrder passed");
}

void Zenith_AutomationTests::TestExecuteEmptyQueue()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestExecuteEmptyQueue...");

	Zenith_EditorAutomation::Reset();

	// Calling ExecuteNextStep when not running should be a no-op
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(!Zenith_EditorAutomation::IsRunning(), "Should not be running");
	Zenith_Assert(!Zenith_EditorAutomation::IsComplete(), "Should not be complete");

	Zenith_EditorAutomation::Reset();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] TestExecuteEmptyQueue passed");
}

void Zenith_AutomationTests::TestCompletionAfterAllSteps()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCompletionAfterAllSteps...");

	Zenith_EditorAutomation::Reset();
	s_uCustomStepCounter = 0;

	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::Begin();

	// Execute the single step - completion detected immediately
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(s_uCustomStepCounter == 1, "Counter should be 1");
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "Should be complete");
	Zenith_Assert(!Zenith_EditorAutomation::IsRunning(), "Should not be running");

	// Additional calls after completion should be no-ops
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "Should still be complete");

	Zenith_EditorAutomation::Reset();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] TestCompletionAfterAllSteps passed");
}

//=============================================================================
// Entity Operation Tests
//=============================================================================

void Zenith_AutomationTests::TestCreateEntityStep()
{
	EDITOR_TEST_BEGIN(TestCreateEntityStep);

	Zenith_EditorAutomation::Reset();

	// Queue a create entity step
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoTestEntity");
	Zenith_EditorAutomation::Begin();

	// Execute the step
	Zenith_EditorAutomation::ExecuteNextStep();

	// Verify entity was created and selected
	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have a selected entity after CreateEntity step");
	Zenith_Assert(strcmp(pxEntity->GetName().c_str(), "AutoTestEntity") == 0,
		"Created entity should be named 'AutoTestEntity'");
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "Should be complete after single step");

	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateEntityStep);
}

void Zenith_AutomationTests::TestEntitySelectionTracking()
{
	EDITOR_TEST_BEGIN(TestEntitySelectionTracking);

	Zenith_EditorAutomation::Reset();

	// Queue: create A, create B, select A
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoEntityA");
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoEntityB");
	Zenith_EditorAutomation::AddStep_SelectEntity("AutoEntityA");
	Zenith_EditorAutomation::Begin();

	// Step 1: Create A
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selection after creating A");
	Zenith_Assert(strcmp(pxEntity->GetName().c_str(), "AutoEntityA") == 0,
		"Selection should be A after creating A");

	// Step 2: Create B (auto-selects B)
	Zenith_EditorAutomation::ExecuteNextStep();
	pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selection after creating B");
	Zenith_Assert(strcmp(pxEntity->GetName().c_str(), "AutoEntityB") == 0,
		"Selection should be B after creating B");

	// Step 3: Select A again
	Zenith_EditorAutomation::ExecuteNextStep();
	pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selection after selecting A");
	Zenith_Assert(strcmp(pxEntity->GetName().c_str(), "AutoEntityA") == 0,
		"Selection should be A after SelectEntity step");
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "Should be complete after last step");

	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestEntitySelectionTracking);
}

//=============================================================================
// Component Operation Tests
//=============================================================================

void Zenith_AutomationTests::TestAddComponentStep()
{
	EDITOR_TEST_BEGIN(TestAddComponentStep);

	Zenith_EditorAutomation::Reset();

	// Queue: create entity, add camera component
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoCamEntity");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::Begin();

	// Execute both steps
	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity
	Zenith_EditorAutomation::ExecuteNextStep(); // Add camera

	// Verify camera was added
	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");
	Zenith_Assert(pxEntity->HasComponent<Zenith_CameraComponent>(),
		"Entity should have CameraComponent after AddCamera step");

	// Advance to completion
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestAddComponentStep);
}

//=============================================================================
// Transform Operation Tests
//=============================================================================

void Zenith_AutomationTests::TestSetTransformPositionStep()
{
	EDITOR_TEST_BEGIN(TestSetTransformPositionStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoPosEntity");
	Zenith_EditorAutomation::AddStep_SetTransformPosition(10.f, 20.f, 30.f);
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create
	Zenith_EditorAutomation::ExecuteNextStep(); // Set position

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");

	Zenith_TransformComponent& xTransform = pxEntity->GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPos;
	xTransform.GetPosition(xPos);

	Zenith_Assert(std::abs(xPos.x - 10.f) < 0.001f, "X position should be 10");
	Zenith_Assert(std::abs(xPos.y - 20.f) < 0.001f, "Y position should be 20");
	Zenith_Assert(std::abs(xPos.z - 30.f) < 0.001f, "Z position should be 30");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetTransformPositionStep);
}

void Zenith_AutomationTests::TestSetTransformScaleStep()
{
	EDITOR_TEST_BEGIN(TestSetTransformScaleStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoScaleEntity");
	Zenith_EditorAutomation::AddStep_SetTransformScale(2.f, 3.f, 4.f);
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create
	Zenith_EditorAutomation::ExecuteNextStep(); // Set scale

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");

	Zenith_TransformComponent& xTransform = pxEntity->GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);

	Zenith_Assert(std::abs(xScale.x - 2.f) < 0.001f, "X scale should be 2");
	Zenith_Assert(std::abs(xScale.y - 3.f) < 0.001f, "Y scale should be 3");
	Zenith_Assert(std::abs(xScale.z - 4.f) < 0.001f, "Z scale should be 4");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetTransformScaleStep);
}

//=============================================================================
// Camera Operation Tests
//=============================================================================

void Zenith_AutomationTests::TestSetCameraFOVStep()
{
	EDITOR_TEST_BEGIN(TestSetCameraFOVStep);

	Zenith_EditorAutomation::Reset();

	float fTargetFOV = 1.2f;
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoFOVEntity");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraFOV(fTargetFOV);
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create
	Zenith_EditorAutomation::ExecuteNextStep(); // Add camera
	Zenith_EditorAutomation::ExecuteNextStep(); // Set FOV

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");
	float fActual = pxEntity->GetComponent<Zenith_CameraComponent>().GetFOV();
	Zenith_Assert(std::abs(fActual - fTargetFOV) < 0.001f, "FOV should match target");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetCameraFOVStep);
}

void Zenith_AutomationTests::TestSetCameraPitchYawStep()
{
	EDITOR_TEST_BEGIN(TestSetCameraPitchYawStep);

	Zenith_EditorAutomation::Reset();

	float fTargetPitch = -0.5f;
	float fTargetYaw = 2.0f;
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoPYEntity");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPitch(fTargetPitch);
	Zenith_EditorAutomation::AddStep_SetCameraYaw(fTargetYaw);
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create
	Zenith_EditorAutomation::ExecuteNextStep(); // Add camera
	Zenith_EditorAutomation::ExecuteNextStep(); // Set pitch
	Zenith_EditorAutomation::ExecuteNextStep(); // Set yaw

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");
	Zenith_CameraComponent& xCam = pxEntity->GetComponent<Zenith_CameraComponent>();
	Zenith_Assert(std::abs(static_cast<float>(xCam.GetPitch()) - fTargetPitch) < 0.001f, "Pitch should match target");
	Zenith_Assert(std::abs(static_cast<float>(xCam.GetYaw()) - fTargetYaw) < 0.001f, "Yaw should match target");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetCameraPitchYawStep);
}

void Zenith_AutomationTests::TestSetCameraPositionStep()
{
	EDITOR_TEST_BEGIN(TestSetCameraPositionStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoCamPosEntity");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraPosition(5.f, 10.f, 15.f);
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create
	Zenith_EditorAutomation::ExecuteNextStep(); // Add camera
	Zenith_EditorAutomation::ExecuteNextStep(); // Set position

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");
	Zenith_Maths::Vector3 xPos;
	pxEntity->GetComponent<Zenith_CameraComponent>().GetPosition(xPos);
	Zenith_Assert(std::abs(xPos.x - 5.f) < 0.001f, "Camera X position should be 5");
	Zenith_Assert(std::abs(xPos.y - 10.f) < 0.001f, "Camera Y position should be 10");
	Zenith_Assert(std::abs(xPos.z - 15.f) < 0.001f, "Camera Z position should be 15");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetCameraPositionStep);
}

void Zenith_AutomationTests::TestSetAsMainCameraStep()
{
	EDITOR_TEST_BEGIN(TestSetAsMainCameraStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoMainCamEntity");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetAsMainCamera();
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create
	Zenith_EditorAutomation::ExecuteNextStep(); // Add camera
	Zenith_EditorAutomation::ExecuteNextStep(); // Set as main camera

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");

	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneDataForEntity(pxEntity->GetEntityID());
	Zenith_Assert(pxSceneData != nullptr, "Entity should be in a scene");
	Zenith_Assert(pxSceneData->GetMainCameraEntity() == pxEntity->GetEntityID(),
		"Entity should be the main camera");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetAsMainCameraStep);
}

//=============================================================================
// Negative Path Tests
//=============================================================================

void Zenith_AutomationTests::TestAddInvalidComponentStep()
{
	EDITOR_TEST_BEGIN(TestAddInvalidComponentStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoInvalidCompEntity");
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity

	// Try to add a component with an invalid name directly through the editor API
	bool bResult = Zenith_Editor::AddComponentToSelected("NonExistentComponent_XYZ");
	Zenith_Assert(!bResult, "Adding invalid component should return false");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestAddInvalidComponentStep);
}

//=============================================================================
// Custom Step Tests
//=============================================================================

static bool s_bCustomStepExecuted = false;
static void SetCustomFlag() { s_bCustomStepExecuted = true; }

void Zenith_AutomationTests::TestCustomStepExecution()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestCustomStepExecution...");

	Zenith_EditorAutomation::Reset();
	s_bCustomStepExecuted = false;

	Zenith_EditorAutomation::AddStep_Custom(&SetCustomFlag);
	Zenith_EditorAutomation::Begin();

	Zenith_Assert(!s_bCustomStepExecuted, "Custom step should not execute before ExecuteNextStep");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(s_bCustomStepExecuted, "Custom step function should have been called");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "Should be complete");

	Zenith_EditorAutomation::Reset();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] TestCustomStepExecution passed");
}

//=============================================================================
// Scene Lifecycle Tests
//=============================================================================

void Zenith_AutomationTests::TestCreateSaveUnloadCycle()
{
	EDITOR_TEST_BEGIN(TestCreateSaveUnloadCycle);

	Zenith_EditorAutomation::Reset();

	const char* szSavePath = ENGINE_ASSETS_DIR "_AutoTest" ZENITH_SCENE_EXT;

	// Queue: create scene, create entity, add camera, save, unload
	Zenith_EditorAutomation::AddStep_CreateScene("AutoTestScene");
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoSceneEntity");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SaveScene(szSavePath);
	Zenith_EditorAutomation::AddStep_UnloadScene();
	Zenith_EditorAutomation::Begin();

	// Step 1: Create scene
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Scene xScene = Zenith_SceneManager::GetActiveScene();
	Zenith_Assert(xScene.IsValid(), "Active scene should be valid after CreateScene step");

	// Step 2: Create entity
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have created entity in new scene");
	Zenith_Assert(strcmp(pxEntity->GetName().c_str(), "AutoSceneEntity") == 0,
		"Entity name should match");

	// Step 3: Add camera
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(pxEntity->HasComponent<Zenith_CameraComponent>(),
		"Entity should have camera component");

	// Step 4: Save scene
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(std::filesystem::exists(szSavePath), "Scene file should exist after save");

	// Step 5: Unload scene
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "Should be complete after last step");

	// Clean up the temp scene file
	std::filesystem::remove(szSavePath);

	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateSaveUnloadCycle);
}

//=============================================================================
// UI Operation Tests
//=============================================================================

void Zenith_AutomationTests::TestCreateUITextStep()
{
	EDITOR_TEST_BEGIN(TestCreateUITextStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoUITextEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIText("Label1", "Hello");
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity
	Zenith_EditorAutomation::ExecuteNextStep(); // Add UI
	Zenith_EditorAutomation::ExecuteNextStep(); // Create text

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");
	Zenith_Assert(pxEntity->HasComponent<Zenith_UIComponent>(), "Entity should have UIComponent");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>("Label1");
	Zenith_Assert(pxText != nullptr, "Should find UI text element 'Label1'");
	Zenith_Assert(pxText->GetText() == "Hello", "Text content should be 'Hello'");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateUITextStep);
}

void Zenith_AutomationTests::TestCreateUIButtonStep()
{
	EDITOR_TEST_BEGIN(TestCreateUIButtonStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoUIBtnEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("Btn1", "Click Me");
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity
	Zenith_EditorAutomation::ExecuteNextStep(); // Add UI
	Zenith_EditorAutomation::ExecuteNextStep(); // Create button

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>("Btn1");
	Zenith_Assert(pxButton != nullptr, "Should find UI button 'Btn1'");
	Zenith_Assert(pxButton->GetText() == "Click Me", "Button text should be 'Click Me'");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateUIButtonStep);
}

void Zenith_AutomationTests::TestCreateUIRectStep()
{
	EDITOR_TEST_BEGIN(TestCreateUIRectStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoUIRectEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIRect("Rect1");
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity
	Zenith_EditorAutomation::ExecuteNextStep(); // Add UI
	Zenith_EditorAutomation::ExecuteNextStep(); // Create rect

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>("Rect1");
	Zenith_Assert(pxRect != nullptr, "Should find UI rect 'Rect1'");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateUIRectStep);
}

void Zenith_AutomationTests::TestSetUIPropertiesStep()
{
	EDITOR_TEST_BEGIN(TestSetUIPropertiesStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoUIPropEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIText("Txt", "Test");
	Zenith_EditorAutomation::AddStep_SetUIPosition("Txt", 100.f, 200.f);
	Zenith_EditorAutomation::AddStep_SetUISize("Txt", 300.f, 50.f);
	Zenith_EditorAutomation::AddStep_SetUIFontSize("Txt", 32.f);
	Zenith_EditorAutomation::AddStep_SetUIColor("Txt", 1.f, 0.f, 0.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIAnchor("Txt", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	Zenith_EditorAutomation::AddStep_SetUIAlignment("Txt", static_cast<int>(Zenith_UI::TextAlignment::Center));
	Zenith_EditorAutomation::AddStep_SetUIVisible("Txt", false);
	Zenith_EditorAutomation::Begin();

	// Execute all 10 steps
	for (uint32_t i = 0; i < 10; i++)
	{
		Zenith_EditorAutomation::ExecuteNextStep();
	}

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>("Txt");
	Zenith_Assert(pxText != nullptr, "Should find UI text 'Txt'");

	Zenith_Maths::Vector2 xPos = pxText->GetPosition();
	Zenith_Assert(std::abs(xPos.x - 100.f) < 0.001f, "UI position X should be 100");
	Zenith_Assert(std::abs(xPos.y - 200.f) < 0.001f, "UI position Y should be 200");

	Zenith_Maths::Vector2 xSize = pxText->GetSize();
	Zenith_Assert(std::abs(xSize.x - 300.f) < 0.001f, "UI size W should be 300");
	Zenith_Assert(std::abs(xSize.y - 50.f) < 0.001f, "UI size H should be 50");

	Zenith_Assert(std::abs(pxText->GetFontSize() - 32.f) < 0.001f, "Font size should be 32");

	Zenith_Maths::Vector4 xColor = pxText->GetColor();
	Zenith_Assert(std::abs(xColor.x - 1.f) < 0.001f, "Color R should be 1");
	Zenith_Assert(std::abs(xColor.y - 0.f) < 0.001f, "Color G should be 0");
	Zenith_Assert(std::abs(xColor.z - 0.f) < 0.001f, "Color B should be 0");
	Zenith_Assert(std::abs(xColor.w - 1.f) < 0.001f, "Color A should be 1");

	Zenith_Assert(!pxText->IsVisible(), "Element should not be visible");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIPropertiesStep);
}

void Zenith_AutomationTests::TestSetUIButtonStyleStep()
{
	EDITOR_TEST_BEGIN(TestSetUIButtonStyleStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoUIBtnStyleEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("Btn", "Test");
	Zenith_EditorAutomation::AddStep_SetUIButtonNormalColor("Btn", 1.f, 0.f, 0.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonHoverColor("Btn", 0.f, 1.f, 0.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonPressedColor("Btn", 0.f, 0.f, 1.f, 1.f);
	Zenith_EditorAutomation::AddStep_SetUIButtonFontSize("Btn", 18.f);
	Zenith_EditorAutomation::Begin();

	// Execute all 7 steps
	for (uint32_t i = 0; i < 7; i++)
	{
		Zenith_EditorAutomation::ExecuteNextStep();
	}

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>("Btn");
	Zenith_Assert(pxButton != nullptr, "Should find UI button 'Btn'");

	Zenith_Maths::Vector4 xNormal = pxButton->GetNormalColor();
	Zenith_Assert(std::abs(xNormal.x - 1.f) < 0.001f && std::abs(xNormal.y) < 0.001f &&
		std::abs(xNormal.z) < 0.001f && std::abs(xNormal.w - 1.f) < 0.001f,
		"Normal color should be red");

	Zenith_Maths::Vector4 xHover = pxButton->GetHoverColor();
	Zenith_Assert(std::abs(xHover.x) < 0.001f && std::abs(xHover.y - 1.f) < 0.001f &&
		std::abs(xHover.z) < 0.001f && std::abs(xHover.w - 1.f) < 0.001f,
		"Hover color should be green");

	Zenith_Maths::Vector4 xPressed = pxButton->GetPressedColor();
	Zenith_Assert(std::abs(xPressed.x) < 0.001f && std::abs(xPressed.y) < 0.001f &&
		std::abs(xPressed.z - 1.f) < 0.001f && std::abs(xPressed.w - 1.f) < 0.001f,
		"Pressed color should be blue");

	Zenith_Assert(std::abs(pxButton->GetFontSize() - 18.f) < 0.001f, "Button font size should be 18");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIButtonStyleStep);
}

//=============================================================================
// Script/Behaviour Tests
//=============================================================================

static bool s_bTestBehaviourAwakeCalled = false;

class AutomationTestBehaviour : public Zenith_ScriptBehaviour
{
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(AutomationTestBehaviour)
	AutomationTestBehaviour(Zenith_Entity& xEntity) { m_xParentEntity = xEntity; }
	void OnAwake() override { s_bTestBehaviourAwakeCalled = true; }
};

static bool s_bTestBehaviourRegistered = false;
static void EnsureTestBehaviourRegistered()
{
	if (!s_bTestBehaviourRegistered)
	{
		AutomationTestBehaviour::RegisterBehaviour();
		s_bTestBehaviourRegistered = true;
	}
}

void Zenith_AutomationTests::TestSetBehaviourStep()
{
	EDITOR_TEST_BEGIN(TestSetBehaviourStep);

	EnsureTestBehaviourRegistered();
	Zenith_EditorAutomation::Reset();
	s_bTestBehaviourAwakeCalled = false;

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoScriptEntity");
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviour("AutomationTestBehaviour");
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity
	Zenith_EditorAutomation::ExecuteNextStep(); // Add script
	Zenith_EditorAutomation::ExecuteNextStep(); // Set behaviour

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");
	Zenith_Assert(pxEntity->HasComponent<Zenith_ScriptComponent>(), "Entity should have ScriptComponent");

	Zenith_ScriptComponent& xScript = pxEntity->GetComponent<Zenith_ScriptComponent>();
	Zenith_Assert(xScript.GetBehaviourRaw() != nullptr, "Behaviour should be set");
	Zenith_Assert(strcmp(xScript.GetBehaviourRaw()->GetBehaviourTypeName(), "AutomationTestBehaviour") == 0,
		"Behaviour type name should be 'AutomationTestBehaviour'");
	Zenith_Assert(s_bTestBehaviourAwakeCalled, "OnAwake should have been called by SetBehaviourOnSelected");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetBehaviourStep);
}

void Zenith_AutomationTests::TestSetBehaviourForSerializationStep()
{
	EDITOR_TEST_BEGIN(TestSetBehaviourForSerializationStep);

	EnsureTestBehaviourRegistered();
	Zenith_EditorAutomation::Reset();
	s_bTestBehaviourAwakeCalled = false;

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoSerEntity");
	Zenith_EditorAutomation::AddStep_AddScript();
	Zenith_EditorAutomation::AddStep_SetBehaviourForSerialization("AutomationTestBehaviour");
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity
	Zenith_EditorAutomation::ExecuteNextStep(); // Add script
	Zenith_EditorAutomation::ExecuteNextStep(); // Set behaviour for serialization

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");
	Zenith_Assert(pxEntity->HasComponent<Zenith_ScriptComponent>(), "Entity should have ScriptComponent");

	Zenith_ScriptComponent& xScript = pxEntity->GetComponent<Zenith_ScriptComponent>();
	Zenith_Assert(xScript.GetBehaviourRaw() != nullptr, "Behaviour should be set");
	Zenith_Assert(strcmp(xScript.GetBehaviourRaw()->GetBehaviourTypeName(), "AutomationTestBehaviour") == 0,
		"Behaviour type name should be 'AutomationTestBehaviour'");
	Zenith_Assert(!s_bTestBehaviourAwakeCalled, "OnAwake should NOT have been called by SetBehaviourForSerializationOnSelected");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetBehaviourForSerializationStep);
}

//=============================================================================
// Camera Extended Tests
//=============================================================================

void Zenith_AutomationTests::TestSetCameraNearFarAspectStep()
{
	EDITOR_TEST_BEGIN(TestSetCameraNearFarAspectStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoCamExtEntity");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraNear(0.5f);
	Zenith_EditorAutomation::AddStep_SetCameraFar(500.f);
	Zenith_EditorAutomation::AddStep_SetCameraAspect(1.5f);
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create
	Zenith_EditorAutomation::ExecuteNextStep(); // Add camera
	Zenith_EditorAutomation::ExecuteNextStep(); // Near
	Zenith_EditorAutomation::ExecuteNextStep(); // Far
	Zenith_EditorAutomation::ExecuteNextStep(); // Aspect

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_Assert(pxEntity != nullptr, "Should have selected entity");

	Zenith_CameraComponent& xCam = pxEntity->GetComponent<Zenith_CameraComponent>();
	Zenith_Assert(std::abs(xCam.GetNearPlane() - 0.5f) < 0.001f, "Near plane should be 0.5");
	Zenith_Assert(std::abs(xCam.GetFarPlane() - 500.f) < 0.1f, "Far plane should be 500");
	Zenith_Assert(std::abs(xCam.GetAspectRatio() - 1.5f) < 0.001f, "Aspect ratio should be 1.5");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetCameraNearFarAspectStep);
}

//=============================================================================
// Scene Round-Trip Tests
//=============================================================================

void Zenith_AutomationTests::TestSceneSaveLoadRoundTrip()
{
	EDITOR_TEST_BEGIN(TestSceneSaveLoadRoundTrip);

	Zenith_EditorAutomation::Reset();

	const char* szSavePath = ENGINE_ASSETS_DIR "_AutoRoundTrip" ZENITH_SCENE_EXT;

	// Queue: create scene, entity, camera, set FOV and position, save, unload
	Zenith_EditorAutomation::AddStep_CreateScene("RoundTripScene");
	Zenith_EditorAutomation::AddStep_CreateEntity("RTEntity");
	Zenith_EditorAutomation::AddStep_AddCamera();
	Zenith_EditorAutomation::AddStep_SetCameraFOV(1.5f);
	Zenith_EditorAutomation::AddStep_SetCameraPosition(1.f, 2.f, 3.f);
	Zenith_EditorAutomation::AddStep_SaveScene(szSavePath);
	Zenith_EditorAutomation::AddStep_UnloadScene();
	Zenith_EditorAutomation::Begin();

	// Execute all 7 steps
	for (uint32_t i = 0; i < 7; i++)
	{
		Zenith_EditorAutomation::ExecuteNextStep();
	}
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "Should be complete after last step");

	// Verify file exists
	Zenith_Assert(std::filesystem::exists(szSavePath), "Scene file should exist after save");

	// Load the saved scene and verify contents survived serialization
	Zenith_Scene xLoadedScene = Zenith_SceneManager::LoadScene(szSavePath, SCENE_LOAD_ADDITIVE);
	Zenith_Assert(xLoadedScene.IsValid(), "Loaded scene should be valid");

	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xLoadedScene);
	Zenith_Assert(pxSceneData != nullptr, "Should have scene data");

	Zenith_Entity xEntity = pxSceneData->FindEntityByName("RTEntity");
	Zenith_Assert(xEntity.IsValid(), "Should find entity 'RTEntity' in loaded scene");
	Zenith_Assert(xEntity.HasComponent<Zenith_CameraComponent>(), "Entity should have camera component");

	Zenith_CameraComponent& xCam = xEntity.GetComponent<Zenith_CameraComponent>();
	Zenith_Assert(std::abs(xCam.GetFOV() - 1.5f) < 0.001f, "Camera FOV should survive round-trip");

	Zenith_Maths::Vector3 xPos;
	xCam.GetPosition(xPos);
	Zenith_Assert(std::abs(xPos.x - 1.f) < 0.001f, "Camera pos X should survive round-trip");
	Zenith_Assert(std::abs(xPos.y - 2.f) < 0.001f, "Camera pos Y should survive round-trip");
	Zenith_Assert(std::abs(xPos.z - 3.f) < 0.001f, "Camera pos Z should survive round-trip");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xLoadedScene);
	std::filesystem::remove(szSavePath);

	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSceneSaveLoadRoundTrip);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

void Zenith_AutomationTests::TestResetDuringExecution()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestResetDuringExecution...");

	Zenith_EditorAutomation::Reset();
	s_uCustomStepCounter = 0;

	// Queue 3 steps
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::Begin();

	// Execute only 1 step
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(s_uCustomStepCounter == 1, "Counter should be 1 after first step");
	Zenith_Assert(Zenith_EditorAutomation::IsRunning(), "Should still be running");

	// Reset mid-sequence
	Zenith_EditorAutomation::Reset();
	Zenith_Assert(!Zenith_EditorAutomation::IsRunning(), "Should not be running after mid-execution Reset");
	Zenith_Assert(!Zenith_EditorAutomation::IsComplete(), "Should not be complete after mid-execution Reset");

	// Counter should not advance further
	Zenith_Assert(s_uCustomStepCounter == 1, "Counter should still be 1 after Reset");

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] TestResetDuringExecution passed");
}

void Zenith_AutomationTests::TestBeginWithZeroSteps()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestBeginWithZeroSteps...");

	Zenith_EditorAutomation::Reset();

	// Begin with no steps queued
	Zenith_EditorAutomation::Begin();
	Zenith_Assert(Zenith_EditorAutomation::IsRunning(), "Should be running after Begin even with 0 steps");

	// First ExecuteNextStep should detect empty queue and complete immediately
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(!Zenith_EditorAutomation::IsRunning(), "Should not be running after empty queue detected");
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "Should be complete after empty queue detected");

	Zenith_EditorAutomation::Reset();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] TestBeginWithZeroSteps passed");
}

void Zenith_AutomationTests::TestDoubleBeginWithoutReset()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running TestDoubleBeginWithoutReset...");

	Zenith_EditorAutomation::Reset();
	s_uCustomStepCounter = 0;

	// First sequence: add and run 1 step to completion
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::Begin();
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "First sequence should be complete");
	Zenith_Assert(s_uCustomStepCounter == 1, "Counter should be 1 after first sequence");

	// Second Begin without Reset - queue was cleared on completion, so this starts fresh
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::Begin();
	Zenith_Assert(Zenith_EditorAutomation::IsRunning(), "Should be running after second Begin");
	Zenith_Assert(!Zenith_EditorAutomation::IsComplete(), "Should not be complete after second Begin");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Assert(s_uCustomStepCounter == 2, "Counter should be 2 after second sequence");
	Zenith_Assert(Zenith_EditorAutomation::IsComplete(), "Second sequence should be complete");

	Zenith_EditorAutomation::Reset();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "[AutomationTests] TestDoubleBeginWithoutReset passed");
}

#endif // ZENITH_TOOLS
