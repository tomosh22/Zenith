#include "Zenith.h"

// ============================================================================
// ZM_Tests_SaveModel -- S7 item 1 SC1 RED contract for the complete durable
// in-memory game model. These tests intentionally name production APIs which
// do not exist yet; the production pass owns making this suite compile and pass.
// Pure and headless: no ECS, scene, renderer, disk I/O, or serialization mocks.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Zenithmon/Source/Party/ZM_Monster.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Party/ZM_BoxStorage.h"
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"
#include "Zenithmon/Source/Battle/ZM_BattleTower.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"

namespace
{
	void HashBoxValue(u_int64& ulFingerprint, u_int64 ulValue)
	{
		ulFingerprint ^= ulValue;
		ulFingerprint *= 1099511628211ull;
	}

	u_int64 BoxFingerprint(const ZM_BoxStorage& xBoxes)
	{
		u_int64 ulFingerprint = 1469598103934665603ull;
		for (u_int uBox = 0u; uBox < uZM_BOX_COUNT; ++uBox)
		{
			for (u_int uSlot = 0u; uSlot < uZM_BOX_SLOTS_PER_BOX; ++uSlot)
			{
				const ZM_Monster* pxMonster = xBoxes.TryGet(uBox, uSlot);
				HashBoxValue(ulFingerprint, (u_int64)uBox);
				HashBoxValue(ulFingerprint, (u_int64)uSlot);
				HashBoxValue(ulFingerprint, pxMonster == nullptr ? 0ull : 1ull);
				if (pxMonster == nullptr) { continue; }

				HashBoxValue(ulFingerprint, (u_int64)(u_int)pxMonster->m_eSpecies);
				HashBoxValue(ulFingerprint, (u_int64)pxMonster->m_uLevel);
				HashBoxValue(ulFingerprint, (u_int64)pxMonster->m_uCurrentExp);
				for (u_int i = 0u; i < ZM_STAT_COUNT; ++i)
				{
					HashBoxValue(ulFingerprint, (u_int64)pxMonster->m_auIV[i]);
					HashBoxValue(ulFingerprint, (u_int64)pxMonster->m_auEV[i]);
				}
				HashBoxValue(ulFingerprint, (u_int64)(u_int)pxMonster->m_eNature);
				HashBoxValue(ulFingerprint, (u_int64)(u_int)pxMonster->m_eAbility);
				HashBoxValue(ulFingerprint, (u_int64)(u_int)pxMonster->m_eStatus);
				for (u_int i = 0u; i < uZM_MAX_MOVES; ++i)
				{
					HashBoxValue(ulFingerprint, (u_int64)(u_int)pxMonster->m_axMoves[i].m_eMove);
					HashBoxValue(ulFingerprint, (u_int64)pxMonster->m_axMoves[i].m_uCurPP);
					HashBoxValue(ulFingerprint, (u_int64)pxMonster->m_axMoves[i].m_uMaxPP);
				}
				HashBoxValue(ulFingerprint, (u_int64)pxMonster->m_uCurrentHp);
				HashBoxValue(ulFingerprint, (u_int64)(u_int)pxMonster->m_eGender);
				HashBoxValue(ulFingerprint, (u_int64)pxMonster->m_uFlags);
				HashBoxValue(ulFingerprint, (u_int64)pxMonster->m_uFriendship);
				for (u_int i = 0u; i < uZM_MONSTER_NICKNAME_CAPACITY; ++i)
				{
					HashBoxValue(ulFingerprint, (u_int64)(unsigned char)pxMonster->m_szNickname[i]);
				}
			}
		}
		return ulFingerprint;
	}
}

ZENITH_TEST(ZM_Save, Monster_FriendshipDefaultsZeroAndNicknameDefaultsEmpty)
{
	const ZM_Monster xDefault;
	const ZM_Monster xBuilt = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, 5u);
	const ZM_BattleMonster xCaught =
		ZM_BuildBattleMonster(ZM_BuildWildEnemySpec(ZM_SPECIES_KINDLET, 3u));
	const ZM_Monster xFromBattle = ZM_MonsterFromBattleMonster(xCaught);

	ZENITH_ASSERT_EQ(xDefault.m_uFriendship, 0u, "a value-initialized record starts at zero friendship");
	ZENITH_ASSERT_EQ(xBuilt.m_uFriendship, 0u, "the ordinary record factory starts at zero friendship");
	ZENITH_ASSERT_EQ(xFromBattle.m_uFriendship, 0u,
		"a caught-monster conversion starts at zero friendship");
	ZENITH_ASSERT_EQ((u_int)sizeof(xDefault.m_szNickname), uZM_MONSTER_NICKNAME_CAPACITY,
		"the durable nickname buffer has the locked 16-byte width");
	ZENITH_ASSERT_EQ(uZM_MONSTER_NICKNAME_CAPACITY, 16u, "nickname capacity is format-locked");
	for (u_int i = 0u; i < uZM_MONSTER_NICKNAME_CAPACITY; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)(unsigned char)xDefault.m_szNickname[i], 0u,
			"default nickname byte %u is zero", i);
		ZENITH_ASSERT_EQ((u_int)(unsigned char)xBuilt.m_szNickname[i], 0u,
			"factory nickname byte %u is zero", i);
		ZENITH_ASSERT_EQ((u_int)(unsigned char)xFromBattle.m_szNickname[i], 0u,
			"caught-monster nickname byte %u is zero", i);
	}
}

ZENITH_TEST(ZM_Save, Monster_DurableFieldsKeepCurrentHpGenderAndDistinctPp)
{
	ZM_Monster xOriginal = ZM_BuildMonsterRecord(ZM_SPECIES_KINDLET, 12u);
	xOriginal.m_uCurrentHp = 3u;
	xOriginal.m_eGender = ZM_GENDER_FEMALE;
	xOriginal.m_uFriendship = 201u;
	xOriginal.m_axMoves[0].m_uCurPP = 2u;
	xOriginal.m_axMoves[0].m_uMaxPP = 17u;
	xOriginal.m_szNickname[0] = 'E';
	xOriginal.m_szNickname[1] = 'm';
	xOriginal.m_szNickname[2] = 'b';
	xOriginal.m_szNickname[3] = 'e';
	xOriginal.m_szNickname[4] = 'r';
	xOriginal.m_szNickname[5] = '\0';

	const ZM_Monster xDurableCopy = xOriginal;
	ZENITH_ASSERT_EQ(xDurableCopy.m_uCurrentHp, 3u, "damaged current HP remains instance state");
	ZENITH_ASSERT_EQ((u_int)xDurableCopy.m_eGender, (u_int)ZM_GENDER_FEMALE,
		"gender remains instance state");
	ZENITH_ASSERT_EQ(xDurableCopy.m_uFriendship, 201u, "friendship remains instance state");
	ZENITH_ASSERT_EQ(xDurableCopy.m_axMoves[0].m_uCurPP, 2u, "current PP remains distinct");
	ZENITH_ASSERT_EQ(xDurableCopy.m_axMoves[0].m_uMaxPP, 17u, "maximum PP remains distinct");
	ZENITH_ASSERT_NE(xDurableCopy.m_axMoves[0].m_uCurPP, xDurableCopy.m_axMoves[0].m_uMaxPP,
		"spent PP must not be normalized to maximum PP");
	ZENITH_ASSERT_TRUE(xDurableCopy.m_szNickname[0] == 'E' && xDurableCopy.m_szNickname[4] == 'r' &&
		xDurableCopy.m_szNickname[5] == '\0', "nickname bytes remain instance state");
}

ZENITH_TEST(ZM_Save, MonsterFromBattle_NoneAbilityNormalizesToRegular)
{
	ZM_BattleMonster xCaught = ZM_BuildBattleMonster(ZM_BuildWildEnemySpec(ZM_SPECIES_FERNFAWN, 5u));
	const ZM_SpeciesAbilities xAbilities = ZM_GetSpeciesAbilities(ZM_SPECIES_FERNFAWN);

	xCaught.m_eAbility = ZM_ABILITY_NONE;
	const ZM_Monster xNormalized = ZM_MonsterFromBattleMonster(xCaught);
	ZENITH_ASSERT_EQ((u_int)xNormalized.m_eAbility, (u_int)xAbilities.m_eRegular,
		"NONE from an authored wild battle normalizes to the species regular ability");
	ZENITH_ASSERT_NE((u_int)xNormalized.m_eAbility, (u_int)ZM_ABILITY_NONE,
		"the durable record never retains the battle authoring sentinel");

	xCaught.m_eAbility = xAbilities.m_eHidden;
	const ZM_Monster xExplicit = ZM_MonsterFromBattleMonster(xCaught);
	ZENITH_ASSERT_EQ((u_int)xExplicit.m_eAbility, (u_int)xAbilities.m_eHidden,
		"an explicit non-NONE ability is preserved rather than normalized");
}

ZENITH_TEST(ZM_Save, Boxes_DefaultShapeIsSixteenByThirtyAndEmpty)
{
	const ZM_BoxStorage xBoxes;
	ZENITH_ASSERT_EQ(uZM_BOX_COUNT, 16u, "the save model owns sixteen boxes");
	ZENITH_ASSERT_EQ(uZM_BOX_SLOTS_PER_BOX, 30u, "each box owns thirty slots");
	ZENITH_ASSERT_EQ(xBoxes.Count(), 0u, "new storage is empty");
	ZENITH_ASSERT_FALSE(xBoxes.IsFull(), "new storage is not full");
	for (u_int uBox = 0u; uBox < uZM_BOX_COUNT; ++uBox)
	{
		for (u_int uSlot = 0u; uSlot < uZM_BOX_SLOTS_PER_BOX; ++uSlot)
		{
			ZENITH_ASSERT_TRUE(xBoxes.TryGet(uBox, uSlot) == nullptr,
				"default box %u slot %u is unoccupied", uBox, uSlot);
		}
	}
}

ZENITH_TEST(ZM_Save, Boxes_StoreFirstFreeUsesBoxMajorSlotMajorOrder)
{
	ZM_BoxStorage xBoxes;
	for (u_int i = 0u; i < uZM_BOX_SLOTS_PER_BOX + 1u; ++i)
	{
		const ZM_Monster xMonster = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, i + 1u);
		ZENITH_ASSERT_TRUE(xBoxes.StoreFirstFree(xMonster), "store ordered record %u", i);
	}

	ZENITH_ASSERT_EQ(xBoxes.Count(), uZM_BOX_SLOTS_PER_BOX + 1u, "all 31 records are stored");
	const ZM_Monster* pxFirst = xBoxes.TryGet(0u, 0u);
	const ZM_Monster* pxLastFirstBox = xBoxes.TryGet(0u, uZM_BOX_SLOTS_PER_BOX - 1u);
	const ZM_Monster* pxFirstSecondBox = xBoxes.TryGet(1u, 0u);
	ZENITH_ASSERT_TRUE(pxFirst != nullptr, "the first record occupies box 0 slot 0");
	ZENITH_ASSERT_TRUE(pxLastFirstBox != nullptr, "the thirtieth record occupies box 0 slot 29");
	ZENITH_ASSERT_TRUE(pxFirstSecondBox != nullptr, "the thirty-first record occupies box 1 slot 0");
	if (pxFirst != nullptr)
	{
		ZENITH_ASSERT_EQ(pxFirst->m_uLevel, 1u, "box-major insertion starts at box 0 slot 0");
	}
	if (pxLastFirstBox != nullptr)
	{
		ZENITH_ASSERT_EQ(pxLastFirstBox->m_uLevel, 30u, "slot-major insertion fills box 0 before box 1");
	}
	if (pxFirstSecondBox != nullptr)
	{
		ZENITH_ASSERT_EQ(pxFirstSecondBox->m_uLevel, 31u, "overflow continues at box 1 slot 0");
	}
	ZENITH_ASSERT_TRUE(xBoxes.TryGet(1u, 1u) == nullptr, "the next slot remains empty");
}

ZENITH_TEST(ZM_Save, Boxes_FullRejectsWithoutMutation)
{
	ZM_BoxStorage xBoxes;
	for (u_int i = 0u; i < uZM_BOX_COUNT * uZM_BOX_SLOTS_PER_BOX; ++i)
	{
		const ZM_Monster xMonster = ZM_BuildMonsterRecord(ZM_SPECIES_FERNFAWN, (i % 100u) + 1u);
		ZENITH_ASSERT_TRUE(xBoxes.StoreFirstFree(xMonster), "fill storage position %u", i);
	}
	ZENITH_ASSERT_TRUE(xBoxes.IsFull(), "all 480 slots are occupied");
	ZENITH_ASSERT_EQ(xBoxes.Count(), uZM_BOX_COUNT * uZM_BOX_SLOTS_PER_BOX,
		"the count reaches the fixed capacity");

	const u_int64 ulBefore = BoxFingerprint(xBoxes);
	const ZM_Monster xRejected = ZM_BuildMonsterRecord(ZM_SPECIES_NIBBIN, 77u);
	ZENITH_ASSERT_FALSE(xBoxes.StoreFirstFree(xRejected), "a full store rejects the incoming record");
	ZENITH_ASSERT_EQ(xBoxes.Count(), uZM_BOX_COUNT * uZM_BOX_SLOTS_PER_BOX,
		"a rejected store does not change the count");
	ZENITH_ASSERT_TRUE(BoxFingerprint(xBoxes) == ulBefore,
		"a rejected store does not overwrite any occupied slot");
}

ZENITH_TEST(ZM_Save, Boxes_BoundsSafeReadsRejectInvalidCoordinates)
{
	ZM_BoxStorage xBoxes;
	ZENITH_ASSERT_TRUE(xBoxes.TryGet(0u, 0u) == nullptr, "an in-range empty read returns null");

	const ZM_Monster xSparse = ZM_BuildMonsterRecord(ZM_SPECIES_KINDLET, 8u);
	ZENITH_ASSERT_TRUE(xBoxes.StoreAt(3u, 7u, xSparse),
		"an exact indexed write restores sparse box 3 slot 7");
	ZENITH_ASSERT_EQ(xBoxes.Count(), 1u, "an indexed write into an empty slot increments count");
	const ZM_Monster* pxSparse = xBoxes.TryGet(3u, 7u);
	ZENITH_ASSERT_TRUE(pxSparse != nullptr, "the sparse indexed slot is occupied");
	if (pxSparse != nullptr)
	{
		ZENITH_ASSERT_EQ((u_int)pxSparse->m_eSpecies, (u_int)ZM_SPECIES_KINDLET,
			"the sparse indexed slot preserves species");
		ZENITH_ASSERT_EQ(pxSparse->m_uLevel, 8u, "the sparse indexed slot preserves level");
	}
	ZENITH_ASSERT_TRUE(xBoxes.TryGet(0u, 0u) == nullptr,
		"an exact sparse write does not collapse into the first free slot");

	const ZM_Monster xReplacement = ZM_BuildMonsterRecord(ZM_SPECIES_NIBBIN, 19u);
	ZENITH_ASSERT_TRUE(xBoxes.StoreAt(3u, 7u, xReplacement),
		"an occupied indexed slot can be transactionally replaced");
	ZENITH_ASSERT_EQ(xBoxes.Count(), 1u, "replacement preserves the occupied-slot count");
	const ZM_Monster* pxReplacement = xBoxes.TryGet(3u, 7u);
	ZENITH_ASSERT_TRUE(pxReplacement != nullptr, "the replacement remains occupied");
	if (pxReplacement != nullptr)
	{
		ZENITH_ASSERT_EQ((u_int)pxReplacement->m_eSpecies, (u_int)ZM_SPECIES_NIBBIN,
			"replacement updates the exact slot's species");
		ZENITH_ASSERT_EQ(pxReplacement->m_uLevel, 19u,
			"replacement updates the exact slot's level");
	}

	const u_int64 ulBeforeInvalid = BoxFingerprint(xBoxes);
	ZENITH_ASSERT_FALSE(xBoxes.StoreAt(uZM_BOX_COUNT, 7u, xSparse),
		"one-past-end box indexed write is rejected");
	ZENITH_ASSERT_FALSE(xBoxes.StoreAt(3u, uZM_BOX_SLOTS_PER_BOX, xSparse),
		"one-past-end slot indexed write is rejected");
	ZENITH_ASSERT_FALSE(xBoxes.ClearAt(uZM_BOX_COUNT, 7u),
		"one-past-end box clear is rejected");
	ZENITH_ASSERT_FALSE(xBoxes.ClearAt(3u, uZM_BOX_SLOTS_PER_BOX),
		"one-past-end slot clear is rejected");
	ZENITH_ASSERT_EQ(xBoxes.Count(), 1u, "invalid indexed mutations preserve count");
	ZENITH_ASSERT_TRUE(BoxFingerprint(xBoxes) == ulBeforeInvalid,
		"invalid indexed writes and clears preserve the complete box fingerprint");

	ZENITH_ASSERT_TRUE(xBoxes.ClearAt(3u, 7u), "clearing an occupied exact slot succeeds");
	ZENITH_ASSERT_EQ(xBoxes.Count(), 0u, "clearing an occupied slot decrements count");
	ZENITH_ASSERT_TRUE(xBoxes.TryGet(3u, 7u) == nullptr, "the cleared exact slot reads empty");
	const u_int64 ulAfterClear = BoxFingerprint(xBoxes);
	ZENITH_ASSERT_FALSE(xBoxes.ClearAt(3u, 7u), "clearing an already-empty slot is a harmless false");
	ZENITH_ASSERT_EQ(xBoxes.Count(), 0u, "clearing an empty slot preserves count");
	ZENITH_ASSERT_TRUE(BoxFingerprint(xBoxes) == ulAfterClear,
		"clearing an empty slot preserves the complete box fingerprint");

	ZENITH_ASSERT_TRUE(xBoxes.TryGet(uZM_BOX_COUNT, 0u) == nullptr,
		"box == count is rejected without indexing");
	ZENITH_ASSERT_TRUE(xBoxes.TryGet(0u, uZM_BOX_SLOTS_PER_BOX) == nullptr,
		"slot == slots-per-box is rejected without indexing");
	ZENITH_ASSERT_TRUE(xBoxes.TryGet(uZM_BOX_COUNT + 99u, uZM_BOX_SLOTS_PER_BOX + 99u) == nullptr,
		"coordinates far outside both dimensions are rejected");
}

ZENITH_TEST(ZM_Save, Dex_MarkSeenDoesNotMarkCaught)
{
	ZM_GameState xState;
	ZENITH_ASSERT_FALSE(xState.IsSeen(ZM_SPECIES_KINDLET), "species begins unseen");
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_KINDLET), "species begins uncaught");

	xState.MarkSeen(ZM_SPECIES_KINDLET);
	ZENITH_ASSERT_TRUE(xState.IsSeen(ZM_SPECIES_KINDLET), "MarkSeen sets the seen dex");
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_KINDLET), "MarkSeen does not set the caught dex");
	ZENITH_ASSERT_EQ(xState.GetSeenCount(), 1u, "exactly one species is seen");
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 0u, "no species is caught");
}

ZENITH_TEST(ZM_Save, Dex_MarkCaughtAlsoMarksSeenAndIsIdempotent)
{
	ZM_GameState xState;
	xState.MarkCaught(ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_TRUE(xState.IsSeen(ZM_SPECIES_NIBBIN), "caught implies seen");
	ZENITH_ASSERT_TRUE(xState.IsCaught(ZM_SPECIES_NIBBIN), "MarkCaught sets the caught dex");
	ZENITH_ASSERT_EQ(xState.GetSeenCount(), 1u, "the implied seen mark counts once");
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 1u, "the caught mark counts once");

	xState.MarkCaught(ZM_SPECIES_NIBBIN);
	ZENITH_ASSERT_EQ(xState.GetSeenCount(), 1u, "repeated MarkCaught does not double-count seen");
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 1u, "repeated MarkCaught does not double-count caught");
	ZENITH_ASSERT_FALSE(xState.IsSeen(ZM_SPECIES_KINDLET), "unrelated dex entries remain clear");
	ZENITH_ASSERT_FALSE(xState.IsCaught(ZM_SPECIES_KINDLET), "unrelated caught entries remain clear");
}

ZENITH_TEST(ZM_Save, StoryFlags_SetClearCountAndBounds)
{
	ZM_StoryFlagSet xFlags;
	ZENITH_ASSERT_EQ(uZM_MAX_STORY_FLAGS, 4096u, "story flags have the locked 4096-bit capacity");
	ZENITH_ASSERT_EQ(xFlags.Count(), 0u, "new story flags are clear");
	ZENITH_ASSERT_TRUE(xFlags.Set(0u, true), "set the first valid story flag");
	ZENITH_ASSERT_TRUE(xFlags.Set(uZM_MAX_STORY_FLAGS - 1u, true), "set the last valid story flag");
	ZENITH_ASSERT_TRUE(xFlags.IsSet(0u), "first flag reads set");
	ZENITH_ASSERT_TRUE(xFlags.IsSet(uZM_MAX_STORY_FLAGS - 1u), "last flag reads set");
	ZENITH_ASSERT_EQ(xFlags.Count(), 2u, "two distinct flags count twice");
	ZENITH_ASSERT_TRUE(xFlags.Set(0u, true), "setting an already-set valid flag still succeeds");
	ZENITH_ASSERT_EQ(xFlags.Count(), 2u, "idempotent set does not inflate the count");
	ZENITH_ASSERT_TRUE(xFlags.Set(0u, false), "clear the first valid story flag");
	ZENITH_ASSERT_FALSE(xFlags.IsSet(0u), "cleared flag reads clear");
	ZENITH_ASSERT_EQ(xFlags.Count(), 1u, "clear decrements the distinct count");
	ZENITH_ASSERT_FALSE(xFlags.Set(uZM_MAX_STORY_FLAGS, true), "one-past-end set is rejected");
	ZENITH_ASSERT_FALSE(xFlags.IsSet(uZM_MAX_STORY_FLAGS), "one-past-end read is safely false");
	ZENITH_ASSERT_EQ(xFlags.Count(), 1u, "an invalid set causes no mutation");
}

ZENITH_TEST(ZM_Save, Badges_AwardQueryCountAndBounds)
{
	ZM_GameState xState;
	ZENITH_ASSERT_EQ(uZM_BADGE_COUNT, 8u, "the region owns eight badges");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 0u, "new state has no badges");
	ZENITH_ASSERT_FALSE(xState.HasBadge(0u), "badge zero begins unawarded");
	ZENITH_ASSERT_TRUE(xState.AwardBadge(0u), "award the first badge");
	ZENITH_ASSERT_TRUE(xState.AwardBadge(uZM_BADGE_COUNT - 1u), "award the last badge");
	ZENITH_ASSERT_TRUE(xState.HasBadge(0u), "first badge queries awarded");
	ZENITH_ASSERT_TRUE(xState.HasBadge(uZM_BADGE_COUNT - 1u), "last badge queries awarded");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 2u, "two distinct awards count twice");
	ZENITH_ASSERT_TRUE(xState.AwardBadge(0u), "re-awarding a valid badge is idempotent success");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 2u, "re-awarding does not inflate the count");
	ZENITH_ASSERT_FALSE(xState.AwardBadge(uZM_BADGE_COUNT), "one-past-end badge is rejected");
	ZENITH_ASSERT_FALSE(xState.HasBadge(uZM_BADGE_COUNT), "one-past-end query is safely false");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 2u, "invalid award causes no mutation");
}

ZENITH_TEST(ZM_Save, Money_OverGameplayCapIsPreservedAndCreditsNothing)
{
	ZM_GameState xState;
	xState.m_uMoney = uZM_MONEY_CAP + 17u;
	const u_int uBefore = xState.m_uMoney;

	xState.AddMoney(1u);
	ZENITH_ASSERT_EQ(xState.m_uMoney, uBefore,
		"an imported uint32 above the gameplay cap remains byte-for-byte preserved");
	xState.AddMoney(0xffffffffu);
	ZENITH_ASSERT_EQ(xState.m_uMoney, uBefore,
		"even a maximal credit is a no-op rather than clamp or wrap when already over cap");
	ZENITH_ASSERT_TRUE(xState.SpendMoney(7u), "over-cap durable money remains spendable");
	ZENITH_ASSERT_EQ(xState.m_uMoney, uBefore - 7u, "ordinary debit arithmetic still applies");
}

ZENITH_TEST(ZM_Save, Daycare_DefaultHasNoParentsEggOrProgress)
{
	const ZM_GameState xState;
	ZENITH_ASSERT_EQ(xState.m_xDaycare.m_uParentCount, 0u, "new daycare has no parents");
	ZENITH_ASSERT_EQ(uZM_DAYCARE_PARENT_CAPACITY, 2u, "daycare persists exactly two parent records");
	for (u_int i = 0u; i < uZM_DAYCARE_PARENT_CAPACITY; ++i)
	{
		ZENITH_ASSERT_EQ((u_int)xState.m_xDaycare.m_axParents[i].m_eSpecies, (u_int)ZM_SPECIES_NONE,
			"default parent record %u is empty", i);
	}
	ZENITH_ASSERT_FALSE(xState.m_xDaycare.m_bEggPresent, "new daycare has no pending egg");
	ZENITH_ASSERT_EQ((u_int)xState.m_xDaycare.m_xEgg.m_eSpecies, (u_int)ZM_SPECIES_NONE,
		"default pending egg record is empty");
	ZENITH_ASSERT_EQ(xState.m_xDaycare.m_uEggStepsRemaining, 0u,
		"new daycare has no egg hatch progress");
}

ZENITH_TEST(ZM_Save, Daycare_EggProgressIsDaycareOwned)
{
	ZM_GameState xState;
	xState.m_xDaycare.m_bEggPresent = true;
	xState.m_xDaycare.m_xEgg = ZM_BuildMonsterRecord(ZM_SPECIES_NIBBIN, 1u);
	xState.m_xDaycare.m_xEgg.m_uFlags |= uZM_MONSTER_FLAG_IS_EGG;
	xState.m_xDaycare.m_uEggStepsRemaining = 321u;

	const ZM_Monster xEggSnapshot = xState.m_xDaycare.m_xEgg;
	xState.m_xDaycare.m_uEggStepsRemaining = 123u;
	ZENITH_ASSERT_TRUE(xState.m_xDaycare.m_bEggPresent, "changing hatch progress does not consume the egg");
	ZENITH_ASSERT_EQ(xState.m_xDaycare.m_uEggStepsRemaining, 123u,
		"hatch progress lives on the daycare aggregate");
	ZENITH_ASSERT_EQ((u_int)xState.m_xDaycare.m_xEgg.m_eSpecies, (u_int)xEggSnapshot.m_eSpecies,
		"progress mutation leaves the durable egg identity unchanged");
	ZENITH_ASSERT_EQ(xState.m_xDaycare.m_xEgg.m_uFlags, xEggSnapshot.m_uFlags,
		"progress mutation leaves the durable egg flags unchanged");
	ZENITH_ASSERT_EQ(xState.m_xDaycare.m_xEgg.m_uCurrentHp, xEggSnapshot.m_uCurrentHp,
		"progress mutation leaves all ordinary monster vitals unchanged");
}

ZENITH_TEST(ZM_Save, Tower_GameStateRetainsCurrentBestAndSeed)
{
	ZM_GameState xState;
	xState.m_xTowerRun.m_uCurrentStreak = 9u;
	xState.m_xTowerRun.m_uBestStreak = 27u;
	xState.m_xTowerRun.m_ulSeed = 0x123456789abcdef0ull;

	const ZM_GameState xCopy = xState;
	ZENITH_ASSERT_EQ(xCopy.m_xTowerRun.m_uCurrentStreak, 9u, "current tower streak is durable state");
	ZENITH_ASSERT_EQ(xCopy.m_xTowerRun.m_uBestStreak, 27u, "best tower streak is durable state");
	ZENITH_ASSERT_TRUE(xCopy.m_xTowerRun.m_ulSeed == 0x123456789abcdef0ull,
		"the full 64-bit procedural run seed is durable state");
	ZENITH_ASSERT_NE(xCopy.m_xTowerRun.m_uCurrentStreak, xCopy.m_xTowerRun.m_uBestStreak,
		"current and best streak are independent fields");
}

ZENITH_TEST(ZM_Save, WorldPosition_DefaultIsUnset)
{
	const ZM_GameState xState;
	ZENITH_ASSERT_EQ(xState.m_xWorldPosition.m_uSceneBuildIndex, uZM_WORLD_SCENE_UNSET,
		"a new state has no resumable scene build index");
	ZENITH_ASSERT_EQ((u_int)sizeof(xState.m_xWorldPosition.m_szSpawnTag), 32u,
		"the durable spawn-tag buffer has the locked width");
	for (u_int i = 0u; i < (u_int)sizeof(xState.m_xWorldPosition.m_szSpawnTag); ++i)
	{
		ZENITH_ASSERT_EQ((u_int)(unsigned char)xState.m_xWorldPosition.m_szSpawnTag[i], 0u,
			"unset spawn-tag byte %u is zero", i);
	}
	ZENITH_ASSERT_TRUE(xState.m_xWorldPosition.m_afPosition[0] == 0.0f &&
		xState.m_xWorldPosition.m_afPosition[1] == 0.0f &&
		xState.m_xWorldPosition.m_afPosition[2] == 0.0f,
		"unset world position has deterministic zero coordinates");
	ZENITH_ASSERT_TRUE(xState.m_xWorldPosition.m_fYaw == 0.0f,
		"unset world position has deterministic zero yaw");
}

ZENITH_TEST(ZM_Save, Options_DefaultTextSpeedIsNormal)
{
	const ZM_GameState xState;
	ZENITH_ASSERT_EQ((u_int)xState.m_xOptions.m_eTextSpeed, (u_int)ZM_TEXT_SPEED_NORMAL,
		"new saves use normal text speed");
	ZENITH_ASSERT_EQ((u_int)ZM_TEXT_SPEED_COUNT, 3u, "slow, normal, and fast are the complete persisted domain");
	ZENITH_ASSERT_NE((u_int)ZM_TEXT_SPEED_NORMAL, (u_int)ZM_TEXT_SPEED_SLOW,
		"normal is not silently aliased to slow");
	ZENITH_ASSERT_NE((u_int)ZM_TEXT_SPEED_NORMAL, (u_int)ZM_TEXT_SPEED_FAST,
		"normal is not silently aliased to fast");
}

ZENITH_TEST(ZM_Save, StarterState_SeedsSeenCaughtAndEmptyNewModules)
{
	const ZM_GameState xState = ZM_MakeStarterGameState();
	ZENITH_ASSERT_EQ(xState.m_xParty.Count(), 1u, "starter retains its single party lead");
	ZENITH_ASSERT_TRUE(xState.IsSeen(ZM_SPECIES_FERNFAWN), "the starter species is seen");
	ZENITH_ASSERT_TRUE(xState.IsCaught(ZM_SPECIES_FERNFAWN), "the starter species is caught");
	ZENITH_ASSERT_EQ(xState.GetSeenCount(), 1u, "only the starter is seen initially");
	ZENITH_ASSERT_EQ(xState.GetCaughtCount(), 1u, "only the starter is caught initially");
	ZENITH_ASSERT_EQ(xState.m_xBoxes.Count(), 0u, "starter boxes are empty");
	ZENITH_ASSERT_EQ(xState.m_xStoryFlags.Count(), 0u, "starter story flags are clear");
	ZENITH_ASSERT_EQ(xState.GetBadgeCount(), 0u, "starter has no badges");
	ZENITH_ASSERT_EQ(xState.m_xDaycare.m_uParentCount, 0u, "starter daycare has no parents");
	ZENITH_ASSERT_FALSE(xState.m_xDaycare.m_bEggPresent, "starter daycare has no pending egg");
	ZENITH_ASSERT_EQ(xState.m_xDaycare.m_uEggStepsRemaining, 0u, "starter daycare has no egg progress");
	ZENITH_ASSERT_EQ(xState.m_xTowerRun.m_uCurrentStreak, 0u, "starter tower current streak is zero");
	ZENITH_ASSERT_EQ(xState.m_xTowerRun.m_uBestStreak, 0u, "starter tower best streak is zero");
	ZENITH_ASSERT_TRUE(xState.m_xTowerRun.m_ulSeed == 0ull, "starter tower seed is zero");
	ZENITH_ASSERT_EQ(xState.m_xWorldPosition.m_uSceneBuildIndex, uZM_WORLD_SCENE_UNSET,
		"starter world resume point is unset");
	ZENITH_ASSERT_EQ((u_int)xState.m_xOptions.m_eTextSpeed, (u_int)ZM_TEXT_SPEED_NORMAL,
		"starter options use normal text speed");
	ZENITH_ASSERT_FALSE(xState.m_bPendingWhiteout, "starter has no transient pending whiteout");
}
