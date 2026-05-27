#pragma once

#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"
#include <unordered_set>

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

	void KeyPressedCallback(Zenith_KeyCode iKey);
	void MouseButtonPressedCallback(Zenith_KeyCode iKey);
	void GetMousePosition(Zenith_Maths::Vector2_64& xOut);
	void GetMouseDelta(Zenith_Maths::Vector2_64& xOut);
	bool IsKeyDown(Zenith_KeyCode iKey);
	bool IsKeyHeld(Zenith_KeyCode iKey) { return IsKeyDown(iKey); }
	bool IsMouseButtonHeld(Zenith_KeyCode iMouseButton) { return IsKeyDown(iMouseButton); }
	bool WasKeyPressedThisFrame(Zenith_KeyCode iKey);

	void MouseWheelCallback(double fXOffset, double fYOffset);
	float GetMouseWheelDelta() const { return m_fMouseWheelDelta; }

	bool IsGamepadConnected(int iGamepad = 0);
	bool IsGamepadButtonDown(int iButton, int iGamepad = 0);
	bool WasGamepadButtonPressedThisFrame(int iButton, int iGamepad = 0);
	float GetGamepadAxis(int iAxis, int iGamepad = 0);
	void GetGamepadLeftStick(float& fX, float& fY, int iGamepad = 0);
	void GetGamepadRightStick(float& fX, float& fY, int iGamepad = 0);
	float GetGamepadLeftTrigger(int iGamepad = 0);
	float GetGamepadRightTrigger(int iGamepad = 0);

	static constexpr float GAMEPAD_DEADZONE = 0.15f;

	std::unordered_set<Zenith_KeyCode> m_xFrameKeyPresses;

	Zenith_Maths::Vector2_64 m_xLastMousePosition = { 0.0, 0.0 };
	Zenith_Maths::Vector2_64 m_xMouseDelta        = { 0.0, 0.0 };
	float                    m_fMouseWheelDelta   = 0.0f;
	bool                     m_bFirstFrame        = true;

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
