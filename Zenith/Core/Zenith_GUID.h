#pragma once

#include <string>
#include <functional>
#include <cstdint>

class Zenith_DataStream;

/**
 * Zenith_AssetGUID - A 128-bit globally unique identifier for assets
 *
 * Used by the asset system to uniquely identify assets regardless of their file path.
 * This allows assets to be moved/renamed without breaking references.
 *
 * GUIDs are stored in .zmeta files alongside each asset and serialized
 * into scene files and other assets that reference them.
 *
 * Note: This is separate from the simpler Zenith_GUID (64-bit) used for entity IDs.
 */
struct Zenith_AssetGUID
{
	uint64_t m_ulHigh = 0;
	uint64_t m_ulLow = 0;

	// Default constructor creates an invalid GUID
	Zenith_AssetGUID() = default;

	// Construct from two 64-bit values
	Zenith_AssetGUID(uint64_t ulHigh, uint64_t ulLow)
		: m_ulHigh(ulHigh)
		, m_ulLow(ulLow)
	{
	}

	/**
	 * Generate a new random GUID
	 * Uses platform-specific UUID generation for true uniqueness
	 */
	static Zenith_AssetGUID Generate();

	/**
	 * Parse a GUID from a string in format "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
	 * Returns INVALID if the string is malformed
	 */
	static Zenith_AssetGUID FromString(const std::string& strGUID);

	/**
	 * Convert GUID to string in format "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
	 */
	std::string ToString() const;

	/**
	 * Check if this GUID is valid (non-zero)
	 */
	bool IsValid() const
	{
		return m_ulHigh != 0 || m_ulLow != 0;
	}

	// Comparison operators
	bool operator==(const Zenith_AssetGUID& xOther) const
	{
		return m_ulHigh == xOther.m_ulHigh && m_ulLow == xOther.m_ulLow;
	}

	bool operator!=(const Zenith_AssetGUID& xOther) const
	{
		return !(*this == xOther);
	}

	bool operator<(const Zenith_AssetGUID& xOther) const
	{
		if (m_ulHigh != xOther.m_ulHigh)
			return m_ulHigh < xOther.m_ulHigh;
		return m_ulLow < xOther.m_ulLow;
	}

	// Serialization
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// Invalid GUID constant (all zeros)
	static const Zenith_AssetGUID INVALID;
};

// Hash specialization for use in unordered_map/unordered_set
namespace std
{
	template<>
	struct hash<Zenith_AssetGUID>
	{
		size_t operator()(const Zenith_AssetGUID& xGUID) const noexcept
		{
			// Combine the two 64-bit values using a mixing function
			size_t ulHash = hash<uint64_t>{}(xGUID.m_ulHigh);
			ulHash ^= hash<uint64_t>{}(xGUID.m_ulLow) + 0x9e3779b9 + (ulHash << 6) + (ulHash >> 2);
			return ulHash;
		}
	};
}
