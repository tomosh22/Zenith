#include "Zenith.h"
#include "Zenith_EventSystem.h"

//------------------------------------------------------------------------------
// Zenith_EventDispatcher Implementation
//------------------------------------------------------------------------------

Zenith_EventDispatcher& Zenith_EventDispatcher::Get()
{
	static Zenith_EventDispatcher s_xInstance;
	return s_xInstance;
}

void Zenith_EventDispatcher::Unsubscribe(Zenith_EventHandle uHandle)
{
	auto xIt = m_xSubscriptions.find(uHandle);
	if (xIt == m_xSubscriptions.end())
	{
		return;
	}

	const u_int uEventTypeID = xIt->second.m_uEventTypeID;

	// Remove from subscribers list
	auto xTypeIt = m_xSubscribersByEventType.find(uEventTypeID);
	if (xTypeIt != m_xSubscribersByEventType.end())
	{
		auto& xHandles = xTypeIt->second;
		xHandles.erase(
			std::remove(xHandles.begin(), xHandles.end(), uHandle),
			xHandles.end());
	}

	// Remove subscription
	m_xSubscriptions.erase(xIt);
}

void Zenith_EventDispatcher::ProcessDeferredEvents()
{
	// Swap the deferred events vector to minimize lock time
	std::vector<std::unique_ptr<Zenith_EventBase>> xEventsToProcess;
	{
		std::lock_guard<std::mutex> xLock(m_xDeferredMutex);
		xEventsToProcess = std::move(m_xDeferredEvents);
		m_xDeferredEvents.clear();
	}

	// Process events without holding the lock
	for (const auto& pxEvent : xEventsToProcess)
	{
		pxEvent->Dispatch(*this);
	}
}

void Zenith_EventDispatcher::ClearAllSubscriptions()
{
	m_xSubscriptions.clear();
	m_xSubscribersByEventType.clear();

	// Also clear deferred events
	std::lock_guard<std::mutex> xLock(m_xDeferredMutex);
	m_xDeferredEvents.clear();
}
