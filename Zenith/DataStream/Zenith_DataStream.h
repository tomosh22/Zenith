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
	//
	// ★ THE FIELD LISTS HERE ARE EXPLICIT, so every new member has to be added to
	// BOTH of them by hand. m_bReadFailed travels with the bytes it describes: a
	// stream that failed a read is still a failed stream after it is moved, and the
	// gutted source is reset to the clean state its null buffer implies.
	Zenith_DataStream(Zenith_DataStream&& other)
		: m_bOwnsData(other.m_bOwnsData)
		, m_ulDataSize(other.m_ulDataSize)
		, m_ulCursor(other.m_ulCursor)
		, m_pData(other.m_pData)
		, m_bReadFailed(other.m_bReadFailed)
	{
		other.m_pData = nullptr;
		other.m_bOwnsData = false;
		other.m_ulDataSize = 0;
		other.m_ulCursor = 0;
		other.m_bReadFailed = false;
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
			m_bReadFailed = other.m_bReadFailed;

			other.m_pData = nullptr;
			other.m_bOwnsData = false;
			other.m_ulDataSize = 0;
			other.m_ulCursor = 0;
			other.m_bReadFailed = false;
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

	// ★ A RESET POINT for the read-failure flag (the other is ReadFromFile). SetCursor
	// is this repo's only "rewind and read it again" gesture, and two long-lived member
	// streams are re-read from cursor 0 on EVERY use without ever being reloaded from a
	// file (Zenith_Prefab's m_xComponentData, Zenith_ComponentMeta's property
	// overrides). Without the reset here, ONE bad read would mark those streams failed
	// for the rest of the process; Zenith_ComponentMeta's seek-to-next-component also
	// relies on it to drop the previous component's failure.
	void SetCursor(uint64_t ulCursor)
	{
		Zenith_Assert(ulCursor <= m_ulDataSize, "SetCursor: cursor %llu exceeds data size %llu", ulCursor, m_ulDataSize);
		// Clamp to valid range in release builds for safety
		m_ulCursor = (ulCursor > m_ulDataSize) ? m_ulDataSize : ulCursor;
		m_bReadFailed = false;
	}

	uint64_t GetCursor() const
	{
		return m_ulCursor;
	}

	// Buffer capacity in bytes (== data length for wrapped/file-loaded streams).
	// For an owned write stream this is NOT the bytes written — use GetCursor().
	uint64_t GetCapacity() const
	{
		return m_ulDataSize;
	}

	// Bytes between the cursor and the end of the buffer. Reading more than this is a
	// buffer overflow, so a length-prefixed block can budget against it BEFORE it
	// reserves anything (`uCount > GetRemainingBytes() / uRECORD_BYTES` is the shape).
	// For an OWNED write stream this is capacity-based exactly like GetCapacity() — it
	// is the space left in the ALLOCATION, not the bytes still to be read — so a
	// budget check only means what it says on a wrapped or file-loaded stream.
	uint64_t GetRemainingBytes() const
	{
		return m_ulDataSize - m_ulCursor;
	}

	// ★ THE READ-FAILURE FLAG IS A REPORT, NOT A GATE.
	//
	// Every read that cannot be satisfied — a null pointer, a bounds overflow, a
	// length prefix past its sanity cap — logs, leaves the cursor where it was, and
	// sets this flag. It does NOT change what any later read does: reads stay
	// individually bounds-checked and individually harmless, because three production
	// read-to-EOF loops advance ONLY via the next read (Zenith_Telemetry,
	// Zenith_AssetRegistry's .zdata type name, Flux_GrassTypeTable's measuring pass)
	// and would spin forever if a set flag short-circuited them.
	//
	// What the flag buys is that a CALLER can ask, once, at the end of a block, "did
	// any of that actually land?" instead of a parse silently producing zero-filled
	// data. Cleared by SetCursor() and ReadFromFile() — the two rewind points.
	bool HasReadFailure() const
	{
		return m_bReadFailed;
	}

	// The public door onto the same flag, for SEMANTIC corruption the stream itself
	// cannot see: an out-of-range enum, a count that cannot fit the bytes that remain,
	// a parallel-array length that does not parallel anything. Same contract as a
	// read failure — it reports, it does not gate.
	void MarkCorrupt(const char* szReason)
	{
		FailRead("DataStream::MarkCorrupt: %s", szReason != nullptr ? szReason : "(no reason given)");
	}

	// Owned streams may grow; wrapped external buffers have a fixed capacity.
	bool OwnsData() const
	{
		return m_bOwnsData;
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
		// ★ SUBTRACTION, NOT ADDITION. `m_ulCursor + ulSize` WRAPS for a hostile size
		// read out of a corrupt file (0xFFFFFFFFFFFFFFF8 + 8 == 0), and a wrapped sum
		// sails through the bounds check straight into a memcpy of ~2^64 bytes. The
		// invariant m_ulCursor <= m_ulDataSize holds at every mutation of either (the
		// ctors set both, SetCursor and SkipBytes clamp, every advance is post-check),
		// so m_ulDataSize - m_ulCursor cannot underflow.
		Zenith_Assert(ulSize <= m_ulDataSize - m_ulCursor, "Reading past end of DataStream");

		// Runtime safety checks (execute in all builds, not just debug)
		if (pData == nullptr || m_pData == nullptr)
		{
			FailRead("DataStream::ReadData: null pointer");
			return;
		}
		if (ulSize > m_ulDataSize - m_ulCursor)
		{
			FailRead("DataStream::ReadData: buffer overflow (cursor=%llu, size=%llu, dataSize=%llu)",
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
			FailRead("DataStream::operator>>: null data pointer");
			return;
		}
		if (m_ulCursor + sizeof(T) > m_ulDataSize)
		{
			FailRead("DataStream::operator>>: buffer overflow (cursor=%llu, typeSize=%zu, dataSize=%llu)",
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
		// Zero-initialised: a prefix read that FAILS leaves this untouched, and an
		// indeterminate count is exactly the loop bound nobody wants.
		u_int uSize = 0;
		*this >> uSize;

		// Sanity check to prevent OOM from corrupted data
		constexpr u_int uMAX_REASONABLE_SIZE = 100000000;
		Zenith_Assert(uSize <= uMAX_REASONABLE_SIZE,
			"std::vector deserialization: Size %u exceeds reasonable limit", uSize);
		if (uSize > uMAX_REASONABLE_SIZE)
		{
			FailRead("DataStream::operator>>(std::vector): size %u exceeds the reasonable limit %u - possible corruption",
				uSize, uMAX_REASONABLE_SIZE);
			return;
		}

		// The COMPOSITE readers are the only place the flag is branched on, and only
		// within their own block: a count that could not be read is not a count, so
		// reserve nothing and loop zero times.
		if (HasReadFailure()) return;

		xVec.reserve(uSize);
		for (u_int u = 0; u < uSize; u++)
		{
			T x;
			*this >> x;
			// Checked AFTER the read, so the element that failed is not appended:
			// a short vector is a better answer than a vector with a zero-filled tail.
			if (HasReadFailure()) break;
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
		u_int uLength = 0;
		*this >> uLength;

		// A length that could not be READ is not a length. Refuse before the
		// zero-length shortcut, so the caller can tell "empty string" from "no bytes".
		if (HasReadFailure()) return;

		if (uLength == 0) return;

		// Maximum string length to prevent OOM from corrupted data
		constexpr u_int uMAX_STRING_LENGTH = 1024 * 1024;  // 1 MB limit
		if (uLength > uMAX_STRING_LENGTH)
		{
			FailRead("DataStream string length %u exceeds maximum %u - possible corruption",
				uLength, uMAX_STRING_LENGTH);
			return;
		}

		// Bounds check before reading
		Zenith_Assert(m_ulCursor + uLength <= m_ulDataSize, "String read would exceed DataStream bounds");
		if (m_ulCursor + uLength > m_ulDataSize)
		{
			FailRead("DataStream string read overflow: cursor=%llu, length=%u, dataSize=%llu",
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
	void operator<<(const std::unordered_map<T1, T2>& xMap)
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
		u_int uCount = 0;
		*this >> uCount;

		// Sanity check to prevent OOM from corrupted data
		constexpr u_int uMAX_REASONABLE_SIZE = 100000000;
		Zenith_Assert(uCount <= uMAX_REASONABLE_SIZE,
			"std::unordered_map deserialization: Count %u exceeds reasonable limit", uCount);
		if (uCount > uMAX_REASONABLE_SIZE)
		{
			FailRead("DataStream::operator>>(std::unordered_map): count %u exceeds the reasonable limit %u - possible corruption",
				uCount, uMAX_REASONABLE_SIZE);
			return;
		}

		if (HasReadFailure()) return;

		for (u_int u = 0; u < uCount; u++)
		{
			T1 xFirst;
			T2 xSecond;
			*this >> xFirst;
			*this >> xSecond;
			if (HasReadFailure()) break;
			xMap.insert({ xFirst, xSecond });
		}
	}
#pragma endregion

	void ReadFromFile(const char* szFilename)
	{
		Zenith_Assert(szFilename != nullptr && szFilename[0] != '\0',
			"ReadFromFile: Invalid filename");

		uint64_t ulFileSize = 0;
		char* pFileData = Zenith_FileAccess::ReadFile(szFilename, ulFileSize);

		// A default-constructed stream already owns its initial write buffer.
		// Release that allocation (or a prior file buffer) before adopting the
		// newly-read bytes; assigning m_pData directly leaks it on every load.
		if (m_bOwnsData && m_pData != nullptr)
		{
			Zenith_MemoryManagement::Deallocate(m_pData);
		}
		m_pData = pFileData;
		m_ulDataSize = ulFileSize;

		Zenith_Assert(m_pData != nullptr || m_ulDataSize == 0,
			"ReadFromFile: Failed to read file '%s'", szFilename);

		m_bOwnsData = true;
		m_ulCursor = 0;
		// ★ A RESET POINT (the other is SetCursor). These are brand new bytes; whatever
		// the previous contents failed to read says nothing about them.
		m_bReadFailed = false;
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
	// Records a read failure: sets the flag and logs the reason through the existing
	// LOG_CATEGORY_CORE sink (there is no DataStream category and this does not add
	// one). Deliberately does NOT touch the cursor — every caller has already decided
	// to leave it where it was, and moving it here would silently change what the
	// NEXT read sees. See HasReadFailure() for why this reports rather than gates.
	template<typename... Args>
	void FailRead(const char* szFormat, Args... xArgs)
	{
		m_bReadFailed = true;
		Zenith_Error(LOG_CATEGORY_CORE, szFormat, xArgs...);
	}

	static constexpr u_int uDEFAULT_INITIAL_SIZE = 1024;

	void* m_pData = nullptr;
	uint64_t m_ulDataSize = 0;
	uint64_t m_ulCursor = 0;
	bool m_bOwnsData = false;
	// Sticky until SetCursor() or ReadFromFile(). A REPORT, never a gate.
	bool m_bReadFailed = false;
};
