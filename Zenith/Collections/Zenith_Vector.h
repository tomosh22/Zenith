#pragma once
#include "DataStream/Zenith_DataStream.h"

template<typename T>
class Zenith_Vector
{
public:
	Zenith_Vector() : Zenith_Vector(uDEFAULT_INITIAL_COUNT) {}

	explicit Zenith_Vector(u_int uCapacity)
	: m_pxData(nullptr)
	, m_uSize(0)
	, m_uCapacity(uCapacity)
	{
		// Check for integer overflow in size calculation
		if (uCapacity > 0)
		{
			constexpr u_int uMAX_SAFE_CAPACITY = UINT_MAX / sizeof(T);
			Zenith_Assert(uCapacity <= uMAX_SAFE_CAPACITY,
				"Vector capacity %u would overflow when multiplied by sizeof(T)=%zu", uCapacity, sizeof(T));

			m_pxData = static_cast<T*>(Zenith_MemoryManagement::Allocate(uCapacity * sizeof(T)));
			Zenith_Assert(m_pxData != nullptr,
				"Vector allocation failed for capacity %u (requested %zu bytes)", uCapacity, uCapacity * sizeof(T));
		}
	}

	Zenith_Vector(const Zenith_Vector& xOther)
	: m_pxData(nullptr), m_uSize(0), m_uCapacity(0)
	{
		CopyFromOther(xOther);
	}

	Zenith_Vector(Zenith_Vector&& xOther)
	: m_pxData(xOther.m_pxData)
	, m_uSize(xOther.m_uSize)
	, m_uCapacity(xOther.m_uCapacity)
	{
		xOther.m_pxData = nullptr;
		xOther.m_uSize = 0;
		xOther.m_uCapacity = 0;
	}

	Zenith_Vector& operator=(const Zenith_Vector& xOther)
	{
		if (this == &xOther) return *this;  // Self-assignment check - prevents use-after-free
		Clear();
		Zenith_MemoryManagement::Deallocate(m_pxData);
		CopyFromOther(xOther);
		return *this;
	}

	Zenith_Vector& operator=(Zenith_Vector&& xOther)
	{
		if (this == &xOther) return *this;  // Self-assignment check - prevents use-after-free
		Clear();
		Zenith_MemoryManagement::Deallocate(m_pxData);

		m_pxData = xOther.m_pxData;
		m_uSize = xOther.m_uSize;
		m_uCapacity = xOther.m_uCapacity;

		xOther.m_pxData = nullptr;
		xOther.m_uSize = 0;
		xOther.m_uCapacity = 0;

		return *this;
	}

	~Zenith_Vector()
	{
		Clear();
		Zenith_MemoryManagement::Deallocate(m_pxData);
	}

	u_int GetSize() const {return m_uSize;}
	u_int GetCapacity() const {return m_uCapacity;}

	void PushBack(const T& xValue)
	{
		while (m_uSize >= m_uCapacity) Resize();

		#include "Memory/Zenith_MemoryManagement_Disabled.h"
		new (&m_pxData[m_uSize]) T(xValue);
		#include "Memory/Zenith_MemoryManagement_Enabled.h"
		m_uSize++;
	}

	void PushBack(T&& xValue)
	{
		while (m_uSize >= m_uCapacity) Resize();

		#include "Memory/Zenith_MemoryManagement_Disabled.h"
		new (&m_pxData[m_uSize]) T(std::move(xValue));
		#include "Memory/Zenith_MemoryManagement_Enabled.h"
		m_uSize++;
	}

	template<typename... Args>
	void EmplaceBack(Args&&... args)
	{
		while (m_uSize >= m_uCapacity) Resize();

		#include "Memory/Zenith_MemoryManagement_Disabled.h"
		new (&m_pxData[m_uSize]) T(std::forward<Args>(args)...);
		#include "Memory/Zenith_MemoryManagement_Enabled.h"
		m_uSize++;
	}

	T& Get(u_int uIndex)
	{
		Zenith_Assert(uIndex < m_uSize, "Index %u out of range (size=%u)", uIndex, m_uSize);
		return m_pxData[uIndex];
	}

	const T& Get(u_int uIndex) const
	{
		Zenith_Assert(uIndex < m_uSize, "Index %u out of range (size=%u)", uIndex, m_uSize);
		return m_pxData[uIndex];
	}

	T& GetFront()
	{
		Zenith_Assert(m_uSize > 0, "Vector is empty");
		return m_pxData[0];
	}

	const T& GetFront() const
	{
		Zenith_Assert(m_uSize > 0, "Vector is empty");
		return m_pxData[0];
	}

	T& GetBack()
	{
		Zenith_Assert(m_uSize > 0, "Vector is empty");
		return m_pxData[m_uSize - 1];
	}

	const T& GetBack() const
	{
		Zenith_Assert(m_uSize > 0, "Vector is empty");
		return m_pxData[m_uSize - 1];
	}

	T* GetDataPointer() { return m_pxData; }
	const T* GetDataPointer() const { return m_pxData; }

	u_int Find(const T& xValue) const
	{
		for (u_int u = 0; u < m_uSize; u++)
		{
			if (m_pxData[u] == xValue) return u;
		}
		return m_uSize;
	}

	template<typename Predicate>
	u_int FindIf(Predicate pfnPredicate) const
	{
		for (u_int u = 0; u < m_uSize; u++)
		{
			if (pfnPredicate(m_pxData[u])) return u;
		}
		return m_uSize;
	}

	bool Contains(const T& xValue) const { return Find(xValue) != m_uSize; }

	bool Erase(u_int uIndex)
	{
		if (uIndex >= m_uSize) return false;
		Remove(uIndex);
		return true;
	}

	bool EraseValue(const T& xValue)
	{
		u_int uIndex = Find(xValue);
		if (uIndex != m_uSize)
		{
			Remove(uIndex);
			return true;
		}
		return false;
	}

	// O(1) swap-and-pop removal - does NOT preserve order
	// Use when order doesn't matter for better performance
	void RemoveSwap(u_int uIndex)
	{
		Zenith_Assert(uIndex < m_uSize, "RemoveSwap: Index out of range");

		if (uIndex != m_uSize - 1)
		{
			// Move last element to removed position
			m_pxData[uIndex].~T();
			#include "Memory/Zenith_MemoryManagement_Disabled.h"
			new (&m_pxData[uIndex]) T(std::move(m_pxData[m_uSize - 1]));
			#include "Memory/Zenith_MemoryManagement_Enabled.h"
		}

		m_pxData[m_uSize - 1].~T();
		m_uSize--;
	}

	// O(1) swap-and-pop removal with bounds check - does NOT preserve order
	bool EraseSwap(u_int uIndex)
	{
		if (uIndex >= m_uSize) return false;
		RemoveSwap(uIndex);
		return true;
	}

	// O(n) find + O(1) swap-and-pop removal - does NOT preserve order
	bool EraseValueSwap(const T& xValue)
	{
		u_int uIndex = Find(xValue);
		if (uIndex != m_uSize)
		{
			RemoveSwap(uIndex);
			return true;
		}
		return false;
	}

	// O(n) removal that preserves order (existing behavior)
	void Remove(u_int uIndex)
	{
		Zenith_Assert(uIndex < m_uSize, "Index out of range");
		m_pxData[uIndex].~T();
		// Move remaining valid elements using move semantics for proper handling of non-trivial types
		for (u_int u = uIndex; u < m_uSize - 1; u++)
		{
			#include "Memory/Zenith_MemoryManagement_Disabled.h"
			new (&m_pxData[u]) T(std::move(m_pxData[u + 1]));
			#include "Memory/Zenith_MemoryManagement_Enabled.h"
			m_pxData[u + 1].~T();
		}
		m_uSize--;
	}

	void Clear()
	{
		for (u_int u = 0; u < m_uSize; u++)
		{
			m_pxData[u].~T();
		}
		m_uSize = 0;
	}

	void PopBack()
	{
		Zenith_Assert(m_uSize > 0, "Cannot pop from empty vector");
		m_pxData[m_uSize - 1].~T();
		m_uSize--;
	}

	void Reserve(u_int uNewCapacity)
	{
		if (uNewCapacity <= m_uCapacity) return;

		// Check for integer overflow in size calculation
		constexpr u_int uMAX_SAFE_CAPACITY = UINT_MAX / sizeof(T);
		Zenith_Assert(uNewCapacity <= uMAX_SAFE_CAPACITY,
			"Reserve: capacity %u would overflow when multiplied by sizeof(T)=%zu", uNewCapacity, sizeof(T));
		if (uNewCapacity > uMAX_SAFE_CAPACITY) return;

		// Invalidate any active iterators - buffer is being reallocated
		m_uGeneration++;

		// Allocate new buffer and move construct existing elements for proper handling of non-trivial types
		T* pNewData = static_cast<T*>(Zenith_MemoryManagement::Allocate(uNewCapacity * sizeof(T)));
		Zenith_Assert(pNewData != nullptr,
			"Reserve: allocation failed for capacity %u (requested %zu bytes)", uNewCapacity, uNewCapacity * sizeof(T));
		if (pNewData == nullptr) return;  // Fail gracefully in release builds

		for (u_int u = 0; u < m_uSize; u++)
		{
			#include "Memory/Zenith_MemoryManagement_Disabled.h"
			new (&pNewData[u]) T(std::move(m_pxData[u]));
			#include "Memory/Zenith_MemoryManagement_Enabled.h"
			m_pxData[u].~T();
		}
		Zenith_MemoryManagement::Deallocate(m_pxData);
		m_pxData = pNewData;
		m_uCapacity = uNewCapacity;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uSize;
		xStream >> uSize;

		// Sanity check to prevent allocation of absurd sizes from corrupted data
		constexpr u_int uMAX_REASONABLE_SIZE = 100000000;
		Zenith_Assert(uSize <= uMAX_REASONABLE_SIZE,
			"ReadFromDataStream: Size %u exceeds reasonable limit - possible data corruption", uSize);
		if (uSize > uMAX_REASONABLE_SIZE)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "ReadFromDataStream: Size %u exceeds limit, aborting", uSize);
			return;
		}

		// Check for integer overflow
		constexpr u_int uMAX_SAFE_CAPACITY = UINT_MAX / sizeof(T);
		if (uSize > uMAX_SAFE_CAPACITY)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "ReadFromDataStream: Size %u would overflow allocation", uSize);
			return;
		}

		Clear();
		Reserve(uSize);

		// Verify Reserve succeeded
		if (uSize > 0 && GetCapacity() < uSize)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "ReadFromDataStream: Reserve failed (capacity=%u, needed=%u)", GetCapacity(), uSize);
			return;
		}

		for (u_int u = 0; u < uSize; u++)
		{
			// Check stream has remaining data
			if (xStream.GetCursor() >= xStream.GetSize())
			{
				Zenith_Error(LOG_CATEGORY_CORE, "ReadFromDataStream: Premature end of stream at element %u of %u", u, uSize);
				break;
			}
			T x;
			xStream >> x;
			PushBack(x);
		}
	}

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << m_uSize;
		for (Iterator xIt(*this); !xIt.Done(); xIt.Next())
		{
			xStream << xIt.GetData();
		}
	}

	class Iterator
	{
	public:
		explicit Iterator(const Zenith_Vector& xVec)
		: m_xVec(xVec)
		, m_uIndex(0)
		, m_uGeneration(xVec.m_uGeneration)
		{}

		void Next()
		{
			Zenith_Assert(m_uGeneration == m_xVec.m_uGeneration,
				"Iterator invalidated: vector was reallocated during iteration");
			Zenith_Assert(m_uIndex < m_xVec.GetSize(), "Iterated past end of vector");
			m_uIndex++;
		}

		bool Done() const
		{
			Zenith_Assert(m_uGeneration == m_xVec.m_uGeneration,
				"Iterator invalidated: vector was reallocated during iteration");
			return m_uIndex == m_xVec.GetSize();
		}

		const T& GetData() const
		{
			Zenith_Assert(m_uGeneration == m_xVec.m_uGeneration,
				"Iterator invalidated: vector was reallocated during iteration");
			return m_xVec.Get(m_uIndex);
		}

	private:
		const Zenith_Vector<T>& m_xVec;
		u_int m_uIndex;
		u_int m_uGeneration;
	};

private:
	static constexpr u_int uDEFAULT_INITIAL_COUNT = 8;

	void Resize()
	{
		u_int uNewCapacity;
		if (m_uCapacity == 0)
		{
			uNewCapacity = uDEFAULT_INITIAL_COUNT;
		}
		else if (m_uCapacity > UINT_MAX / 2)
		{
			// Prevent overflow - try to allocate max safe capacity
			constexpr u_int uMAX_SAFE_CAPACITY = UINT_MAX / sizeof(T);
			uNewCapacity = uMAX_SAFE_CAPACITY;
			Zenith_Assert(uNewCapacity > m_uCapacity, "Resize: cannot grow vector further - at maximum capacity");
		}
		else
		{
			uNewCapacity = m_uCapacity * 2;
		}
		Reserve(uNewCapacity);
	}

	void CopyFromOther(const Zenith_Vector& xOther)
	{
		m_uCapacity = xOther.GetCapacity();
		m_uSize = xOther.GetSize();

		if (m_uCapacity == 0)
		{
			m_pxData = nullptr;
			return;
		}

		// Check for overflow
		constexpr u_int uMAX_SAFE_CAPACITY = UINT_MAX / sizeof(T);
		Zenith_Assert(m_uCapacity <= uMAX_SAFE_CAPACITY,
			"CopyFromOther: capacity %u would overflow", m_uCapacity);

		m_pxData = static_cast<T*>(Zenith_MemoryManagement::Allocate(m_uCapacity * sizeof(T)));
		Zenith_Assert(m_pxData != nullptr,
			"CopyFromOther: allocation failed for capacity %u", m_uCapacity);
		if (m_pxData == nullptr)
		{
			m_uSize = 0;
			m_uCapacity = 0;
			return;
		}

		// Use placement new with copy construction for proper handling of non-trivial types (e.g. std::string)
		for (u_int u = 0; u < m_uSize; u++)
		{
			#include "Memory/Zenith_MemoryManagement_Disabled.h"
			new (&m_pxData[u]) T(xOther.m_pxData[u]);
			#include "Memory/Zenith_MemoryManagement_Enabled.h"
		}
	}

	T* m_pxData = nullptr;
	u_int m_uSize = 0;
	u_int m_uCapacity = 0;
	u_int m_uGeneration = 0;  // Incremented on reallocation to detect iterator invalidation
};
