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
	// Defer unsubscribe if we're currently dispatching to avoid modifying vectors during iteration
	if (m_bDispatching)
	{
		m_axPendingUnsubscribes.PushBack(uHandle);
		return;
	}

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
		xTypeIt->second.EraseValue(uHandle);
	}

	// Remove subscription
	m_xSubscriptions.erase(xIt);
}

void Zenith_EventDispatcher::ProcessDeferredEvents()
{
	// Swap the deferred events vector to minimize lock time
	Zenith_Vector<Zenith_EventBase*> axEventsToProcess;
	m_xDeferredMutex.Lock();
	axEventsToProcess = std::move(m_axDeferredEvents);
	m_axDeferredEvents.Clear();
	m_xDeferredMutex.Unlock();

	// Process events without holding the lock
	for (u_int uIdx = 0; uIdx < axEventsToProcess.GetSize(); uIdx++)
	{
		axEventsToProcess.Get(uIdx)->Dispatch(*this);
		delete axEventsToProcess.Get(uIdx);
	}
}

void Zenith_EventDispatcher::ClearAllSubscriptions()
{
	m_xSubscriptions.clear();
	m_xSubscribersByEventType.clear();

	// Also clear deferred events
	m_xDeferredMutex.Lock();
	for (u_int uIdx = 0; uIdx < m_axDeferredEvents.GetSize(); uIdx++)
	{
		delete m_axDeferredEvents.Get(uIdx);
	}
	m_axDeferredEvents.Clear();
	m_xDeferredMutex.Unlock();
}
