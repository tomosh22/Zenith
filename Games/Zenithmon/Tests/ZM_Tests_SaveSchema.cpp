#include "Zenith.h"

// ============================================================================
// ZM_Tests_SaveSchema -- S7 item 1 SC2 contract for the complete v1 game
// payload codec. These tests were authored and observed RED before the
// production header/API landed. They are pure and headless: no SaveData slots,
// disk, ECS, scenes, renderer, or automation.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Core/Zenith_ErrorCode.h"
#include "DataStream/Zenith_DataStream.h"
#include "Zenithmon/Source/Core/ZM_SaveSchema.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace
{
	static constexpr uint32_t uTEST_INNER_HEADER_BYTES = 12u;
	static constexpr uint32_t uTEST_MODULE_HEADER_BYTES = 12u;
	static constexpr uint32_t uTEST_MODULE_COUNT = 11u;
	static constexpr uint32_t uTEST_MONSTER_BYTES = 61u;

	struct ZM_TestModuleSpan
	{
		u_int m_uHeader = 0u;
		u_int m_uPayload = 0u;
		u_int m_uLength = 0u;
		bool m_bFound = false;
	};

	uint16_t ReadU16(const std::vector<uint8_t>& xBytes, u_int uOffset)
	{
		if ((uint64_t)uOffset + 2u > xBytes.size())
		{
			ZENITH_ASSERT_TRUE(false, "ReadU16 offset %u exceeds %zu-byte fixture",
				uOffset, xBytes.size());
			return 0u;
		}
		return (uint16_t)((uint16_t)xBytes[uOffset]
			| ((uint16_t)xBytes[uOffset + 1u] << 8u));
	}

	uint32_t ReadU32(const std::vector<uint8_t>& xBytes, u_int uOffset)
	{
		if ((uint64_t)uOffset + 4u > xBytes.size())
		{
			ZENITH_ASSERT_TRUE(false, "ReadU32 offset %u exceeds %zu-byte fixture",
				uOffset, xBytes.size());
			return 0u;
		}
		return (uint32_t)xBytes[uOffset]
			| ((uint32_t)xBytes[uOffset + 1u] << 8u)
			| ((uint32_t)xBytes[uOffset + 2u] << 16u)
			| ((uint32_t)xBytes[uOffset + 3u] << 24u);
	}

	void AppendU16(std::vector<uint8_t>& xBytes, uint16_t uValue)
	{
		xBytes.push_back((uint8_t)(uValue & 0xffu));
		xBytes.push_back((uint8_t)((uValue >> 8u) & 0xffu));
	}

	void AppendU32(std::vector<uint8_t>& xBytes, uint32_t uValue)
	{
		xBytes.push_back((uint8_t)(uValue & 0xffu));
		xBytes.push_back((uint8_t)((uValue >> 8u) & 0xffu));
		xBytes.push_back((uint8_t)((uValue >> 16u) & 0xffu));
		xBytes.push_back((uint8_t)((uValue >> 24u) & 0xffu));
	}

	void WriteU16(std::vector<uint8_t>& xBytes, u_int uOffset, uint16_t uValue)
	{
		if ((uint64_t)uOffset + 2u > xBytes.size())
		{
			ZENITH_ASSERT_TRUE(false, "WriteU16 offset %u exceeds %zu-byte fixture",
				uOffset, xBytes.size());
			return;
		}
		xBytes[uOffset] = (uint8_t)(uValue & 0xffu);
		xBytes[uOffset + 1u] = (uint8_t)((uValue >> 8u) & 0xffu);
	}

	void WriteU32(std::vector<uint8_t>& xBytes, u_int uOffset, uint32_t uValue)
	{
		if ((uint64_t)uOffset + 4u > xBytes.size())
		{
			ZENITH_ASSERT_TRUE(false, "WriteU32 offset %u exceeds %zu-byte fixture",
				uOffset, xBytes.size());
			return;
		}
		xBytes[uOffset] = (uint8_t)(uValue & 0xffu);
		xBytes[uOffset + 1u] = (uint8_t)((uValue >> 8u) & 0xffu);
		xBytes[uOffset + 2u] = (uint8_t)((uValue >> 16u) & 0xffu);
		xBytes[uOffset + 3u] = (uint8_t)((uValue >> 24u) & 0xffu);
	}

	std::vector<uint8_t> Snapshot(const Zenith_DataStream& xStream)
	{
		std::vector<uint8_t> xBytes;
		const uint64_t ulLength = xStream.GetCursor();
		xBytes.resize((size_t)ulLength);
		if (ulLength > 0u)
		{
			memcpy(xBytes.data(), xStream.GetData(), (size_t)ulLength);
		}
		return xBytes;
	}

	ZM_TestModuleSpan FindModule(const std::vector<uint8_t>& xBytes, uint32_t uModuleId)
	{
		u_int uCursor = uTEST_INNER_HEADER_BYTES;
		for (u_int uModule = 0u; uModule < uTEST_MODULE_COUNT; ++uModule)
		{
			if ((uint64_t)uCursor + uTEST_MODULE_HEADER_BYTES > xBytes.size())
			{
				ZENITH_ASSERT_TRUE(false, "test fixture ended before module header %u", uModule);
				return {};
			}
			const uint32_t uFoundId = ReadU32(xBytes, uCursor);
			const uint32_t uLength = ReadU32(xBytes, uCursor + 8u);
			const uint64_t ulNext = (uint64_t)uCursor + uTEST_MODULE_HEADER_BYTES + uLength;
			if (ulNext > xBytes.size() || ulNext > UINT32_MAX)
			{
				ZENITH_ASSERT_TRUE(false, "test fixture module %u exceeds its blob", uFoundId);
				return {};
			}
			if (uFoundId == uModuleId)
			{
				return { uCursor, uCursor + uTEST_MODULE_HEADER_BYTES, uLength, true };
			}
			uCursor = (u_int)ulNext;
		}
		ZENITH_ASSERT_TRUE(false, "test fixture did not contain module %u", uModuleId);
		return {};
	}

	void ReplaceModulePayload(std::vector<uint8_t>& xBytes, uint32_t uModuleId,
		const std::vector<uint8_t>& xPayload)
	{
		const ZM_TestModuleSpan xSpan = FindModule(xBytes, uModuleId);
		if (!xSpan.m_bFound) { return; }
		xBytes.erase(xBytes.begin() + xSpan.m_uPayload,
			xBytes.begin() + xSpan.m_uPayload + xSpan.m_uLength);
		xBytes.insert(xBytes.begin() + xSpan.m_uPayload, xPayload.begin(), xPayload.end());
		WriteU32(xBytes, xSpan.m_uHeader + 8u, (uint32_t)xPayload.size());
	}

	void SetNickname(ZM_Monster& xMonster, const char* szNickname)
	{
		memset(xMonster.m_szNickname, 0, sizeof(xMonster.m_szNickname));
		const size_t uLength = (std::min)(strlen(szNickname),
			(size_t)uZM_MONSTER_NICKNAME_CAPACITY - 1u);
		memcpy(xMonster.m_szNickname, szNickname, uLength);
	}

	ZM_Monster MakeMonster(ZM_SPECIES_ID eSpecies, u_int uLevel, const char* szNickname)
	{
		ZM_Monster xMonster = ZM_BuildMonsterRecord(eSpecies, uLevel);
		xMonster.m_uFriendship = 40u + uLevel;
		SetNickname(xMonster, szNickname);
		return xMonster;
	}

	ZM_GameState MakeWireFixture()
	{
		ZM_GameState xState;
		ZM_Monster xMonster = MakeMonster(ZM_SPECIES_FERNFAWN, 5u, "Leaf");
		xMonster.m_uCurrentHp = 1u;
		xState.m_xParty.Add(xMonster);
		xState.MarkCaught(ZM_SPECIES_FERNFAWN);
		xState.m_xStoryFlags.Set(9u, true);
		xState.AwardBadge(0u);
		xState.m_xBag.Add(ZM_ITEM_CATCHORB, 2u);
		xState.m_uMoney = 0x12345678u;
		xState.m_xTowerRun.m_uCurrentStreak = 1u;
		xState.m_xTowerRun.m_uBestStreak = 2u;
		xState.m_xTowerRun.m_ulSeed = 0x0102030405060708ull;
		xState.m_xOptions.m_eTextSpeed = ZM_TEXT_SPEED_FAST;
		return xState;
	}

	ZM_GameState MakeMaximalState()
	{
		ZM_GameState xState;
		const ZM_SPECIES_ID aeSpecies[uZM_MAX_PARTY_SIZE] =
		{
			ZM_SPECIES_FERNFAWN,
			ZM_SPECIES_KINDLET,
			ZM_SPECIES_NIBBIN,
			(ZM_SPECIES_ID)3u,
			(ZM_SPECIES_ID)4u,
			(ZM_SPECIES_ID)5u,
		};
		for (u_int u = 0u; u < uZM_MAX_PARTY_SIZE; ++u)
		{
			char szName[8];
			snprintf(szName, sizeof(szName), "P%u", u);
			ZM_Monster xMonster = MakeMonster(aeSpecies[u], 10u + u, szName);
			xMonster.m_uCurrentHp = (u == 0u) ? 0u : (std::min)(xMonster.m_uCurrentHp, u + 1u);
			xMonster.m_uFriendship = 250u - u;
			if (u == 1u)
			{
				xMonster.m_eStatus = ZM_MAJOR_STATUS_POISON;
				xMonster.m_uFlags |= uZM_MONSTER_FLAG_IS_SHINY;
			}
			if (u == 2u)
			{
				xMonster.m_eNature = ZM_NATURE_RECKLESS;
				xMonster.m_eAbility = ZM_GetSpeciesAbilities(xMonster.m_eSpecies).m_eHidden;
				xMonster.m_eGender = ZM_GENDER_GENDERLESS;
				SetNickname(xMonster, "");
			}
			if (xMonster.m_axMoves[0].m_eMove != ZM_MOVE_NONE
				&& xMonster.m_axMoves[0].m_uMaxPP > 0u)
			{
				xMonster.m_axMoves[0].m_uCurPP = xMonster.m_axMoves[0].m_uMaxPP - 1u;
			}
			xState.m_xParty.Add(xMonster);
		}

		xState.m_xBoxes.StoreAt(0u, 0u, MakeMonster((ZM_SPECIES_ID)6u, 20u, "BoxA"));
		xState.m_xBoxes.StoreAt(3u, 7u, MakeMonster((ZM_SPECIES_ID)7u, 21u, "BoxB"));
		xState.m_xBoxes.StoreAt(15u, 29u, MakeMonster((ZM_SPECIES_ID)8u, 22u, "BoxZ"));

		xState.MarkSeen((ZM_SPECIES_ID)9u);
		xState.MarkCaught(ZM_SPECIES_FERNFAWN);
		xState.MarkCaught((ZM_SPECIES_ID)10u);
		xState.m_xStoryFlags.Set(0u, true);
		xState.m_xStoryFlags.Set(7u, true);
		xState.m_xStoryFlags.Set(8u, true);
		xState.m_xStoryFlags.Set(uZM_MAX_STORY_FLAGS - 1u, true);
		xState.AwardBadge(0u);
		xState.AwardBadge(7u);

		xState.m_xBag.Add(ZM_ITEM_CATCHORB, uZM_BAG_MAX_STACK_COUNT);
		xState.m_xBag.Add(ZM_ITEM_SALVE, 17u);
		xState.m_xBag.Add(ZM_ITEM_TM_TITANBEAM, 1u);
		xState.m_uMoney = 0xffffffffu;

		xState.m_xDaycare.m_uParentCount = 2u;
		xState.m_xDaycare.m_axParents[0] = MakeMonster((ZM_SPECIES_ID)11u, 30u, "Dam");
		xState.m_xDaycare.m_axParents[1] = MakeMonster((ZM_SPECIES_ID)12u, 31u, "Sire");
		xState.m_xDaycare.m_bEggPresent = true;
		xState.m_xDaycare.m_xEgg = MakeMonster((ZM_SPECIES_ID)13u, 1u, "Egg");
		xState.m_xDaycare.m_xEgg.m_uFlags |= uZM_MONSTER_FLAG_IS_EGG;
		xState.m_xDaycare.m_uEggStepsRemaining = 321u;

		xState.m_xTowerRun.m_uCurrentStreak = 9u;
		xState.m_xTowerRun.m_uBestStreak = 27u;
		xState.m_xTowerRun.m_ulSeed = 0x123456789abcdef0ull;
		xState.m_xWorldPosition.m_uSceneBuildIndex = 2u;
		memcpy(xState.m_xWorldPosition.m_szSpawnTag, "TownCenter", 11u);
		xState.m_xWorldPosition.m_afPosition[0] = 512.25f;
		xState.m_xWorldPosition.m_afPosition[1] = 26.5f;
		xState.m_xWorldPosition.m_afPosition[2] = 480.75f;
		xState.m_xWorldPosition.m_fYaw = -1.25f;
		xState.m_xOptions.m_eTextSpeed = ZM_TEXT_SPEED_FAST;
		xState.m_bPendingWhiteout = true;
		return xState;
	}

	void AssertMonsterEqual(const ZM_Monster& xExpected, const ZM_Monster& xActual,
		const char* szContext)
	{
		ZENITH_ASSERT_EQ((u_int)xActual.m_eSpecies, (u_int)xExpected.m_eSpecies,
			"%s species", szContext);
		ZENITH_ASSERT_EQ(xActual.m_uLevel, xExpected.m_uLevel, "%s level", szContext);
		ZENITH_ASSERT_EQ(xActual.m_uCurrentExp, xExpected.m_uCurrentExp, "%s exp", szContext);
		for (u_int u = 0u; u < ZM_STAT_COUNT; ++u)
		{
			ZENITH_ASSERT_EQ(xActual.m_auIV[u], xExpected.m_auIV[u], "%s IV %u", szContext, u);
			ZENITH_ASSERT_EQ(xActual.m_auEV[u], xExpected.m_auEV[u], "%s EV %u", szContext, u);
		}
		ZENITH_ASSERT_EQ((u_int)xActual.m_eNature, (u_int)xExpected.m_eNature,
			"%s nature", szContext);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eAbility, (u_int)xExpected.m_eAbility,
			"%s ability", szContext);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eStatus, (u_int)xExpected.m_eStatus,
			"%s status", szContext);
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
		{
			ZENITH_ASSERT_EQ((u_int)xActual.m_axMoves[u].m_eMove,
				(u_int)xExpected.m_axMoves[u].m_eMove, "%s move %u", szContext, u);
			ZENITH_ASSERT_EQ(xActual.m_axMoves[u].m_uCurPP,
				xExpected.m_axMoves[u].m_uCurPP, "%s current PP %u", szContext, u);
			ZENITH_ASSERT_EQ(xActual.m_axMoves[u].m_uMaxPP,
				xExpected.m_axMoves[u].m_uMaxPP, "%s max PP %u", szContext, u);
		}
		ZENITH_ASSERT_EQ(xActual.m_uCurrentHp, xExpected.m_uCurrentHp, "%s HP", szContext);
		ZENITH_ASSERT_EQ((u_int)xActual.m_eGender, (u_int)xExpected.m_eGender,
			"%s gender", szContext);
		ZENITH_ASSERT_EQ(xActual.m_uFriendship, xExpected.m_uFriendship,
			"%s friendship", szContext);
		ZENITH_ASSERT_EQ(xActual.m_uFlags, xExpected.m_uFlags, "%s flags", szContext);
		ZENITH_ASSERT_TRUE(memcmp(xActual.m_szNickname, xExpected.m_szNickname,
			uZM_MONSTER_NICKNAME_CAPACITY) == 0, "%s nickname", szContext);
	}

	void AssertStateEqual(const ZM_GameState& xExpected, const ZM_GameState& xActual,
		const char* szContext)
	{
		ZENITH_ASSERT_EQ(xActual.m_xParty.Count(), xExpected.m_xParty.Count(),
			"%s party count", szContext);
		const u_int uComparablePartyCount = (std::min)(xExpected.m_xParty.Count(),
			xActual.m_xParty.Count());
		for (u_int u = 0u; u < uComparablePartyCount; ++u)
		{
			AssertMonsterEqual(xExpected.m_xParty.Get(u), xActual.m_xParty.Get(u), szContext);
		}
		ZENITH_ASSERT_EQ(xActual.m_xBoxes.Count(), xExpected.m_xBoxes.Count(),
			"%s box count", szContext);
		for (u_int uBox = 0u; uBox < uZM_BOX_COUNT; ++uBox)
		{
			for (u_int uSlot = 0u; uSlot < uZM_BOX_SLOTS_PER_BOX; ++uSlot)
			{
				const ZM_Monster* pxExpected = xExpected.m_xBoxes.TryGet(uBox, uSlot);
				const ZM_Monster* pxActual = xActual.m_xBoxes.TryGet(uBox, uSlot);
				ZENITH_ASSERT_EQ(pxActual != nullptr, pxExpected != nullptr,
					"%s box %u slot %u occupancy", szContext, uBox, uSlot);
				if (pxExpected != nullptr && pxActual != nullptr)
				{
					AssertMonsterEqual(*pxExpected, *pxActual, szContext);
				}
			}
		}
		for (u_int u = 0u; u < (u_int)ZM_SPECIES_COUNT; ++u)
		{
			ZENITH_ASSERT_EQ(xActual.m_xSeen.m_abFlags[u], xExpected.m_xSeen.m_abFlags[u],
				"%s seen %u", szContext, u);
			ZENITH_ASSERT_EQ(xActual.m_xCaught.m_abFlags[u], xExpected.m_xCaught.m_abFlags[u],
				"%s caught %u", szContext, u);
		}
		ZENITH_ASSERT_TRUE(memcmp(xActual.m_xStoryFlags.m_auFlags,
			xExpected.m_xStoryFlags.m_auFlags, uZM_STORY_FLAG_BYTE_COUNT) == 0,
			"%s story flags", szContext);
		ZENITH_ASSERT_EQ((u_int)xActual.m_uBadgeMask, (u_int)xExpected.m_uBadgeMask,
			"%s badges", szContext);
		for (u_int u = 0u; u < (u_int)ZM_ITEM_COUNT; ++u)
		{
			ZENITH_ASSERT_EQ(xActual.m_xBag.GetCount((ZM_ITEM_ID)u),
				xExpected.m_xBag.GetCount((ZM_ITEM_ID)u), "%s bag item %u", szContext, u);
		}
		ZENITH_ASSERT_EQ(xActual.m_uMoney, xExpected.m_uMoney, "%s money", szContext);
		ZENITH_ASSERT_EQ(xActual.m_xDaycare.m_uParentCount,
			xExpected.m_xDaycare.m_uParentCount, "%s daycare parents", szContext);
		for (u_int u = 0u; u < xExpected.m_xDaycare.m_uParentCount; ++u)
		{
			AssertMonsterEqual(xExpected.m_xDaycare.m_axParents[u],
				xActual.m_xDaycare.m_axParents[u], szContext);
		}
		ZENITH_ASSERT_EQ(xActual.m_xDaycare.m_bEggPresent,
			xExpected.m_xDaycare.m_bEggPresent, "%s daycare egg presence", szContext);
		if (xExpected.m_xDaycare.m_bEggPresent && xActual.m_xDaycare.m_bEggPresent)
		{
			AssertMonsterEqual(xExpected.m_xDaycare.m_xEgg,
				xActual.m_xDaycare.m_xEgg, szContext);
		}
		ZENITH_ASSERT_EQ(xActual.m_xDaycare.m_uEggStepsRemaining,
			xExpected.m_xDaycare.m_uEggStepsRemaining, "%s daycare steps", szContext);
		ZENITH_ASSERT_EQ(xActual.m_xTowerRun.m_uCurrentStreak,
			xExpected.m_xTowerRun.m_uCurrentStreak, "%s tower current", szContext);
		ZENITH_ASSERT_EQ(xActual.m_xTowerRun.m_uBestStreak,
			xExpected.m_xTowerRun.m_uBestStreak, "%s tower best", szContext);
		ZENITH_ASSERT_TRUE(xActual.m_xTowerRun.m_ulSeed == xExpected.m_xTowerRun.m_ulSeed,
			"%s tower seed", szContext);
		ZENITH_ASSERT_EQ(xActual.m_xWorldPosition.m_uSceneBuildIndex,
			xExpected.m_xWorldPosition.m_uSceneBuildIndex, "%s scene", szContext);
		ZENITH_ASSERT_TRUE(memcmp(xActual.m_xWorldPosition.m_szSpawnTag,
			xExpected.m_xWorldPosition.m_szSpawnTag, uZM_WORLD_SPAWN_TAG_CAPACITY) == 0,
			"%s spawn tag", szContext);
		for (u_int u = 0u; u < 3u; ++u)
		{
			ZENITH_ASSERT_TRUE(xActual.m_xWorldPosition.m_afPosition[u]
				== xExpected.m_xWorldPosition.m_afPosition[u], "%s position %u", szContext, u);
		}
		ZENITH_ASSERT_TRUE(xActual.m_xWorldPosition.m_fYaw == xExpected.m_xWorldPosition.m_fYaw,
			"%s yaw", szContext);
		ZENITH_ASSERT_EQ((u_int)xActual.m_xOptions.m_eTextSpeed,
			(u_int)xExpected.m_xOptions.m_eTextSpeed, "%s text speed", szContext);
		ZENITH_ASSERT_EQ(xActual.m_bPendingWhiteout, xExpected.m_bPendingWhiteout,
			"%s transient pending-whiteout state", szContext);
	}

	std::vector<uint8_t> Encode(const ZM_GameState& xState)
	{
		Zenith_DataStream xStream;
		const Zenith_Status xStatus = ZM_SaveSchema::Write(xState, xStream);
		ZENITH_ASSERT_TRUE(xStatus.IsOk(), "valid test fixture failed to encode (error %u)",
			(u_int)xStatus.Error());
		if (!xStatus.IsOk()) { return {}; }
		return Snapshot(xStream);
	}

	Zenith_Status Decode(const std::vector<uint8_t>& xBytes, ZM_GameState& xOutState,
		uint64_t ulLength = UINT64_MAX)
	{
		const uint64_t ulExactLength = ulLength == UINT64_MAX ? xBytes.size() : ulLength;
		Zenith_DataStream xStream((void*)xBytes.data(), xBytes.size());
		return ZM_SaveSchema::Read(xStream, ulExactLength, xOutState);
	}

	void ExpectReadError(const std::vector<uint8_t>& xBytes, Zenith_ErrorCode eExpected,
		const char* szCase)
	{
		ZM_GameState xDestination = MakeMaximalState();
		const ZM_GameState xBefore = xDestination;
		Zenith_DataStream xStream((void*)xBytes.data(), xBytes.size());
		const uint64_t ulBeforeCursor = xStream.GetCursor();
		const Zenith_Status xStatus = ZM_SaveSchema::Read(xStream, xBytes.size(), xDestination);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "%s unexpectedly decoded", szCase);
		ZENITH_ASSERT_EQ((u_int)xStatus.Error(), (u_int)eExpected,
			"%s returned the wrong error", szCase);
		ZENITH_ASSERT_TRUE(xStream.GetCursor() == ulBeforeCursor,
			"%s advanced the input cursor on failure", szCase);
		AssertStateEqual(xBefore, xDestination, szCase);
	}

	std::vector<uint8_t> CopyMonsterRecord(const std::vector<uint8_t>& xBytes)
	{
		const ZM_TestModuleSpan xParty = FindModule(xBytes, 1u);
		if (!xParty.m_bFound || xParty.m_uLength < 1u + uTEST_MONSTER_BYTES
			|| (uint64_t)xParty.m_uPayload + 1u + uTEST_MONSTER_BYTES > xBytes.size())
		{
			ZENITH_ASSERT_TRUE(false, "fixture party has no complete monster record");
			return {};
		}
		return std::vector<uint8_t>(xBytes.begin() + xParty.m_uPayload + 1u,
			xBytes.begin() + xParty.m_uPayload + 1u + uTEST_MONSTER_BYTES);
	}

	std::vector<uint8_t> MakeDexPayload(uint16_t uCount, uint8_t uSeenFirst,
		uint8_t uCaughtFirst)
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, uCount);
		const u_int uBytes = ((u_int)uCount + 7u) / 8u;
		xPayload.resize(2u + uBytes * 2u, 0u);
		if (uBytes > 0u)
		{
			xPayload[2u] = uSeenFirst;
			xPayload[2u + uBytes] = uCaughtFirst;
		}
		return xPayload;
	}
}

ZENITH_TEST(ZM_Save, SchemaV1_MaximalRoundTripPreservesAllDurableFields)
{
	const ZM_GameState xSource = MakeMaximalState();
	const ZM_Monster& xSourceVariant = xSource.m_xParty.Get(2u);
	ZENITH_ASSERT_TRUE(xSourceVariant.m_eNature != ZM_NATURE_FERAL,
		"maximal fixture must exercise a non-default nature");
	ZENITH_ASSERT_EQ((u_int)xSourceVariant.m_eAbility,
		(u_int)ZM_GetSpeciesAbilities(xSourceVariant.m_eSpecies).m_eHidden,
		"maximal fixture must exercise the hidden ability slot");
	ZENITH_ASSERT_EQ((u_int)xSourceVariant.m_eGender, (u_int)ZM_GENDER_GENDERLESS,
		"maximal fixture must exercise genderless");
	ZENITH_ASSERT_EQ((u_int)(uint8_t)xSourceVariant.m_szNickname[0], 0u,
		"maximal fixture must exercise an empty nickname");
	const std::vector<uint8_t> xBytes = Encode(xSource);
	ZM_GameState xDecoded;
	const Zenith_Status xStatus = Decode(xBytes, xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "maximal v1 state did not decode");
	if (!xStatus.IsOk()) { return; }
	ZM_GameState xExpected = xSource;
	xExpected.m_bPendingWhiteout = false;
	AssertStateEqual(xExpected, xDecoded, "maximal round trip");
	const ZM_Monster& xDecodedVariant = xDecoded.m_xParty.Get(2u);
	ZENITH_ASSERT_EQ((u_int)xDecodedVariant.m_eNature, (u_int)xSourceVariant.m_eNature,
		"non-default nature was not field-exact");
	ZENITH_ASSERT_EQ((u_int)xDecodedVariant.m_eAbility, (u_int)xSourceVariant.m_eAbility,
		"hidden ability was not field-exact");
	ZENITH_ASSERT_EQ((u_int)xDecodedVariant.m_eGender, (u_int)xSourceVariant.m_eGender,
		"genderless value was not field-exact");
	ZENITH_ASSERT_TRUE(memcmp(xDecodedVariant.m_szNickname, xSourceVariant.m_szNickname,
		uZM_MONSTER_NICKNAME_CAPACITY) == 0, "empty nickname was not field-exact");
}

ZENITH_TEST(ZM_Save, SchemaV1_EmptyEdgesRoundTrip)
{
	const ZM_GameState xSource;
	const std::vector<uint8_t> xBytes = Encode(xSource);
	ZM_GameState xDecoded = MakeMaximalState();
	const Zenith_Status xStatus = Decode(xBytes, xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "empty v1 state did not decode");
	AssertStateEqual(xSource, xDecoded, "empty party/boxes/bag/flags/daycare");
}

ZENITH_TEST(ZM_Save, SchemaV1_EggOnlyDaycareRoundTrip)
{
	ZM_GameState xSource;
	xSource.m_xDaycare.m_bEggPresent = true;
	xSource.m_xDaycare.m_xEgg = MakeMonster(ZM_SPECIES_NIBBIN, 1u, "SoloEgg");
	xSource.m_xDaycare.m_xEgg.m_uFlags |= uZM_MONSTER_FLAG_IS_EGG;
	xSource.m_xDaycare.m_uEggStepsRemaining = 77u;
	const std::vector<uint8_t> xBytes = Encode(xSource);
	ZM_GameState xDecoded;
	const Zenith_Status xStatus = Decode(xBytes, xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "egg-only daycare did not decode");
	AssertStateEqual(xSource, xDecoded, "egg-only daycare");
}

ZENITH_TEST(ZM_Save, Write_AppendsAtCurrentCursorAndReturnsSuccess)
{
	const uint8_t auPrefix[] = { 0x91u, 0x82u, 0x73u, 0x64u, 0x55u };
	Zenith_DataStream xStream;
	xStream.WriteData(auPrefix, sizeof(auPrefix));
	const uint64_t ulEntryCursor = xStream.GetCursor();
	const Zenith_Status xStatus = ZM_SaveSchema::Write(MakeWireFixture(), xStream);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "valid append returned error %u", (u_int)xStatus.Error());
	ZENITH_ASSERT_TRUE(xStream.GetCursor() > ulEntryCursor, "writer appended zero bytes");
	ZENITH_ASSERT_TRUE(memcmp(xStream.GetData(), auPrefix, sizeof(auPrefix)) == 0,
		"writer replaced or changed the existing prefix");
	const uint8_t* pBytes = static_cast<const uint8_t*>(xStream.GetData());
	ZENITH_ASSERT_EQ((u_int)pBytes[ulEntryCursor + 0u], 0x5au, "ZMSV byte Z");
	ZENITH_ASSERT_EQ((u_int)pBytes[ulEntryCursor + 1u], 0x4du, "ZMSV byte M");
	ZENITH_ASSERT_EQ((u_int)pBytes[ulEntryCursor + 2u], 0x53u, "ZMSV byte S");
	ZENITH_ASSERT_EQ((u_int)pBytes[ulEntryCursor + 3u], 0x56u, "ZMSV byte V");
}

ZENITH_TEST(ZM_Save, Write_InvalidStateIsAtomic)
{
	const uint8_t auPrefix[] = { 0xdeu, 0xadu, 0xbeu, 0xefu };
	struct InvalidCase
	{
		const char* m_szName;
		void (*m_pfnMutate)(ZM_GameState&);
	};
	const InvalidCase axCases[] =
	{
		{ "party count", +[](ZM_GameState& xState)
			{ xState.m_xParty.m_uCount = uZM_MAX_PARTY_SIZE + 1u; } },
		{ "monster level", +[](ZM_GameState& xState)
			{ xState.m_xParty.Get(0u).m_uLevel = 0u; } },
		{ "caught without seen", +[](ZM_GameState& xState)
			{ xState.m_xSeen.m_abFlags[(u_int)ZM_SPECIES_FERNFAWN] = false; } },
		{ "bag pocket count 65", +[](ZM_GameState& xState)
			{ xState.m_xBag.m_auPocketCount[0] = uZM_BAG_MAX_STACKS_PER_POCKET + 1u; } },
		{ "bag pocket count UINT_MAX", +[](ZM_GameState& xState)
			{ xState.m_xBag.m_auPocketCount[0] = UINT_MAX; } },
		{ "daycare progress without egg", +[](ZM_GameState& xState)
			{ xState.m_xDaycare.m_uEggStepsRemaining = 1u; } },
		{ "tower current above best", +[](ZM_GameState& xState)
			{ xState.m_xTowerRun.m_uCurrentStreak = 2u; xState.m_xTowerRun.m_uBestStreak = 1u; } },
		{ "unknown world scene", +[](ZM_GameState& xState)
			{ xState.m_xWorldPosition.m_uSceneBuildIndex = 9999u; } },
		{ "options sentinel", +[](ZM_GameState& xState)
			{ xState.m_xOptions.m_eTextSpeed = ZM_TEXT_SPEED_COUNT; } },
	};
	for (const InvalidCase& xCase : axCases)
	{
		Zenith_DataStream xStream;
		xStream.WriteData(auPrefix, sizeof(auPrefix));
		const std::vector<uint8_t> xBefore = Snapshot(xStream);
		const uint64_t ulBeforeCursor = xStream.GetCursor();
		ZM_GameState xInvalid = MakeWireFixture();
		xCase.m_pfnMutate(xInvalid);
		const Zenith_Status xStatus = ZM_SaveSchema::Write(xInvalid, xStream);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "%s encoded", xCase.m_szName);
		ZENITH_ASSERT_EQ((u_int)xStatus.Error(), (u_int)Zenith_ErrorCode::INVALID_ARGUMENT,
			"%s returned the wrong status", xCase.m_szName);
		ZENITH_ASSERT_TRUE(xStream.GetCursor() == ulBeforeCursor,
			"%s changed the output cursor", xCase.m_szName);
		ZENITH_ASSERT_TRUE(Snapshot(xStream) == xBefore,
			"%s changed the output bytes", xCase.m_szName);
	}
	{
		Zenith_DataStream xNullStream((void*)nullptr, 64u);
		const Zenith_Status xStatus = ZM_SaveSchema::Write(MakeWireFixture(), xNullStream);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "null wrapped output stream accepted a write");
		ZENITH_ASSERT_EQ((u_int)xStatus.Error(), (u_int)Zenith_ErrorCode::INVALID_ARGUMENT,
			"null wrapped output stream returned the wrong status");
		ZENITH_ASSERT_EQ(xNullStream.GetCursor(), 0ull,
			"null wrapped output stream advanced its cursor");
	}
	{
		uint8_t auExternal[64];
		memset(auExternal, 0x6d, sizeof(auExternal));
		uint8_t auBefore[sizeof(auExternal)];
		memcpy(auBefore, auExternal, sizeof(auExternal));
		Zenith_DataStream xExternalStream(auExternal, sizeof(auExternal));
		xExternalStream.SetCursor(4u);
		const Zenith_Status xStatus = ZM_SaveSchema::Write(MakeWireFixture(), xExternalStream);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "undersized external output stream accepted a write");
		ZENITH_ASSERT_EQ((u_int)xStatus.Error(), (u_int)Zenith_ErrorCode::OUT_OF_MEMORY,
			"undersized external output stream returned the wrong status");
		ZENITH_ASSERT_EQ(xExternalStream.GetCursor(), 4ull,
			"undersized external output stream advanced its cursor");
		ZENITH_ASSERT_TRUE(memcmp(auExternal, auBefore, sizeof(auExternal)) == 0,
			"undersized external output stream changed its bytes");
	}
}

ZENITH_TEST(ZM_Save, Read_ConsumesExactlyRequestedSlice)
{
	const std::vector<uint8_t> xPayload = Encode(MakeWireFixture());
	std::vector<uint8_t> xFramed = { 0xa1u, 0xb2u, 0xc3u };
	xFramed.insert(xFramed.end(), xPayload.begin(), xPayload.end());
	xFramed.push_back(0xd4u);
	xFramed.push_back(0xe5u);
	Zenith_DataStream xStream(xFramed.data(), xFramed.size());
	xStream.SetCursor(3u);
	ZM_GameState xDecoded;
	const Zenith_Status xStatus = ZM_SaveSchema::Read(xStream, xPayload.size(), xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "bounded slice did not decode");
	ZENITH_ASSERT_TRUE(xStream.GetCursor() == 3u + xPayload.size(),
		"reader did not consume exactly the requested payload");
	ZENITH_ASSERT_EQ((u_int)xFramed[xStream.GetCursor()], 0xd4u,
		"reader consumed the first suffix byte");
}

ZENITH_TEST(ZM_Save, Read_FailureIsAtomicForCursorAndDestination)
{
	const std::vector<uint8_t> xValidPayload = Encode(MakeWireFixture());
	if (xValidPayload.empty()) { return; }
	{
		std::vector<uint8_t> xPayload = xValidPayload;
		xPayload.back() ^= 0xffu;
		std::vector<uint8_t> xFramed = { 0x11u, 0x22u };
		xFramed.insert(xFramed.end(), xPayload.begin(), xPayload.end());
		Zenith_DataStream xStream(xFramed.data(), xFramed.size());
		xStream.SetCursor(2u);
		ZM_GameState xDestination = MakeMaximalState();
		const ZM_GameState xBefore = xDestination;
		const Zenith_Status xStatus = ZM_SaveSchema::Read(xStream, xPayload.size(), xDestination);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "corrupt options value decoded");
		ZENITH_ASSERT_EQ((u_int)xStatus.Error(), (u_int)Zenith_ErrorCode::CORRUPT_DATA);
		ZENITH_ASSERT_TRUE(xStream.GetCursor() == 2u, "failed read changed entry cursor");
		AssertStateEqual(xBefore, xDestination, "failed read transaction");
	}
	{
		std::vector<uint8_t> xFramed = { 0x11u, 0x22u };
		xFramed.insert(xFramed.end(), xValidPayload.begin(), xValidPayload.end());
		Zenith_DataStream xStream(xFramed.data(), xFramed.size());
		xStream.SetCursor(2u);
		ZM_GameState xDestination = MakeMaximalState();
		const ZM_GameState xBefore = xDestination;
		const Zenith_Status xStatus = ZM_SaveSchema::Read(xStream,
			xValidPayload.size() + 1u, xDestination);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "exact length beyond remaining capacity decoded");
		ZENITH_ASSERT_EQ((u_int)xStatus.Error(), (u_int)Zenith_ErrorCode::CORRUPT_DATA);
		ZENITH_ASSERT_TRUE(xStream.GetCursor() == 2u,
			"out-of-capacity read changed entry cursor");
		AssertStateEqual(xBefore, xDestination, "out-of-capacity read transaction");
	}
}

ZENITH_TEST(ZM_Save, Header_BadMagicReturnsBadMagic)
{
	std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	const uint8_t auBadMagic[] = { 0x00u, 0x5au, 0xffu, 0x31u };
	for (u_int u = 0u; u < 4u; ++u)
	{
		std::vector<uint8_t> xMutant = xBytes;
		xMutant[u] = auBadMagic[u];
		ExpectReadError(xMutant, Zenith_ErrorCode::BAD_MAGIC, "bad inner magic");
	}
}

ZENITH_TEST(ZM_Save, Header_SchemaVersionMismatchReturnsVersionMismatch)
{
	const std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	for (uint32_t uVersion : { 0u, 2u, 0xffffffffu })
	{
		std::vector<uint8_t> xMutant = xBytes;
		WriteU32(xMutant, 4u, uVersion);
		ExpectReadError(xMutant, Zenith_ErrorCode::VERSION_MISMATCH,
			"unsupported global schema version");
	}
}

ZENITH_TEST(ZM_Save, Header_ModuleCountOrderAndIdsAreExact)
{
	const std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	for (uint32_t uCount : { 0u, 10u, 12u, 0xffffffffu })
	{
		std::vector<uint8_t> xMutant = xBytes;
		WriteU32(xMutant, 8u, uCount);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "wrong module count");
	}
	for (uint32_t uId = 1u; uId <= uTEST_MODULE_COUNT; ++uId)
	{
		std::vector<uint8_t> xMutant = xBytes;
		const ZM_TestModuleSpan xSpan = FindModule(xMutant, uId);
		WriteU32(xMutant, xSpan.m_uHeader, uId == 1u ? 2u : 1u);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA,
			"missing/duplicate/reordered module id");
	}
}

ZENITH_TEST(ZM_Save, Module_VersionMismatchReturnsVersionMismatch)
{
	const std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	for (uint32_t uId = 1u; uId <= uTEST_MODULE_COUNT; ++uId)
	{
		for (uint32_t uVersion : { 0u, 2u })
		{
			std::vector<uint8_t> xMutant = xBytes;
			const ZM_TestModuleSpan xSpan = FindModule(xMutant, uId);
			WriteU32(xMutant, xSpan.m_uHeader + 4u, uVersion);
			ExpectReadError(xMutant, Zenith_ErrorCode::VERSION_MISMATCH,
				"unsupported module version");
		}
	}
}

ZENITH_TEST(ZM_Save, Module_LengthAndTopLevelTrailingBytesAreExact)
{
	const std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	for (uint32_t uId = 1u; uId <= uTEST_MODULE_COUNT; ++uId)
	{
		const ZM_TestModuleSpan xOriginal = FindModule(xBytes, uId);
		if (!xOriginal.m_bFound) { continue; }
		for (int iDelta : { -1, 1 })
		{
			std::vector<uint8_t> xMutant = xBytes;
			const uint32_t uLength = (uint32_t)((int)xOriginal.m_uLength + iDelta);
			WriteU32(xMutant, xOriginal.m_uHeader + 8u, uLength);
			ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA,
				"module byteLength mismatch");
		}
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		const ZM_TestModuleSpan xParty = FindModule(xMutant, 1u);
		if (xParty.m_bFound)
		{
			WriteU32(xMutant, xParty.m_uHeader + 8u, 0xffffffffu);
			ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA,
				"module byteLength uint32 maximum");
		}
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		const ZM_TestModuleSpan xTower = FindModule(xMutant, 9u);
		if (xTower.m_bFound)
		{
			WriteU32(xMutant, xTower.m_uPayload, 3u);
			WriteU32(xMutant, xTower.m_uPayload + 4u, 2u);
			ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA,
				"tower current streak exceeds best streak");
		}
	}
	std::vector<uint8_t> xTrailing = xBytes;
	xTrailing.push_back(0x5au);
	ExpectReadError(xTrailing, Zenith_ErrorCode::CORRUPT_DATA, "top-level trailing byte");
}

ZENITH_TEST(ZM_Save, Truncation_EveryByteBoundaryReturnsCorruptData)
{
	const std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	for (u_int uLength = 0u; uLength < xBytes.size(); ++uLength)
	{
		std::vector<uint8_t> xTruncated(xBytes.begin(), xBytes.begin() + uLength);
		ZM_GameState xDestination = MakeWireFixture();
		const ZM_GameState xBefore = xDestination;
		Zenith_DataStream xStream(xTruncated.data(), xTruncated.size());
		const Zenith_Status xStatus = ZM_SaveSchema::Read(xStream, xTruncated.size(), xDestination);
		ZENITH_ASSERT_FALSE(xStatus.IsOk(), "truncation at byte %u decoded", uLength);
		ZENITH_ASSERT_EQ((u_int)xStatus.Error(), (u_int)Zenith_ErrorCode::CORRUPT_DATA,
			"truncation at byte %u returned wrong status", uLength);
		ZENITH_ASSERT_TRUE(xStream.GetCursor() == 0u,
			"truncation at byte %u advanced cursor", uLength);
		AssertStateEqual(xBefore, xDestination, "truncation transaction");
	}
}

ZENITH_TEST(ZM_Save, Party_CountCapIsEnforced)
{
	const std::vector<uint8_t> xBase = Encode(MakeWireFixture());
	const std::vector<uint8_t> xMonster = CopyMonsterRecord(xBase);
	for (uint8_t uCount : { (uint8_t)7u, (uint8_t)0xffu })
	{
		std::vector<uint8_t> xMutant = xBase;
		std::vector<uint8_t> xPayload = { uCount };
		for (u_int u = 0u; u < uCount; ++u)
		{
			xPayload.insert(xPayload.end(), xMonster.begin(), xMonster.end());
		}
		ReplaceModulePayload(xMutant, 1u, xPayload);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "party count above six");
	}
}

ZENITH_TEST(ZM_Save, Boxes_GridAndOccupancyDomainAreEnforced)
{
	const std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	const std::vector<uint8_t> xMonster = CopyMonsterRecord(xBytes);
	for (uint8_t uBoxes : { (uint8_t)15u, (uint8_t)17u })
	{
		std::vector<uint8_t> xMutant = xBytes;
		std::vector<uint8_t> xPayload = { uBoxes, (uint8_t)30u };
		xPayload.resize(2u + (u_int)uBoxes * 30u, 0u);
		ReplaceModulePayload(xMutant, 2u, xPayload);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "box count not sixteen");
	}
	for (uint8_t uSlots : { (uint8_t)29u, (uint8_t)31u })
	{
		std::vector<uint8_t> xMutant = xBytes;
		std::vector<uint8_t> xPayload = { (uint8_t)16u, uSlots };
		xPayload.resize(2u + 16u * (u_int)uSlots, 0u);
		ReplaceModulePayload(xMutant, 2u, xPayload);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "slots per box not thirty");
	}
	std::vector<uint8_t> xBadOccupancy = xBytes;
	std::vector<uint8_t> xPayload = { (uint8_t)16u, (uint8_t)30u, (uint8_t)2u };
	xPayload.insert(xPayload.end(), xMonster.begin(), xMonster.end());
	xPayload.resize(xPayload.size() + 479u, 0u);
	ReplaceModulePayload(xBadOccupancy, 2u, xPayload);
	ExpectReadError(xBadOccupancy, Zenith_ErrorCode::CORRUPT_DATA,
		"box occupancy outside boolean domain");
}

ZENITH_TEST(ZM_Save, Monster_IdentityAndProgressDomainsAreEnforced)
{
	const std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	const ZM_TestModuleSpan xParty = FindModule(xBytes, 1u);
	if (!xParty.m_bFound) { return; }
	const u_int uRecord = xParty.m_uPayload + 1u;
	struct Mutation { u_int m_uOffset; uint32_t m_uValue; u_int m_uWidth; const char* m_szName; };
	const Mutation axMutations[] =
	{
		{ 0u, 0u, 2u, "zero species wire id" },
		{ 0u, (uint32_t)ZM_SPECIES_COUNT + 1u, 2u, "species above table" },
		{ 2u, 0u, 1u, "level zero" },
		{ 2u, 101u, 1u, "level above cap" },
		{ 3u, 0xffffffffu, 4u, "exp inconsistent with level" },
		{ 19u, (uint32_t)ZM_NATURE_COUNT, 1u, "nature sentinel" },
		{ 20u, 2u, 1u, "ability slot above hidden" },
		{ 21u, (uint32_t)ZM_MAJOR_STATUS_COUNT, 1u, "major status sentinel" },
	};
	for (const Mutation& xMutation : axMutations)
	{
		std::vector<uint8_t> xMutant = xBytes;
		if (xMutation.m_uWidth == 1u) { xMutant[uRecord + xMutation.m_uOffset] = (uint8_t)xMutation.m_uValue; }
		else if (xMutation.m_uWidth == 2u) { WriteU16(xMutant, uRecord + xMutation.m_uOffset, (uint16_t)xMutation.m_uValue); }
		else { WriteU32(xMutant, uRecord + xMutation.m_uOffset, xMutation.m_uValue); }
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, xMutation.m_szName);
	}
}

ZENITH_TEST(ZM_Save, Monster_StatsAndVitalsDomainsAreEnforced)
{
	const std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	const ZM_TestModuleSpan xParty = FindModule(xBytes, 1u);
	if (!xParty.m_bFound) { return; }
	const u_int uRecord = xParty.m_uPayload + 1u;
	{
		ZM_GameState xMoveState = MakeWireFixture();
		ZM_Monster& xMoveMonster = xMoveState.m_xParty.Get(0u);
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
		{
			xMoveMonster.m_axMoves[u].m_eMove = ZM_MOVE_NONE;
			xMoveMonster.m_axMoves[u].m_uCurPP = 0u;
			xMoveMonster.m_axMoves[u].m_uMaxPP = 0u;
		}
		xMoveMonster.m_axMoves[0].m_eMove = ZM_MOVE_RAMBASH;
		xMoveMonster.m_axMoves[0].m_uCurPP = 7u;
		xMoveMonster.m_axMoves[0].m_uMaxPP = 19u;
		xMoveMonster.m_axMoves[1].m_eMove = ZM_MOVE_QUICKJAB;
		xMoveMonster.m_axMoves[1].m_uCurPP = 3u;
		xMoveMonster.m_axMoves[1].m_uMaxPP = 11u;
		xMoveMonster.m_axMoves[2].m_eMove = ZM_MOVE_BRUTESLAM;
		const std::vector<uint8_t> xMoveBytes = Encode(xMoveState);
		const ZM_TestModuleSpan xMoveParty = FindModule(xMoveBytes, 1u);
		if (xMoveParty.m_bFound)
		{
			const u_int uMoveRecord = xMoveParty.m_uPayload + 1u;
			ZENITH_ASSERT_EQ((u_int)ReadU16(xMoveBytes, uMoveRecord + 22u),
				(u_int)ZM_MOVE_RAMBASH + 1u, "slot zero move wire id");
			ZENITH_ASSERT_EQ((u_int)ReadU16(xMoveBytes, uMoveRecord + 24u),
				(u_int)ZM_MOVE_QUICKJAB + 1u, "slot one move wire id");
			ZENITH_ASSERT_EQ((u_int)ReadU16(xMoveBytes, uMoveRecord + 26u),
				(u_int)ZM_MOVE_BRUTESLAM + 1u, "slot two move wire id");
			ZENITH_ASSERT_EQ((u_int)ReadU16(xMoveBytes, uMoveRecord + 28u), 0u,
				"empty slot wire id");
			ZENITH_ASSERT_EQ((u_int)xMoveBytes[uMoveRecord + 30u], 7u, "slot zero current PP");
			ZENITH_ASSERT_EQ((u_int)xMoveBytes[uMoveRecord + 31u], 3u, "slot one current PP");
			ZENITH_ASSERT_EQ((u_int)xMoveBytes[uMoveRecord + 34u], 19u, "slot zero maximum PP");
			ZENITH_ASSERT_EQ((u_int)xMoveBytes[uMoveRecord + 35u], 11u, "slot one maximum PP");
			ZENITH_ASSERT_EQ((u_int)xMoveBytes[uMoveRecord + 32u], 0u,
				"real zero-PP move current PP");
			ZENITH_ASSERT_EQ((u_int)xMoveBytes[uMoveRecord + 36u], 0u,
				"real zero-PP move maximum PP");
		}
		ZM_GameState xMoveDecoded;
		const Zenith_Status xMoveStatus = Decode(xMoveBytes, xMoveDecoded);
		ZENITH_ASSERT_TRUE(xMoveStatus.IsOk(), "contracted real move with {0,0} PP was rejected");
		if (xMoveStatus.IsOk())
		{
			ZENITH_ASSERT_EQ((u_int)xMoveDecoded.m_xParty.Get(0u).m_axMoves[2].m_eMove,
				(u_int)ZM_MOVE_BRUTESLAM, "real zero-PP move id did not decode");
		}
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		xMutant[uRecord + 7u] = 32u;
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "IV above 31");
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		xMutant[uRecord + 13u] = 253u;
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "EV above 252");
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		xMutant[uRecord + 13u] = 252u;
		xMutant[uRecord + 14u] = 252u;
		xMutant[uRecord + 15u] = 7u;
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "EV total above 510");
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		const ZM_GameState xFixture = MakeWireFixture();
		WriteU32(xMutant, uRecord + 38u, xFixture.m_xParty.Get(0u).GetMaxHP() + 1u);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "current HP above derived max");
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		xMutant[uRecord + 42u] = (uint8_t)ZM_GENDER_COUNT;
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "gender sentinel");
	}
}

ZENITH_TEST(ZM_Save, Monster_MovesFlagsAndNicknameDomainsAreEnforced)
{
	const std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	const ZM_TestModuleSpan xParty = FindModule(xBytes, 1u);
	if (!xParty.m_bFound) { return; }
	const u_int uRecord = xParty.m_uPayload + 1u;
	{
		std::vector<uint8_t> xMutant = xBytes;
		WriteU16(xMutant, uRecord + 22u, (uint16_t)((u_int)ZM_MOVE_COUNT + 1u));
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "move id above table");
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		WriteU16(xMutant, uRecord + 22u, 0u);
		xMutant[uRecord + 30u] = 1u;
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "empty move with current PP");
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		WriteU16(xMutant, uRecord + 22u, 1u);
		xMutant[uRecord + 30u] = 2u;
		xMutant[uRecord + 34u] = 1u;
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "current PP above max PP");
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		xMutant[uRecord + 44u] = 4u;
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "reserved monster flag");
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		memset(xMutant.data() + uRecord + 45u, 'A', uZM_MONSTER_NICKNAME_CAPACITY);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "nickname without NUL");
	}
	{
		std::vector<uint8_t> xMutant = xBytes;
		xMutant[uRecord + 45u] = 'A';
		xMutant[uRecord + 46u] = 0u;
		xMutant[uRecord + 47u] = 'B';
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "nickname nonzero after NUL");
	}
	struct InvalidNicknameByte { uint8_t m_uByte; const char* m_szCase; };
	const InvalidNicknameByte axInvalidNicknameBytes[] =
	{
		{ 0x1fu, "nickname control byte" },
		{ 0x7fu, "nickname DEL byte" },
		{ 0x80u, "nickname high byte" },
		{ 0xffu, "nickname maximum byte" },
	};
	for (const InvalidNicknameByte& xInvalid : axInvalidNicknameBytes)
	{
		std::vector<uint8_t> xMutant = xBytes;
		memset(xMutant.data() + uRecord + 45u, 0, uZM_MONSTER_NICKNAME_CAPACITY);
		xMutant[uRecord + 45u] = xInvalid.m_uByte;
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, xInvalid.m_szCase);
	}
}

ZENITH_TEST(ZM_Save, Dex_WriterUsesCurrentCountAndCanonicalPadding)
{
	ZM_GameState xState;
	xState.MarkCaught(ZM_SPECIES_FERNFAWN);
	const std::vector<uint8_t> xBytes = Encode(xState);
	const ZM_TestModuleSpan xDex = FindModule(xBytes, 3u);
	const u_int uSpeciesCount = ZM_GetSpeciesCount();
	ZENITH_ASSERT_EQ((u_int)ReadU16(xBytes, xDex.m_uPayload), uSpeciesCount,
		"writer did not use the current roster count");
	const u_int uBitBytes = (uSpeciesCount + 7u) / 8u;
	ZENITH_ASSERT_EQ(xDex.m_uLength, 2u + uBitBytes * 2u, "dex payload length");
	const u_int uUsedBits = uSpeciesCount % 8u;
	if (uUsedBits != 0u)
	{
		const uint8_t uPaddingMask = (uint8_t)(0xffu << uUsedBits);
		ZENITH_ASSERT_EQ((u_int)(xBytes[xDex.m_uPayload + 2u + uBitBytes - 1u] & uPaddingMask), 0u,
			"seen padding was nonzero");
		ZENITH_ASSERT_EQ((u_int)(xBytes[xDex.m_uPayload + 2u + uBitBytes * 2u - 1u] & uPaddingMask), 0u,
			"caught padding was nonzero");
	}
}

ZENITH_TEST(ZM_Save, Dex_OlderCountZeroFillsAppendedSpecies)
{
	std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	ReplaceModulePayload(xBytes, 3u, MakeDexPayload(1u, 1u, 1u));
	ZM_GameState xDecoded = MakeMaximalState();
	const Zenith_Status xStatus = Decode(xBytes, xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "older one-species dex did not decode");
	ZENITH_ASSERT_TRUE(xDecoded.IsSeen((ZM_SPECIES_ID)0u), "encoded species was not seen");
	ZENITH_ASSERT_TRUE(xDecoded.IsCaught((ZM_SPECIES_ID)0u), "encoded species was not caught");
	for (u_int u = 1u; u < (u_int)ZM_SPECIES_COUNT; ++u)
	{
		ZENITH_ASSERT_FALSE(xDecoded.IsSeen((ZM_SPECIES_ID)u), "appended species %u was not zero-filled", u);
		ZENITH_ASSERT_FALSE(xDecoded.IsCaught((ZM_SPECIES_ID)u), "appended caught %u was not zero-filled", u);
	}
}

ZENITH_TEST(ZM_Save, Dex_NewerCountReturnsVersionMismatchAtomically)
{
	static_assert((u_int)ZM_SPECIES_COUNT < 512u, "test requires room below the schema cap");
	std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	ReplaceModulePayload(xBytes, 3u, MakeDexPayload((uint16_t)((u_int)ZM_SPECIES_COUNT + 1u), 0u, 0u));
	ExpectReadError(xBytes, Zenith_ErrorCode::VERSION_MISMATCH,
		"same-v1 dex from a newer roster");
}

ZENITH_TEST(ZM_Save, Dex_CountCapAndCaughtImpliesSeenAreEnforced)
{
	std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	std::vector<uint8_t> xTooMany = xBytes;
	ReplaceModulePayload(xTooMany, 3u, MakeDexPayload(513u, 0u, 0u));
	ExpectReadError(xTooMany, Zenith_ErrorCode::CORRUPT_DATA, "dex count above 512");

	std::vector<uint8_t> xSubset = xBytes;
	ReplaceModulePayload(xSubset, 3u, MakeDexPayload(8u, 0u, 1u));
	ExpectReadError(xSubset, Zenith_ErrorCode::CORRUPT_DATA, "caught bit without seen bit");

	std::vector<uint8_t> xPadding = xBytes;
	ReplaceModulePayload(xPadding, 3u, MakeDexPayload(1u, 2u, 0u));
	ExpectReadError(xPadding, Zenith_ErrorCode::CORRUPT_DATA, "nonzero dex padding");
}

ZENITH_TEST(ZM_Save, Story_WriterUsesHighestSetBitPlusOne)
{
	ZM_GameState xState;
	std::vector<uint8_t> xBytes = Encode(xState);
	ZM_TestModuleSpan xStory = FindModule(xBytes, 4u);
	ZENITH_ASSERT_EQ(xStory.m_uLength, 2u, "empty story payload is count only");
	ZENITH_ASSERT_EQ((u_int)ReadU16(xBytes, xStory.m_uPayload), 0u, "empty story count");

	xState.m_xStoryFlags.Set(9u, true);
	xBytes = Encode(xState);
	xStory = FindModule(xBytes, 4u);
	ZENITH_ASSERT_EQ((u_int)ReadU16(xBytes, xStory.m_uPayload), 10u,
		"highest set index 9 requires count 10");
	ZENITH_ASSERT_EQ(xStory.m_uLength, 4u, "count 10 requires two bit bytes");
	ZENITH_ASSERT_EQ((u_int)xBytes[xStory.m_uPayload + 2u], 0u, "story byte zero");
	ZENITH_ASSERT_EQ((u_int)xBytes[xStory.m_uPayload + 3u], 2u, "story bit nine");

	xState.m_xStoryFlags.Set(uZM_MAX_STORY_FLAGS - 1u, true);
	xBytes = Encode(xState);
	xStory = FindModule(xBytes, 4u);
	ZENITH_ASSERT_EQ((u_int)ReadU16(xBytes, xStory.m_uPayload), uZM_MAX_STORY_FLAGS,
		"last valid story bit requires full count");
	ZENITH_ASSERT_EQ(xStory.m_uLength, 2u + uZM_STORY_FLAG_BYTE_COUNT,
		"full high-water story payload length");
}

ZENITH_TEST(ZM_Save, Story_CountPaddingAndZeroFilledTailAreEnforced)
{
	std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	std::vector<uint8_t> xShortStory;
	AppendU16(xShortStory, 8u);
	xShortStory.push_back(1u);
	ReplaceModulePayload(xBytes, 4u, xShortStory);
	ZM_GameState xDecoded = MakeMaximalState();
	const Zenith_Status xStatus = Decode(xBytes, xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "short story bitset did not decode");
	ZENITH_ASSERT_TRUE(xDecoded.m_xStoryFlags.IsSet(0u), "encoded story bit zero lost");
	for (u_int u = 8u; u < uZM_MAX_STORY_FLAGS; ++u)
	{
		ZENITH_ASSERT_FALSE(xDecoded.m_xStoryFlags.IsSet(u), "story tail %u was not zero-filled", u);
	}

	std::vector<uint8_t> xTooMany = Encode(MakeWireFixture());
	std::vector<uint8_t> xTooManyPayload;
	AppendU16(xTooManyPayload, 4097u);
	xTooManyPayload.resize(2u + (4097u + 7u) / 8u, 0u);
	ReplaceModulePayload(xTooMany, 4u, xTooManyPayload);
	ExpectReadError(xTooMany, Zenith_ErrorCode::CORRUPT_DATA, "story count above 4096");

	std::vector<uint8_t> xPadding = Encode(MakeWireFixture());
	std::vector<uint8_t> xPaddingPayload;
	AppendU16(xPaddingPayload, 9u);
	xPaddingPayload.push_back(0u);
	xPaddingPayload.push_back(2u);
	ReplaceModulePayload(xPadding, 4u, xPaddingPayload);
	ExpectReadError(xPadding, Zenith_ErrorCode::CORRUPT_DATA, "nonzero story padding");
}

ZENITH_TEST(ZM_Save, Bag_CountIdsCountsOrderingAndUniquenessAreEnforced)
{
	ZM_GameState xOrdered;
	xOrdered.m_xBag.Add(ZM_ITEM_SALVE, 3u);
	xOrdered.m_xBag.Add(ZM_ITEM_GREATORB, 2u);
	xOrdered.m_xBag.Add(ZM_ITEM_CATCHORB, 1u);
	const std::vector<uint8_t> xOrderedBytes = Encode(xOrdered);
	const ZM_TestModuleSpan xBag = FindModule(xOrderedBytes, 6u);
	ZENITH_ASSERT_EQ((u_int)ReadU16(xOrderedBytes, xBag.m_uPayload), 3u, "bag entry count");
	ZENITH_ASSERT_EQ((u_int)ReadU16(xOrderedBytes, xBag.m_uPayload + 2u),
		(u_int)ZM_ITEM_CATCHORB, "first bag id is ascending");
	ZENITH_ASSERT_EQ((u_int)ReadU16(xOrderedBytes, xBag.m_uPayload + 6u),
		(u_int)ZM_ITEM_GREATORB, "second bag id is ascending");
	ZENITH_ASSERT_EQ((u_int)ReadU16(xOrderedBytes, xBag.m_uPayload + 10u),
		(u_int)ZM_ITEM_SALVE, "third bag id is ascending");

	const std::vector<uint8_t> xBase = Encode(MakeWireFixture());
	auto CheckPayload = [&](const std::vector<uint8_t>& xPayload, const char* szCase)
	{
		std::vector<uint8_t> xMutant = xBase;
		ReplaceModulePayload(xMutant, 6u, xPayload);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, szCase);
	};
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 513u);
		xPayload.resize(2u + 513u * 4u, 0u);
		CheckPayload(xPayload, "bag entry count above 512");
	}
	for (uint16_t uCount : { (uint16_t)0u, (uint16_t)1000u })
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, (uint16_t)ZM_ITEM_CATCHORB);
		AppendU16(xPayload, uCount);
		CheckPayload(xPayload, "bag stack count outside 1..999");
	}
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, (uint16_t)ZM_ITEM_COUNT);
		AppendU16(xPayload, 1u);
		CheckPayload(xPayload, "bag item id outside table");
	}
	for (bool bDuplicate : { true, false })
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 2u);
		AppendU16(xPayload, bDuplicate ? 0u : 1u);
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, 0u);
		AppendU16(xPayload, 1u);
		CheckPayload(xPayload, bDuplicate ? "duplicate bag id" : "descending bag ids");
	}
}

ZENITH_TEST(ZM_Save, Daycare_CountPresenceEggAndProgressInvariantsAreEnforced)
{
	const std::vector<uint8_t> xBase = Encode(MakeWireFixture());
	const std::vector<uint8_t> xMonster = CopyMonsterRecord(xBase);
	auto CheckPayload = [&](const std::vector<uint8_t>& xPayload, const char* szCase)
	{
		std::vector<uint8_t> xMutant = xBase;
		ReplaceModulePayload(xMutant, 8u, xPayload);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, szCase);
	};
	std::vector<uint8_t> xThreeParents = { 3u };
	for (u_int u = 0u; u < 3u; ++u)
	{
		xThreeParents.insert(xThreeParents.end(), xMonster.begin(), xMonster.end());
	}
	xThreeParents.push_back(0u);
	AppendU32(xThreeParents, 0u);
	CheckPayload(xThreeParents, "daycare parent count above two");
	CheckPayload({ 0u, 2u, 0u, 0u, 0u, 0u }, "daycare egg presence outside boolean");
	CheckPayload({ 0u, 0u, 1u, 0u, 0u, 0u }, "egg progress without egg");

	std::vector<uint8_t> xNoEggFlag;
	xNoEggFlag.push_back(0u);
	xNoEggFlag.push_back(1u);
	xNoEggFlag.insert(xNoEggFlag.end(), xMonster.begin(), xMonster.end());
	AppendU32(xNoEggFlag, 10u);
	CheckPayload(xNoEggFlag, "daycare egg record without IS_EGG");
}

ZENITH_TEST(ZM_Save, WorldPosition_SceneSpawnTagAndTransformContractsAreEnforced)
{
	const std::vector<uint8_t> xBase = Encode(MakeMaximalState());
	const ZM_TestModuleSpan xWorld = FindModule(xBase, 10u);
	if (!xWorld.m_bFound) { return; }
	const uint8_t auTransformBytes[] =
	{
		0x00u, 0x10u, 0x00u, 0x44u,
		0x00u, 0x00u, 0xd4u, 0x41u,
		0x00u, 0x60u, 0xf0u, 0x43u,
		0x00u, 0x00u, 0xa0u, 0xbfu,
	};
	for (u_int u = 0u; u < sizeof(auTransformBytes); ++u)
	{
		ZENITH_ASSERT_EQ((u_int)xBase[xWorld.m_uPayload + 36u + u],
			(u_int)auTransformBytes[u], "WorldPos IEEE-754 little-endian byte %u", u);
	}
	auto CheckMutation = [&](u_int uOffset, uint32_t uValue, const char* szCase)
	{
		std::vector<uint8_t> xMutant = xBase;
		WriteU32(xMutant, xWorld.m_uPayload + uOffset, uValue);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, szCase);
	};
	CheckMutation(0u, 9999u, "unknown world build index");
	CheckMutation(36u, 0x7fc00000u, "NaN world X");
	CheckMutation(48u, 0x7f800000u, "infinite world yaw");

	{
		std::vector<uint8_t> xMutant = xBase;
		WriteU32(xMutant, xWorld.m_uPayload, uZM_WORLD_SCENE_UNSET);
		xMutant[xWorld.m_uPayload + 4u] = 'X';
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA,
			"unset scene with nonempty tag");
	}
	{
		std::vector<uint8_t> xMutant = xBase;
		memset(xMutant.data() + xWorld.m_uPayload + 4u, 0, uZM_WORLD_SPAWN_TAG_CAPACITY);
		memcpy(xMutant.data() + xWorld.m_uPayload + 4u, "NoSuchTag", 9u);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA,
			"spawn tag absent from target WorldSpec row");
	}
	{
		std::vector<uint8_t> xMutant = xBase;
		memset(xMutant.data() + xWorld.m_uPayload + 4u, 'A', uZM_WORLD_SPAWN_TAG_CAPACITY);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, "spawn tag without NUL");
	}
	{
		std::vector<uint8_t> xMutant = xBase;
		memset(xMutant.data() + xWorld.m_uPayload + 4u, 0, uZM_WORLD_SPAWN_TAG_CAPACITY);
		xMutant[xWorld.m_uPayload + 4u] = 0x1fu;
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA,
			"spawn tag with nonprintable byte");
	}
	{
		std::vector<uint8_t> xMutant = xBase;
		xMutant[xWorld.m_uPayload + 4u + 11u] = 'X';
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA,
			"spawn tag with nonzero padding after NUL");
	}
}

ZENITH_TEST(ZM_Save, Options_KnownFieldRoundTripsAndUnknownTlvIsSkipped)
{
	std::vector<uint8_t> xBytes = Encode(MakeWireFixture());
	const ZM_TestModuleSpan xOriginal = FindModule(xBytes, 11u);
	ZENITH_ASSERT_EQ(xOriginal.m_uLength, 7u, "canonical options payload length");
	ZENITH_ASSERT_EQ((u_int)ReadU16(xBytes, xOriginal.m_uPayload), 1u, "options field count");
	ZENITH_ASSERT_EQ((u_int)ReadU16(xBytes, xOriginal.m_uPayload + 2u), 1u, "text-speed tag");
	ZENITH_ASSERT_EQ((u_int)ReadU16(xBytes, xOriginal.m_uPayload + 4u), 1u, "text-speed length");
	ZENITH_ASSERT_EQ((u_int)xBytes[xOriginal.m_uPayload + 6u], (u_int)ZM_TEXT_SPEED_FAST,
		"text-speed value");

	std::vector<uint8_t> xOptions;
	AppendU16(xOptions, 2u);
	AppendU16(xOptions, 77u);
	AppendU16(xOptions, 3u);
	xOptions.push_back(0xaau);
	xOptions.push_back(0xbbu);
	xOptions.push_back(0xccu);
	AppendU16(xOptions, 1u);
	AppendU16(xOptions, 1u);
	xOptions.push_back((uint8_t)ZM_TEXT_SPEED_SLOW);
	ReplaceModulePayload(xBytes, 11u, xOptions);
	ZM_GameState xDecoded;
	const Zenith_Status xStatus = Decode(xBytes, xDecoded);
	ZENITH_ASSERT_TRUE(xStatus.IsOk(), "bounded unknown option TLV was not skipped");
	ZENITH_ASSERT_EQ((u_int)xDecoded.m_xOptions.m_eTextSpeed, (u_int)ZM_TEXT_SPEED_SLOW,
		"known field after unknown TLV was not decoded");
}

ZENITH_TEST(ZM_Save, Options_FieldCountTagLengthAndDuplicateKnownTagAreEnforced)
{
	const std::vector<uint8_t> xBase = Encode(MakeWireFixture());
	auto CheckPayload = [&](const std::vector<uint8_t>& xPayload, const char* szCase)
	{
		std::vector<uint8_t> xMutant = xBase;
		ReplaceModulePayload(xMutant, 11u, xPayload);
		ExpectReadError(xMutant, Zenith_ErrorCode::CORRUPT_DATA, szCase);
	};
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, 77u);
		AppendU16(xPayload, 2u);
		xPayload.push_back(0xaau);
		xPayload.push_back(0xbbu);
		CheckPayload(xPayload, "unknown-only options omitted required text speed");
	}
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, 1u);
		xPayload.push_back((uint8_t)ZM_TEXT_SPEED_COUNT);
		CheckPayload(xPayload, "invalid text speed");
	}
	for (uint16_t uLength : { (uint16_t)0u, (uint16_t)2u })
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, uLength);
		for (uint16_t u = 0u; u < uLength; ++u) { xPayload.push_back(0u); }
		CheckPayload(xPayload, "known option length not one");
	}
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 2u);
		for (u_int u = 0u; u < 2u; ++u)
		{
			AppendU16(xPayload, 1u);
			AppendU16(xPayload, 1u);
			xPayload.push_back((uint8_t)ZM_TEXT_SPEED_NORMAL);
		}
		CheckPayload(xPayload, "duplicate known option tag");
	}
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 2u);
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, 1u);
		xPayload.push_back((uint8_t)ZM_TEXT_SPEED_NORMAL);
		CheckPayload(xPayload, "field count exceeds supplied TLVs");
	}
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, 77u);
		AppendU16(xPayload, 4u);
		xPayload.push_back(0u);
		CheckPayload(xPayload, "unknown TLV length exceeds module");
	}
	{
		std::vector<uint8_t> xPayload;
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, 1u);
		AppendU16(xPayload, 1u);
		xPayload.push_back((uint8_t)ZM_TEXT_SPEED_NORMAL);
		xPayload.push_back(0u);
		CheckPayload(xPayload, "options trailing byte");
	}
}
