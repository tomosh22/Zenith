#pragma once

#include "Input/Zenith_Input.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

namespace DP_Input
{
	inline Zenith_Maths::Vector2 ReadMoveVillager()
	{
		Zenith_Maths::Vector2 xInput(0.0f);
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_W) || Zenith_Input::IsKeyHeld(ZENITH_KEY_UP))
			xInput.y += 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_S) || Zenith_Input::IsKeyHeld(ZENITH_KEY_DOWN))
			xInput.y -= 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_A) || Zenith_Input::IsKeyHeld(ZENITH_KEY_LEFT))
			xInput.x -= 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_D) || Zenith_Input::IsKeyHeld(ZENITH_KEY_RIGHT))
			xInput.x += 1.0f;
		return xInput;
	}

	inline bool ReadInteractPressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_F);
	}

	inline bool ReadAbilityPressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE);
	}

	inline float ReadCameraRotate()
	{
		float f = 0.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_Q)) f -= 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_E)) f += 1.0f;
		return f;
	}

	inline bool ReadEscapePressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE);
	}

	inline bool ReadPossessClickPressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT);
	}
}
