#include "Zenith.h"

// ============================================================================
// ZM_Tests_Abilities -- integrity of the ability roster (category ZM_Data). S1
// ships roster + metadata + each ability's declared HOOK SURFACE bitmask; the
// fn-pointer hook bodies are S2. This suite locks the roster and the mask
// contract: valid non-zero masks with no stray bits, every hook bit covered by
// at least one ability, and the hook-query accessor. See DecisionLog ZM-D-026.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_AbilityData.h"

#include <cstring>

namespace
{
	constexpr u_int uEXPECTED_ABILITIES = 50;

	const ZM_AbilityData& Ab(u_int i) { return ZM_GetAbilityData((ZM_ABILITY_ID)i); }
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
