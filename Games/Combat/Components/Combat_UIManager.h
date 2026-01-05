#pragma once
/**
 * Combat_UIManager.h - Health bars and combo display
 *
 * Demonstrates:
 * - Zenith_UIComponent text element management
 * - Dynamic text updates for health/combo
 * - Color changes based on health state
 * - Game over / victory screens
 */

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Maths/Zenith_Maths.h"
#include <string>
#include <cstdio>

// ============================================================================
// Game State
// ============================================================================

enum class Combat_GameState : uint8_t
{
	PLAYING,
	PAUSED,
	VICTORY,
	GAME_OVER
};

// ============================================================================
// UI Manager
// ============================================================================

/**
 * Combat_UIManager - Manages combat game HUD
 */
class Combat_UIManager
{
public:
	// UI Element Names
	static constexpr const char* TITLE = "Title";
	static constexpr const char* PLAYER_HEALTH = "PlayerHealth";
	static constexpr const char* PLAYER_HEALTH_BAR = "PlayerHealthBar";
	static constexpr const char* COMBO_COUNT = "ComboCount";
	static constexpr const char* COMBO_TEXT = "ComboText";
	static constexpr const char* ENEMY_COUNT = "EnemyCount";
	static constexpr const char* CONTROLS = "Controls";
	static constexpr const char* STATUS = "Status";

	// ========================================================================
	// Update Functions
	// ========================================================================

	/**
	 * UpdatePlayerHealth - Update player health display
	 */
	static void UpdatePlayerHealth(Zenith_UIComponent& xUI, float fHealth, float fMaxHealth)
	{
		Zenith_UI::Zenith_UIText* pxHealthText = xUI.FindElement<Zenith_UI::Zenith_UIText>(PLAYER_HEALTH);
		if (pxHealthText)
		{
			char szBuffer[64];
			snprintf(szBuffer, sizeof(szBuffer), "Health: %.0f / %.0f", fHealth, fMaxHealth);
			pxHealthText->SetText(szBuffer);

			// Color based on health percentage
			float fPercent = fHealth / fMaxHealth;
			if (fPercent > 0.6f)
			{
				pxHealthText->SetColor(Zenith_Maths::Vector4(0.2f, 1.0f, 0.2f, 1.0f));  // Green
			}
			else if (fPercent > 0.3f)
			{
				pxHealthText->SetColor(Zenith_Maths::Vector4(1.0f, 0.8f, 0.2f, 1.0f));  // Yellow
			}
			else
			{
				pxHealthText->SetColor(Zenith_Maths::Vector4(1.0f, 0.2f, 0.2f, 1.0f));  // Red
			}
		}

		// Update health bar (visual representation)
		Zenith_UI::Zenith_UIText* pxBar = xUI.FindElement<Zenith_UI::Zenith_UIText>(PLAYER_HEALTH_BAR);
		if (pxBar)
		{
			// Create a simple bar using characters
			float fPercent = fHealth / fMaxHealth;
			int iBarLength = static_cast<int>(fPercent * 20.0f);
			iBarLength = std::max(0, std::min(20, iBarLength));

			std::string strBar = "[";
			for (int i = 0; i < 20; i++)
			{
				strBar += (i < iBarLength) ? "|" : ".";
			}
			strBar += "]";
			pxBar->SetText(strBar.c_str());

			// Same color coding
			if (fPercent > 0.6f)
				pxBar->SetColor(Zenith_Maths::Vector4(0.2f, 1.0f, 0.2f, 1.0f));
			else if (fPercent > 0.3f)
				pxBar->SetColor(Zenith_Maths::Vector4(1.0f, 0.8f, 0.2f, 1.0f));
			else
				pxBar->SetColor(Zenith_Maths::Vector4(1.0f, 0.2f, 0.2f, 1.0f));
		}
	}

	/**
	 * UpdateCombo - Update combo counter display
	 */
	static void UpdateCombo(Zenith_UIComponent& xUI, uint32_t uComboCount, float fComboTimer)
	{
		Zenith_UI::Zenith_UIText* pxComboCount = xUI.FindElement<Zenith_UI::Zenith_UIText>(COMBO_COUNT);
		Zenith_UI::Zenith_UIText* pxComboText = xUI.FindElement<Zenith_UI::Zenith_UIText>(COMBO_TEXT);

		if (uComboCount > 1)
		{
			if (pxComboCount)
			{
				char szBuffer[32];
				snprintf(szBuffer, sizeof(szBuffer), "%u", uComboCount);
				pxComboCount->SetText(szBuffer);

				// Combo color ramps up
				float fIntensity = std::min(1.0f, uComboCount / 5.0f);
				pxComboCount->SetColor(Zenith_Maths::Vector4(1.0f, 1.0f - fIntensity * 0.5f, 0.2f, 1.0f));
			}

			if (pxComboText)
			{
				pxComboText->SetText("COMBO!");
				pxComboText->SetColor(Zenith_Maths::Vector4(1.0f, 0.8f, 0.2f, 1.0f));
			}
		}
		else
		{
			if (pxComboCount)
				pxComboCount->SetText("");
			if (pxComboText)
				pxComboText->SetText("");
		}
	}

	/**
	 * UpdateEnemyCount - Update remaining enemy count
	 */
	static void UpdateEnemyCount(Zenith_UIComponent& xUI, uint32_t uAliveEnemies, uint32_t uTotalEnemies)
	{
		Zenith_UI::Zenith_UIText* pxEnemyCount = xUI.FindElement<Zenith_UI::Zenith_UIText>(ENEMY_COUNT);
		if (pxEnemyCount)
		{
			char szBuffer[64];
			snprintf(szBuffer, sizeof(szBuffer), "Enemies: %u / %u", uAliveEnemies, uTotalEnemies);
			pxEnemyCount->SetText(szBuffer);

			if (uAliveEnemies == 0)
			{
				pxEnemyCount->SetColor(Zenith_Maths::Vector4(0.2f, 1.0f, 0.2f, 1.0f));  // All defeated
			}
			else
			{
				pxEnemyCount->SetColor(Zenith_Maths::Vector4(0.8f, 0.8f, 0.8f, 1.0f));
			}
		}
	}

	/**
	 * UpdateGameState - Update status text based on game state
	 */
	static void UpdateGameState(Zenith_UIComponent& xUI, Combat_GameState eState)
	{
		Zenith_UI::Zenith_UIText* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>(STATUS);
		if (!pxStatus)
			return;

		switch (eState)
		{
		case Combat_GameState::PLAYING:
			pxStatus->SetText("");
			break;

		case Combat_GameState::PAUSED:
			pxStatus->SetText("PAUSED");
			pxStatus->SetColor(Zenith_Maths::Vector4(1.0f, 1.0f, 0.2f, 1.0f));
			break;

		case Combat_GameState::VICTORY:
			pxStatus->SetText("VICTORY!");
			pxStatus->SetColor(Zenith_Maths::Vector4(0.2f, 1.0f, 0.2f, 1.0f));
			break;

		case Combat_GameState::GAME_OVER:
			pxStatus->SetText("GAME OVER");
			pxStatus->SetColor(Zenith_Maths::Vector4(1.0f, 0.2f, 0.2f, 1.0f));
			break;
		}
	}

	/**
	 * UpdateAll - Convenience function to update all UI elements
	 */
	static void UpdateAll(
		Zenith_UIComponent& xUI,
		float fPlayerHealth,
		float fPlayerMaxHealth,
		uint32_t uComboCount,
		float fComboTimer,
		uint32_t uAliveEnemies,
		uint32_t uTotalEnemies,
		Combat_GameState eState)
	{
		UpdatePlayerHealth(xUI, fPlayerHealth, fPlayerMaxHealth);
		UpdateCombo(xUI, uComboCount, fComboTimer);
		UpdateEnemyCount(xUI, uAliveEnemies, uTotalEnemies);
		UpdateGameState(xUI, eState);
	}
};
