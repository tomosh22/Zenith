#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS
#include "UnitTests/Zenith_EditorTestFixture.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Editor/Panels/Zenith_EditorPanel_GraphEditor.h"
// WU-3.4's animation steps: the panel they drive, the clip type their fixture
// writes, and the preview-slot arbiter opening a clip claims (reset at both
// ends of the unit, exactly as Zenith_EditorPanel_Animation.Tests.inl does —
// a unit that left the process-level slot claimed would hand its claim on).
#include "Editor/Panels/Zenith_EditorPanel_Animation.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "Flux/RenderViews/Flux_PreviewSlotArbiter.h"
// WU-4.3's pose steps additionally need a resolvable RIG: the pose verbs address
// a BONE, so a clip with no skeleton has nothing for them to select.
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "Scripting/Zenith_BehaviourGraph.h"
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
#include "Prefab/Zenith_Prefab.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Maths/Zenith_Maths.h"
#include <cmath>
#include <filesystem>

static void NoOp() {}

//=============================================================================
// State Machine Tests
//=============================================================================
ZENITH_TEST(Automation, InitialState)
{

	g_xEngine.EditorAutomation().Reset();

	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsRunning(), "Should not be running after reset");
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsComplete(), "Should not be complete after reset");

}
ZENITH_TEST(Automation, BeginSetsRunning)
{

	g_xEngine.EditorAutomation().Reset();

	// Add a dummy step so Begin has something to work with
	g_xEngine.EditorAutomation().AddStep_Custom(&NoOp);
	g_xEngine.EditorAutomation().Begin();

	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsRunning(), "Should be running after Begin");
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsComplete(), "Should not be complete right after Begin");

	g_xEngine.EditorAutomation().Reset();

}
ZENITH_TEST(Automation, ResetClearsState)
{

	// Add steps and begin
	g_xEngine.EditorAutomation().AddStep_Custom(&NoOp);
	g_xEngine.EditorAutomation().AddStep_Custom(&NoOp);
	g_xEngine.EditorAutomation().Begin();

	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsRunning(), "Should be running");

	// Reset should clear everything
	g_xEngine.EditorAutomation().Reset();

	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsRunning(), "Should not be running after Reset");
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsComplete(), "Should not be complete after Reset");

}

//=============================================================================
// Step Execution Tests
//=============================================================================

static uint32_t s_uCustomStepCounter = 0;
static void IncrementCounter() { s_uCustomStepCounter++; }
ZENITH_TEST(Automation, StepExecutionOrder)
{

	g_xEngine.EditorAutomation().Reset();
	s_uCustomStepCounter = 0;

	// Add 3 custom steps
	g_xEngine.EditorAutomation().AddStep_Custom(&IncrementCounter);
	g_xEngine.EditorAutomation().AddStep_Custom(&IncrementCounter);
	g_xEngine.EditorAutomation().AddStep_Custom(&IncrementCounter);

	g_xEngine.EditorAutomation().Begin();

	// Execute steps one at a time
	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 1, "Counter should be 1 after first step");
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsRunning(), "Should still be running after first step");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 2, "Counter should be 2 after second step");
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsRunning(), "Should still be running after second step");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 3, "Counter should be 3 after third step");
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsRunning(), "Should not be running after all steps");
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "Should be complete after all steps");

	g_xEngine.EditorAutomation().Reset();

}
ZENITH_TEST(Automation, ExecuteEmptyQueue)
{

	g_xEngine.EditorAutomation().Reset();

	// Calling ExecuteNextStep when not running should be a no-op
	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsRunning(), "Should not be running");
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsComplete(), "Should not be complete");

	g_xEngine.EditorAutomation().Reset();

}
ZENITH_TEST(Automation, CompletionAfterAllSteps)
{

	g_xEngine.EditorAutomation().Reset();
	s_uCustomStepCounter = 0;

	g_xEngine.EditorAutomation().AddStep_Custom(&IncrementCounter);
	g_xEngine.EditorAutomation().Begin();

	// Execute the single step - completion detected immediately
	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 1, "Counter should be 1");
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "Should be complete");
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsRunning(), "Should not be running");

	// Additional calls after completion should be no-ops
	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "Should still be complete");

	g_xEngine.EditorAutomation().Reset();

}

//=============================================================================
// Entity Operation Tests
//=============================================================================
ZENITH_TEST(Automation, CreateEntityStep)
{
	EDITOR_TEST_BEGIN(TestCreateEntityStep);

	g_xEngine.EditorAutomation().Reset();

	// Queue a create entity step
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoTestEntity");
	g_xEngine.EditorAutomation().Begin();

	// Execute the step
	g_xEngine.EditorAutomation().ExecuteNextStep();

	// Verify entity was created and selected
	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have a selected entity after CreateEntity step");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "AutoTestEntity", "Created entity should be named 'AutoTestEntity'");
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "Should be complete after single step");

	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestCreateEntityStep);
}
ZENITH_TEST(Automation, EntitySelectionTracking)
{
	EDITOR_TEST_BEGIN(TestEntitySelectionTracking);

	g_xEngine.EditorAutomation().Reset();

	// Queue: create A, create B, select A
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoEntityA");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoEntityB");
	g_xEngine.EditorAutomation().AddStep_SelectEntity("AutoEntityA");
	g_xEngine.EditorAutomation().Begin();

	// Step 1: Create A
	g_xEngine.EditorAutomation().ExecuteNextStep();
	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selection after creating A");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "AutoEntityA", "Selection should be A after creating A");

	// Step 2: Create B (auto-selects B)
	g_xEngine.EditorAutomation().ExecuteNextStep();
	pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selection after creating B");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "AutoEntityB", "Selection should be B after creating B");

	// Step 3: Select A again
	g_xEngine.EditorAutomation().ExecuteNextStep();
	pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selection after selecting A");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "AutoEntityA", "Selection should be A after SelectEntity step");
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "Should be complete after last step");

	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestEntitySelectionTracking);
}

//=============================================================================
// Component Operation Tests
//=============================================================================
ZENITH_TEST(Automation, AddComponentStep)
{
	EDITOR_TEST_BEGIN(TestAddComponentStep);

	g_xEngine.EditorAutomation().Reset();

	// Queue: create entity, add camera component
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoCamEntity");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().Begin();

	// Execute both steps
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create entity
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add camera

	// Verify camera was added
	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_CameraComponent>(), "Entity should have CameraComponent after AddCamera step");

	// Advance to completion
	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestAddComponentStep);
}

//=============================================================================
// Transform Operation Tests
//=============================================================================
ZENITH_TEST(Automation, SetTransformPositionStep)
{
	EDITOR_TEST_BEGIN(TestSetTransformPositionStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoPosEntity");
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(10.f, 20.f, 30.f);
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Set position

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_TransformComponent& xTransform = pxEntity->GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPos;
	xTransform.GetPosition(xPos);

	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 10.f, 0.001f, "X position should be 10");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 20.f, 0.001f, "Y position should be 20");
	ZENITH_ASSERT_EQ_FLOAT(xPos.z, 30.f, 0.001f, "Z position should be 30");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetTransformPositionStep);
}
ZENITH_TEST(Automation, SetTransformScaleStep)
{
	EDITOR_TEST_BEGIN(TestSetTransformScaleStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoScaleEntity");
	g_xEngine.EditorAutomation().AddStep_SetTransformScale(2.f, 3.f, 4.f);
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Set scale

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_TransformComponent& xTransform = pxEntity->GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xScale;
	xTransform.GetScale(xScale);

	ZENITH_ASSERT_EQ_FLOAT(xScale.x, 2.f, 0.001f, "X scale should be 2");
	ZENITH_ASSERT_EQ_FLOAT(xScale.y, 3.f, 0.001f, "Y scale should be 3");
	ZENITH_ASSERT_EQ_FLOAT(xScale.z, 4.f, 0.001f, "Z scale should be 4");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetTransformScaleStep);
}

//=============================================================================
// Collider Operation Tests
//=============================================================================
ZENITH_TEST(Automation, AddCapsuleColliderStep)
{
	EDITOR_TEST_BEGIN(TestAddCapsuleColliderStep);

	g_xEngine.EditorAutomation().Reset();

	// Create an entity, add a ColliderComponent, then attach an EXPLICIT capsule.
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoCapsuleEntity");
	g_xEngine.EditorAutomation().AddStep_AddCollider();
	g_xEngine.EditorAutomation().AddStep_AddCapsuleCollider(0.4f, 0.4f, RIGIDBODY_TYPE_STATIC);
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create entity
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add ColliderComponent
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add explicit capsule shape

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_ColliderComponent>(),
		"Entity should have ColliderComponent");
	ZENITH_ASSERT_TRUE(
		pxEntity->GetComponent<Zenith_ColliderComponent>().GetCollisionVolumeType() == COLLISION_VOLUME_TYPE_CAPSULE,
		"Volume type should be CAPSULE after AddCapsuleCollider step");
	ZENITH_ASSERT_TRUE(
		pxEntity->GetComponent<Zenith_ColliderComponent>().GetRigidBodyType() == RIGIDBODY_TYPE_STATIC,
		"Body type should be STATIC");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestAddCapsuleColliderStep);
}

//=============================================================================
// Camera Operation Tests
//=============================================================================
ZENITH_TEST(Automation, SetCameraFOVStep)
{
	EDITOR_TEST_BEGIN(TestSetCameraFOVStep);

	g_xEngine.EditorAutomation().Reset();

	float fTargetFOV = 1.2f;
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoFOVEntity");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(fTargetFOV);
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add camera
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Set FOV

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	float fActual = pxEntity->GetComponent<Zenith_CameraComponent>().GetFOV();
	ZENITH_ASSERT_EQ_FLOAT(fActual, fTargetFOV, 0.001f, "FOV should match target");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetCameraFOVStep);
}
ZENITH_TEST(Automation, SetCameraPitchYawStep)
{
	EDITOR_TEST_BEGIN(TestSetCameraPitchYawStep);

	g_xEngine.EditorAutomation().Reset();

	float fTargetPitch = -0.5f;
	float fTargetYaw = 2.0f;
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoPYEntity");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPitch(fTargetPitch);
	g_xEngine.EditorAutomation().AddStep_SetCameraYaw(fTargetYaw);
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add camera
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Set pitch
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Set yaw

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	Zenith_CameraComponent& xCam = pxEntity->GetComponent<Zenith_CameraComponent>();
	ZENITH_ASSERT_EQ_FLOAT(static_cast<float>(xCam.GetPitch()), fTargetPitch, 0.001f, "Pitch should match target");
	ZENITH_ASSERT_EQ_FLOAT(static_cast<float>(xCam.GetYaw()), fTargetYaw, 0.001f, "Yaw should match target");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetCameraPitchYawStep);
}
ZENITH_TEST(Automation, SetCameraPositionStep)
{
	EDITOR_TEST_BEGIN(TestSetCameraPositionStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoCamPosEntity");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(5.f, 10.f, 15.f);
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add camera
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Set position

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	Zenith_Maths::Vector3 xPos;
	pxEntity->GetComponent<Zenith_CameraComponent>().GetPosition(xPos);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 5.f, 0.001f, "Camera X position should be 5");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 10.f, 0.001f, "Camera Y position should be 10");
	ZENITH_ASSERT_EQ_FLOAT(xPos.z, 15.f, 0.001f, "Camera Z position should be 15");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetCameraPositionStep);
}
ZENITH_TEST(Automation, SetAsMainCameraStep)
{
	EDITOR_TEST_BEGIN(TestSetAsMainCameraStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoMainCamEntity");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetAsMainCamera();
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add camera
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Set as main camera

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(pxEntity->GetEntityID());
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "Entity should be in a scene");
	ZENITH_ASSERT_EQ(pxSceneData->GetMainCameraEntity(), pxEntity->GetEntityID(), "Entity should be the main camera");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetAsMainCameraStep);
}

//=============================================================================
// Negative Path Tests
//=============================================================================
ZENITH_TEST(Automation, AddInvalidComponentStep)
{
	EDITOR_TEST_BEGIN(TestAddInvalidComponentStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoInvalidCompEntity");
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create entity

	// Try to add a component with an invalid name directly through the editor API
	bool bResult = g_xEngine.Editor().AddComponentToSelected("NonExistentComponent_XYZ");
	ZENITH_ASSERT_FALSE(bResult, "Adding invalid component should return false");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestAddInvalidComponentStep);
}

//=============================================================================
// Custom Step Tests
//=============================================================================

static bool s_bCustomStepExecuted = false;
static void SetCustomFlag() { s_bCustomStepExecuted = true; }
ZENITH_TEST(Automation, CustomStepExecution)
{

	g_xEngine.EditorAutomation().Reset();
	s_bCustomStepExecuted = false;

	g_xEngine.EditorAutomation().AddStep_Custom(&SetCustomFlag);
	g_xEngine.EditorAutomation().Begin();

	ZENITH_ASSERT_FALSE(s_bCustomStepExecuted, "Custom step should not execute before ExecuteNextStep");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_TRUE(s_bCustomStepExecuted, "Custom step function should have been called");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "Should be complete");

	g_xEngine.EditorAutomation().Reset();

}

//=============================================================================
// Scene Lifecycle Tests
//=============================================================================
ZENITH_TEST(Automation, CreateSaveUnloadCycle)
{
	EDITOR_TEST_BEGIN(TestCreateSaveUnloadCycle);

	g_xEngine.EditorAutomation().Reset();

	const char* szSavePath = ENGINE_ASSETS_DIR "_AutoTest" ZENITH_SCENE_EXT;

	// Queue: create scene, create entity, add camera, save, unload
	g_xEngine.EditorAutomation().AddStep_CreateScene("AutoTestScene");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoSceneEntity");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SaveScene(szSavePath);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();
	g_xEngine.EditorAutomation().Begin();

	// Step 1: Create scene
	g_xEngine.EditorAutomation().ExecuteNextStep();
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	ZENITH_ASSERT_TRUE(xScene.IsValid(), "Active scene should be valid after CreateScene step");

	// Step 2: Create entity
	g_xEngine.EditorAutomation().ExecuteNextStep();
	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have created entity in new scene");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "AutoSceneEntity", "Entity name should match");

	// Step 3: Add camera
	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_CameraComponent>(), "Entity should have camera component");

	// Step 4: Save scene
	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_TRUE(std::filesystem::exists(szSavePath), "Scene file should exist after save");

	// Step 5: Unload scene
	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "Should be complete after last step");

	// Clean up the temp scene file
	std::filesystem::remove(szSavePath);

	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestCreateSaveUnloadCycle);
}

//=============================================================================
// UI Operation Tests
//=============================================================================
ZENITH_TEST(Automation, CreateUITextStep)
{
	EDITOR_TEST_BEGIN(TestCreateUITextStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoUITextEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Label1", "Hello");
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create entity
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add UI
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create text

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_UIComponent>(), "Entity should have UIComponent");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>("Label1");
	ZENITH_ASSERT_NOT_NULL(pxText, "Should find UI text element 'Label1'");
	ZENITH_ASSERT_EQ(pxText->GetText(), "Hello", "Text content should be 'Hello'");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestCreateUITextStep);
}
ZENITH_TEST(Automation, CreateUIButtonStep)
{
	EDITOR_TEST_BEGIN(TestCreateUIButtonStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoUIBtnEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("Btn1", "Click Me");
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create entity
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add UI
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create button

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIButton* pxButton = xUI.FindElement<Zenith_UI::Zenith_UIButton>("Btn1");
	ZENITH_ASSERT_NOT_NULL(pxButton, "Should find UI button 'Btn1'");
	ZENITH_ASSERT_EQ(pxButton->GetText(), "Click Me", "Button text should be 'Click Me'");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestCreateUIButtonStep);
}
ZENITH_TEST(Automation, CreateUIRectStep)
{
	EDITOR_TEST_BEGIN(TestCreateUIRectStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoUIRectEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("Rect1");
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create entity
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add UI
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create rect

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIRect* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>("Rect1");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect 'Rect1'");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestCreateUIRectStep);
}
ZENITH_TEST(Automation, SetUIPropertiesStep)
{
	EDITOR_TEST_BEGIN(TestSetUIPropertiesStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoUIPropEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Txt", "Test");
	g_xEngine.EditorAutomation().AddStep_SetUIPosition("Txt", 100.f, 200.f);
	g_xEngine.EditorAutomation().AddStep_SetUISize("Txt", 300.f, 50.f);
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("Txt", 32.f);
	g_xEngine.EditorAutomation().AddStep_SetUIColor("Txt", 1.f, 0.f, 0.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIAnchor("Txt", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIAlignment("Txt", static_cast<int>(Zenith_UI::TextAlignment::Center));
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("Txt", false);
	g_xEngine.EditorAutomation().Begin();

	// Execute all 10 steps
	for (uint32_t i = 0; i < 10; i++)
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIPropertiesStep);
}
ZENITH_TEST(Automation, SetUIButtonStyleStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonStyleStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoUIBtnStyleEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("Btn", "Test");
	g_xEngine.EditorAutomation().AddStep_SetUIButtonNormalColor("Btn", 1.f, 0.f, 0.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonHoverColor("Btn", 0.f, 1.f, 0.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonPressedColor("Btn", 0.f, 0.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_SetUIButtonFontSize("Btn", 18.f);
	g_xEngine.EditorAutomation().Begin();

	// Execute all 7 steps
	for (uint32_t i = 0; i < 7; i++)
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIButtonStyleStep);
}

//=============================================================================
// Behaviour Graph attach test
//=============================================================================

ZENITH_TEST(Automation, AttachGraphStep)
{
	EDITOR_TEST_BEGIN(TestAttachGraphStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoSerEntity");
	g_xEngine.EditorAutomation().AddStep_AttachGraph("game:Graphs/AutomationTest_DoesNotExist.bgraph");
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create entity
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Attach graph (adds GraphComponent + slot)

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_GraphComponent>(), "Entity should have GraphComponent");

	Zenith_GraphComponent& xGraphs = pxEntity->GetComponent<Zenith_GraphComponent>();
	ZENITH_ASSERT_EQ(xGraphs.GetGraphCount(), 1u, "Should have exactly one graph slot");

	// The asset doesn't exist - the slot is preserved unresolved with its path
	// intact (the round-trip contract).
	ZENITH_ASSERT_NULL(xGraphs.GetGraphAt(0), "Missing asset keeps the slot unresolved");
	ZENITH_ASSERT_STREQ(xGraphs.GetGraphAssetPathAt(0), "game:Graphs/AutomationTest_DoesNotExist.bgraph",
		"Slot path should round-trip verbatim");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestAttachGraphStep);
}

//=============================================================================
// Graph authoring steps - the atomic graph-editor actions driven through the
// automation queue (the boot-authoring path games use). Proves the
// step-authored asset round-trips with exactly the authored contents.
//=============================================================================
ZENITH_TEST(Automation, GraphAuthoringSteps)
{
	EDITOR_TEST_BEGIN(TestGraphAuthoringSteps);

	constexpr const char* szPATH = "game:Graphs/AutomationTest_Authoring.bgraph";
	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_GraphOpenFresh(szPATH);
	g_xEngine.EditorAutomation().AddStep_GraphAddNode("OnUpdate");
	g_xEngine.EditorAutomation().AddStep_GraphAddNode("RotateEntity");
	g_xEngine.EditorAutomation().AddStep_GraphSelectNode("RotateEntity", 0);
	g_xEngine.EditorAutomation().AddStep_GraphSetNodeParamFloat("m_fDegreesPerSecond", 123.0f);
	g_xEngine.EditorAutomation().AddStep_GraphConnect("OnUpdate", 0, 0, "RotateEntity", 0);
	g_xEngine.EditorAutomation().AddStep_GraphAddVariable("speed", "float", 2.0f);
	g_xEngine.EditorAutomation().AddStep_GraphSave();
	g_xEngine.EditorAutomation().AddStep_GraphClose();
	g_xEngine.EditorAutomation().Begin();
	for (u_int u = 0; u < 9u; ++u)
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	// The saved asset holds exactly what the steps authored.
	Zenith_BehaviourGraphAsset* pxAsset = Zenith_AssetRegistry::GetView<Zenith_BehaviourGraphAsset>(szPATH);
	ZENITH_ASSERT_NOT_NULL(pxAsset, "Step-authored asset should load from the registry");
	if (pxAsset)
	{
		const Zenith_GraphDefinition& xDef = pxAsset->GetDefinition();
		ZENITH_ASSERT_EQ(xDef.GetNodeCount(), 2u, "Two nodes authored");
		ZENITH_ASSERT_EQ(xDef.GetEdgeCount(), 1u, "One edge authored");
		ZENITH_ASSERT_EQ(xDef.GetVariableCount(), 1u, "One variable declared");
		if (xDef.GetVariableCount() == 1u)
		{
			ZENITH_ASSERT_STREQ(xDef.GetVariableAt(0).m_strName.c_str(), "speed", "Variable name round-trips");
			ZENITH_ASSERT_EQ_FLOAT(xDef.GetVariableAt(0).m_xDefault.GetFloat(), 2.0f, 0.0001f, "Variable default round-trips");
		}

		// Re-open in the editor and read the param back through the same
		// select-then-inspect path a human uses.
		Zenith_GraphEditorPanel::OpenAsset(szPATH);
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::Action_SelectNode("RotateEntity", 0), "Re-select authored node");
		float fDegrees = 0.0f;
		ZENITH_ASSERT_TRUE(Zenith_GraphEditorPanel::GetSelectedNodeParamFloat("m_fDegreesPerSecond", fDegrees), "Param readable");
		ZENITH_ASSERT_EQ_FLOAT(fDegrees, 123.0f, 0.0001f, "Param value round-trips");
		Zenith_GraphEditorPanel::Close();
	}

	std::error_code xEC;
	std::filesystem::remove(Zenith_AssetRegistry::ResolvePath(szPATH), xEC);
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestGraphAuthoringSteps);
}

//=============================================================================
// Camera Extended Tests
//=============================================================================
ZENITH_TEST(Automation, SetCameraNearFarAspectStep)
{
	EDITOR_TEST_BEGIN(TestSetCameraNearFarAspectStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoCamExtEntity");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraNear(0.5f);
	g_xEngine.EditorAutomation().AddStep_SetCameraFar(500.f);
	g_xEngine.EditorAutomation().AddStep_SetCameraAspect(1.5f);
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add camera
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Near
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Far
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Aspect

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_CameraComponent& xCam = pxEntity->GetComponent<Zenith_CameraComponent>();
	ZENITH_ASSERT_EQ_FLOAT(xCam.GetNearPlane(), 0.5f, 0.001f, "Near plane should be 0.5");
	ZENITH_ASSERT_EQ_FLOAT(xCam.GetFarPlane(), 500.f, 0.1f, "Far plane should be 500");
	ZENITH_ASSERT_EQ_FLOAT(xCam.GetAspectRatio(), 1.5f, 0.001f, "Aspect ratio should be 1.5");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetCameraNearFarAspectStep);
}

//=============================================================================
// Scene Round-Trip Tests
//=============================================================================
ZENITH_TEST(Automation, SceneSaveLoadRoundTrip)
{
	EDITOR_TEST_BEGIN(TestSceneSaveLoadRoundTrip);

	g_xEngine.EditorAutomation().Reset();

	const char* szSavePath = ENGINE_ASSETS_DIR "_AutoRoundTrip" ZENITH_SCENE_EXT;

	// Queue: create scene, entity, camera, set FOV and position, save, unload
	g_xEngine.EditorAutomation().AddStep_CreateScene("RoundTripScene");
	g_xEngine.EditorAutomation().AddStep_CreateEntity("RTEntity");
	g_xEngine.EditorAutomation().AddStep_AddCamera();
	g_xEngine.EditorAutomation().AddStep_SetCameraFOV(1.5f);
	g_xEngine.EditorAutomation().AddStep_SetCameraPosition(1.f, 2.f, 3.f);
	g_xEngine.EditorAutomation().AddStep_SaveScene(szSavePath);
	g_xEngine.EditorAutomation().AddStep_UnloadScene();
	g_xEngine.EditorAutomation().Begin();

	// Execute all 7 steps
	for (uint32_t i = 0; i < 7; i++)
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "Should be complete after last step");

	// Verify file exists
	ZENITH_ASSERT_TRUE(std::filesystem::exists(szSavePath), "Scene file should exist after save");

	// Load the saved scene and verify contents survived serialization
	Zenith_Scene xLoadedScene = g_xEngine.Scenes().LoadScene(szSavePath, SCENE_LOAD_ADDITIVE);
	ZENITH_ASSERT_TRUE(xLoadedScene.IsValid(), "Loaded scene should be valid");

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xLoadedScene);
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
	g_xEngine.Scenes().UnloadScene(xLoadedScene);
	std::filesystem::remove(szSavePath);

	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSceneSaveLoadRoundTrip);
}

//=============================================================================
// Edge Case Tests
//=============================================================================
ZENITH_TEST(Automation, ResetDuringExecution)
{

	g_xEngine.EditorAutomation().Reset();
	s_uCustomStepCounter = 0;

	// Queue 3 steps
	g_xEngine.EditorAutomation().AddStep_Custom(&IncrementCounter);
	g_xEngine.EditorAutomation().AddStep_Custom(&IncrementCounter);
	g_xEngine.EditorAutomation().AddStep_Custom(&IncrementCounter);
	g_xEngine.EditorAutomation().Begin();

	// Execute only 1 step
	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 1, "Counter should be 1 after first step");
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsRunning(), "Should still be running");

	// Reset mid-sequence
	g_xEngine.EditorAutomation().Reset();
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsRunning(), "Should not be running after mid-execution Reset");
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsComplete(), "Should not be complete after mid-execution Reset");

	// Counter should not advance further
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 1, "Counter should still be 1 after Reset");

}
ZENITH_TEST(Automation, BeginWithZeroSteps)
{

	g_xEngine.EditorAutomation().Reset();

	// Begin with no steps queued
	g_xEngine.EditorAutomation().Begin();
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsRunning(), "Should be running after Begin even with 0 steps");

	// First ExecuteNextStep should detect empty queue and complete immediately
	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsRunning(), "Should not be running after empty queue detected");
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "Should be complete after empty queue detected");

	g_xEngine.EditorAutomation().Reset();

}
ZENITH_TEST(Automation, DoubleBeginWithoutReset)
{

	g_xEngine.EditorAutomation().Reset();
	s_uCustomStepCounter = 0;

	// First sequence: add and run 1 step to completion
	g_xEngine.EditorAutomation().AddStep_Custom(&IncrementCounter);
	g_xEngine.EditorAutomation().Begin();
	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "First sequence should be complete");
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 1, "Counter should be 1 after first sequence");

	// Second Begin without Reset - queue was cleared on completion, so this starts fresh
	g_xEngine.EditorAutomation().AddStep_Custom(&IncrementCounter);
	g_xEngine.EditorAutomation().Begin();
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsRunning(), "Should be running after second Begin");
	ZENITH_ASSERT_FALSE(g_xEngine.EditorAutomation().IsComplete(), "Should not be complete after second Begin");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	ZENITH_ASSERT_EQ(s_uCustomStepCounter, 2, "Counter should be 2 after second sequence");
	ZENITH_ASSERT_TRUE(g_xEngine.EditorAutomation().IsComplete(), "Second sequence should be complete");

	g_xEngine.EditorAutomation().Reset();

}

//=============================================================================
// UI Image Operation Tests
//=============================================================================
ZENITH_TEST(Automation, CreateUIImageStep)
{
	EDITOR_TEST_BEGIN(TestCreateUIImageStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoUIImageEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIImage("Img1");
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create entity
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add UI
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create image

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIImage* pxImage = xUI.FindElement<Zenith_UI::Zenith_UIImage>("Img1");
	ZENITH_ASSERT_NOT_NULL(pxImage, "Should find UI image 'Img1'");
	ZENITH_ASSERT_EQ(pxImage->GetType(), Zenith_UI::UIElementType::Image, "Element type should be Image");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestCreateUIImageStep);
}
ZENITH_TEST(Automation, SetUIImageTexturePathStep)
{
	EDITOR_TEST_BEGIN(TestSetUIImageTexturePathStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoUIImgTexEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIImage("TexImg");
	g_xEngine.EditorAutomation().AddStep_SetUIImageTexturePath("TexImg",
		ENGINE_ASSETS_DIR "Textures/Font/FontAtlas.ztxtr");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIImage* pxImage = xUI.FindElement<Zenith_UI::Zenith_UIImage>("TexImg");
	ZENITH_ASSERT_NOT_NULL(pxImage, "Should find UI image 'TexImg'");
	ZENITH_ASSERT_EQ(pxImage->GetTexturePath(), "engine:Textures/Font/FontAtlas.ztxtr", "Texture path should be set");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

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

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoParticleEntity");
	g_xEngine.EditorAutomation().AddStep_AddParticleEmitter();
	g_xEngine.EditorAutomation().AddStep_SetParticleConfigByName("AutoTestConfig");
	g_xEngine.EditorAutomation().AddStep_SetParticleEmitting(false);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");
	ZENITH_ASSERT_TRUE(pxEntity->HasComponent<Zenith_ParticleEmitterComponent>(), "Entity should have ParticleEmitterComponent");

	Zenith_ParticleEmitterComponent& xEmitter =
		pxEntity->GetComponent<Zenith_ParticleEmitterComponent>();
	ZENITH_ASSERT_NOT_NULL(xEmitter.GetConfig(), "Particle emitter should have config assigned");
	ZENITH_ASSERT_EQ(xEmitter.GetConfig()->m_uBurstCount, 42, "Config burst count should be 42");
	ZENITH_ASSERT_FALSE(xEmitter.IsEmitting(), "Emitter should not be emitting");

	// Cleanup
	Flux_ParticleEmitterConfig::Unregister("AutoTestConfig");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetParticleConfigByNameStep);
}

//=============================================================================
// Layout Group Tests
//=============================================================================
ZENITH_TEST(Automation, CreateUILayoutGroupStep)
{
	EDITOR_TEST_BEGIN(TestCreateUILayoutGroupStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoLayoutEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("TestLayout");
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create entity
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Add UI
	g_xEngine.EditorAutomation().ExecuteNextStep(); // Create layout group

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestCreateUILayoutGroupStep);
}
ZENITH_TEST(Automation, AddUIChildStep)
{
	EDITOR_TEST_BEGIN(TestAddUIChildStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoChildEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("Parent");
	g_xEngine.EditorAutomation().AddStep_CreateUIText("Child1", "Hello");
	g_xEngine.EditorAutomation().AddStep_CreateUIImage("Child2");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("Parent", "Child1");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("Parent", "Child2");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 7; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("Parent");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group 'Parent'");
	ZENITH_ASSERT_EQ(pxLayout->GetChildCount(), 2, "Layout group should have 2 children");
	ZENITH_ASSERT_EQ(pxLayout->GetChild(0)->GetName(), "Child1", "First child should be 'Child1'");
	ZENITH_ASSERT_EQ(pxLayout->GetChild(1)->GetName(), "Child2", "Second child should be 'Child2'");
	ZENITH_ASSERT_EQ(pxLayout->GetChild(0)->GetParent(), pxLayout, "Child1 parent should be layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetChild(1)->GetParent(), pxLayout, "Child2 parent should be layout group");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestAddUIChildStep);
}
ZENITH_TEST(Automation, SetUILayoutDirectionStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutDirectionStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoDirEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("DirLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("DirLayout", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("DirLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetDirection(), Zenith_UI::LayoutDirection::Vertical, "Direction should be Vertical");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUILayoutDirectionStep);
}
ZENITH_TEST(Automation, SetUILayoutSpacingStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutSpacingStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoSpaceEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("SpaceLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("SpaceLayout", 15.f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("SpaceLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSpacing(), 15.f, 0.001f, "Spacing should be 15");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUILayoutSpacingStep);
}
ZENITH_TEST(Automation, SetUILayoutChildAlignmentStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutChildAlignmentStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoAlignEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("AlignLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("AlignLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("AlignLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetChildAlignment(), Zenith_UI::ChildAlignment::UpperLeft, "Child alignment should be UpperLeft");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUILayoutChildAlignmentStep);
}
ZENITH_TEST(Automation, SetUILayoutPaddingStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutPaddingStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoPadEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("PadLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutPadding("PadLayout", 10.f, 20.f, 30.f, 40.f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("PadLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	Zenith_Maths::Vector4 xPadding = pxLayout->GetPadding();
	ZENITH_ASSERT_EQ_FLOAT(xPadding.x, 10.f, 0.001f, "Padding left should be 10");
	ZENITH_ASSERT_EQ_FLOAT(xPadding.y, 20.f, 0.001f, "Padding top should be 20");
	ZENITH_ASSERT_EQ_FLOAT(xPadding.z, 30.f, 0.001f, "Padding right should be 30");
	ZENITH_ASSERT_EQ_FLOAT(xPadding.w, 40.f, 0.001f, "Padding bottom should be 40");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUILayoutPaddingStep);
}
ZENITH_TEST(Automation, SetUILayoutFitToContentStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutFitToContentStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoFitEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("FitLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("FitLayout", false);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("FitLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetFitToContent(), false, "FitToContent should be false");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUILayoutFitToContentStep);
}
ZENITH_TEST(Automation, SetUILayoutChildForceExpandStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutChildForceExpandStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoExpandEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("ExpandLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildForceExpand("ExpandLayout", true, false);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("ExpandLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetChildForceExpandWidth(), true, "ChildForceExpandWidth should be true");
	ZENITH_ASSERT_EQ(pxLayout->GetChildForceExpandHeight(), false, "ChildForceExpandHeight should be false");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUILayoutChildForceExpandStep);
}
ZENITH_TEST(Automation, SetUILayoutReverseStep)
{
	EDITOR_TEST_BEGIN(TestSetUILayoutReverseStep);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoRevEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("RevLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutReverse("RevLayout", true);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("RevLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");
	ZENITH_ASSERT_EQ(pxLayout->GetReverseArrangement(), true, "ReverseArrangement should be true");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUILayoutReverseStep);
}
ZENITH_TEST(Automation, LayoutHorizontalPositioning)
{
	EDITOR_TEST_BEGIN(TestLayoutHorizontalPositioning);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoHPosEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("HLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("HLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("HLayout", 10.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("HLayout", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("HLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("RectA");
	g_xEngine.EditorAutomation().AddStep_SetUISize("RectA", 50.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("RectB");
	g_xEngine.EditorAutomation().AddStep_SetUISize("RectB", 80.f, 40.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("RectC");
	g_xEngine.EditorAutomation().AddStep_SetUISize("RectC", 60.f, 20.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HLayout", "RectA");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HLayout", "RectB");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("HLayout", "RectC");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 16; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutHorizontalPositioning);
}
ZENITH_TEST(Automation, LayoutVerticalPositioning)
{
	EDITOR_TEST_BEGIN(TestLayoutVerticalPositioning);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoVPosEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("VLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("VLayout", static_cast<int>(Zenith_UI::LayoutDirection::Vertical));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("VLayout", 5.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("VLayout", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("VLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("VA");
	g_xEngine.EditorAutomation().AddStep_SetUISize("VA", 100.f, 20.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("VB");
	g_xEngine.EditorAutomation().AddStep_SetUISize("VB", 80.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("VLayout", "VA");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("VLayout", "VB");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 13; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutVerticalPositioning);
}
ZENITH_TEST(Automation, LayoutPaddingAffectsPositioning)
{
	EDITOR_TEST_BEGIN(TestLayoutPaddingAffectsPositioning);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoPadPosEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("PadPosLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("PadPosLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("PadPosLayout", 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutPadding("PadPosLayout", 10.f, 20.f, 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("PadPosLayout", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("PadPosLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("PadChild");
	g_xEngine.EditorAutomation().AddStep_SetUISize("PadChild", 50.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("PadPosLayout", "PadChild");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 11; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutPaddingAffectsPositioning);
}
ZENITH_TEST(Automation, LayoutMiddleCenterAlignment)
{
	EDITOR_TEST_BEGIN(TestLayoutMiddleCenterAlignment);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoMCEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("MCLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("MCLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("MCLayout", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("MCLayout", false);
	g_xEngine.EditorAutomation().AddStep_SetUISize("MCLayout", 400.f, 100.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("MCChild");
	g_xEngine.EditorAutomation().AddStep_SetUISize("MCChild", 50.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("MCLayout", "MCChild");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 10; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("MCLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxChild = pxLayout->GetChild(0);
	// Cross-axis (vertical) should be centered: (100 - 30) / 2 = 35
	ZENITH_ASSERT_EQ_FLOAT(pxChild->GetPosition().y, 35.f, 0.001f, "Child position.y should be 35 (centered on cross axis)");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutMiddleCenterAlignment);
}
ZENITH_TEST(Automation, LayoutUpperLeftAlignment)
{
	EDITOR_TEST_BEGIN(TestLayoutUpperLeftAlignment);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoULEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("ULLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("ULLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("ULLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("ULLayout", false);
	g_xEngine.EditorAutomation().AddStep_SetUISize("ULLayout", 400.f, 100.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("ULChild");
	g_xEngine.EditorAutomation().AddStep_SetUISize("ULChild", 50.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("ULLayout", "ULChild");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 10; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("ULLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxChild = pxLayout->GetChild(0);
	// Upper = top-aligned, cross-axis Y should be 0
	ZENITH_ASSERT_EQ_FLOAT(pxChild->GetPosition().y, 0.f, 0.001f, "Child position.y should be 0 (top-aligned)");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutUpperLeftAlignment);
}
ZENITH_TEST(Automation, LayoutLowerRightAlignment)
{
	EDITOR_TEST_BEGIN(TestLayoutLowerRightAlignment);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoLREntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("LRLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("LRLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("LRLayout", static_cast<int>(Zenith_UI::ChildAlignment::LowerRight));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("LRLayout", false);
	g_xEngine.EditorAutomation().AddStep_SetUISize("LRLayout", 400.f, 100.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("LRChild");
	g_xEngine.EditorAutomation().AddStep_SetUISize("LRChild", 50.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("LRLayout", "LRChild");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 10; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("LRLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	pxLayout->Update(0.f);

	Zenith_UI::Zenith_UIElement* pxChild = pxLayout->GetChild(0);
	// Lower = bottom-aligned, cross-axis Y should be 100 - 30 = 70
	ZENITH_ASSERT_EQ_FLOAT(pxChild->GetPosition().y, 70.f, 0.001f, "Child position.y should be 70 (bottom-aligned)");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutLowerRightAlignment);
}
ZENITH_TEST(Automation, LayoutReverseArrangement)
{
	EDITOR_TEST_BEGIN(TestLayoutReverseArrangement);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoRevArrEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("RevArrLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("RevArrLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("RevArrLayout", 10.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutReverse("RevArrLayout", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("RevArrLayout", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("RevArrLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("RevA");
	g_xEngine.EditorAutomation().AddStep_SetUISize("RevA", 50.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("RevB");
	g_xEngine.EditorAutomation().AddStep_SetUISize("RevB", 80.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("RevArrLayout", "RevA");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("RevArrLayout", "RevB");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 14; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutReverseArrangement);
}
ZENITH_TEST(Automation, LayoutChildForceExpand)
{
	EDITOR_TEST_BEGIN(TestLayoutChildForceExpand);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoForceExpEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("ForceExpLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("ForceExpLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildForceExpand("ForceExpLayout", true, false);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("ForceExpLayout", false);
	g_xEngine.EditorAutomation().AddStep_SetUISize("ForceExpLayout", 300.f, 50.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("ForceExpLayout", 0.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("ForceExpLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("ExpA");
	g_xEngine.EditorAutomation().AddStep_SetUISize("ExpA", 50.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("ExpB");
	g_xEngine.EditorAutomation().AddStep_SetUISize("ExpB", 50.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("ForceExpLayout", "ExpA");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("ForceExpLayout", "ExpB");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 15; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutChildForceExpand);
}
ZENITH_TEST(Automation, LayoutFitToContentResizing)
{
	EDITOR_TEST_BEGIN(TestLayoutFitToContentResizing);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoFitResEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("FitResLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("FitResLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("FitResLayout", 10.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("FitResLayout", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("FitResLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("FitA");
	g_xEngine.EditorAutomation().AddStep_SetUISize("FitA", 100.f, 40.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("FitB");
	g_xEngine.EditorAutomation().AddStep_SetUISize("FitB", 80.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("FitResLayout", "FitA");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("FitResLayout", "FitB");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 13; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutFitToContentResizing);
}
ZENITH_TEST(Automation, LayoutWithTextChild)
{
	EDITOR_TEST_BEGIN(TestLayoutWithTextChild);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoTextLayoutEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("TextLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("TextLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("TextLayout", 8.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("TextLayout", static_cast<int>(Zenith_UI::ChildAlignment::MiddleCenter));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("TextLayout", true);
	g_xEngine.EditorAutomation().AddStep_CreateUIImage("TLImg");
	g_xEngine.EditorAutomation().AddStep_SetUISize("TLImg", 36.f, 36.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIText("TLText", "Test");
	g_xEngine.EditorAutomation().AddStep_SetUIFontSize("TLText", 36.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("TextLayout", "TLImg");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("TextLayout", "TLText");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 13; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutWithTextChild);
}
ZENITH_TEST(Automation, LayoutEmptyGroup)
{
	EDITOR_TEST_BEGIN(TestLayoutEmptyGroup);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoEmptyEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("EmptyLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutPadding("EmptyLayout", 5.f, 10.f, 15.f, 20.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("EmptyLayout", true);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 5; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UILayoutGroup* pxLayout = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("EmptyLayout");
	ZENITH_ASSERT_NOT_NULL(pxLayout, "Should find layout group");

	// Should not crash with no children
	pxLayout->Update(0.f);

	// With fit-to-content, size should be padding only: (5+15, 10+20) = (20, 30)
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().x, 20.f, 0.001f, "Empty layout width should be paddingLeft + paddingRight");
	ZENITH_ASSERT_EQ_FLOAT(pxLayout->GetSize().y, 30.f, 0.001f, "Empty layout height should be paddingTop + paddingBottom");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutEmptyGroup);
}
ZENITH_TEST(Automation, LayoutSingleChild)
{
	EDITOR_TEST_BEGIN(TestLayoutSingleChild);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoSingleEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("SingleLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("SingleLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("SingleLayout", 100.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("SingleLayout", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("SingleLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("SingleChild");
	g_xEngine.EditorAutomation().AddStep_SetUISize("SingleChild", 50.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("SingleLayout", "SingleChild");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 10; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutSingleChild);
}
ZENITH_TEST(Automation, LayoutInvisibleChildrenSkipped)
{
	EDITOR_TEST_BEGIN(TestLayoutInvisibleChildrenSkipped);

	g_xEngine.EditorAutomation().Reset();

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoInvisEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("InvisLayout");
	g_xEngine.EditorAutomation().AddStep_SetUILayoutDirection("InvisLayout", static_cast<int>(Zenith_UI::LayoutDirection::Horizontal));
	g_xEngine.EditorAutomation().AddStep_SetUILayoutSpacing("InvisLayout", 10.f);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutFitToContent("InvisLayout", true);
	g_xEngine.EditorAutomation().AddStep_SetUILayoutChildAlignment("InvisLayout", static_cast<int>(Zenith_UI::ChildAlignment::UpperLeft));
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("InvisA");
	g_xEngine.EditorAutomation().AddStep_SetUISize("InvisA", 50.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("InvisB");
	g_xEngine.EditorAutomation().AddStep_SetUISize("InvisB", 80.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_SetUIVisible("InvisB", false);
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("InvisC");
	g_xEngine.EditorAutomation().AddStep_SetUISize("InvisC", 60.f, 30.f);
	g_xEngine.EditorAutomation().AddStep_AddUIChild("InvisLayout", "InvisA");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("InvisLayout", "InvisB");
	g_xEngine.EditorAutomation().AddStep_AddUIChild("InvisLayout", "InvisC");
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 17; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
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

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutInvisibleChildrenSkipped);
}
ZENITH_TEST(Automation, LayoutSerializationRoundTrip)
{
	EDITOR_TEST_BEGIN(TestLayoutSerializationRoundTrip);

	g_xEngine.EditorAutomation().Reset();

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

	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestLayoutSerializationRoundTrip);
}

//=============================================================================
// UIStyle Automation Tests
//=============================================================================
ZENITH_TEST(Automation, SetUICornerRadiusStep)
{
	EDITOR_TEST_BEGIN(TestSetUICornerRadiusStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoCornerRadiusEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("TestRect");
	g_xEngine.EditorAutomation().AddStep_SetUICornerRadius("TestRect", 12.0f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	auto* pxRect = xUI.FindElement<Zenith_UI::Zenith_UIRect>("TestRect");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_fCornerRadius, 12.0f, 0.001f, "Corner radius should be 12");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUICornerRadiusStep);
}
ZENITH_TEST(Automation, SetUIGradientColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIGradientColorStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoGradientEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("TestRect");
	g_xEngine.EditorAutomation().AddStep_SetUIGradientColor("TestRect", 0.5f, 0.3f, 0.1f, 1.0f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxRect = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIRect>("TestRect");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xGradientBottomColor.x, 0.5f, 0.001f, "Gradient R should be 0.5");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xGradientBottomColor.y, 0.3f, 0.001f, "Gradient G should be 0.3");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIGradientColorStep);
}
ZENITH_TEST(Automation, SetUIShadowStep)
{
	EDITOR_TEST_BEGIN(TestSetUIShadowStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoShadowEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("TestRect");
	g_xEngine.EditorAutomation().AddStep_SetUIShadow("TestRect", 3.0f, 4.0f, 2.0f, true);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxRect = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIRect>("TestRect");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect");
	ZENITH_ASSERT_EQ(pxRect->GetStyle().m_bShadowEnabled, true, "Shadow should be enabled");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xShadowOffset.x, 3.0f, 0.001f, "Shadow offset X should be 3");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xShadowOffset.y, 4.0f, 0.001f, "Shadow offset Y should be 4");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_fShadowSpread, 2.0f, 0.001f, "Shadow spread should be 2");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIShadowStep);
}
ZENITH_TEST(Automation, SetUIShadowColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIShadowColorStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoShadowColorEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("TestRect");
	g_xEngine.EditorAutomation().AddStep_SetUIShadowColor("TestRect", 0.1f, 0.2f, 0.3f, 0.5f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxRect = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIRect>("TestRect");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xShadowColor.x, 0.1f, 0.001f, "Shadow color R");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xShadowColor.w, 0.5f, 0.001f, "Shadow color A");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIShadowColorStep);
}
ZENITH_TEST(Automation, SetUIRectBorderStep)
{
	EDITOR_TEST_BEGIN(TestSetUIRectBorderStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoRectBorderEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIRect("TestRect");
	g_xEngine.EditorAutomation().AddStep_SetUIRectBorder("TestRect", 0.5f, 0.6f, 0.7f, 3.0f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxRect = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIRect>("TestRect");
	ZENITH_ASSERT_NOT_NULL(pxRect, "Should find UI rect");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_xBorderColor.x, 0.5f, 0.001f, "Border color R");
	ZENITH_ASSERT_EQ_FLOAT(pxRect->GetStyle().m_fBorderThickness, 3.0f, 0.001f, "Border thickness should be 3");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIRectBorderStep);
}
ZENITH_TEST(Automation, SetUITextShadowStep)
{
	EDITOR_TEST_BEGIN(TestSetUITextShadowStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoTextShadowEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("TestText", "Hello");
	g_xEngine.EditorAutomation().AddStep_SetUITextShadow("TestText", 2.0f, 3.0f, true);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxText = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIText>("TestText");
	ZENITH_ASSERT_NOT_NULL(pxText, "Should find UI text");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUITextShadowStep);
}
ZENITH_TEST(Automation, SetUITextShadowColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUITextShadowColorStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoTextShadowColorEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIText("TestText", "Hello");
	g_xEngine.EditorAutomation().AddStep_SetUITextShadowColor("TestText", 0.1f, 0.2f, 0.3f, 0.8f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxText = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIText>("TestText");
	ZENITH_ASSERT_NOT_NULL(pxText, "Should find UI text");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUITextShadowColorStep);
}
ZENITH_TEST(Automation, SetUIButtonCornerRadiusStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonCornerRadiusStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoBtnCornerEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("TestBtn", "Click");
	g_xEngine.EditorAutomation().AddStep_SetUIButtonCornerRadius("TestBtn", 16.0f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxBtn = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_fCornerRadius, 16.0f, 0.001f, "Normal corner radius should be 16");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetHoveredStyle().m_fCornerRadius, 16.0f, 0.001f, "Hovered corner radius should be 16");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetPressedStyle().m_fCornerRadius, 16.0f, 0.001f, "Pressed corner radius should be 16");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIButtonCornerRadiusStep);
}
ZENITH_TEST(Automation, SetUIButtonShadowStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonShadowStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoBtnShadowEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("TestBtn", "Click");
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadow("TestBtn", 3.0f, 3.0f, 2.0f, true);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxBtn = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ(pxBtn->GetNormalStyle().m_bShadowEnabled, true, "Shadow should be enabled on all states");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_xShadowOffset.x, 3.0f, 0.001f, "Shadow offset X");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_fShadowSpread, 2.0f, 0.001f, "Shadow spread");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIButtonShadowStep);
}
ZENITH_TEST(Automation, SetUIButtonShadowColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonShadowColorStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoBtnShadowColEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("TestBtn", "Click");
	g_xEngine.EditorAutomation().AddStep_SetUIButtonShadowColor("TestBtn", 0.0f, 0.0f, 0.0f, 0.3f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxBtn = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_xShadowColor.w, 0.3f, 0.001f, "Shadow color A should be 0.3");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIButtonShadowColorStep);
}
ZENITH_TEST(Automation, SetUIButtonGradientColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonGradientColorStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoBtnGradEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("TestBtn", "Click");
	g_xEngine.EditorAutomation().AddStep_SetUIButtonGradientColor("TestBtn", 0.2f, 0.4f, 0.6f, 1.0f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxBtn = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_xGradientBottomColor.x, 0.2f, 0.001f, "Gradient R");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_xGradientBottomColor.y, 0.4f, 0.001f, "Gradient G");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIButtonGradientColorStep);
}
ZENITH_TEST(Automation, SetUIButtonBorderColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonBorderColorStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoBtnBorderColEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("TestBtn", "Click");
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderColor("TestBtn", 0.3f, 0.5f, 0.7f, 1.0f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxBtn = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_xBorderColor.x, 0.3f, 0.001f, "Border color R");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetHoveredStyle().m_xBorderColor.y, 0.5f, 0.001f, "Hover border color G");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetPressedStyle().m_xBorderColor.z, 0.7f, 0.001f, "Pressed border color B");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIButtonBorderColorStep);
}
ZENITH_TEST(Automation, SetUIButtonBorderThicknessStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonBorderThicknessStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoBtnBorderThickEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("TestBtn", "Click");
	g_xEngine.EditorAutomation().AddStep_SetUIButtonBorderThickness("TestBtn", 3.0f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxBtn = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");
	ZENITH_ASSERT_EQ_FLOAT(pxBtn->GetNormalStyle().m_fBorderThickness, 3.0f, 0.001f, "Border thickness should be 3");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIButtonBorderThicknessStep);
}
ZENITH_TEST(Automation, SetUIButtonTransitionDurationStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonTransitionDurationStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoBtnTransEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("TestBtn", "Click");
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTransitionDuration("TestBtn", 0.25f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxBtn = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIButtonTransitionDurationStep);
}
ZENITH_TEST(Automation, SetUIButtonTextShadowStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonTextShadowStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoBtnTextShadEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("TestBtn", "Click");
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadow("TestBtn", 1.0f, 2.0f, true);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxBtn = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestSetUIButtonTextShadowStep);
}
ZENITH_TEST(Automation, SetUIButtonTextShadowColorStep)
{
	EDITOR_TEST_BEGIN(TestSetUIButtonTextShadowColorStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoBtnTextShadColEntity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUIButton("TestBtn", "Click");
	g_xEngine.EditorAutomation().AddStep_SetUIButtonTextShadowColor("TestBtn", 0.0f, 0.0f, 0.0f, 0.4f);
	g_xEngine.EditorAutomation().Begin();

	for (uint32_t i = 0; i < 4; i++)
		g_xEngine.EditorAutomation().ExecuteNextStep();

	auto* pxBtn = g_xEngine.Editor().GetSelectedEntity()->GetComponent<Zenith_UIComponent>()
		.FindElement<Zenith_UI::Zenith_UIButton>("TestBtn");
	ZENITH_ASSERT_NOT_NULL(pxBtn, "Should find UI button");

	g_xEngine.EditorAutomation().ExecuteNextStep();
	g_xEngine.EditorAutomation().Reset();

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

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("Entity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("TestGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundColor("TestGroup", 0.1f, 0.2f, 0.3f, 0.5f);
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	auto* pxGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("TestGroup");
	ZENITH_ASSERT_TRUE(pxGroup->HasBackground(), "Background must be enabled");
	ZENITH_ASSERT_EQ_FLOAT(pxGroup->GetBackgroundStyle().m_xFillColor.x, 0.1f, 0.001f, "Bg color R");
	ZENITH_ASSERT_EQ_FLOAT(pxGroup->GetBackgroundStyle().m_xFillColor.w, 0.5f, 0.001f, "Bg color A");

	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestSetUIBackgroundColorStep);
}
ZENITH_TEST(Automation, SetUIBackgroundCornerRadiusStep)
{
	EDITOR_TEST_BEGIN(TestSetUIBackgroundCornerRadiusStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("Entity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("TestGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundColor("TestGroup", 0.5f, 0.5f, 0.5f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundCornerRadius("TestGroup", 16.0f);
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	auto* pxGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("TestGroup");
	ZENITH_ASSERT_EQ_FLOAT(pxGroup->GetBackgroundStyle().m_fCornerRadius, 16.0f, 0.001f, "Bg corner radius must be 16");

	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestSetUIBackgroundCornerRadiusStep);
}
ZENITH_TEST(Automation, SetUIBackgroundBorderStep)
{
	EDITOR_TEST_BEGIN(TestSetUIBackgroundBorderStep);

	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_CreateEntity("Entity");
	g_xEngine.EditorAutomation().AddStep_AddUI();
	g_xEngine.EditorAutomation().AddStep_CreateUILayoutGroup("TestGroup");
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundColor("TestGroup", 0.5f, 0.5f, 0.5f, 1.0f);
	g_xEngine.EditorAutomation().AddStep_SetUIBackgroundBorder("TestGroup", 0.3f, 0.4f, 0.5f, 2.0f);
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
		g_xEngine.EditorAutomation().ExecuteNextStep();

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	Zenith_UIComponent& xUI = pxEntity->GetComponent<Zenith_UIComponent>();
	auto* pxGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("TestGroup");
	ZENITH_ASSERT_EQ_FLOAT(pxGroup->GetBackgroundStyle().m_xBorderColor.x, 0.3f, 0.001f, "Bg border R");
	ZENITH_ASSERT_EQ_FLOAT(pxGroup->GetBackgroundStyle().m_fBorderThickness, 2.0f, 0.001f, "Bg border thickness");

	g_xEngine.EditorAutomation().Reset();
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

//=============================================================================
// Prefab Variant Authoring via Automation
//=============================================================================
// These tests exercise the four new variant-authoring action types
// (CREATE_PREFAB_FROM_SELECTED, CREATE_PREFAB_VARIANT,
// ADD_PREFAB_VARIANT_OVERRIDE_VEC3, INSTANTIATE_PREFAB) end-to-end.
// Each test composes them as a step queue, runs the queue, and asserts on
// the resulting on-disk file or instantiated entity. Files are deleted in
// the cleanup tail of each test.
//
// Path strings passed to AddStep_* must be static-storage. Tests use string
// literals and `static const char*` aliases to satisfy that contract.
//=============================================================================

ZENITH_TEST(Automation, CreatePrefabFromSelectedStep)
{
	EDITOR_TEST_BEGIN(TestAutomationCreatePrefabFromSelected);

	g_xEngine.EditorAutomation().Reset();

	static const char* szSavePath = "auto_create_from_selected.zpfb";

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoPrefabSrc");
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(11.f, 22.f, 33.f);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("AutoPrefabName", szSavePath);
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	ZENITH_ASSERT_TRUE(std::filesystem::exists(szSavePath),
		"CreatePrefabFromSelected: save file should exist on disk");

	Zenith_Prefab* pxLoaded = Zenith_AssetRegistry::GetView<Zenith_Prefab>(szSavePath);
	ZENITH_ASSERT_NOT_NULL(pxLoaded, "CreatePrefabFromSelected: registry should resolve saved prefab");
	ZENITH_ASSERT_EQ(pxLoaded->GetName(), std::string("AutoPrefabName"),
		"CreatePrefabFromSelected: prefab name should match step argument");
	ZENITH_ASSERT_FALSE(pxLoaded->IsVariant(),
		"CreatePrefabFromSelected: base prefab should not be a variant");

	std::filesystem::remove(szSavePath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationCreatePrefabFromSelected);
}

ZENITH_TEST(Automation, CreatePrefabVariantStep)
{
	EDITOR_TEST_BEGIN(TestAutomationCreatePrefabVariant);

	g_xEngine.EditorAutomation().Reset();

	static const char* szBasePath    = "auto_variant_base.zpfb";
	static const char* szVariantPath = "auto_variant_child.zpfb";

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoVariantBaseSrc");
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("AutoVariantBase", szBasePath);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabVariant("AutoVariantChild", szBasePath, szVariantPath);
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Prefab* pxVariant = Zenith_AssetRegistry::GetView<Zenith_Prefab>(szVariantPath);
	ZENITH_ASSERT_NOT_NULL(pxVariant, "CreatePrefabVariant: variant should reload from disk");
	ZENITH_ASSERT_TRUE(pxVariant->IsVariant(),
		"CreatePrefabVariant: derived prefab should be marked as a variant");
	ZENITH_ASSERT_EQ(pxVariant->GetName(), std::string("AutoVariantChild"),
		"CreatePrefabVariant: variant name should match");
	ZENITH_ASSERT_EQ(pxVariant->GetBasePrefab().GetPath(), std::string(szBasePath),
		"CreatePrefabVariant: base path should match");
	ZENITH_ASSERT_EQ(pxVariant->GetOverrides().GetSize(), 0u,
		"CreatePrefabVariant: a fresh variant has no overrides");

	std::filesystem::remove(szBasePath);
	std::filesystem::remove(szVariantPath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationCreatePrefabVariant);
}

ZENITH_TEST(Automation, AddPrefabVariantOverrideStep)
{
	EDITOR_TEST_BEGIN(TestAutomationAddPrefabVariantOverride);

	g_xEngine.EditorAutomation().Reset();

	static const char* szBasePath    = "auto_override_base.zpfb";
	static const char* szVariantPath = "auto_override_variant.zpfb";

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoOvBaseSrc");
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("AutoOvBase", szBasePath);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabVariant("AutoOvVariant", szBasePath, szVariantPath);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szVariantPath, "Transform", "Position", 5.f, 6.f, 7.f);
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Prefab* pxVariant = Zenith_AssetRegistry::GetView<Zenith_Prefab>(szVariantPath);
	ZENITH_ASSERT_NOT_NULL(pxVariant, "AddPrefabVariantOverride: variant should be loadable post-step");
	ZENITH_ASSERT_EQ(pxVariant->GetOverrides().GetSize(), 1u,
		"AddPrefabVariantOverride: should have exactly one override after the step");
	ZENITH_ASSERT_EQ(pxVariant->GetOverrides().Get(0).m_strComponentName, std::string("Transform"),
		"AddPrefabVariantOverride: component name should round-trip");
	ZENITH_ASSERT_EQ(pxVariant->GetOverrides().Get(0).m_strPropertyPath, std::string("Position"),
		"AddPrefabVariantOverride: property path should round-trip");

	std::filesystem::remove(szBasePath);
	std::filesystem::remove(szVariantPath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationAddPrefabVariantOverride);
}

ZENITH_TEST(Automation, InstantiatePrefabStep)
{
	EDITOR_TEST_BEGIN(TestAutomationInstantiatePrefab);

	g_xEngine.EditorAutomation().Reset();

	static const char* szBasePath = "auto_instantiate_base.zpfb";

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AutoInstSrc");
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(1.f, 2.f, 3.f);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("AutoInstPrefab", szBasePath);
	// Instantiate places the entity at the transform passed to the step (the
	// prefab's baked transform is no longer auto-applied); request (1,2,3).
	g_xEngine.EditorAutomation().AddStep_InstantiatePrefab(szBasePath, "AutoInstClone", 1.f, 2.f, 3.f);
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	// The instantiate step selects the new entity; it should be valid + named.
	Zenith_Entity* pxInstance = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxInstance, "InstantiatePrefab: instantiated entity should be selected");
	ZENITH_ASSERT_EQ(pxInstance->GetName(), std::string("AutoInstClone"),
		"InstantiatePrefab: instance should carry the szEntityName argument");

	Zenith_Maths::Vector3 xPos;
	pxInstance->GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 1.f, 0.001f, "InstantiatePrefab: position X should match requested transform");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 2.f, 0.001f, "InstantiatePrefab: position Y should match requested transform");
	ZENITH_ASSERT_EQ_FLOAT(xPos.z, 3.f, 0.001f, "InstantiatePrefab: position Z should match requested transform");

	std::filesystem::remove(szBasePath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationInstantiatePrefab);
}

ZENITH_TEST(Automation, FullVariantWorkflow_PositionOverride)
{
	EDITOR_TEST_BEGIN(TestAutomationFullVariantWorkflowPosition);

	// End-to-end: build entity, capture prefab, derive variant, override
	// Position, instantiate variant, verify the override won.
	g_xEngine.EditorAutomation().Reset();

	static const char* szBasePath    = "auto_workflow_pos_base.zpfb";
	static const char* szVariantPath = "auto_workflow_pos_variant.zpfb";

	// Build a base entity at the origin.
	g_xEngine.EditorAutomation().AddStep_CreateEntity("WorkflowPosSrc");
	g_xEngine.EditorAutomation().AddStep_SetTransformPosition(0.f, 0.f, 0.f);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("WorkflowPosBase", szBasePath);

	// Derive a variant that overrides Position.
	g_xEngine.EditorAutomation().AddStep_CreatePrefabVariant("WorkflowPosVariant", szBasePath, szVariantPath);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szVariantPath, "Transform", "Position", 50.f, 60.f, 70.f);

	// Instantiate the variant — Position override should win.
	g_xEngine.EditorAutomation().AddStep_InstantiatePrefab(szVariantPath, "WorkflowPosInstance");
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Entity* pxInstance = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxInstance, "FullVariantWorkflow: variant instance should be selected");
	Zenith_Maths::Vector3 xPos;
	pxInstance->GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 50.f, 0.001f, "FullVariantWorkflow: variant Position override X should win");
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 60.f, 0.001f, "FullVariantWorkflow: variant Position override Y should win");
	ZENITH_ASSERT_EQ_FLOAT(xPos.z, 70.f, 0.001f, "FullVariantWorkflow: variant Position override Z should win");

	std::filesystem::remove(szBasePath);
	std::filesystem::remove(szVariantPath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationFullVariantWorkflowPosition);
}

ZENITH_TEST(Automation, FullVariantWorkflow_ScaleOverride)
{
	EDITOR_TEST_BEGIN(TestAutomationFullVariantWorkflowScale);

	// Same as the Position workflow, but using a Scale override — exercises
	// a different registered Transform property and confirms the SetTransformScale
	// on the source isn't accidentally what's getting saved.
	g_xEngine.EditorAutomation().Reset();

	static const char* szBasePath    = "auto_workflow_scale_base.zpfb";
	static const char* szVariantPath = "auto_workflow_scale_variant.zpfb";

	g_xEngine.EditorAutomation().AddStep_CreateEntity("WorkflowScaleSrc");
	g_xEngine.EditorAutomation().AddStep_SetTransformScale(1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("WorkflowScaleBase", szBasePath);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabVariant("WorkflowScaleVariant", szBasePath, szVariantPath);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szVariantPath, "Transform", "Scale", 4.f, 5.f, 6.f);
	g_xEngine.EditorAutomation().AddStep_InstantiatePrefab(szVariantPath, "WorkflowScaleInstance");
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Entity* pxInstance = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxInstance, "FullVariantWorkflowScale: instance should be selected");
	Zenith_Maths::Vector3 xScale;
	pxInstance->GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_EQ_FLOAT(xScale.x, 4.f, 0.001f, "FullVariantWorkflowScale: Scale X should match override");
	ZENITH_ASSERT_EQ_FLOAT(xScale.y, 5.f, 0.001f, "FullVariantWorkflowScale: Scale Y should match override");
	ZENITH_ASSERT_EQ_FLOAT(xScale.z, 6.f, 0.001f, "FullVariantWorkflowScale: Scale Z should match override");

	std::filesystem::remove(szBasePath);
	std::filesystem::remove(szVariantPath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationFullVariantWorkflowScale);
}

ZENITH_TEST(Automation, FullVariantWorkflow_MultipleOverrides)
{
	EDITOR_TEST_BEGIN(TestAutomationMultipleOverrides);

	// Two overrides on the same component (Position + Scale on Transform) —
	// both should apply to the instantiated entity.
	g_xEngine.EditorAutomation().Reset();

	static const char* szBasePath    = "auto_workflow_multi_base.zpfb";
	static const char* szVariantPath = "auto_workflow_multi_variant.zpfb";

	g_xEngine.EditorAutomation().AddStep_CreateEntity("MultiSrc");
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("MultiBase", szBasePath);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabVariant("MultiVariant", szBasePath, szVariantPath);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szVariantPath, "Transform", "Position", 9.f, 18.f, 27.f);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szVariantPath, "Transform", "Scale", 3.f, 3.f, 3.f);
	g_xEngine.EditorAutomation().AddStep_InstantiatePrefab(szVariantPath, "MultiInstance");
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Entity* pxInstance = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxInstance, "MultipleOverrides: instance should be selected");
	Zenith_Maths::Vector3 xPos, xScale;
	pxInstance->GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	pxInstance->GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 9.f, 0.001f,   "MultipleOverrides: Position X");
	ZENITH_ASSERT_EQ_FLOAT(xScale.x, 3.f, 0.001f, "MultipleOverrides: Scale X");

	std::filesystem::remove(szBasePath);
	std::filesystem::remove(szVariantPath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationMultipleOverrides);
}

ZENITH_TEST(Automation, VariantChainViaAutomation)
{
	EDITOR_TEST_BEGIN(TestAutomationVariantChain);

	// Three-level chain A -> B -> C built entirely through automation steps.
	// B overrides Position, C overrides Scale. Instantiating C should produce
	// an entity that inherits A's components plus both overrides.
	g_xEngine.EditorAutomation().Reset();

	static const char* szPathA = "auto_chain_a.zpfb";
	static const char* szPathB = "auto_chain_b.zpfb";
	static const char* szPathC = "auto_chain_c.zpfb";

	// A: base prefab from a fresh entity
	g_xEngine.EditorAutomation().AddStep_CreateEntity("ChainAutoSrc");
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("ChainAutoA", szPathA);

	// B: variant of A with Position override
	g_xEngine.EditorAutomation().AddStep_CreatePrefabVariant("ChainAutoB", szPathA, szPathB);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szPathB, "Transform", "Position", 100.f, 200.f, 300.f);

	// C: variant of B with Scale override
	g_xEngine.EditorAutomation().AddStep_CreatePrefabVariant("ChainAutoC", szPathB, szPathC);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szPathC, "Transform", "Scale", 8.f, 8.f, 8.f);

	// Instantiate C and verify both overrides win.
	g_xEngine.EditorAutomation().AddStep_InstantiatePrefab(szPathC, "ChainAutoCInstance");

	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Entity* pxInstance = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxInstance, "VariantChain: instance should be selected");
	Zenith_Maths::Vector3 xPos, xScale;
	pxInstance->GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
	pxInstance->GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 100.f, 0.001f, "VariantChain: B's Position override should propagate via C");
	ZENITH_ASSERT_EQ_FLOAT(xScale.x, 8.f,  0.001f, "VariantChain: C's Scale override should win");

	std::filesystem::remove(szPathA);
	std::filesystem::remove(szPathB);
	std::filesystem::remove(szPathC);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationVariantChain);
}

ZENITH_TEST(Automation, InstantiatePrefabUsesPrefabNameWhenEmpty)
{
	EDITOR_TEST_BEGIN(TestAutomationInstantiateEmptyName);

	// Empty entity name -> the instantiate step falls back to the prefab's name.
	g_xEngine.EditorAutomation().Reset();

	static const char* szPath = "auto_empty_name.zpfb";

	g_xEngine.EditorAutomation().AddStep_CreateEntity("EmptyNameSrc");
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("EmptyNamePrefab", szPath);
	g_xEngine.EditorAutomation().AddStep_InstantiatePrefab(szPath, "");
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Entity* pxInstance = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxInstance, "InstantiatePrefabEmpty: should still produce a valid entity");
	ZENITH_ASSERT_EQ(pxInstance->GetName(), std::string("EmptyNamePrefab"),
		"InstantiatePrefabEmpty: instance should fall back to the prefab's own name");

	std::filesystem::remove(szPath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationInstantiateEmptyName);
}

ZENITH_TEST(Automation, InstantiateBaseAndVariantBothWork)
{
	EDITOR_TEST_BEGIN(TestAutomationInstantiateBaseAndVariant);

	// Same base prefab can be instantiated alongside its variant; they should
	// produce two distinct entities with different transforms.
	g_xEngine.EditorAutomation().Reset();

	static const char* szBasePath    = "auto_both_base.zpfb";
	static const char* szVariantPath = "auto_both_variant.zpfb";

	g_xEngine.EditorAutomation().AddStep_CreateEntity("BothSrc");
	g_xEngine.EditorAutomation().AddStep_SetTransformScale(1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("BothBase", szBasePath);

	g_xEngine.EditorAutomation().AddStep_CreatePrefabVariant("BothVariant", szBasePath, szVariantPath);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szVariantPath, "Transform", "Scale", 5.f, 5.f, 5.f);

	// Instantiate the base — Scale should remain (1, 1, 1).
	g_xEngine.EditorAutomation().AddStep_InstantiatePrefab(szBasePath, "BothBaseInstance");
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}
	{
		Zenith_Entity* pxBaseInstance = g_xEngine.Editor().GetSelectedEntity();
		ZENITH_ASSERT_NOT_NULL(pxBaseInstance, "BaseAndVariant: base instance should exist");
		Zenith_Maths::Vector3 xBaseScale;
		pxBaseInstance->GetComponent<Zenith_TransformComponent>().GetScale(xBaseScale);
		ZENITH_ASSERT_EQ_FLOAT(xBaseScale.x, 1.f, 0.001f, "BaseAndVariant: base scale should be 1");
	}
	// Snapshot the EntityID by VALUE — g_xEngine.Editor().GetSelectedEntity() returns
	// a pointer to a singleton selection slot, so re-reading the pointer after a
	// new selection would just give us the new ID. Compare IDs, not pointers.
	const Zenith_EntityID xBaseInstanceID = g_xEngine.Editor().GetSelectedEntity()->GetEntityID();

	// Now instantiate the variant — Scale should be 5.
	g_xEngine.EditorAutomation().Reset();
	g_xEngine.EditorAutomation().AddStep_InstantiatePrefab(szVariantPath, "BothVariantInstance");
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}
	Zenith_Entity* pxVariantInstance = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxVariantInstance, "BaseAndVariant: variant instance should exist");
	const Zenith_EntityID xVariantInstanceID = pxVariantInstance->GetEntityID();
	Zenith_Maths::Vector3 xVariantScale;
	pxVariantInstance->GetComponent<Zenith_TransformComponent>().GetScale(xVariantScale);
	ZENITH_ASSERT_EQ_FLOAT(xVariantScale.x, 5.f, 0.001f, "BaseAndVariant: variant scale should match override");

	// Two distinct entities.
	ZENITH_ASSERT_NE(xBaseInstanceID, xVariantInstanceID,
		"BaseAndVariant: base and variant instances should be distinct entities");

	std::filesystem::remove(szBasePath);
	std::filesystem::remove(szVariantPath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationInstantiateBaseAndVariant);
}

ZENITH_TEST(Automation, OverrideAccumulatesAcrossSteps)
{
	EDITOR_TEST_BEGIN(TestAutomationOverrideAccumulates);

	// Two separate AddPrefabVariantOverrideVec3 steps on the same variant
	// should accumulate (not replace) — counts and ordering verified via the
	// loaded variant's GetOverrides() vector.
	g_xEngine.EditorAutomation().Reset();

	static const char* szBasePath    = "auto_accum_base.zpfb";
	static const char* szVariantPath = "auto_accum_variant.zpfb";

	g_xEngine.EditorAutomation().AddStep_CreateEntity("AccumSrc");
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("AccumBase", szBasePath);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabVariant("AccumVariant", szBasePath, szVariantPath);

	// Two distinct (component, property) pairs — both should land.
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szVariantPath, "Transform", "Position", 1.f, 1.f, 1.f);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szVariantPath, "Transform", "Scale", 2.f, 2.f, 2.f);
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Prefab* pxVariant = Zenith_AssetRegistry::GetView<Zenith_Prefab>(szVariantPath);
	ZENITH_ASSERT_NOT_NULL(pxVariant, "OverrideAccumulates: variant should reload from disk");
	ZENITH_ASSERT_EQ(pxVariant->GetOverrides().GetSize(), 2u,
		"OverrideAccumulates: two distinct overrides should both persist");

	std::filesystem::remove(szBasePath);
	std::filesystem::remove(szVariantPath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationOverrideAccumulates);
}

ZENITH_TEST(Automation, SamePropertyOverrideTwiceReplaces)
{
	EDITOR_TEST_BEGIN(TestAutomationSamePropertyReplaces);

	// AddOverride dedupes by (component, propertyPath). Two AddOverrideVec3
	// steps with identical component + property should produce ONE override
	// — the last value wins.
	g_xEngine.EditorAutomation().Reset();

	static const char* szBasePath    = "auto_replace_base.zpfb";
	static const char* szVariantPath = "auto_replace_variant.zpfb";

	g_xEngine.EditorAutomation().AddStep_CreateEntity("ReplaceSrc");
	g_xEngine.EditorAutomation().AddStep_CreatePrefabFromSelected("ReplaceBase", szBasePath);
	g_xEngine.EditorAutomation().AddStep_CreatePrefabVariant("ReplaceVariant", szBasePath, szVariantPath);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szVariantPath, "Transform", "Scale", 2.f, 2.f, 2.f);
	g_xEngine.EditorAutomation().AddStep_AddPrefabVariantOverrideVec3(
		szVariantPath, "Transform", "Scale", 7.f, 7.f, 7.f);
	g_xEngine.EditorAutomation().AddStep_InstantiatePrefab(szVariantPath, "ReplaceInstance");
	g_xEngine.EditorAutomation().Begin();
	while (!g_xEngine.EditorAutomation().IsComplete())
	{
		g_xEngine.EditorAutomation().ExecuteNextStep();
	}

	Zenith_Prefab* pxVariant = Zenith_AssetRegistry::GetView<Zenith_Prefab>(szVariantPath);
	ZENITH_ASSERT_NOT_NULL(pxVariant, "SamePropertyReplaces: variant should reload");
	ZENITH_ASSERT_EQ(pxVariant->GetOverrides().GetSize(), 1u,
		"SamePropertyReplaces: same (component, property) should not duplicate");

	Zenith_Entity* pxInstance = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxInstance, "SamePropertyReplaces: instance should exist");
	Zenith_Maths::Vector3 xScale;
	pxInstance->GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_EQ_FLOAT(xScale.x, 7.f, 0.001f, "SamePropertyReplaces: last-wins on duplicate path");

	std::filesystem::remove(szBasePath);
	std::filesystem::remove(szVariantPath);
	g_xEngine.EditorAutomation().Reset();
	EDITOR_TEST_END(TestAutomationSamePropertyReplaces);
}

// ============================================================================
// Pure euler-authoring math (ATTACH_TO_BONE / SET_TRANSFORM_ROTATION). No editor
// fixture needed — BuildEulerRotation / BuildEulerOffsetMatrix are pure static.
// Pins the composition order M = T(pos) * Ry * Rx * Rz (= RT_BuildJetpackMount).
// ============================================================================

ZENITH_TEST(Automation, BuildEulerRotationPerAxis)
{
	// Pitch Rx(90): +Z -> -Y.
	{
		const Zenith_Maths::Quat q = Zenith_EditorAutomation::BuildEulerRotation(90.0f, 0.0f, 0.0f);
		ZENITH_ASSERT_NEAR_VEC3(q * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
			Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 1e-4f, "Rx(90): +Z -> -Y");
	}
	// Yaw Ry(90): +Z -> +X.
	{
		const Zenith_Maths::Quat q = Zenith_EditorAutomation::BuildEulerRotation(0.0f, 90.0f, 0.0f);
		ZENITH_ASSERT_NEAR_VEC3(q * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
			Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 1e-4f, "Ry(90): +Z -> +X");
	}
	// Roll Rz(90): +X -> +Y.
	{
		const Zenith_Maths::Quat q = Zenith_EditorAutomation::BuildEulerRotation(0.0f, 0.0f, 90.0f);
		ZENITH_ASSERT_NEAR_VEC3(q * Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),
			Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 1e-4f, "Rz(90): +X -> +Y");
	}
	// Racket rest pose Rx(180): +Y -> -Y.
	{
		const Zenith_Maths::Quat q = Zenith_EditorAutomation::BuildEulerRotation(180.0f, 0.0f, 0.0f);
		ZENITH_ASSERT_NEAR_VEC3(q * Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
			Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 1e-4f, "Rx(180): +Y -> -Y");
	}
}

ZENITH_TEST(Automation, BuildEulerRotationCompositionOrder)
{
	// Ry(90) * Rx(90) on +Y: Rx first (+Y -> +Z), then Ry (+Z -> +X) => +X.
	// The opposite Rx*Ry order would give +Z, so this pins the Ry-before-Rx ordering.
	const Zenith_Maths::Quat q = Zenith_EditorAutomation::BuildEulerRotation(90.0f, 90.0f, 0.0f);
	ZENITH_ASSERT_NEAR_VEC3(q * Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 1e-4f, "Ry(90)*Rx(90): +Y -> +X");

	// Pin Rz's slot in the product too: Ry(90) * Rz(90) applied to +X.
	// Rz first (+X -> +Y), then Ry (+Y unchanged by yaw) => +Y. The alternative
	// Rz*Ry order would instead send +X -> Ry(+X)=-Z -> Rz(-Z)=-Z => -Z, so this
	// discriminates the full Ry * Rx * Rz composition (Rx=identity here).
	const Zenith_Maths::Quat q2 = Zenith_EditorAutomation::BuildEulerRotation(0.0f, 90.0f, 90.0f);
	ZENITH_ASSERT_NEAR_VEC3(q2 * Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), 1e-4f, "Ry(90)*Rz(90): +X -> +Y");
}

ZENITH_TEST(Automation, BuildEulerOffsetMatrixTranslationAndRotation)
{
	const Zenith_Maths::Matrix4 m = Zenith_EditorAutomation::BuildEulerOffsetMatrix(
		1.0f, 2.0f, 3.0f, 90.0f, 0.0f, 0.0f);
	// Translation in the 4th column.
	ZENITH_ASSERT_NEAR_VEC3(Zenith_Maths::Vector3(m[3]),
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), 1e-4f, "offset translation column");
	// Rotation half applies to a direction (w=0): Rx(90) +Z -> -Y.
	const Zenith_Maths::Vector3 xDir(m * Zenith_Maths::Vector4(0.0f, 0.0f, 1.0f, 0.0f));
	ZENITH_ASSERT_NEAR_VEC3(xDir, Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 1e-4f,
		"offset rotation half: Rx(90) +Z -> -Y");
}

// The ATTACH_TO_BONE executor seam (adds Zenith_AttachmentComponent to the selected
// entity, resolves the target by name in the same scene, AttachToBone with the
// euler-built offset). Mirrors the AttachGraphStep test pattern.
ZENITH_TEST(Automation, AttachToBoneStep)
{
	EDITOR_TEST_BEGIN(TestAttachToBoneStep);

	g_xEngine.EditorAutomation().Reset();

	// Target (skeleton) authored FIRST so the executor's FindEntityByName resolves it.
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AttachTarget");
	// Item authored second -> it is the SELECTED entity the executor attaches.
	g_xEngine.EditorAutomation().AddStep_CreateEntity("AttachItem");
	g_xEngine.EditorAutomation().AddStep_AttachToBone("AttachTarget", "RightHand",
		1.0f, 2.0f, 3.0f, 90.0f, 0.0f, 0.0f);
	g_xEngine.EditorAutomation().Begin();

	g_xEngine.EditorAutomation().ExecuteNextStep();   // create target
	g_xEngine.EditorAutomation().ExecuteNextStep();   // create item (selected)
	g_xEngine.EditorAutomation().ExecuteNextStep();   // attach-to-bone

	Zenith_Entity* pxItem = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxItem, "item entity should be selected");
	ZENITH_ASSERT_TRUE(pxItem->HasComponent<Zenith_AttachmentComponent>(),
		"ATTACH_TO_BONE must add a Zenith_AttachmentComponent");

	Zenith_AttachmentComponent& xAtt = pxItem->GetComponent<Zenith_AttachmentComponent>();
	ZENITH_ASSERT_TRUE(xAtt.IsAttached(), "attachment must be live after the step");
	ZENITH_ASSERT_STREQ(xAtt.GetBoneName().c_str(), "RightHand", "bone name must match the step arg");
	ZENITH_ASSERT_NEAR_VEC3(Zenith_Maths::Vector3(xAtt.GetOffset()[3]),
		Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f), 1e-4f, "offset translation must match the step pos args");
	// The resolved skeleton entity must be the named target.
	Zenith_SceneData* pxSceneData = pxItem->GetSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "item must belong to a scene");
	Zenith_Entity xTarget = pxSceneData->FindEntityByName("AttachTarget");
	ZENITH_ASSERT_TRUE(xAtt.GetSkeletonEntity().GetEntityID() == xTarget.GetEntityID(),
		"attachment skeleton entity must be the named target");

	g_xEngine.EditorAutomation().Reset();

	EDITOR_TEST_END(TestAttachToBoneStep);
}

// Boot-tail attribution is gated on the PRODUCTION session, and this test is itself
// the reason why: unit tests drive the same global automation object during boot, and
// their queues complete long before the game's real queue is registered. A test
// session must record nothing, or the tail report becomes a mix of fixtures and real
// authoring work. The named-step overload is covered here too, since the name is what
// makes a long pole legible in that report.
ZENITH_TEST(EditorAutomation, AutomationSessionGating)
{
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.Reset();

	static bool ls_bRan = false;
	ls_bRan = false;
	xAuto.AddStep_Custom([]() { ls_bRan = true; }, "Gating Probe Step");

	// Default session == a test session: no production tail.
	xAuto.Begin();
	xAuto.ExecuteNextStep();

	ZENITH_ASSERT_TRUE(ls_bRan, "the step must still execute in a non-production session");
	ZENITH_ASSERT_EQ(xAuto.GetStepTimings().GetSize(), 0u, "a test session must record NO step timings");

	// The name is carried on the queued action regardless of session kind — only the
	// recording is gated.
	xAuto.Reset();
	xAuto.AddStep_Custom([]() {}, "Named Probe Step");
	ZENITH_ASSERT_STREQ(xAuto.m_axActions.Get(0).m_szStepName.c_str(), "Named Probe Step",
		"the named AddStep_Custom overload must carry the name onto the action");
	xAuto.AddStep_Custom([]() {});
	ZENITH_ASSERT_TRUE(xAuto.m_axActions.Get(1).m_szStepName.empty(),
		"the unnamed overload must leave the name empty (reported as index + type id)");

	xAuto.Reset();
}

//=============================================================================
// Terrain dimensions authoring step
//=============================================================================

ZENITH_TEST(Automation, TerrainSetDimensionsPacksItsPayload)
{
	// The step's payload is the only place the four knobs cross from a game's
	// authoring code into the editor session, and the packing is positional
	// (afArgs[0..1] + aiArgs[0..1]). A transposed slot would stage a terrain of
	// the wrong shape with nothing failing until a bake produced the wrong number
	// of chunks, so the lane assignment is pinned here.
	Zenith_EditorAutomation xAuto;
	xAuto.Reset();
	xAuto.AddStep_TerrainSetDimensions(32.0f, 0.5f, 6, 9);

	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "the step must queue exactly one action");
	const Zenith_EditorAction& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::TERRAIN_EDITOR_SET_DIMENSIONS,
		"the step must queue TERRAIN_EDITOR_SET_DIMENSIONS");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[0], 32.0f, 1.0e-5f, "afArgs[0] carries the chunk world size");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[1], 0.5f, 1.0e-5f, "afArgs[1] carries the vertex spacing");
	ZENITH_ASSERT_EQ(xAction.m_aiArgs[0], 6, "aiArgs[0] carries the X grid extent");
	ZENITH_ASSERT_EQ(xAction.m_aiArgs[1], 9, "aiArgs[1] carries the Z grid extent");

	xAuto.Reset();
}

ZENITH_TEST(Automation, TerrainSetDimensionsIsInsideTheTerrainRoutingRange)
{
	// The block is routed by a pair of range comparisons, and SET_DIMENSIONS was
	// APPENDED past what used to be the last member. If the upper comparison were
	// not moved with it, the action would fall through to the generic executor and
	// assert at boot rather than at build time -- so the membership is asserted
	// from this side too, not only by the header's width static_assert.
	const int iAction = static_cast<int>(Zenith_EditorActionType::TERRAIN_EDITOR_SET_DIMENSIONS);
	const int iFirst = static_cast<int>(Zenith_EditorActionType::TERRAIN_EDITOR_SET_ASSET_SET);
	ZENITH_ASSERT_GE(iAction, iFirst, "SET_DIMENSIONS must sit at or after the terrain block's first member");
	ZENITH_ASSERT_EQ(iAction - static_cast<int>(Zenith_EditorActionType::TERRAIN_EDITOR_EXPORT_CHUNKS_RECT), 1,
		"SET_DIMENSIONS must be the member immediately after the previous last one — the router "
		"compares against it as the block's upper bound");
}

//=============================================================================
// Sub-executor range contiguity
//=============================================================================

ZENITH_TEST(Automation, GrassTypesEnumBlockIsContiguous)
{
	// ExecuteAction dispatches by comparing an action against a block's FIRST
	// and LAST member — it never enumerates the members in between. So a value
	// inserted in the middle of a block silently joins it (harmless), while one
	// inserted anywhere inside a DIFFERENT block's span silently routes to that
	// block's executor and hits its `default:` assert at boot.
	//
	// The header carries a static_assert on the block's width; this pins each
	// member's position inside it, so a reorder that keeps the width fails here
	// with the name of the member that moved.
	const int iFirst = static_cast<int>(Zenith_EditorActionType::GRASS_TYPES_CREATE);
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::GRASS_TYPES_SET_COUNT) - iFirst, 1,
		"GRASS_TYPES_SET_COUNT must be the second member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::GRASS_TYPES_SET_NAME) - iFirst, 2,
		"GRASS_TYPES_SET_NAME must be the third member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::GRASS_TYPES_SET_PARAM_FLOAT) - iFirst, 3,
		"GRASS_TYPES_SET_PARAM_FLOAT must be the fourth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::GRASS_TYPES_SET_PARAM_COLOR) - iFirst, 4,
		"GRASS_TYPES_SET_PARAM_COLOR must be the fifth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::GRASS_TYPES_SAVE) - iFirst, 5,
		"GRASS_TYPES_SAVE must END the block — the router compares against it");

	// ... and the block must not have grown INTO its neighbours: the terrain
	// editor range ends immediately before it, and the prefab range begins
	// immediately after, so both boundaries are pinned from this side too.
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::TERRAIN_EDITOR_SET_DIMENSIONS), 1,
		"the grass-type block must start immediately after the terrain-editor range ends");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::CREATE_PREFAB_FROM_SELECTED) -
		static_cast<int>(Zenith_EditorActionType::GRASS_TYPES_SAVE), 1,
		"the prefab block must start immediately after the grass-type range ends");
}

//=============================================================================
// Animation dope-sheet authoring steps (WU-3.4)
//
// The third of the three layers over the dope sheet: WU-3.1's pure timeline
// maths and WU-3.3's Action_* twins are unit-tested where they live, and these
// pin the AUTOMATION surface on top of them — that a queued step reaches the
// panel at all, that it carries the arguments it was given, and that an
// INDEX-addressed step resolves to the STABLE ID the panel actually takes.
//=============================================================================

ZENITH_TEST(Automation, AnimEnumBlockIsContiguous)
{
	// Same argument as the grass-type block above: ExecuteAction compares an
	// action against this block's FIRST and LAST member and never enumerates
	// what is between them. The header static_asserts the WIDTH; this pins each
	// member's POSITION, so a reorder that preserves the width fails here naming
	// the member that moved rather than at boot inside a neighbour's `default:`.
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_OPEN_CLIP);
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SELECT_KEY) - iFirst, 1,
		"ANIM_SELECT_KEY must be the second member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_BOX_SELECT) - iFirst, 2,
		"ANIM_BOX_SELECT must be the third member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MOVE_SELECTION) - iFirst, 3,
		"ANIM_MOVE_SELECTION must be the fourth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CLOSE_CLIP) - iFirst, 13,
		"ANIM_CLOSE_CLIP must be the fourteenth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_EXPECT_SELECTED_COUNT) - iFirst, 15,
		"ANIM_EXPECT_SELECTED_COUNT must END the block — the router compares against it");

	// ... and the block must not have grown into either neighbour. It was
	// APPENDED after the prefab range precisely so that nothing already pinned
	// had to move (see the header), and the ANIM_POSE block (WU-4.3) — its own
	// contiguous range with its own sub-executor — begins right after it.
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::INSTANTIATE_PREFAB), 1,
		"the ANIM block must start immediately after the prefab range ends");
	// ★ THIS LINE USED TO NAME SET_NAVMESH_ASSET, and it moved with WU-4.3 rather
	// than being deleted: what it pins is that the ANIM range ENDS where the
	// router thinks it does, and the successor being a second animation block
	// instead of the navmesh verb does not weaken that. SET_NAVMESH_ASSET's own
	// "must stay outside every range" is pinned by AnimPoseEnumBlockIsContiguous
	// below, which is where its neighbour now is.
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_SELECT_BONE) -
		static_cast<int>(Zenith_EditorActionType::ANIM_EXPECT_SELECTED_COUNT), 1,
		"the ANIM_POSE block must start immediately after the ANIM range ends — inside it, "
		"the router would hand a pose verb to ExecuteAnimationAction's default: assert");
}

ZENITH_TEST(Automation, AnimPoseEnumBlockIsContiguous)
{
	// The youngest block, pinned the way every block before it is: the header
	// static_asserts the WIDTH, and this pins each member's POSITION so a reorder
	// that preserves the width fails here naming the member that moved.
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_POSE_SELECT_BONE);
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_ROTATE_SELECTED_BONE_WORLD) - iFirst, 1,
		"ANIM_POSE_ROTATE_SELECTED_BONE_WORLD must be the second member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_SET_KEY_FOR_SELECTED_BONE) - iFirst, 2,
		"ANIM_POSE_SET_KEY_FOR_SELECTED_BONE must be the third member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_SET_AUTO_KEY) - iFirst, 3,
		"ANIM_POSE_SET_AUTO_KEY must be the fourth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION) - iFirst, 4,
		"ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION must END the block — the router compares against it");

	// Both boundaries, from this side.
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_EXPECT_SELECTED_COUNT), 1,
		"the ANIM_POSE block must start immediately after the ANIM range ends");
	// ★ THIS LINE USED TO NAME SET_NAVMESH_ASSET, and it moved with WU-6.5 rather
	// than being deleted — exactly as it moved off SET_NAVMESH_ASSET's neighbour
	// once before. What it pins is that the ANIM_POSE range ENDS where the router
	// thinks it does; the successor being a third animation block instead of the
	// navmesh verb does not weaken that. SET_NAVMESH_ASSET's own "must stay
	// outside every range" is pinned by AnimMaskEnumBlockIsContiguous, which is
	// where its neighbour now is (it was AnimSmEnumBlockIsContiguous until WU-7.1
	// appended a fourth animation block — this line has now been re-pointed
	// twice, which is the mechanism working).
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_OPEN) -
		static_cast<int>(Zenith_EditorActionType::ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION), 1,
		"the ANIM_SM block must start immediately after the ANIM_POSE range ends — inside it, the "
		"router would hand a state-machine verb to ExecuteAnimationPoseAction's default: assert");
}

ZENITH_TEST(Automation, AnimSmEnumBlockIsContiguous)
{
	// The youngest block (WU-6.5), pinned the way every block before it is: the
	// header static_asserts the WIDTH, and this pins each member's POSITION so a
	// reorder that preserves the width fails here naming the member that moved
	// rather than at boot inside a neighbour's `default:` assert.
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_SM_OPEN);
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_OPEN_FRESH) - iFirst, 1,
		"ANIM_SM_OPEN_FRESH must be the second member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_CLOSE) - iFirst, 2,
		"ANIM_SM_CLOSE must be the third member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_ADD_STATE) - iFirst, 5,
		"ANIM_SM_ADD_STATE must be the sixth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_ADD_CONDITION) - iFirst, 15,
		"ANIM_SM_ADD_CONDITION must be the sixteenth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_APPLY) - iFirst, 22,
		"ANIM_SM_APPLY must be the twenty-third member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_EXPECT_DEFAULT_STATE) - iFirst, 24,
		"ANIM_SM_EXPECT_DEFAULT_STATE must END the block — the router compares against it");

	// Both boundaries, from this side.
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_POSE_EXPECT_BONE_LOCAL_ROTATION), 1,
		"the ANIM_SM block must start immediately after the ANIM_POSE range ends");
	// ★ THIS LINE USED TO NAME SET_NAVMESH_ASSET, and it MOVED with WU-7.1 rather
	// than being deleted — exactly as it moved off SET_NAVMESH_ASSET's neighbour
	// twice before (WU-4.3, then WU-6.5). What it pins is that the ANIM_SM range
	// ENDS where the router thinks it does; the successor being a fourth animation
	// block instead of the navmesh verb does not weaken that. SET_NAVMESH_ASSET's
	// own "must stay outside every range" is pinned by
	// AnimMaskEnumBlockIsContiguous below, which is where its neighbour now is.
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_OPEN) -
		static_cast<int>(Zenith_EditorActionType::ANIM_SM_EXPECT_DEFAULT_STATE), 1,
		"the ANIM_MASK block must start immediately after the ANIM_SM range ends — inside it, the "
		"router would hand a bone-mask verb to ExecuteAnimStateMachineAction's default: assert");
}

ZENITH_TEST(Automation, AnimMaskEnumBlockIsContiguous)
{
	// The youngest block (WU-7.1), pinned the way every block before it is: the
	// header static_asserts the WIDTH, and this pins each member's POSITION so a
	// reorder that preserves the width fails here naming the member that moved
	// rather than at boot inside a neighbour's `default:` assert.
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_MASK_OPEN);
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_OPEN_FRESH) - iFirst, 1,
		"ANIM_MASK_OPEN_FRESH must be the second member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_CLOSE) - iFirst, 2,
		"ANIM_MASK_CLOSE must be the third member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_SET_WEIGHT) - iFirst, 3,
		"ANIM_MASK_SET_WEIGHT must be the fourth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_SET_SUBTREE) - iFirst, 4,
		"ANIM_MASK_SET_SUBTREE must be the fifth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_SET_HAS_AVATAR) - iFirst, 5,
		"ANIM_MASK_SET_HAS_AVATAR must be the sixth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_UNDO) - iFirst, 6,
		"ANIM_MASK_UNDO must be the seventh member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_REDO) - iFirst, 7,
		"ANIM_MASK_REDO must be the eighth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_SAVE) - iFirst, 8,
		"ANIM_MASK_SAVE must be the ninth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_MASK_EXPECT_WEIGHT) - iFirst, 9,
		"ANIM_MASK_EXPECT_WEIGHT must END the block — the router compares against it");

	// Both boundaries, from this side.
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_SM_EXPECT_DEFAULT_STATE), 1,
		"the ANIM_MASK block must start immediately after the ANIM_SM range ends");
	// ★ THIS LINE USED TO NAME SET_NAVMESH_ASSET, and it MOVED with WU-7.2 rather
	// than being deleted — exactly as it moved off SET_NAVMESH_ASSET's neighbour
	// three times before (WU-4.3, WU-6.5, WU-7.1). What it pins is that the
	// ANIM_MASK range ENDS where the router thinks it does; the successor being a
	// fifth animation block instead of the navmesh verb does not weaken that.
	// SET_NAVMESH_ASSET's own "must stay outside every range" is pinned by
	// AnimLayerEnumBlockIsContiguous below, which is where its neighbour now is.
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_ADD) -
		static_cast<int>(Zenith_EditorActionType::ANIM_MASK_EXPECT_WEIGHT), 1,
		"the ANIM_LAYER block must start immediately after the ANIM_MASK range ends — inside it, the "
		"router would hand a layer verb to ExecuteAnimMaskAction's default: assert");
}

ZENITH_TEST(Automation, AnimLayerEnumBlockIsContiguous)
{
	// The youngest block (WU-7.2), pinned the way every block before it is: the
	// header static_asserts the WIDTH, and this pins each member's POSITION so a
	// reorder that preserves the width fails here naming the member that moved
	// rather than at boot inside a neighbour's `default:` assert.
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_ADD);
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_REMOVE) - iFirst, 1,
		"ANIM_LAYER_REMOVE must be the second member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_RENAME) - iFirst, 2,
		"ANIM_LAYER_RENAME must be the third member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_SET_WEIGHT) - iFirst, 3,
		"ANIM_LAYER_SET_WEIGHT must be the fourth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_SET_BLEND_MODE) - iFirst, 4,
		"ANIM_LAYER_SET_BLEND_MODE must be the fifth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_SET_EMIT_EVENTS) - iFirst, 5,
		"ANIM_LAYER_SET_EMIT_EVENTS must be the sixth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_SET_MASK_PATH) - iFirst, 6,
		"ANIM_LAYER_SET_MASK_PATH must be the seventh member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_MOVE) - iFirst, 7,
		"ANIM_LAYER_MOVE must be the eighth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_SELECT) - iFirst, 8,
		"ANIM_LAYER_SELECT must be the ninth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_EXPECT_ORDER) - iFirst, 9,
		"ANIM_LAYER_EXPECT_ORDER must END the block — the router compares against it");

	// Both boundaries, from this side.
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_MASK_EXPECT_WEIGHT), 1,
		"the ANIM_LAYER block must start immediately after the ANIM_MASK range ends");
	// ★ THIS LINE USED TO NAME SET_NAVMESH_ASSET, and it MOVED with WU-7.3 rather
	// than being deleted — the fifth time it has moved (WU-4.3, WU-6.5, WU-7.1,
	// WU-7.2, and now this). What it pins is that the ANIM_LAYER range ENDS where
	// the router thinks it does; the successor being a sixth animation block
	// instead of the navmesh verb does not weaken that. SET_NAVMESH_ASSET's own
	// "must stay outside every range" is pinned by the unit for whichever block is
	// YOUNGEST — AnimIkEnumBlockIsContiguous as of E1, which is where its
	// neighbour now is.
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_SET_TREE_KIND) -
		static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_EXPECT_ORDER), 1,
		"the ANIM_BLEND block must start immediately after the ANIM_LAYER range ends — inside it, the "
		"router would hand a blend verb to ExecuteAnimLayerAction's default: assert");
}

ZENITH_TEST(Automation, AnimBlendEnumBlockIsContiguous)
{
	// The youngest block (WU-7.3), pinned the way every block before it is: the
	// header static_asserts the WIDTH, and this pins each member's POSITION so a
	// reorder that preserves the width fails here naming the member that moved
	// rather than at boot inside a neighbour's `default:` assert.
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_SET_TREE_KIND);
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_SET_PARAMETER) - iFirst, 1,
		"ANIM_BLEND_SET_PARAMETER must be the second member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_ADD_POINT) - iFirst, 2,
		"ANIM_BLEND_ADD_POINT must be the third member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_REMOVE_POINT) - iFirst, 3,
		"ANIM_BLEND_REMOVE_POINT must be the fourth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_SET_POINT_CLIP) - iFirst, 4,
		"ANIM_BLEND_SET_POINT_CLIP must be the fifth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_SET_POINT_POSITION) - iFirst, 5,
		"ANIM_BLEND_SET_POINT_POSITION must be the sixth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_SELECT_POINT) - iFirst, 6,
		"ANIM_BLEND_SELECT_POINT must be the seventh member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_EXPECT_POINT_COUNT) - iFirst, 7,
		"ANIM_BLEND_EXPECT_POINT_COUNT must be the eighth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_EXPECT_POINT_POSITION) - iFirst, 8,
		"ANIM_BLEND_EXPECT_POINT_POSITION must END the block — the router compares against it");

	// Both boundaries, from this side.
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_LAYER_EXPECT_ORDER), 1,
		"the ANIM_BLEND block must start immediately after the ANIM_LAYER range ends");
	// ★ THIS LINE USED TO NAME SET_NAVMESH_ASSET, and it MOVED with WU-8.2 rather
	// than being deleted — the SIXTH time it has moved (WU-4.3, WU-6.5, WU-7.1,
	// WU-7.2, WU-7.3, and now this). What it pins is that the ANIM_BLEND range
	// ENDS where the router thinks it does; the successor being a seventh
	// animation block instead of the navmesh verb does not weaken that.
	// SET_NAVMESH_ASSET's own "must stay outside every range" is pinned by the unit
	// for whichever block is YOUNGEST — AnimIkEnumBlockIsContiguous as of E1,
	// which is where its neighbour now is.
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_SET_VIEW) -
		static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_EXPECT_POINT_POSITION), 1,
		"the ANIM_CURVE block must start immediately after the ANIM_BLEND range ends — inside it, the "
		"router would hand a curve verb to ExecuteAnimBlendAction's default: assert");
}

ZENITH_TEST(Automation, AnimCurveEnumBlockIsContiguous)
{
	// The SEVENTH animation block (WU-8.2), pinned the way every block before it
	// is: the header static_asserts the WIDTH, and this pins each member's POSITION
	// so a reorder that preserves the width fails here naming the member that moved
	// rather than at boot inside a neighbour's `default:` assert.
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_SET_VIEW);
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_SET_UNIFIED) - iFirst, 1,
		"ANIM_CURVE_SET_UNIFIED must be the second member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_SET_KEY_TANGENTS) - iFirst, 2,
		"ANIM_CURVE_SET_KEY_TANGENTS must be the third member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_SET_SELECTION_AUTO) - iFirst, 3,
		"ANIM_CURVE_SET_SELECTION_AUTO must be the fourth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_SET_SELECTION_LINEAR) - iFirst, 4,
		"ANIM_CURVE_SET_SELECTION_LINEAR must be the fifth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_DRAG_HANDLE_TO_PIXEL) - iFirst, 5,
		"ANIM_CURVE_DRAG_HANDLE_TO_PIXEL must be the sixth member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_FIT_TO_SELECTION) - iFirst, 6,
		"ANIM_CURVE_FIT_TO_SELECTION must be the seventh member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_EXPECT_KEY_TANGENT) - iFirst, 7,
		"ANIM_CURVE_EXPECT_KEY_TANGENT must END the block — the router compares against it");

	// Both boundaries, from this side.
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_BLEND_EXPECT_POINT_POSITION), 1,
		"the ANIM_CURVE block must start immediately after the ANIM_BLEND range ends");
	// ★ THIS LINE USED TO NAME SET_NAVMESH_ASSET, and it MOVED with B3 rather than
	// being deleted — the SEVENTH time it has moved (WU-4.3, WU-6.5, WU-7.1, WU-7.2,
	// WU-7.3, WU-8.2, and now this). What it pins is that the ANIM_CURVE range ENDS
	// where the router thinks it does; the successor being an eighth animation block
	// instead of the navmesh verb does not weaken that. SET_NAVMESH_ASSET's own
	// "must stay outside every range" is pinned by the unit for whichever block is
	// YOUNGEST — AnimIkEnumBlockIsContiguous as of E1, which is where its
	// neighbour now is.
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_TANGENT_SET_KEY_MODE) -
		static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_EXPECT_KEY_TANGENT), 1,
		"the ANIM_TANGENT block must start immediately after the ANIM_CURVE range ends — inside it, the "
		"router would hand a tangent verb to ExecuteAnimCurveAction's default: assert");
}

ZENITH_TEST(Automation, AnimTangentEnumBlockIsContiguous)
{
	// The EIGHTH animation block (B3), pinned the way every block before it is: the
	// header static_asserts the WIDTH, and this pins each member's POSITION so a
	// reorder that preserves the width fails here naming the member that moved
	// rather than at boot inside a neighbour's `default:` assert.
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_TANGENT_SET_KEY_MODE);
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_TANGENT_SET_SELECTION_MODE) - iFirst, 1,
		"ANIM_TANGENT_SET_SELECTION_MODE must be the second member of the block");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_TANGENT_EXPECT_KEY_MODE) - iFirst, 2,
		"ANIM_TANGENT_EXPECT_KEY_MODE must END the block — the router compares against it");

	// Both boundaries, from this side.
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_CURVE_EXPECT_KEY_TANGENT), 1,
		"the ANIM_TANGENT block must start immediately after the ANIM_CURVE range ends");
	// ★ THIS LINE USED TO NAME SET_NAVMESH_ASSET, and it MOVED with E1 rather than
	// being deleted — the EIGHTH time it has moved (WU-4.3, WU-6.5, WU-7.1, WU-7.2,
	// WU-7.3, WU-8.2, B3, and now this). What it pins is that the ANIM_TANGENT
	// range ENDS where the router thinks it does; the successor being a ninth
	// animation block instead of the navmesh verb does not weaken that.
	// SET_NAVMESH_ASSET's own "must stay outside every range" is pinned by
	// AnimIkEnumBlockIsContiguous below, which is where its neighbour now is.
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_IK_BAKE_TO_TARGET) -
		static_cast<int>(Zenith_EditorActionType::ANIM_TANGENT_EXPECT_KEY_MODE), 1,
		"the ANIM_IK block must start immediately after the ANIM_TANGENT range ends — inside it, the "
		"router would hand an IK verb to ExecuteAnimTangentAction's default: assert");
}

ZENITH_TEST(Automation, AnimIkEnumBlockIsContiguous)
{
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_IK_BAKE_TO_TARGET) - static_cast<int>(Zenith_EditorActionType::ANIM_TANGENT_EXPECT_KEY_MODE), 1, "previous boundary");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_ADD) - static_cast<int>(Zenith_EditorActionType::ANIM_IK_BAKE_TO_TARGET), 1, "next boundary pins width");
}

ZENITH_TEST(Automation, AnimEventEnumBlockIsContiguous)
{
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_ADD);
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_IK_BAKE_TO_TARGET), 1, "previous boundary");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_ADD) - iFirst, 0, "ANIM_EVENT_ADD");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_SELECT) - iFirst, 1, "ANIM_EVENT_SELECT");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_MOVE_SELECTED) - iFirst, 2, "ANIM_EVENT_MOVE_SELECTED");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_RENAME) - iFirst, 3, "ANIM_EVENT_RENAME");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_SET_PAYLOAD) - iFirst, 4, "ANIM_EVENT_SET_PAYLOAD");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_SET_EMIT_ON_SCRUB) - iFirst, 5, "ANIM_EVENT_SET_EMIT_ON_SCRUB");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CLIP_SAVE) - static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_SET_EMIT_ON_SCRUB), 1, "next boundary stays outside this range");
}

ZENITH_TEST(Automation, AnimEventAddPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimEventAdd(1.25f, "owned value");
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_EVENT_ADD, "action type");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[0], 1.25f, 0.0f, "float preserved");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
}

ZENITH_TEST(Automation, AnimEventSelectPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimEventSelect(2, 3);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_EVENT_SELECT, "action type");
	ZENITH_ASSERT_EQ(xAction.m_aiArgs[0], 2, "payload preserved");
	ZENITH_ASSERT_EQ(xAction.m_aiArgs[2], 3, "payload preserved");
}

ZENITH_TEST(Automation, AnimEventMoveSelectedPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimEventMoveSelected(1.25f, true);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_EVENT_MOVE_SELECTED, "action type");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[0], 1.25f, 0.0f, "float preserved");
	ZENITH_ASSERT_EQ(xAction.m_bArg, true, "payload preserved");
}

ZENITH_TEST(Automation, AnimEventRenamePacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimEventRename(2, "owned value");
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_EVENT_RENAME, "action type");
	ZENITH_ASSERT_EQ(xAction.m_aiArgs[0], 2, "payload preserved");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
}

ZENITH_TEST(Automation, AnimEventSetPayloadPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimEventSetPayload(2, 2.25f, 3.25f, 4.25f, 5.25f);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_EVENT_SET_PAYLOAD, "action type");
	ZENITH_ASSERT_EQ(xAction.m_aiArgs[0], 2, "payload preserved");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[0], 2.25f, 0.0f, "float preserved");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[1], 3.25f, 0.0f, "float preserved");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[2], 4.25f, 0.0f, "float preserved");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[3], 5.25f, 0.0f, "float preserved");
}

ZENITH_TEST(Automation, AnimEventSetEmitEventsOnScrubPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimEventSetEmitEventsOnScrub(true);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_EVENT_SET_EMIT_ON_SCRUB, "action type");
	ZENITH_ASSERT_EQ(xAction.m_bArg, true, "payload preserved");
}

ZENITH_TEST(Automation, AnimClipEnumBlockIsContiguous)
{
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_CLIP_SAVE);
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_EVENT_SET_EMIT_ON_SCRUB), 1, "previous boundary");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CLIP_SAVE) - iFirst, 0, "ANIM_CLIP_SAVE");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CLIP_SAVE_AS) - iFirst, 1, "ANIM_CLIP_SAVE_AS");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_CLIP_PROMOTE_TO_AUTHORED_OVERRIDE) - iFirst, 2, "ANIM_CLIP_PROMOTE_TO_AUTHORED_OVERRIDE");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_CONTROL_SET_ANGLE_SNAP) - static_cast<int>(Zenith_EditorActionType::ANIM_CLIP_PROMOTE_TO_AUTHORED_OVERRIDE), 1, "next boundary stays outside this range");
}

ZENITH_TEST(Automation, AnimSavePacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSave();
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_CLIP_SAVE, "action type");
}

ZENITH_TEST(Automation, AnimSaveAsPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSaveAs("owned value");
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_CLIP_SAVE_AS, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
}

ZENITH_TEST(Automation, AnimPromoteToAuthoredOverridePacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimPromoteToAuthoredOverride("owned value");
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_CLIP_PROMOTE_TO_AUTHORED_OVERRIDE, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
}

ZENITH_TEST(Automation, AnimPoseControlEnumBlockIsContiguous)
{
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_POSE_CONTROL_SET_ANGLE_SNAP);
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_CLIP_PROMOTE_TO_AUTHORED_OVERRIDE), 1, "previous boundary");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_CONTROL_SET_ANGLE_SNAP) - iFirst, 0, "ANIM_POSE_CONTROL_SET_ANGLE_SNAP");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_CONTROL_CLEAR_BONE_SELECTION) - iFirst, 1, "ANIM_POSE_CONTROL_CLEAR_BONE_SELECTION");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_POSE_CONTROL_SET_KEY_TRANSLATION_FOR_ROOT) - iFirst, 2, "ANIM_POSE_CONTROL_SET_KEY_TRANSLATION_FOR_ROOT");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_SELECT_STATE) - static_cast<int>(Zenith_EditorActionType::ANIM_POSE_CONTROL_SET_KEY_TRANSLATION_FOR_ROOT), 1, "next boundary stays outside this range");
}

ZENITH_TEST(Automation, AnimSetPoseAngleSnapPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSetPoseAngleSnap(true);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_POSE_CONTROL_SET_ANGLE_SNAP, "action type");
	ZENITH_ASSERT_EQ(xAction.m_bArg, true, "payload preserved");
}

ZENITH_TEST(Automation, AnimClearBoneSelectionPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimClearBoneSelection();
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_POSE_CONTROL_CLEAR_BONE_SELECTION, "action type");
}

ZENITH_TEST(Automation, AnimSetKeyTranslationForRootPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSetKeyTranslationForRoot();
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_POSE_CONTROL_SET_KEY_TRANSLATION_FOR_ROOT, "action type");
}

ZENITH_TEST(Automation, AnimSmEditEnumBlockIsContiguous)
{
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_SELECT_STATE);
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_POSE_CONTROL_SET_KEY_TRANSLATION_FOR_ROOT), 1, "previous boundary");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_SELECT_STATE) - iFirst, 0, "ANIM_SM_EDIT_SELECT_STATE");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_SELECT_TRANSITION) - iFirst, 1, "ANIM_SM_EDIT_SELECT_TRANSITION");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_SELECT_ANY_STATE) - iFirst, 2, "ANIM_SM_EDIT_SELECT_ANY_STATE");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_CLEAR_SELECTION) - iFirst, 3, "ANIM_SM_EDIT_CLEAR_SELECTION");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_SET_STATE_POSITION) - iFirst, 4, "ANIM_SM_EDIT_SET_STATE_POSITION");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_REMOVE_CLIP_PATH) - iFirst, 5, "ANIM_SM_EDIT_REMOVE_CLIP_PATH");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_ENABLED) - static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_REMOVE_CLIP_PATH), 1, "next boundary stays outside this range");
}

ZENITH_TEST(Automation, AnimSmSelectStatePacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmSelectState("owned value");
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_EDIT_SELECT_STATE, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
}

ZENITH_TEST(Automation, AnimSmSelectTransitionPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmSelectTransition("owned value", 3);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_EDIT_SELECT_TRANSITION, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
	ZENITH_ASSERT_EQ(xAction.m_aiArgs[0], 3, "payload preserved");
}

ZENITH_TEST(Automation, AnimSmSelectAnyStatePacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmSelectAnyState();
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_EDIT_SELECT_ANY_STATE, "action type");
}

ZENITH_TEST(Automation, AnimSmClearSelectionPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmClearSelection();
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_EDIT_CLEAR_SELECTION, "action type");
}

ZENITH_TEST(Automation, AnimSmSetStatePositionPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmSetStatePosition("owned value", 2.25f, 3.25f);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_EDIT_SET_STATE_POSITION, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[0], 2.25f, 0.0f, "float preserved");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[1], 3.25f, 0.0f, "float preserved");
}

ZENITH_TEST(Automation, AnimSmRemoveClipPathPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmRemoveClipPath("owned value");
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_EDIT_REMOVE_CLIP_PATH, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
}

ZENITH_TEST(Automation, AnimSmPreviewEnumBlockIsContiguous)
{
	const int iFirst = static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_ENABLED);
	ZENITH_ASSERT_EQ(iFirst - static_cast<int>(Zenith_EditorActionType::ANIM_SM_EDIT_REMOVE_CLIP_PATH), 1, "previous boundary");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_ENABLED) - iFirst, 0, "ANIM_SM_PREVIEW_SET_ENABLED");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_TICK) - iFirst, 1, "ANIM_SM_PREVIEW_TICK");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_FLOAT) - iFirst, 2, "ANIM_SM_PREVIEW_SET_FLOAT");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_INT) - iFirst, 3, "ANIM_SM_PREVIEW_SET_INT");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_BOOL) - iFirst, 4, "ANIM_SM_PREVIEW_SET_BOOL");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_TRIGGER) - iFirst, 5, "ANIM_SM_PREVIEW_SET_TRIGGER");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_EXPECT_STATE) - iFirst, 6, "ANIM_SM_PREVIEW_EXPECT_STATE");
	ZENITH_ASSERT_EQ(static_cast<int>(Zenith_EditorActionType::SET_NAVMESH_ASSET) - static_cast<int>(Zenith_EditorActionType::ANIM_SM_PREVIEW_EXPECT_STATE), 1, "next boundary stays outside this range");
}

ZENITH_TEST(Automation, AnimSmSetPreviewEnabledPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmSetPreviewEnabled(true);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_ENABLED, "action type");
	ZENITH_ASSERT_EQ(xAction.m_bArg, true, "payload preserved");
}

ZENITH_TEST(Automation, AnimSmTickPreviewPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmTickPreview(1.25f);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_PREVIEW_TICK, "action type");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[0], 1.25f, 0.0f, "float preserved");
}

ZENITH_TEST(Automation, AnimSmSetPreviewFloatPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmSetPreviewFloat("owned value", 2.25f);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_FLOAT, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
	ZENITH_ASSERT_EQ_FLOAT(xAction.m_afArgs[0], 2.25f, 0.0f, "float preserved");
}

ZENITH_TEST(Automation, AnimSmSetPreviewIntPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmSetPreviewInt("owned value", 3);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_INT, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
	ZENITH_ASSERT_EQ(xAction.m_aiArgs[0], 3, "payload preserved");
}

ZENITH_TEST(Automation, AnimSmSetPreviewBoolPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmSetPreviewBool("owned value", true);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_BOOL, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
	ZENITH_ASSERT_EQ(xAction.m_bArg, true, "payload preserved");
}

ZENITH_TEST(Automation, AnimSmSetPreviewTriggerPacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmSetPreviewTrigger("owned value");
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_PREVIEW_SET_TRIGGER, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
}

ZENITH_TEST(Automation, AnimSmExpectPreviewStatePacksPayload)
{
	Zenith_EditorAutomation xAuto;
	xAuto.AddStep_AnimSmExpectPreviewState("owned value");
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step");
	const auto& xAction = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAction.m_eType == Zenith_EditorActionType::ANIM_SM_PREVIEW_EXPECT_STATE, "action type");
	ZENITH_ASSERT_STREQ(xAction.m_szArg1.c_str(), "owned value", "owned string");
}

ZENITH_TEST(Automation, AnimIkStepPacksItsPayload)
{
	// The queue is drained MUCH later than it is built, so every argument has to
	// survive as an OWNED copy in the action struct. This asserts the packing
	// contract ExecuteAnimIkAction reads back; the two halves are written from the
	// same comment block in the .cpp, and this is what stops them drifting.
	//
	// ★ AND IT ASSERTS THE FLOATS ARE BIT-IDENTICAL, not merely close, with a zero
	// tolerance. The whole claim this step makes is that the target reaches the
	// solver VERBATIM — no scale, no conversion, no arithmetic anywhere on the
	// path (see the header) — and a tolerance would be exactly the guard that
	// cannot see the one failure that matters.
	//
	// ★ ON ITS OWN INSTANCE, NOT THE ENGINE'S, unlike its older siblings above.
	// Packing a step touches nothing but the action vector, so a stack-local queue
	// asserts exactly the same thing without reaching for the engine singleton —
	// and without the "did somebody leave a step in the real queue" coupling that
	// makes the Reset() calls in those tests load-bearing.
	Zenith_EditorAutomation xAuto;

	xAuto.AddStep_AnimBakeIK(0.25f, -1.5f, 3.75f);
	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 1u, "one step queued");

	const Zenith_EditorAction& xBake = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xBake.m_eType == Zenith_EditorActionType::ANIM_IK_BAKE_TO_TARGET,
		"step 0 is ANIM_IK_BAKE_TO_TARGET");
	ZENITH_ASSERT_EQ_FLOAT(xBake.m_afArgs[0], 0.25f, 0.0f, "afArgs[0..2] is the MODEL-SPACE target, verbatim");
	ZENITH_ASSERT_EQ_FLOAT(xBake.m_afArgs[1], -1.5f, 0.0f, "afArgs[0..2] is the MODEL-SPACE target, verbatim");
	ZENITH_ASSERT_EQ_FLOAT(xBake.m_afArgs[2], 3.75f, 0.0f, "afArgs[0..2] is the MODEL-SPACE target, verbatim");

	// ★ NO BONE IS PACKED, AND THAT IS THE CONTRACT RATHER THAN AN OMISSION. The
	// chain is derived from the SELECTED bone, so a recipe addresses it with
	// AnimSelectBone — one address for one thing, exactly as the ANIM_POSE block
	// already works. A bone name here would be a second way to say it, and the two
	// would disagree the first time a recipe set only one of them.
	ZENITH_ASSERT_TRUE(xBake.m_szArg1.empty(), "the effector comes from the SELECTION, not from an argument");
}

ZENITH_TEST(Automation, AnimCurveStepsPackTheirPayloads)
{
	// The queue is drained MUCH later than it is built, so every argument has to
	// survive as an OWNED copy in the action struct. This asserts the packing
	// contract the executor reads back; the two halves are written from the same
	// comment block in the .cpp, and this is what stops them drifting.
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.Reset();

	xAuto.AddStep_AnimCurveSetView(true);
	xAuto.AddStep_AnimCurveSetKeyTangents("Hip", 0 /* translation */, 1,
		1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f);
	xAuto.AddStep_AnimCurveDragHandleToPixel("Hip", 1 /* rotation */, 2, 2 /* z */, true, 400.0f, 250.0f);
	xAuto.AddStep_AnimCurveExpectKeyTangent("Hip", 0, 1, false, 0.0f, 0.0f, 0.0f, 1.0e-3f);

	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 4u, "four steps queued");

	const Zenith_EditorAction& xView = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xView.m_eType == Zenith_EditorActionType::ANIM_CURVE_SET_VIEW,
		"step 0 is ANIM_CURVE_SET_VIEW");
	ZENITH_ASSERT_TRUE(xView.m_bArg, "and the toggle rides m_bArg");

	const Zenith_EditorAction& xTangents = xAuto.m_axActions.Get(1);
	ZENITH_ASSERT_STREQ(xTangents.m_szArg1.c_str(), "Hip", "the BONE name is OWNED by the action, not aliased");
	ZENITH_ASSERT_EQ(xTangents.m_aiArgs[0], 0, "aiArgs[0] carries the Flux_AnimTrack");
	ZENITH_ASSERT_EQ(xTangents.m_aiArgs[1], 1, "aiArgs[1] carries the KEY INDEX — resolved to an id at execution");
	ZENITH_ASSERT_EQ_FLOAT(xTangents.m_afArgs[2], 3.0f, 1e-6f, "afArgs[0..2] is the IN tangent");
	ZENITH_ASSERT_EQ_FLOAT(xTangents.m_afArgs[3], 4.0f, 1e-6f,
		"★ and afArgs[3..5] is the OUT one — six floats, because a key's tangent pair is what the "
		"document's verb takes and splitting it would make one gesture two undo steps");

	const Zenith_EditorAction& xDrag = xAuto.m_axActions.Get(2);
	ZENITH_ASSERT_EQ(xDrag.m_aiArgs[2], 2, "aiArgs[2] is the COMPONENT on a drag");
	ZENITH_ASSERT_TRUE(xDrag.m_bArg, "m_bArg is bIn — which END of the handle pair");
	ZENITH_ASSERT_EQ_FLOAT(xDrag.m_afArgs[0], 400.0f, 1e-6f, "afArgs[0..1] is the ABSOLUTE SCREEN pixel");
	ZENITH_ASSERT_EQ_FLOAT(xDrag.m_afArgs[1], 250.0f, 1e-6f, "afArgs[0..1] is the ABSOLUTE SCREEN pixel");

	const Zenith_EditorAction& xExpect = xAuto.m_axActions.Get(3);
	ZENITH_ASSERT_EQ_FLOAT(xExpect.m_afArgs[6], 1.0e-3f, 1e-9f,
		"afArgs[6] is the tolerance, clear of the six tangent slots");
	ZENITH_ASSERT_FALSE(xExpect.m_bArg, "and m_bArg picks the OUT tangent here");

	xAuto.Reset();
}

ZENITH_TEST(Automation, AnimTangentStepsPackTheirPayloads)
{
	// The queue is drained MUCH later than it is built, so every argument has to
	// survive as an OWNED copy in the action struct. This asserts the packing
	// contract the executor reads back; the two halves are written from the same
	// comment block in the .cpp, and this is what stops them drifting.
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.Reset();

	xAuto.AddStep_AnimTangentSetKeyMode("Hip", 0 /* translation */, 1,
		1 /* Out */, 1 /* Flat */);
	xAuto.AddStep_AnimTangentSetSelectionMode(2 /* Both */, 2 /* Auto */);
	xAuto.AddStep_AnimTangentExpectKeyMode("Hip", 0, 1, 0 /* In */, 3 /* Custom */);

	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 3u, "three steps queued");

	const Zenith_EditorAction& xKeyMode = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xKeyMode.m_eType == Zenith_EditorActionType::ANIM_TANGENT_SET_KEY_MODE,
		"step 0 is ANIM_TANGENT_SET_KEY_MODE");
	ZENITH_ASSERT_STREQ(xKeyMode.m_szArg1.c_str(), "Hip", "the BONE name is OWNED by the action, not aliased");
	ZENITH_ASSERT_EQ(xKeyMode.m_aiArgs[0], 0, "aiArgs[0] carries the Flux_AnimTrack");
	ZENITH_ASSERT_EQ(xKeyMode.m_aiArgs[1], 1, "aiArgs[1] carries the KEY INDEX — resolved to an id at execution");
	ZENITH_ASSERT_EQ(xKeyMode.m_aiArgs[2], 1,
		"★ aiArgs[2] carries the END, and it is an INT rather than the curve family's m_bArg because an "
		"end is THREE-valued here — In, Out and Both, which is the one-undo-step gesture a user performs");
	ZENITH_ASSERT_EQ(xKeyMode.m_aiArgs[3], 1,
		"★ and aiArgs[3] carries the MODE — the last free int on every step in the file, so the payload "
		"struct did not have to grow for this block");
	ZENITH_ASSERT_FALSE(xKeyMode.m_bArg, "m_bArg is untouched: this family never means bIn by it");

	const Zenith_EditorAction& xSelection = xAuto.m_axActions.Get(1);
	ZENITH_ASSERT_TRUE(xSelection.m_eType == Zenith_EditorActionType::ANIM_TANGENT_SET_SELECTION_MODE,
		"step 1 is ANIM_TANGENT_SET_SELECTION_MODE");
	ZENITH_ASSERT_STREQ(xSelection.m_szArg1.c_str(), "",
		"which names NO bone — the selection is the address, and a bone here would be a second one");
	ZENITH_ASSERT_EQ(xSelection.m_aiArgs[2], 2, "the END rides the SAME slot on every verb of the block");
	ZENITH_ASSERT_EQ(xSelection.m_aiArgs[3], 2, "and so does the MODE");

	const Zenith_EditorAction& xExpectMode = xAuto.m_axActions.Get(2);
	ZENITH_ASSERT_TRUE(xExpectMode.m_eType == Zenith_EditorActionType::ANIM_TANGENT_EXPECT_KEY_MODE,
		"step 2 is ANIM_TANGENT_EXPECT_KEY_MODE");
	ZENITH_ASSERT_EQ(xExpectMode.m_aiArgs[1], 1,
		"the assertion step addresses by INDEX like the mutating ones");
	ZENITH_ASSERT_EQ(xExpectMode.m_aiArgs[3], 3, "with the EXPECTED mode in the mode slot");

	xAuto.Reset();
}

ZENITH_TEST(Automation, AnimBlendStepsPackTheirPayloads)
{
	// The queue is drained MUCH later than it is built, so every argument has to
	// survive as an OWNED copy in the action struct. This asserts the packing
	// contract the executor reads back; the two halves are written from the same
	// comment block in the .cpp, and this is what stops them drifting.
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.Reset();

	xAuto.AddStep_AnimBlendSetTreeKind("Locomotion", 1 /* Blend Space 1D */);
	xAuto.AddStep_AnimBlendSetParameter("Locomotion", 0 /* X */, "Speed");
	xAuto.AddStep_AnimBlendAddPoint("Locomotion", "WalkClip", 0.0f, 0.0f);
	xAuto.AddStep_AnimBlendSetPointPosition("Locomotion", 1, 4.0f, -2.0f);
	xAuto.AddStep_AnimBlendSelectPoint(-1);
	xAuto.AddStep_AnimBlendExpectPointPosition("Locomotion", 1, 4.0f, -2.0f, 1.0e-3f);

	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 6u, "six steps queued");

	const Zenith_EditorAction& xKind = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xKind.m_eType == Zenith_EditorActionType::ANIM_BLEND_SET_TREE_KIND,
		"step 0 is ANIM_BLEND_SET_TREE_KIND");
	ZENITH_ASSERT_STREQ(xKind.m_szArg1.c_str(), "Locomotion", "the STATE name is OWNED by the action, not aliased");
	ZENITH_ASSERT_EQ(xKind.m_aiArgs[0], 1, "aiArgs[0] carries the tree kind as an offset from Single Clip");

	const Zenith_EditorAction& xParam = xAuto.m_axActions.Get(1);
	ZENITH_ASSERT_STREQ(xParam.m_szArg2.c_str(), "Speed", "szArg2 is the PARAMETER name");
	ZENITH_ASSERT_EQ(xParam.m_aiArgs[0], 0, "with the axis beside it");

	const Zenith_EditorAction& xAdd = xAuto.m_axActions.Get(2);
	ZENITH_ASSERT_STREQ(xAdd.m_szArg2.c_str(), "WalkClip",
		"★ szArg2 is the CLIP NAME on an add — a point plays a clip by name through the collection, "
		"never by path");

	const Zenith_EditorAction& xMove = xAuto.m_axActions.Get(3);
	ZENITH_ASSERT_EQ(xMove.m_aiArgs[0], 1, "MOVE names the point by INDEX — a blend point has no other handle");
	ZENITH_ASSERT_EQ_FLOAT(xMove.m_afArgs[0], 4.0f, 1e-6f, "afArgs[0] is the x");
	ZENITH_ASSERT_EQ_FLOAT(xMove.m_afArgs[1], -2.0f, 1e-6f,
		"★ and afArgs[1] is the y, carried even for a 1D space — one position shape for both kinds");

	const Zenith_EditorAction& xSelect = xAuto.m_axActions.Get(4);
	ZENITH_ASSERT_EQ(xSelect.m_aiArgs[0], -1, "★ -1 is the CLEAR, and it reaches the executor as a negative");

	const Zenith_EditorAction& xExpect = xAuto.m_axActions.Get(5);
	ZENITH_ASSERT_EQ_FLOAT(xExpect.m_afArgs[2], 1.0e-3f, 1e-9f,
		"afArgs[2] is the tolerance, in its own slot so the position keeps the first two");

	xAuto.Reset();
}

ZENITH_TEST(Automation, AnimLayerStepsPackTheirPayloads)
{
	// The queue is drained MUCH later than it is built, so every argument has to
	// survive as an OWNED copy in the action struct — a caller may legitimately
	// pass a pointer into a stack buffer built in a loop. This asserts the packing
	// contract the executor reads back; the two halves are written from the same
	// comment block in the .cpp, and this is what stops them drifting.
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.Reset();

	xAuto.AddStep_AnimLayerAdd("Aim");
	xAuto.AddStep_AnimLayerSetWeight(1, 0.25f);
	xAuto.AddStep_AnimLayerSetBlendMode(1, 1 /* additive */);
	xAuto.AddStep_AnimLayerSetEmitEvents(1, false);
	xAuto.AddStep_AnimLayerSetMaskPath(0, "game:Anim/UpperBody.zanimmask");
	xAuto.AddStep_AnimLayerMove(1, 0);
	xAuto.AddStep_AnimLayerSelect(1);
	xAuto.AddStep_AnimLayerExpectOrder(0, "Aim");

	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 8u, "eight steps queued");

	const Zenith_EditorAction& xAdd = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xAdd.m_eType == Zenith_EditorActionType::ANIM_LAYER_ADD, "step 0 is ANIM_LAYER_ADD");
	ZENITH_ASSERT_STREQ(xAdd.m_szArg1.c_str(), "Aim", "the layer name is OWNED by the action, not aliased");

	const Zenith_EditorAction& xWeight = xAuto.m_axActions.Get(1);
	ZENITH_ASSERT_EQ(xWeight.m_aiArgs[0], 1, "aiArgs[0] is the stable LAYER ID — never an index (D43)");
	ZENITH_ASSERT_EQ_FLOAT(xWeight.m_afArgs[0], 0.25f, 1e-6f, "afArgs[0] is the weight");

	const Zenith_EditorAction& xMode = xAuto.m_axActions.Get(2);
	ZENITH_ASSERT_EQ(xMode.m_aiArgs[1], 1, "aiArgs[1] carries the blend mode, so aiArgs[0] can stay the id");

	const Zenith_EditorAction& xEmit = xAuto.m_axActions.Get(3);
	ZENITH_ASSERT_FALSE(xEmit.m_bArg, "bArg carries D36's per-layer emit-events flag");

	const Zenith_EditorAction& xMask = xAuto.m_axActions.Get(4);
	ZENITH_ASSERT_STREQ(xMask.m_szArg1.c_str(), "game:Anim/UpperBody.zanimmask", "szArg1 is the mask ASSET PATH");
	ZENITH_ASSERT_EQ(xMask.m_aiArgs[0], 0, "with the layer id beside it");

	const Zenith_EditorAction& xMove = xAuto.m_axActions.Get(5);
	ZENITH_ASSERT_EQ(xMove.m_aiArgs[0], 1, "MOVE names the layer by id");
	ZENITH_ASSERT_EQ(xMove.m_aiArgs[1], 0,
		"★ and its DESTINATION by index — the one index in the family, because a blend-order "
		"position is what a reorder changes");

	const Zenith_EditorAction& xOrder = xAuto.m_axActions.Get(7);
	ZENITH_ASSERT_EQ(xOrder.m_aiArgs[0], 0,
		"★ EXPECT_ORDER's aiArgs[0] is an INDEX, not an id: it asserts about a POSITION");
	ZENITH_ASSERT_STREQ(xOrder.m_szArg1.c_str(), "Aim", "and names the layer it expects to find there");

	xAuto.Reset();
}

ZENITH_TEST(Automation, AnimMaskStepsPackTheirPayloads)
{
	// The queue is drained MUCH later than it is built, so every argument has to
	// survive as an OWNED copy in the action struct — a caller may legitimately
	// pass a pointer into a stack buffer built in a loop. This asserts the packing
	// contract the executor reads back; the two halves are written from the same
	// comment block in the .cpp, and this is what stops them drifting.
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.Reset();

	xAuto.AddStep_AnimMaskOpenFresh("game:Anim/UpperBody.zanimmask");
	xAuto.AddStep_AnimMaskSetWeight("Spine", 0.75f);
	xAuto.AddStep_AnimMaskSetSubtree("Spine", 1.0f);
	xAuto.AddStep_AnimMaskSetHasAvatar(false);
	xAuto.AddStep_AnimMaskExpectWeight("Head", 0.5f, 1.0e-3f);
	xAuto.AddStep_AnimMaskSave();

	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 6u, "six steps queued");

	const Zenith_EditorAction& xOpen = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xOpen.m_eType == Zenith_EditorActionType::ANIM_MASK_OPEN_FRESH, "step 0 is ANIM_MASK_OPEN_FRESH");
	ZENITH_ASSERT_STREQ(xOpen.m_szArg1.c_str(), "game:Anim/UpperBody.zanimmask",
		"the asset path is OWNED by the action, not aliased");

	const Zenith_EditorAction& xWeight = xAuto.m_axActions.Get(1);
	ZENITH_ASSERT_STREQ(xWeight.m_szArg1.c_str(), "Spine", "szArg1 is the BONE NAME — never an index (D46)");
	ZENITH_ASSERT_EQ_FLOAT(xWeight.m_afArgs[0], 0.75f, 1e-6f, "afArgs[0] is the weight");

	const Zenith_EditorAction& xSubtree = xAuto.m_axActions.Get(2);
	ZENITH_ASSERT_TRUE(xSubtree.m_eType == Zenith_EditorActionType::ANIM_MASK_SET_SUBTREE, "step 2 is the subtree verb");
	ZENITH_ASSERT_STREQ(xSubtree.m_szArg1.c_str(), "Spine", "addressed by name too");
	ZENITH_ASSERT_EQ_FLOAT(xSubtree.m_afArgs[0], 1.0f, 1e-6f, "with its own weight");

	const Zenith_EditorAction& xFlag = xAuto.m_axActions.Get(3);
	ZENITH_ASSERT_FALSE(xFlag.m_bArg, "bArg carries D47's has-avatar-mask flag");

	const Zenith_EditorAction& xExpect = xAuto.m_axActions.Get(4);
	ZENITH_ASSERT_STREQ(xExpect.m_szArg1.c_str(), "Head", "the assertion step names its bone");
	ZENITH_ASSERT_EQ_FLOAT(xExpect.m_afArgs[0], 0.5f, 1e-6f, "afArgs[0] is the expected weight");
	ZENITH_ASSERT_EQ_FLOAT(xExpect.m_afArgs[1], 1.0e-3f, 1e-9f, "and afArgs[1] its tolerance");

	const Zenith_EditorAction& xSave = xAuto.m_axActions.Get(5);
	ZENITH_ASSERT_TRUE(xSave.m_eType == Zenith_EditorActionType::ANIM_MASK_SAVE, "step 5 is ANIM_MASK_SAVE");

	xAuto.Reset();
}

ZENITH_TEST(Automation, AnimSmStepsPackTheirPayloads)
{
	// The queue is drained MUCH later than it is built, so every argument has to
	// survive as an OWNED copy in the action struct — a caller may legitimately
	// pass a pointer into a stack buffer built in a loop. This asserts the
	// packing contract the executor reads back; the two halves are written from
	// the same comment block in the .cpp, and this is what stops them drifting.
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.Reset();

	xAuto.AddStep_AnimSmOpenFresh("game:Anim/Probe.zanimctrl");
	xAuto.AddStep_AnimSmAddParameter("Speed", 0 /* Float */, 0.25f);
	xAuto.AddStep_AnimSmSetStateClip("Idle", "IdleClip");
	xAuto.AddStep_AnimSmAddTransition("Idle", "Walk");
	xAuto.AddStep_AnimSmAddCondition("Idle", 3, "Speed", 2 /* Greater */, 0.1f);
	xAuto.AddStep_AnimSmSetTransitionExitTime("Idle", 1, true, 0.8f);
	xAuto.AddStep_AnimSmSelectLayer(-1);
	xAuto.AddStep_AnimSmExpectStateCount(2);

	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 8u, "eight steps queued");

	const Zenith_EditorAction& xOpen = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xOpen.m_eType == Zenith_EditorActionType::ANIM_SM_OPEN_FRESH, "step 0 is ANIM_SM_OPEN_FRESH");
	ZENITH_ASSERT_STREQ(xOpen.m_szArg1.c_str(), "game:Anim/Probe.zanimctrl",
		"the asset path is OWNED by the action, not aliased");

	const Zenith_EditorAction& xParam = xAuto.m_axActions.Get(1);
	ZENITH_ASSERT_EQ(xParam.m_aiArgs[0], 0, "the parameter TYPE rides aiArgs[0]");
	ZENITH_ASSERT_EQ_FLOAT(xParam.m_afArgs[0], 0.25f, 1e-6f, "the parameter DEFAULT rides afArgs[0]");

	const Zenith_EditorAction& xClip = xAuto.m_axActions.Get(2);
	ZENITH_ASSERT_STREQ(xClip.m_szArg1.c_str(), "Idle", "szArg1 is the state name");
	ZENITH_ASSERT_STREQ(xClip.m_szArg2.c_str(), "IdleClip", "szArg2 is the clip NAME, not a path");

	const Zenith_EditorAction& xTrans = xAuto.m_axActions.Get(3);
	ZENITH_ASSERT_STREQ(xTrans.m_szArg2.c_str(), "Walk", "szArg2 is the transition TARGET");

	const Zenith_EditorAction& xCond = xAuto.m_axActions.Get(4);
	ZENITH_ASSERT_EQ(xCond.m_aiArgs[0], 3, "aiArgs[0] is the transition index");
	ZENITH_ASSERT_EQ(xCond.m_aiArgs[1], 2, "aiArgs[1] is the compare op");
	ZENITH_ASSERT_STREQ(xCond.m_szArg2.c_str(), "Speed", "szArg2 is the condition's parameter");
	ZENITH_ASSERT_EQ_FLOAT(xCond.m_afArgs[0], 0.1f, 1e-6f, "afArgs[0] is the threshold");

	const Zenith_EditorAction& xExit = xAuto.m_axActions.Get(5);
	ZENITH_ASSERT_TRUE(xExit.m_bArg, "bArg carries has-exit-time");
	ZENITH_ASSERT_EQ_FLOAT(xExit.m_afArgs[0], 0.8f, 1e-6f, "afArgs[0] carries the normalized exit time");

	const Zenith_EditorAction& xLayer = xAuto.m_axActions.Get(6);
	ZENITH_ASSERT_EQ(xLayer.m_aiArgs[0], -1,
		"a NEGATIVE layer id is how a recipe names the TOP-LEVEL machine, which has no id to type");

	xAuto.Reset();
}

ZENITH_TEST(Automation, AnimStepsPackTheirPayloads)
{
	// The queue is drained MUCH later than it is built, so every argument has to
	// survive as an owned copy in the action struct. This asserts the packing
	// contract the executor reads back — the two halves are written from the same
	// comment block in the .cpp, and this is what stops them drifting.
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.Reset();

	xAuto.AddStep_AnimOpenClip("game:Animations/Probe.zanim");
	xAuto.AddStep_AnimSelectKey("Hip", FLUX_ANIM_TRACK_ROTATION, 2, ZENITH_ANIMSELECT_ADD);
	xAuto.AddStep_AnimBoxSelect(10.0f, 20.0f, 30.0f, 40.0f, ZENITH_ANIMSELECT_TOGGLE);
	xAuto.AddStep_AnimMoveSelection(0.25f, true);
	xAuto.AddStep_AnimPasteToBone("Spine", 0.5f);
	xAuto.AddStep_AnimExpectKeyTime("Hip", FLUX_ANIM_TRACK_SCALE, 3, 1.25f, 0.002f);
	xAuto.AddStep_AnimExpectSelectedCount(4);

	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 7u, "seven steps queued");

	const Zenith_EditorAction& xOpen = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xOpen.m_eType == Zenith_EditorActionType::ANIM_OPEN_CLIP, "step 0 is ANIM_OPEN_CLIP");
	ZENITH_ASSERT_STREQ(xOpen.m_szArg1.c_str(), "game:Animations/Probe.zanim",
		"the asset path is OWNED by the action, not aliased");

	const Zenith_EditorAction& xSelect = xAuto.m_axActions.Get(1);
	ZENITH_ASSERT_TRUE(xSelect.m_eType == Zenith_EditorActionType::ANIM_SELECT_KEY, "step 1 is ANIM_SELECT_KEY");
	ZENITH_ASSERT_STREQ(xSelect.m_szArg1.c_str(), "Hip", "szArg1 carries the bone name");
	ZENITH_ASSERT_EQ(xSelect.m_aiArgs[0], static_cast<int>(FLUX_ANIM_TRACK_ROTATION), "aiArgs[0] carries the track");
	ZENITH_ASSERT_EQ(xSelect.m_aiArgs[1], 2, "aiArgs[1] carries the KEY INDEX (resolved to an id at execution)");
	ZENITH_ASSERT_EQ(xSelect.m_aiArgs[2], static_cast<int>(ZENITH_ANIMSELECT_ADD), "aiArgs[2] carries the select mode");

	const Zenith_EditorAction& xBox = xAuto.m_axActions.Get(2);
	ZENITH_ASSERT_EQ_FLOAT(xBox.m_afArgs[0], 10.0f, 1.0e-5f, "afArgs[0..3] carry the screen rectangle");
	ZENITH_ASSERT_EQ_FLOAT(xBox.m_afArgs[3], 40.0f, 1.0e-5f, "afArgs[0..3] carry the screen rectangle");
	ZENITH_ASSERT_EQ(xBox.m_aiArgs[2], static_cast<int>(ZENITH_ANIMSELECT_TOGGLE),
		"the select mode rides aiArgs[2] on EVERY select verb, box included");

	const Zenith_EditorAction& xMove = xAuto.m_axActions.Get(3);
	ZENITH_ASSERT_EQ_FLOAT(xMove.m_afArgs[0], 0.25f, 1.0e-5f, "afArgs[0] carries the delta in SECONDS");
	ZENITH_ASSERT_TRUE(xMove.m_bArg, "bArg carries the snap flag");

	const Zenith_EditorAction& xPaste = xAuto.m_axActions.Get(4);
	ZENITH_ASSERT_STREQ(xPaste.m_szArg1.c_str(), "Spine", "the paste TARGET bone is owned too");
	ZENITH_ASSERT_EQ_FLOAT(xPaste.m_afArgs[0], 0.5f, 1.0e-5f, "afArgs[0] carries the paste offset");

	const Zenith_EditorAction& xExpectTime = xAuto.m_axActions.Get(5);
	ZENITH_ASSERT_EQ(xExpectTime.m_aiArgs[1], 3, "the assertion step addresses by index like the mutating ones");
	ZENITH_ASSERT_EQ_FLOAT(xExpectTime.m_afArgs[0], 1.25f, 1.0e-5f, "afArgs[0] is the EXPECTED time");
	ZENITH_ASSERT_EQ_FLOAT(xExpectTime.m_afArgs[1], 0.002f, 1.0e-6f, "afArgs[1] is the TOLERANCE");

	ZENITH_ASSERT_EQ(xAuto.m_axActions.Get(6).m_aiArgs[0], 4, "the expected selection count rides aiArgs[0]");

	xAuto.Reset();
}

namespace
{
	// One bone, three position keys at 0 / 1 / 2 s, duration 2 s, 30 fps grid —
	// the same shape Zenith_EditorPanel_Animation.Tests.inl's probe uses, so a
	// failure here can be compared against the panel's own units directly.
	void AutomationWriteAnimProbe(const std::string& strPath)
	{
		Flux_AnimationClip xClip;
		xClip.SetName("AutomationAnimProbe");
		xClip.SetDuration(2.0f);
		xClip.GetMetadata().m_bGenerated = false;
		xClip.GetMetadata().m_uAuthoredFrameRate = 30u;

		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xHip.AddPositionKeyframe(1.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xHip.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f));
		xHip.SortKeyframes();
		xClip.AddBoneChannel("Hip", std::move(xHip));

		xClip.Export(strPath);
	}
}

ZENITH_TEST(Automation, AnimAuthoringStepsDriveTheDopeSheet)
{
	// ★ THE ONE THAT PROVES THE ROUTE EXISTS. Everything else about these verbs
	// is checkable by reading the queue; this is the only unit that shows a
	// QUEUED step reaching Zenith_EditorPanel_Animation and moving a key — the
	// enum value, the router range, the executor case and the panel call all in
	// one line of evidence. No ImGui frame is needed: every Action_* used here is
	// pure document work (the rect-dependent ones, box select in particular, need
	// a rendered frame and are covered by the RenderTest live test instead).
	Flux_PreviewSlotArbiter::ResetForTesting();

	std::error_code xError;
	std::filesystem::path xRoot = std::filesystem::temp_directory_path(xError);
	if (xError)
	{
		xRoot = ".";
	}
	const std::filesystem::path xDirectory = xRoot / "zenith_automation_anim";
	std::filesystem::remove_all(xDirectory, xError);
	std::filesystem::create_directories(xDirectory, xError);
	const std::string strPath = (xDirectory / "steps.zanim").generic_string();
	AutomationWriteAnimProbe(strPath);

	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	Zenith_EditorPanel_Animation& xPanel = Zenith_EditorPanel_Animation::Instance();
	xAuto.Reset();

	xAuto.AddStep_AnimOpenClip(strPath.c_str());
	xAuto.AddStep_AnimSelectKey("Hip", FLUX_ANIM_TRACK_POSITION, 1, ZENITH_ANIMSELECT_REPLACE);
	xAuto.AddStep_AnimExpectSelectedCount(1);
	xAuto.AddStep_AnimMoveSelection(0.5f, true);
	xAuto.AddStep_AnimExpectKeyTime("Hip", FLUX_ANIM_TRACK_POSITION, 1, 1.5f, 0.001f);
	xAuto.AddStep_AnimUndo();
	xAuto.AddStep_AnimExpectKeyTime("Hip", FLUX_ANIM_TRACK_POSITION, 1, 1.0f, 0.001f);
	xAuto.AddStep_AnimCloseClip();
	xAuto.Begin();

	const Zenith_AnimTrackId xTrack = Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION);

	xAuto.ExecuteNextStep();	// open
	ZENITH_ASSERT_TRUE(xPanel.IsOpen(), "AnimOpenClip opened the probe into the dope sheet");
	ZENITH_ASSERT_TRUE(xPanel.IsShown(), "AnimOpenClip SHOWS the window — a hidden sheet records no rects");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 3u, "the probe's three Hip position keys are there");

	// Captured BEFORE the move, because this is the whole point of the index
	// indirection: the step names index 1 and the executor must turn that into
	// THIS id, which then survives the move and the undo.
	const u_int uMiddleKeyId = xPanel.Document().GetKeyIdAtIndex(xTrack, 1u);
	ZENITH_ASSERT_NE(uMiddleKeyId, uINVALID_ANIM_KEY_ID, "the middle key has a stable id");

	xAuto.ExecuteNextStep();	// select key by INDEX
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 1u, "the select step selected exactly one key");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uMiddleKeyId),
		"index 1 resolved to the STABLE ID of the middle key, which is what the panel takes");

	xAuto.ExecuteNextStep();	// expect-selected-count
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 1u, "an assertion step mutates nothing");

	xAuto.ExecuteNextStep();	// move +0.5 s, snapped
	float fTime = 0.0f;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uMiddleKeyId, fTime), "the moved key still resolves");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.5f, 0.001f, "AnimMoveSelection moved the selected key by half a second");

	xAuto.ExecuteNextStep();	// expect-key-time (1.5)
	xAuto.ExecuteNextStep();	// undo
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xTrack, uMiddleKeyId, fTime),
		"the undo re-inserted the key under its ORIGINAL id");
	ZENITH_ASSERT_EQ_FLOAT(fTime, 1.0f, 0.001f, "AnimUndo put the key back where it started");

	xAuto.ExecuteNextStep();	// expect-key-time (1.0)
	xAuto.ExecuteNextStep();	// close
	ZENITH_ASSERT_FALSE(xPanel.IsOpen(), "AnimCloseClip closed the document");

	// ★ A LEAKED ASSET REFERENCE FAILS HERE, ON EVERY RUN. Closing drops the
	// document's owning handle and the session's, so UnloadUnused must be able to
	// free the probe. Without this the only symptom of a retained reference was at
	// ATEXIT — the fixture's ForceUnload below deletes the asset whatever its
	// refcount is, so the leftover handle was left dangling and its destructor wrote
	// into freed memory, asserting "Release called on asset with 0 ref count" only
	// when that word happened to read zero (roughly one run in eight) and corrupting
	// the heap silently otherwise. A refcount assertion at the point of the leak is
	// the difference between a named failure and an intermittent one.
	Zenith_AssetRegistry::UnloadUnused();
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strPath),
		"nothing still holds a reference to the clip asset after AnimCloseClip");

	// ★ CloseClip does NOT hide the window, and AnimOpenClip deliberately shows
	// it — so this unit drives the EDITOR'S single panel into a visible state and
	// has to put it back. Left set, every game would boot with the dope sheet
	// open over its viewport, caused by a unit test.
	xPanel.ShowFlag() = false;

	xAuto.Reset();
	Zenith_AssetRegistry::ForceUnload(strPath);
	std::filesystem::remove_all(xDirectory, xError);
	Flux_PreviewSlotArbiter::ResetForTesting();
}

//=============================================================================
// Tangent-MODE authoring steps (B3)
//=============================================================================

ZENITH_TEST(Automation, AnimTangentStepsRoundTripThroughTheDispatcher)
{
	// ★ THE ONE THAT PROVES THE EIGHTH ROUTE EXISTS. The contiguity and packing
	// units above read the QUEUE; this is the only one that shows a queued
	// ANIM_TANGENT_* step reaching Zenith_EditorPanel_Animation and changing a
	// stored mode — the enum value, the router range, the executor case and the
	// panel call in one line of evidence. No ImGui frame is needed: every Action_*
	// on this path is pure document work.
	//
	// ★★ AND EVERY EXPECT STEP HAS A DIRECT READ-BACK BESIDE IT. An expect step's
	// failure path is Zenith_Assert -> Zenith_DebugBreak, which does NOT fail a
	// ZENITH_TEST — so an expectation ALONE is fail-open, and a route that quietly
	// asserted nothing would look exactly like this test passing.
	Flux_PreviewSlotArbiter::ResetForTesting();

	std::error_code xError;
	std::filesystem::path xRoot = std::filesystem::temp_directory_path(xError);
	if (xError)
	{
		xRoot = ".";
	}
	const std::filesystem::path xDirectory = xRoot / "zenith_automation_animtangent";
	std::filesystem::remove_all(xDirectory, xError);
	std::filesystem::create_directories(xDirectory, xError);
	const std::string strPath = (xDirectory / "modes.zanim").generic_string();
	AutomationWriteAnimProbe(strPath);

	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	Zenith_EditorPanel_Animation& xPanel = Zenith_EditorPanel_Animation::Instance();
	xAuto.Reset();

	xAuto.AddStep_AnimOpenClip(strPath.c_str());
	xAuto.AddStep_AnimTangentSetKeyMode("Hip", FLUX_ANIM_TRACK_POSITION, 1,
		1 /* Out */, 1 /* Flat */);
	xAuto.AddStep_AnimTangentExpectKeyMode("Hip", FLUX_ANIM_TRACK_POSITION, 1, 1 /* Out */, 1 /* Flat */);
	xAuto.AddStep_AnimSelectKey("Hip", FLUX_ANIM_TRACK_POSITION, 0, ZENITH_ANIMSELECT_REPLACE);
	xAuto.AddStep_AnimTangentSetSelectionMode(2 /* Both */, 2 /* Auto */);
	xAuto.AddStep_AnimTangentExpectKeyMode("Hip", FLUX_ANIM_TRACK_POSITION, 0, 2 /* Both */, 2 /* Auto */);
	xAuto.AddStep_AnimCloseClip();
	xAuto.Begin();

	const Zenith_AnimTrackId xTrack = Zenith_AnimTrackId::Bone("Hip", FLUX_ANIM_TRACK_POSITION);

	xAuto.ExecuteNextStep();	// open
	ZENITH_ASSERT_TRUE(xPanel.IsOpen(), "AnimOpenClip opened the probe");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xTrack), 3u, "with its three Hip position keys");

	const u_int uFirstKeyId = xPanel.Document().GetKeyIdAtIndex(xTrack, 0u);
	const u_int uMiddleKeyId = xPanel.Document().GetKeyIdAtIndex(xTrack, 1u);
	ZENITH_ASSERT_NE(uMiddleKeyId, uINVALID_ANIM_KEY_ID, "the middle key has a stable id");

	Flux_TangentMode eMode = Flux_TangentMode::CUSTOM;
	ZENITH_ASSERT_TRUE(xPanel.GetKeyTangentMode(xTrack, uMiddleKeyId, ZENITH_ANIM_TANGENT_END_BOTH, eMode),
		"the middle key's mode reads back before anything is authored");
	ZENITH_ASSERT_TRUE(eMode == Flux_TangentMode::LINEAR, "as LINEAR, which is what the file carries");

	xAuto.ExecuteNextStep();	// set the OUT end FLAT, by key INDEX
	ZENITH_ASSERT_TRUE(xPanel.GetKeyTangentMode(xTrack, uMiddleKeyId, ZENITH_ANIM_TANGENT_END_OUT, eMode),
		"the OUT end reads back");
	ZENITH_ASSERT_TRUE(eMode == Flux_TangentMode::FLAT,
		"★ FLAT — a queued step reached the panel's mode verb, and index 1 resolved to the STABLE id");
	ZENITH_ASSERT_TRUE(xPanel.GetKeyTangentMode(xTrack, uMiddleKeyId, ZENITH_ANIM_TANGENT_END_IN, eMode),
		"the IN end reads back too");
	ZENITH_ASSERT_TRUE(eMode == Flux_TangentMode::LINEAR,
		"★ still LINEAR: the step named ONE end and the other was not touched, all the way through the "
		"dispatcher");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "as exactly one undo entry");

	xAuto.ExecuteNextStep();	// expect (Out, Flat) — asserted directly above, because an expect is fail-open
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "an assertion step mutates nothing");

	xAuto.ExecuteNextStep();	// select the first key
	ZENITH_ASSERT_EQ(xPanel.GetSelectedKeyCount(), 1u, "one key selected for the selection verb");
	ZENITH_ASSERT_TRUE(xPanel.IsKeySelected(xTrack, uFirstKeyId), "and it is the first one");

	xAuto.ExecuteNextStep();	// AUTO on both ends of the selection
	ZENITH_ASSERT_TRUE(xPanel.GetKeyTangentMode(xTrack, uFirstKeyId, ZENITH_ANIM_TANGENT_END_BOTH, eMode),
		"the selected key answers for BOTH ends");
	ZENITH_ASSERT_TRUE(eMode == Flux_TangentMode::AUTO,
		"★ AUTO on both — the selection verb went through the panel's compound and stored the MODE, not "
		"merely a vector");
	ZENITH_ASSERT_TRUE(xPanel.GetKeyTangentMode(xTrack, uMiddleKeyId, ZENITH_ANIM_TANGENT_END_OUT, eMode),
		"and the UNSELECTED middle key reads back");
	ZENITH_ASSERT_TRUE(eMode == Flux_TangentMode::FLAT,
		"★ with its FLAT intact — a selection verb touches the selection and nothing else");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 2u, "and it is ONE more undo entry, being one compound");

	xAuto.ExecuteNextStep();	// expect (Both, Auto)
	xAuto.ExecuteNextStep();	// close
	ZENITH_ASSERT_FALSE(xPanel.IsOpen(), "AnimCloseClip closed the document");

	// A leaked asset reference fails HERE, on every run, rather than at atexit —
	// see AnimAuthoringStepsDriveTheDopeSheet for the full account.
	Zenith_AssetRegistry::UnloadUnused();
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strPath),
		"nothing still holds a reference to the clip asset after AnimCloseClip");

	// AnimOpenClip SHOWS the editor's single panel, so this unit has to put it back.
	xPanel.ShowFlag() = false;

	xAuto.Reset();
	Zenith_AssetRegistry::ForceUnload(strPath);
	std::filesystem::remove_all(xDirectory, xError);
	Flux_PreviewSlotArbiter::ResetForTesting();
}

//=============================================================================
// Animation POSE authoring steps (WU-4.3)
//=============================================================================

namespace
{
	// A clip whose metadata names a REAL rig, so the pose verbs have a bone to
	// select. Two bones (Hip -> Spine, 0.5 m apart) and a three-vertex mesh that
	// exists only to be a resolvable preview PATH — the same shape
	// Zenith_EditorPanel_Animation.Tests.inl's rigged probe uses, and device-free
	// for the same reason (GenerateUnitCube would end in EnsureGPUBuffers).
	//
	// ★ ONLY HIP'S POSITION IS ANIMATED, deliberately. Spine's ROTATION channel
	// is therefore empty, which is what makes the recipe below exercise §5.1's
	// first-key case: writing it CREATES the channel.
	void AutomationWriteRiggedAnimProbe(const std::string& strClipPath,
		const std::string& strSkeletonPath, const std::string& strMeshPath)
	{
		{
			Zenith_SkeletonAsset xSkeleton;
			const Zenith_Maths::Quat xIdentity(1.0f, 0.0f, 0.0f, 0.0f);
			const Zenith_Maths::Vector3 xUnitScale(1.0f);
			xSkeleton.AddBone("Hip", -1, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), xIdentity, xUnitScale);
			xSkeleton.AddBone("Spine", 0, Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f), xIdentity, xUnitScale);
			xSkeleton.ComputeBindPoseMatrices();
			xSkeleton.Export(strSkeletonPath.c_str());
		}
		{
			Zenith_MeshAsset xMesh;
			xMesh.Reserve(3, 3);
			xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_Maths::Vector2(0.0f, 0.0f));
			xMesh.AddVertex(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_Maths::Vector2(1.0f, 0.0f));
			xMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_Maths::Vector2(0.0f, 1.0f));
			xMesh.AddTriangle(0u, 1u, 2u);
			xMesh.AddSubmesh(0u, 3u, 0u);
			xMesh.ComputeBounds();
			xMesh.Export(strMeshPath.c_str());
		}

		Flux_AnimationClip xClip;
		xClip.SetName("AutomationPoseProbe");
		xClip.SetDuration(2.0f);
		xClip.SetLooping(false);
		xClip.GetMetadata().m_bGenerated = false;
		xClip.GetMetadata().m_uAuthoredFrameRate = 30u;
		xClip.GetMetadata().m_strSkeletonPath = strSkeletonPath;
		xClip.GetMetadata().m_strPreviewModelPath = strMeshPath;

		Flux_BoneChannel xHip;
		xHip.AddPositionKeyframe(0.0f, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xHip.AddPositionKeyframe(2.0f, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		xHip.SortKeyframes();
		xClip.AddBoneChannel("Hip", std::move(xHip));

		xClip.Export(strClipPath);
	}
}

ZENITH_TEST(Automation, AnimPoseStepsPackTheirPayloads)
{
	// The queue is drained much later than it is built, so the packing contract
	// the executor reads back is asserted here — the two halves are written from
	// the same comment block in the .cpp and this is what stops them drifting.
	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	xAuto.Reset();

	xAuto.AddStep_AnimSelectBone(1);
	xAuto.AddStep_AnimRotateSelectedBoneWorld(0.0f, 1.0f, 0.0f, 30.0f);
	xAuto.AddStep_AnimSetAutoKey(true);
	xAuto.AddStep_AnimSetKeyForSelectedBone();
	xAuto.AddStep_AnimExpectBoneLocalRotation(1, 0.25f, 0.5f, 0.75f, 1.0f, 0.002f);

	ZENITH_ASSERT_EQ(xAuto.m_axActions.GetSize(), 5u, "five pose steps queued");

	const Zenith_EditorAction& xSelect = xAuto.m_axActions.Get(0);
	ZENITH_ASSERT_TRUE(xSelect.m_eType == Zenith_EditorActionType::ANIM_POSE_SELECT_BONE,
		"step 0 is ANIM_POSE_SELECT_BONE");
	ZENITH_ASSERT_EQ(xSelect.m_aiArgs[0], 1, "aiArgs[0] carries the BONE INDEX (not a name — see the .cpp)");

	const Zenith_EditorAction& xRotate = xAuto.m_axActions.Get(1);
	ZENITH_ASSERT_EQ_FLOAT(xRotate.m_afArgs[0], 0.0f, 1.0e-6f, "afArgs[0..2] carry the axis");
	ZENITH_ASSERT_EQ_FLOAT(xRotate.m_afArgs[1], 1.0f, 1.0e-6f, "afArgs[0..2] carry the axis");
	ZENITH_ASSERT_EQ_FLOAT(xRotate.m_afArgs[2], 0.0f, 1.0e-6f, "afArgs[0..2] carry the axis");
	ZENITH_ASSERT_EQ_FLOAT(xRotate.m_afArgs[3], 30.0f, 1.0e-5f, "afArgs[3] carries the angle in DEGREES");

	ZENITH_ASSERT_TRUE(xAuto.m_axActions.Get(2).m_bArg, "bArg carries the auto-key flag");
	ZENITH_ASSERT_TRUE(xAuto.m_axActions.Get(3).m_eType == Zenith_EditorActionType::ANIM_POSE_SET_KEY_FOR_SELECTED_BONE,
		"step 3 is ANIM_POSE_SET_KEY_FOR_SELECTED_BONE");

	const Zenith_EditorAction& xExpect = xAuto.m_axActions.Get(4);
	ZENITH_ASSERT_EQ(xExpect.m_aiArgs[0], 1, "the assertion step addresses a bone by index too");
	ZENITH_ASSERT_EQ_FLOAT(xExpect.m_afArgs[0], 0.25f, 1.0e-6f, "afArgs[0..3] are the quaternion in SERIALIZED (x,y,z,w) order");
	ZENITH_ASSERT_EQ_FLOAT(xExpect.m_afArgs[3], 1.0f, 1.0e-6f, "afArgs[0..3] are the quaternion in SERIALIZED (x,y,z,w) order");
	ZENITH_ASSERT_EQ_FLOAT(xExpect.m_afArgs[4], 0.002f, 1.0e-7f, "afArgs[4] is the TOLERANCE");

	xAuto.Reset();
}

ZENITH_TEST(Automation, AnimPoseAuthoringStepsDriveTheDopeSheet)
{
	// ★ THE ONE THAT PROVES THE POSE ROUTE EXISTS, end to end: the enum value,
	// the second router range, ExecuteAnimationPoseAction's case, the panel's
	// Action_* and the document's channel, in one line of evidence. No ImGui
	// frame is needed — none of the verbs on this path reads a rect (the pointer
	// drag does, and is covered by the panel's own units).
	Flux_PreviewSlotArbiter::ResetForTesting();

	std::error_code xError;
	std::filesystem::path xRoot = std::filesystem::temp_directory_path(xError);
	if (xError)
	{
		xRoot = ".";
	}
	const std::filesystem::path xDirectory = xRoot / "zenith_automation_animpose";
	std::filesystem::remove_all(xDirectory, xError);
	std::filesystem::create_directories(xDirectory, xError);
	const std::string strClipPath = (xDirectory / "pose.zanim").generic_string();
	const std::string strSkeletonPath = (xDirectory / "pose.zskel").generic_string();
	const std::string strMeshPath = (xDirectory / "pose.zasset").generic_string();
	AutomationWriteRiggedAnimProbe(strClipPath, strSkeletonPath, strMeshPath);

	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();
	Zenith_EditorPanel_Animation& xPanel = Zenith_EditorPanel_Animation::Instance();
	xAuto.Reset();

	// A 30 degree turn about world Y. Spine's PARENT is the root Hip, whose world
	// rotation is identity, and Spine's own local rotation starts at the bind
	// identity — so the conjugation collapses and the expected local rotation is
	// the delta itself: (x, y, z, w) = (0, sin 15, 0, cos 15).
	const float fSIN_15 = 0.25881904510252074f;
	const float fCOS_15 = 0.96592582628906831f;

	xAuto.AddStep_AnimOpenClip(strClipPath.c_str());
	xAuto.AddStep_AnimSelectBone(1);
	xAuto.AddStep_AnimRotateSelectedBoneWorld(0.0f, 1.0f, 0.0f, 30.0f);
	xAuto.AddStep_AnimExpectBoneLocalRotation(1, 0.0f, fSIN_15, 0.0f, fCOS_15, 1.0e-4f);
	xAuto.AddStep_AnimSetKeyForSelectedBone();
	xAuto.AddStep_AnimCloseClip();
	xAuto.Begin();

	const Zenith_AnimTrackId xSpineRotation = Zenith_AnimTrackId::Bone("Spine", FLUX_ANIM_TRACK_ROTATION);

	xAuto.ExecuteNextStep();	// open
	ZENITH_ASSERT_TRUE(xPanel.IsOpen(), "AnimOpenClip opened the rigged probe");
	ZENITH_ASSERT_FALSE(xPanel.Session().NeedsRigSelection(),
		"whose rig resolved — else there is no bone to pose and every assertion below is vacuous");
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xSpineRotation), 0u,
		"and Spine's rotation channel starts EMPTY, so the key below is a first-key case (§5.1)");

	xAuto.ExecuteNextStep();	// select bone 1
	ZENITH_ASSERT_TRUE(xPanel.Session().HasBoneSelection(), "AnimSelectBone selected a bone");
	ZENITH_ASSERT_EQ(xPanel.Session().GetSelectedBoneIndex(), 1u, "the one the step named");

	xAuto.ExecuteNextStep();	// rotate about world Y
	ZENITH_ASSERT_TRUE(xPanel.Session().HasUnkeyedPose(),
		"★ the rotation is a LIVE POSE and nothing else yet — it is not in the clip and the panel says so");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 0u,
		"★ and a live pose is NOT undoable: an undo entry restoring a pose the document never held would be a lie");

	xAuto.ExecuteNextStep();	// expect-bone-local-rotation (the assertion step)
	ZENITH_ASSERT_TRUE(xPanel.Session().HasUnkeyedPose(), "an assertion step mutates nothing");

	xAuto.ExecuteNextStep();	// set key
	ZENITH_ASSERT_EQ(xPanel.Document().GetKeyCount(xSpineRotation), 1u,
		"AnimSetKeyForSelectedBone created the channel and put ONE key in it");
	ZENITH_ASSERT_EQ(xPanel.Document().GetUndoStackSize(), 1u, "as ONE undo step");
	ZENITH_ASSERT_FALSE(xPanel.Session().HasUnkeyedPose(), "and the pose is in the clip now, so the badge goes");

	// The key landed at the play head, snapped to the 30 fps grid — which is 0
	// here, because opening a clip parks the play head at 0.
	float fKeyTime = 1.0f;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyTime(xSpineRotation,
		xPanel.Document().GetKeyIdAtIndex(xSpineRotation, 0u), fKeyTime), "the key resolves");
	ZENITH_ASSERT_EQ_FLOAT(fKeyTime, 0.0f, 1.0e-5f, "at the play head, on the frame grid");

	Zenith_AnimKeyValue xValue;
	ZENITH_ASSERT_TRUE(xPanel.Document().GetKeyValue(xSpineRotation,
		xPanel.Document().GetKeyIdAtIndex(xSpineRotation, 0u), xValue), "and carries a value");
	ZENITH_ASSERT_TRUE(xValue.m_bIsRotation, "tagged as a ROTATION, which is what the track holds");
	ZENITH_ASSERT_EQ_FLOAT(xValue.m_xQuat.y, fSIN_15, 1.0e-4f, "the LIVE POSE is what was keyed, not the clip's old value");

	xAuto.ExecuteNextStep();	// close
	ZENITH_ASSERT_FALSE(xPanel.IsOpen(), "AnimCloseClip closed the document");

	// ★ THE RIGGED PROBE IS WHERE THE LEAK ACTUALLY WAS. The preview session's
	// Flux_AnimationController cached the SKELETON asset behind an AddRef'd handle
	// and nothing ever cleared it — ReleaseRig detaches with Initialize(nullptr),
	// which used to drop only the instance pointer. The fixture's ForceUnload below
	// then deleted the skeleton regardless of refcount (so not even a "still held"
	// warning fired), leaving the controller's handle dangling until ~Session at
	// atexit Released into freed memory. Asserting the registry is EMPTY of all
	// three is what turns that one-run-in-eight assert into a deterministic failure.
	Zenith_AssetRegistry::UnloadUnused();
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strClipPath),
		"nothing still holds a reference to the clip asset");
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strSkeletonPath),
		"★ nor to the SKELETON — the reference the preview controller used to keep for ever");
	ZENITH_ASSERT_FALSE(Zenith_AssetRegistry::IsLoaded(strMeshPath),
		"nor to the preview mesh");

	// AnimOpenClip SHOWS the editor's single panel and CloseClip does not hide
	// it, so this unit has to put it back — left set, every game would boot with
	// the dope sheet open over its viewport, caused by a unit test.
	xPanel.ShowFlag() = false;

	xAuto.Reset();
	Zenith_AssetRegistry::ForceUnload(strClipPath);
	Zenith_AssetRegistry::ForceUnload(strSkeletonPath);
	Zenith_AssetRegistry::ForceUnload(strMeshPath);
	std::filesystem::remove_all(xDirectory, xError);
	Flux_PreviewSlotArbiter::ResetForTesting();
}

#include "Zenith_EditorAutomation_Animation.Tests.inl"

#endif // ZENITH_TOOLS
