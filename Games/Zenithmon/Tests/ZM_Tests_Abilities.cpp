#include "Zenith.h"

// ============================================================================
// ZM_Tests_Abilities -- integrity of the ability roster (category ZM_Data). S1
// ships roster + metadata + each ability's declared HOOK SURFACE bitmask; the
// fn-pointer hook bodies are S2. This suite locks the roster and the mask
// contract: valid non-zero masks with no stray bits, every hook bit covered by
// at least one ability, and the hook-query accessor. See DecisionLog ZM-D-026.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Battle/ZM_AbilityHooks.h"
#include "Zenithmon/Source/Data/ZM_AbilityData.h"

#include <cstring>

namespace
{
	constexpr u_int uEXPECTED_ABILITIES = 50;
	constexpr u_int uEXPECTED_HOOK_SLOTS = 12;

	const ZM_AbilityData& Ab(u_int i) { return ZM_GetAbilityData((ZM_ABILITY_ID)i); }

	bool ZM_IsSC2SwitchInAbility(ZM_ABILITY_ID eId)
	{
		switch (eId)
		{
		case ZM_ABILITY_DAUNTINGROAR:
		case ZM_ABILITY_RAINCALLER:
		case ZM_ABILITY_SUNCALLER:
		case ZM_ABILITY_SANDCALLER:
		case ZM_ABILITY_SNOWCALLER:
		case ZM_ABILITY_PRESSUREAURA:
			return true;
		default:
			return false;
		}
	}

	u_int ZM_LiveHookRealizationMask(const ZM_AbilityHooks& xHooks, u_int uDeclaredMask)
	{
		u_int uMask = 0u;
		if (xHooks.pfnOnSwitchIn != nullptr)        { uMask |= ZM_ABILITY_HOOK_SWITCH_IN; }
		if (xHooks.pfnModifyStat != nullptr)        { uMask |= ZM_ABILITY_HOOK_MODIFY_STAT; }
		if (xHooks.pfnPreventStatDrop != nullptr)   { uMask |= uDeclaredMask & (ZM_ABILITY_HOOK_MODIFY_STAT | ZM_ABILITY_HOOK_ACCURACY); }
		if (xHooks.pfnModifyDamageDealt != nullptr) { uMask |= ZM_ABILITY_HOOK_MODIFY_DAMAGE_DEALT; }
		if (xHooks.pfnModifyDamageTaken != nullptr) { uMask |= ZM_ABILITY_HOOK_MODIFY_DAMAGE_TAKEN; }
		if (xHooks.pfnPreventMajor != nullptr)      { uMask |= ZM_ABILITY_HOOK_STATUS_TRY; }
		if (xHooks.pfnPreventVolatile != nullptr)   { uMask |= ZM_ABILITY_HOOK_STATUS_TRY; }
		if (xHooks.pfnOnContact != nullptr)         { uMask |= uDeclaredMask & (ZM_ABILITY_HOOK_CONTACT | ZM_ABILITY_HOOK_FAINT); }
		if (xHooks.pfnOnTurnEnd != nullptr)         { uMask |= ZM_ABILITY_HOOK_TURN_END; }
		if (xHooks.pfnOnDealtFaint != nullptr)      { uMask |= ZM_ABILITY_HOOK_FAINT; }
		if (xHooks.pfnBypassAccuracy != nullptr)    { uMask |= ZM_ABILITY_HOOK_ACCURACY; }
		if (xHooks.pfnTypeInteraction != nullptr)   { uMask |= ZM_ABILITY_HOOK_TYPE_IMMUNITY; }
		return uMask;
	}

	u_int ZM_EngineSideRealizationMask(ZM_ABILITY_ID eId, u_int uDeclaredMask)
	{
		u_int uMask = 0u;
		switch (eId)
		{
		case ZM_ABILITY_SUNCHASER:
		case ZM_ABILITY_STREAMLINE:
		case ZM_ABILITY_GRITSTRIDE:
		case ZM_ABILITY_RIMESTRIDE:
		case ZM_ABILITY_RAINCALLER:
		case ZM_ABILITY_SUNCALLER:
		case ZM_ABILITY_SANDCALLER:
		case ZM_ABILITY_SNOWCALLER:
		case ZM_ABILITY_RAINBASK:
		case ZM_ABILITY_SUNBASK:
		case ZM_ABILITY_ICEBOUND:
		case ZM_ABILITY_TRUESHOT:
			uMask |= uDeclaredMask & ZM_ABILITY_HOOK_WEATHER;
			break;
		default:
			break;
		}
		if (eId == ZM_ABILITY_QUICKDRAW || eId == ZM_ABILITY_DAUNTINGROAR || eId == ZM_ABILITY_BLOODRUSH)
		{
			uMask |= ZM_ABILITY_HOOK_MODIFY_STAT;
		}
		return uMask;
	}
}

ZENITH_TEST(ZM_Data, Abilities_CountAndSelfConsistent)
{
	ZENITH_ASSERT_EQ(ZM_GetAbilityCount(), uEXPECTED_ABILITIES);
	ZENITH_ASSERT_EQ((u_int)ZM_ABILITY_COUNT, uEXPECTED_ABILITIES);
	ZENITH_ASSERT_EQ((u_int)ZM_ABILITY_NONE, (u_int)ZM_ABILITY_COUNT);
	for (u_int i = 0; i < ZM_ABILITY_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)Ab(i).m_eId, i, "ability row %u has mismatched m_eId", i);
	}
}

ZENITH_TEST(ZM_Data, Abilities_NamesUniqueNonEmpty)
{
	for (u_int i = 0; i < ZM_ABILITY_COUNT; ++i)
	{
		const char* szA = Ab(i).m_szName;
		ZENITH_ASSERT_NOT_NULL(szA);
		ZENITH_ASSERT_TRUE(szA[0] != '\0', "ability %u has an empty name", i);
		for (u_int j = i + 1; j < ZM_ABILITY_COUNT; ++j)
		{
			ZENITH_ASSERT_FALSE(strcmp(szA, Ab(j).m_szName) == 0,
				"duplicate ability name '%s' at %u and %u", szA, i, j);
		}
	}
}

ZENITH_TEST(ZM_Data, Abilities_DescriptionsNonEmpty)
{
	for (u_int i = 0; i < ZM_ABILITY_COUNT; ++i)
	{
		ZENITH_ASSERT_NOT_NULL(Ab(i).m_szDescription);
		ZENITH_ASSERT_TRUE(Ab(i).m_szDescription[0] != '\0', "%s has an empty description", Ab(i).m_szName);
	}
}

// Every ability declares at least one hook and no bits outside the defined set.
ZENITH_TEST(ZM_Data, Abilities_HookMasksValid)
{
	for (u_int i = 0; i < ZM_ABILITY_COUNT; ++i)
	{
		const ZM_AbilityData& x = Ab(i);
		ZENITH_ASSERT_NE(x.m_uHookMask, 0u, "%s implements no hooks", x.m_szName);
		ZENITH_ASSERT_EQ(x.m_uHookMask & ~uZM_ABILITY_HOOK_ALL, 0u, "%s has stray hook bits", x.m_szName);
	}
}

// Every hook bit is exercised by at least one ability (so each S2 hook has a
// data subject to wire).
ZENITH_TEST(ZM_Data, Abilities_EveryHookUsed)
{
	for (u_int b = 0; b < uZM_ABILITY_HOOK_COUNT; ++b)
	{
		const u_int uBit = (1u << b);
		bool bFound = false;
		for (u_int i = 0; i < ZM_ABILITY_COUNT && !bFound; ++i)
		{
			bFound = (Ab(i).m_uHookMask & uBit) != 0u;
		}
		ZENITH_ASSERT_TRUE(bFound, "no ability uses hook bit %u", b);
	}
}

// ZM_AbilityHasHook agrees with the raw mask; name accessor honours its contract.
ZENITH_TEST(ZM_Data, Abilities_HookQueryAndAccessor)
{
	const ZM_ABILITY_HOOK aeHooks[] = {
		ZM_ABILITY_HOOK_SWITCH_IN, ZM_ABILITY_HOOK_MODIFY_STAT,
		ZM_ABILITY_HOOK_MODIFY_DAMAGE_DEALT, ZM_ABILITY_HOOK_MODIFY_DAMAGE_TAKEN,
		ZM_ABILITY_HOOK_STATUS_TRY, ZM_ABILITY_HOOK_CONTACT, ZM_ABILITY_HOOK_TURN_END,
		ZM_ABILITY_HOOK_FAINT, ZM_ABILITY_HOOK_ACCURACY, ZM_ABILITY_HOOK_WEATHER,
		ZM_ABILITY_HOOK_TYPE_IMMUNITY,
	};
	for (u_int i = 0; i < ZM_ABILITY_COUNT; ++i)
	{
		const ZM_ABILITY_ID eId = (ZM_ABILITY_ID)i;
		for (u_int h = 0; h < sizeof(aeHooks) / sizeof(aeHooks[0]); ++h)
		{
			const bool bExpected = (Ab(i).m_uHookMask & (u_int)aeHooks[h]) != 0u;
			ZENITH_ASSERT_EQ(ZM_AbilityHasHook(eId, aeHooks[h]), bExpected,
				"%s hook-query disagrees with mask", Ab(i).m_szName);
		}
		ZENITH_ASSERT_STREQ(ZM_GetAbilityName(eId), Ab(i).m_szName);
	}
	ZENITH_ASSERT_STREQ(ZM_GetAbilityName(ZM_ABILITY_NONE), "NONE");
}

// S2 box-3 SC2 grows a parallel 12-function-pointer table. The sentinel and
// any out-of-range id are deliberately null so dispatch can query NONE without
// tripping ZM_GetAbilityData's in-range assertion.
ZENITH_TEST(ZM_Data, Abilities_HookTableTwelveSlotsAndSentinelContract)
{
	using ZM_TestFunctionPointer = void (*)();
	ZENITH_ASSERT_EQ(sizeof(ZM_AbilityHooks), sizeof(ZM_TestFunctionPointer) * uEXPECTED_HOOK_SLOTS,
		"ZM_AbilityHooks must remain the documented 12-pfn aggregate");
	for (u_int i = 0u; i < ZM_ABILITY_COUNT; ++i)
	{
		ZENITH_ASSERT_NOT_NULL(ZM_GetAbilityHooks((ZM_ABILITY_ID)i));
	}
	ZENITH_ASSERT_TRUE(ZM_GetAbilityHooks(ZM_ABILITY_NONE) == nullptr);
	ZENITH_ASSERT_TRUE(ZM_GetAbilityHooks((ZM_ABILITY_ID)(ZM_ABILITY_COUNT + 1u)) == nullptr);
}

// Incremental SC2 invariant: the six abilities installed in this sub-commit
// completely realize their declared masks through a live pfn and/or one of the
// explicitly documented engine-side mechanisms. Later-SC pfn rows may still be
// null; the all-50 complete-realization gate belongs to SC5.
ZENITH_TEST(ZM_Data, Abilities_HookTableSC2InstalledRowsRealizeDeclaredMasks)
{
	for (u_int i = 0u; i < ZM_ABILITY_COUNT; ++i)
	{
		const ZM_ABILITY_ID eId = (ZM_ABILITY_ID)i;
		if (!ZM_IsSC2SwitchInAbility(eId))
		{
			continue;
		}
		const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(eId);
		ZENITH_ASSERT_NOT_NULL(pxHooks);
		if (pxHooks == nullptr)
		{
			continue;
		}
		const u_int uDeclared = Ab(i).m_uHookMask;
		const u_int uRealized = ZM_LiveHookRealizationMask(*pxHooks, uDeclared)
			| ZM_EngineSideRealizationMask(eId, uDeclared);
		ZENITH_ASSERT_EQ(uDeclared & ~uRealized, 0u,
			"SC2 ability %s has an unrealized declared bit", Ab(i).m_szName);
	}
}

// Converse invariant applies from SC2 onward: a live slot is never installed
// ahead of its declaration. Multi-realization bits accept either of their
// documented slots (MODIFY_STAT/ACCURACY and CONTACT/FAINT).
ZENITH_TEST(ZM_Data, Abilities_HookTableEveryLiveSlotIsDeclared)
{
	for (u_int i = 0u; i < ZM_ABILITY_COUNT; ++i)
	{
		const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks((ZM_ABILITY_ID)i);
		ZENITH_ASSERT_NOT_NULL(pxHooks);
		if (pxHooks == nullptr)
		{
			continue;
		}
		const u_int uMask = Ab(i).m_uHookMask;
		if (pxHooks->pfnOnSwitchIn != nullptr)        { ZENITH_ASSERT_NE(uMask & ZM_ABILITY_HOOK_SWITCH_IN, 0u); }
		if (pxHooks->pfnModifyStat != nullptr)        { ZENITH_ASSERT_NE(uMask & ZM_ABILITY_HOOK_MODIFY_STAT, 0u); }
		if (pxHooks->pfnPreventStatDrop != nullptr)   { ZENITH_ASSERT_NE(uMask & (ZM_ABILITY_HOOK_MODIFY_STAT | ZM_ABILITY_HOOK_ACCURACY), 0u); }
		if (pxHooks->pfnModifyDamageDealt != nullptr) { ZENITH_ASSERT_NE(uMask & ZM_ABILITY_HOOK_MODIFY_DAMAGE_DEALT, 0u); }
		if (pxHooks->pfnModifyDamageTaken != nullptr) { ZENITH_ASSERT_NE(uMask & ZM_ABILITY_HOOK_MODIFY_DAMAGE_TAKEN, 0u); }
		if (pxHooks->pfnPreventMajor != nullptr)      { ZENITH_ASSERT_NE(uMask & ZM_ABILITY_HOOK_STATUS_TRY, 0u); }
		if (pxHooks->pfnPreventVolatile != nullptr)   { ZENITH_ASSERT_NE(uMask & ZM_ABILITY_HOOK_STATUS_TRY, 0u); }
		if (pxHooks->pfnOnContact != nullptr)         { ZENITH_ASSERT_NE(uMask & (ZM_ABILITY_HOOK_CONTACT | ZM_ABILITY_HOOK_FAINT), 0u); }
		if (pxHooks->pfnOnTurnEnd != nullptr)         { ZENITH_ASSERT_NE(uMask & ZM_ABILITY_HOOK_TURN_END, 0u); }
		if (pxHooks->pfnOnDealtFaint != nullptr)      { ZENITH_ASSERT_NE(uMask & ZM_ABILITY_HOOK_FAINT, 0u); }
		if (pxHooks->pfnBypassAccuracy != nullptr)    { ZENITH_ASSERT_NE(uMask & ZM_ABILITY_HOOK_ACCURACY, 0u); }
		if (pxHooks->pfnTypeInteraction != nullptr)   { ZENITH_ASSERT_NE(uMask & ZM_ABILITY_HOOK_TYPE_IMMUNITY, 0u); }
	}
}

ZENITH_TEST(ZM_Data, Abilities_HookTableEngineSideRealizationsAreExplicit)
{
	const ZM_ABILITY_ID aeModifyStatEngineSide[] = {
		ZM_ABILITY_QUICKDRAW, ZM_ABILITY_DAUNTINGROAR, ZM_ABILITY_BLOODRUSH,
	};
	for (u_int i = 0u; i < sizeof(aeModifyStatEngineSide) / sizeof(aeModifyStatEngineSide[0]); ++i)
	{
		const ZM_ABILITY_ID eId = aeModifyStatEngineSide[i];
		const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(eId);
		ZENITH_ASSERT_NOT_NULL(pxHooks);
		ZENITH_ASSERT_NE(ZM_GetAbilityData(eId).m_uHookMask & ZM_ABILITY_HOOK_MODIFY_STAT, 0u);
		if (pxHooks != nullptr)
		{
			ZENITH_ASSERT_TRUE(pxHooks->pfnModifyStat == nullptr,
				"%s MODIFY_STAT is documented engine-side, not pfnModifyStat", ZM_GetAbilityName(eId));
		}
	}

	const ZM_ABILITY_ID aeWeatherEngineSide[] = {
		ZM_ABILITY_SUNCHASER, ZM_ABILITY_STREAMLINE, ZM_ABILITY_GRITSTRIDE,
		ZM_ABILITY_RIMESTRIDE, ZM_ABILITY_RAINCALLER, ZM_ABILITY_SUNCALLER,
		ZM_ABILITY_SANDCALLER, ZM_ABILITY_SNOWCALLER, ZM_ABILITY_RAINBASK,
		ZM_ABILITY_SUNBASK, ZM_ABILITY_ICEBOUND, ZM_ABILITY_TRUESHOT,
	};
	for (u_int i = 0u; i < ZM_ABILITY_COUNT; ++i)
	{
		bool bExpectedWeather = false;
		for (u_int j = 0u; j < sizeof(aeWeatherEngineSide) / sizeof(aeWeatherEngineSide[0]); ++j)
		{
			if ((ZM_ABILITY_ID)i == aeWeatherEngineSide[j])
			{
				bExpectedWeather = true;
				break;
			}
		}
		const u_int uMask = Ab(i).m_uHookMask;
		ZENITH_ASSERT_EQ((uMask & ZM_ABILITY_HOOK_WEATHER) != 0u, bExpectedWeather,
			"%s WEATHER declaration must match the explicit engine-side roster", Ab(i).m_szName);
		if (bExpectedWeather)
		{
			ZENITH_ASSERT_NE(ZM_EngineSideRealizationMask((ZM_ABILITY_ID)i, uMask)
				& ZM_ABILITY_HOOK_WEATHER, 0u, "%s WEATHER bit must be engine-realized", Ab(i).m_szName);
		}
	}
}

ZENITH_TEST(ZM_Data, Abilities_HookTableSC2SwitchInRowsMatchRosterExactly)
{
	for (u_int i = 0u; i < ZM_ABILITY_COUNT; ++i)
	{
		const ZM_ABILITY_ID eId = (ZM_ABILITY_ID)i;
		const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(eId);
		ZENITH_ASSERT_NOT_NULL(pxHooks);
		if (pxHooks != nullptr)
		{
			ZENITH_ASSERT_EQ(pxHooks->pfnOnSwitchIn != nullptr, ZM_IsSC2SwitchInAbility(eId),
				"%s has the wrong SC2 SWITCH_IN installation state", Ab(i).m_szName);
		}
	}
}
