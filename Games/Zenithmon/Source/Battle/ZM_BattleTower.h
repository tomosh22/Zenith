#pragma once

#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"   // ZM_BattleMonsterSpec
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"      // ZM_BattleConfig
#include "Zenithmon/Source/Battle/ZM_BattleAI.h"         // ZM_AI_TIER
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"         // ZM_RARITY (rarity ceiling)
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"           // ZM_BattleRNG

// ============================================================================
// ZM_BattleTower -- S2 box 6 SC2. The PURE, DETERMINISTIC, HEADLESS logic of a
// Battle-Tower facility: a level-50 flat clamp, procedural-by-seed opponent /
// rental teams drawn from the existing dex, a streak -> difficulty schedule, and
// a streak -> ZM_AI_TIER escalation with a boss every 7th battle. No globals, no
// UI, no scene (the tower SCENE is S11). RNG is the ONLY randomness source (a
// CALLER-owned ZM_BattleRNG, never the battle RNG) -- so a fixed seed reproduces
// every team bit-for-bit. This box PRODUCES setup (clamped teams + AI tier +
// config) and ADVANCES a streak from a battle RESULT; it never runs a battle.
// The caller (S5 battle integration / S11 tower scene) drives ZM_BattleEngine and
// feeds the opponent side through ZM_ChooseAction at the tier returned here.
// ============================================================================

// ---- tunable constants (S11-tunable; the GDD fixes only the boss period) ----
static const u_int uZM_TOWER_LEVEL           = 50u;   // flat clamp level for every tower battle
static const u_int uZM_TOWER_TEAM_SIZE       = 3u;    // singles squad size (doubles is cut, Scope)
static const u_int uZM_TOWER_BOSS_PERIOD     = 7u;    // every 7th battle is a boss (GDD 7.1)
static const u_int uZM_TOWER_STREAK_GREEDY   = 7u;    // streak >= this -> GREEDY base tier
static const u_int uZM_TOWER_STREAK_SMART    = 21u;   // streak >= this -> SMART  base tier
static const u_int uZM_TOWER_STREAK_CHAMPION = 35u;   // streak >= this -> CHAMPION base tier

// ---- persistent run state (POD; the S7 ZM_SaveSchema will serialize it) ----
// m_uCurrentStreak == consecutive wins so far == the 0-based index of the
// upcoming battle (the (m_uCurrentStreak+1)-th battle of the run).
struct ZM_TowerRun
{
	u_int   m_uCurrentStreak = 0u;   // reset to 0 on a loss
	u_int   m_uBestStreak    = 0u;   // running max; survives losses / new runs
	u_int64 m_ulSeed         = 0ull; // run seed; per-battle seeds derive from (seed, streak)
};

// ---- level-50 clamp ("Level 50 flat": >50 clamps down, <50 scales up) ----
// Returns a COPY of xSpec with m_uLevel = uZM_TOWER_LEVEL and m_uCurExp =
// uZM_EXP_UNSPECIFIED. Species / IVs / EVs / nature / ability / moves and any
// base-stat override are preserved verbatim; ZM_BuildBattleMonster then recomputes
// all six stats at level 50 and starts the built mon at FULL HP. Total function:
// the input m_uLevel is ignored (the original is never built), so any out-of-range
// input level is normalized to 50 without tripping the build-time assert.
ZM_BattleMonsterSpec ZM_ClampSpecToTowerLevel(const ZM_BattleMonsterSpec& xSpec);

// Clamp uCount specs, preserving order, into paxOut (caller-sized to >= uCount).
void ZM_ClampPartyToTowerLevel(const ZM_BattleMonsterSpec* paxIn, u_int uCount,
                               ZM_BattleMonsterSpec* paxOut);

// ---- streak scaling + AI escalation ----
// True iff the upcoming battle (1-indexed n = uStreak+1) is a boss: n % 7 == 0.
bool       ZM_TowerIsBossBattle(u_int uStreak);

// Base difficulty tier from the streak band (before the boss bump):
//   [0, 7)  -> RANDOM   [7, 21) -> GREEDY   [21, 35) -> SMART   [35, inf) -> CHAMPION.
ZM_AI_TIER ZM_TowerBaseTierForStreak(u_int uStreak);

// The opponent AI tier for the upcoming battle: the base band tier, bumped one
// tier UP (capped at ZM_AI_TIER_CHAMPION) on a boss battle. Always in
// [ZM_AI_TIER_RANDOM, ZM_AI_TIER_CHAMPION]. NOTE: the BASE tier
// (ZM_TowerBaseTierForStreak) is monotonic non-decreasing, but this bumped AI
// tier is NOT globally monotonic -- it dips back to the base tier after each
// interior boss (e.g. streak 13 -> SMART, 14 -> GREEDY). Callers must not rely
// on monotonicity of the returned tier across consecutive streaks.
ZM_AI_TIER ZM_TowerAITierForStreak(u_int uStreak);

// ---- deterministic per-battle seed ----
// A distinct, reproducible seed for the battle at uStreak within xRun (mixes the
// run seed with the streak index via a Weyl step). Pure. Seed a local
// ZM_BattleRNG with this before calling ZM_GenerateTowerTeam / ZM_BattleEngine.
u_int64    ZM_TowerBattleSeed(const ZM_TowerRun& xRun, u_int uStreak);

// ---- opponent / rental team generation (procedural-by-seed from the dex) ----
// Maximum species rarity eligible at uStreak (rises with the band, never
// LEGENDARY):  [0, 7) -> COMMON,  [7, 21) -> UNCOMMON,  [21, inf) -> RARE.
ZM_RARITY  ZM_TowerMaxRarityForStreak(u_int uStreak);

// Fill paxOut[0..uCount) with a legal tower team: uCount DISTINCT species
// rejection-sampled from xRng over the dex filtered to
// (rarity <= ZM_TowerMaxRarityForStreak(uStreak) AND rarity != LEGENDARY), each
// produced as a clamped L50 spec -- IVs 31, EVs 0, ability NONE, nature =
// xRng.RandBelow(ZM_NATURE_COUNT), moves = the up-to-four highest-level learnset
// moves learnable at/below L50. Draw order pinned in spec section 7. Deterministic
// in (uStreak, xRng). Serves BOTH the opponent team and a player rental set (the
// caller seeds a distinct stream for each). Asserts uCount <= uZM_TOWER_TEAM_SIZE
// and that the eligible pool has >= uCount species.
void ZM_GenerateTowerTeam(u_int uStreak, ZM_BattleRNG& xRng,
                          ZM_BattleMonsterSpec* paxOut, u_int uCount = uZM_TOWER_TEAM_SIZE);

// ---- streak settlement ----
// Advance xRun from a battle result: win -> ++m_uCurrentStreak and
// m_uBestStreak = max(m_uBestStreak, m_uCurrentStreak); loss -> m_uCurrentStreak = 0
// (best preserved). Returns the new m_uCurrentStreak.
u_int ZM_TowerAdvance(ZM_TowerRun& xRun, bool bPlayerWon);

// ---- battle-config helper ----
// A ready ZM_BattleConfig for a tower battle: m_uLevelCap = 50, m_bIsWild = false,
// m_bCanCatch = false, m_bCanFlee = false, m_bIsTrainerBattle = true,
// m_bAwardExp = false (a flat-50 facility grants no levels). The level cap is
// belt-and-braces: with awards off no exp is credited anyway, but if a later
// variant enables awards the cap keeps leveling pinned to 50.
ZM_BattleConfig ZM_MakeTowerBattleConfig();
