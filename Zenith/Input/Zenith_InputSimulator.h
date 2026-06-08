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

	// Mouse wheel simulation (EXT-4). Stays asserted for one full frame —
	// EndTestFrame clears it, mirroring SimulateKeyPress semantics.
	static void SimulateMouseWheel(float fDelta);

	// NOTE: the UI-element click helper that resolved a named element to its
	// screen center and clicked it formerly lived here. It pulled UI/Zenith_UICanvas.h
	// into this L1 Input TU (an illegal layer-up dependency). It has been relocated
	// to the UI layer as Zenith_UI::Zenith_UICanvas::SimulateClickOnUIElement, which
	// may legitimately include UICanvas and call back DOWN into SimulateMouseClick.

	// Key state manipulation (for unit tests that need explicit held-key control)
	static void SetKeyHeld(Zenith_KeyCode eKey, bool bHeld);
	static void ClearHeldKeys();

	// State reset
	static void ResetAllInputState();

	// Test frame lifecycle helpers
	static void BeginTestFrame();
	static void EndTestFrame();

	// Fixed dt override (called by Zenith_Core::UpdateTimers when enabled)
	static void SetFixedDt(float fDt) { s_bFixedDtEnabled = true; s_fFixedDt = fDt; }
	static void ClearFixedDt() { s_bFixedDtEnabled = false; }
	static bool HasFixedDtOverride();
	static float GetFixedDt();

	// Query simulated state (called by Zenith_Input when simulation is enabled)
	static bool IsKeyDownSimulated(Zenith_KeyCode eKey);
	static bool WasKeyPressedThisFrameSimulated(Zenith_KeyCode eKey);
	static void GetMousePositionSimulated(Zenith_Maths::Vector2_64& xOut);
	static float GetMouseWheelDeltaSimulated();

	// Frame lifecycle (called by g_xEngine.Input().BeginFrame)
	static void ProcessAutoReleases();

	// Called once per main-loop iteration AFTER the scene/script update
	// phase, by Zenith_Core::Zenith_MainLoop. Clears the simulator's
	// single-frame mouse-wheel delta so a SimulateMouseWheel() value lives
	// for exactly one frame regardless of whether it was queued in Setup
	// (before tick 0) or inside a Step (mid-tick).
	static void EndOfFrameTickComplete();

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

	// Mouse wheel simulation state (EXT-4)
	static float s_fMouseWheelDelta;
};

#endif // ZENITH_INPUT_SIMULATOR
