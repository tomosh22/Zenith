#pragma once
/**
 * Runner_UIManager.h - HUD and UI management
 *
 * Demonstrates:
 * - Zenith_UIComponent - Container for UI elements
 * - Zenith_UIText - Text rendering with anchoring
 * - Dynamic text updates
 * - Color changes based on game state
 *
 * UI Elements:
 * - Distance counter
 * - Score display
 * - Speed indicator
 * - Game over / pause overlay
 */

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Runner_CharacterController.h"
#include "Maths/Zenith_Maths.h"
#include <cstdio>

// Game state for UI
enum class RunnerGameState
{
	PLAYING,
	PAUSED,
	GAME_OVER
};

/**
 * Runner_UIManager - Manages game HUD
 */
class Runner_UIManager
{
public:
	// ========================================================================
	// Update UI
	// ========================================================================
	static void UpdateUI(
		Zenith_UIComponent& xUI,
		float fDistance,
		uint32_t uScore,
		float fSpeed,
		float fMaxSpeed,
		RunnerGameState eGameState)
	{
		// Distance
		Zenith_UI::Zenith_UIText* pxDistance = xUI.FindElement<Zenith_UI::Zenith_UIText>("Distance");
		if (pxDistance)
		{
			char szBuffer[64];
			snprintf(szBuffer, sizeof(szBuffer), "%.0fm", fDistance);
			pxDistance->SetText(szBuffer);

			// Color based on distance milestones
			if (fDistance >= 1000.0f)
			{
				pxDistance->SetColor(Zenith_Maths::Vector4(1.0f, 0.84f, 0.0f, 1.0f));  // Gold
			}
			else if (fDistance >= 500.0f)
			{
				pxDistance->SetColor(Zenith_Maths::Vector4(0.75f, 0.75f, 0.75f, 1.0f));  // Silver
			}
			else
			{
				pxDistance->SetColor(Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));  // White
			}
		}

		// Score
		Zenith_UI::Zenith_UIText* pxScore = xUI.FindElement<Zenith_UI::Zenith_UIText>("Score");
		if (pxScore)
		{
			char szBuffer[64];
			snprintf(szBuffer, sizeof(szBuffer), "Score: %u", uScore);
			pxScore->SetText(szBuffer);
		}

		// Speed indicator
		Zenith_UI::Zenith_UIText* pxSpeed = xUI.FindElement<Zenith_UI::Zenith_UIText>("Speed");
		if (pxSpeed)
		{
			char szBuffer[64];
			snprintf(szBuffer, sizeof(szBuffer), "Speed: %.1f", fSpeed);
			pxSpeed->SetText(szBuffer);

			// Color gradient based on speed ratio
			float fSpeedRatio = fSpeed / fMaxSpeed;
			if (fSpeedRatio >= 0.9f)
			{
				pxSpeed->SetColor(Zenith_Maths::Vector4(1.0f, 0.3f, 0.3f, 1.0f));  // Red - max speed
			}
			else if (fSpeedRatio >= 0.6f)
			{
				pxSpeed->SetColor(Zenith_Maths::Vector4(1.0f, 0.7f, 0.3f, 1.0f));  // Orange
			}
			else
			{
				pxSpeed->SetColor(Zenith_Maths::Vector4(0.6f, 0.8f, 1.0f, 1.0f));  // Blue
			}
		}

		// Status message
		Zenith_UI::Zenith_UIText* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status");
		if (pxStatus)
		{
			switch (eGameState)
			{
			case RunnerGameState::PLAYING:
				pxStatus->SetText("");
				break;

			case RunnerGameState::PAUSED:
				pxStatus->SetText("PAUSED\n\nPress P to Resume");
				pxStatus->SetColor(Zenith_Maths::Vector4(1.0f, 1.0f, 0.3f, 1.0f));  // Yellow
				break;

			case RunnerGameState::GAME_OVER:
				{
					char szBuffer[128];
					snprintf(szBuffer, sizeof(szBuffer), "GAME OVER\n\nDistance: %.0fm\nScore: %u\n\nPress R to Restart", fDistance, uScore);
					pxStatus->SetText(szBuffer);
					pxStatus->SetColor(Zenith_Maths::Vector4(1.0f, 0.3f, 0.3f, 1.0f));  // Red
				}
				break;
			}
		}

		// Controls hint (only shown at start)
		Zenith_UI::Zenith_UIText* pxControls = xUI.FindElement<Zenith_UI::Zenith_UIText>("Controls");
		if (pxControls)
		{
			// Fade out controls hint after 100m
			if (fDistance > 100.0f)
			{
				float fAlpha = glm::max(0.0f, 1.0f - (fDistance - 100.0f) / 50.0f);
				pxControls->SetColor(Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, fAlpha));
			}
		}
	}

	// ========================================================================
	// High Score Display
	// ========================================================================
	static void UpdateHighScore(Zenith_UIComponent& xUI, uint32_t uHighScore)
	{
		Zenith_UI::Zenith_UIText* pxHighScore = xUI.FindElement<Zenith_UI::Zenith_UIText>("HighScore");
		if (pxHighScore)
		{
			char szBuffer[64];
			snprintf(szBuffer, sizeof(szBuffer), "Best: %u", uHighScore);
			pxHighScore->SetText(szBuffer);
		}
	}
};
