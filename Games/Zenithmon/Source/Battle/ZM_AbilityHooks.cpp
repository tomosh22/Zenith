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
		{},                                                     //  7 STATICVEIL
		{},                                                     //  8 CINDERSKIN
		{},                                                     //  9 BARBSKIN
		{},                                                     // 10 THORNMAIL
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
		{},                                                     // 25 IRONWILL
		{},                                                     // 26 KEENEYE
		{},                                                     // 27 DEADAIM
		{},                                                     // 28 WAKEFUL
		{},                                                     // 29 PUREBLOOD
		{},                                                     // 30 THAWHEART
		{},                                                     // 31 LIMBERLITHE
		{},                                                     // 32 OWNPACE
		{},                                                     // 33 COLDBLOOD
		{},                                                     // 34 BLOODRUSH
		{},                                                     // 35 LASTSPITE
		{},                                                     // 36 AFTERSHOCK
		g_MakeDamageTakenHooks(&g_SolidcoreDamageTaken),        // 37 SOLIDCORE
		g_MakeDamageTakenHooks(&g_HeavyplateDamageTaken),       // 38 HEAVYPLATE
		g_MakeDamageTakenHooks(&g_GossamerDamageTaken),         // 39 GOSSAMER
		g_MakeDamageTakenHooks(&g_DowndraftDamageTaken),        // 40 DOWNDRAFT
		{},                                                     // 41 RAINBASK
		{},                                                     // 42 SUNBASK
		{},                                                     // 43 ICEBOUND
		{},                                                     // 44 TOXICTHRIVE
		{},                                                     // 45 ROOTFEED
		{},                                                     // 46 QUICKDRAW (engine-side realization)
		g_MakeSwitchInHooks(&g_PressureAuraOnSwitchIn),         // 47 PRESSUREAURA
		{},                                                     // 48 GUARDIAN
		{},                                                     // 49 TRUESHOT
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
