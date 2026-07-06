#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPVillager_Component - Possessable villager (DevilsPlayground port).
 *
 * 17 villagers are placed around the procgen level (was 14 in early port
 * milestones; extended during M0.5). The player click-to-possesses
 * one at a time. Possessed villager moves under WASD; un-possessed villagers
 * stand still. Possession bumps the villager's remaining-life timer to a
 * fixed value; when it ticks to zero, the villager dies and the player must
 * possess another. Win condition is the pentagram, not survival.
 *
 * SourceBugFixed (RemoveHeldItem): the source RemoveHeldItem null-derefs.
 * The fix lives in DP_Player::RemoveHeldItem.
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Physics/Zenith_Physics.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"
#include "Core/Zenith_AudioBus.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Flux/Flux_ModelInstance.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Prefab/Zenith_Prefab.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPResources.h"
#include "Source/DPInputActions.h"
#include "Source/DPTutorial.h"
#include "Source/DPMaterials.h"
#include "Source/DP_Tuning.h"
#include "Source/DP_Archetypes.h"

#include <cstdio>
#include <cstring>

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

class DPVillager_Component ZENITH_FINAL
{
public:
	// W3 conversion: the villager's DECISIONS (state machine, movement-mode
	// booleans, life drain, footstep cadence) live on this boot-authored
	// graph; the component is the SYSTEMS shim (body config, velocity apply,
	// materials, held visual, sound emission) + the blackboard-backed
	// accessor surface the HUD / DP_Player / tests consume.
	static constexpr const char* kszGraphAsset = "game:Graphs/DP_Villager.bgraph";

	DPVillager_Component() = delete;
	DPVillager_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	// No user-declared destructor / copy / move: every member is trivially
	// movable (PODs, std::string, Zenith_Vector, EntityIDs), no event
	// subscriptions and no statics point at `this`, so the implicit moves
	// keep the component pool-relocation-safe.

	void OnAwake()
	{
		// W3: the decisions graph is SELF-ATTACHED (not bootstrap-attached)
		// so every DPVillager - procgen-spawned AND test-spawned - carries
		// it. Idempotent: re-entry (bootstrap's explicit OnAwake + a later
		// wave) finds the existing slot. Attach happens FIRST so the
		// ApplyArchetype/tuning seeding below lands on the blackboard.
		EnsureGraphAttached();

		// MVP-0.2.3: apply the archetype's life_timer + jog_speed. Archetype
		// id is stored in m_strArchetypeId; default "Farmhand" matches the
		// pre-MVP-0.2.3 behaviour (life=30s, jog=8m/s). Per-villager authoring
		// or test setup can override via ApplyArchetype("Beggar") /
		// ApplyArchetype("Child") to switch stats before OnAwake fires, or
		// after OnAwake to retune at runtime.
		ApplyArchetype(m_strArchetypeId.c_str());

		// B1: cache the hot movement tuning keys once. They are read every
		// frame from TickLife / TickMovement / TickFootsteps while possessed;
		// DP_Tuning::Get is a linear scan + string compare, so per the
		// "hot keys read once at OnAwake" convention they are cached here.
		// Old Bett's Breath (metagame): unlocks cheapen sprint (GDD "30%
		// sprint efficiency" at a full track). Scale is 1.0 on a fresh
		// profile, so the ratified sprint-cost balance is untouched until
		// the player buys Breath nodes at the Liminal.
		m_fSprintLifeCostExtra      = DP_Tuning::Get<float>("movement.sprint_life_cost_extra_per_s")
		                            * DP_MetaSave::GetSprintDrainScale();
		m_fSprintSpeed              = DP_Tuning::Get<float>("movement.sprint_speed_mps");
		m_fWalkSpeed                = DP_Tuning::Get<float>("movement.walk_speed_mps");
		m_fFootstepInterval         = DP_Tuning::Get<float>("movement.footstep_interval_s");
		m_fFootstepLoudness         = DP_Tuning::Get<float>("movement.footstep_loudness");
		m_fFootstepRadius           = DP_Tuning::Get<float>("movement.footstep_radius_m");
		m_fWalkFootstepLoudnessMult = DP_Tuning::Get<float>("movement.walk_footstep_loudness_multiplier");

		// Mirror the graph's decision inputs onto the blackboard. The sprint
		// cost keeps the OnAwake-baked MetaSave scale (NOT re-read live -
		// the ratified balance semantics); footstep numbers are the cached
		// hot keys under the same one-read convention.
		WriteBBFloat("sprintCostExtra", m_fSprintLifeCostExtra);
		WriteBBFloat("footstepInterval", m_fFootstepInterval);
		WriteBBFloat("footstepLoudness", m_fFootstepLoudness);
		WriteBBFloat("footstepRadius", m_fFootstepRadius);
		WriteBBFloat("quietLoudnessMult", m_fWalkFootstepLoudnessMult);

		// Reset transient state — Editor Stop/Play would otherwise leave a
		// stale possession flag from a previous play session.
		m_bIsPossessed = false;
		WriteBBInt("state", (int32_t)DPVillagerState::Idle);
		WriteBBFloat("remainingLife", m_fMaxLife);
		WriteBBFloat("faintRecovery", 0.0f);
		WriteBBFloat("footstepCountdown", 0.0f);
		WriteBBBool("sprinting", false);
		WriteBBBool("walkQuiet", false);

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
		if (Zenith_ColliderComponent* pxCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>())
		{
			if (pxCollider->HasValidBody())
			{
				const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
				g_xEngine.Physics().SetGravityEnabled(xBodyID, false);
				g_xEngine.Physics().LockRotation(xBodyID, /*X=*/true, /*Y=*/false, /*Z=*/true);
			}
		}
	}

	// Component contract: version-only payload (villager state is runtime-
	// driven; archetype is stamped by the bootstrap / test setup).
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() {}
#endif

	void OnUpdate(const float fDt)
	{
		// W3 conversion: the transition table (MVP-1.4.1-3), movement-mode
		// booleans (MVP-1.7, sprint-wins-ties), life drain + Kill decision
		// and footstep cadence (MVP-1.7.5) live on DP_Villager.bgraph. The
		// shim stages this frame's facts, fires "VillagerTick" (dt as the
		// payload - custom-event contexts have dt=0, the graph subtracts the
		// payload var), then runs the systems the decisions selected.
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		Zenith_BehaviourGraph* pxGraph = FindVillagerGraph();
		if (pxGraphs == nullptr || pxGraph == nullptr)
		{
			return;	// attached in OnAwake; nothing to drive pre-awake
		}

		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
		const bool bIsPossessedThisFrame =
			(xPossessed.m_uIndex == m_xParentEntity.GetEntityID().m_uIndex)
			&& (xPossessed.m_uGeneration == m_xParentEntity.GetEntityID().m_uGeneration);
		const bool bWasPossessed =
			(ReadBBInt("state", 0) == (int32_t)DPVillagerState::Possessed);
		const Zenith_Maths::Vector2 xMove = DP_Input::ReadMoveVillager();
		const bool bMoving = (glm::length(xMove) > 0.01f);

		WriteBBBool("possessedNow", bIsPossessedThisFrame);
		WriteBBBool("moving", bMoving);
		WriteBBBool("sprintHeld", DP_Input::ReadSprintHeld());
		WriteBBBool("quietHeld", DP_Input::ReadWalkQuietHeld());

		Zenith_PropertyValue xDtPayload;
		xDtPayload.SetFloat(fDt);
		pxGraphs->FireCustomEvent("VillagerTick", &xDtPayload);

		// Legacy flag sync off the POST-decision state. Kept for the
		// existing public API (IsPossessed()) and Kill()'s conditional
		// possession clear (same one-frame-stale semantics as before).
		m_bIsPossessed =
			(ReadBBInt("state", 0) == (int32_t)DPVillagerState::Possessed);

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

		// Movement systems gate on the graph's mid-tick possession fact
		// ("stateIsPossessed", computed AFTER transitions but BEFORE the
		// life-drain chain) - so on a burn-out death frame the movement
		// still applies once, exactly like the retired inline order
		// (TickLife -> Kill -> TickMovement all inside the same gate).
		if (ReadBBBool("stateIsPossessed", false))
		{
			TickMovement(fDt);
		}
		else
		{
			ZeroHorizontalVelocity();
		}

		// Re-anchor the held visual to the villager's current world position.
		// The marker isn't a proper child entity (no auto-follow yet), so we
		// reposition it every frame. Intentionally NOT gated behind
		// m_bIsPossessed: a villager can still hold an item while unpossessed
		// (e.g. a voluntary switch without a drop), and the IsValid() early-out
		// makes the per-frame cost negligible for the villagers holding nothing.
		if (m_xHeldItemVisual.IsValid())
		{
			PositionHeldItemVisual();
		}
	}

	// W3: the decision state lives on the graph blackboard; the accessor
	// surface reads it back so every consumer (HUD via DP_Player forwarders,
	// priest bridge, tests) compiles unchanged (the W1/W2 shim-accessor
	// pattern).
	float GetRemainingLife() const { return ReadBBFloat("remainingLife", 0.0f); }
	float GetMaxLife() const { return m_fMaxLife; }
	const std::string& GetArchetypeId() const { return m_strArchetypeId; }

	// Re-resolve stats from DP_Archetypes for a new archetype id. Persists
	// the id and re-seeds m_fMaxLife + m_fMoveSpeed; resets m_fRemainingLife
	// only if the villager isn't currently possessed (a mid-possession swap
	// would otherwise interrupt the player's life-timer countdown). MVP-0.2.3
	// authoring path: the spawner calls ApplyArchetype("...") on the freshly-
	// attached component so the entity runs with archetype-correct stats.
	// Falls back to DP_Tuning's possession.life_timer_default_s +
	// movement.jog_speed_mps if the archetype id is missing or DP_Archetypes
	// wasn't initialized.
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
		WriteBBFloat("maxLife", m_fMaxLife);
		if (!m_bIsPossessed)
		{
			WriteBBFloat("remainingLife", m_fMaxLife);
		}
	}

	// Pre-OnAwake authoring setter: stash the id so the next OnAwake call
	// resolves with the new archetype. Does NOT immediately apply -- safe to
	// call from test setup before the Awake wave fires.
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

	// MVP-1.4.1-3 state accessors (blackboard-backed since W3).
	DPVillagerState GetState() const
	{
		return (DPVillagerState)ReadBBInt("state", (int32_t)DPVillagerState::Idle);
	}

	// Whether the player's voluntary-switch path is allowed to
	// possess this villager. Refused for Fainted (still recovering)
	// and Dead (permanent). Idle and Possessed both pass -- re-
	// clicking the same villager is idempotent in
	// TryVoluntaryPossessSwitch.
	bool IsPossessable() const
	{
		const DPVillagerState eState = GetState();
		return eState == DPVillagerState::Idle
			|| eState == DPVillagerState::Possessed;
	}

	// Diagnostic only -- the recovery countdown while Fainted, 0
	// otherwise. Tests inspect this to assert the timer arms on
	// voluntary switch.
	float GetFaintRecoveryRemaining() const { return ReadBBFloat("faintRecovery", 0.0f); }
	// MVP-1.7: test accessor -- returns true if Shift was held AND the
	// villager was actually moving on the most recent OnUpdate. Used by
	// Test_P1Sprint_* to verify the sprint state machine without
	// faking input.
	bool IsSprintingNow() const { return ReadBBBool("sprinting", false); }
	// MVP-1.7: walk-quiet cache; same shape as IsSprintingNow.
	bool IsWalkQuietNow() const { return ReadBBBool("walkQuiet", false); }

	// Test/debug only: shrink the timer so death-by-timeout tests don't need
	// 1800+ simulated frames to fire. Production gameplay sets m_fMaxLife
	// once at authoring and never touches it again.
	void SetRemainingLifeForTest(float fSeconds) { WriteBBFloat("remainingLife", fSeconds); }

	// MVP-1.4 test accessor: shortcut the 10s faint-recovery countdown
	// so Test_P1Faint_RecoversToIdle can verify the Fainted->Idle
	// transition without ticking 600 frames. The graph's Fainted chain
	// reads faintRecovery and transitions to Idle on the frame it
	// reaches 0; the test sets it close to 0 and confirms the
	// next-frame tick makes the transition.
	void SetFaintRecoveryForTest(float fSeconds) { WriteBBFloat("faintRecovery", fSeconds); }

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
		WriteBBFloat("remainingLife", 0.0f);
		// MVP-1.4.3: distinguish death from voluntary-switch faint.
		// Setting state=Dead here (BEFORE the next tick observes the
		// un-possession) is what makes the burn-out path skip the
		// Possessed -> Fainted transition. The graph's SwitchOnInt
		// dispatches on the NEW state next tick, and its Dead pin is
		// terminal, so the un-possession is a no-op rather than a faint.
		WriteBBInt("state", (int32_t)DPVillagerState::Dead);
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
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&iAlive](Zenith_EntityID, DPVillager_Component& xV)
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
		Zenith_ColliderComponent* pxCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCollider == nullptr) return;
		if (!pxCollider->HasValidBody()) return;

		// Camera-relative axes when a main camera exists; world axes
		// otherwise (gym map without camera entity).
		Zenith_Maths::Vector3 xRight(1.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xForward(0.0f, 0.0f, 1.0f);
		if (Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes())
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
			// Sprint wins ties; walk-quiet takes the slow speed. The
			// booleans are the graph's decisions (chain T2), read back
			// off the blackboard; the speed table is shim data.
			float fSpeed = m_fMoveSpeed;
			if (ReadBBBool("sprinting", false))
			{
				fSpeed = m_fSprintSpeed;
			}
			else if (ReadBBBool("walkQuiet", false))
			{
				fSpeed = m_fWalkSpeed;
			}
			xVel = xDir * fSpeed;
		}
		g_xEngine.Physics().SetLinearVelocity(pxCollider->GetBodyID(), xVel);
	}

	// ---- W3 graph plumbing (mirrors DPDoor_Component's blackboard shim) ----

	// Idempotent self-attach of the decisions graph. Called from OnAwake
	// (which the bootstrap invokes explicitly after AddComponent, and the
	// lifecycle wave may invoke again - both orders are safe).
	void EnsureGraphAttached()
	{
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphs == nullptr)
		{
			pxGraphs = &m_xParentEntity.AddComponent<Zenith_GraphComponent>();
		}
		for (u_int u = 0; u < pxGraphs->GetGraphCount(); ++u)
		{
			if (std::strcmp(pxGraphs->GetGraphAssetPathAt(u), kszGraphAsset) == 0)
			{
				return;
			}
		}
		pxGraphs->AddGraphByAssetPath(kszGraphAsset);
	}

	Zenith_BehaviourGraph* FindVillagerGraph() const
	{
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphs == nullptr) return nullptr;
		for (u_int u = 0; u < pxGraphs->GetGraphCount(); ++u)
		{
			if (std::strcmp(pxGraphs->GetGraphAssetPathAt(u), kszGraphAsset) == 0)
			{
				return pxGraphs->GetGraphAt(u);
			}
		}
		return nullptr;
	}

	// Silent no-ops / defaults when the graph isn't attached yet (pre-awake
	// reads, unresolved asset) - the DPDoor SetRequiredKey convention.
	float ReadBBFloat(const char* szVar, float fDefault) const
	{
		Zenith_BehaviourGraph* pxGraph = FindVillagerGraph();
		return pxGraph ? pxGraph->GetBlackboard().GetFloat(szVar, fDefault) : fDefault;
	}
	int32_t ReadBBInt(const char* szVar, int32_t iDefault) const
	{
		Zenith_BehaviourGraph* pxGraph = FindVillagerGraph();
		return pxGraph ? pxGraph->GetBlackboard().GetInt32(szVar, iDefault) : iDefault;
	}
	bool ReadBBBool(const char* szVar, bool bDefault) const
	{
		Zenith_BehaviourGraph* pxGraph = FindVillagerGraph();
		return pxGraph ? pxGraph->GetBlackboard().GetBool(szVar, bDefault) : bDefault;
	}
	void WriteBBFloat(const char* szVar, float fValue)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindVillagerGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetFloat(fValue);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}
	void WriteBBInt(const char* szVar, int32_t iValue)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindVillagerGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetInt32(iValue);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}
	void WriteBBBool(const char* szVar, bool bValue)
	{
		if (Zenith_BehaviourGraph* pxGraph = FindVillagerGraph())
		{
			Zenith_PropertyValue xValue;
			xValue.SetBool(bValue);
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}

private:
	void ZeroHorizontalVelocity()
	{
		// Stop the dynamic body when not possessed so it doesn't coast on
		// residual velocity from the last possession.
		Zenith_ColliderComponent* pxCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCollider == nullptr) return;
		if (!pxCollider->HasValidBody()) return;
		g_xEngine.Physics().SetLinearVelocity(pxCollider->GetBodyID(),
			Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	}

	// Swap the villager's per-mesh materials between the original (un-possessed)
	// material and the procedurally-generated red-emissive Possessed_<Base>
	// variant. The base materials are captured the first time we tint, so if a
	// material is set externally before possession the original is preserved.
	void ApplyPossessionMaterial(bool bPossessed)
	{
		Zenith_ModelComponent* pxModel = m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr) return;
		Flux_ModelInstance* pxModelInstance = pxModel->GetModelInstance();
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
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(m_xParentEntity.GetEntityID());
		if (pxScene == nullptr) return;

		if (!m_xHeldItemVisual.IsValid())
		{
			Zenith_Prefab* pxVisualPrefab = DevilsPlayground::Resources().m_xHeldVisualPrefab.GetDirect();
			if (pxVisualPrefab == nullptr) return;

			char szName[64];
			std::snprintf(szName, sizeof(szName), "HeldVisual_%u", m_xParentEntity.GetEntityID().m_uIndex);
			// Model (cube) is baked into the prefab; instantiate at scale 0.25.
			// Position is updated per-frame below to follow the villager.
			Zenith_Entity xVisual = pxVisualPrefab->Instantiate(pxScene, std::string(szName),
				Zenith_Maths::Vector3(0.0f),
				Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
				Zenith_Maths::Vector3(0.25f, 0.25f, 0.25f));
			if (!xVisual.IsValid()) return;
			m_xHeldItemVisual = xVisual.GetEntityID();
		}

		// Tint the marker by tag. Reuse the same coloured-variant API the items
		// themselves use so the floating cube matches the picked-up item.
		Zenith_Entity xVisual = pxScene->TryGetEntity(m_xHeldItemVisual);
		if (!xVisual.IsValid()) return;
		Zenith_ModelComponent* pxVisualModel = xVisual.TryGetComponent<Zenith_ModelComponent>();
		if (pxVisualModel == nullptr) return;
		Flux_ModelInstance* pxInst = pxVisualModel->GetModelInstance();
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
		Zenith_Entity xVisual = g_xEngine.Scenes().ResolveEntity(m_xHeldItemVisual);
		if (!xVisual.IsValid()) return;
		Zenith_TransformComponent* pxVisualTransform = xVisual.TryGetComponent<Zenith_TransformComponent>();
		if (pxVisualTransform == nullptr) return;
		Zenith_TransformComponent* pxParentTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxParentTransform == nullptr) return;

		Zenith_Maths::Vector3 xPos;
		pxParentTransform->GetPosition(xPos);
		// Hover roughly head-height above the villager.
		xPos.y += 1.6f;
		pxVisualTransform->SetPosition(xPos);
	}

	void DestroyHeldItemVisual()
	{
		if (!m_xHeldItemVisual.IsValid()) return;
		// Snapshot then clear FIRST so we never recurse via this path. If the
		// scene is mid-teardown, Destroy may be a no-op — that's fine, the
		// scene will free the entity itself.
		Zenith_EntityID xHandle = m_xHeldItemVisual;
		m_xHeldItemVisual = INVALID_ENTITY_ID;

		Zenith_Entity xVisual = g_xEngine.Scenes().ResolveEntity(xHandle);
		if (!xVisual.IsValid()) return;
		xVisual.Destroy();
	}

public:
	void OnDestroy()
	{
		// Don't try to destroy the visual entity during scene teardown — the
		// entity may already be in the destruction queue, and calling Destroy
		// on it twice (once here, once in the scene's reset path) can fire
		// asserts on Windows debug builds. Just clear our handle.
		m_xHeldItemVisual = INVALID_ENTITY_ID;
	}

private:
	Zenith_Entity m_xParentEntity;

	float m_fMaxLife        = 60.0f;
	// MVP-0.2.3: archetype id (resolved at OnAwake via DP_Archetypes). Default
	// "Farmhand" keeps pre-MVP-0.2.3 stats (life=30s, jog=8m/s) for scenes
	// that don't yet override per-villager. Updated through SetArchetype()
	// before OnAwake or ApplyArchetype() at runtime.
	std::string m_strArchetypeId = "Farmhand";

	// B1: cached hot movement tuning, populated once in OnAwake (consumed
	// every frame in TickLife/TickMovement/TickFootsteps while possessed).
	float m_fSprintLifeCostExtra      = 0.0f;
	float m_fSprintSpeed              = 0.0f;
	float m_fWalkSpeed                = 0.0f;
	float m_fFootstepInterval         = 0.0f;
	float m_fFootstepLoudness         = 0.0f;
	float m_fFootstepRadius           = 0.0f;
	float m_fWalkFootstepLoudnessMult = 0.0f;
	// 8 m/s — a brisk jog. The previous 4 m/s value made the
	// HumanPlaythrough_Test miss the 3-minute wall-clock budget by a wide
	// margin; doubling it cuts every walk leg in half without changing
	// pathing behaviour. Still well below "teleporting" speeds that would
	// skip collider response.
	float m_fMoveSpeed      = 8.0f;
	// Legacy mirror of (blackboard state == Possessed), synced once per
	// OnUpdate AFTER the decision graph runs. Kept for the IsPossessed()
	// API, Kill()'s conditional possession clear and ApplyArchetype's
	// mid-possession guard - same one-frame-stale semantics as the
	// pre-W3 inline sync. The lifecycle state itself, the faint
	// countdown, the sprint/walk-quiet booleans and the footstep
	// countdown all live on DP_Villager.bgraph's blackboard.
	bool  m_bIsPossessed    = false;
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
