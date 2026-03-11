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
		"CatProgressBg", "CatProgressFill"
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
			snprintf(szText, sizeof(szText), "=^.^= %s\n%s\nLvl %u %s", szName, szBreed, uLevel, GetStarString(uStars));
			pxCard->SetText(szText);
			pxCard->SetFontSize(20.f);

			// Tier-colored border: Tutorial(1-10)=bronze, Easy(11-25)=silver, Medium+(26+)=gold
			if (pxBg)
			{
				Zenith_Maths::Vector4 xBorderColor;
				if (uLevel <= 10)
					xBorderColor = Zenith_Maths::Vector4(0.6f, 0.4f, 0.2f, 0.8f); // Bronze
				else if (uLevel <= 25)
					xBorderColor = Zenith_Maths::Vector4(0.6f, 0.6f, 0.7f, 0.8f); // Silver
				else
					xBorderColor = Zenith_Maths::Vector4(0.8f, 0.7f, 0.2f, 0.8f); // Gold

				pxBg->SetColor(xBorderColor);
			}
		}
		else
		{
			pxCard->SetText("?");
			pxCard->SetFontSize(28.f);
			if (pxBg)
			{
				pxBg->SetColor(Zenith_Maths::Vector4(0.15f, 0.15f, 0.15f, 0.5f));
			}
		}
	}

	// Update cat collection progress bar fill amount
	if (m_pxCatProgressFill)
	{
		float fFillRatio = static_cast<float>(m_xSaveData.uCatsCollectedCount) / static_cast<float>(TilePuzzleSaveData::uMAX_CATS);
		m_pxCatProgressFill->SetFillAmount(fFillRatio);
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
		"VictoryContentGroup", "VictoryCatText", "VictoryCoinsText",
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

		// Configure VictoryContentGroup layout at runtime to ensure correct positioning
		// (scene generation values may differ from saved scene file)
		Zenith_UI::Zenith_UILayoutGroup* pxContentGroup = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("VictoryContentGroup");
		if (pxContentGroup)
		{
			pxContentGroup->SetFitToContent(false);
			pxContentGroup->SetPosition(0.f, 20.f);
			pxContentGroup->SetSize(460.f, 250.f);
			pxContentGroup->SetSpacing(20.f);
		}

		// Keep button hidden until the animation makes it visible
		if (m_pxNextLevelBtn)
		{
			m_pxNextLevelBtn->SetVisible(false);
			m_pxNextLevelBtn->SetPosition(0.f, 145.f);
			m_pxNextLevelBtn->SetNormalColor(Zenith_Maths::Vector4(0.15f, 0.4f, 0.2f, 1.0f));
			m_pxNextLevelBtn->SetHoverColor(Zenith_Maths::Vector4(0.25f, 0.55f, 0.3f, 1.0f));
			m_pxNextLevelBtn->SetPressedColor(Zenith_Maths::Vector4(0.1f, 0.3f, 0.15f, 1.0f));
			m_pxNextLevelBtn->SetGroupAlpha(1.0f);
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
		if (m_pxNextLevelBtn) m_pxNextLevelBtn->SetVisible(false);
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
			// Simulate scale via font size (scaled by star count: 1-star=40, 2-star=48, 3-star=56)
			float fBaseFontSize = (m_uVictoryStarRating >= 3) ? 56.0f : ((m_uVictoryStarRating >= 2) ? 48.0f : 40.0f);
			float fFontSize = fBaseFontSize * fEased;
			if (fFontSize > 0.1f)
				pxTitle->SetFontSize(fFontSize);
		}
	}

	// (0.6s, 1.0s, 1.4s) Stars appear with staggered fade-in
	{
		static const char* s_aszStarNames[] = { "VictoryStar0", "VictoryStar1", "VictoryStar2" };
		static constexpr float s_afStarStarts[] = { s_fStar0Start, s_fStar1Start, s_fStar2Start };

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

			// Fade in (size stays fixed at 48x48 so layout group remains stable)
			float fAlpha = Zenith_ApplyEasing(EASING_QUAD_OUT, fProgress);
			pxStar->SetColor(Zenith_Maths::Vector4(1.f, 0.85f, 0.1f, fAlpha));

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
			pxCatText->SetMaxWidth(460.f);
			pxCatText->SetText(szText);

			if (m_bVictoryNewBest)
			{
				// Gold pulsing animation for "New Best!" text
				float fPulseAlpha = fProgress * (0.7f + 0.3f * sinf(fT * 10.47f)); // ~0.6s period (2*PI/0.6)
				pxCatText->SetColor(Zenith_Maths::Vector4(1.0f, 0.85f, 0.2f, fPulseAlpha));
			}
			else
			{
				pxCatText->SetColor(Zenith_Maths::Vector4(0.9f, 0.7f, 0.5f, fProgress));
			}
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

	// (2.5s) Button appears
	{
		if (m_pxNextLevelBtn)
		{
			float fProgress = EaseInRange(fT, s_fButtonsStart, s_fButtonsDuration);
			if (fProgress > 0.0f && !m_pxNextLevelBtn->IsVisible())
			{
				m_pxNextLevelBtn->SetVisible(true);
			}
		}
	}
}

// ============================================================================
// Lives System
// ============================================================================

void SetLivesDisplayVisible(bool bVisible)
{
	if (m_pxLivesArea)
		m_pxLivesArea->SetVisible(bVisible);
}

void UpdateLivesDisplay()
{
	if (m_pxLivesText)
	{
		char szLives[64];
		snprintf(szLives, sizeof(szLives), "%u/%u",
			m_xSaveData.uLives, TilePuzzleSaveData::uMAX_LIVES);
		m_pxLivesText->SetText(szLives);
	}

	// Timer text shown below lives group when regenerating
	if (m_pxLivesTimerText)
	{
		bool bRegenerating = m_xSaveData.uLives < TilePuzzleSaveData::uMAX_LIVES;
		m_pxLivesTimerText->SetVisible(bRegenerating && m_eState == TILEPUZZLE_STATE_MAIN_MENU);
		if (bRegenerating)
		{
			uint32_t uTimerSecs = m_xSaveData.GetSecondsUntilNextLife(GetCurrentTimestamp());
			uint32_t uMins = uTimerSecs / 60;
			uint32_t uSecs = uTimerSecs % 60;
			char szTimer[32];
			snprintf(szTimer, sizeof(szTimer), "Next: %u:%02u", uMins, uSecs);
			m_pxLivesTimerText->SetText(szTimer);
		}
	}

	// Show/hide refill button based on lives count
	if (m_pxRefillBtn)
	{
		m_pxRefillBtn->SetVisible(m_eState == TILEPUZZLE_STATE_MAIN_MENU &&
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
	if (m_pxMenuCoinText)
	{
		char szCoins[32];
		snprintf(szCoins, sizeof(szCoins), "%u", m_xSaveData.uCoins);
		m_pxMenuCoinText->SetText(szCoins);
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
	if (m_pxStreakText)
	{
		char szStreak[32];
		snprintf(szStreak, sizeof(szStreak), "%u days", m_xSaveData.uDailyStreak);
		m_pxStreakText->SetText(szStreak);
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

	const char* aszSettingsInteractables[] = {
		"SettingsSoundBtn", "SettingsMusicBtn", "SettingsHapticsBtn", "SettingsCreditsBtn", "SettingsBackBtn"
	};
	for (const char* szName : aszSettingsInteractables)
	{
		Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement(szName);
		if (pxElem) pxElem->SetVisible(bVisible);
	}
}

void SyncSettingsToggles()
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	Zenith_UI::Zenith_UIToggle* pxSound = xUI.FindElement<Zenith_UI::Zenith_UIToggle>("SettingsSoundBtn");
	if (pxSound) pxSound->SetIsOn(m_xSaveData.bSoundEnabled);

	Zenith_UI::Zenith_UIToggle* pxMusic = xUI.FindElement<Zenith_UI::Zenith_UIToggle>("SettingsMusicBtn");
	if (pxMusic) pxMusic->SetIsOn(m_xSaveData.bMusicEnabled);

	Zenith_UI::Zenith_UIToggle* pxHaptics = xUI.FindElement<Zenith_UI::Zenith_UIToggle>("SettingsHapticsBtn");
	if (pxHaptics) pxHaptics->SetIsOn(m_xSaveData.bHapticsEnabled);
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

static void OnSettingSoundChanged(bool bNewValue, void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_xSaveData.bSoundEnabled = bNewValue;
}

static void OnSettingMusicChanged(bool bNewValue, void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_xSaveData.bMusicEnabled = bNewValue;
}

static void OnSettingHapticsChanged(bool bNewValue, void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_xSaveData.bHapticsEnabled = bNewValue;
}

static void OnCreditsClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_bCreditsOverlayActive = true;
	if (pxSelf->m_pxCreditsOverlay)
		pxSelf->m_pxCreditsOverlay->Show();
}

static void OnAchievementsClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->StartTransition(TILEPUZZLE_STATE_ACHIEVEMENTS);
}

void UpdateCreditsOverlay(float /*fDeltaTime*/)
{
	if (!m_bCreditsOverlayActive)
		return;

	// Overlay handles its own rendering — just check for dismiss
	bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
	if (bMouseDown && !m_bConfirmDialogMouseWasDown)
	{
		m_bCreditsOverlayActive = false;
		if (m_pxCreditsOverlay)
			m_pxCreditsOverlay->Hide();
	}
	m_bConfirmDialogMouseWasDown = bMouseDown;
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
	if (m_pxTotalStarsText)
	{
		uint32_t uMaxStars = TilePuzzleSaveData::uMAX_LEVELS * 3;
		char szStars[48];
		snprintf(szStars, sizeof(szStars), "%u / %u", m_xSaveData.uTotalStars, uMaxStars);
		m_pxTotalStarsText->SetText(szStars);
	}

	// Regenerate lives periodically
	m_xSaveData.RegenerateLives(GetCurrentTimestamp());

	// Weekly challenge management and display
	uint32_t uToday = GetCurrentDateYYYYMMDD();
	if (m_xSaveData.IsWeeklyChallengeExpired(uToday))
	{
		m_xSaveData.GenerateWeeklyChallenge(uToday);
	}

	// Weekly challenge banner via canvas
	Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
	if (pxCanvas && m_xSaveData.uWeeklyChallengeTarget > 0)
	{
		int32_t iWinWidth, iWinHeight;
		Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);
		float fW = static_cast<float>(iWinWidth);
		float fH = static_cast<float>(iWinHeight);

		// Banner at bottom of screen
		float fBannerY = fH - 90.0f;
		float fBannerH = 80.0f;
		pxCanvas->SubmitQuad(
			Zenith_Maths::Vector4(10.0f, fBannerY, fW - 10.0f, fBannerY + fBannerH),
			Zenith_Maths::Vector4(0.1f, 0.1f, 0.2f, 0.85f));

		if (m_xSaveData.bWeeklyChallengeCompleted)
		{
			pxCanvas->SubmitText(
				"Weekly Challenge Complete!",
				Zenith_Maths::Vector2(20.0f, fBannerY + 10.0f),
				24.0f,
				Zenith_Maths::Vector4(0.3f, 1.0f, 0.3f, 1.0f));
		}
		else
		{
			pxCanvas->SubmitText(
				m_xSaveData.GetWeeklyChallengeDescription(),
				Zenith_Maths::Vector2(20.0f, fBannerY + 8.0f),
				22.0f,
				Zenith_Maths::Vector4(1.0f, 0.9f, 0.7f, 1.0f));

			// Progress bar
			float fBarX = 20.0f;
			float fBarY2 = fBannerY + 38.0f;
			float fBarW = fW - 50.0f;
			float fBarH2 = 14.0f;
			pxCanvas->SubmitQuad(
				Zenith_Maths::Vector4(fBarX, fBarY2, fBarX + fBarW, fBarY2 + fBarH2),
				Zenith_Maths::Vector4(0.2f, 0.2f, 0.25f, 0.8f));

			float fProgress = static_cast<float>(m_xSaveData.uWeeklyChallengeProgress) /
				static_cast<float>(m_xSaveData.uWeeklyChallengeTarget);
			if (fProgress > 1.0f) fProgress = 1.0f;
			pxCanvas->SubmitQuad(
				Zenith_Maths::Vector4(fBarX, fBarY2, fBarX + fBarW * fProgress, fBarY2 + fBarH2),
				Zenith_Maths::Vector4(0.2f, 0.7f, 1.0f, 0.9f));

			char szProgress[48];
			snprintf(szProgress, sizeof(szProgress), "%u/%u  Reward: %u coins",
				m_xSaveData.uWeeklyChallengeProgress, m_xSaveData.uWeeklyChallengeTarget,
				m_xSaveData.uWeeklyChallengeReward);
			pxCanvas->SubmitText(
				szProgress,
				Zenith_Maths::Vector2(20.0f, fBannerY + 55.0f),
				20.0f,
				Zenith_Maths::Vector4(0.7f, 0.7f, 0.8f, 1.0f));
		}
	}
}
