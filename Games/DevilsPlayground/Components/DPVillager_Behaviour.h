#pragma once
/**
 * DPVillager_Behaviour - Possessable villager (DevilsPlayground port).
 *
 * 17 villagers are placed around L_GameLevel (was 14 in early port milestones; extended during M0.5). The player click-to-possesses
 * one at a time. Possessed villager moves under WASD; un-possessed villagers
 * stand still. Possession bumps the villager's remaining-life timer to a
 * fixed value; when it ticks to zero, the villager dies and the player must
 * possess another. Win condition is the pentagram, not survival.
 *
 * SourceBugFixed (RemoveHeldItem): the source RemoveHeldItem null-derefs.
 * The fix lives in DP_Player::RemoveHeldItem (PublicInterfaces.cpp).
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Physics/Zenith_PhysicsImpl.h"
#include "Input/Zenith_InputImpl.h"
#include "Maths/Zenith_Maths.h"
#include "Core/Zenith_AudioBus.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Flux/Flux_ModelInstance.h"
#include "AssetHandling/Zenith_MaterialAsset.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPInputActions.h"
#include "Source/DPTutorial.h"
#include "Source/DPMaterials.h"
#include "Source/DP_Tuning.h"
#include "Source/DP_Archetypes.h"

#include <cstdio>

// MVP-1.4.1-3: villager lifecycle state machine.
//
// Idle:      not possessed; life > 0; available for possession.
// Possessed: currently the player's vessel; TickLife/TickMovement run.
// Fainted:   voluntarily switched off by the player. Recovers to Idle
//            after `possession.faint_recovery_s` seconds. Refused by
//            TryVoluntaryPossessSwitch (player can't re-possess until
//            recovered). System path SetPossessedVillager bypasses the
//            refusal as a debug/test backdoor.
// Dead:      life timer expired (or test-driven Kill()). Terminal --
//            permanently unavailable. Distinct from Fainted so a future
//            GDD "permanent vessel pool decrement" mechanic has a clean
//            boundary.
enum class DPVillagerState : uint8_t
{
	Idle,
	Possessed,
	Fainted,
	Dead
};

class DPVillager_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPVillager_Behaviour)

	DPVillager_Behaviour() = delete;
	DPVillager_Behaviour(Zenith_Entity& /*xParentEntity*/)
	{
		// m_xParentEntity is assigned by Zenith_ScriptComponent after CreateInstance
		// returns; no need to forward here.
	}

	~DPVillager_Behaviour() = default;

	void OnAwake() ZENITH_FINAL override
	{
		// MVP-0.2.3: apply the archetype's life_timer + jog_speed. Archetype
		// id is stored in m_strArchetypeId; default "Farmhand" matches the
		// pre-MVP-0.2.3 behaviour (life=30s, jog=8m/s). Per-villager authoring
		// or test setup can override via ApplyArchetype("Beggar") /
		// ApplyArchetype("Child") to switch stats before OnAwake fires (call
		// pattern: SetArchetype on the freshly-attached script before the
		// Awake wave drains in EditorAutomation), or after OnAwake to retune
		// at runtime.
		ApplyArchetype(m_strArchetypeId.c_str());

		// Reset transient state — Editor Stop/Play would otherwise leave a
		// stale possession flag from a previous play session.
		m_bIsPossessed = false;
		m_fRemainingLife = m_fMaxLife;

		// Villagers use a DYNAMIC capsule rigid body so Jolt resolves wall
		// collisions natively (the player drives the possessed villager via
		// SetLinearVelocity in TickMovement). Two configurations needed:
		//   - Gravity off: top-down game, the floor is a flush slab and we
		//     don't want the body to drift downward into it.
		//   - Lock pitch + roll: the capsule can yaw freely (so the visual
		//     mesh can face its movement direction in future), but should
		//     never tip over from a glancing wall hit. EnforceUpright /
		//     LockRotation reset both axes' inverse inertia to zero so
		//     Jolt won't even try to integrate them.
		if (m_xParentEntity.HasComponent<Zenith_ColliderComponent>())
		{
			Zenith_ColliderComponent& xCollider =
				m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
			if (xCollider.HasValidBody())
			{
				const JPH::BodyID& xBodyID = xCollider.GetBodyID();
				g_xEngine.Physics().SetGravityEnabled(xBodyID, false);
				g_xEngine.Physics().LockRotation(xBodyID, /*X=*/true, /*Y=*/false, /*Z=*/true);
			}
		}
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		// MVP-1.4.1-3: state-machine-driven possession transitions.
		// Observe DP_Player's possessed handle each frame and drive
		// m_eState through Idle/Possessed/Fainted/Dead.
		//
		// Transition table:
		//   Idle      + possessed         -> Possessed  (bump life to max)
		//   Possessed + un-possessed      -> Fainted    (arm faint timer)
		//                                                EXCEPT if we're
		//                                                already Dead --
		//                                                Kill() ran inline
		//                                                from TickLife and
		//                                                set state=Dead
		//                                                before OnUpdate
		//                                                got a chance to
		//                                                observe the
		//                                                un-possession.
		//   Fainted   + (timer expires)   -> Idle
		//   Fainted   + possessed (SYSTEM path bypass) -> Possessed
		//                                                (test backdoor;
		//                                                 voluntary switch
		//                                                 is refused at
		//                                                 the TryVoluntary-
		//                                                 PossessSwitch
		//                                                 level via
		//                                                 IsPossessable())
		//   Dead      + *                 -> Dead (terminal)
		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
		const bool bIsPossessedThisFrame =
			(xPossessed.m_uIndex == m_xParentEntity.GetEntityID().m_uIndex)
			&& (xPossessed.m_uGeneration == m_xParentEntity.GetEntityID().m_uGeneration);
		const DPVillagerState ePrevState = m_eState;

		switch (m_eState)
		{
		case DPVillagerState::Idle:
			if (bIsPossessedThisFrame)
			{
				m_eState = DPVillagerState::Possessed;
				m_fRemainingLife = m_fMaxLife;
			}
			break;
		case DPVillagerState::Possessed:
			if (!bIsPossessedThisFrame)
			{
				// We were the possessed villager last frame and we're
				// not now. Two ways this can happen:
				//   1. Voluntary switch: player chose another vessel.
				//      Kill() did NOT run; m_bDispatchedDeath is false.
				//      -> Fainted.
				//   2. Death: Kill() already set m_eState=Dead inline
				//      from TickLife. We won't reach this branch then
				//      because the switch's outer case is Dead, not
				//      Possessed. So unconditional Fainted is correct.
				m_eState = DPVillagerState::Fainted;
				m_fFaintRecoveryRemaining =
					DP_Tuning::Get<float>("possession.voluntary_switch_faint_recovery_s");
			}
			break;
		case DPVillagerState::Fainted:
			if (bIsPossessedThisFrame)
			{
				// System path bypass -- e.g., a test calling
				// SetPossessedVillager(faintedVillager) directly.
				// The voluntary-switch path refuses this via
				// IsPossessable(); reaching here means the developer
				// chose to override. Wake up.
				m_eState = DPVillagerState::Possessed;
				m_fRemainingLife = m_fMaxLife;
				m_fFaintRecoveryRemaining = 0.0f;
			}
			else
			{
				m_fFaintRecoveryRemaining -= fDt;
				if (m_fFaintRecoveryRemaining <= 0.0f)
				{
					m_fFaintRecoveryRemaining = 0.0f;
					m_eState = DPVillagerState::Idle;
				}
			}
			break;
		case DPVillagerState::Dead:
			// Terminal.
			break;
		}

		// Legacy flag sync. Kept for the existing public API
		// (IsPossessed()) and the rest of OnUpdate's gate below.
		m_bIsPossessed = (m_eState == DPVillagerState::Possessed);
		const bool bWasPossessed = (ePrevState == DPVillagerState::Possessed);

		// Swap to / from the possessed-tint material on transition. Only swap
		// when the possession state actually flipped to avoid thrashing the
		// material slot every frame.
		if (m_bIsPossessed != bWasPossessed)
		{
			ApplyPossessionMaterial(m_bIsPossessed);
		}

		// Held-item visual: poll the public-interface every frame and reflect
		// changes by spawning/destroying a child marker entity. The marker
		// re-uses the prototype cube tinted by tag (DPItemBase's tinting is
		// not reused; we build a fresh entity that has the right tag-coloured
		// material via DPMaterials::GetOrCreateColouredVariant).
		const DP_ItemTag eHeldNow = DP_Player::GetHeldItemTag(m_xParentEntity.GetEntityID());
		if (eHeldNow != m_eLastSeenHeldTag)
		{
			ApplyHeldItemVisual(eHeldNow);
			m_eLastSeenHeldTag = eHeldNow;
		}

		if (m_bIsPossessed)
		{
			// MVP-1.7: compute movement-state booleans once per frame so
			// TickLife / TickMovement / TickFootsteps all agree. Sprint
			// requires Shift+moving; walk-quiet requires Ctrl+moving;
			// sprint wins ties so Shift+Ctrl resolves to sprint.
			const Zenith_Maths::Vector2 xMove = DP_Input::ReadMoveVillager();
			const float fMoveLen = glm::length(xMove);
			const bool bMoving = (fMoveLen > 0.01f);
			m_bIsSprintingNow = DP_Input::ReadSprintHeld() && bMoving;
			m_bIsWalkQuietNow = !m_bIsSprintingNow
				&& DP_Input::ReadWalkQuietHeld() && bMoving;
			// 2026-05-21: first-encounter tutorialisation hooks.
			// Sprint + walk-quiet are continuous states without a
			// dedicated event, so the tutorial fires programmatically
			// on the first frame each state is active.
			if (m_bIsSprintingNow) DP_Tutorial::TriggerIfFirstTime(DP_Tutorial::Kind::FirstSprintUse);
			if (m_bIsWalkQuietNow) DP_Tutorial::TriggerIfFirstTime(DP_Tutorial::Kind::FirstWalkQuietUse);
			TickLife(fDt);
			TickMovement(fDt);
			TickFootsteps(fDt, bMoving);
		}
		else
		{
			m_bIsSprintingNow = false;
			m_bIsWalkQuietNow = false;
			ZeroHorizontalVelocity();
		}

		// Re-anchor the held visual to the villager's current world position.
		// The marker isn't a proper child entity (no auto-follow yet), so we
		// reposition it every frame.
		if (m_xHeldItemVisual.IsValid())
		{
			PositionHeldItemVisual();
		}
	}

	float GetRemainingLife() const { return m_fRemainingLife; }
	float GetMaxLife() const { return m_fMaxLife; }
	const std::string& GetArchetypeId() const { return m_strArchetypeId; }

	// Re-resolve stats from DP_Archetypes for a new archetype id. Persists
	// the id and re-seeds m_fMaxLife + m_fMoveSpeed; resets m_fRemainingLife
	// only if the villager isn't currently possessed (a mid-possession swap
	// would otherwise interrupt the player's life-timer countdown). MVP-0.2.3
	// authoring path: scene authoring calls SetArchetype("...") on the
	// freshly-attached script before the OnAwake wave drains so the entity
	// awakens with archetype-correct stats. Falls back to DP_Tuning's
	// possession.life_timer_default_s + movement.jog_speed_mps if the
	// archetype id is missing or DP_Archetypes wasn't initialized.
	void ApplyArchetype(const char* szId)
	{
		if (szId != nullptr) m_strArchetypeId = szId;
		float fLife  = DP_Tuning::Get<float>("possession.life_timer_default_s");
		float fSpeed = DP_Tuning::Get<float>("movement.jog_speed_mps");
		if (m_strArchetypeId.empty()) m_strArchetypeId = "Farmhand";
		const DP_Archetypes::Archetype* pxA = nullptr;
		if (DP_Archetypes::Count() > 0)
		{
			// Use FindByIndex linear scan so a missing-id silently falls back
			// to DP_Tuning rather than asserting (matches the soft-fail style
			// of LoadModel for missing assets on fresh CI checkouts).
			for (size_t u = 0; u < DP_Archetypes::Count(); ++u)
			{
				const DP_Archetypes::Archetype* pxCandidate = DP_Archetypes::GetByIndex(u);
				if (pxCandidate && pxCandidate->id == m_strArchetypeId)
				{
					pxA = pxCandidate;
					break;
				}
			}
		}
		if (pxA != nullptr)
		{
			fLife  = pxA->life_timer_s;
			fSpeed = pxA->jog_speed_mps;
		}
		m_fMaxLife   = fLife;
		m_fMoveSpeed = fSpeed;
		if (!m_bIsPossessed)
		{
			m_fRemainingLife = m_fMaxLife;
		}
	}

	// Pre-OnAwake authoring setter: stash the id so the next OnAwake call
	// resolves with the new archetype. Does NOT immediately apply -- safe to
	// call from EditorAutomation before the Awake wave fires.
	void SetArchetype(const char* szId)
	{
		if (szId != nullptr) m_strArchetypeId = szId;
	}
	// Test-only accessor — MVP-0.1.2's Test_P1Villager_TuningMigration reads
	// the move speed back to verify it matches DP_Tuning's
	// movement.jog_speed_mps after OnAwake. Production gameplay never reads
	// the move speed externally (it's only consumed inside TickMovement).
	float GetMoveSpeed() const { return m_fMoveSpeed; }
	bool IsPossessed() const { return m_bIsPossessed; }

	// MVP-1.4.1-3 state accessors.
	DPVillagerState GetState() const { return m_eState; }

	// Whether the player's voluntary-switch path is allowed to
	// possess this villager. Refused for Fainted (still recovering)
	// and Dead (permanent). Idle and Possessed both pass -- re-
	// clicking the same villager is idempotent in
	// TryVoluntaryPossessSwitch.
	bool IsPossessable() const
	{
		return m_eState == DPVillagerState::Idle
			|| m_eState == DPVillagerState::Possessed;
	}

	// Diagnostic only -- the recovery countdown while Fainted, 0
	// otherwise. Tests inspect this to assert the timer arms on
	// voluntary switch.
	float GetFaintRecoveryRemaining() const { return m_fFaintRecoveryRemaining; }
	// MVP-1.7: test accessor -- returns true if Shift was held AND the
	// villager was actually moving on the most recent OnUpdate. Used by
	// Test_P1Sprint_* to verify the sprint state machine without
	// faking input.
	bool IsSprintingNow() const { return m_bIsSprintingNow; }
	// MVP-1.7: walk-quiet cache; same shape as IsSprintingNow.
	bool IsWalkQuietNow() const { return m_bIsWalkQuietNow; }

	// Test/debug only: shrink the timer so death-by-timeout tests don't need
	// 1800+ simulated frames to fire. Production gameplay sets m_fMaxLife
	// once at authoring and never touches it again.
	void SetRemainingLifeForTest(float fSeconds) { m_fRemainingLife = fSeconds; }

	// MVP-1.4 test accessor: shortcut the 10s faint-recovery countdown
	// so Test_P1Faint_RecoversToIdle can verify the Fainted->Idle
	// transition without ticking 600 frames. The OnUpdate state machine
	// reads m_fFaintRecoveryRemaining and transitions to Idle on the
	// frame it reaches 0; the test sets it close to 0 and confirms the
	// next-frame OnUpdate makes the transition.
	void SetFaintRecoveryForTest(float fSeconds) { m_fFaintRecoveryRemaining = fSeconds; }

private:
	void TickLife(float fDt)
	{
		// MVP-1.7: sprinting AND moving drains an extra
		// movement.sprint_life_cost_extra_per_s on TOP of the baseline
		// 1.0 s/s drain. The "AND moving" condition is enforced by
		// OnUpdate's m_bIsSprintingNow computation so a player who
		// holds Shift while standing still doesn't burn life for
		// nothing (Test_P1Sprint_NoDrainWhenNotMoving).
		float fDrain = fDt;
		if (m_bIsSprintingNow)
		{
			const float fExtra =
				DP_Tuning::Get<float>("movement.sprint_life_cost_extra_per_s");
			fDrain += fExtra * fDt;
		}
		m_fRemainingLife -= fDrain;
		if (m_fRemainingLife <= 0.0f)
		{
			// MVP-1.3.5: route the death through Kill() so the
			// NoVessels-detection scan runs alongside the per-death
			// DP_OnVillagerDied dispatch. Previously the dispatch
			// happened inline here; refactored so a test (or future
			// non-natural-cause death path) can fire the same
			// sequence without driving TickLife to drain.
			Kill();
		}
	}

public:
	// MVP-1.3.5: dispatch a villager's death + scan for "no remaining
	// vessels" (last alive villager just died -> dispatch
	// DP_OnRunLost{NoVessels}).
	//
	// Production path: called by TickLife when m_fRemainingLife
	// reaches 0. Test path: called directly by
	// Test_P1NoVessels_DispatchesRunLost to drive 17 deaths without
	// needing a possess-tick-die loop per villager.
	//
	// Idempotent: a second Kill() on an already-dead villager
	// early-returns via m_bDispatchedDeath. (Using the life value
	// for the guard would fail because TickLife calls Kill() AFTER
	// decrementing life past 0 -- the guard would prevent the very
	// first death from firing.)
	void Kill()
	{
		if (m_bDispatchedDeath) return;
		m_bDispatchedDeath = true;
		m_fRemainingLife = 0.0f;
		// MVP-1.4.3: distinguish death from voluntary-switch faint.
		// Setting state=Dead here (BEFORE the next OnUpdate observes
		// the un-possession) is what makes the burn-out path skip the
		// Possessed -> Fainted transition. The OnUpdate switch's outer
		// `case Dead` is terminal, so the un-possession on the next
		// frame is a no-op rather than a faint.
		m_eState = DPVillagerState::Dead;
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnVillagerDied{ m_xParentEntity.GetEntityID() });
		// Only clear possession if WE were the possessed villager.
		// (Test-driven kills on unpossessed villagers shouldn't
		// stomp the player's current possession state.)
		if (m_bIsPossessed)
		{
			DP_Player::SetPossessedVillager(INVALID_ENTITY_ID);
		}

		// MVP-1.3.5: scan the active scene for any other villager
		// with life > 0. If none remain, the run is over by the
		// "no vessels" cause -- dispatch the GDD-spec event.
		//
		// Natural place for the scan: it runs exactly once per
		// death (no per-frame polling) and observes the post-this-
		// death state. The just-died villager is in the iteration
		// but its life is 0 so the `> 0` check excludes it.
		int iAlive = 0;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&iAlive](Zenith_EntityID, DPVillager_Behaviour& xV)
			{
				if (xV.GetRemainingLife() > 0.0f) ++iAlive;
			});
		if (iAlive == 0)
		{
			// Payload's m_xVillager carries the last villager to die (this
			// one) so the telemetry visualiser places the RunLost marker at
			// the final death site instead of the world-bounds centre. The
			// caught-by-priest variant uses m_xOther for the priest entity;
			// NoVessels has no second entity so it stays INVALID.
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnRunLost{
					DP_RunLostCause::NoVessels,
					m_xParentEntity.GetEntityID(),
					Zenith_EntityID{} });
		}
	}

private:
	void TickMovement(float /*fDt*/)
	{
		// Velocity-driven movement on a DYNAMIC capsule body. Jolt integrates
		// the position and resolves wall collisions natively — no manual
		// raycasting, no transform writes. The villager body's gravity is
		// disabled and pitch/roll are locked in OnAwake, so a 2D horizontal
		// velocity vector is all we need.
		if (!m_xParentEntity.HasComponent<Zenith_ColliderComponent>()) return;
		Zenith_ColliderComponent& xCollider =
			m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody()) return;

		// Camera-relative axes when a main camera exists; world axes
		// otherwise (gym map without camera entity).
		Zenith_Maths::Vector3 xRight(1.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xForward(0.0f, 0.0f, 1.0f);
		if (Zenith_CameraComponent* pxCam = Zenith_SceneManager::FindMainCameraAcrossScenes())
		{
			pxCam->GetFacingDir(xForward);
			xForward.y = 0.0f;
			if (glm::length(xForward) > 0.001f) xForward = glm::normalize(xForward);
			xRight = glm::normalize(glm::cross(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xForward));
		}

		const Zenith_Maths::Vector2 xInput = DP_Input::ReadMoveVillager();
		const float fInputLen = glm::length(xInput);
		Zenith_Maths::Vector3 xVel(0.0f, 0.0f, 0.0f);
		if (fInputLen > 0.01f)
		{
			const Zenith_Maths::Vector3 xDir =
				glm::normalize(xInput.x * xRight + xInput.y * xForward);
			// MVP-1.7: speed override based on movement modifier state.
			// Sprint wins ties; walk-quiet takes the slow speed.
			float fSpeed = m_fMoveSpeed;
			if (m_bIsSprintingNow)
			{
				fSpeed = DP_Tuning::Get<float>("movement.sprint_speed_mps");
			}
			else if (m_bIsWalkQuietNow)
			{
				fSpeed = DP_Tuning::Get<float>("movement.walk_speed_mps");
			}
			xVel = xDir * fSpeed;
		}
		g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), xVel);
	}

	// MVP-1.7.5: footstep emission. Called once per frame from
	// OnUpdate while the villager is possessed. While moving,
	// accumulates a countdown timer; when it expires, emits a
	// footstep via Zenith_AudioBus::EmitSound (for the test recorder)
	// AND Zenith_PerceptionSystem::EmitSoundStimulus (so the priest
	// can actually hear it). Loudness is multiplied by
	// `movement.walk_footstep_loudness_multiplier` while walk-quiet
	// is active; sprint runs at full loudness (no extra loudness mult
	// in MVP -- the sprint cost is movement.sprint_life_cost, the
	// audibility multiplier is post-MVP).
	void TickFootsteps(float fDt, bool bMoving)
	{
		if (!bMoving)
		{
			// Hold the timer at zero so the FIRST step after starting
			// to move emits immediately, not after a full interval.
			m_fFootstepCountdown = 0.0f;
			return;
		}
		m_fFootstepCountdown -= fDt;
		if (m_fFootstepCountdown > 0.0f) return;

		const float fInterval = DP_Tuning::Get<float>("movement.footstep_interval_s");
		m_fFootstepCountdown = fInterval;

		const float fBaseLoudness = DP_Tuning::Get<float>("movement.footstep_loudness");
		const float fRadius = DP_Tuning::Get<float>("movement.footstep_radius_m");
		float fLoudness = fBaseLoudness;
		if (m_bIsWalkQuietNow)
		{
			fLoudness *= DP_Tuning::Get<float>("movement.walk_footstep_loudness_multiplier");
		}

		Zenith_Maths::Vector3 xPos(0.0f);
		if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		}

		// Tag emissions with a name the AudioBus test recorder can
		// filter on. The "DP.Villager.Footstep" prefix lets future
		// graphics builds attach an actual sample file by name.
		Zenith_AudioBus::EmitSound("DP.Villager.Footstep", xPos, fLoudness, fRadius);

		// Drive priest hearing through the perception system. The
		// Priest_Behaviour bridge consumes the freshest hearing
		// stimulus into BB.InvestigatePos.
		Zenith_PerceptionSystem::EmitSoundStimulus(
			xPos, fLoudness, fRadius, m_xParentEntity.GetEntityID());
	}

	void ZeroHorizontalVelocity()
	{
		// Stop the dynamic body when not possessed so it doesn't coast on
		// residual velocity from the last possession.
		if (!m_xParentEntity.HasComponent<Zenith_ColliderComponent>()) return;
		Zenith_ColliderComponent& xCollider =
			m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody()) return;
		g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(),
			Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	}

	// Swap the villager's per-mesh materials between the original (un-possessed)
	// material and the procedurally-generated red-emissive Possessed_<Base>
	// variant. The base materials are captured the first time we tint, so if a
	// material is set externally before possession the original is preserved.
	void ApplyPossessionMaterial(bool bPossessed)
	{
		if (!m_xParentEntity.HasComponent<Zenith_ModelComponent>()) return;
		Zenith_ModelComponent& xModel = m_xParentEntity.GetComponent<Zenith_ModelComponent>();
		Flux_ModelInstance* pxModelInstance = xModel.GetModelInstance();
		if (!pxModelInstance) return;

		const uint32_t uNumMaterials = pxModelInstance->GetNumMaterials();
		if (uNumMaterials == 0) return;

		if (bPossessed)
		{
			// Capture base materials lazily (first possession only) and swap to
			// the tinted variant.
			if (m_apxBaseMaterials.GetSize() < uNumMaterials)
			{
				m_apxBaseMaterials.Clear();
				for (uint32_t u = 0; u < uNumMaterials; ++u)
				{
					m_apxBaseMaterials.PushBack(pxModelInstance->GetMaterial(u));
				}
			}

			for (uint32_t u = 0; u < uNumMaterials; ++u)
			{
				Zenith_MaterialAsset* pxBase = m_apxBaseMaterials.Get(u);
				Zenith_MaterialAsset* pxTint = DPMaterials::GetOrCreatePossessedTintFor(pxBase);
				if (pxTint) pxModelInstance->SetMaterial(u, pxTint);
			}
		}
		else
		{
			// Restore originals.
			const uint32_t uRestoreCount = (m_apxBaseMaterials.GetSize() < uNumMaterials)
				? m_apxBaseMaterials.GetSize() : uNumMaterials;
			for (uint32_t u = 0; u < uRestoreCount; ++u)
			{
				Zenith_MaterialAsset* pxBase = m_apxBaseMaterials.Get(u);
				if (pxBase) pxModelInstance->SetMaterial(u, pxBase);
			}
		}
	}

	// Build (when held), update (when held tag changes), or destroy (when
	// dropped) the floating "held-item" marker entity that follows this
	// villager. The marker is a small cube with a tag-coloured material — it
	// stays fixed in world space since Zenith doesn't have a parent-child
	// auto-follow yet; instead we update its position every frame inside
	// PositionHeldItemVisual which OnUpdate calls when bIsPossessed.
	void ApplyHeldItemVisual(DP_ItemTag eHeld)
	{
		// Drop case: tag became None → destroy the marker if it exists.
		if (eHeld == DP_ItemTag::None)
		{
			DestroyHeldItemVisual();
			return;
		}

		// First-time creation: spawn a small cube on top of the villager.
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(m_xParentEntity.GetEntityID());
		if (pxScene == nullptr) return;

		if (!m_xHeldItemVisual.IsValid())
		{
			char szName[64];
			std::snprintf(szName, sizeof(szName), "HeldVisual_%u", m_xParentEntity.GetEntityID().m_uIndex);
			Zenith_Entity xVisual(pxScene, std::string(szName));
			if (!xVisual.IsValid()) return;
			m_xHeldItemVisual = xVisual.GetEntityID();

			Zenith_ModelComponent& xModel = xVisual.AddComponent<Zenith_ModelComponent>();
			xModel.LoadModel(std::string(GAME_ASSETS_DIR) + "Meshes/LevelPrototyping_Meshes_SM_Cube" + ZENITH_MODEL_EXT);

			if (xVisual.HasComponent<Zenith_TransformComponent>())
			{
				xVisual.GetComponent<Zenith_TransformComponent>().SetScale(
					Zenith_Maths::Vector3(0.25f, 0.25f, 0.25f));
			}
		}

		// Tint the marker by tag. Reuse the same coloured-variant API the items
		// themselves use so the floating cube matches the picked-up item.
		Zenith_Entity xVisual = pxScene->TryGetEntity(m_xHeldItemVisual);
		if (!xVisual.IsValid()) return;
		if (!xVisual.HasComponent<Zenith_ModelComponent>()) return;
		Flux_ModelInstance* pxInst = xVisual.GetComponent<Zenith_ModelComponent>().GetModelInstance();
		if (!pxInst) return;

		Zenith_Maths::Vector3 xRgb(1.0f, 1.0f, 1.0f);
		const char* szLabel = "Tint";
		switch (eHeld)
		{
			case DP_ItemTag::Iron:        xRgb = Zenith_Maths::Vector3(0.5f, 0.5f, 0.55f); szLabel = "TintIron"; break;
			case DP_ItemTag::Key:         xRgb = Zenith_Maths::Vector3(1.0f, 0.85f, 0.2f); szLabel = "TintKey"; break;
			case DP_ItemTag::SkeletonKey: xRgb = Zenith_Maths::Vector3(0.7f, 0.3f, 0.9f);  szLabel = "TintSkeletonKey"; break;
			// MVP-2.3 forge additions
			case DP_ItemTag::Wood:        xRgb = Zenith_Maths::Vector3(0.55f, 0.35f, 0.15f); szLabel = "TintWood"; break;
			case DP_ItemTag::Spike:       xRgb = Zenith_Maths::Vector3(0.85f, 0.85f, 0.9f);  szLabel = "TintSpike"; break;
			// MVP-2.2 reagent tints (match Reagents.json's tint_rgb values).
			case DP_ItemTag::Caul:        xRgb = Zenith_Maths::Vector3(0.95f, 0.92f, 0.85f); szLabel = "TintCaul"; break;
			case DP_ItemTag::HareTongue:  xRgb = Zenith_Maths::Vector3(0.65f, 0.20f, 0.18f); szLabel = "TintHareTongue"; break;
			case DP_ItemTag::BogWater:    xRgb = Zenith_Maths::Vector3(0.20f, 0.30f, 0.25f); szLabel = "TintBogWater"; break;
			case DP_ItemTag::BurialCoin:  xRgb = Zenith_Maths::Vector3(0.70f, 0.60f, 0.30f); szLabel = "TintBurialCoin"; break;
			case DP_ItemTag::BellSoul:    xRgb = Zenith_Maths::Vector3(0.85f, 0.75f, 0.45f); szLabel = "TintBellSoul"; break;
			default:                      xRgb = Zenith_Maths::Vector3(0.95f, 0.15f, 0.15f); szLabel = "TintObjective"; break;
		}
		const uint32_t uMatCount = pxInst->GetNumMaterials();
		for (uint32_t u = 0; u < uMatCount; ++u)
		{
			Zenith_MaterialAsset* pxBase = pxInst->GetMaterial(u);
			Zenith_MaterialAsset* pxTint = DPMaterials::GetOrCreateColouredVariant(pxBase, xRgb, szLabel);
			if (pxTint) pxInst->SetMaterial(u, pxTint);
		}

		// Place above the villager.
		PositionHeldItemVisual();
	}

	void PositionHeldItemVisual()
	{
		if (!m_xHeldItemVisual.IsValid()) return;
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(m_xHeldItemVisual);
		if (pxScene == nullptr) return;
		Zenith_Entity xVisual = pxScene->TryGetEntity(m_xHeldItemVisual);
		if (!xVisual.IsValid()) return;
		if (!xVisual.HasComponent<Zenith_TransformComponent>()) return;
		if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>()) return;

		Zenith_Maths::Vector3 xPos;
		m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		// Hover roughly head-height above the villager.
		xPos.y += 1.6f;
		xVisual.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
	}

	void DestroyHeldItemVisual()
	{
		if (!m_xHeldItemVisual.IsValid()) return;
		// Snapshot then clear FIRST so we never recurse via this path. If the
		// scene is mid-teardown, Destroy may be a no-op — that's fine, the
		// scene will free the entity itself.
		Zenith_EntityID xHandle = m_xHeldItemVisual;
		m_xHeldItemVisual = INVALID_ENTITY_ID;

		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xHandle);
		if (pxScene == nullptr) return;
		Zenith_Entity xVisual = pxScene->TryGetEntity(xHandle);
		if (!xVisual.IsValid()) return;
		Zenith_SceneManager::Destroy(xVisual);
	}

	void OnDestroy() ZENITH_FINAL override
	{
		// Don't try to destroy the visual entity during scene teardown — the
		// entity may already be in the destruction queue, and calling Destroy
		// on it twice (once here, once in the scene's reset path) can fire
		// asserts on Windows debug builds. Just clear our handle.
		m_xHeldItemVisual = INVALID_ENTITY_ID;
	}

	float m_fMaxLife        = 30.0f;
	float m_fRemainingLife  = 30.0f;
	// MVP-0.2.3: archetype id (resolved at OnAwake via DP_Archetypes). Default
	// "Farmhand" keeps pre-MVP-0.2.3 stats (life=30s, jog=8m/s) for scenes
	// that don't yet override per-villager. Updated through SetArchetype()
	// before OnAwake or ApplyArchetype() at runtime.
	std::string m_strArchetypeId = "Farmhand";
	// 8 m/s — a brisk jog. The previous 4 m/s value made the
	// HumanPlaythrough_Test miss the 3-minute wall-clock budget by a wide
	// margin; doubling it cuts every walk leg in half without changing
	// pathing behaviour. Still well below "teleporting" speeds that would
	// skip collider response.
	float m_fMoveSpeed      = 8.0f;
	bool  m_bIsPossessed    = false;
	// MVP-1.4.1-3: villager lifecycle state. Drives the Idle/Possessed/
	// Fainted/Dead transitions in OnUpdate. The legacy m_bIsPossessed
	// flag (above) is kept in sync for the IsPossessed() API.
	DPVillagerState m_eState = DPVillagerState::Idle;
	// MVP-1.4.2: countdown while Fainted. Set on Possessed->Fainted
	// transition from possession.faint_recovery_s. When it reaches 0
	// the state advances to Idle.
	float m_fFaintRecoveryRemaining = 0.0f;
	// MVP-1.7: sprint-state cache. Set in OnUpdate to
	// (DP_Input::ReadSprintHeld() && moving). TickLife / TickMovement
	// both read this so they agree on whether sprint is active this
	// tick.
	bool  m_bIsSprintingNow = false;
	// MVP-1.7: walk-quiet cache, computed similarly. Sprint wins on
	// Shift+Ctrl ties (the louder, faster mode shouldn't be silenced
	// by a held Ctrl).
	bool  m_bIsWalkQuietNow = false;
	// MVP-1.7: footstep emission countdown. Counts down each frame
	// while moving; when it hits 0, TickFootsteps emits a step + resets.
	float m_fFootstepCountdown = 0.0f;
	// MVP-1.3.5: idempotency flag for Kill(). Flipped true on the
	// first Kill(); subsequent calls early-return. Using a dedicated
	// flag (rather than reading m_fRemainingLife) is necessary because
	// TickLife calls Kill() AFTER decrementing life past 0 -- a
	// life-based guard would suppress the very first dispatch.
	bool  m_bDispatchedDeath = false;
	// Snapshot of the base materials taken on first possession - used to
	// restore the un-tinted look when un-possessed.
	Zenith_Vector<Zenith_MaterialAsset*> m_apxBaseMaterials;

	// Held-item visual state. m_eLastSeenHeldTag drives the
	// re-creation/teardown decision in OnUpdate.
	DP_ItemTag       m_eLastSeenHeldTag = DP_ItemTag::None;
	Zenith_EntityID  m_xHeldItemVisual  = INVALID_ENTITY_ID;
};
