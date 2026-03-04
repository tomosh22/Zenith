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
		"CatCafeBg", "CatCafeTitle", "CatCafeCount"
	};
	for (const char* szName : aszCatCafeElements)
	{
		Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
		if (pxElem) pxElem->SetVisible(bVisible);
	}

	// Cat Cafe navigation layout group
	Zenith_UI::Zenith_UIElement* pxNavGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("CatCafeNavGroup");
	if (pxNavGroup) pxNavGroup->SetVisible(bVisible);

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

	// Update total count with progress percentage
	Zenith_UI::Zenith_UIText* pxCount = xUI.FindElement<Zenith_UI::Zenith_UIText>("CatCafeCount");
	if (pxCount)
	{
		uint32_t uPercent = (m_xSaveData.uCatsCollectedCount * 100) / TilePuzzleSaveData::uMAX_CATS;
		char szCount[96];
		snprintf(szCount, sizeof(szCount), "%u / %u cats rescued (%u%%)",
			m_xSaveData.uCatsCollectedCount, TilePuzzleSaveData::uMAX_CATS, uPercent);
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
			const char* szName = s_aszCatNames[uCatID % s_uCatNameCount];
			const char* szBreed = s_aszCatBreeds[uCatID % s_uCatBreedCount];
			uint32_t uLevel = uCatID + 1;
			uint8_t uStars = m_xSaveData.GetStarRating(uLevel);

			char szText[128];
			snprintf(szText, sizeof(szText), "%s\n%s\nLvl %u %s", szName, szBreed, uLevel, GetStarString(uStars));
			pxCard->SetText(szText);
			pxCard->SetFontSize(20.f);

			// Tint card background based on cat color (derived from level)
			if (pxBg)
			{
				TilePuzzleColor eColor = static_cast<TilePuzzleColor>(uCatID % TILEPUZZLE_COLOR_COUNT);
				Zenith_Maths::Vector4 xTint = GetParticleColorForTile(eColor);
				xTint.w = 0.3f; // Subtle tint
				pxBg->SetColor(xTint);
			}
		}
		else
		{
			pxCard->SetText("???");
			pxCard->SetFontSize(28.f);
			if (pxBg)
			{
				pxBg->SetColor(Zenith_Maths::Vector4(0.15f, 0.15f, 0.15f, 0.5f));
			}
		}
	}
}

static void OnCatCafeClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_uCatCafePage = 0;
	pxSelf->StartTransition(TILEPUZZLE_STATE_CAT_CAFE);
}

static void OnCatCafeBackClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->StartTransition(TILEPUZZLE_STATE_MAIN_MENU);
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

void SetVictoryOverlayVisible(bool bVisible)
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	const char* aszVictoryElements[] = {
		"VictoryBg", "VictoryTitle", "VictoryStars",
		"VictoryCatText", "VictoryCoinsText", "NextLevelBtn",
		"VictoryStarGroup", "VictoryStar0", "VictoryStar1", "VictoryStar2"
	};

	if (bVisible)
	{
		// Show all elements but start fully transparent for staggered reveal
		for (const char* szName : aszVictoryElements)
		{
			Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
			if (pxElem)
			{
				pxElem->SetVisible(true);
				Zenith_Maths::Vector4 xColor = pxElem->GetColor();
				xColor.w = 0.0f;
				pxElem->SetColor(xColor);
			}
		}

		// Reset star sizes to 0 for scale-in animation
		static const char* s_aszStarNames[] = { "VictoryStar0", "VictoryStar1", "VictoryStar2" };
		for (uint32_t u = 0; u < 3; ++u)
		{
			Zenith_UI::Zenith_UIImage* pxStar = xUI.FindElement<Zenith_UI::Zenith_UIImage>(s_aszStarNames[u]);
			if (pxStar)
			{
				pxStar->SetSize(0.0f, 0.0f);
			}
		}

		// Reset coin display counter
		m_uVictoryDisplayedCoins = 0;
	}
	else
	{
		for (const char* szName : aszVictoryElements)
		{
			Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
			if (pxElem) pxElem->SetVisible(false);
		}
	}
}

void UpdateVictoryOverlay(float fDeltaTime)
{
	m_fVictoryTimer += fDeltaTime;

	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	// Staggered reveal timing constants
	static constexpr float s_fBgFadeStart    = 0.0f;
	static constexpr float s_fBgFadeDuration = 0.25f;
	static constexpr float s_fTitleStart     = 0.3f;
	static constexpr float s_fTitleDuration  = 0.3f;
	static constexpr float s_fStar0Start     = 0.6f;
	static constexpr float s_fStar1Start     = 1.0f;
	static constexpr float s_fStar2Start     = 1.4f;
	static constexpr float s_fStarDuration   = 0.3f;
	static constexpr float s_fCatTextStart   = 1.8f;
	static constexpr float s_fCatTextDuration = 0.3f;
	static constexpr float s_fCoinsStart     = 2.2f;
	static constexpr float s_fCoinsDuration  = 0.4f;
	static constexpr float s_fButtonsStart   = 2.5f;
	static constexpr float s_fButtonsDuration = 0.25f;

	float fT = m_fVictoryTimer;

	// Helper: compute 0-1 progress for a timed element
	auto EaseInRange = [](float fTime, float fStart, float fDuration) -> float
	{
		if (fTime < fStart) return 0.0f;
		if (fTime >= fStart + fDuration) return 1.0f;
		return (fTime - fStart) / fDuration;
	};

	// (0.0s) Background fades in
	{
		Zenith_UI::Zenith_UIElement* pxBg = xUI.FindElement<Zenith_UI::Zenith_UIElement>("VictoryBg");
		if (pxBg)
		{
			float fProgress = EaseInRange(fT, s_fBgFadeStart, s_fBgFadeDuration);
			pxBg->SetColor(Zenith_Maths::Vector4(0.05f, 0.05f, 0.15f, 0.9f * fProgress));
		}
	}

	// (0.3s) Title scales up with bounce easing
	{
		Zenith_UI::Zenith_UIText* pxTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("VictoryTitle");
		if (pxTitle)
		{
			float fProgress = EaseInRange(fT, s_fTitleStart, s_fTitleDuration);
			float fEased = Zenith_ApplyEasing(EASING_BACK_OUT, fProgress);
			pxTitle->SetColor(Zenith_Maths::Vector4(1.0f, 1.0f, 0.5f, fProgress));
			// Simulate scale via font size (base 56)
			float fFontSize = 56.0f * fEased;
			if (fFontSize > 0.1f)
				pxTitle->SetFontSize(fFontSize);
		}
	}

	// (0.6s, 1.0s, 1.4s) Stars appear with scale bounce
	{
		static const char* s_aszStarNames[] = { "VictoryStar0", "VictoryStar1", "VictoryStar2" };
		static constexpr float s_afStarStarts[] = { s_fStar0Start, s_fStar1Start, s_fStar2Start };
		static constexpr float s_fStarTargetSize = 48.0f;

		uint32_t uPrevStarsShown = m_uVictoryStarsShown;

		for (uint32_t u = 0; u < 3; ++u)
		{
			Zenith_UI::Zenith_UIImage* pxStar = xUI.FindElement<Zenith_UI::Zenith_UIImage>(s_aszStarNames[u]);
			if (!pxStar)
				continue;

			float fProgress = EaseInRange(fT, s_afStarStarts[u], s_fStarDuration);
			if (fProgress <= 0.0f)
				continue;

			bool bFilled = u < m_uVictoryStarRating;
			pxStar->SetTexturePath(bFilled
				? GAME_ASSETS_DIR "Textures/Icons/star_filled" ZENITH_TEXTURE_EXT
				: GAME_ASSETS_DIR "Textures/Icons/star_empty" ZENITH_TEXTURE_EXT);

			// Scale with bounce easing (overshoot to 1.2x then settle)
			float fEased = Zenith_ApplyEasing(EASING_BACK_OUT, fProgress);
			float fSize = s_fStarTargetSize * fEased;
			pxStar->SetSize(fSize, fSize);

			Zenith_Maths::Vector4 xStarColor = pxStar->GetColor();
			xStarColor.w = fProgress;
			pxStar->SetColor(xStarColor);

			// Track stars shown for particle burst
			if (fProgress >= 1.0f && u >= m_uVictoryStarsShown)
				m_uVictoryStarsShown = u + 1;
		}

		// Burst particles when a new star appears
		if (m_uVictoryStarsShown > uPrevStarsShown)
		{
			Zenith_SceneData* pxParentSceneData = m_xParentEntity.GetSceneData();
			if (m_uEliminationEmitterID.IsValid() && pxParentSceneData && pxParentSceneData->EntityExists(m_uEliminationEmitterID))
			{
				Zenith_Entity xEmitter = pxParentSceneData->GetEntity(m_uEliminationEmitterID);
				Flux_ParticleEmitterConfig* pxConfig = xEmitter.GetComponent<Zenith_ParticleEmitterComponent>().GetConfig();
				if (pxConfig)
				{
					pxConfig->m_xColorStart = Zenith_Maths::Vector4(1.0f, 0.85f, 0.1f, 1.0f); // Gold sparkle
				}
				// Position roughly above the grid center (victory stars are at screen center)
				xEmitter.GetComponent<Zenith_TransformComponent>().SetPosition(
					Zenith_Maths::Vector3(0.0f, s_fShapeHeight + 0.5f, 0.0f));
				xEmitter.GetComponent<Zenith_ParticleEmitterComponent>().Emit(10);
			}
		}

		// Keep the star group visible
		Zenith_UI::Zenith_UIElement* pxStarGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("VictoryStarGroup");
		if (pxStarGroup)
		{
			float fGroupAlpha = EaseInRange(fT, s_fStar0Start, 0.1f);
			Zenith_Maths::Vector4 xGroupColor = pxStarGroup->GetColor();
			xGroupColor.w = fGroupAlpha;
			pxStarGroup->SetColor(xGroupColor);
		}

		// Hide old text stars
		Zenith_UI::Zenith_UIText* pxStarsText = xUI.FindElement<Zenith_UI::Zenith_UIText>("VictoryStars");
		if (pxStarsText) pxStarsText->SetVisible(false);
	}

	// (1.8s) Cat rescued text fades in from below
	{
		Zenith_UI::Zenith_UIText* pxCatText = xUI.FindElement<Zenith_UI::Zenith_UIText>("VictoryCatText");
		if (pxCatText)
		{
			float fProgress = EaseInRange(fT, s_fCatTextStart, s_fCatTextDuration);
			float fEased = Zenith_ApplyEasing(EASING_QUAD_OUT, fProgress);

			uint32_t uCatID = m_uCurrentLevelNumber - 1;
			const char* szCatName = s_aszCatNames[uCatID % s_uCatNameCount];
			const char* szBreed = s_aszCatBreeds[uCatID % s_uCatBreedCount];

			char szText[192];
			if (m_bVictoryFirstCompletion)
			{
				snprintf(szText, sizeof(szText), "%s the %s rescued!\nAdded to Cat Cafe!", szCatName, szBreed);

				// Check for milestones
				uint32_t uCatCount = m_xSaveData.uCatsCollectedCount;
				if (uCatCount == 10 || uCatCount == 25 || uCatCount == 50 || uCatCount == 75 || uCatCount == 100)
				{
					char szMilestone[192];
					snprintf(szMilestone, sizeof(szMilestone), "%s\nMilestone! %u cats rescued!", szText, uCatCount);
					memcpy(szText, szMilestone, sizeof(szText));
				}
			}
			else if (m_bVictoryNewBest)
			{
				snprintf(szText, sizeof(szText), "New Best! %u stars!", m_uStarsEarned);
			}
			else
			{
				snprintf(szText, sizeof(szText), "Cat rescued: %s!", szCatName);
			}
			pxCatText->SetText(szText);

			// Slide up from +10 offset
			pxCatText->SetPosition(0.0f, 20.0f + 10.0f * (1.0f - fEased));
			pxCatText->SetColor(Zenith_Maths::Vector4(0.9f, 0.7f, 0.5f, fProgress));
		}
	}

	// (2.2s) Coin counter ticks up
	{
		Zenith_UI::Zenith_UIText* pxCoins = xUI.FindElement<Zenith_UI::Zenith_UIText>("VictoryCoinsText");
		if (pxCoins)
		{
			float fProgress = EaseInRange(fT, s_fCoinsStart, s_fCoinsDuration);

			if (fProgress > 0.0f)
			{
				uint32_t uTarget = static_cast<uint32_t>(static_cast<float>(m_uVictoryCoinsEarned) * fProgress);
				if (uTarget > m_uVictoryCoinsEarned) uTarget = m_uVictoryCoinsEarned;
				m_uVictoryDisplayedCoins = uTarget;

				char szText[32];
				snprintf(szText, sizeof(szText), "+%u coins", m_uVictoryDisplayedCoins);
				pxCoins->SetText(szText);
			}

			pxCoins->SetColor(Zenith_Maths::Vector4(1.0f, 0.85f, 0.2f, fProgress));
		}
	}

	// (2.5s) Buttons fade in
	{
		Zenith_UI::Zenith_UIButton* pxNextBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NextLevelBtn");
		if (pxNextBtn)
		{
			float fProgress = EaseInRange(fT, s_fButtonsStart, s_fButtonsDuration);
			Zenith_Maths::Vector4 xColor = pxNextBtn->GetColor();
			xColor.w = fProgress;
			pxNextBtn->SetColor(xColor);
		}
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
// Settings Screen
// ============================================================================

void SetSettingsVisible(bool bVisible)
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	const char* aszSettingsElements[] = {
		"SettingsBg", "SettingsTitle"
	};
	for (const char* szName : aszSettingsElements)
	{
		Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
		if (pxElem) pxElem->SetVisible(bVisible);
	}

	const char* aszSettingsButtons[] = {
		"SettingsSoundBtn", "SettingsMusicBtn", "SettingsHapticsBtn", "SettingsBackBtn"
	};
	for (const char* szName : aszSettingsButtons)
	{
		Zenith_UI::Zenith_UIButton* pxBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szName);
		if (pxBtn) pxBtn->SetVisible(bVisible);
	}
}

void UpdateSettingsUI()
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	Zenith_UI::Zenith_UIButton* pxSoundBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SettingsSoundBtn");
	if (pxSoundBtn)
	{
		pxSoundBtn->SetText(m_xSaveData.bSoundEnabled ? "Sound: ON" : "Sound: OFF");
		pxSoundBtn->SetNormalColor(m_xSaveData.bSoundEnabled
			? Zenith_Maths::Vector4(0.2f, 0.4f, 0.3f, 1.0f)
			: Zenith_Maths::Vector4(0.3f, 0.15f, 0.15f, 1.0f));
	}

	Zenith_UI::Zenith_UIButton* pxMusicBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SettingsMusicBtn");
	if (pxMusicBtn)
	{
		pxMusicBtn->SetText(m_xSaveData.bMusicEnabled ? "Music: ON" : "Music: OFF");
		pxMusicBtn->SetNormalColor(m_xSaveData.bMusicEnabled
			? Zenith_Maths::Vector4(0.2f, 0.4f, 0.3f, 1.0f)
			: Zenith_Maths::Vector4(0.3f, 0.15f, 0.15f, 1.0f));
	}

	Zenith_UI::Zenith_UIButton* pxHapticsBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SettingsHapticsBtn");
	if (pxHapticsBtn)
	{
		pxHapticsBtn->SetText(m_xSaveData.bHapticsEnabled ? "Haptics: ON" : "Haptics: OFF");
		pxHapticsBtn->SetNormalColor(m_xSaveData.bHapticsEnabled
			? Zenith_Maths::Vector4(0.2f, 0.4f, 0.3f, 1.0f)
			: Zenith_Maths::Vector4(0.3f, 0.15f, 0.15f, 1.0f));
	}
}

static void OnSettingsClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->StartTransition(TILEPUZZLE_STATE_SETTINGS);
}

static void OnSettingsBackClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
		TilePuzzle_WriteSaveData, &pxSelf->m_xSaveData);
	pxSelf->StartTransition(TILEPUZZLE_STATE_MAIN_MENU);
}

static void OnToggleSoundClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_xSaveData.bSoundEnabled = !pxSelf->m_xSaveData.bSoundEnabled;
	pxSelf->UpdateSettingsUI();
}

static void OnToggleMusicClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_xSaveData.bMusicEnabled = !pxSelf->m_xSaveData.bMusicEnabled;
	pxSelf->UpdateSettingsUI();
}

static void OnToggleHapticsClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_xSaveData.bHapticsEnabled = !pxSelf->m_xSaveData.bHapticsEnabled;
	pxSelf->UpdateSettingsUI();
}

// ============================================================================
// Menu-Level Rendering with Meta-Game Info
// ============================================================================

void UpdateMainMenuUI()
{
	UpdateCoinDisplay();
	UpdateLivesDisplay();
	UpdateDailyStreakDisplay();

	// Star counter on main menu
	if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
	{
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
		Zenith_UI::Zenith_UIText* pxStars = xUI.FindElement<Zenith_UI::Zenith_UIText>("TotalStarsText");
		if (pxStars)
		{
			uint32_t uMaxStars = TilePuzzleSaveData::uMAX_LEVELS * 3;
			char szStars[48];
			snprintf(szStars, sizeof(szStars), "Stars: %u / %u", m_xSaveData.uTotalStars, uMaxStars);
			pxStars->SetText(szStars);
		}
	}

	// Regenerate lives periodically
	m_xSaveData.RegenerateLives(GetCurrentTimestamp());
}
