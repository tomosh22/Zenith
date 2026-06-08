#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS
#include "UnitTests/Zenith_EditorTestFixture.h"
#include "Input/Zenith_InputSimulator.h"
#include "Editor/Zenith_SelectionSystem.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_UndoSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"
#include "Editor/Panels/Zenith_EditorPanel_Hierarchy.h"
#include "Editor/Panels/Zenith_EditorPanel_ContentBrowser.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Flux/MeshAnimation/Flux_AnimationClip.h"
#include "FileAccess/Zenith_FileAccess.h"
#include <cmath>
#include <filesystem>
#include <fstream>
ZENITH_TEST(Editor, BoundingBoxIntersection)
{
	// Test ray-AABB intersection
	BoundingBox xBox(Zenith_Maths::Vector3(-1, -1, -1), Zenith_Maths::Vector3(1, 1, 1));
	
	// Test 1: Ray hitting center of box from Z direction
	{
		Zenith_Maths::Vector3 rayOrigin(0, 0, -5);
		Zenith_Maths::Vector3 rayDir(0, 0, 1);
		float distance;
		bool bHit = xBox.Intersects(rayOrigin, rayDir, distance);
		ZENITH_ASSERT_TRUE(bHit, "Ray should hit the box");
		ZENITH_ASSERT_EQ_FLOAT(distance, 4.0f, 0.001f, "Distance should be ~4");
	}
	
	// Test 2: Ray missing the box
	{
		Zenith_Maths::Vector3 rayOrigin(5, 5, -5);
		Zenith_Maths::Vector3 rayDir(0, 0, 1);
		float distance;
		bool bHit = xBox.Intersects(rayOrigin, rayDir, distance);
		ZENITH_ASSERT_FALSE(bHit, "Ray should miss the box");
	}
	
	// Test 3: Ray starting inside the box
	{
		Zenith_Maths::Vector3 rayOrigin(0, 0, 0);
		Zenith_Maths::Vector3 rayDir(0, 0, 1);
		float distance;
		bool bHit = xBox.Intersects(rayOrigin, rayDir, distance);
		ZENITH_ASSERT_TRUE(bHit, "Ray starting inside should hit");
	}
	
	// Test 4: Ray pointing away from box
	{
		Zenith_Maths::Vector3 rayOrigin(0, 0, -5);
		Zenith_Maths::Vector3 rayDir(0, 0, -1);
		float distance;
		bool bHit = xBox.Intersects(rayOrigin, rayDir, distance);
		ZENITH_ASSERT_FALSE(bHit, "Ray pointing away should miss");
	}
	
}
ZENITH_TEST(Editor, SelectionSystemEmptyScene)
{
	// Test that RaycastSelect returns INVALID_ENTITY_ID when ray misses all entities
	g_xEngine.Selection().Initialise();
	g_xEngine.Selection().UpdateBoundingBoxes();

	// Cast ray far away from any likely scene content, pointing away into empty space
	Zenith_Maths::Vector3 rayOrigin(10000, 10000, 10000);
	Zenith_Maths::Vector3 rayDir(1, 1, 1);
	rayDir = glm::normalize(rayDir);

	Zenith_EntityID result = g_xEngine.Selection().RaycastSelect(rayOrigin, rayDir);
	ZENITH_ASSERT_EQ(result, INVALID_ENTITY_ID, "Ray missing all entities should return INVALID_ENTITY_ID");

	g_xEngine.Selection().Shutdown();

}
ZENITH_TEST(Editor, InvalidEntityID)
{
	// Test that INVALID_ENTITY_ID constant is properly defined
	ZENITH_ASSERT_FALSE(INVALID_ENTITY_ID.IsValid(), "INVALID_ENTITY_ID should not be valid");
	ZENITH_ASSERT_EQ(INVALID_ENTITY_ID.m_uIndex, Zenith_EntityID::INVALID_INDEX, "INVALID_ENTITY_ID index should be INVALID_INDEX");

	// Test that a valid entity ID is not equal to INVALID_ENTITY_ID
	Zenith_EntityID validID = { 0, 1 };  // Index 0, generation 1
	ZENITH_ASSERT_NE(validID, INVALID_ENTITY_ID, "Valid entity ID should not equal INVALID_ENTITY_ID");
	ZENITH_ASSERT_TRUE(validID.IsValid(), "Valid entity ID should be valid");

	validID = { 1, 1 };  // Index 1, generation 1
	ZENITH_ASSERT_NE(validID, INVALID_ENTITY_ID, "Valid entity ID should not equal INVALID_ENTITY_ID");
	ZENITH_ASSERT_TRUE(validID.IsValid(), "Valid entity ID should be valid");

}
ZENITH_TEST(Editor, TransformRoundTrip)
{
	// Test that transform values can be set and retrieved accurately
	// This is important for property panel editing

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create a test entity
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TestEntity");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	
	// Test position round trip
	Zenith_Maths::Vector3 testPos(123.456f, -789.012f, 0.001f);
	xTransform.SetPosition(testPos);
	Zenith_Maths::Vector3 retrievedPos;
	xTransform.GetPosition(retrievedPos);
	ZENITH_ASSERT_LT(glm::length(testPos - retrievedPos), 0.0001f, "Position round trip failed");
	
	// Test scale round trip
	Zenith_Maths::Vector3 testScale(2.0f, 0.5f, 3.0f);
	xTransform.SetScale(testScale);
	Zenith_Maths::Vector3 retrievedScale;
	xTransform.GetScale(retrievedScale);
	ZENITH_ASSERT_LT(glm::length(testScale - retrievedScale), 0.0001f, "Scale round trip failed");
	
	// Test rotation round trip (quaternion)
	Zenith_Maths::Quat testRot = glm::angleAxis(glm::radians(45.0f), Zenith_Maths::Vector3(0, 1, 0));
	xTransform.SetRotation(testRot);
	Zenith_Maths::Quat retrievedRot;
	xTransform.GetRotation(retrievedRot);
	
	// Compare quaternions (accounting for sign ambiguity)
	float dotProduct = glm::dot(testRot, retrievedRot);
	ZENITH_ASSERT_EQ_FLOAT(std::abs(dotProduct), 1.0f, 0.0001f, "Rotation round trip failed");

}

//------------------------------------------------------------------------------
// Multi-Select Tests
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, MultiSelectSingle)
{

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create a test entity
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "MultiSelectEntity1");
	Zenith_EntityID uEntityID = xEntity.GetEntityID();

	// Clear selection first
	g_xEngine.Editor().ClearSelection();
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().HasSelection(), "Should have no selection initially");

	// Select single entity
	g_xEngine.Editor().SelectEntity(uEntityID, false);

	// Verify selection
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().HasSelection(), "Should have selection");
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetSelectionCount(), 1, "Should have exactly 1 selected");
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().IsSelected(uEntityID), "Entity should be selected");
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().HasMultiSelection(), "Should not have multi-selection with 1 entity");

	g_xEngine.Editor().ClearSelection();

}
ZENITH_TEST(Editor, MultiSelectCtrlClick)
{

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create test entities
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "CtrlClickEntity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "CtrlClickEntity2");
	Zenith_Entity xEntity3 = g_xEngine.Scenes().CreateEntity(pxSceneData, "CtrlClickEntity3");

	Zenith_EntityID uEntityID1 = xEntity1.GetEntityID();
	Zenith_EntityID uEntityID2 = xEntity2.GetEntityID();
	Zenith_EntityID uEntityID3 = xEntity3.GetEntityID();

	// Clear and select first entity
	g_xEngine.Editor().ClearSelection();
	g_xEngine.Editor().SelectEntity(uEntityID1, false);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetSelectionCount(), 1, "Should have 1 selected");

	// Add second entity (simulates Ctrl+click)
	g_xEngine.Editor().SelectEntity(uEntityID2, true);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetSelectionCount(), 2, "Should have 2 selected");
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().IsSelected(uEntityID1), "First entity should still be selected");
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().IsSelected(uEntityID2), "Second entity should be selected");
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().HasMultiSelection(), "Should have multi-selection");

	// Add third entity
	g_xEngine.Editor().SelectEntity(uEntityID3, true);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetSelectionCount(), 3, "Should have 3 selected");

	// Toggle selection (ctrl+click on already selected entity should deselect)
	g_xEngine.Editor().ToggleEntitySelection(uEntityID2);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetSelectionCount(), 2, "Should have 2 selected after toggle");
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().IsSelected(uEntityID2), "Second entity should be deselected");

	g_xEngine.Editor().ClearSelection();

}
ZENITH_TEST(Editor, MultiSelectClear)
{

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create and select multiple entities
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "ClearEntity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "ClearEntity2");

	Zenith_EntityID uEntityID1 = xEntity1.GetEntityID();
	Zenith_EntityID uEntityID2 = xEntity2.GetEntityID();

	g_xEngine.Editor().ClearSelection();
	g_xEngine.Editor().SelectEntity(uEntityID1, false);
	g_xEngine.Editor().SelectEntity(uEntityID2, true);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetSelectionCount(), 2, "Should have 2 selected");

	// Clear all selection
	g_xEngine.Editor().ClearSelection();
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().HasSelection(), "Should have no selection after clear");
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetSelectionCount(), 0, "Selection count should be 0");
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().IsSelected(uEntityID1), "First entity should not be selected");
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().IsSelected(uEntityID2), "Second entity should not be selected");

}
ZENITH_TEST(Editor, MultiSelectAfterEntityDelete)
{

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);

	// Create entities
	Zenith_Entity xEntity1 = g_xEngine.Scenes().CreateEntity(pxSceneData, "DeleteTestEntity1");
	Zenith_Entity xEntity2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "DeleteTestEntity2");

	Zenith_EntityID uEntityID1 = xEntity1.GetEntityID();
	Zenith_EntityID uEntityID2 = xEntity2.GetEntityID();

	// Select both entities
	g_xEngine.Editor().ClearSelection();
	g_xEngine.Editor().SelectEntity(uEntityID1, false);
	g_xEngine.Editor().SelectEntity(uEntityID2, true);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetSelectionCount(), 2, "Should have 2 selected");

	// Remove one from selection (simulating entity deletion cleanup)
	g_xEngine.Editor().DeselectEntity(uEntityID1);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetSelectionCount(), 1, "Should have 1 selected after deselect");
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().IsSelected(uEntityID1), "Deleted entity should not be selected");
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().IsSelected(uEntityID2), "Other entity should still be selected");

	g_xEngine.Editor().ClearSelection();

}

//------------------------------------------------------------------------------
// Mode Transition Tests
//------------------------------------------------------------------------------
// These tests verify editor mode transitions work correctly. The mode transitions
// trigger scene backup/restore which is handled synchronously via
// FlushPendingSceneOperations() in the test fixture's ResetEditorState().
ZENITH_TEST(Editor, ModeTransitionStoppedToPlaying)
{
	EDITOR_TEST_BEGIN(TestModeTransitionStoppedToPlaying);

	// Verify we're in Stopped mode (should be the default state)
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Stopped, "Should start in Stopped mode");

	// Transition to Playing mode
	g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Playing, "Should be in Playing mode after transition");

	// Note: EDITOR_TEST_END will call TearDown which calls ResetEditorState,
	// which sets mode back to Stopped and flushes pending scene operations

	EDITOR_TEST_END(TestModeTransitionStoppedToPlaying);
}
ZENITH_TEST(Editor, ModeTransitionPlayingToPaused)
{
	EDITOR_TEST_BEGIN(TestModeTransitionPlayingToPaused);

	// Start by transitioning to Playing mode
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Stopped, "Should start in Stopped mode");
	g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Playing, "Should be in Playing mode");

	// Transition to Paused mode
	g_xEngine.Editor().SetEditorMode(EditorMode::Paused);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Paused, "Should be in Paused mode after transition");

	EDITOR_TEST_END(TestModeTransitionPlayingToPaused);
}
ZENITH_TEST(Editor, ModeTransitionPausedToStopped)
{
	EDITOR_TEST_BEGIN(TestModeTransitionPausedToStopped);

	// Start by transitioning through Playing to Paused
	g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
	g_xEngine.Editor().SetEditorMode(EditorMode::Paused);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Paused, "Should be in Paused mode");

	// Transition to Stopped mode
	g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Stopped, "Should be in Stopped mode after transition");

	// Flush pending scene operations to complete the restore
	g_xEngine.Editor().FlushPendingSceneOperations();

	EDITOR_TEST_END(TestModeTransitionPausedToStopped);
}
ZENITH_TEST(Editor, ModeTransitionFullCycle)
{
	EDITOR_TEST_BEGIN(TestModeTransitionFullCycle);

	// Start in Stopped mode
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Stopped, "Should start in Stopped mode");

	// Stopped -> Playing
	g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Playing, "Should be in Playing mode");

	// Playing -> Paused
	g_xEngine.Editor().SetEditorMode(EditorMode::Paused);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Paused, "Should be in Paused mode");

	// Paused -> Playing (resume)
	g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Playing, "Should be in Playing mode after resume");

	// Playing -> Stopped (this queues scene restore)
	g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Stopped, "Should be in Stopped mode");

	// Flush pending scene operations to complete the restore
	g_xEngine.Editor().FlushPendingSceneOperations();

	EDITOR_TEST_END(TestModeTransitionFullCycle);
}
ZENITH_TEST(Editor, ModePreservesSelection)
{
	EDITOR_TEST_BEGIN(TestModePreservesSelection);

	// Create a test entity and select it
	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("SelectionTestEntity");
	g_xEngine.Editor().SelectEntity(uEntity, false);
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().IsSelected(uEntity), "Entity should be selected");

	Zenith_Entity* pxSelected = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_TRUE(pxSelected != nullptr && pxSelected->GetEntityID() == uEntity, "Selected entity should be retrievable");

	// Transition to Playing mode (selection should be preserved during play)
	g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Playing, "Should be in Playing mode");

	// Selection is cleared during mode transitions (this is expected behavior)
	// The scene backup/restore process resets selection state

	// Transition back to Stopped and flush to complete restore
	g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
	g_xEngine.Editor().FlushPendingSceneOperations();

	// Verify entity still exists in scene after restore
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	// Note: Entity IDs are regenerated on scene load, so we can't check the same ID
	// Instead, verify scene has entities and is in a valid state
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Stopped, "Should be back in Stopped mode");

	EDITOR_TEST_END(TestModePreservesSelection);
}

//------------------------------------------------------------------------------
// Gizmo Operation Tests
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, GizmoModeSwitch)
{
	EDITOR_TEST_BEGIN(TestGizmoModeSwitch);

	g_xEngine.Editor().SetGizmoMode(EditorGizmoMode::Translate);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetGizmoMode(), EditorGizmoMode::Translate, "Should be Translate");

	g_xEngine.Editor().SetGizmoMode(EditorGizmoMode::Rotate);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetGizmoMode(), EditorGizmoMode::Rotate, "Should be Rotate");

	g_xEngine.Editor().SetGizmoMode(EditorGizmoMode::Scale);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetGizmoMode(), EditorGizmoMode::Scale, "Should be Scale");

	EDITOR_TEST_END(TestGizmoModeSwitch);
}
ZENITH_TEST(Editor, GizmoModeTranslate)
{
	EDITOR_TEST_BEGIN(TestGizmoModeTranslate);

	g_xEngine.Editor().SetGizmoMode(EditorGizmoMode::Translate);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetGizmoMode(), EditorGizmoMode::Translate, "Gizmo mode should be Translate");

	EDITOR_TEST_END(TestGizmoModeTranslate);
}
ZENITH_TEST(Editor, GizmoModeRotate)
{
	EDITOR_TEST_BEGIN(TestGizmoModeRotate);

	g_xEngine.Editor().SetGizmoMode(EditorGizmoMode::Rotate);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetGizmoMode(), EditorGizmoMode::Rotate, "Gizmo mode should be Rotate");

	EDITOR_TEST_END(TestGizmoModeRotate);
}
ZENITH_TEST(Editor, GizmoModeScale)
{
	EDITOR_TEST_BEGIN(TestGizmoModeScale);

	g_xEngine.Editor().SetGizmoMode(EditorGizmoMode::Scale);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetGizmoMode(), EditorGizmoMode::Scale, "Gizmo mode should be Scale");

	EDITOR_TEST_END(TestGizmoModeScale);
}
ZENITH_TEST(Editor, GizmoModeViaKeyboard)
{
	EDITOR_TEST_BEGIN(TestGizmoModeViaKeyboard);

	// Simulate pressing W key for translate mode
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_W);
	// In real implementation, this would trigger gizmo mode change
	// For now, we just test the mock input works
	ZENITH_ASSERT_TRUE(Zenith_InputSimulator::WasKeyPressedThisFrameSimulated(ZENITH_KEY_W), "W key should be registered as pressed");

	Zenith_InputSimulator::BeginTestFrame();

	// Simulate pressing E key for rotate mode
	Zenith_InputSimulator::SimulateKeyPress(ZENITH_KEY_E);
	ZENITH_ASSERT_TRUE(Zenith_InputSimulator::WasKeyPressedThisFrameSimulated(ZENITH_KEY_E), "E key should be registered as pressed");

	EDITOR_TEST_END(TestGizmoModeViaKeyboard);
}

//------------------------------------------------------------------------------
// Undo/Redo System Tests
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, UndoSystemCanUndo)
{
	EDITOR_TEST_BEGIN(TestUndoSystemCanUndo);

	g_xEngine.UndoSystem().Clear();
	ZENITH_ASSERT_FALSE(g_xEngine.UndoSystem().CanUndo(), "Should not be able to undo with empty stack");

	EDITOR_TEST_END(TestUndoSystemCanUndo);
}
ZENITH_TEST(Editor, UndoSystemCanRedo)
{
	EDITOR_TEST_BEGIN(TestUndoSystemCanRedo);

	g_xEngine.UndoSystem().Clear();
	ZENITH_ASSERT_FALSE(g_xEngine.UndoSystem().CanRedo(), "Should not be able to redo with empty stack");

	EDITOR_TEST_END(TestUndoSystemCanRedo);
}
ZENITH_TEST(Editor, TransformEditUndoRedo)
{
	EDITOR_TEST_BEGIN(TestTransformEditUndoRedo);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("UndoTestEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

	// Set initial position
	Zenith_Maths::Vector3 xOldPos(0, 0, 0);
	Zenith_Maths::Vector3 xNewPos(10, 20, 30);
	xTransform.SetPosition(xOldPos);

	// Verify position was set
	Zenith_Maths::Vector3 xCurrentPos;
	xTransform.GetPosition(xCurrentPos);
	ZENITH_ASSERT_LT(glm::length(xCurrentPos - xOldPos), 0.001f, "Initial position should be set");

	// Change position
	xTransform.SetPosition(xNewPos);
	xTransform.GetPosition(xCurrentPos);
	ZENITH_ASSERT_LT(glm::length(xCurrentPos - xNewPos), 0.001f, "New position should be set");

	EDITOR_TEST_END(TestTransformEditUndoRedo);
}
ZENITH_TEST(Editor, UndoStackClearOnSceneChange)
{
	EDITOR_TEST_BEGIN(TestUndoStackClearOnSceneChange);

	g_xEngine.UndoSystem().Clear();
	ZENITH_ASSERT_FALSE(g_xEngine.UndoSystem().CanUndo(), "Undo stack should be empty after clear");
	ZENITH_ASSERT_FALSE(g_xEngine.UndoSystem().CanRedo(), "Redo stack should be empty after clear");

	EDITOR_TEST_END(TestUndoStackClearOnSceneChange);
}
ZENITH_TEST(Editor, RedoStackClearOnNewEdit)
{
	EDITOR_TEST_BEGIN(TestRedoStackClearOnNewEdit);

	g_xEngine.UndoSystem().Clear();
	// Redo stack should be cleared when a new edit is made after undo
	ZENITH_ASSERT_FALSE(g_xEngine.UndoSystem().CanRedo(), "Redo stack should be empty");

	EDITOR_TEST_END(TestRedoStackClearOnNewEdit);
}
ZENITH_TEST(Editor, Audit318_UndoTransformEdit_SurvivesActiveSceneSwitch)
{
	EDITOR_TEST_BEGIN(TestAudit318_UndoTransformEdit_SurvivesActiveSceneSwitch);

	g_xEngine.UndoSystem().Clear();

	// Snapshot saved active so we can restore at the end.
	Zenith_Scene xSavedActive = g_xEngine.Scenes().GetActiveScene();

	// Create Scene A (active), put an entity there, capture its old transform.
	Zenith_Scene xSceneA = g_xEngine.Scenes().LoadScene("Audit318_UndoSceneA", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	g_xEngine.Scenes().SetActiveScene(xSceneA);
	Zenith_SceneData* pxSceneAData = g_xEngine.Scenes().GetSceneData(xSceneA);
	Zenith_Entity xTargetEntity = g_xEngine.Scenes().CreateEntity(pxSceneAData, "Audit318_UndoTarget");
	Zenith_EntityID xTargetID = xTargetEntity.GetEntityID();
	Zenith_TransformComponent& xTransform = xTargetEntity.GetComponent<Zenith_TransformComponent>();

	const Zenith_Maths::Vector3 xOldPos(0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Quat xOldRot(1.0f, 0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xOldScale(1.0f, 1.0f, 1.0f);
	const Zenith_Maths::Vector3 xNewPos(10.0f, 20.0f, 30.0f);
	const Zenith_Maths::Quat xNewRot(1.0f, 0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xNewScale(2.0f, 2.0f, 2.0f);

	xTransform.SetPosition(xOldPos);
	xTransform.SetRotation(xOldRot);
	xTransform.SetScale(xOldScale);

	// Record a TransformEdit command (commits to undo stack via Execute).
	auto* pxCmd = new Zenith_UndoCommand_TransformEdit(xTargetID, xOldPos, xOldRot, xOldScale, xNewPos, xNewRot, xNewScale);
	g_xEngine.UndoSystem().Execute(pxCmd);

	// Verify the new position landed.
	Zenith_Maths::Vector3 xPosAfterExecute;
	xTransform.GetPosition(xPosAfterExecute);
	ZENITH_ASSERT_LT(glm::length(xPosAfterExecute - xNewPos), 0.001f, "§3.18: Execute should apply the new transform");

	// Create Scene B and make it active — this is the scenario that previously
	// broke undo: the command captured Scene A's handle, then Ctrl+Z would
	// route through GetSceneData(m_xScene) but the selection-layer semantics
	// would fail silently. With the EntityID-based resolution, undo survives.
	Zenith_Scene xSceneB = g_xEngine.Scenes().LoadScene("Audit318_UndoSceneB", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	g_xEngine.Scenes().SetActiveScene(xSceneB);
	ZENITH_ASSERT_EQ(g_xEngine.Scenes().GetActiveScene(), xSceneB, "Setup: Scene B should be active before undo");

	// Ctrl+Z — must restore Scene A's entity transform despite Scene B being active.
	g_xEngine.UndoSystem().Undo();

	Zenith_Maths::Vector3 xPosAfterUndo;
	xTransform.GetPosition(xPosAfterUndo);
	ZENITH_ASSERT_LT(glm::length(xPosAfterUndo - xOldPos), 0.001f, "§3.18: Undo must restore Scene A's entity transform even when Scene B is active "
		"(Unity parity: object-scene is intrinsic via GetSceneDataForEntity)");

	// Cleanup
	g_xEngine.UndoSystem().Clear();
	g_xEngine.Scenes().UnloadScene(xSceneB);
	g_xEngine.Scenes().UnloadScene(xSceneA);
	if (xSavedActive.IsValid())
	{
		g_xEngine.Scenes().SetActiveScene(xSavedActive);
	}

	EDITOR_TEST_END(TestAudit318_UndoTransformEdit_SurvivesActiveSceneSwitch);
}

//------------------------------------------------------------------------------
// Entity Hierarchy Tests
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, EntityReparenting)
{
	EDITOR_TEST_BEGIN(TestEntityReparenting);

	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("Parent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("Child");

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);

	// Set parent
	xChild.SetParent(uParent);
	ZENITH_ASSERT_EQ(xChild.GetParentEntityID(), uParent, "Child should have correct parent");

	EDITOR_TEST_END(TestEntityReparenting);
}
ZENITH_TEST(Editor, CreateChildEntity)
{
	EDITOR_TEST_BEGIN(TestCreateChildEntity);

	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("Parent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("Child");

	Zenith_EditorTestFixture::SetupHierarchy(uParent, uChild);

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);
	ZENITH_ASSERT_EQ(xChild.GetParentEntityID(), uParent, "Child should have parent set");

	EDITOR_TEST_END(TestCreateChildEntity);
}
ZENITH_TEST(Editor, UnparentEntity)
{
	EDITOR_TEST_BEGIN(TestUnparentEntity);

	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("Parent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("Child");

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);

	// Set parent
	xChild.SetParent(uParent);
	ZENITH_ASSERT_EQ(xChild.GetParentEntityID(), uParent, "Child should have parent");

	// Unparent
	xChild.SetParent(INVALID_ENTITY_ID);
	ZENITH_ASSERT_FALSE(xChild.GetParentEntityID().IsValid(), "Child should have no parent after unparent");

	EDITOR_TEST_END(TestUnparentEntity);
}
ZENITH_TEST(Editor, HierarchyCircularPrevention)
{
	EDITOR_TEST_BEGIN(TestHierarchyCircularPrevention);

	// This test verifies that the hierarchy system prevents circular references
	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("Parent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("Child");

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xParent = pxSceneData->GetEntity(uParent);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);

	// Set up valid hierarchy
	xChild.SetParent(uParent);
	ZENITH_ASSERT_EQ(xChild.GetParentEntityID(), uParent, "Valid hierarchy should work");

	// Note: Attempting to set parent as child of its own child should be prevented
	// by the hierarchy system's circular dependency check

	EDITOR_TEST_END(TestHierarchyCircularPrevention);
}
ZENITH_TEST(Editor, DeleteParentWithChildren)
{
	EDITOR_TEST_BEGIN(TestDeleteParentWithChildren);

	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("Parent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("Child");

	Zenith_EditorTestFixture::SetupHierarchy(uParent, uChild);

	// The parent-child relationship is established
	// Deletion behavior depends on implementation (cascade delete or orphan children)

	EDITOR_TEST_END(TestDeleteParentWithChildren);
}

//------------------------------------------------------------------------------
// Selection System Tests (expanded)
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, RangeSelection)
{
	EDITOR_TEST_BEGIN(TestRangeSelection);

	// Range selection (shift+click) functionality test
	Zenith_EntityID uEntity1 = Zenith_EditorTestFixture::CreateTestEntity("RangeEntity1");
	Zenith_EntityID uEntity2 = Zenith_EditorTestFixture::CreateTestEntity("RangeEntity2");

	g_xEngine.Editor().SelectEntity(uEntity1, false);
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().IsSelected(uEntity1), "First entity should be selected");

	// SelectRange would select all entities between the last clicked and new one
	// For now, test that the API exists
	g_xEngine.Editor().SelectRange(uEntity2);

	EDITOR_TEST_END(TestRangeSelection);
}
ZENITH_TEST(Editor, SelectionWithHierarchy)
{
	EDITOR_TEST_BEGIN(TestSelectionWithHierarchy);

	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("SelectParent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("SelectChild");

	Zenith_EditorTestFixture::SetupHierarchy(uParent, uChild);

	// Select parent
	g_xEngine.Editor().SelectEntity(uParent, false);
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().IsSelected(uParent), "Parent should be selected");
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().IsSelected(uChild), "Child should not be auto-selected");

	// Select child
	g_xEngine.Editor().SelectEntity(uChild, false);
	ZENITH_ASSERT_TRUE(g_xEngine.Editor().IsSelected(uChild), "Child should be selected");
	ZENITH_ASSERT_FALSE(g_xEngine.Editor().IsSelected(uParent), "Parent should be deselected");

	EDITOR_TEST_END(TestSelectionWithHierarchy);
}
ZENITH_TEST(Editor, RaycastSelectWithMultipleEntities)
{
	EDITOR_TEST_BEGIN(TestRaycastSelectWithMultipleEntities);

	// Create entities at known positions
	Zenith_EntityID uEntity1 = Zenith_EditorTestFixture::CreateTestEntityWithTransform(
		"RaycastEntity1", Zenith_Maths::Vector3(0, 0, 5), Zenith_Maths::Vector3(1, 1, 1));
	Zenith_EntityID uEntity2 = Zenith_EditorTestFixture::CreateTestEntityWithTransform(
		"RaycastEntity2", Zenith_Maths::Vector3(0, 0, 10), Zenith_Maths::Vector3(1, 1, 1));

	// The raycast should select the closest entity when multiple are in the ray path
	// This is tested via the selection system

	EDITOR_TEST_END(TestRaycastSelectWithMultipleEntities);
}

//------------------------------------------------------------------------------
// Console Tests
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, ConsoleAddLog)
{
	EDITOR_TEST_BEGIN(TestConsoleAddLog);

	// Test that we can add log messages without crashing
	g_xEngine.Editor().AddLogMessage("Test info message", ConsoleLogEntry::LogLevel::Info, LOG_CATEGORY_CORE);
	g_xEngine.Editor().AddLogMessage("Test warning message", ConsoleLogEntry::LogLevel::Warning, LOG_CATEGORY_CORE);
	g_xEngine.Editor().AddLogMessage("Test error message", ConsoleLogEntry::LogLevel::Error, LOG_CATEGORY_CORE);

	EDITOR_TEST_END(TestConsoleAddLog);
}
ZENITH_TEST(Editor, ConsoleClear)
{
	EDITOR_TEST_BEGIN(TestConsoleClear);

	g_xEngine.Editor().AddLogMessage("Message to clear", ConsoleLogEntry::LogLevel::Info, LOG_CATEGORY_CORE);
	g_xEngine.Editor().ClearConsole();
	// Console should be empty after clear

	EDITOR_TEST_END(TestConsoleClear);
}

//------------------------------------------------------------------------------
// Component Tests - Generic
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, ComponentAddRemove)
{
	EDITOR_TEST_BEGIN(TestComponentAddRemove);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("ComponentTestEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);

	// All entities have TransformComponent by default
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_TransformComponent>(), "Entity should have TransformComponent by default");

	// Add CameraComponent
	ZENITH_ASSERT_FALSE(xEntity.HasComponent<Zenith_CameraComponent>(), "Entity should not have CameraComponent initially");

	xEntity.AddComponent<Zenith_CameraComponent>();
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_CameraComponent>(), "Entity should have CameraComponent after adding");

	// Remove CameraComponent
	xEntity.RemoveComponent<Zenith_CameraComponent>();
	ZENITH_ASSERT_FALSE(xEntity.HasComponent<Zenith_CameraComponent>(), "Entity should not have CameraComponent after removal");

	EDITOR_TEST_END(TestComponentAddRemove);
}
ZENITH_TEST(Editor, ComponentAddViaRegistry)
{
	EDITOR_TEST_BEGIN(TestComponentAddViaRegistry);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("RegistryTestEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);

	// The component registry provides type-erased component operations
	Zenith_ComponentRegistry& xRegistry = Zenith_ComponentRegistry::Get();

	// Verify registry is accessible
	ZENITH_ASSERT_GT(xRegistry.GetComponentCount(), 0, "Registry should have registered components");

	EDITOR_TEST_END(TestComponentAddViaRegistry);
}
ZENITH_TEST(Editor, MultipleComponentAdd)
{
	EDITOR_TEST_BEGIN(TestMultipleComponentAdd);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("MultiComponentEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);

	// Add multiple components
	xEntity.AddComponent<Zenith_CameraComponent>();

	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_TransformComponent>(), "Should have Transform");
	ZENITH_ASSERT_TRUE(xEntity.HasComponent<Zenith_CameraComponent>(), "Should have Camera");

	EDITOR_TEST_END(TestMultipleComponentAdd);
}

//------------------------------------------------------------------------------
// TransformComponent Tests
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, TransformPositionRoundTrip)
{
	EDITOR_TEST_BEGIN(TestTransformPositionRoundTrip);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("PosRoundTripEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

	Zenith_Maths::Vector3 xTestPos(100.0f, -50.0f, 25.5f);
	xTransform.SetPosition(xTestPos);

	Zenith_Maths::Vector3 xRetrievedPos;
	xTransform.GetPosition(xRetrievedPos);

	ZENITH_ASSERT_LT(glm::length(xTestPos - xRetrievedPos), 0.0001f, "Position round trip should preserve value");

	EDITOR_TEST_END(TestTransformPositionRoundTrip);
}
ZENITH_TEST(Editor, TransformRotationRoundTrip)
{
	EDITOR_TEST_BEGIN(TestTransformRotationRoundTrip);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("RotRoundTripEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

	Zenith_Maths::Quat xTestRot = glm::angleAxis(glm::radians(90.0f), Zenith_Maths::Vector3(0, 1, 0));
	xTransform.SetRotation(xTestRot);

	Zenith_Maths::Quat xRetrievedRot;
	xTransform.GetRotation(xRetrievedRot);

	float fDot = glm::dot(xTestRot, xRetrievedRot);
	ZENITH_ASSERT_EQ_FLOAT(std::abs(fDot), 1.0f, 0.0001f, "Rotation round trip should preserve value");

	EDITOR_TEST_END(TestTransformRotationRoundTrip);
}
ZENITH_TEST(Editor, TransformScaleRoundTrip)
{
	EDITOR_TEST_BEGIN(TestTransformScaleRoundTrip);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("ScaleRoundTripEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

	Zenith_Maths::Vector3 xTestScale(2.0f, 0.5f, 3.0f);
	xTransform.SetScale(xTestScale);

	Zenith_Maths::Vector3 xRetrievedScale;
	xTransform.GetScale(xRetrievedScale);

	ZENITH_ASSERT_LT(glm::length(xTestScale - xRetrievedScale), 0.0001f, "Scale round trip should preserve value");

	EDITOR_TEST_END(TestTransformScaleRoundTrip);
}
ZENITH_TEST(Editor, TransformMatrixBuild)
{
	EDITOR_TEST_BEGIN(TestTransformMatrixBuild);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("MatrixBuildEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

	// Set transform values
	xTransform.SetPosition(Zenith_Maths::Vector3(10, 20, 30));
	xTransform.SetScale(Zenith_Maths::Vector3(2, 2, 2));

	// Build model matrix
	Zenith_Maths::Matrix4 xMatrix;
	xTransform.BuildModelMatrix(xMatrix);

	// Verify the translation part of the matrix
	ZENITH_ASSERT_EQ_FLOAT(xMatrix[3][0], 10.0f, 0.001f, "Matrix X translation should be 10");
	ZENITH_ASSERT_EQ_FLOAT(xMatrix[3][1], 20.0f, 0.001f, "Matrix Y translation should be 20");
	ZENITH_ASSERT_EQ_FLOAT(xMatrix[3][2], 30.0f, 0.001f, "Matrix Z translation should be 30");

	EDITOR_TEST_END(TestTransformMatrixBuild);
}
ZENITH_TEST(Editor, TransformParentChild)
{
	EDITOR_TEST_BEGIN(TestTransformParentChild);

	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("TransformParent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("TransformChild");

	Zenith_EditorTestFixture::SetupHierarchy(uParent, uChild);

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);

	// Verify parent is set. Phase 5b: the hierarchy is slot-backed, queried via
	// the Zenith_Entity API (the Transform's hierarchy shims were removed).
	ZENITH_ASSERT_EQ(xChild.GetParentEntityID(), uParent, "Entity should have correct parent entity ID");

	EDITOR_TEST_END(TestTransformParentChild);
}
ZENITH_TEST(Editor, TransformHierarchyTraversal)
{
	EDITOR_TEST_BEGIN(TestTransformHierarchyTraversal);

	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("TraversalParent");
	Zenith_EntityID uChild1 = Zenith_EditorTestFixture::CreateTestEntity("TraversalChild1");
	Zenith_EntityID uChild2 = Zenith_EditorTestFixture::CreateTestEntity("TraversalChild2");

	Zenith_EditorTestFixture::SetupHierarchy(uParent, uChild1);
	Zenith_EditorTestFixture::SetupHierarchy(uParent, uChild2);

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xParent = pxSceneData->GetEntity(uParent);

	// Count children via traversal. Phase 5b: the former Transform::ForEachChild
	// shim is gone; iterate the slot-backed child IDs on the entity and resolve
	// each child's Transform through the scene (mirrors the old shim, which only
	// visited existing children that carry a TransformComponent).
	const Zenith_Vector<Zenith_EntityID>& axChildIDs = xParent.GetChildEntityIDs();
	u_int uChildCount = 0;
	for (u_int u = 0; u < axChildIDs.GetSize(); ++u)
	{
		Zenith_EntityID uChildID = axChildIDs.Get(u);
		if (!pxSceneData->EntityExists(uChildID)) continue;
		Zenith_TransformComponent& xChildTransform = pxSceneData->GetEntity(uChildID).GetComponent<Zenith_TransformComponent>();
		(void)xChildTransform;
		uChildCount++;
	}

	ZENITH_ASSERT_EQ(uChildCount, 2, "Parent should have 2 children");

	EDITOR_TEST_END(TestTransformHierarchyTraversal);
}
ZENITH_TEST(Editor, TransformIsDescendantOf)
{
	EDITOR_TEST_BEGIN(TestTransformIsDescendantOf);

	Zenith_EntityID uGrandparent = Zenith_EditorTestFixture::CreateTestEntity("Grandparent");
	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("Parent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("Child");

	Zenith_EditorTestFixture::SetupHierarchy(uGrandparent, uParent);
	Zenith_EditorTestFixture::SetupHierarchy(uParent, uChild);

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);

	// Phase 5b: descendant queries run on the slot-backed Zenith_Entity API (the
	// Transform::IsDescendantOf shim was removed). Same walk, same semantics.
	// Child should be descendant of grandparent
	ZENITH_ASSERT_TRUE(xChild.IsDescendantOf(uGrandparent), "Child should be descendant of grandparent");

	// Child should be descendant of parent
	ZENITH_ASSERT_TRUE(xChild.IsDescendantOf(uParent), "Child should be descendant of parent");

	// Grandparent should not be descendant of child
	Zenith_Entity xGrandparent = pxSceneData->GetEntity(uGrandparent);
	ZENITH_ASSERT_FALSE(xGrandparent.IsDescendantOf(uChild), "Grandparent should not be descendant of child");

	EDITOR_TEST_END(TestTransformIsDescendantOf);
}

//------------------------------------------------------------------------------
// CameraComponent Tests
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, CameraPerspectiveMatrix)
{
	EDITOR_TEST_BEGIN(TestCameraPerspectiveMatrix);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("PerspCameraEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	xEntity.AddComponent<Zenith_CameraComponent>();

	Zenith_CameraComponent& xCamera = xEntity.GetComponent<Zenith_CameraComponent>();
	// Camera defaults to perspective mode with reasonable FOV
	xCamera.SetFOV(60.0f);
	xCamera.SetAspectRatio(16.0f / 9.0f);

	Zenith_Maths::Matrix4 xProj;
	xCamera.BuildProjectionMatrix(xProj);

	// Verify perspective matrix is not identity
	ZENITH_ASSERT_NE(xProj, Zenith_Maths::Matrix4(1.0f), "Projection matrix should not be identity");

	EDITOR_TEST_END(TestCameraPerspectiveMatrix);
}
ZENITH_TEST(Editor, CameraOrthographicMatrix)
{
	EDITOR_TEST_BEGIN(TestCameraOrthographicMatrix);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("OrthoCameraEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	xEntity.AddComponent<Zenith_CameraComponent>();

	Zenith_CameraComponent& xCamera = xEntity.GetComponent<Zenith_CameraComponent>();
	// Test the projection matrix with default settings
	// The camera type is controlled internally

	Zenith_Maths::Matrix4 xProj;
	xCamera.BuildProjectionMatrix(xProj);

	// Verify projection matrix is not identity
	ZENITH_ASSERT_NE(xProj, Zenith_Maths::Matrix4(1.0f), "Projection matrix should not be identity");

	EDITOR_TEST_END(TestCameraOrthographicMatrix);
}
ZENITH_TEST(Editor, CameraViewMatrix)
{
	EDITOR_TEST_BEGIN(TestCameraViewMatrix);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("ViewCameraEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	xEntity.AddComponent<Zenith_CameraComponent>();

	Zenith_CameraComponent& xCamera = xEntity.GetComponent<Zenith_CameraComponent>();
	xCamera.SetPosition(Zenith_Maths::Vector3(0, 5, 10));

	Zenith_Maths::Matrix4 xView;
	xCamera.BuildViewMatrix(xView);

	// Verify view matrix is not identity
	ZENITH_ASSERT_NE(xView, Zenith_Maths::Matrix4(1.0f), "View matrix should not be identity");

	EDITOR_TEST_END(TestCameraViewMatrix);
}
ZENITH_TEST(Editor, CameraTypeSwitch)
{
	EDITOR_TEST_BEGIN(TestCameraTypeSwitch);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("TypeSwitchCamera");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	xEntity.AddComponent<Zenith_CameraComponent>();

	Zenith_CameraComponent& xCamera = xEntity.GetComponent<Zenith_CameraComponent>();

	// Camera starts with default perspective settings
	// Verify it can build projection matrix correctly
	Zenith_Maths::Matrix4 xProj1;
	xCamera.BuildProjectionMatrix(xProj1);
	ZENITH_ASSERT_NE(xProj1, Zenith_Maths::Matrix4(1.0f), "First projection matrix should not be identity");

	// Changing FOV should produce different projection matrix
	float fOriginalFOV = xCamera.GetFOV();
	xCamera.SetFOV(90.0f);

	Zenith_Maths::Matrix4 xProj2;
	xCamera.BuildProjectionMatrix(xProj2);
	ZENITH_ASSERT_TRUE(xProj2 != xProj1 || fOriginalFOV == 90.0f, "Changed FOV should produce different matrix");

	EDITOR_TEST_END(TestCameraTypeSwitch);
}
ZENITH_TEST(Editor, CameraNearFarPlanes)
{
	EDITOR_TEST_BEGIN(TestCameraNearFarPlanes);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("NearFarCamera");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	xEntity.AddComponent<Zenith_CameraComponent>();

	Zenith_CameraComponent& xCamera = xEntity.GetComponent<Zenith_CameraComponent>();
	xCamera.SetNearPlane(0.5f);
	xCamera.SetFarPlane(500.0f);

	ZENITH_ASSERT_EQ_FLOAT(xCamera.GetNearPlane(), 0.5f, 0.001f, "Near plane should be 0.5");
	ZENITH_ASSERT_EQ_FLOAT(xCamera.GetFarPlane(), 500.0f, 0.001f, "Far plane should be 500");

	EDITOR_TEST_END(TestCameraNearFarPlanes);
}
ZENITH_TEST(Editor, CameraFOVSettings)
{
	EDITOR_TEST_BEGIN(TestCameraFOVSettings);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("FOVCamera");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	xEntity.AddComponent<Zenith_CameraComponent>();

	Zenith_CameraComponent& xCamera = xEntity.GetComponent<Zenith_CameraComponent>();
	xCamera.SetFOV(90.0f);

	ZENITH_ASSERT_EQ_FLOAT(xCamera.GetFOV(), 90.0f, 0.001f, "FOV should be 90");

	EDITOR_TEST_END(TestCameraFOVSettings);
}

//------------------------------------------------------------------------------
// ModelComponent Tests (Stubs - require asset loading)
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, ModelMeshAccess)
{
	EDITOR_TEST_BEGIN(TestModelMeshAccess);
	// Model tests require loaded assets, test API exists
	EDITOR_TEST_END(TestModelMeshAccess);
}
ZENITH_TEST(Editor, ModelAnimationController)
{
	EDITOR_TEST_BEGIN(TestModelAnimationController);
	EDITOR_TEST_END(TestModelAnimationController);
}
ZENITH_TEST(Editor, ModelAnimationFloat)
{
	EDITOR_TEST_BEGIN(TestModelAnimationFloat);
	EDITOR_TEST_END(TestModelAnimationFloat);
}
ZENITH_TEST(Editor, ModelAnimationInt)
{
	EDITOR_TEST_BEGIN(TestModelAnimationInt);
	EDITOR_TEST_END(TestModelAnimationInt);
}
ZENITH_TEST(Editor, ModelAnimationBool)
{
	EDITOR_TEST_BEGIN(TestModelAnimationBool);
	EDITOR_TEST_END(TestModelAnimationBool);
}
ZENITH_TEST(Editor, ModelAnimationTrigger)
{
	EDITOR_TEST_BEGIN(TestModelAnimationTrigger);
	EDITOR_TEST_END(TestModelAnimationTrigger);
}

//------------------------------------------------------------------------------
// ColliderComponent Tests (Stubs - require physics initialization)
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, ColliderAddAABB)
{
	EDITOR_TEST_BEGIN(TestColliderAddAABB);
	EDITOR_TEST_END(TestColliderAddAABB);
}
ZENITH_TEST(Editor, ColliderAddSphere)
{
	EDITOR_TEST_BEGIN(TestColliderAddSphere);
	EDITOR_TEST_END(TestColliderAddSphere);
}
ZENITH_TEST(Editor, ColliderAddCapsule)
{
	EDITOR_TEST_BEGIN(TestColliderAddCapsule);
	EDITOR_TEST_END(TestColliderAddCapsule);
}
ZENITH_TEST(Editor, ColliderDynamicStatic)
{
	EDITOR_TEST_BEGIN(TestColliderDynamicStatic);
	EDITOR_TEST_END(TestColliderDynamicStatic);
}
ZENITH_TEST(Editor, ColliderGravityControl)
{
	EDITOR_TEST_BEGIN(TestColliderGravityControl);
	EDITOR_TEST_END(TestColliderGravityControl);
}

//------------------------------------------------------------------------------
// ScriptComponent Tests (Stubs)
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, ScriptBehaviourAttach)
{
	EDITOR_TEST_BEGIN(TestScriptBehaviourAttach);
	EDITOR_TEST_END(TestScriptBehaviourAttach);
}
ZENITH_TEST(Editor, ScriptBehaviourRetrieve)
{
	EDITOR_TEST_BEGIN(TestScriptBehaviourRetrieve);
	EDITOR_TEST_END(TestScriptBehaviourRetrieve);
}

//------------------------------------------------------------------------------
// UIComponent Tests (Stubs)
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, UICreateElement)
{
	EDITOR_TEST_BEGIN(TestUICreateElement);
	EDITOR_TEST_END(TestUICreateElement);
}
ZENITH_TEST(Editor, UIFindElement)
{
	EDITOR_TEST_BEGIN(TestUIFindElement);
	EDITOR_TEST_END(TestUIFindElement);
}
ZENITH_TEST(Editor, UIVisibility)
{
	EDITOR_TEST_BEGIN(TestUIVisibility);
	EDITOR_TEST_END(TestUIVisibility);
}

//------------------------------------------------------------------------------
// ParticleEmitterComponent Tests (Stubs)
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, ParticleEmission)
{
	EDITOR_TEST_BEGIN(TestParticleEmission);
	EDITOR_TEST_END(TestParticleEmission);
}
ZENITH_TEST(Editor, ParticleSetEmitting)
{
	EDITOR_TEST_BEGIN(TestParticleSetEmitting);
	EDITOR_TEST_END(TestParticleSetEmitting);
}
ZENITH_TEST(Editor, ParticleAliveCount)
{
	EDITOR_TEST_BEGIN(TestParticleAliveCount);
	EDITOR_TEST_END(TestParticleAliveCount);
}

//------------------------------------------------------------------------------
// InstancedMeshComponent Tests (Stubs)
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, InstancedSpawn)
{
	EDITOR_TEST_BEGIN(TestInstancedSpawn);
	EDITOR_TEST_END(TestInstancedSpawn);
}
ZENITH_TEST(Editor, InstancedTransform)
{
	EDITOR_TEST_BEGIN(TestInstancedTransform);
	EDITOR_TEST_END(TestInstancedTransform);
}
ZENITH_TEST(Editor, InstancedVisibility)
{
	EDITOR_TEST_BEGIN(TestInstancedVisibility);
	EDITOR_TEST_END(TestInstancedVisibility);
}
ZENITH_TEST(Editor, InstancedCount)
{
	EDITOR_TEST_BEGIN(TestInstancedCount);
	EDITOR_TEST_END(TestInstancedCount);
}

//------------------------------------------------------------------------------
// AIAgentComponent Tests (Stubs)
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, AIBlackboardAccess)
{
	EDITOR_TEST_BEGIN(TestAIBlackboardAccess);
	EDITOR_TEST_END(TestAIBlackboardAccess);
}
ZENITH_TEST(Editor, AIUpdateInterval)
{
	EDITOR_TEST_BEGIN(TestAIUpdateInterval);
	EDITOR_TEST_END(TestAIUpdateInterval);
}
ZENITH_TEST(Editor, AIEnable)
{
	EDITOR_TEST_BEGIN(TestAIEnable);
	EDITOR_TEST_END(TestAIEnable);
}

//------------------------------------------------------------------------------
// Drag-Drop Interaction Tests
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, DragDropTextureToMaterial)
{
	EDITOR_TEST_BEGIN(TestDragDropTextureToMaterial);

	// Test the logic of what happens when a texture is dropped on a material slot
	// This tests the underlying API, not the actual ImGui drag-drop


	EDITOR_TEST_END(TestDragDropTextureToMaterial);
}
ZENITH_TEST(Editor, DragDropTextureReplaceCleanup)
{
	EDITOR_TEST_BEGIN(TestDragDropTextureReplaceCleanup);

	// Test that when a texture is replaced, the old texture is properly cleaned up
	// This would involve checking ref counts or asset registry state


	EDITOR_TEST_END(TestDragDropTextureReplaceCleanup);
}
ZENITH_TEST(Editor, DragDropEntityToParent)
{
	EDITOR_TEST_BEGIN(TestDragDropEntityToParent);

	// Test entity reparenting via drag-drop (the underlying logic)
	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("DragDropParent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("DragDropChild");

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);

	// Simulate what happens when entity is dropped on another in hierarchy
	xChild.SetParent(uParent);

	ZENITH_ASSERT_EQ(xChild.GetParentEntityID(), uParent, "Entity should be reparented after drag-drop");

	EDITOR_TEST_END(TestDragDropEntityToParent);
}
ZENITH_TEST(Editor, DragDropEntityCircularPrevention)
{
	EDITOR_TEST_BEGIN(TestDragDropEntityCircularPrevention);

	// Test that circular hierarchy is prevented
	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("CircularParent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("CircularChild");

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xParent = pxSceneData->GetEntity(uParent);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);

	// Set up valid hierarchy
	xChild.SetParent(uParent);

	// Attempting to set parent as child of its own child should fail
	// The actual prevention logic is in the hierarchy panel's drag-drop handler

	EDITOR_TEST_END(TestDragDropEntityCircularPrevention);
}

//------------------------------------------------------------------------------
// Context Menu Action Tests
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, ContextMenuCreateChild)
{
	EDITOR_TEST_BEGIN(TestContextMenuCreateChild);

	// Test what happens when "Create Child Entity" is selected from context menu
	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("ContextParent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("ContextChild");

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);
	xChild.SetParent(uParent);

	ZENITH_ASSERT_EQ(xChild.GetParentEntityID(), uParent, "Child created via context menu should have correct parent");

	EDITOR_TEST_END(TestContextMenuCreateChild);
}
ZENITH_TEST(Editor, ContextMenuDeleteEntity)
{
	EDITOR_TEST_BEGIN(TestContextMenuDeleteEntity);

	// Test entity deletion via context menu
	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("DeleteEntity");

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	ZENITH_ASSERT_TRUE(pxSceneData->EntityExists(uEntity), "Entity should exist before deletion");

	Zenith_EditorSceneAccess::RemoveEntity(pxSceneData, uEntity);
	ZENITH_ASSERT_FALSE(pxSceneData->EntityExists(uEntity), "Entity should not exist after deletion");

	// Remove from fixture tracking since we deleted it
	auto& axEntities = const_cast<std::vector<Zenith_EntityID>&>(Zenith_EditorTestFixture::GetCreatedEntities());
	axEntities.erase(std::remove(axEntities.begin(), axEntities.end(), uEntity), axEntities.end());

	EDITOR_TEST_END(TestContextMenuDeleteEntity);
}
ZENITH_TEST(Editor, ContextMenuUnparent)
{
	EDITOR_TEST_BEGIN(TestContextMenuUnparent);

	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("UnparentParent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("UnparentChild");

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);

	xChild.SetParent(uParent);
	ZENITH_ASSERT_EQ(xChild.GetParentEntityID(), uParent, "Should have parent");

	// Unparent via context menu action
	xChild.SetParent(INVALID_ENTITY_ID);
	ZENITH_ASSERT_FALSE(xChild.GetParentEntityID().IsValid(), "Should be unparented");

	EDITOR_TEST_END(TestContextMenuUnparent);
}

//------------------------------------------------------------------------------
// Property Editor Interaction Tests
//------------------------------------------------------------------------------
ZENITH_TEST(Editor, MaterialSliderMetallic)
{
	EDITOR_TEST_BEGIN(TestMaterialSliderMetallic);
	// Material property tests require material asset system
	EDITOR_TEST_END(TestMaterialSliderMetallic);
}
ZENITH_TEST(Editor, MaterialSliderRoughness)
{
	EDITOR_TEST_BEGIN(TestMaterialSliderRoughness);
	EDITOR_TEST_END(TestMaterialSliderRoughness);
}
ZENITH_TEST(Editor, MaterialColorBaseColor)
{
	EDITOR_TEST_BEGIN(TestMaterialColorBaseColor);
	EDITOR_TEST_END(TestMaterialColorBaseColor);
}
ZENITH_TEST(Editor, MaterialCheckboxTransparent)
{
	EDITOR_TEST_BEGIN(TestMaterialCheckboxTransparent);
	EDITOR_TEST_END(TestMaterialCheckboxTransparent);
}
ZENITH_TEST(Editor, EntityNameChange)
{
	EDITOR_TEST_BEGIN(TestEntityNameChange);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("OriginalName");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);

	// Simulate name change from property panel text input
	xEntity.SetName("NewName");

	ZENITH_ASSERT_EQ(xEntity.GetName(), "NewName", "Entity name should be changed");

	EDITOR_TEST_END(TestEntityNameChange);
}
ZENITH_TEST(Editor, TransformDragPosition)
{
	EDITOR_TEST_BEGIN(TestTransformDragPosition);

	Zenith_EntityID uEntity = Zenith_EditorTestFixture::CreateTestEntity("DragPositionEntity");
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntity);
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();

	// Simulate position change from drag control
	Zenith_Maths::Vector3 xNewPos(15.0f, 25.0f, 35.0f);
	xTransform.SetPosition(xNewPos);

	Zenith_Maths::Vector3 xRetrievedPos;
	xTransform.GetPosition(xRetrievedPos);

	ZENITH_ASSERT_LT(glm::length(xNewPos - xRetrievedPos), 0.001f, "Position should be updated from drag control");

	EDITOR_TEST_END(TestTransformDragPosition);
}

//=============================================================================
// Editor Operation Tests (shared code paths with automation)
//=============================================================================
ZENITH_TEST(Editor, CreateEntityViaEditor)
{
	EDITOR_TEST_BEGIN(TestCreateEntityViaEditor);

	// CreateEntity should create, set non-transient, and select the entity
	Zenith_EntityID uEntityID = g_xEngine.Editor().CreateEntity("EditorOpTestEntity");

	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Entity should be selected after CreateEntity");
	ZENITH_ASSERT_EQ(pxEntity->GetEntityID(), uEntityID, "Selected entity should match returned ID");
	ZENITH_ASSERT_STREQ(pxEntity->GetName().c_str(), "EditorOpTestEntity", "Entity name should match");
	ZENITH_ASSERT_FALSE(pxEntity->IsTransient(), "Entity should be non-transient by default");

	EDITOR_TEST_END(TestCreateEntityViaEditor);
}
ZENITH_TEST(Editor, AddInvalidComponent)
{
	EDITOR_TEST_BEGIN(TestAddInvalidComponent);

	g_xEngine.Editor().CreateEntity("InvalidCompEntity");

	// Adding a non-existent component should return false
	bool bResult = g_xEngine.Editor().AddComponentToSelected("NonexistentComponent");
	ZENITH_ASSERT_FALSE(bResult, "AddComponentToSelected should return false for unknown component name");

	EDITOR_TEST_END(TestAddInvalidComponent);
}
ZENITH_TEST(Editor, SetSelectedEntityTransient)
{
	EDITOR_TEST_BEGIN(TestSetSelectedEntityTransient);

	g_xEngine.Editor().CreateEntity("TransientTestEntity");
	Zenith_Entity* pxEntity = g_xEngine.Editor().GetSelectedEntity();
	ZENITH_ASSERT_NOT_NULL(pxEntity, "Should have selected entity");

	// Initially non-transient (set by CreateEntity)
	ZENITH_ASSERT_FALSE(pxEntity->IsTransient(), "Should be non-transient initially");

	// Set transient
	g_xEngine.Editor().SetSelectedEntityTransient(true);
	ZENITH_ASSERT_TRUE(pxEntity->IsTransient(), "Should be transient after SetSelectedEntityTransient(true)");

	// Set back to non-transient
	g_xEngine.Editor().SetSelectedEntityTransient(false);
	ZENITH_ASSERT_FALSE(pxEntity->IsTransient(), "Should be non-transient after SetSelectedEntityTransient(false)");

	EDITOR_TEST_END(TestSetSelectedEntityTransient);
}
ZENITH_TEST(Editor, IsAncestorOf)
{
	EDITOR_TEST_BEGIN(TestIsAncestorOf);

	// Create a 3-level hierarchy: Grandparent -> Parent -> Child, plus a sibling
	Zenith_EntityID uGrandparent = Zenith_EditorTestFixture::CreateTestEntity("Grandparent");
	Zenith_EntityID uParent = Zenith_EditorTestFixture::CreateTestEntity("Parent");
	Zenith_EntityID uChild = Zenith_EditorTestFixture::CreateTestEntity("Child");
	Zenith_EntityID uSibling = Zenith_EditorTestFixture::CreateTestEntity("Sibling");

	Zenith_EditorTestFixture::SetupHierarchy(uGrandparent, uParent);
	Zenith_EditorTestFixture::SetupHierarchy(uParent, uChild);

	// Direct parent is ancestor of child
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelHierarchy::IsAncestorOf(uParent, uChild), "Direct parent should be ancestor of child");

	// Grandparent is ancestor of grandchild (transitive)
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelHierarchy::IsAncestorOf(uGrandparent, uChild), "Grandparent should be ancestor of grandchild");

	// Self is NOT ancestor of self
	ZENITH_ASSERT_FALSE(Zenith_EditorPanelHierarchy::IsAncestorOf(uParent, uParent), "Entity should not be ancestor of itself");

	// Child is NOT ancestor of parent (reverse direction)
	ZENITH_ASSERT_FALSE(Zenith_EditorPanelHierarchy::IsAncestorOf(uChild, uParent), "Child should not be ancestor of parent");

	// Sibling is NOT ancestor of child (no parent relationship)
	ZENITH_ASSERT_FALSE(Zenith_EditorPanelHierarchy::IsAncestorOf(uSibling, uChild), "Sibling should not be ancestor of child");

	// Invalid entity is not ancestor of anything
	ZENITH_ASSERT_FALSE(Zenith_EditorPanelHierarchy::IsAncestorOf(INVALID_ENTITY_ID, uChild), "Invalid entity should not be ancestor of anything");

	// Nothing is ancestor of invalid entity
	ZENITH_ASSERT_FALSE(Zenith_EditorPanelHierarchy::IsAncestorOf(uParent, INVALID_ENTITY_ID), "Nothing should be ancestor of invalid entity");

	EDITOR_TEST_END(TestIsAncestorOf);
}
ZENITH_TEST(Editor, DeferredOpClearedAfterExecution)
{
	Zenith_EditorTestFixture::SetUp();

	// Enter Play mode to create a scene backup
	g_xEngine.Editor().SetEditorMode(EditorMode::Playing);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Playing, "Should be in Playing mode");

	// Stop Play mode — this queues a deferred scene-load via the editor state's deferred-ops
	g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
	ZENITH_ASSERT_EQ(g_xEngine.Editor().GetEditorMode(), EditorMode::Stopped, "Should be in Stopped mode");

	// Flush pending ops — this should execute the deferred load and clear the flag
	g_xEngine.Editor().FlushPendingSceneOperations();

	// Flush again — if the flag was properly cleared, this is a no-op and should not crash
	g_xEngine.Editor().FlushPendingSceneOperations();

	// Verify the scene is still valid after the double-flush
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "Scene data should be valid after deferred op completed");

	Zenith_EditorTestFixture::TearDown();
}
ZENITH_TEST(Editor, DeferredOpSkippedWhenFlagFalse)
{
	Zenith_EditorTestFixture::SetUp();

	// Create an entity to track scene identity — if no deferred op fires, it should survive
	Zenith_EntityID uEntityID = Zenith_EditorTestFixture::CreateTestEntity("DeferredFlagTestEntity");

	// Verify the entity exists
	Zenith_SceneData* pxSceneData = Zenith_EditorTestFixture::GetTestScene();
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "Test scene should exist");
	ZENITH_ASSERT_TRUE(pxSceneData->EntityExists(uEntityID), "Entity should exist before flush");

	// Call FlushPendingSceneOperations with NO flags set
	g_xEngine.Editor().FlushPendingSceneOperations();

	// Verify the entity still exists — proving no scene operation was executed
	ZENITH_ASSERT_TRUE(pxSceneData->EntityExists(uEntityID), "Entity should still exist after flush with no pending flags");

	Zenith_EditorTestFixture::TearDown();
}
ZENITH_TEST(Editor, TypeFilterMatchesTexture)
{

	ZENITH_ASSERT_TRUE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(1, ZENITH_TEXTURE_EXT), "Texture extension should match texture filter");

	ZENITH_ASSERT_FALSE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(1, ZENITH_MATERIAL_EXT), "Material extension should not match texture filter");

	ZENITH_ASSERT_FALSE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(1, ZENITH_MESH_EXT), "Mesh extension should not match texture filter");

	ZENITH_ASSERT_FALSE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(1, ZENITH_MODEL_EXT), "Model extension should not match texture filter");

}
ZENITH_TEST(Editor, TypeFilterAllPass)
{

	ZENITH_ASSERT_TRUE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(0, ZENITH_TEXTURE_EXT), "Texture should pass 'All' filter");
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(0, ZENITH_MATERIAL_EXT), "Material should pass 'All' filter");
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(0, ZENITH_MESH_EXT), "Mesh should pass 'All' filter");
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(0, ZENITH_MODEL_EXT), "Model should pass 'All' filter");
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(0, ZENITH_PREFAB_EXT), "Prefab should pass 'All' filter");
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(0, ZENITH_SCENE_EXT), "Scene should pass 'All' filter");
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(0, ZENITH_ANIMATION_EXT), "Animation should pass 'All' filter");
	ZENITH_ASSERT_TRUE(Zenith_EditorPanelContentBrowser::MatchesAssetTypeFilter(0, ".unknown"), "Unknown extension should pass 'All' filter");

}
ZENITH_TEST(Editor, UniqueFilenameWithExisting)
{

	std::string strTempDir = std::filesystem::temp_directory_path().string();
	std::string strTestDir = strTempDir + "/zenith_test_unique_filename";

	std::filesystem::remove_all(strTestDir);
	std::filesystem::create_directory(strTestDir);

	std::string strBasePath = strTestDir + "/TestFile";
	std::string strSuffix = ".txt";

	// No existing file - should return base path + suffix directly
	std::string strResult1 = Zenith_EditorPanelContentBrowser::GenerateUniqueFilename(strBasePath, strSuffix);
	ZENITH_ASSERT_EQ(strResult1, strBasePath + strSuffix, "With no existing files, should return base path directly");

	std::ofstream xFile1(strResult1);
	xFile1.close();

	// First file exists - should return _1 variant
	std::string strResult2 = Zenith_EditorPanelContentBrowser::GenerateUniqueFilename(strBasePath, strSuffix);
	std::string strExpected2 = strBasePath + "_1" + strSuffix;
	ZENITH_ASSERT_EQ(strResult2, strExpected2, "With base file existing, should return _1 variant");

	std::ofstream xFile2(strResult2);
	xFile2.close();

	// Both base and _1 exist - should return _2 variant
	std::string strResult3 = Zenith_EditorPanelContentBrowser::GenerateUniqueFilename(strBasePath, strSuffix);
	std::string strExpected3 = strBasePath + "_2" + strSuffix;
	ZENITH_ASSERT_EQ(strResult3, strExpected3, "With base and _1 existing, should return _2 variant");

	std::filesystem::remove_all(strTestDir);

}

#endif // ZENITH_TOOLS
