#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Flux/Zenith_RenderBus.h"

#include <cstring>

// ============================================================================
// Test_T0RenderBus_RecordsDrawCalls (MVP-0.4.2)
//
// Tier-0 smoke for Zenith_RenderBus -- mirrors Test_T0AudioBus structure:
//   1. ClearDrawCallsForTest() empties the buffer.
//   2. RecordDrawCall() round-trips name / matrix / vertex-count /
//      material-id correctly.
//   3. Multiple records accumulate.
//   4. AdvanceFrameForTest() bumps the frame stamp on the next record.
// ============================================================================

namespace
{
	int g_iFailures = 0;
}

static void Setup_T0RenderBus_RecordsDrawCalls()
{
	g_iFailures = 0;
	Zenith_RenderBus::ClearDrawCallsForTest();
}

static bool Step_T0RenderBus_RecordsDrawCalls(int iFrame)
{
	(void)iFrame;
	return false;
}

static bool Verify_T0RenderBus_RecordsDrawCalls()
{
	g_iFailures = 0;

	// 1) Empty after Clear.
	if (Zenith_RenderBus::GetSubmittedDrawCallsForTest().GetSize() != 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0RenderBus: buffer not empty after Clear");
		++g_iFailures;
	}

	// 2) Single record round-trips faithfully.
	Zenith_Maths::Matrix4 xWorld = Zenith_Maths::Matrix4(1.0f);
	xWorld[3][0] = 7.0f; // translation.x = 7
	Zenith_RenderBus::RecordDrawCall("Test.StaticMesh.Floor", xWorld, 1024u, 42u);
	const auto& xCalls = Zenith_RenderBus::GetSubmittedDrawCallsForTest();
	if (xCalls.GetSize() != 1)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0RenderBus: expected 1 draw call, got %u", xCalls.GetSize());
		++g_iFailures;
	}
	else
	{
		const Zenith_RenderBus::DrawCall& xC = xCalls.Get(0);
		if (xC.m_szName == nullptr || std::strcmp(xC.m_szName, "Test.StaticMesh.Floor") != 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_T0RenderBus: name mismatch ('%s')",
				xC.m_szName ? xC.m_szName : "(null)");
			++g_iFailures;
		}
		if (xC.m_uVertexCount != 1024u)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_T0RenderBus: vertex count mismatch (got %u, expected 1024)",
				xC.m_uVertexCount);
			++g_iFailures;
		}
		if (xC.m_uMaterialId != 42u)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_T0RenderBus: material id mismatch (got %u, expected 42)",
				xC.m_uMaterialId);
			++g_iFailures;
		}
		// Matrix translation.x preserved.
		if (xC.m_xWorldMatrix[3][0] != 7.0f)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_T0RenderBus: matrix translation lost (got %f, expected 7.0)",
				xC.m_xWorldMatrix[3][0]);
			++g_iFailures;
		}
	}

	// 3) Multiple records accumulate.
	Zenith_RenderBus::RecordDrawCall("Test.AnotherMesh", xWorld, 256u, 17u);
	Zenith_RenderBus::RecordDrawCall("Test.ThirdMesh", xWorld, 512u, 33u);
	const auto& xCallsAfter = Zenith_RenderBus::GetSubmittedDrawCallsForTest();
	if (xCallsAfter.GetSize() != 3)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0RenderBus: expected 3 calls, got %u", xCallsAfter.GetSize());
		++g_iFailures;
	}

	// 4) AdvanceFrameForTest bumps the frame stamp.
	const u_int uPrevFrame = xCallsAfter.Get(0).m_uFrame;
	Zenith_RenderBus::AdvanceFrameForTest();
	Zenith_RenderBus::RecordDrawCall("Test.NextFrame", xWorld, 64u, 7u);
	const auto& xCallsFinal = Zenith_RenderBus::GetSubmittedDrawCallsForTest();
	if (xCallsFinal.GetSize() < 4)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"Test_T0RenderBus: expected at least 4 calls, got %u",
			xCallsFinal.GetSize());
		++g_iFailures;
	}
	else
	{
		const Zenith_RenderBus::DrawCall& xLatest = xCallsFinal.Get(xCallsFinal.GetSize() - 1);
		if (xLatest.m_uFrame <= uPrevFrame)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"Test_T0RenderBus: frame stamp didn't advance (was %u, now %u)",
				uPrevFrame, xLatest.m_uFrame);
			++g_iFailures;
		}
	}

	Zenith_RenderBus::ClearDrawCallsForTest();

	return g_iFailures == 0;
}

static const Zenith_AutomatedTest g_xRenderBusRecordsTest = {
	"Test_T0RenderBus_RecordsDrawCalls",
	&Setup_T0RenderBus_RecordsDrawCalls,
	&Step_T0RenderBus_RecordsDrawCalls,
	&Verify_T0RenderBus_RecordsDrawCalls,
	10,
	false // m_bRequiresGraphics: false -- pure engine-API exercise
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xRenderBusRecordsTest);

#endif // ZENITH_INPUT_SIMULATOR
