#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Core.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "Source/PublicInterfaces.h"
#include "Components/DPVillager_Behaviour.h"

// ============================================================================
// LifeTimer_Test
//
// Exercises DPVillager_Behaviour::TickLife on a possessed villager. Possession
// is set explicitly via DP_Player::SetPossessedVillager (the click-to-possess
// raycast path is covered separately by DPPlayerController_Behaviour wiring).
//
// Phases:
//   kLT_Start         — request LoadSceneByIndex(1, SINGLE)
//   kLT_WaitScene     — wait for DPVillager to appear
//   kLT_RecordBaseline — possess + capture the initial life value
//   kLT_Tick          — run kTICK_FRAMES frames, measure wall-clock elapsed
//   kLT_Verify        — life drop ≈ measured wall-clock elapsed
//
// The previous version of this test assumed a fixed dt of 0.01666s, but the
// harness runs at variable dt by default and the actual elapsed time depends
// on frame rate. We now measure wall-clock elapsed across the tick window
// and compare against the life delta — independent of the dt regime.
// ============================================================================

namespace
{
	enum Phase : int { kLT_Start, kLT_WaitScene, kLT_RecordBaseline,
	                   kLT_Tick, kLT_Verify, kLT_Done };

	int             g_iLTPhase    = kLT_Start;
	Zenith_EntityID g_xLTVillager;
	float           g_fInitialLife = 0.0f;
	float           g_fFinalLife   = 0.0f;
	int             g_iTicks       = 0;
	bool            g_bPassed      = false;
	float           g_fTickStartSec = 0.0f;
	float           g_fElapsedSec   = 0.0f;

	// Run 60 frames. The actual game-time duration depends on Zenith_Core's
	// dt (which the InputSimulator may pin to fixed-dt or leave as wall-clock)
	// — we measure it directly via Zenith_Core::GetTimePassed and compare
	// against the life drop, so the expected-vs-actual envelope below stays
	// meaningful regardless of dt regime.
	constexpr int   kTICK_FRAMES = 60;
}

static void Setup_LifeTimer()
{
	g_iLTPhase     = kLT_Start;
	g_xLTVillager  = INVALID_ENTITY_ID;
	g_fInitialLife = 0.0f;
	g_fFinalLife   = 0.0f;
	g_iTicks       = 0;
	g_bPassed      = false;
	g_fElapsedSec  = 0.0f;
}

static bool Step_LifeTimer(int iFrame)
{
	switch (g_iLTPhase)
	{
	case kLT_Start:
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		g_iLTPhase = kLT_WaitScene;
		return true;

	case kLT_WaitScene:
	{
		Zenith_EntityID xFound;
		float fLife = 0.0f;
		DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
			[&xFound, &fLife](Zenith_EntityID xId, DPVillager_Behaviour& xV)
			{
				xFound = xId;
				fLife  = xV.GetRemainingLife();
			});
		if (xFound.IsValid() && fLife > 0.0f)
		{
			g_xLTVillager = xFound;
			g_iLTPhase    = kLT_RecordBaseline;
		}
		else if (iFrame > 60)
		{
			g_iLTPhase = kLT_Done;   // bail
		}
		return true;
	}

	case kLT_RecordBaseline:
	{
		// Possess the villager. Per the behaviour spec, the next frame's
		// OnUpdate observes possession and bumps remaining life back to max,
		// so we capture the baseline AFTER one settling frame, not before.
		DP_Player::SetPossessedVillager(g_xLTVillager);
		g_iLTPhase = kLT_Tick;
		return true;
	}

	case kLT_Tick:
	{
		if (g_iTicks == 0)
		{
			// Capture initial life + start time on the first tick frame —
			// possession was just set; the OnUpdate that fires this frame
			// bumps life to max AND ticks it once, so what we read here is
			// "max - 1*dt". Snapshot Zenith_Core's time-passed so the verify
			// step can compare the life delta against the same game-time
			// clock TickLife uses (Zenith_Core::GetDt), independent of
			// wall-clock framerate or fixed-dt override.
			DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
				[](Zenith_EntityID, DPVillager_Behaviour& xV)
				{
					g_fInitialLife = xV.GetRemainingLife();
				});
			g_fTickStartSec = g_xEngine.Frame().GetTimePassed();
		}
		++g_iTicks;
		if (g_iTicks >= kTICK_FRAMES)
		{
			DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>(
				[](Zenith_EntityID, DPVillager_Behaviour& xV)
				{
					g_fFinalLife = xV.GetRemainingLife();
				});
			g_fElapsedSec = g_xEngine.Frame().GetTimePassed() - g_fTickStartSec;
			g_iLTPhase = kLT_Verify;
		}
		return true;
	}

	case kLT_Verify:
	{
		const float fDrop = g_fInitialLife - g_fFinalLife;
		// Compare measured life drop against measured game-time elapsed
		// (Zenith_Core::GetTimePassed). TickLife uses fDt, so over a window
		// of g_fElapsedSec game-time seconds the timer should drop by
		// ~g_fElapsedSec seconds (modulo a couple of possession-bump frames
		// at the boundaries).
		//
		// 50% tolerance is generous but mirrors the original test's ±50%
		// band: scene-load jitter, the harness's pre-first-frame possession
		// bump, and OnUpdate scheduling can all knock the ratio off 1:1. We
		// additionally clamp the floor to 0.05 s so the test doesn't pass
		// against a stuck-zero timer just because the elapsed window itself
		// happened to be tiny.
		const float fExpected  = std::max(g_fElapsedSec, 0.05f);
		const float fTolerance = 0.5f * fExpected;
		const float fLow       = fExpected - fTolerance;
		const float fHigh      = fExpected + fTolerance;
		g_bPassed = (fDrop >= fLow) && (fDrop <= fHigh);
		std::printf("[LifeTimer] elapsed=%.3fs drop=%.3fs expected=[%.3f..%.3f] passed=%d\n",
			g_fElapsedSec, fDrop, fLow, fHigh, (int)g_bPassed);
		std::fflush(stdout);
		g_iLTPhase = kLT_Done;
		return false;
	}

	case kLT_Done:
	default:
		return false;
	}
}

static bool Verify_LifeTimer()
{
	return g_bPassed
	    && g_xLTVillager.IsValid()
	    && g_fInitialLife > 0.0f
	    && g_fFinalLife   < g_fInitialLife;
}

static const Zenith_AutomatedTest g_xLifeTimerTest = {
	"LifeTimer_Test",
	&Setup_LifeTimer,
	&Step_LifeTimer,
	&Verify_LifeTimer,
	240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xLifeTimerTest);

#endif // ZENITH_INPUT_SIMULATOR
