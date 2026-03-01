#pragma once

#ifdef ZENITH_INPUT_SIMULATOR

#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

class Zenith_InputSimulator
{
public:
	// Lifecycle
	static void Enable();
	static void Disable();
	static bool IsEnabled();

	// Frame control
	static void StepFrame();
	static void StepFrames(u_int32 uCount);
	static void StepFramesWithFixedDt(u_int32 uCount, float fDt);
	static void StepUntil(bool (*pfnCondition)(), u_int32 uMaxFrames);

	// Key simulation
	static void SimulateKeyDown(Zenith_KeyCode eKey);
	static void SimulateKeyUp(Zenith_KeyCode eKey);
	static void SimulateKeyPress(Zenith_KeyCode eKey);
	static void SimulateKeySequence(const Zenith_KeyCode* aeKeys, u_int32 uCount, u_int32 uFramesBetween);

	// Mouse simulation
	static void SimulateMousePosition(double fScreenX, double fScreenY);
	static void SimulateMouseButtonDown(Zenith_KeyCode eButton);
	static void SimulateMouseButtonUp(Zenith_KeyCode eButton);
	static void SimulateMouseClick(double fScreenX, double fScreenY);
	static void SimulateMouseDrag(double fStartX, double fStartY, double fEndX, double fEndY, u_int32 uDurationFrames);

	// Touch simulation (uses mouse emulation, matching Zenith_TouchInput's approach)
	static void SimulateTap(double fScreenX, double fScreenY);
	static void SimulateSwipe(double fStartX, double fStartY, double fEndX, double fEndY, u_int32 uDurationFrames);

	// Key state manipulation (for unit tests that need explicit held-key control)
	static void SetKeyHeld(Zenith_KeyCode eKey, bool bHeld);
	static void ClearHeldKeys();

	// State reset
	static void ResetAllInputState();

	// Test frame lifecycle helpers
	static void BeginTestFrame();
	static void EndTestFrame();

	// Fixed dt override (called by Zenith_Core::UpdateTimers when enabled)
	static bool HasFixedDtOverride();
	static float GetFixedDt();

	// Query simulated state (called by Zenith_Input when simulation is enabled)
	static bool IsKeyDownSimulated(Zenith_KeyCode eKey);
	static bool WasKeyPressedThisFrameSimulated(Zenith_KeyCode eKey);
	static void GetMousePositionSimulated(Zenith_Maths::Vector2_64& xOut);

	// Frame lifecycle (called by Zenith_Input::BeginFrame)
	static void ProcessAutoReleases();

private:
	static bool s_bEnabled;
	static bool s_bFixedDtEnabled;
	static float s_fFixedDt;

	static constexpr u_int32 uMAX_SIMULATED_KEYS = 512;
	static bool s_abKeyState[uMAX_SIMULATED_KEYS];
	static bool s_abKeyPressedThisFrame[uMAX_SIMULATED_KEYS];
	static Zenith_Maths::Vector2_64 s_xMousePosition;

	static constexpr u_int32 uMAX_AUTO_RELEASE = 32;
	static Zenith_KeyCode s_aeAutoReleaseKeys[uMAX_AUTO_RELEASE];
	static u_int32 s_uAutoReleaseCount;
};

#endif // ZENITH_INPUT_SIMULATOR
