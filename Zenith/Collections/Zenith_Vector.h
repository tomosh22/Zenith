#pragma once
#include "DataStream/Zenith_DataStream.h"

template<typename T>
class Zenith_Vector
{
public:
	Zenith_Vector() : Zenith_Vector(uDEFAULT_INITIAL_COUNT)
	{
	}
	Zenith_Vector(const u_int uCapacity)
		: m_pxData(static_cast<T*>(Zenith_MemoryManagement::Allocate(uCapacity * sizeof(T))))
		, m_uSize(0)
		, m_uCapacity(uCapacity)
	{
	}

	Zenith_Vector(const Zenith_Vector& xOther)
	{
		CopyFromOther(xOther);
	}

	Zenith_Vector(Zenith_Vector&& xOther)
	{
		CopyFromOther(xOther);
	}

	Zenith_Vector& operator=(const Zenith_Vector& xOther)
	{
		Zenith_MemoryManagement::Deallocate(m_pxData);

		CopyFromOther(xOther);

		return *this;
	}

	Zenith_Vector& operator=(Zenith_Vector&& xOther)
	{
		Zenith_MemoryManagement::Deallocate(m_pxData);

		CopyFromOther(xOther);

		return *this;
	}


	~Zenith_Vector()
	{
		Clear();
		Zenith_MemoryManagement::Deallocate(m_pxData);
	}

	const u_int GetSize() const
	{
		return m_uSize;
	}

	const u_int GetCapacity() const
	{
		return m_uCapacity;
	}

	void PushBack(const T& x)
	{
		while (m_uSize >= m_uCapacity) Resize();
		memcpy(static_cast<T*>(m_pxData) + m_uSize, &x, sizeof(T));
		m_uSize++;
	}

	T& Get(u_int uIndex) const
	{
		return m_pxData[uIndex];
	}

	T& GetFront() const
	{
		return m_pxData[0];
	}

	T& GetBack() const
	{
		return m_pxData[m_uSize - 1];
	}

	T* GetDataPointer() const
	{
		return m_pxData;
	}

	void Remove(u_int uIndex)
	{
		Zenith_Assert(uIndex < m_uSize, "Trying to remove element at index greater than vector size");
		m_pxData[uIndex].~T();
		memmove(m_pxData + uIndex, m_pxData + uIndex + 1, sizeof(T) * (m_uCapacity - uIndex - 1));
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

	void Reserve(const u_int uCount)
	{
		m_pxData = static_cast<T*>(Zenith_MemoryManagement::Reallocate(m_pxData, sizeof(T) * uCount));
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uSize;
		xStream >> uSize;
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
		Iterator(const Zenith_Vector& xVec)
		: m_xVec(xVec)
		, m_uCursor(0)
		{
		}

		void Next()
		{
			Zenith_Assert(m_uCursor < m_xVec.GetSize()+1, "Iterated too far");
			m_uCursor++;
		}

		bool Done()
		{
			return m_uCursor == m_xVec.GetSize();
		}

		T& GetData() {return m_xVec.Get(m_uCursor);}
		const T& GetData() const { return m_xVec.Get(m_uCursor); }

	private:
		const Zenith_Vector<T>& m_xVec;
		u_int m_uCursor;
	};

private:
	static constexpr u_int uDEFAULT_INITIAL_COUNT = 8;

	void Resize()
	{
		m_uCapacity *= 2;
		m_pxData = static_cast<T*>(Zenith_MemoryManagement::Reallocate(m_pxData, sizeof(T) * m_uCapacity));
	}

	void CopyFromOther(const Zenith_Vector& xOther)
	{
		m_pxData = static_cast<T*>(Zenith_MemoryManagement::Allocate(xOther.GetCapacity() * sizeof(T)));
		m_uSize = xOther.GetSize();
		m_uCapacity = xOther.GetCapacity();

		memcpy(m_pxData, xOther.m_pxData, sizeof(T) * m_uCapacity);
	}

	T* m_pxData;
	u_int m_uSize;
	u_int m_uCapacity;
};