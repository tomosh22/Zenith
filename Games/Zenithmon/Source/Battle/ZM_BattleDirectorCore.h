#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleEngine.h"   // ZM_BattleEngine (+ state/event/monster/types/rng)
#include "Zenithmon/Source/Battle/ZM_BattleAI.h"        // ZM_AI_TIER, ZM_ChooseAction
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"       // ZM_SPECIES_ID

// ============================================================================
// ZM_BattleDirectorCore -- the pure, headless heart of the battle presenter (S5
// item 4). It deep-owns a ZM_BattleEngine, drives it turn by turn, and
// translates the engine's append-only ZM_BattleEvent stream into TIMED
// presentation ops that a later ECS/UI layer (SC3/SC4) renders. It carries NO
// ECS, NO graphics, NO scene -- just the engine + a state machine + an event
// cursor + the event->op mapping + a zm_instant_battles flag that collapses all
// timing (for deterministic tests). The battle logic already exists and is fully
// tested; this is the driver only. ZM-D-101.
//
// AI non-perturbation (ZM-D-032/033): the director drives the enemy side through
// ZM_ChooseAction with its OWN ZM_BattleRNG (seeded from ZM_DeriveAiRngSeed of the
// battle seed), so choosing never advances the battle RNG stream and a director
// drive is byte-identical to a hand drive of the raw engine with the same picks.
// ============================================================================

// The four states of the presentation state machine.
enum ZM_DIRECTOR_STATE : u_int
{
	ZM_DIRECTOR_NOT_STARTED,    // before Begin()
	ZM_DIRECTOR_AWAIT_INPUT,    // waiting for the player's action (singles: always the player)
	ZM_DIRECTOR_PLAYING_EVENTS, // draining the current turn's (or intro's) event ops
	ZM_DIRECTOR_OVER            // battle resolved; all ops drained
};

// The presentation kinds a mapped event resolves to. A later ECS/UI layer renders
// each op; the director itself only times them.
enum ZM_PRESENTATION_OP : u_int
{
	ZM_POP_NONE,         // framing event, nothing to present
	ZM_POP_TEXT,         // a text line only
	ZM_POP_ANIM,         // an attack/move animation (+ usually text)
	ZM_POP_HP_TWEEN,     // an HP bar tween (+ maybe text)
	ZM_POP_MODEL_REVEAL, // a creature entering (switch-in)
	ZM_POP_FAINT_FALL,   // a faint animation (+ text)
	ZM_POP_BALL,         // a ball arc / wobble / result
	ZM_POP_EXP_TWEEN,    // an exp-bar tween (+ text)
	ZM_POP_COUNT
};

// One presentation op: what to present, how long it takes (wall-clock, collapsed
// to 0 under zm_instant_battles), and whether it puts a line in the battle log.
struct ZM_BattlePresentationOp
{
	ZM_PRESENTATION_OP m_eOp                 = ZM_POP_NONE;
	float              m_fBaseDurationSeconds = 0.0f;   // wall-clock seconds when NOT instant; 0 collapses under zm_instant_battles
	bool               m_bCarriesText         = false;  // does this op put a line in the battle log?
};

// --- global zm_instant_battles flag (ALL configs; the tools DebugVariable binds
//     the ref) -- collapses every op duration to 0 so a whole turn drains in a
//     single Tick. Deterministic tests flip it on. ---
bool  ZM_InstantBattlesEnabled();
void  ZM_SetInstantBattlesForTests(bool bEnabled);
bool& ZM_InstantBattlesRef();   // backing store, for the tools-only DebugVariable AddBoolean

// The total, pure event->op mapping. Defined for EVERY ZM_BATTLE_EVENT kind (no
// kind is dropped); an out-of-range value falls through to {ZM_POP_NONE,0,false}.
ZM_BattlePresentationOp ZM_MapEventToOp(const ZM_BattleEvent& xEvent);

// A wild-enemy spec: species + level, moves = the up-to-four highest-level
// learnset moves learnable at/below uLevel (learn order; keep the LAST four when
// more). IVs 31 / EVs 0 / nature FERAL / ability NONE. Mirrors the shipped tower
// helper g_FillTowerMoves but against uLevel (not the L50 clamp).
ZM_BattleMonsterSpec ZM_BuildWildEnemySpec(ZM_SPECIES_ID eSpecies, u_int uLevel);

// Deterministic AI-rng seed derived from the battle seed (a fixed salt so the AI
// stream is DISTINCT from the battle stream and their draws never coincide).
// Exposed so a test can reproduce the enemy picks. Pure.
u_int64 ZM_DeriveAiRngSeed(u_int64 ulBattleSeed);

class ZM_BattleDirectorCore
{
public:
	ZM_BattleDirectorCore() = default;

	// Begin a battle: build the engine, seed the AI rng from ZM_DeriveAiRngSeed(ulSeed),
	// record the enemy tier, and enter PLAYING_EVENTS to present the intro
	// (BATTLE_BEGIN + both SWITCH_INs). The cursor starts at 0, the turn-end at the
	// current event count.
	void Begin(const ZM_BattleMonsterSpec* paxPlayer, u_int uPlayerCount,
	           const ZM_BattleMonsterSpec* paxEnemy, u_int uEnemyCount,
	           const ZM_BattleConfig& xConfig, u_int64 ulSeed, ZM_AI_TIER eEnemyTier);

	// Valid ONLY in AWAIT_INPUT (asserts). Picks the enemy action via ZM_ChooseAction
	// (own rng), submits both sides, resolves the turn, snapshots the new event range
	// [prevCount, newCount), and enters PLAYING_EVENTS.
	void SubmitPlayerAction(const ZM_BattleAction& xPlayerAction);

	// Advance presentation. In PLAYING_EVENTS, drains ops as wall-clock time accrues;
	// under zm_instant_battles every op is 0-duration, so a single Tick drains the whole
	// range. When the range is exhausted -> OVER if the battle ended, else AWAIT_INPUT.
	void Tick(float fDeltaSeconds);

	ZM_DIRECTOR_STATE GetState() const { return m_eState; }
	bool IsAwaitingInput() const { return m_eState == ZM_DIRECTOR_AWAIT_INPUT; }
	bool IsOver() const { return m_xEngine.IsOver(); }
	ZM_SIDE GetWinner() const { return m_xEngine.GetWinnerSide(); }
	// Latches true exactly once the machine reaches OVER with all resolution ops drained.
	bool ShouldRequestEnd() const { return m_eState == ZM_DIRECTOR_OVER; }

	// The op for the event currently being presented, or {ZM_POP_NONE,...} when not
	// in PLAYING_EVENTS or the cursor is past the range end.
	ZM_BattlePresentationOp CurrentOp() const;
	// &the event currently being presented, or nullptr.
	const ZM_BattleEvent* CurrentEvent() const;

	// The count of events PRESENTED so far -- index one past the last-presented event,
	// INCLUDING the op currently being presented in PLAYING_EVENTS. The HUD scans
	// [0, PresentedEventCount()) for the latest text-carrying line, so it shows the
	// current line while paced (timed) AND the final line after an instant drain (where
	// a single Tick empties the range, so CurrentEvent() is already null when sampled).
	u_int PresentedEventCount() const
	{
		return (m_eState == ZM_DIRECTOR_PLAYING_EVENTS && m_uCursor < m_uTurnEndCursor)
			? m_uCursor + 1u
			: m_uCursor;
	}

	// Read-through accessors for SC3/SC4 (HP bars, name/level panels).
	u_int SideActiveHP(ZM_SIDE eSide) const;
	u_int SideActiveMaxHP(ZM_SIDE eSide) const;
	u_int SideActiveLevel(ZM_SIDE eSide) const;
	ZM_SPECIES_ID SideActiveSpecies(ZM_SIDE eSide) const;

	const ZM_BattleEngine& GetEngine() const { return m_xEngine; }

private:
	ZM_BattleEngine   m_xEngine;
	ZM_BattleRNG      m_xAIRng;                   // AI's OWN generator (distinct from the battle RNG)
	ZM_AI_TIER        m_eEnemyTier = ZM_AI_TIER_GREEDY;
	ZM_DIRECTOR_STATE m_eState = ZM_DIRECTOR_NOT_STARTED;
	u_int             m_uCursor        = 0u;      // index of the event being presented
	u_int             m_uTurnEndCursor = 0u;      // one past the last event of the current range
	float             m_fOpElapsed     = 0.0f;    // seconds spent on the current op
};
