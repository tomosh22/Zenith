#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/Priest_Behaviour.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>
#include <cstdio>

// ============================================================================
// Test_P1WalkQuiet_AelfricEffectiveHearingHalved (MVP-1.7.6)
//
// MVP-1.7.4/5 introduced walk-quiet, which multiplies the footstep
// emission loudness by `movement.walk_footstep_loudness_multiplier`
// (0.5x default). The downstream consequence -- the priest's effective
// hearing RANGE for a walk-quiet villager is reduced -- has been
// asserted only indirectly (we check the loudness multiplier landed in
// the emit call, not that the perception system actually reads less
// far). This test pins the data path:
//
//   walk-quiet footstep at distance D from priest -> NOT heard
//   full-loudness footstep at the SAME distance D -> heard
//
// where D is computed to sit in the sweet spot between the two
// detectability ranges. The perception math (see
// `Zenith_PerceptionSystem::EvaluateHearingForSound`):
//
//   audible := loudness * (1 - dist/radius) >= threshold
//
// With Tuning.json defaults:
//   L_full = 0.3, L_half = 0.15, threshold = 0.05, radius = 10 m
//   Audible-up-to D_full = R * (1 - T/L_full) = 10 * 0.833 = 8.33 m
//   Audible-up-to D_half = R * (1 - T/L_half) = 10 * 0.667 = 6.67 m
//   Test distance picks the MIDPOINT (~7.5 m) so frame-time / tuning
//   drift can wobble +-0.8 m without flipping either assertion.
//
// What this catches:
//   * `walk_footstep_loudness_multiplier` got set to 1.0 by accident
//     (regression: walk-quiet emits at full volume; no test would
//     otherwise notice because the existing
//     Test_P1WalkQuiet_FootstepLoudnessHalved only checks the
//     `Zenith_AudioBus::EmitSound` call's loudness, not the
//     `Zenith_PerceptionSystem::EmitSoundStimulus` call's loudness --
//     a regression that broke the second call but not the first
//     would slip through).
//   * `Priest_Behaviour::OnAwake` stopped reading the
//     `priest.hearing_loudness_threshold` from tuning (and went back
//     to the engine default 0.1, which masks the 0.15 emit entirely
//     -- the priest would stop hearing walk-quiet at ANY distance,
//     which is overshoot rather than the intended ~halved range).
//   * The perception system's audibility formula got refactored to
//     ignore the loudness multiplier altogether.
//
// Procedure:
//   1. Load GameLevel + find the priest.
//   2. Read priest world position. Compute emission point 7.5 m to
//      one side (xPos.x + 7.5).
//   3. Emit a full-loudness footstep stimulus (source = a synthetic
//      EntityID #1) at the emission point. Tick a few frames so
//      `UpdateHearingPerception` runs.
//   4. Read priest's `GetAwarenessOf(synthetic_full_source)` -- must
//      be > 0 (heard).
//   5. Emit a half-loudness footstep stimulus (source = a DIFFERENT
//      synthetic EntityID #2) at the SAME emission point. Tick a few
//      frames.
//   6. Read priest's `GetAwarenessOf(synthetic_half_source)` -- must
//      be == 0 (NOT heard).
//
// Using two distinct source EntityIDs sidesteps the awareness-decay
// timing nightmare of "emit, observe, wait for decay, emit again,
// observe again". Each source's awareness is tracked independently
// in `AgentPerceptionData::m_axPerceivedTargets`.
// ============================================================================

namespace
{
	enum Phase : int { kWQ_Start, kWQ_WaitScene, kWQ_EmitFull,
	                   kWQ_TickFull, kWQ_ReadFull, kWQ_EmitHalf,
	                   kWQ_TickHalf, kWQ_ReadHalf, kWQ_Verify, kWQ_Done };

	int                     g_iPhase = kWQ_Start;
	Zenith_EntityID         g_xPriest;
	Zenith_Maths::Vector3   g_xEmitPos(0.0f);
	float                   g_fLoudnessFull = 0.0f;
	float                   g_fLoudnessHalf = 0.0f;
	float                   g_fRadius = 0.0f;
	float                   g_fThreshold = 0.0f;
	float                   g_fEmitDistance = 0.0f;
	float                   g_fAwarenessFull = -1.0f;
	float                   g_fAwarenessHalf = -1.0f;
	int                     g_iTickCounter = 0;

	// Synthetic source IDs. The perception system tracks awareness
	// keyed by source EntityID -- the IDs don't need to correspond
	// to real entities (UpdateHearingPerception's "don't hear own
	// sounds" check compares source to agent, which fails iff source
	// == agent, never iff source is "invalid"). Use index=1/gen=1 and
	// index=2/gen=1 -- definitely not the priest's ID, and definitely
	// not equal to each other.
	Zenith_EntityID MakeSyntheticID(uint32_t uIndex)
	{
		Zenith_EntityID xId;
		xId.m_uIndex = uIndex;
		xId.m_uGeneration = 1;
		return xId;
	}

	// Tick enough frames that UpdateHearingPerception has definitely
	// processed the sound at least once. Sounds persist 0.5 s, the
	// hearing system stagger should land within that window.
	constexpr int kTICKS_AFTER_EMIT = 10;

	bool TryGetEntityPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xId);
		if (pxScene == nullptr) return false;
		Zenith_Entity xEnt = pxScene->TryGetEntity(xId);
		if (!xEnt.IsValid()) return false;
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return false;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}
}

static void Setup_P1AelfricHearing()
{
	g_iPhase = kWQ_Start;
	g_xPriest = INVALID_ENTITY_ID;
	g_xEmitPos = Zenith_Maths::Vector3(0.0f);
	g_fLoudnessFull = 0.0f;
	g_fLoudnessHalf = 0.0f;
	g_fRadius = 0.0f;
	g_fThreshold = 0.0f;
	g_fEmitDistance = 0.0f;
	g_fAwarenessFull = -1.0f;
	g_fAwarenessHalf = -1.0f;
	g_iTickCounter = 0;
}

static bool Step_P1AelfricHearing(int iFrame)
{
	switch (g_iPhase)
	{
	case kWQ_Start:
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWQ_WaitScene;
		return true;

	case kWQ_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xFound](Zenith_EntityID xId, Priest_Behaviour&)
			{ xFound = xId; });
		if (xFound.IsValid())
		{
			g_xPriest = xFound;
			// Compute the test distance from tuning values. This way
			// future tuning rebalances don't break the test as long as
			// `walk_footstep_loudness_multiplier` stays < 1.0.
			g_fLoudnessFull = DP_Tuning::Get<float>("movement.footstep_loudness");
			const float fMult =
				DP_Tuning::Get<float>("movement.walk_footstep_loudness_multiplier");
			g_fLoudnessHalf = g_fLoudnessFull * fMult;
			g_fRadius = DP_Tuning::Get<float>("movement.footstep_radius_m");
			g_fThreshold =
				DP_Tuning::Get<float>("priest.hearing_loudness_threshold");

			// Audible-up-to distance for each loudness level
			// (algebra of the audibility formula):
			//   L*(1 - d/R) = T  =>  d = R*(1 - T/L)
			const float fDFull = g_fRadius * (1.0f - g_fThreshold / g_fLoudnessFull);
			const float fDHalf = g_fRadius * (1.0f - g_fThreshold / g_fLoudnessHalf);
			// Pick the midpoint of the (D_half, D_full) sweet spot.
			g_fEmitDistance = 0.5f * (fDFull + fDHalf);

			// Get priest position and offset along +X.
			Zenith_Maths::Vector3 xPriestPos;
			if (TryGetEntityPos(g_xPriest, xPriestPos))
			{
				g_xEmitPos = xPriestPos;
				g_xEmitPos.x += g_fEmitDistance;
				g_iPhase = kWQ_EmitFull;
			}
			else
			{
				g_iPhase = kWQ_Done;
			}
		}
		else if (iFrame > 90)
		{
			g_iPhase = kWQ_Done;
		}
		return true;
	}

	case kWQ_EmitFull:
		// Source #1: synthetic source for the full-loudness emission.
		// Tick window from here to kWQ_ReadFull lets the perception
		// system process the sound at least once.
		Zenith_PerceptionSystem::EmitSoundStimulus(
			g_xEmitPos, g_fLoudnessFull, g_fRadius, MakeSyntheticID(/*uIndex=*/9001));
		g_iTickCounter = 0;
		g_iPhase = kWQ_TickFull;
		return true;

	case kWQ_TickFull:
		++g_iTickCounter;
		if (g_iTickCounter >= kTICKS_AFTER_EMIT) g_iPhase = kWQ_ReadFull;
		return true;

	case kWQ_ReadFull:
		g_fAwarenessFull =
			Zenith_PerceptionSystem::GetAwarenessOf(g_xPriest, MakeSyntheticID(9001));
		g_iPhase = kWQ_EmitHalf;
		return true;

	case kWQ_EmitHalf:
		// Source #2: synthetic source for the half-loudness emission.
		// Distinct from source #1 so awareness tracking is independent.
		Zenith_PerceptionSystem::EmitSoundStimulus(
			g_xEmitPos, g_fLoudnessHalf, g_fRadius, MakeSyntheticID(/*uIndex=*/9002));
		g_iTickCounter = 0;
		g_iPhase = kWQ_TickHalf;
		return true;

	case kWQ_TickHalf:
		++g_iTickCounter;
		if (g_iTickCounter >= kTICKS_AFTER_EMIT) g_iPhase = kWQ_ReadHalf;
		return true;

	case kWQ_ReadHalf:
		g_fAwarenessHalf =
			Zenith_PerceptionSystem::GetAwarenessOf(g_xPriest, MakeSyntheticID(9002));
		g_iPhase = kWQ_Verify;
		return true;

	case kWQ_Verify:
		std::printf("[P1AelfricHearing] L_full=%.3f L_half=%.3f T=%.3f R=%.2f D=%.2f -> awareness_full=%.3f awareness_half=%.3f\n",
			g_fLoudnessFull, g_fLoudnessHalf, g_fThreshold, g_fRadius,
			g_fEmitDistance, g_fAwarenessFull, g_fAwarenessHalf);
		std::fflush(stdout);
		g_iPhase = kWQ_Done;
		return false;

	case kWQ_Done:
	default:
		return false;
	}
}

static bool Verify_P1AelfricHearing()
{
	if (!g_xPriest.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1AelfricHearing: priest not found in GameLevel");
		return false;
	}
	// Pre-flight: the tuning values must actually produce a meaningful
	// sweet spot. If the multiplier got bumped to 1.0 (regression that
	// makes walk-quiet emit at full volume), full and half are equal
	// and the test would assert nothing useful.
	if (g_fLoudnessHalf >= g_fLoudnessFull)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1AelfricHearing: tuning anomaly -- L_half (%.3f) >= L_full (%.3f). walk_footstep_loudness_multiplier should reduce loudness; check Tuning.json",
			g_fLoudnessHalf, g_fLoudnessFull);
		return false;
	}
	if (g_fEmitDistance <= 0.0f || g_fEmitDistance >= g_fRadius)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1AelfricHearing: computed emit distance %.2f is out of bounds (radius=%.2f). Check tuning values",
			g_fEmitDistance, g_fRadius);
		return false;
	}
	if (g_fAwarenessFull <= 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1AelfricHearing: priest did NOT hear the FULL-loudness emit (awareness=%.3f) at distance %.2fm. Priest's hearing config may have been overridden -- check Priest_Behaviour::OnAwake reads priest.hearing_loudness_threshold from tuning",
			g_fAwarenessFull, g_fEmitDistance);
		return false;
	}
	if (g_fAwarenessHalf > 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1AelfricHearing: priest HEARD the HALF-loudness emit (awareness=%.3f) at distance %.2fm but should NOT have. walk_footstep_loudness_multiplier may have been bypassed in EmitSoundStimulus, or the audibility formula changed",
			g_fAwarenessHalf, g_fEmitDistance);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1AelfricHearingTest = {
	"Test_P1WalkQuiet_AelfricEffectiveHearingHalved",
	&Setup_P1AelfricHearing,
	&Step_P1AelfricHearing,
	&Verify_P1AelfricHearing,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1AelfricHearingTest);

#endif // ZENITH_INPUT_SIMULATOR
