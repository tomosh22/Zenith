#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_MoveExecutor.h"
#include "Zenithmon/Source/Battle/ZM_DamageCalc.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"

// ============================================================================
// ZM_MoveExecutor -- SC1 seam + SC2 stat-stage effects & category dispatch.
// Draw discipline (locked): per move -- accuracy (only if the move can miss) ->
// [STATUS-category primary: apply effect, no further draws] OR [damaging: immunity
// short-circuit -> crit -> damage-roll -> E3 secondary proc]. Emit order per
// connecting damaging hit: MOVE_USED -> { MISS | IMMUNE | ([CRIT] [SUPER|NOT]
// DAMAGE_DEALT [FAINT]) [STAT_STAGE_CHANGED] }. The acc/eva stat-stage fold reduces
// to the base-accuracy check at stage 0 (box 1 is always stage 0), and the E3
// secondary proc draws NOTHING for a NONE-effect move, so box-1 goldens never move.
//
// SC2 dispatch: Move().m_eCategory picks PRIMARY (STATUS-category, chance 100, no
// crit/roll/immunity/proc draws) vs SECONDARY (damaging move: ApplyDamagingHit
// first, then a RandBelow(100)<chance proc gate applies the stat change if the
// target survived). Both funnel every stat change through g_ApplyStatChange (the
// single clamp+emit site: STAT_STAGE_CHANGED, or MOVE_FAILED(STAT_MAXED) for a
// single-stat primary already at the +/-6 cap; a maxed secondary is silent; a
// multi-stat raise caps each stat silently and fails as a whole only if NOTHING
// moved). RAISE_CRIT adds the row magnitude to the attacker's m_iCritStage counter
// (cap 3) and emits nothing -- it has no ZM_BATTLE_STAT slot.
// ============================================================================

// ---- ZM_MoveContext accessors -- live resolution against the engine-owned state
ZM_BattleMonster& ZM_MoveContext::Atk()     { return m_pxState->Side(m_eAtk).Active(); }
ZM_BattleMonster& ZM_MoveContext::Def()     { return m_pxState->Side(m_eDef).Active(); }
ZM_BattleSide&    ZM_MoveContext::AtkSide() { return m_pxState->Side(m_eAtk); }
ZM_BattleSide&    ZM_MoveContext::DefSide() { return m_pxState->Side(m_eDef); }
ZM_BattleRNG&     ZM_MoveContext::RNG()     { return m_pxState->m_xRNG; }

const ZM_MoveData& ZM_MoveContext::Move() const
{
	return ZM_GetMoveData(m_pxState->Side(m_eAtk).Active().m_axMoves[m_uMoveSlot].m_eMove);
}

void ZM_MoveContext::Emit(const ZM_BattleEvent& xEvent)
{
	m_pxEvents->PushBack(xEvent);
}

// Emit FAINT once, exactly when the given side's active monster has hit 0 HP. The
// caller has already applied damage; this mirrors the engine's post-hit faint emit
// (side, active slot). Battle-over detection stays in ResolveTurn.
void ZM_MoveContext::MaybeFaint(ZM_SIDE eSide)
{
	ZM_BattleSide& xSide = m_pxState->Side(eSide);
	if (xSide.Active().m_uCurHP == 0u)
	{
		Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, eSide, xSide.m_uActiveSlot));
	}
}

// ---- SC2 stat-stage helpers (file-local; the executor's grouped effect body) ---

// True iff eEffect is one of the SC2 stat-stage kinds (LOWER_*/RAISE_*/RAISE_CRIT/
// the multi-stat RAISE_* combos). Non-stat kinds are routed by other sub-commits.
static bool g_IsStatEffect(ZM_MOVE_EFFECT eEffect)
{
	switch (eEffect)
	{
	case ZM_MOVE_EFFECT_LOWER_ATTACK:
	case ZM_MOVE_EFFECT_LOWER_DEFENSE:
	case ZM_MOVE_EFFECT_LOWER_SPATTACK:
	case ZM_MOVE_EFFECT_LOWER_SPDEFENSE:
	case ZM_MOVE_EFFECT_LOWER_SPEED:
	case ZM_MOVE_EFFECT_LOWER_ACCURACY:
	case ZM_MOVE_EFFECT_LOWER_EVASION:
	case ZM_MOVE_EFFECT_RAISE_ATTACK:
	case ZM_MOVE_EFFECT_RAISE_DEFENSE:
	case ZM_MOVE_EFFECT_RAISE_SPATTACK:
	case ZM_MOVE_EFFECT_RAISE_SPDEFENSE:
	case ZM_MOVE_EFFECT_RAISE_SPEED:
	case ZM_MOVE_EFFECT_RAISE_EVASION:
	case ZM_MOVE_EFFECT_RAISE_CRIT:
	case ZM_MOVE_EFFECT_RAISE_ATTACK_SPEED:
	case ZM_MOVE_EFFECT_RAISE_ATTACK_DEFENSE:
	case ZM_MOVE_EFFECT_RAISE_SPATK_SPDEF:
	case ZM_MOVE_EFFECT_RAISE_DEF_SPDEF:
	case ZM_MOVE_EFFECT_RAISE_ALL:
		return true;
	default:
		return false;
	}
}

// Apply ONE stat-stage delta to a target and emit. Clamp to [iZM_MIN_STAGE,
// iZM_MAX_STAGE]. Returns whether the stage actually moved (so a multi-stat caller
// can count changes and decide a single whole-move failure). If the stage is already
// at the cap in the requested direction (new == old): a single-stat STATUS-category
// PRIMARY emits MOVE_FAILED(STAT_MAXED) on the TARGET (the mon whose stage is capped);
// a damaging SECONDARY -- and any per-stat cap of a MULTI-stat effect (passed
// bPrimary=false) -- is silent. Otherwise set the stage and emit STAT_STAGE_CHANGED
// with m_iAmount = the ACTUAL signed delta applied (new-old, may be smaller than
// iDelta near the cap) and m_iAux = the stat.
static bool g_ApplyStatChange(ZM_MoveContext& xCtx, ZM_SIDE eTgt, ZM_BATTLE_STAT eStat,
	int iDelta, bool bPrimary)
{
	ZM_BattleSide&    xTgtSide = xCtx.m_pxState->Side(eTgt);
	ZM_BattleMonster& xTgt     = xTgtSide.Active();

	const int iOld = xTgt.m_aiStage[eStat];
	int iNew = iOld + iDelta;
	if (iNew > iZM_MAX_STAGE) { iNew = iZM_MAX_STAGE; }
	if (iNew < iZM_MIN_STAGE) { iNew = iZM_MIN_STAGE; }

	if (iNew == iOld)
	{
		// Already capped in the requested direction: a single-stat primary announces
		// the failure; a secondary proc / multi-stat per-stat cap is silent.
		if (bPrimary)
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, eTgt, xTgtSide.m_uActiveSlot,
				ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MOVE_FAIL_STAT_MAXED));
		}
		return false;
	}

	xTgt.m_aiStage[eStat] = iNew;
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_STAT_STAGE_CHANGED, eTgt, xTgtSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, iNew - iOld, (int)eStat));
	return true;
}

// Apply a MULTI-stat self-RAISE (RAISE_ATTACK_SPEED / RAISE_ALL / ...): raise each
// listed stat on the attacker by +iMagnitude. A per-stat cap is SILENT here -- each
// g_ApplyStatChange is invoked as a NON-primary so it emits no per-stat MOVE_FAILED;
// instead a SINGLE whole-move MOVE_FAILED(STAT_MAXED) fires only for a PRIMARY whose
// every target stat was already capped (nothing changed). Each stat that DID move
// still emits its own STAT_STAGE_CHANGED, in list order (so a partial cap is a clean
// "the rest changed" stream, never a contradictory failed-AND-changed one).
static void g_ApplyMultiStatRaise(ZM_MoveContext& xCtx, const ZM_BATTLE_STAT* peStats,
	u_int uCount, int iMagnitude, bool bPrimary)
{
	const ZM_SIDE eAtk = xCtx.m_eAtk;
	u_int uChanged = 0u;
	for (u_int i = 0u; i < uCount; ++i)
	{
		if (g_ApplyStatChange(xCtx, eAtk, peStats[i], +iMagnitude, /*bPrimary*/ false))
		{
			++uChanged;
		}
	}
	if (bPrimary && uChanged == 0u)
	{
		// Every target stat was already capped -> the move as a whole did nothing.
		ZM_BattleSide& xSelfSide = xCtx.m_pxState->Side(eAtk);
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, eAtk, xSelfSide.m_uActiveSlot,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MOVE_FAIL_STAT_MAXED));
	}
}

// Route a stat-effect kind to its stat(s)/direction/target and apply each via
// g_ApplyStatChange. iMagnitude is the stage count (1..3); LOWER applies -mag to the
// OPPONENT, RAISE applies +mag to SELF (per the kind, matching the row's m_eTarget).
// RAISE_CRIT has no ZM_BATTLE_STAT slot: it adds iMagnitude to the attacker's
// m_iCritStage counter (cap 3) and emits NO event (documented ZM-D-033 SC2 choice).
static void g_ApplyStatEffect(ZM_MoveContext& xCtx, ZM_MOVE_EFFECT eEffect, int iMagnitude, bool bPrimary)
{
	const ZM_SIDE eAtk = xCtx.m_eAtk;
	const ZM_SIDE eDef = xCtx.m_eDef;

	switch (eEffect)
	{
	// ---- LOWER (opponent) ----
	case ZM_MOVE_EFFECT_LOWER_ATTACK:    g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_ATTACK,    -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_DEFENSE:   g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_DEFENSE,   -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_SPATTACK:  g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_SPATTACK,  -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_SPDEFENSE: g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_SPDEFENSE, -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_SPEED:     g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_SPEED,     -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_ACCURACY:  g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_ACCURACY,  -iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_LOWER_EVASION:   g_ApplyStatChange(xCtx, eDef, ZM_BATTLE_STAT_EVASION,   -iMagnitude, bPrimary); break;

	// ---- RAISE (self) ----
	case ZM_MOVE_EFFECT_RAISE_ATTACK:    g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_ATTACK,    +iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_RAISE_DEFENSE:   g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_DEFENSE,   +iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_RAISE_SPATTACK:  g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_SPATTACK,  +iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_RAISE_SPDEFENSE: g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_SPDEFENSE, +iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_RAISE_SPEED:     g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_SPEED,     +iMagnitude, bPrimary); break;
	case ZM_MOVE_EFFECT_RAISE_EVASION:   g_ApplyStatChange(xCtx, eAtk, ZM_BATTLE_STAT_EVASION,   +iMagnitude, bPrimary); break;

	// ---- multi-stat RAISE (self, each named stat) -- silent per-stat cap, one
	//      whole-move MOVE_FAILED only if EVERY listed stat was already capped ----
	case ZM_MOVE_EFFECT_RAISE_ATTACK_SPEED:
	{
		const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_ATTACK, ZM_BATTLE_STAT_SPEED };
		g_ApplyMultiStatRaise(xCtx, aeStats, 2u, iMagnitude, bPrimary);
		break;
	}
	case ZM_MOVE_EFFECT_RAISE_ATTACK_DEFENSE:
	{
		const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_ATTACK, ZM_BATTLE_STAT_DEFENSE };
		g_ApplyMultiStatRaise(xCtx, aeStats, 2u, iMagnitude, bPrimary);
		break;
	}
	case ZM_MOVE_EFFECT_RAISE_SPATK_SPDEF:
	{
		const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_SPATTACK, ZM_BATTLE_STAT_SPDEFENSE };
		g_ApplyMultiStatRaise(xCtx, aeStats, 2u, iMagnitude, bPrimary);
		break;
	}
	case ZM_MOVE_EFFECT_RAISE_DEF_SPDEF:
	{
		const ZM_BATTLE_STAT aeStats[] = { ZM_BATTLE_STAT_DEFENSE, ZM_BATTLE_STAT_SPDEFENSE };
		g_ApplyMultiStatRaise(xCtx, aeStats, 2u, iMagnitude, bPrimary);
		break;
	}
	case ZM_MOVE_EFFECT_RAISE_ALL:   // all five battle stats (not accuracy/evasion)
	{
		const ZM_BATTLE_STAT aeStats[] = {
			ZM_BATTLE_STAT_ATTACK, ZM_BATTLE_STAT_DEFENSE, ZM_BATTLE_STAT_SPATTACK,
			ZM_BATTLE_STAT_SPDEFENSE, ZM_BATTLE_STAT_SPEED };
		g_ApplyMultiStatRaise(xCtx, aeStats, 5u, iMagnitude, bPrimary);
		break;
	}

	// ---- RAISE_CRIT (self, counter; no stat slot, no event) ----
	case ZM_MOVE_EFFECT_RAISE_CRIT:
	{
		// Add the row magnitude (Killer Focus = +2), still capped at 3. No event/draw.
		ZM_BattleMonster& xSelf = xCtx.m_pxState->Side(eAtk).Active();
		xSelf.m_iCritStage += iMagnitude;
		if (xSelf.m_iCritStage > 3) { xSelf.m_iCritStage = 3; }
		break;
	}

	default:
		break;   // not a stat kind (routed by another sub-commit)
	}
}

// ---- SC3 field / side setters (STATUS-category primary; STATE-ONLY, no event) ---

// True iff eEffect is one of the SC3 field/side setters (weather / screen / hazard).
static bool g_IsFieldEffect(ZM_MOVE_EFFECT eEffect)
{
	switch (eEffect)
	{
	case ZM_MOVE_EFFECT_WEATHER_RAIN:
	case ZM_MOVE_EFFECT_WEATHER_SUN:
	case ZM_MOVE_EFFECT_WEATHER_SAND:
	case ZM_MOVE_EFFECT_WEATHER_SNOW:
	case ZM_MOVE_EFFECT_SCREEN_PHYSICAL:
	case ZM_MOVE_EFFECT_SCREEN_SPECIAL:
	case ZM_MOVE_EFFECT_HAZARD_SPIKES:
		return true;
	default:
		return false;
	}
}

// Apply a field/side setter. STATE-ONLY: box 2 SETS the field/side condition but
// emits NO event and does NOT count down -- box 3 owns WEATHER_CHANGED/SCREEN_SET/
// SCREEN_EXPIRED, the countdown/expiry, and the weather DAMAGE multiplier (ZM-D-033
// weather line). The ability-independent screen reduction rides bScreen in
// ApplyDamagingHit, so a screen set here is immediately observable via halved damage.
//   WEATHER_* : field m_eWeather + m_uWeatherTurns = 5.
//   SCREEN_*  : the ATTACKER's side m_auScreenTurns[matching] = 5 (protects the user's side).
//   HAZARD_SPIKES : the DEFENDER side m_uHazardSpikeLayers ++, capped at 3.
static void g_ApplyField(ZM_MoveContext& xCtx)
{
	ZM_FieldState& xField = xCtx.m_pxState->m_xField;
	switch (xCtx.Move().m_eEffect)
	{
	case ZM_MOVE_EFFECT_WEATHER_RAIN: xField.m_eWeather = ZM_WEATHER_RAIN; xField.m_uWeatherTurns = 5u; break;
	case ZM_MOVE_EFFECT_WEATHER_SUN:  xField.m_eWeather = ZM_WEATHER_SUN;  xField.m_uWeatherTurns = 5u; break;
	case ZM_MOVE_EFFECT_WEATHER_SAND: xField.m_eWeather = ZM_WEATHER_SAND; xField.m_uWeatherTurns = 5u; break;
	case ZM_MOVE_EFFECT_WEATHER_SNOW: xField.m_eWeather = ZM_WEATHER_SNOW; xField.m_uWeatherTurns = 5u; break;

	case ZM_MOVE_EFFECT_SCREEN_PHYSICAL: xCtx.AtkSide().m_auScreenTurns[ZM_SCREEN_PHYSICAL] = 5u; break;
	case ZM_MOVE_EFFECT_SCREEN_SPECIAL:  xCtx.AtkSide().m_auScreenTurns[ZM_SCREEN_SPECIAL]  = 5u; break;

	case ZM_MOVE_EFFECT_HAZARD_SPIKES:
	{
		ZM_BattleSide& xDefSide = xCtx.DefSide();
		if (xDefSide.m_uHazardSpikeLayers < 3u) { ++xDefSide.m_uHazardSpikeLayers; }
		break;
	}
	default:
		break;
	}
}

// HEAL_HALF: a STATUS-category SELF-heal primary (no crit/roll/proc draw). Heals the
// attacker by floor(maxHP * mag / 100), capped to the missing HP, and ALWAYS emits
// HEAL (m_iAmount = the ACTUAL amount added, m_iAux = attacker new HP) -- even a
// heal-for-0 at full HP emits HEAL(0, maxHP) rather than a MOVE_FAILED. (This is the
// self-heal STATUS event kind; DRAIN's damaging self-heal keeps the separate DRAIN
// kind.) m_iEffectMagnitude is read as a percent for HEAL_HALF (ZM_MoveData).
static void g_ApplyHealHalf(ZM_MoveContext& xCtx)
{
	ZM_BattleMonster& xAtk     = xCtx.Atk();
	ZM_BattleSide&    xAtkSide = xCtx.AtkSide();
	const u_int uMaxHP   = xAtk.m_auMaxStat[ZM_STAT_HP];
	const u_int uMissing = uMaxHP - xAtk.m_uCurHP;          // curHP <= maxHP invariant: no underflow
	u_int uHeal = (uMaxHP * (u_int)xCtx.Move().m_iEffectMagnitude) / 100u;
	if (uHeal > uMissing) { uHeal = uMissing; }
	xAtk.m_uCurHP += uHeal;
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_HEAL, xCtx.m_eAtk, xAtkSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uHeal, (int)xAtk.m_uCurHP));
}

// STATUS-category PRIMARY: the effect IS the point of the move (chance 100). SC2
// lights the stat-stage kinds; SC3 adds the field/side setters (state-only) and the
// HEAL_HALF self-heal. Other STATUS-category effects (status / volatile / cure) light
// up in later sub-commits and are a MOVE_USED-only no-op for now.
static void g_ApplyStatusCategoryPrimary(ZM_MoveContext& xCtx)
{
	const ZM_MoveData& xMove = xCtx.Move();
	if (g_IsStatEffect(xMove.m_eEffect))
	{
		g_ApplyStatEffect(xCtx, xMove.m_eEffect, xMove.m_iEffectMagnitude, /*bPrimary*/ true);
		return;
	}
	if (g_IsFieldEffect(xMove.m_eEffect))
	{
		g_ApplyField(xCtx);
		return;
	}
	if (xMove.m_eEffect == ZM_MOVE_EFFECT_HEAL_HALF)
	{
		g_ApplyHealHalf(xCtx);
		return;
	}
}

// E3 SECONDARY proc for a DAMAGING move that carries a stat effect. Draws NOTHING for
// NONE or a not-yet-lit (status/volatile) secondary, so box-1 NONE moves stay
// byte-identical. Requires the damaged target (defender) to have survived the hit.
// Proc gate: m_uEffectChance >= 100 is unconditional (no draw); else one RandBelow(100).
static void g_ApplyDamagingSecondary(ZM_MoveContext& xCtx)
{
	const ZM_MoveData& xMove = xCtx.Move();
	if (!g_IsStatEffect(xMove.m_eEffect))
	{
		return;   // NONE / non-stat secondary: no proc draw (byte-identical box 1)
	}
	if (xCtx.Def().m_uCurHP == 0u)
	{
		return;   // a fainted target takes no secondary (E3 requires target alive)
	}
	if (xMove.m_uEffectChance < 100u)
	{
		if (xCtx.RNG().RandBelow(100u) >= xMove.m_uEffectChance)
		{
			return;   // proc failed
		}
	}
	g_ApplyStatEffect(xCtx, xMove.m_eEffect, xMove.m_iEffectMagnitude, /*bPrimary*/ false);
}

// ---- SC3 damage-delivery variants ------------------------------------------
// Every one of these is a DAMAGING move (PHYSICAL/SPECIAL): Execute has already
// spent PP + emitted MOVE_USED, passed accuracy, and resolved M6 immunity, so a
// delivery helper is reached ONLY for a non-immune damaging delivery kind. Each
// per-hit damage still funnels through ApplyDamagingHit (the ONE crit/roll/
// effectiveness/DAMAGE_DEALT/FAINT emit-group) so the emit order can never drift.
// Box-1 + SC2 moves are all NONE/stat kinds, so g_IsDeliveryEffect is false for
// them and these paths are unreachable -- the box-1/SC2 goldens stay byte-identical.

// True iff eEffect is one of the SC3 delivery kinds (damaging; routed to
// g_ApplyDelivery). The two-turn / recharge / lock kinds in the ZM_MoveData
// "delivery variants" block (CHARGE_TURN/SEMI_INVULN/RECHARGE/LOCK_IN) are SC5
// volatiles, NOT SC3, and are deliberately excluded here.
static bool g_IsDeliveryEffect(ZM_MOVE_EFFECT eEffect)
{
	switch (eEffect)
	{
	case ZM_MOVE_EFFECT_MULTI_HIT:
	case ZM_MOVE_EFFECT_DOUBLE_HIT:
	case ZM_MOVE_EFFECT_RECOIL:
	case ZM_MOVE_EFFECT_DRAIN:
	case ZM_MOVE_EFFECT_FIXED_LEVEL:
	case ZM_MOVE_EFFECT_HALVE_HP:
	case ZM_MOVE_EFFECT_OHKO:
		return true;
	default:
		return false;
	}
}

// Apply a fixed, unavoidable amount of damage to the defender with NO crit/roll
// draws and NO CRIT/SUPER/NOT effectiveness events (FIXED_LEVEL / HALVE_HP / OHKO):
// immunity was already gated in Execute's M6, so the number lands verbatim. Emits
// DAMAGE_DEALT (m_iAmount=dmg, m_iAux=remaining HP) [+ FAINT].
static void g_ApplyFixedDamage(ZM_MoveContext& xCtx, u_int uDmg)
{
	ZM_BattleMonster& xDef     = xCtx.Def();
	ZM_BattleSide&    xDefSide = xCtx.DefSide();
	xDef.m_uCurHP = (uDmg >= xDef.m_uCurHP) ? 0u : (xDef.m_uCurHP - uDmg);
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, xCtx.m_eDef, xDefSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDmg, (int)xDef.m_uCurHP));
	xCtx.MaybeFaint(xCtx.m_eDef);
}

// MULTI_HIT: E0 hit-count draw RandBelow(8) -> {2,2,2,3,3,3,4,5} (3/8,3/8,1/8,1/8),
// then per-hit ApplyDamagingHit (crit -> roll -> emit-group), breaking on faint.
// Emits MULTI_HIT (m_iAmount = number of hits landed) on the ATTACKER, then the E3
// secondary once after the last hit.
static void g_ApplyMultiHit(ZM_MoveContext& xCtx)
{
	const u_int auHitCount[8] = { 2u, 2u, 2u, 3u, 3u, 3u, 4u, 5u };
	const u_int uPlanned = auHitCount[xCtx.RNG().RandBelow(8u)];

	u_int uLanded = 0u;
	for (u_int i = 0u; i < uPlanned; ++i)
	{
		ZM_MoveExecutor::ApplyDamagingHit(xCtx);
		++uLanded;
		if (xCtx.Def().m_uCurHP == 0u) { break; }   // stop when the target faints
	}

	ZM_BattleSide& xAtkSide = xCtx.AtkSide();
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MULTI_HIT, xCtx.m_eAtk, xAtkSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uLanded));

	g_ApplyDamagingSecondary(xCtx);
}

// DOUBLE_HIT: exactly 2 hits, NO count draw; per-hit ApplyDamagingHit, breaking on
// faint (so a KO on hit 1 never re-draws / re-FAINTs a dead target). Emits MULTI_HIT
// with the landed count (2 when the target survives hit 1 -- the normal case; 1 if
// hit 1 KOs), then the E3 secondary once.
static void g_ApplyDoubleHit(ZM_MoveContext& xCtx)
{
	u_int uLanded = 0u;
	for (u_int i = 0u; i < 2u; ++i)
	{
		ZM_MoveExecutor::ApplyDamagingHit(xCtx);
		++uLanded;
		if (xCtx.Def().m_uCurHP == 0u) { break; }
	}

	ZM_BattleSide& xAtkSide = xCtx.AtkSide();
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MULTI_HIT, xCtx.m_eAtk, xAtkSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uLanded));

	g_ApplyDamagingSecondary(xCtx);
}

// RECOIL: one ApplyDamagingHit, then the attacker takes floor(dmgDealt * mag / 100)
// self-damage (appended AFTER the damage group). Emits RECOIL (m_iAmount = computed
// recoil [RAW, pre-HP-clamp, mirroring DAMAGE_DEALT], m_iAux = attacker HP after
// clamp) on the ATTACKER, then MaybeFaint(attacker), then the E3 secondary.
static void g_ApplyRecoil(ZM_MoveContext& xCtx)
{
	const u_int uDmg = ZM_MoveExecutor::ApplyDamagingHit(xCtx);

	ZM_BattleMonster& xAtk     = xCtx.Atk();
	ZM_BattleSide&    xAtkSide = xCtx.AtkSide();
	const u_int uRecoil = (uDmg * (u_int)xCtx.Move().m_iEffectMagnitude) / 100u;
	xAtk.m_uCurHP = (uRecoil >= xAtk.m_uCurHP) ? 0u : (xAtk.m_uCurHP - uRecoil);

	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_RECOIL, xCtx.m_eAtk, xAtkSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uRecoil, (int)xAtk.m_uCurHP));
	xCtx.MaybeFaint(xCtx.m_eAtk);

	g_ApplyDamagingSecondary(xCtx);
}

// DRAIN: one ApplyDamagingHit, then the attacker heals floor(dmgDealt * mag / 100),
// clamped to maxHP (appended AFTER the damage group). Emits DRAIN (m_iAmount =
// computed heal [RAW, mirroring RECOIL], m_iAux = attacker HP after the max-HP clamp)
// on the ATTACKER, then the E3 secondary. (DRAIN's self-heal uses the DRAIN event
// kind, NOT HEAL; HEAL is reserved for the self-heal STATUS moves in a later commit.)
static void g_ApplyDrain(ZM_MoveContext& xCtx)
{
	const u_int uDmg = ZM_MoveExecutor::ApplyDamagingHit(xCtx);

	ZM_BattleMonster& xAtk     = xCtx.Atk();
	ZM_BattleSide&    xAtkSide = xCtx.AtkSide();
	const u_int uHeal  = (uDmg * (u_int)xCtx.Move().m_iEffectMagnitude) / 100u;
	const u_int uMaxHP = xAtk.m_auMaxStat[ZM_STAT_HP];
	u_int uNewHP = xAtk.m_uCurHP + uHeal;
	if (uNewHP > uMaxHP) { uNewHP = uMaxHP; }
	xAtk.m_uCurHP = uNewHP;

	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_DRAIN, xCtx.m_eAtk, xAtkSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uHeal, (int)uNewHP));

	g_ApplyDamagingSecondary(xCtx);
}

// Route a delivery-effect kind to its handler. FIXED_LEVEL = attacker level;
// HALVE_HP = floor(defender curHP / 2), min 1 while the target is alive; OHKO =
// defender curHP (a full KO -- the OHKO level guard already ran in Execute). All
// three take NO crit/roll draws (g_ApplyFixedDamage).
static void g_ApplyDelivery(ZM_MoveContext& xCtx)
{
	switch (xCtx.Move().m_eEffect)
	{
	case ZM_MOVE_EFFECT_MULTI_HIT:  g_ApplyMultiHit(xCtx);  break;
	case ZM_MOVE_EFFECT_DOUBLE_HIT: g_ApplyDoubleHit(xCtx); break;
	case ZM_MOVE_EFFECT_RECOIL:     g_ApplyRecoil(xCtx);    break;
	case ZM_MOVE_EFFECT_DRAIN:      g_ApplyDrain(xCtx);     break;

	case ZM_MOVE_EFFECT_FIXED_LEVEL:
		g_ApplyFixedDamage(xCtx, xCtx.Atk().m_uLevel);
		break;

	case ZM_MOVE_EFFECT_HALVE_HP:
	{
		const u_int uCur = xCtx.Def().m_uCurHP;
		u_int uDmg = uCur / 2u;
		if (uDmg == 0u && uCur > 0u) { uDmg = 1u; }   // at least 1 while the target lives
		g_ApplyFixedDamage(xCtx, uDmg);
		break;
	}

	case ZM_MOVE_EFFECT_OHKO:
		g_ApplyFixedDamage(xCtx, xCtx.Def().m_uCurHP);   // full KO
		break;

	default:
		break;
	}
}

// ---- The executor ----------------------------------------------------------
void ZM_MoveExecutor::Execute(ZM_MoveContext& xCtx)
{
	ZM_BattleMonster& xAtk     = xCtx.Atk();
	ZM_BattleSide&    xAtkSide = xCtx.AtkSide();
	const ZM_MoveData& xMove   = xCtx.Move();

	ZM_MoveSlot& xMoveSlot = xAtk.m_axMoves[xCtx.m_uMoveSlot];

	// M1: spend PP + announce (box 1 always has PP; NO_PP is a later box).
	--xMoveSlot.m_uCurPP;
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_USED, xCtx.m_eAtk, xAtkSide.m_uActiveSlot, (u_int)xMove.m_eId));

	// OHKO level guard -- runs BEFORE M5 accuracy and draws NOTHING: a one-hit-KO move
	// fails outright against a higher-level target (defender level > attacker level).
	// PP is already spent + MOVE_USED emitted (the move was announced, then failed).
	// MOVE_FAILED is attributed to the DEFENDER (the OHKO's target and the locus of the
	// failure), matching SC2's MOVE_FAILED-on-the-effect-target convention. Box 1/SC2
	// carry no OHKO move, so this guard never fires for them.
	if (xMove.m_eEffect == ZM_MOVE_EFFECT_OHKO && xCtx.Def().m_uLevel > xAtk.m_uLevel)
	{
		ZM_BattleSide& xDefSideOhko = xCtx.DefSide();
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_FAILED, xCtx.m_eDef, xDefSideOhko.m_uActiveSlot,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, (int)ZM_MOVE_FAIL_OHKO_FAILED));
		return;
	}

	// M5: ACCURACY draw -- only if the move CAN miss (sure-hit short-circuit). Applies
	// to STATUS-category and damaging moves alike. effAcc folds the acc/eva stat stages
	// via the SAME ZM_ApplyStatStage idiom; at stage 0 (box 1) the net stage is 0 ->
	// ZM_ApplyStatStage(acc,0) == acc, so this is exactly the base-accuracy check and
	// the box-1 goldens are unchanged.
	if (xMove.m_uAccuracy != uZM_MOVE_ACCURACY_ALWAYS_HITS && xMove.m_uAccuracy < 100u)
	{
		// Fold acc/eva stages, but CLAMP the net stage delta to [iZM_MIN_STAGE,
		// iZM_MAX_STAGE] first: the raw difference ranges [-12,+12], yet a stat stage
		// only ever multiplies within its +/-6 envelope. At stage 0 (box 1) and for
		// |delta| <= 6 (every existing test) the clamp is identity, so no golden moves.
		int iAccStage = xAtk.m_aiStage[ZM_BATTLE_STAT_ACCURACY] - xCtx.Def().m_aiStage[ZM_BATTLE_STAT_EVASION];
		if (iAccStage > iZM_MAX_STAGE) { iAccStage = iZM_MAX_STAGE; }
		if (iAccStage < iZM_MIN_STAGE) { iAccStage = iZM_MIN_STAGE; }
		const u_int uEffAcc = ZM_ApplyStatStage(xMove.m_uAccuracy, iAccStage);
		if (xCtx.RNG().RandBelow(100u) >= uEffAcc)
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_MOVE_MISSED, xCtx.m_eAtk, xAtkSide.m_uActiveSlot, (u_int)xMove.m_eId));
			return;
		}
	}

	// STATUS-category dispatch: the effect is the PRIMARY -- no immunity/crit/roll/proc
	// draws. Apply it directly (chance is effectively 100) and return.
	if (xMove.m_eCategory == ZM_MOVE_CATEGORY_STATUS)
	{
		g_ApplyStatusCategoryPrimary(xCtx);
		return;
	}

	// Damaging move (PHYSICAL/SPECIAL). M6: effectiveness is deterministic (no RNG) --
	// resolve immunity BEFORE any crit draw.
	ZM_BattleMonster&     xDef        = xCtx.Def();
	ZM_BattleSide&        xDefSide    = xCtx.DefSide();
	const ZM_SpeciesData& xDefSpecies = ZM_GetSpeciesData(xDef.m_eSpecies);
	const u_int uEffPct = ZM_EffectivenessPercent(xMove.m_eType, xDefSpecies.m_aeTypes[0], xDefSpecies.m_aeTypes[1]);
	if (uEffPct == 0u)
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_IMMUNE, xCtx.m_eDef, xDefSide.m_uActiveSlot));
		return;
	}

	// PHASE E: delivery dispatch. A delivery-effect damaging kind (multi/double/recoil/
	// drain/fixed/halve/ohko) routes through g_ApplyDelivery -- which still funnels every
	// hit through ApplyDamagingHit. Box-1/SC2 moves are NONE/stat kinds, so this is
	// skipped for them and they take the standard single-hit path below (byte-identical).
	if (g_IsDeliveryEffect(xMove.m_eEffect))
	{
		g_ApplyDelivery(xCtx);
		return;
	}

	// E1/E2: the single damaging emit-group (crit -> roll -> DAMAGE_DEALT -> FAINT).
	ApplyDamagingHit(xCtx);

	// E3: SECONDARY stat proc -- only for a damaging move carrying a stat effect, and
	// only if the target survived. NONE-effect moves take no draw here (box-1 path).
	g_ApplyDamagingSecondary(xCtx);
}

u_int ZM_MoveExecutor::ApplyDamagingHit(ZM_MoveContext& xCtx)
{
	ZM_BattleMonster&  xAtk     = xCtx.Atk();
	ZM_BattleMonster&  xDef     = xCtx.Def();
	ZM_BattleSide&     xDefSide = xCtx.DefSide();
	const ZM_MoveData& xMove    = xCtx.Move();

	const ZM_SpeciesData& xDefSpecies = ZM_GetSpeciesData(xDef.m_eSpecies);
	const u_int uEffPct = ZM_EffectivenessPercent(xMove.m_eType, xDefSpecies.m_aeTypes[0], xDefSpecies.m_aeTypes[1]);

	// (2) CRIT draw. A move with m_uCritStage >= 2 is a guaranteed crit (no draw), as
	// box 1. Otherwise the attacker's accumulated RAISE_CRIT counter sets the rate:
	// 0 -> 1/24, 1 -> 1/8, 2 -> 1/2, >= 3 -> always. At crit stage 0 this is exactly
	// Chance(1,24) -- one RandBelow(24) -- so box-1 goldens (crit stage always 0) are
	// byte-identical. (SC2: the move's own high-crit m_uCritStage==1 is NOT combined
	// with the counter here -- box-1 treated stage 1 as 1/24 too; only >= 2 short-
	// circuits to guaranteed.)
	bool bCrit;
	if (xMove.m_uCritStage >= 2u || xAtk.m_iCritStage >= 3)
	{
		bCrit = true;
	}
	else
	{
		u_int uCritDen = 24u;             // crit stage 0
		if (xAtk.m_iCritStage == 1)      { uCritDen = 8u; }
		else if (xAtk.m_iCritStage == 2) { uCritDen = 2u; }
		bCrit = xCtx.RNG().Chance(1u, uCritDen);
	}
	// (3) DAMAGE ROLL draw -- 85..100 inclusive.
	const u_int uRoll = xCtx.RNG().RandRange(85u, 100u);

	const bool bPhysical = (xMove.m_eCategory == ZM_MOVE_CATEGORY_PHYSICAL);
	const ZM_SpeciesData& xAtkSpecies = ZM_GetSpeciesData(xAtk.m_eSpecies);

	ZM_DamageInput xIn;
	xIn.uLevel  = xAtk.m_uLevel;
	xIn.uPower  = xMove.m_uPower;
	xIn.uAttack = ZM_ApplyStatStage(
		bPhysical ? xAtk.m_auMaxStat[ZM_STAT_ATTACK] : xAtk.m_auMaxStat[ZM_STAT_SPATTACK],
		bPhysical ? xAtk.m_aiStage[ZM_BATTLE_STAT_ATTACK] : xAtk.m_aiStage[ZM_BATTLE_STAT_SPATTACK]);
	xIn.uDefense = ZM_ApplyStatStage(
		bPhysical ? xDef.m_auMaxStat[ZM_STAT_DEFENSE] : xDef.m_auMaxStat[ZM_STAT_SPDEFENSE],
		bPhysical ? xDef.m_aiStage[ZM_BATTLE_STAT_DEFENSE] : xDef.m_aiStage[ZM_BATTLE_STAT_SPDEFENSE]);
	xIn.bStab = (xMove.m_eType == xAtkSpecies.m_aeTypes[0] || xMove.m_eType == xAtkSpecies.m_aeTypes[1]);
	xIn.uEffectivenessPercent = uEffPct;
	xIn.bCrit = bCrit;
	xIn.uRandomPercent = uRoll;
	// bScreen (SC3): a NON-crit hit into an active MATCHING screen on the defender's
	// side (ZM_SCREEN_PHYSICAL for a physical move, ZM_SCREEN_SPECIAL for a special one)
	// halves post-type damage; a crit BYPASSES screens (canonical). box-1/SC2 never set
	// a screen (m_auScreenTurns all 0), so bScreen stays false and their goldens are
	// byte-identical. The weather (1/1) and burn seams keep their box-1 identity defaults.
	const ZM_SCREEN eScreen = bPhysical ? ZM_SCREEN_PHYSICAL : ZM_SCREEN_SPECIAL;
	xIn.bScreen = (xDefSide.m_auScreenTurns[eScreen] > 0u) && !bCrit;

	const u_int uDmg = ZM_CalcDamage(xIn);

	if (bCrit)
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_CRIT, xCtx.m_eDef, xDefSide.m_uActiveSlot));
	}
	if (uEffPct > 100u)
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_SUPER_EFFECTIVE, xCtx.m_eDef, xDefSide.m_uActiveSlot,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uEffPct));
	}
	else if (uEffPct < 100u)
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_NOT_EFFECTIVE, xCtx.m_eDef, xDefSide.m_uActiveSlot,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uEffPct));
	}

	xDef.m_uCurHP = (uDmg >= xDef.m_uCurHP) ? 0u : (xDef.m_uCurHP - uDmg);
	xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_DAMAGE_DEALT, xCtx.m_eDef, xDefSide.m_uActiveSlot,
		ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)uDmg, (int)xDef.m_uCurHP));

	xCtx.MaybeFaint(xCtx.m_eDef);   // emits FAINT iff HP hit 0; battle-over decided in ResolveTurn

	return uDmg;
}
