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
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DPCommonTypes.h"
#include "Source/DP_Tuning.h"

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
class DPNode_DepositHeldObjective : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_DepositHeldObjective)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		if (!xVillager.IsValid())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}

		const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xVillager);
		if (!DP_IsObjectiveTag(eHeld)) return GRAPH_NODE_STATUS_FAILURE;

		const uint32_t uBit = DP_ObjectiveTagToBit(eHeld);
		if (DP_Win::GetCollectedObjectivesMask() & uBit) return GRAPH_NODE_STATUS_FAILURE; // already collected

		DP_Win::NotifyObjectiveCollected(eHeld, xVillager, xContext.m_xSelf.GetEntityID());
		// Consume the held objective.
		Zenith_EntityID xItem = DP_Player::GetHeldItemEntity(xVillager);
		DP_Player::RemoveHeldItem(xVillager);
		if (xItem.IsValid())
		{
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xItem);
			if (pxScene != nullptr)
			{
				Zenith_Entity xEnt = pxScene->TryGetEntity(xItem);
				if (xEnt.IsValid())
				{
					xEnt.Destroy();
				}
			}
		}

		const int iBitIdx = static_cast<int>(eHeld) - static_cast<int>(DP_ItemTag::Objective1);
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnObjectivePlaced{
				xVillager,
				xContext.m_xSelf.GetEntityID(),
				iBitIdx });
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPDepositHeldObjective"; }
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
		if (xContext.m_xSelf.IsValid() && xContext.m_xSelf.HasComponent<Zenith_TransformComponent>())
		{
			xContext.m_xSelf.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
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

// The double-door interact: key-gated open. Guards + key consumption +
// DP_OnDoorOpened dispatch are DPDoubleDoor_Component::HandleInteract
// verbatim; the open flag lives on the blackboard for the animate node
// (and for tests, where the C++ exposed IsOpen()).
class DPNode_TryOpenDoor : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_TryOpenDoor)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")
	ZENITH_PROPERTY(std::string, m_strIsOpenVar, "isOpen")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (xContext.m_pxBlackboard->GetBool(m_strIsOpenVar)) return GRAPH_NODE_STATUS_FAILURE;
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		if (!xVillager.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		if (!DP_Items::TryConsumeKeyForUnlock(xVillager, DP_ItemTag::Key)) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_PropertyValue xTrue;
		xTrue.SetBool(true);
		xContext.m_pxBlackboard->SetValue(m_strIsOpenVar, xTrue);
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnDoorOpened{ xVillager, xContext.m_xSelf.GetEntityID() });
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPTryOpenDoor"; }
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
			if (!xChild.HasComponent<Zenith_TransformComponent>()) continue;
			return &xChild.GetComponent<Zenith_TransformComponent>();
		}
		return nullptr;
	}
};

//==============================================================================
// Chest
//==============================================================================

// The chest interact: open guard + DP_OnChestOpened dispatch
// (DPChest_Component::HandleInteract verbatim).
class DPNode_OpenChest : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_OpenChest)
public:
	ZENITH_PROPERTY(std::string, m_strVillagerVar, "payload")
	ZENITH_PROPERTY(std::string, m_strIsOpenVar, "isOpen")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (xContext.m_pxBlackboard->GetBool(m_strIsOpenVar)) return GRAPH_NODE_STATUS_FAILURE;
		const Zenith_EntityID xVillager = DPGraph_GetEntityVar(xContext, m_strVillagerVar);
		Zenith_PropertyValue xTrue;
		xTrue.SetBool(true);
		xContext.m_pxBlackboard->SetValue(m_strIsOpenVar, xTrue);
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnChestOpened{ xVillager, xContext.m_xSelf.GetEntityID() });
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPOpenChest"; }
};

// Per-frame chest lid progress - DPChest_Component::OnUpdate verbatim (the
// C++ only advanced m_fOpenT; the actual lid rotation remains Wave-4 polish).
class DPNode_AdvanceChestLid : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(DPNode_AdvanceChestLid)
public:
	ZENITH_PROPERTY(std::string, m_strIsOpenVar, "isOpen")
	ZENITH_PROPERTY(std::string, m_strOpenTVar, "openT")
	ZENITH_PROPERTY(std::string, m_strDurationKey, "interactables.chest_open_duration_s")

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
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "DPAdvanceChestLid"; }
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

inline void DP_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<DPNode_ResetWinState>("DPResetWinState", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_DepositHeldObjective>("DPDepositHeldObjective", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_EmitNoise>("DPEmitNoise", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_TryOpenDoor>("DPTryOpenDoor", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_AnimateDoorLeaves>("DPAnimateDoorLeaves", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_OpenChest>("DPOpenChest", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_AdvanceChestLid>("DPAdvanceChestLid", GRAPH_EVENT_NONE, 1, false, "DP");
	xRegistry.RegisterNodeType<DPNode_RequestQuit>("DPRequestQuit", GRAPH_EVENT_NONE, 1, false, "DP");
}
