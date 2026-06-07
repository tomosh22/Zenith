#pragma once

// ============================================================================
// DPTestSupport - dt-robust perception helpers for the automated-test suite.
//
// WHY THIS EXISTS
// ---------------
// The DP batch suite (and CI) runs every test at a small fixed timestep
// (`--fixed-dt 0.01666`, i.e. 1/60 s). Perception is time-integrated:
//   * sight/hearing AWARENESS accrues per frame as `gain * dt`, and
//   * emitted sounds live only ~0.5 s (Zenith_PerceptionSystem::EmitSoundStimulus).
//
// Tests originally tuned against WALL-CLOCK dt got away with waiting a fixed,
// tiny number of frames because a single slow debug frame is ~0.5-0.8 s -- one
// step crossed the awareness threshold. At fixed-dt 1/60 the same N-frame wait
// is only N/60 s of sim time, which starves perception and the tests fail
// (they passed per-process at wall-clock, failed in the fixed-dt batch).
//
// THE PATTERN
// -----------
// Don't wait a fixed frame count for a time-integrated condition. Instead:
//   1. RE-DRIVE the stimulus every frame (sounds expire, so re-emit), and
//   2. POLL the perception RESULT until it holds (or a generous frame cap).
// This is dt-independent: it advances as soon as perception has integrated
// enough, whatever the dt.
//
// ASSERT ON ATTRIBUTION, NOT EMIT-POSITION
// ----------------------------------------
// `GetLastHeardSoundFor` reports the perceived target's m_xLastKnownPosition,
// which is the SOURCE entity's position -- and sight perception overwrites it
// with the source's live position the moment the agent faces the source. So the
// reported position flips between the emit point and the source's location
// depending on whether sight has fired this frame (dt-dependent). Tests should
// assert the sound was HEARD and ATTRIBUTED to the expected source, not that the
// reported position equals an arbitrary emit point.
// ============================================================================

#ifdef ZENITH_INPUT_SIMULATOR

#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Source/PublicInterfaces.h"   // DP_AI::EmitNoise
#include "Maths/Zenith_Maths.h"

namespace DP_TestSupport
{
	// Re-emit a hearing stimulus from xSource at xPos and report whether xAgent
	// now perceives a HEARING stimulus attributed to xSource. Call once per
	// frame from a test's "wait" phase and advance when it returns true. The
	// re-emit keeps the 0.5 s-lived sound alive while hearing awareness builds,
	// so this is robust at any dt (it polls the result rather than counting
	// frames of a time-integrated process).
	inline bool PollHeardFromSource(Zenith_EntityID xAgent, Zenith_EntityID xSource,
		const Zenith_Maths::Vector3& xPos, float fLoudness = 1.0f, float fRadius = 30.0f)
	{
		DP_AI::EmitNoise(xPos, fLoudness, fRadius, xSource);
		const Zenith_PerceptionSystem::Zenith_LastHeardSound xHeard
			= Zenith_PerceptionSystem::GetLastHeardSoundFor(xAgent);
		return xHeard.m_bValid && xHeard.m_xSourceEntity == xSource;
	}
}

#endif // ZENITH_INPUT_SIMULATOR
