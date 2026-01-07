#pragma once

#include "Core/Multithreading/Zenith_Multithreading.h"

/**
 * Zenith_MemoryPool - Fixed-size pool allocator for type T
 *
 * THREAD SAFETY: All public methods are protected by an internal mutex.
 * Safe for concurrent Allocate/Deallocate calls from multiple threads.
 *
 * Features:
 * - O(1) allocation and deallocation
 * - No fragmentation (fixed-size blocks)
 * - Double-free detection via allocation tracking
 * - Pool exhaustion detection
 */
template <typename T, u_int uCount>
class Zenith_MemoryPool
{
	static_assert(uCount > 0, "Memory pool must have at least 1 entry");

public:

	Zenith_MemoryPool()
	: m_pxData(static_cast<T*>(Zenith_MemoryManagement::Allocate(uCount * sizeof(T))))
	, m_uFreeCount(uCount)
	{
		Zenith_Assert(m_pxData != nullptr, "MemoryPool: Failed to allocate pool storage");
		for (u_int u = 0; u < uCount; u++)
		{
			m_apxFreeList[u] = m_pxData + u;
			m_abAllocated[u] = false;
		}
	}

	~Zenith_MemoryPool()
	{
		// Destroy all allocated objects using the tracking array
		// (Can't use free list indices - they don't track allocated slots correctly after deallocs)
		for (u_int u = 0; u < uCount; u++)
		{
			if (m_abAllocated[u])
			{
				(m_pxData + u)->~T();
			}
		}
		Zenith_MemoryManagement::Deallocate(m_pxData);
	}

	template<typename... Args>
	T* Allocate(Args&& ... args)
	{
		Zenith_ScopedMutexLock xLock(m_xMutex);

		// Return nullptr on pool exhaustion instead of crashing
		if (m_uFreeCount == 0)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "MemoryPool::Allocate: Pool exhausted (capacity=%u)", uCount);
			return nullptr;
		}

		T* pxSlot = m_apxFreeList[--m_uFreeCount];
		u_int uIndex = static_cast<u_int>(pxSlot - m_pxData);
		Zenith_Assert(!m_abAllocated[uIndex], "Memory pool slot already allocated - corruption detected");
		m_abAllocated[uIndex] = true;

		#include "Memory/Zenith_MemoryManagement_Disabled.h"
		return (new (pxSlot) T(std::forward<Args>(args)...));
		#include "Memory/Zenith_MemoryManagement_Enabled.h"
	}

	void Deallocate(T* const pxVal)
	{
		Zenith_ScopedMutexLock xLock(m_xMutex);

		// Null pointer check
		Zenith_Assert(pxVal != nullptr, "MemoryPool::Deallocate: Attempted to deallocate null pointer");
		if (pxVal == nullptr) return;

		Zenith_Assert(pxVal >= m_pxData && pxVal < m_pxData + uCount,
			"MemoryPool::Deallocate: Object at %p wasn't allocated from this pool (range %p-%p)",
			static_cast<const void*>(pxVal), static_cast<const void*>(m_pxData), static_cast<const void*>(m_pxData + uCount));
		Zenith_Assert(m_uFreeCount < uCount, "Memory pool free list overflow - possible double-free");

		u_int uIndex = static_cast<u_int>(pxVal - m_pxData);
		Zenith_Assert(m_abAllocated[uIndex], "Memory pool slot not allocated - possible double-free");
		m_abAllocated[uIndex] = false;

		pxVal->~T();
		m_apxFreeList[m_uFreeCount++] = pxVal;
	}

	// Query methods (thread-safe)
	u_int GetFreeCount() const
	{
		Zenith_ScopedMutexLock xLock(m_xMutex);
		return m_uFreeCount;
	}

	u_int GetAllocatedCount() const
	{
		Zenith_ScopedMutexLock xLock(m_xMutex);
		return uCount - m_uFreeCount;
	}

	u_int GetCapacity() const { return uCount; }

	bool IsFull() const
	{
		Zenith_ScopedMutexLock xLock(m_xMutex);
		return m_uFreeCount == 0;
	}

	bool IsEmpty() const
	{
		Zenith_ScopedMutexLock xLock(m_xMutex);
		return m_uFreeCount == uCount;
	}

	// Check if a pointer was allocated from this pool (thread-safe)
	bool OwnsPointer(const T* pxVal) const
	{
		return pxVal >= m_pxData && pxVal < m_pxData + uCount;
	}

	Zenith_MemoryPool(const Zenith_MemoryPool&) = delete;
	Zenith_MemoryPool& operator=(const Zenith_MemoryPool&) = delete;

private:
	T* m_pxData = nullptr;
	T* m_apxFreeList[uCount]{nullptr};
	bool m_abAllocated[uCount]{false};
	u_int m_uFreeCount = 0;
	mutable Zenith_Mutex m_xMutex;
};
