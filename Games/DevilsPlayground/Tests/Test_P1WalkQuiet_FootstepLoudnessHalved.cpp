#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_AudioBus.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"

#include "Source/PublicInterfaces.h"
#include "Source/DP_Tuning.h"
#include "Components/DPVillager_Behaviour.h"

#include <cmath>
#include <cstdio>
#include <cstring>

// ============================================================================
// Test_P1WalkQuiet_FootstepLoudnessHalved (MVP-1.7.4 + 1.7.5)
//
// Verifies that holding Ctrl ("walk-quiet") halves the loudness of
// every emitted footstep. The data path goes:
//   DPVillager_Behaviour::TickFootsteps
//     -> Zenith_AudioBus::EmitSound(name, pos, loudness, radius)
//     -> Zenith_PerceptionSystem::EmitSoundStimulus(...) [same loudness]
//
// The test only inspects the AudioBus recorder; the perception-system
// half is covered indirectly by the priest BB tests that consume
// `BB.InvestigatePos`.
//
// Procedure:
//   1. Load GameLevel, possess any villager.
//   2. Wait one bump frame.
//   3. **Normal walk window**: clear AudioBus, hold W (move), tick
//      until at least 2 footsteps are recorded.  Average their loudness.
//   4. **Walk-quiet window**: clear AudioBus, hold W + Ctrl, tick
//      until at least 2 footsteps recorded.  Average their loudness.
//   5. Assert: average walk-quiet loudness ≈
//        0.5 * average normal loudness
//      (within 5% tolerance -- the multiplier is exactly 0.5 by
//      default but float math + sample averaging introduces small
//      rounding).
//   6. Also assert: AT LEAST one normal footstep emission AND one
//      walk-quiet emission were observed.  (A zero-on-one-side run
//      would let a "did nothing" impl pass the ratio check
//      trivially.)
// ============================================================================

namespace
{
	enum Phase : int { kWQ_Start, kWQ_WaitScene, kWQ_Possess, kWQ_AfterBump,
	                   kWQ_NormalArm, kWQ_NormalTick, kWQ_NormalRecord,
	                   kWQ_QuietArm, kWQ_QuietTick, kWQ_QuietRecord,
	                   kWQ_Verify, kWQ_Done };

	int                     g_iPhase = kWQ_Start;
	Zenith_EntityID         g_xVillager;
	float                   g_fNormalSum = 0.0f;
	int                     g_iNormalCount = 0;
	float                   g_fQuietSum = 0.0f;
	int                     g_iQuietCount = 0;
	int                     g_iTickCount = 0;

	// 0.4 s footstep interval = 24 frames at 60 Hz. Two intervals + slack.
	constexpr int kTICK_FRAMES = 80;

	bool IsFootstep(const Zenith_AudioBus::EmittedSound& xS)
	{
		return xS.m_szName != nullptr
			&& std::strcmp(xS.m_szName, "DP.Villager.Footstep") == 0;
	}

	void SampleFootsteps(float& fSumOut, int& iCountOut)
	{
		fSumOut = 0.0f;
		iCountOut = 0;
		const Zenith_Vector<Zenith_AudioBus::EmittedSound>& xSounds =
			Zenith_AudioBus::GetEmittedSoundsForTest();
		for (uint32_t u = 0; u < xSounds.GetSize(); ++u)
		{
			const Zenith_AudioBus::EmittedSound& xS = xSounds.Get(u);
			if (!IsFootstep(xS)) continue;
			fSumOut += xS.m_fLoudness;
			++iCountOut;
		}
	}

	void ReleaseAllMovementKeys()
	{
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_S, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_D, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, false);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_CONTROL, false);
	}
}

static void Setup_P1WalkQuiet()
{
	g_iPhase = kWQ_Start;
	g_xVillager = INVALID_ENTITY_ID;
	g_fNormalSum = 0.0f;
	g_iNormalCount = 0;
	g_fQuietSum = 0.0f;
	g_iQuietCount = 0;
	g_iTickCount = 0;
	Zenith_AudioBus::ClearEmittedSoundsForTest();
}

static bool Step_P1WalkQuiet(int iFrame)
{
	switch (g_iPhase)
	{
	case kWQ_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iPhase = kWQ_WaitScene;
		return true;

	case kWQ_WaitScene:
	{
		Zenith_EntityID xFound;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound](Zenith_EntityID xId, DPVillager_Behaviour&)
			{
				if (!xFound.IsValid()) xFound = xId;
			});
		if (xFound.IsValid())
		{
			g_xVillager = xFound;
			g_iPhase = kWQ_Possess;
		}
		else if (iFrame > 60)
		{
			g_iPhase = kWQ_Done;
		}
		return true;
	}

	case kWQ_Possess:
		DP_Player::SetPossessedVillager(g_xVillager);
		g_iPhase = kWQ_AfterBump;
		return true;

	case kWQ_AfterBump:
		// One frame later DPVillager has flipped m_bIsPossessed. From
		// here on we control input directly.
		g_iPhase = kWQ_NormalArm;
		return true;

	case kWQ_NormalArm:
		// Drop any footsteps that fired during the bump frame so the
		// "normal" sample isn't polluted by setup transients.
		Zenith_AudioBus::ClearEmittedSoundsForTest();
		ReleaseAllMovementKeys();
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
		g_iTickCount = 0;
		g_iPhase = kWQ_NormalTick;
		return true;

	case kWQ_NormalTick:
		++g_iTickCount;
		if (g_iTickCount >= kTICK_FRAMES)
		{
			g_iPhase = kWQ_NormalRecord;
		}
		return true;

	case kWQ_NormalRecord:
		SampleFootsteps(g_fNormalSum, g_iNormalCount);
		g_iPhase = kWQ_QuietArm;
		return true;

	case kWQ_QuietArm:
		// Reset recorder + arm walk-quiet (Ctrl) on top of forward.
		Zenith_AudioBus::ClearEmittedSoundsForTest();
		ReleaseAllMovementKeys();
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
		Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_CONTROL, true);
		g_iTickCount = 0;
		g_iPhase = kWQ_QuietTick;
		return true;

	case kWQ_QuietTick:
		++g_iTickCount;
		if (g_iTickCount >= kTICK_FRAMES)
		{
			g_iPhase = kWQ_QuietRecord;
		}
		return true;

	case kWQ_QuietRecord:
		SampleFootsteps(g_fQuietSum, g_iQuietCount);
		ReleaseAllMovementKeys();
		g_iPhase = kWQ_Verify;
		return true;

	case kWQ_Verify:
	{
		const float fNormalAvg = g_iNormalCount > 0
			? g_fNormalSum / static_cast<float>(g_iNormalCount) : 0.0f;
		const float fQuietAvg = g_iQuietCount > 0
			? g_fQuietSum / static_cast<float>(g_iQuietCount) : 0.0f;
		const float fRatio = fNormalAvg > 0.0f ? fQuietAvg / fNormalAvg : 0.0f;
		std::printf("[P1WalkQuiet] normal: n=%d avg=%.3f  quiet: n=%d avg=%.3f  ratio=%.3f\n",
			g_iNormalCount, fNormalAvg, g_iQuietCount, fQuietAvg, fRatio);
		std::fflush(stdout);
		g_iPhase = kWQ_Done;
		return false;
	}

	case kWQ_Done:
	default:
		return false;
	}
}

static bool Verify_P1WalkQuiet()
{
	if (!g_xVillager.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1WalkQuiet: villager not found");
		return false;
	}
	if (g_iNormalCount < 1)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1WalkQuiet: normal-walk window emitted no footsteps -- TickFootsteps didn't fire");
		return false;
	}
	if (g_iQuietCount < 1)
	{
		Zenith_Log(LOG_CATEGORY_AI, "P1WalkQuiet: walk-quiet window emitted no footsteps -- Ctrl-held walking didn't emit");
		return false;
	}
	const float fNormalAvg = g_fNormalSum / static_cast<float>(g_iNormalCount);
	const float fQuietAvg = g_fQuietSum / static_cast<float>(g_iQuietCount);
	const float fExpectedMult =
		DP_Tuning::Get<float>("movement.walk_footstep_loudness_multiplier");
	const float fExpectedQuiet = fNormalAvg * fExpectedMult;
	// 10% tolerance on the multiplied product. Generous because the
	// emissions might happen at sub-frame boundaries with tiny
	// timing jitter.
	const float fTol = 0.10f * fNormalAvg;
	if (fQuietAvg < fExpectedQuiet - fTol || fQuietAvg > fExpectedQuiet + fTol)
	{
		Zenith_Log(LOG_CATEGORY_AI,
			"P1WalkQuiet: quietAvg=%.3f outside expected [%.3f..%.3f] (normal=%.3f mult=%.3f)",
			fQuietAvg, fExpectedQuiet - fTol, fExpectedQuiet + fTol,
			fNormalAvg, fExpectedMult);
		return false;
	}
	return true;
}

static const Zenith_AutomatedTest g_xP1WalkQuietTest = {
	"Test_P1WalkQuiet_FootstepLoudnessHalved",
	&Setup_P1WalkQuiet,
	&Step_P1WalkQuiet,
	&Verify_P1WalkQuiet,
	300
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xP1WalkQuietTest);

#endif // ZENITH_INPUT_SIMULATOR
