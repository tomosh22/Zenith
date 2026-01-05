#pragma once
/**
 * Runner_CharacterController.h - Character movement on terrain
 *
 * Handles:
 * - Forward movement with increasing speed
 * - Lane-based lateral movement
 * - Jumping and sliding mechanics
 * - Collision detection with obstacles
 * - Terrain height following
 *
 * Demonstrates:
 * - Custom character controller (non-physics based for precise control)
 * - Lane system for mobile-style gameplay
 * - Animation state feedback
 */

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// Character State
// ============================================================================
enum class RunnerCharacterState
{
	RUNNING,
	JUMPING,
	SLIDING,
	DEAD
};

// ============================================================================
// Runner_CharacterController
// ============================================================================
class Runner_CharacterController
{
public:
	// ========================================================================
	// Configuration (set from Runner_Config)
	// ========================================================================
	struct Config
	{
		float m_fForwardSpeed = 15.0f;
		float m_fMaxForwardSpeed = 35.0f;
		float m_fSpeedIncreaseRate = 0.5f;
		float m_fLateralMoveSpeed = 8.0f;
		float m_fJumpForce = 12.0f;
		float m_fGravity = 30.0f;
		float m_fSlideSpeed = 1.2f;
		float m_fSlideDuration = 0.8f;
		float m_fCharacterHeight = 1.8f;
		float m_fCharacterRadius = 0.4f;
		float m_fSlideHeight = 0.6f;
		uint32_t m_uLaneCount = 3;
		float m_fLaneWidth = 3.0f;
		float m_fLaneSwitchTime = 0.2f;
	};

	// ========================================================================
	// State Access
	// ========================================================================
	static RunnerCharacterState GetState() { return s_eState; }
	static float GetCurrentSpeed() { return s_fCurrentSpeed; }
	static int32_t GetCurrentLane() { return s_iCurrentLane; }
	static float GetDistanceTraveled() { return s_fDistanceTraveled; }
	static bool IsGrounded() { return s_bIsGrounded; }

	// ========================================================================
	// Initialization
	// ========================================================================
	static void Initialize(const Config& xConfig)
	{
		s_xConfig = xConfig;
		Reset();
	}

	static void Reset()
	{
		s_eState = RunnerCharacterState::RUNNING;
		s_fCurrentSpeed = s_xConfig.m_fForwardSpeed;
		s_iCurrentLane = 1;  // Start in middle lane (0, 1, 2 for 3 lanes)
		s_iTargetLane = 1;
		s_fLaneSwitchProgress = 1.0f;  // 1 = fully at target lane
		s_fVerticalVelocity = 0.0f;
		s_fSlideTimer = 0.0f;
		s_fDistanceTraveled = 0.0f;
		s_bIsGrounded = true;
		s_fCurrentHeight = 0.0f;
	}

	// ========================================================================
	// Update (called each frame)
	// ========================================================================
	static void Update(float fDt, Zenith_TransformComponent& xTransform, float fTerrainHeight)
	{
		if (s_eState == RunnerCharacterState::DEAD)
		{
			return;
		}

		// Handle input
		HandleInput();

		// Update speed (increases over time)
		if (s_fCurrentSpeed < s_xConfig.m_fMaxForwardSpeed)
		{
			s_fCurrentSpeed += s_xConfig.m_fSpeedIncreaseRate * fDt;
			if (s_fCurrentSpeed > s_xConfig.m_fMaxForwardSpeed)
			{
				s_fCurrentSpeed = s_xConfig.m_fMaxForwardSpeed;
			}
		}

		// Apply speed modifier for sliding
		float fEffectiveSpeed = s_fCurrentSpeed;
		if (s_eState == RunnerCharacterState::SLIDING)
		{
			fEffectiveSpeed *= s_xConfig.m_fSlideSpeed;
		}

		// Update distance traveled
		s_fDistanceTraveled += fEffectiveSpeed * fDt;

		// Update lane position
		UpdateLanePosition(fDt);

		// Update vertical movement
		UpdateVerticalMovement(fDt, fTerrainHeight);

		// Update slide timer
		if (s_eState == RunnerCharacterState::SLIDING)
		{
			s_fSlideTimer -= fDt;
			if (s_fSlideTimer <= 0.0f)
			{
				s_eState = RunnerCharacterState::RUNNING;
				s_fSlideTimer = 0.0f;
			}
		}

		// Calculate final position
		Zenith_Maths::Vector3 xPosition;
		xTransform.GetPosition(xPosition);

		// Forward movement (along +Z axis)
		xPosition.z = s_fDistanceTraveled;

		// Lateral position (X axis based on lane)
		float fLaneOffset = CalculateLaneOffset();
		xPosition.x = fLaneOffset;

		// Vertical position
		xPosition.y = s_fCurrentHeight + s_xConfig.m_fCharacterHeight * 0.5f;

		xTransform.SetPosition(xPosition);
	}

	// ========================================================================
	// Collision
	// ========================================================================
	static void OnObstacleHit()
	{
		s_eState = RunnerCharacterState::DEAD;
	}

	// ========================================================================
	// Animation Feedback
	// ========================================================================
	static float GetSpeedNormalized()
	{
		return s_fCurrentSpeed / s_xConfig.m_fMaxForwardSpeed;
	}

	static bool IsSliding()
	{
		return s_eState == RunnerCharacterState::SLIDING;
	}

	static bool IsJumping()
	{
		return s_eState == RunnerCharacterState::JUMPING;
	}

	static float GetCurrentCharacterHeight()
	{
		if (s_eState == RunnerCharacterState::SLIDING)
		{
			return s_xConfig.m_fSlideHeight;
		}
		return s_xConfig.m_fCharacterHeight;
	}

private:
	// ========================================================================
	// Input Handling
	// ========================================================================
	static void HandleInput()
	{
		if (s_eState == RunnerCharacterState::DEAD)
		{
			return;
		}

		// Lane switching - Left (A or Left Arrow)
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_A) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_LEFT))
		{
			TrySwitchLane(-1);
		}

		// Lane switching - Right (D or Right Arrow)
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_D) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_RIGHT))
		{
			TrySwitchLane(1);
		}

		// Jump (Space or W or Up Arrow)
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_W) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_UP))
		{
			TryJump();
		}

		// Slide (S or Down Arrow)
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_S) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_DOWN))
		{
			TrySlide();
		}
	}

	static void TrySwitchLane(int32_t iDirection)
	{
		int32_t iNewLane = s_iTargetLane + iDirection;
		int32_t iMaxLane = static_cast<int32_t>(s_xConfig.m_uLaneCount) - 1;

		if (iNewLane >= 0 && iNewLane <= iMaxLane)
		{
			s_iCurrentLane = s_iTargetLane;
			s_iTargetLane = iNewLane;
			s_fLaneSwitchProgress = 0.0f;
		}
	}

	static void TryJump()
	{
		if (s_bIsGrounded && s_eState != RunnerCharacterState::SLIDING)
		{
			s_eState = RunnerCharacterState::JUMPING;
			s_fVerticalVelocity = s_xConfig.m_fJumpForce;
			s_bIsGrounded = false;
		}
	}

	static void TrySlide()
	{
		if (s_bIsGrounded && s_eState == RunnerCharacterState::RUNNING)
		{
			s_eState = RunnerCharacterState::SLIDING;
			s_fSlideTimer = s_xConfig.m_fSlideDuration;
		}
	}

	// ========================================================================
	// Movement Updates
	// ========================================================================
	static void UpdateLanePosition(float fDt)
	{
		if (s_fLaneSwitchProgress < 1.0f)
		{
			s_fLaneSwitchProgress += fDt / s_xConfig.m_fLaneSwitchTime;
			if (s_fLaneSwitchProgress > 1.0f)
			{
				s_fLaneSwitchProgress = 1.0f;
				s_iCurrentLane = s_iTargetLane;
			}
		}
	}

	static float CalculateLaneOffset()
	{
		// Calculate center offset (middle lane at 0)
		int32_t iHalfLanes = static_cast<int32_t>(s_xConfig.m_uLaneCount) / 2;

		float fCurrentLanePos = (static_cast<float>(s_iCurrentLane) - static_cast<float>(iHalfLanes)) * s_xConfig.m_fLaneWidth;
		float fTargetLanePos = (static_cast<float>(s_iTargetLane) - static_cast<float>(iHalfLanes)) * s_xConfig.m_fLaneWidth;

		// Smooth interpolation using easing
		float fT = s_fLaneSwitchProgress;
		fT = fT * fT * (3.0f - 2.0f * fT);  // Smoothstep

		return glm::mix(fCurrentLanePos, fTargetLanePos, fT);
	}

	static void UpdateVerticalMovement(float fDt, float fTerrainHeight)
	{
		if (!s_bIsGrounded)
		{
			// Apply gravity
			s_fVerticalVelocity -= s_xConfig.m_fGravity * fDt;
			s_fCurrentHeight += s_fVerticalVelocity * fDt;

			// Check ground collision
			if (s_fCurrentHeight <= fTerrainHeight)
			{
				s_fCurrentHeight = fTerrainHeight;
				s_fVerticalVelocity = 0.0f;
				s_bIsGrounded = true;

				if (s_eState == RunnerCharacterState::JUMPING)
				{
					s_eState = RunnerCharacterState::RUNNING;
				}
			}
		}
		else
		{
			// Follow terrain
			s_fCurrentHeight = fTerrainHeight;
		}
	}

	// ========================================================================
	// Static State
	// ========================================================================
	static inline Config s_xConfig;
	static inline RunnerCharacterState s_eState = RunnerCharacterState::RUNNING;
	static inline float s_fCurrentSpeed = 15.0f;
	static inline int32_t s_iCurrentLane = 1;
	static inline int32_t s_iTargetLane = 1;
	static inline float s_fLaneSwitchProgress = 1.0f;
	static inline float s_fVerticalVelocity = 0.0f;
	static inline float s_fSlideTimer = 0.0f;
	static inline float s_fDistanceTraveled = 0.0f;
	static inline bool s_bIsGrounded = true;
	static inline float s_fCurrentHeight = 0.0f;
};
