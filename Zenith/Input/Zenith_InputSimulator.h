#pragma once

#ifdef ZENITH_INPUT_SIMULATOR

#include "Input/Zenith_KeyCodes.h"
#include "Input/Zenith_Input.h"
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

	// Raw pointer-stream injection. Unlike SimulateTap/SimulateSwipe (which are
	// mouse emulation) these queue real TOUCH_* events that reach the staged
	// touch stream and the B3 projection at ApplyPendingInjection.
	static void SimulateTouchDown(int32_t iPointerId, float fX, float fY);
	static void SimulateTouchMove(int32_t iPointerId, float fX, float fY);
	static void SimulateTouchUp(int32_t iPointerId, float fX, float fY);
	static void SimulateTouchCancel(int32_t iPointerId, float fX, float fY);

	// Mouse wheel simulation (EXT-4). Stays asserted for one full frame —
	// EndTestFrame clears it, mirroring SimulateKeyPress semantics.
	static void SimulateMouseWheel(float fDelta);

	// Gamepad simulation. Values are CANONICAL: sticks [-1,1] (clamped),
	// triggers [0,1] with REST 0 (clamped) — the same domains the device layer
	// produces, so a test never has to know GLFW's [-1,1] trigger encoding.
	static void SimulateGamepadConnected(bool bConnected, int iGamepad = 0);
	static void SimulateGamepadButtonDown(int iButton, int iGamepad = 0);
	static void SimulateGamepadButtonUp(int iButton, int iGamepad = 0);
	static void SimulateGamepadAxis(int iAxis, float fValue, int iGamepad = 0);
	// iAxisX is the X axis of the pair (ZENITH_GAMEPAD_AXIS_LEFT_X or _RIGHT_X);
	// the Y axis of the same stick is iAxisX + 1.
	static void SimulateGamepadStick(int iAxisX, float fX, float fY, int iGamepad = 0);

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
	static bool WasKeyReleasedThisFrameSimulated(Zenith_KeyCode eKey);
	static void GetMousePositionSimulated(Zenith_Maths::Vector2_64& xOut);
	static float GetMouseWheelDeltaSimulated();
	static bool IsGamepadConnectedSimulated(int iGamepad);
	static bool IsGamepadButtonDownSimulated(int iButton, int iGamepad);
	static bool WasGamepadButtonPressedThisFrameSimulated(int iButton, int iGamepad);
	static bool WasGamepadButtonReleasedThisFrameSimulated(int iButton, int iGamepad);
	// RAW (pre-deadzone) canonical axis value; Zenith_Input applies the radial
	// stick deadzone at the getter for simulated and real input alike.
	static float GetGamepadAxisSimulated(int iAxis, int iGamepad);

	// Frame lifecycle (called by g_xEngine.Input().BeginFrame)
	static void ProcessAutoReleases();

	// Frame boundary, called by g_xEngine.Input().BeginFrame BEFORE
	// ProcessAutoReleases: drops anything queued outside a main-loop tick (a
	// unit test driving components by hand never reaches step 7, and its
	// injections would otherwise accumulate until the queue overflowed).
	static void DiscardPendingInjections() { s_uPendingInjectionCount = 0; }

	// Called once per main-loop iteration AFTER the scene/script update
	// phase, by Zenith_Core::Zenith_MainLoop. Clears the simulator's
	// single-frame mouse-wheel delta so a SimulateMouseWheel() value lives
	// for exactly one frame regardless of whether it was queued in Setup
	// (before tick 0) or inside a Step (mid-tick).
	static void EndOfFrameTickComplete();

	// Step 7 of the frame contract. Everything a test Step injected this frame is
	// PULLED from here by Zenith_Input::ApplySimulatorInjection and appended to
	// the transition log / staged touch stream / held tables, so the action layer
	// sees simulated and real input through exactly one path. The direction is
	// deliberate: the simulator must not reach the engine singleton, and pulling
	// keeps this an Input-layer concern with no dependency on Core.
	static constexpr u_int32 uMAX_PENDING_INJECTIONS = 128;
	static u_int32 GetPendingInjectionCount() { return s_uPendingInjectionCount; }
	static const Zenith_InputEvent& GetPendingInjection(u_int32 uIndex) { return s_axPendingInjections[uIndex]; }

private:
	static void QueueInjection(const Zenith_InputEvent& xEvent);
	static void QueueTouchInjection(Zenith_InputEventType eType, int32_t iPointerId, float fX, float fY);

	static bool s_bEnabled;
	static bool s_bFixedDtEnabled;
	static float s_fFixedDt;

	static constexpr u_int32 uMAX_SIMULATED_KEYS = 512;
	static bool s_abKeyState[uMAX_SIMULATED_KEYS];
	static bool s_abKeyPressedThisFrame[uMAX_SIMULATED_KEYS];
	static bool s_abKeyReleasedThisFrame[uMAX_SIMULATED_KEYS];
	static Zenith_Maths::Vector2_64 s_xMousePosition;

	static constexpr u_int32 uMAX_AUTO_RELEASE = 32;
	static Zenith_KeyCode s_aeAutoReleaseKeys[uMAX_AUTO_RELEASE];
	static u_int32 s_uAutoReleaseCount;

	// Mouse wheel simulation state (EXT-4)
	static float s_fMouseWheelDelta;

	// Gamepad simulation state (canonical domains; see SimulateGamepadAxis)
	static constexpr int iMAX_SIMULATED_GAMEPADS = Zenith_Input::MAX_GAMEPADS;
	static constexpr int iSIM_PAD_BUTTON_COUNT   = Zenith_GamepadSnapshot::iBUTTON_COUNT;
	static constexpr int iSIM_PAD_AXIS_COUNT     = Zenith_GamepadSnapshot::iAXIS_COUNT;
	static bool  s_abPadConnected[iMAX_SIMULATED_GAMEPADS];
	static bool  s_aabPadButtons[iMAX_SIMULATED_GAMEPADS][iSIM_PAD_BUTTON_COUNT];
	static bool  s_aabPadPressedThisFrame[iMAX_SIMULATED_GAMEPADS][iSIM_PAD_BUTTON_COUNT];
	static bool  s_aabPadReleasedThisFrame[iMAX_SIMULATED_GAMEPADS][iSIM_PAD_BUTTON_COUNT];
	static float s_aafPadAxes[iMAX_SIMULATED_GAMEPADS][iSIM_PAD_AXIS_COUNT];

	// Events injected this frame, applied to Zenith_Input at step 7.
	static Zenith_InputEvent s_axPendingInjections[uMAX_PENDING_INJECTIONS];
	static u_int32 s_uPendingInjectionCount;
};

#endif // ZENITH_INPUT_SIMULATOR
