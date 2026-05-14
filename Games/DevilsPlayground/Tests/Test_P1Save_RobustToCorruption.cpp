#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "DataStream/Zenith_DataStream.h"

#include "Source/DP_Save.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P1Save_RobustToCorruption (MVP-1.10.2)
//
// Pins the fail-soft contract: truncated, malformed, and zero-byte
// blobs all cause TryLoad to return false WITHOUT crashing, and
// leave the output state at default-constructed values.
//
// This is the "no save file yet" case (zero-byte buffer), the "save
// was interrupted mid-write" case (truncated buffer), and the "save
// got corrupted on disk" case (bad magic / garbage bytes).
//
// Three independent fail-soft scenarios, each verified in turn:
//   1. EMPTY: a stream with zero usable bytes (write nothing then
//      reset cursor). TryLoad must return false.
//   2. WRONG MAGIC: write 4 bytes that aren't the DP magic, then a
//      valid-looking version. TryLoad must reject the magic before
//      it even reads the version.
//   3. TRUNCATED: write valid magic + version + part of a possession
//      ID, then truncate. TryLoad must reject as truncated rather
//      than reading past the end of the buffer (which would assert
//      in the stream or return junk).
//
// What this catches:
//   * Missing magic check (a future Zenith_DataStream change that
//     defaults uninitialized memory to 0 would slip a "version 0"
//     blob past the version gate but the magic check still catches
//     it).
//   * Missing truncation guard (TryLoad reading past the end of a
//     well-formed-prefix blob).
//   * The "leave output at default on failure" promise being broken
//     (e.g., a partial deserialise that wrote half the fields then
//     bailed -- a future caller using the half-written state would
//     see junk).
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	bool IsDefault(const DP_RunState& xState)
	{
		// "Default" means: as constructed by DP_RunState(). Use the
		// known defaults explicitly so a future field's default change
		// also has to update this check (and the assertion below).
		if (xState.m_uSchemaVersion != DP_Save::kCURRENT_SCHEMA_VERSION) return false;
		if (xState.m_xPossessedVillager.IsValid()) return false;
		if (xState.m_fPossessedLife != 0.0f) return false;
		if (xState.m_axHeldItems.GetSize() != 0) return false;
		if (xState.m_axScent.GetSize() != 0) return false;
		if (xState.m_uObjectivesMask != 0) return false;
		// fDawnTimerRemaining defaults to 30.0 -- accept either 30 or
		// any close-to-default value; the constructor sets 30 so we
		// just check exact equality.
		if (xState.m_fDawnTimerRemaining != 30.0f) return false;
		return true;
	}
}

static void Setup_P1SaveCorruption()
{
	g_bPassed = false;
	g_szFailureReason = "";

	// ----- Case 1: EMPTY buffer. -----
	{
		Zenith_DataStream xStream(64);
		// Stream owns 64 bytes but cursor is 0; we explicitly do not
		// write anything. Treat it as "0 readable bytes" by giving
		// TryLoad a wrapped buffer of size 0.
		uint8_t auEmpty[1] = { 0 };
		Zenith_DataStream xEmpty(auEmpty, 0);
		DP_RunState xLoaded;
		// Pollute with non-default values so a "loader silently no-ops"
		// regression would be caught.
		xLoaded.m_xPossessedVillager.m_uIndex = 999;
		const bool bOk = DP_Save::TryLoad(xEmpty, xLoaded);
		if (bOk)
		{
			g_szFailureReason = "TryLoad accepted a zero-byte buffer";
			return;
		}
		if (!IsDefault(xLoaded))
		{
			g_szFailureReason = "Output state not default after EMPTY case";
			return;
		}
	}

	// ----- Case 2: WRONG MAGIC. -----
	{
		Zenith_DataStream xStream(64);
		const uint32_t uBadMagic = 0xDEADBEEFu;
		const uint32_t uValidVer = DP_Save::kCURRENT_SCHEMA_VERSION;
		xStream << uBadMagic;
		xStream << uValidVer;
		xStream.SetCursor(0);
		DP_RunState xLoaded;
		xLoaded.m_xPossessedVillager.m_uIndex = 999;
		const bool bOk = DP_Save::TryLoad(xStream, xLoaded);
		if (bOk)
		{
			g_szFailureReason = "TryLoad accepted a buffer with wrong magic";
			return;
		}
		if (!IsDefault(xLoaded))
		{
			g_szFailureReason = "Output state not default after BAD MAGIC case";
			return;
		}
	}

	// ----- Case 3: TRUNCATED -- valid magic + version + half of an
	// EntityID, then end. -----
	{
		Zenith_DataStream xStream(64);
		xStream << DP_Save::kMAGIC;
		xStream << DP_Save::kCURRENT_SCHEMA_VERSION;
		// Write one uint32_t for possessed.index but NOT the matching
		// generation + life. The stream's "size" tracks the underlying
		// allocation, not the cursor written, so we have to construct
		// a fresh stream wrapping just the written bytes.
		const uint64_t ulWritten = xStream.GetCursor();
		xStream << uint32_t{42u};
		const uint64_t ulTruncated = xStream.GetCursor();
		// Now wrap exactly `ulTruncated` bytes -- the loader sees
		// magic + version + one uint32 with no more bytes available.
		xStream.SetCursor(0);
		// Copy the first ulTruncated bytes into a fresh, size-limited
		// stream so HasBytes() returns false on subsequent reads.
		Zenith_Vector<uint8_t> axCopy;
		for (uint64_t u = 0; u < ulTruncated; ++u)
		{
			uint8_t b = 0;
			xStream.ReadData(&b, sizeof(b));
			axCopy.PushBack(b);
		}
		Zenith_DataStream xTruncated(axCopy.GetDataPointer(), axCopy.GetSize());
		(void)ulWritten; // silence unused-var on builds that strip the prefix
		DP_RunState xLoaded;
		xLoaded.m_xPossessedVillager.m_uIndex = 999;
		const bool bOk = DP_Save::TryLoad(xTruncated, xLoaded);
		if (bOk)
		{
			g_szFailureReason = "TryLoad accepted a truncated buffer";
			return;
		}
		if (!IsDefault(xLoaded))
		{
			g_szFailureReason = "Output state not default after TRUNCATED case";
			return;
		}
	}

	g_bPassed = true;
}

static bool Step_P1SaveCorruption(int /*iFrame*/)
{
	return false;
}

static bool Verify_P1SaveCorruption()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1SaveCorruption: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1SaveCorruptionTest = {
	"Test_P1Save_RobustToCorruption",
	&Setup_P1SaveCorruption,
	&Step_P1SaveCorruption,
	&Verify_P1SaveCorruption,
	60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1SaveCorruptionTest);

#endif // ZENITH_INPUT_SIMULATOR
