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
};

#endif // ZENITH_TOOLS
