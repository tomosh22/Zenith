#pragma once
/**
 * Combat_PlayerController.h - Player movement and combat input
 *
 * Demonstrates:
 * - Zenith_Input for keyboard/mouse polling
 * - Physics-based character movement
 * - Attack input with state blocking
 * - Dodge/roll mechanics
 *
 * Player can move, attack (light/heavy), and dodge.
 * Attacks block movement input until recovery.
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

// ============================================================================
// Player State
// ============================================================================

enum class Combat_PlayerState : uint8_t
{
	IDLE,
	WALKING,
	LIGHT_ATTACK_1,
	LIGHT_ATTACK_2,
	LIGHT_ATTACK_3,
	HEAVY_ATTACK,
	DODGING,
	HIT_STUN,
	DEAD
};

// ============================================================================
// Attack Types
// ============================================================================

enum class Combat_AttackType : uint8_t
{
	NONE,
	LIGHT,
	HEAVY
};

// ============================================================================
// Player Controller
// ============================================================================

/**
 * Combat_PlayerController - Handles player input and state
 */
class Combat_PlayerController
{
public:
	// ========================================================================
	// Configuration (set from Combat_Config)
	// ========================================================================

	float m_fMoveSpeed = 5.0f;
	float m_fRotationSpeed = 10.0f;
	float m_fDodgeSpeed = 12.0f;
	float m_fDodgeDuration = 0.4f;
	float m_fDodgeCooldown = 0.5f;

	float m_fLightAttackDuration = 0.3f;
	float m_fHeavyAttackDuration = 0.6f;
	float m_fComboWindowTime = 0.5f;
	float m_fAttackRecoveryTime = 0.2f;

	float m_fLightAttackRange = 1.5f;
	float m_fHeavyAttackRange = 2.0f;

	// ========================================================================
	// State Accessors
	// ========================================================================

	Combat_PlayerState GetState() const { return m_eState; }
	bool IsAttacking() const
	{
		return m_eState == Combat_PlayerState::LIGHT_ATTACK_1 ||
			   m_eState == Combat_PlayerState::LIGHT_ATTACK_2 ||
			   m_eState == Combat_PlayerState::LIGHT_ATTACK_3 ||
			   m_eState == Combat_PlayerState::HEAVY_ATTACK;
	}
	bool IsDodging() const { return m_eState == Combat_PlayerState::DODGING; }
	bool CanMove() const
	{
		return m_eState == Combat_PlayerState::IDLE ||
			   m_eState == Combat_PlayerState::WALKING;
	}
	bool CanAttack() const
	{
		return CanMove() || IsInComboWindow();
	}
	bool CanDodge() const
	{
		return CanMove() && m_fDodgeCooldownTimer <= 0.0f;
	}

	uint32_t GetComboCount() const { return m_uComboCount; }
	float GetMoveSpeed() const { return m_fCurrentSpeed; }
	Zenith_Maths::Vector3 GetMoveDirection() const { return m_xMoveDirection; }
	Zenith_Maths::Vector3 GetFacingDirection() const { return m_xFacingDirection; }

	// ========================================================================
	// Attack State
	// ========================================================================

	bool WasAttackJustStarted() const { return m_bAttackJustStarted; }
	Combat_AttackType GetCurrentAttackType() const { return m_eCurrentAttackType; }
	float GetAttackProgress() const
	{
		if (!IsAttacking())
			return 0.0f;
		float fDuration = (m_eCurrentAttackType == Combat_AttackType::HEAVY) ?
			m_fHeavyAttackDuration : m_fLightAttackDuration;
		return 1.0f - (m_fStateTimer / fDuration);
	}

	// ========================================================================
	// Update
	// ========================================================================

	/**
	 * Update - Process input and update state
	 */
	void Update(Zenith_TransformComponent& xTransform, Zenith_ColliderComponent& xCollider, float fDt)
	{
		m_bAttackJustStarted = false;

		// Update timers
		UpdateTimers(fDt);

		// State machine update
		switch (m_eState)
		{
		case Combat_PlayerState::IDLE:
		case Combat_PlayerState::WALKING:
			HandleMovementState(xTransform, xCollider, fDt);
			break;

		case Combat_PlayerState::LIGHT_ATTACK_1:
		case Combat_PlayerState::LIGHT_ATTACK_2:
		case Combat_PlayerState::LIGHT_ATTACK_3:
		case Combat_PlayerState::HEAVY_ATTACK:
			HandleAttackState(xTransform, fDt);
			break;

		case Combat_PlayerState::DODGING:
			HandleDodgeState(xTransform, xCollider, fDt);
			break;

		case Combat_PlayerState::HIT_STUN:
			HandleHitStunState(fDt);
			break;

		case Combat_PlayerState::DEAD:
			// No updates when dead
			break;
		}
	}

	/**
	 * TriggerHitStun - Called when player takes damage
	 */
	void TriggerHitStun(float fDuration = 0.3f)
	{
		if (m_eState == Combat_PlayerState::DEAD)
			return;

		// Can't be hit while dodging (invincibility frames)
		if (m_eState == Combat_PlayerState::DODGING)
			return;

		m_eState = Combat_PlayerState::HIT_STUN;
		m_fStateTimer = fDuration;
		m_uComboCount = 0;
		m_bComboWindowOpen = false;
	}

	/**
	 * TriggerDeath - Called when player dies
	 */
	void TriggerDeath()
	{
		m_eState = Combat_PlayerState::DEAD;
		m_fStateTimer = 0.0f;
	}

	/**
	 * Reset - Reset to initial state
	 */
	void Reset()
	{
		m_eState = Combat_PlayerState::IDLE;
		m_fStateTimer = 0.0f;
		m_uComboCount = 0;
		m_bComboWindowOpen = false;
		m_fDodgeCooldownTimer = 0.0f;
		m_xMoveDirection = Zenith_Maths::Vector3(0.0f);
		m_fCurrentSpeed = 0.0f;
	}

private:
	// ========================================================================
	// State Handlers
	// ========================================================================

	void HandleMovementState(Zenith_TransformComponent& xTransform, Zenith_ColliderComponent& xCollider, float fDt)
	{
		// Check for attack input first
		if (CheckAttackInput())
			return;

		// Check for dodge input
		if (CheckDodgeInput(xTransform))
			return;

		// Handle movement
		Zenith_Maths::Vector3 xInput = GetMovementInput();
		m_fCurrentSpeed = glm::length(xInput);

		if (m_fCurrentSpeed > 0.01f)
		{
			m_xMoveDirection = glm::normalize(xInput);
			m_eState = Combat_PlayerState::WALKING;

			// Apply movement via physics
			if (xCollider.HasValidBody())
			{
				Zenith_Maths::Vector3 xVelocity = m_xMoveDirection * m_fMoveSpeed;
				xVelocity.y = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID()).y;  // Preserve Y velocity
				Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
			}

			// Rotate towards movement direction
			RotateTowards(xTransform, m_xMoveDirection, fDt);
			m_xFacingDirection = m_xMoveDirection;
		}
		else
		{
			m_eState = Combat_PlayerState::IDLE;
			m_xMoveDirection = Zenith_Maths::Vector3(0.0f);

			// Stop horizontal movement
			if (xCollider.HasValidBody())
			{
				Zenith_Maths::Vector3 xVelocity = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
				xVelocity.x = 0.0f;
				xVelocity.z = 0.0f;
				Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
			}
		}
	}

	void HandleAttackState(Zenith_TransformComponent& xTransform, float fDt)
	{
		m_fStateTimer -= fDt;

		// Check for combo continuation during window
		if (m_bComboWindowOpen && CheckAttackInput())
		{
			// Combo continuation handled in CheckAttackInput
			return;
		}

		// Attack finished
		if (m_fStateTimer <= 0.0f)
		{
			if (m_eState == Combat_PlayerState::LIGHT_ATTACK_1 ||
				m_eState == Combat_PlayerState::LIGHT_ATTACK_2)
			{
				// Open combo window
				m_bComboWindowOpen = true;
				m_fComboWindowTimer = m_fComboWindowTime;
			}
			else
			{
				// Heavy attack or third light - return to idle
				ReturnToIdle();
			}
		}
	}

	void HandleDodgeState(Zenith_TransformComponent& xTransform, Zenith_ColliderComponent& xCollider, float fDt)
	{
		m_fStateTimer -= fDt;

		// Apply dodge movement
		if (xCollider.HasValidBody())
		{
			Zenith_Maths::Vector3 xVelocity = m_xDodgeDirection * m_fDodgeSpeed;
			xVelocity.y = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID()).y;
			Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVelocity);
		}

		// Dodge finished
		if (m_fStateTimer <= 0.0f)
		{
			ReturnToIdle();
			m_fDodgeCooldownTimer = m_fDodgeCooldown;
		}
	}

	void HandleHitStunState(float fDt)
	{
		m_fStateTimer -= fDt;

		if (m_fStateTimer <= 0.0f)
		{
			ReturnToIdle();
		}
	}

	// ========================================================================
	// Input Handling
	// ========================================================================

	Zenith_Maths::Vector3 GetMovementInput()
	{
		Zenith_Maths::Vector3 xInput(0.0f);

		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_W) || Zenith_Input::IsKeyHeld(ZENITH_KEY_UP))
			xInput.z += 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_S) || Zenith_Input::IsKeyHeld(ZENITH_KEY_DOWN))
			xInput.z -= 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_A) || Zenith_Input::IsKeyHeld(ZENITH_KEY_LEFT))
			xInput.x -= 1.0f;
		if (Zenith_Input::IsKeyHeld(ZENITH_KEY_D) || Zenith_Input::IsKeyHeld(ZENITH_KEY_RIGHT))
			xInput.x += 1.0f;

		if (glm::length(xInput) > 0.01f)
			return glm::normalize(xInput);

		return xInput;
	}

	bool CheckAttackInput()
	{
		// Heavy attack (right click)
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_RIGHT))
		{
			if (CanMove())
			{
				StartAttack(Combat_AttackType::HEAVY);
				return true;
			}
		}

		// Light attack (left click)
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT))
		{
			if (CanMove())
			{
				StartAttack(Combat_AttackType::LIGHT);
				return true;
			}
			else if (IsInComboWindow())
			{
				ContinueCombo();
				return true;
			}
		}

		return false;
	}

	bool CheckDodgeInput(Zenith_TransformComponent& xTransform)
	{
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE))
		{
			if (CanDodge())
			{
				StartDodge(xTransform);
				return true;
			}
		}
		return false;
	}

	// ========================================================================
	// Attack Logic
	// ========================================================================

	void StartAttack(Combat_AttackType eType)
	{
		m_bAttackJustStarted = true;
		m_eCurrentAttackType = eType;

		if (eType == Combat_AttackType::HEAVY)
		{
			m_eState = Combat_PlayerState::HEAVY_ATTACK;
			m_fStateTimer = m_fHeavyAttackDuration;
			m_uComboCount = 0;  // Heavy resets combo
		}
		else
		{
			m_eState = Combat_PlayerState::LIGHT_ATTACK_1;
			m_fStateTimer = m_fLightAttackDuration;
			m_uComboCount = 1;
		}

		m_bComboWindowOpen = false;
	}

	void ContinueCombo()
	{
		m_bAttackJustStarted = true;
		m_eCurrentAttackType = Combat_AttackType::LIGHT;
		m_bComboWindowOpen = false;

		if (m_eState == Combat_PlayerState::LIGHT_ATTACK_1)
		{
			m_eState = Combat_PlayerState::LIGHT_ATTACK_2;
			m_uComboCount = 2;
		}
		else if (m_eState == Combat_PlayerState::LIGHT_ATTACK_2)
		{
			m_eState = Combat_PlayerState::LIGHT_ATTACK_3;
			m_uComboCount = 3;
		}

		m_fStateTimer = m_fLightAttackDuration;
	}

	bool IsInComboWindow() const
	{
		return m_bComboWindowOpen && m_fComboWindowTimer > 0.0f &&
			(m_eState == Combat_PlayerState::LIGHT_ATTACK_1 ||
			 m_eState == Combat_PlayerState::LIGHT_ATTACK_2);
	}

	// ========================================================================
	// Dodge Logic
	// ========================================================================

	void StartDodge(Zenith_TransformComponent& xTransform)
	{
		m_eState = Combat_PlayerState::DODGING;
		m_fStateTimer = m_fDodgeDuration;
		m_uComboCount = 0;
		m_bComboWindowOpen = false;

		// Dodge in movement direction, or backwards if not moving
		Zenith_Maths::Vector3 xInput = GetMovementInput();
		if (glm::length(xInput) > 0.01f)
		{
			m_xDodgeDirection = glm::normalize(xInput);
		}
		else
		{
			// Dodge backwards (opposite of facing)
			m_xDodgeDirection = -m_xFacingDirection;
		}
	}

	// ========================================================================
	// Helpers
	// ========================================================================

	void ReturnToIdle()
	{
		m_eState = Combat_PlayerState::IDLE;
		m_fStateTimer = 0.0f;
		m_eCurrentAttackType = Combat_AttackType::NONE;
		m_bComboWindowOpen = false;
	}

	void UpdateTimers(float fDt)
	{
		if (m_fDodgeCooldownTimer > 0.0f)
			m_fDodgeCooldownTimer -= fDt;

		if (m_bComboWindowOpen)
		{
			m_fComboWindowTimer -= fDt;
			if (m_fComboWindowTimer <= 0.0f)
			{
				ReturnToIdle();
			}
		}
	}

	void RotateTowards(Zenith_TransformComponent& xTransform, const Zenith_Maths::Vector3& xTargetDir, float fDt)
	{
		if (glm::length(xTargetDir) < 0.01f)
			return;

		// Get current rotation
		Zenith_Maths::Quat xCurrentRot;
		xTransform.GetRotation(xCurrentRot);

		// Calculate target rotation
		float fTargetYaw = atan2(xTargetDir.x, xTargetDir.z);
		Zenith_Maths::Quat xTargetRot = glm::angleAxis(fTargetYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));

		// Smooth rotation
		Zenith_Maths::Quat xNewRot = glm::slerp(xCurrentRot, xTargetRot, fDt * m_fRotationSpeed);
		xTransform.SetRotation(xNewRot);
	}

	// ========================================================================
	// State Variables
	// ========================================================================

	Combat_PlayerState m_eState = Combat_PlayerState::IDLE;
	Combat_AttackType m_eCurrentAttackType = Combat_AttackType::NONE;

	float m_fStateTimer = 0.0f;
	float m_fComboWindowTimer = 0.0f;
	float m_fDodgeCooldownTimer = 0.0f;

	uint32_t m_uComboCount = 0;
	bool m_bComboWindowOpen = false;
	bool m_bAttackJustStarted = false;

	Zenith_Maths::Vector3 m_xMoveDirection = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xFacingDirection = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	Zenith_Maths::Vector3 m_xDodgeDirection = Zenith_Maths::Vector3(0.0f);
	float m_fCurrentSpeed = 0.0f;
};
