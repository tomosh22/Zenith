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

	// The SC1 delta, spelled ONCE: the same passing fixture plus a trainer who can
	// physically WALK (the component owns a dynamic capsule on an active
	// simulation). Every approach unit below is this fixture with at most one
	// further field flipped.
	ZM_TrainerSightInputs MakeApproachInputs()
	{
		ZM_TrainerSightInputs xInputs = MakePassingInputs();
		xInputs.m_bApproachPossible = true;
		return xInputs;
	}

	// Drive the one approach BEAT every "starts from APPROACHING" unit depends on,
	// and PROVE it happened -- including that it neither barked nor raised, which is
	// the whole ordering claim of SC1. A helper that merely stepped would hand those
	// units a silently cold watcher to assert against.
	void ApproachOnce(ZM_TrainerSightFsm& xFsm,
		const ZM_TrainerSightInputs& xInputs,
		const ZM_TrainerSightFsmTuning& xTuning)
	{
		EnterSpotted(xFsm, xInputs, xTuning);
		const ZM_TRAINER_SIGHT_ACTION eAction = FinishSpotted(xFsm, xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"the spotted beat of a trainer who can WALK must hand off to the walk, not "
			"to an action (got %s)", ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_APPROACHING,
			"...and must leave the machine APPROACHING (got %s)",
			ZM_TrainerSightStateName(xFsm.GetState()));
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
			"...and must NOT have raised: the walk PRECEDES the encounter");
		ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 0u,
			"...and must NOT have barked either -- the walk precedes the bark too");
	}

	// SC1's fail-OPEN contract for ZM_StepTrainerApproach, spelled once. Zero speed
	// AND arrived: "I cannot walk" must be indistinguishable from "I have walked far
	// enough", because the only thing the caller does with m_bArrived is stop
	// waiting. Answering "not arrived, zero speed" would park a trainer who can
	// never move in APPROACHING until the timeout burned itself out.
	void AssertApproachFailedOpen(const ZM_TrainerApproachStep& xStep, const char* szContext)
	{
		ZENITH_ASSERT_TRUE(xStep.m_bArrived,
			"%s: a degenerate approach must report ARRIVED so the caller fails OPEN",
			szContext);
		ZENITH_ASSERT_EQ_FLOAT(xStep.m_fSpeed, 0.0f, 0.0f,
			"%s: a degenerate approach must request EXACTLY zero speed", szContext);
		ZENITH_ASSERT_NEAR_VEC3(xStep.m_xDirXZ, Zenith_Maths::Vector3(0.0f), 0.0f,
			"%s: a degenerate approach must request an exactly zero direction",
			szContext);
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

// ---- S7 item 1 SC1: the APPROACHING walk-up ---------------------------------
//
// SC1 lands the state and the maths with NO runtime caller, so every unit below
// drives the FSM directly. The compatibility claim -- that m_bApproachPossible
// defaults false and therefore leaves every shipped path byte for byte as it was
// -- is not asserted by a comment: it is the reason the twenty-six units ABOVE
// still pass untouched, and the (a) half of the first unit re-states it.

ZENITH_TEST(ZM_Interaction, Fsm_SpottedRoutesToApproachOnlyWhenPossible)
{
	const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();

	// ---- (a) THE NEGATIVE: no body to walk with -> the SHIPPED exit, unchanged --
	// Swept over BOTH values of m_bChallengeAvailable, because the shipped exit has
	// two destinations and a routing bug that only ever caught the silent one would
	// otherwise hide behind the bark.
	for (u_int uLines = 0u; uLines < 2u; ++uLines)
	{
		ZM_TrainerSightFsm xFsm;
		ZM_TrainerSightInputs xInputs = MakePassingInputs();
		xInputs.m_bChallengeAvailable = (uLines != 0u);
		ZENITH_ASSERT_FALSE(xInputs.m_bApproachPossible,
			"the shared passing fixture must DEFAULT to 'cannot walk' -- that default "
			"is the entire reason the twenty-six units above needed no edit");

		EnterSpotted(xFsm, xInputs, xTuning);
		const ZM_TRAINER_SIGHT_ACTION eAction = FinishSpotted(xFsm, xInputs, xTuning);
		const ZM_TRAINER_SIGHT_ACTION eExpected = (uLines != 0u)
			? ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE
			: ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER;
		ZENITH_ASSERT_EQ(eAction, eExpected,
			"lines=%u: a trainer who cannot walk must take the pre-SC1 spotted exit "
			"(expected %s, got %s)", uLines,
			ZM_TrainerSightActionName(eExpected), ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_NE(xFsm.GetState(), ZM_TRAINER_SIGHT_APPROACHING,
			"lines=%u: a trainer who cannot walk must NEVER enter APPROACHING", uLines);
		ZENITH_ASSERT_EQ(xFsm.GetApproachCount(), 0u,
			"lines=%u: ...and must never book a walk", uLines);
	}

	// ---- (b) THE PAIRED POSITIVE: the SAME fixture plus a body -> APPROACHING ---
	// Without this half the negative above would be satisfied just as well by a
	// state nothing can ever reach.
	for (u_int uLines = 0u; uLines < 2u; ++uLines)
	{
		ZM_TrainerSightFsm xFsm;
		ZM_TrainerSightInputs xInputs = MakeApproachInputs();
		xInputs.m_bChallengeAvailable = (uLines != 0u);

		EnterSpotted(xFsm, xInputs, xTuning);
		const ZM_TRAINER_SIGHT_ACTION eAction = FinishSpotted(xFsm, xInputs, xTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"lines=%u: starting the walk is not itself an action (got %s)",
			uLines, ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_APPROACHING,
			"lines=%u: a trainer who CAN walk must leave SPOTTED into APPROACHING "
			"(got %s)", uLines, ZM_TrainerSightStateName(xFsm.GetState()));
		ZENITH_ASSERT_EQ(xFsm.GetApproachCount(), 1u,
			"lines=%u: one sighting must book exactly one walk", uLines);
		// ★ GRAPH INDEPENDENCE. The challenge .bgraph is gitignored, so a _False or
		// CI build has NO lines at all; if the walk were routed through
		// m_bChallengeAvailable the whole approach would vanish in exactly the builds
		// the gate runs. lines=0 reaching APPROACHING here is that proof.
		ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 0u,
			"lines=%u: the walk PRECEDES the bark and must not have started one", uLines);
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
			"lines=%u: the walk PRECEDES the encounter and must not have raised", uLines);
	}
}

ZENITH_TEST(ZM_Interaction, Fsm_ApproachCancelsOnLostSightAndOnClosedGate)
{
	// TWO ARMS, ONE FLIPPED FIELD EACH, so exactly ONE of the disjunction's operands
	// can be responsible for each cancel. Flipping both together would leave a
	// conjunction (the M2 mutation) perfectly green.
	for (u_int uArm = 0u; uArm < 2u; ++uArm)
	{
		ZM_TrainerSightFsm xFsm;
		const ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
		const ZM_TrainerSightInputs xWalking = MakeApproachInputs();

		ApproachOnce(xFsm, xWalking, xTuning);
		ZENITH_ASSERT_EQ(xFsm.GetApproachCount(), 1u,
			"arm %u must start from exactly one booked walk", uArm);

		// Bank a PARTIAL walk (six 60Hz frames of a 2.0s window), so "clears the
		// timer" below is asserting against a populated field rather than a cold one.
		for (u_int u = 0u; u < 6u; ++u)
		{
			const ZM_TRAINER_SIGHT_ACTION eWalking = xFsm.Step(xWalking, xTuning);
			ZENITH_ASSERT_EQ(eWalking, ZM_TRAINER_SIGHT_ACTION_NONE,
				"arm %u frame %u: walking is not itself an action (got %s)",
				uArm, u, ZM_TrainerSightActionName(eWalking));
		}
		ZENITH_ASSERT_GT(xFsm.GetApproachElapsedSeconds(), 0.0f,
			"arm %u: the partial walk timer must be POPULATED before the cancel, or "
			"the clear below proves nothing", uArm);
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_APPROACHING,
			"arm %u: a tenth of a 2.0s walk must not have finished it (got %s)",
			uArm, ZM_TrainerSightStateName(xFsm.GetState()));

		ZM_TrainerSightInputs xCancelled = xWalking;
		if (uArm == 0u)
		{
			xCancelled.m_bTargetInSight = false;   // he walked out of the cone
		}
		else
		{
			xCancelled.m_bMayEngage = false;       // the gate slammed mid-walk
		}
		const ZM_TRAINER_SIGHT_ACTION eCancelled = xFsm.Step(xCancelled, xTuning);
		ZENITH_ASSERT_EQ(eCancelled, ZM_TRAINER_SIGHT_ACTION_NONE,
			"cancelling arm %u emitted an action (got %s)", uArm,
			ZM_TrainerSightActionName(eCancelled));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_WATCHING,
			"cancelling arm %u must re-arm WATCHING (got %s) -- ONE of the two "
			"conditions is enough, so this arm reds a conjunction", uArm,
			ZM_TrainerSightStateName(xFsm.GetState()));
		ZENITH_ASSERT_EQ_FLOAT(xFsm.GetApproachElapsedSeconds(), 0.0f, 0.0f,
			"cancelling arm %u must clear the partial walk timer", uArm);
		ZENITH_ASSERT_EQ(xFsm.GetChallengeCount(), 0u,
			"cancelling arm %u must not bark", uArm);
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
			"cancelling arm %u must not start a battle", uArm);
		ZENITH_ASSERT_EQ(xFsm.GetApproachCount(), 1u,
			"cancelling arm %u must not book a SECOND walk on its way out", uArm);

		// PAIRED POSITIVE: the only thing that was wrong is the cancelled clause, so
		// a genuinely new sighting must walk again.
		ApproachOnce(xFsm, xWalking, xTuning);
		ZENITH_ASSERT_EQ(xFsm.GetApproachCount(), 2u,
			"a new sighting after cancel arm %u must be a SECOND distinct walk", uArm);
	}
}

ZENITH_TEST(ZM_Interaction, Fsm_ApproachBusyChannelPausesAndOutranksFailOpen)
{
	ZM_TrainerSightFsm xFsm;
	ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
	// Silent fixture on purpose: the handoff at the end is then RAISE_ENCOUNTER, so
	// this unit also pins that the walk's fail-open is graph-independent.
	const ZM_TrainerSightInputs xWalking = MakeApproachInputs();

	ApproachOnce(xFsm, xWalking, xTuning);

	// ---- (a) PAUSE WITHOUT CONSUMING, against a perfectly HEALTHY timeout -------
	// Thirty simulated seconds at 10s a frame, which would blow through the 2.0s
	// window many times over if the accumulator ran while the channel was busy.
	ZM_TrainerSightInputs xBusy = xWalking;
	xBusy.m_bChannelBusy  = true;
	xBusy.m_fDeltaSeconds = 10.0f;
	for (u_int u = 0u; u < 3u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eBusy = xFsm.Step(xBusy, xTuning);
		ZENITH_ASSERT_EQ(eBusy, ZM_TRAINER_SIGHT_ACTION_NONE,
			"busy walking step %u emitted an action (got %s)", u,
			ZM_TrainerSightActionName(eBusy));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_APPROACHING,
			"a busy channel must PAUSE, not consume, the walk (step %u, got %s)",
			u, ZM_TrainerSightStateName(xFsm.GetState()));
	}
	ZENITH_ASSERT_EQ_FLOAT(xFsm.GetApproachElapsedSeconds(), 0.0f, 0.0f,
		"thirty busy seconds must contribute exactly zero walk time");

	// ---- (b) ...AND IT OUTRANKS THE FAIL-OPEN ----------------------------------
	// The timeout now goes degenerate WHILE the channel is still busy. If the
	// fail-open were checked first this would hand off immediately; the whole point
	// of the ordering is that it does not, because handing off into a busy channel
	// dispatches into a raise that is silently dropped.
	xTuning.m_fApproachTimeoutSeconds = 0.0f;
	for (u_int u = 0u; u < 3u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eHeld = xFsm.Step(xBusy, xTuning);
		ZENITH_ASSERT_EQ(eHeld, ZM_TRAINER_SIGHT_ACTION_NONE,
			"the busy defer must OUTRANK the degenerate-timeout fail-open (step %u, "
			"got %s) -- the fail-open is a FREE-TICK guarantee, not an unconditional "
			"one", u, ZM_TrainerSightActionName(eHeld));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_APPROACHING,
			"a busy channel must hold the walk open even with a corrupt timeout "
			"(step %u, got %s)", u, ZM_TrainerSightStateName(xFsm.GetState()));
		ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 0u,
			"nothing may have raised into a busy channel (step %u)", u);
	}

	// ---- (c) THE PAIRED POSITIVE: the free tick, and ONLY the fail-open ---------
	// dt drops back to one 60Hz frame, so the timer cannot possibly be what fires
	// here; the timeout is still degenerate, so the fail-open is the ONLY candidate.
	ZM_TrainerSightInputs xFree = xWalking;
	xFree.m_fDeltaSeconds = fFRAME_DT;
	const ZM_TRAINER_SIGHT_ACTION eFreed = xFsm.Step(xFree, xTuning);
	ZENITH_ASSERT_EQ(eFreed, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"a degenerate walk timeout must fail OPEN on the first free tick (got %s) -- "
		"presentation must never be able to suppress the battle",
		ZM_TrainerSightActionName(eFreed));
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"the fail-open handoff must leave a silent trainer ENGAGED (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"the freed channel must have produced exactly one raise");
	// The accumulator is the WITNESS that the fail-open, not the clock, did this:
	// the fail-open returns before the accumulate, so not one frame of walk time was
	// ever banked across this entire unit.
	ZENITH_ASSERT_EQ_FLOAT(xFsm.GetApproachElapsedSeconds(), 0.0f, 0.0f,
		"the fail-open must be decided BEFORE the accumulate -- a non-zero timer here "
		"means the clock fired and the fail-open was never exercised");
}

ZENITH_TEST(ZM_Interaction, Fsm_ApproachDegenerateTimeoutFailsOpenBeforeEntry)
{
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();
	constexpr u_int uDEGENERATE_COUNT = 5u;
	const float afDEGENERATE[uDEGENERATE_COUNT] = { 0.0f, fNaN, fInf, -fInf, -1.0f };

	for (u_int u = 0u; u < uDEGENERATE_COUNT; ++u)
	{
		ZM_TrainerSightFsm xFsm;
		ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
		xTuning.m_fApproachTimeoutSeconds = afDEGENERATE[u];

		const ZM_TrainerSightInputs xInputs = MakeApproachInputs();
		EnterSpotted(xFsm, xInputs, xTuning);
		const ZM_TRAINER_SIGHT_ACTION eAction = FinishSpotted(xFsm, xInputs, xTuning);

		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
			"degenerate walk timeout %u must fail OPEN straight past the walk (got %s)",
			u, ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
			"degenerate walk timeout %u must leave a silent trainer ENGAGED (got %s)",
			u, ZM_TrainerSightStateName(xFsm.GetState()));

		// ★ THE LOAD-BEARING CLAUSE, AND THE ONLY ONE THAT CAN SEE THE ORDERING.
		// Deciding the fail-open one line LATER -- after the state entry rather than
		// before it -- returns the IDENTICAL action and leaves the IDENTICAL final
		// state. The count is the sole witness that no walk was ever booked, and a
		// booked walk that no frame could render is a beat the caller would submit
		// an indicator for and then immediately tear down.
		ZENITH_ASSERT_EQ(xFsm.GetApproachCount(), 0u,
			"degenerate walk timeout %u BOOKED a walk before failing open -- the "
			"fail-open must be decided BEFORE the state entry", u);
		ZENITH_ASSERT_EQ_FLOAT(xFsm.GetApproachElapsedSeconds(), 0.0f, 0.0f,
			"degenerate walk timeout %u banked walk time it never spent", u);
	}

	// PAIRED POSITIVE: the ONLY thing wrong above was the timeout, so a healthy one
	// on the identical fixture must book exactly one walk. Without this the negative
	// is satisfiable by an APPROACHING state that nothing can ever enter.
	ZM_TrainerSightFsm xHealthy;
	const ZM_TrainerSightFsmTuning xHealthyTuning = MakeShippedTuning();
	ApproachOnce(xHealthy, MakeApproachInputs(), xHealthyTuning);
	ZENITH_ASSERT_EQ(xHealthy.GetApproachCount(), 1u,
		"a HEALTHY timeout on the same fixture must book exactly one walk");
	ZENITH_ASSERT_GT(xHealthyTuning.m_fApproachTimeoutSeconds, 0.0f,
		"the shipped walk timeout must be a real positive duration, or the sweep "
		"above is indistinguishable from the shipped tuning");
}

ZENITH_TEST(ZM_Interaction, Fsm_ApproachArrivalShortCircuitsTheTimer)
{
	// ---- (a) ARRIVAL, with the clock staged so it CANNOT be responsible ---------
	ZM_TrainerSightFsm xFsm;
	ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
	xTuning.m_fApproachTimeoutSeconds = 10.0f;   // an eternity next to five frames

	ZM_TrainerSightInputs xWalking = MakeApproachInputs();
	ApproachOnce(xFsm, xWalking, xTuning);

	for (u_int u = 0u; u < 5u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eWalking = xFsm.Step(xWalking, xTuning);
		ZENITH_ASSERT_EQ(eWalking, ZM_TRAINER_SIGHT_ACTION_NONE,
			"frame %u of a ten-second walk window must produce nothing (got %s)",
			u, ZM_TrainerSightActionName(eWalking));
		ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_APPROACHING,
			"frame %u of a ten-second walk window must still be walking (got %s)",
			u, ZM_TrainerSightStateName(xFsm.GetState()));
	}
	// The accumulator must be RUNNING but nowhere near the window: that is what
	// makes the arrival below the only possible cause of the handoff.
	ZENITH_ASSERT_GT(xFsm.GetApproachElapsedSeconds(), 0.0f,
		"the walk clock must be RUNNING, or the short-circuit below is vacuous");
	ZENITH_ASSERT_LT(xFsm.GetApproachElapsedSeconds(), xTuning.m_fApproachTimeoutSeconds,
		"the walk clock must be nowhere near its window, or the timer -- not the "
		"arrival -- could be what ends the beat");

	ZM_TrainerSightInputs xArrived = xWalking;
	xArrived.m_bApproachArrived = true;
	const ZM_TRAINER_SIGHT_ACTION eArrived = xFsm.Step(xArrived, xTuning);
	ZENITH_ASSERT_EQ(eArrived, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"arriving must end the walk IMMEDIATELY (got %s) -- the timeout exists only "
		"for a body that can never arrive", ZM_TrainerSightActionName(eArrived));
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_ENGAGED,
		"the arrival handoff must leave a silent trainer ENGAGED (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
	ZENITH_ASSERT_EQ(xFsm.GetRaiseCount(), 1u,
		"the arrival must produce exactly one raise");
	ZENITH_ASSERT_LT(xFsm.GetApproachElapsedSeconds(), xTuning.m_fApproachTimeoutSeconds,
		"the arrival must have SHORT-CIRCUITED the timer -- the beat ended with the "
		"clock still short of its window, which is the whole claim of this unit");

	// ---- (b) THE OTHER OPERAND: the timer alone still ends a walk nobody finished
	// Without this half, deleting the timeout arm entirely would leave (a) green.
	ZM_TrainerSightFsm xTimed;
	ZM_TrainerSightFsmTuning xShortTuning = MakeShippedTuning();
	xShortTuning.m_fApproachTimeoutSeconds = 0.2f;

	ZM_TrainerSightInputs xNeverArrives = MakeApproachInputs();
	xNeverArrives.m_fDeltaSeconds = 0.05f;       // four twentieths of the window
	ApproachOnce(xTimed, xNeverArrives, xShortTuning);
	ZENITH_ASSERT_FALSE(xNeverArrives.m_bApproachArrived,
		"this half must run with arrival OFF or it proves nothing about the timer");

	for (u_int u = 1u; u <= 3u; ++u)
	{
		const ZM_TRAINER_SIGHT_ACTION eAction = xTimed.Step(xNeverArrives, xShortTuning);
		ZENITH_ASSERT_EQ(eAction, ZM_TRAINER_SIGHT_ACTION_NONE,
			"the walk window must be TIMED, not immediate -- %u twentieths into it "
			"nothing may have happened (got %s)",
			u, ZM_TrainerSightActionName(eAction));
		ZENITH_ASSERT_TRUE(std::isfinite(xTimed.GetApproachElapsedSeconds()),
			"the walk accumulator went non-finite on step %u", u);
	}
	// Bounded rather than counted, so a rounding crumb in the accumulator cannot
	// decide this unit. A clock that ran BACKWARDS never crosses at all and reds on
	// the bound below rather than on an off-by-one frame.
	ZM_TRAINER_SIGHT_ACTION eExpired = ZM_TRAINER_SIGHT_ACTION_NONE;
	u_int uExtraSteps = 0u;
	while (xTimed.GetState() == ZM_TRAINER_SIGHT_APPROACHING && uExtraSteps < 200u)
	{
		eExpired = xTimed.Step(xNeverArrives, xShortTuning);
		++uExtraSteps;
	}
	ZENITH_ASSERT_NE(xTimed.GetState(), ZM_TRAINER_SIGHT_APPROACHING,
		"a body that never arrives must not walk FOREVER -- 200 steps of a 0.2s "
		"window and the machine is still walking, which is what an accumulator "
		"running the wrong way looks like");
	ZENITH_ASSERT_EQ(eExpired, ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
		"a body that never arrives must still deliver the BATTLE when the walk window "
		"elapses (got %s)", ZM_TrainerSightActionName(eExpired));
	ZENITH_ASSERT_GE(xTimed.GetApproachElapsedSeconds(),
		xShortTuning.m_fApproachTimeoutSeconds,
		"the expiry must have been reached by ACCUMULATION -- a clock that ran "
		"backwards would never cross its window at all");
	ZENITH_ASSERT_EQ(xTimed.GetRaiseCount(), 1u,
		"the expired walk must produce exactly one raise");
}

ZENITH_TEST(ZM_Interaction, Fsm_ApproachCountIsMonotonicAndMatchesEntries)
{
	// ★ THE OBSERVABLE IS DERIVED FROM THE TRANSITION THE MACHINE ACTUALLY MADE --
	// "the state was not APPROACHING before this Step and is APPROACHING after it"
	// -- never from a bare ++ beside the thing under test. A tally incremented next
	// to the call it claims to witness stays green when the call is deleted.
	ZM_TrainerSightFsm xFsm;

	// ★ THE WHOLE UNIT RUNS ON A COARSE 0.1s TICK AND A 0.1s/0.2s TUNING -- DO NOT
	// "RESTORE" 60Hz FRAMES AND THE SHIPPED 0.35s/2.0s WINDOWS. What this unit pins
	// is a COUNTING invariant, not a duration: the tuning only has to be small enough
	// relative to the tick that each round completes several full
	// WATCHING -> SPOTTED -> APPROACHING -> ENGAGED -> re-arm cycles. At 60Hz that
	// cost 845 Steps; at 0.1s it costs 128 and observes MORE cycles per round.
	constexpr float fCOARSE_DT      = 0.1f;
	constexpr float fCOARSE_SPOTTED = 0.1f;

	constexpr u_int uROUND_COUNT = 5u;
	// possible / timeout / frames. Round 1 cannot walk at all; round 2 can, but its
	// timeout is degenerate and it must therefore be OBSERVED never to enter; round
	// 3 is deliberately cut off mid-walk so the next round's out-of-sight frame
	// cancels a PARTIAL walk rather than a finished one.
	const bool  abPossible[uROUND_COUNT] = { true, false, true, true, true };
	const float afTimeout [uROUND_COUNT] = { 0.2f, 0.2f,  0.0f, 0.2f, 0.2f };
	const u_int auFrames  [uROUND_COUNT] = { 30u,  30u,   30u,  3u,   30u  };

	u_int auRoundEntries[uROUND_COUNT] = {};
	u_int uObservedEntries = 0u;
	u_int uPreviousCount   = 0u;

	// The per-frame invariant is checked on all 128 frames but REPORTED once: a
	// broken counter diverges on the first bad frame and stays diverged, so
	// asserting in the loop would emit a hundred identical failure lines and bury
	// every other unit in the run.
	bool  bCountTrackedEntries = true;
	bool  bCountWasMonotonic   = true;
	u_int uBadRound    = 0u;
	u_int uBadFrame    = 0u;
	u_int uBadCount    = 0u;
	u_int uBadObserved = 0u;

	for (u_int uRound = 0u; uRound < uROUND_COUNT; ++uRound)
	{
		const u_int uEntriesBefore = uObservedEntries;

		ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
		xTuning.m_fSpottedSeconds         = fCOARSE_SPOTTED;
		xTuning.m_fApproachTimeoutSeconds = afTimeout[uRound];

		ZM_TrainerSightInputs xInputs = MakePassingInputs();
		xInputs.m_bApproachPossible = abPossible[uRound];
		xInputs.m_fDeltaSeconds     = fCOARSE_DT;
		// Frame 0 of every round is deliberately out of sight, so each round is a
		// genuinely NEW spotting rather than a continuation of the last one -- and so
		// round 4 begins by cancelling round 3's unfinished walk.
		ZM_TrainerSightInputs xGone = xInputs;
		xGone.m_bTargetInSight = false;

		for (u_int uFrame = 0u; uFrame <= auFrames[uRound]; ++uFrame)
		{
			const ZM_TRAINER_SIGHT_STATE eBefore = xFsm.GetState();
			xFsm.Step((uFrame == 0u) ? xGone : xInputs, xTuning);
			const ZM_TRAINER_SIGHT_STATE eAfter = xFsm.GetState();

			if (eAfter == ZM_TRAINER_SIGHT_APPROACHING
				&& eBefore != ZM_TRAINER_SIGHT_APPROACHING)
			{
				++uObservedEntries;
			}

			const u_int uCount = xFsm.GetApproachCount();
			if (uCount < uPreviousCount)
			{
				bCountWasMonotonic = false;
			}
			uPreviousCount = uCount;

			if (uCount != uObservedEntries && bCountTrackedEntries)
			{
				bCountTrackedEntries = false;
				uBadRound    = uRound;
				uBadFrame    = uFrame;
				uBadCount    = uCount;
				uBadObserved = uObservedEntries;
			}
		}

		auRoundEntries[uRound] = uObservedEntries - uEntriesBefore;
	}

	ZENITH_ASSERT_TRUE(bCountWasMonotonic,
		"the approach count went BACKWARDS -- it is a MONOTONIC per-session tally and "
		"every unit that reads it assumes so");
	// ★ THE CENTRAL ASSERTION OF THIS UNIT. GetApproachCount() must equal the number
	// of entries into APPROACHING that were OBSERVED through GetState(), frame by
	// frame. Deleting the real transition and leaving the ++ (or the reverse) breaks
	// this and nothing else in the file.
	ZENITH_ASSERT_TRUE(bCountTrackedEntries,
		"round %u frame %u: the counter claims %u walks but %u entries into "
		"APPROACHING were actually OBSERVED -- the count must TRACK the real "
		"transition, not run beside it",
		uBadRound, uBadFrame, uBadCount, uBadObserved);

	// ANTI-VACUITY, and the three claims the rounds were staged to make.
	ZENITH_ASSERT_GT(auRoundEntries[0], 0u,
		"a trainer who CAN walk, with a healthy timeout, must actually enter the walk "
		"-- otherwise every zero below is satisfied by a state nothing ever reaches");
	ZENITH_ASSERT_EQ(auRoundEntries[1], 0u,
		"a trainer who cannot walk must never enter APPROACHING");
	ZENITH_ASSERT_EQ(auRoundEntries[2], 0u,
		"a DEGENERATE timeout must fail open BEFORE the entry -- a walk observed here "
		"is a beat booked that no frame could ever show");
	ZENITH_ASSERT_GT(auRoundEntries[3], 0u,
		"the paired positive after the degenerate round must walk again");
	ZENITH_ASSERT_GT(auRoundEntries[4], 0u,
		"...and so must the round that follows a mid-walk cancel");
	ZENITH_ASSERT_GT(xFsm.GetApproachCount(), 1u,
		"this unit must have driven MORE than one walk, or monotonicity is untested");

	// Reset clears the two SC1 members. Asserted HERE rather than by editing the
	// shipped Reset unit above, whose fixture predates this state.
	ZENITH_ASSERT_GT(xFsm.GetApproachCount(), 0u,
		"the fixture must carry a non-zero approach count before Reset");
	xFsm.Reset();
	ZENITH_ASSERT_EQ(xFsm.GetApproachCount(), 0u,
		"Reset must clear the approach count");
	ZENITH_ASSERT_EQ_FLOAT(xFsm.GetApproachElapsedSeconds(), 0.0f, 0.0f,
		"Reset must clear the approach accumulator");
	ZENITH_ASSERT_EQ(xFsm.GetState(), ZM_TRAINER_SIGHT_WATCHING,
		"Reset must still return a cold WATCHING machine (got %s)",
		ZM_TrainerSightStateName(xFsm.GetState()));
}

// ---- S7 item 1 SC1: the PURE approach maths ---------------------------------

ZENITH_TEST(ZM_Interaction, Approach_StepIsXZOnlyAndTotalOnDegenerateInput)
{
	constexpr float fEPSILON = 0.0001f;

	// ---- (a) the direction points FROM the trainer TOWARD the target -----------
	const Zenith_Maths::Vector3 xTrainer(0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xTarget(10.0f, 0.0f, 0.0f);
	const ZM_TrainerApproachStep xStep =
		ZM_StepTrainerApproach(xTrainer, xTarget, 2.0f, 3.0f);
	ZENITH_ASSERT_FALSE(xStep.m_bArrived,
		"ten metres out with a two metre standoff is NOT arrived");
	ZENITH_ASSERT_EQ_FLOAT(xStep.m_fSpeed, 3.0f, fEPSILON,
		"a walking step must request the speed it was given, unscaled");
	// ★ THE SIGN IS THE WHOLE POINT. A transposed (trainer, target) pair answers
	// (-1,0,0) here, compiles perfectly, keeps both parameters referenced, and sends
	// the trainer running away from the player forever.
	ZENITH_ASSERT_NEAR_VEC3(xStep.m_xDirXZ, Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),
		fEPSILON,
		"the direction must point FROM the trainer TOWARD the target");
	ZENITH_ASSERT_EQ_FLOAT(xStep.m_xDirXZ.y, 0.0f, 0.0f,
		"the direction must carry EXACTLY no vertical component -- gravity and "
		"terrain response stay in the body's sole ownership");

	// ---- (b) XZ ONLY: height changes NOTHING -----------------------------------
	const ZM_TrainerApproachStep xHigh =
		ZM_StepTrainerApproach(Zenith_Maths::Vector3(0.0f, 37.5f, 0.0f), xTarget, 2.0f, 3.0f);
	ZENITH_ASSERT_NEAR_VEC3(xHigh.m_xDirXZ, xStep.m_xDirXZ, fEPSILON,
		"a 37.5m height difference must not move the horizontal direction");
	ZENITH_ASSERT_EQ_FLOAT(xHigh.m_fSpeed, xStep.m_fSpeed, 0.0f,
		"a height difference must not move the requested speed");
	ZENITH_ASSERT_EQ(xHigh.m_bArrived, xStep.m_bArrived,
		"a height difference must not move the arrival answer");

	// One metre apart horizontally and a hundred metres apart vertically IS arrived:
	// folding Y into the distance would leave a trainer stood on the player's toes
	// believing he was still a hundred metres out.
	const ZM_TrainerApproachStep xTall = ZM_StepTrainerApproach(
		Zenith_Maths::Vector3(0.0f, 100.0f, 0.0f),
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), 2.0f, 3.0f);
	ZENITH_ASSERT_TRUE(xTall.m_bArrived,
		"arrival must be measured in XZ ONLY -- a 100m vertical gap with a 1m "
		"horizontal gap is INSIDE a 2m standoff");
	ZENITH_ASSERT_EQ_FLOAT(xTall.m_fSpeed, 0.0f, 0.0f,
		"an arrived step must request exactly zero speed");

	// ---- (c) TOTAL: it never asserts and never answers with a non-finite --------
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();
	// ★ DELIBERATELY 5 VALUES, NOT 7 -- DO NOT "RESTORE" -inf AND 2.0f.
	// Every guard in ZM_StepTrainerApproach is spelled with std::isfinite, so -inf
	// takes the IDENTICAL branch as +inf (and is named explicitly, as "target.z is
	// -inf", in Approach_NonFiniteInputYieldsZeroSpeedAndArrived next door). 2.0f is
	// a plain finite positive with no branch of its own -- 1.0e30f already covers
	// "finite but large enough to matter to the distance arithmetic", and the healthy
	// case is pinned by the named fixtures at the top of THIS test. 5^4 = 625 cases.
	constexpr u_int uVALUE_COUNT = 5u;
	const float afVALUES[uVALUE_COUNT] = { fNaN, fInf, -1.0f, 0.0f, 1.0e30f };

	u_int uHits = 0u;
	bool  bEveryAnswerFinite = true;
	{
		// NO ZENITH_ASSERT_* MAY APPEAR INSIDE THIS SCOPE (the runner swallows
		// framework failures while a capture is active), and the hit count MUST be
		// copied to a local before the closing brace.
		Zenith_AssertCaptureScope xCapture;

		for (u_int uA = 0u; uA < uVALUE_COUNT; ++uA)
		{
			for (u_int uB = 0u; uB < uVALUE_COUNT; ++uB)
			{
				for (u_int uC = 0u; uC < uVALUE_COUNT; ++uC)
				{
					for (u_int uD = 0u; uD < uVALUE_COUNT; ++uD)
					{
						// ★ THE TWO DERIVED INDICES ARE ROTATED, NOT TIED. Feeding
						// target.z from uA and the speed from uB keeps the STANDOFF
						// (uD) and the SPEED independent of the position indices, so
						// "healthy positions with a NaN standoff" and "healthy
						// positions with a NaN speed" both actually occur and each
						// guard is exercised in ISOLATION rather than always being
						// pre-empted by the position guard above it.
						const float fTargetZ = afVALUES[(uA + 1u) % uVALUE_COUNT];
						const float fSpeed   = afVALUES[(uB + 2u) % uVALUE_COUNT];
						// Both heights are pinned NaN: y is never read, and pinning it
						// to the worst possible value proves so on every one of the
						// 625 cases instead of spending an axis on it.
						const ZM_TrainerApproachStep xSwept = ZM_StepTrainerApproach(
							Zenith_Maths::Vector3(afVALUES[uA], fNaN, afVALUES[uB]),
							Zenith_Maths::Vector3(afVALUES[uC], fNaN, fTargetZ),
							afVALUES[uD],
							fSpeed);
						if (!std::isfinite(xSwept.m_xDirXZ.x)
							|| !std::isfinite(xSwept.m_xDirXZ.y)
							|| !std::isfinite(xSwept.m_xDirXZ.z)
							|| !std::isfinite(xSwept.m_fSpeed))
						{
							bEveryAnswerFinite = false;
						}
					}
				}
			}
		}

		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"the approach maths asserted on an argument -- Zenith_Assert breaks in EVERY "
		"config and the whole unit suite runs at boot, so this would kill the whole "
		"boot unit run rather than fail one test");
	ZENITH_ASSERT_TRUE(bEveryAnswerFinite,
		"a degenerate approach input propagated a NaN or an infinity into the answer "
		"-- a poisoned direction reaches the physics body as a poisoned velocity");
}

ZENITH_TEST(ZM_Interaction, Approach_StandoffIsInclusiveAndNeverOvershoots)
{
	constexpr float fEPSILON  = 0.0001f;
	constexpr float fSTANDOFF = 2.0f;
	constexpr float fSPEED    = 2.0f;
	// ★ 0.1s A STEP, NOT A 60Hz FRAME -- DO NOT "RESTORE" fFRAME_DT HERE.
	// The properties below (the gap never grows, the trainer never crosses the
	// target, the walk stops ON the ring) are scale-free: they hold per STEP, not
	// per second, so a coarse integration pins them exactly as well as a fine one
	// while running 43 iterations instead of 253. The 10.44m fixture still gives
	// forty-odd distinct samples of the shrinking gap, which is the whole point.
	constexpr float fDT       = 0.1f;
	const float fStepLength   = fSPEED * fDT;

	// The standoff the FSM carries is the value SC3 will hand to this function, so
	// it is pinned HERE rather than left as a tuning field nothing ever reads. The
	// FSM itself never sees a position -- this is the only place the two meet.
	const ZM_TrainerSightFsmTuning xShipped = MakeShippedTuning();
	ZENITH_ASSERT_GT(xShipped.m_fApproachStandoffMetres, 0.0f,
		"the shipped standoff must be a real positive distance -- zero or negative "
		"would walk the trainer into the player he is meant to stop in front of");
	ZENITH_ASSERT_EQ_FLOAT(xShipped.m_fApproachStandoffMetres, fSTANDOFF, 0.0f,
		"this unit drives the SHIPPED standoff on purpose; if the tuning moves, move "
		"the fixture with it rather than letting the two silently drift apart");

	// ---- (a) EXACTLY on the ring is ARRIVED (inclusive, as ZM_StepWalker is) ----
	const ZM_TrainerApproachStep xOnRing = ZM_StepTrainerApproach(
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(fSTANDOFF, 0.0f, 0.0f), fSTANDOFF, fSPEED);
	ZENITH_ASSERT_TRUE(xOnRing.m_bArrived,
		"a trainer EXACTLY on the standoff ring has arrived -- the comparison is "
		"inclusive, and an exclusive one would ask for one more step he must not take");
	ZENITH_ASSERT_EQ_FLOAT(xOnRing.m_fSpeed, 0.0f, 0.0f,
		"a trainer on the ring must request exactly zero speed");

	// ---- (b) JUST outside it is NOT arrived (the paired positive) ---------------
	const ZM_TrainerApproachStep xJustOutside = ZM_StepTrainerApproach(
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(fSTANDOFF + 0.01f, 0.0f, 0.0f), fSTANDOFF, fSPEED);
	ZENITH_ASSERT_FALSE(xJustOutside.m_bArrived,
		"a centimetre outside the ring is NOT arrived -- without this half, 'arrived' "
		"could simply be the answer to everything");
	ZENITH_ASSERT_EQ_FLOAT(xJustOutside.m_fSpeed, fSPEED, fEPSILON,
		"a trainer outside the ring must request the full speed");

	// ---- (c) the integrated walk never grows the gap and never crosses over -----
	const Zenith_Maths::Vector3 xTargetPos(10.0f, 0.0f, 3.0f);
	Zenith_Maths::Vector3 xPos(0.0f, 0.0f, 0.0f);
	const float fInitialDX = xTargetPos.x - xPos.x;
	const float fInitialDZ = xTargetPos.z - xPos.z;
	float fDistance = std::hypot(fInitialDX, fInitialDZ);
	ZENITH_ASSERT_GT(fDistance, fSTANDOFF,
		"the walk fixture must START outside the ring or it walks nowhere");

	// The two per-step invariants are checked on every step but REPORTED once: a
	// direction pointing the wrong way violates them on step 1 and on all 1999
	// after it, and two thousand identical failure lines would bury the rest of
	// the boot run.
	bool  bGapNeverGrew   = true;
	bool  bNeverCrossedIt = true;
	u_int uBadStep        = 0u;
	float fBadFrom        = 0.0f;
	float fBadTo          = 0.0f;

	u_int uSteps = 0u;
	bool  bArrived = false;
	// The bound is 200 for a walk that takes 43 steps: generous enough that a
	// legitimate rounding wobble cannot trip it, tight enough that a REVERSED
	// direction (which never arrives) reds in 200 iterations rather than 2000.
	while (!bArrived && uSteps < 200u)
	{
		const ZM_TrainerApproachStep xWalk =
			ZM_StepTrainerApproach(xPos, xTargetPos, fSTANDOFF, fSPEED);
		bArrived = xWalk.m_bArrived;
		if (bArrived)
		{
			break;
		}

		xPos.x += xWalk.m_xDirXZ.x * xWalk.m_fSpeed * fDT;
		xPos.z += xWalk.m_xDirXZ.z * xWalk.m_fSpeed * fDT;
		++uSteps;

		const float fDX = xTargetPos.x - xPos.x;
		const float fDZ = xTargetPos.z - xPos.z;
		const float fNewDistance = std::hypot(fDX, fDZ);
		// A transposed (trainer, target) pair breaks this on step 1: the gap grows
		// instead of shrinking, and the walk then never terminates either.
		if (fNewDistance > fDistance && bGapNeverGrew)
		{
			bGapNeverGrew = false;
			uBadStep = uSteps;
			fBadFrom = fDistance;
			fBadTo   = fNewDistance;
		}
		// NEVER THROUGH THE TARGET: the remaining offset must keep pointing the way
		// it started. A sign flip IS the definition of an overshoot.
		if ((fDX * fInitialDX + fDZ * fInitialDZ) <= 0.0f)
		{
			bNeverCrossedIt = false;
		}
		fDistance = fNewDistance;
	}

	ZENITH_ASSERT_TRUE(bGapNeverGrew,
		"step %u moved the trainer AWAY from the target (%f -> %f) -- the direction "
		"is pointing the wrong way", uBadStep, fBadFrom, fBadTo);
	ZENITH_ASSERT_TRUE(bNeverCrossedIt,
		"the walk carried the trainer THROUGH the target and out the far side");
	ZENITH_ASSERT_TRUE(bArrived,
		"the walk never reached the standoff ring in %u steps -- a direction pointing "
		"away from the target never converges", uSteps);
	ZENITH_ASSERT_LE(fDistance, fSTANDOFF,
		"the walk stopped OUTSIDE the ring it was asked to stop on (%f > %f)",
		fDistance, fSTANDOFF);
	ZENITH_ASSERT_GT(fDistance, fSTANDOFF - fStepLength,
		"the walk blew PAST the ring by more than one step's travel (%f), so it is "
		"not stopping on the ring at all", fDistance);

	// Once arrived the answer must FREEZE: any residual speed here walks the capsule
	// into the player it just stopped in front of.
	const ZM_TrainerApproachStep xHeld =
		ZM_StepTrainerApproach(xPos, xTargetPos, fSTANDOFF, fSPEED);
	ZENITH_ASSERT_TRUE(xHeld.m_bArrived,
		"an arrived walk must stay arrived when asked again");
	ZENITH_ASSERT_EQ_FLOAT(xHeld.m_fSpeed, 0.0f, 0.0f,
		"an arrived walk must request EXACTLY zero speed");
	ZENITH_ASSERT_NEAR_VEC3(xHeld.m_xDirXZ, Zenith_Maths::Vector3(0.0f), 0.0f,
		"an arrived walk must request an exactly zero direction");
}

ZENITH_TEST(ZM_Interaction, Approach_NonFiniteInputYieldsZeroSpeedAndArrived)
{
	const float fNaN = std::numeric_limits<float>::quiet_NaN();
	const float fInf = std::numeric_limits<float>::infinity();
	const Zenith_Maths::Vector3 xHere(0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xThere(10.0f, 0.0f, 0.0f);

	// Each case names exactly ONE poisoned field, so a red message says which guard
	// stopped working rather than "something about degenerate input".
	AssertApproachFailedOpen(
		ZM_StepTrainerApproach(Zenith_Maths::Vector3(fNaN, 0.0f, 0.0f), xThere, 2.0f, 3.0f),
		"trainer.x is NaN");
	AssertApproachFailedOpen(
		ZM_StepTrainerApproach(Zenith_Maths::Vector3(0.0f, 0.0f, fInf), xThere, 2.0f, 3.0f),
		"trainer.z is +inf");
	AssertApproachFailedOpen(
		ZM_StepTrainerApproach(xHere, Zenith_Maths::Vector3(fNaN, 0.0f, 0.0f), 2.0f, 3.0f),
		"target.x is NaN");
	AssertApproachFailedOpen(
		ZM_StepTrainerApproach(xHere, Zenith_Maths::Vector3(0.0f, 0.0f, -fInf), 2.0f, 3.0f),
		"target.z is -inf");
	AssertApproachFailedOpen(ZM_StepTrainerApproach(xHere, xThere, fNaN, 3.0f),
		"the standoff is NaN");
	AssertApproachFailedOpen(ZM_StepTrainerApproach(xHere, xThere, fInf, 3.0f),
		"the standoff is +inf");
	AssertApproachFailedOpen(ZM_StepTrainerApproach(xHere, xThere, -1.0f, 3.0f),
		"the standoff is negative, which names a ring nothing can be inside");
	AssertApproachFailedOpen(ZM_StepTrainerApproach(xHere, xThere, 2.0f, fNaN),
		"the speed is NaN");
	AssertApproachFailedOpen(ZM_StepTrainerApproach(xHere, xThere, 2.0f, fInf),
		"the speed is +inf");
	AssertApproachFailedOpen(ZM_StepTrainerApproach(xHere, xThere, 2.0f, 0.0f),
		"the speed is zero, so this trainer can never close the gap");
	AssertApproachFailedOpen(ZM_StepTrainerApproach(xHere, xThere, 2.0f, -1.0f),
		"the speed is negative, which would otherwise walk him backwards");

	// A NaN HEIGHT is deliberately NOT degenerate: Y is never read, so it must not
	// be able to veto a perfectly good horizontal approach.
	const ZM_TrainerApproachStep xNaNHeight = ZM_StepTrainerApproach(
		Zenith_Maths::Vector3(0.0f, fNaN, 0.0f),
		Zenith_Maths::Vector3(10.0f, fNaN, 0.0f), 2.0f, 3.0f);
	ZENITH_ASSERT_FALSE(xNaNHeight.m_bArrived,
		"a NaN HEIGHT must not fail the approach open -- the maths is XZ-only and "
		"never reads y, so a poisoned height is simply not its business");
	ZENITH_ASSERT_EQ_FLOAT(xNaNHeight.m_fSpeed, 3.0f, 0.0001f,
		"...and the walk must proceed at full speed");

	// A ZERO standoff is legitimate ("stand on the target"), and a coincident pair
	// must arrive rather than divide by zero on the way to a NaN direction.
	const ZM_TrainerApproachStep xCoincident =
		ZM_StepTrainerApproach(xHere, xHere, 0.0f, 3.0f);
	ZENITH_ASSERT_TRUE(xCoincident.m_bArrived,
		"a coincident pair with a zero standoff has arrived, not divided by zero");
	ZENITH_ASSERT_TRUE(std::isfinite(xCoincident.m_xDirXZ.x)
		&& std::isfinite(xCoincident.m_xDirXZ.z),
		"a coincident pair must never produce a non-finite direction");

	// THE PAIRED POSITIVE for this entire unit: with every field healthy the SAME
	// fixture is NOT arrived and asks for the full speed. Without it, a function
	// that answered "arrived, zero speed" to absolutely everything would pass.
	const ZM_TrainerApproachStep xHealthy =
		ZM_StepTrainerApproach(xHere, xThere, 2.0f, 3.0f);
	ZENITH_ASSERT_FALSE(xHealthy.m_bArrived,
		"a HEALTHY ten-metre approach must not report arrived");
	ZENITH_ASSERT_EQ_FLOAT(xHealthy.m_fSpeed, 3.0f, 0.0001f,
		"a HEALTHY approach must request the full speed");
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
		// window TRIPLES, each driven from FOUR of the machine's arms. (The fifth,
		// APPROACHING, is unreachable from here and is swept separately below --
		// see the ★ note there.)
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

		// ---- S7 item 1 SC1: the APPROACHING arm and its three new fields ---------
		// ★ THE CROSS PRODUCT ABOVE CANNOT REACH THIS ARM, and saying so is the point
		// of this comment. It seeds four HAND-BUILT fixtures rather than iterating
		// ZM_TRAINER_SIGHT_STATE_COUNT, its bool sweep is five bits wide, and its
		// window TRIPLE does not include m_fApproachTimeoutSeconds -- so a new state
		// reached only by a new input, gated on a new tuning field, is invisible to
		// it. (The NAME walks below DO iterate to _STATE_COUNT and therefore did pick
		// the new ordinal up for free.)
		//
		// ★ DELIBERATELY 576 CASES, NOT 4608 -- DO NOT "RESTORE" THE WIDER SET.
		// The boot suite is PER-TEST-OVERHEAD BOUND (every unit pays an ECS scene
		// reset in Zenith_TestRunner::RunAllTests), so breadth bought here is paid
		// for at every single boot for the life of the project. What was dropped is
		// REDUNDANT, not merely cheaper:
		//   * m_bSightLineClear is TIED to m_bTargetInSight -- 16 bool cases, not 32.
		//     Step computes `bSees` as their CONJUNCTION once, above the switch, so
		//     the two mixed pairs are behaviourally identical to (false, false); and
		//     the 41,472-case sweep above already drives all four raw combinations
		//     through that very expression.
		//   * the delta and window inventories drop from six values to THREE. Both
		//     guards are spelled `!(isfinite(x) && x > 0.0f)`, so +inf, -inf and
		//     -1.0f take the IDENTICAL branch as NaN. NaN is kept because it is the
		//     only value that can POISON an accumulator, and 0.0f because it is the
		//     boundary of the `> 0.0f` comparison itself.
		// What is NOT reduced, and must not be: both new bools keep all FOUR
		// combinations, because m_bApproachPossible gates the entry and
		// m_bApproachArrived gates the exit and neither is swept anywhere else.
		constexpr u_int uAPPROACH_BIT_COUNT   = 4u;
		constexpr u_int uAPPROACH_CLASS_COUNT = 3u;
		const float afAPPROACH_DELTAS[uAPPROACH_CLASS_COUNT]  = { fNaN, 0.0f, fFRAME_DT };
		const float afAPPROACH_WINDOWS[uAPPROACH_CLASS_COUNT] = { fNaN, 0.0f, 0.5f };

		// Loop-invariant, so hoisted: the seed only has to put a FRESH machine into
		// APPROACHING, and the tuning/inputs that do it never vary with the sweep.
		ZM_TrainerSightFsmTuning xSeedTuning = MakeShippedTuning();
		xSeedTuning.m_fSpottedSeconds = 0.0f;   // fail open so ONE Step reaches the walk
		const ZM_TrainerSightInputs xSeedInputs = MakeApproachInputs();

		for (u_int uBits = 0u; uBits < 16u; ++uBits)
		{
			for (u_int uDelta = 0u; uDelta < uAPPROACH_CLASS_COUNT; ++uDelta)
			{
				for (u_int uApproachBits = 0u; uApproachBits < uAPPROACH_BIT_COUNT; ++uApproachBits)
				{
					for (u_int uWindow = 0u; uWindow < uAPPROACH_CLASS_COUNT; ++uWindow)
					{
						ZM_TrainerSightInputs xInputs;
						xInputs.m_bMayEngage          = (uBits & 1u) != 0u;
						xInputs.m_bTargetInSight      = (uBits & 2u) != 0u;
						// TIED to the bit above on purpose -- see the ★ note.
						xInputs.m_bSightLineClear     = (uBits & 2u) != 0u;
						xInputs.m_bChannelBusy        = (uBits & 4u) != 0u;
						xInputs.m_bChallengeAvailable = (uBits & 8u) != 0u;
						xInputs.m_bApproachPossible   = (uApproachBits & 1u) != 0u;
						xInputs.m_bApproachArrived    = (uApproachBits & 2u) != 0u;
						xInputs.m_fDeltaSeconds       = afAPPROACH_DELTAS[uDelta];

						ZM_TrainerSightFsmTuning xTuning = MakeShippedTuning();
						xTuning.m_fApproachTimeoutSeconds = afAPPROACH_WINDOWS[uWindow];

						// A GENUINE walking machine, seeded from the hoisted fixture.
						ZM_TrainerSightFsm xApproaching;
						const ZM_TRAINER_SIGHT_ACTION eApproachSeed =
							xApproaching.Step(xSeedInputs, xSeedTuning);
						const ZM_TRAINER_SIGHT_ACTION eApproaching =
							xApproaching.Step(xInputs, xTuning);

						// ...and a COLD one, so the rewritten SPOTTED exit is swept
						// from the entry side as well as from inside the walk.
						ZM_TrainerSightFsm xColdWalker;
						const ZM_TRAINER_SIGHT_ACTION eColdWalker =
							xColdWalker.Step(xInputs, xTuning);

						(void)eApproachSeed; (void)eApproaching; (void)eColdWalker;
					}
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

// ---- The process-global CINEMATIC FREEZE latch (S7 item 1 SC2) ----------------
//
// The FOURTH freeze owner. Everything below is PURE -- the latch owns no controller
// and writes no movement bool; it only answers the question
// ZM_UI_MenuStack::UnfreezePlayer now asks. That arbitration is proven LIVE, in the
// real overworld, by the freeze-hold/release phase of ZM_TrainerSightWalkUp_Test;
// these four units pin the latch's own algebra, and the split is deliberate: a
// mutation to the UnfreezePlayer clause must red the AUTOMATED phase and NOT these,
// or the wiring was only ever inspected.
//
// Every unit resets on ENTRY as well as on exit, for the same reason the engagement
// latch units above do: this is ownerless process-global state, so a unit that only
// tidied up afterwards would still inherit whatever ran first.

ZENITH_TEST(ZM_Interaction, CinematicLatch_BeginIsPerTrainerAndEndClearsIt)
{
	ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();
	ZENITH_ASSERT_FALSE(ZM_TrainerCinematicLatch::IsActive(),
		"a reset cinematic latch holds nobody");
	ZENITH_ASSERT_EQ((u_int)ZM_TrainerCinematicLatch::GetActiveTrainerForTests(),
		(u_int)ZM_TRAINER_NONE,
		"a reset cinematic latch parks the NONE sentinel, not a stale id");

	ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_RIVAL_VESPER);
	ZENITH_ASSERT_TRUE(ZM_TrainerCinematicLatch::IsActive(),
		"Begin on a REGISTERED trainer must arm the freeze");
	// ★ THE OWNER IS NAMED, not merely counted. A Begin that armed the latch for a
	// FIXED id would satisfy IsActive() forever while attributing every cinematic to
	// the wrong trainer -- and the release path, which is per-operation rather than
	// per-caller, would then be releasing something nobody can identify.
	ZENITH_ASSERT_EQ((u_int)ZM_TrainerCinematicLatch::GetActiveTrainerForTests(),
		(u_int)ZM_TRAINER_RIVAL_VESPER,
		"the latch must record the trainer it was actually begun for");

	// A DIFFERENT registered trainer takes it over. Last writer wins, by design: the
	// alternative (stacking a second claim) is what turns one missed End() into a
	// permanently frozen player.
	ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_ROUTE1_RAMBLER);
	ZENITH_ASSERT_TRUE(ZM_TrainerCinematicLatch::IsActive(),
		"a hand-over leaves the freeze armed");
	ZENITH_ASSERT_EQ((u_int)ZM_TrainerCinematicLatch::GetActiveTrainerForTests(),
		(u_int)ZM_TRAINER_ROUTE1_RAMBLER,
		"Begin for a second trainer must HAND THE FREEZE OVER, not be ignored in "
		"favour of the incumbent");

	// ★ ONE End() RELEASES, whatever ran before it. This is the clause that stands
	// between a walk-up and a player who can never move again.
	ZM_TrainerCinematicLatch::End();
	ZENITH_ASSERT_FALSE(ZM_TrainerCinematicLatch::IsActive(),
		"End must release the freeze");
	ZENITH_ASSERT_EQ((u_int)ZM_TrainerCinematicLatch::GetActiveTrainerForTests(),
		(u_int)ZM_TRAINER_NONE,
		"End must park the NONE sentinel, so no later reader can see a released "
		"cinematic still naming an owner");

	// TOTAL: a second End() with nothing armed is legal and inert. The release path
	// must never need a precondition.
	ZM_TrainerCinematicLatch::End();
	ZENITH_ASSERT_FALSE(ZM_TrainerCinematicLatch::IsActive(),
		"a redundant End is inert, never a re-arm");

	ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();
}

ZENITH_TEST(ZM_Interaction, CinematicLatch_UnregisteredIdIsInertAndSilent)
{
	ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();

	u_int uHits = 0u;
	bool  bActiveAfterGarbage = true;
	// ★ SAMPLED INSIDE THE SCOPE AND STRICTLY BEFORE ANY End(): End() clears the owner
	// slot, so releasing first would wipe exactly the stray write this unit exists to
	// find and the assertion below would pass on a latch that HAD been corrupted.
	ZM_TRAINER_ID eSlotAfterGarbage = ZM_TRAINER_RIVAL_VESPER;
	{
		Zenith_AssertCaptureScope xCapture;

		ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_NONE);
		ZM_TrainerCinematicLatch::Begin((ZM_TRAINER_ID)9999u);
		eSlotAfterGarbage    = ZM_TrainerCinematicLatch::GetActiveTrainerForTests();
		bActiveAfterGarbage  = ZM_TrainerCinematicLatch::IsActive();
		// End() on a latch nobody armed is part of the SAME totality claim: the release
		// path may never need a precondition, so it may never assert on one either.
		ZM_TrainerCinematicLatch::End();

		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"the cinematic latch asserted on an unregistered id -- Zenith_Assert breaks in "
		"EVERY config, so this would kill the whole boot unit run rather than fail one "
		"test");
	ZENITH_ASSERT_FALSE(bActiveAfterGarbage,
		"an unregistered Begin must never arm the freeze -- an un-attributable owner "
		"holding the player is exactly the stranded-frozen failure");
	// The RAW slot, not just IsActive(): a stray write parked there would be invisible
	// to the accessor (which re-validates before answering) but is still corruption --
	// the SAME reason GetEngagedMaskForTests exists next door.
	ZENITH_ASSERT_EQ((u_int)eSlotAfterGarbage, (u_int)ZM_TRAINER_NONE,
		"an unregistered Begin must leave the owner slot bit-for-bit untouched");

	ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();
}

ZENITH_TEST(ZM_Interaction, CinematicLatch_ResetClearsEverything)
{
	ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();

	ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_RIVAL_VESPER);
	ZENITH_ASSERT_TRUE(ZM_TrainerCinematicLatch::IsActive(),
		"the fixture must really be armed, or the reset below clears nothing and this "
		"unit is vacuous");

	ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();
	ZENITH_ASSERT_FALSE(ZM_TrainerCinematicLatch::IsActive(),
		"Reset must release the freeze");
	ZENITH_ASSERT_EQ((u_int)ZM_TrainerCinematicLatch::GetActiveTrainerForTests(),
		(u_int)ZM_TRAINER_NONE,
		"Reset must leave the owner slot empty, so this unit cannot leak a FREEZE into "
		"the rest of the boot suite");

	// ★ AND IT IS THE SAME RELEASE End() PERFORMS. The between-tests hook in
	// Zenithmon.cpp calls Reset, never End, so if the two ever diverged a batched
	// automated test could inherit a half-released freeze that no End() would clear.
	ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_ROUTE1_RAMBLER);
	ZM_TrainerCinematicLatch::End();
	const ZM_TRAINER_ID eAfterEnd = ZM_TrainerCinematicLatch::GetActiveTrainerForTests();
	ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_ROUTE1_RAMBLER);
	ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();
	ZENITH_ASSERT_EQ((u_int)ZM_TrainerCinematicLatch::GetActiveTrainerForTests(),
		(u_int)eAfterEnd,
		"Reset and End must leave the latch in the SAME observable state -- the "
		"between-tests hook calls Reset, the runtime calls End, and a divergence there "
		"is a freeze that survives into the next test");
}

ZENITH_TEST(ZM_Interaction, CinematicLatch_ReBeginWhileActiveIsIdempotent)
{
	ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();

	// ★ THE WHOLE POINT: there is NO REFCOUNT. Arm the same trainer repeatedly and a
	// SINGLE End() must still release. A depth counter here would be the classic
	// stranded-player bug -- m_bMovementEnabled is a bare bool with four owners, and
	// the one that never releases wins forever.
	ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_RIVAL_VESPER);
	ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_RIVAL_VESPER);
	ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_RIVAL_VESPER);
	ZENITH_ASSERT_TRUE(ZM_TrainerCinematicLatch::IsActive(),
		"three Begins leave the freeze armed");
	ZENITH_ASSERT_EQ((u_int)ZM_TrainerCinematicLatch::GetActiveTrainerForTests(),
		(u_int)ZM_TRAINER_RIVAL_VESPER,
		"re-arming for the SAME trainer must not disturb the owner");

	ZM_TrainerCinematicLatch::End();
	ZENITH_ASSERT_FALSE(ZM_TrainerCinematicLatch::IsActive(),
		"ONE End must release three Begins -- a latch that counted its arms would "
		"still be holding the player here, and nothing in the shipped code would ever "
		"call End the missing two times");
	ZENITH_ASSERT_EQ((u_int)ZM_TrainerCinematicLatch::GetActiveTrainerForTests(),
		(u_int)ZM_TRAINER_NONE,
		"the owner slot is empty after the single End");

	// An unregistered Begin must not be able to DISARM a live cinematic either: the
	// rejection is total in both directions.
	ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_RIVAL_VESPER);
	ZM_TrainerCinematicLatch::Begin(ZM_TRAINER_NONE);
	ZENITH_ASSERT_TRUE(ZM_TrainerCinematicLatch::IsActive(),
		"a rejected Begin must leave the incumbent cinematic alone");
	ZENITH_ASSERT_EQ((u_int)ZM_TrainerCinematicLatch::GetActiveTrainerForTests(),
		(u_int)ZM_TRAINER_RIVAL_VESPER,
		"a rejected Begin must not overwrite the armed owner with the sentinel");

	ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();
}
