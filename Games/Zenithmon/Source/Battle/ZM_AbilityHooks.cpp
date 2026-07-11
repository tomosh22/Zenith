#include "Zenith.h"

#include "Zenithmon/Source/Battle/ZM_AbilityHooks.h"
#include "Zenithmon/Source/Battle/ZM_StatusLogic.h"

// ============================================================================
// SC2 lights SWITCH_IN only. Later ordered box-3 sub-commits fill the remaining
// slots in this same const table. All six SC2 bodies are deterministic: no RNG
// draw is added, and a NONE ability resolves to no dispatch and no event.
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

	ZM_AbilityHooks g_MakeSwitchInHooks(void (*pfnOnSwitchIn)(ZM_AbilityContext&))
	{
		ZM_AbilityHooks xHooks;
		xHooks.pfnOnSwitchIn = pfnOnSwitchIn;
		return xHooks;
	}

	// One row per stable ZM_ABILITY_ID. SC2 installs only the six implemented
	// SWITCH_IN bodies; SC3-SC5 replace their planned null slots in order.
	const ZM_AbilityHooks s_axAbilityHooks[ZM_ABILITY_COUNT] =
	{
		{},                                                     //  0 VERDANTSURGE
		{},                                                     //  1 EMBERSURGE
		{},                                                     //  2 TIDALSURGE
		{},                                                     //  3 HIVESURGE
		g_MakeSwitchInHooks(&g_DauntingRoarOnSwitchIn),         //  4 DAUNTINGROAR
		{},                                                     //  5 SKYWARDGRACE
		{},                                                     //  6 BEDROCK
		{},                                                     //  7 STATICVEIL
		{},                                                     //  8 CINDERSKIN
		{},                                                     //  9 BARBSKIN
		{},                                                     // 10 THORNMAIL
		{},                                                     // 11 SUNCHASER
		{},                                                     // 12 STREAMLINE
		{},                                                     // 13 GRITSTRIDE
		{},                                                     // 14 RIMESTRIDE
		g_MakeSwitchInHooks(&g_RaincallerOnSwitchIn),           // 15 RAINCALLER
		g_MakeSwitchInHooks(&g_SuncallerOnSwitchIn),            // 16 SUNCALLER
		g_MakeSwitchInHooks(&g_SandcallerOnSwitchIn),           // 17 SANDCALLER
		g_MakeSwitchInHooks(&g_SnowcallerOnSwitchIn),           // 18 SNOWCALLER
		{},                                                     // 19 FERVOR
		{},                                                     // 20 BLUBBER
		{},                                                     // 21 AQUIFER
		{},                                                     // 22 DYNAMO
		{},                                                     // 23 CINDERDRINK
		{},                                                     // 24 GRAZER
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
		{},                                                     // 37 SOLIDCORE
		{},                                                     // 38 HEAVYPLATE
		{},                                                     // 39 GOSSAMER
		{},                                                     // 40 DOWNDRAFT
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
