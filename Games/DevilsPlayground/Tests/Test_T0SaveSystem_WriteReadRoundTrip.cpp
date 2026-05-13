#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "FileAccess/Zenith_SaveSystem.h"

#include <cstring>

// ============================================================================
// Test_T0SaveSystem_WriteReadRoundTrip (MVP-0.4.3)
//
// Tier-0 smoke for Zenith_SaveSystem skeleton:
//   1. ClearForTest() empties store + log + readback stash.
//   2. WriteBlob -> ReadBlob round-trips key + payload.
//   3. WriteBlob log records every write.
//   4. SetReadbackBlob stages a payload that ReadBlob returns.
//   5. Re-writing the same key OVERWRITES the in-memory store but
//      APPENDS to the write log (both writes visible to test).
// ============================================================================

namespace
{
	int g_iFailures = 0;
}

static void Setup_T0SaveSystem_WriteReadRoundTrip()
{
	g_iFailures = 0;
	Zenith_SaveSystem::ClearForTest();
}

static bool Step_T0SaveSystem_WriteReadRoundTrip(int iFrame)
{
	(void)iFrame;
	return false;
}

static bool BlobMatches(const Zenith_SaveSystem::Blob* pxBlob, const char* szExpected)
{
	if (pxBlob == nullptr) return false;
	const size_t uLen = std::strlen(szExpected);
	if (pxBlob->m_xData.GetSize() != static_cast<u_int>(uLen)) return false;
	for (size_t u = 0; u < uLen; ++u)
	{
		if (pxBlob->m_xData.Get(static_cast<u_int>(u)) != static_cast<uint8_t>(szExpected[u]))
			return false;
	}
	return true;
}

static bool Verify_T0SaveSystem_WriteReadRoundTrip()
{
	g_iFailures = 0;

	// 1) Empty after Clear.
	if (Zenith_SaveSystem::GetWrittenBlobsForTest().GetSize() != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "Test_T0SaveSystem: write log not empty after Clear");
		++g_iFailures;
	}
	if (Zenith_SaveSystem::ReadBlob("nonexistent") != nullptr)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "Test_T0SaveSystem: ReadBlob returned non-null for missing key");
		++g_iFailures;
	}

	// 2) Write -> Read round-trip.
	const char* szPayload = "hello-save";
	Zenith_SaveSystem::WriteBlob("villager.position", szPayload, std::strlen(szPayload));
	const Zenith_SaveSystem::Blob* pxRead = Zenith_SaveSystem::ReadBlob("villager.position");
	if (!BlobMatches(pxRead, szPayload))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveSystem: round-trip failed for key 'villager.position'");
		++g_iFailures;
	}

	// 3) Write log records the write.
	const auto& xLog = Zenith_SaveSystem::GetWrittenBlobsForTest();
	if (xLog.GetSize() != 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveSystem: expected 1 log entry, got %u", xLog.GetSize());
		++g_iFailures;
	}

	// 4) Stash takes precedence -- ReadBlob returns the stash payload even
	//    when an in-memory store entry exists for the same key.
	const char* szStash = "from-disk";
	Zenith_SaveSystem::SetReadbackBlob("villager.position", szStash, std::strlen(szStash));
	const Zenith_SaveSystem::Blob* pxAfterStash = Zenith_SaveSystem::ReadBlob("villager.position");
	if (!BlobMatches(pxAfterStash, szStash))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveSystem: readback stash didn't override store");
		++g_iFailures;
	}

	// 5) Re-write APPENDS to the log even when key already present.
	const char* szPayload2 = "updated-position";
	Zenith_SaveSystem::WriteBlob("villager.position", szPayload2, std::strlen(szPayload2));
	const auto& xLogAfter = Zenith_SaveSystem::GetWrittenBlobsForTest();
	if (xLogAfter.GetSize() != 2)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveSystem: expected 2 log entries after re-write, got %u",
			xLogAfter.GetSize());
		++g_iFailures;
	}

	Zenith_SaveSystem::ClearForTest();

	return g_iFailures == 0;
}

static const Zenith_AutomatedTest g_xSaveSystemRoundTripTest = {
	"Test_T0SaveSystem_WriteReadRoundTrip",
	&Setup_T0SaveSystem_WriteReadRoundTrip,
	&Step_T0SaveSystem_WriteReadRoundTrip,
	&Verify_T0SaveSystem_WriteReadRoundTrip,
	10,
	false
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xSaveSystemRoundTripTest);

#endif // ZENITH_INPUT_SIMULATOR
