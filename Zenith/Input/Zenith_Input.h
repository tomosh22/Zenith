#pragma once
#include "Input/Zenith_KeyCodes.h"

class Zenith_Input
{
public:
	static void KeyPressedCallback(uint32_t uKeyCode);
	static void MouseButtonPressedCallback(uint32_t uKeyCode);
	static void GetMousePosition(Zenith_Maths::Vector2_64& xOut);
	static bool IsKeyDown(Zenith_KeyCode iKey);
};