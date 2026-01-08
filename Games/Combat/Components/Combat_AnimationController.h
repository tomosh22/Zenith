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
#include "Flux/MeshAnimation/Flux_BlendTree.h"
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

	// BlendSpace2D parameters for movement direction
	static constexpr const char* MOVE_X = "MoveX";
	static constexpr const char* MOVE_Y = "MoveY";

	// Additive blend parameters for hit reactions
	static constexpr const char* HIT_STRENGTH = "HitStrength";

	// Masked blend parameters for upper/lower body split
	static constexpr const char* UPPER_BODY_WEIGHT = "UpperBodyWeight";
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

		// Set up blend tree parameters
		m_xParameters.AddFloat(CombatAnimParams::MOVE_X, 0.0f);
		m_xParameters.AddFloat(CombatAnimParams::MOVE_Y, 0.0f);
		m_xParameters.AddFloat(CombatAnimParams::HIT_STRENGTH, 0.0f);
		m_xParameters.AddFloat(CombatAnimParams::UPPER_BODY_WEIGHT, 0.0f);

		// Set initial state
		m_strCurrentState = CombatAnimStates::IDLE;
		m_fStateTime = 0.0f;
		m_fNormalizedTime = 0.0f;

		// Initialize blend trees
		InitializeBlendTrees();
	}

	/**
	 * Shutdown - Clean up blend tree resources
	 */
	void Shutdown()
	{
		delete m_pxMovementBlendSpace;
		delete m_pxHitReactionAdditive;
		delete m_pxUpperBodyMask;
		delete m_pxAttackSelector;
		m_pxMovementBlendSpace = nullptr;
		m_pxHitReactionAdditive = nullptr;
		m_pxUpperBodyMask = nullptr;
		m_pxAttackSelector = nullptr;
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
	// Blend Tree Initialization
	// ========================================================================

	/**
	 * InitializeBlendTrees - Set up all blend tree nodes
	 *
	 * Demonstrates usage of all 4 unused blend tree variants:
	 * - BlendSpace2D: Movement direction blending (strafe + forward/back)
	 * - Select: Attack combo variations
	 * - Additive: Hit reaction layered on base animation
	 * - Masked: Upper body attacks while lower body walks
	 */
	void InitializeBlendTrees()
	{
		// ----------------------------------------------------------------
		// BlendSpace2D - Movement direction blending
		// Used for 8-directional movement: combines strafe (X) and forward/back (Y)
		// ----------------------------------------------------------------
		m_pxMovementBlendSpace = new Flux_BlendTreeNode_BlendSpace2D();

		// Add blend points for 4 cardinal directions
		// In a real game, these would have actual animation clips
		Flux_BlendTreeNode_Clip* pxForward = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxBackward = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxLeft = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxRight = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		m_pxMovementBlendSpace->AddBlendPoint(pxForward, Zenith_Maths::Vector2(0.0f, 1.0f));   // Forward
		m_pxMovementBlendSpace->AddBlendPoint(pxBackward, Zenith_Maths::Vector2(0.0f, -1.0f)); // Backward
		m_pxMovementBlendSpace->AddBlendPoint(pxLeft, Zenith_Maths::Vector2(-1.0f, 0.0f));     // Left strafe
		m_pxMovementBlendSpace->AddBlendPoint(pxRight, Zenith_Maths::Vector2(1.0f, 0.0f));     // Right strafe
		m_pxMovementBlendSpace->ComputeTriangulation();

		// ----------------------------------------------------------------
		// Select - Attack combo variations
		// Selects one of 3 attack animations based on combo index
		// ----------------------------------------------------------------
		m_pxAttackSelector = new Flux_BlendTreeNode_Select();

		// Add 3 attack variations (combo chain)
		Flux_BlendTreeNode_Clip* pxAttack1 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxAttack2 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxAttack3 = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		m_pxAttackSelector->AddChild(pxAttack1);  // Index 0: Light Attack 1
		m_pxAttackSelector->AddChild(pxAttack2);  // Index 1: Light Attack 2
		m_pxAttackSelector->AddChild(pxAttack3);  // Index 2: Light Attack 3

		// ----------------------------------------------------------------
		// Additive - Hit reaction layer
		// Layers hit stagger animation on top of base movement/attack
		// ----------------------------------------------------------------
		Flux_BlendTreeNode_Clip* pxBaseIdle = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxHitReaction = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		m_pxHitReactionAdditive = new Flux_BlendTreeNode_Additive(pxBaseIdle, pxHitReaction, 0.0f);

		// ----------------------------------------------------------------
		// Masked - Upper/Lower body split
		// Allows attacking while walking (upper body attack, lower body walk)
		// ----------------------------------------------------------------
		Flux_BlendTreeNode_Clip* pxWalkBase = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);
		Flux_BlendTreeNode_Clip* pxAttackOverride = new Flux_BlendTreeNode_Clip(nullptr, 1.0f);

		// Set up upper body mask (spine, arms, head would have weight 1.0)
		Flux_BoneMask xUpperBodyMask;
		xUpperBodyMask.SetBoneWeight(0, 1.0f);  // Spine
		xUpperBodyMask.SetBoneWeight(1, 1.0f);  // Chest
		xUpperBodyMask.SetBoneWeight(2, 1.0f);  // Left Shoulder
		xUpperBodyMask.SetBoneWeight(3, 1.0f);  // Left Arm
		xUpperBodyMask.SetBoneWeight(4, 1.0f);  // Right Shoulder
		xUpperBodyMask.SetBoneWeight(5, 1.0f);  // Right Arm
		xUpperBodyMask.SetBoneWeight(6, 0.0f);  // Hips (lower body, no mask)
		xUpperBodyMask.SetBoneWeight(7, 0.0f);  // Left Leg
		xUpperBodyMask.SetBoneWeight(8, 0.0f);  // Right Leg

		m_pxUpperBodyMask = new Flux_BlendTreeNode_Masked(pxWalkBase, pxAttackOverride, xUpperBodyMask);
	}

	/**
	 * UpdateBlendTrees - Update blend tree parameters based on game state
	 */
	void UpdateBlendTrees(const Combat_PlayerController& xPlayer)
	{
		// Update BlendSpace2D with movement direction
		Zenith_Maths::Vector3 xMoveDir = xPlayer.GetMoveDirection();
		m_pxMovementBlendSpace->SetParameter(Zenith_Maths::Vector2(xMoveDir.x, xMoveDir.z));

		// Update Select with attack combo index
		int32_t iAttackIndex = m_xParameters.GetInt(CombatAnimParams::ATTACK_INDEX);
		if (iAttackIndex >= 1 && iAttackIndex <= 3)
		{
			m_pxAttackSelector->SetSelectedIndex(iAttackIndex - 1);  // Convert 1-3 to 0-2
		}

		// Update Additive weight based on hit state
		bool bIsHit = m_xParameters.GetBool(CombatAnimParams::IS_HIT);
		float fHitStrength = bIsHit ? 1.0f : 0.0f;
		m_pxHitReactionAdditive->SetAdditiveWeight(fHitStrength);

		// Update Masked weight based on attacking while moving
		bool bIsAttacking = m_xParameters.GetBool(CombatAnimParams::IS_ATTACKING);
		float fSpeed = m_xParameters.GetFloat(CombatAnimParams::SPEED);
		bool bAttackWhileMoving = bIsAttacking && fSpeed > m_fIdleToWalkThreshold;

		// Update stored parameters for reference
		m_xParameters.SetFloat(CombatAnimParams::MOVE_X, xMoveDir.x);
		m_xParameters.SetFloat(CombatAnimParams::MOVE_Y, xMoveDir.z);
		m_xParameters.SetFloat(CombatAnimParams::HIT_STRENGTH, fHitStrength);
		m_xParameters.SetFloat(CombatAnimParams::UPPER_BODY_WEIGHT, bAttackWhileMoving ? 1.0f : 0.0f);
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

	// Blend tree nodes demonstrating all 4 unused variants
	Flux_BlendTreeNode_BlendSpace2D* m_pxMovementBlendSpace = nullptr;  // Movement direction
	Flux_BlendTreeNode_Additive* m_pxHitReactionAdditive = nullptr;     // Hit reaction layer
	Flux_BlendTreeNode_Masked* m_pxUpperBodyMask = nullptr;             // Upper/lower body split
	Flux_BlendTreeNode_Select* m_pxAttackSelector = nullptr;            // Attack combo selection
};
