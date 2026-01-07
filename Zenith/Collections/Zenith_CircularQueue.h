#pragma once

/**
 * Zenith_CircularQueue - Fixed-capacity FIFO queue
 *
 * THREAD SAFETY: This container is NOT thread-safe.
 * All operations must be synchronized externally when accessed from multiple threads.
 *
 * DO NOT use IsFull()/IsEmpty()/GetSize() for flow control without holding a lock.
 * These methods have TOCTOU (time-of-check-to-time-of-use) issues in concurrent code.
 * Always use Enqueue/Dequeue return values under lock for correctness.
 *
 * Example usage with external synchronization:
 *   mutex.Lock();
 *   bool bSuccess = queue.Enqueue(item);
 *   mutex.Unlock();
 */
template<typename T, u_int uCapacity>
class Zenith_CircularQueue
{
	static_assert(uCapacity > 0, "CircularQueue capacity must be at least 1");

public:

	Zenith_CircularQueue()
	{
		m_uCurrentSize = 0;
		m_uFront = 0;
	}

	bool Enqueue(const T& tAdd)
	{
		Zenith_Assert(m_uCurrentSize <= uCapacity, "CircularQueue: Size exceeds capacity - corruption detected");
		if (m_uCurrentSize == uCapacity) return false;

		// CRITICAL FIX: Safe modulo arithmetic to prevent integer overflow
		// Since m_uFront and m_uCurrentSize are both < uCapacity after their modulos,
		// their sum is guaranteed < 2*uCapacity, making the final modulo safe
		// This avoids overflow when m_uFront + m_uCurrentSize > UINT_MAX
		u_int uFrontPos = m_uFront % uCapacity;  // Always < uCapacity
		u_int uAddOffset = m_uCurrentSize % uCapacity;  // Always < uCapacity
		// uFrontPos + uAddOffset is now guaranteed < 2*uCapacity, no overflow possible
		u_int uIndex = (uFrontPos + uAddOffset) % uCapacity;

		m_atContents[uIndex] = tAdd;
		m_uCurrentSize++;
		return true;
	}

	bool Dequeue(T& tOut)
	{
		Zenith_Assert(m_uFront < uCapacity, "CircularQueue: Front index out of bounds - corruption detected");
		if (m_uCurrentSize == 0) return false;

		tOut = std::move(m_atContents[m_uFront]);  // Move for efficiency
		m_atContents[m_uFront].~T();  // Destroy to release resources for non-POD types
		new (&m_atContents[m_uFront]) T();  // Reconstruct to valid state
		m_uFront = (m_uFront + 1) % uCapacity;
		m_uCurrentSize--;

		return true;
	}

	// Inspection methods
	u_int GetSize() const { return m_uCurrentSize; }
	u_int GetCapacity() const { return uCapacity; }
	bool IsEmpty() const { return m_uCurrentSize == 0; }
	bool IsFull() const { return m_uCurrentSize == uCapacity; }

	// Peek at front element without removing it
	bool Peek(T& tOut) const
	{
		if (m_uCurrentSize == 0) return false;
		tOut = m_atContents[m_uFront];
		return true;
	}

	// Clear all elements - properly destroys objects for non-POD types
	void Clear()
	{
		// Call destructor on each element, then placement-new default construct
		// This properly releases resources held by non-POD types
		for (u_int u = 0; u < m_uCurrentSize; u++)
		{
			u_int uIdx = (m_uFront + u) % uCapacity;
			m_atContents[uIdx].~T();
			new (&m_atContents[uIdx]) T();  // Reconstruct in valid state for reuse
		}
		m_uCurrentSize = 0;
		m_uFront = 0;
	}

private:
	T m_atContents[uCapacity];
	u_int m_uCurrentSize = 0;
	u_int m_uFront = 0;
};