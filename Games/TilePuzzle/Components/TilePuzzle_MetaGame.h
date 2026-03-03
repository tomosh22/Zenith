#pragma once
/**
 * TilePuzzle_MetaGame.h - Meta-game systems for Paws & Pins
 *
 * Contains:
 * - Cat Cafe UI (collected cats display)
 * - Victory Overlay (level complete animation)
 * - Coin System (earning/spending)
 * - Energy/Lives System
 * - Daily Puzzle
 * - Level Map star rendering
 *
 * These are helper methods mixed into TilePuzzle_Behaviour via inclusion.
 * This file is NOT standalone - it is included inside the class body.
 */

// ============================================================================
// Cat Cafe Screen
// ============================================================================

void SetCatCafeVisible(bool bVisible)
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	const char* aszCatCafeElements[] = {
		"CatCafeBg", "CatCafeTitle", "CatCafeCount",
		"CatCafeBackButton", "CatCafePrevPage", "CatCafeNextPage"
	};
	for (const char* szName : aszCatCafeElements)
	{
		Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
		if (pxElem) pxElem->SetVisible(bVisible);
	}

	// Cat card text elements (8 per page)
	for (uint32_t i = 0; i < 8; ++i)
	{
		char szName[32];
		snprintf(szName, sizeof(szName), "CatCard_%u", i);
		Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
		if (pxElem) pxElem->SetVisible(bVisible);

		snprintf(szName, sizeof(szName), "CatCardBg_%u", i);
		pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
		if (pxElem) pxElem->SetVisible(bVisible);
	}
}

void UpdateCatCafeUI()
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	// Update total count
	Zenith_UI::Zenith_UIText* pxCount = xUI.FindElement<Zenith_UI::Zenith_UIText>("CatCafeCount");
	if (pxCount)
	{
		char szCount[64];
		snprintf(szCount, sizeof(szCount), "%u / %u cats rescued",
			m_xSaveData.uCatsCollectedCount, TilePuzzleSaveData::uMAX_CATS);
		pxCount->SetText(szCount);
	}

	// Update cat cards for current page (8 cats per page)
	uint32_t uStartCat = m_uCatCafePage * 8;
	for (uint32_t i = 0; i < 8; ++i)
	{
		uint32_t uCatID = uStartCat + i;
		char szCardName[32];
		snprintf(szCardName, sizeof(szCardName), "CatCard_%u", i);
		Zenith_UI::Zenith_UIText* pxCard = xUI.FindElement<Zenith_UI::Zenith_UIText>(szCardName);
		if (!pxCard) continue;

		char szBgName[32];
		snprintf(szBgName, sizeof(szBgName), "CatCardBg_%u", i);
		Zenith_UI::Zenith_UIElement* pxBg = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szBgName);

		if (uCatID >= TilePuzzleSaveData::uMAX_CATS)
		{
			pxCard->SetVisible(false);
			if (pxBg) pxBg->SetVisible(false);
			continue;
		}

		pxCard->SetVisible(true);
		if (pxBg) pxBg->SetVisible(true);

		if (m_xSaveData.IsCatCollected(uCatID))
		{
			// Deterministic name/breed from cat ID
			const char* szName = s_aszCatNames[uCatID % s_uCatNameCount];
			const char* szBreed = s_aszCatBreeds[uCatID % s_uCatBreedCount];
			uint32_t uLevel = uCatID + 1;
			bool bThreeStarred = m_xSaveData.GetStarRating(uLevel) >= 3;

			char szText[128];
			if (bThreeStarred)
			{
				snprintf(szText, sizeof(szText), "%s\n%s\nLvl %u ***", szName, szBreed, uLevel);
			}
			else
			{
				snprintf(szText, sizeof(szText), "%s\n%s\nLvl %u", szName, szBreed, uLevel);
			}
			pxCard->SetText(szText);
			pxCard->SetFontSize(20.f);
		}
		else
		{
			pxCard->SetText("???");
			pxCard->SetFontSize(28.f);
		}
	}
}

static void OnCatCafeClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_eState = TILEPUZZLE_STATE_CAT_CAFE;
	pxSelf->m_uCatCafePage = 0;
	pxSelf->SetMenuVisible(false);
	pxSelf->SetCatCafeVisible(true);
	pxSelf->UpdateCatCafeUI();
}

static void OnCatCafeBackClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_eState = TILEPUZZLE_STATE_MAIN_MENU;
	pxSelf->SetCatCafeVisible(false);
	pxSelf->SetMenuVisible(true);
}

static void OnCatCafePrevPageClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	if (pxSelf->m_uCatCafePage > 0)
	{
		pxSelf->m_uCatCafePage--;
		pxSelf->UpdateCatCafeUI();
	}
}

static void OnCatCafeNextPageClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	uint32_t uTotalPages = (TilePuzzleSaveData::uMAX_CATS + 7) / 8;
	if (pxSelf->m_uCatCafePage < uTotalPages - 1)
	{
		pxSelf->m_uCatCafePage++;
		pxSelf->UpdateCatCafeUI();
	}
}

// ============================================================================
// Victory Overlay
// ============================================================================

uint8_t CalculateStarRating() const
{
	uint32_t uPar = m_xCurrentLevel.uMinimumMoves;
	if (uPar == 0) uPar = 1;

	if (m_uMoveCount <= uPar)
		return 3;
	else if (m_uMoveCount <= uPar + 2)
		return 2;
	else
		return 1;
}

void ShowVictoryOverlay()
{
	m_bVictoryOverlayActive = true;
	m_fVictoryTimer = 0.f;
	m_uVictoryStarsShown = 0;
	m_uVictoryStarRating = CalculateStarRating();

	// Calculate coins earned
	m_uVictoryCoinsEarned = s_uCoinsPerLevelComplete;
	if (m_uVictoryStarRating >= 3)
	{
		m_uVictoryCoinsEarned += s_uCoinsPerThreeStar;
	}

	// Award coins
	m_xSaveData.AddCoins(static_cast<int32_t>(m_uVictoryCoinsEarned));

	// Update star rating (only if better)
	m_xSaveData.SetStarRating(m_uCurrentLevelNumber, m_uVictoryStarRating);

	// Collect cat for this level
	uint32_t uCatID = m_uCurrentLevelNumber - 1;
	m_xSaveData.CollectCat(uCatID);

	// Show victory UI elements
	SetVictoryOverlayVisible(true);
}

void SetVictoryOverlayVisible(bool bVisible)
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	const char* aszVictoryElements[] = {
		"VictoryBg", "VictoryTitle", "VictoryStars",
		"VictoryCatText", "VictoryCoinsText", "NextLevelBtn"
	};
	for (const char* szName : aszVictoryElements)
	{
		Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
		if (pxElem) pxElem->SetVisible(bVisible);
	}
}

void UpdateVictoryOverlay(float fDeltaTime)
{
	m_fVictoryTimer += fDeltaTime;

	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	// Animate stars appearing sequentially (one every 0.4 seconds)
	uint32_t uTargetStars = static_cast<uint32_t>(m_fVictoryTimer / 0.4f);
	if (uTargetStars > m_uVictoryStarRating)
		uTargetStars = m_uVictoryStarRating;

	if (uTargetStars > m_uVictoryStarsShown)
	{
		m_uVictoryStarsShown = uTargetStars;
	}

	// Update star display text
	Zenith_UI::Zenith_UIText* pxStars = xUI.FindElement<Zenith_UI::Zenith_UIText>("VictoryStars");
	if (pxStars)
	{
		char szStars[16] = "";
		for (uint32_t i = 0; i < 3; ++i)
		{
			size_t uLen = strlen(szStars);
			if (uLen + 2 < sizeof(szStars))
			{
				if (i < m_uVictoryStarsShown)
				{
					szStars[uLen] = '*';
					szStars[uLen + 1] = ' ';
					szStars[uLen + 2] = '\0';
				}
				else
				{
					szStars[uLen] = '-';
					szStars[uLen + 1] = ' ';
					szStars[uLen + 2] = '\0';
				}
			}
		}
		pxStars->SetText(szStars);
	}

	// Update cat rescued text
	Zenith_UI::Zenith_UIText* pxCatText = xUI.FindElement<Zenith_UI::Zenith_UIText>("VictoryCatText");
	if (pxCatText)
	{
		uint32_t uCatID = m_uCurrentLevelNumber - 1;
		const char* szCatName = s_aszCatNames[uCatID % s_uCatNameCount];
		char szText[64];
		snprintf(szText, sizeof(szText), "Cat rescued: %s!", szCatName);
		pxCatText->SetText(szText);
	}

	// Update coins earned text
	Zenith_UI::Zenith_UIText* pxCoins = xUI.FindElement<Zenith_UI::Zenith_UIText>("VictoryCoinsText");
	if (pxCoins)
	{
		char szText[32];
		snprintf(szText, sizeof(szText), "+%u coins", m_uVictoryCoinsEarned);
		pxCoins->SetText(szText);
	}
}

// ============================================================================
// Lives System
// ============================================================================

void SetLivesDisplayVisible(bool bVisible)
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	Zenith_UI::Zenith_UIText* pxLives = xUI.FindElement<Zenith_UI::Zenith_UIText>("LivesText");
	if (pxLives) pxLives->SetVisible(bVisible);

	Zenith_UI::Zenith_UIButton* pxRefill = xUI.FindElement<Zenith_UI::Zenith_UIButton>("RefillLivesButton");
	if (pxRefill) pxRefill->SetVisible(bVisible && m_xSaveData.uLives < TilePuzzleSaveData::uMAX_LIVES);
}

void UpdateLivesDisplay()
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	Zenith_UI::Zenith_UIText* pxLives = xUI.FindElement<Zenith_UI::Zenith_UIText>("LivesText");
	if (pxLives)
	{
		// Build heart display
		char szLives[64];
		uint32_t uTimerSecs = m_xSaveData.GetSecondsUntilNextLife(GetCurrentTimestamp());
		if (m_xSaveData.uLives >= TilePuzzleSaveData::uMAX_LIVES)
		{
			snprintf(szLives, sizeof(szLives), "Lives: %u/%u",
				m_xSaveData.uLives, TilePuzzleSaveData::uMAX_LIVES);
		}
		else
		{
			uint32_t uMins = uTimerSecs / 60;
			uint32_t uSecs = uTimerSecs % 60;
			snprintf(szLives, sizeof(szLives), "Lives: %u/%u  Next: %u:%02u",
				m_xSaveData.uLives, TilePuzzleSaveData::uMAX_LIVES, uMins, uSecs);
		}
		pxLives->SetText(szLives);
	}

	// Show/hide refill button based on lives count
	Zenith_UI::Zenith_UIButton* pxRefill = xUI.FindElement<Zenith_UI::Zenith_UIButton>("RefillLivesButton");
	if (pxRefill)
	{
		pxRefill->SetVisible(m_eState == TILEPUZZLE_STATE_MAIN_MENU &&
			m_xSaveData.uLives < TilePuzzleSaveData::uMAX_LIVES);
	}
}

static void OnRefillLivesClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	if (pxSelf->m_xSaveData.TryRefillLivesWithCoins())
	{
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &pxSelf->m_xSaveData);
	}
}

// ============================================================================
// Coin Display
// ============================================================================

void UpdateCoinDisplay()
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	Zenith_UI::Zenith_UIText* pxCoins = xUI.FindElement<Zenith_UI::Zenith_UIText>("CoinText");
	if (pxCoins)
	{
		char szCoins[32];
		snprintf(szCoins, sizeof(szCoins), "Coins: %u", m_xSaveData.uCoins);
		pxCoins->SetText(szCoins);
	}
}

// ============================================================================
// Daily Puzzle
// ============================================================================

static void OnDailyPuzzleClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);

	uint32_t uToday = GetCurrentDateYYYYMMDD();

	// Check if daily puzzle file exists
	char szPath[ZENITH_MAX_PATH_LENGTH];
	snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Levels/daily_%08u.tlvl", uToday);

	if (!Zenith_FileAccess::FileExists(szPath))
	{
		// Fall back to a level based on the date (cycle through available levels)
		uint32_t uFallbackLevel = (uToday % pxSelf->m_uAvailableLevelCount) + 1;
		pxSelf->m_uCurrentLevelNumber = uFallbackLevel;
	}
	else
	{
		// Load the daily puzzle file directly
		pxSelf->m_uCurrentLevelNumber = 1; // Will be overridden
	}

	pxSelf->m_bDailyPuzzleMode = true;
	pxSelf->m_xSaveData.UpdateDailyStreak(uToday);

	Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
		TilePuzzle_WriteSaveData, &pxSelf->m_xSaveData);

	Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
}

void UpdateDailyStreakDisplay()
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	Zenith_UI::Zenith_UIText* pxStreak = xUI.FindElement<Zenith_UI::Zenith_UIText>("DailyStreakText");
	if (pxStreak)
	{
		char szStreak[32];
		snprintf(szStreak, sizeof(szStreak), "Streak: %u days", m_xSaveData.uDailyStreak);
		pxStreak->SetText(szStreak);
	}
}

void OnDailyPuzzleCompleted()
{
	uint32_t uToday = GetCurrentDateYYYYMMDD();

	// Award daily puzzle coins
	m_xSaveData.AddCoins(static_cast<int32_t>(s_uCoinsPerDailyPuzzle));

	// Update best moves for today
	if (m_xSaveData.uLastDailyPuzzleDate != uToday ||
		m_xSaveData.uDailyPuzzleBestMoves == 0 ||
		m_uMoveCount < m_xSaveData.uDailyPuzzleBestMoves)
	{
		m_xSaveData.uDailyPuzzleBestMoves = m_uMoveCount;
	}
	m_xSaveData.uLastDailyPuzzleDate = uToday;

	// Note: save is handled by the caller (OnLevelCompleted)
	m_bDailyPuzzleMode = false;
}

// ============================================================================
// Menu-Level Rendering with Meta-Game Info
// ============================================================================

void UpdateMainMenuUI()
{
	UpdateCoinDisplay();
	UpdateLivesDisplay();
	UpdateDailyStreakDisplay();

	// Regenerate lives periodically
	m_xSaveData.RegenerateLives(GetCurrentTimestamp());
}
