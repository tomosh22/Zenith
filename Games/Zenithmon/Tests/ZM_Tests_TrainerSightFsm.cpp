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
#include <cstring>   // strcmp (the name-distinctness walk)
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

	// A default-constructed ZM_TrainerSightFsmTuning already IS the shipped tuning;
	// this exists purely to say so at every call site.
	ZM_TrainerSightFsmTuning MakeShippedTuning()
	{
		return ZM_TrainerSightFsmTuning();
	}

	void EnterSpotted(ZM_TrainerSightFsm& xFsm,
		const ZM_TrainerSightInputs& xInputs,
		const ZM_TrainerSightFsmTuning& xTuning)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"a cold visible trainer must begin with presentation, not an action (got %s)",
			ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_SPOTTED,
			"a cold visible trainer must enter SPOTTED (got %s)",
			ZM_TrainerSightStateName(xFsm.GetState()));
	}

	ZM_TRAINER_SIGHT_ACTION FinishSpotted(ZM_TrainerSightFsm& xFsm,
		const ZM_TrainerSightInputs& xInputs,
		const ZM_TrainerSightFsmTuning& xTuning)
	{
		ZM_TRAINER_SIGHT_ACTION eAction = ZM_TRAINER_SIGHT_ACTION_NONE;
		u_int uSteps = 0u;
		while (xFsm.GetState() == ZM_TRAINER_SIGHT_SPOTTED && uSteps < 120u)
		{
			eAction = xFsm.Step(xInputs, xTuning);
			++uSteps;
		}
		// The STATE is the check, not the step count: a loop bounded at 120 can
		// legitimately spend its 120th step leaving SPOTTED, so asserting uSteps < 120
		// would red a machine that actually worked.
		ZENITH_ASSERT_NE(xFsm.GetState(), ZM_TRAINER_SIGHT_SPOTTED,
			"the shipped spotted beat never completed within %u steps", uSteps);
		return eAction;
	}

	// Drive the one raise every "starts from ENGAGED" unit depends on, and PROVE it
	// happened. A helper that merely stepped would let a broken machine hand those
	// units a silently cold watcher to assert against.
	void RaiseOnce(ZM_TrainerSightFsm& xFsm)
	{
		const ZM_TrainerSightInputs xInputs = MakePassingInputs();
		const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
		EnterSpotted(xFsm, xInputs, xTuning);
		const ZM_TRAINER_SIGHT_ACTION eAction = FinishSpotted(xFsm, xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
			"a cold watcher with every clause satisfied must raise after SPOTTED "
			"(got %s) -- every unit that starts from ENGAGED depends on this",
			ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
			"...and must be ENGAGED afterwards (got %s)",
			ZM_TrainerSightStateName(xFsm.GetState()));
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
			"...and must have counted exactly one raise");
	}

	// The SC7 delta, spelled ONCE: the same passing fixture plus a trainer who
	// actually has something to say (ZM_SelectTrainerChallengeLines yielded a
	// non-zero count). Every challenge unit below is this fixture with at most one
	// further field flipped.
	ZM_TrainerSightInputs MakeChallengeInputs()
	{
		ZM_TrainerSightInputs xInputs = MakePassingInputs();
		xInputs.m_bChallengeAvailable = true;
		return xInputs;
	}

	// Drive the one challenge BEAT every "starts from CHALLENGING" unit depends on,
	// and PROVE it happened -- including that it did NOT raise, which is the whole
	// ordering claim of SC7.
	void ChallengeOnce(ZM_TrainerSightFsm& xFsm)
	{
		const ZM_TrainerSightInputs xInputs = MakeChallengeInputs();
		const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
		EnterSpotted(xFsm, xInputs, xTuning);
		const ZM_TRAINER_SIGHT_ACTION eAction = FinishSpotted(xFsm, xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE,
			"a cold watcher with every clause satisfied AND lines to speak must run the "
			"bark after SPOTTED (got %s)", ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_CHALLENGING,
			"...and must be CHALLENGING afterwards (got %s)",
			ZM_TrainerSightStateName(xFsm.GetState()));
		ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 1u,
			"...and must have counted exactly one challenge beat");
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
			"...and must NOT have raised: the beat PRECEDES the encounter, it never "
			"replaces the ordering");
	}
}

// ---- The rising edge: one raise per continuous spotting -----------------------

ZENITH_TEST(ZM_Interaction, Fsm_ColdWatcherRaisesExactlyOnceOnFirstSighting)
{
	ZM_TrainerSightFsm xFsm;
	const ZM_TrainerSightInputs xInputs = MakePassingInputs();
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	EnterSpotted(xFsm, xInputs, xTuning);
	const ZM_TRAINER_SIGHT_ACTION eFirst = FinishSpotted(xFsm, xInputs, xTuning);
	ZENITH_ASSERT_EQ(eFirst, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"a cold watcher that can see the player must raise after SPOTTED (got %s)",
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
	// start the visual beat and then raise. Without this the unit would also pass on
	// an inert fixture.
	xInputs.m_bMayEngage = true;
	const ZM_TRAINER_SIGHT_ACTION eOpened = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eOpened, ZM_TRAINER_SIGHT_ACTION_NONE,
		"opening the gate on an otherwise-passing fixture must start SPOTTED (got %s) -- "
		"otherwise this unit proved nothing about the gate",
		ZM_TrainerSightActionName(eOpened));
	const ZM_TRAINER_SIGHT_ACTION eAfterSpot = FinishSpotted(xFsm, xInputs, xTuning);
	ZENITH_ASSERT_EQ(eAfterSpot, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the opened gate must raise after its visual beat (got %s)",
		ZM_TrainerSightActionName(eAfterSpot));
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

	// ANTI-VACUITY: clearing the line is the ONLY change, and it must start the beat
	// and then raise.
	xInputs.m_bSightLineClear = true;
	const ZM_TRAINER_SIGHT_ACTION eCleared = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eCleared, ZM_TRAINER_SIGHT_ACTION_NONE,
		"stepping out from behind cover must start SPOTTED (got %s)",
		ZM_TrainerSightActionName(eCleared));
	const ZM_TRAINER_SIGHT_ACTION eAfterSpot = FinishSpotted(xFsm, xInputs, xTuning);
	ZENITH_ASSERT_EQ(eAfterSpot, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the unblocked sighting must raise after SPOTTED (got %s)",
		ZM_TrainerSightActionName(eAfterSpot));
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

	// Walking back in is a NEW spotting and must run a new visual beat before firing.
	const ZM_TRAINER_SIGHT_ACTION eReturned = xFsm.Step(MakePassingInputs(), xTuning);
	ZENITH_ASSERT_EQ(eReturned, ZM_TRAINER_SIGHT_ACTION_NONE,
		"walking back into the cone is a NEW SPOTTED beat (got %s)",
		ZM_TrainerSightActionName(eReturned));
	const ZM_TRAINER_SIGHT_ACTION eAfterSpot =
		FinishSpotted(xFsm, MakePassingInputs(), xTuning);
	ZENITH_ASSERT_EQ(eAfterSpot, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the second spotted beat must raise (got %s)",
		ZM_TrainerSightActionName(eAfterSpot));
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

	// The channel frees up: the deferred sighting is still owed and begins SPOTTED.
	xInputs.m_bChannelBusy = false;
	const ZM_TRAINER_SIGHT_ACTION eFreed = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eFreed, ZM_TRAINER_SIGHT_ACTION_NONE,
		"the deferred sighting must start SPOTTED once the screen is free (got %s) -- deferred, "
		"not consumed", ZM_TrainerSightActionName(eFreed));
	const ZM_TRAINER_SIGHT_ACTION eAfterSpot = FinishSpotted(xFsm, xInputs, xTuning);
	ZENITH_ASSERT_EQ(eAfterSpot, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the freed channel must raise after SPOTTED (got %s)",
		ZM_TrainerSightActionName(eAfterSpot));
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
	ZENITH_ASSERT_EQ(eRetry, ZM_TRAINER_SIGHT_ACTION_NONE,
		"the re-armed watcher must begin a new spotted beat (got %s)",
		ZM_TrainerSightActionName(eRetry));
	const ZM_TRAINER_SIGHT_ACTION eAfterSpot = FinishSpotted(xFsm, xInputs, xTuning);
	ZENITH_ASSERT_EQ(eAfterSpot, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the re-armed watcher must retry after SPOTTED (got %s)",
		ZM_TrainerSightActionName(eAfterSpot));
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

// ---- Known-limit W3: the visible SPOTTED beat -------------------------------

ZENITH_TEST(ZM_Interaction, Fsm_ChallengeTrainerWaitsInSpottedBeforeTheBark)
{
	ZM_TrainerSightFsm xFsm;
	ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
	xTuning.m_fSpottedSeconds = 0.35f;

	ZM_TrainerSightInputs xInputs = MakeChallengeInputs();
	xInputs.m_fDeltaSeconds = 0.1f;
	const ZM_TRAINER_SIGHT_ACTION eSeen = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eSeen, ZM_TRAINER_SIGHT_ACTION_NONE,
		"the first sighting must START the visual beat, not bark or battle (got %s)",
		ZM_TrainerSightActionName(eSeen));
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_SPOTTED,
		"a challenge-capable trainer must enter SPOTTED first (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetSpottedCount(), 1u,
		"one sighting must start exactly one spotted beat");
	ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 0u,
		"the bark may not start on the same tick as the exclamation mark");
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
		"the encounter may not start under the exclamation mark");

	for (u_int u = 1u; u <= 3u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eWaiting = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eWaiting, ZM_TRAINER_SIGHT_ACTION_NONE,
			"the 0.35s spotted window ended after only %u tenths (got %s)",
			u, ZM_TrainerSightActionName(eWaiting));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_SPOTTED,
			"the machine left SPOTTED early after %u tenths (got %s)",
			u, ZM_TrainerSightStateName(xFsm.GetState()));
	}

	const ZM_TRAINER_SIGHT_ACTION eElapsed = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eElapsed, ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE,
		"the bark must begin when the spotted duration elapses (got %s)",
		ZM_TrainerSightActionName(eElapsed));
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_CHALLENGING,
		"the elapsed beat must hand off to CHALLENGING (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 1u,
		"the elapsed spotted beat must start exactly one bark");
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
		"the bark still precedes the encounter after the visual beat");
}

ZENITH_TEST(ZM_Interaction, Fsm_SpottedCancelsOnLostSightOrClosedGateAndRestarts)
{
	for (u_int uArm = 0u; uArm < 2u; ++uArm)
	{
		ZM_TrainerSightFsm xFsm;
		const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
		const ZM_TrainerSightInputs xPassing = MakeChallengeInputs();

		xFsm.Step(xPassing, xTuning);
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_SPOTTED,
			"arm %u never entered the spotted fixture (got %s)", uArm,
			ZM_TrainerSightStateName(xFsm.GetState()));

		ZM_TrainerSightInputs xCancelled = xPassing;
		if (uArm == 0u)
		{
			xCancelled.m_bTargetInSight = false;
		}
		else
		{
			xCancelled.m_bMayEngage = false;
		}
		const ZM_TRAINER_SIGHT_ACTION eCancelled = xFsm.Step(xCancelled, xTuning);
		ZENITH_ASSERT_EQ(eCancelled, ZM_TRAINER_SIGHT_ACTION_NONE,
			"cancelling arm %u emitted an action (got %s)", uArm,
			ZM_TrainerSightActionName(eCancelled));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_WATCHING,
			"cancelling arm %u must re-arm WATCHING (got %s)", uArm,
			ZM_TrainerSightStateName(xFsm.GetState()));
		ZENITH_ASSERT_EQ_FLOAT(xFsm.GetSpottedElapsedSeconds(), 0.0f, 0.0f,
			"cancelling arm %u must clear the partial spotted timer", uArm);
		ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 0u,
			"cancelling arm %u must not bark", uArm);
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
			"cancelling arm %u must not start a battle", uArm);

		xFsm.Step(xPassing, xTuning);
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_SPOTTED,
			"a genuinely new sighting after cancel arm %u must restart SPOTTED (got %s)",
			uArm, ZM_TrainerSightStateName(xFsm.GetState()));
		ZENITH_ASSERT_EQ(xFsm.GetSpottedCount(), 2u,
			"cancel arm %u followed by a new sighting must count two distinct beats", uArm);
	}
}

ZENITH_TEST(ZM_Interaction, Fsm_SpottedBusyPauseAndDegenerateDurationFailOpen)
{
	ZM_TrainerSightFsm xFsm;
	ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
	xTuning.m_fSpottedSeconds = 0.35f;

	ZM_TrainerSightInputs xInputs = MakeChallengeInputs();
	xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_SPOTTED,
		"the fixture must begin SPOTTED (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));

	ZM_TrainerSightInputs xBusy = xInputs;
	xBusy.m_bChannelBusy = true;
	xBusy.m_fDeltaSeconds = 10.0f;
	for (u_int u = 0u; u < 3u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eBusy = xFsm.Step(xBusy, xTuning);
		ZENITH_ASSERT_EQ(eBusy, ZM_TRAINER_SIGHT_ACTION_NONE,
			"busy spotted step %u emitted an action (got %s)", u,
			ZM_TrainerSightActionName(eBusy));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_SPOTTED,
			"a busy channel must PAUSE, not consume, SPOTTED (step %u, got %s)",
			u, ZM_TrainerSightStateName(xFsm.GetState()));
	}
	ZENITH_ASSERT_EQ_FLOAT(xFsm.GetSpottedElapsedSeconds(), 0.0f, 0.0f,
		"thirty busy seconds must contribute exactly zero spotted time");

	// A corrupt/non-positive duration must not strand the battle behind a permanent
	// indicator. It fails open into the already-authored bark on the first free tick.
	xTuning.m_fSpottedSeconds = 0.0f;
	const ZM_TRAINER_SIGHT_ACTION eFreed = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eFreed, ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE,
		"a degenerate spotted duration must fail open to the bark (got %s)",
		ZM_TrainerSightActionName(eFreed));
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_CHALLENGING,
		"the fail-open handoff must be CHALLENGING (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
}

// ---- S7 item 3 SC7: the challenge beat ---------------------------------------

ZENITH_TEST(ZM_Interaction, Fsm_AvailableChallengeRunsTheBeatInsteadOfRaising)
{
	ZM_TrainerSightFsm xFsm;
	const ZM_TrainerSightInputs xInputs = MakeChallengeInputs();
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
	EnterSpotted(xFsm, xInputs, xTuning);
	const ZM_TRAINER_SIGHT_ACTION eAction = FinishSpotted(xFsm, xInputs, xTuning);

	ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE,
		"a trainer with lines must RUN THE BARK after SPOTTED, not raise (got %s)",
		ZM_TrainerSightActionName(eAction));
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_CHALLENGING,
		"the beat must move the machine to CHALLENGING (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 1u,
		"exactly one challenge beat has been started");
	// THE LOAD-BEARING CLAUSE: the bark PRECEDES the encounter. A machine that
	// raised here would still look healthy by state alone.
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
		"the beat must PRECEDE the encounter -- nothing may have been raised yet");
}

ZENITH_TEST(ZM_Interaction, Fsm_AcceptedChallengeRaisesOnceWhenTheChannelClears)
{
	ZM_TrainerSightFsm xFsm;
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	ChallengeOnce(xFsm);

	// The bark LANDED: pushing a ZM_UI_MenuStack dialogue is exactly what makes
	// IsMenuOpen() -- and therefore this input -- true.
	ZM_TrainerSightInputs xBusy = MakeChallengeInputs();
	xBusy.m_bChannelBusy = true;
	const ZM_TRAINER_SIGHT_ACTION eHeard = xFsm.Step(xBusy, xTuning);
	ZENITH_ASSERT_EQ(eHeard, ZM_TRAINER_SIGHT_ACTION_NONE,
		"the busy frame is an acknowledgement, not an action (got %s)",
		ZM_TrainerSightActionName(eHeard));
	ZENITH_ASSERT_TRUE(xFsm.IsChallengeAccepted(),
		"a channel that went busy after our beat CONFIRMS the bark landed");

	// A LONG conversation. 38 more busy frames is far past the 0.5s challenge
	// window, and none of them may time out into an early raise: the window
	// measures "did anyone speak", never "how long they spoke for".
	for (u_int u = 3u; u <= 40u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xBusy, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"step %u of an open dialogue produced an action (got %s)",
			u, ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
			"a LONG bark must never time out into an early raise (step %u)", u);
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_CHALLENGING,
			"the machine must stay CHALLENGING while the box is open (step %u, got %s)",
			u, ZM_TrainerSightStateName(xFsm.GetState()));
	}

	// The box CLOSES. ORDER 112 (MenuStack) < 113 (Interactable) means the menu
	// freeze was already released earlier in this same frame.
	const ZM_TRAINER_SIGHT_ACTION eClosed = xFsm.Step(MakeChallengeInputs(), xTuning);
	ZENITH_ASSERT_EQ(eClosed, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the encounter must be raised once the conversation ends (got %s)",
		ZM_TrainerSightActionName(eClosed));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"the closed conversation must produce exactly one raise");
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"...and must leave the machine ENGAGED (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));

	const ZM_TRAINER_SIGHT_ACTION eAfter = xFsm.Step(MakeChallengeInputs(), xTuning);
	ZENITH_ASSERT_EQ(eAfter, ZM_TRAINER_SIGHT_ACTION_NONE,
		"the raise is a rising edge, not a per-frame event (got %s)",
		ZM_TrainerSightActionName(eAfter));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"one conversation is still exactly one raise");
}

// THE _False-BUILD / MISSING-.bgraph CONTRACT: nothing ever spoke, and the battle
// still happens.
ZENITH_TEST(ZM_Interaction, Fsm_DroppedChallengeRaisesAfterTheChallengeWindow)
{
	ZM_TrainerSightFsm xFsm;

	ZM_TrainerSightFsmTuning xTuning;
	xTuning.m_fChallengeConfirmSeconds = 0.5f;

	// The beat is asked for on the shipped tuning (that is what ChallengeOnce
	// pins), then the window is driven at a coarse 0.1s so the arithmetic is
	// readable as "five tenths of a half-second window".
	ChallengeOnce(xFsm);

	ZM_TrainerSightInputs xInputs = MakeChallengeInputs();
	xInputs.m_bChannelBusy  = false;   // NOTHING EVER SPOKE
	xInputs.m_fDeltaSeconds = 0.1f;

	for (u_int u = 1u; u <= 4u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"the challenge window must be TIMED, not immediate -- %u tenths into a "
			"half-second window nothing may have happened (got %s)",
			u, ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
			"nothing may have raised %u tenths into the challenge window", u);
		ZENITH_ASSERT_FALSE(xFsm.IsChallengeAccepted(),
			"a bark nobody ever heard is NOT accepted (step %u)", u);
		// A poisoned accumulator would compare false against the window and leave
		// the STATE looking perfectly healthy.
		ZENITH_ASSERT_TRUE(std::isfinite(xFsm.GetChallengeElapsedSeconds()),
			"the challenge accumulator went non-finite on step %u", u);
	}

	const ZM_TRAINER_SIGHT_ACTION eCrossed = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eCrossed, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"a graph that never spoke must still deliver the BATTLE once the window "
		"elapses (got %s) -- this is the whole missing-.bgraph / _False-build "
		"contract", ZM_TrainerSightActionName(eCrossed));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"the dropped bark must cost the bark and nothing else");
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"the fail-open raise must leave the machine ENGAGED (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_TRUE(std::isfinite(xFsm.GetChallengeElapsedSeconds()),
		"the challenge accumulator must stay finite across the whole beat");
}

// THE TWO POLARITIES, PINNED SIDE BY SIDE so neither can be "tidied" into the
// other. A shared helper for both windows would red clause (a) or clause (b).
ZENITH_TEST(ZM_Interaction, Fsm_DegenerateChallengeWindowFailsOpenWhileTheRaiseWindowFailsClosed)
{
	const float fNaN = std::numeric_limits<float>::quiet_NaN();

	// ---- (a) a degenerate CHALLENGE window FAILS OPEN: raise IMMEDIATELY -------
	constexpr u_int uDEGENERATE_COUNT = 3u;
	const float afDEGENERATE[uDEGENERATE_COUNT] = { 0.0f, fNaN, -1.0f };

	for (u_int u = 0u; u < uDEGENERATE_COUNT; ++u)
	{
		ZM_TrainerSightFsm xFsm;
		ChallengeOnce(xFsm);

		ZM_TrainerSightFsmTuning xTuning;
		xTuning.m_fRaiseConfirmSeconds     = 0.5f;
		xTuning.m_fChallengeConfirmSeconds = afDEGENERATE[u];

		ZM_TrainerSightInputs xInputs = MakeChallengeInputs();
		xInputs.m_bChannelBusy = false;

		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
			"degenerate challenge window %u must FAIL OPEN and raise IMMEDIATELY "
			"(got %s) -- silence here would cost the battle altogether",
			u, ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
			"degenerate challenge window %u must leave the machine ENGAGED (got %s)",
			u, ZM_TrainerSightStateName(xFsm.GetState()));
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
			"degenerate challenge window %u must have raised exactly once", u);
	}

	// ---- (b) a degenerate RAISE window FAILS CLOSED: never re-arm --------------
	// THE SAME UNIT, deliberately: copying the ENGAGED arm's guard shape onto the
	// challenge raise reds (a) while leaving (b) green, and copying the challenge
	// shape onto the ENGAGED arm reds (b) while leaving (a) green. Only having
	// both here makes either mutation visible.
	ZM_TrainerSightFsm xSilent;
	ZM_TrainerSightFsmTuning xClosedTuning;
	xClosedTuning.m_fRaiseConfirmSeconds = fNaN;

	const ZM_TrainerSightInputs xSilentInputs = MakePassingInputs();   // NO lines
	EnterSpotted(xSilent, xSilentInputs, xClosedTuning);
	const ZM_TRAINER_SIGHT_ACTION eSeed =
		FinishSpotted(xSilent, xSilentInputs, xClosedTuning);
	ZENITH_ASSERT_EQ(eSeed, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the silent fixture must raise after SPOTTED or clause (b) proves nothing "
		"(got %s)", ZM_TrainerSightActionName(eSeed));
	ZENITH_ASSERT_FALSE(xSilent.IsRaiseConfirmed(),
		"the drop timer must be running for clause (b) to mean anything");

	for (u_int u = 0u; u < 600u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xSilent.Step(xSilentInputs, xClosedTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"a degenerate RAISE window must FAIL CLOSED -- step %u re-raised (got %s); "
			"silence beats a spurious battle on THIS side",
			u, ZM_TrainerSightActionName(eAction));
	}
	ZENITH_ASSERT_EQ(xSilent.GetRaiseCount(), 1u,
		"ten seconds under a degenerate raise window must still be ONE raise");
	ZENITH_ASSERT_EQ(xSilent.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"a fail-closed machine stays ENGAGED rather than re-arming (got %s)",
		ZM_TrainerSightStateName(xSilent.GetState()));
}

ZENITH_TEST(ZM_Interaction, Fsm_LeavingSightDuringChallengeAbandonsWithoutRaisingOrLatching)
{
	ZM_TrainerSightFsm xFsm;
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	ChallengeOnce(xFsm);

	// The bark never landed (the channel stayed idle) and the player walked out.
	// A bark that HAD landed would have frozen him, so this arm is reachable only
	// in the dropped case.
	ZM_TrainerSightInputs xLeft = MakeChallengeInputs();
	xLeft.m_bTargetInSight = false;
	const ZM_TRAINER_SIGHT_ACTION eLeft = xFsm.Step(xLeft, xTuning);
	ZENITH_ASSERT_EQ(eLeft, ZM_TRAINER_SIGHT_ACTION_NONE,
		"abandoning a beat is not itself an action (got %s)",
		ZM_TrainerSightActionName(eLeft));
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_WATCHING,
		"abandoning must re-arm the WATCHER, not fall through to ENGAGED (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
		"an abandoned beat must NOT raise -- the caller therefore never reaches "
		"MarkEngaged and a flagless trainer keeps his one session shot");

	// Walking back in re-arms SPOTTED and then the BARK, not the battle.
	const ZM_TRAINER_SIGHT_ACTION eReturned = xFsm.Step(MakeChallengeInputs(), xTuning);
	ZENITH_ASSERT_EQ(eReturned, ZM_TRAINER_SIGHT_ACTION_NONE,
		"walking back into the cone must restart SPOTTED (got %s)",
		ZM_TrainerSightActionName(eReturned));
	const ZM_TRAINER_SIGHT_ACTION eRebark =
		FinishSpotted(xFsm, MakeChallengeInputs(), xTuning);
	ZENITH_ASSERT_EQ(eRebark, ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE,
		"walking back into the cone must re-run the BARK after SPOTTED (got %s)",
		ZM_TrainerSightActionName(eRebark));
	ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 2u,
		"the re-bark is the second challenge beat");
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
		"and still nothing has been raised");
}

// A silent trainer still receives the visible W3 cue, then skips CHALLENGING.
ZENITH_TEST(ZM_Interaction, Fsm_SilentTrainerShowsSpottedThenRaisesWithoutChallenge)
{
	ZM_TrainerSightFsm xFsm;
	const ZM_TrainerSightInputs xInputs = MakePassingInputs();   // m_bChallengeAvailable == false
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	ZENITH_ASSERT_FALSE(xInputs.m_bChallengeAvailable,
		"the shared passing fixture must DEFAULT to 'no lines' so a silent trainer "
		"skips CHALLENGING after the shared visual beat");

	EnterSpotted(xFsm, xInputs, xTuning);
	const ZM_TRAINER_SIGHT_ACTION eFirst = FinishSpotted(xFsm, xInputs, xTuning);
	ZENITH_ASSERT_EQ(eFirst, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"a SILENT trainer must raise after the visible cue (got %s)",
		ZM_TrainerSightActionName(eFirst));
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"a silent trainer must skip CHALLENGING after SPOTTED (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"the silent sighting is exactly one raise");
	ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 0u,
		"the beat was never entered, so no challenge window ran");

	for (u_int u = 0u; u < 30u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xFsm.Step(xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"step %u of ONE continuous silent spotting acted again (got %s)",
			u, ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
			"the raise count moved on step %u of one continuous silent spotting", u);
		ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 0u,
			"the challenge count moved for a trainer with nothing to say (step %u)", u);
	}
}

// ---- Reset ------------------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Fsm_ResetReturnsAColdWatcher)
{
	ZM_TrainerSightFsm xFsm;

	const ZM_TrainerSightInputs xInputs = MakePassingInputs();
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	// ---- S7 item 3 SC7: dirty the CHALLENGE members FIRST ----------------------
	// Reset now clears NINE observable members. A fixture that only dirtied the four
	// SC6 ones would keep passing after the five appended fields stopped being
	// cleared, so the visual and bark beats are driven BEFORE the raise -- which is
	// also the production order.
	ChallengeOnce(xFsm);
	ZENITH_ASSERT_EQ(xFsm.GetSpottedCount(), 1u,
		"the fixture must carry a non-zero spotted count before Reset");
	ZENITH_ASSERT_GT(xFsm.GetSpottedElapsedSeconds(), 0.0f,
		"the fixture must carry a populated spotted accumulator before Reset");
	for (u_int u = 0u; u < 2u; ++u)
	{
		xFsm.Step(MakeChallengeInputs(), xTuning);
	}
	ZENITH_ASSERT_GT(xFsm.GetChallengeElapsedSeconds(), 0.0f,
		"the fixture must have a POPULATED challenge accumulator before Reset, or the "
		"new clauses below assert against an already-cold field");
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_CHALLENGING,
		"two frames of a 0.5s challenge window must not have ended the beat (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));

	ZM_TrainerSightInputs xBarkHeard = MakeChallengeInputs();
	xBarkHeard.m_bChannelBusy = true;
	xFsm.Step(xBarkHeard, xTuning);
	ZENITH_ASSERT_TRUE(xFsm.IsChallengeAccepted(),
		"the fixture must be ACCEPTED before Reset for the same reason");

	// The box closes and the withheld encounter is raised. The challenge members
	// stay dirty behind it -- only Reset and a genuinely NEW beat clear them.
	const ZM_TRAINER_SIGHT_ACTION eRaise = xFsm.Step(xInputs, xTuning);
	ZENITH_ASSERT_EQ(eRaise, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"the closed conversation must raise, or the SC6 half of this fixture never "
		"reaches ENGAGED (got %s)", ZM_TrainerSightActionName(eRaise));
	ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 1u,
		"the fixture must carry a non-zero challenge count before Reset");

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

	// ---- S7 item 3 SC7: the three appended members ----------------------------
	ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 0u,
		"Reset must clear the challenge count");
	ZENITH_ASSERT_FALSE(xFsm.IsChallengeAccepted(),
		"Reset must clear the challenge acceptance latch");
	ZENITH_ASSERT_EQ_FLOAT(xFsm.GetChallengeElapsedSeconds(), 0.0f, 0.0f,
		"Reset must clear the challenge accumulator");

	// ---- Known-limit W3: the two appended presentation members ----------------
	ZENITH_ASSERT_EQ(xFsm.GetSpottedCount(), 0u,
		"Reset must clear the spotted count");
	ZENITH_ASSERT_EQ_FLOAT(xFsm.GetSpottedElapsedSeconds(), 0.0f, 0.0f,
		"Reset must clear the spotted accumulator");
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
	constexpr u_int uWINDOW_COUNT = 6u;
	const float afDELTAS[uDELTA_COUNT]   = { fNaN, fInf, -fInf, -1.0f, 0.0f, fFRAME_DT };
	// One degenerate-value inventory feeds all THREE windows, swept independently.
	// The challenge and raise polarities are deliberately opposite, while W3's
	// presentation window adds a second fail-open route.
	const float afWINDOWS[uWINDOW_COUNT] = { fNaN, fInf, -fInf, -1.0f, 0.0f, 0.5f };
	constexpr u_int uWINDOW_TRIPLES =
		uWINDOW_COUNT * uWINDOW_COUNT * uWINDOW_COUNT;

	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;

		// The full cross product: 32 bool combinations x 6 deltas x 216 independent
		// window TRIPLES, each driven from ALL FOUR arms of the machine.
		for (u_int uBits = 0u; uBits < 32u; ++uBits)
		{
			for (u_int uDelta = 0u; uDelta < uDELTA_COUNT; ++uDelta)
			{
				// ONE flat loop over the triple, so all three windows move
				// independently. A sweep that moved them together could never reach
				// the mixed corners -- including the opposite challenge/raise fail
				// polarities and the W3 presentation fail-open arm.
				for (u_int uTriple = 0u; uTriple < uWINDOW_TRIPLES; ++uTriple)
				{
					ZM_TrainerSightInputs xInputs;
					xInputs.m_bMayEngage          = (uBits & 1u) != 0u;
					xInputs.m_bTargetInSight      = (uBits & 2u) != 0u;
					xInputs.m_bSightLineClear     = (uBits & 4u) != 0u;
					xInputs.m_bChannelBusy        = (uBits & 8u) != 0u;
					xInputs.m_bChallengeAvailable = (uBits & 16u) != 0u;
					xInputs.m_fDeltaSeconds       = afDELTAS[uDelta];

					ZM_TrainerSightFsmTuning xTuning;
					xTuning.m_fRaiseConfirmSeconds =
						afWINDOWS[uTriple / (uWINDOW_COUNT * uWINDOW_COUNT)];
					xTuning.m_fChallengeConfirmSeconds =
						afWINDOWS[(uTriple / uWINDOW_COUNT) % uWINDOW_COUNT];
					xTuning.m_fSpottedSeconds = afWINDOWS[uTriple % uWINDOW_COUNT];

					ZM_TrainerSightFsmTuning xImmediateTuning = MakeShippedTuning();
					xImmediateTuning.m_fSpottedSeconds = 0.0f;

					// From a COLD watcher...
					ZM_TrainerSightFsm xCold;
					const ZM_TRAINER_SIGHT_ACTION eCold = xCold.Step(xInputs, xTuning);

					// ...from an ENGAGED one...
					ZM_TrainerSightFsm xEngaged;
					const ZM_TRAINER_SIGHT_ACTION eSeed =
						xEngaged.Step(MakePassingInputs(), xImmediateTuning);
					const ZM_TRAINER_SIGHT_ACTION eEngaged = xEngaged.Step(xInputs, xTuning);

					// ...and from a CHALLENGING one, so the SC7 arm is swept too.
					ZM_TrainerSightFsm xChallenging;
					const ZM_TRAINER_SIGHT_ACTION eChallengeSeed =
						xChallenging.Step(MakeChallengeInputs(), xImmediateTuning);
					const ZM_TRAINER_SIGHT_ACTION eChallenging =
						xChallenging.Step(xInputs, xTuning);

					// ...and from a genuine SPOTTED one, so W3's new arm is not
					// accidentally represented by another cold watcher.
					ZM_TrainerSightFsm xSpotted;
					const ZM_TRAINER_SIGHT_ACTION eSpottedSeed =
						xSpotted.Step(MakePassingInputs(), MakeShippedTuning());
					const ZM_TRAINER_SIGHT_ACTION eSpotted = xSpotted.Step(xInputs, xTuning);

					(void)eCold; (void)eSeed; (void)eEngaged;
					(void)eChallengeSeed; (void)eChallenging;
					(void)eSpottedSeed; (void)eSpotted;
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
	ZENITH_ASSERT_EQ(eFresh, ZM_TRAINER_SIGHT_ACTION_NONE,
		"a fully-passing cold Step must start presentation without acting (got %s)",
		ZM_TrainerSightActionName(eFresh));
	ZENITH_ASSERT_EQ(xFresh.GetState(), ZM_TRAINER_SIGHT_SPOTTED,
		"a fully-passing cold Step must still enter SPOTTED (got %s)",
		ZM_TrainerSightStateName(xFresh.GetState()));
	const ZM_TRAINER_SIGHT_ACTION eFreshFinished =
		FinishSpotted(xFresh, MakePassingInputs(), MakeShippedTuning());
	ZENITH_ASSERT_EQ(eFreshFinished, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"a fully-passing silent trainer must still raise after SPOTTED (got %s)",
		ZM_TrainerSightActionName(eFreshFinished));

	ZM_TrainerSightInputs xGated = MakePassingInputs();
	xGated.m_bMayEngage = false;
	ZM_TrainerSightFsm xGatedFsm;
	const ZM_TRAINER_SIGHT_ACTION eGated = xGatedFsm.Step(xGated, MakeShippedTuning());
	ZENITH_ASSERT_EQ(eGated, ZM_TRAINER_SIGHT_ACTION_NONE,
		"a gated Step must still answer NONE (got %s)",
		ZM_TrainerSightActionName(eGated));
	ZM_TrainerSightFsm xChallengeFsm;
	const ZM_TRAINER_SIGHT_ACTION eChallenge =
		xChallengeFsm.Step(MakeChallengeInputs(), MakeShippedTuning());
	ZENITH_ASSERT_EQ(eChallenge, ZM_TRAINER_SIGHT_ACTION_NONE,
		"a fully-passing cold Step for a trainer WITH lines must start SPOTTED "
		"(got %s)", ZM_TrainerSightActionName(eChallenge));
	const ZM_TRAINER_SIGHT_ACTION eChallengeFinished =
		FinishSpotted(xChallengeFsm, MakeChallengeInputs(), MakeShippedTuning());
	ZENITH_ASSERT_EQ(eChallengeFinished, ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE,
		"a challenge-capable trainer must still bark after SPOTTED (got %s)",
		ZM_TrainerSightActionName(eChallengeFinished));

	ZENITH_ASSERT_STREQ(ZM_TrainerSightStateName((ZM_TRAINER_SIGHT_STATE)9999u), "UNKNOWN",
		"the state namer must answer UNKNOWN for garbage");
	ZENITH_ASSERT_STREQ(ZM_TrainerSightActionName((ZM_TRAINER_SIGHT_ACTION)9999u), "UNKNOWN",
		"the action namer must answer UNKNOWN for garbage");
}

// ---- The name functions stay TOTAL and mutually DISTINCT ---------------------

// ANTI-VACUITY SHAPE: the out-of-range answer is hoisted into a local FIRST and
// every in-range name is then required to DIFFER from it. Without that hoist a
// namer that returned "UNKNOWN" for absolutely everything would sail through a
// naive non-null walk.
ZENITH_TEST(ZM_Interaction, Fsm_StateAndActionNamesStayTotalAndDistinct)
{
	// ---- states ---------------------------------------------------------------
	const char* szStateSentinel =
		ZM_TrainerSightStateName((ZM_TRAINER_SIGHT_STATE)((u_int)ZM_TRAINER_SIGHT_STATE_COUNT));
	ZENITH_ASSERT_NOT_NULL(szStateSentinel,
		"the state namer must answer SOMETHING one past the last enumerator -- every "
		"caller is a log-format argument");
	ZENITH_ASSERT_TRUE(szStateSentinel != nullptr && szStateSentinel[0] != '\0',
		"the out-of-range state answer must be non-empty");
	const char* szStatePastEnd =
		ZM_TrainerSightStateName((ZM_TRAINER_SIGHT_STATE)((u_int)ZM_TRAINER_SIGHT_STATE_COUNT + 1u));
	ZENITH_ASSERT_NOT_NULL(szStatePastEnd,
		"the state namer must answer for values beyond the sentinel too");

	const char* aszStateNames[(u_int)ZM_TRAINER_SIGHT_STATE_COUNT] = {};
	for (u_int u = 0u; u < (u_int)ZM_TRAINER_SIGHT_STATE_COUNT; ++u)
	{
		const char* szName = ZM_TrainerSightStateName((ZM_TRAINER_SIGHT_STATE)u);
		aszStateNames[u] = szName;
		ZENITH_ASSERT_NOT_NULL(szName, "state %u has no name", u);
		ZENITH_ASSERT_TRUE(szName != nullptr && szName[0] != '\0',
			"state %u has an empty name", u);
		// The assert macros RECORD AND CONTINUE, so nothing reaches strcmp until both
		// operands have been proven non-null.
		if (szName == nullptr || szName[0] == '\0'
			|| szStateSentinel == nullptr || szStateSentinel[0] == '\0')
		{
			continue;
		}
		ZENITH_ASSERT_NE(strcmp(szName, szStateSentinel), 0,
			"state %u answers '%s', the same as the OUT-OF-RANGE sentinel -- a namer "
			"that says UNKNOWN for everything is not a namer", u, szName);
	}

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_SIGHT_STATE_COUNT; ++i)
	{
		if (aszStateNames[i] == nullptr) { continue; }
		for (u_int j = i + 1u; j < (u_int)ZM_TRAINER_SIGHT_STATE_COUNT; ++j)
		{
			if (aszStateNames[j] == nullptr) { continue; }
			ZENITH_ASSERT_NE(strcmp(aszStateNames[i], aszStateNames[j]), 0,
				"states %u and %u share the name '%s' -- a shared name makes every "
				"failure message that prints it ambiguous", i, j, aszStateNames[i]);
		}
	}

	// ---- actions --------------------------------------------------------------
	const char* szActionSentinel =
		ZM_TrainerSightActionName((ZM_TRAINER_SIGHT_ACTION)((u_int)ZM_TRAINER_SIGHT_ACTION_COUNT));
	ZENITH_ASSERT_NOT_NULL(szActionSentinel,
		"the action namer must answer SOMETHING one past the last enumerator");
	ZENITH_ASSERT_TRUE(szActionSentinel != nullptr && szActionSentinel[0] != '\0',
		"the out-of-range action answer must be non-empty");
	const char* szActionPastEnd =
		ZM_TrainerSightActionName((ZM_TRAINER_SIGHT_ACTION)((u_int)ZM_TRAINER_SIGHT_ACTION_COUNT + 1u));
	ZENITH_ASSERT_NOT_NULL(szActionPastEnd,
		"the action namer must answer for values beyond the sentinel too");

	const char* aszActionNames[(u_int)ZM_TRAINER_SIGHT_ACTION_COUNT] = {};
	for (u_int u = 0u; u < (u_int)ZM_TRAINER_SIGHT_ACTION_COUNT; ++u)
	{
		const char* szName = ZM_TrainerSightActionName((ZM_TRAINER_SIGHT_ACTION)u);
		aszActionNames[u] = szName;
		ZENITH_ASSERT_NOT_NULL(szName, "action %u has no name", u);
		ZENITH_ASSERT_TRUE(szName != nullptr && szName[0] != '\0',
			"action %u has an empty name", u);
		if (szName == nullptr || szName[0] == '\0'
			|| szActionSentinel == nullptr || szActionSentinel[0] == '\0')
		{
			continue;
		}
		ZENITH_ASSERT_NE(strcmp(szName, szActionSentinel), 0,
			"action %u answers '%s', the same as the OUT-OF-RANGE sentinel", u, szName);
	}

	for (u_int i = 0u; i < (u_int)ZM_TRAINER_SIGHT_ACTION_COUNT; ++i)
	{
		if (aszActionNames[i] == nullptr) { continue; }
		for (u_int j = i + 1u; j < (u_int)ZM_TRAINER_SIGHT_ACTION_COUNT; ++j)
		{
			if (aszActionNames[j] == nullptr) { continue; }
			ZENITH_ASSERT_NE(strcmp(aszActionNames[i], aszActionNames[j]), 0,
				"actions %u and %u share the name '%s'", i, j, aszActionNames[i]);
		}
	}
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
