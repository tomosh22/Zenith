#pragma once

#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include "EntityComponent/Zenith_Scene.h"

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
	Zenith_Scene::TypeID m_uComponentTypeID = 0;
};

struct Zenith_Event_ComponentRemoved
{
	Zenith_EntityID m_uEntityID = INVALID_ENTITY_ID;
	Zenith_Scene::TypeID m_uComponentTypeID = 0;
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
		std::unique_ptr<Zenith_CallbackBase> m_pxCallback;
	};

	std::unordered_map<Zenith_EventHandle, Subscription> m_xSubscriptions;
	std::unordered_map<u_int, std::vector<Zenith_EventHandle>> m_xSubscribersByEventType;

	std::vector<std::unique_ptr<Zenith_EventBase>> m_xDeferredEvents;
	std::mutex m_xDeferredMutex;

	Zenith_EventHandle m_uNextHandle = 1;
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
	const u_int uEventTypeID = Zenith_EventTypeID::GetID<TEvent>();
	const Zenith_EventHandle uHandle = m_uNextHandle++;

	Subscription xSub;
	xSub.m_uEventTypeID = uEventTypeID;
	xSub.m_pxCallback = std::make_unique<Zenith_CallbackWrapper<TEvent>>(pfnCallback);

	m_xSubscriptions[uHandle] = std::move(xSub);
	m_xSubscribersByEventType[uEventTypeID].push_back(uHandle);

	return uHandle;
}

template<typename TEvent, typename TCallback>
Zenith_EventHandle Zenith_EventDispatcher::SubscribeLambda(TCallback&& xCallback)
{
	const u_int uEventTypeID = Zenith_EventTypeID::GetID<TEvent>();
	const Zenith_EventHandle uHandle = m_uNextHandle++;

	Subscription xSub;
	xSub.m_uEventTypeID = uEventTypeID;
	xSub.m_pxCallback = std::make_unique<Zenith_LambdaCallbackWrapper<TEvent, std::decay_t<TCallback>>>(
		std::forward<TCallback>(xCallback));

	m_xSubscriptions[uHandle] = std::move(xSub);
	m_xSubscribersByEventType[uEventTypeID].push_back(uHandle);

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

	for (const Zenith_EventHandle uHandle : xIt->second)
	{
		auto xSubIt = m_xSubscriptions.find(uHandle);
		if (xSubIt != m_xSubscriptions.end())
		{
			xSubIt->second.m_pxCallback->Invoke(&xEvent);
		}
	}
}

template<typename TEvent>
void Zenith_EventDispatcher::QueueEvent(const TEvent& xEvent)
{
	std::lock_guard<std::mutex> xLock(m_xDeferredMutex);
	m_xDeferredEvents.push_back(std::make_unique<Zenith_EventWrapper<TEvent>>(xEvent));
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
	return static_cast<u_int>(xIt->second.size());
}
