#pragma once
/**
 * Sokoban_UIManager.h - HUD text management
 *
 * Demonstrates:
 * - Zenith_UIComponent for UI element containers
 * - Zenith_UIText for text elements
 * - Finding elements by name
 * - Dynamic text updates
 *
 * Key concepts:
 * - UI elements are attached to entities via Zenith_UIComponent
 * - Elements can be found by name using FindElement<T>()
 * - Text updates use SetText() method
 * - Anchor/pivot system for positioning (TopRight, Center, etc.)
 */

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIText.h"
#include <cstdio>
#include <cstdint>

/**
 * Sokoban_UIManager - Manages HUD text elements
 *
 * Expected UI element names (set up in Sokoban.cpp):
 * - "Status"    - Shows move count
 * - "Progress"  - Shows boxes on targets
 * - "MinMoves"  - Shows minimum moves needed
 * - "WinText"   - Shows victory message
 */
class Sokoban_UIManager
{
public:
	/**
	 * UpdateStatusText - Update all HUD text elements
	 *
	 * @param xUIComponent  Reference to the entity's UI component
	 * @param uMoveCount    Current number of moves
	 * @param uBoxesOnTargets Number of boxes currently on targets
	 * @param uTargetCount  Total number of targets
	 * @param uMinMoves     Minimum moves to solve (0 if unknown)
	 * @param bWon          Whether the level is complete
	 */
	static void UpdateStatusText(
		Zenith_UIComponent& xUIComponent,
		uint32_t uMoveCount,
		uint32_t uBoxesOnTargets,
		uint32_t uTargetCount,
		uint32_t uMinMoves,
		bool bWon)
	{
		char acBuffer[64];

		// Update move counter
		// FindElement<T>() searches the UI component's element hierarchy by name
		Zenith_UI::Zenith_UIText* pxStatus = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("Status");
		if (pxStatus)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Moves: %u", uMoveCount);
			pxStatus->SetText(acBuffer);
		}

		// Update progress
		Zenith_UI::Zenith_UIText* pxProgress = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("Progress");
		if (pxProgress)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Boxes: %u / %u", uBoxesOnTargets, uTargetCount);
			pxProgress->SetText(acBuffer);
		}

		// Update minimum moves hint
		Zenith_UI::Zenith_UIText* pxMinMoves = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("MinMoves");
		if (pxMinMoves)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Min Moves: %u", uMinMoves);
			pxMinMoves->SetText(acBuffer);
		}

		// Update win text (visible only when won)
		Zenith_UI::Zenith_UIText* pxWin = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("WinText");
		if (pxWin)
		{
			pxWin->SetText(bWon ? "LEVEL COMPLETE!" : "");
		}
	}

	/**
	 * SetWinText - Show or hide the victory message
	 */
	static void SetWinText(Zenith_UIComponent& xUIComponent, bool bWon)
	{
		Zenith_UI::Zenith_UIText* pxWin = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("WinText");
		if (pxWin)
		{
			pxWin->SetText(bWon ? "LEVEL COMPLETE!" : "");
		}
	}

	/**
	 * UpdateMoveCount - Update just the move counter
	 */
	static void UpdateMoveCount(Zenith_UIComponent& xUIComponent, uint32_t uMoveCount)
	{
		Zenith_UI::Zenith_UIText* pxStatus = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("Status");
		if (pxStatus)
		{
			char acBuffer[32];
			snprintf(acBuffer, sizeof(acBuffer), "Moves: %u", uMoveCount);
			pxStatus->SetText(acBuffer);
		}
	}

	/**
	 * UpdateProgress - Update just the progress counter
	 */
	static void UpdateProgress(Zenith_UIComponent& xUIComponent, uint32_t uBoxesOnTargets, uint32_t uTargetCount)
	{
		Zenith_UI::Zenith_UIText* pxProgress = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("Progress");
		if (pxProgress)
		{
			char acBuffer[32];
			snprintf(acBuffer, sizeof(acBuffer), "Boxes: %u / %u", uBoxesOnTargets, uTargetCount);
			pxProgress->SetText(acBuffer);
		}
	}
};
