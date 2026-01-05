#pragma once
/**
 * Marble_CameraFollow.h - Smooth camera following
 *
 * Demonstrates:
 * - Smooth follow with linear interpolation (glm::mix)
 * - Look-at calculation using pitch/yaw angles
 * - Fixed offset positioning behind target
 *
 * Camera setup:
 * - Positioned behind and above the ball
 * - Smoothly follows ball movement
 * - Always looks at the ball
 */

#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Maths/Zenith_Maths.h"
#include <cmath>

// Camera configuration
static constexpr float s_fMarbleCameraDistance = 8.0f;
static constexpr float s_fMarbleCameraHeight = 5.0f;
static constexpr float s_fMarbleCameraSmoothSpeed = 5.0f;

/**
 * Marble_CameraFollow - Smooth camera following system
 */
class Marble_CameraFollow
{
public:
	/**
	 * Update - Update camera position and rotation to follow target
	 *
	 * @param xCamera    Camera component to update
	 * @param xTargetPos Target position (ball position)
	 * @param fDt        Delta time for smooth interpolation
	 */
	static void Update(Zenith_CameraComponent& xCamera, const Zenith_Maths::Vector3& xTargetPos, float fDt)
	{
		// Calculate target camera position (behind and above the ball)
		// Using fixed Z offset means camera is always "south" of ball
		Zenith_Maths::Vector3 xTargetCamPos = xTargetPos +
			Zenith_Maths::Vector3(0.f, s_fMarbleCameraHeight, -s_fMarbleCameraDistance);

		// Get current camera position
		Zenith_Maths::Vector3 xCurrentPos;
		xCamera.GetPosition(xCurrentPos);

		// Smooth interpolation toward target position
		// Higher smoothSpeed = faster following
		float fLerpFactor = fDt * s_fMarbleCameraSmoothSpeed;
		fLerpFactor = std::min(fLerpFactor, 1.0f); // Clamp to avoid overshooting

		Zenith_Maths::Vector3 xNewPos = glm::mix(xCurrentPos, xTargetCamPos, fLerpFactor);
		xCamera.SetPosition(xNewPos);

		// Calculate look-at direction
		Zenith_Maths::Vector3 xDir = xTargetPos - xNewPos;
		if (glm::length(xDir) > 0.001f)
		{
			xDir = glm::normalize(xDir);

			// Calculate pitch (vertical angle) and yaw (horizontal angle)
			// Pitch: asin(y) - looking up is positive, down is negative
			float fPitch = asin(xDir.y);

			// Yaw: atan2(x, z) - angle in XZ plane from Z axis
			float fYaw = atan2(xDir.x, xDir.z);

			xCamera.SetPitch(fPitch);
			xCamera.SetYaw(fYaw);
		}
	}

	/**
	 * SetInitialPosition - Instantly position camera behind target
	 *
	 * Use this when starting a level to avoid initial camera lerp.
	 */
	static void SetInitialPosition(Zenith_CameraComponent& xCamera, const Zenith_Maths::Vector3& xTargetPos)
	{
		Zenith_Maths::Vector3 xCamPos = xTargetPos +
			Zenith_Maths::Vector3(0.f, s_fMarbleCameraHeight, -s_fMarbleCameraDistance);
		xCamera.SetPosition(xCamPos);

		// Look at target
		Zenith_Maths::Vector3 xDir = xTargetPos - xCamPos;
		if (glm::length(xDir) > 0.001f)
		{
			xDir = glm::normalize(xDir);
			xCamera.SetPitch(asin(xDir.y));
			xCamera.SetYaw(atan2(xDir.x, xDir.z));
		}
	}
};
