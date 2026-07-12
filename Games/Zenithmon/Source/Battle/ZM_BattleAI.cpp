#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_BattleAI.h"
#include "Zenithmon/Source/Battle/ZM_DamageCalc.h"    // ZM_DamageInput, ZM_CalcDamage, ZM_ApplyStatStage, ZM_EffectivenessPercent
#include "Zenithmon/Source/Data/ZM_MoveData.h"        // ZM_GetMoveData, ZM_MoveData, categories / effects / accuracy sentinel
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"     // ZM_GetSpeciesData, ZM_STAT_*, species typing

// ============================================================================
// ZM_BattleAI -- see the header banner. Everything below is a pure, read-only
// function of (const state, side, tier, ai-rng): no ZM_MoveExecutor / SubmitAction
// / ResolveTurn call, no event emission, no mutation of xState, and no draw from
// xState.m_xRNG. Only RANDOM draws, and only from the caller-owned xAIRng. The
// deterministic scoring mirrors ZM_MoveExecutor::ApplyDamagingHit's ZM_DamageInput
// build with a FIXED roll and no crit, so it can never advance a battle stream.
// ============================================================================

namespace
{
	// The two fixed damage rolls (section 4.2). The absolute constant is
	// argmax-invariant (ZM_CalcDamage is monotonic in the roll and every candidate
	// is scored with the same roll); it is pinned only so numbers are reproducible.
	const u_int uZM_AI_ROLL_EXPECTED = 92u;   // GREEDY / CHAMPION expected damage (mean of 85..100)
	const u_int uZM_AI_ROLL_MIN      = 85u;   // SMART guaranteed-KO detection (worst-case roll)

	// Static-eval faint bonus/penalty for the CHAMPION 2-ply position score.
	const int  iZM_AI_FAINT_VALUE    = 100000;

	// "no move slot" sentinel for the move-picker helpers (mirrors uZM_MAX_MOVES
	// meaning "none" in ZM_BattleMonster's charged/locked slot fields).
	const u_int uZM_AI_NO_MOVE = uZM_MAX_MOVES;

	// -- MOVE-action / SWITCH-action constructors (keep intent readable) ---------
	ZM_BattleAction g_MakeMoveAction(u_int uMoveSlot)
	{
		ZM_BattleAction xAction;
		xAction.m_eKind = ZM_ACTION_MOVE;
		xAction.m_uMoveSlot = uMoveSlot;
		return xAction;
	}

	ZM_BattleAction g_MakeSwitchAction(u_int uSwitchSlot)
	{
		ZM_BattleAction xAction;
		xAction.m_eKind = ZM_ACTION_SWITCH;
		xAction.m_uSwitchSlot = uSwitchSlot;
		return xAction;
	}

	// A slot holds a legal MOVE iff it names a real move with PP remaining.
	bool g_IsLegalMoveSlot(const ZM_BattleMonster& xMon, u_int uSlot)
	{
		const ZM_MoveSlot& xSlot = xMon.m_axMoves[uSlot];
		return xSlot.m_eMove != ZM_MOVE_NONE && xSlot.m_uCurPP > 0u;
	}

	bool g_IsDamagingMove(const ZM_MoveData& xMove)
	{
		return xMove.m_eCategory != ZM_MOVE_CATEGORY_STATUS && xMove.m_uPower > 0u;
	}

	// -- Effective speed (section 5.4) -- reimplements ZM_BattleEngine.cpp's
	// g_EffectiveSpeed: SPEED after its stage multiplier, then a 1/4 integer cut
	// when paralyzed. NO draw. Ability speed hooks are deliberately excluded (the
	// model is deterministic and clone-free).
	u_int g_EffectiveSpeed(const ZM_BattleMonster& xMon)
	{
		u_int uSpeed = ZM_ApplyStatStage(xMon.m_auMaxStat[ZM_STAT_SPEED], xMon.m_aiStage[ZM_BATTLE_STAT_SPEED]);
		if (xMon.m_eStatus == ZM_MAJOR_STATUS_PARALYSIS)
		{
			uSpeed /= 4u;
		}
		return uSpeed;
	}

	// -- Legal-action enumeration (section 3) -- MOVES-by-slot then SWITCHES-by-
	// slot. Fixed order so RANDOM's index->action map is deterministic and testable.
	// axOut is bounded by uZM_MAX_MOVES + uZM_MAX_PARTY_SIZE (<=10); returns count.
	u_int g_EnumerateLegal(const ZM_BattleState& xState, ZM_SIDE eSide,
		ZM_BattleAction axOut[uZM_MAX_MOVES + uZM_MAX_PARTY_SIZE])
	{
		const ZM_BattleSide& xSide = xState.Side(eSide);
		const ZM_BattleMonster& xActive = xSide.Active();
		u_int uCount = 0u;

		for (u_int s = 0u; s < uZM_MAX_MOVES; ++s)
		{
			if (g_IsLegalMoveSlot(xActive, s))
			{
				axOut[uCount++] = g_MakeMoveAction(s);
			}
		}

		const u_int uParty = xSide.m_xParty.GetSize();
		for (u_int j = 0u; j < uParty; ++j)
		{
			if (xSide.CanSwitchTo(j))
			{
				axOut[uCount++] = g_MakeSwitchAction(j);
			}
		}

		return uCount;
	}

	// Lowest legal SWITCH, or the documented empty-L fallback (MOVE slot 0). Used
	// when no legal MOVE exists (section 3 edge cases). Reuses FindLowestSwitchTarget.
	ZM_BattleAction g_LowestSwitchOrFallback(const ZM_BattleState& xState, ZM_SIDE eSide)
	{
		const u_int uSlot = xState.Side(eSide).FindLowestSwitchTarget();
		if (uSlot != uZM_MAX_PARTY_SIZE)
		{
			return g_MakeSwitchAction(uSlot);
		}
		// Degenerate: no PP anywhere AND no switch target (engine has no Struggle;
		// never reached by any fixture -- all built mons carry PP). Documented dead
		// path so callers always receive a well-formed action.
		return g_MakeMoveAction(0u);
	}

	// -- Deterministic damage (section 4.1) -- mirrors ApplyDamagingHit's
	// ZM_DamageInput build with a FIXED roll and bCrit=false. Returns 0 for a
	// status / zero-power / type-immune move; else >= 1. Touches no RNG.
	u_int g_DeterministicDamage(const ZM_BattleMonster& xAtk, const ZM_BattleMonster& xDef,
		u_int uMoveSlot, u_int uRoll)
	{
		const ZM_MoveData& xMove = ZM_GetMoveData(xAtk.m_axMoves[uMoveSlot].m_eMove);
		if (!g_IsDamagingMove(xMove))
		{
			return 0u;
		}

		const bool bPhysical = (xMove.m_eCategory == ZM_MOVE_CATEGORY_PHYSICAL);
		const ZM_SpeciesData& xAtkSpecies = ZM_GetSpeciesData(xAtk.m_eSpecies);
		const ZM_SpeciesData& xDefSpecies = ZM_GetSpeciesData(xDef.m_eSpecies);

		ZM_DamageInput xIn;
		xIn.uLevel = xAtk.m_uLevel;
		xIn.uPower = xMove.m_uPower;
		xIn.uAttack = ZM_ApplyStatStage(
			bPhysical ? xAtk.m_auMaxStat[ZM_STAT_ATTACK] : xAtk.m_auMaxStat[ZM_STAT_SPATTACK],
			bPhysical ? xAtk.m_aiStage[ZM_BATTLE_STAT_ATTACK] : xAtk.m_aiStage[ZM_BATTLE_STAT_SPATTACK]);
		xIn.uDefense = ZM_ApplyStatStage(
			bPhysical ? xDef.m_auMaxStat[ZM_STAT_DEFENSE] : xDef.m_auMaxStat[ZM_STAT_SPDEFENSE],
			bPhysical ? xDef.m_aiStage[ZM_BATTLE_STAT_DEFENSE] : xDef.m_aiStage[ZM_BATTLE_STAT_SPDEFENSE]);
		if (xIn.uDefense == 0u)
		{
			xIn.uDefense = 1u;   // ZM_DamageInput invariant "never 0: divisor" (only a stage<0 * tiny stat edge)
		}
		xIn.bStab = (xMove.m_eType == xAtkSpecies.m_aeTypes[0] || xMove.m_eType == xAtkSpecies.m_aeTypes[1]);
		xIn.uEffectivenessPercent = ZM_EffectivenessPercent(xMove.m_eType, xDefSpecies.m_aeTypes[0], xDefSpecies.m_aeTypes[1]);
		xIn.bCrit = false;                 // the AI never assumes a crit
		xIn.uRandomPercent = uRoll;        // fixed; no draw
		// weather / burn / screen left at box-1 identity defaults.
		return ZM_CalcDamage(xIn);
	}

	// -- Hit probability (section 4.3) -- mirrors the executor's M5 accuracy fold.
	// Sure-hit moves return 100; the acc/eva stage delta is clamped to [MIN,MAX]
	// stage exactly as ZM_MoveExecutor.cpp does, and the result is capped at 100.
	u_int g_HitPercent(const ZM_BattleMonster& xAtk, const ZM_BattleMonster& xDef, const ZM_MoveData& xMove)
	{
		if (xMove.m_uAccuracy == uZM_MOVE_ACCURACY_ALWAYS_HITS)
		{
			return 100u;
		}
		int iAccStage = xAtk.m_aiStage[ZM_BATTLE_STAT_ACCURACY] - xDef.m_aiStage[ZM_BATTLE_STAT_EVASION];
		if (iAccStage > iZM_MAX_STAGE) { iAccStage = iZM_MAX_STAGE; }
		if (iAccStage < iZM_MIN_STAGE) { iAccStage = iZM_MIN_STAGE; }
		const u_int uEff = ZM_ApplyStatStage(xMove.m_uAccuracy, iAccStage);
		return (uEff > 100u) ? 100u : uEff;
	}

	// -- GREEDY score (section 4.4) -- expected-damage x accuracy, pure integer.
	u_int g_MoveScore(const ZM_BattleMonster& xAtk, const ZM_BattleMonster& xDef, u_int uMoveSlot)
	{
		const ZM_MoveData& xMove = ZM_GetMoveData(xAtk.m_axMoves[uMoveSlot].m_eMove);
		const u_int uDmg = g_DeterministicDamage(xAtk, xDef, uMoveSlot, uZM_AI_ROLL_EXPECTED);
		return uDmg * g_HitPercent(xAtk, xDef, xMove);
	}

	// Best legal MOVE slot for xAtk vs xDef by GREEDY score; tie-break lowest slot
	// (ascending scan + strict >). Returns uZM_AI_NO_MOVE if no legal MOVE exists.
	u_int g_BestGreedyMoveSlot(const ZM_BattleMonster& xAtk, const ZM_BattleMonster& xDef)
	{
		u_int uBestSlot = uZM_AI_NO_MOVE;
		u_int uBestScore = 0u;
		bool bAny = false;
		for (u_int s = 0u; s < uZM_MAX_MOVES; ++s)
		{
			if (!g_IsLegalMoveSlot(xAtk, s))
			{
				continue;
			}
			const u_int uScore = g_MoveScore(xAtk, xDef, s);
			if (!bAny || uScore > uBestScore)
			{
				bAny = true;
				uBestSlot = s;
				uBestScore = uScore;
			}
		}
		return uBestSlot;
	}

	// -- Effectiveness helpers for the SMART hopeless-matchup test (section 5.3) --

	// Max multiplier the opponent's STAB proxy (its own species types) lands on a
	// defender of (eDefType0,eDefType1). Skips the empty ZM_TYPE_NONE slot.
	u_int g_MaxIncomingEffectiveness(const ZM_BattleMonster& xOpp, ZM_TYPE eDefType0, ZM_TYPE eDefType1)
	{
		const ZM_SpeciesData& xOppSpecies = ZM_GetSpeciesData(xOpp.m_eSpecies);
		u_int uMax = 0u;
		for (u_int i = 0u; i < 2u; ++i)
		{
			const ZM_TYPE eType = xOppSpecies.m_aeTypes[i];
			if (eType == ZM_TYPE_NONE)
			{
				continue;
			}
			const u_int uEff = ZM_EffectivenessPercent(eType, eDefType0, eDefType1);
			if (uEff > uMax)
			{
				uMax = uEff;
			}
		}
		return uMax;
	}

	// My best offensive multiplier vs the opponent, over my legal damaging moves.
	// 0 when I have no damaging move (treated as "cannot hit" -> hopeless-eligible).
	u_int g_BestOffensiveEffectiveness(const ZM_BattleMonster& xMe, const ZM_BattleMonster& xOpp)
	{
		const ZM_SpeciesData& xOppSpecies = ZM_GetSpeciesData(xOpp.m_eSpecies);
		u_int uMax = 0u;
		for (u_int s = 0u; s < uZM_MAX_MOVES; ++s)
		{
			if (!g_IsLegalMoveSlot(xMe, s))
			{
				continue;
			}
			const ZM_MoveData& xMove = ZM_GetMoveData(xMe.m_axMoves[s].m_eMove);
			if (!g_IsDamagingMove(xMove))
			{
				continue;
			}
			const u_int uEff = ZM_EffectivenessPercent(xMove.m_eType, xOppSpecies.m_aeTypes[0], xOppSpecies.m_aeTypes[1]);
			if (uEff > uMax)
			{
				uMax = uEff;
			}
		}
		return uMax;
	}

	// ========================================================================
	// Per-tier policies. `eOther` is precomputed by ZM_ChooseAction.
	// ========================================================================

	// -- RANDOM (section 5.1) -- uniform over the ordered legal set; the ONLY tier
	// that draws, and it draws from xAIRng, never xState.m_xRNG.
	ZM_BattleAction g_ChooseRandom(const ZM_BattleState& xState, ZM_SIDE eSide, ZM_BattleRNG& xAIRng)
	{
		ZM_BattleAction axLegal[uZM_MAX_MOVES + uZM_MAX_PARTY_SIZE];
		const u_int uCount = g_EnumerateLegal(xState, eSide, axLegal);
		if (uCount == 0u)
		{
			return g_LowestSwitchOrFallback(xState, eSide);   // degenerate: no draw
		}
		const u_int uIdx = xAIRng.RandBelow(uCount);
		return axLegal[uIdx];
	}

	// -- GREEDY (section 5.2) -- highest expected-damage move; never switches
	// voluntarily. Fully deterministic. Switches only if there is no legal MOVE.
	ZM_BattleAction g_ChooseGreedy(const ZM_BattleState& xState, ZM_SIDE eSide, ZM_SIDE eOther)
	{
		const ZM_BattleMonster& xMe = xState.Side(eSide).Active();
		const ZM_BattleMonster& xOpp = xState.Side(eOther).Active();
		const u_int uSlot = g_BestGreedyMoveSlot(xMe, xOpp);
		if (uSlot != uZM_AI_NO_MOVE)
		{
			return g_MakeMoveAction(uSlot);
		}
		return g_LowestSwitchOrFallback(xState, eSide);
	}

	// -- SMART (section 5.3) -- fixed cascade, first hit wins:
	//   1) take a GUARANTEED KO (worst-case roll 85, sure-hit only);
	//   2) else SWITCH out of a hopeless matchup to a better-typed bench member;
	//   3) else HEAL when strictly below 50% HP;
	//   4) else fall back to GREEDY. No RNG draw.
	ZM_BattleAction g_ChooseSmart(const ZM_BattleState& xState, ZM_SIDE eSide, ZM_SIDE eOther)
	{
		const ZM_BattleSide& xSide = xState.Side(eSide);
		const ZM_BattleMonster& xMe = xSide.Active();
		const ZM_BattleMonster& xOpp = xState.Side(eOther).Active();

		// (1) Guaranteed KO: damaging + sure-hit + worst-case roll reaches target HP.
		{
			u_int uKOSlot = uZM_AI_NO_MOVE;
			u_int uKOScore = 0u;
			bool bKO = false;
			for (u_int s = 0u; s < uZM_MAX_MOVES; ++s)
			{
				if (!g_IsLegalMoveSlot(xMe, s))
				{
					continue;
				}
				const ZM_MoveData& xMove = ZM_GetMoveData(xMe.m_axMoves[s].m_eMove);
				if (!g_IsDamagingMove(xMove))
				{
					continue;
				}
				const bool bSureHit = (xMove.m_uAccuracy == uZM_MOVE_ACCURACY_ALWAYS_HITS) || (xMove.m_uAccuracy >= 100u);
				if (!bSureHit)
				{
					continue;
				}
				const u_int uMinDmg = g_DeterministicDamage(xMe, xOpp, s, uZM_AI_ROLL_MIN);
				if (uMinDmg >= xOpp.m_uCurHP)
				{
					const u_int uScore = g_MoveScore(xMe, xOpp, s);
					if (!bKO || uScore > uKOScore)
					{
						bKO = true;
						uKOSlot = s;
						uKOScore = uScore;
					}
				}
			}
			if (bKO)
			{
				return g_MakeMoveAction(uKOSlot);
			}
		}

		// (2) Switch out of a hopeless matchup.
		if (xSide.FindLowestSwitchTarget() != uZM_MAX_PARTY_SIZE)
		{
			const ZM_SpeciesData& xMeSpecies = ZM_GetSpeciesData(xMe.m_eSpecies);
			const u_int uEffIn = g_MaxIncomingEffectiveness(xOpp, xMeSpecies.m_aeTypes[0], xMeSpecies.m_aeTypes[1]);
			const u_int uEffOut = g_BestOffensiveEffectiveness(xMe, xOpp);
			if (uEffIn >= 200u && uEffOut <= 100u)
			{
				u_int uBestBench = uZM_MAX_PARTY_SIZE;
				u_int uBestEffIn = uEffIn;   // require strictly smaller incoming effectiveness
				const u_int uParty = xSide.m_xParty.GetSize();
				for (u_int j = 0u; j < uParty; ++j)
				{
					if (!xSide.CanSwitchTo(j))
					{
						continue;
					}
					const ZM_BattleMonster& xBench = xSide.m_xParty.Get(j);
					const ZM_SpeciesData& xBenchSpecies = ZM_GetSpeciesData(xBench.m_eSpecies);
					const u_int uEffInJ = g_MaxIncomingEffectiveness(xOpp, xBenchSpecies.m_aeTypes[0], xBenchSpecies.m_aeTypes[1]);
					if (uEffInJ < uBestEffIn)   // strictly smaller; ascending scan keeps lowest slot on ties
					{
						uBestEffIn = uEffInJ;
						uBestBench = j;
					}
				}
				if (uBestBench != uZM_MAX_PARTY_SIZE)
				{
					return g_MakeSwitchAction(uBestBench);
				}
				// no bench improves the matchup: fall through (do not switch).
			}
		}

		// (3) Heal when strictly below 50% HP.
		if (xMe.m_uCurHP * 2u < xMe.m_auMaxStat[ZM_STAT_HP])
		{
			for (u_int s = 0u; s < uZM_MAX_MOVES; ++s)
			{
				if (!g_IsLegalMoveSlot(xMe, s))
				{
					continue;
				}
				const ZM_MoveData& xMove = ZM_GetMoveData(xMe.m_axMoves[s].m_eMove);
				if (xMove.m_eEffect == ZM_MOVE_EFFECT_HEAL_HALF || xMove.m_eEffect == ZM_MOVE_EFFECT_REST)
				{
					return g_MakeMoveAction(s);   // lowest slot among heal moves
				}
			}
		}

		// (4) Fall back to GREEDY.
		return g_ChooseGreedy(xState, eSide, eOther);
	}

	// -- CHAMPION (section 5.4) -- 2-ply deterministic forward model on scalar HP.
	// Models ONE opponent reply (its GREEDY move from the current position) once,
	// up front, then for each own candidate move resolves order by the engine's
	// real priority / effective-speed rule (exact speed tie => opponent first) and
	// scores the resulting position with the static eval. No clone, no RNG, no
	// executor -- it applies deterministic mean damage directly to two scalars.
	ZM_BattleAction g_ChooseChampion(const ZM_BattleState& xState, ZM_SIDE eSide, ZM_SIDE eOther)
	{
		const ZM_BattleMonster& xMe = xState.Side(eSide).Active();
		const ZM_BattleMonster& xOpp = xState.Side(eOther).Active();

		// Model the opponent's single reply once (its GREEDY move: atk=opp, def=me).
		const u_int uReplySlot = g_BestGreedyMoveSlot(xOpp, xMe);
		int iPrioR = 0;
		u_int uDmgR = 0u;
		if (uReplySlot != uZM_AI_NO_MOVE)
		{
			iPrioR = ZM_GetMoveData(xOpp.m_axMoves[uReplySlot].m_eMove).m_iPriority;
			uDmgR = g_DeterministicDamage(xOpp, xMe, uReplySlot, uZM_AI_ROLL_EXPECTED);
		}

		const u_int uSpeedMe = g_EffectiveSpeed(xMe);
		const u_int uSpeedOpp = g_EffectiveSpeed(xOpp);

		u_int uBestSlot = uZM_AI_NO_MOVE;
		int  iBestValue = 0;
		u_int uBestScore = 0u;
		bool bAny = false;

		for (u_int a = 0u; a < uZM_MAX_MOVES; ++a)
		{
			if (!g_IsLegalMoveSlot(xMe, a))
			{
				continue;
			}
			const int iPrioA = ZM_GetMoveData(xMe.m_axMoves[a].m_eMove).m_iPriority;
			const u_int uDmgA = g_DeterministicDamage(xMe, xOpp, a, uZM_AI_ROLL_EXPECTED);

			// AI acts first iff higher priority bracket, or equal bracket and
			// strictly faster. Exact speed tie => opponent first (conservative).
			const bool bAIFirst = (iPrioA > iPrioR)
				|| (iPrioA == iPrioR && uSpeedMe > uSpeedOpp);

			u_int uHpMe = xMe.m_uCurHP;
			u_int uHpOpp = xOpp.m_uCurHP;
			if (bAIFirst)
			{
				uHpOpp -= (uDmgA < uHpOpp) ? uDmgA : uHpOpp;
				if (uHpOpp != 0u)               // opponent survives -> it replies
				{
					uHpMe -= (uDmgR < uHpMe) ? uDmgR : uHpMe;
				}
			}
			else
			{
				uHpMe -= (uDmgR < uHpMe) ? uDmgR : uHpMe;
				if (uHpMe != 0u)                // AI survives -> its move lands
				{
					uHpOpp -= (uDmgA < uHpOpp) ? uDmgA : uHpOpp;
				}
				// else the AI faints before acting; `a` never lands (uHpOpp unchanged).
			}

			int iValue = (int)uHpMe - (int)uHpOpp;
			if (uHpOpp == 0u) { iValue += iZM_AI_FAINT_VALUE; }
			if (uHpMe == 0u) { iValue -= iZM_AI_FAINT_VALUE; }

			// Pick max V; tie-break higher GREEDY score, then lowest slot.
			const u_int uScore = g_MoveScore(xMe, xOpp, a);
			if (!bAny
				|| iValue > iBestValue
				|| (iValue == iBestValue && uScore > uBestScore))
			{
				bAny = true;
				uBestSlot = a;
				iBestValue = iValue;
				uBestScore = uScore;
			}
		}

		if (uBestSlot != uZM_AI_NO_MOVE)
		{
			return g_MakeMoveAction(uBestSlot);
		}
		return g_LowestSwitchOrFallback(xState, eSide);
	}
}

ZM_BattleAction ZM_ChooseAction(const ZM_BattleState& xState, ZM_SIDE eSide,
	ZM_AI_TIER eTier, ZM_BattleRNG& xAIRng)
{
	const ZM_SIDE eOther = (eSide == ZM_SIDE_PLAYER) ? ZM_SIDE_ENEMY : ZM_SIDE_PLAYER;
	switch (eTier)
	{
	case ZM_AI_TIER_RANDOM:
		return g_ChooseRandom(xState, eSide, xAIRng);
	case ZM_AI_TIER_GREEDY:
		return g_ChooseGreedy(xState, eSide, eOther);
	case ZM_AI_TIER_SMART:
		return g_ChooseSmart(xState, eSide, eOther);
	case ZM_AI_TIER_CHAMPION:
		return g_ChooseChampion(xState, eSide, eOther);
	default:
		// ZM_AI_TIER_COUNT / _NONE are not valid choose-time tiers; fail loudly in
		// tools builds, but still return a well-formed action for release safety.
		Zenith_Assert(false, "ZM_ChooseAction: invalid AI tier");
		return g_ChooseGreedy(xState, eSide, eOther);
	}
}
