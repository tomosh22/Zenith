#pragma once
/**
 * Survival_Inventory.h - Item storage and management
 *
 * Manages player's inventory with item counts for each type.
 * Fires events when inventory changes.
 *
 * Features:
 * - Add/remove items with validation
 * - Check if player has enough for crafting
 * - Event dispatch on changes
 */

#include "Survival_EventBus.h"
#include <cstdint>
#include <array>

class Survival_Inventory
{
public:
	static constexpr uint32_t s_uMaxStack = 99;

	Survival_Inventory()
	{
		Reset();
	}

	/**
	 * Reset - Clear all items
	 */
	void Reset()
	{
		m_auItemCounts.fill(0);
	}

	/**
	 * GetCount - Get current count of an item type
	 */
	uint32_t GetCount(SurvivalItemType eType) const
	{
		if (eType >= ITEM_TYPE_COUNT || eType == ITEM_TYPE_NONE)
			return 0;
		return m_auItemCounts[eType];
	}

	/**
	 * AddItem - Add items to inventory
	 * @return Actual amount added (may be less if hitting max stack)
	 */
	uint32_t AddItem(SurvivalItemType eType, uint32_t uAmount)
	{
		if (eType >= ITEM_TYPE_COUNT || eType == ITEM_TYPE_NONE || uAmount == 0)
			return 0;

		uint32_t uCurrent = m_auItemCounts[eType];
		uint32_t uCanAdd = s_uMaxStack - uCurrent;
		uint32_t uActualAdded = (uAmount < uCanAdd) ? uAmount : uCanAdd;

		m_auItemCounts[eType] = uCurrent + uActualAdded;

		// Fire inventory changed event
		if (uActualAdded > 0)
		{
			Survival_EventBus::Dispatch(Survival_Event_InventoryChanged{
				eType,
				static_cast<int32_t>(uActualAdded),
				m_auItemCounts[eType]
			});
		}

		return uActualAdded;
	}

	/**
	 * RemoveItem - Remove items from inventory
	 * @return true if successfully removed, false if not enough
	 */
	bool RemoveItem(SurvivalItemType eType, uint32_t uAmount)
	{
		if (eType >= ITEM_TYPE_COUNT || eType == ITEM_TYPE_NONE || uAmount == 0)
			return false;

		if (m_auItemCounts[eType] < uAmount)
			return false;

		m_auItemCounts[eType] -= uAmount;

		// Fire inventory changed event
		Survival_EventBus::Dispatch(Survival_Event_InventoryChanged{
			eType,
			-static_cast<int32_t>(uAmount),
			m_auItemCounts[eType]
		});

		return true;
	}

	/**
	 * HasItems - Check if inventory has at least this many items
	 */
	bool HasItems(SurvivalItemType eType, uint32_t uAmount) const
	{
		if (eType >= ITEM_TYPE_COUNT || eType == ITEM_TYPE_NONE)
			return false;
		return m_auItemCounts[eType] >= uAmount;
	}

	/**
	 * HasTool - Check if player has a specific tool
	 */
	bool HasAxe() const
	{
		return m_auItemCounts[ITEM_TYPE_AXE] > 0;
	}

	bool HasPickaxe() const
	{
		return m_auItemCounts[ITEM_TYPE_PICKAXE] > 0;
	}

	/**
	 * GetWood/Stone/Berries - Convenience accessors
	 */
	uint32_t GetWood() const { return m_auItemCounts[ITEM_TYPE_WOOD]; }
	uint32_t GetStone() const { return m_auItemCounts[ITEM_TYPE_STONE]; }
	uint32_t GetBerries() const { return m_auItemCounts[ITEM_TYPE_BERRIES]; }
	uint32_t GetAxeCount() const { return m_auItemCounts[ITEM_TYPE_AXE]; }
	uint32_t GetPickaxeCount() const { return m_auItemCounts[ITEM_TYPE_PICKAXE]; }

	/**
	 * GetTotalItems - Get total number of items across all types
	 */
	uint32_t GetTotalItems() const
	{
		uint32_t uTotal = 0;
		for (uint32_t i = 1; i < ITEM_TYPE_COUNT; i++)
		{
			uTotal += m_auItemCounts[i];
		}
		return uTotal;
	}

	/**
	 * CanCraftAxe - Check if player has materials for axe
	 */
	bool CanCraftAxe(uint32_t uWoodCost, uint32_t uStoneCost) const
	{
		return GetWood() >= uWoodCost && GetStone() >= uStoneCost;
	}

	/**
	 * CanCraftPickaxe - Check if player has materials for pickaxe
	 */
	bool CanCraftPickaxe(uint32_t uWoodCost, uint32_t uStoneCost) const
	{
		return GetWood() >= uWoodCost && GetStone() >= uStoneCost;
	}

private:
	std::array<uint32_t, ITEM_TYPE_COUNT> m_auItemCounts;
};
