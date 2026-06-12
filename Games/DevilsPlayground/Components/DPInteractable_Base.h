#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPInteractable_Base - proximity + F-press based interaction.
 *
 * Plain (NON-component, NON-registered) C++ base class. Only the concrete
 * leaves (DPDoor / DPDoubleDoor / DPChest / DPForge / DPPentagram /
 * DummyNoiseMachine) register as game components; they publicly inherit this
 * base for the proximity + F-press + DP_OnInteract wiring. The leaf's
 * constructor passes its parent entity through to the protected constructor
 * here, which stores it as the explicit m_xParentEntity member.
 *
 * SourceBugFixed (overlap-exit): GameJam0 DPInteractable_Behaviour.cpp:57
 * removed the WRONG delegate type on overlap-exit. This port stores the
 * exact subscription handle on rising-edge and unsubscribes that exact
 * handle on falling-edge, plus on OnDisable/OnDestroy as a defence against
 * entity destruction while in range.
 *
 * Heap-stability note (component pools relocate on resize / swap-and-pop /
 * cross-scene transfer): the DP_OnInteract subscription captures `this`, so
 * the base hand-writes its move operations to unsubscribe the old address
 * and re-subscribe the new one. Leaf components rely on their implicit move
 * constructors chaining into the base move constructor — do NOT add a
 * user-declared destructor or copy/move operation to a leaf without also
 * keeping it move-constructible.
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPInputActions.h"
#include "Source/DP_Tuning.h"

class DPInteractable_Base
{
public:
	DPInteractable_Base() = delete;

	virtual ~DPInteractable_Base()
	{
		// Belt-and-braces: OnDisable/OnDestroy already tear the subscription
		// down; this covers any teardown path that skips the lifecycle hooks.
		TearDownSubscription();
	}

	// Component pools relocate on resize / swap-and-pop / cross-scene moves.
	// The DP_OnInteract subscription lambda captures `this`, so moves must
	// re-point the subscription at the new address. Copies are meaningless
	// for a subscription owner — deleted.
	DPInteractable_Base(const DPInteractable_Base&) = delete;
	DPInteractable_Base& operator=(const DPInteractable_Base&) = delete;

	DPInteractable_Base(DPInteractable_Base&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_bInteractOnOverlap(xOther.m_bInteractOnOverlap)
		, m_fInteractRadius(xOther.m_fInteractRadius)
		, m_xInRangeVillager(xOther.m_xInRangeVillager)
		, m_xInteractSubscription(INVALID_EVENT_HANDLE)
	{
		TakeOverSubscriptionFrom(xOther);
	}

	DPInteractable_Base& operator=(DPInteractable_Base&& xOther) noexcept
	{
		if (this != &xOther)
		{
			TearDownSubscription();
			m_xParentEntity      = xOther.m_xParentEntity;
			m_bInteractOnOverlap = xOther.m_bInteractOnOverlap;
			m_fInteractRadius    = xOther.m_fInteractRadius;
			m_xInRangeVillager   = xOther.m_xInRangeVillager;
			TakeOverSubscriptionFrom(xOther);
		}
		return *this;
	}

	void OnAwake()
	{
		// MVP-0.1.4: read default proximity radius from DP_Tuning. Subclasses
		// (DPDoor / DPDoubleDoor / DPChest / DPForge / DPPentagram /
		// DummyNoiseMachine) override their own per-type durations / loudness
		// in their OnAwake on top of this.
		m_fInteractRadius = DP_Tuning::Get<float>("interactables.default_proximity_radius_m");

		m_xInRangeVillager = INVALID_ENTITY_ID;
		TearDownSubscription();
	}

	void OnDisable()   { TearDownSubscription(); }
	void OnDestroy()   { TearDownSubscription(); }

	// Phase-5-audit follow-up (2026-05-16): cache the in-range villager
	// so OnExitRange dispatches DP_OnInteractionEnd with the villager
	// that ACTUALLY left, not whatever DP_Player::GetPossessedVillager()
	// happens to return on the exit frame.
	//
	// Previously, the state machine compared (DP_Player::GetPossessedVillager,
	// IsVillagerInRange) frame-to-frame. The failure mode: if the
	// possessed villager died while inside the interactable's proximity,
	// the next frame's GetPossessedVillager() returned INVALID (the
	// death path called SetPossessedVillager(INVALID)). IsVillagerInRange
	// then short-circuited to false, m_bWasInRangeLastFrame was still
	// true, and OnExitRange(INVALID) fired -- so DP_OnInteractionEnd's
	// m_xVillager was INVALID and the telemetry visualiser couldn't
	// place the marker on the deceased villager's position.
	//
	// The new shape: each frame, derive a "would-be in-range villager"
	// (the current possessed, IFF in range; else INVALID). Transitions
	// on that value drive enter/exit. Enter caches the villager; exit
	// dispatches with the cached villager + clears the cache. This also
	// correctly handles the rare mid-range possession swap (A in range
	// -> swap to B in range -> exit(A) + enter(B)).
	void OnUpdate(const float /*fDt*/)
	{
		const Zenith_EntityID xCurrent = DP_Player::GetPossessedVillager();
		const bool bCurrentInRange = IsVillagerInRange(xCurrent);
		const Zenith_EntityID xWouldBeInRange =
			bCurrentInRange ? xCurrent : Zenith_EntityID{};

		const bool bWasInRange    = m_xInRangeVillager.IsValid();
		const bool bWillBeInRange = xWouldBeInRange.IsValid();
		const bool bDifferent     =
			(m_xInRangeVillager.m_uIndex      != xWouldBeInRange.m_uIndex) ||
			(m_xInRangeVillager.m_uGeneration != xWouldBeInRange.m_uGeneration);

		if (bDifferent)
		{
			if (bWasInRange)
			{
				// Exit using the CACHED villager -- the one that was
				// actually in range, not the current possessed (which
				// may be INVALID after death or a different villager
				// after a mid-range swap).
				OnExitRange(m_xInRangeVillager);
			}
			if (bWillBeInRange)
			{
				OnEnterRange(xWouldBeInRange);
			}
			m_xInRangeVillager = xWouldBeInRange;
		}

		// While in-range each frame (NOT just rising-edge): check the F-press
		// and dispatch DP_OnInteract. The lambda subscription set up in
		// OnEnterRange catches this and invokes HandleInteract on the right
		// target. Without this, players hold proximity but the F-press only
		// counts on the single rising-edge frame — gameplay-breaking.
		if (bWillBeInRange && !m_bInteractOnOverlap
		    && DP_Input::ReadInteractPressed())
		{
			Zenith_EventDispatcher::Get().Dispatch(
				DP_OnInteract{ xWouldBeInRange, m_xParentEntity.GetEntityID() });
		}
	}

	// Component contract: version-only payload (interactable state is
	// runtime-built; tunables come from DP_Tuning / the spawner).
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

	bool IsInteractOnOverlap() const { return m_bInteractOnOverlap; }
	void SetInteractOnOverlap(bool b) { m_bInteractOnOverlap = b; }

	float GetInteractRadius() const { return m_fInteractRadius; }
	void SetInteractRadius(float f) { m_fInteractRadius = f; }

protected:
	// The leaf component's constructor passes its parent entity through here.
	explicit DPInteractable_Base(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	// Override this in concrete interactables (Door, Chest, Pentagram, …) to
	// implement the actual response. Default is a no-op.
	virtual void HandleInteract(Zenith_EntityID /*xVillager*/) {}

	// 2026-05-25: virtual so subclasses can override with a custom
	// anchor (DPDoor uses its logical centre, not the entity transform,
	// because the corner-anchored SM_Cube mesh offsets the transform
	// position by ~1 m from the geometric door centre).
	virtual bool IsVillagerInRange(Zenith_EntityID xVillager) const
	{
		if (!xVillager.IsValid()) return false;
		Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneDataForEntity(xVillager);
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
		SubscribeInteract();

		// Note: F-press polling moved to OnUpdate so it fires every in-range
		// frame, not just on rising-edge. See OnUpdate above.
	}

	void OnExitRange(Zenith_EntityID xVillager)
	{
		Zenith_EventDispatcher::Get().Dispatch(
			DP_OnInteractionEnd{ xVillager, m_xParentEntity.GetEntityID() });
		TearDownSubscription();
	}

	// The Subscribe captures `this` to filter by target entity. The stored
	// handle MUST be unsubscribed exactly on range-exit (or OnDisable/
	// OnDestroy) so subsequent enters don't accumulate subscriptions and a
	// destroyed entity doesn't leave a dangling `this` pointer in the
	// dispatcher. Factored out of OnEnterRange so the move operations can
	// re-subscribe with the relocated `this`.
	void SubscribeInteract()
	{
		const Zenith_EntityID xMyId = m_xParentEntity.GetEntityID();
		m_xInteractSubscription = Zenith_EventDispatcher::Get().Subscribe<DP_OnInteract>(
			[this, xMyId](const DP_OnInteract& xEvt)
			{
				if (xEvt.m_xTarget.m_uIndex     != xMyId.m_uIndex     ) return;
				if (xEvt.m_xTarget.m_uGeneration != xMyId.m_uGeneration) return;
				HandleInteract(xEvt.m_xVillager);
			});
	}

	void TearDownSubscription()
	{
		if (m_xInteractSubscription == INVALID_EVENT_HANDLE) return;
		Zenith_EventDispatcher::Get().Unsubscribe(m_xInteractSubscription);
		m_xInteractSubscription = INVALID_EVENT_HANDLE;
	}

	// Move-helper: drop the source's this-capturing subscription and stand up
	// a fresh one against the new address iff the source was subscribed.
	void TakeOverSubscriptionFrom(DPInteractable_Base& xOther)
	{
		if (xOther.m_xInteractSubscription == INVALID_EVENT_HANDLE) return;
		Zenith_EventDispatcher::Get().Unsubscribe(xOther.m_xInteractSubscription);
		xOther.m_xInteractSubscription = INVALID_EVENT_HANDLE;
		SubscribeInteract();
	}

	// Explicit parent-entity member (the old script base provided this; the
	// component contract makes it an ordinary member set by the leaf ctor).
	Zenith_Entity       m_xParentEntity;

	bool                m_bInteractOnOverlap = false;
	float               m_fInteractRadius    = 2.0f; // Fallback; OnAwake reads DP_Tuning.
	// The villager currently considered "in range" of this interactable,
	// or INVALID_ENTITY_ID when no villager is in range. Replaces the
	// old m_bWasInRangeLastFrame bool: storing the entity (not just the
	// fact-of) lets OnExitRange dispatch DP_OnInteractionEnd with the
	// villager that actually left, even if that villager has just died
	// (which would have flipped DP_Player::GetPossessedVillager() to
	// INVALID by the time the exit edge fires).
	Zenith_EntityID     m_xInRangeVillager      = INVALID_ENTITY_ID;
	Zenith_EventHandle  m_xInteractSubscription = INVALID_EVENT_HANDLE;
	// Note: GetInteractRadius() is declared above as part of the public API;
	// tests use the same accessor.
};
