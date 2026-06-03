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
// Template implementations
//------------------------------------------------------------------------------

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
