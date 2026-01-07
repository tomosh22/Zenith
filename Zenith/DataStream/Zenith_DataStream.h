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
		Zenith_Assert(m_pData != nullptr, "DataStream: Failed to allocate %llu bytes", ulSize);
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
		Zenith_Assert(m_ulCursor + uNumBytes <= m_ulDataSize,
			"SkipBytes: Would skip past end of stream (cursor=%llu, skip=%u, size=%llu)",
			m_ulCursor, uNumBytes, m_ulDataSize);
		// Clamp to valid range in release builds for safety
		m_ulCursor = (std::min)(m_ulCursor + uNumBytes, m_ulDataSize);
	}

	void SetCursor(uint64_t ulCursor)
	{
		Zenith_Assert(ulCursor <= m_ulDataSize, "SetCursor: cursor %llu exceeds data size %llu", ulCursor, m_ulDataSize);
		// Clamp to valid range in release builds for safety
		m_ulCursor = (ulCursor > m_ulDataSize) ? m_ulDataSize : ulCursor;
	}

	uint64_t GetCursor() const
	{
		return m_ulCursor;
	}

	uint64_t GetSize() const
	{
		return m_ulDataSize;
	}

	// Returns true if the stream contains valid data (non-null pointer and non-zero size)
	// Use this after ReadFromFile() to verify the file was loaded successfully
	bool IsValid() const
	{
		return m_pData != nullptr && m_ulDataSize > 0;
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
		while (ulNewCursor > m_ulDataSize)
		{
			uint64_t ulOldSize = m_ulDataSize;
			Resize();
			// Check if Resize actually grew the buffer - prevents infinite loop on allocation failure
			if (m_ulDataSize == ulOldSize)
			{
				Zenith_Error(LOG_CATEGORY_CORE, "DataStream::WriteData: Resize failed, cannot write %llu bytes", ulSize);
				return;
			}
		}
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
		while (ulNewCursor > m_ulDataSize)
		{
			uint64_t ulOldSize = m_ulDataSize;
			Resize();
			if (m_ulDataSize == ulOldSize)
			{
				Zenith_Error(LOG_CATEGORY_CORE, "DataStream::operator<<: Resize failed");
				return;
			}
		}
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
		const u_int uSize = static_cast<u_int>(xVec.size());
		*this << uSize;
		for (const T& x : xVec) *this << x;
	}
	template<typename T>
	void operator>>(std::vector<T>& xVec)
	{
		u_int uSize;
		*this >> uSize;

		// Sanity check to prevent OOM from corrupted data
		constexpr u_int uMAX_REASONABLE_SIZE = 100000000;
		Zenith_Assert(uSize <= uMAX_REASONABLE_SIZE,
			"std::vector deserialization: Size %u exceeds reasonable limit", uSize);
		if (uSize > uMAX_REASONABLE_SIZE) return;

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
		const u_int uLength = static_cast<u_int>(str.length());
		*this << uLength;
		// Write string data in bulk for efficiency
		if (uLength > 0)
		{
			WriteData(str.data(), uLength);
		}
	}
	template<>
	void operator>>(std::string& str)
	{
		str.clear();
		u_int uLength;
		*this >> uLength;

		if (uLength == 0) return;

		// Maximum string length to prevent OOM from corrupted data
		constexpr u_int uMAX_STRING_LENGTH = 1024 * 1024;  // 1 MB limit
		if (uLength > uMAX_STRING_LENGTH)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "DataStream string length %u exceeds maximum %u - possible corruption",
				uLength, uMAX_STRING_LENGTH);
			return;
		}

		// Bounds check before reading
		Zenith_Assert(m_ulCursor + uLength <= m_ulDataSize, "String read would exceed DataStream bounds");
		if (m_ulCursor + uLength > m_ulDataSize)
		{
			Zenith_Error(LOG_CATEGORY_CORE, "DataStream string read overflow: cursor=%llu, length=%u, dataSize=%llu",
				m_ulCursor, uLength, m_ulDataSize);
			return;
		}

		// Read string data in bulk - O(n) instead of O(n^2)
		str.resize(uLength);
		memcpy(str.data(), static_cast<uint8_t*>(m_pData) + m_ulCursor, uLength);
		m_ulCursor += uLength;
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

		// Sanity check to prevent OOM from corrupted data
		constexpr u_int uMAX_REASONABLE_SIZE = 100000000;
		Zenith_Assert(uCount <= uMAX_REASONABLE_SIZE,
			"std::unordered_map deserialization: Count %u exceeds reasonable limit", uCount);
		if (uCount > uMAX_REASONABLE_SIZE) return;

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
		Zenith_Assert(szFilename != nullptr && szFilename[0] != '\0',
			"ReadFromFile: Invalid filename");

		m_pData = Zenith_FileAccess::ReadFile(szFilename, m_ulDataSize);

		Zenith_Assert(m_pData != nullptr || m_ulDataSize == 0,
			"ReadFromFile: Failed to read file '%s'", szFilename);

		m_bOwnsData = true;
		m_ulCursor = 0;
	}

	void WriteToFile(const char* szFilename)
	{
		Zenith_Assert(szFilename != nullptr && szFilename[0] != '\0',
			"WriteToFile: Invalid filename");
		Zenith_Assert(m_pData != nullptr || m_ulCursor == 0,
			"WriteToFile: No data to write");

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

		uint64_t ulNewSize = m_ulDataSize * 2;
		void* pNewData = Zenith_MemoryManagement::Reallocate(m_pData, ulNewSize);

		Zenith_Assert(pNewData != nullptr, "DataStream::Resize: Failed to reallocate from %llu to %llu bytes",
			m_ulDataSize, ulNewSize);

		if (pNewData != nullptr)
		{
			m_pData = pNewData;
			m_ulDataSize = ulNewSize;
		}
		// Note: If reallocation fails, m_pData remains valid at original size
		// Write operations will continue to try to resize and eventually fail
	}
	static constexpr u_int uDEFAULT_INITIAL_SIZE = 1024;

	void* m_pData = nullptr;
	uint64_t m_ulDataSize = 0;
	uint64_t m_ulCursor = 0;
	bool m_bOwnsData = false;
};