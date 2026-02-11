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
#include "TilePuzzle/Components/TilePuzzle_LevelGenerator.h"

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
	{
	}

	~TilePuzzle_Behaviour() = default;

	// ========================================================================
	// Lifecycle Hooks
	// ========================================================================

	void OnAwake() ZENITH_FINAL override
	{
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
			Zenith_UI::Zenith_UIButton* pxPlayBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			if (pxPlayBtn)
			{
				pxPlayBtn->SetOnClick(&OnPlayClicked, this);
				pxPlayBtn->SetFocused(true);
				bHasMenu = true;
			}
		}

		if (bHasMenu)
		{
			// Start in main menu state
			m_eState = TILEPUZZLE_STATE_MAIN_MENU;
			SetMenuVisible(true);
			SetHUDVisible(false);
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

		case TILEPUZZLE_STATE_PLAYING:
			// Escape returns to menu
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			HandleInput();
			break;

		case TILEPUZZLE_STATE_SHAPE_SLIDING:
			UpdateSlideAnimation(fDeltaTime);
			break;

		case TILEPUZZLE_STATE_CHECK_ELIMINATION:
			CheckCatElimination();
			if (IsLevelComplete())
			{
				m_eState = TILEPUZZLE_STATE_LEVEL_COMPLETE;
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
		if (m_eState != TILEPUZZLE_STATE_MAIN_MENU)
		{
			UpdateVisuals();
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

		const char* aszStateNames[] = { "Menu", "Playing", "Sliding", "Checking", "Complete", "Generating" };
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

	static void OnPlayClicked(void* /*pxUserData*/)
	{
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
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
		// Unload puzzle scene (destroys all level entities automatically)
		if (m_xPuzzleScene.IsValid())
		{
			ClearEntityReferences();
			Zenith_SceneManager::UnloadScene(m_xPuzzleScene);
			m_xPuzzleScene = Zenith_Scene();
		}

		Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
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

	void UpdateMenuInput()
	{
		static constexpr int32_t s_iButtonCount = 1;

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
			Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			if (pxPlay) pxPlay->SetFocused(m_iFocusIndex == 0);
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

	// Menu state
	int32_t m_iFocusIndex;

	// Scene handle for the puzzle scene (created/destroyed on transitions)
	Zenith_Scene m_xPuzzleScene;

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
		StartNewLevel();
	}

	// ========================================================================
	// Input Handling
	// ========================================================================
	void HandleInput()
	{
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
			if (m_iSelectedShapeIndex >= 0)
			{
				m_iSelectedShapeIndex = -1;
			}
			else
			{
				m_iSelectedShapeIndex = GetShapeAtPosition(m_iCursorX, m_iCursorY);
			}
			return;
		}

		if (eDir != TILEPUZZLE_DIR_NONE)
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

	bool CanMoveShape(int32_t iShapeIndex, int32_t iDeltaX, int32_t iDeltaY) const
	{
		const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];

		for (const auto& xOffset : xShape.pxDefinition->axCells)
		{
			int32_t iNewX = xShape.iOriginX + xOffset.iX + iDeltaX;
			int32_t iNewY = xShape.iOriginY + xOffset.iY + iDeltaY;

			if (iNewX < 0 || iNewX >= static_cast<int32_t>(m_xCurrentLevel.uGridWidth) ||
				iNewY < 0 || iNewY >= static_cast<int32_t>(m_xCurrentLevel.uGridHeight))
			{
				return false;
			}

			uint32_t uCellIndex = iNewY * m_xCurrentLevel.uGridWidth + iNewX;
			if (m_xCurrentLevel.aeCells[uCellIndex] == TILEPUZZLE_CELL_EMPTY)
			{
				return false;
			}

			for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
			{
				if (static_cast<int32_t>(i) == iShapeIndex)
					continue;

				const TilePuzzleShapeInstance& xOther = m_xCurrentLevel.axShapes[i];
				for (const auto& xOtherOffset : xOther.pxDefinition->axCells)
				{
					int32_t iOtherX = xOther.iOriginX + xOtherOffset.iX;
					int32_t iOtherY = xOther.iOriginY + xOtherOffset.iY;
					if (iOtherX == iNewX && iOtherY == iNewY)
					{
						return false;
					}
				}
			}
		}

		return true;
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

		for (auto& xShape : m_xCurrentLevel.axShapes)
		{
			if (!xShape.pxDefinition->bDraggable)
				continue;

			for (const auto& xOffset : xShape.pxDefinition->axCells)
			{
				int32_t iCellX = xShape.iOriginX + xOffset.iX;
				int32_t iCellY = xShape.iOriginY + xOffset.iY;

				for (auto& xCat : m_xCurrentLevel.axCats)
				{
					if (xCat.bEliminated)
						continue;

					if (xCat.iGridX == iCellX && xCat.iGridY == iCellY && xCat.eColor == xShape.eColor)
					{
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
			}
		}
	}

	bool IsLevelComplete() const
	{
		return CountRemainingCats() == 0;
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

	void UpdateVisuals()
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		// Update shape positions (for sliding animation)
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
