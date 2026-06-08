#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Marble_Input.h - Camera-relative input handling
 *
 * Demonstrates:
 * - Continuous input with IsKeyHeld (vs discrete WasKeyPressedThisFrame)
 * - Camera-relative movement direction calculation
 * - Projecting camera forward onto XZ plane
 *
 * Key difference from Sokoban:
 * - Sokoban uses WasKeyPressedThisFrame for grid-based movement
 * - Marble uses IsKeyHeld for smooth physics-based movement
 */

#include "Input/Zenith_Input.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Maths/Zenith_Maths.h"

/**
 * Marble_Input - Camera-relative input handling
 */
class Marble_Input
{
public:
	/**
	 * GetMovementDirection - Get camera-relative movement direction
	 *
	 * Calculates a normalized direction vector based on WASD/arrow input,
	 * relative to the camera's forward direction (projected onto XZ plane).
	 *
	 * @param xCamPos   Camera position
	 * @param xBallPos  Ball position (target of camera)
	 * @return Normalized direction in world space, or zero vector if no input
	 */
	static Zenith_Maths::Vector3 GetMovementDirection(
		const Zenith_Maths::Vector3& xCamPos,
		const Zenith_Maths::Vector3& xBallPos)
	{
		// Calculate forward direction from camera to ball (projected to XZ)
		Zenith_Maths::Vector3 xToBall = xBallPos - xCamPos;
		xToBall.y = 0.0f;

		if (glm::length(xToBall) > 0.001f)
		{
			xToBall = glm::normalize(xToBall);
		}
		else
		{
			xToBall = Zenith_Maths::Vector3(0.f, 0.f, 1.f);
		}

		// Forward is toward ball, right is perpendicular (cross with up)
		Zenith_Maths::Vector3 xForward = xToBall;
		Zenith_Maths::Vector3 xRight = glm::cross(Zenith_Maths::Vector3(0.f, 1.f, 0.f), xForward);

		// Accumulate input direction
		Zenith_Maths::Vector3 xDirection(0.f);

		// Forward/backward
		if (g_xEngine.Input().IsKeyHeld(ZENITH_KEY_W) || g_xEngine.Input().IsKeyHeld(ZENITH_KEY_UP))
		{
			xDirection += xForward;
		}
		if (g_xEngine.Input().IsKeyHeld(ZENITH_KEY_S) || g_xEngine.Input().IsKeyHeld(ZENITH_KEY_DOWN))
		{
			xDirection -= xForward;
		}

		// Left/right strafe
		if (g_xEngine.Input().IsKeyHeld(ZENITH_KEY_A) || g_xEngine.Input().IsKeyHeld(ZENITH_KEY_LEFT))
		{
			xDirection -= xRight;
		}
		if (g_xEngine.Input().IsKeyHeld(ZENITH_KEY_D) || g_xEngine.Input().IsKeyHeld(ZENITH_KEY_RIGHT))
		{
			xDirection += xRight;
		}

		// Normalize if any input
		if (glm::length(xDirection) > 0.0f)
		{
			xDirection = glm::normalize(xDirection);
		}

		return xDirection;
	}

	/**
	 * WasJumpPressed - Check for jump input
	 */
	static bool WasJumpPressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_SPACE);
	}

	/**
	 * WasPausePressed - Check for pause toggle
	 */
	static bool WasPausePressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_P) ||
			   g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE);
	}

	/**
	 * WasResetPressed - Check for level reset
	 */
	static bool WasResetPressed()
	{
		return g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_R);
	}
};
