#pragma once
/**
 * DPInteractable_Behaviour - proximity + F-press based interaction.
 *
 * SourceBugFixed (overlap-exit): GameJam0 DPInteractable_Behaviour.cpp:57
 * removed the WRONG delegate type on overlap-exit. This port stores the
 * exact subscription handle on rising-edge and unsubscribes that exact
 * handle on falling-edge, plus on OnDisable/OnDestroy as a defence against
 * entity destruction while in range.
 */

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_EventSystem.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPInputActions.h"

// Public inheritance because DPDoor / DPChest / DPForge / DPPentagram /
// DummyNoiseMachine all derive from this — private inheritance would hide
// Zenith_ScriptBehaviour from those derived classes.
class DPInteractable_Behaviour : public Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(DPInteractable_Behaviour)

	DPInteractable_Behaviour() = delete;
	DPInteractable_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	~DPInteractable_Behaviour() override = default;

	void OnAwake() override
	{
		m_bWasInRangeLastFrame = false;
		m_xInteractSubscription = INVALID_EVENT_HANDLE;
	}

	void OnDisable() override   { TearDownSubscription(); }
	void OnDestroy() override   { TearDownSubscription(); }

	void OnUpdate(const float /*fDt*/) override
	{
		const Zenith_EntityID xVillager = DP_Player::GetPossessedVillager();
		const bool bInRange = IsVillagerInRange(xVillager);

		if (bInRange && !m_bWasInRangeLastFrame)
		{
			OnEnterRange(xVillager);
		}
		else if (!bInRange && m_bWasInRangeLastFrame)
		{
			OnExitRange(xVillager);
		}

		// While in-range each frame (NOT just rising-edge): check the F-press
		// and dispatch DP_OnInteract. The lambda subscription set up in
		// OnEnterRange catches this and invokes HandleInteract on the right
		// target. Without this, players hold proximity but the F-press only
		// counts on the single rising-edge frame — gameplay-breaking.
		if (bInRange && !m_bInteractOnOverlap && xVillager.IsValid()
		    && DP_Input::ReadInteractPressed())
		{
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnInteract{ xVillager, m_xParentEntity.GetEntityID() });
		}

		m_bWasInRangeLastFrame = bInRange;
	}

	bool IsInteractOnOverlap() const { return m_bInteractOnOverlap; }
	void SetInteractOnOverlap(bool b) { m_bInteractOnOverlap = b; }

	float GetInteractRadius() const { return m_fInteractRadius; }
	void SetInteractRadius(float f) { m_fInteractRadius = f; }

protected:
	// Override this in concrete interactables (Door, Chest, Pentagram, …) to
	// implement the actual response. Default is a no-op.
	virtual void HandleInteract(Zenith_EntityID /*xVillager*/) {}

	bool IsVillagerInRange(Zenith_EntityID xVillager) const
	{
		if (!xVillager.IsValid()) return false;
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xVillager);
		if (pxScene == nullptr) return false;
		Zenith_Entity xV = pxScene->TryGetEntity(xVillager);
		if (!xV.IsValid()) return false;
		if (!xV.HasComponent<Zenith_TransformComponent>()) return false;

		Zenith_Maths::Vector3 xVPos, xMyPos;
		xV.GetComponent<Zenith_TransformComponent>().GetPosition(xVPos);
		if (!m_xParentEntity.HasComponent<Zenith_TransformComponent>()) return false;
		m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xMyPos);

		const float fDx = xVPos.x - xMyPos.x;
		const float fDz = xVPos.z - xMyPos.z;
		return fDx * fDx + fDz * fDz <= m_fInteractRadius * m_fInteractRadius;
	}

	void OnEnterRange(Zenith_EntityID xVillager)
	{
		if (m_bInteractOnOverlap)
		{
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnInteractionBegin{ xVillager, m_xParentEntity.GetEntityID() });
			HandleInteract(xVillager);
			return;
		}
		// SubscribeLambda captures `this` to filter by target entity. The
		// stored handle MUST be unsubscribed exactly on range-exit (or
		// OnDisable/OnDestroy) so subsequent enters don't accumulate
		// subscriptions and a destroyed entity doesn't leave a dangling
		// `this` pointer in the dispatcher.
		const Zenith_EntityID xMyId = m_xParentEntity.GetEntityID();
		m_xInteractSubscription = Zenith_EventDispatcher::Get().SubscribeLambda<DP_OnInteract>(
			[this, xMyId](const DP_OnInteract& xEvt)
			{
				if (xEvt.m_xTarget.m_uIndex     != xMyId.m_uIndex     ) return;
				if (xEvt.m_xTarget.m_uGeneration != xMyId.m_uGeneration) return;
				HandleInteract(xEvt.m_xVillager);
			});

		// Note: F-press polling moved to OnUpdate so it fires every in-range
		// frame, not just on rising-edge. See OnUpdate above.
	}

	void OnExitRange(Zenith_EntityID xVillager)
	{
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnInteractionEnd{ xVillager, m_xParentEntity.GetEntityID() });
		TearDownSubscription();
	}

	void TearDownSubscription()
	{
		if (m_xInteractSubscription == INVALID_EVENT_HANDLE) return;
		Zenith_EventDispatcher::Get().Unsubscribe(m_xInteractSubscription);
		m_xInteractSubscription = INVALID_EVENT_HANDLE;
	}

	bool                m_bInteractOnOverlap = false;
	float               m_fInteractRadius    = 2.0f;
	bool                m_bWasInRangeLastFrame = false;
	Zenith_EventHandle  m_xInteractSubscription = INVALID_EVENT_HANDLE;
};
