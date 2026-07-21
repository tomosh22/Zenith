#include "Zenith.h"

#include "Zenithmon/Components/ZM_Interactable.h"

#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"   // the three shipped raise seams

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <cmath>   // std::isfinite (radius sanitising)

// ============================================================================
// ZM_Interactable (S6 item 3 SC4). See the header for the contract. The role ->
// seam map is PURE and lives at the top so a unit can exercise it with no scene,
// no singleton and nothing raised; Interact() below is the only impure part.
// ============================================================================

namespace
{
	constexpr float fZM_WANDER_BODY_FRICTION = 0.8f;
	constexpr float fZM_WANDER_BODY_RESTITUTION = 0.0f;

	bool ZM_IsFiniteVector3(const Zenith_Maths::Vector3& xValue)
	{
		return std::isfinite(xValue.x)
			&& std::isfinite(xValue.y)
			&& std::isfinite(xValue.z);
	}

	bool ZM_IsValidWanderConfiguration(const ZM_WalkerWaypoints& xWaypoints,
		const ZM_WalkerTuning& xTuning)
	{
		if (xWaypoints.m_uCount == 0u
			|| xWaypoints.m_uCount > ZM_WalkerWaypoints::uMAX_WAYPOINTS
			|| !std::isfinite(xTuning.m_fSpeed)
			|| xTuning.m_fSpeed <= 0.0f
			|| !std::isfinite(xTuning.m_fArriveRadius)
			|| xTuning.m_fArriveRadius <= 0.0f
			|| !std::isfinite(xTuning.m_fDwellSeconds)
			|| xTuning.m_fDwellSeconds < 0.0f)
		{
			return false;
		}

		for (u_int u = 0u; u < xWaypoints.m_uCount; ++u)
		{
			if (!ZM_IsFiniteVector3(xWaypoints.m_axPoints[u]))
			{
				return false;
			}
		}
		return true;
	}
}

ZM_NPC_RAISE_KIND ZM_RaiseKindForRole(ZM_NPC_ROLE eRole)
{
	switch (eRole)
	{
	case ZM_NPC_ROLE_TALKER:    return ZM_NPC_RAISE_DIALOGUE;
	case ZM_NPC_ROLE_SHOPKEEP:  return ZM_NPC_RAISE_SHOP;
	case ZM_NPC_ROLE_CARETAKER: return ZM_NPC_RAISE_CARE_CENTER;
	// A switch, not a table lookup, precisely so ZM_NPC_ROLE_COUNT and anything
	// past it land here instead of reading off the end of an array.
	default:                    return ZM_NPC_RAISE_NONE;
	}
}

const char* ZM_NpcRaiseKindName(ZM_NPC_RAISE_KIND eKind)
{
	switch (eKind)
	{
	case ZM_NPC_RAISE_NONE:        return "NONE";
	case ZM_NPC_RAISE_DIALOGUE:    return "DIALOGUE";
	case ZM_NPC_RAISE_SHOP:        return "SHOP";
	case ZM_NPC_RAISE_CARE_CENTER: return "CARE_CENTER";
	default:                       return "UNKNOWN";
	}
}

ZM_Interactable::ZM_Interactable(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_Interactable::OnStart()
{
	m_bLifecycleStarted = true;
	m_xConfiguredWanderBodyID = Zenith_PhysicsBodyID{};
	m_xWalkerState = ZM_WalkerState{};
	m_bOwnsInteractionMenu = false;

	// Re-validate what deserialization / authoring left behind. A row id that no
	// longer exists (the roster shrank) must not survive as a live candidate.
	if (m_eNpcId >= ZM_NPC_COUNT)
	{
		m_eNpcId = ZM_NPC_NONE;
		m_bInteractable = false;
	}
	SetRadius(m_fRadius);
	if (m_bWanderEnabled
		&& !ZM_IsValidWanderConfiguration(m_xWalkerWaypoints, m_xWalkerTuning))
	{
		m_bWanderEnabled = false;
	}

	// Stationary NPCs never enter the physics-facing portion of the runtime contract:
	// their existing static AABBs and runtime behaviour remain unchanged. OnStart is
	// non-strict because editor construction may legitimately still be assembling a
	// body; a valid reloaded scene is configured here, and OnUpdate is the strict gate.
	if (!m_bWanderEnabled)
	{
		return;
	}
	TryConfigureWanderBody(false);
}

bool ZM_Interactable::TryConfigureWanderBody(bool bRequireRuntimeReady)
{
	if (!m_bWanderEnabled)
	{
		return false;
	}

	const bool bEntityValid = m_xParentEntity.IsValid();
	Zenith_ColliderComponent* pxCollider =
		bEntityValid
			? m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>()
			: nullptr;
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	const bool bColliderPresent = pxCollider != nullptr;
	const bool bBodyValid = bColliderPresent && pxCollider->HasValidBody();
	const bool bDynamicCapsule = bColliderPresent
		&& pxCollider->GetCollisionVolumeType() == COLLISION_VOLUME_TYPE_CAPSULE
		&& pxCollider->GetRigidBodyType() == RIGIDBODY_TYPE_DYNAMIC;
	const bool bPhysicsActive = xPhysics.HasActiveSimulation();
	const bool bContractValid = bEntityValid
		&& bColliderPresent
		&& bBodyValid
		&& bDynamicCapsule
		&& bPhysicsActive;
	if (!bContractValid)
	{
		m_xConfiguredWanderBodyID = Zenith_PhysicsBodyID{};
		if (bRequireRuntimeReady)
		{
			Zenith_Assert(bContractValid,
				"[ZM_Interactable] enabled wander patrol requires an active physics "
				"simulation and a valid DYNAMIC CAPSULE body "
				"(npcId=%u entityValid=%d colliderPresent=%d bodyValid=%d "
				"dynamicCapsule=%d physicsActive=%d)",
				(u_int)m_eNpcId, (int)bEntityValid, (int)bColliderPresent,
				(int)bBodyValid, (int)bDynamicCapsule, (int)bPhysicsActive);
			// Release builds compile the assertion out, so the explicit state change is
			// the shipping fail-closed path rather than a debug-only diagnostic.
			m_bWanderEnabled = false;
		}
		return false;
	}

	const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
	if (m_xConfiguredWanderBodyID == xBodyID)
	{
		return true;
	}

	pxCollider->SetIsSensor(false);
	xPhysics.SetGravityEnabled(xBodyID, true);
	xPhysics.LockRotation(xBodyID, true, false, true);
	xPhysics.EnforceUpright(xBodyID);
	xPhysics.SetFriction(xBodyID, fZM_WANDER_BODY_FRICTION);
	xPhysics.SetRestitution(xBodyID, fZM_WANDER_BODY_RESTITUTION);
	m_xConfiguredWanderBodyID = xBodyID;
	return true;
}

void ZM_Interactable::OnUpdate(float fDeltaTime)
{
	if (!m_bWanderEnabled)
	{
		return;
	}
	// This is the runtime-ready point: unlike construction/OnStart, an enabled
	// walker reaching an update must already own the exact body it promises.
	if (!TryConfigureWanderBody(true))
	{
		return;
	}
	if (!std::isfinite(fDeltaTime) || fDeltaTime <= 0.0f)
	{
		return;
	}

	Zenith_TransformComponent* pxTransform =
		m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
	Zenith_ColliderComponent* pxCollider =
		m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	// TryConfigureWanderBody just proved the collider/simulation contract on this
	// same main-thread tick; the transform is the engine's mandatory entity pose.
	if (pxTransform == nullptr)
	{
		return;
	}

	// Interact() latches ownership only after THIS component successfully raises a
	// screen. Another NPC or the pause menu therefore cannot stop this patrol. Keep
	// the authored enabled observation true; halting is an input to the pure step.
	const bool bMenuOpen = ZM_UI_MenuStack::IsMenuOpen();
	const bool bHalted = m_bOwnsInteractionMenu && bMenuOpen;
	if (!bMenuOpen)
	{
		m_bOwnsInteractionMenu = false;
	}

	Zenith_Maths::Vector3 xPosition(0.0f);
	pxTransform->GetPosition(xPosition);
	const ZM_WalkerStep xStep = ZM_StepWalker(
		m_xWalkerWaypoints, m_xWalkerState, xPosition,
		fDeltaTime, bHalted, m_xWalkerTuning);

	const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
	const Zenith_Maths::Vector3 xCurrentVelocity =
		xPhysics.GetLinearVelocity(xBodyID);
	// No SetPosition / teleport: the dynamic capsule alone owns translation. The
	// helper replaces XZ while preserving Y verbatim for gravity/terrain response.
	xPhysics.SetLinearVelocity(xBodyID,
		ZM_BuildPatrolVelocity(xStep.m_xDirXZ, xStep.m_fSpeed, xCurrentVelocity));
}

bool ZM_Interactable::SetNpcId(ZM_NPC_ID eId)
{
	if (eId >= ZM_NPC_COUNT)
	{
		// Fail CLOSED: clearing the row (rather than keeping the previous one) means a
		// bad authoring value produces an inert NPC, never a wrong conversation.
		m_eNpcId = ZM_NPC_NONE;
		m_bInteractable = false;
		return false;
	}
	m_eNpcId = eId;
	return true;
}

bool ZM_Interactable::SetRadius(float fRadius)
{
	if (!std::isfinite(fRadius))
	{
		m_fRadius = fDEFAULT_RADIUS;
		return false;
	}
	if (fRadius < 0.0f)
	{
		m_fRadius = 0.0f;
		return false;
	}
	if (fRadius > fMAX_RADIUS)
	{
		m_fRadius = fMAX_RADIUS;
		return false;
	}
	m_fRadius = fRadius;
	return true;
}

bool ZM_Interactable::ConfigureWander(const ZM_WalkerWaypoints& xWaypoints,
	const ZM_WalkerTuning& xTuning)
{
	m_xWalkerState = ZM_WalkerState{};
	m_bOwnsInteractionMenu = false;
	if (!ZM_IsValidWanderConfiguration(xWaypoints, xTuning))
	{
		m_xWalkerWaypoints = ZM_WalkerWaypoints{};
		m_xWalkerTuning = ZM_WalkerTuning{};
		m_bWanderEnabled = false;
		m_xConfiguredWanderBodyID = Zenith_PhysicsBodyID{};
		return false;
	}

	m_xWalkerWaypoints = xWaypoints;
	m_xWalkerTuning = xTuning;
	m_bWanderEnabled = true;
	// Configure immediately when this is a live, already-started entity with its
	// body ready. Missing pre-body editor state remains harmless until OnUpdate's
	// strict runtime gate; it is still serialized so the completed scene can reload.
	if (m_bLifecycleStarted)
	{
		TryConfigureWanderBody(false);
	}
	return true;
}

bool ZM_Interactable::Interact()
{
	if (m_eNpcId >= ZM_NPC_COUNT)
	{
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_Interactable] Interact on an UNCONFIGURED interactable (no NPC row) -- nothing raised");
		return false;
	}

	const ZM_NpcData& xRow = ZM_GetNpcData(m_eNpcId);
	const ZM_NPC_RAISE_KIND eKind = ZM_RaiseKindForRole(xRow.m_eRole);

	bool bRaised = false;
	switch (eKind)
	{
	case ZM_NPC_RAISE_DIALOGUE:
		bRaised = ZM_UI_MenuStack::TryPushDialogue(xRow.m_paszLines, xRow.m_uLineCount);
		break;
	case ZM_NPC_RAISE_SHOP:
		bRaised = ZM_UI_MenuStack::TryOpenShop(xRow.m_paeStock, xRow.m_uStockCount);
		break;
	case ZM_NPC_RAISE_CARE_CENTER:
		bRaised = ZM_UI_MenuStack::TryOpenCareCenterPrompt();
		break;
	default:
		// An unmapped role is CONTENT breakage, not a runtime condition: it must be
		// loud rather than a silent no-op (Shortfalls 1.6).
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_Interactable] NPC '%s' (id %u) has UNMAPPED role %u -- no seam to raise",
			xRow.m_szDisplayName, (u_int)m_eNpcId, (u_int)xRow.m_eRole);
		return false;
	}

	if (!bRaised)
	{
		// The seam refused. Previously this was silent, so a mis-authored NPC read as
		// a mute one with no diagnostic anywhere; name the NPC and the seam it tried.
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_Interactable] NPC '%s' (id %u, role %u) could not raise its %s screen "
			"-- the seam refused (no ZM_MenuRoot singleton, or the screen rejected its content)",
			xRow.m_szDisplayName, (u_int)m_eNpcId, (u_int)xRow.m_eRole,
			ZM_NpcRaiseKindName(eKind));
	}
	m_bOwnsInteractionMenu = bRaised;
	return bRaised;
}

void ZM_Interactable::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
	xStream << (u_int)m_eNpcId;
	xStream << m_fRadius;
	xStream << m_bInteractable;
	xStream << m_bWanderEnabled;
	xStream << m_xWalkerWaypoints.m_uCount;
	for (u_int u = 0u; u < ZM_WalkerWaypoints::uMAX_WAYPOINTS; ++u)
	{
		xStream << m_xWalkerWaypoints.m_axPoints[u].x;
		xStream << m_xWalkerWaypoints.m_axPoints[u].y;
		xStream << m_xWalkerWaypoints.m_axPoints[u].z;
	}
	xStream << m_xWalkerTuning.m_fSpeed;
	xStream << m_xWalkerTuning.m_fArriveRadius;
	xStream << m_xWalkerTuning.m_fDwellSeconds;
}

void ZM_Interactable::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;
	m_eNpcId = ZM_NPC_NONE;
	m_fRadius = fDEFAULT_RADIUS;
	m_bInteractable = false;
	m_xWalkerWaypoints = ZM_WalkerWaypoints{};
	m_xWalkerTuning = ZM_WalkerTuning{};
	m_xWalkerState = ZM_WalkerState{};
	m_bWanderEnabled = false;
	m_bOwnsInteractionMenu = false;
	m_xConfiguredWanderBodyID = Zenith_PhysicsBodyID{};
	if (uVersion != 1u && uVersion != uSERIALIZATION_VERSION)
	{
		return;
	}

	u_int uNpcId = (u_int)ZM_NPC_NONE;
	float fRadius = fDEFAULT_RADIUS;
	bool bInteractable = false;
	xStream >> uNpcId;
	xStream >> fRadius;
	xStream >> bInteractable;

	// Route every field through the validating setters, so a hand-edited or stale
	// scene file cannot install a state the live setters would have refused.
	SetNpcId((ZM_NPC_ID)uNpcId);
	SetRadius(fRadius);
	SetInteractable(bInteractable);

	// Version 1 scenes predate patrols; their NPCs deliberately remain stationary.
	if (uVersion == 1u)
	{
		return;
	}

	bool bWanderEnabled = false;
	u_int uWaypointCount = 0u;
	ZM_WalkerWaypoints xWaypoints;
	ZM_WalkerTuning xTuning;
	xStream >> bWanderEnabled;
	xStream >> uWaypointCount;
	for (u_int u = 0u; u < ZM_WalkerWaypoints::uMAX_WAYPOINTS; ++u)
	{
		xStream >> xWaypoints.m_axPoints[u].x;
		xStream >> xWaypoints.m_axPoints[u].y;
		xStream >> xWaypoints.m_axPoints[u].z;
	}
	xStream >> xTuning.m_fSpeed;
	xStream >> xTuning.m_fArriveRadius;
	xStream >> xTuning.m_fDwellSeconds;
	xWaypoints.m_uCount = uWaypointCount;

	if (ZM_IsValidWanderConfiguration(xWaypoints, xTuning))
	{
		m_xWalkerWaypoints = xWaypoints;
		m_xWalkerTuning = xTuning;
		m_bWanderEnabled = bWanderEnabled;
		if (m_bWanderEnabled && m_bLifecycleStarted)
		{
			TryConfigureWanderBody(false);
		}
	}
}

#ifdef ZENITH_TOOLS
void ZM_Interactable::RenderPropertiesPanel()
{
	const bool bHasRow = m_eNpcId < ZM_NPC_COUNT;
	ImGui::Text("NPC: %s (id %u)",
		bHasRow ? ZM_GetNpcData(m_eNpcId).m_szDisplayName : "<none>", (u_int)m_eNpcId);
	ImGui::Text("Role raise kind: %s", ZM_NpcRaiseKindName(bHasRow
		? ZM_RaiseKindForRole(ZM_GetNpcData(m_eNpcId).m_eRole)
		: ZM_NPC_RAISE_NONE));
	ImGui::Text("Reach bonus: %.2f", m_fRadius);
	ImGui::Text("Interactable: %s", IsInteractable() ? "true" : "false");
	ImGui::Text("Wander enabled: %s", IsWanderEnabled() ? "true" : "false");
	ImGui::Text("Waypoint: %u / %u", GetWaypointIndex(), GetWaypointCount());
}
#endif
