#pragma once

#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

#ifdef ZENITH_WINDOWS
#include "GLFW/glfw3.h"
#endif

// State + behaviour for the Input subsystem. Held on g_xEngine and
// accessed via g_xEngine.Input().
class Zenith_Input
{
public:
	Zenith_Input() = default;
	~Zenith_Input() = default;

	Zenith_Input(const Zenith_Input&) = delete;
	Zenith_Input& operator=(const Zenith_Input&) = delete;

	void BeginFrame();

	// Pure (window-free) core of BeginFrame's mouse-delta update: given the current
	// cursor position, computes m_xMouseDelta + advances m_xLastMousePosition, applying
	// the first-frame / left-sim / discontinuity one-shot skip. BeginFrame supplies the
	// live GLFW position; unit tests supply a synthetic one to verify the one-shot
	// discontinuity semantics without a window.
	void UpdateMouseDeltaFromPosition(const Zenith_Maths::Vector2_64& xCurrentMousePos, bool bJustLeftSimMode);

	// Signal that the OS cursor position jumped discontinuously this frame (e.g. the
	// window just switched GLFW_CURSOR between NORMAL and DISABLED, which re-centres /
	// teleports the cursor). The NEXT BeginFrame zeroes that frame's mouse delta and
	// resyncs the last-position baseline — mirroring the input-domain (simulator) skip
	// — so the switch doesn't produce a one-frame look spike. One-shot.
	void NotifyMouseDiscontinuity() { m_bMouseDiscontinuity = true; }

	void KeyPressedCallback(Zenith_KeyCode iKey);
	void MouseButtonPressedCallback(Zenith_KeyCode iKey);
	void GetMousePosition(Zenith_Maths::Vector2_64& xOut);
	void GetMouseDelta(Zenith_Maths::Vector2_64& xOut);
	bool IsKeyDown(Zenith_KeyCode iKey);
	bool IsMouseButtonHeld(Zenith_KeyCode iMouseButton) { return IsKeyDown(iMouseButton); }
	bool WasKeyPressedThisFrame(Zenith_KeyCode iKey);

	void MouseWheelCallback(double fXOffset, double fYOffset);
	float GetMouseWheelDelta() const;  // sim-aware; defined in .cpp

	bool IsGamepadConnected(int iGamepad = 0);
	bool IsGamepadButtonDown(int iButton, int iGamepad = 0);
	bool WasGamepadButtonPressedThisFrame(int iButton, int iGamepad = 0);
	float GetGamepadAxis(int iAxis, int iGamepad = 0);
	void GetGamepadLeftStick(float& fX, float& fY, int iGamepad = 0);
	void GetGamepadRightStick(float& fX, float& fY, int iGamepad = 0);
	float GetGamepadLeftTrigger(int iGamepad = 0);
	float GetGamepadRightTrigger(int iGamepad = 0);

	static constexpr float GAMEPAD_DEADZONE = 0.15f;

	// Covers GLFW keycodes (max 348) and mouse buttons (0-7).
	static constexpr int MAX_KEY_CODES = 512;

	bool m_abFrameKeyPresses[MAX_KEY_CODES] = {};

	Zenith_Maths::Vector2_64 m_xLastMousePosition = { 0.0, 0.0 };
	Zenith_Maths::Vector2_64 m_xMouseDelta        = { 0.0, 0.0 };
	float                    m_fMouseWheelDelta   = 0.0f;
	bool                     m_bFirstFrame        = true;
	// One-shot: set by NotifyMouseDiscontinuity (cursor capture/release), consumed by
	// the next BeginFrame to skip that frame's delta + resync the baseline.
	bool                     m_bMouseDiscontinuity = false;

#ifdef ZENITH_INPUT_SIMULATOR
	bool                     m_bSimWasEnabledLastFrame = false;
#endif

#ifdef ZENITH_WINDOWS
	static constexpr int     MAX_GAMEPADS = 4;
	GLFWgamepadstate         m_xLastGamepadState[MAX_GAMEPADS]    = {};
	GLFWgamepadstate         m_xCurrentGamepadState[MAX_GAMEPADS] = {};
	bool                     m_bGamepadStateInitialized[MAX_GAMEPADS] = { false };
#endif
};
