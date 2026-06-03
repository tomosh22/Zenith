#pragma once

#include "Collections/Zenith_Vector.h"
#include "ZenithECS/Zenith_Entity.h"

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
// namespace-scope non-trivial global (keeps the engine singleton's constinit
// invariant untouched). Constructed on first use per thread, destroyed at thread exit.
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
