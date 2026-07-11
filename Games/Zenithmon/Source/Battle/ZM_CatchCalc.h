#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"   // ZM_BattleMonster, ZM_MAJOR_STATUS, ZM_STAT_HP
#include "Zenithmon/Source/Data/ZM_ItemData.h"          // ZM_ITEM_ID, ZM_ITEM_CATEGORY
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"       // ZM_RARITY
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"

// ============================================================================
// ZM_CatchCalc -- the Gen-III/IV four-shake capture calculation plus the paired
// wild-escape odds (S2 box 2, SC6; DecisionLog ZM-D-033 Q2 + ZM-D-035). Pure
// integer math, seeded through ZM_BattleRNG so a catch / flee replays bit-exact.
// The engine's pre-move ITEM / RUN handlers call these; the calc owns no state.
//
// Base catch rate is DERIVED from the species ZM_RARITY (NO new S1 data column --
// ZM-D-033 Q2): COMMON 190 / UNCOMMON 120 / RARE 45 / LEGENDARY 3. Status bonus
// (Gen-IV, Q2): sleep / freeze x5/2, paralysis / poison / toxic / burn x3/2, none
// x1. The ball bonus is the item row's catch param (x10, ZM_ItemData); PRIMEORB's
// param 255 is the guaranteed-capture sentinel (the master-ball analog).
// Conditional ball bonuses (net / dusk / quick) are deferred -- Shortfalls.md 1.2.
//
// Modified capture value a (LOCKED, ZM-D-035; integer, no floating point):
//   hpTerm = 3*maxHP - 2*curHP
//   a = hpTerm * baseRate * ballX10 / (3 * maxHP * 10)
//   a = a * statusNum / statusDen         (min 1)
// a >= 255 (or a guaranteed ball) captures with NO shake draws. Otherwise the
// Gen-III/IV integer shake value b = 1048560 / isqrt(isqrt(16711680 / a)) gates
// four RandBelow(65536) < b checks; all four pass == caught (stops at first fail).
// ============================================================================

struct ZM_CatchResult
{
	bool  m_bCaught      = false;
	u_int m_uShakes      = 0u;   // successful wobbles in [0,4]; 4 == caught
	u_int m_uCatchValueA = 0u;   // the modified 'a' (diagnostic / test hook; 255 when guaranteed)
};

namespace ZM_CatchCalc
{
	// Rarity -> base capture rate (ZM-D-033 Q2). 0 for an unknown rarity.
	u_int BaseCatchRate(ZM_RARITY eRarity);

	// Major status -> capture multiplier numerator / denominator (Gen-IV, Q2).
	void  StatusModifier(ZM_MAJOR_STATUS eStatus, u_int& uNum, u_int& uDen);

	// The item row's catch param (x10 multiplier); 0 if eBall is not a BALL item.
	u_int BallCatchParam(ZM_ITEM_ID eBall);

	// A guaranteed-capture ball (PRIMEORB / any param >= 255): caught with no draws.
	bool  IsGuaranteedBall(ZM_ITEM_ID eBall);

	// The pure arithmetic modified capture value 'a' from raw terms (unit-testable
	// with explicit numbers). Clamped to a minimum of 1; maxHP 0 is guarded to 1.
	u_int ModifiedCatchValue(u_int uMaxHP, u_int uCurHP, u_int uBaseRate,
		u_int uBallX10, u_int uStatusNum, u_int uStatusDen);

	// The integer Gen-III/IV shake gate b. Returns 65536 (the "always passes"
	// sentinel) for a >= 255; otherwise b in [0,65535].
	u_int ShakeProbability(u_int uCatchValueA);

	// The full capture roll for a wild target with a ball, drawing shakes from
	// xRNG. A guaranteed capture (ball sentinel or a >= 255) draws nothing; a
	// computed roll draws one RandBelow(65536) per shake and stops at the first
	// failure. m_uCatchValueA reports the 'a' used (255 for a guaranteed capture).
	ZM_CatchResult Roll(const ZM_BattleMonster& xTarget, ZM_ITEM_ID eBall, ZM_BattleRNG& xRNG);
}

// Wild-escape odds (Gen-III/IV), kept beside catch as the paired wild-battle
// "leave the battle" math. selfSpeed >= oppSpeed (or oppSpeed 0) is a guaranteed
// escape (no draw). Otherwise f = (selfSpeed*128 / oppSpeed + 30*attempt) mod 256
// gates one RandBelow(256) < f check. uAttempt is 1-based (the Nth run this battle).
struct ZM_FleeOdds
{
	bool  m_bGuaranteed = false;
	u_int m_uThreshold  = 0u;   // f in [0,255]; 0 when guaranteed
};

ZM_FleeOdds ZM_ComputeFleeOdds(u_int uSelfSpeed, u_int uOppSpeed, u_int uAttempt);
bool        ZM_RollFlee(u_int uSelfSpeed, u_int uOppSpeed, u_int uAttempt, ZM_BattleRNG& xRNG);
