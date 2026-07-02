#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPDoor_Component - the SYSTEMS SHIM for the graph-driven door.
 *
 * Wave-2 graph conversion: the door's gameplay DECISIONS (key gating with
 * sticky unlock, state transitions, open/close events + audio cues, the
 * pentagram close-deferral) live in the boot-authored DP_Door.bgraph
 * (DPNode_DoorHandleInteract + DPNode_DoorAdvanceAnim). This component keeps
 * everything systems-tier, exactly as before:
 *   - proximity + F-press rising-edge latch (DPInteractable_Base + latch),
 *     relayed to the graph as an "Interact" custom event with the villager
 *     as a packed-EntityID payload,
 *   - navmesh portal stitching (OnStart) + runtime BLOCKED toggling,
 *   - collider solidity (SOLID iff Closed; sensor otherwise),
 *   - lock tint (red/green material variants),
 *   - closed-yaw capture + the open-rotation application,
 *   - the logical-centre anchor for range/audio/navmesh probes.
 *
 * State machine (graph blackboard "anim": 0=Closed 1=Opening 2=Open
 * 3=Closing; "openT"; "requiredKey" as int DP_ItemTag):
 *   Closed -> Opening: F-press in range; consumes a Key iff locked.
 *                      Unlocking is STICKY (requiredKey set to None).
 *   Opening -> Open:   openT reaches 1.0 (door_open_duration_s later).
 *   Open -> Closing:   F-press in range (no key needed). Deferred when a
 *                      pentagram is also in the villager's F-range.
 *   Closing -> Closed: openT reaches 0.0.
 *
 * Navmesh coupling: BLOCKED iff (Closed || Closing). The graph nodes call
 * OnDoorStateChanged() SYNCHRONOUSLY at every transition so the navmesh and
 * collider react the same frame (no 1-frame race for the priest).
 *
 * Logical centre: the geometric door centre (the transform is corner-offset
 * ~1 m by SM_Cube anchoring). Stitching, the in-range test, and audio anchor
 * on it; falls back to the transform when unset (runtime test doors).
 */

#include "Components/DPInteractable_Base.h"
#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "Maths/Zenith_Maths.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "Source/PublicInterfaces.h"
#include "Source/DP_AI.h"
#include "Source/DP_Tuning.h"
#include "Source/DPMaterials.h"
#include "Source/DevilsPlayground_Tags.h"

class DPDoor_Component ZENITH_FINAL : public DPInteractable_Base
{
public:
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

	static constexpr const char* kszGraphAsset = "game:Graphs/DP_Door.bgraph";

	DPDoor_Component() = delete;
	DPDoor_Component(Zenith_Entity& xParentEntity)
		: DPInteractable_Base(xParentEntity)
	{}

	void OnAwake()
	{
		DPInteractable_Base::OnAwake();
		// Doors are runtime-blockable obstacles. The wall colliders surrounding
		// each room have authored doorway GAPS that are exactly door-width;
		// the door entity itself sits IN that gap with its own collider. If
		// the door's collider participated in the navmesh, the gap would be
		// sealed and AI couldn't path between rooms even when the door is
		// open. Mark the collider as nav-mesh-excluded so the generator emits
		// walkable polygons through the doorway; SyncNavMeshBlock then toggles
		// those polygons' BLOCKED flag at runtime based on open/closed state.
		if (Zenith_ColliderComponent* pxCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>())
		{
			pxCollider->SetIncludeInNavMesh(false);
		}
	}

	void OnStart()
	{
		// Defer navmesh work to OnStart: editor automation runs all steps
		// before OnStart fires, so the position is final here.
		StitchNavMeshPortal();
		SyncNavMeshBlock();

		// Cache the original materials BEFORE the first tint so RefreshLockTint
		// can re-tint to a different colour on unlock without building a chain
		// of variants.
		if (Zenith_ModelComponent* pxModel = m_xParentEntity.TryGetComponent<Zenith_ModelComponent>())
		{
			Flux_ModelInstance* pxInstance = pxModel->GetModelInstance();
			if (pxInstance != nullptr)
			{
				const uint32_t uMats = pxInstance->GetNumMaterials();
				for (uint32_t u = 0; u < uMats; ++u)
				{
					m_axOriginalMaterials.PushBack(pxInstance->GetMaterial(u));
				}
			}
		}
		RefreshLockTint();

		// Capture the door's CLOSED yaw from its current transform rotation so
		// the open-rotation interpolation starts from the procgen-set angle.
		if (Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>())
		{
			Zenith_Maths::Quat xQuat;
			pxTransform->GetRotation(xQuat);
			const float fSiny_cosp = 2.0f * (xQuat.w * xQuat.y + xQuat.x * xQuat.z);
			const float fCosy_cosp = 1.0f - 2.0f * (xQuat.y * xQuat.y + xQuat.x * xQuat.x);
			m_fClosedYaw = glm::degrees(std::atan2(fSiny_cosp, fCosy_cosp));
		}
	}

	void OnUpdate(const float fDt)
	{
		// Rising-edge latch on F-press (defensive belt-and-braces against a
		// future held-state ReadInteractPressed semantic).
		m_bWasInteractingLastFrame = m_bIsInteractingThisFrame;
		m_bIsInteractingThisFrame  = false;
		DPInteractable_Base::OnUpdate(fDt);
		// The animation advance itself runs in the graph (DPNode_DoorAdvanceAnim
		// on the OnUpdate event), which calls back into ApplyRotationFromT /
		// OnDoorStateChanged.
	}

	//--------------------------------------------------------------------------
	// State accessors - same public surface as the pre-graph component; the
	// state now lives on the sibling graph's blackboard.
	//--------------------------------------------------------------------------

	DoorAnim GetAnim() const
	{
		return static_cast<DoorAnim>(ReadBlackboardInt("anim", 0));
	}

	// Open iff Open || Opening (tests asserting "F-pressed door is now open"
	// don't wait the full animation).
	bool IsOpen() const
	{
		const DoorAnim eAnim = GetAnim();
		return eAnim == DoorAnim::Open || eAnim == DoorAnim::Opening;
	}

	// Closed iff Closed || Closing. Shared by the bot's path grid and
	// SyncNavMeshBlock.
	bool BlocksPath() const
	{
		const DoorAnim eAnim = GetAnim();
		return eAnim == DoorAnim::Closed || eAnim == DoorAnim::Closing;
	}

	DP_ItemTag GetRequiredKey() const
	{
		return static_cast<DP_ItemTag>(ReadBlackboardInt("requiredKey", static_cast<int32_t>(DP_ItemTag::None)));
	}

	// Bootstrap seeding: writes the per-door lock state into the graph
	// blackboard (call AFTER the graph slot is attached).
	void SetRequiredKey(DP_ItemTag e)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(static_cast<int32_t>(e));
		WriteBlackboardValue("requiredKey", xValue);
	}

	void SetLogicalCentre(const Zenith_Maths::Vector3& v)
	{
		m_xLogicalCentre    = v;
		m_bHasLogicalCentre = true;
	}

	// The actual interaction anchor (geometric door centre); falls back to the
	// transform position. Used by tests, AI, the bot's grid, audio, range.
	Zenith_Maths::Vector3 GetInteractionCentre() const
	{
		if (m_bHasLogicalCentre) return m_xLogicalCentre;
		Zenith_Maths::Vector3 xPos(0.0f);
		if (Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>())
		{
			pxTransform->GetPosition(xPos);
		}
		return xPos;
	}

	// Bot grid needs the door's closed yaw to rasterise the OBB footprint.
	float GetClosedYawDegrees() const { return m_fClosedYaw; }

#ifdef ZENITH_INPUT_SIMULATOR
	// Test getters: the effective animation/audio tuning, read LIVE from
	// DP_Tuning exactly as the graph nodes read it (no cached members).
	float GetOpenYaw() const      { return DP_Tuning::Get<float>("interactables.door_open_yaw_deg"); }
	float GetOpenDuration() const { return DP_Tuning::Get<float>("interactables.door_open_duration_s"); }
	float GetDoorLoudness() const { return DP_Tuning::Get<float>("interactables.door_audible_loudness"); }
	float GetDoorRadius() const   { return DP_Tuning::Get<float>("interactables.door_audible_at_m"); }
#endif

	// External-actor interface for priests + scripted events. Bypasses the
	// range gating AND the F-press release latch; honors lock state (the graph
	// node's key check rejects keyless actors on locked doors).
	void TryInteract(Zenith_EntityID xActor) { FireInteractEvent(xActor); }

	//--------------------------------------------------------------------------
	// Systems execution - called SYNCHRONOUSLY by the door graph nodes at
	// state transitions / animation frames.
	//--------------------------------------------------------------------------

	// Navmesh BLOCKED toggle + collider solidity for the current state.
	void OnDoorStateChanged()
	{
		SyncNavMeshBlock();
		ApplyColliderSolidity();
	}

	// Open-rotation application: m_fOpenYaw is a SWING-BY delta from the
	// captured closed yaw (procgen doors have non-zero closed yaw).
	void ApplyRotationFromT(float fOpenT)
	{
		Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return;
		const float fOpenYaw = DP_Tuning::Get<float>("interactables.door_open_yaw_deg");
		const float fAngle = glm::radians(m_fClosedYaw + fOpenYaw * fOpenT);
		pxTransform->SetRotation(glm::angleAxis(fAngle, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
	}

	// Re-tint from the cached ORIGINAL materials (red locked / green unlocked).
	void RefreshLockTint()
	{
		Zenith_ModelComponent* pxModel = m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr) return;
		Flux_ModelInstance* pxInstance = pxModel->GetModelInstance();
		if (pxInstance == nullptr) return;
		const Zenith_Maths::Vector3 xLocked  (0.9f, 0.2f, 0.2f);   // red
		const Zenith_Maths::Vector3 xUnlocked(0.2f, 0.9f, 0.2f);   // green
		const bool bLocked = (GetRequiredKey() != DP_ItemTag::None);
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

protected:
	// Range check anchors on the logical centre (the actual doorway), not the
	// corner-offset transform.
	bool IsVillagerInRange(Zenith_EntityID xVillager) const override
	{
		if (!xVillager.IsValid()) return false;
		Zenith_Entity xV = g_xEngine.Scenes().ResolveEntity(xVillager);
		if (!xV.IsValid()) return false;
		Zenith_TransformComponent* pxTransform = xV.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return false;

		Zenith_Maths::Vector3 xVPos;
		pxTransform->GetPosition(xVPos);
		const Zenith_Maths::Vector3 xMyPos = GetInteractionCentre();

		const float fDx = xVPos.x - xMyPos.x;
		const float fDz = xVPos.z - xMyPos.z;
		const float fRadius = GetInteractRadius();
		return fDx * fDx + fDz * fDz <= fRadius * fRadius;
	}

	void HandleInteract(Zenith_EntityID xVillager) override
	{
		// Release latch: reject HOLD frames; accept fresh rising-edge presses.
		m_bIsInteractingThisFrame = true;
		if (m_bWasInteractingLastFrame) return;
		FireInteractEvent(xVillager);
	}

private:
	void FireInteractEvent(Zenith_EntityID xActor)
	{
		if (!m_xParentEntity.IsValid())
		{
			return;
		}
		Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr)
		{
			return;
		}
		Zenith_PropertyValue xPayload;
		xPayload.SetPackedEntityID(xActor.GetPacked());
		pxGraph->FireCustomEvent("Interact", &xPayload);
	}

	//--------------------------------------------------------------------------
	// Blackboard plumbing (the door graph's slot on this entity)
	//--------------------------------------------------------------------------

	Zenith_BehaviourGraph* FindDoorGraph() const
	{
		if (!m_xParentEntity.IsValid())
		{
			return nullptr;
		}
		Zenith_GraphComponent* pxGraphs = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraphs == nullptr)
		{
			return nullptr;
		}
		for (u_int u = 0; u < pxGraphs->GetGraphCount(); ++u)
		{
			if (std::strcmp(pxGraphs->GetGraphAssetPathAt(u), kszGraphAsset) == 0)
			{
				return pxGraphs->GetGraphAt(u);
			}
		}
		return nullptr;
	}

	int32_t ReadBlackboardInt(const char* szVar, int32_t iDefault) const
	{
		Zenith_BehaviourGraph* pxGraph = FindDoorGraph();
		return pxGraph ? pxGraph->GetBlackboard().GetInt32(szVar, iDefault) : iDefault;
	}

	void WriteBlackboardValue(const char* szVar, const Zenith_PropertyValue& xValue)
	{
		Zenith_BehaviourGraph* pxGraph = FindDoorGraph();
		if (pxGraph)
		{
			pxGraph->GetBlackboard().SetValue(szVar, xValue);
		}
	}

	//--------------------------------------------------------------------------
	// Systems internals (verbatim from the pre-graph component)
	//--------------------------------------------------------------------------

	// Door collider is a SENSOR while !Closed, SOLID while Closed (the 90 deg
	// open-rotation swings the door's arm into the adjacent room; without the
	// toggle the player capsule gets shoved and wedges in the corridor).
	void ApplyColliderSolidity()
	{
		Zenith_ColliderComponent* pxCollider = m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCollider == nullptr) return;
		const bool bSolid = (GetAnim() == DoorAnim::Closed);
		pxCollider->SetIsSensor(!bSolid);
	}

	void StitchNavMeshPortal()
	{
		// Bridge the two rooms separated by this door at the navmesh level
		// (graph-only neighbour link between the nearest walkable polygons on
		// each side of the door's facing direction).
		Zenith_TransformComponent* pxTransform = m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return;
		const Zenith_NavMesh* pxNavMesh = DP_AI::GetOrBuildLevelNavMesh();
		if (pxNavMesh == nullptr) return;

		// Only stitch the real generated navmesh, not the legacy synthetic.
		constexpr uint32_t kMinPolysForStitching = 16;
		if (pxNavMesh->GetPolygonCount() < kMinPolysForStitching) return;

		// Probe at the LOGICAL centre (the transform is corner-offset and can
		// land inside a wall).
		const Zenith_Maths::Vector3 xPos = GetInteractionCentre();
		Zenith_Maths::Quat xRot;
		pxTransform->GetRotation(xRot);

		const float fYaw = std::atan2(2.0f * (xRot.w * xRot.y + xRot.x * xRot.z),
			1.0f - 2.0f * (xRot.y * xRot.y + xRot.z * xRot.z));
		const float fCos = std::cos(fYaw);
		const float fSin = std::sin(fYaw);
		const Zenith_Maths::Vector3 xForward(fCos, 0.0f, -fSin);

		// All DP navmesh mutation runs on the main thread (stitch in OnStart,
		// BLOCKED toggles from graph dispatch); assert so a future off-thread
		// caller trips here rather than racing.
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DPDoor::StitchNavMeshPortal must run on the main thread (const_cast navmesh mutation)");
		Zenith_NavMesh& xMutable = const_cast<Zenith_NavMesh&>(*pxNavMesh);

		const Zenith_Maths::Vector3 xPerp(-fSin, 0.0f, -fCos);
		// 2026-07-01 widened probe set (priest stuck-in-buildings fix).
		// Procgen emits doors on yawed walls whose voxelised navmesh edges
		// are jagged + erosion-widened; the original 2-axis x 3-distance set
		// missed on ~9 doors per level, and a missed stitch seals the room
		// for pathfinding even with the door physically open. The original
		// axis/distance combos run FIRST (unchanged stitch geometry for
		// doors that already worked); the diagonals + extra distances only
		// engage where the old set failed. Failures are now counted via
		// DP_AI::NotifyDoorStitchFailed and gated at 0 across the canonical
		// 10 seeds by Test_ProcLevel_PriestReachability.
		const Zenith_Maths::Vector3 xDiagA = glm::normalize(xForward + xPerp);
		const Zenith_Maths::Vector3 xDiagB = glm::normalize(xForward - xPerp);
		const Zenith_Maths::Vector3 axProbeAxes[4] = { xForward, xPerp, xDiagA, xDiagB };
		const float afProbeDistances[7] = { 1.0f, 1.5f, 2.5f, 0.75f, 1.25f, 2.0f, 3.0f };
		bool bStitched = false;
		for (int iAxis = 0; iAxis < 4 && !bStitched; ++iAxis)
		{
			for (int iDist = 0; iDist < 7 && !bStitched; ++iDist)
			{
				bStitched = xMutable.StitchPortalAt(xPos, axProbeAxes[iAxis],
					afProbeDistances[iDist], /*fMaxVerticalDist=*/3.0f);
			}
		}
		if (!bStitched)
		{
			// StitchPortalAt returns false BOTH when no polygon pair was
			// found AND when the two sides are already connected (same poly
			// / already neighbours — a benign no-op). Only count + log when
			// the door's two sides genuinely sit in different connected
			// components: that is the "room sealed for pathfinding" hazard
			// Test_ProcLevel_PriestReachability gates at zero.
			Zenith_Maths::Vector3 xSideA = xPos;
			Zenith_Maths::Vector3 xSideB = xPos;
			bool bFoundA = false, bFoundB = false;
			for (int iDist = 0; iDist < 3 && (!bFoundA || !bFoundB); ++iDist)
			{
				const float fD = afProbeDistances[iDist];
				if (!bFoundA && pxNavMesh->FindPolygonContaining(xPos + xForward * fD, 3.0f) != UINT32_MAX)
				{
					xSideA = xPos + xForward * fD;
					bFoundA = true;
				}
				if (!bFoundB && pxNavMesh->FindPolygonContaining(xPos - xForward * fD, 3.0f) != UINT32_MAX)
				{
					xSideB = xPos - xForward * fD;
					bFoundB = true;
				}
			}
			const bool bSidesConnected = bFoundA && bFoundB &&
				DP_AI::ArePositionsConnected(xSideA, xSideB);
			if (!bSidesConnected)
			{
				DP_AI::NotifyDoorStitchFailed();
				Zenith_Log(LOG_CATEGORY_AI,
					"DPDoor::StitchNavMeshPortal: stitch failed at (%g,%g,%g) yaw=%g -- "
					"every probe axis/distance combo returned no-poly and the door's "
					"sides are not path-connected (room sealed for pathfinding)",
					xPos.x, xPos.y, xPos.z, fYaw);
			}
		}
	}

public:
	// Public so the graph's transition node can re-block IMMEDIATELY at the
	// Open -> Closing transition (the priest must not race a closing door).
	void SyncNavMeshBlock()
	{
		// Doors block the navmesh while Closed or Closing (BlocksPath()).
		const Zenith_NavMesh* pxNavMesh = DP_AI::GetOrBuildLevelNavMesh();
		if (pxNavMesh == nullptr) return;
		// Refuse to block on a coarse navmesh (the DP synthetic fallback is a
		// single quad; blocking it would fail every navmesh query).
		constexpr uint32_t kMinPolysForDynamicBlocking = 16;
		if (pxNavMesh->GetPolygonCount() < kMinPolysForDynamicBlocking) return;

		const bool bBlocked = BlocksPath();
		const Zenith_Maths::Vector3 xPos = GetInteractionCentre();
		// 3 m vertical tolerance: door collider centre is at y=1; the navmesh
		// polygons sit around y=1 too.
		pxNavMesh->SetBlockedAtPoint(xPos, bBlocked, /*fMaxVerticalDist=*/3.0f);
	}

private:
	float m_fClosedYaw = 0.0f;
	bool  m_bWasInteractingLastFrame = false;
	bool  m_bIsInteractingThisFrame = false;
	bool  m_bHasLogicalCentre = false;
	Zenith_Maths::Vector3 m_xLogicalCentre = Zenith_Maths::Vector3(0.0f);
	Zenith_Vector<Zenith_MaterialAsset*> m_axOriginalMaterials;
};
