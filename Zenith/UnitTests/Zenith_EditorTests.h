#pragma once

#ifdef ZENITH_TOOLS

class Zenith_EditorTests
{
public:
	static void RunAllTests();

private:
	//--------------------------------------------------------------------------
	// Existing Selection System Tests
	//--------------------------------------------------------------------------
	static void TestBoundingBoxIntersection();
	static void TestSelectionSystemEmptyScene();
	static void TestInvalidEntityID();
	static void TestTransformRoundTrip();

	// Multi-select tests (existing)
	static void TestMultiSelectSingle();
	static void TestMultiSelectCtrlClick();
	static void TestMultiSelectClear();
	static void TestMultiSelectAfterEntityDelete();

	//--------------------------------------------------------------------------
	// Mode Transition Tests
	//--------------------------------------------------------------------------
	static void TestModeTransitionStoppedToPlaying();
	static void TestModeTransitionPlayingToPaused();
	static void TestModeTransitionPausedToStopped();
	static void TestModeTransitionFullCycle();
	static void TestModePreservesSelection();

	//--------------------------------------------------------------------------
	// Gizmo Operation Tests
	//--------------------------------------------------------------------------
	static void TestGizmoModeSwitch();
	static void TestGizmoModeTranslate();
	static void TestGizmoModeRotate();
	static void TestGizmoModeScale();
	static void TestGizmoModeViaKeyboard();

	//--------------------------------------------------------------------------
	// Undo/Redo System Tests
	//--------------------------------------------------------------------------
	static void TestUndoSystemCanUndo();
	static void TestUndoSystemCanRedo();
	static void TestTransformEditUndoRedo();
	static void TestUndoStackClearOnSceneChange();
	static void TestRedoStackClearOnNewEdit();

	//--------------------------------------------------------------------------
	// Entity Hierarchy Tests
	//--------------------------------------------------------------------------
	static void TestEntityReparenting();
	static void TestCreateChildEntity();
	static void TestUnparentEntity();
	static void TestHierarchyCircularPrevention();
	static void TestDeleteParentWithChildren();

	//--------------------------------------------------------------------------
	// Selection System Tests (expanded)
	//--------------------------------------------------------------------------
	static void TestRangeSelection();
	static void TestSelectionWithHierarchy();
	static void TestRaycastSelectWithMultipleEntities();

	//--------------------------------------------------------------------------
	// Console Tests
	//--------------------------------------------------------------------------
	static void TestConsoleAddLog();
	static void TestConsoleClear();

	//--------------------------------------------------------------------------
	// Component Tests - Generic
	//--------------------------------------------------------------------------
	static void TestComponentAddRemove();
	static void TestComponentAddViaRegistry();
	static void TestMultipleComponentAdd();

	//--------------------------------------------------------------------------
	// TransformComponent Tests
	//--------------------------------------------------------------------------
	static void TestTransformPositionRoundTrip();
	static void TestTransformRotationRoundTrip();
	static void TestTransformScaleRoundTrip();
	static void TestTransformMatrixBuild();
	static void TestTransformParentChild();
	static void TestTransformHierarchyTraversal();
	static void TestTransformIsDescendantOf();

	//--------------------------------------------------------------------------
	// CameraComponent Tests
	//--------------------------------------------------------------------------
	static void TestCameraPerspectiveMatrix();
	static void TestCameraOrthographicMatrix();
	static void TestCameraViewMatrix();
	static void TestCameraTypeSwitch();
	static void TestCameraNearFarPlanes();
	static void TestCameraFOVSettings();

	//--------------------------------------------------------------------------
	// ModelComponent Tests
	//--------------------------------------------------------------------------
	static void TestModelMeshAccess();
	static void TestModelAnimationController();
	static void TestModelAnimationFloat();
	static void TestModelAnimationInt();
	static void TestModelAnimationBool();
	static void TestModelAnimationTrigger();

	//--------------------------------------------------------------------------
	// ColliderComponent Tests
	//--------------------------------------------------------------------------
	static void TestColliderAddAABB();
	static void TestColliderAddSphere();
	static void TestColliderAddCapsule();
	static void TestColliderDynamicStatic();
	static void TestColliderGravityControl();

	//--------------------------------------------------------------------------
	// ScriptComponent Tests
	//--------------------------------------------------------------------------
	static void TestScriptBehaviourAttach();
	static void TestScriptBehaviourRetrieve();

	//--------------------------------------------------------------------------
	// UIComponent Tests
	//--------------------------------------------------------------------------
	static void TestUICreateElement();
	static void TestUIFindElement();
	static void TestUIVisibility();

	//--------------------------------------------------------------------------
	// ParticleEmitterComponent Tests
	//--------------------------------------------------------------------------
	static void TestParticleEmission();
	static void TestParticleSetEmitting();
	static void TestParticleAliveCount();

	//--------------------------------------------------------------------------
	// InstancedMeshComponent Tests
	//--------------------------------------------------------------------------
	static void TestInstancedSpawn();
	static void TestInstancedTransform();
	static void TestInstancedVisibility();
	static void TestInstancedCount();

	//--------------------------------------------------------------------------
	// AIAgentComponent Tests
	//--------------------------------------------------------------------------
	static void TestAIBlackboardAccess();
	static void TestAIUpdateInterval();
	static void TestAIEnable();

	//--------------------------------------------------------------------------
	// Drag-Drop Interaction Tests
	//--------------------------------------------------------------------------
	static void TestDragDropTextureToMaterial();
	static void TestDragDropTextureReplaceCleanup();
	static void TestDragDropEntityToParent();
	static void TestDragDropEntityCircularPrevention();

	//--------------------------------------------------------------------------
	// Context Menu Action Tests
	//--------------------------------------------------------------------------
	static void TestContextMenuCreateChild();
	static void TestContextMenuDeleteEntity();
	static void TestContextMenuUnparent();

	//--------------------------------------------------------------------------
	// Property Editor Interaction Tests
	//--------------------------------------------------------------------------
	static void TestMaterialSliderMetallic();
	static void TestMaterialSliderRoughness();
	static void TestMaterialColorBaseColor();
	static void TestMaterialCheckboxTransparent();
	static void TestEntityNameChange();
	static void TestTransformDragPosition();

	//--------------------------------------------------------------------------
	// Editor Operation Tests (shared code paths with automation)
	//--------------------------------------------------------------------------
	static void TestCreateEntityViaEditor();
	static void TestAddInvalidComponent();
	static void TestSetSelectedEntityTransient();
};

#endif // ZENITH_TOOLS
