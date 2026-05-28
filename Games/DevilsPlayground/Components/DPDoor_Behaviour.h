#pragma once
/**
 * DPDoor_Behaviour - two-way openable door, optionally key-locked.
 *
 * State machine (m_eAnim):
 *   Closed -> Opening: F-press while in range; consumes a Key iff the door
 *                      currently requires one (locked). Unlocking is STICKY --
 *                      m_eRequiredKey is set to None on the consume so future
 *                      F-presses skip the key check.
 *   Opening -> Open:   m_fOpenT reaches 1.0 (m_fOpenDuration seconds later).
 *   Open -> Closing:   F-press while in range. No key needed even if the door
 *                      was originally locked.
 *   Closing -> Closed: m_fOpenT reaches 0.0.
 *
 * Navmesh coupling: BLOCKED iff (Closed || Closing). Closing immediately re-
 * blocks so the priest can't race through a closing door. Opening is
 * unblocked from frame 1 so AI can begin pathing the moment the player
 * commits to opening (the gap is partly open from frame 1; full navmesh
 * gating waits would feel laggy).
 *
 * Logical centre (m_xLogicalCentre): the geometric door centre. The entity
 * transform position is corner-offset by ~1 m due to SM_Cube anchoring (see
 * the bootstrap's Door case for the offset math). NavMesh portal stitching,
 * the in-range proximity test, and audio emission all use the LOGICAL
 * centre so they anchor on the actual doorway, not the offset transform.
 * The visible mesh, the physics collider, and the open-rotation animation
 * continue to use the transform (so the mesh+collider stay in sync). When
 * the logical centre isn't set (runtime-constructed test doors don't call
 * SetLogicalCentre), it falls back to the transform position, preserving
 * pre-2026-05-25 behaviour.
 */

#include "Components/DPInteractable_Behaviour.h"
#include "Components/DPPentagram_Behaviour.h"
#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "Maths/Zenith_Maths.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "Source/PublicInterfaces.h"
#include "Source/DP_AI.h"
#include "Source/DP_Tuning.h"
#include "Source/DPMaterials.h"
#include "Source/DevilsPlayground_Tags.h"

class DPDoor_Behaviour ZENITH_FINAL : public DPInteractable_Behaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPDoor_Behaviour)

	// Bot-grid + analyser need to know the door footprint without
	// reaching into the bootstrap's spawn code. These match the
	// dimensions the bootstrap scales SM_Cube to (Door case): 0.3 m
	// thick along the wall normal, 2 m wide along the wall.
	static constexpr float kDoorHalfThick = 0.15f;
	static constexpr float kDoorHalfWide  = 1.0f;

	enum class DoorAnim : uint8_t
	{
		Closed  = 0,
		Opening = 1,
		Open    = 2,
		Closing = 3,
	};

	DPDoor_Behaviour() = delete;
	DPDoor_Behaviour(Zenith_Entity& xParentEntity)
		: DPInteractable_Behaviour(xParentEntity)
	{}

	void OnAwake() ZENITH_FINAL override
	{
		DPInteractable_Behaviour::OnAwake();
		// MVP-0.1.4: tuning reads on top of the base-class proximity radius.
		m_fOpenYaw      = DP_Tuning::Get<float>("interactables.door_open_yaw_deg");
		m_fOpenDuration = DP_Tuning::Get<float>("interactables.door_open_duration_s");
		// 2026-05-25: wire the existing-but-unused noise tuning keys for
		// the door audio cue on open + close. Priest perception treats
		// this as a HEARING stimulus -- opening / closing a door near
		// the priest is a "tell" that draws investigation.
		m_fDoorRadius   = DP_Tuning::Get<float>("interactables.door_audible_at_m");
		m_fDoorLoudness = DP_Tuning::Get<float>("interactables.door_audible_loudness");
		m_eAnim  = DoorAnim::Closed;
		m_fOpenT = 0.0f;

		// Doors are runtime-blockable obstacles. The wall colliders surrounding
		// each room have authored doorway GAPS that are exactly door-width;
		// the door entity itself sits IN that gap with its own collider. If
		// the door's collider participated in the navmesh, the gap would be
		// sealed and AI couldn't path between rooms even when the door is
		// open. Mark the collider as nav-mesh-excluded so the generator emits
		// walkable polygons through the doorway; SyncNavMeshBlock then toggles
		// those polygons' BLOCKED flag at runtime based on open/closed state.
		// See Zenith_ColliderComponent::SetIncludeInNavMesh for the contract.
		if (m_xParentEntity.HasComponent<Zenith_ColliderComponent>())
		{
			m_xParentEntity.GetComponent<Zenith_ColliderComponent>().SetIncludeInNavMesh(false);
		}
	}

	void OnStart() ZENITH_FINAL override
	{
		// Defer navmesh blocking to OnStart: AddStep_SetTransformPosition is
		// enqueued after CreateEntity but the editor automation runs all
		// steps before OnStart fires, so the position is final here. In
		// OnAwake the transform is still at (0,0,0), which would block the
		// origin cell instead of the door's actual footprint.
		StitchNavMeshPortal();
		SyncNavMeshBlock();

		// 2026-05-25: cache the original materials BEFORE the first tint,
		// so ApplyLockTint can re-tint to a different colour on unlock
		// without building a chain of variants (each variant re-read
		// from the model would otherwise become a new "base" for the
		// next tint). Walls and doors share SM_Cube, so without a colour
		// override any visible interruption in a wall outline could
		// equally be either; the tint has to wait until OnStart because
		// the model's material list isn't ready at AddComponent time.
		if (m_xParentEntity.HasComponent<Zenith_ModelComponent>())
		{
			Flux_ModelInstance* pxInstance =
				m_xParentEntity.GetComponent<Zenith_ModelComponent>().GetModelInstance();
			if (pxInstance != nullptr)
			{
				const uint32_t uMats = pxInstance->GetNumMaterials();
				for (uint32_t u = 0; u < uMats; ++u)
				{
					m_axOriginalMaterials.PushBack(pxInstance->GetMaterial(u));
				}
			}
		}
		ApplyLockTint();

		// 2026-05-22: capture the door's CLOSED yaw from its current
		// transform rotation, so the open-rotation interpolation in
		// ApplyRotation() starts from the procgen-set angle rather than
		// from hardcoded 0. Without this, procgen doors (which need a
		// per-wall yaw so their collider lies along the wall) would
		// snap to yaw=0 on the first OnUpdate, mis-orienting the
		// collider + uncovering the corridor gap before any F-press.
		// Runtime-constructed test doors (which spawn at yaw=0) still
		// see m_fClosedYaw=0 -- the read is just a no-op for them.
		if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			Zenith_Maths::Quat xQuat;
			m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetRotation(xQuat);
			// Extract yaw from quaternion: yaw = atan2(2(w*y + x*z), 1 - 2(y*y + x*x)).
			// Standard Tait-Bryan extraction for Y-up rotation; matches
			// glm::yaw conventions and the SpawnWalls fYawRadians usage.
			const float fSiny_cosp = 2.0f * (xQuat.w * xQuat.y + xQuat.x * xQuat.z);
			const float fCosy_cosp = 1.0f - 2.0f * (xQuat.y * xQuat.y + xQuat.x * xQuat.x);
			m_fClosedYaw = glm::degrees(std::atan2(fSiny_cosp, fCosy_cosp));
		}
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		// 2026-05-25: rising-edge latch on F-press. DP_Input::ReadInteractPressed
		// is currently rising-edge (WasKeyPressedThisFrame), so this latch
		// is defensive belt-and-braces -- a future change to held-state
		// semantic mustn't immediately re-close a door the player just
		// opened (Closed -> Opening -> Open -> Closing in three frames).
		// Two bools + ~4 cycles per OnUpdate; cheap insurance.
		m_bWasInteractingLastFrame = m_bIsInteractingThisFrame;
		m_bIsInteractingThisFrame  = false;
		DPInteractable_Behaviour::OnUpdate(fDt);

		// Advance the animation toward the directional target. Open and
		// Closed are steady states; Opening / Closing are transient.
		if (m_eAnim == DoorAnim::Opening)
		{
			m_fOpenT = glm::min(1.0f, m_fOpenT + fDt / m_fOpenDuration);
			ApplyRotation();
			if (m_fOpenT >= 1.0f)
			{
				m_eAnim = DoorAnim::Open;
				// Steady-state navmesh sync (the navmesh was already
				// unblocked at the Closed -> Opening transition; this is
				// idempotent but keeps the contract explicit).
				SyncNavMeshBlock();
			}
		}
		else if (m_eAnim == DoorAnim::Closing)
		{
			m_fOpenT = glm::max(0.0f, m_fOpenT - fDt / m_fOpenDuration);
			ApplyRotation();
			if (m_fOpenT <= 0.0f)
			{
				m_eAnim = DoorAnim::Closed;
				SyncNavMeshBlock();
				// Restore physical blocking now that the door has fully
				// closed -- the player capsule should bump into it again.
				ApplyColliderSolidity();
			}
		}
	}

	// 2026-05-25 v4: door collider is a SENSOR while !Closed, SOLID
	// while Closed. The 90 deg open-rotation swings the door's
	// 2 m-wide arm into the adjacent room; without this toggle the
	// player capsule (and priest) get shoved by the swinging arm and
	// wedge in the corridor (telemetry-confirmed: villager stuck for
	// 109 s at one door on seed 5).
	//
	// Earlier attempt (matrix v6 -> v8): set sensor + ALSO no-op'd
	// SyncNavMeshBlock so the priest's planner ignored door state.
	// Result: priest planned through closed-and-locked doors, found
	// every hand-off zone, dropped matrix to 0% wins. Lesson: sensor
	// alone is fine -- it's the missing navmesh gate that broke
	// things. With SyncNavMeshBlock active (restored in v9), the
	// priest's PLAN doesn't cross a closed door, so the priest never
	// attempts to walk through a sensor-but-still-meshed-shut door.
	// Once the priest's BT calls TryInteract via OpenNearbyDoorsFor,
	// the door transitions to Opening -> Open, the sensor toggle
	// fires here, and the priest's body passes through unblocked
	// (matching the player's expected behaviour).
	void ApplyColliderSolidity()
	{
		if (!m_xParentEntity.HasComponent<Zenith_ColliderComponent>()) return;
		const bool bSolid = (m_eAnim == DoorAnim::Closed);
		m_xParentEntity.GetComponent<Zenith_ColliderComponent>().SetIsSensor(!bSolid);
	}

public:
	// Back-compat with existing test code that just asks "is the door
	// open?" -- treat the Opening transition as already-open so tests
	// asserting "F-pressed door is now open" don't have to wait the
	// full animation duration.
	bool IsOpen() const
	{
		return m_eAnim == DoorAnim::Open || m_eAnim == DoorAnim::Opening;
	}
	DP_ItemTag GetRequiredKey() const { return m_eRequiredKey; }
	DoorAnim   GetAnim()        const { return m_eAnim; }

	// 2026-05-25: closed iff Closed || Closing. Single source of truth
	// shared by the bot's path grid (which rasterises this footprint
	// for A* expansion) and SyncNavMeshBlock (which toggles the navmesh
	// BLOCKED flag for the priest).
	bool BlocksPath() const
	{
		return m_eAnim == DoorAnim::Closed || m_eAnim == DoorAnim::Closing;
	}

	// Setters called by DPProcLevelBootstrap between AddScript and the
	// first OnStart frame. Authored / runtime-constructed test doors
	// don't call these -- m_xLogicalCentre stays unset and
	// GetInteractionCentre falls back to the transform position.
	void SetRequiredKey(DP_ItemTag e) { m_eRequiredKey = e; }
	void SetLogicalCentre(const Zenith_Maths::Vector3& v)
	{
		m_xLogicalCentre    = v;
		m_bHasLogicalCentre = true;
	}

	// The actual interaction anchor (geometric door centre). Returns
	// the logical centre if set; otherwise the entity transform's
	// position (so non-procgen doors that don't call SetLogicalCentre
	// continue to work). Used by tests, AI, the bot's grid, audio,
	// and the in-range proximity check.
	Zenith_Maths::Vector3 GetInteractionCentre() const
	{
		if (m_bHasLogicalCentre) return m_xLogicalCentre;
		Zenith_Maths::Vector3 xPos(0.0f);
		if (m_xParentEntity.HasComponent<Zenith_TransformComponent>())
		{
			m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		}
		return xPos;
	}

	// Bot grid needs the door's closed yaw to rasterise the OBB footprint.
	float GetClosedYawDegrees() const { return m_fClosedYaw; }

protected:
	// 2026-05-25: override the base's proximity check to use the logical
	// centre (the actual doorway position) rather than the entity
	// transform (which is corner-offset by ~1 m). Without this, the
	// in-range check passes at a wall-interior point and the door
	// becomes "interactable" from inside the adjacent wall.
	bool IsVillagerInRange(Zenith_EntityID xVillager) const override
	{
		if (!xVillager.IsValid()) return false;
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return false;
		Zenith_Entity xV = pxScene->TryGetEntity(xVillager);
		if (!xV.IsValid()) return false;
		if (!xV.HasComponent<Zenith_TransformComponent>()) return false;

		Zenith_Maths::Vector3 xVPos;
		xV.GetComponent<Zenith_TransformComponent>().GetPosition(xVPos);
		const Zenith_Maths::Vector3 xMyPos = GetInteractionCentre();

		const float fDx = xVPos.x - xMyPos.x;
		const float fDz = xVPos.z - xMyPos.z;
		const float fRadius = GetInteractRadius();
		return fDx * fDx + fDz * fDz <= fRadius * fRadius;
	}

	void HandleInteract(Zenith_EntityID xVillager) override
	{
		// Release latch: see OnUpdate. Reject HOLD frames; accept fresh
		// rising-edge presses only. (No-op if ReadInteractPressed stays
		// rising-edge as it is today; insurance if it ever changes.)
		m_bIsInteractingThisFrame = true;
		if (m_bWasInteractingLastFrame) return;
		HandleInteractInternal(xVillager);
	}

public:
	// 2026-05-25: external-actor interface for priests + scripted events.
	// Bypasses DPInteractable's range/visibility gating AND the F-press
	// release latch -- the caller is responsible for being "near enough"
	// to make narrative sense and for not spamming per-frame (the
	// `BlocksPath() == false` after a successful open naturally
	// debounces re-attempts). Honors lock state: if the door requires a
	// key, the caller must pass an entity that holds it (priest doesn't,
	// so locked doors still reject the priest; this matches the
	// player-parity design).
	void TryInteract(Zenith_EntityID xActor) { HandleInteractInternal(xActor); }

private:
	void HandleInteractInternal(Zenith_EntityID xVillager)
	{
		switch (m_eAnim)
		{
		case DoorAnim::Closed:
		{
			// Locked door: try to consume the matching key. Unlocked door
			// (m_eRequiredKey == None): skip the consume, otherwise
			// TryConsumeKeyForUnlock returns false because the held tag
			// is None (DP_Items.cpp). Either way, on success we
			// transition Closed -> Opening.
			if (m_eRequiredKey != DP_ItemTag::None)
			{
				if (!DP_Items::TryConsumeKeyForUnlock(xVillager, m_eRequiredKey))
				{
					// 2026-05-21: telegraph the rejection so the player can
					// see the door knows it's locked. The particles system
					// fires a red rejection puff; the HUD raises a
					// "Locked -- needs <key>" line for ~2 s.
					Zenith_EventDispatcher::Get().Dispatch(
						DP_OnDoorLockRejected{ xVillager,
						                       m_xParentEntity.GetEntityID(),
						                       m_eRequiredKey });
					return;
				}
				// Unlocking is STICKY: clear m_eRequiredKey so future
				// F-presses (after a close-and-reopen) just open without
				// needing a fresh key. Re-tint immediately so the door
				// visually flips red -> green at the moment of unlock.
				m_eRequiredKey = DP_ItemTag::None;
				ApplyLockTint();
			}
			m_eAnim = DoorAnim::Opening;
			SyncNavMeshBlock();
			// Switch to sensor so the rotating collider doesn't shove
			// the player out of the doorway as it swings.
			ApplyColliderSolidity();
			// Audio cue: the priest hears doors opening within
			// `door_audible_at_m` (radius) at `door_audible_loudness`.
			DP_AI::EmitNoise(GetInteractionCentre(), m_fDoorLoudness,
			                 m_fDoorRadius, m_xParentEntity.GetEntityID());
			// Phase-5-audit (2026-05-16): announce the door-open milestone so
			// the telemetry analyzer + visualiser can plot it as a discrete
			// event rather than burying it in generic DP_OnInteract noise.
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnDoorOpened{ xVillager, m_xParentEntity.GetEntityID() });
			break;
		}
		case DoorAnim::Open:
		{
			// 2026-05-26 v2: don't close if the F-press is ALSO
			// hitting a Pentagram in range. The player's F-press
			// intent at the pentagram is to DELIVER, not to close
			// the adjacent pent-side door. DPInteractable polls
			// every in-range interactable on F-press, so a single
			// tap at the pentagram fires DP_OnInteract on both the
			// pentagram AND the door (pent-side door sits ~3 m from
			// pent; the bot's 1.95 m stop-distance puts villager
			// ~1.5 m from the door -- inside its 2 m radius).
			//
			// Telemetry-confirmed on seed 1: "ObjectivePlaced" +
			// "DoorClosed" fired in the same frame, every subsequent
			// villager had to re-open the door before re-delivering,
			// burning a villager-life per delivery and capping all
			// personalities at 1 placement per cell.
			//
			// First attempt (v32): "don't close while carrying
			// Objective". Failed because the pentagram's HandleInteract
			// runs BEFORE the door's (script execution order =
			// entity creation order; pent spawned first), consuming
			// the objective before the door's check runs. By the
			// time the door checks the held tag, it's already None.
			//
			// Order-independent fix (this branch): check if a
			// Pentagram is in F-range of the villager. If so,
			// defer -- the pentagram's handler will fire instead.
			// Pentagram has a single fixed entity per scene so the
			// proximity check is cheap.
			//
			// The priest follows this rule too but never triggers
			// it (priest can't possess so DP_Player::GetPossessed-
			// Villager won't match; the priest uses TryInteract
			// directly which still falls through to this case).
			// To make the rule symmetric, also check whether the
			// xVillager is the possessed villager AND a Pentagram
			// is in range.
			if (IsPentagramInRange(xVillager))
			{
				break;
			}
			// F-press an open door swings it closed. No key needed -- the
			// closing direction is unconditional. Lock state already
			// cleared on the original unlock so re-opening also doesn't
			// need a key.
			m_eAnim = DoorAnim::Closing;
			// Block the navmesh IMMEDIATELY (the player has committed to
			// closing; AI loses access from this frame to prevent the
			// priest racing through a closing door).
			SyncNavMeshBlock();
			// Stay sensor during Closing -- the player who just
			// F-pressed shouldn't immediately get shoved by the
			// closing-arm sweep. ApplyColliderSolidity in OnUpdate
			// will restore solidity when Closed is reached.
			ApplyColliderSolidity();
			DP_AI::EmitNoise(GetInteractionCentre(), m_fDoorLoudness,
			                 m_fDoorRadius, m_xParentEntity.GetEntityID());
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnDoorClosed{ xVillager, m_xParentEntity.GetEntityID() });
			break;
		}
		case DoorAnim::Opening:
		case DoorAnim::Closing:
		{
			// Animation in progress; ignore F-presses until it settles.
			break;
		}
		}
	}

private:
	void StitchNavMeshPortal()
	{
		// Bridge the two rooms separated by this door at the navmesh level.
		// The wall colliders on either side of the door fully separate the
		// rooms in the heightfield, so even with the door collider excluded
		// (SetIncludeInNavMesh(false) in OnAwake) the generator emits
		// disconnected polygon islands -- A* between them returns FAILED.
		// StitchPortalAt adds a graph-only neighbour link between the two
		// nearest walkable polygons on each side of the door's facing
		// direction. Pathfinding then routes through the door's footprint
		// without requiring a runtime navmesh rebuild.
		//
		// The "wall-perpendicular axis" we probe along is the door's
		// FORWARD direction: when the door opens, you walk perpendicular
		// to its yaw. We rotate the +X unit vector by the door's yaw to
		// get the forward axis in world space.
		if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>()) return;
		const Zenith_NavMesh* pxNavMesh = DP_AI::GetOrBuildLevelNavMesh();
		if (pxNavMesh == nullptr) return;

		// Same gate as SyncNavMeshBlock: only stitch on the real generated
		// navmesh, not the legacy 1-poly synthetic. The synthetic mesh
		// doesn't NEED stitching (everything's one big walkable region)
		// and stitching it would add a self-loop neighbour link.
		constexpr uint32_t kMinPolysForStitching = 16;
		if (pxNavMesh->GetPolygonCount() < kMinPolysForStitching) return;

		// 2026-05-25: probe at the LOGICAL centre, not the entity
		// transform. The transform is corner-offset by ~1 m via SM_Cube
		// anchoring (see bootstrap's Door case); with wall-aligned
		// procgen doors at DoorPoints, that offset can land the probe
		// origin INSIDE a wall, where no walkable polygon exists.
		const Zenith_Maths::Vector3 xPos = GetInteractionCentre();
		Zenith_Maths::Quat xRot;
		m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetRotation(xRot);

		// Extract yaw from quaternion. Standard Y-up convention:
		// yaw = atan2(2*(w*y + x*z), 1 - 2*(y*y + z*z)).
		const float fYaw = std::atan2(2.0f * (xRot.w * xRot.y + xRot.x * xRot.z),
			1.0f - 2.0f * (xRot.y * xRot.y + xRot.z * xRot.z));
		const float fCos = std::cos(fYaw);
		const float fSin = std::sin(fYaw);
		const Zenith_Maths::Vector3 xForward(fCos, 0.0f, -fSin);

		// StitchPortalAt mutates polygon adjacency (graph topology) on
		// what is otherwise a const navmesh handle. The navmesh's
		// m_axPolygons vector itself isn't reallocated; only the
		// neighbour-slot data within polygons changes. The cast mirrors
		// SetPolygonBlocked / SetBlockedAtPoint's existing convention.
		Zenith_NavMesh& xMutable = const_cast<Zenith_NavMesh&>(*pxNavMesh);

		// Probe candidates: along the door's forward axis (most likely
		// the wall normal), and also along its perpendicular axis in
		// case yaw extraction was wrong. Use a few probe distances to
		// reach past wall thicknesses while staying inside the adjacent
		// rooms. First successful stitch wins.
		const Zenith_Maths::Vector3 xPerp(-fSin, 0.0f, -fCos);
		const Zenith_Maths::Vector3 axProbeAxes[2] = { xForward, xPerp };
		const float afProbeDistances[3] = { 1.0f, 1.5f, 2.5f };
		bool bStitched = false;
		for (int iAxis = 0; iAxis < 2 && !bStitched; ++iAxis)
		{
			for (int iDist = 0; iDist < 3 && !bStitched; ++iDist)
			{
				bStitched = xMutable.StitchPortalAt(xPos, axProbeAxes[iAxis],
					afProbeDistances[iDist], /*fMaxVerticalDist=*/3.0f);
			}
		}
		if (!bStitched)
		{
			Zenith_Log(LOG_CATEGORY_AI,
				"DPDoor::StitchNavMeshPortal: stitch failed at (%g,%g,%g) yaw=%g -- "
				"every probe axis/distance combo returned same-poly or no-poly",
				xPos.x, xPos.y, xPos.z, fYaw);
		}
	}

	void SyncNavMeshBlock()
	{
		// 2026-05-25: doors block the navmesh while Closed or Closing
		// (BlocksPath()). The priest's nav planner won't route through
		// closed doors -- it patrols within the currently-reachable
		// subgraph and pursues only when its plan extends through
		// already-open doors. When the player opens a door, the
		// polygon unblocks and the priest's reach extends.
		//
		// History: 2026-05-25-AM tried no-op'ing this so the priest
		// could plan through closed doors and open them via
		// OpportunisticDoorPress. That combo (free planning + priest
		// auto-opens-on-contact) made the priest reach every objective
		// hand-off zone before the bot could deliver -- the 10-seed
		// matrix went from 50%+ win rates to 0% across all eight
		// personalities. Restored to blocking; priest's TryInteract
		// path stays available for the rare case where the priest's
		// plan ends adjacent to a closed door (e.g. a player opens a
		// door, priest pursues through, player closes it behind --
		// priest opens it again to keep pursuing).
		const Zenith_NavMesh* pxNavMesh = DP_AI::GetOrBuildLevelNavMesh();
		if (pxNavMesh == nullptr) return;
		// Refuse to block on a coarse navmesh. The DP synthetic mesh
		// fallback is a single 300x300 m quad; SetBlockedAtPoint would
		// mark that one polygon, and the Zenith_Pathfinding endpoint-
		// blocked gate would then fail every navmesh query (priest
		// pursuit, patrol sampling, anything). Skip on synthetic.
		constexpr uint32_t kMinPolysForDynamicBlocking = 16;
		if (pxNavMesh->GetPolygonCount() < kMinPolysForDynamicBlocking) return;

		const bool bBlocked = BlocksPath();
		const Zenith_Maths::Vector3 xPos = GetInteractionCentre();
		// 3 m vertical tolerance: door collider centre is at y=1; the
		// navmesh polygons sit around y=1 too. Slack absorbs FP noise.
		pxNavMesh->SetBlockedAtPoint(xPos, bBlocked, /*fMaxVerticalDist=*/3.0f);
	}

	void ApplyRotation()
	{
		if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>()) return;
		Zenith_TransformComponent& xT = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		// 2026-05-25 BUG FIX: previous formula
		//   m_fClosedYaw + (m_fOpenYaw - m_fClosedYaw) * m_fOpenT
		// treated m_fOpenYaw as an ABSOLUTE target angle. For runtime-
		// constructed doors with m_fClosedYaw=0 this happened to work
		// (0 + (90 - 0) * t = 90*t, swinging through 0..90°). For procgen
		// doors with non-zero m_fClosedYaw (set per-wall to align with
		// the wall axis) this rotated the door TO 90° in world space
		// regardless of where it started -- so a door at closed yaw
		// 180° would lerp 180° -> 90°, going backwards by 90° instead
		// of opening by 90°.
		//
		// Correct semantic: m_fOpenYaw is a SWING-BY delta. The door
		// opens by m_fOpenYaw degrees from wherever it was closed.
		const float fAngle = glm::radians(m_fClosedYaw + m_fOpenYaw * m_fOpenT);
		xT.SetRotation(glm::angleAxis(fAngle, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
	}

	// 2026-05-26: returns true if a Pentagram is within the villager's
	// F-press range. Used to defer the door's close-on-F-press when
	// the same F-press is targeting the adjacent pentagram. See the
	// rationale comment in HandleInteractInternal's DoorAnim::Open case.
	bool IsPentagramInRange(Zenith_EntityID xVillager) const
	{
		if (!xVillager.IsValid()) return false;
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return false;
		Zenith_Entity xV = pxScene->TryGetEntity(xVillager);
		if (!xV.IsValid() || !xV.HasComponent<Zenith_TransformComponent>()) return false;
		Zenith_Maths::Vector3 xVPos;
		xV.GetComponent<Zenith_TransformComponent>().GetPosition(xVPos);

		bool bInRange = false;
		DP_Query::ForEachScriptInActiveScene<DPPentagram_Behaviour>(
			[&bInRange, &xVPos, pxScene](Zenith_EntityID xId, DPPentagram_Behaviour& xPent)
			{
				if (bInRange) return;  // already found one
				const float fR = xPent.GetInteractRadius();
				Zenith_Entity xP = pxScene->TryGetEntity(xId);
				if (!xP.IsValid() || !xP.HasComponent<Zenith_TransformComponent>()) return;
				Zenith_Maths::Vector3 xPPos;
				xP.GetComponent<Zenith_TransformComponent>().GetPosition(xPPos);
				const float fDx = xVPos.x - xPPos.x;
				const float fDz = xVPos.z - xPPos.z;
				if (fDx * fDx + fDz * fDz <= fR * fR) bInRange = true;
			});
		return bInRange;
	}

	// 2026-05-25: re-tinting helper. Builds the variant from the cached
	// ORIGINAL materials, not from the current (possibly already-tinted)
	// instance materials, so calling this twice (once on first OnStart,
	// again on unlock) produces exactly two named variants -- not a chain.
	void ApplyLockTint()
	{
		if (!m_xParentEntity.HasComponent<Zenith_ModelComponent>()) return;
		Flux_ModelInstance* pxInstance =
			m_xParentEntity.GetComponent<Zenith_ModelComponent>().GetModelInstance();
		if (pxInstance == nullptr) return;
		const Zenith_Maths::Vector3 xLocked  (0.9f, 0.2f, 0.2f);   // red
		const Zenith_Maths::Vector3 xUnlocked(0.2f, 0.9f, 0.2f);   // green
		const bool bLocked = (m_eRequiredKey != DP_ItemTag::None);
		const Zenith_Maths::Vector3& xCol = bLocked ? xLocked : xUnlocked;
		const char* szCacheKey = bLocked ? "TintDoorLocked" : "TintDoorUnlocked";
		const uint32_t uMats = pxInstance->GetNumMaterials();
		for (uint32_t u = 0; u < uMats && u < m_axOriginalMaterials.GetSize(); ++u)
		{
			Zenith_MaterialAsset* pxBase = m_axOriginalMaterials.Get(u);
			Zenith_MaterialAsset* pxTint =
				DPMaterials::GetOrCreateColouredVariant(pxBase, xCol, szCacheKey);
			if (pxTint) pxInstance->SetMaterial(u, pxTint);
		}
	}

	// 2026-05-25 review fix: default changed from DP_ItemTag::Key to None
	// to match the stated design intent ("unlocked-by-default"). Procgen
	// doors explicitly override this via SetRequiredKey in the bootstrap;
	// runtime-constructed test doors that don't call the setter now
	// start unlocked (matching the new contract) rather than locked.
	DP_ItemTag m_eRequiredKey  = DP_ItemTag::None;
	DoorAnim   m_eAnim         = DoorAnim::Closed;
	float      m_fOpenT        = 0.0f;
	float      m_fClosedYaw    = 0.0f;
	float      m_fOpenYaw      = 90.0f;  // Fallback; OnAwake reads DP_Tuning.
	float      m_fOpenDuration = 0.4f;   // Fallback; OnAwake reads DP_Tuning.

	// 2026-05-25: audio knobs. Cached at OnAwake from the existing-but-
	// previously-unused interactables.door_audible_at_m + _loudness
	// tuning keys.
	float      m_fDoorRadius   = 12.0f;
	float      m_fDoorLoudness = 0.6f;

	// 2026-05-25: logical centre (actual doorway position). Set by the
	// bootstrap via SetLogicalCentre; runtime-constructed test doors
	// leave m_bHasLogicalCentre=false and GetInteractionCentre falls
	// back to the entity transform.
	Zenith_Maths::Vector3 m_xLogicalCentre    = Zenith_Maths::Vector3(0.0f);
	bool                  m_bHasLogicalCentre = false;

	// 2026-05-25: cached BEFORE first tint so ApplyLockTint can rebuild
	// the variant from the un-tinted base on every call (no chaining).
	Zenith_Vector<Zenith_MaterialAsset*> m_axOriginalMaterials;

	// 2026-05-25: rising-edge latch (defensive; ReadInteractPressed is
	// currently rising-edge so the second-frame call should never
	// happen, but the latch insures against a future input-semantic
	// change inadvertently auto-closing a just-opened door).
	bool       m_bWasInteractingLastFrame = false;
	bool       m_bIsInteractingThisFrame  = false;

#ifdef ZENITH_INPUT_SIMULATOR
public:
	float GetOpenYaw() const { return m_fOpenYaw; }
	float GetOpenDuration() const { return m_fOpenDuration; }
	float GetDoorLoudness() const { return m_fDoorLoudness; }
	float GetDoorRadius() const { return m_fDoorRadius; }
#endif
};
