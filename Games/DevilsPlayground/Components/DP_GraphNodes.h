#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DP_GraphNodes - DevilsPlayground's Behaviour Graph node library.
 *
 * Each node wraps EXACTLY the body of the C++ interactable/menu logic it
 * replaced (DPPentagram / DPChest / DPDoubleDoor / DummyNoiseMachine /
 * DPMainMenuController HandleInteract-or-click handlers), so the graph
 * versions are behaviourally identical: same DP_* namespace calls, same
 * analytics events, same guards, same tuning reads. State that tests used
 * to read off component members (isOpen, openT) lives on the graph
 * blackboard under designer-visible variable names.
 *
 * Registered from Project_RegisterGameComponents via DP_RegisterGraphNodes().
 * The "Interact" custom event (packed-villager payload) is fired by
 * DPGraphInteractable_Component; "MenuPlay"/"MenuQuit" by
 * DPMenuRelay_Component.
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "Input/Zenith_Input.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DPCommonTypes.h"
#include "Source/DP_Tuning.h"
#include "Source/DPTutorial.h"
#include "Core/Zenith_AudioBus.h"
#include "Components/DPDoor_Component.h"
#include "Components/DPVillager_Component.h"
#include "Components/DPForge_Component.h"
#include "Components/DPPauseMenuController_Component.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"

namespace
{
	// Shared: read a packed-EntityID blackboard variable (stored by the
	// OnCustomEvent source from the interact payload).
	inline Zenith_EntityID DPGraph_GetEntityVar(Zenith_GraphContext& xContext, const std::string& strVar)
	{
		const Zenith_PropertyValue* pxValue = xContext.m_pxBlackboard->TryGetValue(strVar);
		if (!pxValue || pxValue->GetType() != PROPERTY_TYPE_ENTITY_ID)
		{
			return Zenith_EntityID();
		}
		return Zenith_EntityID::FromPacked(pxValue->GetPackedEntityID());
	}
}

//==============================================================================
// Pentagram
//==============================================================================

// DP_Win::Reset() on scene start - the DPPentagram_Component::OnAwake parity
// (win-state side-table re-inits so a replay starts clean).
class DPNode_ResetWinState : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext&) override
	{
		DP_Win::Reset();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPResetWinState"; }
};

// The pentagram interact: consume one held Objective[1..5] item; collecting
// the victory threshold fires DP_OnVictory (inside NotifyObjectiveCollected).
// Body is DPPentagram_Component::HandleInteract verbatim.
// W3 decomposition of the retired DPDepositHeldObjective mega-node into
// single-action nodes. The chain preserves the ordering invariants:
// gates (zero side effects) -> notify BEFORE consume -> consume ->
// DP_OnObjectivePlaced strictly after the item destroy.

// Gate + stage: villager payload valid AND holding an objective tag. Writes
// the tag to "heldObjective"; FAILURE (chain stops, no side effects)
// otherwise.
class DPNode_ReadHeldObjective : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_ReadHeldObjective)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")
	ZENITH_PROPERTY(std::string, m_strTagVar, "heldObjective")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		if (!xVillager.IsValid())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xVillager);
		if (!DP_IsObjectiveTag(eHeld)) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_PropertyValue xValue;
		xValue.SetInt32((int32_t)eHeld);
		xContext.m_pxBlackboard->SetValue(m_strTagVar, xValue);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPReadHeldObjective"; }
};

// Double-deposit guard: FAILURE when the objective's bit is already set
// (reads the mask BEFORE the notify so a redundant re-delivery has zero
// side effects).
class DPNode_WinCheckAlreadyCollected : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_WinCheckAlreadyCollected)
public:
	ZENITH_PROPERTY(std::string, m_strTagVar, "heldObjective")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const DP_ItemTag eHeld =
			(DP_ItemTag)xContext.m_pxBlackboard->GetInt32(m_strTagVar, (int32_t)DP_ItemTag::None);
		const uint32_t uBit = DP_ObjectiveTagToBit(eHeld);
		if (DP_Win::GetCollectedObjectivesMask() & uBit) return GRAPH_NODE_STATUS_FAILURE;
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPWinCheckAlreadyCollected"; }
};

// NOTIFY happens FIRST - inside, DP_Win sets the bit, banks the Knot, and
// may dispatch DP_OnVictory while the villager STILL HOLDS the item.
class DPNode_WinNotifyCollected : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_WinNotifyCollected)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")
	ZENITH_PROPERTY(std::string, m_strTagVar, "heldObjective")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		const DP_ItemTag eHeld =
			(DP_ItemTag)xContext.m_pxBlackboard->GetInt32(m_strTagVar, (int32_t)DP_ItemTag::None);
		DP_Win::NotifyObjectiveCollected(eHeld, xVillager, xContext.m_xSelf.GetEntityID());
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPWinNotifyCollected"; }
};

// Consume AFTER notify: clear the held-item table entry, then destroy the
// item entity.
class DPNode_ConsumeHeldItem : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_ConsumeHeldItem)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		Zenith_EntityID xItem = DP_Player::GetHeldItemEntity(xVillager);
		DP_Player::RemoveHeldItem(xVillager);
		if (xItem.IsValid())
		{
			Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xItem);
			if (xEnt.IsValid())
			{
				xEnt.Destroy();
			}
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPConsumeHeldItem"; }
};

// DP_OnObjectivePlaced LAST, strictly after the destroy.
class DPNode_DispatchObjectivePlaced : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_DispatchObjectivePlaced)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")
	ZENITH_PROPERTY(std::string, m_strTagVar, "heldObjective")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		const DP_ItemTag eHeld =
			(DP_ItemTag)xContext.m_pxBlackboard->GetInt32(m_strTagVar, (int32_t)DP_ItemTag::None);
		const int iBitIdx = static_cast<int>(eHeld) - static_cast<int>(DP_ItemTag::Objective1);
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnObjectivePlaced{ xVillager, xContext.m_xSelf.GetEntityID(), iBitIdx });
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDispatchObjectivePlaced"; }
};

//==============================================================================
// Noise machine
//==============================================================================

// Emits a sound stimulus at the entity's position - the
// DummyNoiseMachine_Component::HandleInteract body, with the same tuning keys.
class DPNode_EmitNoise : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_EmitNoise)
public:
	ZENITH_PROPERTY(std::string, m_strLoudnessKey, "interactables.noise_machine_loudness")
	ZENITH_PROPERTY(std::string, m_strRadiusKey, "interactables.noise_machine_radius_m")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Zenith_Maths::Vector3 xPos(0.0f);
		if (xContext.m_xSelf.IsValid())
		{
			if (Zenith_TransformComponent* pxTransform = xContext.m_xSelf.TryGetComponent<Zenith_TransformComponent>())
			{
				pxTransform->GetPosition(xPos);
			}
		}
		const float fLoudness = DP_Tuning::Get<float>(m_strLoudnessKey.c_str());
		const float fRadius   = DP_Tuning::Get<float>(m_strRadiusKey.c_str());
		DP_AI::EmitNoise(xPos, fLoudness, fRadius, xContext.m_xSelf.GetEntityID());
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPEmitNoise"; }
};

//==============================================================================
// Double door
//==============================================================================

// W3 decomposition of the retired DPTryOpenDoor mega-node: the isOpen guard
// + villager-valid gate become engine nodes in DP_DoubleDoor.bgraph; these
// two single-action nodes carry the key consume and the event dispatch.

// Key gate: consumes the matching key from the villager's hand (SkeletonKey
// matches without being consumed - DP_Items semantics); FAILURE when the
// villager holds no matching key.
class DPNode_ConsumeKeyForUnlock : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_ConsumeKeyForUnlock)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")
	ZENITH_PROPERTY(int32_t, m_iKeyTag, (int32_t)DP_ItemTag::Key)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		if (!xVillager.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		return DP_Items::TryConsumeKeyForUnlock(xVillager, (DP_ItemTag)m_iKeyTag)
			? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
	}
	const char* GetTypeName() const override { return "DPConsumeKeyForUnlock"; }
};

// DP_OnDoorOpened dispatch (shared by the double door and the single-leaf
// door's Closed->Opening chain).
class DPNode_DispatchDoorOpened : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_DispatchDoorOpened)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnDoorOpened{ DPGraph_GetEntityVar(xContext, m_strVillagerVar),
			                 xContext.m_xSelf.GetEntityID() });
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDispatchDoorOpened"; }
};

// DP_OnDoorClosed dispatch (single-leaf door's Open->Closing chain).
class DPNode_DispatchDoorClosed : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_DispatchDoorClosed)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnDoorClosed{ DPGraph_GetEntityVar(xContext, m_strVillagerVar),
			                 xContext.m_xSelf.GetEntityID() });
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDispatchDoorClosed"; }
};

// Per-frame double-door leaf animation - DPDoubleDoor_Component::OnUpdate +
// ApplyRotations + FindChildTransform verbatim, with openT on the blackboard.
class DPNode_AnimateDoorLeaves : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_AnimateDoorLeaves)
public:
	ZENITH_PROPERTY(std::string, m_strIsOpenVar, "isOpen")
	ZENITH_PROPERTY(std::string, m_strOpenTVar, "openT")
	ZENITH_PROPERTY(std::string, m_strYawKey, "interactables.double_door_open_yaw_deg")
	ZENITH_PROPERTY(std::string, m_strDurationKey, "interactables.double_door_open_duration_s")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_pxBlackboard->GetBool(m_strIsOpenVar)) return GRAPH_NODE_STATUS_SUCCESS;
		float fOpenT = xContext.m_pxBlackboard->GetFloat(m_strOpenTVar);
		if (fOpenT >= 1.0f) return GRAPH_NODE_STATUS_SUCCESS;

		const float fDuration = DP_Tuning::Get<float>(m_strDurationKey.c_str());
		fOpenT = glm::min(1.0f, fOpenT + xContext.m_fDt / fDuration);
		Zenith_PropertyValue xT;
		xT.SetFloat(fOpenT);
		xContext.m_pxBlackboard->SetValue(m_strOpenTVar, xT);

		const float fOpenYaw = DP_Tuning::Get<float>(m_strYawKey.c_str());
		Zenith_TransformComponent* pxLeft  = FindChildTransform(xContext, "Leaf_L");
		Zenith_TransformComponent* pxRight = FindChildTransform(xContext, "Leaf_R");
		const float fA = glm::radians(fOpenYaw * fOpenT);
		if (pxLeft != nullptr)  pxLeft->SetRotation(glm::angleAxis(+fA, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
		if (pxRight != nullptr) pxRight->SetRotation(glm::angleAxis(-fA, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPAnimateDoorLeaves"; }

private:
	static Zenith_TransformComponent* FindChildTransform(Zenith_GraphContext& xContext, const char* szName)
	{
		if (szName == nullptr || !xContext.m_xSelf.IsValid()) return nullptr;
		const Zenith_Vector<Zenith_EntityID>& xChildren = xContext.m_xSelf.GetChildEntityIDs();
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xContext.m_xSelf.GetEntityID());
		if (pxScene == nullptr) return nullptr;
		for (u_int u = 0; u < xChildren.GetSize(); ++u)
		{
			Zenith_Entity xChild = pxScene->TryGetEntity(xChildren.Get(u));
			if (!xChild.IsValid()) continue;
			if (xChild.GetName() != szName) continue;
			Zenith_TransformComponent* pxChildTransform = xChild.TryGetComponent<Zenith_TransformComponent>();
			if (pxChildTransform == nullptr) continue;
			return pxChildTransform;
		}
		return nullptr;
	}
};

//==============================================================================
// Chest
//==============================================================================

// W3 decomposition of the retired DPOpenChest / DPAdvanceChestLid: the open
// guard is an engine Branch, the lid math is engine Math/Add nodes over a
// DPReadTuningFloat-staged duration; only the event dispatch stays a game
// node. QUIRK preserved by the graph: the chest opens (and dispatches) even
// on an INVALID interact payload.
class DPNode_DispatchChestOpened : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_DispatchChestOpened)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnChestOpened{ DPGraph_GetEntityVar(xContext, m_strVillagerVar),
			                  xContext.m_xSelf.GetEntityID() });
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDispatchChestOpened"; }
};

//==============================================================================
// Single-leaf door (wave 2) - decisions here; systems on DPDoor_Component
//==============================================================================

// W3 decomposition of the retired DPDoorHandleInteract mega-node. The
// SwitchOnInt(anim) anchor + the SetBlackboardInt anim writes live in
// DP_Door.bgraph; these single-action nodes preserve the ordering
// invariants: key check before ANY state change; sticky unlock (blackboard
// write + immediate RefreshLockTint) before the anim write; the anim write
// before OnDoorStateChanged (which reads GetAnim() off the blackboard);
// state-change before EmitNoise before event dispatch.

// Closed-state key gate. requiredKey == None -> SUCCESS (skip). Otherwise
// consume-or-reject with the PRIEST-PROBE SPAM GUARD: only the possessed
// villager surfaces DP_OnDoorLockRejected (the priest probes doors every
// frame via DP_AI::OpenNearbyDoorsFor - silent FAILURE for it). On unlock:
// STICKY requiredKey=None + immediate red->green re-tint.
class DPNode_DoorCheckKey : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_DoorCheckKey)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")
	ZENITH_PROPERTY(std::string, m_strRequiredKeyVar, "requiredKey")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		if (!xVillager.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		DPDoor_Component* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<DPDoor_Component>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		const DP_ItemTag eRequiredKey = static_cast<DP_ItemTag>(
			xContext.m_pxBlackboard->GetInt32(m_strRequiredKeyVar, static_cast<int32_t>(DP_ItemTag::None)));
		if (eRequiredKey == DP_ItemTag::None)
		{
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		if (!DP_Items::TryConsumeKeyForUnlock(xVillager, eRequiredKey))
		{
			if (xVillager != DP_Player::GetPossessedVillager()) { return GRAPH_NODE_STATUS_FAILURE; }
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnDoorLockRejected{ xVillager, xContext.m_xSelf.GetEntityID(), eRequiredKey });
			return GRAPH_NODE_STATUS_FAILURE;
		}
		Zenith_PropertyValue xNone;
		xNone.SetInt32(static_cast<int32_t>(DP_ItemTag::None));
		xContext.m_pxBlackboard->SetValue(m_strRequiredKeyVar, xNone);
		pxShim->RefreshLockTint();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDoorCheckKey"; }
};

// Open-state close-deferral: FAILURE when the F-press is ALSO hitting a
// Pentagram in range (the player's intent is to DELIVER).
class DPNode_DoorPentagramDeferral : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_DoorPentagramDeferral)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		if (!xVillager.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		return DP_Win::IsPentagramInRange(xVillager)
			? GRAPH_NODE_STATUS_FAILURE : GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDoorPentagramDeferral"; }
};

// Synchronous shim state-change: navmesh block/unblock + collider solidity,
// reading the JUST-written anim off the blackboard (no 1-frame race).
class DPNode_DoorStateChanged : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		DPDoor_Component* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<DPDoor_Component>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->OnDoorStateChanged();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDoorStateChanged"; }
};

// The door's audio cue at its logical centre, tuning read LIVE per press.
class DPNode_DoorEmitNoise : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_DoorEmitNoise)
public:
	ZENITH_PROPERTY(std::string, m_strLoudnessKey, "interactables.door_audible_loudness")
	ZENITH_PROPERTY(std::string, m_strRadiusKey, "interactables.door_audible_at_m")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		DPDoor_Component* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<DPDoor_Component>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		DP_AI::EmitNoise(pxShim->GetInteractionCentre(),
			DP_Tuning::Get<float>(m_strLoudnessKey.c_str()),
			DP_Tuning::Get<float>(m_strRadiusKey.c_str()),
			xContext.m_xSelf.GetEntityID());
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDoorEmitNoise"; }
};

// Per-frame door animation: advances openT toward the directional target and
// applies the swing rotation through the shim; settles Opening -> Open and
// Closing -> Closed (with the navmesh/collider sync at the Closed settle).
// Body is the pre-graph DPDoor_Component::OnUpdate animation block verbatim.
class DPNode_DoorAdvanceAnim : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_DoorAdvanceAnim)
public:
	ZENITH_PROPERTY(std::string, m_strAnimVar, "anim")
	ZENITH_PROPERTY(std::string, m_strOpenTVar, "openT")
	ZENITH_PROPERTY(std::string, m_strDurationKey, "interactables.door_open_duration_s")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		DPDoor_Component* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<DPDoor_Component>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		const int32_t iAnim = xContext.m_pxBlackboard->GetInt32(m_strAnimVar, 0);
		float fOpenT = xContext.m_pxBlackboard->GetFloat(m_strOpenTVar);

		if (iAnim == static_cast<int32_t>(DPDoor_Component::DoorAnim::Opening))
		{
			const float fDuration = DP_Tuning::Get<float>(m_strDurationKey.c_str());
			fOpenT = glm::min(1.0f, fOpenT + xContext.m_fDt / fDuration);
			StoreT(xContext, fOpenT);
			pxShim->ApplyRotationFromT(fOpenT);
			if (fOpenT >= 1.0f)
			{
				SetAnim(xContext, DPDoor_Component::DoorAnim::Open);
				// Steady-state navmesh sync (already unblocked at the
				// Closed -> Opening transition; idempotent but explicit).
				pxShim->OnDoorStateChanged();
			}
		}
		else if (iAnim == static_cast<int32_t>(DPDoor_Component::DoorAnim::Closing))
		{
			const float fDuration = DP_Tuning::Get<float>(m_strDurationKey.c_str());
			fOpenT = glm::max(0.0f, fOpenT - xContext.m_fDt / fDuration);
			StoreT(xContext, fOpenT);
			pxShim->ApplyRotationFromT(fOpenT);
			if (fOpenT <= 0.0f)
			{
				SetAnim(xContext, DPDoor_Component::DoorAnim::Closed);
				// Restore navmesh block + physical solidity now the door has
				// fully closed -- the player capsule bumps into it again.
				pxShim->OnDoorStateChanged();
			}
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDoorAdvanceAnim"; }

private:
	static void SetAnim(Zenith_GraphContext& xContext, DPDoor_Component::DoorAnim eAnim)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(static_cast<int32_t>(eAnim));
		xContext.m_pxBlackboard->SetValue("anim", xValue);
	}
	void StoreT(Zenith_GraphContext& xContext, float fOpenT)
	{
		Zenith_PropertyValue xValue;
		xValue.SetFloat(fOpenT);
		xContext.m_pxBlackboard->SetValue(m_strOpenTVar, xValue);
	}
};

//==============================================================================
// Front-end menu
//==============================================================================

// The Quit click handler - DPMainMenuController_Component::OnQuitClicked
// verbatim (test builds flip an observable flag; production termination is
// the same post-MVP TODO it was in C++).
class DPNode_RequestQuit : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext&) override
	{
#ifdef ZENITH_INPUT_SIMULATOR
		s_bQuitRequestedForTest = true;
#endif
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPRequestQuit"; }

#ifdef ZENITH_INPUT_SIMULATOR
	static bool WasQuitRequestedForTest() { return s_bQuitRequestedForTest; }
	static void ResetQuitForTest() { s_bQuitRequestedForTest = false; }
private:
	static inline bool s_bQuitRequestedForTest = false;
#endif
};

//==============================================================================
// Registration (called from Project_RegisterGameComponents)
//==============================================================================

//==============================================================================
// Villager (W3 conversion - decisions live on DP_Villager.bgraph; these nodes
// are the graph-facing verbs whose bodies are the retired C++ verbatim)
//==============================================================================

// Kill() forwarder. The death sequence (idempotency guard, Dead-before-
// unpossess ordering, DP_OnVillagerDied dispatch, conditional possession
// clear, NoVessels scan) is ONE atomic C++ sequence on the shim - tests call
// Kill() directly (Test_P1NoVessels), so the body stays there and the graph's
// life-depleted chain routes through the same method.
class DPNode_VillagerKill : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		DPVillager_Component* pxVillager = xContext.m_xSelf.TryGetComponent<DPVillager_Component>();
		if (pxVillager == nullptr)
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		pxVillager->Kill();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPVillagerKill"; }
};

// One footstep: AudioBus emit (test recorder / future audio) + perception
// stimulus (the priest-hearing path), at the villager's position. Loudness =
// footstepLoudness, multiplied by quietLoudnessMult while walk-quiet is
// active (sprint runs FULL loudness - no multiplier, MVP-1.7.5). The cadence
// decision (countdown/interval/first-step-immediate) lives on the graph; this
// node is the single-action emission verb.
class DPNode_VillagerEmitFootstep : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_VillagerEmitFootstep)
public:
	ZENITH_PROPERTY(std::string, m_strWalkQuietVar, "walkQuiet")
	ZENITH_PROPERTY(std::string, m_strLoudnessVar, "footstepLoudness")
	ZENITH_PROPERTY(std::string, m_strRadiusVar, "footstepRadius")
	ZENITH_PROPERTY(std::string, m_strQuietMultVar, "quietLoudnessMult")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		float fLoudness = xContext.m_pxBlackboard->GetFloat(m_strLoudnessVar);
		const float fRadius = xContext.m_pxBlackboard->GetFloat(m_strRadiusVar);
		if (xContext.m_pxBlackboard->GetBool(m_strWalkQuietVar, false))
		{
			fLoudness *= xContext.m_pxBlackboard->GetFloat(m_strQuietMultVar, 1.0f);
		}

		Zenith_Maths::Vector3 xPos(0.0f);
		if (xContext.m_xSelf.IsValid())
		{
			if (Zenith_TransformComponent* pxTransform = xContext.m_xSelf.TryGetComponent<Zenith_TransformComponent>())
			{
				pxTransform->GetPosition(xPos);
			}
		}

		// Same dual emission, same order, same name string as the retired
		// TickFootsteps body ("DP.Villager.Footstep" is a test-recorder
		// filter key - preserve byte-identically).
		Zenith_AudioBus::EmitSound("DP.Villager.Footstep", xPos, fLoudness, fRadius);
		Zenith_PerceptionSystem::EmitSoundStimulus(
			xPos, fLoudness, fRadius, xContext.m_xSelf.GetEntityID());
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPVillagerEmitFootstep"; }
};

// First-encounter tutorial ping. DP_Tutorial::TriggerIfFirstTime latches
// internally, so firing every qualifying frame (like the retired OnUpdate
// did for sprint/walk-quiet) is correct.
class DPNode_VillagerTutorialPing : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_VillagerTutorialPing)
public:
	ZENITH_PROPERTY(int32_t, m_iKind, 0)

	GraphNodeStatus Execute(Zenith_GraphContext&) override
	{
		DP_Tutorial::TriggerIfFirstTime(static_cast<DP_Tutorial::Kind>(m_iKind));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPVillagerTutorialPing"; }
};

//==============================================================================
// Player control (W3 conversion - input dispatch decisions on
// DP_PlayerControl.bgraph; the controller shim stages clickPressed/dropPressed
// and fires "PlayerTick" where the retired handler calls sat)
//==============================================================================

namespace
{
	// Screen-space pick state shared with the function-pointer callback
	// (relocated VERBATIM from DPPlayerController_Component).
	struct DPGraph_PickContext
	{
		Zenith_Maths::Matrix4    m_xVP;
		int32_t                  m_iW = 0;
		int32_t                  m_iH = 0;
		Zenith_Maths::Vector2_64 m_xMousePos;
		double                   m_fBestSq = 0.0;
		Zenith_EntityID          m_xBest;
	};

	// Per-villager screen-space projection + nearest-to-cursor pick.
	inline void DPGraph_PickClosestVillagerCb(Zenith_EntityID xId, void* pUser)
	{
		DPGraph_PickContext& xCtx = *static_cast<DPGraph_PickContext*>(pUser);
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xId);
		if (!xEnt.IsValid()) return;
		Zenith_TransformComponent* pxTransform = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr) return;
		Zenith_Maths::Vector3 xWorld;
		pxTransform->GetPosition(xWorld);
		// Aim at body centre rather than feet — the visible silhouette
		// is centred ~1 m above the entity origin.
		xWorld.y += 1.0f;
		const Zenith_Maths::Vector4 xClip = xCtx.m_xVP *
			Zenith_Maths::Vector4(xWorld.x, xWorld.y, xWorld.z, 1.0f);
		if (xClip.w <= 1e-4f) return;
		const float fNdcX = xClip.x / xClip.w;
		const float fNdcY = xClip.y / xClip.w;
		const double fSx = (fNdcX + 1.0f) * 0.5f * static_cast<float>(xCtx.m_iW);
		const double fSy = (fNdcY + 1.0f) * 0.5f * static_cast<float>(xCtx.m_iH);
		const double fDx = fSx - xCtx.m_xMousePos.x;
		const double fDy = fSy - xCtx.m_xMousePos.y;
		const double fSq = fDx * fDx + fDy * fDy;
		if (fSq < xCtx.m_fBestSq) { xCtx.m_fBestSq = fSq; xCtx.m_xBest = xId; }
	}
}

// Screen-space villager pick (the retired HandleClickToPossess body minus the
// input edge and the possess call): closest-to-cursor within 120px, aimed at
// body centre. Writes the picked villager's packed EntityID to the result
// var; no pick / no camera / no window -> FAILURE (chain stops, silently -
// misclick feedback is a future-MVP polish item, as before).
class DPNode_PickVillagerUnderCursor : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_PickVillagerUnderCursor)
public:
	ZENITH_PROPERTY(std::string, m_strResultVar, "clicked")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
		if (pxCam == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		// Screen-space proximity over a physics raycast: wall colliders sit
		// between the orbit camera and villagers standing inside buildings,
		// so a raycast catches the wall (see the retired handler's rationale).
		Zenith_Window* pxWindow = Zenith_Window::GetInstance();
		if (pxWindow == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		int32_t iW = 0, iH = 0;
		pxWindow->GetSize(iW, iH);
		if (iW <= 0 || iH <= 0) return GRAPH_NODE_STATUS_FAILURE;

		Zenith_Maths::Vector2_64 xMousePos;
		g_xEngine.Input().GetMousePosition(xMousePos);

		Zenith_Maths::Matrix4 xView, xProj;
		pxCam->BuildViewMatrix(xView);
		pxCam->BuildProjectionMatrix(xProj);

		constexpr double kMaxPixelDistSq = 120.0 * 120.0;
		DPGraph_PickContext xCtx;
		xCtx.m_xVP       = xProj * xView;
		xCtx.m_iW        = iW;
		xCtx.m_iH        = iH;
		xCtx.m_xMousePos = xMousePos;
		xCtx.m_fBestSq   = kMaxPixelDistSq;
		DP_Player::ForEachVillagerInActiveScene(&DPGraph_PickClosestVillagerCb, &xCtx);

		if (!xCtx.m_xBest.IsValid())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		Zenith_PropertyValue xValue;
		xValue.SetPackedEntityID(xCtx.m_xBest.GetPacked());
		xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPPickVillagerUnderCursor"; }
};

// The voluntary-switch entry point. The gate chain (idempotent re-click /
// channel / cooldown / possessable / anchor range / Devout channel dispatch)
// stays DP_Player C++ - it reads/writes the controller's friended per-run
// state block consumed by ~40 tests through the namespace surface. Refusal
// is SILENT (returns SUCCESS either way), like the retired handler.
class DPNode_TryPossess : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_TryPossess)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "clicked")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		if (!xVillager.IsValid())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		DP_Player::TryVoluntaryPossessSwitch(xVillager);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPTryPossess"; }
};

// The retired HandleDropItem body VERBATIM (minus the input edge): drop the
// possessed villager's held item at the villager's FOOT position (transform
// origin - the pick aims at +1m, the drop does not), arm the item's post-drop
// cooldown BEFORE the table clear.
class DPNode_DropHeldItem : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext&) override
	{
		const Zenith_EntityID xVillager = DP_Player::GetPossessedVillager();
		if (!xVillager.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		const Zenith_EntityID xItem = DP_Player::GetHeldItemEntity(xVillager);
		if (!xItem.IsValid()) return GRAPH_NODE_STATUS_FAILURE;

		Zenith_Entity xV = g_xEngine.Scenes().ResolveEntity(xVillager);
		if (!xV.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_TransformComponent* pxVTransform = xV.TryGetComponent<Zenith_TransformComponent>();
		if (pxVTransform == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_Maths::Vector3 xFootPos;
		pxVTransform->GetPosition(xFootPos);

		Zenith_Entity xI = g_xEngine.Scenes().ResolveEntity(xItem);
		if (!xI.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_TransformComponent* pxITransform = xI.TryGetComponent<Zenith_TransformComponent>();
		if (pxITransform == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxITransform->SetPosition(xFootPos);

		// Cooldown armed BEFORE the table clear (preserve order).
		DP_Items::BeginPostDropCooldownForItem(xItem);
		DP_Player::RemoveHeldItem(xVillager);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDropHeldItem"; }
};

//==============================================================================
// Forge (W3 conversion)
//==============================================================================

// The retired DPForge_Component::HandleInteract VERBATIM: recipe gate,
// consume-input, spawn+auto-equip (spawn is the shim's systems body), craft
// count (increments EVEN when the spawn fails - preserve), live tuning reads,
// AudioBus -> PerceptionSystem -> DP_OnForgeCrafted order. Recipe tags +
// craft count live on the blackboard (SetRecipe/GetCraftCount read them).
class DPNode_ForgeCraft : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_ForgeCraft)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")
	ZENITH_PROPERTY(std::string, m_strRecipeInputVar, "recipeInput")
	ZENITH_PROPERTY(std::string, m_strRecipeOutputVar, "recipeOutput")
	ZENITH_PROPERTY(std::string, m_strCraftCountVar, "craftCount")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		DPForge_Component* pxShim = xContext.m_xSelf.TryGetComponent<DPForge_Component>();
		if (pxShim == nullptr)
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}

		// NOTE: no villager-validity gate - the retired code had none. An
		// invalid villager reads held tag None, mismatches the recipe and
		// falls out silently, exactly like before.
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xVillager);
		const DP_ItemTag eRecipeInput =
			(DP_ItemTag)xContext.m_pxBlackboard->GetInt32(m_strRecipeInputVar, (int32_t)DP_ItemTag::Iron);
		if (eHeld != eRecipeInput)
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}

		// 1. Consume input (destroy entity AND clear the held-item table).
		Zenith_EntityID xInput = DP_Player::GetHeldItemEntity(xVillager);
		DP_Player::RemoveHeldItem(xVillager);
		if (xInput.IsValid())
		{
			Zenith_Entity xInputEnt = g_xEngine.Scenes().ResolveEntity(xInput);
			if (xInputEnt.IsValid()) xInputEnt.Destroy();
		}

		// 2. Spawn output at the forge + auto-equip. The name uses the
		// PRE-increment count; the count bumps even on spawn failure.
		const DP_ItemTag eRecipeOutput =
			(DP_ItemTag)xContext.m_pxBlackboard->GetInt32(m_strRecipeOutputVar, (int32_t)DP_ItemTag::Key);
		const int32_t iCraftCount = xContext.m_pxBlackboard->GetInt32(m_strCraftCountVar, 0);
		Zenith_EntityID xOutput = pxShim->SpawnOutputItem(eRecipeOutput, (uint32_t)iCraftCount);
		if (xOutput.IsValid())
		{
			DP_Player::SetHeldItem(xVillager, xOutput);
		}
		{
			Zenith_PropertyValue xCount;
			xCount.SetInt32(iCraftCount + 1);
			xContext.m_pxBlackboard->SetValue(m_strCraftCountVar, xCount);
		}

		// 3. Hammer sound: AudioBus (test recorder) then PerceptionSystem
		// (the actual priest-hearing path), tuning read LIVE per craft.
		Zenith_Maths::Vector3 xForgePos(0.0f);
		if (Zenith_TransformComponent* pxTransform = xContext.m_xSelf.TryGetComponent<Zenith_TransformComponent>())
		{
			pxTransform->GetPosition(xForgePos);
		}
		const float fAudibleRadius = DP_Tuning::Get<float>("interactables.forge_audible_at_m");
		const float fAudibleLoudness = DP_Tuning::Get<float>("interactables.forge_audible_loudness");
		Zenith_AudioBus::EmitSound("DP.Forge.Hammer", xForgePos, fAudibleLoudness, fAudibleRadius);
		Zenith_PerceptionSystem::EmitSoundStimulus(
			xForgePos, fAudibleLoudness, fAudibleRadius, xContext.m_xSelf.GetEntityID());

		// 4. Analytics event LAST (output may be INVALID - consumers guard).
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnForgeCrafted{ xVillager, xContext.m_xSelf.GetEntityID(), xOutput });
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPForgeCraft"; }
};

// Reads a DP_Tuning float LIVE into a blackboard variable - the graph-side
// twin of the "cold keys can be read every frame" convention. Used where the
// retired C++ read tuning at the moment of a decision (faint-recovery arm,
// chest/door animation durations).
class DPNode_ReadTuningFloat : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_ReadTuningFloat)
public:
	ZENITH_PROPERTY(std::string, m_strKey, "")
	ZENITH_PROPERTY(std::string, m_strVar, "value")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (m_strKey.empty() || m_strVar.empty())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		Zenith_PropertyValue xValue;
		xValue.SetFloat(DP_Tuning::Get<float>(m_strKey.c_str()));
		xContext.m_pxBlackboard->SetValue(m_strVar, xValue);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPReadTuningFloat"; }
};

//==============================================================================
// Items (W3 conversion - the pickup decision chain lives on DP_Item.bgraph;
// the DPItemBase shim stages possessedValid/handsEmpty/inRange/
// possessedVillager and fires "ItemTick" each frame)
//==============================================================================

// Child-archetype tool gate: FAILURE + the every-frame refusal telegraph
// (no rising-edge latch - preserve) when the possessed villager is a Child
// holding out for a tool-tagged item; SUCCESS otherwise.
class DPNode_ItemChildRefusal : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_ItemChildRefusal)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "possessedVillager")
	ZENITH_PROPERTY(std::string, m_strTagVar, "tag")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		const DP_ItemTag eTag =
			(DP_ItemTag)xContext.m_pxBlackboard->GetInt32(m_strTagVar, (int32_t)DP_ItemTag::None);
		if (!DP_Player::IsChildVillagerWithToolTag(xVillager, eTag))
		{
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		// Telegraph at the VILLAGER position (retired payload semantics).
		Zenith_Maths::Vector3 xVPos(0.0f);
		Zenith_Entity xV = g_xEngine.Scenes().ResolveEntity(xVillager);
		if (xV.IsValid())
		{
			if (Zenith_TransformComponent* pxTransform = xV.TryGetComponent<Zenith_TransformComponent>())
			{
				pxTransform->GetPosition(xVPos);
			}
		}
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnChildToolRefused{ xVillager, xContext.m_xSelf.GetEntityID(), eTag, xVPos });
		return GRAPH_NODE_STATUS_FAILURE;
	}
	const char* GetTypeName() const override { return "DPItemChildRefusal"; }
};

// Arms a fresh reagent pickup channel with the current villager. The arming
// frame does NOT tick dt (start-frame-free quirk) - the chain ends here.
class DPNode_ItemArmChannel : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_ItemArmChannel)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "possessedVillager")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Zenith_PropertyValue xValue;
		xValue.SetPackedEntityID(
			xContext.m_pxBlackboard->GetPackedEntityID(m_strVillagerVar, 0));
		xContext.m_pxBlackboard->SetValue("channelVillager", xValue);
		xValue.SetFloat(xContext.m_pxBlackboard->GetFloat("channelDuration", 0.0f));
		xContext.m_pxBlackboard->SetValue("channelRemaining", xValue);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPItemArmChannel"; }
};

// Pickup commit: clear channel state -> SetHeldItem -> DP_OnItemPickedUp
// (the retired fall-through order).
class DPNode_ItemCommitPickup : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_ItemCommitPickup)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "possessedVillager")
	ZENITH_PROPERTY(std::string, m_strTagVar, "tag")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		if (!xVillager.IsValid())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		Zenith_PropertyValue xValue;
		xValue.SetPackedEntityID(0);
		xContext.m_pxBlackboard->SetValue("channelVillager", xValue);
		xValue.SetFloat(0.0f);
		xContext.m_pxBlackboard->SetValue("channelRemaining", xValue);

		DP_Player::SetHeldItem(xVillager, xContext.m_xSelf.GetEntityID());
		const DP_ItemTag eTag =
			(DP_ItemTag)xContext.m_pxBlackboard->GetInt32(m_strTagVar, (int32_t)DP_ItemTag::None);
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnItemPickedUp{ xVillager, xContext.m_xSelf.GetEntityID(), eTag });
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPItemCommitPickup"; }
};

// BellSoul special: guarded on specialBehaviour == "rings_bell_on_pickup";
// exact retired order = DP_OnBellRing -> EmitSoundStimulus(1.0, 200.0) ->
// NotifyAllPriestsOfInvestigatePos, all at the ITEM's position.
class DPNode_ItemRingBell : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_ItemRingBell)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "possessedVillager")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (xContext.m_pxBlackboard->GetString("specialBehaviour", "") != "rings_bell_on_pickup")
		{
			return GRAPH_NODE_STATUS_SUCCESS;	// non-bell items: silent no-op
		}
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		const Zenith_Maths::Vector3 xMyPos =
			DP_Items::GetItemWorldPos(xContext.m_xSelf.GetEntityID());
		DP_OnBellRing xEvt;
		xEvt.m_xVillager = xVillager;
		xEvt.m_xBellSoul = xContext.m_xSelf.GetEntityID();
		xEvt.m_xPosition = xMyPos;
		Zenith_EventDispatcher::Get().Dispatch(xEvt);
		Zenith_PerceptionSystem::EmitSoundStimulus(
			xMyPos, 1.0f, 200.0f, xContext.m_xSelf.GetEntityID());
		DP_AI::NotifyAllPriestsOfInvestigatePos(xMyPos);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPItemRingBell"; }
};

// Evaporate zero-crossing: capture position BEFORE the destroy, dispatch
// DP_OnItemEvaporated, then Destroy self (deferred to end of frame).
class DPNode_ItemEvaporate : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_ItemEvaporate)
public:
	ZENITH_PROPERTY(std::string, m_strTagVar, "tag")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xContext.m_xSelf.GetEntityID());
		if (!xEnt.IsValid())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		const Zenith_Maths::Vector3 xPos =
			DP_Items::GetItemWorldPos(xContext.m_xSelf.GetEntityID());
		const DP_ItemTag eTag =
			(DP_ItemTag)xContext.m_pxBlackboard->GetInt32(m_strTagVar, (int32_t)DP_ItemTag::None);
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnItemEvaporated{ xContext.m_xSelf.GetEntityID(), eTag, xPos });
		xEnt.Destroy();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPItemEvaporate"; }
};

//==============================================================================
// Pause menu (W3 conversion, risk R6 - the graph rides the DontDestroyOnLoad
// relocation; the shim keeps the singleton machinery + verbs)
//==============================================================================

// Gate: SUCCESS iff the pause overlay UI exists on self - the retired
// toggle's "no UI -> Esc does nothing at all" quirk.
class DPNode_PauseCanToggle : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		DPPauseMenuController_Component* pxShim =
			xContext.m_xSelf.TryGetComponent<DPPauseMenuController_Component>();
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		return pxShim->CanToggleOverlay()
			? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
	}
	const char* GetTypeName() const override { return "DPPauseCanToggle"; }
};

// Applies the already-flipped blackboard "shown": overlay visibility +
// guarded SetScenePaused + DP_OnPauseToggle, in the retired inline order.
class DPNode_PauseApplyToggle : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		DPPauseMenuController_Component* pxShim =
			xContext.m_xSelf.TryGetComponent<DPPauseMenuController_Component>();
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->ApplyToggleFromGraph();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPPauseApplyToggle"; }
};

// R while (shown || runOver): flag-then-reset-then-reload, the same body the
// static test hook drives.
class DPNode_PauseRestart : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		DPPauseMenuController_Component* pxShim =
			xContext.m_xSelf.TryGetComponent<DPPauseMenuController_Component>();
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->HandleRestart();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPPauseRestart"; }
};

// Q while (shown || runOver): quit to the FrontEnd menu.
class DPNode_PauseQuit : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		DPPauseMenuController_Component* pxShim =
			xContext.m_xSelf.TryGetComponent<DPPauseMenuController_Component>();
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->HandleQuit();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPPauseQuit"; }
};

//==============================================================================
// Priest (W3 conversion, risk R3 - the BT decision body is DP_Priest.bgraph's
// reactive Selector; these two nodes carry the DP-specific leaves VERBATIM.
// The perception bridge stays C++ (Priest_Component) and writes THIS graph's
// blackboard under the same DP_AI::BB_KEY_* names.)
//==============================================================================

// The retired DP_BTAction_FindPosInSuspicionSphere body verbatim: suspicion-
// sphere random reachable point, recentred on the high-scent villager above
// the hound-bark threshold (scent diversion suppresses patrol sampling), and
// the balance-ratified 1-in-5 patrol-node round-robin (30 m sphere). Navmesh
// resolved LIVE per Execute (replaces the BT node's SetNavMesh seam).
class DPNode_PriestPickPatrolTarget : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_NavMesh* pxNavMesh = DP_AI::GetOrBuildLevelNavMesh();
		if (pxNavMesh == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_TransformComponent* pxAgentTransform =
			xContext.m_xSelf.TryGetComponent<Zenith_TransformComponent>();
		if (pxAgentTransform == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		Zenith_Maths::Vector3 xAgentPos;
		pxAgentTransform->GetPosition(xAgentPos);

		Zenith_Maths::Vector3 xCenter = xAgentPos;
		const float fRadius = xContext.m_pxBlackboard->GetFloat(DP_AI::BB_KEY_SUSPICION_RADIUS, 15.0f);
		bool bScentDiversion = false;
		const Zenith_EntityID xScentTarget = Zenith_EntityID::FromPacked(
			xContext.m_pxBlackboard->GetPackedEntityID(DP_AI::BB_KEY_HIGH_SCENT_TARGET, 0));
		if (xScentTarget.IsValid())
		{
			const float fScent = DP_Player::GetDemonScent(xScentTarget);
			const float fThreshold =
				DP_Tuning::Get<float>("possession.demon_scent_hound_bark_threshold");
			if (fScent >= fThreshold)
			{
				Zenith_Entity xTgt = g_xEngine.Scenes().ResolveEntity(xScentTarget);
				if (xTgt.IsValid())
				{
					if (Zenith_TransformComponent* pxTransform = xTgt.TryGetComponent<Zenith_TransformComponent>())
					{
						pxTransform->GetPosition(xCenter);
						bScentDiversion = true;
					}
				}
			}
		}

		// 1-in-5 patrol-node round-robin, suppressed by scent diversion.
		// Cadence is balance-ratified (2026-05-25/26: "every cycle" and
		// "every 4th" both dropped matrix wins 70%+ -> 0%) - preserve.
		m_uFindPosTick++;
		const Zenith_Vector<Zenith_Maths::Vector3>& axPatrolNodes = DP_AI::GetPatrolNodes();
		const uint32_t uPatrolN = axPatrolNodes.GetSize();
		Zenith_Maths::Vector3 xResult;
		bool bGot = false;
		if (uPatrolN > 0 && (m_uFindPosTick % 5u) == 0u && !bScentDiversion)
		{
			const uint32_t uIdx = (m_uFindPosTick / 5u) % uPatrolN;
			if (pxNavMesh->GetRandomReachablePointInRadius(axPatrolNodes.Get(uIdx), 30.0f, xResult))
			{
				bGot = true;
			}
		}
		if (!bGot)
		{
			if (!pxNavMesh->GetRandomReachablePointInRadius(xCenter, fRadius, xResult))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
		}
		Zenith_PropertyValue xValue;
		xValue.SetVector3(xResult);
		xContext.m_pxBlackboard->SetValue(DP_AI::BB_KEY_PATROL_TARGET, xValue);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPPriestPickPatrolTarget"; }

private:
	uint32_t m_uFindPosTick = 0u;
};

// The retired DP_BTAction_Apprehend body verbatim (the Selector's pin-0
// branch; self-gating on target validity like the BT's HasTarget+Apprehend
// sequence). XZ-only range; LIVE tuning in OnEnter; keeps pursuing while
// channelling (SetDestination every Execute); event order on completion =
// ChannelComplete THEN RunLost THEN SetPossessedVillager(INVALID);
// Interrupted emitted from every FAILURE path iff Start was emitted.
// OnAbort resets WITHOUT emitting - BT Reset() never called leaf OnAbort
// (composite OnAbort was a no-op), so an aborted channel stays silent and
// the next entry re-runs OnEnter fresh.
class DPNode_PriestApprehendChannel : public Zenith_GraphNode
{
public:
	void OnEnter(Zenith_GraphContext&) override
	{
		m_fAppliedChannelSeconds = DP_Tuning::Get<float>("priest.apprehend_channel_s");
		m_fAppliedRangeMetres    = DP_Tuning::Get<float>("priest.apprehend_range_m");
		m_fChannelElapsed        = 0.0f;
		m_xChannelTarget         = INVALID_ENTITY_ID;
		m_bChannelStartEmitted   = false;
	}

	void OnAbort(Zenith_GraphContext&) override
	{
		m_fChannelElapsed      = 0.0f;
		m_xChannelTarget       = INVALID_ENTITY_ID;
		m_bChannelStartEmitted = false;
	}

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		const Zenith_EntityID xPriestID = xContext.m_xSelf.GetEntityID();
		const Zenith_EntityID xTarget = Zenith_EntityID::FromPacked(
			xContext.m_pxBlackboard->GetPackedEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL, 0));
		if (!xTarget.IsValid())
		{
			EmitInterruptedIfRunning(xPriestID, DP_ApprehendInterruptReason::TargetLost);
			return GRAPH_NODE_STATUS_FAILURE;
		}

		Zenith_Entity xTargetEnt = g_xEngine.Scenes().ResolveEntity(xTarget);
		Zenith_TransformComponent* pxTargetTransform =
			xTargetEnt.IsValid() ? xTargetEnt.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		Zenith_TransformComponent* pxAgentTransform =
			xContext.m_xSelf.TryGetComponent<Zenith_TransformComponent>();
		if (pxTargetTransform == nullptr || pxAgentTransform == nullptr)
		{
			EmitInterruptedIfRunning(xPriestID, DP_ApprehendInterruptReason::TargetLost);
			return GRAPH_NODE_STATUS_FAILURE;
		}

		Zenith_Maths::Vector3 xTargetPos, xAgentPos;
		pxTargetTransform->GetPosition(xTargetPos);
		pxAgentTransform->GetPosition(xAgentPos);

		// Horizontal distance only (capsule-settle Y differences must not
		// reject the channel).
		const float fDx = xTargetPos.x - xAgentPos.x;
		const float fDz = xTargetPos.z - xAgentPos.z;
		const float fDist = std::sqrt(fDx * fDx + fDz * fDz);
		if (fDist > m_fAppliedRangeMetres)
		{
			EmitInterruptedIfRunning(xPriestID, DP_ApprehendInterruptReason::OutOfRange);
			return GRAPH_NODE_STATUS_FAILURE;
		}

		if (!m_xChannelTarget.IsValid())
		{
			m_xChannelTarget = xTarget;
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnApprehendChannelStart{ xPriestID, xTarget });
			m_bChannelStartEmitted = true;
		}
		else if (m_xChannelTarget.m_uIndex != xTarget.m_uIndex
		      || m_xChannelTarget.m_uGeneration != xTarget.m_uGeneration)
		{
			EmitInterruptedIfRunning(xPriestID, DP_ApprehendInterruptReason::TargetSwitched);
			return GRAPH_NODE_STATUS_FAILURE;
		}

		// Keep pursuing while channelling (walkers get caught; sprinters
		// escape via OutOfRange - the ratified catch model).
		if (Zenith_AIAgentComponent* pxAI = xContext.m_xSelf.TryGetComponent<Zenith_AIAgentComponent>())
		{
			if (Zenith_NavMeshAgent* pxNav = pxAI->GetNavMeshAgent())
			{
				pxNav->SetDestination(xTargetPos);
			}
		}

		m_fChannelElapsed += xContext.m_fDt;
		if (m_fChannelElapsed >= m_fAppliedChannelSeconds)
		{
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnApprehendChannelComplete{ xPriestID, xTarget });
			m_bChannelStartEmitted = false;
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnRunLost{ DP_RunLostCause::Apprehended, xTarget, xPriestID });
			DP_Player::SetPossessedVillager(INVALID_ENTITY_ID);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		return GRAPH_NODE_STATUS_RUNNING;
	}
	const char* GetTypeName() const override { return "DPPriestApprehendChannel"; }

private:
	void EmitInterruptedIfRunning(const Zenith_EntityID& xPriestID,
	                              DP_ApprehendInterruptReason eReason)
	{
		if (!m_bChannelStartEmitted) return;
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnApprehendChannelInterrupted{ xPriestID, m_xChannelTarget, eReason });
		m_bChannelStartEmitted = false;
	}

	float           m_fChannelElapsed        = 0.0f;
	float           m_fAppliedChannelSeconds = 3.0f;
	float           m_fAppliedRangeMetres    = 2.0f;
	Zenith_EntityID m_xChannelTarget         = INVALID_ENTITY_ID;
	bool            m_bChannelStartEmitted   = false;
};

inline void DP_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<DPNode_ResetWinState>("DPResetWinState", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_ReadHeldObjective>("DPReadHeldObjective", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_WinCheckAlreadyCollected>("DPWinCheckAlreadyCollected", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_WinNotifyCollected>("DPWinNotifyCollected", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_ConsumeHeldItem>("DPConsumeHeldItem", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DispatchObjectivePlaced>("DPDispatchObjectivePlaced", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_EmitNoise>("DPEmitNoise", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_ConsumeKeyForUnlock>("DPConsumeKeyForUnlock", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DispatchDoorOpened>("DPDispatchDoorOpened", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DispatchDoorClosed>("DPDispatchDoorClosed", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_AnimateDoorLeaves>("DPAnimateDoorLeaves", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DispatchChestOpened>("DPDispatchChestOpened", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_RequestQuit>("DPRequestQuit", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DoorCheckKey>("DPDoorCheckKey", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DoorPentagramDeferral>("DPDoorPentagramDeferral", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DoorStateChanged>("DPDoorStateChanged", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DoorEmitNoise>("DPDoorEmitNoise", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DoorAdvanceAnim>("DPDoorAdvanceAnim", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_PriestPickPatrolTarget>("DPPriestPickPatrolTarget", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_PriestApprehendChannel>("DPPriestApprehendChannel", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_VillagerKill>("DPVillagerKill", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_VillagerEmitFootstep>("DPVillagerEmitFootstep", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_VillagerTutorialPing>("DPVillagerTutorialPing", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_ReadTuningFloat>("DPReadTuningFloat", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_ForgeCraft>("DPForgeCraft", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_PickVillagerUnderCursor>("DPPickVillagerUnderCursor", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_TryPossess>("DPTryPossess", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DropHeldItem>("DPDropHeldItem", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_ItemChildRefusal>("DPItemChildRefusal", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_ItemArmChannel>("DPItemArmChannel", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_ItemCommitPickup>("DPItemCommitPickup", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_ItemRingBell>("DPItemRingBell", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_ItemEvaporate>("DPItemEvaporate", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_PauseCanToggle>("DPPauseCanToggle", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_PauseApplyToggle>("DPPauseApplyToggle", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_PauseRestart>("DPPauseRestart", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_PauseQuit>("DPPauseQuit", GRAPH_EVENT_NONE, 1, false, "DP");
}
