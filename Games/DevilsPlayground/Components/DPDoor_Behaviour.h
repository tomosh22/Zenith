#pragma once
/**
 * DPDoor_Behaviour - lockable door requiring a matching key tag.
 * Lerps the entity rotation between closed and open Y-axis angles.
 *
 * Navmesh coupling: while m_bIsOpen is false, the door blocks the
 * navmesh polygon that contains its world position so AI pathfinding
 * (priest pursuit, future villagers) routes around closed doors. The
 * block flips off on unlock, opening a passage on the next path query
 * without requiring a navmesh rebuild.
 */

#include "Components/DPInteractable_Behaviour.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Maths/Zenith_Maths.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"

class DPDoor_Behaviour ZENITH_FINAL : public DPInteractable_Behaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPDoor_Behaviour)

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
		m_bIsOpen = false;
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
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		DPInteractable_Behaviour::OnUpdate(fDt);

		if (m_bIsOpen && m_fOpenT < 1.0f)
		{
			m_fOpenT = glm::min(1.0f, m_fOpenT + fDt / m_fOpenDuration);
			ApplyRotation();
		}
	}

public:
	bool IsOpen() const { return m_bIsOpen; }
	DP_ItemTag GetRequiredKey() const { return m_eRequiredKey; }

protected:
	void HandleInteract(Zenith_EntityID xVillager) override
	{
		if (m_bIsOpen) return;
		if (!DP_Items::TryConsumeKeyForUnlock(xVillager, m_eRequiredKey))
		{
			// 2026-05-21: telegraph the rejection so the player can see
			// the door knows it's locked. Without this the F-press
			// produced ZERO feedback -- identical to pressing F at empty
			// floor. The particles system fires a red rejection puff;
			// the HUD raises a "Locked -- needs <key>" line for ~2 s.
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnDoorLockRejected{ xVillager, m_xParentEntity.GetEntityID(), m_eRequiredKey });
			return;
		}
		m_bIsOpen = true;
		// Carve the door cell out of the AI blocker set so pursuit paths
		// can now traverse it. The pathfinder picks this up on the next
		// FindPath query — no navmesh rebuild, no agent reset.
		SyncNavMeshBlock();
		// Phase-5-audit (2026-05-16): announce the door-open milestone so
		// the telemetry analyzer + visualiser can plot it as a discrete
		// event rather than burying it in generic DP_OnInteract noise.
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnDoorOpened{ xVillager, m_xParentEntity.GetEntityID() });
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

		Zenith_Maths::Vector3 xPos;
		Zenith_Maths::Quat xRot;
		Zenith_TransformComponent& xT =
			m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		xT.GetPosition(xPos);
		xT.GetRotation(xRot);

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
		if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>()) return;
		const Zenith_NavMesh* pxNavMesh = DP_AI::GetOrBuildLevelNavMesh();
		if (pxNavMesh == nullptr) return;
		// Refuse to block on a coarse navmesh. The current DP synthetic mesh
		// is a single 300×300 m quad, so SetBlockedAtPoint would mark that
		// one polygon blocked — and after the Zenith_Pathfinding endpoint-
		// blocked gate landed, that means EVERY navmesh query returns FAILED
		// (priest pursuit, patrol sampling, anything). The dynamic-obstacle
		// scheme only makes sense once the navmesh has many small polygons
		// aligned with the actual doorway footprints; until then we let the
		// door's physical Jolt collider do all the blocking and leave
		// FLAG_BLOCKED untouched.
		//
		// The threshold of "more than ~16 polygons" is conservative: any
		// reasonable scene-driven navmesh (Recast-equivalent) will produce
		// hundreds of polygons across a single playable area, so this gate
		// auto-enables the moment GenerateFromScene replaces the flat
		// quad. Tools/CI scenes that author tiny test navmeshes can still
		// hit this threshold by tuning Zenith_NavMeshGenerator's poly
		// subdivision.
		constexpr uint32_t kMinPolysForDynamicBlocking = 16;
		if (pxNavMesh->GetPolygonCount() < kMinPolysForDynamicBlocking) return;

		Zenith_Maths::Vector3 xPos;
		m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		// Closed = blocked, open = unblocked. 3 m vertical tolerance covers
		// the door collider's centre being at y=1 while the navmesh quad
		// sits at y=1; the slack absorbs floating-point noise and the
		// occasional door placed at y=0.
		pxNavMesh->SetBlockedAtPoint(xPos, /*bBlocked=*/!m_bIsOpen, /*fMaxVerticalDist=*/3.0f);
	}

private:
	void ApplyRotation()
	{
		if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>()) return;
		Zenith_TransformComponent& xT = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
		const float fAngle = glm::radians(m_fClosedYaw + (m_fOpenYaw - m_fClosedYaw) * m_fOpenT);
		xT.SetRotation(glm::angleAxis(fAngle, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
	}

	DP_ItemTag m_eRequiredKey = DP_ItemTag::Key;
	bool       m_bIsOpen      = false;
	float      m_fOpenT       = 0.0f;
	float      m_fClosedYaw   = 0.0f;
	float      m_fOpenYaw     = 90.0f; // Fallback; OnAwake reads DP_Tuning.
	float      m_fOpenDuration = 0.4f; // Fallback; OnAwake reads DP_Tuning.

#ifdef ZENITH_INPUT_SIMULATOR
public:
	float GetOpenYaw() const { return m_fOpenYaw; }
	float GetOpenDuration() const { return m_fOpenDuration; }
#endif
};
