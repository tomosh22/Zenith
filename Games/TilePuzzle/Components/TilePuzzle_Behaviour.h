#pragma once
/**
 * TilePuzzle_Behaviour.h - Main game coordinator
 *
 * A sliding tile puzzle where players drag colored shapes onto matching colored cats.
 * Shapes can be multi-cube polyominos. Win by eliminating all cats.
 *
 * Architecture:
 * - GameManager entity (persistent): camera + UI + script
 * - Puzzle scene (created/destroyed per level): floor, shapes, cats
 *
 * State machine: MAIN_MENU -> PLAYING -> LEVEL_COMPLETE -> (next level / menu)
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Input/Zenith_Input.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UIButton.h"

#include "TilePuzzle/Components/TilePuzzle_Types.h"
#include "TilePuzzle/Components/TilePuzzle_Rules.h"
#include "TilePuzzle/Components/TilePuzzle_LevelGenerator.h"
#include "TilePuzzle/Components/TilePuzzle_SaveData.h"
#include "SaveData/Zenith_SaveData.h"
#include "Input/Zenith_TouchInput.h"

#include <random>
#include <vector>
#include <unordered_map>
#include <cmath>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// ============================================================================
// TilePuzzle Resources - Global access
// Defined in TilePuzzle.cpp, initialized in Project_RegisterScriptBehaviours
// ============================================================================
namespace TilePuzzle
{
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern Flux_MeshGeometry* g_pxSphereGeometry;
	extern MaterialHandle g_xFloorMaterial;
	extern MaterialHandle g_xBlockerMaterial;
	extern MaterialHandle g_axShapeMaterials[TILEPUZZLE_COLOR_COUNT];
	extern MaterialHandle g_axCatMaterials[TILEPUZZLE_COLOR_COUNT];
	extern Zenith_Prefab* g_pxCellPrefab;
	extern Zenith_Prefab* g_pxShapeCubePrefab;
	extern Zenith_Prefab* g_pxCatPrefab;
}

// Forward declaration for level select button user data
class TilePuzzle_Behaviour;

// User data for level select buttons
struct TilePuzzleLevelButtonData
{
	TilePuzzle_Behaviour* pxBehaviour;
	uint32_t uLevelNumber;
};
static TilePuzzleLevelButtonData s_axLevelButtonData[20];

// Configuration constants
static constexpr uint32_t s_uMaxGridSize = 12;
static constexpr float s_fSlideAnimationDuration = 0.15f;
static constexpr float s_fEliminationDuration = 0.3f;
static constexpr float s_fCellSize = 1.0f;
static constexpr float s_fFloorHeight = 0.05f;
static constexpr float s_fShapeHeight = 0.25f;
static constexpr float s_fCatHeight = 0.35f;
static constexpr float s_fCatRadius = 0.35f;

// ============================================================================
// Main Behavior Class
// ============================================================================
class TilePuzzle_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(TilePuzzle_Behaviour)

	TilePuzzle_Behaviour() = delete;
	TilePuzzle_Behaviour(Zenith_Entity& /*xParentEntity*/)
		: m_eState(TILEPUZZLE_STATE_MAIN_MENU)
		, m_uCurrentLevelNumber(1)
		, m_uMoveCount(0)
		, m_iCursorX(0)
		, m_iCursorY(0)
		, m_iSelectedShapeIndex(-1)
		, m_fSlideProgress(0.0f)
		, m_eSlideDirection(TILEPUZZLE_DIR_NONE)
		, m_xRng(std::random_device{}())
		, m_iFocusIndex(0)
		, m_fLevelTimer(0.f)
		, m_uLevelSelectPage(0)
	{
		m_xSaveData.Reset();
	}

	~TilePuzzle_Behaviour() = default;

	// ========================================================================
	// Lifecycle Hooks
	// ========================================================================

	void OnAwake() ZENITH_FINAL override
	{
		// Load save data
		if (!Zenith_SaveData::Load("autosave", TilePuzzle_ReadSaveData, &m_xSaveData))
		{
			m_xSaveData.Reset();
		}
		m_uCurrentLevelNumber = m_xSaveData.uCurrentLevel;

		// Cache global resources (lightweight)
		m_pxCubeGeometry = TilePuzzle::g_pxCubeGeometry;
		m_pxSphereGeometry = TilePuzzle::g_pxSphereGeometry;
		m_xFloorMaterial = TilePuzzle::g_xFloorMaterial;
		m_xBlockerMaterial = TilePuzzle::g_xBlockerMaterial;

		for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
		{
			m_axShapeMaterials[i] = TilePuzzle::g_axShapeMaterials[i];
			m_axCatMaterials[i] = TilePuzzle::g_axCatMaterials[i];
		}

		// Create highlighted versions of shape materials with emissive glow
		auto& xRegistry = Zenith_AssetRegistry::Get();
		for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
		{
			Zenith_MaterialAsset* pxOriginal = m_axShapeMaterials[i].Get();
			Zenith_MaterialAsset* pxHighlighted = xRegistry.Create<Zenith_MaterialAsset>();

			pxHighlighted->SetName(pxOriginal->GetName() + "_Highlighted");
			pxHighlighted->SetBaseColor(pxOriginal->GetBaseColor());
			pxHighlighted->SetDiffuseTextureDirectly(pxOriginal->GetDiffuseTexture());

			Zenith_Maths::Vector4 xBaseColor = pxOriginal->GetBaseColor();
			pxHighlighted->SetEmissiveColor(Zenith_Maths::Vector3(xBaseColor.x, xBaseColor.y, xBaseColor.z));
			pxHighlighted->SetEmissiveIntensity(0.5f);

			m_axShapeMaterialsHighlighted[i].Set(pxHighlighted);
		}

		// Create highlighted floor material for cursor position
		{
			Zenith_MaterialAsset* pxFloorHighlighted = xRegistry.Create<Zenith_MaterialAsset>();
			pxFloorHighlighted->SetName("TilePuzzleFloor_Cursor");
			pxFloorHighlighted->SetDiffuseTextureDirectly(m_xFloorMaterial.Get()->GetDiffuseTexture());
			pxFloorHighlighted->SetBaseColor({ 150.f/255.f, 150.f/255.f, 180.f/255.f, 1.f });
			pxFloorHighlighted->SetEmissiveColor(Zenith_Maths::Vector3(0.5f, 0.5f, 0.7f));
			pxFloorHighlighted->SetEmissiveIntensity(0.3f);
			m_xFloorMaterialHighlighted.Set(pxFloorHighlighted);
		}

		// Wire up button callbacks
		bool bHasMenu = false;
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

			// New menu buttons
			Zenith_UI::Zenith_UIButton* pxContinueBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("ContinueButton");
			if (pxContinueBtn)
			{
				pxContinueBtn->SetOnClick(&OnContinueClicked, this);
				pxContinueBtn->SetFocused(true);
				bHasMenu = true;
			}

			Zenith_UI::Zenith_UIButton* pxLevelSelectBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("LevelSelectButton");
			if (pxLevelSelectBtn)
			{
				pxLevelSelectBtn->SetOnClick(&OnLevelSelectClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxNewGameBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NewGameButton");
			if (pxNewGameBtn)
			{
				pxNewGameBtn->SetOnClick(&OnNewGameClicked, this);
			}

			// Legacy: support old single Play button as fallback
			if (!bHasMenu)
			{
				Zenith_UI::Zenith_UIButton* pxPlayBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
				if (pxPlayBtn)
				{
					pxPlayBtn->SetOnClick(&OnContinueClicked, this);
					pxPlayBtn->SetFocused(true);
					bHasMenu = true;
				}
			}

			// Gameplay action buttons
			Zenith_UI::Zenith_UIButton* pxResetBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("ResetBtn");
			if (pxResetBtn)
			{
				pxResetBtn->SetOnClick(&OnResetClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxMenuBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuBtn");
			if (pxMenuBtn)
			{
				pxMenuBtn->SetOnClick(&OnMenuClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxNextLevelBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NextLevelBtn");
			if (pxNextLevelBtn)
			{
				pxNextLevelBtn->SetOnClick(&OnNextLevelClicked, this);
				pxNextLevelBtn->SetVisible(false);
			}

			// Level select buttons
			for (uint32_t i = 0; i < 20; ++i)
			{
				char szName[32];
				snprintf(szName, sizeof(szName), "LevelBtn_%u", i);
				Zenith_UI::Zenith_UIButton* pxLevelBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szName);
				if (pxLevelBtn)
				{
					s_axLevelButtonData[i].pxBehaviour = this;
					s_axLevelButtonData[i].uLevelNumber = 0;
					pxLevelBtn->SetOnClick(&OnLevelButtonClicked, &s_axLevelButtonData[i]);
				}
			}

			Zenith_UI::Zenith_UIButton* pxPrevPageBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("PrevPageButton");
			if (pxPrevPageBtn)
			{
				pxPrevPageBtn->SetOnClick(&OnPrevPageClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxNextPageBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NextPageButton");
			if (pxNextPageBtn)
			{
				pxNextPageBtn->SetOnClick(&OnNextPageClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxBackBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("BackButton");
			if (pxBackBtn)
			{
				pxBackBtn->SetOnClick(&OnBackClicked, this);
			}
		}

		if (bHasMenu)
		{
			// Start in main menu state
			m_eState = TILEPUZZLE_STATE_MAIN_MENU;
			SetMenuVisible(true);
			SetHUDVisible(false);
			SetLevelSelectVisible(false);
		}
		else
		{
			// No menu UI (gameplay scene) - start game directly
			StartGame();
		}
	}

	void OnStart() ZENITH_FINAL override
	{
		if (m_eState == TILEPUZZLE_STATE_MAIN_MENU)
		{
			SetMenuVisible(true);
			SetHUDVisible(false);
		}
	}

	void OnUpdate(const float fDeltaTime) ZENITH_FINAL override
	{
		switch (m_eState)
		{
		case TILEPUZZLE_STATE_MAIN_MENU:
			UpdateMenuInput();
			break;

		case TILEPUZZLE_STATE_LEVEL_SELECT:
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				m_eState = TILEPUZZLE_STATE_MAIN_MENU;
				SetMenuVisible(true);
				SetLevelSelectVisible(false);
				return;
			}
			break;

		case TILEPUZZLE_STATE_PLAYING:
			// Escape returns to menu
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			m_fLevelTimer += fDeltaTime;
			HandleInput();
			HandleDragInput();
			break;

		case TILEPUZZLE_STATE_SHAPE_SLIDING:
			UpdateSlideAnimation(fDeltaTime);
			break;

		case TILEPUZZLE_STATE_CHECK_ELIMINATION:
			CheckCatElimination();
			if (IsLevelComplete())
			{
				OnLevelCompleted();
			}
			else
			{
				m_eState = TILEPUZZLE_STATE_PLAYING;
			}
			break;

		case TILEPUZZLE_STATE_LEVEL_COMPLETE:
			// Escape returns to menu even from level complete
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			HandleLevelCompleteInput();
			break;

		case TILEPUZZLE_STATE_GENERATING:
			break;
		}

		// Only update visuals/UI while playing
		if (m_eState != TILEPUZZLE_STATE_MAIN_MENU && m_eState != TILEPUZZLE_STATE_LEVEL_SELECT)
		{
			UpdateVisuals(fDeltaTime);
			UpdateUI();
		}
	}

	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("TilePuzzle Game");
		ImGui::Separator();
		ImGui::Text("Level: %u", m_uCurrentLevelNumber);
		ImGui::Text("Moves: %u", m_uMoveCount);
		ImGui::Text("Cats remaining: %zu", CountRemainingCats());

		const char* aszStateNames[] = { "Menu", "Playing", "Sliding", "Checking", "Complete", "Generating", "LevelSelect" };
		ImGui::Text("State: %s", aszStateNames[m_eState]);

		if (ImGui::Button("New Level"))
		{
			StartNewLevel();
		}

		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			ResetLevel();
		}
#endif
	}

	// ========================================================================
	// Serialization
	// ========================================================================

	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_uCurrentLevelNumber;
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			xStream >> m_uCurrentLevelNumber;
		}
	}

private:
	// ========================================================================
	// Button Callbacks (static function pointers, NOT std::function)
	// ========================================================================

	static void OnContinueClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->m_uCurrentLevelNumber = pxSelf->m_xSaveData.uCurrentLevel;
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	static void OnLevelSelectClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->m_uLevelSelectPage = 0;
		pxSelf->m_eState = TILEPUZZLE_STATE_LEVEL_SELECT;
		pxSelf->SetMenuVisible(false);
		pxSelf->SetLevelSelectVisible(true);
		pxSelf->UpdateLevelSelectUI();
	}

	static void OnNewGameClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->m_xSaveData.Reset();
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &pxSelf->m_xSaveData);
		pxSelf->m_uCurrentLevelNumber = 1;
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	static void OnLevelButtonClicked(void* pxUserData)
	{
		TilePuzzleLevelButtonData* pxData = static_cast<TilePuzzleLevelButtonData*>(pxUserData);
		if (pxData->uLevelNumber == 0 || pxData->uLevelNumber > TilePuzzleSaveData::uMAX_LEVELS)
			return;

		// Only allow unlocked levels
		if (pxData->uLevelNumber > pxData->pxBehaviour->m_xSaveData.uHighestLevelReached)
			return;

		pxData->pxBehaviour->m_uCurrentLevelNumber = pxData->uLevelNumber;
		pxData->pxBehaviour->m_xSaveData.uCurrentLevel = pxData->uLevelNumber;
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	static void OnPrevPageClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		if (pxSelf->m_uLevelSelectPage > 0)
		{
			pxSelf->m_uLevelSelectPage--;
			pxSelf->UpdateLevelSelectUI();
		}
	}

	static void OnNextPageClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		if (pxSelf->m_uLevelSelectPage < 4)
		{
			pxSelf->m_uLevelSelectPage++;
			pxSelf->UpdateLevelSelectUI();
		}
	}

	static void OnBackClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->m_eState = TILEPUZZLE_STATE_MAIN_MENU;
		pxSelf->SetMenuVisible(true);
		pxSelf->SetLevelSelectVisible(false);
	}

	static void OnResetClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		if (pxSelf->m_eState == TILEPUZZLE_STATE_PLAYING)
		{
			pxSelf->ResetLevel();
		}
	}

	static void OnMenuClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->ReturnToMenu();
	}

	static void OnNextLevelClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		if (pxSelf->m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE)
		{
			pxSelf->NextLevel();
		}
	}

	// ========================================================================
	// State Transitions
	// ========================================================================

	void StartGame()
	{
		SetMenuVisible(false);
		SetHUDVisible(true);

		// Create puzzle scene for level entities
		m_xPuzzleScene = Zenith_SceneManager::CreateEmptyScene("Puzzle");
		Zenith_SceneManager::SetActiveScene(m_xPuzzleScene);

		GenerateNewLevel();
	}

	void StartNewLevel()
	{
		// Hide the next level button from level complete screen
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
			Zenith_UI::Zenith_UIButton* pxNextBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NextLevelBtn");
			if (pxNextBtn)
			{
				pxNextBtn->SetVisible(false);
			}
		}

		// Unload current puzzle scene (destroys all level entities automatically)
		if (m_xPuzzleScene.IsValid())
		{
			ClearEntityReferences();
			Zenith_SceneManager::UnloadScene(m_xPuzzleScene);
		}

		// Create fresh puzzle scene
		m_xPuzzleScene = Zenith_SceneManager::CreateEmptyScene("Puzzle");
		Zenith_SceneManager::SetActiveScene(m_xPuzzleScene);

		m_eState = TILEPUZZLE_STATE_GENERATING;
		GenerateNewLevel();
	}

	void ReturnToMenu()
	{
		// Save current progress before returning
		m_xSaveData.uCurrentLevel = m_uCurrentLevelNumber;
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// Unload puzzle scene (destroys all level entities automatically)
		if (m_xPuzzleScene.IsValid())
		{
			ClearEntityReferences();
			Zenith_SceneManager::UnloadScene(m_xPuzzleScene);
			m_xPuzzleScene = Zenith_Scene();
		}

		Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	void OnLevelCompleted()
	{
		m_eState = TILEPUZZLE_STATE_LEVEL_COMPLETE;

		// Update save data
		uint32_t uLevelIndex = m_uCurrentLevelNumber - 1;
		if (uLevelIndex < TilePuzzleSaveData::uMAX_LEVELS)
		{
			TilePuzzleLevelRecord& xRecord = m_xSaveData.axLevelRecords[uLevelIndex];
			xRecord.bCompleted = true;
			if (xRecord.uBestMoves == 0 || m_uMoveCount < xRecord.uBestMoves)
			{
				xRecord.uBestMoves = m_uMoveCount;
			}
			if (xRecord.fBestTime == 0.f || m_fLevelTimer < xRecord.fBestTime)
			{
				xRecord.fBestTime = m_fLevelTimer;
			}
		}

		// Unlock next level
		if (m_uCurrentLevelNumber >= m_xSaveData.uHighestLevelReached &&
			m_uCurrentLevelNumber < TilePuzzleSaveData::uMAX_LEVELS)
		{
			m_xSaveData.uHighestLevelReached = m_uCurrentLevelNumber + 1;
		}

		m_xSaveData.uCurrentLevel = m_uCurrentLevelNumber;

		// Auto-save
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// Show next level button
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
			Zenith_UI::Zenith_UIButton* pxNextBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NextLevelBtn");
			if (pxNextBtn)
			{
				pxNextBtn->SetVisible(true);
			}
		}
	}

	// ========================================================================
	// Touch/Swipe Input
	// ========================================================================

	void HandleDirectionInput(TilePuzzleDirection eDir)
	{
		if (m_iSelectedShapeIndex >= 0)
		{
			TryMoveShape(m_iSelectedShapeIndex, eDir);
		}
		else
		{
			int32_t iDeltaX, iDeltaY;
			TilePuzzleDirections::GetDelta(eDir, iDeltaX, iDeltaY);
			int32_t iNewX = m_iCursorX + iDeltaX;
			int32_t iNewY = m_iCursorY + iDeltaY;

			if (iNewX >= 0 && iNewX < static_cast<int32_t>(m_xCurrentLevel.uGridWidth) &&
				iNewY >= 0 && iNewY < static_cast<int32_t>(m_xCurrentLevel.uGridHeight))
			{
				uint32_t uCellIndex = iNewY * m_xCurrentLevel.uGridWidth + iNewX;
				if (m_xCurrentLevel.aeCells[uCellIndex] == TILEPUZZLE_CELL_FLOOR)
				{
					m_iCursorX = iNewX;
					m_iCursorY = iNewY;
				}
			}
		}
	}

	void ToggleSelection()
	{
		if (m_iSelectedShapeIndex >= 0)
		{
			m_iSelectedShapeIndex = -1;
		}
		else
		{
			m_iSelectedShapeIndex = GetShapeAtPosition(m_iCursorX, m_iCursorY);
		}
	}

	void HandleDragInput()
	{
		if (m_eState != TILEPUZZLE_STATE_PLAYING && !m_bDragging)
			return;

		bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
		Zenith_Maths::Vector2_64 xMousePos64;
		Zenith_Input::GetMousePosition(xMousePos64);
		float fScreenX = static_cast<float>(xMousePos64.x);
		float fScreenY = static_cast<float>(xMousePos64.y);

		if (bMouseDown && !m_bMouseWasDown)
		{
			// Mouse just pressed - try to start drag
			int32_t iGridX, iGridY;
			if (ScreenToGrid(fScreenX, fScreenY, iGridX, iGridY))
			{
				int32_t iShape = GetShapeAtPosition(iGridX, iGridY);
				if (iShape >= 0)
				{
					m_bDragging = true;
					m_iDragShapeIndex = iShape;
					m_iSelectedShapeIndex = iShape;

					const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShape];
					m_iDragGrabOffsetX = iGridX - xShape.iOriginX;
					m_iDragGrabOffsetY = iGridY - xShape.iOriginY;
				}
			}
		}
		else if (bMouseDown && m_bDragging)
		{
			// Mouse held - move shape toward cursor
			int32_t iCursorGridX, iCursorGridY;
			if (ScreenToGrid(fScreenX, fScreenY, iCursorGridX, iCursorGridY))
			{
				int32_t iTargetX = iCursorGridX - m_iDragGrabOffsetX;
				int32_t iTargetY = iCursorGridY - m_iDragGrabOffsetY;

				for (int32_t i = 0; i < 4; ++i)
				{
					if (m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE || m_bPendingLevelComplete)
						break;

					const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iDragShapeIndex];
					int32_t iDX = iTargetX - xShape.iOriginX;
					int32_t iDY = iTargetY - xShape.iOriginY;

					if (iDX == 0 && iDY == 0)
						break;

					TilePuzzleDirection eDir = TILEPUZZLE_DIR_NONE;
					if (abs(iDX) >= abs(iDY))
						eDir = (iDX > 0) ? TILEPUZZLE_DIR_RIGHT : TILEPUZZLE_DIR_LEFT;
					else
						eDir = (iDY > 0) ? TILEPUZZLE_DIR_DOWN : TILEPUZZLE_DIR_UP;

					if (!MoveShapeImmediate(m_iDragShapeIndex, eDir))
						break;
				}
			}
		}
		else if (!bMouseDown && m_bDragging)
		{
			// Mouse released - snap to exact grid position and end drag
			SnapShapeVisuals(m_iDragShapeIndex);
			m_bDragging = false;
			m_iDragShapeIndex = -1;
			m_iSelectedShapeIndex = -1;

			if (m_bPendingLevelComplete)
			{
				m_bPendingLevelComplete = false;
				OnLevelCompleted();
			}
		}

		m_bMouseWasDown = bMouseDown;
	}

	// ========================================================================
	// Menu UI
	// ========================================================================

	void SetMenuVisible(bool bVisible)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIText* pxTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("MenuTitle");
		if (pxTitle) pxTitle->SetVisible(bVisible);

		// New menu buttons
		const char* aszMenuBtns[] = { "ContinueButton", "LevelSelectButton", "NewGameButton" };
		for (const char* szName : aszMenuBtns)
		{
			Zenith_UI::Zenith_UIButton* pxBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szName);
			if (pxBtn) pxBtn->SetVisible(bVisible);
		}

		// Background
		Zenith_UI::Zenith_UIElement* pxBg = xUI.FindElement<Zenith_UI::Zenith_UIElement>("MenuBackground");
		if (pxBg) pxBg->SetVisible(bVisible);

		// Legacy fallback
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay) pxPlay->SetVisible(bVisible);
	}

	void SetHUDVisible(bool bVisible)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		const char* aszHUDElements[] = {
			"Title", "ControlsHeader", "MoveInstr", "ResetInstr",
			"GoalHeader", "GoalDesc", "Status", "Progress", "WinText"
		};

		for (const char* szName : aszHUDElements)
		{
			Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
			if (pxText) pxText->SetVisible(bVisible);
		}
	}

	void SetLevelSelectVisible(bool bVisible)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		const char* aszElements[] = {
			"LevelSelectTitle", "PageText",
			"PrevPageButton", "NextPageButton", "BackButton"
		};
		for (const char* szName : aszElements)
		{
			Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
			if (pxElem) pxElem->SetVisible(bVisible);
		}

		// Level select background
		Zenith_UI::Zenith_UIElement* pxBg = xUI.FindElement<Zenith_UI::Zenith_UIElement>("LevelSelectBg");
		if (pxBg) pxBg->SetVisible(bVisible);

		for (uint32_t i = 0; i < 20; ++i)
		{
			char szName[32];
			snprintf(szName, sizeof(szName), "LevelBtn_%u", i);
			Zenith_UI::Zenith_UIButton* pxBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szName);
			if (pxBtn) pxBtn->SetVisible(bVisible);
		}
	}

	void UpdateLevelSelectUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// Update page text
		Zenith_UI::Zenith_UIText* pxPageText = xUI.FindElement<Zenith_UI::Zenith_UIText>("PageText");
		if (pxPageText)
		{
			char szPage[32];
			snprintf(szPage, sizeof(szPage), "Page %u / 5", m_uLevelSelectPage + 1);
			pxPageText->SetText(szPage);
		}

		// Update level buttons
		uint32_t uStartLevel = m_uLevelSelectPage * 20 + 1;
		for (uint32_t i = 0; i < 20; ++i)
		{
			uint32_t uLevel = uStartLevel + i;
			char szBtnName[32];
			snprintf(szBtnName, sizeof(szBtnName), "LevelBtn_%u", i);
			Zenith_UI::Zenith_UIButton* pxBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szBtnName);
			if (!pxBtn) continue;

			// Update user data for callback
			s_axLevelButtonData[i].pxBehaviour = this;
			s_axLevelButtonData[i].uLevelNumber = uLevel;

			// Update label
			char szLabel[16];
			if (uLevel > TilePuzzleSaveData::uMAX_LEVELS)
			{
				pxBtn->SetVisible(false);
				continue;
			}

			pxBtn->SetVisible(true);

			uint32_t uIndex = uLevel - 1;
			if (m_xSaveData.axLevelRecords[uIndex].bCompleted)
			{
				snprintf(szLabel, sizeof(szLabel), "%u *", uLevel);
			}
			else
			{
				snprintf(szLabel, sizeof(szLabel), "%u", uLevel);
			}
			pxBtn->SetText(szLabel);

			// Color based on lock state
			bool bUnlocked = uLevel <= m_xSaveData.uHighestLevelReached;
			if (bUnlocked)
			{
				pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.2f, 0.3f, 0.5f, 1.f));
			}
			else
			{
				pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.15f, 0.15f, 0.15f, 1.f));
			}
		}
	}

	void UpdateMenuInput()
	{
		static constexpr int32_t s_iButtonCount = 3;

		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_UP) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_W))
		{
			m_iFocusIndex = (m_iFocusIndex - 1 + s_iButtonCount) % s_iButtonCount;
		}
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_DOWN) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_S))
		{
			m_iFocusIndex = (m_iFocusIndex + 1) % s_iButtonCount;
		}

		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

			const char* aszBtnNames[] = { "ContinueButton", "LevelSelectButton", "NewGameButton" };
			for (int32_t i = 0; i < s_iButtonCount; ++i)
			{
				Zenith_UI::Zenith_UIButton* pxBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>(aszBtnNames[i]);
				if (pxBtn) pxBtn->SetFocused(m_iFocusIndex == i);
			}
		}
	}

	// ========================================================================
	// Entity Reference Management
	// ========================================================================

	void ClearEntityReferences()
	{
		m_axFloorEntityIDs.clear();
		for (auto& xShape : m_xCurrentLevel.axShapes)
		{
			xShape.axCubeEntityIDs.clear();
		}
		for (auto& xCat : m_xCurrentLevel.axCats)
		{
			xCat.uEntityID = Zenith_EntityID();
		}
	}

	// ========================================================================
	// Game State
	// ========================================================================
	TilePuzzleGameState m_eState;
	TilePuzzleLevelData m_xCurrentLevel;
	uint32_t m_uCurrentLevelNumber;
	uint32_t m_uMoveCount;

	// Selection/Cursor
	int32_t m_iCursorX;
	int32_t m_iCursorY;
	int32_t m_iPreviousCursorX = -1;
	int32_t m_iPreviousCursorY = -1;
	int32_t m_iSelectedShapeIndex;

	// Animation
	float m_fSlideProgress;
	TilePuzzleDirection m_eSlideDirection;
	int32_t m_iSlidingShapeIndex = -1;
	Zenith_Maths::Vector3 m_xSlideStartPos;
	Zenith_Maths::Vector3 m_xSlideEndPos;

	// Random number generator
	std::mt19937 m_xRng;

	// Entity IDs - floor entities indexed by grid position (y * 1000 + x)
	std::unordered_map<uint32_t, Zenith_EntityID> m_axFloorEntityIDs; // #TODO: Replace with engine hash map

	// Cached resources
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* m_pxSphereGeometry = nullptr;
	MaterialHandle m_xFloorMaterial;
	MaterialHandle m_xFloorMaterialHighlighted;
	MaterialHandle m_xBlockerMaterial;
	MaterialHandle m_axShapeMaterials[TILEPUZZLE_COLOR_COUNT];
	MaterialHandle m_axShapeMaterialsHighlighted[TILEPUZZLE_COLOR_COUNT];
	MaterialHandle m_axCatMaterials[TILEPUZZLE_COLOR_COUNT];

	// Selection tracking
	int32_t m_iPreviousSelectedShapeIndex = -1;

	// Drag state
	bool m_bDragging = false;
	bool m_bMouseWasDown = false;
	bool m_bPendingLevelComplete = false;
	int32_t m_iDragShapeIndex = -1;
	int32_t m_iDragGrabOffsetX = 0;
	int32_t m_iDragGrabOffsetY = 0;

	// Menu state
	int32_t m_iFocusIndex;

	// Scene handle for the puzzle scene (created/destroyed on transitions)
	Zenith_Scene m_xPuzzleScene;

	// Save data
	TilePuzzleSaveData m_xSaveData;
	float m_fLevelTimer;
	uint32_t m_uLevelSelectPage;

	// ========================================================================
	// Level Generation
	// ========================================================================
	void GenerateNewLevel()
	{
		// Generate a solvable level using the level generator
		TilePuzzle_LevelGenerator::GenerateLevel(
			m_xCurrentLevel, m_xRng, m_uCurrentLevelNumber);

		CreateLevelVisuals();

		// Reset cursor to first draggable shape
		m_iCursorX = 1;
		m_iCursorY = 1;
		for (const auto& xShape : m_xCurrentLevel.axShapes)
		{
			if (xShape.pxDefinition && xShape.pxDefinition->bDraggable)
			{
				m_iCursorX = xShape.iOriginX;
				m_iCursorY = xShape.iOriginY;
				break;
			}
		}

		m_uMoveCount = 0;
		m_iSelectedShapeIndex = -1;
		m_iPreviousSelectedShapeIndex = -1;
		m_iPreviousCursorX = -1;
		m_iPreviousCursorY = -1;
		m_bDragging = false;
		m_bMouseWasDown = false;
		m_bPendingLevelComplete = false;
		m_iDragShapeIndex = -1;
		m_iDragGrabOffsetX = 0;
		m_iDragGrabOffsetY = 0;
		m_eState = TILEPUZZLE_STATE_PLAYING;

		UpdateSelectionHighlight();
	}

	void ResetLevel()
	{
		StartNewLevel();
	}

	void NextLevel()
	{
		m_uCurrentLevelNumber++;
		m_xSaveData.uCurrentLevel = m_uCurrentLevelNumber;
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);
		StartNewLevel();
	}

	// ========================================================================
	// Input Handling
	// ========================================================================
	void HandleInput()
	{
		if (m_bDragging)
			return;

		// Reset level
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_R))
		{
			ResetLevel();
			return;
		}

		// Get keyboard direction
		TilePuzzleDirection eDir = TILEPUZZLE_DIR_NONE;
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_W) || Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_UP))
			eDir = TILEPUZZLE_DIR_DOWN;
		else if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_S) || Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_DOWN))
			eDir = TILEPUZZLE_DIR_UP;
		else if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_A) || Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_LEFT))
			eDir = TILEPUZZLE_DIR_LEFT;
		else if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_D) || Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_RIGHT))
			eDir = TILEPUZZLE_DIR_RIGHT;

		// Selection toggle
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE))
		{
			ToggleSelection();
			return;
		}

		if (eDir != TILEPUZZLE_DIR_NONE)
		{
			HandleDirectionInput(eDir);
		}
	}

	void HandleLevelCompleteInput()
	{
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_N) ||
			Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_SPACE))
		{
			NextLevel();
		}
	}

	int32_t GetShapeAtPosition(int32_t iX, int32_t iY) const
	{
		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[i];
			if (!xShape.pxDefinition->bDraggable)
				continue;

			for (const auto& xOffset : xShape.pxDefinition->axCells)
			{
				int32_t iCellX = xShape.iOriginX + xOffset.iX;
				int32_t iCellY = xShape.iOriginY + xOffset.iY;
				if (iCellX == iX && iCellY == iY)
				{
					return static_cast<int32_t>(i);
				}
			}
		}
		return -1;
	}

	// ========================================================================
	// Movement Logic
	// ========================================================================
	bool TryMoveShape(int32_t iShapeIndex, TilePuzzleDirection eDir)
	{
		if (iShapeIndex < 0 || iShapeIndex >= static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			return false;

		TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];
		if (!xShape.pxDefinition->bDraggable)
			return false;

		int32_t iDeltaX, iDeltaY;
		TilePuzzleDirections::GetDelta(eDir, iDeltaX, iDeltaY);

		if (!CanMoveShape(iShapeIndex, iDeltaX, iDeltaY))
			return false;

		// Start slide animation
		m_iSlidingShapeIndex = iShapeIndex;
		m_eSlideDirection = eDir;
		m_fSlideProgress = 0.0f;
		m_xSlideStartPos = GridToWorld(static_cast<float>(xShape.iOriginX), static_cast<float>(xShape.iOriginY), s_fShapeHeight);
		m_xSlideEndPos = GridToWorld(static_cast<float>(xShape.iOriginX + iDeltaX), static_cast<float>(xShape.iOriginY + iDeltaY), s_fShapeHeight);

		xShape.iOriginX += iDeltaX;
		xShape.iOriginY += iDeltaY;

		m_uMoveCount++;
		m_eState = TILEPUZZLE_STATE_SHAPE_SLIDING;
		return true;
	}

	bool MoveShapeImmediate(int32_t iShapeIndex, TilePuzzleDirection eDir)
	{
		if (iShapeIndex < 0 || iShapeIndex >= static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			return false;

		TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];
		if (!xShape.pxDefinition->bDraggable)
			return false;

		int32_t iDeltaX, iDeltaY;
		TilePuzzleDirections::GetDelta(eDir, iDeltaX, iDeltaY);

		if (!CanMoveShape(iShapeIndex, iDeltaX, iDeltaY))
			return false;

		xShape.iOriginX += iDeltaX;
		xShape.iOriginY += iDeltaY;
		m_uMoveCount++;

		CheckCatElimination();
		if (IsLevelComplete())
		{
			m_bPendingLevelComplete = true;
		}

		return true;
	}

	bool CanMoveShape(int32_t iShapeIndex, int32_t iDeltaX, int32_t iDeltaY) const
	{
		const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];

		// Build ShapeState array for all draggable shapes
		Zenith_Vector<TilePuzzle_Rules::ShapeState> axDraggableStates;
		size_t uMovingDraggableIdx = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xOther = m_xCurrentLevel.axShapes[i];
			if (!xOther.pxDefinition || !xOther.pxDefinition->bDraggable)
				continue;

			if (static_cast<int32_t>(i) == iShapeIndex)
				uMovingDraggableIdx = axDraggableStates.GetSize();

			TilePuzzle_Rules::ShapeState xState;
			xState.pxDefinition = xOther.pxDefinition;
			xState.iOriginX = xOther.iOriginX;
			xState.iOriginY = xOther.iOriginY;
			xState.eColor = xOther.eColor;
			axDraggableStates.PushBack(xState);
		}

		// Build CatState array and elimination mask
		Zenith_Vector<TilePuzzle_Rules::CatState> axCatStates;
		uint32_t uEliminatedMask = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			TilePuzzle_Rules::CatState xCatState;
			xCatState.iGridX = m_xCurrentLevel.axCats[i].iGridX;
			xCatState.iGridY = m_xCurrentLevel.axCats[i].iGridY;
			xCatState.eColor = m_xCurrentLevel.axCats[i].eColor;
			axCatStates.PushBack(xCatState);

			if (m_xCurrentLevel.axCats[i].bEliminated)
				uEliminatedMask |= (1u << i);
		}

		int32_t iNewOriginX = xShape.iOriginX + iDeltaX;
		int32_t iNewOriginY = xShape.iOriginY + iDeltaY;

		return TilePuzzle_Rules::CanMoveShape(
			m_xCurrentLevel,
			axDraggableStates.GetDataPointer(), axDraggableStates.GetSize(),
			uMovingDraggableIdx,
			iNewOriginX, iNewOriginY,
			axCatStates.GetDataPointer(), axCatStates.GetSize(),
			uEliminatedMask);
	}

	// ========================================================================
	// Cat Elimination
	// ========================================================================
	void CheckCatElimination()
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		// Build ShapeState array for all draggable shapes
		Zenith_Vector<TilePuzzle_Rules::ShapeState> axDraggableStates;
		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[i];
			if (!xShape.pxDefinition || !xShape.pxDefinition->bDraggable)
				continue;

			TilePuzzle_Rules::ShapeState xState;
			xState.pxDefinition = xShape.pxDefinition;
			xState.iOriginX = xShape.iOriginX;
			xState.iOriginY = xShape.iOriginY;
			xState.eColor = xShape.eColor;
			axDraggableStates.PushBack(xState);
		}

		// Build CatState array and current elimination mask
		Zenith_Vector<TilePuzzle_Rules::CatState> axCatStates;
		uint32_t uOldMask = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			TilePuzzle_Rules::CatState xCatState;
			xCatState.iGridX = m_xCurrentLevel.axCats[i].iGridX;
			xCatState.iGridY = m_xCurrentLevel.axCats[i].iGridY;
			xCatState.eColor = m_xCurrentLevel.axCats[i].eColor;
			axCatStates.PushBack(xCatState);

			if (m_xCurrentLevel.axCats[i].bEliminated)
				uOldMask |= (1u << i);
		}

		uint32_t uNewlyEliminated = TilePuzzle_Rules::ComputeNewlyEliminatedCats(
			axDraggableStates.GetDataPointer(), axDraggableStates.GetSize(),
			axCatStates.GetDataPointer(), axCatStates.GetSize(),
			uOldMask);

		// Apply elimination: set bEliminated and destroy entities for newly eliminated cats
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			if (!(uNewlyEliminated & (1u << i)))
				continue;

			TilePuzzleCatData& xCat = m_xCurrentLevel.axCats[i];
			xCat.bEliminated = true;

			if (xCat.uEntityID.IsValid() && pxSceneData->EntityExists(xCat.uEntityID))
			{
				Zenith_Entity xCatEntity = pxSceneData->GetEntity(xCat.uEntityID);
				if (xCatEntity.IsValid())
				{
					Zenith_SceneManager::Destroy(xCatEntity, 0.3f);
				}
			}
			xCat.uEntityID = Zenith_EntityID();
		}
	}

	bool IsLevelComplete() const
	{
		uint32_t uEliminatedMask = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			if (m_xCurrentLevel.axCats[i].bEliminated)
				uEliminatedMask |= (1u << i);
		}
		return TilePuzzle_Rules::AreAllCatsEliminated(
			uEliminatedMask, static_cast<uint32_t>(m_xCurrentLevel.axCats.size()));
	}

	size_t CountRemainingCats() const
	{
		size_t uCount = 0;
		for (const auto& xCat : m_xCurrentLevel.axCats)
		{
			if (!xCat.bEliminated)
				uCount++;
		}
		return uCount;
	}

	// ========================================================================
	// Animation
	// ========================================================================
	void UpdateSlideAnimation(float fDeltaTime)
	{
		m_fSlideProgress += fDeltaTime / s_fSlideAnimationDuration;

		if (m_fSlideProgress >= 1.0f)
		{
			m_fSlideProgress = 1.0f;
			m_iSlidingShapeIndex = -1;
			m_eState = TILEPUZZLE_STATE_CHECK_ELIMINATION;
		}
	}

	void SnapShapeVisuals(int32_t iShapeIndex)
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];
		for (size_t i = 0; i < xShape.axCubeEntityIDs.size(); ++i)
		{
			if (!pxSceneData->EntityExists(xShape.axCubeEntityIDs[i]))
				continue;

			Zenith_Entity xCube = pxSceneData->GetEntity(xShape.axCubeEntityIDs[i]);
			if (!xCube.IsValid())
				continue;

			const TilePuzzleCellOffset& xOffset = xShape.pxDefinition->axCells[i];
			float fX = static_cast<float>(xShape.iOriginX + xOffset.iX);
			float fY = static_cast<float>(xShape.iOriginY + xOffset.iY);

			Zenith_TransformComponent& xTransform = xCube.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(GridToWorld(fX, fY, s_fShapeHeight));
		}
	}

	// ========================================================================
	// Rendering
	// ========================================================================
	void CreateLevelVisuals()
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData || !TilePuzzle::g_pxCellPrefab || !TilePuzzle::g_pxCellPrefab->IsValid())
		{
			return;
		}

		// Create floor cells
		for (uint32_t y = 0; y < m_xCurrentLevel.uGridHeight; ++y)
		{
			for (uint32_t x = 0; x < m_xCurrentLevel.uGridWidth; ++x)
			{
				uint32_t uIdx = y * m_xCurrentLevel.uGridWidth + x;
				if (m_xCurrentLevel.aeCells[uIdx] == TILEPUZZLE_CELL_FLOOR)
				{
					Zenith_Entity xFloorEntity = TilePuzzle::g_pxCellPrefab->Instantiate(pxSceneData, "Floor");
					if (!xFloorEntity.IsValid())
						continue;

					Zenith_TransformComponent& xTransform = xFloorEntity.GetComponent<Zenith_TransformComponent>();
					xTransform.SetPosition(GridToWorld(static_cast<float>(x), static_cast<float>(y), 0.0f));
					xTransform.SetScale(Zenith_Maths::Vector3(s_fCellSize * 0.95f, s_fFloorHeight, s_fCellSize * 0.95f));

					Zenith_ModelComponent& xModel = xFloorEntity.AddComponent<Zenith_ModelComponent>();
					xModel.AddMeshEntry(*m_pxCubeGeometry, *m_xFloorMaterial.Get());

					uint32_t uKey = y * 1000 + x;
					m_axFloorEntityIDs[uKey] = xFloorEntity.GetEntityID();
				}
			}
		}

		// Create shape visuals
		for (auto& xShape : m_xCurrentLevel.axShapes)
		{
			xShape.axCubeEntityIDs.clear();

			Zenith_MaterialAsset* pxMaterial = m_xBlockerMaterial.Get();
			if (xShape.pxDefinition->bDraggable && xShape.eColor < TILEPUZZLE_COLOR_COUNT)
			{
				pxMaterial = m_axShapeMaterials[xShape.eColor].Get();
			}

			for (const auto& xOffset : xShape.pxDefinition->axCells)
			{
				float fX = static_cast<float>(xShape.iOriginX + xOffset.iX);
				float fY = static_cast<float>(xShape.iOriginY + xOffset.iY);

				Zenith_Entity xCubeEntity = TilePuzzle::g_pxShapeCubePrefab->Instantiate(pxSceneData, "ShapeCube");
				Zenith_TransformComponent& xTransform = xCubeEntity.GetComponent<Zenith_TransformComponent>();
				xTransform.SetPosition(GridToWorld(fX, fY, s_fShapeHeight));
				xTransform.SetScale(Zenith_Maths::Vector3(s_fCellSize * 0.85f, s_fShapeHeight * 2.0f, s_fCellSize * 0.85f));

				Zenith_ModelComponent& xModel = xCubeEntity.AddComponent<Zenith_ModelComponent>();
				xModel.AddMeshEntry(*m_pxCubeGeometry, *pxMaterial);

				xShape.axCubeEntityIDs.push_back(xCubeEntity.GetEntityID());
			}
		}

		// Create cat visuals
		for (auto& xCat : m_xCurrentLevel.axCats)
		{
			Zenith_Entity xCatEntity = TilePuzzle::g_pxCatPrefab->Instantiate(pxSceneData, "Cat");
			Zenith_TransformComponent& xTransform = xCatEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(GridToWorld(static_cast<float>(xCat.iGridX), static_cast<float>(xCat.iGridY), s_fCatHeight));
			xTransform.SetScale(Zenith_Maths::Vector3(s_fCatRadius * 2.0f));

			Zenith_ModelComponent& xModel = xCatEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*m_pxSphereGeometry, *m_axCatMaterials[xCat.eColor].Get());

			xCat.uEntityID = xCatEntity.GetEntityID();
		}
	}

	void UpdateVisuals(float fDeltaTime)
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		// Update shape positions (for keyboard sliding animation)
		if (m_eState == TILEPUZZLE_STATE_SHAPE_SLIDING && m_iSlidingShapeIndex >= 0)
		{
			TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iSlidingShapeIndex];

			Zenith_Maths::Vector3 xCurrentPos = m_xSlideStartPos + (m_xSlideEndPos - m_xSlideStartPos) * m_fSlideProgress;

			for (size_t i = 0; i < xShape.axCubeEntityIDs.size(); ++i)
			{
				if (!pxSceneData->EntityExists(xShape.axCubeEntityIDs[i]))
					continue;

				Zenith_Entity xCube = pxSceneData->GetEntity(xShape.axCubeEntityIDs[i]);
				if (xCube.IsValid())
				{
					const TilePuzzleCellOffset& xOffset = xShape.pxDefinition->axCells[i];
					Zenith_Maths::Vector3 xCubePos = xCurrentPos;
					xCubePos.x += xOffset.iX * s_fCellSize;
					xCubePos.z += xOffset.iY * s_fCellSize;

					Zenith_TransformComponent& xTransform = xCube.GetComponent<Zenith_TransformComponent>();
					xTransform.SetPosition(xCubePos);
				}
			}
		}

		// Lerp dragged shape toward its logical grid position
		if (m_bDragging && m_iDragShapeIndex >= 0)
		{
			static constexpr float s_fDragLerpSpeed = 20.0f;
			float fLerpFactor = fDeltaTime * s_fDragLerpSpeed;
			if (fLerpFactor > 1.0f)
				fLerpFactor = 1.0f;

			TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iDragShapeIndex];
			for (size_t i = 0; i < xShape.axCubeEntityIDs.size(); ++i)
			{
				if (!pxSceneData->EntityExists(xShape.axCubeEntityIDs[i]))
					continue;

				Zenith_Entity xCube = pxSceneData->GetEntity(xShape.axCubeEntityIDs[i]);
				if (!xCube.IsValid())
					continue;

				const TilePuzzleCellOffset& xOffset = xShape.pxDefinition->axCells[i];
				float fTargetGridX = static_cast<float>(xShape.iOriginX + xOffset.iX);
				float fTargetGridY = static_cast<float>(xShape.iOriginY + xOffset.iY);
				Zenith_Maths::Vector3 xTargetPos = GridToWorld(fTargetGridX, fTargetGridY, s_fShapeHeight);

				Zenith_TransformComponent& xTransform = xCube.GetComponent<Zenith_TransformComponent>();
				Zenith_Maths::Vector3 xCurrentPos;
				xTransform.GetPosition(xCurrentPos);

				Zenith_Maths::Vector3 xNewPos = glm::mix(xCurrentPos, xTargetPos, fLerpFactor);
				xTransform.SetPosition(xNewPos);
			}
		}

		// Check if drag lerp has reached the target while level completion is pending
		if (m_bPendingLevelComplete && m_bDragging && m_iDragShapeIndex >= 0)
		{
			TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iDragShapeIndex];
			bool bReachedTarget = true;
			for (size_t i = 0; i < xShape.axCubeEntityIDs.size(); ++i)
			{
				if (!pxSceneData->EntityExists(xShape.axCubeEntityIDs[i]))
					continue;

				Zenith_Entity xCube = pxSceneData->GetEntity(xShape.axCubeEntityIDs[i]);
				if (!xCube.IsValid())
					continue;

				const TilePuzzleCellOffset& xOffset = xShape.pxDefinition->axCells[i];
				Zenith_Maths::Vector3 xTargetPos = GridToWorld(
					static_cast<float>(xShape.iOriginX + xOffset.iX),
					static_cast<float>(xShape.iOriginY + xOffset.iY), s_fShapeHeight);
				Zenith_Maths::Vector3 xCurPos;
				xCube.GetComponent<Zenith_TransformComponent>().GetPosition(xCurPos);

				if (glm::length(xTargetPos - xCurPos) > 0.01f)
				{
					bReachedTarget = false;
					break;
				}
			}

			if (bReachedTarget)
			{
				SnapShapeVisuals(m_iDragShapeIndex);
				m_bDragging = false;
				m_iDragShapeIndex = -1;
				m_iSelectedShapeIndex = -1;
				m_bPendingLevelComplete = false;
				OnLevelCompleted();
			}
		}

		UpdateSelectionHighlight();
	}

	void UpdateSelectionHighlight()
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		bool bShapeSelectionChanged = (m_iPreviousSelectedShapeIndex != m_iSelectedShapeIndex);
		bool bCursorMoved = (m_iPreviousCursorX != m_iCursorX || m_iPreviousCursorY != m_iCursorY);

		// Update shape highlighting if selection changed
		if (bShapeSelectionChanged)
		{
			// Remove highlight from previously selected shape
			if (m_iPreviousSelectedShapeIndex >= 0 &&
				m_iPreviousSelectedShapeIndex < static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			{
				TilePuzzleShapeInstance& xPrevShape = m_xCurrentLevel.axShapes[m_iPreviousSelectedShapeIndex];
				if (xPrevShape.pxDefinition->bDraggable)
				{
					Zenith_MaterialAsset* pxNormalMaterial = m_axShapeMaterials[xPrevShape.eColor].Get();
					for (auto uID : xPrevShape.axCubeEntityIDs)
					{
						if (!pxSceneData->EntityExists(uID))
							continue;
						Zenith_Entity xCube = pxSceneData->GetEntity(uID);
						if (xCube.IsValid() && xCube.HasComponent<Zenith_ModelComponent>())
						{
							Zenith_ModelComponent& xModel = xCube.GetComponent<Zenith_ModelComponent>();
							if (xModel.GetNumMeshEntries() > 0)
							{
								xModel.GetMaterialHandleAtIndex(0).Set(pxNormalMaterial);
							}
						}
					}
				}
			}

			// Apply highlight to newly selected shape
			if (m_iSelectedShapeIndex >= 0 &&
				m_iSelectedShapeIndex < static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			{
				TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iSelectedShapeIndex];
				if (xShape.pxDefinition->bDraggable)
				{
					Zenith_MaterialAsset* pxHighlightMaterial = m_axShapeMaterialsHighlighted[xShape.eColor].Get();
					for (auto uID : xShape.axCubeEntityIDs)
					{
						if (!pxSceneData->EntityExists(uID))
							continue;
						Zenith_Entity xCube = pxSceneData->GetEntity(uID);
						if (xCube.IsValid() && xCube.HasComponent<Zenith_ModelComponent>())
						{
							Zenith_ModelComponent& xModel = xCube.GetComponent<Zenith_ModelComponent>();
							if (xModel.GetNumMeshEntries() > 0)
							{
								xModel.GetMaterialHandleAtIndex(0).Set(pxHighlightMaterial);
							}
						}
					}
				}
			}

			m_iPreviousSelectedShapeIndex = m_iSelectedShapeIndex;
		}

		// Update cursor floor highlighting if cursor moved
		if (bCursorMoved)
		{
			// Remove highlight from previous cursor floor tile
			if (m_iPreviousCursorX >= 0 && m_iPreviousCursorY >= 0)
			{
				uint32_t uPrevKey = m_iPreviousCursorY * 1000 + m_iPreviousCursorX;
				auto itPrev = m_axFloorEntityIDs.find(uPrevKey);
				if (itPrev != m_axFloorEntityIDs.end() && pxSceneData->EntityExists(itPrev->second))
				{
					Zenith_Entity xFloor = pxSceneData->GetEntity(itPrev->second);
					if (xFloor.IsValid() && xFloor.HasComponent<Zenith_ModelComponent>())
					{
						Zenith_ModelComponent& xModel = xFloor.GetComponent<Zenith_ModelComponent>();
						if (xModel.GetNumMeshEntries() > 0)
						{
							xModel.GetMaterialHandleAtIndex(0).Set(m_xFloorMaterial.Get());
						}
					}
				}
			}

			// Apply highlight to current cursor floor tile
			uint32_t uCurKey = m_iCursorY * 1000 + m_iCursorX;
			auto itCur = m_axFloorEntityIDs.find(uCurKey);
			if (itCur != m_axFloorEntityIDs.end() && pxSceneData->EntityExists(itCur->second))
			{
				Zenith_Entity xFloor = pxSceneData->GetEntity(itCur->second);
				if (xFloor.IsValid() && xFloor.HasComponent<Zenith_ModelComponent>())
				{
					Zenith_ModelComponent& xModel = xFloor.GetComponent<Zenith_ModelComponent>();
					if (xModel.GetNumMeshEntries() > 0)
					{
						xModel.GetMaterialHandleAtIndex(0).Set(m_xFloorMaterialHighlighted.Get());
					}
				}
			}

			m_iPreviousCursorX = m_iCursorX;
			m_iPreviousCursorY = m_iCursorY;
		}
	}

	void UpdateUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// Update status text
		char szBuffer[64];
		snprintf(szBuffer, sizeof(szBuffer), "Level: %u  Moves: %u", m_uCurrentLevelNumber, m_uMoveCount);
		Zenith_UI::Zenith_UIText* pxStatus = xUI.FindElement<Zenith_UI::Zenith_UIText>("Status");
		if (pxStatus)
		{
			pxStatus->SetText(szBuffer);
		}

		// Update progress
		size_t uRemaining = CountRemainingCats();
		size_t uTotal = m_xCurrentLevel.axCats.size();
		snprintf(szBuffer, sizeof(szBuffer), "Cats: %zu / %zu", uTotal - uRemaining, uTotal);
		Zenith_UI::Zenith_UIText* pxProgress = xUI.FindElement<Zenith_UI::Zenith_UIText>("Progress");
		if (pxProgress)
		{
			pxProgress->SetText(szBuffer);
		}

		// Update win text
		Zenith_UI::Zenith_UIText* pxWin = xUI.FindElement<Zenith_UI::Zenith_UIText>("WinText");
		if (pxWin)
		{
			if (m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE)
			{
				pxWin->SetText("LEVEL COMPLETE! Press N");
			}
			else
			{
				pxWin->SetText("");
			}
		}
	}

	// ========================================================================
	// Coordinate Conversion
	// ========================================================================
	bool ScreenToGrid(float fScreenX, float fScreenY, int32_t& iGridX, int32_t& iGridY)
	{
		if (!m_xParentEntity.HasComponent<Zenith_CameraComponent>())
			return false;

		Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();

		Zenith_Maths::Vector3 xNear = xCam.ScreenSpaceToWorldSpace(Zenith_Maths::Vector3(fScreenX, fScreenY, 0.0f));
		Zenith_Maths::Vector3 xFar = xCam.ScreenSpaceToWorldSpace(Zenith_Maths::Vector3(fScreenX, fScreenY, 1.0f));

		Zenith_Maths::Vector3 xDir = xFar - xNear;
		if (fabsf(xDir.y) < 1e-6f)
			return false;

		float fT = (s_fShapeHeight - xNear.y) / xDir.y;
		if (fT < 0.0f)
			return false;

		float fWorldX = xNear.x + fT * xDir.x;
		float fWorldZ = xNear.z + fT * xDir.z;

		float fOffsetX = -static_cast<float>(m_xCurrentLevel.uGridWidth) * 0.5f + 0.5f;
		float fOffsetY = -static_cast<float>(m_xCurrentLevel.uGridHeight) * 0.5f + 0.5f;

		iGridX = static_cast<int32_t>(roundf(fWorldX / s_fCellSize - fOffsetX));
		iGridY = static_cast<int32_t>(roundf(fWorldZ / s_fCellSize - fOffsetY));
		return true;
	}

	Zenith_Maths::Vector3 GridToWorld(float fGridX, float fGridY, float fHeight) const
	{
		float fOffsetX = -static_cast<float>(m_xCurrentLevel.uGridWidth) * 0.5f + 0.5f;
		float fOffsetY = -static_cast<float>(m_xCurrentLevel.uGridHeight) * 0.5f + 0.5f;

		return Zenith_Maths::Vector3(
			(fGridX + fOffsetX) * s_fCellSize,
			fHeight,
			(fGridY + fOffsetY) * s_fCellSize
		);
	}
};
