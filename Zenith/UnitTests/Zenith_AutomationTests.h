#pragma once

#ifdef ZENITH_TOOLS

class Zenith_AutomationTests
{
public:
	static void RunAllTests();

private:
	//--------------------------------------------------------------------------
	// State Machine Tests
	//--------------------------------------------------------------------------
	static void TestInitialState();
	static void TestBeginSetsRunning();
	static void TestResetClearsState();

	//--------------------------------------------------------------------------
	// Step Execution Tests
	//--------------------------------------------------------------------------
	static void TestStepExecutionOrder();
	static void TestExecuteEmptyQueue();
	static void TestCompletionAfterAllSteps();

	//--------------------------------------------------------------------------
	// Entity Operation Tests
	//--------------------------------------------------------------------------
	static void TestCreateEntityStep();
	static void TestEntitySelectionTracking();

	//--------------------------------------------------------------------------
	// Component Operation Tests
	//--------------------------------------------------------------------------
	static void TestAddComponentStep();

	//--------------------------------------------------------------------------
	// Transform Operation Tests
	//--------------------------------------------------------------------------
	static void TestSetTransformPositionStep();
	static void TestSetTransformScaleStep();

	//--------------------------------------------------------------------------
	// Camera Operation Tests
	//--------------------------------------------------------------------------
	static void TestSetCameraFOVStep();
	static void TestSetCameraPitchYawStep();
	static void TestSetCameraPositionStep();
	static void TestSetAsMainCameraStep();

	//--------------------------------------------------------------------------
	// Negative Path Tests
	//--------------------------------------------------------------------------
	static void TestAddInvalidComponentStep();

	//--------------------------------------------------------------------------
	// Custom Step Tests
	//--------------------------------------------------------------------------
	static void TestCustomStepExecution();

	//--------------------------------------------------------------------------
	// Scene Lifecycle Tests
	//--------------------------------------------------------------------------
	static void TestCreateSaveUnloadCycle();

	//--------------------------------------------------------------------------
	// UI Operation Tests
	//--------------------------------------------------------------------------
	static void TestCreateUITextStep();
	static void TestCreateUIButtonStep();
	static void TestCreateUIRectStep();
	static void TestSetUIPropertiesStep();
	static void TestSetUIButtonStyleStep();

	//--------------------------------------------------------------------------
	// UI Image Operation Tests
	//--------------------------------------------------------------------------
	static void TestCreateUIImageStep();
	static void TestSetUIImageTexturePathStep();

	//--------------------------------------------------------------------------
	// Particle Config By Name Tests
	//--------------------------------------------------------------------------
	static void TestSetParticleConfigByNameStep();

	//--------------------------------------------------------------------------
	// Script/Behaviour Tests
	//--------------------------------------------------------------------------
	static void TestSetBehaviourStep();
	static void TestSetBehaviourForSerializationStep();

	//--------------------------------------------------------------------------
	// Camera Extended Tests
	//--------------------------------------------------------------------------
	static void TestSetCameraNearFarAspectStep();

	//--------------------------------------------------------------------------
	// Scene Round-Trip Tests
	//--------------------------------------------------------------------------
	static void TestSceneSaveLoadRoundTrip();

	//--------------------------------------------------------------------------
	// Edge Case Tests
	//--------------------------------------------------------------------------
	static void TestResetDuringExecution();
	static void TestBeginWithZeroSteps();
	static void TestDoubleBeginWithoutReset();

	//--------------------------------------------------------------------------
	// UIStyle Automation Tests
	//--------------------------------------------------------------------------
	static void TestSetUICornerRadiusStep();
	static void TestSetUIGradientColorStep();
	static void TestSetUIShadowStep();
	static void TestSetUIShadowColorStep();
	static void TestSetUIRectBorderStep();
	static void TestSetUITextShadowStep();
	static void TestSetUITextShadowColorStep();
	static void TestSetUIButtonCornerRadiusStep();
	static void TestSetUIButtonShadowStep();
	static void TestSetUIButtonShadowColorStep();
	static void TestSetUIButtonGradientColorStep();
	static void TestSetUIButtonBorderColorStep();
	static void TestSetUIButtonBorderThicknessStep();
	static void TestSetUIButtonTransitionDurationStep();
	static void TestSetUIButtonTextShadowStep();
	static void TestSetUIButtonTextShadowColorStep();

	//--------------------------------------------------------------------------
	// Group Alpha Tests
	//--------------------------------------------------------------------------
	static void TestGroupAlphaDefault();
	static void TestGroupAlphaPropagation();
	static void TestGroupInteractableDefault();
	static void TestGroupInteractableParentDisabled();

	//--------------------------------------------------------------------------
	// UIElement Background Tests
	//--------------------------------------------------------------------------
	static void TestSetUIBackgroundColorStep();
	static void TestSetUIBackgroundCornerRadiusStep();
	static void TestSetUIBackgroundBorderStep();

	//--------------------------------------------------------------------------
	// Button Transition Tests
	//--------------------------------------------------------------------------
	static void TestButtonTransitionInitialState();

	//--------------------------------------------------------------------------
	// Layout Group Tests
	//--------------------------------------------------------------------------
	static void TestCreateUILayoutGroupStep();
	static void TestAddUIChildStep();
	static void TestSetUILayoutDirectionStep();
	static void TestSetUILayoutSpacingStep();
	static void TestSetUILayoutChildAlignmentStep();
	static void TestSetUILayoutPaddingStep();
	static void TestSetUILayoutFitToContentStep();
	static void TestSetUILayoutChildForceExpandStep();
	static void TestSetUILayoutReverseStep();
	static void TestLayoutHorizontalPositioning();
	static void TestLayoutVerticalPositioning();
	static void TestLayoutPaddingAffectsPositioning();
	static void TestLayoutMiddleCenterAlignment();
	static void TestLayoutUpperLeftAlignment();
	static void TestLayoutLowerRightAlignment();
	static void TestLayoutReverseArrangement();
	static void TestLayoutChildForceExpand();
	static void TestLayoutFitToContentResizing();
	static void TestLayoutWithTextChild();
	static void TestLayoutEmptyGroup();
	static void TestLayoutSingleChild();
	static void TestLayoutInvisibleChildrenSkipped();
	static void TestLayoutSerializationRoundTrip();
};

#endif // ZENITH_TOOLS
