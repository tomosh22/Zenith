#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Sokoban_GameComponent.h - Main game coordinator
 *
 * Demonstrates:
 * - Game ECS component lifecycle hooks (concept-detected by the component-meta registry)
 * - Multi-scene architecture (persistent GameManager + puzzle scene)
 * - Zenith_UIButton with function pointer callback (no std::function)
 * - Scene transitions via CreateEmptyScene / UnloadScene
 * - DontDestroyOnLoad for persistent entities
 *
 * Architecture:
 * - GameManager entity (persistent): camera + UI + game component + dust emitter
 * - Puzzle scene (created/destroyed per level): tiles, boxes, player
 *
 * State machine: MAIN_MENU -> PLAYING -> (won -> R for next / Esc for menu)
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Input/Zenith_Input.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "UI/Zenith_UIButton.h"

// Include extracted modules
#include "Sokoban_Input.h"
#include "Sokoban_Animation.h"
#include "Sokoban_GridLogic.h"
#include "Sokoban_Rendering.h"
#include "Sokoban_LevelGenerator.h"
#include "Sokoban_Solver.h"
#include "Sokoban_UIManager.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Editor/Zenith_Editor.h"
#include <filesystem>
#endif

// ============================================================================
// Sokoban Resources - per-game ProjectResources struct
// Phase 8: replaces the scattered namespace-scope extern globals with a single
// struct + accessor function. The struct instance lives in Sokoban.cpp; reach
// individual resources via Sokoban::Resources().m_xWhatever.
// ============================================================================
class Zenith_Prefab;
class Flux_ParticleEmitterConfig;

namespace Sokoban
{
	struct SokobanResources
	{
		MeshGeometryHandle  m_xCubeAsset;
		Flux_MeshGeometry*  m_pxCubeGeometry = nullptr;

		MaterialHandle      m_xFloorMaterial;
		MaterialHandle      m_xWallMaterial;
		MaterialHandle      m_xBoxMaterial;
		MaterialHandle      m_xBoxOnTargetMaterial;
		MaterialHandle      m_xPlayerMaterial;
		MaterialHandle      m_xTargetMaterial;

		PrefabHandle        m_xTilePrefab;
		PrefabHandle        m_xBoxPrefab;
		PrefabHandle        m_xPlayerPrefab;

		Flux_ParticleEmitterConfig* m_pxDustConfig    = nullptr;
		Zenith_EntityID             m_uDustEmitterID  = INVALID_ENTITY_ID;
	};

	SokobanResources& Resources();
}

// Note: SokobanTileType and SokobanDirection enums are defined in Sokoban_GridLogic.h

// ============================================================================
// Configuration Constants
// ============================================================================
static constexpr uint32_t s_uMaxGridSizeConfig = 16;

// ============================================================================
// Game State
// ============================================================================
enum class SokobanGameState : uint32_t
{
	MAIN_MENU,
	PLAYING,
	GAME_OVER
};

// ============================================================================
// Main Game Component
// ============================================================================
class Sokoban_GameComponent
{
public:
	static constexpr uint32_t s_uMaxGridCells = s_uMaxGridSizeConfig * s_uMaxGridSizeConfig;

	Sokoban_GameComponent() = delete;
	Sokoban_GameComponent(Zenith_Entity& xParentEntity)
		: m_uGridWidth(8)
		, m_uGridHeight(8)
		, m_uPlayerX(0)
		, m_uPlayerY(0)
		, m_uMoveCount(0)
		, m_uTargetCount(0)
		, m_uMinMoves(0)
		, m_bWon(false)
		, m_xRng(std::random_device{}())
		, m_eState(SokobanGameState::MAIN_MENU)
		, m_iFocusIndex(0)
		, m_xParentEntity(xParentEntity)
	{
		memset(m_aeTiles, 0, sizeof(m_aeTiles));
		memset(m_abTargets, false, sizeof(m_abTargets));
		memset(m_abBoxes, false, sizeof(m_abBoxes));
	}

	~Sokoban_GameComponent()
	{
		if (m_bOwnsGeometry && m_pxCubeGeometry)
		{
			delete m_pxCubeGeometry;
			m_pxCubeGeometry = nullptr;
		}
	}

	// Component pools relocate components on resize / swap-and-pop / cross-scene
	// transfer (move-construct + destruct the source), so the owned cube geometry
	// must transfer ownership on move - hand-written moves, copies deleted.
	Sokoban_GameComponent(const Sokoban_GameComponent&) = delete;
	Sokoban_GameComponent& operator=(const Sokoban_GameComponent&) = delete;

	Sokoban_GameComponent(Sokoban_GameComponent&& xOther) noexcept
		: m_uGridWidth(xOther.m_uGridWidth)
		, m_uGridHeight(xOther.m_uGridHeight)
		, m_uPlayerX(xOther.m_uPlayerX)
		, m_uPlayerY(xOther.m_uPlayerY)
		, m_uMoveCount(xOther.m_uMoveCount)
		, m_uTargetCount(xOther.m_uTargetCount)
		, m_uMinMoves(xOther.m_uMinMoves)
		, m_bWon(xOther.m_bWon)
		, m_xAnimation(xOther.m_xAnimation)
		, m_xRng(xOther.m_xRng)
		, m_xRenderer(std::move(xOther.m_xRenderer))
		, m_eState(xOther.m_eState)
		, m_iFocusIndex(xOther.m_iFocusIndex)
		, m_xPuzzleScene(xOther.m_xPuzzleScene)
		, m_xParentEntity(xOther.m_xParentEntity)
		, m_pxCubeGeometry(xOther.m_pxCubeGeometry)
		, m_bOwnsGeometry(xOther.m_bOwnsGeometry)
		, m_xFloorMaterial(std::move(xOther.m_xFloorMaterial))
		, m_xWallMaterial(std::move(xOther.m_xWallMaterial))
		, m_xBoxMaterial(std::move(xOther.m_xBoxMaterial))
		, m_xBoxOnTargetMaterial(std::move(xOther.m_xBoxOnTargetMaterial))
		, m_xPlayerMaterial(std::move(xOther.m_xPlayerMaterial))
		, m_xTargetMaterial(std::move(xOther.m_xTargetMaterial))
	{
		memcpy(m_aeTiles, xOther.m_aeTiles, sizeof(m_aeTiles));
		memcpy(m_abTargets, xOther.m_abTargets, sizeof(m_abTargets));
		memcpy(m_abBoxes, xOther.m_abBoxes, sizeof(m_abBoxes));
		// Steal geometry ownership so the moved-from destructor doesn't delete it.
		xOther.m_pxCubeGeometry = nullptr;
		xOther.m_bOwnsGeometry = false;
	}

	Sokoban_GameComponent& operator=(Sokoban_GameComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			if (m_bOwnsGeometry && m_pxCubeGeometry)
			{
				delete m_pxCubeGeometry;
			}
			m_uGridWidth = xOther.m_uGridWidth;
			m_uGridHeight = xOther.m_uGridHeight;
			memcpy(m_aeTiles, xOther.m_aeTiles, sizeof(m_aeTiles));
			memcpy(m_abTargets, xOther.m_abTargets, sizeof(m_abTargets));
			memcpy(m_abBoxes, xOther.m_abBoxes, sizeof(m_abBoxes));
			m_uPlayerX = xOther.m_uPlayerX;
			m_uPlayerY = xOther.m_uPlayerY;
			m_uMoveCount = xOther.m_uMoveCount;
			m_uTargetCount = xOther.m_uTargetCount;
			m_uMinMoves = xOther.m_uMinMoves;
			m_bWon = xOther.m_bWon;
			m_xAnimation = xOther.m_xAnimation;
			m_xRng = xOther.m_xRng;
			m_xRenderer = std::move(xOther.m_xRenderer);
			m_eState = xOther.m_eState;
			m_iFocusIndex = xOther.m_iFocusIndex;
			m_xPuzzleScene = xOther.m_xPuzzleScene;
			m_xParentEntity = xOther.m_xParentEntity;
			m_pxCubeGeometry = xOther.m_pxCubeGeometry;
			m_bOwnsGeometry = xOther.m_bOwnsGeometry;
			m_xFloorMaterial = std::move(xOther.m_xFloorMaterial);
			m_xWallMaterial = std::move(xOther.m_xWallMaterial);
			m_xBoxMaterial = std::move(xOther.m_xBoxMaterial);
			m_xBoxOnTargetMaterial = std::move(xOther.m_xBoxOnTargetMaterial);
			m_xPlayerMaterial = std::move(xOther.m_xPlayerMaterial);
			m_xTargetMaterial = std::move(xOther.m_xTargetMaterial);
			// Steal geometry ownership so the moved-from destructor doesn't delete it.
			xOther.m_pxCubeGeometry = nullptr;
			xOther.m_bOwnsGeometry = false;
		}
		return *this;
	}

	// ========================================================================
	// Lifecycle Hooks - Called by engine (concept-detected by the meta registry)
	// ========================================================================

	/**
	 * OnAwake - Called when the component is created at RUNTIME
	 * NOT called during scene loading/deserialization.
	 * Use for: Initial resource setup, wiring button callbacks.
	 */
	void OnAwake()
	{
		// Use global resources (initialized in Sokoban.cpp)
		m_pxCubeGeometry = Sokoban::Resources().m_pxCubeGeometry;
		m_xFloorMaterial = Sokoban::Resources().m_xFloorMaterial;
		m_xWallMaterial = Sokoban::Resources().m_xWallMaterial;
		m_xBoxMaterial = Sokoban::Resources().m_xBoxMaterial;
		m_xBoxOnTargetMaterial = Sokoban::Resources().m_xBoxOnTargetMaterial;
		m_xPlayerMaterial = Sokoban::Resources().m_xPlayerMaterial;
		m_xTargetMaterial = Sokoban::Resources().m_xTargetMaterial;

		// Wire up button callbacks
		bool bHasMenu = false;
		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = *pxUI;
			Zenith_UI::Zenith_UIButton* pxPlayBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			if (pxPlayBtn)
			{
				// No instance userdata: components RELOCATE on pool resize, so a
				// captured `this` could dangle. The callback needs no instance state.
				pxPlayBtn->SetOnClick(&OnPlayClicked, nullptr);
				pxPlayBtn->SetFocused(true);
				bHasMenu = true;
			}
		}

		if (bHasMenu)
		{
			// Start in main menu state
			m_eState = SokobanGameState::MAIN_MENU;
			SetMenuVisible(true);
			SetHUDVisible(false);
		}
		else
		{
			// No menu UI (gameplay scene) - start game directly
			StartGame();
		}
	}

	/**
	 * OnStart - Called before first OnUpdate, for ALL entities
	 * Called even for entities loaded from scene file.
	 */
	void OnStart()
	{
		// Ensure menu state if no level is loaded yet
		if (m_eState == SokobanGameState::MAIN_MENU)
		{
			SetMenuVisible(true);
			SetHUDVisible(false);
		}
	}

	/**
	 * OnUpdate - Called every frame
	 * Dispatches to the current game state handler.
	 */
	void OnUpdate(const float fDt)
	{
		switch (m_eState)
		{
		case SokobanGameState::MAIN_MENU:
			UpdateMenuInput();
			break;

		case SokobanGameState::PLAYING:
			// Escape returns to menu
			if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
			// R starts a new level
			if (Sokoban_Input::WasResetPressed())
			{
				StartNewLevel();
				return;
			}

			if (m_xAnimation.IsAnimating())
			{
				const bool bStepComplete = m_xAnimation.Update(fDt, m_uGridWidth, m_uGridHeight, Sokoban::Resources().m_uDustEmitterID);
				if (bStepComplete && Sokoban_GridLogic::CheckWinCondition(m_abBoxes, m_abTargets,
					m_uGridWidth * m_uGridHeight, m_uTargetCount))
				{
					m_bWon = true;
					UpdateUI();
				}
			}
			else if (!m_bWon)
			{
				HandleInput();
			}
			UpdateVisuals();
			break;

		case SokobanGameState::GAME_OVER:
			break;
		}
	}

#ifdef ZENITH_TOOLS
	/**
	 * RenderPropertiesPanel - Editor UI (tools build only)
	 */
	void RenderPropertiesPanel()
	{
		ImGui::Text("Sokoban Puzzle Game");
		ImGui::Separator();
		ImGui::Text("State: %s", m_eState == SokobanGameState::MAIN_MENU ? "Menu" :
			m_eState == SokobanGameState::PLAYING ? "Playing" : "Game Over");
		ImGui::Text("Grid Size: %u x %u", m_uGridWidth, m_uGridHeight);
		ImGui::Text("Moves: %u", m_uMoveCount);
		ImGui::Text("Min Moves: %u", m_uMinMoves);
		ImGui::Text("Boxes on targets: %u / %u",
			Sokoban_GridLogic::CountBoxesOnTargets(m_abBoxes, m_abTargets, m_uGridWidth * m_uGridHeight),
			m_uTargetCount);

		if (m_bWon)
		{
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "LEVEL COMPLETE!");
		}

		if (ImGui::Button("New Level"))
		{
			StartNewLevel();
		}

		ImGui::Separator();
		ImGui::Text("Controls:");
		ImGui::Text("  WASD / Arrow Keys: Move");
		ImGui::Text("  R: New Level");
		ImGui::Text("  Esc: Return to Menu");

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Visual Assets", ImGuiTreeNodeFlags_DefaultOpen))
		{
			RenderMeshSlot("Cube Mesh", m_pxCubeGeometry);
			ImGui::Separator();
			RenderMaterialSlot("Floor Material", m_xFloorMaterial);
			RenderMaterialSlot("Wall Material", m_xWallMaterial);
			RenderMaterialSlot("Box Material", m_xBoxMaterial);
			RenderMaterialSlot("Box On Target", m_xBoxOnTargetMaterial);
			RenderMaterialSlot("Player Material", m_xPlayerMaterial);
			RenderMaterialSlot("Target Material", m_xTargetMaterial);
		}
	}
#endif

	// ========================================================================
	// Serialization - Save/load component state
	// ========================================================================

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// Component-contract leading version (required by the meta registry).
		const u_int uComponentVersion = 1;
		xStream << uComponentVersion;

		// Parameter payload (byte-identical to the pre-migration parameter block,
		// including its own internal version field).
		const uint32_t uVersion = 1;
		xStream << uVersion;

		// Mesh path
		std::string strMeshPath = (m_pxCubeGeometry && !m_pxCubeGeometry->m_strSourcePath.empty())
			? m_pxCubeGeometry->m_strSourcePath : "";
		xStream << strMeshPath;

		// Materials
		auto WriteMaterial = [&xStream](const MaterialHandle& xMat)
		{
			Zenith_MaterialAsset* pxMat = xMat.GetDirect();
			if (pxMat)
			{
				pxMat->WriteToDataStream(xStream);
			}
			else
			{
				// Write empty material placeholder - use local to avoid registry leak
				Zenith_MaterialAsset xEmptyMat;
				xEmptyMat.SetName("Empty");
				xEmptyMat.WriteToDataStream(xStream);
			}
		};

		WriteMaterial(m_xFloorMaterial);
		WriteMaterial(m_xWallMaterial);
		WriteMaterial(m_xBoxMaterial);
		WriteMaterial(m_xBoxOnTargetMaterial);
		WriteMaterial(m_xPlayerMaterial);
		WriteMaterial(m_xTargetMaterial);
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		// Component-contract leading version (required by the meta registry).
		u_int uComponentVersion = 0;
		xStream >> uComponentVersion;

		// Parameter payload (byte-identical to the pre-migration parameter block).
		uint32_t uVersion;
		xStream >> uVersion;

		if (uVersion >= 1)
		{
			std::string strMeshPath;
			xStream >> strMeshPath;
			if (!strMeshPath.empty())
			{
				// Clean up old geometry if we own it
				if (m_bOwnsGeometry && m_pxCubeGeometry)
				{
					delete m_pxCubeGeometry;
				}
				m_pxCubeGeometry = new Flux_MeshGeometry();
				m_bOwnsGeometry = true;
				Flux_MeshGeometry::LoadFromFile(strMeshPath.c_str(), *m_pxCubeGeometry, 0, true);
			}

			auto ReadMaterial = [&xStream](MaterialHandle& xMat, const char* szName)
			{
				Zenith_MaterialAsset* pxLoaded = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
				pxLoaded->SetName(szName);
				pxLoaded->ReadFromDataStream(xStream);
				xMat.Set(pxLoaded);
			};

			ReadMaterial(m_xFloorMaterial, "Sokoban_Floor");
			ReadMaterial(m_xWallMaterial, "Sokoban_Wall");
			ReadMaterial(m_xBoxMaterial, "Sokoban_Box");
			ReadMaterial(m_xBoxOnTargetMaterial, "Sokoban_BoxOnTarget");
			ReadMaterial(m_xPlayerMaterial, "Sokoban_Player");
			ReadMaterial(m_xTargetMaterial, "Sokoban_Target");
		}
	}

private:
	// ========================================================================
	// Button Callbacks (static function pointers, NOT std::function)
	// ========================================================================

	static void OnPlayClicked(void* /*pxUserData*/)
	{
		g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	// ========================================================================
	// State Transitions
	// ========================================================================

	void StartGame()
	{
		SetMenuVisible(false);
		SetHUDVisible(true);

		// Create puzzle scene for level entities
		m_xPuzzleScene = g_xEngine.Scenes().LoadScene("Puzzle", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xPuzzleScene);

		m_eState = SokobanGameState::PLAYING;
		GenerateNewLevel();
	}

	void StartNewLevel()
	{
		// Unload current puzzle scene (destroys all level entities automatically)
		if (m_xPuzzleScene.IsValid())
		{
			m_xRenderer.ClearEntityIDs();
			g_xEngine.Scenes().UnloadScene(m_xPuzzleScene);
		}

		// Create fresh puzzle scene
		m_xPuzzleScene = g_xEngine.Scenes().LoadScene("Puzzle", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xPuzzleScene);

		m_eState = SokobanGameState::PLAYING;
		GenerateNewLevel();
	}

	void ReturnToMenu()
	{
		// Unload puzzle scene (destroys all level entities automatically)
		if (m_xPuzzleScene.IsValid())
		{
			m_xRenderer.ClearEntityIDs();
			g_xEngine.Scenes().UnloadScene(m_xPuzzleScene);
			m_xPuzzleScene = Zenith_Scene();
		}

		// Reset game state
		m_bWon = false;
		m_xAnimation.Cancel(Sokoban::Resources().m_uDustEmitterID);

		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	// ========================================================================
	// Menu UI
	// ========================================================================

	void SetMenuVisible(bool bVisible)
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;

		Zenith_UI::Zenith_UIText* pxTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("MenuTitle");
		if (pxTitle) pxTitle->SetVisible(bVisible);

		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay) pxPlay->SetVisible(bVisible);
	}

	void SetHUDVisible(bool bVisible)
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;

		const char* aszHUDElements[] = {
			"Title", "ControlsHeader", "MoveInstr", "ResetInstr",
			"GoalHeader", "GoalDesc", "Status", "Progress", "MinMoves", "WinText"
		};

		for (const char* szName : aszHUDElements)
		{
			Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
			if (pxText) pxText->SetVisible(bVisible);
		}
	}

	void UpdateMenuInput()
	{
		// Only 1 button (Play) - keyboard focus stays on it
		// Enter/Space activates via the button's own focus handling
		// Up/Down would cycle if more buttons existed
		static constexpr int32_t s_iButtonCount = 1;

		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_UP) ||
			g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_W))
		{
			m_iFocusIndex = (m_iFocusIndex - 1 + s_iButtonCount) % s_iButtonCount;
		}
		if (g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_DOWN) ||
			g_xEngine.Input().WasKeyPressedThisFrame(ZENITH_KEY_S))
		{
			m_iFocusIndex = (m_iFocusIndex + 1) % s_iButtonCount;
		}

		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = *pxUI;
			Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			if (pxPlay) pxPlay->SetFocused(m_iFocusIndex == 0);
		}
	}

	// ========================================================================
	// Input Handling (movement only - R and Esc handled in OnUpdate)
	// ========================================================================
	void HandleInput()
	{
		if (m_xAnimation.IsAnimating()) return;

		SokobanDirection eDir = Sokoban_Input::GetInputDirection();
		if (eDir != SOKOBAN_DIR_NONE)
		{
			TryMove(eDir);
		}
	}

	// ========================================================================
	// Movement Logic
	// ========================================================================
	bool TryMove(SokobanDirection eDir)
	{
		if (m_xAnimation.IsAnimating()) return false;

		if (!Sokoban_GridLogic::CanMove(m_aeTiles, m_abBoxes, m_uPlayerX, m_uPlayerY,
			m_uGridWidth, m_uGridHeight, eDir))
		{
			return false;
		}

		int32_t iDeltaX, iDeltaY;
		Sokoban_GridLogic::GetDirectionDelta(eDir, iDeltaX, iDeltaY);

		uint32_t uNewX = m_uPlayerX + iDeltaX;
		uint32_t uNewY = m_uPlayerY + iDeltaY;
		uint32_t uNewIndex = uNewY * m_uGridWidth + uNewX;

		bool bPushingBox = false;
		uint32_t uBoxDestX = 0, uBoxDestY = 0;

		if (m_abBoxes[uNewIndex])
		{
			bPushingBox = true;
			uBoxDestX = uNewX + iDeltaX;
			uBoxDestY = uNewY + iDeltaY;
			Sokoban_GridLogic::PushBox(m_abBoxes, uNewX, uNewY, m_uGridWidth, eDir);
		}

		uint32_t uOldX = m_uPlayerX;
		uint32_t uOldY = m_uPlayerY;
		m_uPlayerX = uNewX;
		m_uPlayerY = uNewY;
		m_uMoveCount++;

		m_xAnimation.StartPlayerMove(uOldX, uOldY, uNewX, uNewY);
		if (bPushingBox)
		{
			m_xAnimation.StartBoxPush(uNewX, uNewY, uBoxDestX, uBoxDestY);
		}

		UpdateUI();
		return true;
	}

	// ========================================================================
	// Visual Updates
	// ========================================================================
	void UpdateVisuals()
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxPuzzleData = g_xEngine.Scenes().GetSceneData(m_xPuzzleScene);
		if (!pxPuzzleData)
			return;

		float fVisualX = m_xAnimation.IsAnimating() ? m_xAnimation.GetPlayerVisualX() : static_cast<float>(m_uPlayerX);
		float fVisualY = m_xAnimation.IsAnimating() ? m_xAnimation.GetPlayerVisualY() : static_cast<float>(m_uPlayerY);
		m_xRenderer.UpdatePlayerPosition(fVisualX, fVisualY, pxPuzzleData);

		m_xRenderer.UpdateBoxPositions(m_abBoxes, m_uGridWidth, m_uGridHeight,
			m_xAnimation.IsBoxAnimating(), m_xAnimation.GetBoxToX(), m_xAnimation.GetBoxToY(),
			m_xAnimation.GetBoxVisualX(), m_xAnimation.GetBoxVisualY(),
			pxPuzzleData);
	}

	// ========================================================================
	// Level Generation
	// ========================================================================
	void GenerateNewLevel()
	{
		m_uMoveCount = 0;
		m_bWon = false;
		m_xAnimation.Cancel(Sokoban::Resources().m_uDustEmitterID);

		Sokoban_LevelGenerator::LevelData xData;
		xData.aeTiles = m_aeTiles;
		xData.abTargets = m_abTargets;
		xData.abBoxes = m_abBoxes;

		for (int i = 0; i < 1000; i++)
		{
			Sokoban_LevelGenerator::GenerateLevelAttempt(xData, m_xRng);

			m_uGridWidth = xData.uGridWidth;
			m_uGridHeight = xData.uGridHeight;
			m_uPlayerX = xData.uPlayerX;
			m_uPlayerY = xData.uPlayerY;
			m_uTargetCount = xData.uTargetCount;

			int32_t iMinMoves = Sokoban_Solver::SolveLevel(
				m_aeTiles, m_abBoxes, m_abTargets,
				m_uPlayerX, m_uPlayerY, m_uGridWidth, m_uGridHeight);

			if (iMinMoves >= 5)
			{
				m_uMinMoves = static_cast<uint32_t>(iMinMoves);
				break;
			}
		}

		m_xAnimation.SnapPlayerTo(m_uPlayerX, m_uPlayerY);

		// Create 3D entities in the puzzle scene
		Zenith_SceneData* pxPuzzleData = g_xEngine.Scenes().GetSceneData(m_xPuzzleScene);
		m_xRenderer.Create3DLevel(
			m_uGridWidth, m_uGridHeight,
			m_aeTiles, m_abBoxes, m_abTargets,
			m_uPlayerX, m_uPlayerY,
			Sokoban::Resources().m_xTilePrefab.GetDirect(), Sokoban::Resources().m_xBoxPrefab.GetDirect(), Sokoban::Resources().m_xPlayerPrefab.GetDirect(),
			m_pxCubeGeometry,
			m_xFloorMaterial.GetDirect(), m_xWallMaterial.GetDirect(), m_xTargetMaterial.GetDirect(),
			m_xBoxMaterial.GetDirect(), m_xBoxOnTargetMaterial.GetDirect(), m_xPlayerMaterial.GetDirect(),
			pxPuzzleData);

		m_xRenderer.RepositionCamera(m_uGridWidth, m_uGridHeight);
		UpdateUI();
	}

	// ========================================================================
	// UI Management
	// ========================================================================
	void UpdateUI()
	{
		Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr)
			return;

		Zenith_UIComponent& xUI = *pxUI;
		uint32_t uBoxesOnTargets = Sokoban_GridLogic::CountBoxesOnTargets(
			m_abBoxes, m_abTargets, m_uGridWidth * m_uGridHeight);

		Sokoban_UIManager::UpdateStatusText(xUI, m_uMoveCount, uBoxesOnTargets,
			m_uTargetCount, m_uMinMoves, m_bWon);
	}

	// ========================================================================
	// Editor Helpers
	// ========================================================================
#ifdef ZENITH_TOOLS
	void RenderMaterialSlot(const char* szLabel, MaterialHandle& xMaterial)
	{
		ImGui::PushID(szLabel);
		Zenith_MaterialAsset* pxMat = xMaterial.GetDirect();
		std::string strMaterialName = pxMat ? pxMat->GetName() : "(none)";
		ImGui::Text("%s:", szLabel);
		ImGui::SameLine();
		ImVec2 xButtonSize(150, 20);
		ImGui::Button(strMaterialName.c_str(), xButtonSize);

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_MATERIAL))
			{
				const DragDropFilePayload* pFilePayload =
					static_cast<const DragDropFilePayload*>(pPayload->Data);
				xMaterial.SetPath(pFilePayload->m_szFilePath);
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Drop a .zmat material file here");
		}
		ImGui::PopID();
	}

	void RenderMeshSlot(const char* szLabel, Flux_MeshGeometry*& pxMesh)
	{
		ImGui::PushID(szLabel);
		std::string strMeshName = pxMesh ? "(loaded)" : "(none)";
		if (pxMesh && !pxMesh->m_strSourcePath.empty())
		{
			std::filesystem::path xPath(pxMesh->m_strSourcePath);
			strMeshName = xPath.filename().string();
		}
		ImGui::Text("%s:", szLabel);
		ImGui::SameLine();
		ImVec2 xButtonSize(150, 20);
		ImGui::Button(strMeshName.c_str(), xButtonSize);

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* pPayload = ImGui::AcceptDragDropPayload(DRAGDROP_PAYLOAD_MESH))
			{
				const DragDropFilePayload* pFilePayload =
					static_cast<const DragDropFilePayload*>(pPayload->Data);
				Flux_MeshGeometry* pxNewMesh = new Flux_MeshGeometry();
				Flux_MeshGeometry::LoadFromFile(pFilePayload->m_szFilePath, *pxNewMesh, 0, true);
				if (pxNewMesh->GetNumVerts() > 0)
				{
					if (m_bOwnsGeometry && pxMesh)
					{
						delete pxMesh;
					}
					pxMesh = pxNewMesh;
					m_bOwnsGeometry = true;
				}
				else
				{
					delete pxNewMesh;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Drop a .zmesh mesh file here");
		}
		ImGui::PopID();
	}
#endif

	// ========================================================================
	// Member Variables
	// ========================================================================

	// Grid state
	uint32_t m_uGridWidth;
	uint32_t m_uGridHeight;
	SokobanTileType m_aeTiles[s_uMaxGridCells];
	bool m_abTargets[s_uMaxGridCells];
	bool m_abBoxes[s_uMaxGridCells];

	// Player state
	uint32_t m_uPlayerX;
	uint32_t m_uPlayerY;

	// Game state
	uint32_t m_uMoveCount;
	uint32_t m_uTargetCount;
	uint32_t m_uMinMoves;
	bool m_bWon;

	// Player/box step animation (tween state + dust emitter driving).
	Sokoban_Animation m_xAnimation;

	// Random number generator
	std::mt19937 m_xRng;

	// Renderer module instance
	Sokoban_Renderer m_xRenderer;

	// State machine
	SokobanGameState m_eState;
	int32_t m_iFocusIndex;

	// Scene handle for the puzzle scene (created/destroyed on transitions)
	Zenith_Scene m_xPuzzleScene;

	// Owning entity (explicit member - components store their parent entity)
	Zenith_Entity m_xParentEntity;

public:
	// Resource pointers
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	bool m_bOwnsGeometry = false;  // true if we allocated m_pxCubeGeometry
	MaterialHandle m_xFloorMaterial;
	MaterialHandle m_xWallMaterial;
	MaterialHandle m_xBoxMaterial;
	MaterialHandle m_xBoxOnTargetMaterial;
	MaterialHandle m_xPlayerMaterial;
	MaterialHandle m_xTargetMaterial;
};
