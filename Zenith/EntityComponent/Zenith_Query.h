#pragma once

#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "Collections/Zenith_Vector.h"

//------------------------------------------------------------------------------
// Zenith_Query - Multi-component entity query system
//------------------------------------------------------------------------------
//
// Enables querying entities that have multiple component types with a fluent API.
//
// Usage:
//   pxSceneData->Query<TransformComponent, ColliderComponent>()
//       .ForEach([](Zenith_EntityID uID, TransformComponent& xT, ColliderComponent& xC) {
//           // Process entities with both components
//       });
//
// The query iterates only over entities that have ALL specified component types.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Zenith_QueryScratchPool - per-thread pool of reusable snapshot buffers
//------------------------------------------------------------------------------
//
// ForEach copies the active entity IDs into a scratch buffer before iterating so
// the callback can create/destroy entities without invalidating iteration. To
// avoid allocating a fresh buffer on every call, buffers are pooled and their
// capacity is reused across calls (Zenith_Vector::Clear() resets size to 0
// without deallocating).
//
// The pool is thread_local because ForEach may legitimately run on render-task
// worker threads (see the ForEach assert) as well as the main thread; a single
// shared pool would race. The pool is also a NON-template free helper so every
// Zenith_Query<Ts...> instantiation on a thread shares one pool and reuses
// capacity across different query type-combos.
//
// m_uInUse acts as a nesting-depth cursor: each active (nested) ForEach on the
// thread checks out a distinct buffer, so an inner ForEach never clobbers the
// outer loop's snapshot.
//------------------------------------------------------------------------------
struct Zenith_QueryScratchPool
{
	static constexpr u_int uPOOL_SIZE = 8;
	Zenith_Vector<Zenith_EntityID> m_axBuffers[uPOOL_SIZE];
	u_int m_uInUse = 0;
};

// Accessor wrapping a function-local thread_local static so there is no
// namespace-scope non-trivial global (keeps the constinit g_xEngine invariant
// untouched). Constructed on first use per thread, destroyed at thread exit.
static Zenith_QueryScratchPool& Zenith_GetQueryScratchPool()
{
	thread_local static Zenith_QueryScratchPool tl_xQueryScratchPool;
	return tl_xQueryScratchPool;
}

//------------------------------------------------------------------------------
// Zenith_QueryScratchCheckout - RAII checkout of one pooled scratch buffer
//------------------------------------------------------------------------------
//
// On construction, grabs the next free pooled buffer (LIFO via m_uInUse) and
// Clear()s it to reuse its retained capacity. If the pool is exhausted (nesting
// deeper than uPOOL_SIZE on this thread), falls back to an owned local buffer so
// correctness is preserved - only that one frame allocates. On destruction,
// returns the pooled slot. Non-copyable/non-movable so the LIFO check-in cannot
// be duplicated.
//------------------------------------------------------------------------------
class Zenith_QueryScratchCheckout
{
public:
	explicit Zenith_QueryScratchCheckout(Zenith_QueryScratchPool& xPool)
		: m_pxPool(&xPool)
	{
		if (xPool.m_uInUse < Zenith_QueryScratchPool::uPOOL_SIZE)
		{
			m_pxBuffer = &xPool.m_axBuffers[xPool.m_uInUse];
			++xPool.m_uInUse;
			m_bPooled = true;
			m_pxBuffer->Clear(); // retain capacity, reset size to 0
		}
		else
		{
			// Pool exhausted (deep nesting) - allocate an owned fallback buffer.
			// Lazily allocated ONLY on this rare path, so the common pooled path
			// has zero per-call heap allocation (the whole point of WS6).
			m_bPooled = false;
			m_pxFallbackOwned = new Zenith_Vector<Zenith_EntityID>();
			m_pxBuffer = m_pxFallbackOwned;
		}
	}

	~Zenith_QueryScratchCheckout()
	{
		if (m_bPooled)
		{
			// LIFO check-in - guaranteed because checkouts are strictly nested
			// by call-stack scope.
			--m_pxPool->m_uInUse;
		}
		// Non-null only on the rare pool-exhausted path; delete-of-nullptr is a
		// no-op on the common pooled path, so there is no per-call deallocation.
		delete m_pxFallbackOwned;
	}

	Zenith_QueryScratchCheckout(const Zenith_QueryScratchCheckout&) = delete;
	Zenith_QueryScratchCheckout& operator=(const Zenith_QueryScratchCheckout&) = delete;
	Zenith_QueryScratchCheckout(Zenith_QueryScratchCheckout&&) = delete;
	Zenith_QueryScratchCheckout& operator=(Zenith_QueryScratchCheckout&&) = delete;

	Zenith_Vector<Zenith_EntityID>& Buffer() { return *m_pxBuffer; }

private:
	Zenith_QueryScratchPool* m_pxPool;
	Zenith_Vector<Zenith_EntityID>* m_pxBuffer;
	Zenith_Vector<Zenith_EntityID>* m_pxFallbackOwned = nullptr;
	bool m_bPooled = false;
};

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
		Zenith_Assert(g_xEngine.Threading().IsMainThread() || g_xEngine.Scenes().AreRenderTasksActive(), "Query::ForEach must be called from main thread or during render task execution");
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

	// ForEachUnsafe - iterate without snapshot (no allocation)
	// Caller guarantees no structural changes (entity creation/destruction) during iteration.
	// Use from inside Update() where mutations are already deferred.
	template<typename Func>
	void ForEachUnsafe(Func&& fn)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread() || g_xEngine.Scenes().AreRenderTasksActive(), "Query::ForEachUnsafe must be called from main thread or during render task execution");
		for (u_int u = 0; u < m_pxSceneData->GetActiveEntities().GetSize(); ++u)
		{
			Zenith_EntityID xEntityID = m_pxSceneData->GetActiveEntities().Get(u);

			if (m_pxSceneData->IsMarkedForDestruction(xEntityID)) continue;

			if (HasAllComponents<Ts...>(xEntityID))
			{
				fn(xEntityID, m_pxSceneData->GetComponentFromEntity<Ts>(xEntityID)...);
			}
		}
	}

	// Count - returns the number of entities matching the query
	u_int Count()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread() || g_xEngine.Scenes().AreRenderTasksActive(), "Query::Count must be called from main thread or during render task execution");
		u_int uCount = 0;
		ForEach([&uCount](Zenith_EntityID, Ts&...) { ++uCount; });
		return uCount;
	}

	// CountUnsafe - count without snapshot (no allocation)
	// Same safety requirements as ForEachUnsafe.
	u_int CountUnsafe()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread() || g_xEngine.Scenes().AreRenderTasksActive(), "Query::CountUnsafe must be called from main thread or during render task execution");
		u_int uCount = 0;
		ForEachUnsafe([&uCount](Zenith_EntityID, Ts&...) { ++uCount; });
		return uCount;
	}

	// First - returns the first matching entity ID, or INVALID_ENTITY_ID if none
	Zenith_EntityID First()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread() || g_xEngine.Scenes().AreRenderTasksActive(), "Query::First must be called from main thread or during render task execution");
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

	// Any - returns true if at least one entity matches the query
	bool Any()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread() || g_xEngine.Scenes().AreRenderTasksActive(), "Query::Any must be called from main thread or during render task execution");
		return First().IsValid();
	}

private:
	// Helper to check if entity has all component types using fold expression
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
