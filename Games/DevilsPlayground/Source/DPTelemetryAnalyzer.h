#pragma once

/**
 * DPTelemetryAnalyzer - reads a telemetry recording + applies pass/fail
 * criteria. Produces a Verdict struct enumerated per-criterion so
 * failure modes are diagnosable, not just "PASS / FAIL".
 *
 * Phase 4 of the verification system (2026-05-16).
 *
 * The analyzer is the third leg of the verification pipeline:
 *   Phase 1: Zenith_Telemetry records.
 *   Phase 2: DPTelemetry::Hooks routes DP events into the recorder.
 *   Phase 3: input drives the game (currently
 *            Test_PersonalityPlaythrough's WASD/click simulator; previously
 *            also DPHeuristicBot, dropped 2026-05-17) + the recorder
 *            captures it.
 *   Phase 4 (here): the analyzer asserts "the game was played correctly"
 *     over the captured artefact. Decoupled from the recorder so the
 *     same criteria can be applied to a recording from any source
 *     (bot, human, replay, future procgen runs).
 *
 * Design notes:
 *   - Criteria are an explicit enum, not boolean predicates, so the
 *     verdict can name which criterion failed without callers wiring up
 *     log strings.
 *   - Per-criterion results carry a short reason string -- the analyzer
 *     does the heavy lifting once; downstream UI / CI only formats.
 *   - Pure over a Zenith_Telemetry::Reader (in-memory) so unit tests
 *     can build a synthetic reader and exercise every criterion without
 *     writing files. AnalyzeFile is a thin convenience wrapper.
 */

#include "Telemetry/Zenith_Telemetry.h"
#include "Collections/Zenith_Vector.h"

#include <cstdint>

namespace DPTelemetryAnalyzer
{
	// Stable IDs. Append-only (existing values must not be renumbered)
	// because external tooling (a future CI dashboard) keys off them.
	enum class Criterion : uint8_t
	{
		None                  = 0,
		HeaderMagicValid      = 1,  // header magic == 'ZTLM' and version == 1
		HeaderHasSceneName    = 2,  // strSceneName non-empty
		FramesRecorded        = 3,  // >= N FrameSamples
		FramesNonEmpty        = 4,  // every sample has >=1 EntitySnapshot
		PossessionFired       = 5,  // >=1 Possession event OR >=1 sample with Possessed flag
		AnyPossessedFrame     = 6,  // >=1 sample with the Possessed flag set
		AnySprintFrame        = 7,  // >=1 sample with the Sprinting flag set
		AnyWalkQuietFrame     = 8,  // >=1 sample with the WalkQuiet flag set
		AnyHoldingItemFrame   = 9,  // >=1 sample with the HoldingItem flag set
		InteractFired         = 10, // >=1 InteractionBegin / Interact event
		VictoryFired          = 11, // >=1 Victory event
		RunLostFired          = 12, // >=1 RunLost event (a valid terminal state)
		TerminalEventFired    = 13, // VictoryFired OR RunLostFired -- run ended
		PickupFired           = 14, // >=1 ItemPickup event
		// Phase-5-audit (2026-05-16) granular gameplay-milestone criteria.
		// Each is satisfied iff the corresponding DPEventType fired >=1
		// time in the recording. Lets the bot test require specific
		// mechanics rather than just "the pipeline produced something".
		PossessionChangedFired = 15, // >=1 PossessionChanged event
		DoorOpenedFired        = 16, // >=1 DoorOpened
		ChestOpenedFired       = 17, // >=1 ChestOpened
		ForgeCraftedFired      = 18, // >=1 ForgeCrafted
		ObjectivePlacedFired   = 19, // >=1 ObjectivePlaced
		// 2026-05-17 priest-liveness gate. Sum of per-sample-step
		// horizontal displacement for every IsPriest-flagged entity
		// must exceed Thresholds::fMinPriestPathLengthM. Catches the
		// silent-failure mode where the priest's BT never receives a
		// stimulus + never picks a patrol target, so the priest stays
		// motionless for the whole run. The previous bot recording
		// passed the analyzer with the priest stuck at its spawn for
		// the full 57 s; this criterion makes that loud.
		PriestMoved            = 20,
	};

	// Thresholds for the numeric criteria. Const-ref input so callers
	// can override per-test (e.g. tighten FramesRecorded for long runs).
	struct Thresholds
	{
		uint32_t uMinFrames        = 20;   // FramesRecorded
		uint32_t uMinSampleEntities= 1;    // FramesNonEmpty
		uint32_t uMinPossessFrames = 1;    // AnyPossessedFrame
		uint32_t uMinSprintFrames  = 1;    // AnySprintFrame
		uint32_t uMinQuietFrames   = 1;    // AnyWalkQuietFrame
		uint32_t uMinHoldFrames    = 1;    // AnyHoldingItemFrame
		// PriestMoved: minimum total path length (sum of per-sample
		// step distances) across all IsPriest-flagged entities in the
		// recording. 0.5 m filters out physics jitter (capsule rocking
		// on its collider as it settles) without requiring meaningful
		// AI-driven displacement to be more than half a metre.
		float    fMinPriestPathLengthM = 0.5f;
	};

	struct CriterionResult
	{
		Criterion eCriterion = Criterion::None;
		bool      bPassed    = false;
		// Static string, not owned. Points to literal in this module.
		const char* szReason = "";
	};

	struct Verdict
	{
		bool     bOverallPass = false;
		uint32_t uFrameCount  = 0;
		uint32_t uEventCount  = 0;
		Zenith_Vector<CriterionResult> axResults;
	};

	// Resolve criterion to a stable short string for logs + JSON.
	const char* CriterionToString(Criterion eCriterion);

	// Pure analyzer over a loaded Reader. Each criterion in aeCriteria is
	// evaluated independently; the overall pass is the AND of all of them.
	Verdict Analyze(const Zenith_Telemetry::Reader& xReader,
	                const Criterion* aeCriteria,
	                uint32_t uCount,
	                const Thresholds& xThresholds = Thresholds{});

	// File-wrapper convenience. Returns a Verdict with all criteria
	// failed if the file can't be loaded; otherwise delegates to Analyze.
	Verdict AnalyzeFile(const char* szBinaryPath,
	                    const Criterion* aeCriteria,
	                    uint32_t uCount,
	                    const Thresholds& xThresholds = Thresholds{});

	// Convenience preset: the bare-minimum criteria for "the pipeline
	// ran cleanly and captured something useful". Suitable for the
	// current placeholder GameLevel + straight-line bot. Tightening
	// criteria (e.g. add VictoryFired) is a one-line caller change.
	extern const Criterion akPipelineHealthCriteria[];
	extern const uint32_t  kPipelineHealthCriteriaCount;
}
