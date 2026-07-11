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

	bool ZM_IsSC3Ability(ZM_ABILITY_ID eId)
	{
		switch (eId)
		{
		case ZM_ABILITY_VERDANTSURGE:
		case ZM_ABILITY_EMBERSURGE:
		case ZM_ABILITY_TIDALSURGE:
		case ZM_ABILITY_HIVESURGE:
		case ZM_ABILITY_SKYWARDGRACE:
		case ZM_ABILITY_BEDROCK:
		case ZM_ABILITY_SUNCHASER:
		case ZM_ABILITY_STREAMLINE:
		case ZM_ABILITY_GRITSTRIDE:
		case ZM_ABILITY_RIMESTRIDE:
		case ZM_ABILITY_FERVOR:
		case ZM_ABILITY_BLUBBER:
		case ZM_ABILITY_AQUIFER:
		case ZM_ABILITY_DYNAMO:
		case ZM_ABILITY_CINDERDRINK:
		case ZM_ABILITY_GRAZER:
		case ZM_ABILITY_SOLIDCORE:
		case ZM_ABILITY_HEAVYPLATE:
		case ZM_ABILITY_GOSSAMER:
		case ZM_ABILITY_DOWNDRAFT:
			return true;
		default:
			return false;
		}
	}

	bool ZM_IsSC3ModifyStatAbility(ZM_ABILITY_ID eId)
	{
		return eId == ZM_ABILITY_SUNCHASER || eId == ZM_ABILITY_STREAMLINE
			|| eId == ZM_ABILITY_GRITSTRIDE || eId == ZM_ABILITY_RIMESTRIDE
			|| eId == ZM_ABILITY_FERVOR;
	}

	bool ZM_IsSC3DamageDealtAbility(ZM_ABILITY_ID eId)
	{
		return eId == ZM_ABILITY_VERDANTSURGE || eId == ZM_ABILITY_EMBERSURGE
			|| eId == ZM_ABILITY_TIDALSURGE || eId == ZM_ABILITY_HIVESURGE
			|| eId == ZM_ABILITY_CINDERDRINK;
	}

	bool ZM_IsSC3DamageTakenAbility(ZM_ABILITY_ID eId)
	{
		return eId == ZM_ABILITY_BEDROCK || eId == ZM_ABILITY_BLUBBER
			|| eId == ZM_ABILITY_SOLIDCORE || eId == ZM_ABILITY_HEAVYPLATE
			|| eId == ZM_ABILITY_GOSSAMER || eId == ZM_ABILITY_DOWNDRAFT;
	}

	bool ZM_IsSC3TypeInteractionAbility(ZM_ABILITY_ID eId)
	{
		return eId == ZM_ABILITY_SKYWARDGRACE || eId == ZM_ABILITY_AQUIFER
			|| eId == ZM_ABILITY_DYNAMO || eId == ZM_ABILITY_CINDERDRINK
			|| eId == ZM_ABILITY_GRAZER;
	}

	// SC4 installs 15 abilities across five pfn slots: four CONTACT reactions, the
	// three stat-drop vetoes (MODIFY_STAT/ACCURACY), the six STATUS_TRY blocks
	// (major + Ownpace's volatile), and the two ACCURACY bypasses. BLOODRUSH/
	// LASTSPITE/AFTERSHOCK and the box-3 remainder stay uninstalled until SC5.
	bool ZM_IsSC4Ability(ZM_ABILITY_ID eId)
	{
		switch (eId)
		{
		case ZM_ABILITY_STATICVEIL:
		case ZM_ABILITY_CINDERSKIN:
		case ZM_ABILITY_BARBSKIN:
		case ZM_ABILITY_THORNMAIL:
		case ZM_ABILITY_IRONWILL:
		case ZM_ABILITY_KEENEYE:
		case ZM_ABILITY_DEADAIM:
		case ZM_ABILITY_WAKEFUL:
		case ZM_ABILITY_PUREBLOOD:
		case ZM_ABILITY_THAWHEART:
		case ZM_ABILITY_LIMBERLITHE:
		case ZM_ABILITY_OWNPACE:
		case ZM_ABILITY_COLDBLOOD:
		case ZM_ABILITY_GUARDIAN:
		case ZM_ABILITY_TRUESHOT:
			return true;
		default:
			return false;
		}
	}

	// pfn <-> ability-set maps for the SC4 installation frontier (spec 4). CONTACT
	// is the four skin abilities ONLY (NOT LASTSPITE/AFTERSHOCK, still SC5).
	bool ZM_IsSC4ContactAbility(ZM_ABILITY_ID eId)
	{
		return eId == ZM_ABILITY_STATICVEIL || eId == ZM_ABILITY_CINDERSKIN
			|| eId == ZM_ABILITY_BARBSKIN || eId == ZM_ABILITY_THORNMAIL;
	}

	bool ZM_IsSC4PreventStatDropAbility(ZM_ABILITY_ID eId)
	{
		return eId == ZM_ABILITY_IRONWILL || eId == ZM_ABILITY_KEENEYE
			|| eId == ZM_ABILITY_GUARDIAN;
	}

	bool ZM_IsSC4PreventMajorAbility(ZM_ABILITY_ID eId)
	{
		return eId == ZM_ABILITY_WAKEFUL || eId == ZM_ABILITY_PUREBLOOD
			|| eId == ZM_ABILITY_THAWHEART || eId == ZM_ABILITY_LIMBERLITHE
			|| eId == ZM_ABILITY_COLDBLOOD;
	}

	bool ZM_IsSC4PreventVolatileAbility(ZM_ABILITY_ID eId)
	{
		return eId == ZM_ABILITY_OWNPACE;
	}

	bool ZM_IsSC4BypassAccuracyAbility(ZM_ABILITY_ID eId)
	{
		return eId == ZM_ABILITY_DEADAIM || eId == ZM_ABILITY_TRUESHOT;
	}

	// SC5 closes the roster with the last nine rows across three new pfn slots:
	// pfnOnTurnEnd (the five TURN_END self-heals), pfnOnDealtFaint (Bloodrush), and
	// two more pfnOnContact bodies (Lastspite/Aftershock's bSelfFainted branch).
	// Quickdraw stays engine-side only (its row remains all-null pfn).
	bool ZM_IsSC5Ability(ZM_ABILITY_ID eId)
	{
		switch (eId)
		{
		case ZM_ABILITY_BLOODRUSH:   case ZM_ABILITY_LASTSPITE: case ZM_ABILITY_AFTERSHOCK:
		case ZM_ABILITY_RAINBASK:    case ZM_ABILITY_SUNBASK:   case ZM_ABILITY_ICEBOUND:
		case ZM_ABILITY_TOXICTHRIVE: case ZM_ABILITY_ROOTFEED:  case ZM_ABILITY_QUICKDRAW:
			return true;
		default:
			return false;
		}
	}

	// The five TURN_END self-heal rows (pfnOnTurnEnd). Rainbask/Sunbask/Icebound also
	// read WEATHER engine-side; Toxicthrive/Rootfeed are pure TURN_END.
	bool ZM_IsSC5TurnEndAbility(ZM_ABILITY_ID eId)
	{
		return eId == ZM_ABILITY_RAINBASK || eId == ZM_ABILITY_SUNBASK
			|| eId == ZM_ABILITY_ICEBOUND || eId == ZM_ABILITY_TOXICTHRIVE
			|| eId == ZM_ABILITY_ROOTFEED;
	}

	// The pfnOnContact slot is now the four SC4 skins PLUS Lastspite/Aftershock.
	bool ZM_IsContactSlotAbility(ZM_ABILITY_ID eId)
	{
		return ZM_IsSC4ContactAbility(eId)
			|| eId == ZM_ABILITY_LASTSPITE || eId == ZM_ABILITY_AFTERSHOCK;
	}

	// True iff EVERY one of the twelve executable pfn slots on a row is null. After
	// SC5 exactly one real row (Quickdraw, engine-side only) is all-null.
	bool ZM_AllPfnSlotsNull(const ZM_AbilityHooks& xHooks)
	{
		return xHooks.pfnOnSwitchIn == nullptr && xHooks.pfnModifyStat == nullptr
			&& xHooks.pfnPreventStatDrop == nullptr && xHooks.pfnModifyDamageDealt == nullptr
			&& xHooks.pfnModifyDamageTaken == nullptr && xHooks.pfnPreventMajor == nullptr
			&& xHooks.pfnPreventVolatile == nullptr && xHooks.pfnOnContact == nullptr
			&& xHooks.pfnOnTurnEnd == nullptr && xHooks.pfnOnDealtFaint == nullptr
			&& xHooks.pfnBypassAccuracy == nullptr && xHooks.pfnTypeInteraction == nullptr;
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

// SC5 closing gate: EVERY one of the 50 rows now completely realizes its declared
// mask through a live pfn and/or one of the explicitly documented engine-side
// mechanisms (Trueshot/Rainbask WEATHER; Quickdraw/Bloodrush MODIFY_STAT). The
// SC-membership guard is gone -- the frontier has reached all 50, so no row may
// leave a declared bit unrealized. This is THE complete-realization gate.
ZENITH_TEST(ZM_Data, Abilities_HookTableAll50RowsRealizeDeclaredMasks)
{
	for (u_int i = 0u; i < ZM_ABILITY_COUNT; ++i)
	{
		const ZM_ABILITY_ID eId = (ZM_ABILITY_ID)i;
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
			"ability %s has an unrealized declared bit after SC5", Ab(i).m_szName);
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

ZENITH_TEST(ZM_Data, Abilities_HookTableAll50SlotsMatchRosterExactly)
{
	for (u_int i = 0u; i < ZM_ABILITY_COUNT; ++i)
	{
		const ZM_ABILITY_ID eId = (ZM_ABILITY_ID)i;
		const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(eId);
		ZENITH_ASSERT_NOT_NULL(pxHooks);
		if (pxHooks != nullptr)
		{
			ZENITH_ASSERT_EQ(pxHooks->pfnOnSwitchIn != nullptr, ZM_IsSC2SwitchInAbility(eId),
				"%s has the wrong through-SC4 SWITCH_IN installation state", Ab(i).m_szName);
			ZENITH_ASSERT_EQ(pxHooks->pfnModifyStat != nullptr, ZM_IsSC3ModifyStatAbility(eId),
				"%s has the wrong through-SC4 MODIFY_STAT installation state", Ab(i).m_szName);
			ZENITH_ASSERT_EQ(pxHooks->pfnModifyDamageDealt != nullptr, ZM_IsSC3DamageDealtAbility(eId),
				"%s has the wrong through-SC4 MODIFY_DAMAGE_DEALT installation state", Ab(i).m_szName);
			ZENITH_ASSERT_EQ(pxHooks->pfnModifyDamageTaken != nullptr, ZM_IsSC3DamageTakenAbility(eId),
				"%s has the wrong through-SC4 MODIFY_DAMAGE_TAKEN installation state", Ab(i).m_szName);
			ZENITH_ASSERT_EQ(pxHooks->pfnTypeInteraction != nullptr, ZM_IsSC3TypeInteractionAbility(eId),
				"%s has the wrong through-SC4 TYPE_IMMUNITY installation state", Ab(i).m_szName);

			// CONTACT is now the four SC4 skins PLUS the SC5 Lastspite/Aftershock bodies.
			ZENITH_ASSERT_EQ(pxHooks->pfnOnContact != nullptr, ZM_IsContactSlotAbility(eId),
				"%s has the wrong CONTACT installation state", Ab(i).m_szName);
			ZENITH_ASSERT_EQ(pxHooks->pfnPreventStatDrop != nullptr, ZM_IsSC4PreventStatDropAbility(eId),
				"%s has the wrong stat-drop-veto installation state", Ab(i).m_szName);
			ZENITH_ASSERT_EQ(pxHooks->pfnPreventMajor != nullptr, ZM_IsSC4PreventMajorAbility(eId),
				"%s has the wrong STATUS_TRY(major) installation state", Ab(i).m_szName);
			ZENITH_ASSERT_EQ(pxHooks->pfnPreventVolatile != nullptr, ZM_IsSC4PreventVolatileAbility(eId),
				"%s has the wrong STATUS_TRY(volatile) installation state", Ab(i).m_szName);
			ZENITH_ASSERT_EQ(pxHooks->pfnBypassAccuracy != nullptr, ZM_IsSC4BypassAccuracyAbility(eId),
				"%s has the wrong ACCURACY-bypass installation state", Ab(i).m_szName);

			// SC5 slots are now INSTALLED, pinned to their exact ability sets: the five
			// TURN_END self-heals and Bloodrush's dealt-faint. Every executable slot is
			// thus accounted for, and Quickdraw remains the sole all-null-pfn row (its
			// pfnModifyStat is covered null by the ZM_IsSC3ModifyStatAbility assertion above).
			ZENITH_ASSERT_EQ(pxHooks->pfnOnTurnEnd != nullptr, ZM_IsSC5TurnEndAbility(eId),
				"%s has the wrong TURN_END installation state", Ab(i).m_szName);
			ZENITH_ASSERT_EQ(pxHooks->pfnOnDealtFaint != nullptr, eId == ZM_ABILITY_BLOODRUSH,
				"%s has the wrong FAINT(dealt) installation state", Ab(i).m_szName);
		}
	}
}

// The SC5 closing partition + no-null-executable-slot gate. The four SC installation
// sets (SC2 switch-in 6, SC3 20, SC4 15, SC5 9) partition all 50 rows exactly once,
// and after SC5 every real row carries at least one live pfn EXCEPT Quickdraw, which
// is the sole all-null-pfn row (its MODIFY_STAT bit is realized engine-side). This is
// the "zero uninstalled executable slots remain" wall.
ZENITH_TEST(ZM_Data, Abilities_HookTableAll50InstalledAndOnlyQuickdrawIsAllNull)
{
	u_int uAllNullCount = 0u;
	for (u_int i = 0u; i < ZM_ABILITY_COUNT; ++i)
	{
		const ZM_ABILITY_ID eId = (ZM_ABILITY_ID)i;

		// Every row belongs to exactly one SC installation set (6 + 20 + 15 + 9 == 50).
		const u_int uMemberships =
			(ZM_IsSC2SwitchInAbility(eId) ? 1u : 0u) + (ZM_IsSC3Ability(eId) ? 1u : 0u)
			+ (ZM_IsSC4Ability(eId) ? 1u : 0u) + (ZM_IsSC5Ability(eId) ? 1u : 0u);
		ZENITH_ASSERT_EQ(uMemberships, 1u,
			"%s must belong to exactly one SC installation set", Ab(i).m_szName);

		const ZM_AbilityHooks* pxHooks = ZM_GetAbilityHooks(eId);
		ZENITH_ASSERT_NOT_NULL(pxHooks);
		if (pxHooks == nullptr) { continue; }

		const bool bAllNull = ZM_AllPfnSlotsNull(*pxHooks);
		if (bAllNull) { ++uAllNullCount; }
		// Quickdraw is the ONLY row permitted to leave every executable slot null.
		ZENITH_ASSERT_EQ(bAllNull, eId == ZM_ABILITY_QUICKDRAW,
			"%s: only Quickdraw may be all-null-pfn after SC5", Ab(i).m_szName);
	}
	ZENITH_ASSERT_EQ(uAllNullCount, 1u, "exactly one row (Quickdraw) is all-null-pfn after SC5");
}
