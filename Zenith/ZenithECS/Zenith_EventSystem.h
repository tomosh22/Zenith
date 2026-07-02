#pragma once

#include "Collections/Zenith_HashMap.h"
#include "Collections/Zenith_Vector.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "ZenithECS/Zenith_SceneData.h"
// Zenith_ECS_IsMainThread() forwarder -- lets the Subscribe asserts check thread
// affinity without naming the engine singleton (keeps this header ECS-leaf-clean).
#include "ZenithECS/Internal/Zenith_RenderTaskState.h"
// Type-erasure machinery (Zenith_EventTypeID / Zenith_EventBase /
// Zenith_EventWrapper / Zenith_CallbackBase / Zenith_CallbackWrapper /
// Zenith_LambdaCallbackWrapper). PRIVATE detail, no external file names these.
// Pulled in BEFORE Zenith_EventDispatcher because the dispatcher's inline
// Subscribe / Dispatch / QueueEvent bodies instantiate these templates.
#include "ZenithECS/Internal/Zenith_EventSystem_Detail.h"

//------------------------------------------------------------------------------
// Zenith_EventSystem - Type-safe event dispatcher for ECS events
//------------------------------------------------------------------------------
//
// Usage (MyEvent is any game-defined payload struct; payload types are defined
// by their owners, not by this header):
//   // Subscribe to an event
//   auto uHandle = Zenith_EventDispatcher::Get().Subscribe<MyEvent>(
//       [](const MyEvent& xEvent) {
//           // Handle event
//       });
//
//   // Dispatch an event immediately
//   Zenith_EventDispatcher::Get().Dispatch(MyEvent{ /* ... */ });
//
//   // Queue an event for deferred processing (thread-safe)
//   Zenith_EventDispatcher::Get().QueueEvent(MyEvent{ /* ... */ });
//
//   // Process queued events (call from main thread)
//   Zenith_EventDispatcher::Get().ProcessDeferredEvents();
//
//   // Unsubscribe
//   Zenith_EventDispatcher::Get().Unsubscribe(uHandle);
//------------------------------------------------------------------------------

// Event handle type for subscription management
using Zenith_EventHandle = u_int;
constexpr Zenith_EventHandle INVALID_EVENT_HANDLE = static_cast<Zenith_EventHandle>(-1);

// Move-only RAII wrapper around a subscription handle (defined below the
// dispatcher). Returned by Zenith_EventDispatcher::SubscribeScoped<E>().
class Zenith_Subscription;

//------------------------------------------------------------------------------
// Event Dispatcher (Singleton)
//------------------------------------------------------------------------------

class Zenith_EventDispatcher
{
public:
	static Zenith_EventDispatcher& Get();

	// Subscribe with a function pointer (explicit fast path: stores the raw
	// void(*)(const TEvent&) in a Zenith_CallbackWrapper).
	template<typename TEvent>
	Zenith_EventHandle Subscribe(void(*pfnCallback)(const TEvent&));

	// Subscribe with any callable (capturing lambda / functor). This is the
	// former SubscribeLambda, folded under the single name Subscribe: the
	// function-pointer overload above is more specialized, so existing
	// Subscribe<TEvent>(&Func) calls still bind to it; a capturing lambda binds
	// here. std::function is still rejected (use a function pointer or lambda).
	template<typename TEvent, typename TCallback>
	Zenith_EventHandle Subscribe(TCallback&& xCallback);

	// RAII variants of Subscribe: return a move-only Zenith_Subscription that
	// Unsubscribes on destruction, so the sub/unsub pair collapses to a single
	// owned member (no hand-written Unsubscribe in OnDisable/OnDestroy). Intended
	// for STATELESS / STABLE-ADDRESS subscribers (function pointers, non-capturing
	// lambdas, or `this`-capturing callbacks on objects that do NOT relocate). A
	// component captured by `this` that the pool RELOCATES must still hand-write
	// its moves — see the note on Zenith_Subscription below. Bodies are at the
	// bottom of this header (after Zenith_Subscription is complete).
	template<typename TEvent>
	Zenith_Subscription SubscribeScoped(void(*pfnCallback)(const TEvent&));
	template<typename TEvent, typename TCallback>
	Zenith_Subscription SubscribeScoped(TCallback&& xCallback);

	// Unsubscribe by handle
	void Unsubscribe(Zenith_EventHandle uHandle);

	// Dispatch event immediately to all subscribers
	template<typename TEvent>
	void Dispatch(const TEvent& xEvent);

	// Queue event for deferred processing (thread-safe)
	template<typename TEvent>
	void QueueEvent(const TEvent& xEvent);

	// Process all queued events (call from main thread)
	void ProcessDeferredEvents();

	// Clear all subscriptions (useful for testing)
	void ClearAllSubscriptions();

	// Get subscriber count for an event type (useful for testing)
	template<typename TEvent>
	u_int GetSubscriberCount() const;

private:
	Zenith_EventDispatcher() = default;

	struct Subscription
	{
		u_int m_uEventTypeID = 0;
		Zenith_CallbackBase* m_pxCallback = nullptr;

		Subscription() = default;
		~Subscription() { delete m_pxCallback; }

		// Move-only (prevent double-delete)
		Subscription(Subscription&& xOther) noexcept
			: m_uEventTypeID(xOther.m_uEventTypeID)
			, m_pxCallback(xOther.m_pxCallback)
		{
			xOther.m_pxCallback = nullptr;
		}

		Subscription& operator=(Subscription&& xOther) noexcept
		{
			if (this != &xOther)
			{
				delete m_pxCallback;
				m_uEventTypeID = xOther.m_uEventTypeID;
				m_pxCallback = xOther.m_pxCallback;
				xOther.m_pxCallback = nullptr;
			}
			return *this;
		}

		// Delete copy operations
		Subscription(const Subscription&) = delete;
		Subscription& operator=(const Subscription&) = delete;
	};

public:
	// Test-support RAII guard. On construction it snapshots the process-global
	// dispatcher's subscription state and leaves the live dispatcher empty; on
	// destruction it discards whatever ran inside the scope and restores the
	// snapshot. Unit tests use this instead of ClearAllSubscriptions() so the
	// boot-time test phase (which runs AFTER Project_RegisterGameComponents) can
	// no longer wipe a game's boot-time subscriptions — the whole class behind the
	// "DP_Tutorial / DP_Particles dead in normal launches" bug. Nested, so it
	// reaches the enclosing private Subscription type + members with no friend.
	// Main-thread use only; not for use mid-Dispatch.
	class ScopedTestIsolation
	{
	public:
		ScopedTestIsolation();
		~ScopedTestIsolation();

		ScopedTestIsolation(const ScopedTestIsolation&) = delete;
		ScopedTestIsolation& operator=(const ScopedTestIsolation&) = delete;
		ScopedTestIsolation(ScopedTestIsolation&&) = delete;
		ScopedTestIsolation& operator=(ScopedTestIsolation&&) = delete;

	private:
		Zenith_HashMap<Zenith_EventHandle, Subscription> m_xSavedSubs;
		Zenith_HashMap<u_int, Zenith_Vector<Zenith_EventHandle>> m_xSavedByType;
		Zenith_Vector<Zenith_EventBase*> m_axSavedDeferred;
	};

private:
	Zenith_HashMap<Zenith_EventHandle, Subscription> m_xSubscriptions;
	Zenith_HashMap<u_int, Zenith_Vector<Zenith_EventHandle>> m_xSubscribersByEventType;

	Zenith_Vector<Zenith_EventBase*> m_axDeferredEvents;
	Zenith_Mutex m_xDeferredMutex;

	Zenith_EventHandle m_uNextHandle = 1;

	// Deferred unsubscribe support - prevents modifying subscriber vectors during Dispatch iteration
	bool m_bDispatching = false;
	Zenith_Vector<Zenith_EventHandle> m_axPendingUnsubscribes;
};

//------------------------------------------------------------------------------
// Zenith_EventDispatcher::ScopedTestIsolation — snapshot/restore guard
//------------------------------------------------------------------------------

inline Zenith_EventDispatcher::ScopedTestIsolation::ScopedTestIsolation()
{
	Zenith_EventDispatcher& xDispatcher = Zenith_EventDispatcher::Get();

	// Steal the live subscription tables into the guard, leaving the dispatcher
	// empty for the lifetime of the scope. m_uNextHandle is deliberately left
	// untouched (it is monotonic), so any handle a test allocates is strictly
	// greater than every saved boot handle and cannot collide on restore. The
	// transient dispatch bookkeeping (m_bDispatching / m_axPendingUnsubscribes)
	// never straddles a test boundary, so it needs no snapshot.
	m_xSavedSubs   = std::move(xDispatcher.m_xSubscriptions);
	m_xSavedByType = std::move(xDispatcher.m_xSubscribersByEventType);

	xDispatcher.m_xDeferredMutex.Lock();
	m_axSavedDeferred = std::move(xDispatcher.m_axDeferredEvents);
	xDispatcher.m_xDeferredMutex.Unlock();
}

inline Zenith_EventDispatcher::ScopedTestIsolation::~ScopedTestIsolation()
{
	Zenith_EventDispatcher& xDispatcher = Zenith_EventDispatcher::Get();

	// Discard whatever the scope subscribed / queued — ClearAllSubscriptions is
	// the existing teardown that deletes the owned callbacks and any leftover
	// deferred events — then move the snapshot back over the now-empty tables.
	// HashMap move-assign frees the destination first and Subscription's move
	// nulls its source pointer, so every callback is deleted exactly once.
	xDispatcher.ClearAllSubscriptions();

	xDispatcher.m_xSubscriptions          = std::move(m_xSavedSubs);
	xDispatcher.m_xSubscribersByEventType = std::move(m_xSavedByType);

	xDispatcher.m_xDeferredMutex.Lock();
	xDispatcher.m_axDeferredEvents = std::move(m_axSavedDeferred);
	xDispatcher.m_xDeferredMutex.Unlock();
}

//------------------------------------------------------------------------------
// Zenith_Subscription — move-only RAII handle owner
//------------------------------------------------------------------------------
//
// Owns one Subscribe() handle and Unsubscribes it exactly once on destruction
// (idempotent; a moved-from / default-constructed subscription is an inert no-op).
// It reaches Zenith_EventDispatcher::Get() at destruction time rather than caching
// a dispatcher pointer, so it stays correct across shutdown ordering (the
// dispatcher is a Meyers singleton). Unsubscribe already routes through the
// dispatcher's deferred-unsubscribe-during-dispatch path, so destroying a
// subscription mid-dispatch is safe.
//
// SCOPE OF SAFETY (read before storing as a component member): auto-unsubscribe is
// correct for stateless callbacks and for subscribers with a STABLE address. The
// ECS component pool RELOCATES instances (move-construct + destruct-source); a
// component that subscribed a lambda capturing `this` must STILL hand-write its
// move to unsubscribe the source and re-subscribe a fresh lambda capturing the new
// `this`. A bare Zenith_Subscription member only transfers the handle, leaving the
// dispatcher holding a lambda bound to the freed old address — so do NOT use it to
// replace those component move-helpers.
class Zenith_Subscription
{
public:
	Zenith_Subscription() = default;
	explicit Zenith_Subscription(Zenith_EventHandle uHandle) : m_uHandle(uHandle) {}

	~Zenith_Subscription() { Reset(); }

	Zenith_Subscription(Zenith_Subscription&& xOther) noexcept
		: m_uHandle(xOther.m_uHandle)
	{
		xOther.m_uHandle = INVALID_EVENT_HANDLE;
	}
	Zenith_Subscription& operator=(Zenith_Subscription&& xOther) noexcept
	{
		if (this != &xOther)
		{
			Reset();
			m_uHandle = xOther.m_uHandle;
			xOther.m_uHandle = INVALID_EVENT_HANDLE;
		}
		return *this;
	}

	Zenith_Subscription(const Zenith_Subscription&) = delete;
	Zenith_Subscription& operator=(const Zenith_Subscription&) = delete;

	bool IsValid() const { return m_uHandle != INVALID_EVENT_HANDLE; }
	Zenith_EventHandle GetHandle() const { return m_uHandle; }

	// Unsubscribe now and become inert. Idempotent: a no-op when already
	// invalid / moved-from, and Unsubscribe itself tolerates a stale handle.
	void Reset()
	{
		if (m_uHandle != INVALID_EVENT_HANDLE)
		{
			Zenith_EventDispatcher::Get().Unsubscribe(m_uHandle);
			m_uHandle = INVALID_EVENT_HANDLE;
		}
	}

private:
	Zenith_EventHandle m_uHandle = INVALID_EVENT_HANDLE;
};

//------------------------------------------------------------------------------
// Template implementations
//------------------------------------------------------------------------------

template<typename TEvent>
Zenith_Subscription Zenith_EventDispatcher::SubscribeScoped(void(*pfnCallback)(const TEvent&))
{
	return Zenith_Subscription(Subscribe<TEvent>(pfnCallback));
}

template<typename TEvent, typename TCallback>
Zenith_Subscription Zenith_EventDispatcher::SubscribeScoped(TCallback&& xCallback)
{
	return Zenith_Subscription(Subscribe<TEvent>(std::forward<TCallback>(xCallback)));
}

template<typename TEvent>
void Zenith_EventWrapper<TEvent>::Dispatch(Zenith_EventDispatcher& xDispatcher) const
{
	xDispatcher.Dispatch(m_xEvent);
}

template<typename TEvent>
Zenith_EventHandle Zenith_EventDispatcher::Subscribe(void(*pfnCallback)(const TEvent&))
{
	Zenith_Assert(Zenith_ECS_IsMainThread(), "Subscribe must be called from main thread");
	const u_int uEventTypeID = Zenith_EventTypeID::GetID<TEvent>();
	const Zenith_EventHandle uHandle = m_uNextHandle++;

	Subscription xSub;
	xSub.m_uEventTypeID = uEventTypeID;
	xSub.m_pxCallback = new Zenith_CallbackWrapper<TEvent>(pfnCallback);

	m_xSubscriptions.Insert(uHandle, std::move(xSub));
	m_xSubscribersByEventType[uEventTypeID].PushBack(uHandle);

	return uHandle;
}

template<typename TEvent, typename TCallback>
Zenith_EventHandle Zenith_EventDispatcher::Subscribe(TCallback&& xCallback)
{
	static_assert(!std::is_same_v<std::decay_t<TCallback>, std::function<void(const TEvent&)>>,
		"std::function is forbidden - use a function pointer or lambda");
	Zenith_Assert(Zenith_ECS_IsMainThread(), "Subscribe must be called from main thread");
	const u_int uEventTypeID = Zenith_EventTypeID::GetID<TEvent>();
	const Zenith_EventHandle uHandle = m_uNextHandle++;

	Subscription xSub;
	xSub.m_uEventTypeID = uEventTypeID;
	xSub.m_pxCallback = new Zenith_LambdaCallbackWrapper<TEvent, std::decay_t<TCallback>>(
		std::forward<TCallback>(xCallback));

	m_xSubscriptions.Insert(uHandle, std::move(xSub));
	m_xSubscribersByEventType[uEventTypeID].PushBack(uHandle);

	return uHandle;
}

template<typename TEvent>
void Zenith_EventDispatcher::Dispatch(const TEvent& xEvent)
{
	const u_int uEventTypeID = Zenith_EventTypeID::GetID<TEvent>();

	const Zenith_Vector<Zenith_EventHandle>* pxLiveHandles = m_xSubscribersByEventType.TryGet(uEventTypeID);
	if (pxLiveHandles == nullptr)
	{
		return;
	}

	m_bDispatching = true;

	// Snapshot handle vector before iterating - callbacks may Subscribe to this same event
	// type, which would PushBack to the live vector and potentially reallocate it
	Zenith_Vector<Zenith_EventHandle> axHandles;
	axHandles.Reserve(pxLiveHandles->GetSize());
	for (u_int u = 0; u < pxLiveHandles->GetSize(); ++u)
	{
		axHandles.PushBack(pxLiveHandles->Get(u));
	}

	const u_int uCount = axHandles.GetSize();
	for (u_int uIdx = 0; uIdx < uCount; uIdx++)
	{
		// Skip callbacks that were unsubscribed during this dispatch
		bool bPendingUnsubscribe = false;
		for (u_int p = 0; p < m_axPendingUnsubscribes.GetSize(); ++p)
		{
			if (m_axPendingUnsubscribes.Get(p) == axHandles.Get(uIdx))
			{
				bPendingUnsubscribe = true;
				break;
			}
		}
		if (bPendingUnsubscribe) continue;

		if (Subscription* pxSub = m_xSubscriptions.TryGet(axHandles.Get(uIdx)))
		{
			pxSub->m_pxCallback->Invoke(&xEvent);
		}
	}

	m_bDispatching = false;

	// Process any unsubscribes that were deferred during dispatch
	for (u_int uIdx = 0; uIdx < m_axPendingUnsubscribes.GetSize(); uIdx++)
	{
		Unsubscribe(m_axPendingUnsubscribes.Get(uIdx));
	}
	m_axPendingUnsubscribes.Clear();
}

template<typename TEvent>
void Zenith_EventDispatcher::QueueEvent(const TEvent& xEvent)
{
	m_xDeferredMutex.Lock();
	m_axDeferredEvents.PushBack(new Zenith_EventWrapper<TEvent>(xEvent));
	m_xDeferredMutex.Unlock();
}

template<typename TEvent>
u_int Zenith_EventDispatcher::GetSubscriberCount() const
{
	const u_int uEventTypeID = Zenith_EventTypeID::GetID<TEvent>();
	const Zenith_Vector<Zenith_EventHandle>* pxHandles = m_xSubscribersByEventType.TryGet(uEventTypeID);
	return pxHandles ? pxHandles->GetSize() : 0;
}
