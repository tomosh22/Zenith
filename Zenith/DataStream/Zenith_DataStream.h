#pragma once
#include "FileAccess/Zenith_FileAccess.h"

class Zenith_DataStream
{
public:
	Zenith_DataStream() : Zenith_DataStream(uDEFAULT_INITIAL_SIZE)
	{
	}

	Zenith_DataStream(uint64_t ulSize)
		: m_bOwnsData(true)
		, m_ulDataSize(ulSize)
		, m_ulCursor(0)
		, m_pData(Zenith_MemoryManagement::Allocate(ulSize))
	{
	}

	Zenith_DataStream(void* pData, uint64_t ulSize)
		: m_bOwnsData(false)
		, m_ulDataSize(ulSize)
		, m_ulCursor(0)
		, m_pData(pData)
	{
	}

	// Move constructor
	Zenith_DataStream(Zenith_DataStream&& other)
		: m_bOwnsData(other.m_bOwnsData)
		, m_ulDataSize(other.m_ulDataSize)
		, m_ulCursor(other.m_ulCursor)
		, m_pData(other.m_pData)
	{
		other.m_pData = nullptr;
		other.m_bOwnsData = false;
		other.m_ulDataSize = 0;
		other.m_ulCursor = 0;
	}

	// Move assignment
	Zenith_DataStream& operator=(Zenith_DataStream&& other)
	{
		if (this != &other)
		{
			if (m_bOwnsData && m_pData)
			{
				Zenith_MemoryManagement::Deallocate(m_pData);
			}
			m_pData = other.m_pData;
			m_ulDataSize = other.m_ulDataSize;
			m_ulCursor = other.m_ulCursor;
			m_bOwnsData = other.m_bOwnsData;

			other.m_pData = nullptr;
			other.m_bOwnsData = false;
			other.m_ulDataSize = 0;
			other.m_ulCursor = 0;
		}
		return *this;
	}

	// Delete copy operations to prevent accidental double-free
	Zenith_DataStream(const Zenith_DataStream&) = delete;
	Zenith_DataStream& operator=(const Zenith_DataStream&) = delete;

	~Zenith_DataStream()
	{
		if (m_bOwnsData && m_pData)
		{
			Zenith_MemoryManagement::Deallocate(m_pData);
		}
	}

	void SkipBytes(const u_int uNumBytes)
	{
		m_ulCursor += uNumBytes;
	}

	void SetCursor(uint64_t ulCursor)
	{
		m_ulCursor = ulCursor;
	}

	uint64_t GetCursor() const
	{
		return m_ulCursor;
	}

	uint64_t GetSize() const
	{
		return m_ulDataSize;
	}

	const void* GetData() const
	{
		return m_pData;
	}

	void* GetData()
	{
		return m_pData;
	}

	void WriteData(const void* pData, uint64_t ulSize)
	{
		Zenith_Assert(pData != nullptr, "pData cannot be null");
		uint64_t ulNewCursor = m_ulCursor + ulSize;
		while (ulNewCursor > m_ulDataSize) Resize();
		memcpy(static_cast<uint8_t*>(m_pData) + m_ulCursor, pData, ulSize);
		m_ulCursor = ulNewCursor;
	}

	void Write(const void* pData, uint64_t ulSize)
	{
		WriteData(pData, ulSize);
	}

	void ReadData(void* pData, uint64_t ulSize)
	{
		Zenith_Assert(pData != nullptr, "pData cannot be null");
		Zenith_Assert(m_pData != nullptr, "Stream data is null");
		Zenith_Assert(m_ulCursor + ulSize <= m_ulDataSize, "Reading past end of DataStream");

		// Runtime safety checks (execute in all builds, not just debug)
		if (pData == nullptr || m_pData == nullptr)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "DataStream::ReadData: null pointer");
			return;
		}
		if (m_ulCursor + ulSize > m_ulDataSize)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "DataStream::ReadData: buffer overflow (cursor=%llu, size=%llu, dataSize=%llu)",
				m_ulCursor, ulSize, m_ulDataSize);
			return;
		}

		memcpy(pData, static_cast<uint8_t*>(m_pData) + m_ulCursor, ulSize);
		m_ulCursor += ulSize;
	}

	void Read(void* pData, uint64_t ulSize)
	{
		ReadData(pData, ulSize);
	}

	template<typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
	void operator<<(const T& x)
	{
		uint64_t ulNewCursor = m_ulCursor + sizeof(T);
		while (ulNewCursor > m_ulDataSize) Resize();
		memcpy(static_cast<uint8_t*>(m_pData) + m_ulCursor, &x, sizeof(T));
		m_ulCursor = ulNewCursor;
	}

	template<typename T, std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
	void operator>>(T& x)
	{
		Zenith_Assert(m_pData != nullptr, "Stream data is null");
		Zenith_Assert(m_ulCursor + sizeof(T) <= m_ulDataSize, "Reading past end of DataStream");

		// Runtime safety checks (execute in all builds, not just debug)
		if (m_pData == nullptr)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "DataStream::operator>>: null data pointer");
			return;
		}
		if (m_ulCursor + sizeof(T) > m_ulDataSize)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "DataStream::operator>>: buffer overflow (cursor=%llu, typeSize=%zu, dataSize=%llu)",
				m_ulCursor, sizeof(T), m_ulDataSize);
			return;
		}

		memcpy(&x, static_cast<uint8_t*>(m_pData) + m_ulCursor, sizeof(T));
		m_ulCursor += sizeof(T);
	}

	template<typename T, std::enable_if_t<!std::is_trivially_copyable<T>::value, int> = 0>
	void operator<<(const T& x)
	{
		x.WriteToDataStream(*this);
	}

	template<typename T, std::enable_if_t<!std::is_trivially_copyable<T>::value, int> = 0>
	void operator>>(T& x)
	{
		x.ReadFromDataStream(*this);
	}

#pragma region std::vector
	template<typename T>
	void operator<<(const std::vector<T>& xVec)
	{
		const u_int uSize = xVec.size();
		*this << uSize;
		for (const T& x : xVec) *this << x;
	}
	template<typename T>
	void operator>>(std::vector<T>& xVec)
	{
		u_int uSize;
		*this >> uSize;
		xVec.reserve(uSize);
		for (u_int u = 0; u < uSize; u++)
		{
			T x;
			*this >> x;
			xVec.push_back(x);
		}
	}
#pragma endregion

#pragma region std::pair
	template<typename T1, typename T2>
	void operator<<(const std::pair<T1, T2>& xPair)
	{
		*this << xPair.first;
		*this << xPair.second;
	}
	template<typename T1, typename T2>
	void operator>>(std::pair<T1, T2>& xPair)
	{
		*this >> xPair.first;
		*this >> xPair.second;
	}
#pragma endregion

#pragma region std::string
	void operator<<(const std::string& str)
	{
		const u_int uLength = str.length();
		*this << uLength;
		for (u_int u = 0; u < uLength; u++) *this << str.at(u);
	}
	template<>
	void operator>>(std::string& str)
	{
		str.clear();
		u_int uLength;
		*this >> uLength;
		for (u_int u = 0; u < uLength; u++)
		{
			char c[2];
			*this >> c[0];
			c[1] = '\0';
			str.append(c);
		}
	}
#pragma endregion

#pragma region std::unordered_map
	template<typename T1, typename T2>
	void operator<<(std::unordered_map<T1, T2> xMap)
	{
		*this << u_int(xMap.size());
		for (auto xIt = xMap.cbegin(); xIt != xMap.cend(); xIt++)
		{
			*this << xIt->first;
			*this << xIt->second;
		}
	}
	template<typename T1, typename T2>
	void operator>>(std::unordered_map<T1, T2>& xMap)
	{
		u_int uCount;
		*this >> uCount;
		for (u_int u = 0; u < uCount; u++)
		{
			T1 xFirst;
			T2 xSecond;
			*this >> xFirst;
			*this >> xSecond;
			xMap.insert({ xFirst, xSecond });
		}
	}
#pragma endregion

	void ReadFromFile(const char* szFilename)
	{
		m_pData = Zenith_FileAccess::ReadFile(szFilename, m_ulDataSize);
	}
	void WriteToFile(const char* szFilename)
	{
		Zenith_FileAccess::WriteFile(szFilename, m_pData, m_ulCursor);
	}

private:
	void Resize()
	{
		if (!m_bOwnsData)
		{
			Zenith_Assert(false, "Shouldn't be resizing if we don't own the data");
			return;
		}

		m_ulDataSize *= 2;
		m_pData = Zenith_MemoryManagement::Reallocate(m_pData, m_ulDataSize);
	}
	static constexpr u_int uDEFAULT_INITIAL_SIZE = 1024;

	void* m_pData = nullptr;
	uint64_t m_ulDataSize = 0;
	uint64_t m_ulCursor = 0;
	bool m_bOwnsData = false;
};