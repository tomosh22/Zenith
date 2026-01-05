#pragma once
/**
 * Marble_UIManager.h - HUD management
 *
 * Demonstrates:
 * - Dynamic text with snprintf formatting
 * - Color changes based on game state
 * - Multiple UI elements (Score, Time, Collected, Status)
 *
 * UI element names (set up in Marble.cpp):
 * - "Score"     - Current score
 * - "Time"      - Time remaining
 * - "Collected" - Collectibles progress
 * - "Status"    - Game state message (WIN/LOSE/PAUSED)
 */

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIText.h"
#include "Maths/Zenith_Maths.h"
#include <cstdio>
#include <cstdint>

// Game state enum (shared with Marble_Behaviour.h)
enum class MarbleGameState
{
	PLAYING,
	PAUSED,
	WON,
	LOST
};

/**
 * Marble_UIManager - HUD text management
 */
class Marble_UIManager
{
public:
	/**
	 * UpdateUI - Update all HUD elements
	 *
	 * @param xUIComponent     UI component reference
	 * @param uScore           Current score
	 * @param fTimeRemaining   Time left in seconds
	 * @param uCollectedCount  Number of collectibles collected
	 * @param uTotalCollectibles Total number of collectibles
	 * @param eGameState       Current game state
	 */
	static void UpdateUI(
		Zenith_UIComponent& xUIComponent,
		uint32_t uScore,
		float fTimeRemaining,
		uint32_t uCollectedCount,
		uint32_t uTotalCollectibles,
		MarbleGameState eGameState)
	{
		char acBuffer[64];

		// Score display
		Zenith_UI::Zenith_UIText* pxScore = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("Score");
		if (pxScore)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Score: %u", uScore);
			pxScore->SetText(acBuffer);
		}

		// Time display
		Zenith_UI::Zenith_UIText* pxTime = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("Time");
		if (pxTime)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Time: %.1f", fTimeRemaining);
			pxTime->SetText(acBuffer);
		}

		// Collected display
		Zenith_UI::Zenith_UIText* pxCollected = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("Collected");
		if (pxCollected)
		{
			snprintf(acBuffer, sizeof(acBuffer), "Collected: %u / %u", uCollectedCount, uTotalCollectibles);
			pxCollected->SetText(acBuffer);
		}

		// Status display (changes color based on state)
		UpdateStatusText(xUIComponent, eGameState);
	}

	/**
	 * UpdateStatusText - Update the game state message
	 *
	 * Shows different messages and colors for each state.
	 */
	static void UpdateStatusText(Zenith_UIComponent& xUIComponent, MarbleGameState eGameState)
	{
		Zenith_UI::Zenith_UIText* pxStatus = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("Status");
		if (!pxStatus)
			return;

		switch (eGameState)
		{
		case MarbleGameState::WON:
			pxStatus->SetText("YOU WIN!");
			pxStatus->SetColor(Zenith_Maths::Vector4(0.2f, 1.f, 0.2f, 1.f)); // Green
			break;

		case MarbleGameState::LOST:
			pxStatus->SetText("GAME OVER");
			pxStatus->SetColor(Zenith_Maths::Vector4(1.f, 0.2f, 0.2f, 1.f)); // Red
			break;

		case MarbleGameState::PAUSED:
			pxStatus->SetText("PAUSED");
			pxStatus->SetColor(Zenith_Maths::Vector4(1.f, 1.f, 0.2f, 1.f)); // Yellow
			break;

		default:
			pxStatus->SetText("");
			break;
		}
	}

	/**
	 * UpdateScore - Update just the score display
	 */
	static void UpdateScore(Zenith_UIComponent& xUIComponent, uint32_t uScore)
	{
		Zenith_UI::Zenith_UIText* pxScore = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("Score");
		if (pxScore)
		{
			char acBuffer[32];
			snprintf(acBuffer, sizeof(acBuffer), "Score: %u", uScore);
			pxScore->SetText(acBuffer);
		}
	}

	/**
	 * UpdateTime - Update just the time display
	 */
	static void UpdateTime(Zenith_UIComponent& xUIComponent, float fTimeRemaining)
	{
		Zenith_UI::Zenith_UIText* pxTime = xUIComponent.FindElement<Zenith_UI::Zenith_UIText>("Time");
		if (pxTime)
		{
			char acBuffer[32];
			snprintf(acBuffer, sizeof(acBuffer), "Time: %.1f", fTimeRemaining);
			pxTime->SetText(acBuffer);
		}
	}
};
