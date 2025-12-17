#pragma once

template <typename T, u_int uCount>
class Zenith_MemoryPool
{
public:

	Zenith_MemoryPool()
	: m_pxData(static_cast<T*>(Zenith_MemoryManagement::Allocate(uCount * sizeof(T))))
	, m_uFreeCount(uCount)
	{
		for (u_int u = 0; u < uCount; u++)
		{
			m_apxFreeList[u] = m_pxData + u;
		}
	}

	~Zenith_MemoryPool()
	{
		for (u_int u = m_uFreeCount; u < uCount; u++)
		{
			m_apxFreeList[u]->~T();
		}
		Zenith_MemoryManagement::Deallocate(m_pxData);
	}

	template<typename... Args>
	T* Allocate(Args&& ... args)
	{
		Zenith_Assert(m_uFreeCount > 0, "Memory pool is out of entries");

		#include "Memory/Zenith_MemoryManagement_Disabled.h"
		return (new (m_apxFreeList[--m_uFreeCount]) T(std::forward<Args>(args)...));
		#include "Memory/Zenith_MemoryManagement_Enabled.h"
	}

	void Deallocate(T* const pxVal)
	{
		Zenith_Assert(pxVal >= m_pxData && pxVal < m_pxData + uCount, "Object wasn't allocated from this pool");
		Zenith_Assert(m_uFreeCount < uCount, "Memory pool free list overflow - possible double-free");

		pxVal->~T();
		m_apxFreeList[m_uFreeCount++] = pxVal;
	}

	Zenith_MemoryPool(const Zenith_MemoryPool&) = delete;
	Zenith_MemoryPool& operator=(const Zenith_MemoryPool&) = delete;

private:
	T* m_pxData = nullptr;
	T* m_apxFreeList[uCount]{nullptr};
	u_int m_uFreeCount = 0; // Initialized properly in constructor
};
