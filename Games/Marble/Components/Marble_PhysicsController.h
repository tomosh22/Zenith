#pragma once
/**
 * Marble_PhysicsController.h - Physics-based ball movement
 *
 * Demonstrates:
 * - Zenith_Physics API for impulse-based movement
 * - Zenith_ColliderComponent for physics body access
 * - Velocity checks for jump gating
 * - Fall detection via position check
 *
 * Key Jolt Physics concepts:
 * - BodyID identifies a physics body
 * - AddImpulse applies instant velocity change
 * - GetLinearVelocity returns current velocity
 */

#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Maths/Zenith_Maths.h"

// Configuration constants
static constexpr float s_fMarbleMoveSpeed = 0.5f;
static constexpr float s_fMarbleJumpImpulse = 8.0f;
static constexpr float s_fMarbleFallThreshold = -10.0f;

/**
 * Marble_PhysicsController - Physics-based ball control
 */
class Marble_PhysicsController
{
public:
	/**
	 * ApplyMovement - Apply movement impulse to the ball
	 *
	 * @param xCollider  Ball's collider component
	 * @param xDirection Normalized movement direction (can be zero)
	 */
	static void ApplyMovement(Zenith_ColliderComponent& xCollider, const Zenith_Maths::Vector3& xDirection)
	{
		if (!xCollider.HasValidBody())
			return;

		if (glm::length(xDirection) > 0.0f)
		{
			Zenith_Maths::Vector3 xForce = xDirection * s_fMarbleMoveSpeed;
			Zenith_Physics::AddImpulse(xCollider.GetBodyID(), xForce);
		}
	}

	/**
	 * TryJump - Attempt to jump if grounded
	 *
	 * Uses velocity check to prevent double-jumping.
	 * Only allows jump if vertical velocity is low (ball is on ground or falling).
	 *
	 * @param xCollider Ball's collider component
	 * @return true if jump was performed
	 */
	static bool TryJump(Zenith_ColliderComponent& xCollider)
	{
		if (!xCollider.HasValidBody())
			return false;

		const JPH::BodyID& xBodyID = xCollider.GetBodyID();

		// Check current vertical velocity
		Zenith_Maths::Vector3 xVel = Zenith_Physics::GetLinearVelocity(xBodyID);

		// Only allow jump if not already moving upward significantly
		// This prevents air-jumps and double-jumps
		if (xVel.y < 1.0f)
		{
			Zenith_Physics::AddImpulse(xBodyID, Zenith_Maths::Vector3(0.f, s_fMarbleJumpImpulse, 0.f));
			return true;
		}

		return false;
	}

	/**
	 * HasFallenOff - Check if ball has fallen below the level
	 *
	 * @param xPosition Current ball position
	 * @return true if ball is below fall threshold
	 */
	static bool HasFallenOff(const Zenith_Maths::Vector3& xPosition)
	{
		return xPosition.y < s_fMarbleFallThreshold;
	}

	/**
	 * GetVelocity - Get current ball velocity
	 *
	 * Useful for debugging or UI display.
	 */
	static Zenith_Maths::Vector3 GetVelocity(Zenith_ColliderComponent& xCollider)
	{
		if (!xCollider.HasValidBody())
			return Zenith_Maths::Vector3(0.f);

		return Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
	}
};
