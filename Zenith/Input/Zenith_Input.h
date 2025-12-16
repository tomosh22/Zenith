#pragma once
#include "Input/Zenith_KeyCodes.h"

class Zenith_Input
{
public:
	static void BeginFrame();

	// Keyboard input
	static void KeyPressedCallback(Zenith_KeyCode iKey);
	static void MouseButtonPressedCallback(Zenith_KeyCode iKey);
	static void GetMousePosition(Zenith_Maths::Vector2_64& xOut);
	static void GetMouseDelta(Zenith_Maths::Vector2_64& xOut);
	static bool IsKeyDown(Zenith_KeyCode iKey);
	static bool IsKeyHeld(Zenith_KeyCode iKey) { return IsKeyDown(iKey); }
	static bool IsMouseButtonHeld(Zenith_KeyCode iMouseButton) { return IsKeyDown(iMouseButton); }
	static bool WasKeyPressedThisFrame(Zenith_KeyCode iKey);

	// Gamepad input (gamepad 0 = first connected gamepad)
	static bool IsGamepadConnected(int iGamepad = 0);
	static bool IsGamepadButtonDown(int iButton, int iGamepad = 0);
	static bool WasGamepadButtonPressedThisFrame(int iButton, int iGamepad = 0);
	static float GetGamepadAxis(int iAxis, int iGamepad = 0);
	static void GetGamepadLeftStick(float& fX, float& fY, int iGamepad = 0);
	static void GetGamepadRightStick(float& fX, float& fY, int iGamepad = 0);
	static float GetGamepadLeftTrigger(int iGamepad = 0);
	static float GetGamepadRightTrigger(int iGamepad = 0);

	// Deadzone for analog sticks (values below this are treated as 0)
	static constexpr float GAMEPAD_DEADZONE = 0.15f;
};