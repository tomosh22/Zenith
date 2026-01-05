#pragma once
/**
 * Combat_AnimationController.h - Animation state machine for combat
 *
 * Demonstrates:
 * - Flux_AnimationStateMachine setup and usage
 * - Flux_AnimationParameters for state control
 * - State transitions with conditions
 * - Combo system with timed windows
 *
 * Since we don't have actual animation clips, this simulates the animation
 * state machine structure and timing. In a real game, this would integrate
 * with Flux_AnimationClip and skeletal animation.
 */

#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Maths/Zenith_Maths.h"
#include "Combat_PlayerController.h"

// ============================================================================
// Animation State Names
// ============================================================================

namespace CombatAnimStates
{
	static constexpr const char* IDLE = "Idle";
	static constexpr const char* WALK = "Walk";
	static constexpr const char* RUN = "Run";
	static constexpr const char* LIGHT_ATTACK_1 = "LightAttack1";
	static constexpr const char* LIGHT_ATTACK_2 = "LightAttack2";
	static constexpr const char* LIGHT_ATTACK_3 = "LightAttack3";
	static constexpr const char* HEAVY_ATTACK = "HeavyAttack";
	static constexpr const char* DODGE = "Dodge";
	static constexpr const char* HIT = "Hit";
	static constexpr const char* DEATH = "Death";
}

// ============================================================================
// Animation Parameter Names
// ============================================================================

namespace CombatAnimParams
{
	static constexpr const char* SPEED = "Speed";
	static constexpr const char* IS_ATTACKING = "IsAttacking";
	static constexpr const char* IS_DODGING = "IsDodging";
	static constexpr const char* IS_HIT = "IsHit";
	static constexpr const char* IS_DEAD = "IsDead";
	static constexpr const char* ATTACK_INDEX = "AttackIndex";  // 1, 2, or 3 for combo
	static constexpr const char* ATTACK_TRIGGER = "AttackTrigger";
	static constexpr const char* DODGE_TRIGGER = "DodgeTrigger";
	static constexpr const char* HIT_TRIGGER = "HitTrigger";
	static constexpr const char* DEATH_TRIGGER = "DeathTrigger";
}

// ============================================================================
// Animation Controller
// ============================================================================

/**
 * Combat_AnimationController - Manages combat animation state machine
 *
 * This controller simulates animation playback for procedural characters.
 * It tracks animation states and provides normalized time for procedural
 * animation blending.
 */
class Combat_AnimationController
{
public:
	// ========================================================================
	// Configuration
	// ========================================================================

	float m_fAnimationBlendTime = 0.15f;
	float m_fIdleToWalkThreshold = 0.1f;

	// ========================================================================
	// Initialization
	// ========================================================================

	/**
	 * Initialize - Set up the animation state machine
	 */
	void Initialize()
	{
		// Set up parameters
		m_xParameters.AddFloat(CombatAnimParams::SPEED, 0.0f);
		m_xParameters.AddBool(CombatAnimParams::IS_ATTACKING, false);
		m_xParameters.AddBool(CombatAnimParams::IS_DODGING, false);
		m_xParameters.AddBool(CombatAnimParams::IS_HIT, false);
		m_xParameters.AddBool(CombatAnimParams::IS_DEAD, false);
		m_xParameters.AddInt(CombatAnimParams::ATTACK_INDEX, 0);
		m_xParameters.AddTrigger(CombatAnimParams::ATTACK_TRIGGER);
		m_xParameters.AddTrigger(CombatAnimParams::DODGE_TRIGGER);
		m_xParameters.AddTrigger(CombatAnimParams::HIT_TRIGGER);
		m_xParameters.AddTrigger(CombatAnimParams::DEATH_TRIGGER);

		// Set initial state
		m_strCurrentState = CombatAnimStates::IDLE;
		m_fStateTime = 0.0f;
		m_fNormalizedTime = 0.0f;
	}

	/**
	 * Reset - Return to idle state
	 */
	void Reset()
	{
		m_strCurrentState = CombatAnimStates::IDLE;
		m_fStateTime = 0.0f;
		m_fNormalizedTime = 0.0f;
		m_xParameters.SetFloat(CombatAnimParams::SPEED, 0.0f);
		m_xParameters.SetBool(CombatAnimParams::IS_ATTACKING, false);
		m_xParameters.SetBool(CombatAnimParams::IS_DODGING, false);
		m_xParameters.SetBool(CombatAnimParams::IS_HIT, false);
		m_xParameters.SetBool(CombatAnimParams::IS_DEAD, false);
	}

	// ========================================================================
	// Update from Player State
	// ========================================================================

	/**
	 * UpdateFromPlayerState - Sync animation with player controller
	 */
	void UpdateFromPlayerState(const Combat_PlayerController& xPlayer, float fDt)
	{
		// Update speed parameter
		m_xParameters.SetFloat(CombatAnimParams::SPEED, xPlayer.GetMoveSpeed());

		// Determine target animation state based on player state
		std::string strTargetState = DetermineTargetState(xPlayer);

		// Handle state transitions
		if (strTargetState != m_strCurrentState)
		{
			TransitionToState(strTargetState);
		}

		// Update state time
		m_fStateTime += fDt;
		UpdateNormalizedTime(xPlayer);
	}

	/**
	 * UpdateForEnemy - Simpler update for enemy animations
	 */
	void UpdateForEnemy(float fSpeed, bool bIsAttacking, bool bIsHit, bool bIsDead, float fDt)
	{
		m_xParameters.SetFloat(CombatAnimParams::SPEED, fSpeed);

		std::string strTargetState;
		if (bIsDead)
		{
			strTargetState = CombatAnimStates::DEATH;
		}
		else if (bIsHit)
		{
			strTargetState = CombatAnimStates::HIT;
		}
		else if (bIsAttacking)
		{
			strTargetState = CombatAnimStates::LIGHT_ATTACK_1;
		}
		else if (fSpeed > m_fIdleToWalkThreshold)
		{
			strTargetState = CombatAnimStates::WALK;
		}
		else
		{
			strTargetState = CombatAnimStates::IDLE;
		}

		if (strTargetState != m_strCurrentState)
		{
			TransitionToState(strTargetState);
		}

		m_fStateTime += fDt;
		m_fNormalizedTime = CalculateNormalizedTime();
	}

	// ========================================================================
	// State Queries
	// ========================================================================

	const std::string& GetCurrentState() const { return m_strCurrentState; }
	float GetStateTime() const { return m_fStateTime; }
	float GetNormalizedTime() const { return m_fNormalizedTime; }
	bool IsTransitioning() const { return m_fTransitionProgress < 1.0f; }
	float GetTransitionProgress() const { return m_fTransitionProgress; }

	/**
	 * GetAnimationPhase - Get phase for procedural animation (0-1 per cycle)
	 */
	float GetAnimationPhase() const
	{
		if (m_strCurrentState == CombatAnimStates::WALK)
		{
			// Walking has a repeating cycle
			return fmod(m_fNormalizedTime, 1.0f);
		}
		return m_fNormalizedTime;
	}

	/**
	 * IsAttackHitFrame - Check if current frame is the "hit" frame of an attack
	 * This is when damage should be applied (typically middle of animation)
	 */
	bool IsAttackHitFrame() const
	{
		if (m_strCurrentState == CombatAnimStates::LIGHT_ATTACK_1 ||
			m_strCurrentState == CombatAnimStates::LIGHT_ATTACK_2 ||
			m_strCurrentState == CombatAnimStates::LIGHT_ATTACK_3)
		{
			// Hit frame at 40-60% of animation
			return m_fNormalizedTime >= 0.4f && m_fNormalizedTime <= 0.6f;
		}
		else if (m_strCurrentState == CombatAnimStates::HEAVY_ATTACK)
		{
			// Hit frame at 50-70% of animation (later due to windup)
			return m_fNormalizedTime >= 0.5f && m_fNormalizedTime <= 0.7f;
		}
		return false;
	}

	// ========================================================================
	// Procedural Animation Helpers
	// ========================================================================

	/**
	 * GetProceduralPose - Get procedural animation values for capsule character
	 *
	 * Returns animation offsets/rotations for procedural visualization:
	 * - Body bob for walking
	 * - Attack swing motion
	 * - Hit/death reactions
	 */
	struct ProceduralPose
	{
		float m_fBodyBob = 0.0f;          // Vertical offset
		float m_fBodyLean = 0.0f;         // Forward/back lean angle
		float m_fBodyTwist = 0.0f;        // Y-axis rotation offset
		Zenith_Maths::Vector3 m_xArmOffset = Zenith_Maths::Vector3(0.0f);  // Attack arm offset
	};

	ProceduralPose GetProceduralPose() const
	{
		ProceduralPose xPose;

		if (m_strCurrentState == CombatAnimStates::WALK)
		{
			// Walking bob
			float fPhase = GetAnimationPhase() * 6.28318f;  // 2*PI
			xPose.m_fBodyBob = sin(fPhase * 2.0f) * 0.05f;
			xPose.m_fBodyLean = 0.1f;  // Slight forward lean
		}
		else if (m_strCurrentState == CombatAnimStates::LIGHT_ATTACK_1 ||
				 m_strCurrentState == CombatAnimStates::LIGHT_ATTACK_2 ||
				 m_strCurrentState == CombatAnimStates::LIGHT_ATTACK_3)
		{
			// Attack swing
			float fSwing = sin(m_fNormalizedTime * 3.14159f);  // 0 to 1 to 0
			xPose.m_fBodyTwist = fSwing * 0.5f;  // Twist during swing
			xPose.m_xArmOffset = Zenith_Maths::Vector3(fSwing * 0.5f, 0.0f, fSwing * 0.3f);
		}
		else if (m_strCurrentState == CombatAnimStates::HEAVY_ATTACK)
		{
			// Heavy attack - bigger windup and swing
			float fWindup = std::min(m_fNormalizedTime * 2.0f, 1.0f);  // First half
			float fSwing = std::max((m_fNormalizedTime - 0.5f) * 2.0f, 0.0f);  // Second half

			if (m_fNormalizedTime < 0.5f)
			{
				xPose.m_fBodyLean = -fWindup * 0.2f;  // Lean back for windup
				xPose.m_fBodyTwist = -fWindup * 0.3f;
			}
			else
			{
				xPose.m_fBodyLean = fSwing * 0.3f;  // Lean forward for strike
				xPose.m_fBodyTwist = fSwing * 0.8f;
				xPose.m_xArmOffset = Zenith_Maths::Vector3(fSwing * 0.8f, 0.0f, fSwing * 0.5f);
			}
		}
		else if (m_strCurrentState == CombatAnimStates::DODGE)
		{
			// Dodge roll
			float fRoll = m_fNormalizedTime;
			xPose.m_fBodyBob = -sin(fRoll * 3.14159f) * 0.3f;  // Duck down
			xPose.m_fBodyLean = sin(fRoll * 3.14159f) * 0.5f;
		}
		else if (m_strCurrentState == CombatAnimStates::HIT)
		{
			// Hit stagger
			float fStagger = 1.0f - m_fNormalizedTime;  // Fade out
			xPose.m_fBodyLean = -fStagger * 0.3f;  // Knocked back
		}
		else if (m_strCurrentState == CombatAnimStates::DEATH)
		{
			// Fall over
			float fFall = std::min(m_fNormalizedTime * 2.0f, 1.0f);
			xPose.m_fBodyLean = -fFall * 1.5f;  // Fall backward
			xPose.m_fBodyBob = -fFall * 0.5f;   // Drop down
		}

		return xPose;
	}

private:
	// ========================================================================
	// State Determination
	// ========================================================================

	std::string DetermineTargetState(const Combat_PlayerController& xPlayer)
	{
		switch (xPlayer.GetState())
		{
		case Combat_PlayerState::IDLE:
			return CombatAnimStates::IDLE;

		case Combat_PlayerState::WALKING:
			return CombatAnimStates::WALK;

		case Combat_PlayerState::LIGHT_ATTACK_1:
			return CombatAnimStates::LIGHT_ATTACK_1;

		case Combat_PlayerState::LIGHT_ATTACK_2:
			return CombatAnimStates::LIGHT_ATTACK_2;

		case Combat_PlayerState::LIGHT_ATTACK_3:
			return CombatAnimStates::LIGHT_ATTACK_3;

		case Combat_PlayerState::HEAVY_ATTACK:
			return CombatAnimStates::HEAVY_ATTACK;

		case Combat_PlayerState::DODGING:
			return CombatAnimStates::DODGE;

		case Combat_PlayerState::HIT_STUN:
			return CombatAnimStates::HIT;

		case Combat_PlayerState::DEAD:
			return CombatAnimStates::DEATH;

		default:
			return CombatAnimStates::IDLE;
		}
	}

	void TransitionToState(const std::string& strNewState)
	{
		m_strPreviousState = m_strCurrentState;
		m_strCurrentState = strNewState;
		m_fStateTime = 0.0f;
		m_fNormalizedTime = 0.0f;
		m_fTransitionProgress = 0.0f;
	}

	void UpdateNormalizedTime(const Combat_PlayerController& xPlayer)
	{
		m_fNormalizedTime = CalculateNormalizedTime();

		// Update transition blend
		if (m_fTransitionProgress < 1.0f)
		{
			m_fTransitionProgress += (1.0f / m_fAnimationBlendTime) * 0.016f;  // Assuming ~60fps
			m_fTransitionProgress = std::min(m_fTransitionProgress, 1.0f);
		}
	}

	float CalculateNormalizedTime()
	{
		// Get duration for current state
		float fDuration = GetStateDuration();
		if (fDuration <= 0.0f)
			return 0.0f;

		if (m_strCurrentState == CombatAnimStates::WALK ||
			m_strCurrentState == CombatAnimStates::IDLE)
		{
			// Looping animations
			return fmod(m_fStateTime / fDuration, 1.0f);
		}
		else
		{
			// One-shot animations
			return std::min(m_fStateTime / fDuration, 1.0f);
		}
	}

	float GetStateDuration()
	{
		if (m_strCurrentState == CombatAnimStates::IDLE)
			return 2.0f;  // Idle cycle duration
		else if (m_strCurrentState == CombatAnimStates::WALK)
			return 0.6f;  // Walk cycle duration
		else if (m_strCurrentState == CombatAnimStates::LIGHT_ATTACK_1 ||
				 m_strCurrentState == CombatAnimStates::LIGHT_ATTACK_2 ||
				 m_strCurrentState == CombatAnimStates::LIGHT_ATTACK_3)
			return 0.3f;
		else if (m_strCurrentState == CombatAnimStates::HEAVY_ATTACK)
			return 0.6f;
		else if (m_strCurrentState == CombatAnimStates::DODGE)
			return 0.4f;
		else if (m_strCurrentState == CombatAnimStates::HIT)
			return 0.3f;
		else if (m_strCurrentState == CombatAnimStates::DEATH)
			return 1.0f;

		return 1.0f;
	}

	// ========================================================================
	// Data
	// ========================================================================

	Flux_AnimationParameters m_xParameters;
	std::string m_strCurrentState = CombatAnimStates::IDLE;
	std::string m_strPreviousState = CombatAnimStates::IDLE;

	float m_fStateTime = 0.0f;
	float m_fNormalizedTime = 0.0f;
	float m_fTransitionProgress = 1.0f;
};
