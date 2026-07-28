#pragma once

#include "Zenithmon/Source/Data/ZM_StoryFlags.h"    // ZM_STORY_FLAG_COUNT (the "row has a flag" test)
#include "Zenithmon/Source/Data/ZM_TrainerData.h"   // ZM_TrainerData / ZM_TRAINER_ID / ZM_IsRegisteredTrainer

// ============================================================================
// ZM_TrainerSightFsm (S7 item 3 SC6) -- the PURE trainer sight state machine and
// the PURE re-engagement gate. Plain values in, one action out: NO ECS, NO scene,
// NO physics, NO raycast, NO UI, NO g_xEngine, NO RNG, NO allocation, NO I/O.
// The impure half is exactly three things and lives in ZM_Interactable.cpp:
// reading the two transforms, casting the occlusion ray, and dispatching the
// event.
//
// It does NOT re-implement the sight cone. ZM_IsTargetInTrainerSightFromRotation
// (SC3) is the ONE cone in this game; its answer arrives here as a bool.
//
// EVERY function below is TOTAL and NEVER calls Zenith_Assert: the boot units
// feed it NaN deltas, negative tunings and the ZM_TRAINER_NONE sentinel on
// purpose, and an assert on a unit-supplied input does not fail one test -- it
// ends the whole boot unit run.
// ============================================================================

enum ZM_TRAINER_SIGHT_STATE : u_int
{
	// Eyes open. The only state that may raise.
	ZM_TRAINER_SIGHT_WATCHING = 0u,
	// Has raised once for THIS spotting and is deliberately silent. Re-arms only
	// when the target leaves sight, or when the raise is judged to have been
	// dropped (see m_fRaiseConfirmSeconds).
	ZM_TRAINER_SIGHT_ENGAGED,
	// S7 item 3 SC7. Has asked the challenge graph to speak and is waiting to learn
	// whether it did. APPENDED (not inserted before ENGAGED) so no existing ordinal
	// moves -- this machine is session-only and serializes nowhere, but the roster
	// enums next door are APPEND-ONLY and there is no reason for two doctrines.
	ZM_TRAINER_SIGHT_CHALLENGING,

	// NOT a state -- the walkable bound the totality unit iterates to.
	ZM_TRAINER_SIGHT_STATE_COUNT
};

enum ZM_TRAINER_SIGHT_ACTION : u_int
{
	ZM_TRAINER_SIGHT_ACTION_NONE = 0u,
	// Dispatch ZM_OnTrainerEncounter. Returned on EXACTLY ONE Step per spotting.
	ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,
	// S7 item 3 SC7. Fire the challenge graph's custom event. The caller does NOT
	// mark the engagement latch and does NOT dispatch anything on this action: the
	// bark is a presentation beat, not the encounter.
	ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE,

	ZM_TRAINER_SIGHT_ACTION_COUNT
};

// One tick's worth of world, BY VALUE. Each bool is answered by exactly one
// already-shipped seam, named here so the glue cannot invent a second source.
struct ZM_TrainerSightInputs
{
	// ZM_MayTrainerEngage(row, defeatFlagSet, sessionLatchSet).
	bool  m_bMayEngage      = false;
	// ZM_IsTargetInTrainerSightFromRotation(...) -- the SC3 pure cone. UNCHANGED.
	bool  m_bTargetInSight  = false;
	// ZM_ProbeTrainerSightLine(...).m_bClear -- the occlusion filter, consulted
	// ONLY when m_bTargetInSight already held (there is no raycast budget in this
	// engine, so cheap-gate-first IS the cost control).
	bool  m_bSightLineClear = false;
	// Something already owns the screen: ZM_BattleTransition::IsTransitionActive()
	// || ZM_GameStateManager::IsWarpInProgress() || ZM_UI_MenuStack::IsMenuOpen().
	// A busy channel DEFERS the raise (the trainer keeps staring); it never
	// consumes it, because Zenith_EventDispatcher::Dispatch returns void and a
	// refused raise is indistinguishable from an accepted one at the call site.
	bool  m_bChannelBusy    = false;

	// S7 item 3 SC7. TRUE only when this trainer actually has something to say:
	// ZM_SelectTrainerChallengeLines(row, ...) yielded a non-zero count.
	//
	// ★ IT DEFAULTS TO FALSE, AND THAT IS WHY SC6'S 16 UNITS PASS UNMODIFIED. With
	// this false the machine is byte-for-byte SC6: WATCHING raises the encounter
	// directly, CHALLENGING is never entered, no window runs. It is also what stops
	// a SILENT trainer (ZM_TRAINER_ROUTE1_RAMBLER) paying a half-second of dead air
	// for a beat he was never going to perform.
	bool  m_bChallengeAvailable = false;

	float m_fDeltaSeconds   = 0.0f;
};

struct ZM_TrainerSightFsmTuning
{
	// How long an ENGAGED trainer waits for the battle channel to go BUSY before
	// concluding its raise was SILENTLY DROPPED and re-arming. This exists because
	// ZM_BattleTransition::IsTransitionActive() reports only s_bTransitionActive:
	// there is NO public accessor for s_bPendingEncounter /
	// s_bPendingTrainerEncounter, so a raise made in the same frame a wild
	// encounter latched is discarded with no trace. Without this window the
	// trainer would fall permanently silent while the player stood in the cone.
	// A non-finite or non-positive value disables the re-arm entirely (fail
	// closed: stay silent rather than raise every frame).
	float m_fRaiseConfirmSeconds = 0.5f;

	// S7 item 3 SC7. How long a CHALLENGING trainer waits for the bark to be
	// OBSERVED -- i.e. for m_bChannelBusy to go true, which is exactly what pushing a
	// ZM_UI_MenuStack dialogue does -- before concluding the graph never spoke.
	//
	// ★ THE POLARITY IS DELIBERATELY OPPOSITE to m_fRaiseConfirmSeconds above and
	// MUST NOT be "tidied" into a shared helper; U4 pins both side by side so nobody
	// can. A degenerate RAISE window disables the re-arm (fail CLOSED -- silence
	// beats a spurious battle). A degenerate CHALLENGE window raises IMMEDIATELY
	// (fail OPEN -- silence here would mean NO BATTLE AT ALL).
	//
	// That asymmetry is the whole reason a gitignored, TOOLS-authored .bgraph is
	// safe to depend on: a _False or Android build loses the bark and keeps the
	// battle.
	float m_fChallengeConfirmSeconds = 0.5f;
};

class ZM_TrainerSightFsm
{
public:
	// The whole machine. Returns the action the caller must perform THIS tick.
	ZM_TRAINER_SIGHT_ACTION Step(const ZM_TrainerSightInputs& xInputs,
		const ZM_TrainerSightFsmTuning& xTuning);

	// Back to a cold watcher. Called from ZM_Interactable::OnStart and from
	// ConfigureTrainerSight, exactly where the walker resets m_xWalkerState.
	void Reset();

	ZM_TRAINER_SIGHT_STATE GetState() const { return m_eState; }
	// MONOTONIC count of raises actually emitted. Assert on THIS rather than on
	// the state alone: a machine stubbed to sit in ENGAGED would satisfy a state
	// check while never having raised anything.
	u_int GetRaiseCount() const { return m_uRaiseCount; }
	float GetConfirmElapsedSeconds() const { return m_fConfirmElapsed; }
	bool  IsRaiseConfirmed() const { return m_bRaiseConfirmed; }

	// MONOTONIC count of challenge beats actually STARTED. Assert on this rather
	// than on the state alone: a machine stubbed to sit in CHALLENGING would satisfy
	// a state check while never having asked anyone to speak.
	u_int GetChallengeCount() const { return m_uChallengeCount; }
	// The bark was OBSERVED to land (the channel went busy while CHALLENGING).
	bool  IsChallengeAccepted() const { return m_bChallengeAccepted; }
	float GetChallengeElapsedSeconds() const { return m_fChallengeElapsed; }

private:
	ZM_TRAINER_SIGHT_STATE m_eState          = ZM_TRAINER_SIGHT_WATCHING;
	float                  m_fConfirmElapsed = 0.0f;
	bool                   m_bRaiseConfirmed = false;
	u_int                  m_uRaiseCount     = 0u;

	float                  m_fChallengeElapsed  = 0.0f;
	bool                   m_bChallengeAccepted = false;
	u_int                  m_uChallengeCount    = 0u;
};

// A stable short name for a state / action, for log lines and unit failure
// messages. TOTAL: never null, never indexes out of bounds ("UNKNOWN").
const char* ZM_TrainerSightStateName(ZM_TRAINER_SIGHT_STATE eState);
const char* ZM_TrainerSightActionName(ZM_TRAINER_SIGHT_ACTION eAction);

// ---- The re-engagement gate (the adopted answer to Q-2026-07-28-001) --------
//
// "May this trainer force a battle at all right now?", as a PURE function of the
// row and the two observations, so both arms are unit-testable with no game state.
//
//   * row NOT registered (the shared UNKNOWN row, or garbage) -> FALSE, closed.
//   * row HAS a defeat flag (m_eDefeatFlag < ZM_STORY_FLAG_COUNT) -> !bDefeatFlagSet.
//   * row has NO defeat flag (ZM_STORY_FLAG_NONE, as ZM_TRAINER_ROUTE1_RAMBLER's
//     is) -> !bSessionLatchSet.
//
// The second arm is the WHOLE point. ZM_IsStoryFlagSet(state, ZM_STORY_FLAG_NONE)
// returns false forever, silently (ZM_StoryFlags.cpp:100-117), so a flagless row
// gated on its flag alone is permanently "not yet defeated" and its prize is
// farmable. Do NOT reach for ZM_StoryGatePasses here: its NONE convention is the
// opposite ("never gated" == always passes), which is the wrong answer twice over.
//
// ASYMMETRY, DELIBERATE AND DOCUMENTED: a FLAGGED row ignores the latch, so
// losing to Vesper (a loss writes no flag -- ZM_BattleWriteBack.cpp:124-127 fails
// closed on anything but a WIN) leaves them re-battleable. That is the intended
// RPG behaviour: heal up and come back. A FLAGLESS row's latch is set on the
// RAISE, not on a win, because SC6 observes no battle outcome; it therefore means
// "this trainer has already forced a battle this session", which is exactly what
// its name says and is not farmable in either direction.
//
// TOTAL. Never asserts.
bool ZM_MayTrainerEngage(const ZM_TrainerData& xRow,
	bool bDefeatFlagSet,
	bool bSessionLatchSet);

// ---- The session latch -------------------------------------------------------
//
// OWNERLESS process-global state, one bit per ZM_TRAINER_ID, with static
// accessors -- NOT a per-component bool. A per-component latch would die with the
// scene, and Dawnmere is re-loaded SINGLE on every door round trip, so a flagless
// trainer would be re-farmable by walking into the player's house and back out.
//
// Because it is ownerless it MUST be cleared from the between-tests hook in
// Zenithmon.cpp (convention C3); the harness's scene-0 force-reload cannot reach it.
class ZM_TrainerEngagementLatch
{
public:
	// TOTAL and SILENT: an unregistered id (including the ZM_TRAINER_NONE
	// sentinel) is inert -- no bit is set, nothing is logged, nothing asserts.
	static void MarkEngaged(ZM_TRAINER_ID eTrainer);
	static bool HasEngaged(ZM_TRAINER_ID eTrainer);
	static void ResetRuntimeStateForTests();
	// Raw mask, for the unit that proves an unregistered MarkEngaged sets NO bit
	// at all (asserting only on HasEngaged could not see a stray high bit).
	static u_int GetEngagedMaskForTests();

private:
	static u_int s_uEngagedMask;
};

static_assert((u_int)ZM_TRAINER_COUNT <= 32u,
	"ZM_TrainerEngagementLatch stores one bit per trainer in a single u_int");
