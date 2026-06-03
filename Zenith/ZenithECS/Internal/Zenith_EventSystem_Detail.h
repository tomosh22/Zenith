#pragma once

#include <type_traits>
#include <utility>

//------------------------------------------------------------------------------
// Zenith_EventSystem_Detail - type-erasure machinery for Zenith_EventDispatcher
//------------------------------------------------------------------------------
//
// PRIVATE implementation detail of Zenith_EventSystem.h. No engine/game/tool file
// includes this directly. These types are template-coupled to the inline
// Subscribe / Dispatch / QueueEvent bodies of Zenith_EventDispatcher, so the
// public header pulls this in (near its top) BEFORE declaring the dispatcher.
//
// Contents:
//   - Zenith_EventTypeID          : per-type stable event-id counter
//   - Zenith_EventBase            : type-erased event base for the deferred queue
//   - Zenith_EventWrapper<T>      : concrete deferred-queue event holder
//   - Zenith_CallbackBase         : type-erased callback base
//   - Zenith_CallbackWrapper<T>   : function-pointer callback holder
//   - Zenith_LambdaCallbackWrapper<T,C> : capturing-lambda / functor callback holder
//------------------------------------------------------------------------------

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
