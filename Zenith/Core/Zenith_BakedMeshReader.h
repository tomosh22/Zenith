#pragma once

#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

#include "FileAccess/Zenith_FileAccess.h"

// ============================================================================
// Zenith_BakedMeshReader
//
// Bounds-checked sequential reader for a baked .zmesh file. Every read is
// validated against the bytes actually remaining, and the first failure
// latches the reader invalid, so a truncated or corrupt file can never
// over-read or partially populate its destination — it just returns false.
//
// It exists because the terrain chunk loader was written twice, once in
// Flux/Terrain/Flux_TerrainStreamingManager.cpp (stream-in) and once in
// EntityComponent/.../Zenith_TerrainComponent.cpp (physics + LOW-LOD combine),
// with byte-identical ctor / Read / ReadBytes / Skip / HasRemaining bodies. Two
// copies of a bounds-checking file reader is the worst possible thing to let
// drift: a fix applied to one leaves the other reading past the end.
//
// ★ Goes through Zenith_FileAccess, NEVER a raw std::ifstream. On Android a
// baked .zmesh lives inside the APK, reachable only via AAssetManager — an
// ifstream against the bare relative path silently fails to open (there is no
// such path on the real filesystem), which used to make EVERY terrain chunk
// (and the combined physics mesh) read as "missing or invalid" on-device,
// while the exact same asset tree loaded fine on Windows. See
// Zenith/Android/CLAUDE.md's file-access section: any engine-owned file must
// be reached through Zenith_FileAccess, not std::ifstream/std::filesystem.
//
// ReadAttribute encodes the .zmesh optional-attribute convention — one leading
// one-byte present flag, then the payload if present. The two call sites'
// variants unified exactly: the streaming side's "absent is only OK if we did
// not want it" is this signature with bRequired = (pData != nullptr).
// ============================================================================
class Zenith_BakedMeshReader
{
public:
	explicit Zenith_BakedMeshReader(const char* szPath)
	{
		uint64_t ulFileSize = 0;
		m_pcData = Zenith_FileAccess::ReadFile(szPath, ulFileSize);
		if (m_pcData == nullptr || ulFileSize == 0)
		{
			return;
		}
		m_ulRemaining = ulFileSize;
		m_bValid = true;
	}

	~Zenith_BakedMeshReader()
	{
		if (m_pcData != nullptr)
		{
			Zenith_FileAccess::FreeFileData(m_pcData);
		}
	}

	Zenith_BakedMeshReader(const Zenith_BakedMeshReader&) = delete;
	Zenith_BakedMeshReader& operator=(const Zenith_BakedMeshReader&) = delete;

	template<typename T>
	bool Read(T& xValue)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return ReadBytes(&xValue, sizeof(T));
	}

	bool ReadBytes(void* pData, uint64_t ulSize)
	{
		if (!m_bValid || pData == nullptr || ulSize > m_ulRemaining)
		{
			return false;
		}
		std::memcpy(pData, m_pcData + m_ulCursor, static_cast<size_t>(ulSize));
		m_ulCursor += ulSize;
		m_ulRemaining -= ulSize;
		return true;
	}

	bool Skip(uint64_t ulSize)
	{
		if (!m_bValid || ulSize > m_ulRemaining)
		{
			return false;
		}
		m_ulCursor += ulSize;
		m_ulRemaining -= ulSize;
		return true;
	}

	bool HasRemaining(uint64_t ulSize) const
	{
		return m_bValid && ulSize <= m_ulRemaining;
	}

	// bRequired: whether an absent attribute should fail the read.
	// pData == nullptr with the attribute present skips the payload.
	// pbPresentOut (optional) reports whether the flag was set.
	bool ReadAttribute(uint64_t ulDataSize, bool bRequired, void* pData = nullptr,
		bool* pbPresentOut = nullptr)
	{
		static_assert(sizeof(bool) == sizeof(uint8_t), "The .zmesh attribute flag format requires one-byte bools");
		uint8_t uPresent = 0;
		if (!Read(uPresent) || uPresent > 1u)
			return false;
		if (pbPresentOut != nullptr)
			*pbPresentOut = uPresent != 0u;
		if (uPresent == 0u)
			return !bRequired;
		return pData != nullptr ? ReadBytes(pData, ulDataSize) : Skip(ulDataSize);
	}

	bool IsAtEnd() const { return m_bValid && m_ulRemaining == 0u; }

private:
	char* m_pcData = nullptr;
	uint64_t m_ulCursor = 0;
	uint64_t m_ulRemaining = 0;
	bool m_bValid = false;
};
