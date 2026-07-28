#include "Zenith.h"

// ============================================================================
// ZM_Tests_TrainerSightFsm -- S7 item 3 SC6 unit tests for the PURE trainer
// sight state machine, the PURE re-engagement gate and the process-global
// session latch.
//
// Everything here is PURE: no ECS, no scene, no physics body, no raycast, no UI,
// no baked asset, no engine instance -- plain values in, one action out. Every
// fixture is deterministic and hermetic, so no RequestSkip is needed.
//
// Category ZM_Interaction -- the SAME category as the SC1/SC2/SC3 interaction
// units, because this is the same feature area: the FSM consumes the SC3 cone's
// answer as a bool and never re-derives it.
//
// OCCLUSION APPEARS HERE ONLY AS AN INPUT (m_bSightLineClear). The real raycast
// is ZM_ProbeTrainerSightLine's job and is unit-tested against hermetically
// created static bodies in ZM_Tests_TrainerSightProbe.cpp; what THIS file pins is
// that the occlusion answer is ANDed into the sighting decision rather than being
// decorative.
//
// The FSM units are each a DELTA from MakePassingInputs(): one flipped field per
// unit, so a red test names exactly one clause of the specification.
// ============================================================================

#include <cmath>     // std::isfinite (the poisoned-accumulator assertion)
#include <limits>    // quiet_NaN / infinity (the totality sweep)

#include "Core/Zenith_TestFramework.h"
#include "UnitTests/Zenith_AssertCapture.h"                     // the totality proof
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"                // ZM_STORY_FLAG_* / ZM_IsStoryFlagSet
#include "Zenithmon/Source/Data/ZM_TrainerData.h"               // the shipped roster the gate units read
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"                // ZM_StoryFlagSet (the flagless-row proof)

namespace
{
	// One 60Hz frame. Spelled once so the confirm-window arithmetic below is
	// readable as "N frames of the 0.5s window" rather than as a magic float.
	constexpr float fFRAME_DT = 1.0f / 60.0f;

	// Every clause satisfied at once: the gate open, the player inside the cone,
	// the line clear, and nothing else owning the screen. EVERY FSM unit below is a
	// delta from this, so the one field a unit flips IS what that unit tests.
	ZM_TrainerSightInputs MakePassingInputs()
	{
		ZM_TrainerSightInputs xInputs;
		xInputs.m_bMayEngage      = true;
		xInputs.m_bTargetInSight  = true;
		xInputs.m_bSightLineClear = true;
		xInputs.m_bChannelBusy    = false;
		xInputs.m_fDeltaSeconds   = fFRAME_DT;
		return xInputs;
	}

	// A default-constructed ZM_TrainerSightFsmTuning already IS the shipped tuning
	// (0.5s confirm window); this exists purely to say so at every call site.
	ZM_TrainerSightFsmTuning MakeShippedTuning()
	{
		return ZM_TrainerSightFsmTuning();
	}

	// Drive the one raise every "starts from ENGAGED" unit depends on, and PROVE it
	// happened. A helper that merely stepped would let a broken machine hand those
	// units a silently cold watcher to assert against.
	void RaiseOnce(ZM_TrainerSightFsm& xFsm)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(MakePassingInputs(), MakeShippedTuning());
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
			"a cold watcher with every clause satisfied must raise on its first Step "
			"(got %s) -- every unit that starts from ENGAGED depends on this",
			ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
			"...and must be ENGAGED afterwards (got %s)",
			ZM_TrainerSightStateName(xFsm.GetState()));
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
			"...and must have counted exactly one raise");
	}
}

// ---- The rising edge: one raise per continuous spotting -----------------------

ZENITH_TEST(ZM_Interaction, Fsm_ColdWatcherRaisesExactlyOnceOnFirstSighting)
{
	ZM_TrainerSightFsm xFsm;
	const ZM_TrainerSightInputs xInputs = MakePassingInputs();
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	const ZM_TRAINER_SIGHT_ACTION eFirst = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eFirst, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the first Step of a cold watcher that can see the player must raise (got %s)",
		ZM_TrainerSightActionName(eFirst));
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"a raise must move the machine to ENGAGED (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"exactly one raise has been emitted");

	// Steps 2..30 with the IDENTICAL inputs: the player is still standing there, so
	// this is the SAME spotting and must produce nothing more.
	for (u_int u = 2u; u <= 30u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"step %u of ONE continuous spotting raised again (got %s) -- spotted once, "
			"not once per frame", u, ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
			"the raise count moved on step %u of one continuous spotting", u);
	}
}

// ---- The gate arm: a trainer who may not engage never raises ------------------

ZENITH_TEST(ZM_Interaction, Fsm_DefeatedTrainerIsNeverSpotted)
{
	ZM_TrainerSightFsm xFsm;
	ZM_TrainerSightInputs xInputs = MakePassingInputs();
	xInputs.m_bMayEngage = false;
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	for (u_int u = 0u; u < 30u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"a trainer the gate has closed raised on step %u (got %s)",
			u, ZM_TrainerSightActionName(eAction));
	}
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_WATCHING,
		"a gated trainer must stay WATCHING, not burn its ENGAGED state (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
		"a gated trainer must never have raised");

	// ANTI-VACUITY: the ONLY thing that was wrong is the gate, so opening it must
	// raise immediately. Without this the unit would also pass on an inert fixture.
	xInputs.m_bMayEngage = true;
	const ZM_TRAINER_SIGHT_ACTION eOpened = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eOpened, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"opening the gate on an otherwise-passing fixture must raise (got %s) -- "
		"otherwise this unit proved nothing about the gate",
		ZM_TrainerSightActionName(eOpened));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"the re-opened gate must have produced exactly one raise");
}

// ---- Occlusion is ANDed into sight, not decorative ---------------------------

ZENITH_TEST(ZM_Interaction, Fsm_OccludedTargetInsideTheConeIsNotSpotted)
{
	ZM_TrainerSightFsm xFsm;
	ZM_TrainerSightInputs xInputs = MakePassingInputs();
	xInputs.m_bTargetInSight  = true;    // INSIDE the SC3 cone...
	xInputs.m_bSightLineClear = false;   // ...but behind cover.
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	for (u_int u = 0u; u < 30u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"a target inside the cone but behind cover was spotted on step %u (got %s) "
			"-- occlusion must be ANDed into the sighting decision", u,
			ZM_TrainerSightActionName(eAction));
	}
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
		"a blocked line must never raise");
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_WATCHING,
		"a blocked trainer keeps watching (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));

	// ANTI-VACUITY: clearing the line is the ONLY change, and it must raise.
	xInputs.m_bSightLineClear = true;
	const ZM_TRAINER_SIGHT_ACTION eCleared = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eCleared, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"stepping out from behind cover must raise (got %s)",
		ZM_TrainerSightActionName(eCleared));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"the unblocked line must have produced exactly one raise");
}

// ---- Leaving sight re-arms, and a genuinely NEW spotting fires ---------------

ZENITH_TEST(ZM_Interaction, Fsm_LeavingSightRearmsTheWatcher)
{
	ZM_TrainerSightFsm xFsm;
	RaiseOnce(xFsm);

	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	ZM_TrainerSightInputs xLeft = MakePassingInputs();
	xLeft.m_bTargetInSight = false;
	const ZM_TRAINER_SIGHT_ACTION eLeft = xFsm.Step(xLeft, xTuning);
	ZENITH_ASSERT_EQ(eLeft, ZM_TRAINER_SIGHT_ACTION_NONE,
		"leaving the cone is not itself an action (got %s)",
		ZM_TrainerSightActionName(eLeft));
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_WATCHING,
		"a target that left the cone must RE-ARM the watcher (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"re-arming must not itself raise");

	// Walking back in is a NEW spotting and must fire again.
	const ZM_TRAINER_SIGHT_ACTION eReturned = xFsm.Step(MakePassingInputs(), xTuning);
	ZENITH_ASSERT_EQ(eReturned, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"walking back into the cone is a NEW spotting and must raise (got %s)",
		ZM_TrainerSightActionName(eReturned));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 2u,
		"the second spotting must be the second raise");
}

// ---- A busy screen DEFERS the raise, it never consumes it ---------------------

ZENITH_TEST(ZM_Interaction, Fsm_BusyChannelDefersTheRaiseRatherThanConsumingIt)
{
	ZM_TrainerSightFsm xFsm;
	ZM_TrainerSightInputs xInputs = MakePassingInputs();
	xInputs.m_bChannelBusy = true;
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	for (u_int u = 0u; u < 30u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"a raise into a busy screen would be silently dropped, so step %u must "
			"emit nothing (got %s)", u, ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_WATCHING,
			"a deferred raise must NOT burn the ENGAGED state on step %u (got %s)",
			u, ZM_TrainerSightStateName(xFsm.GetState()));
	}
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
		"nothing was raised while the screen was busy");

	// The channel frees up: the deferred raise is still owed and must arrive.
	xInputs.m_bChannelBusy = false;
	const ZM_TRAINER_SIGHT_ACTION eFreed = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eFreed, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the deferred raise must fire once the screen is free (got %s) -- deferred, "
		"not consumed", ZM_TrainerSightActionName(eFreed));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"the freed channel must have produced exactly one raise");
}

// ---- A raise nobody took re-arms, but only after the window -------------------

ZENITH_TEST(ZM_Interaction, Fsm_UnconfirmedRaiseRearmsAfterTheConfirmWindow)
{
	ZM_TrainerSightFsm xFsm;
	RaiseOnce(xFsm);

	// The player is still in the cone and the screen NEVER goes busy: the dispatch
	// was silently dropped (a wild encounter latched in the same frame).
	const ZM_TrainerSightInputs xInputs = MakePassingInputs();
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	// 20 frames is 0.333s of the shipped 0.5s window: the re-arm must be TIMED, so
	// nothing may have happened yet.
	for (u_int u = 0u; u < 20u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"the drop timer must not raise while it is running (step %u, got %s)",
			u, ZM_TrainerSightActionName(eAction));
	}
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"the re-arm must be TIMED, not immediate -- 0.333s into a 0.5s window the "
		"machine is still ENGAGED (got %s)", ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"nothing may have re-raised before the window elapsed");
	ZENITH_ASSERT_FALSE(xFsm.IsRaiseConfirmed(),
		"a raise the screen never acknowledged is NOT confirmed");

	// Keep stepping until the window elapses. Bounded so a machine that never
	// re-arms fails on the assertions below rather than hanging the boot suite.
	u_int uExtraSteps = 0u;
	while (xFsm.GetState() == ZM_TRAINER_SIGHT_ENGAGED && uExtraSteps < 200u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"re-arming is not itself an action (extra step %u, got %s)",
			uExtraSteps, ZM_TrainerSightActionName(eAction));
		++uExtraSteps;
	}
	ZENITH_ASSERT_LT(uExtraSteps, 200u,
		"a dropped raise must eventually re-arm the watcher, otherwise the trainer "
		"falls permanently silent with the player standing in the cone");
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_WATCHING,
		"past the confirm window an unconfirmed raise re-arms (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"re-arming must not itself raise");

	const ZM_TRAINER_SIGHT_ACTION eRetry = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eRetry, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the re-armed watcher must try again (got %s)",
		ZM_TrainerSightActionName(eRetry));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 2u,
		"the retry is the second raise");
}

// ---- A raise the screen TOOK never re-fires ----------------------------------

ZENITH_TEST(ZM_Interaction, Fsm_ConfirmedRaiseNeverRearmsWhileTheTargetStaysInSight)
{
	ZM_TrainerSightFsm xFsm;
	RaiseOnce(xFsm);

	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	// ONE busy frame: the battle round trip started, so the raise demonstrably TOOK.
	ZM_TrainerSightInputs xBusy = MakePassingInputs();
	xBusy.m_bChannelBusy = true;
	const ZM_TRAINER_SIGHT_ACTION eBusy = xFsm.Step(xBusy, xTuning);
	ZENITH_ASSERT_EQ(eBusy, ZM_TRAINER_SIGHT_ACTION_NONE,
		"the busy frame is an acknowledgement, not an action (got %s)",
		ZM_TrainerSightActionName(eBusy));
	ZENITH_ASSERT_TRUE(xFsm.IsRaiseConfirmed(),
		"a screen that went busy after our raise CONFIRMS it");

	// 10 simulated seconds of the player standing in the cone after the battle.
	const ZM_TrainerSightInputs xIdle = MakePassingInputs();
	for (u_int u = 0u; u < 600u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xIdle, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"a COMPLETED battle must not immediately re-fire (step %u, got %s)",
			u, ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_TRUE(xFsm.IsRaiseConfirmed(),
			"the confirmation is LATCHED -- the channel going idle again at the end "
			"of the round trip must not read as 'the raise was dropped' (step %u)", u);
	}
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"ten seconds inside the cone after a taken raise must still be ONE raise");
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"a confirmed trainer stays ENGAGED until the target leaves (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
}

// ---- Degenerate dt contributes NOTHING ---------------------------------------

ZENITH_TEST(ZM_Interaction, Fsm_DegenerateDeltaNeverAccumulatesOrRearms)
{
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();
	constexpr u_int uDEGENERATE_COUNT = 5u;
	const float afDEGENERATE[uDEGENERATE_COUNT] = { fNaN, fInf, -fInf, -1.0f, 0.0f };

	ZM_TrainerSightFsm xFsm;
	RaiseOnce(xFsm);

	// Deliberately left UNCONFIRMED, so the accumulator is live and a garbage dt
	// has somewhere to do damage.
	ZENITH_ASSERT_FALSE(xFsm.IsRaiseConfirmed(),
		"the drop timer must be running for this unit to mean anything");

	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
	for (u_int u = 0u; u < 1000u; ++u)
	{
		ZM_TrainerSightInputs xInputs = MakePassingInputs();
		xInputs.m_fDeltaSeconds = afDEGENERATE[u % uDEGENERATE_COUNT];
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"a garbage frame must never produce an action (step %u, got %s)",
			u, ZM_TrainerSightActionName(eAction));
	}

	// FINITENESS is the load-bearing assertion: a NaN accumulator would compare
	// false against the window and leave the STATE looking perfectly healthy.
	ZENITH_ASSERT_TRUE(std::isfinite(xFsm.GetConfirmElapsedSeconds()),
		"a degenerate dt poisoned the confirm accumulator -- it must contribute "
		"NOTHING, not propagate NaN");
	ZENITH_ASSERT_EQ_FLOAT(xFsm.GetConfirmElapsedSeconds(), 0.0f, 0.0f,
		"1000 degenerate frames must accumulate EXACTLY zero elapsed time");
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"no garbage frame may re-arm the watcher (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"no garbage frame may raise");
}

// ---- Reset ------------------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Fsm_ResetReturnsAColdWatcher)
{
	ZM_TrainerSightFsm xFsm;
	RaiseOnce(xFsm);

	const ZM_TrainerSightInputs xInputs = MakePassingInputs();
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	// Advance the accumulator (10 frames of the 0.5s window)...
	for (u_int u = 0u; u < 10u; ++u)
	{
		xFsm.Step(xInputs, xTuning);
	}
	ZENITH_ASSERT_GT(xFsm.GetConfirmElapsedSeconds(), 0.0f,
		"the fixture must have a POPULATED accumulator before Reset, or this unit "
		"is asserting against an already-cold machine");

	// ...and confirm the raise, so every single field Reset clears is dirty first.
	ZM_TrainerSightInputs xBusy = MakePassingInputs();
	xBusy.m_bChannelBusy = true;
	xFsm.Step(xBusy, xTuning);
	ZENITH_ASSERT_TRUE(xFsm.IsRaiseConfirmed(),
		"the fixture must be CONFIRMED before Reset for the same reason");
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"the fixture must be ENGAGED before Reset (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"the fixture must have a non-zero raise count before Reset");

	xFsm.Reset();

	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_WATCHING,
		"Reset must return a cold WATCHING machine (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
		"Reset must clear the raise count");
	ZENITH_ASSERT_EQ_FLOAT(xFsm.GetConfirmElapsedSeconds(), 0.0f, 0.0f,
		"Reset must clear the confirm accumulator");
	ZENITH_ASSERT_FALSE(xFsm.IsRaiseConfirmed(),
		"Reset must clear the confirmation latch");
}

// ---- Totality ---------------------------------------------------------------

// THE totality proof, in the ZM_Tests_TrainerData local-hit-count form. Written
// that way for two mandatory reasons: (1) NO ZENITH_ASSERT_* may appear INSIDE
// the scope -- while a capture scope is active Zenith_TestRunner::HandleFailure
// swallows framework failures and merely bumps the hit count, so an in-scope
// assertion could never red this test; (2) the count MUST be copied to a local
// before the closing brace, because ~Zenith_AssertCaptureScope restores the
// previous hit count. Scopes do not nest.
ZENITH_TEST(ZM_Interaction, Fsm_StepNeverAssertsOnAnyDegenerateInput)
{
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();

	constexpr u_int uDELTA_COUNT  = 6u;
	constexpr u_int uWINDOW_COUNT = 4u;
	const float afDELTAS[uDELTA_COUNT]   = { fNaN, fInf, -fInf, -1.0f, 0.0f, fFRAME_DT };
	const float afWINDOWS[uWINDOW_COUNT] = { fNaN, -1.0f, 0.0f, 0.5f };

	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;

		// The full cross product: 16 bool combinations x 6 deltas x 4 windows, each
		// driven from BOTH arms of the machine.
		for (u_int uBits = 0u; uBits < 16u; ++uBits)
		{
			for (u_int uDelta = 0u; uDelta < uDELTA_COUNT; ++uDelta)
			{
				for (u_int uWindow = 0u; uWindow < uWINDOW_COUNT; ++uWindow)
				{
					ZM_TrainerSightInputs xInputs;
					xInputs.m_bMayEngage      = (uBits & 1u) != 0u;
					xInputs.m_bTargetInSight  = (uBits & 2u) != 0u;
					xInputs.m_bSightLineClear = (uBits & 4u) != 0u;
					xInputs.m_bChannelBusy    = (uBits & 8u) != 0u;
					xInputs.m_fDeltaSeconds   = afDELTAS[uDelta];

					ZM_TrainerSightFsmTuning xTuning;
					xTuning.m_fRaiseConfirmSeconds = afWINDOWS[uWindow];

					// From a COLD watcher...
					ZM_TrainerSightFsm xCold;
					const ZM_TRAINER_SIGHT_ACTION eCold = xCold.Step(xInputs, xTuning);

					// ...and from an ENGAGED one, so the second arm is swept too.
					ZM_TrainerSightFsm xEngaged;
					const ZM_TRAINER_SIGHT_ACTION eSeed =
						xEngaged.Step(MakePassingInputs(), MakeShippedTuning());
					const ZM_TRAINER_SIGHT_ACTION eEngaged = xEngaged.Step(xInputs, xTuning);

					(void)eCold; (void)eSeed; (void)eEngaged;
				}
			}
		}

		// The name functions over their FULL walkable range, one PAST the last
		// enumerator included: they are log-format arguments, so they must answer
		// something rather than index off the end of a table.
		for (u_int u = 0u; u <= (u_int)ZM_TRAINER_SIGHT_STATE_COUNT; ++u)
		{
			const char* szState = ZM_TrainerSightStateName((ZM_TRAINER_SIGHT_STATE)u);
			(void)szState;
		}
		for (u_int u = 0u; u <= (u_int)ZM_TRAINER_SIGHT_ACTION_COUNT; ++u)
		{
			const char* szAction = ZM_TrainerSightActionName((ZM_TRAINER_SIGHT_ACTION)u);
			(void)szAction;
		}

		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"the sight FSM asserted on an argument -- Zenith_Assert breaks in EVERY "
		"config and the whole unit suite runs at boot, so this would kill the whole "
		"boot unit run rather than fail one test");

	// ...and the answers are still the DEFINED ones outside the scope, so this unit
	// is not merely "nothing crashed".
	ZM_TrainerSightFsm xFresh;
	const ZM_TRAINER_SIGHT_ACTION eFresh = xFresh.Step(MakePassingInputs(), MakeShippedTuning());
	ZENITH_ASSERT_EQ(eFresh, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"a fully-passing cold Step must still RAISE (got %s)",
		ZM_TrainerSightActionName(eFresh));

	ZM_TrainerSightInputs xGated = MakePassingInputs();
	xGated.m_bMayEngage = false;
	ZM_TrainerSightFsm xGatedFsm;
	const ZM_TRAINER_SIGHT_ACTION eGated = xGatedFsm.Step(xGated, MakeShippedTuning());
	ZENITH_ASSERT_EQ(eGated, ZM_TRAINER_SIGHT_ACTION_NONE,
		"a gated Step must still answer NONE (got %s)",
		ZM_TrainerSightActionName(eGated));
	ZENITH_ASSERT_STREQ(ZM_TrainerSightStateName((ZM_TRAINER_SIGHT_STATE)9999u), "UNKNOWN",
		"the state namer must answer UNKNOWN for garbage");
	ZENITH_ASSERT_STREQ(ZM_TrainerSightActionName((ZM_TRAINER_SIGHT_ACTION)9999u), "UNKNOWN",
		"the action namer must answer UNKNOWN for garbage");
}

// ---- The re-engagement gate (Q-2026-07-28-001), both arms --------------------

ZENITH_TEST(ZM_Interaction, Gate_FlaggedRowKeysOnItsDefeatFlagAndIgnoresTheLatch)
{
	const ZM_TrainerData& xRow = ZM_GetTrainerData(ZM_TRAINER_RIVAL_VESPER);

	// All four corners of (defeatFlagSet, sessionLatchSet).
	ZENITH_ASSERT_TRUE(ZM_MayTrainerEngage(xRow, false, false),
		"an undefeated flagged trainer may engage");
	ZENITH_ASSERT_TRUE(ZM_MayTrainerEngage(xRow, false, true),
		"the SESSION LATCH must NOT gate a FLAGGED row -- a loss writes no flag, so "
		"the rival stays re-battleable after the player heals up");
	ZENITH_ASSERT_FALSE(ZM_MayTrainerEngage(xRow, true, false),
		"a defeated flagged trainer may NOT engage");
	ZENITH_ASSERT_FALSE(ZM_MayTrainerEngage(xRow, true, true),
		"a defeated flagged trainer may NOT engage, latch or no latch");
}

ZENITH_TEST(ZM_Interaction, Gate_FlaglessRowKeysOnTheSessionLatchAndIgnoresTheFlag)
{
	const ZM_TrainerData& xRow = ZM_GetTrainerData(ZM_TRAINER_ROUTE1_RAMBLER);

	// WHY a flag-only gate could never work for this row, asserted rather than
	// merely commented: ZM_IsStoryFlagSet answers false FOREVER for the sentinel,
	// so a flagless trainer gated on their flag is permanently "not yet defeated"
	// and their prize money is farmable.
	ZENITH_ASSERT_EQ(ZM_IsStoryFlagSet(ZM_StoryFlagSet{}, xRow.m_eDefeatFlag), false,
		"a flagless row's defeat flag can never read as SET -- that is precisely why "
		"this arm keys on the session latch instead");

	// All four corners of (defeatFlagSet, sessionLatchSet).
	ZENITH_ASSERT_TRUE(ZM_MayTrainerEngage(xRow, false, false),
		"an un-battled flagless trainer may engage");
	ZENITH_ASSERT_TRUE(ZM_MayTrainerEngage(xRow, true, false),
		"a flag input is MEANINGLESS for a flagless row and must not gate it");
	ZENITH_ASSERT_FALSE(ZM_MayTrainerEngage(xRow, false, true),
		"a flagless trainer who already forced a battle THIS SESSION may not engage "
		"again -- otherwise the prize money is farmable");
	ZENITH_ASSERT_FALSE(ZM_MayTrainerEngage(xRow, true, true),
		"the latched flagless trainer stays closed with the flag set too");
}

ZENITH_TEST(ZM_Interaction, Gate_UnregisteredRowFailsClosed)
{
	// ZM_GetTrainerData logs a non-fatal Zenith_Error for the sentinel and returns
	// the shared UNKNOWN row -- it does NOT assert, so this is safe at boot. That
	// error line is EXPECTED output of this unit, not a failure.
	const ZM_TrainerData& xUnknown = ZM_GetTrainerData(ZM_TRAINER_NONE);

	ZENITH_ASSERT_FALSE(ZM_MayTrainerEngage(xUnknown, false, false),
		"an UNREGISTERED row must fail CLOSED -- it carries ZM_STORY_FLAG_NONE, so "
		"without the registration guard it would fall into the flagless arm and "
		"answer true");
	ZENITH_ASSERT_FALSE(ZM_MayTrainerEngage(xUnknown, false, true),
		"an unregistered row is closed with the latch set too");
}

// ---- ANTI-VACUITY: the shipped roster must still have one row of each kind ----

ZENITH_TEST(ZM_Interaction, Gate_ShippedRosterStillExercisesBothArms)
{
	const u_int uTrainerCount = ZM_GetTrainerCount();
	ZENITH_ASSERT_GT(uTrainerCount, 0u,
		"an empty roster would make every walk below pass vacuously");

	u_int uFlaggedRows  = 0u;
	u_int uFlaglessRows = 0u;
	for (u_int u = 0u; u < uTrainerCount; ++u)
	{
		const ZM_TrainerData& xRow = ZM_GetTrainerData((ZM_TRAINER_ID)u);
		if ((u_int)xRow.m_eDefeatFlag < (u_int)ZM_STORY_FLAG_COUNT)
		{
			++uFlaggedRows;
		}
		else
		{
			++uFlaglessRows;
		}
	}

	ZENITH_ASSERT_GT(uFlaggedRows, 0u,
		"the roster no longer contains ANY row with a defeat flag, so "
		"Gate_FlaggedRowKeysOnItsDefeatFlagAndIgnoresTheLatch is testing nothing -- "
		"point it at a genuinely flagged row");
	ZENITH_ASSERT_GT(uFlaglessRows, 0u,
		"the roster no longer contains ANY flagless row, so "
		"Gate_FlaglessRowKeysOnTheSessionLatchAndIgnoresTheFlag is testing nothing -- "
		"point it at a genuinely flagless row");

	// The two FIXTURES the arm units actually name, pinned individually: a roster
	// that kept one row of each kind but moved the fixtures would still disarm them.
	ZENITH_ASSERT_EQ((u_int)ZM_GetTrainerData(ZM_TRAINER_RIVAL_VESPER).m_eDefeatFlag,
		(u_int)ZM_STORY_FLAG_RIVAL1_DEFEATED,
		"the FLAGGED fixture must still carry a REGISTERED defeat flag");
	ZENITH_ASSERT_EQ((u_int)ZM_GetTrainerData(ZM_TRAINER_ROUTE1_RAMBLER).m_eDefeatFlag,
		(u_int)ZM_STORY_FLAG_NONE,
		"the FLAGLESS fixture gained a defeat flag -- the two gate-arm units above "
		"now silently exercise the SAME arm; point the flagless one at a row that "
		"really has no flag");
}

// ---- The process-global session latch ----------------------------------------

ZENITH_TEST(ZM_Interaction, Latch_MarkIsPerTrainerAndResetClearsEverything)
{
	// Reset on ENTRY as well as exit: this is ownerless process-global state, so a
	// unit that only tidied up afterwards would still inherit whatever ran first.
	ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
	ZENITH_ASSERT_FALSE(ZM_TrainerEngagementLatch::HasEngaged(ZM_TRAINER_RIVAL_VESPER),
		"a reset latch holds nobody");
	ZENITH_ASSERT_FALSE(ZM_TrainerEngagementLatch::HasEngaged(ZM_TRAINER_ROUTE1_RAMBLER),
		"a reset latch holds nobody");
	ZENITH_ASSERT_EQ(ZM_TrainerEngagementLatch::GetEngagedMaskForTests(), 0u,
		"a reset latch is bit-for-bit empty");

	ZM_TrainerEngagementLatch::MarkEngaged(ZM_TRAINER_ROUTE1_RAMBLER);
	ZENITH_ASSERT_TRUE(ZM_TrainerEngagementLatch::HasEngaged(ZM_TRAINER_ROUTE1_RAMBLER),
		"the marked trainer must read back as engaged");
	ZENITH_ASSERT_FALSE(ZM_TrainerEngagementLatch::HasEngaged(ZM_TRAINER_RIVAL_VESPER),
		"one trainer's engagement must NOT silence another -- the latch is one bit "
		"PER trainer, not a global flag");

	ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
	ZENITH_ASSERT_FALSE(ZM_TrainerEngagementLatch::HasEngaged(ZM_TRAINER_ROUTE1_RAMBLER),
		"Reset must clear the marked trainer");
	ZENITH_ASSERT_FALSE(ZM_TrainerEngagementLatch::HasEngaged(ZM_TRAINER_RIVAL_VESPER),
		"Reset must clear everyone");
	ZENITH_ASSERT_EQ(ZM_TrainerEngagementLatch::GetEngagedMaskForTests(), 0u,
		"Reset must leave the mask bit-for-bit empty, so this unit cannot leak into "
		"the rest of the boot suite");
}

ZENITH_TEST(ZM_Interaction, Latch_UnregisteredIdIsInertAndSilent)
{
	ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();

	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;

		ZM_TrainerEngagementLatch::MarkEngaged(ZM_TRAINER_NONE);
		ZM_TrainerEngagementLatch::MarkEngaged((ZM_TRAINER_ID)9999u);
		const bool bSentinel = ZM_TrainerEngagementLatch::HasEngaged(ZM_TRAINER_NONE);
		const bool bGarbage  = ZM_TrainerEngagementLatch::HasEngaged((ZM_TRAINER_ID)9999u);
		(void)bSentinel;
		(void)bGarbage;

		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"the latch asserted on an unregistered id -- Zenith_Assert breaks in EVERY "
		"config, so this would kill the whole boot unit run rather than fail one test");
	// The RAW MASK, not just HasEngaged: a stray high bit would be invisible to the
	// accessor (which rejects the id before reading) but is still corruption.
	ZENITH_ASSERT_EQ(ZM_TrainerEngagementLatch::GetEngagedMaskForTests(), 0u,
		"an unregistered MarkEngaged must set NO bit at all");
	ZENITH_ASSERT_FALSE(ZM_TrainerEngagementLatch::HasEngaged(ZM_TRAINER_NONE),
		"the sentinel is never engaged");

	ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
}
