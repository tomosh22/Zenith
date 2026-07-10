#include "Zenith.h"

// ============================================================================
// ZM_Tests_Moves -- schema integrity of the move table (category ZM_Data). The
// move rows are inert data until the S2 ZM_MoveExecutor interprets them, so this
// suite is the contract that keeps the ~220 rows well-formed: index consistency,
// per-field ranges, the power/category/effect/target cross-rules, and -- the
// load-bearing one -- that every ZM_MOVE_EFFECT kind is used by at least one row
// (so each of S2's per-effect scenarios has a subject). See DecisionLog ZM-D-022.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Data/ZM_Types.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"

#include <cstring>   // strcmp

namespace
{
	constexpr u_int uEXPECTED_MOVES = 218;
	constexpr u_int uMAX_PP = 40;

	const ZM_MoveData& Mv(u_int uIndex)
	{
		return ZM_GetMoveData((ZM_MOVE_ID)uIndex);
	}

	// Independent restatement of the two "own-side" effect buckets (the data
	// generator derives target from these; the test re-derives to catch drift).
	bool IsSelfSideEffect(ZM_MOVE_EFFECT e)
	{
		switch (e)
		{
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
		case ZM_MOVE_EFFECT_HEAL_HALF:
		case ZM_MOVE_EFFECT_REST:
		case ZM_MOVE_EFFECT_CURE_STATUS:
		case ZM_MOVE_EFFECT_HEAL_BELL:
		case ZM_MOVE_EFFECT_PROTECT:
		case ZM_MOVE_EFFECT_ENDURE:
			return true;
		default:
			return false;
		}
	}

	bool IsFieldEffect(ZM_MOVE_EFFECT e)
	{
		switch (e)
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

	// Damaging kinds that legitimately carry zero power (damage is computed, not
	// scaled off a base power).
	bool IsFixedDamageEffect(ZM_MOVE_EFFECT e)
	{
		return e == ZM_MOVE_EFFECT_FIXED_LEVEL
			|| e == ZM_MOVE_EFFECT_HALVE_HP
			|| e == ZM_MOVE_EFFECT_OHKO;
	}

	// Effects whose magnitude is a stat-stage count in [1,3].
	bool IsStatStageEffect(ZM_MOVE_EFFECT e)
	{
		if (IsSelfSideEffect(e))
		{
			// heal/rest/cure/protect/endure are self-side but not stat stages.
			return e != ZM_MOVE_EFFECT_HEAL_HALF
				&& e != ZM_MOVE_EFFECT_REST
				&& e != ZM_MOVE_EFFECT_CURE_STATUS
				&& e != ZM_MOVE_EFFECT_HEAL_BELL
				&& e != ZM_MOVE_EFFECT_PROTECT
				&& e != ZM_MOVE_EFFECT_ENDURE;
		}
		switch (e)
		{
		case ZM_MOVE_EFFECT_LOWER_ATTACK:
		case ZM_MOVE_EFFECT_LOWER_DEFENSE:
		case ZM_MOVE_EFFECT_LOWER_SPATTACK:
		case ZM_MOVE_EFFECT_LOWER_SPDEFENSE:
		case ZM_MOVE_EFFECT_LOWER_SPEED:
		case ZM_MOVE_EFFECT_LOWER_ACCURACY:
		case ZM_MOVE_EFFECT_LOWER_EVASION:
		case ZM_MOVE_EFFECT_SWAGGER:
			return true;
		default:
			return false;
		}
	}
}

// The roster is exactly the expected size and the table is index-consistent.
ZENITH_TEST(ZM_Data, Moves_CountAndSelfConsistent)
{
	ZENITH_ASSERT_EQ(ZM_GetMoveCount(), uEXPECTED_MOVES);
	ZENITH_ASSERT_EQ((u_int)ZM_MOVE_COUNT, uEXPECTED_MOVES);
	ZENITH_ASSERT_EQ((u_int)ZM_MOVE_NONE, (u_int)ZM_MOVE_COUNT);

	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)Mv(i).m_eId, i, "move row %u has mismatched m_eId", i);
	}
}

// Every name is present and unique.
ZENITH_TEST(ZM_Data, Moves_NamesUniqueNonEmpty)
{
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		const char* szA = Mv(i).m_szName;
		ZENITH_ASSERT_NOT_NULL(szA);
		ZENITH_ASSERT_TRUE(szA[0] != '\0', "move %u has an empty name", i);
		for (u_int j = i + 1; j < ZM_MOVE_COUNT; ++j)
		{
			ZENITH_ASSERT_FALSE(strcmp(szA, Mv(j).m_szName) == 0,
				"duplicate move name '%s' at %u and %u", szA, i, j);
		}
	}
}

// Every move has a real element (never NONE/out of range).
ZENITH_TEST(ZM_Data, Moves_TypesValid)
{
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		ZENITH_ASSERT_TRUE(Mv(i).m_eType < ZM_TYPE_COUNT,
			"%s has a non-real type", Mv(i).m_szName);
	}
}

// Category / target / effect enums are all in range.
ZENITH_TEST(ZM_Data, Moves_EnumFieldsValid)
{
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		const ZM_MoveData& x = Mv(i);
		ZENITH_ASSERT_TRUE(x.m_eCategory < ZM_MOVE_CATEGORY_COUNT, "%s bad category", x.m_szName);
		ZENITH_ASSERT_TRUE(x.m_eTarget < ZM_MOVE_TARGET_COUNT, "%s bad target", x.m_szName);
		ZENITH_ASSERT_TRUE(x.m_eEffect < ZM_MOVE_EFFECT_COUNT, "%s bad effect", x.m_szName);
	}
}

// Power vs category: STATUS moves are powerless; damaging moves have power in
// [10,250] unless they are a fixed-damage kind (which is powerless by design).
ZENITH_TEST(ZM_Data, Moves_PowerCategoryConsistent)
{
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		const ZM_MoveData& x = Mv(i);
		if (x.m_eCategory == ZM_MOVE_CATEGORY_STATUS)
		{
			ZENITH_ASSERT_EQ(x.m_uPower, 0u, "status move %s has power", x.m_szName);
		}
		else if (IsFixedDamageEffect(x.m_eEffect))
		{
			ZENITH_ASSERT_EQ(x.m_uPower, 0u, "fixed-damage move %s should have 0 power", x.m_szName);
		}
		else
		{
			ZENITH_ASSERT_GE(x.m_uPower, 10u, "%s power too low", x.m_szName);
			ZENITH_ASSERT_LE(x.m_uPower, 250u, "%s power too high", x.m_szName);
		}
	}
}

// Accuracy in [0,100]; own-side (SELF/FIELD) moves never miss (accuracy == 0).
ZENITH_TEST(ZM_Data, Moves_AccuracyInRange)
{
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		const ZM_MoveData& x = Mv(i);
		ZENITH_ASSERT_LE(x.m_uAccuracy, 100u, "%s accuracy > 100", x.m_szName);
		if (x.m_eTarget == ZM_MOVE_TARGET_SELF || x.m_eTarget == ZM_MOVE_TARGET_FIELD)
		{
			ZENITH_ASSERT_EQ(x.m_uAccuracy, uZM_MOVE_ACCURACY_ALWAYS_HITS,
				"own-side move %s must never miss", x.m_szName);
		}
	}
}

// PP in [1,40]; priority in [-7,5]; crit stage in [0,2].
ZENITH_TEST(ZM_Data, Moves_NumericFieldsInRange)
{
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		const ZM_MoveData& x = Mv(i);
		ZENITH_ASSERT_GE(x.m_uPP, 1u, "%s pp < 1", x.m_szName);
		ZENITH_ASSERT_LE(x.m_uPP, uMAX_PP, "%s pp > max", x.m_szName);
		ZENITH_ASSERT_GE(x.m_iPriority, -7, "%s priority < -7", x.m_szName);
		ZENITH_ASSERT_LE(x.m_iPriority, 5, "%s priority > 5", x.m_szName);
		ZENITH_ASSERT_LE(x.m_uCritStage, 2u, "%s crit stage > 2", x.m_szName);
	}
}

// Effect chance is 0 exactly when the effect is NONE; a status move always
// carries a real effect; chance never exceeds 100.
ZENITH_TEST(ZM_Data, Moves_EffectChanceConsistent)
{
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		const ZM_MoveData& x = Mv(i);
		ZENITH_ASSERT_LE(x.m_uEffectChance, 100u, "%s chance > 100", x.m_szName);
		if (x.m_eEffect == ZM_MOVE_EFFECT_NONE)
		{
			ZENITH_ASSERT_EQ(x.m_uEffectChance, 0u, "NONE-effect %s has a chance", x.m_szName);
		}
		else
		{
			ZENITH_ASSERT_GE(x.m_uEffectChance, 1u, "%s effect has 0 chance", x.m_szName);
		}
		if (x.m_eCategory == ZM_MOVE_CATEGORY_STATUS)
		{
			ZENITH_ASSERT_NE((u_int)x.m_eEffect, (u_int)ZM_MOVE_EFFECT_NONE,
				"status move %s does nothing", x.m_szName);
		}
	}
}

// Target follows from category + effect: attacks hit the opponent; self-side
// effects target SELF; field effects target FIELD; the remaining status effects
// target the opponent.
ZENITH_TEST(ZM_Data, Moves_TargetMatchesEffect)
{
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		const ZM_MoveData& x = Mv(i);
		ZM_MOVE_TARGET eExpected;
		if (x.m_eCategory != ZM_MOVE_CATEGORY_STATUS)
		{
			eExpected = ZM_MOVE_TARGET_OPPONENT;
		}
		else if (IsFieldEffect(x.m_eEffect))
		{
			eExpected = ZM_MOVE_TARGET_FIELD;
		}
		else if (IsSelfSideEffect(x.m_eEffect))
		{
			eExpected = ZM_MOVE_TARGET_SELF;
		}
		else
		{
			eExpected = ZM_MOVE_TARGET_OPPONENT;
		}
		ZENITH_ASSERT_EQ((u_int)x.m_eTarget, (u_int)eExpected,
			"%s target does not match its category/effect", x.m_szName);
	}
}

// Magnitude: stat-stage effects use [1,3]; nothing is negative.
ZENITH_TEST(ZM_Data, Moves_EffectMagnitudeInRange)
{
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		const ZM_MoveData& x = Mv(i);
		ZENITH_ASSERT_GE(x.m_iEffectMagnitude, 0, "%s negative magnitude", x.m_szName);
		if (IsStatStageEffect(x.m_eEffect))
		{
			ZENITH_ASSERT_GE(x.m_iEffectMagnitude, 1, "%s stat magnitude < 1", x.m_szName);
			ZENITH_ASSERT_LE(x.m_iEffectMagnitude, 3, "%s stat magnitude > 3", x.m_szName);
		}
	}
}

// The load-bearing coverage lock: every effect kind is exercised by >= 1 move,
// so every S2 per-effect executor scenario has a data subject.
ZENITH_TEST(ZM_Data, Moves_EveryEffectKindUsed)
{
	for (u_int e = 0; e < ZM_MOVE_EFFECT_COUNT; ++e)
	{
		bool bFound = false;
		for (u_int i = 0; i < ZM_MOVE_COUNT && !bFound; ++i)
		{
			bFound = ((u_int)Mv(i).m_eEffect == e);
		}
		ZENITH_ASSERT_TRUE(bFound, "no move uses effect kind %u", e);
	}
}

// Every one of the 18 types has at least one move (no type is unarmed).
ZENITH_TEST(ZM_Data, Moves_EveryTypeHasAMove)
{
	for (u_int t = 0; t < ZM_TYPE_COUNT; ++t)
	{
		bool bFound = false;
		for (u_int i = 0; i < ZM_MOVE_COUNT && !bFound; ++i)
		{
			bFound = ((u_int)Mv(i).m_eType == t);
		}
		ZENITH_ASSERT_TRUE(bFound, "type %s has no move", ZM_TypeToString((ZM_TYPE)t));
	}
}

// Category spread: a healthy mix of physical / special / status moves.
ZENITH_TEST(ZM_Data, Moves_CategorySpread)
{
	u_int auCount[ZM_MOVE_CATEGORY_COUNT] = { 0, 0, 0 };
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		auCount[Mv(i).m_eCategory]++;
	}
	ZENITH_ASSERT_GE(auCount[ZM_MOVE_CATEGORY_PHYSICAL], 30u, "too few physical moves");
	ZENITH_ASSERT_GE(auCount[ZM_MOVE_CATEGORY_SPECIAL], 30u, "too few special moves");
	ZENITH_ASSERT_GE(auCount[ZM_MOVE_CATEGORY_STATUS], 20u, "too few status moves");
}

// Priority brackets and high-crit moves both exist (needed by S2 turn order /
// crit tests).
ZENITH_TEST(ZM_Data, Moves_PriorityAndCritPresent)
{
	bool bHiPrio = false, bLoPrio = false, bHiCrit = false;
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		const ZM_MoveData& x = Mv(i);
		if (x.m_iPriority > 0) { bHiPrio = true; }
		if (x.m_iPriority < 0) { bLoPrio = true; }
		if (x.m_uCritStage > 0) { bHiCrit = true; }
	}
	ZENITH_ASSERT_TRUE(bHiPrio, "no positive-priority move");
	ZENITH_ASSERT_TRUE(bLoPrio, "no negative-priority move");
	ZENITH_ASSERT_TRUE(bHiCrit, "no elevated-crit move");
}

// Accessors: bounds-consistent data, and out-of-range name lookup is safe.
ZENITH_TEST(ZM_Data, Moves_AccessorContract)
{
	for (u_int i = 0; i < ZM_MOVE_COUNT; ++i)
	{
		ZENITH_ASSERT_STREQ(ZM_GetMoveName((ZM_MOVE_ID)i), Mv(i).m_szName);
	}
	ZENITH_ASSERT_STREQ(ZM_GetMoveName(ZM_MOVE_NONE), "NONE");
	ZENITH_ASSERT_STREQ(ZM_GetMoveName((ZM_MOVE_ID)(ZM_MOVE_COUNT + 99)), "NONE");
}

// Enum-to-string helpers cover every value and reject out-of-range.
ZENITH_TEST(ZM_Data, Moves_ToStringContracts)
{
	for (u_int c = 0; c < ZM_MOVE_CATEGORY_COUNT; ++c)
	{
		const char* sz = ZM_MoveCategoryToString((ZM_MOVE_CATEGORY)c);
		ZENITH_ASSERT_NOT_NULL(sz);
		ZENITH_ASSERT_FALSE(strcmp(sz, "INVALID") == 0, "category %u stringifies as INVALID", c);
	}
	ZENITH_ASSERT_STREQ(ZM_MoveCategoryToString(ZM_MOVE_CATEGORY_COUNT), "INVALID");

	for (u_int t = 0; t < ZM_MOVE_TARGET_COUNT; ++t)
	{
		const char* sz = ZM_MoveTargetToString((ZM_MOVE_TARGET)t);
		ZENITH_ASSERT_NOT_NULL(sz);
		ZENITH_ASSERT_FALSE(strcmp(sz, "INVALID") == 0, "target %u stringifies as INVALID", t);
	}
	ZENITH_ASSERT_STREQ(ZM_MoveTargetToString(ZM_MOVE_TARGET_COUNT), "INVALID");
}
