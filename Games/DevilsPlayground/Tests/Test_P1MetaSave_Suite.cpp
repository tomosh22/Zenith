#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "DataStream/Zenith_DataStream.h"
#include "SaveData/Zenith_SaveData.h"

#include "Source/DP_MetaSave.h"

#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P1MetaSave_* — four tests pinning the metagame persistence layer,
// structural mirrors of the four DP_Save tests:
//
//   RoundTripMeta                     — Save -> TryLoad is field-exact.
//   RobustToCorruption                — empty / bad-magic / truncated blobs
//                                       fail-soft to a default state.
//   VersionMismatchFallsBackToDefault — future schema versions are rejected.
//   Test_T0MetaSave_DiskHooks         — SaveToDisk records through the
//                                       Zenith_SaveData test log; a staged
//                                       readback round-trips LoadOrDefault
//                                       without touching real disk.
// ============================================================================

namespace
{
	bool g_bMetaPassed = false;
	const char* g_szMetaWhy = "";

	DP_MetaState MakeDistinctState()
	{
		DP_MetaState xState;
		xState.m_uKnotBalance = 37u;
		xState.m_auTrackUnlockMasks[0] = 0b0000000000111u;  // Forge: 3 nodes
		xState.m_auTrackUnlockMasks[1] = 0b0000000000001u;  // Eye: 1 node
		xState.m_auTrackUnlockMasks[2] = 0b0000000011111u;  // Breath: 5 nodes
		xState.m_uEarnedUnspentKnotsLastRun = 4u;
		return xState;
	}

	bool StatesEqual(const DP_MetaState& xA, const DP_MetaState& xB)
	{
		if (xA.m_uSchemaVersion != xB.m_uSchemaVersion) return false;
		if (xA.m_uKnotBalance != xB.m_uKnotBalance) return false;
		for (uint32_t u = 0; u < DP_MetaSave::kTRACK_COUNT; ++u)
		{
			if (xA.m_auTrackUnlockMasks[u] != xB.m_auTrackUnlockMasks[u]) return false;
		}
		return xA.m_uEarnedUnspentKnotsLastRun == xB.m_uEarnedUnspentKnotsLastRun;
	}

	bool IsDefaultState(const DP_MetaState& xState)
	{
		return StatesEqual(xState, DP_MetaState{});
	}
}

// ---------------------------------------------------------------------------
// 1) Round trip.
// ---------------------------------------------------------------------------
static void Setup_MetaRoundTrip()
{
	g_bMetaPassed = false;
	g_szMetaWhy = "";

	const DP_MetaState xOrig = MakeDistinctState();
	Zenith_DataStream xStream(256);
	DP_MetaSave::Save(xOrig, xStream);
	xStream.SetCursor(0);

	DP_MetaState xLoaded;
	if (!DP_MetaSave::TryLoad(xStream, xLoaded))
	{
		g_szMetaWhy = "TryLoad failed on a freshly-written blob";
		return;
	}
	if (!StatesEqual(xOrig, xLoaded))
	{
		g_szMetaWhy = "round-trip state mismatch";
		return;
	}
	g_bMetaPassed = true;
}

// ---------------------------------------------------------------------------
// 2) Corruption robustness.
// ---------------------------------------------------------------------------
static void Setup_MetaCorruption()
{
	g_bMetaPassed = false;
	g_szMetaWhy = "";

	// Empty buffer.
	{
		uint8_t auEmpty[1] = { 0 };
		Zenith_DataStream xEmpty(auEmpty, 0);
		DP_MetaState xLoaded = MakeDistinctState(); // pollute
		if (DP_MetaSave::TryLoad(xEmpty, xLoaded) || !IsDefaultState(xLoaded))
		{
			g_szMetaWhy = "empty buffer must fail-soft to default";
			return;
		}
	}

	// Wrong magic.
	{
		Zenith_DataStream xStream(64);
		xStream << 0xDEADBEEFu;
		xStream << DP_MetaSave::kCURRENT_SCHEMA_VERSION;
		xStream.SetCursor(0);
		DP_MetaState xLoaded = MakeDistinctState();
		if (DP_MetaSave::TryLoad(xStream, xLoaded) || !IsDefaultState(xLoaded))
		{
			g_szMetaWhy = "wrong magic must fail-soft to default";
			return;
		}
	}

	// Truncated: valid magic + version, then nothing.
	{
		uint8_t auBytes[8] = {};
		std::memcpy(auBytes, &DP_MetaSave::kMAGIC, 4);
		std::memcpy(auBytes + 4, &DP_MetaSave::kCURRENT_SCHEMA_VERSION, 4);
		Zenith_DataStream xTruncated(auBytes, 8);
		DP_MetaState xLoaded = MakeDistinctState();
		if (DP_MetaSave::TryLoad(xTruncated, xLoaded) || !IsDefaultState(xLoaded))
		{
			g_szMetaWhy = "truncated blob must fail-soft to default";
			return;
		}
	}

	// Insane track count.
	{
		Zenith_DataStream xStream(64);
		xStream << DP_MetaSave::kMAGIC;
		xStream << DP_MetaSave::kCURRENT_SCHEMA_VERSION;
		xStream << 0u;          // balance
		xStream << 0xFFFFFFu;   // track count way past the sanity cap
		xStream.SetCursor(0);
		DP_MetaState xLoaded = MakeDistinctState();
		if (DP_MetaSave::TryLoad(xStream, xLoaded) || !IsDefaultState(xLoaded))
		{
			g_szMetaWhy = "insane track count must fail-soft to default";
			return;
		}
	}

	g_bMetaPassed = true;
}

// ---------------------------------------------------------------------------
// 3) Version mismatch.
// ---------------------------------------------------------------------------
static void Setup_MetaVersionMismatch()
{
	g_bMetaPassed = false;
	g_szMetaWhy = "";

	Zenith_DataStream xStream(256);
	DP_MetaSave::Save(MakeDistinctState(), xStream);
	// Overwrite the version field (bytes 4..7) with a future version.
	xStream.SetCursor(sizeof(uint32_t));
	xStream << (DP_MetaSave::kCURRENT_SCHEMA_VERSION + 100u);
	xStream.SetCursor(0);

	DP_MetaState xLoaded = MakeDistinctState();
	if (DP_MetaSave::TryLoad(xStream, xLoaded) || !IsDefaultState(xLoaded))
	{
		g_szMetaWhy = "future schema version must fail-soft to default";
		return;
	}
	g_bMetaPassed = true;
}

// ---------------------------------------------------------------------------
// 4) Disk hooks: SaveToDisk records via the Zenith_SaveData test log;
//    a staged readback feeds LoadOrDefault without real disk I/O.
// ---------------------------------------------------------------------------
static void Setup_MetaDiskHooks()
{
	g_bMetaPassed = false;
	g_szMetaWhy = "";

	Zenith_SaveData::ClearForTest();
	DP_MetaSave::InvalidateCacheForTest();

	// SaveToDisk must write the meta slot exactly once.
	const DP_MetaState xOrig = MakeDistinctState();
	DP_MetaSave::SaveToDisk(xOrig);
	const auto& axWritten = Zenith_SaveData::GetWrittenSlotsForTest();
	if (axWritten.GetSize() != 1
		|| axWritten.Get(0).m_strSlotName != DP_MetaSave::SlotName())
	{
		g_szMetaWhy = "SaveToDisk did not record exactly one write to the meta slot";
		return;
	}

	// Stage the recorded payload as a readback; a fresh LoadOrDefault must
	// reproduce the state byte-exactly (bypassing disk).
	const auto& xSlot = axWritten.Get(0);
	Zenith_SaveData::SetReadbackForTest(DP_MetaSave::SlotName(),
		xSlot.m_uGameVersion,
		xSlot.m_xPayload.GetDataPointer(),
		xSlot.m_xPayload.GetSize());
	const DP_MetaState xLoaded = DP_MetaSave::LoadOrDefault();
	if (!StatesEqual(xOrig, xLoaded))
	{
		g_szMetaWhy = "staged readback did not round-trip through LoadOrDefault";
		return;
	}

	// Cached() must serve the state SaveToDisk stamped without re-reading.
	if (!StatesEqual(DP_MetaSave::Cached(), xOrig))
	{
		g_szMetaWhy = "Cached() diverged from the last SaveToDisk state";
		return;
	}

	// Test hygiene: the SaveToDisk above wrote a REAL slot file — remove it
	// so later per-process tests (fresh processes reading ambient disk)
	// can't inherit this test's fabricated unlock masks.
	Zenith_SaveData::DeleteSlot(DP_MetaSave::SlotName());
	Zenith_SaveData::ClearForTest();
	DP_MetaSave::InvalidateCacheForTest();
	g_bMetaPassed = true;
}

// ---------------------------------------------------------------------------
// Shared step/verify + registrations.
// ---------------------------------------------------------------------------
static bool Step_MetaSuite(int /*iFrame*/) { return false; }

static bool Verify_MetaSuite()
{
	if (!g_bMetaPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "MetaSave suite failed: %s", g_szMetaWhy);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xMetaRoundTripTest = {
	"Test_P1MetaSave_RoundTripMeta",
	&Setup_MetaRoundTrip, &Step_MetaSuite, &Verify_MetaSuite, 60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMetaRoundTripTest);

static const Zenith_AutomatedTest g_xMetaCorruptionTest = {
	"Test_P1MetaSave_RobustToCorruption",
	&Setup_MetaCorruption, &Step_MetaSuite, &Verify_MetaSuite, 60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMetaCorruptionTest);

static const Zenith_AutomatedTest g_xMetaVersionTest = {
	"Test_P1MetaSave_VersionMismatchFallsBackToDefault",
	&Setup_MetaVersionMismatch, &Step_MetaSuite, &Verify_MetaSuite, 60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMetaVersionTest);

static const Zenith_AutomatedTest g_xMetaDiskHooksTest = {
	"Test_T0MetaSave_DiskHooks",
	&Setup_MetaDiskHooks, &Step_MetaSuite, &Verify_MetaSuite, 60
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xMetaDiskHooksTest);

#endif // ZENITH_INPUT_SIMULATOR
