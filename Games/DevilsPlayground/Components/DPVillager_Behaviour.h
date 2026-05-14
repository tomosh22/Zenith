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
#include "Physics/Zenith_Physics.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/Flux_ModelInstance.h"
#include "AssetHandling/Zenith_MaterialAsset.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPInputActions.h"
#include "Source/DPMaterials.h"
#include "Source/DP_Tuning.h"
#include "Source/DP_Archetypes.h"

#include <cstdio>

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
				Zenith_Physics::SetGravityEnabled(xBodyID, false);
				Zenith_Physics::LockRotation(xBodyID, /*X=*/true, /*Y=*/false, /*Z=*/true);
			}
		}
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		// Possession state is observed via DP_Player; refresh per frame so a
		// click-to-possess on another villager flips us back to idle without
		// the controller having to broadcast.
		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
		const bool bWasPossessed = m_bIsPossessed;
		m_bIsPossessed = (xPossessed.m_uIndex == m_xParentEntity.GetEntityID().m_uIndex)
		             && (xPossessed.m_uGeneration == m_xParentEntity.GetEntityID().m_uGeneration);

		// Bump life on freshly-possessed transition.
		if (m_bIsPossessed && !bWasPossessed)
		{
			m_fRemainingLife = m_fMaxLife;
		}

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
			// MVP-1.7: compute "sprinting now" once per frame so TickLife
			// and TickMovement agree. Sprint requires BOTH the input
			// (Shift held) AND movement input (otherwise standing-still
			// with Shift down would burn life for no movement -- failing
			// MVP-1.7.3's "no drain when not moving" test).
			const Zenith_Maths::Vector2 xMove = DP_Input::ReadMoveVillager();
			const float fMoveLen = glm::length(xMove);
			m_bIsSprintingNow = DP_Input::ReadSprintHeld() && (fMoveLen > 0.01f);
			TickLife(fDt);
			TickMovement(fDt);
		}
		else
		{
			m_bIsSprintingNow = false;
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
	// MVP-1.7: test accessor -- returns true if Shift was held AND the
	// villager was actually moving on the most recent OnUpdate. Used by
	// Test_P1Sprint_* to verify the sprint state machine without
	// faking input.
	bool IsSprintingNow() const { return m_bIsSprintingNow; }

	// Test/debug only: shrink the timer so death-by-timeout tests don't need
	// 1800+ simulated frames to fire. Production gameplay sets m_fMaxLife
	// once at authoring and never touches it again.
	void SetRemainingLifeForTest(float fSeconds) { m_fRemainingLife = fSeconds; }

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
			m_fRemainingLife = 0.0f;
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnVillagerDied{ m_xParentEntity.GetEntityID() });
			// Clear possession so the player has to pick a new villager.
			DP_Player::SetPossessedVillager(INVALID_ENTITY_ID);
		}
	}

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
			// MVP-1.7: sprint speed multiplier. The "AND moving"
			// guard lives in OnUpdate (m_bIsSprintingNow); we just
			// read the flag here to swap m_fMoveSpeed for the sprint
			// tuning value when active.
			float fSpeed = m_fMoveSpeed;
			if (m_bIsSprintingNow)
			{
				fSpeed = DP_Tuning::Get<float>("movement.sprint_speed_mps");
			}
			xVel = xDir * fSpeed;
		}
		Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), xVel);
	}

	void ZeroHorizontalVelocity()
	{
		// Stop the dynamic body when not possessed so it doesn't coast on
		// residual velocity from the last possession.
		if (!m_xParentEntity.HasComponent<Zenith_ColliderComponent>()) return;
		Zenith_ColliderComponent& xCollider =
			m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody()) return;
		Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(),
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
	// MVP-1.7: sprint-state cache. Set in OnUpdate to
	// (DP_Input::ReadSprintHeld() && moving). TickLife / TickMovement
	// both read this so they agree on whether sprint is active this
	// tick.
	bool  m_bIsSprintingNow = false;
	// Snapshot of the base materials taken on first possession - used to
	// restore the un-tinted look when un-possessed.
	Zenith_Vector<Zenith_MaterialAsset*> m_apxBaseMaterials;

	// Held-item visual state. m_eLastSeenHeldTag drives the
	// re-creation/teardown decision in OnUpdate.
	DP_ItemTag       m_eLastSeenHeldTag = DP_ItemTag::None;
	Zenith_EntityID  m_xHeldItemVisual  = INVALID_ENTITY_ID;
};
