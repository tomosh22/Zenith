#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleState.h"   // ZM_BattleState, ZM_BattleSide, ZM_BattleMonster
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"   // ZM_SIDE, ZM_BattleAction, ZM_ACTION_KIND
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"       // ZM_BattleRNG

// ============================================================================
// ZM_BattleAI -- opponent action chooser (S2 box 5). A PURE, DETERMINISTIC
// function of (state, side, tier, ai-rng). Four difficulty tiers. Never touches
// the battle RNG (xState.m_xRNG), never mutates the battle, never emits events:
// choosing an action leaves every box-1..4 golden byte-identical (ZM-D-032/033).
// Models SINGLES only (Scope.md): the opposing "reply" is the one opposing
// active. All tiers share one legal-action enumeration and one deterministic
// expected-damage scoring primitive (mean roll, no crit, type/STAB included --
// it does NOT call the stochastic ZM_MoveExecutor damage path); they differ only
// in the decision policy layered on top. Only RANDOM draws any randomness, and
// only from the caller-owned xAIRng.
// ============================================================================

// Difficulty tiers. SCREAMING_SNAKE with the ZM_AI_TIER_ prefix (matches the
// reserved ZM_BattleConfig comment: "ZM_AI_TIER m_aeSideAI[ZM_SIDE_COUNT]").
enum ZM_AI_TIER : u_int
{
	ZM_AI_TIER_RANDOM,     // uniform over the legal action set
	ZM_AI_TIER_GREEDY,     // max deterministic expected-damage x accuracy this turn
	ZM_AI_TIER_SMART,      // KO -> switch-out-of-hopeless -> heal-when-low -> GREEDY
	ZM_AI_TIER_CHAMPION,   // 2-ply: own move + one modeled GREEDY opponent reply

	ZM_AI_TIER_COUNT,
	ZM_AI_TIER_NONE = ZM_AI_TIER_COUNT   // "no AI" sentinel (human-controlled side)
};

// Choose one action for eSide's ACTIVE monster in xState, at the given tier.
// xAIRng is the AI's OWN generator, DISTINCT from xState.m_xRNG -- calling this
// never advances the battle's RNG stream (non-perturbation contract). Returns a
// ZM_ACTION_MOVE (m_uMoveSlot set) or ZM_ACTION_SWITCH (m_uSwitchSlot set). Never
// returns ITEM or RUN (those are player wild-battle actions; an AI opponent has
// neither -- Scope.md singles, SubmitAction legality).
ZM_BattleAction ZM_ChooseAction(const ZM_BattleState& xState, ZM_SIDE eSide,
	ZM_AI_TIER eTier, ZM_BattleRNG& xAIRng);
