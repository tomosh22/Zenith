#pragma once
#include "Input/Zenith_KeyCodes.h"

class Zenith_Input
{
public:
	static void BeginFrame();

	static void KeyPressedCallback(Zenith_KeyCode iKey);
	static void MouseButtonPressedCallback(Zenith_KeyCode iKey);
	static void GetMousePosition(Zenith_Maths::Vector2_64& xOut);
	static void GetMouseDelta(Zenith_Maths::Vector2_64& xOut);
	static bool IsKeyDown(Zenith_KeyCode iKey);
	static bool IsKeyHeld(Zenith_KeyCode iKey) { return IsKeyDown(iKey); }
	static bool IsMouseButtonHeld(Zenith_KeyCode iMouseButton) { return IsKeyDown(iMouseButton); }
	static bool WasKeyPressedThisFrame(Zenith_KeyCode iKey);
};