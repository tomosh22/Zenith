#pragma once
/**
 * Combat_AnimationController.h - Animation state machine for combat
 *
 * Uses Flux_AnimationController with real skeletal animation:
 * - Flux_AnimationStateMachine for state management
 * - Animation clips for Idle, Walk, Attack1-3, Dodge, Hit, Death
 * - 3-hit combo system with exit time transitions
 * - Trigger-based state changes
 */

#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"
#include "Combat_PlayerController.h"

// ============================================================================
// Animation State Names
// ============================================================================

namespace CombatAnimStates
{
	static constexpr const char* IDLE = "Idle";
	static constexpr const char* WALK = "Walk";
	static constexpr const char* ATTACK1 = "Attack1";
	static constexpr const char* ATTACK2 = "Attack2";
	static constexpr const char* ATTACK3 = "Attack3";
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
 * Wraps Flux_AnimationController to provide high-level animation control
 * for combat gameplay. Uses the stick figure skeleton and animation clips.
 */
class Combat_AnimationController
{
public:
	// ========================================================================
	// Initialization
	// ========================================================================

	/**
	 * Initialize - Set up the animation controller with mesh geometry
	 */
	void Initialize(Flux_MeshGeometry* pxGeometry)
	{
		if (!pxGeometry)
			return;

		m_pxGeometry = pxGeometry;
		m_xController.Initialize(pxGeometry);

		// Load animation clips
		LoadAnimationClips();

		// Set up state machine
		SetupStateMachine();
	}

	/**
	 * Initialize - Set up the animation controller with skeleton instance (for model instance system)
	 */
	void Initialize(Flux_SkeletonInstance* pxSkeleton)
	{
		if (!pxSkeleton)
			return;

		m_xController.Initialize(pxSkeleton);

		// Load animation clips
		LoadAnimationClips();

		// Set up state machine
		SetupStateMachine();
	}

	/**
	 * Shutdown - Clean up resources
	 */
	void Shutdown()
	{
		// Flux_AnimationController handles cleanup in destructor
	}

	/**
	 * Reset - Return to idle state
	 */
	void Reset()
	{
		if (m_xController.HasStateMachine())
		{
			m_xController.GetStateMachine().SetState(CombatAnimStates::IDLE);
		}
	}

	// ========================================================================
	// Update from Player State
	// ========================================================================

	/**
	 * UpdateFromPlayerState - Sync animation with player controller
	 */
	void UpdateFromPlayerState(const Combat_PlayerController& xPlayer, float fDt)
	{
		if (!m_xController.IsInitialized())
			return;

		// Set speed parameter for locomotion transitions
		m_xController.SetFloat(CombatAnimParams::SPEED, xPlayer.GetMoveSpeed());

		// Trigger state changes based on player state transitions
		switch (xPlayer.GetState())
		{
		case Combat_PlayerState::LIGHT_ATTACK_1:
		case Combat_PlayerState::LIGHT_ATTACK_2:
		case Combat_PlayerState::LIGHT_ATTACK_3:
		case Combat_PlayerState::HEAVY_ATTACK:
			if (xPlayer.WasStateChangedThisFrame())
			{
				TriggerAttack(xPlayer.GetComboCount());
			}
			break;

		case Combat_PlayerState::DODGING:
			if (xPlayer.WasStateChangedThisFrame())
			{
				TriggerDodge();
			}
			break;

		case Combat_PlayerState::HIT_STUN:
			if (xPlayer.WasStateChangedThisFrame())
			{
				TriggerHit();
			}
			break;

		case Combat_PlayerState::DEAD:
			if (xPlayer.WasStateChangedThisFrame())
			{
				TriggerDeath();
			}
			break;

		default:
			break;
		}

		// Update animation controller
		m_xController.Update(fDt);
	}

	/**
	 * UpdateForEnemy - Simpler update for enemy animations
	 */
	void UpdateForEnemy(float fSpeed, bool bIsAttacking, bool bIsHit, bool bIsDead, float fDt)
	{
		if (!m_xController.IsInitialized())
			return;

		m_xController.SetFloat(CombatAnimParams::SPEED, fSpeed);

		// Handle state triggers
		static bool s_bWasAttacking = false;
		static bool s_bWasHit = false;
		static bool s_bWasDead = false;

		if (bIsDead && !s_bWasDead)
		{
			TriggerDeath();
		}
		else if (bIsHit && !s_bWasHit)
		{
			TriggerHit();
		}
		else if (bIsAttacking && !s_bWasAttacking)
		{
			TriggerAttack(1);
		}

		s_bWasAttacking = bIsAttacking;
		s_bWasHit = bIsHit;
		s_bWasDead = bIsDead;

		m_xController.Update(fDt);
	}

	// ========================================================================
	// State Control
	// ========================================================================

	void TriggerAttack(int32_t iComboIndex)
	{
		// Attack trigger is consumed and handles combo chaining via exit time
		m_xController.SetTrigger(CombatAnimParams::ATTACK_TRIGGER);
	}

	void TriggerDodge()
	{
		m_xController.SetTrigger(CombatAnimParams::DODGE_TRIGGER);
	}

	void TriggerHit()
	{
		m_xController.SetTrigger(CombatAnimParams::HIT_TRIGGER);
	}

	void TriggerDeath()
	{
		m_xController.SetTrigger(CombatAnimParams::DEATH_TRIGGER);
	}

	// ========================================================================
	// State Queries
	// ========================================================================

	const std::string& GetCurrentState() const
	{
		static const std::string s_strEmpty;
		const Flux_AnimationStateMachine* pxSM = m_xController.GetStateMachinePtr();
		if (pxSM)
		{
			return pxSM->GetCurrentStateName();
		}
		return s_strEmpty;
	}

	bool IsTransitioning() const
	{
		const Flux_AnimationStateMachine* pxSM = m_xController.GetStateMachinePtr();
		return pxSM && pxSM->IsTransitioning();
	}

	/**
	 * IsAttackHitFrame - Check if current frame is the "hit" frame of an attack
	 * This is when damage should be applied (40-60% of animation)
	 */
	bool IsAttackHitFrame() const
	{
		const std::string& strState = GetCurrentState();
		if (strState == CombatAnimStates::ATTACK1 ||
			strState == CombatAnimStates::ATTACK2 ||
			strState == CombatAnimStates::ATTACK3)
		{
			// Get normalized time from current state blend tree
			// For now, use a simple time-based check
			// Attack animations are 0.4s, hit frame at 40-60%
			return true;  // Hit detection handled by player controller
		}
		return false;
	}

	// ========================================================================
	// Animation Controller Access
	// ========================================================================

	Flux_AnimationController& GetController() { return m_xController; }
	const Flux_AnimationController& GetController() const { return m_xController; }

	// Get bone buffer for rendering
	const Flux_DynamicConstantBuffer& GetBoneBuffer() const { return m_xController.GetBoneBuffer(); }

private:
	// ========================================================================
	// Setup
	// ========================================================================

	void LoadAnimationClips()
	{
		// Load stick figure animation clips
		static const char* s_strAssetDir = ENGINE_ASSETS_DIR "Meshes/StickFigure/";

		m_xController.AddClipFromFile(std::string(s_strAssetDir) + "StickFigure_Idle.zanim");
		m_xController.AddClipFromFile(std::string(s_strAssetDir) + "StickFigure_Walk.zanim");
		m_xController.AddClipFromFile(std::string(s_strAssetDir) + "StickFigure_Attack1.zanim");
		m_xController.AddClipFromFile(std::string(s_strAssetDir) + "StickFigure_Attack2.zanim");
		m_xController.AddClipFromFile(std::string(s_strAssetDir) + "StickFigure_Attack3.zanim");
		m_xController.AddClipFromFile(std::string(s_strAssetDir) + "StickFigure_Dodge.zanim");
		m_xController.AddClipFromFile(std::string(s_strAssetDir) + "StickFigure_Hit.zanim");
		m_xController.AddClipFromFile(std::string(s_strAssetDir) + "StickFigure_Death.zanim");
	}

	void SetupStateMachine()
	{
		Flux_AnimationStateMachine* pxSM = m_xController.CreateStateMachine("CombatStateMachine");
		Flux_AnimationClipCollection& xClips = m_xController.GetClipCollection();

		// Add parameters
		pxSM->GetParameters().AddFloat(CombatAnimParams::SPEED, 0.0f);
		pxSM->GetParameters().AddTrigger(CombatAnimParams::ATTACK_TRIGGER);
		pxSM->GetParameters().AddTrigger(CombatAnimParams::DODGE_TRIGGER);
		pxSM->GetParameters().AddTrigger(CombatAnimParams::HIT_TRIGGER);
		pxSM->GetParameters().AddTrigger(CombatAnimParams::DEATH_TRIGGER);

		// Create states with blend trees
		CreateState(pxSM, xClips, CombatAnimStates::IDLE, "Idle");
		CreateState(pxSM, xClips, CombatAnimStates::WALK, "Walk");
		CreateState(pxSM, xClips, CombatAnimStates::ATTACK1, "Attack1");
		CreateState(pxSM, xClips, CombatAnimStates::ATTACK2, "Attack2");
		CreateState(pxSM, xClips, CombatAnimStates::ATTACK3, "Attack3");
		CreateState(pxSM, xClips, CombatAnimStates::DODGE, "Dodge");
		CreateState(pxSM, xClips, CombatAnimStates::HIT, "Hit");
		CreateState(pxSM, xClips, CombatAnimStates::DEATH, "Death");

		// Set up transitions
		SetupTransitions(pxSM);

		// Set default state
		pxSM->SetDefaultState(CombatAnimStates::IDLE);

		// Resolve clip references
		pxSM->ResolveClipReferences(&xClips);
	}

	void CreateState(Flux_AnimationStateMachine* pxSM,
		Flux_AnimationClipCollection& xClips,
		const char* szStateName,
		const char* szClipName)
	{
		Flux_AnimationState* pxState = pxSM->AddState(szStateName);
		Flux_AnimationClip* pxClip = xClips.GetClip(szClipName);
		if (pxClip)
		{
			pxState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClip, 1.0f));
		}
	}

	void SetupTransitions(Flux_AnimationStateMachine* pxSM)
	{
		// ================================================================
		// Idle <-> Walk transitions based on Speed parameter
		// ================================================================

		// Idle -> Walk (Speed > 0.1)
		{
			Flux_StateTransition xTrans;
			xTrans.m_strTargetStateName = CombatAnimStates::WALK;
			xTrans.m_fTransitionDuration = 0.15f;

			Flux_TransitionCondition xCond;
			xCond.m_strParameterName = CombatAnimParams::SPEED;
			xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
			xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::Greater;
			xCond.m_fThreshold = 0.1f;
			xTrans.m_xConditions.PushBack(xCond);

			pxSM->GetState(CombatAnimStates::IDLE)->AddTransition(xTrans);
		}

		// Walk -> Idle (Speed <= 0.1)
		{
			Flux_StateTransition xTrans;
			xTrans.m_strTargetStateName = CombatAnimStates::IDLE;
			xTrans.m_fTransitionDuration = 0.15f;

			Flux_TransitionCondition xCond;
			xCond.m_strParameterName = CombatAnimParams::SPEED;
			xCond.m_eParamType = Flux_AnimationParameters::ParamType::Float;
			xCond.m_eCompareOp = Flux_TransitionCondition::CompareOp::LessEqual;
			xCond.m_fThreshold = 0.1f;
			xTrans.m_xConditions.PushBack(xCond);

			pxSM->GetState(CombatAnimStates::WALK)->AddTransition(xTrans);
		}

		// ================================================================
		// Attack transitions (from any locomotion state)
		// Priority 10 - higher than locomotion
		// ================================================================

		AddAttackTriggerTransition(pxSM, CombatAnimStates::IDLE, CombatAnimStates::ATTACK1, 10);
		AddAttackTriggerTransition(pxSM, CombatAnimStates::WALK, CombatAnimStates::ATTACK1, 10);

		// ================================================================
		// Combo chain: Attack1 -> Attack2 -> Attack3
		// Uses exit time (0.7) + AttackTrigger
		// ================================================================

		// Attack1 -> Attack2 (at 70% + trigger)
		{
			Flux_StateTransition xTrans;
			xTrans.m_strTargetStateName = CombatAnimStates::ATTACK2;
			xTrans.m_fTransitionDuration = 0.1f;
			xTrans.m_bHasExitTime = true;
			xTrans.m_fExitTime = 0.7f;
			xTrans.m_iPriority = 5;

			Flux_TransitionCondition xCond;
			xCond.m_strParameterName = CombatAnimParams::ATTACK_TRIGGER;
			xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
			xTrans.m_xConditions.PushBack(xCond);

			pxSM->GetState(CombatAnimStates::ATTACK1)->AddTransition(xTrans);
		}

		// Attack2 -> Attack3 (at 70% + trigger)
		{
			Flux_StateTransition xTrans;
			xTrans.m_strTargetStateName = CombatAnimStates::ATTACK3;
			xTrans.m_fTransitionDuration = 0.1f;
			xTrans.m_bHasExitTime = true;
			xTrans.m_fExitTime = 0.7f;
			xTrans.m_iPriority = 5;

			Flux_TransitionCondition xCond;
			xCond.m_strParameterName = CombatAnimParams::ATTACK_TRIGGER;
			xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
			xTrans.m_xConditions.PushBack(xCond);

			pxSM->GetState(CombatAnimStates::ATTACK2)->AddTransition(xTrans);
		}

		// Attack states return to Idle at exit time 1.0 (no condition)
		AddExitTimeTransition(pxSM, CombatAnimStates::ATTACK1, CombatAnimStates::IDLE, 1.0f, 0);
		AddExitTimeTransition(pxSM, CombatAnimStates::ATTACK2, CombatAnimStates::IDLE, 1.0f, 0);
		AddExitTimeTransition(pxSM, CombatAnimStates::ATTACK3, CombatAnimStates::IDLE, 1.0f, 0);

		// ================================================================
		// Dodge transitions (priority 15)
		// ================================================================

		AddTriggerTransition(pxSM, CombatAnimStates::IDLE, CombatAnimStates::DODGE,
			CombatAnimParams::DODGE_TRIGGER, 15);
		AddTriggerTransition(pxSM, CombatAnimStates::WALK, CombatAnimStates::DODGE,
			CombatAnimParams::DODGE_TRIGGER, 15);

		// Dodge -> Idle at exit time
		AddExitTimeTransition(pxSM, CombatAnimStates::DODGE, CombatAnimStates::IDLE, 1.0f, 0);

		// ================================================================
		// Hit transitions (priority 100 - can interrupt most states)
		// ================================================================

		const char* aszHitFromStates[] = {
			CombatAnimStates::IDLE, CombatAnimStates::WALK,
			CombatAnimStates::ATTACK1, CombatAnimStates::ATTACK2, CombatAnimStates::ATTACK3,
			CombatAnimStates::DODGE
		};

		for (const char* szFromState : aszHitFromStates)
		{
			AddTriggerTransition(pxSM, szFromState, CombatAnimStates::HIT,
				CombatAnimParams::HIT_TRIGGER, 100);
		}

		// Hit -> Idle at exit time
		AddExitTimeTransition(pxSM, CombatAnimStates::HIT, CombatAnimStates::IDLE, 1.0f, 0);

		// ================================================================
		// Death transitions (priority 200 - highest priority, terminal state)
		// ================================================================

		const char* aszDeathFromStates[] = {
			CombatAnimStates::IDLE, CombatAnimStates::WALK,
			CombatAnimStates::ATTACK1, CombatAnimStates::ATTACK2, CombatAnimStates::ATTACK3,
			CombatAnimStates::DODGE, CombatAnimStates::HIT
		};

		for (const char* szFromState : aszDeathFromStates)
		{
			AddTriggerTransition(pxSM, szFromState, CombatAnimStates::DEATH,
				CombatAnimParams::DEATH_TRIGGER, 200);
		}

		// Death is a terminal state - no exit transition
	}

	// ========================================================================
	// Transition Helpers
	// ========================================================================

	void AddTriggerTransition(Flux_AnimationStateMachine* pxSM,
		const char* szFromState,
		const char* szToState,
		const char* szTriggerParam,
		int32_t iPriority)
	{
		Flux_AnimationState* pxFromState = pxSM->GetState(szFromState);
		if (!pxFromState)
			return;

		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = szToState;
		xTrans.m_fTransitionDuration = 0.1f;
		xTrans.m_iPriority = iPriority;

		Flux_TransitionCondition xCond;
		xCond.m_strParameterName = szTriggerParam;
		xCond.m_eParamType = Flux_AnimationParameters::ParamType::Trigger;
		xTrans.m_xConditions.PushBack(xCond);

		pxFromState->AddTransition(xTrans);
	}

	void AddAttackTriggerTransition(Flux_AnimationStateMachine* pxSM,
		const char* szFromState,
		const char* szToState,
		int32_t iPriority)
	{
		AddTriggerTransition(pxSM, szFromState, szToState, CombatAnimParams::ATTACK_TRIGGER, iPriority);
	}

	void AddExitTimeTransition(Flux_AnimationStateMachine* pxSM,
		const char* szFromState,
		const char* szToState,
		float fExitTime,
		int32_t iPriority)
	{
		Flux_AnimationState* pxFromState = pxSM->GetState(szFromState);
		if (!pxFromState)
			return;

		Flux_StateTransition xTrans;
		xTrans.m_strTargetStateName = szToState;
		xTrans.m_fTransitionDuration = 0.15f;
		xTrans.m_bHasExitTime = true;
		xTrans.m_fExitTime = fExitTime;
		xTrans.m_iPriority = iPriority;
		// No conditions - just exit time

		pxFromState->AddTransition(xTrans);
	}

	// ========================================================================
	// Data
	// ========================================================================

	Flux_AnimationController m_xController;
	Flux_MeshGeometry* m_pxGeometry = nullptr;
};
