#include "Zenith.h"

// ============================================================================
// ZM_Tests_BattleAI -- S2 box-5 opponent-AI action-chooser tests (suite
// ZM_Battle, so they run in the boot unit gate alongside the other battle
// units). ZM_ChooseAction is a PURE, DETERMINISTIC function of
// (const ZM_BattleState&, side, tier, ZM_BattleRNG&): it reads state, scores
// candidate actions with a deterministic mean-roll / no-crit expected-damage
// model, and returns one ZM_ACTION_MOVE or ZM_ACTION_SWITCH. It never mutates
// the battle, never draws the battle RNG (xState.m_xRNG), never emits events.
//
// These are PROPERTY-STYLE tests over hand-constructed states (NOT win-rate
// simulations, per Docs/TestPlan.md). Every fixture is deterministic and
// hermetic (no baked assets), so no RequestSkip is needed. Monsters are built
// with MakeSpecOverride so base stats are pencil-verifiable and the expected
// pick survives a base-stat re-tune. The expected-damage arithmetic below is
// hand-computed against the LOCKED ZM_CalcDamage pipeline; where a test could be
// sensitive to flooring, a local oracle (DetDamage/HitPercent/GreedyScore, built
// on the REAL ZM_CalcDamage / ZM_EffectivenessPercent / ZM_ApplyStatStage) is
// asserted as a construction precondition so a mis-calibrated fixture fails
// loudly instead of pinning a wrong slot.
//
// Design authority: scratchpad/zm_battleai_spec.md (S2 box 5). Tiers:
//   RANDOM   -- uniform over the legal action set (moves AND switches)
//   GREEDY   -- max deterministic expected-damage x accuracy this turn
//   SMART    -- KO -> switch-out-of-hopeless -> heal-when-low -> GREEDY
//   CHAMPION -- 2-ply: own move + one modeled GREEDY opponent reply
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Battle/ZM_BattleAI.h"
#include "Zenithmon/Source/Battle/ZM_BattleState.h"
#include "Zenithmon/Source/Battle/ZM_BattleMonster.h"
#include "Zenithmon/Source/Battle/ZM_BattleTypes.h"
#include "Zenithmon/Source/Battle/ZM_DamageCalc.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_NatureData.h"
#include "Zenithmon/Source/Data/ZM_Types.h"
#include "Zenithmon/Source/Data/ZM_BattleRNG.h"
#include "Collections/Zenith_Vector.h"

// ============================================================================
// Test-local helpers (anonymous namespace: no cross-TU ODR clashes -- the
// ZM_Tests_Battle.cpp builders live in ITS own anonymous namespace and are not
// visible here, so this TU carries its own minimal equivalents in the same
// style).
// ============================================================================
namespace
{
	// --- spec builders (mirrors ZM_Tests_Battle.cpp 347-402) ---------------
	ZM_BattleMonsterSpec MakeSpec(ZM_SPECIES_ID eSpecies, u_int uLevel, ZM_MOVE_ID eMove0)
	{
		ZM_BattleMonsterSpec xSpec;
		xSpec.m_eSpecies = eSpecies;
		xSpec.m_uLevel = uLevel;
		for (u_int i = 0; i < ZM_STAT_COUNT; ++i) { xSpec.m_auIV[i] = 31u; xSpec.m_auEV[i] = 0u; }
		xSpec.m_eNature = ZM_NATURE_FERAL;
		xSpec.m_eAbility = ZM_ABILITY_NONE;
		xSpec.m_aeMoves[0] = eMove0;
		xSpec.m_aeMoves[1] = ZM_MOVE_NONE;
		xSpec.m_aeMoves[2] = ZM_MOVE_NONE;
		xSpec.m_aeMoves[3] = ZM_MOVE_NONE;
		xSpec.m_bOverrideBaseStats = false;
		return xSpec;
	}

	// Same, with an explicit base-stat override (6 values in ZM_STAT order:
	// HP, ATK, DEF, SPA, SPD, SPE) so damage numbers are pencil-verifiable.
	ZM_BattleMonsterSpec MakeSpecOverride(ZM_SPECIES_ID eSpecies, u_int uLevel, ZM_MOVE_ID eMove0,
		u_int uHP, u_int uATK, u_int uDEF, u_int uSPA, u_int uSPD, u_int uSPE)
	{
		ZM_BattleMonsterSpec xSpec = MakeSpec(eSpecies, uLevel, eMove0);
		xSpec.m_bOverrideBaseStats = true;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_HP]        = uHP;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_ATTACK]    = uATK;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_DEFENSE]   = uDEF;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPATTACK]  = uSPA;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPDEFENSE] = uSPD;
		xSpec.m_xBaseStatsOverride.m_au[ZM_STAT_SPEED]     = uSPE;
		return xSpec;
	}

	// Populate a state with a bench (mirrors ZM_Tests_Battle.cpp BuildBattleState
	// but allows >1 mon per side). Both actives = party slot 0; RNG seeded.
	void BuildAIState(ZM_BattleState& xState,
		const ZM_BattleMonsterSpec* paxPlayer, u_int uPC,
		const ZM_BattleMonsterSpec* paxEnemy,  u_int uEC, u_int64 ulSeed = 0x1234ull)
	{
		ZM_BattleSide& xP = xState.Side(ZM_SIDE_PLAYER);
		ZM_BattleSide& xE = xState.Side(ZM_SIDE_ENEMY);
		xP.m_xParty.Clear();
		xE.m_xParty.Clear();
		for (u_int i = 0; i < uPC; ++i) { xP.m_xParty.PushBack(ZM_BuildBattleMonster(paxPlayer[i])); }
		for (u_int i = 0; i < uEC; ++i) { xE.m_xParty.PushBack(ZM_BuildBattleMonster(paxEnemy[i])); }
		xP.m_uActiveSlot = 0u;
		xE.m_uActiveSlot = 0u;
		xState.m_xField = ZM_FieldState();
		xState.m_xRNG.Seed(ulSeed, 54ull);
	}

	// ---- deterministic scoring oracle (built on the REAL engine functions so
	//      it is a faithful reference, not a re-derivation of the arithmetic) --

	// Deterministic damage of move slot uSlot from xAtk onto xDef at a fixed roll,
	// no crit -- mirrors ZM_MoveExecutor::ApplyDamagingHit's ZM_DamageInput build
	// (ZM_Tests_Battle.cpp 1131-1144). 0 for STATUS / zero-power / type-immune.
	u_int DetDamage(const ZM_BattleMonster& xAtk, const ZM_BattleMonster& xDef, u_int uSlot, u_int uRoll)
	{
		const ZM_MoveData& xMove = ZM_GetMoveData(xAtk.m_axMoves[uSlot].m_eMove);
		if (xMove.m_eCategory == ZM_MOVE_CATEGORY_STATUS || xMove.m_uPower == 0u) { return 0u; }
		const bool bPhysical = (xMove.m_eCategory == ZM_MOVE_CATEGORY_PHYSICAL);
		const ZM_SpeciesData& xAtkSp = ZM_GetSpeciesData(xAtk.m_eSpecies);
		const ZM_SpeciesData& xDefSp = ZM_GetSpeciesData(xDef.m_eSpecies);

		ZM_DamageInput xIn;
		xIn.uLevel  = xAtk.m_uLevel;
		xIn.uPower  = xMove.m_uPower;
		xIn.uAttack = ZM_ApplyStatStage(
			bPhysical ? xAtk.m_auMaxStat[ZM_STAT_ATTACK] : xAtk.m_auMaxStat[ZM_STAT_SPATTACK],
			bPhysical ? xAtk.m_aiStage[ZM_BATTLE_STAT_ATTACK] : xAtk.m_aiStage[ZM_BATTLE_STAT_SPATTACK]);
		xIn.uDefense = ZM_ApplyStatStage(
			bPhysical ? xDef.m_auMaxStat[ZM_STAT_DEFENSE] : xDef.m_auMaxStat[ZM_STAT_SPDEFENSE],
			bPhysical ? xDef.m_aiStage[ZM_BATTLE_STAT_DEFENSE] : xDef.m_aiStage[ZM_BATTLE_STAT_SPDEFENSE]);
		xIn.bStab = (xMove.m_eType == xAtkSp.m_aeTypes[0] || xMove.m_eType == xAtkSp.m_aeTypes[1]);
		xIn.uEffectivenessPercent = ZM_EffectivenessPercent(xMove.m_eType, xDefSp.m_aeTypes[0], xDefSp.m_aeTypes[1]);
		xIn.bCrit = false;
		xIn.uRandomPercent = uRoll;
		return ZM_CalcDamage(xIn);
	}

	// Hit probability (percent) -- mirrors the M5 accuracy fold. At box-1..5
	// stage 0 this is just base m_uAccuracy (or 100 for never-miss moves).
	u_int HitPercent(const ZM_BattleMonster& xAtk, const ZM_BattleMonster& xDef, const ZM_MoveData& xMove)
	{
		if (xMove.m_uAccuracy == uZM_MOVE_ACCURACY_ALWAYS_HITS) { return 100u; }
		int iAccStage = xAtk.m_aiStage[ZM_BATTLE_STAT_ACCURACY] - xDef.m_aiStage[ZM_BATTLE_STAT_EVASION];
		if (iAccStage < iZM_MIN_STAGE) { iAccStage = iZM_MIN_STAGE; }
		if (iAccStage > iZM_MAX_STAGE) { iAccStage = iZM_MAX_STAGE; }
		const u_int uEff = ZM_ApplyStatStage(xMove.m_uAccuracy, iAccStage);
		return (uEff > 100u) ? 100u : uEff;
	}

	// GREEDY score of a MOVE slot = deterministic damage (roll 92) x hit percent.
	u_int GreedyScore(const ZM_BattleMonster& xAtk, const ZM_BattleMonster& xDef, u_int uSlot)
	{
		const ZM_MoveData& xMove = ZM_GetMoveData(xAtk.m_axMoves[uSlot].m_eMove);
		return DetDamage(xAtk, xDef, uSlot, 92u) * HitPercent(xAtk, xDef, xMove);
	}

	bool IsLegalMoveSlot(const ZM_BattleMonster& xMon, u_int uSlot)
	{
		return uSlot < uZM_MAX_MOVES
			&& xMon.m_axMoves[uSlot].m_eMove != ZM_MOVE_NONE
			&& xMon.m_axMoves[uSlot].m_uCurPP > 0u;
	}

	// GREEDY argmax over legal MOVE slots (tie-break lowest slot). Returns
	// uZM_MAX_MOVES if there is no legal move. Used only as a construction oracle.
	u_int GreedyArgmaxMove(const ZM_BattleState& xState, ZM_SIDE eSide)
	{
		const ZM_SIDE eOther = (eSide == ZM_SIDE_PLAYER) ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
		const ZM_BattleMonster& xMe  = xState.Side(eSide).Active();
		const ZM_BattleMonster& xOpp = xState.Side(eOther).Active();
		u_int uBest = uZM_MAX_MOVES;
		u_int uBestScore = 0u;
		for (u_int s = 0; s < uZM_MAX_MOVES; ++s)
		{
			if (!IsLegalMoveSlot(xMe, s)) { continue; }
			const u_int uScore = GreedyScore(xMe, xOpp, s);
			if (uBest == uZM_MAX_MOVES || uScore > uBestScore)
			{
				uBest = s;
				uBestScore = uScore;
			}
		}
		return uBest;
	}

	// Is xAction a legal action for eSide in xState (per section 3 enumeration)?
	bool IsLegalAction(const ZM_BattleState& xState, ZM_SIDE eSide, const ZM_BattleAction& xAction)
	{
		const ZM_BattleSide& xMe = xState.Side(eSide);
		if (xAction.m_eKind == ZM_ACTION_MOVE)
		{
			return IsLegalMoveSlot(xMe.Active(), xAction.m_uMoveSlot);
		}
		if (xAction.m_eKind == ZM_ACTION_SWITCH)
		{
			return xMe.CanSwitchTo(xAction.m_uSwitchSlot);
		}
		return false;   // ITEM / RUN / NONE are never legal AI actions
	}

	// Convenience accessors (non-const state -> mutable actives for HP/PP rigging).
	ZM_BattleMonster& PlayerActive(ZM_BattleState& xState) { return xState.Side(ZM_SIDE_PLAYER).Active(); }
	ZM_BattleMonster& EnemyActive(ZM_BattleState& xState)  { return xState.Side(ZM_SIDE_ENEMY).Active(); }

	// Common override profile: symmetric ATK/SPA and DEF/SPD so PHYSICAL and
	// SPECIAL moves compare on equal footing; only type / STAB / power / accuracy
	// then differ. (HP=100, ATK=SPA=150, DEF=SPD=50, SPE=100.)
	//   L50 derived stats: ATK/SPA = 170, DEF/SPD = 70, HP = 175, SPE = 120.
	ZM_BattleMonsterSpec MakeNeutralAtk(ZM_SPECIES_ID eSpecies, ZM_MOVE_ID eMove0)
	{
		return MakeSpecOverride(eSpecies, 50u, eMove0, 100u, 150u, 50u, 150u, 50u, 100u);
	}
	ZM_BattleMonsterSpec MakeNeutralDef(ZM_SPECIES_ID eSpecies, ZM_MOVE_ID eMove0)
	{
		return MakeSpecOverride(eSpecies, 50u, eMove0, 100u, 150u, 50u, 150u, 50u, 100u);
	}
}

// ============================================================================
// RANDOM (4) -- uniform over the ordered legal action set; the only tier that
// draws from xAIRng (RandBelow(n), once per call).
// ============================================================================

// Two legal moves (slots 0,1), no bench: RANDOM only ever returns those two,
// reaches both, and is ~uniform; slots 2/3 and any SWITCH/ITEM/RUN never appear.
ZENITH_TEST(ZM_Battle, BattleAI_Random_UniformOverLegalMoves)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;                     // 2 legal moves
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);

	ZM_BattleRNG xAIRng(0xC0FFEEull, 54ull);   // fixed seed -> reproducible
	const int N = 10000;
	int aiMoveCount[uZM_MAX_MOVES] = { 0, 0, 0, 0 };
	int iIllegal = 0;
	for (int i = 0; i < N; ++i)
	{
		const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_RANDOM, xAIRng);
		if (xA.m_eKind == ZM_ACTION_MOVE && xA.m_uMoveSlot < uZM_MAX_MOVES) { aiMoveCount[xA.m_uMoveSlot]++; }
		else { iIllegal++; }
	}

	ZENITH_ASSERT_EQ(iIllegal, 0, "RANDOM returned a non-MOVE / out-of-range action");
	ZENITH_ASSERT_EQ(aiMoveCount[2], 0, "slot 2 (NONE) must never be returned");
	ZENITH_ASSERT_EQ(aiMoveCount[3], 0, "slot 3 (NONE) must never be returned");
	ZENITH_ASSERT_GT(aiMoveCount[0], 0, "RANDOM never reached legal move slot 0");
	ZENITH_ASSERT_GT(aiMoveCount[1], 0, "RANDOM never reached legal move slot 1");
	// ~N/2 each; the +/-6% band (+/-600) is ~12 sigma for N=10000, p=0.5
	// (sigma = sqrt(10000*0.25) = 50), so an unbiased RandBelow lands in-band.
	ZENITH_ASSERT_GE(aiMoveCount[0], 4400);
	ZENITH_ASSERT_LE(aiMoveCount[0], 5600);
	ZENITH_ASSERT_GE(aiMoveCount[1], 4400);
	ZENITH_ASSERT_LE(aiMoveCount[1], 5600);
}

// One move + one living bench member: the legal set is {MOVE0, SWITCH1};
// RANDOM reaches both ~50/50 and returns nothing else (switch IS in the set).
ZENITH_TEST(ZM_Battle, BattleAI_Random_IncludesSwitch)
{
	ZM_BattleMonsterSpec axP[2];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);       // 1 move
	axP[1] = MakeNeutralAtk(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);    // living bench
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_FINLET, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 2u, axE, 1u);

	ZM_BattleRNG xAIRng(0x5EED01ull, 54ull);
	const int N = 10000;
	int iMove0 = 0, iSwitch1 = 0, iOther = 0;
	for (int i = 0; i < N; ++i)
	{
		const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_RANDOM, xAIRng);
		if (xA.m_eKind == ZM_ACTION_MOVE && xA.m_uMoveSlot == 0u) { iMove0++; }
		else if (xA.m_eKind == ZM_ACTION_SWITCH && xA.m_uSwitchSlot == 1u) { iSwitch1++; }
		else { iOther++; }
	}

	ZENITH_ASSERT_EQ(iOther, 0, "RANDOM returned an action outside {MOVE0, SWITCH1}");
	ZENITH_ASSERT_GT(iMove0, 0, "RANDOM never chose the move");
	ZENITH_ASSERT_GT(iSwitch1, 0, "RANDOM never chose the switch");
	ZENITH_ASSERT_GE(iMove0, 4400);
	ZENITH_ASSERT_LE(iMove0, 5600);
	ZENITH_ASSERT_GE(iSwitch1, 4400);
	ZENITH_ASSERT_LE(iSwitch1, 5600);
}

// Three legal moves + one bench = four legal actions. Over many seeded draws
// all four are reached and the empirical frequencies are ~uniform (~N/4 each).
ZENITH_TEST(ZM_Battle, BattleAI_Random_CoversEveryLegalAction)
{
	ZM_BattleMonsterSpec axP[2];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	axP[0].m_aeMoves[2] = ZM_MOVE_BRUTESLAM;                            // 3 legal moves
	axP[1] = MakeNeutralAtk(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);     // + 1 bench
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_FINLET, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 2u, axE, 1u);

	ZM_BattleRNG xAIRng(0xA11CE0ull, 54ull);
	const int N = 12000;
	int aiMove[uZM_MAX_MOVES] = { 0, 0, 0, 0 };
	int iSwitch1 = 0, iOther = 0;
	for (int i = 0; i < N; ++i)
	{
		const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_RANDOM, xAIRng);
		if (xA.m_eKind == ZM_ACTION_MOVE && xA.m_uMoveSlot < uZM_MAX_MOVES) { aiMove[xA.m_uMoveSlot]++; }
		else if (xA.m_eKind == ZM_ACTION_SWITCH && xA.m_uSwitchSlot == 1u) { iSwitch1++; }
		else { iOther++; }
	}

	ZENITH_ASSERT_EQ(iOther, 0, "RANDOM produced an action outside the 4 legal ones");
	ZENITH_ASSERT_EQ(aiMove[3], 0, "slot 3 (NONE) must never be returned");
	// coverage: every one of the four legal actions appears at least once
	ZENITH_ASSERT_GT(aiMove[0], 0, "move slot 0 never reached");
	ZENITH_ASSERT_GT(aiMove[1], 0, "move slot 1 never reached");
	ZENITH_ASSERT_GT(aiMove[2], 0, "move slot 2 never reached");
	ZENITH_ASSERT_GT(iSwitch1, 0, "switch never reached");
	// ~N/4 each; [0.18N, 0.32N] is a very generous band (>10 sigma).
	const int iLo = 2160;   // 0.18 * 12000
	const int iHi = 3840;   // 0.32 * 12000
	ZENITH_ASSERT_GE(aiMove[0], iLo); ZENITH_ASSERT_LE(aiMove[0], iHi);
	ZENITH_ASSERT_GE(aiMove[1], iLo); ZENITH_ASSERT_LE(aiMove[1], iHi);
	ZENITH_ASSERT_GE(aiMove[2], iLo); ZENITH_ASSERT_LE(aiMove[2], iHi);
	ZENITH_ASSERT_GE(iSwitch1, iLo);  ZENITH_ASSERT_LE(iSwitch1, iHi);
}

// One legal action (single move, no bench): every draw returns that move --
// the degenerate uniform, exercising the RandBelow(1) == 0 path.
ZENITH_TEST(ZM_Battle, BattleAI_Random_SingleLegalActionAlwaysReturnsIt)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);   // exactly 1 move
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);

	ZM_BattleRNG xAIRng(0xD00D42ull, 54ull);
	const int N = 2000;
	int iMove0 = 0, iOther = 0;
	for (int i = 0; i < N; ++i)
	{
		const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_RANDOM, xAIRng);
		if (xA.m_eKind == ZM_ACTION_MOVE && xA.m_uMoveSlot == 0u) { iMove0++; }
		else { iOther++; }
	}
	ZENITH_ASSERT_EQ(iOther, 0, "the only legal action must always be returned");
	ZENITH_ASSERT_EQ(iMove0, N, "single-legal-action state must return that action every draw");
}

// ============================================================================
// GREEDY (6) -- max deterministic expected-damage x accuracy; fully
// deterministic (draws nothing). Tie-break lowest slot.
// ============================================================================

// Two same-type/neutral moves; higher base power wins.
//   RAMBASH pow45 STAB -> dmg 69, score 6900 ; QUICKJAB pow40 STAB -> dmg 60,
//   score 6000. GREEDY returns slot 0.
ZENITH_TEST(ZM_Battle, BattleAI_Greedy_PicksMaxExpectedDamageMove)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);   // NORMAL
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	ZM_BattleRNG xAIRng(1u, 54ull);

	ZENITH_ASSERT_EQ(GreedyArgmaxMove(xState, ZM_SIDE_PLAYER), 0u, "construction: slot 0 should be the greedy pick");
	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE, "GREEDY must return a MOVE");
	ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 0u, "GREEDY must pick the higher-power move (slot 0)");
}

// Super-effective LOWER-power move beats a neutral higher-power move -- proves
// damage includes type effectiveness. Attacker WATER (STAB on neither move).
//   slot0 BRUTESLAM pow80 NORMAL vs GRASS = 100% -> dmg 80, score 8000
//   slot1 FLARELASH pow75 FIRE   vs GRASS = 200% -> dmg 150, score 15000
// GREEDY returns slot 1.
ZENITH_TEST(ZM_Battle, BattleAI_Greedy_TypeEffectivenessCountsInDamage)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_FINLET, ZM_MOVE_BRUTESLAM);   // WATER attacker
	axP[0].m_aeMoves[1] = ZM_MOVE_FLARELASH;                          // FIRE, super vs GRASS
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_FERNFAWN, ZM_MOVE_RAMBASH);   // GRASS defender

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	ZM_BattleRNG xAIRng(2u, 54ull);

	ZENITH_ASSERT_EQ(GreedyArgmaxMove(xState, ZM_SIDE_PLAYER), 1u, "construction: super-effective slot 1 should win");
	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 1u, "GREEDY must pick the super-effective lower-power move (slot 1)");
}

// Accuracy scales the expectation: a 100%-acc move beats a higher-power 80%-acc
// move, then a control flips it. Attacker GRASS (STAB on neither move).
//   Positive: slot0 COMETDASH pow100 acc100 -> 99*100 = 9900
//             slot1 TORRENTCANNON pow110 acc80 -> 109*80 = 8720  => slot 0
//   Control : slot0 RAMBASH pow45 acc100 -> 46*100 = 4600
//             slot1 TORRENTCANNON pow110 acc80 -> 109*80 = 8720  => slot 1
ZENITH_TEST(ZM_Battle, BattleAI_Greedy_AccuracyDiscountsExpectation)
{
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);   // NORMAL: neutral to both moves
	ZM_BattleRNG xAIRng(3u, 54ull);

	// -- positive: accuracy discount makes the higher-power move lose --
	{
		ZM_BattleMonsterSpec axP[1];
		axP[0] = MakeNeutralAtk(ZM_SPECIES_FERNFAWN, ZM_MOVE_COMETDASH);   // GRASS attacker
		axP[0].m_aeMoves[1] = ZM_MOVE_TORRENTCANNON;                        // pow110 acc80
		ZM_BattleState xState;
		BuildAIState(xState, axP, 1u, axE, 1u);

		ZENITH_ASSERT_EQ(GreedyArgmaxMove(xState, ZM_SIDE_PLAYER), 0u, "construction: 100%-acc slot 0 should win");
		const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY, xAIRng);
		ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE);
		ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 0u, "GREEDY must discount the 80%-acc move and pick slot 0");
	}

	// -- control: a big base-power gap survives the discount -> slot 1 wins --
	{
		ZM_BattleMonsterSpec axP[1];
		axP[0] = MakeNeutralAtk(ZM_SPECIES_FERNFAWN, ZM_MOVE_RAMBASH);   // weak slot 0
		axP[0].m_aeMoves[1] = ZM_MOVE_TORRENTCANNON;                      // pow110 acc80
		ZM_BattleState xState;
		BuildAIState(xState, axP, 1u, axE, 1u);

		ZENITH_ASSERT_EQ(GreedyArgmaxMove(xState, ZM_SIDE_PLAYER), 1u, "construction: discounted-but-strong slot 1 should win");
		const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY, xAIRng);
		ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE);
		ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 1u, "control: GREEDY must pick the stronger move despite the discount");
	}
}

// STAB counted: equal base power, attacker's type matches slot 0's move but not
// slot 1's -> the STAB move (slot 0) wins.
//   slot0 RAMBASH  pow45 NORMAL (STAB) -> dmg 69, score 6900
//   slot1 BUBBLESPRAY pow45 WATER (no STAB) -> dmg 46, score 4600
ZENITH_TEST(ZM_Battle, BattleAI_Greedy_StabCounts)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);   // NORMAL
	axP[0].m_aeMoves[1] = ZM_MOVE_BUBBLESPRAY;                      // WATER pow45 (no STAB)
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);   // NORMAL: neutral to both

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	ZM_BattleRNG xAIRng(4u, 54ull);

	ZENITH_ASSERT_EQ(GreedyArgmaxMove(xState, ZM_SIDE_PLAYER), 0u, "construction: the STAB move (slot 0) should win");
	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 0u, "GREEDY must prefer the STAB move (slot 0)");
}

// A STATUS / zero-power move scores 0 and is never chosen over a damaging move.
//   slot0 RAMBASH (damaging) ; slot1 REPOSE (STATUS HEAL_HALF, pow0 -> score 0)
ZENITH_TEST(ZM_Battle, BattleAI_Greedy_ZeroPowerScoresZero)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);
	axP[0].m_aeMoves[1] = ZM_MOVE_REPOSE;                            // status, HEAL_HALF
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	ZM_BattleRNG xAIRng(5u, 54ull);

	ZENITH_ASSERT_EQ(GreedyArgmaxMove(xState, ZM_SIDE_PLAYER), 0u, "construction: the damaging move must outscore the status move");
	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 0u, "GREEDY must never pick the zero-power status move over a damaging move");
}

// Two identical moves (same id) in slots 0 and 1 tie -> lowest slot (0) wins.
ZENITH_TEST(ZM_Battle, BattleAI_Greedy_TieBreakLowestSlot)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);
	axP[0].m_aeMoves[1] = ZM_MOVE_RAMBASH;                           // identical move
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	ZM_BattleRNG xAIRng(6u, 54ull);

	// oracle: identical scores -> argmax resolves to the lowest slot.
	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_EQ(GreedyScore(xMe, xOpp, 0u), GreedyScore(xMe, xOpp, 1u), "construction: the two moves must score equally");

	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 0u, "GREEDY tie-break must pick the lowest slot");
}

// ============================================================================
// SMART (8) -- KO -> switch-out-of-hopeless -> heal-when-low -> GREEDY. Fully
// deterministic (draws nothing).
// ============================================================================

// Takes a GUARANTEED KO (sure-hit, min-roll damage >= opp HP) over a
// higher-GREEDY-score CAN-MISS move -> isolates the KO branch from GREEDY.
//   opp HP = 10. slot0 RAMBASH acc100 roll85 dmg 63 >= 10 -> guaranteed KO.
//   slot1 TORRENTCANNON acc80 (not sure-hit) -> excluded from KO candidates,
//   yet its GREEDY score (109*80 = 8720) beats RAMBASH's (69*100 = 6900).
// SMART returns slot 0; GREEDY returns slot 1.
ZENITH_TEST(ZM_Battle, BattleAI_Smart_TakesGuaranteedKO)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);       // NORMAL, sure-hit
	axP[0].m_aeMoves[1] = ZM_MOVE_TORRENTCANNON;                        // pow110 acc80
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	EnemyActive(xState).m_uCurHP = 10u;                                // rig low HP
	ZM_BattleRNG xAIRng(11u, 54ull);

	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_GE(DetDamage(xMe, xOpp, 0u, 85u), xOpp.m_uCurHP, "construction: slot 0 must be a min-roll KO");
	ZENITH_ASSERT_EQ(GreedyArgmaxMove(xState, ZM_SIDE_PLAYER), 1u, "construction: the can-miss move should be the greedy pick");

	const ZM_BattleAction xSmart  = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_SMART,  xAIRng);
	const ZM_BattleAction xGreedy = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xSmart.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xSmart.m_uMoveSlot, 0u, "SMART must take the guaranteed sure-hit KO (slot 0)");
	ZENITH_ASSERT_EQ(xGreedy.m_uMoveSlot, 1u, "GREEDY (control) prefers the higher-score can-miss move (slot 1)");
}

// KO precedes HEAL: a low-HP SMART mon with BOTH a lethal move and a heal move
// takes the KO, it does not heal.
//   AI HP rigged 10 (< 50%). opp HP 10. slot0 RAMBASH is a guaranteed KO.
// SMART returns the KO move (slot 0).
ZENITH_TEST(ZM_Battle, BattleAI_Smart_KOPrecedesHeal)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 100u, 150u, 50u, 150u, 50u, 100u);
	axP[0].m_aeMoves[1] = ZM_MOVE_REPOSE;                              // heal (HEAL_HALF)
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	PlayerActive(xState).m_uCurHP = 10u;    // < 50% of 175 -> heal WOULD trigger
	EnemyActive(xState).m_uCurHP  = 10u;    // but a KO is available
	ZM_BattleRNG xAIRng(12u, 54ull);

	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_LT(xMe.m_uCurHP * 2u, xMe.m_auMaxStat[ZM_STAT_HP], "construction: AI must be below 50% HP");
	ZENITH_ASSERT_GE(DetDamage(xMe, xOpp, 0u, 85u), xOpp.m_uCurHP, "construction: slot 0 must be a guaranteed KO");

	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_SMART, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE, "SMART must take the KO, not heal");
	ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 0u, "SMART must pick the lethal move (slot 0), not the heal (slot 1)");
}

// Heals when low and NO KO is available (control for KOPrecedesHeal): same
// low-HP mon, but the opponent is at high HP so nothing is lethal -> SMART heals.
ZENITH_TEST(ZM_Battle, BattleAI_Smart_HealsWhenLowAndNoKO)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 100u, 150u, 50u, 150u, 50u, 100u);
	axP[0].m_aeMoves[1] = ZM_MOVE_REPOSE;
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_RAMBASH, 200u, 150u, 50u, 150u, 50u, 100u); // HP 275

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	PlayerActive(xState).m_uCurHP = 10u;    // < 50% of 175
	// opp left at full HP (275) -> no move can KO it
	ZM_BattleRNG xAIRng(13u, 54ull);

	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_LT(xMe.m_uCurHP * 2u, xMe.m_auMaxStat[ZM_STAT_HP], "construction: AI must be below 50% HP");
	ZENITH_ASSERT_LT(DetDamage(xMe, xOpp, 0u, 85u), xOpp.m_uCurHP, "construction: no move may reach opp HP");

	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_SMART, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE, "SMART should heal (a self-targeted MOVE)");
	ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 1u, "SMART must use the heal move (REPOSE, slot 1) when low and no KO");
}

// Does NOT heal at/above 50% HP: a full-HP mon with a heal move and no KO falls
// through to GREEDY and picks a damaging move.
ZENITH_TEST(ZM_Battle, BattleAI_Smart_NoHealAtOrAboveHalf)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 100u, 150u, 50u, 150u, 50u, 100u);
	axP[0].m_aeMoves[1] = ZM_MOVE_REPOSE;
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_RAMBASH, 200u, 150u, 50u, 150u, 50u, 100u);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	// AI at FULL HP (built curHP == maxHP), opp at full HP -> no heal, no KO.
	ZM_BattleRNG xAIRng(14u, 54ull);

	const ZM_BattleMonster& xMe = xState.Side(ZM_SIDE_PLAYER).Active();
	ZENITH_ASSERT_GE(xMe.m_uCurHP * 2u, xMe.m_auMaxStat[ZM_STAT_HP], "construction: AI must be at/above 50% HP");

	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_SMART, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 0u, "SMART must NOT heal above 50% HP -- it falls back to GREEDY (slot 0)");
}

// Switches out of a hopeless matchup to the better-typed bench member.
//   Active KINDLET (FIRE): opp FINLET (WATER) hits FIRE for 200% (effIn) and my
//   only move FLARELASH (FIRE) hits WATER for 50% (effOut) -> hopeless. Bench
//   MINNET (WATER): WATER-STAB vs WATER = 50% < 200 -> improves. No KO.
// SMART returns SWITCH to slot 1.
ZENITH_TEST(ZM_Battle, BattleAI_Smart_SwitchesOutOfHopeless)
{
	ZM_BattleMonsterSpec axP[2];
	axP[0] = MakeSpecOverride(ZM_SPECIES_KINDLET, 50u, ZM_MOVE_FLARELASH, 100u, 100u, 100u, 100u, 100u, 100u);
	axP[1] = MakeSpecOverride(ZM_SPECIES_MINNET,  50u, ZM_MOVE_RAMBASH,   100u, 100u, 100u, 100u, 100u, 100u); // bench
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_FINLET,  50u, ZM_MOVE_RAMBASH,   150u, 100u, 100u, 100u, 100u, 100u); // high HP

	ZM_BattleState xState;
	BuildAIState(xState, axP, 2u, axE, 1u);
	ZM_BattleRNG xAIRng(15u, 54ull);

	// self-guarding species-type premises (a future re-typing must break loudly).
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_KINDLET).m_aeTypes[0], (u_int)ZM_TYPE_FIRE,  "KINDLET must be FIRE");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_KINDLET).m_aeTypes[1], (u_int)ZM_TYPE_NONE,  "KINDLET must be mono-type");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_FINLET).m_aeTypes[0],  (u_int)ZM_TYPE_WATER, "FINLET must be WATER");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_FINLET).m_aeTypes[1],  (u_int)ZM_TYPE_NONE,  "FINLET must be mono-type");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_MINNET).m_aeTypes[0],  (u_int)ZM_TYPE_WATER, "MINNET must be WATER");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_MINNET).m_aeTypes[1],  (u_int)ZM_TYPE_NONE,  "MINNET must be mono-type");

	// construction: verify the exact effectiveness percents that make it hopeless.
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_WATER, ZM_TYPE_FIRE, ZM_TYPE_NONE), 200u, "WATER vs FIRE must be 200%");
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_FIRE, ZM_TYPE_WATER, ZM_TYPE_NONE), 50u,  "FIRE vs WATER must be 50%");
	ZENITH_ASSERT_EQ(ZM_EffectivenessPercent(ZM_TYPE_WATER, ZM_TYPE_WATER, ZM_TYPE_NONE), 50u, "WATER vs WATER must be 50%");

	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_SMART, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_SWITCH, "SMART must switch out of the hopeless matchup");
	ZENITH_ASSERT_EQ(xA.m_uSwitchSlot, 1u, "SMART must switch to the better-typed bench member (slot 1)");
}

// Stays when the matchup is OK (control): active NIBBIN (NORMAL) vs FINLET
// (WATER) has effIn = 100 (< 200) -> not hopeless. A bench exists but SMART does
// NOT switch; it plays a GREEDY move.
ZENITH_TEST(ZM_Battle, BattleAI_Smart_StaysWhenMatchupOkControl)
{
	ZM_BattleMonsterSpec axP[2];
	axP[0] = MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 100u, 150u, 50u, 150u, 50u, 100u);
	axP[1] = MakeSpecOverride(ZM_SPECIES_MINNET, 50u, ZM_MOVE_RAMBASH, 100u, 100u, 100u, 100u, 100u, 100u); // bench
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_FINLET, 50u, ZM_MOVE_RAMBASH, 200u, 100u, 100u, 100u, 100u, 100u);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 2u, axE, 1u);
	ZM_BattleRNG xAIRng(16u, 54ull);

	// self-guarding species-type premises (a future re-typing must break loudly).
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_aeTypes[0], (u_int)ZM_TYPE_NORMAL, "NIBBIN must be NORMAL");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_aeTypes[1], (u_int)ZM_TYPE_NONE,   "NIBBIN must be mono-type");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_FINLET).m_aeTypes[0], (u_int)ZM_TYPE_WATER,  "FINLET must be WATER");

	ZENITH_ASSERT_LT(ZM_EffectivenessPercent(ZM_TYPE_WATER, ZM_TYPE_NORMAL, ZM_TYPE_NONE), 200u, "construction: matchup must not be hopeless");

	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_SMART, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_MOVE, "SMART must not switch when the matchup is OK");
	ZENITH_ASSERT_EQ(xA.m_uMoveSlot, 0u, "SMART falls to GREEDY -> the damaging move (slot 0)");
}

// Bench selection picks the STRICTLY-smallest incoming effectiveness.
//   Active KINDLET (FIRE) hopeless vs FINLET (WATER). Bench: slot1 NIBBIN
//   (NORMAL, WATER->NORMAL = 100%), slot2 MINNET (WATER, WATER->WATER = 50%).
// SMART switches to slot 2 (smallest effIn_j).
ZENITH_TEST(ZM_Battle, BattleAI_Smart_SwitchPicksBestDefensiveBench)
{
	ZM_BattleMonsterSpec axP[3];
	axP[0] = MakeSpecOverride(ZM_SPECIES_KINDLET, 50u, ZM_MOVE_FLARELASH, 100u, 100u, 100u, 100u, 100u, 100u);
	axP[1] = MakeSpecOverride(ZM_SPECIES_NIBBIN,  50u, ZM_MOVE_RAMBASH,   100u, 100u, 100u, 100u, 100u, 100u); // effIn 100
	axP[2] = MakeSpecOverride(ZM_SPECIES_MINNET,  50u, ZM_MOVE_RAMBASH,   100u, 100u, 100u, 100u, 100u, 100u); // effIn 50
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_FINLET,  50u, ZM_MOVE_RAMBASH,   150u, 100u, 100u, 100u, 100u, 100u);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 3u, axE, 1u);
	ZM_BattleRNG xAIRng(17u, 54ull);

	// self-guarding species-type premises (a future re-typing must break loudly).
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_KINDLET).m_aeTypes[0], (u_int)ZM_TYPE_FIRE,   "KINDLET must be FIRE");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_FINLET).m_aeTypes[0],  (u_int)ZM_TYPE_WATER,  "FINLET must be WATER");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_NIBBIN).m_aeTypes[0],  (u_int)ZM_TYPE_NORMAL, "NIBBIN (slot 1 bench) must be NORMAL");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_MINNET).m_aeTypes[0],  (u_int)ZM_TYPE_WATER,  "MINNET (slot 2 bench) must be WATER");

	// construction: slot 2's incoming effectiveness is strictly smaller than slot 1's.
	ZENITH_ASSERT_LT(ZM_EffectivenessPercent(ZM_TYPE_WATER, ZM_TYPE_WATER, ZM_TYPE_NONE),
	                 ZM_EffectivenessPercent(ZM_TYPE_WATER, ZM_TYPE_NORMAL, ZM_TYPE_NONE),
	                 "construction: MINNET (slot 2) must resist the opp STAB more than NIBBIN (slot 1)");

	const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_SMART, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_SWITCH);
	ZENITH_ASSERT_EQ(xA.m_uSwitchSlot, 2u, "SMART must switch to the best-defensive bench member (slot 2)");
}

// Falls back to GREEDY when nothing in the cascade fires: healthy active,
// neutral matchup, no KO, no heal move -> SMART == GREEDY for the same state.
ZENITH_TEST(ZM_Battle, BattleAI_Smart_FallsBackToGreedy)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_RAMBASH, 100u, 150u, 50u, 150u, 50u, 100u); // HP 175, no KO

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	ZM_BattleRNG xAIRng(18u, 54ull);

	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_LT(DetDamage(xMe, xOpp, 0u, 85u), xOpp.m_uCurHP, "construction: no KO available");

	ZM_BattleRNG xRngS(18u, 54ull);
	ZM_BattleRNG xRngG(18u, 54ull);
	const ZM_BattleAction xSmart  = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_SMART,  xRngS);
	const ZM_BattleAction xGreedy = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY, xRngG);
	ZENITH_ASSERT_EQ((u_int)xSmart.m_eKind, (u_int)xGreedy.m_eKind, "SMART fallback must match GREEDY kind");
	ZENITH_ASSERT_EQ(xSmart.m_uMoveSlot, xGreedy.m_uMoveSlot, "SMART fallback must match GREEDY slot");
	ZENITH_ASSERT_EQ((u_int)xSmart.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xSmart.m_uMoveSlot, 0u, "the shared GREEDY pick is the higher-power move (slot 0)");
}

// ============================================================================
// CHAMPION (6) -- 2-ply: own move + one modeled GREEDY opponent reply. Fully
// deterministic (draws nothing). The AI's active carries RAMBASH (slot0, prio0,
// pow45) + QUICKJAB (slot1, prio+1, pow40): GREEDY always prefers RAMBASH, so
// any CHAMPION pick of QUICKJAB is a genuine 2-ply divergence.
// ============================================================================

// The mandated TRAP: the AI is SLOWER, both moves KO the opponent, and the
// opponent's modeled reply one-shots the AI. GREEDY picks RAMBASH (slot0) and
// loses the speed race -> faints before it lands. CHAMPION picks the +priority
// QUICKJAB (slot1): it moves first, KOs, and takes no reply.
ZENITH_TEST(ZM_Battle, BattleAI_Champion_TrapPriorityKO)
{
	// AI: high ATK, LOW speed, low DEF; rig curHP low so the reply is lethal.
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 50u, 150u, 50u, 150u, 50u, 1u); // SPE base 1 -> 21
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	// Opponent: fast, strong single move (its GREEDY reply), low DEF.
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_BRUTESLAM, 50u, 150u, 50u, 150u, 50u, 200u); // SPE base 200 -> 220

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	PlayerActive(xState).m_uCurHP = 10u;   // opp reply one-shots the AI
	EnemyActive(xState).m_uCurHP  = 10u;   // both AI moves KO
	ZM_BattleRNG xAIRng(19u, 54ull);

	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();
	// construction preconditions for the trap:
	ZENITH_ASSERT_LT(xMe.m_auMaxStat[ZM_STAT_SPEED], xOpp.m_auMaxStat[ZM_STAT_SPEED], "AI must be slower");
	ZENITH_ASSERT_GE(DetDamage(xMe, xOpp, 0u, 92u), xOpp.m_uCurHP, "RAMBASH must KO");
	ZENITH_ASSERT_GE(DetDamage(xMe, xOpp, 1u, 92u), xOpp.m_uCurHP, "QUICKJAB must KO");
	ZENITH_ASSERT_GE(DetDamage(xOpp, xMe, 0u, 92u), xMe.m_uCurHP, "opp reply must one-shot the AI");
	ZENITH_ASSERT_EQ(GreedyArgmaxMove(xState, ZM_SIDE_PLAYER), 0u, "GREEDY must prefer RAMBASH");

	const ZM_BattleAction xGreedy   = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY,   xAIRng);
	const ZM_BattleAction xChampion = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_CHAMPION, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xGreedy.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xGreedy.m_uMoveSlot, 0u, "GREEDY walks into the trap (RAMBASH, slot 0)");
	ZENITH_ASSERT_EQ((u_int)xChampion.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xChampion.m_uMoveSlot, 1u, "CHAMPION takes the priority KO (QUICKJAB, slot 1)");
	ZENITH_ASSERT_NE(xChampion.m_uMoveSlot, xGreedy.m_uMoveSlot, "CHAMPION must diverge from GREEDY in the trap");
}

// Non-trap control: identical moves/HP but the AI is FASTER. Acting order no
// longer punishes RAMBASH, so CHAMPION agrees with GREEDY (slot 0).
ZENITH_TEST(ZM_Battle, BattleAI_Champion_NonTrapFasterMatchesGreedy)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 50u, 150u, 50u, 150u, 50u, 200u); // FAST (220)
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_BRUTESLAM, 50u, 150u, 50u, 150u, 50u, 1u); // SLOW (21)

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	PlayerActive(xState).m_uCurHP = 10u;
	EnemyActive(xState).m_uCurHP  = 10u;
	ZM_BattleRNG xAIRng(20u, 54ull);

	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_GT(xMe.m_auMaxStat[ZM_STAT_SPEED], xOpp.m_auMaxStat[ZM_STAT_SPEED], "AI must be faster");
	ZENITH_ASSERT_GE(DetDamage(xMe, xOpp, 0u, 92u), xOpp.m_uCurHP, "RAMBASH must KO");

	const ZM_BattleAction xGreedy   = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY,   xAIRng);
	const ZM_BattleAction xChampion = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_CHAMPION, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xChampion.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xChampion.m_uMoveSlot, 0u, "CHAMPION agrees with GREEDY when faster (RAMBASH, slot 0)");
	ZENITH_ASSERT_EQ(xChampion.m_uMoveSlot, xGreedy.m_uMoveSlot, "no divergence when the 2-ply does not matter");
}

// KO removes the reply valuation: the AI is SLOWER and SURVIVES the reply, but
// the priority KO (QUICKJAB) still wins because taking it first means opp is
// KO'd and the (survivable) reply is never applied -> higher post-state HP.
// GREEDY still walks into the reply with RAMBASH. Distinct sim branch from the
// trap (here the AI does not faint on the RAMBASH line).
ZENITH_TEST(ZM_Battle, BattleAI_Champion_PriorityKODodgesSurvivableReply)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 150u, 150u, 50u, 150u, 50u, 1u); // HP 225, SLOW
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_BRUTESLAM, 50u, 150u, 50u, 150u, 50u, 200u); // FAST

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	PlayerActive(xState).m_uCurHP = 200u;  // survives the reply
	EnemyActive(xState).m_uCurHP  = 10u;   // both AI moves KO
	ZM_BattleRNG xAIRng(21u, 54ull);

	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_LT(xMe.m_auMaxStat[ZM_STAT_SPEED], xOpp.m_auMaxStat[ZM_STAT_SPEED], "AI must be slower");
	ZENITH_ASSERT_GE(DetDamage(xMe, xOpp, 0u, 92u), xOpp.m_uCurHP, "RAMBASH must KO");
	ZENITH_ASSERT_GE(DetDamage(xMe, xOpp, 1u, 92u), xOpp.m_uCurHP, "QUICKJAB must KO");
	ZENITH_ASSERT_GT(DetDamage(xOpp, xMe, 0u, 92u), 0u,            "opp reply must deal real damage");
	ZENITH_ASSERT_LT(DetDamage(xOpp, xMe, 0u, 92u), xMe.m_uCurHP,  "AI must SURVIVE the reply");

	const ZM_BattleAction xGreedy   = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY,   xAIRng);
	const ZM_BattleAction xChampion = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_CHAMPION, xAIRng);
	ZENITH_ASSERT_EQ(xGreedy.m_uMoveSlot, 0u, "GREEDY concedes the reply (RAMBASH, slot 0)");
	ZENITH_ASSERT_EQ((u_int)xChampion.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xChampion.m_uMoveSlot, 1u, "CHAMPION dodges the reply with the priority KO (slot 1)");
	ZENITH_ASSERT_NE(xChampion.m_uMoveSlot, xGreedy.m_uMoveSlot, "CHAMPION must diverge from GREEDY");
}

// Modeled reply drives the decision: the AI is SLOWER (the trap's speed setup)
// but the opponent's only move is a STATUS move, so the modeled reply deals ZERO
// damage. With nothing to dodge, CHAMPION agrees with GREEDY (RAMBASH, slot 0).
// Proves CHAMPION's divergence depends on the reply's DAMAGE, not just on speed.
ZENITH_TEST(ZM_Battle, BattleAI_Champion_HarmlessReplyMatchesGreedy)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 150u, 150u, 50u, 150u, 50u, 1u); // SLOW
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_WHETCLAW, 50u, 150u, 50u, 150u, 50u, 200u); // status-only, FAST

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	PlayerActive(xState).m_uCurHP = 200u;
	EnemyActive(xState).m_uCurHP  = 10u;   // both AI moves KO
	ZM_BattleRNG xAIRng(22u, 54ull);

	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();
	ZENITH_ASSERT_LT(xMe.m_auMaxStat[ZM_STAT_SPEED], xOpp.m_auMaxStat[ZM_STAT_SPEED], "AI must be slower");
	ZENITH_ASSERT_EQ(DetDamage(xOpp, xMe, 0u, 92u), 0u, "opp's status reply must deal zero damage");
	ZENITH_ASSERT_GE(DetDamage(xMe, xOpp, 0u, 92u), xOpp.m_uCurHP, "RAMBASH must KO");

	const ZM_BattleAction xGreedy   = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY,   xAIRng);
	const ZM_BattleAction xChampion = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_CHAMPION, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xChampion.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xChampion.m_uMoveSlot, 0u, "harmless reply -> CHAMPION agrees with GREEDY (slot 0)");
	ZENITH_ASSERT_EQ(xChampion.m_uMoveSlot, xGreedy.m_uMoveSlot, "no divergence when the reply is harmless");
}

// CHAMPION anticipates the opponent's GREEDY-best reply -- NOT its slot-0 move
// and NOT its raw-max-base-power move. The opponent carries THREE moves whose
// GREEDY-best (max deterministic expected-damage x accuracy, roll 92) is a
// DIFFERENT slot from BOTH slot 0 AND the max-base-power slot; only that reply
// is lethal to the (slower) AI, and modeling it flips CHAMPION's own pick. A
// regression to a "reply = slot 0" or "reply = max base power" model would
// choose the AI's other move and fail this test.
//   AI = FERNFAWN (GRASS), slower; moves: slot0 BRUTESLAM (prio0, high dmg,
//   non-KO), slot1 QUICKJAB (prio+1, low dmg, non-KO).
//   Opp = STRAYLING (NORMAL -> no STAB skew on any move), attacking FERNFAWN:
//     slot0 BRINELASH     WATER pow40  (50%) acc100 -> score  2000
//     slot1 TORRENTCANNON WATER pow110 (50%) acc80  -> score  4320  (MAX base power)
//     slot2 FLARELASH     FIRE  pow75 (200%) acc100 -> score 15000  (GREEDY-best)
//   dmgR: only FLARELASH (150) is lethal to the AI (HP 100); BRINELASH (20) and
//   TORRENTCANNON (54) are not. Slower AI + lethal reply => the priority QUICKJAB
//   (slot1) chips before dying (V ~ -100235) beats BRUTESLAM (slot0) which never
//   lands (V ~ -100275). A non-lethal (wrong-model) reply would instead favor the
//   higher-damage BRUTESLAM (slot0). CHAMPION must pick slot 1.
ZENITH_TEST(ZM_Battle, BattleAI_Champion_UsesGreedyReplyNotSlot0OrMaxPower)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeSpecOverride(ZM_SPECIES_FERNFAWN, 50u, ZM_MOVE_BRUTESLAM, 100u, 150u, 50u, 150u, 50u, 1u); // slow
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_BRINELASH, 200u, 150u, 50u, 150u, 50u, 200u); // fast, HP 275
	axE[0].m_aeMoves[1] = ZM_MOVE_TORRENTCANNON;   // max base power (110), but 50% + acc80
	axE[0].m_aeMoves[2] = ZM_MOVE_FLARELASH;       // GREEDY-best (200% super-effective)

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	PlayerActive(xState).m_uCurHP = 100u;   // FLARELASH reply (150) lethal; the others are not
	ZM_BattleRNG xAIRng(23u, 54ull);

	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();

	// self-guarding species-type premises (no STAB skew on the opp reply moves).
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_FERNFAWN).m_aeTypes[0],  (u_int)ZM_TYPE_GRASS,  "FERNFAWN must be GRASS");
	ZENITH_ASSERT_EQ((u_int)ZM_GetSpeciesData(ZM_SPECIES_STRAYLING).m_aeTypes[0], (u_int)ZM_TYPE_NORMAL, "STRAYLING must be NORMAL");

	// opp's GREEDY-best reply slot differs from slot 0 AND from the max-base-power slot.
	u_int uMaxPowSlot = uZM_MAX_MOVES;
	u_int uMaxPow = 0u;
	for (u_int s = 0; s < uZM_MAX_MOVES; ++s)
	{
		if (!IsLegalMoveSlot(xOpp, s)) { continue; }
		const u_int uPow = ZM_GetMoveData(xOpp.m_axMoves[s].m_eMove).m_uPower;
		if (uMaxPowSlot == uZM_MAX_MOVES || uPow > uMaxPow) { uMaxPowSlot = s; uMaxPow = uPow; }
	}
	const u_int uOppGreedy = GreedyArgmaxMove(xState, ZM_SIDE_ENEMY);
	ZENITH_ASSERT_EQ(uOppGreedy, 2u,   "construction: opp GREEDY-best reply must be slot 2 (FLARELASH)");
	ZENITH_ASSERT_EQ(uMaxPowSlot, 1u,  "construction: opp max-base-power move must be slot 1 (TORRENTCANNON)");
	ZENITH_ASSERT_NE(uOppGreedy, 0u,          "GREEDY-best reply must differ from opp slot 0");
	ZENITH_ASSERT_NE(uOppGreedy, uMaxPowSlot, "GREEDY-best reply must differ from opp max-base-power move");

	// only the GREEDY-best reply is lethal; the slot-0 and max-power replies are not.
	ZENITH_ASSERT_GE(DetDamage(xOpp, xMe, uOppGreedy, 92u),  xMe.m_uCurHP, "GREEDY-best reply must be lethal to the AI");
	ZENITH_ASSERT_LT(DetDamage(xOpp, xMe, 0u, 92u),          xMe.m_uCurHP, "slot-0 reply must be NON-lethal");
	ZENITH_ASSERT_LT(DetDamage(xOpp, xMe, uMaxPowSlot, 92u), xMe.m_uCurHP, "max-power reply must be NON-lethal");

	// AI slower; neither AI move KOs; slot 0 is the higher-damage move.
	ZENITH_ASSERT_LT(xMe.m_auMaxStat[ZM_STAT_SPEED], xOpp.m_auMaxStat[ZM_STAT_SPEED], "AI must be slower");
	ZENITH_ASSERT_LT(DetDamage(xMe, xOpp, 0u, 92u), xOpp.m_uCurHP, "AI slot 0 must not KO");
	ZENITH_ASSERT_LT(DetDamage(xMe, xOpp, 1u, 92u), xOpp.m_uCurHP, "AI slot 1 must not KO");
	ZENITH_ASSERT_GT(DetDamage(xMe, xOpp, 0u, 92u), DetDamage(xMe, xOpp, 1u, 92u), "AI slot 0 must out-damage slot 1");

	const ZM_BattleAction xGreedy   = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY,   xAIRng);
	const ZM_BattleAction xChampion = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_CHAMPION, xAIRng);
	ZENITH_ASSERT_EQ(xGreedy.m_uMoveSlot, 0u, "GREEDY (unaware of the reply) picks the higher-damage move (slot 0)");
	ZENITH_ASSERT_EQ((u_int)xChampion.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xChampion.m_uMoveSlot, 1u, "CHAMPION must anticipate the lethal GREEDY-best reply and pick slot 1");
	ZENITH_ASSERT_NE(xChampion.m_uMoveSlot, xGreedy.m_uMoveSlot, "CHAMPION must diverge from GREEDY here");
}

// Exact effective-speed TIE -> CHAMPION's conservative pessimism models the
// OPPONENT as moving first (it never assumes it wins a coin-flip). Same shape as
// the trap but at EQUAL speed: RAMBASH (slot0, prio0) would win only if the AI
// resolved the tie in its own favour, so an impl that models ties AI-first picks
// slot 0; the correct opponent-first pessimism takes the priority QUICKJAB.
ZENITH_TEST(ZM_Battle, BattleAI_Champion_SpeedTieAssumesOpponentFirst)
{
	ZM_BattleMonsterSpec axP[1];
	axP[0] = MakeSpecOverride(ZM_SPECIES_NIBBIN, 50u, ZM_MOVE_RAMBASH, 50u, 150u, 50u, 150u, 50u, 100u); // SPE 120
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeSpecOverride(ZM_SPECIES_STRAYLING, 50u, ZM_MOVE_BRUTESLAM, 50u, 150u, 50u, 150u, 50u, 100u); // SPE 120 (tie)

	ZM_BattleState xState;
	BuildAIState(xState, axP, 1u, axE, 1u);
	PlayerActive(xState).m_uCurHP = 10u;   // opp reply is lethal
	EnemyActive(xState).m_uCurHP  = 10u;   // both AI moves KO
	ZM_BattleRNG xAIRng(24u, 54ull);

	const ZM_BattleMonster& xMe  = xState.Side(ZM_SIDE_PLAYER).Active();
	const ZM_BattleMonster& xOpp = xState.Side(ZM_SIDE_ENEMY).Active();

	// exact tie: neither is paralysed / stage-0, so effective speed == raw SPEED stat.
	ZENITH_ASSERT_EQ(xMe.m_auMaxStat[ZM_STAT_SPEED], xOpp.m_auMaxStat[ZM_STAT_SPEED], "construction: effective speeds must be EQUAL");
	ZENITH_ASSERT_GE(DetDamage(xMe, xOpp, 0u, 92u), xOpp.m_uCurHP, "RAMBASH must KO");
	ZENITH_ASSERT_GE(DetDamage(xMe, xOpp, 1u, 92u), xOpp.m_uCurHP, "QUICKJAB must KO");
	ZENITH_ASSERT_GE(DetDamage(xOpp, xMe, 0u, 92u), xMe.m_uCurHP, "opp reply must be lethal");
	ZENITH_ASSERT_EQ(GreedyArgmaxMove(xState, ZM_SIDE_PLAYER), 0u, "GREEDY (and an AI-first tie model) would pick RAMBASH (slot 0)");

	const ZM_BattleAction xChampion = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_CHAMPION, xAIRng);
	ZENITH_ASSERT_EQ((u_int)xChampion.m_eKind, (u_int)ZM_ACTION_MOVE);
	ZENITH_ASSERT_EQ(xChampion.m_uMoveSlot, 1u, "on an exact speed tie CHAMPION assumes opp-first -> takes the priority KO (slot 1)");
}

// ============================================================================
// API / contract (4)
// ============================================================================

// Choosing an action NEVER advances the battle's own RNG (xState.m_xRNG): after
// calling all four tiers, the battle RNG produces the same next 8 draws as a
// pre-call snapshot. (ZM_ChooseAction reads state through a const& and draws only
// from the caller's xAIRng -- the ZM-D-032/033 zero-perturbation contract.)
ZENITH_TEST(ZM_Battle, BattleAI_NeverPerturbsBattleRNG)
{
	ZM_BattleMonsterSpec axP[2];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	axP[1] = MakeNeutralAtk(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_FINLET, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 2u, axE, 1u, 0xABCDEF01ull);

	const ZM_BattleRNG xBefore = xState.m_xRNG;   // value snapshot of the cursor

	ZM_BattleRNG xAIRng(0x777ull, 54ull);
	(void)ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_RANDOM,   xAIRng);
	(void)ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_GREEDY,   xAIRng);
	(void)ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_SMART,    xAIRng);
	(void)ZM_ChooseAction(xState, ZM_SIDE_PLAYER, ZM_AI_TIER_CHAMPION, xAIRng);

	ZM_BattleRNG xAfter = xState.m_xRNG;
	ZM_BattleRNG xExpect = xBefore;
	for (int i = 0; i < 8; ++i)
	{
		ZENITH_ASSERT_EQ(xAfter.Next(), xExpect.Next(), "battle RNG cursor advanced by ZM_ChooseAction (draw %d)", i);
	}
}

// Every tier returns only ZM_ACTION_MOVE or ZM_ACTION_SWITCH -- never ITEM / RUN
// / NONE (an AI opponent has no ball and cannot flee).
ZENITH_TEST(ZM_Battle, BattleAI_NeverReturnsItemOrRun)
{
	ZM_BattleMonsterSpec axP[2];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	axP[1] = MakeNeutralAtk(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_FINLET, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 2u, axE, 1u);

	const ZM_AI_TIER aeTiers[4] = { ZM_AI_TIER_RANDOM, ZM_AI_TIER_GREEDY, ZM_AI_TIER_SMART, ZM_AI_TIER_CHAMPION };
	for (int t = 0; t < 4; ++t)
	{
		ZM_BattleRNG xAIRng(0x100u + (u_int)t, 54ull);
		// sample many draws for the stochastic tier, once is enough for the rest
		const int iReps = (aeTiers[t] == ZM_AI_TIER_RANDOM) ? 500 : 1;
		for (int r = 0; r < iReps; ++r)
		{
			const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, aeTiers[t], xAIRng);
			const bool bMoveOrSwitch = (xA.m_eKind == ZM_ACTION_MOVE || xA.m_eKind == ZM_ACTION_SWITCH);
			ZENITH_ASSERT_TRUE(bMoveOrSwitch, "tier %d returned kind %u (must be MOVE or SWITCH)", t, (u_int)xA.m_eKind);
			ZENITH_ASSERT_NE((u_int)xA.m_eKind, (u_int)ZM_ACTION_ITEM);
			ZENITH_ASSERT_NE((u_int)xA.m_eKind, (u_int)ZM_ACTION_RUN);
		}
	}
}

// Every tier returns a LEGAL action for a representative state (a MOVE with PP>0,
// or a switch to a valid bench slot per section-3 enumeration).
ZENITH_TEST(ZM_Battle, BattleAI_EachTierReturnsLegalAction)
{
	ZM_BattleMonsterSpec axP[2];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);
	axP[0].m_aeMoves[1] = ZM_MOVE_QUICKJAB;
	axP[1] = MakeNeutralAtk(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_FINLET, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 2u, axE, 1u);

	const ZM_AI_TIER aeTiers[4] = { ZM_AI_TIER_RANDOM, ZM_AI_TIER_GREEDY, ZM_AI_TIER_SMART, ZM_AI_TIER_CHAMPION };
	for (int t = 0; t < 4; ++t)
	{
		ZM_BattleRNG xAIRng(0x200u + (u_int)t, 54ull);
		const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, aeTiers[t], xAIRng);
		ZENITH_ASSERT_TRUE(IsLegalAction(xState, ZM_SIDE_PLAYER, xA), "tier %d returned an illegal action (kind %u slot %u)",
			t, (u_int)xA.m_eKind, xA.m_uMoveSlot);
	}
}

// No legal MOVE (all PP drained) but a living bench -> every tier returns the
// lowest legal SWITCH.
ZENITH_TEST(ZM_Battle, BattleAI_NoLegalMoveReturnsSwitch)
{
	ZM_BattleMonsterSpec axP[2];
	axP[0] = MakeNeutralAtk(ZM_SPECIES_NIBBIN, ZM_MOVE_RAMBASH);     // slot 0 only
	axP[1] = MakeNeutralAtk(ZM_SPECIES_STRAYLING, ZM_MOVE_RAMBASH);  // living bench (slot 1)
	ZM_BattleMonsterSpec axE[1];
	axE[0] = MakeNeutralDef(ZM_SPECIES_FINLET, ZM_MOVE_RAMBASH);

	ZM_BattleState xState;
	BuildAIState(xState, axP, 2u, axE, 1u);
	PlayerActive(xState).m_axMoves[0].m_uCurPP = 0u;   // drain the only move's PP

	// construction: no legal move remains, but SWITCH to slot 1 is legal.
	ZENITH_ASSERT_FALSE(IsLegalMoveSlot(xState.Side(ZM_SIDE_PLAYER).Active(), 0u), "construction: slot 0 must be out of PP");
	ZENITH_ASSERT_TRUE(xState.Side(ZM_SIDE_PLAYER).CanSwitchTo(1u), "construction: bench slot 1 must be switchable");

	const ZM_AI_TIER aeTiers[4] = { ZM_AI_TIER_RANDOM, ZM_AI_TIER_GREEDY, ZM_AI_TIER_SMART, ZM_AI_TIER_CHAMPION };
	for (int t = 0; t < 4; ++t)
	{
		ZM_BattleRNG xAIRng(0x300u + (u_int)t, 54ull);
		const ZM_BattleAction xA = ZM_ChooseAction(xState, ZM_SIDE_PLAYER, aeTiers[t], xAIRng);
		ZENITH_ASSERT_EQ((u_int)xA.m_eKind, (u_int)ZM_ACTION_SWITCH, "tier %d must switch when no move is legal", t);
		ZENITH_ASSERT_EQ(xA.m_uSwitchSlot, 1u, "tier %d must pick the lowest legal switch (slot 1)", t);
	}
}
