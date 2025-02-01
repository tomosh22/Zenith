#pragma once
#include "Input/Zenith_KeyCodes.h"

class Zenith_Input
{
public:
	static void BeginFrame();

	static void KeyPressedCallback(Zenith_KeyCode iKey);
	static void MouseButtonPressedCallback(Zenith_KeyCode iKey);
	static void GetMousePosition(Zenith_Maths::Vector2_64& xOut);
	static bool IsKeyDown(Zenith_KeyCode iKey);
	static bool WasKeyPressedThisFrame(Zenith_KeyCode iKey);
};