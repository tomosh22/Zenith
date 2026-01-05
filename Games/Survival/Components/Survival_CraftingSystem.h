#pragma once
/**
 * Survival_CraftingSystem.h - Recipe processing and crafting
 *
 * Manages crafting recipes and validates crafting operations.
 * Works with the TaskProcessor for asynchronous crafting.
 *
 * Features:
 * - Recipe definitions
 * - Material cost checking
 * - Crafting state tracking
 * - Event dispatch for crafting progress
 */

#include "Survival_EventBus.h"
#include "Survival_Inventory.h"
#include <cstdint>

/**
 * CraftingRecipe - Definition of a craftable item
 */
struct CraftingRecipe
{
	SurvivalItemType m_eOutputType = ITEM_TYPE_NONE;
	uint32_t m_uOutputAmount = 1;

	// Input costs (using item types)
	uint32_t m_uWoodCost = 0;
	uint32_t m_uStoneCost = 0;
	uint32_t m_uBerryCost = 0;

	float m_fCraftTime = 2.0f;  // Seconds to craft
};

/**
 * CraftingState - Current crafting operation state
 */
enum class CraftingState
{
	IDLE,
	CRAFTING,
	COMPLETE
};

/**
 * Survival_CraftingSystem - Manages crafting operations
 */
class Survival_CraftingSystem
{
public:
	Survival_CraftingSystem()
		: m_eCraftingState(CraftingState::IDLE)
		, m_eCurrentCrafting(ITEM_TYPE_NONE)
		, m_fCraftingProgress(0.f)
		, m_fCraftingDuration(0.f)
	{
		InitializeRecipes();
	}

	/**
	 * InitializeRecipes - Set up default recipes
	 */
	void InitializeRecipes()
	{
		// Axe: 3 Wood + 2 Stone
		m_xAxeRecipe.m_eOutputType = ITEM_TYPE_AXE;
		m_xAxeRecipe.m_uOutputAmount = 1;
		m_xAxeRecipe.m_uWoodCost = 3;
		m_xAxeRecipe.m_uStoneCost = 2;
		m_xAxeRecipe.m_fCraftTime = 2.0f;

		// Pickaxe: 2 Wood + 3 Stone
		m_xPickaxeRecipe.m_eOutputType = ITEM_TYPE_PICKAXE;
		m_xPickaxeRecipe.m_uOutputAmount = 1;
		m_xPickaxeRecipe.m_uWoodCost = 2;
		m_xPickaxeRecipe.m_uStoneCost = 3;
		m_xPickaxeRecipe.m_fCraftTime = 2.0f;
	}

	/**
	 * SetRecipeCosts - Update recipe costs from config
	 */
	void SetRecipeCosts(
		uint32_t uAxeWood, uint32_t uAxeStone,
		uint32_t uPickaxeWood, uint32_t uPickaxeStone,
		float fCraftTime)
	{
		m_xAxeRecipe.m_uWoodCost = uAxeWood;
		m_xAxeRecipe.m_uStoneCost = uAxeStone;
		m_xAxeRecipe.m_fCraftTime = fCraftTime;

		m_xPickaxeRecipe.m_uWoodCost = uPickaxeWood;
		m_xPickaxeRecipe.m_uStoneCost = uPickaxeStone;
		m_xPickaxeRecipe.m_fCraftTime = fCraftTime;
	}

	/**
	 * CanCraft - Check if player has materials for a recipe
	 */
	bool CanCraft(SurvivalItemType eType, const Survival_Inventory& xInventory) const
	{
		const CraftingRecipe* pRecipe = GetRecipe(eType);
		if (!pRecipe)
			return false;

		return xInventory.HasItems(ITEM_TYPE_WOOD, pRecipe->m_uWoodCost) &&
			   xInventory.HasItems(ITEM_TYPE_STONE, pRecipe->m_uStoneCost) &&
			   xInventory.HasItems(ITEM_TYPE_BERRIES, pRecipe->m_uBerryCost);
	}

	/**
	 * StartCrafting - Begin crafting an item
	 * @return true if crafting started, false if already crafting or can't afford
	 */
	bool StartCrafting(SurvivalItemType eType, Survival_Inventory& xInventory)
	{
		if (m_eCraftingState != CraftingState::IDLE)
			return false;

		if (!CanCraft(eType, xInventory))
			return false;

		const CraftingRecipe* pRecipe = GetRecipe(eType);
		if (!pRecipe)
			return false;

		// Consume materials
		xInventory.RemoveItem(ITEM_TYPE_WOOD, pRecipe->m_uWoodCost);
		xInventory.RemoveItem(ITEM_TYPE_STONE, pRecipe->m_uStoneCost);
		xInventory.RemoveItem(ITEM_TYPE_BERRIES, pRecipe->m_uBerryCost);

		// Start crafting
		m_eCraftingState = CraftingState::CRAFTING;
		m_eCurrentCrafting = eType;
		m_fCraftingProgress = 0.f;
		m_fCraftingDuration = pRecipe->m_fCraftTime;

		// Dispatch event
		Survival_EventBus::Dispatch(Survival_Event_CraftingStarted{
			eType,
			m_fCraftingDuration
		});

		return true;
	}

	/**
	 * Update - Update crafting progress
	 * @return Item type if crafting completed, ITEM_TYPE_NONE otherwise
	 */
	SurvivalItemType Update(float fDt)
	{
		if (m_eCraftingState != CraftingState::CRAFTING)
			return ITEM_TYPE_NONE;

		m_fCraftingProgress += fDt;

		// Dispatch progress event
		float fProgressPercent = m_fCraftingProgress / m_fCraftingDuration;
		if (fProgressPercent > 1.f)
			fProgressPercent = 1.f;

		Survival_EventBus::Dispatch(Survival_Event_CraftingProgress{
			m_eCurrentCrafting,
			fProgressPercent
		});

		// Check completion
		if (m_fCraftingProgress >= m_fCraftingDuration)
		{
			SurvivalItemType eCompleted = m_eCurrentCrafting;

			m_eCraftingState = CraftingState::COMPLETE;

			// Dispatch completion event (can be used by task processor)
			Survival_EventBus::Dispatch(Survival_Event_CraftingComplete{
				eCompleted,
				true
			});

			return eCompleted;
		}

		return ITEM_TYPE_NONE;
	}

	/**
	 * CollectCraftedItem - Collect the finished item and reset state
	 * @return true if item was collected
	 */
	bool CollectCraftedItem(Survival_Inventory& xInventory)
	{
		if (m_eCraftingState != CraftingState::COMPLETE)
			return false;

		const CraftingRecipe* pRecipe = GetRecipe(m_eCurrentCrafting);
		if (pRecipe)
		{
			xInventory.AddItem(m_eCurrentCrafting, pRecipe->m_uOutputAmount);
		}

		// Reset state
		m_eCraftingState = CraftingState::IDLE;
		m_eCurrentCrafting = ITEM_TYPE_NONE;
		m_fCraftingProgress = 0.f;
		m_fCraftingDuration = 0.f;

		return true;
	}

	/**
	 * CancelCrafting - Cancel current crafting (materials lost)
	 */
	void CancelCrafting()
	{
		m_eCraftingState = CraftingState::IDLE;
		m_eCurrentCrafting = ITEM_TYPE_NONE;
		m_fCraftingProgress = 0.f;
		m_fCraftingDuration = 0.f;
	}

	/**
	 * GetState - Get current crafting state
	 */
	CraftingState GetState() const { return m_eCraftingState; }

	/**
	 * GetCurrentCrafting - Get item being crafted
	 */
	SurvivalItemType GetCurrentCrafting() const { return m_eCurrentCrafting; }

	/**
	 * GetProgress - Get crafting progress (0.0 to 1.0)
	 */
	float GetProgress() const
	{
		if (m_fCraftingDuration <= 0.f)
			return 0.f;
		return m_fCraftingProgress / m_fCraftingDuration;
	}

	/**
	 * IsCrafting - Check if currently crafting
	 */
	bool IsCrafting() const { return m_eCraftingState == CraftingState::CRAFTING; }

	/**
	 * GetRecipe - Get recipe for an item type
	 */
	const CraftingRecipe* GetRecipe(SurvivalItemType eType) const
	{
		switch (eType)
		{
		case ITEM_TYPE_AXE:
			return &m_xAxeRecipe;
		case ITEM_TYPE_PICKAXE:
			return &m_xPickaxeRecipe;
		default:
			return nullptr;
		}
	}

	/**
	 * GetAxeRecipe/GetPickaxeRecipe - Direct recipe access
	 */
	const CraftingRecipe& GetAxeRecipe() const { return m_xAxeRecipe; }
	const CraftingRecipe& GetPickaxeRecipe() const { return m_xPickaxeRecipe; }

private:
	CraftingRecipe m_xAxeRecipe;
	CraftingRecipe m_xPickaxeRecipe;

	CraftingState m_eCraftingState;
	SurvivalItemType m_eCurrentCrafting;
	float m_fCraftingProgress;
	float m_fCraftingDuration;
};
