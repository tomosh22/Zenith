
#ifdef ZENITH_TOOLS
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
#include "UI/Zenith_UIImage.h"
#include "UI/Zenith_UILayoutGroup.h"
#include "UI/Zenith_UICanvas.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "FileAccess/Zenith_FileAccess.h"
#include <cmath>
#include <filesystem>

static void NoOp() {}

//=============================================================================
// State Machine Tests
//=============================================================================
ZENITH_TEST(Automation, InitialState)
{

	Zenith_EditorAutomation::Reset();

	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsRunning(), "Should not be running after reset");
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsComplete(), "Should not be complete after reset");

}
ZENITH_TEST(Automation, BeginSetsRunning)
{

	Zenith_EditorAutomation::Reset();

	// Add a dummy step so Begin has something to work with
	Zenith_EditorAutomation::AddStep_Custom(&NoOp);
	Zenith_EditorAutomation::Begin();

	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsRunning(), "Should be running after Begin");
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsComplete(), "Should not be complete right after Begin");

	Zenith_EditorAutomation::Reset();

}
ZENITH_TEST(Automation, ResetClearsState)
{

	// Add steps and begin
	Zenith_EditorAutomation::AddStep_Custom(&NoOp);
	Zenith_EditorAutomation::AddStep_Custom(&NoOp);
	Zenith_EditorAutomation::Begin();

	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsRunning(), "Should be running");

	// Reset should clear everything
	Zenith_EditorAutomation::Reset();

	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsRunning(), "Should not be running after Reset");
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsComplete(), "Should not be complete after Reset");

}

//=============================================================================
// Step Execution Tests
//=============================================================================

static uint32_t s_uCustomStepCounter = 0;
static void IncrementCounter() { s_uCustomStepCounter++; }
ZENITH_TEST(Automation, StepExecutionOrder)
{

	Zenith_EditorAutomation::Reset();
	s_uCustomStepCounter = 0;

	// Add 3 custom steps
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);

	Zenith_EditorAutomation::Begin();

	// Execute steps one at a time
	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 1, "Counter should be 1 after first step");
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsRunning(), "Should still be running after first step");

	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 2, "Counter should be 2 after second step");
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsRunning(), "Should still be running after second step");

	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 3, "Counter should be 3 after third step");
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsRunning(), "Should not be running after all steps");
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "Should be complete after all steps");

	Zenith_EditorAutomation::Reset();

}
ZENITH_TEST(Automation, ExecuteEmptyQueue)
{

	Zenith_EditorAutomation::Reset();

	// Calling ExecuteNextStep when not running should be a no-op
	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsRunning(), "Should not be running");
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsComplete(), "Should not be complete");

	Zenith_EditorAutomation::Reset();

}
ZENITH_TEST(Automation, CompletionAfterAllSteps)
{

	Zenith_EditorAutomation::Reset();
	s_uCustomStepCounter = 0;

	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::Begin();

	// Execute the single step - completion detected immediately
	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 1, "Counter should be 1");
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "Should be complete");
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsRunning(), "Should not be running");

	// Additional calls after completion should be no-ops
	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "Should still be complete");

	Zenith_EditorAutomation::Reset();

}

//=============================================================================
// Entity Operation Tests
//=============================================================================
ZENITH_TEST(Automation, CreateEntityStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have a selected entity after CreateEntity step");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "AutoTestEntity", "Created entity should be named 'AutoTestEntity'");
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "Should be complete after single step");

	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateEntityStep);
}
ZENITH_TEST(Automation, EntitySelectionTracking)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selection after creating A");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "AutoEntityA", "Selection should be A after creating A");

	// Step 2: Create B (auto-selects B)
	Zenith_EditorAutomation::ExecuteNextStep();
	pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selection after creating B");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "AutoEntityB", "Selection should be B after creating B");

	// Step 3: Select A again
	Zenith_EditorAutomation::ExecuteNextStep();
	pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selection after selecting A");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "AutoEntityA", "Selection should be A after SelectEntity step");
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "Should be complete after last step");

	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestEntitySelectionTracking);
}

//=============================================================================
// Component Operation Tests
//=============================================================================
ZENITH_TEST(Automation, AddComponentStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_CameraComponent>(), "Entity should have CameraComponent after AddCamera step");

	// Advance to completion
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestAddComponentStep);
}

//=============================================================================
// Transform Operation Tests
//=============================================================================
ZENITH_TEST(Automation, SetTransformPositionStep)
{
	EDITOR_TEST_BEGIN(TestSetTransformPositionStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoPosEntity");
	Zenith_EditorAutomation::AddStep_SetTransformPosition(10.f, 20.f, 30.f);
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create
	Zenith_EditorAutomation::ExecuteNextStep(); // Set position

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_TransformComponent& xTransform = pxEntity->GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPos;
	xTransform.GetPosition(xPos);

	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 10.f, 0.001f, "X position should be 10");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 20.f, 0.001f, "Y position should be 20");
	ZENITH_ASSERT_EQ_FLOAT(xPos.z, 30.f, 0.001f, "Z position should be 30");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetTransformPositionStep);
}
ZENITH_TEST(Automation, SetTransformScaleStep)
{
	EDITOR_TEST_BEGIN(TestSetTransformScaleStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoScaleEntity");
	Zenith_EditorAutomation::AddStep_SetTransformScale(2.f, 3.f, 4.f);
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create
	Zenith_EditorAutomation::ExecuteNextStep(); // Set scale

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_TransformComponent& xTransform = pxEntity->GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);

	ZENITH_ASSERT_EQ_FLOAT(xScale.x, 2.f, 0.001f, "X scale should be 2");
	ZENITH_ASSERT_EQ_FLOAT(xScale.y, 3.f, 0.001f, "Y scale should be 3");
	ZENITH_ASSERT_EQ_FLOAT(xScale.z, 4.f, 0.001f, "Z scale should be 4");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetTransformScaleStep);
}

//=============================================================================
// Camera Operation Tests
//=============================================================================
ZENITH_TEST(Automation, SetCameraFOVStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	float fActual = pxEntity->GetComponent<Zenith_CameraComponent>().GetFOV();
	ZENITH_ASSERT_EQ_FLOAT(fActual, fTargetFOV, 0.001f, "FOV should match target");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetCameraFOVStep);
}
ZENITH_TEST(Automation, SetCameraPitchYawStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	Zenith_CameraComponent& xCam = pxEntity->GetComponent<Zenith_CameraComponent>();
	ZENITH_ASSERT_EQ_FLOAT(static_cast<float>(xCam.GetPitch()), fTargetPitch, 0.001f, "Pitch should match target");
	ZENITH_ASSERT_EQ_FLOAT(static_cast<float>(xCam.GetYaw()), fTargetYaw, 0.001f, "Yaw should match target");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetCameraPitchYawStep);
}
ZENITH_TEST(Automation, SetCameraPositionStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	Zenith_Maths::Vector3 xPos;
	pxEntity->GetComponent<Zenith_CameraComponent>().GetPosition(xPos);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 5.f, 0.001f, "Camera X position should be 5");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 10.f, 0.001f, "Camera Y position should be 10");
	ZENITH_ASSERT_EQ_FLOAT(xPos.z, 15.f, 0.001f, "Camera Z position should be 15");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetCameraPositionStep);
}
ZENITH_TEST(Automation, SetAsMainCameraStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneDataForEntity(pxEntity->GetEntityID());
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "Entity should be in a scene");
	ZENITH_ASSERT_EQ(pxSceneData->GetMainCameraEntity(), pxEntity->GetEntityID(), "Entity should be the main camera");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetAsMainCameraStep);
}

//=============================================================================
// Negative Path Tests
//=============================================================================
ZENITH_TEST(Automation, AddInvalidComponentStep)
{
	EDITOR_TEST_BEGIN(TestAddInvalidComponentStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoInvalidCompEntity");
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity

	// Try to add a component with an invalid name directly through the editor API
	bool bResult = Zenith_Editor::AddComponentToSelected("NonExistentComponent_XYZ");
	ZENITH_ASSERT_FALSE(bResult, "Adding invalid component should return false");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestAddInvalidComponentStep);
}

//=============================================================================
// Custom Step Tests
//=============================================================================

static bool s_bCustomStepExecuted = false;
static void SetCustomFlag() { s_bCustomStepExecuted = true; }
ZENITH_TEST(Automation, CustomStepExecution)
{

	Zenith_EditorAutomation::Reset();
	s_bCustomStepExecuted = false;

	Zenith_EditorAutomation::AddStep_Custom(&SetCustomFlag);
	Zenith_EditorAutomation::Begin();

	ZENITH_ASSERT_FALSE(s_bCustomStepExecuted, "Custom step should not execute before ExecuteNextStep");

	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_TRUE(s_bCustomStepExecuted, "Custom step function should have been called");

	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "Should be complete");

	Zenith_EditorAutomation::Reset();

}

//=============================================================================
// Scene Lifecycle Tests
//=============================================================================
ZENITH_TEST(Automation, CreateSaveUnloadCycle)
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
	ZENITH_ASSERT_TRUE(xScene.IsValid(), "Active scene should be valid after CreateScene step");

	// Step 2: Create entity
	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have created entity in new scene");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "AutoSceneEntity", "Entity name should match");

	// Step 3: Add camera
	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_CameraComponent>(), "Entity should have camera component");

	// Step 4: Save scene
	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_TRUE(std::filesystem::exists(szSavePath), "Scene file should exist after save");

	// Step 5: Unload scene
	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "Should be complete after last step");

	// Clean up the temp scene file
	std::filesystem::remove(szSavePath);

	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateSaveUnloadCycle);
}

//=============================================================================
// UI Operation Tests
//=============================================================================
ZENITH_TEST(Automation, CreateUITextStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_UIComponent>(), "Entity should have UIComponent");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>("Label1");
	ZENITH_ASSERT_NOT_NULL(pxText, "Should find UI text element 'Label1'");
	ZENITH_ASSERT_EQ(pxText->GetText(), "Hello", "Text content should be 'Hello'");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateUITextStep);
}
ZENITH_TEST(Automation, CreateUIButtonStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>("Btn1");
	ZENITH_ASSERT_NOT_NULL(pxButton, "Should find UI button 'Btn1'");
	ZENITH_ASSERT_EQ(pxButton->GetText(), "Click Me", "Button text should be 'Click Me'");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateUIButtonStep);
}
ZENITH_TEST(Automation, CreateUIRectStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>("Rect1");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect 'Rect1'");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateUIRectStep);
}
ZENITH_TEST(Automation, SetUIPropertiesStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>("Txt");
	ZENITH_ASSERT_NOT_NULL(pxText, "Should find UI text 'Txt'");

	Zenith_Maths::Vector2 xPos = pxText->GetPosition();
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 100.f, 0.001f, "UI position X should be 100");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 200.f, 0.001f, "UI position Y should be 200");

	Zenith_Maths::Vector2 xSize = pxText->GetSize();
	ZENITH_ASSERT_EQ_FLOAT(xSize.x, 300.f, 0.001f, "UI size W should be 300");
	ZENITH_ASSERT_EQ_FLOAT(xSize.y, 50.f, 0.001f, "UI size H should be 50");

	ZENITH_ASSERT_EQ_FLOAT(pxText->GetFontSize(), 32.f, 0.001f, "Font size should be 32");

	Zenith_Maths::Vector4 xColor = pxText->GetColor();
	ZENITH_ASSERT_EQ_FLOAT(xColor.x, 1.f, 0.001f, "Color R should be 1");
	ZENITH_ASSERT_EQ_FLOAT(xColor.y, 0.f, 0.001f, "Color G should be 0");
	ZENITH_ASSERT_EQ_FLOAT(xColor.z, 0.f, 0.001f, "Color B should be 0");
	ZENITH_ASSERT_EQ_FLOAT(xColor.w, 1.f, 0.001f, "Color A should be 1");

	ZENITH_ASSERT_FALSE(pxText->IsVisible(), "Element should not be visible");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIPropertiesStep);
}
ZENITH_TEST(Automation, SetUIButtonStyleStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>("Btn");
	ZENITH_ASSERT_NOT_NULL(pxButton, "Should find UI button 'Btn'");

	Zenith_Maths::Vector4 xNormal = pxButton->GetNormalColor();
	ZENITH_ASSERT_TRUE(std::abs(xNormal.x - 1.f) < 0.001f && std::abs(xNormal.y) < 0.001f &&
		std::abs(xNormal.z) < 0.001f && std::abs(xNormal.w - 1.f) < 0.001f, "Normal color should be red");

	Zenith_Maths::Vector4 xHover = pxButton->GetHoverColor();
	ZENITH_ASSERT_TRUE(std::abs(xHover.x) < 0.001f && std::abs(xHover.y - 1.f) < 0.001f &&
		std::abs(xHover.z) < 0.001f && std::abs(xHover.w - 1.f) < 0.001f, "Hover color should be green");

	Zenith_Maths::Vector4 xPressed = pxButton->GetPressedColor();
	ZENITH_ASSERT_TRUE(std::abs(xPressed.x) < 0.001f && std::abs(xPressed.y) < 0.001f &&
		std::abs(xPressed.z - 1.f) < 0.001f && std::abs(xPressed.w - 1.f) < 0.001f, "Pressed color should be blue");

	ZENITH_ASSERT_EQ_FLOAT(pxButton->GetFontSize(), 18.f, 0.001f, "Button font size should be 18");

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
	ZENITH_BEHAVIOUR_TYPE_NAME_INTERNAL(AutomationTestBehaviour)
	AutomationTestBehaviour(Zenith_Entity& xEntity) { m_xParentEntity = xEntity; }
	void OnAwake() override { s_bTestBehaviourAwakeCalled = true; }
};

// AutomationTestBehaviour auto-registers via the macro's static initializer.
// No explicit registration call needed.
ZENITH_TEST(Automation, AttachScriptStep)
{
	EDITOR_TEST_BEGIN(TestAttachScriptStep);

	Zenith_EditorAutomation::Reset();
	s_bTestBehaviourAwakeCalled = false;

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoSerEntity");
	Zenith_EditorAutomation::AddStep_AttachScript("AutomationTestBehaviour");
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity
	Zenith_EditorAutomation::ExecuteNextStep(); // Attach script (adds ScriptComponent + slot, no OnAwake)

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_ScriptComponent>(), "Entity should have ScriptComponent");

	Zenith_ScriptComponent& xScript = pxEntity->GetComponent<Zenith_ScriptComponent>();
	ZENITH_ASSERT_EQ(xScript.GetScriptCount(), 1u, "Should have exactly one script slot");

	Zenith_ScriptBehaviour* pxBehaviour = xScript.GetScriptAt(0);
	ZENITH_ASSERT_NOT_NULL(pxBehaviour, "Slot behaviour should be set");
	ZENITH_ASSERT_STREQ(pxBehaviour->GetBehaviourTypeName(), "AutomationTestBehaviour", "Behaviour type name should be 'AutomationTestBehaviour'");
	ZENITH_ASSERT_FALSE(s_bTestBehaviourAwakeCalled, "OnAwake should NOT have been called by AttachScriptForSerializationToSelected");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestAttachScriptStep);
}

//=============================================================================
// Camera Extended Tests
//=============================================================================
ZENITH_TEST(Automation, SetCameraNearFarAspectStep)
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
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_CameraComponent& xCam = pxEntity->GetComponent<Zenith_CameraComponent>();
	ZENITH_ASSERT_EQ_FLOAT(xCam.GetNearPlane(), 0.5f, 0.001f, "Near plane should be 0.5");
	ZENITH_ASSERT_EQ_FLOAT(xCam.GetFarPlane(), 500.f, 0.1f, "Far plane should be 500");
	ZENITH_ASSERT_EQ_FLOAT(xCam.GetAspectRatio(), 1.5f, 0.001f, "Aspect ratio should be 1.5");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetCameraNearFarAspectStep);
}

//=============================================================================
// Scene Round-Trip Tests
//=============================================================================
ZENITH_TEST(Automation, SceneSaveLoadRoundTrip)
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
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "Should be complete after last step");

	// Verify file exists
	ZENITH_ASSERT_TRUE(std::filesystem::exists(szSavePath), "Scene file should exist after save");

	// Load the saved scene and verify contents survived serialization
	Zenith_Scene xLoadedScene = Zenith_SceneManager::LoadSceneBlockingForBootstrap(szSavePath, SCENE_LOAD_ADDITIVE);
	ZENITH_ASSERT_TRUE(xLoadedScene.IsValid(), "Loaded scene should be valid");

	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xLoadedScene);
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "Should have scene data");

	Zenith_Entity xEntity = pxSceneData->FindEntityByName("RTEntity");
	ZENITH_ASSERT_TRUE(xEntity.IsValid(), "Should find entity 'RTEntity' in loaded scene");
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_CameraComponent>(), "Entity should have camera component");

	Zenith_CameraComponent& xCam = xEntity.GetComponent<Zenith_CameraComponent>();
	ZENITH_ASSERT_EQ_FLOAT(xCam.GetFOV(), 1.5f, 0.001f, "Camera FOV should survive round-trip");

	Zenith_Maths::Vector3 xPos;
	xCam.GetPosition(xPos);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 1.f, 0.001f, "Camera pos X should survive round-trip");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 2.f, 0.001f, "Camera pos Y should survive round-trip");
	ZENITH_ASSERT_EQ_FLOAT(xPos.z, 3.f, 0.001f, "Camera pos Z should survive round-trip");

	// Cleanup
	Zenith_SceneManager::UnloadScene(xLoadedScene);
	std::filesystem::remove(szSavePath);

	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSceneSaveLoadRoundTrip);
}

//=============================================================================
// Edge Case Tests
//=============================================================================
ZENITH_TEST(Automation, ResetDuringExecution)
{

	Zenith_EditorAutomation::Reset();
	s_uCustomStepCounter = 0;

	// Queue 3 steps
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::Begin();

	// Execute only 1 step
	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 1, "Counter should be 1 after first step");
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsRunning(), "Should still be running");

	// Reset mid-sequence
	Zenith_EditorAutomation::Reset();
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsRunning(), "Should not be running after mid-execution Reset");
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsComplete(), "Should not be complete after mid-execution Reset");

	// Counter should not advance further
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 1, "Counter should still be 1 after Reset");

}
ZENITH_TEST(Automation, BeginWithZeroSteps)
{

	Zenith_EditorAutomation::Reset();

	// Begin with no steps queued
	Zenith_EditorAutomation::Begin();
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsRunning(), "Should be running after Begin even with 0 steps");

	// First ExecuteNextStep should detect empty queue and complete immediately
	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsRunning(), "Should not be running after empty queue detected");
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "Should be complete after empty queue detected");

	Zenith_EditorAutomation::Reset();

}
ZENITH_TEST(Automation, DoubleBeginWithoutReset)
{

	Zenith_EditorAutomation::Reset();
	s_uCustomStepCounter = 0;

	// First sequence: add and run 1 step to completion
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::Begin();
	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "First sequence should be complete");
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 1, "Counter should be 1 after first sequence");

	// Second Begin without Reset - queue was cleared on completion, so this starts fresh
	Zenith_EditorAutomation::AddStep_Custom(&IncrementCounter);
	Zenith_EditorAutomation::Begin();
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsRunning(), "Should be running after second Begin");
	ZENITH_ASSERT_FALSE(Zenith_EditorAutomation::IsComplete(), "Should not be complete after second Begin");

	Zenith_EditorAutomation::ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 2, "Counter should be 2 after second sequence");
	ZENITH_ASSERT_TRUE(Zenith_EditorAutomation::IsComplete(), "Second sequence should be complete");

	Zenith_EditorAutomation::Reset();

}

//=============================================================================
// UI Image Operation Tests
//=============================================================================
ZENITH_TEST(Automation, CreateUIImageStep)
{
	EDITOR_TEST_BEGIN(TestCreateUIImageStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoUIImageEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIImage("Img1");
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity
	Zenith_EditorAutomation::ExecuteNextStep(); // Add UI
	Zenith_EditorAutomation::ExecuteNextStep(); // Create image

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIImage* pxImage = xUI.FindElement<Zenith_UI::Zenith_UIImage>("Img1");
	ZENITH_ASSERT_NOT_NULL(pxImage, "Should find UI image 'Img1'");
	ZENITH_ASSERT_EQ(pxImage->GetType(), Zenith_UI::UIElementType::Image, "Element type should be Image");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateUIImageStep);
}
ZENITH_TEST(Automation, SetUIImageTexturePathStep)
{
	EDITOR_TEST_BEGIN(TestSetUIImageTexturePathStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoUIImgTexEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIImage("TexImg");
	Zenith_EditorAutomation::AddStep_SetUIImageTexturePath("TexImg",
		ENGINE_ASSETS_DIR "Textures/Font/FontAtlas.ztxtr");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIImage* pxImage = xUI.FindElement<Zenith_UI::Zenith_UIImage>("TexImg");
	ZENITH_ASSERT_NOT_NULL(pxImage, "Should find UI image 'TexImg'");
	ZENITH_ASSERT_EQ(pxImage->GetTexturePath(), "engine:Textures/Font/FontAtlas.ztxtr", "Texture path should be set");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIImageTexturePathStep);
}

//=============================================================================
// Particle Config By Name Tests
//=============================================================================
ZENITH_TEST(Automation, SetParticleConfigByNameStep)
{
	EDITOR_TEST_BEGIN(TestSetParticleConfigByNameStep);

	// Register a temporary test config
	Flux_ParticleEmitterConfig xTestConfig;
	xTestConfig.m_uBurstCount = 42;
	Flux_ParticleEmitterConfig::Register("AutoTestConfig", &xTestConfig);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoParticleEntity");
	Zenith_EditorAutomation::AddStep_AddParticleEmitter();
	Zenith_EditorAutomation::AddStep_SetParticleConfigByName("AutoTestConfig");
	Zenith_EditorAutomation::AddStep_SetParticleEmitting(false);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_ParticleEmitterComponent>(), "Entity should have ParticleEmitterComponent");

	Zenith_ParticleEmitterComponent& xEmitter =
		pxEntity->GetComponent<Zenith_ParticleEmitterComponent>();
	ZENITH_ASSERT_NOT_NULL(xEmitter.GetConfig(), "Particle emitter should have config assigned");
	ZENITH_ASSERT_EQ(xEmitter.GetConfig()->m_uBurstCount, 42, "Config burst count should be 42");
	ZENITH_ASSERT_FALSE(xEmitter.IsEmitting(), "Emitter should not be emitting");

	// Cleanup
	Flux_ParticleEmitterConfig::Unregister("AutoTestConfig");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetParticleConfigByNameStep);
}

//=============================================================================
// Layout Group Tests
//=============================================================================
ZENITH_TEST(Automation, CreateUILayoutGroupStep)
{
	EDITOR_TEST_BEGIN(TestCreateUILayoutGroupStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoLayoutEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("TestLayout");
	Zenith_EditorAutomation::Begin();

	Zenith_EditorAutomation::ExecuteNextStep(); // Create entity
	Zenith_EditorAutomation::ExecuteNextStep(); // Add UI
	Zenith_EditorAutomation::ExecuteNextStep(); // Create layout group

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("TestLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group 'TestLayout'");
	ZENITH_ASSERT_EQ(pxLayout->GetType(), Zenith_UI::UIElementType::LayoutGroup, "Element type should be LayoutGroup");
	ZENITH_ASSERT_EQ(pxLayout->GetDirection(), Zenith_UI::LayoutDirection::Horizontal, "Default direction should be Horizontal");
	ZENITH_ASSERT_EQ(pxLayout->GetChildAlignment(), Zenith_UI::ChildAlignment::MiddleCenter, "Default child alignment should be MiddleCenter");
	ZENITH_ASSERT_LT(std::abs(pxLayout->GetSpacing()), 0.001f, "Default spacing should be 0");
	Zenith_Maths::Vector4 xPad = pxLayout->GetPadding();
	ZENITH_ASSERT_TRUE(std::abs(xPad.x) < 0.001f && std::abs(xPad.y) < 0.001f && std::abs(xPad.z) < 0.001f && std::abs(xPad.w) < 0.001f, "Default padding should be all zeros");
	ZENITH_ASSERT_EQ(pxLayout->GetFitToContent(), true, "Default fit-to-content should be true");
	ZENITH_ASSERT_EQ(pxLayout->GetChildForceExpandWidth(), false, "Default childForceExpandWidth should be false");
	ZENITH_ASSERT_EQ(pxLayout->GetChildForceExpandHeight(), false, "Default childForceExpandHeight should be false");
	ZENITH_ASSERT_EQ(pxLayout->GetReverseArrangement(), false, "Default reverseArrangement should be false");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestCreateUILayoutGroupStep);
}
ZENITH_TEST(Automation, AddUIChildStep)
{
	EDITOR_TEST_BEGIN(TestAddUIChildStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoChildEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("Parent");
	Zenith_EditorAutomation::AddStep_CreateUIText("Child1", "Hello");
	Zenith_EditorAutomation::AddStep_CreateUIImage("Child2");
	Zenith_EditorAutomation::AddStep_AddUIChild("Parent", "Child1");
	Zenith_EditorAutomation::AddStep_AddUIChild("Parent", "Child2");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 7; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("Parent");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group 'Parent'");
	ZENITH_ASSERT_EQ(pxLayout->GetChildCount(), 2, "Layout group should have 2 children");
	ZENITH_ASSERT_EQ(pxLayout->GetChild(0)->GetName(), "Child1", "First child should be 'Child1'");
	ZENITH_ASSERT_EQ(pxLayout->GetChild(1)->GetName(), "Child2", "Second child should be 'Child2'");
	ZENITH_ASSERT_EQ(pxLayout->GetChild(0)->GetParent(), pxLayout, "Child1 parent should be layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetChild(1)->GetParent(), pxLayout, "Child2 parent should be layout group");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestAddUIChildStep);
}
ZENITH_TEST(Automation, SetUILayoutDirectionStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutDirectionStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoDirEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("DirLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("DirLayout", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("DirLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetDirection(), Zenith_UI::LayoutDirection::Vertical, "Direction should be Vertical");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUILayoutDirectionStep);
}
ZENITH_TEST(Automation, SetUILayoutSpacingStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutSpacingStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoSpaceEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("SpaceLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("SpaceLayout", 15.f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("SpaceLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSpacing(), 15.f, 0.001f, "Spacing should be 15");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUILayoutSpacingStep);
}
ZENITH_TEST(Automation, SetUILayoutChildAlignmentStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutChildAlignmentStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoAlignEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("AlignLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("AlignLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("AlignLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetChildAlignment(), Zenith_UI::ChildAlignment::UpperLeft, "Child alignment should be UpperLeft");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUILayoutChildAlignmentStep);
}
ZENITH_TEST(Automation, SetUILayoutPaddingStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutPaddingStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoPadEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("PadLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutPadding("PadLayout", 10.f, 20.f, 30.f, 40.f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("PadLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	Zenith_Maths::Vector4 xPadding = pxLayout->GetPadding();
	ZENITH_ASSERT_EQ_FLOAT(xPadding.x, 10.f, 0.001f, "Padding left should be 10");
	ZENITH_ASSERT_EQ_FLOAT(xPadding.y, 20.f, 0.001f, "Padding top should be 20");
	ZENITH_ASSERT_EQ_FLOAT(xPadding.z, 30.f, 0.001f, "Padding right should be 30");
	ZENITH_ASSERT_EQ_FLOAT(xPadding.w, 40.f, 0.001f, "Padding bottom should be 40");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUILayoutPaddingStep);
}
ZENITH_TEST(Automation, SetUILayoutFitToContentStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutFitToContentStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoFitEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("FitLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("FitLayout", false);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("FitLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetFitToContent(), false, "FitToContent should be false");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUILayoutFitToContentStep);
}
ZENITH_TEST(Automation, SetUILayoutChildForceExpandStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutChildForceExpandStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoExpandEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("ExpandLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutChildForceExpand("ExpandLayout", true, false);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("ExpandLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetChildForceExpandWidth(), true, "ChildForceExpandWidth should be true");
	ZENITH_ASSERT_EQ(pxLayout->GetChildForceExpandHeight(), false, "ChildForceExpandHeight should be false");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUILayoutChildForceExpandStep);
}
ZENITH_TEST(Automation, SetUILayoutReverseStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutReverseStep);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoRevEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("RevLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutReverse("RevLayout", true);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("RevLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetReverseArrangement(), true, "ReverseArrangement should be true");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUILayoutReverseStep);
}
ZENITH_TEST(Automation, LayoutHorizontalPositioning)
{
	EDITOR_TEST_BEGIN(TestLayoutHorizontalPositioning);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoHPosEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("HLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("HLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("HLayout", 10.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("HLayout", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("HLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::AddStep_CreateUIRect("RectA");
	Zenith_EditorAutomation::AddStep_SetUISize("RectA", 50.f, 30.f);
	Zenith_EditorAutomation::AddStep_CreateUIRect("RectB");
	Zenith_EditorAutomation::AddStep_SetUISize("RectB", 80.f, 40.f);
	Zenith_EditorAutomation::AddStep_CreateUIRect("RectC");
	Zenith_EditorAutomation::AddStep_SetUISize("RectC", 60.f, 20.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("HLayout", "RectA");
	Zenith_EditorAutomation::AddStep_AddUIChild("HLayout", "RectB");
	Zenith_EditorAutomation::AddStep_AddUIChild("HLayout", "RectC");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 16; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("HLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	// Trigger layout recalculation
	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxA = pxLayout->GetChild(0);
	Zenith_UI::Zenith_UIElement* pxB = pxLayout->GetChild(1);
	Zenith_UI::Zenith_UIElement* pxC = pxLayout->GetChild(2);

	ZENITH_ASSERT_EQ_FLOAT(pxA->GetPosition().x, 0.f, 0.001f, "Child A position.x should be 0");
	ZENITH_ASSERT_EQ_FLOAT(pxB->GetPosition().x, 60.f, 0.001f, "Child B position.x should be 60 (50 + 10 spacing)");
	ZENITH_ASSERT_EQ_FLOAT(pxC->GetPosition().x, 150.f, 0.001f, "Child C position.x should be 150 (60 + 80 + 10)");

	// Fit-to-content: total width = 50 + 10 + 80 + 10 + 60 = 210
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().x, 210.f, 0.001f, "Layout width should be 210");
	// Max child height = 40
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().y, 40.f, 0.001f, "Layout height should be 40");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutHorizontalPositioning);
}
ZENITH_TEST(Automation, LayoutVerticalPositioning)
{
	EDITOR_TEST_BEGIN(TestLayoutVerticalPositioning);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoVPosEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("VLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("VLayout", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("VLayout", 5.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("VLayout", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("VLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::AddStep_CreateUIRect("VA");
	Zenith_EditorAutomation::AddStep_SetUISize("VA", 100.f, 20.f);
	Zenith_EditorAutomation::AddStep_CreateUIRect("VB");
	Zenith_EditorAutomation::AddStep_SetUISize("VB", 80.f, 30.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("VLayout", "VA");
	Zenith_EditorAutomation::AddStep_AddUIChild("VLayout", "VB");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 13; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("VLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxA = pxLayout->GetChild(0);
	Zenith_UI::Zenith_UIElement* pxB = pxLayout->GetChild(1);

	ZENITH_ASSERT_EQ_FLOAT(pxA->GetPosition().y, 0.f, 0.001f, "Child A position.y should be 0");
	ZENITH_ASSERT_EQ_FLOAT(pxB->GetPosition().y, 25.f, 0.001f, "Child B position.y should be 25 (20 + 5 spacing)");

	// Fit-to-content: total height = 20 + 5 + 30 = 55
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().y, 55.f, 0.001f, "Layout height should be 55");
	// Max child width = 100
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().x, 100.f, 0.001f, "Layout width should be 100");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutVerticalPositioning);
}
ZENITH_TEST(Automation, LayoutPaddingAffectsPositioning)
{
	EDITOR_TEST_BEGIN(TestLayoutPaddingAffectsPositioning);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoPadPosEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("PadPosLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("PadPosLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("PadPosLayout", 0.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutPadding("PadPosLayout", 10.f, 20.f, 0.f, 0.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("PadPosLayout", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("PadPosLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::AddStep_CreateUIRect("PadChild");
	Zenith_EditorAutomation::AddStep_SetUISize("PadChild", 50.f, 30.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("PadPosLayout", "PadChild");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 11; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("PadPosLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxChild = pxLayout->GetChild(0);
	ZENITH_ASSERT_EQ_FLOAT(pxChild->GetPosition().x, 10.f, 0.001f, "Child position.x should be 10 (left padding)");
	ZENITH_ASSERT_EQ_FLOAT(pxChild->GetPosition().y, 20.f, 0.001f, "Child position.y should be 20 (top padding)");

	// Layout size should include padding: 10 + 50 + 0 = 60, 20 + 30 + 0 = 50
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().x, 60.f, 0.001f, "Layout width should include padding");
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().y, 50.f, 0.001f, "Layout height should include padding");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutPaddingAffectsPositioning);
}
ZENITH_TEST(Automation, LayoutMiddleCenterAlignment)
{
	EDITOR_TEST_BEGIN(TestLayoutMiddleCenterAlignment);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoMCEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("MCLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("MCLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("MCLayout", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("MCLayout", false);
	Zenith_EditorAutomation::AddStep_SetUISize("MCLayout", 400.f, 100.f);
	Zenith_EditorAutomation::AddStep_CreateUIRect("MCChild");
	Zenith_EditorAutomation::AddStep_SetUISize("MCChild", 50.f, 30.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("MCLayout", "MCChild");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 10; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("MCLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxChild = pxLayout->GetChild(0);
	// Cross-axis (vertical) should be centered: (100 - 30) / 2 = 35
	ZENITH_ASSERT_EQ_FLOAT(pxChild->GetPosition().y, 35.f, 0.001f, "Child position.y should be 35 (centered on cross axis)");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutMiddleCenterAlignment);
}
ZENITH_TEST(Automation, LayoutUpperLeftAlignment)
{
	EDITOR_TEST_BEGIN(TestLayoutUpperLeftAlignment);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoULEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("ULLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("ULLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("ULLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("ULLayout", false);
	Zenith_EditorAutomation::AddStep_SetUISize("ULLayout", 400.f, 100.f);
	Zenith_EditorAutomation::AddStep_CreateUIRect("ULChild");
	Zenith_EditorAutomation::AddStep_SetUISize("ULChild", 50.f, 30.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("ULLayout", "ULChild");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 10; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("ULLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxChild = pxLayout->GetChild(0);
	// Upper = top-aligned, cross-axis Y should be 0
	ZENITH_ASSERT_EQ_FLOAT(pxChild->GetPosition().y, 0.f, 0.001f, "Child position.y should be 0 (top-aligned)");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutUpperLeftAlignment);
}
ZENITH_TEST(Automation, LayoutLowerRightAlignment)
{
	EDITOR_TEST_BEGIN(TestLayoutLowerRightAlignment);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoLREntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("LRLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("LRLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("LRLayout", static_cast<int>(Zenith_UI::ChildAlignment::LowerRight));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("LRLayout", false);
	Zenith_EditorAutomation::AddStep_SetUISize("LRLayout", 400.f, 100.f);
	Zenith_EditorAutomation::AddStep_CreateUIRect("LRChild");
	Zenith_EditorAutomation::AddStep_SetUISize("LRChild", 50.f, 30.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("LRLayout", "LRChild");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 10; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("LRLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxChild = pxLayout->GetChild(0);
	// Lower = bottom-aligned, cross-axis Y should be 100 - 30 = 70
	ZENITH_ASSERT_EQ_FLOAT(pxChild->GetPosition().y, 70.f, 0.001f, "Child position.y should be 70 (bottom-aligned)");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutLowerRightAlignment);
}
ZENITH_TEST(Automation, LayoutReverseArrangement)
{
	EDITOR_TEST_BEGIN(TestLayoutReverseArrangement);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoRevArrEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("RevArrLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("RevArrLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("RevArrLayout", 10.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutReverse("RevArrLayout", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("RevArrLayout", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("RevArrLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::AddStep_CreateUIRect("RevA");
	Zenith_EditorAutomation::AddStep_SetUISize("RevA", 50.f, 30.f);
	Zenith_EditorAutomation::AddStep_CreateUIRect("RevB");
	Zenith_EditorAutomation::AddStep_SetUISize("RevB", 80.f, 30.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("RevArrLayout", "RevA");
	Zenith_EditorAutomation::AddStep_AddUIChild("RevArrLayout", "RevB");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 14; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("RevArrLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxA = pxLayout->GetChild(0);
	Zenith_UI::Zenith_UIElement* pxB = pxLayout->GetChild(1);

	// Reversed: B is placed first (position.x == 0), then A (position.x == 90)
	ZENITH_ASSERT_EQ_FLOAT(pxB->GetPosition().x, 0.f, 0.001f, "Child B should be placed first at x=0 (reversed)");
	ZENITH_ASSERT_EQ_FLOAT(pxA->GetPosition().x, 90.f, 0.001f, "Child A should be placed second at x=90 (reversed)");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutReverseArrangement);
}
ZENITH_TEST(Automation, LayoutChildForceExpand)
{
	EDITOR_TEST_BEGIN(TestLayoutChildForceExpand);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoForceExpEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("ForceExpLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("ForceExpLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutChildForceExpand("ForceExpLayout", true, false);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("ForceExpLayout", false);
	Zenith_EditorAutomation::AddStep_SetUISize("ForceExpLayout", 300.f, 50.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("ForceExpLayout", 0.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("ForceExpLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::AddStep_CreateUIRect("ExpA");
	Zenith_EditorAutomation::AddStep_SetUISize("ExpA", 50.f, 30.f);
	Zenith_EditorAutomation::AddStep_CreateUIRect("ExpB");
	Zenith_EditorAutomation::AddStep_SetUISize("ExpB", 50.f, 30.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("ForceExpLayout", "ExpA");
	Zenith_EditorAutomation::AddStep_AddUIChild("ForceExpLayout", "ExpB");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 15; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("ForceExpLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxA = pxLayout->GetChild(0);
	Zenith_UI::Zenith_UIElement* pxB = pxLayout->GetChild(1);

	// Each child should get 300/2 = 150 width
	ZENITH_ASSERT_EQ_FLOAT(pxA->GetSize().x, 150.f, 0.001f, "Child A width should be 150 (force expanded)");
	ZENITH_ASSERT_EQ_FLOAT(pxB->GetSize().x, 150.f, 0.001f, "Child B width should be 150 (force expanded)");
	ZENITH_ASSERT_EQ_FLOAT(pxA->GetPosition().x, 0.f, 0.001f, "Child A position.x should be 0");
	ZENITH_ASSERT_EQ_FLOAT(pxB->GetPosition().x, 150.f, 0.001f, "Child B position.x should be 150");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutChildForceExpand);
}
ZENITH_TEST(Automation, LayoutFitToContentResizing)
{
	EDITOR_TEST_BEGIN(TestLayoutFitToContentResizing);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoFitResEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("FitResLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("FitResLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("FitResLayout", 10.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("FitResLayout", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("FitResLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::AddStep_CreateUIRect("FitA");
	Zenith_EditorAutomation::AddStep_SetUISize("FitA", 100.f, 40.f);
	Zenith_EditorAutomation::AddStep_CreateUIRect("FitB");
	Zenith_EditorAutomation::AddStep_SetUISize("FitB", 80.f, 30.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("FitResLayout", "FitA");
	Zenith_EditorAutomation::AddStep_AddUIChild("FitResLayout", "FitB");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 13; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("FitResLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	// First check: width = 100 + 10 + 80 = 190, height = 40
	float fFirstWidth = pxLayout->GetSize().x;
	ZENITH_ASSERT_EQ_FLOAT(fFirstWidth, 190.f, 0.001f, "Initial layout width should be 190");

	// Now add another child directly (not via automation, since we're mid-test)
	Zenith_UI::Zenith_UIRect* pxNewChild = new Zenith_UI::Zenith_UIRect("FitC");
	pxNewChild->SetSize(60.f, 25.f);
	xUI.GetCanvas().AddElement(pxNewChild);
	xUI.GetCanvas().ReparentElement(pxNewChild, pxLayout);
	pxLayout->MarkLayoutDirty();
	pxLayout->Update(0.f);

	// After adding 60px child: width = 100 + 10 + 80 + 10 + 60 = 260
	float fSecondWidth = pxLayout->GetSize().x;
	ZENITH_ASSERT_GT(fSecondWidth, fFirstWidth, "Layout width should grow after adding child");
	ZENITH_ASSERT_EQ_FLOAT(fSecondWidth, 260.f, 0.001f, "New layout width should be 260");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutFitToContentResizing);
}
ZENITH_TEST(Automation, LayoutWithTextChild)
{
	EDITOR_TEST_BEGIN(TestLayoutWithTextChild);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoTextLayoutEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("TextLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("TextLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("TextLayout", 8.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("TextLayout", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("TextLayout", true);
	Zenith_EditorAutomation::AddStep_CreateUIImage("TLImg");
	Zenith_EditorAutomation::AddStep_SetUISize("TLImg", 36.f, 36.f);
	Zenith_EditorAutomation::AddStep_CreateUIText("TLText", "Test");
	Zenith_EditorAutomation::AddStep_SetUIFontSize("TLText", 36.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("TextLayout", "TLImg");
	Zenith_EditorAutomation::AddStep_AddUIChild("TextLayout", "TLText");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 13; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("TextLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIText* pxText = static_cast<Zenith_UI::Zenith_UIText*>(pxLayout->GetChild(1));
	ZENITH_ASSERT_NOT_NULL(pxText, "Should find text child");

	// Text width should be calculated from GetTextWidth()
	float fExpectedTextWidth = pxText->GetTextWidth();
	ZENITH_ASSERT_GT(fExpectedTextWidth, 0.f, "Text width should be positive");

	// Image (36px) + spacing (8px) + text width = total layout width
	float fExpectedWidth = 36.f + 8.f + fExpectedTextWidth;
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().x, fExpectedWidth, 0.001f, "Layout width should include text width");

	// Non-text children should be shifted up by the glyph correction when text siblings exist
	Zenith_UI::Zenith_UIElement* pxImgChild = pxLayout->GetChild(0);
	ZENITH_ASSERT_LT(pxImgChild->GetPosition().y, pxText->GetPosition().y, "Image should be shifted up relative to text for glyph alignment");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutWithTextChild);
}
ZENITH_TEST(Automation, LayoutEmptyGroup)
{
	EDITOR_TEST_BEGIN(TestLayoutEmptyGroup);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoEmptyEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("EmptyLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutPadding("EmptyLayout", 5.f, 10.f, 15.f, 20.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("EmptyLayout", true);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 5; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("EmptyLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	// Should not crash with no children
	pxLayout->Update(0.f);

	// With fit-to-content, size should be padding only: (5+15, 10+20) = (20, 30)
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().x, 20.f, 0.001f, "Empty layout width should be paddingLeft + paddingRight");
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().y, 30.f, 0.001f, "Empty layout height should be paddingTop + paddingBottom");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutEmptyGroup);
}
ZENITH_TEST(Automation, LayoutSingleChild)
{
	EDITOR_TEST_BEGIN(TestLayoutSingleChild);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoSingleEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("SingleLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("SingleLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("SingleLayout", 100.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("SingleLayout", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("SingleLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::AddStep_CreateUIRect("SingleChild");
	Zenith_EditorAutomation::AddStep_SetUISize("SingleChild", 50.f, 30.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("SingleLayout", "SingleChild");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 10; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("SingleLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxChild = pxLayout->GetChild(0);
	ZENITH_ASSERT_EQ_FLOAT(pxChild->GetPosition().x, 0.f, 0.001f, "Single child position.x should be 0");
	ZENITH_ASSERT_EQ_FLOAT(pxChild->GetPosition().y, 0.f, 0.001f, "Single child position.y should be 0");

	// Spacing should have no effect with only 1 child: width = 50, not 50 + 100
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().x, 50.f, 0.001f, "Layout width should be 50 (spacing has no effect with 1 child)");
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().y, 30.f, 0.001f, "Layout height should be 30");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutSingleChild);
}
ZENITH_TEST(Automation, LayoutInvisibleChildrenSkipped)
{
	EDITOR_TEST_BEGIN(TestLayoutInvisibleChildrenSkipped);

	Zenith_EditorAutomation::Reset();

	Zenith_EditorAutomation::AddStep_CreateEntity("AutoInvisEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("InvisLayout");
	Zenith_EditorAutomation::AddStep_SetUILayoutDirection("InvisLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	Zenith_EditorAutomation::AddStep_SetUILayoutSpacing("InvisLayout", 10.f);
	Zenith_EditorAutomation::AddStep_SetUILayoutFitToContent("InvisLayout", true);
	Zenith_EditorAutomation::AddStep_SetUILayoutChildAlignment("InvisLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	Zenith_EditorAutomation::AddStep_CreateUIRect("InvisA");
	Zenith_EditorAutomation::AddStep_SetUISize("InvisA", 50.f, 30.f);
	Zenith_EditorAutomation::AddStep_CreateUIRect("InvisB");
	Zenith_EditorAutomation::AddStep_SetUISize("InvisB", 80.f, 30.f);
	Zenith_EditorAutomation::AddStep_SetUIVisible("InvisB", false);
	Zenith_EditorAutomation::AddStep_CreateUIRect("InvisC");
	Zenith_EditorAutomation::AddStep_SetUISize("InvisC", 60.f, 30.f);
	Zenith_EditorAutomation::AddStep_AddUIChild("InvisLayout", "InvisA");
	Zenith_EditorAutomation::AddStep_AddUIChild("InvisLayout", "InvisB");
	Zenith_EditorAutomation::AddStep_AddUIChild("InvisLayout", "InvisC");
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 17; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("InvisLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxA = pxLayout->GetChild(0);
	Zenith_UI::Zenith_UIElement* pxC = pxLayout->GetChild(2);

	ZENITH_ASSERT_EQ_FLOAT(pxA->GetPosition().x, 0.f, 0.001f, "Child A position.x should be 0");
	// B is invisible, so C follows A directly: 50 + 10 = 60
	ZENITH_ASSERT_EQ_FLOAT(pxC->GetPosition().x, 60.f, 0.001f, "Child C position.x should be 60 (B skipped)");

	// Layout width = 50 + 10 + 60 = 120 (B excluded)
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().x, 120.f, 0.001f, "Layout width should be 120 (invisible child excluded)");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutInvisibleChildrenSkipped);
}
ZENITH_TEST(Automation, LayoutSerializationRoundTrip)
{
	EDITOR_TEST_BEGIN(TestLayoutSerializationRoundTrip);

	Zenith_EditorAutomation::Reset();

	// Create a layout group with non-default values
	Zenith_UI::Zenith_UILayoutGroup xOriginal("SerLayout");
	xOriginal.SetDirection(Zenith_UI::LayoutDirection::Vertical);
	xOriginal.SetChildAlignment(Zenith_UI::ChildAlignment::LowerRight);
	xOriginal.SetPadding(10.f, 20.f, 30.f, 40.f);
	xOriginal.SetSpacing(15.f);
	xOriginal.SetChildForceExpandWidth(true);
	xOriginal.SetChildForceExpandHeight(false);
	xOriginal.SetReverseArrangement(true);
	xOriginal.SetFitToContent(false);
	xOriginal.SetSize(400.f, 300.f);
	xOriginal.SetPosition(50.f, 60.f);
	xOriginal.SetColor({0.5f, 0.6f, 0.7f, 0.8f});

	// Write to DataStream
	Zenith_DataStream xWriteStream;
	xOriginal.WriteToDataStream(xWriteStream);

	// Read into a new layout group
	Zenith_UI::Zenith_UILayoutGroup xLoaded("LoadedLayout");
	xWriteStream.SetCursor(0);
	xLoaded.ReadFromDataStream(xWriteStream);

	// Verify all properties match
	ZENITH_ASSERT_EQ(xLoaded.GetDirection(), Zenith_UI::LayoutDirection::Vertical, "Serialized direction should be Vertical");
	ZENITH_ASSERT_EQ(xLoaded.GetChildAlignment(), Zenith_UI::ChildAlignment::LowerRight, "Serialized child alignment should be LowerRight");

	Zenith_Maths::Vector4 xPad = xLoaded.GetPadding();
	ZENITH_ASSERT_EQ_FLOAT(xPad.x, 10.f, 0.001f, "Serialized padding left should be 10");
	ZENITH_ASSERT_EQ_FLOAT(xPad.y, 20.f, 0.001f, "Serialized padding top should be 20");
	ZENITH_ASSERT_EQ_FLOAT(xPad.z, 30.f, 0.001f, "Serialized padding right should be 30");
	ZENITH_ASSERT_EQ_FLOAT(xPad.w, 40.f, 0.001f, "Serialized padding bottom should be 40");

	ZENITH_ASSERT_EQ_FLOAT(xLoaded.GetSpacing(), 15.f, 0.001f, "Serialized spacing should be 15");
	ZENITH_ASSERT_EQ(xLoaded.GetChildForceExpandWidth(), true, "Serialized childForceExpandWidth should be true");
	ZENITH_ASSERT_EQ(xLoaded.GetChildForceExpandHeight(), false, "Serialized childForceExpandHeight should be false");
	ZENITH_ASSERT_EQ(xLoaded.GetReverseArrangement(), true, "Serialized reverseArrangement should be true");
	ZENITH_ASSERT_EQ(xLoaded.GetFitToContent(), false, "Serialized fitToContent should be false");

	// Also verify base class properties survived
	Zenith_Maths::Vector2 xSize = xLoaded.GetSize();
	ZENITH_ASSERT_EQ_FLOAT(xSize.x, 400.f, 0.001f, "Serialized size.x should be 400");
	ZENITH_ASSERT_EQ_FLOAT(xSize.y, 300.f, 0.001f, "Serialized size.y should be 300");

	Zenith_Maths::Vector2 xPos = xLoaded.GetPosition();
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 50.f, 0.001f, "Serialized position.x should be 50");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 60.f, 0.001f, "Serialized position.y should be 60");

	Zenith_Maths::Vector4 xColor = xLoaded.GetColor();
	ZENITH_ASSERT_EQ_FLOAT(xColor.x, 0.5f, 0.001f, "Serialized color.r should be 0.5");
	ZENITH_ASSERT_EQ_FLOAT(xColor.y, 0.6f, 0.001f, "Serialized color.g should be 0.6");
	ZENITH_ASSERT_EQ_FLOAT(xColor.z, 0.7f, 0.001f, "Serialized color.b should be 0.7");
	ZENITH_ASSERT_EQ_FLOAT(xColor.w, 0.8f, 0.001f, "Serialized color.a should be 0.8");

	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestLayoutSerializationRoundTrip);
}

//=============================================================================
// UIStyle Automation Tests
//=============================================================================
ZENITH_TEST(Automation, SetUICornerRadiusStep)
{
	EDITOR_TEST_BEGIN(TestSetUICornerRadiusStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoCornerRadiusEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIRect("TestRect");
	Zenith_EditorAutomation::AddStep_SetUICornerRadius("TestRect", 12.0f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	auto* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>("TestRect");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_fCornerRadius, 12.0f, 0.001f, "Corner radius should be 12");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUICornerRadiusStep);
}
ZENITH_TEST(Automation, SetUIGradientColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIGradientColorStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoGradientEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIRect("TestRect");
	Zenith_EditorAutomation::AddStep_SetUIGradientColor("TestRect", 0.5f, 0.3f, 0.1f, 1.0f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxRect = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIRect>("TestRect");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xGradientBottomColor.x, 0.5f, 0.001f, "Gradient R should be 0.5");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xGradientBottomColor.y, 0.3f, 0.001f, "Gradient G should be 0.3");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIGradientColorStep);
}
ZENITH_TEST(Automation, SetUIShadowStep)
{
	EDITOR_TEST_BEGIN(TestSetUIShadowStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoShadowEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIRect("TestRect");
	Zenith_EditorAutomation::AddStep_SetUIShadow("TestRect", 3.0f, 4.0f, 2.0f, true);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxRect = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIRect>("TestRect");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect");
	ZENITH_ASSERT_EQ(pxRect->GetStyle().m_bShadowEnabled, true, "Shadow should be enabled");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xShadowOffset.x, 3.0f, 0.001f, "Shadow offset X should be 3");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xShadowOffset.y, 4.0f, 0.001f, "Shadow offset Y should be 4");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_fShadowSpread, 2.0f, 0.001f, "Shadow spread should be 2");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIShadowStep);
}
ZENITH_TEST(Automation, SetUIShadowColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIShadowColorStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoShadowColorEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIRect("TestRect");
	Zenith_EditorAutomation::AddStep_SetUIShadowColor("TestRect", 0.1f, 0.2f, 0.3f, 0.5f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxRect = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIRect>("TestRect");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xShadowColor.x, 0.1f, 0.001f, "Shadow color R");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xShadowColor.w, 0.5f, 0.001f, "Shadow color A");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIShadowColorStep);
}
ZENITH_TEST(Automation, SetUIRectBorderStep)
{
	EDITOR_TEST_BEGIN(TestSetUIRectBorderStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoRectBorderEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIRect("TestRect");
	Zenith_EditorAutomation::AddStep_SetUIRectBorder("TestRect", 0.5f, 0.6f, 0.7f, 3.0f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxRect = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIRect>("TestRect");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xBorderColor.x, 0.5f, 0.001f, "Border color R");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_fBorderThickness, 3.0f, 0.001f, "Border thickness should be 3");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIRectBorderStep);
}
ZENITH_TEST(Automation, SetUITextShadowStep)
{
	EDITOR_TEST_BEGIN(TestSetUITextShadowStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoTextShadowEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIText("TestText", "Hello");
	Zenith_EditorAutomation::AddStep_SetUITextShadow("TestText", 2.0f, 3.0f, true);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxText = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIText>("TestText");
	ZENITH_ASSERT_NOT_NULL(pxText, "Should find UI text");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUITextShadowStep);
}
ZENITH_TEST(Automation, SetUITextShadowColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUITextShadowColorStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoTextShadowColorEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIText("TestText", "Hello");
	Zenith_EditorAutomation::AddStep_SetUITextShadowColor("TestText", 0.1f, 0.2f, 0.3f, 0.8f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxText = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIText>("TestText");
	ZENITH_ASSERT_NOT_NULL(pxText, "Should find UI text");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUITextShadowColorStep);
}
ZENITH_TEST(Automation, SetUIButtonCornerRadiusStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonCornerRadiusStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoBtnCornerEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("TestBtn", "Click");
	Zenith_EditorAutomation::AddStep_SetUIButtonCornerRadius("TestBtn", 16.0f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxBtn = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_fCornerRadius, 16.0f, 0.001f, "Normal corner radius should be 16");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetHoveredStyle().m_fCornerRadius, 16.0f, 0.001f, "Hovered corner radius should be 16");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetPressedStyle().m_fCornerRadius, 16.0f, 0.001f, "Pressed corner radius should be 16");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIButtonCornerRadiusStep);
}
ZENITH_TEST(Automation, SetUIButtonShadowStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonShadowStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoBtnShadowEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("TestBtn", "Click");
	Zenith_EditorAutomation::AddStep_SetUIButtonShadow("TestBtn", 3.0f, 3.0f, 2.0f, true);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxBtn = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ(pxBtn->GetNormalStyle().m_bShadowEnabled, true, "Shadow should be enabled on all states");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_xShadowOffset.x, 3.0f, 0.001f, "Shadow offset X");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_fShadowSpread, 2.0f, 0.001f, "Shadow spread");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIButtonShadowStep);
}
ZENITH_TEST(Automation, SetUIButtonShadowColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonShadowColorStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoBtnShadowColEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("TestBtn", "Click");
	Zenith_EditorAutomation::AddStep_SetUIButtonShadowColor("TestBtn", 0.0f, 0.0f, 0.0f, 0.3f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxBtn = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_xShadowColor.w, 0.3f, 0.001f, "Shadow color A should be 0.3");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIButtonShadowColorStep);
}
ZENITH_TEST(Automation, SetUIButtonGradientColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonGradientColorStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoBtnGradEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("TestBtn", "Click");
	Zenith_EditorAutomation::AddStep_SetUIButtonGradientColor("TestBtn", 0.2f, 0.4f, 0.6f, 1.0f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxBtn = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_xGradientBottomColor.x, 0.2f, 0.001f, "Gradient R");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_xGradientBottomColor.y, 0.4f, 0.001f, "Gradient G");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIButtonGradientColorStep);
}
ZENITH_TEST(Automation, SetUIButtonBorderColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonBorderColorStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoBtnBorderColEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("TestBtn", "Click");
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderColor("TestBtn", 0.3f, 0.5f, 0.7f, 1.0f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxBtn = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_xBorderColor.x, 0.3f, 0.001f, "Border color R");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetHoveredStyle().m_xBorderColor.y, 0.5f, 0.001f, "Hover border color G");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetPressedStyle().m_xBorderColor.z, 0.7f, 0.001f, "Pressed border color B");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIButtonBorderColorStep);
}
ZENITH_TEST(Automation, SetUIButtonBorderThicknessStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonBorderThicknessStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoBtnBorderThickEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("TestBtn", "Click");
	Zenith_EditorAutomation::AddStep_SetUIButtonBorderThickness("TestBtn", 3.0f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxBtn = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_fBorderThickness, 3.0f, 0.001f, "Border thickness should be 3");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIButtonBorderThicknessStep);
}
ZENITH_TEST(Automation, SetUIButtonTransitionDurationStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonTransitionDurationStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoBtnTransEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("TestBtn", "Click");
	Zenith_EditorAutomation::AddStep_SetUIButtonTransitionDuration("TestBtn", 0.25f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxBtn = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIButtonTransitionDurationStep);
}
ZENITH_TEST(Automation, SetUIButtonTextShadowStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonTextShadowStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoBtnTextShadEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("TestBtn", "Click");
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadow("TestBtn", 1.0f, 2.0f, true);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxBtn = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIButtonTextShadowStep);
}
ZENITH_TEST(Automation, SetUIButtonTextShadowColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonTextShadowColorStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("AutoBtnTextShadColEntity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUIButton("TestBtn", "Click");
	Zenith_EditorAutomation::AddStep_SetUIButtonTextShadowColor("TestBtn", 0.0f, 0.0f, 0.0f, 0.4f);
	Zenith_EditorAutomation::Begin();

	for (uint32_t i = 0; i < 4; i++)
		Zenith_EditorAutomation::ExecuteNextStep();

	auto* pxBtn = Zenith_Editor::GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");

	Zenith_EditorAutomation::ExecuteNextStep();
	Zenith_EditorAutomation::Reset();

	EDITOR_TEST_END(TestSetUIButtonTextShadowColorStep);
}

//=============================================================================
// Group Alpha Tests
//=============================================================================
ZENITH_TEST(Automation, GroupAlphaDefault)
{
	EDITOR_TEST_BEGIN(TestGroupAlphaDefault);

	Zenith_UI::Zenith_UIRect xRect("TestRect");
	ZENITH_ASSERT_EQ_FLOAT(xRect.GetGroupAlpha(), 1.0f, 0.001f, "Default group alpha should be 1.0");
	ZENITH_ASSERT_EQ_FLOAT(xRect.GetEffectiveAlpha(), 1.0f, 0.001f, "Default effective alpha should be 1.0");

	EDITOR_TEST_END(TestGroupAlphaDefault);
}
ZENITH_TEST(Automation, GroupAlphaPropagation)
{
	EDITOR_TEST_BEGIN(TestGroupAlphaPropagation);

	Zenith_UI::Zenith_UIRect xParent("Parent");
	Zenith_UI::Zenith_UIRect xChild("Child");
	Zenith_UI::Zenith_UIRect xGrandchild("Grandchild");

	xParent.AddChild(&xChild);
	xChild.AddChild(&xGrandchild);

	// Default: all alpha 1.0
	ZENITH_ASSERT_EQ_FLOAT(xGrandchild.GetEffectiveAlpha(), 1.0f, 0.001f, "Default effective alpha should be 1.0");

	// Set parent alpha to 0.5
	xParent.SetGroupAlpha(0.5f);
	ZENITH_ASSERT_EQ_FLOAT(xChild.GetEffectiveAlpha(), 0.5f, 0.001f, "Child inherits parent alpha 0.5");
	ZENITH_ASSERT_EQ_FLOAT(xGrandchild.GetEffectiveAlpha(), 0.5f, 0.001f, "Grandchild inherits parent alpha 0.5");

	// Set child alpha to 0.5 (effective = 0.5 * 0.5 = 0.25)
	xChild.SetGroupAlpha(0.5f);
	ZENITH_ASSERT_EQ_FLOAT(xGrandchild.GetEffectiveAlpha(), 0.25f, 0.001f, "Grandchild: 0.5 * 0.5 = 0.25");

	// Cleanup hierarchy
	xChild.RemoveChild(&xGrandchild);
	xParent.RemoveChild(&xChild);

	EDITOR_TEST_END(TestGroupAlphaPropagation);
}
ZENITH_TEST(Automation, GroupInteractableDefault)
{
	EDITOR_TEST_BEGIN(TestGroupInteractableDefault);

	Zenith_UI::Zenith_UIRect xRect("TestRect");
	ZENITH_ASSERT_EQ(xRect.IsGroupInteractable(), true, "Default interactable should be true");

	xRect.SetGroupInteractable(false);
	ZENITH_ASSERT_EQ(xRect.IsGroupInteractable(), false, "Interactable should be false after setting");

	EDITOR_TEST_END(TestGroupInteractableDefault);
}
ZENITH_TEST(Automation, GroupInteractableParentDisabled)
{
	EDITOR_TEST_BEGIN(TestGroupInteractableParentDisabled);

	Zenith_UI::Zenith_UIRect xParent("Parent");
	Zenith_UI::Zenith_UIRect xChild("Child");

	xParent.AddChild(&xChild);

	ZENITH_ASSERT_EQ(xChild.IsGroupInteractable(), true, "Child default interactable should be true");

	xParent.SetGroupInteractable(false);
	ZENITH_ASSERT_EQ(xChild.IsGroupInteractable(), false, "Child should not be interactable when parent is disabled");

	// Child's own flag is still true, just parent overrides
	xParent.SetGroupInteractable(true);
	ZENITH_ASSERT_EQ(xChild.IsGroupInteractable(), true, "Child should be interactable when parent re-enabled");

	xParent.RemoveChild(&xChild);

	EDITOR_TEST_END(TestGroupInteractableParentDisabled);
}

//=============================================================================
// UIElement Background Tests
//=============================================================================
ZENITH_TEST(Automation, SetUIBackgroundColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIBackgroundColorStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("Entity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("TestGroup");
	Zenith_EditorAutomation::AddStep_SetUIBackgroundColor("TestGroup", 0.1f, 0.2f, 0.3f, 0.5f);
	Zenith_EditorAutomation::Begin();
	while (!Zenith_EditorAutomation::IsComplete())
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	auto* pxGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("TestGroup");
	ZENITH_ASSERT_TRUE(pxGroup->HasBackground(), "Background must be enabled");
	ZENITH_ASSERT_EQ_FLOAT(pxGroup->GetBackgroundStyle().m_xFillColor.x, 0.1f, 0.001f, "Bg color R");
	ZENITH_ASSERT_EQ_FLOAT(pxGroup->GetBackgroundStyle().m_xFillColor.w, 0.5f, 0.001f, "Bg color A");

	Zenith_EditorAutomation::Reset();
	EDITOR_TEST_END(TestSetUIBackgroundColorStep);
}
ZENITH_TEST(Automation, SetUIBackgroundCornerRadiusStep)
{
	EDITOR_TEST_BEGIN(TestSetUIBackgroundCornerRadiusStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("Entity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("TestGroup");
	Zenith_EditorAutomation::AddStep_SetUIBackgroundColor("TestGroup", 0.5f, 0.5f, 0.5f, 1.0f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundCornerRadius("TestGroup", 16.0f);
	Zenith_EditorAutomation::Begin();
	while (!Zenith_EditorAutomation::IsComplete())
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	auto* pxGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("TestGroup");
	ZENITH_ASSERT_EQ_FLOAT(pxGroup->GetBackgroundStyle().m_fCornerRadius, 16.0f, 0.001f, "Bg corner radius must be 16");

	Zenith_EditorAutomation::Reset();
	EDITOR_TEST_END(TestSetUIBackgroundCornerRadiusStep);
}
ZENITH_TEST(Automation, SetUIBackgroundBorderStep)
{
	EDITOR_TEST_BEGIN(TestSetUIBackgroundBorderStep);

	Zenith_EditorAutomation::Reset();
	Zenith_EditorAutomation::AddStep_CreateEntity("Entity");
	Zenith_EditorAutomation::AddStep_AddUI();
	Zenith_EditorAutomation::AddStep_CreateUILayoutGroup("TestGroup");
	Zenith_EditorAutomation::AddStep_SetUIBackgroundColor("TestGroup", 0.5f, 0.5f, 0.5f, 1.0f);
	Zenith_EditorAutomation::AddStep_SetUIBackgroundBorder("TestGroup", 0.3f, 0.4f, 0.5f, 2.0f);
	Zenith_EditorAutomation::Begin();
	while (!Zenith_EditorAutomation::IsComplete())
		Zenith_EditorAutomation::ExecuteNextStep();

	Zenith_Entity* pxEntity = Zenith_Editor::GetSelectedEntity();
	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	auto* pxGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("TestGroup");
	ZENITH_ASSERT_EQ_FLOAT(pxGroup->GetBackgroundStyle().m_xBorderColor.x, 0.3f, 0.001f, "Bg border R");
	ZENITH_ASSERT_EQ_FLOAT(pxGroup->GetBackgroundStyle().m_fBorderThickness, 2.0f, 0.001f, "Bg border thickness");

	Zenith_EditorAutomation::Reset();
	EDITOR_TEST_END(TestSetUIBackgroundBorderStep);
}

//=============================================================================
// Button Transition Tests
//=============================================================================
ZENITH_TEST(Automation, ButtonTransitionInitialState)
{
	EDITOR_TEST_BEGIN(TestButtonTransitionInitialState);

	Zenith_UI::Zenith_UIButton xButton("Test", "TestBtn");

	// Initial state should be NORMAL
	ZENITH_ASSERT_EQ(xButton.GetState(), Zenith_UI::Zenith_UIButton::ButtonState::NORMAL, "Initial state should be NORMAL");

	// Normal style fill should match what was set in constructor
	ZENITH_ASSERT_EQ_FLOAT(xButton.GetNormalStyle().m_xFillColor.x, 0.25f, 0.001f, "Normal fill R should be 0.25");
	ZENITH_ASSERT_EQ_FLOAT(xButton.GetHoveredStyle().m_xFillColor.x, 0.35f, 0.001f, "Hovered fill R should be 0.35");
	ZENITH_ASSERT_EQ_FLOAT(xButton.GetPressedStyle().m_xFillColor.x, 0.15f, 0.001f, "Pressed fill R should be 0.15");

	EDITOR_TEST_END(TestButtonTransitionInitialState);
}

#endif // ZENITH_TOOLS
