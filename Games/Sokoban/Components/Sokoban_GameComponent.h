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
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "UI/Zenith_UIButton.h"

// Include extracted modules (W2: Sokoban_Input + Sokoban_UIManager are GONE -
// input dispatch is OnKeyPressed chains, HUD text is SetUIText nodes)
#include "Sokoban_Animation.h"
#include "Sokoban_GridLogic.h"
#include "Sokoban_Rendering.h"
#include "Sokoban_LevelGenerator.h"
#include "Sokoban_Solver.h"

#include <random>

#ifdef ZENITH_TOOLS
#include "imgui.h"
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
		, m_uTargetCount(0)
		, m_uMinMoves(0)
		, m_xRng(std::random_device{}())
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
		, m_uTargetCount(xOther.m_uTargetCount)
		, m_uMinMoves(xOther.m_uMinMoves)
		, m_xAnimation(xOther.m_xAnimation)
		, m_xRng(xOther.m_xRng)
		, m_xRenderer(std::move(xOther.m_xRenderer))
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
			m_uTargetCount = xOther.m_uTargetCount;
			m_uMinMoves = xOther.m_uMinMoves;
			m_xAnimation = xOther.m_xAnimation;
			m_xRng = xOther.m_xRng;
			m_xRenderer = std::move(xOther.m_xRenderer);
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
	// State probes for the characterization tests (read-only; same surface
	// before and after the W2 graph conversion). gameState/moveCount/won LIVE
	// on the attached graph's blackboard (Sokoban_LevelFlow declares them;
	// Sokoban_GameFlow pins gameState at MAIN_MENU) - "state moves to the
	// graph blackboard, shim accessor reads it". Grid facts stay C++.
	// ========================================================================
	SokobanGameState GetGameState()
	{
		Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard();
		if (pxBlackboard == nullptr)
		{
			return SokobanGameState::MAIN_MENU;	// pre-resolve fallback
		}
		int32_t iState = pxBlackboard->GetInt32("gameState", 0);
		iState = iState < 0 ? 0 : (iState > 2 ? 2 : iState);
		return static_cast<SokobanGameState>(iState);
	}
	uint32_t GetMoveCount()
	{
		Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard();
		const int32_t iMoves = pxBlackboard ? pxBlackboard->GetInt32("moveCount", 0) : 0;
		return iMoves < 0 ? 0u : static_cast<uint32_t>(iMoves);
	}
	bool IsWon()
	{
		Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard();
		return pxBlackboard ? pxBlackboard->GetBool("won", false) : false;
	}
	bool IsAnimating() const { return m_xAnimation.IsAnimating(); }
	uint32_t GetPlayerX() const { return m_uPlayerX; }
	uint32_t GetPlayerY() const { return m_uPlayerY; }
	uint32_t GetTargetCount() const { return m_uTargetCount; }
	uint32_t GetMinMoves() const { return m_uMinMoves; }
	uint32_t GetBoxesOnTargets() const
	{
		return Sokoban_GridLogic::CountBoxesOnTargets(m_abBoxes, m_abTargets, m_uGridWidth * m_uGridHeight);
	}

	// ========================================================================
	// Graph-facing systems surface (the node seams)
	// ========================================================================

	// The old TryMove body minus its DECISIONS' bookkeeping (the move-count
	// increment and HUD refresh are graph chains). GridLogic queries, the
	// grid/box/player state writes, and the step animation are systems.
	bool TryMoveSystems(SokobanDirection eDir)
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

		m_xAnimation.StartPlayerMove(uOldX, uOldY, uNewX, uNewY);
		if (bPushingBox)
		{
			m_xAnimation.StartBoxPush(uNewX, uNewY, uBoxDestX, uBoxDestY);
		}
		return true;
	}

	// Fresh puzzle scene + generation (the old StartGame/StartNewLevel systems
	// body; counter/state resets are graph-side).
	void RegenerateLevel()
	{
		if (m_xPuzzleScene.IsValid())
		{
			m_xRenderer.ClearEntityIDs();
			g_xEngine.Scenes().UnloadScene(m_xPuzzleScene);
		}
		m_xPuzzleScene = g_xEngine.Scenes().LoadScene("Puzzle", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xPuzzleScene);

		GenerateNewLevel();
	}

	// Level teardown (the escape-to-menu systems body; the menu-scene load is
	// an engine LoadSceneByIndex node at the end of the graph chain).
	void UnloadLevel()
	{
		if (m_xPuzzleScene.IsValid())
		{
			m_xRenderer.ClearEntityIDs();
			g_xEngine.Scenes().UnloadScene(m_xPuzzleScene);
			m_xPuzzleScene = Zenith_Scene();
		}
		m_xAnimation.Cancel(Sokoban::Resources().m_uDustEmitterID);
	}

	// Characterization-test seam: replace the generated level with a tiny
	// fixed corridor - player (1,1), box (3,1), target (4,1) in a 6x3 walled
	// grid, so D is a neutral move and a second D pushes the box onto the
	// target (deterministic win). Systems-only (grid arrays + visuals); no
	// counter/state writes, so it is conversion-neutral. Use at a fresh
	// level only (counters are whatever the current level left them as).
	void Test_LoadFixtureLevel()
	{
		if (m_xPuzzleScene.IsValid())
		{
			m_xRenderer.ClearEntityIDs();
			g_xEngine.Scenes().UnloadScene(m_xPuzzleScene);
		}
		m_xPuzzleScene = g_xEngine.Scenes().LoadScene("Puzzle", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(m_xPuzzleScene);

		m_xAnimation.Cancel(Sokoban::Resources().m_uDustEmitterID);

		memset(m_aeTiles, 0, sizeof(m_aeTiles));	// SOKOBAN_TILE_FLOOR == 0
		memset(m_abTargets, false, sizeof(m_abTargets));
		memset(m_abBoxes, false, sizeof(m_abBoxes));

		m_uGridWidth = 6;
		m_uGridHeight = 3;
		for (uint32_t uX = 0; uX < m_uGridWidth; uX++)
		{
			m_aeTiles[0 * m_uGridWidth + uX] = SOKOBAN_TILE_WALL;
			m_aeTiles[2 * m_uGridWidth + uX] = SOKOBAN_TILE_WALL;
		}
		m_aeTiles[1 * m_uGridWidth + 0] = SOKOBAN_TILE_WALL;
		m_aeTiles[1 * m_uGridWidth + 5] = SOKOBAN_TILE_WALL;
		m_abBoxes[1 * m_uGridWidth + 3] = true;
		m_abTargets[1 * m_uGridWidth + 4] = true;
		m_uPlayerX = 1;
		m_uPlayerY = 1;
		m_uTargetCount = 1;
		m_uMinMoves = 2;

		m_xAnimation.SnapPlayerTo(m_uPlayerX, m_uPlayerY);

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
	}

	// ========================================================================
	// Lifecycle Hooks - Called by engine (concept-detected by the meta registry)
	// ========================================================================

	/**
	 * OnAwake - Called when the component is created at RUNTIME
	 * NOT called during scene loading/deserialization.
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

		// The menu scene's MenuManager owns the Play canvas (its clicks are
		// the graph's OnUIButtonClicked source - no C++ wiring). The gameplay
		// scene's GameManager has no menu: build the level directly; its
		// state comes from Sokoban_LevelFlow's declared defaults (PLAYING,
		// moveCount 0, not won).
		bool bHasMenu = false;
		if (Zenith_UIComponent* pxUI = m_xParentEntity.TryGetComponent<Zenith_UIComponent>())
		{
			bHasMenu = pxUI->FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay") != nullptr;
		}
		if (!bHasMenu)
		{
			RegenerateLevel();
		}
	}

	/**
	 * OnUpdate - per-state SYSTEMS dispatch. Reads the graph-owned state and
	 * runs the systems passes (step-animation tween + visuals); every
	 * DECISION - key -> move dispatch, blocked/push, move counting, the
	 * win-on-step-complete check, R/Esc level flow, menu focus - is a chain
	 * in Sokoban_LevelFlow / Sokoban_GameFlow.
	 */
	void OnUpdate(const float fDt)
	{
		switch (GetGameState())
		{
		case SokobanGameState::MAIN_MENU:
			// Menu focus/click handling is fully graph + UI-system side.
			break;

		case SokobanGameState::PLAYING:
			if (m_xAnimation.IsAnimating())
			{
				const bool bStepComplete = m_xAnimation.Update(fDt, m_uGridWidth, m_uGridHeight, Sokoban::Resources().m_uDustEmitterID);
				if (bStepComplete)
				{
					// The win decision lives in the graph (board facts are
					// staged, compared, and the won flag set graph-side).
					FireStepComplete();
				}
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
		const SokobanGameState eState = GetGameState();
		ImGui::Text("Sokoban Puzzle Game");
		ImGui::Separator();
		ImGui::Text("State: %s", eState == SokobanGameState::MAIN_MENU ? "Menu" :
			eState == SokobanGameState::PLAYING ? "Playing" : "Game Over");
		ImGui::Text("Grid Size: %u x %u", m_uGridWidth, m_uGridHeight);
		ImGui::Text("Moves: %u", GetMoveCount());
		ImGui::Text("Min Moves: %u", m_uMinMoves);
		ImGui::Text("Boxes on targets: %u / %u", GetBoxesOnTargets(), m_uTargetCount);

		if (IsWon())
		{
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "LEVEL COMPLETE!");
		}

		if (ImGui::Button("New Level"))
		{
			// Editor convenience: the systems body + the same blackboard
			// resets the graph's R chain performs.
			RegenerateLevel();
			if (Zenith_GraphBlackboard* pxBlackboard = TryGetGraphBlackboard())
			{
				Zenith_PropertyValue xValue;
				xValue.SetInt32(0);
				pxBlackboard->SetValue("moveCount", xValue);
				xValue.SetBool(false);
				pxBlackboard->SetValue("won", xValue);
			}
		}

		ImGui::Separator();
		ImGui::TextWrapped("Flow decisions live in Sokoban_LevelFlow / Sokoban_GameFlow; "
			"state is on the graph blackboard (gameState/moveCount/won).");
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
				auto xhLoaded = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
					Zenith_MaterialAsset* pxLoaded = xhLoaded.GetDirect();
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
	// The attached graph's blackboard (first slot): Sokoban_LevelFlow on the
	// gameplay GameManager, Sokoban_GameFlow on the menu MenuManager.
	Zenith_GraphBlackboard* TryGetGraphBlackboard()
	{
		if (!m_xParentEntity.IsValid())
		{
			return nullptr;
		}
		Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr || pxGraph->GetGraphCount() == 0)
		{
			return nullptr;
		}
		Zenith_BehaviourGraph* pxBehaviour = pxGraph->GetGraphAt(0);
		return pxBehaviour ? &pxBehaviour->GetBlackboard() : nullptr;
	}

	// Fired the frame a step animation completes - the graph stages the board
	// facts and makes the win decision.
	void FireStepComplete()
	{
		Zenith_GraphComponent* pxGraph = m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
		if (pxGraph == nullptr)
		{
			return;
		}
		pxGraph->FireCustomEvent("SokobanStepComplete");
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
	// Level Generation (systems: generator + solver + visuals; the counter/
	// state resets are the graph's R chain)
	// ========================================================================
	void GenerateNewLevel()
	{
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
		// HUD text is graph-side now (the "SokobanRefreshHUD" chain's
		// SetUIText nodes), fired by the R / OnStart / move / win chains.
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

	// Level facts (gameState/moveCount/won live on the graph blackboard)
	uint32_t m_uTargetCount;
	uint32_t m_uMinMoves;

	// Player/box step animation (tween state + dust emitter driving).
	Sokoban_Animation m_xAnimation;

	// Random number generator
	std::mt19937 m_xRng;

	// Renderer module instance
	Sokoban_Renderer m_xRenderer;

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
