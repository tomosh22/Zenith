#pragma once
/**
 * Sokoban_Input.h - Input handling module
 *
 * Demonstrates: Zenith_Input API for keyboard polling
 *
 * Key concepts:
 * - WasKeyPressedThisFrame() for discrete input (movement)
 * - IsKeyHeld() for continuous input (not used here, but available)
 * - Key codes defined in Input/Zenith_KeyCodes.h
 */

#include "Input/Zenith_Input.h"
#include "Sokoban_GridLogic.h"  // For SokobanDirection enum

/**
 * Sokoban_Input - Static utility class for input handling
 *
 * Separates input polling from game logic for cleaner architecture.
 * In more complex games, you might use an input mapping system.
 */
class Sokoban_Input
{
public:
	/**
	 * GetInputDirection - Check for movement input
	 *
	 * Returns the direction pressed this frame, or SOKOBAN_DIR_NONE.
	 * Uses WasKeyPressedThisFrame for discrete, turn-based movement.
	 */
	static SokobanDirection GetInputDirection()
	{
		// Check WASD keys (common for modern games)
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_W))
		{
			return SOKOBAN_DIR_UP;
		}
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_S))
		{
			return SOKOBAN_DIR_DOWN;
		}
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_A))
		{
			return SOKOBAN_DIR_LEFT;
		}
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_D))
		{
			return SOKOBAN_DIR_RIGHT;
		}

		// Also check arrow keys (traditional Sokoban controls)
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_UP))
		{
			return SOKOBAN_DIR_UP;
		}
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_DOWN))
		{
			return SOKOBAN_DIR_DOWN;
		}
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_LEFT))
		{
			return SOKOBAN_DIR_LEFT;
		}
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_RIGHT))
		{
			return SOKOBAN_DIR_RIGHT;
		}

		return SOKOBAN_DIR_NONE;
	}

	/**
	 * WasResetPressed - Check for level reset input
	 */
	static bool WasResetPressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R);
	}
};
