#pragma once

#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_KeyCodes.h"
#include "Maths/Zenith_Maths.h"

// Game-local action names over Zenith's raw keyboard input. Keeping this layer
// stateless makes the mappings easy to replace when rebinding lands, while the
// pure resolver gives tests the same opposite-key and diagonal semantics as the
// live readers without requiring an engine instance.
namespace ZM_InputActions
{
	inline Zenith_Maths::Vector2 ResolveMove(
		bool bForward,
		bool bBackward,
		bool bLeft,
		bool bRight)
	{
		Zenith_Maths::Vector2 xMove(0.0f);
		if (bForward)
		{
			xMove.y += 1.0f;
		}
		if (bBackward)
		{
			xMove.y -= 1.0f;
		}
		if (bLeft)
		{
			xMove.x -= 1.0f;
		}
		if (bRight)
		{
			xMove.x += 1.0f;
		}
		return xMove;
	}

	inline Zenith_Maths::Vector2 ReadMove()
	{
		Zenith_Input& xInput = g_xEngine.Input();
		return ResolveMove(
			xInput.IsKeyDown(ZENITH_KEY_W) || xInput.IsKeyDown(ZENITH_KEY_UP),
			xInput.IsKeyDown(ZENITH_KEY_S) || xInput.IsKeyDown(ZENITH_KEY_DOWN),
			xInput.IsKeyDown(ZENITH_KEY_A) || xInput.IsKeyDown(ZENITH_KEY_LEFT),
			xInput.IsKeyDown(ZENITH_KEY_D) || xInput.IsKeyDown(ZENITH_KEY_RIGHT));
	}

	inline bool ReadConfirmPressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ENTER)
			|| g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_SPACE);
	}

	inline bool ReadCancelPressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE)
			|| g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_BACKSPACE);
	}

	inline bool ReadMenuPressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_M)
			|| g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_TAB);
	}

	inline bool ReadRunHeld()
	{
		return g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT_SHIFT)
			|| g_xEngine.Input().IsKeyDown(ZENITH_KEY_RIGHT_SHIFT);
	}
}
