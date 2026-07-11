#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_AbilityHooks.h"
#include "Zenithmon/Source/Battle/ZM_StatusLogic.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"

// ============================================================================
// SC2 lights SWITCH_IN; SC3 adds deterministic stat, damage and type-interaction
// hooks. Later ordered box-3 sub-commits fill the remaining slots in this same
// const table. A NONE ability resolves to no dispatch and no event.
// ============================================================================

namespace
{
	static const u_int uZM_ABILITY_WEATHER_TURNS = 5u;

	void g_EmitAbilityTrigger(const ZM_AbilityContext& xCtx)
	{
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_ABILITY_TRIGGER, xCtx.m_eSelf,
			xCtx.SelfSide().m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE, 0, 0,
			(int)xCtx.SelfMon().m_eAbility));
	}

	void g_SetWeatherOnSwitchIn(ZM_AbilityContext& xCtx, ZM_WEATHER eWeather)
	{
		ZM_FieldState& xField = xCtx.m_pxState->m_xField;
		if (xField.m_eWeather == eWeather)
		{
			return;   // identical callers never refresh or re-announce
		}

		const ZM_WEATHER ePrevious = xField.m_eWeather;
		xField.m_eWeather = eWeather;
		xField.m_uWeatherTurns = uZM_ABILITY_WEATHER_TURNS;
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_WEATHER_CHANGED, ZM_SIDE_COUNT, 0u,
			ZM_MOVE_NONE, ZM_SPECIES_NONE, (int)eWeather,
			(int)uZM_ABILITY_WEATHER_TURNS, (int)ePrevious));
		g_EmitAbilityTrigger(xCtx);
	}

	void g_DauntingRoarOnSwitchIn(ZM_AbilityContext& xCtx)
	{
		ZM_StatusLogic::ApplyStatChange(*xCtx.m_pxState, *xCtx.m_pxEvents,
			xCtx.m_eOther, ZM_BATTLE_STAT_ATTACK, -1, false, true);
		g_EmitAbilityTrigger(xCtx);
	}

	void g_RaincallerOnSwitchIn(ZM_AbilityContext& xCtx)
	{
		g_SetWeatherOnSwitchIn(xCtx, ZM_WEATHER_RAIN);
	}

	void g_SuncallerOnSwitchIn(ZM_AbilityContext& xCtx)
	{
		g_SetWeatherOnSwitchIn(xCtx, ZM_WEATHER_SUN);
	}

	void g_SandcallerOnSwitchIn(ZM_AbilityContext& xCtx)
	{
		g_SetWeatherOnSwitchIn(xCtx, ZM_WEATHER_SAND);
	}

	void g_SnowcallerOnSwitchIn(ZM_AbilityContext& xCtx)
	{
		g_SetWeatherOnSwitchIn(xCtx, ZM_WEATHER_SNOW);
	}

	void g_PressureAuraOnSwitchIn(ZM_AbilityContext& xCtx)
	{
		g_EmitAbilityTrigger(xCtx);
	}

	u_int g_SurgeDamageDealt(const ZM_AbilityContext& xCtx,
		const ZM_MoveData& xMove, u_int uDamage, ZM_TYPE eType)
	{
		const ZM_BattleMonster& xMon = xCtx.SelfMon();
		if (xMove.m_eType != eType || xMon.m_uCurHP * 3u > xMon.m_auMaxStat[ZM_STAT_HP])
		{
			return uDamage;
		}
		g_EmitAbilityTrigger(xCtx);
		return (uDamage * 3u) / 2u;
	}

	u_int g_VerdantSurgeDamageDealt(const ZM_AbilityContext& xCtx,
		const ZM_MoveData& xMove, u_int uDamage)
	{
		return g_SurgeDamageDealt(xCtx, xMove, uDamage, ZM_TYPE_GRASS);
	}

	u_int g_EmberSurgeDamageDealt(const ZM_AbilityContext& xCtx,
		const ZM_MoveData& xMove, u_int uDamage)
	{
		return g_SurgeDamageDealt(xCtx, xMove, uDamage, ZM_TYPE_FIRE);
	}

	u_int g_TidalSurgeDamageDealt(const ZM_AbilityContext& xCtx,
		const ZM_MoveData& xMove, u_int uDamage)
	{
		return g_SurgeDamageDealt(xCtx, xMove, uDamage, ZM_TYPE_WATER);
	}

	u_int g_HiveSurgeDamageDealt(const ZM_AbilityContext& xCtx,
		const ZM_MoveData& xMove, u_int uDamage)
	{
		return g_SurgeDamageDealt(xCtx, xMove, uDamage, ZM_TYPE_SWARM);
	}

	int g_SkywardGraceTypeInteraction(const ZM_AbilityContext& xCtx,
		ZM_TYPE eMoveType)
	{
		if (eMoveType != ZM_TYPE_EARTH)
		{
			return 0;
		}
		g_EmitAbilityTrigger(xCtx);
		return 1;
	}

	u_int g_BedrockDamageTaken(const ZM_AbilityContext& xCtx,
		const ZM_MoveData&, u_int, u_int uDamage)
	{
		const ZM_BattleMonster& xMon = xCtx.SelfMon();
		const u_int uMaxHP = xMon.m_auMaxStat[ZM_STAT_HP];
		if (xMon.m_uCurHP == 0u || xMon.m_uCurHP != uMaxHP
			|| uDamage < xMon.m_uCurHP)
		{
			return uDamage;
		}
		g_EmitAbilityTrigger(xCtx);
		return xMon.m_uCurHP - 1u;
	}

	u_int g_WeatherSpeed(const ZM_AbilityContext& xCtx, ZM_BATTLE_STAT eStat,
		u_int uValue, ZM_WEATHER eWeather)
	{
		if (eStat != ZM_BATTLE_STAT_SPEED
			|| xCtx.m_pxState->m_xField.m_eWeather != eWeather)
		{
			return uValue;
		}
		g_EmitAbilityTrigger(xCtx);
		return uValue * 2u;
	}

	u_int g_SunchaserModifyStat(const ZM_AbilityContext& xCtx,
		ZM_BATTLE_STAT eStat, u_int uValue)
	{
		return g_WeatherSpeed(xCtx, eStat, uValue, ZM_WEATHER_SUN);
	}

	u_int g_StreamlineModifyStat(const ZM_AbilityContext& xCtx,
		ZM_BATTLE_STAT eStat, u_int uValue)
	{
		return g_WeatherSpeed(xCtx, eStat, uValue, ZM_WEATHER_RAIN);
	}

	u_int g_GritstrideModifyStat(const ZM_AbilityContext& xCtx,
		ZM_BATTLE_STAT eStat, u_int uValue)
	{
		return g_WeatherSpeed(xCtx, eStat, uValue, ZM_WEATHER_SAND);
	}

	u_int g_RimestrideModifyStat(const ZM_AbilityContext& xCtx,
		ZM_BATTLE_STAT eStat, u_int uValue)
	{
		return g_WeatherSpeed(xCtx, eStat, uValue, ZM_WEATHER_SNOW);
	}

	u_int g_FervorModifyStat(const ZM_AbilityContext& xCtx,
		ZM_BATTLE_STAT eStat, u_int uValue)
	{
		if (eStat != ZM_BATTLE_STAT_ATTACK
			|| xCtx.SelfMon().m_eStatus == ZM_MAJOR_STATUS_NONE)
		{
			return uValue;
		}
		g_EmitAbilityTrigger(xCtx);
		return (uValue * 3u) / 2u;
	}

	u_int g_BlubberDamageTaken(const ZM_AbilityContext& xCtx,
		const ZM_MoveData& xMove, u_int, u_int uDamage)
	{
		if (xMove.m_eType != ZM_TYPE_FIRE && xMove.m_eType != ZM_TYPE_ICE)
		{
			return uDamage;
		}
		g_EmitAbilityTrigger(xCtx);
		return uDamage / 2u;
	}

	int g_AbsorbType(const ZM_AbilityContext& xCtx, ZM_TYPE eMoveType,
		ZM_TYPE eAbsorbedType)
	{
		if (eMoveType != eAbsorbedType)
		{
			return 0;
		}
		g_EmitAbilityTrigger(xCtx);
		return 2;
	}

	int g_AquiferTypeInteraction(const ZM_AbilityContext& xCtx,
		ZM_TYPE eMoveType)
	{
		return g_AbsorbType(xCtx, eMoveType, ZM_TYPE_WATER);
	}

	int g_DynamoTypeInteraction(const ZM_AbilityContext& xCtx,
		ZM_TYPE eMoveType)
	{
		return g_AbsorbType(xCtx, eMoveType, ZM_TYPE_ELECTRIC);
	}

	int g_CinderdrinkTypeInteraction(const ZM_AbilityContext& xCtx,
		ZM_TYPE eMoveType)
	{
		return g_AbsorbType(xCtx, eMoveType, ZM_TYPE_FIRE);
	}

	u_int g_CinderdrinkDamageDealt(const ZM_AbilityContext& xCtx,
		const ZM_MoveData& xMove, u_int uDamage)
	{
		if (xMove.m_eType != ZM_TYPE_FIRE)
		{
			return uDamage;
		}
		g_EmitAbilityTrigger(xCtx);
		return (uDamage * 3u) / 2u;
	}

	int g_GrazerTypeInteraction(const ZM_AbilityContext& xCtx,
		ZM_TYPE eMoveType)
	{
		return g_AbsorbType(xCtx, eMoveType, ZM_TYPE_GRASS);
	}

	u_int g_SolidcoreDamageTaken(const ZM_AbilityContext& xCtx,
		const ZM_MoveData&, u_int uEffectivenessPercent, u_int uDamage)
	{
		if (uEffectivenessPercent <= 100u)
		{
			return uDamage;
		}
		g_EmitAbilityTrigger(xCtx);
		return (uDamage * 3u) / 4u;
	}

	u_int g_HeavyplateDamageTaken(const ZM_AbilityContext& xCtx,
		const ZM_MoveData& xMove, u_int, u_int uDamage)
	{
		if (xMove.m_eCategory != ZM_MOVE_CATEGORY_PHYSICAL)
		{
			return uDamage;
		}
		g_EmitAbilityTrigger(xCtx);
		return (uDamage * 2u) / 3u;
	}

	u_int g_GossamerDamageTaken(const ZM_AbilityContext& xCtx,
		const ZM_MoveData& xMove, u_int, u_int uDamage)
	{
		if (xMove.m_eCategory != ZM_MOVE_CATEGORY_SPECIAL)
		{
			return uDamage;
		}
		g_EmitAbilityTrigger(xCtx);
		return (uDamage * 2u) / 3u;
	}

	u_int g_DowndraftDamageTaken(const ZM_AbilityContext& xCtx,
		const ZM_MoveData& xMove, u_int, u_int uDamage)
	{
		if (xMove.m_eType != ZM_TYPE_SKY)
		{
			return uDamage;
		}
		g_EmitAbilityTrigger(xCtx);
		return uDamage / 2u;
	}

	// ---- SC4 CONTACT reactions (rows 7-10) -- pfnOnContact -------------------
	// xCtx.m_eSelf = the CONTACTED defender (ability owner); xCtx.m_eOther = the
	// ATTACKER. bSelfFainted is IGNORED by every SC4 body (it exists for SC5's
	// Lastspite/Aftershock). Dispatched from ZM_MoveExecutor's PHASE E4 seam ONLY
	// for a connecting contact move, so no "did it hit" gate is needed here.

	// Shared 30%-proc status applier (Staticveil/Cinderskin/Barbskin): exactly ONE
	// RandBelow(100) draw, threshold < 30. On success announce FIRST (generic
	// ABILITY_TRIGGER, m_iAmount=0, owner=defender) THEN status the attacker via the
	// state+events ApplyMajor -- which may itself block silently on a type-immune
	// attacker (ABILITY_TRIGGER still fired, STATUS_APPLIED suppressed). On failure:
	// no announce, no status, no further draw.
	void g_ContactStatusProc(ZM_AbilityContext& xCtx, ZM_MAJOR_STATUS eMajor)
	{
		if (xCtx.RNG().RandBelow(100u) >= 30u)
		{
			return;   // proc FAILED: no effect
		}
		g_EmitAbilityTrigger(xCtx);
		ZM_StatusLogic::ApplyMajor(*xCtx.m_pxState, *xCtx.m_pxEvents,
			xCtx.m_eOther, eMajor);
	}

	void g_StaticveilOnContact(ZM_AbilityContext& xCtx, bool)
	{
		g_ContactStatusProc(xCtx, ZM_MAJOR_STATUS_PARALYSIS);
	}

	void g_CinderskinOnContact(ZM_AbilityContext& xCtx, bool)
	{
		g_ContactStatusProc(xCtx, ZM_MAJOR_STATUS_BURN);
	}

	void g_BarbskinOnContact(ZM_AbilityContext& xCtx, bool)
	{
		g_ContactStatusProc(xCtx, ZM_MAJOR_STATUS_POISON);
	}

	// Thornmail: NO draw. Chip the attacker for maxHP/8 (min 1, underflow-clamped),
	// then emit ABILITY_TRIGGER carrying the chip in m_iAmount and the attacker's new
	// HP in m_iAux (owner = defender/self, m_iTag = THORNMAIL), mirroring DAMAGE_DEALT's
	// (amount, remaining-HP) convention (R-C1). A lethal chip appends a FAINT on the
	// attacker.
	void g_ThornmailOnContact(ZM_AbilityContext& xCtx, bool)
	{
		ZM_BattleMonster& xAtk = xCtx.OtherMon();
		u_int uChip = xAtk.m_auMaxStat[ZM_STAT_HP] / 8u;
		if (uChip == 0u) { uChip = 1u; }
		const u_int uNewHP = (uChip >= xAtk.m_uCurHP) ? 0u : (xAtk.m_uCurHP - uChip);
		xAtk.m_uCurHP = uNewHP;
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_ABILITY_TRIGGER, xCtx.m_eSelf,
			xCtx.SelfSide().m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE,
			(int)uChip, (int)uNewHP, (int)xCtx.SelfMon().m_eAbility));
		if (uNewHP == 0u)
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, xCtx.m_eOther,
				xCtx.OtherSide().m_uActiveSlot));
		}
	}

	// ---- SC4 stat-drop veto (rows 25/26/48) -- pfnPreventStatDrop -----------
	// Pure predicates: no RNG, no emit. The ABILITY_TRIGGER emit + "apply nothing"
	// is owned by ZM_StatusLogic::ApplyStatChange's already-live seam (gated on
	// iDelta<0 && bFromFoe). Ironwill/Guardian veto ANY foe drop; Keeneye only ACCURACY.
	bool g_IronwillPreventStatDrop(const ZM_AbilityContext&, ZM_BATTLE_STAT)
	{
		return true;
	}

	bool g_GuardianPreventStatDrop(const ZM_AbilityContext&, ZM_BATTLE_STAT)
	{
		return true;
	}

	bool g_KeeneyePreventStatDrop(const ZM_AbilityContext&, ZM_BATTLE_STAT eStat)
	{
		return eStat == ZM_BATTLE_STAT_ACCURACY;
	}

	// ---- SC4 STATUS_TRY predicates (rows 28-33) -- pfnPreventMajor/Volatile --
	// Pure status->bool predicates (NO context, NO emit). The block + ABILITY_TRIGGER
	// is owned by ApplyMajor/ApplyVolatile.
	bool g_WakefulPreventMajor(ZM_MAJOR_STATUS e)
	{
		return e == ZM_MAJOR_STATUS_SLEEP;
	}

	bool g_PurebloodPreventMajor(ZM_MAJOR_STATUS e)
	{
		return e == ZM_MAJOR_STATUS_POISON || e == ZM_MAJOR_STATUS_TOXIC;
	}

	bool g_ThawheartPreventMajor(ZM_MAJOR_STATUS e)
	{
		return e == ZM_MAJOR_STATUS_FREEZE;
	}

	bool g_LimberlithePreventMajor(ZM_MAJOR_STATUS e)
	{
		return e == ZM_MAJOR_STATUS_PARALYSIS;
	}

	bool g_ColdbloodPreventMajor(ZM_MAJOR_STATUS e)
	{
		return e == ZM_MAJOR_STATUS_BURN;
	}

	bool g_OwnpacePreventVolatile(ZM_VOLATILE e)
	{
		return e == ZM_VOLATILE_CONFUSED;
	}

	// ---- SC4 ACCURACY bypass (rows 27/49) -- pfnBypassAccuracy --------------
	// Pure "auto-hit?" predicates: NO RNG, NO emit. Observability is the move HITTING
	// where the control would miss (the design intentionally omits an event). Deadaim
	// always; Trueshot only while weather != NONE.
	bool g_DeadaimBypassAccuracy(const ZM_AbilityContext&)
	{
		return true;
	}

	bool g_TrueshotBypassAccuracy(const ZM_AbilityContext& xCtx)
	{
		return xCtx.m_pxState->m_xField.m_eWeather != ZM_WEATHER_NONE;
	}

	// ---- SC5 TURN_END self-heals (rows 41-45) -- pfnOnTurnEnd -----------------
	// Shared body: heal SelfMon by maxHP/uDivisor (min 1), capped to missing HP.
	// Event order mirrors the SC3 absorb-heal convention (ABILITY_TRIGGER first,
	// then HEAL with m_iAmount = amount healed, m_iAux = new HP). A full-HP holder
	// no-ops with NO event (proves both "fires" and "does-not-fire"). NO RNG. The
	// EoT dispatch fainted-guard guarantees SelfMon is not fainted here.
	void g_AbilityTurnEndHeal(ZM_AbilityContext& xCtx, u_int uDivisor)
	{
		ZM_BattleMonster& xMon = xCtx.SelfMon();
		const u_int uMaxHP = xMon.m_auMaxStat[ZM_STAT_HP];
		if (xMon.m_uCurHP >= uMaxHP)
		{
			return;   // already full: no heal, no event
		}
		u_int uHeal = uMaxHP / uDivisor;
		if (uHeal == 0u) { uHeal = 1u; }
		const u_int uMissing = uMaxHP - xMon.m_uCurHP;
		if (uHeal > uMissing) { uHeal = uMissing; }
		xMon.m_uCurHP += uHeal;
		g_EmitAbilityTrigger(xCtx);
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_HEAL, xCtx.m_eSelf,
			xCtx.SelfSide().m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE,
			(int)uHeal, (int)xMon.m_uCurHP));
	}

	// Weather read is the engine-side WEATHER-bit realization (no dedicated pfn).
	void g_RainbaskOnTurnEnd(ZM_AbilityContext& xCtx)
	{
		if (xCtx.m_pxState->m_xField.m_eWeather != ZM_WEATHER_RAIN) { return; }
		g_AbilityTurnEndHeal(xCtx, 16u);
	}

	void g_SunbaskOnTurnEnd(ZM_AbilityContext& xCtx)
	{
		if (xCtx.m_pxState->m_xField.m_eWeather != ZM_WEATHER_SUN) { return; }
		g_AbilityTurnEndHeal(xCtx, 16u);
	}

	void g_IceboundOnTurnEnd(ZM_AbilityContext& xCtx)
	{
		if (xCtx.m_pxState->m_xField.m_eWeather != ZM_WEATHER_SNOW) { return; }
		g_AbilityTurnEndHeal(xCtx, 16u);
	}

	// Toxicthrive: heal maxHP/8 while POISON/TOXIC. The poison chip for this holder
	// is SKIPPED in ZM_StatusLogic::EndOfTurn, so the net effect is a heal, not chip.
	void g_ToxicthriveOnTurnEnd(ZM_AbilityContext& xCtx)
	{
		const ZM_MAJOR_STATUS eStatus = xCtx.SelfMon().m_eStatus;
		if (eStatus != ZM_MAJOR_STATUS_POISON && eStatus != ZM_MAJOR_STATUS_TOXIC) { return; }
		g_AbilityTurnEndHeal(xCtx, 8u);
	}

	// Rootfeed: unconditional EoT heal maxHP/16.
	void g_RootfeedOnTurnEnd(ZM_AbilityContext& xCtx)
	{
		g_AbilityTurnEndHeal(xCtx, 16u);
	}

	// ---- SC5 FAINT branch ----------------------------------------------------
	// Bloodrush (row 34) -- pfnOnDealtFaint: on downing a foe with its move, raise
	// own ATTACK +1. Mirrors DauntingRoar's order: ApplyStatChange first (emits
	// STAT_STAGE_CHANGED on success), then ABILITY_TRIGGER. bFromFoe = false (a
	// self-raise is never vetoed). At the +6 cap ApplyStatChange emits nothing but
	// ABILITY_TRIGGER still fires. The MODIFY_STAT bit is realized by this call
	// (engine-side), so pfnModifyStat stays null on the row.
	void g_BloodrushOnDealtFaint(ZM_AbilityContext& xCtx)
	{
		ZM_StatusLogic::ApplyStatChange(*xCtx.m_pxState, *xCtx.m_pxEvents,
			xCtx.m_eSelf, ZM_BATTLE_STAT_ATTACK, +1, /*bPrimary*/ false, /*bFromFoe*/ false);
		g_EmitAbilityTrigger(xCtx);
	}

	// Lastspite (row 35) -- pfnOnContact bSelfFainted branch: when KO'd by a contact
	// hit, set the attacker's used-move PP to 0. m_eSelf = the contacted holder (KO'd);
	// m_eOther = the attacker; m_uOtherMoveSlot = the attacker's used-move slot (set at
	// the E4 contact seam). A non-lethal contact (!bSelfFainted) is a clean no-op. The
	// PP change is observable via the attacker's m_axMoves[slot].m_uCurPP == 0; no
	// dedicated PP event exists, so ABILITY_TRIGGER is the only emit.
	void g_LastspiteOnContact(ZM_AbilityContext& xCtx, bool bSelfFainted)
	{
		if (!bSelfFainted) { return; }
		Zenith_Assert(xCtx.m_uOtherMoveSlot < uZM_MAX_MOVES,
			"Lastspite: attacker used-move slot not populated at E4 dispatch");
		xCtx.OtherMon().m_axMoves[xCtx.m_uOtherMoveSlot].m_uCurPP = 0u;
		g_EmitAbilityTrigger(xCtx);
	}

	// Aftershock (row 36) -- pfnOnContact bSelfFainted branch: when KO'd by a contact
	// hit, chip the attacker maxHP/4 (min 1, underflow-clamped). Structurally identical
	// to Thornmail with /4 and the bSelfFainted guard: ABILITY_TRIGGER carries the chip
	// in m_iAmount and the attacker's new HP in m_iAux (DAMAGE_DEALT-style convention);
	// a lethal chip appends FAINT on the attacker.
	void g_AftershockOnContact(ZM_AbilityContext& xCtx, bool bSelfFainted)
	{
		if (!bSelfFainted) { return; }
		ZM_BattleMonster& xAtk = xCtx.OtherMon();
		u_int uChip = xAtk.m_auMaxStat[ZM_STAT_HP] / 4u;
		if (uChip == 0u) { uChip = 1u; }
		const u_int uNewHP = (uChip >= xAtk.m_uCurHP) ? 0u : (xAtk.m_uCurHP - uChip);
		xAtk.m_uCurHP = uNewHP;
		xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_ABILITY_TRIGGER, xCtx.m_eSelf,
			xCtx.SelfSide().m_uActiveSlot, ZM_MOVE_NONE, ZM_SPECIES_NONE,
			(int)uChip, (int)uNewHP, (int)xCtx.SelfMon().m_eAbility));
		if (uNewHP == 0u)
		{
			xCtx.Emit(ZM_MakeEvent(ZM_BATTLE_EVENT_FAINT, xCtx.m_eOther,
				xCtx.OtherSide().m_uActiveSlot));
		}
	}

	ZM_AbilityHooks g_MakeSwitchInHooks(void (*pfnOnSwitchIn)(ZM_AbilityContext&))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnOnSwitchIn = pfnOnSwitchIn;
		return xHooks;
	}

	ZM_AbilityHooks g_MakeModifyStatHooks(u_int (*pfnModifyStat)(
		const ZM_AbilityContext&, ZM_BATTLE_STAT, u_int))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnModifyStat = pfnModifyStat;
		return xHooks;
	}

	ZM_AbilityHooks g_MakeDamageDealtHooks(u_int (*pfnModifyDamageDealt)(
		const ZM_AbilityContext&, const ZM_MoveData&, u_int))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnModifyDamageDealt = pfnModifyDamageDealt;
		return xHooks;
	}

	ZM_AbilityHooks g_MakeDamageTakenHooks(u_int (*pfnModifyDamageTaken)(
		const ZM_AbilityContext&, const ZM_MoveData&, u_int, u_int))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnModifyDamageTaken = pfnModifyDamageTaken;
		return xHooks;
	}

	ZM_AbilityHooks g_MakeTypeInteractionHooks(int (*pfnTypeInteraction)(
		const ZM_AbilityContext&, ZM_TYPE))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnTypeInteraction = pfnTypeInteraction;
		return xHooks;
	}

	ZM_AbilityHooks g_MakeCinderdrinkHooks()
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnModifyDamageDealt = &g_CinderdrinkDamageDealt;
		xHooks.pfnTypeInteraction = &g_CinderdrinkTypeInteraction;
		return xHooks;
	}

	ZM_AbilityHooks g_MakeContactHooks(void (*pfnOnContact)(ZM_AbilityContext&, bool))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnOnContact = pfnOnContact;
		return xHooks;
	}

	ZM_AbilityHooks g_MakePreventStatDropHooks(bool (*pfnPreventStatDrop)(
		const ZM_AbilityContext&, ZM_BATTLE_STAT))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnPreventStatDrop = pfnPreventStatDrop;
		return xHooks;
	}

	ZM_AbilityHooks g_MakePreventMajorHooks(bool (*pfnPreventMajor)(ZM_MAJOR_STATUS))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnPreventMajor = pfnPreventMajor;
		return xHooks;
	}

	ZM_AbilityHooks g_MakePreventVolatileHooks(bool (*pfnPreventVolatile)(ZM_VOLATILE))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnPreventVolatile = pfnPreventVolatile;
		return xHooks;
	}

	ZM_AbilityHooks g_MakeBypassAccuracyHooks(bool (*pfnBypassAccuracy)(
		const ZM_AbilityContext&))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnBypassAccuracy = pfnBypassAccuracy;
		return xHooks;
	}

	ZM_AbilityHooks g_MakeTurnEndHooks(void (*pfnOnTurnEnd)(ZM_AbilityContext&))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnOnTurnEnd = pfnOnTurnEnd;
		return xHooks;
	}

	ZM_AbilityHooks g_MakeDealtFaintHooks(void (*pfnOnDealtFaint)(ZM_AbilityContext&))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnOnDealtFaint = pfnOnDealtFaint;
		return xHooks;
	}

	// One row per stable ZM_ABILITY_ID. SC2/SC3 install only their implemented
	// slots; SC4-SC5 retain their planned null slots until their ordered slices.
	const ZM_AbilityHooks s_axAbilityHooks[ZM_ABILITY_COUNT] =
	{
		g_MakeDamageDealtHooks(&g_VerdantSurgeDamageDealt),     //  0 VERDANTSURGE
		g_MakeDamageDealtHooks(&g_EmberSurgeDamageDealt),       //  1 EMBERSURGE
		g_MakeDamageDealtHooks(&g_TidalSurgeDamageDealt),       //  2 TIDALSURGE
		g_MakeDamageDealtHooks(&g_HiveSurgeDamageDealt),        //  3 HIVESURGE
		g_MakeSwitchInHooks(&g_DauntingRoarOnSwitchIn),         //  4 DAUNTINGROAR
		g_MakeTypeInteractionHooks(&g_SkywardGraceTypeInteraction), //  5 SKYWARDGRACE
		g_MakeDamageTakenHooks(&g_BedrockDamageTaken),          //  6 BEDROCK
		g_MakeContactHooks(&g_StaticveilOnContact),             //  7 STATICVEIL
		g_MakeContactHooks(&g_CinderskinOnContact),             //  8 CINDERSKIN
		g_MakeContactHooks(&g_BarbskinOnContact),               //  9 BARBSKIN
		g_MakeContactHooks(&g_ThornmailOnContact),              // 10 THORNMAIL
		g_MakeModifyStatHooks(&g_SunchaserModifyStat),          // 11 SUNCHASER
		g_MakeModifyStatHooks(&g_StreamlineModifyStat),         // 12 STREAMLINE
		g_MakeModifyStatHooks(&g_GritstrideModifyStat),         // 13 GRITSTRIDE
		g_MakeModifyStatHooks(&g_RimestrideModifyStat),         // 14 RIMESTRIDE
		g_MakeSwitchInHooks(&g_RaincallerOnSwitchIn),           // 15 RAINCALLER
		g_MakeSwitchInHooks(&g_SuncallerOnSwitchIn),            // 16 SUNCALLER
		g_MakeSwitchInHooks(&g_SandcallerOnSwitchIn),           // 17 SANDCALLER
		g_MakeSwitchInHooks(&g_SnowcallerOnSwitchIn),           // 18 SNOWCALLER
		g_MakeModifyStatHooks(&g_FervorModifyStat),             // 19 FERVOR
		g_MakeDamageTakenHooks(&g_BlubberDamageTaken),          // 20 BLUBBER
		g_MakeTypeInteractionHooks(&g_AquiferTypeInteraction),  // 21 AQUIFER
		g_MakeTypeInteractionHooks(&g_DynamoTypeInteraction),   // 22 DYNAMO
		g_MakeCinderdrinkHooks(),                               // 23 CINDERDRINK
		g_MakeTypeInteractionHooks(&g_GrazerTypeInteraction),   // 24 GRAZER
		g_MakePreventStatDropHooks(&g_IronwillPreventStatDrop), // 25 IRONWILL
		g_MakePreventStatDropHooks(&g_KeeneyePreventStatDrop),  // 26 KEENEYE
		g_MakeBypassAccuracyHooks(&g_DeadaimBypassAccuracy),    // 27 DEADAIM
		g_MakePreventMajorHooks(&g_WakefulPreventMajor),        // 28 WAKEFUL
		g_MakePreventMajorHooks(&g_PurebloodPreventMajor),      // 29 PUREBLOOD
		g_MakePreventMajorHooks(&g_ThawheartPreventMajor),      // 30 THAWHEART
		g_MakePreventMajorHooks(&g_LimberlithePreventMajor),    // 31 LIMBERLITHE
		g_MakePreventVolatileHooks(&g_OwnpacePreventVolatile),  // 32 OWNPACE
		g_MakePreventMajorHooks(&g_ColdbloodPreventMajor),      // 33 COLDBLOOD
		g_MakeDealtFaintHooks(&g_BloodrushOnDealtFaint),        // 34 BLOODRUSH
		g_MakeContactHooks(&g_LastspiteOnContact),              // 35 LASTSPITE
		g_MakeContactHooks(&g_AftershockOnContact),             // 36 AFTERSHOCK
		g_MakeDamageTakenHooks(&g_SolidcoreDamageTaken),        // 37 SOLIDCORE
		g_MakeDamageTakenHooks(&g_HeavyplateDamageTaken),       // 38 HEAVYPLATE
		g_MakeDamageTakenHooks(&g_GossamerDamageTaken),         // 39 GOSSAMER
		g_MakeDamageTakenHooks(&g_DowndraftDamageTaken),        // 40 DOWNDRAFT
		g_MakeTurnEndHooks(&g_RainbaskOnTurnEnd),               // 41 RAINBASK
		g_MakeTurnEndHooks(&g_SunbaskOnTurnEnd),                // 42 SUNBASK
		g_MakeTurnEndHooks(&g_IceboundOnTurnEnd),               // 43 ICEBOUND
		g_MakeTurnEndHooks(&g_ToxicthriveOnTurnEnd),            // 44 TOXICTHRIVE
		g_MakeTurnEndHooks(&g_RootfeedOnTurnEnd),               // 45 ROOTFEED
		{},                                                     // 46 QUICKDRAW (engine-side realization)
		g_MakeSwitchInHooks(&g_PressureAuraOnSwitchIn),         // 47 PRESSUREAURA
		g_MakePreventStatDropHooks(&g_GuardianPreventStatDrop), // 48 GUARDIAN
		g_MakeBypassAccuracyHooks(&g_TrueshotBypassAccuracy),   // 49 TRUESHOT
	};
}

const ZM_AbilityHooks* ZM_GetAbilityHooks(ZM_ABILITY_ID eId)
{
	if (eId >= ZM_ABILITY_COUNT)
	{
		return nullptr;
	}
	return &s_axAbilityHooks[eId];
}
