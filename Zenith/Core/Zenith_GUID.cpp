#include "Zenith.h"
#include "Zenith_GUID.h"
#include "DataStream/Zenith_DataStream.h"

#include <random>

#ifdef _WIN32
#include <objbase.h>
#pragma comment(lib, "ole32.lib")
#endif

// Static constant for invalid GUID
const Zenith_AssetGUID Zenith_AssetGUID::INVALID = Zenith_AssetGUID(0, 0);

Zenith_AssetGUID Zenith_AssetGUID::Generate()
{
	Zenith_AssetGUID xResult;

#ifdef _WIN32
	// Use Windows COM GUID generation for true uniqueness
	GUID xWinGUID;
	HRESULT hr = CoCreateGuid(&xWinGUID);
	if (SUCCEEDED(hr))
	{
		// Pack GUID into two 64-bit values
		// GUID structure: Data1 (32-bit), Data2 (16-bit), Data3 (16-bit), Data4 (8 bytes)
		xResult.m_ulHigh = (static_cast<uint64_t>(xWinGUID.Data1) << 32) |
						   (static_cast<uint64_t>(xWinGUID.Data2) << 16) |
						   static_cast<uint64_t>(xWinGUID.Data3);
		xResult.m_ulLow = (static_cast<uint64_t>(xWinGUID.Data4[0]) << 56) |
						  (static_cast<uint64_t>(xWinGUID.Data4[1]) << 48) |
						  (static_cast<uint64_t>(xWinGUID.Data4[2]) << 40) |
						  (static_cast<uint64_t>(xWinGUID.Data4[3]) << 32) |
						  (static_cast<uint64_t>(xWinGUID.Data4[4]) << 24) |
						  (static_cast<uint64_t>(xWinGUID.Data4[5]) << 16) |
						  (static_cast<uint64_t>(xWinGUID.Data4[6]) << 8) |
						  static_cast<uint64_t>(xWinGUID.Data4[7]);
	}
	else
	{
		Zenith_Log("Warning: CoCreateGuid failed, using random fallback");
		// Fallback to random (less unique but still usable)
		std::random_device rd;
		std::mt19937_64 gen(rd());
		xResult.m_ulHigh = gen();
		xResult.m_ulLow = gen();
	}
#else
	// Use random number generation for other platforms
	// Note: This is less unique than platform-specific UUID APIs but works everywhere
	std::random_device rd;
	std::mt19937_64 gen(rd());
	xResult.m_ulHigh = gen();
	xResult.m_ulLow = gen();

	// Set version 4 (random) UUID bits for RFC 4122 compliance
	// Version 4: bits 12-15 of time_hi_and_version = 0100
	xResult.m_ulHigh = (xResult.m_ulHigh & ~0x000000000000F000ULL) | 0x0000000000004000ULL;
	// Variant 1: bits 6-7 of clock_seq_hi_and_reserved = 10
	xResult.m_ulLow = (xResult.m_ulLow & ~0xC000000000000000ULL) | 0x8000000000000000ULL;
#endif

	return xResult;
}

Zenith_AssetGUID Zenith_AssetGUID::FromString(const std::string& strGUID)
{
	// Expected format: "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX" (36 chars)
	if (strGUID.length() != 36)
	{
		return INVALID;
	}

	// Validate dashes are in correct positions
	if (strGUID[8] != '-' || strGUID[13] != '-' || strGUID[18] != '-' || strGUID[23] != '-')
	{
		return INVALID;
	}

	// Parse hex segments
	// Format: AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE
	// High = AAAAAAAA BBBB CCCC (64 bits)
	// Low  = DDDD EEEEEEEEEEEE (64 bits)
	try
	{
		uint64_t ulPart1 = std::stoull(strGUID.substr(0, 8), nullptr, 16);   // AAAAAAAA
		uint64_t ulPart2 = std::stoull(strGUID.substr(9, 4), nullptr, 16);   // BBBB
		uint64_t ulPart3 = std::stoull(strGUID.substr(14, 4), nullptr, 16);  // CCCC
		uint64_t ulPart4 = std::stoull(strGUID.substr(19, 4), nullptr, 16);  // DDDD
		uint64_t ulPart5 = std::stoull(strGUID.substr(24, 12), nullptr, 16); // EEEEEEEEEEEE

		Zenith_AssetGUID xResult;
		xResult.m_ulHigh = (ulPart1 << 32) | (ulPart2 << 16) | ulPart3;
		xResult.m_ulLow = (ulPart4 << 48) | ulPart5;
		return xResult;
	}
	catch (...)
	{
		return INVALID;
	}
}

std::string Zenith_AssetGUID::ToString() const
{
	// Format: "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"
	char acBuffer[37];

	// Extract parts from high and low
	uint32_t uPart1 = static_cast<uint32_t>(m_ulHigh >> 32);         // AAAAAAAA
	uint16_t uPart2 = static_cast<uint16_t>((m_ulHigh >> 16) & 0xFFFF);  // BBBB
	uint16_t uPart3 = static_cast<uint16_t>(m_ulHigh & 0xFFFF);          // CCCC
	uint16_t uPart4 = static_cast<uint16_t>(m_ulLow >> 48);              // DDDD
	uint64_t ulPart5 = m_ulLow & 0x0000FFFFFFFFFFFFULL;                  // EEEEEEEEEEEE

	snprintf(acBuffer, sizeof(acBuffer),
		"%08X-%04X-%04X-%04X-%012llX",
		uPart1, uPart2, uPart3, uPart4, static_cast<unsigned long long>(ulPart5));

	return std::string(acBuffer);
}

void Zenith_AssetGUID::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_ulHigh;
	xStream << m_ulLow;
}

void Zenith_AssetGUID::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_ulHigh;
	xStream >> m_ulLow;
}
