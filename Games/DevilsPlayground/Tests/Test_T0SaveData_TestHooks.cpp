#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "SaveData/Zenith_SaveData.h"

#include <cstring>

// ============================================================================
// Test_T0SaveData_TestHooks (MVP-0.4.3, post-rework)
//
// Tier-0 smoke for the test-only hooks added to Zenith_SaveData. The hooks:
//   - GetWrittenSlotsForTest() returns every Save() since last Clear.
//   - SetReadbackForTest(slot, gameVer, data, size) stages a payload that
//     the next Load(slot, ...) returns INSTEAD of reading from disk.
//   - ClearForTest() wipes the recording log + readback stash.
//
// This replaces the closed PR #27's Zenith_SaveSystem skeleton, which
// duplicated the existing Zenith_SaveData. The right design is to layer
// test instrumentation onto the production system (which TilePuzzle already
// uses) rather than parallel a new namespace.
//
// The test does NOT call Save() / Load() against real disk -- the
// recording-log path is exercised entirely via the readback stash (which
// bypasses disk by design) so the test stays hermetic and doesn't depend
// on Zenith_SaveData::Initialise having been called.
// ============================================================================

namespace
{
	int g_iFailures = 0;

	struct TestPayload
	{
		uint32_t m_uLevel = 0;
		float    m_fScore = 0.0f;
	};

	void WriteCallback(Zenith_DataStream& xStream, void* pxUserData)
	{
		const TestPayload* pxData = static_cast<TestPayload*>(pxUserData);
		xStream << pxData->m_uLevel;
		xStream << pxData->m_fScore;
	}

	void ReadCallback(Zenith_DataStream& xStream, uint32_t /*uGameVersion*/, void* pxUserData)
	{
		TestPayload* pxData = static_cast<TestPayload*>(pxUserData);
		xStream >> pxData->m_uLevel;
		xStream >> pxData->m_fScore;
	}
}

static void Setup_T0SaveData_TestHooks()
{
	g_iFailures = 0;
	Zenith_SaveData::ClearForTest();
}

static bool Step_T0SaveData_TestHooks(int iFrame)
{
	(void)iFrame;
	return false;
}

static bool Verify_T0SaveData_TestHooks()
{
	g_iFailures = 0;

	// 1) ClearForTest leaves both buffers empty.
	if (Zenith_SaveData::GetWrittenSlotsForTest().GetSize() != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveData_TestHooks: write log not empty after Clear");
		++g_iFailures;
	}

	// 2) SetReadbackForTest then Load via the public Zenith_SaveData::Load
	//    short-circuits to the stash. We must NOT call Save() because we
	//    haven't Initialise()'d the save directory in this test.
	//
	//    Build the stash payload by running the write callback into a
	//    DataStream, then handing the byte buffer to SetReadbackForTest.
	TestPayload xExpected = { 42, 1234.5f };
	Zenith_DataStream xStashStream;
	WriteCallback(xStashStream, &xExpected);
	const uint64_t ulStashSize = xStashStream.GetCursor();
	Zenith_SaveData::SetReadbackForTest("autosave", /*uGameVersion=*/1,
		xStashStream.GetData(), ulStashSize);

	TestPayload xLoaded = { 0, 0.0f };
	const bool bLoaded = Zenith_SaveData::Load("autosave", ReadCallback, &xLoaded);
	if (!bLoaded)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveData_TestHooks: Load returned false on staged readback");
		++g_iFailures;
	}
	if (xLoaded.m_uLevel != 42)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveData_TestHooks: level didn't round-trip (got %u, expected 42)",
			xLoaded.m_uLevel);
		++g_iFailures;
	}
	if (xLoaded.m_fScore < 1234.4f || xLoaded.m_fScore > 1234.6f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveData_TestHooks: score didn't round-trip (got %f, expected 1234.5)",
			xLoaded.m_fScore);
		++g_iFailures;
	}

	// 3) SetReadbackForTest with a different payload for the same slot
	//    REPLACES the stash (not appends).
	TestPayload xExpected2 = { 99, 7777.0f };
	Zenith_DataStream xStashStream2;
	WriteCallback(xStashStream2, &xExpected2);
	Zenith_SaveData::SetReadbackForTest("autosave", 1,
		xStashStream2.GetData(), xStashStream2.GetCursor());

	TestPayload xLoaded2 = { 0, 0.0f };
	Zenith_SaveData::Load("autosave", ReadCallback, &xLoaded2);
	if (xLoaded2.m_uLevel != 99)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveData_TestHooks: re-stage didn't replace (got level=%u, expected 99)",
			xLoaded2.m_uLevel);
		++g_iFailures;
	}

	// 4) ClearForTest wipes the stash. We verify by re-staging with a new
	//    payload (which should completely replace, not merge with, any
	//    previous stash state) -- if Clear didn't work then ANY load after
	//    re-stage would still see the previous (wrong) values via the
	//    stash entry that should have been cleared.
	Zenith_SaveData::ClearForTest();
	if (Zenith_SaveData::GetWrittenSlotsForTest().GetSize() != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveData_TestHooks: write log not empty after second Clear");
		++g_iFailures;
	}
	TestPayload xExpected4 = { 7, 0.5f };
	Zenith_DataStream xStashStream4;
	WriteCallback(xStashStream4, &xExpected4);
	Zenith_SaveData::SetReadbackForTest("autosave", 1,
		xStashStream4.GetData(), xStashStream4.GetCursor());

	TestPayload xLoaded4 = { 0, 0.0f };
	Zenith_SaveData::Load("autosave", ReadCallback, &xLoaded4);
	if (xLoaded4.m_uLevel != 7)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0SaveData_TestHooks: post-Clear restage broken (got level=%u, expected 7)",
			xLoaded4.m_uLevel);
		++g_iFailures;
	}

	return g_iFailures == 0;
}

static const Zenith_AutomatedTest g_xSaveDataTestHooksTest = {
	"Test_T0SaveData_TestHooks",
	&Setup_T0SaveData_TestHooks,
	&Step_T0SaveData_TestHooks,
	&Verify_T0SaveData_TestHooks,
	10,
	false // m_bRequiresGraphics: pure save-system-API exercise
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xSaveDataTestHooksTest);

#endif // ZENITH_INPUT_SIMULATOR
