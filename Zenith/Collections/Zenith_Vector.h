#pragma once
#include "DataStream/Zenith_DataStream.h"

template<typename T>
class Zenith_Vector
{
public:
	Zenith_Vector() : Zenith_Vector(uDEFAULT_INITIAL_COUNT) {}

	explicit Zenith_Vector(u_int uCapacity)
	: m_pxData(static_cast<T*>(Zenith_MemoryManagement::Allocate(uCapacity * sizeof(T))))
	, m_uSize(0)
	, m_uCapacity(uCapacity)
	{}

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
		Clear();
		Zenith_MemoryManagement::Deallocate(m_pxData);
		CopyFromOther(xOther);
		return *this;
	}

	Zenith_Vector& operator=(Zenith_Vector&& xOther)
	{
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

	T& Get(u_int uIndex) const
	{
		Zenith_Assert(uIndex < m_uSize, "Index out of range");
		return m_pxData[uIndex];
	}

	T& GetFront() const
	{
		Zenith_Assert(m_uSize > 0, "Vector is empty");
		return m_pxData[0];
	}

	T& GetBack() const
	{
		Zenith_Assert(m_uSize > 0, "Vector is empty");
		return m_pxData[m_uSize - 1];
	}

	T* GetDataPointer() const {return m_pxData;}

	void Remove(u_int uIndex)
	{
		Zenith_Assert(uIndex < m_uSize, "Index out of range");
		m_pxData[uIndex].~T();
		// Move remaining valid elements (use m_uSize, not m_uCapacity)
		memmove(m_pxData + uIndex, m_pxData + uIndex + 1, sizeof(T) * (m_uSize - uIndex - 1));
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

	void Reserve(u_int uNewCapcacity)
	{
		if (uNewCapcacity <= m_uCapacity) return;

		m_pxData = static_cast<T*>(Zenith_MemoryManagement::Reallocate(m_pxData, sizeof(T) * uNewCapcacity));
		m_uCapacity = uNewCapcacity;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uSize;
		xStream >> uSize;
		Clear();
		Reserve(uSize);
		for (u_int u = 0; u < uSize; u++)
		{
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
		{}

		void Next()
		{
			Zenith_Assert(m_uIndex < m_xVec.GetSize() + 1, "Iterated too far");
			m_uIndex++;
		}

		bool Done() const {return m_uIndex == m_xVec.GetSize();}

		T& GetData() {return m_xVec.Get(m_uIndex);}

	private:
		const Zenith_Vector<T>& m_xVec;
		u_int m_uIndex;
	};

private:
	static constexpr u_int uDEFAULT_INITIAL_COUNT = 8;

	void Resize()
	{
		Reserve(m_uCapacity * 2);
	}

	void CopyFromOther(const Zenith_Vector& xOther)
	{
		m_pxData = static_cast<T*>(Zenith_MemoryManagement::Allocate(xOther.GetCapacity() * sizeof(T)));
		m_uSize = xOther.GetSize();
		m_uCapacity = xOther.GetCapacity();

		// Only copy valid elements (use m_uSize, not m_uCapacity) to avoid copying uninitialized data
		memcpy(m_pxData, xOther.m_pxData, sizeof(T) * m_uSize);
	}

	T* m_pxData = nullptr;
	u_int m_uSize = 0;
	u_int m_uCapacity = 0;
};
