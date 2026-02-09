#pragma once

#include <unordered_map>
#include "Collections/Zenith_Vector.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "EntityComponent/Zenith_SceneData.h"

//------------------------------------------------------------------------------
// Zenith_EventSystem - Type-safe event dispatcher for ECS events
//------------------------------------------------------------------------------
//
// Usage:
//   // Subscribe to an event
//   auto uHandle = Zenith_EventDispatcher::Get().Subscribe<Zenith_Event_EntityCreated>(
//       [](const Zenith_Event_EntityCreated& xEvent) {
//           // Handle event
//       });
//
//   // Dispatch an event immediately
//   Zenith_EventDispatcher::Get().Dispatch(Zenith_Event_EntityCreated{ uEntityID });
//
//   // Queue an event for deferred processing (thread-safe)
//   Zenith_EventDispatcher::Get().QueueEvent(Zenith_Event_EntityCreated{ uEntityID });
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
// Built-in ECS Events
//------------------------------------------------------------------------------

struct Zenith_Event_EntityCreated
{
	Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
};

struct Zenith_Event_EntityDestroyed
{
	Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
};

struct Zenith_Event_ComponentAdded
{
	Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
	Zenith_SceneData::TypeID m_uComponentTypeID = 0;
};

struct Zenith_Event_ComponentRemoved
{
	Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
	Zenith_SceneData::TypeID m_uComponentTypeID = 0;
};

//------------------------------------------------------------------------------
// Event Type ID Generator
//------------------------------------------------------------------------------

class Zenith_EventTypeID
{
public:
	template<typename TEvent>
	static u_int GetID()
	{
		static u_int ls_uID = s_uCounter++;
		return ls_uID;
	}

private:
	inline static u_int s_uCounter = 0;
};

//------------------------------------------------------------------------------
// Type-erased event storage for deferred queue
//------------------------------------------------------------------------------

class Zenith_EventBase
{
public:
	virtual ~Zenith_EventBase() = default;
	virtual u_int GetTypeID() const = 0;
	virtual void Dispatch(class Zenith_EventDispatcher& xDispatcher) const = 0;
};

template<typename TEvent>
class Zenith_EventWrapper : public Zenith_EventBase
{
public:
	explicit Zenith_EventWrapper(const TEvent& xEvent) : m_xEvent(xEvent) {}

	u_int GetTypeID() const override
	{
		return Zenith_EventTypeID::GetID<TEvent>();
	}

	void Dispatch(class Zenith_EventDispatcher& xDispatcher) const override;

	TEvent m_xEvent;
};

//------------------------------------------------------------------------------
// Callback wrapper base for type-erased storage
//------------------------------------------------------------------------------

class Zenith_CallbackBase
{
public:
	virtual ~Zenith_CallbackBase() = default;
	virtual void Invoke(const void* pEvent) = 0;
};

template<typename TEvent>
class Zenith_CallbackWrapper : public Zenith_CallbackBase
{
public:
	using CallbackFn = void(*)(const TEvent&);

	explicit Zenith_CallbackWrapper(CallbackFn pfnCallback)
		: m_pfnCallback(pfnCallback)
	{
	}

	void Invoke(const void* pEvent) override
	{
		m_pfnCallback(*static_cast<const TEvent*>(pEvent));
	}

private:
	CallbackFn m_pfnCallback;
};

// Wrapper for lambda/std::function callbacks (for convenience)
template<typename TEvent, typename TCallback>
class Zenith_LambdaCallbackWrapper : public Zenith_CallbackBase
{
public:
	explicit Zenith_LambdaCallbackWrapper(TCallback&& xCallback)
		: m_xCallback(std::forward<TCallback>(xCallback))
	{
	}

	void Invoke(const void* pEvent) override
	{
		m_xCallback(*static_cast<const TEvent*>(pEvent));
	}

private:
	TCallback m_xCallback;
};

//------------------------------------------------------------------------------
// Event Dispatcher (Singleton)
//------------------------------------------------------------------------------

class Zenith_EventDispatcher
{
public:
	static Zenith_EventDispatcher& Get();

	// Subscribe with a function pointer
	template<typename TEvent>
	Zenith_EventHandle Subscribe(void(*pfnCallback)(const TEvent&));

	// Subscribe with a lambda or callable object
	template<typename TEvent, typename TCallback>
	Zenith_EventHandle SubscribeLambda(TCallback&& xCallback);

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

	// #TODO: Replace std::unordered_map with engine hash map when available
	std::unordered_map<Zenith_EventHandle, Subscription> m_xSubscriptions;
	std::unordered_map<u_int, Zenith_Vector<Zenith_EventHandle>> m_xSubscribersByEventType;

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
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "Subscribe must be called from main thread");
	const u_int uEventTypeID = Zenith_EventTypeID::GetID<TEvent>();
	const Zenith_EventHandle uHandle = m_uNextHandle++;

	Subscription xSub;
	xSub.m_uEventTypeID = uEventTypeID;
	xSub.m_pxCallback = new Zenith_CallbackWrapper<TEvent>(pfnCallback);

	m_xSubscriptions[uHandle] = std::move(xSub);
	m_xSubscribersByEventType[uEventTypeID].PushBack(uHandle);

	return uHandle;
}

template<typename TEvent, typename TCallback>
Zenith_EventHandle Zenith_EventDispatcher::SubscribeLambda(TCallback&& xCallback)
{
	static_assert(!std::is_same_v<std::decay_t<TCallback>, std::function<void(const TEvent&)>>,
		"std::function is forbidden - use a function pointer or lambda");
	Zenith_Assert(Zenith_Multithreading::IsMainThread(), "SubscribeLambda must be called from main thread");
	const u_int uEventTypeID = Zenith_EventTypeID::GetID<TEvent>();
	const Zenith_EventHandle uHandle = m_uNextHandle++;

	Subscription xSub;
	xSub.m_uEventTypeID = uEventTypeID;
	xSub.m_pxCallback = new Zenith_LambdaCallbackWrapper<TEvent, std::decay_t<TCallback>>(
		std::forward<TCallback>(xCallback));

	m_xSubscriptions[uHandle] = std::move(xSub);
	m_xSubscribersByEventType[uEventTypeID].PushBack(uHandle);

	return uHandle;
}

template<typename TEvent>
void Zenith_EventDispatcher::Dispatch(const TEvent& xEvent)
{
	const u_int uEventTypeID = Zenith_EventTypeID::GetID<TEvent>();

	auto xIt = m_xSubscribersByEventType.find(uEventTypeID);
	if (xIt == m_xSubscribersByEventType.end())
	{
		return;
	}

	m_bDispatching = true;

	// Snapshot handle vector before iterating - callbacks may Subscribe to this same event
	// type, which would PushBack to the live vector and potentially reallocate it
	const Zenith_Vector<Zenith_EventHandle>& axLiveHandles = xIt->second;
	Zenith_Vector<Zenith_EventHandle> axHandles;
	axHandles.Reserve(axLiveHandles.GetSize());
	for (u_int u = 0; u < axLiveHandles.GetSize(); ++u)
	{
		axHandles.PushBack(axLiveHandles.Get(u));
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

		auto xSubIt = m_xSubscriptions.find(axHandles.Get(uIdx));
		if (xSubIt != m_xSubscriptions.end())
		{
			xSubIt->second.m_pxCallback->Invoke(&xEvent);
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
	auto xIt = m_xSubscribersByEventType.find(uEventTypeID);
	if (xIt == m_xSubscribersByEventType.end())
	{
		return 0;
	}
	return xIt->second.GetSize();
}
