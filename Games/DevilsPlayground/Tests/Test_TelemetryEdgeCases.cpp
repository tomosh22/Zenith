#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Telemetry/Zenith_Telemetry.h"
#include "DataStream/Zenith_DataStream.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

// ============================================================================
// Test_TelemetryEdgeCases
//
// Edge-case coverage for the Phase 1 Zenith_Telemetry recorder + reader.
// Test_TelemetryRoundTrip pins the happy path; this file pins the
// error / boundary behaviours so a regression there can't slip through
// the happy-path test:
//
//   1. Pause suppresses both RecordFrame AND RecordEvent until resumed,
//      then resumes recording cleanly.
//   2. Begin with samplePeriodFrames=0 falls back to the 6-frame default
//      (10 Hz at fixed-dt 1/60).
//   3. Reader rejects a file whose magic doesn't match 'ZTLM'.
//   4. Reader rejects a file whose version is unknown.
//   5. End is a no-op when no Begin happened.
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	bool Fail(const char* sz) { g_szFailureReason = sz; return false; }

	std::string TempPath(const char* szSuffix)
	{
		std::error_code xErr;
		std::filesystem::path xDir = std::filesystem::temp_directory_path(xErr);
		if (xErr) xDir = ".";
		xDir /= std::string("dp_telemetry_edge_") + szSuffix;
		return xDir.string();
	}
}

// -----------------------------------------------------------------------
// 1. Pause + resume.
// -----------------------------------------------------------------------
static bool TestPauseResume()
{
	const std::string strBin = TempPath("pause.ztlm");

	Zenith_Telemetry::Header xHeader;
	xHeader.uSeed = 0x1111u;
	xHeader.strSceneName = "PauseTest";
	xHeader.uSamplePeriodFrames = 2;

	auto& xRec = Zenith_Telemetry::GetRecorder();
	xRec.Begin(xHeader);

	// Record one frame + event while running.
	xRec.NextFrame();
	{
		Zenith_Telemetry::FrameSample xS; xS.fTimeS = 0.016f;
		xRec.RecordFrame(xS);
		Zenith_Telemetry::Event xE; xE.uEventType = 1u;
		xRec.RecordEvent(xE);
	}

	// Pause and try recording -- both calls should be silently dropped.
	xRec.SetPaused(true);
	if (!xRec.IsPaused()) return Fail("pause: IsPaused did not flip true");
	xRec.NextFrame();
	{
		Zenith_Telemetry::FrameSample xS; xS.fTimeS = 0.032f;
		xRec.RecordFrame(xS);
		Zenith_Telemetry::Event xE; xE.uEventType = 2u;
		xRec.RecordEvent(xE);
	}

	// Resume and record one more frame + event.
	xRec.SetPaused(false);
	if (xRec.IsPaused()) return Fail("pause: IsPaused did not flip false on resume");
	xRec.NextFrame();
	{
		Zenith_Telemetry::FrameSample xS; xS.fTimeS = 0.048f;
		xRec.RecordFrame(xS);
		Zenith_Telemetry::Event xE; xE.uEventType = 3u;
		xRec.RecordEvent(xE);
	}

	if (!xRec.End(strBin.c_str(), nullptr, nullptr))
		return Fail("pause: End returned false");

	// Round-trip and assert the paused frame/event are absent.
	Zenith_Telemetry::Reader xReader;
	if (!xReader.LoadFromFile(strBin.c_str())) return Fail("pause: reader load failed");
	if (xReader.GetFrames().GetSize() != 2u) return Fail("pause: expected 2 frames, paused one should be dropped");
	if (xReader.GetEvents().GetSize() != 2u) return Fail("pause: expected 2 events, paused one should be dropped");

	// Event types should be 1 then 3 (the eventType=2 event was paused).
	if (xReader.GetEvents().Get(0).uEventType != 1u) return Fail("pause: first event uEventType != 1");
	if (xReader.GetEvents().Get(1).uEventType != 3u) return Fail("pause: second event uEventType != 3 (paused event leaked)");

	return true;
}

// -----------------------------------------------------------------------
// 2. samplePeriodFrames=0 -> 6 default.
// -----------------------------------------------------------------------
static bool TestSamplePeriodDefault()
{
	const std::string strBin = TempPath("period.ztlm");

	Zenith_Telemetry::Header xHeader;
	xHeader.strSceneName = "PeriodTest";
	xHeader.uSamplePeriodFrames = 0u;  // invalid -- should default to 6

	auto& xRec = Zenith_Telemetry::GetRecorder();
	xRec.Begin(xHeader);

	// Advance 1 frame; ShouldSampleThisFrame at frame index 1 should be
	// false (1 % 6 != 0). Bug shape if the recorder doesn't apply the
	// default: division by 0 (crash) or always-true (every-frame samples).
	xRec.NextFrame();
	if (xRec.ShouldSampleThisFrame()) return Fail("samplePeriod: should NOT sample at frame 1 under default period 6");

	// Frame 6 should sample.
	for (int i = 0; i < 5; ++i) xRec.NextFrame();
	if (!xRec.ShouldSampleThisFrame()) return Fail("samplePeriod: should sample at frame 6 under default period 6");

	if (!xRec.End(strBin.c_str(), nullptr, nullptr))
		return Fail("samplePeriod: End returned false");

	// Header in the file should reflect the corrected value 6, not 0.
	Zenith_Telemetry::Reader xReader;
	if (!xReader.LoadFromFile(strBin.c_str())) return Fail("samplePeriod: reader load failed");
	if (xReader.GetHeader().uSamplePeriodFrames != 6u)
		return Fail("samplePeriod: file header should record the corrected default 6");

	return true;
}

// -----------------------------------------------------------------------
// 3. Bad magic -> Reader returns false.
// -----------------------------------------------------------------------
static bool TestBadMagic()
{
	const std::string strBin = TempPath("badmagic.ztlm");

	// Build a header with a corrupted magic field and write it to disk.
	Zenith_DataStream xS(1024);
	Zenith_Telemetry::Header xHeader;
	xHeader.uMagic = 0xDEADBEEFu;        // wrong magic
	xHeader.strSceneName = "Bad";
	xHeader.WriteToDataStream(xS);
	const uint8_t uEnd = static_cast<uint8_t>(Zenith_Telemetry::RecordType::End);
	xS << uEnd;
	xS.WriteToFile(strBin.c_str());

	Zenith_Telemetry::Reader xReader;
	if (xReader.LoadFromFile(strBin.c_str()))
		return Fail("magic: reader should reject wrong magic, but it accepted");

	return true;
}

// -----------------------------------------------------------------------
// 4. Bad version -> Reader returns false.
// -----------------------------------------------------------------------
static bool TestBadVersion()
{
	const std::string strBin = TempPath("badver.ztlm");

	Zenith_DataStream xS(1024);
	Zenith_Telemetry::Header xHeader;
	xHeader.uVersion = 99999u;            // unknown version
	xHeader.strSceneName = "Bad";
	xHeader.WriteToDataStream(xS);
	const uint8_t uEnd = static_cast<uint8_t>(Zenith_Telemetry::RecordType::End);
	xS << uEnd;
	xS.WriteToFile(strBin.c_str());

	Zenith_Telemetry::Reader xReader;
	if (xReader.LoadFromFile(strBin.c_str()))
		return Fail("version: reader should reject unknown version, but it accepted");

	return true;
}

// -----------------------------------------------------------------------
// 5. JSON exporter works with no event-name resolver (nullptr is the
// documented "fall back to numeric form" sentinel).
// -----------------------------------------------------------------------
static bool TestJsonExportWithoutResolver()
{
	const std::string strBin  = TempPath("noresolver.ztlm");
	const std::string strJson = TempPath("noresolver.json");

	auto& xRec = Zenith_Telemetry::GetRecorder();
	Zenith_Telemetry::Header xHeader;
	xHeader.strSceneName = "NoResolverTest";
	xRec.Begin(xHeader);
	xRec.NextFrame();
	{
		Zenith_Telemetry::FrameSample xS;
		xS.fTimeS = 0.016f;
		xRec.RecordFrame(xS);
	}
	{
		Zenith_Telemetry::Event xE;
		xE.uEventType = 12345u; // arbitrary value, no resolver match
		xRec.RecordEvent(xE);
	}
	// End with nullptr resolver -> exporter must NOT attempt to call it.
	if (!xRec.End(strBin.c_str(), strJson.c_str(), /*pfnEventTypeToString=*/nullptr))
		return Fail("noresolver: End returned false");

	// Open the JSON file and verify:
	//   * the event row is present (type:12345)
	//   * no "name":"..." key was inserted (resolver bypassed)
	std::ifstream xIn(strJson, std::ios::binary | std::ios::ate);
	if (!xIn.is_open()) return Fail("noresolver: JSON file did not open");
	const std::streamsize llLen = xIn.tellg();
	if (llLen <= 0) return Fail("noresolver: JSON file empty");
	xIn.seekg(0);
	std::string strBody(static_cast<size_t>(llLen), '\0');
	xIn.read(strBody.data(), llLen);

	if (strBody.find("\"type\":12345") == std::string::npos)
		return Fail("noresolver: event type 12345 missing from JSON");
	if (strBody.find("\"name\":") != std::string::npos)
		return Fail("noresolver: 'name' key emitted despite nullptr resolver");

	return true;
}

// -----------------------------------------------------------------------
// 6. End-without-Begin returns false (no crash, no file write).
// -----------------------------------------------------------------------
static bool TestEndWithoutBegin()
{
	auto& xRec = Zenith_Telemetry::GetRecorder();
	// Defensive: in case some prior test left m_bRecording=true, end first.
	// The double-End cycle is itself the test: after the second call, the
	// recorder should be idle, and End on an idle recorder must return
	// false rather than asserting or writing an empty file.
	xRec.End(TempPath("ignored1.ztlm").c_str(), nullptr, nullptr);

	const std::string strBin = TempPath("end_no_begin.ztlm");

	// Make sure the file doesn't exist before the call.
	std::error_code xErr;
	std::filesystem::remove(strBin, xErr);

	const bool bRet = xRec.End(strBin.c_str(), nullptr, nullptr);
	if (bRet) return Fail("end-without-begin: End returned true (should be false)");

	// And no file was written.
	if (std::filesystem::exists(strBin, xErr))
		return Fail("end-without-begin: file should not have been written");

	return true;
}

static void Setup_TelemetryEdge()
{
	g_bPassed = false;
	g_szFailureReason = "";

	if (!TestEndWithoutBegin())          return;
	if (!TestPauseResume())              return;
	if (!TestSamplePeriodDefault())      return;
	if (!TestBadMagic())                 return;
	if (!TestBadVersion())               return;
	if (!TestJsonExportWithoutResolver())return;

	g_bPassed = true;
	std::printf("[TelemetryEdgeCases] all 6 edge cases passed\n");
	std::fflush(stdout);
}

static bool Step_TelemetryEdge(int /*iFrame*/)
{
	return false;
}

static bool Verify_TelemetryEdge()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "TelemetryEdgeCases: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xTelemetryEdgeTest = {
	"Test_TelemetryEdgeCases",
	&Setup_TelemetryEdge,
	&Step_TelemetryEdge,
	&Verify_TelemetryEdge,
	30
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTelemetryEdgeTest);

#endif // ZENITH_INPUT_SIMULATOR
