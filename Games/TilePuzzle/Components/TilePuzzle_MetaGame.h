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

void CreateCatCafeDisplayEntity()
{
	Zenith_SceneData* pxSceneData = m_xParentEntity.GetSceneData();
	if (!pxSceneData)
		return;

	uint32_t uColorIndex = 0;
	if (m_axCatCafeCats.GetSize() > 0)
	{
		uint32_t uCatID = m_axCatCafeCats.Get(m_uCatCafeCurrentIndex);
		uColorIndex = uCatID % TILEPUZZLE_COLOR_COUNT;
	}

	Zenith_MaterialAsset* pxMaterial = TilePuzzle::g_axCatCafeDisplayMaterials[uColorIndex].Get();
	if (!pxMaterial || !TilePuzzle::g_pxCatMeshGeometry)
		return;

	m_xCatCafeDisplayEntity = Zenith_Entity(pxSceneData, "CatCafeDisplay");
	Zenith_TransformComponent& xTransform = m_xCatCafeDisplayEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(0.f, 0.f, 0.f));
	xTransform.SetScale(Zenith_Maths::Vector3(2.f, 2.f, 2.f));

	Zenith_ModelComponent& xModel = m_xCatCafeDisplayEntity.AddComponent<Zenith_ModelComponent>();
	xModel.AddMeshEntry(*TilePuzzle::g_pxCatMeshGeometry, *pxMaterial);
}

void DestroyCatCafeDisplayEntity()
{
	if (m_xCatCafeDisplayEntity.IsValid())
	{
		Zenith_SceneManager::Destroy(m_xCatCafeDisplayEntity);
	}
}

void UpdateCatCafeDisplayMaterial()
{
	if (!m_xCatCafeDisplayEntity.IsValid())
		return;

	uint32_t uColorIndex = 0;
	if (m_axCatCafeCats.GetSize() > 0)
	{
		uint32_t uCatID = m_axCatCafeCats.Get(m_uCatCafeCurrentIndex);
		uColorIndex = uCatID % TILEPUZZLE_COLOR_COUNT;
	}

	Zenith_MaterialAsset* pxMaterial = TilePuzzle::g_axCatCafeDisplayMaterials[uColorIndex].Get();
	if (!pxMaterial)
		return;

	Zenith_ModelComponent& xModel = m_xCatCafeDisplayEntity.GetComponent<Zenith_ModelComponent>();
	xModel.m_xMeshEntries.Clear();
	xModel.AddMeshEntry(*TilePuzzle::g_pxCatMeshGeometry, *pxMaterial);
}

void SetCatCafeVisible(bool bVisible)
{
	if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
		return;

	Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

	const char* aszCatCafeElements[] = {
		"CatCafeTitle", "CatCafeCount",
		"CatProgressBg", "CatProgressFill",
		"CatCafeNavGroup"
	};
	for (const char* szName : aszCatCafeElements)
	{
		Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
		if (pxElem) pxElem->SetVisible(bVisible);
	}

	const char* aszInfoElements[] = {
		"CatCafeInfoName", "CatCafeInfoBreed", "CatCafeInfoLevel", "CatCafeEmpty", "CatCafeSwipeHint"
	};
	for (const char* szName : aszInfoElements)
	{
		Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
		if (pxElem) pxElem->SetVisible(false); // UpdateCatCafeUI decides which are shown
	}

	if (bVisible)
	{
		// Save camera state and set up front-facing view for 3D cat display
		if (m_xParentEntity.HasComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
			xCam.GetPosition(m_xCatCafeSavedCameraPos);
			m_fCatCafeSavedPitch = xCam.GetPitch();
			m_fCatCafeSavedYaw = xCam.GetYaw();
			xCam.SetPosition(Zenith_Maths::Vector3(0.f, 0.5f, -6.f));
			xCam.SetPitch(-0.05);
			xCam.SetYaw(0.0);
			m_xCameraBasePosition = Zenith_Maths::Vector3(0.f, 0.5f, -6.f);
		}

		// Build collected cats list
		m_axCatCafeCats.Clear();
		for (uint32_t i = 0; i < TilePuzzleSaveData::uMAX_CATS; ++i)
		{
			if (m_xSaveData.IsCatCollected(i))
				m_axCatCafeCats.PushBack(i);
		}

		if (m_axCatCafeCats.GetSize() == 0)
		{
			Zenith_UI::Zenith_UIElement* pxEmpty = xUI.FindElement<Zenith_UI::Zenith_UIElement>("CatCafeEmpty");
			if (pxEmpty) pxEmpty->SetVisible(true);
		}
		else
		{
			if (m_uCatCafeCurrentIndex >= m_axCatCafeCats.GetSize())
				m_uCatCafeCurrentIndex = 0;
			CreateCatCafeDisplayEntity();
		}

		UpdateCatCafeUI();
	}
	else
	{
		DestroyCatCafeDisplayEntity();

		// Restore camera state
		if (m_xParentEntity.HasComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
			xCam.SetPosition(m_xCatCafeSavedCameraPos);
			xCam.SetPitch(m_fCatCafeSavedPitch);
			xCam.SetYaw(m_fCatCafeSavedYaw);
			m_xCameraBasePosition = m_xCatCafeSavedCameraPos;
		}
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
		uint32_t uPercent = (m_xSaveData.uCatsCollectedCount * 100) / TilePuzzleSaveData::uMAX_CATS;
		char szCount[96];
		snprintf(szCount, sizeof(szCount), "%u / %u cats rescued (%u%%)",
			m_xSaveData.uCatsCollectedCount, TilePuzzleSaveData::uMAX_CATS, uPercent);
		pxCount->SetText(szCount);
	}

	// Update progress bar fill
	if (m_pxCatProgressFill)
	{
		float fFillRatio = static_cast<float>(m_xSaveData.uCatsCollectedCount) / static_cast<float>(TilePuzzleSaveData::uMAX_CATS);
		m_pxCatProgressFill->SetFillAmount(fFillRatio);
	}

	// Update the 3D display entity material to match the current cat
	UpdateCatCafeDisplayMaterial();

	if (m_axCatCafeCats.GetSize() == 0)
		return;

	uint32_t uCatID = m_axCatCafeCats.Get(m_uCatCafeCurrentIndex);
	const char* szCatName = s_aszCatNames[uCatID % s_uCatNameCount];
	const char* szBreed = s_aszCatBreeds[uCatID % s_uCatBreedCount];
	uint32_t uLevel = uCatID + 1;
	uint8_t uStars = m_xSaveData.GetStarRating(uLevel);

	Zenith_UI::Zenith_UIText* pxName = xUI.FindElement<Zenith_UI::Zenith_UIText>("CatCafeInfoName");
	if (pxName) { pxName->SetText(szCatName); pxName->SetFontSize(56.f); pxName->SetVisible(true); }

	Zenith_UI::Zenith_UIText* pxBreed = xUI.FindElement<Zenith_UI::Zenith_UIText>("CatCafeInfoBreed");
	if (pxBreed) { pxBreed->SetText(szBreed); pxBreed->SetFontSize(44.f); pxBreed->SetVisible(true); }

	Zenith_UI::Zenith_UIText* pxLevel = xUI.FindElement<Zenith_UI::Zenith_UIText>("CatCafeInfoLevel");
	if (pxLevel)
	{
		char szLevel[64];
		snprintf(szLevel, sizeof(szLevel), "Lvl %u | %s", uLevel, GetStarString(uStars));
		pxLevel->SetText(szLevel);
		pxLevel->SetFontSize(40.f);
		pxLevel->SetVisible(true);
	}

	// Show swipe hint only when there are multiple cats
	Zenith_UI::Zenith_UIText* pxHint = xUI.FindElement<Zenith_UI::Zenith_UIText>("CatCafeSwipeHint");
	if (pxHint)
	{
		Zenith_UI::Zenith_UIText* pxHintText = static_cast<Zenith_UI::Zenith_UIText*>(pxHint);
		pxHintText->SetFontSize(36.f);
		pxHint->SetVisible(m_axCatCafeCats.GetSize() > 1);
	}

	// Show/hide nav buttons based on prev/next availability
	Zenith_UI::Zenith_UIElement* pxPrev = xUI.FindElement<Zenith_UI::Zenith_UIElement>("CatCafePrevPage");
	if (pxPrev) pxPrev->SetVisible(m_uCatCafeCurrentIndex > 0);

	Zenith_UI::Zenith_UIElement* pxNext = xUI.FindElement<Zenith_UI::Zenith_UIElement>("CatCafeNextPage");
	if (pxNext) pxNext->SetVisible(m_uCatCafeCurrentIndex + 1 < m_axCatCafeCats.GetSize());
}

void HandleCatCafeInput(float /*fDeltaTime*/)
{
	if (m_axCatCafeCats.GetSize() <= 1)
		return;

	bool bDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
	Zenith_Maths::Vector2_64 xPos;
	Zenith_Input::GetMousePosition(xPos);
	float fX = static_cast<float>(xPos.x);

	if (bDown && !m_bCatCafeMouseWasDown)
	{
		m_fCatCafeSwipeStartX = fX;
		m_bCatCafeSwipeActive = true;
	}
	else if (!bDown && m_bCatCafeMouseWasDown && m_bCatCafeSwipeActive)
	{
		static constexpr float fSWIPE_THRESHOLD = 60.f;
		float fDelta = fX - m_fCatCafeSwipeStartX;
		if (fDelta > fSWIPE_THRESHOLD)
			OnCatCafePrevCat();
		else if (fDelta < -fSWIPE_THRESHOLD)
			OnCatCafeNextCat();
		m_bCatCafeSwipeActive = false;
	}

	m_bCatCafeMouseWasDown = bDown;
}

void OnCatCafePrevCat()
{
	if (m_uCatCafeCurrentIndex > 0)
	{
		m_uCatCafeCurrentIndex--;
		UpdateCatCafeUI();
	}
}

void OnCatCafeNextCat()
{
	if (m_uCatCafeCurrentIndex + 1 < m_axCatCafeCats.GetSize())
	{
		m_uCatCafeCurrentIndex++;
		UpdateCatCafeUI();
	}
}

static void OnCatCafeClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->m_uCatCafeCurrentIndex = 0;
	pxSelf->StartTransition(TILEPUZZLE_STATE_CAT_CAFE);
}

static void OnCatCafeBackClicked(void* pxUserData)
{
	TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
	pxSelf->StartTransition(TILEPUZZLE_STATE_MAIN_MENU);
}

static void OnCatCafePrevPageClicked(void* pxUserData)
{
	static_cast<TilePuzzle_Behaviour*>(pxUserData)->OnCatCafePrevCat();
}

static void OnCatCafeNextPageClicked(void* pxUserData)
{
	static_cast<TilePuzzle_Behaviour*>(pxUserData)->OnCatCafeNextCat();
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

		// Reparent star images to VictoryBg so they can be centered on screen,
		// but keep the star group in the content layout as a fixed-height spacer
		Zenith_UI::Zenith_UIElement* pxVictoryBg = xUI.FindElement<Zenith_UI::Zenith_UIElement>("VictoryBg");
		Zenith_UI::Zenith_UILayoutGroup* pxStarGroup = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("VictoryStarGroup");
		if (pxVictoryBg && pxStarGroup)
		{
			static const char* s_aszStarNames[] = { "VictoryStar0", "VictoryStar1", "VictoryStar2" };
			for (const char* szName : s_aszStarNames)
			{
				Zenith_UI::Zenith_UIImage* pxStar = xUI.FindElement<Zenith_UI::Zenith_UIImage>(szName);
				if (pxStar && pxStar->GetParent() == pxStarGroup)
				{
					pxStarGroup->RemoveChild(pxStar);
					pxVictoryBg->AddChild(pxStar);
				}
			}
			// Star group stays in layout as a spacer with the height of a star
			pxStarGroup->SetFitToContent(false);
			pxStarGroup->SetSize(0.f, 48.f);
		}

		// Configure VictoryContentGroup layout at runtime to ensure correct positioning
		// (scene generation values may differ from saved scene file)
		Zenith_UI::Zenith_UILayoutGroup* pxContentGroup = xUI.FindElement<Zenith_UI::Zenith_UILayoutGroup>("VictoryContentGroup");
		if (pxContentGroup)
		{
			pxContentGroup->SetFitToContent(false);
			pxContentGroup->SetPosition(0.f, 20.f);
			pxContentGroup->SetSize(TilePuzzleUI::fVICTORY_CONTENT_W, TilePuzzleUI::fVICTORY_CONTENT_H);
			pxContentGroup->SetSpacing(TilePuzzleUI::fVICTORY_CONTENT_SPACING);
		}

		// Keep button hidden until the animation makes it visible
		if (m_pxNextLevelBtn)
		{
			m_pxNextLevelBtn->SetVisible(false);
			m_pxNextLevelBtn->SetPosition(0.f, TilePuzzleUI::fNEXT_LEVEL_BTN_Y);
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
			float fBaseFontSize = (m_uVictoryStarRating >= 3) ? TilePuzzleUI::fVICTORY_TITLE_3STAR : ((m_uVictoryStarRating >= 2) ? TilePuzzleUI::fVICTORY_TITLE_2STAR : TilePuzzleUI::fVICTORY_TITLE_1STAR);
			float fFontSize = fBaseFontSize * fEased;
			if (fFontSize > 0.1f)
				pxTitle->SetFontSize(fFontSize);
		}
	}

	// (0.6s, 1.0s, 1.4s) Stars appear with staggered fade-in, positioned centered on screen
	{
		static const char* s_aszStarNames[] = { "VictoryStar0", "VictoryStar1", "VictoryStar2" };
		static constexpr float s_afStarStarts[] = { s_fStar0Start, s_fStar1Start, s_fStar2Start };
		static constexpr float s_fStarSize = 48.f;
		static constexpr float s_fStarSpacing = 8.f;

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

			// Position each star centered horizontally: star 0 = left, 1 = center, 2 = right
			float fTotalWidth = 3.f * s_fStarSize + 2.f * s_fStarSpacing;
			float fStartX = -fTotalWidth * 0.5f;
			float fX = fStartX + static_cast<float>(u) * (s_fStarSize + s_fStarSpacing) + s_fStarSize * 0.5f;
			pxStar->SetAnchorAndPivot(Zenith_UI::AnchorPreset::Center);
			pxStar->SetPosition(fX, -110.f);
			pxStar->SetSize(s_fStarSize, s_fStarSize);
			pxStar->SetVisible(true);

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
					pxConfig->m_xColorStart = Zenith_Maths::Vector4(1.0f, 0.85f, 0.1f, 1.0f);
				}
				xEmitter.GetComponent<Zenith_TransformComponent>().SetPosition(
					Zenith_Maths::Vector3(0.0f, s_fShapeHeight + 0.5f, 0.0f));
				xEmitter.GetComponent<Zenith_ParticleEmitterComponent>().Emit(10);
			}
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
			pxCatText->SetMaxWidth(TilePuzzleUI::fVICTORY_CONTENT_W);
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

	// (2.5s) Next Level button appears
	{
		if (m_pxNextLevelBtn)
		{
			float fProgress = EaseInRange(fT, s_fButtonsStart, s_fButtonsDuration);
			if (fProgress > 0.0f && !m_pxNextLevelBtn->IsVisible())
			{
				m_pxNextLevelBtn->SetText("Next Level");
				m_pxNextLevelBtn->SetNormalColor(Zenith_Maths::Vector4(0.15f, 0.4f, 0.2f, 1.0f));
				m_pxNextLevelBtn->SetHoverColor(Zenith_Maths::Vector4(0.25f, 0.55f, 0.3f, 1.0f));
				m_pxNextLevelBtn->SetPressedColor(Zenith_Maths::Vector4(0.1f, 0.3f, 0.15f, 1.0f));
				m_pxNextLevelBtn->SetOnClick(&OnNextLevelClicked, this);
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

	// Hint token counter
	if (m_pxHintTokenText)
	{
		char szTokens[16];
		snprintf(szTokens, sizeof(szTokens), "%u", m_xSaveData.uFreeHintTokens);
		m_pxHintTokenText->SetText(szTokens);
	}

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
		float fBannerY = fH - TilePuzzleUI::fWEEKLY_BANNER_BOTTOM_OFFSET;
		float fBannerH = TilePuzzleUI::fWEEKLY_BANNER_H;
		pxCanvas->SubmitQuad(
			Zenith_Maths::Vector4(10.0f, fBannerY, fW - 10.0f, fBannerY + fBannerH),
			Zenith_Maths::Vector4(0.1f, 0.1f, 0.2f, 0.85f));

		if (m_xSaveData.bWeeklyChallengeCompleted)
		{
			pxCanvas->SubmitText(
				"Weekly Challenge Complete!",
				Zenith_Maths::Vector2(20.0f, fBannerY + 10.0f),
				TilePuzzleUI::fWEEKLY_TITLE_FONT,
				Zenith_Maths::Vector4(0.3f, 1.0f, 0.3f, 1.0f));
		}
		else
		{
			pxCanvas->SubmitText(
				m_xSaveData.GetWeeklyChallengeDescription(),
				Zenith_Maths::Vector2(20.0f, fBannerY + 8.0f),
				TilePuzzleUI::fWEEKLY_DESC_FONT,
				Zenith_Maths::Vector4(1.0f, 0.9f, 0.7f, 1.0f));

			// Progress bar
			float fBarX = 20.0f;
			float fBarY2 = fBannerY + 38.0f;
			float fBarW = fW - 50.0f;
			float fBarH2 = TilePuzzleUI::fWEEKLY_BAR_H;
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
				TilePuzzleUI::fWEEKLY_PROGRESS_FONT,
				Zenith_Maths::Vector4(0.7f, 0.7f, 0.8f, 1.0f));
		}
	}
}
