#pragma once
/**
 * Exploration_PlayerController.h - First-person camera and movement
 *
 * Demonstrates:
 * - Mouse-look with pitch/yaw rotation
 * - WASD movement relative to camera facing
 * - Terrain-following height adjustment
 * - Sprint modifier (Shift key)
 *
 * Engine APIs used:
 * - Zenith_Input for keyboard and mouse
 * - Zenith_CameraComponent for camera manipulation
 */

#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

#include <cmath>
#include <algorithm>

namespace Exploration_PlayerController
{
	// ========================================================================
	// Configuration (can be overridden via Exploration_Config)
	// ========================================================================
	static float s_fMoveSpeed = 10.0f;
	static float s_fSprintMultiplier = 2.5f;
	static float s_fMouseSensitivity = 0.002f;
	static float s_fPlayerEyeHeight = 1.8f;
	static float s_fPitchLimit = 1.4f;  // ~80 degrees
	static float s_fGravity = 20.0f;
	static float s_fJumpVelocity = 8.0f;

	// ========================================================================
	// State
	// ========================================================================
	static bool s_bMouseCaptured = false;
	static float s_fVerticalVelocity = 0.0f;
	static bool s_bOnGround = true;
	static int32_t s_iLastMouseX = 0;
	static int32_t s_iLastMouseY = 0;
	static bool s_bFirstMouse = true;

	/**
	 * Configure controller with settings from Exploration_Config
	 */
	inline void Configure(
		float fMoveSpeed,
		float fSprintMultiplier,
		float fMouseSensitivity,
		float fPlayerEyeHeight,
		float fPitchLimit,
		float fGravity,
		float fJumpVelocity)
	{
		s_fMoveSpeed = fMoveSpeed;
		s_fSprintMultiplier = fSprintMultiplier;
		s_fMouseSensitivity = fMouseSensitivity;
		s_fPlayerEyeHeight = fPlayerEyeHeight;
		s_fPitchLimit = fPitchLimit;
		s_fGravity = fGravity;
		s_fJumpVelocity = fJumpVelocity;
	}

	/**
	 * Set/release mouse capture
	 */
	inline void SetMouseCapture(bool bCapture)
	{
		s_bMouseCaptured = bCapture;
		// Note: Cursor visibility handled by platform layer
		if (bCapture)
		{
			s_bFirstMouse = true;
		}
	}

	/**
	 * Check if escape was pressed to release mouse
	 */
	inline bool WasEscapePressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE);
	}

	/**
	 * Check if mouse was clicked to capture it
	 */
	inline bool WasMouseClicked()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT);
	}

	/**
	 * Handle mouse look (pitch/yaw camera rotation)
	 * @param xCamera Camera component to modify
	 */
	inline void HandleMouseLook(Zenith_CameraComponent& xCamera)
	{
		if (!s_bMouseCaptured)
			return;

		// Skip first frame after capture to avoid jump
		if (s_bFirstMouse)
		{
			s_bFirstMouse = false;
			return;
		}

		// Get mouse delta directly from engine
		Zenith_Maths::Vector2_64 xDelta;
		Zenith_Input::GetMouseDelta(xDelta);

		// Apply sensitivity
		float fYawDelta = static_cast<float>(xDelta.x) * s_fMouseSensitivity;
		float fPitchDelta = static_cast<float>(xDelta.y) * s_fMouseSensitivity;

		// Update yaw (horizontal rotation)
		double fYaw = xCamera.GetYaw() + fYawDelta;
		// Keep yaw in range [0, 2*PI]
		const double fTwoPi = 6.28318530718;
		while (fYaw < 0.0) fYaw += fTwoPi;
		while (fYaw >= fTwoPi) fYaw -= fTwoPi;
		xCamera.SetYaw(fYaw);

		// Update pitch (vertical rotation) with clamping
		double fPitch = xCamera.GetPitch() - fPitchDelta;  // Inverted for natural feel
		fPitch = std::clamp(fPitch, -static_cast<double>(s_fPitchLimit), static_cast<double>(s_fPitchLimit));
		xCamera.SetPitch(fPitch);
	}

	/**
	 * Get movement input direction
	 * @return Normalized movement direction in world XZ plane, or zero if no input
	 */
	inline Zenith_Maths::Vector3 GetMovementInput()
	{
		Zenith_Maths::Vector3 xInput(0.0f, 0.0f, 0.0f);

		// Forward/backward (W/S or Up/Down)
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_W) || Zenith_Input::IsKeyHeld(ZENITH_KEY_UP))
		{
			xInput.z += 1.0f;
		}
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_S) || Zenith_Input::IsKeyHeld(ZENITH_KEY_DOWN))
		{
			xInput.z -= 1.0f;
		}

		// Strafe left/right (A/D or Left/Right)
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_A) || Zenith_Input::IsKeyHeld(ZENITH_KEY_LEFT))
		{
			xInput.x -= 1.0f;
		}
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_D) || Zenith_Input::IsKeyHeld(ZENITH_KEY_RIGHT))
		{
			xInput.x += 1.0f;
		}

		// Normalize if non-zero to prevent faster diagonal movement
		float fLengthSq = xInput.x * xInput.x + xInput.z * xInput.z;
		if (fLengthSq > 0.001f)
		{
			float fInvLength = 1.0f / std::sqrt(fLengthSq);
			xInput.x *= fInvLength;
			xInput.z *= fInvLength;
		}

		return xInput;
	}

	/**
	 * Check if sprint key is held
	 */
	inline bool IsSprinting()
	{
		return Zenith_Input::IsKeyHeld(ZENITH_KEY_LEFT_SHIFT) ||
		       Zenith_Input::IsKeyHeld(ZENITH_KEY_RIGHT_SHIFT);
	}

	/**
	 * Check if jump was requested
	 */
	inline bool WasJumpPressed()
	{
		return Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE);
	}

	/**
	 * Update player position and camera
	 * @param xCamera Camera component to update
	 * @param fTerrainHeight Terrain height at current XZ position
	 * @param fDt Delta time
	 */
	inline void Update(Zenith_CameraComponent& xCamera, float fTerrainHeight, float fDt)
	{
		// Handle mouse capture toggle
		if (s_bMouseCaptured && WasEscapePressed())
		{
			SetMouseCapture(false);
		}
		else if (!s_bMouseCaptured && WasMouseClicked())
		{
			SetMouseCapture(true);
		}

		// Mouse look
		HandleMouseLook(xCamera);

		// Get camera position
		Zenith_Maths::Vector3 xPosition;
		xCamera.GetPosition(xPosition);

		// Calculate camera facing direction on XZ plane
		double fYaw = xCamera.GetYaw();
		Zenith_Maths::Vector3 xForward(
			static_cast<float>(std::sin(fYaw)),
			0.0f,
			static_cast<float>(std::cos(fYaw))
		);
		Zenith_Maths::Vector3 xRight(
			static_cast<float>(std::cos(fYaw)),
			0.0f,
			static_cast<float>(-std::sin(fYaw))
		);

		// Get movement input
		Zenith_Maths::Vector3 xMoveInput = GetMovementInput();

		// Convert to world space direction
		Zenith_Maths::Vector3 xMoveDir = xForward * xMoveInput.z + xRight * xMoveInput.x;

		// Apply speed (with sprint)
		float fSpeed = s_fMoveSpeed;
		if (IsSprinting())
		{
			fSpeed *= s_fSprintMultiplier;
		}

		// Update position
		xPosition.x += xMoveDir.x * fSpeed * fDt;
		xPosition.z += xMoveDir.z * fSpeed * fDt;

		// Calculate target height (terrain + eye height)
		float fTargetY = fTerrainHeight + s_fPlayerEyeHeight;

		// Handle vertical movement (jumping/gravity)
		if (s_bOnGround)
		{
			// On ground - check for jump
			if (WasJumpPressed())
			{
				s_fVerticalVelocity = s_fJumpVelocity;
				s_bOnGround = false;
			}
			else
			{
				// Snap to terrain
				xPosition.y = fTargetY;
				s_fVerticalVelocity = 0.0f;
			}
		}
		else
		{
			// In air - apply gravity
			s_fVerticalVelocity -= s_fGravity * fDt;
			xPosition.y += s_fVerticalVelocity * fDt;

			// Check if landed
			if (xPosition.y <= fTargetY)
			{
				xPosition.y = fTargetY;
				s_fVerticalVelocity = 0.0f;
				s_bOnGround = true;
			}
		}

		// Update camera position
		xCamera.SetPosition(xPosition);
	}

	/**
	 * Reset controller state (e.g., when reloading scene)
	 */
	inline void Reset()
	{
		s_bMouseCaptured = false;
		s_fVerticalVelocity = 0.0f;
		s_bOnGround = true;
		s_bFirstMouse = true;
		// Note: Cursor visibility handled by platform layer
	}

	/**
	 * Get current player world position
	 */
	inline Zenith_Maths::Vector3 GetPosition(const Zenith_CameraComponent& xCamera)
	{
		Zenith_Maths::Vector3 xPos;
		xCamera.GetPosition(xPos);
		return xPos;
	}

	/**
	 * Check if player is on ground
	 */
	inline bool IsOnGround()
	{
		return s_bOnGround;
	}

	/**
	 * Check if mouse is currently captured
	 */
	inline bool IsMouseCaptured()
	{
		return s_bMouseCaptured;
	}

} // namespace Exploration_PlayerController
