#pragma once

#include "Maths/Zenith_Maths.h"                     // Zenith_Maths::Vector3 (the S7 item 1 SC1 approach step)
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
	// Known-limit W3. A short, cancellable presentation window between first sight
	// and either the challenge bark or a silent trainer's encounter. APPENDED to
	// preserve every shipped ordinal; session-only and never serialized.
	ZM_TRAINER_SIGHT_SPOTTED,
	// S7 item 1 SC1. The walk-up: the trainer physically closes the gap before the
	// bark/encounter handoff runs. APPENDED at ordinal 4 -- SPOTTED keeps 3 and no
	// shipped ordinal moves, for the same reason CHALLENGING and SPOTTED were
	// appended rather than inserted.
	//
	// ★ NOTHING ENTERS THIS STATE UNLESS ZM_TrainerSightInputs::m_bApproachPossible
	// IS TRUE, and that field defaults FALSE, so every caller that has not been
	// taught about the walk keeps the pre-SC1 SPOTTED exit byte for byte. SC1 ships
	// the state and its maths with NO runtime caller on purpose.
	ZM_TRAINER_SIGHT_APPROACHING,

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
	// ZM_MayTrainerEngage(row, defeatFlagSet, sessionLatchSet, playerCanBattle).
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
	// It defaults to false so a silent trainer never enters CHALLENGING. Known-limit
	// W3 still gives every trainer the shared SPOTTED presentation window first;
	// after it, false routes directly to the encounter and true routes to the bark.
	bool  m_bChallengeAvailable = false;

	// S7 item 1 SC1. TRUE only when this trainer can physically WALK: the component
	// owns a valid DYNAMIC CAPSULE body on an active simulation (the contract
	// ZM_Interactable::TryConfigureWanderBody already enforces). It is NOT a
	// designer switch and it is NOT read from the roster row -- a trainer authored
	// as a static box simply never approaches.
	//
	// ★ IT DEFAULTS FALSE, AND THAT DEFAULT IS THE WHOLE COMPATIBILITY STORY. Every
	// shipped caller that has not been taught to fill it keeps the pre-SC1 SPOTTED
	// exit unchanged, which is exactly the technique m_bChallengeAvailable above
	// used when SC7 landed.
	bool  m_bApproachPossible = false;
	// S7 item 1 SC1. TRUE when the walk is DONE -- the impure half measured the XZ
	// gap with ZM_StepTrainerApproach and found it inside the standoff ring. It
	// SHORT-CIRCUITS m_fApproachTimeoutSeconds; that timeout exists only so a body
	// which can never arrive (wedged on geometry, asleep, shoved off a ledge)
	// cannot strand the battle behind the walk.
	bool  m_bApproachArrived  = false;

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

	// Known-limit W3. How long the visible exclamation mark stays up before the
	// existing challenge/encounter path continues. While SPOTTED, lost sight or a
	// closed engagement gate cancels cleanly, and a busy channel pauses the clock.
	// A non-finite or non-positive duration fails OPEN ON A FREE TICK so
	// presentation can never suppress the battle -- the busy-channel defer is
	// checked FIRST and outranks it, exactly as it already does in WATCHING, since
	// raising into a busy channel is silently dropped.
	float m_fSpottedSeconds = 0.35f;

	// S7 item 1 SC1. The longest the walk-up may run before the machine stops
	// waiting to arrive and hands off anyway.
	//
	// ★ FAILS OPEN ON A FREE TICK, the SAME polarity as m_fSpottedSeconds above and
	// for the same reason: the walk is PRESENTATION, and presentation must never be
	// able to suppress the battle. Two ordering rules come with it and are pinned by
	// units, not by this comment:
	//   * the busy-channel defer is checked FIRST and OUTRANKS the fail-open, so the
	//     fail-open is a FREE-TICK guarantee rather than an unconditional one;
	//   * the fail-open is decided BEFORE the state entry, so m_uApproachCount can
	//     never book a walk that no frame could ever show. (Deciding it after entry
	//     produces an IDENTICAL action and an IDENTICAL final state -- only the
	//     count can see the difference, which is why the count exists.)
	float m_fApproachTimeoutSeconds = 2.0f;

	// S7 item 1 SC1. How close, in metres, the trainer stops to the target. Read
	// ONLY by ZM_StepTrainerApproach; the FSM itself never sees a position, which is
	// precisely why the answer reaches it as the m_bApproachArrived bool.
	float m_fApproachStandoffMetres = 2.0f;
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

	// Known-limit W3 observables. The count is monotonic per component session and
	// distinguishes a real visual-beat entry from merely naming the SPOTTED state.
	u_int GetSpottedCount() const { return m_uSpottedCount; }
	float GetSpottedElapsedSeconds() const { return m_fSpottedElapsed; }

	// S7 item 1 SC1 observables, same doctrine as the three counters above.
	// MONOTONIC count of walk-ups actually ENTERED. Assert on THIS rather than on
	// the state alone: a machine stubbed to sit in APPROACHING would satisfy a state
	// check while never having taken a step, and -- the sharper case -- a fail-open
	// decided one line too late books a beat here that no frame could ever render,
	// which the action and the final state are both blind to.
	u_int GetApproachCount() const { return m_uApproachCount; }
	float GetApproachElapsedSeconds() const { return m_fApproachElapsed; }

private:
	ZM_TRAINER_SIGHT_STATE m_eState          = ZM_TRAINER_SIGHT_WATCHING;
	float                  m_fConfirmElapsed = 0.0f;
	bool                   m_bRaiseConfirmed = false;
	u_int                  m_uRaiseCount     = 0u;

	float                  m_fChallengeElapsed  = 0.0f;
	bool                   m_bChallengeAccepted = false;
	u_int                  m_uChallengeCount    = 0u;

	float                  m_fSpottedElapsed = 0.0f;
	u_int                  m_uSpottedCount    = 0u;

	float                  m_fApproachElapsed = 0.0f;
	u_int                  m_uApproachCount   = 0u;
};

// A stable short name for a state / action, for log lines and unit failure
// messages. TOTAL: never null, never indexes out of bounds ("UNKNOWN").
const char* ZM_TrainerSightStateName(ZM_TRAINER_SIGHT_STATE eState);
const char* ZM_TrainerSightActionName(ZM_TRAINER_SIGHT_ACTION eAction);

// ---- The approach step (S7 item 1 SC1) ---------------------------------------
//
// The PURE maths behind the walk-up. One call takes the two world positions and
// answers "which way, how fast, and are we there yet". The impure half -- reading
// the two transforms and pushing the answer through ZM_BuildPatrolVelocity onto a
// dynamic capsule -- stays in ZM_Interactable.cpp, exactly where UpdateWander's
// already lives; NOTHING here touches the ECS, the physics world or a navmesh.
//
// ★ SC1 SHIPS THIS WITH NO RUNTIME CALLER, deliberately. It is the same shape
// ZM_BattleDirector::BuildTrainerBattleConfig() landed in: the state and its
// arithmetic are proven in isolation first, and a later sub-commit wires them to a
// body. A pure layer with no caller changes no shipped behaviour by construction.
struct ZM_TrainerApproachStep
{
	// Unit length in XZ with y == 0 exactly, or exactly zero when no motion is
	// requested. Fed straight to ZM_BuildPatrolVelocity, which re-normalises and
	// leaves the vertical component in the body's sole ownership.
	Zenith_Maths::Vector3 m_xDirXZ = Zenith_Maths::Vector3(0.0f);
	float m_fSpeed   = 0.0f;
	bool  m_bArrived = false;
};

// TOTAL, and NEVER calls Zenith_Assert -- the boot units feed it NaN positions,
// infinite standoffs and negative speeds on purpose.
//
// XZ ONLY: the Y difference belongs to the terrain and the capsule. Folding it in
// would make a trainer standing on a slope believe he was still metres away while
// stood on the player's toes.
//
// ★ IT FAILS **OPEN**. Every rejection below returns m_fSpeed == 0 AND
// m_bArrived == true, because "I cannot walk" and "I have walked far enough" must
// be INDISTINGUISHABLE to the caller: the only thing the caller does with
// m_bArrived is stop waiting. A rejection that answered "not arrived, zero speed"
// would park a trainer who can never move in APPROACHING until the timeout
// expired -- i.e. it would turn a corrupt tuning into seconds of dead screen.
//
// Arrival is INCLUSIVE (distance <= standoff), matching ZM_StepWalker's arrive
// radius, so the walk can never request a step that carries the trainer past the
// ring it was asked to stop on.
ZM_TrainerApproachStep ZM_StepTrainerApproach(const Zenith_Maths::Vector3& xTrainer,
	const Zenith_Maths::Vector3& xTarget,
	float fStandoff,
	float fSpeed);

// ---- The re-engagement gate (the adopted answer to Q-2026-07-28-001) --------
//
// "May this trainer force a battle at all right now?", as a PURE function of the
// row and the two observations, so both arms are unit-testable with no game state.
//
//   * row NOT registered (the shared UNKNOWN row, or garbage) -> FALSE, closed.
//   * the player cannot battle (no party to send out) -> FALSE, closed, whichever
//     arm the row would otherwise take.
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
// bPlayerCanBattle is answered by exactly one seam -- ZM_CanEnterBattle(state)
// (ZM_StarterChoice.h) -- and arrives here as a plain bool, so this header keeps
// its "plain values in, one action out" contract and pulls in no game state.
// It takes NO DEFAULT ARGUMENT on purpose: a default would let every shipped call
// site compile untouched, which is precisely how a new arm ships untested.
//
// TOTAL. Never asserts.
bool ZM_MayTrainerEngage(const ZM_TrainerData& xRow,
	bool bDefeatFlagSet,
	bool bSessionLatchSet,
	bool bPlayerCanBattle);

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

// ---- The cinematic freeze latch (S7 item 1 SC2) ------------------------------
//
// THE FOURTH FREEZE OWNER -- and deliberately NOT a fourth freeze CONCEPT. The three
// that already exist (ZM_GameStateManager's warp, ZM_BattleTransition's battle park,
// ZM_UI_MenuStack's dialogue / pause freeze) all write the SAME bare bool,
// ZM_PlayerController::m_bMovementEnabled, which is NOT refcounted; they arbitrate BY
// NAME, in ZM_UI_MenuStack::UnfreezePlayer, which asks whether one of the others now
// owns the player before it re-enables movement. This latch adds exactly one more
// name to that one question, and nothing else. There is no second freeze machinery
// to keep in step, because there is no second freeze machinery.
//
// SAME SHAPE AS ZM_TrainerEngagementLatch above, on purpose: OWNERLESS process-global
// state behind static accessors, which therefore MUST be cleared from the
// between-tests hook in Zenithmon.cpp (convention C3) -- the harness's scene-0
// force-reload cannot reach it.
//
// IT SERIALIZES NOWHERE, and no version number moves for it. A freeze that survived a
// save would restore a player who can never move again; the latch is session state
// about an operation in flight, and an operation in flight does not survive a save.
//
// ★ THERE IS NO REFCOUNT, AND THAT IS THE DESIGN. One End() always releases, whatever
// ran before it, so the release path can never be starved by a Begin nobody paired.
// A re-Begin while active REPLACES the owner instead of stacking a second claim: the
// alternative (a depth counter) turns a single missed End() into a player frozen for
// the rest of the session, which is the standing hazard this class is written around
// -- m_bMovementEnabled is one bool and four owners now write it.
//
// EVERY function is TOTAL and SILENT: an unregistered id (including the
// ZM_TRAINER_NONE sentinel) is inert -- nothing latches, nothing logs, nothing
// asserts. Same rule, same reason, as MarkEngaged above.
class ZM_TrainerCinematicLatch
{
public:
	// Take the freeze for eTrainer. Unregistered ids are ignored outright, so a
	// caller that lost its row cannot arm a freeze nobody can attribute.
	static void Begin(ZM_TRAINER_ID eTrainer);
	// Release it. TOTAL: ending a latch nobody began is legal and inert.
	static void End();
	// The question ZM_UI_MenuStack::UnfreezePlayer asks.
	static bool IsActive();
	// The RAW owner, for the unit that proves an unregistered Begin latched NOBODY:
	// asserting only on IsActive() could not see a stray write parked in the slot,
	// exactly as GetEngagedMaskForTests exists to see a stray bit.
	static ZM_TRAINER_ID GetActiveTrainerForTests();
	static void ResetRuntimeStateForTests();

private:
	static ZM_TRAINER_ID s_eActiveTrainer;
};
