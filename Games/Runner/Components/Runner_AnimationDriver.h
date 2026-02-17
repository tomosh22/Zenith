#pragma once
/**
 * Runner_AnimationDriver.h - Animation state machine control
 *
 * Demonstrates:
 * - Flux_AnimationStateMachine - State machine with transitions
 * - Flux_BlendTreeNode_BlendSpace1D - Speed-based animation blending
 * - Animation parameter control from gameplay
 *
 * Animation States:
 * - Idle: Standing still
 * - Run: Speed-based blend between walk and sprint
 * - Jump: Jump up and fall
 * - Slide: Low slide under obstacles
 *
 * Note: Since this demo uses procedural geometry (capsule), we simulate
 * what the animation system would do. In a real game with skeletal meshes,
 * this would drive Flux_AnimationStateMachine directly.
 */

#include "Runner_CharacterController.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

// Forward declaration - in a real implementation these would be used
// class Flux_AnimationStateMachine;
// class Flux_BlendTreeNode_BlendSpace1D;

// ============================================================================
// Animation State (mirrors what Flux_AnimationStateMachine would track)
// ============================================================================
enum class RunnerAnimState
{
	IDLE,
	RUN,
	JUMP,
	SLIDE
};

/**
 * Runner_AnimationDriver - Controls animation based on gameplay state
 *
 * This class demonstrates how you would set up and control:
 * 1. Animation state machine with states: Idle, Run, Jump, Slide
 * 2. BlendSpace1D for speed-based run animation (walk -> jog -> sprint)
 * 3. Transition conditions based on parameters
 *
 * In this procedural demo, we simulate the visual effects by modifying
 * the character's scale and orientation.
 */
class Runner_AnimationDriver
{
public:
	// ========================================================================
	// Configuration
	// ========================================================================
	struct Config
	{
		float m_fRunAnimSpeedMultiplier = 1.0f;
		float m_fBlendSpaceMinSpeed = 0.0f;
		float m_fBlendSpaceMaxSpeed = 35.0f;
	};

	// ========================================================================
	// Initialization
	// ========================================================================
	static void Initialize(const Config& xConfig)
	{
		s_xConfig = xConfig;

		// In a real implementation with skeletal meshes, we would:
		// 1. Add Zenith_AnimatorComponent to the entity (auto-discovers skeleton from ModelComponent)
		// 2. Set up clips and state machine on the AnimatorComponent's controller
		// 3. Set parameters each frame from gameplay code
		// AnimatorComponent::OnUpdate handles evaluation, GPU upload, etc.

		/*
		// EXAMPLE: How to set up animation with Zenith_AnimatorComponent

		// Entity creation:
		Zenith_AnimatorComponent& xAnimator = xEntity.AddComponent<Zenith_AnimatorComponent>();
		// Skeleton is auto-discovered from ModelComponent during OnStart

		// Load clips and set up state machine:
		Flux_AnimationController& xController = xAnimator.GetController();
		xController.AddClipFromFile("Meshes/Character/Idle.zanim");
		xController.AddClipFromFile("Meshes/Character/Walk.zanim");

		Flux_AnimationStateMachine* pxSM = xController.CreateStateMachine("RunnerAnimations");
		pxSM->GetParameters().AddFloat("Speed", 0.0f);
		pxSM->GetParameters().AddTrigger("Jump");
		pxSM->GetParameters().AddTrigger("Slide");

		// Create states, transitions, etc. (same as Combat_AnimationController pattern)
		pxSM->SetDefaultState("Idle");

		// Each frame, just set parameters - AnimatorComponent handles the rest:
		xAnimator.SetFloat("Speed", fCurrentSpeed);
		if (bJumped)
			xAnimator.SetTrigger("Jump");
		*/

		Reset();
	}

	static void Reset()
	{
		s_eCurrentState = RunnerAnimState::IDLE;
		s_fBlendSpaceParameter = 0.0f;
		s_fStateTime = 0.0f;
		s_fJumpPhase = 0.0f;
	}

	// ========================================================================
	// Update (called each frame)
	// ========================================================================
	static void Update(float fDt, Zenith_TransformComponent& xTransform)
	{
		// Get character state for animation decisions
		RunnerCharacterState eCharState = Runner_CharacterController::GetState();
		float fSpeed = Runner_CharacterController::GetCurrentSpeed();

		// Update animation parameters (would feed into state machine)
		s_fBlendSpaceParameter = glm::clamp(fSpeed / s_xConfig.m_fBlendSpaceMaxSpeed, 0.0f, 1.0f);

		// State machine logic
		RunnerAnimState eNewState = s_eCurrentState;

		switch (eCharState)
		{
		case RunnerCharacterState::DEAD:
			// Keep last state
			break;

		case RunnerCharacterState::JUMPING:
			eNewState = RunnerAnimState::JUMP;
			break;

		case RunnerCharacterState::SLIDING:
			eNewState = RunnerAnimState::SLIDE;
			break;

		case RunnerCharacterState::RUNNING:
			if (fSpeed > 0.1f)
			{
				eNewState = RunnerAnimState::RUN;
			}
			else
			{
				eNewState = RunnerAnimState::IDLE;
			}
			break;
		}

		// Handle state transitions
		if (eNewState != s_eCurrentState)
		{
			OnStateExit();
			s_eCurrentState = eNewState;
			s_fStateTime = 0.0f;
			OnStateEnter(s_eCurrentState);
		}

		// Update state time
		s_fStateTime += fDt;

		// Apply visual animation (procedural for this demo)
		ApplyProceduralAnimation(fDt, xTransform);

		/*
		// In a real implementation with Zenith_AnimatorComponent:
		// Just set parameters each frame - AnimatorComponent::OnUpdate handles everything else
		xAnimator.SetFloat("Speed", fSpeed);
		if (eCharState == RunnerCharacterState::JUMPING)
			xAnimator.SetTrigger("Jump");
		if (eCharState == RunnerCharacterState::SLIDING)
			xAnimator.SetTrigger("Slide");
		*/
	}

	// ========================================================================
	// State Info
	// ========================================================================
	static RunnerAnimState GetCurrentState() { return s_eCurrentState; }
	static float GetBlendSpaceParameter() { return s_fBlendSpaceParameter; }
	static float GetStateTime() { return s_fStateTime; }

private:
	// ========================================================================
	// State Callbacks
	// ========================================================================
	static void OnStateEnter(RunnerAnimState eState)
	{
		switch (eState)
		{
		case RunnerAnimState::JUMP:
			s_fJumpPhase = 0.0f;
			break;
		case RunnerAnimState::SLIDE:
			// Slide entered
			break;
		default:
			break;
		}
	}

	static void OnStateExit()
	{
		// Cleanup if needed
	}

	// ========================================================================
	// Procedural Animation (simulates what skeletal animation would do)
	// ========================================================================
	static void ApplyProceduralAnimation(float fDt, Zenith_TransformComponent& xTransform)
	{
		Zenith_Maths::Vector3 xScale;
		xTransform.GetScale(xScale);

		// Base scale (capsule dimensions)
		float fHeight = Runner_CharacterController::GetCurrentCharacterHeight();
		float fRadius = 0.4f;

		switch (s_eCurrentState)
		{
		case RunnerAnimState::IDLE:
			// Slight breathing motion
			{
				float fBreath = 1.0f + sin(s_fStateTime * 2.0f) * 0.02f;
				xScale = Zenith_Maths::Vector3(fRadius * 2.0f, fHeight * fBreath, fRadius * 2.0f);
			}
			break;

		case RunnerAnimState::RUN:
			// Running bob motion based on speed
			{
				float fBobFreq = 8.0f + s_fBlendSpaceParameter * 4.0f;
				float fBobAmp = 0.03f + s_fBlendSpaceParameter * 0.02f;
				float fBob = 1.0f + sin(s_fStateTime * fBobFreq) * fBobAmp;
				xScale = Zenith_Maths::Vector3(fRadius * 2.0f * fBob, fHeight, fRadius * 2.0f);
			}
			break;

		case RunnerAnimState::JUMP:
			// Jump stretch/squash
			{
				s_fJumpPhase += fDt;
				float fStretch = 1.0f;
				if (s_fJumpPhase < 0.1f)
				{
					// Launch stretch
					fStretch = 1.0f + s_fJumpPhase * 3.0f;
				}
				else
				{
					// Air time - slight tuck
					fStretch = 1.2f - (s_fJumpPhase - 0.1f) * 0.5f;
					fStretch = glm::max(fStretch, 0.9f);
				}
				xScale = Zenith_Maths::Vector3(fRadius * 2.0f / fStretch, fHeight * fStretch, fRadius * 2.0f / fStretch);
			}
			break;

		case RunnerAnimState::SLIDE:
			// Low sliding crouch
			{
				float fCrouch = Runner_CharacterController::GetCurrentCharacterHeight() / fHeight;
				xScale = Zenith_Maths::Vector3(fRadius * 2.0f * 1.5f, fHeight * fCrouch, fRadius * 2.0f);
			}
			break;
		}

		xTransform.SetScale(xScale);
	}

	// ========================================================================
	// Static State
	// ========================================================================
	static inline Config s_xConfig;
	static inline RunnerAnimState s_eCurrentState = RunnerAnimState::IDLE;
	static inline float s_fBlendSpaceParameter = 0.0f;
	static inline float s_fStateTime = 0.0f;
	static inline float s_fJumpPhase = 0.0f;

	// In real implementation with skeletal meshes:
	// Use Zenith_AnimatorComponent on the entity (auto-discovers skeleton, auto-updates)
};
