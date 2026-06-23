#include "UnitTests/Zenith_UnitTests.h"
#include "Telemetry/Zenith_Telemetry.h"

// ============================================================================
// Telemetry Recorder Tests
// ============================================================================

// FlushSnapshot writes the FULL recording to disk WITHOUT ending it, so a process
// killed before End() still leaves a complete, valid file (the tennis referee uses
// this because its windowed run is killed, never reaching OnDestroy). Pins the
// contract: returns false when not recording; leaves IsRecording() true; the
// snapshot round-trips through Reader; a later snapshot / End is a superset.
ZENITH_TEST(Telemetry, RecorderFlushSnapshotIsKillSafe)
{
	using namespace Zenith_Telemetry;

	Recorder xRec;
	const char* szBin = "ztlm_flushsnapshot_test.ztlm";

	// Not recording yet -> FlushSnapshot is a no-op returning false.
	ZENITH_ASSERT_FALSE(xRec.FlushSnapshot(szBin), "FlushSnapshot must fail when not recording");

	Header xHeader;
	xHeader.strSceneName = "FlushTest";
	xRec.Begin(xHeader);

	// One frame (one entity) + one event.
	FrameSample xF;
	xF.uFrameIdx = xRec.GetFrameIdx();
	EntitySnapshot xE;
	xE.xPos = Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f);
	xF.axEntities.PushBack(xE);
	xRec.RecordFrame(xF);
	Event xEvt;
	xEvt.uEventType = 7;
	xRec.RecordEvent(xEvt);

	// Snapshot mid-recording: succeeds, and recording CONTINUES.
	ZENITH_ASSERT_TRUE(xRec.FlushSnapshot(szBin), "FlushSnapshot should succeed while recording");
	ZENITH_ASSERT_TRUE(xRec.IsRecording(), "Recording must continue after a snapshot");

	// The snapshot is a complete, loadable file with the frame + event recorded so far.
	Reader xReader1;
	ZENITH_ASSERT_TRUE(xReader1.LoadFromFile(szBin), "Snapshot file must load");
	ZENITH_ASSERT_EQ(static_cast<int>(xReader1.GetFrames().GetSize()), 1, "Snapshot has the recorded frame");
	ZENITH_ASSERT_EQ(static_cast<int>(xReader1.GetEvents().GetSize()), 1, "Snapshot has the recorded event");

	// Record more, then End. The final file is a SUPERSET and still loads.
	FrameSample xF2;
	xF2.uFrameIdx = xRec.GetFrameIdx();
	xF2.axEntities.PushBack(xE);
	xRec.RecordFrame(xF2);
	ZENITH_ASSERT_TRUE(xRec.End(szBin), "End should flush the full recording");
	ZENITH_ASSERT_FALSE(xRec.IsRecording(), "End stops recording");

	Reader xReader2;
	ZENITH_ASSERT_TRUE(xReader2.LoadFromFile(szBin), "Final file must load");
	ZENITH_ASSERT_EQ(static_cast<int>(xReader2.GetFrames().GetSize()), 2, "Final file is a superset (2 frames)");
	ZENITH_ASSERT_EQ(static_cast<int>(xReader2.GetEvents().GetSize()), 1, "Final file retains the event");

	// After End, FlushSnapshot is a no-op again.
	ZENITH_ASSERT_FALSE(xRec.FlushSnapshot(szBin), "FlushSnapshot must fail after End");
}
