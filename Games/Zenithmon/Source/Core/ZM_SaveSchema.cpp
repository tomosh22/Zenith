#include "Zenith.h"

#include "Zenithmon/Source/Core/ZM_SaveSchema.h"

#include "Zenithmon/Source/Battle/ZM_ExpAndLevel.h"
#include "Zenithmon/Source/Data/ZM_MoveData.h"
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace
{
	static constexpr uint32_t uMODULE_PARTY = 1u;
	static constexpr uint32_t uMODULE_BOXES = 2u;
	static constexpr uint32_t uMODULE_DEX = 3u;
	static constexpr uint32_t uMODULE_STORY = 4u;
	static constexpr uint32_t uMODULE_BADGES = 5u;
	static constexpr uint32_t uMODULE_BAG = 6u;
	static constexpr uint32_t uMODULE_MONEY = 7u;
	static constexpr uint32_t uMODULE_DAYCARE = 8u;
	static constexpr uint32_t uMODULE_TOWER = 9u;
	static constexpr uint32_t uMODULE_WORLD = 10u;
	static constexpr uint32_t uMODULE_OPTIONS = 11u;

	static constexpr uint16_t uOPTION_TAG_TEXT_SPEED = 1u;
	static constexpr uint16_t uDEX_SANITY_CAP = 512u;
	static constexpr uint16_t uBAG_ENTRY_SANITY_CAP = 512u;
	static constexpr uint32_t uMONSTER_WIRE_BYTES = 61u;
	static_assert((u_int)ZM_SPECIES_COUNT <= uDEX_SANITY_CAP,
		"The species table exceeds the save schema Dex sanity cap");
	static_assert(2u + 1u + 4u + ZM_STAT_COUNT + ZM_STAT_COUNT + 1u + 1u + 1u
		+ uZM_MAX_MOVES * 2u + uZM_MAX_MOVES + uZM_MAX_MOVES + 4u + 1u + 1u + 1u
		+ uZM_MONSTER_NICKNAME_CAPACITY == uMONSTER_WIRE_BYTES);

	Zenith_Status Corrupt(const char* szModule, const char* szField)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY, "[ZM Save] corrupt %s.%s", szModule, szField);
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	Zenith_Status InvalidSource(const char* szModule, const char* szField)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY, "[ZM Save] invalid source %s.%s", szModule, szField);
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	class ZM_ByteWriter
	{
	public:
		void U8(uint8_t uValue) { Bytes(&uValue, sizeof(uValue)); }

		void U16(uint16_t uValue)
		{
			const uint8_t auBytes[2] =
			{
				(uint8_t)(uValue & 0xffu),
				(uint8_t)((uValue >> 8u) & 0xffu),
			};
			Bytes(auBytes, sizeof(auBytes));
		}

		void U32(uint32_t uValue)
		{
			const uint8_t auBytes[4] =
			{
				(uint8_t)(uValue & 0xffu),
				(uint8_t)((uValue >> 8u) & 0xffu),
				(uint8_t)((uValue >> 16u) & 0xffu),
				(uint8_t)((uValue >> 24u) & 0xffu),
			};
			Bytes(auBytes, sizeof(auBytes));
		}

		void U64(uint64_t ulValue)
		{
			uint8_t auBytes[8];
			for (u_int u = 0u; u < 8u; ++u)
			{
				auBytes[u] = (uint8_t)((ulValue >> (u * 8u)) & 0xffull);
			}
			Bytes(auBytes, sizeof(auBytes));
		}

		void Float(float fValue)
		{
			uint32_t uBits = 0u;
			static_assert(sizeof(fValue) == sizeof(uBits));
			memcpy(&uBits, &fValue, sizeof(uBits));
			U32(uBits);
		}

		void Bytes(const void* pData, uint64_t ulSize)
		{
			if (!m_bOk || ulSize == 0u) { return; }
			const uint64_t ulBefore = m_xStream.GetCursor();
			if (ulBefore > (std::numeric_limits<uint64_t>::max)() - ulSize)
			{
				m_bOk = false;
				return;
			}
			m_xStream.WriteData(pData, ulSize);
			m_bOk = m_xStream.GetCursor() == ulBefore + ulSize;
		}

		uint64_t BeginModule(uint32_t uModuleId)
		{
			const uint64_t ulHeader = Cursor();
			U32(uModuleId);
			U32(ZM_SaveSchema::uMODULE_VERSION_CURRENT);
			U32(0u);
			return ulHeader;
		}

		void EndModule(uint64_t ulHeader)
		{
			if (!m_bOk) { return; }
			const uint64_t ulEnd = Cursor();
			const uint64_t ulPayload = ulHeader + 12u;
			if (ulEnd < ulPayload || ulEnd - ulPayload > (std::numeric_limits<uint32_t>::max)())
			{
				m_bOk = false;
				return;
			}
			m_xStream.SetCursor(ulHeader + 8u);
			U32((uint32_t)(ulEnd - ulPayload));
			m_xStream.SetCursor(ulEnd);
		}

		bool IsOk() const { return m_bOk; }
		uint64_t Cursor() const { return m_xStream.GetCursor(); }
		const Zenith_DataStream& Stream() const { return m_xStream; }

	private:
		Zenith_DataStream m_xStream;
		bool m_bOk = true;
	};

	class ZM_ByteReader
	{
	public:
		ZM_ByteReader() = default;
		ZM_ByteReader(const uint8_t* pData, uint64_t ulLength)
			: m_pData(pData), m_ulLength(ulLength) {}

		bool U8(uint8_t& uOut)
		{
			return Bytes(&uOut, sizeof(uOut));
		}

		bool U16(uint16_t& uOut)
		{
			uint8_t auBytes[2];
			if (!Bytes(auBytes, sizeof(auBytes))) { return false; }
			uOut = (uint16_t)((uint16_t)auBytes[0]
				| ((uint16_t)auBytes[1] << 8u));
			return true;
		}

		bool U32(uint32_t& uOut)
		{
			uint8_t auBytes[4];
			if (!Bytes(auBytes, sizeof(auBytes))) { return false; }
			uOut = (uint32_t)auBytes[0]
				| ((uint32_t)auBytes[1] << 8u)
				| ((uint32_t)auBytes[2] << 16u)
				| ((uint32_t)auBytes[3] << 24u);
			return true;
		}

		bool U64(uint64_t& ulOut)
		{
			uint8_t auBytes[8];
			if (!Bytes(auBytes, sizeof(auBytes))) { return false; }
			ulOut = 0u;
			for (u_int u = 0u; u < 8u; ++u)
			{
				ulOut |= (uint64_t)auBytes[u] << (u * 8u);
			}
			return true;
		}

		bool Float(float& fOut)
		{
			uint32_t uBits = 0u;
			if (!U32(uBits)) { return false; }
			static_assert(sizeof(fOut) == sizeof(uBits));
			memcpy(&fOut, &uBits, sizeof(fOut));
			return true;
		}

		bool Bytes(void* pOut, uint64_t ulSize)
		{
			if (ulSize > Remaining()) { return false; }
			if (ulSize > 0u)
			{
				if (pOut == nullptr || m_pData == nullptr) { return false; }
				memcpy(pOut, m_pData + m_ulCursor, (size_t)ulSize);
			}
			m_ulCursor += ulSize;
			return true;
		}

		bool Skip(uint64_t ulSize)
		{
			if (ulSize > Remaining()) { return false; }
			m_ulCursor += ulSize;
			return true;
		}

		bool Take(uint64_t ulSize, ZM_ByteReader& xOut)
		{
			if (ulSize > Remaining()) { return false; }
			xOut = ZM_ByteReader(m_pData + m_ulCursor, ulSize);
			m_ulCursor += ulSize;
			return true;
		}

		uint64_t Remaining() const { return m_ulLength - m_ulCursor; }
		bool IsAtEnd() const { return m_ulCursor == m_ulLength; }

	private:
		const uint8_t* m_pData = nullptr;
		uint64_t m_ulLength = 0u;
		uint64_t m_ulCursor = 0u;
	};

	bool IsPrintablePadded(const char* szValue, u_int uCapacity, bool bAllowEmpty)
	{
		u_int uTerminator = uCapacity;
		for (u_int u = 0u; u < uCapacity; ++u)
		{
			const uint8_t uByte = (uint8_t)szValue[u];
			if (uByte == 0u)
			{
				uTerminator = u;
				break;
			}
			if (uByte < 0x20u || uByte > 0x7eu) { return false; }
		}
		if (uTerminator == uCapacity || (!bAllowEmpty && uTerminator == 0u)) { return false; }
		for (u_int u = uTerminator + 1u; u < uCapacity; ++u)
		{
			if (szValue[u] != '\0') { return false; }
		}
		return true;
	}

	bool IsAllZero(const char* pBytes, u_int uCount)
	{
		for (u_int u = 0u; u < uCount; ++u)
		{
			if (pBytes[u] != '\0') { return false; }
		}
		return true;
	}

	bool ValidateMonster(const ZM_Monster& xMonster, const char*& szField)
	{
		if ((u_int)xMonster.m_eSpecies >= (u_int)ZM_SPECIES_COUNT)
		{
			szField = "speciesId";
			return false;
		}
		if (xMonster.m_uLevel < 1u || xMonster.m_uLevel > 100u)
		{
			szField = "level";
			return false;
		}
		if (ZM_LevelForExp(ZM_GetSpeciesGrowthRate(xMonster.m_eSpecies),
			xMonster.m_uCurrentExp) != xMonster.m_uLevel)
		{
			szField = "exp";
			return false;
		}
		u_int64 ulEVTotal = 0u;
		for (u_int u = 0u; u < ZM_STAT_COUNT; ++u)
		{
			if (xMonster.m_auIV[u] > 31u)
			{
				szField = "ivs";
				return false;
			}
			if (xMonster.m_auEV[u] > 252u)
			{
				szField = "evs";
				return false;
			}
			ulEVTotal += xMonster.m_auEV[u];
		}
		if (ulEVTotal > 510u)
		{
			szField = "evTotal";
			return false;
		}
		if ((u_int)xMonster.m_eNature >= (u_int)ZM_NATURE_COUNT)
		{
			szField = "nature";
			return false;
		}
		const ZM_SpeciesAbilities xAbilities = ZM_GetSpeciesAbilities(xMonster.m_eSpecies);
		if (xMonster.m_eAbility != xAbilities.m_eRegular
			&& xMonster.m_eAbility != xAbilities.m_eHidden)
		{
			szField = "abilitySlot";
			return false;
		}
		if ((u_int)xMonster.m_eStatus >= (u_int)ZM_MAJOR_STATUS_COUNT)
		{
			szField = "status";
			return false;
		}
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
		{
			const ZM_MoveSlot& xMove = xMonster.m_axMoves[u];
			if (xMove.m_eMove == ZM_MOVE_NONE)
			{
				if (xMove.m_uCurPP != 0u || xMove.m_uMaxPP != 0u)
				{
					szField = "emptyMovePP";
					return false;
				}
			}
			else if ((u_int)xMove.m_eMove >= (u_int)ZM_MOVE_COUNT)
			{
				szField = "moveId";
				return false;
			}
			if (xMove.m_uCurPP > xMove.m_uMaxPP || xMove.m_uCurPP > 255u
				|| xMove.m_uMaxPP > 255u)
			{
				szField = "movePP";
				return false;
			}
		}
		if (xMonster.m_uCurrentHp > xMonster.GetMaxHP())
		{
			szField = "currentHp";
			return false;
		}
		if ((u_int)xMonster.m_eGender >= (u_int)ZM_GENDER_COUNT)
		{
			szField = "gender";
			return false;
		}
		if (xMonster.m_uFriendship > 255u)
		{
			szField = "friendship";
			return false;
		}
		if ((xMonster.m_uFlags & ~(uZM_MONSTER_FLAG_IS_EGG | uZM_MONSTER_FLAG_IS_SHINY)) != 0u)
		{
			szField = "flags";
			return false;
		}
		if (!IsPrintablePadded(xMonster.m_szNickname, uZM_MONSTER_NICKNAME_CAPACITY, true))
		{
			szField = "nickname";
			return false;
		}
		return true;
	}

	bool ValidateWorldPosition(const ZM_WorldPosition& xWorld, const char*& szField)
	{
		for (u_int u = 0u; u < 3u; ++u)
		{
			if (!std::isfinite(xWorld.m_afPosition[u]))
			{
				szField = "position";
				return false;
			}
		}
		if (!std::isfinite(xWorld.m_fYaw))
		{
			szField = "yaw";
			return false;
		}
		if (xWorld.m_uSceneBuildIndex == uZM_WORLD_SCENE_UNSET)
		{
			if (!IsAllZero(xWorld.m_szSpawnTag, uZM_WORLD_SPAWN_TAG_CAPACITY))
			{
				szField = "unsetSpawnTag";
				return false;
			}
			return true;
		}
		const ZM_SCENE_ID eScene = ZM_FindSceneByBuildIndex(xWorld.m_uSceneBuildIndex);
		if (eScene == ZM_SCENE_NONE)
		{
			szField = "sceneBuildIndex";
			return false;
		}
		if (!IsPrintablePadded(xWorld.m_szSpawnTag, uZM_WORLD_SPAWN_TAG_CAPACITY, false))
		{
			szField = "spawnTag";
			return false;
		}
		const ZM_WorldSpec& xSpec = ZM_GetWorldSpec(eScene);
		for (u_int u = 0u; u < xSpec.m_uSpawnTagCount; ++u)
		{
			if (strcmp(xSpec.m_pszSpawnTags[u], xWorld.m_szSpawnTag) == 0) { return true; }
		}
		szField = "spawnTagResolve";
		return false;
	}

	bool ValidateState(const ZM_GameState& xState, const char*& szModule, const char*& szField)
	{
		if (xState.m_xParty.Count() > uZM_MAX_PARTY_SIZE)
		{
			szModule = "Party";
			szField = "count";
			return false;
		}
		for (u_int u = 0u; u < xState.m_xParty.Count(); ++u)
		{
			if (!ValidateMonster(xState.m_xParty.Get(u), szField))
			{
				szModule = "Party";
				return false;
			}
		}

		u_int uOccupiedBoxes = 0u;
		for (u_int uBox = 0u; uBox < uZM_BOX_COUNT; ++uBox)
		{
			for (u_int uSlot = 0u; uSlot < uZM_BOX_SLOTS_PER_BOX; ++uSlot)
			{
				const ZM_Monster* pxMonster = xState.m_xBoxes.TryGet(uBox, uSlot);
				if (pxMonster == nullptr) { continue; }
				++uOccupiedBoxes;
				if (!ValidateMonster(*pxMonster, szField))
				{
					szModule = "Boxes";
					return false;
				}
			}
		}
		if (uOccupiedBoxes != xState.m_xBoxes.Count())
		{
			szModule = "Boxes";
			szField = "occupiedCount";
			return false;
		}

		for (u_int u = 0u; u < (u_int)ZM_SPECIES_COUNT; ++u)
		{
			if (xState.m_xCaught.m_abFlags[u] && !xState.m_xSeen.m_abFlags[u])
			{
				szModule = "Dex";
				szField = "caughtImpliesSeen";
				return false;
			}
		}

		for (u_int uCategory = 0u; uCategory < (u_int)ZM_ITEM_CATEGORY_COUNT; ++uCategory)
		{
			if (xState.m_xBag.m_auPocketCount[uCategory] > uZM_BAG_MAX_STACKS_PER_POCKET)
			{
				szModule = "Bag";
				szField = "pocketCount";
				return false;
			}
		}

		u_int uBagEntries = 0u;
		for (u_int uCategory = 0u; uCategory < (u_int)ZM_ITEM_CATEGORY_COUNT; ++uCategory)
		{
			const u_int uPocketCount = xState.m_xBag.m_auPocketCount[uCategory];
			if (uBagEntries > uBAG_ENTRY_SANITY_CAP - uPocketCount)
			{
				szModule = "Bag";
				szField = "entryCount";
				return false;
			}
			uBagEntries += uPocketCount;
			for (u_int uSlot = 0u; uSlot < uPocketCount; ++uSlot)
			{
				const ZM_ItemStack& xStack = xState.m_xBag.m_axPocket[uCategory][uSlot];
				if ((u_int)xStack.m_eItem >= (u_int)ZM_ITEM_COUNT)
				{
					szModule = "Bag";
					szField = "itemId";
					return false;
				}
				if ((u_int)ZM_GetItemData(xStack.m_eItem).m_eCategory != uCategory)
				{
					szModule = "Bag";
					szField = "pocketCategory";
					return false;
				}
				if (xStack.m_uCount == 0u || xStack.m_uCount > uZM_BAG_MAX_STACK_COUNT)
				{
					szModule = "Bag";
					szField = "count";
					return false;
				}
				if (uSlot > 0u && (u_int)xState.m_xBag.m_axPocket[uCategory][uSlot - 1u].m_eItem
					>= (u_int)xStack.m_eItem)
				{
					szModule = "Bag";
					szField = "ordering";
					return false;
				}
			}
		}

		if (xState.m_xDaycare.m_uParentCount > uZM_DAYCARE_PARENT_CAPACITY)
		{
			szModule = "Daycare";
			szField = "parentCount";
			return false;
		}
		for (u_int u = 0u; u < xState.m_xDaycare.m_uParentCount; ++u)
		{
			if (!ValidateMonster(xState.m_xDaycare.m_axParents[u], szField))
			{
				szModule = "Daycare";
				return false;
			}
		}
		if (xState.m_xDaycare.m_bEggPresent)
		{
			if (!ValidateMonster(xState.m_xDaycare.m_xEgg, szField))
			{
				szModule = "Daycare";
				return false;
			}
			if ((xState.m_xDaycare.m_xEgg.m_uFlags & uZM_MONSTER_FLAG_IS_EGG) == 0u)
			{
				szModule = "Daycare";
				szField = "eggFlag";
				return false;
			}
		}
		else if (xState.m_xDaycare.m_uEggStepsRemaining != 0u)
		{
			szModule = "Daycare";
			szField = "eggStepsRemaining";
			return false;
		}
		if (xState.m_xTowerRun.m_uCurrentStreak > xState.m_xTowerRun.m_uBestStreak)
		{
			szModule = "Tower";
			szField = "currentStreak";
			return false;
		}

		if (!ValidateWorldPosition(xState.m_xWorldPosition, szField))
		{
			szModule = "WorldPos";
			return false;
		}
		if ((u_int)xState.m_xOptions.m_eTextSpeed >= (u_int)ZM_TEXT_SPEED_COUNT)
		{
			szModule = "Options";
			szField = "textSpeed";
			return false;
		}
		return true;
	}

	void WriteMonster(ZM_ByteWriter& xWriter, const ZM_Monster& xMonster)
	{
		xWriter.U16((uint16_t)((u_int)xMonster.m_eSpecies + 1u));
		xWriter.U8((uint8_t)xMonster.m_uLevel);
		xWriter.U32((uint32_t)xMonster.m_uCurrentExp);
		for (u_int u = 0u; u < ZM_STAT_COUNT; ++u) { xWriter.U8((uint8_t)xMonster.m_auIV[u]); }
		for (u_int u = 0u; u < ZM_STAT_COUNT; ++u) { xWriter.U8((uint8_t)xMonster.m_auEV[u]); }
		xWriter.U8((uint8_t)xMonster.m_eNature);
		const ZM_SpeciesAbilities xAbilities = ZM_GetSpeciesAbilities(xMonster.m_eSpecies);
		xWriter.U8((uint8_t)(xMonster.m_eAbility == xAbilities.m_eRegular ? 0u : 1u));
		xWriter.U8((uint8_t)xMonster.m_eStatus);
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
		{
			xWriter.U16(xMonster.m_axMoves[u].m_eMove == ZM_MOVE_NONE
				? (uint16_t)0u : (uint16_t)((u_int)xMonster.m_axMoves[u].m_eMove + 1u));
		}
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u) { xWriter.U8((uint8_t)xMonster.m_axMoves[u].m_uCurPP); }
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u) { xWriter.U8((uint8_t)xMonster.m_axMoves[u].m_uMaxPP); }
		xWriter.U32((uint32_t)xMonster.m_uCurrentHp);
		xWriter.U8((uint8_t)xMonster.m_eGender);
		xWriter.U8((uint8_t)xMonster.m_uFriendship);
		xWriter.U8((uint8_t)xMonster.m_uFlags);
		xWriter.Bytes(xMonster.m_szNickname, uZM_MONSTER_NICKNAME_CAPACITY);
	}

	Zenith_Status ReadMonster(ZM_ByteReader& xReader, ZM_Monster& xMonster,
		const char* szModule)
	{
		uint16_t uSpeciesWire = 0u;
		uint8_t uLevel = 0u;
		uint32_t uExp = 0u;
		if (!xReader.U16(uSpeciesWire) || !xReader.U8(uLevel) || !xReader.U32(uExp))
		{
			return Corrupt(szModule, "monster.identityTruncated");
		}
		if (uSpeciesWire == 0u || uSpeciesWire > (uint16_t)ZM_SPECIES_COUNT)
		{
			return Corrupt(szModule, "monster.speciesId");
		}
		xMonster.m_eSpecies = (ZM_SPECIES_ID)(uSpeciesWire - 1u);
		xMonster.m_uLevel = uLevel;
		xMonster.m_uCurrentExp = uExp;
		for (u_int u = 0u; u < ZM_STAT_COUNT; ++u)
		{
			uint8_t uValue = 0u;
			if (!xReader.U8(uValue)) { return Corrupt(szModule, "monster.ivsTruncated"); }
			xMonster.m_auIV[u] = uValue;
		}
		for (u_int u = 0u; u < ZM_STAT_COUNT; ++u)
		{
			uint8_t uValue = 0u;
			if (!xReader.U8(uValue)) { return Corrupt(szModule, "monster.evsTruncated"); }
			xMonster.m_auEV[u] = uValue;
		}
		uint8_t uNature = 0u;
		uint8_t uAbilitySlot = 0u;
		uint8_t uStatus = 0u;
		if (!xReader.U8(uNature) || !xReader.U8(uAbilitySlot) || !xReader.U8(uStatus))
		{
			return Corrupt(szModule, "monster.traitsTruncated");
		}
		if (uAbilitySlot > 1u) { return Corrupt(szModule, "monster.abilitySlot"); }
		xMonster.m_eNature = (ZM_NATURE)uNature;
		const ZM_SpeciesAbilities xAbilities = ZM_GetSpeciesAbilities(xMonster.m_eSpecies);
		xMonster.m_eAbility = uAbilitySlot == 0u ? xAbilities.m_eRegular : xAbilities.m_eHidden;
		xMonster.m_eStatus = (ZM_MAJOR_STATUS)uStatus;
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
		{
			uint16_t uMoveWire = 0u;
			if (!xReader.U16(uMoveWire)) { return Corrupt(szModule, "monster.moveIdsTruncated"); }
			if (uMoveWire > (uint16_t)ZM_MOVE_COUNT) { return Corrupt(szModule, "monster.moveId"); }
			xMonster.m_axMoves[u].m_eMove = uMoveWire == 0u
				? ZM_MOVE_NONE : (ZM_MOVE_ID)(uMoveWire - 1u);
		}
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
		{
			uint8_t uPP = 0u;
			if (!xReader.U8(uPP)) { return Corrupt(szModule, "monster.currentPPTruncated"); }
			xMonster.m_axMoves[u].m_uCurPP = uPP;
		}
		for (u_int u = 0u; u < uZM_MAX_MOVES; ++u)
		{
			uint8_t uPP = 0u;
			if (!xReader.U8(uPP)) { return Corrupt(szModule, "monster.maxPPTruncated"); }
			xMonster.m_axMoves[u].m_uMaxPP = uPP;
		}
		uint32_t uCurrentHp = 0u;
		uint8_t uGender = 0u;
		uint8_t uFriendship = 0u;
		uint8_t uFlags = 0u;
		if (!xReader.U32(uCurrentHp) || !xReader.U8(uGender)
			|| !xReader.U8(uFriendship) || !xReader.U8(uFlags)
			|| !xReader.Bytes(xMonster.m_szNickname, uZM_MONSTER_NICKNAME_CAPACITY))
		{
			return Corrupt(szModule, "monster.tailTruncated");
		}
		xMonster.m_uCurrentHp = uCurrentHp;
		xMonster.m_eGender = (ZM_GENDER)uGender;
		xMonster.m_uFriendship = uFriendship;
		xMonster.m_uFlags = uFlags;
		const char* szField = nullptr;
		if (!ValidateMonster(xMonster, szField)) { return Corrupt(szModule, szField); }
		return true;
	}

	Zenith_Status ParseParty(ZM_ByteReader& xReader, ZM_GameState& xState)
	{
		uint8_t uCount = 0u;
		if (!xReader.U8(uCount)) { return Corrupt("Party", "countTruncated"); }
		if (uCount > uZM_MAX_PARTY_SIZE) { return Corrupt("Party", "count"); }
		for (u_int u = 0u; u < uCount; ++u)
		{
			ZM_Monster xMonster;
			Zenith_Status xStatus = ReadMonster(xReader, xMonster, "Party");
			if (!xStatus.IsOk()) { return xStatus.Error(); }
			if (!xState.m_xParty.Add(xMonster)) { return Corrupt("Party", "restore"); }
		}
		return true;
	}

	Zenith_Status ParseBoxes(ZM_ByteReader& xReader, ZM_GameState& xState)
	{
		uint8_t uBoxCount = 0u;
		uint8_t uSlotsPerBox = 0u;
		if (!xReader.U8(uBoxCount) || !xReader.U8(uSlotsPerBox))
		{
			return Corrupt("Boxes", "shapeTruncated");
		}
		if (uBoxCount != uZM_BOX_COUNT || uSlotsPerBox != uZM_BOX_SLOTS_PER_BOX)
		{
			return Corrupt("Boxes", "shape");
		}
		for (u_int uBox = 0u; uBox < uZM_BOX_COUNT; ++uBox)
		{
			for (u_int uSlot = 0u; uSlot < uZM_BOX_SLOTS_PER_BOX; ++uSlot)
			{
				uint8_t uOccupied = 0u;
				if (!xReader.U8(uOccupied)) { return Corrupt("Boxes", "occupancyTruncated"); }
				if (uOccupied > 1u) { return Corrupt("Boxes", "occupied"); }
				if (uOccupied == 0u) { continue; }
				ZM_Monster xMonster;
				Zenith_Status xStatus = ReadMonster(xReader, xMonster, "Boxes");
				if (!xStatus.IsOk()) { return xStatus.Error(); }
				if (!xState.m_xBoxes.StoreAt(uBox, uSlot, xMonster))
				{
					return Corrupt("Boxes", "restore");
				}
			}
		}
		return true;
	}

	Zenith_Status ParseDex(ZM_ByteReader& xReader, ZM_GameState& xState)
	{
		uint16_t uSpeciesCount = 0u;
		if (!xReader.U16(uSpeciesCount)) { return Corrupt("Dex", "speciesCountTruncated"); }
		if (uSpeciesCount > uDEX_SANITY_CAP) { return Corrupt("Dex", "speciesCount"); }
		if (uSpeciesCount > (uint16_t)ZM_SPECIES_COUNT)
		{
			Zenith_Error(LOG_CATEGORY_GAMEPLAY,
				"[ZM Save] dex species count %u is newer than current roster %u",
				(u_int)uSpeciesCount, (u_int)ZM_SPECIES_COUNT);
			return Zenith_ErrorCode::VERSION_MISMATCH;
		}
		const u_int uByteCount = ((u_int)uSpeciesCount + 7u) / 8u;
		uint8_t auSeen[uDEX_SANITY_CAP / 8u] = {};
		uint8_t auCaught[uDEX_SANITY_CAP / 8u] = {};
		if (!xReader.Bytes(auSeen, uByteCount) || !xReader.Bytes(auCaught, uByteCount))
		{
			return Corrupt("Dex", "bitsetsTruncated");
		}
		if (uByteCount > 0u && (uSpeciesCount % 8u) != 0u)
		{
			const uint8_t uValidMask = (uint8_t)((1u << (uSpeciesCount % 8u)) - 1u);
			if ((auSeen[uByteCount - 1u] & (uint8_t)~uValidMask) != 0u
				|| (auCaught[uByteCount - 1u] & (uint8_t)~uValidMask) != 0u)
			{
				return Corrupt("Dex", "padding");
			}
		}
		for (u_int u = 0u; u < uByteCount; ++u)
		{
			if ((auCaught[u] & (uint8_t)~auSeen[u]) != 0u)
			{
				return Corrupt("Dex", "caughtImpliesSeen");
			}
		}
		for (u_int u = 0u; u < uSpeciesCount; ++u)
		{
			xState.m_xSeen.m_abFlags[u] = (auSeen[u / 8u] & (1u << (u % 8u))) != 0u;
			xState.m_xCaught.m_abFlags[u] = (auCaught[u / 8u] & (1u << (u % 8u))) != 0u;
		}
		return true;
	}

	Zenith_Status ParseStory(ZM_ByteReader& xReader, ZM_GameState& xState)
	{
		uint16_t uFlagCount = 0u;
		if (!xReader.U16(uFlagCount)) { return Corrupt("StoryFlags", "flagCountTruncated"); }
		if (uFlagCount > uZM_MAX_STORY_FLAGS) { return Corrupt("StoryFlags", "flagCount"); }
		const u_int uByteCount = ((u_int)uFlagCount + 7u) / 8u;
		if (!xReader.Bytes(xState.m_xStoryFlags.m_auFlags, uByteCount))
		{
			return Corrupt("StoryFlags", "flagsTruncated");
		}
		if (uByteCount > 0u && (uFlagCount % 8u) != 0u)
		{
			const uint8_t uValidMask = (uint8_t)((1u << (uFlagCount % 8u)) - 1u);
			if ((xState.m_xStoryFlags.m_auFlags[uByteCount - 1u]
				& (uint8_t)~uValidMask) != 0u)
			{
				return Corrupt("StoryFlags", "padding");
			}
		}
		return true;
	}

	Zenith_Status ParseBag(ZM_ByteReader& xReader, ZM_GameState& xState)
	{
		uint16_t uEntryCount = 0u;
		if (!xReader.U16(uEntryCount)) { return Corrupt("Bag", "entryCountTruncated"); }
		if (uEntryCount > uBAG_ENTRY_SANITY_CAP) { return Corrupt("Bag", "entryCount"); }
		uint16_t uPreviousId = 0u;
		for (u_int u = 0u; u < uEntryCount; ++u)
		{
			uint16_t uItemId = 0u;
			uint16_t uCount = 0u;
			if (!xReader.U16(uItemId) || !xReader.U16(uCount))
			{
				return Corrupt("Bag", "entryTruncated");
			}
			if (uItemId >= (uint16_t)ZM_ITEM_COUNT) { return Corrupt("Bag", "itemId"); }
			if (uCount == 0u || uCount > uZM_BAG_MAX_STACK_COUNT)
			{
				return Corrupt("Bag", "count");
			}
			if (u > 0u && uItemId <= uPreviousId) { return Corrupt("Bag", "ordering"); }
			uPreviousId = uItemId;
			if (!xState.m_xBag.Add((ZM_ITEM_ID)uItemId, uCount))
			{
				return Corrupt("Bag", "restore");
			}
		}
		return true;
	}

	Zenith_Status ParseDaycare(ZM_ByteReader& xReader, ZM_GameState& xState)
	{
		uint8_t uParentCount = 0u;
		if (!xReader.U8(uParentCount)) { return Corrupt("Daycare", "parentCountTruncated"); }
		if (uParentCount > uZM_DAYCARE_PARENT_CAPACITY) { return Corrupt("Daycare", "parentCount"); }
		xState.m_xDaycare.m_uParentCount = uParentCount;
		for (u_int u = 0u; u < uParentCount; ++u)
		{
			Zenith_Status xStatus = ReadMonster(xReader,
				xState.m_xDaycare.m_axParents[u], "Daycare");
			if (!xStatus.IsOk()) { return xStatus.Error(); }
		}
		uint8_t uEggPresent = 0u;
		if (!xReader.U8(uEggPresent)) { return Corrupt("Daycare", "eggPresentTruncated"); }
		if (uEggPresent > 1u) { return Corrupt("Daycare", "eggPresent"); }
		xState.m_xDaycare.m_bEggPresent = uEggPresent != 0u;
		if (xState.m_xDaycare.m_bEggPresent)
		{
			Zenith_Status xStatus = ReadMonster(xReader,
				xState.m_xDaycare.m_xEgg, "Daycare");
			if (!xStatus.IsOk()) { return xStatus.Error(); }
			if ((xState.m_xDaycare.m_xEgg.m_uFlags & uZM_MONSTER_FLAG_IS_EGG) == 0u)
			{
				return Corrupt("Daycare", "eggFlag");
			}
		}
		uint32_t uEggSteps = 0u;
		if (!xReader.U32(uEggSteps)) { return Corrupt("Daycare", "eggStepsTruncated"); }
		xState.m_xDaycare.m_uEggStepsRemaining = uEggSteps;
		if (!xState.m_xDaycare.m_bEggPresent && uEggSteps != 0u)
		{
			return Corrupt("Daycare", "eggStepsRemaining");
		}
		return true;
	}

	Zenith_Status ParseWorld(ZM_ByteReader& xReader, ZM_GameState& xState)
	{
		if (!xReader.U32(xState.m_xWorldPosition.m_uSceneBuildIndex)
			|| !xReader.Bytes(xState.m_xWorldPosition.m_szSpawnTag, uZM_WORLD_SPAWN_TAG_CAPACITY))
		{
			return Corrupt("WorldPos", "headerTruncated");
		}
		for (u_int u = 0u; u < 3u; ++u)
		{
			if (!xReader.Float(xState.m_xWorldPosition.m_afPosition[u]))
			{
				return Corrupt("WorldPos", "positionTruncated");
			}
		}
		if (!xReader.Float(xState.m_xWorldPosition.m_fYaw))
		{
			return Corrupt("WorldPos", "yawTruncated");
		}
		const char* szField = nullptr;
		if (!ValidateWorldPosition(xState.m_xWorldPosition, szField))
		{
			return Corrupt("WorldPos", szField);
		}
		return true;
	}

	Zenith_Status ParseOptions(ZM_ByteReader& xReader, ZM_GameState& xState)
	{
		uint16_t uFieldCount = 0u;
		if (!xReader.U16(uFieldCount)) { return Corrupt("Options", "fieldCountTruncated"); }
		bool bSawTextSpeed = false;
		for (u_int u = 0u; u < uFieldCount; ++u)
		{
			uint16_t uTag = 0u;
			uint16_t uLength = 0u;
			if (!xReader.U16(uTag) || !xReader.U16(uLength))
			{
				return Corrupt("Options", "tlvHeaderTruncated");
			}
			if (uTag == uOPTION_TAG_TEXT_SPEED)
			{
				if (bSawTextSpeed) { return Corrupt("Options", "duplicateTextSpeed"); }
				if (uLength != 1u) { return Corrupt("Options", "textSpeedLength"); }
				uint8_t uTextSpeed = 0u;
				if (!xReader.U8(uTextSpeed)) { return Corrupt("Options", "textSpeedTruncated"); }
				if (uTextSpeed >= (uint8_t)ZM_TEXT_SPEED_COUNT)
				{
					return Corrupt("Options", "textSpeed");
				}
				xState.m_xOptions.m_eTextSpeed = (ZM_TEXT_SPEED)uTextSpeed;
				bSawTextSpeed = true;
			}
			else if (!xReader.Skip(uLength))
			{
				return Corrupt("Options", "unknownValueTruncated");
			}
		}
		if (!bSawTextSpeed) { return Corrupt("Options", "missingTextSpeed"); }
		return true;
	}

	Zenith_Status ParseModule(uint32_t uModuleId, ZM_ByteReader& xReader,
		ZM_GameState& xState)
	{
		switch (uModuleId)
		{
		case uMODULE_PARTY: return ParseParty(xReader, xState);
		case uMODULE_BOXES: return ParseBoxes(xReader, xState);
		case uMODULE_DEX: return ParseDex(xReader, xState);
		case uMODULE_STORY: return ParseStory(xReader, xState);
		case uMODULE_BADGES:
			if (!xReader.U8(xState.m_uBadgeMask)) { return Corrupt("Badges", "maskTruncated"); }
			return true;
		case uMODULE_BAG: return ParseBag(xReader, xState);
		case uMODULE_MONEY:
			if (!xReader.U32(xState.m_uMoney)) { return Corrupt("Money", "valueTruncated"); }
			return true;
		case uMODULE_DAYCARE: return ParseDaycare(xReader, xState);
		case uMODULE_TOWER:
			if (!xReader.U32(xState.m_xTowerRun.m_uCurrentStreak)
				|| !xReader.U32(xState.m_xTowerRun.m_uBestStreak)
				|| !xReader.U64(xState.m_xTowerRun.m_ulSeed))
			{
				return Corrupt("Tower", "payloadTruncated");
			}
			return true;
		case uMODULE_WORLD: return ParseWorld(xReader, xState);
		case uMODULE_OPTIONS: return ParseOptions(xReader, xState);
		default: return Corrupt("Header", "moduleId");
		}
	}

	void WriteModules(ZM_ByteWriter& xWriter, const ZM_GameState& xState)
	{
		uint64_t ulHeader = xWriter.BeginModule(uMODULE_PARTY);
		xWriter.U8((uint8_t)xState.m_xParty.Count());
		for (u_int u = 0u; u < xState.m_xParty.Count(); ++u)
		{
			WriteMonster(xWriter, xState.m_xParty.Get(u));
		}
		xWriter.EndModule(ulHeader);

		ulHeader = xWriter.BeginModule(uMODULE_BOXES);
		xWriter.U8((uint8_t)uZM_BOX_COUNT);
		xWriter.U8((uint8_t)uZM_BOX_SLOTS_PER_BOX);
		for (u_int uBox = 0u; uBox < uZM_BOX_COUNT; ++uBox)
		{
			for (u_int uSlot = 0u; uSlot < uZM_BOX_SLOTS_PER_BOX; ++uSlot)
			{
				const ZM_Monster* pxMonster = xState.m_xBoxes.TryGet(uBox, uSlot);
				xWriter.U8((uint8_t)(pxMonster == nullptr ? 0u : 1u));
				if (pxMonster != nullptr) { WriteMonster(xWriter, *pxMonster); }
			}
		}
		xWriter.EndModule(ulHeader);

		ulHeader = xWriter.BeginModule(uMODULE_DEX);
		xWriter.U16((uint16_t)ZM_SPECIES_COUNT);
		const u_int uDexBytes = ((u_int)ZM_SPECIES_COUNT + 7u) / 8u;
		uint8_t auBits[uDEX_SANITY_CAP / 8u] = {};
		for (u_int u = 0u; u < (u_int)ZM_SPECIES_COUNT; ++u)
		{
			if (xState.m_xSeen.m_abFlags[u]) { auBits[u / 8u] |= (uint8_t)(1u << (u % 8u)); }
		}
		xWriter.Bytes(auBits, uDexBytes);
		memset(auBits, 0, sizeof(auBits));
		for (u_int u = 0u; u < (u_int)ZM_SPECIES_COUNT; ++u)
		{
			if (xState.m_xCaught.m_abFlags[u]) { auBits[u / 8u] |= (uint8_t)(1u << (u % 8u)); }
		}
		xWriter.Bytes(auBits, uDexBytes);
		xWriter.EndModule(ulHeader);

		ulHeader = xWriter.BeginModule(uMODULE_STORY);
		u_int uStoryCount = 0u;
		for (u_int u = uZM_MAX_STORY_FLAGS; u > 0u; --u)
		{
			if (xState.m_xStoryFlags.IsSet(u - 1u))
			{
				uStoryCount = u;
				break;
			}
		}
		xWriter.U16((uint16_t)uStoryCount);
		xWriter.Bytes(xState.m_xStoryFlags.m_auFlags, (uStoryCount + 7u) / 8u);
		xWriter.EndModule(ulHeader);

		ulHeader = xWriter.BeginModule(uMODULE_BADGES);
		xWriter.U8(xState.m_uBadgeMask);
		xWriter.EndModule(ulHeader);

		ulHeader = xWriter.BeginModule(uMODULE_BAG);
		xWriter.U16((uint16_t)xState.m_xBag.TotalStackCount());
		for (u_int u = 0u; u < (u_int)ZM_ITEM_COUNT; ++u)
		{
			const u_int uCount = xState.m_xBag.GetCount((ZM_ITEM_ID)u);
			if (uCount == 0u) { continue; }
			xWriter.U16((uint16_t)u);
			xWriter.U16((uint16_t)uCount);
		}
		xWriter.EndModule(ulHeader);

		ulHeader = xWriter.BeginModule(uMODULE_MONEY);
		xWriter.U32(xState.m_uMoney);
		xWriter.EndModule(ulHeader);

		ulHeader = xWriter.BeginModule(uMODULE_DAYCARE);
		xWriter.U8((uint8_t)xState.m_xDaycare.m_uParentCount);
		for (u_int u = 0u; u < xState.m_xDaycare.m_uParentCount; ++u)
		{
			WriteMonster(xWriter, xState.m_xDaycare.m_axParents[u]);
		}
		xWriter.U8((uint8_t)(xState.m_xDaycare.m_bEggPresent ? 1u : 0u));
		if (xState.m_xDaycare.m_bEggPresent) { WriteMonster(xWriter, xState.m_xDaycare.m_xEgg); }
		xWriter.U32(xState.m_xDaycare.m_uEggStepsRemaining);
		xWriter.EndModule(ulHeader);

		ulHeader = xWriter.BeginModule(uMODULE_TOWER);
		xWriter.U32(xState.m_xTowerRun.m_uCurrentStreak);
		xWriter.U32(xState.m_xTowerRun.m_uBestStreak);
		xWriter.U64(xState.m_xTowerRun.m_ulSeed);
		xWriter.EndModule(ulHeader);

		ulHeader = xWriter.BeginModule(uMODULE_WORLD);
		xWriter.U32(xState.m_xWorldPosition.m_uSceneBuildIndex);
		xWriter.Bytes(xState.m_xWorldPosition.m_szSpawnTag, uZM_WORLD_SPAWN_TAG_CAPACITY);
		for (u_int u = 0u; u < 3u; ++u) { xWriter.Float(xState.m_xWorldPosition.m_afPosition[u]); }
		xWriter.Float(xState.m_xWorldPosition.m_fYaw);
		xWriter.EndModule(ulHeader);

		ulHeader = xWriter.BeginModule(uMODULE_OPTIONS);
		xWriter.U16(1u);
		xWriter.U16(uOPTION_TAG_TEXT_SPEED);
		xWriter.U16(1u);
		xWriter.U8((uint8_t)xState.m_xOptions.m_eTextSpeed);
		xWriter.EndModule(ulHeader);
	}
}

Zenith_Status ZM_SaveSchema::Write(const ZM_GameState& xState,
	Zenith_DataStream& xOutStream)
{
	if (xOutStream.GetData() == nullptr
		|| xOutStream.GetCursor() > xOutStream.GetCapacity())
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY, "[ZM Save] invalid output stream");
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	const char* szModule = nullptr;
	const char* szField = nullptr;
	if (!ValidateState(xState, szModule, szField))
	{
		return InvalidSource(szModule, szField);
	}

	ZM_ByteWriter xWriter;
	xWriter.U32(uMAGIC);
	xWriter.U32(uSCHEMA_VERSION_CURRENT);
	xWriter.U32(uMODULE_COUNT);
	WriteModules(xWriter, xState);
	if (!xWriter.IsOk()) { return Zenith_ErrorCode::OUT_OF_MEMORY; }

	const uint64_t ulSize = xWriter.Cursor();
	const uint64_t ulBefore = xOutStream.GetCursor();
	if (ulBefore > (std::numeric_limits<uint64_t>::max)() - ulSize)
	{
		return Zenith_ErrorCode::OUT_OF_MEMORY;
	}
	if (!xOutStream.OwnsData() && ulSize > xOutStream.GetCapacity() - ulBefore)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM Save] external output stream has %llu bytes remaining; %llu required",
			xOutStream.GetCapacity() - ulBefore, ulSize);
		return Zenith_ErrorCode::OUT_OF_MEMORY;
	}
	xOutStream.WriteData(xWriter.Stream().GetData(), ulSize);
	if (xOutStream.GetCursor() != ulBefore + ulSize)
	{
		return Zenith_ErrorCode::OUT_OF_MEMORY;
	}
	return true;
}

Zenith_Status ZM_SaveSchema::Read(Zenith_DataStream& xInStream,
	uint64_t ulByteLength, ZM_GameState& xOutState)
{
	const uint64_t ulEntryCursor = xInStream.GetCursor();
	if (ulEntryCursor > xInStream.GetCapacity()
		|| ulByteLength > xInStream.GetCapacity() - ulEntryCursor
		|| ulByteLength == 0u || xInStream.GetData() == nullptr)
	{
		return Corrupt("Header", "payloadLength");
	}
	const uint8_t* pPayload = static_cast<const uint8_t*>(xInStream.GetData()) + ulEntryCursor;
	ZM_ByteReader xReader(pPayload, ulByteLength);

	uint32_t uMagic = 0u;
	if (!xReader.U32(uMagic)) { return Corrupt("Header", "magicTruncated"); }
	if (uMagic != uMAGIC)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY, "[ZM Save] bad inner magic 0x%08X", uMagic);
		return Zenith_ErrorCode::BAD_MAGIC;
	}
	uint32_t uSchemaVersion = 0u;
	if (!xReader.U32(uSchemaVersion)) { return Corrupt("Header", "schemaVersionTruncated"); }
	if (uSchemaVersion != uSCHEMA_VERSION_CURRENT)
	{
		Zenith_Error(LOG_CATEGORY_GAMEPLAY,
			"[ZM Save] schema version %u is unsupported (current %u)",
			uSchemaVersion, uSCHEMA_VERSION_CURRENT);
		return Zenith_ErrorCode::VERSION_MISMATCH;
	}
	uint32_t uModuleCount = 0u;
	if (!xReader.U32(uModuleCount)) { return Corrupt("Header", "moduleCountTruncated"); }
	if (uModuleCount != uMODULE_COUNT) { return Corrupt("Header", "moduleCount"); }

	ZM_GameState xCandidate;
	xCandidate.m_bPendingWhiteout = false;
	for (uint32_t uExpectedId = 1u; uExpectedId <= uMODULE_COUNT; ++uExpectedId)
	{
		uint32_t uModuleId = 0u;
		uint32_t uModuleVersion = 0u;
		uint32_t uModuleLength = 0u;
		if (!xReader.U32(uModuleId) || !xReader.U32(uModuleVersion)
			|| !xReader.U32(uModuleLength))
		{
			return Corrupt("Header", "moduleHeaderTruncated");
		}
		if (uModuleId != uExpectedId) { return Corrupt("Header", "moduleOrder"); }
		if (uModuleVersion != uMODULE_VERSION_CURRENT)
		{
			Zenith_Error(LOG_CATEGORY_GAMEPLAY,
				"[ZM Save] module %u version %u is unsupported (current %u)",
				uModuleId, uModuleVersion, uMODULE_VERSION_CURRENT);
			return Zenith_ErrorCode::VERSION_MISMATCH;
		}
		ZM_ByteReader xModuleReader;
		if (!xReader.Take(uModuleLength, xModuleReader))
		{
			return Corrupt("Header", "moduleLength");
		}
		Zenith_Status xStatus = ParseModule(uModuleId, xModuleReader, xCandidate);
		if (!xStatus.IsOk()) { return xStatus.Error(); }
		if (!xModuleReader.IsAtEnd()) { return Corrupt("Header", "moduleLengthExact"); }
	}
	if (!xReader.IsAtEnd()) { return Corrupt("Header", "trailingBytes"); }

	const char* szModule = nullptr;
	const char* szField = nullptr;
	if (!ValidateState(xCandidate, szModule, szField))
	{
		return Corrupt(szModule, szField);
	}
	xOutState = xCandidate;
	xInStream.SetCursor(ulEntryCursor + ulByteLength);
	return true;
}
