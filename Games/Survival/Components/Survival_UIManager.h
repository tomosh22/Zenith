#pragma once
/**
 * Survival_UIManager.h - UI management for inventory and crafting
 *
 * Demonstrates:
 * - Dynamic text updates via Zenith_UIText
 * - Finding UI elements by name
 * - Formatting text with snprintf
 * - Color changes based on state
 *
 * UI Elements (set up in Survival.cpp):
 * - "WoodCount", "StoneCount", "BerriesCount" - Resource counts
 * - "AxeCount", "PickaxeCount" - Tool counts
 * - "InteractPrompt" - Contextual interaction text
 * - "CraftProgress" - Crafting progress bar
 * - "Status" - Game status messages
 */

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIText.h"
#include "Maths/Zenith_Maths.h"
#include "Survival_Inventory.h"
#include "Survival_CraftingSystem.h"
#include "Survival_ResourceNode.h"
#include <cstdio>

/**
 * Survival_UIManager - HUD text management
 */
class Survival_UIManager
{
public:
	/**
	 * UpdateInventoryUI - Update all inventory displays
	 */
	static void UpdateInventoryUI(
		Zenith_UIComponent& xUI,
		const Survival_Inventory& xInventory)
	{
		char acBuffer[64];

		// Wood
		Zenith_UI::Zenith_UIText* pxWood = xUI.FindElement<Zenith_UI::Zenith_UIText>("WoodCount");
		if (pxWood)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Wood: %u", xInventory.GetWood());
			pxWood->SetText(acBuffer);
		}

		// Stone
		Zenith_UI::Zenith_UIText* pxStone = xUI.FindElement<Zenith_UI::Zenith_UIText>("StoneCount");
		if (pxStone)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Stone: %u", xInventory.GetStone());
			pxStone->SetText(acBuffer);
		}

		// Berries
		Zenith_UI::Zenith_UIText* pxBerries = xUI.FindElement<Zenith_UI::Zenith_UIText>("BerriesCount");
		if (pxBerries)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Berries: %u", xInventory.GetBerries());
			pxBerries->SetText(acBuffer);
		}

		// Axe
		Zenith_UI::Zenith_UIText* pxAxe = xUI.FindElement<Zenith_UI::Zenith_UIText>("AxeCount");
		if (pxAxe)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Axe: %u", xInventory.GetAxeCount());
			pxAxe->SetText(acBuffer);

			// Highlight if have axe
			if (xInventory.HasAxe())
				pxAxe->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));
			else
				pxAxe->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));
		}

		// Pickaxe
		Zenith_UI::Zenith_UIText* pxPickaxe = xUI.FindElement<Zenith_UI::Zenith_UIText>("PickaxeCount");
		if (pxPickaxe)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Pickaxe: %u", xInventory.GetPickaxeCount());
			pxPickaxe->SetText(acBuffer);

			if (xInventory.HasPickaxe())
				pxPickaxe->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f));
			else
				pxPickaxe->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.f, 1.f));
		}
	}

	/**
	 * UpdateInteractionPrompt - Show contextual interaction text
	 */
	static void UpdateInteractionPrompt(
		Zenith_UIComponent& xUI,
		const Survival_ResourceNodeData* pxNearestNode,
		bool bCanInteract)
	{
		Zenith_UI::Zenith_UIText* pxPrompt = xUI.FindElement<Zenith_UI::Zenith_UIText>("InteractPrompt");
		if (!pxPrompt)
			return;

		if (!pxNearestNode || !bCanInteract)
		{
			pxPrompt->SetText("");
			return;
		}

		char acBuffer[128];
		const char* szResourceName = GetResourceName(pxNearestNode->m_eResourceType);
		uint32_t uHitsRemaining = pxNearestNode->m_uCurrentHits;

		if (pxNearestNode->m_bDepleted)
		{
			float fRespawnPercent = pxNearestNode->GetRespawnProgress() * 100.f;
			snprintf(acBuffer, sizeof(acBuffer), "%s (Respawning: %.0f%%)", szResourceName, fRespawnPercent);
			pxPrompt->SetColor(Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 1.f));
		}
		else
		{
			snprintf(acBuffer, sizeof(acBuffer), "[E] Harvest %s (%u hits left)", szResourceName, uHitsRemaining);
			pxPrompt->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 0.6f, 1.f));
		}

		pxPrompt->SetText(acBuffer);
	}

	/**
	 * UpdateCraftingUI - Update crafting progress display
	 */
	static void UpdateCraftingUI(
		Zenith_UIComponent& xUI,
		const Survival_CraftingSystem& xCrafting,
		const Survival_Inventory& xInventory)
	{
		Zenith_UI::Zenith_UIText* pxProgress = xUI.FindElement<Zenith_UI::Zenith_UIText>("CraftProgress");
		if (!pxProgress)
			return;

		char acBuffer[128];

		if (xCrafting.IsCrafting())
		{
			// Show crafting progress
			const char* szItemName = GetItemName(xCrafting.GetCurrentCrafting());
			float fPercent = xCrafting.GetProgress() * 100.f;

			// Create progress bar
			int uBarLength = 20;
			int uFilled = static_cast<int>(xCrafting.GetProgress() * uBarLength);

			char acBar[32];
			for (int i = 0; i < uBarLength; i++)
			{
				acBar[i] = (i < uFilled) ? '#' : '-';
			}
			acBar[uBarLength] = '\0';

			snprintf(acBuffer, sizeof(acBuffer), "Crafting %s [%s] %.0f%%", szItemName, acBar, fPercent);
			pxProgress->SetText(acBuffer);
			pxProgress->SetColor(Zenith_Maths::Vector4(0.6f, 1.f, 0.6f, 1.f));
		}
		else
		{
			// Show crafting hints
			const CraftingRecipe& xAxe = xCrafting.GetAxeRecipe();
			const CraftingRecipe& xPickaxe = xCrafting.GetPickaxeRecipe();

			bool bCanAxe = xCrafting.CanCraft(ITEM_TYPE_AXE, xInventory);
			bool bCanPickaxe = xCrafting.CanCraft(ITEM_TYPE_PICKAXE, xInventory);

			if (bCanAxe || bCanPickaxe)
			{
				std::string strHint;
				if (bCanAxe)
					strHint += "[1] Craft Axe  ";
				if (bCanPickaxe)
					strHint += "[2] Craft Pickaxe";

				pxProgress->SetText(strHint.c_str());
				pxProgress->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.2f, 1.f));
			}
			else
			{
				// Show recipe requirements
				snprintf(acBuffer, sizeof(acBuffer),
					"Axe: %uW %uS | Pickaxe: %uW %uS",
					xAxe.m_uWoodCost, xAxe.m_uStoneCost,
					xPickaxe.m_uWoodCost, xPickaxe.m_uStoneCost);
				pxProgress->SetText(acBuffer);
				pxProgress->SetColor(Zenith_Maths::Vector4(0.6f, 0.6f, 0.6f, 1.f));
			}
		}
	}

	/**
	 * ShowStatusMessage - Display a centered status message
	 */
	static void ShowStatusMessage(
		Zenith_UIComponent& xUI,
		const char* szMessage,
		const Zenith_Maths::Vector4& xColor = Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f))
	{
		Zenith_UI::Zenith_UIText* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status");
		if (pxStatus)
		{
			pxStatus->SetText(szMessage);
			pxStatus->SetColor(xColor);
		}
	}

	/**
	 * ClearStatusMessage - Clear the status message
	 */
	static void ClearStatusMessage(Zenith_UIComponent& xUI)
	{
		Zenith_UI::Zenith_UIText* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status");
		if (pxStatus)
		{
			pxStatus->SetText("");
		}
	}

	/**
	 * ShowHarvestFeedback - Show feedback when harvesting
	 */
	static void ShowHarvestFeedback(
		Zenith_UIComponent& xUI,
		SurvivalItemType eItemType,
		uint32_t uAmount)
	{
		char acBuffer[64];
		snprintf(acBuffer, sizeof(acBuffer), "+%u %s", uAmount, GetItemName(eItemType));
		ShowStatusMessage(xUI, acBuffer, Zenith_Maths::Vector4(0.8f, 1.f, 0.4f, 1.f));
	}

	/**
	 * ShowCraftingComplete - Show crafting completion message
	 */
	static void ShowCraftingComplete(
		Zenith_UIComponent& xUI,
		SurvivalItemType eItemType)
	{
		char acBuffer[64];
		snprintf(acBuffer, sizeof(acBuffer), "Crafted: %s!", GetItemName(eItemType));
		ShowStatusMessage(xUI, acBuffer, Zenith_Maths::Vector4(0.2f, 1.f, 0.6f, 1.f));
	}

	/**
	 * ShowNotEnoughMaterials - Show "not enough materials" message
	 */
	static void ShowNotEnoughMaterials(Zenith_UIComponent& xUI)
	{
		ShowStatusMessage(xUI, "Not enough materials!", Zenith_Maths::Vector4(1.f, 0.4f, 0.4f, 1.f));
	}

	/**
	 * UpdateAllUI - Update all UI elements
	 */
	static void UpdateAllUI(
		Zenith_UIComponent& xUI,
		const Survival_Inventory& xInventory,
		const Survival_CraftingSystem& xCrafting,
		const Survival_ResourceNodeData* pxNearestNode,
		bool bCanInteract)
	{
		UpdateInventoryUI(xUI, xInventory);
		UpdateInteractionPrompt(xUI, pxNearestNode, bCanInteract);
		UpdateCraftingUI(xUI, xCrafting, xInventory);
	}
};
