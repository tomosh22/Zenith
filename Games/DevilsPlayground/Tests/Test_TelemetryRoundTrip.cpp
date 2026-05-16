#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Telemetry/Zenith_Telemetry.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>

// ============================================================================
// TelemetryRoundTrip_Test
//
// Phase 1 of the DP telemetry/verification system (2026-05-16).
//
// Exercises the engine-side recorder + reader + JSON exporter end-to-end:
//   1. Begin a recording with a known header.
//   2. Record N frame samples + M events with varied payloads.
//   3. End -> writes a .ztlm binary and a .json export.
//   4. Read the binary back -- assert every field matches what we wrote.
//   5. Read the JSON file -- assert it contains the expected event name +
//      a couple of sentinel positions.
//
// Intentionally exercises the ordering invariant (events interleaved with
// samples in frame-index order) by mixing a few RecordFrame / RecordEvent
// calls at different frames.
// ============================================================================

static bool g_bSetupOk  = false;
static bool g_bVerifyOk = false;

static std::string TempPath(const char* szSuffix)
{
	// %TEMP% on Windows, /tmp elsewhere -- std::filesystem::temp_directory_path
	// handles both, and is more portable than getenv("TEMP").
	std::error_code xErr;
	std::filesystem::path xDir = std::filesystem::temp_directory_path(xErr);
	if (xErr) xDir = "."; // fall back to cwd if the env is too locked-down
	xDir /= std::string("dp_telemetry_test_") + szSuffix;
	return xDir.string();
}

// Game-defined event-type enum (test scoped). The real DP enum lives in
// DPTelemetry once Phase 2 lands.
enum class TestEventType : uint16_t
{
	None        = 0,
	Possession  = 1,
	Victory     = 2,
};
static const char* TestEventTypeName(uint16_t u)
{
	switch (static_cast<TestEventType>(u))
	{
	case TestEventType::Possession: return "Possession";
	case TestEventType::Victory:    return "Victory";
	default:                        return nullptr;
	}
}

static void Setup_TelemetryRoundTrip()
{
	const std::string strBin  = TempPath("roundtrip.ztlm");
	const std::string strJson = TempPath("roundtrip.json");

	// 1) Begin recording with a known header.
	Zenith_Telemetry::Header xHeader;
	xHeader.uSeed         = 0xDEADBEEFDEADBEEFull;
	xHeader.uStartUTCMs   = 42424242ull;
	xHeader.strSceneName  = "TestScene";
	xHeader.fFixedDt      = 1.0f / 60.0f;
	xHeader.uSamplePeriodFrames = 6;

	Zenith_Telemetry::Recorder& xRec = Zenith_Telemetry::GetRecorder();
	xRec.Begin(xHeader);

	// Roll forward 30 frames, dropping samples + events at known places.
	for (int i = 0; i < 30; ++i)
	{
		xRec.NextFrame();

		if (xRec.ShouldSampleThisFrame())
		{
			Zenith_Telemetry::FrameSample xS;
			xS.fTimeS = static_cast<float>(i) * (1.0f / 60.0f);
			// One sentinel entity per sample so we can verify positions.
			Zenith_Telemetry::EntitySnapshot xE;
			xE.xId.m_uIndex      = 7u;
			xE.xId.m_uGeneration = 1u;
			xE.xPos              = { static_cast<float>(i), 0.0f, 0.0f };
			xE.xForward          = { 1.0f, 0.0f, 0.0f };
			xE.uStateFlags       = 0xAA55u;
			xS.axEntities.PushBack(xE);
			xRec.RecordFrame(xS);
		}

		if (i == 10)
		{
			Zenith_Telemetry::Event xEvt;
			xEvt.fTimeS     = static_cast<float>(i) * (1.0f / 60.0f);
			xEvt.uEventType = static_cast<uint16_t>(TestEventType::Possession);
			xEvt.xPayload.afFloats[0] = 3.14f;
			xEvt.xPayload.aiInts[1]   = -99;
			xEvt.xPayload.xEntityA.m_uIndex = 7u;
			xEvt.xPayload.xEntityA.m_uGeneration = 1u;
			std::snprintf(xEvt.xPayload.szLabel, sizeof(xEvt.xPayload.szLabel), "Devout");
			xRec.RecordEvent(xEvt);
		}
		if (i == 29)
		{
			Zenith_Telemetry::Event xEvt;
			xEvt.fTimeS     = static_cast<float>(i) * (1.0f / 60.0f);
			xEvt.uEventType = static_cast<uint16_t>(TestEventType::Victory);
			xEvt.xPayload.aiInts[0] = 5; // objectives delivered
			xRec.RecordEvent(xEvt);
		}
	}

	if (!xRec.End(strBin.c_str(), strJson.c_str(), &TestEventTypeName))
	{
		return; // g_bSetupOk stays false -> Verify fails
	}

	// 2) Round-trip the binary.
	Zenith_Telemetry::Reader xReader;
	if (!xReader.LoadFromFile(strBin.c_str())) return;
	const Zenith_Telemetry::Header& xH = xReader.GetHeader();
	if (xH.uSeed != 0xDEADBEEFDEADBEEFull) return;
	if (xH.uStartUTCMs != 42424242ull) return;
	if (xH.strSceneName != "TestScene") return;
	if (xH.uSamplePeriodFrames != 6u) return;

	// Sample period 6 over 30 frames -> samples at frames 6, 12, 18, 24, 30.
	// (NextFrame at i=0..29 -> frame indices 1..30 inside the recorder.)
	const Zenith_Vector<Zenith_Telemetry::FrameSample>& axF = xReader.GetFrames();
	if (axF.GetSize() < 4u) return;
	// At frame index 6 (i==5 in the test loop) the sentinel x-position is 5.0f.
	const Zenith_Telemetry::FrameSample& xS0 = axF.Get(0);
	if (xS0.uFrameIdx != 6u) return;
	if (xS0.axEntities.GetSize() != 1u) return;
	const Zenith_Telemetry::EntitySnapshot& xE0 = xS0.axEntities.Get(0);
	if (xE0.xId.m_uIndex != 7u) return;
	if (xE0.xId.m_uGeneration != 1u) return;
	if (xE0.uStateFlags != 0xAA55u) return;
	// Coordinate sanity: the recorded x equals the loop index when the
	// sample fires (loop counts 0..29, sample at i==5 -> x=5.0).
	if (xE0.xPos.x < 4.5f || xE0.xPos.x > 5.5f) return;

	// Events round-trip.
	const Zenith_Vector<Zenith_Telemetry::Event>& axE = xReader.GetEvents();
	if (axE.GetSize() != 2u) return;
	const Zenith_Telemetry::Event& xPossess = axE.Get(0);
	if (xPossess.uEventType != static_cast<uint16_t>(TestEventType::Possession)) return;
	if (xPossess.xPayload.afFloats[0] < 3.13f || xPossess.xPayload.afFloats[0] > 3.15f) return;
	if (xPossess.xPayload.aiInts[1] != -99) return;
	if (xPossess.xPayload.xEntityA.m_uIndex != 7u) return;
	if (std::strncmp(xPossess.xPayload.szLabel, "Devout", 6) != 0) return;

	const Zenith_Telemetry::Event& xVictory = axE.Get(1);
	if (xVictory.uEventType != static_cast<uint16_t>(TestEventType::Victory)) return;
	if (xVictory.xPayload.aiInts[0] != 5) return;

	// 3) JSON file: cheap content-substring check. Avoids a JSON dep.
	// std::ifstream rather than fopen because MSVC's /W4 + treat-as-errors
	// flags std::fopen as deprecated even with extension headers in scope.
	{
		std::ifstream xIn(strJson, std::ios::binary | std::ios::ate);
		if (!xIn.is_open()) return;
		const std::streamsize llLen = xIn.tellg();
		if (llLen <= 0) return;
		xIn.seekg(0);
		std::string strJsonBody(static_cast<size_t>(llLen), '\0');
		xIn.read(strJsonBody.data(), llLen);
		xIn.close();

		if (strJsonBody.find("\"sceneName\": \"TestScene\"") == std::string::npos) return;
		if (strJsonBody.find("\"name\":\"Possession\"") == std::string::npos) return;
		if (strJsonBody.find("\"name\":\"Victory\"") == std::string::npos) return;
		if (strJsonBody.find("\"label\":\"Devout\"") == std::string::npos) return;
	}

	g_bSetupOk = true;
}

static bool Step_TelemetryRoundTrip(int iFrame)
{
	return iFrame < 2; // pure setup-side work; minimal step loop
}

static bool Verify_TelemetryRoundTrip()
{
	g_bVerifyOk = g_bSetupOk;
	return g_bVerifyOk;
}

static const Zenith_AutomatedTest g_xTelemetryRoundTripTest = {
	"TelemetryRoundTrip_Test",
	&Setup_TelemetryRoundTrip,
	&Step_TelemetryRoundTrip,
	&Verify_TelemetryRoundTrip,
	30 // max-frames safety net
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTelemetryRoundTripTest);

#endif // ZENITH_INPUT_SIMULATOR
