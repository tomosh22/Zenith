#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Telemetry/Zenith_Telemetry.h"

#include "Source/DPTelemetry.h"
#include "Source/DPTelemetryAnalyzer.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

// ============================================================================
// Test_DPTelemetryAnalyzer
//
// Phase 4 of the verification system (2026-05-16).
//
// Unit-tests the analyzer in isolation, against synthesised telemetry
// produced via the recorder API directly (no scene, no bot). The
// recorder + writer + reader are already covered by Test_TelemetryRoundTrip;
// this file only asserts that the analyzer's predicate logic is correct.
//
// For each criterion the test:
//   1. Builds a minimal happy-path recording that satisfies it.
//   2. Asserts the analyzer returns bPassed=true.
//   3. Builds a recording that violates ONLY that criterion.
//   4. Asserts the analyzer returns bPassed=false with the right reason.
//
// Plus integration tests:
//   - File-not-found returns a verdict with bOverallPass=false.
//   - Pipeline-health preset (HeaderMagicValid + HeaderHasSceneName +
//     FramesRecorded + FramesNonEmpty) passes on a real-shaped recording.
// ============================================================================

namespace
{
	bool g_bPassed = false;
	const char* g_szFailureReason = "";

	bool Fail(const char* sz) { g_szFailureReason = sz; return false; }

	std::string TempPath(const char* sz)
	{
		std::error_code xErr;
		std::filesystem::path xDir = std::filesystem::temp_directory_path(xErr);
		if (xErr) xDir = ".";
		xDir /= std::string("dp_analyzer_") + sz;
		return xDir.string();
	}

	// Build a recording with N samples (each carrying a sentinel entity)
	// + the supplied events. Returns the file path written.
	std::string BuildRecording(const char* szSuffix,
	                           const char* szSceneName,
	                           uint32_t uSamplesToEmit,
	                           uint32_t uEntityFlagsPerSample,
	                           const Zenith_Telemetry::Event* axEvents,
	                           uint32_t uEventCount)
	{
		const std::string strPath = TempPath(szSuffix);

		Zenith_Telemetry::Header xHeader;
		xHeader.strSceneName = szSceneName;
		xHeader.uSeed        = 0xA11C5u;
		xHeader.uSamplePeriodFrames = 1;

		auto& xRec = Zenith_Telemetry::GetRecorder();
		xRec.Begin(xHeader);

		for (uint32_t i = 0; i < uSamplesToEmit; ++i)
		{
			xRec.NextFrame();
			Zenith_Telemetry::FrameSample xS;
			xS.fTimeS = static_cast<float>(i) * (1.0f / 60.0f);
			Zenith_Telemetry::EntitySnapshot xE;
			xE.xId.m_uIndex = 1u + i;
			xE.uStateFlags  = uEntityFlagsPerSample;
			xS.axEntities.PushBack(xE);
			xRec.RecordFrame(xS);
		}

		for (uint32_t i = 0; i < uEventCount; ++i)
		{
			xRec.RecordEvent(axEvents[i]);
		}

		xRec.End(strPath.c_str(), nullptr, nullptr);
		return strPath;
	}

	using A = DPTelemetryAnalyzer::Criterion;

	// Run one criterion in isolation against a recording. Returns the
	// passed bool.
	bool CheckOne(const char* szPath, A eCriterion)
	{
		Zenith_Telemetry::Reader xReader;
		if (!xReader.LoadFromFile(szPath)) return false;
		const DPTelemetryAnalyzer::Verdict xV =
			DPTelemetryAnalyzer::Analyze(xReader, &eCriterion, 1u);
		return xV.bOverallPass;
	}
}

// ============================================================================
// 1) Header magic + version + scene name.
// ============================================================================
static bool TestHeaderCriteria()
{
	const std::string strOk = BuildRecording("hdr_ok.ztlm", "TestScene", 1, 0, nullptr, 0);
	if (!CheckOne(strOk.c_str(), A::HeaderMagicValid)) return Fail("hdr: valid magic should pass");
	if (!CheckOne(strOk.c_str(), A::HeaderHasSceneName)) return Fail("hdr: non-empty scene name should pass");

	const std::string strNoName = BuildRecording("hdr_noname.ztlm", "", 1, 0, nullptr, 0);
	if (CheckOne(strNoName.c_str(), A::HeaderHasSceneName))
		return Fail("hdr: empty scene name should fail");

	return true;
}

// ============================================================================
// 2) Frame-count + non-empty.
// ============================================================================
static bool TestFrameCriteria()
{
	// 25 samples, each with 1 entity -> both criteria should pass with
	// default thresholds (uMinFrames=20, uMinSampleEntities=1).
	const std::string strOk = BuildRecording("frames_ok.ztlm", "S", 25u, 0u, nullptr, 0u);
	if (!CheckOne(strOk.c_str(), A::FramesRecorded)) return Fail("frames: 25 should pass FramesRecorded");
	if (!CheckOne(strOk.c_str(), A::FramesNonEmpty)) return Fail("frames: each sample non-empty");

	// 5 samples -> below default threshold, fails FramesRecorded.
	const std::string strFew = BuildRecording("frames_few.ztlm", "S", 5u, 0u, nullptr, 0u);
	if (CheckOne(strFew.c_str(), A::FramesRecorded))
		return Fail("frames: 5 should fail under uMinFrames=20");

	return true;
}

// ============================================================================
// 3) State-flag scans (possessed / sprint / quiet / holding).
// ============================================================================
static bool TestStateFlagCriteria()
{
	const uint32_t uPoss = DPTelemetry::StateFlags::Possessed;
	const uint32_t uSpr  = DPTelemetry::StateFlags::Sprinting;
	const uint32_t uQt   = DPTelemetry::StateFlags::WalkQuiet;
	const uint32_t uHold = DPTelemetry::StateFlags::HoldingItem;

	// One recording with every flag set.
	{
		const std::string strAll = BuildRecording("flags_all.ztlm", "S",
			25u, uPoss | uSpr | uQt | uHold, nullptr, 0u);
		if (!CheckOne(strAll.c_str(), A::AnyPossessedFrame)) return Fail("flags-all: possessed");
		if (!CheckOne(strAll.c_str(), A::AnySprintFrame))    return Fail("flags-all: sprint");
		if (!CheckOne(strAll.c_str(), A::AnyWalkQuietFrame)) return Fail("flags-all: quiet");
		if (!CheckOne(strAll.c_str(), A::AnyHoldingItemFrame))return Fail("flags-all: holding");
	}

	// One recording with NO flags set; every flag-criterion fails.
	{
		const std::string strNone = BuildRecording("flags_none.ztlm", "S", 25u, 0u, nullptr, 0u);
		if (CheckOne(strNone.c_str(), A::AnyPossessedFrame)) return Fail("flags-none: possessed false-positive");
		if (CheckOne(strNone.c_str(), A::AnySprintFrame))    return Fail("flags-none: sprint false-positive");
		if (CheckOne(strNone.c_str(), A::AnyWalkQuietFrame)) return Fail("flags-none: quiet false-positive");
		if (CheckOne(strNone.c_str(), A::AnyHoldingItemFrame))return Fail("flags-none: holding false-positive");
	}

	return true;
}

// ============================================================================
// 4) Event-type scans (Possession / Interact / Victory / RunLost / Pickup
//    / Terminal).
// ============================================================================
static bool TestEventCriteria()
{
	auto MakeEvent = [](DPTelemetry::DPEventType e) -> Zenith_Telemetry::Event
	{
		Zenith_Telemetry::Event xE;
		xE.uEventType = static_cast<uint16_t>(e);
		return xE;
	};

	// Empty event list -> nothing fires.
	{
		const std::string strNone = BuildRecording("ev_none.ztlm", "S", 25u, 0u, nullptr, 0u);
		if (CheckOne(strNone.c_str(), A::VictoryFired))      return Fail("events: no-events should not pass VictoryFired");
		if (CheckOne(strNone.c_str(), A::InteractFired))     return Fail("events: no-events should not pass InteractFired");
		if (CheckOne(strNone.c_str(), A::PickupFired))       return Fail("events: no-events should not pass PickupFired");
		if (CheckOne(strNone.c_str(), A::RunLostFired))      return Fail("events: no-events should not pass RunLostFired");
		if (CheckOne(strNone.c_str(), A::TerminalEventFired))return Fail("events: no-events should not pass TerminalEventFired");
	}

	// One Victory only -> Victory + Terminal pass, others fail.
	{
		Zenith_Telemetry::Event aE[1] = { MakeEvent(DPTelemetry::DPEventType::Victory) };
		const std::string strV = BuildRecording("ev_victory.ztlm", "S", 25u, 0u, aE, 1u);
		if (!CheckOne(strV.c_str(), A::VictoryFired))         return Fail("events: Victory should pass");
		if (!CheckOne(strV.c_str(), A::TerminalEventFired))   return Fail("events: Victory should pass Terminal");
		if (CheckOne(strV.c_str(), A::RunLostFired))          return Fail("events: RunLost should not fire on Victory");
	}

	// One RunLost only -> RunLost + Terminal pass.
	{
		Zenith_Telemetry::Event aE[1] = { MakeEvent(DPTelemetry::DPEventType::RunLost) };
		const std::string strR = BuildRecording("ev_runlost.ztlm", "S", 25u, 0u, aE, 1u);
		if (!CheckOne(strR.c_str(), A::RunLostFired))         return Fail("events: RunLost should pass");
		if (!CheckOne(strR.c_str(), A::TerminalEventFired))   return Fail("events: RunLost should pass Terminal");
	}

	// Interact event -> InteractFired passes. Same for InteractionBegin.
	{
		Zenith_Telemetry::Event aE[2] = {
			MakeEvent(DPTelemetry::DPEventType::Interact),
			MakeEvent(DPTelemetry::DPEventType::InteractionBegin),
		};
		const std::string strI = BuildRecording("ev_interact.ztlm", "S", 25u, 0u, aE, 2u);
		if (!CheckOne(strI.c_str(), A::InteractFired)) return Fail("events: InteractFired should accept Interact or InteractionBegin");
	}

	// Pickup event.
	{
		Zenith_Telemetry::Event aE[1] = { MakeEvent(DPTelemetry::DPEventType::ItemPickup) };
		const std::string strP = BuildRecording("ev_pickup.ztlm", "S", 25u, 0u, aE, 1u);
		if (!CheckOne(strP.c_str(), A::PickupFired)) return Fail("events: PickupFired should fire on ItemPickup");
	}

	// PossessionChanged event OR a Possessed flag frame should pass
	// PossessionFired. (Legacy Possession/Unpossession aliases were
	// removed in the 2026-05-17 cleanup pass; PossessionChanged is the
	// canonical event for any possession transition.)
	{
		Zenith_Telemetry::Event aE[1] = { MakeEvent(DPTelemetry::DPEventType::PossessionChanged) };
		const std::string strPE = BuildRecording("ev_poss.ztlm", "S", 25u, 0u, aE, 1u);
		if (!CheckOne(strPE.c_str(), A::PossessionFired)) return Fail("possess: event should pass PossessionFired");

		const std::string strPF = BuildRecording("frame_poss.ztlm", "S",
			25u, DPTelemetry::StateFlags::Possessed, nullptr, 0u);
		if (!CheckOne(strPF.c_str(), A::PossessionFired)) return Fail("possess: frame flag should pass PossessionFired");

		const std::string strNone = BuildRecording("poss_none.ztlm", "S", 25u, 0u, nullptr, 0u);
		if (CheckOne(strNone.c_str(), A::PossessionFired)) return Fail("possess: neither path should fail PossessionFired");
	}

	// Phase-5-audit (2026-05-16) granular criteria. Each fires when its
	// own event type is present + falls back to false when absent.
	auto AssertGranular = [&MakeEvent](DPTelemetry::DPEventType eEvt, A eCrit, const char* szWhich) -> bool
	{
		Zenith_Telemetry::Event aE[1] = { MakeEvent(eEvt) };
		const std::string strPos = BuildRecording("ev_granular_pos.ztlm", "S", 25u, 0u, aE, 1u);
		if (!CheckOne(strPos.c_str(), eCrit))
			return Fail("granular: positive case should pass");
		const std::string strNeg = BuildRecording("ev_granular_neg.ztlm", "S", 25u, 0u, nullptr, 0u);
		if (CheckOne(strNeg.c_str(), eCrit))
			return Fail("granular: negative case should fail");
		(void)szWhich;
		return true;
	};
	if (!AssertGranular(DPTelemetry::DPEventType::PossessionChanged, A::PossessionChangedFired, "PossessionChanged")) return false;
	if (!AssertGranular(DPTelemetry::DPEventType::DoorOpened,        A::DoorOpenedFired,        "DoorOpened"))         return false;
	if (!AssertGranular(DPTelemetry::DPEventType::ChestOpened,       A::ChestOpenedFired,       "ChestOpened"))        return false;
	if (!AssertGranular(DPTelemetry::DPEventType::ForgeCrafted,      A::ForgeCraftedFired,      "ForgeCrafted"))       return false;
	if (!AssertGranular(DPTelemetry::DPEventType::ObjectivePlaced,   A::ObjectivePlacedFired,   "ObjectivePlaced"))    return false;

	return true;
}

// ============================================================================
// 5) Bad-magic file produces a usable verdict.
//
// Zenith_DataStream::ReadFromFile asserts on a missing file (debug
// builds break on the underlying file-access assert), so the
// "non-existent path" case can't be exercised here. Instead this builds
// a valid-shape header with a corrupted magic field -- the same path
// Test_TelemetryEdgeCases::TestBadMagic exercises -- and verifies
// AnalyzeFile threads the rejection through to a failed verdict.
// ============================================================================
static bool TestBadMagicFile()
{
	const std::string strPath = TempPath("badmagic.ztlm");
	{
		Zenith_DataStream xS(256);
		Zenith_Telemetry::Header xH;
		xH.uMagic = 0xDEADBEEFu;     // intentionally wrong
		xH.strSceneName = "X";
		xH.WriteToDataStream(xS);
		const uint8_t uEnd = static_cast<uint8_t>(Zenith_Telemetry::RecordType::End);
		xS << uEnd;
		xS.WriteToFile(strPath.c_str());
	}

	const A aE[] = { A::HeaderMagicValid };
	const DPTelemetryAnalyzer::Verdict xV =
		DPTelemetryAnalyzer::AnalyzeFile(strPath.c_str(), aE, 1u);
	if (xV.bOverallPass) return Fail("bad-magic: should not pass");
	if (xV.axResults.GetSize() == 0u) return Fail("bad-magic: should surface a result row");
	if (xV.axResults.Get(0).bPassed) return Fail("bad-magic: result should be false");
	return true;
}

// ============================================================================
// 6) Pipeline-health preset on a real-shaped recording.
// ============================================================================
static bool TestPipelineHealthPreset()
{
	const std::string str = BuildRecording("pipeline.ztlm", "GameLevel",
		30u, DPTelemetry::StateFlags::Alive, nullptr, 0u);

	const DPTelemetryAnalyzer::Verdict xV = DPTelemetryAnalyzer::AnalyzeFile(
		str.c_str(),
		DPTelemetryAnalyzer::akPipelineHealthCriteria,
		DPTelemetryAnalyzer::kPipelineHealthCriteriaCount);

	if (!xV.bOverallPass) return Fail("pipeline-preset: should pass on healthy recording");
	if (xV.axResults.GetSize() != DPTelemetryAnalyzer::kPipelineHealthCriteriaCount)
		return Fail("pipeline-preset: result count mismatch");
	for (uint32_t i = 0; i < xV.axResults.GetSize(); ++i)
	{
		if (!xV.axResults.Get(i).bPassed) return Fail("pipeline-preset: criterion failed unexpectedly");
	}
	return true;
}

// ============================================================================
// 7) Criterion-to-string resolver.
// ============================================================================
static bool TestCriterionToString()
{
	if (std::strcmp(DPTelemetryAnalyzer::CriterionToString(A::None), "None") != 0)
		return Fail("name: None");
	if (std::strcmp(DPTelemetryAnalyzer::CriterionToString(A::VictoryFired), "VictoryFired") != 0)
		return Fail("name: VictoryFired");
	if (std::strcmp(DPTelemetryAnalyzer::CriterionToString(A::TerminalEventFired), "TerminalEventFired") != 0)
		return Fail("name: TerminalEventFired");
	if (std::strcmp(DPTelemetryAnalyzer::CriterionToString(A::DoorOpenedFired), "DoorOpenedFired") != 0)
		return Fail("name: DoorOpenedFired");
	if (std::strcmp(DPTelemetryAnalyzer::CriterionToString(A::ForgeCraftedFired), "ForgeCraftedFired") != 0)
		return Fail("name: ForgeCraftedFired");
	if (std::strcmp(DPTelemetryAnalyzer::CriterionToString(A::ObjectivePlacedFired), "ObjectivePlacedFired") != 0)
		return Fail("name: ObjectivePlacedFired");
	if (std::strcmp(DPTelemetryAnalyzer::CriterionToString(A::PriestMoved), "PriestMoved") != 0)
		return Fail("name: PriestMoved");
	if (std::strcmp(DPTelemetryAnalyzer::CriterionToString(static_cast<A>(0xFFu)), "Unknown") != 0)
		return Fail("name: unknown enum should be 'Unknown'");
	return true;
}

// ============================================================================
// 8) PriestMoved: positive case sums per-sample horizontal displacement
//    across IsPriest-flagged entities + passes when above threshold;
//    negative case (priest never moves) fails. The generic BuildRecording
//    helper doesn't support per-sample-varying entity positions so this
//    test builds the recording inline.
// ============================================================================
namespace
{
	// Helper: write a recording with N samples; on each, emit one entity
	// flagged IsPriest at the supplied (X, Z) position. Used by both the
	// "stationary" and "moves linearly" cases below.
	std::string BuildPriestMovementRecording(const char* szSuffix,
	                                          uint32_t uSamples,
	                                          float fX0, float fZ0,
	                                          float fDx, float fDz)
	{
		const std::string strPath = TempPath(szSuffix);

		Zenith_Telemetry::Header xHeader;
		xHeader.strSceneName = "PriestMovementTest";
		xHeader.uSamplePeriodFrames = 1;

		auto& xRec = Zenith_Telemetry::GetRecorder();
		xRec.Begin(xHeader);

		const Zenith_EntityID xPriestId = { 42u, 1u };
		for (uint32_t i = 0; i < uSamples; ++i)
		{
			xRec.NextFrame();
			Zenith_Telemetry::FrameSample xS;
			xS.fTimeS = static_cast<float>(i) * (1.0f / 60.0f);

			Zenith_Telemetry::EntitySnapshot xE;
			xE.xId        = xPriestId;
			xE.xPos       = Zenith_Maths::Vector3(
				fX0 + fDx * static_cast<float>(i),
				1.0f,
				fZ0 + fDz * static_cast<float>(i));
			xE.uStateFlags = DPTelemetry::StateFlags::IsPriest
			               | DPTelemetry::StateFlags::Alive;
			xS.axEntities.PushBack(xE);
			xRec.RecordFrame(xS);
		}

		xRec.End(strPath.c_str(), nullptr, nullptr);
		return strPath;
	}
}

static bool TestPriestMoved()
{
	// Positive: priest walks 1 m east per sample over 11 samples
	// (10 inter-sample steps) -> 10 m total path. Default threshold
	// is 0.5 m; this is 20x past it.
	{
		const std::string str = BuildPriestMovementRecording(
			"priest_moves.ztlm", /*uSamples=*/11u,
			/*fX0=*/0.0f, /*fZ0=*/0.0f,
			/*fDx=*/1.0f, /*fDz=*/0.0f);
		if (!CheckOne(str.c_str(), A::PriestMoved))
			return Fail("priest-moved: 10 m linear walk should pass");
	}

	// Negative: priest sits perfectly still for 11 samples -> 0 m path.
	// Must fail PriestMoved.
	{
		const std::string str = BuildPriestMovementRecording(
			"priest_still.ztlm", /*uSamples=*/11u,
			/*fX0=*/5.0f, /*fZ0=*/5.0f,
			/*fDx=*/0.0f, /*fDz=*/0.0f);
		if (CheckOne(str.c_str(), A::PriestMoved))
			return Fail("priest-moved: stationary priest should fail PriestMoved");
	}

	// Negative: recording with NO IsPriest-flagged entities at all
	// (only villagers) should fail -- no priest to measure means we
	// can't claim he moved.
	{
		const std::string str = BuildRecording(
			"priest_absent.ztlm", "PriestMovementTest",
			11u, DPTelemetry::StateFlags::Alive, nullptr, 0u);
		if (CheckOne(str.c_str(), A::PriestMoved))
			return Fail("priest-moved: no priest in recording should fail");
	}

	return true;
}

static void Setup_Analyzer()
{
	g_bPassed = false;
	g_szFailureReason = "";

	if (!TestHeaderCriteria())      return;
	if (!TestFrameCriteria())       return;
	if (!TestStateFlagCriteria())   return;
	if (!TestEventCriteria())       return;
	if (!TestBadMagicFile())        return;
	if (!TestPipelineHealthPreset())return;
	if (!TestCriterionToString())   return;
	if (!TestPriestMoved())         return;

	g_bPassed = true;
	std::printf("[DPTelemetryAnalyzer] all 8 test clusters passed\n");
	std::fflush(stdout);
}

static bool Step_Analyzer(int /*iFrame*/) { return false; }

static bool Verify_Analyzer()
{
	if (!g_bPassed)
	{
		Zenith_Log(LOG_CATEGORY_AI, "DPTelemetryAnalyzer: %s", g_szFailureReason);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xAnalyzerTest = {
	"Test_DPTelemetryAnalyzer",
	&Setup_Analyzer,
	&Step_Analyzer,
	&Verify_Analyzer,
	30
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xAnalyzerTest);

#endif // ZENITH_INPUT_SIMULATOR
