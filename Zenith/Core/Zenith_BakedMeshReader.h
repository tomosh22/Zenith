#pragma once

#include <cstdint>
#include <fstream>
#include <limits>
#include <type_traits>

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
// ReadAttribute encodes the .zmesh optional-attribute convention — one leading
// one-byte present flag, then the payload if present. The two call sites'
// variants unified exactly: the streaming side's "absent is only OK if we did
// not want it" is this signature with bRequired = (pData != nullptr).
// ============================================================================
class Zenith_BakedMeshReader
{
public:
	explicit Zenith_BakedMeshReader(const char* szPath)
		: m_xFile(szPath, std::ios::binary | std::ios::ate)
	{
		if (!m_xFile.good())
			return;
		const std::streamoff iSize = static_cast<std::streamoff>(m_xFile.tellg());
		if (iSize <= 0)
			return;
		m_ulRemaining = static_cast<uint64_t>(iSize);
		m_xFile.seekg(0, std::ios::beg);
		m_bValid = m_xFile.good();
	}

	template<typename T>
	bool Read(T& xValue)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return ReadBytes(&xValue, sizeof(T));
	}

	bool ReadBytes(void* pData, uint64_t ulSize)
	{
		if (!m_bValid || pData == nullptr || ulSize > m_ulRemaining ||
			ulSize > static_cast<uint64_t>((std::numeric_limits<std::streamsize>::max)()))
		{
			return false;
		}
		m_xFile.read(static_cast<char*>(pData), static_cast<std::streamsize>(ulSize));
		if (!m_xFile || static_cast<uint64_t>(m_xFile.gcount()) != ulSize)
		{
			m_bValid = false;
			return false;
		}
		m_ulRemaining -= ulSize;
		return true;
	}

	bool Skip(uint64_t ulSize)
	{
		if (!m_bValid || ulSize > m_ulRemaining ||
			ulSize > static_cast<uint64_t>((std::numeric_limits<std::streamoff>::max)()))
		{
			return false;
		}
		m_xFile.seekg(static_cast<std::streamoff>(ulSize), std::ios::cur);
		if (!m_xFile)
		{
			m_bValid = false;
			return false;
		}
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
	std::ifstream m_xFile;
	uint64_t m_ulRemaining = 0;
	bool m_bValid = false;
};
