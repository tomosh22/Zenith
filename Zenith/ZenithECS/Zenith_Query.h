#pragma once

#include "ZenithECS/Zenith_SceneData.h"
#include "Collections/Zenith_Vector.h"
// WS10: the sparse-set read toggle is read through the free-function forwarder
// Zenith_AreSparseQueryReadsEnabled() (declared in Zenith_RenderTaskState.h,
// included below; defined in Zenith_SceneSystem_Lifecycle.cpp). Routing the read
// through the forwarder -- rather than #including Zenith_SceneSystem.h for the
// accessor -- keeps it uniform with the render-task-active pattern, independent
// of accessor inlining, and breaks what would otherwise be a
// Zenith_SceneSystem.h <-> Zenith_Query.h include cycle (Zenith_SceneSystem.h
// pulls this header for its AllScenesQuery templates).
#include "ZenithECS/Internal/Zenith_RenderTaskState.h"

//------------------------------------------------------------------------------
// Zenith_Query - Multi-component entity query system
//------------------------------------------------------------------------------
//
// Enables querying entities that have multiple component types with a fluent API.
//
// Usage:
//   pxSceneData->Query<ComponentA, ComponentB>()
//       .ForEach([](Zenith_EntityID uID, ComponentA& xA, ComponentB& xB) {
//           // Process entities with both components
//       });
//
// The query iterates only over entities that have ALL specified component types.
//------------------------------------------------------------------------------

// Per-thread scratch-pool machinery used by the ForEach/ForEach_Sparse snapshot
// buffers below (Zenith_QueryScratchPool / Zenith_GetQueryScratchPool /
// Zenith_QueryScratchCheckout). Kept in an Internal detail header so consumers of
// this public header see only Zenith_Query<Ts...>.
#include "ZenithECS/Internal/Zenith_QueryScratch.h"

template<typename... Ts>
class Zenith_Query
{
public:
	explicit Zenith_Query(Zenith_SceneData& xSceneData)
		: m_pxSceneData(&xSceneData)
	{
	}

	// ForEach - iterate over all entities that have all queried component types
	// Callback signature: void(Zenith_EntityID, T1&, T2&, ...)
	// NOTE: Safe against entity creation/destruction during iteration via snapshot
	template<typename Func>
	void ForEach(Func&& fn)
	{
		Zenith_Assert(Zenith_ECS_IsMainThread() || Zenith_AreRenderTasksActive(), "Query::ForEach must be called from main thread or during render task execution");
		if (Zenith_AreSparseQueryReadsEnabled())
		{
			ForEach_Sparse(std::forward<Func>(fn));
		}
		else
		{
			ForEach_Legacy(std::forward<Func>(fn));
		}
	}

	// Count - returns the number of entities matching the query
	u_int Count()
	{
		Zenith_Assert(Zenith_ECS_IsMainThread() || Zenith_AreRenderTasksActive(), "Query::Count must be called from main thread or during render task execution");
		// Count delegates to ForEach in BOTH paths (so it inherits whichever read
		// path is active and its exact skip semantics — see spec PART 2).
		u_int uCount = 0;
		ForEach([&uCount](Zenith_EntityID, Ts&...) { ++uCount; });
		return uCount;
	}

	// First - returns the first matching entity ID, or INVALID_ENTITY_ID if none
	Zenith_EntityID First()
	{
		Zenith_Assert(Zenith_ECS_IsMainThread() || Zenith_AreRenderTasksActive(), "Query::First must be called from main thread or during render task execution");
		if (Zenith_AreSparseQueryReadsEnabled())
		{
			return First_Sparse();
		}
		return First_Legacy();
	}

	// Any - returns true if at least one entity matches the query
	bool Any()
	{
		Zenith_Assert(Zenith_ECS_IsMainThread() || Zenith_AreRenderTasksActive(), "Query::Any must be called from main thread or during render task execution");
		return First().IsValid();
	}

private:
	//==========================================================================
	// LEGACY read path (toggle OFF) — byte-for-byte the pre-WS10 implementation.
	//==========================================================================

	template<typename Func>
	void ForEach_Legacy(Func&& fn)
	{
		// Snapshot entity IDs before iteration to prevent invalidation
		// if callback creates/destroys entities. The buffer is checked out from a
		// per-thread pool that reuses capacity across calls (and gives nested
		// ForEach calls distinct buffers), so no per-call allocation occurs after
		// the first growth on each thread.
		Zenith_QueryScratchCheckout xCheckout(Zenith_GetQueryScratchPool());
		Zenith_Vector<Zenith_EntityID>& xSnapshot = xCheckout.Buffer();
		xSnapshot.Reserve(m_pxSceneData->GetActiveEntities().GetSize());
		for (u_int u = 0; u < m_pxSceneData->GetActiveEntities().GetSize(); ++u)
		{
			xSnapshot.PushBack(m_pxSceneData->GetActiveEntities().Get(u));
		}

		// Iterate snapshot
		for (u_int u = 0; u < xSnapshot.GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = xSnapshot.Get(u);

			// Skip entities that were destroyed during iteration
			if (!m_pxSceneData->EntityExists(xEntityID)) continue;

			// Skip entities pending destruction (Unity-style)
			if (m_pxSceneData->IsMarkedForDestruction(xEntityID)) continue;

			if (HasAllComponents<Ts...>(xEntityID))
			{
				fn(xEntityID, m_pxSceneData->GetComponentFromEntity<Ts>(xEntityID)...);
			}
		}
	}

	Zenith_EntityID First_Legacy()
	{
		for (u_int u = 0; u < m_pxSceneData->GetActiveEntities().GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = m_pxSceneData->GetActiveEntities().Get(u);

			// Skip entities pending destruction
			if (m_pxSceneData->IsMarkedForDestruction(xEntityID)) continue;

			if (HasAllComponents<Ts...>(xEntityID))
			{
				return xEntityID;
			}
		}
		return INVALID_ENTITY_ID;
	}

	//==========================================================================
	// SPARSE-SET read path (toggle ON) — WS10 keystone.
	//
	// Instead of scanning every active entity and probing the per-entity
	// component map once per queried type (O(entities x types)), pick the
	// SMALLEST queried pool as the DRIVER and walk its dense owner array. For
	// each driver owner, probe the OTHER pools' sparse index in O(1) and skip
	// unless all are present. Result is O(min-pool-size x types).
	//
	// Correctness rules (see spec):
	//  * Round-trip EntityID compare (poolU->m_xOwningEntities[dense] == xId)
	//    rejects slot-generation reuse — a stale sparse entry from a recycled
	//    slot fails the compare.
	//  * ALL dense indices are resolved INSIDE the per-entity loop body and
	//    NEVER cached across the fn callback — a callback that Adds/Removes
	//    components triggers swap-and-pop and shifts dense indices, so we
	//    re-probe every visit (including the DRIVER, whose snapshot dense may
	//    have shifted).
	//==========================================================================

	// Resolve the driver pool's owner array: the queried pool with the smallest
	// GetSize(). Returns nullptr if ANY queried pool is missing (unregistered /
	// null) or empty — in which case the intersection is provably empty. On
	// success, *ppxOwnersOut points at the driver pool's m_xOwningEntities.
	const Zenith_Vector<Zenith_EntityID>* PickDriverOwners()
	{
		const Zenith_Vector<Zenith_EntityID>* pxDriverOwners = nullptr;
		u_int uMinSize = 0xFFFFFFFFu;
		bool bAnyMissingOrEmpty = false;

		// Fold over Ts...: for each type, fetch its (non-asserting) pool. If any
		// is null or size 0, the whole query is empty. Otherwise track the
		// minimum-size pool's owner array as the driver.
		(
			[&]
			{
				if (bAnyMissingOrEmpty) return; // short-circuit once known empty
				Zenith_ComponentPool<Ts>* pxPool = m_pxSceneData->TryGetComponentPool<Ts>();
				if (pxPool == nullptr || pxPool->GetSize() == 0u)
				{
					bAnyMissingOrEmpty = true;
					return;
				}
				if (pxPool->GetSize() < uMinSize)
				{
					uMinSize = pxPool->GetSize();
					pxDriverOwners = &pxPool->m_xOwningEntities;
				}
			}(),
			...
		);

		if (bAnyMissingOrEmpty) return nullptr;
		return pxDriverOwners;
	}

	// Probe a SINGLE non-driver type U for entity xId: is there a live component
	// of type U owned by xId, and if so what is its CURRENT dense index? Writes
	// the dense index to uDenseOut on success. Returns false (skip) when absent
	// or when the sparse entry is stale (generation reuse — full EntityID
	// round-trip fails). Re-probed on EVERY visit; never cached.
	template<typename U>
	bool ProbeComponent(Zenith_EntityID xId, Zenith_ComponentPool<U>* pxPool, u_int& uDenseOut) const
	{
		const u_int uDense = pxPool->GetSparseDense(xId.m_uIndex);
		if (uDense == Zenith_ComponentPool<U>::uINVALID_DENSE) return false;
		// Round-trip the full EntityID (index AND generation) to reject a stale
		// sparse entry left by a recycled slot.
		if (pxPool->m_xOwningEntities.Get(uDense) != xId) return false;
		uDenseOut = uDense;
		return true;
	}

	template<typename Func>
	void ForEach_Sparse(Func&& fn)
	{
		const Zenith_Vector<Zenith_EntityID>* pxDriverOwners = PickDriverOwners();
		if (pxDriverOwners == nullptr) return; // some queried pool missing/empty

		// Snapshot the DRIVER pool's owners so the callback can Add/Remove/Destroy
		// without invalidating iteration (same re-entrancy + mutation safety as
		// the legacy snapshot). WS6 pooled buffer: no per-call alloc after warmup.
		Zenith_QueryScratchCheckout xCheckout(Zenith_GetQueryScratchPool());
		Zenith_Vector<Zenith_EntityID>& xSnapshot = xCheckout.Buffer();
		xSnapshot.Reserve(pxDriverOwners->GetSize());
		for (u_int u = 0; u < pxDriverOwners->GetSize(); ++u)
		{
			xSnapshot.PushBack(pxDriverOwners->Get(u));
		}

		for (u_int u = 0; u < xSnapshot.GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = xSnapshot.Get(u);

			// PRESERVE the exact ForEach skip semantics (same as legacy):
			// destroyed-during-iteration and pending-destruction are skipped.
			if (!m_pxSceneData->EntityExists(xEntityID)) continue;
			if (m_pxSceneData->IsMarkedForDestruction(xEntityID)) continue;

			// Probe + dispatch. AllPresentInvoke re-resolves EVERY dense index
			// (driver included) inside this loop body and never caches across fn.
			AllPresentInvoke(xEntityID, fn);
		}
	}

	Zenith_EntityID First_Sparse()
	{
		const Zenith_Vector<Zenith_EntityID>* pxDriverOwners = PickDriverOwners();
		if (pxDriverOwners == nullptr) return INVALID_ENTITY_ID;

		// PRESERVE First skip semantics EXACTLY: legacy First does NOT snapshot
		// and only checks marked-for-destruction (NOT EntityExists). Replicate.
		for (u_int u = 0; u < pxDriverOwners->GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = pxDriverOwners->Get(u);

			if (m_pxSceneData->IsMarkedForDestruction(xEntityID)) continue;

			if (HasAllComponentsSparse(xEntityID))
			{
				return xEntityID;
			}
		}
		return INVALID_ENTITY_ID;
	}

	//--------------------------------------------------------------------------
	// Sparse probe-and-dispatch helpers
	//--------------------------------------------------------------------------

	// True iff xId has a live component of EVERY queried type (round-trip safe).
	// Used by First/Any/Count-style membership checks where the component refs
	// are not needed. Re-probes each type; caches nothing.
	bool HasAllComponentsSparse(Zenith_EntityID xId)
	{
		bool bAll = true;
		(
			[&]
			{
				if (!bAll) return;
				Zenith_ComponentPool<Ts>* pxPool = m_pxSceneData->TryGetComponentPool<Ts>();
				if (pxPool == nullptr) { bAll = false; return; }
				u_int uDense = 0;
				if (!ProbeComponent<Ts>(xId, pxPool, uDense)) { bAll = false; }
			}(),
			...
		);
		return bAll;
	}

	// Probe every queried type for xId; if ALL are present, fetch each component
	// (re-resolving its dense index via GetSparseDense) and invoke fn. Resolves
	// all dense indices INSIDE this body — nothing is cached across fn.
	template<typename Func>
	void AllPresentInvoke(Zenith_EntityID xId, Func&& fn)
	{
		// Presence pass: verify EVERY queried type has a live, round-trip-valid
		// component for xId before any fetch. Bail (skip this entity) the moment
		// one is missing/stale. PickDriverOwners already proved every pool is
		// non-null for this query, so TryGetComponentPool here returns non-null;
		// the defensive null-check keeps the helper self-contained.
		bool bAll = true;
		(
			[&]
			{
				if (!bAll) return;
				Zenith_ComponentPool<Ts>* pxPool = m_pxSceneData->TryGetComponentPool<Ts>();
				if (pxPool == nullptr) { bAll = false; return; }
				u_int uDense = 0;
				if (!ProbeComponent<Ts>(xId, pxPool, uDense)) { bAll = false; }
			}(),
			...
		);
		if (!bAll) return;

		// All present: fetch each component by RE-RESOLVING its dense index right
		// now (driver included — the snapshot dense may have shifted). Round-trip
		// already validated in the presence pass above; here we trust the current
		// sparse mapping for the fetch, re-resolved fresh.
		fn(xId, FetchComponentRef<Ts>(xId)...);
	}

	// Re-resolve T's CURRENT dense index for xId and return the component ref.
	// Called only after AllPresentInvoke's presence pass confirmed all types are
	// present; re-resolves (never caches) so a callback earlier in the SAME fold
	// cannot stale this. The asserting Get() inside is safe: presence is proven.
	template<typename T>
	T& FetchComponentRef(Zenith_EntityID xId)
	{
		Zenith_ComponentPool<T>* pxPool = m_pxSceneData->TryGetComponentPool<T>();
		const u_int uDense = pxPool->GetSparseDense(xId.m_uIndex);
		return pxPool->Get(uDense);
	}

	// Helper to check if entity has all component types using fold expression
	// (LEGACY path only — reads the per-entity component map via EntityHasComponent).
	template<typename... Us>
	bool HasAllComponents(Zenith_EntityID xEntityID)
	{
		return (m_pxSceneData->EntityHasComponent<Us>(xEntityID) && ...);
	}

	Zenith_SceneData* m_pxSceneData;
};

//------------------------------------------------------------------------------
// Zenith_SceneData::Query implementation (must be after Zenith_Query definition)
//------------------------------------------------------------------------------

template<typename... Ts>
Zenith_Query<Ts...> Zenith_SceneData::Query()
{
	return Zenith_Query<Ts...>(*this);
}
